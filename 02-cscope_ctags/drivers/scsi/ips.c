/*****************************************************************************/
/* ips.c -- driver for the Adaptec / IBM ServeRAID controller                */
/*                                                                           */
/* Written By: Keith Mitchell, IBM Corporation                               */
/*             Jack Hammer, Adaptec, Inc.                                    */
/*             David Jeffery, Adaptec, Inc.                                  */
/*                                                                           */
/* Copyright (C) 2000 IBM Corporation                                        */
/* Copyright (C) 2002,2003 Adaptec, Inc.                                     */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation; either version 2 of the License, or         */
/* (at your option) any later version.                                       */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* NO WARRANTY                                                               */
/* THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR        */
/* CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT      */
/* LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,      */
/* MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is    */
/* solely responsible for determining the appropriateness of using and       */
/* distributing the Program and assumes all risks associated with its        */
/* exercise of rights under this Agreement, including but not limited to     */
/* the risks and costs of program errors, damage to or loss of data,         */
/* programs or equipment, and unavailability or interruption of operations.  */
/*                                                                           */
/* DISCLAIMER OF LIABILITY                                                   */
/* NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY   */
/* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL        */
/* DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND   */
/* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR     */
/* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE    */
/* USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED  */
/* HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES             */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program; if not, write to the Free Software               */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */
/*                                                                           */
/* Bugs/Comments/Suggestions about this driver should be mailed to:          */
/*      ipslinux@adaptec.com        	                                     */
/*                                                                           */
/* For system support issues, contact your local IBM Customer support.       */
/* Directions to find IBM Customer Support for each country can be found at: */
/*      http://www.ibm.com/planetwide/                                       */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Change Log                                                                */
/*                                                                           */
/* 0.99.02  - Breakup commands that are bigger than 8 * the stripe size      */
/* 0.99.03  - Make interrupt routine handle all completed request on the     */
/*            adapter not just the first one                                 */
/*          - Make sure passthru commands get woken up if we run out of      */
/*            SCBs                                                           */
/*          - Send all of the commands on the queue at once rather than      */
/*            one at a time since the card will support it.                  */
/* 0.99.04  - Fix race condition in the passthru mechanism -- this required  */
/*            the interface to the utilities to change                       */
/*          - Fix error recovery code                                        */
/* 0.99.05  - Fix an oops when we get certain passthru commands              */
/* 1.00.00  - Initial Public Release                                         */
/*            Functionally equivalent to 0.99.05                             */
/* 3.60.00  - Bump max commands to 128 for use with firmware 3.60            */
/*          - Change version to 3.60 to coincide with release numbering.     */
/* 3.60.01  - Remove bogus error check in passthru routine                   */
/* 3.60.02  - Make DCDB direction based on lookup table                      */
/*          - Only allow one DCDB command to a SCSI ID at a time             */
/* 4.00.00  - Add support for ServeRAID 4                                    */
/* 4.00.01  - Add support for First Failure Data Capture                     */
/* 4.00.02  - Fix problem with PT DCDB with no buffer                        */
/* 4.00.03  - Add alternative passthru interface                             */
/*          - Add ability to flash BIOS                                      */
/* 4.00.04  - Rename structures/constants to be prefixed with IPS_           */
/* 4.00.05  - Remove wish_block from init routine                            */
/*          - Use linux/spinlock.h instead of asm/spinlock.h for kernels     */
/*            2.3.18 and later                                               */
/*          - Sync with other changes from the 2.3 kernels                   */
/* 4.00.06  - Fix timeout with initial FFDC command                          */
/* 4.00.06a - Port to 2.4 (trivial) -- Christoph Hellwig <hch@infradead.org> */
/* 4.10.00  - Add support for ServeRAID 4M/4L                                */
/* 4.10.13  - Fix for dynamic unload and proc file system                    */
/* 4.20.03  - Rename version to coincide with new release schedules          */
/*            Performance fixes                                              */
/*            Fix truncation of /proc files with cat                         */
/*            Merge in changes through kernel 2.4.0test1ac21                 */
/* 4.20.13  - Fix some failure cases / reset code                            */
/*          - Hook into the reboot_notifier to flush the controller cache    */
/* 4.50.01  - Fix problem when there is a hole in logical drive numbering    */
/* 4.70.09  - Use a Common ( Large Buffer ) for Flashing from the JCRM CD    */
/*          - Add IPSSEND Flash Support                                      */
/*          - Set Sense Data for Unknown SCSI Command                        */
/*          - Use Slot Number from NVRAM Page 5                              */
/*          - Restore caller's DCDB Structure                                */
/* 4.70.12  - Corrective actions for bad controller ( during initialization )*/
/* 4.70.13  - Don't Send CDB's if we already know the device is not present  */
/*          - Don't release HA Lock in ips_next() until SC taken off queue   */
/*          - Unregister SCSI device in ips_release()                        */
/* 4.70.15  - Fix Breakup for very large ( non-SG ) requests in ips_done()   */
/* 4.71.00  - Change all memory allocations to not use GFP_DMA flag          */
/*            Code Clean-Up for 2.4.x kernel                                 */
/* 4.72.00  - Allow for a Scatter-Gather Element to exceed MAX_XFER Size     */
/* 4.72.01  - I/O Mapped Memory release ( so "insmod ips" does not Fail )    */
/*          - Don't Issue Internal FFDC Command if there are Active Commands */
/*          - Close Window for getting too many IOCTL's active               */
/* 4.80.00  - Make ia64 Safe                                                 */
/* 4.80.04  - Eliminate calls to strtok() if 2.4.x or greater                */
/*          - Adjustments to Device Queue Depth                              */
/* 4.80.14  - Take all semaphores off stack                                  */
/*          - Clean Up New_IOCTL path                                        */
/* 4.80.20  - Set max_sectors in Scsi_Host structure ( if >= 2.4.7 kernel )  */
/*          - 5 second delay needed after resetting an i960 adapter          */
/* 4.80.26  - Clean up potential code problems ( Arjan's recommendations )   */
/* 4.90.01  - Version Matching for FirmWare, BIOS, and Driver                */
/* 4.90.05  - Use New PCI Architecture to facilitate Hot Plug Development    */
/* 4.90.08  - Increase Delays in Flashing ( Trombone Only - 4H )             */
/* 4.90.08  - Data Corruption if First Scatter Gather Element is > 64K       */
/* 4.90.11  - Don't actually RESET unless it's physically required           */
/*          - Remove unused compile options                                  */
/* 5.00.01  - Sarasota ( 5i ) adapters must always be scanned first          */
/*          - Get rid on IOCTL_NEW_COMMAND code                              */
/*          - Add Extended DCDB Commands for Tape Support in 5I              */
/* 5.10.12  - use pci_dma interfaces, update for 2.5 kernel changes          */
/* 5.10.15  - remove unused code (sem, macros, etc.)                         */
/* 5.30.00  - use __devexit_p()                                              */
/* 6.00.00  - Add 6x Adapters and Battery Flash                              */
/* 6.10.00  - Remove 1G Addressing Limitations                               */
/* 6.11.xx  - Get VersionInfo buffer off the stack !              DDTS 60401 */
/* 6.11.xx  - Make Logical Drive Info structure safe for DMA      DDTS 60639 */
/* 7.10.18  - Add highmem_io flag in SCSI Templete for 2.4 kernels           */
/*          - Fix path/name for scsi_hosts.h include for 2.6 kernels         */
/*          - Fix sort order of 7k                                           */
/*          - Remove 3 unused "inline" functions                             */
/* 7.12.xx  - Use STATIC functions whereever possible                        */
/*          - Clean up deprecated MODULE_PARM calls                          */
/* 7.12.05  - Remove Version Matching per IBM request                        */
/*****************************************************************************/

/*
 * Conditional Compilation directives for this driver:
 *
 * IPS_DEBUG            - Turn on debugging info
 *
 * Parameters:
 *
 * debug:<number>       - Set debug level to <number>
 *                        NOTE: only works when IPS_DEBUG compile directive is used.
 *       1              - Normal debug messages
 *       2              - Verbose debug messages
 *       11             - Method trace (non interrupt)
 *       12             - Method trace (includes interrupt)
 *
 * noi2o                - Don't use I2O Queues (ServeRAID 4 only)
 * nommap               - Don't use memory mapped I/O
 * ioctlsize            - Initial size of the IOCTL buffer
 */

#include <asm/io.h>
#include <asm/byteorder.h>
#include <asm/page.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>

#include <linux/blkdev.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>

#include <scsi/sg.h>
#include "scsi.h"
#include <scsi/scsi_host.h>

#include "ips.h"

#include <linux/module.h>

#include <linux/stat.h>

#include <linux/spinlock.h>
#include <linux/init.h>

#include <linux/smp.h>

#ifdef MODULE
static char *ips = NULL;
module_param(ips, charp, 0);
#endif

/*
 * DRIVER_VER
 */
#define IPS_VERSION_HIGH        IPS_VER_MAJOR_STRING "." IPS_VER_MINOR_STRING
#define IPS_VERSION_LOW         "." IPS_VER_BUILD_STRING " "

#if !defined(__i386__) && !defined(__ia64__) && !defined(__x86_64__)
#warning "This driver has only been tested on the x86/ia64/x86_64 platforms"
#endif

#define IPS_DMA_DIR(scb) ((!scb->scsi_cmd || ips_is_passthru(scb->scsi_cmd) || \
                         DMA_NONE == scb->scsi_cmd->sc_data_direction) ? \
                         PCI_DMA_BIDIRECTIONAL : \
                         scb->scsi_cmd->sc_data_direction)

#ifdef IPS_DEBUG
#define METHOD_TRACE(s, i)    if (ips_debug >= (i+10)) printk(KERN_NOTICE s "\n");
#define DEBUG(i, s)           if (ips_debug >= i) printk(KERN_NOTICE s "\n");
#define DEBUG_VAR(i, s, v...) if (ips_debug >= i) printk(KERN_NOTICE s "\n", v);
#else
#define METHOD_TRACE(s, i)
#define DEBUG(i, s)
#define DEBUG_VAR(i, s, v...)
#endif

/*
 * Function prototypes
 */
static int ips_detect(struct scsi_host_template *);
static int ips_release(struct Scsi_Host *);
static int ips_eh_abort(struct scsi_cmnd *);
static int ips_eh_reset(struct scsi_cmnd *);
static int ips_queue(struct scsi_cmnd *, void (*)(struct scsi_cmnd *));
static const char *ips_info(struct Scsi_Host *);
static irqreturn_t do_ipsintr(int, void *);
static int ips_hainit(ips_ha_t *);
static int ips_map_status(ips_ha_t *, ips_scb_t *, ips_stat_t *);
static int ips_send_wait(ips_ha_t *, ips_scb_t *, int, int);
static int ips_send_cmd(ips_ha_t *, ips_scb_t *);
static int ips_online(ips_ha_t *, ips_scb_t *);
static int ips_inquiry(ips_ha_t *, ips_scb_t *);
static int ips_rdcap(ips_ha_t *, ips_scb_t *);
static int ips_msense(ips_ha_t *, ips_scb_t *);
static int ips_reqsen(ips_ha_t *, ips_scb_t *);
static int ips_deallocatescbs(ips_ha_t *, int);
static int ips_allocatescbs(ips_ha_t *);
static int ips_reset_copperhead(ips_ha_t *);
static int ips_reset_copperhead_memio(ips_ha_t *);
static int ips_reset_morpheus(ips_ha_t *);
static int ips_issue_copperhead(ips_ha_t *, ips_scb_t *);
static int ips_issue_copperhead_memio(ips_ha_t *, ips_scb_t *);
static int ips_issue_i2o(ips_ha_t *, ips_scb_t *);
static int ips_issue_i2o_memio(ips_ha_t *, ips_scb_t *);
static int ips_isintr_copperhead(ips_ha_t *);
static int ips_isintr_copperhead_memio(ips_ha_t *);
static int ips_isintr_morpheus(ips_ha_t *);
static int ips_wait(ips_ha_t *, int, int);
static int ips_write_driver_status(ips_ha_t *, int);
static int ips_read_adapter_status(ips_ha_t *, int);
static int ips_read_subsystem_parameters(ips_ha_t *, int);
static int ips_read_config(ips_ha_t *, int);
static int ips_clear_adapter(ips_ha_t *, int);
static int ips_readwrite_page5(ips_ha_t *, int, int);
static int ips_init_copperhead(ips_ha_t *);
static int ips_init_copperhead_memio(ips_ha_t *);
static int ips_init_morpheus(ips_ha_t *);
static int ips_isinit_copperhead(ips_ha_t *);
static int ips_isinit_copperhead_memio(ips_ha_t *);
static int ips_isinit_morpheus(ips_ha_t *);
static int ips_erase_bios(ips_ha_t *);
static int ips_program_bios(ips_ha_t *, char *, uint32_t, uint32_t);
static int ips_verify_bios(ips_ha_t *, char *, uint32_t, uint32_t);
static int ips_erase_bios_memio(ips_ha_t *);
static int ips_program_bios_memio(ips_ha_t *, char *, uint32_t, uint32_t);
static int ips_verify_bios_memio(ips_ha_t *, char *, uint32_t, uint32_t);
static int ips_flash_copperhead(ips_ha_t *, ips_passthru_t *, ips_scb_t *);
static int ips_flash_bios(ips_ha_t *, ips_passthru_t *, ips_scb_t *);
static int ips_flash_firmware(ips_ha_t *, ips_passthru_t *, ips_scb_t *);
static void ips_free_flash_copperhead(ips_ha_t * ha);
static void ips_get_bios_version(ips_ha_t *, int);
static void ips_identify_controller(ips_ha_t *);
static void ips_chkstatus(ips_ha_t *, IPS_STATUS *);
static void ips_enable_int_copperhead(ips_ha_t *);
static void ips_enable_int_copperhead_memio(ips_ha_t *);
static void ips_enable_int_morpheus(ips_ha_t *);
static int ips_intr_copperhead(ips_ha_t *);
static int ips_intr_morpheus(ips_ha_t *);
static void ips_next(ips_ha_t *, int);
static void ipsintr_blocking(ips_ha_t *, struct ips_scb *);
static void ipsintr_done(ips_ha_t *, struct ips_scb *);
static void ips_done(ips_ha_t *, ips_scb_t *);
static void ips_free(ips_ha_t *);
static void ips_init_scb(ips_ha_t *, ips_scb_t *);
static void ips_freescb(ips_ha_t *, ips_scb_t *);
static void ips_setup_funclist(ips_ha_t *);
static void ips_statinit(ips_ha_t *);
static void ips_statinit_memio(ips_ha_t *);
static void ips_fix_ffdc_time(ips_ha_t *, ips_scb_t *, time_t);
static void ips_ffdc_reset(ips_ha_t *, int);
static void ips_ffdc_time(ips_ha_t *);
static uint32_t ips_statupd_copperhead(ips_ha_t *);
static uint32_t ips_statupd_copperhead_memio(ips_ha_t *);
static uint32_t ips_statupd_morpheus(ips_ha_t *);
static ips_scb_t *ips_getscb(ips_ha_t *);
static void ips_putq_scb_head(ips_scb_queue_t *, ips_scb_t *);
static void ips_putq_wait_tail(ips_wait_queue_t *, struct scsi_cmnd *);
static void ips_putq_copp_tail(ips_copp_queue_t *,
				      ips_copp_wait_item_t *);
static ips_scb_t *ips_removeq_scb_head(ips_scb_queue_t *);
static ips_scb_t *ips_removeq_scb(ips_scb_queue_t *, ips_scb_t *);
static struct scsi_cmnd *ips_removeq_wait_head(ips_wait_queue_t *);
static struct scsi_cmnd *ips_removeq_wait(ips_wait_queue_t *,
					  struct scsi_cmnd *);
static ips_copp_wait_item_t *ips_removeq_copp(ips_copp_queue_t *,
						     ips_copp_wait_item_t *);
static ips_copp_wait_item_t *ips_removeq_copp_head(ips_copp_queue_t *);

static int ips_is_passthru(struct scsi_cmnd *);
static int ips_make_passthru(ips_ha_t *, struct scsi_cmnd *, ips_scb_t *, int);
static int ips_usrcmd(ips_ha_t *, ips_passthru_t *, ips_scb_t *);
static void ips_cleanup_passthru(ips_ha_t *, ips_scb_t *);
static void ips_scmd_buf_write(struct scsi_cmnd * scmd, void *data,
			       unsigned int count);
static void ips_scmd_buf_read(struct scsi_cmnd * scmd, void *data,
			      unsigned int count);

static int ips_proc_info(struct Scsi_Host *, char *, char **, off_t, int, int);
static int ips_host_info(ips_ha_t *, char *, off_t, int);
static void copy_mem_info(IPS_INFOSTR *, char *, int);
static int copy_info(IPS_INFOSTR *, char *, ...);
static int ips_abort_init(ips_ha_t * ha, int index);
static int ips_init_phase2(int index);

static int ips_init_phase1(struct pci_dev *pci_dev, int *indexPtr);
static int ips_register_scsi(int index);

static int  ips_poll_for_flush_complete(ips_ha_t * ha);
static void ips_flush_and_reset(ips_ha_t *ha);

/*
 * global variables
 */
static const char ips_name[] = "ips";
static struct Scsi_Host *ips_sh[IPS_MAX_ADAPTERS];	/* Array of host controller structures */
static ips_ha_t *ips_ha[IPS_MAX_ADAPTERS];	/* Array of HA structures */
static unsigned int ips_next_controller;
static unsigned int ips_num_controllers;
static unsigned int ips_released_controllers;
static int ips_hotplug;
static int ips_cmd_timeout = 60;
static int ips_reset_timeout = 60 * 5;
static int ips_force_memio = 1;		/* Always use Memory Mapped I/O    */
static int ips_force_i2o = 1;	/* Always use I2O command delivery */
static int ips_ioctlsize = IPS_IOCTL_SIZE;	/* Size of the ioctl buffer        */
static int ips_cd_boot;			/* Booting from Manager CD         */
static char *ips_FlashData = NULL;	/* CD Boot - Flash Data Buffer      */
static dma_addr_t ips_flashbusaddr;
static long ips_FlashDataInUse;		/* CD Boot - Flash Data In Use Flag */
static uint32_t MaxLiteCmds = 32;	/* Max Active Cmds for a Lite Adapter */
static struct scsi_host_template ips_driver_template = {
	.detect			= ips_detect,
	.release		= ips_release,
	.info			= ips_info,
	.queuecommand		= ips_queue,
	.eh_abort_handler	= ips_eh_abort,
	.eh_host_reset_handler	= ips_eh_reset,
	.proc_name		= "ips",
	.proc_info		= ips_proc_info,
	.slave_configure	= ips_slave_configure,
	.bios_param		= ips_biosparam,
	.this_id		= -1,
	.sg_tablesize		= IPS_MAX_SG,
	.cmd_per_lun		= 3,
	.use_clustering		= ENABLE_CLUSTERING,
};


/* This table describes all ServeRAID Adapters */
static struct  pci_device_id  ips_pci_table[] = {
	{ 0x1014, 0x002E, PCI_ANY_ID, PCI_ANY_ID, 0, 0 },
	{ 0x1014, 0x01BD, PCI_ANY_ID, PCI_ANY_ID, 0, 0 },
	{ 0x9005, 0x0250, PCI_ANY_ID, PCI_ANY_ID, 0, 0 },
	{ 0, }
};

MODULE_DEVICE_TABLE( pci, ips_pci_table );

static char ips_hot_plug_name[] = "ips";

static int __devinit  ips_insert_device(struct pci_dev *pci_dev, const struct pci_device_id *ent);
static void __devexit ips_remove_device(struct pci_dev *pci_dev);

static struct pci_driver ips_pci_driver = {
	.name		= ips_hot_plug_name,
	.id_table	= ips_pci_table,
	.probe		= ips_insert_device,
	.remove		= __devexit_p(ips_remove_device),
};


/*
 * Necessary forward function protoypes
 */
static int ips_halt(struct notifier_block *nb, ulong event, void *buf);

#define MAX_ADAPTER_NAME 15

static char ips_adapter_name[][30] = {
	"ServeRAID",
	"ServeRAID II",
	"ServeRAID on motherboard",
	"ServeRAID on motherboard",
	"ServeRAID 3H",
	"ServeRAID 3L",
	"ServeRAID 4H",
	"ServeRAID 4M",
	"ServeRAID 4L",
	"ServeRAID 4Mx",
	"ServeRAID 4Lx",
	"ServeRAID 5i",
	"ServeRAID 5i",
	"ServeRAID 6M",
	"ServeRAID 6i",
	"ServeRAID 7t",
	"ServeRAID 7k",
	"ServeRAID 7M"
};

static struct notifier_block ips_notifier = {
	ips_halt, NULL, 0
};

/*
 * Direction table
 */
static char ips_command_direction[] = {
	IPS_DATA_NONE, IPS_DATA_NONE, IPS_DATA_IN, IPS_DATA_IN, IPS_DATA_OUT,
	IPS_DATA_IN, IPS_DATA_IN, IPS_DATA_OUT, IPS_DATA_IN, IPS_DATA_UNK,
	IPS_DATA_OUT, IPS_DATA_OUT, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_IN, IPS_DATA_NONE, IPS_DATA_NONE, IPS_DATA_IN, IPS_DATA_OUT,
	IPS_DATA_IN, IPS_DATA_OUT, IPS_DATA_NONE, IPS_DATA_NONE, IPS_DATA_OUT,
	IPS_DATA_NONE, IPS_DATA_IN, IPS_DATA_NONE, IPS_DATA_IN, IPS_DATA_OUT,
	IPS_DATA_NONE, IPS_DATA_UNK, IPS_DATA_IN, IPS_DATA_UNK, IPS_DATA_IN,
	IPS_DATA_UNK, IPS_DATA_OUT, IPS_DATA_IN, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_IN, IPS_DATA_IN, IPS_DATA_OUT, IPS_DATA_NONE, IPS_DATA_UNK,
	IPS_DATA_IN, IPS_DATA_OUT, IPS_DATA_OUT, IPS_DATA_OUT, IPS_DATA_OUT,
	IPS_DATA_OUT, IPS_DATA_NONE, IPS_DATA_IN, IPS_DATA_NONE, IPS_DATA_NONE,
	IPS_DATA_IN, IPS_DATA_OUT, IPS_DATA_OUT, IPS_DATA_OUT, IPS_DATA_OUT,
	IPS_DATA_IN, IPS_DATA_OUT, IPS_DATA_IN, IPS_DATA_OUT, IPS_DATA_OUT,
	IPS_DATA_OUT, IPS_DATA_IN, IPS_DATA_IN, IPS_DATA_IN, IPS_DATA_NONE,
	IPS_DATA_UNK, IPS_DATA_NONE, IPS_DATA_NONE, IPS_DATA_NONE, IPS_DATA_UNK,
	IPS_DATA_NONE, IPS_DATA_OUT, IPS_DATA_IN, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_OUT, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_IN, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_NONE, IPS_DATA_NONE, IPS_DATA_UNK, IPS_DATA_IN, IPS_DATA_NONE,
	IPS_DATA_OUT, IPS_DATA_UNK, IPS_DATA_NONE, IPS_DATA_UNK, IPS_DATA_OUT,
	IPS_DATA_OUT, IPS_DATA_OUT, IPS_DATA_OUT, IPS_DATA_OUT, IPS_DATA_NONE,
	IPS_DATA_UNK, IPS_DATA_IN, IPS_DATA_OUT, IPS_DATA_IN, IPS_DATA_IN,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_OUT,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK
};


/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_setup                                                  */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   setup parameters to the driver                                         */
/*                                                                          */
/****************************************************************************/
static int
ips_setup(char *ips_str)
{

	int i;
	char *key;
	char *value;
	IPS_OPTION options[] = {
		{"noi2o", &ips_force_i2o, 0},
		{"nommap", &ips_force_memio, 0},
		{"ioctlsize", &ips_ioctlsize, IPS_IOCTL_SIZE},
		{"cdboot", &ips_cd_boot, 0},
		{"maxcmds", &MaxLiteCmds, 32},
	};

	/* Don't use strtok() anymore ( if 2.4 Kernel or beyond ) */
	/* Search for value */
	while ((key = strsep(&ips_str, ",."))) {
		if (!*key)
			continue;
		value = strchr(key, ':');
		if (value)
			*value++ = '\0';
		/*
		 * We now have key/value pairs.
		 * Update the variables
		 */
		for (i = 0; i < ARRAY_SIZE(options); i++) {
			if (strnicmp
			    (key, options[i].option_name,
			     strlen(options[i].option_name)) == 0) {
				if (value)
					*options[i].option_flag =
					    simple_strtoul(value, NULL, 0);
				else
					*options[i].option_flag =
					    options[i].option_value;
				break;
			}
		}
	}

	return (1);
}

__setup("ips=", ips_setup);

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_detect                                                 */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Detect and initialize the driver                                       */
/*                                                                          */
/* NOTE: this routine is called under the io_request_lock spinlock          */
/*                                                                          */
/****************************************************************************/
static int
ips_detect(struct scsi_host_template * SHT)
{
	int i;

	METHOD_TRACE("ips_detect", 1);

#ifdef MODULE
	if (ips)
		ips_setup(ips);
#endif

	for (i = 0; i < ips_num_controllers; i++) {
		if (ips_register_scsi(i))
			ips_free(ips_ha[i]);
		ips_released_controllers++;
	}
	ips_hotplug = 1;
	return (ips_num_controllers);
}

/****************************************************************************/
/*   configure the function pointers to use the functions that will work    */
/*   with the found version of the adapter                                  */
/****************************************************************************/
static void
ips_setup_funclist(ips_ha_t * ha)
{

	/*
	 * Setup Functions
	 */
	if (IPS_IS_MORPHEUS(ha) || IPS_IS_MARCO(ha)) {
		/* morpheus / marco / sebring */
		ha->func.isintr = ips_isintr_morpheus;
		ha->func.isinit = ips_isinit_morpheus;
		ha->func.issue = ips_issue_i2o_memio;
		ha->func.init = ips_init_morpheus;
		ha->func.statupd = ips_statupd_morpheus;
		ha->func.reset = ips_reset_morpheus;
		ha->func.intr = ips_intr_morpheus;
		ha->func.enableint = ips_enable_int_morpheus;
	} else if (IPS_USE_MEMIO(ha)) {
		/* copperhead w/MEMIO */
		ha->func.isintr = ips_isintr_copperhead_memio;
		ha->func.isinit = ips_isinit_copperhead_memio;
		ha->func.init = ips_init_copperhead_memio;
		ha->func.statupd = ips_statupd_copperhead_memio;
		ha->func.statinit = ips_statinit_memio;
		ha->func.reset = ips_reset_copperhead_memio;
		ha->func.intr = ips_intr_copperhead;
		ha->func.erasebios = ips_erase_bios_memio;
		ha->func.programbios = ips_program_bios_memio;
		ha->func.verifybios = ips_verify_bios_memio;
		ha->func.enableint = ips_enable_int_copperhead_memio;
		if (IPS_USE_I2O_DELIVER(ha))
			ha->func.issue = ips_issue_i2o_memio;
		else
			ha->func.issue = ips_issue_copperhead_memio;
	} else {
		/* copperhead */
		ha->func.isintr = ips_isintr_copperhead;
		ha->func.isinit = ips_isinit_copperhead;
		ha->func.init = ips_init_copperhead;
		ha->func.statupd = ips_statupd_copperhead;
		ha->func.statinit = ips_statinit;
		ha->func.reset = ips_reset_copperhead;
		ha->func.intr = ips_intr_copperhead;
		ha->func.erasebios = ips_erase_bios;
		ha->func.programbios = ips_program_bios;
		ha->func.verifybios = ips_verify_bios;
		ha->func.enableint = ips_enable_int_copperhead;

		if (IPS_USE_I2O_DELIVER(ha))
			ha->func.issue = ips_issue_i2o;
		else
			ha->func.issue = ips_issue_copperhead;
	}
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_release                                                */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Remove a driver                                                        */
/*                                                                          */
/****************************************************************************/
static int
ips_release(struct Scsi_Host *sh)
{
	ips_scb_t *scb;
	ips_ha_t *ha;
	int i;

	METHOD_TRACE("ips_release", 1);

	scsi_remove_host(sh);

	for (i = 0; i < IPS_MAX_ADAPTERS && ips_sh[i] != sh; i++) ;

	if (i == IPS_MAX_ADAPTERS) {
		printk(KERN_WARNING
		       "(%s) release, invalid Scsi_Host pointer.\n", ips_name);
		BUG();
		return (FALSE);
	}

	ha = IPS_HA(sh);

	if (!ha)
		return (FALSE);

	/* flush the cache on the controller */
	scb = &ha->scbs[ha->max_cmds - 1];

	ips_init_scb(ha, scb);

	scb->timeout = ips_cmd_timeout;
	scb->cdb[0] = IPS_CMD_FLUSH;

	scb->cmd.flush_cache.op_code = IPS_CMD_FLUSH;
	scb->cmd.flush_cache.command_id = IPS_COMMAND_ID(ha, scb);
	scb->cmd.flush_cache.state = IPS_NORM_STATE;
	scb->cmd.flush_cache.reserved = 0;
	scb->cmd.flush_cache.reserved2 = 0;
	scb->cmd.flush_cache.reserved3 = 0;
	scb->cmd.flush_cache.reserved4 = 0;

	IPS_PRINTK(KERN_WARNING, ha->pcidev, "Flushing Cache.\n");

	/* send command */
	if (ips_send_wait(ha, scb, ips_cmd_timeout, IPS_INTR_ON) == IPS_FAILURE)
		IPS_PRINTK(KERN_WARNING, ha->pcidev, "Incomplete Flush.\n");

	IPS_PRINTK(KERN_WARNING, ha->pcidev, "Flushing Complete.\n");

	ips_sh[i] = NULL;
	ips_ha[i] = NULL;

	/* free extra memory */
	ips_free(ha);

	/* free IRQ */
	free_irq(ha->pcidev->irq, ha);

	scsi_host_put(sh);

	ips_released_controllers++;

	return (FALSE);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_halt                                                   */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Perform cleanup when the system reboots                                */
/*                                                                          */
/****************************************************************************/
static int
ips_halt(struct notifier_block *nb, ulong event, void *buf)
{
	ips_scb_t *scb;
	ips_ha_t *ha;
	int i;

	if ((event != SYS_RESTART) && (event != SYS_HALT) &&
	    (event != SYS_POWER_OFF))
		return (NOTIFY_DONE);

	for (i = 0; i < ips_next_controller; i++) {
		ha = (ips_ha_t *) ips_ha[i];

		if (!ha)
			continue;

		if (!ha->active)
			continue;

		/* flush the cache on the controller */
		scb = &ha->scbs[ha->max_cmds - 1];

		ips_init_scb(ha, scb);

		scb->timeout = ips_cmd_timeout;
		scb->cdb[0] = IPS_CMD_FLUSH;

		scb->cmd.flush_cache.op_code = IPS_CMD_FLUSH;
		scb->cmd.flush_cache.command_id = IPS_COMMAND_ID(ha, scb);
		scb->cmd.flush_cache.state = IPS_NORM_STATE;
		scb->cmd.flush_cache.reserved = 0;
		scb->cmd.flush_cache.reserved2 = 0;
		scb->cmd.flush_cache.reserved3 = 0;
		scb->cmd.flush_cache.reserved4 = 0;

		IPS_PRINTK(KERN_WARNING, ha->pcidev, "Flushing Cache.\n");

		/* send command */
		if (ips_send_wait(ha, scb, ips_cmd_timeout, IPS_INTR_ON) ==
		    IPS_FAILURE)
			IPS_PRINTK(KERN_WARNING, ha->pcidev,
				   "Incomplete Flush.\n");
		else
			IPS_PRINTK(KERN_WARNING, ha->pcidev,
				   "Flushing Complete.\n");
	}

	return (NOTIFY_OK);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_eh_abort                                               */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Abort a command (using the new error code stuff)                       */
/* Note: this routine is called under the io_request_lock                   */
/****************************************************************************/
int ips_eh_abort(struct scsi_cmnd *SC)
{
	ips_ha_t *ha;
	ips_copp_wait_item_t *item;
	int ret;
	struct Scsi_Host *host;

	METHOD_TRACE("ips_eh_abort", 1);

	if (!SC)
		return (FAILED);

	host = SC->device->host;
	ha = (ips_ha_t *) SC->device->host->hostdata;

	if (!ha)
		return (FAILED);

	if (!ha->active)
		return (FAILED);

	spin_lock(host->host_lock);

	/* See if the command is on the copp queue */
	item = ha->copp_waitlist.head;
	while ((item) && (item->scsi_cmd != SC))
		item = item->next;

	if (item) {
		/* Found it */
		ips_removeq_copp(&ha->copp_waitlist, item);
		ret = (SUCCESS);

		/* See if the command is on the wait queue */
	} else if (ips_removeq_wait(&ha->scb_waitlist, SC)) {
		/* command not sent yet */
		ret = (SUCCESS);
	} else {
		/* command must have already been sent */
		ret = (FAILED);
	}

	spin_unlock(host->host_lock);
	return ret;
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_eh_reset                                               */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Reset the controller (with new eh error code)                          */
/*                                                                          */
/* NOTE: this routine is called under the io_request_lock spinlock          */
/*                                                                          */
/****************************************************************************/
static int __ips_eh_reset(struct scsi_cmnd *SC)
{
	int ret;
	int i;
	ips_ha_t *ha;
	ips_scb_t *scb;
	ips_copp_wait_item_t *item;

	METHOD_TRACE("ips_eh_reset", 1);

#ifdef NO_IPS_RESET
	return (FAILED);
#else

	if (!SC) {
		DEBUG(1, "Reset called with NULL scsi command");

		return (FAILED);
	}

	ha = (ips_ha_t *) SC->device->host->hostdata;

	if (!ha) {
		DEBUG(1, "Reset called with NULL ha struct");

		return (FAILED);
	}

	if (!ha->active)
		return (FAILED);

	/* See if the command is on the copp queue */
	item = ha->copp_waitlist.head;
	while ((item) && (item->scsi_cmd != SC))
		item = item->next;

	if (item) {
		/* Found it */
		ips_removeq_copp(&ha->copp_waitlist, item);
		return (SUCCESS);
	}

	/* See if the command is on the wait queue */
	if (ips_removeq_wait(&ha->scb_waitlist, SC)) {
		/* command not sent yet */
		return (SUCCESS);
	}

	/* An explanation for the casual observer:                              */
	/* Part of the function of a RAID controller is automatic error         */
	/* detection and recovery.  As such, the only problem that physically   */
	/* resetting an adapter will ever fix is when, for some reason,         */
	/* the driver is not successfully communicating with the adapter.       */
	/* Therefore, we will attempt to flush this adapter.  If that succeeds, */
	/* then there's no real purpose in a physical reset. This will complete */
	/* much faster and avoids any problems that might be caused by a        */
	/* physical reset ( such as having to fail all the outstanding I/O's ). */

	if (ha->ioctl_reset == 0) {	/* IF Not an IOCTL Requested Reset */
		scb = &ha->scbs[ha->max_cmds - 1];

		ips_init_scb(ha, scb);

		scb->timeout = ips_cmd_timeout;
		scb->cdb[0] = IPS_CMD_FLUSH;

		scb->cmd.flush_cache.op_code = IPS_CMD_FLUSH;
		scb->cmd.flush_cache.command_id = IPS_COMMAND_ID(ha, scb);
		scb->cmd.flush_cache.state = IPS_NORM_STATE;
		scb->cmd.flush_cache.reserved = 0;
		scb->cmd.flush_cache.reserved2 = 0;
		scb->cmd.flush_cache.reserved3 = 0;
		scb->cmd.flush_cache.reserved4 = 0;

		/* Attempt the flush command */
		ret = ips_send_wait(ha, scb, ips_cmd_timeout, IPS_INTR_IORL);
		if (ret == IPS_SUCCESS) {
			IPS_PRINTK(KERN_NOTICE, ha->pcidev,
				   "Reset Request - Flushed Cache\n");
			return (SUCCESS);
		}
	}

	/* Either we can't communicate with the adapter or it's an IOCTL request */
	/* from a utility.  A physical reset is needed at this point.            */

	ha->ioctl_reset = 0;	/* Reset the IOCTL Requested Reset Flag */

	/*
	 * command must have already been sent
	 * reset the controller
	 */
	IPS_PRINTK(KERN_NOTICE, ha->pcidev, "Resetting controller.\n");
	ret = (*ha->func.reset) (ha);

	if (!ret) {
		struct scsi_cmnd *scsi_cmd;

		IPS_PRINTK(KERN_NOTICE, ha->pcidev,
			   "Controller reset failed - controller now offline.\n");

		/* Now fail all of the active commands */
		DEBUG_VAR(1, "(%s%d) Failing active commands",
			  ips_name, ha->host_num);

		while ((scb = ips_removeq_scb_head(&ha->scb_activelist))) {
			scb->scsi_cmd->result = DID_ERROR << 16;
			scb->scsi_cmd->scsi_done(scb->scsi_cmd);
			ips_freescb(ha, scb);
		}

		/* Now fail all of the pending commands */
		DEBUG_VAR(1, "(%s%d) Failing pending commands",
			  ips_name, ha->host_num);

		while ((scsi_cmd = ips_removeq_wait_head(&ha->scb_waitlist))) {
			scsi_cmd->result = DID_ERROR;
			scsi_cmd->scsi_done(scsi_cmd);
		}

		ha->active = FALSE;
		return (FAILED);
	}

	if (!ips_clear_adapter(ha, IPS_INTR_IORL)) {
		struct scsi_cmnd *scsi_cmd;

		IPS_PRINTK(KERN_NOTICE, ha->pcidev,
			   "Controller reset failed - controller now offline.\n");

		/* Now fail all of the active commands */
		DEBUG_VAR(1, "(%s%d) Failing active commands",
			  ips_name, ha->host_num);

		while ((scb = ips_removeq_scb_head(&ha->scb_activelist))) {
			scb->scsi_cmd->result = DID_ERROR << 16;
			scb->scsi_cmd->scsi_done(scb->scsi_cmd);
			ips_freescb(ha, scb);
		}

		/* Now fail all of the pending commands */
		DEBUG_VAR(1, "(%s%d) Failing pending commands",
			  ips_name, ha->host_num);

		while ((scsi_cmd = ips_removeq_wait_head(&ha->scb_waitlist))) {
			scsi_cmd->result = DID_ERROR << 16;
			scsi_cmd->scsi_done(scsi_cmd);
		}

		ha->active = FALSE;
		return (FAILED);
	}

	/* FFDC */
	if (le32_to_cpu(ha->subsys->param[3]) & 0x300000) {
		struct timeval tv;

		do_gettimeofday(&tv);
		ha->last_ffdc = tv.tv_sec;
		ha->reset_count++;
		ips_ffdc_reset(ha, IPS_INTR_IORL);
	}

	/* Now fail all of the active commands */
	DEBUG_VAR(1, "(%s%d) Failing active commands", ips_name, ha->host_num);

	while ((scb = ips_removeq_scb_head(&ha->scb_activelist))) {
		scb->scsi_cmd->result = DID_RESET << 16;
		scb->scsi_cmd->scsi_done(scb->scsi_cmd);
		ips_freescb(ha, scb);
	}

	/* Reset DCDB active command bits */
	for (i = 1; i < ha->nbus; i++)
		ha->dcdb_active[i - 1] = 0;

	/* Reset the number of active IOCTLs */
	ha->num_ioctl = 0;

	ips_next(ha, IPS_INTR_IORL);

	return (SUCCESS);
#endif				/* NO_IPS_RESET */

}

static int ips_eh_reset(struct scsi_cmnd *SC)
{
	int rc;

	spin_lock_irq(SC->device->host->host_lock);
	rc = __ips_eh_reset(SC);
	spin_unlock_irq(SC->device->host->host_lock);

	return rc;
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_queue                                                  */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Send a command to the controller                                       */
/*                                                                          */
/* NOTE:                                                                    */
/*    Linux obtains io_request_lock before calling this function            */
/*                                                                          */
/****************************************************************************/
static int ips_queue(struct scsi_cmnd *SC, void (*done) (struct scsi_cmnd *))
{
	ips_ha_t *ha;
	ips_passthru_t *pt;

	METHOD_TRACE("ips_queue", 1);

	ha = (ips_ha_t *) SC->device->host->hostdata;

	if (!ha)
		return (1);

	if (!ha->active)
		return (DID_ERROR);

	if (ips_is_passthru(SC)) {
		if (ha->copp_waitlist.count == IPS_MAX_IOCTL_QUEUE) {
			SC->result = DID_BUS_BUSY << 16;
			done(SC);

			return (0);
		}
	} else if (ha->scb_waitlist.count == IPS_MAX_QUEUE) {
		SC->result = DID_BUS_BUSY << 16;
		done(SC);

		return (0);
	}

	SC->scsi_done = done;

	DEBUG_VAR(2, "(%s%d): ips_queue: cmd 0x%X (%d %d %d)",
		  ips_name,
		  ha->host_num,
		  SC->cmnd[0],
		  SC->device->channel, SC->device->id, SC->device->lun);

	/* Check for command to initiator IDs */
	if ((scmd_channel(SC) > 0)
	    && (scmd_id(SC) == ha->ha_id[scmd_channel(SC)])) {
		SC->result = DID_NO_CONNECT << 16;
		done(SC);

		return (0);
	}

	if (ips_is_passthru(SC)) {

		ips_copp_wait_item_t *scratch;

		/* A Reset IOCTL is only sent by the boot CD in extreme cases.           */
		/* There can never be any system activity ( network or disk ), but check */
		/* anyway just as a good practice.                                       */
		pt = (ips_passthru_t *) scsi_sglist(SC);
		if ((pt->CoppCP.cmd.reset.op_code == IPS_CMD_RESET_CHANNEL) &&
		    (pt->CoppCP.cmd.reset.adapter_flag == 1)) {
			if (ha->scb_activelist.count != 0) {
				SC->result = DID_BUS_BUSY << 16;
				done(SC);
				return (0);
			}
			ha->ioctl_reset = 1;	/* This reset request is from an IOCTL */
			__ips_eh_reset(SC);
			SC->result = DID_OK << 16;
			SC->scsi_done(SC);
			return (0);
		}

		/* allocate space for the scribble */
		scratch = kmalloc(sizeof (ips_copp_wait_item_t), GFP_ATOMIC);

		if (!scratch) {
			SC->result = DID_ERROR << 16;
			done(SC);

			return (0);
		}

		scratch->scsi_cmd = SC;
		scratch->next = NULL;

		ips_putq_copp_tail(&ha->copp_waitlist, scratch);
	} else {
		ips_putq_wait_tail(&ha->scb_waitlist, SC);
	}

	ips_next(ha, IPS_INTR_IORL);

	return (0);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_biosparam                                              */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Set bios geometry for the controller                                   */
/*                                                                          */
/****************************************************************************/
static int ips_biosparam(struct scsi_device *sdev, struct block_device *bdev,
			 sector_t capacity, int geom[])
{
	ips_ha_t *ha = (ips_ha_t *) sdev->host->hostdata;
	int heads;
	int sectors;
	int cylinders;

	METHOD_TRACE("ips_biosparam", 1);

	if (!ha)
		/* ?!?! host adater info invalid */
		return (0);

	if (!ha->active)
		return (0);

	if (!ips_read_adapter_status(ha, IPS_INTR_ON))
		/* ?!?! Enquiry command failed */
		return (0);

	if ((capacity > 0x400000) && ((ha->enq->ucMiscFlag & 0x8) == 0)) {
		heads = IPS_NORM_HEADS;
		sectors = IPS_NORM_SECTORS;
	} else {
		heads = IPS_COMP_HEADS;
		sectors = IPS_COMP_SECTORS;
	}

	cylinders = (unsigned long) capacity / (heads * sectors);

	DEBUG_VAR(2, "Geometry: heads: %d, sectors: %d, cylinders: %d",
		  heads, sectors, cylinders);

	geom[0] = heads;
	geom[1] = sectors;
	geom[2] = cylinders;

	return (0);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_slave_configure                                        */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Set queue depths on devices once scan is complete                      */
/*                                                                          */
/****************************************************************************/
static int
ips_slave_configure(struct scsi_device * SDptr)
{
	ips_ha_t *ha;
	int min;

	ha = IPS_HA(SDptr->host);
	if (SDptr->tagged_supported && SDptr->type == TYPE_DISK) {
		min = ha->max_cmds / 2;
		if (ha->enq->ucLogDriveCount <= 2)
			min = ha->max_cmds - 1;
		scsi_adjust_queue_depth(SDptr, MSG_ORDERED_TAG, min);
	}

	SDptr->skip_ms_page_8 = 1;
	SDptr->skip_ms_page_3f = 1;
	return 0;
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: do_ipsintr                                                 */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Wrapper for the interrupt handler                                      */
/*                                                                          */
/****************************************************************************/
static irqreturn_t
do_ipsintr(int irq, void *dev_id)
{
	ips_ha_t *ha;
	struct Scsi_Host *host;
	int irqstatus;

	METHOD_TRACE("do_ipsintr", 2);

	ha = (ips_ha_t *) dev_id;
	if (!ha)
		return IRQ_NONE;
	host = ips_sh[ha->host_num];
	/* interrupt during initialization */
	if (!host) {
		(*ha->func.intr) (ha);
		return IRQ_HANDLED;
	}

	spin_lock(host->host_lock);

	if (!ha->active) {
		spin_unlock(host->host_lock);
		return IRQ_HANDLED;
	}

	irqstatus = (*ha->func.intr) (ha);

	spin_unlock(host->host_lock);

	/* start the next command */
	ips_next(ha, IPS_INTR_ON);
	return IRQ_RETVAL(irqstatus);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_intr_copperhead                                        */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Polling interrupt handler                                              */
/*                                                                          */
/*   ASSUMES interrupts are disabled                                        */
/*                                                                          */
/****************************************************************************/
int
ips_intr_copperhead(ips_ha_t * ha)
{
	ips_stat_t *sp;
	ips_scb_t *scb;
	IPS_STATUS cstatus;
	int intrstatus;

	METHOD_TRACE("ips_intr", 2);

	if (!ha)
		return 0;

	if (!ha->active)
		return 0;

	intrstatus = (*ha->func.isintr) (ha);

	if (!intrstatus) {
		/*
		 * Unexpected/Shared interrupt
		 */

		return 0;
	}

	while (TRUE) {
		sp = &ha->sp;

		intrstatus = (*ha->func.isintr) (ha);

		if (!intrstatus)
			break;
		else
			cstatus.value = (*ha->func.statupd) (ha);

		if (cstatus.fields.command_id > (IPS_MAX_CMDS - 1)) {
			/* Spurious Interrupt ? */
			continue;
		}

		ips_chkstatus(ha, &cstatus);
		scb = (ips_scb_t *) sp->scb_addr;

		/*
		 * use the callback function to finish things up
		 * NOTE: interrupts are OFF for this
		 */
		(*scb->callback) (ha, scb);
	}			/* end while */
	return 1;
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_intr_morpheus                                          */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Polling interrupt handler                                              */
/*                                                                          */
/*   ASSUMES interrupts are disabled                                        */
/*                                                                          */
/****************************************************************************/
int
ips_intr_morpheus(ips_ha_t * ha)
{
	ips_stat_t *sp;
	ips_scb_t *scb;
	IPS_STATUS cstatus;
	int intrstatus;

	METHOD_TRACE("ips_intr_morpheus", 2);

	if (!ha)
		return 0;

	if (!ha->active)
		return 0;

	intrstatus = (*ha->func.isintr) (ha);

	if (!intrstatus) {
		/*
		 * Unexpected/Shared interrupt
		 */

		return 0;
	}

	while (TRUE) {
		sp = &ha->sp;

		intrstatus = (*ha->func.isintr) (ha);

		if (!intrstatus)
			break;
		else
			cstatus.value = (*ha->func.statupd) (ha);

		if (cstatus.value == 0xffffffff)
			/* No more to process */
			break;

		if (cstatus.fields.command_id > (IPS_MAX_CMDS - 1)) {
			IPS_PRINTK(KERN_WARNING, ha->pcidev,
				   "Spurious interrupt; no ccb.\n");

			continue;
		}

		ips_chkstatus(ha, &cstatus);
		scb = (ips_scb_t *) sp->scb_addr;

		/*
		 * use the callback function to finish things up
		 * NOTE: interrupts are OFF for this
		 */
		(*scb->callback) (ha, scb);
	}			/* end while */
	return 1;
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_info                                                   */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Return info about the driver                                           */
/*                                                                          */
/****************************************************************************/
static const char *
ips_info(struct Scsi_Host *SH)
{
	static char buffer[256];
	char *bp;
	ips_ha_t *ha;

	METHOD_TRACE("ips_info", 1);

	ha = IPS_HA(SH);

	if (!ha)
		return (NULL);

	bp = &buffer[0];
	memset(bp, 0, sizeof (buffer));

	sprintf(bp, "%s%s%s Build %d", "IBM PCI ServeRAID ",
		IPS_VERSION_HIGH, IPS_VERSION_LOW, IPS_BUILD_IDENT);

	if (ha->ad_type > 0 && ha->ad_type <= MAX_ADAPTER_NAME) {
		strcat(bp, " <");
		strcat(bp, ips_adapter_name[ha->ad_type - 1]);
		strcat(bp, ">");
	}

	return (bp);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_proc_info                                              */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   The passthru interface for the driver                                  */
/*                                                                          */
/****************************************************************************/
static int
ips_proc_info(struct Scsi_Host *host, char *buffer, char **start, off_t offset,
	      int length, int func)
{
	int i;
	int ret;
	ips_ha_t *ha = NULL;

	METHOD_TRACE("ips_proc_info", 1);

	/* Find our host structure */
	for (i = 0; i < ips_next_controller; i++) {
		if (ips_sh[i]) {
			if (ips_sh[i] == host) {
				ha = (ips_ha_t *) ips_sh[i]->hostdata;
				break;
			}
		}
	}

	if (!ha)
		return (-EINVAL);

	if (func) {
		/* write */
		return (0);
	} else {
		/* read */
		if (start)
			*start = buffer;

		ret = ips_host_info(ha, buffer, offset, length);

		return (ret);
	}
}

/*--------------------------------------------------------------------------*/
/* Helper Functions                                                         */
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_is_passthru                                            */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Determine if the specified SCSI command is really a passthru command   */
/*                                                                          */
/****************************************************************************/
static int ips_is_passthru(struct scsi_cmnd *SC)
{
	unsigned long flags;

	METHOD_TRACE("ips_is_passthru", 1);

	if (!SC)
		return (0);

	if ((SC->cmnd[0] == IPS_IOCTL_COMMAND) &&
	    (SC->device->channel == 0) &&
	    (SC->device->id == IPS_ADAPTER_ID) &&
	    (SC->device->lun == 0) && scsi_sglist(SC)) {
                struct scatterlist *sg = scsi_sglist(SC);
                char  *buffer;

                /* kmap_atomic() ensures addressability of the user buffer.*/
                /* local_irq_save() protects the KM_IRQ0 address slot.     */
                local_irq_save(flags);
                buffer = kmap_atomic(sg_page(sg), KM_IRQ0) + sg->offset;
                if (buffer && buffer[0] == 'C' && buffer[1] == 'O' &&
                    buffer[2] == 'P' && buffer[3] == 'P') {
                        kunmap_atomic(buffer - sg->offset, KM_IRQ0);
                        local_irq_restore(flags);
                        return 1;
                }
                kunmap_atomic(buffer - sg->offset, KM_IRQ0);
                local_irq_restore(flags);
	}
	return 0;
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_alloc_passthru_buffer                                  */
/*                                                                          */
/* Routine Description:                                                     */
/*   allocate a buffer large enough for the ioctl data if the ioctl buffer  */
/*   is too small or doesn't exist                                          */
/****************************************************************************/
static int
ips_alloc_passthru_buffer(ips_ha_t * ha, int length)
{
	void *bigger_buf;
	dma_addr_t dma_busaddr;

	if (ha->ioctl_data && length <= ha->ioctl_len)
		return 0;
	/* there is no buffer or it's not big enough, allocate a new one */
	bigger_buf = pci_alloc_consistent(ha->pcidev, length, &dma_busaddr);
	if (bigger_buf) {
		/* free the old memory */
		pci_free_consistent(ha->pcidev, ha->ioctl_len, ha->ioctl_data,
				    ha->ioctl_busaddr);
		/* use the new memory */
		ha->ioctl_data = (char *) bigger_buf;
		ha->ioctl_len = length;
		ha->ioctl_busaddr = dma_busaddr;
	} else {
		return -1;
	}
	return 0;
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_make_passthru                                          */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Make a passthru command out of the info in the Scsi block              */
/*                                                                          */
/****************************************************************************/
static int
ips_make_passthru(ips_ha_t *ha, struct scsi_cmnd *SC, ips_scb_t *scb, int intr)
{
	ips_passthru_t *pt;
	int length = 0;
	int i, ret;
        struct scatterlist *sg = scsi_sglist(SC);

	METHOD_TRACE("ips_make_passthru", 1);

        scsi_for_each_sg(SC, sg, scsi_sg_count(SC), i)
		length += sg->length;

	if (length < sizeof (ips_passthru_t)) {
		/* wrong size */
		DEBUG_VAR(1, "(%s%d) Passthru structure wrong size",
			  ips_name, ha->host_num);
		return (IPS_FAILURE);
	}
	if (ips_alloc_passthru_buffer(ha, length)) {
		/* allocation failure!  If ha->ioctl_data exists, use it to return
		   some error codes.  Return a failed command to the scsi layer. */
		if (ha->ioctl_data) {
			pt = (ips_passthru_t *) ha->ioctl_data;
			ips_scmd_buf_read(SC, pt, sizeof (ips_passthru_t));
			pt->BasicStatus = 0x0B;
			pt->ExtendedStatus = 0x00;
			ips_scmd_buf_write(SC, pt, sizeof (ips_passthru_t));
		}
		return IPS_FAILURE;
	}
	ha->ioctl_datasize = length;

	ips_scmd_buf_read(SC, ha->ioctl_data, ha->ioctl_datasize);
	pt = (ips_passthru_t *) ha->ioctl_data;

	/*
	 * Some notes about the passthru interface used
	 *
	 * IF the scsi op_code == 0x0d then we assume
	 * that the data came along with/goes with the
	 * packet we received from the sg driver. In this
	 * case the CmdBSize field of the pt structure is
	 * used for the size of the buffer.
	 */

	switch (pt->CoppCmd) {
	case IPS_NUMCTRLS:
		memcpy(ha->ioctl_data + sizeof (ips_passthru_t),
		       &ips_num_controllers, sizeof (int));
		ips_scmd_buf_write(SC, ha->ioctl_data,
				   sizeof (ips_passthru_t) + sizeof (int));
		SC->result = DID_OK << 16;

		return (IPS_SUCCESS_IMM);

	case IPS_COPPUSRCMD:
	case IPS_COPPIOCCMD:
		if (SC->cmnd[0] == IPS_IOCTL_COMMAND) {
			if (length < (sizeof (ips_passthru_t) + pt->CmdBSize)) {
				/* wrong size */
				DEBUG_VAR(1,
					  "(%s%d) Passthru structure wrong size",
					  ips_name, ha->host_num);

				return (IPS_FAILURE);
			}

			if (ha->pcidev->device == IPS_DEVICEID_COPPERHEAD &&
			    pt->CoppCP.cmd.flashfw.op_code ==
			    IPS_CMD_RW_BIOSFW) {
				ret = ips_flash_copperhead(ha, pt, scb);
				ips_scmd_buf_write(SC, ha->ioctl_data,
						   sizeof (ips_passthru_t));
				return ret;
			}
			if (ips_usrcmd(ha, pt, scb))
				return (IPS_SUCCESS);
			else
				return (IPS_FAILURE);
		}

		break;

	}			/* end switch */

	return (IPS_FAILURE);
}

/****************************************************************************/
/* Routine Name: ips_flash_copperhead                                       */
/* Routine Description:                                                     */
/*   Flash the BIOS/FW on a Copperhead style controller                     */
/****************************************************************************/
static int
ips_flash_copperhead(ips_ha_t * ha, ips_passthru_t * pt, ips_scb_t * scb)
{
	int datasize;

	/* Trombone is the only copperhead that can do packet flash, but only
	 * for firmware. No one said it had to make sence. */
	if (IPS_IS_TROMBONE(ha) && pt->CoppCP.cmd.flashfw.type == IPS_FW_IMAGE) {
		if (ips_usrcmd(ha, pt, scb))
			return IPS_SUCCESS;
		else
			return IPS_FAILURE;
	}
	pt->BasicStatus = 0x0B;
	pt->ExtendedStatus = 0;
	scb->scsi_cmd->result = DID_OK << 16;
	/* IF it's OK to Use the "CD BOOT" Flash Buffer, then you can     */
	/* avoid allocating a huge buffer per adapter ( which can fail ). */
	if (pt->CoppCP.cmd.flashfw.type == IPS_BIOS_IMAGE &&
	    pt->CoppCP.cmd.flashfw.direction == IPS_ERASE_BIOS) {
		pt->BasicStatus = 0;
		return ips_flash_bios(ha, pt, scb);
	} else if (pt->CoppCP.cmd.flashfw.packet_num == 0) {
		if (ips_FlashData && !test_and_set_bit(0, &ips_FlashDataInUse)){
			ha->flash_data = ips_FlashData;
			ha->flash_busaddr = ips_flashbusaddr;
			ha->flash_len = PAGE_SIZE << 7;
			ha->flash_datasize = 0;
		} else if (!ha->flash_data) {
			datasize = pt->CoppCP.cmd.flashfw.total_packets *
			    pt->CoppCP.cmd.flashfw.count;
			ha->flash_data = pci_alloc_consistent(ha->pcidev,
					                      datasize,
							      &ha->flash_busaddr);
			if (!ha->flash_data){
				printk(KERN_WARNING "Unable to allocate a flash buffer\n");
				return IPS_FAILURE;
			}
			ha->flash_datasize = 0;
			ha->flash_len = datasize;
		} else
			return IPS_FAILURE;
	} else {
		if (pt->CoppCP.cmd.flashfw.count + ha->flash_datasize >
		    ha->flash_len) {
			ips_free_flash_copperhead(ha);
			IPS_PRINTK(KERN_WARNING, ha->pcidev,
				   "failed size sanity check\n");
			return IPS_FAILURE;
		}
	}
	if (!ha->flash_data)
		return IPS_FAILURE;
	pt->BasicStatus = 0;
	memcpy(&ha->flash_data[ha->flash_datasize], pt + 1,
	       pt->CoppCP.cmd.flashfw.count);
	ha->flash_datasize += pt->CoppCP.cmd.flashfw.count;
	if (pt->CoppCP.cmd.flashfw.packet_num ==
	    pt->CoppCP.cmd.flashfw.total_packets - 1) {
		if (pt->CoppCP.cmd.flashfw.type == IPS_BIOS_IMAGE)
			return ips_flash_bios(ha, pt, scb);
		else if (pt->CoppCP.cmd.flashfw.type == IPS_FW_IMAGE)
			return ips_flash_firmware(ha, pt, scb);
	}
	return IPS_SUCCESS_IMM;
}

/****************************************************************************/
/* Routine Name: ips_flash_bios                                             */
/* Routine Description:                                                     */
/*   flashes the bios of a copperhead adapter                               */
/****************************************************************************/
static int
ips_flash_bios(ips_ha_t * ha, ips_passthru_t * pt, ips_scb_t * scb)
{

	if (pt->CoppCP.cmd.flashfw.type == IPS_BIOS_IMAGE &&
	    pt->CoppCP.cmd.flashfw.direction == IPS_WRITE_BIOS) {
		if ((!ha->func.programbios) || (!ha->func.erasebios) ||
		    (!ha->func.verifybios))
			goto error;
		if ((*ha->func.erasebios) (ha)) {
			DEBUG_VAR(1,
				  "(%s%d) flash bios failed - unable to erase flash",
				  ips_name, ha->host_num);
			goto error;
		} else
		    if ((*ha->func.programbios) (ha,
						 ha->flash_data +
						 IPS_BIOS_HEADER,
						 ha->flash_datasize -
						 IPS_BIOS_HEADER, 0)) {
			DEBUG_VAR(1,
				  "(%s%d) flash bios failed - unable to flash",
				  ips_name, ha->host_num);
			goto error;
		} else
		    if ((*ha->func.verifybios) (ha,
						ha->flash_data +
						IPS_BIOS_HEADER,
						ha->flash_datasize -
						IPS_BIOS_HEADER, 0)) {
			DEBUG_VAR(1,
				  "(%s%d) flash bios failed - unable to verify flash",
				  ips_name, ha->host_num);
			goto error;
		}
		ips_free_flash_copperhead(ha);
		return IPS_SUCCESS_IMM;
	} else if (pt->CoppCP.cmd.flashfw.type == IPS_BIOS_IMAGE &&
		   pt->CoppCP.cmd.flashfw.direction == IPS_ERASE_BIOS) {
		if (!ha->func.erasebios)
			goto error;
		if ((*ha->func.erasebios) (ha)) {
			DEBUG_VAR(1,
				  "(%s%d) flash bios failed - unable to erase flash",
				  ips_name, ha->host_num);
			goto error;
		}
		return IPS_SUCCESS_IMM;
	}
      error:
	pt->BasicStatus = 0x0B;
	pt->ExtendedStatus = 0x00;
	ips_free_flash_copperhead(ha);
	return IPS_FAILURE;
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_fill_scb_sg_single                                     */
/*                                                                          */
/* Routine Description:                                                     */
/*   Fill in a single scb sg_list element from an address                   */
/*   return a -1 if a breakup occurred                                      */
/****************************************************************************/
static int
ips_fill_scb_sg_single(ips_ha_t * ha, dma_addr_t busaddr,
		       ips_scb_t * scb, int indx, unsigned int e_len)
{

	int ret_val = 0;

	if ((scb->data_len + e_len) > ha->max_xfer) {
		e_len = ha->max_xfer - scb->data_len;
		scb->breakup = indx;
		++scb->sg_break;
		ret_val = -1;
	} else {
		scb->breakup = 0;
		scb->sg_break = 0;
	}
	if (IPS_USE_ENH_SGLIST(ha)) {
		scb->sg_list.enh_list[indx].address_lo =
		    cpu_to_le32(pci_dma_lo32(busaddr));
		scb->sg_list.enh_list[indx].address_hi =
		    cpu_to_le32(pci_dma_hi32(busaddr));
		scb->sg_list.enh_list[indx].length = cpu_to_le32(e_len);
	} else {
		scb->sg_list.std_list[indx].address =
		    cpu_to_le32(pci_dma_lo32(busaddr));
		scb->sg_list.std_list[indx].length = cpu_to_le32(e_len);
	}

	++scb->sg_len;
	scb->data_len += e_len;
	return ret_val;
}

/****************************************************************************/
/* Routine Name: ips_flash_firmware                                         */
/* Routine Description:                                                     */
/*   flashes the firmware of a copperhead adapter                           */
/****************************************************************************/
static int
ips_flash_firmware(ips_ha_t * ha, ips_passthru_t * pt, ips_scb_t * scb)
{
	IPS_SG_LIST sg_list;
	uint32_t cmd_busaddr;

	if (pt->CoppCP.cmd.flashfw.type == IPS_FW_IMAGE &&
	    pt->CoppCP.cmd.flashfw.direction == IPS_WRITE_FW) {
		memset(&pt->CoppCP.cmd, 0, sizeof (IPS_HOST_COMMAND));
		pt->CoppCP.cmd.flashfw.op_code = IPS_CMD_DOWNLOAD;
		pt->CoppCP.cmd.flashfw.count = cpu_to_le32(ha->flash_datasize);
	} else {
		pt->BasicStatus = 0x0B;
		pt->ExtendedStatus = 0x00;
		ips_free_flash_copperhead(ha);
		return IPS_FAILURE;
	}
	/* Save the S/G list pointer so it doesn't get clobbered */
	sg_list.list = scb->sg_list.list;
	cmd_busaddr = scb->scb_busaddr;
	/* copy in the CP */
	memcpy(&scb->cmd, &pt->CoppCP.cmd, sizeof (IPS_IOCTL_CMD));
	/* FIX stuff that might be wrong */
	scb->sg_list.list = sg_list.list;
	scb->scb_busaddr = cmd_busaddr;
	scb->bus = scb->scsi_cmd->device->channel;
	scb->target_id = scb->scsi_cmd->device->id;
	scb->lun = scb->scsi_cmd->device->lun;
	scb->sg_len = 0;
	scb->data_len = 0;
	scb->flags = 0;
	scb->op_code = 0;
	scb->callback = ipsintr_done;
	scb->timeout = ips_cmd_timeout;

	scb->data_len = ha->flash_datasize;
	scb->data_busaddr =
	    pci_map_single(ha->pcidev, ha->flash_data, scb->data_len,
			   IPS_DMA_DIR(scb));
	scb->flags |= IPS_SCB_MAP_SINGLE;
	scb->cmd.flashfw.command_id = IPS_COMMAND_ID(ha, scb);
	scb->cmd.flashfw.buffer_addr = cpu_to_le32(scb->data_busaddr);
	if (pt->TimeOut)
		scb->timeout = pt->TimeOut;
	scb->scsi_cmd->result = DID_OK << 16;
	return IPS_SUCCESS;
}

/****************************************************************************/
/* Routine Name: ips_free_flash_copperhead                                  */
/* Routine Description:                                                     */
/*   release the memory resources used to hold the flash image              */
/****************************************************************************/
static void
ips_free_flash_copperhead(ips_ha_t * ha)
{
	if (ha->flash_data == ips_FlashData)
		test_and_clear_bit(0, &ips_FlashDataInUse);
	else if (ha->flash_data)
		pci_free_consistent(ha->pcidev, ha->flash_len, ha->flash_data,
				    ha->flash_busaddr);
	ha->flash_data = NULL;
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_usrcmd                                                 */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Process a user command and make it ready to send                       */
/*                                                                          */
/****************************************************************************/
static int
ips_usrcmd(ips_ha_t * ha, ips_passthru_t * pt, ips_scb_t * scb)
{
	IPS_SG_LIST sg_list;
	uint32_t cmd_busaddr;

	METHOD_TRACE("ips_usrcmd", 1);

	if ((!scb) || (!pt) || (!ha))
		return (0);

	/* Save the S/G list pointer so it doesn't get clobbered */
	sg_list.list = scb->sg_list.list;
	cmd_busaddr = scb->scb_busaddr;
	/* copy in the CP */
	memcpy(&scb->cmd, &pt->CoppCP.cmd, sizeof (IPS_IOCTL_CMD));
	memcpy(&scb->dcdb, &pt->CoppCP.dcdb, sizeof (IPS_DCDB_TABLE));

	/* FIX stuff that might be wrong */
	scb->sg_list.list = sg_list.list;
	scb->scb_busaddr = cmd_busaddr;
	scb->bus = scb->scsi_cmd->device->channel;
	scb->target_id = scb->scsi_cmd->device->id;
	scb->lun = scb->scsi_cmd->device->lun;
	scb->sg_len = 0;
	scb->data_len = 0;
	scb->flags = 0;
	scb->op_code = 0;
	scb->callback = ipsintr_done;
	scb->timeout = ips_cmd_timeout;
	scb->cmd.basic_io.command_id = IPS_COMMAND_ID(ha, scb);

	/* we don't support DCDB/READ/WRITE Scatter Gather */
	if ((scb->cmd.basic_io.op_code == IPS_CMD_READ_SG) ||
	    (scb->cmd.basic_io.op_code == IPS_CMD_WRITE_SG) ||
	    (scb->cmd.basic_io.op_code == IPS_CMD_DCDB_SG))
		return (0);

	if (pt->CmdBSize) {
		scb->data_len = pt->CmdBSize;
		scb->data_busaddr = ha->ioctl_busaddr + sizeof (ips_passthru_t);
	} else {
		scb->data_busaddr = 0L;
	}

	if (scb->cmd.dcdb.op_code == IPS_CMD_DCDB)
		scb->cmd.dcdb.dcdb_address = cpu_to_le32(scb->scb_busaddr +
							 (unsigned long) &scb->
							 dcdb -
							 (unsigned long) scb);

	if (pt->CmdBSize) {
		if (scb->cmd.dcdb.op_code == IPS_CMD_DCDB)
			scb->dcdb.buffer_pointer =
			    cpu_to_le32(scb->data_busaddr);
		else
			scb->cmd.basic_io.sg_addr =
			    cpu_to_le32(scb->data_busaddr);
	}

	/* set timeouts */
	if (pt->TimeOut) {
		scb->timeout = pt->TimeOut;

		if (pt->TimeOut <= 10)
			scb->dcdb.cmd_attribute |= IPS_TIMEOUT10;
		else if (pt->TimeOut <= 60)
			scb->dcdb.cmd_attribute |= IPS_TIMEOUT60;
		else
			scb->dcdb.cmd_attribute |= IPS_TIMEOUT20M;
	}

	/* assume success */
	scb->scsi_cmd->result = DID_OK << 16;

	/* success */
	return (1);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_cleanup_passthru                                       */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Cleanup after a passthru command                                       */
/*                                                                          */
/****************************************************************************/
static void
ips_cleanup_passthru(ips_ha_t * ha, ips_scb_t * scb)
{
	ips_passthru_t *pt;

	METHOD_TRACE("ips_cleanup_passthru", 1);

	if ((!scb) || (!scb->scsi_cmd) || (!scsi_sglist(scb->scsi_cmd))) {
		DEBUG_VAR(1, "(%s%d) couldn't cleanup after passthru",
			  ips_name, ha->host_num);

		return;
	}
	pt = (ips_passthru_t *) ha->ioctl_data;

	/* Copy data back to the user */
	if (scb->cmd.dcdb.op_code == IPS_CMD_DCDB)	/* Copy DCDB Back to Caller's Area */
		memcpy(&pt->CoppCP.dcdb, &scb->dcdb, sizeof (IPS_DCDB_TABLE));

	pt->BasicStatus = scb->basic_status;
	pt->ExtendedStatus = scb->extended_status;
	pt->AdapterType = ha->ad_type;

	if (ha->pcidev->device == IPS_DEVICEID_COPPERHEAD &&
	    (scb->cmd.flashfw.op_code == IPS_CMD_DOWNLOAD ||
	     scb->cmd.flashfw.op_code == IPS_CMD_RW_BIOSFW))
		ips_free_flash_copperhead(ha);

	ips_scmd_buf_write(scb->scsi_cmd, ha->ioctl_data, ha->ioctl_datasize);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_host_info                                              */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   The passthru interface for the driver                                  */
/*                                                                          */
/****************************************************************************/
static int
ips_host_info(ips_ha_t * ha, char *ptr, off_t offset, int len)
{
	IPS_INFOSTR info;

	METHOD_TRACE("ips_host_info", 1);

	info.buffer = ptr;
	info.length = len;
	info.offset = offset;
	info.pos = 0;
	info.localpos = 0;

	copy_info(&info, "\nIBM ServeRAID General Information:\n\n");

	if ((le32_to_cpu(ha->nvram->signature) == IPS_NVRAM_P5_SIG) &&
	    (le16_to_cpu(ha->nvram->adapter_type) != 0))
		copy_info(&info, "\tController Type                   : %s\n",
			  ips_adapter_name[ha->ad_type - 1]);
	else
		copy_info(&info,
			  "\tController Type                   : Unknown\n");

	if (ha->io_addr)
		copy_info(&info,
			  "\tIO region                         : 0x%lx (%d bytes)\n",
			  ha->io_addr, ha->io_len);

	if (ha->mem_addr) {
		copy_info(&info,
			  "\tMemory region                     : 0x%lx (%d bytes)\n",
			  ha->mem_addr, ha->mem_len);
		copy_info(&info,
			  "\tShared memory address             : 0x%lx\n",
			  ha->mem_ptr);
	}

	copy_info(&info, "\tIRQ number                        : %d\n", ha->pcidev->irq);

    /* For the Next 3 lines Check for Binary 0 at the end and don't include it if it's there. */
    /* That keeps everything happy for "text" operations on the proc file.                    */

	if (le32_to_cpu(ha->nvram->signature) == IPS_NVRAM_P5_SIG) {
	if (ha->nvram->bios_low[3] == 0) {
            copy_info(&info,
			          "\tBIOS Version                      : %c%c%c%c%c%c%c\n",
			          ha->nvram->bios_high[0], ha->nvram->bios_high[1],
			          ha->nvram->bios_high[2], ha->nvram->bios_high[3],
			          ha->nvram->bios_low[0], ha->nvram->bios_low[1],
			          ha->nvram->bios_low[2]);

        } else {
		    copy_info(&info,
			          "\tBIOS Version                      : %c%c%c%c%c%c%c%c\n",
			          ha->nvram->bios_high[0], ha->nvram->bios_high[1],
			          ha->nvram->bios_high[2], ha->nvram->bios_high[3],
			          ha->nvram->bios_low[0], ha->nvram->bios_low[1],
			          ha->nvram->bios_low[2], ha->nvram->bios_low[3]);
        }

    }

    if (ha->enq->CodeBlkVersion[7] == 0) {
        copy_info(&info,
		          "\tFirmware Version                  : %c%c%c%c%c%c%c\n",
		          ha->enq->CodeBlkVersion[0], ha->enq->CodeBlkVersion[1],
		          ha->enq->CodeBlkVersion[2], ha->enq->CodeBlkVersion[3],
		          ha->enq->CodeBlkVersion[4], ha->enq->CodeBlkVersion[5],
		          ha->enq->CodeBlkVersion[6]);
    } else {
        copy_info(&info,
		          "\tFirmware Version                  : %c%c%c%c%c%c%c%c\n",
		          ha->enq->CodeBlkVersion[0], ha->enq->CodeBlkVersion[1],
		          ha->enq->CodeBlkVersion[2], ha->enq->CodeBlkVersion[3],
		          ha->enq->CodeBlkVersion[4], ha->enq->CodeBlkVersion[5],
		          ha->enq->CodeBlkVersion[6], ha->enq->CodeBlkVersion[7]);
    }

    if (ha->enq->BootBlkVersion[7] == 0) {
        copy_info(&info,
		          "\tBoot Block Version                : %c%c%c%c%c%c%c\n",
		          ha->enq->BootBlkVersion[0], ha->enq->BootBlkVersion[1],
		          ha->enq->BootBlkVersion[2], ha->enq->BootBlkVersion[3],
		          ha->enq->BootBlkVersion[4], ha->enq->BootBlkVersion[5],
		          ha->enq->BootBlkVersion[6]);
    } else {
        copy_info(&info,
		          "\tBoot Block Version                : %c%c%c%c%c%c%c%c\n",
		          ha->enq->BootBlkVersion[0], ha->enq->BootBlkVersion[1],
		          ha->enq->BootBlkVersion[2], ha->enq->BootBlkVersion[3],
		          ha->enq->BootBlkVersion[4], ha->enq->BootBlkVersion[5],
		          ha->enq->BootBlkVersion[6], ha->enq->BootBlkVersion[7]);
    }

	copy_info(&info, "\tDriver Version                    : %s%s\n",
		  IPS_VERSION_HIGH, IPS_VERSION_LOW);

	copy_info(&info, "\tDriver Build                      : %d\n",
		  IPS_BUILD_IDENT);

	copy_info(&info, "\tMax Physical Devices              : %d\n",
		  ha->enq->ucMaxPhysicalDevices);
	copy_info(&info, "\tMax Active Commands               : %d\n",
		  ha->max_cmds);
	copy_info(&info, "\tCurrent Queued Commands           : %d\n",
		  ha->scb_waitlist.count);
	copy_info(&info, "\tCurrent Active Commands           : %d\n",
		  ha->scb_activelist.count - ha->num_ioctl);
	copy_info(&info, "\tCurrent Queued PT Commands        : %d\n",
		  ha->copp_waitlist.count);
	copy_info(&info, "\tCurrent Active PT Commands        : %d\n",
		  ha->num_ioctl);

	copy_info(&info, "\n");

	return (info.localpos);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: copy_mem_info                                              */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Copy data into an IPS_INFOSTR structure                                */
/*                                                                          */
/****************************************************************************/
static void
copy_mem_info(IPS_INFOSTR * info, char *data, int len)
{
	METHOD_TRACE("copy_mem_info", 1);

	if (info->pos + len < info->offset) {
		info->pos += len;
		return;
	}

	if (info->pos < info->offset) {
		data += (info->offset - info->pos);
		len -= (info->offset - info->pos);
		info->pos += (info->offset - info->pos);
	}

	if (info->localpos + len > info->length)
		len = info->length - info->localpos;

	if (len > 0) {
		memcpy(info->buffer + info->localpos, data, len);
		info->pos += len;
		info->localpos += len;
	}
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: copy_info                                                  */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   printf style wrapper for an info structure                             */
/*                                                                          */
/****************************************************************************/
static int
copy_info(IPS_INFOSTR * info, char *fmt, ...)
{
	va_list args;
	char buf[128];
	int len;

	METHOD_TRACE("copy_info", 1);

	va_start(args, fmt);
	len = vsprintf(buf, fmt, args);
	va_end(args);

	copy_mem_info(info, buf, len);

	return (len);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_identify_controller                                    */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Identify this controller                                               */
/*                                                                          */
/****************************************************************************/
static void
ips_identify_controller(ips_ha_t * ha)
{
	METHOD_TRACE("ips_identify_controller", 1);

	switch (ha->pcidev->device) {
	case IPS_DEVICEID_COPPERHEAD:
		if (ha->pcidev->revision <= IPS_REVID_SERVERAID) {
			ha->ad_type = IPS_ADTYPE_SERVERAID;
		} else if (ha->pcidev->revision == IPS_REVID_SERVERAID2) {
			ha->ad_type = IPS_ADTYPE_SERVERAID2;
		} else if (ha->pcidev->revision == IPS_REVID_NAVAJO) {
			ha->ad_type = IPS_ADTYPE_NAVAJO;
		} else if ((ha->pcidev->revision == IPS_REVID_SERVERAID2)
			   && (ha->slot_num == 0)) {
			ha->ad_type = IPS_ADTYPE_KIOWA;
		} else if ((ha->pcidev->revision >= IPS_REVID_CLARINETP1) &&
			   (ha->pcidev->revision <= IPS_REVID_CLARINETP3)) {
			if (ha->enq->ucMaxPhysicalDevices == 15)
				ha->ad_type = IPS_ADTYPE_SERVERAID3L;
			else
				ha->ad_type = IPS_ADTYPE_SERVERAID3;
		} else if ((ha->pcidev->revision >= IPS_REVID_TROMBONE32) &&
			   (ha->pcidev->revision <= IPS_REVID_TROMBONE64)) {
			ha->ad_type = IPS_ADTYPE_SERVERAID4H;
		}
		break;

	case IPS_DEVICEID_MORPHEUS:
		switch (ha->pcidev->subsystem_device) {
		case IPS_SUBDEVICEID_4L:
			ha->ad_type = IPS_ADTYPE_SERVERAID4L;
			break;

		case IPS_SUBDEVICEID_4M:
			ha->ad_type = IPS_ADTYPE_SERVERAID4M;
			break;

		case IPS_SUBDEVICEID_4MX:
			ha->ad_type = IPS_ADTYPE_SERVERAID4MX;
			break;

		case IPS_SUBDEVICEID_4LX:
			ha->ad_type = IPS_ADTYPE_SERVERAID4LX;
			break;

		case IPS_SUBDEVICEID_5I2:
			ha->ad_type = IPS_ADTYPE_SERVERAID5I2;
			break;

		case IPS_SUBDEVICEID_5I1:
			ha->ad_type = IPS_ADTYPE_SERVERAID5I1;
			break;
		}

		break;

	case IPS_DEVICEID_MARCO:
		switch (ha->pcidev->subsystem_device) {
		case IPS_SUBDEVICEID_6M:
			ha->ad_type = IPS_ADTYPE_SERVERAID6M;
			break;
		case IPS_SUBDEVICEID_6I:
			ha->ad_type = IPS_ADTYPE_SERVERAID6I;
			break;
		case IPS_SUBDEVICEID_7k:
			ha->ad_type = IPS_ADTYPE_SERVERAID7k;
			break;
		case IPS_SUBDEVICEID_7M:
			ha->ad_type = IPS_ADTYPE_SERVERAID7M;
			break;
		}
		break;
	}
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_get_bios_version                                       */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Get the BIOS revision number                                           */
/*                                                                          */
/****************************************************************************/
static void
ips_get_bios_version(ips_ha_t * ha, int intr)
{
	ips_scb_t *scb;
	int ret;
	uint8_t major;
	uint8_t minor;
	uint8_t subminor;
	uint8_t *buffer;
	char hexDigits[] =
	    { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C',
     'D', 'E', 'F' };

	METHOD_TRACE("ips_get_bios_version", 1);

	major = 0;
	minor = 0;

	strncpy(ha->bios_version, "       ?", 8);

	if (ha->pcidev->device == IPS_DEVICEID_COPPERHEAD) {
		if (IPS_USE_MEMIO(ha)) {
			/* Memory Mapped I/O */

			/* test 1st byte */
			writel(0, ha->mem_ptr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */

			if (readb(ha->mem_ptr + IPS_REG_FLDP) != 0x55)
				return;

			writel(1, ha->mem_ptr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */

			if (readb(ha->mem_ptr + IPS_REG_FLDP) != 0xAA)
				return;

			/* Get Major version */
			writel(0x1FF, ha->mem_ptr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */

			major = readb(ha->mem_ptr + IPS_REG_FLDP);

			/* Get Minor version */
			writel(0x1FE, ha->mem_ptr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */
			minor = readb(ha->mem_ptr + IPS_REG_FLDP);

			/* Get SubMinor version */
			writel(0x1FD, ha->mem_ptr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */
			subminor = readb(ha->mem_ptr + IPS_REG_FLDP);

		} else {
			/* Programmed I/O */

			/* test 1st byte */
			outl(0, ha->io_addr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */

			if (inb(ha->io_addr + IPS_REG_FLDP) != 0x55)
				return;

			outl(1, ha->io_addr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */

			if (inb(ha->io_addr + IPS_REG_FLDP) != 0xAA)
				return;

			/* Get Major version */
			outl(0x1FF, ha->io_addr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */

			major = inb(ha->io_addr + IPS_REG_FLDP);

			/* Get Minor version */
			outl(0x1FE, ha->io_addr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */

			minor = inb(ha->io_addr + IPS_REG_FLDP);

			/* Get SubMinor version */
			outl(0x1FD, ha->io_addr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */

			subminor = inb(ha->io_addr + IPS_REG_FLDP);

		}
	} else {
		/* Morpheus Family - Send Command to the card */

		buffer = ha->ioctl_data;

		memset(buffer, 0, 0x1000);

		scb = &ha->scbs[ha->max_cmds - 1];

		ips_init_scb(ha, scb);

		scb->timeout = ips_cmd_timeout;
		scb->cdb[0] = IPS_CMD_RW_BIOSFW;

		scb->cmd.flashfw.op_code = IPS_CMD_RW_BIOSFW;
		scb->cmd.flashfw.command_id = IPS_COMMAND_ID(ha, scb);
		scb->cmd.flashfw.type = 1;
		scb->cmd.flashfw.direction = 0;
		scb->cmd.flashfw.count = cpu_to_le32(0x800);
		scb->cmd.flashfw.total_packets = 1;
		scb->cmd.flashfw.packet_num = 0;
		scb->data_len = 0x1000;
		scb->cmd.flashfw.buffer_addr = ha->ioctl_busaddr;

		/* issue the command */
		if (((ret =
		      ips_send_wait(ha, scb, ips_cmd_timeout,
				    intr)) == IPS_FAILURE)
		    || (ret == IPS_SUCCESS_IMM)
		    || ((scb->basic_status & IPS_GSC_STATUS_MASK) > 1)) {
			/* Error occurred */

			return;
		}

		if ((buffer[0xC0] == 0x55) && (buffer[0xC1] == 0xAA)) {
			major = buffer[0x1ff + 0xC0];	/* Offset 0x1ff after the header (0xc0) */
			minor = buffer[0x1fe + 0xC0];	/* Offset 0x1fe after the header (0xc0) */
			subminor = buffer[0x1fd + 0xC0];	/* Offset 0x1fd after the header (0xc0) */
		} else {
			return;
		}
	}

	ha->bios_version[0] = hexDigits[(major & 0xF0) >> 4];
	ha->bios_version[1] = '.';
	ha->bios_version[2] = hexDigits[major & 0x0F];
	ha->bios_version[3] = hexDigits[subminor];
	ha->bios_version[4] = '.';
	ha->bios_version[5] = hexDigits[(minor & 0xF0) >> 4];
	ha->bios_version[6] = hexDigits[minor & 0x0F];
	ha->bios_version[7] = 0;
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_hainit                                                 */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Initialize the controller                                              */
/*                                                                          */
/* NOTE: Assumes to be called from with a lock                              */
/*                                                                          */
/****************************************************************************/
static int
ips_hainit(ips_ha_t * ha)
{
	int i;
	struct timeval tv;

	METHOD_TRACE("ips_hainit", 1);

	if (!ha)
		return (0);

	if (ha->func.statinit)
		(*ha->func.statinit) (ha);

	if (ha->func.enableint)
		(*ha->func.enableint) (ha);

	/* Send FFDC */
	ha->reset_count = 1;
	do_gettimeofday(&tv);
	ha->last_ffdc = tv.tv_sec;
	ips_ffdc_reset(ha, IPS_INTR_IORL);

	if (!ips_read_config(ha, IPS_INTR_IORL)) {
		IPS_PRINTK(KERN_WARNING, ha->pcidev,
			   "unable to read config from controller.\n");

		return (0);
	}
	/* end if */
	if (!ips_read_adapter_status(ha, IPS_INTR_IORL)) {
		IPS_PRINTK(KERN_WARNING, ha->pcidev,
			   "unable to read controller status.\n");

		return (0);
	}

	/* Identify this controller */
	ips_identify_controller(ha);

	if (!ips_read_subsystem_parameters(ha, IPS_INTR_IORL)) {
		IPS_PRINTK(KERN_WARNING, ha->pcidev,
			   "unable to read subsystem parameters.\n");

		return (0);
	}

	/* write nvram user page 5 */
	if (!ips_write_driver_status(ha, IPS_INTR_IORL)) {
		IPS_PRINTK(KERN_WARNING, ha->pcidev,
			   "unable to write driver info to controller.\n");

		return (0);
	}

	/* If there are Logical Drives and a Reset Occurred, then an EraseStripeLock is Needed */
	if ((ha->conf->ucLogDriveCount > 0) && (ha->requires_esl == 1))
		ips_clear_adapter(ha, IPS_INTR_IORL);

	/* set limits on SID, LUN, BUS */
	ha->ntargets = IPS_MAX_TARGETS + 1;
	ha->nlun = 1;
	ha->nbus = (ha->enq->ucMaxPhysicalDevices / IPS_MAX_TARGETS) + 1;

	switch (ha->conf->logical_drive[0].ucStripeSize) {
	case 4:
		ha->max_xfer = 0x10000;
		break;

	case 5:
		ha->max_xfer = 0x20000;
		break;

	case 6:
		ha->max_xfer = 0x40000;
		break;

	case 7:
	default:
		ha->max_xfer = 0x80000;
		break;
	}

	/* setup max concurrent commands */
	if (le32_to_cpu(ha->subsys->param[4]) & 0x1) {
		/* Use the new method */
		ha->max_cmds = ha->enq->ucConcurrentCmdCount;
	} else {
		/* use the old method */
		switch (ha->conf->logical_drive[0].ucStripeSize) {
		case 4:
			ha->max_cmds = 32;
			break;

		case 5:
			ha->max_cmds = 16;
			break;

		case 6:
			ha->max_cmds = 8;
			break;

		case 7:
		default:
			ha->max_cmds = 4;
			break;
		}
	}

	/* Limit the Active Commands on a Lite Adapter */
	if ((ha->ad_type == IPS_ADTYPE_SERVERAID3L) ||
	    (ha->ad_type == IPS_ADTYPE_SERVERAID4L) ||
	    (ha->ad_type == IPS_ADTYPE_SERVERAID4LX)) {
		if ((ha->max_cmds > MaxLiteCmds) && (MaxLiteCmds))
			ha->max_cmds = MaxLiteCmds;
	}

	/* set controller IDs */
	ha->ha_id[0] = IPS_ADAPTER_ID;
	for (i = 1; i < ha->nbus; i++) {
		ha->ha_id[i] = ha->conf->init_id[i - 1] & 0x1f;
		ha->dcdb_active[i - 1] = 0;
	}

	return (1);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_next                                                   */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Take the next command off the queue and send it to the controller      */
/*                                                                          */
/****************************************************************************/
static void
ips_next(ips_ha_t * ha, int intr)
{
	ips_scb_t *scb;
	struct scsi_cmnd *SC;
	struct scsi_cmnd *p;
	struct scsi_cmnd *q;
	ips_copp_wait_item_t *item;
	int ret;
	struct Scsi_Host *host;
	METHOD_TRACE("ips_next", 1);

	if (!ha)
		return;
	host = ips_sh[ha->host_num];
	/*
	 * Block access to the queue function so
	 * this command won't time out
	 */
	if (intr == IPS_INTR_ON)
		spin_lock(host->host_lock);

	if ((ha->subsys->param[3] & 0x300000)
	    && (ha->scb_activelist.count == 0)) {
		struct timeval tv;

		do_gettimeofday(&tv);

		if (tv.tv_sec - ha->last_ffdc > IPS_SECS_8HOURS) {
			ha->last_ffdc = tv.tv_sec;
			ips_ffdc_time(ha);
		}
	}

	/*
	 * Send passthru commands
	 * These have priority over normal I/O
	 * but shouldn't affect performance too much
	 * since we limit the number that can be active
	 * on the card at any one time
	 */
	while ((ha->num_ioctl < IPS_MAX_IOCTL) &&
	       (ha->copp_waitlist.head) && (scb = ips_getscb(ha))) {

		item = ips_removeq_copp_head(&ha->copp_waitlist);
		ha->num_ioctl++;
		if (intr == IPS_INTR_ON)
			spin_unlock(host->host_lock);
		scb->scsi_cmd = item->scsi_cmd;
		kfree(item);

		ret = ips_make_passthru(ha, scb->scsi_cmd, scb, intr);

		if (intr == IPS_INTR_ON)
			spin_lock(host->host_lock);
		switch (ret) {
		case IPS_FAILURE:
			if (scb->scsi_cmd) {
				scb->scsi_cmd->result = DID_ERROR << 16;
				scb->scsi_cmd->scsi_done(scb->scsi_cmd);
			}

			ips_freescb(ha, scb);
			break;
		case IPS_SUCCESS_IMM:
			if (scb->scsi_cmd) {
				scb->scsi_cmd->result = DID_OK << 16;
				scb->scsi_cmd->scsi_done(scb->scsi_cmd);
			}

			ips_freescb(ha, scb);
			break;
		default:
			break;
		}		/* end case */

		if (ret != IPS_SUCCESS) {
			ha->num_ioctl--;
			continue;
		}

		ret = ips_send_cmd(ha, scb);

		if (ret == IPS_SUCCESS)
			ips_putq_scb_head(&ha->scb_activelist, scb);
		else
			ha->num_ioctl--;

		switch (ret) {
		case IPS_FAILURE:
			if (scb->scsi_cmd) {
				scb->scsi_cmd->result = DID_ERROR << 16;
			}

			ips_freescb(ha, scb);
			break;
		case IPS_SUCCESS_IMM:
			ips_freescb(ha, scb);
			break;
		default:
			break;
		}		/* end case */

	}

	/*
	 * Send "Normal" I/O commands
	 */

	p = ha->scb_waitlist.head;
	while ((p) && (scb = ips_getscb(ha))) {
		if ((scmd_channel(p) > 0)
		    && (ha->
			dcdb_active[scmd_channel(p) -
				    1] & (1 << scmd_id(p)))) {
			ips_freescb(ha, scb);
			p = (struct scsi_cmnd *) p->host_scribble;
			continue;
		}

		q = p;
		SC = ips_removeq_wait(&ha->scb_waitlist, q);

		if (intr == IPS_INTR_ON)
			spin_unlock(host->host_lock);	/* Unlock HA after command is taken off queue */

		SC->result = DID_OK;
		SC->host_scribble = NULL;

		scb->target_id = SC->device->id;
		scb->lun = SC->device->lun;
		scb->bus = SC->device->channel;
		scb->scsi_cmd = SC;
		scb->breakup = 0;
		scb->data_len = 0;
		scb->callback = ipsintr_done;
		scb->timeout = ips_cmd_timeout;
		memset(&scb->cmd, 0, 16);

		/* copy in the CDB */
		memcpy(scb->cdb, SC->cmnd, SC->cmd_len);

                scb->sg_count = scsi_dma_map(SC);
                BUG_ON(scb->sg_count < 0);
		if (scb->sg_count) {
			struct scatterlist *sg;
			int i;

			scb->flags |= IPS_SCB_MAP_SG;

                        scsi_for_each_sg(SC, sg, scb->sg_count, i) {
				if (ips_fill_scb_sg_single
				    (ha, sg_dma_address(sg), scb, i,
				     sg_dma_len(sg)) < 0)
					break;
			}
			scb->dcdb.transfer_length = scb->data_len;
		} else {
                        scb->data_busaddr = 0L;
                        scb->sg_len = 0;
                        scb->data_len = 0;
                        scb->dcdb.transfer_length = 0;
		}

		scb->dcdb.cmd_attribute =
		    ips_command_direction[scb->scsi_cmd->cmnd[0]];

		/* Allow a WRITE BUFFER Command to Have no Data */
		/* This is Used by Tape Flash Utilites          */
		if ((scb->scsi_cmd->cmnd[0] == WRITE_BUFFER) &&
				(scb->data_len == 0))
			scb->dcdb.cmd_attribute = 0;

		if (!(scb->dcdb.cmd_attribute & 0x3))
			scb->dcdb.transfer_length = 0;

		if (scb->data_len >= IPS_MAX_XFER) {
			scb->dcdb.cmd_attribute |= IPS_TRANSFER64K;
			scb->dcdb.transfer_length = 0;
		}
		if (intr == IPS_INTR_ON)
			spin_lock(host->host_lock);

		ret = ips_send_cmd(ha, scb);

		switch (ret) {
		case IPS_SUCCESS:
			ips_putq_scb_head(&ha->scb_activelist, scb);
			break;
		case IPS_FAILURE:
			if (scb->scsi_cmd) {
				scb->scsi_cmd->result = DID_ERROR << 16;
				scb->scsi_cmd->scsi_done(scb->scsi_cmd);
			}

			if (scb->bus)
				ha->dcdb_active[scb->bus - 1] &=
				    ~(1 << scb->target_id);

			ips_freescb(ha, scb);
			break;
		case IPS_SUCCESS_IMM:
			if (scb->scsi_cmd)
				scb->scsi_cmd->scsi_done(scb->scsi_cmd);

			if (scb->bus)
				ha->dcdb_active[scb->bus - 1] &=
				    ~(1 << scb->target_id);

			ips_freescb(ha, scb);
			break;
		default:
			break;
		}		/* end case */

		p = (struct scsi_cmnd *) p->host_scribble;

	}			/* end while */

	if (intr == IPS_INTR_ON)
		spin_unlock(host->host_lock);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_putq_scb_head                                          */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Add an item to the head of the queue                                   */
/*                                                                          */
/* ASSUMED to be called from within the HA lock                             */
/*                                                                          */
/****************************************************************************/
static void
ips_putq_scb_head(ips_scb_queue_t * queue, ips_scb_t * item)
{
	METHOD_TRACE("ips_putq_scb_head", 1);

	if (!item)
		return;

	item->q_next = queue->head;
	queue->head = item;

	if (!queue->tail)
		queue->tail = item;

	queue->count++;
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_removeq_scb_head                                       */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Remove the head of the queue                                           */
/*                                                                          */
/* ASSUMED to be called from within the HA lock                             */
/*                                                                          */
/****************************************************************************/
static ips_scb_t *
ips_removeq_scb_head(ips_scb_queue_t * queue)
{
	ips_scb_t *item;

	METHOD_TRACE("ips_removeq_scb_head", 1);

	item = queue->head;

	if (!item) {
		return (NULL);
	}

	queue->head = item->q_next;
	item->q_next = NULL;

	if (queue->tail == item)
		queue->tail = NULL;

	queue->count--;

	return (item);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_removeq_scb                                            */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Remove an item from a queue                                            */
/*                                                                          */
/* ASSUMED to be called from within the HA lock                             */
/*                                                                          */
/****************************************************************************/
static ips_scb_t *
ips_removeq_scb(ips_scb_queue_t * queue, ips_scb_t * item)
{
	ips_scb_t *p;

	METHOD_TRACE("ips_removeq_scb", 1);

	if (!item)
		return (NULL);

	if (item == queue->head) {
		return (ips_removeq_scb_head(queue));
	}

	p = queue->head;

	while ((p) && (item != p->q_next))
		p = p->q_next;

	if (p) {
		/* found a match */
		p->q_next = item->q_next;

		if (!item->q_next)
			queue->tail = p;

		item->q_next = NULL;
		queue->count--;

		return (item);
	}

	return (NULL);
}

/****************************************************************************/
/*                     /*****************************************************/
/* Routine Name: ips_putq_wait_tail***********************************************ler                */
/*                                                 **************Description:
/*                                                        er                */
/*                                                          Add an item to the  -- dof     queueeith Mitchell, IBM Corporation                               */
/*             Jack Hammer, Adaptec, Inc.               ASSUMED    be called from within     HA lockitchell, IBM Corporation                               */
/*             Jack Hammer, Adaptec, Inc.                                         */
/*                                           /
static void*****/
/* ips.c -- (****ips.c     _t *     , struct scsi_cmnd *    )
{
	METHOD_TRACE("****/
/* ips.c -- ", 1);

	if (!redist		returnder ttem->host_    bble = NULLder the      -> --  of by      */
l Public License a(char *)     der by      */
 =ersion 2 the tby     head/* the Freeat ynse, or    by     count++;
}
                             */
/*                                              oller                */
/*                                                                **********remove* ips.c any er for the Adaptec / IBM ServeRAID controller                */
/*                                                                           */
/* Written By: Keith Mitchell, IBM Corporation                               */
/*             Jack Hammer, Adaptec, Inc.                 R    *      any              David Jeffery, Adaptec, Inc.      c.                                  */
/*                                                                           */
/* Copyright (C) 2000 IBM Corporation                                        */
/* Copyright (C) 2002,2003 Adaptec, Inc.                                     */
/*                                                     software; you can r        */
/* but WI/
/* This program is frestribsoftware; you can redider ute it and/or modi     */
/* but WIit under t    =      n) anyder the terms  {of the GN (s pu);
	}ersion.    any la(software; you can versiol Public Licens;General Public License as published by      */
 =se, or/* the Free Sofe as publision.        --der h its   ram e                             */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* ben By: Keith Mitchell, IBM Corporation                               */
/*             Jack Hammer, Adaptec, Inc.               SE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                                   */
a   */
/* NO WARRANTY                                                                */
/* THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR        */
/* CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT      */
/* LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,      */
/* MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is    */
/* /
/* This program is free
	      software; you can redistribsoftware; you can pf using and       */
/* distributiit under the terms of the GN       */       and aassumes all red with its   ent is    */
/* solelyetermi */
/* ep assumes all riskswhile ((p) &&ite 330!ights under this Agreempl Public Licens) of         */
/* Bugs/Comments/Suggestions ace, Suitped wit/* found a match    		ts/Suggestions anse, ored to:          */
 the termss/Suggestions ab    rs, damage to  the eneral Public License as publ         ,         **/
/* programs o
/* e59 Temple Place                           */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      /
/* coppc -- driver for the Adaptec / IBM ServeRAID controller                */
/*                                                                           */
/* Written By: Keith Mitchell, IBM Corporation                               */
/*             Jack Hammer, Adaptec, Inc.                                    */
/*             David Jeffery, Adaptec, Inc.                                  */
/*                                                                           */
/* Copyright (C) 2000 IBM Corporation                                        */
/* Copyright (C) 2002,2003 Adaptec, Inc.                                     */
/*                                                         
       */
/*      SA */
/*  program i      ,*****
/*  ips.c and     redistribute it and/or modify   
/*      it under the terms of the GNU General nexte as published by      */
/* the Free Softwthe quersion 2 of the License, or         */
/* (at your option) any later version.                                       */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */

/*  t WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* NO WARRANTY                                                               */
/* THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR        */
/* CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT      */
/* LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,      */
/* MERCHANTABILITY OR FITNESS FOR A PARTICULAR PU                       if w                     */
/*            SCBsstrib                      ss of using and       */
/* distri         e Program and assumes all risks associated with its        */
/* exercise of rigds on the not limitthe queue at once rather than program errors, damage to or loss of data,         */
/* programs or equipment, and unavailability or interruption of operations.  */
/*                                                                           */
/* DISCLAIMER OF LIABILITY                 
/*                               */
/* NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY   */
/* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL        */
/* DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND   */
/* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR     */
/* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE    */
/* USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED  */
/* HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES             */
/*                                                                           */
/* You shoulrt for ServeRAID 4                         */
/*            SCBs                                                         the Free Software             
/* 
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */
/*                                                                       plternabout thise reboo */
/*      ipslinux@adaptec.com             one at to flush /
/*        eboot                              the queue at  */
/* For system support issues, contact your local IBM Customer support.       */
/* Directions to find IBM Customer Support for each country can be found at: */
/*      http://www.ibm.com/planetwide/                                   intr_bC) 2ing                              */
/* NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY   */
/* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL        */
/* DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND   */
/* ON ANY THEORFinalizLIABILnterrupt for
/*   nal commandsMake interrupt routine handle all completed request on the     */
/*            adapter not just the first one                                 */
/*          - Make sure passthru commands get woken up if      - Use SlSA */ha     ha      scb     scbstribute it and/or mod     - Use Sl", 2ace, Sps_freescb( allcatinot f ((ha->ips.flag0, BTRUE       2.4cmd_in_progress0, Bscb->cdb[0])ed wit 2.4.x kernel  FALSEm support icontaJCRM CD    */
/*          - Add IPSSEND Flash Support                                      */
/*          - Set Sense Data for Unknown SCSI Command                        */
/*          don CONTRACT, STRICT LIABILITY, OR     */
/* TOR                 */
/*          - Restore caller's DCDB Structure                                */
/* 4.70.12  - Corrective actions for bad controller ( during initialization )*/
/* 4.70.13  - Don't Send CDB's if we already know the device is not present  */
/*          - non-Don't release HA Lock in ips_next() untiltaken off queue   */
/*          - Unregister SCSI device in ips_release()                        */
/* 4.70.15  - Fix Breakup for very large ( non-SG ) requests in ips_done()   */
/*  00  - Change all memory allocations to not use GFP_DMA fla/*     */
/*  he tcatid witIPS_PRINTK(KERN_WARNING,  2.4pcidev        "Spurious
/*       ;
/* as pu.\n"ace, lement to exdapter/* 4.e; you d0, B       ipslinunexpected
/*             */
/* 4.80.26  - Clean up potential code problems ( Arjan's recommatchingnot setons )   */
/* 4.90.01  -pskernelode Clean-                           */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      /*          - Close Window for getting too many IOC IOCTL's active               */
/* 4.80.00  - Make ia64 Safe                                                 */
/* 4.80.04  - Eliminate calls to strtok() if 2.4.x or greater                */
/*          - Adjustments to Device Queue Depth         Do housekeep Sloonleaspled Drease HA Lock in ips_next() until SC ed compile op                      or/
/* Copyrighrequest(C) 2000 IBM Corporation    one                                 */
/*          - Make sure passthru commands get woken up if wernel )  */
/*          - 5 second delayint relush ute it and/or modig an i9under the tcatiof the GNU Gen forrsion Matchin       ps_is_passthru     */
/* 6.10 - Allo     leanupve 1G Addrode Clean-Uow fonum_ioctl    	} else  ipsli
		 * Check    see i    is         had too muchfer ofdata   DDTS 604    broke up.  If so,      fer of      st        1.xx  - Mcontinue.fer o             *breakup) ||Version g_o fla - AlTHOUT ANY WARRANTY; withsoftware;atterlist *sg;4 kernels           */
/*.00.i, sg_dma_indexll memogrnels  = 0   */slinweDTS 6aS 6063o flag  - Add	/* 4.1.xx_len        kernels           */
/* tter Plugsgx pa     */
/* 6.10                */
/*      /* Spinerneware Loglast dma chunk - A             */
/*        6 kernels   - Rm_io flag ame for scsi_hosts.h inclu  - (i      i <               i++)IC functions whereever po/*          - g_the (sg)   */slinTak    re    possi    partialuppo12.xxUse STATIC functions whereever po     illmory sg_singl Dela                       */
/* 7.12ns whereever possible add    e Ve******************************/

/*
 * Conditionalcb        */
/* ++******************************/

/*
 * Conditional Compilene Ve functions                  up de;ossible       MODUemove_     nused "inline" fuTHOUT ANY WARRANTY; without essible      ++r 2.5  - Remove Ve 2.4 kernels           */
/**********uite*********************ompile directive is used.
 *  od trac*********************************/

/*
 * l Compilation directives for this driver:
 *
 * IPS_DEBUG  - Turn on debugging info
 *
 * Parameters:
 *
 * debug:<    - Set debug < 0                        */
/* 7.12ize     o flacompile directive is used.01          cdb.transferSet gth                /
/*ffer
 */

#    attribute |=code pr       se HA_direc    [rsion Matchin    nd[0]]rsion pter      *r.h>
#include <asm/& 0x3bout uffer
 */

#include <asm/io.h>      -- Version         >= */
/MAX_XFERrmWareyteorder.h>
#include <asm/pah>
#iTRANSFER64Km/byternel.h>
#include <linux/ioport.		.01  */
/*
/* memoendchin        */
/bytewi.com(retlinux/dcaseh>
#iFAILURE:ux/de- Version Matchinlinux/de      clude <linresule <lDID_ERROR << 16nux/prpping.h>

#inclu: onlernelused "inline" fusi.h
#incl            Code Clean-U			      -h>
#include SUCCESS_IMMtypes.h>
#include <linux/dma-mapping.h>

#include <scsi/sg.h>
#include "scsi.h"
#include <scsi/scsi_host.h>

#include "ips.h"

#include <linux/module.h>

#include <lidefaulttypes.nclude <li}sline */
incl*/#includent to 	}
	}on Mat    if e 1G Add_VER_M- Versionbus- Allow fo*/

_activeh>
#inbus - 1] &= ~(1clud     target_ide "i01  "
#include <scsi/scsi_host.h>

#include "*            Code Clean-                           */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      map_    u     */
/* 5.10.12  - use pci_dma i
/* NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY   */
/* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL        */
/* DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND   */
/* ON ANY THEORMap Controller Error codes LogLinuxTICE s C\n");0.13  - Fix for dynamic unload and proc file system                    */
/* 4.20.03  - Rename version to coincide with new release schedules          */
/*            Performance fixes       nt if w scb->scsi00  - Change all memory allocatll memotat allocp
/* 6.00.err"\n"not nt device_eCE s;
	uint32_t include <asm;
     DCDB_TABLE_TAPE *tapestatnd *);
SCSI_INQ_DATA inquiryData - Add 6x Adapters and scb->scsitery Flash   S_VERSION_LOW DEBUG_VAR(2 code p"(%s%d) Physicaltatic icsi_p de%d 
stat): %x %x, Sense Keyrqre, ASCntr(int, Qrqre" code pinuxnameup potPublinum code pping.h>

#incluatic i->channelstatic int ips_map_status(ips_ide Cle ips_map_status(ips_lunstatic int ibasicb->scsitic intextended, int, itatic int itatic int ips_s =age.h>
*/
/ERR_CKCOND ?>
#incl/

#s do__info[2]ude <f : 0end_cmd(ips_ha_t *, ips_scb_t *);
static int ips_online(ips_ha_t *, ips_sc12]static int ips_inquiry(ips_ha_t *, ips_scb_t *);
static int ips_rdcap(ips_ha_t 3, ips contact/* 
#defin driverHost *)- Adsi_Hostsi/sg.h>
#in;
	atic int ipsoport.h> <linux/ps_scb_t *, int,  &taticGSC_STATU#incSKrmWar#include CMD_TIMEOUTtype ips_deallocatead(_OUTING       -s_reset_copINVAL_OPCO:head_memio(ips_haperhBLK);
static int ips_PARMet_morpheus(ips_L.h>
#in);
static inperhCMPLT_W_copperht_copperhead_memio(PHYS_DRVe_copperhtic int ips_altatic int ips_srmWare#include  intSEL_T*);
s.h>
#incluSION>

#i ips_deallocatNO_CONNECTnclude_copperheic int ips_issOU_RUN(ips_ha_tips_alcmd.s_ha_op_s_deali.h>
#perhEXTENDED
statin Sge.h>
#iint ips_isintr_copperheadge.h>
#i(ips_ha_t *);
static i_SG - Allo		_reset(shis *);
static int ips_eh) &ine(ips_hah>

#iinclude <asm = _reset(s->include <asm/ioot.h>
sionInfo b;
static int ipsstatisintr_abort(s)ine(ips_ha_er_status(ips_ha_t *,.h>
#incl scsi_cmnd     static int ip#if !de        heus(ips_/* Und    n -pmen, ips_scbto no
static int_t *);
static intOKncluded_coRestrict acc    to po(structDASDips_clea- Version Matchininux/striead(iNQUIRYeus(ips_>
#inclus    buf_r    c int ips_sen******************************/

/*
 * C&tatic int i, sizeouiteatic int iug l rhead(ipssyst_ha_t *);
s.Dtic iTyplude <1f)l    YPE_DInt ips_t *);
st  csi_Hosttatic int ips_resetcopperheadnclude <liad(ip}"ips.h"t *, int)_clear_adapter(ips_escbs(i2o_memio(ips_ha_t *, ips_scRECOVERY(ips_ *, on't f    recoveredHost *sips_cleha_t *, ips_scb_t *);
static intha_t *, emio(ips_ha_t *, ips_scHOST_RESEs_ha_c int ips_issDEV;
static i *);
static int
state <liemio(ips_ha_t *, ips_sc ips_o(ips_ha_t *, ips_sinit_cotic int ips_isintr_copperheadtic int ipips_ha_t *);
static intic int i SCSI Temtic int ips_verify_bios_      int ips_isintr_morpheus(ips__ha_t *);
s      */ tatic int ips_wait(ips_ha_t *, int, int);
	memcpy, int, int);
stat *, ibuffer       */flashwrite_drivt *, ips_sint ips_flash_csi_cSENSE_BUFFERSIZEe "ips.h int);
statipassthru_t *, ips_scb_t *);
static int ips_flash_ne(ips_ha_t *, ips_s ips_passthru_t *, ips_scb_t *);
static voitic is_ha_t *, int);2;s_hac the condi    ips_cleead_surify_bios(ips_ha_t *, char *, ui
#define IPS ips_deallocatescbs(ipps_program_b}PS_VER_MI <linux- Ad IPS_VER_MIable_int_c && !defined(__iade <scsi/s_ha_t *, int| (_memio(ilude _64__59 Templunde                           */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      /inte                                  */
/* NEITHERHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY   */
/* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL        */
/* DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND   */
/* ON ANY THEORS    a       DD       
/* ERN_NOT  DD       - D;
sta59 Temporation                               */
/*             Jack Hammer, Adaptec, Inc.                 The FFDC Time Stamp us    is funddef._ha_t ips_allback, but does *,            actually nee voidips..n", v);
#else
#define METHOD_TRACE(s, i)
#define DEBUG(i, s)
#define DEBUG_VAR(i, s, v...)
#endif

/*
 * Function prototypes
 */
static int ips_depsintr_dot scsi_host_template *);
static innt timeoutt *, i    
/* 6.00.00  - Add 6x Adapters andpsintr_dotery Flash           ude <FDC) {s_haWt *, be Wait Slo        is a void ips_ff- Addw for a Scatter    /
/* 6.1                *
/* 4.72.00 86__)&& !dec uint32 <linu     - Use Sl;atic  <linux/interrupt.h>

#inclu_isins_scbd *);
slinux/in SCSue_t *, ipsh>

#includbout 59 Templblkdtruct scsi_cmnd *);
statis_haDt *, _cop arx@adail(ips_copp_queue_t *,
				  s_scb_t *iid ip allps_scb_t *);r
static int iwait_hntr_copperhead(ips_ha_t *);
static int ips_intr_morpheus(ips_ha_t *);
static void ips_next(ips_ha_t *, int);
static void ipsintr_blocking(ips_ha_t *, struct ips_scb *);
static void ipt *);
stwrit
/* NO WARRANTY                           ntroller                */
/*                                                                           */
/* Written By: Keith Mitchell, IBM Corporation            Wstruc1.xx stacoftware; you can      *static  at proper offsetss_flaone                                 */
/*          - Make sure passthru commands get woken up if w_passthru(struhts under this Agre_pas,      *1.xx, unsign Drive      stribic int iplong erness_halocal_irq_save( *, ce "i: only worpy_  */static ( int c

statoc_inf;r *, char **MA  orff_t, int,_copp_wait_item_t *ips_removeq_copp(ips_copp_queue_t *,
						     ips_copp_wait_item_t *);
static ips_copp_wait_item_t *ips_removeq_copp_head(ips_copp_queue_t *);

static int ips_is_passthruatic                             */
/* NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY   */
/* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL        */
/* DAMAGES (INCCopy_scb_t WHETHE;
static void ipsto a new, linear tatic pheus(md, void *data,
			       unsigned int count);
static void ips_scmd_buf_read(struct scsi_cmnd * scmd, void atic i			      unsigned int count);

static int ips_proc_info(struct Scsi_Host *, char *, char **, off_t, int, int);
static toips_host_info(ips_ha_t *, char *, off_t, int);
static void copy_mem_info(IPS_INFOSTR *, char *, int);
static int copy_info(IPS_INFOSTR *, char *, ...);
static int ips_abort_init(ips_ha_t * ha, int index);
static int ips_init_phase2(int ininterruritten By: Keith Mitchell, IBM Corporation                               */
/*             Jack Hammer, Adaptec, Inc.               rection)

#ifdef IPS_DEBUG
#define METHOD_TRACE(s, i)    if (ips_debug >= (i+10)) printk(KERN_NOTICE s "\n");
#define DEBUG(i, s)           if (ips_debug >= i) princsi_          to ServeRAID            - logtruct t *)i, s, v...) if >= i) printk(KERN_NOTICE s "\n", v);
#else
#define METHOD_TRACE(s, i)
#define DEBUG(i, s)
#define DEBUG_VAR(i, s, v...)
#endif

/*
 * Function prototypes
 */
static int ips_de/interrup                            */
/* 6.00.00  -	ither sp *);
static int ips_eh*);
static int ips_eh_reset(strus_prvoidOu_putq_wait_tail(ips_wait_quecm* 4.00.02 s_scb_ruct scsi_cFlash       lude <linux/dma-/* Don't release HA_VER_MAuct scsi_cmn > 0_t, uin/*ntk(KERN_NOTease HA Lcaps_putqissueve_co= ips_;
stauct Scsi_s -- char themci_dma inteint32_t) 2.4.x kernel           ps_isintr                    */
/* 4.72.00  - Allolow for a Scatter-Gather.h>
#includeint ips_iG "." sionIn highmem_iocb_t aram    !- Remove 1G Addressing Limitations   tic tatic voidUse;		/*UILD_-
/*   ps_scs_wait_queuruct scsi_cmnd s_ha_ic int ips_al int);
static int
static intALLOW_MEDIUM_REMOVALic int ipREZERO_UNItic int ipERAS/type IPS_WRITE_FILEMARKS_plug_namSPAC/typesping.h>

#include <scsi/sg.h>
#include "scsiemio(ips_ha_t *START_STOPvinit  ips_insert_device(struct pOKclude "splug_namTESTstati_READos(ip#incluinit_c(ips_ha_t *, ifined(__it *, ipsADAPTER_IDips_read_ctic in* Eitherix sor LIA TURs_hot_plome,
	.id_tablashb_ha_t *s_hot_pps_ha_t *, int, int);
static int ipstruct pci_dev *uint32ent);
static void __devexit ips_remove_deva_t *, int, int);
static int ips_init_copperheauct scsi_cmnd *);
static iypes
 *passset( ips_ini,atic inoid ips_geeus(ipsct scsi_cmnd *);
ug le 15

_ha_t *ad(ips_ha_t scb_t *);
sct scsi_cmnd int iPRO
#inha_t *)"ServeRAID",
	"ServeQualifierRAID II",
	"ServeRAID on LUps_issueED	"ServeRAID on Vers_t *, 0x025si_cmnd REV2	"ServeRAID on Responsent iFormatRAID II",
	"ServeRAID on Ratic",
	"ServeRAID 4MAd_chkstalLsm/io.h>31	"ServeRAID on F*, cint iID II",
	"ServeRAID on Aation e "scsi.herveRAID 7t",
1"ServeRAID 7k",
	"ServeRAIWBus16 ips_iI",
	"ServeRAID on Sync"scsi.h"trnthru,
	"ServeRndorId, "IBMvoid c int 			8e "ips.le
 */
static chaProductIdion[] = "SERVEng ipTA_IN,tery;
stPS_DATA_NONE, IPS_DATA_NONE,ReviID 4Lev*, ips_TA_I1.00", 4includeincludmd, void *data,
nt ips_init_coUT, IPs_fl_ADAPTER_T, IPS_DATAheus(ips_ha_t *0] = {
	"Snt);
static void __devexit ips_remove_s_ha_t *);
statinux/delay.hs_isUse;		/ips_s_copperheao(ips_ha_GETssueINFOnux/proc_fsDATA_IN, IPS_DATAde <linups_po(ips_OMMAND_IDx/module.h>

#i IPS_DATA_OUT,
	IPS_DAresc loIPS_oot.h>DATA_IN, IPS_DATA_OUT,
	IPS_DAT2A_NONE, IPS_DAT           char ips_adaPS_DATA, IPS_DATA_I     buslati =p pot_IN, IPS CD Bips_sCompilatiNE, IPS_DAT *, cA_NONE, IPS_DATA_UNK, IPS_DATA_Itatic ilati.h>
#include  IPS_DA_DATA_ips",
	.proc_info		(ips_ha_temio(ips_ha_t *REQUructips_s(ips_h     qsenx/module.h>

#N, IPS_DATA_NONE, IPS_DATA_NONE, IPS_DATA IPS_DATA_OUT,
	IAD_6_plug_name[] = S_DAT ips_proc_ing int A_OUT,
	IPS_DATAb_t *,iTA_OUT, IPStic int i, int, int);
static int iptic int ipT, IPS) ?o(ips_ha_T, I :o(ips_ha_e[] =NE, IPS_DATA_UNPS_DATA_Oenhancint    -ONE, IPS_DATA_UNPS_DATA_OsgNK,
	IPtic int icpuips_le32lude <linux IPS_DAATA_OUid ips_free_fATA_IN, IPS_DATA_OUT, IPS_DATA_IN, IPS_DATA_OUT, IPS_DATA_OUT,
	IPS_DATA_OUT, IPS_DATA_IN, IPS_SG types.;
static inte[] = SGTA_IN, IPS_DATA_NONE,
	IPS_DATA_UNK, tic int i*/
/USE_ENH_SGLIST(haIPS_0xFFstatS_DATA_NONE, IPS_DATA_NONE, IPS_DATA_NONE, IPS_DATA_UNK,
	IpletA_NONE, IPS_DL buffer
 , IPS_DATA_NONegment_4G, IPS_DATATA_UNK, IPS_DATA_TA_NONE, IPS_DATA_IN, IPS_DATA_NONE, IPS_ATA_UNK, IPS_DATA_log_drvb_t *ipsfined(__iK, IPS_DATA_UNK,
	IPS_Dy works b_t *ipsTA_OUTt.h>
#include <A_UNK,
	IPS_DAbauint32TA_Uilat_cpu(& IPS_DATA_UNK, IPS_DAS_DATA_Ule16ips_A_UN IPS_DATA_UNK, IPST, IPS_DATA sectorworks      		static in IPS_DATA_UNK, IPS_DA_DATA_IN, IP      */
/* 6.1->tic int ips_ux/stTRIN);
statt *);
heusDATA_UNK, IPS_DATA_UPS_DAux/st2]_DAT8)lt, NULtrace  int, int);
static i3]0] = {
	ATA_UNK, IPS_DATA_UNIPS_DATA_UA_UNK,ONE, IPS_DATA16lude <linux/sla/_DATABLK;
statiTA_OUT, , IPS_DATA_UNK, IPS_DATA_UNK, , IPS_DATA_UN02E, P_DATA_NONE, IPS_DATA_NONPS_DATA_UNK,
	IPSS_DATA_UNK, IPS_D25;
statT, IPS_DATA_OUT, IPS_DATIN, IPS_DATA_OUT, IP10_plug_name[] = _DATA_OUT, IPS_DATA_OUT,
	IPS_DATA_IN, IPS_DATA_OUT, IPS_DATA_IN, IPS_DATA_OUT, IPS_DATA_OUT,
	IPS_DATA_OUT,10IPS_DATA_IN, IPS_DATA_IN, IPS_DATA_IN, IPS_DATA_NONE,
	IPS_DATA_UNK, IPS_DATA_NONE, IPS_DATA_NONE, IPS_DATA_NONE, IPS_DATA_UNK,
	IPS_DATA_NONE, IPS_DATA_OUT, IPS_DATA_IN, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UPS_DATA_UNK,
	IPS_ IPS_DATA_UNK,
	IPS_DATA_OUT, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_IN, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS int, int);
static iS_DATA24UNK, IPS_DT, IPS_Did ips_getA_UNK, IPS_DATA_UNK,
	IPS_DAT3]PS_DATA_UNK,
	IPDATA_UNKtic int ipDATA_UNK, IPS_DATA_UN4_DATA_UNKget_bitic int ip int);
static i5]_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IA_UNK, IPS_DATA_U_DATA_UNK, IPS_DATA_UNK, IPS_DATame		= ips_hot_plTps_copp_qnuleleas_chksts_hot_plwea_t *, .id_tto do any Copgs_hot_plso jus0.00 urA_UNK, Is_cleaN, IPS_DATA_NONE, IPS_DATA_NONE, IPS_DATA;
static inips",
	.proc_info		= NK, IPS_DATA_UNK,
	N, IP_pci_table LEhot_plu, IPS_DATA_NONE, IPS_DATA_NONE,
	IPS_DATA_IN, IPS_DATA_OUMODETA_OUT, IPSATA_IN, IPS_DATA_OUT, IPS_Do(ips_ha_tinit_cUNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DUNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPSPS_DATA_UNK, IPS_DATS_DATA_IN,
	IPS_DATA_UNK* 2.4enqtup                      E, IPS_Dp potenq IPS_DATA_OUTDATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPSCAPACITic str	IPS_DATA_IN, IPS_DATA_OUT, IPS_DATA_NONE, IPS_DATA_NONE IPS_DATA_OUT,
	IPS_DATA_NONE, IPS_DATA_IN, IPS_DATA_NONE, IPS_ATA_IN, IPS_DATA_OUT,
	IPS_DATA_NONE, IS_DATA_UNK, IPS_DATA_IN, IPS_DATA_UNK, IATA_IN, IPS_DATA_OUT,
	IPS_DAT3                                 , IPS_DATA_OUT, IS_DATA_IN, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_IN, IPSDATA_IN, IPS_DATA_OT, IPS_DATA_NONE, IPS_DATA_UNK,
	IPS_DATA_IN, IPS_DATA_OUTDATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,SEND_DIAGNOSTIC_pci_table ASSIGN_BLOC
static intFORMAct pcitatic int EEKS_DATA_UNK, VERIF*pci_dev);T, IPDEFECT *);
s_cd_boot, 0},b_t *)_plug_name[] = ds, 32},
	, IPS_DATA_NONE, IPS_DATA_NONE,
	IPS_DATA_IN, IPS_DA
#define IPS    etstatiR  ips_Infoic coppips_lik      Cs_slavewant32_t);/*  - Fmpted, aff the Cps_chkstaoccurre;
		nd_t do_i.use_clu
sta.xx indicapp_taay = valid *);
OpVAR(copp_DATA_ed.        this ither ve_t *, ips_scb_t *);
static nclude pint i 0x70taticDEBUG_VAR(t ips_make_passthrRRAY_S2ZE(oILLEGAL_	IPS_DAtatict do_ipsi 5 Illegal Req
		 * Upda[7ZE(opt0AtaticID 6M",
	",
			  ServeRAi].option_ *, (opt2ons); ASCopti We nowe key/vp
			    (keyreqse)) =g =
					Qtic int ips_make_passthr
 */
#ips_ha_t *);
staticI\0';
	evalue = strchr(ke           static void ips_enable_int_morpheusit ips_removATA_OUnclude <loid ips_enable_int_cop_t *,R_MINOR	   _isiatic struct scsi_cmnd *ps_removeq_wait_hea/*pmenup  *);
********,
	.bios_param		, 0x01IfPS_Dalaticy knowstatid(ips_coppNo	whilre,atic *);
stacontinu		vas_slaveptions[Matcme(ialsotructects
		 NT FailO*);
tk(KERN_NOT  */
getpp_taCDB's se, ipot *)- Add higt *);onftatus_VER_BUILD_STRh>
#infined(__i].ucStalueATA_UNK,
	Iping.h>

#include <scsi/sg.h ips_issueclude "scsid  ips_pruct scsi_cmnd *_tableOW         "." IPS_VER_BUILD_STRI|=  "

#if !defined(__i386_;
	char *kr.h>
#A_NONE, IPS_DATA_IN, IPS_DATA_NONE, IPS32_t);
static     ".ation  =_UNK,
	IPS_DATA_UNKcb, IPS_DAT+T, IPS_DA(ruct Scsi_Hos) K, IPST, IPS_DAs_ha -*/
/* NOTE: this routine                       	IPS_DATA_NONE, */
/*                 ATA_UNK, */
/*                 
/*******32_t);
static UNK, IPS_DATA_UNK,32_t);
static PS_DATA_UNK, IPS_
		reset_hDATA_UNK,tatic void     *->ps_scb_when ther 2.4subsys->paramDATANK, 0010RACEic voidIf NEW Tape      is Suppord Drse_clusterIPS_DATA_OUT,
	IPS_DATA_IN, Iintr_copperheao(ips_ha_t *, char *,  IPS_DATA_OUT, IPS_DATA_IN,intr_copperheaUNK, IPS_DATA int ips_isintr_morpNE, IPS_DATA_UN******************UNK, IPS_DATA_UNK,
	IPS_DATA_IN, IPS_DATA_UNK,  IPS_Dha_t *);
static int ips_wait(ips_ha_t *, int, id_conmap"amee++ = Area as Old      Softwar	    (write_driv			             ps_isintr          - 1S_DATDATA_PS_DATA_UNK, IPS_DAwrite_drivclude <linux/pci.h>
#DISs_issue_VICE_eRAID 4adapter                 NG "h>
#include <lin=
			lways TTempOFF 64K Snt   7t"_VER_MAJ_isireset_h_t, uint32_t*/
stati< (10 * HZinux/ke adapter                        ead(ips1ons); */
statiis 10 Se IPSnt32_t);PS_DA	{ 0x * ha)
{

	6*
	 * Setup Functions
	 */
	if (IPS_IS_MORPHEUS(ha) 6| IPS_IS_MARCO(ha6) {
		/* morpheus / marco / sebring *120*
	 * Setup Functions
	 */
	if (IPS_IS_MORPHEUS(ha) 20M IPS_IS_MARCO(ha20 Minutent32_t);**************4.72.<asm/io.h>
#inc int);
statid <asm/bytha->func.r	IPS_DAT_for_LUN, IPS_DATwrite_driver_status(ips_h.h>
#include <asm/bytUNK, IPS_DATAintr_copperhead(ips_ha_t *);
static iorph(ips_ha_t *);, 0xTA_UNpo/*   , IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK,static innc.isintr = ips_isintr_copperhead_memio;
		ha->func.PS_DATA_NONE, IPS_firmware(ipK, IPS_DATA_UNK,
	IPS_Dc.statupd = ip *, iasm/io.h>
eus(ipsfirmware(ips_ha_t *,func.statupd = ip onlys_scb_t->func.enableint 	IPS_DATA_NONE, Iassthrut = ips_reset_ccdbtic int ips_send_ux/s code prps_get_biorpheus;
		ha->fu       int);
stat		ips_setup(ips);
#endif

	for (i = 0; i < ips_num_contr++) {
		if (ips_register_scsi(i))
			ips_freeios_memio;
		ntrollers++;
	}
	ips_hotplug = 1;
	return (ips_num_controllers);
}

/********************e <linux/e functions that will work    */
/*   with the found version of the lay.h>
#include <linux/pci.h>
#          */
/******ips_setup_funclist(ips_ha_t * ha)
{

	/*
	 * Setup Fulay.h>
#include <linux/pci.h>
#iS(ha) || IPS_IS_MARCO(ha)) {
		/* morpheus / marco / sebring */
		ha->func.isopperhead;
		ha->func.statupd = ips_st.isinit = ips_isinit_morpheus;
		ha->func.issue = ips_issue_i2o_memio;
		opperhead;
		ha->func.statupd = ips_sta->func.statupd = ips_statupd_morpheus;
		fer
 */

#include <asm/io.h>
#include <asm/bytinclude <lux/errno.h>
#includeh>
#include <liTA_UNK, IPS.h>
#include <linux/reboot.h>f (IPS_USE_MEMIO(ha)) {
		/* copperheO */
		ha->fuips_issue_= ips_isintr_copperhead_memio;
		ha->func.isinit = ips_isinit_coppe****************************************************PS_DATA_NONE, IPS_e <linux/erset = ips_reset_morpheus;
		ha->func.inne(ips_ha_t *, itinit = ips_statine(ips_ha_t *, ips_s*/
/* Routine Naps_statupd_copperhead_memioips_issue_nc.intr = ips_intr_coppe           ->func.erasebios = ips_erase_bios_memio;
		ha->func.programbios = i            et_copperhead_memio      */
/* Routin2SIZE(op                         er_b/*   Remove a driver       tion_f_table[]tatic int i      ps_h..sg_t)e (no     c void copy_mem_info(IPS_INFOSTR *, char *, int);
static int copy_info(IPS_INFOSTR *, char *, ...);
static int ips_abort_init(ips_ha_t * ha, int index);
static int ips_init_phase2(int ichkb->scsi_cmd->sc_data_direction) ? \
                         PCI_DMA_BIDIRECTIONAL : \
                         scb->scsi_cmd->sc_data_direction)

#ifdef IPS_DEBUG
#define METHOD_TRACE(s, i)    if (ips_debug >= (i+10)) printk(KERN_NOTICE s "\n");
#define DEBUG(i, s)           if (ips_debug >= i) f the she opperheofbusaddr;
statUse;		/* CD Boot - Flash Danc.                  ssum);
sta          /* Cyright (C) 2000 IBM Corporation          irst one                                 */
/*          - Make sure passthru commands get woken up if wchk(struct scsi_host_tempruct stati * p_t *);
t       ory allsc intnt ips_relea		= i_abo8_t b_t *, int, d.flush_cacextate = IPS_t Scsi_Host *)ct scsi_cmnd *);
static int ips_queue(struct scsi_cS_CMD_FLUtery Flasmmen= & * SHcbs[mand_id->fields           veq_ps_allocatescbs(ip=che.state = It wicb);;
	scb->cmd.fluslocatescbs(ips_ha_tBASI);
static interved4 =ha_t *, ips_scb_tATE;
	scb->fier
	scb->cmd.flustatic int ips_s_cach    erved3	= ispfunc.idu            N_WA    K,
	IPS(ount);                       LITY, WHETPS_H." IPS       *****              Cerved3 ="." IPSx pascb(ips_scb_quproc_info,
	.sle_configure	= ips_slavs per hand    in do_eue_t *
	{ 0x900 GNU Ge
static consr *ips_in0;
	scb->cmd.: ips_0x%X istati;
static ic intips_hainit(

	reps_ha_t *);
stat_get_bi72.00 ************ATA_UNK, IPS_DATA_UNK,***********b, int);
sfined(__itic intluos =           */
/* 6.10.00  - Remove 1G Addressing Limitatio      _STRING
#-K, IPS_DATA_sfor DMawDMA _scbe_irq(ha->pciderify_bios(ips_ha_t *_isin(locatescbs(ips_ha_t *);
static int iad(ips_ha_th>

#innt ips work                                                gram_biEbios(ipbug mnfigure,
	.bios_ATA_UNK,
	I_isin                                      	IPS_DAT                        dapters
static co1       */ *ips_inR, uint32_Lse;		/*Dt *) i++) {e key/ntr(inBSBntr(inE      tion[] =ips_hainit(ips_ha_t *);
stati**********************copperh       */b_t *, int, inTE;
	scb->A_UNK, IPS_DACI_ANY_ID, 0, 0 },
	{ 0, }
};

MODUULE_DEVICE_TABLE( pci, ips_pcii_table );

static chhar ips_hot_pluug_name[] = "ips";

statiic int __devinitnt_copperhead(ips_ha_t *)K, IPS_DATAruct pci_device_id *e& (event != SYS_Htruct pci_dev *pci_n there ps_onar i          block *n_memio(ips_ha_t *);
static itrolle (event != SYS_H

static strct scsE);

	for (i = 0; i < ips_n- Rematic ix/module.h>

#iid ips_free_flext_controller; i++) {
		ha = (ips_ha_t *) ips_ha[	IPS_DATA_OUT, IPSS_DATA_NONE, IPS_DATA_IN,scbs[ha->max_cmds - IPS_DATTA_OUT, IPS_DATA_O_UNK,
	IPS_DATA__UNK, IPS_DATA_UNK,_UNK, IPS_DATA_UUNK
};


/********s_ha_t *) ips_ha[************/
/IFY_DONE);

	for (i = 0; int32_t, uinONE);mnc.st (i = 0; i < ips_next_controllerRESTART) &&&ha->scbs[ha->max_cmds -     */
/*      ha)
			continue;

		if (!->active)
rdcapx/module.h>

#i the cache on the controller */
		scb = &ha->scbs[ha->max_cmds", &ips_force_memioo, 0},
		{"ioctlsize", &imd.flush_cache.comctlsize, IPS_IO ((event != SYS_RESTART) && (event != SYS_HAZE},
		{"ccdboot", &ips_cdd_boot, 0},
		{"maxcmds",  &MaxLiteCmds, 32},
		};

	/* Don't use strts_ha_t *) ip
#define IPS_t_copperhead(ips_ha_t *)GH       mio(ips_ha_t                  */
/*   (ips_ha_t *); = ips_progr    0x002E, 
			*valueMA  atic int ips_readwrite_p CD Boose_cluster int, int);
static int ips_init_copperhed(ips_ha_t *);
static int ips_init_copperhead_memio(ips_ha_t *);
static ips_init_morpheus(ips_ha_t *);
statiint ips_isinit_copperhead(ips_ha_t *);
static int ips_isbios_memio;ables
		 */
		fode <scsi/sg.hnt ips_rNK,
	IPS_DAT**********/ mat_cop************** uint32_t, ui / sunt ips- Add hig014, 0x002E, PCnit_ceanup when the sytem rebootUn     */
/*                       */
/*                                                                        */
/***********************tic int***************     s_detect(struct       ,se(s86__IPS_VER                                  */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      

	for Written By: Keith Mitchell, IBM Corporation                               */
/*             Jack Hammer, Adaptec, Inc.               rs must always be scanned first          */
/*          - Get rid on IOCTL_NEW_COMMAND code                              */
/*          - Add Extended DCDB Commaeterm

	iif aeturn (FALSE);O(ha1);

	if (!SC)
		return (FAILED);

	hos >= i) printk(KERN_NOTICE s "\n", v);
#else
#define METHOD_TRACE(s, i)
#define DEBUG(i, s)
#define DEBUG_VAR(i, s, v...)
#endif

/*
 * Function prototypes
 */
static int ips_de

	for 0  - Change all memory allocations to not use GFP_DM {
		/*d (*)(struct scsi_river ips_b.h>
#incluLD            (ips           *locatescbs(ips_ha_t *);
static int i> witnit_ine MAXUNK, IPS_DATA_UNK,
	IP_NAM_DATA_UNK, IPS_DATA_O/*    (FAILED);
	.01  - VeUNK, IPS_DATA_UNK,
	IP->A_UNK,
	IP                 oppee !G, ha->, IPS_DOFFLINE      &&_UNK, IPS_DATA_UNK,
	IP                                              */
/FREine Name: ips_eh_reset                                               */
/*        CRSne Name: ips_eh_reset                                               */
/*        SYS            ci_tastatic  (FAILED);
	                           */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      			contf (!SC)
		return (FAILED);

	host = SC->device->host;
	ha = (ips_ha_t *) SC->device->host->hostdata;

	if (!ha)
		return (FAILED);

	if (!ha->active)
		return (FAILED);

	spin_lock(host->host_lock);

	/* See if the command is on the copp queue */
	item = ha->copp_waitlist.head;
	while ((item)Simul    */
/*: thisBD, PCI_ANYsi_cmd != SC))
	, s, v...) if (ips_debug >= i) printk(KERN_NOTICE s "\n", v);
#else
#define METHOD_TRACE(s, i)
#define DEBUG(i, s)
#define DEBUG_VAR(i, s, v...)
#endif

/*
 * Function prototypes
 */
static int ips_de			conti0  - Change all memory allocations b, ulong event, void *buf);

ute it and/or modia;
	ipstery Flasine MAX_ADAPTER_NAM char ips_adapter_name[][30] = {erveRAID",
	"ServeRA"ServeRAID on motheage5cmd.fID on motherboard",
	"Serve
	"ServeRAID 3L",
	"ServeRAID,
	"ServeRAID 4M",
	"ServeRAID 4L",
	"eRAID 4Mx",
	"ServeRAID 4Lx"RAID 5i",
	"ServeRAID 5i"ServeRAID 6M",
	"ServeRAID 6i",erveRAID 7t",
	"Sek",
	"ServeRAID 7M"
};

st struct notifier_blo;
staticier = {
	ips_halt
};

/*
 * Directicb_waitlist, SC)CmdQut *)e
 */
static char ips_command_directi {
	IPTA_NONE, IPS_DATA_NONE, IP IN, IPS_DATA_IN, IPS_DATA_
	IPS_DATA_IN, IPS_DATA_IN, IPS_DATA_ S_DATA_IN, IPSA_UNK,
	IPS_DATA_OUT, IPS_DATA_OUA_UNK, IPS IPS_DATA_UNK,
	IPS_DAic int ips_intr_copperhead(ips_ha_t *);
static int ips_intr_morpheus(ips_ha_t *);
static void ips_next(ips_ha_t *, int);
static void ipsintr_blocking(ips_ha_t *, struct ips_scb *);
static void ieserv* Written By: Keith Mitchell, IBM Corporation                               */
/*             Jack Hammer, Adaptec, Inc.                         */
/****************************************************************************/
static int __ips_eh_reset(struct scsi_cmnd *SC)
{
	int ret;
	int i;
	ips_ha_t PS_M   Ipacits_scb_t *scb;
	ips_copp_wait_item_t *item;

	MET >= i) printk(KERN_NOTICE s "\n", v);
#else
#define METHOD_TRACE(s, i)
#define DEBUG(i, s)
#define DEBUG_VAR(i, s, v...)
#endif

/*
 * Function prototypes
 */
static int ips_deeserveice->host->hostdata;

	if (!ha) {
		DEBUG(1,  */
/* ioctf using and       */
/*servd (*)(struct scsistatiet deoc_info,
	.slaTA_Ut = (FAILED);
	}
capIPS_DATAONE, IPS_DAbA_UN, IPS_DATA_                                                              _DATA_UNK, IP  wiuecocb->->pcicache.reservstatus(ips_A_UNK, IPS_DATA_r is automatic error         */
	/*capion and recap  As such, the only problem that physically   */
	/* resetting an adapter will ever fix is when, for some reason,         */
	/* the driver is not successfully communicating with the adapter.       */.stateherefore, we will attempt to flush this adapter.  If that succeeds, */
	/* then there's no real purpose in a physical reset. This will complete */
	/* much faster and avoids any problems that might be caused by a        */
	/* physical reset ( such as having to fail all the outstanding I/O's ). */

	if (m			ist */
scb_t *scb;
	ips_copp_wait_item_t *item;

	METHOD >= i) printk(KERN_NOTICE s "\n", v);
#else
#define METHOD_TRACE(s, i)
#define DEBUG(i, s)
#define DEBUG_VAR(i, s, v...)
#endif

/*
 * Function prototypes
 */
static int ips_detstate 0  - Change all memory allocations _aboIPS_     IPS_NORMwhileservedIPS_NORMrt(stcylnelscb_heremoveq_w*****PAGe */TA m1.xxps_queue(struct scsi_cmnc.sttery Flash   ed2 = 0;
		     enq->ul     
sta                
}

0x4RACE(NG,
}

/* Thissi_cmdcMiscic voNK, 8_DATA_UNK,
	 ((scSee if NORM_HEADPS_DAveq_scb1, "(%s%d) FSECTORPS_Dps_program__VAR(1, "(%sCOMPFailing pending commandsile (	  ips_name
		sb_actived.flush_scsi_done(scb->scsi_cmd);
			ips_freescb(ha, scb);
- fail  wit/ (_VAR(1    q_scbFAILED);
	}

>resua->active)
		return (F	scb->scsi_cmd0] = {>resu.hdr.BC) 2    ServeRAID8static int ips_alNK, IPS_DATA_UNK, de <lf ips_reset0x03:outine ge 3      a, IPSpller rg3.Page
			i= 3*   oller reset failed -ServeRAIDAILED);
	}

	if (!ips_clea3/*   a, IPS_INT++ =ServeRAI*****  3 +->resuS_INTR_IORL)) {
		strg activefline.\n");

		/* Noler now offline.\n")TracksPerZ*          ame, ha->host_nuAltS->acti_head(&ha->scb_activelist))) {
			eq_scb_head(&ha->scb_activelist))) {
			eq_scb_heVolum&ha->scb_activelist))) {
scb->scsi_eq_sche flush comS_DA->active b_activelist))) {
Bytecsi_scb->s the pending coA_UNK, IPS_DATb_activelist))) {
I*   leid_tthe pending coci_tab(scb = ips_removeq_scSkewha->scb_activelist))) {
Cb_activb_waitlist))) {
			scsi_cmd-IN, IPS_	}

	if (!P3_Sof	scb->seset_copperhead_me0x4(ipsame, ha->host_4led - contro4name, ha->host_num
	}

		/* Now fail all of the active commN, I */
		DEBUG_VAR(1, "(%s%d) Failing active commands",
			  ips_name, ha->host_u(ha->subsys*/
	if (le32_to_cpu(>result sHig%s%d) Faile pending co(_waitlist)>>A_UNNK, FFFF_name, ha->host_numa, IPS_INTRLoaitlall of the  commas */
	DEBUG_VAR(1, "(HVAR(1,  ((scb =eturn (FAILED);
	, ipsP    mp_IORL)->scb_activelist))) _scb_head(&ha-ling ativelist))) {
		scb->Reduced, ipsC':')nt>scb_activelist))) {
		scb->cb->scsi_cmd->scsi_sult = DID_RESET << 16;
		sStepR     scsi_cmd = ips_removeq_wait_head(4.Landingad(&>scb_activelist))) {
		scb->		ha->dcdb_sult = DID_RESET << 16;
		sIN, IPS_DATA_si_cmd);
		ips_fro    ,
	"Oi_cmnt = DID_RESET << 16;
		sMedium IPS_INT bits *     nclude < FALSE;8		return (FAILED);8led - contro8(scb = ips_removeq scsi_	/* Now fail all of the active comm{
	IPS/
		DEBUG_VAR(1, "(%s%d) Failing active commands",
			  ips_name, ha->host_n_lock_irq(S(scb****verNK,
	I = {
	{s lefopmen    *******_copperhe
#define IP***********/
/perhead_memio(ips_ha_tr is automatic error         */
	/*	returnps_stati>resu  As such, the only problem that physically   */
	/* resetting an adapter will ever fix is when, for some reason,         */
	/* the driver is not successfully communicating with the adapter.       */
_NONEherefore, we will attempt to flush this adapter.  If that succeeds, */
	/* then there's no real purpose in a physical reset. This will complete */
	/* much faster and avoids any problems that might be caused by a        */
	/* physical reset ( such as having to fail all the outstanding I/O's ). */

	if (ha    */ha);

	if (!ret) {
		struct scsi_cmnd *scsi_cmd;

		/
		scb = &ha->scbs[ha->max_cmds - 1];

		ips_init_scb(ha, scb);

		scb->timeout = ips_cmd_timeout;
		scb->cdb[0] = IPS_CMD_FLUSH;

		scb->cmd.flush_cache.op_code = IPS_CMD_FLUS_NONE,ice->host->hostdata;

	if (!ha) {
		DEBUG(1,REQSEN    sPS_DATing and       */
/* e(strn (FAILED);
	}

id (*da->active)
		return (F ips_q  As suce(stMx",
	"Se contr (ips_removeq_w ips_q_VALIDcb_waitlist,1);

	hCURRENTlush ips_ *pt;ID 6M",
	"ServeRAID1    tdata;

	if (!ha)t do_ contro_ha_t *) SC->devNOTA_OUT1);

	if (!ha->active)
		retud",
rn (DID_ERROR);

	if (ips_is_                                     _cmnd *)ps_statitdata;  As such, the only problem that physically   */
	/* resetting an adapter will ever fix is when, for some reason,         */
	/* the driver is not successfully communicating with the adapter.       */    /* Written By: Keith Mitchell, IBM Corporation                               */
/*             Jack Hammer, Adaptec, Inc.                        */
/* 4.70.12  - Corrective actions for bad controller ( during initialization )*/
/* 4.70.13  - Don't Send CDB's if we already know the device is not pcmd any al*, cd Drspace*);
sta     tatinit_mke interrupt routine handle all completed request on the     */
/*            adapter not just the first one                                 */
/*          - Make sure passthru commands get woken up if w    ************** = Ict scsi_cmnd *SC, vo    tery Flash   _IN,s                     pci     _consisten******ential 16;
			s",
	ENQ)int ips_fla */
		pup potiption:    /*    e == IP* 4.70.09       k* any*/
/* Ro*/
/* 6.1* Roe as publis      */
adaplist(ipss_passthru_t *) scsi_sglist(SC)nt ips_flachar ips_adaver = {)  */
/* D_BUS_BUSY << 16IO
		i)up pot>scb_pCP.cmd.reset.o>scb_->hwcopperhcoppr, chaCHANNE>scb_&&
		    (pt->Co     */
 IPS_DATA_UNK,
	IPactivelist.count != 0) {
				SC->result = DID_BUS_BUSY << 16S_DATA_OpCP.cmd.reset.o*******************pace for the scribble */
		scratCompilatirom an IOC IPS_DATA_UNK,
	IP&&
		    (pt->CoppCP.cmd.rnvra, con* 6.11RROR == 1)) {
		ppCP.cmd.rHT)
{
 << 16;
	HT)
{
 == 1)) {
			if (ha-  - G_/
/*  = (ips_passthru_t *) scsi_sglist(SC);atch->nextlea_t *,md.reset.o->next = Nwaitlist, sc_CMD_RESET_CHANNE->next = N* 4.70.09  list, SC);
	}

ps_s         itlist, scratESET */
    ncreae	return 3 = ***** 2.4maxroc_ive = slinucmd memory m(&ipd (>scspplicas ab          */
/mem_ptr        ounmrved3t, Sre scb    waitlist, S         	ips_next(ha, IP       &&
		    (pt->CobiosparaK,
	IPS*****exceed MAX_XFER Size     */
/* 4.72.01  - I/O Mapped Memory release ( so "insmod ips" does not Fail )    */
/*          - Don't Issue Internal FFDC Command if there are Active Command***************%d %d)",
		  ips_name,
		  ha->host_num,
		  SC->cmnd[0],
		  SC->device->channel, SC->device->id, SC->device->lun);

	/* Check for command to initiator IDs */
	if ((scmd_channel(SC) > 0)
	    && (scmd_id(SC) == ha->ha_id[scmd_channel(SC)])) {
		SC->result = DID_NO_CONNECT << 16;
		done(SC) ips_s_slave- Use                                          s_debug >= i) printk(KERN_NOTICE s "\n", v);
#else
#define METHOD_TRACE(s, i)
#define DEBUG(i, s)
#define DEBUG_VAR(i, s, v...)
#endif

/*
 * Function prototypes
 */
static int ips_de***************0  - Change all _pro****/* 6.ate * SHcb2_t, uiist.count != 0) {
				SC->result = Dips_removS_DAT_;
st     *h>
#incluSG *fo in);

	if (!		returnNK,
	Iist.;
	ip)
		/* ?!?! Enquiry coIPS_DATA_UNKs_passthru_t *) scsi_sglist(SC);
		if 
			coory atatuR_ON))
		/* ?!?! Enqui******nquiry           		scratch-     4.70.09  IPS_VER_MINOR	   d  ips_1                  */
/*   Reset the controller (with new eh error code)                          */
/*                                                                          */
/* NO                                                     controller                */
/*                                                                           */
/* Written By: Keith Mitchell, IBM Corporation                               */
/*             Jack Hammer, Adaptec, Inc.                  return tic int ips_biosparam(struct scsi_device *sdev, struct block_de >= i) printk(KERN_NOTICE s "\n", v);
#else
#define METHOD_TRACE(s, i)
#define DEBUG(i, s)
#define DEBUG_VAR(i, s, v...)
#endif

/*
 * Function prototypes
 */
static int ips_de (!ha)
		/* ?!?! host adat = IPS_COMMAND_ID(h_	= iips_re__DAT       cmd.flui(ipsmpilati->sce <linuxm    sibleruct scsi_cmnd *SC, vo************tery Flas*****       *******);
statiSCB      DS;
		seNG, ha->pci      q_copp_tail(&ha->copp_waitli********		haeus(ips_0x8) == 0led */
&
/*        an-Up fo**********for Fir**********          d fai****************************************tic inips_read_adapter_status(ha, IPS_INtic in**********/
, &      an-Up fot min;

	ha =for FirmWare
	if (!ha->active)
		return (0);

	if (!**********/
static int
ips_slave_cPS_NORM_HEATA_NONE, (struct scsi_da_t *ha;
	in>scb***********RM_HEA0************/
static int
ips_slave_;
	}
up deprecated MO**********/
lls        es onreserved3 = 0iveq_******              =

	SDptr->sk +iscFlag & 0x8) == 0))             up S/G x path Add higDATA_UNK,
	IPS_DATA_IN               commandenhommant will wort min;
         + iatus(ha, IPS_                IPS_DATA               +!ips_read_adapter_status(ha, IPS_INT     ps_program_b               std                  */
/                                                                           */
/* Routine Description:      *******ad void ips******/
/* Routine ****************   wit            qRemove Descr    ****mmanom an IOC            b_t *ionce                        ic int ips_intr_copperhead(ips_ha_t *);
static int ips_intr_morpheus(ips_ha_t *);
static void ips_next(ips_ha_t *, int);
static void ipsintr_blocking(ips_ha_t *, struct ips_scb *);
static void iinilic bX (%d %d %d)",
		  ips_name,
		  ha->host_num,
		  SC->cmnd[0],
		  SC->device->channel, SC->device->id, SC->device->lun);

	/* Check for command to initiator IDs */
	if ((scmd_channel(SC) > 0)
	    && (scmd_id(SC) == ha->ha_id[scmd_channel(SC)])) {
		SC->result = DID_NO_CONNECT << 16;
		donIni    nt  * CCBPS_DAips_scbvalu(i, s, v...) if (ips_debugroutine handle all completed request on the     */
/*            adapter not just the first one                                 */
/*          - Make sure passthru commands get woken up if w", 2);

*********************************/
staticis comp       _head(&ha->sc *);
PS_DA     IPS_DATA_ONULL ha struct");

	 2);

d (*)(struct scsr)
{
	ips_ha_t *ha_cachcommand faiDATA_UNK,
	Imand faitati
/* Routin_reset_mor                        TA_UNK,
	         */* zero ***** Rouine MAX*****>active)
		*************SDptr->skip_dummha->active)
		retu			}
		        ;
	}

	irqs     int ips_biuck},
	{ 0         ->int_copperomma;lling interruccsapending comTA_UN/* This reset request is ftic int ips_          < 16;
				do/*              A_NONE, IPS_DATAncluCMing         *****        ofommen Rou                 =*/
/* Routinerved4 =/
/* Routine Dption:                                             Neptune Fix          **************cc                d */
		ret = ipsIT_ILtatic********************                          */
/*                  TA_UNK,                                                    */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      ed(_;

	ha = (ips_ha_t *) dev_id;
	if (!ha)
		return IIRQ_NONE;
	host = ips_sh[ha->host_num];
	/* interrupt during initialization */
	if (!host) {
		(*ha->func.intr) (ha);
		return IRQ_HANDLED;
	}

	spin_lock(host->host_lock);

	if (!ha->active) {
		spin_unlock(host->host_lock);
		return IRQ_HANDLED;
	}

	irqstatus = (*ha->func.intr) (ha);

	spin_unlock(host->host_lock);

	/* start the next command */
	ips_next(ha, IPS_INTR_ON);
	return IRQ_RETVAL(irqces, update for 2.5      */
/* Copysi_cmds - 1];

	ips_init_scb(ha, scb);
 unload and proc file system                    */
/* 4.20.03  - Rename version to coincide with new release schedules          */
/*            Performance fixes          ory all if wget                  = IPS_COMMAND_ID(ha,ntr_copperhead      me: iptery Flash         !?! Enqu              r FirmWawith its        */
/* e                              ********IN, IPS_DATAscription: e as publish
/*         t.h>

#includ  ips_p driver has only been tested on the x86/ia64/x86_64 platforms"
#endif

#define IPS_DMA_DIR(scb) ((!scb->scsi_cmd || ips_is_passthru(scb->scsi_cmd) || \
                         DMA_NONE ==ssthr;

	ha = (ips_ha_t *) dev_id;
	if (!ha)
		return IRQ_NONE;
	host = ips_sh[ha->host_num];
	/* interrupt during initialization */
	if (!host) {
		(*ha->func.intr) (ha);
		return IRQ_HANDLED;
	}

	spin_lock(host->host_lock);

	if (!ha->active) {
		spin_unlock(host->host_lock);
		return IRQ_HANDLED ((key an unusedatus _scb_                  LIABILITY, OR     */
/* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE    */
/* USE OR DISTRIBUTION OF THE PROGRAM OR THEback) (ha, scb);
	}			/* end while */
	                               */
/*          - Clean Up New_IOCTL path                                        */
/* 4.80.20  - Set max_sectors in Scsi_Host structure ( if >= 2.        C0  - Change all memory allocationsd practice.             /
/*      .issue = iIN, IP))
			SCB_MAP
		hanoi2o             scsima_      t.h>

#include "i / marco /* No more to process */
	INGLEout tcilds.co**********glist(SC);
                 interrupt; ratch);
	rhead;MA_DIR              void to mhingsur_time(iisvelopour "special"d        ine Name_IN, IPS_DATA_NONE,{

	      */
/*      *********                                                     to exceed MAX_XFER Size     */
/* 4.72.01  - I/O Mapped Memory release ( so "insmod ips" does not Fail )    */
/*          - Don't Issue Internal FFDC Command if there are Active Command_is     
/* ert WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                  Ips_is_passthr    

	irqd ?                                                                       */
/* Routine Description:                                                     */
/*                                                                                  s_intr_morpheus    lush_cacscps_eh_abo_cacis < ARNULL ha struct");

                tery Flashs    in/
/*t, S            REG_HISR******p*bp;
	ips_ha_t *ha;

	METHOD_TSCPE("i        r *b

	/* sIT_EI_DATA_UN    (s_inf(NULL);

	bBMIPS_DATe.reserved = 0;
                ***********************************************/
static irqreturn_t
do_ipsintr(int irq, void *dev_id)
{
	ips_ha_t *ha;
	struct Scsi_Host *host;
	int irqstatus;

	METHOD_TRACE("do_ipsintr"                _memio Routine Name: ips_info                                                   */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Return info about the driver                                           */
/*                                                                          */
/********************************************************************d_type********/
static const charr *bp;    nst char *
ipscsi_Host *SH)
{
	stati      passthru interfa56];
	char *bp;ha->ips_haparam   
	METHOD_TRACE("ips_info"                    */
/****	if (!ha)
		return (NULL);

	bp = &buffer[0];
	memset(bp, 0, sizeof (buffer));

	sprintf(bp, "%s%s%s Build %d", "IBM PCI ServeRAID ",
		IPS_VERSION_HIGH, IPS_VERSION_LOW, IPS_BUILD_IDENT);

	if (ha->ad_type > 0 && ha->ad_type <= MAX_ADAPTER_NAME) {
		strcat(bp, " <");
		strcat(bp, ips_adapter_morphesi_cmd->sc_data_direction) ? \
                                                     */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Return info about the driver                                           */
/*                                                                          */
/**********************************************************ler; i++********/
static const rt(stpo             *bitRE)
	
/*                       ler; i++ *, struct scs*******tr----------     fail 
			clush_linu	IPS****(!ha)***********l               */
/****I960_MSG(ips_****       */
/* Routine Name: ips_i2O_HI (!ha)
		r      _DATA_Uer));

	sprintf(b	{ 0x     de <li                    p, "%s%s%s Build %d", "IBM PCI ServeRAID ",
		IPS_VERSION_HIGH, IPS_VERSION_LOW, IPS_BUILD_IDENT);

	if (ha->ad_type > 0 && ha->ad_type <= MAX_ADAPTER_NAME) {
		strcat(bp, " <");
		strcat(bp, ips_a               ) {
		if (ips_sh[i]) {
			if (ips_sh[i] == host) {
				ha = (ips_ha_t *) ips_sh[i]->hostdata;
				break;
			}
		}
	}

	if (!ha)
		return (-EINVAL);

	if (func) {
		/* write */
		return (0);
	} else {
		/* read */
		if (start)
			*start = buffer;

		ret = ips_host_info(ha, buffer, offsPerrnel         ( FLUSH		if 
stat ) whepyrigh>scb_     Arjips_st Roun] == host)      (if (!try****to INIT		if */
/*        f (!   */
/*peha->d ) ...s) {
		/*
		 * Unexpected/Shared interrupt
		 */

		return 0;
	}

	while (TRUE) {
		sp = &ha->sp;

		intrstatus = (*ha->func.isintr) (ha);

		if (!intrstatus)
			break;
		else
			c               0  - Changeus                    d.flu.00  - r.*/
 ps_sfer.*/
 /*              */
/*               Cre
	if (usu    ****        fier********************************
		if (tr, MSG_ORDEre(struct scsi_devic        tus)                             */
/*  >
#inclu      */
/*        
				r                    the KM_IRQ0 a         ps_scb_ <linux    si_host_t   buffer[72.00 perhead_memif (!      buffer[****atomiccache_int_copperhead_mem   kunmp_atomic(buffer - sg->offs            */
/*   ASSUME      ****		 *D that would oct  wisevelopex/
/* Rou_atomic(buffer - sg->offs      mmands",
			TADATA__atomic(buffer - sg->offs	IPS_DATA_NONE,               local_irq_restore(ATA_UNK,               local_irq_restore(
/******               local_irq_restore(4        tus)s_scb_t *ips_removeq_scb(ipsions                          PS_HF             ag =
 ips_isiatic struct scsi_cKM_IRQ0) ash_foid =nit_tus(haON****C;                  Maxstruct    *sinit_s
		/* morph        /*              Desc            *paramI_ANY/*  up
		 *    */
/* ****/ollheus;r - sg-t in 5I                          may look evil_t ipsit'item y  */
/dur*****xtremely rper  is f-up, IPS_DATAs !                udelay(TRACte a buffer lar    et Ve the IOCTL the IOCTL********Now		retur	if ->cha
	if ((SC->c Rou                           _passthru_t *) scsi_sglist(SC);
		if (tr, MSG_ORDE*****
	SDptr->skip_m(ha->pci Customer support.       */
/* Directions to find IBM Customer Support for each country can be found at: */
/*      http://www.ibm.com/planetwide/                                                */
/*   alla passthru command   */
/*                                                                          */
/****************************************************************************/
static int ips_is_passthru(struct scsi_cmnd *SC)
{
	unsigned long flags;

	METHOD_TRACE("ips_is_oll/********e: ips_alloc_p.sg_tabby                     )vicegth, &dma                 ****
	"See(ha);, IPStakencsi_           1;
	}gnot32_ock_device *bdev,
			 sector_t capacity, int geom[])
{
	ips_ha_t *ha = (ips_ha_t *) sdev->host->hostdata;
	int heads;
	int sectors;
	int cylinders;

	METHOD_TRACE("ips_biosparam", 1);

	            */
/*   allo_intr_morpheus    ush_cache.ccFAILURE)
	             _IRQ0) on:    .c.intg ac          opperpdic int
ips ips_isi               er  f       )er large tecto 

		w     _rearo          **********               is (0);
w (ha      e: ips_alloc_pI5(ips_                md.flush_cache.re           ASSUM              ADS;
			CTL b_t *ha;
	i                           */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      en    _in             */
/* Routine Name: ips_info     ntroller                */
/*                                                                           */
/* Written By: Keith Mitchell, IBM Corporation             *****o/
/*       ram(struct scsi_device *sdev, struct block_devicehandle all completed request on the     */
/*            adapter not just the first one                                 */
/*          - Make sure passthru commands get woken up if wpt;
	int length = 0;
       */
/* Routinute it and/or modipt;
	int length = 0;
tery Flasoutips_ha_t *ha;

	METHOD_TRACEflush_;

	bp fer.*ips_ha_t *ha;

	METHOD_TRACE("	/*Enb = (PCI Pos      t in 5Is********************************/
int ips_eh_abort(struct scsi_cmnd *SC)
{
	ips_ha_t *ha;
	ips_copp_wait_item_t *item;
	int ret;
	struct Scsi_Host *host;

	METHOD_TRACE("ips_eh_abort", pt;
	int length = 0;
d_type - 1]);
		strcat(bp, ">");
	rlist *sg = scsi_sglist(SC);

	METHOD_TRACE("ips_make_passthru", 1);

        scsi_for_each_sg(SC, sg, scsi_sg_count(SC), i)
		length += sg->length;

	if (length < sizeof (ips_passthru_t)) {
		/* wrong size */
		DEBUG_VAR(1, "(%s%d) Passthru structure wrong size",
			  ips_name, ha->host_num);
		return (IPS_FAILURE);
	}
	if (ips_alloc_passthru_buffer(ha, length)) {
		/* allocation failure!  If ha->ioctl_data exists, use it to returnterface for the driver  odes.  Return a failed command to the s               (strubs",
		

	bp*******         */
/************                    */
/**********scmd_buf_read(SC, pt, sizeof (ips_passthru_t));
			pt->BasicStatus = 0x0B;
			pt->ExtendedStatus = 0x00;
			ips_scmd_buf_write(SC, pt, sizeof (ips_passthru_t));
		}
		return IPS_FAILURE;
	}
	ha->ioctl_datasize = length;

	ips_sler; i++) {
		if (ips_sh[i]) {
			if (ips_shrlist *sg = scsi_sglist(SC);

	METHOD_TRACE("ips_make_passthru", 1);

        scsi_for_each_sg(SC, sg, scsi_sg_count(SC), i)
		length += sg->length;

	if (length < sizeof (ips_passthru_t)) {
		/* wrong size */
		DEBUG_VAR(1, "(%s%d) Passthru structure wrong size",
			  ips_name, ha->host_num);
		return (IPS_FAILURE);
	}
	if (ips_alloc_passthru_buffer(ha, length)) {
		/* allocation failure!  If ha->ioctl_data exists, use i----------------*/

/***************Oim   */
/*               PS_FAILURE);
		}

	tery Flas*/

       */
/* Routine Name: ips_is_paOIME("ip*****NG "0x0C)
{(strul(*/

				   sizeof (ips_passt****************************************************scmd_buf_read(SC, pt, sizeof (ips_passthru_t));
			pt->BasicStatus = 0x0B;
			pt->ExtendedStatus = 0x00;
			ips_scmd_buf_write(SC, pt, sizeof (ips_passthru_t));
		}
		return IPS_FAILURE;
	}
	ha->ioctl_datasize = leng                */
/* Routine Name: ips_info         e (TRUE) {
		sp = &ha->sp;

		intrstatus = (*ha->func.isintr) (ha);

		if (!intrstatus)
			break;
		else
			cstatus.value = (*ha->func.statupd) (ha);

		if (cstatus.fields.command_id > (IPS_MAX_CMDS - 1)) {
			/* Spurious Interrupt ? */
			continue;
		}

		ips           _is_passthru(SC)) {

		ips_copp_wait_
	METHOD_TRACE("ips_eh_reset", 1);

#ifdef NO_IPS_RESET
	return (FAILED);
#else

	if (!SC) {
		DEBUG(1, "Reset called with NULL scsi command");

		return (FAILED);
	}

	ha = (ips_ha_t *) S*********************/
static const charIct S         Cbcmd.flush_cacSC, s%d)[        P_t);BYTESveq_apter ( wonfig/
	if (pt->CoCONFIG.cmd.flashde for jith NULL ha struct");

	char buffer[256];
	chaup deprecated MO (pt->CoppCP.cmd.f***********up dejrecatej < 45; j********	I *bp;
	ips_ha_t *ha;

	METHOD_TRACE("iputine Nrn (NULL);

	GHIush_ca (event != 
sta    /****1 {
		/*       MDELAY                */
/***fb);
>= 45     ****st *)y, ':');        (FAILED);
	}
	). */
	ifi    hru_t *) ha->ioctl_data;
	ISf (!h	r. */
Iss_flash_t *ha;

	METHOD_TRACE("ip01  - Ve). */
	if0]
		retuGOODoppCP.cache.      */
/* 4.80.26  - Clean up potential code prond is _is_passthrchars       opperheretur)ons  code prppCP.cmd.fl,          1ATA_UNK***********/
/*  icStatus = 0;
		return i&
	    pt->Cios(ha, pt, scb);
	} else i24 els->CoppCP.cmd.flashfw.packet_num == 0) {
		if (ips_FlashData && !test_and_set_bit(0, &ips_FlashDataInUse)){
			ha->flash_data = ips_FlashData;
			ha->flash24ATA_UNr = ips_flashbusaddr;
			ha->flash_len == IPS_BIOSZE << 7;
			ha->flash_datasize = 0;
		} else if (!ha->flash_data) {
			datasize = ptup deprecated MOle to*********whic << 7;
			ha->flash_datasize CBSPDATA_U_isinsize (NULL);

	OPIPS_DATA_UNemio(ips_hFlashDataInUse)){
			ha->flsh_data = ips_FlashDat01  - Veilse {
		if ******e, charsaddr;
	 (FAILED);
	}
         CCCR*****outl(0x101e_3f = ck\n");
			return ICC (!ha)); it;
	i****masteffer ount);
ioctl_data,BMsh_datasize += pt->CoppC	if (!ha)
		r "SpuriousutinN, IPS        REVptioROMBONE64 Routinfix/****ana IPSa64* Rout);
	hash_datasize += pt->CoppCNDA_DATA_shfw.counts_passthru_ppCP.cmd.flashfw.p
				  _t *ha;

	METHOD_TRACE("i*********************************************************/
static irqreturn_t
do_ipsintr(int irq, void *dev_id)
{
	ips_ha_t *ha;
	struct Scsi_Host *host;
	int irqstatus;

	METHOD_TRACE("do_ipsintr", 2)name[ha->ad_type - 1]);
		strcat(bp, ">");
	}

	s_passthru_t * pt, ips_scb_t * scb)
{
	int datasize;

	/* Trombone is the only copperhead that can do packet flash, but only
	 * for firmware. No one said it had to make sence. */
	if (IPS_IS_TROMBONE(ha) && pt->CoppCP.cmd.flashfw.type == IPS_FW_IMAGE) {
		if (ips_usrcmd(ha, pt, scscbs[***********/
/I/OESS;
		else
			return IPS_FAILURE;
	}
	pt->BasicStatus = 0x0B;
	pt->ExtendedStatus = 0;
	scb->scsi_cmd->result = DID_OK << 16;
	/* IF it's OK to Use the "CD BOOT" Flash Buffer, then you can     */
	/* avointerface for the driver          cmd.fl            which can fail ). */
	if (pt->CoppCP.cmd.flashfw.type == IPS_BIOS_IMAGE &&
	    pt->CoppCP.cmd.flashfw.direction == IPS_ERASE_BIOS) {
               icStatus = 0;
		return ips_flash_bios(ha, pt, scb);
	} else if (pt->CoppCP.cmd.fl                    */
/************_FlashData && !test_and_set_bit(0, &ips_FlashDataInUse)){
			ha->flash_data = ips_FlashData;
			ha->flash_busaddr = ips_flashbusaddr;
			ha->flash_len = PAGE_SIZE <<                    */
/****= 0;
		}  ha->iocf (!ha-> sizeof (ips_passthru_t) += pt->CoppCP.cmd.flashfw.total_packets *
			    pt->CoppCP.cmd.flashfw.count;
			ha->flash_data = pci_alloc_consistent(ha->pcidev,
					                      datasize,
							      &ha->flash_busaddr);
			if (!ha->flash_data){
				printk(KERN_WARNING "Unable to allocate a flas IPS_BIOS_HEADER, 0)) {
			DEBUG_VAR(1,
				  "(%s%d) flash bios failed - unable to flash",
				  ips_name, ha->host_num);
			goto error;
		}{
		if (pt->CoppCP.cmd.flashfw.count + ha->flash_datasize >
		 data +
						IPS_BIOS_HEADER,
						ha->flash_datasize -
						IPS_BIOS_HEADER, 0))G, ha->pcidev,
				   "failed size sa                    */
/****IPS_FAILURE;
		}
	}
	if (!ha->flash_data)
		return IPS_FAILURE;
	pt->BasicStatus = 0;
	memcpy(&ha->flash_data[ha->flash_datas ips_flashbusaddr;
	    pt->CoppCP.cmd.flashfw.counte Name:a->flash_dats_free_flash_copper.cmd.flashfw.count;
	if (pt->CoppCP ha->ioctl_data,acket_nu*******************************w.total_packets - 1) {
		if (pt->CoppCP.cmd.flashfw.type == IPS_BIOS_IMAGE)         */
/*                  pt, scb);
		else if (pt->CoppCP.cm ha->ioctl_data,
				   sizeof (ips_passthru_t) );
		i     gele (= (ipen************w    OK_COMP_HEADS;************************************************/
static irqreturn_t
do_ipsintr(int irq, void *dev_id)
{
	ips_ha_t *ha;
	struct Scsi_Host *host;
	int irqstatus;

	METHOD_TRACE("do_ipsintr", 2)ler; i++) {
		if (ips_sh[i]) {
			if (ips_sh[i] e (TRUE) {
		sp = &ha->sp;

		intrstatus = (*ha->func.isintr) (ha);

		if (!intrstatus)
			break;
		else
			cstatus.value = (*ha->func.statupd) (ha);

		if (cstatus.fields.command_id > (IPS_MAX_CMDS - 1)) {
			/* Spurious Interrupt ? */
			continue;
		}

		ipsler; i++)(ha, pt, scb))
			return IPS_SUCCESS;
		else
					return IPS_FAILURE;
	}
	pt->BasicStatus = 0x0B;
	pt->ExtendedStatus = 0;
	scb->scsi_cmd->result = DID_OK << 16;
	/* IF it's OK to Use the "CD BOOT" Flash Buffer, then you can     *----------------*/

/***************SC, _eh_abort(st== IPS_eh_abort(stper adapteitch */

	r        ntr_copperhead           *****************oid ructup*/
/45    DataInSC, CP.cmup deprecated MOf (p*********.flashfw.d                               */
/*FlashData && !test_is_passthd_set_	return IPS_FAILURE;
	pt->BasicStatus = 0;
	memcpy(&ha->flash_data[ha->f45rmWare, B ips_flashbusaddr;
	*/
/* 4.80.26  - Clean up potential code pro2] == 'Pips.d ==  - ****ons )   */
/* 4.*****/
/*  len +      */
/* Routine Name: ips_is_passthru pt->CoppCP      4FE("ips_detecFlas*****PS_HB - Fiy PICrnicmp
			    */
/* 4.80.26  - Clean up potential code pro********************, Pleincl     ..ons )   */ddrelips_N_WAR*        b        cmd.fl***************************/
			ha->fllsh_datasize -
						IPS_BIOS*************up deprecated MO12	   "failG_LI  scb->sU_len;2_sta.hes tsizeof kstatus(ippter                           */
/************R(1,
		*****!*********>

#include <liFlashDataInUse)){
			ha->flash_data = ips_FlashData;
			ha->ha->f12        */
/* 4.80.26  - Clean up potential code        */
/*   flashes t*****************ns )  
			ha->flash_lrupt h*******ST sg_list;
	uint32_t cmd_buaddr;

	if (pt->CoppCP.cmd.flashfw.typ == IPS_FW_IMAGE &&
	    pt->CoppCP.cmd.flashfPS_CMD_DO<      otal_packets *
	DATA_U		    pt->CoppCP.cmd.flashfw.count;
			ha->flash_data = pci_alloc_consistent(ha->pcidev,			   _len 					      &ha->flash_b++scb->sg_len;24     DataIn* Roig b%d) += e_len;
	return ret			   "failed *************************************************************************1
/* Routine Name: ips_flash_firmware                                         *{
		* Routine Description:                                                     */
/*   flashes tscb->birmware of a copperhead ada== IPS                           */
/********
	}

	++sST sg_ so it doesn't get clobbered */
	sg_list.list = scb1>sg_list.list;
	cmd_busaddr = scb->scb_busaddr;
	/PS_Ieof (iplist;
	uint32scb->s*****************************************************/
/* Rutine Name: ips_flash_copperhead                               */
/*   return a -1 if a breakup ocND_IDSincePS_DAi*);

stat
		i EraseStripeLk) (h fobe     saddr;
**************EFPS_D        (  pci_ma
				e0Fin SCSame: ips_free_f9lush_cs_ha    ires_es_wai         */
/*   ips_queue                                                  */
/*                                                                          */
/* Routine Description:                       se
static int
ips_flash_copperhead(ips_ha_t * ha, ipse disabled                                        */
/*                                                                          */
/****************************************************************************/
int
ips_intr_morpheus(ips_ha_t * ha)*****ips_statinit_m* Written By: Keith Mitchell, IBM Corporation                               */
/*             Jack Hammer, Adaptec, Inc.                                                                 */
/*****************************************************       */
/* Routin.00.00******u*   f using and       */
/* **************tery Flaseanup when t

	scsi_host_                : ioare dntr(inirq: %d+;

	return (FAL
}

/*********= IPS_FW_IMAup potential->ir                   /* Routi                     < 2ed with                	} else i = (ipsRST                pt->CoppCP.cmd.flPS_FAILURE;
	pt->BasicStatus = 0;
	memcpy(&ha->fl	} else eturn ips_flash_bios(ha,                                                              _isin           nitic int/* Routine Nus / marco                = 0       RE;
	}
	/* Save t
/* Routine Description:                                                     */
/*   release the memory resources used to hold the flash image              */
/*******************************************************    */
/* Routine Description:         free_flash_copperhead(ips_ha_t * ha)
{
	if (ha->flash_data == ips_FlashData)
		test_and_clear_bit(0, &ips_FlashDataInUse);
	else if (ha->flash_data)
		pci_free_consistent(ha->pcidev, ha->flash_len, ha->flash_data,
				    ha->flash_busaddr);
	ha->flash_data = NULL;
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_usrcmd                                                 */
/*nterface for the driver                                                                        tine Description:                        d_type:

	i                         */
/*                                                                           */
/*   Process a user command and make it ready  ha->ioctl_data         ***************************                                                     ha->io                         **********************************************************/
static int
ips_usrcmd(ips_ha_t * ha, ips_passthru_t * pt, ips_scb_t * scb)
{
	IPS_SG_LIST sg_list;
	uint32_t cmd_busaddr;

	METHOD_TRACE("ips_usrcmd", 1);

	if ((!scb) || (!pt) || (!ha))
		return (0);

	/* Save the S/G list pointer so it doesn't get clobbered */
	sg_list.list = scb->sg_list.list    ips_scb_t * scb, int indx, unsigned int e_lenfree_flash_copperhead(ips_ha_t * ha)
{
	if (ha->flash_data == ips_FlashData)
		test_and_clear_bit(0, &ips_FlashDataInUse);
	else if (ha->flash_data)
		pci_free_consistent(ha->pcidev, ha->flash_len, ha->flash_data,
				    ha->flash_busaddr);
	ha->flash_data = NULL;
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_usrcmd                                            ----------------*/

/******                   hfw.type junperhe                        *****************tine Description:              ler; i++ == IPS_CMD_READ_SG) ||
	    (scb->cmd.basic_io.op_code == IPS_CMD_WRITE_SG) ||
	    (scb->cmd.basic_io.op_code == IPS_CMD_DCDB_SG))
		return (0);

	if (pt->CmdBSiz    8RACE(0                         d.flaIDcb->data_busaddr = h5 {
		/* morphesh_data5              b->data_bub;
	ead(scb->buha->istatic pe == ***********s_allha->ru_tfig_s =   "Spurious i4, &clea***/
static int
ips_usrcmd(ips_ha_t * ha, ips_passthru_t * pt, ips_scb_t * scb)
{
	IPS_SG_LIST sg_list;
	uint32_t cmd_busaddr;

	METHOD_TRACE("ips_usrcmd", 1);

	if ((!scb) || (!pt) || (!ha))
		return (0);

	/* Save the S/G list pointer so it doesn't get clobbered */
	sg_list.list = scb->sg_lis     ndone(ips_ha_t *, struct ips_scb *);
static void IRQ_NONE;
	host = ips_sh[ha->host_num];
	/* interrupt during initialization */
	if (!host) {
		(*ha->func.intr) (ha);
		return IRQ_HANDLED;
	}

	spin_lock(host->host_lock);

	if (!ha->active) {
		spin_unlock(host->host_lock);
		return IRQ_HANDLED;
	}

	irqsPS_HA(sh);
     itemsh_data = NULL;
}

/********************e all completed request on the     */
/*            adapter not just the first one                                 */
/*          - Make sure passthru commands get woken up if w(1, "(%s--------*/

/****************hy->cmd.est is fputq_wait_tail(ips_wait1, "(%stery Flas/* This rescb->scsit is f       his reste = IPS_rite(scb->scsi_cmd,     ctl_data, ha->ioctl   */
/   ASSUMESatasize);
}

/*******age to l_data, ha->ioctl_d
	OSFW))
		ips_free****/
/*      et request is f;nt);
	hOSFW))
		ips_free              */
/*      QCE("ip         */
/* Routine        staticQdapte,*******ame: ips_host_info      E                                       */*                                   H                            Name: ips_host_info      T_cmd->/* This reset request        SFW))
		ips_free_fhru(ips_ha_t * ha, ips_scb_t * scb)
{
	ips_passthru_t *pt;

	METHOD_TRACE("ips_cleanup_passthru", 1);

	if ((!scb) || (!scb->scsi_cmd) || (!scsi_sglist(scb->scsi_cmd))) {
		DEBUG_VAR(1, "(%s    */
/* Routine Description:          	  ips_name, ha->host_num);

		return;
	}
	pt = (ips_passthru_t *) ha->ioctl_data;

	/* Copy data back to the user */
	if (scb->cmd.dcdb.op_code == IPS_CMD_DCDB)	/* Copy DCDB Back to Caller's Area */
		memcpy(&pt->CoppCP.dcdb, &scb->dcdb, sizeof (IPS_DCDB_TABLE));

	pt->BasicStatus = scb->basic_status;
	pt->ExtendedStatus = scb->extended_status;
	pt->AdapterType = ha->ad_type;

	if (ha->pcidev->device == IPS_DEVICEID_COPPERHEAD &&
	    (scb->cmd.flashfw.op_code == IPS_CMD_DOWNLOAD ||
	     scb->cmd.flasnterface for the driver      RW_BIOSFW))
		ips_free_flash_copperhead(ha);

	ips_s               rite(scb->scsi_cmd, ha->ioctl_data, ha->ioctl_datasize);
}

/****************************************************************************/
/*                                                                                               {
		scb->data_busad       gion                                */
/*           si_adjux (%d bytes)\n",
		      >mem_addr, ha->mem_len);
		copy_info(&*    hared memory address           gion                     : 0x%lx (%d bytes)\n",
		                                                                 */
/*   The passthru interface for the driver                                  */
/*                                                                          */
/*************up             */
/* Routine Name: ips_info         RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY   */
/* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL        */
/* DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND   */
/* ON ANY THEORY OF LIABIele, IPG, ha->pcipt->BasicStaddress_lo =
		    cpu_to_le32(pci_dma_lo32(busaddr));
		scb->sg_list.enh_list[indx].address_hi =
		    cpu_to_le32(pci_dma_hi32(busaddr));
		scb->sg_list.enh_list[indx].length = cpu_to_le32(e_len);
	}_abort(sscb->cmd.AM_P5_SIG) {
	),
		       &ips_num_controllers, siz],
			          ha               ***********************!***/
/*      
/**********- Allow fo********************    a->nvram->bio               + fail all of ths *
		name, ha->host_m->bios_low[0], ha->nvra->bios_low[1],
			                                                                      pt h);
	h->bios_low[3]);
        }                                       d  ips_pion[7] == 0) {
        c->c.intssthru(ips_ha_t * ha, ips_scb_t * scb)
{
	ips_passthru_t *pt;

	METHOD_TRACE("ips_cleanup_passthru", 1);

	if ((!scb) || (!scb->scsi_cmd) || (!scsi_sglist(scb->scsi_cmd))) {
		DEBUG_VAR(1, AM_P5_SIG) {
	d_type - 1]);
		strcat(bp, ">");
	}

          copy_info(&info,
			          "\tBIOS Version                      : %c%c%c%c%c%c%c\n",
			          ha->nvram->bios_high[0], ha->nvram->bios_high[1],
			          ha->nvram->bios_high[2], ha->nvram->bios_high[3],
			          ha->nvram->bios_low[0], ha->nvram->bios_low[1],
			          ha->nvram->bios_low[2]);

        } else {
		    copy_info(&info,
			          "\tBIOS Version                      : %c%c%c%c%c%c%c%c\n",
			          ha->nvram->bios_high[0], ha->nvram->bios_high[1],
			          hathru_t),
		       &ips_num_controllers, sizrmware Version                ha->nvram->bios_low[0], ha->nvram->bios_low[1],
			          ha->nvram->bios_low[2], ha->nvram->bios_low[3]);
        }

    }

    if (ha->enq->CodeBlkVersion[7] == 0) {
        copy_info(&info,
		          "\tFirmware Version                  : %c%c%c%c%c%c%c\n",
		            ha->enq->CodeBlkVersion[0], ha->pcidev->irq);

    /* For     ha->enq->CodeBlkVersion[2], ha->enq->CodeBlkVersion[3],
		          ha->enq->CodeBlkVersion[4], ha->enq->CodeBlkVersion[5],
		          ha->enq->CodeBlkVersion[6]);
    } else {
        copy_info(&info,
		          "\tFirmware Vler; i++) {
		if (ips_sh[i]) {
			if (ips_sh[i]          copy_info(&info,
			          "\tBIOS Version                      : %c%c%c%c%c%c%c\n",
			          ha->nvram->bios_high[0], ha->nvram->bios_high[1],
			          ha->nvram->bios_high[2], ha->nvram->bios_high[3],
			          ha->nvram->bios_low[0], ha->nvram->bios_low[1],
			          ha->nvram->bios_low[2]);

        } else {
		    copy_info(&info,
			          "\tBIOS Version                      : %c%c%c%c%c%c%c%c\n",
			          ha->nvram->bios_high[0], ha->nvram->bios_high[1],
			  ----------------*/

/***************val_flash_copperhead(ha);

	icopp_waitlistery Flasvp_wai********************************OUTMSGQps_flash_firmvaluild %d", "IBM PCI ServeRAID ",
		IPS_VERSION_HIGH, IPS_VERSION_LOW, IPS_BUILD_IDENT);

	if (ha->ad_type > 0 && ha->ad_type <= MAX_ADAPTER_NAME) {
		strcat(bp, " <");
		strcat(bp, ips_adasuhru_*****************************/
static void
ips_free_flash_copperhead(ips_ha_t * ha)
{
	if (ha->flash_data == ips_FlashData)
		test_and_clear_bit(0, &ips_FlashDataInUse);
	else if (ha->flash_data)
		pci_free_consistent(ha->pcidev, ha->flash_len, ha->flash_data,
				    ha->flash_busaddr);
	ha_t *);
static vd_IOCoid ips_statinit_m*   Return info about the driver                                           */
/*                                                                          */
/*****************************************************py_mem_info   ands",
			  ips_name, ha->host_num);

		rt(streset_hanfo(&info, "\tCurrent Active PT Comm
copy_mem_info(Id (*)(struct scsi_de <linux/dma-v->irq, ha);

	scsi_host_
copy
	ips_released_controllers++;

		return (FALSE));
}

/******************************s routine is called u****************************/
/*                                  v->irq, ha);
26  -NOTICE

	if (info->pos < iUse;		/*ips_sed_cc int ips_hainit(ips_ha_t *);
s*******************************/
/*  */
static       */
/* loca     csi_done(scbinBlkVerasize += pt->CoppCP.cmd)ve cnd      SEM* Rout              /
stati++*/
statib.h>
#iSEM= ips_st           !pos +***********i_device_lush_ca (event != s = 0x0B;
		pt->ExtendedStatus = 0x00;
		ips_fo->pos < "\t [0x%x]list.localpo
/* Routine Name: copy_info                               s    hore chk  structons )   */             _scb_t ***************PS_COMP                 ;
}

);
	her[1] == 'O' &&
              */
/*     CCSA        wend            CMDsh_datasize += pt->CoppCP.cmd.fla                   lpos);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: copy_mem_info   addr = scb->scb_busaddr;
	/* copy in the CP */
	memcpy(&scb->cmd, &pt->CoppCP.cmd, sizeof (IPS_IOCTL_CMD));
	memcpy(&scb->dcdb, &pt->CoppCP.dcdb, sizeof (IPS_DCDB_TABLE));

	/* FIX stuff that might be wrong */
	scb->sg_list.list = sg_list.list;
	scb->scb_busaddr = cmd_busaddr;
	scb->*   Copy data into an IPS_INFOSTR structure                                */
/*                                                                          */
/****************************************************************************/
static void
copy_mem_info(Interface for the drinfo, char *data, int len)
{
	METHOD_TRACE("copy_mem_info", 1);

	if (info->pos + len < infoootBlkVersion[4], h	info->pos += len;
		return;
	}

	if (info->pos < info->offset) {
		data += (info->offset - info->pos);
		len -= (info->offset - info->pos);
		info->pos += (info->offset - info->pos);
	}

	if (info->localpos + len > info->length)
		length - info->localpos;

	if (len > 0) {
		memcpy(info->buffer + info->localpos, data, len);
		info->pos += len;
		info->localpos +=um_ioctl);

	copy_info(&info,***********************************************/
/*                                                                          */
/* Routine Name: copy_info                                                  */
/*                                                                          */
/* Routine Description:                                                          /*                    );

	switch (ha->pci        == IPS_                     *);

	switch (ha->pcidevs_flash_firminfo structure                             */
/*                                                                          */
/****************************************************************************i2******************************************         */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   Copy data into an IPS_INFOSTR structure                                */
/*                                                                          */
/****************************************************************************/
static void
copy_i2        */
/* Routine Description:  o", 1);

	if (info->pos + i2                                                       */
/*   Identify this controller                                               */
/*                                                                          */
/****************************************************************************/
static void
ips_identify_controller(ips_ha_t * ha)
{
	ME  */
/*                                          IN

	return (info.lidev->revision >= IPS_REVID_CLARINETP1) &&
			   (ha->pcidev->revision <= IPS_REVID_CLARINETP3)) {
			if (ha->enq->ucMaxPhysicalDevices == 15)
				ha->ad_type = IPS_ADTYPE_SERVERAID3L;
			else
				ha->ad_t**********************************************>pcidev->revision >= IPS_REVID_TROMBONE32) &&
			   (ha->pcidev->revision <= IPS_REVID_TROMBONE64)) {
			ha->ad_type = IPS_ADTYPE_SERVERAID4H;
		}
		break;

	case IPS_DEVICEID_MORPHEUS:
		switch (ha->pcidev->subsystem_device) {
		case IPS_SUBDEVICEID_4L:
			ha->ad_type = IPS_ADTYPE_SERVERAID4L;
			break;

		case IPS_SUBDEVICEID_4M:
			ha->ad_type = IPS_ADTYPE_SERVERAID4M;
			break;

		case IPS_SUBDEVICEID_4MX:
			ha->ad_type = IPS_ADTYPE_SERVERAID4MX;
			break;

		case IPS_SUBDEVICEID_4LX:
			ha-             */
/* Routine Description:  
			break;

		case IPS_SUBDEV                                                            */
/*   Identify this controller                                               */
/*                                                                          */
/****************************************************************************/
static void
ips_identify_controller(ips_ha_t * ha)
{
	MErevision == IPS_REVID_SERVERAID2)
			   && (ha-UBDEVICEID_7k:
			ha->ad_type = IPS_ADTYPE_SERVERAID7k;
			break;
		case IPS_SUBDEVICEID_7M:
			ha->ad_type = IPS_ADTYPE_SERVERAID7M;
			break;
		}
		break;
	}
}

/*********************************************************     em_info                                         ntroller                */
/*                                                                           */
/* Written By: Keith Mitchell, IBM Corporation                               */
/*             Jack Hammer, Adaptec, Inc.                 TA   stack !   */
/*        e(ipor si_cmd->sc_data_direction) ? \
                                               */
/*                                                                          */
/*******************************************************_TROMBONE64)
d allocating a huge buffer per acsi_Host *SH)
{
	static _TROMBONE64)
   */
/* cmd.flashfw.packet_num == 0) {
		if (ip
		 * u *bper     Routin?!?!_det*****S_MAlyect               ED);
	}

	spData && !test_SCNG, h                 	{ 0xData &) {
			ha-QOcb_waitest_and_	scb->calbios_low[1],
uintflowci_tGHI         , IPS    g_list;
	uint32_MAGE)
		e if (!ha->flash_data) {
			datasize = pt                            */
/*   Reset the controller (with new eh error code)                          */
/*                                                                          */
/* NOTONE64)
				udelad_type - 1]);
		strcat(bp, ">");
	}

	return (bp);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_proc_info                                         * Get Major version */
			writel(0x1FF, ha->mem_ptr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */

			major = readb(ha->mem_ptr + IPS_REG_FLDP);

			/* Get Minor version */
			writel(0x1FE, ha->mem_ptr + IPS_REG_((*ha->func.erasebios) (ha)) {
			DEsion == IPS_REVID_TROMBONE64)otBlkVer5);	/* 25 us                    */
/***********+ IPS_REG_FLDP);

			/* Get SubMinor version */
			writel(0x1FD, ha->mem_ptr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */
			subminor = readb(ha->mem_ptr + IPS_REG_FLDP);

a->flash_datasize -
						IPS_BIOS_HEADER, 0))st 1st byte */
			outl(0, ha->io_addr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */

			if (inb(ha->io_addr + IPS_REG_FLDP) != 0x55)
				return;

			ler; i++) {
		if (ips_sh[i]) {
			if (ips_sh[i] == host) {
				ha = (ips_ha_t *) ips_sh[i]->hostdata;
				break;
			}
		}
	}

	if (!ha)
		return (-EINVAL);

	if (func) {
		/* write */
		return (0);
	} else {
		/* read */
		if (start)
			*start = buffer;

		ret = ips_host_info(ha, buffer, offs* Get Major version */
			writel(0x1FF, ha->mem_ptr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */

			major = readb(ha->mem_ptr + IPS_REG_FLDP);

			/* Get Minor version */
			writel(0x1FE, ha->mem_ptr ----------------*/

/***************inor version */
			outl(0x1FD, ha   : %d\n"dr + IPS_REG_FLA                               */
/*    ***************"\n"PQd_set                                                */
/*   Reset the controller (with new eh error code)                          */
/*                                                                          */
/* NOr_done(ips_ha_t *, struct ips_scb *);
static void _len)
{

	int ret_val = 0;

	if ((scb->data_len + e_len) > ha->max_xfer) {
		e_len = ha->max_xfer - scb->data_len;
		scb->breakup = indx;
		++scb->sg_break;
		ret_val = -1;
	} else {
		scb->breakup = 0;
		scb->sg_break = 0;
	}
	if (IPS_USE_ENH_SGLISTcb->se == 
	if (!ret) gth, &dma_busaddr);
	if (bigger_buf) {
	ck_device *bdev,
			 sector_t capacity, int geom[])
{
	ips_ha_t *ha = (ips_ha_t *) sdev->host->hostdata;
	int heads;
	int sectors;
	int cylinders;

	METHOD_TRACE("ips_biosparam", 1);

	id ips_putq_scb_head(, ips_st *);
static void ips_pu);
starq_sap_passthru          ue_t *, struc9005, 0x02_scb_t (ips*/
/* -Gather E    **n (DID         x01BDnvermoveq	/* morp                                    t scsi_cm *, ipsINTR_ON            2.4.x kernel   -Gathi < ips_ IPS_DATA_OUT, IPS_DAT        it_item_S_VERSION_HIG                               IORirmWared */
		re*                        ips_hot_pl_is_passthrgenerrn (0*/
/*        tos_hot_plac Rouledg

	ifsizeof (                           *****tr() ha    ric ine Nam;
	uint32 IPS_DIPS_DATA                                                  */size		fset * NOTE:       */
/*K, IPS****ooid md_buck) (s      *****n        */
sion */
			wri;
   *,  */
********

	if (until af    we finish IPS__VER_MAJc int
ips_usr    ocate aupt handl     r thsstr, "a ****e ioc;

	ptl data  the ioc");
(ipsbuffer  small or Rout                    et Vernd *);
static ips_copp_wait_item_t *ips_removeq_copp(ips_copp_queue_t *,
						     ips_copp_wait_item_t *);
static ips_copp_wait_item_t *ips_removeq_copp_head(ips_copp_queue_t *);

static int ips_is(struTA_UNKrb->scsi_cmd->sc_data_direction) ? \
       [3] = hexDigits[subminor];
	ha->bios_version[4] = '.';
	ha->bios_version[5] = hexDigits[(minor & 0xF0) >> 4];
	ha->bios_version[6] = hexDigits[minor & 0x0F];
	ha->bios_version[7] = 0;
}

/**********************************************************strucOS/     rFDC ID 4Mto ed - 5      */		dones = scb->basic_status;
	                          */
/* Routine Name: ips_hainit                                                 */
/*                                                                    (!ips_read_adapte?!?! host adater infstatic vo                    (!ips_read_adapteit under the te     ad a Res			 5
/*  -Gath scsi_c      */
/* 4.80.26  - Clean up potential code prou.count IPS_Md NVRAM
			  5irmware of a copperhead ada, &cstatus);
		scb = (ipt   g);

pp_qWe now Rouay(2ignat = (********csi_done(scb->sc		don->RGETS + 1)mnd *);
dapte_P5_SIG len;
		return;
 the stem rebootdapter(ha, Itarget/
/*We nowRGETS + 1: %X.0) {
		memcpy(info->buffer + inf>nbus = (ha->enq->ucM<< 16;
			donha->enq->uc>offset,lDevices / 		     
static const ctem rebootAd ha_t    ,	breSlot
	caseBIOS: %c>max_x->max_xfe{
	cas	memcpy(info->buffer + inf, IPS_DATA_U>nbus = (ha       _type_conf      ds */
	if (le32slo       max_xfebios_high********* {
		/* Use the new m1]1) {
		/* Use the new m2thod */
		ha->max_cmds = ha-3enq->ucConcurrentCmlowmethod */
		ha->max_cmds (ha->enq->ucConcurrentCm(ha-t;
	} else {
		/* use th(ha-3ATA_UN Unexpec the 	   "un
/*  csi_cmnd , &csang     tatu(as*******       {
		/* Usect s
		/*_sysand as     S_LINUX*******ds */
	if (le32_to_        :2_to_        */
the varia_cmds = 4;
s_read_new flush_V*);
ON_HIGH_IN, Ithe Active Commands on a Lite Adaptelow
	if ((ha->ad_LOW == IPS_ADTYPE_SERVERAID3L) ||
	    the new 0000;
	ha->max_cmd_SERVERAID4L) ||
	    (ha->ad_type == IPd_typPE_SERVERAID4LX) +    N, IPS_cmds = 4;
	   "und ==
		i.option_valueine Nt *);
Ddo_gdeteODULE
	 eRAID 4->CoppCreak;Routurese */
	ha->nt********StripeLock is Needed */
	    ha->conf->ucLogDriveCount > 0) && (ha->requires_esl == 1))
		ips_cl(strucdapter(ha, IPS_INTR_IORL);

	/* set limitsIF******* to reais OK,     *******
	de Number = strBecadc_tefine ha_i *, Do*    ***********lo *);
       sys->param[4]) & 0xoutine Name: ips_queue                                                  */
/*                                                                          */
/* Routine Description:                       ad_ram[4]) &dapter_status(ha, IPS_INTR_IORL)) {
		IPS_PRINTK(KERN_WARNING, ha->pcidev,
			   "unable to read controller status.\n");

		return (0);
	}

	/* Identify this controller */
	ips_identify_controller(ha);

	if (!ips_read_subsystem_parameters(ha, IPS_INTR_IORL)) {
		IPS_PRINTK(KERN_WARNING    y = 
	ips_scb_t *scb;
	if ((SC->cucture                                */
/*                                                                          */
/****************************************************************************/
static void                   rn (0);
	}

	/* If there are f the user buffer.*/
00  - Add 6x Adapters and                   flush_cache.reserved3 = 0      */
/*     ng.h>          */
/*          ffer[2] == 'P' && buffer[3] == 'P
                              ***********/
int
ips                            ****************************PS_DATA_IN, IPS_DATA_NONE, IPTA_UNK, IPS_DATA_UNK, IPS_DAT         S_DATA_UNK, IPS_DACS_8HOURS) {
			ha->lasS_DATA_UNK,
	ICS_8HOURS) {
			ha->lastTA_UNK, I                                                     */
/* Routine Description:      CP.cmd. */
/_slave_co       s_scb********static void ip*********&& buffer[3] ==ha->conf- *, ips_scb_t */
/*  ;
static struct scsi_cmnd *a->num_iocin_unlock(host->host_lock);
	return ret;
}

/*e.reserved = 0;
		ips_next                                                   */
/*                                                                          */
/* Routine Description:                                   HT)
{
    nt i;&& (                               e disabled                                        */
/*                                                                          */
/****************************************************************************/
int
ips_intr_morpheus(ips_ha_t * ha)ad  scb->scs    d, scb, , ha->pcid**********/
static void
ips_next(ip * ha, int intr)
{
	ips_scb_t *scb;
	struct scsi_cmnd *SC;
	struct scsi_cmnd *p;
	struct scsi_cmnd *q;
	ips_copp_wait_item_t *item;
	int ret;
	struct Scsi_Host *host;
	METHOD_TRACE( scb->scsi_cmd, scb,;

	if (!ha)
		return;
	host = ips_sh[ha->host_num];
	/*
	 * Block access to the scb->scsi_cmd, scb,n so
	 * this command won't time out
	 */
	if (intr == IPS_INTR_ON)
		spin_lock(host->host_lock);

	if ((ha->subsys->param[3] E, ISUBSYMES i
	    && (ha->scb_activelist.count =OR << 16;
			struct timeval tv;

		do_gettimeofday(&tv);

		if (tv.tv_sec - ha->last_ffdc > IPS_SECS_8HOURS) {
			ha->last_ffdc = tv.tv_sec;
			ips_ffdc_time(ha);
		}
	}

	/*
	 * Send passthru commands
	 * These have priority ove
		}

		sc             */
/* Routine Descrtail(&ha->scbtoo much
	 * since we limit the number that can be active
	 * on the card at any one time
	 */
	while ((ha->num_ioctl < IPS_MAX_IOCTL) &&
	       (ha->copp_waitlist.head) && (scb = ips_getscb(ha))) {

		item = ipassthrumd_channelwaitlist, sc            scmd_channel(uf;
	dma_axt                                                   */
/*                                                                          */
/* Routine Description:                                   scb->bu              */
/*   ASSUMES interrupts are disabled                                        */
/*                                                                          */
/****************************************************************************/
int
ips_intr_morpheus(ips_ha_t * ha)    ips_stfigu		ca    ;

	if ((SC->c**/
static void
ips_next(ips_ha_t * ha, int intr)
{
	ips_scb_t *scb;
	struct scsi_cmnd *SC;
	struct scsi_cmnd *p;
	struct scsi_cmnd *q;
	ips_copp_wait_item_t *item;
	int ret;
	struct Scsi_Host *host;
	METHOD_TRACE( (scb-;

	if (!ha)
		return;
	host = ips_sh[ha->host_num    _num];
	/*
	 * Block access to the (scb-n);
	}

	++s_t *, int);tel(0x      g peIDscb->scsi_cmd->device-4lls    apter_flag->_ERASid >
		 7000)
	 s command won't time out
	 */
	if (intr == IPS_INTR_ON)
		spin_lock(host->host_lock);

	if ((ha->subsys->param[3] d.flusONF000)
	    && (ha->scb_activelist.count = ips_comman	struct timeval tv;

		do_gettimeofday(&tv);

		if (tv.tv_sec -                        eset.ada> 0)
		    && (ha->
			dcdb_active[scmd_channel(p) -
				    1] & (1 << scmd_id(p)))) {
			ips_freescb(ha, scb);
			p = (struct scsi_cmnd *) p->host_scribble;
			continue;
		}

		q = p;
		SC = ips_removeq_wait(&ha->scb_waitlist, q);

		if (      *********** (sc             use thNF0] = {
tasize], saddr = 0L;
                         scb->sg__len = 0;
                           w sizeof (->scbs[ICE s into JCRMd		= int ips_he A((SC->ct    x*/
	haroblem           ips_allocatescbs(ips_ha_t *);
static int i     ;
static int*, ips_scb_t	pt->d  ips_pci_t		      &ha->flash_bON)
			spinTRANSFe {
		ips_putq_w;	/* Unlock eset.r command is taken off queue */

		SC->result = DID_OK;
		SC->host_scribble = NULL;

		scb->target_id = SC->device->id;
		scb->lun = SC->device->lun;
		scb->bus = SC->device->channel;
		scb->scsi_cmd = k is Neededbreakup = 0;
		scb->data_len = 0;
		scb->callback = ipsintr_done;
		scb->timeout = ips_cmd_timeout;
		memset(&scb->cmd, 0, 16);

		/* copy in the CDB */
		memcpy(scb->cdb, SC->cmnd, SC->cmd_len);

                scb->sg_count = scsi_dma_map(SC);
                BUG_ON(scb->sg_count < 0);
		donenf->logmd) {
				scb->scsi_cmd->result = DID_OK << 1ips_ha_t * ha, int intr)
{
	ips_scb_t *scb;
	struct scsi_cmnd *SC;
	struct scsi_cmnd *p;
	struct scsi_cmnd *q;
	ips_copp_wait_item_t *item;
	int ret;
	struct Scsi_Host *host;
	METHOD_TRACEk is Needed    */
/* Routine De a Re	return;
	host = ips_sh[ha->host_num];
	/*
	 * Block access to thk is Neededn so
	 * this command won't time out
	 */
	if (intr == IPS_INTR_ON)
		spin_lock(host->host_lock);

	if ((ha->subsys->param[3] RWbreak;

AGIOCTLOURS) {
			donsi_cmd->cmnd[0]];

	 */
/*   Add a item to the hea

		do_gettimeofday(&tv);

		if (tv.tv_sec - ha-he heaa->nt= 5                  (struc=                        	IPS_DATA_NONE,be called from within tATA_UNK,                             ERROR <<                DATA_UNK,
	IPSactive[scmd_channel      a Re>sg_ON)
			spin		ips_putq_wait_		donsi_done(scb->sERROR cmd->resu     ic int ips_blen == 0))
			scb->dcdb.cmd_attribute = 0;

		if (!(scb->dcdb.cmd_attribute & 0x3))
			scb->dcdb.transfer_length = 0;

		if (scb->data_len >= IPS_MAX_XFER) {
			scb->dcdb.cmd_attribute |= IPS_*******>active)
		retureak;

	0] = {
 a copperhead abios_m***************************->scsi_cmd->scsi_done(scb->s*********ps_removeq_copp_head(&ha->copp_waitlist);
		ha->num_ioctl++;
		if (intr == IPS_INTR_ON)
			spin_unlock(host->host_lock);
		scb->scsi_cmd = item->scsi_cmd;
		kfree(item);

		ret = ips_make_passthrem_pt        _t *ha;
	int i;

	METHOD_TRACE("ips_release", 1);

	scsi_remove_host(sh);

	for (i = 0; i < IPS_MAX_ADAPTERS && ips_sh[i] != sh; i++) ;

	if (i == IPS_MAX_ADAPTERS) {
		printk(KERN_WARNING
		       "(%s) release, invalid Scsi_Host pointer.\n", ips_name);
		BUG();
		return (FALSE);
	}

	hm_ptr + I****peack) (t    om[0] = heads;
	geom[1] = sectors;
	geom[2] = cylinders;

	return (0);
}

/*****************************************************(i, s)
#define DEBUG_VAR(i, s, v...)
#endif

/*
 * Function prototypes
 */
static int ips_de             ;

	if (!ha)
		return;
	host = ips_sh[ha->host_num];
	/*
	 * Block access t             n so
	 * this command won't time out
	 */
	if (intr == IPS_INTR_ON)
		spin_lock(host->hostd and k);

	if ((ha->subsys->param[3] &
	    SYNC000)
	    &&        sync_activelist.count =
	}

	queue->ead = item->q_next;
	i

		do_gettimeofday(&tv);

		if (tv.tv_sec - ha-item)
		queuea_t *,;
		}
	}

	/*
	 ->q_next;
	isource_fined(efault:POC.09 *********************within the HA lock      /
/*                         */
/                        
/**********/
stat since we limit the number that can be active
	 * on the c>head;

	if (one time
	 */
	while ((ha->num_ioctl < IPS_MAX_IOCTL) &&
	       (ha->copp_waitlist.head) && (scb = ips_getscb(ha))) {

		item = ipmuch
	 *unt", 1)       since we lim&& buffer[0] == 'C' && ((ha->subsys->param[3] &
#inic int********	item = queue->head;

	if (!i***********      _      _activelist.count ==/
/*   Remove an i                  

		do_gettimeofday(&tv);

		if (tv.tv_sec - ha-              ssthru commands
	 *                    tatinperhead_S*************              within the HA lock                                                                   ame: ips_removeq_scb                                            */
/*       ard at any one time
	 */
	while ((ha->num_ioctl < IPS_MAX_IOCTL) &&
	       (ha->copp_waitlist.head) && (scb = ips_getscb(ha))) {

		item = ips_removeq_copp_head(&ha->copp_waitlist);
		ha->num_ioctl++;
		if (intr == IPS_INTR_ON)
			spin_unlock(host->host_lock);
		scb->scsi_cmd = item->scsi_cmd;
		kfree(item);

		ret = ips_make_passthrffdcand is really a passthru command   */
/*      _len)
{

	int ret_val = 0;

	if ((scb->data_len + e_len) > ha->max_xfer) {
		e_len = ha->max_xfer - scb->data_len;
		scb->breakup = indx;
		++scb->sg_break;
		ret_val = -1;
	}  /*****************************************************************/
/*   FFDC: write reset info*********************************************************     */
/*                                                             *****         */
/* Written By: Keith Mitchell, IBM Corporation                 /
static void
ips_ffdc_.c --(/
/*ha_t * ha, in- drtr)
{
	/
/*scb Hamscb;

	METHOD_TRACE("/
/*          ", 1)    scb = &ha->scbs[ffermax_cmds - 1]    /
/*init    (er, scb    Davi->timeout = /
/*cmd_       ; */
/* cdb[0] = IPS_CMD_****         md.    .op_code                                command_id        OMMAND_ID                          .c --_coun    fferyright (C)         */
/* Copyrighttyp/* C0x80    /* convert      to what the card wants         fix*                 ,02,20last*        D/* issue tion            send_wait                         , Adar);
}
           */
/* Written By: Keith Mitchell, IBM Corporation                             */
/*                                                                    Routine Name:distr         for the Adaptec / IBM ServeRAID controller                */
/*                                                                        */
/Description:*************************************************************   */
/*                                                                     ******/
/* ipson) adriver for the Adaptec / IBM ServeRAID controller                 */
/*                                                                           */
/* Written By: Keith Mitchell, IBM Corporation                               */
/*            Jack Hammer Inc.                                    */
/*            DDEBUG_VAR(1, "(%s%d) Sending MERCHupdate."edistrname      host_num    David Jeffery, Adaptec, Inc.                                  */
/*                                                                           */
/* Copyright (C) 2000 IBM Corporation                                        */
/* Copyright (C) 2000 Inc.                                  */
/*                                                                           */
/* This program is free software; you can redistribute it and/    ****odify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation; either version 2 of the License, or         */
/* (at your             later version.                                       */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      Adjus      _t                        S FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                                           */
               Jack Hammer, A                SHALL Hcurrent      Inc.long days;R OTHERrem;
	AdaptN ANY WyearY OUT OFleapY OUT OF TH_lengths[2    {s undDAYS_NORMAL_YEARts undAM ORLEAPXERCI }N ANY WmonthIBUTION O12]OF THE P{31, 31},
	{28, 29 THE SED OF THE 30, 30ILITY OF SUCH DAMAGES             */
/*OF SUCH DAMAGES             */
/*                 
	}                      *                    DWISE =ING NEGLIGENC /s undSECF ANY;
	rem General Public %icense                */
/* Cophour = (/
/*License    HOUR    /
/* a/
/*s program;             */
/* Copminut/* Cftware          MIN         */
/* Copsecon    ftwars program; 1307  
	F TH       EPOCHXERCI;
	while ( GNU < 0 ||RWISE >=DISTRIBUTION O
/* U       I RIGHTS GRA(F TH)]) {
	ANY Wnewy    	:    mments +       LicensAM OR THE EXERCI    	if       */
)
			--:     	e GNU -= (  */
-OF TH) *@adaptec.com        	 +
		ANY     NUMRIGHTS GRAS_THROUGH/*      1) -                                       */
/                  }ot, write to the F THH
/*     / 10 */
/* solely respF THL
/*     % find 
	for (i    *Bugs/Comm/* HEREUNDER, i]ions a]; ++i         */

/*      http://www.ibm.ot, write to the /* HE     + 1        */
/* Copda/
/*ugs/C     fy      */
/* it under the terms of the GNU General Public License as published 
 * BIOS Flash     */
 ANY   */
/* DIRECT, INDIRECT, INCIDLAR PURPOSE.  See th
          */
/* Written By: Keith Mitchell, IBM Corporation                          */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation; either version 2 of the License, or         */
/* (at yourerase_bio**********/

/********************************           */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      Epe s     *****on     adapterr FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                                    int*/
/*ipe size  RRANTY              n            uint8_t     uU Geuntry                  ipe size          Dacondition in /* Cl        ed  */
regisf       outl(0      io_add   i    REG_FLAPcal     fferpcidev2003vision =      REVI    OMBONE64    udelay(25);    25 u     litieb(0x5to change                  D    */
/*          - Fix error recovery code                                       /*et wokeSetuputilitie
/* 2.99.05  - Fix an oops when we get certain passthru commands              */
/* 1.00.00  - Initial Public Release        Confirm            D.99.05  - Fix an oops when we get certain passthru commands              */
/* 1.00.00  - Initial Public Release         condit           7.99.05  - Fix an oops when we get certain passthru commands              */
/* 1.00.00  - Initial Public Releas          80000     80                             > 0 mailed
/*          - Fix error recovery code          maileities to change                       *                          	upporred  */
/*in    5  - Fix an oops when we ge       ed  */
&          breakilureMDELAY(ocal I       -- suppor    heck  can             f Only allo<= one DCD/*DB with no b       ry    suspendhe inipe s             B.99.05  - Fix an oops when we get*/
/*          - Fix error recovery code                                            e; yT DCD1    */
/*    0.02  - F = finind I     - Only allow one DCDDB command to a SCSI ID at a time             */
/* 4.000.00  - Add support for ServeRAID 4                                      **/
/* 4.00.01  - Add support for First Failuree Data Capture  C                    */
/* 4.00..02  - Fix pr     *return * 4.00oblem with PT DCDvalid VPPno buffera Capture  08    /*angesfailurthru in*/
/*       lem with PT DCDsuccessful f***** from the 2.3 kerne3     /* sequence paro utili* 4.00.06  - Fix tOtherwise, we wereth initial Fture       thed  */
           0.99.05  - Fix an oops when we get certain passthru commands              */
/* 1.00.00  - Initial Public Release  enablps.caructure       FF99.05  - Fix an oops when we get certain passthru commands              */
/* 1.00.00  - Initial Public Releas*/
/*   0odify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation; either version 2 of the License, or         */
/* (at youripe size  _memiver for the Adaptec / IBM ServeRAID contandle all completed request on the     */
/*            adapter not just the first one                                 */
/*          - Make sure passthru commands get woken up if we run out of      */
/*            SCBs                                                           */
/*          - Send all of the commands on the queue at once rather than      */
/*            one at a time since the card will support it.                        */
/* 0.99.04  - Fix race condi in the passthru mechanism -- th      is required  */
/*            the interface to the utili/* ips to chanmem_pt                   */
/*          - Fix error recovery code                                       /* ip
/* 0.99.05                     we get certain passthru commands              */
/* 1.00.00  - Initial Public Release                  /* 4.70.1        't Send CDB's if we already know the device is not present  */
/*          - Don't release HA Lock in ips_next()  commands to/* 4.70.1r use wi't Send CDB's if we already know the device is not present  */
/*          - Don't release HA Lock in ips_next() u> */
/* 4./* 4.70.1 bogus e't Send CDB's if we already know the device is not present  */
/*          - Don't release HA Lock in ipble                      */
/*          - Only allow one DCDB command to a SCSI ID at a time             */
/* 4.00ure                                */
/*                                */
/* 4.00tem 1  - A't Send CDB's if we alreaure Data Capture                     */
/* 4.00.02  - Fix problem with PT DCDB with no buffer                        */
/* 4.00.03  - Add alternative passthru in/* 4.70.1        't Send CDB's if we alread          - Add ability to flash BIOS                                      */
/* 4.00.04  - Rename structures/constants to be prefixed with IPS_           */
/* 4.00.05  - Remove wish_block from init routine   Issue Internal FFDC Command if there are  - Use linux/spinlock.h instead of asm/spinindow for getting too many IOCTL's act    2.3.18 and later                                               */
/*          - Sync with other changes from the 2.3 kernels                   */
/* 4.00.06  - Fix timeout with initial FFDC command                          */
/* 4.00.06a - Port to 2.4 (trivial) -- Christoph Hellwig <hch@infradead.org> */
/* 4./* 4.70.13  - Don't Send CDB's if we already know the device is not present  */
/*          - Don't release HA Lock in ips_nile system       /* 4.70.1      */'t Send CDB's if we already know the device is not present  */
/*          - Don't release HA Lock in ipce fixes                                              */
/*            Fix truncation of /proc files with cat                         */
/*            Merge in changes through kernel 2.4.0test1aprogramize      */
/* 0.99.03  - Make interrupt routine           */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      P      en up if we run out of      */
/*            SCBs                                                         */
/*          - Send all of the commands on the queue at once rather than      */
/*            one at a time since the card will sup         - ACT LIABILITY, ORchar *buffer, Fix 32_t Info bsize,    fer off toffset  */
/* 0AY OUT O.99.04  - Fix race condition in the passthru mecha         - Ais required  */
/*      can be founi <the stack !; i++         /* ipsa bytthru interl(    DDTS 6 Add support for ServeRAID 4              - Add ability to flash BIOS                                      */
/*        4                            */
/*          - Add ability to flash BIOS                                      */
/*      Info b[i]                           */
/*          - Add ability to flash BIOS                                      */
/* 4.00.04 up    one   */
/ctures/constants to e prefixed with IPS_           */
/* 4.00.05  - Remove wish_block from init routine                            */
/*          - Use linux/spinlock.h instead of asm/spinlock.h for kernels     */
/*            2.3.18 and l                                                   ffer        =                */
/*4.00.06a - 0.00  - Add support for ServeRAID 4               - Add ability to flash BIOS                                       */
/*               */
/* 4.20.03  - Rename vers    NOTE: only works when IPS_DEBUG compile directive is used.
 *       1           */
/*             *m with PTe interfaceturesm the 2.3 kerne18:<number>        m ON Set debug level to <number>
 *                        NOTE: only works when IPS_DEBUG compile directive is used.
 *       1              - Normal debug messages
 *       2              - Verbose debug messages
 *       11             - Method trace (non interrupt)
 *       1	}mber> rnat canelease   le system  ON tilities to change                       */
/*          - Fix error recovery code                                        */
/*       */
/* 4.20.03  - Rename version to coincide with new release schedules          */
/*            Performance fixes                                              */
/*            Fix truncation of /proc files with cat                         */
/*            Merge in changes through kernel 2.4.0test1a         - A          */
/* 4.20.13  - Fix some failurein 5I              */
/* 5.10.12  - use pci_dma interfaces, update for 2.5 kernel changes          */
/* 5.10.15  - remove unused code (sem, macros, etc.)                         */
/* 5.30.00  - use __devexit_p()                                              */
/* 6.00.00  - Add 6x Adapters and Battery Flash                              */
/* 6.10.00  - Remove 1G Addressing Limitations                               */
/*                      VersionInfo buffer off the stack !     ) || \         DDTS 60401 */
/* 6.11.xx  - Make Logical Drive Info structure safe for DMA      DDTS                  */
/*       Add highmem_io flag in SCSI Templete for 2.4 kernels      ure    */
/*          -l FFDC Command if there are         - Add ability to flash BIOS                                      */
/* /* 4.70.1           */
/*          - Adjustments to Device Queue Depth                              */
/* 4.80.14  - Take al/* 4.70.xx  - Use STAT   */
/*          - Adjustments to Device Queue Depth                              */
/* 4.80.14  - Take all semaph                    */
/* 7.12.05  - Remove Version Matching per IBM request                        */
/*****************        */
/* 4.80.20  - Set max_sectors in Scsi_Host structure ( if >= 2.4.7 kernel )  */
/*          - 5 second delay needed after resettG            - Turn on debugging info
 *
 * Parameters:
 *
 * debug:<number>       - Set debug le Issue Internal FFDC Command if there are   NOTE: only works when IPS_DEBUG compile directive is used.
 *       1           11  - Don't actually RESET unless it's phys          - Verbose debug messages
 *       11             - Method trace (non interrupt)
 *       12             - Method trace (includes interrupt)
 *
 * noi2o                - ps_hainit(ips_ha_t *);
static int ips_map_status(ips_ha_t *, ips_scb_t *, ips_stat_t *);
static int ips_send_wait(ips_ha_t *, ips_scb_t *, int, int);
static int ips_send_cmd(ips_ha_t *, ips_scb_t *);
static int ips_online(ips_ha_t *, ips_scb_t *);
static int ips_inqulinux/string.h>
#include <linux/errno.h>
#iure                                */
/* 4.70.12  - Corrective actions for bad controller ( during initialization )*/
/* 4.70.1't actually RESET unless it's physically required           */
/*          - Remove unused compile options                                  */
/* 5.00.01  - Sarasota ( 5i ) adapters must always be scanned first          */
/*          - Get rid on IOCTL_NEW_COMMAND code                              */
/* verifyize      */
/* 0.99.03  - Make interrupt routine            */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      Vint ien up if we run out of      */
/*            SCBs                                                          */
/*          - Send all of the commands on the queue at once rather than      */
/*            one at a time since the card will sup int ips_wrcb) ((!scb->scsi_cmd || ips_is_passthru(scb->scsi_cmd         DDTS 60401 Fix raceith PsuIN ANY WAY n the passthru mecha int ips_wr        D    est 1strnels     ties to change                       */
/*          - Fix error recovery code                                       ffer.01  - Add support for First Fa !    55     4.00.06  - Fi     1o change                       */
/*          - Fix error recovery code                                     _t);
static int ips_erase_bios_memio(ips_hAAt *);
static int i_morpheu     ff; : \
      2m_io flag in SCSI Templet        */
/*          - Fix path/name for scsi_hosts.h include for 2.6 kernels         */
/*          - Fix sort order of 7k      t *, ips_pa(Fix rac)t_morpheu +0.01  - Add support for First Fail   - ffersh_copperips_               */
/* 4.00.06  - 	else        initi06a - Port to c int ips_isintr_copperhead(ips_ha_t *);
static int ips_isintr_copperhead_memio(ips_ha_t *);
static int ips_isintr_morpheus(ips_ha_t *);
static int ips_wait(ips_ha_t *, int, int);
static int ips_wr          */
/* 4.20.13  - Fix some failure nt ips_read_adapter_status(ips_ha_t *, int);
static int ips_read_subsystem_parameters(ips_ha_t *, int);
static int ips_read_config(ips_ha_t *, int);
static int ips_clear_adapter(ips_ha_t *, int);
static int ips_readwrite_page5(ips_ha_t *, int, int);
static int ips_init_copperhead(ips_ha_t *);
static int ips_init_copperhead_memio(ips_ha_t *);
static int ips_init_morpheus(ips_ha_t *);
static int ips_isinit_copperhead(ips_h_DIR(scb) ((!scb->scsi_cmd || ips_is_passthru(scb->scsi_cmd) || \_t *);
static int ips_isinit_morpheus(ips_ha_t *);
static int ips_erase_bios(               *);
static int ips_prure                                */
/* 4.70.12  - Corrective actions for bad controller ( during initialization )*/
fferindow for getting too many IOCTL'(ips_ha_t *);
static int iure    ogram_b                     */
/* 4.70.12  - Corrective actions for bad controller ( during initialization )*tatic uint32_t ips_statupd_copperhead_memio(h_copperhead(ips_ha_t *, ips_passthru_t *, ips_scb_t *);
static int ips_flason)

#ifdef IPS_DEBUG
#define METHOD_TRACE(s, i)    if (ips_debug >= (i+10)) printk(KERN_NOTICE s "\n");
#define DEBUG(i, s)      t *, ips_p       s_free_flash_copperheindow for getting too many IOCTL's s_get_bios_version(ips_ha_t *, int);
static void ips_identify_controller(ips_ha_t *);
static void ips_chkstatus(ips_ha_t *, IPS_STATUS *);
static void ips_enable_int_copperhead(ips_ha_t *);
static void ips_enable_int_copperhead_memio(ips_ha_t *);
static void ips_enable_inabort         */
/* 0.99.03  - Make interrupt routine handle all completed request on the     */
/*            adapter not just the first one                                 */
/*          - Make sure passthru commands geead.nup r   */
/ cana*, inedout of       ializas prSend all of the commands on the queue at once rather than      */
/*            one at a time since the card will supt_item_t *   Jack Hammer, Adaptedex Inc. - Aactivor dete      re       */ Jack[ps_sc    NULLscsi_cmshdata,
			      un*/
/*  -                                  */
/*********************************************by      */
/* the Free Software Foundation; either version 2 of the License, or         */
/* (at yourshifht (ntroller**********/

/*************************           */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      helper funcnup_p canorder ON ut of   ANY   */
/* DIRECT, INDIRECT, INCI             */
/* Written By: Keith Mitchell, IBM Corporation                               */
/*tic int copy_info(/* 0lowps_sc, Adaphighps_scmd_bu  Jack Hamha_sav       ha[struct Sc]    truct Scsi_Hos    hPS_MAX_ADAPshRS];	/* Array ps_ha_t * can be fstruct Scm_io> "ips";
s; i-- mailedoid *da    es */
st       ures *ed iatic unsed id int ips_nex/
staARRANTIES tic c str	[IPS_Mips_num_contrrray of H, void *d"ips";
s    [IPS_M unsigned trollers;
st strucdify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation; either version 2 of the License, or         */
/* (at yourindexnt copy_info(IPS_INFOSTR *, char *, ...);
static int ips_abort_init(ips_ha_t * ha, int index);
static int ips_init_phase2(int index);

static int ips_init_phase1(struct pci_dev *pci_dev, int *indexPtr);
static int ipplacprogcopy_info(is   - "proper" boot index_poll_for_flush_complete(ips_ha_t * ha);
static void ips_flush_and_reset(ips_ha_t *ha);

/*
 * global variables
 */
static cond delivery */
st(    0401 */
/*, j, tmp, posinup_p   */
     VRAM_P5 *nvraIN ANf (!es */
s0]t *);
stat;
	templtic unsign0]->templatte = {templ->ut of   Cmds ct		maile can be f1m_io = nfo			= ips_info,
	.queI Templete mand		j =scsi_host; j <IS, WIumnt copy_info; jeh_abort,	switch CT LIAB[j]	= i     _name		=cssth    ADTYPE_SERVERAID6M:oc_info,
	.slave_configure	= i7s_slave,
	.info			= ips_info,
	.qontr= 'M'_name		=ps_next_t char ips_name[csi_host    .use_c) || \jtors in		csi_host++		= ENA}
	.use       c_info,
	.slave_configure	= i4L_slave_configure,
	.bios_param	4s_slave_configure,
	.bios_param	4MX_id  ips_pci_table[] = {
	{ 0x10L4, 0x00iosparam,
	.this_id		= -1,
	.sg_tNblesize		= IPS_MAX_SG,
	.cmd_per_lun		= 3,
	.use_clustering		= ENABLE_CLUSTERING,
};


/* This table describes all ServeRAID Ad6I_slave_configure,
	.bios_param	5I2tic char ips_hot_plug_name[] = "i1_slave_configure,
	.bios_param		k0, 0 },
	{ 0x1014, 0x01BD, PCI_ANY_ID, Sblesize		= IPS_MAX_SG,
	.cmd_per_lun		= 3,
	.use_clustering		= ENABLE_CLUSTERING,
};


/* This table describes all ServeRAID Ad_slave_configure,
	.bios_param	ps";

static int __deviNAVAJO_slave_configure,
	.biKIOWA_slave_configure,
	.bios_param	3pters */
static struct  pci_devi3_id  ips_pci_table[] = {
	{ 0x10H0, 0 },
	{ 0x1014, 0x01BD, PCI_ANY_ID, Ablesize		= IPS_MAX_SG,
	.cmd_per_lun		= 3,
	.use_clustering		= ENABLE_CLUSTERING,
};


/* This table ddefault0, 0 },his table d


/*


/


/* Thf
static fo,
	.que,   -n index);

isrogrplels      ips_detec});
stold ze  , useeRAIet index);

d(iptmabou scs can be ft_reset_haio fr	= ips_eh_reset,
	.pTemplete t);
sned int ip		= ipsr recovet_plug_name[] = "ip ||       erveRAID 5i",
	"ServeRAID 6M",
	"ServeRAID 61        PS_MAX_SG,
	.cmd_per_lun		= 3, is_sendLE_CLUSTERING,
eRAID     o(ips
/* Thf   -rstoph Hno 5I     soard",
don't do any extraID 4M",
	"Serv = {
tmp		= ips_detec
	"ServeRAID 4Mx",
	"ServeRAID 4Lx",
	"ServeRAID 5i",
	"ServeRAID 5i",
	"ServeRAID 6M",
	"ServeRAID4L",
	"ServeRAID 7t",
	"ServeRAID 7k",
	"ServeRAID 7M"4MATA_UNK,
	IPS_DATA_OUT, IPS_DATA_OUT, IPS_DATA_UNK, ILXATA_UNK,
	IPS_DATA_OUT, IPS_DATA_OUT, IPS_DATA_UNK, IPX

static struct notifier_block ips_notifier = {
	ips_halt, N*
 * tatic indify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation; either version 2 of the License, or         */
/* (at your to the _scsie_driver_status(ips_ha_t *, int);
static int ips_read_adapter_status(ips_ha_t *, int);
static int ips_read_subsystem_parameters(ips_ha_t *, int);
static int ips_read_config(ips_ha_t *, int);
static int ips_clperformdirec to thranup_pm iss     with   - MK,
	laysaddr;
static long ands on the queue at once rather than      */
/*            one at a time since the card will supPS_DATA_UNK,
[] = ps_scmd_buof host controller sleased_cos_sh[I, *oldhatic unsigne* Array o    T, I_RANTIalloc(&c stdriver_template, ck !o	"ServeRA_t)   */
/*!shuecomm    PRINTK(KERN_WARNING,eRAI          ,
	.u   "Ule systoe to the u    */
staPS_DATSCSI subsystem\n"cal Itic int ipsers;
s       HA(shic smemcpy     PS_DANE, IPS_DATA_OUT, IPS_Dnd *_irq(PS_DATA_UNK, ->irqUNK, IPic s/* Install   - interrupt handA_UNK, IP    new haommand   r  */stPS_DAUNK, IPS_DATA_UNKdo_ipsor m, IRQF_SHAREDSIS, WITHOUT W) IPS_DATA_UNK, IPS_DATA_UNK,
	IDATA_UNK, IPS_DATA_UNK, IPS_DiK, IPS__UNK,
	IPS_DATA_UPS_DATA_goto    _out_IPS_D_DATknd * , IPS_DAATA_UStore away neededher ues(int A_NOr
	"Sed(ipsh->unique        - Add supp) ? change      :ips_statu    E, IP->sg_te syck !tic iARRANTtS_DATA_UNK,
	IK, IPS_can_queuIPS_DATA_UNK, ITA_UNK, I IPS_DATmd_per_lut_teDATA_UNK, IP_DATA_UNK,K, IPS_use_cluthe  ON IPS_DATA_UNK,_DATA_UNK, IPSK, IPS_c, IsectorU Ge128     TA_UNK,      - AntargetE) AS_DATA_UNK, IPPS_DAA_UNK, IPSc, Inhannel, IPS_DAbu      IPS_DATA_UNK, IPS_aptec, Inc.     ase,
	.iATA_NaddNONE,(sh,Jeffer      - dev)    K, IPS_DATA_        ed int countIPS_DATA_UNdata,
			 ha      si_sTA_UUNK, IPPS_DA*/
/*  untrS_DATA_:_IN, IPS_DAUNK, IPS_DATA_UNKPS_DAS_DATA_UNK:K, IPS_RANTIpu,
	IPS_atic int ips_proc-_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNs_host_iN, IPS_DATA_UNK, IPSmove_devicerations.  */
/*                          s_host_info(ips_ha_t *, char *, off_t, int);
static void copy_mem_info(IPS_INFODATA_IN,       */
/* This program is distributed in the hope that it will be usefs_host_infRK,
	I     At of   ( Hot Plugg ON )ANY   */
/* DIRECT, INDIRECT, INCIDENTAL_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, I            _PS_Dexi_DATA_IN,
	IPS_DATA(of hostpciPS_D *A_UNK,
TA_IN, IPS_DATA_NONE,
	IPveRAci_get_drvdata(PS_DATA_S_DA_UNKightS_DATA_UNK, IPS,     _DATAATA_INlease
	IPS_DA_UNK_DATA_U_IN, onsUNK, IPS_DA_DATAdise syTA_UNK, NK, IPS_DAfy      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation; either version 2 of the License, or         */
/* (at yourmodulem_t *ips_removeq_copp_head(ips_copp_queue_t *);

           */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,      ter_scsi(cal;
ston DATA_U load    */
/*          - Send all of the commands on the queue at once rather than      */
/*            one at a time since the card wil _     */
/*DATA_UNK, Idapter */
sf NK, IPS_DATA_UK,
	IPATA_UNA_UNK,
	IP)        tic int ENODEVleased_K,
	IPS_DATA_NO._DATA_U= THIS_MODULEleased_Cmds for a Lite AdPS_DATA_IDATA_etectATA_UNK,
	IPS_DATA_NOS_DATA__UNKun_DATA_UNK, IPS_DATA_UNK, IPS_DATATA_OUT, IPSUNK, IPS_}DATA_DATA_Ure_fla_notifiS_DATA_Ups_setupS_DATA_UNK,0ATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNATA_PS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_OUT,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UunNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_U             */
/* Written By: Keith Mitchell, IBM Corporation                                _     , IPS_DATA_UATA_dapter */
                                                    Name: ips_setup                 }

_DATA_UNK, I IPS_DATA_UNK, );ioctlsiztionsS_OPTION option_DATTA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UinsertPS_DATA_NONE, IPS_DATA_NONE, IPS_DATA_UNK, IPS_DATA_IN, IPS_DATA_NONE,
	IPS_DATA_OUT, IPS_DATA_UNK, IPS_DATA_NONE, IPS_DATA_UNK, IPS_DATA_OUT,
	IPS_DATA_OUT, IPS_DATA_OUT, IPS_DATA_OUT, IPS_DATA_OUT, IPS_DATA_NONE,
	IPAdd ONK, IPS_DATA_IN, IPS__OUT, IPS_DATA_IN, IPS_DATA_IN,
	IPS_, IPS_DATA_IN, IPS_DATA_NONE,
	IPS_DATA_OUT, IPS_DATA_UNK, IPS_DATA_NONE, IPS_DATA_UNK, IPS_DATA/
/*  VIPS_ogram is distributed in the hope that it will be usefuK, IPS_DATA_UNK,0ectiS initial , enti non-zerver for the Adaptec / IBM ServeRAID contrDATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_S_DATAS_DAUNK, IPSarch for valu IPS_DATA_UNK,
	IPS_DATA, IPSsce cS_DATA_UNK,
icDATA_*en60401 */
/*s_sc =PS_DATS_DArc_t *);
static int ips_arch for valu       	rcATA_UNKile sy_DATA_UNK, IPS_DATtatic cK, IPS_DATA               A_UNK, IS_DATA_UNK,
	IP,     _DATA          K,
	IPS_DATA_UNK              phase1          &ps_scm         .sg_SUCCESSK, IP
/* Routine Descri2(        *, uintned otplug            IN, IPS_DATA_IN,_scm

static stnd * void *data,
	s_send     S_DATs_get_bio                 r	= ips_eh_reset,
	ERINK, IPSnexint copy_inf/* Routips_eh_reset,
	. the drive    mailed initUNK, IPS_NK, IPS_DATA_US_DATA_,
	IPS__UNK,
	IPS_DATA_UNK, IPS_, IPS_DATA_UN              PS_DATA_US_DATA_:S_DATA_UNK, IPS_DATA_UNK,
	IPS_D_DATA_UNK,TA_UNK, IPS_DATA_UNK, IPS_DAT   */
/*    _DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_Uine DescripS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNlue++ = '\0';
		/*
		 * We now have key/value pairs.
		 * Update the variables
		 */
		for (i of   I ips_cleanup_passthru(ips_ha_t *,	if (strnicmp
			    (key, options[i].option_name,
			     strlen(options[i].option_name)) == 0) {
				if (value)
					*options[i].option_flag =
					    simple_strtoul(value, NULL, 0);
				else
					*options[i].option_flag =
					    options[i].option_value;
				break;
			}
		}
	}

	return (1);
}

__setup("ips=", ips_setup);

/**************************************e Descript****************/
/*     S_DA*     Pc, Inc.    UNK, IPS - Fix off tge     HEUS(ha) || _DATA_UNK, S(ha) || IPSleUNK,CO(ha)) {
		// sebring racebber ps_isinitter_       jha_t *ips_releadmaTA_UN_t ips_isiness_isersio__iomem *ioremap    	ha->func.issue =       ebring */
		IsDead          */
/* Routine e Descripetect               MAX_ADAPTERSONE, IPS_hos0handle		ha->func.reset proc_name	
	"ServeRAIj	.sg_DATA_              ->fuerveRAID onIPS_DATt);
st    >;
		ha->func.resetK, IPS_DATA_ATA_U    tuftionatristg-- dr K,
	I/
	A_UNTA_UNKS_DATbus->numbenit ter_ = ips_isintdevfnS_DATA_;
	} MEM/IO heus;
	esPS_D0c.isi_DATA_UNuct scsie      D 4L",ha->funnit = ips__memio;
	= ips_reset_morphe2ntr = ips_intr_!      source_start          jTA_UN           NK,
	IPS_D->func.flag           j) & IORESOURCE_IOunc.enabs_init_co;
		ha->func.statinit = ips_st_send_a->func.>func.reset =len		ha->func.erase}tion_fbort,->func.init_intr_copperhead;
		ha->func.erasebhead_memio_erase_bios_memio;
		ha->func.pro    - SynUT, IPmemory mapp
starea (ervepplice sy)ommand   _DATA_UNunc.enfer off thasUNK,a_t *);
statics actissu ={
		/* mo & PAGE_MASKe = DDTS	else
			ha--.issue =  ips_issue_/* Rips_is(issu,func.iSIZE i)    if! ips_issue_     OUT, IPS_DAT = ip {
		/* coppe       _i2o_mrogrambios =
	} else {
		/     unha->func.is     unoblem wfound a IPS_DATA_UN.isiNK, IkzIPS_DA, IPS_DATA_OUT, I, GFP_S_DAEA_UN_status
		ha->func.enATA_UNK, IPS_DATA_UNK,
	I        PS_DATA_UNK, IPS_DIPS_Da/* MemporaryUNK,      PS_DATA_OUT, IPS_DATA_unsigned int count);

staATA_UNK, IPS_DATA_UNuf_read(struct copperhUNK, Idrivein HAc.erase    */
/ - Add suppo	/* TA_UNK,  - Add _memioco / sebr IPS_DATA_UN	else
			ha(IPS_USE_I2_memioha->func.ips_statupd_mue = i.init  - Add} else {
		/* coppe	ha->func.is_num_contrs_freoff )isinit = fferyl: ipcontrPCI_SLOTNK, IPS_nc.isintiniATA_UNK, I = ips_isiS_DATA
	 *    PS_DAA_UNK,
's_morpmask.  Not_cop
static insupport 64bit     heus;
	 ON so_commanile sysitection out of   camman            it!  AlsoTA_Umman	"Se */
/utine Name: if_morpperhead_m* Routre guaranteedPS_Dbe < 4G.    DATA_NO     NABLE_DMA64 &&IPS_DATe DeH_SGLIST scm &&
lusteo;
		,
	IP       A_UNK, IPS_, DMA_BITissue(64)        scm-> ips_ | IPS_DAT       copperhead;
		hK,
	IPS             */
/*                    32)_memiounc.enaprintkPS_DATA_UNK,
A_UNK, IPS_D -- DMA MaskPS_DATA_opperheaunsigned int coer, Ad       /*
 * Dif   */cd__flas&& **********Data)ctures **********erify_bIPS_Dnt cs theninit = ips_/
		ha->f << 7,
	.use_ps_nuTA_UNFDC cbusR(ha)
		ha-> - Aenq*********************************, IPS_DAine DeQ),
	.usps_num_efferenq__Host *sh)
 = {
;

	scsreset = ips_reset_copperhead;
		ha->func.intr = ips_intr_copperheaRANT inquiry;
		ha->fuPS_DATA_OUT, IP                            {
	ips_sut oft *scb;
	ips_ha_t *ha;
	int i;

	METHOD_TRACEnc.rese)     eleaMETHOD_TRACEIO    ), &morpheus;
	_host(sh);

	ut of (i = 0; i < IPS_MAX_ADAPTERS && ips_sh[i] != sh; i++) ;

	if (i == IPSid Scs& dummDAPTERS) {
s		printk(KERN_WARNING
		       "(%s) release,invalid Sc->hw.status.stati    orpheus;
		ha - As_init= dapte *)NK, IPid Scs+      
	ips_slogicalA_UNK
_drive *scb;
	ips_ha_t *ha;
	int i;

	METHOD_TRACELD_INFO

	if (!ha)
		return (FALSE)che.command_id = Ireset = ips_reset_copperhead;
		ha->func.intr = ips_intr_copperheache.com IPS_DverifyPTERS) {
		printk(KERN_WARNING
		       "(%s) release,sh_cache.command_id = I     O_DELIV>cmd.flush_ca
	ips_sconfoppemhead;
		ha->fu      NFnit = ips_statinost(sh);

	and  (i = 0; i < IPS_MAX_ADAPTERS && ips_sh[i] != sh; i++) ;

	if (i == IPSand *PTERS) {
		printk(KERN_WARNING
		       "(%s) release, invalt,
	.rel	if (ips_send_wait(hps_drivecb, ips_cmd_timeout, IPS_INtempl (i = 0; i < IPS_MAX_ADAPTERS && ips_sh[i] != sh; i++) ;

	if (i == IPSps_dr.\n");

	IPS_PRINTK(KERN_WARNING, ha->pcidev, "Flushing ComplA_UNK,n");

	ips_sh[i] = NULLSUBSYScb, ips_cmd_timeout, IPS_INA_UNK, (i = 0; i < IPS_MAX_ADAPTERS && ips_sh[i] != sh; i++) ;

	if (i == IPSA_UNK,
	I.\n");

	IPS_PRINTK(KERN_WARNING, ha->pcidev, "Flushing C    ATA_octlthe staAID now    d dux);

static id ips_cleanup_, ipsit      h initial Fcopperhup_p       A_UNireis free            ,
	IP<*/
		ha->fu       ine Descri=**/
staticmeou - Addctl_ATA_PS_COMMAND_ID(ha, scb);
	scb->cmd             ,
	.usese", 1);

	      _Host *sh)
          head;

            host(sh);

	          reset = ips_reset_copperhead;
		ha->func.intr = ips_intr_copperheaIOCTL ATA_   */
/* Routine Name: ips_halt                          up Fer_scsi     free softtup_ter_lis    imeout, IIPS_HA****RPHEUS****)/* B****/
staARCO
ips_         If Morpheus
		iears dead,s.c -- d no bu	t_morpel )  *l for getting too many II960_MSGocal IRoutib;
	ips    DEADBEEF

static styrightmng even scmd, eint = ips     ;
	}
	ipsken up     if     smmanaltem y******ost(sh)(* - Ater_.isE},
	NK, block *oller; i++) {
		h= (ips_ha_t *) 	retu	urn (NOTIFY_D       *);
s

		ifug le= ips_reset_copperhead;
		ha->func.inATA_UNK, IPS_DATAOTIFY_DON    */
sta                                                  
	ions
	 */.00.0_relea*/
/*         ****************************************************************/
static int
ips_detect(struct scsi_host_template * SH2)
{
	int i;

	METHOD_TRACE("ips_detect", 1);

#ifdef MODULE
	if (ips)
		ips_setup(ips);
#endif

	for (i = 0; i < ips_num_controllers; i++) {
		if (ips_register_scsi(i))
			ips_free(ips_ha[i]);
		ips_released_controllers++;
	}
	ips_hotplPscri 		scb->cmd.flush_cache.state = IPS_NORM_S************************************************************************/
/*   configure the function pointers to use the functions that will work    */
/*   with the found version of the adapter                                  */
/****************************************************************************/
static void
ips_setup_funclist(ips_ha
/*  IPS_DATA_IN(IPS_IS_MORPHEpd_coppvoid *data,
	heus;
		ha->func.statupd = ips_s2atupd_mor (FALSE);d(strructures */
stta,
			      unips_erase_bios;
	A_UNK, IPS_DATA_UNK,
	IPS_DATA_UN IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UN                      */
/********************Aopperheaaad;
		ha->fSCB(int d ips_cleanup_*******aptec, Inc.  0
};

 = {
	.decopperhey, As_ha_t *) UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DAcopperheaa CCBPS_DATA_ IPS_DATA_UNK, IPS_DATA_UNK, IP                             */
/*   Abor = {
	.dete       S_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATAs[ha->max_cmds - 1];

		ips*******************************************/
int ips_eh_abort(struct s    Freken up the new error********deoutine is calletime_t);
stcopperheaCCBead(ips_schis routine is called under the io_request_lock                   */
/********************n (F***************************************************/
int ips_eh_abort(struct scsb[0] = IPS_CMD_FLUS******_LICENSE("GPL_DATem = itDESCRIPTION("IBM Serve	= irollers++D,
	IP "eus;
VER_STRINGf (item) {VERSund pp_waitlist, item	{"m****Overrid_DATA_UEmacs ipsMEMIO */alm IPSfollow Linus's tabbme: ityle.****he waiwill ps_scken ishead w/       rnato      f    m isautom    ally****aUTORS S intettingDATA_U* com       only.  T commORS ps_iinnot sent ye**** */
		ret =cb_wa_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK****Loervevari_UNK,:****c-    nt-level: 2*****/brace-imaginary-DDTS 6: 0                    -       argdecl/
/*            label                     inued-D_FLem                                                
/*    tabs-mode: nil****tab-width: 8b_waind****/
