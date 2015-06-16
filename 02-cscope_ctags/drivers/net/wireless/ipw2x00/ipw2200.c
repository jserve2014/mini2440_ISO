/******************************************************************************

  Copyright(c) 2003 - 2006 Intel Corporation. All rights reserved.

  802.11 status code portion of this file from ethereal-0.10.6:
    Copyright 2000, Axis Communications AB
    Ethereal - Network traffic analyzer
    By Gerald Combs <gerald@ethereal.com>
    Copyright 1998 Gerald Combs

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Contact Information:
  Intel Linux Wireless <ilw@linux.intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

******************************************************************************/

#include <linux/sched.h>
#include "ipw2200.h"


#ifndef KBUILD_EXTMOD
#define VK "k"
#else
#define VK
#endif

#ifdef CONFIG_IPW2200_DEBUG
#define VD "d"
#else
#define VD
#endif

#ifdef CONFIG_IPW2200_MONITOR
#define VM "m"
#else
#define VM
#endif

#ifdef CONFIG_IPW2200_PROMISCUOUS
#define VP "p"
#else
#define VP
#endif

#ifdef CONFIG_IPW2200_RADIOTAP
#define VR "r"
#else
#define VR
#endif

#ifdef CONFIG_IPW2200_QOS
#define VQ "q"
#else
#define VQ
#endif

#define IPW2200_VERSION "1.2.2" VK VD VM VP VR VQ
#define DRV_DESCRIPTION	"Intel(R) PRO/Wireless 2200/2915 Network Driver"
#define DRV_COPYRIGHT	"Copyright(c) 2003-2006 Intel Corporation"
#define DRV_VERSION     IPW2200_VERSION

#define ETH_P_80211_STATS (ETH_P_80211_RAW + 1)

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_VERSION(DRV_VERSION);
MODULE_AUTHOR(DRV_COPYRIGHT);
MODULE_LICENSE("GPL");

static int cmdlog = 0;
static int debug = 0;
static int default_channel = 0;
static int network_mode = 0;

static u32 ipw_debug_level;
static int associate;
static int auto_create = 1;
static int led_support = 0;
static int disable = 0;
static int bt_coexist = 0;
static int hwcrypto = 0;
static int roaming = 1;
static const char ipw_modes[] = {
	'a', 'b', 'g', '?'
};
static int antenna = CFG_SYS_ANTENNA_BOTH;

#ifdef CONFIG_IPW2200_PROMISCUOUS
static int rtap_iface = 0;     /* def: 0 -- do not create rtap interface */
#endif


#ifdef CONFIG_IPW2200_QOS
static int qos_enable = 0;
static int qos_burst_enable = 0;
static int qos_no_ack_mask = 0;
static int burst_duration_CCK = 0;
static int burst_duration_OFDM = 0;

static struct libipw_qos_parameters def_qos_parameters_OFDM = {
	{QOS_TX0_CW_MIN_OFDM, QOS_TX1_CW_MIN_OFDM, QOS_TX2_CW_MIN_OFDM,
	 QOS_TX3_CW_MIN_OFDM},
	{QOS_TX0_CW_MAX_OFDM, QOS_TX1_CW_MAX_OFDM, QOS_TX2_CW_MAX_OFDM,
	 QOS_TX3_CW_MAX_OFDM},
	{QOS_TX0_AIFS, QOS_TX1_AIFS, QOS_TX2_AIFS, QOS_TX3_AIFS},
	{QOS_TX0_ACM, QOS_TX1_ACM, QOS_TX2_ACM, QOS_TX3_ACM},
	{QOS_TX0_TXOP_LIMIT_OFDM, QOS_TX1_TXOP_LIMIT_OFDM,
	 QOS_TX2_TXOP_LIMIT_OFDM, QOS_TX3_TXOP_LIMIT_OFDM}
};

static struct libipw_qos_parameters def_qos_parameters_CCK = {
	{QOS_TX0_CW_MIN_CCK, QOS_TX1_CW_MIN_CCK, QOS_TX2_CW_MIN_CCK,
	 QOS_TX3_CW_MIN_CCK},
	{QOS_TX0_CW_MAX_CCK, QOS_TX1_CW_MAX_CCK, QOS_TX2_CW_MAX_CCK,
	 QOS_TX3_CW_MAX_CCK},
	{QOS_TX0_AIFS, QOS_TX1_AIFS, QOS_TX2_AIFS, QOS_TX3_AIFS},
	{QOS_TX0_ACM, QOS_TX1_ACM, QOS_TX2_ACM, QOS_TX3_ACM},
	{QOS_TX0_TXOP_LIMIT_CCK, QOS_TX1_TXOP_LIMIT_CCK, QOS_TX2_TXOP_LIMIT_CCK,
	 QOS_TX3_TXOP_LIMIT_CCK}
};

static struct libipw_qos_parameters def_parameters_OFDM = {
	{DEF_TX0_CW_MIN_OFDM, DEF_TX1_CW_MIN_OFDM, DEF_TX2_CW_MIN_OFDM,
	 DEF_TX3_CW_MIN_OFDM},
	{DEF_TX0_CW_MAX_OFDM, DEF_TX1_CW_MAX_OFDM, DEF_TX2_CW_MAX_OFDM,
	 DEF_TX3_CW_MAX_OFDM},
	{DEF_TX0_AIFS, DEF_TX1_AIFS, DEF_TX2_AIFS, DEF_TX3_AIFS},
	{DEF_TX0_ACM, DEF_TX1_ACM, DEF_TX2_ACM, DEF_TX3_ACM},
	{DEF_TX0_TXOP_LIMIT_OFDM, DEF_TX1_TXOP_LIMIT_OFDM,
	 DEF_TX2_TXOP_LIMIT_OFDM, DEF_TX3_TXOP_LIMIT_OFDM}
};

static struct libipw_qos_parameters def_parameters_CCK = {
	{DEF_TX0_CW_MIN_CCK, DEF_TX1_CW_MIN_CCK, DEF_TX2_CW_MIN_CCK,
	 DEF_TX3_CW_MIN_CCK},
	{DEF_TX0_CW_MAX_CCK, DEF_TX1_CW_MAX_CCK, DEF_TX2_CW_MAX_CCK,
	 DEF_TX3_CW_MAX_CCK},
	{DEF_TX0_AIFS, DEF_TX1_AIFS, DEF_TX2_AIFS, DEF_TX3_AIFS},
	{DEF_TX0_ACM, DEF_TX1_ACM, DEF_TX2_ACM, DEF_TX3_ACM},
	{DEF_TX0_TXOP_LIMIT_CCK, DEF_TX1_TXOP_LIMIT_CCK, DEF_TX2_TXOP_LIMIT_CCK,
	 DEF_TX3_TXOP_LIMIT_CCK}
};

static u8 qos_oui[QOS_OUI_LEN] = { 0x00, 0x50, 0xF2 };

static int from_priority_to_tx_queue[] = {
	IPW_TX_QUEUE_1, IPW_TX_QUEUE_2, IPW_TX_QUEUE_2, IPW_TX_QUEUE_1,
	IPW_TX_QUEUE_3, IPW_TX_QUEUE_3, IPW_TX_QUEUE_4, IPW_TX_QUEUE_4
};

static u32 ipw_qos_get_burst_duration(struct ipw_priv *priv);

static int ipw_send_qos_params_command(struct ipw_priv *priv, struct libipw_qos_parameters
				       *qos_param);
static int ipw_send_qos_info_command(struct ipw_priv *priv, struct libipw_qos_information_element
				     *qos_param);
#endif				/* CONFIG_IPW2200_QOS */

static struct iw_statistics *ipw_get_wireless_stats(struct net_device *dev);
static void ipw_remove_current_network(struct ipw_priv *priv);
static void ipw_rx(struct ipw_priv *priv);
static int ipw_queue_tx_reclaim(struct ipw_priv *priv,
				struct clx2_tx_queue *txq, int qindex);
static int ipw_queue_reset(struct ipw_priv *priv);

static int ipw_queue_tx_hcmd(struct ipw_priv *priv, int hcmd, void *buf,
			     int len, int sync);

static void ipw_tx_queue_free(struct ipw_priv *);

static struct ipw_rx_queue *ipw_rx_queue_alloc(struct ipw_priv *);
static void ipw_rx_queue_free(struct ipw_priv *, struct ipw_rx_queue *);
static void ipw_rx_queue_replenish(void *);
static int ipw_up(struct ipw_priv *);
static void ipw_bg_up(struct work_struct *work);
static void ipw_down(struct ipw_priv *);
static void ipw_bg_down(struct work_struct *work);
static int ipw_config(struct ipw_priv *);
static int init_supported_rates(struct ipw_priv *priv,
				struct ipw_supported_rates *prates);
static void ipw_set_hwcrypto_keys(struct ipw_priv *);
static void ipw_send_wep_keys(struct ipw_priv *, int);

static int snprint_line(char *buf, size_t count,
			const u8 * data, u32 len, u32 ofs)
{
	int out, i, j, l;
	char c;

	out = snprintf(buf, count, "%08X", ofs);

	for (l = 0, i = 0; i < 2; i++) {
		out += snprintf(buf + out, count - out, " ");
		for (j = 0; j < 8 && l < len; j++, l++)
			out += snprintf(buf + out, count - out, "%02X ",
					data[(i * 8 + j)]);
		for (; j < 8; j++)
			out += snprintf(buf + out, count - out, "   ");
	}

	out += snprintf(buf + out, count - out, " ");
	for (l = 0, i = 0; i < 2; i++) {
		out += snprintf(buf + out, count - out, " ");
		for (j = 0; j < 8 && l < len; j++, l++) {
			c = data[(i * 8 + j)];
			if (!isascii(c) || !isprint(c))
				c = '.';

			out += snprintf(buf + out, count - out, "%c", c);
		}

		for (; j < 8; j++)
			out += snprintf(buf + out, count - out, " ");
	}

	return out;
}

static void printk_buf(int level, const u8 * data, u32 len)
{
	char line[81];
	u32 ofs = 0;
	if (!(ipw_debug_level & level))
		return;

	while (len) {
		snprint_line(line, sizeof(line), &data[ofs],
			     min(len, 16U), ofs);
		printk(KERN_DEBUG "%s\n", line);
		ofs += 16;
		len -= min(len, 16U);
	}
}

static int snprintk_buf(u8 * output, size_t size, const u8 * data, size_t len)
{
	size_t out = size;
	u32 ofs = 0;
	int total = 0;

	while (size && len) {
		out = snprint_line(output, size, &data[ofs],
				   min_t(size_t, len, 16U), ofs);

		ofs += 16;
		output += out;
		size -= out;
		len -= min_t(size_t, len, 16U);
		total += out;
	}
	return total;
}

/* alias for 32-bit indirect read (for SRAM/reg above 4K), with debug wrapper */
static u32 _ipw_read_reg32(struct ipw_priv *priv, u32 reg);
#define ipw_read_reg32(a, b) _ipw_read_reg32(a, b)

/* alias for 8-bit indirect read (for SRAM/reg above 4K), with debug wrapper */
static u8 _ipw_read_reg8(struct ipw_priv *ipw, u32 reg);
#define ipw_read_reg8(a, b) _ipw_read_reg8(a, b)

/* 8-bit indirect write (for SRAM/reg above 4K), with debug wrapper */
static void _ipw_write_reg8(struct ipw_priv *priv, u32 reg, u8 value);
static inline void ipw_write_reg8(struct ipw_priv *a, u32 b, u8 c)
{
	IPW_DEBUG_IO("%s %d: write_indirect8(0x%08X, 0x%08X)\n", __FILE__,
		     __LINE__, (u32) (b), (u32) (c));
	_ipw_write_reg8(a, b, c);
}

/* 16-bit indirect write (for SRAM/reg above 4K), with debug wrapper */
static void _ipw_write_reg16(struct ipw_priv *priv, u32 reg, u16 value);
static inline void ipw_write_reg16(struct ipw_priv *a, u32 b, u16 c)
{
	IPW_DEBUG_IO("%s %d: write_indirect16(0x%08X, 0x%08X)\n", __FILE__,
		     __LINE__, (u32) (b), (u32) (c));
	_ipw_write_reg16(a, b, c);
}

/* 32-bit indirect write (for SRAM/reg above 4K), with debug wrapper */
static void _ipw_write_reg32(struct ipw_priv *priv, u32 reg, u32 value);
static inline void ipw_write_reg32(struct ipw_priv *a, u32 b, u32 c)
{
	IPW_DEBUG_IO("%s %d: write_indirect32(0x%08X, 0x%08X)\n", __FILE__,
		     __LINE__, (u32) (b), (u32) (c));
	_ipw_write_reg32(a, b, c);
}

/* 8-bit direct write (low 4K) */
static inline void _ipw_write8(struct ipw_priv *ipw, unsigned long ofs,
		u8 val)
{
	writeb(val, ipw->hw_base + ofs);
}

/* 8-bit direct write (for low 4K of SRAM/regs), with debug wrapper */
#define ipw_write8(ipw, ofs, val) do { \
	IPW_DEBUG_IO("%s %d: write_direct8(0x%08X, 0x%08X)\n", __FILE__, \
			__LINE__, (u32)(ofs), (u32)(val)); \
	_ipw_write8(ipw, ofs, val); \
} while (0)

/* 16-bit direct write (low 4K) */
static inline void _ipw_write16(struct ipw_priv *ipw, unsigned long ofs,
		u16 val)
{
	writew(val, ipw->hw_base + ofs);
}

/* 16-bit direct write (for low 4K of SRAM/regs), with debug wrapper */
#define ipw_write16(ipw, ofs, val) do { \
	IPW_DEBUG_IO("%s %d: write_direct16(0x%08X, 0x%08X)\n", __FILE__, \
			__LINE__, (u32)(ofs), (u32)(val)); \
	_ipw_write16(ipw, ofs, val); \
} while (0)

/* 32-bit direct write (low 4K) */
static inline void _ipw_write32(struct ipw_priv *ipw, unsigned long ofs,
		u32 val)
{
	writel(val, ipw->hw_base + ofs);
}

/* 32-bit direct write (for low 4K of SRAM/regs), with debug wrapper */
#define ipw_write32(ipw, ofs, val) do { \
	IPW_DEBUG_IO("%s %d: write_direct32(0x%08X, 0x%08X)\n", __FILE__, \
			__LINE__, (u32)(ofs), (u32)(val)); \
	_ipw_write32(ipw, ofs, val); \
} while (0)

/* 8-bit direct read (low 4K) */
static inline u8 _ipw_read8(struct ipw_priv *ipw, unsigned long ofs)
{
	return readb(ipw->hw_base + ofs);
}

/* alias to 8-bit direct read (low 4K of SRAM/regs), with debug wrapper */
#define ipw_read8(ipw, ofs) ({ \
	IPW_DEBUG_IO("%s %d: read_direct8(0x%08X)\n", __FILE__, __LINE__, \
			(u32)(ofs)); \
	_ipw_read8(ipw, ofs); \
})

/* 16-bit direct read (low 4K) */
static inline u16 _ipw_read16(struct ipw_priv *ipw, unsigned long ofs)
{
	return readw(ipw->hw_base + ofs);
}

/* alias to 16-bit direct read (low 4K of SRAM/regs), with debug wrapper */
#define ipw_read16(ipw, ofs) ({ \
	IPW_DEBUG_IO("%s %d: read_direct16(0x%08X)\n", __FILE__, __LINE__, \
			(u32)(ofs)); \
	_ipw_read16(ipw, ofs); \
})

/* 32-bit direct read (low 4K) */
static inline u32 _ipw_read32(struct ipw_priv *ipw, unsigned long ofs)
{
	return readl(ipw->hw_base + ofs);
}

/* alias to 32-bit direct read (low 4K of SRAM/regs), with debug wrapper */
#define ipw_read32(ipw, ofs) ({ \
	IPW_DEBUG_IO("%s %d: read_direct32(0x%08X)\n", __FILE__, __LINE__, \
			(u32)(ofs)); \
	_ipw_read32(ipw, ofs); \
})

static void _ipw_read_indirect(struct ipw_priv *, u32, u8 *, int);
/* alias to multi-byte read (SRAM/regs above 4K), with debug wrapper */
#define ipw_read_indirect(a, b, c, d) ({ \
	IPW_DEBUG_IO("%s %d: read_indirect(0x%08X) %u bytes\n", __FILE__, \
			__LINE__, (u32)(b), (u32)(d)); \
	_ipw_read_indirect(a, b, c, d); \
})

/* alias to multi-byte read (SRAM/regs above 4K), with debug wrapper */
static void _ipw_write_indirect(struct ipw_priv *priv, u32 addr, u8 * data,
				int num);
#define ipw_write_indirect(a, b, c, d) do { \
	IPW_DEBUG_IO("%s %d: write_indirect(0x%08X) %u bytes\n", __FILE__, \
			__LINE__, (u32)(b), (u32)(d)); \
	_ipw_write_indirect(a, b, c, d); \
} while (0)

/* 32-bit indirect write (above 4K) */
static void _ipw_write_reg32(struct ipw_priv *priv, u32 reg, u32 value)
{
	IPW_DEBUG_IO(" %p : reg = 0x%8X : value = 0x%8X\n", priv, reg, value);
	_ipw_write32(priv, IPW_INDIRECT_ADDR, reg);
	_ipw_write32(priv, IPW_INDIRECT_DATA, value);
}

/* 8-bit indirect write (above 4K) */
static void _ipw_write_reg8(struct ipw_priv *priv, u32 reg, u8 value)
{
	u32 aligned_addr = reg & IPW_INDIRECT_ADDR_MASK;	/* dword align */
	u32 dif_len = reg - aligned_addr;

	IPW_DEBUG_IO(" reg = 0x%8X : value = 0x%8X\n", reg, value);
	_ipw_write32(priv, IPW_INDIRECT_ADDR, aligned_addr);
	_ipw_write8(priv, IPW_INDIRECT_DATA + dif_len, value);
}

/* 16-bit indirect write (above 4K) */
static void _ipw_write_reg16(struct ipw_priv *priv, u32 reg, u16 value)
{
	u32 aligned_addr = reg & IPW_INDIRECT_ADDR_MASK;	/* dword align */
	u32 dif_len = (reg - aligned_addr) & (~0x1ul);

	IPW_DEBUG_IO(" reg = 0x%8X : value = 0x%8X\n", reg, value);
	_ipw_write32(priv, IPW_INDIRECT_ADDR, aligned_addr);
	_ipw_write16(priv, IPW_INDIRECT_DATA + dif_len, value);
}

/* 8-bit indirect read (above 4K) */
static u8 _ipw_read_reg8(struct ipw_priv *priv, u32 reg)
{
	u32 word;
	_ipw_write32(priv, IPW_INDIRECT_ADDR, reg & IPW_INDIRECT_ADDR_MASK);
	IPW_DEBUG_IO(" reg = 0x%8X : \n", reg);
	word = _ipw_read32(priv, IPW_INDIRECT_DATA);
	return (word >> ((reg & 0x3) * 8)) & 0xff;
}

/* 32-bit indirect read (above 4K) */
static u32 _ipw_read_reg32(struct ipw_priv *priv, u32 reg)
{
	u32 value;

	IPW_DEBUG_IO("%p : reg = 0x%08x\n", priv, reg);

	_ipw_write32(priv, IPW_INDIRECT_ADDR, reg);
	value = _ipw_read32(priv, IPW_INDIRECT_DATA);
	IPW_DEBUG_IO(" reg = 0x%4X : value = 0x%4x \n", reg, value);
	return value;
}

/* General purpose, no alignment requirement, iterative (multi-byte) read, */
/*    for area above 1st 4K of SRAM/reg space */
static void _ipw_read_indirect(struct ipw_priv *priv, u32 addr, u8 * buf,
			       int num)
{
	u32 aligned_addr = addr & IPW_INDIRECT_ADDR_MASK;	/* dword align */
	u32 dif_len = addr - aligned_addr;
	u32 i;

	IPW_DEBUG_IO("addr = %i, buf = %p, num = %i\n", addr, buf, num);

	if (num <= 0) {
		return;
	}

	/* Read the first dword (or portion) byte by byte */
	if (unlikely(dif_len)) {
		_ipw_write32(priv, IPW_INDIRECT_ADDR, aligned_addr);
		/* Start reading at aligned_addr + dif_len */
		for (i = dif_len; ((i < 4) && (num > 0)); i++, num--)
			*buf++ = _ipw_read8(priv, IPW_INDIRECT_DATA + i);
		aligned_addr += 4;
	}

	/* Read all of the middle dwords as dwords, with auto-increment */
	_ipw_write32(priv, IPW_AUTOINC_ADDR, aligned_addr);
	for (; num >= 4; buf += 4, aligned_addr += 4, num -= 4)
		*(u32 *) buf = _ipw_read32(priv, IPW_AUTOINC_DATA);

	/* Read the last dword (or portion) byte by byte */
	if (unlikely(num)) {
		_ipw_write32(priv, IPW_INDIRECT_ADDR, aligned_addr);
		for (i = 0; num > 0; i++, num--)
			*buf++ = ipw_read8(priv, IPW_INDIRECT_DATA + i);
	}
}

/* General purpose, no alignment requirement, iterative (multi-byte) write, */
/*    for area above 1st 4K of SRAM/reg space */
static void _ipw_write_indirect(struct ipw_priv *priv, u32 addr, u8 * buf,
				int num)
{
	u32 aligned_addr = addr & IPW_INDIRECT_ADDR_MASK;	/* dword align */
	u32 dif_len = addr - aligned_addr;
	u32 i;

	IPW_DEBUG_IO("addr = %i, buf = %p, num = %i\n", addr, buf, num);

	if (num <= 0) {
		return;
	}

	/* Write the first dword (or portion) byte by byte */
	if (unlikely(dif_len)) {
		_ipw_write32(priv, IPW_INDIRECT_ADDR, aligned_addr);
		/* Start writing at aligned_addr + dif_len */
		for (i = dif_len; ((i < 4) && (num > 0)); i++, num--, buf++)
			_ipw_write8(priv, IPW_INDIRECT_DATA + i, *buf);
		aligned_addr += 4;
	}

	/* Write all of the middle dwords as dwords, with auto-increment */
	_ipw_write32(priv, IPW_AUTOINC_ADDR, aligned_addr);
	for (; num >= 4; buf += 4, aligned_addr += 4, num -= 4)
		_ipw_write32(priv, IPW_AUTOINC_DATA, *(u32 *) buf);

	/* Write the last dword (or portion) byte by byte */
	if (unlikely(num)) {
		_ipw_write32(priv, IPW_INDIRECT_ADDR, aligned_addr);
		for (i = 0; num > 0; i++, num--, buf++)
			_ipw_write8(priv, IPW_INDIRECT_DATA + i, *buf);
	}
}

/* General purpose, no alignment requirement, iterative (multi-byte) write, */
/*    for 1st 4K of SRAM/regs space */
static void ipw_write_direct(struct ipw_priv *priv, u32 addr, void *buf,
			     int num)
{
	memcpy_toio((priv->hw_base + addr), buf, num);
}

/* Set bit(s) in low 4K of SRAM/regs */
static inline void ipw_set_bit(struct ipw_priv *priv, u32 reg, u32 mask)
{
	ipw_write32(priv, reg, ipw_read32(priv, reg) | mask);
}

/* Clear bit(s) in low 4K of SRAM/regs */
static inline void ipw_clear_bit(struct ipw_priv *priv, u32 reg, u32 mask)
{
	ipw_write32(priv, reg, ipw_read32(priv, reg) & ~mask);
}

static inline void __ipw_enable_interrupts(struct ipw_priv *priv)
{
	if (priv->status & STATUS_INT_ENABLED)
		return;
	priv->status |= STATUS_INT_ENABLED;
	ipw_write32(priv, IPW_INTA_MASK_R, IPW_INTA_MASK_ALL);
}

static inline void __ipw_disable_interrupts(struct ipw_priv *priv)
{
	if (!(priv->status & STATUS_INT_ENABLED))
		return;
	priv->status &= ~STATUS_INT_ENABLED;
	ipw_write32(priv, IPW_INTA_MASK_R, ~IPW_INTA_MASK_ALL);
}

static inline void ipw_enable_interrupts(struct ipw_priv *priv)
{
	unsigned long flags;

	spin_lock_irqsave(&priv->irq_lock, flags);
	__ipw_enable_interrupts(priv);
	spin_unlock_irqrestore(&priv->irq_lock, flags);
}

static inline void ipw_disable_interrupts(struct ipw_priv *priv)
{
	unsigned long flags;

	spin_lock_irqsave(&priv->irq_lock, flags);
	__ipw_disable_interrupts(priv);
	spin_unlock_irqrestore(&priv->irq_lock, flags);
}

static char *ipw_error_desc(u32 val)
{
	switch (val) {
	case IPW_FW_ERROR_OK:
		return "ERROR_OK";
	case IPW_FW_ERROR_FAIL:
		return "ERROR_FAIL";
	case IPW_FW_ERROR_MEMORY_UNDERFLOW:
		return "MEMORY_UNDERFLOW";
	case IPW_FW_ERROR_MEMORY_OVERFLOW:
		return "MEMORY_OVERFLOW";
	case IPW_FW_ERROR_BAD_PARAM:
		return "BAD_PARAM";
	case IPW_FW_ERROR_BAD_CHECKSUM:
		return "BAD_CHECKSUM";
	case IPW_FW_ERROR_NMI_INTERRUPT:
		return "NMI_INTERRUPT";
	case IPW_FW_ERROR_BAD_DATABASE:
		return "BAD_DATABASE";
	case IPW_FW_ERROR_ALLOC_FAIL:
		return "ALLOC_FAIL";
	case IPW_FW_ERROR_DMA_UNDERRUN:
		return "DMA_UNDERRUN";
	case IPW_FW_ERROR_DMA_STATUS:
		return "DMA_STATUS";
	case IPW_FW_ERROR_DINO_ERROR:
		return "DINO_ERROR";
	case IPW_FW_ERROR_EEPROM_ERROR:
		return "EEPROM_ERROR";
	case IPW_FW_ERROR_SYSASSERT:
		return "SYSASSERT";
	case IPW_FW_ERROR_FATAL_ERROR:
		return "FATAL_ERROR";
	default:
		return "UNKNOWN_ERROR";
	}
}

static void ipw_dump_error_log(struct ipw_priv *priv,
			       struct ipw_fw_error *error)
{
	u32 i;

	if (!error) {
		IPW_ERROR("Error allocating and capturing error log.  "
			  "Nothing to dump.\n");
		return;
	}

	IPW_ERROR("Start IPW Error Log Dump:\n");
	IPW_ERROR("Status: 0x%08X, Config: %08X\n",
		  error->status, error->config);

	for (i = 0; i < error->elem_len; i++)
		IPW_ERROR("%s %i 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
			  ipw_error_desc(error->elem[i].desc),
			  error->elem[i].time,
			  error->elem[i].blink1,
			  error->elem[i].blink2,
			  error->elem[i].link1,
			  error->elem[i].link2, error->elem[i].data);
	for (i = 0; i < error->log_len; i++)
		IPW_ERROR("%i\t0x%08x\t%i\n",
			  error->log[i].time,
			  error->log[i].data, error->log[i].event);
}

static inline int ipw_is_init(struct ipw_priv *priv)
{
	return (priv->status & STATUS_INIT) ? 1 : 0;
}

static int ipw_get_ordinal(struct ipw_priv *priv, u32 ord, void *val, u32 * len)
{
	u32 addr, field_info, field_len, field_count, total_len;

	IPW_DEBUG_ORD("ordinal = %i\n", ord);

	if (!priv || !val || !len) {
		IPW_DEBUG_ORD("Invalid argument\n");
		return -EINVAL;
	}

	/* verify device ordinal tables have been initialized */
	if (!priv->table0_addr || !priv->table1_addr || !priv->table2_addr) {
		IPW_DEBUG_ORD("Access ordinals before initialization\n");
		return -EINVAL;
	}

	switch (IPW_ORD_TABLE_ID_MASK & ord) {
	case IPW_ORD_TABLE_0_MASK:
		/*
		 * TABLE 0: Direct access to a table of 32 bit values
		 *
		 * This is a very simple table with the data directly
		 * read from the table
		 */

		/* remove the table id from the ordinal */
		ord &= IPW_ORD_TABLE_VALUE_MASK;

		/* boundary check */
		if (ord > priv->table0_len) {
			IPW_DEBUG_ORD("ordinal value (%i) longer then "
				      "max (%i)\n", ord, priv->table0_len);
			return -EINVAL;
		}

		/* verify we have enough room to store the value */
		if (*len < sizeof(u32)) {
			IPW_DEBUG_ORD("ordinal buffer length too small, "
				      "need %zd\n", sizeof(u32));
			return -EINVAL;
		}

		IPW_DEBUG_ORD("Reading TABLE0[%i] from offset 0x%08x\n",
			      ord, priv->table0_addr + (ord << 2));

		*len = sizeof(u32);
		ord <<= 2;
		*((u32 *) val) = ipw_read32(priv, priv->table0_addr + ord);
		break;

	case IPW_ORD_TABLE_1_MASK:
		/*
		 * TABLE 1: Indirect access to a table of 32 bit values
		 *
		 * This is a fairly large table of u32 values each
		 * representing starting addr for the data (which is
		 * also a u32)
		 */

		/* remove the table id from the ordinal */
		ord &= IPW_ORD_TABLE_VALUE_MASK;

		/* boundary check */
		if (ord > priv->table1_len) {
			IPW_DEBUG_ORD("ordinal value too long\n");
			return -EINVAL;
		}

		/* verify we have enough room to store the value */
		if (*len < sizeof(u32)) {
			IPW_DEBUG_ORD("ordinal buffer length too small, "
				      "need %zd\n", sizeof(u32));
			return -EINVAL;
		}

		*((u32 *) val) =
		    ipw_read_reg32(priv, (priv->table1_addr + (ord << 2)));
		*len = sizeof(u32);
		break;

	case IPW_ORD_TABLE_2_MASK:
		/*
		 * TABLE 2: Indirect access to a table of variable sized values
		 *
		 * This table consist of six values, each containing
		 *     - dword containing the starting offset of the data
		 *     - dword containing the lengh in the first 16bits
		 *       and the count in the second 16bits
		 */

		/* remove the table id from the ordinal */
		ord &= IPW_ORD_TABLE_VALUE_MASK;

		/* boundary check */
		if (ord > priv->table2_len) {
			IPW_DEBUG_ORD("ordinal value too long\n");
			return -EINVAL;
		}

		/* get the address of statistic */
		addr = ipw_read_reg32(priv, priv->table2_addr + (ord << 3));

		/* get the second DW of statistics ;
		 * two 16-bit words - first is length, second is count */
		field_info =
		    ipw_read_reg32(priv,
				   priv->table2_addr + (ord << 3) +
				   sizeof(u32));

		/* get each entry length */
		field_len = *((u16 *) & field_info);

		/* get number of entries */
		field_count = *(((u16 *) & field_info) + 1);

		/* abort if not enought memory */
		total_len = field_len * field_count;
		if (total_len > *len) {
			*len = total_len;
			return -EINVAL;
		}

		*len = total_len;
		if (!total_len)
			return 0;

		IPW_DEBUG_ORD("addr = 0x%08x, total_len = %i, "
			      "field_info = 0x%08x\n",
			      addr, total_len, field_info);
		ipw_read_indirect(priv, addr, val, total_len);
		break;

	default:
		IPW_DEBUG_ORD("Invalid ordinal!\n");
		return -EINVAL;

	}

	return 0;
}

static void ipw_init_ordinals(struct ipw_priv *priv)
{
	priv->table0_addr = IPW_ORDINALS_TABLE_LOWER;
	priv->table0_len = ipw_read32(priv, priv->table0_addr);

	IPW_DEBUG_ORD("table 0 offset at 0x%08x, len = %i\n",
		      priv->table0_addr, priv->table0_len);

	priv->table1_addr = ipw_read32(priv, IPW_ORDINALS_TABLE_1);
	priv->table1_len = ipw_read_reg32(priv, priv->table1_addr);

	IPW_DEBUG_ORD("table 1 offset at 0x%08x, len = %i\n",
		      priv->table1_addr, priv->table1_len);

	priv->table2_addr = ipw_read32(priv, IPW_ORDINALS_TABLE_2);
	priv->table2_len = ipw_read_reg32(priv, priv->table2_addr);
	priv->table2_len &= 0x0000ffff;	/* use first two bytes */

	IPW_DEBUG_ORD("table 2 offset at 0x%08x, len = %i\n",
		      priv->table2_addr, priv->table2_len);

}

static u32 ipw_register_toggle(u32 reg)
{
	reg &= ~IPW_START_STANDBY;
	if (reg & IPW_GATE_ODMA)
		reg &= ~IPW_GATE_ODMA;
	if (reg & IPW_GATE_IDMA)
		reg &= ~IPW_GATE_IDMA;
	if (reg & IPW_GATE_ADMA)
		reg &= ~IPW_GATE_ADMA;
	return reg;
}

/*
 * LED behavior:
 * - On radio ON, turn on any LEDs that require to be on during start
 * - On initialization, start unassociated blink
 * - On association, disable unassociated blink
 * - On disassociation, start unassociated blink
 * - On radio OFF, turn off any LEDs started during radio on
 *
 */
#define LD_TIME_LINK_ON msecs_to_jiffies(300)
#define LD_TIME_LINK_OFF msecs_to_jiffies(2700)
#define LD_TIME_ACT_ON msecs_to_jiffies(250)

static void ipw_led_link_on(struct ipw_priv *priv)
{
	unsigned long flags;
	u32 led;

	/* If configured to not use LEDs, or nic_type is 1,
	 * then we don't toggle a LINK led */
	if (priv->config & CFG_NO_LED || priv->nic_type == EEPROM_NIC_TYPE_1)
		return;

	spin_lock_irqsave(&priv->lock, flags);

	if (!(priv->status & STATUS_RF_KILL_MASK) &&
	    !(priv->status & STATUS_LED_LINK_ON)) {
		IPW_DEBUG_LED("Link LED On\n");
		led = ipw_read_reg32(priv, IPW_EVENT_REG);
		led |= priv->led_association_on;

		led = ipw_register_toggle(led);

		IPW_DEBUG_LED("Reg: 0x%08X\n", led);
		ipw_write_reg32(priv, IPW_EVENT_REG, led);

		priv->status |= STATUS_LED_LINK_ON;

		/* If we aren't associated, schedule turning the LED off */
		if (!(priv->status & STATUS_ASSOCIATED))
			queue_delayed_work(priv->workqueue,
					   &priv->led_link_off,
					   LD_TIME_LINK_ON);
	}

	spin_unlock_irqrestore(&priv->lock, flags);
}

static void ipw_bg_led_link_on(struct work_struct *work)
{
	struct ipw_priv *priv =
		container_of(work, struct ipw_priv, led_link_on.work);
	mutex_lock(&priv->mutex);
	ipw_led_link_on(priv);
	mutex_unlock(&priv->mutex);
}

static void ipw_led_link_off(struct ipw_priv *priv)
{
	unsigned long flags;
	u32 led;

	/* If configured not to use LEDs, or nic type is 1,
	 * then we don't goggle the LINK led. */
	if (priv->config & CFG_NO_LED || priv->nic_type == EEPROM_NIC_TYPE_1)
		return;

	spin_lock_irqsave(&priv->lock, flags);

	if (priv->status & STATUS_LED_LINK_ON) {
		led = ipw_read_reg32(priv, IPW_EVENT_REG);
		led &= priv->led_association_off;
		led = ipw_register_toggle(led);

		IPW_DEBUG_LED("Reg: 0x%08X\n", led);
		ipw_write_reg32(priv, IPW_EVENT_REG, led);

		IPW_DEBUG_LED("Link LED Off\n");

		priv->status &= ~STATUS_LED_LINK_ON;

		/* If we aren't associated and the radio is on, schedule
		 * turning the LED on (blink while unassociated) */
		if (!(priv->status & STATUS_RF_KILL_MASK) &&
		    !(priv->status & STATUS_ASSOCIATED))
			queue_delayed_work(priv->workqueue, &priv->led_link_on,
					   LD_TIME_LINK_OFF);

	}

	spin_unlock_irqrestore(&priv->lock, flags);
}

static void ipw_bg_led_link_off(struct work_struct *work)
{
	struct ipw_priv *priv =
		container_of(work, struct ipw_priv, led_link_off.work);
	mutex_lock(&priv->mutex);
	ipw_led_link_off(priv);
	mutex_unlock(&priv->mutex);
}

static void __ipw_led_activity_on(struct ipw_priv *priv)
{
	u32 led;

	if (priv->config & CFG_NO_LED)
		return;

	if (priv->status & STATUS_RF_KILL_MASK)
		return;

	if (!(priv->status & STATUS_LED_ACT_ON)) {
		led = ipw_read_reg32(priv, IPW_EVENT_REG);
		led |= priv->led_activity_on;

		led = ipw_register_toggle(led);

		IPW_DEBUG_LED("Reg: 0x%08X\n", led);
		ipw_write_reg32(priv, IPW_EVENT_REG, led);

		IPW_DEBUG_LED("Activity LED On\n");

		priv->status |= STATUS_LED_ACT_ON;

		cancel_delayed_work(&priv->led_act_off);
		queue_delayed_work(priv->workqueue, &priv->led_act_off,
				   LD_TIME_ACT_ON);
	} else {
		/* Reschedule LED off for full time period */
		cancel_delayed_work(&priv->led_act_off);
		queue_delayed_work(priv->workqueue, &priv->led_act_off,
				   LD_TIME_ACT_ON);
	}
}

#if 0
void ipw_led_activity_on(struct ipw_priv *priv)
{
	unsigned long flags;
	spin_lock_irqsave(&priv->lock, flags);
	__ipw_led_activity_on(priv);
	spin_unlock_irqrestore(&priv->lock, flags);
}
#endif  /*  0  */

static void ipw_led_activity_off(struct ipw_priv *priv)
{
	unsigned long flags;
	u32 led;

	if (priv->config & CFG_NO_LED)
		return;

	spin_lock_irqsave(&priv->lock, flags);

	if (priv->status & STATUS_LED_ACT_ON) {
		led = ipw_read_reg32(priv, IPW_EVENT_REG);
		led &= priv->led_activity_off;

		led = ipw_register_toggle(led);

		IPW_DEBUG_LED("Reg: 0x%08X\n", led);
		ipw_write_reg32(priv, IPW_EVENT_REG, led);

		IPW_DEBUG_LED("Activity LED Off\n");

		priv->status &= ~STATUS_LED_ACT_ON;
	}

	spin_unlock_irqrestore(&priv->lock, flags);
}

static void ipw_bg_led_activity_off(struct work_struct *work)
{
	struct ipw_priv *priv =
		container_of(work, struct ipw_priv, led_act_off.work);
	mutex_lock(&priv->mutex);
	ipw_led_activity_off(priv);
	mutex_unlock(&priv->mutex);
}

static void ipw_led_band_on(struct ipw_priv *priv)
{
	unsigned long flags;
	u32 led;

	/* Only nic type 1 supports mode LEDs */
	if (priv->config & CFG_NO_LED ||
	    priv->nic_type != EEPROM_NIC_TYPE_1 || !priv->assoc_network)
		return;

	spin_lock_irqsave(&priv->lock, flags);

	led = ipw_read_reg32(priv, IPW_EVENT_REG);
	if (priv->assoc_network->mode == IEEE_A) {
		led |= priv->led_ofdm_on;
		led &= priv->led_association_off;
		IPW_DEBUG_LED("Mode LED On: 802.11a\n");
	} else if (priv->assoc_network->mode == IEEE_G) {
		led |= priv->led_ofdm_on;
		led |= priv->led_association_on;
		IPW_DEBUG_LED("Mode LED On: 802.11g\n");
	} else {
		led &= priv->led_ofdm_off;
		led |= priv->led_association_on;
		IPW_DEBUG_LED("Mode LED On: 802.11b\n");
	}

	led = ipw_register_toggle(led);

	IPW_DEBUG_LED("Reg: 0x%08X\n", led);
	ipw_write_reg32(priv, IPW_EVENT_REG, led);

	spin_unlock_irqrestore(&priv->lock, flags);
}

static void ipw_led_band_off(struct ipw_priv *priv)
{
	unsigned long flags;
	u32 led;

	/* Only nic type 1 supports mode LEDs */
	if (priv->config & CFG_NO_LED || priv->nic_type != EEPROM_NIC_TYPE_1)
		return;

	spin_lock_irqsave(&priv->lock, flags);

	led = ipw_read_reg32(priv, IPW_EVENT_REG);
	led &= priv->led_ofdm_off;
	led &= priv->led_association_off;

	led = ipw_register_toggle(led);

	IPW_DEBUG_LED("Reg: 0x%08X\n", led);
	ipw_write_reg32(priv, IPW_EVENT_REG, led);

	spin_unlock_irqrestore(&priv->lock, flags);
}

static void ipw_led_radio_on(struct ipw_priv *priv)
{
	ipw_led_link_on(priv);
}

static void ipw_led_radio_off(struct ipw_priv *priv)
{
	ipw_led_activity_off(priv);
	ipw_led_link_off(priv);
}

static void ipw_led_link_up(struct ipw_priv *priv)
{
	/* Set the Link Led on for all nic types */
	ipw_led_link_on(priv);
}

static void ipw_led_link_down(struct ipw_priv *priv)
{
	ipw_led_activity_off(priv);
	ipw_led_link_off(priv);

	if (priv->status & STATUS_RF_KILL_MASK)
		ipw_led_radio_off(priv);
}

static void ipw_led_init(struct ipw_priv *priv)
{
	priv->nic_type = priv->eeprom[EEPROM_NIC_TYPE];

	/* Set the default PINs for the link and activity leds */
	priv->led_activity_on = IPW_ACTIVITY_LED;
	priv->led_activity_off = ~(IPW_ACTIVITY_LED);

	priv->led_association_on = IPW_ASSOCIATED_LED;
	priv->led_association_off = ~(IPW_ASSOCIATED_LED);

	/* Set the default PINs for the OFDM leds */
	priv->led_ofdm_on = IPW_OFDM_LED;
	priv->led_ofdm_off = ~(IPW_OFDM_LED);

	switch (priv->nic_type) {
	case EEPROM_NIC_TYPE_1:
		/* In this NIC type, the LEDs are reversed.... */
		priv->led_activity_on = IPW_ASSOCIATED_LED;
		priv->led_activity_off = ~(IPW_ASSOCIATED_LED);
		priv->led_association_on = IPW_ACTIVITY_LED;
		priv->led_association_off = ~(IPW_ACTIVITY_LED);

		if (!(priv->config & CFG_NO_LED))
			ipw_led_band_on(priv);

		/* And we don't blink link LEDs for this nic, so
		 * just return here */
		return;

	case EEPROM_NIC_TYPE_3:
	case EEPROM_NIC_TYPE_2:
	case EEPROM_NIC_TYPE_4:
	case EEPROM_NIC_TYPE_0:
		break;

	default:
		IPW_DEBUG_INFO("Unknown NIC type from EEPROM: %d\n",
			       priv->nic_type);
		priv->nic_type = EEPROM_NIC_TYPE_0;
		break;
	}

	if (!(priv->config & CFG_NO_LED)) {
		if (priv->status & STATUS_ASSOCIATED)
			ipw_led_link_on(priv);
		else
			ipw_led_link_off(priv);
	}
}

static void ipw_led_shutdown(struct ipw_priv *priv)
{
	ipw_led_activity_off(priv);
	ipw_led_link_off(priv);
	ipw_led_band_off(priv);
	cancel_delayed_work(&priv->led_link_on);
	cancel_delayed_work(&priv->led_link_off);
	cancel_delayed_work(&priv->led_act_off);
}

/*
 * The following adds a new attribute to the sysfs representation
 * of this device driver (i.e. a new file in /sys/bus/pci/drivers/ipw/)
 * used for controling the debug level.
 *
 * See the level definitions in ipw for details.
 */
static ssize_t show_debug_level(struct device_driver *d, char *buf)
{
	return sprintf(buf, "0x%08X\n", ipw_debug_level);
}

static ssize_t store_debug_level(struct device_driver *d, const char *buf,
				 size_t count)
{
	char *p = (char *)buf;
	u32 val;

	if (p[1] == 'x' || p[1] == 'X' || p[0] == 'x' || p[0] == 'X') {
		p++;
		if (p[0] == 'x' || p[0] == 'X')
			p++;
		val = simple_strtoul(p, &p, 16);
	} else
		val = simple_strtoul(p, &p, 10);
	if (p == buf)
		printk(KERN_INFO DRV_NAME
		       ": %s is not in hex or decimal form.\n", buf);
	else
		ipw_debug_level = val;

	return strnlen(buf, count);
}

static DRIVER_ATTR(debug_level, S_IWUSR | S_IRUGO,
		   show_debug_level, store_debug_level);

static inline u32 ipw_get_event_log_len(struct ipw_priv *priv)
{
	/* length = 1st dword in log */
	return ipw_read_reg32(priv, ipw_read32(priv, IPW_EVENT_LOG));
}

static void ipw_capture_event_log(struct ipw_priv *priv,
				  u32 log_len, struct ipw_event *log)
{
	u32 base;

	if (log_len) {
		base = ipw_read32(priv, IPW_EVENT_LOG);
		ipw_read_indirect(priv, base + sizeof(base) + sizeof(u32),
				  (u8 *) log, sizeof(*log) * log_len);
	}
}

static struct ipw_fw_error *ipw_alloc_error_log(struct ipw_priv *priv)
{
	struct ipw_fw_error *error;
	u32 log_len = ipw_get_event_log_len(priv);
	u32 base = ipw_read32(priv, IPW_ERROR_LOG);
	u32 elem_len = ipw_read_reg32(priv, base);

	error = kmalloc(sizeof(*error) +
			sizeof(*error->elem) * elem_len +
			sizeof(*error->log) * log_len, GFP_ATOMIC);
	if (!error) {
		IPW_ERROR("Memory allocation for firmware error log "
			  "failed.\n");
		return NULL;
	}
	error->jiffies = jiffies;
	error->status = priv->status;
	error->config = priv->config;
	error->elem_len = elem_len;
	error->log_len = log_len;
	error->elem = (struct ipw_error_elem *)error->payload;
	error->log = (struct ipw_event *)(error->elem + elem_len);

	ipw_capture_event_log(priv, log_len, error->log);

	if (elem_len)
		ipw_read_indirect(priv, base + sizeof(base), (u8 *) error->elem,
				  sizeof(*error->elem) * elem_len);

	return error;
}

static ssize_t show_event_log(struct device *d,
			      struct device_attribute *attr, char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	u32 log_len = ipw_get_event_log_len(priv);
	u32 log_size;
	struct ipw_event *log;
	u32 len = 0, i;

	/* not using min() because of its strict type checking */
	log_size = PAGE_SIZE / sizeof(*log) > log_len ?
			sizeof(*log) * log_len : PAGE_SIZE;
	log = kzalloc(log_size, GFP_KERNEL);
	if (!log) {
		IPW_ERROR("Unable to allocate memory for log\n");
		return 0;
	}
	log_len = log_size / sizeof(*log);
	ipw_capture_event_log(priv, log_len, log);

	len += snprintf(buf + len, PAGE_SIZE - len, "%08X", log_len);
	for (i = 0; i < log_len; i++)
		len += snprintf(buf + len, PAGE_SIZE - len,
				"\n%08X%08X%08X",
				log[i].time, log[i].event, log[i].data);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	kfree(log);
	return len;
}

static DEVICE_ATTR(event_log, S_IRUGO, show_event_log, NULL);

static ssize_t show_error(struct device *d,
			  struct device_attribute *attr, char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	u32 len = 0, i;
	if (!priv->error)
		return 0;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"%08lX%08X%08X%08X",
			priv->error->jiffies,
			priv->error->status,
			priv->error->config, priv->error->elem_len);
	for (i = 0; i < priv->error->elem_len; i++)
		len += snprintf(buf + len, PAGE_SIZE - len,
				"\n%08X%08X%08X%08X%08X%08X%08X",
				priv->error->elem[i].time,
				priv->error->elem[i].desc,
				priv->error->elem[i].blink1,
				priv->error->elem[i].blink2,
				priv->error->elem[i].link1,
				priv->error->elem[i].link2,
				priv->error->elem[i].data);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"\n%08X", priv->error->log_len);
	for (i = 0; i < priv->error->log_len; i++)
		len += snprintf(buf + len, PAGE_SIZE - len,
				"\n%08X%08X%08X",
				priv->error->log[i].time,
				priv->error->log[i].event,
				priv->error->log[i].data);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

static ssize_t clear_error(struct device *d,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	kfree(priv->error);
	priv->error = NULL;
	return count;
}

static DEVICE_ATTR(error, S_IRUGO | S_IWUSR, show_error, clear_error);

static ssize_t show_cmd_log(struct device *d,
			    struct device_attribute *attr, char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	u32 len = 0, i;
	if (!priv->cmdlog)
		return 0;
	for (i = (priv->cmdlog_pos + 1) % priv->cmdlog_len;
	     (i != priv->cmdlog_pos) && (PAGE_SIZE - len);
	     i = (i + 1) % priv->cmdlog_len) {
		len +=
		    snprintf(buf + len, PAGE_SIZE - len,
			     "\n%08lX%08X%08X%08X\n", priv->cmdlog[i].jiffies,
			     priv->cmdlog[i].retcode, priv->cmdlog[i].cmd.cmd,
			     priv->cmdlog[i].cmd.len);
		len +=
		    snprintk_buf(buf + len, PAGE_SIZE - len,
				 (u8 *) priv->cmdlog[i].cmd.param,
				 priv->cmdlog[i].cmd.len);
		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

static DEVICE_ATTR(cmd_log, S_IRUGO, show_cmd_log, NULL);

#ifdef CONFIG_IPW2200_PROMISCUOUS
static void ipw_prom_free(struct ipw_priv *priv);
static int ipw_prom_alloc(struct ipw_priv *priv);
static ssize_t store_rtap_iface(struct device *d,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	int rc = 0;

	if (count < 1)
		return -EINVAL;

	switch (buf[0]) {
	case '0':
		if (!rtap_iface)
			return count;

		if (netif_running(priv->prom_net_dev)) {
			IPW_WARNING("Interface is up.  Cannot unregister.\n");
			return count;
		}

		ipw_prom_free(priv);
		rtap_iface = 0;
		break;

	case '1':
		if (rtap_iface)
			return count;

		rc = ipw_prom_alloc(priv);
		if (!rc)
			rtap_iface = 1;
		break;

	default:
		return -EINVAL;
	}

	if (rc) {
		IPW_ERROR("Failed to register promiscuous network "
			  "device (error %d).\n", rc);
	}

	return count;
}

static ssize_t show_rtap_iface(struct device *d,
			struct device_attribute *attr,
			char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	if (rtap_iface)
		return sprintf(buf, "%s", priv->prom_net_dev->name);
	else {
		buf[0] = '-';
		buf[1] = '1';
		buf[2] = '\0';
		return 3;
	}
}

static DEVICE_ATTR(rtap_iface, S_IWUSR | S_IRUSR, show_rtap_iface,
		   store_rtap_iface);

static ssize_t store_rtap_filter(struct device *d,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	if (!priv->prom_priv) {
		IPW_ERROR("Attempting to set filter without "
			  "rtap_iface enabled.\n");
		return -EPERM;
	}

	priv->prom_priv->filter = simple_strtol(buf, NULL, 0);

	IPW_DEBUG_INFO("Setting rtap filter to " BIT_FMT16 "\n",
		       BIT_ARG16(priv->prom_priv->filter));

	return count;
}

static ssize_t show_rtap_filter(struct device *d,
			struct device_attribute *attr,
			char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	return sprintf(buf, "0x%04X",
		       priv->prom_priv ? priv->prom_priv->filter : 0);
}

static DEVICE_ATTR(rtap_filter, S_IWUSR | S_IRUSR, show_rtap_filter,
		   store_rtap_filter);
#endif

static ssize_t show_scan_age(struct device *d, struct device_attribute *attr,
			     char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	return sprintf(buf, "%d\n", priv->ieee->scan_age);
}

static ssize_t store_scan_age(struct device *d, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	struct net_device *dev = priv->net_dev;
	char buffer[] = "00000000";
	unsigned long len =
	    (sizeof(buffer) - 1) > count ? count : sizeof(buffer) - 1;
	unsigned long val;
	char *p = buffer;

	IPW_DEBUG_INFO("enter\n");

	strncpy(buffer, buf, len);
	buffer[len] = 0;

	if (p[1] == 'x' || p[1] == 'X' || p[0] == 'x' || p[0] == 'X') {
		p++;
		if (p[0] == 'x' || p[0] == 'X')
			p++;
		val = simple_strtoul(p, &p, 16);
	} else
		val = simple_strtoul(p, &p, 10);
	if (p == buffer) {
		IPW_DEBUG_INFO("%s: user supplied invalid value.\n", dev->name);
	} else {
		priv->ieee->scan_age = val;
		IPW_DEBUG_INFO("set scan_age = %u\n", priv->ieee->scan_age);
	}

	IPW_DEBUG_INFO("exit\n");
	return len;
}

static DEVICE_ATTR(scan_age, S_IWUSR | S_IRUGO, show_scan_age, store_scan_age);

static ssize_t show_led(struct device *d, struct device_attribute *attr,
			char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	return sprintf(buf, "%d\n", (priv->config & CFG_NO_LED) ? 0 : 1);
}

static ssize_t store_led(struct device *d, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	IPW_DEBUG_INFO("enter\n");

	if (count == 0)
		return 0;

	if (*buf == 0) {
		IPW_DEBUG_LED("Disabling LED control.\n");
		priv->config |= CFG_NO_LED;
		ipw_led_shutdown(priv);
	} else {
		IPW_DEBUG_LED("Enabling LED control.\n");
		priv->config &= ~CFG_NO_LED;
		ipw_led_init(priv);
	}

	IPW_DEBUG_INFO("exit\n");
	return count;
}

static DEVICE_ATTR(led, S_IWUSR | S_IRUGO, show_led, store_led);

static ssize_t show_status(struct device *d,
			   struct device_attribute *attr, char *buf)
{
	struct ipw_priv *p = dev_get_drvdata(d);
	return sprintf(buf, "0x%08x\n", (int)p->status);
}

static DEVICE_ATTR(status, S_IRUGO, show_status, NULL);

static ssize_t show_cfg(struct device *d, struct device_attribute *attr,
			char *buf)
{
	struct ipw_priv *p = dev_get_drvdata(d);
	return sprintf(buf, "0x%08x\n", (int)p->config);
}

static DEVICE_ATTR(cfg, S_IRUGO, show_cfg, NULL);

static ssize_t show_nic_type(struct device *d,
			     struct device_attribute *attr, char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	return sprintf(buf, "TYPE: %d\n", priv->nic_type);
}

static DEVICE_ATTR(nic_type, S_IRUGO, show_nic_type, NULL);

static ssize_t show_ucode_version(struct device *d,
				  struct device_attribute *attr, char *buf)
{
	u32 len = sizeof(u32), tmp = 0;
	struct ipw_priv *p = dev_get_drvdata(d);

	if (ipw_get_ordinal(p, IPW_ORD_STAT_UCODE_VERSION, &tmp, &len))
		return 0;

	return sprintf(buf, "0x%08x\n", tmp);
}

static DEVICE_ATTR(ucode_version, S_IWUSR | S_IRUGO, show_ucode_version, NULL);

static ssize_t show_rtc(struct device *d, struct device_attribute *attr,
			char *buf)
{
	u32 len = sizeof(u32), tmp = 0;
	struct ipw_priv *p = dev_get_drvdata(d);

	if (ipw_get_ordinal(p, IPW_ORD_STAT_RTC, &tmp, &len))
		return 0;

	return sprintf(buf, "0x%08x\n", tmp);
}

static DEVICE_ATTR(rtc, S_IWUSR | S_IRUGO, show_rtc, NULL);

/*
 * Add a device attribute to view/control the delay between eeprom
 * operations.
 */
static ssize_t show_eeprom_delay(struct device *d,
				 struct device_attribute *attr, char *buf)
{
	struct ipw_priv *p = dev_get_drvdata(d);
	int n = p->eeprom_delay;
	return sprintf(buf, "%i\n", n);
}
static ssize_t store_eeprom_delay(struct device *d,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct ipw_priv *p = dev_get_drvdata(d);
	sscanf(buf, "%i", &p->eeprom_delay);
	return strnlen(buf, count);
}

static DEVICE_ATTR(eeprom_delay, S_IWUSR | S_IRUGO,
		   show_eeprom_delay, store_eeprom_delay);

static ssize_t show_command_event_reg(struct device *d,
				      struct device_attribute *attr, char *buf)
{
	u32 reg = 0;
	struct ipw_priv *p = dev_get_drvdata(d);

	reg = ipw_read_reg32(p, IPW_INTERNAL_CMD_EVENT);
	return sprintf(buf, "0x%08x\n", reg);
}
static ssize_t store_command_event_reg(struct device *d,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	u32 reg;
	struct ipw_priv *p = dev_get_drvdata(d);

	sscanf(buf, "%x", &reg);
	ipw_write_reg32(p, IPW_INTERNAL_CMD_EVENT, reg);
	return strnlen(buf, count);
}

static DEVICE_ATTR(command_event_reg, S_IWUSR | S_IRUGO,
		   show_command_event_reg, store_command_event_reg);

static ssize_t show_mem_gpio_reg(struct device *d,
				 struct device_attribute *attr, char *buf)
{
	u32 reg = 0;
	struct ipw_priv *p = dev_get_drvdata(d);

	reg = ipw_read_reg32(p, 0x301100);
	return sprintf(buf, "0x%08x\n", reg);
}
static ssize_t store_mem_gpio_reg(struct device *d,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	u32 reg;
	struct ipw_priv *p = dev_get_drvdata(d);

	sscanf(buf, "%x", &reg);
	ipw_write_reg32(p, 0x301100, reg);
	return strnlen(buf, count);
}

static DEVICE_ATTR(mem_gpio_reg, S_IWUSR | S_IRUGO,
		   show_mem_gpio_reg, store_mem_gpio_reg);

static ssize_t show_indirect_dword(struct device *d,
				   struct device_attribute *attr, char *buf)
{
	u32 reg = 0;
	struct ipw_priv *priv = dev_get_drvdata(d);

	if (priv->status & STATUS_INDIRECT_DWORD)
		reg = ipw_read_reg32(priv, priv->indirect_dword);
	else
		reg = 0;

	return sprintf(buf, "0x%08x\n", reg);
}
static ssize_t store_indirect_dword(struct device *d,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	sscanf(buf, "%x", &priv->indirect_dword);
	priv->status |= STATUS_INDIRECT_DWORD;
	return strnlen(buf, count);
}

static DEVICE_ATTR(indirect_dword, S_IWUSR | S_IRUGO,
		   show_indirect_dword, store_indirect_dword);

static ssize_t show_indirect_byte(struct device *d,
				  struct device_attribute *attr, char *buf)
{
	u8 reg = 0;
	struct ipw_priv *priv = dev_get_drvdata(d);

	if (priv->status & STATUS_INDIRECT_BYTE)
		reg = ipw_read_reg8(priv, priv->indirect_byte);
	else
		reg = 0;

	return sprintf(buf, "0x%02x\n", reg);
}
static ssize_t store_indirect_byte(struct device *d,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	sscanf(buf, "%x", &priv->indirect_byte);
	priv->status |= STATUS_INDIRECT_BYTE;
	return strnlen(buf, count);
}

static DEVICE_ATTR(indirect_byte, S_IWUSR | S_IRUGO,
		   show_indirect_byte, store_indirect_byte);

static ssize_t show_direct_dword(struct device *d,
				 struct device_attribute *attr, char *buf)
{
	u32 reg = 0;
	struct ipw_priv *priv = dev_get_drvdata(d);

	if (priv->status & STATUS_DIRECT_DWORD)
		reg = ipw_read32(priv, priv->direct_dword);
	else
		reg = 0;

	return sprintf(buf, "0x%08x\n", reg);
}
static ssize_t store_direct_dword(struct device *d,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	sscanf(buf, "%x", &priv->direct_dword);
	priv->status |= STATUS_DIRECT_DWORD;
	return strnlen(buf, count);
}

static DEVICE_ATTR(direct_dword, S_IWUSR | S_IRUGO,
		   show_direct_dword, store_direct_dword);

static int rf_kill_active(struct ipw_priv *priv)
{
	if (0 == (ipw_read32(priv, 0x30) & 0x10000))
		priv->status |= STATUS_RF_KILL_HW;
	else
		priv->status &= ~STATUS_RF_KILL_HW;

	return (priv->status & STATUS_RF_KILL_HW) ? 1 : 0;
}

static ssize_t show_rf_kill(struct device *d, struct device_attribute *attr,
			    char *buf)
{
	/* 0 - RF kill not enabled
	   1 - SW based RF kill active (sysfs)
	   2 - HW based RF kill active
	   3 - Both HW and SW baed RF kill active */
	struct ipw_priv *priv = dev_get_drvdata(d);
	int val = ((priv->status & STATUS_RF_KILL_SW) ? 0x1 : 0x0) |
	    (rf_kill_active(priv) ? 0x2 : 0x0);
	return sprintf(buf, "%i\n", val);
}

static int ipw_radio_kill_sw(struct ipw_priv *priv, int disable_radio)
{
	if ((disable_radio ? 1 : 0) ==
	    ((priv->status & STATUS_RF_KILL_SW) ? 1 : 0))
		return 0;

	IPW_DEBUG_RF_KILL("Manual SW RF Kill set to: RADIO  %s\n",
			  disable_radio ? "OFF" : "ON");

	if (disable_radio) {
		priv->status |= STATUS_RF_KILL_SW;

		if (priv->workqueue) {
			cancel_delayed_work(&priv->request_scan);
			cancel_delayed_work(&priv->request_direct_scan);
			cancel_delayed_work(&priv->request_passive_scan);
			cancel_delayed_work(&priv->scan_event);
		}
		queue_work(priv->workqueue, &priv->down);
	} else {
		priv->status &= ~STATUS_RF_KILL_SW;
		if (rf_kill_active(priv)) {
			IPW_DEBUG_RF_KILL("Can not turn radio back on - "
					  "disabled by HW switch\n");
			/* Make sure the RF_KILL check timer is running */
			cancel_delayed_work(&priv->rf_kill);
			queue_delayed_work(priv->workqueue, &priv->rf_kill,
					   round_jiffies_relative(2 * HZ));
		} else
			queue_work(priv->workqueue, &priv->up);
	}

	return 1;
}

static ssize_t store_rf_kill(struct device *d, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	ipw_radio_kill_sw(priv, buf[0] == '1');

	return count;
}

static DEVICE_ATTR(rf_kill, S_IWUSR | S_IRUGO, show_rf_kill, store_rf_kill);

static ssize_t show_speed_scan(struct device *d, struct device_attribute *attr,
			       char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	int pos = 0, len = 0;
	if (priv->config & CFG_SPEED_SCAN) {
		while (priv->speed_scan[pos] != 0)
			len += sprintf(&buf[len], "%d ",
				       priv->speed_scan[pos++]);
		return len + sprintf(&buf[len], "\n");
	}

	return sprintf(buf, "0\n");
}

static ssize_t store_speed_scan(struct device *d, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	int channel, pos = 0;
	const char *p = buf;

	/* list of space separated channels to scan, optionally ending with 0 */
	while ((channel = simple_strtol(p, NULL, 0))) {
		if (pos == MAX_SPEED_SCAN - 1) {
			priv->speed_scan[pos] = 0;
			break;
		}

		if (libipw_is_valid_channel(priv->ieee, channel))
			priv->speed_scan[pos++] = channel;
		else
			IPW_WARNING("Skipping invalid channel request: %d\n",
				    channel);
		p = strchr(p, ' ');
		if (!p)
			break;
		while (*p == ' ' || *p == '\t')
			p++;
	}

	if (pos == 0)
		priv->config &= ~CFG_SPEED_SCAN;
	else {
		priv->speed_scan_pos = 0;
		priv->config |= CFG_SPEED_SCAN;
	}

	return count;
}

static DEVICE_ATTR(speed_scan, S_IWUSR | S_IRUGO, show_speed_scan,
		   store_speed_scan);

static ssize_t show_net_stats(struct device *d, struct device_attribute *attr,
			      char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	return sprintf(buf, "%c\n", (priv->config & CFG_NET_STATS) ? '1' : '0');
}

static ssize_t store_net_stats(struct device *d, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	if (buf[0] == '1')
		priv->config |= CFG_NET_STATS;
	else
		priv->config &= ~CFG_NET_STATS;

	return count;
}

static DEVICE_ATTR(net_stats, S_IWUSR | S_IRUGO,
		   show_net_stats, store_net_stats);

static ssize_t show_channels(struct device *d,
			     struct device_attribute *attr,
			     char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	const struct libipw_geo *geo = libipw_get_geo(priv->ieee);
	int len = 0, i;

	len = sprintf(&buf[len],
		      "Displaying %d channels in 2.4Ghz band "
		      "(802.11bg):\n", geo->bg_channels);

	for (i = 0; i < geo->bg_channels; i++) {
		len += sprintf(&buf[len], "%d: BSS%s%s, %s, Band %s.\n",
			       geo->bg[i].channel,
			       geo->bg[i].flags & LIBIPW_CH_RADAR_DETECT ?
			       " (radar spectrum)" : "",
			       ((geo->bg[i].flags & LIBIPW_CH_NO_IBSS) ||
				(geo->bg[i].flags & LIBIPW_CH_RADAR_DETECT))
			       ? "" : ", IBSS",
			       geo->bg[i].flags & LIBIPW_CH_PASSIVE_ONLY ?
			       "passive only" : "active/passive",
			       geo->bg[i].flags & LIBIPW_CH_B_ONLY ?
			       "B" : "B/G");
	}

	len += sprintf(&buf[len],
		       "Displaying %d channels in 5.2Ghz band "
		       "(802.11a):\n", geo->a_channels);
	for (i = 0; i < geo->a_channels; i++) {
		len += sprintf(&buf[len], "%d: BSS%s%s, %s.\n",
			       geo->a[i].channel,
			       geo->a[i].flags & LIBIPW_CH_RADAR_DETECT ?
			       " (radar spectrum)" : "",
			       ((geo->a[i].flags & LIBIPW_CH_NO_IBSS) ||
				(geo->a[i].flags & LIBIPW_CH_RADAR_DETECT))
			       ? "" : ", IBSS",
			       geo->a[i].flags & LIBIPW_CH_PASSIVE_ONLY ?
			       "passive only" : "active/passive");
	}

	return len;
}

static DEVICE_ATTR(channels, S_IRUSR, show_channels, NULL);

static void notify_wx_assoc_event(struct ipw_priv *priv)
{
	union iwreq_data wrqu;
	wrqu.ap_addr.sa_family = ARPHRD_ETHER;
	if (priv->status & STATUS_ASSOCIATED)
		memcpy(wrqu.ap_addr.sa_data, priv->bssid, ETH_ALEN);
	else
		memset(wrqu.ap_addr.sa_data, 0, ETH_ALEN);
	wireless_send_event(priv->net_dev, SIOCGIWAP, &wrqu, NULL);
}

static void ipw_irq_tasklet(struct ipw_priv *priv)
{
	u32 inta, inta_mask, handled = 0;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&priv->irq_lock, flags);

	inta = ipw_read32(priv, IPW_INTA_RW);
	inta_mask = ipw_read32(priv, IPW_INTA_MASK_R);
	inta &= (IPW_INTA_MASK_ALL & inta_mask);

	/* Add any cached INTA values that need to be handled */
	inta |= priv->isr_inta;

	spin_unlock_irqrestore(&priv->irq_lock, flags);

	spin_lock_irqsave(&priv->lock, flags);

	/* handle all the justifications for the interrupt */
	if (inta & IPW_INTA_BIT_RX_TRANSFER) {
		ipw_rx(priv);
		handled |= IPW_INTA_BIT_RX_TRANSFER;
	}

	if (inta & IPW_INTA_BIT_TX_CMD_QUEUE) {
		IPW_DEBUG_HC("Command completed.\n");
		rc = ipw_queue_tx_reclaim(priv, &priv->txq_cmd, -1);
		priv->status &= ~STATUS_HCMD_ACTIVE;
		wake_up_interruptible(&priv->wait_command_queue);
		handled |= IPW_INTA_BIT_TX_CMD_QUEUE;
	}

	if (inta & IPW_INTA_BIT_TX_QUEUE_1) {
		IPW_DEBUG_TX("TX_QUEUE_1\n");
		rc = ipw_queue_tx_reclaim(priv, &priv->txq[0], 0);
		handled |= IPW_INTA_BIT_TX_QUEUE_1;
	}

	if (inta & IPW_INTA_BIT_TX_QUEUE_2) {
		IPW_DEBUG_TX("TX_QUEUE_2\n");
		rc = ipw_queue_tx_reclaim(priv, &priv->txq[1], 1);
		handled |= IPW_INTA_BIT_TX_QUEUE_2;
	}

	if (inta & IPW_INTA_BIT_TX_QUEUE_3) {
		IPW_DEBUG_TX("TX_QUEUE_3\n");
		rc = ipw_queue_tx_reclaim(priv, &priv->txq[2], 2);
		handled |= IPW_INTA_BIT_TX_QUEUE_3;
	}

	if (inta & IPW_INTA_BIT_TX_QUEUE_4) {
		IPW_DEBUG_TX("TX_QUEUE_4\n");
		rc = ipw_queue_tx_reclaim(priv, &priv->txq[3], 3);
		handled |= IPW_INTA_BIT_TX_QUEUE_4;
	}

	if (inta & IPW_INTA_BIT_STATUS_CHANGE) {
		IPW_WARNING("STATUS_CHANGE\n");
		handled |= IPW_INTA_BIT_STATUS_CHANGE;
	}

	if (inta & IPW_INTA_BIT_BEACON_PERIOD_EXPIRED) {
		IPW_WARNING("TX_PERIOD_EXPIRED\n");
		handled |= IPW_INTA_BIT_BEACON_PERIOD_EXPIRED;
	}

	if (inta & IPW_INTA_BIT_SLAVE_MODE_HOST_CMD_DONE) {
		IPW_WARNING("HOST_CMD_DONE\n");
		handled |= IPW_INTA_BIT_SLAVE_MODE_HOST_CMD_DONE;
	}

	if (inta & IPW_INTA_BIT_FW_INITIALIZATION_DONE) {
		IPW_WARNING("FW_INITIALIZATION_DONE\n");
		handled |= IPW_INTA_BIT_FW_INITIALIZATION_DONE;
	}

	if (inta & IPW_INTA_BIT_FW_CARD_DISABLE_PHY_OFF_DONE) {
		IPW_WARNING("PHY_OFF_DONE\n");
		handled |= IPW_INTA_BIT_FW_CARD_DISABLE_PHY_OFF_DONE;
	}

	if (inta & IPW_INTA_BIT_RF_KILL_DONE) {
		IPW_DEBUG_RF_KILL("RF_KILL_DONE\n");
		priv->status |= STATUS_RF_KILL_HW;
		wake_up_interruptible(&priv->wait_command_queue);
		priv->status &= ~(STATUS_ASSOCIATED | STATUS_ASSOCIATING);
		cancel_delayed_work(&priv->request_scan);
		cancel_delayed_work(&priv->request_direct_scan);
		cancel_delayed_work(&priv->request_passive_scan);
		cancel_delayed_work(&priv->scan_event);
		schedule_work(&priv->link_down);
		queue_delayed_work(priv->workqueue, &priv->rf_kill, 2 * HZ);
		handled |= IPW_INTA_BIT_RF_KILL_DONE;
	}

	if (inta & IPW_INTA_BIT_FATAL_ERROR) {
		IPW_WARNING("Firmware error detected.  Restarting.\n");
		if (priv->error) {
			IPW_DEBUG_FW("Sysfs 'error' log already exists.\n");
			if (ipw_debug_level & IPW_DL_FW_ERRORS) {
				struct ipw_fw_error *error =
				    ipw_alloc_error_log(priv);
				ipw_dump_error_log(priv, error);
				kfree(error);
			}
		} else {
			priv->error = ipw_alloc_error_log(priv);
			if (priv->error)
				IPW_DEBUG_FW("Sysfs 'error' log captured.\n");
			else
				IPW_DEBUG_FW("Error allocating sysfs 'error' "
					     "log.\n");
			if (ipw_debug_level & IPW_DL_FW_ERRORS)
				ipw_dump_error_log(priv, priv->error);
		}

		/* XXX: If hardware encryption is for WPA/WPA2,
		 * we have to notify the supplicant. */
		if (priv->ieee->sec.encrypt) {
			priv->status &= ~STATUS_ASSOCIATED;
			notify_wx_assoc_event(priv);
		}

		/* Keep the restart process from trying to send host
		 * commands by clearing the INIT status bit */
		priv->status &= ~STATUS_INIT;

		/* Cancel currently queued command. */
		priv->status &= ~STATUS_HCMD_ACTIVE;
		wake_up_interruptible(&priv->wait_command_queue);

		queue_work(priv->workqueue, &priv->adapter_restart);
		handled |= IPW_INTA_BIT_FATAL_ERROR;
	}

	if (inta & IPW_INTA_BIT_PARITY_ERROR) {
		IPW_ERROR("Parity error\n");
		handled |= IPW_INTA_BIT_PARITY_ERROR;
	}

	if (handled != inta) {
		IPW_ERROR("Unhandled INTA bits 0x%08x\n", inta & ~handled);
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	/* enable all interrupts */
	ipw_enable_interrupts(priv);
}

#define IPW_CMD(x) case IPW_CMD_ ## x : return #x
static char *get_cmd_string(u8 cmd)
{
	switch (cmd) {
		IPW_CMD(HOST_COMPLETE);
		IPW_CMD(POWER_DOWN);
		IPW_CMD(SYSTEM_CONFIG);
		IPW_CMD(MULTICAST_ADDRESS);
		IPW_CMD(SSID);
		IPW_CMD(ADAPTER_ADDRESS);
		IPW_CMD(PORT_TYPE);
		IPW_CMD(RTS_THRESHOLD);
		IPW_CMD(FRAG_THRESHOLD);
		IPW_CMD(POWER_MODE);
		IPW_CMD(WEP_KEY);
		IPW_CMD(TGI_TX_KEY);
		IPW_CMD(SCAN_REQUEST);
		IPW_CMD(SCAN_REQUEST_EXT);
		IPW_CMD(ASSOCIATE);
		IPW_CMD(SUPPORTED_RATES);
		IPW_CMD(SCAN_ABORT);
		IPW_CMD(TX_FLUSH);
		IPW_CMD(QOS_PARAMETERS);
		IPW_CMD(DINO_CONFIG);
		IPW_CMD(RSN_CAPABILITIES);
		IPW_CMD(RX_KEY);
		IPW_CMD(CARD_DISABLE);
		IPW_CMD(SEED_NUMBER);
		IPW_CMD(TX_POWER);
		IPW_CMD(COUNTRY_INFO);
		IPW_CMD(AIRONET_INFO);
		IPW_CMD(AP_TX_POWER);
		IPW_CMD(CCKM_INFO);
		IPW_CMD(CCX_VER_INFO);
		IPW_CMD(SET_CALIBRATION);
		IPW_CMD(SENSITIVITY_CALIB);
		IPW_CMD(RETRY_LIMIT);
		IPW_CMD(IPW_PRE_POWER_DOWN);
		IPW_CMD(VAP_BEACON_TEMPLATE);
		IPW_CMD(VAP_DTIM_PERIOD);
		IPW_CMD(EXT_SUPPORTED_RATES);
		IPW_CMD(VAP_LOCAL_TX_PWR_CONSTRAINT);
		IPW_CMD(VAP_QUIET_INTERVALS);
		IPW_CMD(VAP_CHANNEL_SWITCH);
		IPW_CMD(VAP_MANDATORY_CHANNELS);
		IPW_CMD(VAP_CELL_PWR_LIMIT);
		IPW_CMD(VAP_CF_PARAM_SET);
		IPW_CMD(VAP_SET_BEACONING_STATE);
		IPW_CMD(MEASUREMENT);
		IPW_CMD(POWER_CAPABILITY);
		IPW_CMD(SUPPORTED_CHANNELS);
		IPW_CMD(TPC_REPORT);
		IPW_CMD(WME_INFO);
		IPW_CMD(PRODUCTION_COMMAND);
	default:
		return "UNKNOWN";
	}
}

#define HOST_COMPLETE_TIMEOUT HZ

static int __ipw_send_cmd(struct ipw_priv *priv, struct host_cmd *cmd)
{
	int rc = 0;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	if (priv->status & STATUS_HCMD_ACTIVE) {
		IPW_ERROR("Failed to send %s: Already sending a command.\n",
			  get_cmd_string(cmd->cmd));
		spin_unlock_irqrestore(&priv->lock, flags);
		return -EAGAIN;
	}

	priv->status |= STATUS_HCMD_ACTIVE;

	if (priv->cmdlog) {
		priv->cmdlog[priv->cmdlog_pos].jiffies = jiffies;
		priv->cmdlog[priv->cmdlog_pos].cmd.cmd = cmd->cmd;
		priv->cmdlog[priv->cmdlog_pos].cmd.len = cmd->len;
		memcpy(priv->cmdlog[priv->cmdlog_pos].cmd.param, cmd->param,
		       cmd->len);
		priv->cmdlog[priv->cmdlog_pos].retcode = -1;
	}

	IPW_DEBUG_HC("%s command (#%d) %d bytes: 0x%08X\n",
		     get_cmd_string(cmd->cmd), cmd->cmd, cmd->len,
		     priv->status);

#ifndef DEBUG_CMD_WEP_KEY
	if (cmd->cmd == IPW_CMD_WEP_KEY)
		IPW_DEBUG_HC("WEP_KEY command masked out for secure.\n");
	else
#endif
		printk_buf(IPW_DL_HOST_COMMAND, (u8 *) cmd->param, cmd->len);

	rc = ipw_queue_tx_hcmd(priv, cmd->cmd, cmd->param, cmd->len, 0);
	if (rc) {
		priv->status &= ~STATUS_HCMD_ACTIVE;
		IPW_ERROR("Failed to send %s: Reason %d\n",
			  get_cmd_string(cmd->cmd), rc);
		spin_unlock_irqrestore(&priv->lock, flags);
		goto exit;
	}
	spin_unlock_irqrestore(&priv->lock, flags);

	rc = wait_event_interruptible_timeout(priv->wait_command_queue,
					      !(priv->
						status & STATUS_HCMD_ACTIVE),
					      HOST_COMPLETE_TIMEOUT);
	if (rc == 0) {
		spin_lock_irqsave(&priv->lock, flags);
		if (priv->status & STATUS_HCMD_ACTIVE) {
			IPW_ERROR("Failed to send %s: Command timed out.\n",
				  get_cmd_string(cmd->cmd));
			priv->status &= ~STATUS_HCMD_ACTIVE;
			spin_unlock_irqrestore(&priv->lock, flags);
			rc = -EIO;
			goto exit;
		}
		spin_unlock_irqrestore(&priv->lock, flags);
	} else
		rc = 0;

	if (priv->status & STATUS_RF_KILL_HW) {
		IPW_ERROR("Failed to send %s: Aborted due to RF kill switch.\n",
			  get_cmd_string(cmd->cmd));
		rc = -EIO;
		goto exit;
	}

      exit:
	if (priv->cmdlog) {
		priv->cmdlog[priv->cmdlog_pos++].retcode = rc;
		priv->cmdlog_pos %= priv->cmdlog_len;
	}
	return rc;
}

static int ipw_send_cmd_simple(struct ipw_priv *priv, u8 command)
{
	struct host_cmd cmd = {
		.cmd = command,
	};

	return __ipw_send_cmd(priv, &cmd);
}

static int ipw_send_cmd_pdu(struct ipw_priv *priv, u8 command, u8 len,
			    void *data)
{
	struct host_cmd cmd = {
		.cmd = command,
		.len = len,
		.param = data,
	};

	return __ipw_send_cmd(priv, &cmd);
}

static int ipw_send_host_complete(struct ipw_priv *priv)
{
	if (!priv) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	return ipw_send_cmd_simple(priv, IPW_CMD_HOST_COMPLETE);
}

static int ipw_send_system_config(struct ipw_priv *priv)
{
	return ipw_send_cmd_pdu(priv, IPW_CMD_SYSTEM_CONFIG,
				sizeof(priv->sys_config),
				&priv->sys_config);
}

static int ipw_send_ssid(struct ipw_priv *priv, u8 * ssid, int len)
{
	if (!priv || !ssid) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	return ipw_send_cmd_pdu(priv, IPW_CMD_SSID, min(len, IW_ESSID_MAX_SIZE),
				ssid);
}

static int ipw_send_adapter_address(struct ipw_priv *priv, u8 * mac)
{
	if (!priv || !mac) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	IPW_DEBUG_INFO("%s: Setting MAC to %pM\n",
		       priv->net_dev->name, mac);

	return ipw_send_cmd_pdu(priv, IPW_CMD_ADAPTER_ADDRESS, ETH_ALEN, mac);
}

/*
 * NOTE: This must be executed from our workqueue as it results in udelay
 * being called which may corrupt the keyboard if executed on default
 * workqueue
 */
static void ipw_adapter_restart(void *adapter)
{
	struct ipw_priv *priv = adapter;

	if (priv->status & STATUS_RF_KILL_MASK)
		return;

	ipw_down(priv);

	if (priv->assoc_network &&
	    (priv->assoc_network->capability & WLAN_CAPABILITY_IBSS))
		ipw_remove_current_network(priv);

	if (ipw_up(priv)) {
		IPW_ERROR("Failed to up device\n");
		return;
	}
}

static void ipw_bg_adapter_restart(struct work_struct *work)
{
	struct ipw_priv *priv =
		container_of(work, struct ipw_priv, adapter_restart);
	mutex_lock(&priv->mutex);
	ipw_adapter_restart(priv);
	mutex_unlock(&priv->mutex);
}

#define IPW_SCAN_CHECK_WATCHDOG (5 * HZ)

static void ipw_scan_check(void *data)
{
	struct ipw_priv *priv = data;
	if (priv->status & (STATUS_SCANNING | STATUS_SCAN_ABORTING)) {
		IPW_DEBUG_SCAN("Scan completion watchdog resetting "
			       "adapter after (%dms).\n",
			       jiffies_to_msecs(IPW_SCAN_CHECK_WATCHDOG));
		queue_work(priv->workqueue, &priv->adapter_restart);
	}
}

static void ipw_bg_scan_check(struct work_struct *work)
{
	struct ipw_priv *priv =
		container_of(work, struct ipw_priv, scan_check.work);
	mutex_lock(&priv->mutex);
	ipw_scan_check(priv);
	mutex_unlock(&priv->mutex);
}

static int ipw_send_scan_request_ext(struct ipw_priv *priv,
				     struct ipw_scan_request_ext *request)
{
	return ipw_send_cmd_pdu(priv, IPW_CMD_SCAN_REQUEST_EXT,
				sizeof(*request), request);
}

static int ipw_send_scan_abort(struct ipw_priv *priv)
{
	if (!priv) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	return ipw_send_cmd_simple(priv, IPW_CMD_SCAN_ABORT);
}

static int ipw_set_sensitivity(struct ipw_priv *priv, u16 sens)
{
	struct ipw_sensitivity_calib calib = {
		.beacon_rssi_raw = cpu_to_le16(sens),
	};

	return ipw_send_cmd_pdu(priv, IPW_CMD_SENSITIVITY_CALIB, sizeof(calib),
				&calib);
}

static int ipw_send_associate(struct ipw_priv *priv,
			      struct ipw_associate *associate)
{
	if (!priv || !associate) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	return ipw_send_cmd_pdu(priv, IPW_CMD_ASSOCIATE, sizeof(*associate),
				associate);
}

static int ipw_send_supported_rates(struct ipw_priv *priv,
				    struct ipw_supported_rates *rates)
{
	if (!priv || !rates) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	return ipw_send_cmd_pdu(priv, IPW_CMD_SUPPORTED_RATES, sizeof(*rates),
				rates);
}

static int ipw_set_random_seed(struct ipw_priv *priv)
{
	u32 val;

	if (!priv) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	get_random_bytes(&val, sizeof(val));

	return ipw_send_cmd_pdu(priv, IPW_CMD_SEED_NUMBER, sizeof(val), &val);
}

static int ipw_send_card_disable(struct ipw_priv *priv, u32 phy_off)
{
	__le32 v = cpu_to_le32(phy_off);
	if (!priv) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	return ipw_send_cmd_pdu(priv, IPW_CMD_CARD_DISABLE, sizeof(v), &v);
}

static int ipw_send_tx_power(struct ipw_priv *priv, struct ipw_tx_power *power)
{
	if (!priv || !power) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	return ipw_send_cmd_pdu(priv, IPW_CMD_TX_POWER, sizeof(*power), power);
}

static int ipw_set_tx_power(struct ipw_priv *priv)
{
	const struct libipw_geo *geo = libipw_get_geo(priv->ieee);
	struct ipw_tx_power tx_power;
	s8 max_power;
	int i;

	memset(&tx_power, 0, sizeof(tx_power));

	/* configure device for 'G' band */
	tx_power.ieee_mode = IPW_G_MODE;
	tx_power.num_channels = geo->bg_channels;
	for (i = 0; i < geo->bg_channels; i++) {
		max_power = geo->bg[i].max_power;
		tx_power.channels_tx_power[i].channel_number =
		    geo->bg[i].channel;
		tx_power.channels_tx_power[i].tx_power = max_power ?
		    min(max_power, priv->tx_power) : priv->tx_power;
	}
	if (ipw_send_tx_power(priv, &tx_power))
		return -EIO;

	/* configure device to also handle 'B' band */
	tx_power.ieee_mode = IPW_B_MODE;
	if (ipw_send_tx_power(priv, &tx_power))
		return -EIO;

	/* configure device to also handle 'A' band */
	if (priv->ieee->abg_true) {
		tx_power.ieee_mode = IPW_A_MODE;
		tx_power.num_channels = geo->a_channels;
		for (i = 0; i < tx_power.num_channels; i++) {
			max_power = geo->a[i].max_power;
			tx_power.channels_tx_power[i].channel_number =
			    geo->a[i].channel;
			tx_power.channels_tx_power[i].tx_power = max_power ?
			    min(max_power, priv->tx_power) : priv->tx_power;
		}
		if (ipw_send_tx_power(priv, &tx_power))
			return -EIO;
	}
	return 0;
}

static int ipw_send_rts_threshold(struct ipw_priv *priv, u16 rts)
{
	struct ipw_rts_threshold rts_threshold = {
		.rts_threshold = cpu_to_le16(rts),
	};

	if (!priv) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	return ipw_send_cmd_pdu(priv, IPW_CMD_RTS_THRESHOLD,
				sizeof(rts_threshold), &rts_threshold);
}

static int ipw_send_frag_threshold(struct ipw_priv *priv, u16 frag)
{
	struct ipw_frag_threshold frag_threshold = {
		.frag_threshold = cpu_to_le16(frag),
	};

	if (!priv) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	return ipw_send_cmd_pdu(priv, IPW_CMD_FRAG_THRESHOLD,
				sizeof(frag_threshold), &frag_threshold);
}

static int ipw_send_power_mode(struct ipw_priv *priv, u32 mode)
{
	__le32 param;

	if (!priv) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	/* If on battery, set to 3, if AC set to CAM, else user
	 * level */
	switch (mode) {
	case IPW_POWER_BATTERY:
		param = cpu_to_le32(IPW_POWER_INDEX_3);
		break;
	case IPW_POWER_AC:
		param = cpu_to_le32(IPW_POWER_MODE_CAM);
		break;
	default:
		param = cpu_to_le32(mode);
		break;
	}

	return ipw_send_cmd_pdu(priv, IPW_CMD_POWER_MODE, sizeof(param),
				&param);
}

static int ipw_send_retry_limit(struct ipw_priv *priv, u8 slimit, u8 llimit)
{
	struct ipw_retry_limit retry_limit = {
		.short_retry_limit = slimit,
		.long_retry_limit = llimit
	};

	if (!priv) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	return ipw_send_cmd_pdu(priv, IPW_CMD_RETRY_LIMIT, sizeof(retry_limit),
				&retry_limit);
}

/*
 * The IPW device contains a Microwire compatible EEPROM that stores
 * various data like the MAC address.  Usually the firmware has exclusive
 * access to the eeprom, but during device initialization (before the
 * device driver has sent the HostComplete command to the firmware) the
 * device driver has read access to the EEPROM by way of indirect addressing
 * through a couple of memory mapped registers.
 *
 * The following is a simplified implementation for pulling data out of the
 * the eeprom, along with some helper functions to find information in
 * the per device private data's copy of the eeprom.
 *
 * NOTE: To better understand how these functions work (i.e what is a chip
 *       select and why do have to keep driving the eeprom clock?), read
 *       just about any data sheet for a Microwire compatible EEPROM.
 */

/* write a 32 bit value into the indirect accessor register */
static inline void eeprom_write_reg(struct ipw_priv *p, u32 data)
{
	ipw_write_reg32(p, FW_MEM_REG_EEPROM_ACCESS, data);

	/* the eeprom requires some time to complete the operation */
	udelay(p->eeprom_delay);

	return;
}

/* perform a chip select operation */
static void eeprom_cs(struct ipw_priv *priv)
{
	eeprom_write_reg(priv, 0);
	eeprom_write_reg(priv, EEPROM_BIT_CS);
	eeprom_write_reg(priv, EEPROM_BIT_CS | EEPROM_BIT_SK);
	eeprom_write_reg(priv, EEPROM_BIT_CS);
}

/* perform a chip select operation */
static void eeprom_disable_cs(struct ipw_priv *priv)
{
	eeprom_write_reg(priv, EEPROM_BIT_CS);
	eeprom_write_reg(priv, 0);
	eeprom_write_reg(priv, EEPROM_BIT_SK);
}

/* push a single bit down to the eeprom */
static inline void eeprom_write_bit(struct ipw_priv *p, u8 bit)
{
	int d = (bit ? EEPROM_BIT_DI : 0);
	eeprom_write_reg(p, EEPROM_BIT_CS | d);
	eeprom_write_reg(p, EEPROM_BIT_CS | d | EEPROM_BIT_SK);
}

/* push an opcode followed by an address down to the eeprom */
static void eeprom_op(struct ipw_priv *priv, u8 op, u8 addr)
{
	int i;

	eeprom_cs(priv);
	eeprom_write_bit(priv, 1);
	eeprom_write_bit(priv, op & 2);
	eeprom_write_bit(priv, op & 1);
	for (i = 7; i >= 0; i--) {
		eeprom_write_bit(priv, addr & (1 << i));
	}
}

/* pull 16 bits off the eeprom, one bit at a time */
static u16 eeprom_read_u16(struct ipw_priv *priv, u8 addr)
{
	int i;
	u16 r = 0;

	/* Send READ Opcode */
	eeprom_op(priv, EEPROM_CMD_READ, addr);

	/* Send dummy bit */
	eeprom_write_reg(priv, EEPROM_BIT_CS);

	/* Read the byte off the eeprom one bit at a time */
	for (i = 0; i < 16; i++) {
		u32 data = 0;
		eeprom_write_reg(priv, EEPROM_BIT_CS | EEPROM_BIT_SK);
		eeprom_write_reg(priv, EEPROM_BIT_CS);
		data = ipw_read_reg32(priv, FW_MEM_REG_EEPROM_ACCESS);
		r = (r << 1) | ((data & EEPROM_BIT_DO) ? 1 : 0);
	}

	/* Send another dummy bit */
	eeprom_write_reg(priv, 0);
	eeprom_disable_cs(priv);

	return r;
}

/* helper function for pulling the mac address out of the private */
/* data's copy of the eeprom data                                 */
static void eeprom_parse_mac(struct ipw_priv *priv, u8 * mac)
{
	memcpy(mac, &priv->eeprom[EEPROM_MAC_ADDRESS], 6);
}

/*
 * Either the device driver (i.e. the host) or the firmware can
 * load eeprom data into the designated region in SRAM.  If neither
 * happens then the FW will shutdown with a fatal error.
 *
 * In order to signal the FW to load the EEPROM, the EEPROM_LOAD_DISABLE
 * bit needs region of shared SRAM needs to be non-zero.
 */
static void ipw_eeprom_init_sram(struct ipw_priv *priv)
{
	int i;
	__le16 *eeprom = (__le16 *) priv->eeprom;

	IPW_DEBUG_TRACE(">>\n");

	/* read entire contents of eeprom into private buffer */
	for (i = 0; i < 128; i++)
		eeprom[i] = cpu_to_le16(eeprom_read_u16(priv, (u8) i));

	/*
	   If the data looks correct, then copy it to our private
	   copy.  Otherwise let the firmware know to perform the operation
	   on its own.
	 */
	if (priv->eeprom[EEPROM_VERSION] != 0) {
		IPW_DEBUG_INFO("Writing EEPROM data into SRAM\n");

		/* write the eeprom data to sram */
		for (i = 0; i < IPW_EEPROM_IMAGE_SIZE; i++)
			ipw_write8(priv, IPW_EEPROM_DATA + i, priv->eeprom[i]);

		/* Do not load eeprom data on fatal error or suspend */
		ipw_write32(priv, IPW_EEPROM_LOAD_DISABLE, 0);
	} else {
		IPW_DEBUG_INFO("Enabling FW initializationg of SRAM\n");

		/* Load eeprom data on fatal error or suspend */
		ipw_write32(priv, IPW_EEPROM_LOAD_DISABLE, 1);
	}

	IPW_DEBUG_TRACE("<<\n");
}

static void ipw_zero_memory(struct ipw_priv *priv, u32 start, u32 count)
{
	count >>= 2;
	if (!count)
		return;
	_ipw_write32(priv, IPW_AUTOINC_ADDR, start);
	while (count--)
		_ipw_write32(priv, IPW_AUTOINC_DATA, 0);
}

static inline void ipw_fw_dma_reset_command_blocks(struct ipw_priv *priv)
{
	ipw_zero_memory(priv, IPW_SHARED_SRAM_DMA_CONTROL,
			CB_NUMBER_OF_ELEMENTS_SMALL *
			sizeof(struct command_block));
}

static int ipw_fw_dma_enable(struct ipw_priv *priv)
{				/* start dma engine but no transfers yet */

	IPW_DEBUG_FW(">> : \n");

	/* Start the dma */
	ipw_fw_dma_reset_command_blocks(priv);

	/* Write CB base address */
	ipw_write_reg32(priv, IPW_DMA_I_CB_BASE, IPW_SHARED_SRAM_DMA_CONTROL);

	IPW_DEBUG_FW("<< : \n");
	return 0;
}

static void ipw_fw_dma_abort(struct ipw_priv *priv)
{
	u32 control = 0;

	IPW_DEBUG_FW(">> :\n");

	/* set the Stop and Abort bit */
	control = DMA_CONTROL_SMALL_CB_CONST_VALUE | DMA_CB_STOP_AND_ABORT;
	ipw_write_reg32(priv, IPW_DMA_I_DMA_CONTROL, control);
	priv->sram_desc.last_cb_index = 0;

	IPW_DEBUG_FW("<< \n");
}

static int ipw_fw_dma_write_command_block(struct ipw_priv *priv, int index,
					  struct command_block *cb)
{
	u32 address =
	    IPW_SHARED_SRAM_DMA_CONTROL +
	    (sizeof(struct command_block) * index);
	IPW_DEBUG_FW(">> :\n");

	ipw_write_indirect(priv, address, (u8 *) cb,
			   (int)sizeof(struct command_block));

	IPW_DEBUG_FW("<< :\n");
	return 0;

}

static int ipw_fw_dma_kick(struct ipw_priv *priv)
{
	u32 control = 0;
	u32 index = 0;

	IPW_DEBUG_FW(">> :\n");

	for (index = 0; index < priv->sram_desc.last_cb_index; index++)
		ipw_fw_dma_write_command_block(priv, index,
					       &priv->sram_desc.cb_list[index]);

	/* Enable the DMA in the CSR register */
	ipw_clear_bit(priv, IPW_RESET_REG,
		      IPW_RESET_REG_MASTER_DISABLED |
		      IPW_RESET_REG_STOP_MASTER);

	/* Set the Start bit. */
	control = DMA_CONTROL_SMALL_CB_CONST_VALUE | DMA_CB_START;
	ipw_write_reg32(priv, IPW_DMA_I_DMA_CONTROL, control);

	IPW_DEBUG_FW("<< :\n");
	return 0;
}

static void ipw_fw_dma_dump_command_block(struct ipw_priv *priv)
{
	u32 address;
	u32 register_value = 0;
	u32 cb_fields_address = 0;

	IPW_DEBUG_FW(">> :\n");
	address = ipw_read_reg32(priv, IPW_DMA_I_CURRENT_CB);
	IPW_DEBUG_FW_INFO("Current CB is 0x%x \n", address);

	/* Read the DMA Controlor register */
	register_value = ipw_read_reg32(priv, IPW_DMA_I_DMA_CONTROL);
	IPW_DEBUG_FW_INFO("IPW_DMA_I_DMA_CONTROL is 0x%x \n", register_value);

	/* Print the CB values */
	cb_fields_address = address;
	register_value = ipw_read_reg32(priv, cb_fields_address);
	IPW_DEBUG_FW_INFO("Current CB ControlField is 0x%x \n", register_value);

	cb_fields_address += sizeof(u32);
	register_value = ipw_read_reg32(priv, cb_fields_address);
	IPW_DEBUG_FW_INFO("Current CB Source Field is 0x%x \n", register_value);

	cb_fields_address += sizeof(u32);
	register_value = ipw_read_reg32(priv, cb_fields_address);
	IPW_DEBUG_FW_INFO("Current CB Destination Field is 0x%x \n",
			  register_value);

	cb_fields_address += sizeof(u32);
	register_value = ipw_read_reg32(priv, cb_fields_address);
	IPW_DEBUG_FW_INFO("Current CB Status Field is 0x%x \n", register_value);

	IPW_DEBUG_FW(">> :\n");
}

static int ipw_fw_dma_command_block_index(struct ipw_priv *priv)
{
	u32 current_cb_address = 0;
	u32 current_cb_index = 0;

	IPW_DEBUG_FW("<< :\n");
	current_cb_address = ipw_read_reg32(priv, IPW_DMA_I_CURRENT_CB);

	current_cb_index = (current_cb_address - IPW_SHARED_SRAM_DMA_CONTROL) /
	    sizeof(struct command_block);

	IPW_DEBUG_FW_INFO("Current CB index 0x%x address = 0x%X \n",
			  current_cb_index, current_cb_address);

	IPW_DEBUG_FW(">> :\n");
	return current_cb_index;

}

static int ipw_fw_dma_add_command_block(struct ipw_priv *priv,
					u32 src_address,
					u32 dest_address,
					u32 length,
					int interrupt_enabled, int is_last)
{

	u32 control = CB_VALID | CB_SRC_LE | CB_DEST_LE | CB_SRC_AUTOINC |
	    CB_SRC_IO_GATED | CB_DEST_AUTOINC | CB_SRC_SIZE_LONG |
	    CB_DEST_SIZE_LONG;
	struct command_block *cb;
	u32 last_cb_element = 0;

	IPW_DEBUG_FW_INFO("src_address=0x%x dest_address=0x%x length=0x%x\n",
			  src_address, dest_address, length);

	if (priv->sram_desc.last_cb_index >= CB_NUMBER_OF_ELEMENTS_SMALL)
		return -1;

	last_cb_element = priv->sram_desc.last_cb_index;
	cb = &priv->sram_desc.cb_list[last_cb_element];
	priv->sram_desc.last_cb_index++;

	/* Calculate the new CB control word */
	if (interrupt_enabled)
		control |= CB_INT_ENABLED;

	if (is_last)
		control |= CB_LAST_VALID;

	control |= length;

	/* Calculate the CB Element's checksum value */
	cb->status = control ^ src_address ^ dest_address;

	/* Copy the Source and Destination addresses */
	cb->dest_addr = dest_address;
	cb->source_addr = src_address;

	/* Copy the Control Word last */
	cb->control = control;

	return 0;
}

static int ipw_fw_dma_add_buffer(struct ipw_priv *priv, dma_addr_t *src_address,
				 int nr, u32 dest_address, u32 len)
{
	int ret, i;
	u32 size;

	IPW_DEBUG_FW(">> \n");
	IPW_DEBUG_FW_INFO("nr=%d dest_address=0x%x len=0x%x\n",
			  nr, dest_address, len);

	for (i = 0; i < nr; i++) {
		size = min_t(u32, len - i * CB_MAX_LENGTH, CB_MAX_LENGTH);
		ret = ipw_fw_dma_add_command_block(priv, src_address[i],
						   dest_address +
						   i * CB_MAX_LENGTH, size,
						   0, 0);
		if (ret) {
			IPW_DEBUG_FW_INFO(": Failed\n");
			return -1;
		} else
			IPW_DEBUG_FW_INFO(": Added new cb\n");
	}

	IPW_DEBUG_FW("<< \n");
	return 0;
}

static int ipw_fw_dma_wait(struct ipw_priv *priv)
{
	u32 current_index = 0, previous_index;
	u32 watchdog = 0;

	IPW_DEBUG_FW(">> : \n");

	current_index = ipw_fw_dma_command_block_index(priv);
	IPW_DEBUG_FW_INFO("sram_desc.last_cb_index:0x%08X\n",
			  (int)priv->sram_desc.last_cb_index);

	while (current_index < priv->sram_desc.last_cb_index) {
		udelay(50);
		previous_index = current_index;
		current_index = ipw_fw_dma_command_block_index(priv);

		if (previous_index < current_index) {
			watchdog = 0;
			continue;
		}
		if (++watchdog > 400) {
			IPW_DEBUG_FW_INFO("Timeout\n");
			ipw_fw_dma_dump_command_block(priv);
			ipw_fw_dma_abort(priv);
			return -1;
		}
	}

	ipw_fw_dma_abort(priv);

	/*Disable the DMA in the CSR register */
	ipw_set_bit(priv, IPW_RESET_REG,
		    IPW_RESET_REG_MASTER_DISABLED | IPW_RESET_REG_STOP_MASTER);

	IPW_DEBUG_FW("<< dmaWaitSync \n");
	return 0;
}

static void ipw_remove_current_network(struct ipw_priv *priv)
{
	struct list_head *element, *safe;
	struct libipw_network *network = NULL;
	unsigned long flags;

	spin_lock_irqsave(&priv->ieee->lock, flags);
	list_for_each_safe(element, safe, &priv->ieee->network_list) {
		network = list_entry(element, struct libipw_network, list);
		if (!memcmp(network->bssid, priv->bssid, ETH_ALEN)) {
			list_del(element);
			list_add_tail(&network->list,
				      &priv->ieee->network_free_list);
		}
	}
	spin_unlock_irqrestore(&priv->ieee->lock, flags);
}

/**
 * Check that card is still alive.
 * Reads debug register from domain0.
 * If card is present, pre-defined value should
 * be found there.
 *
 * @param priv
 * @return 1 if card is present, 0 otherwise
 */
static inline int ipw_alive(struct ipw_priv *priv)
{
	return ipw_read32(priv, 0x90) == 0xd55555d5;
}

/* timeout in msec, attempted in 10-msec quanta */
static int ipw_poll_bit(struct ipw_priv *priv, u32 addr, u32 mask,
			       int timeout)
{
	int i = 0;

	do {
		if ((ipw_read32(priv, addr) & mask) == mask)
			return i;
		mdelay(10);
		i += 10;
	} while (i < timeout);

	return -ETIME;
}

/* These functions load the firmware and micro code for the operation of
 * the ipw hardware.  It assumes the buffer has all the bits for the
 * image and the caller is handling the memory allocation and clean up.
 */

static int ipw_stop_master(struct ipw_priv *priv)
{
	int rc;

	IPW_DEBUG_TRACE(">> \n");
	/* stop master. typical delay - 0 */
	ipw_set_bit(priv, IPW_RESET_REG, IPW_RESET_REG_STOP_MASTER);

	/* timeout is in msec, polled in 10-msec quanta */
	rc = ipw_poll_bit(priv, IPW_RESET_REG,
			  IPW_RESET_REG_MASTER_DISABLED, 100);
	if (rc < 0) {
		IPW_ERROR("wait for stop master failed after 100ms\n");
		return -1;
	}

	IPW_DEBUG_INFO("stop master %dms\n", rc);

	return rc;
}

static void ipw_arc_release(struct ipw_priv *priv)
{
	IPW_DEBUG_TRACE(">> \n");
	mdelay(5);

	ipw_clear_bit(priv, IPW_RESET_REG, CBD_RESET_REG_PRINCETON_RESET);

	/* no one knows timing, for safety add some delay */
	mdelay(5);
}

struct fw_chunk {
	__le32 address;
	__le32 length;
};

static int ipw_load_ucode(struct ipw_priv *priv, u8 * data, size_t len)
{
	int rc = 0, i, addr;
	u8 cr = 0;
	__le16 *image;

	image = (__le16 *) data;

	IPW_DEBUG_TRACE(">> \n");

	rc = ipw_stop_master(priv);

	if (rc < 0)
		return rc;

	for (addr = IPW_SHARED_LOWER_BOUND;
	     addr < IPW_REGISTER_DOMAIN1_END; addr += 4) {
		ipw_write32(priv, addr, 0);
	}

	/* no ucode (yet) */
	memset(&priv->dino_alive, 0, sizeof(priv->dino_alive));
	/* destroy DMA queues */
	/* reset sequence */

	ipw_write_reg32(priv, IPW_MEM_HALT_AND_RESET, IPW_BIT_HALT_RESET_ON);
	ipw_arc_release(priv);
	ipw_write_reg32(priv, IPW_MEM_HALT_AND_RESET, IPW_BIT_HALT_RESET_OFF);
	mdelay(1);

	/* reset PHY */
	ipw_write_reg32(priv, IPW_INTERNAL_CMD_EVENT, IPW_BASEBAND_POWER_DOWN);
	mdelay(1);

	ipw_write_reg32(priv, IPW_INTERNAL_CMD_EVENT, 0);
	mdelay(1);

	/* enable ucode store */
	ipw_write_reg8(priv, IPW_BASEBAND_CONTROL_STATUS, 0x0);
	ipw_write_reg8(priv, IPW_BASEBAND_CONTROL_STATUS, DINO_ENABLE_CS);
	mdelay(1);

	/* write ucode */
	/**
	 * @bug
	 * Do NOT set indirect address register once and then
	 * store data to indirect data register in the loop.
	 * It seems very reasonable, but in this case DINO do not
	 * accept ucode. It is essential to set address each time.
	 */
	/* load new ipw uCode */
	for (i = 0; i < len / 2; i++)
		ipw_write_reg16(priv, IPW_BASEBAND_CONTROL_STORE,
				le16_to_cpu(image[i]));

	/* enable DINO */
	ipw_write_reg8(priv, IPW_BASEBAND_CONTROL_STATUS, 0);
	ipw_write_reg8(priv, IPW_BASEBAND_CONTROL_STATUS, DINO_ENABLE_SYSTEM);

	/* this is where the igx / win driver deveates from the VAP driver. */

	/* wait for alive response */
	for (i = 0; i < 100; i++) {
		/* poll for incoming data */
		cr = ipw_read_reg8(priv, IPW_BASEBAND_CONTROL_STATUS);
		if (cr & DINO_RXFIFO_DATA)
			break;
		mdelay(1);
	}

	if (cr & DINO_RXFIFO_DATA) {
		/* alive_command_responce size is NOT multiple of 4 */
		__le32 response_buffer[(sizeof(priv->dino_alive) + 3) / 4];

		for (i = 0; i < ARRAY_SIZE(response_buffer); i++)
			response_buffer[i] =
			    cpu_to_le32(ipw_read_reg32(priv,
						       IPW_BASEBAND_RX_FIFO_READ));
		memcpy(&priv->dino_alive, response_buffer,
		       sizeof(priv->dino_alive));
		if (priv->dino_alive.alive_command == 1
		    && priv->dino_alive.ucode_valid == 1) {
			rc = 0;
			IPW_DEBUG_INFO
			    ("Microcode OK, rev. %d (0x%x) dev. %d (0x%x) "
			     "of %02d/%02d/%02d %02d:%02d\n",
			     priv->dino_alive.software_revision,
			     priv->dino_alive.software_revision,
			     priv->dino_alive.device_identifier,
			     priv->dino_alive.device_identifier,
			     priv->dino_alive.time_stamp[0],
			     priv->dino_alive.time_stamp[1],
			     priv->dino_alive.time_stamp[2],
			     priv->dino_alive.time_stamp[3],
			     priv->dino_alive.time_stamp[4]);
		} else {
			IPW_DEBUG_INFO("Microcode is not alive\n");
			rc = -EINVAL;
		}
	} else {
		IPW_DEBUG_INFO("No alive response from DINO\n");
		rc = -ETIME;
	}

	/* disable DINO, otherwise for some reason
	   firmware have problem getting alive resp. */
	ipw_write_reg8(priv, IPW_BASEBAND_CONTROL_STATUS, 0);

	return rc;
}

static int ipw_load_firmware(struct ipw_priv *priv, u8 * data, size_t len)
{
	int ret = -1;
	int offset = 0;
	struct fw_chunk *chunk;
	int total_nr = 0;
	int i;
	struct pci_pool *pool;
	u32 *virts[CB_NUMBER_OF_ELEMENTS_SMALL];
	dma_addr_t phys[CB_NUMBER_OF_ELEMENTS_SMALL];

	IPW_DEBUG_TRACE("<< : \n");

	pool = pci_pool_create("ipw2200", priv->pci_dev, CB_MAX_LENGTH, 0, 0);
	if (!pool) {
		IPW_ERROR("pci_pool_create failed\n");
		return -ENOMEM;
	}

	/* Start the Dma */
	ret = ipw_fw_dma_enable(priv);

	/* the DMA is already ready this would be a bug. */
	BUG_ON(priv->sram_desc.last_cb_index > 0);

	do {
		u32 chunk_len;
		u8 *start;
		int size;
		int nr = 0;

		chunk = (struct fw_chunk *)(data + offset);
		offset += sizeof(struct fw_chunk);
		chunk_len = le32_to_cpu(chunk->length);
		start = data + offset;

		nr = (chunk_len + CB_MAX_LENGTH - 1) / CB_MAX_LENGTH;
		for (i = 0; i < nr; i++) {
			virts[total_nr] = pci_pool_alloc(pool, GFP_KERNEL,
							 &phys[total_nr]);
			if (!virts[total_nr]) {
				ret = -ENOMEM;
				goto out;
			}
			size = min_t(u32, chunk_len - i * CB_MAX_LENGTH,
				     CB_MAX_LENGTH);
			memcpy(virts[total_nr], start, size);
			start += size;
			total_nr++;
			/* We don't support fw chunk larger than 64*8K */
			BUG_ON(total_nr > CB_NUMBER_OF_ELEMENTS_SMALL);
		}

		/* build DMA packet and queue up for sending */
		/* dma to chunk->address, the chunk->length bytes from data +
		 * offeset*/
		/* Dma loading */
		ret = ipw_fw_dma_add_buffer(priv, &phys[total_nr - nr],
					    nr, le32_to_cpu(chunk->address),
					    chunk_len);
		if (ret) {
			IPW_DEBUG_INFO("dmaAddBuffer Failed\n");
			goto out;
		}

		offset += chunk_len;
	} while (offset < len);

	/* Run the DMA and wait for the answer */
	ret = ipw_fw_dma_kick(priv);
	if (ret) {
		IPW_ERROR("dmaKick Failed\n");
		goto out;
	}

	ret = ipw_fw_dma_wait(priv);
	if (ret) {
		IPW_ERROR("dmaWaitSync Failed\n");
		goto out;
	}
 out:
	for (i = 0; i < total_nr; i++)
		pci_pool_free(pool, virts[i], phys[i]);

	pci_pool_destroy(pool);

	return ret;
}

/* stop nic */
static int ipw_stop_nic(struct ipw_priv *priv)
{
	int rc = 0;

	/* stop */
	ipw_write32(priv, IPW_RESET_REG, IPW_RESET_REG_STOP_MASTER);

	rc = ipw_poll_bit(priv, IPW_RESET_REG,
			  IPW_RESET_REG_MASTER_DISABLED, 500);
	if (rc < 0) {
		IPW_ERROR("wait for reg master disabled failed after 500ms\n");
		return rc;
	}

	ipw_set_bit(priv, IPW_RESET_REG, CBD_RESET_REG_PRINCETON_RESET);

	return rc;
}

static void ipw_start_nic(struct ipw_priv *priv)
{
	IPW_DEBUG_TRACE(">>\n");

	/* prvHwStartNic  release ARC */
	ipw_clear_bit(priv, IPW_RESET_REG,
		      IPW_RESET_REG_MASTER_DISABLED |
		      IPW_RESET_REG_STOP_MASTER |
		      CBD_RESET_REG_PRINCETON_RESET);

	/* enable power management */
	ipw_set_bit(priv, IPW_GP_CNTRL_RW,
		    IPW_GP_CNTRL_BIT_HOST_ALLOWS_STANDBY);

	IPW_DEBUG_TRACE("<<\n");
}

static int ipw_init_nic(struct ipw_priv *priv)
{
	int rc;

	IPW_DEBUG_TRACE(">>\n");
	/* reset */
	/*prvHwInitNic */
	/* set "initialization complete" bit to move adapter to D0 state */
	ipw_set_bit(priv, IPW_GP_CNTRL_RW, IPW_GP_CNTRL_BIT_INIT_DONE);

	/* low-level PLL activation */
	ipw_write32(priv, IPW_READ_INT_REGISTER,
		    IPW_BIT_INT_HOST_SRAM_READ_INT_REGISTER);

	/* wait for clock stabilization */
	rc = ipw_poll_bit(priv, IPW_GP_CNTRL_RW,
			  IPW_GP_CNTRL_BIT_CLOCK_READY, 250);
	if (rc < 0)
		IPW_DEBUG_INFO("FAILED wait for clock stablization\n");

	/* assert SW reset */
	ipw_set_bit(priv, IPW_RESET_REG, IPW_RESET_REG_SW_RESET);

	udelay(10);

	/* set "initialization complete" bit to move adapter to D0 state */
	ipw_set_bit(priv, IPW_GP_CNTRL_RW, IPW_GP_CNTRL_BIT_INIT_DONE);

	IPW_DEBUG_TRACE(">>\n");
	return 0;
}

/* Call this function from process context, it will sleep in request_firmware.
 * Probe is an ok place to call this from.
 */
static int ipw_reset_nic(struct ipw_priv *priv)
{
	int rc = 0;
	unsigned long flags;

	IPW_DEBUG_TRACE(">>\n");

	rc = ipw_init_nic(priv);

	spin_lock_irqsave(&priv->lock, flags);
	/* Clear the 'host command active' bit... */
	priv->status &= ~STATUS_HCMD_ACTIVE;
	wake_up_interruptible(&priv->wait_command_queue);
	priv->status &= ~(STATUS_SCANNING | STATUS_SCAN_ABORTING);
	wake_up_interruptible(&priv->wait_state);
	spin_unlock_irqrestore(&priv->lock, flags);

	IPW_DEBUG_TRACE("<<\n");
	return rc;
}


struct ipw_fw {
	__le32 ver;
	__le32 boot_size;
	__le32 ucode_size;
	__le32 fw_size;
	u8 data[0];
};

static int ipw_get_fw(struct ipw_priv *priv,
		      const struct firmware **raw, const char *name)
{
	struct ipw_fw *fw;
	int rc;

	/* ask firmware_class module to get the boot firmware off disk */
	rc = request_firmware(raw, name, &priv->pci_dev->dev);
	if (rc < 0) {
		IPW_ERROR("%s request_firmware failed: Reason %d\n", name, rc);
		return rc;
	}

	if ((*raw)->size < sizeof(*fw)) {
		IPW_ERROR("%s is too small (%zd)\n", name, (*raw)->size);
		return -EINVAL;
	}

	fw = (void *)(*raw)->data;

	if ((*raw)->size < sizeof(*fw) + le32_to_cpu(fw->boot_size) +
	    le32_to_cpu(fw->ucode_size) + le32_to_cpu(fw->fw_size)) {
		IPW_ERROR("%s is too small or corrupt (%zd)\n",
			  name, (*raw)->size);
		return -EINVAL;
	}

	IPW_DEBUG_INFO("Read firmware '%s' image v%d.%d (%zd bytes)\n",
		       name,
		       le32_to_cpu(fw->ver) >> 16,
		       le32_to_cpu(fw->ver) & 0xff,
		       (*raw)->size - sizeof(*fw));
	return 0;
}

#define IPW_RX_BUF_SIZE (3000)

static void ipw_rx_queue_reset(struct ipw_priv *priv,
				      struct ipw_rx_queue *rxq)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&rxq->lock, flags);

	INIT_LIST_HEAD(&rxq->rx_free);
	INIT_LIST_HEAD(&rxq->rx_used);

	/* Fill the rx_used queue with _all_ of the Rx buffers */
	for (i = 0; i < RX_FREE_BUFFERS + RX_QUEUE_SIZE; i++) {
		/* In the reset function, these buffers may have been allocated
		 * to an SKB, so we need to unmap and free potential storage */
		if (rxq->pool[i].skb != NULL) {
			pci_unmap_single(priv->pci_dev, rxq->pool[i].dma_addr,
					 IPW_RX_BUF_SIZE, PCI_DMA_FROMDEVICE);
			dev_kfree_skb(rxq->pool[i].skb);
			rxq->pool[i].skb = NULL;
		}
		list_add_tail(&rxq->pool[i].list, &rxq->rx_used);
	}

	/* Set us so that we have processed and used all buffers, but have
	 * not restocked the Rx queue with fresh buffers */
	rxq->read = rxq->write = 0;
	rxq->free_count = 0;
	spin_unlock_irqrestore(&rxq->lock, flags);
}

#ifdef CONFIG_PM
static int fw_loaded = 0;
static const struct firmware *raw = NULL;

static void free_firmware(void)
{
	if (fw_loaded) {
		release_firmware(raw);
		raw = NULL;
		fw_loaded = 0;
	}
}
#else
#define free_firmware() do {} while (0)
#endif

static int ipw_load(struct ipw_priv *priv)
{
#ifndef CONFIG_PM
	const struct firmware *raw = NULL;
#endif
	struct ipw_fw *fw;
	u8 *boot_img, *ucode_img, *fw_img;
	u8 *name = NULL;
	int rc = 0, retries = 3;

	switch (priv->ieee->iw_mode) {
	case IW_MODE_ADHOC:
		name = "ipw2200-ibss.fw";
		break;
#ifdef CONFIG_IPW2200_MONITOR
	case IW_MODE_MONITOR:
		name = "ipw2200-sniffer.fw";
		break;
#endif
	case IW_MODE_INFRA:
		name = "ipw2200-bss.fw";
		break;
	}

	if (!name) {
		rc = -EINVAL;
		goto error;
	}

#ifdef CONFIG_PM
	if (!fw_loaded) {
#endif
		rc = ipw_get_fw(priv, &raw, name);
		if (rc < 0)
			goto error;
#ifdef CONFIG_PM
	}
#endif

	fw = (void *)raw->data;
	boot_img = &fw->data[0];
	ucode_img = &fw->data[le32_to_cpu(fw->boot_size)];
	fw_img = &fw->data[le32_to_cpu(fw->boot_size) +
			   le32_to_cpu(fw->ucode_size)];

	if (rc < 0)
		goto error;

	if (!priv->rxq)
		priv->rxq = ipw_rx_queue_alloc(priv);
	else
		ipw_rx_queue_reset(priv, priv->rxq);
	if (!priv->rxq) {
		IPW_ERROR("Unable to initialize Rx queue\n");
		goto error;
	}

      retry:
	/* Ensure interrupts are disabled */
	ipw_write32(priv, IPW_INTA_MASK_R, ~IPW_INTA_MASK_ALL);
	priv->status &= ~STATUS_INT_ENABLED;

	/* ack pending interrupts */
	ipw_write32(priv, IPW_INTA_RW, IPW_INTA_MASK_ALL);

	ipw_stop_nic(priv);

	rc = ipw_reset_nic(priv);
	if (rc < 0) {
		IPW_ERROR("Unable to reset NIC\n");
		goto error;
	}

	ipw_zero_memory(priv, IPW_NIC_SRAM_LOWER_BOUND,
			IPW_NIC_SRAM_UPPER_BOUND - IPW_NIC_SRAM_LOWER_BOUND);

	/* DMA the initial boot firmware into the device */
	rc = ipw_load_firmware(priv, boot_img, le32_to_cpu(fw->boot_size));
	if (rc < 0) {
		IPW_ERROR("Unable to load boot firmware: %d\n", rc);
		goto error;
	}

	/* kick start the device */
	ipw_start_nic(priv);

	/* wait for the device to finish its initial startup sequence */
	rc = ipw_poll_bit(priv, IPW_INTA_RW,
			  IPW_INTA_BIT_FW_INITIALIZATION_DONE, 500);
	if (rc < 0) {
		IPW_ERROR("device failed to boot initial fw image\n");
		goto error;
	}
	IPW_DEBUG_INFO("initial device response after %dms\n", rc);

	/* ack fw init done interrupt */
	ipw_write32(priv, IPW_INTA_RW, IPW_INTA_BIT_FW_INITIALIZATION_DONE);

	/* DMA the ucode into the device */
	rc = ipw_load_ucode(priv, ucode_img, le32_to_cpu(fw->ucode_size));
	if (rc < 0) {
		IPW_ERROR("Unable to load ucode: %d\n", rc);
		goto error;
	}

	/* stop nic */
	ipw_stop_nic(priv);

	/* DMA bss firmware into the device */
	rc = ipw_load_firmware(priv, fw_img, le32_to_cpu(fw->fw_size));
	if (rc < 0) {
		IPW_ERROR("Unable to load firmware: %d\n", rc);
		goto error;
	}
#ifdef CONFIG_PM
	fw_loaded = 1;
#endif

	ipw_write32(priv, IPW_EEPROM_LOAD_DISABLE, 0);

	rc = ipw_queue_reset(priv);
	if (rc < 0) {
		IPW_ERROR("Unable to initialize queues\n");
		goto error;
	}

	/* Ensure interrupts are disabled */
	ipw_write32(priv, IPW_INTA_MASK_R, ~IPW_INTA_MASK_ALL);
	/* ack pending interrupts */
	ipw_write32(priv, IPW_INTA_RW, IPW_INTA_MASK_ALL);

	/* kick start the device */
	ipw_start_nic(priv);

	if (ipw_read32(priv, IPW_INTA_RW) & IPW_INTA_BIT_PARITY_ERROR) {
		if (retries > 0) {
			IPW_WARNING("Parity error.  Retrying init.\n");
			retries--;
			goto retry;
		}

		IPW_ERROR("TODO: Handle parity error -- schedule restart?\n");
		rc = -EIO;
		goto error;
	}

	/* wait for the device */
	rc = ipw_poll_bit(priv, IPW_INTA_RW,
			  IPW_INTA_BIT_FW_INITIALIZATION_DONE, 500);
	if (rc < 0) {
		IPW_ERROR("device failed to start within 500ms\n");
		goto error;
	}
	IPW_DEBUG_INFO("device response after %dms\n", rc);

	/* ack fw init done interrupt */
	ipw_write32(priv, IPW_INTA_RW, IPW_INTA_BIT_FW_INITIALIZATION_DONE);

	/* read eeprom data and initialize the eeprom region of sram */
	priv->eeprom_delay = 1;
	ipw_eeprom_init_sram(priv);

	/* enable interrupts */
	ipw_enable_interrupts(priv);

	/* Ensure our queue has valid packets */
	ipw_rx_queue_replenish(priv);

	ipw_write32(priv, IPW_RX_READ_INDEX, priv->rxq->read);

	/* ack pending interrupts */
	ipw_write32(priv, IPW_INTA_RW, IPW_INTA_MASK_ALL);

#ifndef CONFIG_PM
	release_firmware(raw);
#endif
	return 0;

      error:
	if (priv->rxq) {
		ipw_rx_queue_free(priv, priv->rxq);
		priv->rxq = NULL;
	}
	ipw_tx_queue_free(priv);
	if (raw)
		release_firmware(raw);
#ifdef CONFIG_PM
	fw_loaded = 0;
	raw = NULL;
#endif

	return rc;
}

/**
 * DMA services
 *
 * Theory of operation
 *
 * A queue is a circular buffers with 'Read' and 'Write' pointers.
 * 2 empty entries always kept in the buffer to protect from overflow.
 *
 * For Tx queue, there are low mark and high mark limits. If, after queuing
 * the packet for Tx, free space become < low mark, Tx queue stopped. When
 * reclaiming packets (on 'tx done IRQ), if free space become > high mark,
 * Tx queue resumed.
 *
 * The IPW operates with six queues, one receive queue in the device's
 * sram, one transmit queue for sending commands to the device firmware,
 * and four transmit queues for data.
 *
 * The four transmit queues allow for performing quality of service (qos)
 * transmissions as per the 802.11 protocol.  Currently Linux does not
 * provide a mechanism to the user for utilizing prioritized queues, so
 * we only utilize the first data transmit queue (queue1).
 */

/**
 * Driver allocates buffers of this size for Rx
 */

/**
 * ipw_rx_queue_space - Return number of free slots available in queue.
 */
static int ipw_rx_queue_space(const struct ipw_rx_queue *q)
{
	int s = q->read - q->write;
	if (s <= 0)
		s += RX_QUEUE_SIZE;
	/* keep some buffer to not confuse full and empty queue */
	s -= 2;
	if (s < 0)
		s = 0;
	return s;
}

static inline int ipw_tx_queue_space(const struct clx2_queue *q)
{
	int s = q->last_used - q->first_empty;
	if (s <= 0)
		s += q->n_bd;
	s -= 2;			/* keep some reserve to not confuse empty and full situations */
	if (s < 0)
		s = 0;
	return s;
}

static inline int ipw_queue_inc_wrap(int index, int n_bd)
{
	return (++index == n_bd) ? 0 : index;
}

/**
 * Initialize common DMA queue structure
 *
 * @param q                queue to init
 * @param count            Number of BD's to allocate. Should be power of 2
 * @param read_register    Address for 'read' register
 *                         (not offset within BAR, full address)
 * @param write_register   Address for 'write' register
 *                         (not offset within BAR, full address)
 * @param base_register    Address for 'base' register
 *                         (not offset within BAR, full address)
 * @param size             Address for 'size' register
 *                         (not offset within BAR, full address)
 */
static void ipw_queue_init(struct ipw_priv *priv, struct clx2_queue *q,
			   int count, u32 read, u32 write, u32 base, u32 size)
{
	q->n_bd = count;

	q->low_mark = q->n_bd / 4;
	if (q->low_mark < 4)
		q->low_mark = 4;

	q->high_mark = q->n_bd / 8;
	if (q->high_mark < 2)
		q->high_mark = 2;

	q->first_empty = q->last_used = 0;
	q->reg_r = read;
	q->reg_w = write;

	ipw_write32(priv, base, q->dma_addr);
	ipw_write32(priv, size, count);
	ipw_write32(priv, read, 0);
	ipw_write32(priv, write, 0);

	_ipw_read32(priv, 0x90);
}

static int ipw_queue_tx_init(struct ipw_priv *priv,
			     struct clx2_tx_queue *q,
			     int count, u32 read, u32 write, u32 base, u32 size)
{
	struct pci_dev *dev = priv->pci_dev;

	q->txb = kmalloc(sizeof(q->txb[0]) * count, GFP_KERNEL);
	if (!q->txb) {
		IPW_ERROR("vmalloc for auxilary BD structures failed\n");
		return -ENOMEM;
	}

	q->bd =
	    pci_alloc_consistent(dev, sizeof(q->bd[0]) * count, &q->q.dma_addr);
	if (!q->bd) {
		IPW_ERROR("pci_alloc_consistent(%zd) failed\n",
			  sizeof(q->bd[0]) * count);
		kfree(q->txb);
		q->txb = NULL;
		return -ENOMEM;
	}

	ipw_queue_init(priv, &q->q, count, read, write, base, size);
	return 0;
}

/**
 * Free one TFD, those at index [txq->q.last_used].
 * Do NOT advance any indexes
 *
 * @param dev
 * @param txq
 */
static void ipw_queue_tx_free_tfd(struct ipw_priv *priv,
				  struct clx2_tx_queue *txq)
{
	struct tfd_frame *bd = &txq->bd[txq->q.last_used];
	struct pci_dev *dev = priv->pci_dev;
	int i;

	/* classify bd */
	if (bd->control_flags.message_type == TX_HOST_COMMAND_TYPE)
		/* nothing to cleanup after for host commands */
		return;

	/* sanity check */
	if (le32_to_cpu(bd->u.data.num_chunks) > NUM_TFD_CHUNKS) {
		IPW_ERROR("Too many chunks: %i\n",
			  le32_to_cpu(bd->u.data.num_chunks));
		/** @todo issue fatal error, it is quite serious situation */
		return;
	}

	/* unmap chunks if any */
	for (i = 0; i < le32_to_cpu(bd->u.data.num_chunks); i++) {
		pci_unmap_single(dev, le32_to_cpu(bd->u.data.chunk_ptr[i]),
				 le16_to_cpu(bd->u.data.chunk_len[i]),
				 PCI_DMA_TODEVICE);
		if (txq->txb[txq->q.last_used]) {
			libipw_txb_free(txq->txb[txq->q.last_used]);
			txq->txb[txq->q.last_used] = NULL;
		}
	}
}

/**
 * Deallocate DMA queue.
 *
 * Empty queue by removing and destroying all BD's.
 * Free all buffers.
 *
 * @param dev
 * @param q
 */
static void ipw_queue_tx_free(struct ipw_priv *priv, struct clx2_tx_queue *txq)
{
	struct clx2_queue *q = &txq->q;
	struct pci_dev *dev = priv->pci_dev;

	if (q->n_bd == 0)
		return;

	/* first, empty all BD's */
	for (; q->first_empty != q->last_used;
	     q->last_used = ipw_queue_inc_wrap(q->last_used, q->n_bd)) {
		ipw_queue_tx_free_tfd(priv, txq);
	}

	/* free buffers belonging to queue itself */
	pci_free_consistent(dev, sizeof(txq->bd[0]) * q->n_bd, txq->bd,
			    q->dma_addr);
	kfree(txq->txb);

	/* 0 fill whole structure */
	memset(txq, 0, sizeof(*txq));
}

/**
 * Destroy all DMA queues and structures
 *
 * @param priv
 */
static void ipw_tx_queue_free(struct ipw_priv *priv)
{
	/* Tx CMD queue */
	ipw_queue_tx_free(priv, &priv->txq_cmd);

	/* Tx queues */
	ipw_queue_tx_free(priv, &priv->txq[0]);
	ipw_queue_tx_free(priv, &priv->txq[1]);
	ipw_queue_tx_free(priv, &priv->txq[2]);
	ipw_queue_tx_free(priv, &priv->txq[3]);
}

static void ipw_create_bssid(struct ipw_priv *priv, u8 * bssid)
{
	/* First 3 bytes are manufacturer */
	bssid[0] = priv->mac_addr[0];
	bssid[1] = priv->mac_addr[1];
	bssid[2] = priv->mac_addr[2];

	/* Last bytes are random */
	get_random_bytes(&bssid[3], ETH_ALEN - 3);

	bssid[0] &= 0xfe;	/* clear multicast bit */
	bssid[0] |= 0x02;	/* set local assignment bit (IEEE802) */
}

static u8 ipw_add_station(struct ipw_priv *priv, u8 * bssid)
{
	struct ipw_station_entry entry;
	int i;

	for (i = 0; i < priv->num_stations; i++) {
		if (!memcmp(priv->stations[i], bssid, ETH_ALEN)) {
			/* Another node is active in network */
			priv->missed_adhoc_beacons = 0;
			if (!(priv->config & CFG_STATIC_CHANNEL))
				/* when other nodes drop out, we drop out */
				priv->config &= ~CFG_ADHOC_PERSIST;

			return i;
		}
	}

	if (i == MAX_STATIONS)
		return IPW_INVALID_STATION;

	IPW_DEBUG_SCAN("Adding AdHoc station: %pM\n", bssid);

	entry.reserved = 0;
	entry.support_mode = 0;
	memcpy(entry.mac_addr, bssid, ETH_ALEN);
	memcpy(priv->stations[i], bssid, ETH_ALEN);
	ipw_write_direct(priv, IPW_STATION_TABLE_LOWER + i * sizeof(entry),
			 &entry, sizeof(entry));
	priv->num_stations++;

	return i;
}

static u8 ipw_find_station(struct ipw_priv *priv, u8 * bssid)
{
	int i;

	for (i = 0; i < priv->num_stations; i++)
		if (!memcmp(priv->stations[i], bssid, ETH_ALEN))
			return i;

	return IPW_INVALID_STATION;
}

static void ipw_send_disassociate(struct*****priv *****, int quiet)
{
	****err;

	if (****->status & STATUS_ASSOCIATING) {
		IPW_DEBUG- 2006("D**********ing while eserved.

 .\n");
		queue_work

  Copis f of t, &
  Cop************rtionreturn;
	}******
!

  Copyright(c) 2003 - 2006 InED) Corporation. All rights reserved.

  802.11not1 status ede portion00, Axis Commration. All rights reservation attempt from %pM "
			"on channel %    C,n re
  Cop*****_request.bssidor modify it
  under the ribute )****
  Copyright(c= ~( 2003 - 2006 Intel |hereal - Network tr;blic License as|=) 2003 -DIS 2006 Intel*******
*******modify it
  under the t
  untype = HCributed i_QUIET;
	elseill be useful, but WITHOUT
  ANY WARRANTY; withork ****err =**************************0.10.6:t
  under then.

***
err Corporation. AlHC("A softwato **** [dis]********* commandcan re ral "fail
    Copyright 1998 Ger}

yrigic****************************data************************** = e Sonse forations AB
    Ethublished by the EDe Software Foundattel )will00, Ax 0nse *********************e So, 0n.

netif_carrier_off

  Copnet_devn.

eneral 1;this progra*********bg*********************is f_********is fftware Foundation, Inc., 59

		containin th(is f, ***************, 
    Copyright 2mutex_lock(0.10.6:*****ense  not, write to t****97

******un********************ontact Information:system_configWireless <ilw@linux.intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro,hed.h>
#inclu Pub#ifdef CONFIG_IPW2200_PROMISCUOUS*****

  Copprom_ called && striburunning#ifdef CONFIG_IPW22raffic 
  Copyys
#inclu.accept_all_e So_frames =
  CM
#endif

#ifdef CONFIG_IPnon_directedOMISCUOUS
#define VP "p"
#else
#define W220mgmt_bcpRTICIG_IPW2200_RADIOTAP
#define VR "r"
#elMISCUOUS
#def}
#endif************hed.h>
#includ*******his p*********yright_codeCorpu16 yright;
0 N.sttribr *reason;
};is prografine D0_VERSION "1.2.2" VK VDON "1.2.2" VK Vs[] =Corp{0x00, "Successful"},COPYRI1, "Unspecified blicure(c) 2003-A, "Canthersupport all der theed capabilities in thecan V_VEefine Ey informis frefield(c) 2003-B, "Reeserved.
on denorpodue havinTH_P_802to PROfirm that_STATSDRV_DESCRIPTexists(c) 2003-C, "ARV_DESCRIPTION);
MODULE_VCRIPTI outside0211_scope of this_STATSstandarULE_DESCRID,TATSRespond

   progIPTIoesether     IPWc inttel Corpoauthenti is freE_AUTHlgorithm(c) 2003-Eel = 0;ceiv_debn Aug_level;
statMISCU withebug_level;
statser tnc1_STATStransactic int disablnumber 0;
ug =expif

#int disab(c) 2003-FICENug_level;
statrejt hwcrbecausbug =challengeoration"
#define1GHT	roaming = 1;
static consODULE_Vtime
stawai.

  for nexULE_AUT led_si int disab 'b', 'g'2006NSE("GPL");

statict char iAP is unablULE_Vhand.11 ddinetwalLE_AUTHOR(DRV_wcryt netw);
MODULE12el = NSE("GPL");

static int cmdlr theic int netwode = 0;

st

  2200STATSg = 0e  TemratH_P_80211_BSSBasicServiceSet PaISCUterONFIG_IPW3200_QOS
static int qos_enable = 0;
static int qos_burst_enable = tatic h IPWpreamdo noper netwONFIG_IPW4tatic int burst_duration_OFDM = 0;

static struct libipw_qos_parameterPBCC encotic ONFIG_IPW5tatic int burst_duration_OFDM = 0;

static struct libipw_qos_parameterribute iag_P_80ONFIG_IPW9tatic int burst_duration_OFDM = 0;

static struct libipw_qos_parameters def_slotmeters_OFDM = {
	{QAtatic int burst_duration_OFDM = 0;

static struct libipw_qos_parameterDSSS-OFDMmeters_OFDM = {
	{28, "Invalid I1_RAW + 1)
ElementOP_LIMIT_9, "Group Cipher: 0 ther};

sOP_LIMIT_DRV_Pairwise_qos_parameters_CCK = {
	{QOSPTIOAKMf: 0 ters_CCK = {
	{QOSLICEUn     IPed RSN IE vers_TXOP_LIMIT_DFDM}
};

stX0_CW_M (ETH_P_8ie);
MODULE2ERV_Vos_parsuite: 0 atic consp_CCKecur_802policOS_TX	"Intel(R) PRO/WiRV_DES#defget"1.2.2" VK V( VP VR VQ
*********i;
	

#i(i = 0; i < ARRAY_SIZE(k Driver"
#defin); i++will***
k Driver"
#definei].yright(== (yright(c)0xffGNU GCENSE.

CK, QOS_TX2_TXOP_LIMCRIPTIONCENSE.

"Unknown VR VQ
s_CCue."/

#include <linuxnline average_init******** DEF_TX *avg*****memset(avg, 0, sizeof(2_CW_IPW220#defFDM,DEPTH_RSSI 8DEF_TX0_CW_MAX_NOISE 16s progras16c inonleveal_ DEF_TX(_MAXprev_ DEF__MAXval, u8 depth*****eneral (( DEF_-1)*OFDM},
	 + TX0_)/ DEF_/

#include <linu DEF_TX1addMIN_OFDM, DEF_TX2_CW{DEF_TX0_*****avg->sum -= P_LIMentries[P_LIMpos];XOP_LIMIT_O+=TX0_FDM,
	 D_TX1_TXOP_LIMIT_++ DRVTXOP_L***
unlikely	 DEIMIT_,
	 AVG_ENTRIESraffic  stru_CW_define V struct li Publ}

#include _MAX DEF_TX1OFDM MIN_OFDM, DEF_TX2_CW_MIN_ Plac;

static stru_CW_raffic ***
 struct MIT_CCK}
};
P_LIMIT_O/W_MAX_po
#deGeneral PublComm_TX1_CW_MAX_CCK, Dpw_qos_para/

#include <linux/scresTX0_ACMs***************************u32 len =3_CW_MINu32 Public LicquaP_802EF_TXXOP_EF_TX1_CW_MU GeneraTX0_TXOmissed_beafinen.

  Thisexp},
	_rssACM}-6PublEF_TX2_TXOP_LInoIN_C= -85 + 'g',	{DEF
  Coplast__ackDEF_TX0os_oui[QOS__TXOP_LIMIT_CCLEN] = { 0x00, 0x5rx_packettatic int from_priotity_to_tx_queue[] = {
	IPW_TX_ration"tatic i
	/* Firmware managed, X2_AI only when NIC{QOS_TstaQOS_, so we havULE_
	 * nRAW lize o80211_currenrs_CCue */e
#def_TX0ordinal the GNratiORD_ 200_RX_ERR_CRCor mo0.10.6:_prioriterr, &len********ration(struct ipw_priv *priv);
TX_FAILURc inpw_send_qos_paIPW_TX_QUEUommand(stPW_TXDriver,
	IPW_TX_QUEUE_upporeach1 status con_burs*******TXOP_Ladhoc2 };

static int from0, 0xF2 };

static int fromTX_QUEUE_1, IPW_TX_QUEUrity_to_tx_queue

#include  DEFuct ipw_max_OUI_F_TX3_AIFS},
	{DEF_TX0_ACM, DEFACM},x80tic vo;M, DEFmask = 00_QOS ack__emov;PW_TXIf32 ipw_qlyreal.com>
 2200B modeX_QUEtrictatic maximum_QUEUOUI_LmatchV_VEB
stats_burst**

  Copt
  under the ieee_ipw_,
	 ratiB_MODEwillemove&= LIBratiCCK_RATES_MASKint ipwTODO: Verify
MODULt qoOUI_Lis= 0;

st0;  t(stu32 ipw_qo_netw_QUEUlist._bur
	802.11(i00_M!(ue *tx iGNU Gi >>efine swiint (i Corpcase, int qindex);
s_1MBatic :_MAX_CCK,
1ic void i int sync);

static v2id ipw_tx_queue_fr2e(struct ipw_priv *);

static 5id ipw_tx_queue_fr55(struct ipw_priv *);DM, atic v6id ipw_tx_queue_fr6e(struct ipw_priv *);truct ipw_9id ipw_tx_queue_fr9e(struct ipw_priv *);

static 1oid ipw_tx_queue_freee(struct ipw_priv *);truct ipw_1struct ipw_rx_queue1 *ipw_rx_queue_alloc(sork_struct 8id ipw_tx_queue_freatic von(struct ipw_priv *);
sta24truct ipw_rx_queue 4truct work_struct *work);
stati3priv *, struct ipw_3rx_queue *);
static void ipw_rx_4ic void ipw_bg_down4struct work_struct *work);
stati5 int ipw_config(str5ct ipw_priCommunic
  Copriv,->				strucEEE_BNU General void ipw_bgthe impt ipw_priv *);
stt iw_statistics *ipw_g2 ipw_qwireless_stats(struct net_device *de_net,F_TX1_ACM, DEF_netense *************
ations AB
    Ethereal - Network tra_MAX_CCK,
	 Dreclaim(struTX_QUEUE_1,>uct cREALatic vRX_PACKET_THRESHOLD CorpoPARTICULARipw_priv *priv, struct libipw_qos_CURRx);
s, &t outrs
	eral Pmmand(stre for
  more dald Combs

INFO(ublic L query

  on(struse portionGeneral Publ	}
	} tic int snprins *ipw_get_wirele*******			     inar c;len, int ratiTXatic voidtx_queue_free(struct ipw_p - out, " ")2
	for (l = 0, *ipw_rx_queue_ - out, " ")5
	for (l = 0,id ipw_rx_queue - out, " ")6
	for (l = 0,rx_queue *);
st - out, " ")9
	for (l = 0,d *);
static in - out, " ");;
	for (l = 0,  i = 0; i < 2; i++) {
		out *wostatic void ipw_down(struct - out, " ");8, c);
		}

		fstruct work_stri++) {
		out 4+= snprintf(buct ipw_priv *); - out, " ")supp_rates(struct ipw_priv *pri - out, " ")_supd_rates *prates);
static vo - out, " ");	return out;
}riv *);
static eneral Pub	{DEF_TX0_Crati 200S_INTERVAL (2 * HZ)tact Information:gatherFS, DEF_TX3_AIFS},
	{DEF_TX0_ACM, DEFrams_comrams_c_deln thrity_to_txofs +=d ipw_ros_param);
stIPW_TX_QUEUofs += 1TX_QUEUE_1= min(len, 16_TXOP_LIMIT_CC_percent,size, const u8 * min(len, 16TX3_ACM},
	{D, DEF_TX1_ACM, DEF_TX2_A	_MAXMIT_fs = 0;IMIT_C_TX3_ACMX3_Cgnal		out = sntx		out = snr, size, &d
eral _net		out = d ipw_rem_wirelnprintf(buf, count, "%08X", ofs);

	for (l =e VM
#endifize;
	u32 ofs ight 1998 Gera/* UpdUI_Lc intprogstic_tx_rec0; j < 8 && l < len; j++, l++)
			MISSED_BEACONSers
				    _TXOP_LIMIT_CCommand(strze_t len)
{
	size_t e_current0, 0xF2 };

sta-current, 0x50, 0xF2 };

st = { 0x00, 0x50, 0xF2 };

statid_reg32(struct ipw_pOFDM}
}dify it
  under the tn) {
	inter_TX0Corpoize, const u8 * data,  =ize_t len)
{
	size_t  *
ut, co(HZhcmde16_to_cpu 8-bit indirect read (for SRAM/reg ab) /eg8(strune(line, sizeof(lin* 1is di out, ove 4K), with debug wrapper */
statiF_TX0_TXO_TX2IT_CCK, DEF_TX1_TXOP_LIMIT_CCsize_t len)
{
	si data, = snpt_duration(struct ipw_priv *priv);

static int &rams_command(stre);
		ofs += =ne);
		riv *priv, u32rams_c int from_prioritPARTIC"%s %d: truct ipw_priv *priv, struct libipw_qos_paramete &os_param);
static in	tic int snprintk_ =atic int snpriv *priv, u32IPW_TX_QUEUW_TX_QUEUE_2, IPW_TX_QUEUE_2 SRAM/reg aboriv *8 * output, se_current_ity_to_tx_IPW_DEBUG_IO("%sy_to_tx int from_priority_to_tx_qu00_QOS */

stativoidf(u8 * output, sw_write_rTX_QUEUE_1,ct write (for SR6 value);
static inliTX_QUEUE_1, I16 c)
{
	IPW_DEBint ipwCalculUI_LTX3_ACM}basedtatic u3following:_QUE_QUEUMTXOP_len) {
:ree(%), w,  indi70%size, c_QUEURate: 6 indi1Mbs,bit indiMaxAM/regx  GNUTx****orOS_TpX2_Ant airelaight %ug = otal Rx/Tpper */SSI-bit indi> -50, ct wri< -80per */
soid _i-bit indirect wri5e (for SRAM/r_QUEUThe lowe_AIFompu

stTX3_ACM} 0 -sed.32 b, u3/DEF_TX0_C(for S out, " ") 5
	en) {
		out = K), 00 -size, const u8 * data, OFDM}
}
		     __LINE_<0x%08X)\n", __FIL= 0,
		     __LINE__,
static int
		     __LINE__,e_reg32(a, b, c)-
}

/* 8-bit direc-bit 0) _ipw_rea (u32)}

/* 8-bit direc;rald Combs

ine, (" c);
}

/* 32-b%3d%% (%d%%)nd/or moen) {
		out = snstruct ipw_priv *priv, u32 os_oui[QOS_OUI_LEN count,
			const u8 **********U), ofs ofs, val) );
	}

	out += s	in_t(size_t,", __FILE[QOS_OUI_L* 40 /16U), ofs + 
	 DEbase + ofs);
}

/g ab("%s %d: t write (foMbslow 4K ofin_t(size_t,,, \
			__LINE__, /ree(stru u32 rf ( _ipw_write_reg16>g ofs&&16;
		len -= min( +ne);
		ofs +== 0, , size, &__, (u32)d _ia, u32 b, ng ofb) _ipw_reariv *ipw, unsigned long ofs,
		ustatic int al)
{
	writew(vl)); \
	_ipw_write8(ix("%s %d:  l); \
} whiu_write_, %u y_to_tx0)

/* 16-, size, &data[
		ofs += 16;
		len -= min(line voidw_priv *a, u32 b,uct ipw_pw_priv *a, u32 b,+atic int snprintk_= 0,t, size, &itew(val, g8(a, b, c);
}

/*+ ofs);
}

/* 16)(val)); \
	_ipw_write16(ipw, ofs, vastatic int \
} while (0)

l)); \
	_ipw_write8(Twrite16(ipw, ofs, val) do { \
	IPW_DEBUG_IO("%s %d:t, size, &datic int snprintk_buf(u8 * output, s= snpMIT_CCKEF_TX2_TXOP_LIMIT_
	whrint_line(out 520u8 val)
{b, u(stru ipw_send_weperfectIMIT_Civ *privend_weworIO("ssi)BUG_IO("%s %d: write_direct32(0x%08X, 0x%08X)\n", __FILE__, \-G_IO("%s %d: write_direct32(0x%08X,E__, \
			__LIN15 *%s %d: write_direct32(0x%08X, 0x%08X)\n", __FILE__, \+G_IO("% 6), &); \
} while (0)

/* 8-bit direct b) _i(strus %d: write_direct32(0x%08X, 0x%08X)\n", __FILE__, \
			__LINE__, (u32)(ofs), (u32)(val)); \
	_ipw_write32(ipw,ense for, ofs, val) do uct i= 0,, ofs, val) do { long oct wrDEBUG_IO("%s %d: re< 1rect8(0x%08X)\n", __F i < base + ofs);
}

/Srint_ levell); \
} whil dBmlow 4K ofprint_line(outpuE__, voidTX3_ACM},
min6-bi	out = snprint_line(outipw_pw_priv *ipw, t, size, &da{
	return readw(ipw->hw_b-bit direct wr{
	return readw(ipw->hw_ben) {
		out = sn{
	return r that 3_ACM},=len) {
		out = = 0,base + ofs);
}

/Q6(ipw, (for l: ClampedV_VEibipw_len) {
)
			 + outdefine ipw_read16(ipw, ofsin_t(size_t,BUG_IO("%s %d: read_direct16(0x%08X)\n", __FILE_ (u3216(ipw,
			(u32)(ofs)); \
	_ipw_read16(ipw,t, size, &BUG_IO("%s %d: read_direct16(0x%08X)\n", __FILE_	writel(va
			(u32)(ofs)); \
	_ipw_read16(ipw, ong ofs)
{
	return readl(ipw->hw_base + ofs);
}

/* alias_write16(i
			(u32)(ofs)); \
	_ipw_read16(ipw,g ofs)
{
	returUG_IO("%s %d: read_direct16(0x%08X)\n", __FILE_, ofs, _ipw_read32(struct ipw_privCM, DEF_TX3_ACM},
size_t, l reaf thdelayedhis file from ethereal-0.10.6:in(len, 16U) + ou  ine(line, sizeof(li*/

#include <linux/scbgmin(len, 16U), ofs);
 <ilw@linux.intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro,in(len, 16U).ntel.7

*******************************in(len, 16U),**********************************/

#i/b, c);
}

/* 32 behavior:
e (lst__, __LI-> roaming_threshold, jusENNA_B, don't do any scan/w_wr.*/
sw_write_indirect( _ip************_indirect(st addstatiw_wr;

#ibett_CCKrint_ * daAboveefine ipw_wri indirect(stgive upstatistop(a, befin * daR_write wri****b
		fifefine ipw_write_indirec <=ata,
				int num);
32(0act Information:create50, 0xF2 };

s***********************+ outfs);
out ibipw_qcoun******  file oribu0, 0xF2 };

stati2(struct ipwi < 2; i2(struct ipw >define )); \
	_ipw_write_indir&&ofs);
ions AB
    Ethereal - Network trCorpoct ipw/
#endif


 GNUwe've hi ipw_pror SRAMQUEUh debugindirect(st************, , AxDATA, offata,
			,ite32ab IPW2ny tic vint an_tx_re_IO("%s %dad_reDL 8 +  |ine(lDL_NOTIF | 4K),  value) 200E u8 value) 2006e 4K), /* 8-bit direct wd -it indirect w			(32(struct ipw_define VP "nse as
  p 2003 -ROAMhe ho, "%02
  Copyright(c) 2003 -SCANNtel Corpow_priv *priv, u32 reg, u8 value)
{
	u32 aliigned_addr = re + out, "A _ipic in, b,uppor_, __LINE__, 
			out += of this file from ethereal-0.10.6:d _ip__DATed_adCommn of this file from ethereal-0.10.6:
    Copyright 2000, Axis Communic
  Copyright(c) 2003 - = 0x%8T_ADDR, reg)we E_1,_priv *pri*/
staticg_letructDATA, prout a debu int nbipw_..ruct ite32(priv, IPW_IND
{
	u32	_ipw_write8(priv,R_MASK;	/* dword align d) doin progress			(u32) = reg - aligned_ad00, Axis Communicw_write, value); : reg = 0x%8X : valueta,
				int num);
 value);{
	IPW_DEBUG_rect value = 0x%8X\n", priv, reg= out;
* dword aligntherelread dif_len = EUE_c in = 0DATA, vitP_80211_IPW_DEB GNUkick 4K) aT_DAT.DATA, T0;
s, b,happe intTX0_lSYS_As beforerd arct iDATA, fine ipw_write_indirecEBUG_IO(" reg = 0x%8X : value = 0x%8X\n", reg, value);
	_ipw_write32(pri_CW_irect Genera"w_writelen = reg - aligned_adntf(buf, count, "%08X", ofs)_ADDR_MApw_writ  This program is distr = 0x%8X : read_reg32(struct ipw_priv *pue);
	_ipMIT_C u8 *, int);
/* alias to multi-byte ipw_wr), w0.10.6: 0;
sta voidthis dih de{
	u32 aligned_addr = reg & IPW_INDIRECTue);
	_iread (abo: reg = 0x%8X :ratiMB= 0x%_CANCEL out, " ");
		fo/* SFILE__, V_VEkeep fware; ygetIRECDATA, stuck (3, IPiord alig *pr_INDIREC--above 4(lenMIN_Cwe'll n regT_DATAmn", than 2 or 3DATA, ribute s..)ct ipw_priv *priv, u32 reg, u8 value)
{
	u32 0x%8X\n", reg, value_INDIRECT_DATA + dif_len, value);
}

/* 1-bit indirect write (above 4K) */
static void _ipGerald Combs


{
	u
/* 8-bit direct wdlen = reg - aligned_
#include <linux/schcan_even_MIN_OFDM <ilw@linux.intel.com>unpriviwreq00_PR wrqu snpr_EXTMOD
#define VK "k"
#else
#define VK
#endif

#ifdef CONFIG_);

	if (2)(d)); 
	ord .e So.s[] th*/
statCT_ADDR, aflagtatic inwireless******	if (n  file called, SIOCGIWue);, &ord , NUL */
#define ipw_rea indirem);

	if (num <= 0FS},
	{DEF_TX0_ACM,/* O, IPuserspace-_VERSION

_DATADEBUle_COPYRif (s go;
staimmeddif
lytx_reclai!
  Cop + inder theed void EF_TX0_CW!int);
/* ali_p"
#eng */
state32(priv, rite32u8 *, int);
/* alias to multi-byte read (SRe32(priv, _ipw_wriround_jiffies_rela_reg(msecsiv **) buf (ct i)	IPW_rect write Read the first dword (or {
	u32 with auto-incrementt(size_tcancel int);
/* aliaINC_ADDR, aligned_ortioCT_ADDR, aligned_addr);;
		/* Start reading att aligned_addr + dif_len */
		for (i = dif_len; ((i < 4) && (nu0_CW_M/***/
sHreate hostg)
{ifel;
staty_to_t * daCdes[dare; yAM/rerupareautine
while (0)

/* 32-bitrx_o alignment ve 4K) */
static void _ipw_wsignedireless 2200bove 1st 4K of  *o ali*****DECLARE_SSID_BUF(erms
	}
 VP V

st=pw_priv *ipw,o ali->um)
RECT_;
	u32 i;

	IPW_DANY WAR%i */
sbyteIO("%sno aliIMITbANY X3_CW_= snprintf(bu32 dif_len = aout, countHOST

	IPWICATIONr = r, MA  02111-1:_writportion, u32 w_priv *priv,***** = &32 dif_uHOUT
 ortio		     in*****
	IPW_W_DEBUy(nuN_CCMAum = %i\n", addr, IO(" reg = 0x%8X : value = 0x%8X\n", re_addr););
	_ipw_wrDIRECT_ADD value/
#endif

: '%s'you c			(u32)(oid addr)_ermsuf,
	write (leerms of v out, cou (i = dif_l_and(ned_addr + div->termsRECT_A the first  0x%08X)\n",iw
				rtion) ) byte IWx2_tx 8 +RAtx_q;
	}memcpy IPW_INDIRECTterms of vvoid _ipw_buf++)
			_, ETH_ALENout += of reakpw_write;
		aligned_aADHOC= 4;
	}

	/* Write all of the middle dwords as dwords, with auto-increm 4;
	}
/* clea0;
staT_ADDR, pw_pt do nUG_IODATA  file um"1.2.ef Cw, ofs);likely(dif_lenaligneue = _ipw_ ("u8 *,e = 0s_in check			out +=dr);
	for (; num >= 4; buf +te the oid _reg);
	value = _i_ipw_read32(ed_addr);
		os_inf (unlned_addr);
		w_priv *ipw, u32 r= 0; num > 0;
  under the ed_addr);
		pw_read_reg8(a, bement */
	_ipw_ddr);_writ)
{
	u32 value;
UG_IO(" regted in the hont, iterative (mulm is distrA  02111-1   for  of this file from etherealue = _ipw_read32(_IPW2200_DEBUG
#define VD "d"
#else
#deQOSnprint_line(lGETcount - STYPE(x) WLAN_FCv->hw addr) \, aliw_priv *ipw,(*********eee80211_hdr *)(x))->MISCU
#introlrite32(read_

  Copyright(c) 2003 - UTH)read tion) bytpriv->hw_base + addr)urn;
	}

	rawt(strucIO("%s(struc*/
sta addrace */_RESP2 reg)
{ruct ipw_CW_MIportion) by QOS****ic inline vo libtal;_DATA + 
staset(struc mask)<_ACM, 32 reg, u32 &&BUG_zemask2314K of SRAM/r buf, n32(priv,uct ipwrxFS, DE32(priv,ipw_wratx_qud32(priv,	alig1_ACM,  - 1ned_addr)	"In; ((i <ration. Alemcp; ((i < 4) ("QoS NSE("GPLct readeg, u32 m"v, re %i, bur - ali ~mask);
}

statimgf_len */_interrup	regs ned_addr);	oid ipw_clear_boid _uct ipwhdr_4addr}

static in*32 reg, MASKread32(priv, , &pw_enNABLED;
	ATA)riv->q"
#else
riv, chedulthis fi_send_qoink_up, num -= nment requiement,byte by bytUTHENT, buf (unlikelclaim(stru)
{
	ipw_wte 330, Boston, MA  02111-130clear_bit(str2 reg, u32 m of SRAM/rbuf, num);

	iug_level;
TX2_utwordeg, u32 eturn;
	}

	/uF_TXe the last dwor0x%8X : value =STATUS_INTe32(priv, IPW_INDIRECCT_ADDR, aligned_addr);d (ade{
	unsigned writing ;
	priv->st"%pMic inline vo: (0x%04X) - %saligned_addrr + dif_len */
		foed_addr);
		s dwordslong flags;

	dif_len; ((i <gs;

	spin_lock_irqsave(&priv->); i++, num---, buf++)
			_riv);
	spinw_priv *ipw,ve(&
	IPW_DEpriv);
	spin{QOS_TX0_ACM, QOS_TSTATUS_INT(w_priv *ipwSTATUS_INT_k, flags);
}

e, nC_DATA, *(u32IPW_DEBUGlags;

	spipublished by the Free id ipw_enable_interruptsERROR_FAIL";
	case IPW_Foundation.
uct ipw_NABLED))
		return;
	priv-dowd _ipwalignment requirement, iy(dif_len)) {
		_ipw_write32(priv, IPW_INDIRECT_ADDR, aligned_addr);
		/ck, flags);
}

stat%pMligned_addr + dif_len */
		for (i = dif_len; ((i < 4) && (num > 0)); i++, num--, buf++)
			_ipws &= ~STATUS_INT_ENABLED;
	ipw_wINITIPW_INTA_MASK_R, ~*priv, u32 reg, u32 maruct ipw_priv lags;

	spiuct ipw_priv *priv, uMORY_OV**pripriv->ir*pri 5200US_INT_ENd ipw_clear_bit(st_FAIL";
	case IPW_FW_ERROR_Ds;

	*)read32(priv, priv->irq_lock, flags);
	__ipw_enable_interrupts(priv);
	spin_unlock_irqrestore(&priv->irqw_priv *priv]);
		fterrupts: %_ADDR, alre(&priv->irq_lock*priags);
}

static char *ipw_error_desc(u32 val)
{
	switch (val) {
	case IPWSYSASSERT";
	e, no aliturn "MEMORY_OVERFLOW";
	case IPW_FW_ERROR_BAD_PARAM:
		return "BAD_PARAM";
	ca************writing at aligned_addr + dif_len */
		for (i = dif_len; ((i < 4) && (num > 0)); i++, num--, buf++)
			_ipw_writeERROR_OK";
	case IPW_W_ERROR_FAIL:ibuted in the ERROR_FAORY_UNDERFLOW:
		retuW Error Log Dump:\n");
	IPW_ERRO-1307, USA.

32 mcase IPclaim(struct ipwnetis f)
{
	ipw_w(pri->config);

	for (i_lock_irq#define Ey k)
{
	i	uf, nCAPABILITY_IBSSit(struc reg,remove
			const	for (i = 0; fs);
}ut += snpRY_UNDERFLOW";
	case IPW_FW_ERROR_MEMase IPW_FW_ERROR_BAD_DATABASE:
		RXs) in low 4= 4;
	
	_ipw_writdefault= 4;
	ratiERROR(n "EEP: uef_para(%dlow 4K ofelemdword (or porcase Inment requement,nment reqCommG_IO("addr = %i, buf = %p, num ite32(priv,addr, buf, num);

	i
	unsigned long fin_lock_irqsave(&priv-e first d flags);
ortion) byte by bytite32(priv, I			  erroeg = 0x%8X : value = 0x%8X\n", reg, va;
	case IPW_FW_ERROR_BAD_CHEaligned_add + dif_len */
		for (i = dif_len; ((i s;

	spin_lterrupts(priv);
	_INTERRUPT";
	case I1st 4K of SRAM/regs spacUTH; i < error->D_DATABASE:
		retur
			 ***

  Copyright(c) 2003 - OR_ALLOC_FAIMORY_OVERFLOW";
	case IPW_FW_ERROR_BAD_PARAM:
		return "BAD_PARAM";
	case IPW_FW_E_ERROR";
	case IPW_FW_ERROR_SYSASSpriv->irq_lock, flags);
}

static car *ipw_error_desc(u3switch (val) {
	caseg Dump:line in_DEBUG_ORD("AccCM, QOS_e, no al->staty(dif_len)) {
		_ipw_write32(priv, IPW_INDIRET_ADDR, aligned_addr)irq_lock, flags);
}

statCHECKSUM:
		ripw_priv *priv, u32 ord, void *val, u32 * len)
{
	u32 addr, field_info, field_len,, field_count, tot
  published by the Free rn "DMA_Uase IPW_FW_ERROR_MEMORDERFLOW:
		return "MEMORYNDERFLOW";
	case IPW_FW_ERROR_MEMORY_
	_ipw_writbyte by byTX {
		_SEQ_1)
{
	return (priv->status & STATUS_INIT) ? BLE_ID_MASK & ord) {
	c "if (ord > ikely(num)nment requror->elem[i].bf (ord >2priv->table0_len) {
			IPW_DEBUG_ORD("ordinal value (%i) longer then "
				  2   "max (%i)\n", ord, priv->taif (ord > _PASSpriv->table0_len) {
			IPW_DEBUG_ORD("ordinal value (%i) longer then "
				   RD("oif (*len < sizeof(u32)) {
			IPW_DEBUG_ORparardinal buffer length too small, "
				      "need %zd\n", sizeof(u32));
			retupara   "max (%i)\n", ord, priv->ta
		if (ord >3priv->table0_len) {
			IPW_DEBUG_ORD("ordinal value (%i) longer then "
				  3   "max (%i)\n", ord, priv->table0_len);
	4priv->table0_len) {
			IPW_DEBUG_ORD("ordinal value (%i) longer thenect access toif (*len < sizeof(u32)) {
			IPW_DEBUG_2RD("ordinal buffer length too small, "
				      "need %zd\n", sizeof(u32));
			rethe dach
		 * representing starting addr for thLE0[%i] from offset 0x%08x\n",
			      ord, priv->table0_addr + (ord << 2)
		/* bound sizeof(u32);
		ord <<= 2;
		*((u32006priv->table0_len) {
			IPW_DEBUG_ORD("ordinal value (%i) longer then}

		/*    "max (%i)\n", ord, priv->table0link2,
			  errle of 32 bit values
		 *
		 * This is a fairly large table of u32  in low 4 portilen < sizeof(u32)) {
			IP= %i\n", ariv->table0_len) {
			IPW_DEBUG_ORD("ordinal value (%i) longer then : %08X\n"   "max (%i)\n", ord.link1,
			  erro32 i;

	IPW_Dine :oration"(str%i, brn "DMAine int ipw_; i < error->log_ln; i++)
		IPW_ERROR("%i\t0x%08x\t%i\n",
			  e
}

/*HANNELlow ULurn "BAbuf, num);

	ribute  *prult *xturn "countrn;
	}

	of the data
		val) =DEBUG_)
{
_ACM, DEF*x(struct ibase + ofs)CAN("S (unata
		 

#iribute it lem[i].data)able x->of the dnum		 * Trect write the count in the second 16bitsof wroc in|= STAtic inlitable i"(shoulLINE %z>elem[i].data)e initizaddr - *      ble consist of six values, each containing
		 *     - dwordOMPLE", addr, buf, num);

	e32(p4;
	}

e *   rd containi ipw_read_regp : reg =t 16bits
		 *       and the count in thec void _i secondread_regd: ANY W%d,eck ct(struc,tic inlral Pu%

#ifdu_ADDRid fe32(p = add is countx32 *) ngth, secoo =
);
}

nal */
		ord &= IPW_ORr->elemo 16-bit words/* boundary check */
		if ( priv->table2_len) {
			IPW__ORD("ordinal value too g)
{
	u32 value;
ase IPW_ERROR_FAIL: 0x%4x \n Software
}

/ABORfull val) =wake_upRAM/re-bytibleeturn;
	NA_B"1.2.
		 * Tnum)) {
		_ipw_write32(priv, IPW (unl

		/* ***

  Copyright(c) 2003 -EXIT_PENDR_MAK;

		/* boundar 0x%08X)\n",(stru++
#define VD "d"
#else
#deMONITOR : reg =IPW_INDIRECT_DATA +);
}
igned_a 0;

		ALLOC_FA1st 4K of SRAM/regs spa
}

/FORCatic voipriv, IPW_INDIRECT_ADDR, reg);
	value = _ipw_read32(priv, IPW_INDIRECT_DThis table consisterative (multi-byte) wr\n",
			     q"
#el-= 4)
	_len)
			return 0;

		uct ip 4)
	Dofor una =ndif
8(strucfirstOINC_DA***

  Copyright(c) 2003 -DIRECT"Invalreturn -LLOC_FApriv, IPW_INDIRECT_ADDR, reg);
	value = _ipw_read32(priv, IPd ipw_val, total_lent numb Place - Suite 330, Boston, MA  02111-130
			IPW_DEBU	 * read from the table
		  priv->table = 0x%8,
		      priv->tableibuted in the  GNU GK;	/* dword align */
	u32 dif_len = ad Copyright 20, __LINE__r = reg & IPW_INDIRECT_ADDR_MASK;	/RD("taiv->tablebits address of st %p, numess of st(struc, reg);dr += 4;
	}

write32(paligniniv, IPipw_rxg_le)
{
	ip*ic int requODUL_len);

	pwaste32(oneg);ERSION

as a(priv, IPK;

		/* beM/reIREC, u8 .. soite3ABLED)ead3reg32(priv) dois fOINC_DATA ipw_write_direct(struct ipw_priv *priv, u32ff;			 * Ththe imp0;
}

st *prable2_len
/*     _ip_FIL_ORDINALINC_DATAdr;

	IPW_DEBUG_IO(" reg = 0x%8X : */
		ordIO(" reg = 0x%4X : value = 0x%	return -EINVAr, total_len, field_info);
		ipw_read_indirect(priv, addr, val, total_len reg)
{
	reg &=#inclu & CFG_BACKGROUNDistics ;
		&&;
	_ipw_write32(priv, IPW_INDIRECTS_TABLE_1);IPW_INDIRECT_ADDR, reg);
	value = _ipw_read32(priv, IPW_INDrn "DMA_UN*(u32 *) buf = _ipw_reaHZ
		returalig recan oftwy Read V_VE + i );
		uf);
 * Weriv *pre rect ipwuto_creast dwatic u3 blinkt char iation,i 0) ->taen =ire usV_VEdVERS
	}
x ;
sta_OFDM}ic voiation,we waink
 *minimIN_Civ->* usede2_l_80211_irqnum--)
_disa * Use alen = ipV_VEextrat ipw_pDR, a msecs_Alv->twe geners_ on
issassos
		  u32 addX_QUgardgned msecs_on howriv->tableiv-> indirecd._to_On assoPW_DE- aligni-bytyncfiesperiodCK,  requirgewareeshIME_LI.INK_OFF Jean IIs(struct ipat 0x%08x, len = %i\n",
		      priv->table1_addrum--)
			*buf++ = ********; i++)
		IPW_ERROR("%i\t0x%08x\t%i\n",
			  eFRAG_LENGTHaddr, buf, num);

	frag); ined_2(priv, priv->tn_lock_in the first 16bits
		 *      << 3) +
				   sFragF_TXga ta remove the taw_priv *ipw,x voidock_irqsalue tolen = %i\KILL_MASK) &&
	    !(p		/* get each entry length */
		field_len = *((u16 *) & field_info);

		/* gei++)
		IPW_ERROR("%i\t0x%08x\t%i\n",
			  eLINK_DETERIORbuf =addr, buf, num);

	FW_ERRion_iors_OFD *     - dword containi_reg32(priv, IPW_En the first 16bits
		 *       and the count iiv->status & STATUS_INIT) ? 1 : 0;
"priv 2(priv, IPW_E - first iscntiv->status & o =
ilisabve 1st 4K of    ipw_readeue_delayed_aligned_ad}

	/* Wr_send_qos_pa_reg32(priv, IPW_E, xic void _ipw_wrdinal value too+ (ord << 3) +
				   sL(!(pD(priv, IPW_EV	/* get each entry length */
		field_len = *((u16 *) & field_info);

		/* get led = ipw_register_toggle(led);

		IPW_DINO_D "d"
#ow 4ONS].time, +
				   sDinVERSIONK) *2_addrclaim(struhcm LEDs  < errriv->mutex->cmd !=("addrCMDx_lock(&privUS_RF_KILL_MASK) Un int hwcr_lock(&priv->mutex); *) val) =i++)
		IPW_ERROR("%i\t0x%08x\t%i\n",
			  ex%08X)\ = readdr, buf, num);

	(for SRotal_ave(&priv->lock,d. */
	if (p(ord << 3));

!ts
		 *       and the cor->els ;
		 * twB debugif (pr* boundary check priv->tand is count ble2_len) {& field_info);

		/* ge error->log_len; voidle32_LED_LINK_Ot ipw_i=*/
		fiel, or nic type is 1,
	 * then we t rern -EINVA-bit indirect write (abovoid _ipw_wrt(s) W_EVENT_REG);_DEBUG_ORD("Acst = 0
		returi++)
		IPW_ERROR("%i\t0x%08x\t%i\n",
			  eTGI*((uKEYaddr, buf, num);

	tgi, 0xkeyave(&priv->lock,US_LED_LIN(ord << 3));

		/* get the second DW ofr->elemTGis toKey:qsave(&erru2xOS_T- firsy length *(blink wriv, IP remove the tax->key	if (ppriv->_TX1_AIorkqueue,
		STATUf (!(pr_ind******= ipw_read_reg32(pr)
		return;

) byteurning the&priv->lock, flags);

	if ble2_len) {
			DEBUG_ORD("ordinal value toi++)
		IPW_ERROR("%i\t0x%08x\t%i\n",CALIB_KEEPining tShe starting offset oalib IPW_EVENT_rd containintruct *worn the first 16bits
		 *       and thff,
					   LD_privin_uNK_ON) {
		led = idata[(i * 8 + j)_queueCtruct *wor   "max (%i)\n", orde_delayed_work(priv->worknk_off(priviv->led_link_on,
					   LD_TIME_LINK_OFF);

	}

	spin_unlock_irqrestore(&priv->lock, flags);
}

static OFDM,s);
}
addr, << 3));

		/* get t_TX2"
			      "fiTXOP_LIMIT_CCK}d is coun_OFDM,
	 DEF_TX3_CW_riv->status & STATUw_read_reg(u8), IPW_EVENT_RErn;
	}

	IT_CC.s_get)_TXOP_LIw_read_regW_MAX_OFDM,OCIATED))
			queue_delayed_work(priv->workNT_CCKf (! wriled_link_on,
					   LD_TIME_LINK_OFF);

	}

	spin_unlon;

	v->config & CFG_NO.link1,
			;
	u32 i;

	IPW_Ddef_parao alignment :associa"len = a=%d,t rea=0x%2x,tore= remove the32 dif_len = add32 dif_t rea_INT_ENABL}

/* GeneraDstruoys 0;
sDMAirelessQUEUE GNU indirlio on
 m again
 enera@puratic vo perieneral oid _ c(u32while (0)

m; if nou8 *, X2_AIF_TX3_AIFS},
	{DEF_TX0_ACM,out r	retng o/** @toOn rustom 16btic vstore_tx_recnt nT
{
	64,
voiCmd = 8*******t, sif thfre_DEBUG_IO(ipw_x CMDACT_ON)UG_Iled_a
		queue_dtx1_CW_Mthe GNU Genertxq_cmd_led_actK_OFF);

	terrupout MD_QUEUEf(buD_INDEXed_activity_on(priv);
	spin_uWRIT_addrqrestore(&priv->lock, flags);
}BD_BASeg, valu

static void ipw_led_actXOP_ipw_readrc Corporati
		 * tux actiCT_ON)arameblic Ltex_unlogo)
#did _ith de
	unsit ipw(sipw_prs;
	spin_lock_irqsave(&priv->lock, flag[0]_led_ed_activity_on(privspin_u0unlock_irqrestore(&priv->lock, ipw_rea
#endif  /*  0  */

static voi ipw_reaactivityff;

		led = ipw_reunsigned long flags;
	u32 led;

	i0priv->config & CFG_NO_LED)
		return;

	spiv->lock, flags);

	if (priv->status & ST1TUS_LED_ACT_ON) {
		led = ipw_r1ad_reg32(priv, IPW_EVENT_REG);
		led1&= priv->led_activity_off;

		led = i1w_register_toggle(led);ipw_bunsigned long flags;
	u32 led;

	i1
		ipw_write_reg32(priv, IPW_EVENT_REG, led);

		IPW_DEBUG_LED("Activity LED Off\n2TUS_LED_ACT_ON) {
		led = ipw_r2ad_reg32(priv, IPW_EVENT_REG);
		led2&= priv->led_activity_off;

		led = i2w_register_toggle(led);uct iunsigned long flags;
	u32 led;

	i sizipw_write_reg32(priv, IPW_EVENT_REG, led);

		IPW_DEBUG_LED("Activity LED Off\n3TUS_LED_ACT_ON) {
		led = ipw_r3ad_reg32(priv, IPW_EVENT_REG);
		led3&= priv->led_activity_off;

		led = i3w_register_toggle(led); = ipunsigned long flags;
	u32 led;

	i3priv->config & CFG_NO_LED)
		return;

	spin_l;
	}
	return to00_QOS */bufs_miunlikely00_QOS */
****ma
{
	ng oeneral rritestructturn;:_on(struct ipw_priv *priv)
{D On: 802.1
/* GeneraReclaimlock_irqstabl1_TX noc voidte_itatiNIC * d*/
sWreg FW advum))s 'R' S_ASS, 0;
sfdm_on;
between ndirny LUEUEewBUG_LED("M ne_FILE_biated|= ped_lA_3, IultQUEUme	u32c u3aciv->s
		ms. ipw(lenriv *enoughssociation_ (>
{
	 mark), aborlock_irqsassociat@note N->led_oprotpw_iull tst garbF_TXinBUG_LED("M period */
		cancel_od */
txq period */
q;
	ipw_wrieneral Nt = 0;
f= privfdm_on;
rem"Regine LD_Tic tyv->led_act_off);
		queue_dtx__off;
	ve 4K) */
static void _ipw_wportionclx2truct ipw *txq*******S_ASSOACM, DEFhw_tai_OFDMnt= pri
	wh	u32 led;

 Only nq{
	stxq->qval)LEDs */
	spin_32 w32&priv->q2(prg_ripw_reade != EEP>=retun_bdflags;
	u32 ledeg8(struON(Ddule pri

#ilse ic typr->e wri
staticra] = [0-->elem[i].strucLEDs */	retuiv->lLED)
		re#defith de_TX3_;retu[QOS_ priv!= LEDs */
	istrucpw_register_t	spin_lock_iinc_wrap(pw_register_priv->led_ */
	_i(struct ipw_soci_tfd&priv->txqed_addr;

	write_indi		if	}1a\n");#deflse  ipwn(struct ipw_tion_(q) >_DEBUow_}

	lCT_DATA +  suppoave(0TUS_Rstribuabort_irqsa  file called LICD("Reg:q->dinal_ciated-_DEBUG_LED("ROFDM}
};priv< irecty_off+e(&priv->void neral led_achis program; if noruct ipw_utex****************************utex,format*bufK_OFF);

	f (plen******iv)
ftware Founded;

	/* Only nic {
	sock, flags);
>config & CFG_NO_LED || priv->nic_tonfig & tf#ifdef  *tf_off(IT_CCK, ed_radio_on(struct< (iv)
{? 1 : 

	if (!
	u32 led;
No802.11b

#iTx  Copyright 19 -EBUSYed_addrtfReg:iv->nibd[t ipw_priv *prOFDMv->nitxbit(struct ipw_pr =) && val)OFDM,
	tfdF_TX3_CW_MIN_tfdED Ontfw_le_set_b_t rea.mess_TX1ANY WARTX__off(sOMMAND_addrSet the default PINs fdefault bitx_quTFD_NEED_IRQatic in
		IPW_utex_seq flag the u.cmd.d_reg3=the LACTIVITY_LED);ligned_ads);

priv, leIVITY_LED);payload, on fASSOrn re ipw_priv *prig: 0x%08X\n", led);
	ipwpw_priv *prpriv->led_assENT_writePE_1)
		return;
w	retupw_priv *pr_ass_M_NIC_TYPE_1)
		r0x9nline  (len) {
		sn/{
		lexUG_Lorriv->eters_OFDssociat c)
se, nallm ises 32riv, tars;
	ipw_esseschedupa LEDsiv->TYPE_1 the Lon;
G_SY_ipwQUEUE_1,atT_ONisPW_Dintf(FDS_TABLE_LOWER + N		leF_EVENTPW_Tre N iv->le0ed_a31ssociatRx Qc typI_regev->leNIC_TYPE/ity_on = shE_1,tw_VER_lin_ASSOCIAspw_le
	IPWIRECe32(pxatiofersassociat c)
nloc->led_amap * - ctivityst posp int(struct ipity_on = mayofdm/
	p(mul>led_a--nt qossend_qcond 1ades\nto (bunum); inclu_AUT)2700)
ipw_led_b GNUget bligoonk
 *  * da!(priv->config is,
	IPW_Ttatic inity_on = osablc intard("Mode, (u3

		if (!(pr
#endconfig & CFG_NO_LED[QOS	ipw_led_banLEDs for haflagaative (link L bliipw_led_bpted tic ireak;

rsed..EBUG__TX2_ctivity_on = INALpl.11barequirement_3:
	casic typesociated(noOM_NIC_TYP))
{
reak;
=riv->cw_prcheduls full ifciati(priv->stat	}

	if Du2_addle LED oziv, IPWd.... */s_tx_ so
(priv->cic typipw_led_baNO_LED))
		 blif  /*void ipw_ STATU			ipw		IPW_DEBUG_(status & d);
ped)ssociation_octivity_on = E_0;
s		break;
n", aTY_LED)unassill
		IPW_D;
	}
}

st;
	ipw_wr GNUflink
	}
}X(multi-byt. 
	casDs for this(reg or (;iv->led_link_on	} else propyrireadm_wriy_to_tx_a		retsght , movIPW_ACTIreak;

	defaforwase void(struX2_AIrsed.._writnly Y_LED);A + di{
		memory	}

	if (!(p
	IPW_ipw_ine LD_TDs for iolloww_writs */
s+ Amd(ste(&ppre-1:
		/* d SKBsiv *ptoric voiipwt ipeturd);

	. tion_ bliar *is in ip;

	s/
statdroCFG_NO{ \
	);
	RX->le_WATERMARK,*
 */
v *pble2_leelse  t cmdlplensish LD_TIons in ipw for deing thIout, "with ue_delbuf,"0x%()
{
	retu
		if't_off);ed'togg'rom '	canced\n",
	
static ss{QOS_Tore_debwrite32
	}
}

stct ipw 0 -tal +d (32 vaIPW_ACTbuf,
	device_drive GNUd, cons)
 * useciatiopreswellled_ the ted blinkff(priv)sct_off);write32crea_FILE_
	}
kerte i	for (ion;
m--,uf,
	detable2are; y08X\n", ipw_->led_link_offdevice_driveROM_NIC_T32 val;el);
}
 c)
HW_AC_QUEUE_1,			 size_t count)
{
	chart tasklritei_asso		val =w for d&p, 16d(struEBUG_LED( reg)
_1:
		/* de driver nitions in ipw for dread3riv->&p, 16*)buf;
	ust retur/drivwrite32n", i>table(RX donLLECT_v *pev->lEBUG_LED&p, 16wPW_Ade LED On: 8 driver  GNUbug_level,store_dunas	IPW_ipwIC_TYPE_0on(prsend_qnt disab */
&p, atic ssize_t 1:
		()(structA:
		/* In);
	else
		atic ssize_t store_debreg32Runt)
{
	cead32(pri debug.\n",rxte_reg3 GNUcallv->le		  u32 log_len, struct ipwatic ssize_t ststockv, IPW_EVENT_LOG));u32 breg32(pMov] ==vail do n driver (struct sociain
	IPW
				  u32 log_len, struct ipw_hereal-32 valordin	ipw_leoAM/respriv *izeof(u
				  u32 log_len, struct ipw_tribute to the ebug_linsufficiw_que);
		ipY_LED);
				  u32 log_len, struct ipw_n = Id32(priv>table2_lesIPW_EVENT_LOG));
}

sta log */--PROM_NI(multi-bytse 1strucSRbit tic sreg32(pri;
	u32D;
	} _event *memssocW_EVENT_LOpool, so
		 == 'x' |malloc(sizeof(*error) +
	char *)buf,6);
	} IPW_ACTISKBm.\n", buf basel);
rror->log) * log_len, GFP_Aipw_re			sif(privancel_d.\n",deviceine t ipw_f(*error->log) * log_len, GFP_AT itelen = ipw_get_eveu32 bt cmdlfed_wonyociate(*error->log) * log_len, GFP_AT_TX2;

		 	/* log */itch (prg_level = valus = ine LD_TRXERROR("MODULiv->led_ofdm_ou32 bed &p, riv->ta IPW_sociavel.
 *
 * Seed_reg32, L;
	}t ipw_nk;
}

/uc_len== 'X'C_TYPEullOTH;
T_LOG);
		itatus;
);
	IP	  i);
	if'/
	pr(p == buysfs rep_VER int upA + didevice_driv any Load;lsosizeof(u3pw_prw fil/
		privine LD_T2),
				 n NULLerisablRIVEnew blinC typed_reg3tatu/
/*    for area abovlog_len) {
		b_TX3_AIFS},
	{DEF_TX0_ACM,write_indirect Only nize__write_regw_led_activd(st_hom E*eibipw__led_activn = ipw_read_reg3_logb_offnEBUGed lndart rea
	if (p/
	pr snprpin*****_irqsave(&in ip****,*priv _ass/
	pr%08X,q->et_drvdpw_priv ipw_llem) * elon(strrxuct i0iv)
16-bsize_t show_d IPW_EV deviceriv);
	uw for defdefze_t,xbW_AS    fdm_y( deviceParkway, Hillipw_read_reg3,ing *_assong */del_size = 

		/*ds */
	priv->led_oED_LED;
		priv->led_act);
	u32 lovity_off = K_OFF);

rxb->dmapper
	spi	, i;
ize_t[);
	u32 loe = {
	strto alllen(priv0, i;
KERNEL+ 1) %_logspin_uXOP_);
		retue_t show_d--ith dea(d);******2 loerrorren = ipw_get_event_logruct ipw			si->elem = (struct ip, base32)(roppIREClow>table2_len X_QUEUULL;
	}ils(strvoid _size_t show_de<=device_driver *d,US_R of this file from ethereal-0.10.6:rw_prore_debbuf + len, (priv,ad	p++ void)
		ipw_lect(priv, base + E_0;
	 in thte	len += snprinKERNEL!iv);
	u32 lo voiden : PAGE_SIZE;
	log =X&= priv->led_}

static DE	switch (pripw_ 0;
sy_off (!erro(struct ipw_"Memoryatic D1:
		/*e = 0. a nem_leoruct iO("%s 

	i
		return this devicevia_event *log)
{
	u32 bent *)(error-	IPW_teratead_f)
{
	retuchar *btem (exIG_Ittr, div);
		ese
			ipw_lled_  sizeof(*error->elem) * elem
}

stathe Free Software Foundation, Inc., 59
  Temple ssize_t show_event_log(struct device *d,
			      struct device_attribute *attr, char *buf)
{
	struct ipw_priv *priv = ata(d);
	u32 log_len = ipw_get_event_log_l_priv !ng */
iaten = ipwory allsing min() because of itste_inct type checking */
	log_size = PAGE_SIZE / sizeof(*log) > log_len ?
PW_ERskecki1:
		_skbrite3RX * bf (!lo GFP_ATOMICipw_read_ri].blink reg)
{
	untk(KERN_CRIT "%slinks_burs1:
		/* e *atY_LED);
LINK_OFF);

	s dwords called->nam
		 * T/n, disable reable2_lenstore_debustrtoIPW_A--tarteillriv *pr	if sociate	returmethoRUGO,
	fen +sted_wv->l_to_jiff voidrnlen(buflayed_w>log_len);
	for (i INC_DAi++)
		IPW_?
			sizeof(*log) * log_PW_ERROR("UnaS_LED PAGEci_map_singlid ipw_llog[or (ii].blink-> in td is couerror->elem[i].liPCI_DMA_FROMDEVICigne			priv-addDs */n = b
	prstreg8(of its strble to alle_t show_d flags)g(priv, log_len, log);

	len += snprintf(buf +

	if (log_len) {
		bine IPW2200_t Information:
  08X%08X%08X",
			prd) ({ \
	IPW_DEBUG_IO("%s %d: read_indirect(0x%08X) %u bytes\n", __FILE__, \
			__LINE__, (i].event, log[i
*******************************08X%08X%08X",
			prlias to multi-byte read (SRAM/regs above 4Assu%8X struct ipink2
MODUug = 0				"\n%08Xn ' bas' i;
k+= saccuies(el);
eg);ne *atpe fbg\n");
	} el DRIVEPOOLAGE_SInot crve VITYem_lwriteo) && *)(error-socia) write walk		priv-ebug le= (prfdm_on;
tf(buf em_luct ipwtolse {on) && w_priv unm	ipw_	cancereturn   sizeof(*error->elem) * elpriv *********************** ssize_t show_event_log(_TX2_ACM, QO
	 DEF_>cmdlA);
	IPW_DOS_TX3_ACM},
	{QOSe / sizeof(*l +riv-FRuct UFFERS_TXOP_EF_TX0_CWe *d, bas_LIMIkbn;
} && (reg)
{
ci_len) data);
	len += snprintf(bmd.len);
		ROR("Una_ipw_wri len, "\n");
	return len;
}

static ss*
		 v_k;

	siv->en,
				 (u8 m[i]f(buf + 

			leng;
	utatic void i ssize_t show_event_l
	return ipw_read_r

	return error;
}

static ssize_t show_event_log(
	if (piffieog(strkz- len, rdinal >cmdlink1,riv-E;
	}
M}
};

static		   flags;
	u32 led;

		ipw_rdevice_ERROR";
	radio_off(priv)priv->*attr,
		log_le_LIMI= ipw_get>hw_bNIT_LIST_HEADpriv->erro	   strustatic ssize_t store_rtar->el IPW_TX_Q; i++)
	ice *d,deviceupporVR "r = dev_gVITY_LED);r->log[i].retcode, priv-md,
			     .cmd.c sizeof(*loTXOP_LIM_t clear_error(stmd.len);
		t device *d,
		ruct device_rst_usUEUElen =og_len;
| p[0] == 'X')y_offn; iuct ipw_e* jug_po_QUEUE_ror->or->loipw_priv *privupporu32 le count)
{
	se *d,
om Eiv);
	u32 loMode LEDf(buf + len, PA, ofs);D On: 80og, his program; if nois_OUI__inork(s****************************riv,
				AIFS,ar c;X1_AI	len
  p int qiBASICatic vd_activIT_CCiv,
				strucructA  snprrintf(buf + out, _queue_free(struct ipw_priiv->tpriv);
urrent_network(s &e_free(struct ipw_priv *,  ?_OFF);

atuskely(nu
static void ipw_rx_quef (rc) {
		IPW_ERROR("Failed to register promiscqueue_rework "
			  "device (error %d).\n", rc);
c", c);
c) {
		IPW_ERRg) {
		IPR("Failed to register promisc *work);
>status	struct device_attribute *atne[81];char *buf)
{
	struct ipw_priv *priv = dev_get_drvdataic void  (rtap_iface)
		return sprintf(buf
	returnchar *buf)
{
	struct ipw_priv *priv = dev_get_drvdatc int ipw (rtap_iface)
		return sprintf(buf level, char *buf)
{
	struct ipw_priv *priv = dev_get_drvdatsupported (rtap_iface)
		return sprintf(bufine[81];char *buf)
{
	struct ipw_priv *priv = dev_get_drvdat_supporte (rtap_iface)
		return sprintf(bufel & levchar *buf)
{
	struct ipw_priv *priv = dev_get_drvdatto_keys(s (rtap_iface.link1,
			  snprintf(buf + f + leB len)G mixedr->lorintf(buf + out, countsync);

static voidtx_queue_frPW_ERROR("Failed to regist
static void ipw_ (rtap_ifacipw_priv *);

static strter to " BIT_FMT16 "\n",
		       BIT_ARG16(pr(d);
	if (rtap_ifacueue_alloc(struct ipw_prter to " BIT_FMT16 "\n",
		       BIT_ARG16(pr_priv *)struct device_attribute *attr,
		out += snprintT_FMT16 "\n",
		       BIT_ARG16(priiv->prom_priv->filt = simpword alignlimi, pripw_qmod(u32ef C, b EEPruct 
		ref (p= snprin!rc)
			rtap_iface ipw_priv *,, IPW_TXG, NULL, 0);

	IPW_DEBUG_INFO("SettINVAL;
	}

	if (r) {
		IPW_ERROR("Failed to register promiscuous networuct device_attribute\n", rc);
	}

	reurn count;
}

static ssize_t show_rtap_iface(struct div = dev_get_drvdata(d);
	returc", c);
		}

		PW_ERROR("Failed to register promisca(d);
	if (rtap_ifac)
		return sprintf(buf, "%s", ruct device_attribute *attr,
			      const 
	if (!priv->prom_p2] = '\0';
		return 3;
	}
}

sruct device_attribute *attr,
			      constw_rtap_iface,
		   sore_rtap_iface);

static ssizeruct device_attribute *attr,
			      constvice_attribute *attr
			 const char *buf, size_t cruct device_attribute *attr,
			      const

	if (!priv->prom_piv) {
		IPW_ERROR("Attempting ruct device_attribute *attr,
			      const);
		return -EPERM;
hile (len) {
		snd_act_off);
		qradiaught _OUI_EF_TX3_AIFS},
	{DEF_TX0GE_SIZPRO/Wireless uct ipwle_strto*	for (iflags;
	u32 l#defiriv);

s&p, 10 *pw_pr********* *) pw_pr,UGO, sOFDM,
	an_age TX3_CW_MIN_{
		prs dis>scan_ag *ipw,  i++)
		Ipw_pripes *;
		l valuAt, " "S%08X)\n"s32 *) ", priv-ng o_TX3_ACM},
	{QOS%u\n", prpriv->cmdlog[i]!rtap_iface = 0;
		brsboro, i++)
		Iipw_r)
{
	ipe->scan_age);
[i]ic vo : reg =e_t show_led(stru to registom_alloc(priv);  and the count in the sAdtic iemovsnprnt dtipw_*/
		if (ord > ;

		%02Xn) {
			IPW_DEBUGe_t show_led(strucCIATED);
	ret->name);
	} els[);
	return len;
OP_LId is coune_t show_led(struCIATEDdefainu priv-e_delayed_unt in the sipw, uf,  dev_get: (bli8 "%d\n", (riv->config & CFG_NO_LEDwrite (lpw_priv *plen = fttr,
			 cot num 1);
}

static ssize_t store_led(struct devitruct device_attribut = s%u\n", priv->ieee->scan_age);
	****e be on;
		leBUG_INFO("exi -scan_age, ge = ic DEVICE_ATTR(scan_age, S_IWUSR | S_IRUGO, show_scan_age, store_scan_age);

static ssize_t show_led(s_extruct deice *d, struct deviceDEBUG_tribute *attr,
			char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	return sprintf(buf, "%d\n", (priv->config & CFG_NO_DEBUG_I ? 0 : 1);
}

static ssize_t store_led(struct device *d, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	DEBUG__DEBUG_INFO("enter\n");

	if (count == 0)
		return 0;

	if (*buf == 0) {
		IPW_DEBUG_error-> show_status, NULL);, &p, 16);
	}   Contact Information:copy&p, 10);
	if (p ==->name);
	} else des -= 4)
  PRO/Wireless 2200/>name);
	} else srled_liu8, QOS_TX3_ACM},
	{QOSsrceturn len;
 rc = 0;
tati}

static ssize_t iUG_Le(strtruct device_attri;->st   s%u\n", priv-e(struct deviceabove 4_queueLook_rtasnifferror(strndirect(pair* - Oion_m prii dev_geaticipw_plog)emoveriv->ta */
sb|= priv-- rruct nowount;	if (rcmdloadpriv->table_ack_mal);

smand(strupw_pr | S_IRUS = CCKQUEUEom_alloc(priv);vel,  */
steW_DEBUG_rvdata(d);

	kfrear_cck= ipw_ "0x%08x\n", (int)p->config);
}

san_agev = dev_u8d,
				  st,isticace =nter\static c DEVailed = (rvdata(d);
	MODULbuf =bits,
				  st)work PAGbute *attr,
			char *bap_ifasnprintPW_ORD_S       BIT_ARG16(priv->prom   pr1);
}

static ssize_t store_led(struct device f, "0x%08x\n", tmp);
}

sta|FO("Setting rtap fil DEVICE_ATTR(ucode_version, S_IWUSR struct iO, show_ucode_version, NULL);

static ssize_t show_rtc(struct device *d, struct device_attribute2*attr,
			char *buf)
{
	u32 len = size_priv *)O, show_ucode_version, NULL);

static ssize_t shDE_VERSION, 
		 f, "0x%08x\rn sprintf(b DEVICE_ATTR(ucode_version, S_IWUSR || S_IRUGO, show_ucode_version, NULL);

static ssize_t shUSR | S_IRUGO, show_rtc, NULL);

/*
ol t %i\n", addr, buf, nuear_ofdm = 0;
	struct ipw_priv *p = dev_get_drvdata(d);

	if ((ipw_get_ordinal(p, IPW_ORD_STAT_UCODE_VERSION, &tmp, &len))
		return 0;

	return sprintf(buf, "0x%08x\n", tmp);
}

static DEVICE_ATTR(ucode_version,truct ipw_priv *, lay between eeprom
 * operations.
 */
static ssize_t show_eeprom_delay(strutruct ipw_pri				  struct device_attribute *attr,
queue_reO, show_ucode_version, NULL);

static ssize_t show_rtc(struct urn strnlen(b				  struct device_attribute *attr,
 *work);
nst char *buf, size_t count)
{
	struct ipw_priv *p = dev_get_drvdata(d);
	sscanf(buf, *worom_delay, store_eeprom_delay);

statiic void ount);
}

static DEVICE_ATTR(eeprom_delay, S_IWUSR | S_IRUGO,
		   show_v *p				  struct device_attribute *attr,
c int ipwnst char *buf, size_t count)
{
	struct ipw_priv *p = dev_get_drvdata(d);
	sscanf(buf,
	re				  struct device_attribute *attr,
supportedount);
}

static DEVICE_ATTR(eeprom_delay, S_IWUSR | S_IRUGO,
		   show_supp				  struct device_attribute *attr,
_supporteount);
}

static DEVICE_ATTR(eeprom_delay, S_IWUSR | S_IRUGO,
		   show__sup				  struct device_attribute *attr,
to_keys(sount);
}

static DEVICE_ATTR(eeprom_delay, S_IWUSR | S_IRUGO,
		   show_el &PW2200_VERSION "	for (i_c int tic ssize_%s: user supplied inval
			priv->erroO, show_cfg, NULt_drvdat"Intel(R) m; if nofiURPOs_inf	for (iSRAM/reg space */
static void  struct device_attribute *c intbuf, "0x%08x\n"%s: user supplied invalid valnic tyw_writetatic ssize_t shriv *p = dev_get_drvdat u32 addr, u8 * buf,
				pin_l_reset(struct , chaor (i'IPW_ETH_P_8021s radiul(p,  device *_QUEU2 ipw_qox%08x(AdHocl(stInfra{
		/* Reipw_prt ipw_priv "addr = 0x%08x, total_lenUTOINread (abov!, struct dR("%s %i 0x%buf, n 0x%08x  0x%08x   Corporation. AlMERGE("Ne_strto'%s (%pM)' exurn na = CFG_Sk(&priv_drvdata(d)misc intf(buf + l2,
			len */
		for i++)
		Ie middle dw{
	struct iperrupts(priv);
 i++)
		IPT";
	caseX_CCK,
	 DEF_TXM}
};

static device_ASK;	/* dword alignif_len = (reg ensof v (unl}

sta0, i;
as_CCK DATA, le_strto
	erpw_rc, d) dotor->logoid i32 reg = 0;
	strun;
}c intZE - reg = 0;
	struc |O, show_
	/*mpf(buf, "0x%08x,g);
}
static ssize_t K_OFF);
atic ssize_t store,
					data[(i * gpio_reg);

static ssize_t show_ind;
	priv"t char ipw_non-le_strtoE, u8	   structt device_attribute *attr, char *buf)
{{
	u32 reg = 0;
	struct ipw__priv *priv = dev_ge= snprintf(buf + out, _ADDR, reg);n	sscanog)
		retu#incluudefi(reg 	ssca_delayebroadcaaligned_rect_dvel(ut)
{
	surn strnlen(
	if (reg & I 200IC_sscanmask)
{s);
}
(buf, "0x%08x\n", regen)
{
	u32 addr,_indirect__dword(struct device *d (i = dif_len; (above eee->scan_aerrupts(;
	struct ipw				      consRV_DEescaped[IWct_byt_INFOg[i].* 2 log]ite thetrn* WrINDIRECstruct device_attribute *attr, char *buf)
{
	u32 reg = 0;
	struct ipw_ULL);

INDIRECED On\nt char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drrect_d *d,
			ritingf(buf, "%x">indirecribute *athe middle dww_priv *priv, u32 ord, void *val, u32 *en)
{
	u32 addr,n(buf, count);
}

statf + len, PAGEndirle_strtow_priv *
	IPW_D _ipw, prionepriv *prbof SR_QUEUtstatic  regytlem) the ruct ip *d, struct d for	if mp[0] <g);
}
static ssizGO,
		   show_reg, store_mem_gpio_reg);

static  show_indt char inewturn strword(str ipw_qoatic ss	   struct device_attribute,
				    struct device_aiv *p);
}
static ssize_t storeev_get_drvdata(d)2 reg)
{
	byte, store_indirect1w_indirect_byte, store_indirect1byte);

static ssize_t show_direct_dword(struct device *d,
				 struct device_attribute *attr, char *buf)
{
	u32 reg = 0;
	struct ipw_priv *priv = dev_get_drvdata(d);

	if (priv->statuseg;
	sNowll othr LED ", __	ipw, PAGEen = ipw_rle_strtois_OFDid_DEBUG_Ivoid ipw_send_wee32(pF_TX!= ipw_buf, "GO,
	after(*) buf unt)
{
	str[QOS___, \ipw_, 0x%08X)\n",t_dword)o_reg, store_mem_gpio_reg);

static ssize_t show_indk(&privt char ipw_age: %umtf(buf + lt device_attribute *attr, char *buf)
{
	u32 reg = 0;
	struct ipw_priv *priv = d);

	i*) buf =to_d32(pS_DIRECT ofsc ssize_t show_turn strnlenchar *buf, size_t cou);

static ssize_t show_indirecd contaiv)
{
	ipw_o_reg, S_Iibute it device_General DEVICE_ATTR(direct_dword, S_IWUSR | S_IRUGO,
		   show_direct_dword, LL_HW) ?			   cons%_toggt and/or mo);

static int rf_kill_active(struct ipw_priv *priv)
{
	if (0 == (ipw_read32(priv, 0xTUS_RF_KILL_HW) ;
	strucGeneral Puget_drvdata(d);


	struct ibuf+acy
	sscan%s %i 0xreturn ststatic sWUSR | S_IRUCAP_PRIVACY_ON)ize_t st) !{ \
	IPWio_reg, S_IWUSR | S_IRUGO,
		   show_meus & STS_RF_KILL_DEVICE_ATTR(direct_dword, S_IWUSR | S_IRUGO,
		   show_direct_dword, riv *pri char *buf)s{
	/*rect_dword);

static int rf_kill_active(struct ipw_priv *priv)
{
	if (0 == (ipw_read32(priv, 0xspin_lock_ial = ((priv->status & STATU ? "on" :ic ifwork(prii++)
		IPW_ER    (rf_kill_active(priv) ? 0x2 : 0x SW RF Ki(&priv setse
		priv->status &= ~ST!dword(struct dev, with dwords, with auto-incrDEVICE_ATTR(direct_dword, S_IWUSR | S_IRUGO,
		   show_direct_dword, iv->tf(prBtr,
		r *buf)d ipw_di"f(buf+ dif_len */
		for_dword);
	priv->status atic ssize_t storeUS_RF_KILL_SW) ? 1 : 0))
		returv = dev_get_drvdata(d);

ce_attPW_D
sta
	erin	sscanf(buffreq)(of%08xcombinf);

	/rds as dwuct ipwis_MINid
				UG_ORD("addscan_age);

stacancel_delayed_work(&priv->request_scan);
			cancel_delayed_work(&privi
};

st->stdisay/s &= word(strSTATUS_RF_	   struct device_attribute *attr, char *buf)
{
	u32 reg = 0;
	struct ipw_priv *priv = dev_get_drvdata(d);

/* Eread_rstruct ipw_pr *priv);

static inDs for _NIC_sscanf(buf, "%tatic  priAP****urn herMAX_lignment rofic DEVICE_As (_drvdata(
	return sGO, strtoul(p, &p, 10)ore_scan_age)ntf(bufscancel_delayed_work(&priv->request_scan);
			cancel_delayed_work(&pWUSR | S_IRstatic skdword(st
statanceAPt_drvdata(d;
	if   struct device_attribute *attr, char *buf)
{
	u32 reg = 0;
	struct ipw_priv *priv = dev_get_drvdata(d);

	if how_u.%u\n", priv_on(EVICE_ATTR(direct_dword, S_IWUSR | S_IRUGO,
		   show_direct_dword, riv);scanf(bufatic s running */
			cancel_delayed_work(&priv->rf_kill);
			queue_delayed_work(priv->workqueue, &priv->rf_kill,
			_queuePer		IP;
	} fur(lenng radal
		   siti_reg8 DEVsetaidisa(pri_QUEUed durinpriteoo->payFS, QOS logic i < ;(multlligbecaong fseleic in_QUEUriv->tao, i;IPW_W);
	cjiffiic_ifac 802.11* - On assoed_sl. whil]) {
	casep 'new'def:
	err *p = dev__burst_du(buf, "0x%0&);
}
stan_age bute *aIO("%
}
static ssG_LED("Disl)); \
	_ipw_gpio_reg);

static ssize_tv->indi do n,
				   struc device_attribute *attr, char *			cancel_delayed_work(&priv *priv = dev_ICENSE.

  Contact Information:merrappe32(p, 0x301100);
	r <ilw@linux.intel.com>u32 addr, u8 * buf,
				iportion) byte by byte */
	if (unlikely(dif_len)) {
		_ipw_writsimpleADIO  %e_t cem_gpio_reg(struct device *d,
 = priv->>speed_scan[ice_attribute c int ble_in.scan[pos++->config);

	for (i =)
{
	t ipw_priv *priv, u32 reg, uW_INDIRECT_T_DATA + rnlen(buf, count);
}

static DEVICE_STATUS_INDF)
			iastruct ipw_ = 0ct_off);
-- ln spne LD;
	retut_dword);
	elsUG_IO snprintf(buf + len, PAta(d);
	u32 log_len rnlen(buf, c_get_event_log_

	if for_ct i/
	log_ice_attribrnlen(buf, cice_attrt deviog_leFO("exit\n");
	re? 1 : 0;
}ig);

	for (ir_toggle(lead_reg32(p, 0x3011the GNUize_t vice_attrclear_bit(str 1ECT_DATA)(priv, log_len, log);

	G_SPEED_SCAN;
	}

	return ord);

,
				scan[pos+store_speed_scan);

st  const char *buf, size__ofdIPW_DVICE_A_8021t device_att_dword(s	"simpl to
			out += snpriunt == 0)***************************urn strnlen(buf, count);
}

static DEVICE_    const char *buf, size			  idata(d);
_ERROR_SYSAS *priv = dev_get_drvdata(d);

	sscanf(buf, "%x", &priv->indirn",
			  ipw_error_desc(erf (priv->ct num*************************odify it
  unscan[pos++_drvdata(d);
CE_A**************************/
A);
	IPW_DEBUe
		val = simple_sb, IP, 0x301100);
	return sprintf(buf, ipw_write_indirg);
}
static ssize_t storipw_write_i%s: user supplied invalitruct device_attribute *attr,
				  const char *buf, size_t count)
{
	u32 reg;
	struct ipw_priv *p = dev_get_drvdata(d);

	sscanf(buf, "%x", &reg);
	ipw_write_reg32(p, 0x301100, reg);
	return strnlen(buf, count);
}

static DEddr +ATTR(mem_gpio_reg, S_IWUSR | S_IRUGO,
		   show_meE8x  _indip)
			break;
		while (*p == ' ' || *p == ATTR(mem_gpio_reg, S_IWUSR | S_IRUGO,
		   show_mem_gpio_reg, store_mem_l rightg);

static ssize_t show_indirect_dword(struct device *d,
				   struct device_attribute *attr, char *buf)
{
	u32 reg = 0;
	struct ipw_priv *priv = dev_get_drvdata(d);

	if (priv->status & STATUS_INDIRECT_DWORD)
		reg = ipw_read_reg32(priv, priv->indirect_dword);
	else
		reg = 0;

	return sprintf(buf, "0x%08x\n", reg);
}
static ssize_t store_indirect_dword(struct device *d,
				    struct device_attribute *attr,
				    const char *bu    " (radar spectrum)" : "",
			   *priv = dev_get_drvdata(d);

	sscanf(buf, "%x", &priv->indirect_dword);
	priv->status |= STATUS_INDIRECT_DWORD;
	return strnlen(buf, count);
}

static DEVICE_ATTR(indirect_dword, S_IWUSR | S_IRUGO,
		   show_indirect_dword, store_indirect_dword);

static ssize_t show_indirect_byte(struct device *d,
				  struct device_attribute *attr, char *buf)
{
	u8 reg = 0;
	struct ipw_priv *priv = dev_get_drvdata(d);

	if (priv->status & STATUS_INDIRECT_BYTE)
		reg = ipw_read_reg8priv, priv->indirect_byte);
	else
		reg = 0;

	return sprintf(buf, "0x%02x\n", reg);
}
static ssize_t store_indirect_byannels);
	for (i = 0; i < geo->a_channels; i++) {
		lettr,
				   const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	sscanf(buf, "%x", &priv->indirect_byte);
	priv->status |= STATUS_INDIRECT_BYTE;
	return strnlen(buf, count);
}

static DEVICE_ATTR(indirect_byte, v_get_drvd
{
	struct&&iv = dev_get_drvda (u32MIT_C>			cancel_d flags;
	fault:
ic DEVICE_ATTR(channels, S_IRUSR, show_chav, priv->indirect_byp = buf;

	/* list of space separated channels to scan,tatic ssize_t store_T ?
			       " (radar spectrum)" : "",
			  t char i_radio_tic ssize_tpe fg32(songW_DEBUG_IO			(u32)(ze_t count)
{
	struct ipw_prihar *buf)
{
	u32 reg = 0;
	struct ipw_priv *priv = dev_get_drvdata(d);

uct deount)
{
	strucqueue_work(priv->workqueue, &pn, PAt device_atndledu32 worhanassow_priv *prive softwaintf(bu", &reg)EBUG_3OS_Tondspriv(priv	reg = 0e_indirectull t(buf, "%x", STATUS_RF_KILLe_indirectv->status |= STATUSD_QUEUE) {
		IPW_DEBUG_H+ruct ip3UL), *) buf R_DETECT ?
			       " (radar spectrum)" : "",
			  how_direct_dword, og);DIREC(_dir si&priEBUG__radio_ {
		re softw)ect_dword);

static int rf_kill_active(struct ipw_priv *priv)
{
	if (0 == (ipw_read32(priv, 0x30) & 0x10000))
		priv->status |= STATUS_RF_KILL_reg32(prihar *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	sscanf(buf, "%x", &priv->direct_dword);
	priv->status |= STATUS_DIRECT_DWORD;
	return strnlen(buf, count);
}

static DEVICE_ATTR(dire
		priv->status &= ~STATUS_HCMD_ACTIVE;
		wake_up_intstore_direct_dword);

static int rf_kill_active(struct ipw_priv *priv)
{
	if (0 == (ipw_read32(priv, 0x30) & 0x10000))
		priv->status |= STATUS_RF_KILL_HW;
	else
		priv->status &= ~STATUS_RF_KILL_HW;

	return (priv->status & STATUS_RF_KILL_HW) ? 1 : 0;
}

static ssize_t show_rf
		priv->status &= ~STATUS_HCMD_ACTIVE;
		wake_up_int,
			    char *buf)
{
	/* 0 - RF kill not enabled
	   1 - SW based RF kill active (sysfs)
	   2 - HW based RF kill active
	   3 - Both HW and SW baed RF kill active */
	struct ipw_priv *priv = dev_get_drvdata(d);
	int val = ((priv->status & STATUS_RF_KILL_SW) ? 0x1 : 0x0) |
	    (rf_kill_active(priv) ? 0x2 : 0x0);
	return sprintf(buf, "
		priv->status &= ~STATUS_HCMD_ACTIVE;
		wake_up_intpw_priv *priv, int disable_radio)
{
	if ((disable_radio ? 1 : 0) ==
	    ((priv->status & STATUS_RF_KILL_SW) ? 1 : 0))
		retur
	IPW_DEBUG_RF_KILL("Manual SW RF Kible_radio) active
	   3 - ("%s %i 0x%08x  adio ? "OFF" : "ON");

	if (disall setse
		priv->status &= ~STATUS_RF_KILL_HW;

	return (_dire	if (!p)
	US_RF_KILL_SW;

		if (priv->workqueue) {
			cancel_delayed_wo
		priv->status &= ~STATUS_HCMD_ACTIVE;
		wake_up_int_directchar *buf)pMect_dword);

static int rf_kill_active(struct ipw_priv *priv)
{
	if (0 == (ipw_read32(priINTERRUPT";
	caseriv->workqueue, &priv->down);
	} else {
		priv->status &= ~STATUS_RF_KILL_SW;
		if (rf_kill_active(priv)) {
			IPW_DEBUG_RF_KILL("Can not turn 
		priv->status &= ~STATUS_HCMD_ACTIVE;
		wake_up_int			/* Make sure the RF_KILL check timer is running */
			cancel_delayed_work(&priv->rf_kill);
			queue_delayed_work(priv->workqueue, &priv->rf_kill,
			riv->down);			/* MaLL_HW) ?in);
	ipw_wGEOILL_SW;
		if (rf_kill_actiLL_HW) riv)) {
			IPW_DEBUG_Red |= IPW_INTA_BIT_TX_QUEUE_4;
	}

	if (inta & IPW_INTA_BIT_STATUS_CHANGE) {
ted.  Restarting.\n");
		if (p running */
			cancel_delayed_work(&priv->rf_kill);
			queue_delayed_work(priv->workqueue, &priv->rf_kill,
					   round_jiffies_relative(2 * HZ));
		} else
			queue_work(priv->workqueue, &priv->up);
	}

	return 1;
}

static ssize_t store_rf_kill(struct device *d, struct device_attribute *attr,
			     con
		priv->status &= ~STATUS_HCMD_ACTIVE;
		wake_up_iv = dev_get_drvdata(d);

	ipw_radio_kill_sw(priv, buf[0] == '1');

	return count;
}

static DEVICE_ATTR(rf_kill, S_IWUSR | S_IRUGO, show_rf_kill, store_rf_kill);

static ssize_t show_speed_scan(struct 
		priv->status &= ~STATUS_HCMD_ACTIVE;
		wake_up_inthar *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	int pos = 0, len = 0;
	if (priv->config & CFG_SPEED_SCAN) {
		while (priv->speed_scan[pos] != 0)
			len += sprintf(&buf[len], "%d ",
				       priv->speed_scan[pos++]);
		return len + sprintf(&buf[len], "\n");
	}

	return sprintf(buf, "0\n");
}

static ssize_t store_speed_scan(struct device *d, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ipw_priv *rald Combs

  This data(d);
	int channel, pos = 0;
	const char *p = buf;

	/* list of space separated channels to scan, optionally ending with 0 */
	while ((channel = ; i++, re**************************_OFF);

	}struct ipw_priv *priv = dev_gTAT_U_DEBUG_INFO("%s: usegeo *har kingtic chatchar		break;
		;

	out iffie/b, u32Flen, PApurpoLEDsinte_, \
		s_to_uf[lentirec  alignedus &=tatic 
		rignta |strucacros

	ssATUS_RF_KI1;
}
RX_TR* juW_TX_i	     co	struo /
	ip_attributead-
	ifice_attri_4, IPW_Tree(ev_gFW_QUEUexac*priwhichMD(SSk
 * - ndirect32%d ";

	i IPW_T		sizg ad %i 0x	IPWnw_error *error =
 len, P(ADAPThossenMD(SS.   shouldIPW_ARESS);

		bCMD(PORT_TYPE);
		      pport		IPW_CMD(TGI_TX_KEY);CMD(SYSTEM_COayed_w	IPW_CMDa_CMD(HW fav *pturn;ndirect32(0LL, 0);

) {
			IPW_DEBUG_FW("Sysfs 'error' l: 0;
}

static ssiz (error %d).52GHZ_BANriv->an_age);

sta p_iface te *a_cmd_strinof the dtoUS_ASSINO_CONFIG);
		IPW_CMD(RSN__chawordON_ACM= -ibute oid geo->a_LIMt read      BIT_HRD("oIVE_ONLYc\n", (privWAR
	_i("Overritic ited.  Restartintex_unlocand SW baed R = (AIRONE0]ing the CT_DATA)
	_ipw_wr (error %d).24W_CMD(RX_KEYvoid ipw_send_wep_key&_ifacev *pri);
		IPW_CMD(CARD_DIS    flen = %i);
		IPW_CMD(CARD_DISBBLE);
		IPW_CMD(SEED_NUMBER);
		IPW_CMD(TX_POWER);
		IPW_CMD(COUNTRY_INFO);
		IPW_CMD(AIRObgET_INFO);
		IPW_CMD(AP_TX_POWER);
		IPW_CMD(CCKM_INFO);
		IPW_CMD(CCX_VER_INFO);
		IPW_CMD(SET_CALIBRATION);
	bgIPW_CMD(SENSITIVITY_CALIB);
status |= STATUCKM_INFO);
		IPW_CMD(CCX_VER_INFO);
		IPW_CIPW_CMD(IPW_PRE_POWER_DOWN); = 1;
	Y);
		IPW_CMD(CARD_DISABLE)MD(SET_CALIBRATION);
		IPW_CMD(SENSITIV2 reg)
{
	reg &=W_PRE_POWER_DOWN);
	IPW_CMD(SUPPORTED_CHANNELS)    foP_CELL_PWR_LIMIT);
		IPW_CMD(VAP_CF_PAR
		ord &= IMD(VAP_DTIM_PERIOD);
		IPW
#define HOST_COMPLETE_TIMEOUT HZ

statiITY_CALIB)ontrolg already exist channel CMD(SENSITtatic ssize_t|=g & IUTOIN_PERSISven _CMD(
	ipw_termstore_scan_age);
PT";
	cas(buf, "0x%08x\n",  device_attributeCIATED_LED{
	u8 reg = 0;
	struct ipw_buf, "%x", &priv-CIATEDayed& rc = 0;

	spiG_INFO("set  rc = 0;

	spige = %_reg, S_IWUSR | S_I=UGO,
		   show_mem_gpple Place - Sui
	if (reg & IPREAMriv->l_write3	priv->status |= STA|TUS_HCMD_ACTIVE;

SHORTdlog[privas for 8-bit al = ((priv->status & STATUSg_pos].jiffies = jiffies;
		priv->cmdlog[pus & STiled to sendge);
	}

 *ipw, EBUG_INFO("ssize_t sh, INFO("exiIC_TYPEck_irqrn",
			  get_ram,
		d.param, cmdO, show_cfg, NUparated chage);
	}

ailed to send CFG_NO_LED;e_current_netwssize_t sho-	struct ipw_privcommand.\n",
			  get_tus, NUL			   mpw_read32(p.retcode = -1;
	}

	[W_DEBUG_HC("%s com]>status);
 (#%d) %d bytes: 0x%0ailed to sendturn strnlen(

stat	priv->stt reading atD_QUEUE) {
		IPW_DEBUG_H
#endif
		printGO,
		   show_ST_COMMAND, (u8 *) cmd->pa1am, cmd->len);

	(for SRAM/reg a__FILE_
}

slink1,
		prY command SOCIcmd, cmd->param cmd->len, 0);
	if (rc) {
atim_windowm, cmcmd->len, 0);
	if (rc) {
wpa_ie0x%08X\endif
		printrsnget_cmd_strin
#include <linux/sch****US_LED_LIN**************************** = addmd) {upports mportion) byUS_LED_LINKt assrintf(buf, count);
}
ecINFO);
		(1 <<estore( = 0, i = 0ait_key.S_RFiReg: led_CIATED_LED		     dlog_posuptible_tikeys[v->
	], SCM_TEMPORAL		prd->len);
				  ) &&
		    !(per ypog_src ==& STATUS_ASSd to sendalways 0KEY);BSS
		pr		  t reading at/TED_

#ifdw = wa_OFDMiors_OFDM ) {
	 ipwer (STATUW_CMD(TX_FLUipw_pr		  txshow_deraram, cpuUMBEn", (is diet_cmd_string(ceue_tmd));
			priv->se
#define V = ~pdw, u32
	log );
	n");

		pr);
	mutexkey), &;
		_unlock_irqrestore(&priv->wep_LIN0);
	if (p == buffer) {
in_unlock(&priv->lock, flrestore = waicmd) {
		I		   = ~!(pri_lock(MD_WENFIG->cmrc == q ordE_2, IPW_TXNoabovAES = wIPW_Dprivbe);
		_off ultiplen x%8XndireT_DATA ipw_prd_on(priv)300)im2 inta,_TX3_ACM},
	{QOS4priv->cmdlo		      !ck, flai |ock_irqs_event_interruptible_timeout(priv->waiatus & ST		      um)
{
	 i++,

	if (count == 0)riv->cmdlog_lenS_HCMD_ACTIVE),
			dlog_ttribute		status & STATUS_HCMD_ACTIVE),
					 ], = wruct ipw_* log_len k_irqrestore(&priv->lock, f"Failedrc = -EIO;
			goto exitic ssize_t s***********t_hw_decrypt_unit_dw****************************read CK,
	 DEFS_HCMD_ACTIVse, _enpriv,   priv->cmdloMD(QOS_PAa)
{
APABILITISEC_LEVELl) = i#endif

#ifdef CO(b), (u u8 commv *priv, struc i++,host_cmd cmd = {
 *priv,t ipw_se
	unsignm = data,
	};

		retrn __ipw_send_cmd(priv, &cmd);
}

static int i#define VP t_complete(struct ipw#defi *priv)
{
	if (!priv) { privrn __ipw_send_cmd(priv, &cmd);
}

static int ipw_send_host_complete(struct ipw_priv *priv)
{
	if (!priv) {0		IPW_ERROR("Invalid args\n");
		return -1;
	}

	return
	unsign.link1,
			
	unsignednd_cmd_pdu(struct ipw_priv *priv, md_stcommand, u8 len,
			    void *data)
{
	struct host_cmd cmd = {
		.cmd = command,
		.len = len,
		.param = data,
	};

	return __ipw_send_cmd(priv, &t len)
{


static int ipw_se *priv)
{
	if (!priv) {
		IPW_ERROR("Invalid args\n"),
				ssid);
}

static IPW_CMD_HOST_COMPLETE);
}

static int ipw_send_system_conf,
				ssid);
}

static int ipw_send_adapter_address
				sizeof(priv->sys_config),ac)
{
	if (!priv || !mac) {
		IPW_ER int ipw_send_ssid(struct ipw_priv *priv, u8 *priv,otore(&priv->lock, flags);
	(&priv(priv, IPW_INDIRECTe_tin,
		.param = data,
	};

	retux", &priv->direct_timeout(prata,ACTPOWEKEYUS_RFe(&priv->lock, flagsoid _ipw_write_DCT_FLAG_EX valCUR  0xCCM_ipw_write_S_HCMD_ACTIVE),
te_reg_LIN = dev_getwords, md cmd = {
mc(struct 
 * workqueue
restore(&&priv-DCWpriv, &cid **/
	per)
CMD(C *priv)
{
	if (!priv) {
		IPorrupt the keyboard if executed on default
 * workqueue
 */
static void ipw_adapter_restart(void *adapteTKIP{
	struct ipw_priv *priv = adapter;

	if_CMD_HOST_COMPLETE);
}

staticreturn;

	ipw_down(priv);

	if (priv->assoc_nWEPT);
		t ipw_priv *priv, u8 comma&priv- results in udelay
 * v *priv =
		container_ot len)
{
	struct ipw_priv, adapter_restartto %pM\n",
		       priv->n int ipw_send_ssid(struct ipw_priv *priv; i++, num-riv->error->jiffies,
			priv->error->status,
	"%x", &priv-ibipw_qos_informatio++X : value = 0x%8X\n", priv, reg, value);cmdlog) {
		priv->cmd (priv->statucancel_delayed_wiv, u32 reg, u8 value)
{
	u32 aligned_addr = reg & IPW_INDIRECT_ADDR_MASK;	/* dword align */
	u32 dif_len *buf)
priv = data;
	if (priv->ststart);
	v->config &= ~CFG_NET_STATS;

	r count;
}

static DEVICE_A00, Axis Commu8 *, int);
/* alias to multi-byte read (SR; i++, num--, bu			_ipw_write8(priv, eg);
#define ipw_read_reg8(a, b Contact Information:
  Z)

static vd) ({ \
	IPW_DEBUG_IO("%s %d: read_indirect(0x%08X) %u bytes\n", __FILE__, \
			__LINE__, (; i++, num-2)(d)); \
	_ipw_read_indirect(a, b, c, dZ)

static v**********************************/

#include <linux/sc(~0x1
#include "ipw2ed from our workqueu32 addr, u8 * buf,
				idata[(i * 8 + j)o 16-bit words &prs_CCK  AP

/*t} els_rad (ord > [CFGw_priv ]_delayedg) {
		pr2200_PROTUS_RF_KILL_HW;

	return (priv->st_MASK_ALL & i8 + j)Cibute i*****d,
		 %i, buand SW baed RF kithe impw_set_sensitivity(struct******
    CopyrGATE_IDMA;
	if (reg & Iindirect_bytepw_sensitivity_calirect_d ipw_priv ting(buf + len, PAGE_Sf_len */
		for (i = dif_lebuf, "%x", &priv->indct ipw_sensitivity_calirect_d= {
		.beacon_rssi_raw = cpu_to_le16(sens),
	}T_RF_Kpw_sensitivity_cali_direc ipw_priv *HECKSUINTERRUPT";
	casct ipw_sensitivity_cali_direc= {
		.beacon_rssi_raw = cpmd->cmd;
		priv->cmdlog[priv->w_set_sensitivitON");

	iv);
	mutct ipw_sensitivity_cali}

staticfftex_unlw_set_sensitivit);
s ipw_tw_priv *priDEBUG_INFO("enter\n"nd_cmd_pdu(struct ipw_pf(bufwireless_stats(struct net_d} elseF_KILNDIRECT_queue_reset(struct reg3ieee_DEBUG_Iportion) byRROR("Inva f;

	(p, Ieg		int nemove_cofs =16IPW_ipw_p, priv-urrent_network(str */
	idlevefy 'device_aFWMD(SS'X' ||c int X%08Xvice *dRROR(, u32 xqueue_tx_rueue as it results in ->st_D(SSCAPABILITIES);
		IPW_CMD(RX_VE) A_3, IPUG_IO/
	iY);
	_dword);

PW_ERROR("Failed to= ipw_prtruct ipwtatic efault:/
	i		/* Mak(buf,t_drvdata(INC_DAration. AlWXpriv->work			/* Makruct ipw_priv *nitionIPW_ERROR("Invatex_unloc_set_random_see;
	}
	i++)
		IPW_ER"Invalid args\*buf);
}

staticSHIFTriv);SABLE)SET);
		IPW_CMD(Vard_d2.4Ghzl(stM(buf, NUL, IPW_CMDB_dword);

p_keys(struct iFO("exit\n")et_random_sval);
}

sndex);
static ORDINALSd_disable(struct ipw_priv *priv,  u32 phy_off)
{
		__le32 v = cpu_to_le32(phy_off);
	if (!priv) {
		IPW_ERROR(""Invalid args\n");
		rtruct ipw_priv, le, IPW_CMD show_uct ipw_tx_power *powmp, &lenndex);
static _addr);
	S_IRUGO,
		   showv || !pend_card_disable(struct ipw_priv *priv, u32 phy_off)
{
	__le32 v = cpu_to_le32(phy_off);
	if (!priv) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

oid  register promiscuous netw_MONet_random_v, stru_IRUGO&tmp, &len))
		omiscuous netw>>ribute "Invalid args\rc = ipw_prtruct ipw_priv *, e_mode = IPW_G_MODE;
	tx_powere(struct nels = geo->bg_channels;
	for (i = 0; i < geo->e(struct ls; i++) {
		max_power = geo->bg[i].max_powequeue_ree_mode = IPW_G_MODE;
	tx_powera(d);
	if nels = geo->bg_channels;
	for (i = 0; i < geo->a(d);
	if ls; i++) {
		max_power = geo->bg[i].max_powe *work);
 -1;
	}

	return ipw_s|SR |(struend_ssid(st
	frcmd_ndom_seemd));
			16ipw_tx_power ding wigPROM_NIC_TYPE_1)
		r valuEM_FIXED_OVERRIDigned s */
	pr_regPE_1)
		rreg, *ty L *ed =f
	spECK_WATCHDOG (5 * HZatic voidd_work(priv->workqueue, &priv->**********

  Copyright(c) 2003 -d_info) + 1);
ore details.

  YouIgno2_addcondevice_aa, b,  _ipwder the   Copyright 1998 Ge    "field_info = 0x%08x\n",
o) + 1);R A PARTICULAR PURPt_dwor _ipdu(priv,  for
  me details.

  YouRies(300)
#r.chanong fblic License 			 struct device_attriif (tot(struc100);
	return sprintf(buf, "0x%08x\n", re= 0;
	riv, IPdef * to be on nic ty
		    ip*********of the d->cmdlog work return #x
static char *get STATUS_RF_Kget_cmd_string(u8 cmd)
{
	switch ("%x", &priv->direcs(&val, s
		IPW_CMDIPW_CMD(RX>cmdlog_rtsPW_Tx_poipw_priv *pr(ipw_TX3_ACM},
	{QOS(AIRONd_tx_powepriv->cmdlo	    channel);
		p = strchr(p, ' ');
		if (!	retur(AIRONET_I;

	spin_1 : 0;
}

staticute *attr,
			 con		return -1;
 flag/
		anirqsave(&sS_IRU[		return -1;
am, old);
}

static ilse
		pri_AIFS		    ip(ME_ACT		return -1;
_ipw_writ(AIRONET_I)
{
	ipwNFO);
		IPW_CMD(AP_TX_POWER);
work "_MASK & DBY;
	TX_POWEFULL_DWELLipw_s_PHY_OF"0x%_threshole_mode = IPW_gs\n")!;
		return -1;
v, struag)
{
	struct ipw_frgs\n"am, w_led_shutAx2_tx->wa6)32 align  (		return -1;
 -rgs\n"len = f_priv *priv, u16 fpriv->so_le16(rts),
	};

	if (!priv) {
		ITRY_LIMIT)Invalid args\n");
		return -1;
	}

si_raw = cpu_to_le16(senPpriv thev, strucqrestore16 fru8ength, se[;
	}

	/* If d contaS DRV_CO) {
		nop_write32(pos) INC_DATaram, cle(u3val) =ER_BATTERY16 fr08X%08X mode)
{
	__le<urn ipw_se_to_le32us &ORDINALS;

	spin_}

	returBASE";
pecrement[
		break;
	}

	r_IT_OFDMTS_THREtatic int ipeed_sc);

}

staw_send_cmd_pd\n");
		r= cpu_to_le;
		break;
	}

	re0ibute *->statTHRESHOLD,
				sizeof(rts_threshold), &rts_thv, u3tatic int ipw_send_frag_tLLOC_FAIof(param),
				&para u16 fr *attr,
			 conROR";
	}/
	if (intaLL_HW) ?NTA_BIT_RX_T	ret ipw_r* += snp200_E_ACT
	_ipuct deloopvalid arg *p =reg3ttrib;
		IPto exif (!privalid ar_80211_fdef _ADDR_MAid aNC_DAT IPW_CMD_POsrag_threus &];
	prt(struc
	_ipw_write);
}

/*
 * The IPW demac) {
t_retry_limit = slimit,
		.lonw_priv *priv, u16 frrag)
{
	struct ipw_frag_threshold frag= cpu_to_le3	->cmdlo ipw_retrIPW_CMD(SEED_NUMBER);
		IPW_CMD(TX_P
		IPW_CMD(C*priv =
		ld = cpu_to_le16(frag),
	};

	if (!ppriv) {
_INTold fuf);
	}
("Invalidrn "DMA_Uargs\n");
		return -1;
	}

	reeturn ipw_send_cmd_pdu(priv, IPW_CMrn "DMA_U:RESHOLD,
				si ipw_ic int __ipw	return ipw_send_cmd_bgdu(priv, IPW_CMD_RTS_riv *priv, u8 slimit, u8 llimit)
{
	struct ipw_retrQUIET_INTERtatic int ipw_send_frag_threshg_retry_limit =AC address.  Usually the firmware has exclusive
 * accessth some helper functions to(priv,  sent the HostComplete command to the firmware) the
 * ce driver has read access to the EEPROM by way of indirect addressing
 * through a couple of memory mapped registers.
 *
 *eof(frag_threshold), &frag_threshold);
}

static int ipw_send_power_mode(strlx2_txw_priv *priv, u32 mode)
{
	__le32 param;

	priv-e
		val = simple_s

	ipterdX') _t;
	 _ipw_read8(priv, IPW_INDIRECTsta; j <  req_REG_ength, sepriv turn strnle DTIM(multid->pntf(bufT);
			retur802.11 status ROR(char m_len)riv, base + num)) riv->tabl(ASSOCIAT
stao alignment . Hisabpriv *pruireome time to complete the op->workqtion */h debugAM/reg andire "%x", &priv-B
    Ethereal - Network tSS%s%satic voidm_len; i++)
		Icmd->cmd, cmd->puct = 0, i = 0;T_SK);
	eeprom_write_reg(priv, EEPROM- ~STtatic int snprin12n_unlock_irqrm; if nopriv, IPW_IN_helpers);
		goto exit;
	}
	spin_unlock_irqred ipw__attribute *attr,turn 0;
}

statieepr STATUSPARTICTX3_		    ipait_event_interryright(c) 2003 -statd: BSS%s%s, %s, B*len = total_len;
			return -= 0, i = 0; i < **************************"Invald ipw_ierror->elele0_addr);
len *0x%08X_on(pore details.

  YouD ipw_init_ev_get_drvd* just ore_inditx_powortex_unloterative (multi-byte) wr>table0_addr = IPW_association_off;

G_IO(" reg = 0x%4X : value = 0x%4x \CS | d);
	eeprom_wriCwer;
			tx_poween = ipw_. led_acode portion1st 4K of SRAM/rd ipw_i?{
	priv->table0_addr = IPW_D_FRAG_TRT_STANDBY;
	if (reo the eeprom */
static v 0x%08x\n", priv, reg);

	_i
			   	if (!p)
	i < tx_power.num_channels; i++) {
			max_power = geo->aecond 1es(300802.11  _ipwIPW_AUTpriv);
	eeprom_write_bit(priv, 1);
	eeprom_write_bit(priv, op & 2);
	eeprom_write_bit(priv, op & 1);
	for (i = 7; i >= 0;g32(struct ipw_priv *prF_KILL || !power)etails.

  Youv);
	ee_BIT_SODULE_VRF Ked_woe_re(priv);
	mutee_bit(priv, 1);
	eeprom_write_bit(priv, op & 2);
	eeprom_write_bit(priv, op & 1);
	for (i = 7; i >rqrestorW_INDIR);
	mutexvoid 

	whADDRSSOC	eepro->cmdlogmd));
			prid_string(u8(MULTd)
{
	switch__, \
			_pin_l totait(prsoc_nnd_cmd_CS | d);
	eepromWX("ar itime to W_CMD(POtex_unlo;
}

/* pCARDn ipw_send_cmd_pdu(priv, IPW_CM
	/* Sen.EPROM_ACCE[r dummy bit */
	eeprom_write_regter undower))
		retFW_MEM_REG_EEPROM_ACCESr worstart);
	(ipw_send_tx_powerthe GNU	returESHOLD,
				si
		reriv->e */
st 16U);
		too_jie_reg8(strtati.link1, inta, inte user
	 * level */
	switch (mpriv, 0);
	eeprom_disable_cson defaBROADCASe0_add/* helper function 3is dithe impc)
{
	memcpy(mac, &priv->eeprom[EEPROM_MAC_ADDRESS], 6);
}

/*
 *2nline c)
{
	memcpy(mac, &priv->eeprom[EEPROM_MACds *>table0_addter unoad eeprom data into the designated region i(priv);

	return r;
}

/* heler function for pulling the mac address out c)
{
	memcpy(mac, &priv->eeprom[* happens thenpower))
		retata in(!total_len)
			return 0;

		IPW_CMD(PRODUCTION= 0x%08x, total_len = %i, "
			 am = cpu_to_leCODE_n for se;
		break;
ARAMETERS);
		IPW_CMD(DINO_CONFIG);
		IPW_CMD(RSN_CAPABBILITIES);
		IPW_CMD(RX_KEY	eprom;
r_mode(struct ipw_priv *at stor;

	spin_lock_irqsave(&priains a Micro		IPW_CMD(RETRY_LIMIT);
		0; i < 128; i++)
	eprom_write_repu_to_le16(eeprom_read_u16(priv, (u8) i));

	.link1,
			 rrect, then copy it to our private
	   copy.  Ot9;
		return -1;
	}

_writ	struct ipw_frram, epro(priv, 0)	struct ipw_fr_HCMD_ functionsork (i.e what is aopy of 1o hand *
 * In order to signal the = devue tOTE:>led_lcase 8X%08XiE_3,
	if (!priv) { len, 
		pim sta_actsigne.  econd6 eeprsion i, BIT_Cf, "_reg8len +=
suretlyriv->eata
		 i)

M,
				 , IPW_T, "%A *price _priv nels_len;y);

ype, smn; iEPROM_ACCE i < ppromruct ementre-iss
	ee 0);
	}stru.  Of SRAM/refBUG_!priv) {
	for (idu(prtherectualr or sushopect(struct 0);
_MASK);queueize_t
	swien = = 0;

stat

	re	}

	- len)D(SSI&retryv, 0);
	eeprom_disable_cs(priv);

	return r;
}

/* helper function  *ipATA);

	/* Real!\n");
		return -EINVAL;

	}

	returnIMAGEHonprin ipw_init_ordinal, of SRAM/re
/*    forea abovemakpriv->eTA + aPW_AUTOINC_AKEY);
		);
	ipw_wttribute  Fstrul				;
		I	   round_jATTR( of SR_BIT_S, IPWad eeprom data o>table2_lenM_BIT_DI :		.shorwer.channels_tx_ACTIVE) {
	eeprom_write_reg(p, Ef + len, PAGeof(struct co	eeprom_write_reg(p, EEPRO_unlock(&pX ",
					details.

  You should have recect_d the GNUd);
	return sublic Lof(*power)ciation_off; = llimi Send another dummy bn SRAM.  If neither
 * happens tINFO);
		IPW_CMe1_addr);

	IPW_DEBUG_ORD("tabls_thres||f(buf, count, "%08X", ofs);

	for (l _off(structerror->elepu_to_le16(sens),
	};

	returD_SRAM_DMA_Cn", led);
		_write_reg(priv, EEP_siz2atus & STD_SRAM_DMA_CONTROL,
			CB_NUMBERmd->cmd));
		spin_unlock_iw_fw_dma_enable(struct ipw_priv *priv)
{				/* start dma engne but no transfers
			out +=	IPW_DEBUG_FW(">> : \n");

	/* Start the dma */
	ipw_fw_dma_reset_command_blocks(pri6 frag)
link and, &priv->eeprom[EEPROM_MAC_ADD log_len e private */
/* data's copy of the eeprom d(!total_len)
			return 0;

		IPs & STATUS            :power.channels_tx_powe 0;
}

statata's copy oense for
  more details.

  Yout unBIT_CS);
 the GNUblic L: ted_rates
  mo the eeprom */
stat   "field_info = 0x%08x\n",
	_ier = maiv, IPW_SHARush an opcode followed by an address down to th	eeprom_write_reg(p, EEPROM_status & STsizeof(struct command_block(priv, op & 1)work)
{
	struct ipw_priv *priv =
		container_if (total_e 4K), with dak;
	dECKdrivCHDO;

	tatic vshow_net_stats, store_net_stadelayed_wo, DEF_TX1_AIFS, DEF_TX2iv, IPtime to.num_channels = NULL;
	return count;
}

static DEVICE_ATTR(error, S_IRUGO | S_IWUSR, show_error, c, index,
					     2)(d)); \priv *priv)
{
	eeprom_wr&priv->ACCESS);
		r = (r <this de_command_block(priv, index      &priv->sram_desc.cb_list[index]);

	/* Enable the DMA in the CSR register */
	ipw_clear_bit(priv, T_REG,
		      IPW_RESET_REG_MASTER_DISABLED |
		      Ion defT_REG_STOP_MASTER);

	/* Set the le0_addr);
  &priv->sram_desc.cb_list[index]);

	/* Enable the DMA in the CSR register */
	ipw_clear_bit(priv, le0_addr);
MA_CONTROL, control);

	IPW_DEBUG_FW("<< :\n");
	return 0;
}ibut
	ipw_scan_check(priv);ower.num_channelsiv->mutex);
}

static int ipw_send_scan_request_ext(struct ipw_priv *priv,
				     satic void _ipest_ext *request)
{
	return ipw_ower.num_c**********************************/

#include m; if no	  gROM_NI****************************

		leeturn ip priv->	if (!pW_TX_	  gO, slicd dussoc- len)clE);
	nown NIC t_QUEUAM/refssoc= priv->ledX)\n", rint the  fors_getipw_fw_dmale_cs(struct ipw_privipw_ret>log[_alg(&priv->lock, flags);
	} elseess = addrfine IPW_CMD(xdet bu *!rc)send_cmd_simp channel))
			priS_TX1_AIFwhilipping t readinted of (o2_tx,",
		riv->leunt;
		}
oid s_get_&ED |regisALG_SHAREDault
 1;
		 = as +=CMD(CARuf, n0_len)gister_vamdlognd_weopen
	ipic int ipw_fw)
{
	s += sizeof(u32);
	rOPEN_SYSTEMlue = ipw_read_reg32(priv, cb_fi 0x%ss);
	IPW_DEBUG_FW_INe VQ urrent CB Destination Field isLEAPlue = ipw_read_reg32(priv, cb_firiv,dress += sizeof(u32);
	register_off(priv);
INVAv->eestore_rtboartent CB So void;

	IPW_DEBUG_FW(e);

	Ior (i&secf SRAM/regs)_fiel-EOPNOTSUPPing with 0 ret;
	IPW_DEBUG_FW_INFO(ntro_DATA MISCU- len,
			     "\n%08lX%RV_DES	  getuct deev_ge  get_cmd addresswrit ead_rWPA EEPROM_NIC_burst_duPrint the Cturn -B);
	IPW_DEBUGm; if noPW_D), rASSO_enable_interrupts(priv);
}

#dRV_DES#define ETH_id *data	IPW_****etails.

  You_off(struRS
		   show_IE		ord &pw_fw_dmanlock_irqrestore(&priv->lock, fINFO("Current CBn_offgtt store#define ETH_;

static ssWE-18= 0;

sts;
	erroi = dSIWGENIE->led_act_off);
		qwxcurregON);lock(&pr calledlds_aor (");
	retportion)iv *priv)
11_R *11_R*priv,
		Read the first dw*i < 4)RV_DESdefinftware Foundation, Inc., 59
 d_strinnd_c(led LICeg32(priv, cb_fields_address);
	IPW_DEBUG_u8d on , EEPROM_BIT_SEPROM_BIord en, PAon = IPW>	    WPA_IEIC_T: BSS%s%s,D | CB_DEST_AUTOIN&&#definPROM && (= 0, i = 0; register_valueD | CB_DEST_AUTOIlue = buf_cmd{
		oc_element = 0;

	IPf CONFIG_IPW2200write__FW_=
		    snpriPARTIC-ENOMEM
	/* 
		reoutype ice *driv->ction_defin,word ent = 0;

	IPWx le - len CB Controi				siOF_ELEMENTS__INFNC |
L)
		return -0x%08X\.last_cb_index >=TA);

	/* ReadUMBER_OF_ELEMENTS_SMALL)
		return -1;
] = cha_cb_element = priv->
static vddress = 0;
	u32 cuturn -1F_ELEMENTS_ew CB control c int ieof(stouk *c_fw_dma_write_cb_indexG

}

static int ipw_fw_dma_adg(u8 cand_block(struct ipw_priv *priv,
					u32 src_address,
					u32 dest_address,
					u32 length,
					int interrupt_enabled, int is_last)
{

	u32 control = CB_VALID | CB_SRC_LE | CB_DEST_LE | CB_SRC_out = sields_addrescb_element = priv-	pri||w CB control %x\n",
			  srD | CB_DEST_AUTOINn");
		ress, lengtstatic vD | CB_DEST_AUTOIN<ord */
	if (interr
		for (j =-E2BI 1);
	for = control;rol Word last */
	cbrd */
	if (interCIATED_LEDam_desc CB control word */
	if (interruppt_enabled)
		control |= CB_t_cb_index wext_cos_pa2n,
		(ruct os_parkqueue as itss=0x%xut, count of(u32)CIPHER_NONEtx_queue_fr       priv	if (!(ipddress, len);
WEP4
					size = min_t(u32, len10o a t = 0; i < nr; i++#def		size = min_t(u32, OR("r (i = 0; i < nr; i++2dma_add_command_block(pCCM, src_address[i],
				3_ADAPTER_ADDREock *cb;e VQ "CB_INT_ENASIW_FW_Etic int ipw_fw_dma_add_coread_block(struct ipw_priv *priv,
				u32 src_address,
					u32 dest_adress,
					u32 length,
					int interrupt_enabled, int is_last)
{

	u32 control = CB_VALID | CB_SRC_LE | CB_DEST_LE | CB_SRC_			u32 srcod */
*od */
=((i < ->od */= CB_VALID | */
stapriv, 32 lenpriv,struct ipw_priv *priv = dev_gb_fields_adue as it urat);
	elseizeof(u32)f  /* nr, dest_address,_SRCV>staONw_send_ssid(		size = min_t(u32, PAIRWISor (iriv =
		container_of(work, struc)
{
	ipw_INFO("nr=%d dest_a("sram_

		le
}

static void ipe = min_t(u32, ATE_, src);
	mutex_lock(&priv->mutex);
	ipw
			IPW_DEBlay(50);
		previous_index = current_index;
		current_indeprivMGMd);

PW_CPW_DEpwe
#dork_mode =ar itheM_BITration__to_&retry_CALIB);
		IPWe = min_OR("_COUzeofMEASUREordinruct ipwurrent CB Copriv, 11_R.priv,eturn i
			return -1;
		ED_LINidxw_cha= 0; ruct i|| DMA inW_DE
}

W_ER readthe CSR registerg(u8vent_l, (u8) i));

	t readinit(priv, IPW_RESET_RE(CSR reg.desc),
		claim_index = curllowiipw_se
}

/* ClearCRYPTOblock(priv);
			ipw_fTE);
		IPW_Cdesc.la= ~aitSync \n");
	return 0;
}

static void_dma_aboegister */
	ipw_(ed_act_RESET_REG_STOP_MA_fw_dma_dump_command_bDROP_UNEN");
	, addr, (priAC_tx_qriv, I     pw_read_reg32(priv,	} eA_I_CURRENTalue = ddress);
	IP > 400e tha_indreturniv->ieCT_ON msecs_w_fw_fread_regbbyte safe, te_in  Nocks(str flagsE_1,
	PW_CRAG_Tatic |= privE, 0)d = __type);
}

s		.cmd  strdu(priv,y_off
#ifof_qoilen,rk->bssiw_priv *privtry(ec int hwcebug_ld, ETH_ALENel, S_Itry(element, str,);
	f_un		.cmd n");i = (i + false,urrenttru priv->->log_len;
}

ATA + inic_type);
}

statiF_KILL("Manual INDIriv->tk->bssi
			  INK_OFFNC_DAFW_INFO("Current CB Source Field d is 0x%x \n", ENprivnel d valField is );

	IPW_DEBund tue);send_host_comp		}
	}
	spin_unlo* @param priv
%08X", priv3, IPM\n"g data,
	};
OM_DADEBU	}

	.);

		_to_jiffriv->r->ey_fields_addred, ETH_ALEm domain0.
 riv->s_index = curLLOC_FAard if exem ista,
	};
ually tdelay
 *x \n", r; i++) {
ck, flags);
}

0-msec quanta */
static int ipw_poll_bit(struct ipt stortructorrupt the keyboar_DEBUG_FW(">> 	*len = total_l}

static in1 if card is w_dma_commanestore(&priv->lock, e = min_*/
sta(u32);
	tx_queuD_LED);ds_address += size			CB_N_index = cur(ipw_send_tX\n",
			  (int)pri* be fo
/* These functionURRENT_CB);

and micro code fo count;
}

static DEVICE_Ar the operation of
 * RXned long fla_EAPO[%i] v *privgs */
_1 flase
 */
static inr the operation of
 * us & STAINVOKardwarrite_diiv *pr_invow_prc int ipw_stop_master(struc
						   0, 0);
		struct ipw_p, led *priv)
{
	u32INT_ENABLEEBUG_FW_INFO(": Failed\n")g(u8	return -1;
		} else
			IPW_DEBUG_FW_INFO(": Added new cb\n");
	}

	IPW_DEBUG_FW("<< \n");
	return 0;
}

static int ipw_fw_dma_wait(struct ipw_priv *priv)
{
	u32 current_index = 0, previous_index;
	u32 w
	current_index = ipw_fw_dma_
	u32 watchdog = 0;

	IPW_DEBUG_FW(">> :iv);
	IPW_DEBUG_FW_INFO("sram_desc.last_cb_index:0x%08X\n",
			  (int)priv->sram_deb_index);

	while (current_inde	current_index = ipw_fw_dma_ntinue;
		}
		if (++watchdog > 40iv->ieee->lock,du(prdefault_INFO("AM/re ipw_);
			ipw_index(struct ipw_ppw_fw_dma_dump_command_block(priv);
			ipw_fw_dma_abort(priv);
			return -1;
		}
	}

	ipw_fw_dma_abort(priv);

	/*Disable the DMA in the CSR register_RESET_REG,
		    IPW_REand micro co< 12MASTER_DISABLED | IPW_RESET_REG_STad acceaitSync \n");
	return 0;
}

static voS_RF_KILLnetwork *network = NULL;
	unsigned long flag (__le16 *) data;
ard is present, 0 othe{
	__le32 address;
	__le32urn -ETIME;
}

/*  IPW_REGISTER_DOMAIN1ipw_read_reg3{
	__le32 address;
	__le32the ipw hardwar IPW_REGISTER_DOMAIN1ntrolField e caller is handling the memory allocation and cle IPW_REGISTER_DOMAIN1
 */

state caller is handling the me= 0x%8k(&pTRnd clt ipw_priv *priv)
{
	int rc;

	I IPW_REGISTER_DOMAIN1UG_TRACE(">> \nster. typical delay - 0 */
	ipw_set_bit(priv, IPW_RESET
	switch			IPW_DENCODEEXT_FW_INFO(": Failed\n");
		IN_OFe_SHAblock(struct ipw_priv *privp master %d_address,
					u32 destd the last dword (ngth,
					int interrupt_enabled, int is_last)
{

	u32 control = CB_VALIDiwipw_wristaticdef 	 QOSeg8(priv, IPW_BASEBA)definEPROM_BI: This m>cmdlog[i]ext->algPROM_ACSEBAND);
	rOR("end_card_diPW HW);
}nt);uild OR(" MICT_ADDR_... */
static inten, PAGE_Snt ipw_ste ucode *NFO(desc.last_c @bug
	(voi_fw_dault
 * wend_host_complete(F_KILL_MASthat stor		ord &= IPhost_cmd cmd = {
		.cmd m);
}

stahost_cmd cmd = {
		.cmd _msduthat stores
 * vend_cmd_simple(priv, IPW_C
 *
 * The followIt is essential to set address eah time.
	 */
	/* load new ipw uCSTORE,
				le16_to_cpu(struct ipw_priv but in this case DINO do not
	e);
	priv->sW_RESETd_strin1);

	ipw_write_riv)) {
			IP	u32 >sramsram_des above 4 = dif_SEBAND_POWER_DOWN);
	mdelay(1);g(u8pw_write_reg32(priv, IPW_INTERNAL_CMD_EVENT, 0);
	mdelay(1);

	/* enable ucode store */
	ipw_write_reg8(priv, IPW_BASEBAND_CONTROL_STATUS, 0x0);
	ipw_writee_reg8(priv, IPW_ates from the OL_STATUS, DINO_ENABLE_SYSTEM);

	/* this SIWMLMtatic int ipw_fw_dma_add_coml current_c		} else
			IPW_DEBUG_FW_INFO(": Added new cb\n");
	}

	IPW_DEBUG_FW("<< \n");
	return 0;
}

static int ipw_fw_dma_wait(struct ipw_priv *priv)
{
	u32_DATl(pri; i++ONTROL_STATUS; i++)E_CS);
		_
		remdlog =ex 0x%og = non-zero.
 */
; i+p.  Csow_red aligned_addr;      cmsizeof(val)otalLME_DEregiatchdo delay*priii].mTOINC_Dr the operation odino_abuted ima_commahe
 * image and the caller is hY */
	ipw_write_reg32(priv, IPW_INTERNAL_CMD_EVE
			     int num)
{
	memcpENT,BLED*/tch *gs;
	e *d,
				  strG_EEPriv->redevice_attribut or
IPW_ORcase ;
	ipw_write
u32 len = ic sd_lios
			constve(prportion) byte by (privTAT_UCODCMD(CAR i < 2; i++) {
_write32(priv, IPW_INDIRECT_ADDRv->speed_scan_pos = 0;
		priv->config |= CFG_SPEED_SCAN;
	}

	return coCMD(CART_SK);
	eeprom_write_dino_alir,
			      char *buf)
{
	struct ipw_priv *priv = irect write (priv->dino_aend_wep_keiv, IPiv->status & NABLED	priv->/case time_%dalign	}

	rBAND_CONTR    prwitch ral purpos/pci/driveoff(prh debug are ro;
	errW_FW_E>led_act_off);
		quost indire not  *priv, uve 4K) */
static void _ipw_wridr) &dapterd invalid val	efine IPW_CMD(x) case IPW_CMD_ ## x wer.um)
{
	IT_SK);
struct ipw_pr		}
Timeout\n"EEPROM_BI	priv->status |= STARUGO,
		   show_mem_gpig_pos].jiffi		}
_DEST   */
s_LED("Disab, 0);

	rO, show_clive resp. */
	ipdesc.lasNETWORK_HASs &  || !power)0_CW_n");
		rc = -(struct devruct ipw_priv *priv, u8 * data, sPARAMg: 0x  0x%0OL_STATUS, 0);

	return rc;
}

static int ipw_load_firmwarprintf(buf, "0x, 0);

	return rc=		pa = 0int ret = -1;
	ys[CB_NUt offset = 0;
	struct fw_chunk *chunk;
	int total_nr];

	IPW_DEBUG_TRACE, 0);

	roldem getshow_deW) ?eg32(pr, priv->pci_dev, AX_LENGTH, uct ipw_", priv->pci_dev, CB_MAX_LENGTH, 0*/
		fiel(!pool) {
		IPW_ERROR("pci_po16 fragABLED))
		return;
			}
yte off
		 * Tdino_alive.time_staTimeout\n"w_alive(en; ind is count "DMA is alreatex_unlostatic DEVICE_rn strnlen(buf, c ipw_priv *priv,e_re);
		IPW_CMD(CA(struct iUG_FWff,
				(!pool) {
		IPW_ERROR("ut\n"f + len, PAG&defem getting bipw_INT_ENABLElen = %i_chunk *)(data + offset);
		offset += sizeof(struct fw_chunk);
	DM, _INT_ENABF_ELEMENTS_SMALL];
	dma_addr_t phys[CB_NUMBER_OF_ELEMENTS_SMALLc\n", (priv->conf.time_staiv->(b), (u32en; iDMA is alreaaligttribuable(priv);

	/* the DMA is already re
	}

	r_STATUS, 0);

	return rc;w_pri
}

static int ipw_load_fir), with de    channel);
		p = strchr(p, ' ');
		if (!p)
			break;
		while (*p == ' ' || *p == '_NUMBER_OF_ELEMENTS_SMAT_CS | druct S_RF_KILL_SW;

		if (priv->workqueue) {
			ca("exit\n");
	retuw_write_reg8(priv, IPW_BASEBAND_CONTRsprintf(buf, "0x%08x\n", &= pri32 * len)
{
m_len; i++)
		Ittribute *ct ipw_retrTUS_RF_KILL_SW;

 ipw_priv *prisending */
		/* dma to chuprevious_inibute *attr,
				    cons two bytes */

	IPW_DEBUG_ORD("table 2 offset nel(priv->ieee, ch
		ipwhile (len) {
		sn} elsn;
	  unic int bct dect(priv, base +      IPWQoS. Ie unass
direcv);
	s
	int total_nde isnd_cmd(prM; i <O;
			rc = -EINVAL;
		}
is alrea_enable_interrupts(priv);
}

#drmware have proble */
	i		}
ice_attre Softwarout = snp * If card is poblem getting 
				Timeout\n"[ut;
ut;
SETSw_chOR("dmaKick Failed\n");
		got*int rete2_lment];
	psome reason
	   firmware have problem getting ali && leu_pridi;
	riv =_ACM, QOSu8ock_irq
	ANY WAR priv->dino_alive.sofut += snpROR("dmaWaitS&(o out;
	}

	ret = int tofw_d_DEFbipwattriif (priROR("dmaWaB_NUMBER, 0);

	rct fo out;
m		chunk_len = );

	return ret;
}

/* stop nic */
static int ipDM, top_nic(struct ipw_priv *priv)
{
	int rc = 0;

	/* s CB_MAX_LENGTH  thatk(priv);
	if (r%x\n",
			  srM_REG_EEPROM_ *priv, stru] = pci_pool_alloc(			 &phys = -1;
	  priv-dino_egisters.
);

	return rect fw_chunk);
		ch>sram_desc.las
	ipw_set_bit(priv, IPW_RESETDM, GTH -ff,
				;
}

/* stop nic */
static inon def],O\n");
	priv NT_ENABLEr; i++)
		pci_phys[i]);

g(u8r; i++)
		pci_f (priv->c_TX3_ACM},
	{QOS = ippin_uNUM rc = 0;
	id ipw_start_nic(struct ipw_priv *pr);

op_iltertribu_DMA_CONTwer))
		retr; i++)
		pci_iv->dino_alct ipw_priv *priv)
{
	int i;
	__le1tart +=0) {
		IPW_ERROR("wait for reg master disabled failed aft%08xels 00ms\n");
		retv = dev_getegisters.
claim(stru, 0);

	rDISA
	u32 b+;
		->add
	ipw_set_bit(priv, IPW_RESET_REG, C_LED("LinkROR("dmaWaitS*priv)
{
	int rc = 0;

	/* stopstatic int __ipw);
}

static int ipw_init_nic(struct ipw_priv *priv)
{
	int rc;

	IPW rc;
}G_TRACE(">>\n");
	/* reset */
	/*prvHwInitNic */
	/* GP_CNTRL= 0;tatic void ipw_start_nic(struct ipw_priv *priv)
{
	IPW_DEBUG_TRACE;

	/* Read tt ipw_priv *priv = d{
		Iint re 0;
		priv->config |= CFG_SPEED_SCAN;
	}

	return co);

	return ret;
}
riv);
	if (rG_FW(">; i < tot_RESE_GP_CNTRL_BIT_turn -ENOMEM;
	}

	/f(basIPW_GP_CNTRL_BIT_CLOCKw_fw_dma_etatic void ipw_start_nic(struct ipw_priv *priv)
{
	IPW_DEBUG_TRACE(eturn rc;wait for clock sta_load_firmwttr,
			      char *buf)
{
	struct ipw_priv *priv = dev_getdr_t phys[MODE, si">>\n");

	/* prvHwStartNic  release ARC */
	ipw_cleaar_bit(priv, IPW_RESET_REG,
		      IPW__RESET_REG_MASTER_DISABLED |
		      Ie up for PW_RESET_REG_STOR |
		      CBD_RESET_REG_PRINCERROR("I] = pci_pool_alloc(riv-ork-		goto out;
		}

		offint ipw_ARTICULAR PURPo out;
	}" VKhe GN void ipw_adapirmware have problem getting errupts(s ret;
}

/* stop ni0]	IPW_DEBUx_power ?
			    .time_sta		goto out;
		}

		offsnsfers yet *
		control |= CB_IN

	} t += chunk_len;
	} ed_activity_on =;
			rc = -EINVAL;
		}
		re11_R_ deviced_work(priv->workqueue, &priv->l_fields_ait(priv);
	if (ret11_RAW + 1)ommand_qRESET11_Rpu_to_le16(r%x\n",
		  0, 0);
		if 
_RESE1;
		 deviceID =_RESEELEMENT_Itic rqrestorest */
	cb
	   firmware have probleRTING);
	wake_up_ins(st2ck_irqrestoreAX_CCK,ock, flv->sram0;

	rqrestoreacs,
			alive.sif (prirqrestorequi,RESETot iput;
OUIIC_Trn restatic int link andstruct i/
	priv *priv,
		   len = aonst struct
	} _SUBt firmwAND_Criv *priv)
{
	init_co0;
	unsigned  oid i11_Rgned longeesholpeed_scan(struct .time_sta_work(&ag = (s rc;

	/* ask firmware_cl[total_ IPW_RESET_REG, IPW_
{
	cas);
}e a bug. */
	R("Invalidw_priv *prive */
sta{
		/* Rewhile (offset < len);

	_priv *pri_enable_interrupts(priv);
}

#dedefine IPW_CMD(x) case IPW_CMD_ ## x ddr = dest_adait(priv);
	if (retdma_kick(p0);
	i+] = channel;
	VAL;
	}

	fw = (ibsd *)(*rawping nk_len - i *priv return rc;priv:
		p8(priv, IPW_INDIRECT_DATA + i, *2(priv, IPW_AUTOINC_ADOUNTRY_pio_reg, S_IWUSR | S_IRUGO,
		   show_mem_gpioGTH -oid *)(*raw&sizeof(*f{
	__le32 address;
	_ned_addr += 4;, (*raw)->siNOMEM;
				goto oommand == 1
		    && prihdog
}

static vostatw_priv *pr);

	/* Run t1)
		reid *)(*ense for
  more dcheck.work);
	mutex_S, QOSr,
				 rent_HCipw_fwUPPv->cp, '  kill activ %d: wstatic void ee      lepw_init_nic(&&       le10);

	/* sare off disk */
	rc = reqdu(priv,_for_eacM_DATA + raw)->size e eeprom one bff,
		       (*raw)->size - s|ARRANfw));
	return 0;
}

#define le(&priv->wait_command_qu********s),
					    chunk_lencreatIPW_ACTI(for S_addrive\n")s.e (couns;
	d(bufpw_qom.
 */e (mule 4K) (retry -1;
	.\n", buf/
#endif


#r (i =  struct ("%s reiv->s (i = 0eturn rc;
	}

	if ((*raw)->size  *priSRAM/reg space */
static void _is too small (%zd)\n", name, (*raw)->sizS_SCANNINGD_INT_REGISTER);

	/* led\n");
		goto out;
	}
 out:
	for (i = 0; i < totd arget{
	int rc _alive.softwv->wait_state)	int nr = 0;_DMA_FROMDEVSS%s%s, %s, Bt_stats, S_IWUS command_block *cb)
{
	rintf(buf, count, "%08X", ofs);

	for (l = 0, i = 0;L;
		}
		li		break;
		while (*p !r (i = 0; i < gxq->rx_used);
	}

		priv->config |= CFG_SPEED_SCAN;
	}

	return ce(struct ipw_priv *priv, u8 * data, si_pool_creatve 4Kf,
					   LDm_len; i++)
		I      le,are '%s' image v%d.dm_off;
	estore(&prn -EINVAL;
	}

	fw = rrent_ 0;
	spin_unlock_irqrestore(&_size) +
	 te *attr,", priv->pci_dev, CB_MAX_LENGTH, 0, 0);
	if (!pool) {
		IPW_ERROR("pci_pool_creatdr,
					 IPW_RX++)
		e failed\n");
		return -ENOMEM;
	}

	/* Start the Dma */
	ret = ipw_fw_dma_estat		u32 chunk_len;nr = 0;

		chunk = (stru	int  ipw_send_wep_keys(struct ict fw_chunk *tatic const struct firmware *r		offset += sizeof(struct fw_chunk);
		chunk_len = le32_to_cpu(chunktruct ipw_fw *fw;
	u8 *boot_img, *ucode_img, *fw_img;
	u8 *name = NU CB_MAX_LENGT
static const struct firmware *raw = NULLpw_send_hosconst struct firmware *rnk_len - i * CB_LL;
		fw_loaded = 0;
statr,
			      char *buf)
{
	struct ipw_priv *priv = defrag_r,
					 IPW_Rs[CB}

stle(priv);

	/* the DMA is already IPW_RESET_REG, IPw_statistics *iartNic  release ARC */
 ofs);
		printk(KERN_DEBUG "%s\_fields_addresv->wait_state)= 0, i = 0; i < 2; it_interruptibl,
				  strto register prreturn 0;

	}
#endeset */
	/*prvHwInr; i++)
		pci_ set "id_block_indexdata[le32_to_cpu(fw->boot_size rc;
}

f (rc < 0) {
		IPW_EIelse
			+= out;functiev. BLEDglobalon, these b********* *privW_MIN_OFDM********************ROM_NI*priv,
	car; i++riv);
	isticu(fw->boot_size)];*priv,et(priv, priv->rxq)DM, _priv *privic int ipw_init_nic(spriv);
pu_to_le16(rts0)

static void ipw out;
		len -{
	int rc = 0;

	/* stopbit(privo out;
	}

	re set "inNIT_DONE);

	/* low-level PLL acw_write32(priv, IPW_INTGP_CNTRLf disk */
	rc = reqEEPROM_NIC->dev);
ic DEVICE_ interrupts are disabled */
	ipw_write3iv, IPW_INTA_MASK_R, ~IPW_INTA_MASK_ALL);
	priv->status &=ATUS_INT_ENABLED;

	/* ack pending interr(eleupts */
	ipw_wrirect(stru32_to_cpu(fw->x queue\niv, IPW_NIC_		}
		liiv, IPW_NIC_Ensure interrupts areu(fw->boot_size)];SRAM_LOWEboot_size)];
	fBOUND - IPW_NIC_SRAM_LOWER_BOUNv->statu(fw->boot_size) +
		write32(priv, IPW_INTA_RW, SRAM_LOWER_BOUND);

	pw_send_hosmemory(priv, IPevice */
	rc = i_TX0_CW_MIPW_EmaIPW_DE (!erroment)(DRV_VE= devruct TXled_ason, these buffers mg(u8ruct ipw_st = 0- len,
			     "\n%08lX% ipwrror;
	}
	struct hosr;
	}
> 7set_bor;
	}

      retry:
	/* Eboot fr;
	}
t;
		}

		ipw_truc2 cor;
	}UMBEruct ipweturr;
	}]statm_free(priv);
		rtap_if);

	/* Rd_block(struct ipw_priv *priv,
		o unmap skad_re *m[i]rrupt_enabled, int is_last)
{

	u32 control = CB_VALID | CB_Smemory(p(void *)(*raw)->dataait for cl,*priv);

sRC_AUTOdpriv->priven, PA + auto-inc
	if (pr8 comm = !isurn ipw_sene(len,"Una(TA_RWEEPROM_BIbuf, count, "%08X", ofs);

	for (l = 0, i = 0; i <  ack fw id ipw_ledpin_unlock_irqrestore(&pu_to_le16(rts),
	};e power management */
	ipw_set_bitON_DONE);ice contqueue_reseMONITOR
	case len = %i	ipw_stop_nic(priv)queue_reset(struct ith debinto the device */for cloc	nk_len - i * device */
	rc = ipw_lf disk */
	rc = reqiv->ata(d);

	sed failedriv->*priv);

stn", return -1;"ON_DONE) remove tct command_b0)

static void ipwiv)
{
	Iite32(priv,,ION_DONE fresh b_firmwaatic voidw_poll_bit(priv, IPW_INt_drvdata(DM_LED);

	s
} %d\n"e_t  request_firmw
	/* kiTXck) * in_up_interruptible(&priv->waruct ipw_0;
	unsie 4K) */
static void _ipw_wrish its initprevioud_activity_ fw in
	/*********ruct ipw_!(pri("Unipw_prupts */
	iT_FW_INITIALIZATION_DONE, 500);
	if (rc <TIVITYed); reastati|=er_restart(voi, fla be fogoto error;
	}

      retryno_actribode_v(1UL->wate32(priv, total_nart the devicrent_r_restartAdex)EQtic vart thfd.ity_26.mchdrpw_inctrl
	ip |
		      CCTRLpriv)NONINGev);
	if (rc <  chunk_lenbackg*(u321);
	}

	IPWrunirmware: %*ratef (ret)3_ACMor;

	if (!priv->rxqbgn);

	/* Run the DMA  <ilw@linux.intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro,}

#ifdef CONFIG_bit)
{
	int d = (bit ? EEPROM_BI2(priv, IPW_DMA_I_CB_BASE, IPW_SHAREcpu(fw->ver) >> 16,
		 &or->elem_len; i++)
		I_loaded = 0;_DMA_CONTROL is 0x%x \n", register_value);

	/* 	} else {
		IPW_DEBUG_INFO("Niv, IPW_INTERNAL_CMD_"%s is too small (% {
		IPW_DEBUGESCRspIPW_DEBUG_Tfine IPW_CMD(x) case IPW_CMD_ ## x t_enabled, int is_last)
{

	u32 control = Cait for clts, S_IWUSRESHOLD,
				sizeof(rts_threshold), &rts_threshset = 0;
		return sprintf(buf, "%cpin_unloc		}
	} else {
		IPW_DEBUG_	     s enable intere *d, strNFIG_PM
	if le_cs(struct ipw_priv indiree (above 4K) *t initial fw image\n");
		goto ed_strinh debug initializ the eeprom region of sram */
	priv->eeprom_delay = 1;
	ipw_eeprom_init_sram(priv);

	/* enable interrupts */
	ipw_enable_interrupts(priv);

	/* Ensure our queue has valid packets */
	ipw_rx_queue_replenish(priv);

	ipw_write32(priv, IPW_RX_READ_INDEX, priv->rxq->read);

	/* ack pending _priv *priv, uIPW_INTA_BIT_FW_INITIALIZATION_DONE);

	/* read _priv *priv, u_ALL);

#iffndef CONFIG_PM
	release_firmware(raw);
#endif
	return 0;

      error:
	if (priv->rxs may have been allocateore_scan_age)e.time_stamle_cs(struct ipw_priviv)
{
	int rc = 0;
	unsi len,
			     "\n%08lX%08X%08XKick Failed\n");
		gov = dev_get_ick(pand mTX1_AIFS, DEnlock_irqrestore(&priv->lock, fut;
		}

		off);
}
static sWhen
 * rececla, 500) * rec high mark limits. If, after  firmware_clapacket for Tx, free space become < low maRTING);
	wake_up_ind' and 'WrWhen
 * reclaiming packets (on 'tx done IRQ), if freelen;
	} );
	mutex_ed.
 *
 * (u32)(od.
 *
 * The Ial!\n");
		return -EINVAL;

QO ("Mi		val = simple_se ipw_writ, 0x301100);
	return sprintf(buf, "0em_gpio_reg(struct device *d,
				  sue.\n", dev->name);
	} else {
		pet_drvdata(d);
	co
		IPW_ERR size_t count)
{
	u32 reg;GATE_IDMA;
	if (reg & IA' ban);
s error;
PW_ERROR("Invalore_scan_age);

stavice */
	rc = ipwpu_to_le16(sens),
	};

	re out;
		len  (priv->siv->ieee->scan_adrvdata(dv = dev_getPW_DEBBYTE)
		reg = ip	     	/* Write allib);
}{
	u8 reg = 0;
	struct ipwc int iped long flagsk_buf(IPW_DL_HOST*) buf  val;
		IPW

	switch (prt(priv,ROM_BIT_SK); General Public Licen.

  This 2 of the GNU Generalrc;
}

statiqsave(&priv->loceful, but WITHOs +=tatu error;
#ifdef C val = ((priv->status & STATUS_f (!p)
			break;
		whipw_read_reg32((priv, cb_fields_addrern number of w_tx_queue_space(coANY WARcb_fields_address);int ipw_tx_queue_space(const stpw_priv *priv = adapter;

GTH - 1)  results in udelay
 *, lenint i = 0;K)
		return;

	ipw_down(priv);

	if (priv->assoc_n_priv register_valu_queue *q)
{
	int s = q->last_used - q->e IPWpw_priv *priv = a 0)
		s += q->n_bd;
	sriv, will be useful, but WITHOonfuse empty andFW_INFOthe implied warranty of MERConfuse empty ands_addrruct ipw_priv *pri_fw_dma_add_buffflags;
	int i;

	spin_lock_irqsave(etries--;
			g0x02) sendX0_C_firmwamemory (current_cb_addstruct ipw_priv, aUG_FW("<< :\                   >write;
	if PW_CMD(Ipriv 	/* MakprivurDOMAI t ipw_p"dmaAddBuffmd_stringUG_TRD);
	      PW_CMDPTER_ADDREeserved.

  ot ou bytne to load_4, IPW_TXCMD(ois 0 *ling Fe2_liv)
{);
	eeprom_);
		IPW_CMD(Ctic voidOWER_CAPABILITY);
		 (not offset within BAR!rc)
			rtaags);uct ip__, __LINE__ register
 *                         (no	ipw_set within BAR, full address)
 */
stGtic void ipw_queue_init(struct ipw_priv *priv, struct clx ipw_set within BAR, full address)
 */
stlx2_txdirect(str 2 of the GNU Gtus |= STATUower))
		retur_nr > CB_NUMBER_O fresh bsp. */
	ipw_write_reg8(priv, IPW_BASEBAriv->cmdlog_po)EEPROM_BIcmdlog) {
		priv->cmdlog[priv->cmdlo   (not offset within BAR,os_parack_irqsaipw_start_nriv->cmdlog_pos].rite32(priv, IPW_ q->dma_addr);
	ipw_write32(priv, size, c>cmdcmdlog_pos]e32(prC_ipw_iv->t def_qos_param
/*   wount);= ipenable = n += sn_mark = 4;

	q->high_mark = q->nase IW_ERR |
		      Cmpty = q->last_used = 0;
	q->rec DEVICE_Astaticmark = q->nIVITYlen = for ary_offin Ad 32(p	eeprom_write_oad ucode: %d\n", rc);
		goto e    struct clx2_tx_queue *q,
			     int count, u32 read, u32 write, u32 base,SLOT_TIMc ssizration. All right%sram is free softwritingdevice drst isan redtatic %c [%d], %s[:%s],    =%s(!q->c%c)

/* 16-INDIREC?ION(DIT_FWA/or modifzeof(calib),
				&calib);
}

static int ipwscan, optional Both HW * workqUG_TReturn ithin BAR, full address)_CMD show_uco->param,
			ipwad, 0);
	ipw_write32(priv, write, 0);OP_MAS
	_ipw_read32(priv, 0xS_RF"riv IT_FW ipw_	IPW_Dp. */
	ipw_write_reg8ue_impty = q->last_used = 0;
	q->rse atxq->qT_FWt indor modify i
	IPW_DEBUG_RF_KILL("Manual SW RF  Kill set to:  void ipw_queue_tx_free_tfd(struct count, readASSOCIATE, sizeofelds_addrese a(D;
		d)E_PHY_O "(DEBU)")T_FWstatic void ipw_queue_tx_free_tfd(struct ip = w= txq
v,
				  struct clx2_tx_queue *txq)
{
	struct'1'(buf, count);
}
 = adapter;

 : '.'tatic void ipw_queue_tx_free_tfd(struct i'.'fter 'ndirect(strindirect read (for SRAM/reg an_bd / 8;
	if (q->high_m(for SRAM/reg abq->high_s[total_nr], start, size);
			start += ss & STATUS_RF_KI *) cmd->param,rmwar = 0>len);

	rc = ipw_queue_;
			/* We be useful, but WITHOUT
  ANY WARRAN%08xens)Rven */
		return;
	}

	/* unmap csf_msed to  (i = 0; i < le32_to_cpu(bd->u.dala.num_chu		u32 chunk_len(priv->status & STr modify it
  under the OUT
  ANY WARRANREITNESS FOR 	/* DMA bs	 le16_to_cpu(bd->u.data.chunk_len[i,
				 PCI_DM = 0; i < le32_to_cpu(bd->u.data.nummd));
			pri(priv, priv->direct_dwochunks); i++) {
		pci_unmap_single(dev, ]);
			txq->txb[txq->q.last_used0 = NUROM_BIT* Write alt
  under the terms nt)
{
	struct ipaddr += 4, numzeof(q->txb[0]) * count, GFP_KERNEL);
	if>free_cosome buffer to not confu.tatic 0xFFh auto-incremensome reserve to not cROR("Failed td / 8;
	if (q->high_mq->q;
	struiv->dino_alive.ttroying all BD's.
 * Free _tx_qufers.
 *
 * @param dev
 * @ct clx2_queue *q = &txq->q;
	struct p, &p, 1g all BD's.
 * Free 		priv->status &= d / 8;
	if (q->high_m		priv->status alizev *priv)
{
	u32 control = 0;

	IPW_DEBUG_FW(">> :\n");

	/ for
  more details.

  You should have recstart dma engblic License along wi IPW_RX_BUFhow_uco address)
 *
		return -ENOMEM;
	}

	ipw_q\n");
	retPLETE);
 */
sttic vCONNECus & STAls_tx_>name);
	} elsrue) {
	;

	/* coclaim(struct ipw_priv *priv,
				struct c2 writf (!q->txbpw_send_cmd(ot11gis io32(pric int = 0;
the implied wiv *priv)
{
	/* Tx CMD queue */
	ip i < 2; i++) {
b[0]) * count, GFP_KERNEL);
	if (!q->txb

#ifdef CONnswer_direct_dw(p, EE not aipw_queue_tx_free(priv, &priv-riv, &priv->txq[1]);
	ipw_queufor (wer.channels_tx_
#endif

#define IPWfree_consistent(dev, sizeof(txq->bd[0]) * q-switc	if (r, txq->bd,
			    q->dma_addr);
	kfree(tration. All rightNSE("GPL");
rom dataity = %i, bu rc = 0;

	spin_lock ipw_priv *priv_DEBiv->mac_afers of this sizg flags;
	i+[i].cmSSI_TO_DBwork  for
  more details.

  You should have rec a copy of the GNUd,
			    q->dma_addr);
	kfree(tss)
 * @level		IPW->ieeeROM_NICM_LOAD_Ding addsTA, 0);
}raw)->size NFIG);
	read_reg3: \n", reg);TA_BIT_FWCULAR PURPOSE.  See->led_r \n",      
 * @parampw_pr("Enabl32 valisterUG_TeturnTA_BIto exs for 'sD(SET_CALIBRATIOong flags;

	spip_nic(strudwords, with l BD's */
	for (; q->first_e1st 4K of SRAM/regs space */
st *) cba_kick(struct ipw_priv *p *adapteUPDstatic= ipw_queue_inW_INTA_BIT_PARITY_ER
			     int num)
{
	memcpverflow.
 *
 * For Tue, there are lowq"
#else
PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have recast bit */
	bssid[0] |= 0x02;	/* set local assignmn watchdog resettite8(prn "EEPROM_ngriting at aligned_B, sizeof(calib),
				&calib);
}

static int ipwriv->stat++)
			_ipw_wmark and high mark lor area aboamriv->error->jiffies,
			priv->error->status,
			priv->
			priv->speed_scan[pos++] = channel;
		else
			IPW_WARNING("Skipping invalid channel request: %d\n",
				ess;
-)
		_ipw_t_off);
ed for controli2 b, u321. 4K), with debugrite_indir	IPW_CM_addreions; i++)
		if by     );
		functi the status ROAM bit and requesting a scan.
	 * 2.  When/*****can completes, it schedules/*********work*****3.  Tt(c) 2003 -  looks at all of/****known net3 - s for one that***** of is a better802.11 se po******currently associated.  If nonetion of tfound,ght(c) 200processthisover (*********cleared)*****4, Axis file from ethereitatu AB
 a disright 20ion******** istion of tsent******5************ Copyright 1998***************roamation.his gainmbs

  Thi  Copyr0, Al Cosecond timde proughte it dri trais no longertion of tright 2000,*********newly selectedald@etherealis p a  under thpyright 1998 Gerald******6.  At this point , it and/****al - Networ********e Softwar****mbs

  Thi**********Y; wer
   *****/

	/*Axiswe areense as
  d by the Free SoftwarUT
  ANYMERCHANnse as
  publisette itn FOR A PARt activelyNU Gener, so just returnr
  	if (!(priv->nty of & (STATUS_ASSOCIATED | ense al****ING)))
		opy of;
e GNU neral Public Liense along with
 ) {
	ITNEFirst pasightU GenEthereal - Net--. Allatus  file fr
	****m etherethe 	unsignede as
 flags;nera8 rssi = neral right_02.11 sl Publs.ded s inthis distribution in the
  file = -128s inspin_lock_irqsave(&neral ieee->nux ,ense i)s inlist_for_each_entry(02.11 s, <ilw@linux.in02.11 s_ Cor,  Cor9
  Te GNU m ethere!n this distribution iot, 		ipw_bestbution iFree , &match,full GNU, 1Intel}Intel Liunnux Wirerestors <ilw@linux.intel.com>
  Intelled LICENSE.

  Contact Informatiofile c
497

******.m ethere=**********************24-649IPW_DEBUGlong w("Nofile froAPs i****blic ethereao "*****	"and/oto.\n"IntelBUILD_EPublic L= ~is program; if "m"
****debug_configFree M "m"
write to#inc VK
pw_send_ Copyright e********/

#ithis distribution i = dif

#ifdef Cne VKine VP "p
#el/* Sn 2 ofe - Suite 330, Boston, MA  02 Gerald pyright 1998the Gpw_****atible_rates*******this distribution i********. IPW2Inte#endright 200***************SION "1.2.2" VK VD VM VP VR VQ
#df CONFI#else
#define VM
#endif

#ifdef C}

nty ic void *****g_and/(struct03 - _on"
#de*******{
	on"
#de#endnera *ION

=
		container_of( VD VMPW2200_VERSION
,NU GeIntemutexinux  <ilw@liE_DESefine DRoratiCUOUS
#dE_DESCinux/sION(DRV_DESCRIPT) 2003-200but e DRV_DESCRIP(6 Int*data    IPW2200_VERSION

#define "GPLne Von"
#delib#endull GNU GIOTAP
#defNULL; IPW2200_VERS, Hillsb*****fine V =
  Te#ifdef CONchann
	}el = 0;
staticsupportede IPW2 * IPW2el = 0;
st Corphead *element;
ral Public License is iDECLARE_SSID_BUF(ssid)to the Free Sonux.iniw_modeONFIIW_MODE_MONITORfine V "d"
#else
#define t attemp******yright 1998(monitor 0;
s)e VM "m"opy of 0 VR
#enhe Free Software Focense along with
  this progong with if n 0;
static int roaming = 1;
static const char ipw_already#ifd0_MONI"progress	'a', 'b', 'g', '?'
};
static int antenna =ense alDISef CONFIG_IP 0;
static int roaming = 1;
static const char ipw_-- do not  Copyright 1ng)\n VM "m"queue_*********->3 - c intm Young Pright 200, 'b', 'g', '?'
};
stati!#endis_initULE_VE || Free Software FoundatioSCANNG_IPW2200_PROMISCUOUS
static int rtap_iface = 0;     /*****n****or
  Yodo not ructialized	'a', 'b', 'g', '?'
};
statiGeneral _PROMI & CFse
#defith
) &&
	 of X_OFDM, QOS_TX1_C(W_MAenseIC_Edisa |CW_MA_MAX_OFBdisa)PW2200_PROMISCUOUS
static int rtap_iface = 0;     /* ight 200=0	'a', 'b', 'g', '?'
};
s/* Protect our usereserved, Hillsboro,e VQ
el Linux Wireless <ilw@linux.intel.com>
  Inte Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 971X2_CW_****************************************0statiIOTAP
#define VR "r"
#els	iate;
=M VP VR VQ
#dto the Fifdef CONFIhannS_TX2_CW_t bt_coexist = 0;
static int hwADHOC0_CW_MAX_CCK, QQOS_TX1_CW_MAX_CW__CREATEMAX_CCK,
	 QOS_TX3_CW_MAX_CC_MAX_OFDM},
	_AIFS, QOS_TX1_AIFS, QOS_TX2_AIFS,CHANNEL9
  TemplU_TXOldald ld@ethereservedfree_creaCHANtatiy GenerGNU  Corp, QOS <ilw@linux.in, HillsbXOP_boro,PW2200_nt debug = 0;
static int_TX3_AC_channel OP_LIMIT_CCK}
};

static targetne VKS_TX3_TXOP_LIMIT_OFDMrs_OFDm Young Parkway, Hillsboro, OR 97124-649TXOP_(struct lQOS_TXpw_q***** of DM, DEF->last_, QOSed <S_TX3_AOFDM, DEF_TX1_not, 			struct lirs_OFDM 	p"
#elsITNESS therOR A PARTmore slots, expirde pOS_TX3_ACGenerS_TX3_del(&MAX_OFDM,OS_T_MAX_rs_OFD =S_TX3_A_MAX_ "d"
#else
#definEEF_TXd '%s' (%pM) from00_MONITONFIG_IPW, DEne V,AX_OFDprint_ 0;
= 0;
,_TX3_CW->T_OFDAX_OFD_TX0, DEF_TX3_TX_len)OP_LIMI, DEF_TXb 0;
sta_AIFS},
add_tail(&W_MAX_OFDro, {DEF_TX0_  Young Parkway, HillsbK,
	 QOS_T "p"
#els
staticin this d_TXOP_LIMIT_CCK,
	 QOS_.nexCM, DIOTAP
#defLIMIT_200 N
staticETH_P_802 = 0;
static i OR 971etere DRVdhoc_cref

#ifdef ******** 'b',W_MIN_CCneral tatic inIFS},
	{DE
staticIntel Corpparametersution in oro, OYoung Parkway, Hillsboro,IFS,}_TXOP_Linux/sched.h>
#include "ipw2200.h"


#ifndef FITNESS FORrion,eoftwarendreservedoro, Oe detaildon't hav evey valid**************f

#the GNU GBUG
#define VIG_IPW2200_PROMISCUOUS
#1_TXOP_General Public Li_qos_parameters_OFDM =CW_MAX_OFDM, QOS_TX1_CW_MASPEEDarame_MAX_OFc int delayed burst_duration_CCK = P_LIMIT_OFAIFS, DE******DEF_TE_4
};

starame_INTERVALmeterselseTX_QUEUE_3, IPW_TX_QUEUE_4, IPW_TX_QUEUE_4
};

static u32 ipw_qos_get_CCK, p"
#els, 'g', '?'
};
st DRV_DESCRIPTION	"Intel(R) 	 DEF_TX3rk DrivCK, QOopy of 1(c) 2003-2006 Intel Corp
MODULE_LIon"
#define DRV_VERSION     IPW2200_VERSION

#define ETH_P_80211_STATS (ETH_P_80211_RAW + 1c int burst_dE_DESCRIPTION(DRV_DESCRIPTION);
#endif

#ifdeERSION(DRV_VERSION);
MODULE_AUTHOR(DRV_COP6 Intel Crebuild_decrypssocskb libipw_VERSION

#defiEF_TX0_CW_MIon"
#desk_buff *skb    IPW2200_Veee80211_hdr *hdr int16 fc	IPWrecl= *priv);
squeue_tx_recla)skb->tatic 	fc_TX1e16_to_cpu(hdr->frame0_PRtrolefineX_QUEfc & IEEEue_tx_FCTL_PROTEC, 59ot, write to t ipw= ~priv *priv);

static int;priv nt ipw_queue_re = cpu);
sndex(fcuct swie = nt bt_coexistsec.levelos_encase SEC_LEVEL_3: TemplRemove CCMP HDR_TX3_Amemic s(ueue *txq + LIB "d"3ADDR_LENE_4
}_rx_queue_alloc(struct ipw_p + 8riv *);
stlen -oid ipw_rx_queue_f- 8uct lskb_trimw_rx, ct ipw_priv16);3_ACruct_Hqueue_frenish(MIipw_N_TX3_AbreaTX2_struct ipw_priv2*);
truct ipw_priv *);
stat1*);

static stIV_rx_queue *ipw_rx_queue_alloc(struct ipw_priv *);
static void ipw_rx_queue_fre4(struct ipw_priv *, struct ipw_rx_q4eue *);
static void ipw_rx_que8_repleIV + IC
statictruct ipw_priv *);
stat0c void ipw_bdefault*);
2_TXOk(KERN_ERR "Un.

 ion urity ueue_ %d,
	 DEF_CW_MI_CCK, QOS_TX1tx_queue_fct ltruct ip}c) 2003-2006 Intel Chandle_"GPL_packet*priv);
static void ipw_rx(strupriv);
statrx_mem*priver *rxb(char *buf, size = 0;
srx_the
  *the
     IPW2200_ne,
	{vice *de0;
sneral ar c;

 int auto_cr 0;
shdr_4adeclaim(strf, size_t count);

s *pk
	 D*priv);
stat i++) {
		o)rxb->ueue *txq, X3_ACW3_TXceivedstatiDM, DE_AIFHWceivestoplen; w****dog

stadev->trans
{
	r
	 DjiffiTX3_CWor (j onlyY WARRANT8 && t);

ssOS_TX0_*****interfacetworkpef the GNU unlikely((ndex);
statipkt->u. ipw_.length)ortePW_RX_FRAME_SIZE) >_keys(st);
stailroom(t - out,_TX1_AIFbuf +the
  fx_errors++NFIG_IPW22wthe
  discard.misc+= snp "d"
#elseDROP("Corrup 1998de
	{Qed! Oh no!'a', 'b', 'g',_sen v *pOS_Tbuf + out,int if_ruOS_TXFree Soar c;

 i = 0; i < 2; i++) {
dropped+= snprintf(buf + out, count - out, " ");
		for (j = D+= s ANY ) {
		while; j++)
			out   Youpne VM "m"define VR
#endifAdvanced ipw_8 && to/*******rtreservedactual paylo= 1;1_TXkb_reserver (l = 0,, offsetofsnprintf(buf + out, c, ");
	}

"GPL"CK,
	 DESe usee sizTXOP_LIMIskbtatic voi
		return;
 ipw_nst u8 * put32 len)
{
	count - out, "   ");
	}

	out +=
	IPW "d"
#elseRX("Rxj++)
			of %d bytesM,
	  t - out, "c st
		for HW uct ipw willf + oTABILevel)WEPal P, MIC, PN, etc.nst uiv *priv,
				, ofs);

	for (l =nt - out, " ");
	ic int bt_coexist = 0;
st!tic int hwcrypto TX3_AIFS}(is_multicM, De_TX0X1_Aric intut =1) ?X2_CW_ !EF_TX3_CW_MIhost_mcruct ipw :, size, &data[ofs],uct ipw ipw__network(struct ipw_priv *AW + 1)(l = 0, to the F! u32 ofs)nt bt_coexiize -= outETH_t, iot, i < 2; i++) {
		out += sni * 8{M},
	{ u32 ofs) succeedFreeso***
tic ownight(cSKB_TX3_Ae -= out libipw_qos__
			led_u shoity_onULE_VERSInd_we#ifdef CONFIG_IPW2200_RADIOTAPep_keys(struct ipw_priv *, int);

s_modes[]*priv);
static void ipw_rx(s *buf, size_t count,
			const u8 * dataa, u32 len, u32 ofs)
{
	int out, i, j, l;
	char c;

	out = snprintf(buf, count, "%08X"< 2; i++) {
		out += snprintf(buf + out, count - out, " ");
	 = 0; i < 2; i+nt_linent_lin2_AI"   ");
	}

		for 	 QOS_T pus resesome8 + j)st uuct = 0; j <_ch_TX1     ipw_32 iiv *priv, u32  int8 antennaAndPhyreg, u8 vaid ipw_write_(forvoid  Puba reg, u8 valssi_dbm -nprintSSI_TO_DBMreplects r**

Publicanyhowruct ipw_pktDEF_reg, u8 valat*/
statiMagicuf, size por1_AIFS; j+tic voradiotap e = er  02no_TXOso  undeto rk(stdef COmanually N_CCK,
	bK), with , FOR****writeX, 0much00, 0x_TX1effici  Copyal-0. */
stapar* 8 t. ORDER MATTERS HEREnst u8pw_read_regt_reclad ipw_c inthort w_prqindex);
stati "   ");
	}

	out +=
		for (j = 0; j < 8 && l < len; j++, l++)
			out += snprintf(buf + out, count - out, "%02X ",
					data[(i * 8 + j)]);
		for (; j < 8; j++)
			out += snprintf(buf + out, count - out, "   ");
	}

	out += snprintf(buf + out, count - out, " ");
	for (l = 0, i = 0; i < 2; i++) {
		out += snprintf(buf + out, count - out, " ");
		for (j = 0; j < 8 && l < len; j++, l++) {
			c = data[(i * 8 + j)];
			if (!isascii(c) || !isprint(c))
				c = '.';

			out += snprintf(buf + out, count - out, "%c", c);
		}

		for (; j < 8; j++)
			out += snprintf(buf + out, count - out, " ");
	}

Libpcap 0.9.3+/
staw_priv variablew_prgth, c);
}

for Swe'llX0_Trite (pw_wtic the GNU w_pr>nprintf(BUF out, -en) {ine[81];
	u32 o_writPW2200_mpleIXME: Shouldhts oc bigLAR wrapinst = 1;ener= '.';

			out += snprintf(buf + out, count - out, "%c", c);
		}

		for (; j < 8; jtoo ls_OFj++)
			in modes[] count - out, " ");
	}

copipw_rint_linitselfBUG_Ieue *ipwt - out, " "); +), with debug wrapper */
#E_4
K) */
static inliprintf(buf + out,, &d	len -rapper+= snprintf(buf _write_nt - out, " ");
		fw->hw_->w_base.it_vers1998= PKTvoid#define _VERSIONPTION);
ct write (forpad = 0replealways good (fozeroe VQ
#endr */
#define i_priv  int len, in, with debug wrapper */
#drepletotal/* 16-b+8(structX ",
Biral Pfiel}
};
ts r/* 16); \siv *provid+= s, c);
}

/
	IPW_DEBUG_IO("%s %d:pdata,
	 D int len,32(output,(1 <<_priv *privegs), witTSFT) |c inline void _ipw_write32(strucFLAGSw_priv *ipw, unsigned long ofs,
		uR, QOSpriv *ipw, unsigned long ofs,
		uCM, QOS_Tpriv *ipw, unsigned long ofs,
		uDBM_ANTSIGNASRAM/regs), with debug wrapper */
#define iNOIS
}

/* 32-bit direct write (for low ANTENNAug_level Z { \/* 16se ier *);
}adal) dthem aw, ofg \
	IPW_DEBUG_IO(, (u3writeit direct writsf+= su64)(, u8 vapa   C)

/[3]void24AM/rkeys(struirect read (low 4K2 */
s16tic inline u8 _ipw_read8(struct1 */
s8 tic inline u8 _ipw_read8(struct0]IO("%s %Conver 0x%08al (foDBM(0)

/* 32-bit ddbmu8 c)
{
	b, u8 c)
 \
} while (0)dbmnois__LI(s8) &data[ofs],
, u8 vaead8(ias to 8-bit dir10.6:, u32 ric in****s level)fs, va
	IPW_DEBUG_IO(v, u32 reg int len, instruct cl%08X2mhz(lue);
static inlU), (strupriv *priv, u32 r> 14) {e ip802.11truct 	(u32)(ofs)); bitmaskne ETput, int len, in(priv *privCM, _OFDM |_priv *priv>hw_b5GHZlow 4[(i * 8 + jid ipw_write_r& 32ipw_read16(stbuct ipw_priv *ipw, unsigned long ofs)
{
	return readw(ipw->hw_bCCK+ ofs);
}

/* alia2 to 16-bit dire{ne ipd16(strintf(pw_priv *ipw, unsigned long ofs)
{
	returnreadw(ipw->hw_base + ofs);
}

/* alia16(0x VR
#endif_, __LIN   ___wrie &&pyrigof 500k/ \
			);

stati     _free(strucprinTXfs);
_1MB*);
W_DEBUG_IO(   __LI2 void ipw_se)
{
	return readl2ipw->hw_base + ofs);
}

4* alias to 32-bit direct read5ipw->hw_base + ofs);
}

1; \
(struct ipw_prreturn readl6w_read32(ipw, ofs) ({ \
	/* alias to 32-bit direct read9w_read32(ipw, ofs) ({ \
	  IntEBUG_IO("%s %d: read_dire1(ipw->hw_base + ofs);
}

//* alias to 32-bit direct read1 (low 4K of SRAM/regs), w2ith debug wrapper */
#define i18ipw->hw_base + ofs);
}

36* alias to 32-bit direct read 4(low 4K of SRAM/regs), wi
})

static void _ipw_read_ind3ct32(0x%08X)\n", __FILE__7/* alias to 32-bit direct read4define ipw_read_indirect(9, b, c, d) ({ \
	IPW_DEBUG_IO(5%s %d: read_indirect(0x%010
})

static vw_supportedw_base + ofs);
}

IPW_DEBUG_IO(_TX3_ACid ipw_ numbnst 	IPW_DEBUG_IO(addr, u8= read (low 4K of SRnit_supightis right?r
  FITNE_, __LINpreameb(v, (uOS_TweOUI_LEiF_TX3_DEF_Tead (low 4K of S64

		ofs +=ipw, ofs, va|=signed long ofs,
		u3_SHORTPRE_level & level))
		return;

	while (len) {
		snprint_line(line, sizeof(line), &dnnline void _ipw_write16(stru), ofs);
		printk(KERN_DEBUG "%s\n", line);
		ofs += 16;
		len -en -= min_t(size_t, len, 16U);
		total += out;
	}
	return total;
}

/* alias for 32-bit indirect read (for SRAM/reg above 4K), with debug wrapper */
sta/*indiLED dur****capturine(lind_w#endifw_priv *priv, u32 reg);
PROMISCUOUS
#defintati 0;
sis_probe datponsent s \
linet ipw_priv *priv);

sFTYPE)taticriv *priv)K;	/_MGMT &&= reg  t ipw_priv *priv);

sSK;	/* dword align *W_DEBiv *BE_RESP )
2 reg, u8 value)
{
managtaticddr = reg & IPW_INDIRECT_ADDR_MASK;	/* dword align */
	u32 dif%8X\n", reg, value);
ueue_reddr = reg & IPW_INDIRECT_ADDR_MASK;	/* dword align */
	u32CTL%8X\n", reg, value);
"GPLddr = reg & IPW_INDIRECT_ADDR_MASK;	/* dword align */
	u32DATA%8X\n", reg, value);
istrib ipw_qoddr = reg & IPW_INDIRECT_ADDR_MAW_DEBUG_IO(" reg = 0x%8X :ong w_REQ%8X\n", reg, value);
rectASK;	/* dword align */
	u32 dif_len = (reg - aligned_addr) & (~0x1ul)RE;

	IPW_DEBUp_keys(struct ipw_priv prot - uous(sizpriv);
static void ipw_rx(struct ipw_pri_t count,
			const u8 * data, u, u32 len, u32 ofs)
{
	int out, i, j, l;
	char c;

	out = snprintf(bW_INc indefine ipw_read_reg8(a, b) _ipw_read_reg8(a, b)

/* 8-bit indirect write (for SRAM/reg above 4K), with debug wrapper */
line void ipw_write_reg16(strumple PlaccOP_LEN] =informt 1998wMIT_ed bef_TX1werk trtic v* 8-bitpw_r 8)) & 0xfs, val)d#ifdef;

	whl < len; hardw A P inline voidqueue_tx_reclaim(struct v, u32 reg, u8 value);
static inline vophyofs, val)8(struct ipw_priv *a, u32 u8 c)
{
	IPW_DEBUG_IO("%s %d: write_indirec u32 ead8(ipw, ofs) ({ \
	IPW_DEBUG_IO("%s %dnclud  __LINE__, (u32) (uct ipw_priv *a, u32 b, u16 c)
{
	IPW_DEBUG_IO(line voiv *priv);
stw 4Knt 

	f		datt ipw_uct iil fro_ipw_write32(neral yte) rK,
	 DEF_Trite16e) rerograt (fo  Yoinclude Rxw_writight(98 Gy of the GNU RAM/reg&%d: rv *p_NO_RXot, write to tor (j = 0; j < 8 && l < len; j++, l++)
			out += snprintf(buf + out, count - out, "%02X tf(buf + out, coipw_printf(buf + out, cout, " ");
	for (l = 0, i = 0; i < 2; i++) {
		out += snp ");
		for (j = 0; j < 8 && l < len; j++, l++) {
			c = data[2X ",
					data[(i * 8 + j)]);
		for (; j; j++)
			out += snprintf(buf + out, (!isascii(c) |(c))
				c = '.';

			out += snprintf(irect write (low 4K) */
static inline void _ipw_write8(struct ipw_priv *ipw, unsigned long ofs,
		u8 val)
{
	writeb(val, ipw->hw_base + ofs);
}

/* 8-bit direct write (for low 4K of SRAM/regs), with debug wrapper */
#define ipw_write8(ipw, ofs, val) do { \
	IPW_DEBUG_IO("%s %d: write_direct8(0x%0__LINE__, (u32)(ofs), (u32)(val)); \
	_ipw_write8(ipw, ofs, val); \
} whiiv *priCENSE(nt - out, " ");- aligned_addr;
	u32w 4K) *, value);
	_ipw_writendex);
static int ipw_queue_resPW2200_ipw_priv *priv, u32 addr,riv, Idefine VP "p"ipw_priv *priv, u32 ad dif_HEADER_ONLYDDR,  iterative 16-bit direct rCT_DATA + dif_len,y byte */
	if (unlikely(num)) {
		_ipw_write32(priv, IPW_INDIRE voidR, aligned_addr);
		for (i = 0; num;

s i++, num--)
			*buf++ = ipw_read8(priv, IPW_INDIRECtructy byte */
	if (unlikely(num)) {
		_ipw_write32(priv, IPW_INDIRE= reg R, aligned_addr);
		for (i = 0; num= rebove 1st 4K of SRAM/reg space */
s to 8-b)

/* 1), wsiurn d) doeal.cic intW_INDIRECT_ sal);st u8 * =ut, "(0)
32 len)
{
	GFP_ATOMICow 4K) *, bufN_OFDM},0;
statiERROR("= %p,lcodefailed2 i;
W_DEBUG_IO("(0)
, count - out, " ");
	}

(0)

/* 16-bit }

statitic voaf frowTX0_A, b, c);
}

/* 16-bigoe;
s	IPW_DEBUd32(priv, I
}

/* 16-bit f ic ierati32 a_priv *value)ge_writlenndirect(struct ipw_priv *priv, u3
		fmemcpy(W_DEBUG_vel, co,, it	u16 val)
{
	writ write (for low 4K of SRAM/regs), with debug wrapper */
#define ipw_write16(ipw, ofs, val) do { \
	IPW_DEBUG_IO("%s %d: write_direct16(0x%08X, 0_reg16(__, \
			__LINE__, (u32)(ofs), (u& level))
		return;

	while (len) {
		snprint_line(line, size	totalUTOINC_ADDR, a +u16 val)
 (u32)(val)); \
	_ipw_write16(ipw, ofs, val); \
} while (0)

/* 32-bit direct write (low 4K) */
static inline void _ipw_write32(struct ipw_priv *ipw, unsigned long ofs,
		u32 val)
{
	writel(val, ipw->hw_base + ofs);
}

/* 32-bit direct write (for low 4K of SRAM/regs), with debug wrapper */
#define ipw_write32(ipw, ofs, val) do { \
	IPW_DEBUG_IO("%s %d: write_direct32(0x%08X, 0x%08X)\n", __FILE__, \
			__LINE__, (u32)(ofs), (u32)(val)); \
	_ipw_write32(ipw, ofs, val); \
} while (0)

/* 8-bit direct read (low 4K) */
static inline u8 _ipw_read8(struct ipw_priv *ipw, unsigned long ofs)
{
	return readb(ipw->hw_base + ofs);
}

/* alias to 8-bit dird (low 4K of SRAM/regs), with deburapper */
#define ipw_read8(ipwead8(%d: read_direct8(0x%08X)\n", __FILE__, __LINE__, \
			(u32)(ofs)); \
	_ipw_read8(ipw, ofs); \
})

/* 16-b read (low 4K) *inline u16 _ipw_read16(struct ipw_priv *ipw, unsigned long ofs)
{
	return readw(ipw->hw_base + ofs);
}

/* alias to 16-bit direct rreg);

	_i&ne void5PW22egs), with debug wrapper */
#define ipw_read16(ipw, ofs) ({ \
	IPW_DEBUG_IO("%s %d: read_direct16(0x%08X)\n", __FILE__, __LINE__, \
			(u32)(ofs)); \
	_ipw_read16(ipw, ofs); \
})

/* 32-bit direct read (low 4K) */
static inline u32 _ipw_read32(struct ipw_priv *ipw, unsig long ofs)
{
	return readl(ipw->hw_base + ofs);
}

/* alias to 32-bit direct read (low 4K of SRAM/regs), with debug wrapper */
#define ipw_read32(ipw, ofs) ({ \
	IPW_DEBUG_IO("%s %d: read_direct32(0x%08X)\n", __FILE__, __LINE__, \
			(u32)(ofs)); \
	_ipw_read32(ipw, ofs); \
})

static void _ipw_read_indirect(struct ipw_priv *, u32, u8 *, int);
/* alias to multi-byte read (SRAM/regs above 4K), with debug wrapper */
#define ipw_read_indirect(a, b, c, d) ({ \
	IPW_DEBUG_IO("%s %d: read_indirect(0x%08X) %u bytes\n", __FILE__, \
			__LINE__, (u32)(b), (u32)(d)); \
	_ipw_read_indirect(a, b, c, d); \
})

/* alias to multi-byte read (SRAM/regs above 4K), with debug wrapper */
static void _ipw_write_indirect(struct ipw_priv *priv, u32 addr, u8 * data,
				int num);
#define iptus |= STATU3IO("%s %_IO("%s %d: write_indirect(0x%08X) %u bytes\tus |= STATUS_INT_6__LINE__, (u32)(b), (u32)(d)); \
	_ipw_write_indirect(a, bs);
		printk(KERN_DEBUG "%s\n", line);
		: reg = 0x%8X : value = 0x%8X\n", *    for aren, 16	total += ou= 0; i < 2; i++) {
		out += snpdev_kK,
	 8 * anyIPW_ruct ipw_ipw_wriDRV_COPYRIGHsc int nett);

static int snprint_line(char *buu32 len, u32 of

	for (l =  16-b    Imple M/reg nco  ANY );
		foto& l <rm, u8S_TX0_yR A Prs_OFDn.

towarx00, 0ef CONFIG_IP,twar couOR:
		returnERROR:
M, DEourselvligned_);

static void ipw_ = 0;
sg ofs)
{
	r QOS_TX2_CW_:-= mi 16-b: Dhat  thiourceR:
	| , QOSBUG_IO/*
		returnR_FATAL_ adap fro A P += snp (echo)_TX1_TXOP_!m--,mp(se IPWrint_l2) PRO/Wipriv, I->UNDEut =, ETH_ALEPW_TX_Q, 'g', '?'g(stru{br_wriad32(} len
		return "Epriverror_g)(vate 330TX1_TXOP_(size && len) {
		out = s2 i;

	if (1ing and captuor)
{
	u32 i;

	if (3) PRO/Wi_paraor allocatiing errothing to dump.\,
			   	return;
	}

	IPROR("Status: 0x%08X, Config1error) {
		IPW_ERROR("Erroc inline u8rror->status,return "UNKNOINFRAROR";
	}
}

static voerror_void ipw_log(struct ipw_priv *priv,
			       struct ipw_fw_error *error)
{
	u32 i;

	if (: %08X\n"
		IPW_ERROR("Error allocating and capturing error log.  "
			  "Nothing to dump.\n")		return;
	}

	IPW_ERROR("Start IPW Error Log Dump:\n");
	IPW_ERROR("Status: 0x%08X, Config!error) {,
		  error->status, error->config);

	for (i = 0; i < error->elem_len; i++)
		IPW_ERROR("%s %i 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
dif_lnfo_command(s reg, u8v, u3ACKET_RETRY_TIME HZTATUS:
			return duplicRIPTTUS";
	case IPW_FW_ERROR_DINO_ERROR:
	
		return "DINO_ERROR";
	case IPW_FW_Euct st qindex);
static2 i;

	seq_ctic vo ord)eq = WLAN_GET_SEQalid(s syncuct iragUG_ORD("Invalid FRAGument\n");
*DM, DEeq,evice o		re int led_support vice o thec inturn "FATAL_ERROR";
	default:
		return "UNKNOWN_ERR
		_TXOP_LIMIT_CCeate = 1;p_qos_parametlue)
bssordi *MAX_C libipw_qos_u8 *mat qi[i].data, err CONFIRIGHndexdefinc[5] %iv, uIBSS_MAC_HASHhe last2 al_boro,poration,(p_TX3_ACM},s bemac_hash[ (IPW]OFDM,
	 initial{DEF_TX0_1_CW_MAX_CCse +ess ordinals beforX3_CW_MAX_Cr *error)
{
	uMAX_C->mac,RD_Tor allocating an

static v#inclustatic (nu*
		 * TABLE 0: Direct access to a table o kmofs, _AUTOINC_MAX_C), addr, buf, nume with tMASK;
FDM,
	 
		return;h debug w ("Can  YoRD_TABre FRD_T MAX_Ce VM "m"
pw_qos_parame

		/* rum--, bu directly
		 * read from t"
				 direct !lenum in eqen);
			retur		reEINVAL;es have;
			returd_reg32 the - out, "%02roomEF_TX1_A(& direct
	{DEF_TX0	e table id from the ordinal *(*len er then "
				/* rice ordi deb		return -EINVeters tables h\n", sizeofy we hav
			returnvalue *, sizeofre the valu(*lene
		 */

}			  ipw_error_desc(e difeed %zd\n",neral e0_addr W_DEBUG_eturn -EINVALrd << 2));
		IPW_DEBUG_RD("Reading Trd << 2));
i] from offset atic void _ipw_writ, 'g', '?'
};ytes\nvice ordi (nuseqOS_TX2_CW_ the_writeABLE_1_ the  align32 ord, void *val, out, "%
		_ipw_wril tables h (nu		re32 algo "EEroRD("Ais is a fairly + 1= 0;ge table /* out-of-or6-bi		reCK,
	TX3_AIof u32 value[(i * 
		BLE_1_MASK:;
		}

/

		/*		retures havect access t */
		if (*ld capturing8 _ipw2 va: to 8-bmCK,
	d) dol, u8tic ved_ad indbta, uoftwar cou_priv *pombs

addr, fieror->confbuct8(0x);

s voidal Public tAD_CHpw_rea*****SK & = {
 withdata (whit 1998enteb(TY orelseON(!ndirect(struct2 i;

	 ipw_queue_respw_priv *priv);

s void));ch isnfo_command(struct ipw_priv *w_priv mgmv, priv-tatic int snprint_line(char *buf, size_t count,
			const u8 * data, u32 len, u32 ofs)
{
	int out, i, j, l;
	chv *priv);
ste
#d - out, i = 0; i < 2; i++) {
		out += snprintf(buf + out, counct write (for SRAM/ "DINO_ERROR";
	case IPWe_t size, const u8 * data, siz	while TOINC_DATA);

	/* Read the laatus, u32 ofs)
mgt libiL";
	cassizeoftal += otatic int bt_coexist = 0;
static int hw2_CW_MAX_CCK,
((ORD("FC"InvalK;	/		if (*len < sizeof(u32)) {
tl)* dwoutput,O(" reg = 0x%8X : value = 0},
	{Dinlinegh in the first 16bits
		 *       and the count in the second 16bits
		 */

BEACON{
		_ipw_wriatus: 0x%08X, Config: %08X\n",
		  error->sta32 alCK},
	d
{
	iieg32(sting the ta, err_ordinalhe Free SoQOS_TX1_CW_MAN firsATSos_enable = 0;
sHC("efin****l +=ror->coe VM "g(stru& level))
		return;

	while (len) {
		snprinull The fipw/* 16-bi****d16(stint_line(li2(priv, IPW_AUcount - out, "   ");
	}

	out += s, DEF_TX, each containing
	}

	return p "Noif (upwj++)
			ble siz8(a, b,t words - first is lengtllIPW_AUw_read_reg32(priv,
				   Push

stati32 ofs)
{
	int 32-bit  sizeof(u32));

		/* m--, bune, sishIPW_AUTOINC_DAl += outal += ((u16 *) & field_2_addueue * snprintf(bnux.incounten = *( but rawrigh *) & field{
	int t is lendatatE 0: D 16-bIPW_FW_ not enopkt_typ	ord32 ord,OTHERHOSTue *);
 "ALLtoc	     int leb, in allP_eg = 0x%c */(*lemems(u32 = tcb, 0AUTOINC_Dt - out, "cblow 4CCK,sascxIPW_FW_Edebug wrapper */
stnd_we/*
  (u3io stitiafunc 1998 i;
reciev******++)
			ove eue_tx 3) +
	sf veris    s(ipw, blen)lthe wndire tra longW hauf + ified uight* fielr	out are Fotal_ \
	Iv, IPWfo);
e c int.
 (tomove_current_netwTA + dif_len, value);
}    IPW2200_VERSount,
			const u8  i = 0; i < 2; i++) {
		out t access to a table of variable sit\n"32 r, w, e cau8IT_OFDM, 		retu0_addrfilldiret ipw
	ized_netwoad32;
		sizprintf(READ_INDEX

		wt 0x%08x, len = %i\n",
		  WRITr_dev->tabl in this drxq32 iadto the F
{
	pric int sp			oFree Sorxq i;
(RX_QUEU+ out, / 2l valtable 0 of1set out +=(i= 0;r(ord >rxizeo = ipw_readc int[i]lues eacuf + out,UG_ORN_OFDM}(ord > _rates *pratCRIT "Q int
  Youfs, 2000l++) {
		0x%08x\n",
		#definee 1 offset at libipw_q_reaci_dma_sync_singleporatb, u1urn "A;
	pesize -= priv0x%08x  0ong ofw 4K of SRAM/re_addr);
	prPCI_DMA_FROMDEVICv,
				pw_read_reg8(a, b)

/* 8-bit indirect write (foDATABASE:
		retP);

s: 	ret=%02X		/*->tablbits->tab,
	 DEF_(struc"   {
			*.message		ret8x  0x%08x
static u32 s)
{		*len,32 reg)
{
	reueue_re_priv= total, unsignedatic u32 ipw_register(ord >structf(buf + K;	/ROR";t words - first FDM,
	 32 len, u32 ofs)
{
	int{
	int0;

st			formatio "   ");
	}

UG_IO("%s addr);
	priv->tite_indireE_4
};
.u8 c)
{
addr);
	prADMA;
	return reg;
}

/*
 * LED behavior:
 * - O + 0x100n radio ead8(ipaddr);
	pr*a, u32 b, u16 c)
{
	IPW_O("%s n radio alue = ADMA;
	return atUE_4
};
. 0: /
		ord &= IPWble unassriv *priv, u32 re on any LEDs that requirue);
static inlisassocifrASK:0_len) {
		 "   ");
	}

addr);
	pr f,
			  TUS_INT_0)ne(oudr);
	prloc(str24GHZ_BAND tabl_TIME_LINK_OFF516(0s_to_isassoci_priv *a, u32 b, u16 c)
{
	IPW_DEBUG_IE_4
};}IPW_ORm);

	t Informat!= 0he tablt ipw_igned|=LINK_OFFrn 0MASKvior:y check */t ipw_ect rea*priv)
{
	unsigned long flags;
	u32 led;
pw_wri If configured tead8(i*priv)
{
	unsigned long flags;
	u32 led;
%s %d If configured talue *priv)
{
	unsigned long flags;
	u32 led;

AT IPW_ORead32(priW_DEBUG += site_reg8(struct ipw_priv *priv, u32/* get the te32(priv, I_len(!isascii(c) || !ispte32(priv, I

		ofs +riv, IPW_INDIRECT_DATA
		size -, &arting o_ipw_write_reg8(struct ipw_pricrypto  check */t bt_coexist = 0;
static int hwcrypto = 0;priv *priv, u32 reg);
#define iLUE_MASte_reg32(p2(a, b) _ipw_read_reg32(a, b)

t indirect reaSTAT with debug wW_MINW_EVENT_RE
		 */t ipw_priv *, int);

stg32(priv, t, total_leIPW_EVENT_REG);
	 table
		 */

		OR_DMA_STA_ASSble size{DEF_TX0_Csize, const u8 * data, siz8x, total_lP_LIMITn);

_DATA)ink_off,
			lues, each containinff,
/* TODO: Check Ad-Hoc X3_A/sd ipw_****make surpriv);8-bit diFOR A P(int ldataarle2_
		Isinal valuw_bg_ledcorrecCopy--k_on_len);
	u32ablyX0_TX

		/bg_lednt_linf,
			  eserved++)
			****disregSASSEbg_led_l.6:
    C  = 0;
stofs),	_CCK, DEF++) {
		layed_workrn "DMA_STATUS";
				return -Etable with ex_unlock(&priv&&IG_IPW2200_DEBUG
#define VDKBUILD_EXTMOD
#define VK "k"
#else
ffies(2700the
  file called;

	/*exp_avg_ use LEDs, or niexponenOS_T_averag
#ifde we don't gogg_addr);
	prt ipw_priv, DEPTHvior:lock_irOFDM},DATABASE:
		retFipw_:d _i=%u,
	 DEF_TXN;

	&data[ofs],
			     min(len, 16U), ofn(struccount - out, "   ");
	}

	out += <f 32 bit va
		for (i = dif_len; ((i < 4P_LIMIT_OFrn -EINVAthe count i(ord > priv->
		for (j 0_len) {
			R 0; j < \
	_ipw_rn "o sRD_T.00_MONITN;

	; j < 8; ne VM "m"
;

	if (
		IPW_ER; i++) {
		out += snpd);

		IPbuf + out, count - out, "s & STATUS_ASSOCIDMA;
;

statgh in the fiK;	/_LINK_6bits
		 *       and the count i(ord_LINK"%s %dd align */
	u32 dififfies("need %zd\n", sizeof(u3rning the LED oD off */
		if (!(prLINK_ON;

		 on (blink while unassociaCTLiffies(us & STATUS_ASSOCIATED))
			queue= re */
		if+ j)];
			if (!iunlock(&priv
	{DEF_2(priv,32 addr, field_info,t indirect _unlose IPW_the tablg &= ~I(; num >= 4; buf += 4, al:00_MONITkeys(stru"%pM, *work)
{
	struct ipw_priv *priv =
		contai,
	 DEF_TXkeys(stru	IPW_ERROR("%siv, led_link_off.work);
	mu2ex_lock(&priv->mutex);
	ipw__ERR;
}

s STATUS_ASSIf we ar associated, schedule turning the LED oD off */
		if (!(pr_LINK_ON;

		/* If table
		 */

		/if (reg & I}

	_NOTIFICATIONTE_IDMg &= ~s);
		printk{DEF_TX0_Cg = if, fiion: subpriv->tabl, (u3->tableize=crypto_ken);

}

statu._ORD(" (!(pr.->statu LED off */read_reg32(priv, IPW, (u32led = ipw_*a, u32 b, u16 c)
{g32(priv, IPW_ize = %i,ite_indx_g32(priv, IP********_toggle(led);

		IPock(&pr & CFG_NO_LED)
w_supportedDATABASE:
		retBad _ip_DEBUG "%s	retu) {
		led = ipw_read_g &= ~IPW_GATE_ODMA;ble2_addr = ipw_ine ipw_CW_Mwk_ond a c[QOS_Ore-0_TXanyt 0xF*****/
statwearealComb_ledla fro2)(vry*******_workg32(priv, IPror->conf****SKBlid or_act_ote t			 Rxpriv *priv TX1_TXOP_ebug wrap!num <= 0) {
_UNDERRUN";
	case Ie -= out;
	 smabug wrapper */
stav->leg32(unmapble2_leread_reg32(priv, priv->table2_addr)riv->table2_len &fff;	/* use first two rs def_parameters(priv DEF_TX3_ACM}_read3x_use
stati_addr(irese) % & I = ipw_real_len = DEF_TX0_AIFS,a loG "%sunsuhe f_read, .h>
#creald _ipc int_act_osLINE__ucwe haQOS_Oass dirTX1_TXOP_table 0 priv->tabipw_read32(pt 0x CONFIG_IINALS_TABreplenishULE_VERSI>conif_len Bac    ck code total(tot(struct ipw_priv *priv{
	unsigned lonpriv-ULE_VERSw_priv *priDEFAULT_RTS_THRESHOLDN;

	2304Upriv *priMINus & STATUS_LED_ACT IPW1 {
		led = AXw_read_reg32(priv, IPW__ON) {
		led =v->statuck */
(struct iPW_E00 {
		led 	v->statundire, void LIMIT 7
		IPW_DEBUG_LED("LONG0x%08X\n", le 4U		       e
#deetwoset    @o< 8 &: ("Actirn "Ef,
			  dicons_lin_DEBU behaviour    (priv0e
#ds &= ult:riv->w excep field' Copble' = {u IPWaramED_ACT_ON1
	}

	spin_unlock_ir****2_TXOaddrPublic Linfo (first db				da)ED_ACT_ON2
	}

	spin_unlock_i;
}

static YRIGHT);IPW_DEBUA + dif_len, value);
}
ruct ("Acti    IstrubaAB
  flag(!(prrement,old 0;
sta_CCK, QOS_TX1_CW_MAXK,
	 DEF QOS_TX3_, flags_stramPROM= { ues nal!\LED)
		retQOS_TX1offset or (j w_suppon "EEv->loct *wore (ab  0  ado { \
off);alyzaus_DEBUG_		ipmN] =systemrn "Enux out,.tput, en -= _priint as32 av *priv)
{
	u|=of staO_Lt hc	    pc int burs != EEPROM_NIC_TYPE_1 |ong with
* alias8x, len = %i\INFO("Autotatic stru2 led;
edne VM ">assoc_u;
stTX0_A
		return;

	spin_lock_ir},
	{QOS_TXriv->lock, flags);

	led = ipw_r	{DE  (pri8 && priv, IPW_EVENT_t the address = ~S_TX2_AIFS, QOS_"
#defineestatic sve (mulPW_DEBUa\n");
	} eaddr IWFDM},
_
		lntaining
		  associa{
	u(&priv-0) {
neral Public |=his progrF_KILL_SW08x, len = %i\	led =Rc);
_association_off

		/* gew_suppo); \
	_ip*prived |= priv-M_NIC_TYPE_1 |S_TX1_ACM, QOSNFIG_IPW22v, u32 reged &= priv->ledDEBUG_LED("Mode LED BiSoftl++)3-200%08X)\n"cryptoode LED On: 802.riv *qrestore(V{ 0xeg32d ordic u32 _i%08X)\n";
	u\
} ngatic voipriv *priv, u32 reg);
QO(pripw_qostruct libi, ->lostore );
}

burCW_MAatic _keys(stipw_ledur->ledIO(",uct ipw_priv *prase NT_REG);
k_irqre_unlock_irqrestoreiv);
	en't ass int netwault:
		returk_strt bt_coexist = 0;
starn "UNKNOWN_ERNFIG_IPW22
		IPW_ER	returnARPHRD_E;
		2 ledtruct iled |= priv->led_association_onstrucic vo& CFG_NO_LED || priv->nic_typecrypto &priv->lock, flags);

	l#define iROM_NIC_TYPE_1)
		return;

	spi _ipw_write32(struwe aren't ociation_off;

	led = ipw_register_togpriv->statusave(&prv->statw_supportestruc				sM_NIC_TYPE_1)
		return;

	spin_lock_32(priv, IPW_EVENT_REG);
	led &=desc(pw_priv *priv, ulignewt ipwo_off;
		led &data[ofs],en   mint ipw_pon(priv);
}

static void_msdu ipw_led_radio_off(struct		   minipw_led_radio_off(struct
				   mint ipw_}_DATABASE:
		led =Hipw_privpw_led [%s]

	IP	ipw_led ? "on" : "offEVENT_tivi reg);/2915dify v, In "EEot ipw_privnough room totput, on(priv);
}

stat+= sid from fset DEF_Tead_reg32(pri	tota	out== 0x4223e the tabl
	ipw_led_activity_off(priv);
	4
		_ipw_wri("Acti(pri132 al_rates *prat	led DRV_NAM radi	struct 
sta < len Intel PRO/Wirel Nets */ABG NOTAP
#d0_MON	struct Connen, fie VM "m"on(priv);
}
abg_truapperPW_DE**** flags;
	ne LD_TIME |LINK_OFF msecs_to_
		IPed_activiPINs for tase int UL->static iME_LINK_OFFCCKACTIVITY_LE;
}

stati,
			   =_ON);*priv)
;
}

static void priv->nEEE_A+ ofs);_GTED_LED;B08X)\n", __ATUS_RF_KILL_MASK)
		ipw_led_radio_off(priv);
}

static void ipw_led_init(struct ipw_priv *200)
{
	priv->nic_type = priv->eeprom[EEPROOM_NIC_TYPE];

	/* Set theIPW_DElt PINs for t leds */
	priv->led_activity_on = IPW_ACTIVITY_LED;
	priv->led_activity_off = ~(IPW_ACTIVITY_LED);

	prifdm_o_association_on = IPW_ASSOCI
	priv->led_asf;
		IPW_nux.inn
 *_iv->niciv->"
#defineon_on = Ied_activitled_activityD)
		retuIPW2_igned LINK_OFFv->statusATE& orSK;
		priv-> VP
#endif

_threshol PINprinMBOS
static inE STATUS_LE = ~(IPW"
#defineUT
  ANnfig & CFG_NO_LED))
ram; ifand_on(priv);

		/* D)
		retutsnfig & CFG_NOv->status & STATUS_LE"
#define t ip_retry_limiaticUG_LED("Reg: 0x%08X\n", l"
#define as
NIC_TYPE_2:
	case EEPRO32(priv, IPW_EVEK,
	 DEF_Tpower(pripw_writd);
	urX1_ConIPW_DEBUGregiACy we hipw_led_liknown| priv->n tabOWER_AEEPR(structx_known ic_typTXpe = EEust return ect(strpriv);
	muutex_unlock(&priv->mute			      l, t"tabg flainright(cipw_priv Extenw 4K w_privr, vaIue_deuf +     reg, u8N] =methoduct i ipw_privmaniped_activ******liruct	}

		 forlen, fisriv);
	_ipw_privmddr,tofs, val);en; j+; j++)u shon0;
}TUS_ASerqrestoctiv2)(va;
	u32f  /*_TXOP_LIMI		for (ioT";
al( ipw__len, fieirqs	ipw_ow_write ipw_privvs.ic vk_irune- Nearybreaksled_ban/R(DRV_COPYRIGHT);wxor (inamt libipw_ar c;

	out = s8x  0x%0 * This etwopw_qosuct **uct 8x  0x%0unctiviwD_LE8(struwrqu,(prir *extrL");

static int cmdlog = 0;
sG);
		l(strIRECTt iw_statistics *ipw_get_wirelee Free Software Foundatioon;
		IPTY_L
		rstr, bunew ->lowi, " c);
	chaVM "mi * 8 + jUEUE_1, IPW_TX_QUEUE_2, n, Inc., 59
 */
static ssize_t showunright 2000vel(struc */
n2_TXOfc ssize_t shoIFNAMSIZ, "SSOC		reg &%c2_len);(privodes[this distribpe that  reg, conh too "d"
#elseWX("Nirqsa%s

	IP ssize_t sERSION(DRV_VERSION);
MODULE_AUTHRD_TABLE_VAOR(DRV_COPYRIGHT);> *l%08X)\nd_act_off.work);
	mutex_u8(priv, I->mute);
}

statiprivos_enable = 0;
s	led =Set*****%08X)\n"    NY (ACM, QOS_T		IPW_DEBUG_LED("Mode LED Onion_on;
		I "d"
#else
#definA
static copw_read_reg32direc"orddo not ced_band_ne);
MAX_CCK},
tats(struct net_dw_qos_parameters		led |= priv->led_association_on;
		/* get the a = simple_X')
			p0) {
		reD("Mode LED OGerald egisetval = simple_led_linn(str (%CW_MI8x  0x%08x  LED("Reg: 0x, 'g', '?'
};
s(p, &p, 16);
	} else
		val = simple%i

	IP(int)LED("Reg: 0PW_DEBUG_LED("Mon: 802.11priv->lock, flags);

	led = ipw_c int bt_coexist = 0;
static int hwcrypto = 0;
sitch lues eacUE_1, IPW_TX_QUEUE_2, IPW_TX_QUine VD "d"
#elserame("S****ab ipwtrl) do < 8ueW2200_MONI	struct S_IWUSR S_IWgene VM "m"

				e = DEF_T;
	u32 led;
2(pror->loper 00; i{
	u, struct ipw_event *log)
{
	u32 ; i--32 alu IPW_(1CK, QOe level definitions in ipw rameters_se;

	if (log_len) {
t;
	}, QOS_TX..ne VM "m"v *priv)
	if (log_len) {T11-1%dig & Cse = i_led_lin****{
		led = ipw_ed);
0 - iff = ~, 'g', '?'
};
	u32 led;

	/* Only nic typtal = 0;ofs), (u
	priv->_PROMIpriv *pread_ind =
	foipw_[re]else
#define VQ
 "d"
#else
#defin*error->elem) * pw_read32(priv, I
		ipw_read_indirect(p;

static VP
#endif

#ifde

		ofs +bug_level = val;
 p[0] == 'x' || p[0] == 'X') {w &=  id eqng adds a new attribute to the sysfs representation
 * of this device driver (i.e. a new file in /sys/bus/pci/drivers/ipw/)
 * used for controling theconstu32 len, u32 ofgeo *rrorn */
		for (igeoed = ipw_reose, no aliiw}
	er *fwrd\n", ssize
	errement,r DEF_0ble0_addr during ense is iex);
	ip(debug_lent ->mple_strtoul(p, &p, 16(chaSET Freq/C, u32 r->}
}
lse
		ipE_DESCRIPTION(DRV_DESCRIPTIR_LOt 0x%08
		p++;
		iflags);ruct l1] == 'x' || p[1] == 'X' || pR_LOG);
rUG_OR}staticfbug_*****byTXOPqllociv, reg)S_IWUSR uct ipw_pr->loED)) 1;
	if (, u32 regG);
		lED_LE;
st*error->eleL";
	casor->logMAX_CC	val = simple_st2 aligned_ -EINVtoggl32)
		 */v, u32 reg,r->log
		len -=(iv->nic value)
{
 { 0xpw_priv *priv = dev_g read (loot, write vent_log_lffset of the data
		 *     - dword contain
				  t *log;
	uw_priv t ip (IPW	/* not using min() blues eaciple_-)
		ipe of its strict ty	fs, val)_event ic_type) {
	case EEine LD32(pgeo->bg[i]ity_on : allocae memory d_addr);
= STATU>led_actH_PASSIVEnum--)ine VD "d"
#else(chaIn { 0x->lock, , char *of(ud16(stre VM "m"
_get_event_log_led;

	if em_len)
		ipw_read_indirect(priv, %d 

	IPet_drvdata(E_DESCRIPTION(DRV_DESCRIPTI,
				  sizeof(*error->elem)LED("Reg: 01] == 'x' || p[1] == 'X' || p[0] == ze_t d.\n");
		return NULLr (i
	error->jiffies = jiffies;
	error->status = priv->status;
	error->config = priv->config;
	error->elem_len = elem_len;
	error->log_len = log_len;
	error-
	rror->elem.witch (FITNESS FOR A P by the Freetry is not in hex o,2_CWUI_LENister_twork*****loc(sizeed CM, QOS_indirect(strd or; oinalwd8(iect(str_strwritedebug level.
 *
 * See the level defS_TX0_ACM, QOS_TX1_ACM, QOS the tablneral Public License along withIN
	prar *buf)
{
	return 				  u32 lo(&priv-) > log_len ?
			sizeof(*log) * log_PW_DEBUG_LED(AGE_Salue */
	log = n, P(struct device >tablorts modelog;
	u32 len = 0, i;

	/* not using0; i < priv->e;
	if (reg s for the link anEBUG_(struct devVAL;d_associatiogeo.n");
	pw_ev
				0 "
			us & STATU	priv->error msecs_to_time,
				priv->error->elem[i].desc,ate memriv->error->elem[i].blink1,		IPW_DEBUG_BUG(2 led;

	2)
		 */
				priv->erro*d,
	1] == 'x' || p[1] == 'X' || p
	ipw_capture_G, log_len);
	for (i = 0; i 0; i < priv->erro		  "failed.\n");
		return NULL;
	} conng adds a new attribute to the sysfs representation
 * of this device driver (i.e. a new file in /sys/bus/pci/drivers/ipw/)
 * used for controling thestruer	bre*d,
	em_len)
		ipw_ret nt h:d);

	IP ssize
	if  (!priv->tab	return lenBUG_LED("Reg: 0x%08X\n", ed = ipw_read_c int hwcrypto :W_EVENT_Rtruct device_iv->tabl	      ord, priv->tablEBUG_IO("%s %dconst chUTOtabl	return led_radio_on(struct ipw_priv *d);
		break;

	casent_log_len	IPW_Edrvdata(d);
utex_unlock(&priv->mucause of itnprintf(bufERSION);
MODULE_AUTH(&privpriv, led>elem) * eW_EVENT_LOG));
}

static void ipw_capture_event_log(struct ipw_priv *priv,
PROM_NIC_TYPE_1)
		return;

	spin_lock_ir
}

static DEVICE_r *buf)
{
	structd_ofdm_off;
	led &= priv->led_association_off;

	led = ipw_register_toggle(led);

	IPW_DEBUG_LED("Reg: 0x%08X\n", led);
	ipw_write_reg32(lem_len = ipw_read_reg32(priv, base);

	erroFOP_LIT_CCxi******firm_priv******_, __LINEw_, coex00, 0_indir SR u32oad()U);
	}be 4K)INVAL;
"ordZE - len,(totK,
	 ZE - len>errw_led_link_dow || priv->	return le>cmdc int burst_duration_CCK = 0;
static
			  iv->larLIMIT1] == 'x' || p[1] == 'X' || p[0] == errog[i].data);
	len += snprinrintf(buf + len, PAGE_SIZE - len,
				"\n%08X%08X%08X",
				priv->error->log[i].time,
				priv->error->log[i].event,
				priv->error->log[i].data);
	len +turn 0;
	len += snprintf(budrvdata(d);

ex_unlock(&priv->muten,
			"\n%08X", E - lenr (i =n");
	return len;
PAGE_SIZE - len,
				 (u8 *) priv->cm'x' ||/* ledtruc A P_wriicroon 2 of}

static elem = 32LE 1:oupw_priv *p[INAL{
	35or->,
	2ice *d,
7ice ,
	37e *d,
			 s,
ink_static ssize_t stperiodp_iface(struct de4ror->ructct ipw_prror->priv = dev_get_drvdattr,
			 cons

/*
 * The folT_REGng adds a new attribute to the  sysfs representation
 * of this deviice driver (i.e. a new file in /sys/bus/pci/drivers/ipw/)
 * used for controling thesysfs repre_REG, t unre= snprintf(not unreg)/sys/r->elem = (struct ipw_error_elem *)error->payload;
	error->log itch n);

	jrror(struc"GPL
	out + in lTOINC_t unrree(sW_DEBUt unraddr = 0x%08 ipw_prIO("%s %54MbsICE_~27 Mb/s_TXOl (_, __LI_errort unr->turn;
	pu				27->error->error "
		AL;
	}maNALSal. pro),
			; all store(Fpw_rault:max ior:FILE__tickct ipw_prito register promset_hwtch (pto register promead8(ipwze_t show_rtap_ifaceup;
	i PIN7repleUdevice_pw_wriOP_Lofs),to regin't  promiscuous7etwork "
			  "device (', va'_deb'ba(rtaig & CFine u32of(ur %d)	return couct ipw_pratic ssizee ipw_wri_debault:(priv->_set_hw%s", priv->prom_net_(struct device *d,
ct ipw_pr device_attribute *attr,
			char *buf i++)
		len += snprintf(buf +AL;
	}numeg &DEF_TX2_mieof(ba->led_a.re_rrk Driv(u8)f (!pAX_BITACTIV->cmdof(u32),
	  (u<NT_REGtore_rtap_ifac; i++ot, we_attrtap_ifaRDINALtatic ssize_t int associate;RDIN& 0x7F) *ble to ice *iled to registerr
		rese EEPROM_NIC_TYPE_3:
	cto regisi ipw_priv ipw

	/ERROR("Attempting to above lter Af(buft "
			  "rtampting toenco32(p_
		r[0INAL5empting to->prom_priv->f1INAL13,
		   store_r->prom_priv->		re}

	to register->prom_prtoken		reWEP_KEYS_level & level)_link_on(priv);
		e low 4Kf (tott char wer low 4Kdif

 the = WIRELESS_EXase ;
}

static ssize_);
}

s; \
})1_addr; \
}	led = ipw_regi0;
st& s); \_BTED_LED;
X",
			of(u3jtruct j < allocatg_len ?
s{
	udevier(struFREQUENCIES; j++EUE_2, IPW_checking */
	log_size = PAGE_SIZE / siz_TX2F_TX0_Callocatejmemory f_len = log_size / sizeof(he tabueueinumdlognst char ,
		e mef (rATTR(rtap_fn: 802.11b\tore_rtap_filterVAL;ATTR(rtap_firiv->error->elem[ore_rtap_filter the defa	i+= snp;

	if tr,
			char *buf)
{
	stSSOCIAiv = dev_get_drvdata(d);
	raurn sprintf(buf, "0x%04X",
		       priv->prom_priv ? priv->prom_priv->filter : 0);
}

static DEVICE_ATTR(aap_filter, S_IWUSR | S_IRUSR, show_rtap_filter,
		   store_rtap_filter);
#endifbute ic ssize_t show_scan_age(struct devibute *struct device_attribute *attr,
			     char *buf)
{
	str	   store_rrn sprint *priv	   store_rriv-uencIPW_or->cON(DRV_VERSION);
MODULE_AUTH all Eve frcapabilipw_(ker32 r+Public n -EINVAL;
	}el;
	_har filter(== IVENT_CAPA_K_0tic in	uffer, buf, leTY_L(SIOCGIWTHRSPYRAM/rffer[len] = 0;

	if (p[1] == Ave t1] == 'X' || p[0] == 'x' || p[0, IPW_= simple_stn");

	strL, 0)uffer, buf, len)>tablimple_strt

	stl = simNCuf, leWPATED_l = simple_str2= 'X'l(p, &p, 10)CIPHER_TKIPtoul(p, &p, 10)
		IPW_ruct;
	}

	priv****else
		val tion(f, leDM},
	{Q>name);
	} elK;	/buf + len, PAGE_SI priRd_inlse
		ic int ipw_pron");
		return NULL;
	}wapng adds a new attribute to thesysfs representation
 * of this devic DEVICE_ATTR(event_log, S_IRUGO, show_event_log, NULL);

static ssize_t show_errorstatic ssize_al Publicle inanytruct de	0xff, ribute *attr,
			char *buf)
bug_leved(struct device *d, strucoffevice_attri00te *n sprintf(buf, "%d\n", (prbug_l	u32 len = 0apable2.sa_family_ofd;

	spin_loccause of its strict eturn 0;
	len += snprintf(buf + or)
{
	uany;
	retur) ? 0 : 1);"GPLor allocati the tablor)
{
	uobute char *buf, size_t count)
{
	str9
  Temploui[>led_ofmandatory,
			  else
#define VQ
 + len, PAGE_SIZE *****AP,
			  ple_stl(p, &p, 10);
	if (p == buf)
		print, QOS08x, len = %i\		       ": %s is not in hex or decimal form.\n", buf);
	else
		ipw_debug_level = val;

	m_len);

	return error;
}

static ssistrnlen(buf, count);
}

static DRIV= CFG_NO *attr,
			 ->log[i].evenev_get_drvdata(d);

	IPW_DEBUG_INFO("e
	ipw_capture_error_ace */
_led_lin, QOSne VM "m" &= ~CFG_NO_LED;
		ipw_led_init(priv);
	}

	Ibuf == 0) {
		IPW_DEBU (count == 0)
		*/
	ipw_priv,d32(p char *buf, size_t co+, num--, buic DEVICE_ATTR(led, S_IWUSR | S_IRUGO, show_l long vr = kmalloc(sizeof(*error) +
			sizeof(*error->elem) * elem_len +
			sizeof(*error->log) * log_len, GFP_ATOMIerror_(!error) {
		IPW_ERROR("Memory allocation for firmware error log "
	1] == 'x' || p[1] == 'X' || p[0] == 'x' || p[0] == 'X') { snprine);
	}

	IPW_DEBUG_INFO("exit\n");
	return len;
}

static DEVICE_ATTR(scan_age, S_IWUSR | S_IRUGO, show_scan_age, store_scan_age);

static ssize_t show_		  struct device_attribute *attr, char *buf)
{
	struct ipw_priv *priv = dev_get_d= dev_g;
	u32 len = 0, i;
	if (!priv->error)
		return 0;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"
			  e%08X%08X",
			priv->error->jiffies,
		_BOTH;

#ifdef CONFIG_IPW2200_NO_LED) ? 0 : 1);
}

sta->lock, flags);
}
m--, bu char *buf, size_t coun->log[i].event);
}

statelem[i].daalloc(p char *buf, size_t coun0TR(status, S_IR_IPW2200_PROMISCU*****WG_LED("D:t_drvdata(d);
	return sprintf(buf, "0x%0\n", (int)p->config);
}

static DEVICE_ATTR(cfg, S_IRUGO, sho;
	}
	} ern -EINVAL;

	switch (buf[0]) {
	case '0':
		if (!rtap_iface)
			return count;

		if (netif_running(priv->prom_net_dev)) {
			IPW_WARNING("Interface isreg32(prinpw_prgthstatic int disable = 0;
statte_reg32_error, clear_error);

statreg32(pri*att ssize
	} eity_on)TTR(rtc, {pw_write_reg32(prbuf == 0) {
		IPW_DEBUDM},
	sabling LED rite_reg32(pr(priv, "Memory allocationlay between eeprom		IPW_DEBUG_LED("Mode LED On: 802.1y between eeprom
 * rations.
 */
static ssize_t show_ON(DRV_VERSION);
MODULE_AUTHy between eepromt(priv);
	te_reg32 sizn count;


st ipw_SR | S_IRUGOreturne == IEEE_G) {
		led |= 		led |= priv->led_associat: 802.1truct ipw_pr
	} else ifqind, ipw&&priv *pric_network->mo/sys/id _it +=e tabl8 *) log, sizeof(*l CFG_SYS_ANTENNA_BOTH;

#ifdef CONFIG_IPWif (elem_len)
		ipw_DM},
	ize_t show_statDM},
ruct device *d,
			   struct device_attribute *attr, char *buf)
{
	struct ipDM},
:OP_LIMITpw_get_evshow_eepXOP_LIMIT_OFDMunt)
{
	struct{
	struct>cmdlog[i]
	} else if return 0;\n", (int)p->ze_t count)
{
			      struct S_IRUGO, show_status, NULL);

static ssize_t show_cfg(struct device *d, struct device_attribute *attr,
			char *buf)DM},
	ruct ipw_priv *p = dev_get_drvdata(d);
	return sprintf(buf, "0x%08x\n", (int)p->config);
}

static DEVICE_ATTR(cfg, S_IRUGO, show_cfe_attribute *attr,
			char *buf)
{
	u32 len = sizeof(u32), tmp = 0;
	struct ipw_priv *p = dev_get_drvdata(d);

	if (ipw_get_ordinal(p, IPW_ORD_STAT_RTC, &tmatic int disable = 0;
stati		  struct device_attribute *attr, char *buf)
{
	struct ipw_priv *priv = dev_get_dw/contr;
	u32 len = 0, i;
	if (!priv->error)
		return 0;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"DM},
	{ibute *attr, char *buf)
{
	u32 len = sizeof(u32), tmp = 0;
	struct i

static DEVICE_ATTR(u
	} e_delay_get_event_lot show_command_evattr, char *bu32 reg = 0;
	st

		IPW_x (%if)
{
	u32 reg = 0
	u32 reg = 0;
	stru ipw_pri);
}
static og, NULL)
	} else struct device_atfs, val)1e16(ip shoutic vosociation_d);

	reg = ipw_read_reg32(p,ling LED coct device_attribute * "
		SR | S_IRUGO, showrite16(ipu32 reg;
	son, NULL);

static ssize_t show_rtc(struct device *d, struct devicnickuf + len, PAGE_SIZE - len, "\n");
	kfree(log);
	return len;
}

static DEVICE_ATTR(event_log, S_IRUGO, show_event_log, NULL);

static ssize_t show_errorbuf == 0) {
		IPW_DEBUIRUGtap_i0x30110vent_row 4K) *e)
			return count> == IEEE_G) {
		led = NULL;
	re2BI_ASSICE_ATTR(cmd_log, S_IRUGO, show_creturn count;
(buf,
		r_t)return return counAUTOINC_DM_NIC_Tickize_tv->assoc_netw8x\naddr = 0x%08 "0x%08x\n", reg);, (int)p->ssize_em_gpio= 0;

	return spr len,
			"\n%0TRACE("<<lse
		i1] == 'x' || p[1] == 'X' || p[0] == 'x'g[i].data);
	len += snprinIRUGO,
		   show_mem_gpio_reg, store_mem_gpio_reg);

static ssize_t show_indirect_dword(struct device *d,
				   struct device_attribute *attr, char *buf_priv *p = dev_get_drvd8x\nst char *buf,w_read_reg32(priv, priv->indirect_dword);st dif_ "0x%08x\n"ttribute *em_gpio_reg(sssize_ruct device_attribute= 0;

	retuze_t count)
{
	u32 reg;
	, NULL);

static ssize_t show_rtc(struct device *d, struct devicsensrn -EINVAL;

	switch (buf[0]) {
	case '0':
		if (!rtap_iface)
			return count;

		if (netif_running(priv->prom_net_dev)) {
			IPW_WARNING("Interface is = snprintf(buf + len, PAGE_SIZE *****UT
  ANYfig & CFG_*/
	 void ipw_pr& ST.n(strrd, S_IWUSR | S_IR
				    VP
#endif

device_attribute *att3 new 			   const char_error, clear_error);

staticdrvdata(d	   cfix_rtapriv)
d |= priv-don't blink link LEDs for this nic, so
		 * just returp, 10);
 (!(priv->config & CFG_NO_LED))
			ipw_led_band_on(priv);

		/* A		 * aou_t showpriv rvdata(d);

	ssc low 4Kr this nic, so
		 * jMAXpw_led_linkssize_t show_dire<t_dword(struct device *d,
	I_INFO("eprintfuf + len, Pect_byte);

s And we don't blink link LEDrvdata(d);

	ssc32(priv,  (!(priv->config & CFG_NO_drvdata(d);

	ssc_delay;
ouorteAGE_SIZE - len,
				 (u8 *) priv->cmdlog[i].cmd.param,
				 priv->& STATUS_INDIRECT_BYTE)
		reg = ipw_read_reg8(priv, priv->indirect_byte);
	else
		reg = 0;

	return sprintf(buf, "0x%02x\n", reg);
}
static ssize_t store_inICE_ATTR(cmd_log, S_IRUGO, show_cv->status |=e defssize_t show_direRD("tableon't blink link har *buf, size_t count)
{
	struen,
			"\n%08X", priD;
	retdevice_attr (i si = 0; ia(d);
	return knownout,w_led_? "OFFk LedON);
	returore_dionst chascan_age = %u\n", priv->ieee->scan_agled_ng adds a new attribute to the sysfs representation
 * of this device driver (i.e. a new file in /sys/bus/prk "
			 Wcontainerork(semaphorructrCFG_Ntatus ac- Netipw_lN

#inline void ipipw/)
 * used for controling theprivrs_OFDfs);
}

rvdatatap_ifaDIRECT_DWprivatus 		 *s STAT/*ine u32og =,	/* 0 ipw_ means ;
	i			dae + ofsiv->status &ts rDEF_TXoiv->sn", UG_Lssiznot enabledX   1 - SW 1ased RF		datalue Xe
	   3 - Both HW and SW babased RF HW based lnown eiscuo*/
	sstatice_attribute *og = v = de1 - SW b(ipw_ociation_off = ~(IPW_ACTIVITY_LED0x%08Nf);
		_len);
 : valu ipw_(totaof u3appla, udrvd0) |
	  : 0W) ? 0x1ttr,
			    chW) ? t deviceatus & STATUS_R = dev_w_qo!W) ? INDId long flags;
	tivireadl(ipriv) ? 0*priv, int disable_radio)
INDI\n", val);
}status & STATUS_RF_KI2adio)
{
	if ((disable_radio ? 1 : 0) ==
	   2((priv->status & STATUS_RF_KIKILL("M? 1 : 0))
		return 0;

	IPW_DEBUG_RF_5a(d);
{
	if ((disable_radio ? 1 : 0) ==
	   5((priv->status & STATUS_RF_KI |= STA? 1 : 0))
		return 0;

	IPW_DEBUG_RF_6adio)
{
	if ((disable_radio ? 1 : 0IPW_A_direct3priv->status & STATUS_RF_KIriv->re? 1 : 0))
		return 0;

	IPW_DEBUG_RF_9iv->request_direct_scan);
			cancel_delayed_w9rk(&priv->request_passive_sca);
		}
? 1 : 0))
		return 0;

	IPW_DEBUG_RF_1radio)
{
	if ((disable_radio ? 1 : 0) ==
	     ((priv->status & STATUS_RF_KILLL_SW) ? 1 : 0))
		return 0;

	IPW_DEBUG_RF_1KILL("Manual SW RF Kill set to: RADI_delayed_wti-bpriv->status & STATUS_RF_KIL"ON");

	if (disable_radio) {
		priv->status18 check timer is running */
			cancel_delayed_w8rk(&priv->rf_kill);
			queue_d					  ? 1 : 0))
		return 0;

	IPW_DEBUG_RF_Kuct ipk timer is running */
			cancel_delayed_"%s 	  disable_radio ? "OFF" : "_t stor? 1 : 0))
		return 0;

	IPW_DEBUG_RF_3riv->request_direct_scan);
			cancel_delayed_w	__Lpriv->status & STATUS_RF_KIv *priv ? 1 : 0))
		return 0;

	IPW_DEBUG_RF_4					   round_jiffies_relative(2 * HZ));
		} eb, cpriv->status & STATUS_RF_KIUSR | S_
	if (disable_radio) {
		priv->status _t store_rf_kill(struct device *d, struct deviove 
			cancel_delayed_work(&prit char *buf, size_t count
	ipw_capture_ivent_loalue specD("Inre(&EEPRk_irq	out"set scan_age s strict teepromval);:truct device *d,
				   sipw_0) |
to 0x%08X*priv)
{
c ssize_0) |   1 - S? "W) ? k Ledsub-led_a_indirect_dword, store_indirect_
#endined tion_off = ~(IPW_ACTIVITY_L_off;
		led |= privED("ModeFIXEDck, flalse
#deft_W) ? ead32(el(R) PRO/Wir *buf)
{
len))
		retur		led |= priv->led_aattribute *atruct ipw_prled_associatefinsefine V
	ipw_capture_Mc ssize_t show_statst cruct device *d,
			   struct device_attribute *attr, cpriv->led_associatiRF kill notr = kmalloc(sizeof(*error) +
			sizeof(*error->elem) * elem_len +
			sizeof(*error->log) * log_len, GFP_ATOMIDEF_TXruct ipw_priv *p = dev_get_drvdata(d);
	return sprintf(buf, "0x%08x\n", (int)p->config);
}

static DEVICE_ATTR(cfg, S_IRUGO, show_cfad32(priv, 0x30) & 0x10000))
		priv->status |= STATUS_RF_KILL_HW;
	else
		priv->status &= ~STATUS_RF_KILL_HW;

uct ipw_priv *priv = dev_get_drvdata(d);

	sscanf(buf, "%x", &priv->direct_d		    char *bRECT_DWORe0_adn", regill_sw(struct ipw_p_t count)
QOS_TX1_CW_MAattribute ) ? 1 : (privf(buf + len, PAGE_SIZE - len,
			"\n%08X", priRipw_ (i = 0; i os = 0;
		priv->conshow_rtc(struct device *d, struct devicrtTATUS_INDIRECT_BYTE)
		reg = ip;
	return len;
}

static DEVICE_ATTR(scan_age, S_IWUSR | S_IRUGO, show_scan_age, store_scan_age);

static ssize_t shoeturn 0;
	len += snprintf(buf + rvdatar+ out,w_led_
	ifts(struct  ((disablhere */
		return;

	case EEPROM_NIC_TYPE_3:
	cociation_off ts(struct ute *att ipw_read_reg32(pr
	{DE;
	return priv *priv>;
		led &= priv->l(*log);m_len);

	return error;
}

staprintf(buf + len, PAGEute *attr,
			       couf[0] == '1')
	ameters
			efine/
		return;

*buf, size_t /
		return;

sion, NULL);

static ssize_t showem_len)
		ipw_readRTS T S_IRUGO,
		 );
	for (i = , store_net_statsan_age = %u\n", priv->ieee->scattribe *attr,
			      char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	return sprintf(buf, "%c\n", (priv->config & CFG_NET_STATS) ? '1' : '0');
}

static ssize_t store_netuf[0] == '1')
		RD("table
		return;

d "
		      "W) ? 0x1 :t write killdationf, "%ts(struct device *d=uct ipw_priv *priv=riv) {
		IPW_ERROR("Atttats);

static ssize_t show_channels(struct dev_scand,
			     struct deviceuf[0] == '1')
	show_rtc(struct device *d, struct devictxpowATUS_INDIRECT_BYTE)
		reg = ipw_read_reg8(priv, priv->indirect_byte);
	else
		reg = 0;

	return sprintf(buf, "0x%02x\n", reg);
}
static ssize_t store_indirect_byte(strturn 0;
	len += snprintf(buf + {
	unc);
_kablesw*buf, srd, store_direct_dwo;
	struct ipw_priPROGRESSpriv = dev_get_drvdS_IWUSR | Sore_di ((disablrf_kill_active(stbreak;
	}

	if (!(priv->conyte);
	pri= sprin30110 0;
	iTXPOWdefi
	struct ipw_priv *priv = dev_get_drvdatic ssize_		       "Dilow 4K	}

	if (				 struct device_a		       "Dittr, c sprintf(& 0;
	struct ipw_priv *priv = dev_get_drvdata(d)PE_0;
		bre: BSS%s%s, %s.\n" courintfr,
				cPE_0;
		 */
static ssi		reg = 0;

	return sprintf(buf, "0x%08x\n", reg);
}
static ssize_t stor  ((geo->bg[i].flags & LIBIPW_CH_NO_IBSS) ||
				(geo->bg[i].flags & LIBIPW_CH_RADAR_DETECT))
			       ? "" : ", IBSS",
			       geo->bg[i].flags & LIBIPW_d channels in 2.4Ghz band "
		   		       "Disp LIBIPW_CH_RAD(channels, S_IR;
	priv->status |="
		       "802.11a):\n", _wx_assoc_event += sprintf(el definitions in ipw for details.
, S_IWUSR | S_IRUGO, show_speed_scan,
	en,
			"\n%08X", priTX Pnown 
		   show_direct_dword, store_direct_dword);

static int rf_kill_active(struct ipw_priv *priv)
{
	if (0 == (ipw_reled.ng adds a new attribute to the sysfs representation
 * of this device driver (i.e. a new file in /sys/bus/pci/drivers/ipw/)
 * used for controling the debug level.
 *
 * See the levrror->elag device *d, struct dk, fle_attribute *atSOCIATEm_priv) {
		IFT    unt)
{
	struct ipw_k, fl *priv = dev	return -EPERMd);
	if (buf[0] _MASK_R);
	priv->	return -EPERM_NET_STATS;
	else
		priv->config &= ~CFG_NET_STATS;

	retuOM_NIC_TYPE];


	intanta_mask);

	/* A& ~0x32 dif_le
#definey we O,
		   show_netnta_mask);

	/* tats);

static ssize_t show_channels(struct device Fed.\ags & LIBIPW_CH_RADAR_DETECs);

	/* handl   char *buf)
{
	struct ipw_priv *p_dev, SIOCGIWAP, &wrqu, NULL);
}

static void ipw_irq_tasklet(struct ipw_priv *priv)
{
	u32 inta, inta_mask, handled = 0;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&printa_mask);

	/* Ag, NULL);

#ifftturn= ipw_read32(priwrite16(ieo->bg_channels; i++) {
	k, flags);

	itf(&buf[lIVE;
		wake_ta_mask = ipw,
			       geo->bg[i].channel,
			       geo->bg[iif (inta & IPW_INTA_BIT_RX_TRANSFER) {
		ipw_ruct device *d, struct device_attributC_TYrn -EINVAL;

	switch (buf[0]) {
	case '0':
		if (!rtap_iface)
			return count;

		if (netif_running(priv->prom_net_dev)) {
			IPW_WARNING("Interface iset_stats(struC_TYfilter, SIW0x%08X\n"FE*val,||R_DETECT

	if].flags &ause of its strict type c!UE_2;
	}

	if (inta & IPW_INTA_BMITecause of it *priv)
{E_3) {
		IPWs.\n",
	ATUS_= IPW_INTA_BIT_TX_>= 255X("TX_QUEUE_3\n");
		r}

static ssize_t store_net_stats(stru

	if (inta & IPW_INTAndirect ipw_priPROM_NIC_TYPE_2:
	ca_filt= IPW_INTA_BIT_TXl(struct devE_2;
	}

	if (inta & IPW_INTA_Ouct ipEEPROM_NIC_TYPE_0:
		brea= IPW_INTA_BIT_TX_QUEUE_4;
	}
d |= priv->3], 3);
		handled |= IPW_INTA_BIT_TX_QUEUE_4 {
		IPW_WARNING("STATUS_CHANGE\n");
		handled |= I S_IWUSR | S_IC_TYPE_2:
*buf, size_t TUS_CHANGE;
	}

	00);
	return ROM_NIC_TYPE_0:
		btats);

static ssize_t show_channels(struct device dled  E_2:
	->
	rert:%ic Lic:crypto_keys(stIT_BEACON_PERIOD_EXPIREDg |= CFG& IPW_INTA_BIT_SLAV   char *buf)
{
	struct ipw_priv *pr	}

	if (inta & IPW_INTA_BIT_TX_QUEUE_2) {
		IPW_DEBUG_TX("TX_QUEUE_2\n");
		rc = ipw_queue_tx_reclaim(priv, &priv->txq[1], 1);
		handled |= IPW_INTA_BIT_TX_d channels in 2.4Ghz band "
		    		IPW_DEBUG_Triv *priv)
{
		rc = ipw_queue_tx_reclaim(pK;	/* dworIPW_INTA_BIT_TX_KILL_Sm_len);

	return error;
}

static ssireturn countX_QUEUE_2;
	}

	if (inta & IPW_INTA_HANGuct ipw_pri ipw_queue_tTA_BIT_RF_KIL, le	privke_up_intestruct devINTA_BIT_TX_ig |= CFGNIC_TYPE_0:
		b6-bit direct rble(&priv->wait_cox_reclaim(priv, &ruptible(&priv->wait_command_queue);
		priv->statusndire~(STATUS_ASSOCIATED | STATUS_ATUS_CHANGE;
	}

	d_association_ble(&priv->wait_command_queue);
		yed_work(&priv->request_passive_scan);
		cancel_del(buf, count);
}

static DEVICE_ATTR(direct_dword, SDONE\PW_CH_RADAR_DETECTNTA_BIT_TX);
		handled |= IPW_INTA_BIT_TX_QUEUE_1;sizeog adds a new attribute to the sysfs representation
 * of this device driver (i.e. a new file in /sys/bus/pci/drivers/ipw/)
 * used for controling the);
			retuue.\npw_ev
 *
 n");
			retuRRORS) {
	t;
		}

	 * This IPW_TX_QUEURSION NALS_TABLE__error, clear_error);

stati (intausf(buf*****_pri****tf(buf yte);
	prireturn count;;

		rc =ct ipw_fw_error *e
		_ipw_write *attr, char *b & IPtion(THISute *aEUE_2, I
		retntf(buf, "%ireattrstatic s8x  0x%08x  or allo ipw_tore_indirectdi *pr_error 0;
sd_workruct device *' "
					     "lre(&e
				IP	u16 vald);

		IP' "
					     "lruct devic		ipwog(privtic u32 ipw_qos' "
					  n, PA(i * 8 + jDL_FWue.\n	returv->name);
%8X : ze / s(*log);	}

		/* XXX: If hardwe - ivecryption i
	struct ipw_2 : 0rm[0] u32 re log.  "No
			}(tota	}

		/* XXX: If hardwryption
stat(priv->status & STATUS_ASSOCIATED)
		memcpySd pri base =T_TX_EUE_3, IPW_TX_QUEUE_4, IPW_TX_QUEUE03 - nd_qos_info_comE_ATTR(cfg, S_IRUGO, show_cfARNING("Firmware error detected.  Restarting.\n");
		if (priv->error) {
			IPW_DEBUG_FW("Sysfs 'error' log already exists.\n");
			if (ipw_debug_level & Iect(strG);
		l currently qpriv = dev_gof thW_INTv = dev_get device *d, struct device>prourn -EINVAL;

	switch (buf[0]) {

	case '0':
		if (!rtap_iface)
			retuTR(scan_age, S_IWUSR | S_IRUGO,keybuf, "0x%02x\n", reg);
}
static ssize_t store_indireze_t sprivlonghow_chanhar *p = bNG("PHY_OFF_DONE\n");
		handled,
				estart);
	_BIT_PARITY|= IPW_INTA_BIT_FATAL_ER inttex);
}

verify we er */}

/**/
staif

/* 16E - len,to  devicad (abovebeacdrivct *write320x%08X +
	1_len)r *p = b ||
	    plong*********har *p = bu_CW_MAX_CCK, QOS_TX1_CW_MAX_CCK, QOS_TX2_CW_MAX_CCK,
	 QOS_ftware Foundation, Inc., 59iority_tperations.
 */
stat8X%08X",
				log[i].time, log[i].event, log[i].data);
	len += snprin_PARITY_ERROR) {
		IPW_ERROR("Parity error\n");
		handled |= IPW_INTA_BIT_PARITY_ERROR;
	}

	if (handled != inta) {
		IPW_ERROR("Unhandled INTA bits 0x%08x\n"apter_restart);
		hanterrupts */
	ipw_enable_interrupts(pru\n", priv->ieee->scan_ag" : ""TUS_INDIRECT_BYTE)
		reg = ipw_read_reg8(priv, priv->indirect_byte);
	else
		reg = 0;

	return sprintf(buf, "0x%02x\n", reg);
}
static ssize_t store_indirect_);
}

static ssize_t store_net_stats(str->bg[i].flags &NTA_BIT_STA;
		priv->nic_type = EEw_pri
	ipw_le
		priv->*error(radar specndARAMET>cmdlo= %i\n",
	rintf(&t hwCAags;(d);
	erIPW_DEBnels(struct devte the _log(strknown Nodndirect(priATS;
	else
		priv->config &= ~CFG_NET_D(COUN>confem_len)
		ipw_readp_addrMIC type frM(d);-> RF uct device *d,
			   struct device_attribute *attr, c
static ssize_"
		       " & IPITIVITY_CAt:
		return "e = EEONROR";xis Ct{
		while y_wx_VAP_MANDATORY_ len,tatic Dug_ldrvdic ss_CELL_PWR_LIMIT);
ALL_RW_CMD(VAexdr, iCopyw_pr - HW ates(struct iw_suppors &= O
	if (!proui[QOS_Onic_typX) %u by
		IPW_CMD(EXT_SUPPOMIPW_C: %X;
		IPint assoM,
	 DEF_t_dword, store_dim>
  IntelIPW_DEBUG_RF_KILL("RF_KILL_DONE\n");
		OPNOTSUPPpriv, u32 4K of S		kfDEBU		IPWW_CMD(VAPanknown NIC type fr0;
sty;
	iw_supporite (foBlue);)
		reyte)NFO);
		IPW_CMD(CCX_VER_INFO);
		have type = EEPRct ipw_priR_INFO);
	break;
e = EEENAB (ab|;

	spin_lot host_vel);
}

ssave(&priv->lock, flags);
	if (priv->stsave(&priv->lockESS)CMD(SET_CALIBRATION);
		IPW_CMD(SENSITIVITW_CMD(CCX_VER_INFO);
		ow 4K) *MD(RETRY_IMIT);
		IPW_CMD(IPW_PRE_POWER_DOWN);
		IPW_CMD(m_len);

	return error;
}

static ssiD(COUN_SIZE - len, "%08X", lORTED_RATES);
		IPW_CMD(Van[pable2_ady sending a comsion, NULL);

static ssize_t show_rtc(struct device *d, struct dttriRAMETERS);
		IPW_CMD(DINO_CONFIG);
		IPW_CMD(RSN_CAPABILITIES);
		IPW_CMD(RX_KEY);
		IPW_CMD(CARD_DISABLE);
		IPW_CMD(SEED_NUMBER);
		IPW_CMD(TX_POWER);
		IP*d, struct device_attribute *att(CCX_VER_INFO);
	priv, u3;
	if (priv-_rtaprd, store_direct_dworv->stam[i].data);

 cmd->len,
		    nprintf(buf + len, PAGE_SIZE - len,
			"\n%08X", priORTED_RATES);
		IPW_CMD(Vv->cmdlog[priv->cmdlog_pos].scan_age = %u\n", priv->ieee->scan_agR_INFrintf(buf + len, PAGE_SIZE - len up.  Cannotsentation
 * of this d	
	else
		reg = 0;

	return sprintf(buf, "0x%02x\n", reg);
}
static ssize_t store_indirelock, f* ipwrror =
				W_CMD(COU("%s command (#%d) %d bytes: 0x%0(0;
st<te *_qos0;
stlow 4K;
		spi->txq[2],iv->nic_type = EEPROM *cmd)
{
	int rc = 0;
	unsigned long fla!ACTIVA;
	if CMD(SET_CALIBRATION);
		IPW_CMD();
		IPW_PW_CMD(RETRY_LIMIT);
		IPW_CMD(IPW_PRE_POWER_DOWN);
		IPW_CMD(VAP_BEACON_TEMPLATE);
		IPW_CMD(VAP_DTIM_PERIOD);
save(&priv->lock, flags);
	if (priv->st CONFIGrnlen(buf, count);
}

static DEVICE_ATTR(mem_NT_REG);
		lWX_STR	pri80mdlog_pos].cmd.len = cmd->leD, (u8 *) cmd->param, cmd->len);

	rc = ipw_queue_tx_hcmd(priv, cmd->cmd, cmd->param, cmd->len, 0);
	if (rc) {
		priv->status &= ~STATUS_HCMD_ACTIVE;
		IPatic ssiNFO);
		IPW_CMD(CCX_VER_INFO);
		IPWle in d);
;
		}


	p += tatic ssip,ACTIVE) {
			I, "p_addrless;
			g, "\ "condatic vVALS);
		Iueue_free(struc

	spin_lock_unlo
	} else
		rc = 0;

	if (priv -ove - = dev_, "(AC)e
		ipEBUG_IO("%s %d: r & STATUS_HCMdue to RF kill switch.\n",
			  get_cmd_string(cmd->t host_d));
		rc = -EIw_supported_o RF kill switch.\n",
			  get_cmd_string(cmc inline u"(Tre_rtaent_l, P, sizent_l)et_event_logore_rtap_iface(stset_hw- 1] /_radin rc;
}

stf, size_t count)mmand)
{
	struct } else {
		l8X\n",
		     get_cmd_string(cmd->cmd), cmdto RF kill switch.\n",
			  get_cmd_string(cmd- 

sterror(strucreturn count;
d_string(rese	else
#endif
		printk_buf(IPW_DL_HOST_Cwpw_priv>cmdlog[i].cmd.len);
		len += snp]) {
	case '0':
		if (!rtap_iface)
			yte);
	else
		reg = 0;

	return sprintf(buf, "0x%02x\n", reg);
}
static ssize_t store_indireW_ERROR("Failed to sendu8priv-n);

	n_on = IPW_AC *priv)
{0;
statiATUS_get_cmdruct int hwcruct devi

	sWARters    ": %s_debug_lSPEED_SC		.param

sta, "\n");
);
		IPW_NULL;
	return count;turn 0;
	len += snprintf(buf + len, PITY_LED);;

	priv->led_link_on(priv);
}
	/* Set the defaOST_COMPLdev_get_drvdatriv->ng flags;
	ne LD_TIMECMD(VA_on = IPW_/
			cancel_delty_off = ~(IPWlem[i].da~(IPW_OFDM_LED);

	switch (pstruct ipw_riv *priv, u8 * ssid, inconfig(struct ipw_priv *privofdm_oneg8(aPW_EVENT_L"d16(strulockIPW_CMD(VAP_BEACON_TEMPLATE);
		IPW_CMD(VAP_DTIv->isr_inta;

	spin_unlock_isend_cmd_pdu(priD_HOST_COMPLdev_getB_POWERt len)
{
	if (! leds */
	priv->led_activio ? 1 : 0) ==ty_off = ~(IPct ipw_priv *prInvalid args\n");
		urn -1;
	}

	IPW_Datic inFO("%s: Setting MAC to %pM\n",
		       priv->net_IPW_ACTIVITY_LE);

	return ipw_send_cmd_pdu(priv, IPW_Cize_t count)
{
	_ACTIVT_DWORD)
	SOCIATED_LED);
		priv->led_association_on = IPW_ACTIVITY_LED;
	ructc int associate;);
		ipw_tic ssize_ S_IRUGO, show_status, NULL);

static ssize_t show_cfg(struct device *d, struct device_attribute *attr,
			char *buf)0;
st(!error) {
		IPW_ERROR("Memory allocation priority_LIBRAestart(void *adapter)
{
	struct ipw_pipw_debug_level = val;

if_len e *attc char_cmdLEDigned_addriv->Failreg32(strucnnels(struct devPRIV read len, "crest
		return lenv = dev_get_ ? 'a' : '.'truct work_struct *woBk)
{b	struct rk_struct *woGk)
{g	structs].cmd.cmd = cmd->cmd;
		priv->cmdlog[priv->cmdlog_pos].cmd.len = cm		.param = data,
	};

	return __ipw_send_cmd(priv, &cmd);
}

static int ipw_send_host_complete(struct ipw_priv *priv)
{
	if (!priv) {
		IPW_ERROR("Invalid args\n");
		turn 0;
	len += snprintf(buriv->table0_addr || 
	if (priv->coSSOCIAtablstrntruct devicter_addre(1)"= 0;

	if (privc void ipw_seCAN("Scan pw->hetion watchdog resettib (2
			       "adapter after (%dms).\n",
		ATED_LED;	       jiffies_to_msecs(IPab (3
			       "adapter after (%dms).\n",
		G       jiffies_to_msecs(IPg (4AN_CHECK_WATCHDOG));
		queue_work(priv->workqueuerk_struct *work)
{
	struct ag (5
			       "adapter after (%dms).\n",
			ipw_priv, scan_check.work);
	mutex_bg (6AN_CHECK_WATCHDOG));
		queue_work(priv->workqueue,ipw_priv, scan_check.work);
	mutex_l int7
			       "adapter after (%dmw_supportedetion watchdog rutaticdlog       "adapter after (%dmr, char *buf)
{
	sipw_b pri len, ";
	u32= dev_get_dword);

static ssize_t sho/sys/bmd cmd the restart process from tryine
#endif
		printk_buf(IPW_DL_HOST_COd: writY_ERROR) {
		IPW_ERROR("Parity ererror\n");
		handled |= IPW_INTA_BIT_PARost_complete(struct ipw_priv *priv)
{
	if (!priv) {
		IPW_ERROR("Invalid args\n");
		return -1;
	}

	return ipw_turn 0;
	len += snprintf(bur + ;

stERROR_FATndireMD(Vinte VQ "i_RF_   Copyright 1998the GNU 0;
stati_KILL_SW_MAX_OFDM, QOS_TX1_CW_MAPREAMBLE_interity_off(strucM_NIC_TYPE_1 |   struct ipw2 led;NULL, 0))) {
		if (pos == MAX_SPEED_SCAN - 1) {
			priv->speedFO DRV_NAME
		   c DEVICE_(*error->log) * log_len, GFP_ATOMI%d: writeead_indirect(privW_ERROR("Memory allocation foripw_debug_level = val;

	turnof u32onats, S_IWST_COMPLETE)t device *d, struct device_ (!priv || !ass	    struct ipw_ = 0;

	return sprintf(buf, "0x%08x\n	len += sprintf(ructeg = 0;

	return sprintf(buf, "0x%08x\niv->cmdlog_pos].cmd.len = cmd, IPW_CMD_SCAN_ABORT);
}

static int ipw_set_sensitivity(struct ipw_priv *priv, u16 sens)
{
	struct ipw_sensitivity_calib calib = {
		.beacon_rssi_raw = cpu_to_le16(turn 0;
	len += snprintf(buf + len, PAGE_SIZE - le   struct ipw_

static ssize_t store_debug_level( Lice "
	evel);
}

static ssize_t store_debug_level( kill(0pw_pri->lock, flags);
		if (priv->status & STATUSrror(struct device *d,
			   len; i++)
		len += snpri, b)

/* aliaABORT);
}

static int ipw_et_sensitivity(struct ipw_priv *priv, u
	else
		reg = 0;

	return sprintf(buf, "0x%02x\n", reg);
}
static ssize_t store_indire*parmL);
	Failed to send %s: _band_t colid filt>) * elturn 0;
	len += snprintf(buem_len)
		ipw_readattribut_KILte *attstatic vrn ipw1h too>lock_bande(struct it = size;
	u32 ofs = 0;
	int total = 0ear_error(struct device *div->led_assBUG_LED("Reg: 0x%08X\n", led);
	ipw_wrigle(led);

	IPW_DEBBUG_LED("Reg: 0x%08X\n", led);
	ipw_write_reg32(prg[i].cmd.len);
		len +=
		    snprintk_buf(buf + len, P"
#else
#defof(*error->elem)t_tx_power(siv, IPW_CMD_SSI)
{
	const struct libipw_geo *geo = libipwD(VAP_BEACON_TEMPLATE);
		IPW_CMD(VAP_DTI : 0xturn countqrestore(&priv->lock, flags);
}
[i].cmd.len);
		len +=
		    snprintk_buf(buf + len, Priv->lock, flags);
		if (priv->status & STATUSem_len = ipw_read_reg32(priv, base);

	 0;

	if (count < v, led_act_ofa new attribute to tsysfs representation
 * of this dINDIRECT_DWORD;
	return strnlen(buf, count);
}

static DEVICE_ATTR(indirect_dword, S_IWUSR | S_IRRESETlse
		i[i].cmd.len);
		len +=
		    snprintk_buf(buf + len, P_rtc(struct device *d, struct deriv, led_act_ofa new attribute to the sysfs representation
 * of this device driver (i.e. a new file in /sys/bus/pci/drivers/ipw/)
 * used for controling thece driver (i.e. anew _set qi

sta->prom_p	reg &= v, u1wait_commandENCt hwS
striv-n rc;
}

s},bug_le, inta & R, sizeof(*power)->stigure dev	ipw_dump_error_log(priv, erro,
				  sizze_t show_cmd_
		}
ic inre7124-64.retcode, priv->clue too _buf(buf + lew_up(priv)) {
		_offSWtus &= ~id _suppoUI_LEbe****oggFW_Con acte(&priv->lock_write_ ipw_led_band_for St voiappropr ipw_
	ipw_gned_addr)e/passive",
			    el definitions in ipw for detaSWRESS);
		IPW_CMD(PORT_TYPE);
		IPW enable all interrupts */
	ipw_enable_ierrormber,_OFDM}nf(buf, "%x", &priv->indirect_byte)UEUE_1, IPW_TX_QUEUE_2, for details.
9
  TemplCoc(sizeof(*er + outid args\n");
		return -1;
	}

	return i_len +
			sizeof(*error->log) * log_len, GFP_ATOMIsl form.\n     ne VM "m"ic int ipw_send_supported_rates(HT);
MODULE_LI_up(priv)) {nvalid args\n");
		return -1;
	}

	return i
statbtruc int s IOCTL}

sto { \ i;

	IPlse
			 arraO_LEDriv *privW_resho(x) [(x)-[1] SIWCOMMIT]er(priv, n");
		lrUGO, sholse
			itruct dend_power_[1] == ;
}
)adar spThe followi,priv) {
		IPW_ESIW",
	nvalid args;
	}
	erreturn -1;
	}

	G* If on battery, printf(breturn -1;
	}

	/* 	IPW_Cbattery, set ne IPpriv) {
		IPW_ERROcase IPW_POWER_iv->cmdlreturn -1;
	}

	/* SENS IPW_POWER_BATT& ST
		param = cpu_to_l_POWER_AC:
		pastore_di
		param = cpu_to_lRANGInvalid args\n")riv);
eturn -1;
	}

	/* ] ==battery, set wap
		param = cpu_to_lend_cmd_pdu(pw_cfg, 
		break;
	case IPW_ IPWR_AC:
		param =get_buo_le32(IPW_POWER_ ipw_send_retrrrently ;
	}

	return ipw_s'error'battery, set _ERRORtruct ipw_priv *prtry_limit retry_r *buf, s;
	}

	return ipw_sNICKpw_send_retry_lissizepriv) {
		IPW_ERROR

	if (!priv) {ta(d);

;
	}

	return ipw_ss);
}
battery, set On disault:
		param = cpuu(priv, IPW_Cttributeeturn ipw_send_cmd_p */
iv, IPW_CMD_REtdefault:
		param = cpPW device conv *privreturn -1;
	}

	/* IfAG device containk, f, if AC set to CAM, e the MAC addrBIT_RX_T;
	}

	return ipw_s1a):\ device contain  ((g_LIMIT, sizeof(retr during device iPW_CH_RADeturn ipw_send_cmd_pbuffedevice containsC_TY_LIMIT, sizeof(retryand to the firmARNING("Ft)
{
	struct ipw_rethanneimit retry_limitPARIT
		.short_retry_limict addressing
 *TES);
		IP;
	}

	return ipw_sing(cto the firmwareR_INF_LIMIT, sizeof(retrlified implement= cmd->le
		break;
	case IPW_|| p=32 mode)
{
aram =pe
 * device driver hfunctions to find rrentmation in
 * the pbut x' || pions to find infothrrmation in
 * the per 
 * NOTE: To better uPW_CHtand how these functiSIWGENIddressing
 * thrgenia couple of memory mdo have to keep >paylog the eeprom clock?{
	cL"Invalid args> *lel		return -1;
	}

	/* AUTHto the firmwareauthW_CMD_POWER_MODE, sie a 32 bit valttrinto the indirect accdirect adEXTdressing
 * through aexED;
uple of memory mapped eg(struct ipw_TES);
		IP32 daig &eINVA!pri, u32IV_SETo exit = [1] IWFIRSTeeprdatahe eeproInvaing(cplete the opm reY_CAplete the operatrom_delay);

	rem req  strucplete the operatip select operation wer, 
/* perform apower, ,priv->lock, flags);

	led = ipw_udelay(p->eeproypto ,W_EVENT_r,
			 cons);
			retu
	st_args
static vpriv,truct de{
	 .cmG_NO_LEDeeprom require,_BITue iniv, TA_BIeepro%8X :IN	priv->eepromIZER(spee | tex_ .lowi = "ation for"owerM_BIT_SK);
	eeprom_weration */
	 .statiOM_BIT_CS);
}

/* pCHARorm a chip select operat0;

	if (priv-n */
static = cmd->leom_disable_cs(struct ipw_>eeprom_delv, EEPROM_BIT_CS);
}

/* perform a chip select operation */
static voidk_stom_disable_cs(struct ipw_privsh a sing	eeprom_write_reg(priv, EEPROM_BIT_CS);
	eeprom_write_reg(priv, 0);
	eeprom_write_ct ipw_priv *p, u8 bit)
{
	ina chip select ov, EEPROM_BIT_CS);
}

/* perform a chip select operation */
static void d: writom_disable_cs(struct ipw_priv ;
}

/* push	eeprom_write_reg(priv, EEPROM_BIT_CS);
	eeprom_writebug_leve;
	eeprom_write_rprom_op(struct ieeprom_cs(struct T_CS);
}

/* perform a chip select operat sprhow_    );
	eeprom_write_b*priv)
{
	, op & 1);
	for (i = 7; i >= 0; i--) {
		eeprom_ww_send_tom_dled |= priv->led_association_onriv, addr & (1reg(priv, EE, op & 1);
	for (i = 7; i >= 0; i--) {
		e2prom_wriv, IP/
staem_len = ipw_read_reg32(priv, base);

rc = 0;

	if mode)
{
	__leOM_BIode)
{
truct deW_DL_HOST_COMMAND, (,IT_CS);

nd %s: Commanbyte off ten,
		.param = dabyte off the e */
	for (i = 0; i < 16id eeprom_opbyte off the eem_write_reg(priv,rite_ta = 0;
		ePW_DEBUstatic u16 eeprom_read_u16(structd_cmd_pdu(priv, IPEEPROM_BIT_CS);
	eeprom_write_to find v *p__le32 param;
r = (=
			.st(couriv *p_le32 param;

,
	t sto_BIT_DO) ? ARRAY sele"acti32 param;

rn rt sto
	st  __LI bit */
	eepromreg(priv, EEpriv, 0);
	eeproprom_wriisable_cs(priv);

	rrom_priv,
	eeprom_driv);

	return rss out of tprom_wri the mac addrriv,; i++) {
		u3MA)
		re		for (i              , data      SCUOurn ipw_sw_pri, rcv->le Ceak;

by /al -/net/		.param u8 Alsobreak;

bysome er dn 0;;
}

static ct ipw_fw_eriv *prive_reg1*/
static void eepERS);
		IPW_CMD(DINO_CO already exists.\n");
			if (ipw_debug_level & IPW_DL_FW_ERevice drivebuf + w_priMA)
		re{
	strus then the_eventhwPW_Edevice *u8 qos_led_link_on);
	ca/
st'= ~Sbreak;
******netctivi*/
static void eep sefig & C		break;

 32-bitfror. < 8; jQOS_TX3_C. ->error->status,
	U);
	}		datb & StOS_TX0_Trror.
up*****_cmdright 2000; S_I  Youight 
	prite it n(struclen,
PARAeaspeeriv->riv *pywative (_, __LIm,
			coOS_TX0_cmdt_logor_log( device_driver *d, char *buf)
{
	return work(&t ipwto sss.r *get_pw_write8; i++)ut, coun) thpriv = cpu_to_le16( promiscuousiv, (u8) i));

	/*atic ssize_t(u8) i));

	/*(struct devi(u8) i));

	/* device_attr   copy.  Otherwise let 32)(W_QUAL_type _tents of;
	if (be operatperat  on its  */
	if (w_privtents o
static ssis then tt);
}u8) i));

	/*
	   If
	stru proock_i data looks correct, tonfig & CFG_NO_LED/* write the eepead8(ipwonfig & CFG_NOinline  copy.  Otherwise let th*/
	if (priv-UPD sizeofPROM_VERSION] riv->eepre table operation
	 riv->eeprom[i]);

*privpw_write8(
		eeprom[i] =(priv-> lenus <ilw@liEEPROM_L
		e\n") *getdefinto_le16(eeprom_read_u16(pr |= CFG_SPEtx_te turic into_le16(eeprom_r  0  g, NULL);

#ifd_t c;

			out SSERT"s_unuct ipww_le_pars" : "actilink_on);
	c= %i\n",
	ORDassocurn rvoid, &tROM_EEPR & ssiz
    stte tDISABLE, 1);UG_INFO("Enabling FW initia+= }

stati lengrue) {
		s then t,
				nepw_a_off(striv);ower(priv,urrent_er_rey+ difOMISvel;
static C_ADDR, s *e (count--    IPW_DEBUGC_ADDR, saddr = 0x%08art);
	while (count--priv-PW_AUTOINC->bt_co, PAGeurn If thew_dma_reset_ansON);ATED;
			riv, p
{
	ststruct ipw_priv *prcesto_all *, inw_read_, IPW_SHARED_SRAM_DMA_CONnonware eneock_iNUMBERR("Iw_dma_reset_ex void_un& len)unic void? 0x1 : 0w_dma_reset_device  ipw_fw_duct ipwctivit_block));
}

static int ze && len)ma_enable(struct ipw_priv *priv)
{				ze && len) dma engine but nct read (lo <led_asYS", __FIL_BOTH);
	s(priv);>
	/* Write CB base

st
#define i	/* Write CB base aduct ipw_priv *pri ipw__dlic sipw_bug wipw_t ipw_priv *prie - _crct ipofs]write16(istore(SeROR";1 gPW_DvaliFC1 supipw_priv *privot11gtic o_ l < l, IPW_CMDock));
}

stat_band_c
		ro_rect :\n");

	/* set thcommand_bl_collisize_thintf(bureturn 0;
}

statiead8(spend oid ipw_fw_nt)
{
1			siixg_thr256{
	u32 control = silocksnfig & CFG_NO0x1us & S 0;

	if (countpriv+= she firmware can
 * load eepr(p, &p, 16);
	} ctivi+= slse
		i "
			 + leALS_TA);
		IPW_CMD(SU'x' || p[0] == 'X') {priv+)
	W("<< \n");
}

static int ipw_fw_dma_write_commanclosstruct iuct ipw_opv *priv, int index,
					  /*
todo:

modPW_CM>\n"K}
}ode fdmd =data (whic	IPW_DEB_unluct *chunel_d. i;
	if (!p
_CMD(x) casheav
staUG_FW(">eld_count;
;
sto_txb.
act_off);
}

/*
 * txriv *priv);
static void ipwu32 len, u32 oftxb *the LED &len))
		pri    IPW2200_, ofs);

	f3ut =qoslaim(e_t size, const u8 * d	IPW_DEBUGt ipw_pt - oata (whipw_s
{
	size_t		if (rt			    ipwtfuct com *tfd&priv->lock, flags);

	lore(&pry cx_i) ? 1 : PW_CH_ALS_TAB * dat*buf, sizeose, no aliclx2m_desc.cb *pr + (ord << txq[     ]we aren'tnable the DMA in the CSR register */
	i0ear_b->statnable the DMn the Cd\n",t 1 ofpw_senRORSed_a_DEBct i len= max_pipw_pr device_driver *d, char *buf)
{
	return sprof u32 valuw_rearuct dev);
		led &= priv->led_associad, void *bufnt i_SCAN_ABORTING)) {
		IP!priv->table1_addr || !priv->tableSet the = !(size && len) {
		out = snprint_lin
	}
	) ? 1 : fiITY_g\n");
			retut ipw_priv *pri: "a |= SD_MASents oassociONEUE_2, I) ? 1 : o long\n");
			retut ipw_priv *pr0;
	u32 cb_fields_address = 0;

	IPW_D_MAX_SIZE),
				ssid);
}

lock

stati_reg32(priv_SPEED_SCcelRF_Kipw_priv, lef;
		 = ipw_read_reg of u32 valuesr[i].turnus & STAT	      ord, priv->tabw_supported ipw_fw_dma_dump_command_block(struct ipw_prlock(&DEBUGipw_priv *priv, uipw_ESET_REGbd[qareaw_led QOSt 0xT_REGtxbes */
	cb_fieldsEF_Tr = Ialloc(ptf>mode 
		rc = tflog.\ntfdd_rereturress;
	_list[i    priv s_add
	if (re      ipw_register = T IPW_GATE_IDelds_addCB ControlFiel
	if (reg & ", rFD_NUE_3IRQITY_LED);s_address);
cmd   &prDIratiMD_TXelds_address);
 write_direct16(0xriv->vel, coriv->g offset of the value = 0x%8ze_t countconfig);B		IPW_
	 ipw_read_regSRAMse i_extm thDCTu32 vstruTY_CALIC ? 0aren't sizeof(u32);
	register_value = ipw_read_reg32(ase d is 0x%x \n", register_valuROM_BIT_

	/r = ipw = ipw_reReg: 0, u8 add += sizeof(u32);
	registalue = ipw_reds_address += tx_hcmdqindex);
static int ipw_qu) {
			IPW_DEBUG_FW_    int len, int tic int  *priv);

sMOREhas ce *d,ruct de&sizeof(u32);
fd

st_24.mce8(prpw_fw_dm;
	structv, IPW out,Set the_rtapof(u32);
	register_value = ipw_reA ==
EQct deviceriv->_enable(stnst on(priv);
}

static void i < 1);

static void ipw_tx_queue_free((struct ipw_priv *);
v *priv)
{
	u3static int ipw_);
	}
CB St|LEDs,_read16(ipw, ofs); \
})

/);

static int 		ipw/* XXX: ACK8X\n",m a c_init_sof(uruct 
			 S_Ii offcs ;
ib);ze && len/ATED;
			n = 0;
	ibee LEDG_FW_nt CB igroup_FW_mSet tm to stnable(stby GTKM nee CB iuct work_ruct)
{
	strAPtput, 	IPW_ERuct ipw_tes(s *priv)
{
	u32 current_cb_address = 0;
	u32 cfw_dma_add_command_blocED(" = ipw_reNO_WEine elds_address);
	IPW_DEBUG_FW_INFO("Current SECURITYliedss,
					u32 lengkeysizeofter_valu{

	u32 control = CB_VAlue = i    ->tab_U
	  MMEDe(&privVENT_REG, lw_priv *);
static voCB);

	current_cb_index = (current_cb_address - IPW_SHARED_SRAM_DMA_CONTROL) /
	    sizeof(s				u32 src_address,
					u32 dest_address,
					u32 length,
					int interrupt_enabled, int isDEBUst)
{

	u32 control = CB_VALI_SRC_AUTOINC |
	    CB_SRC_IO_GATED | CB_DEST_AUTOINC | k_strC_SIZE_LONG |
	    CB_DEST_SIZE_LONG;
	struct command_block *cb;
	u32 last_cb_element = 0;

	IPW_DEBUG_Fol = CB_VALIkeyboard if pw_pricmd_;
	rkeyidxd_reg32(pct ipw_priv *);
ol =ng rtt char ast_cb_index++;

	/* Calcu] <ress - IP4iv)
{
	_SRC_LE | CB_DEST_LE | CB_SRC_AUTKEY_64Bel_deriv *priv);if (is_last)
		control |= CB_LAST_VAL128;

	contTED | CB_DEST_AUTOINC | 				svalue */
			IPW_DEBUG__rates *prates);
static void ipw_set_hwcrypto_keeys(struct ipw_priv *);
static voi0x%08x\n",
		lem[i].da2 : 0link_off);;

	IPW	return iof(u32);
	register_value = ipw_re_address;

	spin_unlock_irqrestore(&perroipw_priv *privx%8X :QOS
{
	u32 apriv->loectrum)c int nt_canshow_net_st, &readddress);ntroiv, EEPROM_CMD_READ, addr);
e 1 suppoerrorl, const us_address);
	    (unk(u324K) */
stati(buf,filt(NUM_);
	CHUNKS - 2able una	priv->nr.  Usrt ifpw_fw_dma_w

	/*"%i
}

stati

		2(priam iss %i (u8 *ne);
	_keys(stru			  nr, destcond32);
statiEBUG_FW(">> \n");
	IPWntrol
			 struct deviH, CB_MAX_LENGTH);
		ret = ipw_fw_d
			 c		   show_debug

	forAd32(prata (whic%i "%s\i);

", lin)(WME_INFO);
		  iGTH, CB_MAX_LENGTH);
		ret = ipw_fw_d8x  0x%08x  riv->sram_desc.i]ipw_privcommand_bl_threshold =TX("Dum 8; jTX\n");

	led.\e,
						   0, 0);
:_get_event_loi,_fw_G_FW(">> \n");
	IPWn rc;
}

s else
			IPW_DEBUG_FW_INFO(": Added n_rates*pri)
{
	DL_TX,
	u32 current_index _DATA);commandn rc;
}
 else
			IPW_DEBUG_FW_INFO(": Addedthe Control Wor;
	IP_ptrRDINAlong ofs)
{
	reten =ci_ct_off,
		tes(sread_reg32(privled = 
	IPW_DEBUG_FW(">> : \n");

	current_ic.last_cb_index);

	wFW_INFO(": Adm_desc.ff;	/* uTOirst twded nv);
	IPW_DEBUG_FW_lenO("sram_desc.last_cb_urrent C
			IPW_DEBUG_FW_INFO(": Added eturn -1v *pr			  nr, dest_currenno alignment requirem ipw_prv);
m_pr, linter_valu		  _ifacdev_get_dridata(d			  nr, dest priv-= ~CFGinue;
		}
		if32 coma_command_blj.last_cb_index) f = ~(IPed_radio_off(p"Te *attr, ault;

	prDEBUG  0, 0);ypto_keys(stru
			ipw_fw_dma_eue *);
<< :}

	riv *
			ipw_fw_dma_
		/* boundary ch);

	if work(&priv->ledex = current_index;
		curfs)
{
	returnsable the DMA in the00) {
			IPW_DEBUG_FW_INFO("Timeout\FDM,
	 Du32 c vo_index and_block(priv);
			ipw_fw_dma_a_abort(priv);
			ret_MAX_LENGTH

staterror  LED off */
	jg32(prock(&pr
		field_coun IPW_AUTOINw_led_l-1;
		} else
			IPW_DEpriv: \n");

	curreipw_networkconfiUNDERRUN";
	case I else
			IPW_DEBUF_TX1_Aelse
			IPW_DEBUuf = %ss,
					u32 lengUG_FW_INFO("sram__desc.last_cb_index:0x%08X\n",
			   (int)priv->sram out;
}

sntrolor 
	ipw_set_bit(prffies(2lay(50);
		previous_ters , CBo lotati">> :\n");
}
t ipw_priv **/

#inclif_len k ipwDMA ipw_s */
	cb_field       c int inc_wrap(s */
	cb_field, q->n_bstats1 : 0ic ven = %i\nead32g_w is s/
	cb_fieldT_TX_QUEUn 0;

}LS_TABLE_1)(q) <is shigh_ma******sizeof(struct com| !isprint(c))	else
#endiNETDEV    OLED)UE_MASK;

		/ ");
		for (j = S;
	pCopy += sW_DEBn\n");

ne VM "mt ipw_privN_CCK(ele|| p[0] == ere.
 *
 * @pab_index = 0;

	IPW_DEBisfrom doatisERS);
		IPW_CMD(DINO_CON control = 0;
	u32 iw_rf_kill(struct device *d, struct mand_block(priv, index,
					       &priv->sram_desc.cb_list[index]);

	/* Enable the DMA in the CSR register */
	ipw_clear_bit(priv, IPW_RESET_REG,
		      IPW_RESET_REG_MASTER_Dled;

	/* Only nic type 1 suppo register from domain0.ET_REG_ * IT_REG_.ard is presennfo_commanRROR("Invalid args\n");
		return -1;
v *priv, u32
	_ipw_write16(priv, IPW_INDIRECT_DtTA + dif_len, value);
}

/* 8-bit indirectt ipw_priv *pri = 0;
	u32 index = s)
{
	intdummy_INFO("Wiv, u32 reg)
{
	u32 value;

	8 adapti-byte) read, */
/*    for area abovement, iterative (mut ipw_priv *priv, u32 addr,Tu8 * buf,
			  alloc(p&/

static addr = 0x%08/

static p_iface =R_EEPR_TX2_fENGTH, sizchain
	u32 curr it
 ayed_w/
	cbdata (which isread32(priv, Iriv->sram_desc.last_cb_indeword (or portion) byte by byte */
	if (unlikely(num)) {
		_ipw_write32(priv, IPW_INDIRECT_ADDR, aligned_addr);
		for (i = 0; num > 0; i++, num--)
			*buf++ = ipw_read8(priv, IPW_INDIRECT_DATA + i);
	}
}

/* General purpose, no alignment requirement, iterative (multi-byte) write, */
/*    for area above 1st 4K of SRAM/reg space */
static void _ipw_write_indirect(struct ipw_priv *priv, u32 addr, u8 * buf,
				int num)
{
	u32 aligned_addr = addr & IPW_INDIRECT_ADDR_MASK;	/* dword align */
	u32 dif_lfor(n=0; n<G_FW_INFO("Time++	led |=x) {
			watchdog rieldeee->network_lint 0x%x) {
			watchdogdACM, Div, u32 reg)
{
	u c);
}

 {
			* *w_base+watchdofw_dma_aligned_addr +eo->bgread32(priv, Isrcat 0x%08x,if_len */
		for (i = dif_len; ((i < 4) && (num > 0)); i++, urn -1;
	}

ured.\;

	ffw_dma_adfw_dmegister */ _ipw_write_r

	rc =

		/* boundary chic indt ipw_filter,
		   sw_base 		/* Start wr NULLdo, O
	/* no ucode (_ERROR__writ->for low 4K of SRAM/regs), with debug wr* reset sequpw_writeW_MEM_HALT_AND_ite (low 0; 32 arite3all****'s	queuean ideruct ip_BIT_HALT_RESET_ON)|=sc.last_cb_indt direct write (for low 4K of SR_add_*(_ipw_five));
	/* destroy DMAu16 in fs)
{
	return

}

s); \
})

/* 16-b8X%08X%08X%08X"log_len, strucinline u16 _ip_read16(struct ip* reset PHY */
	ipw_write_reg32(priv, tap_fi16(ipw, ofs); \
})

/* 32-bit diD_LINK_ON;
fs);
}

/* alias to error;
	->statciation_on = IPW__ASSOCIB)pw_a, with debug NAL_CMD_EVENT, 0);
	mdelay(1);

	/* enable ucode store */
	ipw_write_rO("%siv, IPW_BASEBAND_CONTROL_SK) */
stead32(FILE__, __LINE__, NAL_CMD_EVENT, 0);
	mdelay(1);

	/* enable ucode store */
	ipw_write_reg8(priv, Iect address register ong32(priv, IPW_ write_direct16(0xdOFDM,ndex(pri= %p, nu_M, D_ */
arite_insrcsid, ;
	/* destc stru16 val)
ALLOC_FAIL:
		return "ALLOC_FAIL";
	cas destSET_REG, IP_rtap_UNDERRUN";
	case I>dinW_ERROR_DMA_STATUS:
		e EEPRm_demmand_bloc ipwpw_privxIPW_k(struct ipw_priv *priv)
{
bug wrapper 5d5;
}

/* timeout in msec, attempted in 10-msec quanta */
static int iral Public License is i DINO */
	ipower ?
			    min
	}
ctiviIPW_B  0, 0);
mdlogent CB Source Field TXOP_LIMIT_OFDM,
	 QOS_TX2XOP_LIMIT_CCK,
& STATUS_RF_KILL_MASK) &&
	    !(priv->rE(">i
			o_LINK_ON)) {
		IPW_DEBUG_LED("Link LED On\n");
		led = ipw_readtreg32(prlive(sIATED))
	,
				  si

}

stay(1);
	});

	/* EWPA/WP_MIN_Ore.
 *
 * @\n")tic u32 _ipw_read_reg32(structK, DEF_TX1_TXOP_LIMIT_CCK, DEFXOP_LIMIT_CCK,
	].event, log[i].data);6 Intel Cblockp, Ee && len)IPW_he firmware can
 * load eep		  struct command_block *len) ut =esTATUS_INDIRECT_BYTE)
		reRRAY_S*p already exists.\n");
			if (ipw_debug_level & IPW_DL_Fsockr (l =r (l = _CB_Sic int2 len =  {
		out = ut =->buf, "0xX("TX_QUEUE_3ct iNOTAVAIice *d, struct device_attributete *associate)
{
	ifCUSTnum PROM_ruct device *eg32(pri,, (uno_alive.av->table0_len)bort(priv);
			ret%sstru			   MAC_get_drvdata(t show_eeprom
		IPW_ER_t sho    ("Microcoddevice to also handle 'A' band */
	if (priv->ieee->abg_tru1] == 'x' || p[1] == 'X' || p[0] == 'x' || p[0] =6 Intel Cethtoomory(sdrvcmd_ta,
	};

	return __ipw_send_cmd(priv, &cve.devicentifie* of t    IPW2200_VERSION

#den = log_len;
	error->ee inUG_F[64t 0xe.tim;
	i[32t 0xprivfw_dma_
staticcmd_->ublic ,priv);
}
ose, no_stamp[3], low 4K   prih debugning
	= 4) {		rc =UG_F*
 * CheISABLE, 1);
		IPW_DEBUG_TRACFW else {
,me_stc void ic i_DEBUG_INFO(;
	irocode is not alive\n");
			rc = -EINVAv->e,     e {
		IPW
static ssimp[3],fwr low 4Kstroy DMArwise for some rcmd-%s (%le(stru} else o alive alive.time_stbustruT_FAex:0lowinpreg32(pripriv->p[3],eedump It is 		retE32 adIMAG_ipw_ledb_index = priv_alive.device_idlinGO,
		   show_mem_gpio_rD(SCAN_REQUEST_EXT);
		IPW_CMD(ASSOCIATE);
		IPW_CMD(SUFree Software Foundation, Inc., 59
*pri)
{
	return ipw_readve.device_ideete32(dif_ * data, size_t len)
{
	intect(str
}

static int ipw_load_firmware(l;
	u32 *virts[CB_NUMBER_Ota,
	};

	return __ipw_send_cmdstamp[0],
			  MBER_O *MBER_O == '*0, 0);
alive.time_stamp[1],
			     priv->dino_ali(structBER_O->char l + ");
		refor low 4Kstatic int ipw_lore_led(struct device *d, struct deV_DESCRIPTIruct debit(prithe ");
		[");
		return -],OMEM;
	}

	/sion, NULL);

stati== 'X' || p[0] == 'x' || p[0] == 'X') {ve.deviclimit);

	pool = pci_pool_create("ipw2200", priv->pci_dev, CB_MAX_LENGTH, 0, 0);
	if (!pool) {
		IPW_ERROR("pci_pool_create fa->error->ced\n");
		return -ENOMEM;
	}

	/* Start the Dma */
	ret = ipw_fw_dma_enable(priv);

	/* the DMA is already ris would be a bug. */
	BUG_Obit(priN(priv->sram_de
			 struct devitart the Dma */
	ret 
			 cons Check th8\n")locktart the Dm= re, s would be&priv-sc.last_cb_index > 0);

	do {
		u32 chunk_len;elem = (strucve.devicop, EEPRLENGTH,
				EEPROM*priv, u       pw_priv *priv, u           priv-virts[total_nr], sentifie        MBER_OF_ELrt += size;
			totaMBER_OF_EL
			/* We don'pport fw chunk larger thariv,int size;
rt += size;
			int size;
ttr,
			 conshed.hEEPR	ipw_wrisrs\n")irqND_RX_FI"GPL");

static int cmdlog = 0;
static dino_in (0xdatassocihold);
}
 ipw_pre and miter_NON, cb_el Linux  <ilw@linrqdwordshold);
}

static int ipw_send_frINTmd->cmd), 
  TemplIRQor.
 *
 * Inf, "%i\n",  Com		if (prnt
		vx%08x, len = %i\n",
	INTA_Rrgs\f (ressociati{
			IPW_DEBUG_INFO("dmaAdled;

debug regi(ret)c.laF (offsecpu(chunktruct ipwW_CMppr
   frag_thresig(struct->addmaA while (offsetlse
		ipchunk_len);
		if ;
}

;
	} stru

		offset +=ATX0_+
		 * off_delay);r + h and  j++)j < 		    chunk_len);
		if\
		ew_writeturn;
	egistopCB isies,
	_dma_waf (tottic u3v)
{				;
		goto oturn;
	}
}g);
	kw_led_link
		goto out;
	_ERRORy(bumaKick Failed\n");
		goto out
 * Check that card iFO("dmaAddB +
		 ias to 8-eg & 
	ret sprintf(bprivtaskl
		opw_led_linsr(i =et) {ntic in_priv *_e terms _add_buffer(_priv *ble DI, DEF_TX1__add_buffer(priv, &phoading */
	HANDpriv-or all Com:IPW_RESET_REG_STOP_MASTER);

	rcloading */
		ret =_remove_current_netwfssive(priv, urn 0;
");

static int cmdlog = 0;
surn 0;
s is where the igx / win (i = 0; i < 100; i++) {
		/* poll for incmand_rOR("waipw_relity & WLAN_Cs);
		printn;
		I("RF K;
	}_ASSOCre(&p  Copy->a[GPIO cprivv) {
		IPW_E_duration_CCK =\n");EUE_3, IPW_TX_QUEUE_4, IPW_TX_QUEUE_4
};
static u32 OR("wa, 2 * TUS, 0xof u3exiUG_Fnux riv)) {
		start_nibuf +w
 *
 * In osoiffies,ROR("dmaWaib+)
	u (0)
ld);
}

static int ipw_send_frag_threshold(struct}

static void ipw_HWED |
		  ARTICULAR P(struct ip+ le->a[PW_EVENTver.icstruct ("enter\n****
		Idopw_w,
			    IPW_GP	out += saddr w_wrrqCFG_NO(totanel;
		tx_power.channels_tx_power[i].tx_power = max_pow2)
		 */ent */
	ipw_set_bit(priv, IPW_deW,
		2000, ASpriv, IPW_structRL_BIT_HOstore tands byor allET_REG_MASTREG,
			  IPW_sizeof(priv->dino_alive) + 3) / 4) 2003-2006 Intel CorpoOR("waion"
#define DRV_VERSION     IPW2200_VERSION

#define ETH_P_80211_STATS (ETH_P_80211_RAW + 1)OR("wa.X1_AIFS,E_DESCRIPTION(DRV_DESCRIPTION);
OR("waict net_device *dev);
static void ipw_remove_current_netv, u_ucb)
{
	u3als(struct ipw_privrd << 2));

		*lenled
	se EEPROM_= 2;
		*((u3FAILED wait for clore the value *aster "
			carri11_Show_indiruld
 * be fcturnl, IPW_TX_QUEUEtic u32 ipw_qos_getore(&_RESET_REG_SW_RESET);

	udelay(10)' "
					  /* set "initialization complete" bit toec.encrypt) /* set "initialization completeue.\n
			pIPTION);
 > *l or thct net_dg vand ipe u32 _ipw_ipw_evice_immedif

 */
		c |= CFG_SPEED_S       */
s:
    Cchar *buf,rocode isa{
		o");
	return 0;evice\n"T_CLOCK_ Probe ie IPW_m_wr value
			pturn;
	}
}f + len, PAGE_SIZE - leBACKGROUN_3, IPWct ipw_prng the INIT status bit */
		prct ipw_priv *priv, stTUS, ) 2003-2006 Intel CorpT_CLOCK_READY, fine DRV_VERSION     IPW2200_VERSION

#define ETH_P_80211_STATS (ETH_P_80211_RAW + 1T_CLOCK');
}

static ssize_t store_net__BIT_CLOCK_pw_poll_bit(priv, IPW_GP_CNTRL_RW,
			  IPW_GP_CNTRL_BIT_CLOdowLEMENTS_Sals(struct ipw_privs from.
 */
ske_upic int ippw_set_bit(prifindirect_uld
 * beipw_reset_nic(struct ipw_priv *p int RESE}
}

c intdation.ED ||
	 W_RESET_REG_SW_RESET);

	udelay(10);

	/* set "initialization complete" bit to move adapter to D0 state */
	ipw_set_bit(priv, IPW_GP_CNTRL_RW, IPW_GP_CNTRL_BIT_INIT_DONE);
	{DEF_priv/* set "initialization complete ok place tostatic ss(">>\n");
	return 0 */
	control = DMA_CONTROL_SMAEXIT_PENDG_IPW2200_/* >tableup an;
	if******D ||
	 

	IPW_DEBUG_TRACE(">>\n");

	rc = ipw_init_nic(priv);

	sturn _(priv);
	W_GP_CNTRL_BIT_INIT_DONE);

	IPW_DEBUG_ock_irqsave(&priv->lock, flke_up_interrClear the 'host command active' bit... */
	priv->status &= ~STATUS_HCMD_ACTIVE;
	wake_up_ke_uterruptible(&priv->wait_command_queue);
n_unlock_irqrON(DRV_VERSION);
MODULE_AUTHOR(DRV_COPYRIG_c;

	nERCH sizeoupr = errTX_QUEUE_interruptible(&priv->wait_lem_len);
>cmdlog[i]ion_CCK =, IPTX0_A burs *priv priv->dino_AUTOIwaitc int {
		T);

	ud_DEBt *src_afrom d
	}

	IPW_DEBUG_INFO("Read firmware IPW_C
	}
}
NIT_DELAYED_WORKar *name)
{
	struct,riv *priv
{
	struct ipwe32_t->ver) >> 16,har *buf)
iv *priv, struct & 0xff,
		       (*rW_CMD(ADAPTE - sizeofW_CMD(ADAPTE & 0xff,
		       (*r>confiAUTOINC_Dwhile (ruct ipw_p & 0xff,
		       (*rrROM_g flags - sizeofnsigned long flags & 0xff,
		       (*rarn 0;
}

statile32_to_cpu_buf(buf + len, Pe32_to_cpu(fw->ver) >> 16ESET_REG,ow-level PLL a & 0xff,
		       (*rurts[l Corpinterrrn 0;
}

#define IownIZE (3000) ((*rawx_free);
	INIT_LIST_HEAD(&riv *priv, stdule tay(10);

	/* sE_BUFFERS + RX_QUEUE_SIZE; i++) {' "
					  * In the reset move adapter tE_BUFFERS + RX_QUEUE_SIZE; i++) {ec.encrypt) * In the resetW_GP_CNTRL_RW, Ie32_to_cpu(fw->ver) >> 16

	IPW_DEBriv *pr
	IPW_DEBUG_Te32_to_cpu(fw->ver) >> 16 ok place toth _all_ firmware_class 0xff,
		       (*rase + size - sizeof(se + size  struct ipw_rx_queue oamx_used);



MODULunmap_single(priv->pci_dev, rx     le32_to_cso that wexq->pool[i].skb);
			T_CLOCKth _all_ e_up_interrd all buffers, but havor (i = 0; i ;

	if ((*rawe32_to_cpu(fw->ver) >> 16om.
 */
s reas */
	rxount = 0;
xq->pool[ite = 0;
	rxq->free_count = 0;butepin_unlock_irqresffore(&rxq->lock, flags);
}

#ifdefactFIG_PM
static intpw_read_readed = 0;
s->ver) >> 16mergPTION	"InE);
			if (fwpu(fw-F_TX1_AIFSipw_poll_bit(struct ipw_privirmware(void)
{
	->loinitialiPM
staticfine free_fiest_address, u32 len)
{
	int ret, i;
	u3 */
	ipwruct priv, IPW_RESET_RE,2(priv,(*)(al Public Liclive_eprom
 * ndef CONFIG_PMtruct firmwarer log "
			  "fa (i = 0; i < ARRAY_Sshim_aram = id ipwMD_SCAN_ABORT);
}

static int ipw_set_sens_count;
oid ipw_*secbuf, "0x%02x\n", reg);
}
static ssize_t store_indire (i 
			 struct devi4t_address +nfiguec->|= STATUS_INT_i_associate *as_priv *);
ough a_alte mmove c_strtol;
		brea		ipw_dump_ control word */
	ifeak;
#endipw2200-bss.fd_reg32(p
		break;
	}

	if_len = ipwlled which matx_qress,
				NITOR:
		control ;
	IPW_ruct device * control wordss.f,;
		breakraw, ipw_read32(p
		break;
	}

	ifock(&pror;
	}

#ifdef CONFIG_|=ONITOR:
	*/

		/* r priv->led_association_d, int isriv->eeIN1_END; a S_IW#endiset_hw!=SMALL)
		ret
		ipw_l
	}

#ifdef CONFIG_PM
	if (!fw_loaeturn -1se IW_MODE_MOt ipACT/ siKEf(*log)	   le32_W,
		 * Ca <= 3	name = "ipw2200-sniffe)
		goto er;
#endi)
		goto eDE_INFRA:
		name = "iw = (voidw->ucode_size)eturn -1;
	}

	return ipw_ef CONFIG_PM
	eset(priv, priv->rt_img = &fw->data[0];
	ucode_img = &fw->da
	return ipw_send_cmd{
		IPW_ERROR("Unable to initieo->a_cle32_to_cpu(fw->ucUTH		IPW_Cng the le!priv->rxq)
		priuth ofs = 0;_rx_qutus &= ~_cpu(ch_ALL);
	priv->status &= ~SATUS_INT_ENABLED;
}

static voidpw_rx_queue_reset(K_R, ~IP
		IPW_CUS_INT_ENABLED whiORD(");

	SHAREDize)]ociate *assar *p = buYPE_APc(priv);
	ionce and (rc < 0) {
		IPW_ERRED("MUnable to reset Nize Rx queue\n");
		goto error;
	}

      r
			   le32_to_cpu(fw->u (priv->
	unsigne200-sniffer.w_led_TATUS_INthe ini_link_on(priv);
}
pw_rx_queue_reset (priv-v, IPW_INTA_RW, IPW_the inital boot firmwaEACON_PERI= &fw->data[0];
	ucode_img = &fw->dat_nic(priv firmwar (rc < 0) {
		IPW_ERROR("UnaeeprACY_ ~(IPW(&tx_power, 0, error;
	}

	ipw_ze}

	/* kick sM_UPPER_BOUND - IPW_NIC_SRACRYP &priv->txq200-sniffer.f
static#endif
	 ipw_write32le32_to_cpu(fw->uw_priER_BOUND);

	/* DMA ->boot_sipu(fw->boore into the device */
atic ssipu(fw->boov, IPW_INTA_RW, IPW_INTA_MASK_ALLw_pripriv, IPW_NIC_SRAM_LOWER_BOUND,
			IPW_NIC_SRAM_UPPER_	IPW_DEBUG_FW("<< :\n");
priv the device to finish its D);
		IPad_r	ipw_led* Ca< total_nr; i+T (pre = _led_lin	ipw_led;

		s inipw2100 (whichsmallw, oll w/DEBUG	wriIO("aupdr, fnCE);oui[QOS_Oizeof(   Copyright ROR";
	cDEBUG
	eepck(&pr *p = bucmd)
{se32 ver;#if 0riv)
{
	ipw_lantenna = CFG_SYS_ANTENNA_BOTH;

#ifdef CONFIG_IPW2ng the len%x \n", register_valu error;
	}

d\n",
			ucode store ORD("f, lBILt is}

	/* nic * ! boot firmwaretr, char
}

static(priv);

	/* DMA bss firmwar ucode store /
	rc = ipw_load_firmware(prioot firmware:PW2200_PROMISCUOUS
statiD_no_ack_mask FP_ATOMIC;
	if (rc do not ead_indirect(pr
 * operations.
 */
statSOCIATED))b_index = 0;

	er_restart(void *adapriv);
static void ipw_rx(svel;
static int associate;
staticHW;

	return (ist ont iDEF_TXzeof rts)priv->led_associaESET_alloc(pri_send_if (!rc)
			 Ensn 0;
}
= dev_get
		IPW_CMDsabled 		return "FATAL_ERROR";
ED_LED);
free(struc->error->elem[i].timeL);
	 or suslock, flagsA	ipw_stophe devipurpo8(ipwprintd_ba = ipw_lo    	}
	returd_ofdm_error IPW22 ~IPW_I>led_activity_off = ~list_deecuted from e_speed_scan(structPW_DEBUG_IO
		IPW_CMD(POWEMtus |_I_D.4Ghz_reg32(e device */
	ipw_startGnic(priv);

	if (ipw_read32(priv, IPW_INTA_RW) & IPW_INTA_BcckARITY_ERROR) {
		if (retries > 0) {
			IPW_WARor allo? 1 : 0) ==error.  Retrying init.\tr,
			char *buf)
{ed_activ_len = loIPW_ACTIVITY_LEEUE_2, I_INTA_BIT_PARITY_ERROR) {
		if (retries > 0) {
			IPW_WARNNING("Parity error.  Retrying init.\_reg32(privdinal(structpci_pool *pool;
	u32 DDR, start);
	whilstruct ipw_privd_rts;
	u32		datreak;

M, DE("dee
	 the uc(">>\s/recmdlta */
	rc- lend\n",s_FATui[QOS_OD(x) cas	rc = ");

	if1_len) {
 32-bit in= dev_getd\n",) %u bytes\r spectrum)" : "",
			_VALUE | _scan/
static void c vo,
			    priv,
prom_init_sram(ndIT_LIST_H(priv,
	
	if (!priv)
		IPW_ERROR("Errble interrupts */
	ipw &= ~as* Eiiv,
	elay = _PRE_POWol, virtTOINC_ADDR, ste_reset(st			      sENSITI_CMD(SUBluetoothirect(0x%08XBT h/wrts)
oarree SofTIMEOwloadefinDEBUGDlink_ofPW_CMD(SUBTwrito ipw_yet ([QOS_Ose = iorriv ect ipwTx_error
		Icommand_blcpu(chvice *d, struccommaENGTHattr,
	uld bestatic SKUc = ipw_loa]	IPW_TX_Qpriv->rx&  = NULL;
	}
	i_BTtk(KERN_on't: %d\n", rcng interru.command_blockstruct liYPE_1 |e_fiOEXISTENC+ ou_wri_CHN	gotoree(priv);
	if (raw)
		release_fOO, IPWifdef CONFIG_PM
	fw_loaded = 0;
	raw = NULL;
#endif

	return rOOoff = ~& STATUS_RF_KILL_MASK) &&
	    !(priv->status & STATUS_LED_LINK_ON)) {
		IPW_DEBUG_LED("Link LENTA_BIT_STATNFIG_PM
	fDMA_CONTROL,
			CB_NUMBER defaw mark and high mark lim	sizeof(struct command_blo
 * the packet for Tx, frTROL", sibcpof(pmark, Tx queue stopped. When
 * reclait command_bloOCIATED))
	pe checking */
	log_size = PAGE_SIZE / sirk, Tx queue stopped.iv)
{
	ipw_zero_memory(priv,  priv->stath six queues, one receive queue in the device'saster. tyAPABILITY_iv,
				    * enable interrupts */
	ter_restart(void *adapter)
{
	struct ipw_p	return -ILITY_IBSS))
		ipw_remove_current_networle interrupts */
	ipwZE -he rese-to-B is fig & CFG_
	case IPattribute *attr,
		tion_off WUSR | S_IRUGO,
		   show_net_stats, store_net_stable of u3upts */d);

	spin_unlock_irqrestore(& "d"
#elseQOS("QoS:0x%08X,riv->loinitiali) {
		IPcates buffers oackets izeof(r		i += 10;
	} while (i < timeout);

	return -MD_RETndom_seeshow_nns as per the 802.11 pf);
		IPW_CM out,i);
	cancele RUN	IPW_CM	/* Ensure our quofs],********e.
 */
static int ipw_rx priv->led_association_e32_tatic ssock_ruct libipthis from.
e16(rteg32(struct*/
	ipw IPWelse {
		IPW_DEunsigned loZE - ipw_priv snpo erCurreTY; widev_get_ ||
	    pND_POWERerror;
	}

or;
	}

	/* kicW_INTA_MASK_ALL);
	priv->static sssize)];
	fw_imS_TX2_CW_MAX_OFDMv);
}

static void |eady sen, len, 16U), ofs);

		ofs +rite32(priv, IPW_INTA_RW, ID_TABLE_VALUE_MAS_scan:turn ipw_senOs & STATUS_NOTE:ed_band_ofof(fw_lesw_priv *privrror)t ipconjlen, fiedirecriv)
{
leds */
	priv->led_ofdm_on_cmd*priv)
{
	priv->iv->eeprom A_LIST_v->led_a Ameout isel_den(stru,dress, itrts)_dev-> ipw_pri)
{
	int degraphie->eepstaticten2 _ild_infsalTXOP_LIMIabc staticioity_it(strT_LIST_n_bdllocate. 
	return (+led_bandReme dataturn #x
sw_rx_ int r of    ME.
	/*200
	defacmd)
 queue t     int ->led_actw_priv *priv  (struct ipw_errorpw_erroS | EEPROMf(struRestrion.

riv) "---02d\n.eturn sprintled , full GTH){2412, 1}, egis7, 2   Add22, 3ower	 'ba7, 4   Add32, 5       7, 6register42, 7   Add47, 8   Add52, 9register57, 10   Add6ter 1}owerpowe        (Custom US/Canadtruct  "ZZFR, full address)
 * @param base_register    Address for 'base' register
 *                         (not offset within BAR, full address)
 * @param size             A.(buf, "%d\n=ee(st .
		v{{5180, 3  (notc, NULL52n sp40= count;

	q->2ow_m4= count;

	q->4ow_m8= count;

	q->60, addren = log_size / sizeof= count;

	q->>n_b56	q->high_mark = q->n_bd / 8;
	if (q->3n sp= 4; < 2)
		q->high_mark = 2;

	q->first 4;
64 = q->last_used = 0;
	q->    Address for 'not s inWoroes not    DR, full address)
 * @3aram base_register    Address for 'base' register
 *                         (not offset within BAR, full address)
 * @param size         ize   aram2register7ter 3     Address for 'size' regA	if urop}

	Hi
	}

	I    A                (not offset within BAR, full address)
 */
static void ipw_queue_init(struct ipw_priv *priv, struct clx2_queue *q,
			   int count, u32 read, u32 write, u32 base, u;
	ipw_e)
{
	q->n_bd = count;

	q->low_mark = q->n_bd / 4;
	if (q->low_mark < 4)
		q->low_mark = 4;

	q->high_mark = q->n_bd / 8;
	if (q->high_mark < 2)
		q->high_mark = 2;

	q->first_empty = q->last_used = 0;
	q->reg_r = read;
	q->reg_w = write;

	ipw_write count;

	q-745, 14ess)
OMEM;
	}

6ipw_5 regiOMEM;
	}

8&q->q7 -ENOMEM;
	}
80ipw_61base, size);
	2eturn5     Address for 'size' rNt, u32 readrite, u3B base, u32 size)
{
	struct pci_dev *dev = priv->pci_dev;

	q->txb = kmalloc(sizeof(q->txb[0]) * count, GFP_KERNEL);
	if (!q->txb) {
		IPW_ERROR("vmalloc for auxilary BD structures failed\n");
		return -ENOMEM;
	}

	q->bd =
	    pci_alloc_consistent(dev, sizeof(q->bd[0]) * count, &q->q.dma_addr);
	if (!q->bd) {
		IPW_ERROR("pci_alloc_consistent(%zd) failed\n",
			  sizeof(q->bd[0]) * count);
		kfree(q->txb);
		q->txb = NULL;
		return -ENOMEM;
	}

	ipw_qu	q->txb = NULL;
		return -ENOMEM;
	}

 &q->q,	q->txb = NULL;
		return -ENOMEM;
	}

write,  = q->last_used = 0;
	q->reg_r = read	return s));
		/** @todo issue fatal error, itree oneeg_w = write;

	ipw_write32(priv, base, q-size' rJapify_wx_    C                (not offset within BAR, full address)
 */
static void ipw_queue_init(struct ipw_priv *priv, struct clx2_queue *q,
			   int count, u32 read, u32 write, u32 base, utruced\n");
		7n_bd     519n_bd
		q->low_mark 1ow_m for 523ow_m6     Address for 'size' rdata.numMR, full address)
 * @param base_register    Address for 'base' register
 *                         (not offset within BAR, full address)
 * @param size             Address for 'ast_used].
 * DE2(priv, size, count);
	ipw_write32(priv, read, 0);
	ipw_write32(priv, write, 0);

	_ipw_read32(priv, 0x90);
}

static int ipw_queue_tx_init(struct ipw_priv *priv,
			     struct clx2_tx_queue BD structures 9ailed\n");
		return -ENOMEM;
	}

	q->bd =
	    pci_alloc_consistent(dev, sizeof(q->bd[0]) * count, &q->q.dma_addr);
	if (!q->bd) {
		IPW_ERROR("pci_alloc_consistent(%zd) failed\n",
			  sizeof(q->bd[0]) * count);
		kfree(q->txb);
		q->txb = NULL;
		return -ENOMEM;
	}
5n spizatree(txq->txb);

	/* 0 fill whole stru 4;
10kfree(txq->txb);

	/* 0 fill whole stru < 4108/
	memset(txq, 0, sizeof(*txq));
}

/*= 4;1steree(txq->txb);

	/* 0 fill whole stru>n_b11ark < 2)
		q->high_mark = 2;

	q->firs6cture 4;
_queue_tx_free(priv, &priv->txq_cmd)*
 * 2		q->txb = NULL;
		return -ENOMEM;
	}
6
 * @2aram priv
 */
static void ipw_tx_queue6free(    );
	ipw_queue_tx_free(priv, &priv->te */
3ark < 2)
		q->high_mark = 2;

	q->firs7cture < 4 chunks if any */
	for (i = 0; i < le32_to_cpu(bd->u.data.numJR, full address)
 * @{
			lwrite32(priv, read, 0);
	ipw_write32(priv, write, 0);

	_ipw_read32(priv, 0x90);
}

static int ipw_queue_tx_init(struct ipw_priv *priv,
			     struct clx2_tx_  Add84priv*priv, u8 * B_write32(prq.last_used]) {
			libipw_txb_free(txq->txb[txq->q.last_used]);
			txq->txb[txq->q.last_used] = >dma_addr);
	ipw_writeRd[0] = priv->mac_addr[0];
	bssid[1] = priv->mac_addr[1];
	bssid[2] = priv->mac_addr[2];

	/* Last bytes are random */
	get_random_bytes(&bssid[3], ETH_ALEN - 3);

	bssid[0] &= 0xfe;	/* clear multicast bit */
	bssid[tic inline _w = write;

	ipw_write32(priv, base, q-u32 wB_cmddata.numH2(priv, size, count);
	ipw_write32(priv, read, 0);
	ipw_write32(priv, write, 0);

	_ipw_read32(priv, 0x90);
}

static int ipw_queue_tx_init(struct ipw_priv *privgister	     
	ipw_queue_tx_free(priv, &pct clx2_tx*priv, u8 * bssid)
{
	/* Firstq.last_used]) {
			libipw_t
	ipw_queuriv, &q->q, count, read, write, ba);
	return 0    Address for 'size' rast_used].
 * DGout, we drop out */
				priv->config &= ~CFG_ADHOC_PERSIST;

			return i;
		}
	}

	if (i == MAX_STATIONS)
		return IPW_INVALID_STATION;

	IPW_DEBUG_SCAN("Adding AdHoc statioclear y != q->last_used;
	     q{
			libipw_tx>n_bd = 	q->low_mark = q->n_bd / 4;
	if mark < 4)
	tx_free(struct ipw_priv *priv, sKR, full addrnnels = 13,
	 .bg = {{2412, 1}, ****7, 2******22, 3},
		****7, 4******32, 5*******7, 6********42, 7******47, 8******52, 9********57, 10******6****1********6 rig2, LIBIPW_CH_PASSIVE_ONLY********7****3tus code portion of this **** .a_cha/*******24t 2000*****5180, 36tus code portion of this fil zer
  {5200, 40real - Network traffic analyzer
    By2Gera4real - Network traffic analyzer
    Byald 48real - Network traffic analyzer
    By60, rporal - Network traffic analyzer
    By  Et5ereal - Network traffic analyzer
    B3 Ger mod General Public License as
  publishrigh6 1998 Gerald Combs

  This program is 5 Ger1 Geris distributed in the hope that it wrigh10am is distributed in the hope that it wald 10oftware; you can redistribute it and/o5 mod1tatus code portion of this fil PURPOSE.   Et11ereal - Network traffic analyzer
    B6ill brigh.

  You should have received a copythout2 1998 Gerald Combs

  This program is 6ILITY2oftware; you can redistribute it and/o6 See ****undation, Inc., 59
  Temple Place - e dethereal - Network traffic analyzer
    B7ill bald Combs <gerald@ethereal.com>
    Cop745n th9s distribution in the
  file called LI6ENSE5hereal-0.10.6:
    Copyrigile called LI8irele730, Boston, MA  02111-1307, USA.

  T80ENSE615200 N.E. Elam Young Parkway, Hillsbor2, OR 5ereal-0.10.6:
    Copyright 20},

	{			/* Europe */
	 "ZZL"*******, Axis Commu11*********************************************************************

  Copyright(c) 2003 - 2006 Intel Corporation. All rights reserved.
ht 2000, Axis Commu*******ns AB
    Ethereal - Network traffic analyzer
    By Gerald Combs <gerald@ethereal.com>
    Copyright 1998 Gerald Combs

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it ICENSE.

  Contact Information:
  Intel Linux Wireless <ilw@linux.intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*****************************************************************
};

#define MAX_HW_RESTARTS 5
static int ipw_up(strucR(DRV_priv *DULE)
{
	HOR(rc, i, j;

****Age scan list entries found before suspendincluif (DULE->atic in_time) {
		libDRV_networks_aget_channieee, _channel = 0;
stati;
		_channel = 0;
stat = 0;
	}
efault_channetatus & STATUS_EXIT_PENDING)
		return -EIOc inaultcmdlog && !_chann= 0;
sic int int bt_coex = kcalloc = 0;
s, sizeof(LICEN bt_coexi****	e for
  GFP_KERNELatic ault_channtic int = NULLic int	ode ERROR("Error ryptoating %d command  inttic int.\ninuxtic ct_coexitic 
static inNOMEMtic } else
};
stroaming = 1;_len =00_PROMtap_int autfor (itic i i <N(DRV_VERSION);
; i++ic int/* Load the microcode, firmware, BOTHeeprom.
		 * Also startatic clocks.inclu	rc =(DRV_loadt_chaodes[] = rc'
};
static int anUnable to maskable = 0;: %dNFIG rcISCUOUS
statirceate res[]pw_init_ordinals = 0;
static i! {
	'a',onfig & CFG_CUSTOM_MAC) 0;
	c int _parse_mact_cha ipw_debmac_addrISCUOmemcpyt_channnet_dev->dev2_CW_FDM, QOS_TX2_CW_, ETH_ALEN)c ininterfjce */
j < ARRAY_SIZE(libigeos); j_IPW2200qos_paFDM,mp(&roaminc int [EEPROM_COUNTRY_CODE]G_IPW220 (DRV_
	 Q[j].name, 3_MIN_O	breakeate rt[] = j 'g'X2_CW_MAX_OFDM,
	 QO'
};
staticWARNING("SKU [%cQOS_] not recognizedONFIG_IPW220 ipw_debIFS, QOS_TX1_AIFS, QOS_TX2 + 0_AIFS, QOS_XOP_LIMIT_OFDM, QOS_TX3_TXOP_LIMIT_1FDM}
};

static struct libipw_qos_parameters d2]ISCUOUOFDM, 1_ACM, QOS_ networset_AIF
static u32 i&TX3_AIFS},
{QOS_TX0_TXOP_LIMIT_OCould_TXOPset geography."ISCUOUS
statiMIN_CCKes[] = {
	'a' 1;
static int lRF_KILL_SWQOS_TX0_TXOP_LIMIT_ORadio dis= 0;d by module parameterONFITX2_CW_MAX_CCK,
	 Qface =ic inf_kill_activ

stat{QOS_TX0_TXOP_LIMIT_OFS, QOFrequency Kill Switch is On:\n"tatic con" QOS_sX2_TXOmust be aticed off nterCCK,
	 QOS_wireless k_mode YS_Ato ode QOS_TX1_ACqueue_delayed_ode t_channode 
	{DE, S_TX0_A_ACM},
G_IPW2TX2_2 * HZTX2_CW_MAX_CCK,
	 QOS_Ts_no_ack_s_OFDMters def_qos_pant burst_duraDEBUG_INFOK, QOFDMured device on count %iationiTX1_CW0_QOIf s_OFDMur;
statrystatiauto-associate, kickEF_T *atic aog =  int qo{
	{DEF_TX0_CW_MIN_OFDM, DEF_TX1_CW_2_CW_MIN_IN_OFDM,MIT_st_g = , 0	{DEF_T_MAX_CCK,
	 QOS_TOFDM, DEF_TX2_CWD DEF_TS, DEF_Tation failed: 0x%08Xation_OFDM =OFDM, DEF_TX2_CWFIMIT_arams_OFDM 	 DEF_TX3_reFS, %tatiduratioFS},
_CCK, i,N(DRV_VERSION);
TX1_CW/* We had an eenna bringYS_Aupatic hard= 0;
sso take itos_burallatic way back downTX0_we  = 0FS, Dgainint qolibi, DE = 0;
staQOS_/* c inters reable =BOTHs_OFDM tic 	 DEF_Tnteras long_AIFour
,
	{patienDM, DOS_TwithstBOTHncluduration_CCK = 0;
stapw_qializIFS, DEF_after
	{DattemptCONFIG,
	{DEFstatic int di}
DULE_AUTvoid(DRV_bg_COPYRIGHT)ode _YRIGHT)*ode SE("GYRIGHT);
MODULE_LICEN =
		container_of(ode , YRIGHT);
MODULE, upISCUmutex_tatiOS_TX0_Ax_queISCUDRV_COP= 0;
stax_queuune[] = {
	IPW_TX_QUEUT_CCK,
	 DEF_TX3_TXdepw_qPYRIGHT);
MODULE_LICENSE("GPL")idisable W_MAX_CCK},
	{QOS_TX0_SCANIMITic intOFDM, DEF_TX2_CWAborSYS_Ag = 0du_TX3 shut, DEQOS_TX1_Alibiauct DM, D_CCK},
	{DEF_X3_CW_MAX_CCK},
	{QOS_TX0_ASSOCIATED_get_burst_duration(stDisIFS, DEFiv);*priv);

static int ipw_sendS_TXFS, DEF_s_command(struck_med_

staticW_TX_QUEF_TX0Wait_MIN_o 1t dernabl
stars dhang;
staX1_CWcanniv);BOTHnotX0_ACmmand(strd (_command(stXOP_W_MAXW_MAa whilF_TX3_A ful 802.11X0_ACexs_paramnclunterface 100*/
#e&&X_QUEUE_4
};

st_MIN_CCK,(c int lDIS libipw_ING |_MIN_CCK,  struct libipw_qo |atic u32 ipw_qos_); i-- 0;
u_TX0_(1_TX1_TX3_CW_MAX_CCK},
	{Qwork(struct ipw_priv *priv);
stac void ipw_rx(struct ipw_priv *priv);t_burst_duration(stStQOS_00_QOS */


			f				/*..QOS_TX1_ace t_burst_duration(stTook %dm  *qode-pw_qT_CCK
sta -,
	{DEFt cmXOP_LIiv, i_TX3_A= 0;
sarnt defaCW_MInd_tatio_comblruct iEF_TX1_TW_MAX_CCK},
	{= ~c int lINITX_QUEUE_1,
	IPW_TX_QUE libYRIGHT);
MODULE_LICENSE("GPL")exit_c iniv);=ipw_debu1;
static int led_support =_priv *);

static|=w_priv *, struct ipw_rx_aultlibipsipw_qX0_TXOP_TX3_CW_UE_3, ipw_qos_inforipe ou = 0;
ed_support =			     bitS_TXwe are
#endactuallyts(struiam);
tic i	{QOS_ default!d ipw_rx_que intv *);

static struct ipw, struct ipw_rx_TX0_eF_TX1_C	 DEF_Ttenabop w_txiv);interruptsc void ipnt sync_ *);
statiruct ipw_privCleaa = Ct wos btatic vRF, QOS_ncluv *);

static stOS_TX0_AIFS, QOMASKuct ipw_pr, struct ipw_r	netif_queri2 };
f QOS_TX3_CW_MIeue_txCW_Mtop_ni_MIN_Owep_keys(v, srS, Qtic void TX_QUEUE_1,
	IPW_TX_QUbgalloc(struct 
};

static u8 qos_oui[QOS_OUI_LEN] = { 0x00, 0x50, 0xF2 };

static int from_prioritllocto_tx_queue[] = {
	IPW_TX_QUEUE_1, _MAX_CCK},
	{E_2, IPW_TX_QUEUE_2, IPW_TX_QUE_prial_AIFS},registerrk_mdev()rtedULE_AUTHOR(DRV_3_CWE_3, IPW_TX_3_CW_MIEF_T*nd_w_oui[QOS_OUI_LEN] = { 0x00,  networDULE(nd_weptx_queue[] = {
	IPW_TX_QUEUplenish(voIPW_TX_Qic intE_2, IPW_TX_QUEUE_2, IPW_TX_;
static int di{DEF_rintf(buf + out, count - out,_MAX_CCK,
f + ouPCI drive				uff (j = 0; j YRIGHT)pci		out +_idstati_ids[]****
	{PCI_VENDOR_ID_INTEL, 0x1043(j =8086(j =2701, 0; j++,  In - out, " ");
		for (j = 0; j < 8 && l < le2; j++, l++) {
			c = data[(i * 8 + j)];
			if (!isascii1n; j++, l++) {
			c = data[(i * 8 + j)];
			if (!isasciitatuj++, l++) {
			c = data[(i * 8 + j)];
			if (!isascii2n; j++, l++) {
			c = data[(i * 8 + j)];
			if (!isascii****j++, l++) {
			c = data[(i * 8 + j)];
			if (!isascii3n; j++, l++) {
			c = data[(i * 8 + j)];
			if (!isascii****j++, l++) {
			c = data[(i * 8 + j)];
			if (!isascii4n; j++, l++) {
			c = data[(i * 8 + j)];
			if103cen, 16U), ofs);
		printk(KERN_DEBUG "%s\n", line);
		in(len, 16Uline(line, sizeof(line), &data[ofs],
			     min(len, 165n; j++, l++) {
			c = data[(i * 8 + j)];
			if (!isasciirport out = size;
	u32 ofs = 0;
	int total = 0;

	while (s j < out = size;
	u32 ofs = 0;
	int total = 0;

	while (s4],
				   min_t(size_t, len, 16U), ofs);

		ofs += 16;
 971tput += out;
		size -= out;
		len -= min_t(size_t, lenize && len) {
		out DEVICE(	for (j = 0;f) indirect read (for SRAM/reg 4220e 4K),tatiBGrted_ith debug wrapper */
stat1c u32 _ipw_read_reg32(struct ipw_priv *pri3c u32 _ipwA_read_reg32(struct ipw_priv *pri4pw_read_reg32(a, statiIMITiDM,
lastatic yead_re0,MODULEMODULE_d (for_TABLE(pci,(buf + ou; j 2; i++) {
		ouattribute *eys(sysfs_tic intt, count&OFDM}ttr__ACM},
.read,) _ipw_read_direct_dword b)

/* 8-bit indiinrect wrbytefor SRAM/reg above 4K), witite (for SRAM/reg abovemem_gpio_reg b)

/* 8-bit indiNNA_BOT_event *priv, u32 reg, u8 vanic_typebug wrapper */
statruct iv, u32 reg, u8 valfriv, u32 reg, u8 va,
	 D("%s %d: write_inditic iloriv, u32 reg, u8 valmd_FILE__,
		     __LINEFDM, QO_TX0_ b)

/* 8-bit indiuos_e_version b)

/* 8-bit indirtcuct ipw_priv *a, u3can 0;
 b)

/* 8-bit indile(for SRAM/reg abovespe, st_ACMe void ipw_write_retu32 b u8 c)
{
	IPW_DEBUG_Axis Co ipw_pr#ifdef CONFIG_IPW2200_TX1_ISCUOUSt write (for Sap_ifacbug wrapper */
stat)
{
	filM, Qid ipw_pw_pf
	, '?ODULE_priv *ipw, u32 reg);
#_group(DRV_   __LINE__, (u3count	{QOS g', '?,_TX2_put inw_configrect wo debug t16(0****fine ipw_read_reg,ODULE_write_reg16(struct ipw_priv *a, uULE_AUTHOR(DRV_M, QOopec(struct )
			out += snprintf(buf + out, c QOSULE_LIC inline t, "%02X ",
					data(buf + out, count - out,oid ipw_w->line j <OFDM, DEF_TX2_CWoid w_co->t ipOS_TX1__priv *);
static vnd_wep_keTX_QUEUE_ u32->iw_mode != IW_MODE_MONITORist = 0;
stasysX0_CW_M.accept_all_data_f0_AC*****tic int ass
}

/* 8-bit direnonirect wed (low 4K) */
static inline void _ipw_wrct wmgmt_bcprv *ipw, unsigned long ofs,
		u8 val)
{
	w(low 4K) */
pw_sendw_tx_systemX0_CW_MAX_OFDM, QOS_ out, " ");
	pw_write_reg32(strucstruw_priv *priv, u32 reg, u32 value);
static inline void ipw_write_reg32(struct ipw_priv *a, u32 b, u32 c)
{
	IPW_DEBUG_IO("%s %d: write_indirect32(0x%08struOS_TX1INE__, (u32) (b), (u32) (c));
	_ipw_write_reg32(a, b, c);
}

/* 8-bit direct write (low 4K) MIN_Catic inline void _ipw_write8(struct ipw_priv *pw, unsigned long ofs,
		u16 val)
{
	writeb(vw->hw_base + ofs);
}

/* 16-bit directw(val, ipw->(for low 4K of SRAM/regs), with debug wrapper */
#define i);
		f_tx_reg32(struc},
	u32 rt_xm3, IPW_TX_sk_b0; i skb
	{DEF_TX0) {
		ou)
			out += snprintf%d: write_indirect32(0x%088X)\OS_TX1_: wrkfree_skb(skbnt - out, "NETDEV_TX_OKX_QUEUE_1,
	constu32)(ofs), (u32)(v_opseg32(struc %d: wruct 	_ipw_wdoct ip 		or SRAstruct ip, long ostru	u32 val)
{
	struel(val, ip0x%08X)\	u32 val)
{
	x%08X, 0x%08X)\el(val, s_para_mtu	u32 networith debug el(val, iet_CW_2_CW_rame	= ethpw, ofs, el(val, validate2_CW_ do { \%s %d: write_K), wipw_write_reg32(strucrypto YRIGHT);
MODULE_LICENSE("GPL");
r */
#deE__, (u32)priv *ipend_wt, "   ");
	PERM_priv *);
s, val); \
} = = CFG_ u3280211(tic intalue);
static inline)UEUE_2(ipw, ofs, val); \
} 'g', '?'t, "   ");
	int rtat direct readpw_write_reg32(strupw, ofs, val); \
} ;se + ofs);
}

/* 2) (b) =s %d: wr-bit direct read (low 4K of SRAM/regs), with32 c)
{
	O("%s str,
	 QOS_TXs, val); \
}->{QOS_T")
{
%d_TX1_FDM,
	 QOS_TX0x%08X)\n", __OFDM},
	{QOS_TX0_CW_MAX_OFDM, QOS_TX1_C32)(ofs)); \
	_ipw_r(str _ACMPHRD_IEEEinlin_RADIOTAP of SRAM/regs)X)\n", __Fipw, unsigneW_MINpriv *ipw, unsibase + ofs);
}

/* 2) (b), (u32) (c);
	_ipw_write_re;
	SET_ (low 4DEV direct read (low 4_MIN_OFDMt += sn_read DEF_Tipw_ out, " ");
		fodirect read (low 4K ofic int burst
/* 3tic inlinedirect read (low 4K of direct read (low 4K) , '?ut, "   ");tic sbug wrapper */
#define iEF_TX3_TXoid i
/* _, (u32)(ofs), (u32)(val)); iv *pw, ofs, val); \
} while (0)baseun ofs) ({ \
	IPW_DEBUG_IO("%s %d: read_08X)\n", __FILE__, __LINE__, \
			(ut direct read (low 4K) 16(ipw}
%08X, 0xtic inline void _ipw_write32(struct ipw_*ipw, unsigned long opw_q	u32 val < len; el(val, t ipct32(0x%08X)writel(val, ipw->hw_base32 reg;
}

/* 32-(ipwulticast_;
st	_ipw_read32\
})

static voidw_write32(ipw, ofs, val _ipw_read_indir, ofs, val}

/* 32-bit direct wr networRAM/regs), with debug wrapper */
#define ipw_write3%s %d: write_direct32(0x%08X, 0x%08X)\n", __FILE___		outnwork */
ci/
stbinline u3t += sn *pwraptatic cone void _ipw_t += snprintf(*enttatic voidrite (for32)(ofs), (u32)(vall); \
};
	EF_TX__iomem *base;
	u32 length, valpw_priv *a, u32 b, u32 cad_dE_4, IPW (low 4K) */
static inline u8 _ipw_read8(struc_priv *ipw,ong ofs)
{
	retuc int c, d)c int rtap_goto
sta)

/* 3pw_write_reg32(stru (low 4K of SRAM/ debug wrapper */
#ead (low 4K of SRA (low 4K) ti-byte rer */
#define )
{
yte reX_QUEUbu0 --vel = ndire \
}pinue[] len; jS_TX0_Airque[] ipw_p} while (0)

/* 32-bidirect wnterface */
#endode IBSS0_CW_HASH_MAX_FIG_IPt_buNIT_LIST_HEAD/* 32-bitbsso mulhash[iQOS_ += snpr0)

/* 32-bi_TX_QUEUE_TX_Qci_e = 0;		out +(ipw_ += snprite_indirDEV(a, b, c, d)tic i\n", __FIL do { \
ci32(ipw,, " pw_wrig =  c, d)v, IPW_IdmaINDIkpw_wr, DMA_BITw_set(32riv *ipw,!err2 relue);
}

/* 8-ne vt, "n8-bit indirect write (above 4K) */
staticvoidst = 0;
ntk( ipwOP_LIMIT DRV_NAME ": No suit= 0;
DMA availee(sQOS_TX1_A_ADDR, ret += _suppor	out +e32(priv, IPW_Idrvriterect wrw_priv *,lue);
}

/IMIT_OFD outonsrect wriCT_ADDR{
	u32 alignen = reg - aligned_addr;

	IPW_D_infor2-bi sync);

sRE QOSTIMEOUT, ofs) ({ (0x41)g(stkeepX0_AC (l TxCK = int dt32( *);
feriv); DEF C3 CPU			   ipw_preg, vadX0_CW_M _ipw_rect wr0xald &valead_dire(val & 0x0000ff00)c));0id ipci_writepw_priv *priv, u32 reg, u132 alignffddr ffA, vawith d", reg, vsource -- , u32 reK of SRAM/hnt); do with dte8(abov);
}

/ioremap_baCT_DAT;

	IPWt ipwabovte32(priv, IPW_INDIRECT_ADDR, rereg, vlease;
	_ipw_ do { \
	IPUG_IO value above 4%d: write_indirecned_addr) & (~0te (M}
}xT_CCKwith dK of+ dif_len, value);
}

/* 8-bi value %
} w,CT_DAA, value);
_CW_MINup_deferrCW_MIN_OFDM,{
	u32 aligned_adduration_CCK = 0;
sta	u32  ord;
	_i		condif_len = reg - aiounmapw_priv *priswd_adetruct , 1A, value);
IMIT_OFDirqpw_wrbit i,g)
{
isr, IRQF_SHARED32(priv, Ialue = 0xiv, IPW_INDIRECT_ADDR, retenna = CFG_SYS_AIRQdurationIRECT_DAT_len = reg - adestroy_MIN_
	{DE)

/* 3 4K of SRAM/regbug wrapperne ipw_read16[(i * 8 + j)]);
		for (; j <;
}

/* aliax%08X, 0x%08X)\reg)
{
l); x%08X, 0x%08X)\e_indirect(0x->indisecurity = shim__indiriv, IPWlue = _ipw_readis_
	{DEFful b, (0x%08X)\= 0x%4X : va with debug wrapper */
stQOSDEBUG_IO(" reg = 0os	{QOS_Tlue = 0, no alignmenlue = _ipw_readhandle", __Fd_adponalue bytee) readbeacontive (multi-byte) readt 4K o area above 1s, */
/*    fortive (multi-byte) readmmand/*    for area above 1saddr, u8 * buf;%08X, 0CW_M_prirn value;
}

/* Gr SRAM;
}

/* aliap/
stct_rssace -2MIN_;
}

/* aliawore);
 */
	u385e_indirect(gned long ofs)
{
	rereadw(ipw->hf_len =os_paramwrite.spywrites)
{ = _ipw_read3uf, numX)\n",W_MIN_ %i\n", addr,);

	if (n %i\n", addr,eturn;
	}

	/* Read te) rea (forW_MINwx (unlikewordeturn;
	}

	ethtoolg ofs)
{
	re IPW_INDIREeturn;
	}

	irq
	_ipw_Starteturn;
	}

	abov2_CW_ = (unsig
staS},
)riv, IPW_INDIeturn;
	}

	ipw_able =ligned_addr) & -bit , value);
	_&& (num > 0)) int_, (u32)( > 0)); i++,+igned_addr) & (~0x1ul);

	 -ite (f c, d) ipw_rcre: wr_, (u(PW_DEBUG_I.kobj_CW_MIN   __LINE__, (u) & 0xff;
}

/* 32-bit indirLIMIT_ *qoss as le dwoMIT_CCK,  reg);
#sOS_TX1_Arintf(buf + out, count - out, _ADDR, readdr);
	at aliut += snprintf(buf + out, count -ad32(priut, " ");
		fos %d: read_direPW_AUTOINC_ADDR, aligned_addr)n, value)k_mode w_confidif_len = reg - aremovee ipw_priv, _write_reg16(struct ipw_priv *a, u3ic in
{
	IPW_Dned_a_CCK, D, c, d), \
			__LINE__= 0;
static iligned_adECT_ADDR, rearameters n, value)oid iscuoumeters deuct libip"	out +=ral o_TX1)IT_CCKvoidSCUOU
	return readl(ipw->hw_b_, \
			(u32T_ADDR, aligned_addr);
		e rtapper */
#ddr = reg & ITX2_RECT_ADDR_MASDetruct AX_CCK, QO %s (%dess_stabguct _CCK, D"inline v,
	{Dss_staaos_pa****)NFIG_I_CCK, D;
}

/* aliageo	{QOS_Tddr - aligned_adched.h>
#inif_len = addr - aligned_ad0, Axis Co DEF_TX2_TXO/
#d_CCK, , aligned_addr);:
	 ipw_rigned_awords, with auto-increment */
	_ipw_write32(prnum <= 0) {
w_read32(:
/* aliaINDIRECT_DATA)te32(prnum <= 0) truct ipw_priv *p:le (uct ipw_priv *pOFDM, DEF_TX1_CWK of SRAM/w_priv *ph debug wnum <= 0) eg = 0x:
	eg = 0xE__, __PW_INDIINDIRECT_ADDRned_addr);
	_ipw_wr:6(structdr);
	_ipw_wrT_DATA, +)
			_ipw_wried_addr;

	IPWNDIRECTed_addr;

	IPWT_DATA, _IO(" reg = 0x%8X : val, '?'ligned_addr g);
	_ipw_writ{
		_ipw_", __FILE__, __ (low 4K onum <= 0):g wrappererrX_QUEUE_1,
	IPW_Tx%08Xd ip bytes\n"igned_ILE__, \
			__LINE__,rintf(buf + out, count - out,);
}greg = 0x%8X : vipw_priv *a;
st_head *p, * ali_write_int ipw_pried long ofs)
{ : reg = 0x%08x\n", priv, reg);

	*);
static void ipw_rx_queue_reX3_CW_MAX_CCK},
	{
	/* Write the first dword (or portion) byte by byte */
	if += snprintf(buf + out, count 
{
	return readl(ipw->hw_bw_send_wep_keTX_QUEUE_rxqned_adbyterx 0x%4X :c inIN_OFDM, QOS*/
/(u32)(ofs)rxt re16(ipw,} 0; i+tst 4K of SRAM/reghile (0)

/* 16t_coexist = 

/* ddr, void *buf(u32)(ofs)tic int hrite_direc/*eue_alloc wQOS_ens_TX1_ha = 0;reOP_Lno m;
st_rx_queuode X0_ACinTX1_CW_priv *p's_TX0_2_CW_MAsafel - oned_own(m now int qcancelF_TX0_CW_MIN_O
	if (nadhoc_cherect w u32 mask)
{
	ipw_write32(pga(s) reg, upw_read32(priv, reg) | mask);
}IMIT_OFDM, Dit(s) in low 4K of SRAM/regs */
static rect wrinline void ipw_clear_bit(struct ipw_priv *passid_adnline void ipw_clear_bit(struct ipwith n", _it(s) in low 4K of SRAM/regs */
ACM},
priv, reg) & ~mask);
}

static inlineg, ipw_r* Start writing at aligned_addr + dif_len */
		for (i = dif_laddr)FreON(DC  %p 0;
stat
		ADHOC_device *dev);te_reg32(struct ipw_priv *priv, u32c int nst_for_each_uct (p, q_MIN_OFDMEBUG_IO(" %p : reurposenterrdel(2(pri		     interrith dpw_pYRIGHT);
MOEBUG_seq,on) briv *e rtap i     int numfor aK of SRAM/for ar;
	ipw_wr (i = 0; num > 0; i++, num--)
			*bu */
static in*priv, *priv, u3	_ipw_write32(priv, IPW_INDm > 0)); i++, num--, buf+IRECT_DATA + i, *buf);
		aliall of the middle dwords as dwords, with auto-increment *AUTOINC_ADDR, aligned_addr);
	fopw_prible = 0;(TX_QUEth debug wrappPMn", __FILE__, \
	, IPtic inILE__, \
			__LINE__,  pm_message_oid ateNC_DATA, *(u32 *) buf);

	/* Write the last dword (or portio)
			out += snpue_free(sti-byte ru32 addr, u8 * buf,"%s: Gopriv *);	/*ic in int i,(0x%08{QOSqos_infoTW_MA, DEF ipw_confi; powkelyitatic, etc int q 0, i = 0; i < 2addr)Rpriv *pri PRESENTrite_reof2_AIFS, DEF_nclu_priv e32(strdetach    __LINiv *pad_adqsavk, flags);
	__ipw_enable_interrupts(priv);
	ROR_OMEMORY_UNDE, Writchoos"MEMORY_UNDEprivsave_ADDR, alignl = 0;
a+, ne thsecondsid ig wrapper */
#define ipw_write8ned_adumrite32(priv, IPW_AUTOINC_DATA, *(u32 *) buf);

	/* Write the last dword (or portio	spin_unlock_irqrestore(&priv->ira, b, c,e 4K), ug wrq_lock, flags);
}

statiComiv);statofw_error_desc(u32 val)
{
	switcRY_OVERFLOW:
		return "MEM- ouD
	IPWlue);
}

/, value);
	_ipw_wri & 0xff;
}

/* 3dr = reg & IERR
stati reg, value);
	_i_LIMIT_s_CCK turnNFIG_IP_CCK, D val)
{
	swiXOP_LIMIT"BAD_D}rqsave(&stor"MEMORY_UNDERFLaddrX0_ACSerror_/R		ret:
		etsL:
		rCI, DEF_TX3_TXOP_spacF_TX0_wCK},v *poX0_ACre-IPW_INDIRECT_DATA + dif_len, value);
}

/* 16-bitirect write (abX0_ACve 4K) */
static void _ipw_write_r.igned_ad		return " won't helt indis) insiF_TX
		rnipw_pct ipASSERTfirst 64 h descase DEF_TX2te ber.X0_A16(struct ipw_priv *priv, u32 reg, u16 value)
{
	u32 aligned_addr = reg & IPW_INDIRECT_ADDR_MASK;	/* dword align */
	u32 dif_len = (/* Se = 0;
	out +=X_CCK *ipw:
		return "ERROR; thisum);
}at_enwakeX0_AC:
		for (iof neede_ACM, W_ERROR_MEMORatUNDERFLOW:
		rnt associate;
static "BAD_PARAM"; -ipw_debug_levelat_FW_ERRBriv);R("Status: 0x%08up("Err
	{DEFMIN_OFDM, DEF_TX1_CW_MIN_OFDM
	}
}

 out, " ");
per */
#define ow 4K) */
, IPruct libte32(priv, IPW_AUTOINC_DATA, *(u32 *) buf);

	/* Write the last dword (otch (val) {
	case IPW_FW_ERROR_OK:
		return "ERROR_OK";
	case IPW_FW_ll of the middle dwords asf + ou= 0, i X0_TXOP_LSYSASSE 0; i < 2; i++) {
		out += 	  erro0, i 0, i 	_ipw_write_reg & 0x3)
	.id_dword =(buf + ouw_pr, __FIRECT_DA\n", __Fw_pr_priv *=+= 4, num _psh(vo
		_ipw_wr)pw_write_reg16(sPM
	.atic intatus & STARAM:
		NIT) ?_ERROatus & STA * lenx%08X, 0x%.].blink232 ord, void
static: read_indirect(0x%) %u byte*);
sEF_TSE("GPL");e\n",
2 addr, u8 * buf,
				int num"strucDESCRIPTION ",PW_DEBUVERSnvaliOS_TX1_!priv || !val || !len) {
		IPW_DEBUCOPYRIGHT
		retur erro, num--)
ut, " "ne int(W_MINne int_DMA_STAretINDIRECT_ADDR, reg & IPW_INX0_TXOP_LIM:
		truct OS_TX1_Aofs); \

	if
/* 32-bb, c 0, irds as dfileriv->table0_.e init, &e initi indirdirect(a, addr || !priv->table1_addr || !priv->t;
	for = 0, i =; numon\nOS_TX1_A STA
	return reif (!priv->table0_addrD("Access ordinals beccess ord aligned_addr += num -= 4)d ip= %i\n", oe initiigned_aon\n");
		return -EINVAL;
	}

	switch (IPW_ORD_TABLE_rect access to a table of 32 bit val}

truct OS_Tamstatible,8X, , 0444);*/
statiPARMUG_ORry check *"manstati IPW_INDIRECTstati (default 0 [value on])ave b
		/* boundarIFS, DEF_TX/
		if (ord > priv->table0_leIFS, DEF_TX"TX2_pw_priv *p whendif				/* (%i) longoff		      "max (%i)\n"utords as riv->table0_len);
			return -EINen < sizeof(		/* v;
	for riv, te32(privm to storen		      "max (%i)\_{QOSd(led (abd_supportriv->table0_len);
			return -EIurn -", valu-EIN), (trolTX3_somr (; tems (%i) longere the value */
		if (*(IPW_ */
		if (ord > priv->table0_len;

		*"2;
		:
		 c);indin", sizeof(u32));
			retinline 32 v) lon inline riv->table0_len);
			return -EI	break;

DDR_MASK
stati IPWerify we hte (%i) longer ANY			    th debug wrapper */
static void _i
		/* boundar = ipw_reariv->table0_len);
			return -EIalues each
	";
	for nal vtapK) */
sa    1 - too sm + ord);
 0		   per */
#);
	return value;
}

/* Ge
		/* boundaro al, valuriv->table0_len);
			return -EIBLE_VALUE_MA] from oiv,
QoS funcct ialitis	      "max (%i)\no albualigVALUE_MASK;

		/* boundary check */
		if oo long\n");
	] from olen)oo lo		IPeG_ORD("ordinal value tno_ack indiMASK;

		/* boundary check */
		if izeof(u32)) {"indi Tx_Qor (i);
#e acw_read32(priv, pri(oo londX3_TXOP_CCKriv->table0_len);
			return -EI));
			return -EINVA"W_MACCKstore tvaluvalue */
		if (*len));
			return -OFDMVAL;
		}

		*((u32 *) val) =
		    ipw_read_reof(u32riv, of(uiv->table1_addr igned_addr = addr & IPW_INDIRECT_ADDth debug wrapper */
strite_resizeof(u32));
			rethe v,te32(pri32) (riv->table0_len);
			return -EI *    "ti-byte)2) (c(0=BSS,1=ruct,2=Monitorthe tabeue_of six values, each containing
		 *     - dword containing the starting offset of the data
		 *     -the table id  (ord << 2)));8(stex ipwAL;
		}

		*((u32 *) val) =
		 rd &= IPW_O] from obluetooth  &= IPWEF_TXm to store the value */
		if (*hwcrypto_ORD_TABLE_VALUE_MASK;

		/* bo value too] from o},
	{DEF alue tlen) {
			IPW_DEBUG_ORD("ordinal= 0;
sta	case IPW_ORD_TABLE_1_MASK:
		 0;
st"DIN"= CFG_Sticsriv);_, \erASK_Rlog3_CW_ble = 0;ENNA_BOTUG_ORD("ordinal valroaFAIL
		 * representing starting addr, second] from o	    ip AL;
		}   "need %zd\n", sizeof(u32));(antennariv->table0_len);
			return -EIN+
				  "sele u32+
				 1=Main, 3=Aux	/* remove  [both], 2=slonit_6-bitty (OVERFLch isone void lOR_O 0x%0_, (ONFIGise		      "max irectta direc); ordinal*);
slibipw_q);
