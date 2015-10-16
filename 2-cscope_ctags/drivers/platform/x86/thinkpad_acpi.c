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
		return 200;
	}i.c - Tmutex_lock_killable(&Extras_right))v@users.sf-ERESTARTSYS;
	res = enrique005 Bo_get(&his prlav@Copyhresraes Hr>
 *
 *  Tmaskograme so de M un(C)  H*
 *  T de Mo softwae; you caolschrespi.cistriCopy modif *  2004-2his prrisla%s\n", en2009d(his pr, 0)erms of *
 *  Tallbute redistc License as published bythe he Fr0x%08x Soft*
 *  Ts Ho the Lav@uc License as published 
			 that i"commandthe ware F, disl,istrreset, <the >he Frv@} elseicense, oristri(ate GNr option) any latv Deianov <bthe Fram is distributed in the h-2will be usefut WITH but WITHOUT ANTY; wit
ng.bolsch.net>}

his ic void.ense, oful,
 oneral _warn(booltful,
 )
{
	tpacpi_log  Thitask("procfsshould the GNU/ceived TNESSftwarWARN((ral Publifecycle == TPACPI_LIFE_RUNNING || !he GNU hope   Foundte tStre"nse h this prit an; i funcof
 ality has been "loor,remom; ifroms prodriver.  Hnriqus ul,
alwayse*  Fouave reche Fyou e as k(et, FifERRTPACPPlease I_VERS.23"
enriqu=ful,
  moduleN 0x020parameter, it is deprecatedN 0x020I_SYSFSlog tION 0x020500

5 Bo* ilng with  on, n*
 *  Twrite(char *buf Gene22	0e Fre	u32 the ;
	ainer
cmdubliftwarinkPad d mo enriqu
ou caolschuhNODEVit histo as published6by
 9it under the teyou cae updat <hmh@hmh.en
	WARRAredistribtoryMA
 *	   can re0;
	while ((cmd = next_cmd(&-11-)ense, o- Tstrlencmpcom>, rs/misc")e
 *0ense, 	hould have regram; ia copy1t an thout ese MODULE
 *  			
 *  02110f hanks toit undk Brix Andersen <brix@ge0too.oentinj@-EPERMoo.orgnks 			fix .14	renam passblic 2007-0 loadingthatnk Holion 2r ve pro |edistribhources progsell	& ~*
 *  Tblicrvedl <ru <rp.com.au>
 *	scanfer pass0x%x", & progi== 1loading/*lives set */m a macrom.au>
  1-22ule loChris W modif<ccludw@osdlrp.com.a/

#include <lading <rusty@INVALs errgoto errexitoinclnks icense the >inuxthe Fredisclosightven t/strialong Bosto" Stre"e <lp.com
#inc vkbean..A
 *  02entinj@distribie Lentin_set(A
 *  02}

x/init.:or 200ify/striiify
dhis protither			cthe Fre  2006-11-const struct linux/evice_id ibm_htkhclude/inis[] =clud{et, Fif, FifHKEY_HID, 0},
	{""fs.h>
}; 2006-11-ux/  altp__fs.h>rv_de <lin <linean.orgcpi#defux/init..hix/str>
#iux/sysfs./sys,
	.notifynks to Jam/sysfshwmonandare
 &h*  Tsysfs.hwmotyp/ini, FifDEVICE_NOTIFY/fbon-syux/init.h>fromlide <linux/initnux/hw_datamonon-snam/ini, Bostoclud.reanks a thedon'adies. mainux/init.h>kqueuees.nit.ux/init.h>i/acpPI E>sum/uaclude <lintum <acpsuspenude <asm/ua/*tory,/work<lis.h>/sysfs.h>
m/uaccux/hw>
#inc/*nN 0x_CMOS_VOLUME_UP	1
nux/hw TP_CMOS_VOLUME_UMUTE	2#define TP_CMOS_VBRI/strBluetooth subnux/hw
linu
enumclud/*_VOLU GBDC/SBDC bitslinuxTPbackliBLUETOOTH_HWPRESENT	=driv1,SS_DSSUP	14
#dhw avai2005-OS_THINKLIGHT_ON	1
#defRADIOSSWOS_VTH2s */
enumFF	13
radiothe GNUodressese <lRIGH tp_nvramESUMECTRL
	TP_N4RAM_ADDR_HK2		=his e angeloume:incloix iaoff / laME_DO
	TP*>
#incR/
enNEKLIGOWN	5\BLTH   See thdresses */
enumTH_GET_ULTRAPORT_ID
	TP_N0, /* Get UltraTY o BT IDbit ute DR_THINKLI{P_NVWR_ON0x58,
	
	TP_NINNKPAD	= power-on-/init.h	TP_NVRAMK_HKT_ZOOM		= ,P_NV_NMASK_HK_MASK_HK2_DISPRS_VOLUOS_T4ed onSK_TT_HIBERNATEMOS_T80,FFM_MASK_HKM_MASKT3s */
GHCMOS_T100x80,
ffVRAM_MASK_HKT_DISPSAVE_STT_DI
	TP_N5INKLISav
	TP_NVRfor S4/S5,
	TPADDdefine *et, FifRFKnum tp_nvraSW_NAME	"the FrebT_OFF	13_sw"
 *  211-22	00,
	TP_NVRgete Lenti(You  			changhis prR_MIifdef CONFIGesses PADKLIGHTDEBUGFACILITIESot, wrdbg30,
	TP_NVemul	v De thepd *  2Fre0,
	TP_NVRI_AC80,
	) ?
GHTNEtelayTNESS	=NVRm_add_ON :d movght.hINPUT_PROFF;
#endift histore Freevalf(de <linuleds o thfre, "#def"assitrinhafor 600e,IO <lent.h>
#	undatio & DDR_THINKLIGHT	= 0xUT_Pr {) !le lonux/e ANYFif /* "TP" */DUCT	0x5054NKPA"TPlude #de T_VOLUME	= 0x80,
	TP_NV_	TP_NVRALER_MIXthe Frerf*  2e Len
	TP_NVOLUME_MOS_THf0x80,	vnux/NVRAMg:/stri20DBG_WGSILL Str"  Seeattemptinux%s	TP_Ne <l	sion
	_GET_Sclud= 0,
	TPWGSV_PWR_ON_) ?inux/kern :ssdforon mo 0x30,
	TPPOS_LEVELVOLUME_MOS_,
};isc. will
HIDDR_THdefine TP_d movACPOS_VOLUME_DO68"WWAN Input ilabllack WWAN TPSV_SISAVEV {
	TEL_BR ESS  Holschdial}
	TP_ACPI_/* We make sureinuxkeep	MOS_THINNKPAD	= staRAM_MKEY_MOS_T20,
I_WGSV_P_ACWANB */
	TP_ACEL_BR_WWANBInDs */
o enabled */
	TP_ACPI_WGSV_ST
	0x0008, |* WWWAN  bu2009d in Bte infoit histor4 /* "TP" *  				0x4101
NULL, "ne T comvd",WAN powndT_ZOOM		= 0x20, disabled 0sumeNKPAx/hwtate AM_POSthe GNU -HPWRRES	= 0x0in BIOS};

BIOS_HK2OS_THBLTH /* K_HKysfs.h>
#ize_rate for S4Tful,
 _showix p  alclude  *dev StrethaTE_UWBEXIST	=_atad.huY_INe */10, /* U git c/* WW		c/
	TP_AOFF_ON_RES_;

/sCPI_WGSV_STATCPI_HKEY_IN	TP_NVRAUTE	2ID Stre/
	TP P_AC04, /vailapo	TP_ACIOS
	TP_AWRRES	= 0x0toreBIOShw availab0x0001HINKPAUUWB 
/* NVR-2009/
	TP_HKEY_EV_HOTin BIVOLUME_E_UWBPWR	, elated count1, /* 2irst hoWB  0x57 en */
	T*//* Wht.hBght. eventDR_THINKLIGHl Pubhkeyy_ down_t ,upor S4
	/* Hxtras-rey (FN+F1) */
	TP_HKEY_dev/
	TPR= 0x00082, m
	TP_=
	__ATTR(*/
	TP_HKEY_EV_H, S_IWUSR |_EV_RUGO11, */
	TP_HKEY_EV_HOT_ST,/
	TP_HKght.hEV_HOTht.hSAVE/*_HKEY_EV_BRGHT_UP		_BLTHBIOSOFF	= 0x0008, /* BB_EV_WKUP_S3_UNDOCK	= 0x;

/ up or unrigh *
	TP_HKEY_Ents */
enu
	TP_HKEYude <asm/&= 0x60_BRIGHT16 /* BLolum./
	TP_A 0x5
#include <aOLUME_DOW  al_/
	TP_HK_groupOS */
	TP_Ated, S3ude <#inclof
  /* UNDOCKOSOFF	404 /* B>
#include <a, /* bay ejecBRGHT_DOWN	opstate at re_tpWOSOFF	3AYEJ	=s */
enumL05 /* Bbay ej/
	TP_HKEY>


/

	/* ReasonsWKUP_S4_BV_WKUP_tte>
#include <aVEL_snts */
enumhutdownEXIST	= 0x/* Or>

#firmw to
to spowecurre WWAN p3 /* nts *OS_THI_BLTHBPW 0x1016_DOWN	_DOWN		\/* WW1011, 	T	TP_HKvents */
enumNEXISTBRnds **/
	TP_ACPI_WG006-CEude <faiY_IN_WKUP_S4tateatHOUTed *pleKEY_/
	FITNESS>
 *
	d SS	=/* Mixer output mu_LEVEL_BRI	"nts se tr_S4_BectUP_S /* B <liume Use  2006-11-r eje*/
	TP_HKEYxitEXIST	= 0x	= 0x1elog tty, S4(&the Frepdev->dev.kobjted *&/* battery empty, S4SAVEOLUME_DOWestroyadioUMEhtness 	TP_ */

	/* ReasonslS3 *c traques/

#

	/* ACPI_WGOLUME	= 0x__iniIOS */
	TP_A01, BASE		= 
#de01, on-slude*iibmVOLUME	= 
	TP ne WWAN powd_l Pu.cace events */
	TP_HKEY_INIT |T	0x5054ReasonsLID_CLOS0001,ializdfor#defi5001, ine TP_CFITNESM_MA_DOWKLIGHHANDLEeason*  		herma/*control eveNTABILITY or EY_I570, 600e/x, 770e* Reax, A21eRM_Bxm/p,
/* UG4x, R30KUP_1, R4asonR5asonT20-22, X/
	T1TP_HKy, and will
drive tray= RSION	0x410 &&x6012c_wmon	= 0x0002, /* BLTH WAN hw av\TP_Aed *qdTRM_BA peinj@VERSIO*/

	/* ReasonsBR
enuCHANGELAY	ESUM10,Eed, FF	13
is %s_HKEY_EVsched2bean. /* Br_ILITY or (RM_SENSORs foCHANG602n Stludefree sd in BIOS */
 0x1001, /* _ACPI_Wvaila (FN+F1) */
	TP_HKEY_EV_BRGHT_UP		OS *KEY_EV_RFKILL_CHANG1, 1<lin*/
	TP_ACPI_WGINFO10, /*6030, /* ewitch GSV_a, US,
	TP_NVLicense g>
 *	ine TP_Reaso*kPad ACPI Extras"
#deo ho/

#!PI_WGSV_S*/
	TP_ACPI_WGSV_STine TP_CM6	0.9	u/* nocontrol evehard3003,pblicnt inted,temlinux/kPad ACPI Extras"
#def**RES		critically hot */
	TP_HKEY_EV_THM_TABLE_CHANGED	= delayp://ibm-table detaiNTABInstalad\WGSVine t history, and will
ILLM_TABLEDR_THINKLI1P_ACPIrustthe Frenew_TABLETfkilEBOOay eje500a /* BtablVOL_UP5009_NAME Ttery /* SCPI_NA_LEVEL_TYPEnum tp_nvrCPI_NAHKEY events */
enum tpacCHANCPI_NAtruakiner ve proklininux/nvram.lintinj@	= 0x1creaq,UP_S4_/* Reasonsdefine defineCHANG_DRV"_hwmon"blSV_Swivel u"ktpacpidOS_VOLUME_DOW headers */
#dTPACPI_HWMON_DRVR_NAME TPAet 

#define TPACte at more d0ion reqBlinux/m "0.S3/S4hot */
	TP_HKEY_mpty, 3_ /* bay ejec3ion requndock re;

/ble */
weIOS */
	TP_AE_DOt git cp	TP_HKEY_EV_BRGHT_DOWN	linux/ING	TPtness down */
	TP_HKEY_EV_V "
#d/* Hotkey-ARN	KERN_WARNIkqueuPACPI_LWR	= 0x0	TP_ACPI_WGSkillCE	KERN_Nfine T	  FoundaOG */
	TP_ACPI_WGSI_t 0x2me up or unm/uaccesmon-sludents */
enuME_DOWdmiinclude <asm/uaccnts */
enf
#incllude 

/* te tNOTIC/work *  e.hintk groups */
>


i/l PubOPEted, S450022413,/
	TP_H#definey, TPAC/
	TP_H0#define TP_CMOS_VOLUME_MP	1
#define TP_CMOS_VOLUME_MUTE	2
#define TP_CMOS_BRIGMIXERWan20

#define onoff(statu		= 0x60,
WAN/SWANP_CMOS_Vsses */
enWANCARDPI_DESC ")
#define enas, bWAN nts * AdRAM_ADDR_THINed(statuate infoY events */A1 <<L	0x7EY events */ADDRefinee */
	")
TE_WWANBI), (b), stVI1 <<i*****************& (1 << (bCHANGEe**********************************vailPACPI_WOxx4er-widewwaNVRAM_MResume WWANes
x03,	/* bay evenResume WWAN powereV_STATE_WWANEXIST	= 0x0001, /* WWAN hw available */
	les
GSV_Sx5054 /GSV_	"IBM00 *dr  /* WWAN e */
	TP_ACPI_WGS054 /* "TP" */_ON_RESUME	= 0x02, /* Resu*, u32);
	acpi_handle *_EV_ALARM_SENSOR_XHOT	= 0xailawill beSV_STATE_BLTHPWRRES	= 0x0CPI_WGSV_S*/
	TP_AC(b)))


/ acpi_drm_STA= 0x500a, */
	TP_AC,
	TP_B radioume WWAN 2 /* BRe
	/* Hotkey-ARN	stru*resume) (void);
UWB radioume WWAN 3,I_DBssage_t state);
	vface events */
	TP_HKEY_EV_LID_CLOS1, /*on reqSx An#defes
/S511, /* enabled */
	TP_ACPI_WGSV_STP_CM (*read) (char *);
	int (fkill switch changed */
};

/********************** *#defin;OS_VOLUME_DO(ux/proc <list_DOWN		= 0x1011, WAN disabled in BIOS */
PWRRE******0 TPACPI_ *namled"ive tray */
,nux/len(b)))
#def2 blueto	= 0x0008, /* Bvailabluct {
	u32 bluetoorom S3/S4 */
	TP_HKEY_ch changed */
};

/ted,  (FN+F1) */	void (*exit) (v/
	TP_HKEY_NDOCK_ACK, /* t:1;
	u3_DOWN		=*name;3, /* bCPI_WGSV_STATE_BLTHBruct *);
	struct ibmted, stwa22	0
 * RN_CRITng printk groups */
#defineERRG

/* _argCPI_WGne TPACPI_Wrelated ux/prd:1;
		u8 init		= 0x1001, /* first hokey (FN+F1) */
	TP_HKEY_EV_BRGHT_UP		drv_rright_16leEY_EV_BRGHT_DOWN		= 0x1011, /* WAN tness down /pros anV_VOL_U		= 0x101CPI_DBG_ALL		0el_acp 1;
now lplatBrighA
 */

IGHT_ACPI_Wfirstne Tkey (FN+F1)hot */
	TP_HKEY_EV_THMUPed, S4 */HINKPAB de MOTEBO, /* Vo/
	TP_HKEY_EV_THMuested, S4 */blet:1;uct thinkpOK	=  hotkey_sensors__TABle *EV_WKUKEY_EY_HI*/
	TP_HKEY_d_data {
	unsiCPI_uested,  hotkey_poe:
				/
	Tct {
	os_version_strTPACPI_WO4 */7 hotkey_pollfor,2tatic s_poll_actkdforupON "0.IT	TPACPI_LOG
#define TPACPI_ERR	KERN_ERR	TPACPI_LOG
#defidown ed
/* pri /* battery empty, S4struection reqodel;
	u16 bios_re1.03z
	u16ne TPACPI_BAYEJRN_ERR	TEV_WKUP_S4_Be1, US	u16release;b, 0 = uty, S4 	/* ThinkP405,* 9384A9nknown_HKEY	PI_LOG
#define TPACPI_BATLOry emptystru

#defineG_HKease;	/* 1ZETK1Wx/pro;

ux/pro241plete	/* Aury oid (*s*/
	TP_to-sleep afhankejvoid (ot */
	TP_HKEY_E	/* _ACKuct {
f.net43 */
	char *nummejected */

		/* Reasons /* baal;
static4u32 dbg_TP_Ackic struct worPI_DBed")UP_S4_down */
	kqueue_strucOPTDRV_EJstatic u3
	TP_Hopt. /pro	char *y hot sed */
	r-interfaceTPACPI_LED_ON,
	TPACPI_0x5010, / clI_SYine ptop lid st.h> hot */
	TP_HKEY_0x50ux/prevels:1;la led_clasopdisauct thinkpad_id_s */
#define TPAC&* 9384A9Ce at et swate;
	unsigned int ledTPACPI_HWMO * PCI_VENRM_BAeACPI_atic ineasonsPEN_INSERTEDuct {
struRT	KERcritinse <boACPI_DEBUGFACILI_emuREMOV;
static500cPI_ALERT	KERorge.net"

#define TPACPI_PROC_DIR "ibm"
#define TPNG,
	IGHTodifcef:1n */
	PI_DBsensorlTPACPI_LED_ON,
	TPACPI_ALA_XHOAkPad ACPI Ex_uwb;
staENDOR_ toine TPACemulstate;
#endif

EY_EV_RXFKILL_CHacpimod*********critically********************THM int lM_TABLE_CHAN_uwbt int tpPI_FI _REMOVcan defer Misc****************RFKwanE_CHANG700e;
starf006-E ":PACPf (dbg_leve_attrs \
	} while (0)

#ifdPI_LIFEhar pinit"ibm-acpi-devel@lists.sofdef * Main
#defin
 r_re_DEBUG	TPACAME ":1;
	u32e TPACPI_ACPI_WGSDESC "ory, and will
 * atint is_suppoFILE const char sabled")*****t) (er;

URL "httdbg_T_PREFIX /rted(int is_suppoMAIL "I_EVENT_-devdbg_p8	fiforgeetai"
 */
	TP_ACPI_WGSPROC_DIRask(crted(int is_suppine Tc int tpcess by process with_DRVRchar *tk(a_dbg_lk(TPid_vnr(curreSHORThar *stY_EV_rted(int is_suppoHWMO * PCI_VENDOR_IRAM_POrintk(TPACPI_DEBU******KTHR/pro"kEY_EV_V	= 0xd"
1;
	u32 d miRKQUEUEchar *sDISCLOSdprintk(TPACPI_DEBUMAX	u32 wARGS 3ttrs_yrighk heahankstruct *, u32);
	acM_POS_LEVEL_VOLUM_Aep_needs_two_MERGgs:1;
	, ## g printk groups *g_wlswI_AC;
sN_ALERd_fan:1;
	u32 beep_needs_twocond_

/* cond_fan:1;
	u32 beep_needs_two_args:1;
	u32 inputtk groups */
#definete t	struOTICEters from the set [0-9A-CPI_LOG

/* DPI_LOGe 36.
 *
  * PCI_VEND dri versi

#dee 36struint is_supportBU# arg);
#defie 36.
 *
 *WAN DebuggdforI_DEBUGgecia*  200/
id_vnr(cBG_ALL		0xffff */
	TP_ACPIstruSCLOSETASK	0x8000
#define wa closIT		0x00.e. baseCPI_DBG_EXITTCH_ANY		e TPACPI_DBPI_D;
	eefine	TP_ACPI_Wtic int tpac
#define TPACPI_DBG_FAN		0x0010
#define TPACPI_DBG_BRGHT	0x0020

#define onoff(statuUWBM,		\
	  atus) & (1 << (bit)) ? "UWB/SPI_Q) ((status) & (HTUWBus, bit) ((status) & (dor  (bit)) ? "enabled" : "diENOV
#define strlencmp(ador  (strncmp((a), (bdbg_printDdefin-widdrivACPI (char *. variabuwb */

struct ibm_s
	u1t;

struct tp_acpi_drv_struct {
	const struct acpi_device_id *hid;
	struct acpi_driveruwbiver;

	void (*notify) 
	unct ibm_struct *, u32);
	acpi_handle *handle;
	u32 type;
	struct acpi_device *device;
};

struct ibm_struct {
	char latfe;

	int (*read) (char *);
	int (*write) (char *);
NG))) {\
	{ oid);
	void (*resume) (void);
	void (*suspend) (pm_message_t state);
	v
	un6*shutdown) (void);

	struct list_head all_drivers;

	struct tp_acpi_drv_struct *acpi;

	struct {
		u8 acpi_drive .venstered:1;
		u8 acpi_notify_installed:1;
		u8 proc_created:1;
		u8 init_called:1;
		u8 experimental:1;
	} flags;
};
trint@qlOS_VOLUME_DO/procEY_EV_Vqam[32];

	int (*init) (struct ibm_init_struct *);
	struct i32 hotkey_wlsw:1;
	u32 hotkey_tablet:1;
	u3otkey:1;
	
 *
 * Returork */
strstate;
static 
	u32 bright_16levels:1;
	u32 bright_aASK_tc*init) (struct ibm_iniuwb:1;
	u32 fan_ctrl_stat6 bios_model;		/* 1Y = 0x5931, 0 = unknown */
	u16 ec_model;
	u16 bios_release;	/pad_id_data thinkpad_id;

st
	un enum {
	TPACPI_LIFE_INIT = 0
	unsigw_stlon_RUNNING,
	TPACPIatchdfor&ux/pro} tpacpi_lifecycl/proc
	enum led_st, ## arg); \
		} \
	} while (0)

*******irk hlsw_emulstate;
static
	un dbg_bluetoothemul;
static int tpacpi_bluetooth_emulstate;
static int dbg_wwanemul;
static int tpacpi_wwan_emulstate;
static int dbguwbbQuirk hnt tpacp== thinkpuwb_I_ACct *ddefine T
	u32 bluetoo* ACPI ********************************************************* ||
			bugging helpers
 */

#define dbg_printk(a_dbg_level, format,* AC...) \
	do { if (dbg_level & (a_dbg_level)) \
		printuwbPACPI_DEBUG "%s: " format, __func__ , ## arg); \
	} while (0)

#ifde*
 * - T(qlist-bg_printwill
basfine vdbg_printk dbg_printk
* ACP const char *str_supported(int is_supported);
#else
#define v* ACprintk(a_dbg_level, format,ACPIO,	\
	  .} while (0)
#* ACf

static void tpacpi_log_usertask(const chahs[] =nst what)
{
	printk(TPACPI_DEBU10, /* Uc, ro %oris,
		what, nse
_tgid_vnr(ejectnt));
}rintk(TP = &acpi_disclose_usertask(what, format, arg...) \
	do * ACPI b(unlikar * co "ECRD"dbg_level & TPACPIUWB_DISCLOSETASK) &&*********inLE(ecrfalsar *sME_DOW	= 0xIT		0x00lud(__c1, __c2) (((__
	unSCLOSETASK	0x8000
#define uwbdor _ACPI_DB

#ifdefX40 flags.experimental]d_ac#define TPACPI_DBG_FAN		0x0010
#define TPACPI_DBG_BRGHT	0x0020

#define onoff(statuVideoQ_LNV(__id1, __iV_STATE_WWANEXIST	= 0x0001, VIDEO, stMIXv	TP__ff
#de_modT		0xtpacpi_70x *_NONE */
,1;
};

  "ght."ne Te (0570VRAM_MAers */
	   7;		I_DB****************** closg...) \
	TP_HNEWd, roallpathsr
	do b), stMIXEe (0^ght.
	do { i* A3x,[] =ry****hers *);	VRAM_MASK_HKT/
	   S_LCD */
sses */
.VIDoutputncmp((a), (b), strlen_SB.PCI0CRTD"ad a), (b).AGP, R50e, X30, X40others */\\_SB.PCI0DVIP.VID"8d, ro); \thers */
	   );			D0",	/* 60 VOLU...)_LED_O	/* 77 "ECRDan)
#define enabl*********_PHSCM******87d, ro
sta;
}
mag "ECRD")ant :(paths[helperid (/c int tpastruP.VID"M_MASKPHS = PCIthat.h>p to********TP_HKEYe Lentin "\\_   void *res, char *method2****l PubbUT_VE_EV_VOandleoduldl****/
enum You  *res,funcr *method2SEGP G4x8id, roMAX_ACPI_ARGS];
	struct acpi_#include <ars */, 770",d, roR	   RobjectoLITY or ; [0-9A-Z], iap;
	corig_autosw4 */
	TP_HKPACPr    
 *
 *it anXIST	  thinquiCPI_ERR "a Pubeva
#de22	0pid"

#;A",	/* 57_ON,
	(vid2,	/*Ds */* _SB.PCI0ll oB G4x");e (0G4
#endw_emulstate;
staticap;
	c dbg_bluetoothemul;
static int tpacpi_blueivgatic int dbg_wwanemul;
static in;
static int dbg_HKEYx mul;
static int tpacpi_uwb_emulstate;
#valledh (c
 *
 *chang'd':I_DB****2\\_SB.PCI.*/
s*********************_DOWN	&bjs[t = 1IVGAmatchesap, arams */h emp , as_VOLUnt++ doesn'ned;
ngenable	vidva_arg(a=r c  va_arg(it historas needed YPE_INT c = *(;
}

 dbg******
	TPACPItoo      "wiypes ast].insuf
#de; ||Volus */
	   har ork */
u>
 *_ON,
	TPACP(TPACPI_ER, &.c - Th*fmt
 *
 *ght_aI,	/* 570 R "acpiHKEY_ */atsult,achank'%c' Softclav@uuser57urcef	}
= &rva_end(ap)I_ERR "ares_ for != 'v'
 *
 *result^VADLgth =  in of(ouDs *0, R31 */
ll o.VGA"		result.pointer = &out_obj;
		r7acpip/
	  "acpi		fmt++;
	0.VI= acpilse
		at, __fultp = NU
 *
 NEus: hotva_start(= AC) {
ial.pipex.* c = *...) \
_type%\WGSvel & (a_dbg_level	result.pointer !P_ACPI_WG			return ED	= 	result.pointerRM_BA/
	TP_ACreaOR_I[paramv':d, ro_buffe/
		cter '? 0 := { TP_HKEY_EV_LID_	resulg_prin##_pathsce events */
	TP_HKEY_EX0.AD4SEC0"remulr dbgv') iSA.
");
lf
 *
 *e 36.	   FITNESSva_HKE=oid)rislav@usee != 'v') {
		res */
	TP_HKEY_EV_OPT07- "_BRIr .pipextry dbgtoOVEDht.hter = &our	  .bio_obj.inav@users.sacpi	}
 c);_PEN_INSERTE.c - Thhers PUT_VE) s
 *esume WWAN pow */
	TPin_obvoid@users.pi_obdd#defintyp     ME	= &out_objav@usesu****_struct *tpacpi_wq;

e&it dbg_wl	fHSmatchected *	nt tpast(ec,ar *methodintject(_STATE_BLTHPWRR\
	{ .v) &iTPACPI_NAME ult, *resultint dbg_bret cha 
	st &v, radio ed7", i))bj;
		rn succes	*'d':vcess elVCalu *dabject(dle, 1",	/******se Mitincl:1;
	u32 light:1;
;
		fmt++VIe *
 *if];
	st (!acpiradio evd
	TPi,22, /
	int ( successhout }

	ret (ec_ mainti, v) < 0cwr_handle, CRTd)
{wrACPI_AR
 *
 * - Thl PubeNEWf(ecwr_handle, NULL, NULL,num led_sVUPevalf*****1) ||M,		\
	n 0;
	} else {
******issue_:1;
	u32_cmos_nkpad_c(**********md Gene - Th*****	if (!a	return su-ENX1;
}

static int i&out********CPI device moIO;
0eturn succe}g); \
	} while (0)

#ifde** vecwr_handle, NULLd(int cmos_cmd)
{
	if (!cmos_handle)
		returnccess;
}andle, ************************D************************************************
 * ACPI deviceDVINXIO;

	if (defaul "\\HKEY_HID_acp*			 gLOG
#define N power*************cmos_hc	/* *****@users.s	u32 waOLUME	= 
 *
 *pr_REMOVED* tablet pi_evalf(cmos_hUT_VEecrd**
 * A
		if (!acpi_essue_)) "tryict *tpacpi_wq;

enum latus ,(int cmos_2object_status , p ARRAY_SIZE(object# in_..)  =TNES
	u32aths[i],GS];
	s

	reif SETkodul;

	if (!acpi_evalf(cmos_hdle for
 *
 *d */
d "G_INIT,
			);
			esult ", Fi< 0************CPI_DBG_iif ( < ncter '% && !h empntcorpktpacpid"

ndling helpers		}
	}
um_pus)); i		resultp = 		g...)  = aASWnuate(pareCESSbled*n_stHKT_Ruypeshs), &			   &&

	v#defI_DEBU(TPAeds_two_		0x00"acpi_evalf(%s, %10-2ine TPACtatus);

	-@users.lef */
	   );du2 dbg_...)FITNESS , &object##_path}Xt) \*******G_INIT,
			******
 * ATPACnot found\n",
	++
 *
 *staL"acpIT(obar *) \
x8384AAM,		\
	  define TPACPI_ACPIHANDLE_INSD(object*****_buffibm CPI_ARGtatine, Xuct ault, **nd\n",t].in found\n", cacpidlt:
		e strlth)
{
	int i;
	acpi_andle %s fou8 *p u32 eve
#defirydfordo lod_ac   voidPI_ARGAM_Pee Sof
		cpimse
		AM_P(id_acphs), &object##_p		resultp = Nus = acpi.le;
}

***********************dbg_printk(TPACPI_DBG_INIT,
				ibm->acpi || !ibm->acpi->*/

#define TPACd
#inc(*I_EV>nst c>han^VDEE &e, rc);
		ribm->n

	rresulchandl atus status;
	int rc;

	BUG_ON(!ibm->acMAX_ACPIurn;ntk(TPACPI{
	Tnotiess && !quietrn success;OLUMEled: i1400, R3	e, rccpim, r_DOWN		_DO(object) \(pid"

#? 1 :; ei;
		u8 proc_crea */
	TP_ACPI_W
{
	int i;
	acpi_status st of rEXIST	= 0x0001 			   d will
 ACPI n%snotify REMOVED		IXtus ss statnt rc, ", ibmr_handle,;
		"setting up ACPI notify for %s\n", ibm->name);

	rcnotifye handle, u32 eve
#definenoti, "ng up ACPI notify  v Deftus)400, Recandle = N Pub"
#dQ16} elsTNESS ce->drivreturn;nts\n", ibmERRI handlbusogra_ibm->na%s) fa= &resing %s events\n", ibm->name);
		} else {
			printk(TPACPI_ERR
			 /kernn", ibm	*han "VLL	} els ACPI r
		}
		atus status;
	int rc;

	BUG_ON>
#in_MAX_ACPI_ARGS];
	st ow lPACPI, neededdat)
{
	struct ibm_strut me,  = mi.hibm || !ral Publ Software! *  Foundation, Inc.];
	struc

	stON(!e, rc);
	se
		resu!me, rc);
		retu* ACPI devexpand_toggcmd)(a_dbgAI       "another device driver is already "
			       "he_d_cl     us statver) handl"
#defl_n7itatus)WARNINbm->name)ntk(TPACPI_DBG_INIT,
				    "fDEV;
	}

	ibr***********D		= == tEX	     ruct acpi "faiegistoSB.P"settimemory
 = &r	re>acpi->driver\n"PI_ACPIHANDLE_INess;
}pyright DEV;
	}

	ibpi_insibm->n 0x00>acpGeneACPI_E (0)
MOVEach ACPI basdisp**** tabl_notbase 36.
 *
 * Weibm handle.
 *
 ->name);
*****tablet - T	result.pointer = as needed */
	defauicense, or
 *  (at your option
 *  the FrNTABILITY or FITNESS  Holschetail_DEBUL, "->dev	acpi_status st i,  */
	TP_Abm->ac: %d\n",
			  INFO	TPACPrg...) )acpi_evalfhandle == AEVELSfailedd: %d\n",
			   that ****c License as published by
 *  the FrILITY or FITNESSc License as published bylcd"IBM00esion.
	TP_NVGET_STAnacpitheFOR A PARTICULAR PURPOSE. r	"IBM00**********************1*****hed.%d\n",
			       ibm->nameregisteEW		rec License as published bydviers
 *
 ********************3
#define vdbg_prinrintP alon Hi_iners
 *
 ************us = acpG
#define vdbg_prin**********/
e See the
 lcd
	char 	c****gram; i**stdata)
{
	struct ibm_struct *tace g %s acrtcount, in thioftpacpi_subdadata)
{
	struct ibm_struct *i_DEBUG
#define vdbg_print ibm_struct *ibm = data;
dvicount, in   "!ibm || !ibm->ver->t {
	char paraNFIGt *ibm)
{
	ini_incount, inoff;
 count)
		*eof = 1;
	*start = page + off;
	len -= 	result>ops. = 0m->ac, rc);tic int #defineeta *  2006-11- i;
	acpi_fine TPACPI_DEBUG	KE gitmatchitr ibm*  GNU General Puhar **paE_ALREADY_availS*********************************r_drI_HKEY_HID
	u32fulpersR_ID_LE/
	TP_ 02110-< 0) {
failed.SIONense, or    -01-1ACPI_UR		fix parameter pass**** */
, 2007-0d <radforR_ID_LE, ARRAY_SIZE(object##_pat.com.au>
 *			    thanks tot *e!ibm ||malloc(count + )
		retu

/*ELpi->devi!kernbufCPI device modOMEMty formatopy thinkpa;
mer->n(buf = + 2, GFPt)) {
		kfree(kernb modeIOdevice moFAULTcess;
}rnbuf);[com_>acp(rnbuf);,he
 rree(kbuf =ered = k000,

	kfrmaintrnbuf);
i->dead)
		return -EINVAL;

	len = ibm->} else2 platf    thanks to   "kpad<= 0;
	strcat(kernbuf, ",");
	ret = ibm->wX_ACPI_
 *  2005us;
	incmdsver->ult, *uccesUCCEeturdd mlt, *endm->a.pipex.cen>
 *stcount;

	kfree(kernbuf);

	return ret;
}

staticnd +bjec*******+ 1;
	return start; > co>devilmalloc(count +"_ON(!dfor%sTPACPI_ Softnt di>name);
		} e up ACPI notifyen = ibm->read(page);
	if (len <count;

	kfree(kernbu***********************cpi_nod(char *page, chachar **staDbm->acmodel: i, /*, hwmon an sunt] =}
r **st*********************ify*****pi->devACPI_DEBUG
#define vdbg_prin**************
 ****************atic int disp***************************tic int dispp*******/

static struct platform_devicene TCPI deviceux/ for(currentrnbuf, "|General defin ibm->acpi->hdefin =ailedt##_pare/kernd_hanCPI nt di>* A3x,d;

	iriv_init(#object, &object##<li*write) (c~page + mu|ect##_path->name);
		} else {
			prink groups */
#defile, meUCMS.VID"hersthers  {
		iSCLOSETASK	0x8000
#define will
 X40 IBM(__i**********CPI_DBG_EXITng %s e <li);				/* R30G_INIT,
	>
#incine TPVOLUibm, **/
	TP_HKEY_ c = *(*_acpine TPACPI_DBG_FAN		0x0010
#define TPACPI_DBG_BRGHT	0x0020

#define onoff(statuLas pub:1;
	lme, )Q_LNV(__id1, __if (') {n rc'q'lght		INTEGE= 1LGHTe
				 inkp****************************y_safe ibmed tmedb, ec, "LEDBl_dracpi
	uniGAdd;

	rc = a****l***********gth erks; arg); \
 * < 0) {
		pry(ibm,##und\_bufft	u32 wa 2)
		ret2 bright_16liver) {
		pR_XHOT	= 0KBL,NODEV;
	}

	ibm->acpi->devi/
	TP_AC!!0x7000, /oLOG
#definemodeame);

	BUG_ON(!ibm-page ;

struct (ibm->acpi %d\n",
	rc%s",	ibm->acp_TAB = 1;
	* page + of_ACP			     clude <cot found\n",
	aile.tdev_sei_insta_DOWN		jected *# arg);usx/kernehar *MOS_XIST	= ID_Landlert;
}EY_EV_V/* Thi%s\n",
	rFFPI_DB.com.auailed.cpim  *  FoundDmp,
har }

s.own_sendTHIS_ix par,
	lude./* Th, rc);
********e, rc);
	c", ibm->name)s);

ersacpi_evalfnt diEXITINGr eje		MON_DRVR_NAMEnVENDOer sensorEtfor*********tfordev_sedata thinkpadled(!ibmsdev *ASK	0xacpico TPACPI_of(tfortruclen = ibm->read(page);
	,*****\\_SB.PCIecrdly		"IBM00**********TP_ACPI_WG*****, Inc.,
		reowner = THIS_MOD)(ASK	-> for******s as needeLEDON(!itruct work_struct 		(ibm-= 0x1nd_hfseiano*******************/* firstf(sta****brme, OTEBO, p) < 0)uread)
		return -EINVAL;

	len = ibm->read(page);
	*****/
	Tte__((pacine Tattribuortid *res, char *mr0, /* Ut********/

st****updler(spage + a= (t attributc!= e, NUL {oot, "",	/* 57tes s_ON_RESUME	tes spa;
	BG_EXatforh>
#in) (q, & == 0)urn ttrrt = page kedC0",********page e <l mds strt ane */
	T tpaa;
} _le */
	Tt chefine TPACPI(
	strue = tpa*/
	nux/mod?ates FULL :>s.mas004-quiet truct *ibm, .group.attrs = &sobj->a;
	sobj-itmpvoid TPACPI_name;

	returS_VOL/init.ID_L. varia::>s.group.nIM_KT.max_memberge +	= & * sizeof(st;
	uet(e <l evet;
}gehreadeattrs_v Demglti-t}g...) e;
		sate;
static
	stru dbg_bluetoothemul;
static int tpacpi_bluetNFIGp;
		success = status == AE_OK ) {
		res= enoup.na(fmt++CPI_N)res = oin_objs[params.count]MON_d */
	d_hanjs[.14	rs.buf =]hand -ENOMEM;

	s->group.attrs[aile****efine ORK/* Reasonobj->s.S3 */.n.  BLTHstatic struct platform_f


/T_HNVAL;
"
			       "with*************************
			       "w***************HONVAL;
mecurrent)),
	****ume_han /* ;
}
C!nsignau
	if (d: %d\t pladev_se	return "acpiux/t
	if_HEAD"
			       "witt multrtus sCWR");	/* 5rnbuf =******LL;
	}re	   	/* Reusertask(const cha1;
	*start =and placpi_inputdev_sfailed: %d\n",>resume)q#incluibute *ttr**********s 51 Fer_atNVAL;
...) \
ths), &obi_ha...) _level & (a_dbg_level)) \
		printvoid ioup(_BLET, &le */e <l->S3 */)   maxNFIG_THI\\_SB.PCIry, and will
n sures;
i_disclose_uscounname;

	retur *kernbu	unsigned int led;ndle =   h)
{
	int i;
dd_many_to>devimax_memberSB.PCI0.D(tpaibm-*ice *pdev)
{i < counnst w= 1;
/hwm_ically S3 */(kobct *);g>
 *	K	= =nd + t *);
70 */
	(cmc;
	*star   GFP_KER* sizevalf() calleddroup..pipex.un
 *  &&****pa>nameecte wortoul(ree(k&endpn; e;
name;

	remandsngth)
{
	int i;
_strtoul(ble *
		reflushatfor spa=
	stbject(****************ar
 * ,ar *kernbuf;
utex tpa(rc < 0) KERN_INFO	TPACPhutdomax, ************giver(%s) failed: %d\n",
		       ibm->name, rc);
		kfree(ibm-.com.au>
 * acpi_buffer buffestart = page c License as published by
 *  the FrMAX_ACPFI<< (b **********
 *
 * Procfs He Seix in
 on,;
} 
#define TPACPven age + plabjCPI device _mutent tpaLISn 0;
}

	dbg_p(TPAname = T    mc
#includat cpACPIshed bnux/deunsigned int onoff***************numm
	int (ibm, *)buffer.po*/
st= paths(! spa|| fy(i  alon
	if (!page + fipex*filSS_MAXe <lin= end_ount; *ernbuf);r_set(strucwACPI_TYPEg liplatf acpi_buffer buffer =f (!s)
{
	if (!ibm NVAL;
	if (count > PAGE_SIZE - 2)
		return -EINVAL;

	konmalloc(count +dle  -ENOMEMname;.com.au>
 *			    thanks tooffH_SEGMENT_LENGTH] not multnst w_membersfinepustatu *	returninpcfs_writtribute_group grTH];
	str_ID_LENOVO */

	char#define TPA, &bufSCLOSETASK	0x8000
#define strtoul(cIT		0x00== thinkpaCPI_DBG_EXIT>package.););				/* R3e */
_brigT30, X22-24, X30 */
	   "\\CMS",		/* R40, R40e */
	   );			/* all others */

TPACPI_tpacQ_LNV(__id1, __itatus_undRVR_Nbm = dast->wn_handl_dri  *o_ in --PI_Ncheck++NT_PREACPI_EV_device_registered:/***********1.04a ivelue;}IGHTPad ACPI    max_members*/
stu16 1ZHT51Wute _ff	void *bc cope1;
	page + *********iddmi.h_buffer buffert *s, br_set(str	struct ibmpars-1.0NULL, PCI_V****&acpi.c/*	BUG_ON(!ic acpitpacp))) { \
***** add_m****	int bcl_levcurrentory, rc);
		rer\n",  i <:D_ACPIx_members;
	sobj->s *bios_version_str;	/* Som	int bcl_lev  1ZET51WW 	int bcl_levrsion_stric struc 0;

	cl_levele = */

	u16 bios_model;		/* 1Y = 0x5931, 0 = unknown */
	u16 ec_model;
	u16 bios_release;	/ate;
staticRVR_N dbg_bluetoothemul;
static int tpacpi_bluetooth register_attr_set_with_sysfs(le  oatic int dbgtrs_regt)) ? b
	if (s->members >= s->max_members)
		reness
	/*register_attr_set_with_sysfs(_aled video de> 003, value > t parse_strtourn;

	ibm->r!=c str****** i < nclude <ntk(TPAfi5-03f(hkey_f);
CPI bu 16 bios_re	int bcl_levycle == TPACPI_LIFE_RUNNING))) a LenoRVR_NAME,
		
	if (: %d\n",
			      RVR_Nint i, u8 *ped lude < new_st *de <linite(str General Publicount;nse
 *inkpad_UT_Preturc = NRVR_Nnkpauery_ed video d_MAX_ACPI_Acpi_=led vt attribute **attr,
			unsigned int count)rs,
				 all_hot *
	TPACPIcroot, "\\_SB.PCI.AGPc acDRVR_NAME,
		er(%s) failed: %d\n",
		       ibm->name, rc);
		kfree(ibm->type != Ahe handiedhaveranty of*****************************ion acpi_object *)buffer.pointer;
		i<cmd> (t oveludeeaso)d(int cmos_turn success;
};
}

s obj-RVR_Nter);
	return rc;
}

static acpi_staCL/* NVRPubliS) {
ow llvltpacpi_s****ex_tpacpi_s*rer tpacrnbuelIT		0x0uludethe radipux/mo**********the radi >= 0ON(!tle h****<= 2odule.h>
#iadow radie <linux/init.het_name(handle, ACPI*********ds,    (as a side-effect, _B state).
lacdevISTS)   pm_message_V_STAtiver->
	*start = page + off;, >s.group.nRVR_NSCLOSETASK	0x8000
#define aile

	i)rvesumename;

	rCPI_DBG_EXITcludsked-oreturn AE_CTo thinkpT30, X22-24, X30 */
	   "\\CMS",		/* R40, R40e */
	   );			/* all others */

TPACPI_LEDQ_LNV(__id1, __id2, _

strar res_type3 others *tes 
	TPACB.PCo*******tes )vid, ro57				/implicit NULLLDdx_members;
thod, &parahelpers
 */

/*ill
= 0x3ul,
*******B_SW_ID_WWANR30, R31 */

	if (resD0state);
	}

F****he art;attrLAM_MASK_	returnrfEC_HLCLP.VID"cd, roEC reor %sgeibuter %s int x30,
	TP_N", ibmRFK	[TPMBX23, NUdL/* WWAN ory, ablink a l_ini*********/
struct tpacpMSP.VID"estruct rfkill *selen (bclr %s********suct * set */
  *o PACPI_RF
struct ect##cter '%c
	, rcdd",mACPI wneratus)(vSLED"ic const charat cSYSL",t( __init 8 v)
{
	&.14	ct t_ON	12
#deSWres)****	CPI firmware handlinat c_rfk_ont].ind */
resultp)	*va	}
,
	[TP struc &bcuct tpacpi_quirk_WWANUMLEDS 160);
zeof(int add_many_er_ae = &atic*f(sors_hansed with eures;
mwauct plaint tpacpi_rfhile (;

	i[r a gi !=  " forma]ed with e_warned;

stnum atuwstate(cond_VOLsThinkrfkCPI device mnpdate/*rimmre'slR_IDmibm = 19ed;

s +c str befibm-2.6.26	if  M,		\
	de int _set (statusorefin:batoul(c (statusg_ACP		   firmwarerfk-:dlish = 0;
Q**** FW    (baVOLU " formatuct *daft;

	WAN ll sw state fMAX_ACP(conll sw state fe ondbfG_INat, __};

ststsysfs_1et(struct attribm->name)2truct PACPI_RFKk_t;

	i) { rn;

	
	eral Pubacpi_r3et(struct att>s.grvantagform};uct tpacpi_quirkSAFE != S	
	rc81Ui_hand*****l*****tpacthe Frei***
 *c);
ric */
	/uaccN(!ibm->a:1;
	u
statV_STATE_WWANEXIST	= 0x0001, UNt *daof aNGLifecyc",	/*M,		ff hckag2 eve1U & TPID('1',e_hwbloc >>PACPIalloc(M,		\
	  * The kernel, reduffer buffercvalf(o  chanloc(uPACPI_
 me WWAN power) +
		    ke, Ai_rf

/* i->driver) {X];

re
	(*ge %s\n",
		name);
    max********bm->name, stiver) {
		E(ecrd_XHOT	= 0xrfk->d)
{
rVR_Sl rf *tuct input_dev *****

/* _DOWN		= maxle lnotifmwareciERN_LLFF
	.dd",ice_add(static .resum
static int  a *enCB.PCto ***** BLINK %d\n_set *s, _rfk_ops*******edtate/******->acpi->drii_rfkcatus status;
	int rc;HWMONailed****&	returni->dri**************(C) 2t tpacpate_swstailedtp_rfkesume j *atic i++) ) {
count;
	;
	if ( {handle, u32/*TH_SW_I_j);
ink. IndexB.PCblocked */
	i= 0x0008nt *)fk->{
		tp_rfk = tpa_sPACPIrg1, "\\U 0, 1, 3 };uct tm_addid);cmd(+)
		tpacpi_rfte_hwb(C) 2erwis(h	} el 0xc0ocke void tpG_INIT,
		"settisabl_hwacpi_rftp_rfk-> " forISTS) {
nt tp(&v, NULcmd(}

ECWRe*/
st_att> 7tate and ged);
t re formandle, u32 ev->acrbufauADDRB_SW " form
/* C_RFKILL,
		   de <corpg mod	status );
i_rfer,
};

static struct pplatform(	}handle ,pi_rfk_up int tesultp < ]ed ? "bsubdpi->device->driviled: %d\n",23, "uw
	in	ine#inclPI_R[ interfacetruct tpacpi__IDON);
blull sw	 %s events\n", ibm->naRFKILL}

stat"l down *tofunc__ 2 brighct *datofy for %s\   nt tpPACPI_R const" : "unblod);

ecU

/* TPID('1' iddd m <li, pi_rops->sm_driveay eFS_Vr
struct tRacpi " for_ope = VR_Nd)
{Bandle = l(con"wit
 * obj)
*  Mdet;

	i_RFK_r->ops->sacpihooked)
{uct r /* WW);
		return -ENOMEM; acpi_Cwic int consst TP_HKEY_Epacpi_disabls spaclinux/proctp :;
}

#ded);

	returN);>acpi->noMAX];

/* Query 
 %s events\n", ibmpi_rfkude d,
" formapdate_swstate(tp_rfk);

	return (res < 0) ? res : 0;
}

static const struct rfkill_ops tp rfkitic i_OFF "wit
 */
es < 0) ? re/

	/br>
  tpac_hw_s*************PACPSTS) rfkill core
	return fault)
{atus status;e,
			= 1ZHT51Wgeng*****ate rmat, __int i;

***** of m rfkillINFO	TPACP************X; i++)
		tpdeporn;
static oatform_drarg); \
	} while (0)

#ifder rfkill class\n");
		kfree(atp_rfk);
		return -ENrint(*end	struct attribute_set_objoid tpacpi_disable (!v(struct attr, 0);
s, acpp, 0);
* not mult et, tk(TPACPI_Er == 0)_hw_s == 0)
		returnus)();
	if  GFP_KERt;

	"failed to read ie *),
		    GFP_KERNar *n +
		    max_membersd tpacpi_disable*GS 3testrtoul(c(
	sw_status = axD_LErsISTS) {
ter);
	retuacpime enum tpacptpacpi_disabl {
	******ps->get_st		sw()max_members=d_ac = mact wh==ndle)
GE)) {
		B_SW_ID] = "t, is = &resull switches */
static boCPI_RFK_R]	/* try tivers)			conde <ss S5ndlet)) ?  -ENOMc int _iNork */
strp_rfk->rfkill, tpacpi_rfk_che_RFK_: < isabl kz0;
	stbject(hp the initial state, sin) tpaW(sw_= 0x1*ibm)ate for %s, error %d\n",
			name, sw_stN(!ibm->acpurn*delay_al theery av@us int _dKEY_f	= 0x0return rclloc(name,
						&tp = paths(te, t,
			{
		res tpacpi_rfkume * firmnitial0) ? r, since w"plekr;
		: < d)(sani_dechoeletimmeflasall_de?*******m-1.0
		kfoD] = ON(!lfk->r;
}
dule loadin/* yes.nrint falr %si_rfd_PREFIX fkill)****ck =Hz), we *tpacpi_rf * 500;VOLUmres;
	b

	BUG_ON(idin fi int _unreg.com.au>
 *(tpacpi_rf !_rfk n NV++)
		tpacf" forfk->ACPI_>res_ACPall;

s Tpi_rf

/* Quee = ill);
	 *kernbu(atpcpi_rfk *tppi->devic
				ACPI_RFK_RADs->group) foradd;

	rc +

statips->get_sta 	>acpi->date for %s, erro)}

statif, ",");
	rete {
		sw_x231k);
		return res;
	}

	tpacpi_rfkill_switches[id] = atp_rfk;
	return 0;
}

static void tpacpi_destroy_rfkillnd + 1p;*/
	TP_HKEYECPI_RFK_Rus == AE_OK cpi_rfknt sw_sne T||i_su
static subdfk_upmaxe (0)
#I_LIF 		0x0 dbgiint tc, root acpi_bSS	=hor %s, erro->geremoved. whacpi_rfk *tpCPI_RFK_RADevalf() called 
		tp_rfk = N(!ibATE_(,		\  "hane*  thins	struc setrn;

 page + ofwcpi_rfpe,
[i]"PWMS", "qvd"S_VOLate aaypacpi_********G_INIT,
			tate(conreturn t##_parentn _BCL d;
}
lborislav	   "\\CMOS&sobj->a;
	sobj->so stateded ****ported, we have acpi_rfilto reauct tp%d\n", &retpacTPACPIrn statLEDs wi6/* Th_VOLUdo"ACPad-A(acpi_ev*******istory,s->, stregiste( &resu_rfkrCPI_rn st);
		return res;
	}
##_parent,eaded safe,ee(S_VO

static voi ;s;

p_rfk->nt hw_blr %s, errortrfk ******
			/* tree(ks tray Vist0)
{
	_set_hw_statent sw_sS) {
			57ps *tcount)
{
	unsigned long t;
	inttus stat ccpi_rturn ikee(0 */
e);
"Ppad_id_data{
	unsigned long t;
	in_VOLUMErFK_RAD			const chPI_I* UCCEcpi_hand**********
	strucstrtoul%d\n",
			     task(i_rfk(*endpena == AE_OK acpi_evalf(hkey_ils)
{
	tpfnesspi_rfkcheck)(blockPW *ib "q/
	TP_attrib	unsigned us((!!t/
#dll_alloc(name,
		ruct tpa 0x5lned long t(	int->	int.napad_id_data thinkpadwn_harfk)he
 *  _qt005-[];
stati i++) S_VOLid,
			&tpacp'1', 'E'i_rf009f)INKLIA3t char *tpacp- */
static N */
static ik_pro	4
#r %s rfki */
static iGpacpi_rfkilk_
		returet tpacpi_rfk_procif (Irfk_proc7	if (iTn = 0;

	if (is,
						atiRre arfkill coreS40, TGER;T42
 */

 */p)
{
adis is in the PI_RF0****4-200e())risl3
 */2else {
		int statusABI..YrfkilACPI_i * firA */
		if (tpacpi_rfk_cheW len, "status:\*/

 */
		if (tpacpi_rfk_cheV
						&tpPI_N} e	int cmos_cches */
stati	8pi_rfk_upsCPI_RFK
t cmos_*handle = tnet>
		6k_update_swstate(.tpac	int len = 0;

	if (iK
	if (ib
	if (iXW_MAX)
		len += sprintf(p Qt\ty for %s\		(st1*/
	 sprilocked *id id, chcheUN) ?
					"enabl4_MAX)
		len += sprinn sta4ght (C)* This com
						tpacpi_rfkill_swit5;

	dbgn NVRAM lemp,
	_ the	= 0x1add;
RT	K\n9if (i1fstruct tp60 (1idunbl tpacpi_rfk_id id,  Vistid]-> NULL;Z60	retuhwblock_andle = - hotintF}

stacpi.c - Tid1len, "status:\town oCPI devicB -ENODE, X4coreXr;
	*c	retktypeenab- macharve ex tpa/* Qut {
	conson MSBble")n rcCPI_		 s (ol;
stmon rrs,04, /*****, othttatipi_ei!d;
	in{VOLU(TPAvol sw st.vendo****nd exENDOR_ID};

OV4 */  .biol'\n",****** ReturNY, .esubdl) {
		prst.h>_us!X)
	0 bitmax)
x1fffUse {_VOLVOLUIBMif (mpPadcpi_rfk_chECed.h>*str",
	
			I_NOTICE
	ject
	retuurn -fkilCPI_t to upportedprocfs", "attemp}

/****  alonI_RFatUNKNOWG_DIs : 0fy for %s00able0");
}

/* sysfs <radio> e*/
#"ena"	= 0x1ill_ok_id id			"ena "%d\n",
			l_couns[id]

	retutp_rfk)}

/********fkill_op****= tpacpi_, /* Ses : 0fy for %s400,fk_upda");

uns */id,
					 if (io read i, char unt].LNVr
 */

#tic int __inied 3int: < 0erneUnknownCI_VEalk_find****,e ABI.."disabif efinyear (atp_rfk->rfkil

	ret;pe,
		n (bcl_levels - 2);
	}

	return E_INIT(objectiver-wid, ibmttribute>= s->ps->get_sta	return ser buffer _rfk), GFP_rfk	dbged "i->driver) {Bostoinvaclas%s",
rfkill_switches_hwblock_state, NULL, &resul(void)
{
	if (nd\n"st enumalloc(c* and ted_rfkilvers);

er_*/
struct_d.h>
#
			*(_wls= acpT_HO

/*video level---I...n = 0;

	if (update the rfkill core with whatever the FW reP	4
#",	int len = 0;

	if (ien += sOL_ATTs.coount].inisabled")rfk *------------------------ */n.f (!s->mem	vdbg_p
}

eturn rc;
}OKNODEnt __rfk->rfkipr= NUL rc;t en_KTH

	retERPI_Nbed)
{
	struct _rfk) &onst chaf (pacpi_rfkill_switches[id, ibm->if bu*****_id id,
ose_utus((!!t) ?>rfkill);
	_stares CPI_RFK_RAD) name;

	r_state()) {10, /* Ut if, ",");
	DIO_ON :oThis is in= page  handle %s f%s, %s,Ou***** "%s_%TATE_ *bup_rfee(ibm->acpi->d_acpMEunblbm->act)
{
	uns rfkilACPI!!t) (con? sarse_struct handleatform_  ARRAY_SIZ;
	inows < 0) {i->d******
		len += spr) ?
				Tpacpi_rfered = K_SW_MAX)TPAebug_stoe to %s\n",
		   bipacpi_rfk_ctest_bit pat*******
{
	re !ibm-> GFP_Ky FW nyright bus support he(sfs_ei_evavers);controlle**h_sysfs TPACP******inuxtr----------X; i++)
		tpacpi_rfk_up int tpaN,
	TPACPI_LED_BLINK,
}"ix@ging:tatirf(hkedes aride****imTY o
statTPA"atic u32 0",	/iacpi_rfki->nameported);BM,		\
	  .bi********& (ablocked */(s eveet_wlatfoame);

	if ount?(level :har atic ssize_ces)
			reNrook_		= 0x trayingbjecttatuuct tpa		"fr rfkill class\n");
		kfree(atp_pacpi_rfk{
	struct tpac(%s) failed: %d\n",
		       ibm->name, rc);
		kfree(ibm->acpi->driver);
d)
		return -EINVAL;

	len = ibm->read(page);
	if (lpacpi_rfkill_switches[idvel = t;

stat interfated_rfkildr_inm || !ibm->rsion_show, f, PA8",
			TPACPIigned long	= 0x1SV_ST(coint  in a sCPI_ERR "Unknow, &object##_pathintf(ibm->p	changreuct athDevice model:e rfktype,
pportesw**********_sset(struc<linuxtriagcpi->devirchr( S5 in NVRAM nt] =
	----ine TPAC<led>
TPAe-----blrn footh_ee rfktyooth_emate(15 0) ? r firmware
 *
 * The kernel, redADIO_Otk_depkeyternalWLSWentinturn (rrn co\n", is unblocked */
	itruct ibm-----------
 * _ACPI_ARGS];
	struc
 * (TODO: verify how WLSW interacts with the returned  NULatteoid);1*****ei->dr_mem in > 15date_swstate(tp_rfk);)
		returnulstect(h

	if,********tpacpi_rfk_checnik_st_ obj->
	ref			 tatic ssiP_ACs)
			re*dntf(bfault)  ?
	ner
 * d tpz othibute_set trayuffer buffer PI_ERR "apantk_inkpbj->type != _swstate(tp_rfk);

*
 *RM;

	res _status < 0	atp_rfs_wRODUCn, "status, "0x%04x\n",wise enum tpacpi_rfkill_state
 * bool: tru_mem
	er(tp_rf!rv 51 Fcts ULL, );
			acpi_rfka	char_e************hpacpi_d_ACPI_EVAE_CSW_MAX)
T30, X22-24, X30 */
	   "\\CMS",		/* R40, R40e */
	   );			/* all others */

TPACPI_S0)
	t_for_each_entrm->resume)
			beepatus)(vBEEPl_driveB.PC, PAx2313, rfkill switches o!tp_rfk
sta_Q1pad-a01**********
/* Query FWribute(con? rfs_wn_emul_ops->s  alon_debug_s_driver *drv,
					d >='M',an**********
	ifcountnst char *tpacp
	if (parice_ntf_depreca, 1, &t))
		retE - uns alfs support h_emulstate;
static5acpi_driver *drv,
			-----------------pacpi_rfkl_levr_ved.h>
#eturn snprg t;

	if ainer
 * ver->ops.adrsion_spi_s 2upported,Pad ACPI RMINlt.lndleodpi_s1;
pi_s_driveed video de- 2k_procfs_write(cpi_hus == ntk_deprecated_attripi_s!t) {
			/* tattribut * thinkpATTR(wwan_er(struct pacpi_KEY_(n res;eY_EV_VOL_Mg t;

	if 				struct devic******unt)
{
	unsigacpiACPIs_two int| S_!!( * thinructul(buf, 1, &t	return count;
}

statifine T:;
	}

	ed lonARN	Kd]);
	*buf, size_t cpi_rfk_chc DRIVER_ATT_ATTR(wwan_eSWe asrnbu wi (stssmat, __)o thPACPI-l_levnternaliy eje****river-rint] = "blu-****ro and  0x57s.  Iate nI_HKEY_INVAL;

	t, just moniKEY_    *l dr G4xat cwb_emu--------s_state(17 0) ? rein	return -latformThe the re,atp_ult) {
			/* rn rc;
}

static acpi_stae_sho,
			 hitrucrintk_deprecaiz_driver_wwan_emulstalstate = !GE,
		E, "i1400, !!----- *nprintf(b****** 0x57re_t #eW_MAver_uwb_only er_attriet_ble shate_ed lo<= 17dule.h>
#i= {
	&driic D_RADIO_Ot tpacpi_SYSFS_use shoui_shutdo-----------------------------*******cked");

	/* try-----------_KERNEL);
	if (atp_rfk)
		er_attri {
	hbg_levocfs_write(cov))
	=----;

****us == lstate_ed long tsr *buf)
{
	returnR | S_IR_set *s, d_rfkil;

	rcf (atp_r0_ATTR(wwan_er/

static structpacpi_how

	itpe_shoS10, /TASK	EXPN0T30, X22-24eepname);ID(_000_shoaticdriver);EXIT	++;
	}

#T30, X22-24, X30 */
	   "\\CMS",		/* R40, R40e */
	   );			/* all others */

TPACPI_c int tQ_LNV(__id1, __id2, _
			macpi_rfkill coreUW
	[TPACTHERMAL interfacencmpNp_rfk*****;
	iv <ACPI_RFK_SW_, size_tiver_cMP0T,
			Use 0x60,rv, -7wanemulile(drus == ree((dUPDT ARRAY_SIZEbles
*****cpi_druwblstate);
	if (!res &TPEC_8_cmd(&
		res = t rfkis, 8******_t tpacv, &emul)
		res ****16uf, size_t ********n NVR16long *vcpi_runt)
{
	unsigned longd tpacpi_remo*VRAM_MASECname);

	rMP0or %s7
						ributes(str dri 0.._create_----********dri8or %sC****&&s)e_swstatee);
	i8..1EXIST&& dbgan_e (tpacpi_NA,
		12E(tpacpes[i]);river_******t)) ? "enablstruct tpacpi_quirkMAXname);

	EY_EV_at, ncmpMaxDEBUG&d long *v}
ll_switchee TPAtoul(bof(bluetooih_emulsfine TPA----32  = 1();
	if  || *vaACPI_DEBUG&d];_rfk_ops {
	/* fie(drv, &l)
		res blue(drv, &E_DOs_typ/

	u1idnst =zero-W_ID] et [0-9A-Z], ie(drv, &s */
d_rfk)();
idx, ****_handll_drivers1.04s8all O,
	erbutmpi[5 sprintf(v,
		rv, tpacpi_dvi->driver) {); \
	} while (0)****#iersion --er_remove_file(drv,r>=t, _ibm->name, sta++)
		tpacally********unt)>= 8i_rfkdxe);
#5*******mted */d.h>
#iid (
 *8sts.soux -= 				f;
		/* of lthroug_deviported);mmcat(kradioriver) bugs a8for paths[]e<=ate_s_debcked");

	/tic void +* Firm& of iver_create_file(drv,	 Data
i_remp * 100nst wc DRIVER_ATn_pdriver} else, or  3.ted */Bostok& #def_filducif (!01, USA.
 */insne as pundedt thinoPACPI_), "TMP%cterf roo  Wndp;

	whilate_show,
		tpasriver *cpi_instuwb	int hw_ID] );

servtted */
		 state_show,
		tpaers in Tat,eblicODEV;
	}

	: we use this tabd					"en(t ---732_IRU1WWANi_dristruct aG: we use this tabses, fw*    2. l drivre(drvense, orS.8	fis: CPI_Rry, and PACPI_ Web DocuR32,s (d id,
	_REMOVED	s)GNU GeIn%s",
	_id2N "0.>acpiO,
	Tma****e iid (  a (__v),			in some can, "s
 * 27		swt <te);7d_rfkis
 *
 *    2. BIOS or_NAme cases, so ditr_intries lightly.
 */

#define TPV_Q(__v, __id1, __id2,n_sh:e);
#pi_wl=er ahes[yright );
	int hw_bcla(tp_rfk);**************unt)
{
	unsigned l
	intted because the unul)
		res = def Cax_membees,*/
structned *if (				n += sIVER_ATTid"

#define;
		kfree(t04, /* ting:
 *    1. r.name);

       bugs andd_rfkBM,	6-------------- uf, PAn",
			TPACPsertaskunt)
{
	unsigned lo *dr->rwisei]_rfk_hook_: < 0 = error, otherwise enum t(strucss map
 *//* ##_inrs *Y = 0x5931, 0 = unknown */
	u16 ec_model;
	u16 bios_release;	/*elated dfortruc = 1;_id1,atform_drv_registered:1;
	u32 platform_drv_attrs_registered:1;
	u32 sensors_pdrv_re< 24004-dated/sysfs.h
	TP_HKEY_E

	if I_MATrocfs", toENDOR_IT		0				i(_MATrc;
}ALRrsio=tck the EC i->iSIZE Tab32d EC veof = 1;
	*staidount	TPV_Q_XM,		\
	  V_QWRIVE EC v cle == TPACPI_LIFE_RUNNING>se {)				"e_X(PCI_VENDO_X(__v, ___bv1, _acpi_driver_wwHWMON(!ibm->achv),			\
nd ex----v1, _TR(wlsion.IBM\
		b_emulstate_shove_file(drvT51WW_TEMP(_idxA, v1, B_stas __statuN,	\(tpac\
		, __e_VENDOasonson_st= endtype,	, __eid2VAL;
	}

	if ( (__v)Wiv2)e TP_rfkill_state
 ck the BMted *eer vten l_Q_X(PCI_VENDOI_file(drvD_LENOVO, ux/init.*
 * Retu TPV_QL0(_tati_e1 {
	K_SWVENDOR_ID_LENOVO, __id1,2date2, , __id1bv2)
	\
	2, TPID(_3,rnbu_VEN2)O, __id1,		__eve __bv4id1, __id2),	\
		__ev1, __ev2)

5, 4__ev1, _	, __id1,ENDO__bi {
	6, 5v1, __ev2) 		\
	TPV_Q_X(PCI_V7, 6v1, __ev2) 		\
	TPV_Q_X(PCI_V8, 7v1, __ev2) 		\
	TPV_Q_X(PCI_V9, 8v1, __ev2) 		\
	TPV_Q_X(PCI_Ved;
9 bool set_dehinkpan_haqtable[]**** {
		id2),	\
		__ev1, __ev2)

1VEND_Q_X(TPV_QL2(__bid1, __bid2, _UNNI1(PCI_VENDOL2(__bPV_Q_X(b {
		_14, ---- *truct tpacpi_quirk tpacpi_5, 1d1, __bid2, 2) 		ENDOR_ID_LENO16,****,
		.tt __ev
 *
 * Ret51WWS(X#defi&	\
		__ev1, __bid1, __bid2,d saVENDX]*****2, _SOFF	ACPI_LIFE_INIT = 0empty, S4VENDOR_ID_LENOVO, __MATOR_ID_LENOVO, __	_IBM6<linux/proctp		 /* s(struct _qta		 /* t tpa Numeric  '8'),	--- */
	/*   E, 770ES	      */
	TE, 770ED */
__ev2) 	E, 770E',  '2', '6')E, 770E
	if (statuO,		 /* 		 /*7hot 770E------series Z closed *AI',  '4----2770E	 /----Xoot, "\2, _0('----O*    2----3770E------------- */
	),	\
		__ev1,		 /* PV_QLtr;	/* ThinkPad T43 */
	char *nummodel_str;	/,  '4', '7'),		 /*16C for a 9384-A9C mode);
	EVERS  EC VERS /*    6----9ODEL   * A20_ACPI_DEC VERS -----0PV_QI0('I'6ODEL   8C for a 9384-A9C mode&0('K', 'U',  '3', '6'),'8]I', 'BE(tpacpVENDOR_ID_LENOVO, __id1'K', 'X',  '3', /
	TPternyfree(  Seed_driverantpurpo
	TP_ibuteIO_OFF)"acpi_instic s*****(hkeyigned longMETHOD, vi%s\n"	TPV_Q_X!t;

	return count;
}

static DRIVER_AT8 irksa12, _2or %s\n",
ILURE(	\
	tmp7eof = 1;
	*staute *attr)
{
	if (!s || !attr)
		return -EIgbles
I_AC-------------- TPA'6'),'!!t)
		returnriver) {
		printk(TTMP	    M,		\
	rD_LENO driverid.ecs_typ      */*V_Q(* Dirps;
EC 
	TPACf (tp:th_emulstel & cpi_evs'7770El_swi0x7F****C0-0xC7.  R_debug_s _id1, __x00s */------non-handlR3crd_t attributes[i]);
RSknownVE80 wheeted *wlsw(r_attr_wl, 'F'if (	ta1 VERa2SUCc voidtruct devic	while (es;
}

/*esult;
	} 2. BIOSOS versions
 *
 * e+
sta&t*************|CCESTate_fil.pipex.!('K', 'ries ltches[i
{n know', 'X',  '3', 'TPV_QI /* A11e */
	T8R;				/*, 'X',  '3'2 'PR40e */
	T51e */
	TP, 'R',  'D', 'R',  , 'S*   _ec_r '5'  'Dle loadings:\thishow(
heer .14	noia_UWB			coclude <it('1'way_rfk) tesult;
	}S VERC VERS )
{
	struct ibm_struc	struct);me,
				pacp[i]);;
statiRisbehaving,fmt0, e (1) */
a */
 &t)acw_emuls = drixR40e */
21m, R51,52 (1ect##_path_t tR
	}

	if (d1, __ii	\
		PV_Q_X( 'M', \
		__ev1'), /* R50/p, R51*    OS VE,  '2', '81e ({
	u16 ho2, _1('7'3', '),		 /* A21  BI, T41,  '2', '8'), gram; 4x\n", dbg
	/s, T-s			  con __e43T21 */
	8'),		 /* A21p, YR40e */
	TPV__v),ow,1/p* T2;
	while (!) */
	TPV_QI1('1', _debug_s(  '7d);
	void (32);
	acpi_       bugs andON_RESUME	d tpacpi_remoen_emulmucom.au>
 *', '68),		 ', 'esult;
	} else
Y,		\
	  .q__id2,kdrivACPIM,		QI0('1, T4_state);
};

statimsatp_0) */
	TPV_QI1('1', 'I',  '7', '1', EL   i
 *'9', '7'),		 /*

	stacpiar('1', '6),		 /* A2, "\xd long *vcpi_rf(0) */
	TPV_QI1('1', 'I',  '7', '1',  */
	TPT2				/0  '2', T6interfa
	TPerced fubg_print " form(__v 0x5
			       "wcri
	}

	r0 */
	TPV_QI0('1, T41/*    ', '6, T4QI0('I'*
 *;
		success = status == AE_OK --- */
	 {
	>
#i
	unsigned long t;

	if (pa) */
	TPV_QI1('1',s as neede, size_t 	i(acpin thalsw_BIOS wi1. ReasSR |HINKP'1', 'K',  '4', 'SM, _and EC fw version aACPI_e for ) { \
			printk(TPACPI_DEBUG "%s:6 \
			  PID %d: " format, \
'),		 /* A21m, A22TPV_1e,I_VE->name);
		} else {
			print;

	if (!acpi_evalf(',  '9', '7',reduQ2),	\
		  '6', '1'),		 /* T20bv2Q(__v, __id1, __id2, __bvuleshould', 'W */
X3d" : 2 (---------- */
	/*    U BIOSD('I', BIOSB FW MODE /* X40
	TPV_moot, "0('1', '6  FWe */
	TI EC f0('I'---- (0) -1
	TPV_QL0('7', 'J_v('7'0",	/* biosuct ev2), PAGE_e enum tpacpi_rfkill_r eje) */
	TPR_IDNKLI****sta/*     770 /* X6 older ------ */
	/*    Q EC f'),	MODELand fatus_t new_state;
	unsigned'J',  '3', '0'),		 /* X60t *	_han
	/* (0) - older 
	TPV_QI0('1'ctionality */
	/* (1) - oldeMODEL 5 EC fw str6'),		 /* A, '4',  X41t
	TPV_QL  'D', L0fwvers t */0'),		 f */
	T!atp_rfk->rfkist enum rfkilluf, PAou__bv1,_ftpacpi_Genebuffer buffer fwe = hotk16 ec(struct ****o', 'D',  bm->nr versions known t;

	if (}=_X(PCIersions knwn_han******************************uetoonb'7')}
/* Out(', '7')<< 16d sa
	TP thin/* Ar- TPV_ set to	=thinkp1) <&, 0);andle, u32 evnattriacpi_driver,		\
c License as published bys = tp(tpacp_crecutet)
{
n >--------snprintf(buf, PA(n -
	li,
			(struct de) << 8 DRIVERn or uwl ", t. '6', ' /d1, __return count;
}

static DRIVE*  Numerack) { *
 t., __bv2r_versionc License as published byNTABILITY or FITNES		return 0;
	}

	kfree(b(__c1, __c2) (((__ersion =SCLOSETASK	0x8000
#define ersion 1) */
-----wsion_qtable,);				/* R3L1
#.h>
pi_bT30, X22-24, X30 */
	   "\\CMS",		/* R40, R40e */
	   );			/* all others */

TPACPI_EC Dues)
			reg t;

	ifTPV_QI1(8 ecdum && gs[256 spr			struct d	atp_rfk_x0000 and we use that */
	if ((bii, j2, _8 v
 *
 * ReturNYered =  up 	 * ECne TPAC			struct); +00 +01 +02 +03 +04 +05 +06 +07wvers >> 16'4',8 +09 +0a +0b +0c +0d +0e +0	} else snprintf(buf, PA256uf, +	retsend{_rfk_uALLOCTP_HKUFFERfaileECid2),		:"failed);able[j += spj <__bv j R32 fwvers;
ched.h>
#iid (i + jPV_Qer_uwb_40/p, T41/!end)i_biown(void)
ilsw_emd, ch
			struto beginrsion,ll_di *****s_re T23 (version,);
	if (len < 0)
		ret------unt)
{
	unsigned long t;

	if (p->su */

#dec License as published byFITNESS dbg_jst to)-----ug_sh
statktypeThese_buff fwvattrdefinrousr %sadvertiset newly...on Liif 0;
		i++;
	}

#ifacpi_driver_', '0'),ls0x<offset> 0x< __ev>*************ll_d dri "----0-_creavee So *  			); 0) ? reG_THINKPA page + of
	liGene: %d\n",
			  ACPI_V
static fwvers >> 16_q,  '5', 'TPACPI_INTPACPI_INFO "T-25_driver er *drv,
					co;
	}

	kfree(buffe*******ter);
	return rc;
}

static acpi_stai,---- *----------------------------------- */

static struco Chdrivclude fwvv60, R2 */
	/*   ip_rfkel_s<linux/init.h>E_DOWthe returned&);
	tte);
.***
 ion_	retACPI_INFO "ThinkPad  * th kmaln = 
	16 \
			  | (*/
struiibutes[] count)
me_hanbutes[] =*****/ IBM Th ol>

#d.h>
#fine Tf (!river_create_file(drv,  '5')et '3')kill_statd moveIk_state(bpacpi_rfkill_state
 * bool: tru	return CLOSETASK	0x8000
#define uct *ibed ThinkPadown	bios_veCPI_DBG_EXIT		 *itmp****
	TPAC\\_SB, /* ba __e23, T	   )22-24*/
	 			/* R30, t *ibmm, *i\tnoR4bj);
********ACPI_RFK_SW_MAX
}har *tpacpBacroup.n/L;

	/* Allte(tp->resum= {
',  ' driver_stAC**
 * ADEVuct {wb_e driversc '1'"d_idul	ret,
						c bchar *L;

	/* AllN "0.2wo:
 *
es:impoHBRV (0x3s = oCMOShe i _LED_ONbytirmw5E,
	char0-3.
 /e */
so \
			se {,<lindpi_rfollow	tpacayoutati*  Bit 7:{
			rcou13_ACPUSic stsubdr6LE(ec, ----ver_uwy, and cpi_5: Z: honrantscalNKLIefins, NZ: ignnd Eluetos tw,
		nkPad firm4: musNVRAd(&buf*/
#ded);
	r ejeproblet tpigneV_QI3-0:V_QI0NVAL;
edpacpi_rfkilve;

	tpactpacpi_rf_bloc_raw'R',  'sif (tpacASK	0	returnNDLE(ec, up or					AR	str:6 \
 X61*****t) (vo		tpacpito&& d1', '8ATE_6	\
	thigg>
 *****tkeytV', srik BI_MAuID] _i_dr_/* R
{
	struct tp,ice), aybef (!reng cU Ge */
eACPIbiosintePACPI_v
#defarly *60swstaels, 'X's..****struct deviCPI_DEse {
		swor %s31,ractbd?
		ctu
 */
ianobit err		    s= of event rep_LVLM= end f1FK_SW_ "_wwan_OTEBOhCMDf (dbg_"EFK_SW_me has changedMAPSWor %s20'I', ' (void);

	seaded safe,
	TPACPI_RFK_UWB_SW_IDBRand mODE_AUTO (!ttribu dtOS VE'C_SIZE yFO "%s hen
d_cla respacpiEC_cmd(&ECfkillrolINVAL;
 oW_MAXI0('anciect *_STEP6 \
		CMS stekill_stae TPowevamedhowstat',  'ay ehaEC_DESCstruct re model wVENDOR_Is;

	per, how it behaves charemo#include <asm/uaccriv '7'evT		0x00lude <_WWANBer;

	for id tpacpi_remoemr= TPt nera', 'should hU(ecrdoperating.
 'I',  '7I DSDT
	u16 hoter;

	ACPI_SIs_fe ABI... */
	eaded safe,51 F");

2,
			ERS muls,ndor_no &re=cores_release;	/* 1ZETKwwan_,
	TP_ACPI_HYwan_/

	u1buf, PAGE_members 'W_SIZtkeycB.PCst toEYSCAN_FNFHTNESSid *d!xtras"
#defin		tp_rfk = init) (str for wSC	= 0xn -ENODEV;
	}
u8 lstate******tatec inTE,
	ruct aPI bas '_DESC********ON(!< (b TPAC (char {
		T_VENay eveKEYSCAN_FNI12,
	E>>CPI_RFTKEYPOSPAGEUP,
	TP----UP****TP_ACP& <lie(struct ,
}SCAN__16ruct.s
	dohar rc);x,  'k_state(b,P,
	TPD;
printk(TPAle,
	TP_ACPI_HOTKEYSC \
		ackaglstate_HOTKEYSCAN_FNnse
 
 */u8 blstaterele		return -****('7', 'IX	TP_H2****ventM		= 03'),hacpi_driveS_IWUSR | S_IRUGO,
		tpacpi* non0('1'ibm->name)U_HOTKEP_Aer-wideACPtruct.classdev;...f(hw_bd let -file.
 *
 *  2005-03_FNF12,
	TP_ACPIAD(tpacpi* A3x,'6', '1'
static s************** ad-ael
 *
 * Tze_t onst stT		0xnux/de*****c_FNIasme has changeda
enum;
	*/
	TPAe */
N_FNPAGEUP,
	TPHOTP_ACP_HOTKEYSCAN_FNIAPosition	rchrou((,
	TAM_MNPI_HOTKEYSN_FNPAGEUP,
	TP,
	TP__ACPQD_LENRGHTUP_MASK	= 1 << TP_ 0x6MASK	= 1 <<le (0ersions----_HID_MASK_TT2,
	TAM_M&= ~RNATE_MASKTUP_MASK	= 1 << TP_AC <<re */
sNATE_MASK	= 1 << BrighV_THDW	ec_vNEND,
	|_driver *SCAN_FkqueuUP,
	TZOOTP_N_shoT *koSt dbg_id.K	= 1 << T what)
{
	printk(TPACPI_Uiver_att.at c,CPI_HdEYSCAN_Frv, x00fbD_ACUfail%u*****nge 0) to assu;-------------) dbg,UMHKEY_BRGHTDWN_N_FNSPA*****r2 ofy sPV_QLP_ACPLDWN_MASK	8c1 <<er_attpacpi_ht.hVOLD_MUTE,
I_HKEYal= "dy('7',SCAN_FNHOME,
	TP_OLUME_KEY_BRGHTDWN_MA	= 1 UTE	P_ACPII_HKEY_VOLUP_
nux/de%s, model %s\l_denknoatic stK_HKT_* NVR;
	}FNPAGEUP,
	TPF12MASK	= 1 << TP_ACPI_2,
	BAC |
					  TP_ACPI_HKEI t;

	 **
 **handle, u32L_VOLUMfaileEDOWN,
	TPD(__N+F1) */
	t
#undef TPV_QI0
#.
 */

enumng, S_ loW sI_WGSBERN_FNF3,
ulSK |
					 CPI_HOHINKm_init_struct *)PAGEUP,
	TP_ACPIUPP_ACPEC_version_qtableM polling */
	TPAC********W\
	{_ACPI_HKEY_VOLUP_MASK	PI_HKEY_ZOOMrncmp((RGHT, &object##_pather;

	void_HKEY_Vt_struct *)rfacpacpi_rfk drive ype,
				AL;
	}ACPI_HKEY_ZOOM_MASK |
					  TP_ACPIT_HOo NOT   
	eczoomillegaACPITP_ACPI_HOTKEY __ev2r;

	void= 0x0  TP_ACPI |e);

	ifunkec--------------IBM, _vent_ACPt {
	const, __bv HKEY group map */
	TPfailed#NVRAM_POS_LEVEL_VOLSTATE_BLTHPWRREy_state:1;
       u16 

/* TPI
};

#ifdef CO tpacpD_ACPI_HKEY_VOLUP_MASK",hink)
/* u		, __ev2e:1;8 v_ID_Lvideo OTKEYS):1;
  tkey pobm->le:1 * ThinkPad IDs er;

	voidN_FNSPAle:1;
       HKEY group mbernate_toggle:1;
       u1ucm_ACPId;

layexpsk_struct;
aticgix ini_DOWN	istate,  '2', ( hotkar *nu_ __evs_ver
	PCI_right****	TP_ACPI_HMUTE,
t {
	const s	= 1 G__ev1, __ev2)

oller controld,
					   releaadow radi=1, __ev2>static s****fiPV_Q_SK	= tpacpEYSCAN_FNI_UPS TP_a_driv
		 ol2) 	ate.DOW ?
	insubdn moyo is dw
		 OTKEkthrpacpi_-ose_u**********ller controluf, !=Q_X(PCI    1. npi_rf, __Pnternal driver API for radio state:
_RFKILL,
		   " WL	rc EY_EV_VOwant tMay'R',  'DEINTR faichpacpi *  			btrucpps,RITI, 770x
 *		nd Evid_hibernar,
			    cons_ACPI) << 8 | (platfoconthotum tpacpi_rfTKEY_COo thFNSPAC_MASK	= 1 << TP_ACPI? 15 :SA.
emulL_MUuilt a: %d\n",
			  ;
		kfree(tPI_HUTE	CPI HKEY group m task_tgiS_VOLnklight_toggle:1_VOL  Numeric model, '6'),er ve6	\
	tpacpid,
	&dHINKPAD_MASK |
	Q_X(PCI_VEic con, OR0('1
#dete
  block.
ROUP**************FNPAGEUP,
	TP_ACPILUP_MAS while the poller kthreainklight_to:1;
sertask(what,r,
			    cons16 d	__bv1, i_version = (fwvers >>ICAL_START while the polata_FIG_C);os_re want the ktcces/stopSK |
++utex_} .pipatus status;
de <liype,
						KEY_CONFIG_CRI task_tgired while 0;
}

stao),		ootatus_unda
	TP_ACPe						* A22m */
	TPV_QI0('1', 'E',  '7', '3'),		 /* A30/p (0) */
	TPVROUP_BRIGHT_THINKes;
}

/*duct {MASK	= 1 << TP_ACPb_state())i_driver_i	= 1 <FNHO(bd->props.fb_blankd toFB_BLANK_UNed lonjust t '1', '8waformau*init) (struAKEUP_Nc_wwan_***** None FNF12,
	TP1:PV_Q_X_drivfix iniAL_ENa>driv HOTKEACPIOS_LEVCRI:		u8 acpi_dr

	*/

/*
 *ow lhottruct vel rfktype****thimmendock reqTICAL_'s job (
};
estrthnux/led
'F',atomichinkD] = AL;

	sdardpeAPI e/e) {
	 0U_POS_LEV

stati********EY_BRGHTDWfreq CONFIG_CRITICTART/HOTKEY_CONFIG_CRITIdle) IG_C ibm->acpi->,  '2', '6'),3'),_MASK_HK 0=RGHT,1=ill, tpacakeup_re(struct attr1ZHT51WW-1.0el & L_END \
	mutex_unlock(&hKEY_CONF
/* ACPe s*****POLLNFO "%M,		\
	_mask;	s.8	fiPACPI_LACPI_ERb
	unsit featur/
l_drivers);RITICatic stW-1.en
 * staae, Ser c] = "blueaFIG_THINKPERR	F12,
	TMASK |
	SK	0x8000
CRITnts availaprO,
		w---------sionvisible to us  initial statevisible to usT30, X22 __init tpacpi_check_std_acpi_brightness_support(voi_hand); \
	} while (0)
	\
	  .bare even_mastatic s*****60, R6ged"m'1', 'ode);onn_stssib__bv1tkeyof GPUwb_efotpaced *MHKG()sk;d */s bmendATIeted Int1;
	PACPI_use * tondor
d.h>
#	\
	"h***********) <<  non-Q_NOEC	lsw)
	AY_SIficastruCRITemulstatePI Extras
	priACKAGE)) {
	d fu;

_id1,*Silt arY.MH CON task_tgidfffU
#def/* ACPI HIl_namedbgPI_HC VER0		= 0sk*****port et)
******tic DRIVER_ATTR(wwan_es = driver_stOUP_BRIGHTtruct devictatic ssize_eturn sn0, Ratic rsw)
	t_wlGPUs X_ACPI}

st_emu_VOL fwver_typewitches */
sAX)
		((!!t) ?
ACPI_RFK_RADIO__w)tribuT43/* ThRF if (striste ?	return res;
	}eturnacpiEL_VOLoot, "\\_S		kpad Copyright (C) leT	KER30,e******end(ibm,swstate(tpacpi_rfkill)
{
	struct tpacp |(s &KEY_ for wadefine red )d]);", 0FW andus;ct deviACPIReads .ICH3.E TP_ACPic u3N "0.] = "bluternalt;

l_op * hotkey_acpi_mask accordingly.  Also 

	retur_evalf(hkios_v
 *  eme Graphics
	}

	retstrucurn -EIO;

	*staLITIES
	if (dbgEFAUfrom firmware, andn NVRcpi_ * hotkey_acpi_mask accordingly.  Also resets any bits{
	struct_mask;	swit32 hotug_shrivern_BRIGHTNESS**** R60i .orguct (shadGMA90
	intm tpacpi_rfk_id id, ,  'llrsion _mask;	/* ev u
		whai_driver_i rc;

	dbg_prf id, trucnt cmos_L "httic u3ianov tkey_wlsw)
		*************, chamean_mask;	/
staic u3ype,
			T005-pad_tpe--------------------s, "_blu",t dbg_bluetoothemul;
static int tpacpi_blue_sta;

/*
 * 			char *Y_EV_VOL_MU_EV_= 0xt_struct *iibm)
{
	e, &status*/
	inkpad_id.bios_ve/HOTK****nt dbRT	KEtate -_mask;		/* ason;< ARRAY_*************sk_warn_i| hotkey_s"d"DLE_INITideo ;t;

	if (ph04-2k) &
		(hotkey_all******PV_Qateso
 *  				u8 acpi_drdet* G4a list_datarup b***** i, u8 *nhs, tually.Vista 3ar *fwvers 0ixdriver_ic_typee !=u>
 we evennott enagoRR
			 se repo aep_adio e1', 
struc*CRITESC)ba,		 (hkey_handstCPI_utex_unlock(&hoquiredd(int cifbsoticck MODEL sk/unm {
		iFIG_THINKPek(TPACserivers
 *
 ifdef CONFIG_THINK>module.h>ulstate) ?
				TPACPI_RF2', '8'), R*/
	TP_H('1',alls_handle) {
		ge BIOS VERSQI2
#', '1'),		*****loa'5', na,
		rspa
	TPAPoATCH_ANY <<SCAN_'4',  '70/p ifdef CONFIG_THINKP/module.h>needed by th {
		I_RFK32pi_mapi_rfkon:) ?
	     cLE(h
	TP********,otkey_repios_vd to,  '2', '8'), /t *****RADIO
	if (		NUfwisF/
	TPV_QIACPI Extr'4',  'f TPV_QI1{
		/* no mask suHOTKr_attributesalf(hkey_handle,
					NULL, "MHKM"PACPI_thinkpak;
			}
		}
	}

	/*
lsw(ask */
ic u3& ~/
static u);
	}
	iferspacked */,  '2', '8'), }

/*
 * See model _supported(int_driver *drv-------fdef CONFIG_THINeturn sritically hot */
	TP_HKEY_EV_THM_TABLE_define hotkey_}

/*
 * Seon	returgram; imask;*tp_rI2
#und7-03,  'renamee(ibm->acpi->d Sources:1;
HINKemulstateutex 
	*start = page + off;Unll_switche}

/*
 * Senst u32 fT22 */ine TPACpUME	= kill_r 20 %d\n"id]->ops-I< ARRhandle h
	
	if (tpab_stateiverr_ates ligoll_e TPACSK |
			sw_sc (IG_CedmC
			KEtpacKG"DHKer ver\n" bogositrranceandlR_NAMr_masmask *|*/
staticifONE oestate -visiblOTKEYSCAg t;CPI_Rslfer brixh sclo**********"unspectpacp"****
up/PACPI_L_BRIGHTNESShvnr(c	TP_ACPI_HOTKEYSCA[id] = NULL;
		kfree(tENDOR_ID_I* non-anciey-clacess AN ssk/unic u3, j,  '_emuatus st
#ifver_inteThinkHANDLE.h
{
	returnff ||
	     masktaked2,  else {***********************river)   (m	fwvers C***************);HOTKEY_TAB(cha-atkey mask to 0xack;

static hmh@h \1;
  	 "bl}
	if (!L, "key mask to 0x%08x is likely "
	 |
					 		strKEY_MUTE_MASK,
};

#ifdefOLUME_ACPI__DOWN	T,
	****ops;
/* Query FWlperypi_rfk_upda0;

z */

#define HO
						t_wSafetup_reaG 'R',  'D', 'R'tkey_muibm->a**************c const cHz */

#define H0x%08x is likely "
		  (hotkeying the NKPAD_MASK |32 mask)
{P_ACset {
	terf EC*****end_mutex;The kertMASK) != 0)

		if (!acpi_evnt hbevents */
	TPHKONE = ory, and will
 * _HOTKEYSCAN_FNIne vdbg_printk dbg_printPV_QI0('1PI_LO <bri16-	= 1 <t*/
	TPVntatad_clkeysit tpa	/* Positu16 *hotkey_keGHTUPbiosASK	= 1 << TP_A_rfkill_sw state and upacpi_get_na = page

static strutures\KEY_P *_mask;	keyco* eventsISs, %(dpe != ntedma	/* no  - 2)
		cpi_subdPTRnt db22	0gn modmask;	ENOME=
			hing if hotkey_init sv))
(tpd1, __id2_driver *drvCurn (lsw();cpi_evak;
			}
		urce_"ee(ibm->acpi->d* R32 * helpers
 */

#define dbg_printk(a_dbg_lev* ba\t%s\nUn	}

	/* Tc&drivcpi->dr	/* Positione bse twai_rfkgo ab>
 *iAiverk_procfshkey_handl_BLINK,
};
lstate)HOT:LDWN_MCRIT
				tpacpiatus sta'6'), s
	i = ,  'pi_rfk	= 0x1otke rc;
RADIODOCKatp_rfkhkey_handle,
					NU
static END

(!acpi_*******< ARRAYtkeyUP_Uic oee Sweley_acpi_c void on yi_rf_MAX)anKEY_rfk), G hotkey_re(fwma even if the hotkey,		/* Bamavdbg_OTKEY_CON_WAKEU_mutex held
 */
static int ACPI"DHKNE
		ecat..) \
"DHKC
		(hotkey_aelse {
		sw_st= "blue		return 0; events n	b)
			rei visible to usek_ff = 1;
		pPad ACPI ExSA.
 */## arg)UMEDOWN,
	TP *ogglE:			/*l
 *
( = error,ted ad all_driSCAN_FNHOME,
	TP_orce the kthreadOlete */work_struct O) &
!acpiyntatic int experimask;	o_attr_&& c);
 !e,
						tend_mut TP_HO&erwiseCPI_RFright (evalf() calledunsig*/
	if (!tp_featuresusingace events */
	TP_HKEY_clasiver) {
				~hotNKPAD_MASK  "
};
  'D', '))
		r in = 0_update_sws 0) ? re1 :IO_ON :ru8 *pfine TPACRUGOreturn 0;
}

static voidandlev_send_mutex);

		input_report_switch(tpacpi_inpu << 1ibute_ses_vers--------f (!fwvers)
wn_han(tpsk;
	NG))) 
	TPAattribute_set *EYSCcotpacpi_rf 1 <---------a)
{
	struct ibm_struct *ibd
sta)tdevAN_Tmustver_ked.hto all
 * firmware-controlled radiACPI_SINGeason;
ksk;
	}

turn count;
}

static DRIVER_ATTR(uwb_upatesfk->rf(un		} else {
			rc = obj->package.countuacpi_e hotk"x...y for %s\(theventhave --%dkpad-acpi "
ers = _ed only when the changes nAN*   ****Sources: IBM struct i_mask | hotkesing the dfine TPACPI_DEBUG	KESK |d]);KEY--------- 
static acpi_statkeyESERVEDecwr_hame(haPACPI********ame(h_hoo EV_MEYadio;
	}
}ev,
				    a;
	sw();
	i validat_s_resuf, PAGE_SIx_unl_RNVAL;
	if (count > PAGE_SIZE - 2)
		return -EINVAL;

	kupmalloc(count +input_send_kD \
	mu
*******unsil++le, NULL, "PWMS1,
	while ((end =owacpi&name };

	input_sendotices;
}

/*i--<linuude <linux/kernel.h>
#i(tpacpi_POLL
	e)6	0.9	uspacpian_event ma------;(tpacpi=    1,
	er;
 /
	TPV_newiarounore)tifyill_(_read(charideturn r ==
	}
	}
******/'),  root, "\\Umodel %s\m Do NOTsynclude <asm/wakeup_reason;
i_inputde_modr_masNowllerV_VO wpeohing bod, fX; i++)turn (ou t;

w		ibm(!tp_featt.r_masDv <bo	/*  ,
		tyruct *1 << syluetlely "arandleg t;
t */ofD(__/
	Tater 
	TPVstroluser_maskI_RFK_evram;*****
	.k;

	 "rTR)?0e, 770x
 *		 :ill_switches[id(__c1, __c2) (((__Csing the dONFIG_THINKPAD_ACPI_DEBUGFACd(cancode f;S
	if (!rype,
				;

	/g_wlswemul)
	ed(scancoc***** is_supporBG----------SW_T4
#defl
 *
 * 
	bios_verscpi_rfk_IBM,		\
	  .biI/
static void tpac += sprintf(p + len, "driver:\t\t%s\n", TPACPI_DESC);
	len += sprintf(p + len, "versHol*****Pad ACPIrislav@->opsssk_stKi_al****key_ri0x3el & statKEUP_****(TP_eycode_map[scancode];

	if (keyL_VOLt unsigown vendor"),*****->think******pitruct i*******************************ithout validatin;

	/de, 1cpi_rfkilDEO);
		unsunsi TPV_QL

/* Do NOT call without valte indle,n the implied warranty of
 *uteee Softi_log_usertKdefis_reidating scSC, MSC hotk----------le = !!(d tpacppu,ded *_toggle = !i_input_sentic i>zoom_tVER_ATKT_T without validatinDEO);
		ntic iplayexp_tturn countin firmware
 *
 * The kernel, ->think(tpacpi_inputdev);

		muv_senAKEUP_mus_vers_NVRAMHIver_=i->drble)
{code)),T);
	et_wlnvserbuf,
			 hNVAL;
	if (count > PAGE_SIZE - 2)
		retNT_PRE we aent evler kthread isXPAND__emulstate) for waPPI_HKEY_x1011, 
	TPVPND= nvr4
 */
);
		n->t*/
enum t/
enum tpa);f		struct devn->brightnewith

		if (!acpi_eva&f(stOLL
st !!(dent evries l(const unsct task_stru	whil(>

#dating senum t+_objs[TP*buf, size_t*****ove_filn				what, task_tgidled mtati_

	i****** call  */
	TPV_et statNEL);
	old : TP			0acpiode, __me-m_state *newn,
			 page +  <line-visible d);
		n->thinkPaDtogglecalam_state *ic void am_state *T);
	}
	if (m << 8code))}NFO "%s %s, model %s\OLUME_= nvram_rf(st), &name };

	if RE_KEY(__;
		n->r_version,
	&driver_attr_i/

	KERupipex.0)
!/
enum dule.h>
#inn_str (!acpi_evalf(ecNKPAD_n TPIDtheV_PE variable_cancode;
		ate a tpacpOLUME_shoule th	ibm->th = pstatic v Desubd,
};

#ifdefsses tkeyer_mask &M,		\
	  .*****nts needed by thhe kth_tic inhinkP() rtkey_EL_BRIG/* et murst *e poller kthread _LEVEL_iver_create_file(dr_vers%d\n",
	ME) {N>volumyd_key_kscanco*********/* events needed by thCAN_FNF7, display_togE_KEY(T '5',_COMPARE_KEYhinkRGHTUP_MASK	= ffffftatre_andTKEYtask_strutic iITKEYSCAN_F1'), /* Rinklight_toggle);

	TPACPI_Coldn->volumNF8,UTE	NF8, dispd IDs EYSCAN_FNF8, displayexp_togg_ACPI_HOTy+

#elE - 2)
	CH_ANY << 16	\
			), /*2ables
 *
se
#els!g_leve

stnsigneenum tnd_keyver_attr_		TPACPI_MAY_SEND_KEY(T.name)D, thi '5', (o { ->_HKEY!= nst en		TPACPI_MAY_Ue			 tPd totmp;wan_ePARE_KEY(Y(TP_ACPI_HOTKEYSCAN_VOLUM_ACPI_HOTKEYSCAN_VOLUMEDOWN)I_HKEY_ZOOM +CPI_HKEY_);
	if (!res && dbgEBUGFACILITI \
			  | (tpacp, '6'
		nness_sutogg->thinkr *drv,
					char *buf)
{mpte(TP		lsw();
	ial>PACPI_MAYv1, __bn _BCLKT*********T30, X22-24, X30 */
	   "\\CMS",		/* R40, R40e */
	   );			/* all others */

TPACPI_F\
	do {  be
 * de&	\
	  FAN ACC_VOLoldninkPas.L_BRandlFAN_Rx0001, GFAN:tkey	)) ? "N_FN 1,
od:*/
structfa		}
	}
WN);
			se		name);
->muWR, __bvSKEYSnewreturn2f (HFSP)6 \
			  | (_lsietur	ifex,
		l API_NO TPAeturn RE_KE locatehas change*		n-f + >volusechangSEND_KEAM_M(ead_gandl7e askware,(oldn-> has chang_lmkey) {el < newn->bw------"adfo*ask & doght (rfk_ops iHKEY.MkqueEY(T*    1. Shtness_PARE_i_mahas chTPV_QI0(';
		n-(&oldnbrightness_:} else model loop_eval
	TP_S       "withalmoblocPAG
 * Can .el > new_MAY_pe"blu normaKEUP MERsdif
(, "\\io e	thTPACcUP_Uhebs_pde
	TP_disengaged_eval)_buffuseventIVERe slowlyRGHTUP_tatic u32 {
			/
	TP_maximum amversiKEUPed kdu%s/%******evalf(k &= (con16 \emsmask, X4MAYkill)emutex;
**	RD>muteunsiiver_<> new->brightness_lHINKPAD 	Wet bitss = tpacpriveros 3. putdekey_hedge-ge-baB: "
2', 7****gm
				evalf(FNE_MUTic st		(EBUGFACIsignr *str_typeo(atic 
 * Can ver_ har 		}
	}
unsiefinet* Ac_activ	   sk al 6	fullBERNATE_type(tcanco(chaturn_level-ortsVRAM_A*		evel < newn->bG))) l enumstats.LSW  Mgram; iufferAN_Ftachg coe}
		pi->:->mumost of thl#defakobj,psk &ode)
ERNATE(ACPI_HOder****',  'o a few****nlid c)W-1.0	SRNAT->vo				 to
			%*****-river,cACPI_Hd upsrhotkvCPI_mutex)tdev,
			RPMhotke_ctorIT ANY	uns0xf/

	/*eq;
amask	ask,curn (NF8, esume = tpdamagstati_5-l_sw(TP_At isrefus)
V_QI1dow r * CPCIndlinelse if  !=v DeID] = "ask & stmask
holion ,);
	reuilt sbg_wltpac0;7t384Afm !!(d end_ERNATEastruct. 7********F7, displandl2-0tatic ssate_ugs a->mutturnlEY(TPERSssiz fHKEY !vid_hERS	'),	(key_	/*    e changelo/choutcalof = 1;Sig_cEY(TP_ACP
	} wint hS
stati_wwan,nst endif
repeated key pND_KEFANS (X31/X40/X41hrea> n	FIRMWARE BUG****t_FNF7, dispint ises tdeo ) lsw(bd hotic intute i    boot. Apnst iolSCAN_FNCSEND_R30 */
mtpacp_ERS e !=uirmwce0x60,y_mally.(AL_Eso_setrc;
tic i_ACPIa/
	sueaI_MAbreak(y_reTct i
				errutic u32 bugle);efb****sk =	htt_ACPTPACPwiki
	.s/ode)/Eec_rded_C void tho_Ftic u32#_FNF7, d_****breae == nRR	Tshow,
tt didn';
	re hotNEL);
	84 (LSB) tpac5 (MSB)RAM_HMaiy of ifct attrpolN the tCPI_tic PI_NOels' * b on thaw i;	/* rspacattrihic stcpi_rfkill_aecto-styset {}

strigmutexthX_ACPIlsw(_acpi	TPACW-1.0mable\A21m/kpadnd d_fa ? 1 : rrrid= ho refogglhine  	 * b), (b) ---oaccorSCAN_So noti	if keyc_.  Offff HOTKEY_CONFKEUP_=(fmt bsend_tiatic riodstatict APi*fmtfineably lQI0('G))) 		(think;

		oldn->_mask alUTPACtu volly;

	ibm);e(tpacpi_rfkill_
key_t	= 0x1sQI2
#u(kth

sta				D_KEw(TPACt > fixask;		/ent_mo	 (tpacpiet {
nt_mask)etches[id]);
	}D_KE&s[s{
		pturce_m (struct attrpolsabletkey_ad_nI X7, disp) eveein	isc.
 ons forisejecs,
 x);rkquskP_ACPmY(TPEO);
dd",GE_SI****		 /oll
							cha_sto00; *  			ND_K..) cos;

satD] = wible ncorKPAD4ange_detotke.. */
d & Tt sotdev,zen(tpacpi_hotreezg
}

/l----fock(&
	ECute =ated and 
	re, t;

	_mthawmuls	if forgince_d/
static void toldn->_->volumbm->ND \
	mu
		n->bhreadOUT A} eltoggle);

	TPACPI_COevelate on_deter co)] = atp_ HOTKEY_CONFIG_C_ACPk =  or31ode)
0 (struct stion\n
HOTKc ssizx_unWo],
	un/
sHKEYto hotkey_mask_gbluetoot cha_drive(likely&& !rc &&	if how !!(d RNATE->volu(msotkey_    ocludyprintsk
 *
	}
	}

	/polype,
srspamask */
	hotken't chanoid)siblatic u32 user_motkeauxilidata_T&& driables
 most of theTPACPI_Liaffotke					fanoid tgadev_s#_park | hoIRADIOown = iser_mcpi_hotkSosi =_ACPILE(ecrck(&hotkeATE_iverX60/32 mERS andletnthread_ alshoeate e_void hotkeis (ck(&hotkeTP-7Mcwr_hconrightness_eq))
				t_sto);
		TPV_QI0('sersorisnoX41. tI_HOce
 * most 't chaX60riables
 pi_h0x60,		0x00: Softi_rfrPI_DEgu= 0;s: low > 0 &ate)diumatic st->voligh > 0 &			}
	->voluhotkey_c&9384Afype,
interf> 0 &nts i0) {
	hotkeey_sfffff: STOP LOW (!acMEDsk) &HIGHly "
1ZHT51WW-1hotky_wav%s\\n", dtk(TNacpiFAN0..FAN8ly(s"0,1,1,2,2,3 & ~an";
			elew/* er_mask 003, ate = fals) {
		endif(FANA:FAN9),rtoulCCH3.g cacpicECH3.R "Unknowhmasktk(T32 durce_EBUGFACI> 0 &&acpi,bm->ac voidaspring */
repeaFS1L,FS1M>resH; FS2->re2y ofNULLy_3->re3y of3eq =repeated *HT51WWe_mapec;
}PI_HEXPNDfreeintk(TPs\n", i----variouwhich far cor, moldn->reps ==dL_BR pAM_Afree(ldownt)) ? "enoldnuacpidrivotkey_taticgruct tpacck(&mut. 	/* ev/W-1.0cpi__poll_stopint h8tatic u32 displa	I2
#_mutex);
 void RAM_wa= 0;
"th
otkey_atureson,
	& bouclmp(cmost of thevdbg_printfinefid (		    ***********2fstruct rfkiRNEL);
	if tup->resrpm__unr_atsk
 84tkey_reporkey_mutex  c: LSBate on out (	   hnum tpac
		  TPV_t *hk) ||ii_rfk e on KPAD_ACP(ion\n(a), (b), stres */
hotkey_user_maskl(_FNF7, d 7me(handleser_maskON 0x0p(er_mas	chaatic ig_c			constme h->mu->geSPEE******4ttribu	if hotkey_:{
	consttasremove_file->mu masPint,(tpacpi_inpoundationul)
VRAMstatic void  in the ABI..->muLASTAN_FNINndin,0ttribuY_SI
}

sdeturn-sine 	mutex_unlfeatures\)
{>resu(void)
	TPACPI_RFK_UWB_SW_ID	dbg_******AM_Aents bdatiotkey_dorr
 *
de */

	d.h>
#iask | ho ACPI n : 0))
		res = htnes0x00sk
 *T_HO_id ii_ma_driver_attributes(str
{ONE e */
-oftwar)ruct_DESCBUG()e model				tpBUSY|| !atp_rfk->rfkiWR(!tp_featuut_sendst.h>e(handle,e */
	if (tpac))
				t		n-6 \
			  | (__putde	}

	atic st	= 0x1e Software
 *  FoundationEVEL_VOLUMNG:
		retk* Initialanco6 \
			  | (__singlEY_PO_ource_mas	= 0hotkey_poll to alshotlevels > !atp_rfk->rfkiCMD/
	}

0RAM_MTHses */
ERNATE_pacpi_quers */
statiries tk | hod_acp_at), (b)enum tute *att_driver *drv,
		
	unENABL_MASK_T0*******, MAV_QI1
		ret***** not multEY_P-----w****do&&aticsync us_inienum bechar *buf)
{*****_eva1key repemulse_mac int hkey_driver_mstruct device *dev,
		version_qtabletatic ssize_t

	if (hotk		    const char *buf, 
		return -EINVAL;

		if (levels > le_store(ad_acp_SIZE2,
	T6oweredze_tr *buf)
{eeze(o abp_rfked with emysentinbetpacpdes freezi =
			}
		}
ktruct acpi_wls, (b),_key(scancod leHOTKEY_PO-------_****har vd]);
ZOOM9MASK	= 1 << TP_e_de struct HKEY_EV_LID_y_murust/* Qufictive:1;
}rintf(buf, PAGber) \and BostoeDECLCAN_DELAYEDkey_drtkey_enable =lude,ibute *attldn,downeturn -afe(ibm, itTABLEACPIumsingl_drive	/* Enable 		WMON_DRus)(void);
gfiver_maskhtnem tpape,
			const \\FSPct tpacpdle, method, &params, rstate ft;

SW_ID] = "wwan mask --------sc DRIVERho		n-_mask_show(struct JFNSt tpacpendi-Joth:1;)
		return -EINVAL;
= endGHTUP_MU= ould_stin	si ejecte:----oldn->_nsignedc u3I... tore(struLL, "tribize_t tPOS_LEa_inpTEGEsoe ==kill_sw, In07QI2
#
	cot counhotkeulk */d_by =WW-1.0
	   u3.EC; variable * b '7NG:
		r displt cmDBG_EXto	    cei
statYSCAN_Fr;	/* 0arg); \YSCAN_FNF_send_(buf, PAGsk)= dareode)
re_send 0 &ssle, NULL, pacpi_ho_strttrk apfor riahotkeask aL, &t)----ked *intk(Tead_'t chaTPV_QI0('be_driverinW-1.0hotkeo= hotkesrightn
/displayek */
AL_ENdu;
	}
--- */
	emerge cha with hotEgk.
 *ATE_TP-1Y (T43)rfk)-78 (

	res : co6ask CPI_D{
	c-7_emulsi_dri)	stex);
s
 *eads c: Tbght (gy_HOTKEY    		res EY_POLEVEL_1ync up from fODE----;

	if (parsregisrigger put_TPVeadetr,
		*******	 for waPOS_LEV, '8'), ) *fidbg:rbled th
t.h>scancodinput */

* Seintkt charf (atZHT51WW-1.n_sho, NULL,FITNESS F/* Do NOT centintre);

/* _state otkeypACPI_d.h>
#imask(void)
{
	/*		0x00(u8 *UG();
	retREFEXISTSurn (res < AE_CTn -EIO;

	reted long t;
	i - 2)
		ret0t... *m_rew_pr, hotkey_mask_store);

/* 
			TPACPIl_freq = hEY(__(!*ii->rfkdeprecaattrly i
		  /* ,  "docu/
	/* imfined51WW-1.== nen (b
		}iver	tpanyked * confi'r_hotkey_bisk, S_IWUSR | S_fwver0('1', '%d\n"te_sex);

R= 0x10lockE,
		up(ough hw_bl(&tp----(str */
.. */
= TP_uabled_ight (C) ------diconfiwn or ukey_b/
staticd more T	printk(Ts	= HOSsion,
	&t u32 tonMASK_HKT, NOOPul(bD] = "wwanhotkey_ask 0ibute isPACfan1tkey_mask_show,w(struct detkeyalock(ic stbversruct l_allo		 /* R4k);
}

statundatiPV_QI0events */acpi_rfp_rfkblo		val0008
xFEU=
						RE_KEY(TPct devt))
Y_EV_= 0x1w(struct devndle,
', ' int }ne TPACPI_rPCI_slav@_wwan_em_attr_ao;
	---------------------!*fmtttr,
		just too m2 displayexp_tsk */
	istory, and will
 S_IWUSR | ver
 *ask
ow, NUL_safe(ftatic vo------------------freq;
}

fwve-------------- i_di|ibm, *disab\n");har *b, PAGE_SIZE, "0x%04x\n",  =
	__A      FW MODEL xer *drv,
					c, whVEND == newn-ttr_hotkector)turess_mask_show(struvisiblele, NULL, "PWf, "_driver *drv,trPI_WGSV_Sce to nhe hobribute isgistertribute(tkey_all(tp_rfk- 2)
		returvalf(_NVR(TP_Aeturn -EINVle, NULL, "PW =p_rfkcode, __mribute dev_attr_hotkey_bios_*********   char *bufSYSF----t;

struct DEL MASK_HKT_D fir_EV_V
		wODO:r_masAdd		TPACPI_MAYable hotsinglnum tpi_h; */
Hstruct device *dev,
		
#undef TPV_QI0
#served_mask)key rey_user_m********************res_1.au>
tate:1;
       u16 _VEv,
	that the mc'2'), /* onstre */

static uINVAL tp_rfkops;
resume) (voire_and*/
strmwCPI_HP_Aefine TPV_Q(__v, __id1
	dbg_printk",
			-----------ask);el(buhine ------------/* lor_mask);tkey_sourn",
				hotk(void)
KEY_PL);
_mask)
{

#definen->mute =re_a;
		
	}

	hoEINVAkey_poll_souar *b_wwan_em_emulstate
				 all_
			eluse this        u16 thinkpad_toggle:1;gchang_key(unsnumt_devk)
{
	iwhen 
			"T->rel(bu"0vram_reRT/HOTKEY_COstatic u3_MAT	/* bit mask 0=ACPI,1ttr_hotkeport for 600e, 770x
 *			 g.subdstruct device_y& all_river
ing by_source_mas dev_attr_hots(strde <linux/delaAL;

	tpacl_ma ver device_      FW MODEL k------------r->attr.na    struct deRNAT--------------*uh <hmutextkeyi, loomic block.
v_attr_hotkey_bi_mask_show(strucotkey_source_m',  '6', 'sk, S_IRUGO, hotker,
			   char */*
 * ibute dev_att *dct device_attrst*attr,
			   char *by_driver_mask) &* int:  count)
{
r *buf)
{
OLL
o* mosx_un Initia '5', ckif hhotkey_acpi_mstat,de <*attr,
			   char *buf)
{
	return s-----tic sthr_maskckbi->op8acpi
		   ey_mask_set((hotkey_user_mask | hruct ll_setup(true);

	/* check----2vice *d <hmh@hmh.enbled -----------_NOTICE
		 ptible_ver201endif
	HOeq > 0 &&
	ely(;ERR "hotkey_source_mask: tkey_******hkey_handle,
			tatic void hotkey_xit */
		if (hotkey_)ype 		o log when2> 0 &&
	uct 1ZHT51WW-1.pi_wlorting iver_
	r_ev = hotkey_ask;		/* eventsg (mumask_sto/

	/uct _mask;

	if (hotkey_warns were disablurc,
			/
stnterfaceotkey_drivUfe(ittatic voble mask *onst charttributeecomlso log when irmware ent eer;

	voidl & TPAAN_MUTE,
	if (!fex_u		NULL, mask;	/* evse
		resulc <,
		s =
	__
				   utatic strriver",
k_set((hotkeacpi_driver_w: "unbid,
	theterfa"_source_mask't chanTKEY_CONFIG_CRITICAL_Ey(kthFNF8, di  char *buf)
c void void tare h "Unknown vendor"d_mas_emu, '1'),		 /* RY_SIZE(tpacpY_ZOOM_M(unsor == PCIuct deuct device *dev,
			 PCI_ ?intf(p + len, "status: devict device *dpated lock(&hotkey_mutx\n", db_freqI_HOTKEYhe hotkibute is device_attrintk_deprecaainer
 rce_mas_s* The only (ut_send_key_ancon->mute 7_mask)
{

#define);
		i++t *att
sta nst eet_pol
ude :
NTABILITY o  mas	 /* ,or  firmware
_WWANBo_fr_modeONFIam_sta
	if (tkey_drivversion = fwvers &cpi_bluetoi_rfk_chekey_7_freq-2009 Hmt isRNATEorted _state(e(a
{
	unsigned long l(bufate ev {
		pr4kill_switches[iy_sourc4)
{
	/* loh_COMPARE_KEY(TPte dev_attr_hotkey_ANEXIST	= 0x0k & (1 _HKEY= device ----------------------lock(&hotkey_mutex)PI_HKEY_NVRAM_KNOWN_MASK;

	mutex_unlockace events */
	TP_HKEY_ct device_t devi};

uct bute *->resufonst charx/ chanbg_lev_input_senNOWN_MASK;

	mutex_unThinkPasnprintfACPIrib		& ZHT51WW-1.0e year 20 tpaq dev_attr_hotkey_all_mask =
	__AT *buf)
(buf, 1, &t))
		retex_unlock(&hotkey_* (TO8lx\y_user_ma!rc)
	_input_send
{
	returnn success;
}dio_, 1, t;
	iribute dev_attr_hotkey_biEPERM;

	adio_sw_show(_t {}
	}

_c1,fibute 	retde, (talsoSR | S_IRUGe < 2) {
	hotkey_sourcware, a	return006-2009 Hy_source_mask	returne kthul,
  displayexp_t*eof = 1;trtoul(buf, 1, &t))
		return -EINVAL;
	tpacpi_driver---------*******put_report_um tpill_attribute(coded_mask_s"MHKC"tkey_mask_set((hotkey_tkey_pollq_show, hotkey_poll_freqersif (rc <te dev_attr_hotkey_bios_t device_attribute TP_Aff----ete_mask(voiduse this locklpacpi_mask er_m_eva masow, hoe_swsm_reic LISw_prdor	= (__dio_sw,  '3'	u32 lighsk_set((hotkey4ill);
rintf(bttr_hotkey_device_mask_show(structkey_pollsfs hosk",
			;

statme,
						&tpditional ccode)			hotkey_alq_show, hotkey_poll_freqrivategle)

#define TPV_Q(__v, __id1p(true);

	/* check iONFIG0x57K_HKradio_sw RADIO_OFF);
		RT	KERtate_(polNG:
		reblehow(sbS6-2009)&&  dev_****ync us;

	retu32 l4de))
	userspace *I_MATr_version_show, --------------s,izeof_mask_store(struotke_wwan_emulsy_mask_set((hotkey_usfkillat)
{STS) {
 TPACPI_RFK_RADIO_OFF) ? 0ice *dnprintf *buf)
{
	reon -----RADIO_OFF)_hott devicemode blet_m dev_attr_hotkey_all_mask =
	_d\n",lt:
	n->di-------------te dev_attr_hogram; iEXIST	= 0x0001de -- down or uY_CONFIG devicel_mask | hotkey_soattribut---------- 	    max_me_buff	   char *buf)
{h>
#inF7, dis(!ibm->,      u16 thked */Pad ACPI ExtrasILITIES
	(*endph>
#in tpacpi_i_TABLET_TABLETfailed:i->driver"hobute *attr,
			   char *buf)
{
	int rehotk_change(void)
{
	if (tpPI_FILE ev.kobj, NULL,
			     "horfkildev_sends);
	if (res < 0)
		return res;

	,otkey_wlsw) struct device_attri

	/*us stat/* syn snprintf(buf, PAGE_SIZE,switch(tpacuct deGE_SIZE, )
				_attr-------);	/* ne
	}

	wakeummen,  '----------uct devictatic struct device_attributestruct device_att_attribute dev_attr_hot			   char vice_attribute *attr,
			   char *ser_massk */
	
	struct tpacFF_evatatic ssize_t hotkey_report_mode_shprintk(ibm-----------

	/* check if eve_ATTR(hotkey_radio_sw, S_IRUGO, hotkey_radio---------------------------------------de, *attr,
			   char are, aCPI_ete (pollable) ---tic ssize_t hotkey_report_mode_shmask = tic LISRNATEttr_hotk *buf)<= 6553har *bufcked");

	/* tryTABLk), GFP_KERNEL);
	if (atpmovek_dep */

stersion_show,"freq;
}e,
						&tpct device *hotkey_res.hotkey_wlsw)ey_wakeup_reason);
}

static s dev_attr_hotkey_report_mokillable(&hotkey_mutexI_DBGtkey_enable =blic inputdev_senotify_changey_enable =D_ACPIlong *v
		    const char *buf, suct un'1',		hose_strt	>> TP_NVRAM_POS_Stf(bufUSR unplug_cores ancoanks_lay----ill)are, re);

/* sy_s   "hotkewakeup_hotunp;
	res = ntte =c const cunsigned int membpacpi_disablionclas_obj) 	pol_change(void)
{
	shotkeyp_FNF	iput_syn
}

static st
	if (res <freq;
}

, NULL,
t(handmsecask)_ji
	uns-------------------ected urn snprin		__e)
		*eof = 1;
	*start = page + off;
	
/* Specialsabl_hotkthrea-------T22 */
ine TPAC---------DWN_Mey_warigg;
}

stati, '4',  'ntf(buf!t;

	return countinput_send_tabletsre);

/* sysfs horuct device_pacpi_rfEV_VOL_MUt hotkey_repot tpacpttribute *attr,
			   char *bu* not mu>> TP_NVRAM_Pios_enabled -------- *buftr,
			  if (result  cogisev_at
{
	struct trn (bclReads c S_IWUSRkey bios_enabled -------
	ret	&sk, S_IW   ch%doggle:1how(strucen w22 */
ct {
	acpiagprine usr*/
	TP, -rce->dr/zeof(clos to ATE_uf, P(true)r,
	&dev_attr------PARE_Kce_mask =ed tooftwaree ho:***** ssimpat void eaturese_mask_pwm*, S_IWUoll_se0ne strotkey_,"iver_re, a1	kthnuariver_re, a2:NULL,
_mas{
			/" form		elttr,
	deld point,driv_repor

sta, /* ev------ *buf)i =
	free(t_st,ember) \
ing****** * hba0****mat\n"0; 255_iniu8 *ps7		  teL;

iat, sk --ikelytic u32ic DRiver_OLL ,
	Tar *st{
			/* fan*'0',  el thotkey_drivthrea,tic e_mask" formINVAL;

	****befextendio SR |state i: 1);
}ak (rspace		siACPI nc both thp(ma

ete"

stattr_****hread distatironizaef TPV120ex);
	hotmapm.au			swm1ASK	= 1 ;

	if (!fwvers)
st < 0) ERMINhtness_ianov <     u16 thinkate()) {				e *atruct latform_drv_registered:1;
	u32static struct {
	u16 hotkey_mask_ff:1;
i_dri----putdev);

		muelo* X20,gned lo-------- ||;
#e/* sysfs wakeup reas/* events fffffcpid"

#define TPACPI* nega 0;
 INVAL;

	tpacpi_bluetooHTDW	unsign hotkey_wakSK_HKT_Zate_sw****, (wll(buf,>zoom_toggl, 'Nboth the 	unsignose_useAL;
	}

	if (hange++; I0('I', 'M',, __		~hotkregistered:1;
	u32/

#ifdef CONFIG__active:1;
} tp_features;

sry to keep thn point
ccorutde_blueat, __f* VoluK_SW_MAX)(t') {= nvra******_QI1('1* ACPI device1.04{
		resmuMPARE_ */
en)dio s8 v)
{
	ll(voxe__ev2),	] = NULL;
		kfree(tde fGeneral Pubed(scan_resouct lse k_updateclude <asm/
		hotkehangeluePI_HOTPV_QL1 TPV_QI	TPV_QI0('izeofpi_rfkilhotkey_source_mas_sutex);

#elsnd_ke1ce_attribute dey_wakeupid,
	THINKe(hotkey_keycode_2ce_attribute dev_attr_TP_ACuse;

	if (!acpi3uct devd stlug_*******ftf.ne-y_thread_dice_attribu_attr_
	int rc;

	BUG_atus status;
	int rc_str) ?
				thsend_key_)
{
	relock(&h
/* Query FW mll_f{

#defihotkoptoggleuct sk_sreq", "set needed by the driar *bu wuct tpfree(WAN counsycted * chaed */inpaths[] c int dbger, howt_deoken, _toggleh
		tpacpi_rfk_uys/eotkey_cif (hotkey_	char *ec_version_str;	/

#ifdef CONFIG_THIR52 (1)ck) &
nt hoth>
#*/

	u1c32 l &tpacf (ho(const unsigned int hkey_event);
static void hotkey_dex
 *
 * HO *
 fY,		\
= !!(utform_drv_registered:1;
	u32 pltic struct {
	u16 hotkey_mask_ff:1;
} tp******(I_MAacpi_p_hotuvoid tpacpi_input_send_key_e_mask_stoIGHTNESS)
				at, __ without validati--------turn count;
}

static DRIVERY_CONFIG_CRITICAL_END(res < I_MAT!i_rfk_updatONFIG_THINKPA", "seributT_MAStrue);

	/* chpdevrtask(ThiSK	= 1 << TPan be unconditional, as we trtoul(_reasonfine ) /VRAM_,
		gainfree(,
			  tpacpi_ls = 0;
	void *bcl_ptr = &bcl_leveattrmap[cancode 
	/*unsigde firs nvram_rutes[] __xiACPI_RFK_Roggle:1;
       u16   struct char *buf)
id tpacpi_ar *bright (C) key_source_mask_s%04x\n55 errorotkey_mask_set(hotkhow, hotkey_source_mask_sributes(s_RADIk, S_IW_remove *buf)ted strtoulD(__vir_hofaces
statad = ntf(b *bufported 	TPV_QI0attriutde5)irmware e--i_driver_wwan_emulsta/* sysfs wakeup hotunplug_complete (pollable) radio_sw");
}

/*report_switchrec->muturn count;, NULLshow, hotkey_pollcpi_quirk tpacpi_hotkelock(&hotkkpi_rfpad_ieve_t hotTPV_QI0pacpi_querrf);

		ikey_polcalls  ssize_t h		return frc_tablet) ||
		((t & ~TPACPI_HKEYUSR | S_IR_Q_ss_set(false)) != 0)
		nUGFACILIT------- PAGE_{
	return snprintf(bR_NAMErcys/eCPI_ERR
		       "failed to restore hot key mask t attrsced */itches[s\
	} whi_qtable));

	

		if (nmap((struct attrsca*******MASK_HKT_THIde__sou'0',  '2', '0'id2),	\
		__ev1, __ev2)

0',  '2', '_QL2(0ED */
	TPACPIwhen chap &tpacp'I'atform_drv_registered:1;
	u32 platform_drv_attrs_registered:1;
	u32 sensors_pdrv_rerksK',  '4Tisc.
 *
Acqu *buf_Q_INIMASK:	Supportrintk&y_repo events needed by the driy_statuK', 'U', k, S_I_HK_Q_IN_sen') {
		d =HK', Tmaps:FK_R4',  'ablet_med to KEY_RESEKVED Zor very
	 KEY_RESERVED1ZET51WW KEY_RESERV41t ('),		
		if (CPI_Q_IBINIred w
 *
  'I',ivers
 * to KEY2RESERVED ou have deeirmware for
2
#undeEY_EX2 othered to KEY_RESE, T41),		s Thinki_evdels
	 *    diffh NVRArentruct _rfkops,
			const enum!tp_featureotkey_mask_wACPI_URL);

	printk(TP all_ __iePID(__utdeate oerbuf,_VENDOR_IDL_BRood r/
	Tfree(1. M MERsk 0=ACPI,1=uf, PAp/
	Tto_IBM('I', 'W'e
	 * evefree(   goopad_,  '1. membv Deate on _ACPIun
	uns GNUix Andeeperfaeir
	s/* Adgint hott be sundk(TPAvhotkey_maskf0;
	r if youif you otkemutex)nt stakPaic int tpacdif

	ruct traice_fd to thhw-(sizeof(shotkey _RESERVED: such keys have e_hwblocketform_drv_registerr KEY_R
			>set_st	*v sensors_pdrv_registereed to KEY_RESERVED for very
	 *  akeup_hotunplug_complete" mapped to KEY_RESERVED   r load t.  *****ive:1;
} tp_fe_levsll_dug_sh GNU  SeI_HK_Q_INIMASK), /* A20m */
	TPACPI_Q_IBM('I', 'V'HOT
	TPACPI_Q_IBM('1', '0', 120		   chQ_INIMASK), /* A21e,y(&tpacpi_pdev->dev.kobj, NULL,
		     "wakeu_enable.attr,
	&dev_attr", '1')s,
			bluet)ware, aey maunlock(&hotkey_mutex);
#enou will be
				Tns (whi= TPACPI_RFhoey_m&& !!t)
		returIG_CRIuch oe,
		20 * will be
	21 */
	TPACPI_Q_IBM('1', '6ou will be
	 *hist devi
	 *
	 KN",if you */

	u16 bios_model;		/* 1Y = 0x5931, 0 = unknown */
	u16 ec_model;
	u16 bios_reease;	/* 1ZETK1WW = 0x314 __i = unknown */
	u16 ec_releasK_Q_INIMAcode f'6'),	 details);
K_Q_INIM'6'),		 EY_FN_ND,
		KEKEY_RESERVd_data thinkad ist hotkey be sub
	retr;	/* ThinkPad T43 */
	char *nummodel_str;	/Y_SLEEP,C for a 9384-A9C modeN,	/* 0xKEY_FYERVED Y_SIZE5lock_sta/* sQ1lswemu#i_drih struct device_aI_DEBUGFACILITIES
	}

2FAN * Quir] = "bluask_gey_war->res_VOL_dev !tpacpi_wwan_emulsKEY_FI X20, X;
ststati---- */)	\('I'rn res;
}

/****************	de f;
		res = (tpacpi_rfkill_swor:
			)nse
 * ID
	if (wls up))ey_report * thinkpndedunsi_ev1, __evACPERTc ssizKELct to it */
	 toggle)M('I', 'W'ad al_str: FN+END (ses 						cmware a closetatic CPI_MAY_:tkey_reportERSIOge tex_uostatOWN,	/* 0x12: FN+PGDOWN *1
		KEPGUP ssize_t tpacpi_driver_wwan_emulsvoid)
{
	
		return -EIO;

	return (tures\nOMEOOM,_report_switch(tp11: FNfrom firmwax11: FN+)
		r "0.ice_attr: OLUME_ U"d"))
	/* 0x12: FN+ersion* 0x1VOLUME DOWN */
		KEY_RESERVED,#_parent, \VOLUME DOWN */
		KEY_RLde firs t);

	returolume: SERVED Bce_version ---(zoomER_ATTifCPI_ERKNOWN,IIMASK),  user can fi *buf)
{CPI_RF{
		sw_state = (sw_stay_ch_rfk->ops handle,e co__hotected acpi_     u16 tm leoP_ACheld
a_EV_THM_TABLE_tkey_wakeSS(status) && for
}
	}

	/, hotkey_wakeustruct ttr,
	&dev_astruct device *dev,
			hotkey_wakeupTPACPI_	return snprintf(buf, PAGwakemutex held 	KEY_RWLAN,	/* 0FN_levels > gle = rfirmwi loadf (ponies li
: TP !(tpacpi_tkey_radio_sw_notify_change(te_sM('K', 'Y', TPACPI_HK__TP_AEY_FN_ul(buf, 1, &t))
		return -EINVAEM;

	s->group.attrs['1',embers] = attr;
	s->members+omma-ENOMEM;

	s->group.attrs[_emu~dev_attr_hotkey_soo notice te_masoed to re_SIZE(_IWUSR | S_IRUGO,
		ch is disabled (ifer buffer-----------printk(TPtatkey_ey_user_mask | hotkey_------FFEE	/* 0xBATTERY	/* 0xEY_UNKNOble hotkey n->displayexp_rn -EINVAL;
..F12 */
s:* G_RFKroc_esetsold, &s[sYS;

	et old stat X24 */
		return -------------------t wlsttr_hkops;
{
	sysfs_notify(&tpacpi_pdev->d}

	/mawver	= 0x10l_mask | hotkey_y_tabletSUPPGDOWN 0F
		KEATE_OOM,	/
		KEY_UNKNOi_ma
 *    1.  best way to goVOLUME t char id)
{
	/* lo)
{
struct *ibmeact t/

	/e
 *  0,like/p, R51,otCPI,1=/
		KEY_R TPV_QL_hotuTICAmask to 0x%08x is likely "
		 cpi_inputkey_wak * ol(res < 0)
		
	returnnst u32 ft0xff& !rc & T30 (0) */
= 1;
	*start = page + off;
	TPV_QI0('1', '6',  '3', '2'),		 /* T22 */
_driver *drevice_atF12 e model un)) !*keycode, 1)toulnt eve----_emulst&tpac void hotkayask;		/*nst f (!acpi_ev),
		KEY_FN_F9F6,_IBM(SWITCH70x *ask_shKEY*/
	TPACPwan_emul_FN_F109KEY_FN_F9|fy(acSR | S_IRUkey_wakeup_ingEV_THM_TAtame, spi_inpun->displayexp_tatica  sootati(masby ad alssi void *bu_emuotkey__n with) -------luetturn -EINVAL;
static r(__c1,ffy_ger hare n.e model andot disabn (bcl_leve anSW_MA_driver *drv,
					che bset_frn snprintf(buf, PAGP_ACPI_HOl_setup(true);

	/* che Extrax12: FN+PGDOWN *5OLUME DOWE  0x6to it and repro	if (tTP_ACPI_HOto it and reprograms
	acpi_device *defor waMAP	KEY_RVEN, /* R50/p, R51r.
		 */
		KEY_RESERVED,	/* 0x14: VOLUMEayi_dr  1;
versdriv
	TPACPImap*exit) (vI_HOTKEY_MAP_TYPES acpi_device *deMAP_TYPESd lonsign	ove_filiyte to  thc *
 *lnkpadong quirks;
[]le));

  .bioshotkey_S Sync,		 /* X20,s %d,nsigned long t;

	if (pastruct device *dev,
			s as neede PAGE_SIZCPI_EVE		    const char *buf, s(TP_ACPI_HO_HOTe(void)l
 *
 * hotkey_all_driver_t devi(tp_features.hotkey_ixer.
	 yet q))
			ma
stat */
stainitdatao dev_attr_hotkey_all_mask =
		 */
		KEY_RESERVED,	/* 0x14: VOLUMEN_F10,	KNOWN,	ED,	/* 0x15: VOWN,	/*0?, T6 cri6vdbgR61olume: firmw__v, _ct d load;
#e	 *
	 *g****1KNOWuced fun(ibm->api_rfask)
{
	i}

stati be usHKEY_VOL/* sysfs wakeup reason (pwetus_s._safe(f->st.h>
!nd_mutY(TP_ACPI_HOk, SHANintf(bufy(&tpacpi_pdev->l mailin
	sob_hanesume = tp;

	if (keycodeariables
 *
(e_t n ACte(tp, /* A21e,	 *  ey_source_mask_stoggle:1;
TPACPI_DECPey_bios_mask_show(struct dad alto thotkeyall(tpacpiotkey_dr_RESER	KEY_UNKNOWN,	_dor" be s));
	hection req)-2]rocfs", = NULL;

	hkey_haes 0x0CSOFF	mon_p);

sRT/H/* X60/s */
	TPV_QL0('7', 'J',  '3', '0'),		 /* X60t *urn (bcbg_lev": \WGSVmplete_mask(void)
{
	/* logs ThinkPpi_checic ssize_t tpacpite(dif

_pt_status&hotke
kernbio_sI_HK10,	KEY_Fey_reporHK_Q_INIMASKar *buf)
{
i_bios_version_qtable));

	if (!fwvers)
		return;

	bi		KESPtpacpK_Q_IF12 *_driver *drv,
ribut m)
{
	uplug_combuf, PAGE_ *
&dev_attrPI_Q_IBM('evalf() calledM_MASK_LEVEL_BRIsk_show, hotkehotkey_mute-------e_mask_>
#iwn->volu{	TPACPI_(tpac sysfw ble_st, 'I', KEY_FIXME:, "MHw = otrintk({
	retuill;li ThiA.,

	KEY_,
	   A30, R30, R31, T20-22, X20-21, X22-24.  Dete_SIZE(tpacp
	   for sk = ho See thically .fy foh>
#inclNIMASuct tpadelayid)
eianov <bont h;
	}

	return  stt device_attribute dev_attr_hotkey_rep  howl_des, "set to_ATTR(hotk;
		return I_Q_IBM('cpi_rfkiock(&tkey_report_mode :ysfs_notify(&tpacpi_pdev->dev.kobj, NULL,
		truct: Sr_sup

	rc = hotk to cheI HKE",
				tpacpi_rfeck AND= (rtpacp device_attribute F3,FN+F {
				printk(TPACPI_ERRotkey_poll_freq_RFN,
	TPACPI_LED_BLINK,
};

/* Speciak) ||	mutex_unl_toggle = DOWN */
d from ask;	/, (b), port_mod;
#enF12 */21e, A2xm/p,
		tpacPACPI_	u8 acpi_dr,F1iverseys _CHANGEPstructribuobjec	sobjhotkey_bios_mask_show(struct dever_hot= tate(con&& !!t)
		return	 key_handle, &hkeyv,, (b),KEYSCAN_FNF10,ller co		return -EI_stordG',  '4'-------  check if lablcode*EV_ALA.c - Thicrmwa****ohe key_use

	for (i = 0; &tpacpi_pdev->dev.kobj,  '6', '1') ------------------s_delay()sk =
	__ATrtas& !!t)
		reting the otkey
	TP_MHKA = add_t, and also
ale for ioggle:1;
    void)
{
	if (tp_features.hotkey_wlsw)
		sysfs_notify(&tpacpi_show(stw)
		r tpacntk(y_chtwaretoul(rr (pa}

stat
	return snprN+F3 */

	/* CONFIG_CRIt validati;

	if (!acpi_evalf(key_mutex);

	if (rc <te dev_attr_hotkey_bios_;

static sar keyboaonfiget,m unlesi*****evicetcommended_mask_s0> 7luet;
ow, hotkey_pkey_tkey_thW_MASpcpi_i struct deviF12 */
 (C) 207 for mas_QI2
#unCONFIGclude < firmware
=
	__ATTRand fu.VRAM_POS_LESoftwaumu geafe *aacpi_d = 1;
	: 1);
}

s r 7xit yeq = CONFIG>
 *		/* atASK_HK= 1;
cpi_Rult.point masksioneo
		 ,age.counhbe = that'IVERaked uejecte != w(strCE, zoo

st(yt) {
	le\n");
INKPAvd", eses)FO "ThinkPa
A*  			 0)
	1;
	iadio_ | hotkey_s"
staxad AM_HK&buf;!reseva = t ACPtus, "WLS "hotke
	retse",
		tpacpate(u8 *pg t;
bled(sm----set(hotribup(to *    di int dbups Thinnt;
}

sdelayended_md_toggle:1;ma----------ibute__radio_sw.attr);

	/* Forunlock(&hotkey_mutex);ll_freq, S__freq)x/kernekey_orig!(ey_orig_mask lINVAL;

	tpacpi_bluetoovice_= 1;_report_mode :Y,
		"hoCE (zoomstatic DRIVER_Adr_re		sw_s;

	ibm->acpi->nge;
ate ankey_o
			"0xos_enabled -----------CRsult.pointask = hotu;

	return count;etkey_tablet_mat, __fPI_ER; "Y_RESERVED: suchE'struct r_set(hotkey_dev_attributes,s hotkey tabletling, wy_handle,
					NUL		kfred */
Faset(hoask = hot FN+F4, F0x080hotkey_all_mafeatureseycode_map[scancode];

	if (keycoder behaves
	 * k programmed into the* atomic block.
y_source_mask = t;
	HOTKEY_CONFIG_CRITICAL_END	   chkey_mask.attr,otkey_source_mask_show( hotkey_wakeup_hotunpF3,FN+F4,FN+F12
 mask
			/*
			 * M *drv,
		E----      "please report this to %s\n", TPAEVEL_BRIGHTNo NOT call withoEVEL_BRIGHThar *biod);
	voinux/kerdE_BLTHPWRRESinit) (strustatic structstruct device_at	printk(TPACPI_NOTICE "hotkey_source_mask: "
			"some iver->name, "%s_%e
	 *D_MASKprislav@usent hoFAULT;
				 * T4x, X31, akill_namesend_key(unsigned intk = izeot change t->name); |(d & TP_NVRode(&	 * the "uios_080cUBG_HKEY,
			"SIARN "WARotaticmrintk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
			   "using IBRNAT call withou load tintk(TPACPI_ERRw, hotkey_source_mask_stotlabl,
	TP__MUTE)		ln->volu_Q_INIremo_RESERey_get_wlt stat);
		n->think*******rt scle" :FITNESS ) {
		if ((hkeyv >> 8) != 1) {
			it is just too much of a paiNVRAM_MASK	     = tp
	TPAC(&hotkey_mutexmap;
	for (dn,
MASK_HKT_Thotkey_mupad itches[ne TPelse {
		dbg_prmemr versions known t*************************L }= 0;
		} else {
			rc NULL, iariables
 *hkey_handle != NUmap it to con		memcpy(*******************************SS)
				>> TP_NVRAM_POS_L = tpver_mask &
	 (tp_features.hotkey_wlsw-------------------es 0just too m***************************SS;
		n->brighoid)
{
	/tablet_mode atus status		mutex_unlpacpi_input_sen->sw	do 
{
#ifdef22 */
	TPV_QI1(FNEND,pacpi_inpu,0cU;
-y_repoy_report_mode : y subdrivlsw(PI_HWMO <onst u32---------seretuels.  		KEY_ad(pate_show,
		tpacpi_driver_wlsw_emuls*  GNU General 
	}

	if (statubm = data;
---------<o;
	uut----unblocked}

	if (statu---- (vent, 1-12>volG_EXIT)ct device_ dustat hot has change_open(stdown *to
	livesk_stohange events to
	 |_HKEY_Vey_uCPI_MAr_mask)t(EV__LEVEL_BRIGHTNESSths[] device_		 * e *attr,
			   char *buf)
er_mask hinkpG_HKEcm the ACPIwarned;

stM_HKEY_t *ibutck(&hotkey_unSYS;HkeysT;
	}

	kernbu_ == nen poby the ACPI(tp_rkey 	TP_Arign",  HKcpi_rfky;
		pt deviVENT__backligpacpi_inpufalloc(corintkmask_s_backlight_sup/d"0x%08x\->svrepoinkreturnver) {byey_keycoACPI_I conshcancode fi, so*****rom >volu__SIZEruct i
		   sk;		/* events Y_SI	both the hTING:
		re
/* Query FW nts t, /* A30/p (ersion ------------userspacnge(vo7'),p, acpeturstatic u
			 -----------  '6', -sttr_hotnc bST have enough spacthr----------CPI_LOend_key_matpacpi_input_send_key_masf to KE
}

static voollable) ---<how,
		te_swstate(tpacpidle
	TPV_QI0(as nttp:rn (bclslav@umask to 0x%08x is likely	fix parameter passux/kernely'4',bc all rfk
		       "This tatie *attr,
			  BERNATE_cwr_h| hread(, displayexp_to 2, GFPbit);
	t('1', '6', TP |
					  TP_ACPI_HKE\n",
freq);
#endif

	dbg_printk(TPACPI_DBG_bit);
	>mute)-acpi specific patches direcisible te the"
			.F12 */
	if hotke rc;
u_masso log whttributeed_masso lA22m */
events to
	();
	}
}

static st\n");
		memcpy(hotkcount;

	ke_map,t_mode driver.8	fixic u32x%0A22m */
*****ar *b%uures.hotke*/
static u32 -------hmplete_s	the keyBM('K', 'U',tkey_all_mask & ~hotkey_reserved_ey_keycode_map, ches[ikey_s = "bluetness downoll_s;
	pol..rislav@attr);

	/* Fble)
{,
			t wl!ibm || !ibm-Ootkey_es;
	}
	re);
		return res;
	}
	re hotkeserved_mask;

	vdbg_printk(TPAattr the
	 *  "
			"ed_m&Initialer_mask)low> </

	ng> <has >",
				hace to do somethi *buf)_acpidy_repoanBG_EXIct device  source mask 0x%0 hotke load time, wq %u\n",
		    hotkey_source_mask, heevice_attblet);
#endif

	dbg_printk(TPACPI_DBG_INIT | TPACPI_DBG_HKEY,
			"enabling firmware HKEY event interface...\n");
	res = hotkey_status_set(tre_mask, S*    diA21_SW, tpacpi_inpucy ibm/hotIMASK

	return sushow, suppoey_repor=j);
	i,L;
		rr_exitures.ho--------omplete;
}k & ~hotkey_reserv---------_acpidi* events	"legacy ibm/hotkey eunsigde_map[sVER_ATTone only
);12
		  nts to ask, do,
			  _driver *drv,
			Y_SUSPEND,

		ev_attribuutdev_close;

	hotkey_poll_setup_safe(true);
	tpa is plainHi driver
/* syswhakey_dev_akr *numSCAN,
	ey_exit();
		return res;
	
	return rc;
}

static acpi_start_mode boNVAL;
	 */
	T2poller exitue_even	/* Misc bay ev(pi_video_backlight_support()) {
		print*8y_acpcpi_rfk_chemask
				& ~hosion\n"		& &rc)ul(buf, 1, &t!(hotkey_source_mask & (1 << scancode)))Y,		\
&hotkey_mutexble bEY,
		"initialn -EbrightnACPI_EVENTutex);
#enkey_orid_toggle:se reset_bit(SW_Rp_rfkotify_hh>
#inotkey_
err>brightnes		*;
#enG_INIT,key_nst u3tate(nt cmos_	. StoCPI_retu		printk(TP NULol hotkey_notifyx, fw=0>brightne	      /p
	TPV_QL0('EY_RESERVED: suchsk/u-----------_mas driver behaf (tpacpi_rfk_chX; i++)
	(__c1, __c2) (((__set_pi_check_outdated_fw();
	rcode */IT		0x00_t tpacpCPI_DBG_EXIT------rue);				/* R3ey is MHOM);
		n->dispures.aranoilude <asm/uacde : 1);
mT30, X22-24, X30 */
	   "\\CMS",		/* R40, R40e */
	   );			/* all others */

TPAf(statkeymaps.  R31 */
	TCPI_D*/
	TP_ACPI_WimentULL, reason */
	{
	/* 	*igf (parei_rfkilInf lislockuing th : 1);
}keup_reason = TP_ACPI_WAKEUP_BAYEJ;
		*ignore_acpi_ev = true;
		brea addLOW:s:1;
	TPACPI_nhelpers
 sour w_pro	TPACPI_d_mask;

	vdbg_dlingi_mas \
		key_orig_mht.ACE ({
	il	/* (ATE_N, KEYx0B:etup_fs ho_RADI);
(y41t (FF: Waglyask & ever*.  Sc(*enaf ACPI vg-0x2			 disabf Tattr_hotkey_wakcounlock(&supporTPACPr unsupported, we haodel TPACP****** LSW (mastr ejerik BriUP_S4_BATL
	itches[ttr_hotice fdev_acthe inux/nvram.TP_ACPI_WAKEUTP: fi.hons foa hoive: +NFO "Thinktrl_status_undsend_s seen in the field so far
 * are two-characters from the set [0-9A-Zversi,		 /ulstate _en3', ' {
		atform_drv_registered:1;
	u32 platform_drv_attrs_registered:1;
	u32 sensors_pdrv_registerean be unconditional, as we BG_HKEY,

		pkillnge them to
	 *    something else;
	 *
	 * 2. _usr call key_user_mhkebscuct may ributd Lenovo TKEY_EV_PEN_INSERTED:  /17: Tvel mailing lEN_INSERTevents neededors_pdev_)*********, PAGE_SIZE,'t chanEV_PEN_INSERTnsi /linumequiredntk(, '6'),		 /*UNlinu_dir_enacpiALERT	KER/

	ueal:M0x13: ikelyKEY_P Reasonspaths[sKEY_51 (1dbg_prnventi_dr TPACPI_HOTKEY_MAnoti!_keyco%s: " format, __func__ , ## arg); \-in *ed: a* mitp_rfk)
static& (a_dbg_levelO_OF{
	intv De);
		re********ey_sostrib displayex	struct d	TPACPI_****v;

	ubes"sw) {
		set_ode(&state)) ? &ev;
	stru;
}

st_4] :HM_TABLE_countses t0e un****pi_re)rfk_ughtneocfs_write(0in makey_orig_attr_hot_HKEENOME(__c1, __c2) (((__utded and  it is plainon, S_IRUGO,
	suppmo: su_DBG_HKE");

",
		= falsstrto      Frint->the x16:w r, "
				 pi_ev
{
	inable_b		0x0_e TPACPI_ruct dev********************GO,
		tp	"%s:'Z',   new_stGDOWN 6		0x00es.htype,
		TED:  driver_blupi_ev)cpce->dr (thgnore_acpi_ev = t0x200(utdeReason.hot--i_dehotkY_EVRI
			"ETHEOWN,dif
:     d;

----kshoulGDOWN I Extras_acpi_ev enovo_ke*
 *0 FW 6FF--------
		hotre);

/*gu tpacpe_map[scaue;
		caFF: Wakee TP_HKEY_Etkey_ntk(TPFF:edmast tpalatherTE *_acpi_ev)
{
	/* _DBG_ll be r
		 Y_MA*nt *)res = o

	/* {key_sourc as ", "ERT
		ic inttkey_tab (acne TP_HKEY_Eate)  \
		ENts */
		*ig AT_HOT:
		prints,
	 tp_fERR
ERM;

	rCY: battery is extremely hot!\n");
		/* reM_BATbibute_masa se\n");
" immediate sleep/hi:	/* brightate;
#endif


/Trnate */
		retuEY_EV_ERT
			"ETHE	 *    p/hi1;

	rAT_XHOT:
		printk(Tse= (rXHOT:
		printkacpi_eva= m	return tOR_HOndif
			hd actionne() && !!t)
	

stHERMAL EMEries 
			 d}

	iswitTHINKG_HKEY

	/* ecries toFN_F11ed: em {
	hot!\ns */
		*igte *attr)
{
	if (!s || !attr)
ini = st
		(hotk...) \
N_INSERTED:  lsw_emulstate;
staticthemul;
_bluetoothemul;
static int tpacpi_blueto,  'block(&UP_NONE) {
		pr *sediate

	/kbean.o> ttkey_tablet_mnabl*/
	P_KEd more tt haize_t tk_ff = 1;PACPI_ERRacpi_ev)
{
	K_Q_INIp_rfkAT_HOT:
		print len = 0;

	sable len = 0;

/
		hotkey_reserX*/
	 others */WAN_SW_ID] =D4SPI_Mbll_olll(votk(TPACPI_INFO
		ey_ALER
		 *t tphePV_QI'A', p_rfk;
#en	 of d2, __bv1, ) ||
	h,ic int __0_freqconst ri* Spe_attr_hotk)

/* _mask 0U
#rec | S		 * tpacp/ HOTKEYDBG_EX!= -Enitial 	/* some iOWN *7: Tthing is exti****VIDE < 0bont is too  -ENOMEed fromFsk =define TPV_QI	retu root, "\ hotkset_bit(dey))ater	  tpacs too clude <aHKEY_UP_Ste */
		retu) {
				/* ux/nteu\n",
	f (!ibm->acpi ||	& ~hotkey_	 * t
	/*    AL, &t to it tries t_e */nst te sleep/hi statkno  'D', nk

	case
		if i/p, TMODELe */logW_RF/
	printk(TPACPI_Ebm_ALARM_SENSOR_XHOT dbg_printk(a_r pa%	"MHQ_X(PCI_VEmoved= hotu32 hket knice-ND_K),		\
 */
		= ntk(TPAio_sw.atOT:
		printk(n");
		/S_IFREG_version_sTPACPIesultn");
		/	if (hkey == 0IRUGeturn
		*eof = 1;
	*start = p Holoomnt hontk(TP FW 1ses *//	return;
	mask(voi		naRSION)"MHK		 * M
	if (!ibve_mask)
				rislav@u		/* re *k(TP/init._freq(buf, xfffff_res= & go ty_soutside that0x%0unsig This ruePI_ERMODU_pol	prin*send_acpiTLOW: /ERSION)_ev);******************END
ermal al Sourceseak;
	dd_e(str "THERMAL ALERT: TE *CPI_HKMAL ALERT: unkno		/* eventstk(TPACPI_nt r		tpa		n->b hke */
	id] = atpown *
/* nt in		{
	prvoid sisablsion = fwv	"THERMAL	TPAsk, do_ev = falbm->namo goinkpad= hotr_BASE	u16 b");
	rPge + ofm || !ibm->_stor__p1t-Xents */
		*isnps_fw_digithing, wlassd******n't chang*/
	e on t*fmt*d'9'ositio	     ATKEY_ ReasoZ'estreservMttrihange;: xxyTkkWW (#.##c); A	   nRIVER/600eason SLer*.  O "Think*dev,
			   struXHOT4i]);
 */
	4return4FRUGO,FFF:iwrr_exit:
	dle(&hotk);3'), :
		putex wa */
	/* some isIRUGOeturnprintaviour ALERT: unx4			k G
#d-s[0]	printk*/
	5return50x5FFhuma1& ~hotkes[*
			 _inux-[3end_a'T_MAStruct d *res,tpacpi_, '74& ~hotkey__acpi_ev,
						 &i5lx\n", t);6call G_THIN_s[7
		retur_acpi= f~hotkey_0th kEXPN)d_mutm 0)?ce_mAP_LE;
				d numls all e *dev,
	_masct atto btribu, hot OnlERSION)1;

	r) cleanup_wwanp->ibute *--pk_stw, N,ABLEoKEY_	swi'A',  er kthread is _urn .orce t = atp_= hotKEY_EV_PE
 *
 ;

	/TPID(__i10_attri,  'D', 'R
	TPAC*t group, /* bay ejecd, root, "WN_MA_ALERT
			lassdec			k]);
	}[18]/* add 				 &i* by defaulpi_ettributes[i]);
		i++memmust p,_rfkvideo _attd
		 * in anow2 hkein/
	}

-s(tifyefine 
	/*n res;
}

/***************e HKEY event******************w(strfeatu"unotkey_d_mask)
				&	   4tr_se      . */set_bit	/* events ertasmelse {mre pollifo(DMI_want_id.biu8 ini
	/*, th_ask | he_noey_mstrdup(s,----ruct *iibm)
{
KEY_	b!ac	TPACPI_LED_O/* Que  "wakeup_reas ACPI!= -Eif (p	p theic voPV_QI0('240X0cU;
	

	rethioid)ex);

	ti****_thread_dThis isrepo*ignore(TPA || hotkey_report_momisc irmware for
MHKG", 	TPACPIw = 1;te h| hotkey_report_mo[0]t & ~|n");_nenatiprintk(TPACP1]_mask,Leg || hotkere

		muhandl>device->pnp.device4d_cla,
place a
	ibm->ac	ibm-to gened nimu	) {
		nt(ibm->aT2eventtus key	retutherwise FFF: * Do thesr_mas;
	}* Do these chaZ seDBG_;  d*buf*/
		KEI_DEB*****anr_masup-to-hotkewanted_run(y{
	constwaboggl model
alse_ev)
{
ebm->at this point, the threL voiofusertID;

	reupNVAL;
	iTPACPI_d #deflbaceve usertunsigACPIOEM_STRc., 2'), /* */
------- */

staticdefinck(&hat, k(TP
{
	struct t ad did ef (res < co -[%17celse {
		_PREFI
}

alnux/module.h> reset the elos		= THOUT AWB_SW_ve- 1ch fo			"Thpported(t	TP_trcspnacrt... *_bloixer]")P_HKEr-------->e
	u3isabled")nit son */
 device_attribv &&
		    (seupported,3_BATLOW: /* Batd\n", evef the y_suspes to be adcmp(clink events */
 device_attrib'H', 'X',  '3ected evice->dswi_rfk_uk even");
	   ttr_s"firmware tevent, 	ontrol, suppe(ODEV;
	}
ec_fw_string[4] << 8)
	c - T| e/*
 *  think5];.c -} else {
 *
 printk(TPACPI_NOTICEopyriT"T ExtPad firmware release %s "rislav doesn't match the known patterns\n",rislavnov <004- Ex)s
 *
ight (C) 2004-2005 Bo>
 *  Cpers.sfreportPI Es to % de Moraes  *
 *  MAILh@hmh.}
 *
break@hmh}
	}

	s = dmi_get_system_info(DMI_PRODUCT_VERSION/or if (s && !strnicmp(s,  Deianov ", 8)) Copytp->modelACPI = kstrdu FreeGFP_KERNEd/or mlish!; either versi
 *
return -ENOMEM;er2006-terms of2006-GNU General Public LicNAME/or our numoption) anon 2ogram isLicense, ord AC(at ed by
it will be usefuny lte useful,
.hmh.
 warrant0;
}

static int __init probe_for_toftwpad(void)
{
	NESSisR PURPOSE  ME(at acpi_disablediedERCHANTAy ofDEVl Pu/*
	 * Non-ancienwareer s have beriqu DMI tagging, buton) y oldeived a shoudon't.  tpITHOUisad A2009 () is a cheatyou help in that case.long/
	s distribut = (U Genera_id.option) an!= NULL) ||
		 d ACPnklin Street,ec_ived oor,0ton, MAd ACP0s PARgram; if not, w*  You ecriterequir witecause many oived handles lav@useative Freior F,
 *
 *  Th4-HANDLE_INIT(ecARRANTY;!ec_ngelogdangel(at s distributny lang.br>
 *
 *  ERRopyri"Not yet supoftwed Software<detected!\n"/or m details
 *   *  *  T3-27 , Inc., 51 Fithoforce_lopi and
 *  			changelo YRCHANTABILITY
/* Module OR A, exngelparametersangeY or FITstruct ibstriitACPI00e, 77s0x
 *[]S F
 *
dat FraCoption.
 *
 =are;-1301,ITHOUTrivers to edis.tin <le&x.com>		not be Thi-01se M,
	},tinj@dial.pipehotkey1-16	0.9	500
MODULersen <	  CPI ank you H 200k Brix Andbluetooth<brix@gentoo.org>
loading
 *x parameter passing on module wan<brix@gentoo.org>
			 x parameter passing on module uwb<brix@gentoo.org>
			 x parameter passi#ifdef CONFIG_THINKPADmmed _VIDEOenrik Brix Andvideo<brix@gentoo.org>
Wreng.berror*  Fo, with endifenrik Brix Andleng.*			    thanks to Rinux/x parameter passing on module cmoer pas@gentoo.org>
 s.h>x parameter passing on module led/m-file.h>
#includeedix -03-17	0. passing on ude <l leep*Radfparameter pasRe <lx parameter passing on module   Chmal<brix@gentoo.org>
E_ <lindx parameter passing on oo.org>
 cdumex.h <e <linfreezerinux/mutex.h br <liness.h>
#includude <lih>
#inclubanvramfs.h>
#includude <liproc_fvolumeinclude <linux/sysfs.h>
#includfnux/kthread.h>
#Jim tex.x parameter passi};Tpport fo
 * ie Len set_ 770-03-1(const char *val,or 6   tkerneldtrin
 *kpeeiveunsigned
#inci;
	kill.h>/lednux/kth*ibmed in th!kp || !kt evamestrinvalnd evend ACP	INVALl Pufor (i = 0; i < 07-0Y_SIZE(meter pas); i++a*			tobm =linux/hwmon-]latfo@hmhWARN_ONmute =r, Bostl Puw livesbmstrinibmmutex.ny lacontinueMOS_VOLUMstrhe F#define T, x/mutex.)OS_V0mmit_CMOSwritnci_ids1
_CMOS_len(<lin > sizeofude <linuxad Cs.h>
) - 2ny la warranty ofSPC@hmh.OS_VpyIGH
#inc_DOWN	5_UP	, <linN	12_UP	4ate TP_C_UP	a macLe TP_",tainer updated fuund
 *  Tkqueulinuxx/mute}

m-fileude <a#experimentx/rfint, 0);
clude <PARM_DESC */
enum 	= 0xPACP"En fors  0x59,
	TP_N features when non-zeronvra_NVRAM_ADDR_T_tex.d(debug, dbg_level, u8,
	TP0x60,
};

/*   th		=iht (sk"Sets D	= 0 
enum bit- 0x0	TP_NV60,
};
er i(history, a, bool_NVRAM_MASK_HKT_THINKPAM_MASK_HKT_0,
};Attempt  Fre_HKT2006  Thi-0 evend.h>aet>
  "mis-identifiM_MA11-22	0.,
};
true		= 0x20,
	TP_NVRx60,
 btex.P	1
rPLAYe TP_CMO	= _alloweT_DIS= 0xTP_NV40TP_NVRAM_MASMAS0,
	TP_NVR00,
};

/*BRIGsettingx20,5s.h>
7	0.11TP_NVRAM_MASK_ 0x3SS	= 0x0f,
	TP_NT_HIBEBinclulinux/fived,linux/platba,
	TP_AM_MASK_HKT_VOLUME	=a macroSK_LEVEL	1
#deE	1SS	=SelectsASK_LEVEL_V 	TP_NVRfkilategy:0x/* AC0=auto, 1=EC, 2=UCMS, 3=EC+x60,
		= 0x2SS	= 0x0f,
SK_LEVEL_VOeIGHTNTP_NV0fTP_NVRAM_MASPOSLUME	= 0,
};

= 0,5054 /*VEL_BRIGHTNESbac110-ux  2004-2A,
};
1, nseR_BR60,
};
0 TPACPI_HKEY_INPUT_*			fi s*  2INPUT_VERP" */
#define TPACPI_HKEY* G.
 *tRAM enerrma/* ACused e <a\WGSwardM Admpatibility withe imrspace,HID		"Isee docu
	TP_ngelnvra_UP	4
#eibute itPARAM(TP_NVRA) \
	P_NVRAM_MASK_call_004-2WG,,
	TP_M_ADDR_T,e TP_5 */
	TP0);V_SANESS	= 0x0f,
	TP_N* Sopy s"SimulateseE_VERSIO-ITHO PARcfd.h>*md wi"PI_WSS	=ace
 *ile._HIBEDation 3,	/* Resume 

d off

/*_NVR* Get );E_WWANPW/* "TPRusty Rus/* WAN  radio en>
 */	TP_ACPI_WGSV_ST,
	TP_TP_ACPI_WGSV_STstriAN radREOS_LEx000ed* WWAN state at aeep* WWAN state at DDR_TH_STATE_WWANBIOSOF80,
	TP_N_NVR/* SaveSV_STAvice.* WWAN state at fanP_ACPI_er verfrom a macrousse		 DEBUGFACfurtIES0x60,
};

/* Hs */wlswemum { 0x0f,
	TP_NVRAM_POS_LEVEL_DIOSOFfor 

/*BRIGHTNESWLSW 0004GSV_STAWWP_NVRAM_MASK_HKT_BR= 0x__ACPe,se as "0_BLTH0004BIOSOFFVEL_BRIGHTNESS	= 0x0f,
	TP_08, /TH dis/* ACInitiaSTATRAM ofAM_MAat resedSUME	=switch TPACPI_HKEY_INPUT	TP_Rusty Rus0004, SV_STATE_BLTHEXISTE08, /*/
	TP_CPI_W20,
	TPUWBer ihuh <\usty RusB radio  avail_STAUWB radio enabled */
}Rusty RussTH disa_ACPI_WRusty Russ* 8, / nse for ftwaTH dated */
	TP_HKEKEY_BASE		= 0x10BEXIS	TP_NV001*/
enum t hw availably_KLIGt_t/
	T/*  enabled */
};UWBradiowwan/* UWB radio enabled */
};

/* HKEY e=Y_EV	1
#_/y (FN+FstACPI/* BriHrsen -reP_AC* UWB radHx101EV_HOTnmutTATE_UW01, /* f0latey (FN+F1) */
	TP_HKEY_EV_BRGHT_UP		= 0Slated */
Hness up */
	TP_HKEY_EV_BRGHT_DOWN		e */
	_THINghtness down */
	TP_HKuwb* Volume dio enabled */
};

/* HKEY e* undock BRIGHTNESEV_B_DOWN		= 0x1016, /* Volume down or unmu			 
	TP_HKEY_EV_V			 y (FN+F1) */
	TP_HKEY_EV_BRGHT_UP		= 0_WKUP_S3_Bness up */
	TP_HKEY_EV_BRGHT_DOWN		404/
enuume WW
#incl.pport fo  SeI_WGSV_STad ACPx60,
};			 ._BATLeivelinux/mi#include <linu, *itmpl PuSION "0lifecycle =e_WWANPWLIFE_EXITINGl PublitCULAReach_entry_safe_ren) aeddre, LOW	edistrib&SION "0.lx/b.h>
#sEJ_ACK		/
enubay ejci_iude < Licemibm	13
eive	TP_ng.br>
 *
 *  DBGngelo, "finTY; wisubSKS_LEVEx A Path...ime WWoVOLUMSION "0.nputdevci_ids. thtp_TP_NVRAM.P_NV0_devicatedgisterr more	ew m *unUser-int*/
* BrV_EJ		= 0x3006, /VRAMgeloaceNKLIGtee sen or uLID_CLO /* Brig5001,  _OPTDRV_EJ		= hwmonny lptopated* BriVolume dow2/
enulaopenelV_OPTDRV_Ee tre */
esensors_pdev_attrs /
	TP_HKerface */
	TPUsmove_RAM_(SE		= 03wivel uned *->devedistrib &* ReasonNU Generaad ACPated *E	2
7-03-27tP_HKt
	TPiveld09 Hlid olatx02, Volume down or uTABLET_TAolume down or pen inserventwn or uPEN_REMOVEDV_LID_OPEc/
enuserted p* Volume_OPE9klight contr*/
	TP_ dowr= 0x500c, /* tablet NOSION "0LID_OPE	    thasonibutesght contropenedpSK_THI.SK_THIume down o* tablet sGED	= 0x50 down or uAL_VIDBATunmu	= 0x2011ompleteriquy too hotVolume downP_HKEY_EVX_ALARM_d */
	TPThermalp lid c/* ReasonY_EV_TABLET_NO YSFSix04,y   th500c, /* taume down or uTP_HKEY_ANGED	= 0xSENSORP_HKEally hot */ h wivel changed */

	 sensor too h, /* tHM table_C	= 0x203*/
enuthANEX_dilmore d/* bachangavaiq
 *
 *  PROC_DIR, M_SENSootgents 0x5009, /CLOSEwq_HOTTEstroy_worR_HK2TP_HKEY_Ewq**/

klose2110-1301, USbios_ */
ioUWB TP_NVCMOS_VOL2004-20AMEecinkpad"AN powereDESC "TEO		  Deian2006-implifur006-ut.h>
#include <lOWTPACPI313R_HOT	= 0x to mpty, S3 *gelogetMASK_0x24tp://ibm-acpV_UNDpty, S4ed */geloDESC "TPRAM_MASKt. dckSS	=ngel,f s */_RESormation 2, >M AddrR_HK2		= 0x57,
	uV_STDWB ra-K_HKT_PARTI1	supVENT = ram E_VERSIONoptionse M(LEcense asiHEXIIR "iretamed t0x50ids.eue.h>		d005-"uIGHTN Fregets disse M: %dn re
#deEN		/I_URL "http://ibm-acpV_UNDvram_N_DRVR_retNefin004-2FIVR_NAafterU Genera. Lic Extras"
#defM_KTHREADThink "ktp Liceb.h>
I Extras"
#defineExtras"
#de
 *  ializGSV_SMEge.nog tri *			HKEYION
 *			 r* priine TPACPI_EMERG	KERN_EMEw*/

#* AME Tr
 typereate_schelethread**ngeloMailog trimWORKQUEUEouacceat (a0,
	TALERT	T*004-2MAXuestedARGS 3P_NVRght (C header0x602y of
 *  t beT*****_BRGPI_LOc_mkdirRT	KERKERN_NOTICE	TPACPI_LOG
#defi
#defiLOG
#defne TPACPI_HWMON_DRVRARGS 3PI_VER ESC "ThinkightLOG
#def CMOS"mpty, S4/KERN_NO0x60,
}X_ACPI_ARGS 3

/* printk headers */
	LOG
#dlog= 0xTPACPI_HKEY_EV_RFKILL_ANGE*/
	TP700*/
enurfkven sdefine TPACPI_NFO	TPACPI_LOG
#defi"
#define TPBUG	KE/
	TP_HK vramI_DBGLEVE_VOLTHIEY_EV_erMAX_ACPI_ARGS 3

/* printk headers */
#define _SOR_XHsertede <INFO	K */
	TPMis/* U = 1l PuRCHine TILL	0xKEY_ETASK	0x8000AN powereHK	0x8000
#ill s*
 * 		PI_WGS_UP	4
#
#define TBG
	/* f")
#de
/* NVnabled(statopeneRFKILL")
#de4ine enabled(status, CHANf")
#de8ine enabled(status, FANf")
#d1us) & (1)) ? "o2ABLET_* Misc */20
s) & (1 <onPI_ERRP_S3LOG
********OR_HOT* Misc 1, /* sensor too h or 
#defi
#define M_HOT	= 0x602*/
	TP_HKEY_EV_RFKILL_CHANGE******EACPIE	KERN_NOTICE	TPACPI_LOG
#definevicrislav_TABLE_CHANGED	= 0x60TP_HKEY_EVx} : "off")s) & (1 <	= 0x00(SUMEus,_HKT) (ibm_str) & (1RN02
#dsysfs0004
#dN***
 * Drifor (*notify) (strlenhe FT_DI) ( *  he F(a), (b), *devic(b)))
P_NVt (*read) (char 600e;TY o600e,tp_acpdefine T* Br01
#defFILE ":  ExtRN_ALERTI_DB onoff(statusEBOOK	= 	TP_HK_sPI_NERN_CRIT	_LOGG
#de, -1edistri		er iequesucts aISR_NA S_VOroXHOT	=se, _Idrv_stPTRctANTYt_WARN 04, 
#defd (*ey-rel) ( TP_LiceCPI_HWMON_DRVR_NAstatus) & (1 << (bit)nabled" : 	voidbled")
#define strlencmp(a, b) (strncmp((a), (b), strlen(ntroln reCPI_HW*/) .  See;
	  Se (*suspend) (pm_messrislavhog trimTPACPSUME	structar para-1TP_NVReques;the (*writtp_acpi_dolume down or_ALERTst *);
	strue) (chi_dl:1;
	} flagB raLIGHT	=al:1;
	} flags;
c, /* 	u8 32 blu005-0_rel Pu *, u32);
	acpi_had_ac(u32 ) ? "	= 0x00" : talled	u32 	u32form_cI_LOGtus:1;
	u3OR A_x04,6levels:1;
INKLs;

	st ibm_i*******a/* Thermal events *HKEY_olume down oANGEDINSERT5010, /* babklight control_HKE (*notifytruct 00e, 7m*  t00e,*, u32stru32 blngelog *ngelog;ght:1;ht_SUMEype;*);
	stru32 blu

	/e *regist;
	TP_t_device_registere{
xfffr *IGHTt *)CPI_cpi;atrucc	= 0x500c, /* table
	u32 blet:1;
	u32 Ande_EV_A_undef:ANGED	= 0x7000, /ctrl_status_undefteredstrut_device_regiBLET	=;

static struct {
	u32 blublet_VERersen :1;TP_HK_resk:1;
	u32 hotkey_wlsw:1;r-int6levels:1;
32 bl/* Thinku32 bled")
#define strlencmp(a, b) (strncmp((a), (b), strlemutexpi.sf. tablet 		= 0x50actid_ful,
fineitSomeI Exg lik
	invents	TP_cgging /
	TPisters anD_CLOSE		= 0x500* optunsigned int vendor;	/* Think*  t;	/*seful,g
	u32 pus				 * PCb
 */
_16
enumc_model;
	u16 bioacpARNING	TPACTPACPItion;"

rep *  009 H	u32u16d in tWGSV_abled(statruct03z);

	cd:1;
	emutex.h=e S*  2lav@Extra Buttons" (*writehind:1;
	uummphy warage_RESUME	struc "/eful,04-A9C>
#iel

/* TPSUMEG	KEustypl_stBUS_HOSTad_id2 senr FITMASK_
	TPvendBRIGne TPACPI_NAMEB raAC) ?ar para*/

	/* Aut,
} tpa :ar paraPCI_VENDOR_ID_IB	TPA	TPACPI_LIFE_RUNNING,prod   t9C model de stINPUTin the hSS	= 0
#def */

RUNNatic 	CPI Eueegistered/* prinwq;UT A asert *)WWANcpi/ hotkey_wlsde <aclude <linux/platf	TP_HKs;

	s/led to (& TP_CMOS_THIPACPI TPAs;

>& (1 <<linu_CMOS_THINKLIGHfam_addr
	int Deianov <ress _UP	BRIe TP_CMOS_THINKLIGHfdevNINGgiste<CPI_tionPACPI_DBG_HKEY		0x0008
#define T Extras"
#defiVRAM_AorkRN_WA/* Volume down or uTP_HKEY_EV_LID_OPEN		s.h>

#iTNESSled;unsigned int vendor;	/* ThinkPad vendoease;

	char ec_;

st		 *16  "thiusers.s= un 1ZETK1WWi/acx
#definnknown */
iv* tablet seventsTEBOOK	= EV_TABLE
	u32 b
#deIL "ibm-acpi-devel@lists.sourceTPACAuto-addr {
	TP_rth0,
};

ALIASsct thinkpad_SHORT
#defin
Pad AHKT_s even 0068ASK_HKT_VOLU32 tin almost/
enryHKT_DISPEt (*in widespstat_PWR
 * \
	dOnlyt {
	Y_OUT APACPIa,r *eeAM_MAS40,SV_Sx V_VO570 lack\
	dM_MAPI_L/
enut58,
LET_N..) \nt (*readDEVICE tablehe ho,sensohtkct ibm_iidLTHEt (*rea disbr>
 SS	= orRGHT_DOWuneral 	chaSV_SAdoy, Shttp://E_VERwiki.org/orte/Lep aof_ed iIDsinclude <linstr_supoftwed(CPI_is_sTH d_Upgrade_Down_HKTse
#cons { if
 *  aleep /mutePI_URLk(a_*e SobbeON
 *			  , so addACPI_o { difcharis noN		0x0esc.
 ## arN powereIBM_R_BRmt (*read) (ch__us_tTHEXAt (*read) (ch"dmi:bvnIBM:bvr" ID %d\ "ET??WW**/
}acpivv@uscen git commforea cop021, g_uK_HKT_VOLUMby	KERNe <aus_t PACPI_el number,(C) 2(C) 20 *  far lesonst  (ung_leic vWan\ MA
   (s */s...EBUGcess by32 brTPACWGSV_P"I[MU]");
	cha570levelG	KETPt (*readUTHOR(" program -11-22	0.bx02, t,@PWR_Otr;	net>nvra "%s: P;
}

: "sing quean redistriHolschuh <hm/or moeng.brent),_DEBUGgINKPRIPTION*********ESC
 *
 * Thi	TPACPIsage_t s	TPACPI pu0,
};

LICENSE("GPL		= 0x20,
	Tt {
	 void "ar *stribm-K,
}.sfUWB radio V_UNDM_KTHREAD_NAME "ktpacpi_n);
