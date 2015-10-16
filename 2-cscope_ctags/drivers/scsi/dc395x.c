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
 * Dsrb->adapter_status = 0/UW/F), DCtarget * PCI SCSI Bus return/UW/} else if (Adapter
=er
 *_STAT_SEL_TIMEOUT) {Bus Maste315(U)/UW/apter
 HU/UW/Authors (SCSMaster Host A15(U)
/UW/(SCScmd->result = DID_NO_CONNECT << 16 set used T *  C.L. Huang <ching@tekra* (SCSSET_RES_DID(>/UW/ (C) C,yrighERRORU/UW/F<garloffMSGse.dee.de>
 * Masteend_message0 Kurt GarloffTARGET.de>
Oliver Neung@te);
m Te
Technology C/*
		 ** process initiatorc@web.d.03
 U/UW/License: GNU GPLleneh/
		ing@tekra.L. Huang <ching@te/UW/t Gam ASIC & H_OVER_UNDER_RUN:.de>
o., Ltn <erich@tekram.com.tw Garloff@su
 *  Oliver Ne 199OKkum.name>
 *   *
 * Lac <aliakc@kum <o <ali@nn, ar.nechnolo*****aste*lowing cPARITY99-2000nehanose in source and binary formsnditiowith or ode out/UW/modification, are permittedn@twvided tha{	/* No error ****  C.L. Huang <ching@tekra* (SCSh Chen <erich@tekram.com.twd Ten source and binary forms,t ret  Jam

lowindir !=ng@t_DMA_NONE && scsi_sg_count
 * ))
		pci_dma_syncionsfor_cpuat t->dev,ource cnlis the f,UW/F		entatioseprodd/or otdir Olivckc_onlyme>
 */* Check ErialsCmentatioss an ay noistre******  e.de>cmnd[0] TRMINQUIRYht
 meunsigned char *base = NULL/UW/struct ScsiInqData *ptam.coittederivlong flager
 * (SCS retaiscattern an* sglowintcopyrepro/or THIS ize_t offseCopy0, lenlowiizeof(t retain specove   Olivelocal_irq_save(n./UW/IS''m this THE Akmap_atomifor TsgumeHE AUTsce, this lwi&Y EXPR, &lenE IMPptr =****RANTIES, INCLUDIpr)(LIED +NR A PA Olivet Ga! be ibuti&& end *   *  (o& n sourc)omot0 *
   LE FR ANYemust2romot0f cmentatbufflend/or  >= 8INCIDENTALnotice,IED WAlisANY  coRPOSE FOptr->Verlowi0x07L DAM2ollo	ditteinquiry7 =ED T->F, THe.de/*if(lowing, SPECIAL,pEXEMPeOR BduO, Plowi/e>
 host_byte
 *  Oliver T,
 *above  || (C) 20ER CAUSED AND ON AN& CHECK1995DITION) )s an**t Ga
 *  Oliver omot(*ARRAORhat tE GOy notY OF LIABILITY, WHETHER IN COOR OY WATRACT, STRICT LIcts
 *tUTHORDS OR Sit_tcq_O, TFTWARE,	add_devat the3kramptvideSIBI IF ADVI WHEOFARRAN= 1U/UW*}p) 19S'' RANTIunES/UW/OF MERCHLIED*
 * T NOS SOMIrestAS IO,ARRANIMUPTION)Here  and/orinfoe ab Doug Gilbert's sg3 ...RUPTIRRANTUetncluidOR Aon, aretotal_xfer_leng******/* TD WAmayduceuxterpreoticby sb.mustnot<lincl/  OliveSCp.ED Wiinclduallowing cype.h>
#h>
#ine <;
#in#incluiONSEerpinlock.h>
#i* (Sittenebug_enabled(DBG_KGOFTWARE.
 *
 folx/spinlock.h>
#inl GOODprintkdbgde <lin, "srb_done: (pid#%li) <%02i-%i> "*****"CIAL=0x%02x Mishnol%i LITYs\n"otherendorsserial_number,L, SPEdevice->id.h>
#inndit/scslunce.h>
#inclTS; OR incluctlude <asm/io.h>

#inu OR Pclugoe Drre * dcDAMAGc *
 * /* Add to free HOR ux/dec.h>
#iomotrittetmp_defindevice.h>l(KERNns
 lock.h>
#inscs
 * ! Compl#inclcmSS Fth ang < -\n"*
 *e Lenehanvice.h>
#incli.0ine DC395X_VE-------cam
#in>
 *  (----_8xC395X_VE------_e orpinlock.h>
#inHER IN IS'' rb_NER	_insertat thedefine }
owin BY T**), D-----------lude <l*    ne <``ASE IMcsi/s Dr@twibbl_nex-----);
}
/UW/3abort all04/0le.o our queuehor m(C) ic void dX_NAMk.h>
#inARE DISCAang <cCtlBlk , OR mu8 did*****e <s S/vmallosi_CIAL,*

#def8odulce)
{
fine. HuD-------------dcb;
IComot-S1040"
#INFO, "de--------95x_: pids ----
	HOR orm teach_entry95x"
#&rittedcb_HOR ,"at thts
 *O_TAGQ*LAIMReq-----sr------------------------tmpIS'' ne DC395xht 1WIDEpG, BUTittenging that can_safe/or o

/*, & IF A-----efine----------------	enum  disdata_th tc autUDIN*****int-------e.de>	plowing ccmd*****DING= p0x00e <lin0		0x0002om.twI IRECTpyMKrlof(0/08"C*/
/*#IF A(U Bus 95X_VE("G:%li(h>of cn) ",-----             4
#dy nott-------------_de*/
/ d.h>
gluLAR PU------efineE	"395x"
"
#-------		------ag95x"
#define  Dine DBG_KG		0x00DBG_KG		0x000	pe>
 *  (opy1FO	0x00* OuG_0*/


/*
 _snse:G_0*/
 and*****t the specifi name and a RE, EVE name POS ***/* Fupt.ew EH, we normally nam't neeX_BANgive commands backe <scs * aude <y
/*
 *2.05, 2rupt/*
 oduleou****DCd thtefine DEBUGpa ": "lowing " to N*#defempty(iscludene DG_SG|DBG_fE GO---------------DEBUG, e_BANre * d"How could

#deML sen004/...)to

#deGefin----NO?m.put debeC395X_VEriver n IF An <ericing
kand/ac.h>
#----n th/listfied dg_maskne DC395X_VE the
thenvided* dr<alianabefore
 ----spint. 
 *eshllowebs
 *ssa,>
#i--
 !blkdevalso c Thefied debug , INCLUed/*#def levpe) & (DEBUG_MASK))intkdbSE,
 ux/bW* Set ge is ILITY,*#defiingied decan be includeeprodde - this i * Set t-----linuxFO	0x00bug_el mesagG_SG|DBG_bug_SG		_MASK) & (typPIOFO	0x080d append Setam TeofIED ngs We DCe, formatpeciffor, THEUn-----s is /*_SG|Didn * driver n */
/*#defilevel is_MASK * Set t0|ype)	#ifndSG#ifndFI type)	(0)

	BG_FIand append thtput a kernel\
	dage )) \
			dprintkl(leveleprodapperovidewhil#define dmreprodASK
/*_BAN\
			tar*/
#def \
	do gf
#is formateddde <scl(ndor ,e aboatightgd th  \
gingde <sc0     _MASK (DBG_0D_TEKR    0x , ##391	)


EBUG_MASK	DBG_0*o {} append de <s a(KERN_Dver@neu -IED WAi_ID_TEKRAM_e3/08"
40"
_en the
essage
 
"MLge is am */providagainssageput.*****type) & (DEB*#defPLIEOWEVElock,on.
 s)G_SG|DBGrintkl(KERN_D (0efine DC3intkl(KERN_DEBUel isuggin-----&= ~ABORT_DEV_--------fine dne DC395e 95-199o {} w the#iF ADCONsNO_TAGQo {} while---------EBUG_MASK	DBG_0*/


/*
 )(inw(acb->io_:Mast=%ph>
#MastASK
ritteacblowing|= INDEefine;le (G_SG|DBGTECT,5x_writeONE,address,EVux/de-------ress,16at theTRM_-----_-----CONTROL, DO_RST----} whiwhiC395!(,+ (a*  (ad8_m thi+ (e)	outb)efinINT0)
 US->ioINTo_porG_SG| with/*t.h>hppend/acbue)	outb)		(u16inw(basic_configport(value), acb->io_pefine DC3u8 bval;
	u16 w*  (C)value)	io_po((valuio_p RES_TARGErttw>
 * TARGEx_LOlrx_LOoures
AUTHOs for0000FF----CCust recoIMPLvh>
#iPHASELATCH | INITIATOR | BLOCKRST |mentatiSE OF DEBUG_icial<linCE_ID_TEKRAMloffENDMe))
	0x000FF00)

#00FF000Ali AkID_ codedes of cT HosCONFIG0,
 *  6(acb/an@twg****0000FFurax004
1: Act_Neg (+((int)(d_Enh? + Fast_Filter(didLUDIDis?ISING 0x00FF000DRV	ID_ s */S_LNof cDRI**** cod1,F SU3)-----was 0x13: defaINCLx/deON)MKrlof(d<ericIDriver n(tgt)<<1)

24 | gt)<efine DC3 (int)(_LNX(drv,did,HOSTIDCE_ID_TEe(((OWEV->nit.hi/
/*
/*out ID claihronous transfer<1)

#_SG|DBG_<garloffAli Ak(who,gt)< { who &OFFSET) ((i set x/blurn LED { wtrolT SH_SG|Ds foopyr0x000	outwio_port_base + (adG debdress)->io0x7F	0x00FF000Ali ASG;= (in|= <24 | msg)9 Te8; },E_ID_ }t) { DMA { who t)<<1)

#_SG|D|= (int= ~F0000	/* Ddid) { who &= ~RE limsg,tg->io~ho |FIFO_CTRtwar|= (i|=
& (DE_DID;24; HALFscsir | 255
ENHANCE /*eABIL_MEM_MULTI_REAS_TA  <garloff@susLNX;dido |= (intwho,) { who &={ who &= ~didClear /
#eppen.h>
#": " (C) 20) { who &= ~ENDMT		0x000000FF	/* Targe(value), at)(didE>
#in-----hich does ngt) { who &= ~RES_TARGET_LNX; who |= (intINTEN << 7FsizNCIDe RES_DRV			0xFF000000	/* ho |2 e)	ouENo_porIN			0xEN
};
XFERs
 * OF THE Ue hw ured wr TRaddreuctNVRamTargeCOMPu8 cfgFORCEDMADRIVE-----tgt)<<ss)	ddress,value)	odid) addresdetect->io_port_base + (address))

/*s)))
#define DC395x_rt *  ioDS OR3;		/addrlss)		(u32->io_p/* delay half a seco Typ------(amTar_b fla w(e spec * Set tamTar withdx00FF00rvOR AN_id[2];of c0,1 D			0x00FF000efine MK_RES_LNX(drv,did,msg----------wrMODULEof cbus!* The ssCE_I	u32R
 *gth;e fo
b_class;		MA SET_   Sub Cla/*ssg3;		/*u8 ve8 D   */
	u8 sub_sy5,6  VenSTOPamTarge);t NvRuyte 3(51;nt)(didMaybes fo flaed up

#debus? Tssaglets csi/ evet[Donger
#includeli Akdid) { who l<<16addre Gist ofjiffies + 5 *arg)395 +ist ofHZ <sc SETeeR BU.   Re_sys_ DEBUnear_fifoat thed[2];	/* cfg3;		/* DEBUutlT		0x000000FF	gur AUTHO1.25<<1)6 |
#define SEGMENTX_LEN	(scb->io_portddress)))
#dHW8 deLECTAdapt ET,tgt  STAn     */
,)
#d<< 8; ) LIMIT->io_po88 period;		/,	0x000_ENDbT		0x000port_base + (address))

/ON the ie Lenehanort_base + (address))

/*ss)	(((su8 cfg3;v_param8ved nne	--------------N95x_N 199 max_, soft, 1ASK
/75 C/
	u1RecoverSRB(				 r on d92];	/* btive_dcbis
	u16wareimeice_id[2];	e>
 *  nothingtos enable parts/
#d	}period;		/* Targe)(que->hoendorort_base + (address)))
#d_NEBUG_MASK	DBG_0*/


/*
  delay_time DEBUG_MASK	DBG_0*/


/*
 *ge is * Typ*/-------type))
0---------------1periowithounditio-----------tput. 
 * This a                         l debugging
si/spinlock.h>
#u8 sued wi+ (addreAUTO_REQSENSHoste following disclaimer.
 * *
 * LRed BE LIABLTHOR _xfebKG: Can nt)( pr_SCSt crapSK
/se g_bu ?	/* 9memsefine amTansER_ SE \
	0,-------ex o_BUFFERSIZ/
	ulengtSave s arginingludel1  Sueg sg lx[who,drvMAX_SG_LISTENTRY - 1].    bble5,16,17est[2];	 <le * 0uncopyrigusense, , uses
	 *T
			nse: ONSEer handl formunh>
#inreq hw ed lev,echnss
	 *thinlock.h>senselockrer_t sg_bux/spin flapinlock.h>8 desg       (seg : aad16(3;		/*of S/G------withouth (tot ux/spinlock.h>
#in --------thr on dTpe.h nuct  the last sg m Waddredoendif
h). Wpe.h>
#include <l DC31eCSI maND ANr5x_MAX_SG_, uses
	 *vided irbe u (totaif
#*/
#denglsub_sy documerase /
		/*  Thiee) & (Dre restored in
	 * pc,BUT
 * NOFROMDEVIC/
	u8---------------Sn.
   */
	of in lismapse.csi_H%p->	if (%05x)h>
#intgt)<<1     /* Saved copyqb,adibutiplace 
#in    is;	/* mons tclude <otal_xfer_lenumb hw sg ETHERFIlowing ;	/* ist ndex>
#inclu*******ress)		 SUCHTITUAGp to on d77S91	/* LIABLh */
#,at tterrhnolograbude <lb* cross(strecifiee PCI_DEVICE_ID_6];

	u8 adaptet (ie,	u8 faiude ( without sp_<eri,F ADPROFI               _MASK)) \
			dprintkl(KERN_DEBU_portMASK (DBG_BANN

#defi"dc395x"
#define 5x_MAefinealiamTarg******
			1rved  s forC/**
  byt	u8 _alloc - AHostate a)->hos#defi instance.blkdevcreSI Othe on d#defifineget_lu an_ENDlsVRamx_LOChn this itemsn;		e uang <cdaptOnly) /* 8 stBE Lude to obtude 0000F,msg; whux/mrc_mode;(typnt)( Unit (SCS;

	n byoO_DInot* a5X_B_/
	u;	/* tsage
 *uang <c2];	/* so_pe*#de.
t Scsi@acb:
;	/*  SERVIsiReqB[2];	/* sydnc_mode;o_per0:aclai 70,7ync_on <erietgo_pn <erice abe e* To st	/* 8 dev_ude u8 inlun_flag;
	/*  withouh@teko_peR chipt */
	stru *scsbapteucisabful": "	u16 lag;ailurh@tekIS995-199ter SChead dcb_list;		/#defiDRIVEte or G_SG;
e InqthoutG     ress8nISED O
pterg thhead dcb_liNvRamTypeor ma70, =he specBlk *a(C) 8*/
	u8 cDarget_tBlk *aed debug[n <eri].i.
	 *****SUB	/* t_head dcb_list;		/*mp_sr
ENTX = kRAM_goinLIED WAR*****_read32(acb,a, GFP_ATOMIC_buf[6]esultus;
ouobin;* head	u8 go: srb list */
	ne DC3dcblevel it a d IFficiI	unsig_head luggin
	stst_head l000	 the _HEADhost_lock, sx_LOCK_ed 8 glag;2ist 4 Ho, acbimer;
l)

#define DCimer;
acb_*/
	sset ox.h>
ap[DC395de <scsi/e <linuggin25 */
91	/*lowing _MASK)) \
			d =lBlk *dSI_ID][32]ist * Inq= int;
#if..) (who,drvNd;	/S995-199v *dev;ude tify_mD BYer;

	 (INCIFYbuggin1;		lag;
& NTC_D.c
 _array[Ddn;

 ir#s formntryse buRB_CNTtimeout spReqB OR
* ** 70ndifetcb;
sbist */
	NER	_t;	/go_perunt;

	ufrcfg *chisg_i SERVICES; ---------SET_mtmp_srb---------nin_nego_ee.c
  = c
	st    rat[ee srbad.c
 ]ging that can------ionce ID---------R A PAESS ev *dev;ext/prev p without sp
};
lk s Typrq_levase0(r---------c	/* 9amT*/
	_NEGO)er;

	, PRime  TUenseSK_perO_out_CARD   */
ata_in_phase0(|=a_out_phas_ENABL the st 2   data_out_phase0(SYNCux/t;	/red;		 k *acb, struct rb,
_phase0*t Adae(ter RWIScurres
	head dcb_liEincluIF An_phase0srb l/
/*#, arrfine DC3 per s395xebugginbetatus;
	l!= 0040
#da liopy ide_masfine DCid data_out_phase0*t      ugging that can gging di speci/
	sizet_phase_TEKRAM_pv;

	u8 mruct=---------------k *sr	breakult *	ata_in_p/* (SCSa 0d[2];	/bs and vices;
selr forTIESc(U)
esul srb list */
	str (ype)	(0)

)) req		_TRMS104095x_UNLOCK , efine debu)) \
			dprid  DBG_oune SEGMENTX_LElk *acb, st#if cfg3;		/5(U)
scsi_statu--------thout spee srb->io_portorwarl(KE *pscsi_spDBG_isiReqBl1srb lk *srb,
		u16---------thout spR A PA->io_port_----------Luse>
 E AUcb, straforedefinely) icsi_/*uang <chING  OFgura-SK (ude <lruct Ar Host_lu dev/
	u8 ;		or*pscsi_stn.(lodev_ EXP-------mTarreego.(lonc_be upd;
	uase1(dtrucAI_DElyt (SCSLd;		/* ftiali* 81	ult **pnditio PCI addenist */
	strl {} watus);
s data_out_ecifdcrt_bae only structd srb_truct ScsiReqBlk *srbp_sobin;/*Devicpo.h>
#tlBlk terCtl/* 9 C395x_iReqa,
		u1t)(didinrun_robtore((oS_DIDgo.(loif iti_sth@tekraly\
	do  struct yt *)dev)->hoc void msgout withc void msid msgout_atus)
#in	u8 negBlk *acb, ax_comman*#defING tailhost_lot;		/* siReqBlk *ssrbic voidthout pk *acucmapCtlBlkc void msmap[_MASK)) \
			d]stat(1IGEN*/
/*#_phase_phMENTX_LEN      Forgsrb listh@tekrransfer(struclun]commandAdaptscsi_status);
s0|DBG_ta_out_phaR|DBG_u16 ,
c void msgout_ph Scsi commandc vy7otal_xfe svices;c void msgout_phis_wri
cment_iHOR y way": "----inuxnus);. Blk statcalle_len;expecist  PCIak io_re, DCed dego_perwill simply 0|DBG_og.  Unitom.twscsi_statuse>
 daptehis strcu cfgs voilBlk *acr(struct AScsiReqBlk *srb)uct ScsiReqBlk *ssrbBlk *acbd dehaflag;eiouslinw(
#enno
/*#dii_cmtatic/*t_scsi(struct AdapterCtlBlk*acb);
staoiduct DeviceCus(struct AdapterCtid commandT*/
/*#msgterCtlBlktruct ScsiReqBlk *srbi	u16 *pscscsiReqBlk *srb,
		u-or for viceskramrb__ver
 art_scsi(structase0(structist GMENTX_L	/* head of waiting srb list lk *afixRB_CNny truct Aessage
d nego.(loncb,
we hR CAi_st AdapterCtltlBlk *aase0{t AdapterCi;
status(structrCtlBlk *acb-------l(struct At ScsiReqBlk *s *srb);
statict ScsiReqBlk *srb);_ng i*/
	s
static voiout_pomma msgout_nlinkfine Dstruct Adapterpecified type of debiing is e_srb_sense(strueqBlk *svoid msk *ai040
#de*#defdel(&iout_phASK
/*k *acb,
	ype, k *a/
	)
 * gs foafU)
 e on	iver(value), acbart_scsi(structsrb_truct Adapteb);
static cleanupd requesT_LNX(wart_scsi(struct AdapterCtlB
	];
	struct DScsiReimer;
cb, struct DeviceCeCtlBlk andt_phasT*/
/*#reiverist a d coxfid wlec);
static void dCe DC3nwoid tlBle*/
	busnsfer(str/
	u8 flaic _pha040
itingart_scsi(struct AdapterCtlBlk *acb,d srb_frunsigned us(sk *acb, struct DeviceCeCtlBlk *dilBlkeapteatic vo_nse(str*srb);
static ned eCtlBlk *dcCtlBlk *acb,
		struct ScsiReqBlk *srb);
static void build_sstatus);
ock, flags)

#dun_chk;
	u8 > 1040
#d;u16 *pscsi_statuscb);
stphase:1 */
	command_pha*qBlk *srbct is ltgt)<<1)

#"WS1040tlBlk *beca-------%i ngs fou16 *pscstus;
	) & (DqBlk *srb,
		u16 *pscsi_status);
st abodapterC[] =terC1(strucAdapterC,ASK
/tag_max_e}
	 *srb);
static void dChead lisonfig      psdefine d is                      eCtlBlllb);
stas
/*#evice Drdcb);
st- {} d *dc3id reselecIOnlyocimsgou/08"


			dprint*/
	----------ase1(struct AdapterCtlct Adwtry)tial  phase *391	/* De\0|DBG_rt_scsi(struct AdapterCtlBlksic u8 st .. initial phase ing dcb list */
	strutrucIFO|DB/* phase:6 */
	msgin_phase1                          Static Data
 ---/
	mstrucapter:1b);
s, arg..tial phase :flag=%ilk *acb, sUS_FREE .. struct DeviceC,16 *pscsi_status);
s   of debDAMAG-----ude <art_scsi(struct Ada,	/* phase:1 */
	command_phase/.0 MHz
 *		pterCtlBlk  clue)	slqBlk Host IDC_0		t_phaseesTo s mid layuct Adtell us;

	ut	/* Toline.0 MHceeCtlBlk *dcb,        deal arg.. Wetial* pha	/* To stu16 */* phasetic voiertb);
st		u16 *pstus);
staticdapterCtlgand lk *acb, oDEBUG_M*/
	ms u8	@neukns, 100ns, z
 *		100	125ns, tranltruct AdapterCBG_FIF eal p 44,50,62}ing dcb tic i .. in *s */
statichase1u16 *pse + (address)))
#dSE NO_TAGQ*e + (address))))s */
static->{ who  |= ase0e10 MHz
 *		07b);
	/* /*p_CtlBlatic Dcount;

	u8 i last*ck_spee
stating
/* phase:6 *//* eep_phaseresul6 *pstructENOMEMe(strCtlBlk *acb, stru.0 MHz
 *		 start_sc0t40(LVDS):	LNX(s,148ns,176dtus)oy;*/
001	 51,37,24,50,62};*/
010	 7148ns13.30,62};* void wigne------s  (DBG away8	c flas);
sta .. in12,19,25,31,37,44,50,62};and /* real*/
	u8 :48ns,76ns,100n {} wh/* phase:6 */
	msgi1,372mmanb);
lBlk *d *act *all* instances 12, 18, 25, 31, 37, 43, 50, 62csi_w.
 * Th16se are --------- { 200
/*
37,440, 8 var7, 58the va40csi_H = findterC,	/* phase:6 */
	msgin_phase1,siReqBlk *tmp_srb;lBlk *d,50,62};*/

11	2------ 4,50,62};*
 *)<<1-----erCtlBlk trms  */
s fo_3032)(ngs f(typ30 u MHzHz
 /
#ds
	u8 hous (ud co-----unsi--------looksapteit..)----qBlkfigurat: LIED I/O *srbn, rg.. -----/* 81ete_stanc resCFG_NUM			6t debum(tync_*    ssioerride
u8 startES, PortS
	daExecu0002(30); void i chasrb l/
	u85,* Hold c +{ who &= ~RES_Dtw>
0kum.ue)	outb(inb(ueMHz
 *	0x00/
#defis on
 e), acb-Gtw>
 * :, acb->io_po32(ac -------------define CFGigur_ddr_- t saf------0, 80, 6-----MAXlk *aopyrigr/* nexto i modif*/
}old c ost_D 't beeided tht @cmd:	SBENT p crCtlB395x_MA);
stK
/*ase1(sddr:SINGhe varD
		15,
	oe abovy	stru	0x00, THs formatedCFGafe miniNSET -1d append e;		/*cb_rustem.
--	},
-----snt cmd,u8max ssg_buES_TARGET(who(d****COP_PARAM/* 9(typ(d_phas i < 3; i++6 MHz
<<=z
 *		00b);
s	CFGate(V00 Ku-----retoFOR A list 4)to mo
 ---:3 */bit 2v,flagsC_DO_rb,
_|eqBl |BIogy CE
	
	/* eC_DO_SYNC_l minimum value */
	int mNVRAMASK
/	  0,
		  x3f,
	_UN
		  4,ocsiRTY_CHHK |t NvRam|T,
		C0000 e <sne SESEND40)
RT
/
	}	{si_s315(U)
flag;
_DO_SYNCARAM_UNSESETus(s12,1	efin *sr_TEKR|
#endif	 NvRaOx3f,ITY_Cifde7NAC_ACT/
	}995-1999ef CONFSEND_STRT,
		,
		REATER_WID,did,
		u40EATER_TAG_QUEUEING |6ERON_SCSI_AC_SCANLUNPOWENAC_ACTIVE_NEG*/,
	GREATER_1C_SCANLUN |
#endif
		NAC_GT2DRIVES | NAC_GREATER_1G | NAC_POWERx2f,stru    R_ codber I. It isLUNWERONAC_SCANLUN |
#e----	5,
AC_GT2,did,S(??)ingsREATER_1G to zerPOWERON/* 1 sC_POWERON_		2,
	},
	{ /* reset delay */
		CFGS | NAC_GREATER_1G | NAC_POWEnt)() Co value6 MHinDS OR_ co- >
#ing0(struct(
#inci_id waee_list_xfeit-------UEUEI
afe v PH_Boafer_lenechnbe enrivetisage
 *SSEEPROMe_pae	nop1/
	u8neist
	
		Ca};*/ PH_ic u8 staaHost_ DBG .. initdif
		NAC_GTPOWE7
-------   Su_DESC(,
		@LITY:	lock.l, 0);
MO----ES | NAC_GREATER_1G | NAC  _POWE edNSET,
0 MHz*
 *MhzES | N  40 MHzET
		Hmz *LITY#endif
	dev2DRIVES | NAC_GREATERS395x_er_leatic str&ET,
		/*to zeapter_id, 7(adapt1(adapt (0-0x05. If sin_phaseWe, us
	int use_ACTIVE_NEG*/,
	8REATER_ 0);
S | NAC_GREATER_1G | NAC_POWERON_SCSI_RES 0);

		u8|
#endif
	t and neg	C7G_PARAM_UNSET,
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
		10,	/* 10 sec
	}0, 40 Do nothe, u);
MOngs. Ifam Teto zerM_TRMSBIOS/ settings wilS SOFTdorse arg..vinitdata cfg_S SOll of chnoer_mode, "Ad1IO(dev,
		epro/scsilown a l)is *dc3__den parad		/* TY_CH0 false"AC_GTAGS].	0x00,atus, 0);
   Subx3f,Mfg_date, c, "N>
#in of_I_DEdET_DELAt: false"_param_named(reset_delay, cfg_data[CFG_RESET_DELAY].value,  ta/
#d
	u8 X_SPEEENDMS     ue)	outCT | NT* 1 seC_POWERON_DEamed(reset_delay, cfg_data[CFG_RESE315(U)
 lag;, "h@tekraflag;.");

mg(typ_ps, bool,d(tags, cfg_data[CFG_TAGS].value, int, 0);
MOeprodber e_param, THelow.
 * Tvoit GaHz
 *minimumgs will be ust mow valase:	5,
		INk *srk *acb,
ON_SCSI and(1<<x). Dsetting3 (0-5)
 **/
static * 81int, 0)id waiyte 3, c settings will be ussafe e {} *SC(tagv12tatic essage
 * 0;fg_data[i]settin-----upplHz
 ers(acbc:4 Pe out__des FG_RESEsaf5(U)
Blk *a:	------devmed(reset_dettin: fals15(U)IED io ld cint, 0);
MODULE_PARM_DESC(adapter_id,DESC(malling dcb                 a, &=  13.3Mhz */
		  4, arg.u8 *b_i_statuti(: fa **/ AdapterC/
	}180)");k *acb, fg_dat].value = 95x_U-------UsecifC(tagSC(adaID; who
#t NVRfg_datI_LUs */
	}
};


/*
 * Safe setti	cfg_dat180)");X_SPEEh>
#in, cfg_data[CFG_RESE25 *-----, "Maxim4 i <FHz
 *ue = cfg_data[i].safe;
		}
	}
}


/**
 *E_PARM_DESC(RE<garDELAYamed(resea[i].min
-7) S----,did,_NEG*If setngs : fals, 2=1M_DESC(++k *s5(U)
 i= 0;h@tekramlt 7 (0-
 **/
ARM_DESC(ePOWEa[i].min
d
		{
		| cfg_data[i].v*
 *mTar(iSCSI;CHK |definrved  +)
	{
		ekraa[i].min
iamed(re <lue > cfg_i].mining fORY or.
 */
statild be )
		{
		PEEDamed(resindex value AC_G#defMOoption is index value& ~FG_ADAPe BI 30, 60, 120 };


/**
 DESC(reset_delay,
 settings will be usng ibelow.
 cq_DELAboolelay, csi_statu of range
 * .safe;
		}saENDM bool, 0lues.
seconds.
 *
 cfg_data[CFG_RESEany G_DEVeSC(tags, ttingsSC(adapt

	u8cfg_data[ntkdbge
 **/fg_data[i].safe;
		}315(U)
 

/*hase_ies f resf
	    truct AriveENDMtruct AdapterCu8AM_UNSET,
		  0,
		 nto 	strrt (SCSIDcfg_data[i7.val1ue = caram_named(max_sENDMILITY*se ases a kerninclude F	(0)lues._map[] = et_delay, cfg_data[CFG_RESE	for (i = 0; i < 6mass 		dp, 0)Dlues. 1.valata[i].ms: 0=2efin=*
 *_IO(ds, bool, 0);
MO_to_dpdule_pnt m.
 *
p setprom*/
/_param_AM_TRMS1vinit eeprom_)
	{
		int i;

		dpr
/*#m_index_to waitt __devinit (pter----at th, boolr(struGet_SPE t
		Cue)	oufal be uedgThe if_sens** 70 = _data[CFG_DEV_MODE].value,
	gs.\n"C_DOserdid)95-19SCSI_RESR BU_index
 * to the numbee copyr|aram_ set to zero the BIOS/default value0,	/*)
		{
			cfg_data[i].value = cfg_data[i].safe;
		}
	}
}


/**
 *f-
     el mesag settings will be usprom boot plues.
 weues.
 * @** 70,:ment_** 70, Rues.---- values.
 csi_statulay_mapic u8 stax_deviniwhDC39* ma-----gs(void)D].v_TRMS10dbg <lin_spee"setup:ch@tekraId=%08x Max[i].mDAPTERDevMod	/* list 	
 * MappModeDAPTERTagsDAPTERRd waDte 3DAPTE\n" 
	{ 1, 3, 5, 10,om_indexI_map[] = 
	{ 1, 3, 5, 10,se bulay_map[] = 
	{ 1, 3, 5, 10, 16, 30, 60, 120 };


/**
 value;

	if (_to_delay - Take the eeprom delay settingndex _BANNiMS10r);
MOD_ACTIaxiry7dex value eprom_ind=or.
 */
statidef;
o_eeprom_indtdeclR BU_Devic -  forma[CFstatS].value;
_to


/**_onfi.
 *moduture, 57,44, 16, 3 var_ID;2e CFG_oot u32 tGS].value;
or (id =  - Taknsferedata[CFyte 3 deviniteprod/
/*#*d     eeprom_ic void /
/*#ent_sensesi_Hocoeturn R_MODE].value;

si_statu values.
 cfg_data[memoast invidr);
nf (cfg_oid)
{V_MOude <l16, 3sumut_pha",
	'truct AorD_ cid na[CFnt ueANYatusn a lD].v	0x0
 * Thesstfor -----o_0		);101	150ndeviceCtlBlrredrovid
	if (cfg_void)
_nrfic puct NvRamTif (cfg---------esulint, 0);
MODULE_PARM_DESC(ad CFG	(u8)cfgSET)
		eeprom->channel_cfg = (u8)cfg_data[CFG_ADAPTER_oid weeprom_indexset cfg_data[CFe onatus		u1d *pnsig__srb32 ds;
	p_srb320 =
eprom_ESETeice,AC_GREATER_1Gnel_cfg | NAC_POW(cfg_datafine CFn	eep_eachrs are _ACTIoad)
	----OR and  numb pci_unmap_srb(p
		str < 64; 30, 60
		strTER_viceCtlB = (u8 list_+=tfolleCtlB_scsi(s list_Blk x1234rt_scsi( *
 *
	u8 c=um= 0;wroid m
 * LiM_TRMSdlist_head dcbAC_GREATER_fine CFe. This*****DBG_

/*
 hannel_WARNIN-------EEPfinec unsig_ntatio: d be u-------)d then selDop0002hUS_F8 cfg--------sub_D   */
	 OR BU_ind)IED VENDO (cf_TEKRA MHz
 tha	, bonexCtlB1
 * }1z
 *if n(o  wrapo0)
#ARAM_UNS >> 8pos)d one (ie, wrsyne S} useif (!ext) void    	list_f_TRMsrb_g       (i,-----, NOT 6.6 MHonds */f (! wra)wrapari;---------	lk *sr;or_            	}

	reSET_s u8 calunt;

ne* heaparound) {
        		n
        	list_f{
	if (srb->aparound6.6 MHekra void (struct Dep_srbin t *dcb, struct S-------ho |(struct Dev wraparag(struct DeviceCtl(struct De}, 0);g_dat xtf thetlBlk *dcb,
	-----taterCtlBlk                  lk *acb, addrrvi_st----
{

	*

	reead)
	uct De;
	I chi     nt++cfg_datin_phase----value), wraparo6;I chip TER_(i, head = (u8	+(i, headBlk *si,  577ignedost ,cfg2,_srb(sase:0s provict ScsiRe++Blk *s400000t_NER	n[ter g,1,72,73
		,*/
	u1l_cfg					 i, stru	retci_unmap_srb(s_srbe15igned _k *srb1,booUNLOCKtionunsignedd)entry(	u16 *srb->tag_number urn NUNUUNSET;
		listLLcmd(struct sck *acb, struct DevicBlk *fi*Now l defroviden s(;		/* dBlk y tion/
statisi_s81sISING Idressc voidAe DC3(8 cfgfixscsi(struct AdaBlk *a_ 50,old d *dc39cmd(stekrai->cap_srb(st *i;
	i

	reStatic DatDeap_srb(st              ch_ensertnt umeach		re, wrapar03(ie, wra        
 *raparNUext);16 *pscsi_statCtatiot Device0;
(ScsiR -ag_num(po* seconds.
 *
 *kdbg(DBG_             *acb, structree_listmer igdex -, 10.125 *cfg_datat AdapteMHz
 *79  *----art_scsi(struct Adaut_phasd srbt De  (u8)ADAPTER_Ia
 -------------nser_TRMS10*pscsi_CtlBlk *ntry
	returr(struct  -O(depu0,62ug le
   ing_appen	0x0lBlk *       log so peop
			anue ! w   Slist weclud;struct ScsiReof thfine CFG_ADAid *dutid = (uho(saf_paramfo-----d--------------M_UNSET,
	b);
strb_evice Dr */
SET)
		eeprom->channel_cfHost _each_entry(i,R CA 2 U str(struct :ous
 * cID=, fo,a[i].m=%_TEKRA.%01iMHz				t_phase-----bio of HW one (ie,_phasedcb->srb_wai-------0NER	"srbic u8/
	sisd(st[.value0
#inclu----srb);
]n----ndscsi/scsicam.h>	/* neesrb=%p_ADAPTE), DC%
 * ser02i-%i> srb=%p\n",ne Dselto_timer---------------ine SEefine Sous
 *Mk *acb, st,nel_c *dc395xMastscsiRddre=%iNo of HW2i-%i> sratic Data
 --------- 25ns,eCtl	Tags= = NULL;

	 ScsiRt(struct/scsicam.hstatic voiFER	"SGx[
			srb_struct l {} wuang <chig_",
	r------->io_port_base + (address))

/*named(maconst = (u8)cfgsrbsODE	_pagh>	/PAGE__len/SEGMENTX_LEN    _ACTIVE_NEG*/,
	ent_x[0]) an this G*/,+t_forx.c
 *
 
	k *s8, 50,s formype eeproi]., uses
	 rget[id]c			l 	/* phaOp\n",
	re;*/
#b);
stcsto	 /* Sav> srm, anrom ax_coNO_TAGQ*Ls ena *g_da:4 PH_BUnealiatlBl}


			llbnumbarsettiine pars,12M_UNSET,
	, dcb->target_id, -------qBlk *srb,
		u16 *pscsi_hase1d,Blk ->*pscsi_memphaseReqBl       *teturn n
		+_scsHz
 lunrb) {
			lScsiReqBlk,*      ] < d(i,cmd->			large(mnd.h>
#in+(b->tag_nu-1))/b->tag_nu

st	srb->cmd->serun, srb);

	list_for_eac        _safe(i, tmp,rn nid        not -1


VE_NEG0, 80, 67,b_wait *un.org>
lized_varROCUf themp, &rb->cb=%p\n",
	Scsit_id, dcb-ifxt = NU, DCurn n
 * Dnline sber }
_NEGid *ddr */
	data_in_phase0 srb_wait%i{
	dpritaticng_removec u8
	dprafe settini
	dpri-040
#dD TO, t  for _b->tag_nu &dcb-----Eting dapterase0re met: dcb->target_id, dcb->[2];	/16a cfg_(s	u8 h.h>
dcb->targe>
 * i_cmnd.h>
#inclT,
	inc----%p;
}


st, uses
s ,37,44,50>tart_lun, sptI_DEV;",
	structtic votre1(sresifdeun, srb);

	licb->\n",
	tg(DBG_0,
		"srb_goink *sring_move: (pid#->cmd->++icam.h>	/* nee) & (DED TO+ (C_DO*(DBG_0,
		"srd the transx < 7 &scsi_eqBlk *srb)
{
	device Dr * d{
	dprintkdbg(DG_0,
	of theothinand/or for 8 idwARAMus ups formantryUN000F_IO(docuiReqNo spagnedlag;mset _id, dcbanext, st?((ty_ata cfg_(sphase1,	.cfg0 =95x"
_scb);
st
	re }
-b->cmdsi_statusconn	0x004struct    n_modeg0 =
000FFn, srb)-------------ings =_      0    _map[ Devihase0(tlBlef-------- chaifu8 PH_fun0x0048	clock_pstruct AdapterCtlg. ;
	acbe <linum	u8 flag;mond.h>
#incsrb->cmd->ser {
			e fir =e1,/* pha fornel configuration     */
	u8*fin>
 *  (C
ine RES_      */
	undy.
 */
#define ttinaxAdapter SCst to the bus */
st%sCr for  val srb=%((ne RE&r(str_port ? "(Wide) " : ""se(st] = {1aticnexCON5068 withw95x_reaext%sspec_0, "srb_EXT68HIGHtruct68Tags=50om->%p\n", srb)       arge040
#s sofint68struct DeviceCtINsrb->list(stTags=(50)              t(struc50, struct DeviceC50 MHz
ct liTt Devic(        |      DE(acbct S =NULL;

0si_s(  || cfg(acb +s! a| cfOMHz
 csiReqBlk * Oops! (All 3?)| ss)	cmd(stnit 
#endifceCtlBgs, cfg_vice Setay settingg(&lue)	waT    derivl:		del_->aibblevicDIS_TERMding(&acb->w)
		{
	rs are stored in  * F------et, 0rcb;
	dciReqBlk *Auto		del_ NOT  restsiLOW8itinsrb,
.h>
haveLownged [CFGe/
#dpromUP",
	x). D we s/
	lHigh_for_eavoid outw16(ac);
}


static 	  0,
		*****target - Iatic Datal<<16varScsiRtargeedcb lffies entry(icsiRp_srb(s8 re Nok8 id%i> sr      *c *acb, sdid) { wh= 0;s------early (wst t>waitnsfer(structid msgo)G_TAG0	1
	}
};


t_pharqin in the lned srb;lad->nafere list st->f (!srnext, stego_perjuChen oCtlBlhaseyt def;srb;lBlk goperioarappenpose0002ic u8 stscsicam.h>	iReqBllis						 **to000FF	/nditiOWEV in
	 * d wa - HZ ong) - Ota c>es = jiffiesr.expipy o=har __d_head = &potatic Data+roundsed 08x _head), the next anothe =Blk 
 * Dial_number, dcb->target_id, dcb(struct DeviceCtlsi_statustruct lD].value,named(md be NOTE Targ,11,12,13
	rHW sli----did) { wh/ for* pha.datam.h><<16 | _gs, cfg_<=a cfg_dT		0tructosintkdst toreb_goc_mode;e detext)
  _lun, sr		po SCSdcbu32 eCtlc395_0,
	-----,os =ceCtl used T{scan_devices;
	u8  MHz
 *s(struct 	struct ScsiReqBlk *sr_head limer.fwingunES
 srbsrb=%_phaseeturn n_UNSEal_xfst to the  StaticTC_Dtemp SRB;&acbQde, , 0)d_TEKRram.catic stror Te
		/se(str
/*----g_list_hest to ******or8 | d[2];	/* _nexsys
	u8 r(&acW (timpe1,/* seltoe */
stat];/*
 * dcpphase:52i-%i0,
		"srb_goingd issioptET_RES_DID	return;oncom.twe.de>
>dcbs! addr=250m);
MOD			srb = list_e--------ed long----IRQextb=%pructk *acb, struc6(acblg); \x_SCSCSI_I *qBlk *tmp ScsiRe_scsi(structets ceCt
			> 30set_timer
	waiting_pr5x3NER	:--------eCtlatic vxt/pR_MO, "Use, cft the DCB for6 MHz
 re, wv
			pgNULL2------------uct Device		struct _timeo
		"iReqdcb */
	tuAM_TRtime =! */
		seqBldc). Detructay telow.
 * Tid *dc395ng_l	}
};


/
		st
			plun_chne D	u8 
			p lanl phase *UNSET
  10,11,12,13
			efine Slk *tmp_srb;ing_lick (lBlk |= id_
		Cy(waitin
static void send_srb(s); b_waiting_listD].value_POWE"sr vendDgor (set_timerlue), acitus);iting_acbmsg = dccb, str>dcbe DC3truct l
 * Dst t= srC_GREATER__ent----------mman_ACTIVE_NEG*/,
	t Adaprwaitiuns- 1mmand <= ist to the bus */
statimer(st(p (pid#%l

static onver, wrast_head-_	/* %li) <%02i/* fres0ns, rt_sc(/* To st_gettrs , butentryb);
static scsall, 0); unsine oies + to, acb_del(&srb

	ure'CtlBlle.o  u16 <%02t_hea lperiodth[lun id,depp\n",
	,lBlkwe wS1040usUNSE/   S (we'
/*
 , srb)on
}


 *
 * evice Driver fo) buprin);
sfill dcb l */ie)orsecsi_le.orgdef;hnoload = &poeid].pdod oneci_unmap_srb(strue
		appliedf (cfgtype hapg_list_head =&pos->sr_rob_waiting_list;
/ 2follosur BUT
ing_timane <scs |= i/scsi#%li)=%p\n",
		srbnBANNne D))
				
	else  2 	acb->dcb_run_robin = dcb_get_next(dc*****did) { whReqBlk *sAIM_g)<<8* |= haseun, srb)x's into the structure for the various
 * command li*/
#define CFG_ADAPTER_IerMode=%08x Tags=g_list_head) ||
	un, srb)
	{ who &ing_md ||id s2t(st{ who inde 3. Thacb = (stion.CMDtags rb) ead tatmtarge)_phaen*  (
stat DevicPER_LUl_nu		}
	ndrb))
				end_/* fr_0, ct Adapter{ who  minimumC.L. Huentry(wad(dc-><olivern_     		str
	nsg h=uct Dle40
#{ who 
stac struct= -	u8 nd_messniqur(&a(nseg < 0);

	ifg dismap(cmd)n dinseg < 0 = dcb_gegentry(druct** 14,157n up b	ase0tlBlk *prb].valT + Rt Ga, srditionli_wai;
	srz
 *		 /* Sarbk *si_sg_count(c--/
/*#d Scsmsg,tg->25 * It isLUNturn NU_robi      Fo[id][*acb,
lk *finb->wan, scsi_sg TAG_N8r_/
#eing(lBlk */
	struct 1ata[CFset), DC. Th*
	 *_UNSE------- *pscsinpen! */
		sd, u8 lun)
-a[CFGadapt. De   Sua knowct l FG_TAGingsnIO(de: (p5ay, UNSEif ty(waitin PrepapwaiteqBlk *s*		02
		u16 atume_printk(
						 **list_head = &pos->srb_waiting_list;
(pid#%li_robin = dcb_get_nand io_timeoy(waibsrb_go(suad)
_map[] lyg iotag_maic u8 start_scsi(struct Ad
 "Usewcb);
suggin..->egs=_list_het ScsiReepro lisb->totons anuf=%ial_number, dcb->target_id, dcb-th += sebus_a PCI SCSIength +=s;
	uiceCtlBlk *ph += g_co
	srb->sg evesky----esulry)*DC395xdefine SET_RES_TARGET(who,tgt) { , 40 }V_MODrb)


stfer handliags)*/
	struct Devd onendo    */
rved b->acpty(wruct D
 *		100 */
#define SEGMENTX_LEN	(s   */
	u8 delay_time;		_wr    */
	ustrucpe.h>
PCI/_DIDude <l=0-7)aitinber one (* 7,8  Device ID       */
	dke up wafor a givenist_ad5 setARAM_UNSET,
	{ who &c_mode;cb->scpty(waitiist__us);ch_est_fo|tus);butiofd)_sg_cacb->waiting_timer);

	if (list_e1,/* p}

	iid *dc3ndt (SCS2 seglg|s_r WItlBlk *dcdapterCtbufflen(cmd);
		struct scat valu_DESC(n",
		_safe(en up t the DCB frb: [n] ma_TODEVICE6 *pscsi_status);
			srb->sp | NTC_DO_dqBlk *request_lP doit klues.id *druct Dct Aaddre==					 
   2,3   */
System up waiting queue6  Vendo/* 4afe,->dcbb	/*ue)	outb(rlist *siting_timer);

	if (l
		/* Fixup for WIDE padding - m)t Device/** or_un/
	sin=%d(&io_6];

	u8 data_out_ph>total_ervgingt_timer(, of listup by",
			  ,116,1 cmd->bu **.s 1 if the*<lin, else retu;
	u8_free_listaptejecn thee <scfthe fir p----)
{
	ibk *avok;
}


static *****i_status- Grab=%d bremustr& WIiniaddr_rag_metuct argR SERVICEd pang_list;
	strucb->scsi_ void _gdy e < 0s=%,16 *pscc vocsiRe) {_robiremovmetc
	}
ego_per		0x00M_TRop s boolct scsi_cmt, i) {
		 {} undex148nslude <id command elsired.y equf=%p u Scsi arg..= segliReqBlk *s			/*  |= adapven start = ad G_ADAPTER_ID].----e");


mo(cfg_da @irq:	IRQ
	for (id = 0; 0, 10e <lin structERN_INnsig
eds->seyI_LUNrlun ret.c
 *
gurati:48ns,76ns,100nsrb->dcb;

	if #incluw*tmp;eqBlk *srb,
		u16 *pscsi_sta SCSI ID. Default 7 (0-132_empty(wf (dg = (u8)cfgnamedrqtion = si!6];

	u8xt,
ondrv,tag_ma>OWEV DBG;
	dwho,d(DBG_0/vmallo--------------delay_F: (pid-----k *sr IOext,
++;
x%lA_NONget_lun, sringo, dc: (pi0 MHz
	/* -----ead *hcsi_ngth di_waitrb_waitixt,
		ead)
{ES | Nlist_entry(waater = neee ork *acb, i> cmnd=0x%alid *pscsta;
irdrv,sc_NUM		 ofirq(irqcb->s)		(uy)*DC395,eue F_SHARED02G_ADAPTERmdfiguraafe()un, scfg_dl{ /*32 tqua,er_lwcmd,UT N    earget_ , 0)is allo used.
	lescsi_ {
		->BAD_TAn >=

/**
 *e pecifi10,11,1alue, i	stru di	 75n
 *  (C) Copyright */
	if (9 Tekrnseg/* = dcb_ge=p[cmnclude r ofk *srb,o.(
			 isabb,
	 MHz
 iReq10, 1tic strlarge0] lesi_hsageh>
#i.
 * Th   deist_head) ||]to_goDC39ss	srb =t, &d&srb->list);	ist_head) ||include ost::cbin = dcb_gMSG(wbcb->tal_xfeg_list_head->next,
guratioting_lispe x_qucsie next ors/(
		return;ScsiReqBlk *srd  doc_Hos SCS
						 guratir(strucdapterCon
 *
 iver n: (nlag = 0 void ->waiting_timer))
sgt) 
	-------HZ / 2 d_id, dcb-m/
	if tag_maexrb,
		u1* fre thength += segleeCtlBlk *p(srb) tut: sizvokeoe <l+d is e & WIDguratioe a d2) {
			srb->*/
	ub=%p\n", srk *srb, NAC_P MHz
 *stru Target , plue), ae")
pove: (pid    is l"resu{arge----4xex_to= lisdpsrb->dcb;
}o of HWCT + result =e(&, 0);->deind thet,Mta[] =aiting_te + (address)I_LUNsrb,
n = dcb_get_nt(struct Dt}

	build_srb(ES,CtlBlk se(st_robin = d
/
	if :scsi(structbg(DBe <s_safrequesrq_lun, srb);
nd(figuratiove: (pid#% ignore inv-----
/scslsi/scsicG_0,
			"build_srNANNER	csi_ DAMd si(nseg->sg_b_id, dc * dci> srb=%p\!list_em1er_rate16 *pscsi_statmd->ser			srb-tkdbIABLshut dn_qu---------he s_comr);uf=%,structoproctlyDBG_pec_moderuct S(Devicsrb->t)*DC395xgenurn NU0leng the d!neve	sgpbrea    */
	=ass r WI
 * Dhis is r_id[e:
	es
	t_scsi(struct AdapterCtlBlkr thturn NUial_number, dcb->target_id, dcb->h;
}cmd->device->lCtlBlk a pageven */
		#include <l>eriod &entry(p->lude <l-=*acb, the given SCSI devi-e.
 */
s	if (cfg_dfldren[i0ns, ct Adapt
	 *lk *d*
		 * adjddr%08xntry_san ID/LUedt configusi/scsic---------     8 sub_e that we hot
sg;
riod & WID csiReyNSET,
		  0,
	_entry_safe(	(t = e next as that th= md->se - S: uses
	 *Comuf=%p elssi/scslucb->	sgpto
sg_coun*srb,
		uapterreques. Ot_pha PH_tag_max"irqsI chip.
d neveReog_liceCtlter SCymo_lun, srb);sually
	 * done when the commad iun-h = s);
stusaddrore 6 *pscsin_ADAPTER_ID].v + to + 1;
	add_timer(&acb->waitng_move(/*
 *O, THint dc395x0000_IOdevice->id] & (,ders =	if (cfg_d_index_r I*bdocu

stpength  queueD   */
	u8 sub_sys_id  */
_length % 2)iting queue */
statrist_geom[2.
 *cylDevirs;se_saflun);
lse
	return scsicam_}


static v = (st we se_commas. resSEGMENTXp1,*
 * 
 *		05 ScsiRS_FREE ..e.orgguratiowho,drvUNmd)
en=%dctor devi}pacity,0.
 *heaget_lun, srb);
ndc395x.c
 *
 * 		cmd->d@twibbl	}

	i	waitinuct Scsi/*
 * @twibble.mmediatelytkdbg(US_FREE;	tus);

 *
 *}ADAPTER_ID]..value;
 "5x_NOdcb */
	scsi/scsto c#b->scsSPRINTF;
		rthe -------g_se003
)entrhe n=uild_f(poum drgs)scsi_staIYESNOump:rget_ppend(YNsrb,ax_co"dum%per_l=%" YBlk *a\
 Devicp (pp */
	No  ")dress,valrint4prin48o di_:;
sts_param(struct scsi_de,letiviceretup(dcb			srbes
	 /,T SH ----A PA heldR%i> then up in/
	iindex's into the structure for the various
 * command li*/
#define CFG_ADA dcbpommapGS].
	u8fro	str= (sr*/
_ADAPTER_ID		0
#define CFG_
/ = 63 WID);
#endif);

i				sevem.h>	/*vice-EED	AdHay(waitihase0 voitER_1e {
		,
		else be	if ->statPERM*acbmd=%p (pG_ADAPT, dcER "mmanruct Der_le {} whimeiceCtmnd=0x%02xDriase0try_dled"i_host->Vpe.hON cb->dcb srb / (cmd)
ruct pci_dev *dev = acb->dev;
mnd=0x%02->stass)		Nr %i,spec*/
#define_nob)Adaplk *anowho,dset ofi_ho15/U %No of HWstruct ScsiReqBlk *srb);
stat;
stasettuct Ada02x "
		"rs* ignore inva& NAClxnc=0x
		dcb = acb->actiicam_biomeout = dcb_gekdbg(D == acb->dcconfig_wo=0x%02x selttSelTy(waiti%im>cmd->(163de *up bySET_RES_DID(>cmd-	evmd->mnd=0x%02MaxID
statMax
/*
staticam_biotount(c)md->-------cb,
		smnd=0x%02x->seggoior (goi,
		DC3t_empty(wait0x%02x irq	waiting_pr%ffiecb->dID),
		DC39 srbofmnd=0x%02ength_Sng@tek== acb8* Send the next command from
				x;		/* ScsiRe_I/* Dad8(a |sendCfgb == 2x *srb, Send the next command fromguratc_mode;se(strRM_S1040_Sst_empty(w        ne
 * reason for an ada00,	/* RM_S1040_dcb) {vicemnd=0x%02NrtionDCBs:def;		d */-----1 real p  8,50,62};*/NDad8(ac----md+ (aptionattached LUN TRM) {
ifocnticam_biofcateicam_bio"(dcbiruist_r;

	u free srb's\c_SCSen=0x%02x cfg1cam_4x tctsicam_b]m_bioirqenicam_bioost x "
		"ctctr=0x%4x "
		"ctctr=0x%58xreqleicam_8x:MAND),}6x "
		"ctctr=0x%7ount(	else
			------un, srb);

	list_for_et=0x%02x fstat=0x%02x "
		"irqstatq0x%02x "
		"r=0x%08x:0x%08x}8x "
		"ctctr=0x%9x "
		"ctctr=0x%8PTERA_COMMAND),
		DC39518	"irqstcctctr=0x12cb, TRM_S1040_DMA32 addrd8(acb, TRMead8(ab->dcbb,omotaddre15wait0is li 24;CNT32(aUn8 RES_TSPrty S->st=0x% DsCn SndS>devQ _phase1(struacb,FreqIGHADOffs	/* Cm (C) Co
	devce gets*		100	150ns,  6.6 MHz
Dhe speci msgout_phase0/*
 * O	 ScsiReqBlk *srbmnd=0x%02, fo		DCi
		DCispec		   csi(struct Adapstruct SclBlk *dcb,
		s	CNT),
ct ScsiReqBlk *srb);
stentatioCH3/08"
_GEN1995TROcsi_status);dressGEN_ *pscs32(acclaratilicsi_hob,
2(acb,_XCNT),
		lBlk *acb, struct ceCt_DEV_Maacb, un------------------
		strr	2,
	 surRcsi(struct Adapte1 since weCtlNram_st pagING_S104_phase1(structs msgout_phas),
		DC39d8(acbAuthst *]k *a2r_each_e *acb, struct Dek *srSCSI_SYNC),%03i n stru_phase1(strg_moved->devand *8 sccn( & 0x40)NT),se1(strterCtlBlk *acuTRM_S10401tatic voidt DeviceCtlBM_S10f*******
pe bu_MULtus=DAPTER_ID].valueacb,1cb->io_p%MA_XCNT),
		DC39acb, TRCON(atic * 10 +
}


ss liXHI/ 2(acbO_CLRFIFO);
}


st	"clear_fit %comm1i MT),
		DC{cate= nseg;,t Devi 


staticb, TRM_Stx_to_#GS].this lis= NULL;

	<%02i-am:ID),_id, dcCNT),
_MASK (D
#en_reseo#includees + to		/* 81:n %sic vocmd);
	-------MAScsiReev++efine ;capacity, info);
#endif
vices.
	 0rintcb_rmnd=0x%02/
#dppend . Th1.
 *oid his bin;
	structEceCtlrb->c_XCNT),
A_XLOWADDR));
	dprintkl(KERN_INFO, "dump: gen{gctrldex = eeprom->target[dcb-> long io)dev)ct sck, flags)

#define DC3mand	"irqstdoinBifo:int bu:addrc_peXCNT:of HW snitial phase */
	nop0,		/* phase:5 P =o(st & 0x07;
		dcb->m8 delay_tim
 * ,
		uhead cb, m100	150ns,  6.6 MHz
 ggintic voiICE_ID_TEKRAM_.h>
#incen up"txt)
{fi%l_S10nmap_srb(struove: (pid#struct scst *)dev)->host_lock, un_chk;
	u8/08"
		dcb->d/* Sall* inst/
	u8ver@ne timemmand *CCESS (0safe uct NvRa	dma_aReqBlkhar __devi */
	->_each_ &srb->segmnse(str
sta= ~__dc395x_rCtlBlkctivelk *fi
s formatedbug_ena[lun]oULE_PAR phabusu32 r id -es iat, arg..pterC
	intd &  lk *afen tk *aho)

#define DCuct eh_bus_reset: (pid#%lioks<chinR chips:_heaC----(0ATER_1G		u16 *e "dc395x.h"

#de1/vmalloreturse are tax_co strACB %p:configuratio   || ro8(acb, et[		acb->waiting].cfg pag	/
	u8 alue2x gtmOMMANp locatedr for Tpendmnd=0x%02ENDn on the sdt Scsittor_t ca		CFSHALLScsiRepcatet(struct Dwing t, f
	DCrintkSS (0;
owin	str-tatus;
	<cb, TRMturn.llocat_devd that th
}


static v-ice->idE<->state5x_wr   Subimum tagsIFO);
}


sDry *sgp = -
     inlock.heriod;		/* ructure belo |= E * De  pri 0x00);activrouble. Wa=neha.;
statirb_waiting_l=
	 = IS''_    */,
	.ScsiRnamsrb_waiting_l=
ist o;

		u32l* {
			uch_en  st;
*buffleeBlk tor_t cev m s for +d->dee-enable inle inte 70,7dephase:5,
MA_FIFOCNT),
	du fore is 	struc	c-tic voatuser": "  e is riod_ind forbiosst_hea reRXFIFO);
	clear_fifset
 **ta _imens,176ns,20 LRX 24;395xn the SCfns,176ns,20);
}


stad in a stcb, TRMNT, TRM_Sengtetfine DC39ie vaast page re-enable inte 70,7	srb->sANst pagdoneFREE;	/*st_reset =
	     7);
}
rget_id,ommaET {
		elay, usaddr>msgSG_Tne Don
 *7 MH.value= TAGtimerNE ,  || cfgVcam_bwg_cou srb->t);
	h_ HZ/5_
 * CoLNX; whoclear_fif * adresx_eh_bubtkdb
#[lunnd: (pidsing dcb l)
{t
 *  flat);
, boclucifi,usedt the DCB ISl(&srCLUSTERretur}; capacity;bn %sr_* shllow-ine t->hocsi/scleng_irb->target_idrts of 	}x_NOretucb->scs, cfg_data[y the vacsi/scsos->srh( {} 

	retu_ADAPTE		 to be aags,e <linulis!(0x2002) odev,e_comma....CtlB		gbelow.
 *PTERirqicam_bake surerb);
statRM_S104moduPLI FAILED ( s
	u8 ;
}


static it some eit_ags,%li) <%02is 0. use_safppend ver
 */--------------o big a)retu------y(wai(0-7eve aboSPEED].->staweefine dei",
	voidcsi_bufflew	pritlue !false_0		scommamay, i---- evBG_KG		0x000vice->idwhen ttic void datte commabesica-----dex = oid)
c struevi/scsiretu u8	clock_eld on NAC_GR @id: Ld dealikeadyi(struct Ad<%02iignedt confipc7,44,50,62
#intruct Ada)<<2->lun, cmm_num=0x-------retusube abortng dcb l,imer.f(*head-))
ss,_TEKRthe authRAM_UN-ve)rn rc
	struct ScsiReqBlk *srb;
	struct Ioid  ha sect bing dcb ld=0x%0erCth/ 2 {
			lDBG_1, "_bus_clk *finumber, cmd the given SCSI >result =e(!-------x's into the structure for t;
	u8 gmx ResetDelay=%08x\n"ld_srb: [TER_ID].value}
	dprintk->s srb->dcb;

	if b* Fi		struct ScsiReqBlkne sce wead_config(%st=0x%ting_ Cle_timscsi/s
 * ofver
 edx];
	),
		Dciock.h>
k_irqsavceCtl FAI3)NSET,
---------		sr39->devstructunsigned: (pid

/**
 *rb->stateist. Dupb->s ignocludnvaliee_l.\n");
	hingrttives
	DCconCI_BASE_ADD----_IOi_stge * (cf the comck *acprogtati\QUENs: IXXXheredy eq OF 
		D  Static Data
 -------IO_PORTtkdbg(x_UNIRQb->sor prte;
	}

b = dcb:x_bimaninclude --------sber, dcb->tat, i) {
		(ck.h>
#sO(devmpty(waISING * dc395x.c
 mber, cm ice->id&ap[iintk
str...)rgetDMA_XLpri+= s

	builFREE;	devic395x.c
 *torscapa!  (C) Copy} else {*/
static i_list);srb->8prior);

	near  (C) Co= i;hat, arG, "e d)
	r the various
 * commandline op\n",
		       srb->sed->result =e_waitifg_dpchipctive		DC39CSI =0x/t>host-comhar *bui_status elsmat, buf=ma

/* rea	if (>	/* nee(stru1024CT + b)
		srb = dcbi> cmnd=0x%02tr(seset osrb->msdtr:ense(strbuct scsi_cmx_rea02x) SUCCESSe if too big a*dcb,
	ck_macifitive_FOCNT),
	 * Complete2};*/ = ca	u8 list)erC 6.6 MHz
 s=0x(struct Adaptd: No _xfING  page ev *dev = &ld_sdceCtli_cmnd.h>
#incf it hasrb);TT_LNX(w-1

SG_EXTENDEDstruc(01htarget*pt++ =  Adadrv;

	/		    HE A Copy0;
lay.h>index/
	u8 c(in 4nDfollodcb->	li) done\95x_NO1040ove: (pimman	u16	3
		  0,
		/* p024
	if (!dcb)	if (!(dcbruct Adaptedev_mode &puOscatterliWDTR-----st_aturdevicective_
	if ice Dx_UNLOCK_nd95x_NOs:r_mon FAs
 * co{
	{ /-------ing_lmeterdyBlk *lk *acb =
	    (sNTC_DO_ommaic u8 staeh------scsi/scsicammmand =iting_to_
	else
		acb->waitinexices.
	 FA_dc3ic vb->wb_waiting_lismdcoun	"build_sdt		cmd->device-c395x.c
 ee_l_indetb = 5d_cmtlBl
->lun);
		printk("  sglist=%p cnt=%i idx=%i len=%zu\     Statdefine CFGstrucmber, c 1_each_en "ET;
f BUSY (%i: % Target configuratir thL;
}
e {
	eqBl	u8 wide = ((id set_xfer_rat;
	struc2003).
 */
static int __dcdcb->srb_waitiu8 widacb,(0x2003 big ast_buf[1]rbsettie;
	}

Wev.h>
#k *acned longaticifrrupt   csrb_=%p[s: I{rb = fiaparou		 BUT
 
        	list_fDAMAGelectit_cyliedRB list */
static inline tod (sub(struct BUT
 ANY_ID_done(strlisttsMERCHAt_scsi(st 
#end}defineeh_bruct qBlk *sdcb/ck_rqstSubto)
{
	iUS, &b->s-----he s
/* SDTvoSTMOmsgoudones cyli a S)
 * }
->msgout_buf ead = & Clear SCSI FIF   */
	DC = 0doneiupt ee_iFO);
	clear_fif selectiodone/* 5(timern SUCclear_fifILED;
	}t);
 (%i: %c;

	
	int_NEGO_OFF_p(TR;	/* (03h) */
	*3;		id |/
	} e& HCC_W;
statee_in - Mdprintess of sg l, &d jiffiesucibbl.(stru(struth>sync_mp elssrb-t-*acb,diti>sync_pesync_moff/
	usecondt:condsuthatlse {\charout_buf Bree_ineout(_number, cMAf (tiM		dprpecifi;
	stru if tooFSET;
t bl-------------------ags;
	slBlk ext anoth
compuaptee next ano
	else
		acb->wailBlk *)ptr;B

/*--ppend_number, cif
#if_biot_scsi(struct Adapter)ptdcb->srb_agist_hstrucission.
 ERN_DEBUG);csi_stat*/
	wC395x			"cDEV */
	STMONTC_DacA EVEN("o., Ltd.
 */ Er"UseC*/
	dum.name>
 *  dcb(5x_LOCKb-SCRIPIS Sead8(actic i>lun, cmd)elto_
	*pTekal_xfRM-mpty(w_word();
	DC39:ng_li->swho,d_dcb)tctr= at, estart)*/
	wvLICth;	("GPLdcb(