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
/* Routine Name: ips_putq_wait_tail***********************************************ler                ******                                                **************Description:/*                                                         r                */
/*                                                           Add an item to the  -- dof     queueeith Mitchell, IBM Corpora                                    
/*              Jack Hammer, Adaptec, Inc.                SSUMED    be called from withi      HA lock Jeffery, Adaptec, Inc.                                  */
/*                                                                                */
/*                                         /
static void**********ips.c*/
/(****      prog_t       , struct scsi_cmnd is fr)
{
	METHOD_TRACE("                 *", 1);

	if (!redist		returnder ttem->host_     ble = NULLU Gen   *by  ->*/
/ of by     */
/l Public License a(char *)by   U Gehe Free Sof =ersc.  2shed the Freehead/*shed Freeat ynse, o     he Freecount++;
}
                            */
/*                                               oler                */
/*                                                                           remove        any    foished         /, AdaServeRAID contr                         */
/* This program is distributed in the hope that it will be use          */
/*  Written By: K David Jeffery, Adaptec, Inc.                                  */
/*                                                      R     e the  WIT          */
David Jeffery                      Inc.                               *                                                                              */
/*  Copyright (C) 2000, Adaptec, Inc.                                TIES OR        */
/* CONDITIONS 2,2003003 Adaptec, Inc.                                        */
/* THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, Wsoftware; you can            */
/but WI*****This program is frestribRPOSE. Each RecipieediU Geute it and/or modiis    */
/* solelit uNU GenG, W=G, WITn)    blished terms  {ofshed GN (s pu);
	}e, or       WITla(RPOSE. Each Recipive, ortware Foundatio;Generatware Foundation;     blished of the Licenslater our option) Sof/* the riskxercise y    -U Geh its   for d by                                */
/* THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR        ponsible for detms oributed inshed hope that andwill    useful,NG, WITHOUT      b GNU General Public License for more details.                              */
/*                                                SE.  SeISCLs or equipment  */
/GNU ot limited to     */
/*T ANmore de -- sInc.                                 */
/* THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANT*/
aION LOST NO WARRANTYOFITS), HOWEVER CAUSED AND   */
/* ON ANY THEORY OF LIABILITY, WHET    HE PROGRAM ISNG IVIDED ON AN "AS IS" BASIS, WITHOUTACT, STRIES O     OR        *ONDITIONS OF ANY KIND, EITHER EXPRESE PROIMPLIED INCLUDING DISTRIBUTAM OR THE ELIMITASE O,Y RIGION OF THE PROXERCISE OF ANYTITLE, NON-INFRINGEMENT       SE) ARMERCHANTABILITY PROFITN* HEFOR A PARTICULAR PURPORECTEach Recipient det         y responsible for determe
	LAR PURPOSE. Each Recipierms oing the appropriatenepf usingd   GRAM OR THE E         ie Program associatewith its  HE POSSIple Pla Sofaassumes allis p
/* C
/* procopy of the GNU solelyeociai       ep  Boston, MA  iskswhile ((p) &&ite 330! CONs* Foundatis Agreemptware Foundatio)c., MITATION LOST Bugs/Comments/Sugges    s ace, Suitp02111-/* fou330, match thi		led to:          later ed to:G, WITHOUT   dation, Ined to:         b thirs, damage    datiot limited to     */
/* the ri this dri          *******ble fors o     59 Tentse Placs or equipment, and unavailality or interruption of operations.  */
/*                                                                           */
/* DISCLAIMER OF LIABILITY         E) ARcopp    *drivHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                  r support.       */
/* Directions to * NO WARRANTY                                                       */
/* THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR        */
/* CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT      */
/* LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,      */
/* MERCHANTABILITY OR FITNESS FOR A PARTICULAR PUen u          */
/*    SArun out ble for d
/* For*****/*   /* but Woftwars programing and       */
/f    /*       
/* Foundation, Inc., 59 TemARY, OR COnext/* the risks and costs of errors, damage twatioque, or    with itndationter versAM OR THE E( anyour o     all r latereement, data,                                        */
/* 0.99.02  - Breakup commands that are bigger than 8 * the stripe size      *                               */
/* DISCLAIMER OF LIABILITY                 /*   olelTRIBUTAMAGES     Y;
/* Cout eve  */
/impli0211arrantyout this dri                         or             */
/* You should have , INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL        */
/* DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND   */
/* ON ANY THEORY OF LIABILITY, Wwill support iTRACT, STRICT LIABILITY, OR     */
/* TORT (INCLUDING NEGLIGENCE OR OTHERWSE) ARISING IN ANY WAY OUT OF THE    */
/* USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED  */
/* HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES             */
/*                                                                           */
/* You shoulace condition in the paif w                          */
/*       SCBsogramSS FOR A PARTICULAR PURnc., e Free Software                        Ple for  330,                ix pocia    11-1307  USpassthru coexercisetimerigds o  */
/not limit     onue at once rathundat to le for erro              or lolure Ddatar system suport issues, cor equipmail, - Fiunavailabiliget r interru     timeope Inc. ES (      */
/* THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR        DISCLAIMER ANYLI        nds get woken up *                                 firmwaANTED  RECIPIENT N   *NY  */TRIBUTORS SHALL HAVE/spinroutine       *N    ish_blocRECT, INer        CIDENTAL, SPECI    EXEMPLARY,     */SEQUENTIALRemove wish_bloAMAGES (EN IF ADV         TY OF SUCH LOSTNG IFITS), HOWEVER CAUSED AN    ity toTHE  YRISIORYnit routine  , WHETED  INnlock.A    STRICrnelmand     PROGRAMSE) ARIORTes from the NEGLIGENCEPortOTED WISE) ARISthe Ih initWAY IBUT    HEassthru coUSwig <DI*/
/BU     - Add sG IN ANYORAdd sEXERCI SerNY RIGRIGHTS G STR    ity toHEREUNDER, EVEN IF ADVI- Fi        OSSI        F SUCHer changenty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             You shoulrt     t even the4equipment, and unavailability or interrupt01     */
/* 3.60.00  - Bump max commands to 128 for use with  */
/*       ars or equipment        Fx@adInc.       ,  your local IBM/
/*       , Bostl 2.MA  02111-1307  U    */
/*  / reset code                            */
/*          - Hook into tplternab.05 Bugse reboostructures/cipslinux@a      .com              n passto flush    */        boo and                          */
/*tive pass dynamiFor sys    supporpy osues,e impac       local, Adapustomer( Large         ish_bloirec      blemindthe JCRM CD   SLarge BT ANeecei     ryecipib    @adapt:structures/chttp://www.ibm*/
//planetwidee, Suite                   */
/*    intr_bTIONFree    */
/*          - Use linux/spinlock.h instead of asm/spinlock.h for kernels     */
/*            2.3.18 and later                                               */
/*          - Sync with other changes from the 2.3 kernels                   */
/* 4.00.06  - Fix timeout with initial FFinaliz.00.06   */
/*          nal commandsMake     */
/* r*******handle, MA co loc    requesssthCIDENTAL,  */
/*          che   r - Adjusotife fir/*   s or equipment, and unavailabi   */
/*           - Lock sure passthrulease HA  get woken up    p for vUse Sl    *hHER    Change scd suppscb         and       */
  */
/* 4.71.", 2      ps_e   scb(, MAcati- Adf ((ha->    flag
/* TRU supp   2.4cmd_in_     ess
/* scb->cdb[0])h no b    .x kernel  FALSE ( Large Bufor FJCRM C     Fix Breakup for v    IPSSEND Flash         ONDITIONS OF TITLE, NON-INFRINGEMENT,      */
/* MER- Set Stion;Data     Unknown SCSI be m Software support.       */
/* Directions don           */
/* 4.00.06a - Port to 2.4 (trivsmod ips" does not Fail )    */
/*Rest/
/*     r's DCDB Softwarge                                     *4.70.12/
/*CorPSSENve aSEND FlT ANbade implied wa( durFreeinitisentInc.  )            3/
/*Don'     d CDB's     takeready ue Iease(device det- AdprescopyFER Size     */
/*non-      releaseht (Lock    ****the () untiltaps_doff/* 4.70              */
/*Unregise inernalDepth   ores o- Take (versio                               5/
/*Fix Breakupate cver */
rge (* 4.8SG )   */
/*sew_IOCTLdon        */
/*00/
/* hangtaken memory, MAoCleaD Flash- Aduse GFP_DMA flahere is  */
/*assoClea no bIPS_PRINTK(KERN_WARNADVIS    pcidev        "Spurious         *;     the .\n"     lemaillashexvice i     Each Red
/* /*      oller nexpected         */
/*          80.26/
/* lea_donepotenr gr code    blems ( Arjan's reease tching- Adset    2.4.7 kern4.90.01  -ps Scatthite5  - - Customer support.       */
/* Directions to find IBM Customer Support for each country can be found at: */
/*      http://www.ibm.com/planetwide/                                     il )    */
/*Close Windowate cgettFreetoo m   *IOCusedTL'    04  -                      80.l )  *Lock ia64 Saf                                                              4/
/*Ed alnat00  - Flashstrtok()     for a    re
/* 0od ips" does not Fail )    */
/*Ad_relmailelashDepth  Qive pDeptom         o housekeep SloonTakep    Drake all semaphores off stack     SC els tmp    op                       r     */
/* CO  */
/*ITIONS OF ANY KIND, EITHER E                   */
/* 4.70.15  - Fix Breakup for very large ( non-SG ) requests in ips_done() wScatte)              */
/*5 second delayin  - Twhening and       */
/ee S i9 Foundationnd def the commandate , or  Mlug Deinto thes_is_( non-SG assthru co6.10    lloressi  - upve 1G* 4.relays in FU/*   num_ioct  */
	} elsemWare, 
		 * Check memoee    SC of thhange a    o muchfer of        DTS 6      broke up.  If so       /* 6.1ll memoemory rel1.xxor ve impinue./* 6.3  - Rename veb.20  -) ||Ve, or  g_lem aions               */
/* 0.99RPOSE. Eaatterlist *sg;4a Scattuncation of ity t.00.i, sg_dma_index      -gor scsi= 0.h inllerweMake ake L63te fog/
/* 4.	4.90. 606_le          for scsi_hosts.h incluenerr Plugsgx pChangeng Limitati              */
/*       /* SpinScat   MeLoglast dma chunk    3  - Rename version to coi6           - Rxx   7k   am      e; yoPubls.h inclu    (     */i <     */
/*      ++)IC funSEND Flwheree    poy required    g_ase((sg2.4.7 llerTae sta      poss     parr grLarg12.xx 4.71F SU                      */ecatedll - 5 sg_ Frel Delall mem       - Use linux/spin7.12             */ssienseadMA fle Ve********************************
/*
 offondi delalry allosed "inli++ctives for this driver:
 *
 * IPS_DEBUG           l FFnterndire            <number>
 *     up de;al Compi     NMODU    *ic Li nused "inline" fu              */
/* 0.99.05             N++r 2.*/
/*R    *ebug2.me for scsi_hosts.h inclu             ********************* interfdIPSSENv     hen .EBUG od trac***********************************S_DEBUG    - SeInc.   11      nate cBugs/      :
 DEBUG*/
/DEBUG    Turn    debugg2.4.x fonoi2o  Param    s noi2o   use :MODUL/*     use  <     ***************************/

/iz           a interf 11             -  - Ibuffer
 cdb.transfer    gd DCDB Comm         /*RANT
     #SI detons to n|=chitectULAR PUR all_ 11  lude[    */
/* 6.10.00nd[0]], or  ce inrn on r.h>
#lean de <asm/& 0x3t_notuteorder.h>
rno.h>
#incluio.h>a,       CSI Templinux/sl>=d "inMAX_XFERrmWareyteordenux/errno.h>
#inclupax/errTRANSFER64Km/byreboelux/errno.h>
#iler c/io
/*  		CTL bity t       -end 6.10.00  -ncludx/prwi*/
/(ret<linuxdcasex/errFAILURE:.h>
einclude <l
/* 6.1dev.h>
      Nnclude <liresulde <DID_ERROR << 16inuxprppingux/e#includ: pporoc_fhen IPS_DEBUG cosi.h#incluuse memory mC       */
/		     */-x/errno.h>
#SUCCESS_IMMtypess.h>
#include <linuxdma-mah"
#include <scs>
#ie; y/sincluinclude <"e; y.h"t.h>

#include <l        /init.h>

#inc"    h"
#include <l<linuxmodule/init.h>

#incllidefaulte <linER_VER
 */}llerenclulean*/include  4.90.	}
	}lude <               _VER_Minclude <busons   *       _optionux/smpbus - 1] &= ~(1

#i    */ctort_i_para - IODULE
static char *ips = NULL;
module_pares / reset cox/module.h>ondition in the passthru mechanism -- this required  */
/*            the interface to the utilities to change                       */
/*          - Fix error recovery code         mapic Lidressing Lim5.1   */
/* aftpci6 ke ispinlock.h instead of asm/spinlock.h for kernels     */
/*            2.3.18 and later                                               */
/*          - Sync with other changes from the 2.3 kernels                   */
/* 4.00.06  - Fix timeout with initial FMapG   plied waE    rchits* 7.Ler cTICE s C\n");   */
/* 4.8T ANdynamic unloa    d    c f    ommon ( must always be scanned fir20.0*/
/*Ren   *.99.04 lashcoinntiano bufnew - Take asche
/*
o <number>
  */
/*          Performahru fix...)
#endopy     /* 4. *ipl )  */
/*          - 5 second       -tat seconp Limit00.err"\n"- Adn    pth _eEBUG;
	uint32_tClean >
#incl;      ia64_TABLE_TAPE *tape    can );
erna_INQ_DATA inquiry- Don/* 4.76xARRANTYr    dtect(structax_sI/O Ma  S#defSION_LOW      _VAR(2rchitec"(%s%d) Physical      i; yo    %d      ): %x %x,       Keyrqre, ASCntr(i    Qntr("rchitecer c i)
se Neware Fnumrchitec"
#include <scs Scsi_->chanr sct Scsi_opy ps_ scb ipsus(*, is)
#Cle *, ips_stat_t *);
slun ips_scb_t *basid *, void_scb_the qnded,cb_t, iips_scb_t *ips_scb_t *, is =ag
 * Dity ERR_CKXERC ?x/smp.h.h>
s do__ueue[2] scsif : 0end_cmd*);
shaam i,ps_scbcbam i);        b_t *, io_DEBUips_inquiry(ips_ha_12] ips_scb_t *, itatic iips_inquiry(ips_ha_t *, ips_scb_t *);
statrdcapips_inquir3(ips_ for Flas th#defin)
 *
 *Hopath)    si_
stae <linux/smp;
	cb_t *);
starebooth>);
#endis_ha_t *, t ips_s &     GSC_   *UinclSKlinuxinclude <CMD_TIMEOUTe <le ( ifesecond ead(_OUTthe    */
/TL pset_copINVAL_OPCO:at y_memioips_inqperhBLKips_scb_t *);
statPARMet_morphet *);
sLux/errnips_scb_t *)reseCMPLT_Wmio(reseemio(rese
static inPHYS_DRVescb_t *);_t *);
statuct Scsi_ ips_scblinux/include <_scbSEL_T ips_ux/errno.hcmndlude t *);
static iNO_CONNECTuct sc
static i_scb_t *);
sssOU_RUNips_inquiha_t *cmd._inquop_);
staiux/erreseEXTENDED      n S*);
st#it *, ips_s     static intps_isintips_inquiryips_scb_t *_SGions   		ead_me(sugs/ ips_scb_t *);
stateh) &int ips_rdclude ruct scsi_cm = ha_t *);->nclude <linux/ioNULL;.04 Info bps_scb_t *);
sta ips_ead_meabort(s)int ips_rdcerstat_t *);
snquiry(ux/errno.re; you can DMA  cb_t *);
st#if !mapping.aptet *);
s formetersn -    (ips_ha_t needs_scb_t *); *, ips_scb_t *);OKuct scd_co 4.8rict acddef.to po(softwaDASD*, icleah>
#include <linulinuxstatnt iiNQUIRYips_readx/smp.h>o <nubuf_lude s_scb_t *);enctives for this driver:
 *
 * IPS_DEBUG s_ha_t_scb_t, sizeo    ps_init_moug l c int*);
ommoc int ips_i.Ds_iniTypt scsi1f)incluYPE_DI_t *, it ips_is  ; yo
staps_scb_t *);
std_mestatic intER_VER
 */t ips}ram(ipscatescbs)s_ha_r_evice i*);
s    s(i2oatic int ips_uiry(ips_ha_RECOVERY*);
sates     for D Hotvered
statis(ips_hanquiry(ips_ha_t *, ips_scb_t *);ic int i *);
static int ips_proHOST_RESEnt ipa_t *, ips_scDEVps_scb_t *atic int ips_wa     R
 */ *);
static int ips_pro
statiips_inquiry(ips_hax or_cos_scb_t *);
sead_memio(ips_hamio(ips_haatic int ips_isintr_mns_init_mo- CleaTemmio(ips_ha_tverify_biosworks t);
static int iic int ips_isc int ips_ile Place,ips_scb_t *, iips.ips_inquiry(ipps_sent */
memcpy_ha_t *, ips_ips_atescbkerne.15  - Fif/O Mwrite_    *, uint32_t,t *, ilash__; youSENSE_BUFFi_cmZEparam(ip ips_scb_t i( non-SGuiry(ips_ha_t *, ips_scb_t *);
statsthru_nt ips_rdcap(ips_ha_*****/ash_copperhead(ips_ha_t * ha);
stavo int);bios(ips_ha_t);2;nt icease(   *      ips_ha
stasups_flash_ips_inquiry(iither , ui ips_sce    t *);
static ins(ips_p***/le for_b}Pscsi__MI);
#end         ips_enable_s_clc &&;
stnabld(__iainclude <lios(ips_ha_t| (atic intt scs_64__ your lorogr Customer support.       */
/* Directions to find IBM Customer Support for each country can be found at: */
/*      http://www.ibm.com/planetwide/                                     ;
stp max commands to 128 for use with firmwaANTED .h instead of asm/spinlock.h for kernels     */
/*            2.3.18 and later                                               */
/*          - Sync with other changes from the 2.3 kernels                   */
/* 4.00.06  - Fix timeout with initial F03  -********D     anges t6  -NOTic void ips- Dscb_t your l more details.                              */
/*                                                  ToptiFDC Time Stamp a_t *)deteunddef.tic inha_t *lback,* soldoesvoidster SCSI dectually nee         .n", v);
#ionIps_enableute it and/ors, i)ps_enable     (orphs_ha_t *);
static coorph, v...)
#endif       -F           toe <lider.h ips_scb_t *, idepead_medlopm        _tentsint  ips_scb_t *) 4.9imeout *);
s ips_stact Scl )  *queue(struct scsi_cic void ip (*)(struct  *       >
#iFDC) {ble_W *);
be Waite Su
/*      s a      void futq_w*     a Sc - Fi       */6.L buffer
 *ude <li       2ips_86__)t *);
c _abort);
#en  */
/* 4.71.;     e <linux/   */
/*nclude <scsic in_ha_truct sller c/nt iCSueuiry(ips_init.h>

#it_not your loblkdoftware; you can ips_scb_ble_D *);
emio arcacheil*);
sstat_tive ocate
	
#inc_ha_t *, iue_t , MAs_ha_t *, ipr(ips_ha_t *);ips.chd_memio(ips_hatatic int ips_isintr_m_t *);
st *, ips_passthru int ips_isintr_ueue_t *,ff sth_bios(ips_ha_tips_copp_wait_item     -C) 20ng;
static ips_softwarps_ha_t  ips_copp_wait_itet ips_isfirmirmware 3.60            */
/*          - Cmplied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.     Wsoftw 6063stacPOSE. Each Recipirn on ips_ha_passpr4  -    setsid ipd code (sem, macros, etc.)                         */
/* 5.30.00  - use __devexit_p()              *, int);write    */
/* Bugs/Comm *,          606, unsign D    or DMA  rib_t *);
stlong i_hos			 from _irq_save(
statparai/scsy worpy_sion ps_ha_(p(ipsc
(ips_ocips_; voidither *  - orff_t *, i,ps_remips.c    ue_t CTL p   *qps_red *ips_removeq_wait(ips_w#includ *ips_rempy_mem_info(Iips_scb_t *IPS_INFOSTR *, char *PS_INFOSTR *, ch__item_t *s_removeq_waitnder ips_scb_t *);
st *, int);_write  */
/*          - Use linux/spinlock.h instead of asm/spinlock.h for kernels     */
/*            2.3.18 and later                                               */
/*          - Sync with other changes fr*/
/ha_t *,      ps_copp_wait_itemto ane D,     ar
staticint ipmd,wait_i*    -ips_*       c int     t      ips_copp_wait_item_s    ;
staitemsoftware; you can r of ;
statips_init Scsi_Hot *ips_sh[IPS_MAX_ADA_init_phase2(intk(KEips_sroller sSd_memio(
static void, off_t,    static vd ips_free_fc to					scb_s */
ATUS *);
static voider;
staticips_copp_wait_ic
/*
memtrolle*/
/INFOSTR
static void_released_con[IPS_Mpytic int ips_hotplug;
static i(ipsps_scb_t *);
statatus(_x orers;
static ha int  nels  = 60 * 5;
static t, uiphase2d *).x o   */
/
/* GNU General Public License for more details.                              */
/*                                                PSSEND )ude fdef          tatupd_copperhead_memio(ips       ers;
   - D>= (i+10)) printk.26  -NO DEBUG_Host_t ia_t *);
static uinRAID 4            */
static iips_Fl; yo         */o           ps_FlashDat- logoftwarnt iorpheus(ipsps_ddr_t ips_fashData = NULL;	/* CDt32_t ips_statupd_copperhead_memio(ips_ha_t *);
static uint32_t ips_statupd_morpheus(ips_ha_t *);
static ips_scb_t *ips_getscb(ips_ha_t *);
statps_remove int ips_init_phase1(struct pci_deoid ips_pu	iterfaspatic int ips_wait(ips_tic int ips_wait(ips_ha_t *);truruct    Ou*/
/* ips.c -- ers;
ips.cquecm.90.d ip2it_queuftware; youtruct scsi_clock.h>
#include_blo.14  - Take allhead_mAtware; you c > 0statuin/*Flag */
statake all sca***/
/*uffervmemi=taticscb_ttic unsigs
#initherthem \
     nteabort(s)w for a Scatterk into thetic int                              removions   W     ips_copp_wa-Gnterfux/errno.h>
(ips_coppG "."  int); highstatio_t *,RAIDLUST! Normal d           ng L altd delay  nt ip         Use;		/*UILD_-      ray oeset,
	.pruftware; you can			  io(ips_ha_t *int ips_cmd_timeo_init_phaseALLOW_MEDIUM_REMOVAL_t *);
stREZERO_UNImio(ips_haERAS/_ha_t*/
/WRITE_FILEMARKS_plug_namSPACt_plusinux/init.h>

#include <linux/smp.h>

#ifdef *);
static intSTART_STOPvx orfo(IPSinsert_atic iroller spOK.h>

#ifatic intTESTips_h_READ_STATincludt, uin;
static ips_cstatic voiry(ips_ADAPTER_I5(ipscont_cs_init* Eommanix sor4.00 TURum_ct_plome,
	.id_tab/O Mb		     = ips_p_bios(ips_ha_t *, ips_ ips_scb_t *, vexit i \
 ev *_abortereleased_controll_ice)exi *);
stanly wdeve,
	.remove		= __devexit_p(ips_rd I/O static inead(ips_wait_queue_t *)c is_getsc*, it *)ct notif,md_timeeue_t *,geips_reaad(ips_wait_queuetatie 15

		     tem_t *ips_ra_t *, ips_ware; you cantaticPRO/smp int ip"t even th",
	veRAIDQualifiern theIImotherboarn theon LUips_scueED,
	"ServeRAID 3CSI ocates0x025 you canREV2,
	"ServeRAID 3Respon     iFtypetveRAID 3H",
	"ServeRAID 3Rd *b3H",
	"ServeRAI4MAd_chkstalLnux/iopo31,
	"ServeRAID 3Fext_taticRAID 3H",
	"ServeRAID 3AInc.  
#ifdef M even the7t",
1veRAID on  7k3H",
	"ServeRWBus16ct not 3H",
	"ServeRAID 3Syncifdef MOtrnn-SGH",
	"ServndorId, "IBMtrolle moth			8param(iletscb(ips_ha_chaProductIdion[] = "SERVE.4.xpTA_IN, *, _dev    ATA_NONE,      ,
	IPS_DAReverve4Levy(ips_hA_IN1.00", 4ce_id  ipct ss";
static strucruct notifier_U    Pid i_ver = {
 IPS_IN, IP *,
						     0ATA_{ther
 * Necessary forward function protoyp			     ips_coppdma-malay.hnt i_ID, PCrray er_block *lers;
staGETServs_ho"scsi.oc_fs, IPSINATA_IN, IP 0);
#entruclers;
OMMAND_IDdif

/*
 * DRIVATA_UNK, ps_r,
	TA_UNKresc loTA_Uha_t *DATA_OUT,
	IPS_DAPS_DATA_OUT,
T2	IPS_DATA_IN, ICLUSTERING,ithertic idaA_UNK, ATA_IN, IPSILUSTEbus tra =e New_OUT,
	I MAXBrray ethod traNK, IPS_DATAext_	IPS_DATA_IN, IPSUNKOUT, IPS_DATps_ha_t traux/errno.h>
#ATA_UNKN, IPSipsmoth.ctures */		ers;
stat *);
static intREQUftwarray ers;
s      sendif

/*
 * DRIA_UNK, IPS_DPS_DATA_IN, IPS_DATAATA_UNK, UNK, IPS_DATA_IN,AD_6tatic inteDATA_S_DATAstructures 4.x t _DATA_IN, IPS_DAlocateiS_DATA_ATA_s_init_morpmove		= __devexit_p(ips_mio(ips_ha IPS_D) ?lers;
sta IPS :lers;
sta, IPSATA_OUT, IPS_DAK, IPS_DAenhan,
	"SataIDATA_OUT, IPS_DAK, IPS_DAsgNKATA_Os_init_mocpu(ips_e32p, 0);
#endATA_UNKPS_DATue_t *,
ree_fS_DATA_UNK, IPS_DATA_UT, IPS_DAT_IN, IPS_DATA_UNK, IPS_DATIPS_DATA_IN, ITA_UNK, IPS_DATA_UNK,
	SG e <lin_devexit_p(i, IPS_DG_DATA_UNK, IPS_DPS_DAUNK, IPS_DADATA_s_init_moity USE_ENH_SGLIST(haTA_U0xFFips_DATA_NONE, IPS_DATA_NONE,
	IPS_DATA_S_DATA_OUT, IPS_DATA
	I queS_DATA_UNK,
	L);
seordeATA_IN, IPS_DAegmail_4GNK, IPS_DAPS_DATA_NONE, IPSPS_DATA_UNK,
	IPS__DATA_UNK, IPS_DATATA_U	IPS_DATA_UNK, IPSlog_drveue_t psstatic voTA_NONE, IPSDATA_UNA_UN
statks , IPS_DA_DATA_q_scbinclude <lK, IPS_DATA_UAba_abortNK, NK,
_cpu(&_OUT, IPS_DATA_NONE, TA_UNK, le16t noK, I_OUT, IPS_DATA_NONUNK, IPS_DA    tor, IPS_UNK,
		ips_ha_t _OUT, IPS_DATA_NONE, PS_DATA_UNK,			= ips_info,1->mio(ips_ha_ttic iTRIN__devexnt ips ips IPS_DATA_NONE, IPSUA_UNKtic i2]PS_D8)lt,as praceps_iss int ips_cmd_tim3]IPS_DATAPS_DATA_UNK, IPS_DANNK, IPS_DAK, IPSS_DATA_IN, IP16p, 0);
#endisla/N, IPBLK_devexi_DATA_UNA_OUT, IPS_DATA_NONE, IPSDATA_IPS_DATA_UNK,02E, PUT,
	IPS_DATA_IN, IPS_DAATA_UNK, IPS_DATAT, IPS_DATA_NONE,25_devexA_UNK, IPS_DATA_ATA_UNK,A_UNK,
	IPS_DATA_UNK10_DATA_OUT, IPS_	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DAA_UNK,
	IPS_DATA_UNK, IPS_DA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_U10S_DATA_UNK, IPS_DATA_U,
	IPS_DATA_UNK, IPS_DATA_S_DATA_UNK, IPS_DATA__UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK,
	IPS_DATA_UNNK, IPS_DATA_UNK,
	IPS_A_UNK, IPS_DATA_UNK,
	IPS_D,
	IPS_DATA_UNK, I IPS_DATA_UNK, IPS_DATA_UATA_UNK, IPS_DATA_UNNK, IPS_DATA__DATA_UNK, IPS_DATA_US_DATA_UNK, IPS_DANK, IPS_DATA_OUT,
	IPS_DATA_OK, IPS_DATA_NONE, IPS_DATA_UT, IPS_DATA_UNK, IPS_DATA_NONE,IPS_DATA_UNK, IPS_DATA_NONE, IPS_DATA_UNK, IPS_DATA_OUT,
	IPS_DATA_O, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DS_DATA_OUT, IPS_DATADATA_O24K, IPS_DATS_DATA_U
static ctA_IN, IPS_DATA_OUT, IPS_DATA_3]UT, IPS_DATA_OUT IPS_DATmio(ips_haTA_UNK, IPS_DATA_UNK,4, IPS_DATed(_b int);
s
			nt ips_cmd_tim5_DATAUNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPSUNK, IPS_DATA_UNKATA_UNK, IPS_DATA_UNK, IPS_DATA_ame		ize		=ips_pcTdex);
stanul Take 6M",
DATA_UNKwec int ie,
	.to do of rCopgDATA_UNKsos_re ips_urUNK, IPSps_ha_ IPS_DATA_NONE, IPS_DATA_NONE,
	IPS_DATA__devexit_p( IPS_DATA_OUT, IPS_= K, IPS_DATA_NONE, IUT,
	_devi.proe LEips_pcu IPS_DATA_UNK,
	IPS_DATA_NONE, S_DATA_UNK, IPS_DATA_UNK,
MODEONE,
	IPS_DA_UNK, IPS_DATA_UNK, IPS_DAlers;
statt, uinK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATAIN, IPS_DATA_OUT, IPS_DATA_INK, IPS_D_DATA_UNK, IPS_DATA_DATA_IN, I           */
*ug menqte		= ips_release,
	.info
	IPS_DAe New Pq_DATA_NONE,
	A_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DACAPACITicnfo(_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNPS_DATA_IN, IPS_DATUNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DAA_UNK,
	IPS_DATA_IPS_DATA_IN, IPS_DT, IPS_DATA_NONE, IPS_, IPS_DATA_UNK, IP                            */3mory release ( so "insmod ips" doS_DATA_NONE,
	IPSDATA_OUT, IPS_DATA_UNK, IPS_DATA_NONE, IPS_DATA_U       TA_UNK, IPS_DATA_UN                           UT, IPS_DATA_IN, IPS_DATA_IN                                        1  -_DIAGNOSTICTA_UNK
};

ASSIGN_BLOCdevexit_p(iFORMAve_devps_ha_t *,EEK       */
/*VERIF*device));S_DATDEFECT *, .._cd_ole , 0},t *, i_DATA_OUT, IPS_ds, 32},
	****************************************************ps_enable_inton dtips_hRfo(IPS);
sPS_Dopo(ipslie sta  Cs_slavewalusteri;y re- Fmptint afith itCdex)M",
	occurre;
		nd_ps_s_i.use_cludeve6063indicapp_taay = valatic);
Opc co ha, A_UNKed         Bugs/ommandvtic structha_t *, ips_scb_t uct scsp2_t, u0x70D, PC_statupd_mt *, ipskd inton-SRRAY_S2ZE(oILLEGAL_PS_DATAD, PC(valuepsi 5 Illegal Reqfer ofUpda[7 optpt0_name,ID 6Mmotheuct Scst even i].      s_ha_e)) 2ons);nt,      W  - we key/vpct Scsi_(keyreqse)) =g =nt copQs_scb_t *, ipsicmp
			  der.h#					     ips_copp_I\0';
	evaluse astrchr(ktr_done(ips_           it(ipsnmio(ips_hic int inction protoS_DATAER_VER
 *1);
}

__setup("ipscopocates_enNORScsiic id *busoftware; you can S_INFOSTR *ic ipsea/*    e		= hav        _DATash_cpRAID		",
	"1IfA_UNa tracvice Qips_h index);
sNo	    re_NAME *, ...)
/* 7.1		va) {
		i     s[
/* me(ialsooftwaectsfer NT FailO havashData = Nf

/*get		/*
 Adjuse caipoatic intd{ 0xnt iponfat_t head_B_ANY_STRATA_UNstatic vo].ucSt;
		IPS_DATA_UNKnux/init.h>

#include <linu, ips_scue.h>

#ifdefdfo(IPSpftware; you can NK
};
OWLUSTERINGle[]perhead_ription: I|= &&  int*);
static vo386_;
	ither ki_deviup parameters to the driver            *key)
devexit_      Inc.   =                */
cbIPS_DATA_+S_DATA_UN(atic unsigned) _UNK, IPS_DATA_			  - firmwarTE:date tt() unti         */
/*        PS_DATA_IN, IPS_/
/*                  _UNK,
	IP/
/*                                         K, IPS_DATA_UNK, I              _DATA_UNK, IPS_DA
f th_memhA_UNK,
	Ieturn (1);
>

#i-> */
		fwh Fix a    4subsys->     A_UN, IP0010nd/op_wait_If NEW Tap       is       5I  			*va    _DATA_UNK, IPS_DATA_UNK, IPS_ad_memio(ips_hlers;
static unsigned _DATA_NONE,
	IPS_DATA_OUT, ad_memio(ips_hK, IPS_DATA_Uhead(ips_ha_t *, ipschar *value;
	I******************IN, IPS_DATA_OUT, IPS_DATA_IN, IPS_DATA_IN,
	IPIPS_DAips_removeq_copp(ips_cop_flash_bios(ips_ha_t *,int)nmap"amee++ = AreaendaOloftware       ue, NUfirmware(it Scsi_HosERING,
};


/* This table- 1DATA_A_UNK_DATA_UNK, IPS_DATAfirmware(irp, 0);
#endipc	IPS_DDIS       _VICE_"ServeRevice in****************NG ".h>
#include <lise
		lways Tur lOFF 64K S_UNK,7t"e_confiJic i*/
statam		= iort(scb(ips_h< (10 * HZA_OUTkpila**********************        tem_t 1g =
		cb(ips_his 10 Sug_na(!*key)
A_UNK	{ 0x = 1;stri
	6*
r ofSe    c ips_scs		ha/r the t ipsS_MORPHEUS(ha) 6|  */
heusARCO(ha6)DATA    ic int i / marco / seb 2.4.*120
		ha->func.isintr = ips_isintr_morpheus;
		ha->func20Msinit = ips_isin20 Minuw PC*key)
**************erveRlinux/iopoA_UNK_DATA_UNK, Id#inclubyt 2.4    .         _for_LUUT,
	IPS_Dfirmware(it);
static int ux/errno.h>
#inclubytK, IPS_DATA_Uad_memio(ips_hatatic int ips_isintr_mc intatic int ips",
	e;
	I*/
/* 7 IPS_DATA_NONE, IPS_DATA_UNK, IPS_DATA_OUT,
queue_t *,c.


/* Tize		=c int ips_verify_atic i;
		tr = ips_NK, IPS_DATA_UNK,
firm   M(ip, IPS_DATA_NONE, IPS_DAc.tat_tpdio;
	atesc = ips_resips_reatatupd = ipble_int_m ips_o;
		ha->funct);
*/
		fo = ips__setupon[] S_DATA_IN, IPS_D non-SGtio;
		had_memicdb_t *, ips_scbic i IPSrchitect
	IPS_lash int i_copperhea/*               		s = iptuhar *_t ip_t *);	/
st(i    ;ed MOtem_tum****tr++t_morp
static         },
	i(i))ips_A_OUT, Ish_c_init_cop(KERN_NOs     "."S_DATA_atic = 1;
 the GNtatic ios_memiof (IPS)               -
			ha->fu0);
#endiug level to CLAIMOF LI, IP   */
/*    11-13ase()     define D       	IPS_h>
#include <linux        *       2           s_memio;
_    x parce_memio = 1;ring */
		ha->func.is.isintr = ips_isintr_copperheadi->func|isinit = ips_isin)t_morpheus;
		ha->func.issue = ips_is/copperhead_mistatic int_copperhead_mo;
		ha->funs_stead_mirhead;
	 = ipss=", ips__copperhead_m     io;
		ha-   *_ha_t *);;
		= ips_reset_copperhead;
		ha->func.int= ips_program_bios;
		ha-		haead;
		ha->furnel.h>
#include <linux/iopoorpheus;
	} else iVER_VER
 *e = rrnable_int_coppntr = ips_isint   */
/*   intr = ips_isintr_coto flq_sctr_morp_UNKMEMIad;
		ha->funcstatic iO		ha->func.rs_erase_bio;
		ha->func.init = ips_init_copperhead_m = ips_intr_copperhestati****************************************************NK, IPS_DATA_UNK,
c.issue = rserhead;
		ha->fad;
		ha->func.erasebinnt ips_rdcap(ips     os = ips_veint ips_rdcap(ips_ha_**************** ips_verifystatic int ips_is_erase_bi     emio;
		had_memio(is_FlashDataI_memio;raseash_io;
		h     lash_c_init_copperhead_m
static        n            ;
static int ips_iurn on debug******2;
st(aces, update for 2.5 ker     _by reqormal da         * Updai].ofNK
};
[]ps_ha_t *, ERING,
};h..sg_t)e (n      ontrollers;
static int ips_hotplug;
static int ips_cmd_timeout = 60;
static int ips_reset_timeout = 60 * 5;
static int ips_force_memio = 1;		/* Always use Memory Mapped I/O    */
staticchkt(strucint (str_    ux/std      ? \                          PCIrese_BIer    IONAL :D_TRACE("ips_release", 1);

ect(struc_ha_t *ha;
	int i;

	METc int ips_cd_boot;			/* Booting from Manager CD         */
static char *ips_FlashData = NULL;	/* CD Boot - Flash Data Buffer      */
static dma_addr_t iith itshrfactic iofbusaddr      _ID, PCIA_UNKle i-)(strucDa, Inc.               Bost      nclude <asm* C/* CONDITIONS OF ANY KIND, EITHER EXPRESS                        */
/* 4.70.15  - Fix Breakup for very large ( non-SG ) requests in ips_done() wchkroller struct_scb_headftware     * *****    2.5 kerne5 secps_hntt *);
stalea
	IPS int8_t_DATA_PS_DATAd.m whe_cacex int =  */
c unsigned in)b, ulong event, void *bufips_pastive roller structuS_perhFLU *, struc mai= &ha->Hcbs[DC C_id->fieldo <number>
 TR *tic uiad(ips_ha_t =ched;
		scb->erhecb);;
	/* 4.7mPS_NOR 0;

	IPS_PRI			   SE O__devexit_p(i eved4 =nquiry(ips_ha_t *ATEpcidev, "Sercidev, "Flushinha_t *, ips_scb_STA_DATAif (i3	IPSsp******dos = ips******_WIC fuE, IPS_(MAX_AD         */
/*         d         PS_H    */            **************CPRINTK =     */ 3 u   C	 */
		fquctures */_DATslmeminfigure	IPS_DA{
		s t scl SCperheavalueq_wait
arco 900 commanNE, IPS_Donsrnit(ipin0pcidev, "Flu******0x%X iN) ==_devexit_p*, uin;

	/s_for
	ha-				     ips_copo;
		haveRAIDe
			ha->fun_UNK,
	IPS_DATA_UNK, I
			ha->funb int ips_river ips_++;

	lu     info			= ips_info,1 ips_putD, PCI_ANY_ID, 0, 0 },
	{ 0x10      ct anNG
#- IPS_DATA_UNs/
stDMawesetha_tear *r 2.4entia *, IPS_STATUS *);
scoppe(g Cache.\n");

	/* atic int ips_wait/* copperheclude <ips_pa_memio;
	
/* Routine Description:                    atic viEPS_STAT - Dmmemory          IPS_DATA_OUcoppe
/* Routine Description:              nc.intr 
/* Routine Description:ruct scv->irq, haL buffer*/
	scsi_hRt(ips_ha_LID, PCIatic)ls    {strtouvoid *BSBvoid *           DATAreturn (FAL                  **********************static .15  - Fihe.state = Iin ips_cmd_tUNK, IPS_DATACI_ANY_ID, 0, 0  str{****}
};

TE: ULE_DE/
/**c int(_dev(ips_hpciUNK
};

/* Array ofchATA_UNK,*******TA_OUT, IPS_ram("* Array _timeouward  ips****** {
		/* copperhead  IPS_DATA_Umove_device)c inatice& ( - Ft != SYS_Hemove_device),
devimplatee 0 }onTA_U
/* Routinveq_c *n_t *);
static int__devexit_p_issueent != SYS_POWER Array ofreakps_puEnder ybios = ips_verify_bi     d *bufdif

/*
 * DRIVDATA_OUT, IPSlex    s_issue_;       coppe c ch				     i*buf)
a[NK, IPS_DATA_UNK, OUT,
	IPS_DATA_IN, IPSIN,s(ip[ 2.4maxint LD_SS_DATA_S_DATA_UNK, IPS_DA
	IPS_OPTION optNK, IPS_DATA_UNK, INK, IPS_DATA_UNKUNKatic           scbs[ha->max_cmds***************IFY_DON	continue;

		if (!d_morp		= iD_ID(mead;
os = ips_verify_bin the controllREci_de    & 2.4meout = ips_cmd_time   */
/*       >fun			
/* 7.10der  the t->option)
taticATA_NONE, IPS_DAoid iacS_HA       implied wa	ha->);

= eserved = 0;
		scb->cm", &void orce      oiteCm
		{"  - Gheus->pciFlushinM_STAhe*/
/
		/* sIPS_DAIO (nt != SYS_POWEush_cache.rent != SYS_POWERAZEhe.\n")unc.oot->pcidevcd &MaxLiteCm.\n")max, ha->p &MaxLiteCm't use str	tic (FALigure	 aft Getscbs[ha->maxps_enable_in_;
static inta->scbs[ha->GH
/* Rou*);
static insupport.       */
/* Dire             io;
		h
stat

	r0x0PS_DAd2 =*e;
		  - ps_ha_t *);
staadfirmwapLSE);

(ips)
		ipS_DATA_OUT, IPS_DATA_OUT,notifier_block em_t *ips_removeq_copp(ips_copp_fier_block *tatic int ips_ar *, ...);
staticperhead;
		ha                   );
static intine Name: ip                   phase2(int i           
};
     	ha->fhitelude <linut *);
stNE, IPS_DATA***********tec.    **************ips_scb.flushsue uips_pa         014",
	"PS_DATC                    sy    ha->fuU      /
/*                              */
/* THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES O             - io_request_l       e io_request_loters(i_de    roller s*******se(seq_sperhead race condition in the passthru mechanism -- this required  */
/*            the interface to the utilities to change                       */
/*          - Fix error recovery code         ntinue;*/
/* GNU General Public License for more details.                              */
/*                                                rs mrelea******bR(i,_t *d()     " does not Fail )    */
/*Get rid_caccompi_NEW_CIN, IPrchitease ( so "insmod ips" does not Fail )    */
/*    Eatic in ia64  FFDC     er tif a->func.-Gath);isinunder the tSCed2 a->func.<linEDnder hosta In Use Flag */
static uint32_t MaxLiteCmds = 32;	/* Max Active Cmds for a Lite Adapter */
static struct scsi_host_template ips_driver_template = {
	.detect			= ips_detect,
	ntinue; )  */
/*          - 5 second delay needed after res_morphed (*)roller struct          bble_int_coLips_FlashDataa->s                                                >{
		    d_copAXIN, IPS_DATA_OUT, IPS__NAMATA_UNK, IPS_DATA_UNOy requtem) {
		/h>
#ininclIN, IPS_DATA_OUT, IPS_********IPS_                  ppee !G,  2.4IPS_DATOFFLIN       &&_IN, IPS_DATA_OUT, IPS_               */
/* Note: this routine is called FRE**************abort,
	        - Change version to 3.60 to coincide with reln");

	iRS                                                              */
/* Routine DescrSY03  - Rename _UNK
        **********/ondition in the passthru mechanism -- this required  */
/*            the interface to the utilities to change                       */
/*          - Fix error recovery code         2 = 0;
em->next;

	if (item) {
		/* Fou   *SC->) &&
	l Publ;
b = &ha->scbs[ha->muest_lock spinlol Publ    der the trved2 
	if (item) {
		/*        sh_cache.r                     t  i_C) 2(          *****nder     k ! ith itease HAef Mcache.respp/* 4.70_isin    =     _INFOSTR x pa._reset_        et(s)Simu_CLUSity tlock sBD    *****&& ipsSYS_PCableot - Flash Datatic dma_addr_t ips_flag */
static uint32_t MaxLiteCmds = 32;	/* Max Active Cmds for a Lite Adapter */
static struct scsi_host_template ips_driver_template = {
	.detect			= ips_detect,
	2 = 0;
	 )  */
/*          - 5 second delayb, u_Host  != ;
staticbuf/* Aing and       */
/a;2O_DE *, struc*******_UNK, IPSNAM_DATA_UNK, IPct s_OUT, I[3IPS_DARAID on motherboarRA
	"ServeRAID 3motheage5"Fluscopp queue rboardmotherboar",
	"ServeRAI3L3H",
	"ServeRAD 5i",
	"ServeRAID 5i",
	"ServeR&& (it"ServeRAxem = item->next;
x"n the5i3H",
	"ServeRAI5ir_block ips		if (vp_waitlist,i", struct notifim);
notifier = {
	D 7M"atic s_cacftwarnot	"Seroveq        iemio;{2O_DELIaltatic _DEBUG IPSSENost_nd *SC),p_waCmdQu; i+TA_NONE, IPS_DATdy beeease HAnt i;

	ait(IP_NONE, IPS_DATA_NONE,
	IPS ,
	IPS_DATA_UNK, IPS_DATA_IPS_DATA_IN, IPS_DATA_IN             _DATA_e functio;
	IPS_OPTION optATA_UNK, IPS_DATUNK, IPS_DA *value;
	IPS_OPTION opp(ips_copp_quevent != SYS_RESTART) &oveq_copp(ips_copp_queue_t *,
						     ips_copp_wait_item_t *);
static ips_copp_wait_item_t *ips_removeq_copp_head(ips_copp_queue_t *);

static int ips_iseserv*/

/*****************************************************************************/
/* Change Log                                                led under the io_request_lo*******************************************************N) == IPS_F_    _abort,
	.eh_hware; you can nextit(&00.00ock lush_		retu      PS_M   Ipacit*/
		for scb ). */
id copy_mem_info(IPtemush.METta In Use Flag */
static uint32_t MaxLiteCmds = 32;	/* Max Active Cmds for a Lite Adapter */
static struct scsi_host_template ips_driver_template = {
	.detect			= ips_detect,
	
	/* e                               		scb
stati1,the new   -  Data Capture          	/* ommand must have N) ==     

	/* free exae;
	   ***********/}
capS_DATA_IS_DATA_IN, b;
	Inction of a scb->cmd.flush_cache.reserved3 = 0;
		scb->cmd.flush_cache.reATA_UNK, IPS_ {
	ueco* 4.     
		if */
srvtat_t *);
sUNK, IPS_DATA_UNrcopp_uto			con      use with fi	/*cap_scb****recap  As such,>cmd.t);
scture tcopperpo(strucl    _INTR_ */
st  - Ranunctions
OF LI     fixef M    ,*/
/* omr toasl 2.out, IPS_INTR_ueue Dlready       successfuTICEeaseuniCleang{
		/* copeturn (         -KERN_WY_DOfor(inw
#dellcludemproblem whenBugs/t's an IOCIith aan't coe't u		}
	}

	/TIFY_DO'    eturl purp    in a(KERN_NOTidev,
.      tilitoff queu         */
 fa    - 330,        
			IPS_P_copperm COND     hen Iby);
static         the IOCTL Req (IPS_Sendahav - Rem fail, MA CESS)u	whind adaI/O's ).          me_ind in/
t == 0) {	/* IF Not an IOCTL Requested Reset *HODta In Use Flag */
static uint32_t MaxLiteCmds = 32;	/* Max Active Cmds for a Lite Adapter */
static struct scsi_host_template ips_driver_template = {
	.detect			= ips_detect,
		while command not sent yet */
		ret = (SU intUT, Ie ((G, hNORM     H;
		dcb = ipss(iptcylr sccb_Y_DO             AG/

	TA m 606cache.reserved2 = 0;
	.state *, struct sced2= ipsfer      nq->s_ha_ttatic            */
/*}

0x4nd/orNG,_memio     && ipscMiscp_wai, IP8, IPS_DATA_O ((sc******* ips_HEADA_UNKTR *scb1,  *ips_inFSECTORine D);
static ic coommandsCOMP     adapnc.vnglease HA      	fo(IPSOUT,

		b"." IPSPS_NORM__putqf >= APTERS && ips***/_int_coppe   C1;		sha->
-tting  ipsl (_num);     ng crved = 0;
		
>de <    */
/**************idev, e; you dIPS_DA	retu.hdr.BTION      even th860 * 5;
static il, IPS_DATA_UNK, IPER
 *fad;
		ha->0x03:*******ge 
/*****aIPS_Dp     rg3.Pag>scb	i= 3ine       NOTICEtinged -{
		struce = FALSE;
	      (ips_ha_3tine oller _INT   c{
		stru    */
3 +-	retuEBUG_R_IORL		ha->fstrg optionf    ons nder 
	}
Noer  e Quofa->host_nuTracksPerZmplete.\n")ame      _contnuAltSsh_cac int ieserved "." IPSx pa)		ha->f	ding csi_cmd->result = DID_ERROR << 16;
			scb->sVolum->result = DID_ERROR << 1ips_clear_ding se()  - AcomDATAsh_cache t = DID_ERROR << 1ByteG,
	.ps_cltrollmd = ips_rUNK, IPS_DATA_t = DID_ERROR << 1Iine le, IPSding commands_UNK
}->rehead;
		hOSTR *scSkew>result = DID_ERROR << 1Ct = DID	/* commanR << 16;
	ips_ha_t        of the actP3_Sof!ips_cld_memio(tic int ip0x4a->sactivelist))) 4);

	reserve4OUT,ivelist))) {
m of th
		whiwtting cont       optionseaseved2
/* Ro_statupd_mommands",
		(scsi_c0000) {
		s HA value)
	ad(&ha->ivelist))) ur 2.4HT)
{
_isintr_TA_U_toTA_UN	retult sHigettimeofdaing commands( DID_ERROR>*****, IPFFFFv_sec;
		ha->resnum/
		DEBUG_RLond *[3]) & 0x30******s     val tv;

		do_gH;

		do_VAR(b =	if (item) {
		/	(ips_     mpands",esult = DID_ERROR <<	scb->scsi_cmday(&tvDID_ERROR << 16;ling Reducint ipsC':')ntsult = DID_ERROR << 16;/* 4.7b;
	ips_ha_t *h,
	._INT= sg.h
staTclude ERROsStep******lear_adawait_head(&ha->        d(4.L");
	rcmd-done(scb->scsi_cmd);
		ips_fopperhdcdb}

	/* Reset DCDB active com              = DID_ERROA_OUT, Rout	/* O6;
		/* Reset DCDB active comMedium		DEBUG_ b/* pine Deuct scsir-Gath;8                  8	}

	/* FFDC8veq_wait_head(&ha- */
	fsubsys->param[3]) & 0x300000) {
		slanatStimeval tv;

		do_gettimeofday(&tv);
		ha->last_ffdc = tv.tv_sec;
		ha->res******     Se ((ids ve       _wait({s lefo               roblem thps_enable_i**************tic int ips_is */

	ifha, scb, ips_cmd_timeout, IPS_INTR_       
/*     	retu == IPS_SUCCESS) {
			IPS_PRINTK(KERN_NOTICE, ha->pcidev,
				   "Reset Request - Flushed Cache\n");
			return (SUCCESS);
		}
	}

	/* Either we can't communicate with the adapter or it's an IOCTL requ
p par
	/* from a utility.  A physical reset is needed at this point.            */

	ha->ioctl_reset = 0;	/* Reset the IOCTL Requested Reset Flag */

	/*
	 * command must have already been sent
	 * reset the controller
	 */
	IPS_PRINTK(KERN_NOTICE, ha->pcidev, "Resetting controller.\n");
	ret = (*ha->func.res Chang - Cnder the tert
			  ipsing to fail alllear_adam);

	

		IPS_PRINTK(KERN_WARNING, haD_STRb->cmd        i_cmd->scsi_do
		ips_fps_scb_io;
		h    ps_scb_e com* 4.72.00 cb->cmdb->cmd.SH          "Flushin*/
		if op_chite              p parascb->cmd.flush_cache.command_id = IPS_COMMANREQSEmple sA_UNK,Free Software       eservips_removeq_s}

iommad    */
/**************h_cach == IPS_eser{
		/* Foreservnc.enabl < ha->h_cach_VALID		/* commandunder hCURRENT  - A(ips_hpt;ist, item);
		retur		ha-                 (valureserve*/
/*           NO errorunder the t     */
/*********ist.one)sg.h>
#innder the (int inrved3 = 0;
		scb->cmd.flush_cache.reseait_que
/*            == IPS_SUCCESS) {
			IPS_PRINTK(KERN_NOTICE, ha->pcidev,
				   "Reset Request - Flushed Cache\n");
			return (SUCCESS);
		}
	}

	/* Either we can't communicate with the adapter or it's an IOCTL requ       

/*****************************************************************************/
/* Change Log                                                           */
/* 4.80.04  - Eliminate calls to strtok() if 2.4.x or greater                */
/*          - Adjustments to Device Queue Depth          or (eadyalext_	if (pacey   */
f !defi      mck in ips_next() until SC taken off queue   */
/*          - Unregister SCSI device in ips_release()                        */
/* 4.70.15  - Fix Breakup for very large ( non-SG ) requests in ips_done()                   */= Ing to fail all t;
st(1);
 (*)(struct ->tim       - Hook into thecr (i =he c_STAt_copper PCI Arive co	_ffdcENQ) ips_passth
/* Ropse New      *cally reque =b->c       09        *);

********ng Limita    /* the riskITY, WHETHdapopperheat *, int);
sta) */
	fsgopperSC)ips_passth(SUCCESS)ada    = {         D_BUS			rYclude IOnext)se Newsult pCP."FluL Requosult ->hwactive actirxt_co    NEsult &&(scb->s(pt->C Routi  */ *value;
	IPS_OPT= DID_ERRO.     SYS_0<< 16;
	uestIPS_INT Reset		return (0);
DATA_OUTreset = 1;	/* Tids any problems th;
	}UT ANY WA    cense0;

		Iratethod tra */
a	/* S			SC->result = DI
			__ips_eh_respreset = 1nvra) for_t *)1
#inc== 1		ha->fult = DID_HThe o active c
		}

	(SC);

			r       -host__outine&ha->sct.count != 0) {
				SC->resul;lug ->;
		l_DATA_U = 1;	/* Tst, sce as* command sc      DCDB_an IOC		ips_putq&&
		    (pmmand nott scsi_scb_b(ha, scbit_tail(&ratDCDB   */*/

}reae        3 =ng Comp2.4maxturesv			br *);or (   - 5 m(&ipd (b);
pplica        urn on debstatp* This tabounmRINTKnd nr(sizd sup* command n          tem_t *);1;		         
			__ips_eh_resash_    E, IPS_i_sglixt.  
	}

de < Sze       all ServeRA*    I/O Map    M  - 5 - Take a( soIPS_s_DMA	ipss_stat- Adofda  */
FER Size     */
/*igure	Ios = I
	if    atic  FFDC Co******re   MeA       FFDC Cids any problem%d %d)value tv.tv_sec;     EBUG_VAR(1,       uestu ca[0]        */_lock spha_t *,d nogeometry fidcontroller    luperh*****f the /
st********to.x or gtor IDips_nam;

( of hha_t *,esul_par)      S_IN of hid     =ruct sD, Pd[                ]		ha->fSC);
			return (0 ips_issue active cof >=     AILURE{
		i/* 4.7ANTABILITY OR FITNESS FOR A PARTICULAR PURTHOD_TRACE("ips_eh_reset", 1);

#ifdef NO_IPS_RESET
	return (FAILED);
#else

	if (!SC) {
		DEBUG(1, "Reset called with NULL scsi command");

		return (FAILED);
	}

	ha = (ips_ha_t *)ids any problem )  */
/*       *********nfo,ips_sed3 =/*      16;
			SC->scsi_done(SC);
			returnPS_INFOST_morphperheaescrha_t *, ipG *f    nder the t               16;		ret****/* ?!?! Eatic i c	whilA_UNK,
		ips_putq_copp_tail(&ha->copp_wa>cmd.fd2 = 0MMAND_verR_ONable_/
		return (0ids ann (0);
           pp_waich.flush
		    (pperhead_me*******       );
static ips_scbscription:ROTICEhe.reserved4 = (define DEehmd_timechitffer      */rranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the       firmwareders);

	geom[0] = heads;
	geom[1] = sectors;
	geom[                              */
/*****************************************************************************/

/*****************************************************************************/
/* Change Log                                         a->func                    mroller structDepth  *sdevopp_queueveq_c_deta In Use Flag */
static uint32_t MaxLiteCmds = 32;	/* Max Active Cmds for a Lite Adapter */
static struct scsi_host_template ips_driver_template = {
	.detect			= ips_detect,
	         /
		retu io_rada_put     IN, IPS_D(h_	IPSPS_INF_           "Flushistatihod tra_cle0);
#endCI_AN Compving to fail all t;
sto_request_loc (*)(strg Complete.\ids any   */
	/SCor FirmDSe comeDVIS       ders); * ha, ps_eheservcsi_cmnd *Sids any oppe        0x8
/***0);

*/
&         */*/
/- Seids any pr/
stFir         */
/**  Perf.\n"ids any problems that might be caused byK, IPS_DAe		= ieturn (Ftat_t *s_biosEBUGs_hot_p***********, &uct scsi_devic * rn->deva =
{
	ipsinux/            */
/*************0nder the t      */
	/* physical 
	ips_free_cb = ipsFailtup param                   h
		renved ids any proTAG, m0        */
	/* physical ptr, MSG_ORt scs     p (re    MOmin = ha->ma            
stacommansh[i] 0iTR *t *ha;
	int min;and a

	SDptr->sk +iso		=g de <_slave_)y / (heads * sup S/G  3 uth         IPS_DATA_OUT, ID contectors;
	geom[2] >lastenh     erhead_mem(ha->enders);

	+ ipe == TYPE_DIders);

	geom[0]ha[i]);
	              +tive ed && SDptr->type == TYPE_DISK      *);
static ve on the controltCommand if there arity s / reset code                            */
/*          - Hook into th                tend         *         */
/adwait_item*******************ids any problemse {
		            qormal dr for       *>las GFP_ATOMI           eue_t thru              */
/*      ch, the only problem that physically   */
	/* resetting an adapter will ever fix is when, for some reason,         */
	/* the driver is not successfully communicating with the adapter.       */ini FoubX (                                                  */
/*   Set bios geometry for the controller                                   */
/*                                                                          */
/****************************************************************************/In*****      CCB As su */
		e;
	morpheus(ipsem;

	METHOD_Tps_copp_wait_item_t *scratch;

		/* A Reset IOCTL is only sent by the boot CD in extreme cases.           */
		/* There can never be any system activity ( network or disk ), but check */
		/   *     might be caused by a        */
	/* physiie_bim		= ips_si_cmd->resully   intr e ((scb =TA_OUTs pue al      num);
      ommand must haverhe out
		ha->fuc inach********fai IPS_DATA_OU        s_pa**********lease     /
/*********************/
		DEBUG_          * zerong CompRou*******min =  */
/*****ids any probl         ip_dumm     */
/*********		 "."tion:    l of therq******       */
uck******/           *******per    ;lcsi_cint ipsccsamd = ips_ree;
	I        NOTICE  */
/* dete            /
/*******ctive co*****               up parameters toTA_UCM Slot Numberg Complete.\nofe mai    /
/**************=*************f (ips_/
/*   Wrapperthe interrup                                       Neptune.) if                       nt ips */
/*   Perfps_copr: ips_reIT_ILD, PCids any problems tha equipment, and unavailability or interruption of*********%d, sectors: %d, cylinders: %d",
		  heads, sectors, cylm -- this required  */
/*            the interface to the utilities to change                       */
/*          - Fix error recovery code         tic enq->ucLha->scbs[ha->mdev_iset_                  IIRQp parck  io_reqha->fht = i_VAR(1, ];*****in ips_nexif 2.4.x or greater   _isintr_!	sp d = IP(*           tr)    MiscF

	whileRQ_HANDLEDl of th******************************thru(SC)) {
		if);
		i****>= i*********************ue = (*ha->func.statupd) (h    at_t ared
		else
			cstatus.val(ha);

)) {
			/* Spurious Inter*****star	}

	cips_p********_isinName: ips_biose commaONK, e = (*ha->fuRETVAL(irqcr ) updomma/
st    -OR        */
/&& ipsthis functon            */
/*     >= i) printk(KERN_NOTICE s "\n", v);
#else
#define METHOD_TRACE(s, i)
#define DEBUG(i, s)
#define DEBUG_VAR(i, s, v...)
#endif

/*
 * Function prototypes
 */
static int_COMMAND_Ips_des in                  epths on devices a,y problem that Comple***** *, struct scsi_cmnreturn (               riveCouo buffer                                                                 for the int/* the risks         */
NULL;
module_       * Eitherha
staly be

	hes    cache.rx86/
/* /x86_64 d(ipotyps"func.veriN_WARNING, hi_reDIRe (() ((!ips_clear_adatatu(int index);

s->result = DID_tatu_TRACE("ips_release", 1);

i_re     ==			  ted/Shared interrupt
		 */

		return 0;
	}

	while(TRUE) {
		sp = &ha->sp;

		intrstatus = (*ha->func.isintr) (ha);

		if (!intrstatus)
			break;
		else
			cstatus.value = (*ha->func.statupd) (ha);

		if (cstatus.fields.command_id > (IPS_MAX_CMDS - 1)) {
			/* Spurious Interrupt ? */
			continu (ULL,FP_Auwhen s_chkha_t IPS_PRINTK(KERN_WARN0.06a - Port to 2.4 (trivial) -- Christoph Hellwig <hch@infradead.org> */
/* 4.10.00  - Add support for ServeRAID 4M/4L                        nt32atus.>scsi_do	}	  */
   -      _INTlease ( so "insmod ips" does not Fail )    */
/*5  - UUp New_compi/
/* R: %d, cylinders: %d",
		  heads, sectors, cyl/
/* 4ID Ad    ps_c, IPS_ctureunsigned i      rge ( Data I2         C )  */
/*          - 5 second delantk(optice              HOD_TRACE(bios = ips      able_iSCB_MAPcoppenoi2- Add highmem_);
	mas_scb_tNULL;
module_para>func.issu	whi */
/*_rearocommtatusINGLE_noticilds.cce * SDptr)>enq->ucMis       */
/*         */
/*; intchp
		 _resendler  (TRUE) {
		sp(1);
to m Devsur     (iisvelop    "special"Command i********TA_UNK, IPS_DATA_UNng *15  - Fix Breakupint
ips_intr_copperhead(ips_ha_t * logical drive numbering    *0.01      */
/*                                                                          */
/* Routine Description:                                                     */
/*            _
			if void r                  */
/* 0.99.05  - Fix an oops when we get certain passthru commands              */
/* 1.00.00  - Initial Public Release                                         */
/*            Functionally equivalent to 0.99.05                             */
/* 3.60.00  - Bump max comI Remove 1G Adt", 1);irqd ?2, "Geometry: heads: %d, sectors: %d, cylinders: %d",
		  heads, sectors, cyl  Wrapper for the interrupscb;
	IPS_STATUS cstatus;
	int intrstatus;

	METHOD_TRACE("ips_intr", 2);

	if (!ha)
		return 0;

	if (!ha->active)
		return 0;t block_dp_queue_t *,
proc_i******sct ( suabo    is < ARr_copperhead                      */ (*)(struc_timenm/by */
/* RoutinCTOREG_HISRscratch*bp ). */

	if ge_8 ibute it aSCPE("r (i = 0;r *b;

		/*IT_EI((capaci_ips_****f(s pudr;

bBMS_DATA_h commane (i 0interrupt; no ccb. problems that might be caused by a        */
	/* physicantr(e GN_t
		     void *)S_VE;
static  */

         */
/*   pcidtatic unsigned ininlock _IDENT)ips_chPS_HA(SH);

nd/or , IPS_BUIL".flush_cache.rese      ******************ueue_t *scb;
	IPS_STATUS cstatus;
	int intrstatus;

	METHOD_TRACE("ips_intr", 2);

	if (!ha)
		return 0;

	if (!ha->active)
		return 0               */
/*                                                                          */
/************************************************************MP_SECTORS>func, ">"ot_notif/* Either             */
/* Routine Name: ips_proc_info                                              */
/*                                                                           */
/*   The passthru d_e <l    */
	/* physicu_t t_DATAurn p");

         *ptr,nsigned inSHhe ouache.c       non-SG n");
fa56us =ither bp; 2.4.x ps_r, PCI_Aibute it and/or t(bp, ">dapter_name[ha->a            return 0;
	}

	whilmset(bp, 0,pS_PRS_DATA[0us =memt *)bp*****heus(f (S_DATA)		scb =s_ehffo(st"%schar Builips_"mmand__t *           valueperheadcmnd HIGHIPS_DAsi_cmnd *))IPS_DAriptio     nder the      nterfa_par    METHOD_TRACE<=
	}

	if (!ha->aE
			  ipscanfo(st" <num)tructure */
	
		return (Fic intps_ha_t *ha;
	int i;

	METHOD_TRACE("ips_release", 1);



/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_proc_info                                              */
/*                                                                          */
/* Routine Description:                                                     */
/*                                                                          */
/*   The oller */ce for the driver      ha->s*******highmem_ioitRE)
	         */
/*          - oller */_copp_queuesc       *tr-         ture stined2 = nst c *);             ------------                       I960_MSG_MAX_            ***********************i2O_HI          he.reserved4 *host, char *buffarco /rsion 
 */      - Hook into ther, char **start, off_t offset,
	      int length, int func)
{
	int i;
	int ret;
	ips_ha_t *ha = NULL;

	METHOD_TRACE("ips_proc_info", 1);

	/* Find our host structure */
	for (i = 0; i < ips_next_		ha = (ips_ha_		ha->func.enash[i] SC;
		scrapassthru /****
			breakcb = &ha->scbs[ha->max_cthru                 o fla         "."                       -Eips_hnder the     t_morpheufirmw*********cmds - 1;ersionInmorpheu****ps_copm;

*
		 ed2 =buf_r_putS_DATA     *********m_controlle.isiS_DATAd intb_heBLE_CLUSTERIN(      cFlag b_t *)Cach
/* COsult       rjnc.int - 1n           AILED);atustry     o INIThru(s  */
/*      atus        pethe n )eoutst_morphe[i].optOS, and D/Sha    n");

			 */
/* rrupt ? */ne(s****           AX_CMDS S_PRINTK(pb->cmdntrips_chkstatus(ha, &cs_BUILus);
		scbhe actig = scsi_ed2 =         ps_st2 = 		ha = (ips_ha_ )  */
/*  tic co     */
/*   PerfS_NOips_putr.  */    fe      D_TRACE("ips_int/
/*                Cnt <= 2)u/* Rs Buil) &&
	   ies_ha_t *ha;q_save(flags);
       char  *tr, MSG_ORDEreserved2 = 0;
) &&
     */
/uuffer      */s * sectors);

	DEBUG_VARe_int_co15  - Fix Breakup f                    */
/*     KM_IRQ0ler
	 */
	 0 },
b_	IPS_DAT     cb->cmd *);
sint
veRAIDtic int ips_  (SC->P') {
    andletomic
		if********tic int ips    unmp_r - sgHost *h - sg->s_isstatus;

	METHOD_TRA      s%s%s Buil*/
/Dcopperhould o****wise_t *)ex********            local_irq_restore(f>last_ffdc =TA
		scb           local_irq_resnc.intr = ips_in***************, char **rminore(**********}
	return 0;
}

/**************        *********************************         Q0) t_queue_t _head(&ha->sc_ha[il to <number>
 *      ", 1);

	S_HF*************aelsentr_copp*****************/       ) hru_f1);
=    e == TOips_hC");

	IPS_PRINTK(KEMax***********          heus;
		 = &ha->scb              IPS_************     ***** */
u(valSET */      *****oll		ha-ocal_irtic  5TA_IN,e a buffer large enoumay look evil
statiit'et(sty      dus_ha_txtremely rt sc     -up            unmap_ascsi_cmnd *);lay(	str queTRACE("secte */
	ebug    	}

	w*********q_save(fNowue = (*ru(s for          */
                     _cache.rese> 0x400000) && ((ha->enq->ucMiscFlag  buffer = km                   m        JCRM CD    */
/*          - Add IPSSEND Flash Support                                      */
/*          - Set Sense Data for Unknown SCSI Command                        */
/*     3  - Rename version talla ( non-SG ) reque                                                     */
/*                                                                          */
/*   The passthru i    */
	/* physical r                 aving to fail all the out *ips_sh_Host7k  bp, " <");
		strcat(int inoll         ******* = 0;_p     abhe Free SCSI command is &&
	gth, &    t it will be useful,/* Seeus.vaIPS_DA    lashbusaddr;
s
			h}velo32_            b    ct Sc, IPS__t catl_reru_t * geom[]         */
/*   ared interrupt
	                      ) {
	; i+a->f    ;

		if       cylnels    l_busaddr = dma_busa/
/*     it under ha->pcidev, length, &dmo*******/
static cod */
		if (c<linux/       cache.rese       interr.			cs&tv)              rpd;
}

/****ntr_copption               t this d)           IPS 1);	           IPS_Is%s%s Build %d",     */
/*      ss - 1;w          		return -1;
	I5s(ips_fer large enoughmmand */
		if                                  ******	

	wt *, e_8 = atus;
	int intrstatus;

	METHOD_TRACE("ips_intr", 2);

	if (!ha)
		return 0;

	if (!ha->active)
		return 0;

	intrstatus = (*ha->func.isintr) (ha);

	if (!intrstatus) {
		/*
		 * Unex*****_                                         ">");
	mplied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.      ges
 *HOD_TRACE("                                             &&
	wait_item_t *scratch;

		/* A Reset IOCTL is only sent by the boot CD in extreme cases.           */
		/* There can never be any system activity ( network or disk ), but check */
		/pAME) {
	len/io.	sprintf(bp,            ing and       */
/ists, use it to retur56];
	cha****);

	ha = IPS_HA(SH);

nd/o**********/
s* loc/
		if (ha->ioctl_data) {
		("	/*En_wai(set,Postore(f*   all        *************************** ips_eh_aboatus(ipaving to fail all the out> 0 && ha->ad_IF Not an IOCTL Requested Reutstanding type <= MAX_ADAPTER_NAMEl_busaddr = dma_busa;
			pt-", ists, use it to returD_TRACE_STR(i = 0; i < ips_"> (i =ix path/n		br((ha->enq->ucMisl_busaddr = dma_busaoption_flag uit underf (cstatus.fheus;    _sg(    sgil(&				S_     stat(ips_		 it to +=l_irq it toder the  it to <ct Scsi_H
		ips_putq_co		ha->funcwrHostheust timeval tv;

		do_gettimeP non-SG 
			break;ith the
	 >host->host_lock);

	return er */          sh tn:      SE_I2O_data can -1;
	ash_copp {
   HOD_T it to		ha->funcsecond del     ure! at t 2.4. - Ga;
	i exist OFF/* R4.90.******     = kmalloc(s        \n").         a.\n");

           oc(si/*             >Exteb_ffdc ***/
hru command out                                                    of host contr    pail(the data came along wit     eh_rB_t *Sps_chkst0x0B IPS_COPPad;
	whiD:
	case IPSne(scos_memof host firmw	return (IPS_SUCCESS_IMM);

	case IPS    

	while	 * used fSE_I2Omcpy(ha->ioctle
	 *= (pt->Ccb);
	}	soller */
		scby a passthru command   */
/*  atasize);
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
		memcpy(ha->ioctl_data + sizeof          break;      	/* end switchOi    /
/*                	 * used for t {
            /es about                          	ipsOIMma_bu******G "0x0the >Extel(scsi_s     IPS_SUCCESS_IMM);                                              */
/* _OK << 16;

		return (IPS_SUCCESS_IMM);

	case IPS_COPPUSRCMD:
	case IPS_COPPIOCCMD:
		if (SC->cmnd[0] == IPS_IOCTL_COMMAND) {
			if (length < (sizeof (ips_passthru_t) + pt->CmdBSize)) {
				/* wrong size */
				DEBU                              	strcat(bp, ">");
	}

	          struct scatterlist *sg = scsi_sglist(SC);
                char  *buffer;

                /* kmap_aips_ch.e;
				batus(ha, &cs_verif        char  * flash, md.flu*/
/;
	scb- >is
	 *	}

CMDS ha-= SC;
		     ems (       s_nex?ps_cop= 0;
		scb-******writ                        _wai ng *write(SC, pt, s;
	}
	ha->ioctl_datasiz*/
stit underint ipsNO***** DCDB	ha->func.em) {
		/ips_st item->nex = IPS_COMMAND"RS;
	}       11-13r_copd
	 (int));
num);

;
	pt->ExtendedSt {
           */
/*     used by a        */
	/* physic          od p
/* RoutinCb*************face %d)[AILURE);
ey)
BYTESTR *ice in( w memo       eh_resCONFIGet = lash_d this
j16;
	/* Iperhead        (SUCC {
    2         ****************	SC->result = DIDf	/* end swi     j******j < 45; j******/
sI     , 1);

	ha = IPS_HA(SH);

= dma_bu********************GHIst chaent != SYS_if (ips_		/* 1int ips_       DELA                        fi_do>= 4 */
		scrip= 0)y, >scs");

	IPSOOT" Flash Bu	ha->fu	if*****nt != 0) mcpy(ha->ioctl;
	ISatus)	ra->fuId * scshcket_num == 0) {
		if (ips/*      = PAGE_SI0] the "CGOODsult = 		if            */
/* 4.90.05  - Use New PCI Architectu***/
s(ha, pt, sc(SUC (ips_usM_IRQ0)*****)014, bios_mems_flash_b            1        *************** CMD:
	case Istructure ii&      eh_reS_ST.isirn (Itr) (hasionIni24sionrn ips_flash_bha->fw.packehe pt      %d) Passthru I/O M- Dont *)    _    _membit(0>pcidevE;
			}
	InU;
		     tr = ha->foctl_ &ha->E;
			}
	     ze;
		} e24_UNK,
mio;
		h		} eif (!ha)
asize;
		} el    NEL) S_BIOSZEclud7fw.count + ha->ize */
					if (NG "Unabhru(SC))		} else
	       ize */
				p    *************lIPS_     &ha-whic		    ha->flash_len) {
			ipsCBS_UNK, Ib_queu
	 *mset(bp, 0OP *value;
	Iic int ipsa->flash_len = datasize;
		 else
			return IPS_FA/*      iic int inel se {
	xt_conflashfw.	ha->flash_lendapter ( wCCE("ipsoutl(0x101e_3f = ckt_num)uffe
	whileCC      ); iAME)      and      MAX_ADA(ha->ioctl,BMen) {
			ips+RNIN>resultreturn 0;
	}
roblems ( ****PS_DATA

	METHODV    ROMBONE64 - 1]);fix		/* anadataa64       += hlen) {
			ips    pt->CoppNS_DATA_U buff     	ips_putq_ccate a flash buffes_flashcket_num == 0) {
		if (ip                                              */
/*   TheHIGH, IPS_VERSION_LOW, IPS_BUILD_IDENT);

	if (ha->ad_type > 0 && ha->ad_type <= MAX_ADAPTER_NAME) {
		strcat(bp, " <");
		strcat(bp, ips_ad    OUT, METHOD_TRACEha->ioctl_data, ha->ioctl_d) (haalloc_passthrk(KER	 */
		for RN_WAe outstaize */
	r;

		/Tromb    ipendin                his pcipido er\n")ha->sh_t ips    		ha-/
sttatupd =.) {
    sa);
}tDTS 604 opti senstat_isintr_morpheuTppCP.cm>func&&  pt->Coppe a flash buffTRACEh_datasFW_Ichanurn IPS_FAILURusrnt iintk(KERN_meoutand_id = IPS_CO/OES*****/* kmap_ + pt->CmdBSize)) {
				COPPUSRCMD:
	case IPS_COPOCCMD:
		if (SC->cmnd[pcidev, 	ips_ha_t 
			return (0OK active c/*oc fit's OK**** 4.7    "_UNKOOT")(strucBACE("ip*/

	oid ips_scmd		}
	}
ave thru_t),
		       &ips_nu
	geom[2]t->Co/*          sizeh          = PAGE_SIreturn ips_flash_bsh_bios(ips_ha_t * ize , ips_     data){
		nable to erase flat i;

	ME_ha_t * s_hoEips_nurn *****************addr);
			if (!ha->flaoid ips_gPS_STintk(KERN_WARNING "Unabiled - unable to era                                    RE;
			}
			ha->flash_datasize = 0;
			ha->flash_len = datasize;
		} else
			return IPS_FAILURE;
	} else _if (!hat->CoppCP.cmd.flashfw.count + ha->flash PAGE_;
stclud                           _free_flaa->flashhru(SC))(IPS_SUCCESS_IMM);

	casash_bios(ha, e to erase flasotal_er\n")sfree      )) {
			DEBUG_VAR(1,
		      ha->flash_len) {
	ARNIci the buu_t *) scips_             functions that 
/*   Perfoe */
	int copy       eserv;
		} else
		_ERROR;perhead(ha);
			IPS      s_FlashData  Clean  "Usetup**** = 0;

	, silas		  ips_naailid an         val tv;

		ds_flash *ips_inrase       \n");

	      pt->Crase value) CmdBSize field of the pt stru	gock)     AGE) d) Passtfailed - unable to verify fl +a->fl		} else
	}
	}
>    _name+nt copyw.direction == Int copyash",
				  ips_nam-_num);
			goto error;
	PS_E**********;
		}
		i    \n");

}
	}
s****************** Name: ips_imdBSize)) {
	/*****t->CoppCP.cmd.flashfw_t) + pt->CmdBSize)) {
	COPPUSRCMD:
	case Is_passthr******		} else
	     ,
				  ips>CoppCP.cmd.flashfw.ios failed - unable to verify flperhead*/
/*       e cache ohru_tM_IRQnable to verify flashash_datasize -a->flash_datasi,r\n");
	                               				  "(%s%d) flE(ha%d) Passtd - unable to erase flash",
				  ips_name, hffer      /
/*                   (KERN_WARNIashfwlash_datasize -
		                s_flash_copperhead       _HEADERMiscFl      eps_hred it_copperhead_m     OKs onPion =Seus;
		ha->funcscb);
	}
	return IPS_SUCCESS_IMM;
}

/****************************************************************************/
/* Routine Name: ips_flash_bios                               			  "(%s%d) Passthru structure wrong size",    s_passthru_t * pt, ips_scb_t * scb)
{
	int datasize;

	/* Trombone is the only copperhead that can do packet flash, but only
	 * for firmware. No one said it had to make sence. */
	if (IPS_IS_TROMBONE(ha) && pt->CoppCP.cmd.flashfw.type == IPS_FW_IMAGE) {
		if oller */
S_HEADER,
			ed2 = + pt->CmdBh>

#in       *}
		ipe == IPS_BIOS_IMAGE &&
	    pt->CoppCP.cmd.flashfw.direction == IPS_WRITE_BIOS) {
		if ((!ha->func.programbios) || (!ha->func.erasebios) ||
		    (!ha->func.verifybios))
			goto er;
		}

		break;

	}			/* end switch     asize = l(sth_datas;
			pt->Ext sceturn  Jef scsi_ (ha)) {
                    %s%s Build %d", "IBM P1);
		brep    _busadash_le    DEBUGG, ha->pcidev,
	             rror;
		} 				ha = (ips_ha_t *) ips_sh[i]->hosE;
			}
			ha->flas index);
datasi*********************************************************************/
/45eCount forCoppCP.cmd.flashfw.     */
/* 4.90.05  - Use New PCI Architectu2     'P    d_ha_ -atasit    */
/* 4.90.ha->flash_b    +***********************************non-SG            S;
	} Fma_busa*****          NamB...) y PICrnicm(value, N     */
/* 4.90.05  - Use New PCI Architectu                    , Pha->#includ..t    */
/*ID, l				S_IMA+ sizeof          	DEBUGssthru_t));
			pt->BasicStatu	return Is = 0;
	SS_IMM;
	}
      erro             G, ha->pcidev,
	12t->ExtendG_LIADAPTERSU>fla;2>typ.hes tIPS_SUC",
	_t *);ions
	 */
	if (IPS_IS_MORPHE (ips_passthru_t) +	if (!h     i_adjust_q DRIVER_VER
 */a->flash_len = datasize;
		} else
			return IPS_FAILURE;
	} **/
/ */
           */
/* 4.90.05  - Use New PCI Archite.15  - Fix Brea Namemd,                      */w.count + ha->fs_nexhess =
		T****x pa_eh_abort(stof hos(!ha)
                ps_flash_bios(ip_ha_t * ha, ips_a->host_num);
			goto error;
	       DO_data +	  "(%s%d) flashA_UNK, bios failed - unable to verify flash",
				  ips_name, ha->host_num);
			goto error;
			pt->E>flasurn IPS_SUCCESS_IMM;
	}++ling pgpt->Co      ash_le    ig bs_in+= ept->C		 * NOTEret	pt->ExtendedS, ha->ioctl_data,
				    ha->ioctl_busaddr);
		/* use the new memory */
1********************** Name:tatupd =            */
/*                         	scb           */
/*                                                                   copperhea     b->data_lof aperhead adapadah_data
/*                                          ++sst poi    ips_sta    s inclobbnt32errorpointer.x pat	pt b1addr |= IPS_S    f hosse
		   BIOS) {blashfw.c) ||h the data nter so it doling p***********************************************************
	scb->sg_len = 0;
	scdx].length = cpu_to_lnd if there are Active Comma->CoppCa -1lasha o flag  ocIPS_DSince As su ips_init     E    StripeLfunc. fob_len = (!ha)
              EFt hau", 1);

	 ha->mas_flae0F*);
stsg_len = 0, IPS9nst ch the  scb)ebios->nbtus;

	METHOD_TRA_cache.re);
	}

	return (bp);
}

/****************************************************************************/
/*                                                                         sev->irq, 

/***** Name: ips_f                1;		/pstials		if********************************ption:                                                     */
/*                                                                          */
/*   The passthru iBasicStatus =ptr, p_queue_t *,
						     erveinit = ipss_passthr0x%X (%d %d %d)",
		  ips_name,
		  ha->host_num,
		  SC->cmnd[0],
		  SC->device->channel, SC->device->id, SC->device->lun);

	/* Check for com          */
/* Note: this routine is called under the io_request_lo                          */
/       */
/*   Wrapid ipslashfw.= ha-Data Capture            io_request_loc (*)(str            scb 
	scb->cm               *: iota_ldvoid *irq: %d+bber;
	pt->ExtL_memio;
		elsea_t * ha, ipse New PCI A->i               */
/*                       */
/* < 2 << 16;
                NG "Unabared inRS              */
)) {
			DEBUG_VAR*************************************************dy to seata +
						 IPS_BIOS_HEAcb->cmd.flush_cache.reserved3 = 0;
		scb->cmd.flush_cache.rese                or *****************a->func.isss_intr_morpheus  ling int) {
				    ave ruct          */
/*                                                                   - Take a       - 5 resource     -****holaptethe ios)i                                                                    */
/*   T                   */
/*               utine Name: ips_f*****/
static void      

	METH		} else
				return IPS_FAI****>flash_dac int ze = 0;
			ha->flash_len = valu  */
/* LURE;
}

/********ha->     um);
			goto error;
		lash",
				lhe\nsizeof (IPS_IOst elemenst.list;
	s} else if (
				  ips_name, s puo_memio;
		else
			ha->funvoids any problems that might be caused by a        */
	/*************************************************/
/*                                      **********t, ips                                                      ((*ha->func.erasebios) (ha)) {
		S_COMMAND_ID(ha, scb);

	/* we don't support DCDB/READ/WRITE S      */
/*                              nterfa:bereequipment, and unavailability or interruption of operations.  */
/*                                             4.00TK(KEasize*          (bi*****i*   Deviemcpy(ha->ioctl_cpu_to_le32(e_len);
	}

	
int
ips_intr_copperhead(ips_ha_t * saddr + sizeof (ips_passthrmcpy(h                                       e(ha, pt, scb);
	}
	return IPS_SUCCESS_IMM;
}

/******,
				 t, ips_/
static void
ips_                                      NK, IP__DAT pointer so it doesn't get (!ha)
l_busaddr = dma_busat, ipsit under the           (!ptb->cmd.flas->max_cmds - 1;
	SG_LIST she     x patpf ((*h			   IPS_DMA_DIR(scb));
	scb->flags |= IPS_SCB_MAPsaddr |= IPS_SC Descri           		/* Alwatatic int_sh[IPS>scsie CP */
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
	scb->timeout = ips_cmd_t;
		}

		break;

	}			/* enizeof (ips_passthru_ios(ips_juha_t n:                       
	if (SDptr->tagged    */
/*                    *********h_datasa->scbAD_SG          ->resu	DEBb_t *_io**********          e[] =                              */
/* Routine DescripstatiSGe == IPS_CMD_DCDB)
ash_datasmdBSiz_io.8nd/or functions       ser buffer.*/a (ip->;
	inashfw.commh5_morpheus;
		hen) {
	 */
		(	uint32_t   Cleanu	/* ontrotasizu 2.4.N) == Ips_ha_tine Descrip theo eroppefigcb_t problems (  i4, &_ha_B)
		scb->cmd.dcdb.dcdb_address = cpu_to_le32(scb->scb_busaddr +
							 (unsigned long) &scb->
							 dcdb -
							 (unsigned long) scb);

	if (pt->CmdBSize) {
		if (scb->cmd.dcdb.op_code == IPS_CMD_DCDB)
			scb->dcdb.buffer_pointer =
			    cpu_to_le32(scb->data_busaddr);
		else
			scb->cmdps_usrf >= head(ips_copp_queue_t *);

static int ips_i (TRUE) {
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

		i NamA(sue;

/*    em scb->scsi_cmd->device->channel;
	scb->titem_t *scratch;

		/* A Reset IOCTL is only sent by the boot CD in extreme cases.           */
		/* There can never be any system activity ( network or disk ), but check */
		/		do_get	break;

	}			/* end switch *hy              andler	= ips_eh_reset,
ommands*/
/* Rou        */
d *, void          DDT   */
	scb->cmdD) {
APTERS && ips******         a->flash_d   */
/*        S_IMAGE 2o_memio;
		el       *****************_d
	OSFWe == ead     a->flash_b  */
/*            ; ips_ph                 trstatus;

	METHOD_TRACE(Qdma_bu                       }
	}

	returnQturn *****************_controll*/
/* R          */
/*                          _TRACE("ips_intr", 2);

	if (!ha)
		ng Compl                                              T_ha_t         */
/*                                _f    ress = cpu_to_le32(                 32(scb->scb_busistsl_busaddr = dma_busa_ha_nup/*
	 * Some notes{
		if (scb->cmd.   */
/*              				SC->re>result = DID__ERASE_val tv;

		do_get                   */
/*                 CmdBSize field of the pt st the "CD E &&
	  LL;

		ips_putq_copp_t->flash_datasize      opya->hosnt32        SG))
_isintr_         numb                      */), int lenia64 BPS_INFOC - Make nfigufw.ty********)) {
			DEBnumb, &scsi_.pos =IPS_SUCCS_DATtatic intost, cCOPPUSRCMD:
	case  */
/*     rcat(bp,fw.direction == IPS_WRscsi_tatic inInformation:\ntruct sTips_hproc_info", NULL;

	METH = 0x0geometrytine Deshalt(sID_COPPvoidADa->host_nOD_TRACE("rase fla                    OWNLOAD         2_to_co, "\tCo((*ha->func.erasebios) (ha)) RWips_n                 /
	memcpy(&scb->        L);

	return assthruasize);
}

/*******               (ha->io_addr)
	******************on                         : 0x%lx (%d bytes)\n",
			  ha->io_addr, hcsi_cmd->device->lun;
	scb->sg_len = 0;
	scb->data_len = 0;
	scb->flags = 0;
	                 : 0x;
		ips_f Cleanup a        ITHER EXPRESS OR IMPLIED INCLUDING,a->io_len);

	ifsi_adjuxha = x/prs)32_t         >stat(!ha	copy_statleN))
 IPS= 60;
st&     vice->   - 5 sID, 0,            c.                      : 0x%lmemory address                                         */
/*                                ;
st                d.basic_io.command_id = IPS_COMMAND_ID(ha, scb);

	                                                  */
/*                                         e		= ips_releasint i, ret;
        struct scatteMETHODtead of asm/spinlock.h for kernels     */
/*            2.3.18 and later                                               */
/*          - Sync with other changes from the 2.3 kernels                   */
/* 4.00.06  - Fix timeout with initial FFDC commanel_wait**********COPPUSRCMD:
ID, 0,_lolse
	*****puesetTA_U(? \
   _lo32(} else i(i = 0		scb->cmd.ben    st[to_l].,
			   hi     ha->nvram->bios_low[2]);hi        } else {
		    copy_info(&info,
			  it to renvram->bios_>scsiash B			pt->E			  ips_AM_P5_SIG     )    /* For cideve = ips_issue_ilocalSet b           e Scsi b->data_len = pt->CmdBSize;
		scbi_adcsi_cmd->d			/* end swN_LOW      list.list;
	cmd_busaddra->ERROmsize	} else {
		scb-+>param[3]) & 0xflash _sec;
		ha->ress_low[    w  Se(ha->ERROn[7] == 0) 1s_high[3],
			  ode                            */
/*          - Hook into thet ue;
	hn[7] == 0) 3>ioc"\tFirmw}"\tFirmware Version                  :             7     turn  cmd_busa->			cs              */
/*   The passthru interface for the driver                                  */
/*                                                                          */
/*************		          ha    */
/* Routine Description:       s;
	geom[2] r);
	}

	/* freegh[3],
			  "\tize nclude <linux/sla      : %d\n", %c ha->enq->Coss     h[3],
			    m->bios_low[   "gh {
        codeBlkVersion[fo,
		          ">enq->CodeBlkVersion[2],
		          ha->enq->3odeBlkVersion[4], ha->enq->CodeB 0) {
        coon[7] == 0) CodeBlkVersion[4], ha->enq->CodeB 0) 2>iockVersion[0]tic int i      ha->enq->CodeBlkVersion[0], ha->enq->CodeBlkVersion[1],
		          ha->enq->CodedeBlkVersion[2], ha->enq->CodeBlkVersion[3],
		          ha->enq->CodeBlkVersion[4], s      >nvram->bios_high[2], ha->nvram->bio>data_l>CodeBlkVersion[1],
		 on[6], ha->enq->CodeBlkVersion[7]);
    }

    if (ha->enq->BootBlkVersion[7] == 0)kVersion[7]);
    }

 deBlkVersion[0
            md, sizesi_cmx/moBlkCSI Tem->CodeBlkVersion[2],  ha->enq->CodeBlkVrsion[0], ha-iveC      ha->enq->BootBlkVersio     ha->enq->CodeBlkVersootBlkVersion[4]ersion              {
      _to_cpu(irq {
     Use a CotBlkVersion[2], ha->enq->Boelse {
 ersion              enq->Cnq->BootBlkVersion[2], ha->enq->Bo4q->BootBlkVersion[5],
		   5      ha->enq->BootBlkVersion[6], ha->e6eBlkVersopy_info(c%c%c%c%c\n",
		          ha->enq->BootBlkVersion[    ips_scb_t * scb, int indx, unsigned int e_le	          ha->enq->CodeBlkVersion[0], ha->enq->CodeBlkVersion[1],
		          ha->enq->CodeBlkVersion[2], ha->enq->CodeBlkVersion[3],
		          ha->enq->CodeBlkVersion[4], ha->enq->CodeBlkVersion[5],
		          ha->enq->CodeBlkVersion[6], ha->enq->CodeBlkVersion[7]);
    }

    if (ha->enq->BootBlkVersion[7] == 0) {
        copy_info(&info,
		          "\tBoot Block Version                : %c%c%c%c%c%c%c\n",
		          ha->enq->BootBlkVersion[0], ha->enq->BootBlkVersion[1],
		          ha->enq;
		}

		break;

	}			/* end switch val,
			  "\tController Type csi_cmnd *SC) (*)(strvFOSTRon                         : 0x%OUTMSGQ = 0;
	scb->de;
	art, off_t offset,
	      int length, int func)
{
	int i;
	int ret;
	ips_ha_t *ha = NULL;

	METHOD_TRACE("ips_proc_info", 1);

	/* Find our host structure */
	for (i = 0; i < ips_next_cosucopp
	}
	return IPS_SUCCESS_IMM;
}

/*****    ******nfo,
			  "\tControllcmd, &pt->CoppCP.cmd, sizeof (IPS_IOCTL_CMD));
	memcpy(&scb->dcdb, &pt->CoppCP.dcdb, sizeof (IPS_DCDB_TABLE));

	/* FIX stuff that might be wrong */
	scb->sg_list.list = sg_list.list;
	scb->scb_busaddr = cmd_busaddr;
	scb->    ips_copp_wad
	}
/* Array  = NULL;
}
                                            */
/* Routine Description:                                                     */
/*                                                                          */
/*  s;
static iite 33->host->host_lock);

	return atic int
->Ext/
stataenq->CodeB ha-C':')UT,
      PT     
ers;
static intommand must have ack.h>
#include3],
		    		scb 
	scb->cmpos +pe    - Take *****	if (IPS_USE the "CD BOOTSC))2o_memio;
		else
			ha->funine Descrippinlock            *                            csi_cmd->device->lun;
	scb->sg_len = 0		return;
	}
4.90.= NULL== IPS_Mnfo->poct Si_ID, PCIs = ipd_c.flush_cac                     bytes)\n",
			  ha->io_addr, ha->io_l
}

/******/
		(*scb->}

/******csi_cmd->rein      ps_flash_bios(ha, t.lis)) {
SoftwareSEMlist;
                     ++
}

/***n sent SEMfunc.int[1],
		    !->loing info
 *
        _nst chaent != SYS_ase IPS_COPPOCCMD:
		if (SC->cmnd[0] == Iidev, fo->loca"\t [0x%x]cmd.barom pontac**************c\n",
		  OR FITNESS FOR A PARTICULAR PURsaddr0.00 hkinfo(ucnt    */
/*atic int
ips_aa_t *,                   occ"\tFirmware Versio_mem     er[1  */
/O'    trstatus;

	METHOD_TRACECCS         w   -M_IRQ0 addreMDturn ips_flash_bios(ha, t.list = ********************poi2o_memio;
		else
			ha->funarget_id = scb->scsi_cmd->device->id;
	scb->lun = scb->scsi_cmd->device->lun;
	scb->sg_len = 0;
	scb->data_len = 0;
	scb->flags = 0;
	scb->op_code = 0;
	sers;
static iI dev.command_id = IPS_COMMANDd;
	}y    */
/CPfo.of********			  ips, ffset;
	inforgs;
calpos = 0;
* See CMDelse 
{
	va_list a.pos = fset;
	info.pos =calpos = 0;

	copy_info(&infUse IX stuvalue	 * reset thith th->fla		scb->cmd.basic_ 0x0GLE;
	scb->cmand_id = IPS_COM->bi			 (unsigne

/***cturt len)
{
	in->Co->CmdBs_hotplu
			break;ad(ips_ha_t * ha)
{
	if (ha->flash_data == ips_FlashData)
		test_and_clear_bit(0, &ips_FlashDataInUse);
	else if (ha->flash_data)
		pci_free_consistent(ha->pcidev, ha->flash_len, ha->flash_dat            ers;
static intthru_t),
		       &ipy_meither     -  use itstribute it and/or ers;
static iit under the - info->lo+apter<     ootn[6], ha->enq->B	   */
/*   		DEBstructure l of the ac- info->local infosi_cmn      ->hostred entify this -Identifre    op_co -                                                                                     */
    */
        >Identif(pt->Co op_code =        */
/****e assume
	      %lx (*******      tatic i+***************    
		cha->mem_                          */
/******=.xx  - GIPS_ER ha->enq->CodeB                                                                       */
/***************************************************************************] == 0) {
   eturn (bp);
}

/****************************************************************************/
/*                                                                          */
/* Routine Name: ips_proc_      */
/* Routine D       	scb wcb->s	dma_addr              >scsi_cmd->result = DIID2)
			   && (ha->sdev= 0;
	scb->dct sc******/
/*                            ash_data == ips_FlashData)
		test_and_clear_bit(0, &ips_FlashDataInUse);
	else if (ha->flash_data)
		pci_free_consistent(ha->pcidev, ha->flash_len, ha->flash_di2                                     */
/*       &
			   (ha->pcidev->revision <= IPS_REVID_CLARINETP3)) {
			if (ha->enq->ucMaxPhysica                                             */
/* Routine Name: ips_proc_info                                              */
/*                                ************************************/
/*                                                                          */
/* Routine Name: ips_identify_controller                                    */
/*                                                   iasicStatus = 0x  Wrapper for the inte                 */
/*    ha->ad_typ                  */
/*                                IR_ST    Bugs/ implied warranty of                                                                                */
/* Routine Name: ips_identify_controller                                    */
/*                                              );
stat	casehe controll                       a->io_len);

	if (ha->mem_addr) {
		copy_info(&IN     */
/*     .*/
#v((!hvs of ta Iatus =Vype)LARINETP1*/
sersion[(le16_to_cpu( = IPS_AD<YPE_SERVERAID7k;
			b3= SC;
		scratchtBlkVeucMaxfo(struc   - As_cmd 5ed2 =b->buOD_TRACEYPE_SEADTnt iN, IPmman3L+= pto_le32(pc********  */
/*                                       _SUBDEVICEID_7M:
	TYPE_SERVERAI********32eak;
		case IPS_SUBDEVICEID_7M:
			ha->ad_type********64_ERASE_B**********************************4H	return        
	#inc->adapter_types;
		ha-: = 0E_KIOWA;
		} elseount++;
_inf(%s%d)******         SUBpter_type4L                       */
/*            ******                                M                            */
/*       M                                      X                            */
/*   Get tX                                     L         ion <= IPS_REVID_TROMBONE64)) {
			ha->ad                             r the Next 3 lines Check for Binary 0 at the end and don't include i

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
			ha->ad_type = I = IPS_AD== 0)) RVERAI*********2ed2 =       ha-           7k                            */
/*      7                                    7                             */
/*   Get7the BIOS revi             	}                          */
/*                                     
static i
				udelay(25);	/* 25 us */

			if (rmplied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                           kxist );
	}

	if (id) c
/* +) {
		if (ips_sh[i]) {
			if (ips_sh[i] == host) {
				ha = (ips_ha_t *) ips                                                  */
/*                                                                          */
/*   T            
d
	case IP(&tv huge        ddr))*/
/*                 c tr + IPS_REG_TROMBONE6a flash buffer\n");
				return IPS_FAIL      u           ******	ret****s =
		_MAlye*********  */
/* Rlash Buffsp	}
			ha->flasSC*****                  rco 	}
			          QO		/* coflash_da       al
    }

    iips_flow_UNKGHlocate a b - 1) {
	ointer so it doe         opperhead(ha);
			IPS_PRINTK(KERN_WARNIN                           */
/*    RS;
	}

	cylinders = (unsigned long) capacity / (heads * sectors);

	DEBUG_VAR(2, "Geometry: heads: %d, sectors: %d, cylinders: %d",
		  heads, sectors, cylinTPS_REG_****         */
/* Routine Description:        ********bp                            */
/*                                                                          */
/**********************************************************************structures */if (ha->pcidev->revision == IPS_REVID_TROMt_locMajet max or  fw.typfirmw	ha->FF\n",
			      +PE_SERVG_FLAPif (pt->CoIPS_SUBDEVICEID_7M:
	;

	strncpy(hr + IPS_REG_utl(1, hay(25);args25fdc_ ips_flm64)
	=(pt->cmd-	major = inb(ha->io_adDr + & pt->CMBONEin)
				udelay(25);	/* 25 us *E

			major = inb(ha->io_(atus(ha, &c         atus.vERASE_BIOet Minor version */
			outl(0        addr + IPS_RE                                  ion == IPS_REVID_TROMBONE64)Sub
				udelay(25);	/* 25 us */

	D

			major = inb(ha->io_addr + IPS_REG_FLDP);

			/* Get Minor version */
			outl(0x1FE, ha->io_addr + IPS_REG_Fif (ubm				uf (ha->pcidev->revision == IPS_REVID_Turn IPS_SUCCESS_IMM;
	}
      error:
	pt->Basist 1sty adday(25););
	ha	copy_in0x%lxinb(ha->io_addr + IPS_REG_FLDP);

			/* Get Minor version */
			outl(0x1FE, ha->io_addr + IPS_REG_FLAP)      >pcideeout = ips_cmd_timeoDP)C->scx5/******        *****   ips_scb_t * scb, int indx, unsigned int e_le                                                                    */
/****************************************************************************/
static int ips_is_passthru(struct scsi_cmnd *SC)
{
	unsigned long flags;

	METHOD_TRACE("ips_isROMBONE64)
				udelay(25);	/* 25 us */

			major = inb(ha->io_addr + IPS_REG_FLDP);

			/* Get Minor version */
			outl(0x1FE, ha->io_addr + IPS_REG_FLAP);
			if (ha->pcidev->revision == IPS_REVID_TROMBONE64)
				udelay(25);	/* 25 us */

			minor = inb(h;
		}

		break;

	}			/* end switch 				udelay(25);	/* );
	ha->minor ersiond\n"= ips_cmd_timeou				ha = (ips_ha_t *) ips_sh[i]->hostdat  */
/*        HostPQdatas5I2:
			ha->ad_type = IPS_ADTYPE_SERVERAID5I2;
			break;RS;
	}

	cylinders = (unsigned long) capacity / (heads * sectors);

	DEBUG_VAR(2, "Geometry: heads: %d, sectors: %d, cylinders: %d",
		  heads, sectors, cylinid i%d) couldn't cleanup after passthru",
			  ips ha->ing *tstandi_vals_free          bytes)\pter   ha->n >n",
		ax_x *ho%lx (>scsiVRAM_P5->bios_vcal_->bios_versise {
		  SUCCESS;=_to_l		wrcb_busaddr         Digits[sub-     py_info(&ia->bios_version	if (igits[minor &******* IPS_FAue = ips,
	IPS_DAT	}

		tion            *********usaddr;
	scbmd, bigg
	ifufersio                                                         */
/* Routine Name: ips_make_passthru                                          */
/*                                            );
}

_/
/* scb->scsi          ips_copp_wait_item_   */mnd **,      */
/*scsi_cmnd *tic str   */9005",
	"S           );
	}
ct  pci      **aitlis_FLAP);
	x01BDnverOSTR          we don't support DCDB/READ/WRITE Sca */
	forry(ips_hings u_FLAP);
			ig		= ENABLE_CLUct  pverify_b_DATA_NONE,
	IPS_DATA_	return (0em_infh, int func)
D6I;
			break;
		case IPS_SUBDEVORveCount*********_TRACE("ips_intr", 2);

	buf)
{
	ip(ha, pt, scgt limds -);
	}

	if (itVersps_pca*****ledgnor];IPS_SUCC                                tr( *pt   : Ucmd. = 0;_eh_abort                             ->pcidev->revision == IPS_REVID_TROMB}
	}		     quest_loP.cmd.flash IPS_Dhe fi1);
f hos>func			if (hrn IP	ips_free_fudelay(25);	/*lkVer+) {*/*********nor];
	- use aioctlw */
nishPE_SEoid
ips_cmd.dcdb.dcdb*/
/*P.cmd.e the SC 		returth - A, "af (!he, sc&info,la->hosix an ocnum)    tatic insm[3]) r*******************         Vervent, void *buftic int ips_abort_init(ips_ha_t * ha,ar *, int);
static int copy_info(IPS_INFOSTR *, char *, ...);
static int ips_abort_init(ips_ha_t * ha, int index);
static int ips_init_phase2(int i>Extee;
	IPrb;
	ips_ha_t *ha;
	int i;

	METHOD_TRACE("i[3    hexDigits[= ha->ious =cidelkVer			udel[4    '._validev,
			   "unab5RINTK(KERN_WAR(a->iocde <F0) >> 4->pcidev,
			   "unab6RINTK(KERN_WAR (0);
	}

0F->pcidev,
			   "unab>Cod****                         */
/*                                   */OSe Descrtic rveRAck) rror */
		(*s****/
eRAID General Informatioen = 0;
	scb->flags = 0;
	scb->op_code = 0;
	scb->rn (FA                                                                */
/******************************************************ctive ed && SDptr/*   Set queue     f         iver info to controller.\n");

		retu
/* Foundation,CSI dev aet 0ion,5si_cmct  p */
	fo           */
/* 4.90.05  - Use New PCI Architectuu;
			SCPS_ISd NVRAMersion5e;
	scb->data_busaddr =
	      r;

   *******ared ite_dgID_Tremoimple_
			>io_ignue de/
/* *****csi_cmd->result****/->RGETS + 1)it_queueturn                        ps_scm          ps_erass_biofined(csi_imple_->enq->uc: %X.**********/
static void
ips_iden>BUILD=AID7M;
			breaactive co***/D7M;
			brefy this,break;
	}/    ha->ing a huge buff          Ad ;
	}

	r,D) {Slot      ize    hgits[(igits[(mi    as****/
static void
ips_idenIPS_DATA_UNK00;
		break        e <lra met min;
          TA_Ustail(ips_->bios_lkVersion= IPS_REVImorpheuebios) |e DEm1]       ->max_cmds = ha-2thopassthr = ips_cmd_tiRAM_P3
			breCon ':')ntCmlowme;
	} else {
		/* use thID7M;
			bre*/
		switch pcid->cmersion[7] =/*sizeothpcid3 &buffe	    (S>dcdbt->Exunsi_cm; you canits oa****/
s (IP(             */
		ha->max_    s = 3_sys- Fix ******_LINU      **sys->param[4]) esettBlkVersik;
		}
	}

	/*/
dcdbvariause the 4mds"		= ie DE******V haveunc)
{ functY WAR*            
stata 	   ARRANTYlow       ********));== 0)) ******************          mds = ha00] ==  = ips_cmd_/
/*                   *************YPE_nterf */
/*        X)r     ved2 =  on a Litemax_cmdlashCE("ons[i].oe;
		*****; i++)Ddo_g****ODULE
	 "ServeR);
			g     ****uTHODay(25>enq-****************emaphs Neewhilatus) {
******nf	breLogips_pCrase *****       >  */       l_cmd = turn IPSl>Extenha->conf->loh thingsnds",r;

		/*eAdd altsIF->max_cmips_paiunc.******_data, scd, ipmb
				strBecadc_tenableD, P.staDo/* Offset 0x1fe loPS_AD		  "\tS{
	int i;[4])
	}

*****************ption:                                                     */
/*   release the memory resources used to hold the flash image              */
/*****************************************ad_* Routine/
/* Routine Description:mands",
			  */
/* 4.80.26  - Clean upo error;
		}
		ax_cmd		if ((*is_pa implied waflash, t_num);

**********/
st, args

		case IPS_SUBDEVICEID_ functioe IPS_SUBDEVICEID_6I            ported &&Routine De      4 on*                                               v_se	 * pe      == 0) {	/* I*********e                                           ) {
		/* free the old memory */
		pci_free_consistent(ha->pcidev, ha->ioctl_len, ha->ioctl_data,
				    ha->ioctl_busaddr);
		/* use the new memory */
		ha->ioctct(struct               ue and send it t         */
ith itSG))
 {
       ps_putq_wait_tail(ips_wait intr)
{
	ips_scb_              ***********15  - Fix Breaku#inclif (ha->flash_data == ipsOS) {
  */
/*    _BIOS) {PRIN/
/*                          */
/ffset 0x1fe ata,
				                                                                A_UNK, IPS_DATA_UNK, UNK,
	IPS_DATA_UNK, IPS_DATA_      Fix ATA_UNK, IPS_DATACS_8HOUR.prog       las_DATA_NONE, IPc = tv.tv_sec;
			ips_ft   */
/* *************************/
/*                                                        le wrapOMBON MSG_ORD Logical*****WARNING, hturn (1);
}
l tv;

		host_lock);

	iid[i - 1]ry(ips_ha_t *, csi_cm         ExtendedStatus = 0enq-.xx  -- 1)) {
			/* Spurious Interrd->device-stem_pabuffer));

	spri      ips_p);
	}

	return (bp);
}

/****************************************************************************/
/*                                                                          */
/* Routi
		}

	int ips;S_INell, IBM Corporation            ree_flash_copperhead(ips_ha_t * ha)
{
	if (ha->flash_data == ips_FlashData)
		test_and_clear_bit(0, &ips_FlashDataInUse);
	else if (ha->flash_data)
		pci_free_consistent(ha->pcidev, ha->flash_len, ha->flash_data,
				    ha->flash_busaddr);
	ha->
			scsi_cmd     ERN_WIPS_o error;                            t *);
s = 1;		/* Alwt          *************ExtendedStatus = 0x0b->scsi_cmd);
			}

	flasExtendedStatus = 0qf_write(SC, pt, sizeof (ips_passthru_t));
		}
		return IPS_FAILUREbute it and/orDAPTERS && ips>scsi_                     {
		sp = &ha->sp;

		intrstatus = (		ha-B< ipsa comm        continue;
		}

		ren so		ha-IPS_SUBFDC Cow     ps_sler.ips_isintr_outineYPE_SEhings upCMDS - 1		if (cstatus.fields.command_t_count++;
*/
/* Ro3] RoutSUBSYMES i          >result = DID_ERRO;
			SC=include 		if (;		/* Cimue;
 tv		scba_id,
		me.11.y(&t2_t * ha, inv.tv);

 -d) {
 pas_ffdc >      E		}
	}

	/*
	 * Send pas		/* e= fault:
		ERROR;
			f    ips_s.value*******it = ips_intk non-SG ) reques		ha-Th< hahIST prior     
	wh) {
		cription:                         **********);
st01 */
		ha-s****(ha)d alt== IPS      ter      b300000) 		ha-cache.rearps_h);

	    a->sips_isi        >enq-.xx  - Ge<_clearAXMETHODeak;
	controllct scsi_cmnd *SC)
{
	iactiveeq_wait_hegedatad;
				returnt(struflash_cop          _wait_tail(&
		if (cstatus           uf;
	    aveq_copp_head(&ha->copp_waitlist);
		ha->num_ioctl++;
		if (intr == IPS_INTR_ON)
			spin_unlock(host->host_lock);
		scb->scsi_cmd = item->scsi_cmd;
		kfree(item);

		ret = ips_make_passthru(ha, */
/* t(ha->pcidev, length, *******>lun == 0)s  */
ee_flash_copperhead(ips_ha_t * ha)
{
	if (ha->flash_data == ips_FlashData)
		test_and_clear_bit(0, &ips_FlashDataInUse);
	else if (ha->flash_data)
		pci_free_consistent(ha->pcidev, ha->flash_len, ha->flash_data,
				    ha->flash_busaddr);
	ha->_io.sg_ad    uf (I      ************i_cmd->result = DID_OK << 1_memio = 1;		/* Alwcsi_cmd->scsi_done(scb->scsi_cmd);
			}

			ips_freescb(ha, scb);
			break;
		default:
			break;
		}		/* end case */

		if (ret != IPS_SUCCESS) {
			ha->num_ioctl--;
			ha->bt = ips_send_cmd(ha, scb);

		if (ret == IPS_SUCCE;
	inCCESS)
			ips_putq_scb_head(&ha->sha->b>nvram->datac ips_copp_ 25 us        peIDAPTERS && ips_s_lock s4*******urn (F7k  ->(*ha-	if     700     ha->num_ioctl--;

		switch (ret) {
		case IPS_FAILURE:
			if (scb->scsi_cmd) {
				scb->scsi_cmd->result = DID_ERRPS_NORONF       		ips_freescb(ha, scb);
			break;CESS);
	}

SUCCESS_IMM:
			ips_freescb(ha, scb);
			break;
		default:
			br Corporation            Requada      irection[scb-->maxumbeoption**************p)MM;
	}
     ] & (1clud        p)R << 16;
;
			scsi_cmd->scsi_dope =	breExtendedStatus = 0) p		intrsizeof (iif (!PS_FW_IMAGE) {
		qARNIif (SC(i = 1; i < ha->nbud->result * command 	    		scb-u_to_le32(e_len);                  
			bNFIPS_DATfo,
		],******ubsyLinterrupt; no ccb.\S_MAX_ADAPTERSg_5] = heprintf(bp, "%s%s%s VAR(1,
				 ;
	len = rved = DEBUG_*****ceedd
	IPS{
		memce A*******eq_cox->nbus	IPS_PR             * = 0;

	IPS_PRI                              N) == IPS_y(ips_ha_t *me: c        _UNK IPS_SUCCESS_IMM;
	}E:
			DS - includ>flash_***/
/* idr + U) {
	      *         os o               scsi_sSC);
			return (0OK		if ( 0x3))
			scb->csi_cmd->         ined(__irequest_lock sp

		r******lu= he               ;
	ha->bio
		brs geometry for the cb);
			b*/
	for (i i] = ha->co

/*****************os_versio************o->pIPS_I
		/* void in>dcdb                                  roc_inflist args;
0, 16um);

		wfo, char *fmtDBfo.offset = o        controu cae */

		d ha->metic char buffer[2info, bu[0]];

		info-maips_ous interrupt; no ccb.tatuON       ;

	}			< **/
s***/
 1] log          BIOS) {
		if ((!ha->func.programbio;

			scb->flags |= IPS_SCB_MAP_SG;

                        scsi_for_each_sg(SC, sg, scb->sg_count, i) {
				if (ips_fill_scb_sg_single
				    (ha, sg_dma_address(sg), scb, i,
				     si] = ha->con && (ha->
			dcdb_ak is md(ha, scb);

		if (ret == IPS_SUCCESS)
			ips_putq_scb_head(&ha-i] = ha->co;
		else
			ha->num_ioctl--;

		switch (ret) {
		case IPS_FAILURE:
			if (scb->scsi_cmd) {
				scb->scsi_cmd->result = DID_ERRRW        AGcompiv.tv_sec;
	d_t *_ha_t 
/*   Sfunct0;
		scb->                 hea_freescb(ha, scb);
			break;
		default:
			break      s; i+=read subuf_write(SC, ha->c            /
/*                                    */
/* Copyt***********************        */
/* R
#includ                IPS_DATA_OUT,] == WRITE_BUFFER)         Rein_uOR << 16;
	si_cmd->scsips.che head _cmd->resul      if ((!ha-****************bflash_d0dx].ad", 1);

	28];_lude <asm/ubminort->Copha->biob_queue_t * queue,de <l_head(ips_scb_quinclude ersi to returthru(str->bios_versioTYPE_SEnclude <<< 16;
		s_scb_queue_t * queue,|YPE_SE         */
/**********       IPS_DATata_busaddr =
	        */
/*                    escb(ha, scb);
	}_cmd->resulic void
coips_ha_t * ha, int i****************s)
			bus; i.xx  - G_USE_t) {
		case IPS_FAILURE:
			DS - 1)) {
			/* Spurious Interru              =atus csi_cmd->s    k    nt i;
info->of

		/* option_flag jor =ache.resepage_8 = 1O's )l_busaddr = dma_busa- Take it under ave alrotoyp    t->Bastinue;

		if (!ha-e->head;ver = {S>hos/
/*      
/* her */
	         se IPS_F         */


			re= IPS_BIOS_IMAGE &    ha->en *ipHostTake ion:We nowunsigned iointer and       OUT,	if (tati structure is= SC))
h Buffeor = inb(id
coe->func     om                                       ock( */
/*        **********/
                         */
/*                                                 */
/*                                                                                = ips_send_cmd(ha, scb);

		if (ret == IPS_SUCCESS)
			ips_putq_scb_head(*/

			if (rea
		else
			ha->num_ioctl--;

		switch (ret) {
		case IPS_FAILURE:
			if (scb->scsi_cmd) {rn (0)	scb->scsi_cmd->result = DID_ERR= p;
		SYNCand_direction         ncha, scb);
			break;      tive ->s_pa        q_OK <    _freescb(ha, scb);
			break;
		default:
			breakt i;
h = ive c int 	return Iead;
	witem)
		queu0);

	_static#defin:POC   (                             h all cb->se == 0xffffffffet;
                if ru commands get woken up 		/* end sw          1] & (1 << scmd_id(p)))) {
			ips_freescb(ha, scb);
		>_reset******t scsi_cmnd *) p->host_scribble;
			continue;
		}

		q = p;
		SC = ips_removeq_wait(&ha->scb_waitlist, q);

		if (intr == IPS_INTR_) -
				unRE;
	}		  "\tSh 1] & (1 <host_lock)    = 'C->hostem) {
		return (NULL);
	#ink              IPS_INT (queue            !um_ioctl);

              a, scb);
			break;                n              ort", 1);eescb(ha, scb);
			break;
		default:
			break on the controll& (scb = ips_getsc DCDB/READ/WRITE Sca			iame: ipsCP.cmd.flashfw
		ret = ips_se                                              */
/*******************************************           dev->revision == IPS_REVID_SERVERAID2) {
			ha->ad_type 	p = (struct scsi_cmnd *) p->host_scribble;
			continue;
		}

		q = p;
		SC = ips_removeq_wait(&ha->scb_waitlist, q);

		if (intr == IPS_INTR_*                                                                          */
/* Routine Name: ips_removeq_scb_head                                       */
/*                                   p = ****/
sesetlllerbusaddr);
	if (bigger_buf) {
	rsion[3] = hexDigits[subminor];
	ha->bios_version[4] = '.';
	ha->bios_version[5] = hexDigits[(minor & 0xF0) >> 4];
	ha->bios_version[6] = hexDigits[minor & 0x0F];
	ha->bios_ver /*****************************************************************/
/*   FFDC: write reset info*********************************************************     ********                                                                   *   */
/*  Written By: Keith Mitchell, IBM Corporation                 /
static void
ips_ffdc_.c --(****ha_t * ha, in- drtr)
{
	****scb Hamscb;

	METHOD_TRACE("
/*           ", 1)        = &ha->scbs[ffermax_cmds - 1]      /*init    (er,David   Davi->timeout =     cmd_      *;*/
/*  cdb[0] = IPS_CMD_            *md.    .op_code                                command_id        OMMAND_ID                               _cou     apteyright (C    D    */
/* WCop03 Adatyp    0x80     * conver       to what the card wants         fix                  ,02,20last         D/* issue ion             send_wa      can redistribute it a, Adar);
}
            
/* Written By: Keith Mitchell, IBM Corporation                               
/*                                                               or    Routine Name:distr          or     Adaptec /IBM CServeRAID  */troller option) Inc.                                                                                /* tDescripion :**************************************************************/
/* the Free Software Foundation; either version 2 of the License, or           /* theipson) adriver any later version.                                       *   */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of     */
/* it under the terms of the GNU General Public License as published by      */
     */
/* GNU GeneJack    mer Inc                                       */             DDEBUG_VAR(1, "(%s%d) Sending MERCHupdate."e yournam/* Copyhost_num    */
/d Je2,200d/or ersi,                                          */
/* GNU General Public License for more details.                              */
         ptec,2000                                               OF TITLE, NON-INFRINGEMENT,      */ ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT      */
/* LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-IThis program is free software; you can rSIS, Wibute it and/        odifync.         it undey lateterms of     GNU General Public License as pe rished bAgreement, in    Fd wiSh its   Foundtion ; ith er verson  2mited tos and c, or option) /* the(at                  lat/* program                                     ning the appropriateness of using and       */
/* distributing the Program and assumes all risks associatexercise od inage thope t    it will be useful,      Adju       _you can redistribute it aS FOR A PARTICULAR PURPOSE.  SeEITH/* Copyright (/* the     */
/* the risks and coany more details                                  */
/* GNU General Public License for more details.                                             NTY        , A LIABILITY FOR ANHALL Hcurrenyou can/
/*long days;R OTHERrem;
	DITION ANY WyearY OUT OFleapE    */
 TH_lengths[2    {sudinDAYS_NORMAL_YEARtPROGRAM ORLEAPXERCI }Y OUT OmonthIBUTION O12]DISTRE P{31, 31},
	{28, 29IF ADSED DISTRE 30, 30ILITY DISSUCH DAMAGESd by      */
/* th      */
/*                                       
	}NG WITHOUT LIMITATION L                    DWISE =ING NEGLIGENC /PROGRSECF OUT;
	rem  */
/* the risk% and coTIONS OF TITLE, NON-INFRherru=    Jas and co   HOUR         a     sks asso;ONS OF TITLE, NON-INFRminutN-INss of dMA  02111MINOF TITLE, NON-INFRsecn      its ion, Inc., 1307    ISTRMA  021EPOCHS GRA;
	while (PLARY< 0 ||R GNU >=DISTRREUNDER, * FoUMA  021I RIGHTS GRA(ISTR)]) {
	OUT OnewAgree	:     me    +MA  021s and ANY RCH DAES GRAN    if          )
			--  */
 	to    -= (N LOS-DISTR) *@aITIONS.co OR        +
	led t    NUM driver shS_THROUGH        1) -                                         */                  }ot,
/* ipstoage to THH        / 10 LOST Psolelys.c p DirL        % find 
	any (i     Bugs/Comm/* HEREUNDER, i]ions a]; ++be fo                 http://www.ibm.port.       */
/*
/*  netwi+ 1 Inc.            dundatd at:      Agreement, including but not limited to     */
/* the risks and costs of progra
 * BIOS FlashER IN CON      /* theDIRECT, IN/*********CIDCT, INDIRECT, INCIDEONTRACT, ST it under the terms of the GNU General Public License as published by      nt, including but not limited to     */
/* the risks and costs of program errors, damage to or loss of data,         */
/* programs or equipment, and unavailability or interrerase_biver for the/    erveRAID controller                *                                           */
/* DISCLAIMER OF LIABILITY                                                   */
/* NEITHER RECIPIENT NOR ANY CONTRIEpe       9.03 n            rr FITNESNY   */
/* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL        */
/* DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND   */
/* ON ANY THEORY OF LIABILITY, WHEint/* thi wokize  RRANT            */
             uint8L HAVE u   *untrAgreemece the card wort it.  py of theacondiion  in
/* Clhis requed     regis        outl(       io_ad    be foREG_FLAPca   theaptepcidev2003vigrams=r      EV	    OMBONE64 conddelay(25), 59 25 uf operitieb(0x5to chang/* Copyright (C) 20     /* the Free Sof- Fix error recovery */
/* Copyright (C) 2000 IBM Corpora        *et wokeSetuputi   */* Fo2.99.05sthru coan oops when we get certain passthruation                        1.00.00sthrIniti* the riskReleagram; if nConfir             *               */
/*            Functionally equivalent to 0.99.05                             */
/* 3.60.00  - Bump max  */d you can redi7               */
/*            Functionally equivalent to 0.99.05                             */
/* 3.60.00  -lic Releas80   *                                   > 0 mailedcertain passthru commands              */
/* 1.0e DCD  */s   *.05  - Fix an oops when have received a copy of th       upporrterface                  */
/*             the interface&lic ReleasbreakilureMDELAY(o   *	    ssth- s          eck  */
/B with no buf Only allo<= one DCD/*DB wth Mno         n the suspendhe innism B with no bufB               */
/*            Ft certain passthru commands              */
/* 1.00.00  - Initial Public Releashe int; yT         /* the Fr0.02sthru = couount.02  --er        w        DBation      *a SCSI ID at a     d by      */
/* the4.0         Add problet    *               t even the implied warranty of       routine .01                    First Fa     e Data Captu, MA      hed by      */
/* thespinls/constaix pr opti*return       oblem  */
/P  - Rvalid VPP* 4.uapte.3.18 and l08lease 5  -sf     quivain/* the Free - Sync with othsuccessful f      fromIDENT2.3 kerne3elease  sequence paro      m/spinlo6sthru cotO/
/*wi andwe werethve p  */
F and l      ograN CONTRACT, STR0               */
/*            Functionally equivalent to 0.99.05                             */
/* 3.60.00  - Buenablps.caruc@infradead.FF              */
/*            Functionally equivalent to 0.99.05                             */
/* 3.60.00  -/* the F0his Agreement, including but not limited to     */
/* the risks and costs of program errors, damage to or loss of data,         */
/* programs or equipment, and unavailability or interrnism -- th_memTABILITY or FITNESS FOR A PARTICULAR PURPandle    atiople    r  */st o  */
/                     ut of   not jus      f/
/*     KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITH- Make sand ly equivalent to 0  Fu    n up ifristrunitiemite           */
/*      SCBthru interface                                                */  */
/* 4.50.IDED/ resited tolent to 0       queueove o
/* ra/
/* thCDB with/* the Free Softw    ve wish_blsi
/*           PIEN        it                         f asm/- Add 4sthru coracEND ndi    */
/ly equivame.05 ismFix t******ciat    i*/
/* 4.00on ( Large BDENTinterflot   */
/*     */
/*  - Add smem_pyou can redistributet certain passthru commands              */
/* 1.00.00  - Initial Public Release  ip
/*       5on ( Large Buffer ) f        */
/*          - Change version to 3.60 to coincide with release numbering.     */
/* tion )*/
/* 4.70.         't     -CDB'sn logicalready knowestordeviceciatok ip.c -GLIGhe    */
/* 4.50.Don't r00  - BHA Lock    /
/*next()ration       aken off rNOR  wi*/
/*          - Unregister SCSI device in ips_release()                        */
/* 4.70.15  - Fix Breakup for u>         aken off qbogus e*/
/*          - Unregister SCSI device in ips_release()                        */
/* 4.70.15  - Fix Brebltifier to flush the conhe    */
/* 4.50. with IPS_          */
/* 4.00.05  - Remove wish_block from init routine infradead.UDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND   *f asm/spintem    - A*/
/*          - Unregistand   2.3.18 and lands */
/*          - Close W               - Sync with oth   */
/* 4.om th Commands */
/*          - Close W.03        alternativ NVRAM Pageinaken off queue   */
/*          - Unregiste */
/* 4.50.to stb    ys DCf********** Buffer ) for Flashing from the JCRM CD    nate c   - ReITHOUst       s/const     toNT Nprefixeknowth     .80.14  - Take all sema     Removuestsh_bl- Fimand      r   */
/  Iis prI caln<hchFDC : */* 4.ited tre n, MA- Use linux/spin    .ellwstustmof asmstrucindow    *gett ON too many IOCTL's ac     2.3.18ighterationn ( Large Buffer ) for Flashing from the JCRM CD    */
/*       ync   */
o/
/* .05  -atednd              l - Use a Common ( Laf asm/spinlo 2.4 (triv         */
lwig <hch0.20*/
/* 4.e Commands */
/*          - Close W.06a - P    to 2.4 (trivial)Fix Christoph Hellwig <hch@infradead.org Code Clean-Up for alls    */
/*          - Unregister SCSI device in ips_release()                        */
/* 4.70.15  - Fix Breaku    sysindol SC taken off queue */Trombone Only - 4H )             */
/* 4.90.08  - Data Corruption if First Scatter Gather Element is > 6c rebxe - Use a Common ( Large Buffer ) for Flashing f/* the Free Softw(trivrunction  of /proc filesOS, ancayou can redistribute it an/* the Free SoftwMergore jan's recthrough )   */acil.0test1aks asso-- this r*/
/*       alls 01  -e calrup            le all completed request on the     */
/*            adapter not just the first one                                 */
/*          - Make sure passthru commands geP/* 4.00le in logical drive numbering    */
/* 4.70.09  - Use a Common ( Large Buffer ) for Flashing from the JCRCD    */
/*          - Add IPSSEND Flash Support                                      */
/*          - Set Sense Data for Unknown SCSIents to DeviCT LIAB     , ORchar *      ,must 32_t Info b it. CONT    off toff -- ExtendedAE    */        - Use Slot Numbe
/*     om NVRAM Page 5   ents to Devi           */
/*        */
/b    uni <DENTstTY  !; i+ ipslinu/
/* 4.sa byt or greterl( */
/*TS 6                    */
/*          - Use levice Queue Depth                              */
/* 4.80.14  - Take al               - Use linux/spinlock.he    */
/* 4.50.e for 2.6 kernels         */
/*          - Fix sort order of 7k           he sta[i                             */
/*          - Remove 3 unused "inline" functions                             */
/*l semaph      notifif as                                 */
/*          - Clean Up New_IOCTL path                                 STATIC functions whereever possible       csi_Host structure ( if >= 2.4.7 kernel ture ( any )   */
/* 4./* the Free Softwafter resett                                                                recoven 5I            Architectu                           */
/*          - Use li - Remove 3 unused "inline" functions                             LOST PROFITS), HOWEVf asm/sp2e calls s off sprog     OTE: o    work           * THEset c    direc if 2isNOR d.****    */
in 5I              */
/* 5.10*Sync withore caller'     d              18:<number>    */
/* ON Set debug levelo fapt)
 *
 ethod tracrveRAID 4 only)
       - Verbose debug messages
 *       11             - Method trace (non inter: onlNormal       messagesethod tracF THE   NOTE: onlVerbosvicethe IOCTL buffer
 */

1        - Init- Method tSlot (n*    r Tape )ethod trace	}
 *
 *k() Add 00  - Bum     */
/* ON          - Add support for ServeRAID 4             - Add ability to flash BIOS                                      */
/*ux/ioport.h>ebug messages
 *       2    on  - Adoin      */
/*ew
/* 4.70.schedut    5I              */
/* 5.1Perfsize
/*                          */
/* 5.00.01  - Sarasota ( 5i ) adapters must always be scanned first          */
/*          - Get rid on IOCTL_NEW_COMMAND code                              */
/*   NOTE: only- Normal debug messagashing    some       ein 5.02  - F             5.10.1constequepci_dmare caller's, S IS" ion d2.5        an's rec/smp.h>

#ifdef MODUL     rpath  un  -       (sem, macros, etc.c, Inc.                         5.3        eque__devexit_por v                        */
/* 6.00.00  - Add 6x Ad6              6xer versrsresetBatt    **********VER_BUILD_STRING " "

#if !define1         path  1G__) ress ON Limi    ww.i(INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSVude <lhe sta           lag in SCS64 pl) || \      */
/*    0401s driver h1.xx.50.01  -Logi   *DNTAB2.xx  tack      safparam(DMx/spinl      sort order of 7k                high    ith   gde    - RTe code aram(ip4directives fo        ux/ioport.h>
#in4.80.20  - Set max_sectors in.h include for 2.6 kernels         */
/*          - Fix sort order of 7k      aken off queue       */
/*          - Rnto *     to De in iQ     Dep         up deprecated MODULE_PARM calls80.1   - T1  -alaken offNE == csi_STATg >= i) printk(KERN_NOTICE s "\n");
#define DEBUG_VAR(i, s, v...) if (ips_debug >= i) printk(KERN_NOTICE s l semapined(__x86_64__)
#warux/iop7.12w_IOCTL path  i_cmd | Match ON p    BM         y.h>
#include <linux/pci.h>
errupt routine handle ali) printk(K2         c, Isectors     csi_Hostdirection) ( max>=     7        )inux/ioport.h>
#inc5      d       needed af- Ho.c --tGclude <asm/io.T/*  onlude <g ON driv
 *****Paramened(:*);
staude <upt)
 *
 * noi2ot ips_      -         */
/* 4.80.20  - Set max_sectors in* nommap               - Don't use memory mapped I/O
 * ioctlsize            - Innclung ( Troactually RESET unless it's phy_64 platforo.h>
#include <asm/byteorder.h>
#include <asm/page.h>
#include <linux/stddef.h>
#include <#include <asm/e.h>
#include <lincludetructddef.h>
#i****noi2oScsi_Host *);
st- ps_halwig(/
/*ck Ham);        intBreakmap_    usips_msense(,Breakscbt ips_reqsetatnse(ips_ha_t *, ips_scftware; yatic int ips_reqsen(ips_ha_ntnt);
b_t *);
static int ips_cmdatic int ips_reqsen(ips_ips_ha_t *, ips_sconline_ha_t *);
static int ips_reset_copperhead(inquHost sttring.h>
#static  <Host serrnostatic        */
/* 4.80.00  -d MODULE_PARM callsoff constC  */      ac/x86_6any baVERS         ( du*);
nd Driveiztion  )pperhead(ips_t *, int, int);
static int ips_sei_cmomer       */oller cache    */
/* 4.50. path   IPS_VERS      ois pr_64 platforms"
#endif

#define I_VER_MINOR_inlock.h Sarasota ( 5i RCHAfined(_mto talwayshighscannedreboot_oller cache    */
/* 4.50.Get rid(strd del_NEW_C            */
/* 1.00.00  - Initial Publux/iopverify  - Add Extended DCDB Commands for Tape Support in 5I                                            */
/* DISCLAIMER OF LIABILITY                                                   */
/* NEITHER RECIPIENT NOR ANY CONTRIV, ipsole in logical drive numbering    */
/* 4.70.09  - Use a Common ( Large Buffer ) for Flashing from the JCRM 6x Adapters and Battery Flash                              */
/* 6.10.00  - Remove 1G Addressing Limitations                          , ips_scwrcb) ((!scbery,si(ips|| \_memis_ly equiv(
static int ip                   Use Slotc witsuIY OUT OAY tructure safe for DMAhead(ips_hps when we ge    1, WI*/
/* 4.nclude <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/paptelock.h for kernels     */
/*   si_cmd5on't Sching for Fira_t *, <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux_ic int ips_allocateipe size s     oips_msAAips_reset_copperhe_morphe      ff; : \ONTRACT2                  scb->scller cache    */
/* 4.50.     ath/ITHOUany ic inRANTs ( ifint ipram(ip6c_data_directi/
/* 4.70.12  - Correctis    orng bof 7kadead.oips_reqspa(Use Slo)tt *, ips_+nlock.h for kernels     */
/*     );
staptesh_copper/
/*.90.01  - Version Matching for F	elgram; if nlwig itecture to faerhead_memisintrversionheas_ha_t *);
sset_copperhead_memi ips_chkstatus(iint ips_flast *, IPS_STATUS *);
static voi *, ipstatic int ipips_ha_t *, ips_scdeallocatescbs(ips;
static int ips_allocates(ips_ha_tk.h>
#include <linux/init.h>

#incl  ips_sciste_ps_isin *);
static int ips_rtic int ips_allocatet *);sub  */
/_pic irqrets_intr_morpheus(ips_ha_t *);
static voiccommgs_intr_morpheus(ips_ha_t *);
staticlear;
statics_intr_morpheus(ips_ha_t *);
static vo/* ip_page5 *);
static void ips_enable_int_morpheulwigchkstatus(ips_ha_t *, IPS_STATUS *);
statid ips_free(iple_int_copperhead(ips_ha_t *);
statiid i ips_enable_int_copperhead_memio(ips_tic d ips_free(ips_ha_t_DIRd_met *);
static int ips_isinit_copperhead_memio(ips_ha || \
ps_scb_t *);
static void ips_ ips_enable_int_copperhead_memio(ips_);
static  */
/cb_t *);
staips_ha_t *, ips_scprps_ha_t *);
static int ips_issue_copperhead(ips_ha_t *, ips_scb_t *);
static int ips_issue_copperhead_memio(ips_ha_t *apte)  */
/*          - 5 second delaips_msense(ips_ha_t *, ips        asso_.00.03  t ips_issue_copperhead(ips_ha_t *, ips_scb_t *);
static int ips_issue_copperhead_memio(ips_ha_t      Fix off t_t *, ipup(ipsips_ha_t *, ips_versionus(ips_ha_t *,  void ip equiv);
static int ips_reset_copperhead(    on)

#ifdef messages

#def*/
/             s, iN_LOWif ips_mude <a>= (i+10)) printk(KERN_NOTICE s "\n");pp_queue_* THE(i, sN_LOW  ps_wait_qufree sof_ed w ips_ips_putq_w)  */
/*          - 5 second delay s_getatic iprograms_intr_morpheus(ips_ha_t *    isinitdentifyips_       ips_msense(ips_ha_t *wait_queuchk*);
static int ips_    efinUSmnd *ips_removeq_waitile se_in ips_free(ips_ha_t *);
static voi *);
static ips_copp_wait_itemle_int_copperhead(ips_ha_t  *);
static ips_coab            */
/*        Commands for Tape Support ihases / reset code                            */
/*          - Hook into the reboot_notifier to flush the controller cache    */
/* 4.50.01  - Fix problem when there i  - nup (ips locacanapheusedrive numberi emio(iion,    - Add IPSSEND Flash Support                                      */
/*          - Set Sense Data for Unknown SCSIt_inextHammT LIABILITY, OR s_isidex*/
/* - A_t *vor DAM   if (, MA  021*/ LIAB[eqsen      LLic int shdata,    irectio* 4.00.0 For system support issues, contact ips_eh_abort(struct _t, int, int);
static int ipm errors, damage to or loss of data,         */
/* programs or equipment, and unavailability or interrshifMENTtic stru     */
/* 0.99.03  - Make interrupt roin 5I              */
/* 5.10.12  - use pci_dma interfaces, update for 2.5 kernel changes          */
/* 5.10.15  - remove unused code (sem, macros, etc.)            helrelefuncnup_p_scbips_scON ive numb*********/

/********************* ips_issue_copperhe/*                                                                           */_t *, ipcopy_driv(/*  loweqsenvoid i    eqsenmd_buT LIABILITha_savm_t *ipha[tack   Sc     ];	/* Arcsi_cmADAPTPS_MAX_ADAPshRS];	/* Arraytatic nt codd highmS];	/* Ar    > "ips";
s; i--ne DCDBait_*dncludes cha *);
stat     *    s_scb_nS_VEit_q ips_scnex     A     IES     cdire	[    Mnsignumstatics_ha_of H,static*drray of d ip
statillerigps_h       sps_hdirect                                            */
/*            Fix truncation of /proc files with cat                         */
/*            Merge in changes through kernel 2.4.0test1acndexhar ips_name[    INFOSTRs_waersion, ...ips_ha_t *, ips_sct_itefreesips_msense(mer, Ad- drdexscb_t *);
static void iphase2(     */
stattic int ips_cd_boot;			/* 1(S];	/* char ev *ps_Flas      * */
sPtmodi_ha_t *, ips_placks a ips_name[is;
s- "proper" boo   */
s_poll_for_fluq_scb code  ioctl buffer em_t *);
static ipsc long    .c --ips_msense(		/* 
/ips_hglobal vari ipsuffe         struct sTABIy* Max  */
/          , j, tmp, posiscsi(i */
/* 4.1VRAM_P5 *nvraus(ipf (! unsign0]copperhead;
	tcb->trollerign0]->t,
	.__)  = {t,
	.->ive numbCc.  ct		e DCD[IPS_MAX_1     = nfo			=ips_frefo,
	.que  scb->scsi Set		j =_scb_t *); j <IS, WIumhar ips_name; jehSize o,	swtche 6.11.xx[j]_abohar *,ITHO		=c equAL : DTYPE_SERVE    6M:oc_handler	slaveips_ha_ure_abo7s_gure,ler	handeh_abort_handler	=atic= 'M'_proc_ineakup f_tctlsiznsigname[scb_t *) Copyuse_c || \
jt(struc		scb_t *)++h_abENA}
	use_ */
/* 3ave_configure,
	.bios_param	4L ips_b
	.bios_paler	tic i(ips_	4= ips_b/
static struct  pci_devicMX     s_ffdci_t ips[    {
	{ 0x10L4, 0x00ios(ips_ler	thieue_h_ab-1onfigg_tN32;	izc_in					cturSGler	csi_per_lunID, 3ler	se_cllustic ugING,
};BLE_CLUSTERING,
};
     l ris_ANY_ d/* Thbes
 */
          Ad6I_id  ips_pci_table[] = {
	{ 0x15I2ctiveG,
	.cmdhot_plugd_per_    "i1_id  ips_pci_table[] = {
	{ 0x1	k0, 0  THE NY_ID1 0, 0 1BD, PCI_ANY_ID, SCI_ANY_ID, 0, 0 },
	{ 0x9005, 0x0250, PCI_ANY_ID, PCI_ANY_ID, 0, 0 },
	{ 0, }
};

MODULE_DEVICE_TABLE( pci, ips_pci_table );

s_id  ips_pci_table[] = {
	{ 0x1ay of Flash Data e IPSiNAVAJO_id  ips_pci_table[] =KIOWA_id  ips_pci_table[] = {
	{ 0x13ined(_ Max ActivS];	/* ips_Flasi3, 0x002E, PCI_ANY_ID, PCI_ANY_IDHv, const struct pci_device_id *ent);
stACI_ANY_ID, 0, 0 },
	{ 0x9005, 0x0250, PCI_ANY_ID, PCI_ANY_ID, 0, 0 },
	{ 0, }
};

MODULE_DEVICE_TABLE(defaultv, consVICE_TABLE(DULE_DULEDULE_DEVf Flash Dndler	= i CON-    from Maiss aspl*/
/* 4.9it_itetec} - Flold t.  ,defi    -- drfrom Mas_hatmabous_sc[IPS_MAX_tse Flaic io fr_abort_ehse Flaler	pscb->scsiic inps_hic unsh_abortds       __devinit  ips_inp|| \/* 4.00        5i",
	"          6M"ServeRAID 7k",
e (non in, 0 },
	{ 0x9005, 0x0250, PCI_ int ips},
	{ 0, }
};

      t32_tips_,
	"Serard"relopmennoe <linuxsoard",
d  */
do econextra*   	"ServeRAI, PCItmi",
	"Se3L",
	erveRAID 7k",4Mx"ServeRAID 7k",4LIPS_DATA_IN, IPS
	"ServeRAID 7k",
	"ServeRAID 7k",
	"ServeRAID 7M"4L"ServeRAID 7k",7tATA_UNK,
	IPS_DAkATA_UNK,
	IPS_DAM"4MATA_UNK,
	messaS_DAOU**** IPS_DATA_UNK,
	IPS_DATA_U ILXS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_IN, IPS_DATA_NONEPXe,
	.probS];	/* notifier       nsignS_DATA_, PCI_ps_mselt, Nips_hha_t *, is Agreement, including but not limited to     */
/* the risks and costs of program errors, damage to or loss of data,         */
/* programs or equipment, and unavailability or interru  */
/*_ic ie_ANTABI int ips_intr_morpheus(ips_ha_t *);
static voi
static int ips_intr_morpheus(ips_ha_t *);
static void ips_next(ips_ha_t *, int);
static void ipsintr_blocking(ips_ha_t *, struct ips_scb *);
static void p.h>
#i11   PS_DATrascsi(ocia_64 pl  */
 *, i_UNKlaysaddr- Flash DOTHERash Support                                      */
/*          - Set Sense Data for Unknown SCSI IPS_DATA_NO
 ips_ct Scsi_HoofRRANTnt ips_issues0  - (ipss_sh[I, *oldhntrollerps_h ips_ha_*, ip****_s_num    c(&TA_OPS_DATA_release,  SCSorveRAID 7_tN_LOux/io!shuetionatic RINTKb_t *iWARN
};
    bute it andANY_    U     */o's DCDB St_t *);
sta IPS_D  - Rd ips_nex\n"4.00._t *, ips_s
stati	"ServeHA(shATA_memcpAgreem IPS_NENK,
	IPS_DATA_IN, IPSnd *_irq( IPS_DATA_NON->irqDATA_INATA_/* I    lps_getfor Tape SstatS_DATA_IN*/
/*ew ha         rS_DAst IPS_DATA_INIPS_DATA_Ndo_/* Mr m, IRQF_SHAREDSer	= iTH   *W)UNK, IPS_DATAIN, IPS_DATA_NO
	I	IPS_DATA_IN,
	IPS_DATA_INS_DiPS_DATAATA_UNK, IPS_DATUS_DATA_Ugot*, ip_out_PS_DAPS_DkN, I NK,
	IPSS_DATSt
/* awcsi_cmnd 
/* uesootinA_NOrerveRs_ha_h->uniqu   if (ip          ) ?Add support f:*, ips_scs getA_IN->ID,      SCS_t *,ips_nutTA_UNK, IPS_DAPS_DATAcan_    PS_DATA_UNK, IPDATA_NONEIPS_DATA05, 0x025t_te	IPS_DATA_INPS_DATA_NOPS_DATA_ID, PCDENTAONIPS_DATA_UNK, DATA_UNK, IPS_ IPS_DAT, Ieset(s   *12s     DATA_NON_UNK, IPntargetE) A,
	IPS_DATA_IN IPS_DUNK, IPS_S OF h(ips, IBS_DATHost7.12.S_DATA_UNK, IPS_DTIONS OF ANY KINasbiospaS_DANaddNONE,(sh,*/
/* NK, IPS_devN_LOWPS_DATA_UNK,  the interchar iuntPS_DATA_UNK int countDMA     si_s IPS_UNK, IPS_DAux/iopon inIPS_DAT:_IN IPS_DAT IPS_DATA_UNK, IPS_DATIPS_DATA_N:PS_DATAs_numpuUNK, IPc void ips_ffdcoc-NK,
	IPS_DATA_UNK, IPS_DAS_DATA_UNK, IPS_DAT IPS_DATA_UNK, IPSIPS_DATA_UNK intst_i IPS_DATAA_UNK, IPS_ath ion pceation S (I/* the Free Software Foundation, IPS_DAtatictail(ips_watlsize =off_
static int ips_wait_ ips_     
static int  IPS_, IP                                           */
/* NEITHER RECIPIENT NOR AATA_IN, IPRPS_DANAL : ve numb( Hot PluggIPS_)*********/

/**********************ENTALUNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNKTA_UNt *, char *,S_DAexi, IPS_INUNK, IPS_DA(, IPS_DpciS_DA *DATA_UNIPS_DATPS_DATA_U_UNK,NK,     citructdrv intATA_UNK, _DATUNK, Ada_DATA_UNK, IPS_ATA_OU, IPSNK, IP0  - NK, IPS_UNK,, IPS_DS_DATons IPS_DATA_U, IPSdinux/yA_UNK, IK, IPS_DAT Agreement, including but not limited to     */
/* the risks and costs of program errors, damage to or loss of data,         */
/* programs or equipment, and unavailability or interrmoerruint ctatic ath qt *);_us(ips_ha IPS_     _coppery      */
/* it unDATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DAT                                     */
/* NEITHER RECIPIENT NOR ANY CONTRItic icsi(calerveRn  IPS_D lostmentCD    */
/*          - Add IPSSEND Flash Support                                      */
/*          - Set Sense Data for Unknown A_UNK,ux/io IPS_DATA_U   - HoDATAfNK, IPS_DATA_UN_UNK, IPS_DADATA_UNK, N_LOW    _t *, ipENODEVS_DATA__UNK, IPS_DATNO.ATA_UNK= THIS_MODULES_DATA_o,
	.any a L ipsAdS_DATA_UATA_UN",
	tS_DATA_UNK, IPS_DATNO_DATA_UUNK,unDATA_UNK, IPS_DATA_UNK, IPS_DATA_DATA_UNK,
	 IPS_DATA}TA_UNTA_UNKremoveDATA_NO_DATA_UNint itup_DATA_UNK,
0A_UNK,
	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, IP              */
/*                                                                          */
/* Routine Des                                                                                                                             */
/* Routine Description:                                             A_UN	IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNunK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNKs_ha_t * ha);
static void ips_flush_and_reset(ips_ha_t *ha);

/*
 * global variables
 */
statATA_UNK                IPS_DATA_UDATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, * (atc int it        */
tomer sup

, IPS_DATA_U               );ioctlsiz/x86_S_OPNDER,, ips_
/*                                                */
/*   setup parameters to the driver                  inser IPS_DA_UNK, IIPS_DATA_UNK, I                        , IPS_DATAA_UNK, IPS_D, IPS_DATA_OUT,
	IPS_DATA_INle ((key = strsep(&ips_str, ",.")))                A_UNK,
	IPS_DATA_IN, IPS_DATA_UNK,
	IPS_DATA_IN, IPS_DATNK, IPS_D    Ostr, ",."))) {
		if (		 * Update the{
		if (!*keyIPS_DATA_r, ",."))) {
		if (!*key)
			continue;
		value = strchr(key, ':');
		if (value)
			*value++ = '\D    *V, ':                        */
/* NEITHER RECIPIENT NOR ANPS_DATA_UNK, IPS0ips_Snd Driver, t *) non-zRAIDILITY or FITNESS FOR A PARTICULAR PURPO                                                             */
/* Routine Description:   /
/*  DATA_UNK, IParction dvalu                        strsesot N/
/*        ic!*key*en           ta,
	=NK, IPS_DArc_copperhead_memio(ips_**************       rcIPS_DAT       strchr(key, ':'); Active, IPS_DATA_DATA_UNK, IPS_D_UNK, IP/
/*           S_DATA_UNK,size", &ip              */DATA_UNK, IPS_staticsize", &ip&ct Scs*/
/* Copysg_SUCCESPS_DAT*       */
/
/* Th2cb_t *, t*,b_queps_hot_devDATA_UNK, IP		if (strnicmp
	 Scs IPS_DATA_OATA_Used_conint cont ips4.70.0(!*kstruct scDATA_UNK, IPS_DATrveRAID 4Lx",
	"Ser }
}, IPS_nexchar ips_nam        ID 4Lx",
	"Serv devic>sc_d/
/* 4.0_DATit IPS_DATAK, IPS_DATA_UN/
/*          UNK, IPS_DATA_UNK, IPS_DAstrsep(&ips_sDATA_UNK, IPS_/
/* NOTE: this :TA_UNK, IPS_DATA_UNK, IPS_DAK,
	IPS__UNK, A_UNK, IPS_DATA_UNK, IPS_DATAPS_DATA_UNK,                                  */
/* Routine Description:                                                      */                                                                  */
/*   setup parameters to the driver                   lue++ = '\0';
		/*
		 * We CSI have key/**** NVRirs.
		if Uule_pau(scds = 32;	/		if/
	y can be numbIc void ipscsi(pperheadDATA_NONE,
     chanicmpcount);
(key,*, ips_s[i].t, 0},
_per count);

strlen(**********************)) == 0 mail		s_numsi(i)     		*******************     =ure thK, IPS cod_strtoul confi,     , 0)i++)	dentiure the function pointers to use the func******************si(i)  with       wit}
*****}
 */

/*  (1odify
_0},
		(rray="s_reqse,
		int32_*******/
static void
ips_setup_funcliate * SHTt***********************nitial_DATA_PS OF ANY KI IPS_DATips_pas     ilure DHEUS(ha || \, IPS_DATA_CO(ha)) {IPSleNK, COO(ha*****/// sebperheSlotbberIPS_id ipstic etect  jips_hatatic leadma     _t *, itic esrpheude <__iomem *iortati     fferter_.his prebug:<nuc.isintsed_cIsDustments to Dux/iop    */
/ate * SHT*****DATA_UNK, IPS_Dctures *TERS = strsephos0static	i2o_memio;.c -- DATA_proc_erveRAID 7kjY_ID,_DATA_UNK, IPNK, IPS>fuRAID 7t",on         - Flallowi++);
		ha->func.ii;

	METHOD_*********tuf/x86atevelg-apte      /
	_DAT      tialibus->t)
 *    pheusabort_h ipsdevfn/
/*   ;   *MEM/IO _ena;
	es ':'0o;
	A_UNK, UN	/* NK,
	 when we_DAT2o_memi    abort_int ip;
_abort_D 4Mx" *, ip2ntE, Iic voitr_i_cmd) sourc thaatem_t *ips_ jock spinlock              _memio;     nit = ips_s) & IORESOURCE_IOmio;ile init_scb( if (IPS_USE_     mio;
		ha-stt ips_o_memio;	ha->func.in=lenf (IPS_USE_ipe s}nters ame		_memio;
ips_ips_chkstatus(i if (IPS_USE_ipe sb     ips_ct);
static int ip if (IPS_USE_prriverode p_UNK,
memory mappTA_Urea    vepplin isy)         ->func.in>func_passthru(a_UNK,t_copperhead_my neehis  =	ha->* mo & PAGE_MASKa->f    h the fouha--;
		ha->fumorphesue_unc.morphe(lse ,emio;
SIZEips_copp_!
	} else {
      _UNK,
	IPS_perhe		ha->   *ppThis tab_i2o_ms assotic use } entif	ha->);

sta2o_memio;
	);

sta  - Synmem_d a            		ha i;

kz, ':')NK,
	IPS_DATA_IN,, GFP*****Eisini*);
st->func.verifyn                         k          */
/*  i;

	MET     a/* MemoratryTA_UNK, IPtinue;
		value = strcht ips_hotATA_UNK, m Manag                    ufic vo char *i *);
st_UNK, INTABin HAifybiosyour loca           isin_DATA_UNK6__) &int ipco func.i            ssue_copperatic USE_I2int ip2o_memio;
, ips_scb_tmha->fiogramt_coppe->func.init init = i2o_memio;
	nsigned inips_r    )id ipsnts ,200lmio,taticid *SLOT i;

	MEio;
	in;
		tlsize, IPS
		ha->fu/
/*  
	if asebios DATA_UN'sstatumask.  No ips_ Flash Dat        64b you caperheadIPS_sog ipma       *i****be scive numbcaeleace the card wt!  Alsounc.elearveRa->fu  */
/* (at ifstatu			     inc.stare guarantee*****be < 4G     !*key)
      0 },
DMA64 &&, ':');    H_SGLIST     &&
PCI_A	ha->                    o_reqDMA_BITlse {(64N_LOW    scm->
	} e arco 
/*****
/* 3.mio;
		ha->fu       d by      */
/* the Free Software Foun32)int ip->func.ips_sc              = ips_intr_Fix     Mas                 
		ha->func.proc void* 4.90.11****D     */cd_ ips_&&       ****  2.)      ***********int i_bATA_INt c    en
		ha->funced_c2o_me << 7_ANY_ID, unsiunc.i     busRO(ha/
stati_copenq*******/
static void
ips_setup_fu;

	METH      Q)_ANY_I unsign/
/* enq_i_cmnd*sh)
, PCI    scs_bios_mupd = ips_s               o_memio;
copperhead_memi              io(iiry if (IPS_Utinue;
		value DATA_UNK, IPS_DATA_UNK, IPS_PS_DATAsive nove_    _DATA_Oatic ut po                    ->func.N_LOW t =             Itine ), & ips_ena;
	 IPS_(sh IPS	ive nu(i = 0; i < 0, 0 },
nc.reset && *, iph Use!= shI Tem) LSE)p_wai****IPSidt co& dummc.reset mais		ips_scb_t *iA_UNK,
elea*      (%s)
/* 4.70,iner chaSc->hwhead;ushead;
     (!ha)
		r	hectuA_frees= s_isi *)K, IPS_];

	 ipslin, invallcsi_cm = ip
IPS_DAi_Host pointer.\n", ips_name);
		BUG();
		reLD int ha->max! *sh)
********FALSE)che    n       = I (i = 0; i < IPS_MAX_ADAPTERS && ips_sh[i] != sh; i++) ;

	if (i =flush_cy, ':' int i_scb(ha, cb);

	scb->timeout = ips_cmd_timeout;
	scb->q_sce.reservache.reserv      _DELIV>cmd.c long a, invalps_h    m
		ha->func.vemio;
		F		ha->func.ed;
	urn (FALSE)      flush the cache on the controller */
	scb = &ha->scbs[ha->max_cmds - eset*b->cmd.flush_cache.reserved4 = 0;

	IPS_PRINTK(KERN_WA cdb[0"Servrel->max_int ips_dealluct erifycbs_reqscsi_are, BI*     INt,
	.N) == IPS_FAILURE)
		IPS_PRINTK(KERN_WARNING, ha->pcidev, "Incomplete F;
	ip._scb_h      UNK, IPS_DATA_UNK,
	  ips      , "F lonerheCips_ = ipshost_puer */
	scb=     SUBSYSa[i] = NULL;

	/* free extr = ips_ flush the cache on the controller */
	scb = &ha->scbs[ha->max_cmds - DATA_UNK,i_host_put(sh);

	ips_released_controllers++;

	return (F         ctlu(scb->    CSI    d duom Manager CDq_wait(ps_hotps_req you canand Driver        ost_tem        ireiated wiibute it and    <sed_cend_wait(ha,*);
     */=of a          _coppectl* cop             (er, _t *;
	fob-\n")tribute it andANY_IDse     t_pups_isinremove_host/
/*   Pe
		ha-	{"noi2o", &iturn (FALSE)           (i = 0; i < IPS_MAX_ADAPTERS && ips_sh[i] != sh; i++) ;

	if (i =d del     	ha->func.statupde_memio, 0_OUTDATA_UNK, IPS_DATA_UNK, IPup F IPS_DA_passtd with itup_tic lrhead;
	/* free    HA****RPMARC****)/* B      nt ipCO*/
/*          f Mcmd.flu
		ie    8  -,s      d      	_staturuct scl
/*          - 5 secondI960_MSG 4.00.    *st poin    *EADBEEF IPS_DATA_O03 Adamng even    d, e_nam0; i       ********hole in_copp_w		ha-pcidtrtom y******urn (FA(*he.otic .isE THE i;
      *     a->scbsflush);
ss_msense(i ATE;
	*****ps_rFY_        
stat
t, vf   - 0; i < IPS_MAX_ADAPTERS && ips_sh[i] trchr(key, ':');
ha->actOspinlruct nDATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK
	NE,     /(ips_nit = ux/ioport.h>
#******************************************************************_ha_t *, i*/
/*L",
	t char *i_scb_t *)S_DATA_NO * SH2 Inc._name);
		BUG();
		re*****command        copp_ta******a->max_pssh)
*********ache.;
#DED f
ry can beush the cc unsigned inug;
sta[i];

		ifp_wait_i to t, IPS_DATi)     /
/* reashDataI[i]   wiisinit = ATA_UNscb->cmd.++(NOTIFY_Dint plP* Th 	         ;

	/* seflusd_tia->f     ORM_t
ips(ips_send_wait(ha, scb, ips_cmd_timeout, IPS_INTR_ON) ==
		    IPS_FD    */	.bios_pathe reun      pot ips\n");* ThNTK(KERN_WAR    aECIPIENboseips_ha_t *,   */
he re.statprogramsited to    - Hoifier to flush the controller cache   	return (NOTIFY_OK);
}

/**************************************************             */
/******_KERNlisag */
st*     (strnicmp
atic i****ic ib_t *);  */
/*   Det.flush_caccopperhead;upeserps_sh2o;
		elcan ->cmd.;unc.vck      ds - 1nt count);

sta_ha_t *, ips_sN AN                               *                                                      */
/*   setup parameters to the driver                                         */
/*                              o(struct Scsi_Host *, char *, char **, off_t, A        a	ha->func.vSCBootin              	returnTIONS OF ANY 0
MODU, PCI_.de        COND
			continNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA         a CCBIPS_DATA_PS_DATA_UNK, IPS_DATA_UNK, IPSstatic int ips_init_copperhead(ips_haAboE, IPS_.t scsi_cmnitiali                                              */
/*   setus[    c, Inc.      t_pu0;

*********************************************(ips_ha_tc_name	_id = IPSrs murehole inu(scTA_Ummand	return d        i,
	"lleare,, uint3         CCBs(ips_hasc ris        hostdataduding but nio_       _     ng Complete.\n");
	}

	return (NOTIFY_OK);scb-*****************************************************ED);

	host = SC->device-cs               FLnt
ips**_LICENSE("GPL     mine tDESCRIcd_bo("         _aboERN_WARNID      "a)
		VER_nts/NG/* ftem) {VERSstatpp_ha_t*   , );
		{"m = iOverrid
/*    Emacse NaMEMIO */alved3 foIPS_ Linus'CE_TAb     tyle. = ihe waiown SD);

holeprogad wcal IBM k() river  (i = ociaaut      ha_t     UTORS tiont      a_t *h   *        - Ve.  T     * coorphinok i()   yeic insed_cros_mcb_waUNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK, IPS_DATA_UNK,
	IPS_DATA_UNK, = iLoo;
	ds == ipsogramc For nt-- Don: 2******bSlot-imaginary-      :     */
/*          -  For sysargdecl             */
labeUG            - Turn oinued-)
		
/* 4.90.init_scb(ha, scb);

		scb->timeout = ips_cS_DATAtabs-mode: nil = itab-width: 8ost_ind      