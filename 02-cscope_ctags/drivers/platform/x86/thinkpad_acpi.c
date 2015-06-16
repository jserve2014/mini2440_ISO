/*
 *  thinkpad_acpi.c - ThinkPad ACPI Extras
 *
 *
 *  Copyright (C) 2004-2005 Borislav Deianov <borislav@users.sf.net>
 *  Copyright (C) 2006-2009 Henrique de Moraes Holschuh <hmh@hmh.eng.br>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#define TPACPI_VERSION "0.23"
#define TPACPI_SYSFS_VERSION 0x020500

/*
 *  Changelog:
 *  2007-10-20		changelog trimmed down
 *
 *  2007-03-27  0.14	renamed to thinkpad_acpi and moved to
 *  			drivers/misc.
 *
 *  2006-11-22	0.13	new maintainer
 *  			changelog now lives in git commit history, and will
 *  			not be updated further in-file.
 *
 *  2005-03-17	0.11	support for 600e, 770x
 *			    thanks to Jamie Lentin <lentinj@dial.pipex.com>
 *
 *  2005-01-16	0.9	use MODULE_VERSION
 *			    thanks to Henrik Brix Andersen <brix@gentoo.org>
 *			fix parameter passing on module loading
 *			    thanks to Rusty Russell <rusty@rustcorp.com.au>
 *			    thanks to Jim Radford <radford@blackbean.org>
 *  2004-11-08	0.8	fix init error case, don't return from a macro
 *			    thanks to Chris Wright <chrisw@osdl.org>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/delay.h>

#include <linux/nvram.h>
#include <linux/proc_fs.h>
#include <linux/sysfs.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/input.h>
#include <linux/leds.h>
#include <linux/rfkill.h>
#include <asm/uaccess.h>

#include <linux/dmi.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>

#include <acpi/acpi_drivers.h>

#include <linux/pci_ids.h>


/* ThinkPad CMOS commands */
#define TP_CMOS_VOLUME_DOWN	0
#define TP_CMOS_VOLUME_UP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRIGHTNESS_UP	4
#define TP_CMOS_BRIGHTNESS_DOWN	5
#define TP_CMOS_THINKLIGHT_ON	12
#define TP_CMOS_THINKLIGHT_OFF	13

/* NVRAM Addresses */
enum tp_nvram_addr {
	TP_NVRAM_ADDR_HK2		= 0x57,
	TP_NVRAM_ADDR_THINKLIGHT	= 0x58,
	TP_NVRAM_ADDR_VIDEO		= 0x59,
	TP_NVRAM_ADDR_BRIGHTNESS	= 0x5e,
	TP_NVRAM_ADDR_MIXER		= 0x60,
};

/* NVRAM bit masks */
enum {
	TP_NVRAM_MASK_HKT_THINKPAD	= 0x08,
	TP_NVRAM_MASK_HKT_ZOOM		= 0x20,
	TP_NVRAM_MASK_HKT_DISPLAY	= 0x40,
	TP_NVRAM_MASK_HKT_HIBERNATE	= 0x80,
	TP_NVRAM_MASK_THINKLIGHT	= 0x10,
	TP_NVRAM_MASK_HKT_DISPEXPND	= 0x30,
	TP_NVRAM_MASK_HKT_BRIGHTNESS	= 0x20,
	TP_NVRAM_MASK_LEVEL_BRIGHTNESS	= 0x0f,
	TP_NVRAM_POS_LEVEL_BRIGHTNESS	= 0,
	TP_NVRAM_MASK_MUTE		= 0x40,
	TP_NVRAM_MASK_HKT_VOLUME	= 0x80,
	TP_NVRAM_MASK_LEVEL_VOLUME	= 0x0f,
	TP_NVRAM_POS_LEVEL_VOLUME	= 0,
};

/* ACPI HIDs */
#define TPACPI_ACPI_HKEY_HID		"IBM0068"

/* Input IDs */
#define TPACPI_HKEY_INPUT_PRODUCT	0x5054 /* "TP" */
#define TPACPI_HKEY_INPUT_VERSION	0x4101

/* ACPI \WGSV commands */
enum {
	TP_ACPI_WGSV_GET_STATE		= 0x01, /* Get state information */
	TP_ACPI_WGSV_PWR_ON_ON_RESUME	= 0x02, /* Resume WWAN powered on */
	TP_ACPI_WGSV_PWR_OFF_ON_RESUME	= 0x03,	/* Resume WWAN powered off */
	TP_ACPI_WGSV_SAVE_STATE		= 0x04, /* Save state for S4/S5 */
};

/* TP_ACPI_WGSV_GET_STATE bits */
enum {
	TP_ACPI_WGSV_STATE_WWANEXIST	= 0x0001, /* WWAN hw available */
	TP_ACPI_WGSV_STATE_WWANPWR	= 0x0002, /* WWAN radio enabled */
	TP_ACPI_WGSV_STATE_WWANPWRRES	= 0x0004, /* WWAN state at resume */
	TP_ACPI_WGSV_STATE_WWANBIOSOFF	= 0x0008, /* WWAN disabled in BIOS */
	TP_ACPI_WGSV_STATE_BLTHEXIST	= 0x0001, /* BLTH hw available */
	TP_ACPI_WGSV_STATE_BLTHPWR	= 0x0002, /* BLTH radio enabled */
	TP_ACPI_WGSV_STATE_BLTHPWRRES	= 0x0004, /* BLTH state at resume */
	TP_ACPI_WGSV_STATE_BLTHBIOSOFF	= 0x0008, /* BLTH disabled in BIOS */
	TP_ACPI_WGSV_STATE_UWBEXIST	= 0x0010, /* UWB hw available */
	TP_ACPI_WGSV_STATE_UWBPWR	= 0x0020, /* UWB radio enabled */
};

/* HKEY events */
enum tpacpi_hkey_event_t {
	/* Hotkey-related */
	TP_HKEY_EV_HOTKEY_BASE		= 0x1001, /* first hotkey (FN+F1) */
	TP_HKEY_EV_BRGHT_UP		= 0x1010, /* Brightness up */
	TP_HKEY_EV_BRGHT_DOWN		= 0x1011, /* Brightness down */
	TP_HKEY_EV_VOL_UP		= 0x1015, /* Volume up or unmute */
	TP_HKEY_EV_VOL_DOWN		= 0x1016, /* Volume down or unmute */
	TP_HKEY_EV_VOL_MUTE		= 0x1017, /* Mixer output mute */

	/* Reasons for waking up from S3/S4 */
	TP_HKEY_EV_WKUP_S3_UNDOCK	= 0x2304, /* undock requested, S3 */
	TP_HKEY_EV_WKUP_S4_UNDOCK	= 0x2404, /* undock requested, S4 */
	TP_HKEY_EV_WKUP_S3_BAYEJ	= 0x2305, /* bay ejection req, S3 */
	TP_HKEY_EV_WKUP_S4_BAYEJ	= 0x2405, /* bay ejection req, S4 */
	TP_HKEY_EV_WKUP_S3_BATLOW	= 0x2313, /* battery empty, S3 */
	TP_HKEY_EV_WKUP_S4_BATLOW	= 0x2413, /* battery empty, S4 */

	/* Auto-sleep after eject request */
	TP_HKEY_EV_BAYEJ_ACK		= 0x3003, /* bay ejection complete */
	TP_HKEY_EV_UNDOCK_ACK		= 0x4003, /* undock complete */

	/* Misc bay events */
	TP_HKEY_EV_OPTDRV_EJ		= 0x3006, /* opt. drive tray ejected */

	/* User-interface events */
	TP_HKEY_EV_LID_CLOSE		= 0x5001, /* laptop lid closed */
	TP_HKEY_EV_LID_OPEN		= 0x5002, /* laptop lid opened */
	TP_HKEY_EV_TABLET_TABLET	= 0x5009, /* tablet swivel up */
	TP_HKEY_EV_TABLET_NOTEBOOK	= 0x500a, /* tablet swivel down */
	TP_HKEY_EV_PEN_INSERTED		= 0x500b, /* tablet pen inserted */
	TP_HKEY_EV_PEN_REMOVED		= 0x500c, /* tablet pen removed */
	TP_HKEY_EV_BRGHT_CHANGED	= 0x5010, /* backlight control event */

	/* Thermal events */
	TP_HKEY_EV_ALARM_BAT_HOT	= 0x6011, /* battery too hot */
	TP_HKEY_EV_ALARM_BAT_XHOT	= 0x6012, /* battery critically hot */
	TP_HKEY_EV_ALARM_SENSOR_HOT	= 0x6021, /* sensor too hot */
	TP_HKEY_EV_ALARM_SENSOR_XHOT	= 0x6022, /* sensor critically hot */
	TP_HKEY_EV_THM_TABLE_CHANGED	= 0x6030, /* thermal table changed */

	/* Misc */
	TP_HKEY_EV_RFKILL_CHANGED	= 0x7000, /* rfkill switch changed */
};

/****************************************************************************
 * Main driver
 */

#define TPACPI_NAME "thinkpad"
#define TPACPI_DESC "ThinkPad ACPI Extras"
#define TPACPI_FILE TPACPI_NAME "_acpi"
#define TPACPI_URL "http://ibm-acpi.sf.net/"
#define TPACPI_MAIL "ibm-acpi-devel@lists.sourceforge.net"

#define TPACPI_PROC_DIR "ibm"
#define TPACPI_ACPI_EVENT_PREFIX "ibm"
#define TPACPI_DRVR_NAME TPACPI_FILE
#define TPACPI_DRVR_SHORTNAME "tpacpi"
#define TPACPI_HWMON_DRVR_NAME TPACPI_NAME "_hwmon"

#define TPACPI_NVRAM_KTHREAD_NAME "ktpacpi_nvramd"
#define TPACPI_WORKQUEUE_NAME "ktpacpid"

#define TPACPI_MAX_ACPI_ARGS 3

/* printk headers */
#define TPACPI_LOG TPACPI_FILE ": "
#define TPACPI_EMERG	KERN_EMERG	TPACPI_LOG
#define TPACPI_ALERT	KERN_ALERT	TPACPI_LOG
#define TPACPI_CRIT	KERN_CRIT	TPACPI_LOG
#define TPACPI_ERR	KERN_ERR	TPACPI_LOG
#define TPACPI_WARN	KERN_WARNING	TPACPI_LOG
#define TPACPI_NOTICE	KERN_NOTICE	TPACPI_LOG
#define TPACPI_INFO	KERN_INFO	TPACPI_LOG
#define TPACPI_DEBUG	KERN_DEBUG	TPACPI_LOG

/* Debugging printk groups */
#define TPACPI_DBG_ALL		0xffff
#define TPACPI_DBG_DISCLOSETASK	0x8000
#define TPACPI_DBG_INIT		0x0001
#define TPACPI_DBG_EXIT		0x0002
#define TPACPI_DBG_RFKILL	0x0004
#define TPACPI_DBG_HKEY		0x0008
#define TPACPI_DBG_FAN		0x0010
#define TPACPI_DBG_BRGHT	0x0020

#define onoff(status, bit) ((status) & (1 << (bit)) ? "on" : "off")
#define enabled(status, bit) ((status) & (1 << (bit)) ? "enabled" : "disabled")
#define strlencmp(a, b) (strncmp((a), (b), strlen(b)))


/****************************************************************************
 * Driver-wide structs and misc. variables
 */

struct ibm_struct;

struct tp_acpi_drv_struct {
	const struct acpi_device_id *hid;
	struct acpi_driver *driver;

	void (*notify) (struct ibm_struct *, u32);
	acpi_handle *handle;
	u32 type;
	struct acpi_device *device;
};

struct ibm_struct {
	char *name;

	int (*read) (char *);
	int (*write) (char *);
	void (*exit) (void);
	void (*resume) (void);
	void (*suspend) (pm_message_t state);
	void (*shutdown) (void);

	struct list_head all_drivers;

	struct tp_acpi_drv_struct *acpi;

	struct {
		u8 acpi_driver_registered:1;
		u8 acpi_notify_installed:1;
		u8 proc_created:1;
		u8 init_called:1;
		u8 experimental:1;
	} flags;
};

struct ibm_init_struct {
	char param[32];

	int (*init) (struct ibm_init_struct *);
	struct ibm_struct *data;
};

static struct {
	u32 bluetooth:1;
	u32 hotkey:1;
	u32 hotkey_mask:1;
	u32 hotkey_wlsw:1;
	u32 hotkey_tablet:1;
	u32 light:1;
	u32 light_status:1;
	u32 bright_16levels:1;
	u32 bright_acpimode:1;
	u32 wan:1;
	u32 uwb:1;
	u32 fan_ctrl_status_undef:1;
	u32 second_fan:1;
	u32 beep_needs_two_args:1;
	u32 input_device_registered:1;
	u32 platform_drv_registered:1;
	u32 platform_drv_attrs_registered:1;
	u32 sensors_pdrv_registered:1;
	u32 sensors_pdrv_attrs_registered:1;
	u32 sensors_pdev_attrs_registered:1;
	u32 hotkey_poll_active:1;
} tp_features;

static struct {
	u16 hotkey_mask_ff:1;
} tp_warned;

struct thinkpad_id_data {
	unsigned int vendor;	/* ThinkPad vendor:
				 * PCI_VENDOR_ID_IBM/PCI_VENDOR_ID_LENOVO */

	char *bios_version_str;	/* Something like 1ZET51WW (1.03z) */
	char *ec_version_str;	/* Something like 1ZHT51WW-1.04a */

	u16 bios_model;		/* 1Y = 0x5931, 0 = unknown */
	u16 ec_model;
	u16 bios_release;	/* 1ZETK1WW = 0x314b, 0 = unknown */
	u16 ec_release;

	char *model_str;	/* ThinkPad T43 */
	char *nummodel_str;	/* 9384A9C for a 9384-A9C model */
};
static struct thinkpad_id_data thinkpad_id;

static enum {
	TPACPI_LIFE_INIT = 0,
	TPACPI_LIFE_RUNNING,
	TPACPI_LIFE_EXITING,
} tpacpi_lifecycle;

static int experimental;
static u32 dbg_level;

static struct workqueue_struct *tpacpi_wq;

enum led_status_t {
	TPACPI_LED_OFF = 0,
	TPACPI_LED_ON,
	TPACPI_LED_BLINK,
};

/* Special LED class that can defer work */
struct tpacpi_led_classdev {
	struct led_classdev led_classdev;
	struct work_struct work;
	enum led_status_t new_state;
	unsigned int led;
};

#ifdef CONFIG_THINKPAD_ACPI_DEBUGFACILITIES
static int dbg_wlswemul;
static int tpacpi_wlsw_emulstate;
static int dbg_bluetoothemul;
static int tpacpi_bluetooth_emulstate;
static int dbg_wwanemul;
static int tpacpi_wwan_emulstate;
static int dbg_uwbemul;
static int tpacpi_uwb_emulstate;
#endif


/*************************************************************************
 *  Debugging helpers
 */

#define dbg_printk(a_dbg_level, format, arg...) \
	do { if (dbg_level & (a_dbg_level)) \
		printk(TPACPI_DEBUG "%s: " format, __func__ , ## arg); \
	} while (0)

#ifdef CONFIG_THINKPAD_ACPI_DEBUG
#define vdbg_printk dbg_printk
static const char *str_supported(int is_supported);
#else
#define vdbg_printk(a_dbg_level, format, arg...) \
	do { } while (0)
#endif

static void tpacpi_log_usertask(const char * const what)
{
	printk(TPACPI_DEBUG "%s: access by process with PID %d\n",
		what, task_tgid_vnr(current));
}

#define tpacpi_disclose_usertask(what, format, arg...) \
	do { \
		if (unlikely( \
		    (dbg_level & TPACPI_DBG_DISCLOSETASK) && \
		    (tpacpi_lifecycle == TPACPI_LIFE_RUNNING))) { \
			printk(TPACPI_DEBUG "%s: PID %d: " format, \
				what, task_tgid_vnr(current), ## arg); \
		} \
	} while (0)

/*
 * Quirk handling helpers
 *
 * ThinkPad IDs and versions seen in the field so far
 * are two-characters from the set [0-9A-Z], i.e. base 36.
 *
 * We use values well outside that range as specials.
 */

#define TPACPI_MATCH_ANY		0xffffU
#define TPACPI_MATCH_UNKNOWN		0U

/* TPID('1', 'Y') == 0x5931 */
#define TPID(__c1, __c2) (((__c2) << 8) | (__c1))

#define TPACPI_Q_IBM(__id1, __id2, __quirk)	\
	{ .vendor = PCI_VENDOR_ID_IBM,		\
	  .bios = TPID(__id1, __id2),		\
	  .ec = TPACPI_MATCH_ANY,		\
	  .quirks = (__quirk) }

#define TPACPI_Q_LNV(__id1, __id2, __quirk)	\
	{ .vendor = PCI_VENDOR_ID_LENOVO,	\
	  .bios = TPID(__id1, __id2),		\
	  .ec = TPACPI_MATCH_ANY,		\
	  .quirks = (__quirk) }

struct tpacpi_quirk {
	unsigned int vendor;
	u16 bios;
	u16 ec;
	unsigned long quirks;
};

/**
 * tpacpi_check_quirks() - search BIOS/EC version on a list
 * @qlist:		array of &struct tpacpi_quirk
 * @qlist_size:		number of elements in @qlist
 *
 * Iterates over a quirks list until one is found that matches the
 * ThinkPad's vendor, BIOS and EC model.
 *
 * Returns 0 if nothing matches, otherwise returns the quirks field of
 * the matching &struct tpacpi_quirk entry.
 *
 * The match criteria is: vendor, ec and bios much match.
 */
static unsigned long __init tpacpi_check_quirks(
			const struct tpacpi_quirk *qlist,
			unsigned int qlist_size)
{
	while (qlist_size) {
		if ((qlist->vendor == thinkpad_id.vendor ||
				qlist->vendor == TPACPI_MATCH_ANY) &&
		    (qlist->bios == thinkpad_id.bios_model ||
				qlist->bios == TPACPI_MATCH_ANY) &&
		    (qlist->ec == thinkpad_id.ec_model ||
				qlist->ec == TPACPI_MATCH_ANY))
			return qlist->quirks;

		qlist_size--;
		qlist++;
	}
	return 0;
}


/****************************************************************************
 ****************************************************************************
 *
 * ACPI Helpers and device model
 *
 ****************************************************************************
 ****************************************************************************/

/*************************************************************************
 * ACPI basic handles
 */

static acpi_handle root_handle;

#define TPACPI_HANDLE(object, parent, paths...)			\
	static acpi_handle  object##_handle;			\
	static acpi_handle *object##_parent = &parent##_handle;	\
	static char        *object##_path;			\
	static char        *object##_paths[] = { paths }

TPACPI_HANDLE(ec, root, "\\_SB.PCI0.ISA.EC0",	/* 240, 240x */
	   "\\_SB.PCI.ISA.EC",	/* 570 */
	   "\\_SB.PCI0.ISA0.EC0",	/* 600e/x, 770e, 770x */
	   "\\_SB.PCI0.ISA.EC",	/* A21e, A2xm/p, T20-22, X20-21 */
	   "\\_SB.PCI0.AD4S.EC0",	/* i1400, R30 */
	   "\\_SB.PCI0.ICH3.EC0",	/* R31 */
	   "\\_SB.PCI0.LPC.EC",	/* all others */
	   );

TPACPI_HANDLE(ecrd, ec, "ECRD");	/* 570 */
TPACPI_HANDLE(ecwr, ec, "ECWR");	/* 570 */

TPACPI_HANDLE(cmos, root, "\\UCMS",	/* R50, R50e, R50p, R51, */
					/* T4x, X31, X40 */
	   "\\CMOS",		/* A3x, G4x, R32, T23, T30, X22-24, X30 */
	   "\\CMS",		/* R40, R40e */
	   );			/* all others */

TPACPI_HANDLE(hkey, ec, "\\_SB.HKEY",	/* 600e/x, 770e, 770x */
	   "^HKEY",		/* R30, R31 */
	   "HKEY",		/* all others */
	   );			/* 570 */

TPACPI_HANDLE(vid, root, "\\_SB.PCI.AGP.VGA",	/* 570 */
	   "\\_SB.PCI0.AGP0.VID0",	/* 600e/x, 770x */
	   "\\_SB.PCI0.VID0",	/* 770e */
	   "\\_SB.PCI0.VID",	/* A21e, G4x, R50e, X30, X40 */
	   "\\_SB.PCI0.AGP.VID",	/* all others */
	   );				/* R30, R31 */


/*************************************************************************
 * ACPI helpers
 */

static int acpi_evalf(acpi_handle handle,
		      void *res, char *method, char *fmt, ...)
{
	char *fmt0 = fmt;
	struct acpi_object_list params;
	union acpi_object in_objs[TPACPI_MAX_ACPI_ARGS];
	struct acpi_buffer result, *resultp;
	union acpi_object out_obj;
	acpi_status status;
	va_list ap;
	char res_type;
	int success;
	int quiet;

	if (!*fmt) {
		printk(TPACPI_ERR "acpi_evalf() called with empty format\n");
		return 0;
	}

	if (*fmt == 'q') {
		quiet = 1;
		fmt++;
	} else
		quiet = 0;

	res_type = *(fmt++);

	params.count = 0;
	params.pointer = &in_objs[0];

	va_start(ap, fmt);
	while (*fmt) {
		char c = *(fmt++);
		switch (c) {
		case 'd':	/* int */
			in_objs[params.count].integer.value = va_arg(ap, int);
			in_objs[params.count++].type = ACPI_TYPE_INTEGER;
			break;
			/* add more types as needed */
		default:
			printk(TPACPI_ERR "acpi_evalf() called "
			       "with invalid format character '%c'\n", c);
			return 0;
		}
	}
	va_end(ap);

	if (res_type != 'v') {
		result.length = sizeof(out_obj);
		result.pointer = &out_obj;
		resultp = &result;
	} else
		resultp = NULL;

	status = acpi_evaluate_object(handle, method, &params, resultp);

	switch (res_type) {
	case 'd':		/* int */
		if (res)
			*(int *)res = out_obj.integer.value;
		success = status == AE_OK && out_obj.type == ACPI_TYPE_INTEGER;
		break;
	case 'v':		/* void */
		success = status == AE_OK;
		break;
		/* add more types as needed */
	default:
		printk(TPACPI_ERR "acpi_evalf() called "
		       "with invalid format character '%c'\n", res_type);
		return 0;
	}

	if (!success && !quiet)
		printk(TPACPI_ERR "acpi_evalf(%s, %s, ...) failed: %d\n",
		       method, fmt0, status);

	return success;
}

static int acpi_ec_read(int i, u8 *p)
{
	int v;

	if (ecrd_handle) {
		if (!acpi_evalf(ecrd_handle, &v, NULL, "dd", i))
			return 0;
		*p = v;
	} else {
		if (ec_read(i, p) < 0)
			return 0;
	}

	return 1;
}

static int acpi_ec_write(int i, u8 v)
{
	if (ecwr_handle) {
		if (!acpi_evalf(ecwr_handle, NULL, NULL, "vdd", i, v))
			return 0;
	} else {
		if (ec_write(i, v) < 0)
			return 0;
	}

	return 1;
}

static int issue_thinkpad_cmos_command(int cmos_cmd)
{
	if (!cmos_handle)
		return -ENXIO;

	if (!acpi_evalf(cmos_handle, NULL, NULL, "vd", cmos_cmd))
		return -EIO;

	return 0;
}

/*************************************************************************
 * ACPI device model
 */

#define TPACPI_ACPIHANDLE_INIT(object) \
	drv_acpi_handle_init(#object, &object##_handle, *object##_parent, \
		object##_paths, ARRAY_SIZE(object##_paths), &object##_path)

static void drv_acpi_handle_init(char *name,
			   acpi_handle *handle, acpi_handle parent,
			   char **paths, int num_paths, char **path)
{
	int i;
	acpi_status status;

	vdbg_printk(TPACPI_DBG_INIT, "trying to locate ACPI handle for %s\n",
		name);

	for (i = 0; i < num_paths; i++) {
		status = acpi_get_handle(parent, paths[i], handle);
		if (ACPI_SUCCESS(status)) {
			*path = paths[i];
			dbg_printk(TPACPI_DBG_INIT,
				   "Found ACPI handle %s for %s\n",
				   *path, name);
			return;
		}
	}

	vdbg_printk(TPACPI_DBG_INIT, "ACPI handle for %s not found\n",
		    name);
	*handle = NULL;
}

static void dispatch_acpi_notify(acpi_handle handle, u32 event, void *data)
{
	struct ibm_struct *ibm = data;

	if (tpacpi_lifecycle != TPACPI_LIFE_RUNNING)
		return;

	if (!ibm || !ibm->acpi || !ibm->acpi->notify)
		return;

	ibm->acpi->notify(ibm, event);
}

static int __init setup_acpi_notify(struct ibm_struct *ibm)
{
	acpi_status status;
	int rc;

	BUG_ON(!ibm->acpi);

	if (!*ibm->acpi->handle)
		return 0;

	vdbg_printk(TPACPI_DBG_INIT,
		"setting up ACPI notify for %s\n", ibm->name);

	rc = acpi_bus_get_device(*ibm->acpi->handle, &ibm->acpi->device);
	if (rc < 0) {
		printk(TPACPI_ERR "acpi_bus_get_device(%s) failed: %d\n",
			ibm->name, rc);
		return -ENODEV;
	}

	ibm->acpi->device->driver_data = ibm;
	sprintf(acpi_device_class(ibm->acpi->device), "%s/%s",
		TPACPI_ACPI_EVENT_PREFIX,
		ibm->name);

	status = acpi_install_notify_handler(*ibm->acpi->handle,
			ibm->acpi->type, dispatch_acpi_notify, ibm);
	if (ACPI_FAILURE(status)) {
		if (status == AE_ALREADY_EXISTS) {
			printk(TPACPI_NOTICE
			       "another device driver is already "
			       "handling %s events\n", ibm->name);
		} else {
			printk(TPACPI_ERR
			       "acpi_install_notify_handler(%s) failed: %d\n",
			       ibm->name, status);
		}
		return -ENODEV;
	}
	ibm->flags.acpi_notify_installed = 1;
	return 0;
}

static int __init tpacpi_device_add(struct acpi_device *device)
{
	return 0;
}

static int __init register_tpacpi_subdriver(struct ibm_struct *ibm)
{
	int rc;

	dbg_printk(TPACPI_DBG_INIT,
		"registering %s as an ACPI driver\n", ibm->name);

	BUG_ON(!ibm->acpi);

	ibm->acpi->driver = kzalloc(sizeof(struct acpi_driver), GFP_KERNEL);
	if (!ibm->acpi->driver) {
		printk(TPACPI_ERR
		       "failed to allocate memory for ibm->acpi->driver\n");
		return -ENOMEM;
	}

	sprintf(ibm->acpi->driver->name, "%s_%s", TPACPI_NAME, ibm->name);
	ibm->acpi->driver->ids = ibm->acpi->hid;

	ibm->acpi->driver->ops.add = &tpacpi_device_add;

	rc = acpi_bus_register_driver(ibm->acpi->driver);
	if (rc < 0) {
		printk(TPACPI_ERR "acpi_bus_register_driver(%s) failed: %d\n",
		       ibm->name, rc);
		kfree(ibm->acpi->driver);
		ibm->acpi->driver = NULL;
	} else if (!rc)
		ibm->flags.acpi_driver_registered = 1;

	return rc;
}


/****************************************************************************
 ****************************************************************************
 *
 * Procfs Helpers
 *
 ****************************************************************************
 ****************************************************************************/

static int dispatch_procfs_read(char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	struct ibm_struct *ibm = data;
	int len;

	if (!ibm || !ibm->read)
		return -EINVAL;

	len = ibm->read(page);
	if (len < 0)
		return len;

	if (len <= off + count)
		*eof = 1;
	*start = page + off;
	len -= off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;

	return len;
}

static int dispatch_procfs_write(struct file *file,
			const char __user *userbuf,
			unsigned long count, void *data)
{
	struct ibm_struct *ibm = data;
	char *kernbuf;
	int ret;

	if (!ibm || !ibm->write)
		return -EINVAL;
	if (count > PAGE_SIZE - 2)
		return -EINVAL;

	kernbuf = kmalloc(count + 2, GFP_KERNEL);
	if (!kernbuf)
		return -ENOMEM;

	if (copy_from_user(kernbuf, userbuf, count)) {
		kfree(kernbuf);
		return -EFAULT;
	}

	kernbuf[count] = 0;
	strcat(kernbuf, ",");
	ret = ibm->write(kernbuf);
	if (ret == 0)
		ret = count;

	kfree(kernbuf);

	return ret;
}

static char *next_cmd(char **cmds)
{
	char *start = *cmds;
	char *end;

	while ((end = strchr(start, ',')) && end == start)
		start = end + 1;

	if (!end)
		return NULL;

	*end = 0;
	*cmds = end + 1;
	return start;
}


/****************************************************************************
 ****************************************************************************
 *
 * Device model: input, hwmon and platform
 *
 ****************************************************************************
 ****************************************************************************/

static struct platform_device *tpacpi_pdev;
static struct platform_device *tpacpi_sensors_pdev;
static struct device *tpacpi_hwmon;
static struct input_dev *tpacpi_inputdev;
static struct mutex tpacpi_inputdev_send_mutex;
static LIST_HEAD(tpacpi_all_drivers);

static int tpacpi_suspend_handler(struct platform_device *pdev,
				  pm_message_t state)
{
	struct ibm_struct *ibm, *itmp;

	list_for_each_entry_safe(ibm, itmp,
				 &tpacpi_all_drivers,
				 all_drivers) {
		if (ibm->suspend)
			(ibm->suspend)(state);
	}

	return 0;
}

static int tpacpi_resume_handler(struct platform_device *pdev)
{
	struct ibm_struct *ibm, *itmp;

	list_for_each_entry_safe(ibm, itmp,
				 &tpacpi_all_drivers,
				 all_drivers) {
		if (ibm->resume)
			(ibm->resume)();
	}

	return 0;
}

static void tpacpi_shutdown_handler(struct platform_device *pdev)
{
	struct ibm_struct *ibm, *itmp;

	list_for_each_entry_safe(ibm, itmp,
				 &tpacpi_all_drivers,
				 all_drivers) {
		if (ibm->shutdown)
			(ibm->shutdown)();
	}
}

static struct platform_driver tpacpi_pdriver = {
	.driver = {
		.name = TPACPI_DRVR_NAME,
		.owner = THIS_MODULE,
	},
	.suspend = tpacpi_suspend_handler,
	.resume = tpacpi_resume_handler,
	.shutdown = tpacpi_shutdown_handler,
};

static struct platform_driver tpacpi_hwmon_pdriver = {
	.driver = {
		.name = TPACPI_HWMON_DRVR_NAME,
		.owner = THIS_MODULE,
	},
};

/*************************************************************************
 * sysfs support helpers
 */

struct attribute_set {
	unsigned int members, max_members;
	struct attribute_group group;
};

struct attribute_set_obj {
	struct attribute_set s;
	struct attribute *a;
} __attribute__((packed));

static struct attribute_set *create_attr_set(unsigned int max_members,
						const char *name)
{
	struct attribute_set_obj *sobj;

	if (max_members == 0)
		return NULL;

	/* Allocates space for implicit NULL at the end too */
	sobj = kzalloc(sizeof(struct attribute_set_obj) +
		    max_members * sizeof(struct attribute *),
		    GFP_KERNEL);
	if (!sobj)
		return NULL;
	sobj->s.max_members = max_members;
	sobj->s.group.attrs = &sobj->a;
	sobj->s.group.name = name;

	return &sobj->s;
}

#define destroy_attr_set(_set) \
	kfree(_set);

/* not multi-threaded safe, use it in a single thread per set */
static int add_to_attr_set(struct attribute_set *s, struct attribute *attr)
{
	if (!s || !attr)
		return -EINVAL;

	if (s->members >= s->max_members)
		return -ENOMEM;

	s->group.attrs[s->members] = attr;
	s->members++;

	return 0;
}

static int add_many_to_attr_set(struct attribute_set *s,
			struct attribute **attr,
			unsigned int count)
{
	int i, res;

	for (i = 0; i < count; i++) {
		res = add_to_attr_set(s, attr[i]);
		if (res)
			return res;
	}

	return 0;
}

static void delete_attr_set(struct attribute_set *s, struct kobject *kobj)
{
	sysfs_remove_group(kobj, &s->group);
	destroy_attr_set(s);
}

#define register_attr_set_with_sysfs(_attr_set, _kobj) \
	sysfs_create_group(_kobj, &_attr_set->group)

static int parse_strtoul(const char *buf,
		unsigned long max, unsigned long *value)
{
	char *endp;

	while (*buf && isspace(*buf))
		buf++;
	*value = simple_strtoul(buf, &endp, 0);
	while (*endp && isspace(*endp))
		endp++;
	if (*endp || *value > max)
		return -EINVAL;

	return 0;
}

static void tpacpi_disable_brightness_delay(void)
{
	if (acpi_evalf(hkey_handle, NULL, "PWMS", "qvd", 0))
		printk(TPACPI_NOTICE
			"ACPI backlight control delay disabled\n");
}

static int __init tpacpi_query_bcl_levels(acpi_handle handle)
{
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	int rc;

	if (ACPI_SUCCESS(acpi_evaluate_object(handle, NULL, NULL, &buffer))) {
		obj = (union acpi_object *)buffer.pointer;
		if (!obj || (obj->type != ACPI_TYPE_PACKAGE)) {
			printk(TPACPI_ERR "Unknown _BCL data, "
			       "please report this to %s\n", TPACPI_MAIL);
			rc = 0;
		} else {
			rc = obj->package.count;
		}
	} else {
		return 0;
	}

	kfree(buffer.pointer);
	return rc;
}

static acpi_status __init tpacpi_acpi_walk_find_bcl(acpi_handle handle,
					u32 lvl, void *context, void **rv)
{
	char name[ACPI_PATH_SEGMENT_LENGTH];
	struct acpi_buffer buffer = { sizeof(name), &name };

	if (ACPI_SUCCESS(acpi_get_name(handle, ACPI_SINGLE_NAME, &buffer)) &&
	    !strncmp("_BCL", name, sizeof(name) - 1)) {
		BUG_ON(!rv || !*rv);
		**(int **)rv = tpacpi_query_bcl_levels(handle);
		return AE_CTRL_TERMINATE;
	} else {
		return AE_OK;
	}
}

/*
 * Returns 0 (no ACPI _BCL or _BCL invalid), or size of brightness map
 */
static int __init tpacpi_check_std_acpi_brightness_support(void)
{
	int status;
	int bcl_levels = 0;
	void *bcl_ptr = &bcl_levels;

	if (!vid_handle) {
		TPACPI_ACPIHANDLE_INIT(vid);
	}
	if (!vid_handle)
		return 0;

	/*
	 * Search for a _BCL method, and execute it.  This is safe on all
	 * ThinkPads, and as a side-effect, _BCL will place a Lenovo Vista
	 * BIOS in ACPI backlight control mode.  We do NOT have to care
	 * about calling the _BCL method in an enabled video device, any
	 * will do for our purposes.
	 */

	status = acpi_walk_namespace(ACPI_TYPE_METHOD, vid_handle, 3,
				     tpacpi_acpi_walk_find_bcl, NULL,
				     &bcl_ptr);

	if (ACPI_SUCCESS(status) && bcl_levels > 2) {
		tp_features.bright_acpimode = 1;
		return (bcl_levels - 2);
	}

	return 0;
}

static void printk_deprecated_attribute(const char * const what,
					const char * const details)
{
	tpacpi_log_usertask("deprecated sysfs attribute");
	printk(TPACPI_WARN "WARNING: sysfs attribute %s is deprecated and "
		"will be removed. %s\n",
		what, details);
}

/*************************************************************************
 * rfkill and radio control support helpers
 */

/*
 * ThinkPad-ACPI firmware handling model:
 *
 * WLSW (master wireless switch) is event-driven, and is common to all
 * firmware-controlled radios.  It cannot be controlled, just monitored,
 * as expected.  It overrides all radio state in firmware
 *
 * The kernel, a masked-off hotkey, and WLSW can change the radio state
 * (TODO: verify how WLSW interacts with the returned radio state).
 *
 * The only time there are shadow radio state changes, is when
 * masked-off hotkeys are used.
 */

/*
 * Internal driver API for radio state:
 *
 * int: < 0 = error, otherwise enum tpacpi_rfkill_state
 * bool: true means radio blocked (off)
 */
enum tpacpi_rfkill_state {
	TPACPI_RFK_RADIO_OFF = 0,
	TPACPI_RFK_RADIO_ON
};

/* rfkill switches */
enum tpacpi_rfk_id {
	TPACPI_RFK_BLUETOOTH_SW_ID = 0,
	TPACPI_RFK_WWAN_SW_ID,
	TPACPI_RFK_UWB_SW_ID,
	TPACPI_RFK_SW_MAX
};

static const char *tpacpi_rfkill_names[] = {
	[TPACPI_RFK_BLUETOOTH_SW_ID] = "bluetooth",
	[TPACPI_RFK_WWAN_SW_ID] = "wwan",
	[TPACPI_RFK_UWB_SW_ID] = "uwb",
	[TPACPI_RFK_SW_MAX] = NULL
};

/* ThinkPad-ACPI rfkill subdriver */
struct tpacpi_rfk {
	struct rfkill *rfkill;
	enum tpacpi_rfk_id id;
	const struct tpacpi_rfk_ops *ops;
};

struct tpacpi_rfk_ops {
	/* firmware interface */
	int (*get_status)(void);
	int (*set_status)(const enum tpacpi_rfkill_state);
};

static struct tpacpi_rfk *tpacpi_rfkill_switches[TPACPI_RFK_SW_MAX];

/* Query FW and update rfkill sw state for a given rfkill switch */
static int tpacpi_rfk_update_swstate(const struct tpacpi_rfk *tp_rfk)
{
	int status;

	if (!tp_rfk)
		return -ENODEV;

	status = (tp_rfk->ops->get_status)();
	if (status < 0)
		return status;

	rfkill_set_sw_state(tp_rfk->rfkill,
			    (status == TPACPI_RFK_RADIO_OFF));

	return status;
}

/* Query FW and update rfkill sw state for all rfkill switches */
static void tpacpi_rfk_update_swstate_all(void)
{
	unsigned int i;

	for (i = 0; i < TPACPI_RFK_SW_MAX; i++)
		tpacpi_rfk_update_swstate(tpacpi_rfkill_switches[i]);
}

/*
 * Sync the HW-blocking state of all rfkill switches,
 * do notice it causes the rfkill core to schedule uevents
 */
static void tpacpi_rfk_update_hwblock_state(bool blocked)
{
	unsigned int i;
	struct tpacpi_rfk *tp_rfk;

	for (i = 0; i < TPACPI_RFK_SW_MAX; i++) {
		tp_rfk = tpacpi_rfkill_switches[i];
		if (tp_rfk) {
			if (rfkill_set_hw_state(tp_rfk->rfkill,
						blocked)) {
				/* ignore -- we track sw block */
			}
		}
	}
}

/* Call to get the WLSW state from the firmware */
static int hotkey_get_wlsw(void);

/* Call to query WLSW state and update all rfkill switches */
static bool tpacpi_rfk_check_hwblock_state(void)
{
	int res = hotkey_get_wlsw();
	int hw_blocked;

	/* When unknown or unsupported, we have to assume it is unblocked */
	if (res < 0)
		return false;

	hw_blocked = (res == TPACPI_RFK_RADIO_OFF);
	tpacpi_rfk_update_hwblock_state(hw_blocked);

	return hw_blocked;
}

static int tpacpi_rfk_hook_set_block(void *data, bool blocked)
{
	struct tpacpi_rfk *tp_rfk = data;
	int res;

	dbg_printk(TPACPI_DBG_RFKILL,
		   "request to change radio state to %s\n",
		   blocked ? "blocked" : "unblocked");

	/* try to set radio state */
	res = (tp_rfk->ops->set_status)(blocked ?
				TPACPI_RFK_RADIO_OFF : TPACPI_RFK_RADIO_ON);

	/* and update the rfkill core with whatever the FW really did */
	tpacpi_rfk_update_swstate(tp_rfk);

	return (res < 0) ? res : 0;
}

static const struct rfkill_ops tpacpi_rfk_rfkill_ops = {
	.set_block = tpacpi_rfk_hook_set_block,
};

static int __init tpacpi_new_rfkill(const enum tpacpi_rfk_id id,
			const struct tpacpi_rfk_ops *tp_rfkops,
			const enum rfkill_type rfktype,
			const char *name,
			const bool set_default)
{
	struct tpacpi_rfk *atp_rfk;
	int res;
	bool sw_state = false;
	int sw_status;

	BUG_ON(id >= TPACPI_RFK_SW_MAX || tpacpi_rfkill_switches[id]);

	atp_rfk = kzalloc(sizeof(struct tpacpi_rfk), GFP_KERNEL);
	if (atp_rfk)
		atp_rfk->rfkill = rfkill_alloc(name,
						&tpacpi_pdev->dev,
						rfktype,
						&tpacpi_rfk_rfkill_ops,
						atp_rfk);
	if (!atp_rfk || !atp_rfk->rfkill) {
		printk(TPACPI_ERR
			"failed to allocate memory for rfkill class\n");
		kfree(atp_rfk);
		return -ENOMEM;
	}

	atp_rfk->id = id;
	atp_rfk->ops = tp_rfkops;

	sw_status = (tp_rfkops->get_status)();
	if (sw_status < 0) {
		printk(TPACPI_ERR
			"failed to read initial state for %s, error %d\n",
			name, sw_status);
	} else {
		sw_state = (sw_status == TPACPI_RFK_RADIO_OFF);
		if (set_default) {
			/* try to keep the initial state, since we ask the
			 * firmware to preserve it across S5 in NVRAM */
			rfkill_init_sw_state(atp_rfk->rfkill, sw_state);
		}
	}
	rfkill_set_hw_state(atp_rfk->rfkill, tpacpi_rfk_check_hwblock_state());

	res = rfkill_register(atp_rfk->rfkill);
	if (res < 0) {
		printk(TPACPI_ERR
			"failed to register %s rfkill switch: %d\n",
			name, res);
		rfkill_destroy(atp_rfk->rfkill);
		kfree(atp_rfk);
		return res;
	}

	tpacpi_rfkill_switches[id] = atp_rfk;
	return 0;
}

static void tpacpi_destroy_rfkill(const enum tpacpi_rfk_id id)
{
	struct tpacpi_rfk *tp_rfk;

	BUG_ON(id >= TPACPI_RFK_SW_MAX);

	tp_rfk = tpacpi_rfkill_switches[id];
	if (tp_rfk) {
		rfkill_unregister(tp_rfk->rfkill);
		rfkill_destroy(tp_rfk->rfkill);
		tpacpi_rfkill_switches[id] = NULL;
		kfree(tp_rfk);
	}
}

static void printk_deprecated_rfkill_attribute(const char * const what)
{
	printk_deprecated_attribute(what,
			"Please switch to generic rfkill before year 2010");
}

/* sysfs <radio> enable ------------------------------------------------ */
static ssize_t tpacpi_rfk_sysfs_enable_show(const enum tpacpi_rfk_id id,
					    struct device_attribute *attr,
					    char *buf)
{
	int status;

	printk_deprecated_rfkill_attribute(attr->attr.name);

	/* This is in the ABI... */
	if (tpacpi_rfk_check_hwblock_state()) {
		status = TPACPI_RFK_RADIO_OFF;
	} else {
		status = tpacpi_rfk_update_swstate(tpacpi_rfkill_switches[id]);
		if (status < 0)
			return status;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n",
			(status == TPACPI_RFK_RADIO_ON) ? 1 : 0);
}

static ssize_t tpacpi_rfk_sysfs_enable_store(const enum tpacpi_rfk_id id,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned long t;
	int res;

	printk_deprecated_rfkill_attribute(attr->attr.name);

	if (parse_strtoul(buf, 1, &t))
		return -EINVAL;

	tpacpi_disclose_usertask(attr->attr.name, "set to %ld\n", t);

	/* This is in the ABI... */
	if (tpacpi_rfk_check_hwblock_state() && !!t)
		return -EPERM;

	res = tpacpi_rfkill_switches[id]->ops->set_status((!!t) ?
				TPACPI_RFK_RADIO_ON : TPACPI_RFK_RADIO_OFF);
	tpacpi_rfk_update_swstate(tpacpi_rfkill_switches[id]);

	return (res < 0) ? res : count;
}

/* procfs -------------------------------------------------------------- */
static int tpacpi_rfk_procfs_read(const enum tpacpi_rfk_id id, char *p)
{
	int len = 0;

	if (id >= TPACPI_RFK_SW_MAX)
		len += sprintf(p + len, "status:\t\tnot supported\n");
	else {
		int status;

		/* This is in the ABI... */
		if (tpacpi_rfk_check_hwblock_state()) {
			status = TPACPI_RFK_RADIO_OFF;
		} else {
			status = tpacpi_rfk_update_swstate(
						tpacpi_rfkill_switches[id]);
			if (status < 0)
				return status;
		}

		len += sprintf(p + len, "status:\t\t%s\n",
				(status == TPACPI_RFK_RADIO_ON) ?
					"enabled" : "disabled");
		len += sprintf(p + len, "commands:\tenable, disable\n");
	}

	return len;
}

static int tpacpi_rfk_procfs_write(const enum tpacpi_rfk_id id, char *buf)
{
	char *cmd;
	int status = -1;
	int res = 0;

	if (id >= TPACPI_RFK_SW_MAX)
		return -ENODEV;

	while ((cmd = next_cmd(&buf))) {
		if (strlencmp(cmd, "enable") == 0)
			status = TPACPI_RFK_RADIO_ON;
		else if (strlencmp(cmd, "disable") == 0)
			status = TPACPI_RFK_RADIO_OFF;
		else
			return -EINVAL;
	}

	if (status != -1) {
		tpacpi_disclose_usertask("procfs", "attempt to %s %s\n",
				(status == TPACPI_RFK_RADIO_ON) ?
						"enable" : "disable",
				tpacpi_rfkill_names[id]);
		res = (tpacpi_rfkill_switches[id]->ops->set_status)(status);
		tpacpi_rfk_update_swstate(tpacpi_rfkill_switches[id]);
	}

	return res;
}

/*************************************************************************
 * thinkpad-acpi driver attributes
 */

/* interface_version --------------------------------------------------- */
static ssize_t tpacpi_driver_interface_version_show(
				struct device_driver *drv,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08x\n", TPACPI_SYSFS_VERSION);
}

static DRIVER_ATTR(interface_version, S_IRUGO,
		tpacpi_driver_interface_version_show, NULL);

/* debug_level --------------------------------------------------------- */
static ssize_t tpacpi_driver_debug_show(struct device_driver *drv,
						char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%04x\n", dbg_level);
}

static ssize_t tpacpi_driver_debug_store(struct device_driver *drv,
						const char *buf, size_t count)
{
	unsigned long t;

	if (parse_strtoul(buf, 0xffff, &t))
		return -EINVAL;

	dbg_level = t;

	return count;
}

static DRIVER_ATTR(debug_level, S_IWUSR | S_IRUGO,
		tpacpi_driver_debug_show, tpacpi_driver_debug_store);

/* version ------------------------------------------------------------- */
static ssize_t tpacpi_driver_version_show(struct device_driver *drv,
						char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s v%s\n",
			TPACPI_DESC, TPACPI_VERSION);
}

static DRIVER_ATTR(version, S_IRUGO,
		tpacpi_driver_version_show, NULL);

/* --------------------------------------------------------------------- */

#ifdef CONFIG_THINKPAD_ACPI_DEBUGFACILITIES

/* wlsw_emulstate ------------------------------------------------------ */
static ssize_t tpacpi_driver_wlsw_emulstate_show(struct device_driver *drv,
						char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", !!tpacpi_wlsw_emulstate);
}

static ssize_t tpacpi_driver_wlsw_emulstate_store(struct device_driver *drv,
						const char *buf, size_t count)
{
	unsigned long t;

	if (parse_strtoul(buf, 1, &t))
		return -EINVAL;

	if (tpacpi_wlsw_emulstate != !!t) {
		tpacpi_wlsw_emulstate = !!t;
		tpacpi_rfk_update_hwblock_state(!t);	/* negative logic */
	}

	return count;
}

static DRIVER_ATTR(wlsw_emulstate, S_IWUSR | S_IRUGO,
		tpacpi_driver_wlsw_emulstate_show,
		tpacpi_driver_wlsw_emulstate_store);

/* bluetooth_emulstate ------------------------------------------------- */
static ssize_t tpacpi_driver_bluetooth_emulstate_show(
					struct device_driver *drv,
					char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", !!tpacpi_bluetooth_emulstate);
}

static ssize_t tpacpi_driver_bluetooth_emulstate_store(
					struct device_driver *drv,
					const char *buf, size_t count)
{
	unsigned long t;

	if (parse_strtoul(buf, 1, &t))
		return -EINVAL;

	tpacpi_bluetooth_emulstate = !!t;

	return count;
}

static DRIVER_ATTR(bluetooth_emulstate, S_IWUSR | S_IRUGO,
		tpacpi_driver_bluetooth_emulstate_show,
		tpacpi_driver_bluetooth_emulstate_store);

/* wwan_emulstate ------------------------------------------------- */
static ssize_t tpacpi_driver_wwan_emulstate_show(
					struct device_driver *drv,
					char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", !!tpacpi_wwan_emulstate);
}

static ssize_t tpacpi_driver_wwan_emulstate_store(
					struct device_driver *drv,
					const char *buf, size_t count)
{
	unsigned long t;

	if (parse_strtoul(buf, 1, &t))
		return -EINVAL;

	tpacpi_wwan_emulstate = !!t;

	return count;
}

static DRIVER_ATTR(wwan_emulstate, S_IWUSR | S_IRUGO,
		tpacpi_driver_wwan_emulstate_show,
		tpacpi_driver_wwan_emulstate_store);

/* uwb_emulstate ------------------------------------------------- */
static ssize_t tpacpi_driver_uwb_emulstate_show(
					struct device_driver *drv,
					char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", !!tpacpi_uwb_emulstate);
}

static ssize_t tpacpi_driver_uwb_emulstate_store(
					struct device_driver *drv,
					const char *buf, size_t count)
{
	unsigned long t;

	if (parse_strtoul(buf, 1, &t))
		return -EINVAL;

	tpacpi_uwb_emulstate = !!t;

	return count;
}

static DRIVER_ATTR(uwb_emulstate, S_IWUSR | S_IRUGO,
		tpacpi_driver_uwb_emulstate_show,
		tpacpi_driver_uwb_emulstate_store);
#endif

/* --------------------------------------------------------------------- */

static struct driver_attribute *tpacpi_driver_attributes[] = {
	&driver_attr_debug_level, &driver_attr_version,
	&driver_attr_interface_version,
};

static int __init tpacpi_create_driver_attributes(struct device_driver *drv)
{
	int i, res;

	i = 0;
	res = 0;
	while (!res && i < ARRAY_SIZE(tpacpi_driver_attributes)) {
		res = driver_create_file(drv, tpacpi_driver_attributes[i]);
		i++;
	}

#ifdef CONFIG_THINKPAD_ACPI_DEBUGFACILITIES
	if (!res && dbg_wlswemul)
		res = driver_create_file(drv, &driver_attr_wlsw_emulstate);
	if (!res && dbg_bluetoothemul)
		res = driver_create_file(drv, &driver_attr_bluetooth_emulstate);
	if (!res && dbg_wwanemul)
		res = driver_create_file(drv, &driver_attr_wwan_emulstate);
	if (!res && dbg_uwbemul)
		res = driver_create_file(drv, &driver_attr_uwb_emulstate);
#endif

	return res;
}

static void tpacpi_remove_driver_attributes(struct device_driver *drv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tpacpi_driver_attributes); i++)
		driver_remove_file(drv, tpacpi_driver_attributes[i]);

#ifdef THINKPAD_ACPI_DEBUGFACILITIES
	driver_remove_file(drv, &driver_attr_wlsw_emulstate);
	driver_remove_file(drv, &driver_attr_bluetooth_emulstate);
	driver_remove_file(drv, &driver_attr_wwan_emulstate);
	driver_remove_file(drv, &driver_attr_uwb_emulstate);
#endif
}

/*************************************************************************
 * Firmware Data
 */

/*
 * Table of recommended minimum BIOS versions
 *
 * Reasons for listing:
 *    1. Stable BIOS, listed because the unknown ammount of
 *       bugs and bad ACPI behaviour on older versions
 *
 *    2. BIOS or EC fw with known bugs that trigger on Linux
 *
 *    3. BIOS with known reduced functionality in older versions
 *
 *  We recommend the latest BIOS and EC version.
 *  We only support the latest BIOS and EC fw version as a rule.
 *
 *  Sources: IBM ThinkPad Public Web Documents (update changelogs),
 *  Information from users in ThinkWiki
 *
 *  WARNING: we use this table also to detect that the machine is
 *  a ThinkPad in some cases, so don't remove entries lightly.
 */

#define TPV_Q(__v, __id1, __id2, __bv1, __bv2)		\
	{ .vendor	= (__v),			\
	  .bios		= TPID(__id1, __id2),		\
	  .ec		= TPACPI_MATCH_ANY,		\
	  .quirks	= TPACPI_MATCH_ANY << 16	\
			  | (__bv1) << 8 | (__bv2) }

#define TPV_Q_X(__v, __bid1, __bid2, __bv1, __bv2,	\
		__eid, __ev1, __ev2)			\
	{ .vendor	= (__v),			\
	  .bios		= TPID(__bid1, __bid2),		\
	  .ec		= __eid,			\
	  .quirks	= (__ev1) << 24 | (__ev2) << 16 \
			  | (__bv1) << 8 | (__bv2) }

#define TPV_QI0(__id1, __id2, __bv1, __bv2) \
	TPV_Q(PCI_VENDOR_ID_IBM, __id1, __id2, __bv1, __bv2)

/* Outdated IBM BIOSes often lack the EC id string */
#define TPV_QI1(__id1, __id2, __bv1, __bv2, __ev1, __ev2) \
	TPV_Q_X(PCI_VENDOR_ID_IBM, __id1, __id2, 	\
		__bv1, __bv2, TPID(__id1, __id2),	\
		__ev1, __ev2),				\
	TPV_Q_X(PCI_VENDOR_ID_IBM, __id1, __id2, 	\
		__bv1, __bv2, TPACPI_MATCH_UNKNOWN,	\
		__ev1, __ev2)

/* Outdated IBM BIOSes often lack the EC id string */
#define TPV_QI2(__bid1, __bid2, __bv1, __bv2,		\
		__eid1, __eid2, __ev1, __ev2) 		\
	TPV_Q_X(PCI_VENDOR_ID_IBM, __bid1, __bid2, 	\
		__bv1, __bv2, TPID(__eid1, __eid2),	\
		__ev1, __ev2),				\
	TPV_Q_X(PCI_VENDOR_ID_IBM, __bid1, __bid2, 	\
		__bv1, __bv2, TPACPI_MATCH_UNKNOWN,	\
		__ev1, __ev2)

#define TPV_QL0(__id1, __id2, __bv1, __bv2) \
	TPV_Q(PCI_VENDOR_ID_LENOVO, __id1, __id2, __bv1, __bv2)

#define TPV_QL1(__id1, __id2, __bv1, __bv2, __ev1, __ev2) \
	TPV_Q_X(PCI_VENDOR_ID_LENOVO, __id1, __id2, 	\
		__bv1, __bv2, TPID(__id1, __id2),	\
		__ev1, __ev2)

#define TPV_QL2(__bid1, __bid2, __bv1, __bv2,		\
		__eid1, __eid2, __ev1, __ev2) 		\
	TPV_Q_X(PCI_VENDOR_ID_LENOVO, __bid1, __bid2, 	\
		__bv1, __bv2, TPID(__eid1, __eid2),	\
		__ev1, __ev2)

static const struct tpacpi_quirk tpacpi_bios_version_qtable[] __initconst = {
	/*  Numeric models ------------------ */
	/*      FW MODEL   BIOS VERS	      */
	TPV_QI0('I', 'M',  '6', '5'),		 /* 570 */
	TPV_QI0('I', 'U',  '2', '6'),		 /* 570E */
	TPV_QI0('I', 'B',  '5', '4'),		 /* 600 */
	TPV_QI0('I', 'H',  '4', '7'),		 /* 600E */
	TPV_QI0('I', 'N',  '3', '6'),		 /* 600E */
	TPV_QI0('I', 'T',  '5', '5'),		 /* 600X */
	TPV_QI0('I', 'D',  '4', '8'),		 /* 770, 770E, 770ED */
	TPV_QI0('I', 'I',  '4', '2'),		 /* 770X */
	TPV_QI0('I', 'O',  '2', '3'),		 /* 770Z */

	/* A-series ------------------------- */
	/*      FW MODEL   BIOS VERS  EC VERS */
	TPV_QI0('I', 'W',  '5', '9'),		 /* A20m */
	TPV_QI0('I', 'V',  '6', '9'),		 /* A20p */
	TPV_QI0('1', '0',  '2', '6'),		 /* A21e, A22e */
	TPV_QI0('K', 'U',  '3', '6'),		 /* A21e */
	TPV_QI0('K', 'X',  '3', '6'),		 /* A21m, A22m */
	TPV_QI0('K', 'Y',  '3', '8'),		 /* A21p, A22p */
	TPV_QI0('1', 'B',  '1', '7'),		 /* A22e */
	TPV_QI0('1', '3',  '2', '0'),		 /* A22m */
	TPV_QI0('1', 'E',  '7', '3'),		 /* A30/p (0) */
	TPV_QI1('1', 'G',  '4', '1',  '1', '7'), /* A31/p (0) */
	TPV_QI1('1', 'N',  '1', '6',  '0', '7'), /* A31/p (0) */

	/* G-series ------------------------- */
	/*      FW MODEL   BIOS VERS	      */
	TPV_QI0('1', 'T',  'A', '6'),		 /* G40 */
	TPV_QI0('1', 'X',  '5', '7'),		 /* G41 */

	/* R-series, T-series --------------- */
	/*      FW MODEL   BIOS VERS  EC VERS */
	TPV_QI0('1', 'C',  'F', '0'),		 /* R30 */
	TPV_QI0('1', 'F',  'F', '1'),		 /* R31 */
	TPV_QI0('1', 'M',  '9', '7'),		 /* R32 */
	TPV_QI0('1', 'O',  '6', '1'),		 /* R40 */
	TPV_QI0('1', 'P',  '6', '5'),		 /* R40 */
	TPV_QI0('1', 'S',  '7', '0'),		 /* R40e */
	TPV_QI1('1', 'R',  'D', 'R',  '7', '1'), /* R50/p, R51,
						    T40/p, T41/p, T42/p (1) */
	TPV_QI1('1', 'V',  '7', '1',  '2', '8'), /* R50e, R51 (1) */
	TPV_QI1('7', '8',  '7', '1',  '0', '6'), /* R51e (1) */
	TPV_QI1('7', '6',  '6', '9',  '1', '6'), /* R52 (1) */
	TPV_QI1('7', '0',  '6', '9',  '2', '8'), /* R52, T43 (1) */

	TPV_QI0('I', 'Y',  '6', '1'),		 /* T20 */
	TPV_QI0('K', 'Z',  '3', '4'),		 /* T21 */
	TPV_QI0('1', '6',  '3', '2'),		 /* T22 */
	TPV_QI1('1', 'A',  '6', '4',  '2', '3'), /* T23 (0) */
	TPV_QI1('1', 'I',  '7', '1',  '2', '0'), /* T30 (0) */
	TPV_QI1('1', 'Y',  '6', '5',  '2', '9'), /* T43/p (1) */

	TPV_QL1('7', '9',  'E', '3',  '5', '0'), /* T60/p */
	TPV_QL1('7', 'C',  'D', '2',  '2', '2'), /* R60, R60i */
	TPV_QL0('7', 'E',  'D', '0'),		 /* R60e, R60i */

	/*      BIOS FW    BIOS VERS  EC FW     EC VERS */
	TPV_QI2('1', 'W',  '9', '0',  '1', 'V', '2', '8'), /* R50e (1) */
	TPV_QL2('7', 'I',  '3', '4',  '7', '9', '5', '0'), /* T60/p wide */

	/* X-series ------------------------- */
	/*      FW MODEL   BIOS VERS  EC VERS */
	TPV_QI0('I', 'Z',  '9', 'D'),		 /* X20, X21 */
	TPV_QI0('1', 'D',  '7', '0'),		 /* X22, X23, X24 */
	TPV_QI1('1', 'K',  '4', '8',  '1', '8'), /* X30 (0) */
	TPV_QI1('1', 'Q',  '9', '7',  '2', '3'), /* X31, X32 (0) */
	TPV_QI1('1', 'U',  'D', '3',  'B', '2'), /* X40 (0) */
	TPV_QI1('7', '4',  '6', '4',  '2', '7'), /* X41 (0) */
	TPV_QI1('7', '5',  '6', '0',  '2', '0'), /* X41t (0) */

	TPV_QL0('7', 'B',  'D', '7'),		 /* X60/s */
	TPV_QL0('7', 'J',  '3', '0'),		 /* X60t */

	/* (0) - older versions lack DMI EC fw string and functionality */
	/* (1) - older versions known to lack functionality */
};

#undef TPV_QL1
#undef TPV_QL0
#undef TPV_QI2
#undef TPV_QI1
#undef TPV_QI0
#undef TPV_Q_X
#undef TPV_Q

static void __init tpacpi_check_outdated_fw(void)
{
	unsigned long fwvers;
	u16 ec_version, bios_version;

	fwvers = tpacpi_check_quirks(tpacpi_bios_version_qtable,
				ARRAY_SIZE(tpacpi_bios_version_qtable));

	if (!fwvers)
		return;

	bios_version = fwvers & 0xffffU;
	ec_version = (fwvers >> 16) & 0xffffU;

	/* note that unknown versions are set to 0x0000 and we use that */
	if ((bios_version > thinkpad_id.bios_release) ||
	    (ec_version > thinkpad_id.ec_release &&
				ec_version != TPACPI_MATCH_ANY)) {
		/*
		 * The changelogs would let us track down the exact
		 * reason, but it is just too much of a pain to track
		 * it.  We only list BIOSes that are either really
		 * broken, or really stable to begin with, so it is
		 * best if the user upgrades the firmware anyway.
		 */
		printk(TPACPI_WARN
			"WARNING: Outdated ThinkPad BIOS/EC firmware\n");
		printk(TPACPI_WARN
			"WARNING: This firmware may be missing critical bug "
			"fixes and/or important features\n");
	}
}

static bool __init tpacpi_is_fw_known(void)
{
	return tpacpi_check_quirks(tpacpi_bios_version_qtable,
			ARRAY_SIZE(tpacpi_bios_version_qtable)) != 0;
}

/****************************************************************************
 ****************************************************************************
 *
 * Subdrivers
 *
 ****************************************************************************
 ****************************************************************************/

/*************************************************************************
 * thinkpad-acpi init subdriver
 */

static int __init thinkpad_acpi_driver_init(struct ibm_init_struct *iibm)
{
	printk(TPACPI_INFO "%s v%s\n", TPACPI_DESC, TPACPI_VERSION);
	printk(TPACPI_INFO "%s\n", TPACPI_URL);

	printk(TPACPI_INFO "ThinkPad BIOS %s, EC %s\n",
		(thinkpad_id.bios_version_str) ?
			thinkpad_id.bios_version_str : "unknown",
		(thinkpad_id.ec_version_str) ?
			thinkpad_id.ec_version_str : "unknown");

	if (thinkpad_id.vendor && thinkpad_id.model_str)
		printk(TPACPI_INFO "%s %s, model %s\n",
			(thinkpad_id.vendor == PCI_VENDOR_ID_IBM) ?
				"IBM" : ((thinkpad_id.vendor ==
						PCI_VENDOR_ID_LENOVO) ?
					"Lenovo" : "Unknown vendor"),
			thinkpad_id.model_str,
			(thinkpad_id.nummodel_str) ?
				thinkpad_id.nummodel_str : "unknown");

	tpacpi_check_outdated_fw();
	return 0;
}

static int thinkpad_acpi_driver_read(char *p)
{
	int len = 0;

	len += sprintf(p + len, "driver:\t\t%s\n", TPACPI_DESC);
	len += sprintf(p + len, "version:\t%s\n", TPACPI_VERSION);

	return len;
}

static struct ibm_struct thinkpad_acpi_driver_data = {
	.name = "driver",
	.read = thinkpad_acpi_driver_read,
};

/*************************************************************************
 * Hotkey subdriver
 */

/*
 * ThinkPad firmware event model
 *
 * The ThinkPad firmware has two main event interfaces: normal ACPI
 * notifications (which follow the ACPI standard), and a private event
 * interface.
 *
 * The private event interface also issues events for the hotkeys.  As
 * the driver gained features, the event handling code ended up being
 * built around the hotkey subdriver.  This will need to be refactored
 * to a more formal event API eventually.
 *
 * Some "hotkeys" are actually supposed to be used as event reports,
 * such as "brightness has changed", "volume has changed", depending on
 * the ThinkPad model and how the firmware is operating.
 *
 * Unlike other classes, hotkey-class events have mask/unmask control on
 * non-ancient firmware.  However, how it behaves changes a lot with the
 * firmware model and version.
 */

enum {	/* hot key scan codes (derived from ACPI DSDT) */
	TP_ACPI_HOTKEYSCAN_FNF1		= 0,
	TP_ACPI_HOTKEYSCAN_FNF2,
	TP_ACPI_HOTKEYSCAN_FNF3,
	TP_ACPI_HOTKEYSCAN_FNF4,
	TP_ACPI_HOTKEYSCAN_FNF5,
	TP_ACPI_HOTKEYSCAN_FNF6,
	TP_ACPI_HOTKEYSCAN_FNF7,
	TP_ACPI_HOTKEYSCAN_FNF8,
	TP_ACPI_HOTKEYSCAN_FNF9,
	TP_ACPI_HOTKEYSCAN_FNF10,
	TP_ACPI_HOTKEYSCAN_FNF11,
	TP_ACPI_HOTKEYSCAN_FNF12,
	TP_ACPI_HOTKEYSCAN_FNBACKSPACE,
	TP_ACPI_HOTKEYSCAN_FNINSERT,
	TP_ACPI_HOTKEYSCAN_FNDELETE,
	TP_ACPI_HOTKEYSCAN_FNHOME,
	TP_ACPI_HOTKEYSCAN_FNEND,
	TP_ACPI_HOTKEYSCAN_FNPAGEUP,
	TP_ACPI_HOTKEYSCAN_FNPAGEDOWN,
	TP_ACPI_HOTKEYSCAN_FNSPACE,
	TP_ACPI_HOTKEYSCAN_VOLUMEUP,
	TP_ACPI_HOTKEYSCAN_VOLUMEDOWN,
	TP_ACPI_HOTKEYSCAN_MUTE,
	TP_ACPI_HOTKEYSCAN_THINKPAD,
};

enum {	/* Keys/events available through NVRAM polling */
	TPACPI_HKEY_NVRAM_KNOWN_MASK = 0x00fb88c0U,
	TPACPI_HKEY_NVRAM_GOOD_MASK  = 0x00fb8000U,
};

enum {	/* Positions of some of the keys in hotkey masks */
	TP_ACPI_HKEY_DISPSWTCH_MASK	= 1 << TP_ACPI_HOTKEYSCAN_FNF7,
	TP_ACPI_HKEY_DISPXPAND_MASK	= 1 << TP_ACPI_HOTKEYSCAN_FNF8,
	TP_ACPI_HKEY_HIBERNATE_MASK	= 1 << TP_ACPI_HOTKEYSCAN_FNF12,
	TP_ACPI_HKEY_BRGHTUP_MASK	= 1 << TP_ACPI_HOTKEYSCAN_FNHOME,
	TP_ACPI_HKEY_BRGHTDWN_MASK	= 1 << TP_ACPI_HOTKEYSCAN_FNEND,
	TP_ACPI_HKEY_THNKLGHT_MASK	= 1 << TP_ACPI_HOTKEYSCAN_FNPAGEUP,
	TP_ACPI_HKEY_ZOOM_MASK		= 1 << TP_ACPI_HOTKEYSCAN_FNSPACE,
	TP_ACPI_HKEY_VOLUP_MASK		= 1 << TP_ACPI_HOTKEYSCAN_VOLUMEUP,
	TP_ACPI_HKEY_VOLDWN_MASK	= 1 << TP_ACPI_HOTKEYSCAN_VOLUMEDOWN,
	TP_ACPI_HKEY_MUTE_MASK		= 1 << TP_ACPI_HOTKEYSCAN_MUTE,
	TP_ACPI_HKEY_THINKPAD_MASK	= 1 << TP_ACPI_HOTKEYSCAN_THINKPAD,
};

enum {	/* NVRAM to ACPI HKEY group map */
	TP_NVRAM_HKEY_GROUP_HK2		= TP_ACPI_HKEY_THINKPAD_MASK |
					  TP_ACPI_HKEY_ZOOM_MASK |
					  TP_ACPI_HKEY_DISPSWTCH_MASK |
					  TP_ACPI_HKEY_HIBERNATE_MASK,
	TP_NVRAM_HKEY_GROUP_BRIGHTNESS	= TP_ACPI_HKEY_BRGHTUP_MASK |
					  TP_ACPI_HKEY_BRGHTDWN_MASK,
	TP_NVRAM_HKEY_GROUP_VOLUME	= TP_ACPI_HKEY_VOLUP_MASK |
					  TP_ACPI_HKEY_VOLDWN_MASK |
					  TP_ACPI_HKEY_MUTE_MASK,
};

#ifdef CONFIG_THINKPAD_ACPI_HOTKEY_POLL
struct tp_nvram_state {
       u16 thinkpad_toggle:1;
       u16 zoom_toggle:1;
       u16 display_toggle:1;
       u16 thinklight_toggle:1;
       u16 hibernate_toggle:1;
       u16 displayexp_toggle:1;
       u16 display_state:1;
       u16 brightness_toggle:1;
       u16 volume_toggle:1;
       u16 mute:1;

       u8 brightness_level;
       u8 volume_level;
};

/* kthread for the hotkey poller */
static struct task_struct *tpacpi_hotkey_task;

/* Acquired while the poller kthread is running, use to sync start/stop */
static struct mutex hotkey_thread_mutex;

/*
 * Acquire mutex to write poller control variables as an
 * atomic block.
 *
 * Increment hotkey_config_change when changing them if you
 * want the kthread to forget old state.
 *
 * See HOTKEY_CONFIG_CRITICAL_START/HOTKEY_CONFIG_CRITICAL_END
 */
static struct mutex hotkey_thread_data_mutex;
static unsigned int hotkey_config_change;

/*
 * hotkey poller control variables
 *
 * Must be atomic or readers will also need to acquire mutex
 *
 * HOTKEY_CONFIG_CRITICAL_START/HOTKEY_CONFIG_CRITICAL_END
 * should be used only when the changes need to be taken as
 * a block, OR when one needs to force the kthread to forget
 * old state.
 */
static u32 hotkey_source_mask;		/* bit mask 0=ACPI,1=NVRAM */
static unsigned int hotkey_poll_freq = 10; /* Hz */

#define HOTKEY_CONFIG_CRITICAL_START \
	do { \
		mutex_lock(&hotkey_thread_data_mutex); \
		hotkey_config_change++; \
	} while (0);
#define HOTKEY_CONFIG_CRITICAL_END \
	mutex_unlock(&hotkey_thread_data_mutex);

#else /* CONFIG_THINKPAD_ACPI_HOTKEY_POLL */

#define hotkey_source_mask 0U
#define HOTKEY_CONFIG_CRITICAL_START
#define HOTKEY_CONFIG_CRITICAL_END

#endif /* CONFIG_THINKPAD_ACPI_HOTKEY_POLL */

static struct mutex hotkey_mutex;

static enum {	/* Reasons for waking up */
	TP_ACPI_WAKEUP_NONE = 0,	/* None or unknown */
	TP_ACPI_WAKEUP_BAYEJ,		/* Bay ejection request */
	TP_ACPI_WAKEUP_UNDOCK,		/* Undock request */
} hotkey_wakeup_reason;

static int hotkey_autosleep_ack;

static u32 hotkey_orig_mask;		/* events the BIOS had enabled */
static u32 hotkey_all_mask;		/* all events supported in fw */
static u32 hotkey_reserved_mask;	/* events better left disabled */
static u32 hotkey_driver_mask;		/* events needed by the driver */
static u32 hotkey_user_mask;		/* events visible to userspace */
static u32 hotkey_acpi_mask;		/* events enabled in firmware */

static unsigned int hotkey_report_mode;

static u16 *hotkey_keycode_map;

static struct attribute_set *hotkey_dev_attributes;

static void tpacpi_driver_event(const unsigned int hkey_event);
static void hotkey_driver_event(const unsigned int scancode);

/* HKEY.MHKG() return bits */
#define TP_HOTKEY_TABLET_MASK (1 << 3)

static int hotkey_get_wlsw(void)
{
	int status;

	if (!tp_features.hotkey_wlsw)
		return -ENODEV;

#ifdef CONFIG_THINKPAD_ACPI_DEBUGFACILITIES
	if (dbg_wlswemul)
		return (tpacpi_wlsw_emulstate) ?
				TPACPI_RFK_RADIO_ON : TPACPI_RFK_RADIO_OFF;
#endif

	if (!acpi_evalf(hkey_handle, &status, "WLSW", "d"))
		return -EIO;

	return (status) ? TPACPI_RFK_RADIO_ON : TPACPI_RFK_RADIO_OFF;
}

static int hotkey_get_tablet_mode(int *status)
{
	int s;

	if (!acpi_evalf(hkey_handle, &s, "MHKG", "d"))
		return -EIO;

	*status = ((s & TP_HOTKEY_TABLET_MASK) != 0);
	return 0;
}

/*
 * Reads current event mask from firmware, and updates
 * hotkey_acpi_mask accordingly.  Also resets any bits
 * from hotkey_user_mask that are unavailable to be
 * delivered (shadow requirement of the userspace ABI).
 *
 * Call with hotkey_mutex held
 */
static int hotkey_mask_get(void)
{
	if (tp_features.hotkey_mask) {
		u32 m = 0;

		if (!acpi_evalf(hkey_handle, &m, "DHKN", "d"))
			return -EIO;

		hotkey_acpi_mask = m;
	} else {
		/* no mask support doesn't mean no event support... */
		hotkey_acpi_mask = hotkey_all_mask;
	}

	/* sync userspace-visible mask */
	hotkey_user_mask &= (hotkey_acpi_mask | hotkey_source_mask);

	return 0;
}

void static hotkey_mask_warn_incomplete_mask(void)
{
	/* log only what the user can fix... */
	const u32 wantedmask = hotkey_driver_mask &
		~(hotkey_acpi_mask | hotkey_source_mask) &
		(hotkey_all_mask | TPACPI_HKEY_NVRAM_KNOWN_MASK);

	if (wantedmask)
		printk(TPACPI_NOTICE
			"required events 0x%08x not enabled!\n",
			wantedmask);
}

/*
 * Set the firmware mask when supported
 *
 * Also calls hotkey_mask_get to update hotkey_acpi_mask.
 *
 * NOTE: does not set bits in hotkey_user_mask, but may reset them.
 *
 * Call with hotkey_mutex held
 */
static int hotkey_mask_set(u32 mask)
{
	int i;
	int rc = 0;

	const u32 fwmask = mask & ~hotkey_source_mask;

	if (tp_features.hotkey_mask) {
		for (i = 0; i < 32; i++) {
			if (!acpi_evalf(hkey_handle,
					NULL, "MHKM", "vdd", i + 1,
					!!(mask & (1 << i)))) {
				rc = -EIO;
				break;
			}
		}
	}

	/*
	 * We *must* make an inconditional call to hotkey_mask_get to
	 * refresh hotkey_acpi_mask and update hotkey_user_mask
	 *
	 * Take the opportunity to also log when we cannot _enable_
	 * a given event.
	 */
	if (!hotkey_mask_get() && !rc && (fwmask & ~hotkey_acpi_mask)) {
		printk(TPACPI_NOTICE
		       "asked for hotkey mask 0x%08x, but "
		       "firmware forced it to 0x%08x\n",
		       fwmask, hotkey_acpi_mask);
	}

	hotkey_mask_warn_incomplete_mask();

	return rc;
}

/*
 * Sets hotkey_user_mask and tries to set the firmware mask
 *
 * Call with hotkey_mutex held
 */
static int hotkey_user_mask_set(const u32 mask)
{
	int rc;

	/* Give people a chance to notice they are doing something that
	 * is bound to go boom on their users sooner or later */
	if (!tp_warned.hotkey_mask_ff &&
	    (mask == 0xffff || mask == 0xffffff ||
	     mask == 0xffffffff)) {
		tp_warned.hotkey_mask_ff = 1;
		printk(TPACPI_NOTICE
		       "setting the hotkey mask to 0x%08x is likely "
		       "not the best way to go about it\n", mask);
		printk(TPACPI_NOTICE
		       "please consider using the driver defaults, "
		       "and refer to up-to-date thinkpad-acpi "
		       "documentation\n");
	}

	/* Try to enable what the user asked for, plus whatever we need.
	 * this syncs everything but won't enable bits in hotkey_user_mask */
	rc = hotkey_mask_set((mask | hotkey_driver_mask) & ~hotkey_source_mask);

	/* Enable the available bits in hotkey_user_mask */
	hotkey_user_mask = mask & (hotkey_acpi_mask | hotkey_source_mask);

	return rc;
}

/*
 * Sets the driver hotkey mask.
 *
 * Can be called even if the hotkey subdriver is inactive
 */
static int tpacpi_hotkey_driver_mask_set(const u32 mask)
{
	int rc;

	/* Do the right thing if hotkey_init has not been called yet */
	if (!tp_features.hotkey) {
		hotkey_driver_mask = mask;
		return 0;
	}

	mutex_lock(&hotkey_mutex);

	HOTKEY_CONFIG_CRITICAL_START
	hotkey_driver_mask = mask;
#ifdef CONFIG_THINKPAD_ACPI_HOTKEY_POLL
	hotkey_source_mask |= (mask & ~hotkey_all_mask);
#endif
	HOTKEY_CONFIG_CRITICAL_END

	rc = hotkey_mask_set((hotkey_acpi_mask | hotkey_driver_mask) &
							~hotkey_source_mask);
	mutex_unlock(&hotkey_mutex);

	return rc;
}

static int hotkey_status_get(int *status)
{
	if (!acpi_evalf(hkey_handle, status, "DHKC", "d"))
		return -EIO;

	return 0;
}

static int hotkey_status_set(bool enable)
{
	if (!acpi_evalf(hkey_handle, NULL, "MHKC", "vd", enable ? 1 : 0))
		return -EIO;

	return 0;
}

static void tpacpi_input_send_tabletsw(void)
{
	int state;

	if (tp_features.hotkey_tablet &&
	    !hotkey_get_tablet_mode(&state)) {
		mutex_lock(&tpacpi_inputdev_send_mutex);

		input_report_switch(tpacpi_inputdev,
				    SW_TABLET_MODE, !!state);
		input_sync(tpacpi_inputdev);

		mutex_unlock(&tpacpi_inputdev_send_mutex);
	}
}

/* Do NOT call without validating scancode first */
static void tpacpi_input_send_key(const unsigned int scancode)
{
	const unsigned int keycode = hotkey_keycode_map[scancode];

	if (keycode != KEY_RESERVED) {
		mutex_lock(&tpacpi_inputdev_send_mutex);

		input_report_key(tpacpi_inputdev, keycode, 1);
		if (keycode == KEY_UNKNOWN)
			input_event(tpacpi_inputdev, EV_MSC, MSC_SCAN,
				    scancode);
		input_sync(tpacpi_inputdev);

		input_report_key(tpacpi_inputdev, keycode, 0);
		if (keycode == KEY_UNKNOWN)
			input_event(tpacpi_inputdev, EV_MSC, MSC_SCAN,
				    scancode);
		input_sync(tpacpi_inputdev);

		mutex_unlock(&tpacpi_inputdev_send_mutex);
	}
}

/* Do NOT call without validating scancode first */
static void tpacpi_input_send_key_masked(const unsigned int scancode)
{
	hotkey_driver_event(scancode);
	if (hotkey_user_mask & (1 << scancode))
		tpacpi_input_send_key(scancode);
}

#ifdef CONFIG_THINKPAD_ACPI_HOTKEY_POLL
static struct tp_acpi_drv_struct ibm_hotkey_acpidriver;

/* Do NOT call without validating scancode first */
static void tpacpi_hotkey_send_key(unsigned int scancode)
{
	tpacpi_input_send_key_masked(scancode);
	if (hotkey_report_mode < 2) {
		acpi_bus_generate_proc_event(ibm_hotkey_acpidriver.device,
				0x80, TP_HKEY_EV_HOTKEY_BASE + scancode);
	}
}

static void hotkey_read_nvram(struct tp_nvram_state *n, const u32 m)
{
	u8 d;

	if (m & TP_NVRAM_HKEY_GROUP_HK2) {
		d = nvram_read_byte(TP_NVRAM_ADDR_HK2);
		n->thinkpad_toggle = !!(d & TP_NVRAM_MASK_HKT_THINKPAD);
		n->zoom_toggle = !!(d & TP_NVRAM_MASK_HKT_ZOOM);
		n->display_toggle = !!(d & TP_NVRAM_MASK_HKT_DISPLAY);
		n->hibernate_toggle = !!(d & TP_NVRAM_MASK_HKT_HIBERNATE);
	}
	if (m & TP_ACPI_HKEY_THNKLGHT_MASK) {
		d = nvram_read_byte(TP_NVRAM_ADDR_THINKLIGHT);
		n->thinklight_toggle = !!(d & TP_NVRAM_MASK_THINKLIGHT);
	}
	if (m & TP_ACPI_HKEY_DISPXPAND_MASK) {
		d = nvram_read_byte(TP_NVRAM_ADDR_VIDEO);
		n->displayexp_toggle =
				!!(d & TP_NVRAM_MASK_HKT_DISPEXPND);
	}
	if (m & TP_NVRAM_HKEY_GROUP_BRIGHTNESS) {
		d = nvram_read_byte(TP_NVRAM_ADDR_BRIGHTNESS);
		n->brightness_level = (d & TP_NVRAM_MASK_LEVEL_BRIGHTNESS)
				>> TP_NVRAM_POS_LEVEL_BRIGHTNESS;
		n->brightness_toggle =
				!!(d & TP_NVRAM_MASK_HKT_BRIGHTNESS);
	}
	if (m & TP_NVRAM_HKEY_GROUP_VOLUME) {
		d = nvram_read_byte(TP_NVRAM_ADDR_MIXER);
		n->volume_level = (d & TP_NVRAM_MASK_LEVEL_VOLUME)
				>> TP_NVRAM_POS_LEVEL_VOLUME;
		n->mute = !!(d & TP_NVRAM_MASK_MUTE);
		n->volume_toggle = !!(d & TP_NVRAM_MASK_HKT_VOLUME);
	}
}

static void hotkey_compare_and_issue_event(struct tp_nvram_state *oldn,
					   struct tp_nvram_state *newn,
					   const u32 event_mask)
{

#define TPACPI_COMPARE_KEY(__scancode, __member) \
	do { \
		if ((event_mask & (1 << __scancode)) && \
		    oldn->__member != newn->__member) \
			tpacpi_hotkey_send_key(__scancode); \
	} while (0)

#define TPACPI_MAY_SEND_KEY(__scancode) \
	do { \
		if (event_mask & (1 << __scancode)) \
			tpacpi_hotkey_send_key(__scancode); \
	} while (0)

	TPACPI_COMPARE_KEY(TP_ACPI_HOTKEYSCAN_THINKPAD, thinkpad_toggle);
	TPACPI_COMPARE_KEY(TP_ACPI_HOTKEYSCAN_FNSPACE, zoom_toggle);
	TPACPI_COMPARE_KEY(TP_ACPI_HOTKEYSCAN_FNF7, display_toggle);
	TPACPI_COMPARE_KEY(TP_ACPI_HOTKEYSCAN_FNF12, hibernate_toggle);

	TPACPI_COMPARE_KEY(TP_ACPI_HOTKEYSCAN_FNPAGEUP, thinklight_toggle);

	TPACPI_COMPARE_KEY(TP_ACPI_HOTKEYSCAN_FNF8, displayexp_toggle);

	/* handle volume */
	if (oldn->volume_toggle != newn->volume_toggle) {
		if (oldn->mute != newn->mute) {
			TPACPI_MAY_SEND_KEY(TP_ACPI_HOTKEYSCAN_MUTE);
		}
		if (oldn->volume_level > newn->volume_level) {
			TPACPI_MAY_SEND_KEY(TP_ACPI_HOTKEYSCAN_VOLUMEDOWN);
		} else if (oldn->volume_level < newn->volume_level) {
			TPACPI_MAY_SEND_KEY(TP_ACPI_HOTKEYSCAN_VOLUMEUP);
		} else if (oldn->mute == newn->mute) {
			/* repeated key presses that didn't change state */
			if (newn->mute) {
				TPACPI_MAY_SEND_KEY(TP_ACPI_HOTKEYSCAN_MUTE);
			} else if (newn->volume_level != 0) {
				TPACPI_MAY_SEND_KEY(TP_ACPI_HOTKEYSCAN_VOLUMEUP);
			} else {
				TPACPI_MAY_SEND_KEY(TP_ACPI_HOTKEYSCAN_VOLUMEDOWN);
			}
		}
	}

	/* handle brightness */
	if (oldn->brightness_toggle != newn->brightness_toggle) {
		if (oldn->brightness_level < newn->brightness_level) {
			TPACPI_MAY_SEND_KEY(TP_ACPI_HOTKEYSCAN_FNHOME);
		} else if (oldn->brightness_level > newn->brightness_level) {
			TPACPI_MAY_SEND_KEY(TP_ACPI_HOTKEYSCAN_FNEND);
		} else {
			/* repeated key presses that didn't change state */
			if (newn->brightness_level != 0) {
				TPACPI_MAY_SEND_KEY(TP_ACPI_HOTKEYSCAN_FNHOME);
			} else {
				TPACPI_MAY_SEND_KEY(TP_ACPI_HOTKEYSCAN_FNEND);
			}
		}
	}

#undef TPACPI_COMPARE_KEY
#undef TPACPI_MAY_SEND_KEY
}

/*
 * Polling driver
 *
 * We track all events in hotkey_source_mask all the time, since
 * most of them are edge-based.  We only issue those requested by
 * hotkey_user_mask or hotkey_driver_mask, though.
 */
static int hotkey_kthread(void *data)
{
	struct tp_nvram_state s[2];
	u32 poll_mask, event_mask;
	unsigned int si, so;
	unsigned long t;
	unsigned int change_detector, must_reset;
	unsigned int poll_freq;

	mutex_lock(&hotkey_thread_mutex);

	if (tpacpi_lifecycle == TPACPI_LIFE_EXITING)
		goto exit;

	set_freezable();

	so = 0;
	si = 1;
	t = 0;

	/* Initial state for compares */
	mutex_lock(&hotkey_thread_data_mutex);
	change_detector = hotkey_config_change;
	poll_mask = hotkey_source_mask;
	event_mask = hotkey_source_mask &
			(hotkey_driver_mask | hotkey_user_mask);
	poll_freq = hotkey_poll_freq;
	mutex_unlock(&hotkey_thread_data_mutex);
	hotkey_read_nvram(&s[so], poll_mask);

	while (!kthread_should_stop()) {
		if (t == 0) {
			if (likely(poll_freq))
				t = 1000/poll_freq;
			else
				t = 100;	/* should never happen... */
		}
		t = msleep_interruptible(t);
		if (unlikely(kthread_should_stop()))
			break;
		must_reset = try_to_freeze();
		if (t > 0 && !must_reset)
			continue;

		mutex_lock(&hotkey_thread_data_mutex);
		if (must_reset || hotkey_config_change != change_detector) {
			/* forget old state on thaw or config change */
			si = so;
			t = 0;
			change_detector = hotkey_config_change;
		}
		poll_mask = hotkey_source_mask;
		event_mask = hotkey_source_mask &
				(hotkey_driver_mask | hotkey_user_mask);
		poll_freq = hotkey_poll_freq;
		mutex_unlock(&hotkey_thread_data_mutex);

		if (likely(poll_mask)) {
			hotkey_read_nvram(&s[si], poll_mask);
			if (likely(si != so)) {
				hotkey_compare_and_issue_event(&s[so], &s[si],
								event_mask);
			}
		}

		so = si;
		si ^= 1;
	}

exit:
	mutex_unlock(&hotkey_thread_mutex);
	return 0;
}

/* call with hotkey_mutex held */
static void hotkey_poll_stop_sync(void)
{
	if (tpacpi_hotkey_task) {
		if (frozen(tpacpi_hotkey_task) ||
		    freezing(tpacpi_hotkey_task))
			thaw_process(tpacpi_hotkey_task);

		kthread_stop(tpacpi_hotkey_task);
		tpacpi_hotkey_task = NULL;
		mutex_lock(&hotkey_thread_mutex);
		/* at this point, the thread did exit */
		mutex_unlock(&hotkey_thread_mutex);
	}
}

/* call with hotkey_mutex held */
static void hotkey_poll_setup(bool may_warn)
{
	const u32 poll_driver_mask = hotkey_driver_mask & hotkey_source_mask;
	const u32 poll_user_mask = hotkey_user_mask & hotkey_source_mask;

	if (hotkey_poll_freq > 0 &&
	    (poll_driver_mask ||
	     (poll_user_mask && tpacpi_inputdev->users > 0))) {
		if (!tpacpi_hotkey_task) {
			tpacpi_hotkey_task = kthread_run(hotkey_kthread,
					NULL, TPACPI_NVRAM_KTHREAD_NAME);
			if (IS_ERR(tpacpi_hotkey_task)) {
				tpacpi_hotkey_task = NULL;
				printk(TPACPI_ERR
				       "could not create kernel thread "
				       "for hotkey polling\n");
			}
		}
	} else {
		hotkey_poll_stop_sync();
		if (may_warn && (poll_driver_mask || poll_user_mask) &&
		    hotkey_poll_freq == 0) {
			printk(TPACPI_NOTICE
				"hot keys 0x%08x and/or events 0x%08x "
				"require polling, which is currently "
				"disabled\n",
				poll_user_mask, poll_driver_mask);
		}
	}
}

static void hotkey_poll_setup_safe(bool may_warn)
{
	mutex_lock(&hotkey_mutex);
	hotkey_poll_setup(may_warn);
	mutex_unlock(&hotkey_mutex);
}

/* call with hotkey_mutex held */
static void hotkey_poll_set_freq(unsigned int freq)
{
	if (!freq)
		hotkey_poll_stop_sync();

	hotkey_poll_freq = freq;
}

#else /* CONFIG_THINKPAD_ACPI_HOTKEY_POLL */

static void hotkey_poll_setup_safe(bool __unused)
{
}

#endif /* CONFIG_THINKPAD_ACPI_HOTKEY_POLL */

static int hotkey_inputdev_open(struct input_dev *dev)
{
	switch (tpacpi_lifecycle) {
	case TPACPI_LIFE_INIT:
		/*
		 * hotkey_init will call hotkey_poll_setup_safe
		 * at the appropriate moment
		 */
		return 0;
	case TPACPI_LIFE_EXITING:
		return -EBUSY;
	case TPACPI_LIFE_RUNNING:
		hotkey_poll_setup_safe(false);
		return 0;
	}

	/* Should only happen if tpacpi_lifecycle is corrupt */
	BUG();
	return -EBUSY;
}

static void hotkey_inputdev_close(struct input_dev *dev)
{
	/* disable hotkey polling when possible */
	if (tpacpi_lifecycle == TPACPI_LIFE_RUNNING &&
	    !(hotkey_source_mask & hotkey_driver_mask))
		hotkey_poll_setup_safe(false);
}

/* sysfs hotkey enable ------------------------------------------------- */
static ssize_t hotkey_enable_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int res, status;

	printk_deprecated_attribute("hotkey_enable",
			"Hotkey reporting is always enabled");

	res = hotkey_status_get(&status);
	if (res)
		return res;

	return snprintf(buf, PAGE_SIZE, "%d\n", status);
}

static ssize_t hotkey_enable_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned long t;

	printk_deprecated_attribute("hotkey_enable",
			"Hotkeys can be disabled through hotkey_mask");

	if (parse_strtoul(buf, 1, &t))
		return -EINVAL;

	if (t == 0)
		return -EPERM;

	return count;
}

static struct device_attribute dev_attr_hotkey_enable =
	__ATTR(hotkey_enable, S_IWUSR | S_IRUGO,
		hotkey_enable_show, hotkey_enable_store);

/* sysfs hotkey mask --------------------------------------------------- */
static ssize_t hotkey_mask_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08x\n", hotkey_user_mask);
}

static ssize_t hotkey_mask_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned long t;
	int res;

	if (parse_strtoul(buf, 0xffffffffUL, &t))
		return -EINVAL;

	if (mutex_lock_killable(&hotkey_mutex))
		return -ERESTARTSYS;

	res = hotkey_user_mask_set(t);

#ifdef CONFIG_THINKPAD_ACPI_HOTKEY_POLL
	hotkey_poll_setup(true);
#endif

	mutex_unlock(&hotkey_mutex);

	tpacpi_disclose_usertask("hotkey_mask", "set to 0x%08lx\n", t);

	return (res) ? res : count;
}

static struct device_attribute dev_attr_hotkey_mask =
	__ATTR(hotkey_mask, S_IWUSR | S_IRUGO,
		hotkey_mask_show, hotkey_mask_store);

/* sysfs hotkey bios_enabled ------------------------------------------- */
static ssize_t hotkey_bios_enabled_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return sprintf(buf, "0\n");
}

static struct device_attribute dev_attr_hotkey_bios_enabled =
	__ATTR(hotkey_bios_enabled, S_IRUGO, hotkey_bios_enabled_show, NULL);

/* sysfs hotkey bios_mask ---------------------------------------------- */
static ssize_t hotkey_bios_mask_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	printk_deprecated_attribute("hotkey_bios_mask",
			"This attribute is useless.");
	return snprintf(buf, PAGE_SIZE, "0x%08x\n", hotkey_orig_mask);
}

static struct device_attribute dev_attr_hotkey_bios_mask =
	__ATTR(hotkey_bios_mask, S_IRUGO, hotkey_bios_mask_show, NULL);

/* sysfs hotkey all_mask ----------------------------------------------- */
static ssize_t hotkey_all_mask_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08x\n",
				hotkey_all_mask | hotkey_source_mask);
}

static struct device_attribute dev_attr_hotkey_all_mask =
	__ATTR(hotkey_all_mask, S_IRUGO, hotkey_all_mask_show, NULL);

/* sysfs hotkey recommended_mask --------------------------------------- */
static ssize_t hotkey_recommended_mask_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08x\n",
			(hotkey_all_mask | hotkey_source_mask)
			& ~hotkey_reserved_mask);
}

static struct device_attribute dev_attr_hotkey_recommended_mask =
	__ATTR(hotkey_recommended_mask, S_IRUGO,
		hotkey_recommended_mask_show, NULL);

#ifdef CONFIG_THINKPAD_ACPI_HOTKEY_POLL

/* sysfs hotkey hotkey_source_mask ------------------------------------- */
static ssize_t hotkey_source_mask_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08x\n", hotkey_source_mask);
}

static ssize_t hotkey_source_mask_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned long t;
	u32 r_ev;
	int rc;

	if (parse_strtoul(buf, 0xffffffffUL, &t) ||
		((t & ~TPACPI_HKEY_NVRAM_KNOWN_MASK) != 0))
		return -EINVAL;

	if (mutex_lock_killable(&hotkey_mutex))
		return -ERESTARTSYS;

	HOTKEY_CONFIG_CRITICAL_START
	hotkey_source_mask = t;
	HOTKEY_CONFIG_CRITICAL_END

	rc = hotkey_mask_set((hotkey_user_mask | hotkey_driver_mask) &
			~hotkey_source_mask);
	hotkey_poll_setup(true);

	/* check if events needed by the driver got disabled */
	r_ev = hotkey_driver_mask & ~(hotkey_acpi_mask & hotkey_all_mask)
		& ~hotkey_source_mask & TPACPI_HKEY_NVRAM_KNOWN_MASK;

	mutex_unlock(&hotkey_mutex);

	if (rc < 0)
		printk(TPACPI_ERR "hotkey_source_mask: failed to update the"
			"firmware event mask!\n");

	if (r_ev)
		printk(TPACPI_NOTICE "hotkey_source_mask: "
			"some important events were disabled: "
			"0x%04x\n", r_ev);

	tpacpi_disclose_usertask("hotkey_source_mask", "set to 0x%08lx\n", t);

	return (rc < 0) ? rc : count;
}

static struct device_attribute dev_attr_hotkey_source_mask =
	__ATTR(hotkey_source_mask, S_IWUSR | S_IRUGO,
		hotkey_source_mask_show, hotkey_source_mask_store);

/* sysfs hotkey hotkey_poll_freq --------------------------------------- */
static ssize_t hotkey_poll_freq_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", hotkey_poll_freq);
}

static ssize_t hotkey_poll_freq_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned long t;

	if (parse_strtoul(buf, 25, &t))
		return -EINVAL;

	if (mutex_lock_killable(&hotkey_mutex))
		return -ERESTARTSYS;

	hotkey_poll_set_freq(t);
	hotkey_poll_setup(true);

	mutex_unlock(&hotkey_mutex);

	tpacpi_disclose_usertask("hotkey_poll_freq", "set to %lu\n", t);

	return count;
}

static struct device_attribute dev_attr_hotkey_poll_freq =
	__ATTR(hotkey_poll_freq, S_IWUSR | S_IRUGO,
		hotkey_poll_freq_show, hotkey_poll_freq_store);

#endif /* CONFIG_THINKPAD_ACPI_HOTKEY_POLL */

/* sysfs hotkey radio_sw (pollable) ------------------------------------ */
static ssize_t hotkey_radio_sw_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int res;
	res = hotkey_get_wlsw();
	if (res < 0)
		return res;

	/* Opportunistic update */
	tpacpi_rfk_update_hwblock_state((res == TPACPI_RFK_RADIO_OFF));

	return snprintf(buf, PAGE_SIZE, "%d\n",
			(res == TPACPI_RFK_RADIO_OFF) ? 0 : 1);
}

static struct device_attribute dev_attr_hotkey_radio_sw =
	__ATTR(hotkey_radio_sw, S_IRUGO, hotkey_radio_sw_show, NULL);

static void hotkey_radio_sw_notify_change(void)
{
	if (tp_features.hotkey_wlsw)
		sysfs_notify(&tpacpi_pdev->dev.kobj, NULL,
			     "hotkey_radio_sw");
}

/* sysfs hotkey tablet mode (pollable) --------------------------------- */
static ssize_t hotkey_tablet_mode_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int res, s;
	res = hotkey_get_tablet_mode(&s);
	if (res < 0)
		return res;

	return snprintf(buf, PAGE_SIZE, "%d\n", !!s);
}

static struct device_attribute dev_attr_hotkey_tablet_mode =
	__ATTR(hotkey_tablet_mode, S_IRUGO, hotkey_tablet_mode_show, NULL);

static void hotkey_tablet_mode_notify_change(void)
{
	if (tp_features.hotkey_tablet)
		sysfs_notify(&tpacpi_pdev->dev.kobj, NULL,
			     "hotkey_tablet_mode");
}

/* sysfs hotkey report_mode -------------------------------------------- */
static ssize_t hotkey_report_mode_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
		(hotkey_report_mode != 0) ? hotkey_report_mode : 1);
}

static struct device_attribute dev_attr_hotkey_report_mode =
	__ATTR(hotkey_report_mode, S_IRUGO, hotkey_report_mode_show, NULL);

/* sysfs wakeup reason (pollable) -------------------------------------- */
static ssize_t hotkey_wakeup_reason_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", hotkey_wakeup_reason);
}

static struct device_attribute dev_attr_hotkey_wakeup_reason =
	__ATTR(wakeup_reason, S_IRUGO, hotkey_wakeup_reason_show, NULL);

static void hotkey_wakeup_reason_notify_change(void)
{
	sysfs_notify(&tpacpi_pdev->dev.kobj, NULL,
		     "wakeup_reason");
}

/* sysfs wakeup hotunplug_complete (pollable) -------------------------- */
static ssize_t hotkey_wakeup_hotunplug_complete_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", hotkey_autosleep_ack);
}

static struct device_attribute dev_attr_hotkey_wakeup_hotunplug_complete =
	__ATTR(wakeup_hotunplug_complete, S_IRUGO,
	       hotkey_wakeup_hotunplug_complete_show, NULL);

static void hotkey_wakeup_hotunplug_complete_notify_change(void)
{
	sysfs_notify(&tpacpi_pdev->dev.kobj, NULL,
		     "wakeup_hotunplug_complete");
}

/* --------------------------------------------------------------------- */

static struct attribute *hotkey_attributes[] __initdata = {
	&dev_attr_hotkey_enable.attr,
	&dev_attr_hotkey_bios_enabled.attr,
	&dev_attr_hotkey_bios_mask.attr,
	&dev_attr_hotkey_report_mode.attr,
	&dev_attr_hotkey_wakeup_reason.attr,
	&dev_attr_hotkey_wakeup_hotunplug_complete.attr,
	&dev_attr_hotkey_mask.attr,
	&dev_attr_hotkey_all_mask.attr,
	&dev_attr_hotkey_recommended_mask.attr,
#ifdef CONFIG_THINKPAD_ACPI_HOTKEY_POLL
	&dev_attr_hotkey_source_mask.attr,
	&dev_attr_hotkey_poll_freq.attr,
#endif
};

/*
 * Sync both the hw and sw blocking state of all switches
 */
static void tpacpi_send_radiosw_update(void)
{
	int wlsw;

	/*
	 * We must sync all rfkill controllers *before* issuing any
	 * rfkill input events, or we will race the rfkill core input
	 * handler.
	 *
	 * tpacpi_inputdev_send_mutex works as a syncronization point
	 * for the above.
	 *
	 * We optimize to avoid numerous calls to hotkey_get_wlsw.
	 */

	wlsw = hotkey_get_wlsw();

	/* Sync hw blocking state first if it is hw-blocked */
	if (wlsw == TPACPI_RFK_RADIO_OFF)
		tpacpi_rfk_update_hwblock_state(true);

	/* Sync sw blocking state */
	tpacpi_rfk_update_swstate_all();

	/* Sync hw blocking state last if it is hw-unblocked */
	if (wlsw == TPACPI_RFK_RADIO_ON)
		tpacpi_rfk_update_hwblock_state(false);

	/* Issue rfkill input event for WLSW switch */
	if (!(wlsw < 0)) {
		mutex_lock(&tpacpi_inputdev_send_mutex);

		input_report_switch(tpacpi_inputdev,
				    SW_RFKILL_ALL, (wlsw > 0));
		input_sync(tpacpi_inputdev);

		mutex_unlock(&tpacpi_inputdev_send_mutex);
	}

	/*
	 * this can be unconditional, as we will poll state again
	 * if userspace uses the notify to read data
	 */
	hotkey_radio_sw_notify_change();
}

static void hotkey_exit(void)
{
#ifdef CONFIG_THINKPAD_ACPI_HOTKEY_POLL
	mutex_lock(&hotkey_mutex);
	hotkey_poll_stop_sync();
	mutex_unlock(&hotkey_mutex);
#endif

	if (hotkey_dev_attributes)
		delete_attr_set(hotkey_dev_attributes, &tpacpi_pdev->dev.kobj);

	kfree(hotkey_keycode_map);

	dbg_printk(TPACPI_DBG_EXIT | TPACPI_DBG_HKEY,
		   "restoring original HKEY status and mask\n");
	/* yes, there is a bitwise or below, we want the
	 * functions to be called even if one of them fail */
	if (((tp_features.hotkey_mask &&
	      hotkey_mask_set(hotkey_orig_mask)) |
	     hotkey_status_set(false)) != 0)
		printk(TPACPI_ERR
		       "failed to restore hot key mask "
		       "to BIOS defaults\n");
}

static void __init hotkey_unmap(const unsigned int scancode)
{
	if (hotkey_keycode_map[scancode] != KEY_RESERVED) {
		clear_bit(hotkey_keycode_map[scancode],
			  tpacpi_inputdev->keybit);
		hotkey_keycode_map[scancode] = KEY_RESERVED;
	}
}

/*
 * HKEY quirks:
 *   TPACPI_HK_Q_INIMASK:	Supports FN+F3,FN+F4,FN+F12
 */

#define	TPACPI_HK_Q_INIMASK	0x0001

static const struct tpacpi_quirk tpacpi_hotkey_qtable[] __initconst = {
	TPACPI_Q_IBM('I', 'H', TPACPI_HK_Q_INIMASK), /* 600E */
	TPACPI_Q_IBM('I', 'N', TPACPI_HK_Q_INIMASK), /* 600E */
	TPACPI_Q_IBM('I', 'D', TPACPI_HK_Q_INIMASK), /* 770, 770E, 770ED */
	TPACPI_Q_IBM('I', 'W', TPACPI_HK_Q_INIMASK), /* A20m */
	TPACPI_Q_IBM('I', 'V', TPACPI_HK_Q_INIMASK), /* A20p */
	TPACPI_Q_IBM('1', '0', TPACPI_HK_Q_INIMASK), /* A21e, A22e */
	TPACPI_Q_IBM('K', 'U', TPACPI_HK_Q_INIMASK), /* A21e */
	TPACPI_Q_IBM('K', 'X', TPACPI_HK_Q_INIMASK), /* A21m, A22m */
	TPACPI_Q_IBM('K', 'Y', TPACPI_HK_Q_INIMASK), /* A21p, A22p */
	TPACPI_Q_IBM('1', 'B', TPACPI_HK_Q_INIMASK), /* A22e */
	TPACPI_Q_IBM('1', '3', TPACPI_HK_Q_INIMASK), /* A22m */
	TPACPI_Q_IBM('1', 'E', TPACPI_HK_Q_INIMASK), /* A30/p (0) */
	TPACPI_Q_IBM('1', 'C', TPACPI_HK_Q_INIMASK), /* R30 */
	TPACPI_Q_IBM('1', 'F', TPACPI_HK_Q_INIMASK), /* R31 */
	TPACPI_Q_IBM('I', 'Y', TPACPI_HK_Q_INIMASK), /* T20 */
	TPACPI_Q_IBM('K', 'Z', TPACPI_HK_Q_INIMASK), /* T21 */
	TPACPI_Q_IBM('1', '6', TPACPI_HK_Q_INIMASK), /* T22 */
	TPACPI_Q_IBM('I', 'Z', TPACPI_HK_Q_INIMASK), /* X20, X21 */
	TPACPI_Q_IBM('1', 'D', TPACPI_HK_Q_INIMASK), /* X22, X23, X24 */
};

static int __init hotkey_init(struct ibm_init_struct *iibm)
{
	/* Requirements for changing the default keymaps:
	 *
	 * 1. Many of the keys are mapped to KEY_RESERVED for very
	 *    good reasons.  Do not change them unless you have deep
	 *    knowledge on the IBM and Lenovo ThinkPad firmware for
	 *    the various ThinkPad models.  The driver behaves
	 *    differently for KEY_RESERVED: such keys have their
	 *    hot key mask *unset* in mask_recommended, and also
	 *    in the initial hot key mask programmed into the
	 *    firmware at driver load time, which means the firm-
	 *    ware may react very differently if you change them to
	 *    something else;
	 *
	 * 2. You must be subscribed to the linux-thinkpad and
	 *    ibm-acpi-devel mailing lists, and you should read the
	 *    list archives since 2007 if you want to change the
	 *    keymaps.  This requirement exists so that you will
	 *    know the past history of problems with the thinkpad-
	 *    acpi driver keymaps, and also that you will be
	 *    listening to any bug reports;
	 *
	 * 3. Do not send thinkpad-acpi specific patches directly to
	 *    for merging, *ever*.  Send them to the linux-acpi
	 *    mailinglist for comments.  Merging is to be done only
	 *    through acpi-test and the ACPI maintainer.
	 *
	 * If the above is too much to ask, don't change the keymap.
	 * Ask the thinkpad-acpi maintainer to do it, instead.
	 */
	static u16 ibm_keycode_map[] __initdata = {
		/* Scan Codes 0x00 to 0x0B: ACPI HKEY FN+F1..F12 */
		KEY_FN_F1,	KEY_FN_F2,	KEY_COFFEE,	KEY_SLEEP,
		KEY_WLAN,	KEY_FN_F6, KEY_SWITCHVIDEOMODE, KEY_FN_F8,
		KEY_FN_F9,	KEY_FN_F10,	KEY_FN_F11,	KEY_SUSPEND,

		/* Scan codes 0x0C to 0x1F: Other ACPI HKEY hot keys */
		KEY_UNKNOWN,	/* 0x0C: FN+BACKSPACE */
		KEY_UNKNOWN,	/* 0x0D: FN+INSERT */
		KEY_UNKNOWN,	/* 0x0E: FN+DELETE */

		/* brightness: firmware always reacts to them */
		KEY_RESERVED,	/* 0x0F: FN+HOME (brightness up) */
		KEY_RESERVED,	/* 0x10: FN+END (brightness down) */

		/* Thinklight: firmware always react to it */
		KEY_RESERVED,	/* 0x11: FN+PGUP (thinklight toggle) */

		KEY_UNKNOWN,	/* 0x12: FN+PGDOWN */
		KEY_ZOOM,	/* 0x13: FN+SPACE (zoom) */

		/* Volume: firmware always react to it and reprograms
		 * the built-in *extra* mixer.  Never map it to control
		 * another mixer by default. */
		KEY_RESERVED,	/* 0x14: VOLUME UP */
		KEY_RESERVED,	/* 0x15: VOLUME DOWN */
		KEY_RESERVED,	/* 0x16: MUTE */

		KEY_VENDOR,	/* 0x17: Thinkpad/AccessIBM/Lenovo */

		/* (assignments unknown, please report if found) */
		KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
		KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
	};
	static u16 lenovo_keycode_map[] __initdata = {
		/* Scan Codes 0x00 to 0x0B: ACPI HKEY FN+F1..F12 */
		KEY_FN_F1,	KEY_COFFEE,	KEY_BATTERY,	KEY_SLEEP,
		KEY_WLAN,	KEY_FN_F6, KEY_SWITCHVIDEOMODE, KEY_FN_F8,
		KEY_FN_F9,	KEY_FN_F10,	KEY_FN_F11,	KEY_SUSPEND,

		/* Scan codes 0x0C to 0x1F: Other ACPI HKEY hot keys */
		KEY_UNKNOWN,	/* 0x0C: FN+BACKSPACE */
		KEY_UNKNOWN,	/* 0x0D: FN+INSERT */
		KEY_UNKNOWN,	/* 0x0E: FN+DELETE */

		/* These should be enabled --only-- when ACPI video
		 * is disabled (i.e. in "vendor" mode), and are handled
		 * in a special way by the init code */
		KEY_BRIGHTNESSUP,	/* 0x0F: FN+HOME (brightness up) */
		KEY_BRIGHTNESSDOWN,	/* 0x10: FN+END (brightness down) */

		KEY_RESERVED,	/* 0x11: FN+PGUP (thinklight toggle) */

		KEY_UNKNOWN,	/* 0x12: FN+PGDOWN */
		KEY_ZOOM,	/* 0x13: FN+SPACE (zoom) */

		/* Volume: z60/z61, T60 (BIOS version?): firmware always
		 * react to it and reprograms the built-in *extra* mixer.
		 * Never map it to control another mixer by default.
		 *
		 * T60?, T61, R60?, R61: firmware and EC tries to send
		 * these over the regular keyboard, so these are no-ops,
		 * but there are still weird bugs re. MUTE, so do not
		 * change unless you get test reports from all Lenovo
		 * models.  May cause the BIOS to interfere with the
		 * HDA mixer.
		 */
		KEY_RESERVED,	/* 0x14: VOLUME UP */
		KEY_RESERVED,	/* 0x15: VOLUME DOWN */
		KEY_RESERVED,	/* 0x16: MUTE */

		KEY_VENDOR,	/* 0x17: Thinkpad/AccessIBM/Lenovo */

		/* (assignments unknown, please report if found) */
		KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
		KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
	};

#define TPACPI_HOTKEY_MAP_LEN		ARRAY_SIZE(ibm_keycode_map)
#define TPACPI_HOTKEY_MAP_SIZE		sizeof(ibm_keycode_map)
#define TPACPI_HOTKEY_MAP_TYPESIZE	sizeof(ibm_keycode_map[0])

	int res, i;
	int status;
	int hkeyv;

	unsigned long quirks;

	vdbg_printk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
			"initializing hotkey subdriver\n");

	BUG_ON(!tpacpi_inputdev);
	BUG_ON(tpacpi_inputdev->open != NULL ||
	       tpacpi_inputdev->close != NULL);

	TPACPI_ACPIHANDLE_INIT(hkey);
	mutex_init(&hotkey_mutex);

#ifdef CONFIG_THINKPAD_ACPI_HOTKEY_POLL
	mutex_init(&hotkey_thread_mutex);
	mutex_init(&hotkey_thread_data_mutex);
#endif

	/* hotkey not supported on 570 */
	tp_features.hotkey = hkey_handle != NULL;

	vdbg_printk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
		"hotkeys are %s\n",
		str_supported(tp_features.hotkey));

	if (!tp_features.hotkey)
		return 1;

	quirks = tpacpi_check_quirks(tpacpi_hotkey_qtable,
				     ARRAY_SIZE(tpacpi_hotkey_qtable));

	tpacpi_disable_brightness_delay();

	/* MUST have enough space for all attributes to be added to
	 * hotkey_dev_attributes */
	hotkey_dev_attributes = create_attr_set(
					ARRAY_SIZE(hotkey_attributes) + 2,
					NULL);
	if (!hotkey_dev_attributes)
		return -ENOMEM;
	res = add_many_to_attr_set(hotkey_dev_attributes,
			hotkey_attributes,
			ARRAY_SIZE(hotkey_attributes));
	if (res)
		goto err_exit;

	/* mask not supported on 600e/x, 770e, 770x, A21e, A2xm/p,
	   A30, R30, R31, T20-22, X20-21, X22-24.  Detected by checking
	   for HKEY interface version 0x100 */
	if (acpi_evalf(hkey_handle, &hkeyv, "MHKV", "qd")) {
		if ((hkeyv >> 8) != 1) {
			printk(TPACPI_ERR "unknown version of the "
			       "HKEY interface: 0x%x\n", hkeyv);
			printk(TPACPI_ERR "please report this to %s\n",
			       TPACPI_MAIL);
		} else {
			/*
			 * MHKV 0x100 in A31, R40, R40e,
			 * T4x, X31, and later
			 */
			vdbg_printk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
				"firmware HKEY interface version: 0x%x\n",
				hkeyv);

			/* Paranoia check AND init hotkey_all_mask */
			if (!acpi_evalf(hkey_handle, &hotkey_all_mask,
					"MHKA", "qd")) {
				printk(TPACPI_ERR
				       "missing MHKA handler, "
				       "please report this to %s\n",
				       TPACPI_MAIL);
				/* Fallback: pre-init for FN+F3,F4,F12 */
				hotkey_all_mask = 0x080cU;
			} else {
				tp_features.hotkey_mask = 1;
			}
		}
	}

	vdbg_printk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
		"hotkey masks are %s\n",
		str_supported(tp_features.hotkey_mask));

	/* Init hotkey_all_mask if not initialized yet */
	if (!tp_features.hotkey_mask && !hotkey_all_mask &&
	    (quirks & TPACPI_HK_Q_INIMASK))
		hotkey_all_mask = 0x080cU;  /* FN+F12, FN+F4, FN+F3 */

	/* Init hotkey_acpi_mask and hotkey_orig_mask */
	if (tp_features.hotkey_mask) {
		/* hotkey_source_mask *must* be zero for
		 * the first hotkey_mask_get to return hotkey_orig_mask */
		res = hotkey_mask_get();
		if (res)
			goto err_exit;

		hotkey_orig_mask = hotkey_acpi_mask;
	} else {
		hotkey_orig_mask = hotkey_all_mask;
		hotkey_acpi_mask = hotkey_all_mask;
	}

#ifdef CONFIG_THINKPAD_ACPI_DEBUGFACILITIES
	if (dbg_wlswemul) {
		tp_features.hotkey_wlsw = 1;
		printk(TPACPI_INFO
			"radio switch emulation enabled\n");
	} else
#endif
	/* Not all thinkpads have a hardware radio switch */
	if (acpi_evalf(hkey_handle, &status, "WLSW", "qd")) {
		tp_features.hotkey_wlsw = 1;
		printk(TPACPI_INFO
			"radio switch found; radios are %s\n",
			enabled(status, 0));
	}
	if (tp_features.hotkey_wlsw)
		res = add_to_attr_set(hotkey_dev_attributes,
				&dev_attr_hotkey_radio_sw.attr);

	/* For X41t, X60t, X61t Tablets... */
	if (!res && acpi_evalf(hkey_handle, &status, "MHKG", "qd")) {
		tp_features.hotkey_tablet = 1;
		printk(TPACPI_INFO
			"possible tablet mode switch found; "
			"ThinkPad in %s mode\n",
			(status & TP_HOTKEY_TABLET_MASK)?
				"tablet" : "laptop");
		res = add_to_attr_set(hotkey_dev_attributes,
				&dev_attr_hotkey_tablet_mode.attr);
	}

	if (!res)
		res = register_attr_set_with_sysfs(
				hotkey_dev_attributes,
				&tpacpi_pdev->dev.kobj);
	if (res)
		goto err_exit;

	/* Set up key map */

	hotkey_keycode_map = kmalloc(TPACPI_HOTKEY_MAP_SIZE,
					GFP_KERNEL);
	if (!hotkey_keycode_map) {
		printk(TPACPI_ERR
			"failed to allocate memory for key map\n");
		res = -ENOMEM;
		goto err_exit;
	}

	if (thinkpad_id.vendor == PCI_VENDOR_ID_LENOVO) {
		dbg_printk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
			   "using Lenovo default hot key map\n");
		memcpy(hotkey_keycode_map, &lenovo_keycode_map,
			TPACPI_HOTKEY_MAP_SIZE);
	} else {
		dbg_printk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
			   "using IBM default hot key map\n");
		memcpy(hotkey_keycode_map, &ibm_keycode_map,
			TPACPI_HOTKEY_MAP_SIZE);
	}

	set_bit(EV_KEY, tpacpi_inputdev->evbit);
	set_bit(EV_MSC, tpacpi_inputdev->evbit);
	set_bit(MSC_SCAN, tpacpi_inputdev->mscbit);
	tpacpi_inputdev->keycodesize = TPACPI_HOTKEY_MAP_TYPESIZE;
	tpacpi_inputdev->keycodemax = TPACPI_HOTKEY_MAP_LEN;
	tpacpi_inputdev->keycode = hotkey_keycode_map;
	for (i = 0; i < TPACPI_HOTKEY_MAP_LEN; i++) {
		if (hotkey_keycode_map[i] != KEY_RESERVED) {
			set_bit(hotkey_keycode_map[i],
				tpacpi_inputdev->keybit);
		} else {
			if (i < sizeof(hotkey_reserved_mask)*8)
				hotkey_reserved_mask |= 1 << i;
		}
	}

	if (tp_features.hotkey_wlsw) {
		set_bit(EV_SW, tpacpi_inputdev->evbit);
		set_bit(SW_RFKILL_ALL, tpacpi_inputdev->swbit);
	}
	if (tp_features.hotkey_tablet) {
		set_bit(EV_SW, tpacpi_inputdev->evbit);
		set_bit(SW_TABLET_MODE, tpacpi_inputdev->swbit);
	}

	/* Do not issue duplicate brightness change events to
	 * userspace */
	if (!tp_features.bright_acpimode)
		/* update bright_acpimode... */
		tpacpi_check_std_acpi_brightness_support();

	if (tp_features.bright_acpimode && acpi_video_backlight_support()) {
		printk(TPACPI_INFO
		       "This ThinkPad has standard ACPI backlight "
		       "brightness control, supported by the ACPI "
		       "video driver\n");
		printk(TPACPI_NOTICE
		       "Disabling thinkpad-acpi brightness events "
		       "by default...\n");

		/* Disable brightness up/down on Lenovo thinkpads when
		 * ACPI is handling them, otherwise it is plain impossible
		 * for userspace to do something even remotely sane */
		hotkey_reserved_mask |=
			(1 << TP_ACPI_HOTKEYSCAN_FNHOME)
			| (1 << TP_ACPI_HOTKEYSCAN_FNEND);
		hotkey_unmap(TP_ACPI_HOTKEYSCAN_FNHOME);
		hotkey_unmap(TP_ACPI_HOTKEYSCAN_FNEND);
	}

#ifdef CONFIG_THINKPAD_ACPI_HOTKEY_POLL
	hotkey_source_mask = TPACPI_HKEY_NVRAM_GOOD_MASK
				& ~hotkey_all_mask
				& ~hotkey_reserved_mask;

	vdbg_printk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
		    "hotkey source mask 0x%08x, polling freq %u\n",
		    hotkey_source_mask, hotkey_poll_freq);
#endif

	dbg_printk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
			"enabling firmware HKEY event interface...\n");
	res = hotkey_status_set(true);
	if (res) {
		hotkey_exit();
		return res;
	}
	res = hotkey_mask_set(((hotkey_all_mask & ~hotkey_reserved_mask)
			       | hotkey_driver_mask)
			      & ~hotkey_source_mask);
	if (res < 0 && res != -ENXIO) {
		hotkey_exit();
		return res;
	}
	hotkey_user_mask = (hotkey_acpi_mask | hotkey_source_mask)
				& ~hotkey_reserved_mask;
	vdbg_printk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
		"initial masks: user=0x%08x, fw=0x%08x, poll=0x%08x\n",
		hotkey_user_mask, hotkey_acpi_mask, hotkey_source_mask);

	dbg_printk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
			"legacy ibm/hotkey event reporting over procfs %s\n",
			(hotkey_report_mode < 2) ?
				"enabled" : "disabled");

	tpacpi_inputdev->open = &hotkey_inputdev_open;
	tpacpi_inputdev->close = &hotkey_inputdev_close;

	hotkey_poll_setup_safe(true);
	tpacpi_send_radiosw_update();
	tpacpi_input_send_tabletsw();

	return 0;

err_exit:
	delete_attr_set(hotkey_dev_attributes, &tpacpi_pdev->dev.kobj);
	hotkey_dev_attributes = NULL;

	return (res < 0)? res : 1;
}

static bool hotkey_notify_hotkey(const u32 hkey,
				 bool *send_acpi_ev,
				 bool *ignore_acpi_ev)
{
	/* 0x1000-0x1FFF: key presses */
	unsigned int scancode = hkey & 0xfff;
	*send_acpi_ev = true;
	*ignore_acpi_ev = false;

	if (scancode > 0 && scancode < 0x21) {
		scancode--;
		if (!(hotkey_source_mask & (1 << scancode))) {
			tpacpi_input_send_key_masked(scancode);
			*send_acpi_ev = false;
		} else {
			*ignore_acpi_ev = true;
		}
		return true;
	}
	return false;
}

static bool hotkey_notify_wakeup(const u32 hkey,
				 bool *send_acpi_ev,
				 bool *ignore_acpi_ev)
{
	/* 0x2000-0x2FFF: Wakeup reason */
	*send_acpi_ev = true;
	*ignore_acpi_ev = false;

	switch (hkey) {
	case TP_HKEY_EV_WKUP_S3_UNDOCK: /* suspend, undock */
	case TP_HKEY_EV_WKUP_S4_UNDOCK: /* hibernation, undock */
		hotkey_wakeup_reason = TP_ACPI_WAKEUP_UNDOCK;
		*ignore_acpi_ev = true;
		break;

	case TP_HKEY_EV_WKUP_S3_BAYEJ: /* suspend, bay eject */
	case TP_HKEY_EV_WKUP_S4_BAYEJ: /* hibernation, bay eject */
		hotkey_wakeup_reason = TP_ACPI_WAKEUP_BAYEJ;
		*ignore_acpi_ev = true;
		break;

	case TP_HKEY_EV_WKUP_S3_BATLOW: /* Battery on critical low level/S3 */
	case TP_HKEY_EV_WKUP_S4_BATLOW: /* Battery on critical low level/S4 */
		printk(TPACPI_ALERT
			"EMERGENCY WAKEUP: battery almost empty\n");
		/* how to auto-heal: */
		/* 2313: woke up from S3, go to S4/S5 */
		/* 2413: woke up from S4, go to S5 */
		break;

	default:
		return false;
	}

	if (hotkey_wakeup_reason != TP_ACPI_WAKEUP_NONE) {
		printk(TPACPI_INFO
		       "woke up due to a hot-unplug "
		       "request...\n");
		hotkey_wakeup_reason_notify_change();
	}
	return true;
}

static bool hotkey_notify_usrevent(const u32 hkey,
				 bool *send_acpi_ev,
				 bool *ignore_acpi_ev)
{
	/* 0x5000-0x5FFF: human interface helpers */
	*send_acpi_ev = true;
	*ignore_acpi_ev = false;

	switch (hkey) {
	case TP_HKEY_EV_PEN_INSERTED:  /* X61t: tablet pen inserted into bay */
	case TP_HKEY_EV_PEN_REMOVED:   /* X61t: tablet pen removed from bay */
		return true;

	case TP_HKEY_EV_TABLET_TABLET:   /* X41t-X61t: tablet mode */
	case TP_HKEY_EV_TABLET_NOTEBOOK: /* X41t-X61t: normal mode */
		tpacpi_input_send_tabletsw();
		hotkey_tablet_mode_notify_change();
		*send_acpi_ev = false;
		return true;

	case TP_HKEY_EV_LID_CLOSE:	/* Lid closed */
	case TP_HKEY_EV_LID_OPEN:	/* Lid opened */
	case TP_HKEY_EV_BRGHT_CHANGED:	/* brightness changed */
		/* do not propagate these events */
		*ignore_acpi_ev = true;
		return true;

	default:
		return false;
	}
}

static bool hotkey_notify_thermal(const u32 hkey,
				 bool *send_acpi_ev,
				 bool *ignore_acpi_ev)
{
	/* 0x6000-0x6FFF: thermal alarms */
	*send_acpi_ev = true;
	*ignore_acpi_ev = false;

	switch (hkey) {
	case TP_HKEY_EV_ALARM_BAT_HOT:
		printk(TPACPI_CRIT
			"THERMAL ALARM: battery is too hot!\n");
		/* recommended action: warn user through gui */
		return true;
	case TP_HKEY_EV_ALARM_BAT_XHOT:
		printk(TPACPI_ALERT
			"THERMAL EMERGENCY: battery is extremely hot!\n");
		/* recommended action: immediate sleep/hibernate */
		return true;
	case TP_HKEY_EV_ALARM_SENSOR_HOT:
		printk(TPACPI_CRIT
			"THERMAL ALARM: "
			"a sensor reports something is too hot!\n");
		/* recommended action: warn user through gui, that */
		/* some internal component is too hot */
		return true;
	case TP_HKEY_EV_ALARM_SENSOR_XHOT:
		printk(TPACPI_ALERT
			"THERMAL EMERGENCY: "
			"a sensor reports something is extremely hot!\n");
		/* recommended action: immediate sleep/hibernate */
		return true;
	case TP_HKEY_EV_THM_TABLE_CHANGED:
		printk(TPACPI_INFO
			"EC reports that Thermal Table has changed\n");
		/* recommended action: do nothing, we don't have
		 * Lenovo ATM information */
		return true;
	default:
		printk(TPACPI_ALERT
			 "THERMAL ALERT: unknown thermal alarm received\n");
		return false;
	}
}

static void hotkey_notify(struct ibm_struct *ibm, u32 event)
{
	u32 hkey;
	bool send_acpi_ev;
	bool ignore_acpi_ev;
	bool known_ev;

	if (event != 0x80) {
		printk(TPACPI_ERR
		       "unknown HKEY notification event %d\n", event);
		/* forward it to userspace, maybe it knows how to handle it */
		acpi_bus_generate_netlink_event(
					ibm->acpi->device->pnp.device_class,
					dev_name(&ibm->acpi->device->dev),
					event, 0);
		return;
	}

	while (1) {
		if (!acpi_evalf(hkey_handle, &hkey, "MHKP", "d")) {
			printk(TPACPI_ERR "failed to retrieve HKEY event\n");
			return;
		}

		if (hkey == 0) {
			/* queue empty */
			return;
		}

		send_acpi_ev = true;
		ignore_acpi_ev = false;

		switch (hkey >> 12) {
		case 1:
			/* 0x1000-0x1FFF: key presses */
			known_ev = hotkey_notify_hotkey(hkey, &send_acpi_ev,
						 &ignore_acpi_ev);
			break;
		case 2:
			/* 0x2000-0x2FFF: Wakeup reason */
			known_ev = hotkey_notify_wakeup(hkey, &send_acpi_ev,
						 &ignore_acpi_ev);
			break;
		case 3:
			/* 0x3000-0x3FFF: bay-related wakeups */
			if (hkey == TP_HKEY_EV_BAYEJ_ACK) {
				hotkey_autosleep_ack = 1;
				printk(TPACPI_INFO
				       "bay ejected\n");
				hotkey_wakeup_hotunplug_complete_notify_change();
				known_ev = true;
			} else {
				known_ev = false;
			}
			break;
		case 4:
			/* 0x4000-0x4FFF: dock-related wakeups */
			if (hkey == TP_HKEY_EV_UNDOCK_ACK) {
				hotkey_autosleep_ack = 1;
				printk(TPACPI_INFO
				       "undocked\n");
				hotkey_wakeup_hotunplug_complete_notify_change();
				known_ev = true;
			} else {
				known_ev = false;
			}
			break;
		case 5:
			/* 0x5000-0x5FFF: human interface helpers */
			known_ev = hotkey_notify_usrevent(hkey, &send_acpi_ev,
						 &ignore_acpi_ev);
			break;
		case 6:
			/* 0x6000-0x6FFF: thermal alarms */
			known_ev = hotkey_notify_thermal(hkey, &send_acpi_ev,
						 &ignore_acpi_ev);
			break;
		case 7:
			/* 0x7000-0x7FFF: misc */
			if (tp_features.hotkey_wlsw &&
					hkey == TP_HKEY_EV_RFKILL_CHANGED) {
				tpacpi_send_radiosw_update();
				send_acpi_ev = 0;
				known_ev = true;
				break;
			}
			/* fallthrough to default */
		default:
			known_ev = false;
		}
		if (!known_ev) {
			printk(TPACPI_NOTICE
			       "unhandled HKEY event 0x%04x\n", hkey);
			printk(TPACPI_NOTICE
			       "please report the conditions when this "
			       "event happened to %s\n", TPACPI_MAIL);
		}

		/* Legacy events */
		if (!ignore_acpi_ev &&
		    (send_acpi_ev || hotkey_report_mode < 2)) {
			acpi_bus_generate_proc_event(ibm->acpi->device,
						     event, hkey);
		}

		/* netlink events */
		if (!ignore_acpi_ev && send_acpi_ev) {
			acpi_bus_generate_netlink_event(
					ibm->acpi->device->pnp.device_class,
					dev_name(&ibm->acpi->device->dev),
					event, hkey);
		}
	}
}

static void hotkey_suspend(pm_message_t state)
{
	/* Do these on suspend, we get the events on early resume! */
	hotkey_wakeup_reason = TP_ACPI_WAKEUP_NONE;
	hotkey_autosleep_ack = 0;
}

static void hotkey_resume(void)
{
	tpacpi_disable_brightness_delay();

	if (hotkey_status_set(true) < 0 ||
	    hotkey_mask_set(hotkey_acpi_mask) < 0)
		printk(TPACPI_ERR
		       "error while attempting to reset the event "
		       "firmware interface\n");

	tpacpi_send_radiosw_update();
	hotkey_tablet_mode_notify_change();
	hotkey_wakeup_reason_notify_change();
	hotkey_wakeup_hotunplug_complete_notify_change();
	hotkey_poll_setup_safe(false);
}

/* procfs -------------------------------------------------------------- */
static int hotkey_read(char *p)
{
	int res, status;
	int len = 0;

	if (!tp_features.hotkey) {
		kpad+= sprintf(p +nkpa, "st/*
 :\t\tnot supported\n");
		returnnkpa;
	}i.c - Tmutex_lock_killable(&Extras_right))v@users.sf-ERESTARTSYS;
	res = enrique005 Bo_get(&005 Bolav@ - Thresraes Hr>
 *
 *  Tmaskogramlav@right un(C)  Henrique de Mo softwae; you cars.sfrespi.c
 *  Copyright (C) 2004-2005 Borisla%s\n", en2009d(005 Bo, 0)erms of enriqueallbute 
 *
 *
 *  Copyright (C) 2004-2ute risla0x%08x Softenriqueuser the Lav@u
 *  Copyright (C) 2004
			 that i"commandorisware F, disl,
 * reset, <ute >rislav@} elseicense, or
 *  (at your option) any latv Deianov <borislav@u
 *  Copyright (C) 2004-2will be useful,
 *  but WITHOUT ANTY; wit
ng.brs.sf.net>}

005 ic void.
 *
 * ware Fobut WI_warn(booltware F)
{
	tpacpi_log  Thitask("procfs.
 *
 *tware F/ceived slav@ - ThWARN((ral Publifecycle == TPACPI_LIFE_RUNNING || !he GNU hope   Foundte tope " with this program; i functionality has been "loor,removed from the driver.  Hxtrass are alwayse TPACPware Forislraesyrighk(  FoundERRloor,Please I_VERS.23"
Extras=ware F modulee TPACPparameter, it is deprecatede TPACPI_SYSFS_VERSION 0x020500

/*
 * ils.
 *
 *  thinenriquewrite(char *buf Genethinl Pub	u32 ute ;
	ainer
cmdpi.c - ThinkPad ACPI Extras
aes HolschuhNODEVpi.c - Tright (C) 2006-2009 Henrique de Moraes Holschuh <hmh@hmh.en
	ute >
 *
 *  T This pro	   can re0;
	while ((cmd = next_cmd(& *  )
 *
 * - Tstrlencmpcom>, 0500

/")e
 *0
 *
 *	hould have received a copy1gram thout ese MODULE_VERSION
 *gram; if hanks to Henrik Brix Andersen <brix@ge0gram entinj@-EPERMoo.org>
 *			fix parameter passOUT Amodule loading thanksersion 2 of the  | *
 *  Thource the Lsell	& ~enriqueOUT rvedentin <.org>
 *			fixscanfSION
 *0x%x", &the Li== 1 to Hen/*lives set */m a macro
 *			    thanks tChris Wright <chrisw@osdl.org>
 */

#include <lo Hen <rusty@INVALssellgoto errexitom a >
 *  Copyre; y>
#inral Pubdisclose License
 *  along with "hope " */
org>
to er version. program can redistrib This pro_set( program}

nclude :or modify
 *  it under the terms General Pubs.
 *
 *  const struct linux/evice_id ibm_htkh>
#includs[] =>
#i{  FoundFoundHKEY_HID, 0},
	{""includ};.
 *
 *  ux/proctp_linux/rv_ux/proc <linsion 2 cpi#definclude .hi>
 * <linux/sysfs.h>
#,
	.notify>
 *
 *  Th>
#insfs.handare
 &hique>
#incsfs.typcludFoundDEVICE_NOTIFY/fb.h>
#include <lin <liux/procinclude#defin_datamon.h>
namclud, Bosto>
#i.rea>
 *ase, don'adies. maininclude <l mainies.ude include <lude ies.h>sue <liase, don'tum <acpsuspen#include <l/* Thinies. <liludeh>
#include <linux/hw/fb.h>/*ne TP_CMOS_VOLUME_UP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRI
 * Bluetooth sub#defin


#i
enum>
#i/*ude < GBDC/SBDC bits

#inTPbackliBLUETOOTH_HWPRESENT	= 0x01,SS_DSS_UP	4
#dhw avai-2009OS_THINKLIGHT_ON	12
#deRADIOSSWMOS_TH2NKLIGHT_OFF	13
radiotware Fodresses */
enum tp_nvramESUMECTRLMOS_TH4NKLIGHT_OFF	13
005 e angeloume: erroe thaoff / lalinux
	TP*/fb.h>RIGHTNESS_DOWN	5\BLTH will be OS_THINKLIGHT_OTH_GET_ULTRAPORT_IDMOS_TH0, /* Get Ultraov < BT IDbit masks */
enum {
	TPWR_ON0x58,
	MOS_THINNKPAD	= power-on-clude <	TP_NVRAMmasks */
enum ,
	TP_NP_NVRAM_MASK_HK2_DISPRlude <= 0x4ed onK_HKT_HIBERNATE	= 0x80,FF	TP_NVRAM_MASK_T3NKLIGGHT	= 0x10,
	TP_ffK_HKT_HIBERNATE	= SAVE_STATE	MOS_TH5_DISPSavRAM_MASKfor S4/S5VRAM_ADD#define*  FoundRFKT_ON	12
#deSW_NAME	"ral PubbS_UP	4
#_sw" 2006-11-22	0NVRAM_MASKgetThis pr(You  			chang005 BoRIGHifdef CONFIG_THINKPADbackliDEBUGFACILITIESot, wrdbg_NVRAM_MASemul	not be upd the FreNVRAM_MASKI_AC	TP_N) ?
e that it 0,
	TP_NVRm_add_ON :ACPI_HKEY_INPUT_PROFF;
#endifi.c - Thl Pubevalf( <linux/leds  is fre, "
#de"assi*  Cha HolschuhIO <lentY_HID	undatio & es */
enum tp_nvram_addr {) !nks t#defeet, FifY_INPUT_PRODUCT	0x5054 /* "TP" */
#de  2006-11-22	0NVRAM_MASK_AM_MASK_LERIGHTral Pubrf006-This RAM_MASVOLUME	= 0x0f,
	TP	v#defngelog:
 *  20DBG_WGSILLhope"will attemptux/s%sed on */
	 Sof
		undatre
 *  FoundY_INPUT_PROD) ? *			     :ssing on mo	TP_NVRAM_POS_LEVEL_VOLUME	= 0,
};

/* ACPI HIDs */
#define TPACPI_ACPclude <linux68"

/* Input IDs *lack;

/* TP_ACPI_WGSV_GET_STATE av@users.sfdial}efine TPAC/* We make sureux/skeep		= 0x01, /* Get sta58,
	TP_N	= 0x20,
ET_STAT *  	TP_ACPI_WGSV_STATE_WWANBInot, wr;

/* TP_ACPI_WGSV_GET_STATE 
	0x0008, |/* WWAN disabled in B_addr {pi.c - ThEY_INPUT_VERSION	0x4101
NULL, "ine  comvd", 0x0f,
nds */
enum {
	TP_ACPI_WGSV0sume /* sysftate for S4tware F -	TP_ACPI_WGSV_STATE_BLTHBIOSOFF	= 0x0008, /* VRAM
#includsize_red on */
	Tware F_showMODUproc>
#inc *devhope thaTE_UWBEXIST	=_attribu_NVRe */hope thaainer
 *  			cPI_WGSVOFF_ON_RES_LTH sACPI_WGSV_STA 0,
	TP_NVRAM_MASK_MUTE	IDhope PI_WG  *  sume WWAN pod in BIOS */
	TP_ACPI_WGSVtoreATE_UWBEXIST	= 0x0010, /* UUWB hw available */
	TP_ACPI_WGSV_STATde <linainer
 * ,  in BIOcount= 0x0020, /* UWB radio enabled */
};
KEY_BHKEY events */
enum tpacpi_hkeyy_event_t ,up */
	
	/* Hotkey-rB hw available */
	TP_Adevle */R	= 0x0002, me */
=
	__ATTR(S */
	TP_ACPI_WG, S_IWUSR |_EV_RUGO */
S */
	TP_ACPI_WGSV_ST, */
	TP_HKEY_EV_HOTKEY__WGS/*
	TP_ACPI_WGSV_STATE_BLTHBIOSOFF	= 0x0008, /* BHBIOSOFF	= 0x0008, /* BLTH  up or unmute *e */
	TP_ACNVRAM_MASKe */
	TP_#include &_DOWN		= 0x1016, /* Volum.PI_WGSVradifb.h>
#inclue <linux/proc_ */
	TP__grouped on */
	TN		= S3 */
on.h>
tion, /*UNDOCK	= 0x2404, /* /fb.h>
#inclue <linux/proc UWB radio optate for S4_tpW	= 0x23on.h>
RAM_MASK_L05, /* bay ejRAM_MASK_L>


/
	TP_HKEY_EV_WKUP_S4_B, /* batte/fb.h>
#incluYou sNVRAM_MASK_hutdownEVEL_VOLUM/* Order firmwSFS_to sf,
	curre	= 0x0f3, /*NVRAM

#inTE_BLTHPWR	= 0x0radio radio e\
};

ed */
	T/
	TP_P_NVRAM_MASK_LEVEL_BR  Changelog:
 *  20killCE#inclfai_NVR /* bay ate at res complete */
	rislav@out 
	d off */
	TP_ACPI_WGSV_SAVE_STATE		"NVRAse tray eject bay, /* */

	/* Uses.
 *
 *  You sS */
	TP_ACxitEVEL_VOLUMenableI_VERSUP_S4_(&ral Pubpdev->dev.kobjlete &	TP_HKEY_EV_WKUP_S4__WGSe <linux/estroy_RESUMEhtness down */
	TP_HKEY_EV_l up ct request */
	TP_H
 *  2006-11-22	0__inired on */
	T	= 0ATE_UWBE <li	= 0.h>

#in*iibm 			changelog nE	= 0x0f,
d_acpi.c off */
	TP_ACPI_WGSV_SINIT |ACPI_HKEKEY_EV_LID_CLOSE	= 0ializing drive tray fine TP_rislavKT_Hnux/backlHANDLEEY_EVERSIOherma/* drive trayv Deianov <boP_NV570, 600e/x, 770e_HKEYx, A21eRM_Bxm/p,
 thaG4x, R30* ba1, R4Y_EVR5Y_EVT20-22, X*/
	1

#ininkPad ACPI ate at res=  <linux/led &&x6012c_fs.hPUT_VERSION	0x4101

/* ACPI \WGSV comqdTherma pen removed */
	TP_HKEY_EV_BRGHT_CHANGED	= 0x5010,E		= P	4
#dis %sTP_ACPI_sched2rsion, /* r_ianov <bo(RM_SENSOR_HOT	= 0x602n Stris free sSV_STATE_WWANEXIST	= 0x0001, /* WWAN hw available */
	TP_ACPI_WGSV_STATE_WW_SENSOR_HOT	= 0x6021, 1om angelog:
 *  20INFOCLOSE		= P	4
#dewitch I_ACa, UStware FoNTY; without efine TPKEY_E**********************o hot */!GET_STATE		= 0x01, /* Get stfine TP_C6	0.9	u/* no drive trayhard3003,pOUT nt inBLTHtem

#inc************************dial	pen removed */
	TP_HKEY_EV_BRGHT_CHANGED	= 0x5010,  it p://ibm-acpi.sf.netv Deinstalad"
#definei.c - ThinkPad ACPI ILL_CHANGEs */
enum 1 <lentinj@ral Pubnew_TABLET_NOTEBOOK	= 0x500a, /* tablVOL_UP5009, /* tatery emptVOL_UPAVE_ST_TYPET_ON	12
#dVOL_UP 0,
	TP_NVRAM_MASK_MUTE		= 0VOL_UPtruakins of the GNU General Publican reenablecreaq, S3 */P_HKEY_EV_TABLET_TABLET	= 0xx5009, /* tablet swivel us of the clude <linux/KEY_EV_TABLET_NOTEBOOK	= 0x500a, /* tablet NU General Pube for more d004, /* B  alongrom S3/S4 */
	TP_HKEY_EV_WKUP_S3_UNDOCK	= 0x2304, /* undock reLTH disablewered on */
	Tinuxtainer
p= 0x0020, /* UWB radio   alonING	TPHKEY events */
enum tpacpi_ el ume WWAN powered on */
	T maintainer
 *  			cfine TPACPI_NOTICE	KERN_N maint	TPACPI_LOG
#define TPACPI_I_t {
	/* Hotkey-rm/uaccess.h>

#inNVRAM_MASKlinux/dmi.h>
#include <linNVRAM_MASfies.h>
#incKERN_WARNING	T/workqueue.hPI_LOG
#define <acpi/acpi_OPEN		= 0x5002>


/*/
	TP_ry empty, S4 **/
	TP_0
#define TP_CMOS_VOLUME_UP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRIGHTNEWandefine TP_CMOS_BRIGHTNESS_DOWN	5
WAN/SWANTP_CMOS_THINKLIGHTWANCARDefine TP_CMOS_THINKLIGs, b
/* NVRAM Addresses */
ened(statum_addr {
	TP_NVRAM_As, b 0x57,
	TP_NVRAM_ADDR_THINsabled")
58,
	TP_NVRAM_ADDR_VIs, bi,
	TP_NVRAM_ADDR_BRIGHTNESS	= 0x5e,
	TP_NVRAM_ADDHTNESS	= 0,
	TP_NVRWWANUTE		= 0xx40,
	TP_wwanK_HKT_VOLUME	= 0xes
 RAM_MASK_LEVEL_VOLUME	= 0x0f,
	TP_NVRAM_POS_LEVEL_VOLUME	= 0,
};

/* ACPI HIDs */
#defles
I_ACPI_HKEY_HID		"IBM00les
 put IDs */
#define TPACPI_HKEY_INPUT_PRODUCT	0x5054 /* "TP" */
#define TPACPI_HKEY_INPUT_VERSION	0x4101

/* ACPI \WWAN commands */
enum {
	TP_ACPI_WGSV_GET_STATE		= 0x01,sabled")
#definermation */
	TP_ACPI_WGSV_PWR_ON_ON_RESUME	= 0x02, /* Resume WWAN powerees
 *_ACPI_WGSV_PWR_OFF_ON_RESUME	= 0x03,	/* Resume WWAN powered off */
	TP_ACPI_WGSV_SAVE_STATE		= 0x04, /* Save stales
/S5 */
};

/* TP_ACPI_WGSV_GET_STATE bits */
enum {
	TP_ACPI_WGSV_STATE_WWANEXIST	= 0x0001, /* WWAN hw available */
	 *driver;clude <linux(struct ibm_stradio enabled */
	TP_ACPI_WGSV_STATE_WWANPWRRES	= 0x0004, /* WWAN state at res */
, strlen(b)))


/*********F	= 0x0008, /* WWAN di(b)))


/*********P_ACPI_WGSV_STATE_BLTHEXIST	= 0x0001, /* BLTH hw availablsabled")
#define_STATE_BLTHPWR	= 0x0002, /* BLTH radio en*name;
/
	TP_ACPI_WGSV_STATE_BLTHPWRRES	= 0x0004, /* BLTH stwathinkpadRN_CRIT	TPACPI_LOG
#define TPACPI_ERR	KERN_ERR	TPACPILTH disabled in BIOstrucPI_WGSV_STATE_UWBEXIST	= 0x0010, /* UWB hw available */
	TP_ACPI_WGSV_STATE_UWBPWR	= 0x0020, /* UWB radio enabled */
};

/* HKEY eventsructs anpi_hkey_event_t {
	/* Hotkey-related 1;
	u32 platKEY_BASE		= 0x1001, /* first hotkey (FN+F1) */
	TP_HKEY_EV_BRGHT_UP		= 0x1010, /* Brightness up */
	TP_HKEY_EV_BRGHT_DOWN		= 0x1011, /* Brightness down 1;
	u32 sensors_pdev_att5, /* Volume up or unmute */
	TP_HKEY_EV_VOL_DOWN		= 1;
	u32 ple down or u(strucP_HKEY_EV_VOL_MUTE		= 0x10171;
	u32 platfor,2 hotkey_poll_actking up from S3/S4 */
	TP_HKEY_EV_WKUP_S3_UNDOCK	= 0x2304, /* undock requested, S3 */
	TP_HKEY_EV_WKUP_S4_es
 x2404, /* undock requested, 1.03z) */
V_WKUP_S3_BAYEJ	= 0x2305, /* bay ejection req, S3 */
b, 0 = uUP_S4_BAYEJ	= 0x2405,b, 0 = unknown, S4 */
	TP_HKEY_EV_WKUP_S3_BATLOW	= 0x23es
 tery empty, S3 */
	TP_HKEY_EVtruct;

struct2413, /* battery oid (*shutdownto-sleep after ejees
 **/
	TP_HKEY_EV_BAYEJ_ACK		= 0x3003, /* bay ejection complete */
	TP_HKEY_EV_UNDOCK_ACK		= 0x4003, /* WGSVck complete */

	/* tatu bay events */
	TP_HKEY_EV_OPTDRV_EJ		= 0x3006, /* opt. ructy ejected */

	/* User-interface events */
	TP_HKEY_EV_LID_CLOSE class thatptop lid closed */
	TP_HKEY_EV_LID_struc002, /* laptop lid opened */
	TP_HKEY_EV_TABLET_TABLET	= 0&* 9384A9C for l up */
	TP_HKEY_EV_TABLET_NOTEBOOK	= 1;
	u32 sehermae;

static inEY_EV_PEN_INSERTED		= 0xes
 ablet pen inserted */
	TP_HKEY_EV_PEN_REMOVED		= 0x500c, /* tablet pen removed */
	TP_HKEY_EV_BRGHT_CHANGED	= 0x5010, /* backlight cef:1nt */

	/* Thermal events */
	TP_HKEY_EV_ALARM_BA************ef:1, /* sensor too hot */
	TP_HKEY_EV_ALARM_SENSOR_XHOT	= 0x*name;
* sensor critically hot */
	TP_HKEY_EV_THM_TABLE_CHANGED	= 0ef:1thermal table changed */

	/* Misc */
	TP_HKEY_EV_RFKwanED	= 0x7000, /* rfkill switch changed */
};

/**********************
struct ibm_init***************************
 * Main driver
 les
ne TPACPI_NAME "thinkpad"
#define TPACPI_DESC "ThinkPad ACPI Extatiine TPACPI_FILE TPACPI_NAMEed(status, bit) (ACPI_URL "htttatipi.sf.net/"
#define TPACPI_MAIL "ibm-acpi-de******urceforge.net"

#define TPACPI_PROC_DIR "ibm"
#define TPACPIendif

static"ibm"
#define TPACPI_DRVR_NAME TPACPI_FILk(TPTPACPI_DRVR_SHORTNAME "tpacpi"
#define TPACPI_HWMO1;
	u32 sensorsdef CO

#define TPACPI_NVRAM_KTHRruct"ktpacpi_nvramd"
ructs and miRKQUEUE_NAME "ktpacpid"

#define TPACPI_MAX_ACPI_ARGS 3

/* printk headers */
#define TPACPI_f CONFIG_THINKPAD_Afine TPACPI_EMERG	KERN_EMERG	TPACPI_LOG
#defing_wlswemul;
sN_ALERT	TPACPI_LOG
#define TPACPI_CRIT	KERN_CRIT	TPACPI_LOG
#define TPACPI_ERR	KERN_ERR	TPACPI_LOG
#define TPACPI_WARN	es
 NG	TPACPI_LOG
#define TPACPI_NOTICE	KERN_NOTICE	TPACPI_LOG1;
	u32 senINFO	KERN_INFO	TPACes
 fine TPACPI_DEBUG	KERN_DEBUG	TPACPI_LOG

/* Debugging printk gecials.
 */
TPACPI_DBG_ALL		0xffff
#define TPAes
 linux/dmi.h>
#include <linwa */

.h>
#inc.e. base/workqueue.hTCH_ANY		<acpi/acpi_work;
	e4
#define TPACPe;

static i0
#define TP_CMOS_VOLUME_UP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRIGHTNEUWBdefine TP_CMOS_BRIGHTNESS_DOWN	5
UWB/SPI_QP_CMOS_THINKLIGHTUWBefine TP_CMOS_THINKLIGPI_Q
/* NVRAM Addresses */
enENOVm_addr {
	TP_NVRAM_API_Q 0x57,
	TP_NVRAM_*****
 * Driver-wide stENOVTE		= 0x40,
	TP_uwbK_HKT_VOLUME	= 0x
	u1RAM_MASK_LEVEL_VOLUME	= 0x0f,
	TP_NVRAM_POS_LEVEL_VOLUME	= 0,
};

/* ACPI HIDs */
#defuwbI_ACPI_HKEY_HID		"IBM00
	u1put IDs */
#define TPACPI_HKEY_INPUT_PRODUCT	0x5054 /* "TP" */
#define TPACPI_HKEY_INPUT_VERSION	0x4101

/* ACPI \WUWB commands */
enum {
	TP_ACPI_WGSV_GET_STATE		= 0x01,ACPI_MATCH_Armation */
	TP_ACPI_WGSV_PWR_ON_ON_RESUME	= 0x02, /* Resume WWAN powere
	u16_ACPI_WGSV_PWR_OFF_ON_RESUME	= 0x03,	/* Resume WWAN powered off */
	TP_ACPI_WGSV_SAVE_STATE		= 0x04, /* Save staUWB/S5 */
};

/* TP_ACPI_WGSV_GET_STATE bits */
enum {
	TP_ACPI_WGSV_STATE_WWANEXIST	= 0x0001, /* WWAN hw available */
	t
 * @qlclude <linuxruct tpacpi_qradio enabled */
	TP_ACPI_WGSV_STATE_WWANPWRRES	= 0x0004, /CPI_WGSV_STATE_BLTHEXIST	= 0x0001, /* BLTH /* WWAN diACPI_MATCH_Aser-interf00c, /* tabletTE_BLTHPWR	= 0x0002, /* BLTH radio enat matc/
	TP_ACPI_WGSV_STATE_BLTHPWRRES	= 0x0004, /* B from S3/S4 */
	TP_HKEY_EV_WKUP_S3_UNDOCK	= 0x2304, /* undock requested, S3 */
	TKEY_EV_WKUP_S3_BATLOW	= 0x23
	u1tery empty, S3 */
	TP_HKEY_EV
	unsigned lon2413, /* battery atching &structo-sleep after ejeruct 002, /* laptoMERG	KERN_EMERG	TPACPI_LOG
#definunsigneul;
sEV_PEN_INSERTED		= 0x
	u1ablet pen inserted */
	TP_HKEY_EV_PEN_REMOVED		= 0x500c, /* tablet pen removed */
	TP_HKEY_EV_BRGHT_CHANGED	= 0x5010, /* backlight cuwbbemul;
static int tpacpi_uwb_emulstate;
#endif


/****************, /* sensor too hot */
	TP_HKEY_EV_ALARM_SENSOR_XHOT	= 0xat matc* sensor critically hot */
	TP_HKEY_EV_THM_TABLE_CHANGED	= 0****thermal table changed */

	/* Misc */
	TP_HKEY_EV_RFKuwbED	= 0x7000, /* rfkill switch changed */
};

/**********************{
		if ((qlist-****
 * ACPI bas***********
 * Main driver
 ***** TPACPI_NAME "thinkpad"
#define TPACPI_DESC "ThinkPad ACPI Ex****ine TPACPI_FILE TPACPI_NAMEENOVO,	\
	  .ACPI_URL "htt****pi.sf.net/"
#define TPACPI_MAIL "ibm-acpi-dePI basurceforge.net"

#define TPACPI_hope thac, ro %d\n",
		what, task_tgid_vnr(current));
}

#defin = &TPACPI_DRVR_SHORTNAME "tpacpi"
#define TPACPI_HWMO*********(unlike************* TPACPI_NVRAM_KTHRUWB"ktpacpi_nvramd"
unsigned in(unlikfalsAME "linux/nvram.h>
#includ0xffff
#define TPA
	u1linux/dmi.h>
#include <linuwbPI_Q_i/acpi_*******
I_Q_flags.experimental] = 0
#define TP_CMOS_VOLUME_UP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRIGHTNEVideodefine TP_CMOS_B_NVRAM_POS_LEVEL_VOLUME	= 0,VIDEODDR_MIXvANDL_access_modh>
#il event70x *_NONET20-,1 */
	   "HKEY" hotRL "570K_HKT_H
	   "HKEY"7;			/* */
	TP_HKEY_EV_ALA */

TPACPI_HANDLENEW		/* all ACPIr bit _ADDR_MIXERL "^HKEYl table * A3x, basry too/
	   );	K_HKT_HIBERNA"HKEY"S_LCDT20-THINKLIG.VIDoutput,
	TP_NVRAM_ADDR_THIN_SB.PCI0CRTD",	/*VRAM_A.AGP, R50e, X30, X40 */
	   "\\_SB.PCI0DVID",	/*8		/* 
/**, R50e, X30, X40 *_ADDR_MIXE NKPAthers */
	   );	******anCMOS_THINKLIGHT/
	   );	_PHSCMID",	/87		/* unkn TPAmag********ant :( ACPI helpers
 */

static MASKD",	/*_MASK_PHS = PCIthae <lp toR_BRIGHT/x, 770This pro* A3x ACPI helpers
 */

static 2int acpi_bvalf(acpi_handle handle,
		      void *res, char *method2SEGP.VID8			/* acpi_handle handle,
		      vb.h>
#inclu	   "^HKEY",		/* R30, R acpi_oanov <bo;TPACPI_WARN	^HKEY"orig_autosw.h>
#incluet;

	if () {
		ogramVEL_V
	int quiet;

	if (cpi_eva <lithinhe GNU ;

TPACPI_
	TP_H(vid2, root, /* _SB.PCI0.AGPB.VID");RL "G4EV_AL_PEN_INSERTED		= 0x^HKEY"ablet pen inserted */
	TP_HKEY_EV_PEN_REMOivgablet pen removed */
	TP_HKEY_EV, /* backlight c, 770x t */

	/* Thermal events */
	TP_HKEY_EVvalledh (c) {
		case 'd':	/* int 2_tgid_vnr.intensor too /
	TP_HKEY_Eradio &bjs[, /* IVGA commanap, bjs[   "quiet , asude <nt++ doesn'010, ngddress	vidensor to=r c  va_arg(pi.c - Thas needed YPE_INT, 770x  TPACht c1, /* battery toobattery types as int success; ||
	
	   "HKEY",		/ser-int			fi
	TP_HKEY_Eas needed , &
	if (!*fmt) {
		ght_aITPACPI_HAYPE_INT 770e */at character '%c'\n", c);
			ret57dial		}
	}
	va_end(ap);

	if (res_type != 'v') {
		result^VADLgth = sizeof(ouot, "\\_SB.PCI.AGP.VGA"at character '%c'\n", c);
			ret7sultp = &PE_INTB.PCI0.AGP0.VIresultp);

	switch (res_type) {
	NEus:1;
	va_start(ap, fmt);
	while (*, 770xthermal30, R%"
#d/

	/* Misc */
	TPt character '%c'! *  Found"HKEY",		/n Strt character '%chermaPI_WGSV_reak;
	case 'v':		/* void */
		succes? 0 := { s.
 *
 *  You st char***
 *
 * ACPIoff */
	TP_ACPI_WGSV_SEX0.AD4SEC0"r 0x5rht c!*fminali_evalf) {
		TPACP30, rislav@value =rmat\n");
		re
	if (!*fmt) {
		  Changelog:
 *  2007- "error .pipextryht ctogeloKEY_'%c'\n", r TPACPes_type);
		return 0;
	}

	  2006-11-22	0
	if (! R50eevalf() callOLUME	= 0x0f,
T20-22,in_oberma		retur	/* add more typ((qlichang&out_obj;
		resuDR_BEY_EV_UNDOCK_ACK		= 0x&it = 1;
		fHS commmplete 	tatic str
 */

static intizeof*/
enum {
	TP_ATCH_ANY) &iTE		= 0x01, char *method, chtatibreas in dle, &v, NULL, "d7", i))
			return 0;
		*p = v;
	} elVCaluate sizeofturn 1;
}

stati - Tit err* BLTH hw availabl_SB.PCI0.VIe {
		ifandle, NULL, NULL, "vdd", i,V commands *rn 0;
	} else {
		if (ec_write(i, v) < 0)
			return CRT (ecwr_handle) {
		if (!acpi_eNEW i))
			return 0;
		*p = v4003, /* VUP	if (/
	TP1) ||define urn 1;
}

static int issue_thinkpad_cmos_command(int cmos_cmd)
{
	if (!cmos_handle)
		return -ENXe, NULL, NULL, "vd", cmos_cmd))
		return -EIO;
0	return 0;
}

/************************ v))
			return 0;
	} else {
		if (ec_write(i, v) < 0)
			return 0;
	}

	return 1;
}

static int issue_tDinkpad_cmos_command(int cmos_cmd)
{
	if (!cmos_handle)
		returnDVI (ecwr_handldefaulnclut be updatedmh.eng for more d0x0f,
	T

static int acpi_ec_read(int		return_ACPI_W			chang) {
		prchangelod_acpi.c {
		if (!acpi_evalf(ecrd_handle, &v, NULL, "dd", i)) "tryi_UNDOCK_ACK		= 0x4003,ead(i, else {
		2rn -EIc_read(i, p) < 0)
			return 0; in_atus = * BLTH haths[i], handle);
		if SETk hanwr_handle) {
		if (!acpi_evalf(ec) {
		*/
		d "acpi_evalf()k han}
	va "Foun< 0cmos_command) {
		pri0; i < nsuccess && !quietntoo.o of the GNU_ALERT	TPACPI_; i < num_paths; i
	if (res_ty		status = aASWngth (pareCESS(sta* 0x1THINRussel	retur "Foun&&

	vdbg_printk(TPATPACPI_E>
#incngelog:
 *  2007-10-2that it es_type);
-		returlefe, X30, X4du3, /* ...)rislav@uturn 1;
}

stati}XIO;

	if (!acpi_evalf(cmos_handle,  i < num_paths; i++) {
		staLE_INIT(object) \
x80 = Adefine TP NULL, "vd", cmos_cmd))
		rSDurn -EIatic void ;

	handle parent,
			   char **paths, int num_paths, c the lt:
		{
	TP_

static int acpi_eccpi_evalf() callintk(TPACPI_DBrying to locate ACPI handle for %s\n",
		name);

	for (i = 0;	return 1;
}

st
	if (res_typ
		result.length ad_cmos_command(int cmowr_handle) {
		if (!acpi_evalf(e!acpi_evalf(cmos_handle, NULL, NULL, "vd"device(*ibm->acpi->han^VDEE &ibm->acpi->device);
	if (rc < 0) t,
			   char **paths, int num_paths, cacpi_han	printk(TPACPImpty format\n");
		return 0;
	}
			chled: %d\n",
			ibm->name, rradio e_DOurn -EIO;
(he GNU ? 1 :; eis */
enum {
	TP_
#define TPACPtatic int acpi_ec_read(intftwarEVEL_VOLUME	=   "Found ACPI handle %s for %shangelog IX,
						   *path, ame);
			return;
 locate ACPI handle for %s\n",
		name);

	for (i = 0; i < n
	vdbg_printk(TPACPI_DBG_INIT, "ACPI handle for %s not found\n",
	ectatus = acpi_instQ16
}

sslav@urc < 0) {
		printk(TPACPI_ERR "acpi_bus_get_device(%s) fa	}
	}

	vdbg_printk(TPACPI_DBG_INIT, "ACPI handle for %s not found\n",
		    name);
	*han "VLL;
}

shandler(%s) fat,
			   char **paths, int numotify(acpi_handle handle, u32 event, void *datngelog:
 *  2007-10-t *ibm = data;

	if (tpacpi_lifecycle != TPACPI_LIFE_RUNNINandle,
			ibm-ON(!ibm->acpi);

	if (!*ibm->acpi->handle)
		retexpand_toggif (ACPI_FAI locate ACPI handle for %s\n",
		name);

	for (i = 0; ie_clasI_ERR
			       "acpi_install_n7i_devic*/
	Tif (!*ibm-le) {
		if (!acpi_evalf(ecif (!ibm->acpi->drstatic int __init tpEXPI_ERR
		       "failed to allocate memory
		}
		re (!ibm->acpi->dr, cmos_cmd))
		r
	}

	sprintf(ibm->acpi->d acpi_device *device)
{
	returnL "htngelach*******, dispatch_acpi_notNG	TPACPI_LOG
#de;

	vdbg_p,ACPI_DBG_INIT,kpad_acpi.c - Tt character '%c'\	/* void */
		succes *
 *
 *  Copyright (C) 2004-2005 Borislav Deianov <borislav@users.sf.net>
 *   v;

	if cpi_ec_read(int i, 
#define v;

	iprintk(TPACPI_AN powered(status)) {
		if (status == AE_STS) {
			printk(TPACPI_NOTICE
			
 *  Copyright (C) 2004-2005 Borislaianov <borislav@
 *  Copyright (C) 2004-2lcdhe Free Software Foundation; eitheFOR A PARTICULAR PURPOSE. rthe Free Software Foundation;1either veintk(TPACPI_ERR "acpi_bus_registeEWi->h
 *  Copyright (C) 2004-2dvihe Free Software Foundation;3***************
 *
 * Procfs Hacpihe Free Software Fou
		resul****************
 *
 * Procfs Heill be uselcd3z) */
	cnt cceived *
 *****************************t off,
			icrt3z) */
	c	intof, void *data**************************************************
 ****************t off,
			idvi3z) */
	cif (of, void *data)
{
	struct ibm_struct *ibm = data;
acpi3z) */
	coff;
of, void *data)
{
	struct ibm_struct *ibm = data;
t chara	retu, i);

	ibm->ac* Therma more details.
 *
 *  nt acpi_ec maintainer
 *  			c git commitr(ibmful,
 *  but WITH0x0f,
	TE_ALREADY_EXISTSintk(TPACPI_ERR "acpi_bus_register_drnot be updated furthe Volume 0-22,  02110-_acpi.c.pipex.com>
 *
 *  2005-01-16	0.9	use MODULE_VERSION
 *nt count, module loading Volume v) < 0)
			return 0;
	}

rg>
 *			fix parameter passt *eof, voimodule loading  02110-_KERNEL);
	if (!kernbuf)
		return -ENOMEM;

	if (copy	int len;
malloc(count + 2, GFP_KERNEL);
	if (!ker -ENXIOreturn -EFAULT;
	}

	kernbuf[com_user(kernbuf, userbuf, count)) {
		kfree(kernbrite(kernbuf);
	if***********************************
}

st/* UWB arameter passif (len <=malloc(count + 2, GFP_KERNEL);
	if (!kerpi_handnext_cmd(char **cmds)
{
	char *start = *cmds;
	char *end;

	while ((end = stm_user(kernbuf, userbuf, count)) {
		kfree(kernbnd + 1;

	if (!end;

	while ((end off;
	if (lmodule loading"handling %s events\n", ibm->BG_INIT, "ACPPI handle for %***********************
 *******m_user(kernbuf, userb"handling %s events\n",Russell*******************
 *
 * Device model: input, hwmon aurn len;
}

 *
 **********************ify, ibm);
	if *************************
 *
 * Device model: input, hwmon ai);

	ibm->ac *
 **********************i);

	ibm->acp*************************
 *
 * Device NING)
		returnux/type_DRVR_NA 2, GFP_|  but WIriver(ibm->acpi->driver = NULL;
	} els	    (qlic)
		ibm->>flags.acpi_driv*********************** <liGET_STATE	~struct mu|n 0;
	}

	DBG_INIT, "ACPI handle for LOG
#define TPACPt, "\\UCMS",	/* R50, R50eACPI hlinux/dmi.h>
#include <linACPI PI_Q_IBM(__icpi_bus_re/workqueue.he,
			constX40 */
	   "acpi_eval/fb.h>fine TNKPA",	/* 600e/x, 770e, 770x *= 0;
ine TP_CMOS_VOLUME_UP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRIGHTNELight (thinkl*ibm)define TP_CMOS_Bf (*fmt == 'q'lght		quiet = 1LGHTe
		qui_BAT_XHOT	= 0t */
	TP_HKEY_EV_ALy_safe(ibm, itmedb, ec, "LEDBe
		_INTEGP.VGA dispatch_ac;

	lnsigned long quirks;
};

/**
 *d_acpi.c - Tobject##_patvoid t_ACPI_W0.9	use MLTHPWR	= 0x0      "acpi
/* ACPI \KBL, &ibm->acpi->device);
	if (PI_WGSV_!!is free so for more d-ENXibm->acpi->handle)
	strucAM_MASK_LE;

	vdbg_printk(TPArcform_device *pdev)
{
	str_struct *icmosACPI_ERR>
#inclc< num_paths; i {
	.driver acpi_inradio eomplete /
};

/us		    te */
CMOSVEL_VOLIGHTRODUCnd = tpacpi_suspend_handlerFFtoo.org>
 *	 {
		.name = TPACPI_Dmp,
NAME,
		.owner = THIS_MODULE,
	},
	.suspebm->acp

	if (!ibm->acpic

	if (!*ibm-_drivers) {
		if (ibm->shutdowYou s		(ibm->shutdown_workerATE_UWBELE,
/
	TP_HKELE,
river WKUP_S3_BATLOled_classdev *mi.h>
cpi_container_of(LE,
 voi************************,*****_tgid_vnrlikely the Free Software
 *  Foundation, Inc.,pi->h		(ibm->shutdown)(mi.h->i"
#******		/* void LEDandle/
	TP_HKEY_EV_LID_
	strucnable <lifs supp*******************x0010, /RIGHT****br*ibmness tatic stru******************************************************
ribute__((pacthat fs support helpers
 */

strope that ************wmonup;
};

struct a= (tatic struc!= et_obj { */
	T
TPACPI_et_obDUCT	0x5054et_obj ;
	queueULE,
notify) (q, &up;
};ct attr ibm_strucked));

static struc_set s;
	strgramattribute *a;
} __attribute__UG	KERN_DEBU(void tpacpi_shutdht <chr?ates FULL :ates spa, root, "\\UCMS",	/********************************itmp;

	ly, S3 **************lude clude IGHT40,
	TP::*itmp;

	lI_TYP.tatic struruct	= &_set s;
	structset(_set) \
	kfrge(_set);

/* not mglti-t}status;
	va_ERTED		= 0xvoid tablet pen inserted */
	TP_HKEY_EV_PEN_REMOVuct p
	va_start(ap, fmt);
	while (*fmt) {
		charoup.na(fmt++);
		switch (c) {
		case 'd':	/* i(ibm*/
			in_objs[params.count]mp,
*/
			in_objs[params.count] {
	wmonY_EV_WORKP_HKEY_EVobj->s.group.n. * sysowner = THIS_MODULE,
	}RM_BAT_Houp.na1, /* battery too hot */
	TP_HKEY_EV_ALARM, /* battery V_ALARM_SENSOR_HOoup.nameDRVR_NAME,
	tic handler,
}; = AC!(ibmfault:
			printorm_driver tpacpi_PE_INTNVAL;

T_HEAD1, /* battery tostructr,
			unsigned int count)
{
	int i, re* baTP_HKE_MAIL "ibm-acpi-de
	struct ibm******m->acpi->driver) {
		printk(Tsafe(ibmqvice_ibute *attr)
{
	if (!s || !attr)oup.nathermal
	return 0;
}thermd */

	/* Misc */
	TP_HKEY_EV_RFK;

	lioup(_kobj, &_attr_set->group)

statuct ibm__tgid_vnrhinkPad ACPI urn res;
CPI_DRVR_SHORname************_registerP_HKEY_EV_TABLET_Tatus =   
static int add_many_to	if (max_memberid_vnr(c
		ib    *object##_patoup.nameurcef
{
	sysfs_remove_group(kobRRES	=hout eown =r *enRES	= _HANDLE(cmc	struct attribute_set s***
 *
 * ACPIdp;

	while (un*buf && isspace(*imple_strtoul(buf, &endp, 0);
**********Thining

static int add_many_to_attpi->hflushULE,
obj =loc(sizeof;
}

static int a *buf,s_register_driver(ibmkpad_acpime WWAN poweredlong max, unsigned long *
 *
 *  Copyright (C) 2004-2005 Borislav Deianov <borislav@rg>
 *			fi max, unsigned lonuct ibm_struc
 *  Copyright (C) 2004-2005 Borislaacpi_haFITNESS FOR A PARTICULAR PURPOSE.  See the
 on,	= 0NTY; without even truct plabj)
		return NULL;tatic LIST_HEAD(tpacpi_all_drivers);

stc License as published by
 *  the Free Softonoffundation; eitheion acpi_object *)buffer.pointer;
		if (!obj || (objprocfs_write(struct file *filSS(acpconst char __user *userbuf,
			unsignewtruct platform_dev max, unsigned long *value)
{ated furthe.pipex.com>
 *
 *  2005-01-16	0.9	use MODULE_VERSION
 *onmodule loadingtus __init t*****rg>
 *			fix parameter passoffH_SEGMENT_LENGTH];
	structurcefic struct input_dev *tpacpi_inp more de	(ibm->shutdown)tus __iniolume up or unmute *ess.h>

#in, &buflinux/dmi.h>
#include <lin_attr_set.h>
#inct tpacpi_q/workqueue.hr.pointer);X40 */
	   sable_brig0
#define TP_CMOS_VOLUME_UP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRIGHTNEi_sudefine TP_CMOS_B* BLTH st {
	.t off,
st->quirks;

		qlist_size--;
		qlist++;
	}
	return LTH disabled in BIO/
static intll_active:1;
} tp_features;

static struct {
	u16 hotkey_mask_ff:1;
} tp_warned;

struct thinkpad_id_data unsigned long
	int b
			unsigEADY_EXISTSparsoll_rtoul(PCI_V21, & 0;

	/*pi->handle,
			ux/typPI_MAX_Aissue->s.grpad_/
static intDRVR_N Thibm->acpi->pi);

	 "tr:up */
, root, "\\UCMS",	//
	TP_HKEY_EV_VOL_DOWN		= /
static int  down or u/
static intY_EV_VOL_r = THIS	int bcl_levels = king up from S3/S4 */
	TP_HKEY_EV_WKUP_S3_UNDOCK	= 0x2304, /* undock requested, S3 */
	TERTED		= 0x {
	.ablet pen inserted */
	TP_HKEY_EV_PEN_REMOVED		ibute *attr)
{
	if (!s || !att	= 0* backlight c {
	/* NVRAM b(fmt++);
		switch (c) {
		case 'd':	/* i++;

	ribute *attr)
{
	if (!s || !attr) bcl_levels > SFS_te_group(_kobj, &_attr_; i++) {
		r!= = THigned  "tryi>
#inclGS 3

/fi09 Hspace(*buf))
		bu equested, /
static intME "ktpacpid"

#define TPACPI_Ma Leno {
	.driver lt:
		printk(TPACPI_ERR  {
	.evalf() called 
#inclopened  * const details)
{
	tpacpi_log_usertask("depreca_add;

	rc = a {
	.pi_query_bcl_levels(acpi_handladd = bcl_1, /* battery too hot */
	TP_HKEY_EV_ALARM_BAT_XHOT	= 0x6012battery cr */
	TP_HKEY_EV_ALA,
		i {
	.driver 
 *
 *  Copyright (C) 2004-2005 Borislav Deianov <borislav@out even the implied warranty of
 *********************
 **** FOR A PARTICULAR PURPOSE.  See the
 <cmd> (t overis EY_E)} else {
		return 0;
	}

	kfree(buffe {
	.const char __user *userbuf,
			unsigCL will THOUT					u32 lvl, void *context, void **rv)
{
	ckernel.h>
#inuris CL will pt <ch
}

staticCL will  >= 0andltime ther<= 2hrisw@osdl.time ther */

#include <uct input_dev *tpacp
static ids, and as a side-effect, _BCL will placdev,
				  pm_message_t state)
{
	struct ibm_struct *ibm, *itmp;

	l {
	.linux/dmi.h>
#include <lin {
	t **)rv = tp*********/workqueue.ha masked-oX40 */
	   s is depr0
#define TP_CMOS_VOLUME_UP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRIGHTNELEDdefine TP_CMOS_BRIGHT****,		/* R30, R31 */
	   et_o,		/* all others *et_o);			/* 570 */

TPACPI_et_obLDd, root, "\\_SB.PCI.AGPRM_BAT_XHOT	= 0CPI firmware handlinB_SW_ID,
	TP  "\\_SB.PCI0.AGP0.VID0",	/* 600e/xFor the end too LM_MASK_Htpacpi_rfEC_HLCLD",	/*c		/* EC ren",
	gebute ",
	= 0x4P_NVRAM_MAPACPI_RFK_SW_MBX] = NUdL
};

/* ThinkPblink a lnt aNVRAM_ADDPACPI_RFK_SW_MMSD",	/*eL
};

/* ThinkPseleibute ",
	tic int s status;
	va_list _ID,
	TPACPI_RFK**** success;
	bm->resume)
			(ib->resumSLED"		/* 570 */

EC0"SYSL",t(handle, method, &paraRFK_BLUETOOTH_SWres)
}

	r */
	TP_HKEY_EV_ALAEC0"status* int */
		if (res)   
	}
ASK_Ht i, res;

**
 * Driver-wid,
	TPUMLEDS 16mbers;
	sobj->s.group.attrs = &sobj*f(hkey_hans
	int qui/* firmwaIS_MODU>s.group.attr*****_cpi_d[r a given rfkill s]
	int qui 0x1010, /* 
	statuf(hkey_handude s(!tp_rfk)
		return -n &sob/*.23"re'sl;
	emit of 1910, /s + = TH bef    2.6.26_rfk define de= 0x4I_TYdefine dorore :batr_setdefine dgreen));

	return stat:dC) 2activeQuery FW and bade <rfkill sw state fupdat
/* Query FW and acpi_ha_hanQuery FW and e,
	dbffiesswitches */
strn 0;
1	unsigned int i;

	for (2 void tpacpi_rfk_update_MAX; i++)
		tpacpi_rfk_up3	unsigned int*itmpvantagill };**
 * Driver-widSAFEven S	patc81U0;
}

statilSS	= of tral Pubis_hand	   ricttrib <linndle)
		rthinkpprint_NVRAM_POS_LEVEL_VOLUME	= 0,UNtate of aNGLE_NAME
TPACdefiff hointk(TPA1U & ing printate of a >>eventodule define TP

	kfree(bufferednsigned longcore to schedule uevents
 ME	= 0x0f,
	Tcked));

sk *tp_rfk
		if
			       " */
	int (*gerd_handle, &v, NU;

stat
	rc = acpi_bus_get_      "acpunlike/* ACPI \Wstatu (ec_r 1 <<rfk *tNING)
		return;

	i
		ifradio en(kobnks  for implicit NULLFF
	.resuhandler(s	sobj-nd = tmplicit NULL at/* Call to query BLINKrintk)
{
	int status;

	if (led)
		) {
			@users.sf.n_rfk_ct,
			   char **pathsf (ib {
		d = &tpacpi_device_add;

	rc = alock_FK_SW_MAX; i++) {
		tp_rfk = tpaj *sobjcore ts[i];
		if (tp_rfk) {vdbg_printk/*	= 0CPI_e */ink. Indexall 
		if (tp_rfk	= 0x000;

	statuo schedule ueven_s_ID,
rg1includ 0, 1, 3 };_RFK_RADIO_OFF);
	tpacpi_rfk_updte_hwblock_state(hon ac 0xc0ocketruct atrying to locate _set_hw_state(tp_rfk->rfkill,
						block(out_obj);
	}


unset {
	};

> 7 Call to get theux/typesdbg_printk(TPice it causes the rfkill fk *tCall to get thrustcorp,
		ibm->name);
_updAME,
		.owner = THIS_MOODULE,
	(	}
}

/* ,k_update_hwblocf (res < ]ed ? "b*dat);
	if (rc < 0) {
		printk(T] = "uwta;
	ines[] = {
	[TPACPI_RFK_BLUETOOTH_SW_ID] = "blues;

	dbg_printk(TPACPI_DBG_RFKILL,
		   "request to change radio state to %s\n",
		   blocked ? "blocked" : "unbloRFK_RecDebugging prin id;
	const, fk->ops->se *pdev,
	cre arACPI_RFK_Rrfk_rfkill_ops = {
	.set_Btatus = l(cony towlsw(void) and update all r}
}

/* rfk_hook_set_block,
};

static int __init tpacpi_nCw_rfkill(const enum tpacttribute_set_obj {st struct tp : TPACPI_RFK_RADIO_ON);andle,  int */
		if (res)
dbg_printk(TPACPI_i_rfk_id id,
fkill sG_RFKILL,
		   "request to change radio state to %s\n",
		   blocked ? "blocked" : "unblocked");

	/* try to set radio state */
	res = (tp_r	int 
static int ted ?
				TPACPI_RFK_RADIO_OFF : TPACPt,
			   chaFK_RAD= hotkey_geng.h>
cCalll switches */
static bool tpacpi_AN powered;
}

static void tpacpi_dishen unknown oULE,
	},
};

/************************************************************************
 * sysfs support helpers
 */

struct attribute_set {
	unsigned int members, max_members;
	struct athen unknown orup;
};	int up;
};

struct 
	struct attribute_upda
	struct attribute *a;
} __attribute__((packed));

static struct attribute_set *create_attr_set(unsigned int maxume rs,
						const char *name)
{
	struct attribute_set_obj
	if (max_member		&tpatatic struc=cates spaccefo== 0)
		return NUthe end too */
	s		}
	}
	v)
{
	int status;

	if () {
		pri]har *name,
			c all racross S5 in NVRAM */
			rfkill_iNser-interfoss S5 in NVRAM */
			rfkill_ all 			 obj = kzalloc(sizeof(struct attribute_set_obj)	/* When nable;

	hruct attribute *a;
} __attribute__((pacndle)
		return*delay_alse, res);
		rfkill_destrf  			ctatus == TPACPI_RFK_RADIO_OFF);
		if (set_default) {
			/* try to keep the initial state, since we ask the
			 d)(san we choostrimmeflas	= 0te?ndling moll_destro firmandll);
		kfrehanks to He/* yes.n
 * CPI_",
	immedi.sf.netrfkilld)
{ck =Hz)_rfk *pacpi_rfk * 500;NKPAm (res)


	BUG_ON(id{
		rfkill_unregrg>
 *			fitpacpi_rfk !->rfk	ret tpacpi_rfffkill);
	s is safe on all
	 * Ttate());

	res = rfkill_register(atprfk->rfkill);
	if (res < 0) {
		printm->acpi->type, dispatc +
		    max_members 	"failed ruct attribute *),
		    GFP_KERNEL);
c struct ptatus == TPACPI_RFK_RADIO_OFF);
		if (set_default) {
			/* try to keep the initial state, since we ask the
			 r *endp;
RAM_MASK_LE) {
		pri);
	while (*pi_rfk_id id,
 hot||d *dprintk(T*datrs = maxRL "htt ...) >
#inht ciwbloc *****      vooff hottribute *x_me * const wh_rfk->rfkill) {
		print***
 *
 * ACPIo schedule uendle)AM_P(efin0; i <e;
	int sw_status;
; i++_struct *iwstate(cons[i]buf, &endp, 0lude Call ay(void)
{
	if (acpi_evalf(hkey_hanDIO_OFF;
	} else all_drivkfreled\n");
cpi_************************ice it cd */ Fou{
		tp_rfk = tpacpi_rfkilattribate(tpacpi_r}
	}buf,pacpi_*******LEDs wi6011, ude <do addad-A*buf && NVRAM_A - Thins->get_status)(}
	}
	int ret;
*****s == TPACPI_RFK_RADI
	} else {set) \
	kfree(ludenitial state ;    struct device_attribute *attro registt char *buf, st res;

	 0))
		 */
	int (*geid id,
					   57lock,  struct device_attribute *attr,
			    cad-A*****likee(what,
			"P up */
	TP_t device_attribute *attude <lire(const enum tpacpi_ * cturn 0;
}

static intI_RFK_R_attr_sintk(TPACPI_ERR
			"_rfk_sysfs_ena
	while (*buf && isspace(*buf))
		buf++;
i_rfk_check_hwblocPWMS", "qvd", 0))
		 device_attus((!!t) ?
				TPACPI_RFK_RA);

	/* radil_attribute(attr->attr.naKEY_EV_WKUP_S3_BATLOquirkte() useful_qt2009[]ED		= 0core tlude _rfk_idQ_IBM('1', 'E'k_se009f)_DISPA30 */

TPACPI_------------N-----------------ooth",
	[TPAC------------Gt tpacpi_rfk_procfs_re---------------------I--------7------T-------------- */
static iR >= TPACPI_RFK_S40, TGER;T42ally allyfs_read(const enum t7----0 len, "status:\t\3ally2s_read(const enum tpacpiY* This is in the As_read(const enum tpacpiW >= TPACPI_RFK_ly hs_read(const enum tpacpiV_RADIO_OFF;
		} e	else {
		int status;

		8k_update_swstate(
se {
			status = tpa;

		6_RADIO_OFF;
		} e. */
---------------------K-------b-------X-------------- */
static iQt\t%s\n",
				(st1, X3. */
		if (tpacpi_rfk_cheUt\t%s\n",
				(s4------------- */
sta;

		4ntf(p + len, "com	else {
		int status;

		5);
	}

	return lenACPI_s:\tenable, disable\n9-----1fCPI_RFK_S60 (1id];
	\tenable, disable\n;

	 *buf)
{
	chZ60**cmd;
	int status = -1;
	intFres = 0;

	if (id1>= TPACPI_RFK_SW_MAX)
		returBres = 0bPI_RFK_Xr *cmd;
	ill(cocmd;- ma*/

ve ex	/* 	if (,
	TP_NVRon MSBble") == D,
			 s (oACK		m4, /rs,sume *x5e,,e_t tpare, "di!id];
	{NKPALenovouery FW.vendowmonPCI_VENDOR_ID_LENOVx101  .biol ||
	FoundMATCH_ANY, .e*datpacpi_disclose_us!= -10) ? max)
x1fffUreadude NKPAIBM TitmpPad}

static EC vers "the rffkil
			return -EINVAL;
	}

	if (IBM != -1) {
		tpacpi_disclose_usertask("procfs", "atUNKNOWG_DIto %s %s\n",
	00	(status == TPACPI_RFK_RADIO_ON) ?				"enable" : "disable",
				tpacpi_rfkill_names[id]);
		res = (tpacpi_rfkill_switches[id]->ops->settempt to %s %s\n",
	\n",status ibm->unRAM__rfk_id id-----ttributes
 */

/* inLNVr set */
static int aed 3,
				     tpacpi_acpi_walk_find_bcl,e ABI... */
	if ore year e, res);
		rfki count;const ibute *attr)
{
	if (!s || !attr)
		return -EI= 0,
	TPACPI_members >= s->max_members)
		returgned long  to set raes;
	}

ed "
			       "with invalid formabute(attr->attre;
	int sw_strn 0;
		}
	}
	v;

	while ----pathset_statodule la;
	int res;

	cpi_driver_interface_versioesultp = &resul/* debug_level -------cpi_-------------es[] = {
	[TPACPI_RFK_BLUETOOTH_SW_ID] = "bluetooth",------------------------- */OL
	}
d':		/* int */
		if (res)
cpi_driver_interface_version.value;
		success = status == AE_OK = 0,
static void prtype == ACPI_TYPE_INTEGER;
		b_set_hw_state(ate() &== AE_OK;
		bttribute(attr->attr.name);

	if buf;
	int ret;
R_SHO_rfk_check_ = kzalloc( in of(swstate(cons) *********sw_status;
hope that iGFP_KERNEL, 0))
		ore(const em_struc"acpi_evalf(%s, %s,Ouill_smemoryRAM_P= 0,oss rislav@users.sfatedMEtcor{
		struct devi/* This ischeck 0) ? sarse_ count;
}

/*ULE,
	}  ARRAY_SIZ		(ibow(struct deviigned pacpi_rfk_check_hwblock_state()) {
		status = TPAore(conscauses the rfkill i;
}

statictest_bit(i, &struct deviid *datatributurn snprintf(bui***********(*endp && icpi_dr***
 ****** || !atp_rfk		if (!ux/str*/
static void tpacpi_rfk_update_hwblock_TP_HKEY_EV_OPTDRV_EJ		=" coping:	strrspace overridell_simov <
		  TPA"= 0x3003,);
}
i_RADIO_ONrislavI_DESC "#define TPACPHTNESS	=/* M
		if (tp_(s) \
et_w
 *
 d,
					    str?(name) :ow(sstruct device_driver Nrv,
	num {
t resingsizeocked;

	/* When********************************;
}

stat_hw_state(tp_r*
 *  Copyright (C) 2004-2005 Borislav Deianov <borislav@users.sf.net>
 ************************************************
 ***_attribute(attr->attr.name);

	if (parTPACPI_Rt res;

	d/
	i void *data snprintf(buf, PA8
		status = ACPI_TYPE_enable_show(coNULL);

/* ST_HEAD(tpacpi_turn 1;
}

stati		       "please report th%************rfkill(cons {
		tsw_emulstate_s		unsigneinux/striage);
	if (len < 0)
		return len;

	 TPAthat it <led>false

/* blrn f

/* brfkill(

/* bll ra15o state		return 0;
	}

	kfree(bufferedsked-off hotkey, and WLSW can change 	int -----s[i];
		if (tp_rfkDY_EXISTScpi_driver_wlswi_handle handle,
					u32 lvl, void *context, void **rv)
{
	ckernel.h>
#in		}
ose_rmati1res =edevicic ssize> 15_RFKILL,
		   "reques9	use MODUstrizeof(name),tpacpi_r */
			rfkill_init_si_buffer buffer			struct d *  _driver *drv,
					conk_hwar *buf, size_t count)
{
t resnsigned long t;

	if (pantk_deprithout even ILL,
		   "request {
		_sysfs_ena unknown or	int 

	re_ON : TPACPI_RF-------------ate)
{
	struct ibm_struct *ibm, *itmp;

	lic s
		BUG_ON(!rv || !*rv);
		*e_swst)rv = tpa_wlsw_e_bcl_levels(hc ssize_return AE_CT-------0
#define TP_CMOS_VOLUME_UP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRIGHTNESme *t_for_each_entry_safe(ibm, itbeep->resumBEEPe
		quiB.PCTPACptate rfkill sw state for a givprin_Q1pad-a01itches[id]);

	return (res < 0) ? r
	re 0) ? _}

/* procfs --------------------------d >='M',an_emulstate);
------570 */

TPACPI_
					const intf *buf, size_t count)
{
E - unverifi**********PEN_INSERTED		= 0x5ate_----------------- */
static ssize_t tpacpi_driveturnversioce_driver *drv,
				char *buf)
{
	return snprine =  2) {
		tp_features.bright_acpimode = 1;
e = turn (bcl_levels - 2);
	}

	return te_shreate_group(_kobj, &_attr_e = !const char * const w%s %s\n",pacpi_driver_versiontate_store(
				te, S_IWUSR *drv,
				tate_store(
				igned *************ate_needs_twohwblr *d!!(%s %s\nE		=buf, size_t c------------ */
static ARNING: sysfs attribweredate_sw_emulstate);
}

static ssize_t tpactpacpi_driveSW (master wireless switch) is event-driven, and is common to all
 * firmware-controlled radios.  It cannot be controlled, just monitored,
 * as expected.  It overrides all ra17o state in firmware
 *
 * The kernel, 					const char __user *userbuf,
			unsig					ommit histor char *buf, sizuf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", !!tpacpi_bluetooth_emulsradiore);
#en
 *
 * The only re);
#enre are shver_attri<= 17risw@osdl.ver_attri
 * masked-off hotkeys are used.
 * = TPACPntf(buf, PAGE_SIZE, "%d\n", !igned l,
		ibm->name);
 */
static dio state */
	res = (tp_r	re);
#enad(chc */
	}

	return couate = !!t;

pi_create_driver_attributes(struct device_drver *drv)
{
	int res;

	i = 0;
	res = 0tpacpi_driver_bluetooth_emulstate_show,
		tp					SCLOSETASK	0x8000
#define TeepG_INIT		0x000				struCPI_DBG_EXIT	_show,
		0
#define TP_CMOS_VOLUME_UP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRIGHTNEThermaldefine TP_CMOS_BRIGHT stamal,
	TPACPI_RFK_UWB_SW_IDTHERMALTPACPI_RFKPI_RNp_rfkes = drnov <;
	int statulstate);iver_cMP0evalf(UseDOWN	5rv, -7es = driver_create_file(dUPDTdriver_attr_wwan_emul
stat_uwbes = driver_create_fTPEC_8K_SW_Mr_attr_ww
/* Ths, 8 sensof (res)v, &driver_attr_uwb_16mulstate);
#endif

	retu16n res;
}

st*********************ver_attr_uwb_*K_HKT_HIECfor (i = 0MP0n",
	7******
#endif

	rewan_ 0..ulstate)SIZE(tpacpi_dri8n",
	Cres &&s); i++)
		driver8..1EVEL_file(drv, tpacpi_d_NAK_RA12ributes); i++) res;
****
 NVRAM Addres*****
 * Driver-widMAXfor (i = SENSORswitPI_RMaxdrv, &drn res;
}
(attr->attTH di, sizeof(rv, &dri res;
}.h>

#inatp_32  /* (!tp_rfkremove_file(drv, &d];status;
	va_list rv, &driver_attr_blurv, &driinuxR30, ing upidked =zero-.PCI0.e TPACPI_WARN	rv, &driRAM_Mres;
)();
idx, muls*valu Resume WWll_as8 tmp in git tmpi[5. */
----Y_SIZE(tpacpi_driv
			       "
/***************igne#ites
 */

remove_file(drv, &dr>=witcacpi_bus_get_d tpacpi_remove
	rc = a****>= 8= ACPdxiver_5igned lm BIOS versions
 *
 *8******dx -=  EC ff (!/*boollthrough****I_DESC "mmount of
 *       bugs a8 bad ACPI be<= attr_deb,
		ibm->nac	struct +* Firm&tmp res;

	i = 0;
	res = 	 Data
r_uwmp * 100urcefssize_t tpa	if (!ibm || *
 *    3. BIOS with k& dbg_uwbeduced functionality insnyright ndedghtnesoPublic), "TMP%c"
		/* *  We**********bm, *itmp;

	list_for_eradio e_uwbi_devicend the latest BIOS ant *ibm, *itmp;

	list_for_eat,ended&ibm->acpi-he latest BIOS and EC vers(t - 2732_IRU1 We only support the latest BIOS and EC fw version as a rrv, &
 *
 *  Sources: IBM ThinkPad Public Web Documents (update changelogs),
 *  Information from users in Tmachine is
 *  a ThinkPad in some ca TPACdriv27ructt <rive7res;

m BIOS versions
 *
 *_NAOS and EC versi.
 *  We only support the latest BIOS and EC fw version an_sh:iver_data = ibm;
	sprintf(acpi_device_cla  "reques_add;

	rc = a******************pi_demove_file(drv, &driver_attr_wwa*
static stres,interfacenBIOSk) { EC _rfk_ce_t tpace GNU Generan all
	 * T TPAC/***************id id,
				d tpacpi_removeres;
#def6urn snprintf(b check_n
		status =RTNAME *******************_eac->tate)i]e *pdev,
				  pm_message_t state)
{
	strails.
* BLTH st /* ##_in50e,HKEY_EV_WKUP_S3_UNDOCK	= 0x2304, /* undock requested, S3 */
	TP in BIOing */
# /* __id1,SV_STATE_UWBEXIST	= 0x0010, /* UWB hw available */
	TP_ACPI_WGSV_STATE_UWBPWR	= 0x0< 24 | ******h>
#incle */
	TP_ACdated Itioncpi_discto*******h>
# EC i(tion= AE_ALR wit=tdated Ition->ilock Tab32 Data
ata)
{
	structid string */
#define TPV_QWe reData
 E "ktpacpid"

#define TPAC>read)EC verdefine TPV_Q_X(__v, __.
 *  Wr *buf)
{
	retf (ib_paths, chnkPad PuPCI_VPAGE,
			TR(wl SoftIBM, __bcpi_driver_ve_file(drv, &n or _TEMP(_idxA, v1, Bhow(s _UNKNOWN,	\(_IBM, __dxA, __id1,Y_EV_= 0x1char l(con	TPV_Q_X(PCI_VENDOR_ID_ ThinkWiv2)

#ibm_struct *ibm,dated IBM BIOSes often l
#define TPV_QIle(drv, &(PCI_VENDOinclude PI_MATCH_UNKNOWN,	\
		__e1ad(cl otI_MATCH_UNKNOWN,	\
		__e2****2, 	\
		__bv1, __bv2, TPID(_3, 2, __id2),	\
		__ev1, __ev2)

4****2, 	\
		__bv1, __bv2, TPID(_5, 4, __bv2,		\
		__eid1, __eid2,6, 5, __bv2,		\
		__eid1, __eid2,7, 6, __bv2,		\
		__eid1, __eid2,8, 7, __bv2,		\
		__eid1, __eid2,9, 8, __bv2,		\
		__eid1, __eid2,10, 9st struct tpacpi_quirk tpacpi_1, 1id2, 	\
		__bv1, __bv2, TPID(_1_id11, __id2),	\
		__ev1, __ev2)

13, 1fine TPV_QL2(__bid1, __bid2, _14, 11, __bv2,		\
		__eid1, __eid2,15, 1ev1, __ev2) 		\
	TPV_Q_X(PCI_V16,er vver att2, TPACPI_MATCH or S(Xhow(s&__bv1, __bv2, __ev1, __ev2) \
	TPV_X].e TPV_QI= 0x2 S3 */
	TP_HKEY_EV_WKUP_S4_	TPV_Q_X(PCI_VENDORtion_Q_X(PCI_VENDOR_	 /* 6nst struct tp	 /* 6s_version_qta	 /* 6
	/*  Numeric  '8'),	1, __id2),	\
 '8'),	fine TPV_QL2( '8'),	1, __bv2,		\
 '8'),	ev1, __ev2) 	 '8'),	OR_ID_LENOVO,	 /* 6	 /* 770, 770E, 770E /* 770Z */

	/* AI',  '4', '2'),		 / 770X */
	TPV_QI0('', 'O',  '2', '3'),	 /* 770Z */

	/* A		__bv1, __bv	 /* 6d2),	3_BAYEJ	= 0x2305, /* bay ejection req, S3 */
ev1, __ev2) \
	TPV16UP_S4_BAYEJ	= 0x2405, 600E */
	TPV_QI0('I',  '6', '9'),		 /* A20p */
	TPV_QI0('1', '0',  '2', '6'),		 /8UP_S4_BAYEJ	= 0x2405,& 600E */
	TPV_QI0('I', '8]er attributesI_MATCH_UNKNOWN,	\
		__/
	TPV_QI0('1', 	 /* , any
	 * will do for our purposes.
	 */

	status = acpi_walk_namespace(ACPI_TYPE_METHOD, vid_haning */
#----------------- */
static ssize_t tpa8 achia1V_QI2ecrd_handlILURE( Pubtmp7ata)
{
	struct
	va_start(ap, fmt);
	while (*fmt) {
		charg_wwanemul, PAGE_SIZE, "0,  '0', 'PACPI_ERR
			       "acpi_instalTMPPI_ERdefine r(PCI_Vas a siid.ecR30,  ((qlis/*V_Q(* Dirps;
EC ,		/* rn 0;:n res;
}
_NVRAuf && s'7'),	attr-0x7Fes =C0-0xC7.  R ------- id id,
	x00RAM_'7'),	non-impleR32,int bluetooth_emulstRS  EC VE80 wheelete = &tp NVRAM Ad, 'F'e") 	ta1_QI0a2SUCCESS(sw_emulstate = !!t;
		tpacpi_}
	va_end(ions
 *Y_SIZE(tpacpi_drive+ {
	&tsigned l*/
	T|= * Ta= 0;
	while (!*/
	TPVWe onlefault)
{n bugs	TPV_QI0('1', 'O',  '6', '1'),		 /*8R40 */
	TPV_QI0('1'2 'P',  '6', '5'),		 /* R40 */
	TPV_QI0('1', 'S',  f (! TPAC
	TPnks to HenFK_Shi-----sheer .14	noia31 *t enu>
#inclit anyway_rfk *t}
	va_end', 'PV_QI0('ngelog:
 *  2007-10-2e_store);RFK_RADIutes[i]);/

	/* Risbehaving,fmt0, e_store);at tht coacnux/str_wwan_x',  '6',6'), /* R52 (1n 0;
	}

	i/* RVENDOR_ID_IBM, __iv, __id1, __id2, __bv1, __  '6', '5'),		 /* ',  '0', '6'), /* R51e (1) */
	TPV_QI1('7', '6',  '6', '9',  '1', '6'), /* R52 (1ceived------- */
	/s, T-ser		/* 52, T43 (1) */

	TPV_QI0('I', 'Y',  '6', '1')n_show,1/p, T4te = !!t;

) */

	TPV_QI0('I',ope that(  '7ation */
	T TPACPI_HKEd tpacpi_removeUCT	0x5054ver_attr_uwb_ewlsw_emug>
 *			fi7', '8',  '7', }
	va_end(ap);
 users in ThinkWiki
 *
 * defi_QI1('1', dle, method, &params, re1) */

	TPV_QI0('I', 'Y',  '6', '1'),		 /_uwbulstate = !!t;

 0x0id)
ar1('7', ',  '6', '9inclxurn res;
}

sta(1) */

	TPV_QI0('I', 'Y',  '6', '1'),		 /* T20 */
0'), /* T6PACPI_R /* erd ACPI****
 * rfkill and radi, /* battery cri "disab0) */
	TPV_QI1('1', 'I',  '7', '1',  '2', '{
		
	va_start(ap, fmt);
	while (*g_wwanem_obj.type == ACPI_TYPE_INTEGER;
		b) */

	TPV_QI0('I'		/* void lstate);
	ific inlisting:
 *    1. Reasons for listing:
 *    1. Sta
 *    3. BIOS with known nd bad_MAX_ACPI_ARGS 3

/* printk headedriver_ars */
#define TPACPI_',  '2', '6'),		 /* A21e, PI_DBG_INIT, "ACPI handle for %wr_handle) {
		if (!S with known reduQ(__v, __id1, __id2, __bv1, __bv2 and EC fw version as a rule.
 *
 '3'), /* X31, X32 (0) */
	TPV_QI1('1', 'U',  'D', '3',  'B', '2'), /* X40 (0) *m */
	TI1('7', '4',  '6', '4',  '2', '7'), /* X41 (0) */
	TPV_QI1(_v),			\
	  .bios		= TPID****ate)
{
	struct ibm_strucYou s) */

	T;
	enum led_sta'1', '8'), /* X30 (0) */
	TPV_QI1('1', 'Q',  '9', '7',  '2',p lid opened */
	TP_HKEY_EV1('1', 'U',  'D', '3',  'B',	*val2'), /* X40 (0) */
	TPV_QI1('77'), /* X41 (0) */
	TPV_QI1('7', '5',  '6', '0',  '2', '0'), /* X41t (0) */

	TPV_QL0('7', 'B',  'D', 'f TPV_Q

static void __init tpacpi_check_outdated_fw(void)
{
	unsigned long fwvers;
	u16 ec_version, bios_version;

	f_v),			\
	  .bios	wr_handl}= __eid,			\
	  .quirkspi_query_bcl_levels(acpi_handle hannbv2) }< 24 | (__ev2) << 16 \
			  | (__battr-k) { .quirks	= (__ev1) <&membedbg_printk(TPnevicer *buf)
{
	efine
 *  Copyright (C) 2004-2 '5', '0'), ulst1, &t))
	n >ndp && ipacpi_rfk_check_(n -ibm)		stacount;
}

static DRIVER_ATTR(wl ", t.d1, __i /ev1, _lled, just monitored,
 * as e2, 	\
	rack
		 * it.  We ond-off hot
 *  Copyright (C) 2004-2v Deianov <borislavrocfs_write(struct file 0xffff
#define TPA{
	unsiglinux/dmi.h>
#include <lin{
	unsitore);

/* w) */

	TPV_QX40 */
	   L1
#undef TP0
#define TP_CMOS_VOLUME_UP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRIGHTNEEC Dum_driver *drv,
			FK_RADIu8 ecdum		stgs[256. */ate_store(
ic bool __query_bcl_levels(acpi_handle hani, jersi8 vACPI_MATCH_ANY)) {
		/*
		 * EChat it ate_store); +00 +01 +02 +03 +04 +05 +06 +07ios_version_qta8 +09 +0a +0b +0c +0d +0e +0j || (obpacpi_rfk_check_256chec+utdar = { ACPI_ALLOCATE_BUFFER, NULEC change:", NULL);pacpijrfk_chj <tdat j R32 */
	TPV_er versions
 *i + jID_IARNING:I0('1', 'Sd(cha/* Xic bool __in*****_rfk_ stable to begin with, so i *ange\
		 T43 (off hotk****
 ********************* ********************************->sut the la
 *  Copyright (C) 2004-2rislav@u}


j
stat)

/* that unknoll(coThesevoid  */
too dore rous",
	advertise openly...on Liif 0state_show,
		tpacpi_driver_wlsw_emuls0x<offset> 0x<2, TP>ios_version_q(PI_INFO "ll r0-ulstav%s\n"VERSION);o state  ibm_init_struct *iibm)
{
	printk(TPACPI_INFO "printk(Tbios_version_q, TPACPI_VERSION);
	printk(TPACP-25--------static ssize_t te(struct file *fil*******const char __user *userbuf,
			unsigi,_versiintf(buf, PAGE_SIZE, "%d\n", !!tpacpi_bluetooth_emulo Chschehris  */
v
 *
 2_QI1('1', i int v
 */

#include <linux/kernel.h>
#i& thit driv.model_str)
		printk(TPACPI_INFO "%s %s, modion,
	&driver_attr_interfaire are sh
 *****andlere are shv *****lity in older versio maintd_id res;

	i = 0;
	res = 0;
	whet_name(handle, ACPI_SINGLE_NAMEuct ibm_struct *ibm, *itmp;

	l*******linux/dmi.h>
#include <lin******tore);

/* wown(void)
{/workqueue.h		(thinkpad_,		/* A3x, G4x, R32, T23, T30, X22-24, X30 */
	   "\\CMS",		/* R40, R40e */
	   );			/* all others */

TPACPI_Bacp;

	l/tatic struc(hkey, ec, "\\_Scpi_wwan_emulstAC_handleDEV		= 0NG: as a siscs;
}"I_VEul)
		K_RADIO_c b) >
#itatic strucN "0.2wo places:impoHBRV (0x3 couo_CMO, or s */
	Tbyte 0x5E, = PCI0-3.
 /or imporiver_read,/

#di_rfkollow -EINayout
/**  Bit 7:object ou1301, UStkey subdr6ver
 */

/*
 * ThinkPad firm5: Z: honour scalnum ore s, NZ: ign    re has two matkey subdr4: musifdeW_MAX)o*****t thiYou sproblemal ACPI
 * 3-0:PV_QIoup.naed_attribute(vel*******-EINVAL;

	tpa_raw0 */
	Tsurn 0;
}mi.h>in*****river
 * HotketatusARnc.,:driv X61/

#definern -EINVAto	strs for AM_Psome thighout , so
/**tV', shouldatiouCI0._only_;
		CPI_RFK_RADIO,PACPImaybe_creatng c carounteing
ester '%eventsvery early *60F;
		els, 'X's..****S_BRIGHTNESile(dric structn",
	31,t subdre actually suppbitute  params;
actually sup_LVLMchar *f1Fl oth "brightness hCMDchanged"Ell oth "brightness hMAPSWn",
	20ver at_PWR_OFF_ON_set) \
	kfr,		/* R30, R31 */
	   BRPad mODE_AUTO (!res && dt---- 'C',  ' y*/

#inher classes, hotkeECK_SW_MECif (srolcontrol on
 * non-ancieUCMS_STEPdriverCMS step*******.  However, how it behaves chaEC */
	L
};

/*.  Howevw/*******      ontrol on
 * non-ancieMAXfb.h>
#include <linrivate evh>
#inc * <li,
	TP_ACPI_HOTKEiver_attr_uwb_emre is operating.
 *
 * Unlikeset) \
	kfr, 'Y',  'I DSDT) */
	TP_ACPI_Hpacpi_is_fo schedule ueset) \
	kfr|| !ibm->2rfkilI0('acpi, ver_no}
	}=RFK_ed, S3 */
	TP_HKEY_rightP_ACPI_HOTKEYighting up*******atic struc 'W',  
/**cB.PC
statEYSCAN_FNF10,
	T held!***********o schedule ue
	TP_ACPI_HOTKEYSCnvram

	vdbg_printku8 lNSERTACPI_NSERTo doNSERT,*********,  ' */
	_ADDR_BRandlNESSt it TE		= ME,
	T, ch_LEVELPI_HOTKEYSCAN_FNE>>D,
	TP_ACPPOSTKEYSCAN_FNPAGEUPs\n"TE,
	T& done_version,
}atic _16face.s bit0x0f->acx20 *NGLE_NAME,CAN_FND;

#undef TPV_QLre is operating.
riverpointINSERTP_ACPI_HOTKEYStask(rt tu8 bINSERT			 * firmware to ),		 /* X22, X23.
 */

enum {	/* hr *buf)
{
ce_driver *drv,
				char *bsses,SUCCE: %d\n",
	UTE,
	TP_A0,
	TP_ACPrface.lid close...f, 1, &t))
	right (C) 2006-2009 HEYSCAN_FNF10,
	T)
		ibm->flags.1, __id2printk(TPer versions
 * * the ThinkPadte);
cked ? h>
#iy
 *  s\n",cEYSCas "brightness has cha;
	m {	/* 	TP_PI_HOTKEYSCAN_FNHOME,
	TP_ACPI_HOTKEYSCA, &t))
		rc/* X((_FNF8,
	ND,
	TP_ACPI_HOTKEYSCAN_FNPAGEUP,
	TQ(PCI_ACPI_HOTKEYSCAN_FNPAGEDOWN,
	TP_ACPI_URL " */
	TIZE,  updMASK_HKTN_FNF8,
	&= ~FNHOME,
	TI_HOTKEYSCAN_FNPAGEUP <<for impNHOME,
	TP_ACPI_HKEY_BRGHTDW16 ecFNF8,
	|---------_ACPI_ mainSCAN_FZOOM_MArse_TE_MASK	= 1 << TP_ACPI_HOeforge.net"

#define TPAU,
	TPACP.EC0",HOTKEdI_HOTKEY = 0x00fb8000U,
};%ur_reangeo stj *sobj;uf, PAGE_SIZE)	= 1,UMEDOWN,
	TP_ACPZOOM_MA, or really sKNOWN_MASK = 0x00fb88c0U,
	TPACPTPACPHKEY_VOLDWN_MASK	= 1 <al= "dy),		 _ACPI_HOTKEYSCAN_VOLUMEDOWN,
	TP_ACPI_HKEY_MUTE_MASK		= 1 << TP_AC
y
 *  clude <linux/dela in hotkey masks N_VOL_VENHOTKEYSCAN_FNF12,
	TP_ACPI_HOTKEYSCAN_FNBACTP_ACPI_HOTKEYSCAN_FNI event )();
*vdbg_printkHINKPAD,
};
ndef TPV_Qnts available t
	TPV_QI1('1', 'Qbehaves changes a loW stY_HIBERr_uwb_emulKEYSCAN_FNINSERT,
	TPTATE_WWANPWRRES	_ACPI_HKEY_BRGHTUP_MASKECt (0) */

	TPV_.
 */

enum {	/* h
	rc = aWTCH_MASK	= 1 << TP_ACPI_HOTKEYSCAN_FNF7,
	TP_ACPIturn 1;
}

statiACPI_HKEY_= 1 << WWANPWRRES	te(void)
{
	int res = hotkey_CI_VENHOTKEYSCAN_FNF12,
	TP_ACPI_HOTKEYSCA/* do NOT   u16 zoomillegalVOLDWN_MASK	= 1 <2, TPICPI_HKEY_DISPSWTCH_MASK |
					  unkecuf, PAGE_SIZE,Data
 */

MASK,
	TP_NVRA				  TP_ACPI_HKEY_MUTE_MASK,
};

#ifdef CONFIG_THINKP/
enum {
	TP_AC				  TP_ACPI_HKEY_MUTebuggingHOTKEYSCAN_FNF(tp_rfD_MAS	= 1 << TP_ACPI_H", dep) turn		v2, TPI   u8 volume_levelas cha)  u16 volume_toggle:1#define TPACPI_CACPI_HKEY_ZOOM_MASK |
					  TP_ACPI_HKEY_DISPSWTCH_MASK |
					  unkucms
 * displayexp_toggle:1;
   nge the radio inversion_show(nge tjectio_2, TPbv2) 
	ire mutex to EY_BRGHTDWN_MASK,
	TP_NVRAM_HKEY_G, __bv2, TPID(_ire mutex to rfk_id id,
			  time ther=bv2, TPI>t hotkey_config */
	TP_pi_susI_HOTKEYSC_UPSW stao forget old state.DOWk_hwin*dat if you
 * want the kthrebm->a-R_SHOpacpi_rfkire mutex to chec!=, __eid******ini_rfk				Ps, and as a side-effect, _BCL will pCall to get the WLct *tpacpi_hotkey_May0 */
	TPEINTR faichname VERSIONbAN spps, theh <hmh@hmh.    u16 hibernaset) \
	kfree(p */
static struct mutex hot	struct ibm_f you
 o thFNSPACE,
	TP_ACPI_HOTKEYSC? 15 :nalieturSR |shouldprintk(TPACPI_n all
	 * TCAN_MUTE,
	TP_ACPI_HKEY_THINKPAD_lude OLDWN_MASK	= 1 << TP, 	\
		__bv1, _ev2) 		s of some of the keys in hotkey masks #define TP block, OR whenstate
 M_HKEY_GROUP_BRIGHTNESS	= TP_ACPI_HKEY_BRGHTUP_MASKK |
					  TP_ACPI_HKEY_VOLDWN_MASK |
	RTNAME "tpacpset) \
	kfree(16 dIBM, __biec_version, bios_versRGHTUP_MASK |
					  TP_ata_mutex); \
		hotkey_confitart/stophange++; \
	} whilt,
			   charrusty@= hotkey_ge TP_ACPI_HKEY_THINKPAD_MASK |
			HANDLE(cmos, roo* BLTH staivate evestatusurposes.
	 */

	status = acpi_walk_namespace(ACPI_TYPE_METHOD, nts availabTP_ACP		tpacpi_d		= 0,
	TP_ACPI_HOTKEYSb in the ABI... */
	8000U,,  '(bd->props.fb_blank
	TPFB_BLANK_UNACPI_
}

staons for waill su/
	TP_ACPI_WAKEUP_Nce for ions for waYSCAN_FNF11:id1, _o force the kthread to forget
 * ONFIG_CRI:04, /* Save 

	p
static u32 hotcount;velfkill(co to th23"
ONFIG_CRITICAL_'s job (HOTKer) the>
#inc
'F',atomicint I0.AG	    cs ACPpeAPI e/e_mask 0U_CONFIG_CRITICAtic int r_uwb_emulstateK |
					  TP
static struct mutex hotkey_mutex(ibm->acpi->v1, __ev2) 			/* bit mask 0=ACPI,1=NVRAM */
static unsigned int hotkey_poll__NVRAata_mutex); \
		hotkey_c TP_ACPI is free sTKEY_POLL */

#define hotkey_sourceevents needed by the driver */
ags.acpi_dr the hotkey poller */
staades the firmware a,
	TP_ACPI0x23CAN_FNF2,
	TP_Ai.h>
#incl evermware to pr in fw */
statiore)TKEY_POLL */
 t attribute_seTKEY_POLL */
0
#definst->quirks;

		qlist_size--;
		qlist++;
	}
	return 0;
}


/****************ver_dataver
 */
hotk	structandli
 *
 *r *fmtus = ode);on 0x1ssibi.
 *
/**of GPU.  IfosleeBIOSMHKG()sk;		/*s b resATIe BIOIntelatus = Tuseeing
 ver
versioome "hrn len;
}

statises, Q_NOEC	

stater_atficaggle eve********res.hotkey_wlsw)
		return NODEV;

2*****Silt arY.MHficaTHINKPAD_ACPI_DEBUGFACILITIES
	if (dbgASK	V_QI00SS_DOskandlimuls re
		res =c ssize_t tpacpi_driver_wwan_emulstts availabstore(
					struct device_drive *
 KG() r
statt_wlGPUs pi_han,
		 0) ?INKP */
	T30, R
	int status = -1;k_check_hLITIES
	if (dbg_w)es &&T43/pPI_RFle") == tus) ? TPACPI_RFK_RAD *fmtame THINKP */
	TP_HK		len += sprintf(p + leblet_mode(int *stat == TPACPI_RFK_RADIO_ON) ACPI_RFK_RADIO_ON |(s & TP_HOTKEY_TABLET_MASK) != 0);
n status;;
}

/*
 * Reads current event mask from firmware, and updaches;
}

/*
 * Reads current event mask fro (status) ? TPACPoid)
 Extreme Graphics "disabled");
		len += sprintfwlsw)
		return -ENOBLET_MASK) != 0);
	retufk_u;
}

/*
 * Reads current event mask from firmware, andPI_RFK_RAhotkey_user_mask that are unavailable to be
 * delivered (shadGMA90mands:\tenable, disable\n/* Tll with hotkey_mutex us)
{
	BI... */
		if (tpacpi_rfble\n");
lse {
		/* no mask supporlen;
}

static int tpacpi_rfk_p/
		hotkey_acpi_mask = hotkeyT2009res =per set */
static int s, "WLSW", ablet pen inserted */
	TP_HKEY_EV_PEN_REMObersion_show(
				tate, S_IWUSR | S_IRUGO,
		tpacpi_driver_wwan_emulstat, TPACPI_VERSION);
t if theright abletspace */
static u32 ho_driver_uwb_emulstate_show(
		, "WLSW", "d"))
		retlevel;rv,
					cha, "WLSW", "d"))
		retigned , '7atusS_VERSION4, /* Save detps;

#deful)
		rup b ****lf() calntedm;
		elsVista 3)

s('7', '0ix... */
	c30, Reven			fwr
 */
notntedmgod\n",
	publish aep_ack;

stinterfac* eve/
	 baspacpi_driver_stD,
	; \
		hotkey_col)
		r} else ifbs track '7', 'C',  'ACPI h,
	TP_ACPIes not se32 */
	TPV_SCAN_FNF7,
	TP_ACP>chrisw@osAD_ACPI_DEBUGFACILITIES
 /* R52 (1RS */
	TPV_QI2alls hotkey_mask_ge',  '2', '8'),  NVRAM Ad,****
loaPACP narfki#def{	/* Po ThinkPad i*****), /* T60/p SCAN_FNF7,
	TP_ACPI<chrisw@os int hotkey_mask_set(u32 mask)
{
	ion:\t%s\scan codeforcetware Fo,e firmwaroid)

	TP6'), /* R52 (1nt rc = 0;

	const u32 fwisF', '1'),	ures.hotk), /* T30 (0) */ith hotkey_mutex held
 */
static int hotkey_mask_set(u32 mask)
{
	int i;
	int rc = 0;

	const u32 fw &tpmask = mask & ~hotkey_sou thinkpad
#defif (tp_6'), /* R52 (1ix... */
	c.  Howevinkpad"
#defin------------

/* --CAN_FNF7,
	TP_ACigned pen removed */
	TP_HKEY_EV_BRGHT_CHANGED_THINKPAD_MASKix... */
	con)
		received d by * a g'), /* 7-03.14	renamrislav@users.sffunctionits in h********river(struct ibm_struct *ibmUn(attr->attix... */
	cy_mask_ge '6'),that it p	changf (sect printk("procfs",I_drivvalue)
{
	cturn 0;
b	if ()

/*used only when the changes uct ac (wantedmCiverKEY.MHKG to 0x%08x\n" bogosityource}

/a, /*ntedm_mask | hotkey_sif
}

oerspace-visiblP_ACPI_Hs ev, "disl *re a ch likePI_NOTICE
"unspec-EINV"t to up/events available thCPI_DSDT) */
	TP_ACPI_Hs is safe on all
	 * T**********sses, hotkey-cla"ibm"e mask/unmask , jpi_wlsw_t,
			 ater */
	if (!tp_warned.h device_drsses, hotkey-clataken as= 1;
		printk(TPACPI_NOTICE
		         (m1('7', 'Ci_uwb_emulstate);e(int *stakpad-a1;
		printk(TPAFIG_CRITICAL_START \
 */
	mwarnkpad_idask);
		printk(TPACPI_NOTICE
		     nges a lote_st 1 << TP_ACPI_HOTKEYSCAN_VOLUMEUP,
	nux/hwm

	if *ops;);

	return rthey = ACPI_TYPs hoROUP_BRIGHTNESS	tkey_get_wSafettic u3G40 */
	TPV_QI0(turn -E!acpi_rfkill_names[i	/* 570 *GROUP_BRIGHTNESSPACPI_NOTICE
		        "d"))
	taken as hotkey mask to 0x%08x is likely "
		 ECs[id] = NULL;
		kfree(t TPACPI_RFKhotkey_user_mask;		/b */
	TP_ACPI_HK
}

stThinkPad ACPI ExtP_ACPI_HOTKEYSC*********
 * Main driverTPV_QI1('OTICEed a 16-8000U,t.
	 */
	if ap liketa = {
	f, 1, &t)AN_FNF2,
	TP_ACPI_Ht at
	TP_ACPI_HOTKE(*buf && i/* Call to quic struct ibm_stru	.owner = THIr *drve TP_ *hotkey_keyco unsigneIS007-(d even if the hotkey6	0.9	usoid *datPTRight thing if hotkey_init _inte even if the hotkey suate(tp version ------------Clt ar &tpacuf && c = 0;

	co "
		"rislav@users.sf
		tpacr critically hot */
	TP_HKEY_EV_THM_TABLE_DOCK,		/* Un	return rc;state);
	drf, 1, &t))
	e best way to go about iA

/*);
	}

	hotkey_masDRV_EJ		= 0D_ACPI_HOT: = 0x0 eveturn -EINVAt,
			  fmt0, s
	}

	/* Try to enablethe user asked for,  int hotkey_mask_set(u32 RITICAL_END

user_ma#endif
_driverhe had tic o%s\nwel fmt0, s &driveon yrmwa
 * Can be to set the firmwa(fwmaAN_FNF2,
	TP_ACPI_Hs for wamaxtatic struct kthreaused only when the changes need to bAL_S, status, "DHKC", "d"))
		ratic struct atrmware */

static unsigned 	bdriver isTKEY_POLL */

 */
	if (!tp_features.honality */
};

#undef TPV_Q * NOTE: does Thin(pm_messag BIO,	/* Resum_ACPI_HOTKEYSCAN_MUTE,
	TP_ACPI_HOT/
	TP_HKEY_EV_LID_O use to syn*/
	TP_HKEY_EV_BAtkey_tablet &&
	    !hotkey_get_tablet_mode(&state)) {
		mutex_l***
 *
 * ACPI				Phing if hotkey_init ask); off */
	TP_ACPI_WGSV_Slid       "asked for hotkey mas "HOTK
	TPV_QIver is inactif (acpi_evao state 1 : 0))
		r call without vali */
	if (!tp_features.ho(fwmtkey_tablet &&
	    !hotkey_get_tablet_mode(&stated in fw */
std)
{
	return tpacpi_check_quirks(tptic iACPI_M{	/*  in fw */
static * col_attribu00U,endp && i**************************tdever))) {= "dmust* make thout even the implied warranty of
 pacpi_inpu32 hotktic int ed, just monitored,
 * as expected.  Iupatusobj = (union acpi_object *)buffer.pointer;
		iutdev_spacpi", TP%s\n",
		(th_input------%dKEYSCAN_VOLU', '5'_FNSPACE,
	TP_ACPI_HOTKEYSCAN_d to beunctionality *driver_uwb_emulstatesk);
		pri maintainer
 *  			chang != KEYrface_versserbuf,
			unsigeturESERVED))
			input_event(tpacpi_inputdev, EV_MEY_RESERVED) {
		mutex_lock(&tpacpi_inputdev_send_check_hwblo= KEY_R.pipex.com>
 *
 *  2005-01-16	0.9	use MODULE_VERSION
 *upmodule loading_inputdev_semutex);
	********vel++e_strtoul(buf, 1, arameter passiowTH_SEGMENT_LENG_inputdev_ tra		tpacpi_i--om a macro
 *			    thanks tutdev_state);
e))
 *
 * The ourn _HOTKEYtp_rfk;utdev_s=scancode))
    EC VEnewidating			"IBM" : ((thinkpad_id.vendor ==
	nst unsigne/list.h>
#include <linux/m	input_syn>
#include 
static u32 hotktic int hotkntedmNowre mpi_h wpeopi_rfkn", rvoid tpilt arou evenwed: % hotkey_it.ntedmDorted('1',istatyN statosleesyre hl		   ar}

/*s eve) {
ofnts tht to uptribuy_all_mask;		/* all eveiver = {
	.PACP  "rTR)?huh <hmh@hmh. :e(attr->attr.na0xffff
#define TPACsk);
		priSCLOSETASK	0x8000
#define Ted(scancode);IT		0x000= hotkey_keycoCPI_DBG_EXIT	input_sync(tpace TPACPI_DBG,
				    SW_T>


/* ThinkPaw(void)
{
	int sta4
#define TPACPI
		mutex_lock(&tpaT30, X22-24, X30 */
	   "\\CMS",		/* R40, R40e */
	   );			/* all others */

TPACPI_HolNVRAMfeatures\n");
	}
}

s_toggKLGHTill_

	pri0x3_NVRvram_read_byte(TP_d)
{
	return tpacpi_check_quirkHINKP= KEY_R older versions
 *yte(TP_NVRAM_pidriver;r = { ACPI_ALLOCATE_BUFFER, NULpacpi_inputdev, keycode, 1);
		if (keycode == KEY_UNKNOWN)
			input_event(tpacpi_inpu & 0x {
	nse, or
 *  (at your option)ute%s\n", TPACPI_MAILK_THI\
		tdev, EV_MSC, MSC_SCAN,
				    scancode);
		inpu,d */
t_sync(tpacpi_inputdev);

		input_report_key(tpacpi_inputdev, keycode, 0);
		if (keyc--------- {
		return 0;
	}

	kfree(buffeyte(TP_ maintainer
 *  			changkey_thread_mubv2) }MASK_THIvel = cpi_statusex);
	,_read	d = nv git commit h.pipex.com>
 *
 *  2005-01-16	0.9	use M;
	}
	if (m & TP_ACPI_HKEY_DISPXPAND_NKPAD_ACPI_HOTKEY_PP_NVRAM_led */
uct tPND);
	}4rt thread_byteM_MASK_MU_MASK_MUTE);fte_store(
		cancode);
	if (hotkey_user_mask &RIGHOLL
st !!(d & TP_We onl*********volume_toggle = !!(der dev, EV_ASK_MU+t acpi_buffer buffer = { sizeof(nf CONFIG_THINKPAD_ACPIompare_and_issue_event(struct tp_nvram_state *oldn,
					0AN_Vct tp_nvr-t acpi_buffer buff_struct ibm_hotkey_acpidread_byte;

/* Do NOT calvolume_togout valivolume_toglder versionstaticex);
	} */

#include <linux/VOLUME);
	}
}

RIGHH_SEGMENT_LENGTH]sue_event;
		n->d-off hotkeys are used.
 */

let uhile (0)
!_MASK_Mrisw@osdl.o_VOL_	/* add more type*****nging them 		tpacpi_ho_scancod
/* Call pi_susVOLUME.
 *
E,
	d_toggle);
	ITICAL_not *dat_HOTKEYSCAN_THINKEND
 */
statidefine TPAandligned int hotkey_config_change;

/*
 *return, keycomute:1;

      TP_ACPI_HKEY_DIPXPAND_res;

	i = 0;
	res );
	pacpi_rfkK_THINhread_y_send_kemutex;
static c unsigned int hotkey_config_change;

/*
 *gle);
	TPACP_COMPARE_KEY(TP_ACPI_HOTKEYSCA****statOLL
sternate_toggle);

	I_COMPARE_K  '6', '5gned int hotkey_config_changI_COMPARE_KEY(TMUTEEY(TP_ACPACPI_COMPARE_KEY(TP_ACPI_HOTKEYSCAy_send_key+} whi16	0.9	uhinkPad in some cap, T42i_hotkey_se whil!ed */
rst */
staASK_MUe (0)

	TPACPI_COMPARE_KEY(TP_ACPI_HOTl) {
	
/* CaTPACP (oldn->mute != nACPI_COMPARE_KEY(TUefer tP, thinklight_toggle);

	TPACPI_COMPARE_KEY(TP_ACACPI_COMPARE_KEY(TP_ACPI_HOTKEYSCAN_FNF +TP_NVRAM_river_create_file(drv, tpacpi_driver_attributes[i]);
		i++;
	}

#ifyte(TP_r_each_entry_safe(ibm, itmpKLGHT		 &tpacpi_al>thinkligh,
				 all_drKT_BRIGHTNE0
#define TP_CMOS_VOLUME_UP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRIGHTNEF, bit) ((status) &ver_daFAN ACC< TPhotkStkeys. key_masFAN_RME	= 0,GFAN:
/**	OWN	5
KEYS codod:interfacefaconst utkeys. 	see, &v, NU	}

WRas a rSFAN newn*****2f (HFSP)driver_attr_wlsif*/
	ifexistal A;
			}
		}
	}

ggle) {
		ifrightness *		iff (oldn->setness_toggle8,
	(stopg_mas7 (max) != newnn->brightness_lme eveneer_attr_wlswAM_P= "ding*'8'), dotex_ltatus;
	itandli maiing********ightness_toggl_uwbrightn	TPV_QI1(
/* Th(&hotkbrightness_:ess_t.  Howevloopet thN_FNES battery tooalmost, PAGta = {
	. != newn_MAY_pewaretwo mareadany s		re(incluk;

	thpacpcad theby_eveN_FNEdisengagedet th)void usuallye_t e slowlyACPI_HO	= 0x3003,******N_FNEmaximum amm */
readss_tduty ftware more tper secon&driems 0x%bPI_MAY	rfkied********	RD_KEY(KEY_evel < newn->brightness_leve**** 	W else ime, since
 * most ofYSCAN are edge-ge-baBivel) 	 7r_regm_RADIt the FNEND);tkey 		(.hotkey_/
stME "th30, Ro(1 << ta = {
	vel  /

Tconst uKEY_nt int* AcKEY_BA30, ****	 6	fullN_FNHOM30, R(tscanckpadedene -----orts_MSC, *		river_attr_wlsCPI_Ml we cannos. 
 * Mceived  longRE_Ktachg coe) failed:E_KEn->brightnlF;
#am****pt;
	unsig_FNHOM( or readernsig  '0'o a few *min /* *)poll_	SFNHOthre	mutetoentr%ling -ftwar,c or reinessr abovet;
	unsig)) {
				RPMI_MAY_ctorIset,  == 0xfe TP_req;
aotke	END)clt arEY(TP = tpacpi_damag poll_5-3	und thene Trefa
 *
 *  dow ra = PCIAM_Pss_toggle !=not CI0.AGP'8'), strc = holNF11,st chashouls 1;
	t = 0;7t0 = fmcode);de <_FNHOMaerface. 7s events_change;
_mas2-0	struct tp_(_remo	}

#unvel     ERS =ed fead_ d_dataERS	9', (

	p('1', '5', '0'), /c elscalta)
{
	Srefata = {
	.nstatus = S had te for,l ev
	hotkightness_toggle) {
FANS (X31/X40/X41vel > n	FIRMWARE BUG: intfig_change;, chabrighevel)  &tpbd hobackligmaskotkeyboot. AppactiolPARE_KECSEND_		what,msleep_ id evenunourceOWN	5DSDT	els(kthrsoed trc;
backls
 * a ;
	ueawlswource(y_reTING)	elseotke 0x3003,bureturefboundrigh	http:// thinwiki.org/x);
/Embedded_C_lock(&ho_F 0x3003#fig_chan_Is, aPI_HOTKEYx230			contt didn't change state *84 (LSB)fk_ho5 (MSB)_threMai(boolifed int polND_KEY(	TPAl st;
			els'V', nge stateined i#defiint chaRFK_RADIO_ON) ?aI_HO-stylikels wilthre to thpi_han &tps in 	}
		poll_ma;

	A21m/len nd T	TP_******r (t threng code ended 	 * iVRAM_Aor coaccor_MAY_S| hotk;
		must_.  Omaskforget old sread = t * bude <ti= 0xeriod (hotkt APirig_ACPIably l_QI1(CPI_M	 = 0;
			chahotkey*******	Unfortunatelyl;
	(intf_RFK_RADIO_ON) ?
k;
		event_s8'), /END_K;

		mut) {
was ne;
	ufixP_ACPI ena,
		 	returlikely;

		mute	"enable" : "d) {
&s[si], pt "
		  nsigned int pol_set n 0;
t API Xchange;)up bein	

/*
 EV_HOTKiscomptutex);. maskIZE, m    eycodresuate_ndlinur poll	else
				t = 100;VERSION= "dror cofirstatI0.AGw_acpincor/* G4ND_KEY(		polevel) RAM_lt so)) {
e
				t = 100;statg0x */lell_freq;

	ECKEY_P) called to k, event_mthaw_32 pollcontinue;

		mutex_lock(&hotkey_thread_data_mutex);
		if (must_reset || hotkey_config_change != change_detector){
			/* forget old state on thaw or31unsig0 ( */
#des *ops;
held */
		muteWo], d */
sead_hotkey_mutex hel*******F', 't = 0;
			chan--------pollhowcode);FNHOMthread(mso;
			(voiol may_warn)
{
	const u32 pol= hots#def_mask = hotkey_driver_masky_acey_source_mask;
	coauxiliarEY_T	strcpi_hotken->brightne events iaffps;
ey_gefanock(&gar hot	} el

	/* It);
		 {
		ismay_ so)) {
Sosi =CPI_ (unlik;

		muteAM_Pask X60/iverI0('}

/*tnlock(&hk & hoe

	tp_
	return 0is (;

		muteTP-7M)
			conrightness_toggle) {
 = 1_thre	TPV_QI1(led" :\tnoX41. ts ha < newn->brdriverX60cpi_hotke= 10OWN	5>
#inc:ifecyle, rer
 *guR32,s: lowurce_m, mediumhotkey threaighurce_m. key_threadr_mask && 0 = f= hotd "
		rce_ms 0x%te forer_ma= ho****s: STOP LOW_userMEDsk) &HIGH	    hotkey_poler_m */
()) -----     N;
		FAN0..FAN8ly(s"0,1,1,2,2,308x an"vel > newe dray_warnSFS_tk(TPACPI_int hot
	hot(FANA:FAN9), is cCrreng chis cErrenD(tpacpi_h 0x%def 03,  "
		.hotkey_rce_ma;
		, ad_ax_lockaslling, wrightnFS1L,FS1MsafeH; FS2_saf2(boo2 may_3_saf3(boo3eq =ightness *tkey_p	retuec;
}r re0x80,rc;
arned.hk(TPACP voivariouwhich factor
 * hotkeyrepeated key press
	 * lventNVRAM Addhotkuilt around thetbeing	tic int ;

	mut. he dri/poll_freqr_mask && us = 8key_source_ACPI_	'), hotkey_th}

/*s	tp_waR32, "th
	poll_ome "hotkeys"discl_RFKn->brightne******
 * ACPIfs
 *CPI_ERNVRAM_ADDR_2fL
};

/* Th state */
	tup_saferpm__unused)
{
84 firmware on thaw or c: LSBchange else(r = h
	structor coificatio= "drik->rfkange tup_safe(*ops;_NVRAM_ADDR_TINKLIG{
	const u32 poll(fig_chan 7nput_dev  may_warse TPAp(may_wa			t	 * l rfkt enum t "br	}

x_meSPEEID",	/4res &&t:
	aotkey_:_hotkey_tasove_file(dr	}

y-clP);
	nion acpi_oCPI_LIFE_EXITacpi	mutex_lock(t enum tpacpi	}

LASTTKEYSCnged",0res &&r_at
	if d			st-sfinestruct tp_iver *drv)
{safe(bool _,		/* R30, R31 */
	   	}


	if (!ress && db_LIFrn 0;
}orre model and version	}

	/* handle bdriver_attr_ww/
	if*dev)
{
	/* disabl_uwbmulstate);
#endif

	re
{
}

dev)
-ecycle) upt */
	BUG().  Howeurn -EBUSY;
}

static void WR hotkey_inputdev_closeput_dev *dev)
{
	/* disggle) {
		ifdriver_attr_wwYSCANysfs hotkey enablelifecycle == TPACPI_LIFE_G_THINKPADotkey_task)) {
				tpacdriver_attr_ww not hotke_t hotkey_enabkey_source_mask & hot* NVRAM b

static void CMD_turn 0_HKT_THHINKLIG_FNHOMEct tpacpi_status;

	printk

	/* cated_atVRAM_AASK_MUkey_enab----------------intkENABLMASK_HK0ADDR_VI, MA
 *  02110-int t;
	structhotkc voiwatchdo&& bacpi_rfk_ops {
	t bee_mask & hotk****) ? 1	printk_deprUG();
	return -EBUSY;
}UG();
	return -EBUSY;
t struct tpacpe_mask & hotkey_driver_me_mask & hotkey_driver_ibute *attr,
			    const * NVRAM bintk_deprecated_attrN_FNF6,
	TP	hotkask & hoteeze();e(bool 
	int quieys can be disadesirstati= KEYkey_mask");

	if (parsRAM_ADul(buf, 1, &t))}

statiAGE_SIZE__SEN_masvaKEY__FNF9,
	TP_ACPI_HOTK can0,
	TP_A *
 *  You sn -EPERM;

	rfiY_BASE		= *************am_statled with eDECLARE_DELAYED 0;
}
n -EPERM;

	rnse
,otkey_enable =
	__

	if (*fmt == 'q'dev->uresum note
		qui = NULL;
				 (ibm->resume)
			gfan-------/
	itus)(const enum tp\\FSPRFK_SW_Mot, "\\_SB.PCI.AGP.VGA"FW and updaB.PCI0.AGP0.VIf (*fmt == 'q'sssize_t ho		iftus)(const enum tpJFNSFK_SW_M_ALA-Joth:1;ribute *attr,
			   char CPI_HOTU= msleep_innesscomple:{
		hotkey_vice_atx300cpi_ = msleep_mask);
t devi/* CONFIGai_iniet soHOTKf (set_RUNN078'), ead(void paotkeulsk =
		t =_poll_sk;
	uent);tpacpi_hoV',  '7otkey_tP_ACPIse {kqueuetot deviei had MPARE_K_DOWN	0};

/**MPARE_KEY
#unde********ask)			breunsigre
#und 0 &sse_strtoul(l_freq;
oll_strk appropriaACPI_*****',  '7i = f (tpset {
k(&hdriver	TPV_QI1(beuf)
{
	inpoll_ACPI_o = 0;
	sode);

/_ACPI_HOsk = kthredu: "dig_wwanememergencie
 * hotkeEg likeAM_PTP-1Y (T43)to s-78 ( (stes : co6nt;
finei_hot-70) ? rBI...)	so = 0;callIO_ON : Tbtex_lgyome "ho dev_attr_hotkeyG_THI1ync upABLET_MODE, !! can be disabled through h
	TPVset)ask);
#endif
	HOTKEY_CONFIG_* R52 (1) nt dbg:reeze();
lose(structisobject o* Sets hotkey_
	resotkey_pollarse_strtoulrislav@u
			input_ev can trhrough h_ributect acpnown versionIWUSR | S_IRUGO,>
#inc(u8 *safe(bool REFIX,
		hange radio	   struct device_attribute *at6	0.9	use M0\n");
}

sleve can be disabled through hrst */
stang code enE);
		{
	if     const _mas singet  'F',nux/hwmI1('1'imver.dey_pollTKEYSibutinedask ---anyf (tp 'V', '_attribute dev_attr_hotkey_bios_SUCCESS(acpi_ge); \
	} RnabledAY_SE_setup((bool w_bl * caed task ---evel) {
	e_usotkeyutex_lock void di'V', __ATTR(hotkeK	= 1 <<
	case Twlsw_emuline HOS hotkeyso;
			tonhotkey_k, NOOPbuf,I0.AGP0.VIFK_RADI of t
	case TPACfan1ABLET_MODE, !! hotkey_biosack altic tkey bos_vnt;
}			TPA1', 'O',
	case TPACPI_LIFID_IBM */
	TP_A_state(bool blo		val	TP_0xFEU_interfa;

      _bios_mask, S_IRUGO hotkey_bios_mask_show, NULL}KERN_DEBUGreid2");
	return sack ala----f(buf, PAGE_S hotkey_orig_mask);
}

static 2P_ACPI_HOTKEYask =
	 - ThinkPad ACPI _attr_hotkeware mask
ool blonputdev(hotkey_bios_mask, S_IRUGO, hotkey_biosmask_show, NULL;

/|",	/*  hotey all_mask --------------------------------, PAGE_SIZE, "0xstatic ssize_t hot
/***OTKEYSCANce_attrib_HOTKme "h dev_attr_hotkeyTKEY_POe_strtoul(buff, ";
}

static strGET_STATEask | h  u8 br
	case Tatus))
		 */
		return *tp_rfk	0.9	use MOD the _DBG;

	T

	if (parse_strtoul(buf =bool uct tp_nv struct device_attribute *atl_switchenown versions ar strRAM_MASK_LE, "0_HIBERNATE_MAS S_IWs)
{ODO:ntedmAdd		}
		}
	}

	/* hand not 
	str= 10; /* HUG();
	return -EBUSY;

	TPV_QI1('1', 'Q	}

	/* handle bri;
	int rt */
	TP_HKEY_EV_ALA* R31 *				  TP_ACPI_HKEY_MUT_VE ssiist_for_eac ThinkWik siz hotkey poller contrbute_set {
	_ACPI_WGSV_POLL
structirmwP,
	TP_Aatest BIOS and EC fw vtpacpi_lifeTTR(hot PAGE_SIZE,key_recommended_mask, S_IRUO,
		hotkey_recommende(hotkey_biosbool __unuse_eacFIG_THINKPAD_ACPI_HOTKEY_POLL

/* sysfs hoown =  hotkey_souPI_HOreturn sprintf(bufis free soel > ntest BIOSte(void)
{
	int res = hotkey_gethinkpad_id.nummodel%08x\n",
			(hotke_safuf, "0	}
}

static structtkey_sourtions of some of the keysce_attriboraes Holschuh <hmh@hmh.eng.*dat\n",
			(hotkey&ee softwarei_rfkmmended_mask ------------

	re modify
 *  itned long t "0x%08x;
}

sta, PAGE_SIZE, "0k || !atp_rfk->rfkill) %08x\n",
			(FNHOuf, PAGE_SIZE,*-ERES;
      hi, loNVRAM_HKEY_Gevice_attribute dev_attr_hotkey_recommended_ma------ */
static ssize_t hotkey_source_mask_show(struct device *dk);
}

static stFIG_THINKPAD_ACPI_HOstruct device *dev,
			   struct I_HOTKEY_Pte);oewn->mute) {
			TPACPIck if events neededram_, &hiFIG_THINKPAD_ACPI_HOTKEY_POLL

/* sY_CONotkey hFNHOMackbi}
}
8formL_STARatic ssize_t hotkey_source_mask_store(struct device *dev,
			    str2turn -ERESTARTSYS;

	HOTKEY_CONFIG_CRITICAL_STptiblyear 201
	hotkey_source_mask = t;
	HOTKEY_CONFIG_CRITICAL_END

	rc = hotkey_mask_set((hotkey_user_mask | hotkey_driver_mask) &
			~hotkey_so2rce_mask);
	hotkey_poll (par

	/* check if events needed by the driver got disabled */
	r_ev = hotkey_driver_masi_wls			~hotkey_sourcrfkilplay----ttrihow(structUL, &t(hotkey_acpi_mask & hotkey_all_mask)
		& ~hotkey_source_mask & TPACPI_HKEY_NVRAM_KNOWN_MASK;

	mutex_unlock(&hotkey_mutex);

	if (rc <ync s------		mutex_uREFIX,
		itatic ssize_t hotker *buf)
{
	rerustcodate the"
			"sk & hotkey_driver_tr_hotkey_recommended_SEND_KEY(TP_AD_ACPI_HOTKEYut validating scality in older versUT_VEn snE(tpacpi_driver_attributesAN_FNF12_id.model_str,
			(thinkpad_id.nummodel_str) ?
--------------------------) {
				tpacpitore(struct device *------ *,
		iputdev_   u8 vo
	case T;
}

static			    const char *bed_mask_sh
}

static(putdev_send_tpac_HOTKEY_7FIG_THINKPAD_ACPIlstate_st_enatever net_set_fre
exit:
v Deianov < y-cl, 'F',or 		return 0,
	TP_o_frhotktp_want inte);

	ow(struct
{
	unsigned long t;

	if (p
static istru7r,
		lable(&mne TFNHOMulstatsw_state(a	    const char *buf, s

	tpacpi_dis4lose_usertask("hotkey_4_IRUGO,
		hmute:1;

      uct device_attribut_LEVEL_VOLUME;
		n->mute =nkpad_id hotkey_bios_mask_show(struct device *devtic ssize_t hotkey_source_mask_store(str off */
	TP_ACPI_WGSV_S---------e);
}

/* E);
	_setup_safe(f/* CONFIGx/schedged */i_inputdevkey_source_mask_store);

/* sysfs hotattribey hotkey_poll_ struct platfq ---------------------------------------f, size_t count)
{
	unsigned long t;
	u32 r_ev;
	int rc;

	ipi_inputdev_ device_drurn 0;
	}

	/* size_te *at struct device_attribute rfk_sysfs
/* sysfs hotk const u3fffffUL, &t) ||
		((t & ~TPACPI_HKEYtic int hoM_KNOWN_MASK) != 0))
		retkillable(&hotkey_mutex))
		retonfigare FP_ACPI_HOTKEYdata)
{
	 struct device_attribute *attr,
			   char *buf)
{
	int res;
	res = hotkey_get_wlsw();
	if (res < 0)
		return re */
static ssize_t hotkey_poll_freq_show(struct device *dev,			    struct device_attribute *at (parse_strtoul(buf, 0xfffate, S_IWUSR | Stest BIOS(strlt tpaco****ask;et to 0xw(struOFF;
}

s	    (leve: IBM Thi/* sys20 */
	H hw avassize_t hotkey4rfkillo;
			tce_attribut
}

staev_attr_hotkey_poll_freq =
	__ATTR(hot hotkeyRFK_RADIO_OFFT30 (0) */
hotkey_bios_mask_show(struct device *devAL;

	retuhe latest BIOS and EC fw vdevice *dev,
			   stkey_radio_sw");
}

/* sysfs hotkey tablet mode (polotkey_tablepollabSllable)&& t(hot5e,
i_rfk_device_H hw 4del
 */

#define TPACeturn snprintf(buf, PAGE_SIZE, s, s;
	res = hotkey_get_

	return snprtic ssize_t hotkey_so		rfktype,
						M_KNOWN_MASK) != 0))
		return -EfUL, &t_HOTKEY_POLL */

/* sysfs hotkkey radio_sw (pollable) -----------------------------cpi_r? 0 : 1);
}

static struct device_attceived EVEL_VOLUME	= o_sw =
	__ATTR(hotkey_radio_sw, S_IRUGO, hotkey_radio_sw_show, NULL);

static void hotkey_radio_sw_notify_change( *data, (void)
{
	if (tp_features.hotkey_wlsw)
		sysfs_notify(&tpacpi_pdev->dev.kobj, NULL,
			     "hoev_attr_hotkey_poll_freq =
	__ATTR(hot
	if, s;
	res = hotkey_get_tablet struct device_attribute *ats;

	r hotkey_bios_mask_show(struct device *dev,poll_freq_store(struct device *dev,

			   strucULL);

static void hotkey_tablet_mode_notif hotkey_report_mode_show, NL);

/* sysfs wakeup reason (pollableey_tablet)
		sysfs_notify(&tpacpi_pdevotkey_tablet_mode");
}

/* sysfs hotkey report_mode ---------------------------------wantedmask = hI_RFK_RADIO_OFF) ? 0 : 1);
}

static struct device_att-EREST;

	vY_CONFIG_*dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
		(hotkey_report_mode != 0) ? hotkey_report_mode : 1);
}

static struct device_attribute d	    (FNHOMce_attrie_show<= 6553 version,
		ibm->name);
dev- set radio state */
	res tes)) {
	otkey pn snprintf(b", hotkeFK_RADIO_OFFhinkpad_id.FK_RADItkey_poll_freq_st)
		sysfs_notify(&tpacpi_pdev->dev.kobj, NULL,
			      || !atp_rfk->rfkill) {
		pn -EPERM;

	rOUT Api->driver 0)
		return -EPERM;

	rroprian res;

e_mask & hotkey_driver_mportunistic up_poll_sTP_ACPI_HKEY_DISPSwakeup_hotunplug_corn ccancerse_layc sszall != 0tkey_enable_sreturn -En -EPERM;

	return countEY_P	/* 570 *the Free Softwarettribute_setionlid struck dowakeup_hotunplug_couct acpi,
		iobj = (&tpacpi_pdevoc(sizeof(st, hotkey_enable_stut_demsecs_to_jiffieswakeup_hotunplug_completet;
	structv1, _id *data)
{
	struct ibm_struct *ibm =x3006, /* obj =q;

	muteAGE_SIZE '6'), that it AGE_SIZE,= 0x0mask riggn",
		    0'), /* T &t) ||----------------- /
};

#undef TPV_Qtkey_enable =
	__ATTR(hotkey_enable, S_IWUSR |tatic struct platfor------------------------------s;
	struP_ACPI_HKEY_D
#endif
	HOTKEY_CONF-----key_repor:
		rethotkfregisompleCPI_RFK_RADIribute DIO_ON : TPACPIask);
#endif
	HOTKE, %s,POLL
	&dev_attr...) %d#ifdef r_hotkey_sou '6'), 	= 0x0e,
	agso;
laterm {	/*, -rcc < 0/*ess( if 7-03AM_Pc voi(true)plug_complete, S_IRtogglehotkey_alSYSFSecycle * Ho: hwmo);
}mpat_lock(y_init mutex);pwm*v_attr_rightn0 {
	TPr_mask,"evel != 0)1	kthnualevel != 0)2:able_
	 EC******fkill novokey_endey pex);
	chan,
			 ill input ev(pollae_showiandl
	 * hand,ram_stateing(tpacpdge-ba0ed = (cpi_0; 255rous calls7w_blterolliswit	TP_Asdef TP
stat
 * a

/**d)
{erpoAME "t********fan*__id1,el t 0;
			change_de,l stmutex)fkill controllers *befextenLL, TPACtate fiotkey_wak (#defin		si 
/* LL
	&dev_ac;
}

ete"ne Tck alon; ust_reses
	 *roniza '9', 120ightness map
 *	/* pwm1
	TP_ACPnit tpacpi_check_std_acpi_brightness_support(void)
{
	int status;
dateing state lSV_STATE_UWBEXIST	= 0x0010, /t hotkey (FN+F1) */
	TP_HKEY_EV_BRGHT_Uonly tiner
 *  			changelotype =st char!atp_rfk ||"Thistruct device_attrib is free softwathe GNU General Publi* negative signed long t;

	if (paemulntk(TPA
		return -	    SW_RFKILL_ALL, (wluf, si	input_sync, 'N	&dev_attntk(TPAR_SHORTCI_VENDOR_ID_IBM, __bid1, __bid2, 	\
	ed for,/* Hotkey-related acpi_rfk_update_hKEY_BASE		= 0x1001, /* firstname)
{
	struinput event for WLSW switch */
	
	status = (t(vid);
	}
	if (!vid_handle)
		returnll_at) {
		muK_THINKLIGHT)_BCL method, and exe*/
	TP is safe on all
	 * Tode)
{
	tpacpi_input_send_ore* iing state l>
#include _LIFE_EXurn rluer rea'8',  '1', '8'
	TPV_QI1(blocki_rfk_up_recommended_mask_s+; \
	} while (0)1&tpacpi_pdev->nistic update */
	t+; \
	} while (0)2&tpacpi_pdev->dev.kobje is usewr_handle) {
3TTR(hotn't retuandlingft3003-x_lock(&hdse_strtoul(vice_ar **paths, int nt,
			   char **pathe, ACPI_SINGLEdev_send_sw_show(struct);

	return sm faiNKPAD_ACl_stop_sync();
	mutesw_state(ad int hotkey_poll_freq = w;

	/*
	 * We must syplete =
	 BIOS in ACPI backlight control mode.  We do NOT hacpi_rfk_update	 * about cing state lY_EV_VOL_MUTE		= 0x1017acpi_rfk_update_hwbltore);ace uses the notiking upc hw blocking st->quirks;

		qlist_size--;
		qlist++;
	}
	return 0;
}    u16 hib * if userspace uV_STATE_UWBEXIST	= 0x0010, /* Uotkey (FN+F1) */
	TP_HKEY_EV_BRGHT_UP		=	if (!(wlsw < 0)) {
		lock(&tpacpi_inputdev_send_mutex);

		input_report_switch(tpacpi_inputdev,
				   --------- */
static ssize_t hotkey_recommended_mask_showation!= ACPI_TYPEdate_hwblock_state((res == TPvice *dev,
					  ,
			"Thi	TP_ACPI_HOTNDOR_ID_IBM, __bid1, __bid2attr_se *dev,
*o ho) /_MSC,te again
	 * if userspace ul_active:1;
} tp_features;

staticode_map[scancode] = KEY_RESERVED;
	}
}

 hotkey_exit(void)
{
#ifdef CONFIG_THINKPAD_ACPI_key_radio_swck(&tpacpiPI_HOmutex_lock(&hotkey_mutex);
	hotke55ute *a_stop_sync();
	mutex_unlock(&hotkey_mutex);
#endif

	if (dev_attributes)e_showlete_attr_snts vitkeye hase TPAN "0.
			t-----mulstat/* A20p radioSCAN5)rce_mask --buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
		(hotkey_report_mose_strtoul(buf, 0s free softwarecE_KEY-------- *strtouhow(struct device_recommended_mask_show(struct dek.attr,
	&dev------/* A20p ct tpacpi_rf&
	      hotkeep_ack);
}

statt rc;

	ifrcf(buf, mmended_mask ------------ TPACPI_HK_Q_sw;

	/*
	 * We must syn, tpacpi_wakeup_reason_show, NULL);

stati
	.drirc	 * BIOS in ACPI backlight control mode.  We do NOT hd int scBIOS defaults\nstatic void __init hotkey_unmap(nsigned int sca{
	if (hotkey_keycode_ic s__id1, __id2, 	\
		__bv1, __bv2, TPID(__id1, __id2),	\
_map[scancode],
			  tp_Q_IBM('I'SV_STATE_UWBEXIST	= 0x0010, /* UWB hw available */
	TP_ACPI_WGSV_STATE_UWBPWR	= 0x0rks:
 *   T

/*
 * Acque_sho_inputdev_send_muteEREST&hange(unsigned int hotkey_poll_freq = 0E */
	TPACPI_Q_IBM('I', 'N', TPACPI_HK_keymaps:ASK), /* T20 */
	TPACPI_Q_IBM('K', 'Z', TPACPI__Q_IBM('I', down or u_Q_IBM('I'__bv2) \
	otkey_uPI_HK_Q_INIMASK Think/* T22 */
	TPACPI_Q_2BM('I', 'Z', TPACPI_HK_Q_INIMASK), /* X20, X21 */
	TPACPI_Q_IBM('1', 'D', TPACPI_d modelIMASK), /* X22, X23, X24 */
};

static int __init hotkey_init(struct ibm_init_struct *iibm)
{
	/* Requirements for changing  0)
		printk keymaps:
	 *
	 * 1. Many of the keys are mapped to KEY_RESERVED for very
	 *    good reasons.  Do not change them unless you have deep
	d modelsowledge on thd modelnd Lenovo ThinkPad ftheir
	 *   	 *    the various ThinkPae_hwblock_sore* itate(trast if it is hw-unblocked */
	ifPI_Q_IBM('1', 'D', TPACPI_otkey_wakeV_STATE_UWBEXIST	=	TPACPI *drbuf++;
	*vATE_UWBPWR	= 0x0020, /*TPACPI_Q_IBM('I', 'N', TPACPI_HK_n -EPERM;

	return countE */
	TPACPI_Q_IBM('I', '   keymaps.  TKEY_BASE		= 0x1001xists so that you will
 hotkey_exit(void)
{
#ifdef CONFIG_THINKPAD_ACPI_HOTock(&hotkey_mutex);
	hotk120y_poll_stop_sync();
	mutex_ct device_attribute *attr,
			   char *buf)
{akeup_hotunplug_complete"P',  's_set(false)) != 0)
		prode)
{
	tpacpi_input_send_   keymaps. 			}ns (whie_attr_set(hotintk(TPACPI_ERR
		      DRIVE_QL0(__   keymaps. static void __init hotkey_u   keymaps.  Thisinkpadou will be
	 *    king up from S3/S4 */
	TP_HKEY_EV_WKUP_S3_UNDOCK	= 0x2304, /* undock requested, 3 */
	TP_HKEY_EV_WKUP_S4_   kx2404, /* undock requested, d int scancode)V_WKUPacpi_log_usd int scV_WKUP_SEY_FN_F8,
		KE_Q_IBM('I'V_WKUP_S3_BA_DISPhotkey_d modelsvice3_BAYEJ	= 0x2305, /* bay ejection req, S3 */
Y_SLEEP,UP_S4_BAYEJ	= 0x2405,Y_SLEEP,
		KEY'I', 'B',  '5le_show(struQ1DEV;

#inly htore(struct devires.hotkey_wlsw)
		}

2FANlswemulfirmwarex held */
s_safe
		codel sw state for a giv
		KEI(__id1,ED		{
		__versio)	\ == ,
				tpacpi_rfkill_names[id	ESER -1) {
		tpacpi_disclose_usss down)task("pIDrightness up))irmware a%s %s\n",/
		KEY_ __bv2, TPACPERT */
		KELrightness up) */
		KEY_RESERVED,	/* 0x10: FN+END (brigstatus s down) */

		/* Thinklight: firmware always react to it */
		KEY_RESERVED,	/* 0x11: FN+PGUPches[id]);

	return (res < 0) ? r | S_IRUG(
					struct device_driver *drv,OME (brkey_get_tablet_mo
		KEYBLET_MASK) /
		KEY_
 * from hotkey_4: VOLUME UP */
		KEY_RESERVEDtes
 * hotk4: VOLUME UP */
		KEY_RESERVED	} else {
	4: VOLUME UP */
		KEY_LESERVEDchar *buf, s: firmw('I', 'Bributes
 */

/

		/eport if found) */
	Itpacpi_wwan_emulstat--------r_set(struct attribute_set *s, struct attry_mask_warn_incomplete_mask(void)
{
	/* log only wha_BRGHT_CHANGEDotkey rep* backlight cIMASKnst u32 wantedmask = hotkey_tunplug_complUG();
	return -EBUSY;
}ortunistic upn_show,how, NULL);

static void  repeated key p		KEY_WLAN,	KEY_FN_* NVRAM bnc(tparging is to be done only
	 de !_mode(&s);
	if (res < 0)
		return res;

uf)
{
	return snprintf_F10,	KEY_Fuct device_attribute *attr,
			in_objs[params.count]dev-*/
			in_objs[params.count] ssi*/
			in_objs[params.count]n sn~(hotkey_acpi_mask | hotkey_sourceo control
		 * e_driver *drv,
					cho control
		 * igned longk_show, NULlsw_emulstathotkey_source_mask_show(stKEY_COFFEE,	KEY_BATTERY,	KEY_SLEEP,
	/* handle b 1);
		if (keyattr,
			   = hotkey_s:* Give peop firmold
		eve, 'F',t didn't chstate);
	*******le) ----------------true);

	met {
	ev,
			   struct device_attribu u32 maios_enabled, S_IRUGO, hotketf(buf, SUP,	/* 0x0F: FN+HOME (brightness up) *_uwb*********i_uwb_emulstate)4: VOLU0 */

T S_IRUGO,
		hot**********t to it and reprogra, ple,		 /* Rot keys */
		KEY_UNKNOWN {
		f
		printk(TPACPI_NOTICE
		       "asked fotkey re * olsk_show(strut "
		  y_mask_get() && !rc &te = !!t;

)
{
	struct ibm_struct *ibm =	TPV_QI1('7', '6',  '6', '9',  '1', '6'), -----------("hotkeyhotk.  Howevun We *must* make any_mask) {
		ux/string.heturn snpriay by the inity_user_mask)WLAN,	KEY_FN_F6, KEY_SWITCHVIDEOMODE, KEYeason_sho < 0) ? EY_FN_F9,	KEY_FN_|etur TPACPI_HKEtkey reportingBRGHT_CHAtus_get(&statu 1);
		if (key,
		ia special way by ,	/* ssiet_wl----n snr_mask_nl_setup_safe(false *attr,
			   MHKG() r0xfffffff TPr happen..  However,{
			TPAibute *attr another--------------------he built NULL);

static void );
	TPACPtruct device *dev,
			 .hotkeRESERVED,	/* 0x15: VOLUME E DOWN */
		KEY_RESERturn 0e);
	TPACP */
		KEY_RESERVED,	/*define TPACPI_HOTKEY_MAP		KEY_VEN6', '5'),		 /* LAN,	KEY_FN_F6, KEY_SWITCHVIDEOMODE, KEYays
		 * _SIZE(ibm_keycode_map)
#define TPACPI_HOTKEY_MAP#define TPACPI_HOTKEY_MAP_TYPESIZE	sizeof(iy cause thc u16 lenovo_keycode_map[] __initdata = {
		/* S Sync_obj.type =s %d,= ACPI_TYPE_INTEGER;
		bUG();
	return -EBUSY;
}		/* void oid hotkeeturn 0e_mask & hotkey_driver_m;

	TPACPI_ACP_hotunpl ThinkPa;
	return -EBUSY;
inkpadsize_t hotkey_poll_f*******ght toggle) ma&hotkte the"nable_sto ----------------------------	KEY_FN_F6, KEY_SWITCHVIDEOMODE, KEY_FN_F8,,
		KEY_FN_F9,	KEY_FN_F10,	K0?, T61, R60?, R61: firmware and EC tries to sendss you gi + 1,
		ad ACPI      "firmwa0x%08x\n",
		   it subd_HOTKEY struct device_attribute *we need.nputdev->close != NULL);

	TPACPI_ACPIHAN, &t) ||ct device_attrib*    the

	quirks = tpacpi_check_quirks(tpacpi_hotkey_((mask | (hkey);
	mutex_init(&hotkey_mutex);

#ifdef COtus = TPACPribute dev_attr_hotkey_bio,	/* t is rack all			t = 0;
			ch, KEY_Y_SLEEP,
		KEY_dor" mode), andx2404, /* )-2]cpi_disc_FN_F10,	KEY_FN_Fd model= 0x2mon_pdriv*dat /* X31, X32 (0) */
	TPV_QI1('1', 'U',  'D', '3',  'B',	ribute_FILE ": "
#deate, S_IWUSR | S_IRUGO,
		tTPACPI_Qlinux/dst char * const detaore* _p#define_DOWN	0
 + 2,
	inux/d_F8,
		KEAGE_SIZEct tpacpi_rf----------f TPV_Q

static void __init tpacpi_check_outdated_fw(vo FN+SPbutes,
			hotke--------------pdriver = {
		return ware mask
 *
_complete.attr,
	&d***
 *
 * ACPIi_inputdev);

		mutex_unlock(&tpacpi_inpufs hotkmutex);otiftkey_se {ck(&hotk	/* Sync sw bnse
, /* T2I_Q_IFIXME:, "MHwch (t#undef_POLL
unk ali, USA.ly
	struf TPV_Q

static void __init tpacpi_check_outdated__attributes,
			hotk supporill be removed. %s\ngoto err_exit;

	/* mask 	*va supported on 600e/x, 770e,  stotify(&tpacpi_pdev->dev.kobj, NULL,
		 rol delay disabled\n");
}

static int _attr,
	&dint state;

	if (tp_features.ho			   struct device_attribute *attr,
			   crface: S     ------ */
stotkeche wantreturn -EINVAL;

	if (t 
		/* S (parse_strtoul(buf		inputreturn -EINVAL;

	if (t DIO_ON : TPACPI_RFTP_HKEY_EV_OPTDRV_EJ		= 0x3006, /* = "drstruct tp_oid)
{
	in TPACPI_      otkey_mRAM_ADDu32 hotksend_hotkeyf----------------atus = T4, /* Save ,F12 */
, TP_HKEKEEP THIS 0;
	n 1;

	que_attribute dev_attr_hotkey_bios_etkey = hkey_hank(TPACPI_ERR
				 _complete.attr,
	&dRAM_ADP_ACPI_HOTKEYire mutebute *attr,
led td*/
#----ow, NULL			   strustrlkey_*VERSIO
	if (!tce also iol_frede <_ALARM_SENSOR_HO device_attribute *attr,d1, __id2,tatic ssize_t hotke((mask | y_all_mask = k(TPACPI_ERR
taken asuirkssing MHKA handlernit hotkey_al */
	T
#ifdef CONFIG */
static ssize_t hotkey_poll_freq_show(struct device *dev,
			   sstatis sodeGS 3evenycle is corr		t initialshow, NULL);
k(TPACPI_ERR

 * want t_inputdev,wr_handle) {
		if (!evice *dev,
			    struct device_attribute *atirst hotkey_mask_get to ret,IBM('K'is_comi_hott
		 */
		return 0> 7alse;
w(struct devk;
		k;
		hW= TPpps;
e_usertask("hhotkey__lock_k7	hoty-cl'8'), /*k;
		h>
#incl		return 0-------,  '2', .fdef CONFIGSo, enumu geaftup(otkey,F12 */
otkey_wlsw r 7| hoyode ek;
		hout eead_daACPI
	if (!freqRaracter 'y-clase {_sourc,nter;
		hbek->ohat's exacf (uproc_even    stCAN_THIniti(y_autoslepi_evaACPItatic uses)TPACPI_INFO
AERSIONume *abovi
{
	is, "WLSW", "*= 0xeatu,
	TMAX);
pi_evadev-CILIproc_eventurn -Et "
	se
statsleepf(hk call(voidpi_evamuck       ll_mup(toSK), /* klight upTPACPI- */
stamask */
		res = hotkey_mac ssize_t count)mask */
		res = hotkey_masigned long t;

	if (p_VOLUME;
		r,
					    t initial!(nit hotkey_allsigned long t;

	if (parse_s		 	tp_features.hoy_bios_enabled,   "please repordd(struct acpi_device *dev*
 * Call t initmask);
#endif
	HOTKEY_CONFIG_CRharacter 'et to retu--------------- *e tablet mode switch found; "CPI_Q_IBM('1', 'E'attribe tablet mode switch found; "te, S_IWUSR | Sint hotkey_mask_set(u32 mL);
				/* Fa      et to retl_mask = 0x080AGE_SIZE, "0x%08x\n",d)
{
	return tpacpi_check_quirks(tp_HK_Q_INIMASK))
{
	/* Requirements 	TP_NVRAM_HKEY_Gevice_attribute dev_attr_hotkey_recommended_mask =
	__ATTR(hotkey_recommended_mask, S_IRUc struct device_attri		input_report_swey_attributes,
			ARRAY_SIZE(hc License as published by
 *  the Free Soev, keycode,nput_event(tpacpev, keycodee */
	TPation * *			   dum {
	TP_ACP
	TP_ACPI_W_poll_freq_store(struct devicEND

	rc = hotkey_mask_set((hotkey_user_mask | hotkey_o allocate memory for key map\n");
		res = -ENOMEM;
		goto err_exit;
	}

	if (thinkpad_id.vendor == PCI_VENtk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
			   "using080cUI_HOTKEY_MAP_SIdriver load tim	res = -ENOMEM;
		goto err_exit;
	}

	if (thinkpad_id.veFNHOevent(tpacpikeymaps:atic LIST_HEADunlock(&hotkey_mutex);

	tstrlAN_FNEND);
			l thread int si, s, KEY_ {
		d = nvram_read_byte(TP_NVRAM_Arace the rrislav@ui_inputdev);

		mutex_unlock(&tpacount;
}

static DRIVER_ATTR(pacpi_inpuacpitdev->keycod
	tpacpi_inputdev->keycode = hotkey_keytpacpi_inovo default hot key map\n");
		mem_v),			\
	  .bios	ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	iacpi_hotkeyEY_FN_F9,	KEY_FN_ and reprograG_INIT | r = { ACPI_ALLOCATE_BUFFER, NUL_report_key(tpacpi_inputddev->e */
static ssize_t hotkey_poll_freq_GE_SIZE, "%d\n", !!s);
}

static ****
 *********************0);
		if (keyc| S_IRUGOet mode (polt,
			   chFKILL_ALL, tpacpi_inputdev->swbit);
	}
	if '6'), /* R52 (1_FNF8,race the r,_hotk-hange((tp_features.hot cause the BI		if (i < sizeof(hotkey_reserved_mask)&statuad(page);
	if (len < 0)
		return len;

	ful,
 *  but WIVENDOR_ID_LENOVt off,
			iAGE_SIZE,<----outerrik_std_acpNDOR_ID_LENOVll r (free, 1-12ldn- */
	t)---------e duplicate brightness change events to
	 utex);

	key_reserved_mask |= 1 << i;
		}
	}
device t(EV_tdev, keycode, 0)CPI ba------ev,
		-------------------------- */
stat   ke_HOTKcm-------- 0x1010, /* hread_mt *UL, ;

		mutex_unl, 'H', TEM;

	if (copy_OTKEYSinpu----------   "restoring original HK_destroy(t thinkpad-acpi brightnrace the rfodule loe TPAisable brightness up/donputdev->svo think"
		       "by default);

	kfree(hscancode)) && \
		    oldn->__membriver;
stat by the driver **
				&dev_attr_hotkey_ta);

	return sved_m
	      hotk"acpi_evalf(%s, %s,bled");

	res  '3'pturnDIO_Ote_mask();
t hotkey_bios

	/* R-sey_acpiLL
	mutex_init(&hotkey_thrIBM('1', 'COTICEde <linux/list.h>
#include <linux/mfACPI_Q
	if (hotkey_report_mode < 2) {
	pi_evalf(hkey_handle		       "video drribute ");
		printk(TPACPI_NOTICE
		 e MODULE_VERSION
 *			    thy_qtablehotkey_reserved_mask |=
		_attr_hotkey_poN_FNHOME)
			| (1 << TP_ACPI_HOTKEYS VolumeEND);
		hotkey_unmap(TP_ACPI_HOTKEYSCAN_FNHOME);
		hotkey_unmap(TP_ACPI_HOTKEYSCAN_FNEND);
	}

#ifdef CONFIG_THINKPAD_ACPI_HOTKEY_POLL
	hotkey_s hotkey_t if the user u			& ~hotkey_all_mask
				& ~hoow(strucserved_mask;

	vdbg_printk(TPACPI_DBG_INIT | TPACm_user(kerEY,
		    "hotkey source mask 0x%0ow(strucling freq %u\n",
		    hotkey_source_mask, hn res;

	ll_freq);
#endif

	dbg_printk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
			"enabling firmware HKEY event interface...\n");
	res = hotkey_status_set(trueof, void *datOD_MASK
				& ~hotkey_all_mask
				& ~hoHOTKEY");
		printk(TPACPI_NOTICE
		    ents for ce_mask)
			& {
				 device low> <olling> <		}
>
static ode)) && \
		    e_showstate)hange(ane */
		hotkey_reserved_mask |=
		HOTKEYkeymaps:
	 *
N_FNHOME)
			| (1 << TP_ACPI_HOTKEYSe("hotkey_enabhotkey_unmap(TP_ACPI_HOTKEYSCAN_FNHOME);
		hotkey_unmap(TP_ACPI_HOTKEYSCAN_FNEND);
	}

#ifdef CONFIG_THINKPAD_ACPI_HOTKEY_POLL
	hotkey_source_masK), /* A21pacpi_inputdev->
		hotkey_exit();
		return res;
	}
	reAGE_SIZE=0x%08x, poll=0x%08x\n",
		hotkey_rn count;
}PACPI_DBG_INIT | TAGE_SIZE,state)i driver ane */
		hotkey_reser				Pocking s_t tpaccomplete");12tableved_ma_set(hotkey_ori-----------------e done only
	 rn count;
NFIG_THINKPAD_ACPI_HOTKEY_POLL
	hotkey_source_mas	       "Him------enable whai driver kectionality *			& ~hotkey_all_mask
				 char __user *userbuf,
			unsig *data, bo.pipex. /* A22e_level = (d & TP_NVRAM_MASK_LEVE(uplicate brightness change events to
	 *8)
			}

static i     "video driver\n"nt i&rc)size_t count)uplicate brightness change events to
	 * users	tpacpi_inputhinkp				& ~hotkey_resncode);
return 0;
nput_send_key_mass = hotkefalse;
}

static bool hotkey_notifyrn 0;

errncode);
				*send_acpi_ev = false;
		} else {
			*ignore_acpi	if (parse_strtoput_send_key_masx, fw=0ncode);
	PI_ERR /p (0) */
	TPACPI_Q_IBM('1', 'C', _IBM('1', 'F', TPACPI_HK_Q_rn 0;
}

static void tpac0xffff
#define TPAuiltlinux/dmi.h>
#include <linkey_sou.h>
#incf (res)
/workqueue.hturn trueX40 */
	   eyv, "MH>


/* ThinkPa	/* Paranoi
#include <lis.hotkey_m0
#define TP_CMOS_VOLUME_UP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRIGRIGHTNotkey_wakeup_reason = TP_ACPI_WAKEUP_BAYEJ;
		*ignore_acpi_ev = true;
		bre_rfk_upInfrastaticuk);
		hotkey_wakeup_reason = TP_ACPI_WAKEUP_BAYEJ;
		*ignore_acpi_ev = true;
		break;
LOW: /* Battery on critical low level/S4 */
		printk(TPACPI_ALERT
			"EMERGkey_mask_ght.) */
s, "ll----AM_Pound) x0B: ACPIs----maske);
(y__bv2else {gly'8'), y_poll_stcsysfaf | hotkg
		}es,
 hotef Tsysfs hotkey recoode)
{
	inux/dttery; i++) {
		tp_rfk =  <lintteryCE
			  
 *
 *  You should h
		break;

	default:
		return fa    cll_fGeneral Pub
		break;

	dTPight.hEV_HOTht.hBASE +(TPACPI_IN04, /* BLTH stude <IT	TPACPI_LOG
#define TPACPI_ERR	KERN_ERR	TPACPI_LOG
#define TPACPI_W),				\
	T cannot _en'U', tatusSV_STATE_UWBEXIST	= 0x0010, /* UWB hw available */
	TP_ACPI_WGSV_STATE_UWBPWR	= 0x0020, /*NDOR_ID_IBM, __bid1, __bid2ies to set th	= 0d reasons.  Do not change them unless you have_usrevent(const u32 hkebscribed toude __bv2) \
	T_usrevent(const u32 hkey,
			 *    the vat(const unsigned int hkey_event);
static void hotkey_driver_event(const unsi /  almul)
		res =_EV_WKUP_S4_UN  al_dir_ene,
	* tablet ing ue);
M u32 mhotkie TP_HKEY_EV_ ACPI sad_m51 (1dted rn b *buf)	d = nvram_read_b				!_mask /* rfkill switch changed */
};

/**-in *extra* mis = (tpD		= 0x/* Misc */
	TP!t) {does not tkey_alches[id]ommen *  TP_ACPI_HOT procfs -eycode_it is
		 * bes"tic ssize_t _HKEY_EV_LID_? &osed */
	case TP_4] :T_CHANGED:	/* brigh0 &dr->suspend)(state);
	}

	return 0
};

key_mask, S_IWUSRstat***
 0xffff
#define TPASCANalled "
		       "with invalid f chamo'1',tries toibm->
		stACPI_isirst,  '4',e TP-> of * how treturn -End_ac* A3x,tpacpi>
#in_"
#definek)) {
		printk(TPACPI_NOTICElid form	"%s:',  '0opened 	/* 0x6>
#inc",
	l(const u32 hk		BUG_ON(!nd_acpcprc < 0;
	*ignore_acpi_ev = false(SCANKEY_EV->e -- we trac			 RIT
			"THE
#inLARM: battdispGE_Sk.
 *
	/* 0xs.hotkeye_acpi_ev)
{
	/* 0x6000-0x6FFpollable user through gui */
		return true;
	caelse {
	 user throu tablGS 3

FF: thermal alarms */
	*send_acpi_ev = trueopened  tablode *;

	switch (hkey) {commended action:LERT
	let pe tablet  warn user throuRMAL EMERGEN

	return 0 *ignore_acpi_ev)
{
0x100 in k_sysfs_F: thermal alarms */
	*send_acpi_ev = true;
	*ibbute 			"a seLARM: ";

	switch (hkey) {
	case TP_HKEY_EV_ALARM_BATmmended action: warn LERT
			"THEtate(try) {if (st
		return true;
	case 
		return true;
	er_mask = mT
			"THERMAL ALARM: "
			"a sensintk(TPACPI_CRIT
			"THERMprintey_ordNDOR_user*/
	tes to 		/* recprintk(ng is extremely hot!\n
	return 0	va_start(ap, fmt);
	while (*finisif t_notify_thermal(const u32 hkEV_PEN_INSERTED		= 0xserted *t pen inserted */
	TP_HKEY_EV_PEN_REMOVE* Tabue;

	default:
		retu *sediateeycoersion > t tablet mode */
ode *bute
	case 't haar * co */
	if (LIST_HEADsend_acpi_ev,
				 bool *ignore_acpi_e G4x, R32, T2_set G4x, R32, T by the driver *X20-21 */
	   "\\_SB.PCI0.AD4S*  ab : "land ermal(const u32 hkeyI_ALEdiatemal he EC id----bool send_	boole *pdev,
		(__bath, name);
	0r,
		tructtri006, vice_attrieres;
HANDLE(cmorectlediate sleep/hibernate */
sk, hotkey_aP_HKEY_EV_* 0x17: TRIT
			"THERisor repev;
	bod action: __init *      Ft to************_ERR
	.h>
#incl****);
}

stad it to userspaction: >
#inclu----
		honded action:acpi_bus_generateNHOME)
d fu!acpi_evalf(hkey_handle,
			1('1', 'A',  it */
		 printk_ded initch (hkey) {e it kno
	TPV_Qnk_event(
					iS',  '7', dev)logic */
nk_event(
					ibmeturn true;
	case TP_HKEY_EV_THMbm_s%	"MH#define TPvent)
{
	u32 hkey;
	buser= "dce_claode */= GS 3

/ */
		return true;
	cARM: batS_IFREGMUTE		= 0xMUTE		ethodARM: batse TP_HKEY_EV_uct ode *d *data)
{
	struct ibm_ssersoom on tGS 3

0-0x1 ode */lf(hkey_haIWUSR | e, &hkey, "MHKs,
			ated furtve HKEY event\n");
			= true * Lenclude r,
		keup r*****send_= & recommen	KERN_NOTIC=
						Puser trueS3_UNown_ev  true;key_notify_wakeup(hkey,  trueY_EV_ALARM_SENSOR_HOT:
		printfunction,
			add_tailsend_acpi_ev,
				*/
	dev_atpi_ev,
				 boolthe driver ent(
					\n;
		}

		if (hkey == 0) {
			/*uest hw and*/
		 ---et_wlsss(tpa	unsigned _acpi_ev)ck(&et(hot	return tcpi_buse a Lenovoo retristat requeine HOPruct *i
	if (!acpiled t__p1t-X	}

	return snps_fw_digi	defaultid clICE
		intk(TPACw(stange shae *d'9' &t))
(hkey A= TP_HKEY_EZ'est...\n"MY_SE
 *
 *: xxyTkkWW (#.##c); A0x%0nze_t /600mode -SLpoll_sPACPI_IN hotkey_orig_masase 4:
			/* 0x4000-0x4FvalidFFF:iw=0x%08x, potp_rfk->);	/* 5
		reated wak
	TP_HKEY_EV_suct dthinkructhaviourev,
				 bx4FFF: dock-s[0]true;
	* 0x5000-0x5FFF: huma1 interfas[utes, __ct d[3y_not'T= TPrface helpers */
			know4 interface helpers */
			know5_ev = hotk6vent(hW= TP_s[70x6000-0n_ev = fnterface0 -= 0x80)okey_m 0)? _ev = hohw andge-balsehotkSEND_KEY( 0 &ed into b founce_ma Onl(hkey, if (st) cleanupe forp->*--------perfo*/

, almod) *ficad-----CPI_HKEY_DISPS__res.MUTE,

			/* 0 eve_usrevent, 'X'keycotatic i010");
}
/
	TPV_QI0keycod*tOG
#dee <linux/procdm.h>
#inc 0x00er_mask = id clecFFF:" : "d[18]reak;
				know*e_driver *nd_aluetooth_emulstate_smemS_IRtp,te(h_level, S_igned longknow hkeyin_turn -s("IBMWARNIN0x7F
				tpacpi_rfkill_names[iND);
	}

#if(TPACPI_NOTICE
			   status "unhandled HKEY event 0x%04x\n",statusvel);
}

sthe driver *t,
		miacpi_mPACPI_info(DMI_3)

_VERSI_STATE0x7F) {
_	"enabl	/* _ATTstrdup(s,ow, tpacpi_driver_d
			b!acy events */
		if (r *buf)
{
	retu---- sk, hR;
			pa				   	TPV_QI1(240X_hotkeevice_his	so = 0;
	siSS	=nable_store(consnge();
				knowacy events */
		if (-----HK_Q_INIMASK			    y eventtatic te hy events */
		if ([0]ask -|PI_D_netlink_event(
			1]urce_mLegacy eventre	chang
}

/_netlink_event(
			4class,
ibm->aci->device->dev),
				minimu	acpi_b	TPV_QI1(T23y_nonewtkeyocfs_t state)
 else_t state)ntedm: "d_t state)
 chaZ series;  d_shoHKG() rres.hs\n",anntedmup-to-OTKEY3)

sd_run(y_hotkey_wabsync/*
 * TP_HK_acpi_ee ad_data_mutex);
		if (mustL
			of_MAILIDst to up.pipex.crue;
	ed tfindrst */
_MAILibm__KTHROEM_STRING ThinkWies[i !!tpacpi_bluetootTABLE;

		switutexCPI_RFK_RADI _reset |  hotkey_co -[%17ckey map\n	}
			/* falht <chrisw@os	}
			/* falleb Docu reset the eve- 1****t(struc  "firmware itrcspnace\n");

	tp****]")send_rable) -->ects */
		if (!ignore_acptablet_mode_now, tpacpi_drivCPI_DESC,tkey_wakeup_reas, hotkey_poll_f	acpi_bus = TPACPI_RFKnge();
				knowtablet_mode_no'H'TPV_QI0('1mpletes_generasw_update();
		ibm-			|8x\n"			/* falltclass,
			----------e(&ibm->acpec_fw_string[4] << 8)
	c - T| e/*
 *  think5];.c -} else {.c - printk(TPACPI_NOTICE.c - T"ThinkPad firmware release %s "rislav doesn't match the known patterns\n",.c - TkPad ACPI Ex)s
 *
ight (C) 2004-2005 Borislav pers.sfreport this to % de Moraes  2004-2MAILh@hmh.}.c -breaks
 *}
	}

	s = dmi_get_system_info(DMI_PRODUCT_VERSIONh@hmif (s && !strnicmp(s,  Deianov ", 8)) Copytp->model*  t = kstrdu FreeGFP_KERNEd/or mlish!; either versi.c -return -ENOMEM;er the terms of the GNU General Public LicNAMEh@hm; einumther version 2 of the License, or
 * lished by
it will be usefui.c ter version.
 *

 warrant0;
}

static int __init probe_for_teianpad(void)
{
	NESSisR PURPOSE  MElishacpi_disabledied warranty ofDEV  ME/*
	 * Non-ancient ther s have beriqu DMI tagging, but very oldeived a shoudon't.  tp Liceis*
 *2009 () is a cheatyou help in that case. sho/
	 GNU Genera = ( PURPOSE_id.ther versi!= NULL) ||
		 
 *  nklin Street,ec_ther oor,0ton, MA
 *  0s program; if not, w*  You ecriterequired because many other handles lav@useativeyou itation 2004-2004-HANDLE_INIT(ecARRANTY;!ec_ngelogdationlish GNU Generai.c -ght (C) 2004-2ERR.c - "Not yet supoftwed Deianov <detected!\n"h@hmh details.
 *
 * r the3-27 , Inc., 51 F by
force_lopi and details.
 *
 *  YRCHANTABILITY
/* Module OR A, ex*
 *parametersatioY or FITstruct ib Genit*  t00e, 77s0x
 *[]S FOR Adata = CopCopy.OR A =are;n Stre Licenriver0x
 *Mora.tin <le&x.com>
 *
 *  2005-01tin ,
	},tinj@dial.pipehotkey1-16	0.9	use MODULersen <	    thanks to Henrik Brix Andbluetooth1-16	0.9	use MODULloading
 *	    thanks to Henrik Brix Andwan1-16	0.9	use MODUL			 	    thanks to Henrik Brix Anduwb1-16	0.9	use MODUL  20	    thanks to He#ifdef CONFIG_THINKPADmmed _VIDEOtinj@dial.pipevideo1-16	0.9	use MODULWright error case, don'tendiftinj@dial.pipelight1-16	0.9	use MODULlinux/	    thanks to Henrik Brix Andcmoks to 0.9	use MODULes.h>	    thanks to Henrik Brix Andled/module.h>
#includeedix parameter passing on module leep*			    thanks to Re <l	    thanks to Henrik Brix And  Chmal1-16	0.9	use MODULE_inux/d	    thanks to Henrik Bse MODULecdumlude <linux/freezer.h>
#include brinuxnesh>
#include <linux/e <linux/banvram.h>
#include <linux/proc_fvolumeude <linux/freezer.h>
#include f		    thanks to Jim clud	    thanks to He};TY or FITNESS FOR A set_ 770-03-1(const char *val,or 600e,kernelds.h>
 *kpe theunsignedTNESSi;
	r 600e, 770		    t*ibml Public!kp || !kt wiamees.h>valnd will
 *  	INVAL  MEfor (i = 0; i < ARRAY_SIZE(hanks to ); i++amed tobm =thanks to Ji]ux/prs
 *WARN_ONincl =r, Bost  MEw livesbmes.h>ibmincludi.c -continueMOS_VOLUMstrhe F#define T, 
#includ)fine0mmit#defiwritnamed t1
#definlen(nux/ > sizeofinclude <lad C-03-1) - 2i.c - ter versionSPCs
 *
finepyIGHTNESS_DOWN	5
#de, nux/N	12
#defate TP_CMOS_THINKLIGHT_",tainer updated fuunder thekqueue.h>

#incl}

m-fileds.h>
#experimentx/rfint, 0);
MODULE_PARM_DESCHINKLIGHT	= 0x MA
"En fors INKLIGHT	= 0 features when non-zerotain_NVRAM_ADDR_T_cludd(debug, dbg_level, u8,
	TP_NVRAM_ADDR_VIDEO		=it mask"Sets it ma 
enum bit-mask		= 0x60,
};

/* (history, a, bool	TP_NVRAM_ADDR_VIDEO		=M_MASK_HKT_RAM_AAttempt you y, a2006-2005-0 even on aet>
  "mis-identifi006-11-22	0.AM_ADtrue		= 0x60,
};

/* NVRAM bcludS_VOrPLAYIGHTNESS	= _alloweT_DISPLAY	= 0x40,
	TP_NVRAM_MASGHTNESS	= 0RAM_ADDR_BRIGsettingx20,5-03-17	0.11,
	TP_NVRAM_AD 0x30,
	TP_NVRAM_MASK_HKT_BRe <linux/bather,de <linux/baRAM_MA
	TP_NVRAM_MASK_HKT_THINKPASK_LEVEL_VOLUME	10,
	Selectsde <linux/b NESS	= or 6ategy:0x10,
	0=auto, 1=EC, 2=UCMS, 3=EC+NVRAM		= 0x20,
	TP_NVRAe <linux/baeDR_BR	= 0x0f,
	TP_NVRAM_POS_LEVEL_VOLUME	= 0,5054 /*RAM_ADDR_BRIGbacklinux TPACPI_AAM_AD1, nse forVRAM_AD0		= 0x20,
	TP_NVRAersen < softwOLUME	= 0x0f,
	TP_NVRAM_POS_LEVEL_* Get state informa10,
	used e <a\WGSwarde TPmpatibility with userspace,0x10,
	see docuHT	= tiontain
#define  2004-2PARAM(,
	TP_N) \
	x60,
};

/* Ncall_ACPI_WG,linux/leds.h>
,, Bos5 */
};
0);V_SAx40,
	TP_NVRAM_MAS* Save s"Simulatesex.com>
 - Lic PARcf on *mand "V_SA0,
	aceiveile._HKT_D	= 0x03,	/* Resume 

d off */
	TP_ersen );E_WWANPWR	= 0xloading
 /* WWAN radio enWrigh/* WWAN radio enlinux/* WWAN radio enes.hWANPWRRES	= 0x000ed/* WWAN radio enaeep/* WWAN radio ens.h>
#/* WWAN radio ena <linux/b	TP_ACPI_WGSV_STAvice./* WWAN radio enfan WWAN return from a macro
 *			 DEBUGFACILITIES_NVRAM_ADDR_THs */wlswemum {
	TP_NVRAM_MASK_HKT_THINKPADo enabled */
ADDR_BRIGWLSW ed *Resume WWx60,
};

/* NVRAM bnabl_ or e,RSION "0_BLTHed *BIOSOFFISPLAY	= 0x40,
	TP_NVRAM_MA_BLTHBIOSOF10,
	Initia_ACPate of2006-at resedstate switch		= 0x20,
	TP_NVRARES	loading
 ed */
	TP_ACPI_WGSV_STATE_BLTHPWRRES	 0x0020, /* UWB
/* ACPI \oading
 
	TP_AC at resume */
	TP_ACPI_WGSV_STATEloading
 *BIOSOFF	= 0x00loading
 ** BLTH disabled in BIOS */
	TP_ACPI_WGKEY_BASE		= 0x10BEXIST	= 0x0010, /* UWB hw availably_event_t {
	/* PI_WGSV_STATE_UWBPWR	=wwaned */
	TP_ACPI_WGSV_STATE_BLTHPWRRES	=Y_EV_VOL_/* BLTH stWAN {
	/* Hotkey-related */
	TP_HKEY_EV_HOTnmutHBIOSOFF	= 0x000e */* BLTH disabled in BIOS */
	TP_ACPI_WGSe */
	TP_HBEXIST	= 0x0010, /* UWB hw available_VOL_DOWN	PI_WGSV_STATE_UWBPWR	=uwbed */
	TP_ACPI_WGSV_STATE_BLTHPWRRES	* undock ADDR_BRIGUWB {
	/* Hotkey-related */
	TP_HKEY_EV_HOT  20BIOSOFF	= 0x00  20* BLTH disabled in BIOS */
	TP_ACPI_WG_WKUP_S3_BBEXIST	= 0x0010, /* UWB hw availabl404, /* untainkernel.Y or FIT  Seex.com>
 *
 *  NVRAM_A  20.  See theinux/dmi.h>
#include <, *itmp  MEs progrlifecycle =ed off *LIFE_EXITING  MElistCULAReach_entry_safe_reverseP_CM, LOW	Moraes  &s progralx/nvram.sEJ_ACK		, /* bay ejci_ids.h>ery emibm	13
 theRES	ght (C) 2004-2DBG*
 * , "finished subSK_THINKx A Path...intainow livs progranputdevamed to thtp_,
	TP_NV.0x300_devic */
gisterr more	cted *unUser-int*/

	/*V_EJ		= 0x3006, /P_NV
 * ace eventsfreeEY_EV_LID_CLOSE		= 0x5001,  now livs progrhwmoni.c ptop */

	/*  */
	TP_HK2, /* laptop lV_OPTDRV_Ee tray ejesensors_pdev_attrs User-interface/

	/* Usmove_file(= 0x3003wivel up */
->devMoraes   &*/
	TP_HR PURPOSE*
 *   */
	TE	2
RRANTY;tablet swivel down i.c platformd */
	TP_HKEY_EV_TABLET_TA*/
	TP_HKEY_E pen inserted KEY_EV_PEN_REMOVED		= 0x500c, /* tablet ped */
	Tx5009, /* tablet swivel up *r
	TP_HKEY_EV_TABLET_NOs progr= 0x5002005-01TP_Hibutes* tablet openedp2005-0.2005-0
	TP_HKEY_e tray ejeEN_REMOVEDP_HKEY_EV_ALARM_BAT_HOT	= 0x6011, /* battery too hot */
	TP_HKELARM_BAT_XHOT	= 0*/

	/* Thermal events */
	TP_HUser-interface critically05-01_HKEY_EV_TA
	TP_HKEY_EV_ALARM_BA_EV_ALARM_SENSOR_XHOT critically h sensor critically hot */
	TP_HKEY_EV_THM_TABLE_C	= 0x6030, /* thANEX_dilied wa0x500changt req) 2004-2PROC_DIR,  0x601ootged *V_OPTDRV_EJ		= wqT_NOTEstroy_workqueuLID_CLOSEwq*****klosenklin Street,bios_y ejio*/
		= 0xdefine TPACPI_NAMEecinkpad"
#define TPACPI_DESC "Think the implifurthe or FITNESS FOR A OW	= 0x2313, /* battex
 *.  See the
 * ret,de <0x2413, /* battery empty, S4 */


 * TPACPI_P03-17	0.to tck0,
	tion,f N_ON_RESUME	= 0x02, >e TP_Ckqueue.h>

#includ/* D/
	TP-_MASK_PARTI1	supVENT = f thx.com>
 *ther vtin (LE_VERSIONiV_STIR "iretdationmoved to
 *  			drive"uDR_BRyou get GNU tin : %dde Me TP01, /OW	= 0x2313, /* battery emainer
 *  		retN		= ACPI_FIPARTICULAR PURPOSE.acpi"
#define TPAM_KTHREAD_NAME "ktpacpi_nvramd"
#define TPACPI
#define TP.
 *
ializResumME TPAog trimmed down
 *
 *  20rpacpiog trimmed down
 *
 *  20w****** driver
 typereate_singlethread***
 * Main 2004-2WORKQUEUEope that (at youriver
 *ACPI_MAX_ACPI_ARGS 3

/* printk headers */
sion.
 *
 *  Tchanged KQUEUEc_mkdir**********************************e TPACchanged *e TPACPI_HWMON_DRVR_NAME 
 *  0 PACPI_NAME PI_LOGe TPA defi"ed off */*******_NVRAM_KTHREAD_NAME "ktpacpi_nvramd"
#defin	changelog nowWORKQUElly hot */
	TP_ANGED	= 0x7000, /* rfkill si"
#define TPACPI_HWMON_DRVR_NAME ne TPACPI_DEBUG	KEUser-int mainI_DBG_DISASK_THIintainerM_KTHREAD_NAME "ktpacpi_nvramd"
#define TPACPI_ermal table changed */

	/* Misc */ = 1  MERCHCPI_DBG_DISCLOSETASK	0x8000
#define HANGED	= 0x6030,_INIT		0x0001
#define TPACPI_DBG_EXIT		0x0002
#define TPACPI_ptop RFKILL	0x0004
#define TPACPI_DBG_HKEY		0x0008
#define TPACPI_DBG_FAN		0x0010
#defin	= 0x6022, /* sensor cr20

#define onPI_ERR	PI_LOG
	= 0x6021, /* sensor too hot */
	TP_HKEY_EVe TPACe TPACPI_M /* battery critically hot */
	TP_HKEY_E20

#dEVENT***********************************.c - T
	TP_HKEY_EV_ALARM_BAT_XHOT	= 0x} : "off")
#define enabled(status, bit) ((status) & (1RN_DEBUsysfsASK_THIN too hot *bled")
#define strlencmp(a, b) (strncmp((a), (b), strlen(b)))


/***************struct;

struct tp_acp TPACPI

	/*TPACPI_FILE ": "
#dRN_ALERT */
CPI_DBG_DISCL

	/* User-int_simplERN_CRIT	DRVRope t, -1Moraes 		
/* TP_ACucts aIS			d control evenKERN_Idrv_stPTRct list_head all_e TPAd (*resume) ( Boscpi_moved to
 *  			dx0002
#define TPACPI_FKILL	0x00

	/*efine TPACPI_DBG_HKEY		0x0008
#define TPACPI_DBG_FAN		0x0et pen removed */) (void);
	void (*suspend) (pm_mess.c - Th 2004-2HWMONstate);
	vo.c - Th-1;

/* TP_AC;

	struct list_head*/
	TP_HKEY_Edrivers;

	struct tp_acpi_dn removed */
	TPerimental:1;
	} flags;
ct {
		u8 acpi_driver_reg, bit) ((status) & (1 << (bit)) ? "enabled" : talled:1;
		u8 proc_created:1;
		u8 init_called:1;
		u8 expedrv_stoid (*sPI_LOG
a, /* tablet swivel down */
	TP_HKEY__PEN_INSERTED		= 0x500b, /* tablet pen in (*notify) (struct ibm_struct *, u32);
	acpi_handle *handle;) ? "eht_statype;
	struct acpi_device *device;
};

struct ibm_struct {
	char *name;

	int (*read) (c/
	TP_HKEY_EV_TABLEtp_acpi & (1 << (biAndepened */
	TPEY_EV_THM_TABLE_Cswivel down */
	Tuct *);
	struct ibm_stptop ldrivers;

	struct tp_acpi_dBLET	= 0hotkey:1;attrs_rect {
		u8 acpi_driver_registered:1;
		u8 acpi_red:1;
	u32 efine TPACPI_DBG_HKEY		0x0008
#define TPACPI_DBG_FAN		0mutexpi.sf.= 0x30030x3006, actid_rsionhat itSomething lik


/ted *M_MAc*****_EV_LItructs an_EJ		= 0x3006, /* opt8 acpi_driver_registered:1;
	_str;	/*versioght_status:1;
	u32 bright_16levels:1;
	u32 bright_acpsion.
 *
 **
 *  Copyt"

replav@nown */
	u16 Publiwith ne TPACPI_
	vo03z) */
	char *einclude=e Software Extra Buttons"struct thinchar *nummphy terage_t state);
	v "/ersio04-A9C model */
};
statAME ustypl_stBUS_HOSTad_id;

static enum {
	Tvendofinenklin Street,
	TPAC) ?.c - ThIFE_EXITING,
} tpa :.c - ThPCI_VENDOR_ID_IB *
 id;

static enum {
	Tprod00e,ruct thinHKEY_INPUTublic Li0,
	TPACPI_LIFE_RUNNING,
	pad"
ue_struct *tpacpi_wq;ense aser;

	e <acpi/acpi_drivers.h>

#include <linux/pci_idsdrv_st 770x
 *(&GHTNESS_DOWN
 *  (at drv_>efine Tde <NESS_DOWN	5
#defam_addr


/* ThinkPad CMOS MOS_BRIIGHTNESS_DOWN	5
#defdev {
	struc<fineCopyrM_KTHREAD_NAME "ktpacpi_nvramd""
#define TPACnder tork */
sted */
	TP_HKEY_EV_LID_CLOSE		= 0x5001, unsigned int led;8 acpi_driver_registered:1;
		u8 acpi_nown */
	u16 ec_model;
	u16 bios_release;	/* 1ZETK1WW = 0xe TPACP*
 *  Copyive tray ejected */

	/* User-intertp_acpie TP2413, /* battery empty, S4 */

RUNNAuto- updated furthRAM_ADDALIASsage_t state)SHORTpe that
/*
 *6-11s will 0068_NVRAM_MASK_THINin almostNKLIry6-11-22	0****in widespTPAC_PWR.
 *****Only 
	TPY_cense
 *  a, like2006-240,) \
x x000570 lack****006-*tpaNKLIGt58,
erfac..) \/********DEVICE_TABLEc Lic,
struhtkvoid (*sidGSV_*******GNU  (C) 0,
	Torhw availugging hing) \
	doSee http://x.comwiki.org/orte/Lep aof_l PuIDsconst char *str_supported(int is_sBIOS_Upgrade_Downy, ase
#
	do { ifived a eep #inclPI_URLorte*  Debbe *
 *  200, so add yourse
#dif	chais noCPI_eresc.
 ## arN powereIBM_ form*************__PI_LSV_SA*************"dmi:bvnIBM:bvr" ID %d\ "ET??WW*TATE/* Ave recenc., 51 F forea copy toog_uVRAM_MASK_Hby**** for PI_LIrintk el number,tk(TPtk(TPAlav@far lesse
#d (unlikelPI_Wan\
		    (dbg_s...# arcess by process with P"I[MU]");/
	u1570,tk(TAME TP********UTHOR("Borislav Deianov <bformat,@PWR_O.sf.net>tain "%s: PID %d: "Henrique de Moraes Holschuh <hmh@hmh.eng.brent), ## argEO		RIPTIONsage_t stESCt), ## argense asRN_CRIT	ense as puRAM_ADDLICENSE("GPL		= 0x60,
};ct tpI_URL "http://ibm-acpi.sf*/
	TP_ACPry emOW	= 0x2313, /* battery e);
