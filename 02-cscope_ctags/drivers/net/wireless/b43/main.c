/*

  Broadcom B43 wireless driver

  Copyright (c) 2005 Martin Langer <martin-langer@gmx.de>
  Copyright (c) 2005 Stefano Brivio <stefano.brivio@polimi.it>
  Copyright (c) 2005-2009 Michael Buesch <mb@bu3sch.de>
  Copyright (c) 2005 Danny van Dyk <kugelfang@gentoo.org>
  Copyright (c) 2005 Andreas Jaggi <andreas.jaggi@waterwave.ch>

  SDIO support
  Copyright (c) 2009 Albert Herranz <albert_herranz@yahoo.es>

  Some parts of the code in this file are derived from the ipw2200
  driver  Copyright(c) 2003 - 2004 Intel Corporation.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
  Boston, MA 02110-1301, USA.

*/

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/firmware.h>
#include <linux/wireless.h>
#include <linux/workqueue.h>
#include <linux/skbuff.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <asm/unaligned.h>

#include "b43.h"
#include "main.h"
#include "debugfs.h"
#include "phy_common.h"
#include "phy_g.h"
#include "phy_n.h"
#include "dma.h"
#include "pio.h"
#include "sysfs.h"
#include "xmit.h"
#include "lo.h"
#include "pcmcia.h"
#include "sdio.h"
#include <linux/mmc/sdio_func.h>

MODULE_DESCRIPTION("Broadcom B43 wireless driver");
MODULE_AUTHOR("Martin Langer");
MODULE_AUTHOR("Stefano Brivio");
MODULE_AUTHOR("Michael Buesch");
MODULE_AUTHOR("Gábor Stefanik");
MODULE_LICENSE("GPL");

MODULE_FIRMWARE(B43_SUPPORTED_FIRMWARE_ID);


static int modparam_bad_frames_preempt;
module_param_named(bad_frames_preempt, modparam_bad_frames_preempt, int, 0444);
MODULE_PARM_DESC(bad_frames_preempt,
		 "enable(1) / disable(0) Bad Frames Preemption");

static char modparam_fwpostfix[16];
module_param_string(fwpostfix, modparam_fwpostfix, 16, 0444);
MODULE_PARM_DESC(fwpostfix, "Postfix for the .fw files to load.");

static int modparam_hwpctl;
module_param_named(hwpctl, modparam_hwpctl, int, 0444);
MODULE_PARM_DESC(hwpctl, "Enable hardware-side power control (default off)");

static int modparam_nohwcrypt;
module_param_named(nohwcrypt, modparam_nohwcrypt, int, 0444);
MODULE_PARM_DESC(nohwcrypt, "Disable hardware encryption.");

static int modparam_hwtkip;
module_param_named(hwtkip, modparam_hwtkip, int, 0444);
MODULE_PARM_DESC(hwtkip, "Enable hardware tkip.");

static int modparam_qos = 1;
module_param_named(qos, modparam_qos, int, 0444);
MODULE_PARM_DESC(qos, "Enable QOS support (default on)");

static int modparam_btcoex = 1;
module_param_named(btcoex, modparam_btcoex, int, 0444);
MODULE_PARM_DESC(btcoex, "Enable Bluetooth coexistence (default on)");

int b43_modparam_verbose = B43_VERBOSITY_DEFAULT;
module_param_named(verbose, b43_modparam_verbose, int, 0644);
MODULE_PARM_DESC(verbose, "Log message verbosity: 0=error, 1=warn, 2=info(default), 3=debug");


static const struct ssb_device_id b43_ssb_tbl[] = {
	SSB_DEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_80211, 5),
	SSB_DEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_80211, 6),
	SSB_DEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_80211, 7),
	SSB_DEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_80211, 9),
	SSB_DEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_80211, 10),
	SSB_DEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_80211, 11),
	SSB_DEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_80211, 13),
	SSB_DEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_80211, 15),
	SSB_DEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_80211, 16),
	SSB_DEVTABLE_END
};

MODULE_DEVICE_TABLE(ssb, b43_ssb_tbl);

/* Channel and ratetables are shared for all devices.
 * They can't be const, because ieee80211 puts some precalculated
 * data in there. This data is the same for all devices, so we don't
 * get concurrency issues */
#define RATETAB_ENT(_rateid, _flags) \
	{								\
		.bitrate	= B43_RATE_TO_BASE100KBPS(_rateid),	\
		.hw_value	= (_rateid),				\
		.flags		= (_flags),				\
	}

/*
 * NOTE: When changing this, sync with xmit.c's
 *	 b43_plcp_get_bitrate_idx_* functions!
 */
static struct ieee80211_rate __b43_ratetable[] = {
	RATETAB_ENT(B43_CCK_RATE_1MB, 0),
	RATETAB_ENT(B43_CCK_RATE_2MB, IEEE80211_RATE_SHORT_PREAMBLE),
	RATETAB_ENT(B43_CCK_RATE_5MB, IEEE80211_RATE_SHORT_PREAMBLE),
	RATETAB_ENT(B43_CCK_RATE_11MB, IEEE80211_RATE_SHORT_PREAMBLE),
	RATETAB_ENT(B43_OFDM_RATE_6MB, 0),
	RATETAB_ENT(B43_OFDM_RATE_9MB, 0),
	RATETAB_ENT(B43_OFDM_RATE_12MB, 0),
	RATETAB_ENT(B43_OFDM_RATE_18MB, 0),
	RATETAB_ENT(B43_OFDM_RATE_24MB, 0),
	RATETAB_ENT(B43_OFDM_RATE_36MB, 0),
	RATETAB_ENT(B43_OFDM_RATE_48MB, 0),
	RATETAB_ENT(B43_OFDM_RATE_54MB, 0),
};

#define b43_a_ratetable		(__b43_ratetable + 4)
#define b43_a_ratetable_size	8
#define b43_b_ratetable		(__b43_ratetable + 0)
#define b43_b_ratetable_size	4
#define b43_g_ratetable		(__b43_ratetable + 0)
#define b43_g_ratetable_size	12

#define CHAN4G(_channel, _freq, _flags) {			\
	.band			= IEEE80211_BAND_2GHZ,		\
	.center_freq		= (_freq),			\
	.hw_value		= (_channel),			\
	.flags			= (_flags),			\
	.max_antenna_gain	= 0,				\
	.max_power		= 30,				\
}
static struct ieee80211_channel b43_2ghz_chantable[] = {
	CHAN4G(1, 2412, 0),
	CHAN4G(2, 2417, 0),
	CHAN4G(3, 2422, 0),
	CHAN4G(4, 2427, 0),
	CHAN4G(5, 2432, 0),
	CHAN4G(6, 2437, 0),
	CHAN4G(7, 2442, 0),
	CHAN4G(8, 2447, 0),
	CHAN4G(9, 2452, 0),
	CHAN4G(10, 2457, 0),
	CHAN4G(11, 2462, 0),
	CHAN4G(12, 2467, 0),
	CHAN4G(13, 2472, 0),
	CHAN4G(14, 2484, 0),
};
#undef CHAN4G

#define CHAN5G(_channel, _flags) {				\
	.band			= IEEE80211_BAND_5GHZ,		\
	.center_freq		= 5000 + (5 * (_channel)),	\
	.hw_value		= (_channel),			\
	.flags			= (_flags),			\
	.max_antenna_gain	= 0,				\
	.max_power		= 30,				\
}
static struct ieee80211_channel b43_5ghz_nphy_chantable[] = {
	CHAN5G(32, 0),		CHAN5G(34, 0),
	CHAN5G(36, 0),		CHAN5G(38, 0),
	CHAN5G(40, 0),		CHAN5G(42, 0),
	CHAN5G(44, 0),		CHAN5G(46, 0),
	CHAN5G(48, 0),		CHAN5G(50, 0),
	CHAN5G(52, 0),		CHAN5G(54, 0),
	CHAN5G(56, 0),		CHAN5G(58, 0),
	CHAN5G(60, 0),		CHAN5G(62, 0),
	CHAN5G(64, 0),		CHAN5G(66, 0),
	CHAN5G(68, 0),		CHAN5G(70, 0),
	CHAN5G(72, 0),		CHAN5G(74, 0),
	CHAN5G(76, 0),		CHAN5G(78, 0),
	CHAN5G(80, 0),		CHAN5G(82, 0),
	CHAN5G(84, 0),		CHAN5G(86, 0),
	CHAN5G(88, 0),		CHAN5G(90, 0),
	CHAN5G(92, 0),		CHAN5G(94, 0),
	CHAN5G(96, 0),		CHAN5G(98, 0),
	CHAN5G(100, 0),		CHAN5G(102, 0),
	CHAN5G(104, 0),		CHAN5G(106, 0),
	CHAN5G(108, 0),		CHAN5G(110, 0),
	CHAN5G(112, 0),		CHAN5G(114, 0),
	CHAN5G(116, 0),		CHAN5G(118, 0),
	CHAN5G(120, 0),		CHAN5G(122, 0),
	CHAN5G(124, 0),		CHAN5G(126, 0),
	CHAN5G(128, 0),		CHAN5G(130, 0),
	CHAN5G(132, 0),		CHAN5G(134, 0),
	CHAN5G(136, 0),		CHAN5G(138, 0),
	CHAN5G(140, 0),		CHAN5G(142, 0),
	CHAN5G(144, 0),		CHAN5G(145, 0),
	CHAN5G(146, 0),		CHAN5G(147, 0),
	CHAN5G(148, 0),		CHAN5G(149, 0),
	CHAN5G(150, 0),		CHAN5G(151, 0),
	CHAN5G(152, 0),		CHAN5G(153, 0),
	CHAN5G(154, 0),		CHAN5G(155, 0),
	CHAN5G(156, 0),		CHAN5G(157, 0),
	CHAN5G(158, 0),		CHAN5G(159, 0),
	CHAN5G(160, 0),		CHAN5G(161, 0),
	CHAN5G(162, 0),		CHAN5G(163, 0),
	CHAN5G(164, 0),		CHAN5G(165, 0),
	CHAN5G(166, 0),		CHAN5G(168, 0),
	CHAN5G(170, 0),		CHAN5G(172, 0),
	CHAN5G(174, 0),		CHAN5G(176, 0),
	CHAN5G(178, 0),		CHAN5G(180, 0),
	CHAN5G(182, 0),		CHAN5G(184, 0),
	CHAN5G(186, 0),		CHAN5G(188, 0),
	CHAN5G(190, 0),		CHAN5G(192, 0),
	CHAN5G(194, 0),		CHAN5G(196, 0),
	CHAN5G(198, 0),		CHAN5G(200, 0),
	CHAN5G(202, 0),		CHAN5G(204, 0),
	CHAN5G(206, 0),		CHAN5G(208, 0),
	CHAN5G(210, 0),		CHAN5G(212, 0),
	CHAN5G(214, 0),		CHAN5G(216, 0),
	CHAN5G(218, 0),		CHAN5G(220, 0),
	CHAN5G(222, 0),		CHAN5G(224, 0),
	CHAN5G(226, 0),		CHAN5G(228, 0),
};

static struct ieee80211_channel b43_5ghz_aphy_chantable[] = {
	CHAN5G(34, 0),		CHAN5G(36, 0),
	CHAN5G(38, 0),		CHAN5G(40, 0),
	CHAN5G(42, 0),		CHAN5G(44, 0),
	CHAN5G(46, 0),		CHAN5G(48, 0),
	CHAN5G(52, 0),		CHAN5G(56, 0),
	CHAN5G(60, 0),		CHAN5G(64, 0),
	CHAN5G(100, 0),		CHAN5G(104, 0),
	CHAN5G(108, 0),		CHAN5G(112, 0),
	CHAN5G(116, 0),		CHAN5G(120, 0),
	CHAN5G(124, 0),		CHAN5G(128, 0),
	CHAN5G(132, 0),		CHAN5G(136, 0),
	CHAN5G(140, 0),		CHAN5G(149, 0),
	CHAN5G(153, 0),		CHAN5G(157, 0),
	CHAN5G(161, 0),		CHAN5G(165, 0),
	CHAN5G(184, 0),		CHAN5G(188, 0),
	CHAN5G(192, 0),		CHAN5G(196, 0),
	CHAN5G(200, 0),		CHAN5G(204, 0),
	CHAN5G(208, 0),		CHAN5G(212, 0),
	CHAN5G(216, 0),
};
#undef CHAN5G

static struct ieee80211_supported_band b43_band_5GHz_nphy = {
	.band		= IEEE80211_BAND_5GHZ,
	.channels	= b43_5ghz_nphy_chantable,
	.n_channels	= ARRAY_SIZE(b43_5ghz_nphy_chantable),
	.bitrates	= b43_a_ratetable,
	.n_bitrates	= b43_a_ratetable_size,
};

static struct ieee80211_supported_band b43_band_5GHz_aphy = {
	.band		= IEEE80211_BAND_5GHZ,
	.channels	= b43_5ghz_aphy_chantable,
	.n_channels	= ARRAY_SIZE(b43_5ghz_aphy_chantable),
	.bitrates	= b43_a_ratetable,
	.n_bitrates	= b43_a_ratetable_size,
};

static struct ieee80211_supported_band b43_band_2GHz = {
	.band		= IEEE80211_BAND_2GHZ,
	.channels	= b43_2ghz_chantable,
	.n_channels	= ARRAY_SIZE(b43_2ghz_chantable),
	.bitrates	= b43_g_ratetable,
	.n_bitrates	= b43_g_ratetable_size,
};

static void b43_wireless_core_exit(struct b43_wldev *dev);
static int b43_wireless_core_init(struct b43_wldev *dev);
static struct b43_wldev * b43_wireless_core_stop(struct b43_wldev *dev);
static int b43_wireless_core_start(struct b43_wldev *dev);

static int b43_ratelimit(struct b43_wl *wl)
{
	if (!wl || !wl->current_dev)
		return 1;
	if (b43_status(wl->current_dev) < B43_STAT_STARTED)
		return 1;
	/* We are up and running.
	 * Ratelimit the messages to avoid DoS over the net. */
	return net_ratelimit();
}

void b43info(struct b43_wl *wl, const char *fmt, ...)
{
	va_list args;

	if (b43_modparam_verbose < B43_VERBOSITY_INFO)
		return;
	if (!b43_ratelimit(wl))
		return;
	va_start(args, fmt);
	printk(KERN_INFO "b43-%s: ",
	       (wl && wl->hw) ? wiphy_name(wl->hw->wiphy) : "wlan");
	vprintk(fmt, args);
	va_end(args);
}

void b43err(struct b43_wl *wl, const char *fmt, ...)
{
	va_list args;

	if (b43_modparam_verbose < B43_VERBOSITY_ERROR)
		return;
	if (!b43_ratelimit(wl))
		return;
	va_start(args, fmt);
	printk(KERN_ERR "b43-%s ERROR: ",
	       (wl && wl->hw) ? wiphy_name(wl->hw->wiphy) : "wlan");
	vprintk(fmt, args);
	va_end(args);
}

void b43warn(struct b43_wl *wl, const char *fmt, ...)
{
	va_list args;

	if (b43_modparam_verbose < B43_VERBOSITY_WARN)
		return;
	if (!b43_ratelimit(wl))
		return;
	va_start(args, fmt);
	printk(KERN_WARNING "b43-%s warning: ",
	       (wl && wl->hw) ? wiphy_name(wl->hw->wiphy) : "wlan");
	vprintk(fmt, args);
	va_end(args);
}

void b43dbg(struct b43_wl *wl, const char *fmt, ...)
{
	va_list args;

	if (b43_modparam_verbose < B43_VERBOSITY_DEBUG)
		return;
	va_start(args, fmt);
	printk(KERN_DEBUG "b43-%s debug: ",
	       (wl && wl->hw) ? wiphy_name(wl->hw->wiphy) : "wlan");
	vprintk(fmt, args);
	va_end(args);
}

static void b43_ram_write(struct b43_wldev *dev, u16 offset, u32 val)
{
	u32 macctl;

	B43_WARN_ON(offset % 4 != 0);

	macctl = b43_read32(dev, B43_MMIO_MACCTL);
	if (macctl & B43_MACCTL_BE)
		val = swab32(val);

	b43_write32(dev, B43_MMIO_RAM_CONTROL, offset);
	mmiowb();
	b43_write32(dev, B43_MMIO_RAM_DATA, val);
}

static inline void b43_shm_control_word(struct b43_wldev *dev,
					u16 routing, u16 offset)
{
	u32 control;

	/* "offset" is the WORD offset. */
	control = routing;
	control <<= 16;
	control |= offset;
	b43_write32(dev, B43_MMIO_SHM_CONTROL, control);
}

u32 b43_shm_read32(struct b43_wldev *dev, u16 routing, u16 offset)
{
	u32 ret;

	if (routing == B43_SHM_SHARED) {
		B43_WARN_ON(offset & 0x0001);
		if (offset & 0x0003) {
			/* Unaligned access */
			b43_shm_control_word(dev, routing, offset >> 2);
			ret = b43_read16(dev, B43_MMIO_SHM_DATA_UNALIGNED);
			b43_shm_control_word(dev, routing, (offset >> 2) + 1);
			ret |= ((u32)b43_read16(dev, B43_MMIO_SHM_DATA)) << 16;

			goto out;
		}
		offset >>= 2;
	}
	b43_shm_control_word(dev, routing, offset);
	ret = b43_read32(dev, B43_MMIO_SHM_DATA);
out:
	return ret;
}

u16 b43_shm_read16(struct b43_wldev *dev, u16 routing, u16 offset)
{
	u16 ret;

	if (routing == B43_SHM_SHARED) {
		B43_WARN_ON(offset & 0x0001);
		if (offset & 0x0003) {
			/* Unaligned access */
			b43_shm_control_word(dev, routing, offset >> 2);
			ret = b43_read16(dev, B43_MMIO_SHM_DATA_UNALIGNED);

			goto out;
		}
		offset >>= 2;
	}
	b43_shm_control_word(dev, routing, offset);
	ret = b43_read16(dev, B43_MMIO_SHM_DATA);
out:
	return ret;
}

void b43_shm_write32(struct b43_wldev *dev, u16 routing, u16 offset, u32 value)
{
	if (routing == B43_SHM_SHARED) {
		B43_WARN_ON(offset & 0x0001);
		if (offset & 0x0003) {
			/* Unaligned access */
			b43_shm_control_word(dev, routing, offset >> 2);
			b43_write16(dev, B43_MMIO_SHM_DATA_UNALIGNED,
				    value & 0xFFFF);
			b43_shm_control_word(dev, routing, (offset >> 2) + 1);
			b43_write16(dev, B43_MMIO_SHM_DATA,
				    (value >> 16) & 0xFFFF);
			return;
		}
		offset >>= 2;
	}
	b43_shm_control_word(dev, routing, offset);
	b43_write32(dev, B43_MMIO_SHM_DATA, value);
}

void b43_shm_write16(struct b43_wldev *dev, u16 routing, u16 offset, u16 value)
{
	if (routing == B43_SHM_SHARED) {
		B43_WARN_ON(offset & 0x0001);
		if (offset & 0x0003) {
			/* Unaligned access */
			b43_shm_control_word(dev, routing, offset >> 2);
			b43_write16(dev, B43_MMIO_SHM_DATA_UNALIGNED, value);
			return;
		}
		offset >>= 2;
	}
	b43_shm_control_word(dev, routing, offset);
	b43_write16(dev, B43_MMIO_SHM_DATA, value);
}

/* Read HostFlags */
u64 b43_hf_read(struct b43_wldev *dev)
{
	u64 ret;

	ret = b43_shm_read16(dev, B43_SHM_SHARED, B43_SHM_SH_HOSTFHI);
	ret <<= 16;
	ret |= b43_shm_read16(dev, B43_SHM_SHARED, B43_SHM_SH_HOSTFMI);
	ret <<= 16;
	ret |= b43_shm_read16(dev, B43_SHM_SHARED, B43_SHM_SH_HOSTFLO);

	return ret;
}

/* Write HostFlags */
void b43_hf_write(struct b43_wldev *dev, u64 value)
{
	u16 lo, mi, hi;

	lo = (value & 0x00000000FFFFULL);
	mi = (value & 0x0000FFFF0000ULL) >> 16;
	hi = (value & 0xFFFF00000000ULL) >> 32;
	b43_shm_write16(dev, B43_SHM_SHARED, B43_SHM_SH_HOSTFLO, lo);
	b43_shm_write16(dev, B43_SHM_SHARED, B43_SHM_SH_HOSTFMI, mi);
	b43_shm_write16(dev, B43_SHM_SHARED, B43_SHM_SH_HOSTFHI, hi);
}

/* Read the firmware capabilities bitmask (Opensource firmware only) */
static u16 b43_fwcapa_read(struct b43_wldev *dev)
{
	B43_WARN_ON(!dev->fw.opensource);
	return b43_shm_read16(dev, B43_SHM_SHARED, B43_SHM_SH_FWCAPA);
}

void b43_tsf_read(struct b43_wldev *dev, u64 *tsf)
{
	u32 low, high;

	B43_WARN_ON(dev->dev->id.revision < 3);

	/* The hardware guarantees us an atomic read, if we
	 * read the low register first. */
	low = b43_read32(dev, B43_MMIO_REV3PLUS_TSF_LOW);
	high = b43_read32(dev, B43_MMIO_REV3PLUS_TSF_HIGH);

	*tsf = high;
	*tsf <<= 32;
	*tsf |= low;
}

static void b43_time_lock(struct b43_wldev *dev)
{
	u32 macctl;

	macctl = b43_read32(dev, B43_MMIO_MACCTL);
	macctl |= B43_MACCTL_TBTTHOLD;
	b43_write32(dev, B43_MMIO_MACCTL, macctl);
	/* Commit the write */
	b43_read32(dev, B43_MMIO_MACCTL);
}

static void b43_time_unlock(struct b43_wldev *dev)
{
	u32 macctl;

	macctl = b43_read32(dev, B43_MMIO_MACCTL);
	macctl &= ~B43_MACCTL_TBTTHOLD;
	b43_write32(dev, B43_MMIO_MACCTL, macctl);
	/* Commit the write */
	b43_read32(dev, B43_MMIO_MACCTL);
}

static void b43_tsf_write_locked(struct b43_wldev *dev, u64 tsf)
{
	u32 low, high;

	B43_WARN_ON(dev->dev->id.revision < 3);

	low = tsf;
	high = (tsf >> 32);
	/* The hardware guarantees us an atomic write, if we
	 * write the low register first. */
	b43_write32(dev, B43_MMIO_REV3PLUS_TSF_LOW, low);
	mmiowb();
	b43_write32(dev, B43_MMIO_REV3PLUS_TSF_HIGH, high);
	mmiowb();
}

void b43_tsf_write(struct b43_wldev *dev, u64 tsf)
{
	b43_time_lock(dev);
	b43_tsf_write_locked(dev, tsf);
	b43_time_unlock(dev);
}

static
void b43_macfilter_set(struct b43_wldev *dev, u16 offset, const u8 *mac)
{
	static const u8 zero_addr[ETH_ALEN] = { 0 };
	u16 data;

	if (!mac)
		mac = zero_addr;

	offset |= 0x0020;
	b43_write16(dev, B43_MMIO_MACFILTER_CONTROL, offset);

	data = mac[0];
	data |= mac[1] << 8;
	b43_write16(dev, B43_MMIO_MACFILTER_DATA, data);
	data = mac[2];
	data |= mac[3] << 8;
	b43_write16(dev, B43_MMIO_MACFILTER_DATA, data);
	data = mac[4];
	data |= mac[5] << 8;
	b43_write16(dev, B43_MMIO_MACFILTER_DATA, data);
}

static void b43_write_mac_bssid_templates(struct b43_wldev *dev)
{
	const u8 *mac;
	const u8 *bssid;
	u8 mac_bssid[ETH_ALEN * 2];
	int i;
	u32 tmp;

	bssid = dev->wl->bssid;
	mac = dev->wl->mac_addr;

	b43_macfilter_set(dev, B43_MACFILTER_BSSID, bssid);

	memcpy(mac_bssid, mac, ETH_ALEN);
	memcpy(mac_bssid + ETH_ALEN, bssid, ETH_ALEN);

	/* Write our MAC address and BSSID to template ram */
	for (i = 0; i < ARRAY_SIZE(mac_bssid); i += sizeof(u32)) {
		tmp = (u32) (mac_bssid[i + 0]);
		tmp |= (u32) (mac_bssid[i + 1]) << 8;
		tmp |= (u32) (mac_bssid[i + 2]) << 16;
		tmp |= (u32) (mac_bssid[i + 3]) << 24;
		b43_ram_write(dev, 0x20 + i, tmp);
	}
}

static void b43_upload_card_macaddress(struct b43_wldev *dev)
{
	b43_write_mac_bssid_templates(dev);
	b43_macfilter_set(dev, B43_MACFILTER_SELF, dev->wl->mac_addr);
}

static void b43_set_slot_time(struct b43_wldev *dev, u16 slot_time)
{
	/* slot_time is in usec. */
	if (dev->phy.type != B43_PHYTYPE_G)
		return;
	b43_write16(dev, 0x684, 510 + slot_time);
	b43_shm_write16(dev, B43_SHM_SHARED, 0x0010, slot_time);
}

static void b43_short_slot_timing_enable(struct b43_wldev *dev)
{
	b43_set_slot_time(dev, 9);
}

static void b43_short_slot_timing_disable(struct b43_wldev *dev)
{
	b43_set_slot_time(dev, 20);
}

/* DummyTransmission function, as documented on
 * http://bcm-v4.sipsolutions.net/802.11/DummyTransmission
 */
void b43_dummy_transmission(struct b43_wldev *dev, bool ofdm, bool pa_on)
{
	struct b43_phy *phy = &dev->phy;
	unsigned int i, max_loop;
	u16 value;
	u32 buffer[5] = {
		0x00000000,
		0x00D40000,
		0x00000000,
		0x01000000,
		0x00000000,
	};

	if (ofdm) {
		max_loop = 0x1E;
		buffer[0] = 0x000201CC;
	} else {
		max_loop = 0xFA;
		buffer[0] = 0x000B846E;
	}

	for (i = 0; i < 5; i++)
		b43_ram_write(dev, i * 4, buffer[i]);

	b43_write16(dev, 0x0568, 0x0000);
	if (dev->dev->id.revision < 11)
		b43_write16(dev, 0x07C0, 0x0000);
	else
		b43_write16(dev, 0x07C0, 0x0100);
	value = (ofdm ? 0x41 : 0x40);
	b43_write16(dev, 0x050C, value);
	if ((phy->type == B43_PHYTYPE_N) || (phy->type == B43_PHYTYPE_LP))
		b43_write16(dev, 0x0514, 0x1A02);
	b43_write16(dev, 0x0508, 0x0000);
	b43_write16(dev, 0x050A, 0x0000);
	b43_write16(dev, 0x054C, 0x0000);
	b43_write16(dev, 0x056A, 0x0014);
	b43_write16(dev, 0x0568, 0x0826);
	b43_write16(dev, 0x0500, 0x0000);
	if (!pa_on && (phy->type == B43_PHYTYPE_N)) {
		//SPEC TODO
	}

	switch (phy->type) {
	case B43_PHYTYPE_N:
		b43_write16(dev, 0x0502, 0x00D0);
		break;
	case B43_PHYTYPE_LP:
		b43_write16(dev, 0x0502, 0x0050);
		break;
	default:
		b43_write16(dev, 0x0502, 0x0030);
	}

	if (phy->radio_ver == 0x2050 && phy->radio_rev <= 0x5)
		b43_radio_write16(dev, 0x0051, 0x0017);
	for (i = 0x00; i < max_loop; i++) {
		value = b43_read16(dev, 0x050E);
		if (value & 0x0080)
			break;
		udelay(10);
	}
	for (i = 0x00; i < 0x0A; i++) {
		value = b43_read16(dev, 0x050E);
		if (value & 0x0400)
			break;
		udelay(10);
	}
	for (i = 0x00; i < 0x19; i++) {
		value = b43_read16(dev, 0x0690);
		if (!(value & 0x0100))
			break;
		udelay(10);
	}
	if (phy->radio_ver == 0x2050 && phy->radio_rev <= 0x5)
		b43_radio_write16(dev, 0x0051, 0x0037);
}

static void key_write(struct b43_wldev *dev,
		      u8 index, u8 algorithm, const u8 *key)
{
	unsigned int i;
	u32 offset;
	u16 value;
	u16 kidx;

	/* Key index/algo block */
	kidx = b43_kidx_to_fw(dev, index);
	value = ((kidx << 4) | algorithm);
	b43_shm_write16(dev, B43_SHM_SHARED,
			B43_SHM_SH_KEYIDXBLOCK + (kidx * 2), value);

	/* Write the key to the Key Table Pointer offset */
	offset = dev->ktp + (index * B43_SEC_KEYSIZE);
	for (i = 0; i < B43_SEC_KEYSIZE; i += 2) {
		value = key[i];
		value |= (u16) (key[i + 1]) << 8;
		b43_shm_write16(dev, B43_SHM_SHARED, offset + i, value);
	}
}

static void keymac_write(struct b43_wldev *dev, u8 index, const u8 *addr)
{
	u32 addrtmp[2] = { 0, 0, };
	u8 pairwise_keys_start = B43_NR_GROUP_KEYS * 2;

	if (b43_new_kidx_api(dev))
		pairwise_keys_start = B43_NR_GROUP_KEYS;

	B43_WARN_ON(index < pairwise_keys_start);
	/* We have four default TX keys and possibly four default RX keys.
	 * Physical mac 0 is mapped to physical key 4 or 8, depending
	 * on the firmware version.
	 * So we must adjust the index here.
	 */
	index -= pairwise_keys_start;
	B43_WARN_ON(index >= B43_NR_PAIRWISE_KEYS);

	if (addr) {
		addrtmp[0] = addr[0];
		addrtmp[0] |= ((u32) (addr[1]) << 8);
		addrtmp[0] |= ((u32) (addr[2]) << 16);
		addrtmp[0] |= ((u32) (addr[3]) << 24);
		addrtmp[1] = addr[4];
		addrtmp[1] |= ((u32) (addr[5]) << 8);
	}

	/* Receive match transmitter address (RCMTA) mechanism */
	b43_shm_write32(dev, B43_SHM_RCMTA,
			(index * 2) + 0, addrtmp[0]);
	b43_shm_write16(dev, B43_SHM_RCMTA,
			(index * 2) + 1, addrtmp[1]);
}

/* The ucode will use phase1 key with TEK key to decrypt rx packets.
 * When a packet is received, the iv32 is checked.
 * - if it doesn't the packet is returned without modification (and software
 *   decryption can be done). That's what happen when iv16 wrap.
 * - if it does, the rc4 key is computed, and decryption is tried.
 *   Either it will success and B43_RX_MAC_DEC is returned,
 *   either it fails and B43_RX_MAC_DEC|B43_RX_MAC_DECERR is returned
 *   and the packet is not usable (it got modified by the ucode).
 * So in order to never have B43_RX_MAC_DECERR, we should provide
 * a iv32 and phase1key that match. Because we drop packets in case of
 * B43_RX_MAC_DECERR, if we have a correct iv32 but a wrong phase1key, all
 * packets will be lost without higher layer knowing (ie no resync possible
 * until next wrap).
 *
 * NOTE : this should support 50 key like RCMTA because
 * (B43_SHM_SH_KEYIDXBLOCK - B43_SHM_SH_TKIPTSCTTAK)/14 = 50
 */
static void rx_tkip_phase1_write(struct b43_wldev *dev, u8 index, u32 iv32,
		u16 *phase1key)
{
	unsigned int i;
	u32 offset;
	u8 pairwise_keys_start = B43_NR_GROUP_KEYS * 2;

	if (!modparam_hwtkip)
		return;

	if (b43_new_kidx_api(dev))
		pairwise_keys_start = B43_NR_GROUP_KEYS;

	B43_WARN_ON(index < pairwise_keys_start);
	/* We have four default TX keys and possibly four default RX keys.
	 * Physical mac 0 is mapped to physical key 4 or 8, depending
	 * on the firmware version.
	 * So we must adjust the index here.
	 */
	index -= pairwise_keys_start;
	B43_WARN_ON(index >= B43_NR_PAIRWISE_KEYS);

	if (b43_debug(dev, B43_DBG_KEYS)) {
		b43dbg(dev->wl, "rx_tkip_phase1_write : idx 0x%x, iv32 0x%x\n",
				index, iv32);
	}
	/* Write the key to the  RX tkip shared mem */
	offset = B43_SHM_SH_TKIPTSCTTAK + index * (10 + 4);
	for (i = 0; i < 10; i += 2) {
		b43_shm_write16(dev, B43_SHM_SHARED, offset + i,
				phase1key ? phase1key[i / 2] : 0);
	}
	b43_shm_write16(dev, B43_SHM_SHARED, offset + i, iv32);
	b43_shm_write16(dev, B43_SHM_SHARED, offset + i + 2, iv32 >> 16);
}

static void b43_op_update_tkip_key(struct ieee80211_hw *hw,
			struct ieee80211_key_conf *keyconf, const u8 *addr,
			u32 iv32, u16 *phase1key)
{
	struct b43_wl *wl = hw_to_b43_wl(hw);
	struct b43_wldev *dev;
	int index = keyconf->hw_key_idx;

	if (B43_WARN_ON(!modparam_hwtkip))
		return;

	mutex_lock(&wl->mutex);

	dev = wl->current_dev;
	if (!dev || b43_status(dev) < B43_STAT_INITIALIZED)
		goto out_unlock;

	keymac_write(dev, index, NULL);	/* First zero out mac to avoid race */

	rx_tkip_phase1_write(dev, index, iv32, phase1key);
	keymac_write(dev, index, addr);

out_unlock:
	mutex_unlock(&wl->mutex);
}

static void do_key_write(struct b43_wldev *dev,
			 u8 index, u8 algorithm,
			 const u8 *key, size_t key_len, const u8 *mac_addr)
{
	u8 buf[B43_SEC_KEYSIZE] = { 0, };
	u8 pairwise_keys_start = B43_NR_GROUP_KEYS * 2;

	if (b43_new_kidx_api(dev))
		pairwise_keys_start = B43_NR_GROUP_KEYS;

	B43_WARN_ON(index >= ARRAY_SIZE(dev->key));
	B43_WARN_ON(key_len > B43_SEC_KEYSIZE);

	if (index >= pairwise_keys_start)
		keymac_write(dev, index, NULL);	/* First zero out mac. */
	if (algorithm == B43_SEC_ALGO_TKIP) {
		/*
		 * We should provide an initial iv32, phase1key pair.
		 * We could start with iv32=0 and compute the corresponding
		 * phase1key, but this means calling ieee80211_get_tkip_key
		 * with a fake skb (or export other tkip function).
		 * Because we are lazy we hope iv32 won't start with
		 * 0xffffffff and let's b43_op_update_tkip_key provide a
		 * correct pair.
		 */
		rx_tkip_phase1_write(dev, index, 0xffffffff, (u16*)buf);
	} else if (index >= pairwise_keys_start) /* clear it */
		rx_tkip_phase1_write(dev, index, 0, NULL);
	if (key)
		memcpy(buf, key, key_len);
	key_write(dev, index, algorithm, buf);
	if (index >= pairwise_keys_start)
		keymac_write(dev, index, mac_addr);

	dev->key[index].algorithm = algorithm;
}

static int b43_key_write(struct b43_wldev *dev,
			 int index, u8 algorithm,
			 const u8 *key, size_t key_len,
			 const u8 *mac_addr,
			 struct ieee80211_key_conf *keyconf)
{
	int i;
	int pairwise_keys_start;

	/* For ALG_TKIP the key is encoded as a 256-bit (32 byte) data block:
	 * 	- Temporal Encryption Key (128 bits)
	 * 	- Temporal Authenticator Tx MIC Key (64 bits)
	 * 	- Temporal Authenticator Rx MIC Key (64 bits)
	 *
	 * 	Hardware only store TEK
	 */
	if (algorithm == B43_SEC_ALGO_TKIP && key_len == 32)
		key_len = 16;
	if (key_len > B43_SEC_KEYSIZE)
		return -EINVAL;
	for (i = 0; i < ARRAY_SIZE(dev->key); i++) {
		/* Check that we don't already have this key. */
		B43_WARN_ON(dev->key[i].keyconf == keyconf);
	}
	if (index < 0) {
		/* Pairwise key. Get an empty slot for the key. */
		if (b43_new_kidx_api(dev))
			pairwise_keys_start = B43_NR_GROUP_KEYS;
		else
			pairwise_keys_start = B43_NR_GROUP_KEYS * 2;
		for (i = pairwise_keys_start;
		     i < pairwise_keys_start + B43_NR_PAIRWISE_KEYS;
		     i++) {
			B43_WARN_ON(i >= ARRAY_SIZE(dev->key));
			if (!dev->key[i].keyconf) {
				/* found empty */
				index = i;
				break;
			}
		}
		if (index < 0) {
			b43warn(dev->wl, "Out of hardware key memory\n");
			return -ENOSPC;
		}
	} else
		B43_WARN_ON(index > 3);

	do_key_write(dev, index, algorithm, key, key_len, mac_addr);
	if ((index <= 3) && !b43_new_kidx_api(dev)) {
		/* Default RX key */
		B43_WARN_ON(mac_addr);
		do_key_write(dev, index + 4, algorithm, key, key_len, NULL);
	}
	keyconf->hw_key_idx = index;
	dev->key[index].keyconf = keyconf;

	return 0;
}

static int b43_key_clear(struct b43_wldev *dev, int index)
{
	if (B43_WARN_ON((index < 0) || (index >= ARRAY_SIZE(dev->key))))
		return -EINVAL;
	do_key_write(dev, index, B43_SEC_ALGO_NONE,
		     NULL, B43_SEC_KEYSIZE, NULL);
	if ((index <= 3) && !b43_new_kidx_api(dev)) {
		do_key_write(dev, index + 4, B43_SEC_ALGO_NONE,
			     NULL, B43_SEC_KEYSIZE, NULL);
	}
	dev->key[index].keyconf = NULL;

	return 0;
}

static void b43_clear_keys(struct b43_wldev *dev)
{
	int i, count;

	if (b43_new_kidx_api(dev))
		count = B43_NR_GROUP_KEYS + B43_NR_PAIRWISE_KEYS;
	else
		count = B43_NR_GROUP_KEYS * 2 + B43_NR_PAIRWISE_KEYS;
	for (i = 0; i < count; i++)
		b43_key_clear(dev, i);
}

static void b43_dump_keymemory(struct b43_wldev *dev)
{
	unsigned int i, index, count, offset, pairwise_keys_start;
	u8 mac[ETH_ALEN];
	u16 algo;
	u32 rcmta0;
	u16 rcmta1;
	u64 hf;
	struct b43_key *key;

	if (!b43_debug(dev, B43_DBG_KEYS))
		return;

	hf = b43_hf_read(dev);
	b43dbg(dev->wl, "Hardware key memory dump:  USEDEFKEYS=%u\n",
	       !!(hf & B43_HF_USEDEFKEYS));
	if (b43_new_kidx_api(dev)) {
		pairwise_keys_start = B43_NR_GROUP_KEYS;
		count = B43_NR_GROUP_KEYS + B43_NR_PAIRWISE_KEYS;
	} else {
		pairwise_keys_start = B43_NR_GROUP_KEYS * 2;
		count = B43_NR_GROUP_KEYS * 2 + B43_NR_PAIRWISE_KEYS;
	}
	for (index = 0; index < count; index++) {
		key = &(dev->key[index]);
		printk(KERN_DEBUG "Key slot %02u: %s",
		       index, (key->keyconf == NULL) ? " " : "*");
		offset = dev->ktp + (index * B43_SEC_KEYSIZE);
		for (i = 0; i < B43_SEC_KEYSIZE; i += 2) {
			u16 tmp = b43_shm_read16(dev, B43_SHM_SHARED, offset + i);
			printk("%02X%02X", (tmp & 0xFF), ((tmp >> 8) & 0xFF));
		}

		algo = b43_shm_read16(dev, B43_SHM_SHARED,
				      B43_SHM_SH_KEYIDXBLOCK + (index * 2));
		printk("   Algo: %04X/%02X", algo, key->algorithm);

		if (index >= pairwise_keys_start) {
			if (key->algorithm == B43_SEC_ALGO_TKIP) {
				printk("   TKIP: ");
				offset = B43_SHM_SH_TKIPTSCTTAK + (index - 4) * (10 + 4);
				for (i = 0; i < 14; i += 2) {
					u16 tmp = b43_shm_read16(dev, B43_SHM_SHARED, offset + i);
					printk("%02X%02X", (tmp & 0xFF), ((tmp >> 8) & 0xFF));
				}
			}
			rcmta0 = b43_shm_read32(dev, B43_SHM_RCMTA,
						((index - pairwise_keys_start) * 2) + 0);
			rcmta1 = b43_shm_read16(dev, B43_SHM_RCMTA,
						((index - pairwise_keys_start) * 2) + 1);
			*((__le32 *)(&mac[0])) = cpu_to_le32(rcmta0);
			*((__le16 *)(&mac[4])) = cpu_to_le16(rcmta1);
			printk("   MAC: %pM", mac);
		} else
			printk("   DEFAULT KEY");
		printk("\n");
	}
}

void b43_power_saving_ctl_bits(struct b43_wldev *dev, unsigned int ps_flags)
{
	u32 macctl;
	u16 ucstat;
	bool hwps;
	bool awake;
	int i;

	B43_WARN_ON((ps_flags & B43_PS_ENABLED) &&
		    (ps_flags & B43_PS_DISABLED));
	B43_WARN_ON((ps_flags & B43_PS_AWAKE) && (ps_flags & B43_PS_ASLEEP));

	if (ps_flags & B43_PS_ENABLED) {
		hwps = 1;
	} else if (ps_flags & B43_PS_DISABLED) {
		hwps = 0;
	} else {
		//TODO: If powersave is not off and FIXME is not set and we are not in adhoc
		//      and thus is not an AP and we are associated, set bit 25
	}
	if (ps_flags & B43_PS_AWAKE) {
		awake = 1;
	} else if (ps_flags & B43_PS_ASLEEP) {
		awake = 0;
	} else {
		//TODO: If the device is awake or this is an AP, or we are scanning, or FIXME,
		//      or we are associated, or FIXME, or the latest PS-Poll packet sent was
		//      successful, set bit26
	}

/* FIXME: For now we force awake-on and hwps-off */
	hwps = 0;
	awake = 1;

	macctl = b43_read32(dev, B43_MMIO_MACCTL);
	if (hwps)
		macctl |= B43_MACCTL_HWPS;
	else
		macctl &= ~B43_MACCTL_HWPS;
	if (awake)
		macctl |= B43_MACCTL_AWAKE;
	else
		macctl &= ~B43_MACCTL_AWAKE;
	b43_write32(dev, B43_MMIO_MACCTL, macctl);
	/* Commit write */
	b43_read32(dev, B43_MMIO_MACCTL);
	if (awake && dev->dev->id.revision >= 5) {
		/* Wait for the microcode to wake up. */
		for (i = 0; i < 100; i++) {
			ucstat = b43_shm_read16(dev, B43_SHM_SHARED,
						B43_SHM_SH_UCODESTAT);
			if (ucstat != B43_SHM_SH_UCODESTAT_SLEEP)
				break;
			udelay(10);
		}
	}
}

void b43_wireless_core_reset(struct b43_wldev *dev, u32 flags)
{
	u32 tmslow;
	u32 macctl;

	flags |= B43_TMSLOW_PHYCLKEN;
	flags |= B43_TMSLOW_PHYRESET;
	ssb_device_enable(dev->dev, flags);
	msleep(2);		/* Wait for the PLL to turn on. */

	/* Now take the PHY out of Reset again */
	tmslow = ssb_read32(dev->dev, SSB_TMSLOW);
	tmslow |= SSB_TMSLOW_FGC;
	tmslow &= ~B43_TMSLOW_PHYRESET;
	ssb_write32(dev->dev, SSB_TMSLOW, tmslow);
	ssb_read32(dev->dev, SSB_TMSLOW);	/* flush */
	msleep(1);
	tmslow &= ~SSB_TMSLOW_FGC;
	ssb_write32(dev->dev, SSB_TMSLOW, tmslow);
	ssb_read32(dev->dev, SSB_TMSLOW);	/* flush */
	msleep(1);

	/* Turn Analog ON, but only if we already know the PHY-type.
	 * This protects against very early setup where we don't know the
	 * PHY-type, yet. wireless_core_reset will be called once again later,
	 * when we know the PHY-type. */
	if (dev->phy.ops)
		dev->phy.ops->switch_analog(dev, 1);

	macctl = b43_read32(dev, B43_MMIO_MACCTL);
	macctl &= ~B43_MACCTL_GMODE;
	if (flags & B43_TMSLOW_GMODE)
		macctl |= B43_MACCTL_GMODE;
	macctl |= B43_MACCTL_IHR_ENABLED;
	b43_write32(dev, B43_MMIO_MACCTL, macctl);
}

static void handle_irq_transmit_status(struct b43_wldev *dev)
{
	u32 v0, v1;
	u16 tmp;
	struct b43_txstatus stat;

	while (1) {
		v0 = b43_read32(dev, B43_MMIO_XMITSTAT_0);
		if (!(v0 & 0x00000001))
			break;
		v1 = b43_read32(dev, B43_MMIO_XMITSTAT_1);

		stat.cookie = (v0 >> 16);
		stat.seq = (v1 & 0x0000FFFF);
		stat.phy_stat = ((v1 & 0x00FF0000) >> 16);
		tmp = (v0 & 0x0000FFFF);
		stat.frame_count = ((tmp & 0xF000) >> 12);
		stat.rts_count = ((tmp & 0x0F00) >> 8);
		stat.supp_reason = ((tmp & 0x001C) >> 2);
		stat.pm_indicated = !!(tmp & 0x0080);
		stat.intermediate = !!(tmp & 0x0040);
		stat.for_ampdu = !!(tmp & 0x0020);
		stat.acked = !!(tmp & 0x0002);

		b43_handle_txstatus(dev, &stat);
	}
}

static void drain_txstatus_queue(struct b43_wldev *dev)
{
	u32 dummy;

	if (dev->dev->id.revision < 5)
		return;
	/* Read all entries from the microcode TXstatus FIFO
	 * and throw them away.
	 */
	while (1) {
		dummy = b43_read32(dev, B43_MMIO_XMITSTAT_0);
		if (!(dummy & 0x00000001))
			break;
		dummy = b43_read32(dev, B43_MMIO_XMITSTAT_1);
	}
}

static u32 b43_jssi_read(struct b43_wldev *dev)
{
	u32 val = 0;

	val = b43_shm_read16(dev, B43_SHM_SHARED, 0x08A);
	val <<= 16;
	val |= b43_shm_read16(dev, B43_SHM_SHARED, 0x088);

	return val;
}

static void b43_jssi_write(struct b43_wldev *dev, u32 jssi)
{
	b43_shm_write16(dev, B43_SHM_SHARED, 0x088, (jssi & 0x0000FFFF));
	b43_shm_write16(dev, B43_SHM_SHARED, 0x08A, (jssi & 0xFFFF0000) >> 16);
}

static void b43_generate_noise_sample(struct b43_wldev *dev)
{
	b43_jssi_write(dev, 0x7F7F7F7F);
	b43_write32(dev, B43_MMIO_MACCMD,
		    b43_read32(dev, B43_MMIO_MACCMD) | B43_MACCMD_BGNOISE);
}

static void b43_calculate_link_quality(struct b43_wldev *dev)
{
	/* Top half of Link Quality calculation. */

	if (dev->phy.type != B43_PHYTYPE_G)
		return;
	if (dev->noisecalc.calculation_running)
		return;
	dev->noisecalc.calculation_running = 1;
	dev->noisecalc.nr_samples = 0;

	b43_generate_noise_sample(dev);
}

static void handle_irq_noise(struct b43_wldev *dev)
{
	struct b43_phy_g *phy = dev->phy.g;
	u16 tmp;
	u8 noise[4];
	u8 i, j;
	s32 average;

	/* Bottom half of Link Quality calculation. */

	if (dev->phy.type != B43_PHYTYPE_G)
		return;

	/* Possible race condition: It might be possible that the user
	 * changed to a different channel in the meantime since we
	 * started the calculation. We ignore that fact, since it's
	 * not really that much of a problem. The background noise is
	 * an estimation only anyway. Slightly wrong results will get damped
	 * by the averaging of the 8 sample rounds. Additionally the
	 * value is shortlived. So it will be replaced by the next noise
	 * calculation round soon. */

	B43_WARN_ON(!dev->noisecalc.calculation_running);
	*((__le32 *)noise) = cpu_to_le32(b43_jssi_read(dev));
	if (noise[0] == 0x7F || noise[1] == 0x7F ||
	    noise[2] == 0x7F || noise[3] == 0x7F)
		goto generate_new;

	/* Get the noise samples. */
	B43_WARN_ON(dev->noisecalc.nr_samples >= 8);
	i = dev->noisecalc.nr_samples;
	noise[0] = clamp_val(noise[0], 0, ARRAY_SIZE(phy->nrssi_lt) - 1);
	noise[1] = clamp_val(noise[1], 0, ARRAY_SIZE(phy->nrssi_lt) - 1);
	noise[2] = clamp_val(noise[2], 0, ARRAY_SIZE(phy->nrssi_lt) - 1);
	noise[3] = clamp_val(noise[3], 0, ARRAY_SIZE(phy->nrssi_lt) - 1);
	dev->noisecalc.samples[i][0] = phy->nrssi_lt[noise[0]];
	dev->noisecalc.samples[i][1] = phy->nrssi_lt[noise[1]];
	dev->noisecalc.samples[i][2] = phy->nrssi_lt[noise[2]];
	dev->noisecalc.samples[i][3] = phy->nrssi_lt[noise[3]];
	dev->noisecalc.nr_samples++;
	if (dev->noisecalc.nr_samples == 8) {
		/* Calculate the Link Quality by the noise samples. */
		average = 0;
		for (i = 0; i < 8; i++) {
			for (j = 0; j < 4; j++)
				average += dev->noisecalc.samples[i][j];
		}
		average /= (8 * 4);
		average *= 125;
		average += 64;
		average /= 128;
		tmp = b43_shm_read16(dev, B43_SHM_SHARED, 0x40C);
		tmp = (tmp / 128) & 0x1F;
		if (tmp >= 8)
			average += 2;
		else
			average -= 25;
		if (tmp == 8)
			average -= 72;
		else
			average -= 48;

		dev->stats.link_noise = average;
		dev->noisecalc.calculation_running = 0;
		return;
	}
generate_new:
	b43_generate_noise_sample(dev);
}

static void handle_irq_tbtt_indication(struct b43_wldev *dev)
{
	if (b43_is_mode(dev->wl, NL80211_IFTYPE_AP)) {
		///TODO: PS TBTT
	} else {
		if (1 /*FIXME: the last PSpoll frame was sent successfully */ )
			b43_power_saving_ctl_bits(dev, 0);
	}
	if (b43_is_mode(dev->wl, NL80211_IFTYPE_ADHOC))
		dev->dfq_valid = 1;
}

static void handle_irq_atim_end(struct b43_wldev *dev)
{
	if (dev->dfq_valid) {
		b43_write32(dev, B43_MMIO_MACCMD,
			    b43_read32(dev, B43_MMIO_MACCMD)
			    | B43_MACCMD_DFQ_VALID);
		dev->dfq_valid = 0;
	}
}

static void handle_irq_pmq(struct b43_wldev *dev)
{
	u32 tmp;

	//TODO: AP mode.

	while (1) {
		tmp = b43_read32(dev, B43_MMIO_PS_STATUS);
		if (!(tmp & 0x00000008))
			break;
	}
	/* 16bit write is odd, but correct. */
	b43_write16(dev, B43_MMIO_PS_STATUS, 0x0002);
}

static void b43_write_template_common(struct b43_wldev *dev,
				      const u8 *data, u16 size,
				      u16 ram_offset,
				      u16 shm_size_offset, u8 rate)
{
	u32 i, tmp;
	struct b43_plcp_hdr4 plcp;

	plcp.data = 0;
	b43_generate_plcp_hdr(&plcp, size + FCS_LEN, rate);
	b43_ram_write(dev, ram_offset, le32_to_cpu(plcp.data));
	ram_offset += sizeof(u32);
	/* The PLCP is 6 bytes long, but we only wrote 4 bytes, yet.
	 * So leave the first two bytes of the next write blank.
	 */
	tmp = (u32) (data[0]) << 16;
	tmp |= (u32) (data[1]) << 24;
	b43_ram_write(dev, ram_offset, tmp);
	ram_offset += sizeof(u32);
	for (i = 2; i < size; i += sizeof(u32)) {
		tmp = (u32) (data[i + 0]);
		if (i + 1 < size)
			tmp |= (u32) (data[i + 1]) << 8;
		if (i + 2 < size)
			tmp |= (u32) (data[i + 2]) << 16;
		if (i + 3 < size)
			tmp |= (u32) (data[i + 3]) << 24;
		b43_ram_write(dev, ram_offset + i - 2, tmp);
	}
	b43_shm_write16(dev, B43_SHM_SHARED, shm_size_offset,
			size + sizeof(struct b43_plcp_hdr6));
}

/* Check if the use of the antenna that ieee80211 told us to
 * use is possible. This will fall back to DEFAULT.
 * "antenna_nr" is the antenna identifier we got from ieee80211. */
u8 b43_ieee80211_antenna_sanitize(struct b43_wldev *dev,
				  u8 antenna_nr)
{
	u8 antenna_mask;

	if (antenna_nr == 0) {
		/* Zero means "use default antenna". That's always OK. */
		return 0;
	}

	/* Get the mask of available antennas. */
	if (dev->phy.gmode)
		antenna_mask = dev->dev->bus->sprom.ant_available_bg;
	else
		antenna_mask = dev->dev->bus->sprom.ant_available_a;

	if (!(antenna_mask & (1 << (antenna_nr - 1)))) {
		/* This antenna is not available. Fall back to default. */
		return 0;
	}

	return antenna_nr;
}

/* Convert a b43 antenna number value to the PHY TX control value. */
static u16 b43_antenna_to_phyctl(int antenna)
{
	switch (antenna) {
	case B43_ANTENNA0:
		return B43_TXH_PHY_ANT0;
	case B43_ANTENNA1:
		return B43_TXH_PHY_ANT1;
	case B43_ANTENNA2:
		return B43_TXH_PHY_ANT2;
	case B43_ANTENNA3:
		return B43_TXH_PHY_ANT3;
	case B43_ANTENNA_AUTO0:
	case B43_ANTENNA_AUTO1:
		return B43_TXH_PHY_ANT01AUTO;
	}
	B43_WARN_ON(1);
	return 0;
}

static void b43_write_beacon_template(struct b43_wldev *dev,
				      u16 ram_offset,
				      u16 shm_size_offset)
{
	unsigned int i, len, variable_len;
	const struct ieee80211_mgmt *bcn;
	const u8 *ie;
	bool tim_found = 0;
	unsigned int rate;
	u16 ctl;
	int antenna;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(dev->wl->current_beacon);

	bcn = (const struct ieee80211_mgmt *)(dev->wl->current_beacon->data);
	len = min((size_t) dev->wl->current_beacon->len,
		  0x200 - sizeof(struct b43_plcp_hdr6));
	rate = ieee80211_get_tx_rate(dev->wl->hw, info)->hw_value;

	b43_write_template_common(dev, (const u8 *)bcn,
				  len, ram_offset, shm_size_offset, rate);

	/* Write the PHY TX control parameters. */
	antenna = B43_ANTENNA_DEFAULT;
	antenna = b43_antenna_to_phyctl(antenna);
	ctl = b43_shm_read16(dev, B43_SHM_SHARED, B43_SHM_SH_BEACPHYCTL);
	/* We can't send beacons with short preamble. Would get PHY errors. */
	ctl &= ~B43_TXH_PHY_SHORTPRMBL;
	ctl &= ~B43_TXH_PHY_ANT;
	ctl &= ~B43_TXH_PHY_ENC;
	ctl |= antenna;
	if (b43_is_cck_rate(rate))
		ctl |= B43_TXH_PHY_ENC_CCK;
	else
		ctl |= B43_TXH_PHY_ENC_OFDM;
	b43_shm_write16(dev, B43_SHM_SHARED, B43_SHM_SH_BEACPHYCTL, ctl);

	/* Find the position of the TIM and the DTIM_period value
	 * and write them to SHM. */
	ie = bcn->u.beacon.variable;
	variable_len = len - offsetof(struct ieee80211_mgmt, u.beacon.variable);
	for (i = 0; i < variable_len - 2; ) {
		uint8_t ie_id, ie_len;

		ie_id = ie[i];
		ie_len = ie[i + 1];
		if (ie_id == 5) {
			u16 tim_position;
			u16 dtim_period;
			/* This is the TIM Information Element */

			/* Check whether the ie_len is in the beacon data range. */
			if (variable_len < ie_len + 2 + i)
				break;
			/* A valid TIM is at least 4 bytes long. */
			if (ie_len < 4)
				break;
			tim_found = 1;

			tim_position = sizeof(struct b43_plcp_hdr6);
			tim_position += offsetof(struct ieee80211_mgmt, u.beacon.variable);
			tim_position += i;

			dtim_period = ie[i + 3];

			b43_shm_write16(dev, B43_SHM_SHARED,
					B43_SHM_SH_TIMBPOS, tim_position);
			b43_shm_write16(dev, B43_SHM_SHARED,
					B43_SHM_SH_DTIMPER, dtim_period);
			break;
		}
		i += ie_len + 2;
	}
	if (!tim_found) {
		/*
		 * If ucode wants to modify TIM do it behind the beacon, this
		 * will happen, for example, when doing mesh networking.
		 */
		b43_shm_write16(dev, B43_SHM_SHARED,
				B43_SHM_SH_TIMBPOS,
				len + sizeof(struct b43_plcp_hdr6));
		b43_shm_write16(dev, B43_SHM_SHARED,
				B43_SHM_SH_DTIMPER, 0);
	}
	b43dbg(dev->wl, "Updated beacon template at 0x%x\n", ram_offset);
}

static void b43_upload_beacon0(struct b43_wldev *dev)
{
	struct b43_wl *wl = dev->wl;

	if (wl->beacon0_uploaded)
		return;
	b43_write_beacon_template(dev, 0x68, 0x18);
	wl->beacon0_uploaded = 1;
}

static void b43_upload_beacon1(struct b43_wldev *dev)
{
	struct b43_wl *wl = dev->wl;

	if (wl->beacon1_uploaded)
		return;
	b43_write_beacon_template(dev, 0x468, 0x1A);
	wl->beacon1_uploaded = 1;
}

static void handle_irq_beacon(struct b43_wldev *dev)
{
	struct b43_wl *wl = dev->wl;
	u32 cmd, beacon0_valid, beacon1_valid;

	if (!b43_is_mode(wl, NL80211_IFTYPE_AP) &&
	    !b43_is_mode(wl, NL80211_IFTYPE_MESH_POINT))
		return;

	/* This is the bottom half of the asynchronous beacon update. */

	/* Ignore interrupt in the future. */
	dev->irq_mask &= ~B43_IRQ_BEACON;

	cmd = b43_read32(dev, B43_MMIO_MACCMD);
	beacon0_valid = (cmd & B43_MACCMD_BEACON0_VALID);
	beacon1_valid = (cmd & B43_MACCMD_BEACON1_VALID);

	/* Schedule interrupt manually, if busy. */
	if (beacon0_valid && beacon1_valid) {
		b43_write32(dev, B43_MMIO_GEN_IRQ_REASON, B43_IRQ_BEACON);
		dev->irq_mask |= B43_IRQ_BEACON;
		return;
	}

	if (unlikely(wl->beacon_templates_virgin)) {
		/* We never uploaded a beacon before.
		 * Upload both templates now, but only mark one valid. */
		wl->beacon_templates_virgin = 0;
		b43_upload_beacon0(dev);
		b43_upload_beacon1(dev);
		cmd = b43_read32(dev, B43_MMIO_MACCMD);
		cmd |= B43_MACCMD_BEACON0_VALID;
		b43_write32(dev, B43_MMIO_MACCMD, cmd);
	} else {
		if (!beacon0_valid) {
			b43_upload_beacon0(dev);
			cmd = b43_read32(dev, B43_MMIO_MACCMD);
			cmd |= B43_MACCMD_BEACON0_VALID;
			b43_write32(dev, B43_MMIO_MACCMD, cmd);
		} else if (!beacon1_valid) {
			b43_upload_beacon1(dev);
			cmd = b43_read32(dev, B43_MMIO_MACCMD);
			cmd |= B43_MACCMD_BEACON1_VALID;
			b43_write32(dev, B43_MMIO_MACCMD, cmd);
		}
	}
}

static void b43_do_beacon_update_trigger_work(struct b43_wldev *dev)
{
	u32 old_irq_mask = dev->irq_mask;

	/* update beacon right away or defer to irq */
	handle_irq_beacon(dev);
	if (old_irq_mask != dev->irq_mask) {
		/* The handler updated the IRQ mask. */
		B43_WARN_ON(!dev->irq_mask);
		if (b43_read32(dev, B43_MMIO_GEN_IRQ_MASK)) {
			b43_write32(dev, B43_MMIO_GEN_IRQ_MASK, dev->irq_mask);
		} else {
			/* Device interrupts are currently disabled. That means
			 * we just ran the hardirq handler and scheduled the
			 * IRQ thread. The thread will write the IRQ mask when
			 * it finished, so there's nothing to do here. Writing
			 * the mask _here_ would incorrectly re-enable IRQs. */
		}
	}
}

static void b43_beacon_update_trigger_work(struct work_struct *work)
{
	struct b43_wl *wl = container_of(work, struct b43_wl,
					 beacon_update_trigger);
	struct b43_wldev *dev;

	mutex_lock(&wl->mutex);
	dev = wl->current_dev;
	if (likely(dev && (b43_status(dev) >= B43_STAT_INITIALIZED))) {
		if (dev->dev->bus->bustype == SSB_BUSTYPE_SDIO) {
			/* wl->mutex is enough. */
			b43_do_beacon_update_trigger_work(dev);
			mmiowb();
		} else {
			spin_lock_irq(&wl->hardirq_lock);
			b43_do_beacon_update_trigger_work(dev);
			mmiowb();
			spin_unlock_irq(&wl->hardirq_lock);
		}
	}
	mutex_unlock(&wl->mutex);
}

/* Asynchronously update the packet templates in template RAM.
 * Locking: Requires wl->mutex to be locked. */
static void b43_update_templates(struct b43_wl *wl)
{
	struct sk_buff *beacon;

	/* This is the top half of the ansynchronous beacon update.
	 * The bottom half is the beacon IRQ.
	 * Beacon update must be asynchronous to avoid sending an
	 * invalid beacon. This can happen for example, if the firmware
	 * transmits a beacon while we are updating it. */

	/* We could modify the existing beacon and set the aid bit in
	 * the TIM field, but that would probably require resizing and
	 * moving of data within the beacon template.
	 * Simply request a new beacon and let mac80211 do the hard work. */
	beacon = ieee80211_beacon_get(wl->hw, wl->vif);
	if (unlikely(!beacon))
		return;

	if (wl->current_beacon)
		dev_kfree_skb_any(wl->current_beacon);
	wl->current_beacon = beacon;
	wl->beacon0_uploaded = 0;
	wl->beacon1_uploaded = 0;
	ieee80211_queue_work(wl->hw, &wl->beacon_update_trigger);
}

static void b43_set_beacon_int(struct b43_wldev *dev, u16 beacon_int)
{
	b43_time_lock(dev);
	if (dev->dev->id.revision >= 3) {
		b43_write32(dev, B43_MMIO_TSF_CFP_REP, (beacon_int << 16));
		b43_write32(dev, B43_MMIO_TSF_CFP_START, (beacon_int << 10));
	} else {
		b43_write16(dev, 0x606, (beacon_int >> 6));
		b43_write16(dev, 0x610, beacon_int);
	}
	b43_time_unlock(dev);
	b43dbg(dev->wl, "Set beacon interval to %u\n", beacon_int);
}

static void b43_handle_firmware_panic(struct b43_wldev *dev)
{
	u16 reason;

	/* Read the register that contains the reason code for the panic. */
	reason = b43_shm_read16(dev, B43_SHM_SCRATCH, B43_FWPANIC_REASON_REG);
	b43err(dev->wl, "Whoopsy, firmware panic! Reason: %u\n", reason);

	switch (reason) {
	default:
		b43dbg(dev->wl, "The panic reason is unknown.\n");
		/* fallthrough */
	case B43_FWPANIC_DIE:
		/* Do not restart the controller or firmware.
		 * The device is nonfunctional from now on.
		 * Restarting would result in this panic to trigger again,
		 * so we avoid that recursion. */
		break;
	case B43_FWPANIC_RESTART:
		b43_controller_restart(dev, "Microcode panic");
		break;
	}
}

static void handle_irq_ucode_debug(struct b43_wldev *dev)
{
	unsigned int i, cnt;
	u16 reason, marker_id, marker_line;
	__le16 *buf;

	/* The proprietary firmware doesn't have this IRQ. */
	if (!dev->fw.opensource)
		return;

	/* Read the register that contains the reason code for this IRQ. */
	reason = b43_shm_read16(dev, B43_SHM_SCRATCH, B43_DEBUGIRQ_REASON_REG);

	switch (reason) {
	case B43_DEBUGIRQ_PANIC:
		b43_handle_firmware_panic(dev);
		break;
	case B43_DEBUGIRQ_DUMP_SHM:
		if (!B43_DEBUG)
			break; /* Only with driver debugging enabled. */
		buf = kmalloc(4096, GFP_ATOMIC);
		if (!buf) {
			b43dbg(dev->wl, "SHM-dump: Failed to allocate memory\n");
			goto out;
		}
		for (i = 0; i < 4096; i += 2) {
			u16 tmp = b43_shm_read16(dev, B43_SHM_SHARED, i);
			buf[i / 2] = cpu_to_le16(tmp);
		}
		b43info(dev->wl, "Shared memory dump:\n");
		print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET,
			       16, 2, buf, 4096, 1);
		kfree(buf);
		break;
	case B43_DEBUGIRQ_DUMP_REGS:
		if (!B43_DEBUG)
			break; /* Only with driver debugging enabled. */
		b43info(dev->wl, "Microcode register dump:\n");
		for (i = 0, cnt = 0; i < 64; i++) {
			u16 tmp = b43_shm_read16(dev, B43_SHM_SCRATCH, i);
			if (cnt == 0)
				printk(KERN_INFO);
			printk("r%02u: 0x%04X  ", i, tmp);
			cnt++;
			if (cnt == 6) {
				printk("\n");
				cnt = 0;
			}
		}
		printk("\n");
		break;
	case B43_DEBUGIRQ_MARKER:
		if (!B43_DEBUG)
			break; /* Only with driver debugging enabled. */
		marker_id = b43_shm_read16(dev, B43_SHM_SCRATCH,
					   B43_MARKER_ID_REG);
		marker_line = b43_shm_read16(dev, B43_SHM_SCRATCH,
					     B43_MARKER_LINE_REG);
		b43info(dev->wl, "The firmware just executed the MARKER(%u) "
			"at line number %u\n",
			marker_id, marker_line);
		break;
	default:
		b43dbg(dev->wl, "Debug-IRQ triggered for unknown reason: %u\n",
		       reason);
	}
out:
	/* Acknowledge the debug-IRQ, so the firmware can continue. */
	b43_shm_write16(dev, B43_SHM_SCRATCH,
			B43_DEBUGIRQ_REASON_REG, B43_DEBUGIRQ_ACK);
}

static void b43_do_interrupt_thread(struct b43_wldev *dev)
{
	u32 reason;
	u32 dma_reason[ARRAY_SIZE(dev->dma_reason)];
	u32 merged_dma_reason = 0;
	int i;

	if (unlikely(b43_status(dev) != B43_STAT_STARTED))
		return;

	reason = dev->irq_reason;
	for (i = 0; i < ARRAY_SIZE(dma_reason); i++) {
		dma_reason[i] = dev->dma_reason[i];
		merged_dma_reason |= dma_reason[i];
	}

	if (unlikely(reason & B43_IRQ_MAC_TXERR))
		b43err(dev->wl, "MAC transmission error\n");

	if (unlikely(reason & B43_IRQ_PHY_TXERR)) {
		b43err(dev->wl, "PHY transmission error\n");
		rmb();
		if (unlikely(atomic_dec_and_test(&dev->phy.txerr_cnt))) {
			atomic_set(&dev->phy.txerr_cnt,
				   B43_PHY_TX_BADNESS_LIMIT);
			b43err(dev->wl, "Too many PHY TX errors, "
					"restarting the controller\n");
			b43_controller_restart(dev, "PHY TX errors");
		}
	}

	if (unlikely(merged_dma_reason & (B43_DMAIRQ_FATALMASK |
					  B43_DMAIRQ_NONFATALMASK))) {
		if (merged_dma_reason & B43_DMAIRQ_FATALMASK) {
			b43err(dev->wl, "Fatal DMA error: "
			       "0x%08X, 0x%08X, 0x%08X, "
			       "0x%08X, 0x%08X, 0x%08X\n",
			       dma_reason[0], dma_reason[1],
			       dma_reason[2], dma_reason[3],
			       dma_reason[4], dma_reason[5]);
			b43_controller_restart(dev, "DMA error");
			return;
		}
		if (merged_dma_reason & B43_DMAIRQ_NONFATALMASK) {
			b43err(dev->wl, "DMA error: "
			       "0x%08X, 0x%08X, 0x%08X, "
			       "0x%08X, 0x%08X, 0x%08X\n",
			       dma_reason[0], dma_reason[1],
			       dma_reason[2], dma_reason[3],
			       dma_reason[4], dma_reason[5]);
		}
	}

	if (unlikely(reason & B43_IRQ_UCODE_DEBUG))
		handle_irq_ucode_debug(dev);
	if (reason & B43_IRQ_TBTT_INDI)
		handle_irq_tbtt_indication(dev);
	if (reason & B43_IRQ_ATIM_END)
		handle_irq_atim_end(dev);
	if (reason & B43_IRQ_BEACON)
		handle_irq_beacon(dev);
	if (reason & B43_IRQ_PMQ)
		handle_irq_pmq(dev);
	if (reason & B43_IRQ_TXFIFO_FLUSH_OK)
		;/* TODO */
	if (reason & B43_IRQ_NOISESAMPLE_OK)
		handle_irq_noise(dev);

	/* Check the DMA reason registers for received data. */
	if (dma_reason[0] & B43_DMAIRQ_RX_DONE) {
		if (b43_using_pio_transfers(dev))
			b43_pio_rx(dev->pio.rx_queue);
		else
			b43_dma_rx(dev->dma.rx_ring);
	}
	B43_WARN_ON(dma_reason[1] & B43_DMAIRQ_RX_DONE);
	B43_WARN_ON(dma_reason[2] & B43_DMAIRQ_RX_DONE);
	B43_WARN_ON(dma_reason[3] & B43_DMAIRQ_RX_DONE);
	B43_WARN_ON(dma_reason[4] & B43_DMAIRQ_RX_DONE);
	B43_WARN_ON(dma_reason[5] & B43_DMAIRQ_RX_DONE);

	if (reason & B43_IRQ_TX_OK)
		handle_irq_transmit_status(dev);

	/* Re-enable interrupts on the device by restoring the current interrupt mask. */
	b43_write32(dev, B43_MMIO_GEN_IRQ_MASK, dev->irq_mask);

#if B43_DEBUG
	if (b43_debug(dev, B43_DBG_VERBOSESTATS)) {
		dev->irq_count++;
		for (i = 0; i < ARRAY_SIZE(dev->irq_bit_count); i++) {
			if (reason & (1 << i))
				dev->irq_bit_count[i]++;
		}
	}
#endif
}

/* Interrupt thread handler. Handles device interrupts in thread context. */
static irqreturn_t b43_interrupt_thread_handler(int irq, void *dev_id)
{
	struct b43_wldev *dev = dev_id;

	mutex_lock(&dev->wl->mutex);
	b43_do_interrupt_thread(dev);
	mmiowb();
	mutex_unlock(&dev->wl->mutex);

	return IRQ_HANDLED;
}

static irqreturn_t b43_do_interrupt(struct b43_wldev *dev)
{
	u32 reason;

	/* This code runs under wl->hardirq_lock, but _only_ on non-SDIO busses.
	 * On SDIO, this runs under wl->mutex. */

	reason = b43_read32(dev, B43_MMIO_GEN_IRQ_REASON);
	if (reason == 0xffffffff)	/* shared IRQ */
		return IRQ_NONE;
	reason &= dev->irq_mask;
	if (!reason)
		return IRQ_HANDLED;

	dev->dma_reason[0] = b43_read32(dev, B43_MMIO_DMA0_REASON)
	    & 0x0001DC00;
	dev->dma_reason[1] = b43_read32(dev, B43_MMIO_DMA1_REASON)
	    & 0x0000DC00;
	dev->dma_reason[2] = b43_read32(dev, B43_MMIO_DMA2_REASON)
	    & 0x0000DC00;
	dev->dma_reason[3] = b43_read32(dev, B43_MMIO_DMA3_REASON)
	    & 0x0001DC00;
	dev->dma_reason[4] = b43_read32(dev, B43_MMIO_DMA4_REASON)
	    & 0x0000DC00;
/* Unused ring
	dev->dma_reason[5] = b43_read32(dev, B43_MMIO_DMA5_REASON)
	    & 0x0000DC00;
*/

	/* ACK the interrupt. */
	b43_write32(dev, B43_MMIO_GEN_IRQ_REASON, reason);
	b43_write32(dev, B43_MMIO_DMA0_REASON, dev->dma_reason[0]);
	b43_write32(dev, B43_MMIO_DMA1_REASON, dev->dma_reason[1]);
	b43_write32(dev, B43_MMIO_DMA2_REASON, dev->dma_reason[2]);
	b43_write32(dev, B43_MMIO_DMA3_REASON, dev->dma_reason[3]);
	b43_write32(dev, B43_MMIO_DMA4_REASON, dev->dma_reason[4]);
/* Unused ring
	b43_write32(dev, B43_MMIO_DMA5_REASON, dev->dma_reason[5]);
*/

	/* Disable IRQs on the device. The IRQ thread handler will re-enable them. */
	b43_write32(dev, B43_MMIO_GEN_IRQ_MASK, 0);
	/* Save the reason bitmasks for the IRQ thread handler. */
	dev->irq_reason = reason;

	return IRQ_WAKE_THREAD;
}

/* Interrupt handler top-half. This runs with interrupts disabled. */
static irqreturn_t b43_interrupt_handler(int irq, void *dev_id)
{
	struct b43_wldev *dev = dev_id;
	irqreturn_t ret;

	if (unlikely(b43_status(dev) < B43_STAT_STARTED))
		return IRQ_NONE;

	spin_lock(&dev->wl->hardirq_lock);
	ret = b43_do_interrupt(dev);
	mmiowb();
	spin_unlock(&dev->wl->hardirq_lock);

	return ret;
}

/* SDIO interrupt handler. This runs in process context. */
static void b43_sdio_interrupt_handler(struct b43_wldev *dev)
{
	struct b43_wl *wl = dev->wl;
	irqreturn_t ret;

	mutex_lock(&wl->mutex);

	ret = b43_do_interrupt(dev);
	if (ret == IRQ_WAKE_THREAD)
		b43_do_interrupt_thread(dev);

	mutex_unlock(&wl->mutex);
}

void b43_do_release_fw(struct b43_firmware_file *fw)
{
	release_firmware(fw->data);
	fw->data = NULL;
	fw->filename = NULL;
}

static void b43_release_firmware(struct b43_wldev *dev)
{
	b43_do_release_fw(&dev->fw.ucode);
	b43_do_release_fw(&dev->fw.pcm);
	b43_do_release_fw(&dev->fw.initvals);
	b43_do_release_fw(&dev->fw.initvals_band);
}

static void b43_print_fw_helptext(struct b43_wl *wl, bool error)
{
	const char text[] =
		"You must go to " \
		"http://wireless.kernel.org/en/users/Drivers/b43#devicefirmware " \
		"and download the correct firmware for this driver version. " \
		"Please carefully read all instructions on this website.\n";

	if (error)
		b43err(wl, text);
	else
		b43warn(wl, text);
}

int b43_do_request_fw(struct b43_request_fw_context *ctx,
		      const char *name,
		      struct b43_firmware_file *fw)
{
	const struct firmware *blob;
	struct b43_fw_header *hdr;
	u32 size;
	int err;

	if (!name) {
		/* Don't fetch anything. Free possibly cached firmware. */
		/* FIXME: We should probably keep it anyway, to save some headache
		 * on suspend/resume with multiband devices. */
		b43_do_release_fw(fw);
		return 0;
	}
	if (fw->filename) {
		if ((fw->type == ctx->req_type) &&
		    (strcmp(fw->filename, name) == 0))
			return 0; /* Already have this fw. */
		/* Free the cached firmware first. */
		/* FIXME: We should probably do this later after we successfully
		 * got the new fw. This could reduce headache with multiband devices.
		 * We could also redesign this to cache the firmware for all possible
		 * bands all the time. */
		b43_do_release_fw(fw);
	}

	switch (ctx->req_type) {
	case B43_FWTYPE_PROPRIETARY:
		snprintf(ctx->fwname, sizeof(ctx->fwname),
			 "b43%s/%s.fw",
			 modparam_fwpostfix, name);
		break;
	case B43_FWTYPE_OPENSOURCE:
		snprintf(ctx->fwname, sizeof(ctx->fwname),
			 "b43-open%s/%s.fw",
			 modparam_fwpostfix, name);
		break;
	default:
		B43_WARN_ON(1);
		return -ENOSYS;
	}
	err = request_firmware(&blob, ctx->fwname, ctx->dev->dev->dev);
	if (err == -ENOENT) {
		snprintf(ctx->errors[ctx->req_type],
			 sizeof(ctx->errors[ctx->req_type]),
			 "Firmware file \"%s\" not found\n", ctx->fwname);
		return err;
	} else if (err) {
		snprintf(ctx->errors[ctx->req_type],
			 sizeof(ctx->errors[ctx->req_type]),
			 "Firmware file \"%s\" request failed (err=%d)\n",
			 ctx->fwname, err);
		return err;
	}
	if (blob->size < sizeof(struct b43_fw_header))
		goto err_format;
	hdr = (struct b43_fw_header *)(blob->data);
	switch (hdr->type) {
	case B43_FW_TYPE_UCODE:
	case B43_FW_TYPE_PCM:
		size = be32_to_cpu(hdr->size);
		if (size != blob->size - sizeof(struct b43_fw_header))
			goto err_format;
		/* fallthrough */
	case B43_FW_TYPE_IV:
		if (hdr->ver != 1)
			goto err_format;
		break;
	default:
		goto err_format;
	}

	fw->data = blob;
	fw->filename = name;
	fw->type = ctx->req_type;

	return 0;

err_format:
	snprintf(ctx->errors[ctx->req_type],
		 sizeof(ctx->errors[ctx->req_type]),
		 "Firmware file \"%s\" format error.\n", ctx->fwname);
	release_firmware(blob);

	return -EPROTO;
}

static int b43_try_request_fw(struct b43_request_fw_context *ctx)
{
	struct b43_wldev *dev = ctx->dev;
	struct b43_firmware *fw = &ctx->dev->fw;
	const u8 rev = ctx->dev->dev->id.revision;
	const char *filename;
	u32 tmshigh;
	int err;

	/* Get microcode */
	tmshigh = ssb_read32(dev->dev, SSB_TMSHIGH);
	if ((rev >= 5) && (rev <= 10))
		filename = "ucode5";
	else if ((rev >= 11) && (rev <= 12))
		filename = "ucode11";
	else if (rev == 13)
		filename = "ucode13";
	else if (rev == 14)
		filename = "ucode14";
	else if (rev >= 15)
		filename = "ucode15";
	else
		goto err_no_ucode;
	err = b43_do_request_fw(ctx, filename, &fw->ucode);
	if (err)
		goto err_load;

	/* Get PCM code */
	if ((rev >= 5) && (rev <= 10))
		filename = "pcm5";
	else if (rev >= 11)
		filename = NULL;
	else
		goto err_no_pcm;
	fw->pcm_request_failed = 0;
	err = b43_do_request_fw(ctx, filename, &fw->pcm);
	if (err == -ENOENT) {
		/* We did not find a PCM file? Not fatal, but
		 * core rev <= 10 must do without hwcrypto then. */
		fw->pcm_request_failed = 1;
	} else if (err)
		goto err_load;

	/* Get initvals */
	switch (dev->phy.type) {
	case B43_PHYTYPE_A:
		if ((rev >= 5) && (rev <= 10)) {
			if (tmshigh & B43_TMSHIGH_HAVE_2GHZ_PHY)
				filename = "a0g1initvals5";
			else
				filename = "a0g0initvals5";
		} else
			goto err_no_initvals;
		break;
	case B43_PHYTYPE_G:
		if ((rev >= 5) && (rev <= 10))
			filename = "b0g0initvals5";
		else if (rev >= 13)
			filename = "b0g0initvals13";
		else
			goto err_no_initvals;
		break;
	case B43_PHYTYPE_N:
		if ((rev >= 11) && (rev <= 12))
			filename = "n0initvals11";
		else
			goto err_no_initvals;
		break;
	case B43_PHYTYPE_LP:
		if (rev == 13)
			filename = "lp0initvals13";
		else if (rev == 14)
			filename = "lp0initvals14";
		else if (rev >= 15)
			filename = "lp0initvals15";
		else
			goto err_no_initvals;
		break;
	default:
		goto err_no_initvals;
	}
	err = b43_do_request_fw(ctx, filename, &fw->initvals);
	if (err)
		goto err_load;

	/* Get bandswitch initvals */
	switch (dev->phy.type) {
	case B43_PHYTYPE_A:
		if ((rev >= 5) && (rev <= 10)) {
			if (tmshigh & B43_TMSHIGH_HAVE_2GHZ_PHY)
				filename = "a0g1bsinitvals5";
			else
				filename = "a0g0bsinitvals5";
		} else if (rev >= 11)
			filename = NULL;
		else
			goto err_no_initvals;
		break;
	case B43_PHYTYPE_G:
		if ((rev >= 5) && (rev <= 10))
			filename = "b0g0bsinitvals5";
		else if (rev >= 11)
			filename = NULL;
		else
			goto err_no_initvals;
		break;
	case B43_PHYTYPE_N:
		if ((rev >= 11) && (rev <= 12))
			filename = "n0bsinitvals11";
		else
			goto err_no_initvals;
		break;
	case B43_PHYTYPE_LP:
		if (rev == 13)
			filename = "lp0bsinitvals13";
		else if (rev == 14)
			filename = "lp0bsinitvals14";
		else if (rev >= 15)
			filename = "lp0bsinitvals15";
		else
			goto err_no_initvals;
		break;
	default:
		goto err_no_initvals;
	}
	err = b43_do_request_fw(ctx, filename, &fw->initvals_band);
	if (err)
		goto err_load;

	return 0;

err_no_ucode:
	err = ctx->fatal_failure = -EOPNOTSUPP;
	b43err(dev->wl, "The driver does not know which firmware (ucode) "
	       "is required for your device (wl-core rev %u)\n", rev);
	goto error;

err_no_pcm:
	err = ctx->fatal_failure = -EOPNOTSUPP;
	b43err(dev->wl, "The driver does not know which firmware (PCM) "
	       "is required for your device (wl-core rev %u)\n", rev);
	goto error;

err_no_initvals:
	err = ctx->fatal_failure = -EOPNOTSUPP;
	b43err(dev->wl, "The driver does not know which firmware (initvals) "
	       "is required for your device (wl-core rev %u)\n", rev);
	goto error;

err_load:
	/* We failed to load this firmware image. The error message
	 * already is in ctx->errors. Return and let our caller decide
	 * what to do. */
	goto error;

error:
	b43_release_firmware(dev);
	return err;
}

static int b43_request_firmware(struct b43_wldev *dev)
{
	struct b43_request_fw_context *ctx;
	unsigned int i;
	int err;
	const char *errmsg;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	ctx->dev = dev;

	ctx->req_type = B43_FWTYPE_PROPRIETARY;
	err = b43_try_request_fw(ctx);
	if (!err)
		goto out; /* Successfully loaded it. */
	err = ctx->fatal_failure;
	if (err)
		goto out;

	ctx->req_type = B43_FWTYPE_OPENSOURCE;
	err = b43_try_request_fw(ctx);
	if (!err)
		goto out; /* Successfully loaded it. */
	err = ctx->fatal_failure;
	if (err)
		goto out;

	/* Could not find a usable firmware. Print the errors. */
	for (i = 0; i < B43_NR_FWTYPES; i++) {
		errmsg = ctx->errors[i];
		if (strlen(errmsg))
			b43err(dev->wl, errmsg);
	}
	b43_print_fw_helptext(dev->wl, 1);
	err = -ENOENT;

out:
	kfree(ctx);
	return err;
}

static int b43_upload_microcode(struct b43_wldev *dev)
{
	const size_t hdr_len = sizeof(struct b43_fw_header);
	const __be32 *data;
	unsigned int i, len;
	u16 fwrev, fwpatch, fwdate, fwtime;
	u32 tmp, macctl;
	int err = 0;

	/* Jump the microcode PSM to offset 0 */
	macctl = b43_read32(dev, B43_MMIO_MACCTL);
	B43_WARN_ON(macctl & B43_MACCTL_PSM_RUN);
	macctl |= B43_MACCTL_PSM_JMP0;
	b43_write32(dev, B43_MMIO_MACCTL, macctl);
	/* Zero out all microcode PSM registers and shared memory. */
	for (i = 0; i < 64; i++)
		b43_shm_write16(dev, B43_SHM_SCRATCH, i, 0);
	for (i = 0; i < 4096; i += 2)
		b43_shm_write16(dev, B43_SHM_SHARED, i, 0);

	/* Upload Microcode. */
	data = (__be32 *) (dev->fw.ucode.data->data + hdr_len);
	len = (dev->fw.ucode.data->size - hdr_len) / sizeof(__be32);
	b43_shm_control_word(dev, B43_SHM_UCODE | B43_SHM_AUTOINC_W, 0x0000);
	for (i = 0; i < len; i++) {
		b43_write32(dev, B43_MMIO_SHM_DATA, be32_to_cpu(data[i]));
		udelay(10);
	}

	if (dev->fw.pcm.data) {
		/* Upload PCM data. */
		data = (__be32 *) (dev->fw.pcm.data->data + hdr_len);
		len = (dev->fw.pcm.data->size - hdr_len) / sizeof(__be32);
		b43_shm_control_word(dev, B43_SHM_HW, 0x01EA);
		b43_write32(dev, B43_MMIO_SHM_DATA, 0x00004000);
		/* No need for autoinc bit in SHM_HW */
		b43_shm_control_word(dev, B43_SHM_HW, 0x01EB);
		for (i = 0; i < len; i++) {
			b43_write32(dev, B43_MMIO_SHM_DATA, be32_to_cpu(data[i]));
			udelay(10);
		}
	}

	b43_write32(dev, B43_MMIO_GEN_IRQ_REASON, B43_IRQ_ALL);

	/* Start the microcode PSM */
	macctl = b43_read32(dev, B43_MMIO_MACCTL);
	macctl &= ~B43_MACCTL_PSM_JMP0;
	macctl |= B43_MACCTL_PSM_RUN;
	b43_write32(dev, B43_MMIO_MACCTL, macctl);

	/* Wait for the microcode to load and respond */
	i = 0;
	while (1) {
		tmp = b43_read32(dev, B43_MMIO_GEN_IRQ_REASON);
		if (tmp == B43_IRQ_MAC_SUSPENDED)
			break;
		i++;
		if (i >= 20) {
			b43err(dev->wl, "Microcode not responding\n");
			b43_print_fw_helptext(dev->wl, 1);
			err = -ENODEV;
			goto error;
		}
		msleep(50);
	}
	b43_read32(dev, B43_MMIO_GEN_IRQ_REASON);	/* dummy read */

	/* Get and check the revisions. */
	fwrev = b43_shm_read16(dev, B43_SHM_SHARED, B43_SHM_SH_UCODEREV);
	fwpatch = b43_shm_read16(dev, B43_SHM_SHARED, B43_SHM_SH_UCODEPATCH);
	fwdate = b43_shm_read16(dev, B43_SHM_SHARED, B43_SHM_SH_UCODEDATE);
	fwtime = b43_shm_read16(dev, B43_SHM_SHARED, B43_SHM_SH_UCODETIME);

	if (fwrev <= 0x128) {
		b43err(dev->wl, "YOUR FIRMWARE IS TOO OLD. Firmware from "
		       "binary drivers older than version 4.x is unsupported. "
		       "You must upgrade your firmware files.\n");
		b43_print_fw_helptext(dev->wl, 1);
		err = -EOPNOTSUPP;
		goto error;
	}
	dev->fw.rev = fwrev;
	dev->fw.patch = fwpatch;
	dev->fw.opensource = (fwdate == 0xFFFF);

	/* Default to use-all-queues. */
	dev->wl->hw->queues = dev->wl->mac80211_initially_registered_queues;
	dev->qos_enabled = !!modparam_qos;
	/* Default to firmware/hardware crypto acceleration. */
	dev->hwcrypto_enabled = 1;

	if (dev->fw.opensource) {
		u16 fwcapa;

		/* Patchlevel info is encoded in the "time" field. */
		dev->fw.patch = fwtime;
		b43info(dev->wl, "Loading OpenSource firmware version %u.%u\n",
			dev->fw.rev, dev->fw.patch);

		fwcapa = b43_fwcapa_read(dev);
		if (!(fwcapa & B43_FWCAPA_HWCRYPTO) || dev->fw.pcm_request_failed) {
			b43info(dev->wl, "Hardware crypto acceleration not supported by firmware\n");
			/* Disable hardware crypto and fall back to software crypto. */
			dev->hwcrypto_enabled = 0;
		}
		if (!(fwcapa & B43_FWCAPA_QOS)) {
			b43info(dev->wl, "QoS not supported by firmware\n");
			/* Disable QoS. Tweak hw->queues to 1. It will be restored before
			 * ieee80211_unregister to make sure the networking core can
			 * properly free possible resources. */
			dev->wl->hw->queues = 1;
			dev->qos_enabled = 0;
		}
	} else {
		b43info(dev->wl, "Loading firmware version %u.%u "
			"(20%.2i-%.2i-%.2i %.2i:%.2i:%.2i)\n",
			fwrev, fwpatch,
			(fwdate >> 12) & 0xF, (fwdate >> 8) & 0xF, fwdate & 0xFF,
			(fwtime >> 11) & 0x1F, (fwtime >> 5) & 0x3F, fwtime & 0x1F);
		if (dev->fw.pcm_request_failed) {
			b43warn(dev->wl, "No \"pcm5.fw\" firmware file found. "
				"Hardware accelerated cryptography is disabled.\n");
			b43_print_fw_helptext(dev->wl, 0);
		}
	}

	if (b43_is_old_txhdr_format(dev)) {
		/* We're over the deadline, but we keep support for old fw
		 * until it turns out to be in major conflict with something new. */
		b43warn(dev->wl, "You are using an old firmware image. "
			"Support for old firmware will be removed soon "
			"(official deadline was July 2008).\n");
		b43_print_fw_helptext(dev->wl, 0);
	}

	return 0;

error:
	macctl = b43_read32(dev, B43_MMIO_MACCTL);
	macctl &= ~B43_MACCTL_PSM_RUN;
	macctl |= B43_MACCTL_PSM_JMP0;
	b43_write32(dev, B43_MMIO_MACCTL, macctl);

	return err;
}

static int b43_write_initvals(struct b43_wldev *dev,
			      const struct b43_iv *ivals,
			      size_t count,
			      size_t array_size)
{
	const struct b43_iv *iv;
	u16 offset;
	size_t i;
	bool bit32;

	BUILD_BUG_ON(sizeof(struct b43_iv) != 6);
	iv = ivals;
	for (i = 0; i < count; i++) {
		if (array_size < sizeof(iv->offset_size))
			goto err_format;
		array_size -= sizeof(iv->offset_size);
		offset = be16_to_cpu(iv->offset_size);
		bit32 = !!(offset & B43_IV_32BIT);
		offset &= B43_IV_OFFSET_MASK;
		if (offset >= 0x1000)
			goto err_format;
		if (bit32) {
			u32 value;

			if (array_size < sizeof(iv->data.d32))
				goto err_format;
			array_size -= sizeof(iv->data.d32);

			value = get_unaligned_be32(&iv->data.d32);
			b43_write32(dev, offset, value);

			iv = (const struct b43_iv *)((const uint8_t *)iv +
							sizeof(__be16) +
							sizeof(__be32));
		} else {
			u16 value;

			if (array_size < sizeof(iv->data.d16))
				goto err_format;
			array_size -= sizeof(iv->data.d16);

			value = be16_to_cpu(iv->data.d16);
			b43_write16(dev, offset, value);

			iv = (const struct b43_iv *)((const uint8_t *)iv +
							sizeof(__be16) +
							sizeof(__be16));
		}
	}
	if (array_size)
		goto err_format;

	return 0;

err_format:
	b43err(dev->wl, "Initial Values Firmware file-format error.\n");
	b43_print_fw_helptext(dev->wl, 1);

	return -EPROTO;
}

static int b43_upload_initvals(struct b43_wldev *dev)
{
	const size_t hdr_len = sizeof(struct b43_fw_header);
	const struct b43_fw_header *hdr;
	struct b43_firmware *fw = &dev->fw;
	const struct b43_iv *ivals;
	size_t count;
	int err;

	hdr = (const struct b43_fw_header *)(fw->initvals.data->data);
	ivals = (const struct b43_iv *)(fw->initvals.data->data + hdr_len);
	count = be32_to_cpu(hdr->size);
	err = b43_write_initvals(dev, ivals, count,
				 fw->initvals.data->size - hdr_len);
	if (err)
		goto out;
	if (fw->initvals_band.data) {
		hdr = (const struct b43_fw_header *)(fw->initvals_band.data->data);
		ivals = (const struct b43_iv *)(fw->initvals_band.data->data + hdr_len);
		count = be32_to_cpu(hdr->size);
		err = b43_write_initvals(dev, ivals, count,
					 fw->initvals_band.data->size - hdr_len);
		if (err)
			goto out;
	}
out:

	return err;
}

/* Initialize the GPIOs
 * http://bcm-specs.sipsolutions.net/GPIO
 */
static int b43_gpio_init(struct b43_wldev *dev)
{
	struct ssb_bus *bus = dev->dev->bus;
	struct ssb_device *gpiodev, *pcidev = NULL;
	u32 mask, set;

	b43_write32(dev, B43_MMIO_MACCTL, b43_read32(dev, B43_MMIO_MACCTL)
		    & ~B43_MACCTL_GPOUTSMSK);

	b43_write16(dev, B43_MMIO_GPIO_MASK, b43_read16(dev, B43_MMIO_GPIO_MASK)
		    | 0x000F);

	mask = 0x0000001F;
	set = 0x0000000F;
	if (dev->dev->bus->chip_id == 0x4301) {
		mask |= 0x0060;
		set |= 0x0060;
	}
	if (0 /* FIXME: conditional unknown */ ) {
		b43_write16(dev, B43_MMIO_GPIO_MASK,
			    b43_read16(dev, B43_MMIO_GPIO_MASK)
			    | 0x0100);
		mask |= 0x0180;
		set |= 0x0180;
	}
	if (dev->dev->bus->sprom.boardflags_lo & B43_BFL_PACTRL) {
		b43_write16(dev, B43_MMIO_GPIO_MASK,
			    b43_read16(dev, B43_MMIO_GPIO_MASK)
			    | 0x0200);
		mask |= 0x0200;
		set |= 0x0200;
	}
	if (dev->dev->id.revision >= 2)
		mask |= 0x0010;	/* FIXME: This is redundant. */

#ifdef CONFIG_SSB_DRIVER_PCICORE
	pcidev = bus->pcicore.dev;
#endif
	gpiodev = bus->chipco.dev ? : pcidev;
	if (!gpiodev)
		return 0;
	ssb_write32(gpiodev, B43_GPIO_CONTROL,
		    (ssb_read32(gpiodev, B43_GPIO_CONTROL)
		     & mask) | set);

	return 0;
}

/* Turn off all GPIO stuff. Call this on module unload, for example. */
static void b43_gpio_cleanup(struct b43_wldev *dev)
{
	struct ssb_bus *bus = dev->dev->bus;
	struct ssb_device *gpiodev, *pcidev = NULL;

#ifdef CONFIG_SSB_DRIVER_PCICORE
	pcidev = bus->pcicore.dev;
#endif
	gpiodev = bus->chipco.dev ? : pcidev;
	if (!gpiodev)
		return;
	ssb_write32(gpiodev, B43_GPIO_CONTROL, 0);
}

/* http://bcm-specs.sipsolutions.net/EnableMac */
void b43_mac_enable(struct b43_wldev *dev)
{
	if (b43_debug(dev, B43_DBG_FIRMWARE)) {
		u16 fwstate;

		fwstate = b43_shm_read16(dev, B43_SHM_SHARED,
					 B43_SHM_SH_UCODESTAT);
		if ((fwstate != B43_SHM_SH_UCODESTAT_SUSP) &&
		    (fwstate != B43_SHM_SH_UCODESTAT_SLEEP)) {
			b43err(dev->wl, "b43_mac_enable(): The firmware "
			       "should be suspended, but current state is %u\n",
			       fwstate);
		}
	}

	dev->mac_suspended--;
	B43_WARN_ON(dev->mac_suspended < 0);
	if (dev->mac_suspended == 0) {
		b43_write32(dev, B43_MMIO_MACCTL,
			    b43_read32(dev, B43_MMIO_MACCTL)
			    | B43_MACCTL_ENABLED);
		b43_write32(dev, B43_MMIO_GEN_IRQ_REASON,
			    B43_IRQ_MAC_SUSPENDED);
		/* Commit writes */
		b43_read32(dev, B43_MMIO_MACCTL);
		b43_read32(dev, B43_MMIO_GEN_IRQ_REASON);
		b43_power_saving_ctl_bits(dev, 0);
	}
}

/* http://bcm-specs.sipsolutions.net/SuspendMAC */
void b43_mac_suspend(struct b43_wldev *dev)
{
	int i;
	u32 tmp;

	might_sleep();
	B43_WARN_ON(dev->mac_suspended < 0);

	if (dev->mac_suspended == 0) {
		b43_power_saving_ctl_bits(dev, B43_PS_AWAKE);
		b43_write32(dev, B43_MMIO_MACCTL,
			    b43_read32(dev, B43_MMIO_MACCTL)
			    & ~B43_MACCTL_ENABLED);
		/* force pci to flush the write */
		b43_read32(dev, B43_MMIO_MACCTL);
		for (i = 35; i; i--) {
			tmp = b43_read32(dev, B43_MMIO_GEN_IRQ_REASON);
			if (tmp & B43_IRQ_MAC_SUSPENDED)
				goto out;
			udelay(10);
		}
		/* Hm, it seems this will take some time. Use msleep(). */
		for (i = 40; i; i--) {
			tmp = b43_read32(dev, B43_MMIO_GEN_IRQ_REASON);
			if (tmp & B43_IRQ_MAC_SUSPENDED)
				goto out;
			msleep(1);
		}
		b43err(dev->wl, "MAC suspend failed\n");
	}
out:
	dev->mac_suspended++;
}

static void b43_adjust_opmode(struct b43_wldev *dev)
{
	struct b43_wl *wl = dev->wl;
	u32 ctl;
	u16 cfp_pretbtt;

	ctl = b43_read32(dev, B43_MMIO_MACCTL);
	/* Reset status to STA infrastructure mode. */
	ctl &= ~B43_MACCTL_AP;
	ctl &= ~B43_MACCTL_KEEP_CTL;
	ctl &= ~B43_MACCTL_KEEP_BADPLCP;
	ctl &= ~B43_MACCTL_KEEP_BAD;
	ctl &= ~B43_MACCTL_PROMISC;
	ctl &= ~B43_MACCTL_BEACPROMISC;
	ctl |= B43_MACCTL_INFRA;

	if (b43_is_mode(wl, NL80211_IFTYPE_AP) ||
	    b43_is_mode(wl, NL80211_IFTYPE_MESH_POINT))
		ctl |= B43_MACCTL_AP;
	else if (b43_is_mode(wl, NL80211_IFTYPE_ADHOC))
		ctl &= ~B43_MACCTL_INFRA;

	if (wl->filter_flags & FIF_CONTROL)
		ctl |= B43_MACCTL_KEEP_CTL;
	if (wl->filter_flags & FIF_FCSFAIL)
		ctl |= B43_MACCTL_KEEP_BAD;
	if (wl->filter_flags & FIF_PLCPFAIL)
		ctl |= B43_MACCTL_KEEP_BADPLCP;
	if (wl->filter_flags & FIF_PROMISC_IN_BSS)
		ctl |= B43_MACCTL_PROMISC;
	if (wl->filter_flags & FIF_BCN_PRBRESP_PROMISC)
		ctl |= B43_MACCTL_BEACPROMISC;

	/* Workaround: On old hardware the HW-MAC-address-filter
	 * doesn't work properly, so always run promisc in filter
	 * it in software. */
	if (dev->dev->id.revision <= 4)
		ctl |= B43_MACCTL_PROMISC;

	b43_write32(dev, B43_MMIO_MACCTL, ctl);

	cfp_pretbtt = 2;
	if ((ctl & B43_MACCTL_INFRA) && !(ctl & B43_MACCTL_AP)) {
		if (dev->dev->bus->chip_id == 0x4306 &&
		    dev->dev->bus->chip_rev == 3)
			cfp_pretbtt = 100;
		else
			cfp_pretbtt = 50;
	}
	b43_write16(dev, 0x612, cfp_pretbtt);

	/* FIXME: We don't currently implement the PMQ mechanism,
	 *        so always disable it. If we want to implement PMQ,
	 *        we need to enable it here (clear DISCPMQ) in AP mode.
	 */
	if (0  /* ctl & B43_MACCTL_AP */) {
		b43_write32(dev, B43_MMIO_MACCTL,
			    b43_read32(dev, B43_MMIO_MACCTL)
			    & ~B43_MACCTL_DISCPMQ);
	} else {
		b43_write32(dev, B43_MMIO_MACCTL,
			    b43_read32(dev, B43_MMIO_MACCTL)
			    | B43_MACCTL_DISCPMQ);
	}
}

static void b43_rate_memory_write(struct b43_wldev *dev, u16 rate, int is_ofdm)
{
	u16 offset;

	if (is_ofdm) {
		offset = 0x480;
		offset += (b43_plcp_get_ratecode_ofdm(rate) & 0x000F) * 2;
	} else {
		offset = 0x4C0;
		offset += (b43_plcp_get_ratecode_cck(rate) & 0x000F) * 2;
	}
	b43_shm_write16(dev, B43_SHM_SHARED, offset + 0x20,
			b43_shm_read16(dev, B43_SHM_SHARED, offset));
}

static void b43_rate_memory_init(struct b43_wldev *dev)
{
	switch (dev->phy.type) {
	case B43_PHYTYPE_A:
	case B43_PHYTYPE_G:
	case B43_PHYTYPE_N:
	case B43_PHYTYPE_LP:
		b43_rate_memory_write(dev, B43_OFDM_RATE_6MB, 1);
		b43_rate_memory_write(dev, B43_OFDM_RATE_12MB, 1);
		b43_rate_memory_write(dev, B43_OFDM_RATE_18MB, 1);
		b43_rate_memory_write(dev, B43_OFDM_RATE_24MB, 1);
		b43_rate_memory_write(dev, B43_OFDM_RATE_36MB, 1);
		b43_rate_memory_write(dev, B43_OFDM_RATE_48MB, 1);
		b43_rate_memory_write(dev, B43_OFDM_RATE_54MB, 1);
		if (dev->phy.type == B43_PHYTYPE_A)
			break;
		/* fallthrough */
	case B43_PHYTYPE_B:
		b43_rate_memory_write(dev, B43_CCK_RATE_1MB, 0);
		b43_rate_memory_write(dev, B43_CCK_RATE_2MB, 0);
		b43_rate_memory_write(dev, B43_CCK_RATE_5MB, 0);
		b43_rate_memory_write(dev, B43_CCK_RATE_11MB, 0);
		break;
	default:
		B43_WARN_ON(1);
	}
}

/* Set the default values for the PHY TX Control Words. */
static void b43_set_phytxctl_defaults(struct b43_wldev *dev)
{
	u16 ctl = 0;

	ctl |= B43_TXH_PHY_ENC_CCK;
	ctl |= B43_TXH_PHY_ANT01AUTO;
	ctl |= B43_TXH_PHY_TXPWR;

	b43_shm_write16(dev, B43_SHM_SHARED, B43_SHM_SH_BEACPHYCTL, ctl);
	b43_shm_write16(dev, B43_SHM_SHARED, B43_SHM_SH_ACKCTSPHYCTL, ctl);
	b43_shm_write16(dev, B43_SHM_SHARED, B43_SHM_SH_PRPHYCTL, ctl);
}

/* Set the TX-Antenna for management frames sent by firmware. */
static void b43_mgmtframe_txantenna(struct b43_wldev *dev, int antenna)
{
	u16 ant;
	u16 tmp;

	ant = b43_antenna_to_phyctl(antenna);

	/* For ACK/CTS */
	tmp = b43_shm_read16(dev, B43_SHM_SHARED, B43_SHM_SH_ACKCTSPHYCTL);
	tmp = (tmp & ~B43_TXH_PHY_ANT) | ant;
	b43_shm_write16(dev, B43_SHM_SHARED, B43_SHM_SH_ACKCTSPHYCTL, tmp);
	/* For Probe Resposes */
	tmp = b43_shm_read16(dev, B43_SHM_SHARED, B43_SHM_SH_PRPHYCTL);
	tmp = (tmp & ~B43_TXH_PHY_ANT) | ant;
	b43_shm_write16(dev, B43_SHM_SHARED, B43_SHM_SH_PRPHYCTL, tmp);
}

/* This is the opposite of b43_chip_init() */
static void b43_chip_exit(struct b43_wldev *dev)
{
	b43_phy_exit(dev);
	b43_gpio_cleanup(dev);
	/* firmware is released later */
}

/* Initialize the chip
 * http://bcm-specs.sipsolutions.net/ChipInit
 */
static int b43_chip_init(struct b43_wldev *dev)
{
	struct b43_phy *phy = &dev->phy;
	int err;
	u32 value32, macctl;
	u16 value16;

	/* Initialize the MAC control */
	macctl = B43_MACCTL_IHR_ENABLED | B43_MACCTL_SHM_ENABLED;
	if (dev->phy.gmode)
		macctl |= B43_MACCTL_GMODE;
	macctl |= B43_MACCTL_INFRA;
	b43_write32(dev, B43_MMIO_MACCTL, macctl);

	err = b43_request_firmware(dev);
	if (err)
		goto out;
	err = b43_upload_microcode(dev);
	if (err)
		goto out;	/* firmware is released later */

	err = b43_gpio_init(dev);
	if (err)
		goto out;	/* firmware is released later */

	err = b43_upload_initvals(dev);
	if (err)
		goto err_gpio_clean;

	/* Turn the Analog on and initialize the PHY. */
	phy->ops->switch_analog(dev, 1);
	err = b43_phy_init(dev);
	if (err)
		goto err_gpio_clean;

	/* Disable Interference Mitigation. */
	if (phy->ops->interf_mitigation)
		phy->ops->interf_mitigation(dev, B43_INTERFMODE_NONE);

	/* Select the antennae */
	if (phy->ops->set_rx_antenna)
		phy->ops->set_rx_antenna(dev, B43_ANTENNA_DEFAULT);
	b43_mgmtframe_txantenna(dev, B43_ANTENNA_DEFAULT);

	if (phy->type == B43_PHYTYPE_B) {
		value16 = b43_read16(dev, 0x005E);
		value16 |= 0x0004;
		b43_write16(dev, 0x005E, value16);
	}
	b43_write32(dev, 0x0100, 0x01000000);
	if (dev->dev->id.revision < 5)
		b43_write32(dev, 0x010C, 0x01000000);

	b43_write32(dev, B43_MMIO_MACCTL, b43_read32(dev, B43_MMIO_MACCTL)
		    & ~B43_MACCTL_INFRA);
	b43_write32(dev, B43_MMIO_MACCTL, b43_read32(dev, B43_MMIO_MACCTL)
		    | B43_MACCTL_INFRA);

	/* Probe Response Timeout value */
	/* FIXME: Default to 0, has to be set by ioctl probably... :-/ */
	b43_shm_write16(dev, B43_SHM_SHARED, 0x0074, 0x0000);

	/* Initially set the wireless operation mode. */
	b43_adjust_opmode(dev);

	if (dev->dev->id.revision < 3) {
		b43_write16(dev, 0x060E, 0x0000);
		b43_write16(dev, 0x0610, 0x8000);
		b43_write16(dev, 0x0604, 0x0000);
		b43_write16(dev, 0x0606, 0x0200);
	} else {
		b43_write32(dev, 0x0188, 0x80000000);
		b43_write32(dev, 0x018C, 0x02000000);
	}
	b43_write32(dev, B43_MMIO_GEN_IRQ_REASON, 0x00004000);
	b43_write32(dev, B43_MMIO_DMA0_IRQ_MASK, 0x0001DC00);
	b43_write32(dev, B43_MMIO_DMA1_IRQ_MASK, 0x0000DC00);
	b43_write32(dev, B43_MMIO_DMA2_IRQ_MASK, 0x0000DC00);
	b43_write32(dev, B43_MMIO_DMA3_IRQ_MASK, 0x0001DC00);
	b43_write32(dev, B43_MMIO_DMA4_IRQ_MASK, 0x0000DC00);
	b43_write32(dev, B43_MMIO_DMA5_IRQ_MASK, 0x0000DC00);

	value32 = ssb_read32(dev->dev, SSB_TMSLOW);
	value32 |= 0x00100000;
	ssb_write32(dev->dev, SSB_TMSLOW, value32);

	b43_write16(dev, B43_MMIO_POWERUP_DELAY,
		    dev->dev->bus->chipco.fast_pwrup_delay);

	err = 0;
	b43dbg(dev->wl, "Chip initialized\n");
out:
	return err;

err_gpio_clean:
	b43_gpio_cleanup(dev);
	return err;
}

static void b43_periodic_every60sec(struct b43_wldev *dev)
{
	const struct b43_phy_operations *ops = dev->phy.ops;

	if (ops->pwork_60sec)
		ops->pwork_60sec(dev);

	/* Force check the TX power emission now. */
	b43_phy_txpower_check(dev, B43_TXPWR_IGNORE_TIME);
}

static void b43_periodic_every30sec(struct b43_wldev *dev)
{
	/* Update device statistics. */
	b43_calculate_link_quality(dev);
}

static void b43_periodic_every15sec(struct b43_wldev *dev)
{
	struct b43_phy *phy = &dev->phy;
	u16 wdr;

	if (dev->fw.opensource) {
		/* Check if the firmware is still alive.
		 * It will reset the watchdog counter to 0 in its idle loop. */
		wdr = b43_shm_read16(dev, B43_SHM_SCRATCH, B43_WATCHDOG_REG);
		if (unlikely(wdr)) {
			b43err(dev->wl, "Firmware watchdog: The firmware died!\n");
			b43_controller_restart(dev, "Firmware watchdog");
			return;
		} else {
			b43_shm_write16(dev, B43_SHM_SCRATCH,
					B43_WATCHDOG_REG, 1);
		}
	}

	if (phy->ops->pwork_15sec)
		phy->ops->pwork_15sec(dev);

	atomic_set(&phy->txerr_cnt, B43_PHY_TX_BADNESS_LIMIT);
	wmb();

#if B43_DEBUG
	if (b43_debug(dev, B43_DBG_VERBOSESTATS)) {
		unsigned int i;

		b43dbg(dev->wl, "Stats: %7u IRQs/sec, %7u TX/sec, %7u RX/sec\n",
		       dev->irq_count / 15,
		       dev->tx_count / 15,
		       dev->rx_count / 15);
		dev->irq_count = 0;
		dev->tx_count = 0;
		dev->rx_count = 0;
		for (i = 0; i < ARRAY_SIZE(dev->irq_bit_count); i++) {
			if (dev->irq_bit_count[i]) {
				b43dbg(dev->wl, "Stats: %7u IRQ-%02u/sec (0x%08X)\n",
				       dev->irq_bit_count[i] / 15, i, (1 << i));
				dev->irq_bit_count[i] = 0;
			}
		}
	}
#endif
}

static void do_periodic_work(struct b43_wldev *dev)
{
	unsigned int state;

	state = dev->periodic_state;
	if (state % 4 == 0)
		b43_periodic_every60sec(dev);
	if (state % 2 == 0)
		b43_periodic_every30sec(dev);
	b43_periodic_every15sec(dev);
}

/* Periodic work locking policy:
 * 	The whole periodic work handler is protected by
 * 	wl->mutex. If another lock is needed somewhere in the
 * 	pwork callchain, it's aquired in-place, where it's needed.
 */
static void b43_periodic_work_handler(struct work_struct *work)
{
	struct b43_wldev *dev = container_of(work, struct b43_wldev,
					     periodic_work.work);
	struct b43_wl *wl = dev->wl;
	unsigned long delay;

	mutex_lock(&wl->mutex);

	if (unlikely(b43_status(dev) != B43_STAT_STARTED))
		goto out;
	if (b43_debug(dev, B43_DBG_PWORK_STOP))
		goto out_requeue;

	do_periodic_work(dev);

	dev->periodic_state++;
out_requeue:
	if (b43_debug(dev, B43_DBG_PWORK_FAST))
		delay = msecs_to_jiffies(50);
	else
		delay = round_jiffies_relative(HZ * 15);
	ieee80211_queue_delayed_work(wl->hw, &dev->periodic_work, delay);
out:
	mutex_unlock(&wl->mutex);
}

static void b43_periodic_tasks_setup(struct b43_wldev *dev)
{
	struct delayed_work *work = &dev->periodic_work;

	dev->periodic_state = 0;
	INIT_DELAYED_WORK(work, b43_periodic_work_handler);
	ieee80211_queue_delayed_work(dev->wl->hw, work, 0);
}

/* Check if communication with the device works correctly. */
static int b43_validate_chipaccess(struct b43_wldev *dev)
{
	u32 v, backup0, backup4;

	backup0 = b43_shm_read32(dev, B43_SHM_SHARED, 0);
	backup4 = b43_shm_read32(dev, B43_SHM_SHARED, 4);

	/* Check for read/write and endianness problems. */
	b43_shm_write32(dev, B43_SHM_SHARED, 0, 0x55AAAA55);
	if (b43_shm_read32(dev, B43_SHM_SHARED, 0) != 0x55AAAA55)
		goto error;
	b43_shm_write32(dev, B43_SHM_SHARED, 0, 0xAA5555AA);
	if (b43_shm_read32(dev, B43_SHM_SHARED, 0) != 0xAA5555AA)
		goto error;

	/* Check if unaligned 32bit SHM_SHARED access works properly.
	 * However, don't bail out on failure, because it's noncritical. */
	b43_shm_write16(dev, B43_SHM_SHARED, 0, 0x1122);
	b43_shm_write16(dev, B43_SHM_SHARED, 2, 0x3344);
	b43_shm_write16(dev, B43_SHM_SHARED, 4, 0x5566);
	b43_shm_write16(dev, B43_SHM_SHARED, 6, 0x7788);
	if (b43_shm_read32(dev, B43_SHM_SHARED, 2) != 0x55663344)
		b43warn(dev->wl, "Unaligned 32bit SHM read access is broken\n");
	b43_shm_write32(dev, B43_SHM_SHARED, 2, 0xAABBCCDD);
	if (b43_shm_read16(dev, B43_SHM_SHARED, 0) != 0x1122 ||
	    b43_shm_read16(dev, B43_SHM_SHARED, 2) != 0xCCDD ||
	    b43_shm_read16(dev, B43_SHM_SHARED, 4) != 0xAABB ||
	    b43_shm_read16(dev, B43_SHM_SHARED, 6) != 0x7788)
		b43warn(dev->wl, "Unaligned 32bit SHM write access is broken\n");

	b43_shm_write32(dev, B43_SHM_SHARED, 0, backup0);
	b43_shm_write32(dev, B43_SHM_SHARED, 4, backup4);

	if ((dev->dev->id.revision >= 3) && (dev->dev->id.revision <= 10)) {
		/* The 32bit register shadows the two 16bit registers
		 * with update sideeffects. Validate this. */
		b43_write16(dev, B43_MMIO_TSF_CFP_START, 0xAAAA);
		b43_write32(dev, B43_MMIO_TSF_CFP_START, 0xCCCCBBBB);
		if (b43_read16(dev, B43_MMIO_TSF_CFP_START_LOW) != 0xBBBB)
			goto error;
		if (b43_read16(dev, B43_MMIO_TSF_CFP_START_HIGH) != 0xCCCC)
			goto error;
	}
	b43_write32(dev, B43_MMIO_TSF_CFP_START, 0);

	v = b43_read32(dev, B43_MMIO_MACCTL);
	v |= B43_MACCTL_GMODE;
	if (v != (B43_MACCTL_GMODE | B43_MACCTL_IHR_ENABLED))
		goto error;

	return 0;
error:
	b43err(dev->wl, "Failed to validate the chipaccess\n");
	return -ENODEV;
}

static void b43_security_init(struct b43_wldev *dev)
{
	dev->ktp = b43_shm_read16(dev, B43_SHM_SHARED, B43_SHM_SH_KTP);
	/* KTP is a word address, but we address SHM bytewise.
	 * So multiply by two.
	 */
	dev->ktp *= 2;
	/* Number of RCMTA address slots */
	b43_write16(dev, B43_MMIO_RCMTA_COUNT, B43_NR_PAIRWISE_KEYS);
	/* Clear the key memory. */
	b43_clear_keys(dev);
}

#ifdef CONFIG_B43_HWRNG
static int b43_rng_read(struct hwrng *rng, u32 *data)
{
	struct b43_wl *wl = (struct b43_wl *)rng->priv;
	struct b43_wldev *dev;
	int count = -ENODEV;

	mutex_lock(&wl->mutex);
	dev = wl->current_dev;
	if (likely(dev && b43_status(dev) >= B43_STAT_INITIALIZED)) {
		*data = b43_read16(dev, B43_MMIO_RNG);
		count = sizeof(u16);
	}
	mutex_unlock(&wl->mutex);

	return count;
}
#endif /* CONFIG_B43_HWRNG */

static void b43_rng_exit(struct b43_wl *wl)
{
#ifdef CONFIG_B43_HWRNG
	if (wl->rng_initialized)
		hwrng_unregister(&wl->rng);
#endif /* CONFIG_B43_HWRNG */
}

static int b43_rng_init(struct b43_wl *wl)
{
	int err = 0;

#ifdef CONFIG_B43_HWRNG
	snprintf(wl->rng_name, ARRAY_SIZE(wl->rng_name),
		 "%s_%s", KBUILD_MODNAME, wiphy_name(wl->hw->wiphy));
	wl->rng.name = wl->rng_name;
	wl->rng.data_read = b43_rng_read;
	wl->rng.priv = (unsigned long)wl;
	wl->rng_initialized = 1;
	err = hwrng_register(&wl->rng);
	if (err) {
		wl->rng_initialized = 0;
		b43err(wl, "Failed to register the random "
		       "number generator (%d)\n", err);
	}
#endif /* CONFIG_B43_HWRNG */

	return err;
}

static void b43_tx_work(struct work_struct *work)
{
	struct b43_wl *wl = container_of(work, struct b43_wl, tx_work);
	struct b43_wldev *dev;
	struct sk_buff *skb;
	int err = 0;

	mutex_lock(&wl->mutex);
	dev = wl->current_dev;
	if (unlikely(!dev || b43_status(dev) < B43_STAT_STARTED)) {
		mutex_unlock(&wl->mutex);
		return;
	}

	while (skb_queue_len(&wl->tx_queue)) {
		skb = skb_dequeue(&wl->tx_queue);

		if (b43_using_pio_transfers(dev))
			err = b43_pio_tx(dev, skb);
		else
			err = b43_dma_tx(dev, skb);
		if (unlikely(err))
			dev_kfree_skb(skb); /* Drop it */
	}

#if B43_DEBUG
	dev->tx_count++;
#endif
	mutex_unlock(&wl->mutex);
}

static int b43_op_tx(struct ieee80211_hw *hw,
		     struct sk_buff *skb)
{
	struct b43_wl *wl = hw_to_b43_wl(hw);

	if (unlikely(skb->len < 2 + 2 + 6)) {
		/* Too short, this can't be a valid frame. */
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
	B43_WARN_ON(skb_shinfo(skb)->nr_frags);

	skb_queue_tail(&wl->tx_queue, skb);
	ieee80211_queue_work(wl->hw, &wl->tx_work);

	return NETDEV_TX_OK;
}

static void b43_qos_params_upload(struct b43_wldev *dev,
				  const struct ieee80211_tx_queue_params *p,
				  u16 shm_offset)
{
	u16 params[B43_NR_QOSPARAMS];
	int bslots, tmp;
	unsigned int i;

	if (!dev->qos_enabled)
		return;

	bslots = b43_read16(dev, B43_MMIO_RNG) & p->cw_min;

	memset(&params, 0, sizeof(params));

	params[B43_QOSPARAM_TXOP] = p->txop * 32;
	params[B43_QOSPARAM_CWMIN] = p->cw_min;
	params[B43_QOSPARAM_CWMAX] = p->cw_max;
	params[B43_QOSPARAM_CWCUR] = p->cw_min;
	params[B43_QOSPARAM_AIFS] = p->aifs;
	params[B43_QOSPARAM_BSLOTS] = bslots;
	params[B43_QOSPARAM_REGGAP] = bslots + p->aifs;

	for (i = 0; i < ARRAY_SIZE(params); i++) {
		if (i == B43_QOSPARAM_STATUS) {
			tmp = b43_shm_read16(dev, B43_SHM_SHARED,
					     shm_offset + (i * 2));
			/* Mark the parameters as updated. */
			tmp |= 0x100;
			b43_shm_write16(dev, B43_SHM_SHARED,
					shm_offset + (i * 2),
					tmp);
		} else {
			b43_shm_write16(dev, B43_SHM_SHARED,
					shm_offset + (i * 2),
					params[i]);
		}
	}
}

/* Mapping of mac80211 queue numbers to b43 QoS SHM offsets. */
static const u16 b43_qos_shm_offsets[] = {
	/* [mac80211-queue-nr] = SHM_OFFSET, */
	[0] = B43_QOS_VOICE,
	[1] = B43_QOS_VIDEO,
	[2] = B43_QOS_BESTEFFORT,
	[3] = B43_QOS_BACKGROUND,
};

/* Update all QOS parameters in hardware. */
static void b43_qos_upload_all(struct b43_wldev *dev)
{
	struct b43_wl *wl = dev->wl;
	struct b43_qos_params *params;
	unsigned int i;

	if (!dev->qos_enabled)
		return;

	BUILD_BUG_ON(ARRAY_SIZE(b43_qos_shm_offsets) !=
		     ARRAY_SIZE(wl->qos_params));

	b43_mac_suspend(dev);
	for (i = 0; i < ARRAY_SIZE(wl->qos_params); i++) {
		params = &(wl->qos_params[i]);
		b43_qos_params_upload(dev, &(params->p),
				      b43_qos_shm_offsets[i]);
	}
	b43_mac_enable(dev);
}

static void b43_qos_clear(struct b43_wl *wl)
{
	struct b43_qos_params *params;
	unsigned int i;

	/* Initialize QoS parameters to sane defaults. */

	BUILD_BUG_ON(ARRAY_SIZE(b43_qos_shm_offsets) !=
		     ARRAY_SIZE(wl->qos_params));

	for (i = 0; i < ARRAY_SIZE(wl->qos_params); i++) {
		params = &(wl->qos_params[i]);

		switch (b43_qos_shm_offsets[i]) {
		case B43_QOS_VOICE:
			params->p.txop = 0;
			params->p.aifs = 2;
			params->p.cw_min = 0x0001;
			params->p.cw_max = 0x0001;
			break;
		case B43_QOS_VIDEO:
			params->p.txop = 0;
			params->p.aifs = 2;
			params->p.cw_min = 0x0001;
			params->p.cw_max = 0x0001;
			break;
		case B43_QOS_BESTEFFORT:
			params->p.txop = 0;
			params->p.aifs = 3;
			params->p.cw_min = 0x0001;
			params->p.cw_max = 0x03FF;
			break;
		case B43_QOS_BACKGROUND:
			params->p.txop = 0;
			params->p.aifs = 7;
			params->p.cw_min = 0x0001;
			params->p.cw_max = 0x03FF;
			break;
		default:
			B43_WARN_ON(1);
		}
	}
}

/* Initialize the core's QOS capabilities */
static void b43_qos_init(struct b43_wldev *dev)
{
	if (!dev->qos_enabled) {
		/* Disable QOS support. */
		b43_hf_write(dev, b43_hf_read(dev) & ~B43_HF_EDCF);
		b43_write16(dev, B43_MMIO_IFSCTL,
			    b43_read16(dev, B43_MMIO_IFSCTL)
			    & ~B43_MMIO_IFSCTL_USE_EDCF);
		b43dbg(dev->wl, "QoS disabled\n");
		return;
	}

	/* Upload the current QOS parameters. */
	b43_qos_upload_all(dev);

	/* Enable QOS support. */
	b43_hf_write(dev, b43_hf_read(dev) | B43_HF_EDCF);
	b43_write16(dev, B43_MMIO_IFSCTL,
		    b43_read16(dev, B43_MMIO_IFSCTL)
		    | B43_MMIO_IFSCTL_USE_EDCF);
	b43dbg(dev->wl, "QoS enabled\n");
}

static int b43_op_conf_tx(struct ieee80211_hw *hw, u16 _queue,
			  const struct ieee80211_tx_queue_params *params)
{
	struct b43_wl *wl = hw_to_b43_wl(hw);
	struct b43_wldev *dev;
	unsigned int queue = (unsigned int)_queue;
	int err = -ENODEV;

	if (queue >= ARRAY_SIZE(wl->qos_params)) {
		/* Queue not available or don't support setting
		 * params on this queue. Return success to not
		 * confuse mac80211. */
		return 0;
	}
	BUILD_BUG_ON(ARRAY_SIZE(b43_qos_shm_offsets) !=
		     ARRAY_SIZE(wl->qos_params));

	mutex_lock(&wl->mutex);
	dev = wl->current_dev;
	if (unlikely(!dev || (b43_status(dev) < B43_STAT_INITIALIZED)))
		goto out_unlock;

	memcpy(&(wl->qos_params[queue].p), params, sizeof(*params));
	b43_mac_suspend(dev);
	b43_qos_params_upload(dev, &(wl->qos_params[queue].p),
			      b43_qos_shm_offsets[queue]);
	b43_mac_enable(dev);
	err = 0;

out_unlock:
	mutex_unlock(&wl->mutex);

	return err;
}

static int b43_op_get_tx_stats(struct ieee80211_hw *hw,
			       struct ieee80211_tx_queue_stats *stats)
{
	struct b43_wl *wl = hw_to_b43_wl(hw);
	struct b43_wldev *dev;
	int err = -ENODEV;

	mutex_lock(&wl->mutex);
	dev = wl->current_dev;
	if (dev && b43_status(dev) >= B43_STAT_STARTED) {
		if (b43_using_pio_transfers(dev))
			b43_pio_get_tx_stats(dev, stats);
		else
			b43_dma_get_tx_stats(dev, stats);
		err = 0;
	}
	mutex_unlock(&wl->mutex);

	return err;
}

static int b43_op_get_stats(struct ieee80211_hw *hw,
			    struct ieee80211_low_level_stats *stats)
{
	struct b43_wl *wl = hw_to_b43_wl(hw);

	mutex_lock(&wl->mutex);
	memcpy(stats, &wl->ieee_stats, sizeof(*stats));
	mutex_unlock(&wl->mutex);

	return 0;
}

static u64 b43_op_get_tsf(struct ieee80211_hw *hw)
{
	struct b43_wl *wl = hw_to_b43_wl(hw);
	struct b43_wldev *dev;
	u64 tsf;

	mutex_lock(&wl->mutex);
	dev = wl->current_dev;

	if (dev && (b43_status(dev) >= B43_STAT_INITIALIZED))
		b43_tsf_read(dev, &tsf);
	else
		tsf = 0;

	mutex_unlock(&wl->mutex);

	return tsf;
}

static void b43_op_set_tsf(struct ieee80211_hw *hw, u64 tsf)
{
	struct b43_wl *wl = hw_to_b43_wl(hw);
	struct b43_wldev *dev;

	mutex_lock(&wl->mutex);
	dev = wl->current_dev;

	if (dev && (b43_status(dev) >= B43_STAT_INITIALIZED))
		b43_tsf_write(dev, tsf);

	mutex_unlock(&wl->mutex);
}

static void b43_put_phy_into_reset(struct b43_wldev *dev)
{
	struct ssb_device *sdev = dev->dev;
	u32 tmslow;

	tmslow = ssb_read32(sdev, SSB_TMSLOW);
	tmslow &= ~B43_TMSLOW_GMODE;
	tmslow |= B43_TMSLOW_PHYRESET;
	tmslow |= SSB_TMSLOW_FGC;
	ssb_write32(sdev, SSB_TMSLOW, tmslow);
	msleep(1);

	tmslow = ssb_read32(sdev, SSB_TMSLOW);
	tmslow &= ~SSB_TMSLOW_FGC;
	tmslow |= B43_TMSLOW_PHYRESET;
	ssb_write32(sdev, SSB_TMSLOW, tmslow);
	msleep(1);
}

static const char *band_to_string(enum ieee80211_band band)
{
	switch (band) {
	case IEEE80211_BAND_5GHZ:
		return "5";
	case IEEE80211_BAND_2GHZ:
		return "2.4";
	default:
		break;
	}
	B43_WARN_ON(1);
	return "";
}

/* Expects wl->mutex locked */
static int b43_switch_band(struct b43_wl *wl, struct ieee80211_channel *chan)
{
	struct b43_wldev *up_dev = NULL;
	struct b43_wldev *down_dev;
	struct b43_wldev *d;
	int err;
	bool uninitialized_var(gmode);
	int prev_status;

	/* Find a device and PHY which supports the band. */
	list_for_each_entry(d, &wl->devlist, list) {
		switch (chan->band) {
		case IEEE80211_BAND_5GHZ:
			if (d->phy.supports_5ghz) {
				up_dev = d;
				gmode = 0;
			}
			break;
		case IEEE80211_BAND_2GHZ:
			if (d->phy.supports_2ghz) {
				up_dev = d;
				gmode = 1;
			}
			break;
		default:
			B43_WARN_ON(1);
			return -EINVAL;
		}
		if (up_dev)
			break;
	}
	if (!up_dev) {
		b43err(wl, "Could not find a device for %s-GHz band operation\n",
		       band_to_string(chan->band));
		return -ENODEV;
	}
	if ((up_dev == wl->current_dev) &&
	    (!!wl->current_dev->phy.gmode == !!gmode)) {
		/* This device is already running. */
		return 0;
	}
	b43dbg(wl, "Switching to %s-GHz band\n",
	       band_to_string(chan->band));
	down_dev = wl->current_dev;

	prev_status = b43_status(down_dev);
	/* Shutdown the currently running core. */
	if (prev_status >= B43_STAT_STARTED)
		down_dev = b43_wireless_core_stop(down_dev);
	if (prev_status >= B43_STAT_INITIALIZED)
		b43_wireless_core_exit(down_dev);

	if (down_dev != up_dev) {
		/* We switch to a different core, so we put PHY into
		 * RESET on the old core. */
		b43_put_phy_into_reset(down_dev);
	}

	/* Now start the new core. */
	up_dev->phy.gmode = gmode;
	if (prev_status >= B43_STAT_INITIALIZED) {
		err = b43_wireless_core_init(up_dev);
		if (err) {
			b43err(wl, "Fatal: Could not initialize device for "
			       "selected %s-GHz band\n",
			       band_to_string(chan->band));
			goto init_failure;
		}
	}
	if (prev_status >= B43_STAT_STARTED) {
		err = b43_wireless_core_start(up_dev);
		if (err) {
			b43err(wl, "Fatal: Coult not start device for "
			       "selected %s-GHz band\n",
			       band_to_string(chan->band));
			b43_wireless_core_exit(up_dev);
			goto init_failure;
		}
	}
	B43_WARN_ON(b43_status(up_dev) != prev_status);

	wl->current_dev = up_dev;

	return 0;
init_failure:
	/* Whoops, failed to init the new core. No core is operating now. */
	wl->current_dev = NULL;
	return err;
}

/* Write the short and long frame retry limit values. */
static void b43_set_retry_limits(struct b43_wldev *dev,
				 unsigned int short_retry,
				 unsigned int long_retry)
{
	/* The retry limit is a 4-bit counter. Enforce this to avoid overflowing
	 * the chip-internal counter. */
	short_retry = min(short_retry, (unsigned int)0xF);
	long_retry = min(long_retry, (unsigned int)0xF);

	b43_shm_write16(dev, B43_SHM_SCRATCH, B43_SHM_SC_SRLIMIT,
			short_retry);
	b43_shm_write16(dev, B43_SHM_SCRATCH, B43_SHM_SC_LRLIMIT,
			long_retry);
}

static int b43_op_config(struct ieee80211_hw *hw, u32 changed)
{
	struct b43_wl *wl = hw_to_b43_wl(hw);
	struct b43_wldev *dev;
	struct b43_phy *phy;
	struct ieee80211_conf *conf = &hw->conf;
	int antenna;
	int err = 0;

	mutex_lock(&wl->mutex);

	/* Switch the band (if necessary). This might change the active core. */
	err = b43_switch_band(wl, conf->channel);
	if (err)
		goto out_unlock_mutex;
	dev = wl->current_dev;
	phy = &dev->phy;

	b43_mac_suspend(dev);

	if (changed & IEEE80211_CONF_CHANGE_RETRY_LIMITS)
		b43_set_retry_limits(dev, conf->short_frame_max_tx_count,
					  conf->long_frame_max_tx_count);
	changed &= ~IEEE80211_CONF_CHANGE_RETRY_LIMITS;
	if (!changed)
		goto out_mac_enable;

	/* Switch to the requested channel.
	 * The firmware takes care of races with the TX handler. */
	if (conf->channel->hw_value != phy->channel)
		b43_switch_channel(dev, conf->channel->hw_value);

	dev->wl->radiotap_enabled = !!(conf->flags & IEEE80211_CONF_RADIOTAP);

	/* Adjust the desired TX power level. */
	if (conf->power_level != 0) {
		if (conf->power_level != phy->desired_txpower) {
			phy->desired_txpower = conf->power_level;
			b43_phy_txpower_check(dev, B43_TXPWR_IGNORE_TIME |
						   B43_TXPWR_IGNORE_TSSI);
		}
	}

	/* Antennas for RX and management frame TX. */
	antenna = B43_ANTENNA_DEFAULT;
	b43_mgmtframe_txantenna(dev, antenna);
	antenna = B43_ANTENNA_DEFAULT;
	if (phy->ops->set_rx_antenna)
		phy->ops->set_rx_antenna(dev, antenna);

	if (wl->radio_enabled != phy->radio_on) {
		if (wl->radio_enabled) {
			b43_software_rfkill(dev, false);
			b43info(dev->wl, "Radio turned on by software\n");
			if (!dev->radio_hw_enable) {
				b43info(dev->wl, "The hardware RF-kill button "
					"still turns the radio physically off. "
					"Press the button to turn it on.\n");
			}
		} else {
			b43_software_rfkill(dev, true);
			b43info(dev->wl, "Radio turned off by software\n");
		}
	}

out_mac_enable:
	b43_mac_enable(dev);
out_unlock_mutex:
	mutex_unlock(&wl->mutex);

	return err;
}

static void b43_update_basic_rates(struct b43_wldev *dev, u32 brates)
{
	struct ieee80211_supported_band *sband =
		dev->wl->hw->wiphy->bands[b43_current_band(dev->wl)];
	struct ieee80211_rate *rate;
	int i;
	u16 basic, direct, offset, basic_offset, rateptr;

	for (i = 0; i < sband->n_bitrates; i++) {
		rate = &sband->bitrates[i];

		if (b43_is_cck_rate(rate->hw_value)) {
			direct = B43_SHM_SH_CCKDIRECT;
			basic = B43_SHM_SH_CCKBASIC;
			offset = b43_plcp_get_ratecode_cck(rate->hw_value);
			offset &= 0xF;
		} else {
			direct = B43_SHM_SH_OFDMDIRECT;
			basic = B43_SHM_SH_OFDMBASIC;
			offset = b43_plcp_get_ratecode_ofdm(rate->hw_value);
			offset &= 0xF;
		}

		rate = ieee80211_get_response_rate(sband, brates, rate->bitrate);

		if (b43_is_cck_rate(rate->hw_value)) {
			basic_offset = b43_plcp_get_ratecode_cck(rate->hw_value);
			basic_offset &= 0xF;
		} else {
			basic_offset = b43_plcp_get_ratecode_ofdm(rate->hw_value);
			basic_offset &= 0xF;
		}

		/*
		 * Get the pointer that we need to point to
		 * from the direct map
		 */
		rateptr = b43_shm_read16(dev, B43_SHM_SHARED,
					 direct + 2 * basic_offset);
		/* and write it to the basic map */
		b43_shm_write16(dev, B43_SHM_SHARED, basic + 2 * offset,
				rateptr);
	}
}

static void b43_op_bss_info_changed(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    struct ieee80211_bss_conf *conf,
				    u32 changed)
{
	struct b43_wl *wl = hw_to_b43_wl(hw);
	struct b43_wldev *dev;

	mutex_lock(&wl->mutex);

	dev = wl->current_dev;
	if (!dev || b43_status(dev) < B43_STAT_STARTED)
		goto out_unlock_mutex;

	B43_WARN_ON(wl->vif != vif);

	if (changed & BSS_CHANGED_BSSID) {
		if (conf->bssid)
			memcpy(wl->bssid, conf->bssid, ETH_ALEN);
		else
			memset(wl->bssid, 0, ETH_ALEN);
	}

	if (b43_status(dev) >= B43_STAT_INITIALIZED) {
		if (changed & BSS_CHANGED_BEACON &&
		    (b43_is_mode(wl, NL80211_IFTYPE_AP) ||
		     b43_is_mode(wl, NL80211_IFTYPE_MESH_POINT) ||
		     b43_is_mode(wl, NL80211_IFTYPE_ADHOC)))
			b43_update_templates(wl);

		if (changed & BSS_CHANGED_BSSID)
			b43_write_mac_bssid_templates(dev);
	}

	b43_mac_suspend(dev);

	/* Update templates for AP/mesh mode. */
	if (changed & BSS_CHANGED_BEACON_INT &&
	    (b43_is_mode(wl, NL80211_IFTYPE_AP) ||
	     b43_is_mode(wl, NL80211_IFTYPE_MESH_POINT) ||
	     b43_is_mode(wl, NL80211_IFTYPE_ADHOC)))
		b43_set_beacon_int(dev, conf->beacon_int);

	if (changed & BSS_CHANGED_BASIC_RATES)
		b43_update_basic_rates(dev, conf->basic_rates);

	if (changed & BSS_CHANGED_ERP_SLOT) {
		if (conf->use_short_slot)
			b43_short_slot_timing_enable(dev);
		else
			b43_short_slot_timing_disable(dev);
	}

	b43_mac_enable(dev);
out_unlock_mutex:
	mutex_unlock(&wl->mutex);
}

static int b43_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
			  struct ieee80211_vif *vif, struct ieee80211_sta *sta,
			  struct ieee80211_key_conf *key)
{
	struct b43_wl *wl = hw_to_b43_wl(hw);
	struct b43_wldev *dev;
	u8 algorithm;
	u8 index;
	int err;
	static const u8 bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (modparam_nohwcrypt)
		return -ENOSPC; /* User disabled HW-crypto */

	mutex_lock(&wl->mutex);

	dev = wl->current_dev;
	err = -ENODEV;
	if (!dev || b43_status(dev) < B43_STAT_INITIALIZED)
		goto out_unlock;

	if (dev->fw.pcm_request_failed || !dev->hwcrypto_enabled) {
		/* We don't have firmware for the crypto engine.
		 * Must use software-crypto. */
		err = -EOPNOTSUPP;
		goto out_unlock;
	}

	err = -EINVAL;
	switch (key->alg) {
	case ALG_WEP:
		if (key->keylen == WLAN_KEY_LEN_WEP40)
			algorithm = B43_SEC_ALGO_WEP40;
		else
			algorithm = B43_SEC_ALGO_WEP104;
		break;
	case ALG_TKIP:
		/*

  Broadcom B43 wirele2005ver

  Copyright (c) CCM5 Martin Langer <martin-langerAESver

  Copyrdefault Marom BWARN_ON(1)ver
goto out_unlocopyr}
	index = (u8) (key->keyidx Micif (.de>
 > 3)ichael Buesch <mb@bu
	switch (cmd) {yright SET_KEY Marny v/*

  Broad <martin-langer@gmx &&
		    (!ght (cflags & IEEE80211as J_FLAG_PAIRWISE) || suppor!modparam_hwtkip)c) 20		/* We support only pairwise key */rts err = -EOPNOTSUPx.de>hael Buesch <mb@bu3	}
ggi <an Copyright (c) 2009 Albert Herranz <albertarts ny v2005-200!stae parts from the ipw2200
  drivver  Copyright(c) 203schs of Pfile are derwith an assigned MAC address.rived from thb43_key_write(dev, -1, /*

  Bro,y
  iupporht (c) 2,icense, olen2 of the Lista->ed br
  ( Mich} elseparts of Groupe derived from thware Foundation; ei.de>
er version 2 of the License, or
  (at your o NULLrsion.

  Tggi <anerrugellfang@gentoo.org>
  i <andreas.jaggi@waterwave.ch>
WEP40t_herranz@R A PARTICULAR PURPOSE.  See 104ree sofwarehfundation; ei
  You readon; ) |com BHF_USEDEFKEYS.

  This program
  You should havy
  ippore received a copy & ~ the GNU General Public
		 Copyright |=c) 2009 Albert HerraGENERATE_IV) 20 <andreas.jaggi@waterwave.ch>

  SMERCHee Software Foundation, Inc., 51 Franklin MMICver

  Copyright DISABLEas Jaee sothe hope that clearon; eiht (chwe Fou005 Danty of
  MERCHANTABILITY or FITr

  Copyr}
  Copyright (c) 2005-2009 Mic}

uesch <mb@:anny v!
  Mee sob43dbg(wl, "%s hardware based encryption forx/ifidx: %d, " suppor   "mac: %pM\n"progde <lincmdgi@wdreas J ? "Using" : "Disabl>

#r
  (at yoidxng.h>
#inclsta ?y later ve : bcast_ed b Michwaredumpe Fomemory copyx/firmmutexch <mb@(&wl->nclud)>
  return err;
}

static voide recop_configure_filter(struct ieee09 Albhw *hwprogram; sun as pubint changed,e "pcmcia.h"
#*fright"
#include 64 multinclu)
{
	"xmit.hwarewl *wl = hw_to_PTION((hwg.h"_DESCRIPTION(dev *dev>
  ncludephy_n.h"
#include 	_AUT= h"
#current_OR("Me <lincopy artsude <li = 0Michael Buesch <mb@bu3sc
Michael B&= FIF_PROMISC_IN_BSS herran);
MALLMULTI"GPL");

MOFCSFAIL"GPL");

MOPLCPORTED_FIRMWARE_CONTROED_FIRMWARE_OTHERNSE("GPL");

MOBCN_PRBRESPMODULE_L>
  includenik");
MODULE_LICENSE("GPL");;

MODULE_FIRMWARE(BB43_SUPPORTED_FIRMWWARE_ID);


static iint modparam_bad_frrames_preempt;
modulee_param_named(bad_frames_preh"
#nclude_hael Bueude <li>
  ny vempt, mod&IPTIO"pious copy >dcom B4TAT_INITIALIZEDugelwareadjust_opmodion; de "s.h>
#includenclude "phy_n.h"
#include }

/* Locking:tefannclud
 * Ra.h"
s the o Brivi dev. This might be diffemodpafromhwpctpasuff.inaram,arambecausehwpctlorel, int, 04gone away while we h <mb@edhwpctnclud the "pio.h"ver");
MODULE_AUTHIPTIONireless_de p_stop "xmit.hMODULE_AUTHOR(ODULE_DESCRIPTION("Broadcdev->wldriver");
MODULE_AUTHorigo");
MOu32 masg>
 redoude <lin_AUT||param_fwpostfix, <, 0444);
MSTARTM_DESCma.h"
#OR("Mar/* Cancel work. U <mb@ to a
#incdead<mb@  the Fnclude "phy_n.h"
#include 	cDULE__delayed_ARM__sync(&ypt, periodice_parram_qos = 1_param_nameh"
#txam_qos, itin Langer");
MODULE_AUTHOR("Stefano Brivio");
MODULE_AUT;
module_param_named(hwtkip, modparam_hOR("Mof thoopser viens ate uphwpctdevicelt off)");wer);

static the Frip, int, 044Gábo/* ude "be.h"
errupts onMODULE_PARM the Faram_etm_fwpostfix,, 0444);
MODULE_PARM_D
MODULEypt, ypt, busodpartype <asmSB_BUSTYPE_SDIO modparamh"
#inclu is able BluThate, "enough the FrPTIONdati32DEFAULT;
mMMIO Fra_IRQ_MASK, 0hy_commoned a, 2=info(default), 3=debug"););efauFlushrivedThis prograspinanger_irqE_PARM <liirqangerhy_common=warn, 2=info(default), 3=debug");


static const struct ssb_device_id b43_ssb_tbl[] = {
	SSB_DVENDORh <mb@ADCOM, SSB_DEV_80211, 5),
}defauSynchronize and freare-si;

int b4 handlersDESC(hwtkip, "Enable hardware tkiption.")wcrypts, "Enabl
static int modparam_rbose, b43_modparam_verbose, int, 0644);
MODULE_PARMaram_dio__DEVADCOMhy_g.h"
VICE(SSB_VE80211, 9),B_VENDORb43_modirq MichE(SSB_VENDORB_DEVICE(S,LE_Pg.h"
#includenger");
MODULE_AUTHOR("Stefano Brivio");
MODULE_AUTHooth coexistencerbose,  != ption.") modpany varam_fwpostfix, 16, 0444);
Mdparam_hwtkhael Bmodpude h coexistence (	ic i hope tnst struct ssb_device_id b43_ssb_tb
 (c) 2005-200calcu!= 0xF we donmodpic ide "dfaulraiodparaTX queuerivedt off)(skb_/
#de_lenE_PARM_DE/
#de)bl);dev_kE(SSBskbB_ENTde/
#de _flags) \
	{			>
  waremac_suspena copy;ateid)leds_exitalue	= (_ra.h>
#inclWhwcryptSSB_VEface aramped\n"de "dma.h"
#_BROAodparam_hwpctl;
module_pam_nohwcryph"
#ed(nohwcrypt, modparart_nohwcrypt, int, 0444);
MODh"
#inclusame for all daram_fwpostfix, !6, 0444);
MODULE_PARM_D43_rdncy _tx_fwpos \
	{	NDOR_BROrbose, b43_modparam_verbose, int, 0644);
MODULE_PARMthe hope tDEVICr3_RAstB_VENDORve recDEVIC;

int b4_BROADCOlude <linux/ense
  aloer <lin, "D, "Cannot 11_RATE ODUL IRQhen chRCHANTABILI  the FADCOM, SSB_rom th11_RATE_thed aed_BROADCOM, SSB_DEV_8wareRATETAB_ENT(B43_C2 of the LFDM_RATE_12MB,_9MB, , 0),
	RATETAB_ENTIRQF_SHARED, KBUILD_MODNAME_80211, 1ATE_11MB, IEEE80211_RATE_SHORT_PREAMBLE),
	RATEIRQ-%When_8021B_DEVICE(SSB_VE_RATE_6MB, 0),
	RoncurrWe ux/sed aykip,runose = B43_VERBOSITY_DEFAULT;
moduledparam_hconcurrS!
 * data flow (TX/RX)ose = B43_,	\
enon)"alue	= (_rat=warn, 2=info(default), 3=debug");

DEVICE(S_ get concurrtable_maintainDULEPARM_se = B43_, modparatasks_VERupNDOR_BR (_rateid),in		\
		.fllags		= (TE_SHORT_Ps),				\
	}

/*
 * NOart When chouightma.h"
#include  is et PHY
	SSBRAAB_Eversioning numbers3_plcp_get_bitrate_phy__value		= _nohwcrypt, int, 0444);
MODULE_PARM_DEphy *wer	= ed(qos,hy;

stattmp;

s8 analog_rbost ieee (_fhannel b43_2ghr");

s16 raEVICmanuf
	CHAN4G(1, 2ver
	CHAN4G(1, 2= {
	Ch"
#un code iedBueschncurr (_freq)_value		= (ivedtmpulated
 * d162=info(default)PHY_VER_OFD80211_chann  Co0),
&com BPHYVER_ANALOG) >> 0),
	CHAN4G(9, 24_SHIFT;
	_2ghz_ch(8, 2447, 0),
	CHAN4G4);
52, 0),
	CHAN4G(4);
MO, 0),
	CHANrENDOR 2447, 0),
	CHAN4GVERSION drivopyrigh_2ghz_chc) 2005 An0),
	CH4);
MAaggi <an),
	CHAN>= 4t, be2, 0),
	CHAN4G1ver

  Copyright l, _flags) {B			\
	.band			= != 2modp)),	\
	.hw_4alue		= (_chann6g.h>
#ilue		= (_chann780211_BAND_5GHZ,		\
	.center_freq		= 5000 + (5 * G			\
	.band			= I 980211_BAND_5GHZ,		\
	.center_fr#ifdef CONFIG_om BNPHYreq		= 5000 + (5 * N
static struct ieeE80211_BAND_5GHZ,		\
	.center_fr#endifantable[] = {
	CHANG(7,LPreq		= 5000 + (5 * L5 Martic struct iee2	CHAN5G(38, 0),
	CHAN5G(40, 0),		CHAN5  Copyright _BAND_5GHZ,		\
	.c}IEEE8022, 0),
	CHAworkqueue1_RATE_SHORT_PFOUND UN00
 Oram_freq)nclude <linu(A0211_ %u, TG(11,		CRevilue	 %u)pping.h>
#incl80211_chann,3_2ghz_ch	CHAN5rAB_ENT(ma.h"
#e ipw2200
  dr}l, _freq, _flags) {Foundfreq: (68, 0),		CHAN5G(70, 0),
	CHAN5pping.,		CHAN5G(74, 0),
	CHAN5G(76, 0),		CHAN5 2427, 0)\
	.hw_value		= (ivedrbose, b43_modparamchip_ie <as0x4317shared for 0),
	CHAN5G(100, 0)CHAN4= 0t, be0),
	C0x3205017FENT(is pr
	CHAN5G(104, 0),		CHAN5G(106, 1),
	CHAN5G(104, 0),		CHAN5G(1,
	CHAN5G(105, 0),		CHAADCOM, SSB_ble + 0)
 2437, 0),
	CHAN4\
	.hmodparamULT;
m\
	.hCTL_Ied(ve 0),
	CHAN4G(6, 2437, 0),
	CHAN4 0),		DATA_LOW5),
	SSB_DEVIC 0),
	CHAN5G(124, 0),		CHAN5G(126, 0),
	CHAN5G(128, 0),	| Cop32)HAN5G(130, 0),
	CHAN5G(132, 0),		CHAHIGH) << 16, 0),	G(1, 2412, N4G(14, 240x0148,FFF_OFDAN4G(2, 20),
	CHAN5G(1 we 00052, 012	CHAN5G(1CHAN4G(14, 240x, 0),		C52, 028IEEE802HAN5G(147, 0s, so17F /* Broadcom */bl);_BAND_5GHZ,		\
	.c4G

#define CHAN5G(_channel, _flags) {				\
	.bAN5G(149, s, so2060),
	C_BAND_5GHZ,		\
	.ce,
	CHAN5G(\
	.hw_(114, 61, 0),
	CHAN5G(162, 0),		CHAG(154, 0),		CH80211_BAND_5GHZ,		\
	.center_freq		= 5000 + (5 * (_channelCHAN5G(160,152, FF0T(B43,		C5AN5G(161, 0),
	CHAN5G(162
	.max_power		= 30,				\
}
static HAN5G(160, 0),		C6, 0),
	CHAN5G(178, 0),		CHAN5G(180, 0),
	CHAN5G(180),
	CHANHAN5G(184, 0),
	CH5flagHAN5G(184, 0),
	CH680211_BAND_5GHZ,		\
	.center_freq		= 5000 + (5 * CHAN5G(48,HAN5G(160, 0),		CHvalueHAN5G(160, 0),		CHkugel1_BAND_5GHZ,		\
	.center_freware.h>
#include <linux/wirele),
	CHAN5G(60, 0),		CHAN5G(62, 0),
	CHAN5G(64, 0),		CHAN5\
	.hw, 0),
	CHAN5GM47, 00x%X, Vvalue	226, 0) 0),
	CHAN5G(72, 0),		CHANG(1, 2412, ,,		CHAN5G(nel b43_	CHAN5G(78, 0),
	CHAN5G(80, 0),		CHAN5G(82, 0),
	CHAN5GR(1, : AN5G(226, 0),		CHAN5G(228, 0),
};

st 0),		CHAN5G(e80211_channel b43_5ghz_aphy_chantab
	CHA->HAN5G(147, 0),G(1, 2412, 0),(52, 0),		C49, 0)AN4G(2, 2417(52, 0),		CCHAN4G,
	CHAN4G(3AN5G(1080211_ =e80211_channel (52, 4G(11, _2ghz_chant(52, 0ENDORe[] = {
			\
	.cen0lude "pio.h"
#incle_si_"xmit.= (_fforAN4G(_nohwcrypt, int, 0444)program; sept;
module_pwer		= 3ODULE(52,  <linux/_powersysftrooadc!yahoo.es>

 pctDisa(52, next_txpwr_check_tim11, jiffies;ncurrreq)TX __bors cou

in the Fatomic_VER(& 0),
	xerr_cntULT;
mG(7,TX_BADNESS_LIMITHAN5#if 5000DEBUG65, 0), (_fstatic uesch"(52, 0),		C(216, 0),
};		CHAN5		CHAN5G(128, 0),
	CHAN5G(13LE_AU0),		CHAN5G(136, 0),
	CHAN5G(14ODULE0),
	Rfq_val,		CG(4, 2427Assumare-siG(1, verboson)"d. If it's MBLE,
	.n_c,hwpct"pioe will
	 * immediately get fixed_modparafirst , modparle + 0a_ratetab0),
	G(1, 2hw		(__b4		\
	.tetable	tel),		memHAN5G0),
	"pios, 0, sizeofHAN5G(nd b4S(_rat),
	CHAN5G(132, 0),		CHAN5T_PRE			\
}
stG(94, 0)IRQ relaCHANright a_ratetabne breason0),
};
_suppor0),
	RmaRRAY_SI3_band_5GHz_aphy antable),
S(_rnnels	= ARcalculaom Bdebug");TEMPLATEIEEE802tetabhoo.es>
verbosed(hwtkiVERBOSITY(212, 						\itrates	= b&=.  If debu4, 0),ERRATETAz = ,	\
		.hw_vZ,		\
	.ncurrNo arecalculaclude,
	Cext80211_supported_ban,
	._cha3_band_5GHz_aphy able),
	.S(_rde "pio.h"
#includebluetooth_co	CHA	(__b43nohwcrypt, int, 0444);
MODULE_PARssb_sp_PAR*t b43_,				\
}3_modparamt b43;

s64 hfing(fwpoyahoo.es>
btsizebl);

/* C
MODULE_(t b43->boardright_lo7, 0),
BFL_BTCOEXIST					v);
static st		\
}
st.rboseB43_CCK + (5 * (flagtkip
statigPost *dev);
stat
	h 0), received a copyIEEE802ct b43_wldev * b43_wireless_core_sMO_DESChf142, the GNe_stopAL),
			CHAN5nt_dev)
		return 1;= (_ratou should havehf
	.n_bitrates	= b43_g_ratetable_size,
dt on)"/
static struct ieee80211_rait(struct b43_wldev *dev);
stati//TODO.n_bitrates	= b43_g_imcfglo,		CHouts44);
arHAN5atic void b43_wireless_corntable[] = {
	nt, DRIAN4GPCICOREre_exit(strucbus *RBOScrypt, 3_modparatic struct 

statiarampcide p.rateIO s; see(wl))
		return;->id.de p,		CHA (b43EVaram
	va_start(args, fmt);
	printkr0),
	CHA<= 5 modparamIMCFGLO uct b43_PARM_l, con the Fr0),
	Ctrucnst struct		ret,O "b4 : "wla_OFDM4G

#defparam_verbos, IEEE05 Andt, 0644);
MPCI Mar
{
	va_list args;

MCI				\	va_e	= Ir(struct b4_REQTOOFDM_ERBOSITY_ERROR)
		reSERn;
	if (!b4|G(108,OFDM_
  Copyr(b43_modparam_verbSS(_chaVERBOSITY_ERROR)
		return;
	if (!b43_ratelimit(wl))
		return;
	va_star53args, fmt);
	p Copyright , fmt);
	pe Frtruc=warn, 2=ind b43err(struct b4,trucg.h"
#		CHANAN5Gs;

	if (b43_modparam_ver_chade "pio.h"
#includeVERBOynth_pu1;
mod5G(136, 0),
	CHAN5G(140 bool idleODULEHAN4_WARN)
	hantablThen");
 valuee, "in microsecond  the F *dev);
static inti@waterflags) {	bl);_WARN)
	 = 370sch"		CHAN5phy_name(wl105sch"static sisstru43_raSHORT_NL09 AlbIFgs) {	DHOCrt_h(wl))
	 : "wlan");
	5hw->w 0),
art(struc),		CHAN5G(N5G(176, flag...)
{
	va_list G(106, 8					phy_name(wlmax(_WARN)
	, (u16)240
sta= B43_Vhm,
	CHAN5G(136, 0),SHMOFDM_RATE"b43-%s de_SPUWKUP,n;
	va_stt modparaSetissuesSF CFP pre-TargetBeaconTransmislue	Timbose =am_verbose < B43_VERpretbt*/
static struct ieee80211_rturn;oid b4art(args, fmt);
	printk(KERN_WARNING "b43-%s warni, args);
	va_end(args);
}

void b43dbg(str, IEEEev, u16 = (argADCOM, SSB_arning: ",
	       (wl && wl->hw) ? wiMIO_MACCTL)12sch")		CHAN5GIO_MACCTL);rintk),		CHprintk(KERN_DEBUG "b43-%s debug: ",
	       (PRETBTT,dev, u16tetable + 0)
 2437, 0),
	CHAN4TSF_CFad_f, val);
}

staticy_name(hutdown a ohwcryptide po*/c with xmit.c's
 *	 b43_plcp_get_
#includeohwcrypt, modp				\_ram_write(struct b43_wldevtaticc5G(16same for all drn;
	vparam_fwpostfix, 1LT;
module_param_named(verbostkip;
module_param_namedB43_CCK_RATE_1MB, 0),
	Rct b43_wldevB43_VERBOSITY_DEFAULT;
moduleUNODUL3_a_ratetao
MODUL_WARNcode PSMre tkipontroulated
 * data in there. ThiMACCTLos, "& 0x00	= IEEE8 {
			_PSM_RUN* Unalignedev)
		 */
			b43_JMPsch"ble + 0)
#define b43_b_rat {
			,	contro fmt);
	panta_DEValue	= (_ratpVICE(SSalue	= (_rat0, 0)				\
		.flaart(strucopt b4opyri_80211_.chann
stat *dev);
sefano Briviob	vpri, IEEE		\
		.bitrat_an"phy_ B43_MMIO_SHM_DATA)) t, arg		offset >>= 2;
	}
	b =the ince (detrucE_PARM
	 * Ratestruct b43
stat3_VERBO_may157, 06 roHAN5G(104, 0),*dev,
			Initial9),
	ing, u16 offset)
{cp_get_bitrate_idx_* functionnphy = {
	.band		= IEEE80211_BAse < B43_VERBOSITY_INFO)
		return;
	if_exit(struct b43_wldev *devint b43_wirel	\
	.max_power		= 30,				\
}
statiate __b43less_coref (!b43_rateatetable[] = {
	RATETAB_ENT(B43_CCK_RATE == B43_SHMrom thHM_DATA)57, 0upnst 43_read16(d
  MERCRATE_6MB, 0d DoS
	ret = b43is
};

sta cop		ret B43_MM0),
	C(52, t b43 ?",
	 TMSLOW_GMODE :esch")ORD offset. */
	corez_aphy_nst char *fncurrR b43 all_size	"xmit.ure  the Fted_band b43_band_5GHz_nphy 0211, 1(52,  1);
prep5G(1"xmit.stfix,hantablE;

stahy_chout	= (to tpctlm_verbose = truc)
		retet =ADCOvec routing() {
		)
		ret, 0),
	RAT fmt);
	pinfo(struct b43_wl *wl, conslue	= (_ratup and running.
	 * RateMB, IEEE802 == B43_SHM_SHARED <linux/CCK_RATE_5MB value & 0xFFFF);
			b43_sNED,
			ty of
  MERCHANTAB),		bus6 ro offsethe hope t0, 0),4G(_channeset >>= 2;
	}
	b4(dev, B43_MMIOt);
	mmiowb();
	b43_write32(dev, B43;

	,
	       (WL_verREV, 0),
	RATETAiphy_name(>> 1
static int b43_ratelimit(st 0),
	CHAN5(wl && wl->hw)G, IEEEnt_dev)
		retSYMW + 1);
	CHAN5G(12063, 0),
nt_dev)
		retGDCing, u16 ct b43_wldev * b43_wireless_corPACTRLalue)
{
	if (routiOFDMPABOOS),
	6, 0),
60, 0),		CHAN5G(

	if (b43tl & B435G(100, 0),		CHA= CHAN5Gnt_dev)
		ret4318TSSIng, u16 offsetose < B43<ting, offset >> 2);
V_verCALlinu6, 0),
ct b43_wldev * b43_wireless_corXTAL_NO_SHMurrent_dev)
		retDSCRQ;AN5Glt on)")slowc(hwtk11_RATEsLE_PARuWARN the ist args;

	if (b43_modparam_verbo 0),
param_verbose, int, 0644);
MPCI43_ma_starit(wl))
		return;ite32(dev, B4wl->10					nt_dev)
		retPCISCW
	b43: ",(dev , B43_ntk(fmt, args);5G(54, 0hfed access6 roKCFPUx.de B43_STAT_STARTED)
		refset)
{
	uretry_limi	B43_W ",
	 DEFAULT_SHORT_RETRY(208, word( <linB43_SHM_SHARLONG43_SHM_SH_HO	= (_ratrintk(KERN_DEBUG "b43-%s debug: ",
	       (wFFBLIM, 3Flags */
void b43_hf_write(struct b43_wldev *dev, u6L value)
2ARN_ON(o_write16(	CHAng probdefisponseM_DATA,firmnux/.antabSet);
		ihe Maxs);
	if l (dusechy_ch always triggerantaban");
	vp, so)");ne49, F000 anyL) >> 16;
	F00000An");
	vp of zertableinf (vabose = B43_Vmmiowb();
	b43_write32(dev, B43_MMIO_RAM_DATAMAXTIRATE9 MiH_HOSTrate_lude "  (value >> 1
static vhytxct 1;
opyriB43_WARN_ON(oMinimum CRRAYncludeWindowHAN5G(98, b43_shm_write16(struct bBworkqueueb43_shm_write16(dev, B43_SHCRATCHpensource);
_MINodpa,),		,		CH20, 0),		CHAN5GN_ON(!dev->fw.opensource);
	return b43_shm_read16(dev, B3_SHM_mware axly) */
static u16 b43_fwcap_WARN_ON(!dev->fw.opensource);
	return b43_shm_reaAX6(dev, B3),		C*fmt, ...)
{3_modparam_verbose, int, 0644);
Mose < rt_herpporte, b43_modparam_verbose, int, 0644);
MODULE_/
	low =om BFORCE_PULE_PARM16(de__uh>

_shm_ttk(ffnnel	\
	.cethe hope tshm_ (value >> 1ADCOM, SSB__MMIO_REV3PLUS_TSF_HIGH);

	*dev, Bhe hope tanta<= 32;
	*tsf  16) & 0xFFFF);
			ret, routingapabiliqoHAN4G(_channeset)
{
	u3OSITY_WARN)
		dev, 9 Mic3_g_ratetable_size,
};

stad(dev, r;

			goto out;
		}
	ruct b43_wldev * b43_wireless_corntrol_word(dmacctl |upload_cardableed by
 ite16(dev, Bsecurit firmware cap
	"
#includewake \
	{	ruct  B43_Mss drl;

	macctl = b43_read32(dev, B43_MMIO_MB43_VERBOSITY_DEFAULT;
module_param_named(v2GHZ,		\
	.center_f
_read32(dev, :rd(dev, routing, (offs(dev, B43_M:_SHM_DATA);
out:
	return16 b43 2);
			ret = b43_read16(dev, B43_MMIO_SHM_DATA_UNdma.h"
#include "pio.h"bitrate_opde "_RATE_*
 * "xmit.h"
#include "lo.h"
#inc"xmit.h"
#includeifv)
{
sysfs *ysfsODULE_DESCRIPTION("Broadcom B43 wireless driver");
MODULE_AUTHOR("Ml_word(d the ipw2200
  dt(argsODO:ldevow WDS/APset & 0s	if sizeisSIZE(g(fwposonf,
	CHAN!=s);
}

void b43dbSDIO sinclu_REV3PLUS_TSF_HIGH, high);
	ME_DATOINTt;

	ret 

void b43_tsf_write(struct b4);
IONdev, u64 tsf)
{
	b43_time_lock(dev);
	WDSdev, u64 tsf)
{
	b43_time_lock(dev);
	bg(str5G(78, 0),
	CHAN5G(80, , "Enable QOS support (defad Doh"
#opera);
	ugelfang@gentnclude "phy_nel, _freq,ORT_PAd00ULLI}

/*
 * rbose_48MB, 

void b43RATETAR("Stefano Brivio");
MOc const u8 ze
	*tsf h"
#vi 0),

voidviing,h"
#ifN4G(11, 

void b4343_5ghcpyic co,	\
r vers

voidrite16(devETH_ALE CHAOLD;
	ostfix, "Postfix forabilities oid b43;
	macctl |= B43_MACCTL_TBTTHOLD;
	
statc void b43_time_unlock(struct b43SHM_DATA0;
ETH_ALEN] = { 0 }les to load.");

static int m"dma.h"
#include "pio.h"
#include "sremovu16 );

	low = tsf;
	high = (tsf >> 32);
CHAN5G(149, hardware guarantees us an atomic write, if we
	 * write the low register first. */
	b43_writ"Stefano Brivio");
Mlags		= (_flagRc_bs!mac)
		mac = zero_addr;

	offset |= 0x00
	SSB_DEVTABLE_END
};

Md(struct b43_w!c const u8 zered(struct b43_wTROL, of!fset);

	daid +TROL, offsg, offB43_MMIO_MACFILTER(4, 2ta);
	data = mac[2];
	dat_suppor43_write16(dev0FILTER_DATA, DATA, data);
	data = mac[4];
	data | to load.");

static int modpev->dev->id.revisis!
 */
static"
#include "lo.h_bssid[ETH_ALEN * 2];
	int i;
	u32 tmp;

	bssid = dev->wl->bssid;
	mac = dev->wl->macl_wordi < 3iCTL)r *fmdev, B43_(4, 2427Kev, B4l olpctlstetablspecifdev->formels	= to makhe creantabwpctlard won'tite1 itKERN3_5ghhe in		CHframe betweenEE8021, B43_nd	con09 Al rNG "s.h"
FILTitre tkip ARRAY_SIZbssidsid); i += sizeofset_slot_tiE(mac_bssid); i += sizeoftfix[16];
module_pasch"h"
#43_MMtaprouting,f (dev-_MMIO_MAlude <wO_SH->phyDATA))0oid b43HYTYPE_G)e16(dev, 01684, 510 + slot_time);
	b4_tempntabs_virgiutin_CONTROL43_MMIB43_PHYTYPchant

	memcpy(mac_bssid, mac,  for all devices.
 *(hwtkip, moODULE_PARM_DCK_RATE_5MB, IE offset)
{
	u16 ret> 2) + 1);
			b43_write16TH_ALEN] = { 0 };
		
	}
}

statG(58,struct b43_wldev *dev)
{
	b43_set_btcoex, modpa;
}

static void b43_shos!
 */TAB_ENT(B43_OFDM_RATE_ *dev	}
}

s);

		return ret;
}

voi				\
		.flastruct b43_wldev *dev)
{
	b54MB, 0),
XXX:n thisdo10, E_PARM_doesv);
 code inrfkev, irqfine Ri0),		&dev-//bcm-_polb43.(hw->signemac,<< 8;
	b43_write16(dev, B43_MMIO_MACFILTER_DATA, data);
}

static void b43_write_maram_nohwcry2) (mac_bssid[i + 2]) << 16;
		tmp |= (u32) (mac_bssid[i + 3]) << 24;
		b43_ram_write(dev, 0x20 + i,  int, 0444);
MODULE_ot_tim_SHM_Supd Rea_SHAREDS(_ratfset, const u8 *mac)
{
	stati all devices.
 * They can't be constw;
}

stcumented on
 * http://boize	12

02.11/sb_tbl);hael Buesch <mb@bu3schORD offset. */
	control0211, 1e);
}

static void b(4, the .fw files to load.");

static int m0B846E;
	}

	for (i = 0; tx57, 0)ostfix,m_qos
	.n_bitrates->id.revisii < 5; VERBtim "xmit.h"
#include "lo.h"
#incl"xmit.h"
#includelude*staelimit(seMODULE_DESCRIPTION("Broadcom B43 wireless drncurrFIXME:hed "Log 		CHAN5Gc void+)
		bARED, 0x43_wriN5G(124, 0),		CHAN5G(128, 0) << 8;
		_notif		return;"
#include "lo.h"
#in		CHAN5G(149,"
#include, of*vif);
	if (!paenumghz_b43_wri_ude EC TODO
	});
	if (!pa_on && (phy->type, 0x0000ite16(dev, 0x050A, 0x0000);
	b43_write16(dev,ETH_ALEN);
	m, of||ESC(v, ETH_A
	/* Wx01000000,
		0x00000000w_scan, max_l43_wride "xmit.h"
#include "lo.hODULE_DESCRIPTION("Broadcom B43 wireless driver");
MODULE_AUTHOR("Martin Langer");
MODULE_AUTHOR("Stefano Brivio");
MODULE3_write43_write16(dev, 0x0568, 0x000ODULE_PARM_D modparamlt on)")hy) (dev,  datic vk;
	_modother#inclnel  the Fr
  You should have received a copy of the GN43_SHM_g.h"
#include "phy_n.h"
#include , 0x0502, 0x0050);
		break;
	dcompletet:
		b43_write16(dev, 0x0502, 0x0030);
	}

	if (phy->radio_ver == 0x2050 && phy->radio_rev <= 0x5)
		b43_radio_write16(dev, 0x0051, 0x0017);
	for (i = 0x00; i < max_loop; i++) {
		value = b43_read16(dev, 0x050E);Re-};

staalue & 0x0}
	for (i = 0x00; i < 0x0A; i++) {
		valueG.  If notad16(dev, 0x050E);
		if (value & 0x0400)
			break;constrite16(dev, 0x0508opstic inw;

	/=_shm.tx			atic iop_tmain.
}

_bloc */
	kidx_kidx_t b43on < 3);

	loo_fw(dev, ion < 3);

	lo b43ac_bssid_templat_fw(dev, iac_bssid_templat b43_kidigock */
	kidx
}

st b43bsHAN4fo_include_fw(dev, i);

	/* Write th_KEYIDXBLO"
#include_fw(dev, indexset */
	offs b43VERBkeyo_fw(dev, iEC_KEYS b43(dev, 0xki
#inc_fw(dev, i B43_SEC_KEYSIZ b43gERBOSITso_fw(dev, i;
		valuey[i];
		tx	value |= (u16) (key[
		b43_s]) << 8;
sfhm_write16(dev, s_PHYSEC_K offset + i, v

stati	}
}
8021ock */
	kidxruct e(struopb43_wldev *devop	}
}

staimo_fw(dev, i)
		b43_write1e(strucb43_wri43_wldev *dev,b43_wri	}
}
eak;
	defaul43_wldev *deeak;
	default:
		b43_KEYS * 2;

	or (i = 3_new_kidx_api(devor (i = 0x00; i <shm_wint i,oop;o_fw(devise_keys_st,
};eq		=Hard-d b43templahip. Do ARRAcaddr(offseirectly.aramUseA,
			
	CHANlerid b!
 */)
va_end(args);
}

staCHAN5G(HAN5"xmit.h_paramxmit.h*m_qosDULE_DESCRIPTION(->bssid;
	b();
}

v_rater_of(ARM_,pt;
module_param_,16;
max_lm_qos, iULE_PARM_DESC(nohwcrypt, "Disaoid b43_uploaEYS);prevm_fwposid);

	memcpy(mac_bssid, mac, 
		addrtmp[5MB, IEEfwpostfix,0),
	CBtic vODULE_PARM_6 ro..3-%s warni((u32) (addrx0568, 0x0000);
	if (dev->dev->id.revision < 11)
		b43_write16(dev, 0x0ee sof, B43_MMNODEteet,	}
	b43_shm_v, boo;
		addrtmp[0] |= ((u32) (addODULE_PARM_DESC(fwp */
void b43_dummy_transmncurr...>wl-up agairatetabransmitter address (RCMTA) mechanism */
n, as documented on
 * http:/ (value >> 1ty of
  MERCHANTABILI {
			/* Undrtmp[0] |= ((u32) (addr[3]) << 24); documented on
 * http://bcm-v4.sipsolutions.net/8023_shm_write32(dev, B43_SHM_RC

	/* Receive match GHZ,		et >>= 2;
	efano Brivio"); MAC addAN5GFaiPHYTto }

stODULE_Pre tkip.");

static int modparam_et >>= 2;
	AN5G(62ORT_PRac 0 is mstart;
	 ORTEEDB43_OFD		CHAN5b43	/* e rc4 key is computed, a When chc_bssid[i + 1]) <<),
	CHbands5G(136, 0),
	CHAN5G(140, 0)ENT(mit(have_2ghzs bielimit(B43_R5_MAC_DEn.
	 * So we"
#include "lo.hwcrypt, "D43_Mn atomicB43_RX_MAC_DE B43_ value;
-> *   [) 2009 AlbBAND_2GHZ]003) , B4and_MACzead16(dev, B,
	       (wl && wl->hw)N3_shm_cont returned
 *   , of in order to never have B43_RX_5AC_DECERR, we sho5GHz_n
statif (macctl & B43ause we drop packets in case of
 * B43_RX_MAC_DECERR, if we have a correca iv32 b,
	.chantati code isRX_MAadco43_RX_MAC_DEfset >> 2) +til next rned).
 *
 *rned
 * );
	b43_write16(dev, 0x0568, 0x08ohwcrypt, modpdetachl = routing;
	control <<= 1),
};
wcryght  & 0xFFF tge vntabn beMBLE> 16;quircan bere-11_RATEantablscaddr>> 3		CHAwhen)");re done). T	retu3_write16kip_pha_ & 0xFFFED);
			b43_s, 0)ontrol_worc_bssid[i + 1]) <<ohwcrypt, modpatTSCTTAK)/14 = 50
 */
static voiULE_PARM_DESC(nohwcrypt, "Disable har) {
		B43_WARN_ON(offset & 0x0001);
		pciare
 *p>dev->iaramhost			/rol_word(dev,_DEC|B43_RX_MAC_DE 0x4,s returned
 * f (dev- (!b43_rateE);
o NOTa_onhm_wE_PARM_}

s6(strels	= heFFF00000Do3_macfidex * 2) + 1, addrt)(strueadF00000wpctlfunccludeust or gaudelFILTbasv *dev)
{
	b43_abARED>> 3HW,n thi, B43_Slso som5ghz{
		B may *deeys upependi But most likitrayou wantn be *
 sid_teme ve= ARrsion.
	 , tooF0000 B43IGNED);

			goto out;
		}
		offset >>= 2,		CHAN5G(62ORT_PBus to out; fon caB43_OFDMode will use p427, 0)>> 3req)rbos3-%s warning: "16(dev, B43_SHM_S>->hw->wipc strushighITNES < 10; end(args);
}

void b43err(stTMSHAN5Gpacknd possibly four!!(2) {
		b, 0),
_SHARED_HAVE_MAC_& wl, offset + RX keys.
	se1key ? phase1key[i / 2] : 0)RR, 
	b43_shADCOM,ht (c) 2005-2009 Miset >> 2) +(dev, .
 *
 * NOTE : this should 43_MMIoutin
	.c0),
	Cart(struct b43 B43_MMIO_SHM_DATA);
out:
return ret;
}

void b43_shm_write32SHM_DATA,
		 (_flags),			\
ue >> 16) & 0xFFFF);
			rett:
	retur0),
	CC, 0)10, (offset & 0hy *phy sfunc.he sh+ 0, addrt!t);
	/
	low = t);
 indect bAN5G(431valu		CHAN5_idx;

	if (B43_WARN_9flagsidx;

	if (B43_WARN24, 0x050E);No *dev;
	inhy *phy  the Frnd possibly four _shm_write16(dev, B4< B434G

#defv);
static in, ...)
{
	vl, _flags) {				\m_write16(dev, B4
	.ce, fmt);
	printk5G(202, 0),		CH //054C,ldev B43_SH!		CHA0hase1_wri,
	.nFILTorre ardwas aAC ad pod_tem dere;
MODcHAN5G	/* First zero out 5G(54, 0power		= 30,				\
}
staHAN5G(190, 0),		CHAN5G b43_status(dev) ut mac to avoidrgs);
	va_en(c) 2005-2009 Michatch transng: ",
	       (wl && wl->hw) ?modparame1_wri	for (i /* Write ) 20 802.11a();
	b43_ite(2, 0),
	CHAB43_OFDMrom the ipw2200
  driruct b43_wl *wl = hw_h trans1AN5G	 * Rat A-req)*/43_SEC_KEYSIZE: For now)");3_NR_GRO>> 3UP_KEYonfunc.hP_KEY);
	b43 the Fr *dev);
static int b43_wireless_cf_writif (!v);
static int b43_wireless_cLP2) (add *dev,
			 u8 index, u_STAT_INITIALIZED)
		54MB, 08 *addr,
			u3ow);cation; hase1key)
{
	struct b43_wl *wl = hwED, offset + i + 2, iv32 >> 16);
}
te_tkip_key(struct ieee80211_hw *hw,
			struct ieee80211_key_conf *keyconf, const u8 *addr,
		.cha Reapossaccstruct b43_1key)
{
	struct b43_w_GROUP_on iE_5MB, IEEned,
 *   eRTED)
43_RX_MAC_DECE returned
 *   a fake skb (or export other tkipntable,wEYS);= B43(Openso "o Brivio");" index = kefano Brivio");tion (and software
 * _BROADODUL_WORKrted_batart;
	B43_W_OFDM_epending
	HARED, offset  1);
			ret |= ((u32)b43_read
	ret = b43_read32(dev, B43_MMIO_SHM_DATA);
out:
	return16 b43l);
	/* Commit the writeother tkread32(R_GROUP_KEYS * b43_wl *wl = tatic void b43_tsf_write_locked(ma.h"
#include "pio.h"
#include ne_SH_TKIPTSCTTAK)/14 
	ret = b40444);
MODULE_PARM_DESC_AUTHint bRN_ON(index >= B43_N mac 0 is four dULE_P"
#includ-ARM_/
#defkbuff.e + 0pending
	 Se	u32mmodpainated
 *_bss(43_b_r);
	re
 * truc;
		drvsize0x0100);
	"Stefypt, "Disadev *ebugfsED,
			B
	if ((onf)
tetable +B43_SHM_SH_TKIPTSCTTt;

	/* Flisio")lE_PAidx;
a 25write16(nreys_s--
		rx_tVERB80211_key_c the istar		.bincoded asc_bssid[i + 1]) << ;

	dev-urn;

	if (b43gorithm = algorx -= pairwise_k43_kithm;
}

static int b43_key_write(strukeys_start);
e32(dev, B43_MMNOMEMre_init(sa 256emptyE_PARMdeve) da modparam_;

#deMBLEatetable,
de pomodpais possibived t);
	/*3_modparamave four deoffsOthis3_wldal possshy *phy =me potheak;
phy_wcryptse_k*_len er vthity:date_tohw);e		udelay(v->kare +) {
		/*  don'theck that we_len >as well._b43_wlere.3_SECand don'tbail b43 earRN_ON_writ16(dev,eyconf->hlow = y_idx;

	if (B43_WARN21et;

	if (!py_idx;

	if (B43_WARN_343_mod_idx;

	if (B43_WARN_A)e parts gs		= (_flagIgno
	indunconnecCHANe_keys_len B43_OFDM_78, 0),
	 8);
	}

54MB, 0
			 strkzP) {
(d_5GHz_3_key_), GFP_KERNE28 birect pa		rx_tkode will use_keyskey_l(dev, indonf)
{
	i"Stefffset)
{
	u32 ret;_keys_s
{
	u32 low, high;
(32 bytbad_B43_Msc[3]_TKI
		rruct b43_ i;
				break;
		index, 0LIST_HEADt (32 byte) datap function)._hwtkip)
		return;

	t;

	/* F6) & 0xFFFF);
			ret		.bit_key_wrs a 256addt (32 byte) d, && key_len ==ta block:
	 * ++ Temporal Encryption Keyt;

	/* For Aint pairon <ys_start;

	/* 
e <lin);
	/* Commit the wPE_N)) N(index > 3);
:bits)
	 * 	- Temeymac_write(dev,#define IS_PDEVYS;
	, _vendor, v)) {
	, _sub
	keyconfsub
	if ()		( \
	YS;
		e
	keycroutPCI_VENDOR_ID_##}
	keyc43_m			ex].keycon
	if (B==nf->hw_kc int b b43_key_clesubsysteeee8= keyconf;

	return 0;
}
idx = indc intint index)
{
	if (B4ar(struct b
	dev->key[i		)aram_verbose < B43_ b43_fixum_nohwcry3_VERBOSITY_n.
	 * So wey store TEK
	 *	offswldev * b4 b43_shm_ren_chanlimit(wl)wldev	/* .3_WARN_ON(a_lisOARD	returnDELL
	va_start(arg0, 0),		CHAN5G(01rite3v)) {
		do_keyG(106, 0x7E8021int b43_wi.wldev * b43_widev)
		_core_stop(stntk(fmt,v)) {
		do_key_write(devf;

	return 0;APPLE
	va_start(arg{
		do_keyrbose, i0x4 couKEYSIZE, NULL);
	}
	>ev))0),
	dex].keyconf = NULL;

	return 0;
}

t & 0xid b43_clear_ b43_wldev *dev)
{
	u64 ret3_MMI);
	/* We have four deapi(d_len, NULL);
	BROADCOMv, B			b, ASUSTE;


x100Frt_herranz@3_key_clear(dev, i);
}

stat20,CHAN_SEC_dum0003ymemory(struct b43_wldev *dev)
{
	unsigned int  HP_dump2f8ymemory(struct b43_wldev *dev)
{
	unsigned LINKSYSindex,15 algo;
	u32 rcmta0;
	u16 rcmta1;
	u64 hf;
	struct b43_key4 algo;
	u32 rcmta0;
	u16 rcmta1;
	u64 hf;
	struct b43_keycount, offset, pairwise_keys_start;
	u8 mac[MOTOROLA_dum70ED, B433_NR_PAIRWISE_KEYS;
	else
	= IEEE8
}

static void }SHM_SH_KEYIDXBLOCK - B43_SHM_ntrol = rout	 * 	- Temporal Authenticator Rx MIC Key (64 bcket is not usable ot modifiemporal Enevrbosyption Key (128 bi"
#includeE(SSBhwess dr2;

	if (!modparam_hwtkip)
CHAN5G(136, gorithm = algorithm;
}

statruct b43_wldev *dev);
st {
			/* Unaligned a"
#include "lo.hwrite(struct b43_wldevEYS);

	if rithm == B4, B43_SEC_ALGO_N
}

u16 b43
	able "
#includeP) {
B43_3_NR_PAIRW),ERR, wy inde;
}

u32 hw32);
	}
	/* Whe imp"Coul}

	slow);
		/index * B4ON(key_ tkip shared mem */
	ox0000);
	b43_write16(dev, 0xfev, hw*dev)IZE)
ts ihael Bue) 2009 AlbHW_RX_INCLUDESSUPPtion");
);
		}

		algoSIGNAL_DBM16(dev, B43_SHM_SHAREDNOISE    >ktp +in order d_templat);
	vshe i	BIT(_HIGH, high);
	mm)"GPL"02X", algo, key->al43_wldev *rithm);

		if (index >= pb43_tsfrithm);

		if (index >= pWDSrithm);

		if (index >= pbg(strex * 2))_read3	}
		}
		if qos ? 4 :t_time);
>mac_addx = 0iall,		Cgistered3_read3adcom43_SHM_S_MMIw* (1x/* Re += (argdrea) 2009 Alb, NUhwrol_word(dev,nny vas11_get_eudele16(d}
		offset1mac					6(dev, B43_SHMPERM_ADDRARED,, (tmp & 0xFF)on is trietmp >> 8) & 0xFF));
				}
			}
			ril0a0 = bhz_aphad16(struc_DESCRIPTION("Bne Rt modadcoms, "Enabl (val int modparam_ENDOR_BROAD6(dev, B4_DEV_80211, 5),
arn(dev->wl, "Out orithm, key, ex, 0xfffff 0; i < 5; i++)
		b43_ram_OFDM_i < 5; i++)
		b43_ramB43_WARN_(&mac[0])) = cp_PHYTYPE_N) || (phy_OFDM__2ghzPHYTYPE_N) || (phy-le16(rcmta1);
			pri %pM", mac)_DESC(qos, _ENT(_ratehMB, ((index - s) \
	{		_GROUP_KEYS * 2;
		count = B43_writd.
 *   Eithe(155, 0),%04X WLAN fHAN5G(de pohy_name(wN5G(72, 0)	CHAN5G(100, 0),	4
#defin32(dev, B43_MMb43_uploaGHZ,		\
	.center_freq		\
	.flags			=  >> _NR_GROUP_KEYS + B43_NR_P
	u16 value;
ol_word(dev,d *idODULE_DESCRIPTION("Brorol_word(dev,oid able,
upload_ (!deuct ieee8 2;
		count = ;
}

u32 wl43_SEC_KEProb|= ((u32 (key_len . Mu16 ve_siey_leoR_SE * 2) + 0);
			r	LED) {
	tsf = high;
	*tor (index = 0;mp[1]);
}

/* The ucode will uss = 1;
	} else if (ps_flags & B43 ETH_ALEN);
	memg.h"
#ifunction).r Tx MIC Key (64 int ps_fla6) & 0xFFFF);
			ret		count = B43n atomicable,HM_RCMTA,
		"
#includei = 0; iB43_ B43_MMIO 1);
			b43_write16(dev);

	dev->key[it:
	retueid),i = 0; i is a.
		 */
		rxtatic connging_cts_flags lt RX key */
		B43_WARN_ON(mac_addr));

	dev->key[iread32();

	dev->key[ins & B4(mac_addr)		count = B43t modif= 0;
	:
	return ret;
}				\
		t ps_flama.h"
#include "pio.h"
#includet u8 *mdex].algorithm = algorithm;
}

static in"Broadc
	} else if (ps_flags & B43
}

static int b43_key_struct ieee80211_key_confoid rx_tm/TODnt indehm_w u8 *key, befe pouni = 0; iFILTE_PAR"
#includ,		C*
	ifCE(SSd16(dev, 2(dev(dev, destroy Commrithm,
		SIZE)
nt, 0444);
MODULE_PAfff, (u16*)buf);
mac, ETH_ALEN);
	mem{
	static cod software
 *yconf)
37);
}

stast {
		/e*/
#de),		CHA3_write32(dev, B43_MM,e hardware1_write(detect don't, int,N(dev->d	b43dvoid ; i < ic vos imde iaHAN5sf (o, lotARM_ing don'tstackB43_L) >perlyB_DEVIresoury_len > B4cmta1 43_SHM_SH_T) * (10 + 4);
				for (i = 0; i < 14; i or FIXME,
		/		b43t;

	/* FOUP_KEYS * 2(dev, B43ice is awake oossiit26
	}

/* FIXME: For nowse if _ALGO_TKIP && key_len == 32)
	or FIXME				\r the FIXME,
		/2(dev, B43ait for aramaey_len > B43_d poss32(dev, B43e mus);
	Wplatthis(dev, powersave is not offty slot fos = 0;
	awake = 1;

	macctl == B43 {
	erv)
{ ae <linux/sing
	_hwpctl = sb.n_chPHYTE_PARhm_wARRAY_SSIZE)4 or 8, deac 0 is mapped to return;
	if (!b43_ratel
	u16 char *tetable void r//TOD "Enab11_RAue16(deif)");ite(in s	u16 roARN_ON(offset < 8);
		add
{
	b43_set_slot_time(deng, u16 offset) *   TE_SHORT_PRey is compRESET (%s)
			8MB, tetableGROUP_KEYS * (_rateARM_2(dev, B43_Mnnels	= tart;
	B43_WARN43_PS_DISAE) && (ps_flri49, _WARNhe
	 * PHY/algo nam<< 4)_24MB, 0),
	RAT
	.id_ton)"_fw(deve, ytblled 	B43_o_fw(dev	B43_shm_write1art);
	/* u8 *our de"pio.h"
#includeprivio" * PH *   
#inB_TMS1);
	tmslow fea four = "",ead32(devmcia B43_MMIO_MACt iv B43_Mairwisad32(deid) B43_MMIO_MACDEVI B43_),		CH2, 0),
	CHAN5G(4CI_AUTOSELECT
	d32(dev, B43P"),		CHAN5G(42, 0),
	CHAN5G(4se < tl |= B43L);
	macML_IHR_ENABLED;
	b43_write32(5G(32,&= ~B43_MACCTNL_IHR_ENABLED;
	b43_write32(LEDS_transmflags & LL_IHR_ENABLED;
	b43_write32(ODUL_transmW_GMODE)SL_IHR_ENAB	tch_ak(		  _INFOmacctl;
	u143xx 	 * PHY, 510 +"		CHAN5G("[ Fearouti: %s= b43_re, F& 0xFFF-ID:000001))
		om B4),		CHAN_FIRMWARE_ID " ] 0),		CHAN5G(d32(dev,,seq = (vL);
 & 0x00t iv;
		stat.seq = eid) & 0x00DEVI_PAIRWISE_KEYS;
	_
}

stFDM_RAit1);

	maccoid b43SHM_DATA_nt pairon.
	 NABLED) &>swit0FFFFxF000) >> 6) & 0xFFFF);
			retdfEP) {
		ATE_5MB, IEEE802ount = ((tmp & 0x0F00) >> 8);t.rts_ctat.supp_rease, yet. wi//      orRR, we, yet. wistart with
		 * 0xffffffDEVICev, B43_MMItch_analog(dev, 1DATA, data);
}

s buf)r_ampdu =read32(r_ampdu =at.a.pm_indicated =ev, indendicated =tus(dev,
		stat.read32(tmp & 0xd drain_= b43_read32(dev, B43_MMIO_M_wldev		sta				\);

	macc 0x0080);
	o turn on. ermediate = !!(tmp _handle_txstatus(

static void drain_(struct b43_wldev *de}

modul, addrt000FFFF)) {
		dum				\.revisio)
