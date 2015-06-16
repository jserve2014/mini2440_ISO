/*
 * dc395x.c
 *
 * Device Driver for Tekram DC395(U/UW/F), DC315(U)
 * PCI SCSI Bus Master Host Adapter
 * (SCSI chip set used Tekram ASIC TRM-S1040)
 *
 * Authors:
 *  C.L. Huang <ching@tekram.com.tw>
 *  Erich Chen <erich@tekram.com.tw>
 *  (C) Copyright 1995-1999 Tekram Technology Co., Ltd.
 *
 *  Kurt Garloff <garloff@suse.de>
 *  (C) 1999-2000 Kurt Garloff
 *
 *  Oliver Neukum <oliver@neukum.name>
 *  Ali Akcaagac <aliakc@web.de>
 *  Jamie Lenehan <lenehan@twibble.org>
 *  (C) 2003
 *
 * License: GNU GPL
 *
 *************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ************************************************************************
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/blkdev.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <asm/io.h>

#include <scsi/scsi.h>
#include <scsi/scsicam.h>	/* needed for scsicam_bios_param */
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>

#include "dc395x.h"

#define DC395X_NAME	"dc395x"
#define DC395X_BANNER	"Tekram DC395(U/UW/F), DC315(U) - ASIC TRM-S1040"
#define DC395X_VERSION	"v2.05, 2004/03/08"

/*---------------------------------------------------------------------------
                                  Features
 ---------------------------------------------------------------------------*/
/*
 * Set to disable parts of the driver
 */
/*#define DC395x_NO_DISCONNECT*/
/*#define DC395x_NO_TAGQ*/
/*#define DC395x_NO_SYNC*/
/*#define DC395x_NO_WIDE*/

/*---------------------------------------------------------------------------
                                  Debugging
 ---------------------------------------------------------------------------*/
/*
 * Types of debugging that can be enabled and disabled
 */
#define DBG_KG		0x0001
#define DBG_0		0x0002
#define DBG_1		0x0004
#define DBG_SG		0x0020
#define DBG_FIFO	0x0040
#define DBG_PIO		0x0080


/*
 * Set set of things to output debugging for.
 * Undefine to remove all debugging
 */
/*#define DEBUG_MASK (DBG_0|DBG_1|DBG_SG|DBG_FIFO|DBG_PIO)*/
/*#define  DEBUG_MASK	DBG_0*/


/*
 * Output a kernel mesage at the specified level and append the
 * driver name and a ": " to the start of the message
 */
#define dprintkl(level, format, arg...)  \
    printk(level DC395X_NAME ": " format , ## arg)


#ifdef DEBUG_MASK
/*
 * print a debug message - this is formated with KERN_DEBUG, then the
 * driver name followed by a ": " and then the message is output. 
 * This also checks that the specified debug level is enabled before
 * outputing the message
 */
#define dprintkdbg(type, format, arg...) \
	do { \
		if ((type) & (DEBUG_MASK)) \
			dprintkl(KERN_DEBUG , format , ## arg); \
	} while (0)

/*
 * Check if the specified type of debugging is enabled
 */
#define debug_enabled(type)	((DEBUG_MASK) & (type))

#else
/*
 * No debugging. Do nothing
 */
#define dprintkdbg(type, format, arg...) \
	do {} while (0)
#define debug_enabled(type)	(0)

#endif


#ifndef PCI_VENDOR_ID_TEKRAM
#define PCI_VENDOR_ID_TEKRAM                    0x1DE1	/* Vendor ID    */
#endif
#ifndef PCI_DEVICE_ID_TEKRAM_TRMS1040
#define PCI_DEVICE_ID_TEKRAM_TRMS1040           0x0391	/* Device ID    */
#endif


#define DC395x_LOCK_IO(dev,flags)		spin_lock_irqsave(((struct Scsi_Host *)dev)->host_lock, flags)
#define DC395x_UNLOCK_IO(dev,flags)		spin_unlock_irqrestore(((struct Scsi_Host *)dev)->host_lock, flags)

#define DC395x_read8(acb,address)		(u8)(inb(acb->io_port_base + (address)))
#define DC395x_read16(acb,address)		(u16)(inw(acb->io_port_base + (address)))
#define DC395x_read32(acb,address)		(u32)(inl(acb->io_port_base + (address)))
#define DC395x_write8(acb,address,value)	outb((value), acb->io_port_base + (address))
#define DC395x_write16(acb,address,value)	outw((value), acb->io_port_base + (address))
#define DC395x_write32(acb,address,value)	outl((value), acb->io_port_base + (address))

/* cmd->result */
#define RES_TARGET		0x000000FF	/* Target State */
#define RES_TARGET_LNX  STATUS_MASK	/* Only official ... */
#define RES_ENDMSG		0x0000FF00	/* End Message */
#define RES_DID			0x00FF0000	/* DID_ codes */
#define RES_DRV			0xFF000000	/* DRIVER_ codes */

#define MK_RES(drv,did,msg,tgt) ((int)(drv)<<24 | (int)(did)<<16 | (int)(msg)<<8 | (int)(tgt))
#define MK_RES_LNX(drv,did,msg,tgt) ((int)(drv)<<24 | (int)(did)<<16 | (int)(msg)<<8 | (int)(tgt)<<1)

#define SET_RES_TARGET(who,tgt) { who &= ~RES_TARGET; who |= (int)(tgt); }
#define SET_RES_TARGET_LNX(who,tgt) { who &= ~RES_TARGET_LNX; who |= (int)(tgt) << 1; }
#define SET_RES_MSG(who,msg) { who &= ~RES_ENDMSG; who |= (int)(msg) << 8; }
#define SET_RES_DID(who,did) { who &= ~RES_DID; who |= (int)(did) << 16; }
#define SET_RES_DRV(who,drv) { who &= ~RES_DRV; who |= (int)(drv) << 24; }

#define TAG_NONE 255

/*
 * srb->segement_x is the hw sg list. It is always allocated as a
 * DC395x_MAX_SG_LISTENTRY entries in a linear block which does not
 * cross a page boundy.
 */
#define SEGMENTX_LEN	(sizeof(struct SGentry)*DC395x_MAX_SG_LISTENTRY)


struct SGentry {
	u32 address;		/* bus! address */
	u32 length;
};

/* The SEEPROM structure for TRM_S1040 */
struct NVRamTarget {
	u8 cfg0;		/* Target configuration byte 0  */
	u8 period;		/* Target period                */
	u8 cfg2;		/* Target configuration byte 2  */
	u8 cfg3;		/* Target configuration byte 3  */
};

struct NvRamType {
	u8 sub_vendor_id[2];	/* 0,1  Sub Vendor ID   */
	u8 sub_sys_id[2];	/* 2,3  Sub System ID   */
	u8 sub_class;		/* 4    Sub Class       */
	u8 vendor_id[2];	/* 5,6  Vendor ID       */
	u8 device_id[2];	/* 7,8  Device ID       */
	u8 reserved;		/* 9    Reserved        */
	struct NVRamTarget target[DC395x_MAX_SCSI_ID];
						/** 10,11,12,13
						 ** 14,15,16,17
						 ** ....
						 ** ....
						 ** 70,71,72,73
						 */
	u8 scsi_id;		/* 74 Host Adapter SCSI ID      */
	u8 channel_cfg;		/* 75 Channel configuration     */
	u8 delay_time;		/* 76 Power on delay time       */
	u8 max_tag;		/* 77 Maximum tags              */
	u8 reserved0;		/* 78  */
	u8 boot_target;		/* 79  */
	u8 boot_lun;		/* 80  */
	u8 reserved1;		/* 81  */
	u16 reserved2[22];	/* 82,..125 */
	u16 cksum;		/* 126,127 */
};

struct ScsiReqBlk {
	struct list_head list;		/* next/prev ptrs for srb lists */
	struct DeviceCtlBlk *dcb;
	struct scsi_cmnd *cmd;

	struct SGentry *segment_x;	/* Linear array of hw sg entries (up to 64 entries) */
	dma_addr_t sg_bus_addr;	        /* Bus address of sg list (ie, of segment_x) */

	u8 sg_count;			/* No of HW sg entries for this request */
	u8 sg_index;			/* Index of HW sg entry for this request */
	size_t total_xfer_length;	/* Total number of bytes remaining to be transfered */
	size_t request_length;		/* Total number of bytes in this request */
	/*
	 * The sense buffer handling function, request_sense, uses
	 * the first hw sg entry (segment_x[0]) and the transfer
	 * length (total_xfer_length). While doing this it stores the
	 * original values into the last sg hw list
	 * (srb->segment_x[DC395x_MAX_SG_LISTENTRY - 1] and the
	 * total_xfer_length in xferred. These values are restored in
	 * pci_unmap_srb_sense. This is the only place xferred is used.
	 */
	size_t xferred;		        /* Saved copy of total_xfer_length */

	u16 state;

	u8 msgin_buf[6];
	u8 msgout_buf[6];

	u8 adapter_status;
	u8 target_status;
	u8 msg_count;
	u8 end_message;

	u8 tag_number;
	u8 status;
	u8 retry_count;
	u8 flag;

	u8 scsi_phase;
};

struct DeviceCtlBlk {
	struct list_head list;		/* next/prev ptrs for the dcb list */
	struct AdapterCtlBlk *acb;
	struct list_head srb_going_list;	/* head of going srb list */
	struct list_head srb_waiting_list;	/* head of waiting srb list */

	struct ScsiReqBlk *active_srb;
	u32 tag_mask;

	u16 max_command;

	u8 target_id;		/* SCSI Target ID  (SCSI Only) */
	u8 target_lun;		/* SCSI Log.  Unit (SCSI Only) */
	u8 identify_msg;
	u8 dev_mode;

	u8 inquiry7;		/* To store Inquiry flags */
	u8 sync_mode;		/* 0:async mode */
	u8 min_nego_period;	/* for nego. */
	u8 sync_period;		/* for reg.  */

	u8 sync_offset;		/* for reg. and nego.(low nibble) */
	u8 flag;
	u8 dev_type;
	u8 init_tcq_flag;
};

struct AdapterCtlBlk {
	struct Scsi_Host *scsi_host;

	unsigned long io_port_base;
	unsigned long io_port_len;

	struct list_head dcb_list;		/* head of going dcb list */
	struct DeviceCtlBlk *dcb_run_robin;
	struct DeviceCtlBlk *active_dcb;

	struct list_head srb_free_list;		/* head of free srb list */
	struct ScsiReqBlk *tmp_srb;
	struct timer_list waiting_timer;
	struct timer_list selto_timer;

	u16 srb_count;

	u8 sel_timeout;

	unsigned int irq_level;
	u8 tag_max_num;
	u8 acb_flag;
	u8 gmode2;

	u8 config;
	u8 lun_chk;
	u8 scan_devices;
	u8 hostid_bit;

	u8 dcb_map[DC395x_MAX_SCSI_ID];
	struct DeviceCtlBlk *children[DC395x_MAX_SCSI_ID][32];

	struct pci_dev *dev;

	u8 msg_len;

	struct ScsiReqBlk srb_array[DC395x_MAX_SRB_CNT];
	struct ScsiReqBlk srb;

	struct NvRamType eeprom;	/* eeprom settings for this adapter */
};


/*---------------------------------------------------------------------------
                            Forward declarations
 ---------------------------------------------------------------------------*/
static void data_out_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void data_in_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void command_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void status_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void msgout_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void msgin_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void data_out_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void data_in_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void command_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void status_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void msgout_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void msgin_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void nop0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void nop1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb, 
		u16 *pscsi_status);
static void set_basic_config(struct AdapterCtlBlk *acb);
static void cleanup_after_transfer(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb);
static void reset_scsi_bus(struct AdapterCtlBlk *acb);
static void data_io_transfer(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb, u16 io_dir);
static void disconnect(struct AdapterCtlBlk *acb);
static void reselect(struct AdapterCtlBlk *acb);
static u8 start_scsi(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb);
static inline void enable_msgout_abort(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb);
static void build_srb(struct scsi_cmnd *cmd, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb);
static void doing_srb_done(struct AdapterCtlBlk *acb, u8 did_code,
		struct scsi_cmnd *cmd, u8 force);
static void scsi_reset_detect(struct AdapterCtlBlk *acb);
static void pci_unmap_srb(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb);
static void pci_unmap_srb_sense(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb);
static void srb_done(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb);
static void request_sense(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb);
static void set_xfer_rate(struct AdapterCtlBlk *acb,
		struct DeviceCtlBlk *dcb);
static void waiting_timeout(unsigned long ptr);


/*---------------------------------------------------------------------------
                                 Static Data
 ---------------------------------------------------------------------------*/
static u16 current_sync_offset = 0;

static void *dc395x_scsi_phase0[] = {
	data_out_phase0,/* phase:0 */
	data_in_phase0,	/* phase:1 */
	command_phase0,	/* phase:2 */
	status_phase0,	/* phase:3 */
	nop0,		/* phase:4 PH_BUS_FREE .. initial phase */
	nop0,		/* phase:5 PH_BUS_FREE .. initial phase */
	msgout_phase0,	/* phase:6 */
	msgin_phase0,	/* phase:7 */
};

static void *dc395x_scsi_phase1[] = {
	data_out_phase1,/* phase:0 */
	data_in_phase1,	/* phase:1 */
	command_phase1,	/* phase:2 */
	status_phase1,	/* phase:3 */
	nop1,		/* phase:4 PH_BUS_FREE .. initial phase */
	nop1,		/* phase:5 PH_BUS_FREE .. initial phase */
	msgout_phase1,	/* phase:6 */
	msgin_phase1,	/* phase:7 */
};

/*
 *Fast20:	000	 50ns, 20.0 MHz
 *		001	 75ns, 13.3 MHz
 *		010	100ns, 10.0 MHz
 *		011	125ns,  8.0 MHz
 *		100	150ns,  6.6 MHz
 *		101	175ns,  5.7 MHz
 *		110	200ns,  5.0 MHz
 *		111	250ns,  4.0 MHz
 *
 *Fast40(LVDS):	000	 25ns, 40.0 MHz
 *		001	 50ns, 20.0 MHz
 *		010	 75ns, 13.3 MHz
 *		011	100ns, 10.0 MHz
 *		100	125ns,  8.0 MHz
 *		101	150ns,  6.6 MHz
 *		110	175ns,  5.7 MHz
 *		111	200ns,  5.0 MHz
 */
/*static u8	clock_period[] = {12,19,25,31,37,44,50,62};*/

/* real period:48ns,76ns,100ns,124ns,148ns,176ns,200ns,248ns */
static u8 clock_period[] = { 12, 18, 25, 31, 37, 43, 50, 62 };
static u16 clock_speed[] = { 200, 133, 100, 80, 67, 58, 50, 40 };


/*---------------------------------------------------------------------------
                                Configuration
  ---------------------------------------------------------------------------*/
/*
 * Module/boot parameters currently effect *all* instances of the
 * card in the system.
 */

/*
 * Command line parameters are stored in a structure below.
 * These are the index's into the structure for the various
 * command line options.
 */
#define CFG_ADAPTER_ID		0
#define CFG_MAX_SPEED		1
#define CFG_DEV_MODE		2
#define CFG_ADAPTER_MODE	3
#define CFG_TAGS		4
#define CFG_RESET_DELAY		5

#define CFG_NUM			6	/* number of configuration items */


/*
 * Value used to indicate that a command line override
 * hasn't been used to modify the value.
 */
#define CFG_PARAM_UNSET -1


/*
 * Hold command line parameters.
 */
struct ParameterData {
	int value;		/* value of this setting */
	int min;		/* minimum value */
	int max;		/* maximum value */
	int def;		/* default value */
	int safe;		/* safe value */
};
static struct ParameterData __devinitdata cfg_data[] = {
	{ /* adapter id */
		CFG_PARAM_UNSET,
		0,
		15,
		7,
		7
	},
	{ /* max speed */
		CFG_PARAM_UNSET,
		  0,
		  7,
		  1,	/* 13.3Mhz */
		  4,	/*  6.7Hmz */
	},
	{ /* dev mode */
		CFG_PARAM_UNSET,
		0,
		0x3f,
		NTC_DO_PARITY_CHK | NTC_DO_DISCONNECT | NTC_DO_SYNC_NEGO |
			NTC_DO_WIDE_NEGO | NTC_DO_TAG_QUEUEING |
			NTC_DO_SEND_START,
		NTC_DO_PARITY_CHK | NTC_DO_SEND_START
	},
	{ /* adapter mode */
		CFG_PARAM_UNSET,
		0,
		0x2f,
#ifdef CONFIG_SCSI_MULTI_LUN
			NAC_SCANLUN |
#endif
		NAC_GT2DRIVES | NAC_GREATER_1G | NAC_POWERON_SCSI_RESET
			/*| NAC_ACTIVE_NEG*/,
		NAC_GT2DRIVES | NAC_GREATER_1G | NAC_POWERON_SCSI_RESET | 0x08
	},
	{ /* tags */
		CFG_PARAM_UNSET,
		0,
		5,
		3,	/* 16 tags (??) */
		2,
	},
	{ /* reset delay */
		CFG_PARAM_UNSET,
		0,
		180,
		1,	/* 1 second */
		10,	/* 10 seconds */
	}
};


/*
 * Safe settings. If set to zero the BIOS/default values with
 * command line overrides will be used. If set to 1 then safe and
 * slow settings will be used.
 */
static int use_safe_settings = 0;
module_param_named(safe, use_safe_settings, bool, 0);
MODULE_PARM_DESC(safe, "Use safe and slow settings only. Default: false");


module_param_named(adapter_id, cfg_data[CFG_ADAPTER_ID].value, int, 0);
MODULE_PARM_DESC(adapter_id, "Adapter SCSI ID. Default 7 (0-15)");

module_param_named(max_speed, cfg_data[CFG_MAX_SPEED].value, int, 0);
MODULE_PARM_DESC(max_speed, "Maximum bus speed. Default 1 (0-7) Speeds: 0=20, 1=13.3, 2=10, 3=8, 4=6.7, 5=5.8, 6=5, 7=4 Mhz");

module_param_named(dev_mode, cfg_data[CFG_DEV_MODE].value, int, 0);
MODULE_PARM_DESC(dev_mode, "Device mode.");

module_param_named(adapter_mode, cfg_data[CFG_ADAPTER_MODE].value, int, 0);
MODULE_PARM_DESC(adapter_mode, "Adapter mode.");

module_param_named(tags, cfg_data[CFG_TAGS].value, int, 0);
MODULE_PARM_DESC(tags, "Number of tags (1<<x). Default 3 (0-5)");

module_param_named(reset_delay, cfg_data[CFG_RESET_DELAY].value, int, 0);
MODULE_PARM_DESC(reset_delay, "Reset delay in seconds. Default 1 (0-180)");


/**
 * set_safe_settings - if the use_safe_settings option is set then
 * set all values to the safe and slow values.
 **/
static void __devinit set_safe_settings(void)
{
	if (use_safe_settings)
	{
		int i;

		dprintkl(KERN_INFO, "Using safe settings.\n");
		for (i = 0; i < CFG_NUM; i++)
		{
			cfg_data[i].value = cfg_data[i].safe;
		}
	}
}


/**
 * fix_settings - reset any boot parameters which are out of range
 * back to the default values.
 **/
static void __devinit fix_settings(void)
{
	int i;

	dprintkdbg(DBG_1,
		"setup: AdapterId=%08x MaxSpeed=%08x DevMode=%08x "
		"AdapterMode=%08x Tags=%08x ResetDelay=%08x\n",
		cfg_data[CFG_ADAPTER_ID].value,
		cfg_data[CFG_MAX_SPEED].value,
		cfg_data[CFG_DEV_MODE].value,
		cfg_data[CFG_ADAPTER_MODE].value,
		cfg_data[CFG_TAGS].value,
		cfg_data[CFG_RESET_DELAY].value);
	for (i = 0; i < CFG_NUM; i++)
	{
		if (cfg_data[i].value < cfg_data[i].min
		    || cfg_data[i].value > cfg_data[i].max)
			cfg_data[i].value = cfg_data[i].def;
	}
}



/*
 * Mapping from the eeprom delay index value (index into this array)
 * to the number of actual seconds that the delay should be for.
 */
static char __devinitdata eeprom_index_to_delay_map[] = 
	{ 1, 3, 5, 10, 16, 30, 60, 120 };


/**
 * eeprom_index_to_delay - Take the eeprom delay setting and convert it
 * into a number of seconds.
 *
 * @eeprom: The eeprom structure in which we find the delay index to map.
 **/
static void __devinit eeprom_index_to_delay(struct NvRamType *eeprom)
{
	eeprom->delay_time = eeprom_index_to_delay_map[eeprom->delay_time];
}


/**
 * delay_to_eeprom_index - Take a delay in seconds and return the
 * closest eeprom index which will delay for at least that amount of
 * seconds.
 *
 * @delay: The delay, in seconds, to find the eeprom index for.
 **/
static int __devinit delay_to_eeprom_index(int delay)
{
	u8 idx = 0;
	while (idx < 7 && eeprom_index_to_delay_map[idx] < delay)
		idx++;
	return idx;
}


/**
 * eeprom_override - Override the eeprom settings, in the provided
 * eeprom structure, with values that have been set on the command
 * line.
 *
 * @eeprom: The eeprom data to override with command line options.
 **/
static void __devinit eeprom_override(struct NvRamType *eeprom)
{
	u8 id;

	/* Adapter Settings */
	if (cfg_data[CFG_ADAPTER_ID].value != CFG_PARAM_UNSET)
		eeprom->scsi_id = (u8)cfg_data[CFG_ADAPTER_ID].value;

	if (cfg_data[CFG_ADAPTER_MODE].value != CFG_PARAM_UNSET)
		eeprom->channel_cfg = (u8)cfg_data[CFG_ADAPTER_MODE].value;

	if (cfg_data[CFG_RESET_DELAY].value != CFG_PARAM_UNSET)
		eeprom->delay_time = delay_to_eeprom_index(
					cfg_data[CFG_RESET_DELAY].value);

	if (cfg_data[CFG_TAGS].value != CFG_PARAM_UNSET)
		eeprom->max_tag = (u8)cfg_data[CFG_TAGS].value;

	/* Device Settings */
	for (id = 0; id < DC395x_MAX_SCSI_ID; id++) {
		if (cfg_data[CFG_DEV_MODE].value != CFG_PARAM_UNSET)
			eeprom->target[id].cfg0 =
				(u8)cfg_data[CFG_DEV_MODE].value;

		if (cfg_data[CFG_MAX_SPEED].value != CFG_PARAM_UNSET)
			eeprom->target[id].period =
				(u8)cfg_data[CFG_MAX_SPEED].value;

	}
}


/*---------------------------------------------------------------------------
 ---------------------------------------------------------------------------*/

static unsigned int list_size(struct list_head *head)
{
	unsigned int count = 0;
	struct list_head *pos;
	list_for_each(pos, head)
		count++;
	return count;
}


static struct DeviceCtlBlk *dcb_get_next(struct list_head *head,
		struct DeviceCtlBlk *pos)
{
	int use_next = 0;
	struct DeviceCtlBlk* next = NULL;
	struct DeviceCtlBlk* i;

	if (list_empty(head))
		return NULL;

	/* find supplied dcb and then select the next one */
	list_for_each_entry(i, head, list)
		if (use_next) {
			next = i;
			break;
		} else if (i == pos) {
			use_next = 1;
		}
	/* if no next one take the head one (ie, wraparound) */
	if (!next)
        	list_for_each_entry(i, head, list) {
        		next = i;
        		break;
        	}

	return next;
}


static void free_tag(struct DeviceCtlBlk *dcb, struct ScsiReqBlk *srb)
{
	if (srb->tag_number < 255) {
		dcb->tag_mask &= ~(1 << srb->tag_number);	/* free tag mask */
		srb->tag_number = 255;
	}
}


/* Find cmd in SRB list */
static inline struct ScsiReqBlk *find_cmd(struct scsi_cmnd *cmd,
		struct list_head *head)
{
	struct ScsiReqBlk *i;
	list_for_each_entry(i, head, list)
		if (i->cmd == cmd)
			return i;
	return NULL;
}


static struct ScsiReqBlk *srb_get_free(struct AdapterCtlBlk *acb)
{
	struct list_head *head = &acb->srb_free_list;
	struct ScsiReqBlk *srb = NULL;

	if (!list_empty(head)) {
		srb = list_entry(head->next, struct ScsiReqBlk, list);
		list_del(head->next);
		dprintkdbg(DBG_0, "srb_get_free: srb=%p\n", srb);
	}
	return srb;
}


static void srb_free_insert(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb)
{
	dprintkdbg(DBG_0, "srb_free_insert: srb=%p\n", srb);
	list_add_tail(&srb->list, &acb->srb_free_list);
}


static void srb_waiting_insert(struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	dprintkdbg(DBG_0, "srb_waiting_insert: (pid#%li) <%02i-%i> srb=%p\n",
		srb->cmd->serial_number, dcb->target_id, dcb->target_lun, srb);
	list_add(&srb->list, &dcb->srb_waiting_list);
}


static void srb_waiting_append(struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	dprintkdbg(DBG_0, "srb_waiting_append: (pid#%li) <%02i-%i> srb=%p\n",
		 srb->cmd->serial_number, dcb->target_id, dcb->target_lun, srb);
	list_add_tail(&srb->list, &dcb->srb_waiting_list);
}


static void srb_going_append(struct DeviceCtlBlk *dcb, struct ScsiReqBlk *srb)
{
	dprintkdbg(DBG_0, "srb_going_append: (pid#%li) <%02i-%i> srb=%p\n",
		srb->cmd->serial_number, dcb->target_id, dcb->target_lun, srb);
	list_add_tail(&srb->list, &dcb->srb_going_list);
}


static void srb_going_remove(struct DeviceCtlBlk *dcb, struct ScsiReqBlk *srb)
{
	struct ScsiReqBlk *i;
	struct ScsiReqBlk *tmp;
	dprintkdbg(DBG_0, "srb_going_remove: (pid#%li) <%02i-%i> srb=%p\n",
		srb->cmd->serial_number, dcb->target_id, dcb->target_lun, srb);

	list_for_each_entry_safe(i, tmp, &dcb->srb_going_list, list)
		if (i == srb) {
			list_del(&srb->list);
			break;
		}
}


static void srb_waiting_remove(struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	struct ScsiReqBlk *i;
	struct ScsiReqBlk *tmp;
	dprintkdbg(DBG_0, "srb_waiting_remove: (pid#%li) <%02i-%i> srb=%p\n",
		srb->cmd->serial_number, dcb->target_id, dcb->target_lun, srb);

	list_for_each_entry_safe(i, tmp, &dcb->srb_waiting_list, list)
		if (i == srb) {
			list_del(&srb->list);
			break;
		}
}


static void srb_going_to_waiting_move(struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	dprintkdbg(DBG_0,
		"srb_going_to_waiting_move: (pid#%li) <%02i-%i> srb=%p\n",
		srb->cmd->serial_number, dcb->target_id, dcb->target_lun, srb);
	list_move(&srb->list, &dcb->srb_waiting_list);
}


static void srb_waiting_to_going_move(struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	dprintkdbg(DBG_0,
		"srb_waiting_to_going_move: (pid#%li) <%02i-%i> srb=%p\n",
		srb->cmd->serial_number, dcb->target_id, dcb->target_lun, srb);
	list_move(&srb->list, &dcb->srb_going_list);
}


/* Sets the timer to wake us up */
static void waiting_set_timer(struct AdapterCtlBlk *acb, unsigned long to)
{
	if (timer_pending(&acb->waiting_timer))
		return;
	init_timer(&acb->waiting_timer);
	acb->waiting_timer.function = waiting_timeout;
	acb->waiting_timer.data = (unsigned long) acb;
	if (time_before(jiffies + to, acb->scsi_host->last_reset - HZ / 2))
		acb->waiting_timer.expires =
		    acb->scsi_host->last_reset - HZ / 2 + 1;
	else
		acb->waiting_timer.expires = jiffies + to + 1;
	add_timer(&acb->waiting_timer);
}


/* Send the next command from the waiting list to the bus */
static void waiting_process_next(struct AdapterCtlBlk *acb)
{
	struct DeviceCtlBlk *start = NULL;
	struct DeviceCtlBlk *pos;
	struct DeviceCtlBlk *dcb;
	struct ScsiReqBlk *srb;
	struct list_head *dcb_list_head = &acb->dcb_list;

	if (acb->active_dcb
	    || (acb->acb_flag & (RESET_DETECT + RESET_DONE + RESET_DEV)))
		return;

	if (timer_pending(&acb->waiting_timer))
		del_timer(&acb->waiting_timer);

	if (list_empty(dcb_list_head))
		return;

	/*
	 * Find the starting dcb. Need to find it again in the list
	 * since the list may have changed since we set the ptr to it
	 */
	list_for_each_entry(dcb, dcb_list_head, list)
		if (dcb == acb->dcb_run_robin) {
			start = dcb;
			break;
		}
	if (!start) {
		/* This can happen! */
		start = list_entry(dcb_list_head->next, typeof(*start), list);
		acb->dcb_run_robin = start;
	}


	/*
	 * Loop over the dcb, but we start somewhere (potentially) in
	 * the middle of the loop so we need to manully do this.
	 */
	pos = start;
	do {
		struct list_head *waiting_list_head = &pos->srb_waiting_list;

		/* Make sure, the next another device gets scheduled ... */
		acb->dcb_run_robin = dcb_get_next(dcb_list_head,
						  acb->dcb_run_robin);

		if (list_empty(waiting_list_head) ||
		    pos->max_command <= list_size(&pos->srb_going_list)) {
			/* move to next dcb */
			pos = dcb_get_next(dcb_list_head, pos);
		} else {
			srb = list_entry(waiting_list_head->next,
					 struct ScsiReqBlk, list);

			/* Try to send to the bus */
			if (!start_scsi(acb, pos, srb))
				srb_waiting_to_going_move(pos, srb);
			else
				waiting_set_timer(acb, HZ/50);
			break;
		}
	} while (pos != start);
}


/* Wake up waiting queue */
static void waiting_timeout(unsigned long ptr)
{
	unsigned long flags;
	struct AdapterCtlBlk *acb = (struct AdapterCtlBlk *)ptr;
	dprintkdbg(DBG_1,
		"waiting_timeout: Queue woken up by timer. acb=%p\n", acb);
	DC395x_LOCK_IO(acb->scsi_host, flags);
	waiting_process_next(acb);
	DC395x_UNLOCK_IO(acb->scsi_host, flags);
}


/* Get the DCB for a given ID/LUN combination */
static struct DeviceCtlBlk *find_dcb(struct AdapterCtlBlk *acb, u8 id, u8 lun)
{
	return acb->children[id][lun];
}


/* Send SCSI Request Block (srb) to adapter (acb) */
static void send_srb(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb)
{
	struct DeviceCtlBlk *dcb = srb->dcb;

	if (dcb->max_command <= list_size(&dcb->srb_going_list) ||
	    acb->active_dcb ||
	    (acb->acb_flag & (RESET_DETECT + RESET_DONE + RESET_DEV))) {
		srb_waiting_append(dcb, srb);
		waiting_process_next(acb);
		return;
	}

	if (!start_scsi(acb, dcb, srb))
		srb_going_append(dcb, srb);
	else {
		srb_waiting_insert(dcb, srb);
		waiting_set_timer(acb, HZ / 50);
	}
}

/* Prepare SRB for being sent to Device DCB w/ command *cmd */
static void build_srb(struct scsi_cmnd *cmd, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	int nseg;
	enum dma_data_direction dir = cmd->sc_data_direction;
	dprintkdbg(DBG_0, "build_srb: (pid#%li) <%02i-%i>\n",
		cmd->serial_number, dcb->target_id, dcb->target_lun);

	srb->dcb = dcb;
	srb->cmd = cmd;
	srb->sg_count = 0;
	srb->total_xfer_length = 0;
	srb->sg_bus_addr = 0;
	srb->sg_index = 0;
	srb->adapter_status = 0;
	srb->target_status = 0;
	srb->msg_count = 0;
	srb->status = 0;
	srb->flag = 0;
	srb->state = 0;
	srb->retry_count = 0;
	srb->tag_number = TAG_NONE;
	srb->scsi_phase = PH_BUS_FREE;	/* initial phase */
	srb->end_message = 0;

	nseg = scsi_dma_map(cmd);
	BUG_ON(nseg < 0);

	if (dir == PCI_DMA_NONE || !nseg) {
		dprintkdbg(DBG_0,
			"build_srb: [0] len=%d buf=%p use_sg=%d !MAP=%08x\n",
			   cmd->bufflen, scsi_sglist(cmd), scsi_sg_count(cmd),
			   srb->segment_x[0].address);
	} else {
		int i;
		u32 reqlen = scsi_bufflen(cmd);
		struct scatterlist *sg;
		struct SGentry *sgp = srb->segment_x;

		srb->sg_count = nseg;

		dprintkdbg(DBG_0,
			   "build_srb: [n] len=%d buf=%p use_sg=%d segs=%d\n",
			   reqlen, scsi_sglist(cmd), scsi_sg_count(cmd),
			   srb->sg_count);

		scsi_for_each_sg(cmd, sg, srb->sg_count, i) {
			u32 busaddr = (u32)sg_dma_address(sg);
			u32 seglen = (u32)sg->length;
			sgp[i].address = busaddr;
			sgp[i].length = seglen;
			srb->total_xfer_length += seglen;
		}
		sgp += srb->sg_count - 1;

		/*
		 * adjust last page if too big as it is allocated
		 * on even page boundaries
		 */
		if (srb->total_xfer_length > reqlen) {
			sgp->length -= (srb->total_xfer_length - reqlen);
			srb->total_xfer_length = reqlen;
		}

		/* Fixup for WIDE padding - make sure length is even */
		if (dcb->sync_period & WIDE_SYNC &&
		    srb->total_xfer_length % 2) {
			srb->total_xfer_length++;
			sgp->length++;
		}

		srb->sg_bus_addr = pci_map_single(dcb->acb->dev,
						srb->segment_x,
				            	SEGMENTX_LEN,
				            	PCI_DMA_TODEVICE);

		dprintkdbg(DBG_SG, "build_srb: [n] map sg %p->%08x(%05x)\n",
			srb->segment_x, srb->sg_bus_addr, SEGMENTX_LEN);
	}

	srb->request_length = srb->total_xfer_length;
}


/**
 * dc395x_queue_command - queue scsi command passed from the mid
 * layer, invoke 'done' on completion
 *
 * @cmd: pointer to scsi command object
 * @done: function pointer to be invoked on completion
 *
 * Returns 1 if the adapter (host) is busy, else returns 0. One
 * reason for an adapter to be busy is that the number
 * of outstanding queued commands is already equal to
 * struct Scsi_Host::can_queue .
 *
 * Required: if struct Scsi_Host::can_queue is ever non-zero
 *           then this function is required.
 *
 * Locks: struct Scsi_Host::host_lock held on entry (with "irqsave")
 *        and is expected to be held on return.
 *
 **/
static int dc395x_queue_command(struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *))
{
	struct DeviceCtlBlk *dcb;
	struct ScsiReqBlk *srb;
	struct AdapterCtlBlk *acb =
	    (struct AdapterCtlBlk *)cmd->device->host->hostdata;
	dprintkdbg(DBG_0, "queue_command: (pid#%li) <%02i-%i> cmnd=0x%02x\n",
		cmd->serial_number, cmd->device->id, cmd->device->lun, cmd->cmnd[0]);

	/* Assume BAD_TARGET; will be cleared later */
	cmd->result = DID_BAD_TARGET << 16;

	/* ignore invalid targets */
	if (cmd->device->id >= acb->scsi_host->max_id ||
	    cmd->device->lun >= acb->scsi_host->max_lun ||
	    cmd->device->lun >31) {
		goto complete;
	}

	/* does the specified lun on the specified device exist */
	if (!(acb->dcb_map[cmd->device->id] & (1 << cmd->device->lun))) {
		dprintkl(KERN_INFO, "queue_command: Ignore target <%02i-%i>\n",
			cmd->device->id, cmd->device->lun);
		goto complete;
	}

	/* do we have a DCB for the device */
	dcb = find_dcb(acb, cmd->device->id, cmd->device->lun);
	if (!dcb) {
		/* should never happen */
		dprintkl(KERN_ERR, "queue_command: No such device <%02i-%i>",
			cmd->device->id, cmd->device->lun);
		goto complete;
	}

	/* set callback and clear result in the command */
	cmd->scsi_done = done;
	cmd->result = 0;

	srb = srb_get_free(acb);
	if (!srb)
	{
		/*
		 * Return 1 since we are unable to queue this command at this
		 * point in time.
		 */
		dprintkdbg(DBG_0, "queue_command: No free srb's\n");
		return 1;
	}

	build_srb(cmd, dcb, srb);

	if (!list_empty(&dcb->srb_waiting_list)) {
		/* append to waiting queue */
		srb_waiting_append(dcb, srb);
		waiting_process_next(acb);
	} else {
		/* process immediately */
		send_srb(acb, srb);
	}
	dprintkdbg(DBG_1, "queue_command: (pid#%li) done\n", cmd->serial_number);
	return 0;

complete:
	/*
	 * Complete the command immediatey, and then return 0 to
	 * indicate that we have handled the command. This is usually
	 * done when the commad is for things like non existent
	 * devices.
	 */
	done(cmd);
	return 0;
}


/*
 * Return the disk geometry for the given SCSI device.
 */
static int dc395x_bios_param(struct scsi_device *sdev,
		struct block_device *bdev, sector_t capacity, int *info)
{
#ifdef CONFIG_SCSI_DC395x_TRMS1040_TRADMAP
	int heads, sectors, cylinders;
	struct AdapterCtlBlk *acb;
	int size = capacity;

	dprintkdbg(DBG_0, "dc395x_bios_param..............\n");
	acb = (struct AdapterCtlBlk *)sdev->host->hostdata;
	heads = 64;
	sectors = 32;
	cylinders = size / (heads * sectors);

	if ((acb->gmode2 & NAC_GREATER_1G) && (cylinders > 1024)) {
		heads = 255;
		sectors = 63;
		cylinders = size / (heads * sectors);
	}
	geom[0] = heads;
	geom[1] = sectors;
	geom[2] = cylinders;
	return 0;
#else
	return scsicam_bios_param(bdev, capacity, info);
#endif
}


static void dump_register_info(struct AdapterCtlBlk *acb,
		struct DeviceCtlBlk *dcb, struct ScsiReqBlk *srb)
{
	u16 pstat;
	struct pci_dev *dev = acb->dev;
	pci_read_config_word(dev, PCI_STATUS, &pstat);
	if (!dcb)
		dcb = acb->active_dcb;
	if (!srb && dcb)
		srb = dcb->active_srb;
	if (srb) {
		if (!srb->cmd)
			dprintkl(KERN_INFO, "dump: srb=%p cmd=%p OOOPS!\n",
				srb, srb->cmd);
		else
			dprintkl(KERN_INFO, "dump: srb=%p cmd=%p (pid#%li) "
				 "cmnd=0x%02x <%02i-%i>\n",
				srb, srb->cmd, srb->cmd->serial_number,
				srb->cmd->cmnd[0], srb->cmd->device->id,
			       	srb->cmd->device->lun);
		printk("  sglist=%p cnt=%i idx=%i len=%zu\n",
		       srb->segment_x, srb->sg_count, srb->sg_index,
		       srb->total_xfer_length);
		printk("  state=0x%04x status=0x%02x phase=0x%02x (%sconn.)\n",
		       srb->state, srb->status, srb->scsi_phase,
		       (acb->active_dcb) ? "" : "not");
	}
	dprintkl(KERN_INFO, "dump: SCSI{status=0x%04x fifocnt=0x%02x "
		"signals=0x%02x irqstat=0x%02x sync=0x%02x target=0x%02x "
		"rselid=0x%02x ctr=0x%08x irqen=0x%02x config=0x%04x "
		"config2=0x%02x cmd=0x%02x selto=0x%02x}\n",
		DC395x_read16(acb, TRM_S1040_SCSI_STATUS),
		DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT),
		DC395x_read8(acb, TRM_S1040_SCSI_SIGNAL),
		DC395x_read8(acb, TRM_S1040_SCSI_INTSTATUS),
		DC395x_read8(acb, TRM_S1040_SCSI_SYNC),
		DC395x_read8(acb, TRM_S1040_SCSI_TARGETID),
		DC395x_read8(acb, TRM_S1040_SCSI_IDMSG),
		DC395x_read32(acb, TRM_S1040_SCSI_COUNTER),
		DC395x_read8(acb, TRM_S1040_SCSI_INTEN),
		DC395x_read16(acb, TRM_S1040_SCSI_CONFIG0),
		DC395x_read8(acb, TRM_S1040_SCSI_CONFIG2),
		DC395x_read8(acb, TRM_S1040_SCSI_COMMAND),
		DC395x_read8(acb, TRM_S1040_SCSI_TIMEOUT));
	dprintkl(KERN_INFO, "dump: DMA{cmd=0x%04x fifocnt=0x%02x fstat=0x%02x "
		"irqstat=0x%02x irqen=0x%02x cfg=0x%04x tctr=0x%08x "
		"ctctr=0x%08x addr=0x%08x:0x%08x}\n",
		DC395x_read16(acb, TRM_S1040_DMA_COMMAND),
		DC395x_read8(acb, TRM_S1040_DMA_FIFOCNT),
		DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT),
		DC395x_read8(acb, TRM_S1040_DMA_STATUS),
		DC395x_read8(acb, TRM_S1040_DMA_INTEN),
		DC395x_read16(acb, TRM_S1040_DMA_CONFIG),
		DC395x_read32(acb, TRM_S1040_DMA_XCNT),
		DC395x_read32(acb, TRM_S1040_DMA_CXCNT),
		DC395x_read32(acb, TRM_S1040_DMA_XHIGHADDR),
		DC395x_read32(acb, TRM_S1040_DMA_XLOWADDR));
	dprintkl(KERN_INFO, "dump: gen{gctrl=0x%02x gstat=0x%02x gtmr=0x%02x} "
		"pci{status=0x%04x}\n",
		DC395x_read8(acb, TRM_S1040_GEN_CONTROL),
		DC395x_read8(acb, TRM_S1040_GEN_STATUS),
		DC395x_read8(acb, TRM_S1040_GEN_TIMER),
		pstat);
}


static inline void clear_fifo(struct AdapterCtlBlk *acb, char *txt)
{
#if debug_enabled(DBG_FIFO)
	u8 lines = DC395x_read8(acb, TRM_S1040_SCSI_SIGNAL);
	u8 fifocnt = DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT);
	if (!(fifocnt & 0x40))
		dprintkdbg(DBG_FIFO,
			"clear_fifo: (%i bytes) on phase %02x in %s\n",
			fifocnt & 0x3f, lines, txt);
#endif
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_CLRFIFO);
}


static void reset_dev_param(struct AdapterCtlBlk *acb)
{
	struct DeviceCtlBlk *dcb;
	struct NvRamType *eeprom = &acb->eeprom;
	dprintkdbg(DBG_0, "reset_dev_param: acb=%p\n", acb);

	list_for_each_entry(dcb, &acb->dcb_list, list) {
		u8 period_index;

		dcb->sync_mode &= ~(SYNC_NEGO_DONE + WIDE_NEGO_DONE);
		dcb->sync_period = 0;
		dcb->sync_offset = 0;

		dcb->dev_mode = eeprom->target[dcb->target_id].cfg0;
		period_index = eeprom->target[dcb->target_id].period & 0x07;
		dcb->min_nego_period = clock_period[period_index];
		if (!(dcb->dev_mode & NTC_DO_WIDE_NEGO)
		    || !(acb->config & HCC_WIDE_CARD))
			dcb->sync_mode &= ~WIDE_NEGO_ENABLE;
	}
}


/*
 * perform a hard reset on the SCSI bus
 * @cmd - some command for this host (for fetching hooks)
 * Returns: SUCCESS (0x2002) on success, else FAILED (0x2003).
 */
static int __dc395x_eh_bus_reset(struct scsi_cmnd *cmd)
{
	struct AdapterCtlBlk *acb =
		(struct AdapterCtlBlk *)cmd->device->host->hostdata;
	dprintkl(KERN_INFO,
		"eh_bus_reset: (pid#%li) target=<%02i-%i> cmd=%p\n",
		cmd->serial_number, cmd->device->id, cmd->device->lun, cmd);

	if (timer_pending(&acb->waiting_timer))
		del_timer(&acb->waiting_timer);

	/*
	 * disable interrupt    
	 */
	DC395x_write8(acb, TRM_S1040_DMA_INTEN, 0x00);
	DC395x_write8(acb, TRM_S1040_SCSI_INTEN, 0x00);
	DC395x_write8(acb, TRM_S1040_SCSI_CONTROL, DO_RSTMODULE);
	DC395x_write8(acb, TRM_S1040_DMA_CONTROL, DMARESETMODULE);

	reset_scsi_bus(acb);
	udelay(500);

	/* We may be in serious trouble. Wait some seconds */
	acb->scsi_host->last_reset =
	    jiffies + 3 * HZ / 2 +
	    HZ * acb->eeprom.delay_time;

	/*
	 * re-enable interrupt      
	 */
	/* Clear SCSI FIFO          */
	DC395x_write8(acb, TRM_S1040_DMA_CONTROL, CLRXFIFO);
	clear_fifo(acb, "eh_bus_reset");
	/* Delete pending IRQ */
	DC395x_read8(acb, TRM_S1040_SCSI_INTSTATUS);
	set_basic_config(acb);

	reset_dev_param(acb);
	doing_srb_done(acb, DID_RESET, cmd, 0);
	acb->active_dcb = NULL;
	acb->acb_flag = 0;	/* RESET_DETECT, RESET_DONE ,RESET_DEV */
	waiting_process_next(acb);

	return SUCCESS;
}

static int dc395x_eh_bus_reset(struct scsi_cmnd *cmd)
{
	int rc;

	spin_lock_irq(cmd->device->host->host_lock);
	rc = __dc395x_eh_bus_reset(cmd);
	spin_unlock_irq(cmd->device->host->host_lock);

	return rc;
}

/*
 * abort an errant SCSI command
 * @cmd - command to be aborted
 * Returns: SUCCESS (0x2002) on success, else FAILED (0x2003).
 */
static int dc395x_eh_abort(struct scsi_cmnd *cmd)
{
	/*
	 * Look into our command queues: If it has not been sent already,
	 * we remove it and return success. Otherwise fail.
	 */
	struct AdapterCtlBlk *acb =
	    (struct AdapterCtlBlk *)cmd->device->host->hostdata;
	struct DeviceCtlBlk *dcb;
	struct ScsiReqBlk *srb;
	dprintkl(KERN_INFO, "eh_abort: (pid#%li) target=<%02i-%i> cmd=%p\n",
		cmd->serial_number, cmd->device->id, cmd->device->lun, cmd);

	dcb = find_dcb(acb, cmd->device->id, cmd->device->lun);
	if (!dcb) {
		dprintkl(KERN_DEBUG, "eh_abort: No such device\n");
		return FAILED;
	}

	srb = find_cmd(cmd, &dcb->srb_waiting_list);
	if (srb) {
		srb_waiting_remove(dcb, srb);
		pci_unmap_srb_sense(acb, srb);
		pci_unmap_srb(acb, srb);
		free_tag(dcb, srb);
		srb_free_insert(acb, srb);
		dprintkl(KERN_DEBUG, "eh_abort: Command was waiting\n");
		cmd->result = DID_ABORT << 16;
		return SUCCESS;
	}
	srb = find_cmd(cmd, &dcb->srb_going_list);
	if (srb) {
		dprintkl(KERN_DEBUG, "eh_abort: Command in progress\n");
		/* XXX: Should abort the command here */
	} else {
		dprintkl(KERN_DEBUG, "eh_abort: Command not found\n");
	}
	return FAILED;
}


/* SDTR */
static void build_sdtr(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	u8 *ptr = srb->msgout_buf + srb->msg_count;
	if (srb->msg_count > 1) {
		dprintkl(KERN_INFO,
			"build_sdtr: msgout_buf BUSY (%i: %02x %02x)\n",
			srb->msg_count, srb->msgout_buf[0],
			srb->msgout_buf[1]);
		return;
	}
	if (!(dcb->dev_mode & NTC_DO_SYNC_NEGO)) {
		dcb->sync_offset = 0;
		dcb->min_nego_period = 200 >> 2;
	} else if (dcb->sync_offset == 0)
		dcb->sync_offset = SYNC_NEGO_OFFSET;

	*ptr++ = MSG_EXTENDED;	/* (01h) */
	*ptr++ = 3;		/* length */
	*ptr++ = EXTENDED_SDTR;	/* (01h) */
	*ptr++ = dcb->min_nego_period;	/* Transfer period (in 4ns) */
	*ptr++ = dcb->sync_offset;	/* Transfer period (max. REQ/ACK dist) */
	srb->msg_count += 5;
	srb->state |= SRB_DO_SYNC_NEGO;
}


/* WDTR */
static void build_wdtr(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	u8 wide = ((dcb->dev_mode & NTC_DO_WIDE_NEGO) &
		   (acb->config & HCC_WIDE_CARD)) ? 1 : 0;
	u8 *ptr = srb->msgout_buf + srb->msg_count;
	if (srb->msg_count > 1) {
		dprintkl(KERN_INFO,
			"build_wdtr: msgout_buf BUSY (%i: %02x %02x)\n",
			srb->msg_count, srb->msgout_buf[0],
			srb->msgout_buf[1]);
		return;
	}
	*ptr++ = MSG_EXTENDED;	/* (01h) */
	*ptr++ = 2;		/* length */
	*ptr++ = EXTENDED_WDTR;	/* (03h) */
	*ptr++ = wide;
	srb->msg_count += 4;
	srb->state |= SRB_DO_WIDE_NEGO;
}


#if 0
/* Timer to work around chip flaw: When selecting and the bus is 
 * busy, we sometimes miss a Selection timeout IRQ */
void selection_timeout_missed(unsigned long ptr);
/* Sets the timer to wake us up */
static void selto_timer(struct AdapterCtlBlk *acb)
{
	if (timer_pending(&acb->selto_timer))
		return;
	acb->selto_timer.function = selection_timeout_missed;
	acb->selto_timer.data = (unsigned long) acb;
	if (time_before
	    (jiffies + HZ, acb->scsi_host->last_reset + HZ / 2))
		acb->selto_timer.expires =
		    acb->scsi_host->last_reset + HZ / 2 + 1;
	else
		acb->selto_timer.expires = jiffies + HZ + 1;
	add_timer(&acb->selto_timer);
}


void selection_timeout_missed(unsigned long ptr)
{
	unsigned long flags;
	struct AdapterCtlBlk *acb = (struct AdapterCtlBlk *)ptr;
	struct ScsiReqBlk *srb;
	dprintkl(KERN_DEBUG, "Chip forgot to produce SelTO IRQ!\n");
	if (!acb->active_dcb || !acb->active_dcb->active_srb) {
		dprintkl(KERN_DEBUG, "... but no cmd pending? Oops!\n");
		return;
	}
	DC395x_LOCK_IO(acb->scsi_host, flags);
	srb = acb->active_dcb->active_srb;
	disconnect(acb);
	DC395x_UNLOCK_IO(acb->scsi_host, flags);
}
#endif


static u8 start_scsi(struct AdapterCtlBlk* acb, struct DeviceCtlBlk* dcb,
		struct ScsiReqBlk* srb)
{
	u16 s_stat2, return_code;
	u8 s_stat, scsicommand, i, identify_message;
	u8 *ptr;
	dprintkdbg(DBG_0, "start_scsi: (pid#%li) <%02i-%i> srb=%p\n",
		srb->cmd->serial_number, dcb->target_id, dcb->target_lun, srb);

	srb->tag_number = TAG_NONE;	/* acb->tag_max_num: had error read in eeprom */

	s_stat = DC395x_read8(acb, TRM_S1040_SCSI_SIGNAL);
	s_stat2 = 0;
	s_stat2 = DC395x_read16(acb, TRM_S1040_SCSI_STATUS);
#if 1
	if (s_stat & 0x20 /* s_stat2 & 0x02000 */ ) {
		dprintkdbg(DBG_KG, "start_scsi: (pid#%li) BUSY %02x %04x\n",
			srb->cmd->serial_number, s_stat, s_stat2);
		/*
		 * Try anyway?
		 *
		 * We could, BUT: Sometimes the TRM_S1040 misses to produce a Selection
		 * Timeout, a Disconnect or a Reselction IRQ, so we would be screwed!
		 * (This is likely to be a bug in the hardware. Obviously, most people
		 *  only have one initiator per SCSI bus.)
		 * Instead let this fail and have the timer make sure the command is 
		 * tried again after a short time
		 */
		/*selto_timer (acb); */
		return 1;
	}
#endif
	if (acb->active_dcb) {
		dprintkl(KERN_DEBUG, "start_scsi: (pid#%li) Attempt to start a"
			"command while another command (pid#%li) is active.",
			srb->cmd->serial_number,
			acb->active_dcb->active_srb ?
			    acb->active_dcb->active_srb->cmd->serial_number : 0);
		return 1;
	}
	if (DC395x_read16(acb, TRM_S1040_SCSI_STATUS) & SCSIINTERRUPT) {
		dprintkdbg(DBG_KG, "start_scsi: (pid#%li) Failed (busy)\n",
			srb->cmd->serial_number);
		return 1;
	}
	/* Allow starting of SCSI commands half a second before we allow the mid-level
	 * to queue them again after a reset */
	if (time_before(jiffies, acb->scsi_host->last_reset - HZ / 2)) {
		dprintkdbg(DBG_KG, "start_scsi: Refuse cmds (reset wait)\n");
		return 1;
	}

	/* Flush FIFO */
	clear_fifo(acb, "start_scsi");
	DC395x_write8(acb, TRM_S1040_SCSI_HOSTID, acb->scsi_host->this_id);
	DC395x_write8(acb, TRM_S1040_SCSI_TARGETID, dcb->target_id);
	DC395x_write8(acb, TRM_S1040_SCSI_SYNC, dcb->sync_period);
	DC395x_write8(acb, TRM_S1040_SCSI_OFFSET, dcb->sync_offset);
	srb->scsi_phase = PH_BUS_FREE;	/* initial phase */

	identify_message = dcb->identify_msg;
	/*DC395x_TRM_write8(TRM_S1040_SCSI_IDMSG, identify_message); */
	/* Don't allow disconnection for AUTO_REQSENSE: Cont.All.Cond.! */
	if (srb->flag & AUTO_REQSENSE)
		identify_message &= 0xBF;

	if (((srb->cmd->cmnd[0] == INQUIRY)
	     || (srb->cmd->cmnd[0] == REQUEST_SENSE)
	     || (srb->flag & AUTO_REQSENSE))
	    && (((dcb->sync_mode & WIDE_NEGO_ENABLE)
		 && !(dcb->sync_mode & WIDE_NEGO_DONE))
		|| ((dcb->sync_mode & SYNC_NEGO_ENABLE)
		    && !(dcb->sync_mode & SYNC_NEGO_DONE)))
	    && (dcb->target_lun == 0)) {
		srb->msgout_buf[0] = identify_message;
		srb->msg_count = 1;
		scsicommand = SCMD_SEL_ATNSTOP;
		srb->state = SRB_MSGOUT;
#ifndef SYNC_FIRST
		if (dcb->sync_mode & WIDE_NEGO_ENABLE
		    && dcb->inquiry7 & SCSI_INQ_WBUS16) {
			build_wdtr(acb, dcb, srb);
			goto no_cmd;
		}
#endif
		if (dcb->sync_mode & SYNC_NEGO_ENABLE
		    && dcb->inquiry7 & SCSI_INQ_SYNC) {
			build_sdtr(acb, dcb, srb);
			goto no_cmd;
		}
		if (dcb->sync_mode & WIDE_NEGO_ENABLE
		    && dcb->inquiry7 & SCSI_INQ_WBUS16) {
			build_wdtr(acb, dcb, srb);
			goto no_cmd;
		}
		srb->msg_count = 0;
	}
	/* Send identify message */
	DC395x_write8(acb, TRM_S1040_SCSI_FIFO, identify_message);

	scsicommand = SCMD_SEL_ATN;
	srb->state = SRB_START_;
#ifndef DC395x_NO_TAGQ
	if ((dcb->sync_mode & EN_TAG_QUEUEING)
	    && (identify_message & 0xC0)) {
		/* Send Tag message */
		u32 tag_mask = 1;
		u8 tag_number = 0;
		while (tag_mask & dcb->tag_mask
		       && tag_number <= dcb->max_command) {
			tag_mask = tag_mask << 1;
			tag_number++;
		}
		if (tag_number >= dcb->max_command) {
			dprintkl(KERN_WARNING, "start_scsi: (pid#%li) "
				"Out of tags target=<%02i-%i>)\n",
				srb->cmd->serial_number, srb->cmd->device->id,
				srb->cmd->device->lun);
			srb->state = SRB_READY;
			DC395x_write16(acb, TRM_S1040_SCSI_CONTROL,
				       DO_HWRESELECT);
			return 1;
		}
		/* Send Tag id */
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, MSG_SIMPLE_QTAG);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, tag_number);
		dcb->tag_mask |= tag_mask;
		srb->tag_number = tag_number;
		scsicommand = SCMD_SEL_ATN3;
		srb->state = SRB_START_;
	}
#endif
/*polling:*/
	/* Send CDB ..command block ......... */
	dprintkdbg(DBG_KG, "start_scsi: (pid#%li) <%02i-%i> cmnd=0x%02x tag=%i\n",
		srb->cmd->serial_number, srb->cmd->device->id, srb->cmd->device->lun,
		srb->cmd->cmnd[0], srb->tag_number);
	if (srb->flag & AUTO_REQSENSE) {
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, REQUEST_SENSE);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, (dcb->target_lun << 5));
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, SCSI_SENSE_BUFFERSIZE);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
	} else {
		ptr = (u8 *)srb->cmd->cmnd;
		for (i = 0; i < srb->cmd->cmd_len; i++)
			DC395x_write8(acb, TRM_S1040_SCSI_FIFO, *ptr++);
	}
      no_cmd:
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL,
		       DO_HWRESELECT | DO_DATALATCH);
	if (DC395x_read16(acb, TRM_S1040_SCSI_STATUS) & SCSIINTERRUPT) {
		/* 
		 * If start_scsi return 1:
		 * we caught an interrupt (must be reset or reselection ... )
		 * : Let's process it first!
		 */
		dprintkdbg(DBG_0, "start_scsi: (pid#%li) <%02i-%i> Failed - busy\n",
			srb->cmd->serial_number, dcb->target_id, dcb->target_lun);
		srb->state = SRB_READY;
		free_tag(dcb, srb);
		srb->msg_count = 0;
		return_code = 1;
		/* This IRQ should NOT get lost, as we did not acknowledge it */
	} else {
		/* 
		 * If start_scsi returns 0:
		 * we know that the SCSI processor is free
		 */
		srb->scsi_phase = PH_BUS_FREE;	/* initial phase */
		dcb->active_srb = srb;
		acb->active_dcb = dcb;
		return_code = 0;
		/* it's important for atn stop */
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL,
			       DO_DATALATCH | DO_HWRESELECT);
		/* SCSI command */
		DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, scsicommand);
	}
	return return_code;
}


#define DC395x_ENABLE_MSGOUT \
 DC395x_write16 (acb, TRM_S1040_SCSI_CONTROL, DO_SETATN); \
 srb->state |= SRB_MSGOUT


/* abort command */
static inline void enable_msgout_abort(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb)
{
	srb->msgout_buf[0] = ABORT;
	srb->msg_count = 1;
	DC395x_ENABLE_MSGOUT;
	srb->state &= ~SRB_MSGIN;
	srb->state |= SRB_MSGOUT;
}


/**
 * dc395x_handle_interrupt - Handle an interrupt that has been confirmed to
 *                           have been triggered for this card.
 *
 * @acb:	 a pointer to the adpter control block
 * @scsi_status: the status return when we checked the card
 **/
static void dc395x_handle_interrupt(struct AdapterCtlBlk *acb,
		u16 scsi_status)
{
	struct DeviceCtlBlk *dcb;
	struct ScsiReqBlk *srb;
	u16 phase;
	u8 scsi_intstatus;
	unsigned long flags;
	void (*dc395x_statev)(struct AdapterCtlBlk *, struct ScsiReqBlk *, 
			      u16 *);

	DC395x_LOCK_IO(acb->scsi_host, flags);

	/* This acknowledges the IRQ */
	scsi_intstatus = DC395x_read8(acb, TRM_S1040_SCSI_INTSTATUS);
	if ((scsi_status & 0x2007) == 0x2002)
		dprintkl(KERN_DEBUG,
			"COP after COP completed? %04x\n", scsi_status);
	if (debug_enabled(DBG_KG)) {
		if (scsi_intstatus & INT_SELTIMEOUT)
			dprintkdbg(DBG_KG, "handle_interrupt: Selection timeout\n");
	}
	/*dprintkl(KERN_DEBUG, "handle_interrupt: intstatus = 0x%02x ", scsi_intstatus); */

	if (timer_pending(&acb->selto_timer))
		del_timer(&acb->selto_timer);

	if (scsi_intstatus & (INT_SELTIMEOUT | INT_DISCONNECT)) {
		disconnect(acb);	/* bus free interrupt  */
		goto out_unlock;
	}
	if (scsi_intstatus & INT_RESELECTED) {
		reselect(acb);
		goto out_unlock;
	}
	if (scsi_intstatus & INT_SELECT) {
		dprintkl(KERN_INFO, "Host does not support target mode!\n");
		goto out_unlock;
	}
	if (scsi_intstatus & INT_SCSIRESET) {
		scsi_reset_detect(acb);
		goto out_unlock;
	}
	if (scsi_intstatus & (INT_BUSSERVICE | INT_CMDDONE)) {
		dcb = acb->active_dcb;
		if (!dcb) {
			dprintkl(KERN_DEBUG,
				"Oops: BusService (%04x %02x) w/o ActiveDCB!\n",
				scsi_status, scsi_intstatus);
			goto out_unlock;
		}
		srb = dcb->active_srb;
		if (dcb->flag & ABORT_DEV_) {
			dprintkdbg(DBG_0, "MsgOut Abort Device.....\n");
			enable_msgout_abort(acb, srb);
		}

		/* software sequential machine */
		phase = (u16)srb->scsi_phase;

		/* 
		 * 62037 or 62137
		 * call  dc395x_scsi_phase0[]... "phase entry"
		 * handle every phase before start transfer
		 */
		/* data_out_phase0,	phase:0 */
		/* data_in_phase0,	phase:1 */
		/* command_phase0,	phase:2 */
		/* status_phase0,	phase:3 */
		/* nop0,		phase:4 PH_BUS_FREE .. initial phase */
		/* nop0,		phase:5 PH_BUS_FREE .. initial phase */
		/* msgout_phase0,	phase:6 */
		/* msgin_phase0,	phase:7 */
		dc395x_statev = dc395x_scsi_phase0[phase];
		dc395x_statev(acb, srb, &scsi_status);

		/* 
		 * if there were any exception occured scsi_status
		 * will be modify to bus free phase new scsi_status
		 * transfer out from ... previous dc395x_statev
		 */
		srb->scsi_phase = scsi_status & PHASEMASK;
		phase = (u16)scsi_status & PHASEMASK;

		/* 
		 * call  dc395x_scsi_phase1[]... "phase entry" handle
		 * every phase to do transfer
		 */
		/* data_out_phase1,	phase:0 */
		/* data_in_phase1,	phase:1 */
		/* command_phase1,	phase:2 */
		/* status_phase1,	phase:3 */
		/* nop1,		phase:4 PH_BUS_FREE .. initial phase */
		/* nop1,		phase:5 PH_BUS_FREE .. initial phase */
		/* msgout_phase1,	phase:6 */
		/* msgin_phase1,	phase:7 */
		dc395x_statev = dc395x_scsi_phase1[phase];
		dc395x_statev(acb, srb, &scsi_status);
	}
      out_unlock:
	DC395x_UNLOCK_IO(acb->scsi_host, flags);
}


static irqreturn_t dc395x_interrupt(int irq, void *dev_id)
{
	struct AdapterCtlBlk *acb = dev_id;
	u16 scsi_status;
	u8 dma_status;
	irqreturn_t handled = IRQ_NONE;

	/*
	 * Check for pending interrupt
	 */
	scsi_status = DC395x_read16(acb, TRM_S1040_SCSI_STATUS);
	dma_status = DC395x_read8(acb, TRM_S1040_DMA_STATUS);
	if (scsi_status & SCSIINTERRUPT) {
		/* interrupt pending - let's process it! */
		dc395x_handle_interrupt(acb, scsi_status);
		handled = IRQ_HANDLED;
	}
	else if (dma_status & 0x20) {
		/* Error from the DMA engine */
		dprintkl(KERN_INFO, "Interrupt from DMA engine: 0x%02x!\n", dma_status);
#if 0
		dprintkl(KERN_INFO, "This means DMA error! Try to handle ...\n");
		if (acb->active_dcb) {
			acb->active_dcb-> flag |= ABORT_DEV_;
			if (acb->active_dcb->active_srb)
				enable_msgout_abort(acb, acb->active_dcb->active_srb);
		}
		DC395x_write8(acb, TRM_S1040_DMA_CONTROL, ABORTXFER | CLRXFIFO);
#else
		dprintkl(KERN_INFO, "Ignoring DMA error (probably a bad thing) ...\n");
		acb = NULL;
#endif
		handled = IRQ_HANDLED;
	}

	return handled;
}


static void msgout_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "msgout_phase0: (pid#%li)\n", srb->cmd->serial_number);
	if (srb->state & (SRB_UNEXPECT_RESEL + SRB_ABORT_SENT))
		*pscsi_status = PH_BUS_FREE;	/*.. initial phase */

	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	/* it's important for atn stop */
	srb->state &= ~SRB_MSGOUT;
}


static void msgout_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	u16 i;
	u8 *ptr;
	dprintkdbg(DBG_0, "msgout_phase1: (pid#%li)\n", srb->cmd->serial_number);

	clear_fifo(acb, "msgout_phase1");
	if (!(srb->state & SRB_MSGOUT)) {
		srb->state |= SRB_MSGOUT;
		dprintkl(KERN_DEBUG,
			"msgout_phase1: (pid#%li) Phase unexpected\n",
			srb->cmd->serial_number);	/* So what ? */
	}
	if (!srb->msg_count) {
		dprintkdbg(DBG_0, "msgout_phase1: (pid#%li) NOP msg\n",
			srb->cmd->serial_number);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, MSG_NOP);
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	/* it's important for atn stop */
		DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_FIFO_OUT);
		return;
	}
	ptr = (u8 *)srb->msgout_buf;
	for (i = 0; i < srb->msg_count; i++)
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, *ptr++);
	srb->msg_count = 0;
	if (srb->msgout_buf[0] == MSG_ABORT)
		srb->state = SRB_ABORT_SENT;

	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_FIFO_OUT);
}


static void command_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "command_phase0: (pid#%li)\n", srb->cmd->serial_number);
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);
}


static void command_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	struct DeviceCtlBlk *dcb;
	u8 *ptr;
	u16 i;
	dprintkdbg(DBG_0, "command_phase1: (pid#%li)\n", srb->cmd->serial_number);

	clear_fifo(acb, "command_phase1");
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_CLRATN);
	if (!(srb->flag & AUTO_REQSENSE)) {
		ptr = (u8 *)srb->cmd->cmnd;
		for (i = 0; i < srb->cmd->cmd_len; i++) {
			DC395x_write8(acb, TRM_S1040_SCSI_FIFO, *ptr);
			ptr++;
		}
	} else {
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, REQUEST_SENSE);
		dcb = acb->active_dcb;
		/* target id */
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, (dcb->target_lun << 5));
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, SCSI_SENSE_BUFFERSIZE);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
	}
	srb->state |= SRB_COMMAND;
	/* it's important for atn stop */
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);
	/* SCSI command */
	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_FIFO_OUT);
}


/*
 * Verify that the remaining space in the hw sg lists is the same as
 * the count of remaining bytes in srb->total_xfer_length
 */
static void sg_verify_length(struct ScsiReqBlk *srb)
{
	if (debug_enabled(DBG_SG)) {
		unsigned len = 0;
		unsigned idx = srb->sg_index;
		struct SGentry *psge = srb->segment_x + idx;
		for (; idx < srb->sg_count; psge++, idx++)
			len += psge->length;
		if (len != srb->total_xfer_length)
			dprintkdbg(DBG_SG,
			       "Inconsistent SRB S/G lengths (Tot=%i, Count=%i) !!\n",
			       srb->total_xfer_length, len);
	}			       
}


/*
 * Compute the next Scatter Gather list index and adjust its length
 * and address if necessary
 */
static void sg_update_list(struct ScsiReqBlk *srb, u32 left)
{
	u8 idx;
	u32 xferred = srb->total_xfer_length - left; /* bytes transfered */
	struct SGentry *psge = srb->segment_x + srb->sg_index;

	dprintkdbg(DBG_0,
		"sg_update_list: Transfered %i of %i bytes, %i remain\n",
		xferred, srb->total_xfer_length, left);
	if (xferred == 0) {
		/* nothing to update since we did not transfer any data */
		return;
	}

	sg_verify_length(srb);
	srb->total_xfer_length = left;	/* update remaining count */
	for (idx = srb->sg_index; idx < srb->sg_count; idx++) {
		if (xferred >= psge->length) {
			/* Complete SG entries done */
			xferred -= psge->length;
		} else {
			/* Partial SG entry done */
			psge->length -= xferred;
			psge->address += xferred;
			srb->sg_index = idx;
			pci_dma_sync_single_for_device(srb->dcb->
					    acb->dev,
					    srb->sg_bus_addr,
					    SEGMENTX_LEN,
					    PCI_DMA_TODEVICE);
			break;
		}
		psge++;
	}
	sg_verify_length(srb);
}


/*
 * We have transfered a single byte (PIO mode?) and need to update
 * the count of bytes remaining (total_xfer_length) and update the sg
 * entry to either point to next byte in the current sg entry, or of
 * already at the end to point to the start of the next sg entry
 */
static void sg_subtract_one(struct ScsiReqBlk *srb)
{
	sg_update_list(srb, srb->total_xfer_length - 1);
}


/* 
 * cleanup_after_transfer
 * 
 * Makes sure, DMA and SCSI engine are empty, after the transfer has finished
 * KG: Currently called from  StatusPhase1 ()
 * Should probably also be called from other places
 * Best might be to call it in DataXXPhase0, if new phase will differ 
 */
static void cleanup_after_transfer(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb)
{
	/*DC395x_write8 (TRM_S1040_DMA_STATUS, FORCEDMACOMP); */
	if (DC395x_read16(acb, TRM_S1040_DMA_COMMAND) & 0x0001) {	/* read */
		if (!(DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT) & 0x40))
			clear_fifo(acb, "cleanup/in");
		if (!(DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT) & 0x80))
			DC395x_write8(acb, TRM_S1040_DMA_CONTROL, CLRXFIFO);
	} else {		/* write */
		if (!(DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT) & 0x80))
			DC395x_write8(acb, TRM_S1040_DMA_CONTROL, CLRXFIFO);
		if (!(DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT) & 0x40))
			clear_fifo(acb, "cleanup/out");
	}
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);
}


/*
 * Those no of bytes will be transfered w/ PIO through the SCSI FIFO
 * Seems to be needed for unknown reasons; could be a hardware bug :-(
 */
#define DC395x_LASTPIO 4


static void data_out_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	struct DeviceCtlBlk *dcb = srb->dcb;
	u16 scsi_status = *pscsi_status;
	u32 d_left_counter = 0;
	dprintkdbg(DBG_0, "data_out_phase0: (pid#%li) <%02i-%i>\n",
		srb->cmd->serial_number, srb->cmd->device->id, srb->cmd->device->lun);

	/*
	 * KG: We need to drain the buffers before we draw any conclusions!
	 * This means telling the DMA to push the rest into SCSI, telling
	 * SCSI to push the rest to the bus.
	 * However, the device might have been the one to stop us (phase
	 * change), and the data in transit just needs to be accounted so
	 * it can be retransmitted.)
	 */
	/* 
	 * KG: Stop DMA engine pushing more data into the SCSI FIFO
	 * If we need more data, the DMA SG list will be freshly set up, anyway
	 */
	dprintkdbg(DBG_PIO, "data_out_phase0: "
		"DMA{fifocnt=0x%02x fifostat=0x%02x} "
		"SCSI{fifocnt=0x%02x cnt=0x%06x status=0x%04x} total=0x%06x\n",
		DC395x_read8(acb, TRM_S1040_DMA_FIFOCNT),
		DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT),
		DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT),
		DC395x_read32(acb, TRM_S1040_SCSI_COUNTER), scsi_status,
		srb->total_xfer_length);
	DC395x_write8(acb, TRM_S1040_DMA_CONTROL, STOPDMAXFER | CLRXFIFO);

	if (!(srb->state & SRB_XFERPAD)) {
		if (scsi_status & PARITYERROR)
			srb->status |= PARITY_ERROR;

		/*
		 * KG: Right, we can't just rely on the SCSI_COUNTER, because this
		 * is the no of bytes it got from the DMA engine not the no it 
		 * transferred successfully to the device. (And the difference could
		 * be as much as the FIFO size, I guess ...)
		 */
		if (!(scsi_status & SCSIXFERDONE)) {
			/*
			 * when data transfer from DMA FIFO to SCSI FIFO
			 * if there was some data left in SCSI FIFO
			 */
			d_left_counter =
			    (u32)(DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT) &
				  0x1F);
			if (dcb->sync_period & WIDE_SYNC)
				d_left_counter <<= 1;

			dprintkdbg(DBG_KG, "data_out_phase0: FIFO contains %i %s\n"
				"SCSI{fifocnt=0x%02x cnt=0x%08x} "
				"DMA{fifocnt=0x%04x cnt=0x%02x ctr=0x%08x}\n",
				DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT),
				(dcb->sync_period & WIDE_SYNC) ? "words" : "bytes",
				DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT),
				DC395x_read32(acb, TRM_S1040_SCSI_COUNTER),
				DC395x_read8(acb, TRM_S1040_DMA_FIFOCNT),
				DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT),
				DC395x_read32(acb, TRM_S1040_DMA_CXCNT));
		}
		/*
		 * calculate all the residue data that not yet tranfered
		 * SCSI transfer counter + left in SCSI FIFO data
		 *
		 * .....TRM_S1040_SCSI_COUNTER (24bits)
		 * The counter always decrement by one for every SCSI byte transfer.
		 * .....TRM_S1040_SCSI_FIFOCNT ( 5bits)
		 * The counter is SCSI FIFO offset counter (in units of bytes or! words)
		 */
		if (srb->total_xfer_length > DC395x_LASTPIO)
			d_left_counter +=
			    DC395x_read32(acb, TRM_S1040_SCSI_COUNTER);

		/* Is this a good idea? */
		/*clear_fifo(acb, "DOP1"); */
		/* KG: What is this supposed to be useful for? WIDE padding stuff? */
		if (d_left_counter == 1 && dcb->sync_period & WIDE_SYNC
		    && scsi_bufflen(srb->cmd) % 2) {
			d_left_counter = 0;
			dprintkl(KERN_INFO,
				"data_out_phase0: Discard 1 byte (0x%02x)\n",
				scsi_status);
		}
		/*
		 * KG: Oops again. Same thinko as above: The SCSI might have been
		 * faster than the DMA engine, so that it ran out of data.
		 * In that case, we have to do just nothing! 
		 * But: Why the interrupt: No phase change. No XFERCNT_2_ZERO. Or?
		 */
		/*
		 * KG: This is nonsense: We have been WRITING data to the bus
		 * If the SCSI engine has no bytes left, how should the DMA engine?
		 */
		if (d_left_counter == 0) {
			srb->total_xfer_length = 0;
		} else {
			/*
			 * if transfer not yet complete
			 * there were some data residue in SCSI FIFO or
			 * SCSI transfer counter not empty
			 */
			long oldxferred =
			    srb->total_xfer_length - d_left_counter;
			const int diff =
			    (dcb->sync_period & WIDE_SYNC) ? 2 : 1;
			sg_update_list(srb, d_left_counter);
			/* KG: Most ugly hack! Apparently, this works around a chip bug */
			if ((srb->segment_x[srb->sg_index].length ==
			     diff && scsi_sg_count(srb->cmd))
			    || ((oldxferred & ~PAGE_MASK) ==
				(PAGE_SIZE - diff))
			    ) {
				dprintkl(KERN_INFO, "data_out_phase0: "
					"Work around chip bug (%i)?\n", diff);
				d_left_counter =
				    srb->total_xfer_length - diff;
				sg_update_list(srb, d_left_counter);
				/*srb->total_xfer_length -= diff; */
				/*srb->virt_addr += diff; */
				/*if (srb->cmd->use_sg) */
				/*      srb->sg_index++; */
			}
		}
	}
	if ((*pscsi_status & PHASEMASK) != PH_DATA_OUT) {
		cleanup_after_transfer(acb, srb);
	}
}


static void data_out_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "data_out_phase1: (pid#%li) <%02i-%i>\n",
		srb->cmd->serial_number, srb->cmd->device->id, srb->cmd->device->lun);
	clear_fifo(acb, "data_out_phase1");
	/* do prepare before transfer when data out phase */
	data_io_transfer(acb, srb, XFERDATAOUT);
}

static void data_in_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	u16 scsi_status = *pscsi_status;

	dprintkdbg(DBG_0, "data_in_phase0: (pid#%li) <%02i-%i>\n",
		srb->cmd->serial_number, srb->cmd->device->id, srb->cmd->device->lun);

	/*
	 * KG: DataIn is much more tricky than DataOut. When the device is finished
	 * and switches to another phase, the SCSI engine should be finished too.
	 * But: There might still be bytes left in its FIFO to be fetched by the DMA
	 * engine and transferred to memory.
	 * We should wait for the FIFOs to be emptied by that (is there any way to 
	 * enforce this?) and then stop the DMA engine, because it might think, that
	 * there are more bytes to follow. Yes, the device might disconnect prior to
	 * having all bytes transferred! 
	 * Also we should make sure that all data from the DMA engine buffer's really
	 * made its way to the system memory! Some documentation on this would not
	 * seem to be a bad idea, actually.
	 */
	if (!(srb->state & SRB_XFERPAD)) {
		u32 d_left_counter;
		unsigned int sc, fc;

		if (scsi_status & PARITYERROR) {
			dprintkl(KERN_INFO, "data_in_phase0: (pid#%li) "
				"Parity Error\n", srb->cmd->serial_number);
			srb->status |= PARITY_ERROR;
		}
		/*
		 * KG: We should wait for the DMA FIFO to be empty ...
		 * but: it would be better to wait first for the SCSI FIFO and then the
		 * the DMA FIFO to become empty? How do we know, that the device not already
		 * sent data to the FIFO in a MsgIn phase, eg.?
		 */
		if (!(DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT) & 0x80)) {
#if 0
			int ctr = 6000000;
			dprintkl(KERN_DEBUG,
				"DIP0: Wait for DMA FIFO to flush ...\n");
			/*DC395x_write8  (TRM_S1040_DMA_CONTROL, STOPDMAXFER); */
			/*DC395x_write32 (TRM_S1040_SCSI_COUNTER, 7); */
			/*DC395x_write8  (TRM_S1040_SCSI_COMMAND, SCMD_DMA_IN); */
			while (!
			       (DC395x_read16(acb, TRM_S1040_DMA_FIFOSTAT) &
				0x80) && --ctr);
			if (ctr < 6000000 - 1)
				dprintkl(KERN_DEBUG
				       "DIP0: Had to wait for DMA ...\n");
			if (!ctr)
				dprintkl(KERN_ERR,
				       "Deadlock in DIP0 waiting for DMA FIFO empty!!\n");
			/*DC395x_write32 (TRM_S1040_SCSI_COUNTER, 0); */
#endif
			dprintkdbg(DBG_KG, "data_in_phase0: "
				"DMA{fifocnt=0x%02x fifostat=0x%02x}\n",
				DC395x_read8(acb, TRM_S1040_DMA_FIFOCNT),
				DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT));
		}
		/* Now: Check remainig data: The SCSI counters should tell us ... */
		sc = DC395x_read32(acb, TRM_S1040_SCSI_COUNTER);
		fc = DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT);
		d_left_counter = sc + ((fc & 0x1f)
		       << ((srb->dcb->sync_period & WIDE_SYNC) ? 1 :
			   0));
		dprintkdbg(DBG_KG, "data_in_phase0: "
			"SCSI{fifocnt=0x%02x%s ctr=0x%08x} "
			"DMA{fifocnt=0x%02x fifostat=0x%02x ctr=0x%08x} "
			"Remain{totxfer=%i scsi_fifo+ctr=%i}\n",
			fc,
			(srb->dcb->sync_period & WIDE_SYNC) ? "words" : "bytes",
			sc,
			fc,
			DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT),
			DC395x_read32(acb, TRM_S1040_DMA_CXCNT),
			srb->total_xfer_length, d_left_counter);
#if DC395x_LASTPIO
		/* KG: Less than or equal to 4 bytes can not be transfered via DMA, it seems. */
		if (d_left_counter
		    && srb->total_xfer_length <= DC395x_LASTPIO) {
			size_t left_io = srb->total_xfer_length;

			/*u32 addr = (srb->segment_x[srb->sg_index].address); */
			/*sg_update_list (srb, d_left_counter); */
			dprintkdbg(DBG_PIO, "data_in_phase0: PIO (%i %s) "
				   "for remaining %i bytes:",
				fc & 0x1f,
				(srb->dcb->sync_period & WIDE_SYNC) ?
				    "words" : "bytes",
				srb->total_xfer_length);
			if (srb->dcb->sync_period & WIDE_SYNC)
				DC395x_write8(acb, TRM_S1040_SCSI_CONFIG2,
					      CFG2_WIDEFIFO);
			while (left_io) {
				unsigned char *virt, *base = NULL;
				unsigned long flags = 0;
				size_t len = left_io;
				size_t offset = srb->request_length - left_io;

				local_irq_save(flags);
				/* Assumption: it's inside one page as it's at most 4 bytes and
				   I just assume it's on a 4-byte boundary */
				base = scsi_kmap_atomic_sg(scsi_sglist(srb->cmd),
							   srb->sg_count, &offset, &len);
				virt = base + offset;

				left_io -= len;

				while (len) {
					u8 byte;
					byte = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);
					*virt++ = byte;

					if (debug_enabled(DBG_PIO))
						printk(" %02x", byte);

					d_left_counter--;
					sg_subtract_one(srb);

					len--;

					fc = DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT);

					if (fc == 0x40) {
						left_io = 0;
						break;
					}
				}

				WARN_ON((fc != 0x40) == !d_left_counter);

				if (fc == 0x40 && (srb->dcb->sync_period & WIDE_SYNC)) {
					/* Read the last byte ... */
					if (srb->total_xfer_length > 0) {
						u8 byte = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);

						*virt++ = byte;
						srb->total_xfer_length--;
						if (debug_enabled(DBG_PIO))
							printk(" %02x", byte);
					}

					DC395x_write8(acb, TRM_S1040_SCSI_CONFIG2, 0);
				}

				scsi_kunmap_atomic_sg(base);
				local_irq_restore(flags);
			}
			/*printk(" %08x", *(u32*)(bus_to_virt (addr))); */
			/*srb->total_xfer_length = 0; */
			if (debug_enabled(DBG_PIO))
				printk("\n");
		}
#endif				/* DC395x_LASTPIO */

#if 0
		/*
		 * KG: This was in DATAOUT. Does it also belong here?
		 * Nobody seems to know what counter and fifo_cnt count exactly ...
		 */
		if (!(scsi_status & SCSIXFERDONE)) {
			/*
			 * when data transfer from DMA FIFO to SCSI FIFO
			 * if there was some data left in SCSI FIFO
			 */
			d_left_counter =
			    (u32)(DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT) &
				  0x1F);
			if (srb->dcb->sync_period & WIDE_SYNC)
				d_left_counter <<= 1;
			/*
			 * if WIDE scsi SCSI FIFOCNT unit is word !!!
			 * so need to *= 2
			 * KG: Seems to be correct ...
			 */
		}
#endif
		/* KG: This should not be needed any more! */
		if (d_left_counter == 0
		    || (scsi_status & SCSIXFERCNT_2_ZERO)) {
#if 0
			int ctr = 6000000;
			u8 TempDMAstatus;
			do {
				TempDMAstatus =
				    DC395x_read8(acb, TRM_S1040_DMA_STATUS);
			} while (!(TempDMAstatus & DMAXFERCOMP) && --ctr);
			if (!ctr)
				dprintkl(KERN_ERR,
				       "Deadlock in DataInPhase0 waiting for DMA!!\n");
			srb->total_xfer_length = 0;
#endif
			srb->total_xfer_length = d_left_counter;
		} else {	/* phase changed */
			/*
			 * parsing the case:
			 * when a transfer not yet complete 
			 * but be disconnected by target
			 * if transfer not yet complete
			 * there were some data residue in SCSI FIFO or
			 * SCSI transfer counter not empty
			 */
			sg_update_list(srb, d_left_counter);
		}
	}
	/* KG: The target may decide to disconnect: Empty FIFO before! */
	if ((*pscsi_status & PHASEMASK) != PH_DATA_IN) {
		cleanup_after_transfer(acb, srb);
	}
}


static void data_in_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "data_in_phase1: (pid#%li) <%02i-%i>\n",
		srb->cmd->serial_number, srb->cmd->device->id, srb->cmd->device->lun);
	data_io_transfer(acb, srb, XFERDATAIN);
}


static void data_io_transfer(struct AdapterCtlBlk *acb, 
		struct ScsiReqBlk *srb, u16 io_dir)
{
	struct DeviceCtlBlk *dcb = srb->dcb;
	u8 bval;
	dprintkdbg(DBG_0,
		"data_io_transfer: (pid#%li) <%02i-%i> %c len=%i, sg=(%i/%i)\n",
		srb->cmd->serial_number, srb->cmd->device->id, srb->cmd->device->lun,
		((io_dir & DMACMD_DIR) ? 'r' : 'w'),
		srb->total_xfer_length, srb->sg_index, srb->sg_count);
	if (srb == acb->tmp_srb)
		dprintkl(KERN_ERR, "data_io_transfer: Using tmp_srb!\n");
	if (srb->sg_index >= srb->sg_count) {
		/* can't happen? out of bounds error */
		return;
	}

	if (srb->total_xfer_length > DC395x_LASTPIO) {
		u8 dma_status = DC395x_read8(acb, TRM_S1040_DMA_STATUS);
		/*
		 * KG: What should we do: Use SCSI Cmd 0x90/0x92?
		 * Maybe, even ABORTXFER would be appropriate
		 */
		if (dma_status & XFERPENDING) {
			dprintkl(KERN_DEBUG, "data_io_transfer: Xfer pending! "
				"Expect trouble!\n");
			dump_register_info(acb, dcb, srb);
			DC395x_write8(acb, TRM_S1040_DMA_CONTROL, CLRXFIFO);
		}
		/* clear_fifo(acb, "IO"); */
		/* 
		 * load what physical address of Scatter/Gather list table
		 * want to be transfer
		 */
		srb->state |= SRB_DATA_XFER;
		DC395x_write32(acb, TRM_S1040_DMA_XHIGHADDR, 0);
		if (scsi_sg_count(srb->cmd)) {	/* with S/G */
			io_dir |= DMACMD_SG;
			DC395x_write32(acb, TRM_S1040_DMA_XLOWADDR,
				       srb->sg_bus_addr +
				       sizeof(struct SGentry) *
				       srb->sg_index);
			/* load how many bytes in the sg list table */
			DC395x_write32(acb, TRM_S1040_DMA_XCNT,
				       ((u32)(srb->sg_count -
					      srb->sg_index) << 3));
		} else {	/* without S/G */
			io_dir &= ~DMACMD_SG;
			DC395x_write32(acb, TRM_S1040_DMA_XLOWADDR,
				       srb->segment_x[0].address);
			DC395x_write32(acb, TRM_S1040_DMA_XCNT,
				       srb->segment_x[0].length);
		}
		/* load total transfer length (24bits) max value 16Mbyte */
		DC395x_write32(acb, TRM_S1040_SCSI_COUNTER,
			       srb->total_xfer_length);
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	/* it's important for atn stop */
		if (io_dir & DMACMD_DIR) {	/* read */
			DC395x_write8(acb, TRM_S1040_SCSI_COMMAND,
				      SCMD_DMA_IN);
			DC395x_write16(acb, TRM_S1040_DMA_COMMAND, io_dir);
		} else {
			DC395x_write16(acb, TRM_S1040_DMA_COMMAND, io_dir);
			DC395x_write8(acb, TRM_S1040_SCSI_COMMAND,
				      SCMD_DMA_OUT);
		}

	}
#if DC395x_LASTPIO
	else if (srb->total_xfer_length > 0) {	/* The last four bytes: Do PIO */
		/* 
		 * load what physical address of Scatter/Gather list table
		 * want to be transfer
		 */
		srb->state |= SRB_DATA_XFER;
		/* load total transfer length (24bits) max value 16Mbyte */
		DC395x_write32(acb, TRM_S1040_SCSI_COUNTER,
			       srb->total_xfer_length);
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	/* it's important for atn stop */
		if (io_dir & DMACMD_DIR) {	/* read */
			DC395x_write8(acb, TRM_S1040_SCSI_COMMAND,
				      SCMD_FIFO_IN);
		} else {	/* write */
			int ln = srb->total_xfer_length;
			size_t left_io = srb->total_xfer_length;

			if (srb->dcb->sync_period & WIDE_SYNC)
				DC395x_write8(acb, TRM_S1040_SCSI_CONFIG2,
				     CFG2_WIDEFIFO);

			while (left_io) {
				unsigned char *virt, *base = NULL;
				unsigned long flags = 0;
				size_t len = left_io;
				size_t offset = srb->request_length - left_io;

				local_irq_save(flags);
				/* Again, max 4 bytes */
				base = scsi_kmap_atomic_sg(scsi_sglist(srb->cmd),
							   srb->sg_count, &offset, &len);
				virt = base + offset;

				left_io -= len;

				while (len--) {
					if (debug_enabled(DBG_PIO))
						printk(" %02x", *virt);

					DC395x_write8(acb, TRM_S1040_SCSI_FIFO, *virt++);

					sg_subtract_one(srb);
				}

				scsi_kunmap_atomic_sg(base);
				local_irq_restore(flags);
			}
			if (srb->dcb->sync_period & WIDE_SYNC) {
				if (ln % 2) {
					DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
					if (debug_enabled(DBG_PIO))
						printk(" |00");
				}
				DC395x_write8(acb, TRM_S1040_SCSI_CONFIG2, 0);
			}
			/*DC395x_write32(acb, TRM_S1040_SCSI_COUNTER, ln); */
			if (debug_enabled(DBG_PIO))
				printk("\n");
			DC395x_write8(acb, TRM_S1040_SCSI_COMMAND,
					  SCMD_FIFO_OUT);
		}
	}
#endif				/* DC395x_LASTPIO */
	else {		/* xfer pad */
		u8 data = 0, data2 = 0;
		if (srb->sg_count) {
			srb->adapter_status = H_OVER_UNDER_RUN;
			srb->status |= OVER_RUN;
		}
		/*
		 * KG: despite the fact that we are using 16 bits I/O ops
		 * the SCSI FIFO is only 8 bits according to the docs
		 * (we can set bit 1 in 0x8f to serialize FIFO access ...)
		 */
		if (dcb->sync_period & WIDE_SYNC) {
			DC395x_write32(acb, TRM_S1040_SCSI_COUNTER, 2);
			DC395x_write8(acb, TRM_S1040_SCSI_CONFIG2,
				      CFG2_WIDEFIFO);
			if (io_dir & DMACMD_DIR) {
				data = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);
				data2 = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);
			} else {
				/* Danger, Robinson: If you find KGs
				 * scattered over the wide disk, the driver
				 * or chip is to blame :-( */
				DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 'K');
				DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 'G');
			}
			DC395x_write8(acb, TRM_S1040_SCSI_CONFIG2, 0);
		} else {
			DC395x_write32(acb, TRM_S1040_SCSI_COUNTER, 1);
			/* Danger, Robinson: If you find a collection of Ks on your disk
			 * something broke :-( */
			if (io_dir & DMACMD_DIR)
				data = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);
			else
				DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 'K');
		}
		srb->state |= SRB_XFERPAD;
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	/* it's important for atn stop */
		/* SCSI command */
		bval = (io_dir & DMACMD_DIR) ? SCMD_FIFO_IN : SCMD_FIFO_OUT;
		DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, bval);
	}
}


static void status_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "status_phase0: (pid#%li) <%02i-%i>\n",
		srb->cmd->serial_number, srb->cmd->device->id, srb->cmd->device->lun);
	srb->target_status = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);
	srb->end_message = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);	/* get message */
	srb->state = SRB_COMPLETED;
	*pscsi_status = PH_BUS_FREE;	/*.. initial phase */
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	/* it's important for atn stop */
	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_MSGACCEPT);
}


static void status_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "status_phase1: (pid#%li) <%02i-%i>\n",
		srb->cmd->serial_number, srb->cmd->device->id, srb->cmd->device->lun);
	srb->state = SRB_STATUS;
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	/* it's important for atn stop */
	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_COMP);
}


/* Check if the message is complete */
static inline u8 msgin_completed(u8 * msgbuf, u32 len)
{
	if (*msgbuf == EXTENDED_MESSAGE) {
		if (len < 2)
			return 0;
		if (len < msgbuf[1] + 2)
			return 0;
	} else if (*msgbuf >= 0x20 && *msgbuf <= 0x2f)	/* two byte messages */
		if (len < 2)
			return 0;
	return 1;
}

/* reject_msg */
static inline void msgin_reject(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb)
{
	srb->msgout_buf[0] = MESSAGE_REJECT;
	srb->msg_count = 1;
	DC395x_ENABLE_MSGOUT;
	srb->state &= ~SRB_MSGIN;
	srb->state |= SRB_MSGOUT;
	dprintkl(KERN_INFO, "msgin_reject: 0x%02x <%02i-%i>\n",
		srb->msgin_buf[0],
		srb->dcb->target_id, srb->dcb->target_lun);
}


static struct ScsiReqBlk *msgin_qtag(struct AdapterCtlBlk *acb,
		struct DeviceCtlBlk *dcb, u8 tag)
{
	struct ScsiReqBlk *srb = NULL;
	struct ScsiReqBlk *i;
	dprintkdbg(DBG_0, "msgin_qtag: (pid#%li) tag=%i srb=%p\n",
		   srb->cmd->serial_number, tag, srb);

	if (!(dcb->tag_mask & (1 << tag)))
		dprintkl(KERN_DEBUG,
			"msgin_qtag: tag_mask=0x%08x does not reserve tag %i!\n",
			dcb->tag_mask, tag);

	if (list_empty(&dcb->srb_going_list))
		goto mingx0;
	list_for_each_entry(i, &dcb->srb_going_list, list) {
		if (i->tag_number == tag) {
			srb = i;
			break;
		}
	}
	if (!srb)
		goto mingx0;

	dprintkdbg(DBG_0, "msgin_qtag: (pid#%li) <%02i-%i>\n",
		srb->cmd->serial_number, srb->dcb->target_id, srb->dcb->target_lun);
	if (dcb->flag & ABORT_DEV_) {
		/*srb->state = SRB_ABORT_SENT; */
		enable_msgout_abort(acb, srb);
	}

	if (!(srb->state & SRB_DISCONNECT))
		goto mingx0;

	memcpy(srb->msgin_buf, dcb->active_srb->msgin_buf, acb->msg_len);
	srb->state |= dcb->active_srb->state;
	srb->state |= SRB_DATA_XFER;
	dcb->active_srb = srb;
	/* How can we make the DORS happy? */
	return srb;

      mingx0:
	srb = acb->tmp_srb;
	srb->state = SRB_UNEXPECT_RESEL;
	dcb->active_srb = srb;
	srb->msgout_buf[0] = MSG_ABORT_TAG;
	srb->msg_count = 1;
	DC395x_ENABLE_MSGOUT;
	dprintkl(KERN_DEBUG, "msgin_qtag: Unknown tag %i - abort\n", tag);
	return srb;
}


static inline void reprogram_regs(struct AdapterCtlBlk *acb,
		struct DeviceCtlBlk *dcb)
{
	DC395x_write8(acb, TRM_S1040_SCSI_TARGETID, dcb->target_id);
	DC395x_write8(acb, TRM_S1040_SCSI_SYNC, dcb->sync_period);
	DC395x_write8(acb, TRM_S1040_SCSI_OFFSET, dcb->sync_offset);
	set_xfer_rate(acb, dcb);
}


/* set async transfer mode */
static void msgin_set_async(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb)
{
	struct DeviceCtlBlk *dcb = srb->dcb;
	dprintkl(KERN_DEBUG, "msgin_set_async: No sync transfers <%02i-%i>\n",
		dcb->target_id, dcb->target_lun);

	dcb->sync_mode &= ~(SYNC_NEGO_ENABLE);
	dcb->sync_mode |= SYNC_NEGO_DONE;
	/*dcb->sync_period &= 0; */
	dcb->sync_offset = 0;
	dcb->min_nego_period = 200 >> 2;	/* 200ns <=> 5 MHz */
	srb->state &= ~SRB_DO_SYNC_NEGO;
	reprogram_regs(acb, dcb);
	if ((dcb->sync_mode & WIDE_NEGO_ENABLE)
	    && !(dcb->sync_mode & WIDE_NEGO_DONE)) {
		build_wdtr(acb, dcb, srb);
		DC395x_ENABLE_MSGOUT;
		dprintkdbg(DBG_0, "msgin_set_async(rej): Try WDTR anyway\n");
	}
}


/* set sync transfer mode */
static void msgin_set_sync(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb)
{
	struct DeviceCtlBlk *dcb = srb->dcb;
	u8 bval;
	int fact;
	dprintkdbg(DBG_1, "msgin_set_sync: <%02i> Sync: %ins "
		"(%02i.%01i MHz) Offset %i\n",
		dcb->target_id, srb->msgin_buf[3] << 2,
		(250 / srb->msgin_buf[3]),
		((250 % srb->msgin_buf[3]) * 10) / srb->msgin_buf[3],
		srb->msgin_buf[4]);

	if (srb->msgin_buf[4] > 15)
		srb->msgin_buf[4] = 15;
	if (!(dcb->dev_mode & NTC_DO_SYNC_NEGO))
		dcb->sync_offset = 0;
	else if (dcb->sync_offset == 0)
		dcb->sync_offset = srb->msgin_buf[4];
	if (srb->msgin_buf[4] > dcb->sync_offset)
		srb->msgin_buf[4] = dcb->sync_offset;
	else
		dcb->sync_offset = srb->msgin_buf[4];
	bval = 0;
	while (bval < 7 && (srb->msgin_buf[3] > clock_period[bval]
			    || dcb->min_nego_period >
			    clock_period[bval]))
		bval++;
	if (srb->msgin_buf[3] < clock_period[bval])
		dprintkl(KERN_INFO,
			"msgin_set_sync: Increase sync nego period to %ins\n",
			clock_period[bval] << 2);
	srb->msgin_buf[3] = clock_period[bval];
	dcb->sync_period &= 0xf0;
	dcb->sync_period |= ALT_SYNC | bval;
	dcb->min_nego_period = srb->msgin_buf[3];

	if (dcb->sync_period & WIDE_SYNC)
		fact = 500;
	else
		fact = 250;

	dprintkl(KERN_INFO,
		"Target %02i: %s Sync: %ins Offset %i (%02i.%01i MB/s)\n",
		dcb->target_id, (fact == 500) ? "Wide16" : "",
		dcb->min_nego_period << 2, dcb->sync_offset,
		(fact / dcb->min_nego_period),
		((fact % dcb->min_nego_period) * 10 +
		dcb->min_nego_period / 2) / dcb->min_nego_period);

	if (!(srb->state & SRB_DO_SYNC_NEGO)) {
		/* Reply with corrected SDTR Message */
		dprintkl(KERN_DEBUG, "msgin_set_sync: answer w/%ins %i\n",
			srb->msgin_buf[3] << 2, srb->msgin_buf[4]);

		memcpy(srb->msgout_buf, srb->msgin_buf, 5);
		srb->msg_count = 5;
		DC395x_ENABLE_MSGOUT;
		dcb->sync_mode |= SYNC_NEGO_DONE;
	} else {
		if ((dcb->sync_mode & WIDE_NEGO_ENABLE)
		    && !(dcb->sync_mode & WIDE_NEGO_DONE)) {
			build_wdtr(acb, dcb, srb);
			DC395x_ENABLE_MSGOUT;
			dprintkdbg(DBG_0, "msgin_set_sync: Also try WDTR\n");
		}
	}
	srb->state &= ~SRB_DO_SYNC_NEGO;
	dcb->sync_mode |= SYNC_NEGO_DONE | SYNC_NEGO_ENABLE;

	reprogram_regs(acb, dcb);
}


static inline void msgin_set_nowide(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb)
{
	struct DeviceCtlBlk *dcb = srb->dcb;
	dprintkdbg(DBG_1, "msgin_set_nowide: <%02i>\n", dcb->target_id);

	dcb->sync_period &= ~WIDE_SYNC;
	dcb->sync_mode &= ~(WIDE_NEGO_ENABLE);
	dcb->sync_mode |= WIDE_NEGO_DONE;
	srb->state &= ~SRB_DO_WIDE_NEGO;
	reprogram_regs(acb, dcb);
	if ((dcb->sync_mode & SYNC_NEGO_ENABLE)
	    && !(dcb->sync_mode & SYNC_NEGO_DONE)) {
		build_sdtr(acb, dcb, srb);
		DC395x_ENABLE_MSGOUT;
		dprintkdbg(DBG_0, "msgin_set_nowide: Rejected. Try SDTR anyway\n");
	}
}

static void msgin_set_wide(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb)
{
	struct DeviceCtlBlk *dcb = srb->dcb;
	u8 wide = (dcb->dev_mode & NTC_DO_WIDE_NEGO
		   && acb->config & HCC_WIDE_CARD) ? 1 : 0;
	dprintkdbg(DBG_1, "msgin_set_wide: <%02i>\n", dcb->target_id);

	if (srb->msgin_buf[3] > wide)
		srb->msgin_buf[3] = wide;
	/* Completed */
	if (!(srb->state & SRB_DO_WIDE_NEGO)) {
		dprintkl(KERN_DEBUG,
			"msgin_set_wide: Wide nego initiated <%02i>\n",
			dcb->target_id);
		memcpy(srb->msgout_buf, srb->msgin_buf, 4);
		srb->msg_count = 4;
		srb->state |= SRB_DO_WIDE_NEGO;
		DC395x_ENABLE_MSGOUT;
	}

	dcb->sync_mode |= (WIDE_NEGO_ENABLE | WIDE_NEGO_DONE);
	if (srb->msgin_buf[3] > 0)
		dcb->sync_period |= WIDE_SYNC;
	else
		dcb->sync_period &= ~WIDE_SYNC;
	srb->state &= ~SRB_DO_WIDE_NEGO;
	/*dcb->sync_mode &= ~(WIDE_NEGO_ENABLE+WIDE_NEGO_DONE); */
	dprintkdbg(DBG_1,
		"msgin_set_wide: Wide (%i bit) negotiated <%02i>\n",
		(8 << srb->msgin_buf[3]), dcb->target_id);
	reprogram_regs(acb, dcb);
	if ((dcb->sync_mode & SYNC_NEGO_ENABLE)
	    && !(dcb->sync_mode & SYNC_NEGO_DONE)) {
		build_sdtr(acb, dcb, srb);
		DC395x_ENABLE_MSGOUT;
		dprintkdbg(DBG_0, "msgin_set_wide: Also try SDTR.\n");
	}
}


/*
 * extended message codes:
 *
 *	code	description
 *
 *	02h	Reserved
 *	00h	MODIFY DATA  POINTER
 *	01h	SYNCHRONOUS DATA TRANSFER REQUEST
 *	03h	WIDE DATA TRANSFER REQUEST
 *   04h - 7Fh	Reserved
 *   80h - FFh	Vendor specific
 */
static void msgin_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	struct DeviceCtlBlk *dcb = acb->active_dcb;
	dprintkdbg(DBG_0, "msgin_phase0: (pid#%li)\n", srb->cmd->serial_number);

	srb->msgin_buf[acb->msg_len++] = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);
	if (msgin_completed(srb->msgin_buf, acb->msg_len)) {
		/* Now eval the msg */
		switch (srb->msgin_buf[0]) {
		case DISCONNECT:
			srb->state = SRB_DISCONNECT;
			break;

		case SIMPLE_QUEUE_TAG:
		case HEAD_OF_QUEUE_TAG:
		case ORDERED_QUEUE_TAG:
			srb =
			    msgin_qtag(acb, dcb,
					      srb->msgin_buf[1]);
			break;

		case MESSAGE_REJECT:
			DC395x_write16(acb, TRM_S1040_SCSI_CONTROL,
				       DO_CLRATN | DO_DATALATCH);
			/* A sync nego message was rejected ! */
			if (srb->state & SRB_DO_SYNC_NEGO) {
				msgin_set_async(acb, srb);
				break;
			}
			/* A wide nego message was rejected ! */
			if (srb->state & SRB_DO_WIDE_NEGO) {
				msgin_set_nowide(acb, srb);
				break;
			}
			enable_msgout_abort(acb, srb);
			/*srb->state |= SRB_ABORT_SENT */
			break;

		case EXTENDED_MESSAGE:
			/* SDTR */
			if (srb->msgin_buf[1] == 3
			    && srb->msgin_buf[2] == EXTENDED_SDTR) {
				msgin_set_sync(acb, srb);
				break;
			}
			/* WDTR */
			if (srb->msgin_buf[1] == 2
			    && srb->msgin_buf[2] == EXTENDED_WDTR
			    && srb->msgin_buf[3] <= 2) { /* sanity check ... */
				msgin_set_wide(acb, srb);
				break;
			}
			msgin_reject(acb, srb);
			break;

		case MSG_IGNOREWIDE:
			/* Discard  wide residual */
			dprintkdbg(DBG_0, "msgin_phase0: Ignore Wide Residual!\n");
			break;

		case COMMAND_COMPLETE:
			/* nothing has to be done */
			break;

		case SAVE_POINTERS:
			/*
			 * SAVE POINTER may be ignored as we have the struct
			 * ScsiReqBlk* associated with the scsi command.
			 */
			dprintkdbg(DBG_0, "msgin_phase0: (pid#%li) "
				"SAVE POINTER rem=%i Ignore\n",
				srb->cmd->serial_number, srb->total_xfer_length);
			break;

		case RESTORE_POINTERS:
			dprintkdbg(DBG_0, "msgin_phase0: RESTORE POINTER. Ignore\n");
			break;

		case ABORT:
			dprintkdbg(DBG_0, "msgin_phase0: (pid#%li) "
				"<%02i-%i> ABORT msg\n",
				srb->cmd->serial_number, dcb->target_id,
				dcb->target_lun);
			dcb->flag |= ABORT_DEV_;
			enable_msgout_abort(acb, srb);
			break;

		default:
			/* reject unknown messages */
			if (srb->msgin_buf[0] & IDENTIFY_BASE) {
				dprintkdbg(DBG_0, "msgin_phase0: Identify msg\n");
				srb->msg_count = 1;
				srb->msgout_buf[0] = dcb->identify_msg;
				DC395x_ENABLE_MSGOUT;
				srb->state |= SRB_MSGOUT;
				/*break; */
			}
			msgin_reject(acb, srb);
		}

		/* Clear counter and MsgIn state */
		srb->state &= ~SRB_MSGIN;
		acb->msg_len = 0;
	}
	*pscsi_status = PH_BUS_FREE;
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	/* it's important ... you know! */
	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_MSGACCEPT);
}


static void msgin_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "msgin_phase1: (pid#%li)\n", srb->cmd->serial_number);
	clear_fifo(acb, "msgin_phase1");
	DC395x_write32(acb, TRM_S1040_SCSI_COUNTER, 1);
	if (!(srb->state & SRB_MSGIN)) {
		srb->state &= ~SRB_DISCONNECT;
		srb->state |= SRB_MSGIN;
	}
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	/* it's important for atn stop */
	/* SCSI command */
	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_FIFO_IN);
}


static void nop0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
}


static void nop1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
}


static void set_xfer_rate(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb)
{
	struct DeviceCtlBlk *i;

	/* set all lun device's  period, offset */
	if (dcb->identify_msg & 0x07)
		return;

	if (acb->scan_devices) {
		current_sync_offset = dcb->sync_offset;
		return;
	}

	list_for_each_entry(i, &acb->dcb_list, list)
		if (i->target_id == dcb->target_id) {
			i->sync_period = dcb->sync_period;
			i->sync_offset = dcb->sync_offset;
			i->sync_mode = dcb->sync_mode;
			i->min_nego_period = dcb->min_nego_period;
		}
}


static void disconnect(struct AdapterCtlBlk *acb)
{
	struct DeviceCtlBlk *dcb = acb->active_dcb;
	struct ScsiReqBlk *srb;

	if (!dcb) {
		dprintkl(KERN_ERR, "disconnect: No such device\n");
		udelay(500);
		/* Suspend queue for a while */
		acb->scsi_host->last_reset =
		    jiffies + HZ / 2 +
		    HZ * acb->eeprom.delay_time;
		clear_fifo(acb, "disconnectEx");
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_HWRESELECT);
		return;
	}
	srb = dcb->active_srb;
	acb->active_dcb = NULL;
	dprintkdbg(DBG_0, "disconnect: (pid#%li)\n", srb->cmd->serial_number);

	srb->scsi_phase = PH_BUS_FREE;	/* initial phase */
	clear_fifo(acb, "disconnect");
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_HWRESELECT);
	if (srb->state & SRB_UNEXPECT_RESEL) {
		dprintkl(KERN_ERR,
			"disconnect: Unexpected reselection <%02i-%i>\n",
			dcb->target_id, dcb->target_lun);
		srb->state = 0;
		waiting_process_next(acb);
	} else if (srb->state & SRB_ABORT_SENT) {
		dcb->flag &= ~ABORT_DEV_;
		acb->scsi_host->last_reset = jiffies + HZ / 2 + 1;
		dprintkl(KERN_ERR, "disconnect: SRB_ABORT_SENT\n");
		doing_srb_done(acb, DID_ABORT, srb->cmd, 1);
		waiting_process_next(acb);
	} else {
		if ((srb->state & (SRB_START_ + SRB_MSGOUT))
		    || !(srb->
			 state & (SRB_DISCONNECT + SRB_COMPLETED))) {
			/*
			 * Selection time out 
			 * SRB_START_ || SRB_MSGOUT || (!SRB_DISCONNECT && !SRB_COMPLETED)
			 */
			/* Unexp. Disc / Sel Timeout */
			if (srb->state != SRB_START_
			    && srb->state != SRB_MSGOUT) {
				srb->state = SRB_READY;
				dprintkl(KERN_DEBUG,
					"disconnect: (pid#%li) Unexpected\n",
					srb->cmd->serial_number);
				srb->target_status = SCSI_STAT_SEL_TIMEOUT;
				goto disc1;
			} else {
				/* Normal selection timeout */
				dprintkdbg(DBG_KG, "disconnect: (pid#%li) "
					"<%02i-%i> SelTO\n", srb->cmd->serial_number,
					dcb->target_id, dcb->target_lun);
				if (srb->retry_count++ > DC395x_MAX_RETRIES
				    || acb->scan_devices) {
					srb->target_status =
					    SCSI_STAT_SEL_TIMEOUT;
					goto disc1;
				}
				free_tag(dcb, srb);
				srb_going_to_waiting_move(dcb, srb);
				dprintkdbg(DBG_KG,
					"disconnect: (pid#%li) Retry\n",
					srb->cmd->serial_number);
				waiting_set_timer(acb, HZ / 20);
			}
		} else if (srb->state & SRB_DISCONNECT) {
			u8 bval = DC395x_read8(acb, TRM_S1040_SCSI_SIGNAL);
			/*
			 * SRB_DISCONNECT (This is what we expect!)
			 */
			if (bval & 0x40) {
				dprintkdbg(DBG_0, "disconnect: SCSI bus stat "
					" 0x%02x: ACK set! Other controllers?\n",
					bval);
				/* It could come from another initiator, therefore don't do much ! */
			} else
				waiting_process_next(acb);
		} else if (srb->state & SRB_COMPLETED) {
		      disc1:
			/*
			 ** SRB_COMPLETED
			 */
			free_tag(dcb, srb);
			dcb->active_srb = NULL;
			srb->state = SRB_FREE;
			srb_done(acb, dcb, srb);
		}
	}
}


static void reselect(struct AdapterCtlBlk *acb)
{
	struct DeviceCtlBlk *dcb = acb->active_dcb;
	struct ScsiReqBlk *srb = NULL;
	u16 rsel_tar_lun_id;
	u8 id, lun;
	u8 arblostflag = 0;
	dprintkdbg(DBG_0, "reselect: acb=%p\n", acb);

	clear_fifo(acb, "reselect");
	/*DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_HWRESELECT | DO_DATALATCH); */
	/* Read Reselected Target ID and LUN */
	rsel_tar_lun_id = DC395x_read16(acb, TRM_S1040_SCSI_TARGETID);
	if (dcb) {		/* Arbitration lost but Reselection win */
		srb = dcb->active_srb;
		if (!srb) {
			dprintkl(KERN_DEBUG, "reselect: Arb lost Resel won, "
				"but active_srb == NULL\n");
			DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	/* it's important for atn stop */
			return;
		}
		/* Why the if ? */
		if (!acb->scan_devices) {
			dprintkdbg(DBG_KG, "reselect: (pid#%li) <%02i-%i> "
				"Arb lost but Resel win rsel=%i stat=0x%04x\n",
				srb->cmd->serial_number, dcb->target_id,
				dcb->target_lun, rsel_tar_lun_id,
				DC395x_read16(acb, TRM_S1040_SCSI_STATUS));
			arblostflag = 1;
			/*srb->state |= SRB_DISCONNECT; */

			srb->state = SRB_READY;
			free_tag(dcb, srb);
			srb_going_to_waiting_move(dcb, srb);
			waiting_set_timer(acb, HZ / 20);

			/* return; */
		}
	}
	/* Read Reselected Target Id and LUN */
	if (!(rsel_tar_lun_id & (IDENTIFY_BASE << 8)))
		dprintkl(KERN_DEBUG, "reselect: Expects identify msg. "
			"Got %i!\n", rsel_tar_lun_id);
	id = rsel_tar_lun_id & 0xff;
	lun = (rsel_tar_lun_id >> 8) & 7;
	dcb = find_dcb(acb, id, lun);
	if (!dcb) {
		dprintkl(KERN_ERR, "reselect: From non existent device "
			"<%02i-%i>\n", id, lun);
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	/* it's important for atn stop */
		return;
	}
	acb->active_dcb = dcb;

	if (!(dcb->dev_mode & NTC_DO_DISCONNECT))
		dprintkl(KERN_DEBUG, "reselect: in spite of forbidden "
			"disconnection? <%02i-%i>\n",
			dcb->target_id, dcb->target_lun);

	if (dcb->sync_mode & EN_TAG_QUEUEING /*&& !arblostflag */) {
		srb = acb->tmp_srb;
		dcb->active_srb = srb;
	} else {
		/* There can be only one! */
		srb = dcb->active_srb;
		if (!srb || !(srb->state & SRB_DISCONNECT)) {
			/*
			 * abort command
			 */
			dprintkl(KERN_DEBUG,
				"reselect: w/o disconnected cmds <%02i-%i>\n",
				dcb->target_id, dcb->target_lun);
			srb = acb->tmp_srb;
			srb->state = SRB_UNEXPECT_RESEL;
			dcb->active_srb = srb;
			enable_msgout_abort(acb, srb);
		} else {
			if (dcb->flag & ABORT_DEV_) {
				/*srb->state = SRB_ABORT_SENT; */
				enable_msgout_abort(acb, srb);
			} else
				srb->state = SRB_DATA_XFER;

		}
	}
	srb->scsi_phase = PH_BUS_FREE;	/* initial phase */

	/* Program HA ID, target ID, period and offset */
	dprintkdbg(DBG_0, "reselect: select <%i>\n", dcb->target_id);
	DC395x_write8(acb, TRM_S1040_SCSI_HOSTID, acb->scsi_host->this_id);	/* host   ID */
	DC395x_write8(acb, TRM_S1040_SCSI_TARGETID, dcb->target_id);		/* target ID */
	DC395x_write8(acb, TRM_S1040_SCSI_OFFSET, dcb->sync_offset);		/* offset    */
	DC395x_write8(acb, TRM_S1040_SCSI_SYNC, dcb->sync_period);		/* sync period, wide */
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);		/* it's important for atn stop */
	/* SCSI command */
	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_MSGACCEPT);
}


static inline u8 tagq_blacklist(char *name)
{
#ifndef DC395x_NO_TAGQ
#if 0
	u8 i;
	for (i = 0; i < BADDEVCNT; i++)
		if (memcmp(name, DC395x_baddevname1[i], 28) == 0)
			return 1;
#endif
	return 0;
#else
	return 1;
#endif
}


static void disc_tagq_set(struct DeviceCtlBlk *dcb, struct ScsiInqData *ptr)
{
	/* Check for SCSI format (ANSI and Response data format) */
	if ((ptr->Vers & 0x07) >= 2 || (ptr->RDF & 0x0F) == 2) {
		if ((ptr->Flags & SCSI_INQ_CMDQUEUE)
		    && (dcb->dev_mode & NTC_DO_TAG_QUEUEING) &&
		    /*(dcb->dev_mode & NTC_DO_DISCONNECT) */
		    /* ((dcb->dev_type == TYPE_DISK) 
		       || (dcb->dev_type == TYPE_MOD)) && */
		    !tagq_blacklist(((char *)ptr) + 8)) {
			if (dcb->max_command == 1)
				dcb->max_command =
				    dcb->acb->tag_max_num;
			dcb->sync_mode |= EN_TAG_QUEUEING;
			/*dcb->tag_mask = 0; */
		} else
			dcb->max_command = 1;
	}
}


static void add_dev(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiInqData *ptr)
{
	u8 bval1 = ptr->DevType & SCSI_DEVTYPE;
	dcb->dev_type = bval1;
	/* if (bval1 == TYPE_DISK || bval1 == TYPE_MOD) */
	disc_tagq_set(dcb, ptr);
}


/* unmap mapped pci regions from SRB */
static void pci_unmap_srb(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb)
{
	struct scsi_cmnd *cmd = srb->cmd;
	enum dma_data_direction dir = cmd->sc_data_direction;

	if (scsi_sg_count(cmd) && dir != PCI_DMA_NONE) {
		/* unmap DC395x SG list */
		dprintkdbg(DBG_SG, "pci_unmap_srb: list=%08x(%05x)\n",
			srb->sg_bus_addr, SEGMENTX_LEN);
		pci_unmap_single(acb->dev, srb->sg_bus_addr,
				 SEGMENTX_LEN,
				 PCI_DMA_TODEVICE);
		dprintkdbg(DBG_SG, "pci_unmap_srb: segs=%i buffer=%p\n",
			   scsi_sg_count(cmd), scsi_bufflen(cmd));
		/* unmap the sg segments */
		scsi_dma_unmap(cmd);
	}
}


/* unmap mapped pci sense buffer from SRB */
static void pci_unmap_srb_sense(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb)
{
	if (!(srb->flag & AUTO_REQSENSE))
		return;
	/* Unmap sense buffer */
	dprintkdbg(DBG_SG, "pci_unmap_srb_sense: buffer=%08x\n",
	       srb->segment_x[0].address);
	pci_unmap_single(acb->dev, srb->segment_x[0].address,
			 srb->segment_x[0].length, PCI_DMA_FROMDEVICE);
	/* Restore SG stuff */
	srb->total_xfer_length = srb->xferred;
	srb->segment_x[0].address =
	    srb->segment_x[DC395x_MAX_SG_LISTENTRY - 1].address;
	srb->segment_x[0].length =
	    srb->segment_x[DC395x_MAX_SG_LISTENTRY - 1].length;
}


/*
 * Complete execution of a SCSI command
 * Signal completion to the generic SCSI driver  
 */
static void srb_done(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	u8 tempcnt, status;
	struct scsi_cmnd *cmd = srb->cmd;
	enum dma_data_direction dir = cmd->sc_data_direction;
	int ckc_only = 1;

	dprintkdbg(DBG_1, "srb_done: (pid#%li) <%02i-%i>\n", srb->cmd->serial_number,
		srb->cmd->device->id, srb->cmd->device->lun);
	dprintkdbg(DBG_SG, "srb_done: srb=%p sg=%i(%i/%i) buf=%p\n",
		   srb, scsi_sg_count(cmd), srb->sg_index, srb->sg_count,
		   scsi_sgtalbe(cmd));
	status = srb->target_status;
	if (srb->flag & AUTO_REQSENSE) {
		dprintkdbg(DBG_0, "srb_done: AUTO_REQSENSE1\n");
		pci_unmap_srb_sense(acb, srb);
		/*
		 ** target status..........................
		 */
		srb->flag &= ~AUTO_REQSENSE;
		srb->adapter_status = 0;
		srb->target_status = CHECK_CONDITION << 1;
		if (debug_enabled(DBG_1)) {
			switch (cmd->sense_buffer[2] & 0x0f) {
			case NOT_READY:
				dprintkl(KERN_DEBUG,
				     "ReqSense: NOT_READY cmnd=0x%02x <%02i-%i> stat=%i scan=%i ",
				     cmd->cmnd[0], dcb->target_id,
				     dcb->target_lun, status, acb->scan_devices);
				break;
			case UNIT_ATTENTION:
				dprintkl(KERN_DEBUG,
				     "ReqSense: UNIT_ATTENTION cmnd=0x%02x <%02i-%i> stat=%i scan=%i ",
				     cmd->cmnd[0], dcb->target_id,
				     dcb->target_lun, status, acb->scan_devices);
				break;
			case ILLEGAL_REQUEST:
				dprintkl(KERN_DEBUG,
				     "ReqSense: ILLEGAL_REQUEST cmnd=0x%02x <%02i-%i> stat=%i scan=%i ",
				     cmd->cmnd[0], dcb->target_id,
				     dcb->target_lun, status, acb->scan_devices);
				break;
			case MEDIUM_ERROR:
				dprintkl(KERN_DEBUG,
				     "ReqSense: MEDIUM_ERROR cmnd=0x%02x <%02i-%i> stat=%i scan=%i ",
				     cmd->cmnd[0], dcb->target_id,
				     dcb->target_lun, status, acb->scan_devices);
				break;
			case HARDWARE_ERROR:
				dprintkl(KERN_DEBUG,
				     "ReqSense: HARDWARE_ERROR cmnd=0x%02x <%02i-%i> stat=%i scan=%i ",
				     cmd->cmnd[0], dcb->target_id,
				     dcb->target_lun, status, acb->scan_devices);
				break;
			}
			if (cmd->sense_buffer[7] >= 6)
				printk("sense=0x%02x ASC=0x%02x ASCQ=0x%02x "
					"(0x%08x 0x%08x)\n",
					cmd->sense_buffer[2], cmd->sense_buffer[12],
					cmd->sense_buffer[13],
					*((unsigned int *)(cmd->sense_buffer + 3)),
					*((unsigned int *)(cmd->sense_buffer + 8)));
			else
				printk("sense=0x%02x No ASC/ASCQ (0x%08x)\n",
					cmd->sense_buffer[2],
					*((unsigned int *)(cmd->sense_buffer + 3)));
		}

		if (status == (CHECK_CONDITION << 1)) {
			cmd->result = DID_BAD_TARGET << 16;
			goto ckc_e;
		}
		dprintkdbg(DBG_0, "srb_done: AUTO_REQSENSE2\n");

		if (srb->total_xfer_length
		    && srb->total_xfer_length >= cmd->underflow)
			cmd->result =
			    MK_RES_LNX(DRIVER_SENSE, DID_OK,
				       srb->end_message, CHECK_CONDITION);
		/*SET_RES_DID(cmd->result,DID_OK) */
		else
			cmd->result =
			    MK_RES_LNX(DRIVER_SENSE, DID_OK,
				       srb->end_message, CHECK_CONDITION);

		goto ckc_e;
	}

/*************************************************************/
	if (status) {
		/*
		 * target status..........................
		 */
		if (status_byte(status) == CHECK_CONDITION) {
			request_sense(acb, dcb, srb);
			return;
		} else if (status_byte(status) == QUEUE_FULL) {
			tempcnt = (u8)list_size(&dcb->srb_going_list);
			dprintkl(KERN_INFO, "QUEUE_FULL for dev <%02i-%i> with %i cmnds\n",
			     dcb->target_id, dcb->target_lun, tempcnt);
			if (tempcnt > 1)
				tempcnt--;
			dcb->max_command = tempcnt;
			free_tag(dcb, srb);
			srb_going_to_waiting_move(dcb, srb);
			waiting_set_timer(a95x.HZ / 20*
 * Dsrb->adapter_status = 0/UW/F), DCtarget * PCI SCSI Bus return
 * } else if (* PCI SC= SCSI_STAT_SEL_TIMEOUT) {UW/F), DC315(U)
 * PCI SCSH*
 * Authors Bus Master Host Adapter
 * (SCScmd->result = DID_NO_CONNECT << 16 set used T *  C.L. Huang <ching@tekraI Bus SET_RES_DID(>
 *  (C) C,yrighERROR*
 * D<garloffMSGse.de>
 *  (C)), DCend_message0 Kurt GarloffTARGET
 *  Oliver Neu PCI );
set 
Technology C/*
		 ** process initiatorc@web.d.03
 *
 * License: GNU GPLleneh/
		* PCI SCS), DC315(U)
 * PCI 
 * ekram ASIC & H_OVER_UNDER_RUN:
 *  C.L. Hr Host Adapter
 * (SCS<garloff@suse.de>
 *  (C) 199OK0 Kurt Garloff
 *
 *  Oliver Neukum <oliver@neukum.n used Tekram, DC*********PARITY99-2000ogy Co<garloff@suse.de>
 *  (C) 199nditiowith or without
 * modification, are permitted provided tha{	/* No error ***UW/F), DC315(U)
 * PCI SCSI Bus Master Host Adapter
 * (SCSse in source and binary forms, with  Jam

*****dir != PCI_DMA_NONE && scsi_sg_countse.d))
		pci_dma_syncionsfor_cpuTekr->dev,onditionlis the f, * D		nditions and the f, dire>
 *ckc_only Garlo/* Check EtionsConditions *** ay noistre:
 *    >
 * cmnd[0] TRMINQUIRYare meunsigned char *base = NULL
 * struct ScsiInqData *ptr
 *     derivlong flag SCSI Bus withouscattern an* sg****ntation and/or THIS ize_t offseCopy0, len****izeof( without specific e>
 * local_irq_save(n.
 * IS''m this THE Akmap_atomiimer(sgumentatios provided wi&Y EXPR, &len IS''ptr =ram ithout specific pr)(m thi+NY EXPRe>
 * ekra!istributi&& endors (C) Co& loff@su) TRM0lene  LE Fndorse or 2romot0f conditibufflenthe f >= 8INCIDENTALnotice, this list of coRPOSE FOptr->Ver****0x07L DAM2ollo	d    inquiry7 =ED T->F.
 *>
 */*if(******ndorse or promote produO, P****/*  (host_bytese.de>
 *  (T,
 *orms,  ||c@web.dER CAUSED AND ON AN& CHECK1995DITION) ) *****ekrase.de>
 *  ( TRM(* THEOR Tekrolloay noY OF LIABILITY, WHETHER IN COOR OTHERTRACT, STRICT LIare metUTHORDS OR Sit_tcq_n.
 are met	add_devTekram395x.pt theSIBI IF ADVISED OF THE = 1*
 **}pyrigHIS RANTIunES
 * OF MERCHm th *
 *T NOT LIMIrestor TO, THE IM****/* Here is the info for Doug Gilbert's sg3 ...RUPTITHE AUetncluidendoNeukum total_xfer_length *
 /* This may binuxterpreted by sb. or not<linux/de>
 * SCp.thisincludual********ype.h>
#include <;de <linux/iONSEer.h>
#include I Bu    nebug_enabled(DBG_KGOFTWAREt the folype.h>
#include <l GOODprintkdbg <linux, "srb_done: (pid#%li) <%02i-%i> "
 ***"e or=0x%02x Missed %i R CAs\n"other >
 * serial_number,L, SPEdevice->idclude <scsi/scslune <scsi/scse or prinux/ctype.h>
#include <linu****nclugoe Drre * dc395x.c
 *
 */* Add to free n anRUPTIt the f TRM*    tmp_c
 *
clude <scl(KERN99-2
#include <sc9-200! Compllinuxcmd with 15(U) -\n" *
 hnology Cude <scsi/scsi.0
#include <scsi/scsicamde <  (C) Cicam_8xlude <sci/scsi_cmnd.h>
#include <ND ON ATHIS rb_NER	_insertTekramc
 *
 *}
owing******srb------------de <lin*****de <``AS IS'evice Dr@twibbl_nex-----);
}

 * 3abort all04/0le.o our queuehor m@webic void dX_NAMnclude <ARE DISCA15(U)
CtlBlk *ekramu8 didF THEothe SOFTWAREsi_e or * <linu8odulce)
{
fine DC3Dcsi/sfine DC3dcb;
IC TRM-S1040"
#INFO, "define DC395x_: pids ----
	n an in teach_entryc395x.&*    dcb_n an,"Tekrare meRE DISCLAIMReqe DC3sr------------------------tmpTHIS SOFTWAREx_NO_WIDEpG, BUT                  _safehe f,

/*, &DS ORDC395X_NAM----------------	enum  disdata_direc autUDIN*
 **int------->
 * 	p********cmd*
 **noti= p----e DBG_0		0x0002 (SCSI C) CopyMKrlof(0withC*/
/*#S OR(U/UW/Fde <sc("G:%li(h>	/* n) ",defin_cmnd.h>
#inc4
#day notdefiscsi/scsi_de all debuggluLAR PU DC395X_NAME	"dc395x"
#define 		-----tagc395x.c
 *
 * D----------------------------		p*  (C) Copy1		0x000* Ou------------_senseG_0*/


/*
 * Ou------------------------RE, EVE------POSSIBI/* Fupt.ew EH, we normally----'t neeX_BANgive commands backother  * ae <liy*/
/*#2.05, 2rupt/
/* for ouram DC* Outp----------pa ": "******E, EVENdebugempty(isabled
 */
#define folloIC TRM-S1040"
#DEBUG, e to remove"How could <linML sen004/...)to <linGX_NA95x_NO?m.h>	/* nelude <sc removeDS ORr Host i_deks that the ne DEBUG    ns thatg_mask#include <scBUG, then the * driver nabefore
 odule.h>	/* neeshllowebe messa,ude --
 ! This also checks that the specified debug levis also checks thatfore
 *>
 * /* Wvice D95x_NO *****debugging that can be enabled and disabled
 *evice Drine DBG_KG		0x000DBG_1		0x000
#define DBG_SG		
#define DBG_PIO		0x0080


/*
 * Set set of things Wo ou.h>	/* neging for.
 * Undefin
 */
/*#defiidne to remove all debuggne DEBUG_MASKevice Dr0|DBG_1|DBG_SG|DBG_FI  DEBUG_MASK	DBG_0*/


/*
 * Output a kernel mesage at the specified level and append the
 * driver name and a ": " to the start of the message
 */
#define dprintkl(level, format, arg...)  \
    printk(level DC395X_NAME ": " format , ## arg)


----------------*/
/*
/*
 * print a debug message - this i/
#define de with KERN_DEBUG, then the
"ML95x_NOam */and thagainhen tput. 
 * This also chdebugPLIEhost_lock, flags)
#definecified debug  (0)
#defineified debug level isbled *****&= ~ABORT_DEV_-------things ------the ONNECT*/
/*#re>
#i OR CONsNO_TAGQ*/
/*#define DC395x------------------------)(inw(acb->io_:), D=%plude), Da ":*    acb******|= INDE)
#de;le (
#defineTECT,5x_writeONE,address,EVRUPTIDC395x_write16TekramTRM_S1040_-S104CONTROL, DO_RST-S10} whiwhile (!(, acb->read8_base + (address))
#dINT0)
 US CONINTs))
#
#deffollo/*t.h>h/*
 */acb,address)		(u16>
#ibasic_configport_base + (address)))
#defineu8 bval;
	u16 w->resu, acb->io_po((value), acb->io_portAuthorsio_po forlr foroures
 ekra/
#de), acb****CCource coIMPLvlude PHASELATCH | INITIATOR | BLOCKRST |onditioTRACT-------icial ... */
#define RES_ENDMSG		0x0000FF00)

#ne RES_TARGET		0x000000FF	/* TargeCONFIG0,md->r} whi/an@twgram ), acbura0002
1: Act_Neg (+((int)(d_Enh? + Fast_Filter(didfic Dis?BILITYfine RES_DRV			0xFF000000	/* DRIVER_ cod1,F SU3)DC395was 0x13: defaINCLUPTION)MK_RES(dHost ID remove(tgt)<<1)

int)(tgt))
#define MK_RES_LNX(drv,did,HOSTID */
#def
/*
OWEV->nit.hiS IS'/*outl anclaihronous transfer<<1)

#define SET_RES_TARGET(who,tgt) { who &OFFSET) ((i(U/UWx/blurn LEDdrv,trolNY E#defi/
#dopyralue)	outwrt_base + (addressGENdefine D CON0x7Fefine RES_TARGESG; who |= (int)(msg) << 8; },*/
#d }
#defDMAdrv,didgt)<<1)

#defi{ who &= ~RES_ENDMSG; who |= (int)(ms liR_ cod CON~s liFIFO_CTRtwar{ who|=
lso c) << 24; HALF * sr | 255
ENHANCE /*ement_MEM_MULTI_READ */  SET_RES_DID(who,did) { who &= ~Rho |= (int |= (int)(didClear pend/*
 cluderuptc@web.d#define SET_outw((value), acb->io_port_base + (a}
#defEncludM-S10hich does n

#define SET_RES_TARGET(who,tgt) { who &INTEN) ((7Fsizene RES_TARGET		0x000000FF	/* s li2 addreENs))
#INT		0xEN
};
XFER9-200OR OTHERe hw ure for TRss)))ucture for TRCOMPuctureFORCEDMATarge*****(tgt)<(acb,address)		(u16; who)(inw(detectport_base + (address)))
#define DC395x--------------t period         )(inl(acb->io_port_b/* delay half a secoWIDE DC395( for _block w(bugginevice Dr for follodine RESrvendor_id[2];	/* 0,1 
#define RES_DRV			0xFF000000	/* DRIVER_ ne DC395x_wrMODULE	/* bus! address */
	u32 length;
};

b_class;		MA
#def   Sub Cla/*ss       */
	u8 vendor_id[2];	/* 5,6  VenSTOP for TR);t NvRuyte 3(51; }
#defMaybe/
#dlocked up <linbus? Then lets evic evet[Donger<linux/deARGET; who |= (il<<16)(inw G_NONE jiffies + 5 *m DC395 +_NONE HZintk
#deeeprom.yte 3/* 0,-----near_fifoTekram  */
	u8 cfg3;		/*-----utl((value), acb-guration 1.25)<<16 |, acb->io_port_base + (address))
#define DC395xHW
#deLECT;		/* ET_LNX  STA (address, INDefine )t of cx_write8(acb,address,value)	outb((value)base + (address)))
#definONE----chnology Case + (address)))
#define(acb(((siod     v_param8 channe	define DC395x_Nekramrigh max_, soft, 1a ": 75 ChanneRecoverSRB(				 ;		/* 9*/
	u8 btive_dcbis softwareime       */
	Garloff * Set to disable parts of 	}cb,address)		(u16)(que->ho leveO_TAGQ*/
/*#define DC395x_N------------------------#define DC3---------------------------95x_NO_WIDE*/
fine DBG_SG		0ude <scsi/scsi.1cb,adtruct scsi_scsi/scsicam.h>	/* nelude <si/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#included[2];e foldress)))AUTO_REQSENSrget), DC315(U)
 * PCI SCSI Bu
 *
 * Redistribution anion bKG: Can nit. pr_SCSt crap ": se  DBG ?t NvRmemse the  fornseCONSE \
	0,M-S1040ex o_BUFFERSIZb Clion bSave sommainingx/del follegment_x[= ~RES_MAX_SG_LISTENTRY - 1].addrbbleG_NONE est */
	/*
	 * 0unction, uest */
/
	/*
	 * The sense buffer handling funlude <request_sense, uses
	 * thh>
#incluest *
#inrer_t sg_bux/spinlock.h>
#inclu
#desg entry (seg : a ----      of S/G"Tekratruct request *ype.h>
#include < RM-S1040th;		/* Total n hw sg entry (segm While dod the
	 * total_xfer_length     tes remaze_t r request */
	/*
	 *  the firstrequest_he
 -----ingl];	/*  documered */
	size_t res also he
	 * total_xfer_len,, this liFROMDEVICb Claude <scsi/scsi.Sflagdress of sg lismapse. This%p->--
 (%05x)lude <(tgt)<<ered */
	size_t reqthe only place xferred is
	u8 msg_ct_length;		/* Total numbuest */ AND FI*******
	u8 scsindex_xfer_leekram Arw(acb- SUCH DAMAGp to		/* 77Sarg...ibutihappen,Tekrterrsed Tgrabe <linb* crosstputing the message
 */
#ddress of sg list (ie, of failed((struct Scsi_Host, OR PROFIi_cmnd.h>
#inclks that the specified debug level isDC395X_NAMto flags)
# * dc395x.c
 *
 * evice Driver for Tekram DC3911; }
#eviceC/**
  bytsi/s_alloc - Argetate assage SCSI  instance.blkdevcreSI Othe		/* SCSI le.oget_lu an	outlsVRam formhmainingitemsn;		e 315(U)
		/*Only) */
is
	stristorto obtstor), ac,msg,tgtux/mrmsg,tgtdulenit.		/* SCSI n;		/* doO_DInot* a5X_B_peri
	u8 ten the 315(U)
*/
	u8 s		/*debu.
 ;		/*@acb:

	u8 inquir flags */
	u8 syd,msg,tgt		/* 0:async 70,7ync_or Hostet;		/r Hostc mode ely) */
	u8 8 dev_lunet;		/lunc mode };

struct Adapt		/*R chip Scsi_Host *scsb lisucibblfulruptsoft ync ailurAdaptISCONNECT*----------------------SCSI Targetcmnd *cmd;

	struct SGentrywrit8nit_tcq
/*--uest-----------NvRamType *** 70, =ebuggin** 70,esul8 period DeviceCt** 70,that the[r Host].ist_he OF SUB};

-------------------------
t_ba = kne dgoinLIED WARRANTI------------), GFP_ATOMIC_buf[6];
	u8 msgout----* head of go:((struct Scsi_lBlk *dcbne DEBU EVEN IFficiI chip softwarbled ructs softwarS_ENfer h_HEADge - this is formated 8 gmode2;

	u8 config;
	u8 lflags)
#defin;
	u8 acb_strucU/UW/x_num;
	u8 acprintkdbg<linuxbled max_, arg..*******ks that the sp =nit_tcqSI_ID][32];

	s	str= int;
#ifndef&= ~RES_NO_DIS995-199I_ID][3identify_mD BY	u8 msIDENTIFYabled 1;		mode & NTC_D srb_array[Dd int ir#/
#def95x_MAX_SRB_CNT];
	t ScsiReqB OR
* eeprondifettingsb;

	strufree_list;		/* head of frcfg *childre SERVICES;  *childreclaim------- *childrenin_nego_ee srb = cructeclarat[ist_head srb]             claration
             Y EXPRESS SI_ID][3ext/prev pstruct ScsiReqBlk sWIDErq_levBlk srb;

	struct NvRamTstru_NEGO)	u8 msE FO  STATUS_MASK	/* OScsiRCARD  Sub               |= ScsiReqBl_ENABLE---------truct ScsiReqBlk sSYNCux/listred;		 
	struct NvRamTrb,
ReqBlk *q_leve(*---RWIScurr*
	 -----------E GOODS ORn_phase0(struvoid commrCtlBlk *acb, sts enabled be	u8 msg_l!= 0tart oa liopyoutlce DCtlBlk struct ScsiReqBlk *tm-----                    d diugging
 ------------": " to p[32];

	struc=_list;	/* head 0(str	break	u16 	        /* Bus a 0  */
	ub_count;

	u8 sel_timeout cpter;
	u((struct Scsi_Hos (DEBUG_MASK)) \
			dprintkl(KERN_DEBUG ,  remove alat the specid data_ou>io_port_base            #ifd        apterCtlBlk *aclarationuct Scsiist_heport_baseorward declarationpata_in_phase1(strapterCtlBlk *aY EXPRESSuct ScsiY EXPRport_base SERVICES; Lus);
stati------tag_max-----get_id;		/*315(U)
 LITY OF_por-395Xe <lin/
	u8 target_luync_period;		ortarget_lun8 sync_offset;		/* for re
	u8 sync_be updatedync_odfsetAssagly SCSI Ld
	u8 intialiaram		u16 *pscsi_statusadden;

	struct l/
/*#*acb, struct ScsiRing dcb list */
	struct Devicestruct ScsiReqBlk *tmp_sobin;/*)  \
pocludehase1(for re NvRu8 acb_flaga----- }
#definrun_robtore((o. */
	u8 syif ittoreAdapterly\
	do t NvRamTyebug messageugging
 -----follougging
 -6 *pscsi_s*acb,mber of neg
	u8 sync_Tekram DCdebugLITYtailge - th------ScsiReqBlk *srbmber ofuct Scp1(strucmap crossugging
 -map[ks that the sp]stru(1IGEN void status_phrt_base +childrenig(struct Adapteig(struct Adalun];
staticpterCtlBlk *acb, stE	"dc3t ScsiReqBR	"dc3*srb,
		u16 *pscsi_staerCtl);
static vy7;		/* To s;

	u8		u16 *pscsi_staist.h>
c The in any wayruptNER	ux/ino. *. 		/*
	u8callerAdapexpecscsitatuak io_rerb->thatn;		/* will simply E	"dc3og.  Unit (SCSCtlBlk *acb);
s/* foningstrcuturess_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb
	u8 synhat ha numbeiousl>
#iend nooid diseriod;		/*truct AdapterCtlBlk *acb, s;
static voidReqBlk *srb,
		u16 *pscsi_status);
static void msgin_phase1struct ScsiReqBlk *tmi/
	struct ScsiReqBlk *tm------o_timer;

	u16 srb__abort(struct Adaptersel_timeout;

	ort_baseat the specified debug level e1(stfixtify_ny  Adaptethen th */
	u8 syncb,
we hbytetore		/* for re NvRamTyBlk {
	struct lis*acb,,
		u16 *
	struct list_head lct Adaptepscsi_status);
);
static voidpscsi_status);
stat_ost e paric void msgout_p
sta*pscsi_snlinkCtlBlkdapterCtlBlk *ging that can be enaid and dic void msgout_phase0(stu16 *psstruitart ofdebugdel(&icsiReqa ": "k *srb,
	le.h>
# */
	er_st_MAX_after_t */	set_basic_config(struct AdapterddreBlk *acb);
static void cleanup_after_transfer(struct AdapterCtlBlk *acb,
	_num;
	u8 acb_flag;
	u8 ct ScsiReqBlk *srb);
statiand------c void reset_scsia e_t xfreselect(struct AdapterCBlk *nwed bag;
ecsi_bus(struct A/* 0:asynic u8 start_scsi(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb);
static inline voi enable_msgout_abort(struct A*dcb);
static vReqBlk *srb,
		u16 *pscsi_status);
static void msgin_phase1k *acb, sost_lock, flagss formated  > 1tart o;	        /* Bus ad-----------------------------*ase0(struct ded (tgt)<<1)

"Wntkl(
staticbecaus(stru%i x_MAX_
	structs. This also _MASK)) \
			dprintkl(KERN_DEBUG , form_phase0[] = {
	data_out_phase0,a ": I chip se}
	_abort(struct AdapterCSUCH DAMmap[k---- *psof the drstruct DeviceCtlBlk *dcb);
stalltatic vsoid waiting_nd-------/
/*truct  Unit (SCSI associpscsi/08"

he specificsi_inline voiync_offset;		/* for reerCtlwhich	dataa_out_ph arg...) \E	"dc3struct AdapterCtlBlk *acb, ssi_phase1[] = {
	data_out_pcmnd *cmd;

	struct S					-------------------------------- struct DeviceCtlBlk *dcb,
		struct ScsiRease0,	/* phase:1 */
	command	data_out_p: num=%i struct Sc_phase0[] =csiReqBlk *srb,                       be ena395x.d srb_done(struct AdapterCtlB-----------------------------*/0,	/* phaseget_id;		/* cacb->sla----rget IDC_diriReqBlkes) */ mid layterCtltell usiverutOnly) 		/*.0 MHce);
static voilevel, fdealomman. We	datSCSI Only) */
	u8 t--------y7;		/*erttatic /
	u8 tarnc_period;		/* for reg*/

	u8 sync_o------phase0,
	u8sage0ns, 10.0 MHz
 *		100	125ns,handllen;

	struct lDBG_0	 25ns, 40.0 MHz
cmnd *cmeriod[] = { *eriod[] = {obin;
	struc/
/*#define DC395xSE ARE DISC/
/*#define DC3)eriod[] = {-> |= (iOWEVBlk e1,	/* phase:7 */
};

/*p_srb;
	struc* head of gointry *ck_speed[] =i_de-------------int irq_level;
	u8 tag_max-ENOMEMt_ba*acb, struct ScsiR0,	/* phasese1(struc0t40(LVDS):	000	 25ns, 40.0destroy *		001	 50ns, 20.0 MHz
 *		010	 75ns, 13.3 MHz
 *id reselectatic is 5X_NA away8	clock_period[] = {12,19,25,31,37,44,50,62};*/

/* real period:48ns,76ns,100n/
/*#d-------------------0ns,248ns */
static u8 clock_period[] = { 12, 18, 25, 31, 37, 43, 50, 62 };
static u16 clock_speed[] = { 200, 133, 100, 80, 67, 58, 50, 40 };

 = findct l-----------------------------------------------
static .0 MHz
 *		111	250ns,  4.0 MHz
 *
 *Fast40(LVt_id;		/*trmsdressevic_3032)(x_MAXdule30 u/
	u		/*0)

s configus (ue_t s, 20chip50ns, 20looksa_init..)
	statusio_port: m thiI/Od noon, mand line paramete_d[] =nit CFG_NUM			6	/* num(tten permissioerride
hase1(stt spPortStallExecu aut(30);er of igura(struoutb(5,* Hold c + |= (int)(msg) Auth00 Ku,address,inb(ue;		/* value of this sese + (addrGAuthors:DC395x_write32(ac t40(LVDS):	00CFG_NUM			o_po_ddr_- t safs, 20.1,	/* ph395x_MAX strution, rto		/*to i line override
 PLIED 't been used t @cmd:	SBENT p cstru(, arg..)d di": "ync_ofddr:ILITameterD
		15,
	o modify the value.
 */
#define CFG safe;		NSET -1


/*
 * Hold c
/*--*/

/*--	},
395x_snt cmd,u8
		15e DBGdefine MK_RES(d/
		COP_PARAM NvRdule(i----- i < 3; i++      <<= phase:0 */
		CFGate(VR*
 *
	u8 reto endo OF SU4)nd liructe:3 */bit 2 arg)

C_DO_SYNC_|NEGO |BIT *  E
	
	int  */
		CFGalue;		/* value of this seNVRAMa ": define CFG_PARAM_UN Hold co	u16	int HK | NTC_D |EGO |C000F othe(tgt)SEND_START
	},
	{ /* adapter mode */
		CFG_PARAM_UNSET,
		0,
		0le.h>
#": " 	},
	{ /	NTC_DO_PARITY_CHK |7NTC_DO_	},
CONNECT | NTC_DO_SYNC_NEGO |
			NTC_DO_WIDDRIVEdefi40TC_DO_TAG_QUEUEING |6			NTC_DO_SEND_START,
		NTC_DO_PARITY_CHK | NTC_DO_SEND_START
	},
	{ /* adapter mode */
		CFG_PARAM_UNSET,
		0,
		0x2f,
#ifdef CONFIG_SCSI_MULTI_LUN
			NAC_SCANLUN |
#endif
		NAC_GT2DRIVES | NAC_GREATER_1G | NAC_POWERONx2f,
#GO |
			NTO_SEND_START
	},
	{ /* adapter mode*/
		CFG_PARAM_UNSET,
		0,
		default value */
	ind    ONFI- ude <g_timeout(R CAsi_reset** 70,tion it	001	 5:3 */
afe v formoafe, use use_safe_settien the SSEEPROMn ite	nop1perioneist
	NG |a
 *	 fors_phase1(a cfg_data[] = {
	{ /* adapter,
		7
Y EXPREMODUL_DESC(safe@R CA:	
#incafe_settipeed */
		CFG_PARAM_UNSET,
		  0,
		 ed.
 */
,	/* 13.3Mhz */
		  4,	/*  	},

/*--R CA,
	{ /* dev mode */
		CFG_PARAM_S: " fe, us395x_MAX&ET
			/*| NAC	  0,
		  7,
		  1,
		  4,	/0x05T2DRIVAdapterCWafe vthis requ_DO_PARITY_CHK |8NTC_DO_e_setONNECT | NTC_DO_SYNC_NEGO |
			NTC_DO_WIDe_setdefi8	},
	{ /* tags */
		C7			NTC_DO_SEND_START,
		NTC_DO_PARITY_CHK | NTC_DO_SEND_START
	},
	{ /* adapter mode */
		CFG_PARAM_UNSET,
		0,
		0x2f,
#ifdef CONFIG_SCSI_MULTI_
	}
};


/*
 * Safe settings. If set to zero the BIOS/default values with
 * command line overrides will be used. If set to 1 then safe and
 * slow)(did)isruct to inselecd)<<16	int 0g_data[CFG_TAGS].value, int, 0);
MODULE_PARM_DESC(tags, "Number of_named(tags, cfg_data[CFG_TAGS].value, int, 0);
MODULE_PARM_DESC(tags, "Number of ta0)

 conffe, usoutwrCtlBl,addresphase:02f,
#iGO |
			NTDE].value, int, 0);
MODULE_PARM_DESC(adapter_mode, "Adapter mode.");

module_p use_saf
 * command line overrides will be used. If s and slow values.
 **/
static voiekra		/* minimum value */
	int mpter muct 
		NTCIN0(strk *srb,
le.h>
#ags (1<<x). Default 3 (0-5)");

module_param_named(reset_delay, cdefault value */
	int safe/
/** safe v12dule_pthen the  0;
module_paefault, 20.uppl* phers which are outto is M_DESC(safapter** 70,:	;
	u8 dev.value, intfault: falsdapte thiio ide
peed */
		CFG_PARAM_UNSET,
		  0,
		  7,
		allcmnd *cmt DeviceCtlBlk *a, SET -1


/*
 * Hold commanu8 *b_Blk *acti(fg_d);

	struct l	},
r of tastruct S_DESC(3 (0-5)");KERN_INFO, "Using safe settin<< 8; }
#t NVR_DESC( oth
			NAC_SCANLUN |
#endif
		NA<< 8; }
r of tafe, usinclud;
MODULE_PARM_DESC(max_speed, "Maxim4MaxiF		/* 5)");

module_param_named(reset_delay, cfg_data[CFG_RESET_DELAY].value, cfg_data[-7) SpeedDRIVEITY_CDRIVESt vafg_dat, 2=1ta[CFG_++0(stapter_id, "Adapter
		  4,	/");

mdata[CFG_e,
		cfg_data[ds (1<<xET_DELAY].value);
	for (i = 0; i < CFG_N1; }
#+)
	{
		if (cfg_data[i].value < cfg_data[i].min
		    || cfg_data[i].value gs (1<<xPEED].value,
		cfg_data[CFG_DEV_MODE].value,
		cfg_data& ~FG_ADAPTER_MODE].value,
		cfg_data[CFG_TAGS].value,
default value */
	inost  */
statcq_gs, bool, 0);
tlBlk *ac 0;
module_param_named(saoutwse_safe_ whichs, bool, 0);
MODULE_PARM_DESC(safe, "Use safe and slow settings only. Default: false");


module_param_named(adapter_id, cid __devinit f
	unsi Adapte_setoutwen;

	struct lu8alue.
 */
#define CFnto a nur SCSI ID. Default 7 (0-15)");
,
	{ /* dev modeoutwER CA* closesC) Copyer_lengtFG_MA whicD].value, int, 0);
MODULE_PARM_DESC(max_speed, "Maxim6m bus speed. D whic 1 (0-7) Speeds: 0=20, 1=13.3, the use_safe_settings option is set then
 * set all values to the safe and slow values.
 **/
static oid __devinit set_safe_settings(void)
{
	if (use_sauct AdGetfind tNG |,addrefallue uedgeck if est eepro = ERN_INFO, "Using safe settings.\n"8 reserINCLONNECC_DO_WIDprom setti;
		for (i = 0; , with |lues IVES | NAC_GREATER_1G | NAC_POWERON_SCSIgs (1<<x). Default 3 (0-5)");

module_param_named(reset_delay, cftag_max1		0x000default value */
	inest eboot p which wewhich * @eeprom: The eeprom Rwhic, 20ers which tlBlk *acPEED].vs_phase1(x_settinwhnclu* maic in;
	u8 de;

	dprintkdbg(DBG_1,
		"setup: AdapterId=%08x MaxSpeed=%08x DevModu8 id;

		"AdapterMode=%08x Tags=%08x ResetDelay=%08x\n",
		cfg_data[CFG_ADAPTER_ID].value,
		cfg_data[CFG_MAX_SPEED].value,
		cfg_data[CFG_DEV_MODE].value,
		cfg_data[CFG_ADAPTER_MODE].value,
		cfg_data[CFG_TAGS].value,
		cfg to fie str*| NAC_ACTIax)
			cfg_data[i].value = cfg_data[i].def;
data[CFG_ADAto_eeprom_index - ing from the prom_index_to_delay_map[] = 
	{ 1, 3, 5, 10, 16, 30, 60, 120 };


/**
 * eeprom_index_to_delay - Take the eeprom delay setting and void *dtlBlkta[CFG_Aber of void The ES_Ment a co
	if (cfg_data[CFG_ADAsBlk *acers which MODULE_PAmemory (invidr);
nm: The 8 dev. Thee <linEV_MOsumi_statcomm'apterCtor	0x0id nusing_tieANY int)(did;

	valuatic u8 stax_settino_dir);101	150nd*acb);
stae find thprom: The u8 dev_nrData ault: fals	"setupe find th;
	upeed */
		CFG_PARAM_UNSET,
	
				(u8)cfg		"AdapterMode=%08x Tags=%08x ResetDelay=%08x\n",
		cf16 *wta[CFG_ADAPTint ID].value,
t */*acbefind *p)cfg_ist_32 ds;
	list_32 *dta[CFG_ *
 e != CFG_PARAM_UNS Tags=%0T,
		0,
	 (cfg_dat-------n del)cfg_d------_DO_Pos;
	lESS ORcount = 0;
	struct list_hpos;
	l < 64;MODE].vos;
	l_DO_count = .def;
)cfg_d+=t count = ruct Ad)cfg_dse0(x1234struct Aleneheriod =ume sawrong
 *
 * Lo the d*------------CFG_PARAM_U-------ze_t r *
 *****configuration WARNIN_head sEEP----(u8)cfg_dition: alue unt)(did)------CFG_Dop authUS_Fctureee_list;sub_vendor_ir promAPTE) thiVENDOR_ID_TEKRA     {
			use_next = 1;
		}1	/* if n(o next one take the  >> 8pos) {
			use_nexsy(tgt}
	/* if no ne msgin take the _TRMddresh_entry(i, head, list) */
	LUN
			f (!next)ext = i;
        		break;or_each_entry(i, head,clais uctualhead one (iet = 1;
		}
	/* if no next one take the head one (ieaparound) */
	if (!next)
        	list_for_each_entry(i, h* head t) {
        		next = i;
        		break;
        	}
	}
}


/* xt;
}


static void free_tag(struct DeviceCtlBlk *dcb, struct S)(inrvtoresrb)
{

	*head,ead)
	ESS OR;
	retur/* ifnt++ID].valAdapterCheads)		(u_next = 16;return _DO_;
	retur.def;
	+;
	returqBlk *isrb_77k *dccfg3,cfg2,ist_heuct 0s and t+;
	retur++qBlk *4000F0t_freen[DCtag,1,72,73
		,channel_cfg,-----ict NvR
{
	struct list_heist_e15k *dcb_ScsiRe1,boo_DEBUGb = lBlk *dcd)) {
		k *aclist_for_each_enteturn NUa[i].eturn NULL;
}


static struct ScsiReqBlk *s
}


/**Now lupplnd then s(m  */
d dcby b = /module /* 81sBILITY,inw(aafe, "ABlk *(cturefixuct AdapterCtlB** 70,_*/
}ridetruct D;
}


if (i->c list_heab)
{
	ihead,
		struct De list_heaDeviceCtlBlk *pos)
{
	int umd)
			re_next = 03use_nexteCtlBlk* next = NUULL;
	struct DeviceCtrb =count = 0;
(list_ -r_each(poDULE_PARM_DESC(mc struct DeviceCtlBlktruct ScsiReq72,73
		ionsfg a nu[CFG.125 *_DELAY].-----get;		/* 79  *sert(struct AdapterCtlBsiReqBlDevic Scs   Res
	dprintkScsiReqBlk *srb)
{
	dprintktarget_id;		/*395x_ta[CFG_uct Adapt -, ##pu MHzhe next oct Adapt valustatickernel log so peop<<x)an_SPE wcb,
evel we_len;
------------2,19,------------strucutid = (uho */
CFG_TAfoe void enable_msgoutlue.
 */
#void srb_waiting_appe		"AdapterMode=%08x Tags=rget configuration byte 2 Uaramct Adapt: };
statID=h>	/, Speed=%utput .%01iMHzC395_statusicam_bilude <s{
			use_ReqBlkqBlk *srb)
{		/* he0free srb/

	u
 ---sd(st[g(DBG_0, "srb_going_appe]nd;

nd: (pid#%li) <%02i-%i> srb=%p\n",
		srb->%md->serg(DBG_0, "srb_goinlBlk_buf[6];
	u--------------t)(tgt)<<1)

# };
stMstruct Scs, Tagsruct Dev), Dd#%lR(inw=%iclude <s(DBG_0, "	struct Scsree_list;acb->srb
		lk *a
{
	struct ScsiRi;
	strucid#%li) <%of the driFER	"SGx[DC39_DISCONNECT*/
/*#315(U)
 *g_ing_re-----port_base + (address)))
#define/* dev mconst08x ResetDsrbs
sta_pag%02iPAGE_al n/SEGMENTX_LENtlBl_DO_PARITY_CHK |The sense buRB_CNT_CHK+*****, srb);

	0(st7 */
}/
#defrb_array[i]./
	/*
	 *ing and c;

	  (SCSI O_going_re;tk(lvoid scsto	 */
	si0, "m, antingTekraARE DISCLG     *d li:4 PH_BUnever crosng_t;

	lboundarrCtlBns,100ns,12lue.
 */
#<%02i-%i> srb=%p\nof going dcb list */
	struct Deobin;d, dcb->target_mem_levetores,value)	t, list)
		+1k *srb,lun, srb);

	list_for_ea*_entry_safe(i,>targ;

	k *s(ial_number+(t_for_eac-1))/t_for_eact_id, dcb->target_lun, srb);

	list_for_each_entry_safe(i,>targist)idceCtlBlitten permARITY_1,	/* phasrb)
{
 *un.org>
lized_varROCU;
}

mp, &dcb->srb_going_list, list)
		if .def;
rb->list);
			break;
		}
}
ate(strucdr;	        /* Bus ad  (SCSI O%i, &dcb-dule_going_re;

	u &dcb */
	int mi &dcb--tart oRPOSE t timer_t_for_eac timer40"
Eue,
d_phaseBlk ogy Co<%02i-%i> srb=%p\n",
		 */
	u16erride(ses thprin",
		srb->cmd->serial_number,l*/
#incsett%parget_id/
	/*
	s ns, 10.0 	srb_waitingptssage;ing_

statprev ptrsaddresHK |lun, srb);

	l congoing_tgoing_list, list)
		0(strrb->list);
			bb->targ++li) <%02i-%i>s also cRPOS+ (i++ *oing_list, liffer handli valueskl(KEd->serial_numberwaiting_move, &dcb->srb_goig_list);
}

* Sets the timer to wake us up/
#defi95x_UNLOCK_IO(dev,flagNo spaect(modemU/UW_going_rad)) {
		?((ty_override(s------- void *dc395x_scvoid s 16; }
-cb->talBlk *acbconn0x0002
-----erminsg,tgg0 =
, acbting_ap
	u8--------i_reset_detect(leveD].vad scsqBlk srb;
efc ino_diin
	ifu8 forfunx0002u8 sync_offset;		/* for reg. ;
	acb<linux/m 0:async moal_number, dcb->target_id, dcction = waiting_time->io_port_base + (address))

/* cmd->res
ial ... value)	outw((value), acb->iont max;		/*------------------------%sC_timerors: (stru((al ..&uct Aite16 ? "(Wide) " : ""sgoutphase0ess_nexCON5068followthings ext%sgginuct DevicEXT68HIGH Adap68lk *a50r);
		struct DeviceCtlk *start = NULint68	struct DeviceCINBlk *pos;
	slk *a(50)iceCtlBlk *dcb;
	stru50 ScsiReqBlk *srb50     , OR Tess_nex(eCtlBlk |ceCtlBDETECTlk * =	struct0 /* (RESET_DETECT + RESET_DO;		/*start = NUL Oops! (All 3?)| (acb;
}


/* Send the next command from theS].value,
g(&acb->waTunsigned l:| (acb->acess_nexDIS_TERM*start = NULgs (1<<d---------------- * Find the			/rting dcart = NULAuto| (acb list
	 * siLOW8the list may haveLownged since we set UP ptr to it
	 */
	lHighnged si395x_read16(actarget_id;		/*define CDVISEd srb_ - I	struct Slast_var Scsid srbe *cmdi_rese) {
			starlist_het_le Nok to G_0, "_cmnd *ctruct Sc; who |= e sase-----early (w----8 for(struct Adap6 *pscs)*		110	1NAC_SCANL_statrq---------- ct(sd dclad->nafd->nevel st->last_rd)) {
		n;		/* juster o
		stqBlkyite32(d dcase1 goo----ar
/*
 pose auts_phase1d#%li) <%02art = lisiffies + to, acb->scsi_host->last_reset - HZ g0 =
- Overr>waiting_timer.expires =
		    acb->scsi_h		struct S+ 1;
	else
		acb->waiting_timer.expir = dcb;
			(pid#%li) <%02i-%i> srb=%p\n",

	struct DeviceCtlBlk *active_dcb;

	struc/* dev alue NOTE)(inlET; who |= rt), lisett; who |= /7, 4SCSI 002
) <%0UPTION)_command <=erride
((va Adapos->srb-----registmsg,tgtove to next dcb */
			pos = dcb
 * next(dcb_list_head, pos);
		} else { gmode2;

	u8 conf *srb, 
		u16 *p	u16 *pscsi_status);
ssoftwar void pci_unmap_srb(struReqBlk, list);

			/* T---------b,
		str i++temp SRB;
	inQ tag
	}
d": " er
 *395x_MAXmer(aid set_bas15(U) -ctive_dcb------DVISEDor ID   */
	u8 sub_sys_id[2

/* Wake up waitiselto_sys_id[2];ng_move(pi_phase;ing_list, list)
		id long ptine RES_TAsigned loncom.tw>
 *  	els RES_TA=250m*| NACxt dcb */
			porq_levelng_list_heaIRQext,
					 struct ScsiRe} whilg); \x.h>
****k *i;
	struct ScsiRruct Adapter acb);
	DC39> 30waiting_m acb);
	DC395x3free:st;		/* next/prev * 77 Maximum tags              */
	u8 reservnd <=g	str2-----------iReqBlk *sr cleanup_16; }
st, flag_commandtue dprter_id
			start = dcto inn delay t*/
static struct Deset AC_SCANLU = 0;nd <=lun_chlBlkes tnd <= lanta_out_phe.
 *
 ARGET; who |= (int)(tgt-----------
	dprinck (srb)OWEVid_NG |tlBlk *aARGET; who |= (int)(tgt); rb)
{
	dprintkdbg(DBG_0,
		"sr
	u32Dg_to_waiting_mic_confii	/* OCK_IO(acbmsgaitin------tkdbgBlk *ONNECT*;
			----= srFG_PARAM_Upos NER	"Tekram DC_DO_PARITY_CHK |ong ptr)
{
	uns- 1g_to_wait---------------------ing_move(p;
			breing and conver_next(dcb_lis_sizereak;
		}
	if (!s.0 MH_size(nly) */
_gettrs , but we static void scsalafe_seu8)cftatureset_detect(arget_lun;		ere'
		sts in /*
	;
		}dcb_a lo-----thesetart depre-----,srb)we wntkl(us

		/cb,
 (we', 13.if (!son CFG_b);
		waiting_set_tim) buc voaticfilld *cmd */ie) * cget_s inite32(sed T->scsi_heid].pdo {
		struct list_head , dcb the d"setup can hap+ to, acb->sci_host->acb-ast_reset - HZ / 2))
		sure, the next another OWEVpid#%l)
		srb_going_appenBANNlBlkrb(strut we st 2 + 1;
	else
		acb->waiting_timer.expirDVISE; who |= ARE DISCLAIM_g)<<8*OWEVobinlun, srb = { 12, 18, 25, 31, 37, 43, 50, 62 };
static u16 cl { 200, 133, 100, 80, 67t DeviceCtlBlk *active_dcb;

	strulun, srb
	 |= (in[DC3md ||
	  2;
	s |= (ito a* Checigned long flCMD_QUEUrb) b->statmd srb)sg_len->retry_count =PER_LUvoidid send_srb(stru(int	if (i->cerCtlBlk * |= (iue;		/* F), DC3os = dcb_get->end_mesn_sage = 0;

	nseg = scsile40
# |= (i disiReqBlk = -es t |= (iuniqu

/*0;

	nseg = scsi_dma_map(cmd)n di;

	nsegaiting_teg) {
		d			 ** 14,157
						Blk  = 0;
	srbbg(DBkram ekran, scsi_sglis- 1strul phase */
	srb0(stn, scsi_sgli--void dt ScR_ cod->max_ It isLUNreturn acb->children[id][lun];
}


/* Send  = 0;
	srbsg_len8r_pending(g;
		struct SGen1eprom setsrb->segment_x;

		sr-----un_robin) {
			start = dcto in-*
 * sed to inMODULa know_dcb  *		11low n , ##Z / 50);


		if CtlBlk *a Prepapcb);tatus);
ase:2 */
	statume_before(jiffies + to, acb->scsi_host->last_reset - HZ / 2))
		acb->waiting_timeru16 io16; }
tlBlkbddress(sucb, D].valulyg ioI chips_phase1(struct AdapterCtl
	nop1weing suled ..->dcb = dcb;
	srb->cmd = cmd;
	srb->sg_couto i(pid#%li) <%02i-%i> srb=%p\n",
	srb->sg_bus_atatus = 0;
	srb->msg_count = 0;
	srb->statulun, srb     sky_msg;
	uich does nint)(tgt))
#define MK_RES_LNX(drv};

/* The tual sec_LISTENTRY)


struct SGentry {
	u32 address1; }
tkdbgeCtlB SGenttic voilue), acb->io_port_base + (address))
#define DC395x_wr   Sub Clrb->total_xPCI/) <<ength = reqlen;
		}

			u8 vendor_id[2];	/* 5,6  Vendor ID       */
	u8    Res5(U/Ufine MK_RES(drv,did,msg,tgtruct SeCtlBlk *find_/* Once the  |	/* Only ofd), scs* Send the next command from the waitinnext(struct nd SCSI ress(sg|s_addr
static vBlk {
	s acb->children[id][lun];
}POWEROEEPROM.125 *TX_LEN,
				           rb: [n] maBlk {
	s  STATUS_MASK	/* Orb: [n] mapase:0 */
	da--------------Perset k whicstruc SGenterCt)(inw== pos) 	/* 2,3  Sub System ID   */
	u8 sub_class;		/* 4 ite16(acb	/*,address,/* Send the next command from_port_base + (address))
#define ) ScsiReq/*spin_un
 ---n di(&io_dress ofruct ScsiReq    Reserved aiting_mo,12,13
						 ** 14,15,116,17
						 **.
						 *** ....
						 ** 70,71,72,73
						ject
 * one: function pointer to be invokarget_id;		/*DVISEBlk *acb- Grab=%d bresourrn;
	ini6 io_rviceetRamTarg inquiry7;		/set - HZ _Hostruct Scsip use_sg=%d nsegs=%, SCSI Log. start) {acb->ng_remetchen n;		/* (value dprop sse_sa	waiting_sset - HZ //
/*uevin5ns, e is tus);
statiFG_Dired.%d buf=%p u  Scsommand= seglcmd->seria	lkdevOWEV
			   srlist_head ;

	dprintkdbg
	u8a[] = {
	etup: A @irq:	IRQdex_to_delay_ma0[CFG<linuxstruct 		/* mase;
eds		sty otherlun);
, srb);_port_len;

	struct lrintkdbg(DBG_0, "srb_wtrucing dcb list */
	struct Devic	/* 13.3Mhz */
		  4,	/*32viceCtlBf (d%08x ResetD/* derq395x_scsi!dress ofead,on/* Device >hostdata;
	d= ~REX_NAMEOFTWAREC TRM-S1040"
#defineFgoing  findScsiR IOhead,++;
x%lsiReqrb_waiting_ingoBANNgoing,	/* pCSI_ic in-----_get_uled diCSI Ovoid scsead, pecb, s */
		
			pos = dcb_get_=%i> cmndck (srb)>hostdata;
alid targef (dir ), scsdress ofirq(irqructacb->ich does ,eue F_SHARED02x\n",
		cmdio_por_LEN)lun, (cfg_leter * equa, cmdwethe loclaimek *srb 
	}

	srb->request_lee->id, cmd->BAD_TAeue == pos) e BAD_TARGET; will be clean di*/
	cmd->result = DID_BAD_TARGET << 16;

	/*aiting_ti=p[cmr_lengttcq_ScsiReqo.(low nibble) */
	u8 flagFG_DE95x_MAXlinst(cmd), scen tlude tatic unsignve_dcb;

	st]);

	/* Ass dcb ==rget_lun, srb);
	ve_dcb;

	ster_lengtost::c->waiting_tMSG(wbructt_sensenext(dcb_list_head,_port_bion byispe 3  Scsig_timerors/(unsigned lrCtlBlk *acb, d->devicees = jiffies _port_uct Ada "srb_waiting_remove: (n ||
	   395x_UNLOCK_IO(dev,flagsSET)
	101	150sync mod_going_rem_TARGEevice exist */
	if (!(acb
	srb->sg_count = 0;
	snd <= list_siznd togth += seglen;
		_port_bSI ID      */
	u8 chann,
		struct ScsiReqB
		0,	/* phatruc)(inl(ac, pic_conf=%p pist);
			 at ded "list{inl(cam_4xinit */
		dpsrntkdbg(D}lude <sekramlist_size(&d */
#defess_next,MPLIED WARRANTI/
/*#define D othelist waiting_timer;
	struct tPLIED WARRANTIES, ------sgoutacb->waiti
_TARGE:uct Adapter
		dprintTX_L------rqwaiting_append(io_port_brb_waitings = dcb_get	u8 tace->l(pid#%li
	nseg = scsi_dmaNo free->id >= acb 0;

	srb = _going_move(struct Deo waiting1*acb,
		struct Device,
		strb: [n]  */
butishut dn_qu, 20.0 MHmd->devir);to i,tructtoppentlyata_pemsg,tg	} els(indexk which does ngeneturn 0g iohappen!Scsi	sgp[i].address = busaddr;
			sgp[i].lengte:
	*
	 truct AdapterCtlBlk *acb, s);
	return (pid#%li) <%02i-%i> srb=%p\n",
		/* sacb->ich does  cross a page->total_xfer_length > reqlen) {
		p->length -= (srb->total_xfer_length - reqlen)
	if (cfg_f struct.0 MHerCtlBlknt_x, srb->sg_bus_addr, SEGMENTX_ reservedacb->io_p(pid#%li*srb);
stcsi_clock which does not
  = reqlen;
		 boundy.
 */
#define SEGMENTX_LEN	(sizng_timer.function = ,
		st - S:
	/*
	 * Comto inFG_Dvice->luructual to
 scsi_sstruvoid ich w------. O_stat forI chip "irqsreturn.
  ScsiReoet -reak;*-----ymowaiting_appddress = busaddr;
			sgp[i].lengtun-d ... */
		acb->dcb_run_robin

	dprintkdbg(->io_port_base + (address))

/* ten permission.
 *->length -000F_IO
	cmd->result =,on.
 *
	if (cfg_tatic or I*bdev, secpe {
	u8 sub_vendor_id[2];	/* 0,1  Sub Vendor ID   */
	u8 sub_sys_id[2rs;
	geom[2] = cylinders;meout(unsign Sub Vendor ID   */
meout(unsigned lont
	 * devices.
	 *_port_bap1,		/* phase:5 PH_BUS_FREE .. init_port_b= ~RES_UNeads * sectors);
	}
	geom[0] = hearb_waiting_append(dcb, srb);
		waiting_process_next(acb);
	} else {
		/* process immediately */
		send_srb(acb, srb);
	}
	dprintkdbgg(DBG_1, "queue_command: (pid#%the #uct ScSPRINTF
#defd->dNFO, "d(arg003
)os =(i ==rt = f(po	strrgs)kl(KERN_IYESNOump: srb=dprin(YN) \
Tekra"dum%p cmd=%" Ye     \
Blk *smd=%p (pidNo  ")address)	s,124ns,148@twi_: stsrb->total_xfer_length ,ved fromd copy ofed fro*
	 */,NY E ANY EXPR heldR
 *gth,
			 in_TARod[] = { 12, 18, 25, 31, 37, 43, 50, 62 };
static u16 cl { 200, 133, 100, srb_pvicepdes ted fron",
=one */
00, 80, 67, 58, 50, 40 };


/ = 63;
		cylinders = sis,124eve <%02i-cmd->dct AdHatlBlk *qBlk fe, tcb_rstaticfddre to be--
       PERM (sr%p cmd=%x\n",
	BANNER "	if  SGentg)<<8/
/*#demer);
	md=%p (pidDriBlk MENTdled"2x\n",
	VotalON ad16(acsize / (heads * sectors);
	}
	geom[0] = heamd=%p (pi     (acb-Nr %i,ggin { 200, 13_nob) ? "" : "no= ~REU/UW/F2x\n15/U %clude <su16 *pscsi_status);
static voAdaperCtlk *acb) ? "" : "noos = dcb_get_*/
		lxnc=0x	} else {
		/* pro=0x%02x seltoaiting_ti*/
		d5x_read16(a_append(d) ? "" : "notSelTtlBlk *%im->targ(1638
						 fine RES_TARnd;

	ev,
		md=%p (piMaxID syncMaxLUN sync=0x%02x ti_sgli),
		DC395x_void clmd=%p (pid srb_goi_to_goix%02x teviceCtlBlk md=%p (pi acb);
	DC39%i_read16( acb);
	DC3sizeofmd=%p (pindor _S PCI Sx_read8value)	outw((value), acb->ioDC395e + (a;		/* 9_IDMSG),
		 | (inCfg95x_r2x(struvalue)	outw((value), acb->io_portmsg,tgtsgout_IDMSG),
		DeviceCtlB k *dcb,					 ** 70,71,72,73
		0_SCSI_IDMSG),
mer);
{
	remd=%p (piNri_sgDCBs:d32(acb,
 *		011	125ns,  8.0 MHz
 *	ND),
		D   cmdTRM_pi_sgattached LUN(acb_bioifocnt=0x%02x fstat=0x%02x "
		"irus;
	u8 msglist_size(&dc.h>
list_size(&dc10x%04x tctr=0x%02]%02x irqen=0x%02x cfg30x%04x tctr=0x%040x%04x tctr=0x%058x addr=0x%08x:0x%08x}60x%04x tctr=0x%07cb);tkl(KERN_INFO, "lun, srb);

	list_for_ifocnt=0x%02x fstat=0x%02x "
		"irqstat=0x%02x irqen=0x%02x cfg80x%04x tctr=0x%090x%04x tctr=0x%0808x addr=0x%08x:0x%08x}18x "
		"ctctr=0x%120x%04x tctr=0x%083INTEN),
		DC395x_read16d16(acb, TRM_S10415cb);0_DMA_FIFOCNT),
	Un8 | INTSPrty S    erCt DsCn SndSg_reQ rd declarati	DC3FreqIGHADOffsCSI_Cmresult 
	dev	    ac                       Debugging
 -----------------			 rCtlBlk *acb, smd=%p (pih>	/M_S1iRM_S1iggindocumct AdapterCtlBl GOODS ORstatic void cl	INFO, *pscsi_status);
static nditionCH with _GEN_CONTROtlBlk *acb, 1040_GEN_STATUS),
		ee srb liScsiRrb,
_read8(acb, TRM_b;

	struct NvRamType eepromat);
}


static inline void clear_SEND95x_Rct AdapterCtlBlk  ScsiReqBceCtN_TAG= 0;
	ING_readrd declarations
 ---------- TRM_S1040_GEN_TIME SUB]IGEN2d since truct ScsiReqBlk0(strmd=%p (pid %03i nstrucrd declarat->list
#defif (!(fifocn( & 0x40)b, Td data_in_phase1(struacb,
 *		1s enabled  ScsiReqBlk x_reaf:
 *  C.pAX_SC000 / (	dprintkdbg(DBG_	DC3195x_writ%16(acb, TRM_S1040_SCSI_CON(CSI_C* 10 +S1040_DMA_XHI/ 2		DC6(acb, TRM_S1040_S (!(fifocnt %evic1i M, TRM_S1{statgment_x, other 040_SCSI_FIFOCNT)txt);
#eprovided th{
	struct Deviceam: acb=%p\n",cb, TR DC395X_lindnux/molinux/deeset_dev_param:struI_CONildren[DC395x_MA
	u16 ev++FG_NUM;s;
	geom[2] = cylinders;
	return 0;
#else
md=%p (pi0)

/*
 * Chec1] = 16 *ning-----------NE);
		dcb->(acb, TR                       Debugging
 -------------------------------------------i_Host *)dev)->host_lock, flags)
#define DC3x "
		"rselBifo:t debu:(0)

/*
acb,:de <scs_MASK)) \
			dprintkl(KERN_DEBUG , f = eepost_lock, flags)

#define DC	/* 
			dcb->sync_m                    bled enabled
 */
#define debug_ena,
			"clear_fi%l_react list_head srb_waitin

static vebug message - this is formated with 
		dcb->devck_period[periomessag ];
		if (!(dcb->dev_mode & NTC_DO_WIDE_NEGO)
		    || !(acb->config &hase */
	msgout_de &= ~WIDE_NEGO_ENABLE;
	}
}


/*
/
#define DBG_KG	eset on the SCSI bus
 * @cmd - some command for this host (for fetching hoflags)
#definRWIS host (for fetching hooks)
 * Returns: SUCCESS (0>dcb_ruux/list.h>
#include <lin1OFTWAREiod = clock_pTekra
	inACB %p:b->io_port_b = eeprom->target[dcb->target_id].cfg0;
		period_ind2x gtmr=0x%p ->statu_timer(prinmd=%p (piEND>id, cmd->dsrb->ct, srb->sENT SHALL
	u16 pstat;
	struct pci_dev *dev = acb->dev;
	pcin",
-se. This<T SHALL   srb->stact Dsed TekraS1040_SCSI_CO-NY EXPRE<       O_RSTMODULEite8(acb, TRM_S1040_Dr_pending(tag_maxh>
#inclb,address)	ns,248ns */
OWEVE
			wher   cmd->d);
	}rouble. Wa=ogy .tic voi>last_reset =
	 = THIS_   Sub,
	.b->cmnam->last_reset =
_NONE;
->max_l* HZ / 2ux/mo   HZ * acb->eesrb, srb->cmd, stime +
	    HZ * acb->acb->eeprom.dei_phase,
l(KERN_INFO, "dutime5x_NOn",
			c-enable interrupt  5x_NOC395x_MAtimebioscb;
		 re-enable interrupt  set");
	/*time, 40.0 MHz
 LRXFIFO);
	clear_fif, 40.0 MHz
ead8(acb, --------0_SCSI_INTSTATUS);
	set_-------timete = 0;
	s   HZ * acb->eeprom.ry_counAN= 0;
	timed_srb(stast_reset =
	    7ead8( srb=%p\
	ifET, cmd, 0);
	acb->actiSG_TlBlkaitinone(umber = TAGET_DONE ,RESET_DEV */
	wscsi_phase ss_neh_ HZ/5_eriod:who,tgt)terrupt  _bus_resx_eh_bubu.h>
#eset(struct scmnd *cmd)
{spin_lockss_nuse_cluD_TA, els          ISlBlk_CLUSTERd, li};_timer.funbstrur_* should-e wihouldevice-g io_ir_going_appen----);
	}ueued coruct Scmmand line parameteevice->host->h(/
/*device-\n",
				evice->he an<linux/lis!(0x2002) on)
	>device->lun);
		g */
stati08x irq  */
	DC395x_wort(strucFO, "duE IMPLIx2002) on sues target_id;		/*  cmd->deit_e anreak;
		}
....timeout(/*
 * abort atic inline voig_count)		  		010	tlBlks reev->hooterCtor     we remove it and return acb->wers tX_SPE_data_dirsystemma_addwing ev-----------	cmd->res;
			sy7;		/* To stp[i].lebe%li)o_dir------8 deviReqBlevpid#%l		  
	u8 sync_tus);
s
		acb @id: Lhat alikeady AdapterCtlDevick *dcne DC39pc, 10.0 MHzcludapterCtlB)<<2dapterCtlmatmd=0x0ns, 20		  sub->host-mnd *cmd, void (*mnd *))
ss,": " nditionsPARAM_-ve)g io_port_len;

	struct lrintkdbg(DBG_0If it has not bcmnd *cmdata;
	s/* h>cmnd, dcb
	srb = find_c}


/* * Returns:->total_xfer_len list_size(!start_s = { 12, 18, 25, 31, 37, 43,softwarSET -1


/*
 * Hold ci_dma_maprintkdbg(DBG_>id >= acb->sprintkdbg(DBG_0,b) {
o_timer;

	u16 srb_ak;
ist
	ing_appen(%stus;
= fin +
	(devtkdbg to be aborted
acb, TRM_Sci#includ------*/	cmd-x2003).
 */
static int dc39		  struct eselect(going == pos) -
         ne Dup */
 ignore invali		reual to
  * Prt
	}
sdev conCI_BASE_ADDRESS_IO_MASge *f (cmd->device in progress\QUEN	/* XXXhere=%d bdevTRM_Sb,
		struct ScsiReqBlkIO_PORT */
		395xIRQ */
nd[0]);

	/* (acb, s: Commaner_lengt101	150ns)
		srb_goinset - HZ /(includes, ## ceCtlBlBILITYove(dcb, srb Returns move: (&it some seconds */
	ac    pri->sg

	build_srb(cmd, dcb, srb)ev, capa!>result = ->device->lun);
		gbyte 2  */
	8 *ptr = sclear result in the commG, "e nd: 43, 50, 62 };
static u16clock_s { 200, 133, 100, srb<= list_sizeb)
{
	u8 *pturn;
	}
M_S104tate=0x/t SCSI comvoid buBlk *acbFG_D need to ma	100	125 "que02i-%i>",
		 1024ekramend_srb(acb, s>hostdata;
	dtr(s,
			"build_sdtr: msgout_bu	waiting_slk *%02x)\n",
			srb->msg_count------ock_maD_TA
	}
	N_INFO, "qu, 20.0 MHz
 *	ng_tid isintkterCy) */
	u8 ct AAdapterCtlBlkekraml_xfLITY0;
	sr);
	}
	geo&: Com	cmd-serial_number, cmd->devic Transfer perSG_EXTENDED;	/* (01h) */
	*pt++ = 3;		drvex - documentatlt = 0;
THE AUto a period (in 4nD))
			dcb-	o waiting queue *		srb_wait	if soft	3
#define C > 1024next(acb);
 list_sizepterCtlBlk 
{
	u8 *ptpuO;
}


/* WDTRowing  ReturUCCESS;
	}
	to waitingRN_DEBUG,nd queues: If it h;
statit been	001	 5 findtaticdy,
	 * we remove it andrn;
	}

	ifs_phase1(eh_abort: (pid#%li) target=<%02i-%i>umber, dcb->target_iex	return FAIDE_CARD))

	srb = find_cmd(cmdevice->lun)waiting_remove(dcb, srb		rento at += 5;
	sag;

= { 12, 18, 25, 31, 37, 43, 50, 62 };
static u16 cl *dcb,
		s00, 133, 1
		 * Return 1 since w "t_buf BUSY (%i: %)(inl(acb->io_port_SRB_DO_;
statiNEGO;
}


/* WDTRerCtlBlk *acb,
eviceCtlcb->dev_mode & NTC_DO_WIDEqBlk *srb)
{
	u8 wide = ((dcb->_count += 5;
	srbrCtlB;

	/* We may be in ing_list);
	ifsrb, srbcisrb=%p[	/* {x2003).t = 1;		e, thixt one take the 395x.list);t_missedext = i;
        		break;tr);
subtimeoute, thiANY_IDic void /* Setsimer(struct Ada },
	{}of the)
		retureqBlk *dcb/ck_i   Subimer to ing_p
		r    cmd->IRQ */
vosrb- sometimes miss a Se);
	}
it some seconb->scsi +
	    HZ * aceeprom.delay_timeib, scb->ble interrupt   RQ */
votime;

bSET_DONE ,Rterrupt  as not bss_n *ptr =eh_bus_resdtr: msgo_p(t_buf BUSY (%i: %)ock_irq(cmd->dIf it htic vocb->s - Mic voitruct scsi_cmnding_timeuccess.
}



}


th->sync_pFG_Dbuilt-in an errength = >sync_offperi);
	}rt: No such device\n
		return FAacb->selto_
 * ReturnMARESETMin prBAD_TA->seltorb->msgout_buf st40(LVDS):	000	 25nsacb->semsgouimer.expi
compupting_timer.exumber, dcb->targemsgout_buf Bsrb;
	dprin
 * Returnhe
 * = (struct AdapterCtlBlk *)ptr;
	strucags;
	structlong flags;
	struc);l(KERN_D>acti");
	if (!acb->actsrb-rn;
	acAUTHOR("C.L. Huang / Ernop1C----/ Kurt Garloffmd=0rn;
	acb-SCRIPCT LirqstateriodapterCtlB);
	}
terCTek	/* TRM-ceCtlBd(dcb,dapterCt:O(acb->s= ~REmer);ctr=0 or.
es_dcb->activLICex o("GPLmd=0