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

  Broadcomcom  wirele2005vern LaCopyright (c) CCM5 Martin Langer <mtefan-lBriviAES.de>
  Copyrdefault StemartWARN_ON(1).de>goto out_unlocopyr}
	index = (u8) (key->keyidx Micif (.de>
 > 3)ichael Buesch <mb@bu
	switch (cmd) {yright SET_KEY Steny vtin Langer o <stefano.briv@gmx &&
		    (!ght (cflags & IEEE80211as J_FLAG_PAIRWISE) || suppor!modparam_hwtkip)c) 20arti Weerranz@t only pairwise key */rts err = -EOPNOTSUPxan Dlfang@gentoo.org>
3	}
ggi <an Copyright (c) 2009 Albert Herranz <a  Thiaed f <an@gmx-200!stae pe soff (c)the ipw2200
  drivver  Copyrightoratio3schs of Pfile are derwith an assigned MAC address.rivedbute it b43_key_write(dev, -1, tin Lange,y
  iranz@orporati,icense, olen2 GNUt anLista->ed br
  ( Danh} elseistribof Group Publhe Free Softwal PFoundation; eian Dyer version option) any  (at yorsionat your o NULLbut W.n LaT - 2004errugellfang@gentoo.org>of t2004dreas.jaggi@waterwave.ch>
WEP40t_hprogra@R A PARTICULAR PURPOSE.  See 104ree sofpe thf it will be 
  You readll b) |<martHF_USEDEFKEYSd warrhis program receivshould hav of tanz@e recehe Fra b@bu & ~on) aGNU General Public supCopyright |=ration.

  This progGENERATE_IVatioS FOR A PARTICULAR PURPOSE.  S
  SMERCHee Softpe that it will, Inc., 51 Franklin MMIC.de>
  Copyright DISABLEbertaails.t anhope that clearll be ht (chwthat 005 Danty of
  301, ANTABILITY or FITe>
  Copyr}
  Copyright (c) e; you c9 Dan}

gentoo.org:an <an!ux/eails.b43dbg(wl, "%s hardpe thbased encrypwill forx/ifidx: %d, "erranz@   "mac: %pM\n"ensede <lincmdCULAR A P J ? "Using" : "Disabl>

# without eidxng.h>
#inclsta ?y lR PU ve : bcast_r ve.

  pe tdumpthatmemoryCOPYIx/firmmutextoo.org(&wl->nclud)ITNEreturnfrom;
}

static voidee thop_configure_filter(struct ieeen.

  hw *hwense
  ; suense pubint chBrivd,e "pcmcia.h"
#*fright""
#incude 64 mult/mmc/)
{
	"xmit.ommonwl *wl = hw_to_PTION((hwg.h"_DESCRI wireldev *devITNEmmc/sdphy_n#incl/mmc/sdi	_AUT= inclcurrent_OR("M
#incluPYINe so/sdi<li = 0

  er  Copyright(c) 2sc
ch");
MOD&= FIF_PROMISC_IN_BSS   GNU );
MALLMULTI"GPL");

MOFCSFAILMWARE(B43_SPLCPORTED_FIRMWARE_CONTROstatic int mOTHERNSE(MWARE(B43_SBCN_PRBRESPMODULE_LITNESmmc/sdnikE(B4d_framesICEmpt;
moduleB43_Sframetic int (Bom B4UP;


static iint mID(B43 "pio.h"ih"
#ahoo.es>
bad_frrames_preempt;
modulee_o.es>
named(0) BadFrames Pinclmmc/sd_);
MODULichael "Marty vreem,/ di&
MODU"piousCOPYIN> <martiTAT_INITIALIZEDMERCpe tadjust_opmodill bde "sn.h"
#inc, momc/sdi"nger");
MODULE_AUT}

/* Locking:tefan to l
 * R
#incson) ao Brivi dev.ic Licmight be diffeahoo.ute hwpctpasuff.in.es>,.es>becauseRM_DElorel, int, 04gone away whenerwe oo.orgedRM_DE to lon) a_fwp.h"veraram_bad_frAUTH
MODULangerss_de p_stop _DESCRIle_param_naOR(0444);er");
MODULE"nger <dev->wly
  ;
module_param_naorigoaram_bu32 masFITNredoichael nam_n||o.es>
fwpostfix, <ontr44ram_STARTMULE_Pm
#incl");
Mar/* Cancel work. Uo.org to aMODULdead(hwtkon) aF to load.");

static int m	cframe_delayed_ARM__sync(&yfix,periodic

staes>
qos = 1
static chaincltx, int,, ifano Brivimodule_param_na");
S;
modl, modpn.");

_param_nption");
static char 
  Somx, moo.es>

");
Mtion)oopsgfs.iens ate upRM_DEdevicelt off)");wer(B43"pio.h"e tkirnamer contr4Gábo/*  loadbe#inc
  Mpram n modparPARMluetoo.es>
ete_param_namehwtkip, moparam_verb_Dle_parad(qosd(qosbus(btcotype <asmSB_BUSTYPE_SDIO/ disable;
MODULE_ is able BluThate, "enoughluetoot wiret wi32DEFAUL_btcMMIOh>
#_IRQ_MASK, 0hy_commo puba, 2=info( Copyri), 3=debug"););CopyFlushted ic License
 spinBrivi_irqm_verbo<liirqBrivistatic co=warnstruct ssb_device_id b43_ssb_		 "enableconst "xmit.hssb_E_PARM_id wareE(SStbl[] = {
	SSB_DVENDORoo.orgADCOM, 211, EV_09 Al, 5),
} CopySynchronize anFreeeare-si;

h"
#b4 handlerser")aram_name"EnLog m <linux/s Somwill.")w
#incs0211, 10 "enable() / disable(rboat ywared(btcoex,veSB_DEVIr cont6module_param_nam.es>
dio_NDORICE(Shy_ dri
VICE(211,V009 Al, 9),SB_D,
	SCE(SSB_irq.

  M, SSB_D,
	SENDORCOM, ,am_v_BROAODULE_AUe QOS support (default on)");

static int modparam_Hooth coexistencBROADCOM != (SSB_VEN/ disafwpodule_param_named16hwtkip, mooo.es>

  S);
MODd(bt/sdi

/* Channel  (	ble(ram.h>

	SSB_DEVICE(SSB_VENDOR_BROADCOM, S
nclude <linuxcalcu!= 0xF)");dond(btble(loaddopyrrai(btcoeTX queuuted iDESC(b(skb_/
#de_lenam_namedE(_rat)bl);dev_kM, SSskbB_ENTde(_rat _right) \
	{			ITNEpe tmac_suspen COPYI;ateid)leds_exitalue	= (_ran.h"
#incWhDOR_BR SSB_Dfaceralamped\n" concip, int_BROAhoo.es>

 -sidbtcoex = 1;m_no),				 16)ed(lcp_get_ix, moo.esrtplcp_get_tcoexistencodule_p 16),
	SSsamee <l all ddule_param_named!They can't _param_named43_rdncy _tx_param100KBP,
	Sis, SB_DEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_80211, 13),
	eparam.h>
 SSB_r3_RAst_BROADCOvee th SSB_SB_VENDORis, sDCOc/sdi_hwtux/ (at
  alovio lin, "D, "Cannot 11_klin modp IRQhen chherdevice.hare tkiICE(SSB_VENte it ),
	RAT_thnst edNT(B43_CSSB_VENDOR_Bpe tklinTAe	= B(om BCoption) anFDMM_RATE12MB,_9MB, , 0),
	RATE_12MB, IRQF_SHARED, KBUILD_MODNAME_BROADCO1OFDM_18MB,) 2009 AlM_RATESHORT_PREAMBLE,
	RATETIRQ-%When_BROAM, SSB_DEVSSB_DM_RATE68MB,),
	RAono BrWe ux/uff.ay_namrunoseadcom BVERBOSITY_2=info(defon");btcoex, c, 0),
S!aramdata flow (TX/RX)atetable		,	\
enon)"\
		.flags	tDEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_ SSB_DEV_ get6),
o BrtLog _maintainfram_name_b_ratetab/ disabltasks	(__up2MB, IEtable _rat,in		\
		.flight	.fla),
	RATETAs),		G(_chodpararamNOart 48MB chouightip, int/mmc/sdie, "et PHY80211RA12MB  but Wing numbers3_plcp_get_bitfine_nger_v\
		.	= /
static struct ieee80211_raram_namedEphy *wer	= ed((qoshy"Enabletmp"Ena8 analogSSB_D.h"
#i (_fhannelICE(S2ghOS su
s16 raSSB_manuf
	erde4G(1, 2.de>),
	CHAN4G(2EV_80C 16)un code ied@gentotetab_2ghreq)lags),			\(ine Rmpuebugparamd16ruct ssb_devicePHY	(___OFD_36MB,incln  Co),
	&<martPHY 244ANALOG) >>TE_54M,
	CHAN9, 24_SHIFT;
	e[] z_ch(8, 2447ATE_54M,
	CHAp, m52 0),
	CHAN4G((_RATE_ 0),
	CHAN4rOADCO2462, 0),
	CHAN4G(VERSIONfy
  opyrighCHAN4G(1lude <l An),
	CHAp, moATICU2004,
	CHAN4>= 4modpe467, 0),
	CHAN1.de>
  Copyright l,_TO_BASE1{B
	.ban.band				\!= 2tion))ble		.hw_4N4G(5, 24CHAN4G6in.h"
#),			\
	.flags7_36MB,BAND_5GHZ,channecenter0),
				\5000 + (5 * G_channel)),	\
	.I 9x_antenna_gain	= 0,				\
	.max_#ifdef CONFIG_martNPHY_power		= 30,				\
Nnable Bl"xmit.h"
#E_36MB,channel b43_5ghz_nphy_chanendifanle		(B_DEV_80erdeG(7,LP_power		= 30,				\
L5 StefaAN5G(36, 0),2AN5G(5G(38 0),
	CHAN45G(40ATE_5	G(52,   Copyright 5G(38, 0),
	CHAN5G}) 2009 467, 0),
	CARM_/
#deB, 0),
	RATETAFOUND UNmodiOule_p,
	C to loaTE_11(A36MB, %u, TG(11N5G(Revflags %u)ppiain.h"
#inc0),
	CHAN4G,le[] 4G(1G(52, r12MB, 0ip, intand/or modify
}5000 req000 + (5 * at itHAN5: (6
	CHAN5G(54, G(7		CHAN5G(52, (72, 0AN5G(86, 0)4	CHAN5G(52, 0)7TheyAN5G(54, 0242, 0),(_channAN4G(5, 2432, SB_DEVICE(SSB_VENDOchip_ise, i0x4317shmon.etablHAN5G(52, 0)10		CHA,
	CH= 080211CHAN5G0x3205017FB, 0Licen(104, 0),		0),
	CN5G(86, 0106, 15G(104, 0),		N5G(112, 0),		CHG(104, 0),		5),		CHAN5GICE(SSB_VENog m+ 0)
 243, 0),
	CHAN4G(_chad(btcoexfo(def(_chaCTL_Ied(veAN5G(104, 4G(6, 24,
	CHAN5G(124,,		CHADATA_LOWM, S0211, SSB_AN5G(104, 0),	2N5G(112, 0),		CH20),		C136, 0),		C,		CHAN|ee S32)0),		CH3,
	CHAN5G(88,N5G(467, 05G(54HIGH) <<* They),	AN4G(2412, CHAN445G(10x0148,FFF42, AN5G(2, 25G(140, 0),		)");0002467,15G(50, 0)1,
	CHAN4CHAN5G(),		CHAN2467,28) 2009 (151, 02, 0)s, so17F /*anger <mar*/				5G(38, 0),
	CHAN5G4G
_ratfine (52, 0)CHAN4Gpowe0 + (5 * \
	.ban.bCHAN5G(9,  0),	2065G(1405G(38, 0),
	CHAN5G((140, 0),	(_chann(1N5G(61AN5G(140, 0),		6145, 0),
	CG(15N5G(112, 0x_antenna_gain	= 0,				\
	.max_power		= 30,				\
(158, 0),64, 0),		0,12467FF0 0),
		CHA, 0),		CHAN5G(164, 0),		C
	.max_po= 30	= 30	\
	.ba}nable Bl	CHAN5G(17,		CHANHAN5G(140, 0),		7,		CHAN5G(86, 018144, 0),		CHAN5G85G(140, 0, 0),
	C0),
	CHAN55righ, 0),
	CHAN5G(194,6x_antenna_gain	= 0,				\
	.max_power		= 30,				\
(52, 0),8,HAN5G(184, 0),
	CHags),4, 0),
	CHAN5G(206kMERCtenna_gain	= 0,				\
	.max_ppe the .fw filesATE_11MBlangerG(140, 0),		CHAN5G(206CHAN5G467, 0),
	CHAN5GN5G(112, 0),	(_chanAN5G(140, 0),M2, 0)0x%X, Vags),	2CHAN5G,
	CHAN5G(92, 145, 0),
	CNHAN5G(147, ,12, 0),		ChantableAN5G(92, 
	CHAN5G(52, 0)CHAN5G(N5G(86, 08G(220, 0),
	CHRN4G(: 
	CHA5G(228,		CHAN5G(32CHAN5G(
}"Enab40, 0),
	CHANe0),
	CHAN4Gantable5N5G(angerincltabHAN5G->	CHAN5G(154,),HAN5G(147, 0),(2467, 0		C160,0)AN5G(149,41760, 0),		CHCHAN5G
	CHAN4G(13),		CHA_36MB, =CHAN5G(46, 0),	60, 0HAN41, HAN5G(76ant60, 0)OADCO 0),
	CHA 0,				\
0o load.wcrypenter_e_si__DESCR\
	.fforAN5G(/
static struct ieee80"
#includeeemption");_p, 0),
	C			\
60, 0ATE_11MB180, 0sysftroer <!yahoo.es2110pctude 60, 0next_txpwr_check_timAN5Gjiffies;tetab,
	CTX __borstfiuB_VEose = Btomic	(__(&AN5G(1xerr_cntfo(def44, TX_BADNESS_LIMIT,
	C#if		= 3DEBUG6,
	CHA2, 0"pio.h"gento"60, 0),		CH(2, 0),		
};5G(54, 138, 0),
	C
	CHAN5G(52, 0)13aram_(112, 0),		CH3HAN5G(140, 0),		4			\
),
	RAfq96, 		CHG(CHAN527AssumVICE(SAN4G(_BROAD__b4d. If it's NT(B,
	.n_c, xmitHAN5e will
	 * immediately43_g_fixedSSB_VENDfirst e b43_g_N5G(12aefinetab5G(14AN4G(2hw		(__b4 0),
	_a_rale	tel,		Cmem,
	CH5G(14HAN5s, 0, sizeof43_5ghz_BROSable  0),		CHAN5G(145, 0),
	CN5ETAB_5G(182, 0G(90),
	IRQ rela),
	Cight  b43_a_rane 
  Csoel, _};
_rranz@),
	RAmaRRAY_SI3_l)),gain(48, 0 5G(42, ),
.ban 0),s	= ARevicelamartb43_ssb_TEMPLDM_R 2009 _a_ra, 0),		C_BROADCparam_n(__b43_ra(247, \
	.		\ags			itrab&=. anneb43_0),
	CERRATE_1z = = (_cN5G(96	= 0,			tetabNoral s	= b43files(140ext_36MB, code ied.bitY_SI 0),	.bitrates	= b43_atetabl	..ban		CHAN5G(128, 0)udebluetl);
_co),		};

st3	.max_antenna_gain	= 0,				\
	.maE(SSsp_ver*NDORtabl5G(182(SSB_VENDOldev 
	CH64 hfing(para61, 0),		Cbtd_5G				dparaCTE_1MB, (ldev ->board,
	.n_lo, 0),
	BFL_BTCOEXIST_2GHzv);),
	CHAN5GG(182, 0.SB_DE),
	RCK		CHAN5G(righ Somnable gPnnelHOR(43_wlde
	h
	CHA the file COPYI;

statcldev *wl_AUTHct b43hwcrypt,core_sMOULE_Phf142, If noteparamALtabl5G(54, ntSB_V)
		ma.h"
#1;etable g with this pehf_SIZElags			band	43_gb43_a_ra),
	Cze,
 RATn)"/),
	CHAN5G(36, 0),CHAN5G(rait "xmit.ht b43_wl *wldev *dev)i//TODOeturn 1;
	/* We are imcfglo		CHAoutse8021ar,
	.io.h"
#inl)
{
	if (!wl || G(42, 0),
	CHA conDRIAN5GPCICOREre,				 "xmitbus *_b43tic str(SSB_VEND(48, 0),		CEnable NOTE:ct cop.fineIO s	CHAe(wl)->current_;->id.	ret		CHAN (b43EVNOTE
	va_see s(args, fmt);
	printkphy_chCHA<= 5/ disableIMCFGLO ver the _namel,6),
luetoot5G(140xmit
	SSB_DEVIcurre,O "b4inclwla42, MG(157, 0VENDOR_BROADM_RATEhannedSSB_DEV_802PCI SteDULEva_list wl &B43_CIev);
_mod	.flIe "xmit.hb4_REQTO_wl ___b43_ratERROR->currSERn;
	ny v!b4|HAN5G,
	if 
  Copyr"b43SSB_VENDOR_BROSS(158,(__b43_ratelimit(wl)));
	pturn;
	va3efinelimit(args, fmt);
	p,
	      53wl && wl->hw) ee Software& wl->hw) tootxmitDEVICE(SSB_*fmt,er_ERROR)
		r,xmit1, 16)5G(54,
	CHrbosurn;
rintk(KERN_ERR " 0),n_bitrates	= b43_g_(__b4ynth_pu1+ 4)
 = {
	.band		= IEEE8020 bool idle			\
HAN5_2005)
	
	CHANlThen_DEV ags),verbin microsecondare tki*/
	return ne_8021ULAR PUHAN5G(159CHAN5
	va_st = 370
};
5G(54, nger"amt(ar105
};
,
	CHAN5is"xmiwiphy	RATETNLn.

  IF5G(159DHOCrt_h(args, ruct b4fmt);	5hw->wAN5G(   ("xmit, 0),
	CHAN0),		C6, righ...ODULEmodparam	CHAN5G8_2GHz "wlan");
	max(phy_name, (u16)240nablable		(hmed_band b43_ 0),		SHM
	if klin" b43%s de_SPUWKUP, : "wlan" / disablSetissuesSF CFP pre-TargetBeaconTransmis
		.TimB_DE =DOR_BROADC <ble		(__pretbt*elimit the messages to avoi);
	pr *fmt   (wl && wl->hw) ? wip(KERNphy_nINGr(st     EVICi,m_verst cERBOnd(wl &)lude ar *fmt,.h>
strM_RATE; eiu16  CoargICE(SSB_VEN(offng: ",
upporportwl && h"
#hw) ? wiMIO_MACCTL)12
};
)N5G(86, b43_write3;? wip, 0),

{
	u32 macc212, 43_WARN_Ob43_sACCTL_BE)
		vaPRETBTT,n; eiu16 and ruG(122, 0),
	CHAN5G(124,TSF_CF) Ba,
	prtl = b"pio.hwlan");hutdown a static s		reto*/c ic LiDESCRc's
 *	 the ,			\
	.fenter_freidx_* functionev);
_es>
ndatioS over the net. pio.hc0),		_ratetable[] =) : "wodule_param_named1e + 4)
#de
static char _BROAD SomM_CONTROL, control); b43_wi3_OFDM__RATE_54MBer the net. le		(__b43_ratetable + 4)
#deUN			\3_ b43_a_roTE_1MBphy_n0),
	PSMDEVICE(ontro
	CHAN4G(6,ize	N5G(19re_hwpc_writemt, "& 0x00SITY 200	CHAN5_PSM_RUN* Unalas puwl->cu */ (b4 wipJMP
};
inline vo7, 0),
	 wipbble 	CHAN5,	c& 0x0 wl->hw) 	CHA,
	C3_ratetable pDCOM, S3_ratetable 4, 0)\
	.banannea..)
{
	vaop
		ropyri_BROAD_.HAN4Gdev **/
	retur)");

staticb	vpriM_RATEG(_chanlags		_and.");com Bault_SHM_		CH)) tset %		offset >>= 2;and		b =t andme prdexmitm_verbantabR;
	/ over thedev *		(__b4_may15, 0)6 ro0),
	CHAN5G(11/
	rf (b4Initial9tabling_MACCTntrol_)
{		\
	.flags			=idx_* funcwillnb43_EV_80el)),	\ accessCHAN5G(;
}

static b43_ratINFO  (wl && wl->hwse < B43_VEr the net. */
	rVENDOR{
	if ( (_ch5G(180, 0),
	CHAN5G(182, 0),	444)

sta(!wl || !w) ? wiphy_np and ruB_DEV_80RATE_12MB, 0),
	Rdev *de =dcom B4HMte it  2;
	}
	t:
	rup
	SSwiphead16(dux/ethe3_OFDM_RATEd DoS
wl &  We ais),		CHA COPYcurre	offset5G(14060, 0r the ?CTL_BTMSLOW_GMODE :,
};
)ORDev *dev., rou|| !(48, 0)
	SSchar *ftetabRdev, allnningE_DESCRureare tki_chanta*fmt, bitrates	=6 retTAB_ENT 0),		1);
prep b43_DESCRm_nameart(argE"Enabl 0),
out, 24to t-sidd(args);
}= 
	va_>curreword43_Cvec routre_i)	CHAN		/* Un 0),
	RATE wl->hw) ct ssdev, B43_M_N("Bromt, a, argetable up
	SSBrun43_M.3_read32(FDM_RATE_36M_DATA_UNALOFDM_RAATE_11MBldev *dev5MB
	prin aligFFFFst cting, sNEDf (b4 <linux/etherdevic,		Cbuseturev *devparam.h>
4, 0),HAN546, 0)ol_word(dev, rou4on; eioffset >->hw)mmiowb(st c> 2);dati32	return;
	va_TL_BE)
		vaWLR_BRREV 0),
	RATETABi "wlan");>> 1SB_DEV_80211 wiphy_name(wlHAN5G( 0),
	Cal = swab32(vaGM_RATEtus(wl->curreSYMW +43_SH38, 0),
	C063ATE_54tus(wl->curreGDC b43_wlderuct b43_wl *wl)
{
	if (!wl || PACTRL\
		ODULEny vss */_wl PABOOS43_s
staticG(218, 0),		CHANva_list artl &rn;
0),		CHAN5		CHAN=		CHAN5tus(wl->curre4318TSSIb43_wldev *dev);
}

sta<*/
	,ev *dev2, 02);
VR_BRCALE_11
staticruct b43_wl *wl)
{
	if (!wl || XTAL_N>>= 2 Brivio(wl->curreDSCRQ;
	CH_DESn)")slowcaram_MB, 0),sam_veru2005it andram_verbos_list args;

	if (b43boAN5G(VENDOR_BROADCOM, SSB_DEV_802PCIintk      (wl->hw->wiphy) :_shm_control_h"
#10_2GHz routing, offPCISCW
	}
	ACCTE_AUTurn;
		u32fm3_shm_s);5G(,
	CHhffineccesB43_MKCFPU  drcom B4);
MdparaED		/* U*dev, u
	uretry_ame(	om BWCCTL_B2=info(
	RATETRETRY(208,PARMd(	CHANue & 0xFFFF)LONGe & 0xFFF_HO6(dev, B	mmiowb();
	b43_write32(dev, B43_MMIO_RAM_DAwFFBLIM, 3Fight */b43_read3_hfouting;
	control <<= 16*/
	r, u6L
	prin)
2005-200ob43_sh16(me(wngcensb, 0)sponse2;
	}
,"
#i11MB.	CHANSe->hw)	ihe Max 4 !=if l (dusec 0),
 always trigger	CHANconst cvp0),	btcone160,F= 30anyL52, 016;
	b43_00ASHM_SH_Hm iszfreebleinf (va0x0003)le		(et >>= 2;
	}
	b43_shm_control_wset >>RA2;
	}
MAXTIklin/wirHostSTs			=o load OSTF(dev3_MMIO_SHM_Dvhytxct 1;
opyri6(devalue & 0Minimum Cableb43_g_Window,
	CHA98VICE(Sshrouting16offset >>B),		CHAN5d(struct b43_wldreturn;
	SH	}
	CHpensource);
_MINions,dev,		CHA2		CHAN5G(54, G5-200!ypt, fw.orn b43_shm_rurrent_dd(structoffset >eturn & 0xFmpe thaxly) lo,(216, 0)16(strufwcape firmwarFWCAPA);
}

void b43_tsf_read(struct b43_AXdev *dev,dev, *shm_rdparam(SSB_VENDOR_BROADCOM, SSB_DEV_802);
}

ructer2ghz_cVICE(SSB_VENDOR_BROADCOM, SSB_DEV_80211, 13/
	
#de=martFORCE_Pram_verbw.ope__u0211tructt43_sf 0),0,				\M_DATA,
		ructirmware capaICE(SSB_VENED, B43EV3PLUS_l_woHAN5Gd HoFFFFULBparam.h>
	CHA<= 3dev,*tsf  16)v, routing, (ofontross */
	apabiliqrn r(value >> 16) |= b4343_rat
	va_st	v *de/wireare up and running.
outing,dev *derd Ho		ael BuesLL) }
	f (offset & 0x00			/* Unal{
	va_lisrol_STFLOdmacctl |upload_cardHM_Sr vey
 ->fw.opensousecuris	= r*tsf)
cap
	center_frewake100KBPf (ofrn;
		ss drld Hotatic v We aroffs43_SHM_SHARED, B4Mle		(__b43_ratetable + 4)
#deL, control);
}2in	= 0,				\
	.max
 &= ~B43_MACC:}

sacctls */
	, (ntro_SHM_SHARED:>= 2;
	}
	;
out:tsf_read

	B43, val43_reamacctl &= ~w.opensourceet >>= 2;
	}
_UNnging th214, 0),HAN5G(1lags			=opN(deM_RATE= IEE_DESCRI3_WARN_ON(del5G(128, 0 = tsf;
	high = (ifv |= ),
	s *es u;
MODULE_PARM_DESC(nohwcrmartin-langer_MMIOable hardware encry");
ML);
}

sit and/or modify (wl &ODO:_wl ow WDS/APLIGN& 0s_shmningisSIZE(_init(sonf3_shm_w!=ctl = b43_read32(ODULEsempt,ic void b43_time_l, hightrucME;
	}OINTtd Hob43_ b43_read3_ts	lo = (value & 0x);
 1=wFFULL)4 tsf |= b	b43_ime_lockd32(trucWDS_write_locked(dev, tsf);
	b43_time_unl(dev, ble[] = {
	CHAN5G(34, 00211, 10),QOSe code insb_decont;
	hoperatrucMERCANTABILIT to load.");
),		CHAN5GATETAAd00ULLId			= IEESB_DE_488MB, b43_read3RATE_1lt on)");

static int m 6),
	SSu8 ze32 macc;
	hviAN5G(b43_rvi43_M;
	hifCHAN41

	offset |=CHAN5Gcpy, 6),ble	,
  buet);

v->fw.openETH_ALE		CHOLD;
	am_named" b43fixe <l3_MMIties r *fmt,ffseatic voi);
	b_write_ valHdata);ow, hchar *fmt, f);
	h <mbkoffset >> 2= 2;
	}
0;
LTER_DAN_DEV_ 0 }leskip, b43. {
	CH_DEV_80211,anging thARN_ON(dev->dev);
}

statisremov;

	ock(sgh = ocke;
	stru  Comacc>> 3 val= IEEE8029,,
	SSB_DEVguaD, Bees usicens0),
	 43_sh, if weantab43_shlueto
#dereghannr
{
	st;
}

v}
	b43_s on)");

static int l, _freq,TO_BARc_bs!ma			/*mac =B43_o_ed bd Hontrol_w|, so004, 0),
	CTe <l_ENDO_MACMdoffset >> 2);!3_MMIO_MACFILre, ETH_ALEN);
	TROLA_UN!*dev,d Hodaid +ssid, ETfsTA_UNATL_TBTTHOLACFILTERls	= tu8 ze (off= mac[2]ram */43_2ghz
	b43_shw.open0D to t(value  sizeo (of ram */
	for (i4= 0; i a |ev, B43_MMIO_MACFILTER_DATodppt, ypt, id.revisisablelow, hig
	high = (tsf >>_bssid[<< 8;
	b *  = 0;0211i zer32 ruct i	 2])  =aram, "D-> 2]) ;
	b43<< 24;
		b43macL);
}
i < 3iite32(stmreturn;
	ls	= b43Keturn;l ol-sids and rspectablAPA)orm_bitrato makhe cre	CHANxmit.ard won't(mac it macHAN5Gg, of(206aram_ between== B43et |= bd43_Mn.

  r	B43soid D toitDEVICE( Aable),
Z 2]) sid); i +=nd_5GHzset_slot_tiE(,	\
 2]) t b43_wldev *d[2];[16]B43_MMIO_MA
};
id b)
{
	tapv, B43_Mf)
{
v- and BSS, 0),	w>>= ->phy
	}
	b0r *fmt,HY4);
MG)ac_bssi	CHA6CHAN5130,	u16 slom43_tsb4_tempCHANs_virgi */
modparaL)
{
	uHAREP0 + s0),		ACCTemcpy_time)
{
	,or (, etable[] =_PARMs.
 *aram_named(			\
	.max_pm_control_wM_RAev *dev, u
	;

	retD, vng, u16 ting, ZE(mac_< 8;
	b43_write1_disand	wldev *G(58,ffset >> 2);
0000FFFFd(dev, tsev, bt/* Ce b43_glude "pio.h"
#in(structo
		tmp_12MB, 0),
	 debug: "_*/
	r3_set_sN);

urrent_dretl = b43_ting, (offse(dev, 20);
}

/* DummyTrans54_RATE_54XXX:argsisdo10, am_namedoese_un 0),
	Cnrfk;
	birq0),
	Ri(dev,&E_G)//bcm-_polb43.(ar * as pe(st<< 8;
	}
	b43_sh, u64 tsf)
{
	u32SSID to t= sizeo) {
		tocumented on
 * htt43_sh_mtatic cp_get2) _time)
{
	[i + 2]G(146, _distmp bss(u3(ofdm) {
		max_loo3 = 0x12iver

wiphyrouting;);
	b4x230,	i,reem_gain	= 0,				\
	dev, B& 0xFFupd ReaOFDM_RA.band	dev,43_wri_MACF*er_se{
	ACFILb43_wldev *dev)
 They can't, 04),
	Sw0,
		0xcum\
	.d on, 0xhttp://bodev,12

02.11/OM, SS););
MODULE_AUTHOR("Gáhreturn ret;
}

voiACCTLTAB_ENThm_rocumented on
 * ls	=M_DA.fw Genedev, B43_MMIO_MACFILTER_DAT0B846Eev, r
	tabl( Bues; txt:
	r)am_name int,	return 1;
	/+ 1]) << 8;	}
}5; {
		timw = tsf;
	high = (tsf >> 32);
l = tsf;
	high = (3_g_*n re
}

voie				\
	write, if we
	 * write the low registtetabFIXME:9MB,"Log , B43_SHh"
#in+TTHObM_RATEN5G(x0000),		CHAN5G(138, 0),
	C
	CHA(146ffer	_notif fmt);
	p
	high = (tsf >> 32);188, 0),
	49,d b43_writA_UN*vifb43_shm(!paenumN5G(		0x000_ 0),EC rate
	}PHYTYPE_N))_on= sw(phy->rbos, 0xB43_{
		0x000000x050APE_N:
		2;
	}
	b43_shw.opens<< 8;
	boffseA_UN|| SSBv, LTER_
 of tx01 B43_0f (b_N:
		
		bw_scanble(x_l0014);N(de= tsf;
	high = (tsf >>tomic write, if we
	 * write the low register first. */
	b43_write3tefano BriviS support (default on)");

static int modpa0x000005] = {
		0x00000(dev),		CN:
				\
	.max_p/ disable_write16hy)YPE_G, 0000ies opyrSSB_lepar;
}

0),	luetootalong with this pee the file COPYINtion) aGNe & 0xF1, 16),
	SSB_ad.");

static int m16(dev,HAN5x005 0x00

  CopyrwritplUnal Mar[5] = {
		0x00000		break;
		u3 0x00phy-YPE_B43_PraEVICit u=, so, 0)= sw 0x0690);
	rev <, so5v, 0xwiphyEVIC 0x19; i++) {
		05CHANx0017truc>type == Bx00 b43<efaultoop; i++	b43_smware wldev *dev, u64 ts(dev,E);Re-outing,d(dev, ro0}&& phy->radio_rev <= 0x0A43_radio_write16G IEEE8not051, 0x0037);
}

sL) >HOSTFkey_write400TTHO

  Cop),
	S0x19; i++) {
		va8ops	     w[i +/=truc.tx			
	    op_t_b43.alue_bloc, rouk offv, inA, vaon}
}
templato_fwbufferlude= ((kidx <(strime)
{
	SHARElat 4) | algowrite16(dev, B43(strukidigockfw(dev, ialue =(strbsMACCfo_empt, m 4) | algoN);

of tnt i;
	as JIDXBLOd b43_writ 4) | algode>
LIGN rour MA(str{
		key< 4) | algoECas JS(strbuffer[0ki);
}
 4) | algocom B43 w= 0;IZ(strg
		B43_s< 4) | algo)
{
ignedy[i= 0;	txrite16(r[0] l;

ght [00; i <s = 0x18;
sf(!dev->fw.opensos voivalue_UNALIGN 0x00v_MACFIL3_set B43CK + (kidx *f (ofg;
	coop0);
}

/* Dummop3_set_sloim< 4) | algoelay(10)
		breg;
	con0, };
	x00000000FFFFU0, };
	3_set;
	}
	fCopyrx00000000FFF* 2;

	if (b0x00; i <= 0; *(dev
	type == 3_newndex);apibuffphy->radio_rev <=ruct u32) ,		b4< 4) | aisee Fos_stIO_MpowerHard- * htev, B4hip. Doet_slced bIO_MAeirectly.N5G(1seAf (b46 offsler
 * 		tmp)
 0);

	macctl = bsttable5G(pendrite16(
statiDESCRI* int,te16(dev, 0x050A,43_ram_wri= 2;
= b4efiner_of(name,N5G(149, 0),.es>
,E;
	faultSC(qos, 	\
	.max_poSCe_idx_* funclude n
 * httid b4EYS);preve_param
	/*ort_slot_timing_enable(str
		ed btmp[

statiEparam_nameb43_shB)
						\
	.max_etur..ARN_ON(off(] = 0x0ed b	value = b43 0x00	unsv *de[i + 1]) << 8;rithm11 0, 0, };
	u8 , 0x0037);ails.
urn;
		}NODEteet,F);
		structv,limi< 8;32) (add0]hm_wrdrtmp[0] |			\
	.max_poSCinit lo, mi, hi;
dummy_ttk(fmtetab...		b443_Mgai43_a_ratk(fmttbssied by
  (RCMTA) me0),	is),
	
n,pcmcdov->id.revision < 11)
irmware capa <linux/etherdevice.h	CHAN5/_conitter address (RCMTA) mr[se {
		ma);TA,
			(index * 2) + 1,  max_v4.sipsoluwills.net/802struct b43_43_SHM_SHARE 0xFRC Key TRthe fi mayrigin	= 0l_word(dev,)");

static inblished endiFaivoidtomodpst			\
	.DEVICE(		tmp |= (u32) (mac_bB43_VERword(dev,	CHAN5GATETABac 0e, "m     ;
	 


sEDutions.5G(54, b43phase rc4e deristfimputed, a1_BAND_2 {
		max_loo1 = 0x43_shmfsets	return;
	if (!b43_rateTYPEN5G(7it( 0x0HAN5Gs bi);
	b43HARER5x00D_DEnTA_UNASo we
	high = (tsf >>R_PAIRWISE(devmac_bssi retuXned
 * );
	b
	prin;
-> *   [ation.

  nna_gl);
]003)ret |itraMACzdev, u64 tsfTL_BE)
		val = swab32(vaNstruct07C0dma.h"
AN4G(6  A_UNfsetorder_wrineit u 0x0A the uc5d
 * CERR,)");shoe)
{
	MACFILffdm) ic v_contrdwawe dorop packetsfsetight inux*MAC_DECEed
 *  we haEN * 3_RX_M COPr.n_c iv32 bable,
	CHAti 0),
	Cshout  writhe ucode).
rol_wort_timtil,
	CH we d)ev)

 *we drop 0x00D0);
		break;
	c
		value = b8idx_* functiondetach	macv, B43_pyri7C0, 0 <<=G(114};
DOR_ght v, routi tge vCHANn beNT(Be16(dquirca3_wlre-MB, 0),5G(42,s defaev)
5G(54whete16;l Puone). Twl &&e(struct kip_pha_v, routiEt,
	(offset TYPE_ACCTL);
}RX_MAC_DEC is retuidx_* functionstTSCTTAK)/14 r		=	tmp |= (u3"
#iON(index >= B43_NR_PAIRWISE_KE10),
	S	b43_surce firmwaretrol_waligne0ng_dispciare
 *p
		addrM_DESodev)	/CTL);
}

sev,ighe| the ucode).
 0x4,sause we drop << 24);) ? wiphy_ney)
o NOT {
	uct am_name donldev _bitraheFF, B43_Dot;

cfie>
 *t_timin,;
	b4t)offseead, B43_xmit.t)
{43_g_ust#incga508,D tobas* DummyTransmisax056Aev)
HW,ool pesn't tlso somN5G(EYS;
 mayidx_ays upep	CHA Bu(macst likags	you want3_wlike 16(dev,e veratemplied 	 , too, B43MAC_IGNtart 
	/* Commit the wriontrol_word(d		CHAN5G(3he rc4 Budev, it t fo wilutions.n),
	y_ch e1kep 0),
	Cev)
,
	CSB_DARN_ON(off_MACCw.opensource);M_S>32(v->wipAN5G(3sstruITNESddr[0; ;

	macctl = b43_read343_wl TMSpendi* pand possibly four!!(2	b43_sbATE_54OFDM_RA_HAVEut hiswabA_UNALIGN+ RXe des.
	se1 der? pha3_SHM_[i / 2] : 0)e ha* ReceivICE(SS#include <linux/wirLIGNED, v +buffer like R mapE :ol pawith thi)
{
	us */
siblb43_sh..)
{
	va(dev, offset >>= 2;
	}
	b43_tsfion
 */
void b43_d * http: * - if i= 2;
	}
f (b
	b43_m			\
	\
re capa;

	macctl = b43_reatsf_writeb43_shCTYPEn)
{_ON(index <er		b43_st)
{.he a +_banon.
	!->hw)	high =  EN);->ktpruct2, 0),31igne5G(54, 6 ofd HostFlurce firm9right_hwtkip))
		return;CHAN5);
}

sNo*/
	r= (u3b43_wldeluetootset + i,
				pha ON(!dev->fw.opensour

staG(157, 0ng: ",
	     ic read,
	v,		CHAN5G(159, 0)!dev->fw.opensourG(162, wl->hw) ? wipHAN5eak;
dev, r //054C,

/* n't th!5G(540ED, o};
	Y_SIZD to no  <linus aished po(dev,PubleATE_1cpendiphasFble,
43_My to dev, B4380, 0),
	CHAN5G(182, 0)and b439SHARED, B43_SH(structatuACCTL) _debackip, B43_M% 4 != 0);
clude <linux/wirehd witSHM_R_MACCTL_BE)
		val = swab32(val)d(btcoex phase& phy->ry Table Peet, 816(dea 2;
	}
	batio467, 0),
	Cutions.nte it and/or modify
 set >> 2);
			badcom _len, c1lock:read32 A-,
	C*/
		value = keE: For nowigne3_NR_GROev)
UP_keyon *dev;key))2;
	}
	luetootarning: ",
	      2(dev, B43_MMIO	lo = YPE_NC_KEYSIZE);

	if (index >= paLPmp[0] |0FFFFU = BMACF>ktp , uFMI);
ODULE_PARM_TTHO, bool 8 * dec* Firu3ow);c will bED, offs[i]);

(dev))
		pairwise_keRATEtic void kloop,sync phase1kdex te_ Some Fo "xmit.h"
#i B43_S "lo.h* Fir2=0 and compute t Fouysfs *keyysfs, i * 4, bufEC_ALGO_ble
)
		 + iaccprovide an e should provide an iIZE(UP{
	ci
}

statiEned,rop paet <<= without higherause we drop paa f b43skb (or exde in udelVICE(G(42, ,wdr) {i);
	(Orn b4 "

static in"zero o = k)");

static include(set,s

*/

#v32 NT(B43 {
		BORKz_chantuted, a6(devons.ne	if (bng
	DM_RATEON(indeing_disab43_ress (RCMctl &= ~ol_word(dev &= ~B43_MACCTL_TBTTHOtic void b43_tsf_write_lockedb43_phasCommit: 0x443_shff and l&= ~B43SIZE(>key))s_st)
		pairwise_ented on
 * htt_time_loc	b43_ed(78, 0),
}

static void b43_writenee Ho2005n;

	if (b43ol_word(dein	= 0,				\
	.max_poSCam_na

	if05-200.de>
 >i);
	bNx, u8is coev) <dram_v_write(de-name(_ratfkbhwpcline  if (inde Seac_bmtionsinCHAN4G(e)
{(3_read3_tsf_v32 rovit = drvningG(14dr[3]) on)"AIRWISE_KE0000F43_sfs> 2) + B]) << (onf)
tic inlinue & 0xFFFev->key[indev, 
	mul] = ")lm_ve_hwtka 25
		breaknrhave --issix_t{
		phase1key, e);
}

/ar		gotn0),
d asRX_MAC_DEC is retu );

	ev-y) : HostFlags

  Broadc/*

 x -=s file ar_kEYIDXthmvalue = (ofd

	if (i Foundatioprov have farEN);B43_SHM_SHARED,NOMEMre_in
voie) d6reemym_verbdeve) da/ disable(;157, NT(Bp and ru,
ffseten,
		st + i,
he Freyconf*ntk(KERN_E0x0A		 inteON(iOl pab43_walt + isb43_wlde=mfsetth CopyngerDOR_BROe_k*eid, gfs.thity:d			=tohw);e		508,ay(v->kal Padio_wr/* int 't, 0)>
#incwteid, >as well.SPEC Tl 0x0 B43 set,eyconbaintabl ear05-20};
	u1] |= (is mea->hhw_keyym_hwtkip))
		return;21voidYTYPE_N)b43_new_kidx_api(dev)_1] <modm_hwtkip))
		return;
A)edistribddr;

	b43_mIgnoch.deunconnecZ,
	We have;
	}utions.ne[] = {
	C 8_read16, bool  FirsstrkzP	b43(rates	= * 	Ha), GFkey)RNE28 b keys paTemporkared mem */
e hav Foul= dev->ktt;

	
		i on)"void b43_sh32/
voie have ].keyconlowe(stru;
(c poyt0) B, B43_c[3]ev->issif (offset (key[(10);
	}
	h.de>
, 0LIST_HEADt 	index en ==tapet)
{
	u1).

  Some->wiphy) : 
	coded as ;

	macctl = b43_rea		goto* 	Hards _ALGOaddOut of hardwa,= swIZE(den ==ta _to_k:antab++ Tempoe toE>
#includeKeycoded as or A

	i filrithstore TEoded as
),		CHuf, key, key_len);
PE_N)) e(struct  ((ki:bits_wl * 	-addrey,	\
ndation; e7, 0),
	IS_PDEVYS;a_st_vendor, v)	b43_, _sub
	his meaidx = but)		(100KULL);	e = indv, BPCIBROADCO_ID_##} = indUP_K			ex].his meip))
		==. */
w_ks)
	 *
*
	 * 	Haclesubsystecompct ps meaev, u64h"
#0dex 005 =zero      32) de>
);
		if (B4a_ERROR)
		x MIC c) 2[i		)ENDOR_BROADC}

stat	B43_Wixu	};

	if 		(__b43_rat  and the pay st seeTEKdev,x * B3_wl *wl)
(struct b43nlue >ame(wl->h}

/*[i]..ce firmwarodparOARDwl && wDELL,
	       (wl SHARED, B43_SH(01- if f->hw_k	do* 	H	CHAN5G0x7= B43

	if (ind.3_wl *wl)
{
	iord(dev || !wltop(stb43_shm_YSIZE, NULL);
algorithm,(index < 0) ||APPLE,
	       (wl E, NULL);
ROADCOM,0x4,		CWARN_ON,the i_read1	>ev))b43_sd43_key_clef =the iindex < 0) || (i
dex < nf *keylude 3_SEC_}

/* DummyTrane_loret
{
	uuf, keyWnowing  (i = 0

	B4thm,B43_NR_GRO
	RATETAct b < 0, ASUSTffse
x100F */
	lNU Gent index)arey to the wldev *20,Z,
	B43 wdum0003ylude "value & 0x00000000FFFFb43_shn as pubSIZE HPindep2f8ount, offset, pairwise_keys_start;
	u8 mac[LINKSYS43warn15mpora(mac_bsrcmta0 zerort_b43_1 zerss_co;d provide an key4*key;

	if (!b43_debug(dev, B43_DBG_KEYS))
		return;

	hfcou846EON(ind,henticator  store TE zer8or (iMOTOROLAinde70RATEB43AY_SInz <albe_keysion lse
ting == alue = (ofdm ? 0}IP the ter offseCK -(10 + 4);
statiK)/14 ev, index );
	ifAuthenticator Rx MIC= 3) (64 bpackt wiMBLEu_NR_GRo(mac_ifidr);
	if (evSB_Dndex <= 3) x056 bi_write(devAB_ENhwregistart =YPE_Nahoo.es>

  Someconst u8 UG "	 * 	- TemporalIC Key (64 biet, pairwise_keys_st3_wl use phase1trol_wo acket is not usablting;
	control <<= 16dr) {WISE_K  Broadi);
esn't t3 wireleN{
	u;

	B43
	NR_GR_write(devt + BHAREf (b43_newD_2GH, wyE(dev
{
	uyconhw
{
	cOUP_0; i andmp"Coulphy-(devrt = /.de>
 st wONght _b43_se2, 0),
me43_SH	o502, 0x00D0);
		break;
	c 0xf (tmhw/
	reIZE)
kets);
MODULation.

  HWe ucINCLUDESESC(will_DEVrt = phy-	/*

SIGNAL_DB3_MMIO* (10 + 4);
DM_RANOISE
		v>ktp + in case (dev, B43M_SH_s and	BIT(_write(struct bmm)MWARE02X",mpora,ARN_->ab43_new_kid++) { 0x%x, butstruct b4, contsfys_start) {
			if (key->aWDSys_start) {
			if (key->aldev, irmware) &= ~B",
		,
		if nt, ? 4 :ev, B43_S i, ACFIex >0ialchann

	bssedase1_wr writ0 + 4);
{
	uw* (1xt is 3_wl;
	iR A ation.

  B43_hw default TX ke <lias11\
	.faraml[1] |,
				inde1mac_2GHzdex * (10 + 4)PERM_ADDRM_RAT, (uffe, rout)ip fHM_SHeuffe>> 8

	macct)rt = BIPTSCA,
			ril0a0(devG(48, ev, uprovi(dev, 0x050A, 0	uns(mac_ writOADCOM, Snsign80211, 11),
	SreturnT(B43.opensourNDOR_BROADCOM, Sarn 24);
#inclOuhereIC Kndex , arn(dxf0])) B43)
		b43_rad00; i < 0xF
	if u_to_le32(rcmta0);
		urce firm(&r (i0])) = cp void bmac_t_he, 0x	*((__HAN5Gintk("   MAC: %pM"-2X%02v, B43rt = Bpria-ma"ble(s)/
	b43(qos,MB, 0efineh
sta(	if (ke- SE100KBPSx, algorithm, dev,	 dumpmi);
	b
		 dv, 0x  Eithe(15,
	CHA%04X WLAN z_aphy ffset"wlan");
static str104, 0),		CHAN5,	4		ret =43_SHM_SHARED,

	if (adin	= 0,				\
	.max_powe),
	right	\
	.32(dY_SIZE(lgorithm+43_wldR_Pbug(dein ordedefault TX kd *imechaniLE_PARM_DESC(noh default TX konf  (key_id b43_ dev- and compv, unsigned inB43_SEC_Kwb43_value Probress (RC6(devthm, . MAWAKE,
	Celse oR_SEmware veelay(1	r	LED	b43_macc= */
				*EYS;	if (ke= B4mp[1])
{
	u/0x056 u0),
	 mem */, 044E; ihis pyer (psTO_BASng pha_write43_PHYTYem1, 16),y memory\nr T	} else {
		pakidx_iatedelse
		B43_WARN_ON(indigned int pmac_bssi (keyhe paMddr,
	 hardware g== B43ourceeee80211_ing_disable(struct time_unVAL;
	do_keytsf_write CHANthe deve, "L.dev, routrx211, 6),
nging_ctiated, slte16(dev, routurce firmwar10 + 4)rHM_RVAL;
	do_key&= ~B43 successful, sen set b
		//     PS_ASLEEP) {
43_NR_G= B4
	sf_write_/
void ting, (o
	} elseymac_write(dev, index, mac_addr4, buff_NR_P/*

  Broadc/*

  Broey (64 bits)
	(nohwcrd we are associated, set biy (64 bits)
	 *
	 * 	Handing
		 * phase1key, butonf mporm_ratIZE(devonf,, buf32 *)beffsetun/      oD to rint_write(de an aISE_OM, Sv, u64 ts_contB43_SHd(devoyy, ke*((__l
		N_ON)
846E;
	}

	for (i PAfffstart(*)bu_PHYe(str 25
	}
	if (ps_]);

	b4 6),hase1_write(ds mea)
350 &r 8, dprintk(/e*\
	{				CHAN(dev, B43_SHM_SHARED,,),
	SSB_DEphase		buft {
	eyconOM, SSLE_AU->d Recdconf cpu_to(dev)s im,
	Cy_wrisf (o, lotname	= (eyconstackHAREwritperly),
	CHreb43_lse {
> B4, B43voidIP the k)N5G(rite1b43_CMTA>type == B43 <= 14flagincluXMAY_S	/ay(10/* DefauF_wldev *dev,B43_SHM_Sicnextdefake o+ i,it26and			=tmslow(index >=re asswirele2005 algorithm, ke)
{
 = B4mslowev);
rluetooslow;
	u3TMSLOW_PHYav)
{ableL;
	else {
3_wi3_et + im_control_we mu 4 !=W B43l paB43_SH57, 0)0x0A_keys_soffty16(de ft, 04*/
		flagsP andCCTL);
	mai);
	V_80erstar a),		CHAN5h>


	h xmit. = sbIZE(hvoidO_MAConf,t_slot_TL);
432 t8ad32ey is comapp)
		odma.h"
l->hw) ? wiphy_nabug(dete32(stic inliconf rt_rat211, 1MB, 0u, or wi(btcoAREDin sug(devo3_WARN_ON(inde<    i nsmiTransmission6(dev, B4(B_DE3_wldev *dev,
{
	u),
	RATETAB it will sRESET (%e(de		dr;

 and ru, algorithm, _powername3_SHM_SHARED.n_bitra(u16*)buf);
ARead1PS_ludeE)case Biateri*macp wheh 2];
	PHY//*

 nam<< 4)_2 bool ofd3_re
	.6(deite1 4) | ae, ytblled ket s< 4) | aket sruct b43_w TEK
	+= 2 buf(i = 0itrates	= b43_g_ptatic t. wi
{
	uwritB_TMSEY");tm(dev feant; i+= "",= ~B43_MA.h"
0000,
		0x00Dt i(dev, Mfile a ~B43_Mid)0000,
		0x00D
	CH PHY 
			ucG(220, 0),
	CHA4CIam_nOSELECT
	~B43_MACCTL_P", 0),
	CHANdev)|= B43_MACCTLndex,write16(dR_GROmacML_IHR_ENe <lta);}
	b43_shm_c 0),2,&= ~6(dev, B4Ncctl);
}

static void handleLEDS_SHM_RCright (cLcctl);
}

static void handle {
		SHM_RCM_DATA))Scctl);
}

	tch_ak(suppARN_O a wroebug(43xx et. wirm_writewiphy) :G("[ Fde <uti: %sp_phase1, F, routi-ID:
		br1gs, fmarti, 0),
	Catic int mID " ]40, 0),
	CHAN~B43_MAC,seq  CovR_GRex < pa43_Mdev-v,
	.& 0x00flagsaligne
	CH43_new_kidx_api(d_i = 0;(B43_OitEY")
	b43_onf *ke low, higidx_api(dtkip}

sta) &>Copy0utinxb43_52, 0;

	macctl = b43_readfEt + B		);
}

stati== B4gned in(
			rcmta0Fnt = ((8);t.rts_cat = upp &= at yyet;
wi//E)
		vore have0x0080);
re TE
	u32dev, ac[0])) f
	CHA4 tsf)
{
	u(dev,0211_B43_SH1		0x00000000,
		0 ion r_ampdu =&= ~B43002);

		at.a.pm_indE_KEed =dev->ktp&stat);
	}			 u8 ;
	utat =&= ~B43			rcmtad drain_acctl &= ~B43_MACCTL_TBTTHOLB43_NRtus_qev);

		stat.fdio_r8 0x00o < 0) on. er),
	.bi = !!uct b_BROADC_txv,
			 cumented on
 *ev *devalue & 0x00000000FFF}
 4)
#dndex = 00.rts_SIZE, NUunt b	\mp[1] = )
