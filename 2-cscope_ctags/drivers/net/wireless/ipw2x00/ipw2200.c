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
		IPW_DEBUG- 2006("D*****erveding while eserved.

 .\n");
		queue_work

  Copis f of t, &le froeserved.pyrirtionreturn;
	}opyrig
!ile froyright(c)l ri3 ll rig InED) Corporat 20. All 
    s r status cod 802.11not1 yright(ede poht 2000, Axis Commanalyzer
    By Gerald Conalyz attempt from %pM "
			"on channel %    C,n re.10.6:
    _request.bssidor modify it
  under the ribute )municons AB
    Et= ~(ereal - Networktel |hereal - Netis f tr;blic License as|=hereal -DIS by the FreCopyrig
Copyrig version 2 of the GNU  2 of type = HCGenerad i_QUIET;
	elseill be useful, but WITHOUT
  ANY WARRANTY; withdati******* =
    CopyrigSE.  See the G0.10.6: 2 of the GNUn.
at i
PARTffic analyzer
 HC("A softwatoOR A  [dis]SE.  See  commandcar mo ral "fail
 and/ AB
     1998 Ger}

B
  icPOSE.  See the GNe to the Frdata; if not, write to the Fre = e Soprogf analyzs AB LiceEthublished by GNU ED Temuld re Founde sol )willght 19 0prog; if not, write to th Tem, 0ensenetif_carrier_offile fronet_devenseeneal P1;this progroftware Fobg; if not, write to thm et_Wireless <il USA.

  The fion, Inc., 59

		containin th(m et,ic License is in,  License along 2mutex_lock(U Generluded prog not, write to *****97se fo***un; if not, write to tN.E.ct Informl Cor:system_configWireless <ilw@linux.i Fre.com>
 he Freeffic analyz, 5200 N.E. Elam Young Parkway, Hillsboro,hed.h>
#inclu Pub#ifdef CONFIG_IPW2200_PROMISCUOUSpe thaic Licprom_ called && stGenerunningefine VD "d"
#else
raffic, ORnse ys00_DEBU.accept_all_ Tem_frames =#endM
#endif

efine VD "d"
#elnon_directedne VD
#en
#define VP "p"
#the FIG_IPW22se
#mgmt_bcpRTIC"
#else
#deRADIOTAPFIG_IPW220R "rDIOTAfdef CONFIG_I}ine VP e to the Fre_IPW2200_DEBUdefine VQntace to the B
    _codefficu16 B
    ;
elsestNITOr *reason;
};ntact InfIPW22D0_VERSION "1.2.2" VK VD2200/2915 Netwos[] =ffic{0xght "Successful"},COPYRI1, "Unspecifi MA licureEthereal-A, "Canthersup Cop all the GNUed capabilities lam YeU GeV_VE_IPW22Ey ie <lim etrefield
#define B, "Re status con denfic due havinTH_P_802to PROfirm that_ 200SDRV_DESCRIPTexists
#define C, "AOR(DRV_COPYION);
MODULE_VL");

 outside0211_scopehereahisLE_AUTstandarnt cDRV_COD,_AUTRespond#ifdact I);

oeseSION and IPWc****MOD
#defiauLiceti  + 1)
E_AUTHlgorithm
#define Eel = 0;ceiv_debn Aug_level;
yrige VD
ITNESeb 1;
static intshe Gnc1LE_AUTtransactitic u ****blnumber 0;
ug =expP "p"
 bt_coex
#define FICEN 1;
static intrejt hwcrbecausbaticchIPW2nge analyz"FIG_IPW21GHT	roam

  = 1ic intic cons int cmtimec inwais <gefor nexnt cAUT led_sint bt_coex 'b', 'g' rigNSE("GPLortiint antttribr iAPl;
sunablnt cmhand.11 ddinetwal CONFIHOR(HOR(wcryt  intatic int 12 int nt rtap_iface = 0; *****cmdle GNU_enable intod WAR0ace =#ifde
#dE_AUTtati0e  Teald ION(DRV11_BSSBasicServiceSet Pa VD
ter "d"
#els3
#deQOSt qos_enableqos_e- dourst_ent burst_duratioburston_OFDM = t anteh
stapreamdo nope#ifdtw "d"
#els4qos_enablelibipwduanalyz_OFDMrst_enabl ante****** lib****atiopaISCUterPBCC encos_en "d"
#els5S_TX0_CW_MIN_OFDM, QOS_TX1_CW_MIN_OFDM, QOS_TX2_CW_MIN_OFDM,
	 QOS_TX3GeneraliagON(DR "d"
#els9S_TX0_CW_MIN_OFDM, QOS_TX1_CW_MIN_OFDM, QOS_TX2_CW_MIN_OFDM,
	 QOS_TX3s def_slotX1_ACMX1_CW_MI{
	{QAS_TX0_CW_MIN_OFDM, QOS_TX1_CW_MIN_OFDM, QOS_TX2_CW_MIN_OFDM,
	 QOS_TX3DSSS-1_CW_ACM, QOS_TX3_ACM}28, "Invalid I1_RAW + 1)
ElementOP_LIMIT_9, "Group Cipher: 00211r}ace qos_paramHOR(PairwiseCM, QOS_TX1_ACM_CCKX3_ACM},OS;

sAKMframet_CW_MIN_CCK, QOS_LICEUn 0;

sted RSN IE v_CW_TX= {
	{QOS_FDM}
_CCK tX0_CW_M (ESION(Dieatic int 2ERV_VM,
	 Qsuiterame antenna p_MINecur(DRVpolicOS_TX	"e Fre(R)ERSI/WiOR(DRVIG_Iget0/2915 Netwo(200_VR VQat it wi**i;
	"p"
(irst_e i < Y orY_SIZE(k Drive2200G_IPW); i++NU G forIMIT_CCK, QOS_TXei].B
    E== (B
    Eth0xffGNU GCENSE.

CK, QS_TX22CK, QOS_TL");

stCCK}
};
"UnknownCM, QOSW_MIue."/hwcry#defe <


#inlPW22average_ini******, 59DEF_TX *av  Intememset(avg, 0, sizeof(2S_TXelse
#QOS_FDM,DEPTH_RSSI 8 DEF_TOS_TX2AX_NOISE 16tact Infs16enabon
staal_, DEF_T(_MAXprevF_TX3__MAXval, u8 depth*******SE.

((, DEF-1)*1_CW},
	 + TX1_)/, DEF{
	{DEF_TX0_CW_MI, DEF_T1addMINX1_CW,, DEF_T_OFD{DEF_TX1__OFDMavg->sum -= os_paentries[os_papos];, QOS_TX1_O+=TX1_, DE
	 D DEFCK, QOS_TX1_++ DRVK, QOS forunlikely	 DEparamIMITAVG_ENTRIESine VM
S_TX2S_TXG_IPW220S_TX2_CW_MG
#dl}
	{DEF_TX0__MAXACM, DEF1_CW_2_ACM, DEF_TX3_ACM},_2_AC Plac_OFDM, QOS_TX2N_CCine VM
ublic_TX2_CWaramCCKAX_CC,
	 DEF_T/W_MAX_po QOSGIFS, DE_TX0GeraOFDM,CW_MAX_C
staD_OFDM,
	 QO{
	{DEF_TX0_CW_MIN/scresTX1_ACMsm; if not, write to the Freu32 len =3N_CCK,
 DEF_TX0  ThisquaN(DRVDEF_T, QOM, DEF_MAX_U AX_CCKTX1_TXOmissed_be PRO/ense  Thisexp_TX3_rssACM}-6_TX0TX3_ACMK, QOS_noIN_C= -85 +stat,	
	{D#endiflast__ackDEF_TX1os_oui[tic CK, QOS_TX1_CCLEN]X3_A YRIGHT0x5rx_packetqos_enablere; _priotity_to_tx_ of te DR_ACMratiTX_{
	'a',qos_ena
	/* FirmSA.

managed, X2_AI only when NICQOS__TstaE_3,, so wULE_Vnt c
	 * nc stlize osk = 0currenCW_MIue */
#defi] = {rdinal GNU GNanalORD_l ri_RX_ERR_CRC of vU Gener	IPW_****rr, &le*********analyz**********************);
TX_FAILURenab********DM,
	 _2, IPWQUEUthe GN(st2, IPIT_CCK,UE_2, IPWm);
E_    Ieachreal.com>con libie to thK, QOSadhoc2 _CCK, _queue[] = {
m_priFformation_element
			o_command1,
stafo_commarX_QUEUE_1, IPW_
	{DEF_TX0_, DEpriv, stmax_OUI_EF_T3_AIFS_TX3
	{DEF_TXACDEF_TX_CCK,x80s_envo;DEF_TXmaskint tic i ack__emov;PW220If32*****qlyftwaKBUILD_e
#dBf vee_commtricon_elemaximumcommairelLmatchTS (Btic in libip

#ifdef  2 of the GNU ieee_****IMITanalB_MODENU Grk(se&= LIBanalCCK_RATES_MASKryptipwTODO: Verifyic inturatstatiisst_enabl0;  t(st DEFN_OFDM__OFDcommalist. lib
	rald@e(i00_M!(et_btx iIMIT_i >>_IPW22swirypt(i
#defcase*******index);
s_1MB ante:X_CCK},
	
1c voi*********sync int qos_env2*******_1, IPW__fr2********v, struct l int qos_en5truct ipw_rx_queue55ipw_rx_queue_alloc(s DEFtatic 6truct ipw_rx_queue6*ipw_rx_queue_alloc(sw_rx_queue9truct ipw_rx_queue9*ipw_rx_queue_alloc(struct ipw1********pw_rx_queueeex_queue *);
static void ipw_rx_1pw_rx_queuer1, IPW_1 *
static voidPW22oc(sork_pw_rx_q8w_priv *);
static vtatic priv *priv, struct ltic ta24work);
static void  int ipis f*);
stat*is f
statiti3******,_MAX_CCK****3nfig(stru;
statiic voi*******rx_4
				struct bg_down4pw_rx_qpriv *);
static int init_5*********#inclu****5x_queue_alGeraunic#endifriv,->				*****EEE_BMIT_IFS, DEported_ratev *prftwact *work);
statt iw_riv,
sticsipw_dogt ipw_qwe "ipw2uf, ssipw_rx_q calledice *deeue_,0_TXOPvice *deeue_*****te to the Fre
ce - Suite 330, Boftware Foundationax_queue_fr	 Dreclaim*****		/* CONFIG>rx_qcREALv,
			RX_PACKET_THRESHOLD
#defiPA#defULAR, struct libiped_rates(_MIN_OFDM,
CURRatic , &t 0;
rs
	CCK,
	tatic inre Pla
  more dald Gerbs

INFO(ACM, DE query DEFpriv *ps   CopyriAX_CCK,
	 DE	}
	} s_enablesnprin count,
et_t u8 *e to th			 0;

inar c;len******analTXv,
				st*);
static voipw_rx_queue_ - 0;
, " ")2
	

#i(int a,ipw_down(structi++) {
		out5+= snprintf(bstruct ipwg(stru++) {
		out6+= snprintf(bt ipw_priv *prilen; j++, l+9+= snprintf(bdiv *priv,
		inlen; j++, l+;;+= snprintf(bu ACM},
	{QOS2_TXOP Corpooutaticriv,
				struct  *pripw_rx_q = '.';

			8,  *);		}5200ftes);
static vont - out, "%c4+=nt - outf(brx_queue_alloc(s+)
			out +=    W_TXeu32 len, , struct libi+)
			out +=_supdel, co lib, co *priv,
				+)
			out += 	00, Ax 0;
;
}ork);
statis_enX_CCK,
	 Duct net_deCanall riS_INTERVAL (2 * HZ)include <linux/scga8X",FSEF_TX3_Astats(struct net_device *derams_com\n", l_delam Y */

statiofs +=truct iM,
	 QOS
statos_param);
s min( 1		/* CONFI= min(ut, c160, 0xF2 };

st_percent,_CW_,enna tIFS,*ut, size_t sofs););
sructEF_TX3_A j, l;
	cha_ACMA	_MAXaramfsrst_e };

s ofs);CMX3_Cgnalt, "%ceturtxline(outpurX3_CW_, &d
SE.

eue_line(outtruct iem
	}

	rn out;
}
ft leun{
		%08X", of= 0;+= snprinte Vfine VP ize;
	 DEFrintlong with
  ta/* Updtatienablct Ize_t *);rec0; j < 800_Ml <F_TX; j++, l++)n reMISSED_BEACONSe+ ounprintf0, 0xF2 };

ststatic inrze_tF_TX*****_CW__t e32 ipw_t	     *qos_para-ad_reg3_prio	     *qos_par int from_prio	     *qos_param)d_reg32ipw_rx_queue_ DEF_
} be useful, but WITHOn Corpndefr_TX1ffic ze_t len)
{
	size So,  = _ipw_tatic u32 _ipw_ *
 {
	co(HZhcmde16UEUEcpu 8-bit inendif
 read (

#iSRAM/reg ab) /eg8*****ne(OFDMX3_CW_MINlin* 1is di+) {
	ove 4K),ITNES drt = wrapters*/tion_eF_TX1_TXO 0;
 DEF_T = 0;
	int, 0xF2 };

st2 _ipw_tatic u32 wrappereturnFDM, QOS_Tiv *priv, struct libipw_qtion_element&\n", lint - out, intf(rintk_b=nv *a,  && l < lenu32\n", lue[] = {
	IPW_ritr (j ="%s %d:  *priv, struct libipen; j++, l++)
			out	 QOS_TX &U);
	}
}

sta))
					ut, count - outk_ =os_enablet - oPW_DEBUG_IO(IPW2200_QOSnfo_command2G_IPW2200_QOSE_2ead_reg8(a, oect w	sizoutp {
	sread_reg3_X_QUEUE_1,ration. A_IO("%sQUEUE_1: write_indirect8QUEUE_1, Iurrent_*/ ipw_wr		stf(
	sizrite_reg1w_*****_r		/* CONFIG;
st*****pw_read6 valuv *aw_write_rli		/* CONFIG_I16 c*****ration._keys(sCalcultatiize;
	u3based_writeu3following:commcommaMK, QOtati {
:, i %for tf(bndi70%ize_t lcommaRa
	{Q6t wri1Mbs,32 reg);Max_reg8(x  GNUTx**/

r_3, p;

	nt ae "ialong %atic otal Rx/Te 4K), SSIu32 reg);> -32(aG_IO("< -80 4K), wi	str_iu32 reg);
#defwri5s %d: wri_regcommaThe lowetatsompu ipwize;
	u3 0 -sed.32 bG_IO/DEF_TX1_Cpw_rea+) {
		out 5
	
/* 32line(out(for0riteK), with debug wrapperas for 
printf(__LINE_<0xtput)\n", __FILtf(b_reg32(a, b, c)_,tion_elemenrite (low 4K) */
ereg32(aa,rectc)-
}

/*, u32 rendifu32 r0) 
				rea (u32) *ipw, unsigned l;r	data[(i * g8(a,("print *ipw,32-b%3d%% (%d%%)nd/ of v
		     __LINEsnv *priv, struct libipG_IO( { 0x00, 0x5statiEN 16;
		reg len)
{
	siDEBUG_IO(U)+= ousize,e_in) intfbuf  "%cretu	in_t(2 _ipw,bit direEite8(ipw, * 40 /16%s %d: w+ ; i E2) ( += out;
 *ip(a, , u10x%08XIO("%s %d:Mbslow 4K of)\n", __FILE,, \reg w 4K) */
 /, i = 0;fine rf (,
		u8u16 c)
eg16>gsize&&16ntf(_TX1-put, s +8 c)
{
rintk_tf(bua[ofs],
	 */
)
{
	ite_aefine b, nct ibs,
		u8 vaect wipw, unsigned lo+ ofss) douion_elemental*****O("%sw(vl));ite _ipw_write8(ix ofs, val lper *} 802u_write1, %u QUEUE_10)*ipw,16-a[ofs],
		ata[a, u32 b, priv *ipw, unsignOFDM,		ststruct l->hw_baserx_queue___LINE__, (u32)(o+16-bit indirec}

/tf(breg1fs],
	bug wr0_AIg8ct ipw_pr-bit di_ipw_write8( %d:)

/*pper */
#define i16(direcrite_diion_elements, val)le (O("%apper */
#define ipwTipw_write32(struct il) do {ite riv, u32 reg, u10x%0 \
} whileel Ce16(ipw, ofs, vbuw_priv *a, u32 beturn, DEF_TF_TX3_TXOP_LIMaram
	whofs,_OFDM( "%c520u8hw_ba
{ect3ic inl*********weperfect };

sct writerite_dworeg, ssi)* 32-bit direcIO("%s#endif
32(
}

/*_pri

/* 8-bit dire) */
\-			__LINE__, (u32)(ofs), (u32)(val)(ipw, e (low 4K15 *INE__, (u32)(ofs), (u32)(val)); \
	_ipw_write32(ipw, + reg, u 6), & ofs, val)gned lonpw, unsigned lt );
}
; j++)E__, (u32)(ofs), (u32)(val)); \
	_ipw_write32(ipw, e (low 4K) */
)
{
	( outg wrappec inline void _ipw_w32te32(*****for ipw->hw_base +rx_qutf(b ipw->hw_base + ofor lot ipw
/* 32-bit direc re< 1_bas832)(val 8-bit di	{QOS; \
	_ipw_write8(S ofs, 
stat, ofs, val)l dBm0)

/* 16 ofs,, val) dopu) */
		stize;
	u32 min6-biine(outpu u16 _ipw_read (u32)(val));direct write (for{
l & leveine wte32->hw_bipw->hw_basewr}

/* alias to 16-bit dir SRAM/regs), wit}

/* alias
MODU e;
	u32=

/* 32line(outtf(b; \
	_ipw_write8(Qite32(spw_rel: ClampedTS (MIN_OF

/* 32direc	_iputG_IPW22		u8 vadrite32(stru)\n", __FILE__, \
			(u32)(ofa****_bas16_ipw_read8(ipw, 32(i wraprite32(reg wrapper */per */
#defipw_read16( \
} while
/* 32-bit direct read (low 4K) */
static inline debugl(vaad32(struct ipw_priv *ipw, unsigned  or low *****/* alias tol16-bit dir \
	_ipw_write8(* alias_ipw_writead32(struct ipw_priv *ipw, unsigned h debug wrapper/* 32-bit direct read (low 4K) */
static inline(struct *ipw, un2(a, b)

/* alirivtotal = 0;e;
	u32  __FILE lect  = 0delayed2200freture; yode =eal-U Gener, size_t sU)(u32)ct w_reg8(a, b)

/* 8g16(EF_TX1_AIFS, DEF_Tbgt, size_t s%s %d: );
200.h"


#ifndef KBUILD_EXTMOD
#define VK "k"
#else
#define VK
#endif

#ifdef CONFIGect(a, b, c,.def K*********w_read_indirect(a, b, c, ect(a, b, c, ipw_read_indirect(a, b, c, dAM/reg/
#de/direct write _baseE_VEor:
e (lOS_Oit dLI-> '
};
st_threshold, jusENNA_B, don'tse +any scan/w_wr., wiw_write1eg);
#ded _ipw_read_indir	int num);st addw_wriw_wr;
#debettK},
irect  wraAbovefs)); \
	_wr ipwindirect(give upf, sizeopct ip_IPWO("%sR_writeipw_ion:
f + ifwrite_indirec			int num <=appereg a int u}

s(u32nclude <linux/sccn(ste32(a, b) _ipw_w_read_indirect(a, b, cu32)( ({ \ "%cMIN_OFD16;
N_OFDM,lias toGene	     *qos_param)(a, b)

/* aut, coun(a, b)

/* a >G_IPW22pper */
#define i	int n&&) ({ \f, count, "%08X", ofs);

	for (l ffic 8 * da/ine VP "p
tatiwe've hi* data,_read_rm);
AM/reg (0x%08X) %uay, Hillsboro,t 19DATA(strft(a, b, ,fs) (ab
sta2nyout, vegs),nurn to2-bit direa_regDL 8 +  |with DL_NOTIF |e (fore_indirl riEIFS,d_addr = r6te (foradb(ipw->hw_basewd -g32(struct ipd32(2(a, b)

/* alG_IPW2200_Rprogra
  pereal -ROAMhe ho	outp2ions AB
    Ethereal -SCANNMOD
#defirapper */
#define regAIFS,_indir****ine alirite _addr =eneru32)(ICEN

stlow 4 ipw    Ic void K) */
reg X, 0x%0g = 0;
lias to multi-byte read (SRite_p__DAT_ipw_bug n-bit indirect write (above 4K) */
s OR 97124-6497

0ght 1998 Geravoid ipw_sB
    Ethereal - =); \8T_ADDR,DIRE)we NFIGtruct libi, with dc1;
s*****above pr "%caM/regc int IN_OF.._rx_qus) ({/
#defIPW2INDigned_a/
#define ipwsend_Ratic ;W_TXdwordddr)gn dase inact Inessd32(struite8(g -e32(pr_ipw_
	u32 aligned_addw_writee_dirdire )(of int x%8X :DDR, a(a, b, c, d); \
} e_indire__,
		    UG tottDDR, aen, valu8-bit/
#defreg=el))
	pw_write32(pr8X", line idifLINE = appe				c= 0above vitmask = 0ration.tatikicke (f aTvoid.above T0;

 ipwhve 4****TX1_lSYS_As, \
oreite3r8 * above )); \
	_ipw_write_indiu32 reg, if_len, value);
}

/w_read_reg8(sIRECT_indirec8(ipw, ofs) ({pri_MAXw_base_priv "w_write	_ipw_ed_addr);
	_ipw_		ofs += 16;
		output += outSK;	/_MApw_writEF_TX2act Infml;
sdistrite value);ct reeg32(a, b)

/* aliect wr) & 0xff;};

s{
	si*****);) ({ \
	I****multi-byOFDMndirefor U Gener 0;

st");
	f32 vdiAM/rgned_addr)
	_ipw_write8(g &0x%8X : IRECT) & 0xffine ipabodif_len, value)analMB_IO("_CANCEL= '.';

			f + o/* Se32(ipw,TS (keep fSA.
; ygetlue above stuck (3 0x%irite32(p lib: value --a%d:  4size2_ACCwe'll ias gT_ADDAm-bitthan 2 or 3above Generals..)bug wrapper */
#define IRECT_ADDR, aligned_ad ((reg & 0x3) * 8)) : value =tatic +ord;
	_i * 8)) & 0) */
streg32(struct ipw_%s %st 4K oK)), with d				str_ipU);
data[(i * ignedeadb(ipw->hw_basewd) */
static u32 _ipw#define ipw_read_inhcan_evenCK,
	1_CW_00.h"


#ifndef KBUILuirectiwreq#defi wrqulong _EXTMODFIG_IPW220K "kDIOTAP
#define VVKine VP "p"
#else
#defint;
		***
2)(dpw_p
	rite. Tem.ne Dth, with CASK;	/* aflag_write_rt u8 * dt += sn***
n *priv,_IPW22, SIOCGIWdire, &rite, NULeg16IG_IPW22		u8 val(0x%08}

spriv, num <= 0s(struct net_device/* O 0x%userspace-eless 22

tatic*/
sle_ 2003***
s goect16immedVP "lyrn totlai!#endif +um--VERSION

		strEF_TX1_CW!IPW_INDIRECT_ADIOTng), with (" reg = 0ofs) (priv, IPW_INDIRECT_ADDR, reg);
	valine ipSR(" reg = 0_ipw_wrir The_jiffies_2(st= 0x(msecs*****) buf (8 * ));
}
 align */
	Rne iGNU firstw_write(or{ \
ine  SRAMauto-incrbipw_", __FILcancel IPW_INDIRECT_INC	/* StarDEBUG_ICopyr
		/* StarDEBUG_IO(" 		outW_TXStarefine 

  ats), EBUG_IO(" r & IPW_INeg16 no rint =ord;
	_i; (({QOS4)00_M(nuOS_TX2/egs asHindir hostg)
{ifatic intQUEUE_O("%sCdes[diterat_reg8rupareautine

	return readbect itrx_o num--ipw_ dif_len = addr - aligned_w_wwrite e "ipw22e
#dt 4K 1st
/* 16  *e 1stAM/reDECLARE_SSID_BUF(erms(0x%1_ACM ipw=eadw(ipw->hw_e 1st->um)
ddr =min_t(si*bufratioBILITY %i), wi
	vaeg, u1ne 1stte32bBILIsnprW_eturn out;
}
32ord;
	_ipw_av, IP16;
	HOSTT_ADDRICA

strite8, MA  k = 1-1:_writ Copyriefine rapper */
#dec., 59
 &
	u32 iuRCHANTCopyrprintf(burintf(ADDR_tion. y(nuN_CCMAum = %i8-bitw_wr, ECT_DATA);
	return (word >> ((reg & 0x3r);
		f& 0xff;
}

_addr =ADD* 8)) ;
	_ipw_wr: '%s'you cd32(structid (unl)_f,
	uf,h debug (lef,
	ipw_viv, IPcouiv, IPW_IND_tic -)
			*buf++ v->tf,
	gned_a last dword); \
	_ipw_wiwreg aht 20) ) 
	valIWx2lue)8 +RA_1, 8(0xmemcpy%4X : value =
			_en; (
static vobufindirec_, W_MAALEN
/* 16-bitreakpw_writeor (num--)
		ADHOC= 48(0x%08/* W */
	2200g = 0e middlew_writs as= 4; buor SRAMbyte */
	if_ADDR, /* clea0;

stASK;	/* 2)(vpriv,n2 reg addrlias tum0/291e VD2(stru);
stati(rd;
	_inum--)rd >>ic vo ("priv,d >> s_istrieck
}

/* 16
		f+= snpr;); \ >C_ADPW_AU+****he lignedwor;
	(word >>_i
static voidddr);
		f\
	I/
	if (unladdr);
		f
		adw(ipw->hw_baddr,M},
	rite3t_en of the GNU , buf++)
			ipw, unADDR-bit dbipw__readic vo;
		f_writligned_ad_DATA;
DIRECT_DATA with80211 hoiv, ams_ructe (mul
	IPW_DEBU = %i\n",  H;

#i-bit indirect write (above portion) bic void#else
#de*/
stFIG_IPW220D "dDIOTAP
#defQOSng ofs)
{
	rlGET16;
	 - STYPE(x) WLAN_FCvit d+ dif_ \ignedadw(ipw->hw_

  Cluded eesk = 0hdr *)(x))->e VD
wcryprolofs) ({ct retions AB
    Ethereal - UTH)ine ii, *bubyt reg ipw_read32( dif_ Axis C

	rawect(ruceg, u1eg) | , with, ipwa
{
	/_RESPdr, u)
{_rx_queue_CCK, Copyripw_waticACM, x%08X,, \
	 l++tal;= addr &ct16M,
	g) |  emov)<j, l;
ddr, u8 *32 &
{
	rzeemov231t ipw_ u32 bPW_A, n" reg = rx_queurx, 16U)" reg = ue = _a_1, Ivoidsend_riv,  j, l;
 - 1addr);
		_AIFECT_DATanalyzer
 	/* ECT_DATA + ("QoSQOS
statidefine _write32m"ct ip %inty raddr); ~u32 mDDR_Mw_wrimg= ipw_re_AM/rerup	regs addr);
		f		struct 	_ipr_blignerx_queuhdr_4w_wr_write32d: w*ddr, u8 tic ic void/
#def, &pw_enNABLED;
	ATA)te32(qDIOTAP
#
#defchedul indire			     ink_up,INDIR-= 4K of der ibipw_,
	valbyw_wrUTHENTnty , num-stat2; i++) {
lignec voite 330, Bostnum)m = %i\n", 30}

stati32 reipw_write32mead32(priv reg)  \
} 
	i 1;
static_ACMuundariv->stat0, Axis C aliu= 0;_INDIR[QOSw_wri	return (word > 2003 -INT(" reg = 0x%8X : lue DDR, aligned_addr);
		f regTA);
	 write (n */

  ;
	ite32(st"%pMclear_bit(st: 32)(v4X) - %snum--)
			*bbuf++ = ipw_read8(p, buf++)
			+= 4, alfor lt resiv)
W_INDIRECT_DATave(&pspin*****_irqsave(&ite32(1_TXOPiv *p---priv,rds, witbipw_qs);
	adw(ipw->hw_le_is);
}

/ibipw_qs);
	UE_3, _device E_3, nable_inte(adw(ipw->hwnable_inte_k,irqsavipw_wre, nCtatic, *)
{
ration. Aqsave(&pspiposton, MA  02111Free *******n_OFDM_INTA_MAStsERROR_para";
	 int0x%8X Intel Cor.
rx_queuef (!(p)dire00, Axis ite32dowatic v1st 4K of S_INT	if (u, ilast dwor) Corpoff;
}

/* 32-bitpriv);
	spin_uDR, aligned_addr);
		fr (icW_FW_ERROR_OK:w_wr%pMum--)
			*buf++ = ipw_read8(priv, IPW_INDIRECT_DATA + i);
	}IRECTpw_p(priv);
	sin_unlock_irqipws &= ~ {
	case IPEf (!(privc voiINITx%8X :TAatic _R, ~v, u32 addr, u8 *pts(a_rx_queue_alloe IPW_FW_ERebug wrapper */
#defiMORY_OV*****ite32(ir2 len"
#eERROR_BADALL);
}

statenabl_MEMORY_UNDERFLOW:
WtatiOR_Dve(&p*mask)_priv *prDERRUN:
q*****_FW_ERROR_	_ic voL";
	case IPW_FW_riv *tic char_un_ipw_disrestore_interruirqstruct libip]se, no IPW_FW_: %R, aligneROR:
		return*****libiFW_ERROR_BAD_nten /* pw_doerror_desc)
{
IPW_DEB
	switch e ipw{ \
NDERFLOWSYSASSERTRY_U
		re 1st leve"MEERROR_DERFLOWOR_DMA_STATUS:
		returBAD_PARAM:;
	case I "truct ipwRY_UNDw_read_indir
}

stata, num--)
			*buf++ = ipw_read8(priv, IPW_INDIRECT_DATA + i);
	}RUPT:
		return "NMI_INTERRUPT";
	ca_write	returOKRY_UNDERFLOW:
		returpara:TY; with*/
/* ERROR_MERRORUNDic voi_priv *pW EFATA Log Dump: portioIPW2	rete vo7, USA.

pts(NDERFLO2; i++) {
8 * dastris fIPW_INTA_Mriv ->#inclut;
		size i__ipw_disIG_IPW22Ey kIPW_IN	reg) CAPABILITY_IBSSenableucr, u8rue *t do { \
	8(priv, IP0; w_writ, 0x%08npDump:\n");
	OR_DMA_STATUS:
		returMEM_dump_error_log(strucaticBASE_priRXs)rt I0)

/C_ADDR0xff;
}

/*defaultC_ADDRt 4K	retu(n "EEP: uef (b),(%d0)

/* 16elem (or portiporNDERFLERFLOW:
		turn "ERFLOW:
	bug  reg, w_writeATUS_Ifx%08status IO(" reg = (unlik_priv *priv)
{ck, flags);ock_ir
	__ipw_disable_interrst dword FW_ERROR_c inline v;
	ipw_wrIO(" reg = 0xnprin_FATTA);
	return (word >> ((reg & 0x3) * 8 ipw_dump_error_log(strucCHEnum--)
			*uf++ = ipw_read8(priv, IPW_INDIRECT_DAflags);
	___ERROR";
	case IPsizeofRUPn "UNNDERFLtruct ipw_ad_reg8(s );
	UTH
	{QOS_FATA->  error->elem[00, A
			(f

#ifdef B
    Ethereal - OR_ALLOCW_ER}

static void ipw_dump_error_log(struct ipw_priv *priv,
			       stru_STATUS:
				retuOR_DMA_STATUS:
		retur
		ret IPW_FW_ERROR_DINO_ERROR;
	case IPWFW_ERROR_FATAL_ERROR:"FATAL_ERROR";
	defa 0x%08X_bit(in */
staORD("Accr_desc(uKNOWN_ERopyrigMORY_OVERFLOW";
	case IPW_FW_ERROR_BAD_PARAM:DR, aligned_addr);
		ble0_addr || !priv->tableCHECKSUw_priv wrapper */
#define irdruct i *X0_AIF3), &tatic u3d_addunlik
MODUi++,ois a ver_INDis a ver16;
		oto 2 oROR_FAIL:
		return "ERriv,DMA_U].time,
			  error->OR\n");
	IPW_ERRO";
	}
}

serror->elem[i].time,
			  error->RROR0xff;
}

/*t(struct iTXOW";
	SEQ_1ug wrapper *riv *opyright(c) 2003 -retu) ? BLE_IDatic  & tabR";
	d "***
rite> the la *prERFLOW:
		D("orata)[i].b"
				  2len) {tOFDM0LINE__, 	poration. Abeforeon(struc(word (%i), erric Licecan re	  2   "maxom to8-bittable	return  "
				   _P!priv->tan -EINVAL;
		}

		/* verify we have enough room to store the value */
	 we ha***

ipw_<3_CW_MIN
{
	
		}

		/* verify w	 QOn(strucbuffers
		gth too small,));
			retif (nON

%zd8-bit

		IPW_DEBU
	ca, ord	 QOif (*len < sizeof(u32)) {
			I
	+ dif			  suppoal buffer length too small, "
				      "need %zd\n", sizeof(u32));
			re3if (*len < sizeof(u32)) {
			I-EINVAL;
;
	4 = ipw_read32(priv, priv->table0_addr + ord);
		break;

	case IPW_ORic uNFIGsADDREINVAL;
		}

		IPW_DEBUG_ORD("Reading T2we have enoum offset 0x%08x\n",
			      ord, priv->table0_addr + (ord << 2));

		*h				ch_reg* repald n
stats0; n*erro_wri(prithLE0[%i]to muloffset); \
	x8-bi
			(>tablABLE 1: Indirect a		*buf+*) va<< 2dire/* b Ther + (ord <<  > 0;ordin= 2
	ca*(W_DE006 = ipw_read32(priv, priv->table0_addr + ord);
		break;

	case IPW_ORbuf +/* priv:
		/*
		 * TABLE 1: Indirect link2priv->terriv, febug i u8 _ip/reg *rd &= u32 vis a fairly large irect "
	valulink2,
		  CopyL;
		}

		IPW_DEBUG_ORD("Re */
	if (= ipw_read32(priv, priv->table0_addr + ord);
		break;

	case IPW_ORD: tput\n"if (*len < sizeof(u3.leng1h too smaloDIRECT_ADDR_Mbit(: {
	'a',2 reATUS_ble
		 ss ord * datDEBUG_ORD("orlog_lnount -rporatir->ele"%i\tf (ord t*/
	if too smDR_MASHANNEL0)

ULpriv,
	_priv *priv)
General libult *x ordin16;
	ock_irqsa	for (;ect1
	lign) =*/
staturnj, l;
	ch*x>config);read32(ipw,CAN("S numengh i 
#deMAX_OFDMuct tableect1)OFDM x->ng the lnumn", si align */
	l */w_baset IPW Esecond 16bitsof wrod: w|=) 20(0x%08X,-EINVAi"(shoul, va %ziv->tablehe ta orditizSK;

-  {
		  INVAnna ist "
	six   "nee, ct itoo E. Elagrd &= et t-w_writOMPLEif (unlik_priv *priv)
 32-bADDR, ae get rdVAL;
		}
 \
	_ipw_ADDRp dif_lentK;

		/	/* get th  ane lastORD_TABLE_VA- alignedLUE_MAS_addr + d:ABILIT%d,eck rect(ruc,(0x%08XCK,
	 %"p"
#euSK;	/id f 32-b	IPWddf(u316;
	xalue) 0x%0eg16coo =
riv->ttrucread8rn -&=0x%8XORpriv->to%d:     4; buue too laryf (unldr + (***
 1: Indirect2VAL;
		}

		/* y we have enough roox\n" SRAterative (muldinal ta

	IPW_ERRO int4x \n07, USA.
DR_MAABORfulough ) =wake_upd_reg8;
	vible0, Axis ipw_0/291\n", si *prLOW";
	case IPW_FW_ERROR_BAD num-u32)) {f (!priv || !val || !len) EXIT_PEN, u32Kiv)
lue too lar); \
	_ipw_wth, s++uf,
			     int num)
{
	mMONITOR dif_lenBAD_PARAM:
		 addr riv->EBUG_IOt_ena		IPW_DEBUld_count, total_len;

	DR_MAFORCdr - aliERROR_BAD_PARAM:
		returnDDR, aligned_addr*priv, u32ERROR_BAD_PARAM:
		Du32 v-EINVAoo longst 4K of SRAg);
	va) wr > priv->tabl& STAT-= 4)
	LINE_);

		*lrn = %i, fig);

		reDo(priuna = VP "pw_reac dworO32(pDAf (!priv || !val || !len) alue =M}
};
VAL;

	-PW_DEBUr, total_len, field_info);
		ipw_read_indirect(priv, addr, v******
/* 3tiv *ble d); \b DEF_ lenS},
	K_ALL);
}

static inline vo}

		/* veri &= IPadto mulWITHOrect_reg3 = ipw_readG_IO("%write (lo_len);

	priR("Start IPW Etati G
	_ipw_write32(priread;
	u32 i;

	IPWata[6 value)
{_len, valu reg = 0x%4X : value =riv, u32;
	_iforeta ipw_read
		/ASK;
ues n");t,
			  ei\n",
		 th, sec);
		i*bufC_ADDR, a

/* 32-bnum--inROR_BAuct ip1;
sIPW_INT_clearOW:
		 intaccess 
	pwasDR, aonDR, gned_addas a, addr, vEINVAL;
		ereg8lue AIFS,.. soIO("ERFLOWATUSeg32(a	case IPWs fs(strucTA\
	_ipw_wriindirect(nfig);

apper */
#defineff;

	if Thtic int0iv->tab32 ofd_len = 4)
	32(a,ipinlibefoINAL first td*****riv, u32 reg, if_len, value);r + (ordpw_register_to4urn (word >> ((rapper *-EINVAr;

	IPW_DEBis a very si	retuble2_addr(0x%08X) ERROR_his isdr);

	IPW_DEB of SRAwrapg &=0_DEBUG& CFG_BACKGROUNDize_t c
	ca&& 0xff;
}

/* 32-bitOR_BAD_PARAM:
	S_T (!(_1);al_len, field_info);
		ipw_read_indirect(priv, addr, val, tble
		 */NRROR_riv,%i\n",direct(pHZn", ord)  forr1_STA, USyd the TS (withct8(0	uft re* Weect wrigned8 * dauv *iCRIPock,);
	_iporatnk    /* dl Corpiofs,al b_ipwired wTS (dless_irqx ect16 <= 0} - alil Corpwe waink
 *minimM/re ipw*d wadlen ask = 0irq);
	s)
***** * Urogr	_ipw_ipTS (Extra * data Star d32(p_Alipw_we gIFS,s_ on
is*****/* ge
		}
add_comgardite (K_OFF on howpw_read32(pv->t(0x%08Xd.UEUEOn ****ationddr);
	);
	vyncbuf periodtatiW:
		regeSA.
eshIME_LI.INK_OFF Jean IIonst u8 * da	if (ord,F_TX1_ */
	if1_addr = ipw_read32(1EBUG_LINK_O			*_unlo =_DEBUG_IOist of six values, each containing
		 *     -FRAG, ofGTHatistic */
		addr =frag		re	_ip";
	case IPW_Ft	__ipw_d*/
/*  dword;

		/* get the s<< 3) +reg abovsFrag= 0;ga ta 			  i, priv-adw(ipw->hw_xementipw_disab	/* geic_type iKILLable + i)
->tab!(pAL;
	get -EINV_TX1yet 0x%08read8(e table 	if (( VP *) &		reg &= ~IPW_VAL;
	get of six values, each containing
		 *     -L* IfDETERIOR%i\n"atistic */
		addr =:
		reS_TXio, QOS_ get the addreVAL;
		}
= 0x%08	return reE

	if (!(priv->status & STATUecond DW of statn) {
			IPW_DEBUG_ORD("ordin1 (priv" reg)tus |= STATUS - (!(priiscntn) {
			IPW_Div->ilcoext(struct ipw_p  \
	_ipw_ructint);
/_num--)
			, aligned			       *q->status |= STATUS, x */
static voiinfo);

		/* getRD("ordinaF_KILL_MASK) L(!(pDus |= STATUSVw_read_reg32(priv, IPW_EVENT_REG);
		led |= priv->led_association_on;

		leduct des(30privgdefau_toggle(led_on;

ratioINO_  int n)

/ONS].YS_A,KILL_MASK) Dinless 22len 2EBUG_2; i++) {
hcm LEDs UG_ORD= ipw*****->cmd !="%i\t0CMD*******interUS_RF_ LED On\n")Un_ORDIconsct ipw_privoid ipw);>led
		/* t of six values, each containing
		 *     - \
	_ip/
staatistic */
		addr =pw_read	IPW_ble_interruROR_Dd.ENT_R***
plags);
}

_on;!	/* get the second DW ofpriv-	reg & * twBalue);
 || r	/* get each entr = ipw_rn_read_reg3lue en = *((_association_on;

		ledhis table conIREC		stle32_LEDn, vK_O * dati=NT_REG);
of(u nic ANY Wis ABLEpin_e vawe OW:
(reg & IP	/* dword align */
	u32 dck_irqrestot(s) k_onENT_REG);inals before ine ip
		 00, At of six values, each containing
		 *     -TGI priKEYatistic */
		addr =tgi_prikeyriv->config & CFUSEVENT_REpriv->nic_typepw_read_r_VALUE_MASKDW ofpriv->tTG;
		oKey:sable_iA_MA2x_3, STATUS, IPW_EVEN(ssoci;
}
= 0x%v->status & STx->keyED || 2 led;
	int Iork of t1,
	 2003f ipw_r
	ifRAM/reipw_privg = 0x%08pr";
	case IPW
f);
		ur}

	o is>config & CFFW_ERROR_LED |d_len = *((u16 verify we have enough rootot of six values, each containing
		 CALIB_KEEP	}

	 tSVALUVALUE_MA */
		ioali _ipw led);
d);

		privn);
static _LED_LINK_ON;

		/* If we aren't assf
		feg abovLD, regFW_EEG);N Corpouct ipwect16(i * reg,j)& l < C);
static if (*len < sizeof(u3		   &privis f_len) {is fnk theus |=nfig ed, vak_onv, led_link_oTed;

	 If co				 rqsaW_FW_ERROR_EEPROM_ERROR:
		ret_addr || !priv->table1_aM, DEROR_OKhis isated and the radio  0;
an red, privfi, 0xF2 };

stK}_read_regCM, DEval))truct ,
	{n) {
			IPW_DEBUG_)
			queue(u8)_link_ond);
		ock_irqsa;

st.s ");)_ipw_wri)
			queueW_MAX_M, DE06 InLOW";
	n of th->mutex);
}

static voiNDEF_T & S;
}
ivity_on(struct ipw_priv *priv)
{
	u32 led;

	if (priv-priv	vr->elem_reg & INO	 * TABLE 2W_INDIRECT_ADDR_M_dur	 QOe 1st 4K of :*******";

	IPW=%d,efine=0x%2x,ERRO
stastatus &len = ipw_read_d
	u32 iefineROR_BAD_DA *ipw,_priv DDEBUoys 0;

d va "ipw2wrappNALS_(0x%0lio(270 m again
e (len@p, QOS(!(ipnsigIFS, DEligne ROR:

	return rem; if nopriv, QUEUE, ofs);
		printk(KERN_DEBUG "%cr_EVEruct/** @toOn rustomiv->/
		c_ERROurn totBUG_TGATE64,
voiCmt ip8SRAM/re \
}  = 0fre, u32 reg,w_prx CMDACT	mut2 reivitgh i_LED("RtxOP_LIMt ipw_pw_privtxq_cmabled_act{
	u32 ledTA_MAS "%cMDcomman;
}
DX : EX_ipw_live voon
	case IPW_FW_EWRITEBUG_ig & CFG_NO_LED)
		return;

	ifBD_BAS3) * 8));

static 	struct __ipw_l, QOirect(prrc
#define V
	spin_ux ore( long  QOS_CM, DE****ERROgo)
#dignedRAM/rck, fl * da(sRD("tas IPW_FW__ipw_disable_interruROR_DINO_E[0]riv)
estore(&priv->lock,W_FW_E0riv->config & CFG_NO_LED)
		retirect(pine VP  _MAS 0 eg16(strucpw_pr
		led &ore(&priffnd thuct ipw_privt write (for lrqsave(1_lenled			  02 led;\n");

		priv->EVENyed_work(priv	if led_link_on,
					   L_len) {
			IPW_DEB103 -VENTd long ex_lock(&pri_pri1	queue_delaed_link_ond);
		ipwggle(l1&=s);

	i__ipw_l(&priv-r_toggle(led);1priv, led_link_on.work) as dwW_DEBUG_LED("Reg: 0x%08X\n", led)1_GATE_Owrite16(satus |= STATUSunlock_i nicrk);
	mutex_/
staLEore ilags);x);
 Off\n2);

		priv->status &= ~STATUS_L2D_ACT_ON;
	}

	spin_unlock_irqrestor2(&priv->lock, flags);
}

static void 2priv, led_link_on.work)fig);t work_struct *work)
{
	struct ipwr + \n", priv,		container_of(work, struct ipw_priv, led_act_off.work);
	mutex_lock(3);

		priv->status &= ~STATUS_L3D_ACT_ON;
	}

	spin_unlock_irqrestor3(&priv->lock, flags);
}

static void 3priv, led_link_on.work)es(30t work_struct *work)
{
	struct ipw) = ipw_write_reg32(priv, IPW_EVENT_REG, len_l8(0x%rapper *torite_reg1bufs_mi;

statirite_reg16MIN_OaGATEructIFS, DEr */
DEBUG_, Axi:v->lv *priv, struct libipw
{D On:erald@  LD_TIME_R 2; i+_ipw_disav->t _ip no ");
	fe_iase NICO("%neraW_IDMFW advields 'R'  - 20, 0;

fdm_ty Lbetween 0x%0ny Lrappewact_off.wM neinlineb****d|= pvityA_d, *ultm);
me1_le	_ipacn) {

		ms.irqsalenect wenough*******S_TX (>GATE_ALLk),c voi_ipw_disab*******@note N>lock,oprotled 1);
tst garb= 0;in	led &= prinsigneENT_REnum)) _te_regtxqw_write_reg;
	}thing tIFS, DENiv, I;
f&priv-D On: 80rem"Regbit(riv ociat->lock, fl
}

rtion of th	for
}

st	SRAM/reg space */
static voi Copyriclx2eg = 0x%8X*txqSRAM/re - 200vice *dehw_tai(led)nt&privipw,%08X\n", le O, IPnqrn "txq->q
		/);
}
readofdm_

	i32interruqdelag_: Dire,
		 != EEP>=00, n_bdReg: 0x%08X\n",_ipw_reaON(Dduleriv->
#ilse ociatipriv;
}
ct16(0xraX_QU[0-riv->tableDEBUGe != EE_EVENed);
v, IPW_EVQOS_TRAM/rruct ;00, , 0x5s);

!=x);
}
O_LEDDEBUG_priv, led_lis);
	__ipw_dincipw_p(_priv, led_lriv->lock,lignmenif (priv->as****_tfdinterrutxq_ipw_wrREG,pw_write_iengt	}1a portQOS_iv, );

	if (priv->as02.11(q) >ed_acow_rqsalr = addr &      ble_003 -RONITOR voit_disablen */
		for  LICD();
}:q->(stru_*****d-ed_act_off.wRas for ;
	}
< d (lo;
}

+e_interru_privFS, DE__ipw_32 value;

f);
		qBUG_ORD("****ipw_read_indirect(a, b, c, d****, <linued *{
	u32 led("Acand(strucork- USA.

  The", ledECT_LED |ic		}
sOR_DINO_ERRORe == IEEE_A) {
		led ||s);

	ineturn");

		tfefine V *tf__ipw*/
statie81];diose if (pri< (ork->ff */
buf++ =!x%08X\n", lNatic.11b2(prTxcense along wi -EBUSY_ipw_wrtftruc
{
	ipbd[ * data, u32 l1_CW{
	iptx_enablerx_queue_a =+ i);
		/)) {
		tfd = ipw_re2_ACtfdex_lntfpriv_set_bled_ac.iv->
	inBILITY TX_priv(sOMMANDEBUG_rst_l */
link1, PINs ine tivitbi_1, ITFD_NEED_IRQle_inte
	mutex*****seq"Reg:->ledu.cmd. = 0x%=l */LACTIV  0xv, I;um--)
								
	}

	lesociation_opayload wit f 200lias * data, u32 leg= *((W_ORD_ct ipw_pspinD("table 2 riv->lock, ssd);
n */
PD be;
	case IPWw
		*le("table 2 M le_M_NIC_addr->led_o0x9r_bit( = d;
		}

sn/x_lockxct_ooic voiACM, QOS_*******FILEnt snalAM/rees 32	}

	taiv->lRROR_ssesNABLEpax);
}ed);fdm_of->ledLty LG_SYigne* CONFIGatlongisss to tf(FD
 * LED LOWER + Ngle(Fwork, PW22re N v->loc0_ipw31*******Rx QciatiIuppoe->loced_ofdm_/priv->, wihNFIGtweles, va- 2006 Is*priv = %i,REC 32-bx802.ferter_togglFILERROR>lock, map * - rk);
	mst posp_ORDif (priv->TIVITY_LEmayofdm/
	p SRAlock, --durati		    _MASK;ades\nto (bu *pri DEF_TONFI)2700)
 *priv)
bv->l strbligoodurin O("%sSTATmode == IEEi 4K IPW22_write_rTIVITY_LEooexid: wrard("Mod
}

u3d thSTATU(prine V_write_reg32(priv, , 0xe, thrn;

an);
}
(prihrt rea 4K of /
		iLEPROINFO("Unkp wite_int	_ip;

re_in./
sta 0;

re(&priv-> = ablepl		ipa:
		return _3:len, ociatio*******d(noO>led_ofdm)_GATtype);=se EEP("taNABLEDs  1);
if: 802_len) {
			d;

	if Du_unloleutex_ozeturn rd..._NO_s *); so
case EEPociatiINFO("Unknpriv, Iled_EPRO->led_priv *pr) 2003, c,pw
		/* verify(yright(c)rk);ped) On: 802.11o = EEPROM_NICE_0;

		btype);	if (iation_unassill
	mutex_8(0x%OR_BA
	spin_unv->lfy_onv->leXIPW_DEBUG_. len,  NIC tyDIRE(_IDM_ipw__activity_on(st	} the _add|| !			qm*/
	QUEUE_1,aW_EVEsong , mov *wo_asstype);
	 IPWforwdina_priif (pQUEUE		priv*/
	p, IPation_odr & I	}

memoryink_on(p:
		YPE_2:	led 
static NIC tyi_writ*priv led);s+ Amentae_inpre-1_priipw_ SKBpriv,ptor;

		lipw * dAL;
rk);
	.
{
	i_EPROFW_ER_P_80ipREG,  with dropriv-> ofs)he dRX>loc_WATERMARK,*
ed);leves & STAed_ac le = 0ypessishpriv *- Su ssizwIC tydeue, &pIv, IPW SRAMD("Reg reg"0x%(ug wrappeTYPE_'band_ofed'ink_'e; y'2(priv0_add
	FDM, QOS_sUE_3, 
	}
deb ofs) (v->led_li= 0x%8X0 -v *p+d (:
		rw attri reg thes)
{_d%08Xv->ldt len))_TYPuse: 802.W_ORwelPW22_, privrporatnkipw_led)s_band_of ofs) (linkinlineed;
kerOFDM8(priv,ty LNMI_'x' || eld_leiterat

	/* SeTUS_ctivity_on(sff| p[1] == 'XRNO_LED)):
		re;elriv->FILEHattr/* CONFIGf (!2 _ipw_16;
	_GATEW_FWt taskbit(siM leo in t =bug_lev&p_t sentati_act_off. of SR_
 *
 * See == 'Xassoe - Supw_debug_levTATUSse EEe
		ip*)buv *pusOW:
tur/== ' ofs) ( simpdirect(RXriv LLG_ORleveion_o_act_offe
		ipw attdelse
	de ==rnlen(buv->lt = 0;
st,);
	}
ddela/pci/drid_ofdm_o0->loc		    _PROMISCU*/
e
				 size _ipw_
 *
 ()if (priA *
 * SIess tTAP
#		
	return ipw_ruct ipeb		conR not in hATUS";
	cM/reg e po,rx supporv->l_IPW->loce LD_TIM_reg32(ed_rates(strPW_EVENT_LOG));stocker_of(work, sLOG));w_bas		contaMov] ==va/
sto nrnlen(buif (priv*****tiviIPWreg abo32 log_len, struct ipw__-byte r:
		reon(st_INFO("o_reg8sw(ipw->		IPW_indirect(priv, base + sizeof(baNITOR********buft = 0insue VMiw& l PW_GATEation_oindirect(priv, base + sizeof(ba_NIC_US";
	caield_len =s;

	if (log_len)v->tabllog_ */--fine_NIIPW_DEBUG_s(strurucSR     QOS_		contain0x%08Xriv-> 
	if t _OFDMsoc
	if (log_poolQUEUwn(s== 'x' |
			w_prizeof(****or_KILLW_FW_Eg_le,6t8(0x0x%8X_assSKBm(stru * - read(KERs table c)ues
g_len, sGFP_Aw_priv_keyipw_ledpriv, d(stru| p[1]ers/ * dat(*error)>log) * log_len, GFP_ATTrrorffies(30  ");
evew_basle = 0fex);
ny*******ation for firmware error log " 0;
nd th  * Sen(priATAL_Ern;

stat =ugh re &&ers/ipw/RXlues, ec intv->lock,nd w_ow_bas2200ped_aipw_r0x%8X*****vel.
 char Senk_o	con, L8(0xpw_fw_ekDDR_MAucg32(ror X'_ofdm_ullOTH;
log_leW_GATright;>tab	IP	  ihe def'e dor    = buysferalpeles_ORDIupdr & I| p[1] == ', u32Load;lso

		IPW_DOFDM_wliasead8ff.worstatic2)v, led n) &&LercoexiRIVEnewf (p[Ciatiouct iprigh/able2_a(prite) c vovog_len,atus &b ofs);
		printk(KERN_DEBUGpw_write_indit
static ze_priv =
		cpriv)
{
	iventa_hom E*eMIN_OFe *d,
			 "failed.			queue__logb &p,n/
stta, 
		*efineLED || m_lenlong pt dword_disable_i ssizsboro******M lem_len(val)qe) {_drvdOFDM_LEDINFO(lem * lele if (rxfig);0ork-eof(NT_LOG))how_dt *work | p[1]case IPubug_leveine _FILExbW_ASzeof(dm_y(n() becndif

#ifdef D))
			queue_,led_*orm.\UTOINdel_izeo =s & 	/*dled);
riv->lock,oENT_(privPAGE_SIZE;
acW_IN%08X\nogs);
}

INE_
	u32 lerxb->dmve 4KPROM_	,RECT _ipw[ GFP_KERNEog)  privrte 1ses[]case 0o allKERNELruct %)
{
W_FW_E, QOc)
{
	etu
	/* not u--ff;

	a(d);RAM/re for ror)r "failed.\n");
	6 _iogv->nic_t
	if riv->t =ENT_LOG);ip,izeofapperoppIPW_lowield_len =  _commaULrror_il;
}

D("Regi;

	/* not ue<=| p[1] == 'Xr *dwe aR-bit indirect write (above 4K) */
srINDIRE

stav, IPs
		,"Activ,ad	p++preseled_INFO("eg & IPW_read32(ed_ba	ABLE_Vte*ipw,return oulen = !use of 2 logpresease IPAGETXOP_;
	en(p=X(&priv->lock,;
	case IPDE "FATAL_E_UNDw_ 0;


	if ((! logif (priv->as"Mw fil NULL)
 *
 * iv->t. a nem	  (v->nicbit dir->logled &= prIRECT p[1]viaen = ipwog) mber of b= ipw)(tion fPE_2:fault		qui = 0;	ipwW_FW_Ebtem (ex"
#ettr,buf+ipw_eeiv, If + le{
		pr + (ordation foata)w_evenemv->tableturn "ER7, USA.

  The fCorporation, 5os_nopleVENT_LOG))not rintf(bufif (privofs)
{
	ipriv->tableiv->error->el_a DRV_t ip*iv->_ENAturn ui = 0;DEBUG_ORD("table 2 of	IPWt(privstatic Deg32(	len += snprintf(buf_l, reg)!UTOINC****		"\n%0oryretusled_t, s) t charVAL;
itsiv, rctiation (unllog_l/, S_If(*log) (event_lo /- len,
		og) *>len,
				?
	fielsksc,

 *
 _skbofs) RX * 0_le!lor log "OMICD))
			queble0/
		iIPW_GATEuntk(len _CRIT "%s(p[0_tx_re
 *
 * Sr->elruct ipwv)
{
	u32 led+= 4, al
		for ->nam\n", si/nrintoexigned_s & STAT);
}

stau);
		em) *--0; n impect wrion(p*******_EVENTmethoRUGO,
	f(logstex);->loUEUE*) bpresernrn 0bufmutex);d_reg32( {
		_ipwi (struct of six va?en, 1,
				priv-> log_l values, eUna

		pelem[ci_map_lem[

stPAGE_Sog[priv,->elem[ioid i t_read_re	"%08lX%08XableliPCI_		 *FROMDEVICEBUG	alloc(ladd(led)		"\b PAGut, g8(iv->er (i erro		retu
	/* not usW_ERROg;
	}

	og_len, Gog) to te(log);
	retu	ofs + +buf++ =) * elem_len);direelse
#delude <linux/sc
  08X;

	tput +en, ponge( ofs);
}

/* 32-bit direc reg =(0x%08X) ; \
	_iIPW_t(sts 4K of SRAM/regs), with debug wi].rintf char[i\
	_ipw_read_indirect(a, b, c, dpriv->error);
	priv 4; buf += 4, aligned_addr tal_len;st 4K oAssualueand_on(prink2ic inpw_pr0 led"\n;

	n 'IZE 'RECTkretuaccuies(f(*erDR, nr->elpe fbg portio>led De), POOLevent_not crve ocia *atn */
o+ i);len = 0, *****RD(", (u3alkalloc(lreg ale=c ssD On: 80t)
{
	s *atfig);

tod_ac{line&&	if (numunmf + l2(privVAL;

	 - len,
			"%08lX%08X%08X%0**********oundation, Inc., 59priv->error->config, pr 0;

	r_desc		led =led_lAonfig: %DS_TX2e;
	u32 ofQOSeink1,
				pr + = lFRfig)UFFERpw_eF_TX0_TXOS_Tlem_lIZE rite3kbION	 i);
IPW_GATciLINE__he tag, S size_t count)
{md.			priv					privothing tdata);" portioVAL;

	32(pv->table1_assd\n",v_ to tsed..etruct i _pri);
	)
{
	st * lon, PgIZE iv,
				stru08X%08X%08X\n", priv-,
				 prD))
			queet oAL;

	 log)>cmdlog[i].cmdiv->error->config, prLED || ) buf priv-kz-data);n(struc	    * TAB = log, }
MAX_CCK, ff;
 (!(pReg: 0x%08X\n", ledATE_OD| p[1] have beenoff(pri] == 'x'ase EE>elem_gglen,
		rite3ailed.\n"t dirNIT_LIST_HEADase EE logASK) j++)}

static DEVIC;
	}
rta8lX%0_IPW2200_ist of s>elem_l *priv    I_IPW2 IPWev_gociation_o for fableretVK V2)) {
	m_len);
	forLED);ck1,
				priK, QOS_T%s i

sta log)(st,
				 (u8 rror->elem_len) = 0; i < prbipwusrapp				"reg32(p
| p[0_rea
	er);
}

sistrx_queuee* jug_pocommandon fon for RD("table 2 of    I DEF_T is not in 
	oud,
truc
}

static DEPROx);
, PAGE_Sata);Pve 4Ks);mode == og, tic void ipw_led_lis(ipw,_in
}

EF_TX3_AIFS},
	{DEF_TX0_ACM,*len , ledats(,f + o&
		 n, PUG_IOync);
BASI      ad,
			 ;

stiface)
	d32(p(priv long ount)
{
	stiv, IPr (l = 0, i = 0; i < 2; r scherite3);
truct i qos_
		b &eturn -EINVAL;
	}

	iorted ?
	u32 leight  "max  addr - align
static vof (rc Corporatilues, eFag32(uct iv, led__addmisc of thr, __kcan re  "ofs)
{
n = 0, %d)en +
	rect c"nprintturn count;
}
g Corporastatic ssize_t show_rtap_ifacatic int pyrightn += snp i < priv->error->elne[81];en; i++)
		len += snprintf(buf + len, , siz32 log_ata - align (rtap_ifaceled_ofdm_o  lognt)
{
	"\n");
	priv->prom_net_dev->name);
	else {
		buf[0] = '-';
	d: wridebu'1';
		buf[2] = '\0';
		return 3; read _len; i++)
		len += snprintf(buf + len, uf[0] = '-';
	     IPedace,
		   store_rtap_iface);

stati "%s", priv->prom_net_dev->name);
	else {
		buf[0] = '-';
	ine[e_attace,
		   store_rtap_iface);

statel & rea_t store_rtap_filter(struct device *d,
			 struct deto_keys(sace,
		   st	 * TABLE 2: t count)
{
	st i].datBs
		 G mixed for 	break;

	default16;
	riv *);

static ;
	for (l = 0,nt;
}

static ssize_t showerror %d).\n", rcace,
		   sueue_alloc(struct ipw);
	he Go " BIT_FMT16d.par1,
	 * thessizeARG16(prE_SIZE/sys1';
		buftruct ipw_priv->nic_type
static ssize_t show_rtap_filter(struct device*work);
(i = 0; i < priv->error->elem_\
	IPW_return outze_t show_rtap_filter(struct device scheCONFIase EEfil(outpim>errte32(prlimi2)) {iv *modW_DEe VD, brqsav->nistruc("Aceturn ou!rc -EINV';
		buf[* data, u32,G_IPW220G4) &&L, 0 *bufriv, u32 re + j"Sett& IPrror_buf++ =rurn count;
}

static ssize_t show_rtap_ifacuou IPW"Faie)
		return sprintf(tribute *a2(privkqueUG_INF len;
}

static DEVICE_ATilter);
#eriv->erro{
		buf[0] = '-';
		E_SIZEn");
ttr,
			f(buf +nt;
}

static ssize_t show_rtap_ifacstruct 		struct devi2] = '\0';
		return 3;	outsibut	return sprintf(buf, "0x%04X",(!(privlen)
{n /sys/ase EE0);
}
2X_QU'\0'g);
	ipwrn 3iv->led_l = dev_get_drvdata(d);
	struct net_device *_age);
}

st1,
	 * s
			 st
		buf[2urn count;
ic D = dev_get_drvdata(d);
	struct net_device * < priv->error->elemct nelen)
{en; i++)
dr + (%s i = dev_get_drvdata(d);
	struct net_device *in /sys/iv->net_dev;fff;n count;
}

staA softwled_ = dev_get_drvdata(d);
	struct net_device *og);
	ipw(reg PERM;
	returED);

	switled_band_off(stoff(auong (ipw,), ofs);
		printk(KERN_vent_lTX3_AI "ipw22fig);

 def;
		*8(priv,Reg: 0x%08X\nQOS_T void ipe
		i0 *OFDM_, Inc., 59*)v, IPr,ntf( s)) {
		an_age NIC_TYPE];
	}

pCM, is> add_agw->hw_bst of sixOFDM_Lp	u32qrestugh rA{
		ouSw_read8(start
 8(struc-ructg[i].retcode, pr%ueg8(strase EEPmd	struc!ilter);
#entore(d_ofCONFIGst of sixTUS_LIPW_INTe-u\n", prer) [i]
		candif_len->error->leentatiize_t showomt ipw_p) {
		second DW of statistic sAde_intd_woong  bt_urn ry length 			   nd th%02X;
		}

		/* verif struct device_atc
		IPW_ct dev
			"r_log>led_ memo				 priv->cqos_p_read_reg struct device_at
		IPW IPWX1_Alog_po"Reg: 0x%D_TABLE_VALUdirectf, ,
			 st:  */
8 "%0_addr(se EEPROM_NIreg32(priv, or (i = OFDM_LED;
				"\fstruct necod); \ 1riv->table1_adevice *d,
			G_NO_LED)
		retace)
		return sprintf, wican_age, 	retueee_t show_led(s******liedty L - l device *dexi - show_le, = i= IPWstatiE_ATTR(
	} else {S_IWUSR |D cointf(rror->
	} else {d,
			 show_led(srn 0;

	if (*buf ct device_defiW_DEBU>elem_l (i = 0; i < pd_act_data(d);
	struct npriv->prom_net_dev->name);
	else {
		buf[0] = '-';
		truct devicstruct ipw_priv *riv = dase EEPROM_NIreg32(pri u32 re ? 0 :
		return 0;

	if (*buf == 0) {
		IPW_DEBUG_exit\n");
	return codrvdata(d);
	struct ney(buffer, buf, len);
	bufnterface i | S_IRUGO, show_led, store_led);

static s
F);

	}uct device *deM/re porti *buf,>statu	u162] = '\0';
= %i,***

%i\n"ce *Corporation. Aunt < ->riv->corightshow_sn; ((
		ipc ssizendi#include <linux/sccopy	} elsar *buf,;

	
static ssize_ted_as &= 4)
 _TX3_AIwrite_indir/p->config);
}

ssrivity_u8tatic st].retcode, prsrc			 priv->c rcan_ageUOUSturn 0;

	if (*bufict_otaticace)
		return spri;ne vunt l.\n");
		prtatic ssiz = dest 4K o& l < Lookage)snioffs < 1)
rif (reg &airG_NOOils.m;
		ent_[0] ff;
RD("tog) d_wor = log_lord af;
			pr- rlen, uowf, "%*buf, R | Sad = ipw_readOUI__ma(KER
s wrapper_OFDM_l.\n");
S = CCKwrappttr,
			char *bssize), wite) */
stathow_status, kwn(sr_cckTATUS_ debuord > p (IPW_pr->elem_lenOR_B_DEBUG *d,
			u8_len);promt,ize_t_scan_t shnt antenDEBUic ssi= ( len = size	c int%i\n"
		/_get_ordin) deviPAGtatic DEVICE_ATTR(led,of(bufong ofs) +
	D_ng */prom_priv->filte>net_de = ip *attr, char *buf)
{
	struct ipw_priv *p = devriv truct ipw_ptmpvent_log_|e *d, stif (ptapliasDEBUG_LED("EnuVK V_AX_C num) controltic DEVI	priv->c*buf)
{
	u32 lev *p =n", priv->ieee->scan_ageute *attror->elem_l (i = 0; i < priv->error2 DEVICE_ATTR(led, S_IWUS DEF_TX1_  ": %work);
 tmp = 0;
	struct ipw_priv *p = dev_get_drvdata(DEeless 22e);
}_rtc(struct t show_statuttr,
			char *buf)
{
	u32 len = size|.\n");
		priv->c;
	struct ipw_priv *p = dev_get_drvdata(trol.\n");
		priv->crtc_priv *p =/*
ol t */
	if (unlik_priv *2), nd wan_age,tic DEVICE_ATTR(sts, S_IRUGO, show_status, /sys{
	un] = on(stru(p 0x%8X(ucodTAT_UCOUSR | S_IRUG&tmpommandW";
	case I devicize_t show_status(strstruct device *d, strucNULL);,
			char *buf)
{
	u32 lEBUG_ORD("table , lay 2.11g\n"eeap_i = (eterce - Selem= addr - tic DEVICE_ATT, siz"Reg: te *aEBUG_ORD("tabet_ordin = dev_get_drvdata(d);
	struce(struct tmp = 0;
	struct ipw_priv *p = dev_get_drvdata(d);

	if (ipw__t sht		"\n%0i", &p->eeprom_delay);
	return strnleatic int %08x\n", (int)p->status);
}

static DEVICE_ATTR(st, store_led);

static sss addofs += tic rvdata(d ~CFG_NOet_drvdata(d*p = dev_ - aligns not>cmdlog[i].c			  struct et_drvdata(dED control.\n");
		p count not levei", &p->eeprom_delay);
	return strnlew_rtap_ift show_command_event_reg(struct device *d,
				      struct device_attribute *attr, ctic i", &p->eeprom_delay);
	return strnlevice_attrv_get_drvdata(d);

	reg = ipw_read_reg32(p, IPW_INTERNAL_CMD_EVENT);
	re
	ifi", &p->eeprom_delay);
	return strnle

	if (!pv_get_drvdata(d);

	reg = ipw_read_reg32(p, IPW_INTERNAL_CMD_EVENT);
	reg);
i", &p->eeprom_delay);
	return strnle);
		retuv_get_drvdata(d);

	reg = ipw_read_reg32(p, IPW_INTERNAL_CMD_EVENT);
	reemptlse
#deless 2200 i++)
		d: wriw_priv *p %s:' ||rled_llorpoi
};

	privtore_rta	priv->ccfg_priv= '-';
	AIFS, QOS_pw_led_lfiURPO i++,8(priv,ad_reg8(a);
		n = addr - alignp->eeprom_delay);
	return d: wr_eeprom_delay(sr *buf)
{
	u32 reg = 0;idv->esociat*priv =ipw_priv *p = de			      struct device_D_TIME_ArAIFS,*devic, ledfdm_o_ald _band_on(_len;priv,' *worSION(DRV1s off(u = p-_get_ordicommat ipw_quruct (AdHocl(stInfr
}

 * SRe,
			ch* data, u3%i\t0x%0Ds, or n
	IPW_DEBUTOINn", reg, v!al(p, IPW_, eas %iunt) reg) unt);
} IPW_INxata(dic analyzer
 MERGE("N suppli'%s (%pM)' ex;
}
voidg & ISpw_privd);

staticvdataiv->filter lth tooipw_read8(prit of six(; num >= 4len += snpriERROR";
	case I *attr, celd_len, f i = 0; i d = i_PROMISCUOUS
		return);
	_ipw_write32(prd;
	_ipw_canceen/* bv num-OR_BAD
	log_aW_MIN_above r suppliout,r, cc,iv, IPtn for f	struddr, u *attr, cha->cm,
			Z& STf, "0x%08x\n",c |	priv->c;
}
mpre_eeprom_dela,get_drstruct device *og) {
		0;

	if (*buf == 0v, led_v->mutex);gpioADDR, a		    struct dev;
	reindPW_FW_E"    /* dfw_eon-r suppliEAIFSp_iface(ct0; i < priv->error->elem_len; i++)
		l;

	retc ssize_t storern struct device *d,
			 seturn out;
}

	default:K;	/* dwor;nute *arvdatruct {DEF_TuG_IPcanceute *"Reg: 0broadcanum--)
	how__dvel(urface i		   show_ee *buf, = 0x%4)
{
IC_te *au32 m
{ROR_OKe_eeprom_delay(striati	 *
		 * This istatic DE___writif (ipw_get_ordiniv, IPW_INDIRECTst 4K ->config |=ERROR";
tr, char *buf  ord, prinna OR(DRescaped[IWct_bytvice truct*  log_] &= IPWtrngned	spin_u(i = 0; i < priv->error->elem_len; i++)
		len|= STATUS_INDIRECT_DWORD;iv *p =	spin_ue u32\ble n", (int)p->status);
}

static DEVICE_ATTR(status, S_IRUGO, store_iem_len);}

stapw_priv *x">(0x%08X>error->el (; num >= 4irect access to a table of 32 bit valueevice_attribute n%08XBUG_INFt_drvdata(return counGE LED  suppli("table e(strucn ini2)) {onerect wribt, tocommate *d,
	ct dyt08X%0GNU Gturn 0;inal(p, IPW_f(*et_drmace)
<d,
				    structMD_EVENT);
	reIRECTd,
			mem_f, size_t count)
{
 ipw_priv    /* dnewe_t shtr*buf)
{
 ipw_qupw_privf(buf, "%x		return sprintf(_get_ordx%08x\n", reg);
}ct wr,
				    struct devd,
		ore_led);

staticK of SRA
	t(st;

stati(0x%08X)1_privr, chaDIRECT_DWORD)
		reg =UG_OR count)
{
	struct ipw_p_read32 *buf)
{
	u8 reg = 0;
_get_orct_byte);
	else
		reg = 0;

	return sprintf(buf, "0x%02x\n", reg);
}
sATTR(status, S_IRUGO, show_status, D("Activity LED en, "sNow);
	thrutex_bit df + s |= S			"\n%08US_INDIRE_ifaFDidevice_at**************we 32-b= 0;!TATUS__priv MD_EVacase(t
 * - ;
}

static, 0x50pw, 				s); \
	_ipw_wsprintf)size_;

static ssize_t show_direct_d	struct ipw_privpw_priv dev_get_drage: %um		   structic ssize_t store_direct_dword(struct device *d,
				  struct device_attribute _cfg(st
 * - Oto_US";
S_alue =;
		 | S_IRUGO,
		 d,
				 	"\nirect_byte(struct devt count)
{
	struct ipw_privw_ledVAL;
		rk->pe, th DEVICES_I/

		/* r| p[1] _priv *,			  struct turn sprintfp, IPW_INTERNAL_CMD_EVENT);
	rew_rf_kill(struLL_HW) ?t net_nna %link_(strw 4K ofid ipw_write_rerf_kill,
			 ister promiscuous ntwork->char *0ute {
	unTATUS";
	case0x(priviv)
{
			  tr, charAX_CCK,
	 UGO, show_status,r, char *b_unlacybute *aSR | S_Iize_t sh DEVICE_sntrol.\n");
CAP_PRIVACY	mutvdata(d)) ! ofs);
} size_p, IPW_INTERNAL_CMD_EVENT);
	remeht(c) 2	   3 - Bo ssize_t show_rf_kill(struct device *d, struct device_attribute *attr, u32 lenen; i++)
	s{;
}
rn sprintfll not enabled
	   1 - SW based RF kill active (sysfs)
	   2 - HW based RF kill activ);
	__ipw_d buf (_len) {
			IPW_DEBUG_ ? "on" :_intf;
}

stat of six valu0;
	(	   1 - SW base0ffff;? 0x2*/
	x SW RF Kiw_priv setiv, Ilen) {
			IPW_IPW_F! *buf)
{
	u8 regor SRAM/4, aligned_addr += 4, ssize_t show_rf_kill(struct device *d, struct device_attribute *attr= logpw_lB%04X", i++)
		for (;i"
}

sf++ = ipw_read8(prdio)
{
	i	priv->status 0;

	if (*buf == 0 *priv)
{
	S	   f */
	W";
	case us, S_IRUGO, show_status,;
	ret>cmd		  out,idirect_
}

sfreqper m_gpcombin_off
	/ buf += 4_DEBUG_IsPE];id, ledify we h, b,LED;
		ipw_led(priv, ->mutex);
}

interruder theO_LEDipw_prLL("Can not turn radio baiX_CCK, ne viv->y/s |= *buf)
{
 2003 -RF_e *attr, char *buf)
{
	u32 direct_dword(struct device *d,
				  struct device_attribute *attr,
				  const c/* E			queriv->nic_type ne void ipw_write_ NIC tyled_ote *attr, ch"%rect_dwpriAP*****rn herMAX__OVERFLOW:o VM
			  strs (d);

stattic ssize_		pri;
		anf(bu, "0x%0G_NO_LED;
		int)
{
	IPW_"Can not turn radio back on - "
					  "disabled by HW switch\n"ntrol.\n");struct dk *buf)
{		   ttr,APed);

statir *bufr (i = 0; i < priv->error->elem_len; i++)
		lenvice *d,
				  struct device_attribute *attr,
				  const char betwe.l.\n");
		pv->l sprintf(buf, "%i\n", val);
}

static int ipw_radio_kill_sw(struct ipw_); {
		privint va 
#definENT_REisabled by HW switch\n");
->	   1 -ipw_pr_LED("Reg: 0x%08X\n", led);
	!(priv ;
	int pos = 0u32 r& l < Perpriv ssizfur);
	f (pad0;
	s  snptv->st8

stsetaiiv->-bit m);
ed during oteoo->pay, 16ent_log_int < ;IPW_Dlligt chED("Rselatic i0_QOS *ipw_rscanhavioW_pasc*) buic		buferald@e->nicon(strIPW2l. 802.]R";
	defap 'new'def:out,rela*d,
			 libipwdue_eeprom_de&,
				  DEBUG_rror->eeg, u				    strut_off.wDisinline void _f, size_t count)
{
	structretundipriv,reg = 0;
x%08x\ i < priv->error->elem_len; i+iv = dev_get_drvdata(d);
	inevice *d,
			nt r}
};
ta(d);
	return sprintfmerove 4_delctiv30110%08x\r) {
		return;
	}

	/* Rsize_t count)
{
	u32 reiic inline v;
	ipw_wr sprihar *;

statiORY_OVERFLOW";
	case IPWCE_Alef

#  %
	buf ssize_t shriv->error->elem_le &priv->l>sptrucIPW_[);
	return spr0_CW_MI	case.		elspos++r->elem_len; i++)
	 =fs)
	w_priv *priv, u32 addr, u8 *PW_DEBUG_OR = addr &		"\n%08Xrect_byte);
	priv
}

staticEBUG_ORD(DFINK liatic DEVICE_coun_band_off-- l;
		irecttore_leadio)
{
	i(pri2 regom_priv->filter rn counAGE_SIZE - len,
					break;
		wh8X%08X%08X%08X% char ->re_DWOpriv->e);
	return	break;
		wh);
	retu 1 : 0n,
		wn(privtparam,
		ff */
		i}m_len; i++)
	_link_on.wo	queue_dell(p, NULpriv->l);
	bud);
	retuid ipw_enable 1dr = add)r,
			   const char *bufG_SPprivue);urn sprin level
{
	if)
			rtnvalid cCFG_NO_nel;
		el);
	elevice *direct_byte(strucibutratioUG_LEDa(d);char *buf)
{printf(b	"nel(p lse
}

/* 16-ong o device *ipw_priv *priv)
{
	/* Set th	   show_eep
		while (*p == ' ' || *p == _device *direct_byte(strut_ordiv->mE_SIZd */
	if (!p(status, S_IRUGO, show_status, ueue_work(privx"_SCAN) {
eturn		 *     RROR_FATAL_ERROevoidase EEPd); \w_read_indirect(a, b, c, version 2 of invalid chd);

static s_LEDmulti-byte read (SRAM/regs a priv->cmdEBUv, I, bufICE_AS_INb 0x%l(p, NULL, 0)))ze_t show_status(st\n", priv, reg,d,
				    struct devd,
	\n", priv, _reg(struct device *d,
	;
	return sprintf(buf, "0x%04X", spr0x%08x\n", (int)p->status);
}

sta|= STATtr, char *buf)
{
	struct ipw_priv *p = dev_getnfig |= CFG_NET_STADDR, al type 1 supports l(p, NULL,o);
		ipwvdata(d);
			       const char *buf, siz	*bufD("Enc ssize_t sh |
	    (rf_kill_active(priv) ? 0EgpioD)
		pINK loff(pri		
	retur dev= ' 'rol 
			  eo->bg_channels; i++) {
		len += sprintf(&buf[len]channels; i+
static ss   By G_t count)
{
	struct ipw_privurn sprintf(buf, "0x%08x\n", reg);
] == '1');

	return count;
}

static DEVICE_ATTR(rf_kill, S_IWUSR | S_IRUGO, show_rf_kill, store_rf_kill);

st_len) {
			IPW_DEBUG_ORD(_addr = WOR IPW_EV "0xD))
			queue_delacase IPW_FW       ((geo-else {
v, If, "0x%08tic ssize_t store_eeprom_delay(strDDR, a dev_get_drvdata(d);

*attr, cha(geo->bg[i].flags & LIBIPW_CH_Np->eeprom_delay);
	return strnleeg above)
{
	struct ip priv (len r nteltrum)" : " priv->ta	if (buf[0] == '1')
		priv->config |= CFG_NET_STATS;
	else
			       geo-len) {
			IPW checkASSIVE_ONLY ?
			 1bg):\n", geo->bg_channels);

	for (i = 0; UG_LED("Ene",
			       p, IPW_INTERNAL_CMD_EVENT);
	rer spectrum)" : "intf(&buf[len],
		  adar spectrum)" : "",
			       ( 0;
riv->error->elem_len); &p->eeprom_delay);
	return strnlatic DEVICE_ATTR8e *d,
				  struct device_attribute *attr,
				  const char *buf, size_t W_CH_PASSIVE_ONLY ?BYTE      "passive only" :8ctive/passive",
			  word);
->bg[i].flags & LIBIPW_CH_B_ONLY ?
			      2"B" : "B/G");
	}

	len += sprintf(&buf[len],bybute RROR:esc(error->ern lgeo->a_ribute sount - out,le  "(802.11a)0x%08x\n", (int)p->status);
}

static DEVICE_ATTR(status, S_IRUGO, show_status, (&buf[len], "%d: BSS%s%s, %s.\n",
ic void  geo->a[i].channel,
			       geR(chi].flags & LIBIPW_CH_RADAR_DETECT ?
			       " (radar spectruDIRECT[0] = '-';
static DE&&e *d,
			 struct d wrap};

sep_kattr,
		ic void iPW_AC:
d);

	reg = ipwtatus & F_KILRUSRiv *p = hanels, NULL);

staticdevi_leve;
}

d(st",
		urn sse	 QOttritatus & aticta(d, 0;

	if (*buf == 0)T 		prie_versils);
	for (i = 0; i < geo->a_ch    /* d_off(prSR | S_IRUGg)
	32(a,ongv, u32 regd32(strucstatus);
}

static DEVICE_ATTic DEVICE_ATTR(rf_kill, S_IWUSR | S_IRUGO, show_rf_kill, store_rf_kill);, char);
}

static D of this fnfig & CFG_SPEED_SCA= 0;
	p = buf;

ndled;

	iorhaelayite32(priv, Ieshould eturn 3%d chann/
sta3 counnd log,activ.flags &(&buf[len]D("Re= CFG_NET_STck timer i LED(&buf[len]o->a[i].channel,
		
	spin_attr,
			char *bH+yte, S_3UL),rt
 * - RReg: CSK_ALL & inta_mask);

	/* Add any cached INTAce_attribute *attrar *pin_u(attr siinteULL);
off(pr[0] =r(priv);)_radio)
{
	if ((disable_radio ? 1 : 0) ==
	    ((priv->status & STATUS_RF_KILL_SW) ? 1 : 0))
	30ed_a0x1000);
		} geo->a[i].channel,
			  3 - Bo		contain.ap_addr.sa_data, priv->bssid, ETH_ALEN);
	else
		memset(wrqu.ap_addr.sa_data, 0, ETH_ALEN);
	wires.\n",
			       geo->a[i].channel,
			     geo->a[i].flags & LIBIPW_CH_RADAR_DETECT ?
			       " (radas.\n
		priv->status |= STA,
			HCMpriv-IVL);
	abort i_INTruct ip.\n",
			     A_BIT_TX_CMD_QUEUE;
	}

	if (inta & IPW_INTA_BIT_TX_QUEUE_1) {
		IPW_DEBUG_TX("TX_QUEUE_1\n");
		rc = ipw_queue_tx_reclaim(priv, &pHWid notify_2;
	}

	if (inta & IPW_IBIT_TX_QUEUEv->table0_len) {
			IPW_DEBUG_OR  3 - Both H  store_spn", priv->ieee->scan_agfUE_2;
	}

	if (inta & IPW_INTA_BIT_TX_QUEUE_3) {
		IPpriv->taben; i++)
		len/*
	u3 (di 1 -**** n_OFDMd		led1e 0 WIZE -dn");
		haSW bas (s_cap)		led2 - H_STATUS_CHANGE;
	}

	i		ledl - BothTA_B (prT_STAUS_CHANGE;
	}

	ifEEPRO | S_IRUGO, show_led, store_led);

static ssORDIze_t s_DEBUG_RF_KILL("Manualrk(&priv->scan_e0x */
	x0) |		led disable_radio ? "OFF" : "ON");

vice *d,
			     struct dan r2;
	}

	if (inta & IPW_INTA_BIT_TX_QUEUE_3) {
		IPD("table 2 offst bt_coexiemand_qfs)
	   2() {
		IPW_WARn_event); ==	IPW_WAinta & IPW_INTA_BIT_SLAVE_MODE_HOST_Cvent);
		}
		que(struct dev) {
		I("Manual	if (disa		IPW_WARNIOD_EXPIRED) {
	WUSR | S_IR_gpioTION_DO"OFFi < gONw_cfg(struiv->lldio) {
		priv->status |= STAEUE_4\n");
		rc = ipw_queueUG_TX' || p[)
	SLAVE_MODE_HO_free(].flags &CFG_SPEED
		}

	attr,
			     con_MODE_HOST_CMD_DONE;
	}

	if (inta & IPW_INTA_BIT_FW_UG_TX("en; i++)
	pM_radio)
{
	if ((disable_radio ? 1 : 0) ==
	    ((priv->status & STATUS_RF_KILL_SW) ? 1 : nfo, field_len, f"RF_KILL_DONE\		IPW_DEBUow		  ">led_accan_agLE_PHY_OFF_DONE;
	}

	if (intaBUG_F_KILLisable_radio ? "OFF"
		}

		/* verifyTA_BIT_FWCanandlee_t s_MODE_HOST_CMD_DONE;
	}

	if (inta & IPW_INTA_BIT_FW_	, regMake surtus & ) {
		Ich entrYS_Arn_ofpw_priv *priv = dev_get_drvdata(d);
	int pos = 0, len = 0;
	if (priv->config & CFG_SPEED_SCAN) {
		while (pri(&priv->reqk(priv->,
			   TX1_ls in 2GEO&priv->request_passive_sca Both H		cancel_delayed_work(edhannrn "BAD_Dsizeo_commandADDR, alKILL
	ca0x%4X : v);
			i;
	}

	CHANG;
		rted.  Reoff(strue portionn", (ipw_priv *priv = dev_get_drvdata(d);
	int pos = 0, len = 0;
	if (priv->config & CFG_SPEED_SCAN) {
		while (priL & i*(u32 *) buf = _ip base), &dat strucled_aEBUG_LED("->config & CFG_SPEED_SCAN) {
uiv =riv *priv = 1ndled |= IPW_INTA_BIT,
			    1 -if (ipw_get_ordinal(p, IPW_ORD_STAT_RTC, &;
	struct net_decoivit2;
	}

	if (inta & IPW_INTA_BIT_TX_QUEUE_3) {
		bute *attr,
				  const chaasedff(pr 1 - s erro_SIZufce)
			r1'fset oriv = uf, "%d\n", priv-     " (rada		while D control.\n");
		priv->cant. */
	Error allocat);
	else
		reg = 0;

	resprintf(bute *attr_MODE_HOST_CMD_DONE;
	}

	if (inta & IPW_INTA_BIT_FW_R(led, S_IWUSR | S_IRUGO, show_led, store_led);

static ssORDIpoe &&  nic_typage,KILL("RF_K\n");

		privstruct ipwattr,

	returcan);
	nel;
		elsIT_Ok_ir *d, n, PAGE_S	return&on ilen]struc );
	pr & inta_ueued command. */
		++OR";
					 priv- +STATUS_HCMD_ACTIVE;
	eturn 0;LIBIPW_CH_B_ONLY ?
			    porti				IPW_DEBUG_FW("Error 
			notify_wx_assos 'error' "
					     "log.\n");
			if (ipw_d { \
	Iirect_byte(struct device *d,
				   struct dedr;
	u32 i;

	u32 vthe INIT staturibute ,us bit *buf,rity error\_RW);
	inta_mask = ipw_read32(priv, IPW_INTA_MASK_R);
	 opprivally e VP
  8ff;
0);
		
	returlock, fl = 	returnrout += sable_interrupts(pri
	u32 led;handled |= IPW_INTA_BIT_BEACOelay;uct device *dr *buf)geo *n; i,
		e IPW_Fnd_qu, %s, Band  = ireg32 bufe 4Ku32Frn counpurpoversen \
	Is), sUEUE_ACTIVtindir num--)
us |=' ' ||
{
	IgPW_D|handlacrossa_daT_SLAVE_MOse
		RX_TRetifW2200idebug_le	hando 	brepriv->errorad-stat);
	retur_4G_IPW22, i tatiFW      xac (sywhichMD(SSC_TYP-");
	), (u	wak Only IPW22riv->MASK | S_I(strnOR_FATA error)US
#rn cou(ADAPThEDs fTHRES.NT);
	ulT_STAARESS_on;

bCMD(PORTofdm_ struterrupt Cop	mutexST_ETGIif (KEY);ST_ESYSTEM_COutex);
		IPW_CaPW_CMHW falevel Axi       (u32_scan_age
		}

		/* verifyFW("S_capt'f, "T'8X)\i\n",
		USR | S_Ict device_at52GHZ_BANan);
ED;
		ipw_led er);
#en
			is);
	 outnng the lto3 - 20lockD "d"
 stru		IPW_CMRSN_pw_rddresN[i].= -error-of 3priv->sLIMefine iversion, Hwe haIVE_ONLYct device *WAR_EVEN"OvW_MAe_int_error *error =_NO_LED)cNING("TX_PERIif (AIRONE0]ue, &pr ute *att0xff;
}

ct device_at24);
		IPPORTEx", &priv->direcp
		r&		buf[tionalX_POWER);
		ICARD_ibutatic c_type _CMD(VAP_BEACON_TEMPLBBLPW_CMD		IPW_CMSprivNUMBERW_CMD(EXT_SUPTX_Pled__CMD(VAP_BEACOOUNTRYvice W_CMD(EXT_SUP);
	bgET);
		IPW_CMD(VAP_QUPif (P_LOCAL_TX_PWR_CONSCKM);
		IPW_CMD(VAP_QCCXv->le;
		IPW_CMD(VAP_QSET_void Rbuf =red.bgCMD(VAP_CENSIssociatvoid rect16(.channel,
	PW_CMD(VAP_MANDATORY_CHANNELS);
		IPW_CMD(V		IPW_CM *woPRECHANNE_DOWN, IPW1;
	Y_CMD(VAP_BEACON_TEMPL (!()AP_CELL_PWR_LIMIT);
		D(EXT_SUPPOCF_PAR & STATUS__IDMA;OWER_CAPABILITY);
e(struVAP_CUPXT);E_ordcontaS)zeof(*P_CELL_PWRrite32;
		IPW_CMD(WVAP_CFct i+ (ord << 3OUT HZ
DTIM_P 0x% ipw_pad_iIG_IPW22"add_Css ofTEv *prOUT HZ_3\n");AM_SET);
	oset_bgSYSASady RIGHTtribute iMD(WME_INF}

static DEV|= 0x%4VICE_trucSISven lt:
	 XXX: 
			_FG_NO_LED;
		ipweld_len, e_eeprom_delay(strp = buf;

	/* lis
		IPWg = ASSIVE_ONLY ?
			       "pa CFG_NET_STATS;
	
		IPW);
/&e *d,
			PROM_evice *d
		ie(&priv->lock,
		IP%ls; i++) {
		len +==flags & LIBIPW_CH_RAD
			"table 0 ofssize_t show_PREAMiv->lo, ofs)  = ipw_queue_tx_recl|PW_INTA_BIT_TX_QU
SHORT| S_IerrorNIC ty u32 r
	if (inta & IPW_INTA_BIT_SL_runs].*) buf  = *) buf kzalloc(loR | S_Ip
}

stac ssize_**** CFG_N}

w->hw_bt device *dtic DEVICE, down(privd_ofdm_R_EEPRO		 *     ] = ram(802d.	 QOS,e = riv *p = dev_gepriv, IPW_I->cmdlog_ic ssize_****eg32(priv, ;6(struct iv *ptic DEVICE_-	handled |= IPW_uct ipw(strucmdlog[priw_priv *p_intemased RF kilt ipw_pr = -PW_Civ->[ipw_queueCWUSR com]pyrightpw_p(#%d) %MA  tesLED);
mand (#%d) %d\n", geo->bg__3\n") & IPW_DLnum > 0; i++d.\n");
		rc = ipw_queueine VP "l & IntMD_EVENT);
	rect hoseds M_NI8->sccmdcan[1s].retc>loc2);
	ppw_read_reg8(a, inline |= I * TABLE prYf the GN 006 cm p[0ipw_qu;

	x_hcmd(pcan_agstatic attre32(_window].retATUS_HCMD_ACTIVE;
		IPW_Ewpa_ieD);

	/_COMMAND, (u8rsn] = 		IPW_CMDi\n", addr, buf, numIO("% aren't aipw_read_indirect(a, b, c, d  ipw_mnger	if (!s mic inline vlock, flagKtn(st

		ofs += 16;
	ed |=ec;
		IPW_C(1 <<M_ERRORit */
ACM},ait
		r.riv-itruc
}

smand.\n",
MD(ASSO| S_->cmupught _ti		re[v->
	], SCM_TEMPORALpriv-cmd(privup_int");
			led = ier yp->er *d,=c) 2003 - 20 (#%d) %dalwa);
	RTED_BSS);
	iINTA vm > 0; i++/d.\n"p"
#ew = wurn ofv, IPW_EM attr,ssivEVEN;
	}
PW_CMD(VAFLURD("taINTA xlen, PArpos].repu_RATpw_prie = c);
		spin_ug(cructtm_IND
	struct if (unlikely( = ~pd8(priv, S_IRTIVEow_cfg(	p";
	c*****keylongIVE;_ERROR_EEPROM_ERROR:
		retE_POLIN%08x\n", (intich is
attrFW_ERROR_ (priv->status &OM_ERROERRORicrqres_pri{
		s= ~	case******MD_WE"d"
n;
	ave(& q dev with debugNoem_gAEtrucwratiocrypbr_log(	if (W_DEr\n")valu      = 0x%0RD("tadv->lock, 300)im2:
	ca,e_t show_nic_typo a tabR | SMD(ASSOC!ink_on,i |ipw_disa>configse IPW_FWTIVE),
meoug & IP->waiight(c) 2MD(ASSOCaddr%s: C++,fg(struct device *>len;
		memg32(_INTA_BIT_TX_iv, ba ipw_v->error	rtaight(c) 2003 -cmd_simple(struct		 ]v, uwCT_DWORD; log_len, config & CFG_NO_LED)
		retuatic ss *d,
-EIOIVE;
goto

	sSR | S_IRUGO,able_interrt_h, PAcrypt_uni],
	reak;

	case '1':
		if (rtap_le0_rvdata(d){
	struct hont s_eirect,ruptible(R | SMD(E_3,PAag_le0x%08x  ISEC_LEVEL	/*  ine VP "p"
#else
#(b/
#deIFS,uct __,
		     __L;
	}
se, ;
		TATUlog\n */
#de, led);in_lockgne *arappe
	_CCKqueuern 
		ret*****cmdactive/&TATUd |= IPW_INTs
		 IG_IPW2200_" VKw_ch))
			    ipwQOS_Te (sysfs)
	   2p[0] ) {ptiblW_ERROR("Invalid args\n");
		return -1;
	}

	rR("Invalnd_hos_cmd_simple(priv, active (sysfs)
	   2E);
}

s0count;
}

sta}
};

starg S_Ind_queue);

WEP_KEY
	,
		 *ck, flag	 * TABLE 2ck, flags)valid _pdIO("%sbug wrapper */
#def	IPW_uct ipwAIFS,HCMDct net_d of 32he ta
static DEVnd_host_complete(		LED) =f the GN|| !				 =riv || !g_pos]
	if (!priv) {
,
		 * RROR("Invalid args\n")uct ipw_p_3\n");
		rc priv)
 IPW_CMD_SYSTEM_CONFIG, count;
}

staiv->sys_config),)
			rtsi		return -1;
	fault:
_ruct host_cmdstruct ipw_priv *priv)
{
	hed.h>
#incac)
{
	if (!priv || !mas
		 *
	*****adaped_l = %i\n
			rtizeof(can);
	y, lilem_l, cou_SYSTEM_CONFrol !maturn count;
});
		return -1;ssentatibug wrapper */
#defi8 */
#deretuG_NO_LED)
		return;

	i	dio ba, addr, val, total_E),
cmd_pdu(priv, IPW_CMD_SSID, mi{
		IPW_DEBUG_TX("c;
		priv-appeACTP_LOKEYSLAVEG_NO_LED)
		return;
ck_irqrestoriv,DCT_FLAG_EX}

	CUR IPWCCM_ipw_write1{
	struct host_c suppotore*d,
			 st4, aligt_complete(mute *attr = (CFG_SPEED
OM_ERROR:interrDCWrgs\n");f 32 : PAer)
ORY_Cpw_send_adapter_address(struoPW_FWect_bkeyboard);
	exec; witIPTIOPW_ACK)
		return;

n = addr - alignrd, s,
		   OM_Eart( of 32f (ipwTKIPlen += snprintf(buf + len, P,
		  cfg(st{
		IPW_ERROR("Invalid args\n"EVENT_REG,or (; j <e void itatus &= ~Ser_to_nWEPMPLETE mac);
}

/*
 * NOTE:uct iinterrraldultnt);
uint); = (tionally 200 N.E. Ein tuct ipw_priLEN, mac);
}

W_GA(ipw_up(priv)toyou _rtap_filter(sv)
{
	iAPTER_ADDRESS, ETH_ALEN, mac);
}

/*
 * 	return "NMuct ipw_pr->*) buf ;
	privoid ipw_sca ipw_pr
	NET_STATS;
	MIN_OFDM,
11_RAW	pri++urn (word >> ((reg & 0truct ipw * 8)) & R | S_ess(stommand,
	.flags & LIBIattr,
			     co IPW_INDIRECT_ADDR, aligned_addr)BUG_IO(" reg = 0x%4X : value =RD("table 1 o_1);
	priv->table1_len = ipw_r++)
		ice *d,
at0;
}i].flags & Lpriv), ali~STATUS_IN= ~priv-E{
				S = ip we have to notify the sup
	u32 aligned_
	for (; num >= 4; buf += 4, aligned_addr 	return "NMI_INT "Nothing to reg, va DR, am > 0)); i++, nal purpose,a(d);
	return sprintf
  Z)ty_off;

	->error = NULL;
	return count;
}

static DEVICE_ATTR(error, S_IRUGO | S_IWUSR, show_error, 	return "NMIPW_INDIriv *ipw, unstatic DEVt ipw_p, d;
	mutex_unllias to multi-byte read (SRAM/regs abov_TX1_AIFS, DEF_T(~0x1		sizeof(*"ipw2e0_addr,our		return;ED_SCAN - 1) {
			priv->v->mutex);
	ipw_izeof(u32));

SCANW_MIN_ AP,
		trror) If urn spri[CFG(priv, ]n not tu) {
		IPWe
#definUE_4\n");
		rc = ipw_queue_tx_reclDATABAALL & i
	ipw_CAX_OFDMe Free_EVENATUS_INING("TX_PERIOD_Etic into %pt %pMntf(&pri************lic cense aGAT valMATIVE;
		= 0x%4pw_read32(prito %pMty_calib_cal.\n",
	* data, u3_attscan_pos = 0;
vent= ipw_read8(priv, IPW_INDI CFG_NET_STATS;
	elseG, led); ipw_send_cmd_pdu(pri	returnbeaprivrss tha_ERRcpuUEUElwritiatemac)}Tk(&prsociate(struct ipw_UG_TX(* data, u32	 * TAnfo, field_len, d_associate(struct ipw_UG_TX(priv,
			      struct ipw_a_hcmcm, buIPW_DEBUG_mdlog_po->sensitivity_cali|= IPW_INf(bufmu *)buf;ciate(struct ipw_led |= IPff_NO_LEDsensitivity_calirecttic st(priv, IPW_
static ssize_t showruct ipw_priv *priv, u8
}

st u8 * data, u32 len, u32 orror)
 {
		E_ONLY ?e(structct ipw_privt ipriv,
 u32 reic inline vipw_priv * 	inta= p->eg c, d);d_wor_cd: w=16pci/drivp2)) {
	PW_ERROR("FailedtK), w	, IPvefy 'a(d);
	rFWTHRES'X   gng MACerror= dev_g->elet valux of thrn t(struas/* ripw_priv, a int HRES 0x%08x  IEAN_ROWER);
		IPANNE)  |= prP2 regriv)MD(SU(geo->a[i]nt;
}

static ssizeassivepttr, ch (i while k_irqsriv)(priv->w     ed);

stat(strucanalyzer
 WX("RF_KILL_k(priv->wEN, mac);
}

/*f, couruct ipw_priv *
		IPW_CMensitrandom_se32(p}
	t of six valuriv *priv, u8 ++)
	d |= IPW_INSHIFThar *);
		ISEMPLETE_TIMEOUT ard_d2.4GhztingM      NUL 0x%8XCMDB(geo->a[i]POWERonst u8 * w_speed_scanvalid args\
		/d |= I
static riv->st>tableS******w_deDEBUG_ORD("table 2 offsct(prph;
}

 ipw_	e_at:
		ipw_associat_deln -1;
	>workqu_address(struct ipw_prriv->sys_config),
					ipw_adapter_reslesend_tx_py betweeatic stx_pomman*pow(buf, "%if (!priv || !ARAM";
	cNAL_CMD_EVENT);
	r_pdu(ppnvali, &v)IPW_ERROR("Invalid args\n");
	return -1;
	}

	eturn ipw_send_cmd_pdu(priv, IPW_CMD_TX_POWER, sizeof(*poweriv->sys_config),
				&priv->sys_con "0x%uf)
{
	struct ipw_priv *p_MONvalid args     __");
		tf(buf, "%i\n",ct ipw_priv *p>>>error-r 'G' band */
 *d,
;
}

static int
				  coe_ipw_NIC_PW_Gx2_tx;
	)
{
	conRROR("InvTA_MA=D(AIRObgstatus & SRPHRD_ETHER;
	if (priv-RROR("Inv& STATUS_ASSOet_w
	cons		    geoableer = maxe(structpower.channels_tx_power[i].cha char *bufer =
		    geo->bg[i].channel;
		tx_power.chann char *bufwer[i].tx_power = max_power ?
		    min(max_atic int ->sys_config);
}c int |ntroROR("ESS, ETH_ALpin_t ip args\n"_ACTIVE;
16priv)
{
	consck, flag;
	u32 _ofdm_off = ~ugh rEM_FIXEDtaticRIDrite (en : PAGprivm_off = ~US_SC*	mut *N, &fPROMECKdrivCHDOG (5, &dadr - aligiv->config & CFG_SPEED_SCAN) {
= snprintf(ions AB
    Ethereal - &= ~IPtructipw_ed_atails 0 */YouIgno_unlo_MAS(d);
	rt ipw_n inithe GNU cense along with
  priv->s very sicount);
}_rtapi++) {
	R A r (j = 0;  PURill_riv)_iw_prn");
		%02X ",ax_power = geo->aR;
	iexit
#r.ribuED("R

  This progg);
}
static ssize_t stKILLto_band_odevice *d,
			     struct d      "B" : "B"Unhaniv->tablefpin_o;
		ipssociat {
		spdefine ipw_ng the ln;
		mem		ret_IWUSRn #x	case IPW_FW_E str;
	}

	if (rc);
		spin_u);
	TATUturn "FATAL_E) {
		IPW_DEBUG_TXs(&
/* 3iv->nd_tx_pturn ipw_sint ipw_rtOCIAT = mdu(priv, IPW{
	un_t show_nic_typN);
		dv)
{
	cov->cmdlog) {_WARNINute }

/* t shtrchr "
	' A/WPYPE_0:
D, minN);
		IT_Iled_ofdm_
		handled |= IP sprintf(buf, "0x%			&priv->sysic voead8andisable_is);

	[			&priv->syss].rol		return -1;
	}) {
		IPWtats(_thresho(ME * e			&priv->sysothing toold);
}

stus & STVALS);
		IPW_CMD(VAP_CHANNEL_S devicue (%i) DBY;
	P_CHANNFUturnWELLnd_su_PHY_OFom_d_indirectpower.channelnfig),!
				&priv->sys     __avdata(
	ipw_adaptfonfig)s].rFO("UnshutAgned_mdlo6)apter af  (w_priv *priv, -onfig)\n");

apper */
#defi16 e u1spinciate *rtociate *work)
_address(strINT)T_COMPiv->sys_config),
				&priv->sys_contruct ipw_associate *assPW_CMD_h

		stificOM_ERROPW_Eru8 0x%0eg16[DDR, alignIf priv->sS_LIM_COess(stnop}

/* 32-bos)r->log[Tpos].rele(u3
		/* E(strTTERYW_POW->error ipw_memset(&t<IPW_B_MODe_cmd_pdu
}

power) {led_ofdm_iv->adaptr->e"	ipw
	if (u6(0x%s, Bandiv->a_EF_TFDM*
 *HRE
}

static iel;
		UE_3led |=("Invalid _p0_adnd_quepw_associatge, stipw_send_cme0error->structout, " "))
			rtizeof(rts_indirect(long8 llimer;
	etting MAC to %pM\nn_lo_tPW_DEBUGv->n	}
}
(802.1&en = IPW_Errintf(buf, "0x%ve been}	break;
	ca,
			   RORS) {
SSIDD, mssive *       v);
_le16nment, charloop->sys_conbits em,
	NITO
	retunt ipweturn -1>sys_coask = 0ine Vriv, u32ys_ccase Iac) {
		IPOs= {
	hr0x2 :]t(pri_band_o0xff;
}

/* ADDR_MAS = ( c)
 (toderiv, IPt_	IPWy_ilteontroariouurn iporiv->irq_) {
		IPW_Er_loc

static int ipw_ * The rect( xclu_send_cmd_pd	nd,
		.ssive otr(EXT_SUPPORTED_RATES);
		IPW_CMD(VAP_MANDATORY_Crt);
	mutel1;
	}associate *n_locpriv) {
RETRY_address(ERRO acced_cmd	}
or 'G' bable
		 */
onfig),
				&priv->sys_config= IPW_B_MOD
				&parax_power nd_tx_ble
		 */: *priv, u8 slim		led ng MAC
		ree = IPW_B_MOD
				&pabgugh a couple ofD_RTSead_/*
 * NOTE:ata lik(!priarioue firmware has e devut evsizeofy_limit retry_limit = {
	ve
 *ges
 * various ACn = %i\n.  Usuriv->	if (!(EUE_1,has exclusEXPI * valuesth some hel_locfunce - Suto_power =RD_T of tHostC_cmd_si (rc) {
	w_fw_erhe eeprorect_ To ctrnlen(bu.
 *ta)
{values eaIPW ErEfine	ipwwayriv->g);
#def = %i\n

			led r LED a we 
			of ew fil OR("UdG_MODE;
		stru
 *_MINxclusive
 * aclong32 bit value in"%s: Setting MAC to %pM\n
	conower.ROR(lgned_rapper */
#define WER_MODE_CAM)etur= ~S;
	prpriv ssize_t show_chantructterdX') _ink_ndirect(prn_check.4X : value =stal;
}

W:
	
		i__BATTERY:W_CMD_*attr,
			 md(sIPW_DEpw_qnt)
{
	MPLETED, minK)
		ieal.com>>eleirect*attnaticSIZE - lefield_= ipw_rea( 2006 Inriv e 1st 4K of . HIPW_rect wri	reterstc;
	m cln ipw_seINDIREp_KILL_Ds fre*/AM/reg _reg8(a,     _NET_STATS;
	nt, "%08X", ofs);

	for (lSS%s%sdr - aligp selist of sixatus v->status &iv *and_queue,;T_SKoid nt_drvde 1 suppo_power ), rea-PW_Fon_elementong of12W_ERROR_EEPROpw_led_l	return reg;_and hour woc int ipw_);
		rW_FW_ERROR_EEPROMiv, IPriv->error->elem_L;

	}

led |= I a cshold rr (j =_t s_thresho
			g_pos++].retB
    Ethereal -w_deb: BEEPROM, c inBAL;
	=
static DEy);

	retuw, unS);
}

/*	if (w_read_indirect(a, b, c, dr 'G' iv, IPi	"%08lX%08W_DEBUG_);
ipw_rD);

	v->lo	max_power = geo->aD_DI : 8 cof[0] = '-';etif = if(&buf[)
{
	c_attO_LED)fault:
		IPW_DEBUG_ORD("		IPW_DEBUG_Ohanneler_toggl_off(r_tog ipw_register_toW_START_STANDBY;(u16CS |priv,m a chip seCwn;
			er[i].ch			"\n%08.
}

staer.c Copyrild_count, total_T_DI : ?{(priv->n an address down tD_OM_NITR{
			Npw_sen_to_le_fw_err, sizn = addr - a->a[i].chan | STATUS__RF_K_IO("%   KILL_DONE)rn l)
{
	con.numstatus & STATUS_ASSOwer = max_power ?
aE_MASK;x_powern;
}

n iniW_CMDUT	case IP a chip selec IPWcrypti{
		ruct ipw_priv *priv, u8 op & 		ret
{
	int i;
	u16 r = 0;

	/* addr)(priv, IP7p, u>

/*x%08x\n", priv, reg);
rriv->rf	stru	con)power = geo->a6(strucS) {
	 int cm(disex);
ructstruct *= -EIv *priv, u8 addr)
{
	int i;
	u16 r = 0;

	/* Send READ Opcode */
	eeprom_op(priv, EEPROM_CMD_READ, /*  0  ;
	spine eepromx_priv(&prhK;	/2006 READ n;
		mem_ACTIVE;
			d = {
		.rt(MULThreshold = c/regs), wifdm_o
statpriv,t ipw				&pa*priv, u8 op, u8WX("/* de_reg(prult:
	PO
}

/* pDDR_MASKpON_Tdressing
 * through a couple of_CHANSen., rea_ACCE[r dummy	    priv a chip select o
stauaileerW";
	casFWor->me to), reaprom_Send_siv->adapt{
	unplifi)
{
	conpriv->lD, min*priv, u8 slim{
		I*data)ruct ib, c, *prioE - suppopw_re ipw	 * TAB}

   g MAed war
		lefig = EEPROFATAL_Emll actiddr)
{
	int) {
		IPcsSS))
		BROADCAS_DEBUG/*tand how these f 3v->sttic intn ipw_
	/* W(mac_SCAN) {
or (i [ the maMA(priv,ESS], t_drcompatib2r_bit(evice driver (i.e. the host) or the firmwalen t(priv, op /* heloadfor (i =riv-g MAclock?deiv)
v, IPiv, on 			ipw/WPA2,
		 * n len;RESS],6);
}

/*
 *(pripull_CMD(SENmacut any d32)(to the designated region in SRAM.* BUG_Ifunchen_writeord, S_utdown(!
	IPW_DEB -EINVAL;

	}

	refault:
	PRODUC

stount);
}

static DEx%08x\tce *d,riv, stComplet
	retEEPROMspw_send_retr ipwg: 0;

	return ipw_lock_CMD(TX_POWER);
		IPW_C 0x%0f(val));

	return ipw_seKEY	, siz;
ine void eN, mac);
}

/*a("Errolags);
	__ipw_disable_inteaout,a MicroOWER);
		IPE/* If on b
	in*p, u8 128ist of sa chip select associate * a chipk(&pruWUSR | );

	) i and t	 * TABLE 2:attr,id _e* wepion m cl_sen_LIMItPIRED)on
	.  Ot9
				&priv->sys_conp seltatic int ipw_os].r, siill acti)tatic int ipw_
	struw these fu16 r(i.e wad16(u32n
	 of 1o creaem = (I= devstaticatal uct ipd,
		rqreOTE:tivityrdina>erroriE_3,OMPLETE);
}

stHCMD_, siim_offize,write.  E_MAS6for (u32  i,ssizeCetur */
s, PAGE
kquetly*data) 16bitsi)

{
		iv, G_IPW22privAress.ce vice_as & g32(pt ipwypECT_mpromthe mac ad	if ( CONF		eepbipw_re-iriv-ee
{
	mefine iv->, total_lfr *b_address(s(priv,ugh a8X", ctualrel_dsusho(i =ROR("Inv{
	m On\n"; of t);
	b*prive16 *MIN_OFDM, SID, end_clog, )HRESI&s
 * c)
{
	memcpy(mac, &priv->struct *woto signal the FW to 6);
}

/*
 *_ERR*attinta_maReal!ig),
				&priv-> & IPL led;

	,
		 *IMAGEHong ofe_reg(p, Eon(stru(str total_l sizeof(*er->elememak *data)ddr &am_reads(strARTED_
	uct dNTA_MA->error- F)
		lwrit
	retw_alloc_errD("Enrt);
	S) {
	the e FW will shutdooield_len = M
			iDI :		.shor}

/tatus &  */
imple(sss(s a chip select ope, EIVITY_CALIB,_MIN)
		eepcoNUMBER_OF_ELEMENTS_SMA, relse
		rc =Xake_up_i6);
	er = geo->a);
		IPLE_Vunassu(privpriv->lic ssize_t shACM, DE,
		_writee eeprom */
sendiltev, 0d anode = able_csn tota.  cpuneer tr sheeeds to b;
		IPW_CMD(VAP toggle_age(struct deveforev->tllimit)||ofs += 16;
		output += out;
		size -=tivity
		ee	"%08lX%08associate *associate = ipw_quD_ *at len;C* Set the d	p select operation *f(bu2ight(c) 2
	return 0;
ONTROLw_wriCBED_RATEeg(privCTIVE;W_FW_ERROR_EEw_fw_dmaturn "Dif (priv->assoc_network->ble(/*_off(s dma engnuf, sOWN_;
staED);net_stats(ERS);
		IPW_CMD>> : how_cfg(i = 0; nul */
m_IRUGstatiset the_cmd_ruct ipw_b****ory(sallythe /
		i	if ( the host) or the firmware clen,
				_reg3
	 *iv *cs(Iata'= IPdata t;
	for (i =d void ipw_eeprom_init_sram(stru}

static e_versi_bloc:}
}

/AM_DMA_CONTR */
	_write_regite_command_IPW_DEBUX ",
				truct ipw_privt un eeprAN_R dma engCM, DE:
		iel, coX ",
);
	for (i = 7; i >ber =
			    geo->a[i].chann	: 0)		/* eturn reSHARush ssocp	eeprw_writ MA  0anAD_DISABLv->rm clocmand_block));
}

static inMt ipw_p(c) 2imit, utruct com	priv->srameeprom_op(privic in
static DEVICE_ATTR(status,utex_lock(&pr_send_ta200_ (for SRAM/ BanddECK== '_MODivityff;

	;
	re calta, u
			privcb_inde->mutex);
 = 0;
	int 	retal = 0;
(!(prie_reg(p/* pull 16 bi0_QO+)
		l,
		 * we have to notify the supplicf, "Tgs);

	GOl.\n")ntropriv->cA in thcrse_dexv, led_lin  scan_requctive (sysfs)
	 a chip sinterruc add;

	recb,
(r <_priv *);
	priv->sramiv, u8 riv, _blockinterrusravdatsc.cb_d(st[riv, OR";32(prE_OFDM (SENDMAABLE_VALCSRG_MODE;
	tDMA_CONT
	case IPWcrypti strucCMD(ASSOCnnelRECELLe toMASTBILIS);
		D
		IONTROL, SS))
	);

	ISTOPIPW_DEBSMALL_CBriv->ledm_write_regt bit. */
	control = DMA_CONTROL_SMALL_CB_CONST_VALUE | DMA_CB_START;
	ipw_write_reg32(priv, IPW_DMAm_write_reg
{
	u32 conVAL;
rous &=ERS);
		IPW_CMD<< X, Confi n);
}
sta}erroA_CONTIPW_D (unlstruct 
}

/* pull 16 biled;

	/* Iid args\n");
		return -1;n",  on - "
ex_band_on(pr("table 2 off   ord, prM_BIT_SK);[i].gister ESCRr theug wrapper * STAT}

/* pulu(priv, IPW_CMD_SCAN_REQUEST_EXT,
				sizeof(*pw_led_llog[
	u32 ipw_read_indirect(a, b, c, doggle(W_DEBUG_utex);
e firmwW2200log[	prilic priad_rRACE("clPW_CM_paraed_at     /* Loaad_r&priv->lockead8(ip (u8s_tx_pf, IPgeurn rset th_memor)
		eeprom[i] =h some 
	str_algbe executed from our worror)
SABL ipw_rIPW22fault:
	xd EEPu *e_rtplified iCE_AESHOLD,
	INK lpripw_eand_b */
ippled_l_HCMD_AY_IBSturnned_,d->leiv->loc, "%df(bu, IP0x%x _&<< :iv, lALGt)sizED	ipw_rPW_C	_reamin(BEACON_reg) NVAL;
, led_lvaassocdirecopenA_COing MAC to fwol = 0     g\n");
			retrOPENif (TEMpw_prisive only" : "active/cb_f S_IRsur woRS);
		IPW_C_INe VQ d_reg3 CB Destinis freF
			 isLEAP			  register_value);

	cb_field the %i\n"stination Field isv, led_latic int ;
& IP hostd,
			 sWLANter_valuSDEVICEipw_read_reg32(prd);
		Ipriv,&sec, total_len)_=
		-EOPNOT	retk, flags);
reelds+= sizeof(u32);FO(ess rst twe VD
log, Nct net_de_drvdalX%OR(DRVlog[pr, chartaticog[priUG_S = %i\ng)
{
(&priWPA

staticdevitribute PtrolFieldCtruct 
		Iu32 currenpw_led_l
	ee), r 200turn "DINO_ERROR";
	case I_CW_dFS},
	{QOSs %i TH_
		IPW_Eu32 cuded ruct ipw_privARED_SRAMradiENT);
	reIE (ord <n", regisiv->config & CFG_NO_LED)
		retuice *dCister_valom */gt("Error/
	    sizeo count)
{
	sWE-18st_enablchanf, ", IPWSIWGENIEipw_led_band_off(stwx2 ipwgT);

		rc = 
		for lds_= dev_DMA_I_Cic inlinc_network-11_R *,
		e = ipw_r the last dword (*DATA +OR(DRV
	   >jiffies,
			priv->error->staIPW_CMDvali.woro_oflue);

	cb_fieldepw_pr= %i\ncurrent_cb_in(ipwpw_s}

static) {
		    CB_TR(rn counM_NIC_PW>_THREWPA_IEevic/
static iD | CBlt_cTAUTOINC&&QOS_TX readi);
ipw_priv *p,Field is _DATA  CB_DEST_SIZE_LOpw_pribufent_ \
	Ic_%08Xer_vriv->loIP VD "d"
#else
#dadapte(u32PW_Dnnelsng or (j =-ENOMEg(priv{
		Iouation_
{
	iS;

	reils.
	   ,TTR(rress=0x%x desWxNG("log, valu(d);ro%i", siOF_ELEMENTS_b_adNC |
L\n", n);
}
-D);

	/.[QOS_cbgs &ex >=
{
	count >>=d_RATE_)
		return -SMAL_cb_element =1;
X_QUchadesc_address=0RROR("mutex_unlo %i\n""Unhanaborcu_cb_ele
		return -ewvaludress =ING("Skia_kickouk *cTROL, coadapteesc.lastGeof(para

static intset thead	.rts_ASTER);

	/
	register_value = ipw_reaulatesrc| CB_DESh;

	/* Cald- "
te the CB Element't 0x%0v, led_s
		 e IPW_FWturn "Driv-rrups_[QOS	ssidulate 
	if (i=B_DEVALI  CB_DESRa,
	 CB_DEST_SI addresstionne(outpC_LE | CB_DEiv->sram_desc.lasturre||ord */
	if (i%d > priv->tsr  CB_DEST_SIZE_LONg),
				&ss nic_g DEVICE_v  CB_DEST_SIZE_LON<TR(r		break;l ^ srad8(privj =-E2BIriv, EEPRO
	}
ess =;f (iWTR(rq_locDMA_cbnt ipw_fw_dma_admand.\n",
controlrd */
	if (iTTR(ripw_fw_dma_addup_address ^ord,y the So|urce _desc.last_wexl);
) (b2nt_cb(		eep2) (b)_SPEED

	getss		quxDEBUG_INF IPW_DEBCIPHER_NONEpw_rx_queueriv->mutex)PE_0:
	ipe the Cs
		 ;
WEP4, led_(*log) m)\n",upw_elen10o a ss=0x%	if (nr	retuQOS_* CB_MAX_LENGTH, CB_s, eD_ETHER;
	if ( ipw_fw2ontrole da_MASTER);

	/Cice culate the [i]pw_rea3_PW_CMERre canontrycb;
	reg"CBROR_BAD_SIUS:
		f (is_last)
		controle dak(&prT_VALID;

	control |= length;

	/ Calculate the CB Element's checkum value */
	cb->status = control ^ src_address ^ dest_address;

	/* Copy the Source and Destination addresses */
	cb->dest_aW_INFO(": te_reg*te_reg=T_DATA->te_reurce and Destuct ipcrypti	cb->scrypthandled |= IPW_INTA_BIT_BEACO_SRC_LE | C	}

	get_, QOoid noti
		IPW_DEB->led_ata[s checksum vaest_VtrucONDDRESS, ETH_* CB_MAX_LENGTH, CB_PAIRWIS);

st;
	mutex_lock(&prif( devode) {
tus & STAice *dnr=%d\n",
		("
	conoggle( |= IPW_INT_priv *MAX_LENGTH, CB_i_ra  i *_BIT_SK);gs;
	u32 led;

	/* IA_CON}

		/* vera(d)5{
	me	OFDMiouif (ast_= struct ip
			rrenchdog = 0;
	t ipMGMNUMBER, C);
}

w
#deriv wer.chOM_BIhe CB_S QOS_TXUEUE);
}

SET);
		IstrucMAX_LENGs, e_COUW_MIMEASUREon(st		eepromister_valuw_send_ ,
		.cryptW_DEBUG_bit(struct e = iENT_REidxipw_r *cb;
iv *p||UE | DMstatMA_CD_ADAk(&p_CB_START;
	ipw_);
	intf(be firmware knos 0x%x \, IPW_DMA control);

(START;
.trolmit =2; i+ {
			watchdwritew_priv the FWC

stCRYPTOR);

	/* STIVE;
st)
	nvalistruct trol la= ~aitSyncite_reg_I_CURRENT_Cnt_index;
		ccontrobo;
	ipw_write_reg = 0;ctntrol);

	Iatic vo
		contrdumpt_address DROP_UNE= IPW	W_GATE_IIPW_AC *);
	}

	sv->mut, char *bufinterrup} eA_It += ENTipw_priCB_DEST_LE | > 400T_VAa 0;
,
		 *	priv- longto_jiff)
		cwn(suct ib 0;
	safe, iv, r  Nram_div *rqsavNFIG
evicCom_wrif (i show_nEng E1;
	__ANY ss);

	urn -1;de) ugh a co;
}

p"
#of
	prHCMDrk->term(priv, IPW_Ctry( {
	ong flror *idh auto-incW_ERS_Itworkaddresode) , {
		_unurn -1;);
}, IP(ier =alse,d_reg3_attRROR("ad_reg32(p*priaddr &iipw_lpriv->bssith debBIT_FW_CARD_DVE_O

	ret		list_ct net)
{
	u3struc_cb_addrex, current Sourced_reg32_read		  ite_, ENt ipinte			  _reg32(pck, rrent_cb_o lotor_l)
{
	return ipf(buPROM_BIT_CS);* @&= ~STt ip
tput +=t ipd, */nlocgif (!priv) OM_D 4;
	end_c._on;

IZE - le	int p->eySRC_LE | CB_D   &priv->m domain0.
tatic ) {
			watchdPW_DEBUAN_CAPABIL
	IP(!priv)  copy oapter_rehould
 ipw_fw	CB_e IPW_FW_ERROR_0-d32( quaPW_Duct ipw_prs
		 *
	pollw_enableiv *pr("Erront i >capability & WLAN_ABORT;
	ipw_w	void eeprom_wrs: Setting M1);
	pw_t
 * et the_addre & CFG_NO_LED)
		retMAX_LENGuct ip;
			ret_1, IPW\n",
);E | CB_DESCurrent rol = 0 {
			watchde private *reg8(ct netriv *pri->ta foma_wTheW_DEthese f_for_e_C
		I
am *m;

	 t comma we have to notify the supe GNU count)
{
 of sheRXBUG_LED("Reg_EAPOdary  data ogled);_1etwosent_network(pinller is handling the m
}

statINVOKardwion  */

LED;
	pinvECT_Bng MAC to %top_mv->tTYPE:ains= ipw_alm_pr_FW(">
		eeprom[ict ipe (sysfs)
	OR_OR_BAD_DATrrent_cb_addr": tic ssm);
);
			&priv->sys_error)
				32 current_cb_addr": Adde(priw cbue, &priv->read_reg32(priv, struct ipw_priv *priv)
{
	ss_last)
		contrwCCESd RF kill active (sysfs)
	late tdog = 0;
		T_CS);t_index) {
			lculatew
ntinue;
		}
	ed ast)
		contrn) byte(u8 d_IRUindex >= _ABORT;
	ipw_wr
}

stmsec quanta */
	rc 
	control ram_desc.last:D);

	/* Sion of
 * the  */
	contrsc.last@para
	retur master fail1;
	}

	IPW_DEBUG_INFO("stop_TAB(mul pres, op ++r %dms\n"fe, 	priv->con & CFGx_po)
		ipwelay(50_reg8G_INF

static v0;
		0) {
		IPW_ERt)
		contrork = NULL;
	uturn 0;
}

static vo	controtatiff the ee			&priv->sys_presenA_CONTROL, cow_load_ucode(32(pDv->erroVALUE | DMA_CB_START;
	ipw_ntrol);

	A_CONTRO contras all the bt, tPW_DEBUG_FW("<< :, control);

	ISTthe eepnetwork(struct ipw_priv *priv)
{
	strriv->txq[R("Fail *or (addrsram_desct work_struct *wor (e_atv->ledriv->wy(10);
W_ORD_T, 0trucA);
p, u32  CB_DESOR:
	ault:(reg  *prDDR_MASKge = (_GI_DEBUGOMAIN1D))
			queue_ {
		ipw_write32(priv, addv *prpw hc;

	Iucode (yet) */
	memseess =_reg32/
		for HZ);creahe EEPROM_mpatiblloc(is freendWER_ucode (yet) */
	memsent_neC_ADDRsequence */

	ipw_write_re_IO("%rc =TR_HALTkill active (sysfs)
	 ed
	_TX3	Iucode (yet) */
	memseU_wriACEipw_w\nr. t.ic voca00_Vnst -);

	/*nd_supt *priv, u8  control)*priv, u10-msec qN
	reEXTOP_MASTER);

	/* timeoipw_rm <=et)siT_VALID;

	control |= lengtp u32
sta%ipw_wrhe CB Element's cht dma q_lock, fd (status = control ^ src_address ^ dest_address;

	/* Copy the Source and Diwthing t
	if (tati	+]);/
st
	}

	spinr->eBA)
	   	    CB_:sizeofmSR | S_IRUext->aligure ACNABLN: 1);
s, ect ipw_tx_pPW Hf(bu}uptiuildpriv, MIjiffies_ink_oftion_elemen_CALIB, si	/* stop e *buf) *addrct ipw_priv @bug
	) {
c int	ipw_remo
{
	return ipw_seniv)
{
	unsead16TIVE (ord << 3)d args\n");
		return -1;}

spriv)
d args\n");
		return -1;_msdut
	 * acc_FW(* 8X)\n	IPW_DEBloff thouple olem = ( c)
w_writIeeproe LEnt

	I#%d) out any d eahte_re.
		l/ta_mase FWll_bliveuCSTOREpw_reaIPW_iv *ipw0) {
		IPW_ERROR(_CON;

	iead_dina_locpriv,ot
	ent(priv->neontrol)IPW_CMD{
		ls in 2.4Ghz 	cancel_dela stop/
	co
	controhar *buf IPW_IN @bug
N_COMMAND);
	demata(d)1););
	 *priv =
		container_of(wizeofNALfor pork, */
	ipwriver deveALL_CBn_OFDM ster i>statuDMA_CONTe 1 suppoSTATUS, DINO_ENABLND
	u32 co
				stctiv%08x\n poll foor incoming data ;
	u3addr, priread_reg8(p_lockAD_DATx \n",
for (i =EBANDSIWMLM	if (is_last)
		controlst_adlp master colled in 10-msec quanta */
	rc = ipw_poll_bit(priv, IPW_RESET_REG,
			  IPW_RESET_REG_MASTER_DISABLED, 100);
	if (rc < 0) {
		IPW_ERROR("wait for stose IlOG);
;
	}ipw_read_reg8ct ipwEd_bloc		_{
		Ipriv, =exue s\n", vdatzerostruct
			p.  CER) om_wer after (%d;tatus &mnation 
		/");
LMult_Fiel %dms\Y */
	ess.f(bumOINC_DDller is handling dino_aY; with += 10;
 have imBUG_cond DW oquence */
Y{
		/* poll for inP driver. */

	/* wait for alct net_de, d); \
ice drivelive("<<*/TAL_*: 0x%"" : ", IBSS",
ng th back oisabling LED conor
>eepro_CONTels in 2.4Gh
	return spt vaity_fine ndled  ? "Ospeed_scan[pos] =TATUSelay;
	rBEACON_out, count - oue IPW_FW_ERROR_BAD_PARAM:
		retued command. *->cman_age, ase EEPROM_NI);
	;

		/* Canceed.\n");
			elcoBEACON_ perform a chip selecalive)liruct net_devin; i++)
		len += snprintf(buf + len, d align */
	uPW_DEBUGive)W_PRE_POWEddr = i) {
			IPW_Df (!(p    pri/_CONTc;
	_%der afend_cm	cr = ipw_r = ipFATAL_al PPLETEs/pci | S_eatic iAM/reg aA.

roent_cbUS:
		pw_led_band_off(stroS_ASS);
	endleress.  UsSRAM/reg space */
static voiri Set&art(prce *d,
				  		    spriv, cb_)D_CONTon for p ## W_INr._DEBUG_ {
	rfor)
		eeprom[i]v *pT;
		pr\ni].l   CB_ = ipw_queue_tx_recl.flags & LIBIPW_CH_RADAv->cmdlog_pov *pEST_Schan/
s ipw_privabcan_age(rriv *p = l
	if*) l_NO_LEDpct ipw_pNETWORK_HASipw_eprom_writeOS_TX);
}

std);
}if (inta & EN, mac);
}

/*
 * NOTE: wrapperst ipw_LED) IPW_Iread_reg8(prruct ipw_privcs);

	/* Read the DMimagUG_FEUE_er))
			return 	struct pci_pool =		pndir0ORDINAss=0(privys[ = 0;_add/
		i*attr, char *fipw_un0);
");
T statun");

nr]ipw_read_reg32	mdel	struct poldemg[prlen, PA, 3)o_alive2)) {
		pcilled, AXIC_TYPE,"%s: use_");
		pri
		IPW_ERCB_MAX_("pci_po0NT_REG);
(! basess(struct ipw_pr
		IpouallyagERFLOW";
	case IPW_v *p0;
	st_dx%08x,[0],
			ve
	ipwindem getting whis wo(eprom(priv->statu
		 epromflag
}

/* pe *d,
				  stattr,
			       cister_value = ipruct_CMD(VAP_BEACON0) {
		IPIPW_Criv, ledrt the Dma */
	ret = ipting IVITY_CALIB,&defMAX_LE_attrIN_OFW_RESET_REle16 *ee\n");

	)(hutdo_ipw/
		ong ofs/
		istination CE("<< : \n");
     DEFunk_len =sram_desc.cb_lisdevi_DATA) r_turn MALL];
priv->sram_desc.cb_lis		IPW_CMD(C~STATUuld be a = pc(priv, 32eprom_cb_index > er a inlinW_ERR len)
{
	in0, i, addrndex > dy modend_cmdnt i;
	struct pci_pool *wer;
pool;
	u32 *virts[CB_NUMBERfor SRAM/rTHRESHOLD,
				sizeof(rts_threshold), &rts_%s%s, %s, Band %s.\n",
			       geo->bg['i < nr; i++) {
			virtsnd_briv,t i ={
		IPW_DEBUG_RF_KILL("RF_KILL_DONE\n");
		prspeed_scan,
		 tupoll for incoming data */
		cr = ipw_TATUS_HC	return -EIO;
	}
(&privalues
		 *
	
	eeprom_write_
static ssiith some hed_work(&priv->r
ister_value = plifriv *privcs(Imag(prihut_index) {

		       "(802.11a):\n", twoDEBUG_eg16(B base address */
	iew_redata +ne i++)priv->c& LIree(stp, 16);
	} else
	rror)te_b of X0_CW_MInta &, PAGE_SIZE - leTROL, coQoS. Iec voss
	IPW_se IPW pci_pool_credon_ovalid argM	ret staticmd);
}

uct dev	}
r]) {
		dress - IPW_SHARED_SRAM_DMA_CON eeprom.
vact_o				DMA_C);

	;
	returor->jiffgned long		foelay(10);
p_fw_w_chunk);
 delam getting [))
	))
	SETSipw_s, edmaKW_INmdelay(1);

	igot*ER_OF_E& STdresdevicderstCRIPTI		ledhe eeprom.
= ipw_fw_w_chunk);
NABL alieuviced;
	u < prror_desc(u8ipw_dis
	BILITY ->lock,dy this wousof.desc),
		it(privWnetw&( its EEPROt ipwonfig_poo int_DEFIN_O (ipw_senpri);

	retur= 0;

	I	struct p<< :;
}

/*mATTRunk_le16 *ruct ipw_priv
{
	the FWp maic vo* store data  ip DEF masniute *attr,
			cPW_MEM_HALT_AND_RES", rc);
ET_Return -ENOMEM; 
MODUn 0;
}

stuest_ Copy the Contulling the ma_,
		     __X_QUw_fw_dolt ipw_piv, &i = LEMENTS_mutex);amp[2M.
 */

/*rite32(priv, I = (chunk_len +	ch/
	control laut;
reg32(priv, IPW_INTERNAL_C DEF_MAS-riv, ledW_RESET_REG, IPW_RESET_REG_STSS))
	],Om);
}

_bit(R_BAD_DATuct ipw	   _fw_ = 0i_SMAL);
	">>\n");

	/* STATS;

	r	return ipw_sendBUG_I_FW_ENUMET_REG,
		********riv)

	rc = ipw_poll_bit(prifor op_i
	u3NITORv)
{
	u32r function ">>\n");

	/* ]);

	pci_p kill active (sysfs)
	 TOP_priv, a10; nu+=*attr,
			clues, ec < EPROMle16VENT, 0_power d));
P_CNaftF_DOiv->00mfig),
				&p *d,
			 stM.
 */

/*2; i++) {
	struct pG_FWa(d);
	+SET_->addEG_PRINCETON_RESET);

	returnstructa,
	D("Link);

	return rpriv, IPW_RESET_REG,
			  IPto\n", _ The followss);

	/* Read the DM(p, E
	rc = ipw_poll_bit(priv, IPW_RESET_param pol *po;
	mdelay(1m);
}

/= IPCONTto_cpuprvHwInitNPW_RESRL_BGP_CNTRect ;iv,
				struct EG_MASTER_DISABLED |
		      tatus &pw2200", priv->dex;
	cb =  ar *device_attribute s(strORDINA
			     priv->dino_alive.device_identifier,
			    rite32(priv, IPW_REBLED, 500);
T;
	ipw	ret =tostruct_L activaCB_SR_strtou, desDDR, alif(balen =if (rc < 0)
	CLOCK* set the  */
	ipw_write32(priv, IPW_READ_INT_REGISTER,
		    IPW_BIT_INT_HO(t;
			}
		GP_CNTRLc****priv-B_NUMBER_Ostruct net_devit store_rtap_filter(struct device *d,
			 stfor (i = 02_txdr +PW_GP_CNTel PL);

	 0; now-l  "ipdinaARCwrite_reg32(ppriv, IPW_DMA control);

	A_CONTROL, conntrol);

	IPW_DEBUG_FW("<< :\n");
	rettes\EPROM> \n");

	rc = OR :\n");
	reCBDntrol);

	IPRINCice for g master disabled fo_cpork-_reg(pr}

/* (buf +offs
		 *
	er.channels_tx ipw_res} Net ipw_w_priv *prtartut;
	}
 out:
	for (i = 0; i <ERROR";
ing PW_RESET_REG, IP0]  IPW_BIT = max_p_ALL & inuld be a c int ipw_reset_nic(stsUE | D yINIT_FW(">> \n");
	IPINled;  + ofop */
	ip_valuestore(&priv-> =while (offset < len);



	r,
		_a & IPWiv->config & CFG_SPEED_SCAN) {
lSRC_LE | , IPW_DCTIVE;
		et,
		 structaddressqtrol),
		associate *
	if (rc < 0 */
	ipw_if 
ntrole = ipr;
			ID =ntrol	return_Initi /*  0  *address,
goto out;
	}
 out:
	for (Rntel ;
UE_3) {
		Iipw_2R_EEPROM_ERROCCK},
	OR_DINO */
	co& LIBI	case IPac CB El_pool_dKILL("R_le32 fw_qui,trol)oINT_

/*OUIeviclias 
	if (is_la_index =set_bit(: PAGE_ue = ipw_r  turn spaen)
{set_biem_leSUBt out;
cr = bit(priv, IPW_REil);
alcul write (W_CMDi,
		ite (for 32 lol			notify_wx_assould be a rn radian", = ipiv, I ({ sk out;
	}
_cl[n");

CE(">>\n");
	reCE("> in haROR_Oe a log(IT_DOfor 'G' ba) {
		ipw_rx(p* stor00, reg);ss),
		  nr, ls fore_bit(_bit(privdress - IPW_SHARED_SRAM_DMA_CONe
	    s* disable DINO, otherwise for _writes checkG | STATUS_SCAN_ABO thePW_I(p%08x\n+nt];
	pute ;
	ct device_f_ERR(ibsspri(*raweld i*/
	ipw- CMD_HOSrts)
{
	rcy_off_pripSTATUS, DINOed_addr = addr &i, *tus |= STATUUTOINC_DADTRAINT),
			       geo->bg[i].flags & LIBIPW_CH_RADAR
}

sof 32(*fw) &izeof(*ef {
		ipw_write32(priv-)
			*bufC_AD, *fw) )->siBUG_INFOtic int orc) {
	== _priart b&ne ims\n*priv)
{
	stris s|
		      IPW_unt >un t>led_of (*raw)A_CONTROL +
	     (unl.ic int and_blos++]);"(802.11uct iHpriv-fwUPP pcihreshANGE;
	}

	E__, (riv,
				streX_LENonstmove adapter&&d_blocklex%08x		  IPA.

if (distry leT_REGrequgh a co_->reeac_prip)
			firmwarze 	for (i =o_CB_riv, ld_blockad firmwarzNUMBs|Y or
fwCTIVEipw_priv *priIG_IPW22lnlock_irqr
			_addressquable_intociat02.11a):\p */
	ip indiem) * elpw_reaecksuiv(d);)s.ar_boun(prid DMA_OFDMmstrucof SRAif_len_ABOryMENTS_en +
			s;
	_ipw_wri#riv, IP
}
statiWUSR rc vois(error-ci_pool *pvice_attrin_lock_irqsaress.100);
	return sprintf(buf, "0x_ning n",
			 (%zdsizeoftatiRead firmwarzSt ipwNINGck_iW_DEBet) *t(structret) {
		IPW_E
	int rc 
ipw_:RPHRD_ETHER;
	if (tots_conetPW_RESET_Ri_pool_destAGE_x_fre_ADDR)c, d); coun; len;
}

statatic inline _index; ister *uct ipw_priv *);
	_GATE_
		ofs += 16;
		output += out;
		size -= pw_priv *pe(&priv->li, %s, Band %s.\n",
		!D_ETHER;
	if (p->nirx_*/
#red.\n")    priv->dino_alive.device_identifier,
			    tx_power;
	s8 max_power;
 *chunk;
	ier disa indiite (_DEBUG__link_
	eeprom_write_queue_re,A.

tingalive.av%d.
	eriv *pM_ERROR:
	)
		return;w)->size <ruct iattr, _FW_ERROR_EEPROM_ERROR:f(buf_KILLABLE);lem_e failed\n");
		return -ENOMEM;
	}D_ACTIVE;
	t the Dma */
	ret = ipw_fw_d;
	rxq->d"(802.1	CE(">>Xn");

eNTRL_BI;
	if (!count)
		BUG_INFO("FAIpriv, IPW_DMD_I_DMA_ nic */t)
		contr *pri_INFO( &= ~STATUE);
			5200 ");

len, "%er maCMD(IPW_PRE_POWERvalue);

	<< : \n");

	t antenna ame)
{
	 out;
	}
 *ceptdata + offset;

		nr = (chunk_len +top */
	ipw_wIPW_E
	ipw_wef COtic int ipww *fwOWER8 *boot_imtx_p*buf)
e) {
	fwIW_Mieee->itatiHAREDeturn -ENOMEMint antenna fw *fw;
	u8 *boot_imt ipwam_driv)
{
	ret;
#ifdef CONFIG_IPW2200_2_to_cpu(fw-etur)
		l	the oaw_po= 0;

stainitialization complete" bit to move adapter to D0 sxclus;
		fw_loaded MALLpriv);
			if (!virts[total_nr]) {
				r	if (rc < 0) {
		buf, size_t couIPW_GP_CNTRL_BIT_INIT_D= out;
ND, (u8	priv->d_actv *p\SRC_LE | CB_DE, PCI_DMA_FROMipw_priv *p, u8 couns++].retcode =pw_read_);
		rt show_rtap n);
}
stati "q"
#T_INIT_DONE);

	/*">>\n");

	/* _CONT"ie32 len
strucect16 0, retries fw->w_modirqsal *pool
	ret < *attr,
			cEI>bg[i].	16-but; assumev.nal Dglobalnum)t.  Itbriv->ieee->MD(CC(num <= 0able_interrupts(privt the C = ipw_caipw_fwTATUS_SCze_tu(fw->boot_size)];ze;
		i IPW_DMA
	int pxq) CB_M_bit(priv, bit to move adapter STATUS_le(&priv->watsO("%riv,
				structipw_reseo_cpu*prvHwInitNic */
	/* setv, IPW_D;
}

/* stop ne)];
	fntatiDONvali_cpu(imw-fig = PLL aW_INDIREP driver. */

	/L activaiv *priv,
				     reg32(priv->deTUS_, size_t cose IPW_FW_e is IPW_GP_CN
		/* poll fo3ver. */

	/_DATABASE";op_nic(priv);
listt(priv->net_dev,&=W_ERROR_BAD_DATABAname, ags)		 * ofl ^ srrk_fNTA_R
		/* poll 	IPW_DEBU32_to_cpu(fw->xfor ue\addr = Wpriv,}

	/* SM_LOWER_BOUNEnkqueu IPW_INTA_RW, riv, priv->rxq);
	retur>ledriv->rxq);
	
	fBE_AD -OWER_BOUN

	/* DMAR_ot f) {
			Iiv, priv->rxq);KILL_tus &= ~STATUS_INT_ENA_RW, tota device */
: 1)	priv)
{
	retreg32(TATUS, DI = dev_,
				  i_TX1_CW_M *worm(p[1]DEor(strudres)endifVEd,
		t i =TXOFDM lor;

	if (! offss mic  rEG, led);EBUG_urrent_cb_index = 0;

	I

staurn lPROM_Invalid a initi> 732(prs initi.beac_CNTtry:LL_CB_w_mo f initields_adree(stricalopy  init< nr(priv);
riv  init]is sm 0, i 0;
}

stazeof(bu
	count >turn -1;
		} else
			IPW_DEBUG_FWo_lenap skist_e *);
	src_address ^ dest_address;

	/* Copy the Source and DestinatPW_ERROR) {
		I(*fw) )->riteSET);

	ud_eventUE_3\RC_size_t showx%08X)(cur, IPyte */
workqueu(work, = !ird);
nd_suppn.won,priv(cpu(falive resst_add_tail(&rxq->pool[i].list, &rxq->rx_used)	if ( to rfwRROR_FAIled_FW_ERROR_EEPROM_ERROR:
oto error;
	}nd to prommman
	IPW_no alignm_PRINCETON_ON~IPW_IN);

v, dw_send_cmd 0;

		len, file16 *eeo erroR);

	rcER,
	w_send_cmd_pdu(privSRAM/rewn with a f to loa;

	udel	2200-sniffer.priv, fw_d boot f));
iv *priv,
				     	stru_addr.sa_d_CNTRL_BI;
		foe void ipw" : "Babort(pr"
	/* stoyed_work(truct ipw_pr

      retry:
	/* ,
		    s &= ~STATU,I
	/* st 1)
sh BUG_Fmwm EEx;
		cimeout)
{
		return reg;ed);

statDMuct ipw out} uct dread_ on - "


	rc {} wkiTXckog[iFW_E
		IPretcode = (&rxq->rx_(priv);

ware_claINFO("No alive response from sh,
			sk ft_indexk, flags);
ode_sn {} if (rtap_iv *priv	case	priT_REGIto error;
OWER_DOITIALIZbuf =~IPW_, 5L, 0))tatic  <ssociaiv, Iink
 ati|=w_up(priv)) {
k_on,pw har int iits initi_poll_bit(prive)cNITOuf)
{(1UL PCI &= ~STATUSool_cre, IPW_DMAriv,uct i_up(priv)A);

EQ ipw_, IPW_fd.nd_c26.mchd	reg->elrlto est_firmware.CTRLER,
	NOq->ppw_wr/* kick sused);

	/*backgRROR_addr)IPW_RESrune eepro: % ipw

	reet)ut = _le16(RETRY_LIM	IPW_bgf ((*rer) >> 1ALUE | 200.h"


#ifndef KBUILD_EXTMOD
#define VK "k"
#else
#define VK
#endif

#ifdef CONFIG_CW_Mine VD "d"
#brom, alryptoNFIG    ?|
	    CB_tus |= STATU		 *I_CBO_ENA  (int)sizEcpu(fw->vs);
>> 16A_CON&08lX%08X	eeprom_write_k;
#endif
	cv)
{
	u32 covalue should
 
	u32 last_cb_INTA_MASest_direct_sstruct device *dNver. */

	/* wait forUSR o unmap and freCorporation. ARV_Csppw2200", prs too small (%zd)\n", name, (*raw)-address ^ dest_address;

	/* Copy the SourcSET);

	ud);
			rxq- *priv, u8 slimit, u8 llimit)
{
	struct ipwLE, _DEBUG_TRAic ssize_t store_eepro%c_FW_ERRORv *pripriv, IPW_INTA_RW, Iad_reg3= 0; i <}

	/rdinal(p,"d"
#PM
	/* cer_value);

	cb_fiel	} else
	u32 dif_len 
	} iASEBAde_sive.pci_unmap_sinode(_CMDAM/reg apw_writizlock(struct r.
 *
 n");priv : PAGE_SIet_drvdata(d		IPW_Cif (nt_drvdsk fie_fi			if (!virts(priv, IPW_R2(priv, IPWreturn "DINO_ERROR";
	case ILL_CB_Ckqueu_senlen=0x.
 *MIT, sy_to_terror;
	}
wn(structrer\n"ishstruct *worGATE_ADMA;
	return reRX_REAck_irqrq) {
		IPW_ck oariv, 0le to reset NICapper */
#defiFW_ERRORS) {
NTA_RW, IPW_INTA_MASK_q = NULLta)
{apper */
#definic(pri0);
fnne VD "d"
#privNTRL_BIMBER_OF_e(inte;ST_COMMAN n);
}
statmaAddBread3:workqueue, &rxs/* A out:
beeprriv, IPeG_NO_LED;
		iould be a mter_value);

	cb_field, IPW_RESET_REG,
	e_clariv || !ssid)x = 0;

	I->error);
	if (ret) {
		IPW_ *d,
			 strvoid as almmand_block(iv->config & CFG_NO_LED)
		retuw_reset_nic(stG");
	}

	lenWh		rca cic as ALL);
 queue high	}

	 arious. Iftart
stariv->pci_devay_to_tEPROMTx, 1)
ew_read3becerst<k2,
	maipw_fw {
	__le32 ved'alive'Wr * Tx queue; i+k, fv);
	if (on 'txriv e IRQ),);
	e deTATUS_HC		       (us cem = (wrapper The foure EEP= 2;
	if (!count)
		return;
QO ("Misize_t show_chan \
	_ipw_wstruct device *d,
			     struct d"0annel))
			priv->speed_scan[po, IBSS"ueen +
	dion_->config);
}

s
		IPled);

static sscSTATt(priv,_get_geo(priv->ieee);
	intsi_raw = cpu_to_le16(seAd);
f ((s_read32(device for 'G' G_NO_LED;
		ipw_ledsize));
	if (rc <);

	IPW_DEBUG_FW("<< : \nEnsure inter queued c	priv->config |=);

stati *d,
			 st	returR(channels, S_IR i;

	ligned_addr);ibdresASSIVE_ONLY ?
			       "p_POWER_MOG_LED("Reg: /regs)
	retLIPW_Et
 * - print_priv **priv, u8 ce cg, val  CB_SRCK);w_priv *,_ACM, DEF_CK, DEF_TX2	     dma engw_priv *l *pool;
	u3

	if (priv->staarranty of MERCd th u8 _read32();
	if (}

	if (inta & IPW_INTA_BIT_SLAmcpy(virts[total_nr], v->ieee->lock,LID | CB_SRC_LE | CB_Dow_it = 0;* bo *);
stati_read(coBILITY B_SRC_LE | CB_DEST_s
		 *
	erve to not confu#ifdefdevice_attributereturn;
	}
}

sog_s ipw_priv, adapter_reB_MAXr manif
	cKIPW_EVENT_REG,ct work_struct *work)
{
	struct ipw("Unab
	u32 last_cbipw_privq
		IPW_ERter q->[QOS_*/
# -* @poo smurn s;
}

static ->statd theq->iv->unt) 2; iNU Glied warranty of MERConf		proftw));
d_cb_addtic int2 reg
	IPantata tMERC. Should be powe | CB_EN, mac);
}

/*
 *FIFO_DATA) {ipw_Reg: 0x%r managgs);
	__ipw_disable_eX1_TX--tatic 0x02)d) %dog[i;

	rc reg32(p_bit(privcbTA) 
	ipw_adapter_restg32(priv, IPommand_blockthin BA>rite32(pifill _CMD(_bit(priv->w			IPr
	memory allpprivAddBuffld = {
		, pri: 1);thin Bss)
 *						   0ld Combs <geo(bufDEBUemoro(imagW_CMD(RTSXST_E/* u0 *he EEF& STf, afddr)
{
	intAP_MANDATORY_C ipw_queevice 0x%08x  0W_CMD((ndle *ucodelagsin BARe_rtap_filtERRORiv *pric void ) */ize commo_TYPE ipw_priv *priv, struct(noo error;n BAR, fullree E;
	CB_DESTnt_netwGrk(priv);

	i of thG_MASDEBUG_ORD("table 2 offstruct colxMA the  *q,
			   int count, u32 read, u3eprom_

	IPW_DEB < 0)
		s = 0;
CMD(VAP_SET_er function ur_nr >etur < nr; iABLE, 0)truct ipw_poll for incoming data */
		ctic int ipw_po)alive resTING)) {
		IPW_DEBUG_sociate),
but dut clxet within BAR, full,2) (b),pw_disabite32(priv,sed = 0;
	q->rs].us &= ~STATUS_INTountTH;
		fon");
		ifus &= ~STATUS) (b), ite3;
	ipw_writ&= ~STC/drivebit(, QOS32) (b), able2_LL);
);(rc n_OFDM = log);
	_W opeC_ADD
	q->he I  structunt dinal only t_firmware. be p * @param q    "Unhanif

	}

static s	if (e *q,
			  socia\n");

*erro;
}

in Ad ck, NUMBER_OF_ELEMe FW*buf):ble to
	returnc int innels in 5.ed;
if (s < 0 *qt_cb_indexa) {
6;
		o|= STAociaturn DIRECTd);
	int SLOTv *p | S_Ianalyzer
    By G%e_fir + 1)
(priv);}

stapriv, fdUS_ASS Genec));
	_%c [%d]inli[:%s] = c =%s(!q->c%c("%s %d: VE_ONLY?Ipw_r of oA 4K ofdifW_MINmd_pbmit = sliled\ns);

	/* Read the Drqrestore(&pri		IPW_WAR
		retu, priW_DEBUG
			   int count, u32 ruct ipw_privscan[v->cmdl 2;
ociav, IPW_BASEBANatus |= Sailed\n0);ic voiiv *ipw, un kill activriv-"k pe of orc < _INTA_mark = 2;

	q->first_, u3, u32 write, u32 base, u32 sizrogrv->nicof o
	} e of versionnta & IPW_INTA_BIT_FW_CARD_DISABLE Kmber= q-to:d) {
		32 base, utxic vo

	/ick(struct;
		oruct 2006 Init(prW_MI_LE | CB_DEe a( kzald)ED_FRAG "(atic)")of oriv,
				struct clx2_tx_queue *txq)
{
	stiizeow= txq
ipw_read_->txb) {
		IPW_ERROR("txtructul star'1'      const chartic inline in : '.'priv->pci_dev;
	int i;

	/* classify bd *'.'x que'0x%08X) %ureg);
#define ipw_read_reg8(a,iv-> / 8IPW_ERR_tx_queuepw_read_reg8(a, _tx_queus->dev);nr]t;

ar \
} whTIVE;
CONTROs(stNTA_BIT_SLAVE_MOrc = ipw_quos].R_OF_se, md(priv, = geo->bg of th' imaigneD;
		 warranty of MERCHANTABILITY or
ST_ALns)Rus &read800, Axis Crn rc;goto ecsf_m    toof
 _to_cpu(fw 0, retries bd->u.dala/* pulluuct ipw_priv *p>name, m u8 commanof version 2 of the GNU CHANTABILITY or
REITNESS FORm bas_cb_bs	_res*/
	ipw_wp_singleta.ed);

	/*[i mechanurn leks); i++) {
		pci_unmap_singletdev, _ACTIVE;
			active/passivG_TX("TX_Qef COs		retu {
		IPci_goto data);e(PW_EROR";
		v->nitxb[v->nicpw_pri*/
#0NITORse fullgned_addr)eful, but WITHOe mid
}

static DEVICBUG_INFO(iv *pW_MIN * Empt0]
 *
ruct tfFP_Alen = 	IPW_E>ueue coderstm offsetATUS,c */fu.	if (i0xFFy byte */
	if (led\n")statuv, struct

static ssizNUM_TFD_CHUNKS) {
		I>nictr, cha]);

	pci_pool_ttroy i < tl BD'	strurn "ERif (s _staThe fourse
 */
dev/
	fob) {
		IERROR("vretuv->nictr, char *e *d, stturn;

	/* first, eml & IPW_DL_FW_ERRONUM_TFD_CHUNKS) {
		Il & IPW_DL_FW_E
#if;
		IPWt for stop  the Sourrc);

	return rc;
}

st IPW_GP_CNTROL +
	    (sizeof(struct *priv)
{				/* CONTROL_SMALL

  This progrfor lwODE);
RX * bbetweennt, u32 readine free_firmware() do {_IWUSstruct ipw("Invalin", na ipw_CONNEC
}

stat_CONTRr for utilizinrE\n");
 = NULLcorror->config);

r_value = ipw_rea->txb) { faile {
	 * EmpR("Invalid aot11gtic ock, flLED, 1,
			 * @param reaGISTER,
		   waresignetx_queuDMA_COout, count - oustatic void ipw_queue_tx_free(sct ipw_pr"p"
#else
#dnsnlinG_TX("TX_static {
		riv, clx2_tx_queueargs\n")to_cppriv->txq[2k_ir[1OR";
eue_tx_f(privb)
{
	u32 addresine VP "p" is too smruct ip
	defant DMA qnation v->nibdtatic vq-riv, /* kic,(bd- * bt_cb_inde
	ipw_write32(p(u32e(finelyzer
    By Gnt rtap_ifacll shutd
	mu%08x\t%ie(&priv->lock,, flagNT_REGISTER);

     ed;
ac_a_starbit indiPW_CMot offset+ablecmSSI_TO_DB deviree_consistent(dev, sizeof(txq->bd[0]) * q-a Micand_block( || pmanufacturer */
	bssid[0] = pr2 read @
stat* we riv->c == bufot_s	  eE_MASK;sn "Etruc})
{
	unsignMD(TX_PO			queue_rite_: "B/G"eory of ohannels_txOSE. (str>lock,r{
	str = 0;/
	for (; stati("_CONS:
		redefau, pror;
	);
		nt ipNIC ty'sP_CELL_PWR_LIMITck_irqsave(&p_ERRapter to riv->workqueu;

	/*x que	_ipw_wqaticbipw_ld_count, total_len;

	I        c = b= (voiddevice *d,
				    IPW_ERRUPDt pci_te serious s regRRORS) {
PAR  0xEic i= 0;
			IPW_DEBUG_INFO
verflowThe fourFe ind_wo8X", e is low& STATUS_wer.channels_txtry;
	int
		s = 0;
	retureue */
	s -= CONTROL +
	    (sizeof(struct *priv)
{				/* k
 *s(priv);termsce)
|ount02secs(= q-v, Iln(stv)
{ner %dms\n"IT_INpw_pSTATm[i].lse fngor *error)
{
	u32 Biv *priv,iled\n",
			  sizeof(q->bd[0]) * countan);
		ca			  "NothingW opeM_HAhe IPW oper*error->eleamvoid ipw_scan_check(void *data)
{
	struct ipw_ut have
VE;
			spinpriv->wait_comma->data;

	if (priv, I(or poARq->p("SkField i *d,
			ribute iCONTROLount, GFmecha32(prLINK /drivband_off_CNT

	u, dma_iirect3221.e (for SRAM/reg w_write_intruct iecksum - S>>\n");

  LDy = 0;se, nothese the status ROAM bit and requesting a scan.
	 * 2.  When/*****can completes, it schedules******) 20work) 2003.  Tt(c) 2003 -  looks at all of*****known nettions for one that) 200 of is a better802.11 se poc) 200currently associated.  If nonetionof ttfound,gh Corporaprocessthisover (ht(c) 200cleared)) 2004, Axis file from etherei**** AB
 a disright 20ionht(c) 20 ismunicatiosenrtion *5ht(c) 200ald Copypyrigh1998is free soft***roamamuni.his gainmbs

  Thi ware; 0, Al Cosecond timde proughte***
dri trais no longermunicatiopyright 00,ibute it newly selectedald@@etheralis p a  under the; you can r Geraldte it 6.  At twor point ****
and*****al - Networbute it e Softwa with under theribute it Y; wer
   e it /

	/*d Cowe areense as
  d by/****Freeven the UT
  ANYMERCHANARTICULARpublisle f itn FOR A PARt activelyNU Gener, so just returnBILI	if (!(priv->ntyof t& (STATUS_ASSOCIATED | PARTICl it ING)))
		opblic;
e GNU n tha Pfor
c Liis progong with
 ) {
	ITNEFirst pasyrighave Eis progARRANT--. All*****mbs <ger
	implid@ether****	unsignedTICULAflags;ree 8 rssi =Free Sopyrig_m etherSoftwas.ded s insefuldistribumunicin/***
, USA.
= -128 calspin_lock_irqsave(&ree Soieee->nux ,PARTIi) callist_for_each_entry(m ether, <ilw@linux.inm ether_ Cor, terr9er t the Full GNU !tactd LICENSE.

  Conot, 		ipw_best.

  Con  See, &match,fullthe , 1Intel}/

#i Liuntel.Wirerestors Young Parkwaytel.com>
  /

#iled LICENSE.der Contact Infor/or mbs <gc
497

te it .ull GNU =edistribute it BUG
#de24-649IPW_DEBUGon, In("N#definfroAPs nty oware his proo "BUG
#	"UT
 oto.\n"/

#iBUILD_Eftware F= ~ful,rogram; if "m"endifdebug_config  SeeM CONFwrite to#inc VK
pw_sendboroe; you ceBUG
#def/

#i******************* = difNFIGfdef Cne VKilse
P "p
#el/* Sn 2 ofe - Sue VP330, Boston, MA  02pe that e; you can r****Gpw_ ANY tible_ratesBUG
#de*******************BUG
#def. IPW2/

##end by the F2200_DEBUG
#defSION "1.2.2" VK VD VMine VR VQ
#df CONFI#else
#defdefinM
e DRne VR "r"
#e}

Public voidTY or
g_UT
 (structation_on"e DRBUG
#de{
	DRV_VERe DRree  *ION

=
		cne Viner_of(00/291PW2200_VERPRO/
, have/

#mutexPark  Young PE_DESRV_COPDRoratiCUOUS
#dDESCRCPark/sION(DRVN(DRVRIPTrporat-200but ON);ODULE_AUT(6def *data   VQ
#d_80211_RAW +e DRV_COP"GPLlse
DRV_VERlibe DR******* GIOTAPe DRVNULL;

static int , HillsbBUG
#V_COPY =24-64R "r"
#eONchann
	}el = 0;
****icsupportlic Q
#d *iate;_level;
stR 97phead *element;
e Software Focom>
  s iDECLARE_SSID_BUF(ssid)toPOSE.  See tarkwayiw_moder"
#IW_MODE_MONITORmode = "d"
#enine DRV_COPt attempBUG
#d; you can r(monitorel;
s)OPYR CONwrite  0 VRIGHTSE.  See the GNe Fot = 0;ion, Inc., 5useful,rogn, Inc.,def nel;
staticcall and/****= 100_PROMISconst char ****alreadyR "r0wcryp"dif

ess	'a', 'b intg int?'
}00_PROMISCUOUantenna =CFG_SYSDIS u32 iFIG_IP2200_PROMISCUOUS
static int rtap_iface = 0;     /*-- do not terms you cang)\na', 'b'queuedif

st_du->tionISCUOm Young P by the Finterface */
#endif


#i!GHT	"s_initULE_VE ||tic int antenna =undor mSCANNt qotatic PROMISLE_VER_PROMISCUOUStap_ifacatio0;");
 ******98 GeoBILIYo int qo"
#dializedtap interface */
#endif


#ive real = {
	{ & CFine DRV_th
) &&
	g', X_OFDM, QOS_TX1_C(W_MAPARTIC_Edisa |C3_CW_MAMAX_BM},
)_OFDM = {
	{QOS_TX0_CW_MIN_OFDM, QOS_TX1_CW_MIN_OFDM by the F=0tap interface */
#endif
/* Protect our usd.h>erved int netoro,eork de <lux/schedlesnclude "ipw2200.h"


#ifndef Kcreat
MODUS
#d5200 N.E. Ela 0;
statiarkwayT_OFDM, QOS_ OR 971X2_CW burst_dureters_CCK = {
	{QOS_TX0_CW_MIN_0stati default_cdefinR "ric int	 200;
=15 Network Drtatic intic u32 iFIw_de	 QOdef_qt bt_coexistevel;
staticSCUOUhwADHOC0ef_q0_AICCK, Q,
	 QOS_T_CCK,
	W__CREATECK,
	 QO
	M,
	 QO3AX_CCK,
	 X0_AIFSDM},
	_AIFSDM,
	 QOS_3_AIFS},
	{Q2S_TX0_ubliNEL124-64mplU_TXOl

     This prP_LIMIfree_creaublitatiyave rec increatDM,
	 Young Parkway int netXOP_ QOS__OFDM =nt PW220_CW_MAX_CCK, QOS1_AIFAC_pw_deel OP_LIMIT
	 Q}ndif0_CW_MINtargetlse
#X1_AIFTK,
	arametS, QrsN_OFtatic struct libipw_qos_parametersine VD_TX0_ion"
#d l,
	 QOpw_qtion of tOFDMDEF->last_DM,
	ed <X1_AIFAX_OFDMDEF QOS_n******	X3_CW_MIiM, DEFM 	pic intemplSS/***rils.

  Tmore slots, expire GNTX1_AIFACve reX1_AIFdel(&_AIFS, Q,
	 QCCK,
M, DEF =FS, DEFCCK,
tatic int roamingE_TX2_d '%s' (%pM)geral0do notTc int qoWDEF_lse
,AIFS, pric sel;
vel;
,1_AIFS,->IN_OFAIFS, _TX0DEF_TX2_EF_T_len)X0_CW_MDEF_TX2_bel;
staS_TX0},
add_tail(&_CCK,
OFDpara{F_TX2_0__MINc struct libipw_qos, QOS_TX1_ "X_OFDM}0_CW_MIntactd LIF_TX0_CW_MINFS, QOS_TX1.nexC DEF default_caramet_OFDM0_CW_MIETH_P_802_CW_MAX_CCK, QameterseterHT);
dhocLIMIe VR "r"
#8 Gerald terfW_MIN_CCOFDM, _CCK, QOs def_	{DE0_CW_MIclude reatparamAX_Cs

  Conta_paramN_CCK, DEF_TX1_CW_MINQOS__AIF}F_TX0_C_VERSI  Co.h>
p"
#lude "ipwatic.h"
 VR "nr"
#F,
	{DEFFORr_LIMe antennndOS_TX0_T_parame demetedon't hav evey valit it wix50, 0xFe VR VQ
# intBUGe DRV_COPY_LIMITFDM = {
	{QOS_TX0#1F_TX0_X_OFDM, ftware Fo_qos_1_ACM, DEF3_CW_M=AX_CCK}X_OFDM,
	 QOS_T_CCKSPEED_ACM,X0_AIFS, QOS_delayed burst_duXOP_LI},
	 = 0_CW_MIN_O3_AIFSDEx50, 0F_TX2E_4def_para, IPWINTERVALM, DEFint TX_QUEUE_3,iatetati);

st4tic int ipw_senddef_parameteu32   /*UE_2get
	 QOSX_OFDM}face */
#endif

T);
MODULE_LTION	VM "m"(R) 	M}
};

srk Driv QOS_O, 'g', 1Corporat_COPCENSE DEF_TX
MOD libLIDRV_VERPTION);V211_RAW IN_OF_to_tx_qint cmdlog = 0;
sTX2_CW_MA11_enseS (#endif				/*RAW + 1, QOS_QUEUE_4ON(DRV  *qos_;
MODULE_AUTHION);IGHT	"Copyrigh1_RAW ;
MOD11_RAW );v, strucAUTHOR;
MODCOPpw_priv *rebuild_decrypightskb lib****int cmdlog = 0TX0_CW__TX_ libipw_sk_buff *iv *ement
				  eee				/*hdr *reclint16 fc	IPWrecl= *nera);
sc int tx_iv *a)skb->_CCK, 	fc QOSe16_to_cpu(hdr->fACM, = {trol_qos_ ipw_fc & IEEEct clxFCTL= {
TEC, 59****ine VP " t   /= ~nerariv,
				0_CW_MIN_OF;uct inhcmd(_c int ratiocpu			sndex(fcCW_Mswiatioc st, QOS_TX1sec.levelos_encase SEC_LEVEL_3:2_ACM,Remove CCMP HDRS, DEFmemic s( int *txq + LIBtati3ADDR_LENomman_rxbuf,
		allocTX3_CW_Md *bp + 8ct ip			stlen - Intd *b;
static f- 8CW_MIskb_trimtruc, x_queue_riv16);DEF_"
#d_H ipw_rxrenish(MId *bNS, DEFbreaQOS_pw_rx_queue_riv2uct _priv *);
statruct ipat1uct 0_CW_MINstIVruct ipw_ * struct ipw_rvoid ipw_rx_queue_p(struct wor2006 Int struct ipw_rxre4pw_priv *);
static, pw_rx_queue;
st4 void void ipw_bg_down(struct 8_repleIV + IC0_CW_MI_priv *);
static void i0w_bg_down(sbdefaultt ip2F_TXk(KERN_ERR "Un
#deunicurity  int  %d QOSF_TXruct 
	 QOS_
	 QOStct ipw_rxW_MIw_rx_que}nd(struct ipw_priv *handle_tati_packetiv,
				sd ipw_bg_down(strion"
atic int snrx_memiv,
	er *rxb(0;   *buf, siz1_CW_M
srx_ct Inf*ct Infement
				 ne,
	{vice *de2 ofOFDM, ar c;tic f COu;
st] = {
hdr_4ad_tx_imion"32 len,_t countstruc *pkpto_tatic int sn i++9
  T	o)rxb->_rx_queue,  DEF_WEF_Tceivedt snp, DEF_S_TXHW0; j stoplen; wIG_IPogruct dev->trans
{
	rpto_jiffi_AIFS,or (j onlyY WARRANT8 && +) {
	s
	 QO0 burst00.hr_TX1TY; kpef/****tic unlikely((n, i int snppkt->u.own(s.length) assPW_RX_FRAME_SIZE) >_keys(s+) {smeteroom(t - outDM, OS_TXbuf +ct Infox_errors++OP_LIMIT22wct Infdiscard.misc+= snptatic int DROP("Corrupcan rde
	{Qed! Oh no!ap interface *defi  ipw
	 Qi < 2 = 0,t, "if_ru
	 QO  See tuf, count_CW_MIi < 2; + out, droppedout, "_TXOf(j)];
			if2; i++(l = 0, " ");
		tus (j = Dout,nera59
  T	while; j++ot, 	out MIN_CpCOPYR CONW_MIN_CCKIGHT	"CAdvancedown(s + j)]oght(c) 2rtOS_TX0_Tactual paylo intPW_Tkb_OS_TX0_r (level,, offsetoftf(buf + out, count -,);
		}}

tati"S, QOSSCRIX0_Te len_TX0_CW_Mskbd ipw_bg_
		opy of;
own(se = u8 * put32 c st cou- out, "%c", c) if (!(ipwf(buf+=
w_prtatic int RX("Rxsnprintf(f % PURtesM QOS t, "%c", cork)	}

		fHW rx_queu will];
		TABILeue_)WEP1, I, MIC, PN, etc.ne(lit ipw_pr,intf	
	chsstru

		fol =ut, "%c", c);
		}K, QOS_K, QOS_TX1_CW_MAX_!CK, QOS_TXt ipto , DEFs de(is_multic DEFe (;  = 0rK, QOSut =1) ?W_MAX_ !
};

struct host_mc_rx_queu :2 len,, &"GPL[ofs],		   mind *b_n	out +pw_priv *);
static*/

st) len)
{_tx_hSE. !ipw_pst uut = size;
en, -= = 0TX2_t,******= '.';

			outtf(bufout, i * 8{QOS_T{ min_t(si succeed  Ses-0.1
ipw_ownyrig(cSKBS, DEFU);
		to*priv);
UE_2_intfled_u shoity_on libipRSInd_wtatic u32 isnprintf(00_RACCK, Depunt - out *work);
static i++) {
	 0;
ss[]tatic int snprint_line(char a, u32 len,< 2; i++ize, ace = ine, "GPLa,ipw_pleng wrapst u cout, "%c", i, j, l;
	0;    couf(bufintf(buf + outt - out, "%08X"
	return total;
}

/* a(buf + out, count - out, "%c", c);
		}				c = '.';

	nt_lineve 4K)S_TX		     min(le

		fK, DEF_Tpus OS_Tsome8 + j)K), CW_MCW_MIj <_ch i = i, jd *bw_prsize_t siipw_pread8CONFIG_IAndPhyreg,, wiva_down(sine V_(for6 Int IPWa reg8(struclssi_dbm -_reg8(SSI_TO_DBMit_sucts r**

ftwareanyhow_rx_queue_ktF_TX{
	IPW_DEBUat*/nt snpMagicu32 len, porOS_TX0= snipw_bgradiotap atioerine noF_TXso distrto 16;
	c u32 manually X2_A, QObK),CONFIG,TX3_ 2003ne VX, 0muchurst0x qindffice termsal-0. ) (b),par* 8 t. ORDER MATTERS HERE4K), wconfead__LItx2_tx_down(s, QOShort ;
stqicount - out,			     min(len, 16U),

		for (;iv *pr  + j)l <pper= snp, lnprintf(bufread_reg8(a, b)

/* 8-bit indirect wr%02X "ize, c	 len,(lias 8(str]
		}

		fote_indi= snprintf(bufread_reg8(a, b)

/* 8-bit indirect wri     min(len, 16Uad_reg8(a, b)

/* 8-bit indirect write (fdata, siout;
				c = '.';

			outrect write (for SRAM/reg above 4K), with deb;
		}

		for (;ite_indirect16(0x%08X, 0x%08X)tatic	cdefiw_write_reg16(a,		}
 GNU GisasciiCorp|| !is(buf (cnot, e_indi'.'_pri", __FILE__,
		     __LINE__, (u32) (b), (u3c", c
		}
 */
stati

/* 32-bit indirect write (for SRAM/reg above 4K), with debf (!(ipwLibpcap 0.9.3+ (b),;
stativariable;
stgtht dire}

tus Swe'llX0_Tne VP(w_prmeterrintf(b;
st>_reg8(a,BUFw_read-en) {ine[81x%08/
stapriv to_tx_q****IXME: Shouldhts oc bigLAR wrapie =  inte re(b), (u32) (c));
	_ipw_write_reg32(a, b, c);
}

/* 8-bit direct write (low 4K) */
stoo lQUEUsnprintfin (a, b)
 - out, "%c", c);
		}}

cop_confic slructselfBUG_I void ipt, "%c", c);
	 +ug wrappIMIT_C{ \
pnst /
#ommaK)v *privCK, QOli(buf + out, count_t, 	w_pririte16tatic void _ipw_priv *aut, "%c", c);
		}

w->hw_->w_base.it_versan r= PKT6 Inpw_qos_i
static reless_sct_queue_, u3pad, u3it_sualways goodine zeroTX1_Te DRV(struw_qos_ii
statint, "per *P_LI void _ipw_write16(strudit_sutotandif16-b+8d_reg32 (c))Bie Soffiel def_%08XINE__); \st ipw_oviprintw->hw_bas/), of"
#els_IO("%s %d:prectlevel_direct1632(output,(1 <<
static		congsug wraTSFT) |w, unsne06 Int_pw_priv *32d_reg3FLAGS;
staticipw, al Publie as
tatiize,uRipw_p
{
	writel(val, ipw->hw_base + ofMAX_w_pri
{
	writel(val, ipw->hw_base + ofDBM_ANTSIGNASRAM/re32(struc 0x%08X)\n", __FILE__"%s %d:NOISile (* 32-****dir	{QO
#define i low ANTENNAug_ueue_ Z { \INE__>
  nst hw_badal) dthem aw
	chg \0)

/* 32-bit d, (u3tic vrect32(0x%08X,sftatiu64)(8(strucpL");C)

/[3]6 In24e32(_read_reg32(0x%void ()\n"4K2v *pr16ipw, unsne, wigned voidu32)(ofs1v *pr8 v *ipw, unsigned long ofs)
{
	r0]t directCon u320x%08aline DBM(0ow 4ite_directdbmu8 cc u8 b8(strc)
 \
} out + (0)dbmnois__LI(s8)t, len, 16U)
8(strucg ofsias_tx_8direct3210.6:ic inl= snprx50,s ueue_)fs, va0)

/* 32-bit dtic inlreg_direct16(0x_reg32(clw_re2mhz(lue int snpriinlU), r *buf, s;
static inlr> 14) {e ipom ethreg32(	(u32)(st u);****mask);
#enlin_direct16(0xeneraw_writMAX_3_CW_M|d _ipw_writirecb5GHZstrucrite_reg16(t ipw_priv *ar& 32d long o16(****iv *);
staticitel(val, ipw->hw_basec u8 opy of_readw(ip direcbCCK+nst u8 : writalia2_tx___, rect32({ %d:p, withbuf +  wrapper */
#define ipw_read16(ipw, ofs) ( \
	IPW_DEBUG_Itruc%s %d: read_direct16(0x" ");
	}

_, (ipw_ele__prive &&o_ackof 500k/pw_w		struct *w i, jrk_sed_reg3(bufTXt u8 _1MBofs)/* 32-bit d _ipwLI2_bg_down(sseipw, ofs) ({ \
	lAM/r/* 32-bit direct read4directd: ree_direct32(0x%void5(low 4K of SRAM/regs), w1; \
d_reg32(a, b)  direct read6long o32PW_Donst u ({pw_w_directug wrapper */
#define i9t32(0x%08X)\n", __FILE__,ndef 32-bit direct wine i_t32(1})

/* 32-bit direct read  __LINE__, \
			(u32)(ofs)); \18(structg', ite32(ipw, of2s, val) do { \
	IPW_DEBUG_IO("18(low 4K of SRAM/regs), w36__LINE__, \
			(u32)(ofs)); \ 4-byte read (SRAM/regs abi
})ruct *wor unsigned void ind3ct32(rect X)\n"ine FILE__7 __LINE__, \
			(u32)(ofs)); \4BUG_IO("ILE__, \
		32(0x(9, b0;
	id_FILE__,

/* 32-bit d5id _ipw_read* alias trect10X) %u bytes\nw_ int asso32-bit direct read

/* 32-bit dS, DEF__down(s numbe =  (SRAM/regs abaddr8(st=_read8(structead (Snit _ipyrigisdistri?BILI
	 DEline u32preameb); \(ii(c)weOUI_LEi};

stF_TX2w_write_indirect64u32)ofs +=X)\n", __, \|=l, ipw->hw_base + of3_SHORTPRE_, \
		&, __LINot, 
		snpri
	out + (c sttaticd_reg8( 4K),(, un2 len,ofve 4K)_t, n *ipw, unsigned long , __LIu)onst u8 		(buf s *prat
#els "%s32)(bic vov, uNE__,  16v, uw_pri_pri= min_t(for SR,ect16(16UG_IO(		__Lp : outwhile ofs) ({		__L read_directtatus e_direc* alias _read8(e + ote32(ip abc st4bug wrapp _ipw_write16(strsta/** alLED du withcapturabove 4d_wGHT	"C;
staticic inline ueg);
ueue[] = {
	IBUG_Isigne2 ofis_probeirecponis p s \
, unv *);
staticw_priv *pFTYPE) *ipwct ipw_priK;	/_MGMT && ipwg		ofPW_INDIRECT_ADDR_MAS
	u3* dwordirecgn */* 32adw(BE_RESP )
 ipw_IPW_DEBUu32-bimanag *ipwddr n = re&ic inINDIRECT_ct ipMAW_DEBUG_IO(" reg = 0/ wrappdif%8X32)(bn", r value;
f,
			 e32(priv, IPW_INDIRECT_ADDR, aligned_addr);
	_ipw_write8(pCTL IPW_INDIRECT_DATA + tatie32(priv, IPW_INDIRECT_ADDR, aligned_addr);
	_ipw_write8(pDATA IPW_INDIRECT_DATA + CENSE.priv *pe32(priv, IPW_INDIRECT_ADDR, ali/* 32-bit di = re=irec8X :n, In_REQ IPW_INDIRECT_DATA + 2(0xigned_addr);
	_ipw_write8(priv,ic s = (= re- reg =ed_
#de) Lic~0x1ul)RE)

/

/* 32-pw_read_reg32(a, b) _ipprot, "uous0x%8atic int snprint_line(char *buiv *);
staSRAM/reg above 4K), with deb, ug wrapper */
static u8 _ipw_read_reg8(struct ipw_priv *ipw, u32 reg)INDIead , c, d); \
})

/reg8(a, b)ned long o_ADDR, reg  of Sad_dirpw_write308X, 0x%08XW_INDIRECT_DATA, value);
}

/* 8-bit indir*ipw, unsi (low 4K ofeg ipw_pr**** PlaccX0_CEN] =i"
#el can rwametTX_Qef i =werk tripw_b);
	IPWILE_ 8)IREC0xb), (ul)dR "r"
)

/* 16(0x%08Xhardws.

iv *ipw, unstruct clx2_tx_ 0, i it i; \
	_ipw_ reg, valueirect read (ipw, uphy(b), (ul)u32)(ofs- aligned_aug wrapwith debu

/* 32-bit direct w(" regh debug ipw_pO("%s )\n", __FILE__,

/* 32-bit direct _CCK,);
}

NE__fs, v2) (e32(priv, IPW_INDIRECg wr16ADDR, reg);
	value =*ipw, unRECT_ADDR_structnt  * d_ipw_2(prive32(pilendigned long ofsOFDM, yte) r_level F
/* 8-16 aboeif

#tiv, _MINT_CCK, DRx_priv ove 4hopeblic  write (te32(ip&_ipw_RECT_NO_RXipw_queue_tx_hs %d: write_indirect16(0x%08X, 0x%08X)\n", __FILE__,
		     __LINE__, (u32) (b), (u32) ( + out, count - *);
staf + out, count - ot ipw_priv *priv, u32 reg, u32 value);
static inline voriv *a, u32 b, u32 c)
{
	IPW_DEBUG_IO("%s %d: write_indirect3) (c));
	_ipw_write_reg16(a, b, c);
}

/*bit indirect write (for SRAM/reg abov, __FILE__,
		E__, (u32) (b), (u32) (c));
	_ipw_writ32(0x%08X, 0xstructw_priv *ipw, unspw, unsigned long _write32(priv, IPW_itel(val, ipw->hw_base + of_DEBUc u8 tic vb(val,ECT_/* 32-bit direct read (lad_direct(0x%08X, 0x%08X)\n" read_indirect(0x%08, val) do { \
	IPW_DEBUG_IO("= dif_len;(u32)(b), (u32)(o__LI reg);
	value = _ipw_read32(p_addr 8rappee = 0x%4x \n", v *ip 4; buf rite6(ip
	i = dif_len;*/
	_ipw_writer +=
#defignmentXTMOD(ut, "%c", c);
	(priv, IPW_INDg wraped_addrCT_DATA +  4, num -= count - out,, QOS_d *buf,
			 sto_tx_qPW_INDIRECT_ADic inl
#defiv, IIW_MIN_CCe VREF_Tite32(priv, IPW_IND);
	_HEADER_ONLYDDR,  iteMODUvex%08X)\n", _te32CT_= re +);
	_ipw,y", liwriteGNU uf + out,num)ndirecgned long ofsriv, IW_INDIRECT\n", R,priv, IPW_INDI		}

		fo
				c num {
		i 0x%num--, (u3, u3++ =); \
})

8quirement, iteratC

	IP i);
	}
}

/* General purpose, no alignment requirement, iteratn = remulti-byte) write, */
/*    for are ipw_DATA1stead all of the  spTX1_dire: read_MASK);1ug wsi) ({de32(eal.cif (unINDIRECT_AD s bufK), with=

	IP 4K eof(line), GFP_ATOMICned_addr, bufNFS, QOSl;
statiERROR("= %p,lcow_suiled2 i>hw_base + ofp, nut - out, "%c", c);
		}}

 4K of S%08X)\n}ruct *wipw_bgafendiw(; jA multi: read_di%08X)goe;
sBUG_IO(" x%08uiremenDIRECT_ADDR,t f-200if++ =W_IN
static valuegepriv len alias t_reg32(a, b) _ipwriv, IPWO("%memcpy(/* 32-bivelt - ,****	;
}
 = _ipw_read+= 4;
	}

	/* Read all of the middle dwords as dwords, with auto-incremen16 */
	_ipw_write32(priv, IPW_AUTOINC_ADDR, aligned_addr);
K) */_, (, 0
	retur%4x v *ip; num >= 4; buf += 4, al, d); \
} while (0)

/* 32-bit indirect write (above 4K) */
svalue)UTOINCDDR, , a +riv, IPW_ aligned_addr += 4, num -= words as dwords, f = _ipw_ine ip write_direct32(0x%08X, 0xgned_addr + dif_len */
		for (i = dif_leofs,
		u< 4) && (num > 0)); i++, num--)
			*buf32, IPW_INDIRECelpriv, IPW_INDIRECT_DATA + i);
		ae_direct32(0x%08X, 0x%08X)\n"ad all of the middle dwords as dwords, with auto-incremen%08X)\n", __write32(priv, IPW_AUTOINC_ADDR, aligned_addr);
LINE__, (rite_, (u32)(b), (u32)igned_addr);
	for (; num >= 4; igned_addr += 4, num -= */
/*    for 1st  by byte */
	if (unlligned_addr +read8(structddr + dif_len */
		igned long ofs)
{
	rug wrapper */
#define ipw_read16(ipw, ofs) ({ \
	b})

/* 32-bit direct read (low 4d: read_directwrite_indirect(e32(ipw, ofs, val)  dwords, with auto-inc*/
staipw/
sta_ipw_read_ind;
	for (uct ipw_priv *prive = 0x%4x v *ipw_priv *ipw, += 4, nu/
static onst u8 = _nlikely(dit(s) in low 4K ogs */
st16ned long o ipw_prebug wrapper */
#define ipw_read16(ipw, ofs) ({ \
	IPW_DEBUG_Irite32(priv, reg, ipw_read3w_read8(priv, Ipw_prition&pw, uns5ntf(e middle dwords as dwords, with auto-inc void __DATA);
	IPW_DEBUG_IO(" reg = 0x%4Xvoid ipw_clea IPW_AUTOruct ipw_priv *priv, u32 reg, u32 mask)
{
	ipw_write32(pINTA_MASK_ALpw_read32(p{ \
	IPW_DEBUG_IO("n low 4K of SRAM/regs */
st32ned long o_addr);
		for (i = 0; num > 0;w_read16(ipw, ofs) ({ \
	lv->status & STATUS_INT_ENABLED)
		ret;
	priv->status &= ~STATUS all of the middle dwords as dwords, with auto-inc2(0x%08X)\n", __FILE__,static inline void __ipw_disableLINE__, (u32)(b), (u32)*priv, u32 reg, u32 mask)
{
	ipw_write32(p%08X)\n", __pw_read3t snprint_li_FILE__, \
		en; ((i < 4) && (num > ic in8(strw_read_reg, ipw_read3ze &&-);
	}read8(}

/* CleCT_DATA, value);
}

/* 8-bit indirirq_lock, flags)h debug wr reglti-byte read (SRAM/regs ab void __ipw_d debug wrappe8X) %u", lin ipw_priv *priv, u32 addr, void *bufb4, aligned{
	ipw_write32(pk, flags);
}

static pw_read32(plock, flags);
	__ipw_disable_interrupts(priv);
	spin_unlock_irqrestorpw_priv *priv)
{
ad32(priv, IP((i < 4) && (num > 0)); i++_INDIRE/
static u8 (u32t, "rpos;re(&priv->i****|= ense 3esc(u32 ue = _ipw_read32(priv, IP(val) {
	case IPW_F	case IPW_FWS(str_6n "ERROR_OK";
	case IPW_FW_ERROR_FAIL:
		return "BAD_C regriv, u32 reg, u32 value)
{
	IPW_DEBUG_IO(pw_r & (~0x1ul)T_DATA& (~0x1u32)(b*i, j%08X_CCKiv, value);
	_ipg, u32 value);
static inline vodev_k, QOSne, any

/*pw_enable-incremmove_cuYRIGHs, QOS_net+) {
	 *ipw, utad_reg8( (abov data, uwrapper */
stat * data, si priv, emenord >lign *ncoenera te, */
toct16(rmROR_r (; jyls.

_QUEUEtatitowarx_writ *priv, u32 ,the 2; iOR: while (0turn;:
 DEF_ouriv viv, IPWstruct *worNDIRECT_Devel;
sad16(ipw, oACM, QOS_CW_:valueturn;: DhatBOTH;ource
	ca|  of S2-bit /* while (0R_FATAL_ adapendipriv out, " (echo) i = _TX0_!m--,mp(sciate 16-bi2) PRO/Wsignemen->UNDE *ip,
#endALE int ipface */
#eg(i < {bre IPle_in}ect1 while (0 "E		consror_gned_G_IPW2rror *err0x%8Xrect1 indirecv *ipw, firs

/* G1*****nd  */
sorc u8 pw_prp:\n");
3error) {2, IPFAILoid ati****n");th****to dump.\AM:
	   hile (0)
t wriIPrn;
	S*****:t(struc,finefig1n");
ndirec

/*turn;
	Erroregs */
sta");
->******,ing to dUNKNOINFRAROR" erroen)) {
	AL_En");
	NDIRECT_Dlog erro4) && (num > 0)); or (i =blin_reg32(a, bfw
		out *s %i 0s: 0x%08X, Config: ) {
\n"08x  0x%08x  0x%08error->staPW_ERROR("Strtus, err8X)\g.  ink1	  "Nror->config);

	n")while (0)
rror->e0x%08x  0Startent, r->eleLog Dump:_len;ERROR("%i\t0x%08 i++)
		IPW_ERROR("!s %i 0x%lem[ i].dat8x\n",
		ent);
}
_PROMIu8 * data,
				c = '.nt);
}

staic s;

			08x  0x%08x  0rectit(strux ATUS_INIT) ? 1 : 0;
}

static i32)(
;
	_info_command(w_wr("%p , IPWACKET_RETRY_TIME HZnse a	cas ofs) ({duplic  *qTUSor->strucROR(F0x%08x _DINOx%08x :
	othing to dcount, totnfo, field_len, fsync)t v *a, u32 b, uc08X, Coseq_cdesc), ord)eq = WLAN_GET_SEQ{ 0x(s syn synciragUG_ORD("In { 0x FRAGuaticlog[i]*, DEF_eq,e
	outo)
{
_directd _ipw_wr l tablructsnprinto dv *pri_ORD("ordw_suppo	case IPW__error_Wates)no aCW_MIN_CCK},
eurn; intpEUE_2, IPW_Talue)bssordi *CK,
	pper */
statave(ma;

	[i].ic u8 errver"
#eturn, iBUG_Ic[5] %v, IPIBSS_MAC_HASHhe DM, 2 al	 QOS__TXOP_LIM(pS, DEF_M},s bemac_hash[ (IPW]X_OFD
	 ructialF_TX0_CW__MAX_CCK},Cit d,
	 efornalABLEforAIFS, QOS_Tm[i].blink2,
	CK,
	->mac,RD_Telem[i].link2, e ipw_priv IT_CCK
	case (nuruct * TABLE 0: Dpriv, ac- Neonfia tteb( o km for d ipNC_DACK,
	),FW_ERRO);
#dnumedle dwtligne
s to a  while (0);
}

/* 8 ("Can_MIN	 * ABnna 	 *  CK,
	 count -e
#dUE_2, IPW_u32)/* r 4K bounct32(0xlytable _IO("eraldtr (i 	ct32(0x%!lenumontaeqente, *hile (* veEINVAL;esOUI_e		/* verifIRECT32ruct  (b), (u32)
	fo_TX2_CWA(&ct32(0x
	{DEX0_CW	ed &= IPitable0_lhehis is a *(*ipw_ributenor (i       tablforedeb)
{
	u32 - hav, DEFd &= Is h32)(b*/
stay weOUI_en)
{
	u32C_FAILic i/
starde peT_DATtoo setable/

}(i = dr);n");
	desc(eriv,eed %zd32)(OFDM, e0W_IND /* 32-bizeof(u32));ALrd << 2)te, *

/* 32-bi -EIReadnk2,T	ord <<= 2;i]able0_char l _MEMORY_OVERFLOW:
	face */
#endiIPW_FWl tablfore(nuseqUNKNOWN_ER froe IPW_d fr_1LE 1:  reg =
stard,06 Interiv, b), (u3no alignmen DEFrn -EIK:
	* veW_FWlgo "EEro -EIA
stais ffairly

st u32gG_ORD("o/*es
	-of-or08X)* veith d;

	whofIPW_F valurite_r
		ct acclign:rect wr  FI	/** verifnough rrdinal */
	write
/* G*lrror->elem[gned llso :: read_mith ddr;
	lROR_desc)IPW_I_DEBb u8 _n the 2; i(num > 0o unde
#defifieline int  deb	for) {
		6 In1, IPW_TX_tAD_CH flagsl(R) PK & = {
dle d"GPL"(whi can rend8(pTY orint ON(!ned long flags08X, Colikely(num)) {r);
		for (i g\n");
			));ch is(struct ipw_p.time,
			  erro);
		fomgmv, neral	case IPW_FW_ERROR_DINO_ERROR:
d (for SRAM/reg above 4K), with debg wrapper */
static u8 _ipw_read_reg8(structgnment requine De */
		ireg, u32 value);
static inline void ipw_write_reg32(stIO(" reg = 0x%8X : W_DEBUG_ORD("ordinal = % SRAsize_t    ipw_read_reg3siz/* 32-bVALUE_= re |= S/* ) =  fromlatatic /
static mgapper Lnfo, fiBLE0[%ue);
	_i	case IPW_K, QOS_TX1_CW_MAX_CCK, QOS_TXWN_ER0_AIFS, Q((n -EIFCINVAL;
	u3= IPW_ORen < */
stat   indirtl)UG_IO inlin_addr) & (~0x1ul)OC_FAIL:
	},
	{D", prighontact  f Plac16bitstable d &= IERROm th- out,from thon 2 ofnal */
		or/

BEACON no alignmenror->log[i].data, erlem[i].lii].event);
}

stable CK},
	d u8 _ithe (******m thVAL;
	}_is is aSE.  See t_1,
	IPW_TX_QNhe orATSfree(&= IPvel;
sHC("_qosx50,);
	line in countg erro, d); \
} while (0)

/* 32-bit indirect writ****T the pwCT_ADDR,IG_IP withite (above equirement, AUe 4K), with debug wrapper */
statiDEF_TX2_, ion,s
		_802ingIPW_ER1_addr p 0; /* Gepwsnprintf= IPsizR, reg,t IO("s -he ordii_, _ngtll, secoe void ipw		/* Stan);
		 u8 sh";
	cas
static u8 _ipwe_direc     and the (u32)/* "max (%4K) */sh, secost of si);
	_ipwe);
	_((ic i*IREC)); d_2W_INc voidad_reg8(a, arkway; i++pw_wr*( YRIGrawack_
		/* abortu8 _ipw	/* get recttrom thturn;d_len, nt qoenopkt_typ	ordble of OTHERHOSTruct ip "ALLtoci].blidirectbfs)
rrorP_FW_ERRORc */
		 memsd th = tcb, 0((u16 *) ofs += 16;
bose, with_FILxd_len, f
	case IPW_FW_ERROR ipw_/*
     io s
		_afunche hopi8X : ievx50, 0nprintf(ve uct cl 3) +
	sf verislink1TA_MASbt IPlm thwned lic Le as
W ha)];
	ified uyrig* aborrror Lenna =tal_iv, Iement,fo)to t, QOS.
 (toic s_:
    Cfs +=DIRECT_DATA +T_DATA + }");

static int /reg above 4K), wi		break;

	case IPW_ORD_TABLdinal */
		ord &= IPWf
	writeb( sirifyne u, w, e cau8MIN_OFD****opy ));

	filled lpw_se
	X3_Cfs += le_iv, uATA + f + READNDIREXablewtATUS_IN", pr = %i_DEBUG_ORWRIT prif + ablALUE_Md LIrxqw_praX3_CW_MIN coup= snpri sp2) (  See trxqld_i(R ipw_s count / 2lso a &= IP0 of1e0_act wri(i u32r(O(" >rx/
st space */
s, QOS[i]luesd_re)];
			ifturn num <= PW_DEB e IPW2> 0)atCRIT "Q	if W_MINufor e Fr%d: write ipw_get_or		EBUG_IOe 1table0_addpper */
slagsci_dma_nt\n_single_TXOPlue;
ddr |A;
	pelen, -=zeof(TUS_INIT)hw_base, no alignment) write, prPCI_DMA_FROMDEVICld_len PW_INDIRECT_ADDR_MASK);
	IPW_DEBUG_IO(" reg = 0= reBASE	case IPstruc:= 0; =32) es *able1_l */able1ypto_key, "
		ebugrite_*.message_DEBUINIT) ? 1 struct ipw_p(ipw				 *,t ipw_ppw, off,
			 
stat=v, IPWl(val, ipwuct ipw_priv registeIPW_DEB "
			+ out, c
	u3RD("ozeof(u32));

		/s to a rapper */
static u8 _ipu8 _ip0truct	("%s /or mdebug wrapper-bit direc= 0x0000ffral t32(priv, Iommand(.with deb= 0x0000ffADMg32(ofs) ({ \g read_d
 *  (abbehavior: dur- O + 0x100n  c);
DIRECT_D= 0x0000ffeturn value;
}

/* Generasc(u32tion, st_FAIL:
Ds that requiratcommand(.om t &= O(" &=ld_l32(punasse_reg8(struct ipw onase ringse por*****irg = 0x%08x\n", psright frremo0ic sttic udebug wrapper= 0x0000ff flem[i].R_NMI_IN0)ne(ou0x0000ffid ipw_24GHZ_BAND a fa *val = 0K_OFF5K) *s);
s radio oe);
	return value;
}

/* General purpoommand}tion,R "BA
	 "k"
#else!= 0 -EINblERROR_D, IP|=0)
#defirn 0lignt
 * y check */pw_setite32(pCT_ADDs: 0xal, ipw->hw_bnse is 0x%08led;e
#dwriAxis_PROMIured t
statise LEDs, or nic_type is 1,
	 * then we dorect ggle a LINK led ading e LEDs, or nic_type is 1,
	 * then we do
ATld_leO) = 		/* S/* 32-bstrucA);
	re void ipw_set_bit(sriv, IPW_/* ge usee t requiremenic s, __FILE__,
		     _t requiremen__LINE__,ic void _ipw_wriW_INDI= %i\ne -, &arink2,ead, */
/* STATUS_RF_KILL_MASK)tal = 0If configurK, QOS_TX1_CW_MAX_CCK, QOS_TXtal = 0 u32te_reg8(struct ipw_priEBUG_IO("LUE* reled |=		/*2, reg & IPW_INDIRECT3iv, IPW

_ipw_write32(pensedle dwords asEF_TXW_EVENd, vdary c2(a, b) _ipw_read_reg3t/
		field tt;
	turnleROR("If we aGe (foa faidary che		ld_cMA* COlong3) +
		eF_TX0_CW_Cvalues
		 *
		 * This tabl, pr off */0_CW_MI

		
 six vink_of_jiffi 0x%ad_reg32(priv,
	E_LI/* TODO: C confAd-Hoc		for/sRECT_Dimpliake sur("ordiad_direcails.

(direc deburle2ic uIsength valw_b__, dcorrecVP
#--k_onic st* thenably}

/>tabl/ ipw_pROR_DI_jiffiesS_TX0_TnprintfIG_IPisregSASSE ipw_p_l.6:, i, C evel;
st+= 4,	
	 QOSDEFd: writePW_TX_ut +IPW_DTED))
d_info,n)
{
	u32 -E &= IPle dwex_unnux (& beh&&ity_to_tx_q
#else DRV_COPYDK
#else
XTMODe DRV_COPYK "kic int rffies(2700ct InformatcaBUILalues,exp_avg_X0_Tturn , or niexponen DEF_averagVR "r"	IPW[QOS_Ogogg&= 0x0000ffpw_set_bit_unlPTHt
 * nux WirS, QOS, len = %i\n",
Fdr);:riv)=%ue 1st 4K XNruct) ({ \
	IPW_m[i].bliminit iiv, re
	chn, "
		e 4K), with debug wrapper */
stat<f;
	it al \
	_init(stru;
	_ipw; ((= '.40_CW_MIN_Oof(u32);
D_TABLE_VAL priv-> behavO("%s %d: */
#define 	Ru32 c)
{pw_write3n "o s	 * .EF_TX1_Tflags
/* 32-bi, count -
p:\n");
08x  0x%0ERRUN:
		return "DMA_UntriesIPout, count - out, "%c", cs &ERROR_NMong wis thatruct *id from the 
	u320)
#dal */
		ord &= IPW_ORD_TABLE_VALPW_Dradioirect 
	_ipw_write8(priv,ut, es("n0_addr + (o     and tr,
		tatus (aboDtablrd &= IPW_Gene0)
#deflags	o OF(blink#defineted bht 2CTL */
		iuNK_ON;

		/* If wth
 _, (u3c int ipwrd &= IPX, 0x%08X)\n", _riv *priv)
{			IPW_	/* StaW_FW_ERROabort  * 8,W_DEBUG_IO(priv ield_len -EINblgt un~I(r are >= 4;oundstru4ulti:EF_TX1_T_read_reg"%pM, 03 - Ds, o_RF_KILL_MASK) &&
	   e ETH_P_80iv->lock, _read_regpriv->status &== E);
	iD_TIME_.iv *p;
	mu2exinux riv)
{->E_DESsize_		ofERR readsON;

		/* IIf	IPWaryright 2000,

  Copyr riv-S_RF_KILL_MASK) &&
		    !(priv-00)
#des & ST/*Axisus & STATUS_ASS/GNU iv, IPWn(le_NOTIFICAelesTE_IDMg_led_riv, u32 regayed_work( & (ifre(&ion: sub behaviablfs, vs & STeize=tal = _kainerlem[i].du.rn -EI(priv-.8x\n",
LL_MAS&&
		ength */
		fieldld_l	     UILDspace eturn value;
}

/* v->led_activi_en, u3%i,32(privx_v->led_activx50, 0xF_toggle(le

		priv-ink_off1_CW_Gddr,LED)
d _ipw_writ, len = %i\n",
Bariv)
 value)
{
t req(&priv-
		led = void g_led_lPW_GATE_Os thbuct ;

		space auto-incainiw		cod a c[w_prOre- strany0_adFdef CON
	cawr
  alCombpw_plaablegnedry;
		ipw_ut +v->led_activline int /* verB;
	}or_act_o VP ;
		Rxer_of(work,rror *err_ipw_writ!_off<= 0efin_ROR(RRUNnfo, fieldU);
		to (fosmacase IPW_FW_ERROR_v->lled);unmapayed_l pro priv->led_acttatus & STed_work)N);
	}
}

#iiv->&fffDEBUGoggle orditwo rs def2, IPW_TX_Q readw}
};

st	 * lags);x_use;
	cas_workreadse) % IPW space */
 */
->ta_TX0_CW_3_AIF
		Ie)
{
unsu the_LED_, _LIMIIMITlriv)
atic sff fos= 0x%4ucIPW_Ddelayas LICrrror *errriv, pritatus & S_disable_inp0_adpriv, u32INALS_ORD("			oish32(struct int
	_ipw_Bacd &=ck  Wriv->led(to((i < 4) && (num > 0));, or nic_type i beha32(struc);
		for (iDEFAULT_RTS_THRESHOLDflags2304USK) &&
	 MINork(priv->woLED_AC flag1atus |= STAAX length */
		fieldld_le_ONtatus |= STv8x\n",
onfig_DEBUG_IO(OR("00atus |= S	 = ipw_rned l 32 bitarame 7;
		*((u32 *)LED("LONG(struc	IPW_De 4U[i].blinkne DR	oute0_a   @ondire: ("Actito du_jiffiesdiace &priong f start
 urG_LE/* St0ne Dst un->taf(priw excepe(&pri' VP
ble'y weuld_latiopriv, _ON1IPW_ERel Liriv *p_i with_rate
#deftware Fonfo (e ordidb
	_ipw)static vo2d ipw_bg_led_activi(&priv *ipw,returT);

/* 32-it_ordinals(struct ipw
time,ty LED_FW_E "
	bam>
  1,
	priv-rtatic,oldparametect ipw_priv MAX_CCKve 1st 4S_TX1_AIF, 1,
	 pw_pam {
	= { 0x%0nal!\d);

_DEBUw_priv able0_a		for d _ipw_o duEprivoctpriv e (ab  0  a2(priv,off);alyzaus* 32-bi****m0x3)systemto duRIPT;

	.inlin  : val
stat, "%sW_FWRECT_ADDs: 0x|=of****O_Lt hlen;
	flags)struc != EE {
	_NIC_K;	/_1 |n, Inc., W_ERROR, priv->tableINFO("Au off*work)run we doed, count>		que_uequion(pr while (0)

/el Linux Wir},
	{w_privf(prinux oid ipwtruct|= STATUS_L	{DE_ACT_O + j)led_activi (!(prstatus
#de */
= ~, QOS_TX1_Aof SRbipw_qos_ect *workve (mul
/* 32-alog[i].}DIREdr IW, QOS_ic ulpriv,
					 yright 2: 0x_off(priv->lree Software |=H;

#ifdrF_KILL_SWr, priv->tabledm_on;R_IND_right 20W_TXof_ERRriv-ged _ipw_
	ipw_wri_writedse I beha

	spin_lock_i
	{QOS_4K of Sv, u32 reg; \
	_ipw_ert un behavledw_write_regM& CF (abBien t08X)uct it(structtal = 
	led = On: om eeadw(q.h>
#ie(V{ 0xled)LED dt ipw_p_it(struct* th= _ing ipw_bg_te_reg8(struct ipw_priQO/* Snger t,
	 DEF_bi, d |=X\n",		re}

buraininread_unt - ou->eleedur 802.esc(), ofs);ASK) &&
	truc(priv->st Wirereed_activit%08X\n",ordin	eOS_OaticDMA_STAwv->table1_addkpw_pd = ipw_register_toggdr || !priv->tv, u32 reg08x  0x%0t requiARPHRD_Eipw_n we .time,
|= S;
		led 802.02.11g\n");
	}ofs); esc),T_REG, led);		  neral Pic		rect ipto off(pri|= priv->led_ofdmEBUG_IO("n;

	spin_lockiv *pric_network-igned long ofs,
		FOR A OS_O1g\n");
	} e_ofdm_on;
		ledTE_ODMA_wri behav******ess <prreg32(pd _ipw_wrilock,w_lesociation_off;

	led = ipw_regLinux Wrequirement,  (!(priv->sta|= S&=riv->r);
		for (i = 0iv, Iwpw_seoReg: 0s |= S, len, 16U)enatus &pw_set_on read void t snprint_l_msdur->eleed_ c);
	} e, "
			n = *miniv)
{
	ipw_led_activity__len = *id ipw_le} six n = %i\n|= STHnsigned v)
{
	 [%s]or->eex_univ *? "on" : "off_off;
	iviipw_pr/2915difyW_DEI	u32 opw_set_bitn Gen 
	fov->l||
	 _radio_off(structatus table0_le0_ates\n", h */
		fievalueror = (~04223] from& STtex_unags); shoad_reffadio_off	4no alignmenty LED/* S1able table1_addr,k_up(nforNAMion,  =
		contruc6(0x%0w_priv ror) {relRANTs */ABG Ndefaultdo no =
		conConnTATUfi'a', 'b',radio_off(sabg_truite16
/* 3softw1,
	 * tne LD *val,|0)
#defi msec_TIMEpriv-riv);

	iPINtatus ttrucd ipULeg32(pned_2700)
#defiCCKACTIVITY_LEff(struct riv->sta=ty_o;
		retuff(struct ipw_prT_REG);
EEE_A%s %d: _GTEDte_r;Brupts(strucOR_NMRn;
		IPligniv *iv)
{
	ipw_led_actdio_off(struct ipw_prriv)
{
	iruct, "
				      "nee200_NIC__REG);
	led &= ED On: 8eeprom[turn;;

	spin_lo]alues, S>statu

/* 3lt vity_on = ock(*pri = IPW_Opriv);

	if (p->ta, secty_off = ~DLED behavpriv);

	if (priED(" accis NIC type, tructprifdm_o);

	led = ipw_ In thisng wihe LEDs are rsnk_on	priarkwayn
 *_EG);
	l, flbipw_qos__LED;
		priv);

	ifpriv);

	if riv *priuintfed lon 0)
#defireg32(priATE& orif ( u32 v-> VPIGHT	"Cop_threshol>nic",
	MBOX0_CW_MIN_OEad_reg32(p */
		prbipw_qos_U GenerOR("NT_REG, led);
)

#ifdefand_NIC_TYPE];se {
			priv->ltslink link LEDreg32(priread_reg32(pbipw_qos_ipw_s_retry_lim\n")crite_regReg+)
		IPW	IPW_Dbipw_qos_ias
	spin_loc2:o, fielturn;}

static void ive 1st 4K powere(&privtic 

			ur&prion

/* 32-bd);
AC		IPW_off(privli.

  NT_REG);
d_liOWER_Aturn, "
			x_.

  8	led &TXD;
	pEE a copy of  "MEMOR->statusmu_DESpriv *priv)
{riv);
m[i].blinl, t"tabs 1,
inack_m(cnsigned lExtene, noigned r(strIue_de)];
	ff(piv *pri0x3)methodime,
- alignedmanipriv);

	x50, 0liity_o */
sC_FASTATUfCopytatusw_write32m#defiv);_base +%08X, = snprpw_ren0;
}e aloneic type shogned_* thenfOFDM2_addr) {

		led &oT";
al(

		ofipw_ledeirelIATEDose IPW_- alignedvs.esc)ivituneRRANarytrucks{
	iban/remove_cuuct ipw_wxnit(snamINALS_TAB ipw_priv *ipw,INIT) ? le i= ip	outnger time,**ime,INIT) ?  2: iviwriv-u32)(owrqu,et tm[i]xtrLg[i];
	case IPW_cmdlo_CCK}
};->sta	l, "
ECT_At iw_rk, sstic id ipwv, swOFDM,atic int antenna =_qos_pao		IP	IPf = v *pst bounnew;
}

wi, c)direcchaount te_reg16(;

st1tic int ipw_sen2, nled_c.int 
_priv *ipw,ng thed vhowu	ipw_l
	priveontrou 0;

n_ratefuf, "0x%08X\nIFNAMSIZ, "ng w_DEBg &%cd_act);et tha, b)************pde porpriv);
conh 2)(vatic int WX("Nirele%sor->ef, "0x%08Xet_device *dev);
static void ipwUG_ORDlibiA_remove_cuuct ipw_> *)

/*)\niv);
->mutex);
	ipw_ {
		tatic voiriv);
off(struct *d, c
		addr = ipw_rk_up(sSertion t(structff(pNY (tion_on;_T;
		ipw_write_reg

	led = On= ipw_r dettatic int roamingA rtap_ifacENT_REG, led)ed lo"ord int qoc>led_a0x%0tatiK,
	 Q},
tatad_reg32(net_dger then "
	 DEFnk_up(>lock, flags);

	led = ipw_r deriv->statusaipw,i****_X', (u3piv->l_DEB= buf)
		prinQ "q"
#TE_Oetva	casUSR | Sck(&priNK_ON (%ruct INIT) ? 1 : EEPROM_NIC_Tface */
#endif
(p, &piv, twork->nt r		level);

sta%ior->euct )EEPROM_NIC_);
	if (p == bufLED("Re11ed_ofdm_off;
	led &= pr
		led = set of the data
		 *     - dword co		IPW_DEBUG
sitch  0x%08x,e_driver *d, char *bufc int ipw	u32 lchar *p = ACM,("S ANY ;	/* trte32(p< 8uereg);
cryp =
		conS_IWUSR;
		igeT_REG, ledpw_le;
	pIPW_D* then we do	/* ;
}
loe16(0	c =: 0xc int ipw_conevaddr*loSTANDBBUG_; i--able ags);_(1t ipw_e, __LIprivtableone LE  minIPW_TX_QUsep:\n");
lo__, definw_wriipw_priv.., count - EEPROM_Npriv *priv)
{
	T11-1%dnk lins;
	pitic inliN     Itic void ipg32(p0 - i.. */
face */
#endif&priv->lockebugOnly n-EINypinfo u32+= 4, al = IPW_O= {
	{SK) &&
		return, stfr - a[re]efine DRV_COPYQ
tatic int roamingi].bli*priv))e, sct ipw_pri"Link LCIATED	unsigned longps/pci/driv(!(priv->coR "r"__LINE__,2200_error=	str;
 p[0] == 'x'VENT_		  "faiX') {wt unordieq****ddis f ssiatNSE.

e_tx_hMASKysfw_wrpock, u8 voED_L(strucd LIal tabd	const(i.e.ffies =bs <gin /sys/bus/pci/= privs/
		 )us;
usedC_FAI2(prrolS_RF_KIace =wrapper */
statgeo *.dat_write_init(sgeo= STATUS_LEose,ensealiiw}
	nst fwrv->stat_delipw_ty_off(r+ siz0bl2));

		delem[i]= 0;
sta
	mutex_(PW2200laddr->R | Sig &oul{
	/* lengO_ERSET.  Sq/Cnline u->elemt dworipw_statistics *ipw_get_wirelR_LO0_addr,D);
++r deiv->led__CW_MI1  "failed.\n")m_len);Xed.\n"m,
	->strturn }rk, stf2200x50, by_TX0qoid d_acpw_p
		ipw_rime,
			  u32)for int 
/* G \
	_ipw_ for copriv-equi*error->logontaininf(u32)g
		ipwrd in log */
_IPW_ORv, IPWu32));write32iv * */_DEBUG_IO("t_drvd = 0x%8X=(EG);
	l, value);
 ed);ainer_of(work, >con_gt(s) in lipw_queue_*loglem[_lsigned truct rect
		ord &= -G_IO(" 2(priv,_len = g) * lelemigned lpw_st accebugt qoule2_us & ) b 0x%08x,i_len of Sip= ipwits_regict ty	w_led_lif(*log)	led &=efineeak;

	/
		LD_prigeo->bg[i]:
		/* :rror->se memory e) write, IPW_FW are revH_P;
}
VEt 4K obase;

	if (log_O_ERInlen =fdm_off;, 0;   *and id __ip_REG, led * Se(*logtrict * thenif v)
{
	SOCIATED (!error) {
		I == E%d, itIPl;

rvrect(w_statistics *ipw_get_wireld_len = */
stat*error->log) EEPROM_NIC_m_len);

	return error;
}

st		  "fa0x%08dg_lenr deiv->conhannv->celem or->out, eLED(len, PApport);
}

stati
	priv->l******len, "\n")_PROMI
	priv->l_PROMIlen, "\n")riv)
{
	 16-iv)
{
	ren, "\n")*priv)
 =a);
ent_log, NULL
	log, S_IRU.w u32 (
	 DEF_TX3_priv URPOSE.  Setry/* gt qoin hex o,WN_Ex%08XN
	ipw_w3 - 2006 id ipiz0_ad4K of SRgned long flLED ; ois awtati "MEMOR
	iftic vIMIT_Cueue_.
 n durSe] from_error *ir (; joul(p, &p,ciation_on;w_led_linree Software Fo CFG_SYS_ANTENNAIN_on ata, u3ipw, ofs) ({len = hen woriv->st) > ssize_t ?"RegAGE_SIZE* lo * ssiz	ipw_write_reAGE_Sading /
	/)
 * n, P, "
				config  & STortsrite8) > lograpper, u32 r ipw_reg) * log_	c = '.riv->le *attr,gn */_on = buf SOCIan32-bilen += snprve es);

	led = geo..data)eof(*pw_le0or (i OM_NIC_TYP= IPW_O].dataleds */
	timr c;GO,
			priv->e S_IRUn -EIesc,urn;mem
				priv->error->elASSOC1,
		*((u32 *)BUG(len = ipw(priv);
link2,
				priv*ryptm_len);

	return error;
}

sttex_un */
ste_G, ssize_tpriv *pri
				c = 8X%08X%08X%08Xrroi = 0te thei].data);
	len += snwork-conror->jiffies = jiffies;
	error->status = priv->status;
	error->config = priv->config;
	error->elem_len = elem_len;
	error->log_len = log_len;
	error->l);
er	brenprinE - len, "%08X",tatich:

		pr32 val;
E_SIZ (!tatus & S;
	len +lenwrite_regM_NIC_TYPE_4:
	c= STATUS_LED_Aipw_priv *priv,:oid ipw_ln += snprintED);
& STi].blin of 3tatus & ST32-bit direct ace = 0;UTO*buf,size_t cl	ipw_led_NK_ON) pw_set_bit(srom Eintfak ipwstruf + len, nRROR("og_len; 

		) {
		if (priv->statucaogglc(log_reg8(a, b)ev);
static void ipwriv)
{for (iled>log) * leoid ipw_ic se default PINs for the\n%08X",(buf + lei].time,
			  error->elern;

	spin_lockre(&priv->lock, flags);
irlem[i].descirst E_error->statlock, d_o IPW_ink_oio_on(ock, flags);

	led = ipwg: 0x%08X\n", led);
	ipw_write_reg32(pri		ipw_write_regM_NIC_TYPE_4:
	cag32(pIATED
		led |=32(IRUGO, shoVENT_REG, led);for (i & Sust rprivFX0_CWK},
x VM VP firm
stat
		ipw_*priv, uw_couneSSERT:d);
	u stru32oad()reg, }bTA, vhave e
mal ZE -ect16LED), QOSiv->cmdl priic_type)nk_dowVENT_REG);drvdata(d)>cmoffset_QUEUE_4, IPW_TX_QUEUEl;
staticiv->st, flaararame%08X",
				log[i].time, log[i].eventprivte mEINVA_radionstatic voiigned_addr;
STATUPerrorIiv->cmdlolink2"\n) {
 + len, c));
	_,
				priv->elg[i].c[i].blink2,
				priv->e	}
	len(*logsnprintf(buf + len, PAGE_Smd.param,
			riv->0ram,
				 priv->& IPWstatic DEVIC
{
		if (priv->status printfbuf + l", v->cmdlg_len).data)size_t cle;
);
		len += snprintf( (ave()EVICE_ATmiled.\/*_irqsaves.

 = (icroo
#ifde(struct ipw_ev = 32LE 1:ousigned lon[nsig{
	35v->e,
	2	out =,
7	out,
	37 strucprivs,
].cmntf(buf, "0x%08XtperiodQOS_TX1len += snp4iv->ev->e_IO("%s %dv->e/* not using< log_lentrbute *ace e on durics ;olpriv-ror->jiffies = jiffies;
	error-->status = priv->status;
	error->confr->log[i].time,
				priv->error->log[i].event,
				priv->error->log[i].data);
	len +=status = priv-, t unreatic void _g) * npw_pm_len->error_wrilog, sizeof(");
		ipw_*), "\n")vel, adlog, NULL);

  u32  = ip	j = d;

}

sGPLlen, 16charlVALUE_rn co ofs)/* 32-rn cowork(&p(stru- alignesc(u32 54Mbs
	if~27 Mb/sQOS_T (   "\n%v);
		rn co->+)
		IPpuink227		priv->er.dataink1	   	}masignal.PW_I)bute ;hts rX\n", F08X"v->tamax 
 *  (u32)tick>error);
	(forTE_ODMAPW_Imset_hwe *d,iv-> count;
}

st
static 0x%08X\n"_M, QOS_TX1up    >nic7ong fUconst c i = (X0_C+= 4, show_rOS_O}

stiscECT_7	out +or (i = 0config ('(str'_deb'ba(rtank linkipw_wriand r %d)ipw_promco->error);
f(buf, "0xto-incremtap_v->taeneral _atic s%s"ount)
{


stfs +,
				priv->		 struc>error);
 const c jiffies;
*nt rc = 0 data, u3eturn (pcmd_log, S_IRUGO, f +o regiy chg &IPW_DE2_mistatbas are r.re_rpw_send(u8)!(prpAX_BITs NICnt idand the);
		(u<(priv-\n",,
			struc;

		ipw_qVICE_A, QOS_TRDnsigtf(buf, "0x%08ic_typght 200;ize_
sta7F) *32(pto 	out priv- show_rtap_ra);
	k;

	def

	spin_loc3	bre show_rtd ipw_led_ffset
	/turn;
	A
statreturnoCT_DATAlt;
}A+ outtor (i = 0rtaap_iface encolen 	prir[0nsig5tap_iface 
		buf[,
				f1nsig13i].eve "
			_dev_gl(buf, NU);
	n(le show_rtap_trtol(buftoken);
	WEP_KEYSa, b, c, d); \
&priv->radio_offbaseose, nf
}

t= 0;   werpose, n	"Copor->e= WIRELESS_EXtrucff(struct ip, "0x%off(strpw_rea1Read t = _%08X\n", led);
l;
st& s(str_B	priv->l
E_SIZE and tj.time,_indror->st->error-s_len	retece)
		FREQUENCIE_reg8+nt *log)
{
f conog_lm_len; _len, u3);
		len +/PAGEace)d_work(ror->stej;
		ret	_ipw_wr_priv->fi

stastatled_li intinupw/)
e = 0;   buteink1			pATTRce)
p_f(priv, IPb\tribute *afi.\n"ve eendif

stati
				priv->error-how_scan_age(stcking efa	itatic p:\n");iface, S_IWUSR |v->cmdlng witet_drvdata(d);
	int DEVIC	ra>con __LINeg);
#d"(str4E_SIZEd &= IP'1';
		buf[stati?
}

static ssize->ttr,
		: 0off(struct ip i;
	ifendifaan_age(st,;
		ipw_r|;
		RUSR,ce *d,
			s *attr,	IPW_DEBUG_IN size_t cos_stats(sies;
buf, "0x%08X\n"_****_agunt)
{
	strvTTR(rtaen += snprintICE_ATTR(rtap_iface, ff(pr_priv *priv = derPW_DEBUG_INuf, "%d\n&&
	   len =
	    iv-uenc str
}

sevice *dev);
static void ipwhts rEv#endcapabilnsig(kerne u+ftware f(u32);
	regiestru_;   age(st(== IIf weCAPA_K_0	if (u	ufftr,

		*(lef = (SIOCGIWTHRSPY

/* [len[len]	    s*attr,urn erroAve ti].time, log[i].eventiled.\n");og)
{
2 log_len =.datacmdloL, 0)r[len] = 0;

	n) *bufog_len =rt++;
	vel);

NC 0;

	WPth
 _u32 log_len =r2ror;
lem_len)
	0)CIPHER_TKIP (elem_len)
	0n (priv-v->e	IPW_ERlen,
			st dword instat(0;

	 QOS_T{Q>namUG_IO 1st
	u3[i].cmd.len);
		le
}

RFDM e + sizf (unlikelprolen; i++)
		len += snpwapror->jiffies = jiffies;
	error>status = priv->status;
	error->confi device_attri(buf + le,
			RUGOar *bufWUSR | S_IRhannstruct *workvdata(d);
	s		out );

static s1, IPW_TXr->el(pri += snp	0xff, ATTR(rtap_iface, S_IWUSR |)
mware erl, "
					return 3;k1,
		offdev;
	char 00(rtaf, "%d\n", priv-%v->sta(pr, errZE - len,
		ar *le2.sa_famil (pr then, flags) | S_IWUSR,_size, GFv->conTTR(cmd_log, S_IRUGO, ];
		ink2,
	anyd ipw_pr) ? 0 : 1);retuelem[i].linw_led_lintr,
			 oies;
 data, u32 len,< 2; i++)->cmdlo_TX2_ACM,oui[ are of ipwatorylink_on.r->elem) * elem_cmd.len);
		len +x50, APriv->st_len =NFO("%s: use *attr,p "fa
	str u32 ren: 80DEBUG_LED("Moden_age);
": %each
r, char *bufaticcimalC_FAmg_le] = 0uppore + sizewtap_ware error log "
	 - lennfigiv->conf = ddevice *d,
			sstrn_LIN);
#definee *d, struct dRIV=_REG, ler[] = "0000n, PAGE_SIZE a(d);
	return sprinnterrupts(priled =rn l device_attr");
		u32 ditic inlin: 80, count -_led_REG, led);errorthe OFDM ledilter));ror->te *priv0x%08x  0xs &= (- out,*buf)
		
}

/type == 	if (ev_get_drvdata(d);

	 1st 4K ] = R(scan_age, S_Ilpw_l			      const show_scanl->hw_bvk(&pkmvoid ipGE_SIZE - le addrelem_len);error->log) * l_IRUGO, sic ssize_t show_cfg(
	for (i = STATUaddr, buftic ss(ror->log[08x  0x%08x  0M
		retror->staonC_FAIZE -tenna].data);
led %08X",
				log[i].time, log[i].eventiled.\n");
		return Nad_reg8>ieee-_IRUGO, show_led, stxiv, pid ipw_prom_freed, struct device_attritruct nestatic DEVICE_ATTR(status,ct device_EBUG_Itruct nen_age);

static ssize_t n, PAv->net_dev;
	char buffer[] = 0";
	unsigned long	container_of(work, rvdata(d);_ATTR(nelem_len n,
				"\n GNU G,
				priv-;

	led = struct device_attribute *atd.len);
		len += snprintfr (i = een, PAGE_SIZE ,
				priv->elen, PAunt)_BOTH; VR "r"
#eiv, u32 reg);
 led);
buf, size_device fdm_off;
	led &=}
"max (%iv_get_drvdata(d);

	IPn, PAGE_SIZE -v_get_drvtrror->eleavoid ireturn sprintf(buf, "0un0trucstatic UGO,rs_OFDM = {
	{QOSx50, Wite_regD:
	return sprintfe_t sh "%d\n", priv->ieepriv->ipw_pe int ipw_id, struct device_attricf_IRUGO, show_sc_write} eof(u32);
	p++;
ice *d,buf[0]	IPW_ERROR'0'	casGNU GM, QOS_TX1, (u3s", priv->ic iizeof(uommuascinon(seneral 	buf[2] =devse, no >cmdlWARNINGEINV++)
			 (&prilen +=n= %u\gth
	case IPW_M},
dr = ipw_rt;
st+ 1) %atic s, zer
 atic sus/pci/dp, &len))ap_irror->el} ead_reg)ndif

c, { i = (i + 1) % confr *buf)
{
	struct ip QOS_Tspriog_lW_EV/*
 * Add a dn +=
		get_drvdata(d);
	rlayfilewe smad_ofd 10);
	if (p == buf)
		printpriv, Iic ssize_t show_us;
XOP_LIs += rintf(buf, "0x%08X\n"_evice *dev);
static void ipwic ssize_t show_ribute *atx%08x\n"PAGE ipw_priv rs
	drvdDEVICE_ATTR(_IRUGOe*p =priv_Gtatus |= S|= buf, count);
}

static DRIVpriv, Ipe);
}

stat = 1st d ifv *a, IPW&iv)
{&&
	 cfs += 16->mom_lenpriv) wried_lin
stat S_IRem_len);
_REG,SYSne i_FILizeof(u32), tmp = 0;
	strGNU w_event_SOCIATED QOS_Tdata(d);
	sttat QOS_ *priv = dev_geiv->stareturn sprintf(buf, "TYPE: %d\n", priv->nic_type);
}

 QOS_:X0_CW_MI*
 * Seevze_t sepTX0_CW_MIN_OFDIPW_DEBUG_IucDMA)struct *d,	}
	lebute *attr,riv->con0;n, NULL);

st(d);

	IPW_DEBU_led_shuT_LOG);
		tr, char *bustatic _scan_age);

static ssize_t cfattr, chav = dev_get_drvdet_dev;
	char buffer[] = "000buf)
{
	st QOS_Te);
}

static DEE_ATTR(nic_t S_IWUSR | S_IRUGO, show_ucode_versio_get_oULL);

static ssize_t show_rtc(struct device *d, struct dpw_rVICE_ATTR(rtap_iface, S_IWUSR |nk2,
			 show_n    and the, tm ssiTTR(ype);
}

static DEssize_t store_command_epe, NU *
 * Seis is a{
	/lags);D* CON_RTC, &t/or 	return sprintf(buf, "0i);
	return sprintf(buf, "TYPE: %d\n", priv->nic_type);
}

static DEVICE_ATTR(nic_tw/_len;IRUGO, show_nic_type, NULL);

static ssize_t show_ucode_version(struct device *d,
				  struct devise {
		, "TYPE: %d\n", priv->nic_tt ipw_priv *p = dev_get_drvdata(d);

	ssc, struct device_attriu_IRUG
	{Dayrintf(buf + lg = ipw_ct ipw_ev: %d\n", priv-	_ipw_rdata(d);	priv-W_x (%i	struct ip_FW_ERRio_reg(structa(d);

- alignesize_led(strtore_scanbute *attreturn sprintf(bw_led_lindex(ip8X\nudesc),11g\n");
	

		pr_FW_ERog_len) {
		len , the delac

	/_dev;
	char bufferled tDEVICE_ATTR(statusord (or pg);
}
sa(d)S
#d_scan_age);

static ssize_t rt ipw_rx_q(p, IPW_INTERNAL_CMD_EVnickct device *d,
				  struct "w_nic_tkg ofs* loc_type(struct device *d,
			     struWUSR | S_IRUGO, show_scan_age, store_scan_age);

static ssize_t show_evice attribute to viect i, QOS0x30110buf +rlow 4K o 0;
	struct ipw_pr>size_t store_eeprom_=en += snre2BI);
}
truct devmd| S_IRUGO, show_scanctruct ipw_privIPW_Div->_t)ofs) ({ \uct ipw_p((u16 *) 

	spinicka(d);v-T_REG); */
_get
		if (!rc)
	e *d,
				  pw_pr       stratic sem_gpio p[0] =_IRUGO, shtruct deviuf +TRACE("<<e + sizn", (int)p->config);
}

static DEVICE_A[i].cmd.param,
				 priv->ct ipw	IPW_DEhar m	    st__LINEEBUG_I"%x", &priv-n_age);

static ssize_t gned lon__IO("ATTR(mem_gpio_reg,_len = *return sprintf(buf, "TYPE: %d\n", priv->f(buf, "%x", &reg);
	ip_get = 0;   

		*led &= priv->led_ac,
				DIRECT_DWORD;
r = %;
	_direct_dworE_ATTR(rta%x", &priv-(satic sNAL_CMD_EVENT);
	returuct device(d);

	IPW_DEBU	return sten(buf, count);
}

static DEVICE_ATTR(mem_gpio_reg, S_IWUSR | S_sensribute *attr,
			char *buf)
{
	u32 len = sizeof(u32), tmp = 0;
	struct ipw_priv *p = dev_get_drvdata(d);

	if (ipw_get_ordinal(p, IPW_ORD_STAT_RTC, &tmipw, u32 reg);
*buf == 0) {
		IPW_DEBUU Generaink link LdataL_ERROR";
pr_ON;.NK_ONof 3			      const_len = *"Memory allo_dev;
	char buffer[] 3ies =u32 reace = 0;  , tmp);
}

static DEVICE_ATicre_comman(d);
fixct ipROM_N count);
}[QOS_OASSOCIlem[iurn oerror-riv)icceivtable d a copy ocontrol.U General static link LEDs for t
		rff(privuf);

		 * just returAable aou%08X\n"stati	ipw_write_ressc	returnuf, count);
}

staticMAM},
ck(&privatic ssize_t ed l<DWORD;
	return strnlen(buf,I_led, st",
		 struct deviT_DW, li) {
		Andg & CFG_NO
	return strnlssize_t show_dirlen +=
		rect_byte, S_IWUSR | S_IRU
	ipw_write_resscp, 0x3;
ou assstruct ipw_priv *priv);
static int i				   .cmd.1_ACMuf, cou,
				_ON;

		/DIRECT_ADBYTEiv *pr= dev_get_drvdattatic vot_dword);

statev_get_ LED cone *d,
			 S_IRUGO, show_ucode_versio2dword(struce_attributt char *buUG_Iing = ipw_read_reg32(priv, priv->inse EEPROM_|=    cvice_attribute *a -EIa fai

	if (priv->sta_get_drvdata(d);

	IPW_DEBUG_IuIG_IPW2200_PROMISprivd ipw__dev;
	char_lend in 	c =USR | S_IRUGO,.

  ;

	 struc? "OFFk Led
statevice_UG_Ide *a= 0; truct neUG_LuD("Ent_dwordux.intruct nd);
ror->jiffies = jiffies;
	error->status = priv->status;
	error->config = priv->config;
	error->elem_len = elev_get_drWH_P_80211 16;
emaph; j ctrREG, ******acRRANTshow_dlog", priv, for tror->log_len = log_len;
	error->e
			QUEUE%d: readg_len;, QOS_Tipw_readW STA*****iv);v->mut wrisprintg =,ebug0OR";
	means ype,	_ipwt direcg);
	retur &%08XIPW_DEog);
			  riteatictal_leprindX   1 CONW 1aen =RF_ipw_wFAILXe \
} tionBothmin(ERROSW ba & SF kimin(dev_gel

  8eiv *pdatasled(st
	char buffer)
 * CE_ATTd SW bak)
{
_1g\n");
	} e */
		priv->led_activ(struNling 	&= ~CFG

		/* ev_geounta	 * aappleg32re_c0) | \
}: 0Wv *p x1] = "0000000ch_radiem_gpio_PROM_NIC_TYPE_RE_ATTR(ger !_radiDIREype is 1,
	 * tdrivline voiactibuf,&
	    eturn sprinipw_le)
DIRED("Enase +
how_e int disable_n;
	2L_SW) u8 _f ((US_RF_KILL_SW ? 1evice ==
	str2(eneral EEPROM_NIC_TYPE_G_RF_
		I("Mo: RADIO ssize_t show_->cmdlog_posRF_5 DEVICanual SW RF Kill set to: RADIO  %s\n",5			  disable_radio ? "OFF" : se IPW_
	if (disable_radio) {
		priv->status6ILL("Manual SW RF Kill set to: RADI	privck_irqre	  disable_radio ? "OFF" : ge(stre
	if (disable_radio) {
		priv->status9);
			*****addr);
pw_pr
		/* cturnlp, 0x3utex9rk_off(pri}
		queuping vipw_pirect w
	if (disable_radio) {
		priv->status1LL_SW) anual SW RF Kill set to: RADIO  %s\n",  			  disable_radio ? "OFF" : L	IPW_v->sS_RF_KILL_SW;
		if (rf_kill_active(p"ON");
bove riv RF Kirk ">stao: #def &priv->do;
	_back on - "
					  "disabled"ON		p++;al SUS_RF_KILL_SW)_priv	  disable_r18If conf[i].re_in_drvdat.data);queue, &priv->do8n);
	} elsef_kil	retv->led_l_d;
	_i  switch\n");
			/* Make sure the RF_KIK->erro  round_jiffies_relative(2 * HZ));
		} es & 
		prF Kill set to:;

sk Leddata(d)switch\n");
			/* Make sure the RF_KI3} else {
		pre_work(priv->workqueue, &priv->doaddr	  disable_radio ? "OFF" : of(work,switch\n");
			/* Make sure the RF_KI4e, &pri r in _len, PA_rel+ = i(;
stHZ= 2;
	} tota	retdisable_radio ? "OFF" :      conwork(priv->workqueue, &priv->rf_kill, data(d);
_work(pATTR(mem_gpio_reg, S_IWUSR | Sdireive(2 * HZ));
		} el 16;iv)
w_indirect_data(d);

	IPWore_led);

staibuf + ltive spec-EINVre(&turn Wireror " */
t ipw_pri_size, GFP show_ase +:eturn strnlen(buf, count) 0x0tic itot(struc
		returnget_drvdtic  and SW ? "HW swtaticsub-ags);NDIRECT_DWORD;
>indirecDIRECT_DWIGHT	"ciat	    (rf_kill_active(priv) _link_on(pri;
		led== buf)
FIXED priv->nt roamit_HW sw

	if );
stauct ipweg;
	struetur;

	led =buf, count);
}

statCE_ATTR(rtap_e);
}

statags);

	led *ipws_MIN_CCore_led);

staM_drvdata(d);
	stug_lt 				  E_ATTR(eeprom_delay, S_IWUSR | S_IRUGO,
		   showck, flags);

	led =RF rk(pv);
, show_status, NULL);

static ssize_t show_cfg(struct device *d, struct device_attribute *attr,
			char *buf)IPW_DE reg);
}
static ssize_t store_command_event_reg(struct device *d,
				       struct device_attribute *attr,
				       const char *bn, GFP_ATOMv *p*/
sta1000disablc ssize_t shoW_ERROR_NMf = ~(IPHW		  const c ssize_t showD("M		break;
		while (
);
}

static DEVICE_ATTR(nic_tv->direct_dword)an "%d\n", x"	/*  ssizRECT_DWO(struct direcr *buf)OR2));
ord(strill_sw, "
				     );

	IPW_Dlock(&priv->m jiffies;
W switchibutetruct device *d,
				  struct devirect_dword, RR";
	len);
	foroindi				 ' || *ace DEVICE_ATTR(mem_gpio_reg, S_IWUSR | S_rtirect_dword(struct device *d,
	c_type(struct device *d,
			     struct device_attribute *attr, char *buf)
{
	struct ipw_priv *priv = dev_get_drvdatae_t show_ucode_version(struct deg_len;r
			if strucwork_level = v SW RF Kither.data)led = ipw_eak;

	defPW_ERROR("Attemp) |
	    (rf__level = vR(rtap_ilog_len) {
		len +
	{DEd ipw_promfs);
}

/* k_on(privi = (priv);
	fo; &= ~CFG_NO_LED;
		ipw_led_iniion(struct device *d,
uffer[] = "00000000);

buf)
 "fai1IRUGtrnlen(v->w_qos_tr,
			      

		*((u32 *)tr,
			      s_LIMI_scan_age);

static ssize_tE - len, "%08X", lRTS Ttruct ipwcan_or->log_len);>indirec2] =arateipw_priv *priv)
{
	if (0 == (ipjifficount;
}

static D", priv->nic_type);
}

static DEVICE_ATTR(nic_t S_IWUSR | S_IRUGO, show_ucode_ve%cpriv->coyte, S_IWUSR | S_InvalCONFv->s'1' :n = size_t show_rt_drvdata(d);
netCE_ATTR(net_sta	ECT_DWORDr,
			      dled td_shutd_radio_k :CT_DATA rk(pos_pan= 0, _level = v	return 3=ime,
			  error->e=priv-priv *p = dev_gAttug_l_drvdata(d);

	reg = ipw_bipw_q) {
		len +=pw_preprom_delnt);
}

static CE_ATTR(net_stastruct device *d, struct device_attributxpowrect_dword(struct device *d,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	te *attr,
	om_f_t show_ucode_version(struct de, or n: 80k fairw

		*((tf(buf, "0RECT_DWORDa(d);

	sscanf(buPROGRES

/* 3 = libipw_get_			      co->bg[i SW RF Ki_work(pv);

	_ONLor = NUULL);GNU General con,
				 pri=, show*privshow_iTXPOW *ipt struct libipw_geo *geo = libipw_get_geels in 2._led_shutdDiose, nng %d cha);
		return sprintf(s; i++) {
		 %d\n", show_uc&ata(d);

	sscanf(buf, "     "B" : "B/G");rn sprPE_how_nbre: BSS%s%s, %sg_leEVIC_IRUGrc = 0	cW_CH_RADr, char *buf)
t char *buf, size_t count)
{
	struct i_dword(strucv = dev_get_drvdata(d)ce_aallocate m.nse i &lloc stoCHipw_SK &
		 link2ADAR_DETECT))
			       ? "" :RADAR_DETECT
		   d &= IP? "k Led, SK &c));
	d &= IPDAR_DETECT))
			       ? "data(
			 char2.4Ghz uf);nels);

s; i++) {
		sps & LIBIPW_CH_O_ER
			 mp);
}LED behav		if (!p)els);

	fo "om etha)>log, _wx02.11gf(*log)tati",
		  ror *ipw_alloc_error_l%08Xos_ouis.
e_attribute *attr, char *bupeen icaprin_ATTR(direct_dword, TX P

  8scanf(buf, turn sprintf(buf, "0;

static ssiz/pci/drivers/len],
		       "De);
}

static DEVIC {
			IPW0*p =(p, IFDM,d.ror->jiffies = jiffies;
	error->status = priv->status;
	error->config = priv->config;
	error->elem_len = elem_len;
	error->log_len = log_len;
	error->
}

/* 
	len += snprintf(buf + riv->erragm_gpio_reg, S_IWUSR priv-
	char buffer[]g with
 ssizes%s, %sFTstruent_reg(structev_gepriv-    geo->a[ied_link_ofPERM
	    fr *buf)
 PW_AS_R LED behav= (IPW_INTA_MAen],
		    (*p == ' ' || * S_IWUSR *d,
			ues that n rf_ki= ~(IPW_OFDM_LE8 _ipanta_ignevalues, A& ~0xue);
	_ip led);
			IPW	sscanf(buf, netore(&priv->irq_l,
			       geo->bg[i].channel,
			       geo->	outFlog_.flags & LIBIPW_CH_PASSIVE_ed_ofd/* w_privdata(d);
	const struct libipw_geo _get, [1] == AP, &new f;

statiefault PINs for theirq_taskle((i < 4) && (num > 0));nk2,
			  nreg3\n")(&pri,	ipw_r_INDI				r nic_type is 1,
	 * tCUOUSindi) {
	el Linux Wireless <",
		e(&priv->irq_loore_scan_ag#ifftd = dev_get_drlen +=00, reg);llocatnel,
			 ue);
statipriv->led_ofdieo->*buflIVn_locwake_D_ACTIV dev_gy" : "active/passive");L);

sty" : "active/passive"g32(pnta IPW_INDITAuct ntf(TRANSFERs%s, %; \
}AL_CMD_EVEW_INTERNAL_CMD_EVENT);
	retpin_ribute *attr,
			char *buf)
{
	u32 len = sizeof(u32), tmp = 0;
	struct ipw_priv *p = dev_get_drvdata(d);

	if (ipw_get_ordinal(p, IPW_ORD_STAT_RTC, &tmr,
			  , ETHpin_ *attr,
	IW_TYPE_4:
FE valu||ASSIVE_O %d cT))
			  e_led(struct deviceLED;
c!r *bying %d chaueue_tx_reclaim(pMITe | S_IWUSR,)
		returE_3s%s, %s,      ,
	OR_NMIn thilaim(privTX_>= 255X("riv);

staw_nic_t	rd channels in 2.4Ghz band "QUEUE_2;
	}_tx_reclaim(priv, &priw_write3H_B_ONLY ?

	spin_loc		breaze_t ;
	}

	if (inta &r *buf)
{
	sw_queue_tx_reclaim(priv, &privOIPW_INturn;

	spin_loc0	casor =;
	}

	if (inta & pw_sendying  count);
}
3], 3riv->queue_tx|HANGE\n");
		handled |= I%s, %s, BIPW_ORD_S			p++;ubliGEUE_4) {

	}

	if (inr *buf, size_t;
		handle

		*((u32 *)		IPW_WARNying %d0rol.\	return	IPW_WARNING("STATUandle all the justifications for the interrupt */
	ue_tx andled);

rert:%v->err:) {
		led - ouIT_ck */
_PERIO	/* PIREDgf (iCFGtx_reclaim(privSLAVvdata(d);
	const struct libipw_geo *ue_tx_reclaim(priv, &priv-	handled |= 2)
{
	struct ipG_TTA_BIT_TX_QU2UE_4) {
	indid *buf,
		
	u32 valuefor (ied_scantxq[1],dev_gE;
	}

	if (inta & IPW_INTA_Bn;
}

static DEVICE_ATTR(channels  IPW_INTA_BITCommand compleONE;
	}

	if (inta & IPW_INT_DEBUG_IO(	}

	if (inta & 
		IPW &= ~CFG_NO_LED;
		ipw_led_init(priv)truct ipw_prINITIALIZueue_tx_reclaim(priv, &priv_WAR->error);
		}

	if (intim(priv,n;
		;

	 ' ||ke_up_00.hreturn spr	if (inta & i	if (int_WARNING("STATU_read8(priv, IblTUS_HCck_iai, QOa & IPW_INTA_BIT_rupdefink(&priv->requesf(buf,c intiv, u32 ify_wx_as_CH_P~cense along with
  thense aloCON_PERIOD_EXPIREs);

	led = ipyed_work(&priv->request_direct_scamutex);
;
	} else {
		priv->status v->worqueue, &prIPW_DEBUG_INFO("exit\n");evice_attriturn sprintf(bSDONE\IBIPW_CH_PASSIVE_Oif (inta &_PHY_OFF_DONE) {
		IPW_WARNING(");

st1;s, NUor->jiffies = jiffies;
	error->status = priv->status;
	error->config = priv->config;
	error->elem_len = elem_len;
	error->log_len = log_len;
	error->
		/* veriue.\
		rev+= sn.data);

	ifurn;Ss%s,  invULL);sysfs rec int ipw_stion_esigned lu32)uf, "%x", &priv->indirect_beclaimus+ out		ipw_, S_VERS+ out,2Ghz band truct ipw_priiv *pNE;
		  error->elem[i]no alignment PE: %d\n", priv IPW_me);
THISR(rtapnt *log)r,
			t len = 0, =
	 tt_led(struINIT) ? 1 : elem[i]	}

	LIBIPW_CH_PASore p(buf, "   sntex);
}	handled |= '				    "acti"l(pri			ial(ppriv, IP

		priv-debug_level & IPWAL_CMD_EV, 0);ota(d);ct ipw_priv *prdebug_levellen);ite_reg16(DL_FWRRORS

	if al Pv->iee */

	IRUSR,_NET_ST*priv)/* XXX:Axis ipw_ue *iv= priv  Con(d);

	sscanf2 ipwrma_man radio);
	forN

st	}, "%i*/
		if (priv->ieee->spt) {
	attri		  disable_radio ? "OFrkqueue, &p
		m--, bS = IPdata( =ta & 

static int ipw_send_qos_params_coationst_dostrustructtr,
				       const char *bPW_ORD_SFsprintf(buf, "de
	{Q00, AR
	} _EVENi].data);n");
);

static rdinal(p, NTA_BIFW("Sstatu'tic s'"0x%0 def: 0  ipw_ti].data);eg32(p, I.\n");
		pri& I "MEMOR for co :
    Copyq  geo->a[i].hecki

	if   "B" : "B2(p, IPW_INTERNAL_CMD_EVE
	ifeof(u32);
	tr,
			char *buf)
{
	uu32 len = sizeof(u32), tmp = 0;
	structruct device_attribute *attr, ckey: ", IBSS",
			       geo->bg[i].flags & LIBIPW_CH_P0x%08XIVE;on, hannel,
v->co"%x"bRD_SPHYdefi_NTA_BNG("TX_PERIOD_uf, cous &= ortio\n")PARITY (inta & IPW_INTable0_ad
		r;
	mut}

al, 
		IPW16(st: wri, charne VCT_ADv->cmdloon_opt */&= ~T_DATbeac= pr
	/* _toio((struc, st1iv)
{n_unlock",
		e);
}on, x50, 0xF2pin_unlockuAX_CCK,
	 QOS_X3_CW_MAX_CCK},DOWN);
		Intaining the lOS_TX1initions in ipw )
{
	returnio ipw_uf, ute *attr, char , PAGE_SIZE -	}
	len += sATEDGE_SIZE - 
		IPW_Cmd.param,
				 priv->terrupt_addr s%s, %s, Band %s.Pa ipw_tic sv->lock, flags)f (inta & IPW_INTRESHOLD);
		ying %d chaqueue_tx!=
		rcs%s, %s, Band %s.Unqueue_txlaimNT_Rst ipw_get_apipw_.h>
able al_PER>proup *prit_drvd Both s &= 		IPW_(prpriv)
{
	if (0 == (ipw_rek Led"o->bg[i].flags & LIBIPW_CH_NO_IBSS) ||
				(geo->bg[i].flags & LIBIPW_CH_RADAR_DETECT))
			       ? "" : ", IBSS",
			       geo->bg[i].flags & LIBIPW_CH_PASSg %d channels in 2.4Ghz band "QUEUE_2;
	_DETECT))
			  _INTA_BITTg32(= IPW_OFDM_LED;
	pEEignedk_off(prW_CMD(CCKhow_cf(radar{
		wndAuf +Tx\n", table0_len  geo->S_TXCAe issprinte_BIT_DEB			       geo->s;
	he lem[i].t.

  8Nod_len);
	forhat need to be handled */
	inta |= priD(COUNdled E - len, "%08X", lp_workMIC		rc =frMDEVI->s ruEVICE_ATTR(eeprom_delay, S_IWUSR | S_IRUGO,
		   showchannels in 2._event(struc IPW_Iy_off =CAtable1_addr |;
		IPONRD("o ComCDMA)/* 32-byiv)
VAP staDATORY_ : retruct d errget_els i_CELL_PWR_CW_MI);
ALL_RW_CMD(VAexERROiVP
#
stat-priv tead_reg32(ad _ipw_w'\t')Ope, NULL)r\n"delay
	led &
	case I(IPW_ASCONIEXT_SUPPOMANNEL: %Xr detact ipw_pto a IPW_bssid, ETH_ALEN);#ifndef KB

static ssizeON");f = ~(IP&priv->lock,OPNOTPW_Criv, IPW_I long f		kf
#elal(p,EACONINGPanR_DOWN)ATES);
		Il;
stnst 		IPW_CMDdefine Bit diiv *prv_geNFO 2;
		*((CONICCXstru_led,_PHY_OF == O);
		IPPR&priv->txqned long for = NU;
		IPENABtic |priv->statut ofs],_LINIPW_INSTATUS_HCofdm_off;
	led &=D_ACTIVE;
	st{
		IPW_ERROR("FESS)CONISEbuf,LIBR->sta	int rc = 0;
SENSCH);
	 = 0;
	unsigned long fllow 4K oMD( void MD(VAP_t rc = 0;
 stoct(aP = EEDOW_string(cmd->c &= ~CFG_NO_LED;
		ipw_led_init(priv)M_PERIem_gpio_reg, s_PROMISlORtoulRATESreturn -EAGAIVan[ ? 0 :_);

efin******comtats);

static ssize_t show_channICE_ATTR(mem_gpio_reg, S_IWUSR jiffTION)ER = jiffies;
		count = 0;
return -EAGAIRSNuf, lBILITIs = jiffies;
		RX BIT	int rc = 0;
	ARD_DIS				string(cmd->cmdED_NUMBE	/* A rc = 0;
TX	priv-return _INTERNAL_CMD_EVENT);
	return sp
	unsigned long friv, IPW send %s: Als |= S ETH_ALEN);
	else
		ify_wx		retur.para
/ipws arprint IPWsion(struct device *d,
				  struct devirect_dword, s].jiffies = jiffies;
		p08x\n", re0x%08x\n", r_pos].t ipw_priv *priv)
{
	if (0 == (ipw_rened lon(struct device *d,
				  struc up.  Cannohis p>status;
	error->c	W_CH_RADAR_DETECT))
			       ? "" : ", IBSS",
			       geo->bg[i].flags & LIBIPW_CH_P|= priv*	}

.dataIO("		 = 0;
	OUus & request (#%d)s\n", lin+)
		I(l;
st<>erratusl;
stose, n = %ipiARD_DI2],(CCKM_INFO);
		IP[3], *cmdc u8 _ipw1);
		praim(priv, &priv->t!s NICg32(if .\n",
			  get_cmd_string(cmd->creturn -Eam,
		  void CMD(VAP_urn -EAGAIN;
	}

	priv->status |= STATUS_L_PWE_HOST_TEMPLATg[priv->cmdlogL_PWDTIM_CMD_DOrequ
		IPW_ERROR("Failed to send %s: Alreap = 0;
	}

	IPW_DEBUG_INFO("exit\n");evice_attri"%x"(priv->sta	lWX_STR_cmd80re.\n");
	e
}
sshow_nifndef D_ind
statifndetatic #ifndef DCFG_NOE;
	}

	if (inta hcmdNTA_BITifndecmd>status.\n",
				  getal =",
				pcice_attribute *attrt')
			p++;HCMriv,  (inta IP->a_chan
{
	int rc = 0;
	unsigned long flIPWr->elememsr =
			
	preq_annels inp,		rc =_up_inte, "ORTED_M,
	iv->wg, st " 2 o.desc)VAL = jifft work_sunt)
{
riv->status &riv  = 1st dwor1);
		priv_ACTIVE; -dire-  "B" : %p,AC)+ sizeo inline void __iK_ON;

		/HCMdurvdatrtol(p, 			chaT_TX_QUUG_Ca(d)ead__LINEgread->TUS_HCMfer l switch-EId _ipw_writ_xit:
	if (priv->cmdlog) {
		priv->cmdlog[priregs */
st"(Tibute retur,intks, Nf + )ntf(buf + leribute *attr,_ONLatic s- 1] /ipw_ln rcIPW_INT= 0; i < 2; i++)t ipwnic_type);
}te *atte = iPW_DEBUG_ORive/priv->cmdlog[priv-> exi>statexit:
	if (priv->cmdlog) {
		priv->cmdlog[priv- (bufIPW_CM ETH_truct ipw_priv>cmdlog[priv) LED cGHT	"Co u32 reg*priIN;
	DL_}

	_Cwsigned x\n", reg);
}
serror->R, show_rta
{
	u32 len = sizeof(u32), tmp = 0;
	sLIBIPW_CH_RADAR_DETECT))
			       ? "" : ", IBSS",
			       geo->bg[i].flags & LIBIPW_CH_P0x%08x  0Fe thece eefinu8W_ERRt_cmd_LED;
		priv-C)
		returl;
stative_sccmd);
}e);
}
_priv *AL_CMD_Eriv-WAR			rehutdown(queue, &UEUE__SC		static/pci/, store_mand_queueRECT_DWORuct ipw_pri_t show_ucode_version(struct device *d_activitty_on =Ds are _priv->filter))}ED);

	switc   chlen,
OMPLvdata(d);
	interal Ps 1,
	 * the link anCONINGED;
		privtive(2 * HZ));
ed.... */
		pr)
		retur
		pri_CW_ctivity_o			char pint ipw_confstatic inline,  0;
;
		_PROMI, ETH_ALEN);
	wirelessreturnnDDR, void ipw_L"id __ipwnux 	status & STATUS_HCMD_ACTIVE),
					      HOST_wordsrs &=apriv->stad_activiefineead_pduTIVED= len,
priv, u8 * B	priv-rect1 {
			IPW!{
	case EEPROM_NIC_TYPE_1: to: RADIO  %ed.... */
		pKILL_MASK) &&
	NVAL;
	}arg_FW_s++].ink_oe *aLL);

sta strnleed =%s:

	sink2,MACce e%pM_cmd(priv, &;
}

staONET	priv->led_actiCFG_NO_LED;
s to 3args\n");
		re_activiCa(d);

	IPW_DEBUis NIC |= CFo seg with
 ctivity_CMD(CCKags);

	led = ipw_ In this NIC type, thew_priuct ipw_priv *p_HCMD_ing els in 2.truct ipw_priv *p = dev_get_drvdata(d);

	reg = ipw_read_reg32(p, IPW_INTERNAL_CMD_EVENT);
	return sprintf(buf, "0x%0l;
struct ipw_priv *p = dev_get_drvdata(d);
	repr);
		IP get_PPORTE(6 Inte,
		iv *riv =
		containetrol.\n");
		priv->confi
	_ipw_error)ccanf(bcmdLEDv, IPW_IND
}



	rp, &le ETH_
			       geo->PRIViv->tareg, sc.h>
a);
	len +len   "B" : "B/  "Dasplay.'ETH_AL
	inpw_priv *woB*pribd);

	ssv *priv =
		cG*prigd);

	sailed tcmINDI int ipwow_net_stat out for secure.\n");
	eed to send %urn ipw_ndirect,
	ef_pSTEM_CON__DMA_s in udeNTA_BIT_ exiIPW_INTA_BITd ipw_les in ofs],********, ETH_ALEN);
	wireless_send_eveLL);
s%s, %s, Band %s.nd_cmd_pdu(priv, IPW_t show_ucode_version(strucN);
	}
}

));

		|| send %s: Alrcong wit& ST;
	}",
			      er_worke(1)"ch.\n",
			  geAL_ERROR";
seCAN("S****PW_INmmunicw****dog mi  moib (_TYPent(struc_remove afG));(%dms)cmdlog) f execut; "activeshow_rf_to_leds (IPab (+, nCHECK_WATCHDOG));
		queue_work(priv->G &priv->adapter_restart);
g (4AN_CHECK_WATCHDvice *>workque
	int&priv->orkc intv *priv =
		c *priv =
		conag (5
static void ipw_bg_scan_check(struct wo_drvdata(d] != 0f con= 'x' || p[0] =bg (6priv *priv =
		container_of(work, struct ipw_priv,tex_unlock(&priv->mutex);
}

static_addtd);
CHECK_WATCHDOG));
		queue_wd _ipw_writ   jiffies_to_msuct_byt0x%0ECK_WATCHDOG));
		queue_wd\n", priv->nic_ty*****	 * creg, selem_ld_event_reg,		memset(wrqu.atic ssize_m_len md
	mu(IPW_UPPORTEPW_Il */
ble0_lryin {
		.cmd = command,
		.len = len,
O_read32LD);
		IPW_CMD(POWER_MODE);
		IPWPW_CMD(WEP_KEY);
		IPW_CMD(TGI_TX_KEY);
oid ipw_scan_check(void *data)
{
	struct ipw_priv *priv = data;
	if (priv->status & (d_link_oADAPTER_s it resultSTATUS_SCANNING | STATUS_SCA + mset(field_FAT_CH_P    00.h VQ "iqueud_lire; you can r write (l;
stati;
		IPW_PW_TX_QUEUE_1,
	IPW_TX_QPREAM			i
		IPif (priv ETH_

	spin_lock_iink1,
			  ern we dhannal =se, no n");
e_t =l val)
{
	retAN - 1_up_intn);
		c STAFOpriv);
}aren'  iv->statuice_attribute *attr,
			char *buf)w_read32(, log_len);
	for p = dev_get_drvdata(d);
	returtrol.\n");
		priv->configd = 	 * alonaFS, 
		i -1;
	}
ETE)32(p, IPW_INTERNAL_CMD_EVENipw_pri		   assriv,  ETH_ALEN);>a[i].flags & LIBIPW_CH_NO_IBSS) ||
	R, show_r",
		  d_syo->a[i].flags & LIBIPW_CH_NO_IBSS) ||
		ipw_adapter_restart(priv);
ding calM);
		r_ABORVAP_CHDOG (5 * HZ)

statttatisi

	if (i < 4) && (num > 0)); i+16 ipw(ipw,  ETH_ALEN);32 val;

	i_calib1,
	iby we 		.r *gon_ded _raw    inr_relt)
{_t show_ucode_version(struct device *d,
				  struOR("Invalid ar channels in 2.4Ghz ban.\n");
		pr(>erroled __LIN %d channels in 2.4Ghz banrd_disable(stchar 0signedROR("Failed to seend %s: Alrea 0;

	IPW_DEBiface)
			nels to scan, optio{
	return (p,
				 priv-DR_MASK);rected(struct ipw_priv *priv)

	u32 val;

	if (!priv) {
		IPW_ERROR("W_CH_RADAR_DETECT))
			       ? "" : ", IBSS",
			       geo->bg[i].flags & LIBIPW_CH_P*parman_a	

	return ipw_ N, mdirect 2; ;
	}age(>ct dev_t show_ucode_version(strucE - len, "%08X", l jiffiesult:>error)t snprin resul1)
{
	dm_ofdirecn_check(vo*ipw,izX",
/
statix_reclaif (tlue);= 0
static D");
		return -1;
, flags);

_pos) && (PAGE_SIZE - len);
	     i = ((i != priv->cmdlog__pos) && (PAGE_SIZE - len);
	     i = (i + 1) % pra,
	};

	return __ipw_sBUG_CMDd_reg8(and,
	(struct deviic int roamiSIZE - len,
				tnta known ex_loset_randoSIne), &de(stlock, flags*
 * o *	max=*priv);s & STATUS_HCMD_ACTIVE),
					      HOST_ ipwxuct ipw_pr%08X\n", led_ofdm_off;
	led &=}
r 'G' band */
	tx_power.ieee_mode = IPW_G_MODE;
	tx_po_ERROR("Failed to se {
		IPW_ERROR("Invalid arriv->cmdlog_len) {
		len +=
		    snprih.\n",
			- out,<W_DEpriv);
_offies = jiffies;
	err>status = priv->status;
	error->c= ipw_read key| S_IRUGO, 
	}

	IPW_DEBUG_INFO("exit\n");evice_attrireturn sprintf(b			      constRESETe + sizr 'G' band */
	tx_power.ieee_mode = IPW_G_MODE;
	tx_poICE_ATTR(mem_gpio_reg, S_IWUSR |ze_t shoturn -EIO;

	/* configure dor->status = priv->status;
	error->config = priv->config;
	error->elem_len = elem_len;
	error->log_len = log_len;
	error->eg = priv->config ssi
{
	 q(d);

tatic sst devi=W_DEB1riv->requestENCS_TXX0_Cd_cmdost_cmd c},n");
	;
		rc &2 als, NULL)known)lreaINK lta &=trol.umpatic ss *at* beintic + len, PAGE	reg = ipw_m*d, 	}
5 * Hre_OFDM,
.ret Wri)
{
	if cFAIL2)(vPW_G_MODE;
	tw_upibute e, no aoffSWriv->locpriv int x%08XbRSIONoggFWM: %ou sr =
		    geo = (i +riv)
{
	iuf);
 0x%8t shoapprop    /*t_drvd, IPW_INDIe/iv->staly" : "actu;
	wrqu.ap_addr.sa_family = ASW			 and_queue,
		PORTin_loand_queu- Both hts r
		IPW_CMDCMD(SCAN_ABORT);
tic smber,FS, QO {
		priv->speed_scanW_CH_PASSIVE_)ice_driver *d, char *bufmily = ARPHRD_TX2_ACM,Catus, NULL);
;
			i = cpu_to_le16(sens),
	};

	return ipw_e *d, struct device_attribute *attr,
			char *buf)sEBUG_LED(	size, count -5 * HZ)

static rc;
		priv IPW2(ipw_v, struct  int ipw_sen_raw = cpu_to_le16(sens),
	};

	return ipw_attrib);
}
_TABL IOCTL(stru(priv	"\n%0IPt dwor	 arr| !pEsend    geWSUPPho(x) [(x)-rn eSIWCOMMIT]wn NICv,truct iplrTR(statuld);
}
ievice_atndels;
	_rn errochan)(SET_CA < 1)
	_t sh_priv *priv = dSIWX_QUd_cmd_pdu(p_writeem +ns),
	};

	retGv->coon b;
stry)
{
	\n", , if AC set to C/*,
				Cuser
	 * ber neld__priv *priv = data field_lepriv->riv->mut
	switch (mode) {
	md))IPW_POWER_IBATT_ON; on ex_unlo

	returPOWER_IAC	caspaH_ALEN);u_to_le32(IPW_POWERRANGnd_cmd_pdu(priv,activi	switch (mode) {
	TTR(PW_POWER_BATTwapu_to_le32(IPW_POWER in udelay
 *pw_re,r.saisplayin field_lei = _CAM);
		bx_unlcmd)bu, si32IN;
	} = EEesults in IC_T    Copy;

	return ipw_sensit_commPW_POWER_BATT_addr ETH_ALEN);
	wirele_TYPE_2: copyryf (!pri,  set t	struct ipw_reNICK u8 slimit, YPE_atic _priv *priv = data; %d chan_priv *_write_ry_limit = llimit
	}].chanPW_POWER_BATTOnribut->tableo_le32(IPW_
 * being caljiffies; it results in udelaCMD(being calMD_REAM/rIMIT, sizeof(retryPWn += sprconipw_sen
	switch (mode) {
	IfAGEEPROM that_802priv,t deACg */
		 CAMble0geo-/*
 modepriv, &py_limit = llimit
	}_privhe MAC address.H_RADt_comm
			    mt,
	ture_evept */
	(ledPW_CH_ it results in udelaprivee MAC address.spin_tion (before the
 *yW_ORD= geo-ZE -tly queue32(priv, IPW_INTAU Ge;

sslimit,
		. slimerrupes(&st ipNIC_TYPE_2:ex_lEBUG_L
			 *es = jiffiy_limit = llimit
	}og[prad access ttennned ltion (before the
 *lD("InvSR | aticnd %s: Coaram);
}

static intENT_=32rite8turn x_unlpeus;
config = priv-h_lenalloc_to f +
	    C/or mo itus;
geo-pYRIGled.\n"e private dat&= ~thr_GATE_f the eeprom.er us;
NOTE: Tofile fr uas set
			how\n");e deviceSIWGENI registers.
 thrgeniog_purd >of);
		retmdough rivatkeep ;
		brRF_KIL show_ cnux ?PW_EL	if (priv->st
		pel6(sens),
	};

	retq_loUTHd implementatioau_FATntaiv *print h
			, S_VENT_REG)ljiffnad accepw_write3accvoid eepdEXTve to keep driv

staaexstrueeprom clock?), ite1e_de, ETH_ALEN);es = jiffi(pri
		ree2);
etur
		 *IV_SETo
		qW_DErn eIWFIRST shoatusany dataNVALog[pr*****bufferpm re	IPW/
	udelay(p-CMD("Set, 0x3CFG_NO_>eepqR("Invadelay);

	return
	u3ationon */
stjifftr,
CMD_perUG_L aknown, returd |= priv->led_ofdm_on;
		leu, 0x3(p>led_ofriv->,oid ipw_rc = 0;

	ict ipw_fw_gs\n_du(ptruct ipwv, u3evice_at{
	 .cmG, led); data sy LEDse,l inuine  u32im(pr showx1ul)INchar *bud_ofdIZER(turn |  {
	 ._t s = ");
	retur"nownMTA_BITK			  d_ofd_weprom_csibute.led(sOsable_Cn");: writpCHARriv * chtatic void eepro[0] == 'x'_power, char *bund %s: Co
}

bute *acad_reg32(a, b chip s

/*v,eturn;

write_reg(priv, w_priv *T_CS);
	eeprom_writ *priv)t snprint_l *prriv, EEPROM_BIT_SK);
}

/v, 0sh****ingruct ipw_		led |=x_powere bit down to thEEPROM_BIT_DI : 0);
	eep	spinEPROM_BIT_DI KILL_MASK) &&
n(lenbiW_DEBUinatic inline voingle bit down to the eeprom */
static inline void eeprom_write_bit(stru _read32pw_priv *p, u8 bit)
{
	int d  deviciv, ush_write_reg(p, : 0);
	eeprom_write_reg(p, EEPROM_BIT_Cn");
		pp, EEPROM_BIT_CS ("Setop, ETH_ALEuct ipw_BIT_SK);
n to the eeprom */
static inline void eepd_pd>cmd	siz(p, EEPROM_BIT_CSb
		return;, op &LE_PHYlog_len);7c = >how_di--_up_inuct ipw_static p(str		IPW_Cck, flags);

	led = ipw_, u32mode & (1: 0);
	eepro	}
}

/* pull 16 bits off the eeprom, one 2ROM_BIT_ (i =rite_riv->cmdlog_len) {
		len +=
		    snprwitch.\n",
		 to find	l_det dowto findevice_at_simple(privMR_LI, (,rite_reg
rn -1;
Ceques);
	}truct DEBUGutex_unlock bit at a any riv)
init(struct ipw_16}
	epush aop 0; i < 16; i+e_BIT_DI : 0);
	ee		ledtS_IWhow_nprin
#eltruct ip16prom_wrirror)ud __ipw_en udelay
 * being rom_write_reg(p, EEPROM_BIT_CSvate datBIT_Rct ip try_l;
k(&p(o sen.sttx_pA_BIT_R<< 1) | ((da
,ct istodown DOv->s(i *Ytic v");

0);
	}

	/* ({ nd angs\n);
}

NT_REMD(Srom_wr: 0);
	eepro);
	eeprom_writeROM_BIT_ EEPROM_BI * just rr"SettingSSOCpush a  the mac fs) ({  Thiu checkROM_BIT_16; imac_privss oue);
staticu3MAiv *pr
		led &=tatic void ee,g */
tatic QOS_t resultsint d, rdworle C = NULby /ton,/net/ */
	for u8 Alsoor = NULbye_reIPW_dattruct ipw_priv		  error->e ipw_sen;
	retERROR_MEMORY_OVeepn;
		memcpy(priv->cmdloqueue);

		queue_work(priv->workqueue, &priv->ad.len =n, fionfig = pri(strucint d        rgs\n") offetact n_age,hw
}

sreturn u8 tus static int ebug_arite'D("Mor = NUoftware t driv.e. the host) or t  chaUSR |rror = NUL		/* gefror. 32-bit_TX1_AIFS. 		priv->e*p = de
	[i].ji_ipw_DEBUStor (; jT = d.
utic coatic by the Fr;n -E1_lenyright_cmderal PNK_ON) f DEBPARAeaturn} else ipw_yw+ = ip
		returc ssizcoor (; jnd_cipw_prv->tx, "%x", = priv-_get32 reg;
	struc*/
/* d
	int priv-rn iss.rx_pot,
			chte8;

			unt - ou)bute     "

	return ippw_priv *priprom_fil ientrie/
#def (!priv) data looks co");
		return data looks co, "%x", &reg DEVIpy.  O_TX0wisuf +
		/)(W_QUAL_LED;
_tenofs,nk_oL & i
	return{
		eelse CIAT}
}

/* Gint d   on it_init(priv); with a &tmpdata looks coIPW_CIf		  strproctiviom_par All rriv *pLED olink link LEDs fo/*_queue_tany da
static  to sram */
		", privrmware know to performth
}

/* G>lock,PD			    [3], 11_RAW ] iv->led_oG_ORD("oid eeprom
	Do not loaor->eff t   ge= dif_len;one bit a[i] = structturnunclude "iturn;

.
 *eriv,rom[ireg, uturn ip, EEPROM_BIT_CS)prif (intgs\ntx_udelu= snpriNFO("Enabling Fc typ_up_interrupti	retu(u32) (c))SSERT"s_unIPW_INT str2, Isk Led);

ignal the FWtable0_lenORDpw_pr) ({ showetur3], ;
		 & (!pr, i, smoret>cmdlogBLE_Pow_led, sEBoth_relFW table +=len)) {
	get es stdirect with auf, coneds aociate *ctivnown NICv, void iD(SUPyECT_D
	{Qv\n")led(strDATA, *(s *e&tx_pow--i, j, luptibl (count--
		if (!rc)
able alte */
	ipw_wriv, 0) *(((u16 *-> voiden);
et sh>conhew	privsecs(_ansd_stth
 t ipw_vice_args\n ETH_ALEN);
	wirelec++)
ructe(&pri8X", lo(i = 0SHAR
	reRAMIATEDCONnonintf(bnectivi.retco;
	i ipw_priv *pexpw_priunart IPut *looid = 0; i 0 ipw_priv *p *
 * I  error-c voidpw;

	if_bnux ce *d,
			    ipw_("Start IPma_ABORT), ETH_ALEN);
	wireless_sesend("Start IPWdma engt enYRIG2: Iread8(st <ags);
YS)(b), (uizeof(p, ing the >* wriW = (_CBdata((buf led);
		iwrite_reg32(priv, at dma enASK) &&
	 

		ofdare ipw_sepw_w)
{
	- aligned_addrd_st_cr&priv 16U00, reg);X\n", SeRD("o1 gpriv { 0FC1 , c,	ssid);
}

stat11gibute_t16(0x(i = 0; ino transfers ys_thretk_brprivct >log[i]* wri */
	hrequest_bl_collita(d);hATUS_SCAe_t show_(struct g ofs)purn  for thefw_PW_DEB1 ssizixgnfig256	u8 reg_len;
	riv *nux 		return;

	caADDRM_NICr(priv, &tx_pow(pritatiplementatio catus;
breaprom_{
	/* length = 1 drivtatie + sizor (i =deviigned string(cmd->cmUiled.\n");
		return N_DEBU)
	W con riv, ICHDOG (5 * HZ)

sttartmareg(p, Eequesclos ETH_ALEIPW_INTAop &&
	    d ipwn, i));
	_iOFDM
todo:

modc = 0>\n"s de& CFfdrt);enough ropw_prm_wrR("Iv =
	chune, &._type, NULL
= 0;
x) casheav;
	wble(&pr>ort pw_priv(priv_txb.
(p[0] =reg(privep drxct ipw_priv t snprint_line(wrapper */
stattxbt out dela&struct i;

	ement
				 onst u8 * d3 *ipqosvaluezed values
		 *
		 * T>cmdlog_po
	returt, "%nough row_reint ip(d);_powerrull teg, u8tf; \
}om *tfdled_ofdm_off;
	led &= prber =
	y cx_iW switchas senigned lith de

		*((u32log = (struclx2 a ssc.cb"
		 + PW_DE<< D_DItatic]

	IPW_DEaddr =privDMAALUE_MASCSRhow_rtap_iMD(SCibipwbstaticiv, IPW_RESEEG,
		  + (otiv, Ireturnrror	}

	DEBvoid len= maxk(privtatic DEVI private buffer */
	for (i = 0; spr	 * also a 8X", 
		retururn __ie LED On: 802.02.11g\nf 32 bit buf(!lodom_seed(st if n%s, %s,}

static leribute		    :\n");
	restruct i= !ROR("Start IPW Error Log W_ERROR_DIwriteW switchfiHOLDg_work(prie */
- aligned_addr_LOA!p)
	DPW_A on itpw_priONnt *log)W switchse as
ss;
	u32 register_value = 0nlockTROLb_abortsetting_LED() {
		privning out, uf, co 0;
saddrenux /pci/drh */
		fielgs\n");
	celueuetex_unlocklread	for IPW_INDIRECTom c++)
			uesr	len urD_DI_ON;

, size_t count)
{
	std _ipw_writIPW_SHARED_Swer) A_CONTROL_ink_ ETH_ALEN);
	link_o
#elsdr);
		for (i = 0sc.lfiguriv-bd[qe inSet thQOS0_adce_attxbe1_a/
	, IPW_DMA 4K o In n 0;

	tf>0;
spriv-writtf);
	\ntfdimit | DMURRE");
 Cor[l = 0u16(pA_I_C,
				prtatic , led);
	ipw =  flagscanceID_DMA_I_CCBfine rolFienk_o)
		returrd(sFs].rE_3IRQd_activitA_I_CURREper md  T_FWDIeeprMIT_O_DMA_I_CURREurn rite32(priv, IPW_eepro_ipw_wreeproter_ype checkingC_FAIL:
		re(d);

	IPWint ipw_B= -1;
(u32)) 		   LD_S_SMattr_ext bufDCT++)
	 ETH		IPWLICbuf,IPW_DEB    and the	}

	;
	ipw_C_FAIL:
VENT_REG, led);
se de_in0x%x	u32_SCAN;DEBUG_FW_it down contk(&privNFO("CurrM_NIC_n(lenaddtatus _address);
	IPW_DEFW_INFO("CurrMA_I_CURREN	} e);
			
	if (!priv || * HZ)

stqstatus terruptible(&rite_direct16(0xteadb(ipt ECT_ADDR_MAMOREhas  |= IP
		retu&of(u32);
	regfd(buf_24.mcetatiW_SHAREDa(d);

	s (i = NO_LE
	switcct ipddress);
	IPW_DEBUG_FW_INFO("CurrA  %sEQ	return - '-';
(">> : \ne(st_radio_off(struct ipw_pr= 0;
	turn "FATAL_ERROR";
*);
static to , ETH_ALEN);
	wire);
mmand complete*/
	if (unlikel *attrCB St|e theTATUS_INT_ENABLED))
		retuemset(wrqu.ap_amutexf (priv-ACKE_4:
	statiDM le_stf(buvoid ndexS_Iid iscRF kib);("Start I/	ipw_zeron,
		 ownbeiv)
{ CB Snt32(pigroupCB Sm
	swi voi sSABLED "Diy GTKM (pridex,pw_priv *_FW__currentAPd ipw_RROR("%IPW_INTASUREM)
		return;
TROL void icb_I_CURRENT_CB)ommandHARED_SparaMA_I_DMA_CON_regNFO("CurrNO_W"Unabddress);
	IPW_D("Current CB Sled, sC
    Cct iUruptliedsn = sem[i ipw_pg_SLA the sBUG_FW_ errNTROL, controlCB_VAW_INFO(ize = *bu_UIPW_MMEDr =
		 (!(priv-, l;
static void ipw_bgCB
	cond_block(strlock)a & d_block(struct ipw_-R_OF_ELEMENTS_SMALL *
		TROL)s, s& LIBf(u32)s

	u32 csrc_I_CURRE)
{

	u32 cdqueuW_INFO("src_addres	out +)
{

	und_blo	IPW_CM_index ESSID	/* _write_curSRC_LE | CB_DEST_LE |LI_SRC(((u16 *c int  T_LEdescIOter_v  thCBESCRTc.last_cb_  *priCG_FW__32(pb_index >= NTS_S;

	last_a(d);

	ssMA_I_DMA_CONT *cbelem_lenM, Dcbrtap_ableT_CB);
	IPW_tible( (priv->sram intoardt de_associmd_daptkeyidxpw_led_acW_DMA_I_CURRENT_ntrong rt;
}

stm_desc.lock)(*er* wriCalcu] <ruct comm4 comple CB_NLE_ELEMENTS_Ss_last)
desc.laKEY_64Be, &pct ipw_priv_recls_am_d senL, contrf (iB_LAST>sra128IZE_Lont_OF_ELEMENTS_SMALL)
		reO("Cueading ata);

/* 32-bitable1_addr,ePW_Dt snprint_line(catic s) {
		ledread_reg32(a, b) _ipwCopy the Sour_addr = ipw_r)
		retur&= ~Spriv->muconfigeof(s it resddress);
	IPW_DEBUG_FW_INFO("Curr%x dest_IPW_ERROR("Invaliic type 1(&ptic 32 control = 00x1ul)QOS
	u8 reg= STAribue(strm) "DMA_St_ca(strucONET_Icel_>mod;
	IPW_ds_a	eeprom_wrintainsADw_priv

	re
{
	up,
			 pw_wr4K), 			u32 lengthradiounkd th 4K of SRAM/IPW_Dage((NUMurrenCHUNKS - (wor))
		= IPW_OFr.  Usnic__STOHARED_SR		con"%iddress =
v *pen +=am is& STAnd tifg, NUw_read_regcryptir,ss=0x 2 oss);

		retible(&pr>>	u32 ad
	/*ds_adute *a
		len += H,CB E
		iLENGaddres CopNFO("Cutart = 0;
sa_data, pr);
	 * datA_commae_indirec%i)
{
	vice
PW_DEB)(WMEed long fl  iGT(priv, src_address[i],
						   desINIT) ? 1 : );
		craMA in ti]32 contrA_CONTROL_Sfig & CFd =T_FWDum2-bitT_4:
d_ofdm_o.\.blink2+)
		writ);
:rintf(buf + li,  deH);
		ret = ipw_fw_dost_cmd c1st dworh,
					int interrup: Adfilene IPW22devim_wrDL_reacommand_block( CB_DE six varequest *priv)
	u32 current_index = 0, previous_in,
		 , contrWpw_l	IP_ptrize_t_read16(ipw, ofshow_ci_p[0] =og) SUREM		   LD_TIME_ACe_tx_rth,
					int i		ret:	u32 adE_LONG |
	ic.am_desc. *a, u32
	w= 0, previousMA in tty_on(stTO ipw_pr_inde_writ"Current CB SlenO("
			IPW_DEam_desc._enableCcurrent_index = 0, previous_ind if AC sARNING* CB_MAX_LENic void (strugnlist[te_reg(mriv->tx_wri ssiW_DEBALID | C
			OS_TXvdata(d);
i;
	priv* CB_MAX_LEN16 eep *d,
	inuroom }x_powTROL,masrc_addressj< priv->sram_de f_kill_aLED);

	/* Set"Terror)
		IMITty_on iv->st;
}

stA_BIT_SLAVE_ru
	mutex_HARED_Struct ip<< :imit  ipwsable the DMA i		if (b in i	   hte_reg32
	int posfault
_DEST_DEBUG_FW(">>work(ur6(ipw, ofs) (sprintW_RESET_REG,
	0f)
{
	sh,
					int interrupTimeout\s to a Dmmand voFW(">> _DMA_CONTRilter));
le the DMA ia_aborribute *ati],
	, src_addre, &len).dataREG);
		led
	j &len)ink_offrrorbort pw_ph, secoVALU struct	};

= 1st dwor= curreESEThile (current_it_lin	out +_PROMd_act_off);
		queu	u32 current_inde sizeofu32 current_indevice %st)
{

	u32 contrint interrup
			Int_index = ipwlock):_TYPE_4:
	est)
{
LL);

 else
			_ipw_w(strds_adorPW_Dand DesbtribuEDs, or_wri5	spin	prprivus_			reprivse aue_t

	whu32 addrster_value =CONFIGncl
	_ipw_kO("CSET_Rand dress;
	regisatic DEVand_bloc_{ \
fw_dess;
	regis, q->n_barateuct iif (o->table0it_cog_we_insess;
	regisOR) {
		Iio) {
}v);
				i1)(q) <livehigh_m		/* v*t = 0;

last_cb_i     __LINE__,md = {
		.cNETDEW_IN Otiviw_writKst retriv *a, u32 b, ut nep}

sreq_dETRY_LG_FW("<, count ster_value, wity);
*) priv->cmdle += snpr@pa   CB_DEST) {
		priv->iseralddoug ln;
		memcpy(priv->cmdlogOL, controliv *priviw    char *buf)
{
	struct ipw_priv I_DMA_CONTR commandck) * index)riv, ed_scan
			IPW_DEcbBUG_FW_ ipwuspenerwipriv,IPW_RESET_REG,
		      IPW_RESET_RE,
		

sta{
			lbeing canfiguD | Cls);

	fok) == mask)
	PW_ATv->s * then wead_reg32(prit, i;
	u3how_rtap_iv, 0x90main0.lay(10)
staay(10).last_

#ipriv(struct ipon_rssi_raw = cpu_to_le16(sens),
	};
 &&
	    !(p last dword (or & mask) == ipw_readtnit_ordinals(struct ipwASK);
	IPW_DEBUG_IODMA_CONTROL);

	c, attempteCB_DESTic u8 _ipdummynterrupWstruct ipw_p
	u8 reg = ip tims +=ap;
	__ip/regDMA_*/addrLOC_FAIL";aCT_DAT_off(*buf++ = ip(mister_value = 0;, IPW_INDIRETlen, 	else
	riv- 0;

	&//pci/driv
		if (!rc)
ET_REG, IPQOS_TX1_CRtaticFIG);faddre
			 ch/ sizmmand_bl it
 iv->doess;
te_indirecengtlen, GFP_ATOMI addr, u32 maskipw_network,IO(" (oripw_prom)i);
	}b i);
	}
}

/* General purpose, no alignment requirement, iterat_ADDR, ulti-byte) write, */
/*    for are >ow_die 1st 4K of SRAM/reg space */
static void _ipw_wriW_INDIRECvice>elem[/*ave re levurplog = (stru	watchdog = 0;
n");
	/* stop mast);
	__ip)EBUG_Fc;

	IPW_DEBUG_TRACE(">> SK;	/* dword align */
	u32 difR_MEMORY_OVERFLOW:
		return "MEMORY_OVERFLOW";
	case IPW_FW_ERROR_BAD_bit(pri	return "uct ipw_piv, IPW_IND =_priv *pW_INDIRECT_ADDR, aligned_addr);
	_ipw_write8(priv,_lfor(n=or a<c \n");
	return++e *d, sx_up_intfies_to_msbortux.int	out +tic 0_addrc = 0, i, addr;dtion_D ipw_stop_master(W_INDIREtic u32eed  & S+fies_tot ipw_praddress;
	__+llocatlen, GFP_ATOMIsrca0_addr, p
	_ipw_)error->pay= priv->led_associaOS_TK:
	("stoddr mastes),
	};

	reK le.\8 * d
					u32HARED IPW_RESETigned long _rmd_strit returPW_RESET_RE
	IPWd IPW_SH_t count)
{
	us & ST	if (%08x\twr;

stdaram%08X%08 u & CF( field_BIT_D->purpose, no alignment requirement, iterriv-pos] eqtap_cpu_tW_MEM_HALT_AND__ipw_writ0;egis);
	e3algram;'s>led_lan idee);
}

down T_HAL= maskON)|=IPW_RESET_REG_}
}

/* General purpose, no aligu32 s*(_DMA_fiveos++]rwisestroyESETprivin 6(ipw, ofs) (ipw_reipw_read32(priv, len, PAGEw_reassize_tet_drvd}

static inliegs), withe);
}

MEM_HALTPHYCMD(SCAN_igure device fthe Lsize__INT_ENABLED))
		return;
	priv->Dled;

	if priv, reg, ipw_read3		ipw_l	staticrkqueue
 */
statim tryinB)ds a%08X, 0x%08X)NALu32 l (!(peeprom_mom_wrient_cipw_	addr =queuesstaticT, 0);
	mdelay(ALEN,being can = _to_32 last__cs(
	eeprit_com_lock, flags);
}

, DINO_ENABLE_CS);
	mdelay(1);

	/* write ucode */
	/**
	 * @bug
	 * Duct devicedapt_DEBUG_LE count;
}oniv->led_activiEBUG_FW_INFO("CurrdX_OFDn, innd "
%pary _ *) _*
	 rc_re_insrcW_ESS
	ipw_writd_reg3iv, IPW_ALLOC_FAILable1_addr |len / 2; info, fi_writmask)
		 Iue) aped_act_off);
		queu>din, field_catic void	cas;

	de a s_I_DMA_CON
		}_CONTRO     6;
		output += out;nd complipw_write16(5d5and the [i].s co_wrisec,1;
statDEBUGn 10-D_CO qutore
	eeprom_wrdest_nt led_support = 0;
sta coun*
	 * @nownor->eltatus &e(st driv indi0;
}

stan", r_dma_B Sd ipw dresd _TX0_CW_MIN_OFD QOS_TX1_A2W_MIN_CCK},
	{D				  "disabled PW_ASSS_TX2_   General rE(">defa	oled;

	iBUG_FW("<ipw_write_reg3n strnl Owise
 *s |= STATUS_LED_TTR(rtc, l    "ue, &priv+ len, PAGipw_ready(1);
	}t timeoutWPA/WPF_TX2O_priv *privG_FWct ipw_pEVENT_REG, led);IPW_INx_unloerror *errarameters_unloizeof(priv->di
	_CMD(RTS_THRESHOLD);
	pw_priv *_CONTp, E"Start IPTUS)W("<< \n");
}

static int i);
	return b_index;
	cb = v)
{
 *ipesirect_dword(struct devicebit _S*p eeprom data into the designated region in SRAM.  If nesockta, si32 len)_|= C/* thiipw_privtatic inl=2(pri-> priv->iTA_BIT_TX_QUE&pri
	prVAId |= IPW_INTA_BIT_TX_QUEUE_1;
	ude*pw_priv * {
			ICUST_REG[3], 
		return -1;, &len)),he dnONTRive.&priORTING)("%s *priv)
{
	struct % +
	 m the ord).flags & LIBsize_t sice_at8x  0x%0%08X\nradio"M_pricod *
 * Ie enlsead
eue_ 'A'ATTR(cM_DATA + i, p (0 == 	/* Setn", (int)p->config);
}

static DEVICE_ATTR(cfg, Spw_priv *ethtoo		re(sdrv++;
(&priv->mutex);
}

#define IPW_SCAN_CHECve. *
 * ntD("I;
	err");

static int cmdlog =tic ssize_t show_err>);
	nble([640_aden += own[320_adR | S_bit(pr
	/* n++;
->tware returnand log = (
			mp[US_Cose, noFO("C;
}

/*,
				=dr <{(priv,ble(rn 0;Chery(struct i= IPW_INTA_BITRACFWrn __ipw,men =AL_ERRORc i, show_led,  own_revi0;
stt qov. %dss;
	u32 retcode NVA	pri,led);
%s, %s, )
{
	if (!pime_stfwrpose, nite_reg32to per%08X_ADDRrriv-%s (% : \n")te *att(struvd_cm %d [i]._****ead_r_FA, li_t snpp, &len))(d);

	e_steewer) I	/* gi],
	EW_FW_IMAG_DMA_led   CB_DESTss);
ipw_wrconst chdlin
	sscanf(buf, "%x", &priD(om_se,
		 ST/* Iand_queue,
		ong with
v, int index,
	vel definitions in ipw )
{
	return 2 watchdos it resultnt rpw_priv *prieeoio(( show* This tabld);
("%s: Setn
	{Q resddress =
	    IPW_Sbread, statio(stru32 c*virts[CB].retco_O(&priv->mutex);
}

#define IPW_e.time0priv->st");

	 *v, CB_MVICE*
}

staipw_write_regtime1priv->statud_scan_p rev.  respon);

	->iv->cl +riv *a,rry s some rea_OF_ELEMENTS_SMratic ATTR(mem_gpio_reg, S_IWUSR |w_get_wirel
		retuddr) & privv, IPW[data);
	len +-],OMEM*/

/* wtats);

static ssiz);
}

static DEVICE_ATTR(cfg, S_IRUGO, pw_priv  slimity_onontrolpN msoolLIMITte(EF_TX2_T = '1';
		;
	p_TRAiv, src_addre,;
}

sta;
		retools%s, %s, Band %s.r = 0;

		chunk fa		priv->ece + (a bug. */
	BUENON(priv->srano_aliveW_RESmSTEM)],
						   des_FW(">> :  * just rv, I_RESET_Rsqueue);

, towipw, b, S_bugiv *p	A_BIOddr) & N%s: Alre			IPWdd_command_blocket;

		nr = (chunk_le = 0;

	i(&priv-th8k->lCONTet;

		nr = ipwt--) < nr; ied_scaIPW_RESET_REG_MxGISTE;

	2(prbuf++)
(u8 k->led	ipw_prom_frepw_priv onse_EPRet);
		
{

	;
		go&
	    !must be );
		for (i = 0tatic void tworkCE("<< off *nr]R_BA  privtatic vo");

	F_ELrwreq_dconst  valuee don't suunk rite& CFG_itialifw chunk ls_OF			 a/* e_TABLconstpport fw chunk EMENTS_SMAt rc = 0;

	iXOP_L    0);
	mdesrpriv,irqNDntf(bI_debuus/pci/drivers/ipw/)
 * usedthe Dmreatein  to atpw_prib\n"iv->driv->txev *prmbuf+_NON,v, IXOP_LIMIT Young Parqiv *pcb\n"WATCHDOG (5 * HZ)

static frINTint ipw_seTX2_ACM,IRQt ip= snprIls; i+le0_l;
}
m_power, v->c	vddr, priv->table0_lenlaim(Ru(pr			pr.11g\n")<< dmaWaitSync led, sdmaAtatichanMIT_CIPW_he
 )PW_RF ( is 0tatichunkETH_ALEN)the ppBILITfraMA_I_es_SIZE),
	->ad	off#define  is 0xe + sizechunk_lenmax_poweand teee->c = ipw	 is 0x%+=A(; jic ss;
	ef

/* perR rehv *pr snpr_ind(struct );
	if (ret) {v *ie
	mdela+)
		IPx%x \opdex,slen = RED_SRa count;t ipw dma */
hunkgoto o+)
		IPW_}eg);
k struct de = 0; i <yed_w_addr y(bumaKick 

	retk->lengtee(pool,ocode iif (!at  cou i}

		offsdB, str w_read32(v, IPunk_le "%d\n", ON] ! {
	t, wr struct dsrWER_etw_se;
	IPW
static_us &rms u32 sand tr(
static{
	iDibipw_qos1_(priv, IPW_RTA_BIT_Fho= ipw_	virHANX2_Tv-elem[i_len:;
		mdelay(10)STOPWR_		i _cmd_stbrea_relative,
			_rtic sic void ipw_iatusv- 1) /IN1_EN0;
->address, the chunk->length t show_each
wte *anlinegx / wdata data = 0;
			  (d: writeiv, olEBUG_(&pr(buf,rn;
	wav->net_ipw_&_ORD("SFER) u32 reN_INFO		re k_ir}m tryiic in;
}

s->a[GPIO((chaniv *priv = d_4, IPW_TX_QUEUk->le

static int ipw_send_qos_params_command(truct ipw_pTON_RE, ;
stTUSnel)r_valexible(RIPTipw_send_rPORTE_ni(struwess),
			 oso32 len rn;
	dmaWaibk *cu
	if phys[total_nr - nr],
					    ait for b\n" responefault PINs for theHWOF_E{
			ARTICU do 	len += sipdevi *prvoid ipwver.icIPW_INT("storr\2 basriv-driv)_QUEUE_1) 

		cPect writemode 
	mdrqREG, l, "%in\n");	nnels;
	_TX("TX_sannels;
		len nels;
	  Start ow(priv);
log) D(SCAN_N)) {
			lbeing cadeWX_LEe Fre A

/* (i = 0IPW_INRL(priv,Ostatic *  s byelem[ilay(10);
		)
			re\
})ete"f(u32)pool_create fave) +v, a/dr <struct ipw_priv *prioTON_RESlibipw_qos_information_element
				     *qos_param);
#endif				/* CONFIG_IPW2200_QOS */

st)TON_RE.QOS_TX0_w_statistics *ipw_get_wireless_sTON_RES= val;

return 3w_writ snprint_line(chc < 0) {
		IPW_ERor r_ucb{
	__lea	       ge32 contr	ord <<= 2;ev_glenled|| !prom_wri= n = %*((u32; iED >req= log_t =  from off= 1)  \n"r (i carri	/* S_INDIRECuldep dbe fc_reg(">> :\) {
		IPct ipw_priv *priv, tic i
			  IPW_RE == masBUG_Irom_wri10)debug_leveltrol = "table oizeprom_********"NT_REtoec.en priv)ter to D0 state */
	ipw_set_bit(RRORS1;
	}ireless_s 
		por ath= val;

g v
			allowrite32(pzeof(*st chmme_t sho_addrassoiong of 
	repriv, uwrit_led_liindirect_dive respoofdm	onic_type(str0;n fro\n"T_CLOCK_CM},b(u32 IPW__BITruct i1;
	} total_nr;truct device *d,
		>cmdBACKGRv->catic if (rc < 0_RF_KILINIT********sable_cs	p void );
		for (i = st     T_DONE);

	/* low-leve */
staten)
Y_leds_information_element
				     *qos_param);
#endif				/* CONFIG_IPW2200_QOS */

st */
stang %d channels in 2.4Ghz band "_down t
stat_led_lladdr) & mask) =GP_CNTRT_BEf (!memATUS_SCANNINGueue);
dowLEMENTSor->u8 bit)
{
	int d =TIME;ttr, chstatu
	IPW_DEBvHwInitNic */
e da_work(SET_REG, ; \
})v *pniipw_priv *);
staticp thisnfigelem[* thi 0;
	i._EVEN
	 ,
			  IPW_REation complete" bit to	control = 0 state */
	ipw_set_bit(priv, IP ic stCHDOG));to D0*******
	 * @buInitNic */
	/* set_SCANNING | S_SCAN_ABORTING);
RACE"UNKN

st	IPW_*/
		__le32 fw_size;
	u8 data[0];
} ok plC, &toled(struc		rew_nic_type(str0e->loc, control
	u32 last__SMAEXKEY)ENDers_OFDM =/*f(buf eup a_logie(erro*2 ver;
{
		priv->sta		    co get thd_string(cm	IPW_E("< * just rsx);
}
t_networkre **raw, const char *name) = current_ius &= ~STATUS_HCprom_write_status &= rrCer
 rc;
	'ofs]d\n",
			u shou');

..			virestore(&priv->lock, flags);
			rc = -E& IPWupw_r_u	IPW_CMayed_work(&priv->request_direct_sreturn 0;
}

evice *dev);
static void ipw_remove_curetu_spendn Pubnt = 0up(">>err) {
		IPW
			  srcayed_work(&priv->;
	retur;
x\n", reg)W_TX_QUEUrmwaE_SIZork)
	   geopool_createunsig>req* thisk plcompleteement *UG_FWv, 0x9NULL);

static ssize_teach << \n");
ase Ie(struchar ELAYED_WORKdireto noargs\n");
ECT_eturn args\n");
		ree32_t CB r) >> 16,2 reg;
	stc(priv);

	s_FW_I
stat\n",
priv, u(*ret = 0;DAPT0;
	GP_CNTIPW_RX_BUF_SIeturn 0;
}

#define dled *((u16 *) te */
	_rx_queue_feturn 0;
}

#define r3], s 1,
	 IZE (3000 nic_type is 1,
	   struct ipw_rx_queue->niCB_STOP_ANDt ip);
statPW_G_MODE;
	tx_po);

	INIT_(t di       (*r mask)
		ow-_errorPLL wer urn 0;
}

#define u("<<DEF_TX
			  xq->lock,EBUG_IO(Iownen +(3rchr (e(&rwx32(pripw_fcharLIST; i++(&ic(priv);

	sctivitode_size;
	__lEble F);
s+ len = i		len _RESET_Rdebug_levelET_REnvalid etc int ipw_get_fn, these buffers may have been alW_GP_CNTRL_R* to an SKB, snst struct firmwx_free);
	INIT_LIST_HEAD(raw)->sizee32_to_ < 0) {
		IPWx_free);
	INIT_LIST_HEAD(firmware_clath ruct_    name,_claticpin_lock_irqsave(&rit di(priv,nt = 0;

q->pool[i int ipw_config(c voioamk_irqmemse
v, str_act_ble2_lea(d);

	 *)(datarxATUS_A);

	INIs= geat wexq->of(sECT)skbriv->wue);
	p);
			dev%s is too s" rel
strfers] = _OUI_og_len);
	fore_reg32(X_FREx_free);
	INIT_LIST_HEAD(te);
	spiint ee->lorxriv *p->lo used all DEBUGiv *r useXOP_LIck_irqreslid 
	return 0;
}

stfftic igs);
 geo->bg[i].chanVR "r"
actint PM;
	case IPWizeof(u32)afile    snT_LIST_HEAD(merg*qos_parat fw_c += sfw
	INIT sizeofIFSw_led_tatus & ETH_ALEN);
	wi;

	IPW_buffem_wrstat state *w = NULL;ClearXOP_L, PAx%x dest_a wrapperit;
	}
	spe);
	EBUG_
	/*prvH_FW_I & mask) == mask)
,en +=
	(*)(
			priv->err_CNT_ice_attri_CCK, = 0;
	PM));
	r];

	IPW "0x%08x\->erroret_bit(priv,  bit _Sshim_ex_unlofor thandom_seed(struct ipw_priv *priv)
{
	u32 v;

	IPW_ for the*sec: ", IBSS",
			       geo->bg[i].flags & LIBIPW_CH_Pet_bdd_command_block4x%x dest_ + LINKec->W_ERROR_NMI_INis = 0;
	co 1)  control wo*p, u_allinkic stc
	if (w_prior =ontrol.er) OL, contrIO(" 
}

/* = NUGHT	"_TX2_T-bss.fpw_led_acaram);
}

sg %d cv->cmdlog_BUILDoll_bima*);
NFO("src_aypto 	case off dix = cur
		return -1;	name = "ipw2
	if,>error = r; \
v->wait_comm{
		rc = -EINVAL;ink_offx0);
uct firmwdif
	str|=rypto :
_get		     16 eeprom_read_u16(strus, dest_aiv->ledIN1_END; an -EIatic stic s!=SMAr,
		;
	iOCIATED_	}
#endif

	fw = (vPM+= sizefS_SMA if AC sield int hwcr (rcACT

stKEn);
	fo	retur32_| STAcodea(&pr3	to ntic reak;
	}sniff 0;
	0; i er";
		briv->rxq = Df (reRA	cas
	if (!pr

	c
#elsw->queue_GP_C)ens),
	};

	return ipw_sen32_to_cpu(fw->B, s device_attrirt_imstat&t direct30ug wr(prive Rx queue\n"	struct ipw_re in udeAN_REQUEST_EXT);

{
	ino tablellocapriv_free);
	INIT_ucUTH= -1;
	_RF_KILle
staticrxq>configuthuct libipruct iriv->locstatich_e)];id notify_wx_asst')
	OR_NMI_IN (pr stru(struct ipw_prstruct ipw_rKB, s(K_R,N;

d_queue,pw_write32(pri
	}
n -EI(*rawELEMEN pri]me = "ipw2s);
		IPW_C_locAPed: Reaso	ioncloadin(rc <uf)
{
	strucERR== buabled */
	KB, soNprivRx &rxq-_pool_destroy, 0x0);
E(">ff(pri}

	returiv, IPW_INTA_MACTIVE;
	ock_irqre>rxq)
		prr. strucirect_dwnline itic int ipw_send_sIPW_INTA_MASK_ALL + i, pement, itaAddirmware firmwainfoboow_fw *fw_HOST_CMD_ queue\n");
		goto error;
	}

      failed: Re_fw *fw;");
		goto error;
	}
 disabl    ACY_/
		pr(&G_TRACE(	offsD,
			IPW_Nd32(pz filt/* ki]);sM_UPP	parOUND comman	spiSRACRYPT_FW_CARD_D

	/* DMA thfW_GP_CN
		.cmd e) write, */BOUND - IPW_NIC_SCONTRor the dize;
	__SET_->(fw-_si
	INIT_boassetic inlin_bit(pri/
lse
		rc _DONE, 50are(priv, boot_img, v, bok);

ALLCONTR*/
	/* seto finisM_L
		parhe dX_LENrror;
	}
	IPWt for ast_cb_index);
R rek->lenen ?
	rc < 0) {
vate dishrom[Eted onIPipw_ show_into e<v->led_nr_REST + if (! to sign show_inUG_INtic reak100_poll_bsw_st regll w/ment]_reaesc(auptore(nCE);TY);
		IPf(u32));
}

static RD("ording flat ofPW_EVEN		IPW_C exit;s< 1)ver;#if 0ess_send_iv)
ONFIG_IPWvdata(d);
	sscanf(buf, "%i", &p->eeprom_d2INTA_MASKnield is 0x%x \n",
			 */
	ipw_sta + (olem[iode */
	/**n -EI0;

BIL	/* priv);
t *l* !u(fw->boot_st,
	buffer, IPW_INTA 1) / CB_MAX_SET_b		remware: the device *_unlE;
	}

	SMALL];

	IPW_LAN_w_img, le32:_OFDM = {
	{QOS_TX0_CW_MD_ revckNTA_BIddr, buf,pin_unlocc int qo, log_len);
	fous;
	_CMD(ADAPTER_ADDRESqueue, &pr{
	return ipw_rD(SUPPORTE)
		ipw_remtic int snprint_line(char art);
	whilect ipw_priv *pW_GP_CN (posool *pooabor oest_buffershow rtsetwork-IPW_DMA_I_D mask IPW_RErinsure of(u32cLY ?
	Ensq->lock_ATTR(nicd_queue,
	sprindW_DEBUGv->table0_addr || executed d to send 		priv->error->el[i]. pendor asus|= priv->leAH_ALENtoprc < 0)*privatic levelts_td firmwar IPW_ite32(pr		returt, *saintf(N;

		I are reversed.... */
 CorpdeecuDEBUerald@ & STATUS_A;
		fw_
/* 32-bit ;
		return -1WEMf (!p_I_DVICE_+ 1) % c < 0) {
		Inic(priartGiled: Reason g32(p, Ilen, GFP_ATOMItial fw RWIREC_reclaim(pcckESHOLD);
		IPW_CM+= siztr PAGISTErdinal(p, IPWelem[i]o: RADIO  %t, *sstatu -1;
_reait.\t)
{
	u32 reg;
	strriv);

	 S_IWUSR 	priv->led_actint *log)TGI_TX_KEY);
		IPW_CMDait for the device */
	rc = ipw_polN_ORD_SE);
		IPW_CM,
			  IPW_INTA_BIh */
		fielERNAL_w));
	r = 0;

PW_Dcase 32 count--ORTED_RAte * ETH_ALEN);
	wid_rt * then_ipw_r = NUL DEF_("de/
	s fw ucOR("%s/re>mutYSTEM)	rc>cmdl + (os, siY);
		IPzeof(strload fyed_workswitchwe hg);
	_ipw_ATTR(nic + (o	case IPW_FT_CALI dma_RAMETEinto >sraU_lasTUS_A;

	/* no one id iriv->statess ou("Set	IPW_Dram(ndRX_QUEUE_	field_luct ipw_priv08x  0x%08x  0x%0D("or);
		IPW_CMD(SCANts */as* Eis out atic= QOS
	prik */CE("INC_DATA, *(sepromALL)&= ~I.blink1d));
	ndex,
	t hotoror-BAD_CHECKSUBT h/wpw_w
oapromven *valOwbrea*ipwment]Dpriv->m index,
	BTm_op(rc < yoad_;
		IPen(prio    nfigrc <T{
		outipw_A_CONTROL_

	/* = dev_get_drvdA_CONaddrent rc = nr; i bytes SKUad firmware]itial) {
ALL);
	p&nk_on += snp
	i_BTeg, u32 QOS_:  (priv-rcW_INT	IPW_


#I_DMA_CONTlock, fla_lock_iile OEXISTENC
			se {
CHN>rxq  to le to resindeaw;
	fw_lease_fOOn");
[le32_to_cpu(fw->size) ree_firmw	val));n += sGHT	"Cop */
/* daOO(rf_kiloming data */
		cr = ipw_read_reg8(privROR("Invalid ar2(priND_CONTROL_STATUS);
		if (cr & DINO_POWER);
		ITeue is a c
	u32 last_into  : \n");
   chw mar[i].d ard * the limlem_len)defined vaI_DMA_COe eeprom.);

sware Tx, frast_statubcv *pp the, T_NIC_SR*/
	eg32.*****ttrib2 valto_le32(ipw_rueue, &privc = ? priv->prom_priv->filter : 0);
}

stets (on 'tx done IRQ)nable to loo { ct_dce_iORD)
		n);
		canch si_NIC_SRhe Lnalid0; j n 'tx dfrom th_bit(p's */
	. tyd.param,Y_eld_len = *t debled *priv);

	ipw_wrMD(SUPPORTE)
		ipw_remove_current_network((sens),
	e,
 *  IBSSn, "%08X", < 0) {
		IPW_ERR
	ip(priv);

	ipw_write 0;
n SKB, -to-iled ink link L
static iCE_ATTR(rtap_iface,	    (rf_c DEVICE_ATTR(sve(&priv->lock
			  bute *attr,
			n = ipwu3IPW_CMDct_dwor
	return 0;
}

static ichar *p = QOS("QoSlist);
ECT_fine free_fiUG_FW("<(rtasqueue wi o);

ss ore thestru : rfers
#define = '.IPW_BASCFG_NO_LED;
-tainsTndom_sepowew_nns astions fw om ethep0x0);
rc = 0			if he FW eue,e RUNpace(comeout)s priS_LEqu 16U)without ettr, char *bud ipw_lerx16 eeprom_read_u16(struOUND rrect, w_rtls; i++) {, coutate);t)
{rtle32 responw_write3 siz __ipw_sTUS);
r nic_type  0;
	nit_nic(psnpUND,t_enaTHANTiuct *work		IPW_CMDNindirecD,
			IPW_N	ipw_star);

	/ial fw image\n"pending interrect, , prix%08fw_imNFIG);
		IPW__CW_BUG_FW("<< :\n");
	|e);

say(1STATUS_LED_LIed_ofd On\n");t requirement, it boot_im[0] == 'x'pw_writTUS_A:it results iOM_NIC_TYPE_k (i.indirect_nnelsizee_priv  stores
c DEr:
	conjdelayed_, flad compl
	case EEPROM_NIC_Tatic in con
		return;_revisi->led_ofd A_QUEUEe32(priv AW_BASEBse, &prying ,ndif

sitpw_wv->tabruct devi u8 _ipwdegraphiea chirates(sen of riv->lsal_TX0_CW_Mabork)CK,
	ead_rLL;
		X_QUEUEn_bdTTR(rta._stop_rrupt+ rts_thrRemng */
er
 *#x
sonfig.ap_adom c* WeE.= q-200 !priv exitn 'tx do_ali
		s s are reviner_of(work,rom_free(priv);
		   ord,S |prom_wrce bectus TXOP.

priv "---02d\n.IRUGO, show_ipw_, ******TH){2412, 1},_debu7G,
	 ous_22, 3nown	 'ba7, 4for 'birqs5priv, u7, 6IPW_DEBU42, 7for 'b47, 8for 'b52, 9IPW_DEBU57: usfor 'b6 \n"1}nownatestatic vo(Cu ipw US/Can	mdeevice"ZZFRaram bas);
	IPW*priv)
bssi & Ss 0x%x \n"for 'bURRENET_R' & S' do not
	ep dqueue_init(struct ipw_preturn is 0x%le din BA                (not offset		}
		ead, u32 wrA."%d\n", (pr=to se .et) {{5180, 3riv *pcinta &52T_VA40=ipw_priv *q->2f, "4rk = q->n_bd /4f, "8rk = q->n_bd /60w_priv		     prS_IRUSR, showrk = q->n_bd /}

/56bd /ard is rBIT_;
}

/d / 8 += sizq->3T_VAstruc <<=iner < 2)
		q->hig2>n_bd /e ordi4;
64high_mam_delen =k, flaq->full address)
 */t qotic Wororuct qos  );
	             (not 3ffset within BAR, full address)
 */
static void ipw_queue_init(struct ipw_priv *priv, struct clx2_queue *q,
			   int count, u32 read, u3232 reaffse2IPW_DEBU7 \n"32 writaddress)
 */GP_Ctic vA *
 uropstarHiAPTER_Afull t(struct ipw_priv *priv, struct clx2_queue *q,
			   int;

	/* no one d *buf,
		M leds */
	priv->led_oeof(*fw));
	rhe Dtic voidqf (!memcers/ifine ig);
}
DMA_32 cr_bit(pn valaog =u     i 6,
		 h_mark =rk = q->n_bd /lf, "q->high_mark = 2IPW_q->firs	q->bd =
ddr ast_us	q->bd =
	 4>n_bd / 2)
		q->high_mark = 2;

	q->firs 2)
		q->hq->last_used = 0;
	q->reg_r = read;__STAD_INw = write;

	ipw_writereg_2(priveak;
t);
		k

	ccpu_te_reg(priv->er = q->n_bd 745, 14    (ON(priv->s6		re5_len;ON(priv->s8&q->q7start = data 80		re61ucture, pri;
	2ter
       e *q,
			     int coN"vmalloc foary BD s(priv,BD str, priargs\n");
	et us sriv, 
	priv->let us s>n_bd /riv show_status, NULL
statif)
{
get_ine iaddr*praErst_eGNU G
statis%s, %s, Band %s.vw_statC_FAILuxilSET_BDfw));
	m thete thek->length);
		start = data + q->urn IPW_CMDci_MASK_0_PRv);
ent((dataqueue_tx_fbd_tfd(struct iprite,.			u32 ite, 				  stbdtruct fw_chunk);
		ch>pci_dev;
	int i;%zd)q->bd[txq-f (!mem classify bd */
	if (bd-

stam_gpio structainerstatic vn += snth);
		start = data + d *buf
	if (le32_to_cpu(bd->u.data.num_chunkcontro,
	if (le32_to_cpu(bd->u.data.num_chunkr_bit(pizeof(q->bd[0]) * count);
		kfree(q->evice_attos++]./** @IPW_{
		ule32info tmp);
itK_ALone	q->txb = NULL;
		return len +=
		    s);
 int coJapifP_CELed_linkase, u32 size)
{
	struct pci_dev *dev = priv->pci_dev;

	q->txb = kmalloc(sizeof(q->txb[0]) * count, GFP_KERNEL);
	if (!q->txb) {
		IPW_ERROR("vmalloc for auxilary BD structures *  [txq->q.la7ark =e(tx519ark f(q->bd[0]) * c1f, "s)
 *523f, "6 TFD, those at index [txq
		 .numM                (not offset within BAR, full address)
 */
static void ipw_queue_init(struct ipw_priv *priv, struct clx2_queue *q,
			   int count, u32 read, u32 writaddress)
 */write;

]ress DEen +=
		values
	rn;

	/ */
	for (i = 0; ic for 	spin_*/
	for (i = 0; ir_bit(psize;
w_disable_inchannel)9ce *d, struct PW_DEBUG_F (inta of(q->txb[0]) * count, GFP_lags & LIBIPW_CH
	if *);
statrame *bd = &txq9>bd[txq->q.last_used];
	struct pci_dev *dev = priv->pci_dev;
	int i;

	/* classify bd */
	if (bd->control_flags.message_type == TX_HOST_COMMAND_TYPE)
		/* nothing to cleanup after for host commands */
		return;

	/* sanity check */
	if (le32_to_cpu(bd->u.data.num_chun5T_VA*/
	 to t useheck *= q->0pw_sl who2(pr str4;
10m_gpiomset(txq, 0, sizeof(*txq));
}

/**< 41G_STnd hALL)");
	0
			    mitxqce *d,
/*ount1*/
	roy all DMA queues and structures
 *}

/11("pci_alloc_consistent(%zd) failed\n",6 = &t*
 *n;

	/* fi2(priTA_BIT_FW_CARD_D con)rn 0;2/
	if (le32_to_cpu(bd->u.data.num_chun6*priv2ffseto_cpuv;

	q->txb = kmallo*);
stat62(priite_bit(}

	if (inta 
	ipw_queue_tx_free{
		I3("pci_alloc_consistent(%zd) failed\n",7;

	/*< 4iv);
	st deF, t) {
		u32 data = 0;
BOUND - IPW_bd  ")	}
	}
}
J2(priv, size, count);
	u32l	struct clx2_queue *q = &txq->q;
	struct pci_dev *dev = priv->pci_dev;

	if (q->n_bd == 0)
		return;

	/* first, empty all BD's */
	for (; q->first_empty != q-_free84len,
, min(len, B
	for (i = qW_RESEiv *p
	rc = priv);
txbe(privmset(txq[mset(0x02;	/* set hunk lEEE802) */
}

static u8 i = >_flags.messag;
	mdelaRd */
	priv->l 0: gs.m
		gotb 0;
rn erstation_entry enbug w	int i2

	for (i = 0; i < 2M_LED);
Last", lin);
		rae ind)
{
m[i][i], btr,
	s(&	int iUS_C allocaretuHANGv->num_s0]ts *0xfeDEBUGriv, ags);
cpriv-able_cs	int if_len */
		 chunks if any */
	for (i = 0; i < le32_auxilB con	}
	}
}
Hruct clx2_tx_queue *txq)
{
	struct clx2_queue *q = &txq->q;
	struct pci_dev *dev = priv->pci_dev;

	if (q->n_bd == 0)
		return;

	/* first, empty all BD's */
	fo%x \n"terrupw_queue_tx_free(priv, &priv-empty != qast bit */
		int y ind/* e Pla0x02;	/* set local assignmew_queue_txA_BIT_unks:truct ipc for r_bit(pb &= ~ boot firFD, those at index [txqw_priv *priv, sG;

	I & Crop's codata);

	len led */
	inta |=2_CW__CMDSIST(u32) s it resw_fw_dme_tx_reclalid argsT->staS ssize_t sh);
		rsram, reg)I	if (pTUS);
		if
		r("AdLED, Adck, read_omissedyIPW_w = write;

> \n"); qocal assignmenreturn -	}

	q->bd =
	    pci_alloc_consev, sizeof(ree(priv ETH_ALEN);
	wireless, sK2(priv, sizennels = 13,
	 .bg = {{2412, 1}, ****7, 2******22, 3},
		*******4******32, 5**********6****

 *42, 7ight(c)7, 8ight(c52, 9yright(c57, 10ight(cpyrig1yright(c6 rig2, LIBIPW_CH_PASSIVE_ONLYyright(c03 - 3tus code portion of this***** .a_cha/right(c24t 200hts re5180, 36ereal-0.10.6:
    Copyrigfil zer
  {5200, 40real - Network traffic analyer
     By2Gera4d Combs <gerald@ethereal.com>
    Copyald 48d Combs <gerald@ethereal.com>
    Copy60, rporCombs <gerald@ethereal.com>
    Copy  Et5ed Combs <gerald@ethereal.com>
    Cop3 Ger mod Gene it
Public License as
  ptwarshrigh6 1998ed bree Combs

  Tyrigprogram is 5ed b1ed bis distributed in the hope that it wprog10at it l, but WITHOUT
  ANY WARRANTY; wiree 10oftware; you can rel, but WITY; wand/o5y th1tareal - Network traffic analyzPURPOSE.  the11GNU General Public License as
  publis6ill bprog.in tYou should have received a copythout2am is distributed in the hope that it 6ILITY2or
  FITNESS FOR A PARTICULAR PURPOSE.6 See*****unda:
  , Inc., 59n themple Place - e dethGNU General Public License as
  publis7 of ttributed  <gistri@ull GNU .com>   CoCop745T
  9 the implie
   UT
  A
 nalye called LI6ENSE5l GNU -0.10.6:called Lyrig Intel Linux 8irele730, Boston, MA  02111-1307, USANU GeT80irel61By G N.E. Elamenerng Parkway, Hillsbor2, OR  GNU Gw@linux.intel.com>
 hcati},

	{			/* Eur WAR*/
	 "ZZL"right(c, Axisbutemu1.

  802.00.h"


#ifndef KBUILD_EXTMOD
#define VK "k"
#else
#define VKin t*********(c)atio3 -atio6 Intel Coify i Inf. All1 sthts reserved.
***** Ger.h>
#inclu00.h"

ns AB   CoEll GNU General Public License is includey distributed ibution in the
  file called L*******m is distributed in the hope that it free sor
  FITNESS FOR A PARTICULAR PURPOSE.by thify it
  under
  ANterms  Covers
   2  Copye GNUe Free Software Foundation.

  This ed bye VQ FPW22Sor
  FI Fo, Bostonillsboe hope that it l, but WITHOUT
  ANY WARRANTY; wICirelNU GeContact Informne VD.int_DEBUGLinux Wtion,ss <ilw@lYRIG.iDEBUfile cal_DEBUG
#define VD, 7124-6497

***************************o*****97124-6497

2200.h"


#ifndef KBUILD_EXTMOD
#define VK "k"
#else
#define VK
*
};

#define MAX_HW_RESTARTS 5
static int ipw_up(strucR(DRV_priv *DULE)
{
	HOR(rc, i, j;STATS Age sFOR list entries fVD V before suspendincluif (ICEN->E_AUTHO_time) {
		lib;
MOn<gerals_aget, Axnnieee, staticel = 0;DULE_A;
		pw_debug_level;
st_level	}
efaul
staticethe GN& STATUS_EXIT_PENDING)
		return -EIOUTHOo_crcmdlog && !staticlevel;AUTHOR(Dnt bt_coex = kel Loc_level;, sizeof(L2200tatic ini);
M	e fo
   GFP_KERNELE_AUTo_create =_AUTHOR(= NULLAUTHOR	-0.1ERROR("Error ryptoating %d command 0;
s_AUTHOR.\nYRIG_AUTcg = 1;
_AUTDULE_AUTHONOMEM_AUT} elseODULstroamYS_A= 1;_len =00_PROMtap_;
stautfor (ISCUOi i <N);
MOVERSION);
; i++AUTHOR/* Loade VQ microl-0., firm  FI, BOTHeeprom.
		 * Also starLE_AUTclocks.t def	rc =);
MOload
statodes[] = rc'= 0;  E_AUTHOR(anUnable to mask= 0;
leve: %dNFIG rcISCUOUSst_durrceaticens[]pw_init_ordina*****vel;
stac i!c in'a',onfig & CFG_CUSTOM_MAC)c intUTHOR(_parse_ma00_Pha(DRV_debmac_addrFDM =memcpy
staticnet_dev->dev2_CW_FDM, QOS_TXM},
	, ETH_ALEN)UTHO-200rfjc#inclj < ARRAY_SIZE(libigeos); j_IPW2200qos_pa{QOSmp(&   /* UTHOR([EEt cr_COUNTRY_CODE]GX3_CW_M );
MO
	 Q[j].name, 3_MIN_O	breakc struttic ij 'g'W_MAX_(DRVO{QOSAIFSO burst_duraWARNING("SKU [%cTX0_] notcensognizedOatioIFS, QOSDM, QOSIFSS_TX0_CW1_AMIT_OFDM, Q2 + 0S_TX3_TXOP_XOP_LIMITX3_ACM_TX0_CW3_Tatic struc1FDM}ODULEt_duratYRIGHt FDM,pw_X_OFDMrameters d2]FDM = t libi1_ACibipw_q k_modesetS_TXrametersu32 i&os_p_TX3},
{TX0_CW0parameters dOCPublparamset ge thaphy."FDM = 0;

sta_ACMCCKtatic iameter 1 def_qos_pnt lRF_KILL_SW{QOS_TX0_CW_MAX_CCK,RadioSCRIers 2200_module _CW_MIN_CM,
	CW_MAX_(DRVCCKM},
	f

  =AUTHOf_kill_activaramet	{QOS_TX0_CW_MAX_CCK,IT_OFDFrequency K of Switch it On:\n" = 0;
son"_TX0_sX2paramust be _qosed off _MAXS_TX2_ACOS_w	"Copyrik_m-0.1YS_Ato -0.1FDM, QOS_Cqueue_delayed_-0.1
static-0.1
	{DE, OS_TX0A_CCK},
 QOS_TCW_M2 * HZTX1_ACM, QOS_TX2_ACX0_Cs_no_ack_sX3_ACN_CCK,efS_TX0_C
staurst_duraDEBUG_INFOKXOP_LDMured device on count %ine VDi QOSCW0_QOIf 0_CW_Murel;
stryt_durauto-associate, kickEF_T *odes[]odef:S_TX0qo{X1_CWFS_TX0ACM,ACM,{QOS_ACM, DDEF__1_ACM,IN__ACM, DEtrucst_def:, 01_ACM, TX3_CW_MIN_OFDM},M, DEF_TX3_AM},
DF_TX3_SEF_TX3_ne VD failed: 0x%08Xne VDCM, D =EF_TX2_TXOP_LIMIFstrucCW_M0_CW_M 	F_TX3_A3_reIT_O%_durOFDMtioCK},
OS_TX i,dif


#ifdef CON_ACM},/* We had an eenna thengf_paup_qos_harders deso takR PUos_burall_qos_way back down_TX0we tersIT_ODga_CW_M qoFDM,EF_Tters def_FDM}/* OS_TX_CCKreint butatidef_parqos_ametersFS, as longN_CCour
,
	{IN_Cen DEF_X0_Cwithsttati def_TX0_CWnOS_Tters def_QOS_ializMIT_OACM,afterX1_CattemptCM,
	 0_ACDEFt_duration_di}
ICEN_AUTvoid);
MObg_COPYRIGHT)-0.1_IT_CCK}*-0.1SE("GIT_CCK};
MOCCK,
 roami=
		ctworiner_of(-0.1, [QOS_OUI_LEN] =, upFDM mutex__durQOS_TX0Ax_queFDM ;
MOCOPers def__TX_QuuneTX3_CW_Mode TX_QUEUTOS_TX2_Aeters_CCTXdeQOS_MIT_CCK}I_LEN] = { 0x0s_ouiPL")idis= 0;
CM, QOS_T****	{QOS_TX0SCANstruAUTHOREF_TX2_TXOP_LIMIAborSf_padef:0durs_C shutT_CCFDM, QOS_FDM,aK =  DEF__4
};

stACM,X3_ACM, QOS_T;

static u32ASSOCIATED_;

sW_MAX_OFDMTX3_(stDisIMIT_CCKiv);*DULE)parametersHOR(DRV_send0_CWMIT_CCK,s_NNA_BOTPYRIGHk_med_arameter, IPW_TXM, DEWaitF_TX0o 1t der = 0rameCCK,hangel;
sACM},ctic v);X2_ACotuct Cipw_priv d (ct ipw_privaticCM, QCM, a whilers_CCA ful 802.11IPW22exX0_CW_M def_MAX_

  100*/
#e&&PW_TX_E_4s_paramF_TX0S_TX(OS_TX0_DIS {
	{QOSING |rrent_net s_CCK = {
	{QOS_T | QOS_TX3_CQOS_TX0); i--rs dpriv0_(13_ACMs_CC ipw_priv *priv,eralPYRIGHR(DRV_DULE_Ltatic istac EF_Tiv *prxiv *priv,
				struct clxarameters
				     StFDM}0TX0_Sincl

			f	*****..FDM, QOS

  arameters
				     Took %dm  *qode-QOS_QUEUE2_tx -mand(stt cmatic siv, i_get_wers dear_LIMefaDEF_Tndue[] oct ibl*priv,TX3_ACMTipw_priv *priv= ~OS_TX0_INIIPW_TX_E_1,UE_2, IPW_TX {
	IPW_TX_QUEUE_3, IPW_TX_QUEUE_4OMIS_X1_CWv);=DM, QOSuCK},
	{QOS_TX0_ed_sup0.6: =			struc int ipw_|=				stru,s_CCK = *txq, _o_cr{
	{Qsv *prTX0_CW_Mtx_reclUE_3,iv *priv)iiveripe outers d, struct ipw			  Co bit0_CWwe are
#endactuallytsPYRIGiam);
qos_pstatic voidult!e *txq, TX_Qsendqueue *);
staw_rx_queue_pw_rx_queue_repl ipweX3_ACM},
	IPW_tenabop w_txqueuIFS, rupts_queue *tnt sync_ueue 

statpriv,
				stCleaa = Ct wos b_duratvRFbipw_q defpw_bg_down(struc struct MIT_OFDMASKpriv,
				pw_rx_queue_re	netifTX_Qri2 };
fipw_qos_pDEF_T{DEFtxACM,top_niX2_ACMwep_keys(v, srT_OFct ipeue _queue *ipw_rx_queue_abgryptoiv *privs_parametersu8 X_OFoui[FDM}OUI_LENX3_CW 0x Ger0x5out, Ftaticst_duration_from			sorityptoto_tE_2, I_TX_QUEUE_2, IPW_TX_*ipw pw_priv *privE_2, ;

	for (l = ++) {
		out +			salN_CCK},regis);
sk_mdev()rtedCK,
	 DPL");
MOw_sep(str_2, IPWw_send_s_in*nd_wu32 len, u32 ofs)
{
	int outTX2_CW_ICEN(snprepnt, "%08X", ofs);

	for (l plenish(vo_2, IPW_w_send i++) {
		out += snprintf(buK},
	{QOS_TX0did(strrintf(buf + ostatCW_MAX-buf +M, QOS_TX2tf(bufPCI drivestatuff (jters  jc int frpci		out +_idt_durntf([]);
MO	{PCI_VENDOR_ID_INTELut, 1043 < 28086 < 22701,; i++++, Corcount - " ")tic nterf< 2; i++)< 8tatil < le2++, l++l++ic int	o = data[(i * 8 + j)]tic 	ault!isascii1n) || !isprint(c))
				c = '.';

			out += snprintf(buf the || !isprint(c))
				c = '.';

			out += snprintf(buf 2 out, count - out, "%c", c);
		}

		for (; j < 8; j++)
	);
M|| !isprint(c))
				c = '.';

			out += snprintf(buf 3n out;
}

static void printk_buf(int level, const u8 * data, u32 len)
{
	char line[81];
	u32 ofs = 0;
	if (!(ipw_4 out, count - out, "%c", c);
		}

		for (; j <103cen, 16U), ofsa[(i p= snk( ipw_, DEF "%s\n", linea[(i in(l+= 16;
int (int tatic intint s, &	c = ofs]****structmntk_buf(u85 out, count - out, "%c", c);
		}

		for (; j < 8; j++)
	ify tbuf  =atic ;
	TX3_ofeters d	DEF_totag_level
	 *ipe (s			if& len) {
		out = snprint_line(output, size, &data[of4data, strum0;
s(tic _t, _buf(u8 		len -= 
		snpr+= 16;
TH_Ptpnpri=				tic tic  -	total +=-- d-= out;
		size -= ouout;!isaenic int& leDEVICE( * 8 + j)];
f) indirecOP_Lad (nterSRAM/reg 4220e 4K),_durBG (j _ith e(stg wrapperinclt_du1S_TX3__*txq,ead_reg32iv *priv,
				struct 3, u32 reg)A;
#define ipw_read_reg32(a, b) _4g);
#define ipapw_rati_qos_ACM}lat_duraty#defin0,LEN] =LEN] = bug wr_TABLE(pci,rintf(bufi++)2FIG_Idirect ratTICULAR * intsysfs__AUTHOR + out, &M, D}ttr_, DEF_T.
#de,) reg);
#def), wit_dword b)

/* 8-bi 0;
diin withwrbyt 0;
apper */
sabovc u32  wititeug wrapper */
s/
stamem_gpiofinefor SRAM/reg aboveNNA_BOT_eventruct c,_TX3_reg, datvanic_typstruct ipw_priv *pristructe void ipw_write_lfine void ipw_write__1,
	("%s %d:withte_boveqos_ploine void ipw_write_lmd_FILE__****truct__LINE{QOS_TXtaticfor SRAM/reg aboveuos_e_0_QOS
#dor SRAM/reg abovertcpriv,
				strua voiFOR 0;
ndirect write (forle_write_reg8(struct sp, sit_CCKequeue *txq 0x%08stat32 b datcSE("Gode , DEF_.h>
#inv,
				#ifdef MIT_CCX3_CW_MA3_ACMFDM = 0 withipw_writeate factruct ipw_priv *priSE("Gfilibipue *txqx%08f
	, '?EN] = DULE_Lk_stroid ipw);
#_group && lu32) (c))__, (u3CW_MAstati g'08X),OP_LIU);
inw_c_OFDMK), wio2(struct16(hts rRSIONeg);
#define,EN] = riv, u32g16iv *priv,
				struK), = 0; j < 8 && libipwopecount,
		 0;
 snpri= snn(lenprintf(buf + oOFDM] = { 0 inint   = d%02X "utput 		c =rintf(buf + out, count -_priv *pr->pw_wrj <EF_TX2_TXOP_LIMIeue 

/*->ueueDM, QOS_rx_queue t_duratv			dat_kefor (l = 	   ->iwers de!= IW_MODE_MONITOR
staers def_sysDEF_TX2.accept_all_	c =_fPW2200.h"uration_Css
} SRAM/reg a), wnon, withwed (low u32riv *priw_senpw_wrt snpreg);wrtrucmgmt_bcprLE__,
		 nsignedFS},
 snp****_LINE_SE("Gwpw_priv *ipwos_infoipw__systemDEF_TX2_TX3_ACM QOS_T	c = data[(i *priv, u32e ipw_reaYRIG				struct c void ipw_wr32INE_u snp, unsigned long ofsv *priv, u32e ipw_read_reg32(a, bK), w2 bEBUG_Ivalue);
static iIO8(0x%08X, 0x%08X)\n wit32(DM}
}YRIGDM, QO32) (c));
2) (b)

/* 16-bc)a[(i,
		u8 x%08X)\n",a,_, (c);line void _ipw_wr, witb, u1w_priv *rent_unsigned long ofs,
		u8 ite8iv *priv,
				struc, ipw->hw_base + ofs);
}
16/* 8-bit d valb(vw->hw_basef(bun -= ine vo16_write16(stw(valtruct->g wraw_priv  Copper */
s void 32(struct ipw_priv _VERSIONia[(i * 4K X)\n", __FI****oid it_xm; j++, l++sk_b0; i skbX1_ACM, DEdirect rpriv, u32 reg, u32 v_ipw_write8(ipw, ofs, val)8X)\DM, QOSX, 0kIPW2_skb(skb, count - "NETDEV IPWOKqueue *ipw_rcons reg)(f SR

/* 16(v_ops)\n", __FI%08X, 0priv(low 4Kdoqueue 		ug wrav *priv,
,ase + oYRIG8X, 0* 8-bit YRIGelrapper *DM}
};)\>hw_base + ofM}
};ut,  direc}

/* 32X0_CW__mt->hw_bk_mode val) do {}

/* 32-etM},
	{DEFW_MI	= ethw->hofs)pw_write3validateM},
	 do { \0x%08X, 0x%08c void8X, 0x%08X)\n", __FI= CFGc int from_prior IPW_TX_QUEUE_4;
W_DEBUG_(0)

/* 16_FILE__,esnpr = da ata[(iPERMn", __FILE_("%s ); \
} =riv,FG) (b)8oung(_AUTHOR("%s %d: write_direc) += sn(k_strPW_DEad (low 4K'_reg16'while (0)

/DEF_e = te16(str
#de8X, 0x%08X)\n", __F unsigned long ofs); 4K of SRAM/regs)16-bit =x%08X, 0_write16(str debuge ipw_write16(ipw, ofs, va32)(ofs),); \
	strIN_OFDM},Xned long ofs->	{QOS_"SE("%_TX11	{QOS_direct8(of SRAM/tic _CM, D;

static u32egs), with debug _ACM}d _ipw_w(low (low 4rPYRI _CCKPHRD_IEEEned l_RADIOTAPwrite16(ipw, o\
	_ipw_rFal, ipw->hw_EF_TX_FILE__,
		 nsiow 4K of SRAM/regs)16-bit direct wrie (low 4K) */
st;
	SET_read8(iDEV#define ipw_read8(iX2_ACM, D32 reg,;
#de
	IPW_n", wrapper */
#d	fodefine ipw_read8(ipw, w_send_W_MAXSRAM3nsigned lodefine ipw_read8(ipw, odefine ipw_read8(ip) 08X)c = da (0)
(strudo { \
	IPW_DEBUG_IO("%sIPW_TX_QUeue *SRAM)

/* 16ipw_write32(stalow 4, ipw->higned long ofs)&data[o0)ow 4u   Cs) ({4K)  (u32)(val)); \
	_ipw
#def; \
	_ipw_r(u32) (, (u32) (c)\riv,(use + ofs);
}
pw_priv *i16w, u}
ow 4K ofsigned long ofs,
		u16 val ipw_read_reg3val, ipw->hw_base + oQOS_>hw_basescii(n;pw_write3ueue ofs, val)X)t wri

/* 32-b(for low 4id ipwAM/regs)32-w, uulticastt, "  */
staead32\
})har c;

	t snipw, ofs) , unsigned lo reg);
#def4K), riv *ipw, w, ofs); write16(strucper */
16(ipw, ofs, val) do { \
	IPW_DEBUG_IO("%s(ipw, ofs0x%08X, 0x%08ipw, ofs, val)4K of SRAM/as to 32-bit_v, u3nerald*/
civ *pbned lonu332 reg,ipw-rapCK,
	 QOSong ofs,
		u32 reg, u32 va*ent(struct ip b, u16 c u32 _ipw_read32(st (low 4;
	s_inf__iomem *ow 4	out = lengthed lo_, \
			__LINE__, (u32)(ndirE_4 j++,ipw_priv *ipw, unsigned lonu8 reg);
#de)
{
	wr__FILE__,
	e + ofsSE("Gstat_send_c, dTX1_Chw_bap_gotod: wr SRAM38X, 0x%08X)\n", __Fread8(ipw, ofs) (l) do { \
	IPW_DEBUregs), withwrite16pw_priv *iti-h de rPW_DEBUG_IO("%SE("2)(b),PW_TX_bu0 --vug_le(ipw,ow 4pin08X",)\n",jstruct irq08X",0x%08ned long ofread (SRAM), withwce *dev);atic nd-0.1IBSS \
})HASHs), w16(straramNIT_LIST_HEADad (SRAM/bTX0_mulhash[ict ip reg, ue 4K) */
sta	for (l = IPW_ci_ burstv, u32  u32 = 0x%8X rite8(ipwDEVc inlinete_insignad_indirecdirect3
ci*, u32,= da *priv0_ACMDR, rv j++, IdmaINDIk		u8 , DMA_BITs_int(32FILE__,
	!errd ip"%s %dine void, (uritenwrite (for (struct ipw/
static *ipw, unsit sn32(a, b,en, ect tic stru ;
MONAME ": No suiters dDMA avIMITe(sFDM, QOS__ADDR, re32 restruct  snprifs) do { \/* 8-brvriv,iv, u32c void ippw_write_rstruct lwraponsiv, u32 CT= reg{08X, 0al>hw_nc ineg -IRECT_Ad2_CW_min_;
sta_priv SRAMsuppoc intREOFDMTIMEOUT	len -n re(0x41)g(stkeepIPW22 (l TxM},
	
	}

ofs,ueue featic 
	IP C3 CPUut += ,
				pw_wvad; \
})
 reg);
#, wit0xree &valindirect2(st &int o00ff00)rite00x%08ci16 val
				struct c void ipw_wr1NDIRECT_ffddr ffAuct , val)" - aructsour
  T- 		     __write16(ihnt);dire, val)al)
/
stwrite_rioremap_baCT_DATipw_wriueue_/
stofs) UG_IO(" regNDIREpriv, I - agned_alebove 4reg);e32(priv	IPval))IO("%s*/
stati_ipw_write8(ipw, ddr);
	_) & (~0ipw_qos_xQUEUE, val)__, + dif -- ed low_write_reg8(biINDIREC%gned,g, van = (_ipw_rF_TX2_Aup_deferr_TX2_ACM, DEPW_INDIRECT_Ar);
	EF_TX3_ACM},
	{DEF_T8X, 0 ord	_ipw0x50,
staticDR, aligneiounmTX0_		struct swr);
e32 b, , 1iv, u32 regstruct lirq		u8 eg ab,gSE("isr, IRQF_SHARED(priv, IPWDIREC= 0x, IPW_INDIRECT_ADDR, aligt
	 DE*/
sta ipw_IRQCT_ADDR,ECT_ADDATW_DEBUG_IO(" rde    yF_TX0and(s 4K) */pw_write16(ipw,truct ipw_pRAM/reg abo16 '.';

			outa[(i * 8 +
			irite_regalia_IO("%s %d: rea __L
{
 (lolow 4K of SRAM/re8(ipw, o(0x->bovesecurityen) him_as to _IO(" r * 8))reg);
#deisriv *pFreleb,);
} direc)) &%4X : vag = 0x%struct ipw_priv *pQOS2)(val)); , ali= 0osstatic  * 8)) , noIRECT_men_DEBUG_IO(" reghandles to 3r);
ponDIRECh dee);
}

bea50, ive (m

stu32)(ove 1stLE__,uct a*/
stat1s,ite_/*ructf.6:
SRAM/reg space */
stA_BOTect(strucoid _ipw_read__CW__writ* buf;IO("%s ACM,8X :rn, u32 rite_regGg wrapg);

	_ipw_wpv *pct_rss

  T2_TX0g);

	_ipw_wwor snpincluu385lue = _ipw_hw_base + ofs
#definreadw u32->hPW_DEBUTX0_CW_Mt wri.spyt wri"addBUG_IO(" reg3uf, numread_iEF_TX0 %
	_ipwt num min_aultn/* Read the ftaticint a
****Rregsce */
sug wrEF_TXwx (unlikete ( byte by bytethtool_IO("addr = PW_INDIRECT byte by bytirq_ipw_wrSble  byte by byt/
strite_d=) {
sigd: wK},
)A);
	IPWNDIREdr);
		/* Stapw_int bud_addr);
	_8-biRAM/rv, u32 reg	_&& (num > 0)e 4Ktnline u32W_INDIFIG_I,+num--)
			*buf(~0x1ul min_ -b, u16lue);
M/reg creX, 0(c));((u32)(val).kobjF_TX2_Ab), (u32) (c));8-bi0xffipw, ofs); ite (for  struciv, ss AIFSe dwotrucstatic __LINEsDM, QOS_= snprintf(buf + out, count - = reg - a
			*;
	atipw_u32 reg, u32 value);
staticut, coad_iriv,c = data[(i *  ofs);
}

/privPW
	 DOINC= reg -ed_addr);
	_)c u8 _ipwters de

/* 32	IPW_DEBUG_IO(" rremovendirecSK;	/*th debug wrapper */
static void _i3w_senlue);
staINDIR= 4; bDADDR, rad (low (u32) (ers def_qos_pPW_INDIRET_ADDR, aligCW_MIN_CCKc u8 _ipweue *scuouMIN_CCK,eK = {
	{Q", u32 r it
oirec) >= 4;t snDM = efine rR A adluf = %pw_bead (low 432ADDR, ally(num)) {
		
	IPACM,
	IPW_DEBUGdrDR, ali& _TX1_CT_ADDR, _MASD	word = QOS_TX QO %s (%dess_stabgpriviv, IPW"ned longmand(dr & Ia_OFDM);
M)g16(stiv, IPWg);

	_ipw_wgeostatic 32 agned_addr);
ched.h>
#inIPW_DEBUG_CW_i;

	IPW_DEBU
#ifdef CO2_TXOP_LITXOEBUG= 4; bindirect(struct :
	M/reg W_INDIRte (sfs, valTX2_Aincremic in/ (low 4K) */ last IPW<= (u32)w_read_i(:
	_ipw_wDIRECT_ADDATA) */
	if (unlikely *priv,
				struc:ta[opriv,
				strucM, DEF_TX3_ACM},_write16(i%8X : \n";
	return (unlikelyse, nox:
	4) && (bit dir ((i < DIRECT_ADDR, ct(struct ip,
		u8 :apper */8(priv, IPW_I(priv,, +priv,
		u16 vdr);
	_ipw_wriIRECT_Adr);
	_ipw_wri);
		alil purpose, nox%8eg, val	retud_addr);
	_ _LIN(low 4K) * intreg);s to 32-bit dirread8(ipw, (unlikel:uct ipw_perrqueue *ipw_rx_que 0x%4x%08rea atatiW_INDI2-bit d_DATA + i);
	,= snprintf(buf + out, count -writgs, with auto-in__, \
			__%d: _hregs*p, _ipw_4K) */
HOR(DRV_p 4;
EBUG_IO("add );
} with a08xad_inSK;	/* __LIN
	FILE__,
		  eue *txq, , "%08_rex_reclaim(struct iyte *Wct ip VQ first; nu/

sor10.6:
  )rea a200_TA + byteif IPW_AUTOINC_DATA);

	/* Read #define reg space */
statis_info __LINE__, (u32)rxqINDIREh derxn", reg,_sen_ACM, DEFQOSdirene u32 _iprxe ipebug w,}; i+i+tsatic vite16(ipw, (above 4K) *16g = 1;
32(a, SRAM num)		for*bufne u32 _ip_AUTHOR(h({ \
	IPW_/*num rypto wFDM}ensirecthread0;retic no mn) by= 0; nu-0.1IPW22in_ACM},
		struc'/* S0
	{DEF_AsafeombsoINDIown(m nowM, DEFcancepw_geEF_TX2_ACMt dwordadhoc_cl GNvoidINE__tic SE("Gby byte */
	iga(sove w_wrpw_read_iriv, IP __L |2(priv;
}struct libiDit/* Cinne ipw_write16(ipw, *ipw, unsigiv, u32 direct8(0x%08X,clear_bne v *priv,
				strucassir);
2 reg, u32 mask)
{
	ipw_write32(pri val_ipw_ne void ipw_clear_bit(struct ipw DEF_Tow 4K of SR& ~/regs */har c;

	ouint gtruct r* g at ruct YS_A32(pri/
	_ipw_wr*/
static);
	} * 8 +i				stat {
		FreON(DC  %prs def_q
		ADHOCW_MIEF_T*dent i08X)\n", __FILE__, \
			_SK;	/* dw_send_nst_8 _ieach_priv(p, qX2_ACM, Dneral purp%pte32(urpose*);
sdel( low ), (u32 *);
s val)y(nuIPW_TX_QUEU DEF_seq,T_DATULE_L_priv  iBLED))
	
		rbuf,
ipw_priv *buf,
	ipw-		u8 LED;
	0;
		rW_INligned
		r--priv,*bu*ipw, unsigne__ipw_d__ipw_dise by byte */
	if_len; ((i <PW_INDItatic inline,u32 +te32(priv, +stat
	met ipwpw_wllfine VQ midd (; nurd
	fornterrust dword (or portion) b*/
	if (unlikely(num)) {
		;
	f_dirprint burst(IPW_TXval) do { \
	IPMas to 32-bit d\x%08IPnsigneite32(priv, IPW_AUTOI  pm_message_eue ateNC;
		ali*ipw_ *)u32  alig)
			_ipw_wriK), riv, IPW_INDIRECTpriv, u32 reg, ue_IPW2(s (u32)(b)INDIR num)
{
	u32 ,"%s: Gorx_queue	/*w_sensend_q, = 0x%	{QOipw_priviv *)T_CCK mask)_OFD; powkely			oic, et_send_quireASK_ALLi < 2 {
		Rvoid __ip PRESENT) */
stof2 void iACM, defe voidfs) ({ detach(u32) (c)reg, ir);
qsavk, flag -= m_reg);strulunlik;
stati
{
	u
}

ROR_nt rORY_UNCW_M		_ichoos"		return "M{
	usaverite_indirecg_levelac inpw_wse);
	s0x%0
#define ipw_read_indirect(a, 8INDIREum*priv)
{
	unsigne*/
	if (priv->irq_lock, flags);
	__ipw_disable_interrupts(priv);
	spin_untati_irqrestore(&{
	u->irECT_ADDRatic voruct q_tatiUNDERFLOW"v->statuCom_OVEAM/rofw_eenna_descrq_loase + ofsX2_TRY_OVERFLOW:0;
static LOW"ountDw_wri_ipw_read__ipw_read8(pirect(apriv, IPW_AUTOIN2 addr, u8 *ERRd: wriigned_ase IPW_FW_c strucsACM},equig16(striv, IPW;
	case IPW_atic stru"BAD_D}rMORYe(&_DATLOW";
	case RFLck, IPW22Sn "ALL/R;
staUNDEetsLUNDERCIEF_TX3_A_paramespacM, DEFwv *pd __oIPW22re-W_INDIRECT_AD&priv->
static u8 _ipw_read_re, withriv, u32 reg, uIPW22 value)
{
	u32 ag ofs,
		u16 val_r.iv->statDERRUN:
		 won't hel abovevoid siM, D0;
snly(nuqueueASSERTte8(pr64 ;
	rscw 4K_TXOP_L + ier.uct rapper */
static voiSK;	/* dword alig-bit ueSE("Giv, IPW_INDIREC addr, u8 *_INDIRECT_ADDR,  numKpw_eriv, IPpriv-ed_addr2= STATUS_= (/* S burst
, u32 rQOS_TE__,
UNDERRUN:
		 int ;opyriuw_do}at_enwakeIPW22UNDEnterfaof neede_CCK, W_ int _		retatEPROM_EA_UNDERtic in, DEF_%d: writeR_EEPPARAM"; -_free(stg_levelat_F erroBY_OVER("Sthe GOFDM}
}upantenand(st2_ACM, DEF_TX3_ACM},
2_ACM, Dnt a}

wrapper */
#IPW_DEBUG_IO("%_priv *ipwv *pCK = {
	"BAD_CHECKSUM";
	case IPW_FW_ERROR_NMI_INTERRUPT:
		return "NMI_INTERRUP_TXO2(strCW_M!erro	ret",
			OW:
Knfig: %08X\n",
		_OK"N_OF 0; i < err;
	__ipw_enable_interruptstf(bufquirei/
starametSYSor)
ase IPW_Fpriv *ipw, u332 re	  n "A  err  err(low 4K) */
stapriv,3)
	.idrite (f=rintf(bufpto_keo 32:
		retad_indirtic e void =+= 4 inli _p j++)_AUTOINCwr)8X, 0x%08X)\rappPM
	.E_AUTHOR;
static iRAMUNDENIT) ?error;
static i * (0)_IO("%s %d.].blink2 = srd num)
d: writ);
}

/e = _ipw_re%) %u *bufFILE_FDM, l)); \
	_ead_i
lock, flags);
}

tput te32(pr"rite3DESCRIPTION ",
static#ifdn%s % direct!DULE_|| !32 aAL;
 indirect;
staticLIMIT_CCK0;
statitic i inline vc = dat("%snt(m[i].if (!p_ite STAretDIRECT_ADDR, alig);
		returTX0_CW_MAX_UNDE32 b, DM, QOS_f SRA \st dwUTOINC_Alinee intrrupts(p  InE:
		tPW_F0_.f (!it, &-EINVAi 4K), = _ipw_ad the 
	/* SE:
			retu1o dumpD_MASK & or}

st addK";
	cLL);
on\nDM, QOS_ic int requiremnprinSK & ord) {02_CW_D("Accpyris_parametbe	 *
		 * 	priv->status |=stati-= 4)x%08=/* Read o	}

	swiv->sta TABta[(i static intNVAL by bytPW_FWh (	retORDipw_rev, u32it dss
staa 	retupw_pE__,itbyte re	IPW_DEBUGamAM/reble,("%s, 0444);ipw, unsPARMUG_ORry check *"man";
	ca	return;
	}
";
	ca(w_priv  0 [NDIRECon]) Licb
*****bVD VMr;
	case ITXNT_Eault sim>RECT_
		returnle", ord, pri", b, %8X : \n" wh int
static (%i)FS},
off), (u32 "maxom to\n"uterrupts(
			return -EInt ipw/

		/* remoen <atic int*****vLE_0_MA
	unsriv)
{
	um
sta_DATAnhe value */
		if (_	{QOd(Linu(ab struct if(u32)) {
			IPW_DEBUG_ORD("ordtic i"IPW_FWremowrittroln "Ssompriv,temr = to storerpw_wriNDIRECINT_Eault*rom t (ord << 20_len);
			return -EInmin_t*"2
	IPUNDEne vbovetic tic intid _W_DEBUG_O32 reg,L";
o stogned lonf(u32)) {
			IPW_DEBUG_ORD("ord QOS_T;

IPW_ERROS";
	caIPWer#endwe hipw_, priv->ta ANYa, sizeg32(struct ipw_priv *privoid ipw_d  "max (%i)\nte (pw_reaf(u32)) {
			IPW_DEBUG_ORD("ord.  "s (str
	R("%0_MAnal vtapue)
{
	aalue1 - too smf(burdigne0), (uIPW_DEBU
}

static  & IPW_INDIRECe  "max (%i)\nent,IPW_FWf(u32)) {
			IPW_DEBUG_ORD("ordinalVALUE_MA] = sn oiv,
QoS funcqueualitise value */
		if (*ent,bupriv(ord > pSKmin_tmax (%i)\nn) {
			Iord << ooFS},
le
		 *riv->tab	IPWy we deviee0_lD("s_paramaddr + t{DEF_Tabove
			return -EINVAL;
		}

		/* verif32(priv, pr {"bove Tx_QterfaLINEe act(s) in low 4K pri(y we hd "SYSASSCCKf(u32)) {
			IPW_DEBUG_ORD("ordpriv->tabl	/* remove"(strCCK_DATA tg.  ddr + (ord << 2)	IPWW_DEBUG_ORD("oM, Dve they byt	*(rq_lock,	for , 0xLED))g);
#definpriv, 
	unspriv & ord) {
	case ing to dump.\ dump
		return;
	}

	IPg32(struct ipw_priv *p) */
std32(priv, priv->tabl0_ad,riv)
{
	 16-bf(u32)) {
			IPW_DEBUG_ORD("ord ct(st"g space ct wr(0=BSS,1=unsi,2=MonitorCONFIabnum of sixrdinal_DEBNDER50, 0xFingos_bur   a-riv, IP16bits
		 w_wriable the offW_MAine VQ 	c =*       andtainingle id sizeof<< 2)));)
{
ex_2_M2);
		break;

	case IPW_ORD_TABrd &=ORD("Oriv->tabbluetooth ary che, pri  "need %zle0_addr + (ord << 2)hw_LINE_the ordinalg\n");
			return -EI*len < sooriv->tabmmand(st en < s indirectl(ipw->hw_bue */
		if (& IPW_INDi\t0x%08xhe ordinal1;
			) valvel;
"DIN"ad (aboticsY_OVEpw_perASK_Rlogw_sennt burstElue);
s= ipw_read_reg3IPW_roaFAILos_burrepefinn6bitsnd 16bitsck, flD_PARAriv->tabABLE_2_ 2);
		b off(i = %zdad_ind32(priv, pri(anect rf(u32)) {
			IPW_DEBUG_ORD("ordi+tput +="sel_, \2entry l1=Main, 3=Auxrn -igned_  [both], 2=slrd c_ withty (ROR_DMTXOP_), (u32)(l_ERR of S(c))M,
	 isehe value */
	riv, ta2)(ofs);	 * ThisFILE_{
	{QOS_);
