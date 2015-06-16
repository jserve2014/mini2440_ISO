/*+M*************************************************************************
 * Adaptec AIC7xxx device driver for Linux.
 *
 * Copyright (c) 1994 John Aycock
 *   The University of Calgary Department of Computer Science.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Sources include the Adaptec 1740 driver (aha1740.c), the Ultrastor 24F
 * driver (ultrastor.c), various Linux kernel source, the Adaptec EISA
 * config file (!adp7771.cfg), the Adaptec AHA-2740A Series User's Guide,
 * the Linux Kernel Hacker's Guide, Writing a SCSI Device Driver for Linux,
 * the Adaptec 1542 driver (aha1542.c), the Adaptec EISA overlay file
 * (adp7770.ovl), the Adaptec AHA-2740 Series Technical Reference Manual,
 * the Adaptec AIC-7770 Data Book, the ANSI SCSI specification, the
 * ANSI SCSI-2 specification (draft 10c), ...
 *
 * --------------------------------------------------------------------------
 *
 *  Modifications by Daniel M. Eischen (deischen@iworks.InterWorks.org):
 *
 *  Substantially modified to include support for wide and twin bus
 *  adapters, DMAing of SCBs, tagged queueing, IRQ sharing, bug fixes,
 *  SCB paging, and other rework of the code.
 *
 *  Parts of this driver were also based on the FreeBSD driver by
 *  Justin T. Gibbs.  His copyright follows:
 *
 * --------------------------------------------------------------------------  
 * Copyright (c) 1994-1997 Justin Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Where this Software is combined with software released under the terms of 
 * the GNU General Public License ("GPL") and the terms of the GPL would require the 
 * combined work to also be released under the terms of the GPL, the terms
 * and conditions of this License will apply in addition to those of the
 * GPL with the exception of any terms or conditions of this License that
 * conflict with, or are expressly prohibited by, the GPL.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id: aic7xxx.c,v 1.119 1997/06/27 19:39:18 gibbs Exp $
 *---------------------------------------------------------------------------
 *
 *  Thanks also go to (in alphabetical order) the following:
 *
 *    Rory Bolt     - Sequencer bug fixes
 *    Jay Estabrook - Initial DEC Alpha support
 *    Doug Ledford  - Much needed abort/reset bug fixes
 *    Kai Makisara  - DMAing of SCBs
 *
 *  A Boot time option was also added for not resetting the scsi bus.
 *
 *    Form:  aic7xxx=extended
 *           aic7xxx=no_reset
 *           aic7xxx=ultra
 *           aic7xxx=irq_trigger:[0,1]  # 0 edge, 1 level
 *           aic7xxx=verbose
 *
 *  Daniel M. Eischen, deischen@iworks.InterWorks.org, 1/23/97
 *
 *  $Id: aic7xxx.c,v 4.1 1997/06/12 08:23:42 deang Exp $
 *-M*************************************************************************/

/*+M**************************************************************************
 *
 * Further driver modifications made by Doug Ledford <dledford@redhat.com>
 *
 * Copyright (c) 1997-1999 Doug Ledford
 *
 * These changes are released under the same licensing terms as the FreeBSD
 * driver written by Justin Gibbs.  Please see his Copyright notice above
 * for the exact terms and conditions covering my changes as well as the
 * warranty statement.
 *
 * Modifications made to the aic7xxx.c,v 4.1 driver from Dan Eischen include
 * but are not limited to:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: Modification of kernel code to accommodate different sequencer semantics
 *  3: Extensive changes throughout kernel portion of driver to improve
 *     abort/reset processing and error hanndling
 *  4: Other work contributed by various people on the Internet
 *  5: Changes to printk information and verbosity selection code
 *  6: General reliability related changes, especially in IRQ management
 *  7: Modifications to the default probe/attach order for supported cards
 *  8: SMP friendliness has been improved
 *
 * Overall, this driver represents a significant departure from the official
 * aic7xxx driver released by Dan Eischen in two ways.  First, in the code
 * itself.  A diff between the two version of the driver is now a several
 * thousand line diff.  Second, in approach to solving the same problem.  The
 * problem is importing the FreeBSD aic7xxx driver code to linux can be a
 * difficult and time consuming process, that also can be error prone.  Dan
 * Eischen's official driver uses the approach that the linux and FreeBSD
 * drivers should be as identical as possible.  To that end, his next version
 * of this driver will be using a mid-layer code library that he is developing
 * to moderate communications between the linux mid-level SCSI code and the
 * low level FreeBSD driver.  He intends to be able to essentially drop the
 * FreeBSD driver into the linux kernel with only a few minor tweaks to some
 * include files and the like and get things working, making for fast easy
 * imports of the FreeBSD code into linux.
 *
 * I disagree with Dan's approach.  Not that I don't think his way of doing
 * things would be nice, easy to maintain, and create a more uniform driver
 * between FreeBSD and Linux.  I have no objection to those issues.  My
 * disagreement is on the needed functionality.  There simply are certain
 * things that are done differently in FreeBSD than linux that will cause
 * problems for this driver regardless of any middle ware Dan implements.
 * The biggest example of this at the moment is interrupt semantics.  Linux
 * doesn't provide the same protection techniques as FreeBSD does, nor can
 * they be easily implemented in any middle ware code since they would truly
 * belong in the kernel proper and would effect all drivers.  For the time
 * being, I see issues such as these as major stumbling blocks to the 
 * reliability of code based upon such middle ware.  Therefore, I choose to
 * use a different approach to importing the FreeBSD code that doesn't
 * involve any middle ware type code.  My approach is to import the sequencer
 * code from FreeBSD wholesale.  Then, to only make changes in the kernel
 * portion of the driver as they are needed for the new sequencer semantics.
 * In this way, the portion of the driver that speaks to the rest of the
 * linux kernel is fairly static and can be changed/modified to solve
 * any problems one might encounter without concern for the FreeBSD driver.
 *
 * Note: If time and experience should prove me wrong that the middle ware
 * code Dan writes is reliable in its operation, then I'll retract my above
 * statements.  But, for those that don't know, I'm from Missouri (in the US)
 * and our state motto is "The Show-Me State".  Well, before I will put
 * faith into it, you'll have to show me that it works :)
 *
 *_M*************************************************************************/

/*
 * The next three defines are user configurable.  These should be the only
 * defines a user might need to get in here and change.  There are other
 * defines buried deeper in the code, but those really shouldn't need touched
 * under normal conditions.
 */

/*
 * AIC7XXX_STRICT_PCI_SETUP
 *   Should we assume the PCI config options on our controllers are set with
 *   sane and proper values, or should we be anal about our PCI config
 *   registers and force them to what we want?  The main advantage to
 *   defining this option is on non-Intel hardware where the BIOS may not
 *   have been run to set things up, or if you have one of the BIOSless
 *   Adaptec controllers, such as a 2910, that don't get set up by the
 *   BIOS.  However, keep in mind that we really do set the most important
 *   items in the driver regardless of this setting, this only controls some
 *   of the more esoteric PCI options on these cards.  In that sense, I
 *   would default to leaving this off.  However, if people wish to try
 *   things both ways, that would also help me to know if there are some
 *   machines where it works one way but not another.
 *
 *   -- July 7, 17:09
 *     OK...I need this on my machine for testing, so the default is to
 *     leave it defined.
 *
 *   -- July 7, 18:49
 *     I needed it for testing, but it didn't make any difference, so back
 *     off she goes.
 *
 *   -- July 16, 23:04
 *     I turned it back on to try and compensate for the 2.1.x PCI code
 *     which no longer relies solely on the BIOS and now tries to set
 *     things itself.
 */

#define AIC7XXX_STRICT_PCI_SETUP

/*
 * AIC7XXX_VERBOSE_DEBUGGING
 *   This option enables a lot of extra printk();s in the code, surrounded
 *   by if (aic7xxx_verbose ...) statements.  Executing all of those if
 *   statements and the extra checks can get to where it actually does have
 *   an impact on CPU usage and such, as well as code size.  Disabling this
 *   define will keep some of those from becoming part of the code.
 *
 *   NOTE:  Currently, this option has no real effect, I will be adding the
 *   various #ifdef's in the code later when I've decided a section is
 *   complete and no longer needs debugging.  OK...a lot of things are now
 *   surrounded by this define, so turning this off does have an impact.
 */
 
/*
 * #define AIC7XXX_VERBOSE_DEBUGGING
 */
 
#include <linux/module.h>
#include <stdarg.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/byteorder.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include "scsi.h"
#include <scsi/scsi_host.h>
#include "aic7xxx_old/aic7xxx.h"

#include "aic7xxx_old/sequencer.h"
#include "aic7xxx_old/scsi_message.h"
#include "aic7xxx_old/aic7xxx_reg.h"
#include <scsi/scsicam.h>

#include <linux/stat.h>
#include <linux/slab.h>        /* for kmalloc() */

#define AIC7XXX_C_VERSION  "5.2.6"

#define ALL_TARGETS -1
#define ALL_CHANNELS -1
#define ALL_LUNS -1
#define MAX_TARGETS  16
#define MAX_LUNS     8
#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define FALSE 0
#endif

#if defined(__powerpc__) || defined(__i386__) || defined(__x86_64__)
#  define MMAPIO
#endif

/*
 * You can try raising me for better performance or lowering me if you have
 * flaky devices that go off the scsi bus when hit with too many tagged
 * commands (like some IBM SCSI-3 LVD drives).
 */
#define AIC7XXX_CMDS_PER_DEVICE 32

typedef struct
{
  unsigned char tag_commands[16];   /* Allow for wide/twin adapters. */
} adapter_tag_info_t;

/*
 * Make a define that will tell the driver not to the default tag depth
 * everywhere.
 */
#define DEFAULT_TAG_COMMANDS {0, 0, 0, 0, 0, 0, 0, 0,\
                              0, 0, 0, 0, 0, 0, 0, 0}

/*
 * Modify this as you see fit for your system.  By setting tag_commands
 * to 0, the driver will use it's own algorithm for determining the
 * number of commands to use (see above).  When 255, the driver will
 * not enable tagged queueing for that particular device.  When positive
 * (> 0) and (< 255) the values in the array are used for the queue_depth.
 * Note that the maximum value for an entry is 254, but you're insane if
 * you try to use that many commands on one device.
 *
 * In this example, the first line will disable tagged queueing for all
 * the devices on the first probed aic7xxx adapter.
 *
 * The second line enables tagged queueing with 4 commands/LUN for IDs
 * (1, 2-11, 13-15), disables tagged queueing for ID 12, and tells the
 * driver to use its own algorithm for ID 1.
 *
 * The third line is the same as the first line.
 *
 * The fourth line disables tagged queueing for devices 0 and 3.  It
 * enables tagged queueing for the other IDs, with 16 commands/LUN
 * for IDs 1 and 4, 127 commands/LUN for ID 8, and 4 commands/LUN for
 * IDs 2, 5-7, and 9-15.
 */

/*
 * NOTE: The below structure is for reference only, the actual structure
 *       to modify in order to change things is found after this fake one.
 *
adapter_tag_info_t aic7xxx_tag_info[] =
{
  {DEFAULT_TAG_COMMANDS},
  {{4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 255, 4, 4, 4}},
  {DEFAULT_TAG_COMMANDS},
  {{255, 16, 4, 255, 16, 4, 4, 4, 127, 4, 4, 4, 4, 4, 4, 4}}
};
*/

static adapter_tag_info_t aic7xxx_tag_info[] =
{
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS}
};


/*
 * Define an array of board names that can be indexed by aha_type.
 * Don't forget to change this when changing the types!
 */
static const char *board_names[] = {
  "AIC-7xxx Unknown",                                   /* AIC_NONE */
  "Adaptec AIC-7810 Hardware RAID Controller",          /* AIC_7810 */
  "Adaptec AIC-7770 SCSI host adapter",                 /* AIC_7770 */
  "Adaptec AHA-274X SCSI host adapter",                 /* AIC_7771 */
  "Adaptec AHA-284X SCSI host adapter",                 /* AIC_284x */
  "Adaptec AIC-7850 SCSI host adapter",                 /* AIC_7850 */
  "Adaptec AIC-7855 SCSI host adapter",                 /* AIC_7855 */
  "Adaptec AIC-7860 Ultra SCSI host adapter",           /* AIC_7860 */
  "Adaptec AHA-2940A Ultra SCSI host adapter",          /* AIC_7861 */
  "Adaptec AIC-7870 SCSI host adapter",                 /* AIC_7870 */
  "Adaptec AHA-294X SCSI host adapter",                 /* AIC_7871 */
  "Adaptec AHA-394X SCSI host adapter",                 /* AIC_7872 */
  "Adaptec AHA-398X SCSI host adapter",                 /* AIC_7873 */
  "Adaptec AHA-2944 SCSI host adapter",                 /* AIC_7874 */
  "Adaptec AIC-7880 Ultra SCSI host adapter",           /* AIC_7880 */
  "Adaptec AHA-294X Ultra SCSI host adapter",           /* AIC_7881 */
  "Adaptec AHA-394X Ultra SCSI host adapter",           /* AIC_7882 */
  "Adaptec AHA-398X Ultra SCSI host adapter",           /* AIC_7883 */
  "Adaptec AHA-2944 Ultra SCSI host adapter",           /* AIC_7884 */
  "Adaptec AHA-2940UW Pro Ultra SCSI host adapter",     /* AIC_7887 */
  "Adaptec AIC-7895 Ultra SCSI host adapter",           /* AIC_7895 */
  "Adaptec AIC-7890/1 Ultra2 SCSI host adapter",        /* AIC_7890 */
  "Adaptec AHA-293X Ultra2 SCSI host adapter",          /* AIC_7890 */
  "Adaptec AHA-294X Ultra2 SCSI host adapter",          /* AIC_7890 */
  "Adaptec AIC-7896/7 Ultra2 SCSI host adapter",        /* AIC_7896 */
  "Adaptec AHA-394X Ultra2 SCSI host adapter",          /* AIC_7897 */
  "Adaptec AHA-395X Ultra2 SCSI host adapter",          /* AIC_7897 */
  "Adaptec PCMCIA SCSI controller",                     /* card bus stuff */
  "Adaptec AIC-7892 Ultra 160/m SCSI host adapter",     /* AIC_7892 */
  "Adaptec AIC-7899 Ultra 160/m SCSI host adapter",     /* AIC_7899 */
};

/*
 * There should be a specific return value for this in scsi.h, but
 * it seems that most drivers ignore it.
 */
#define DID_UNDERFLOW   DID_ERROR

/*
 *  What we want to do is have the higher level scsi driver requeue
 *  the command to us. There is no specific driver status for this
 *  condition, but the higher level scsi driver will requeue the
 *  command on a DID_BUS_BUSY error.
 *
 *  Upon further inspection and testing, it seems that DID_BUS_BUSY
 *  will *always* retry the command.  We can get into an infinite loop
 *  if this happens when we really want some sort of counter that
 *  will automatically abort/reset the command after so many retries.
 *  Using DID_ERROR will do just that.  (Made by a suggestion by
 *  Doug Ledford 8/1/96)
 */
#define DID_RETRY_COMMAND DID_ERROR

#define HSCSIID        0x07
#define SCSI_RESET     0x040

/*
 * EISA/VL-bus stuff
 */
#define MINSLOT                1
#define MAXSLOT                15
#define SLOTBASE(x)        ((x) << 12)
#define BASE_TO_SLOT(x) ((x) >> 12)

/*
 * Standard EISA Host ID regs  (Offset from slot base)
 */
#define AHC_HID0              0x80   /* 0,1: msb of ID2, 2-7: ID1      */
#define AHC_HID1              0x81   /* 0-4: ID3, 5-7: LSB ID2         */
#define AHC_HID2              0x82   /* product                        */
#define AHC_HID3              0x83   /* firmware revision              */

/*
 * AIC-7770 I/O range to reserve for a card
 */
#define MINREG                0xC00
#define MAXREG                0xCFF

#define INTDEF                0x5C      /* Interrupt Definition Register */

/*
 * AIC-78X0 PCI registers
 */
#define        CLASS_PROGIF_REVID        0x08
#define                DEVREVID        0x000000FFul
#define                PROGINFC        0x0000FF00ul
#define                SUBCLASS        0x00FF0000ul
#define                BASECLASS        0xFF000000ul

#define        CSIZE_LATTIME                0x0C
#define                CACHESIZE        0x0000003Ful        /* only 5 bits */
#define                LATTIME                0x0000FF00ul

#define        DEVCONFIG                0x40
#define                SCBSIZE32        0x00010000ul        /* aic789X only */
#define                MPORTMODE        0x00000400ul        /* aic7870 only */
#define                RAMPSM           0x00000200ul        /* aic7870 only */
#define                RAMPSM_ULTRA2    0x00000004
#define                VOLSENSE         0x00000100ul
#define                SCBRAMSEL        0x00000080ul
#define                SCBRAMSEL_ULTRA2 0x00000008
#define                MRDCEN           0x00000040ul
#define                EXTSCBTIME       0x00000020ul        /* aic7870 only */
#define                EXTSCBPEN        0x00000010ul        /* aic7870 only */
#define                BERREN           0x00000008ul
#define                DACEN            0x00000004ul
#define                STPWLEVEL        0x00000002ul
#define                DIFACTNEGEN      0x00000001ul        /* aic7870 only */

#define        SCAMCTL                  0x1a                /* Ultra2 only  */
#define        CCSCBBADDR               0xf0                /* aic7895/6/7  */

/*
 * Define the different types of SEEPROMs on aic7xxx adapters
 * and make it also represent the address size used in accessing
 * its registers.  The 93C46 chips have 1024 bits organized into
 * 64 16-bit words, while the 93C56 chips have 2048 bits organized
 * into 128 16-bit words.  The C46 chips use 6 bits to address
 * each word, while the C56 and C66 (4096 bits) use 8 bits to
 * address each word.
 */
typedef enum {C46 = 6, C56_66 = 8} seeprom_chip_type;

/*
 *
 * Define the format of the SEEPROM registers (16 bits).
 *
 */
struct seeprom_config {

/*
 * SCSI ID Configuration Flags
 */
#define CFXFER                0x0007      /* synchronous transfer rate */
#define CFSYNCH               0x0008      /* enable synchronous transfer */
#define CFDISC                0x0010      /* enable disconnection */
#define CFWIDEB               0x0020      /* wide bus device (wide card) */
#define CFSYNCHISULTRA        0x0040      /* CFSYNC is an ultra offset */
#define CFNEWULTRAFORMAT      0x0080      /* Use the Ultra2 SEEPROM format */
#define CFSTART               0x0100      /* send start unit SCSI command */
#define CFINCBIOS             0x0200      /* include in BIOS scan */
#define CFRNFOUND             0x0400      /* report even if not found */
#define CFMULTILUN            0x0800      /* probe mult luns in BIOS scan */
#define CFWBCACHEYES          0x4000      /* Enable W-Behind Cache on drive */
#define CFWBCACHENC           0xc000      /* Don't change W-Behind Cache */
/* UNUSED                0x3000 */
  unsigned short device_flags[16];        /* words 0-15 */

/*
 * BIOS Control Bits
 */
#define CFSUPREM        0x0001  /* support all removable drives */
#define CFSUPREMB       0x0002  /* support removable drives for boot only */
#define CFBIOSEN        0x0004  /* BIOS enabled */
/* UNUSED                0x0008 */
#define CFSM2DRV        0x0010  /* support more than two drives */
#define CF284XEXTEND    0x0020  /* extended translation (284x cards) */
/* UNUSED                0x0040 */
#define CFEXTEND        0x0080  /* extended translation enabled */
/* UNUSED                0xFF00 */
  unsigned short bios_control;  /* word 16 */

/*
 * Host Adapter Control Bits
 */
#define CFAUTOTERM      0x0001  /* Perform Auto termination */
#define CFULTRAEN       0x0002  /* Ultra SCSI speed enable (Ultra cards) */
#define CF284XSELTO     0x0003  /* Selection timeout (284x cards) */
#define CF284XFIFO      0x000C  /* FIFO Threshold (284x cards) */
#define CFSTERM         0x0004  /* SCSI low byte termination */
#define CFWSTERM        0x0008  /* SCSI high byte termination (wide card) */
#define CFSPARITY       0x0010  /* SCSI parity */
#define CF284XSTERM     0x0020  /* SCSI low byte termination (284x cards) */
#define CFRESETB        0x0040  /* reset SCSI bus at boot */
#define CFBPRIMARY      0x0100  /* Channel B primary on 7895 chipsets */
#define CFSEAUTOTERM    0x0400  /* aic7890 Perform SE Auto Term */
#define CFLVDSTERM      0x0800  /* aic7890 LVD Termination */
/* UNUSED                0xF280 */
  unsigned short adapter_control;        /* word 17 */

/*
 * Bus Release, Host Adapter ID
 */
#define CFSCSIID        0x000F                /* host adapter SCSI ID */
/* UNUSED                0x00F0 */
#define CFBRTIME        0xFF00                /* bus release time */
  unsigned short brtime_id;                /* word 18 */

/*
 * Maximum targets
 */
#define CFMAXTARG        0x00FF        /* maximum targets */
/* UNUSED                0xFF00 */
  unsigned short max_targets;                /* word 19 */

  unsigned short res_1[11];                /* words 20-30 */
  unsigned short checksum;                /* word 31 */
};

#define SELBUS_MASK                0x0a
#define         SELNARROW        0x00
#define         SELBUSB                0x08
#define SINGLE_BUS                0x00

#define SCB_TARGET(scb)         \
       (((scb)->hscb->target_channel_lun & TID) >> 4)
#define SCB_LUN(scb)            \
       ((scb)->hscb->target_channel_lun & LID)
#define SCB_IS_SCSIBUS_B(scb)   \
       (((scb)->hscb->target_channel_lun & SELBUSB) != 0)

/*
 * If an error occurs during a data transfer phase, run the command
 * to completion - it's easier that way - making a note of the error
 * condition in this location. This then will modify a DID_OK status
 * into an appropriate error for the higher-level SCSI code.
 */
#define aic7xxx_error(cmd)        ((cmd)->SCp.Status)

/*
 * Keep track of the targets returned status.
 */
#define aic7xxx_status(cmd)        ((cmd)->SCp.sent_command)

/*
 * The position of the SCSI commands scb within the scb array.
 */
#define aic7xxx_position(cmd)        ((cmd)->SCp.have_data_in)

/*
 * The stored DMA mapping for single-buffer data transfers.
 */
#define aic7xxx_mapping(cmd)	     ((cmd)->SCp.phase)

/*
 * Get out private data area from a scsi cmd pointer
 */
#define AIC_DEV(cmd)	((struct aic_dev_data *)(cmd)->device->hostdata)

/*
 * So we can keep track of our host structs
 */
static struct aic7xxx_host *first_aic7xxx = NULL;

/*
 * As of Linux 2.1, the mid-level SCSI code uses virtual addresses
 * in the scatter-gather lists.  We need to convert the virtual
 * addresses to physical addresses.
 */
struct hw_scatterlist {
  unsigned int address;
  unsigned int length;
};

/*
 * Maximum number of SG segments these cards can support.
 */
#define        AIC7XXX_MAX_SG 128

/*
 * The maximum number of SCBs we could have for ANY type
 * of card. DON'T FORGET TO CHANGE THE SCB MASK IN THE
 * SEQUENCER CODE IF THIS IS MODIFIED!
 */
#define AIC7XXX_MAXSCB        255


struct aic7xxx_hwscb {
/* ------------    Begin hardware supported fields    ---------------- */
/* 0*/  unsigned char control;
/* 1*/  unsigned char target_channel_lun;       /* 4/1/3 bits */
/* 2*/  unsigned char target_status;
/* 3*/  unsigned char SG_segment_count;
/* 4*/  unsigned int  SG_list_pointer;
/* 8*/  unsigned char residual_SG_segment_count;
/* 9*/  unsigned char residual_data_count[3];
/*12*/  unsigned int  data_pointer;
/*16*/  unsigned int  data_count;
/*20*/  unsigned int  SCSI_cmd_pointer;
/*24*/  unsigned char SCSI_cmd_length;
/*25*/  unsigned char tag;          /* Index into our kernel SCB array.
                                     * Also used as the tag for tagged I/O
                                     */
#define SCB_PIO_TRANSFER_SIZE  26   /* amount we need to upload/download
                                     * via PIO to initialize a transaction.
                                     */
/*26*/  unsigned char next;         /* Used to thread SCBs awaiting selection
                                     * or disconnected down in the sequencer.
                                     */
/*27*/  unsigned char prev;
/*28*/  unsigned int pad;           /*
                                     * Unused by the kernel, but we require
                                     * the padding so that the array of
                                     * hardware SCBs is aligned on 32 byte
                                     * boundaries so the sequencer can index
                                     */
};

typedef enum {
        SCB_FREE                = 0x0000,
        SCB_DTR_SCB             = 0x0001,
        SCB_WAITINGQ            = 0x0002,
        SCB_ACTIVE              = 0x0004,
        SCB_SENSE               = 0x0008,
        SCB_ABORT               = 0x0010,
        SCB_DEVICE_RESET        = 0x0020,
        SCB_RESET               = 0x0040,
        SCB_RECOVERY_SCB        = 0x0080,
        SCB_MSGOUT_PPR          = 0x0100,
        SCB_MSGOUT_SENT         = 0x0200,
        SCB_MSGOUT_SDTR         = 0x0400,
        SCB_MSGOUT_WDTR         = 0x0800,
        SCB_MSGOUT_BITS         = SCB_MSGOUT_PPR |
                                  SCB_MSGOUT_SENT | 
                                  SCB_MSGOUT_SDTR |
                                  SCB_MSGOUT_WDTR,
        SCB_QUEUED_ABORT        = 0x1000,
        SCB_QUEUED_FOR_DONE     = 0x2000,
        SCB_WAS_BUSY            = 0x4000,
	SCB_QUEUE_FULL		= 0x8000
} scb_flag_type;

typedef enum {
        AHC_FNONE                 = 0x00000000,
        AHC_PAGESCBS              = 0x00000001,
        AHC_CHANNEL_B_PRIMARY     = 0x00000002,
        AHC_USEDEFAULTS           = 0x00000004,
        AHC_INDIRECT_PAGING       = 0x00000008,
        AHC_CHNLB                 = 0x00000020,
        AHC_CHNLC                 = 0x00000040,
        AHC_EXTEND_TRANS_A        = 0x00000100,
        AHC_EXTEND_TRANS_B        = 0x00000200,
        AHC_TERM_ENB_A            = 0x00000400,
        AHC_TERM_ENB_SE_LOW       = 0x00000400,
        AHC_TERM_ENB_B            = 0x00000800,
        AHC_TERM_ENB_SE_HIGH      = 0x00000800,
        AHC_HANDLING_REQINITS     = 0x00001000,
        AHC_TARGETMODE            = 0x00002000,
        AHC_NEWEEPROM_FMT         = 0x00004000,
 /*
  *  Here ends the FreeBSD defined flags and here begins the linux defined
  *  flags.  NOTE: I did not preserve the old flag name during this change
  *  specifically to force me to evaluate what flags were being used properly
  *  and what flags weren't.  This way, I could clean up the flag usage on
  *  a use by use basis.  Doug Ledford
  */
        AHC_MOTHERBOARD           = 0x00020000,
        AHC_NO_STPWEN             = 0x00040000,
        AHC_RESET_DELAY           = 0x00080000,
        AHC_A_SCANNED             = 0x00100000,
        AHC_B_SCANNED             = 0x00200000,
        AHC_MULTI_CHANNEL         = 0x00400000,
        AHC_BIOS_ENABLED          = 0x00800000,
        AHC_SEEPROM_FOUND         = 0x01000000,
        AHC_TERM_ENB_LVD          = 0x02000000,
        AHC_ABORT_PENDING         = 0x04000000,
        AHC_RESET_PENDING         = 0x08000000,
#define AHC_IN_ISR_BIT              28
        AHC_IN_ISR                = 0x10000000,
        AHC_IN_ABORT              = 0x20000000,
        AHC_IN_RESET              = 0x40000000,
        AHC_EXTERNAL_SRAM         = 0x80000000
} ahc_flag_type;

typedef enum {
  AHC_NONE             = 0x0000,
  AHC_CHIPID_MASK      = 0x00ff,
  AHC_AIC7770          = 0x0001,
  AHC_AIC7850          = 0x0002,
  AHC_AIC7860          = 0x0003,
  AHC_AIC7870          = 0x0004,
  AHC_AIC7880          = 0x0005,
  AHC_AIC7890          = 0x0006,
  AHC_AIC7895          = 0x0007,
  AHC_AIC7896          = 0x0008,
  AHC_AIC7892          = 0x0009,
  AHC_AIC7899          = 0x000a,
  AHC_VL               = 0x0100,
  AHC_EISA             = 0x0200,
  AHC_PCI              = 0x0400,
} ahc_chip;

typedef enum {
  AHC_FENONE           = 0x0000,
  AHC_ULTRA            = 0x0001,
  AHC_ULTRA2           = 0x0002,
  AHC_WIDE             = 0x0004,
  AHC_TWIN             = 0x0008,
  AHC_MORE_SRAM        = 0x0010,
  AHC_CMD_CHAN         = 0x0020,
  AHC_QUEUE_REGS       = 0x0040,
  AHC_SG_PRELOAD       = 0x0080,
  AHC_SPIOCAP          = 0x0100,
  AHC_ULTRA3           = 0x0200,
  AHC_NEW_AUTOTERM     = 0x0400,
  AHC_AIC7770_FE       = AHC_FENONE,
  AHC_AIC7850_FE       = AHC_SPIOCAP,
  AHC_AIC7860_FE       = AHC_ULTRA|AHC_SPIOCAP,
  AHC_AIC7870_FE       = AHC_FENONE,
  AHC_AIC7880_FE       = AHC_ULTRA,
  AHC_AIC7890_FE       = AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRA2|
                         AHC_QUEUE_REGS|AHC_SG_PRELOAD|AHC_NEW_AUTOTERM,
  AHC_AIC7895_FE       = AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRA,
  AHC_AIC7896_FE       = AHC_AIC7890_FE,
  AHC_AIC7892_FE       = AHC_AIC7890_FE|AHC_ULTRA3,
  AHC_AIC7899_FE       = AHC_AIC7890_FE|AHC_ULTRA3,
} ahc_feature;

#define SCB_DMA_ADDR(scb, addr) ((unsigned long)(addr) + (scb)->scb_dma->dma_offset)

struct aic7xxx_scb_dma {
	unsigned long	       dma_offset;    /* Correction you have to add
					       * to virtual address to get
					       * dma handle in this region */
	dma_addr_t	       dma_address;   /* DMA handle of the start,
					       * for unmap */
	unsigned int	       dma_len;	      /* DMA length */
};

typedef enum {
  AHC_BUG_NONE            = 0x0000,
  AHC_BUG_TMODE_WIDEODD   = 0x0001,
  AHC_BUG_AUTOFLUSH       = 0x0002,
  AHC_BUG_CACHETHEN       = 0x0004,
  AHC_BUG_CACHETHEN_DIS   = 0x0008,
  AHC_BUG_PCI_2_1_RETRY   = 0x0010,
  AHC_BUG_PCI_MWI         = 0x0020,
  AHC_BUG_SCBCHAN_UPLOAD  = 0x0040,
} ahc_bugs;

struct aic7xxx_scb {
	struct aic7xxx_hwscb	*hscb;		/* corresponding hardware scb */
	struct scsi_cmnd	*cmd;		/* scsi_cmnd for this scb */
	struct aic7xxx_scb	*q_next;        /* next scb in queue */
	volatile scb_flag_type	flags;		/* current state of scb */
	struct hw_scatterlist	*sg_list;	/* SG list in adapter format */
	unsigned char		tag_action;
	unsigned char		sg_count;
	unsigned char		*sense_cmd;	/*
						 * Allocate 6 characters for
						 * sense command.
						 */
	unsigned char		*cmnd;
	unsigned int		sg_length;	/*
						 * We init this during
						 * buildscb so we don't have
						 * to calculate anything during
						 * underflow/overflow/stat code
						 */
	void			*kmalloc_ptr;
	struct aic7xxx_scb_dma	*scb_dma;
};

/*
 * Define a linked list of SCBs.
 */
typedef struct {
  struct aic7xxx_scb *head;
  struct aic7xxx_scb *tail;
} scb_queue_type;

static struct {
  unsigned char errno;
  const char *errmesg;
} hard_error[] = {
  { ILLHADDR,  "Illegal Host Access" },
  { ILLSADDR,  "Illegal Sequencer Address referenced" },
  { ILLOPCODE, "Illegal Opcode in sequencer program" },
  { SQPARERR,  "Sequencer Ram Parity Error" },
  { DPARERR,   "Data-Path Ram Parity Error" },
  { MPARERR,   "Scratch Ram/SCB Array Ram Parity Error" },
  { PCIERRSTAT,"PCI Error detected" },
  { CIOPARERR, "CIOBUS Parity Error" }
};

static unsigned char
generic_sense[] = { REQUEST_SENSE, 0, 0, 0, 255, 0 };

typedef struct {
  scb_queue_type free_scbs;        /*
                                    * SCBs assigned to free slot on
                                    * card (no paging required)
                                    */
  struct aic7xxx_scb   *scb_array[AIC7XXX_MAXSCB];
  struct aic7xxx_hwscb *hscbs;
  unsigned char  numscbs;          /* current number of scbs */
  unsigned char  maxhscbs;         /* hardware scbs */
  unsigned char  maxscbs;          /* max scbs including pageable scbs */
  dma_addr_t	 hscbs_dma;	   /* DMA handle to hscbs */
  unsigned int   hscbs_dma_len;    /* length of the above DMA area */
  void          *hscb_kmalloc_ptr;
} scb_data_type;

struct target_cmd {
  unsigned char mesg_bytes[4];
  unsigned char command[28];
};

#define AHC_TRANS_CUR    0x0001
#define AHC_TRANS_ACTIVE 0x0002
#define AHC_TRANS_GOAL   0x0004
#define AHC_TRANS_USER   0x0008
#define AHC_TRANS_QUITE  0x0010
typedef struct {
  unsigned char width;
  unsigned char period;
  unsigned char offset;
  unsigned char options;
} transinfo_type;

struct aic_dev_data {
  volatile scb_queue_type  delayed_scbs;
  volatile unsigned short  temp_q_depth;
  unsigned short           max_q_depth;
  volatile unsigned char   active_cmds;
  /*
   * Statistics Kept:
   *
   * Total Xfers (count for each command that has a data xfer),
   * broken down by reads && writes.
   *
   * Further sorted into a few bins for keeping tabs on how many commands
   * we get of various sizes.
   *
   */
  long w_total;                          /* total writes */
  long r_total;                          /* total reads */
  long barrier_total;			 /* total num of REQ_BARRIER commands */
  long ordered_total;			 /* How many REQ_BARRIER commands we
					    used ordered tags to satisfy */
  long w_bins[6];                       /* binned write */
  long r_bins[6];                       /* binned reads */
  transinfo_type	cur;
  transinfo_type	goal;
#define  BUS_DEVICE_RESET_PENDING       0x01
#define  DEVICE_RESET_DELAY             0x02
#define  DEVICE_PRINT_DTR               0x04
#define  DEVICE_WAS_BUSY                0x08
#define  DEVICE_DTR_SCANNED		0x10
#define  DEVICE_SCSI_3			0x20
  volatile unsigned char   flags;
  unsigned needppr:1;
  unsigned needppr_copy:1;
  unsigned needsdtr:1;
  unsigned needsdtr_copy:1;
  unsigned needwdtr:1;
  unsigned needwdtr_copy:1;
  unsigned dtr_pending:1;
  struct scsi_device *SDptr;
  struct list_head list;
};

/*
 * Define a structure used for each host adapter.  Note, in order to avoid
 * problems with architectures I can't test on (because I don't have one,
 * such as the Alpha based systems) which happen to give faults for
 * non-aligned memory accesses, care was taken to align this structure
 * in a way that gauranteed all accesses larger than 8 bits were aligned
 * on the appropriate boundary.  It's also organized to try and be more
 * cache line efficient.  Be careful when changing this lest you might hurt
 * overall performance and bring down the wrath of the masses.
 */
struct aic7xxx_host {
  /*
   *  This is the first 64 bytes in the host struct
   */

  /*
   * We are grouping things here....first, items that get either read or
   * written with nearly every interrupt
   */
	volatile long	flags;
	ahc_feature	features;	/* chip features */
	unsigned long	base;		/* card base address */
	volatile unsigned char  __iomem *maddr;	/* memory mapped address */
	unsigned long	isr_count;	/* Interrupt count */
	unsigned long	spurious_int;
	scb_data_type	*scb_data;
	struct aic7xxx_cmd_queue {
		struct scsi_cmnd *head;
		struct scsi_cmnd *tail;
	} completeq;

	/*
	* Things read/written on nearly every entry into aic7xxx_queue()
	*/
	volatile scb_queue_type	waiting_scbs;
	unsigned char	unpause;	/* unpause value for HCNTRL */
	unsigned char	pause;		/* pause value for HCNTRL */
	volatile unsigned char	qoutfifonext;
	volatile unsigned char	activescbs;	/* active scbs */
	volatile unsigned char	max_activescbs;
	volatile unsigned char	qinfifonext;
	volatile unsigned char	*untagged_scbs;
	volatile unsigned char	*qoutfifo;
	volatile unsigned char	*qinfifo;

	unsigned char	dev_last_queue_full[MAX_TARGETS];
	unsigned char	dev_last_queue_full_count[MAX_TARGETS];
	unsigned short	ultraenb; /* Gets downloaded to card as a bitmap */
	unsigned short	discenable; /* Gets downloaded to card as a bitmap */
	transinfo_type	user[MAX_TARGETS];

	unsigned char	msg_buf[13];	/* The message for the target */
	unsigned char	msg_type;
#define MSG_TYPE_NONE              0x00
#define MSG_TYPE_INITIATOR_MSGOUT  0x01
#define MSG_TYPE_INITIATOR_MSGIN   0x02
	unsigned char	msg_len;	/* Length of message */
	unsigned char	msg_index;	/* Index into msg_buf array */


	/*
	 * We put the less frequently used host structure items
	 * after the more frequently used items to try and ease
	 * the burden on the cache subsystem.
	 * These entries are not *commonly* accessed, whereas
	 * the preceding entries are accessed very often.
	 */

	unsigned int	irq;		/* IRQ for this adapter */
	int		instance;	/* aic7xxx instance number */
	int		scsi_id;	/* host adapter SCSI ID */
	int		scsi_id_b;	/* channel B for twin adapters */
	unsigned int	bios_address;
	int		board_name_index;
	unsigned short	bios_control;		/* bios control - SEEPROM */
	unsigned short	adapter_control;	/* adapter control - SEEPROM */
	struct pci_dev	*pdev;
	unsigned char	pci_bus;
	unsigned char	pci_device_fn;
	struct seeprom_config	sc;
	unsigned short	sc_type;
	unsigned short	sc_size;
	struct aic7xxx_host	*next;	/* allow for multiple IRQs */
	struct Scsi_Host	*host;	/* pointer to scsi host */
	struct list_head	 aic_devs; /* all aic_dev structs on host */
	int		host_no;	/* SCSI host number */
	unsigned long	mbase;		/* I/O memory address */
	ahc_chip	chip;		/* chip type */
	ahc_bugs	bugs;
	dma_addr_t	fifo_dma;	/* DMA handle for fifo arrays */
};

/*
 * Valid SCSIRATE values. (p. 3-17)
 * Provides a mapping of transfer periods in ns/4 to the proper value to
 * stick in the SCSIRATE reg to use that transfer rate.
 */
#define AHC_SYNCRATE_ULTRA3 0
#define AHC_SYNCRATE_ULTRA2 1
#define AHC_SYNCRATE_ULTRA  3
#define AHC_SYNCRATE_FAST   6
#define AHC_SYNCRATE_CRC 0x40
#define AHC_SYNCRATE_SE  0x10
static struct aic7xxx_syncrate {
  /* Rates in Ultra mode have bit 8 of sxfr set */
#define                ULTRA_SXFR 0x100
  int sxfr_ultra2;
  int sxfr;
  unsigned char period;
  const char *rate[2];
} aic7xxx_syncrates[] = {
  { 0x42,  0x000,   9,  {"80.0", "160.0"} },
  { 0x13,  0x000,  10,  {"40.0", "80.0"} },
  { 0x14,  0x000,  11,  {"33.0", "66.6"} },
  { 0x15,  0x100,  12,  {"20.0", "40.0"} },
  { 0x16,  0x110,  15,  {"16.0", "32.0"} },
  { 0x17,  0x120,  18,  {"13.4", "26.8"} },
  { 0x18,  0x000,  25,  {"10.0", "20.0"} },
  { 0x19,  0x010,  31,  {"8.0",  "16.0"} },
  { 0x1a,  0x020,  37,  {"6.67", "13.3"} },
  { 0x1b,  0x030,  43,  {"5.7",  "11.4"} },
  { 0x10,  0x040,  50,  {"5.0",  "10.0"} },
  { 0x00,  0x050,  56,  {"4.4",  "8.8" } },
  { 0x00,  0x060,  62,  {"4.0",  "8.0" } },
  { 0x00,  0x070,  68,  {"3.6",  "7.2" } },
  { 0x00,  0x000,  0,   {NULL, NULL}   },
};

#define CTL_OF_SCB(scb) (((scb->hscb)->target_channel_lun >> 3) & 0x1),  \
                        (((scb->hscb)->target_channel_lun >> 4) & 0xf), \
                        ((scb->hscb)->target_channel_lun & 0x07)

#define CTL_OF_CMD(cmd) ((cmd->device->channel) & 0x01),  \
                        ((cmd->device->id) & 0x0f), \
                        ((cmd->device->lun) & 0x07)

#define TARGET_INDEX(cmd)  ((cmd)->device->id | ((cmd)->device->channel << 3))

/*
 * A nice little define to make doing our printks a little easier
 */

#define WARN_LEAD KERN_WARNING "(scsi%d:%d:%d:%d) "
#define INFO_LEAD KERN_INFO "(scsi%d:%d:%d:%d) "

/*
 * XXX - these options apply unilaterally to _all_ 274x/284x/294x
 *       cards in the system.  This should be fixed.  Exceptions to this
 *       rule are noted in the comments.
 */

/*
 * Use this as the default queue depth when setting tagged queueing on.
 */
static unsigned int aic7xxx_default_queue_depth = AIC7XXX_CMDS_PER_DEVICE;

/*
 * Skip the scsi bus reset.  Non 0 make us skip the reset at startup.  This
 * has no effect on any later resets that might occur due to things like
 * SCSI bus timeouts.
 */
static unsigned int aic7xxx_no_reset = 0;
/*
 * Certain PCI motherboards will scan PCI devices from highest to lowest,
 * others scan from lowest to highest, and they tend to do all kinds of
 * strange things when they come into contact with PCI bridge chips.  The
 * net result of all this is that the PCI card that is actually used to boot
 * the machine is very hard to detect.  Most motherboards go from lowest
 * PCI slot number to highest, and the first SCSI controller found is the
 * one you boot from.  The only exceptions to this are when a controller
 * has its BIOS disabled.  So, we by default sort all of our SCSI controllers
 * from lowest PCI slot number to highest PCI slot number.  We also force
 * all controllers with their BIOS disabled to the end of the list.  This
 * works on *almost* all computers.  Where it doesn't work, we have this
 * option.  Setting this option to non-0 will reverse the order of the sort
 * to highest first, then lowest, but will still leave cards with their BIOS
 * disabled at the very end.  That should fix everyone up unless there are
 * really strange cirumstances.
 */
static int aic7xxx_reverse_scan = 0;
/*
 * Should we force EXTENDED translation on a controller.
 *     0 == Use whatever is in the SEEPROM or default to off
 *     1 == Use whatever is in the SEEPROM or default to on
 */
static unsigned int aic7xxx_extended = 0;
/*
 * The IRQ trigger method used on EISA controllers. Does not effect PCI cards.
 *   -1 = Use detected settings.
 *    0 = Force Edge triggered mode.
 *    1 = Force Level triggered mode.
 */
static int aic7xxx_irq_trigger = -1;
/*
 * This variable is used to override the termination settings on a controller.
 * This should not be used under normal conditions.  However, in the case
 * that a controller does not have a readable SEEPROM (so that we can't
 * read the SEEPROM settings directly) and that a controller has a buggered
 * version of the cable detection logic, this can be used to force the 
 * correct termination.  It is preferable to use the manual termination
 * settings in the BIOS if possible, but some motherboard controllers store
 * those settings in a format we can't read.  In other cases, auto term
 * should also work, but the chipset was put together with no auto term
 * logic (common on motherboard controllers).  In those cases, we have
 * 32 bits here to work with.  That's good for 8 controllers/channels.  The
 * bits are organized as 4 bits per channel, with scsi0 getting the lowest
 * 4 bits in the int.  A 1 in a bit position indicates the termination setting
 * that corresponds to that bit should be enabled, a 0 is disabled.
 * It looks something like this:
 *
 *    0x0f =  1111-Single Ended Low Byte Termination on/off
 *            ||\-Single Ended High Byte Termination on/off
 *            |\-LVD Low Byte Termination on/off
 *            \-LVD High Byte Termination on/off
 *
 * For non-Ultra2 controllers, the upper 2 bits are not important.  So, to
 * enable both high byte and low byte termination on scsi0, I would need to
 * make sure that the override_term variable was set to 0x03 (bits 0011).
 * To make sure that all termination is enabled on an Ultra2 controller at
 * scsi2 and only high byte termination on scsi1 and high and low byte
 * termination on scsi0, I would set override_term=0xf23 (bits 1111 0010 0011)
 *
 * For the most part, users should never have to use this, that's why I
 * left it fairly cryptic instead of easy to understand.  If you need it,
 * most likely someone will be telling you what your's needs to be set to.
 */
static int aic7xxx_override_term = -1;
/*
 * Certain motherboard chipset controllers tend to screw
 * up the polarity of the term enable output pin.  Use this variable
 * to force the correct polarity for your system.  This is a bitfield variable
 * similar to the previous one, but this one has one bit per channel instead
 * of four.
 *    0 = Force the setting to active low.
 *    1 = Force setting to active high.
 * Most Adaptec cards are active high, several motherboards are active low.
 * To force a 2940 card at SCSI 0 to active high and a motherboard 7895
 * controller at scsi1 and scsi2 to active low, and a 2910 card at scsi3
 * to active high, you would need to set stpwlev=0x9 (bits 1001).
 *
 * People shouldn't need to use this, but if you are experiencing lots of
 * SCSI timeout problems, this may help.  There is one sure way to test what
 * this option needs to be.  Using a boot floppy to boot the system, configure
 * your system to enable all SCSI termination (in the Adaptec SCSI BIOS) and
 * if needed then also pass a value to override_term to make sure that the
 * driver is enabling SCSI termination, then set this variable to either 0
 * or 1.  When the driver boots, make sure there are *NO* SCSI cables
 * connected to your controller.  If it finds and inits the controller
 * without problem, then the setting you passed to stpwlev was correct.  If
 * the driver goes into a reset loop and hangs the system, then you need the
 * other setting for this variable.  If neither setting lets the machine
 * boot then you have definite termination problems that may not be fixable.
 */
static int aic7xxx_stpwlev = -1;
/*
 * Set this to non-0 in order to force the driver to panic the kernel
 * and print out debugging info on a SCSI abort or reset cycle.
 */
static int aic7xxx_panic_on_abort = 0;
/*
 * PCI bus parity checking of the Adaptec controllers.  This is somewhat
 * dubious at best.  To my knowledge, this option has never actually
 * solved a PCI parity problem, but on certain machines with broken PCI
 * chipset configurations, it can generate tons of false error messages.
 * It's included in the driver for completeness.
 *   0 = Shut off PCI parity check
 *  -1 = Normal polarity pci parity checking
 *   1 = reverse polarity pci parity checking
 *
 * NOTE: you can't actually pass -1 on the lilo prompt.  So, to set this
 * variable to -1 you would actually want to simply pass the variable
 * name without a number.  That will invert the 0 which will result in
 * -1.
 */
static int aic7xxx_pci_parity = 0;
/*
 * Set this to any non-0 value to cause us to dump the contents of all
 * the card's registers in a hex dump format tailored to each model of
 * controller.
 * 
 * NOTE: THE CONTROLLER IS LEFT IN AN UNUSEABLE STATE BY THIS OPTION.
 *       YOU CANNOT BOOT UP WITH THIS OPTION, IT IS FOR DEBUGGING PURPOSES
 *       ONLY
 */
static int aic7xxx_dump_card = 0;
/*
 * Set this to a non-0 value to make us dump out the 32 bit instruction
 * registers on the card after completing the sequencer download.  This
 * allows the actual sequencer download to be verified.  It is possible
 * to use this option and still boot up and run your system.  This is
 * only intended for debugging purposes.
 */
static int aic7xxx_dump_sequencer = 0;
/*
 * Certain newer motherboards have put new PCI based devices into the
 * IO spaces that used to typically be occupied by VLB or EISA cards.
 * This overlap can cause these newer motherboards to lock up when scanned
 * for older EISA and VLB devices.  Setting this option to non-0 will
 * cause the driver to skip scanning for any VLB or EISA controllers and
 * only support the PCI controllers.  NOTE: this means that if the kernel
 * os compiled with PCI support disabled, then setting this to non-0
 * would result in never finding any devices :)
 */
static int aic7xxx_no_probe = 0;
/*
 * On some machines, enabling the external SCB RAM isn't reliable yet.  I
 * haven't had time to make test patches for things like changing the
 * timing mode on that external RAM either.  Some of those changes may
 * fix the problem.  Until then though, we default to external SCB RAM
 * off and give a command line option to enable it.
 */
static int aic7xxx_scbram = 0;
/*
 * So that we can set how long each device is given as a selection timeout.
 * The table of values goes like this:
 *   0 - 256ms
 *   1 - 128ms
 *   2 - 64ms
 *   3 - 32ms
 * We default to 64ms because it's fast.  Some old SCSI-I devices need a
 * longer time.  The final value has to be left shifted by 3, hence 0x10
 * is the final value.
 */
static int aic7xxx_seltime = 0x10;
/*
 * So that insmod can find the variable and make it point to something
 */
#ifdef MODULE
static char * aic7xxx = NULL;
module_param(aic7xxx, charp, 0);
#endif

#define VERBOSE_NORMAL         0x0000
#define VERBOSE_NEGOTIATION    0x0001
#define VERBOSE_SEQINT         0x0002
#define VERBOSE_SCSIINT        0x0004
#define VERBOSE_PROBE          0x0008
#define VERBOSE_PROBE2         0x0010
#define VERBOSE_NEGOTIATION2   0x0020
#define VERBOSE_MINOR_ERROR    0x0040
#define VERBOSE_TRACING        0x0080
#define VERBOSE_ABORT          0x0f00
#define VERBOSE_ABORT_MID      0x0100
#define VERBOSE_ABORT_FIND     0x0200
#define VERBOSE_ABORT_PROCESS  0x0400
#define VERBOSE_ABORT_RETURN   0x0800
#define VERBOSE_RESET          0xf000
#define VERBOSE_RESET_MID      0x1000
#define VERBOSE_RESET_FIND     0x2000
#define VERBOSE_RESET_PROCESS  0x4000
#define VERBOSE_RESET_RETURN   0x8000
static int aic7xxx_verbose = VERBOSE_NORMAL | VERBOSE_NEGOTIATION |
           VERBOSE_PROBE;                     /* verbose messages */


/****************************************************************************
 *
 * We're going to start putting in function declarations so that order of
 * functions is no longer important.  As needed, they are added here.
 *
 ***************************************************************************/

static int aic7xxx_release(struct Scsi_Host *host);
static void aic7xxx_set_syncrate(struct aic7xxx_host *p, 
		struct aic7xxx_syncrate *syncrate, int target, int channel,
		unsigned int period, unsigned int offset, unsigned char options,
		unsigned int type, struct aic_dev_data *aic_dev);
static void aic7xxx_set_width(struct aic7xxx_host *p, int target, int channel,
		int lun, unsigned int width, unsigned int type,
		struct aic_dev_data *aic_dev);
static void aic7xxx_panic_abort(struct aic7xxx_host *p, struct scsi_cmnd *cmd);
static void aic7xxx_print_card(struct aic7xxx_host *p);
static void aic7xxx_print_scratch_ram(struct aic7xxx_host *p);
static void aic7xxx_print_sequencer(struct aic7xxx_host *p, int downloaded);
#ifdef AIC7XXX_VERBOSE_DEBUGGING
static void aic7xxx_check_scbs(struct aic7xxx_host *p, char *buffer);
#endif

/****************************************************************************
 *
 * These functions are now used.  They happen to be wrapped in useless
 * inb/outb port read/writes around the real reads and writes because it
 * seems that certain very fast CPUs have a problem dealing with us when
 * going at full speed.
 *
 ***************************************************************************/

static unsigned char
aic_inb(struct aic7xxx_host *p, long port)
{
#ifdef MMAPIO
  unsigned char x;
  if(p->maddr)
  {
    x = readb(p->maddr + port);
  }
  else
  {
    x = inb(p->base + port);
  }
  return(x);
#else
  return(inb(p->base + port));
#endif
}

static void
aic_outb(struct aic7xxx_host *p, unsigned char val, long port)
{
#ifdef MMAPIO
  if(p->maddr)
  {
    writeb(val, p->maddr + port);
    mb(); /* locked operation in order to force CPU ordering */
    readb(p->maddr + HCNTRL); /* dummy read to flush the PCI write */
  }
  else
  {
    outb(val, p->base + port);
    mb(); /* locked operation in order to force CPU ordering */
  }
#else
  outb(val, p->base + port);
  mb(); /* locked operation in order to force CPU ordering */
#endif
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_setup
 *
 * Description:
 *   Handle Linux boot parameters. This routine allows for assigning a value
 *   to a parameter with a ':' between the parameter and the value.
 *   ie. aic7xxx=unpause:0x0A,extended
 *-F*************************************************************************/
static int
aic7xxx_setup(char *s)
{
  int   i, n;
  char *p;
  char *end;

  static struct {
    const char *name;
    unsigned int *flag;
  } options[] = {
    { "extended",    &aic7xxx_extended },
    { "no_reset",    &aic7xxx_no_reset },
    { "irq_trigger", &aic7xxx_irq_trigger },
    { "verbose",     &aic7xxx_verbose },
    { "reverse_scan",&aic7xxx_reverse_scan },
    { "override_term", &aic7xxx_override_term },
    { "stpwlev", &aic7xxx_stpwlev },
    { "no_probe", &aic7xxx_no_probe },
    { "panic_on_abort", &aic7xxx_panic_on_abort },
    { "pci_parity", &aic7xxx_pci_parity },
    { "dump_card", &aic7xxx_dump_card },
    { "dump_sequencer", &aic7xxx_dump_sequencer },
    { "default_queue_depth", &aic7xxx_default_queue_depth },
    { "scbram", &aic7xxx_scbram },
    { "seltime", &aic7xxx_seltime },
    { "tag_info",    NULL }
  };

  end = strchr(s, '\0');

  while ((p = strsep(&s, ",.")) != NULL)
  {
    for (i = 0; i < ARRAY_SIZE(options); i++)
    {
      n = strlen(options[i].name);
      if (!strncmp(options[i].name, p, n))
      {
        if (!strncmp(p, "tag_info", n))
        {
          if (p[n] == ':')
          {
            char *base;
            char *tok, *tok_end, *tok_end2;
            char tok_list[] = { '.', ',', '{', '}', '\0' };
            int i, instance = -1, device = -1;
            unsigned char done = FALSE;

            base = p;
            tok = base + n + 1;  /* Forward us just past the ':' */
            tok_end = strchr(tok, '\0');
            if (tok_end < end)
              *tok_end = ',';
            while(!done)
            {
              switch(*tok)
              {
                case '{':
                  if (instance == -1)
                    instance = 0;
                  else if (device == -1)
                    device = 0;
                  tok++;
                  break;
                case '}':
                  if (device != -1)
                    device = -1;
                  else if (instance != -1)
                    instance = -1;
                  tok++;
                  break;
                case ',':
                case '.':
                  if (instance == -1)
                    done = TRUE;
                  else if (device >= 0)
                    device++;
                  else if (instance >= 0)
                    instance++;
                  if ( (device >= MAX_TARGETS) || 
                       (instance >= ARRAY_SIZE(aic7xxx_tag_info)) )
                    done = TRUE;
                  tok++;
                  if (!done)
                  {
                    base = tok;
                  }
                  break;
                case '\0':
                  done = TRUE;
                  break;
                default:
                  done = TRUE;
                  tok_end = strchr(tok, '\0');
                  for(i=0; tok_list[i]; i++)
                  {
                    tok_end2 = strchr(tok, tok_list[i]);
                    if ( (tok_end2) && (tok_end2 < tok_end) )
                    {
                      tok_end = tok_end2;
                      done = FALSE;
                    }
                  }
                  if ( (instance >= 0) && (device >= 0) &&
                       (instance < ARRAY_SIZE(aic7xxx_tag_info)) &&
                       (device < MAX_TARGETS) )
                    aic7xxx_tag_info[instance].tag_commands[device] =
                      simple_strtoul(tok, NULL, 0) & 0xff;
                  tok = tok_end;
                  break;
              }
            }
            while((p != base) && (p != NULL))
              p = strsep(&s, ",.");
          }
        }
        else if (p[n] == ':')
        {
          *(options[i].flag) = simple_strtoul(p + n + 1, NULL, 0);
          if(!strncmp(p, "seltime", n))
          {
            *(options[i].flag) = (*(options[i].flag) % 4) << 3;
          }
        }
        else if (!strncmp(p, "verbose", n))
        {
          *(options[i].flag) = 0xff29;
        }
        else
        {
          *(options[i].flag) = ~(*(options[i].flag));
          if(!strncmp(p, "seltime", n))
          {
            *(options[i].flag) = (*(options[i].flag) % 4) << 3;
          }
        }
      }
    }
  }
  return 1;
}

__setup("aic7xxx=", aic7xxx_setup);

/*+F*************************************************************************
 * Function:
 *   pause_sequencer
 *
 * Description:
 *   Pause the sequencer and wait for it to actually stop - this
 *   is important since the sequencer can disable pausing for critical
 *   sections.
 *-F*************************************************************************/
static void
pause_sequencer(struct aic7xxx_host *p)
{
  aic_outb(p, p->pause, HCNTRL);
  while ((aic_inb(p, HCNTRL) & PAUSE) == 0)
  {
    ;
  }
  if(p->features & AHC_ULTRA2)
  {
    aic_inb(p, CCSCBCTL);
  }
}

/*+F*************************************************************************
 * Function:
 *   unpause_sequencer
 *
 * Description:
 *   Unpause the sequencer. Unremarkable, yet done often enough to
 *   warrant an easy way to do it.
 *-F*************************************************************************/
static void
unpause_sequencer(struct aic7xxx_host *p, int unpause_always)
{
  if (unpause_always ||
      ( !(aic_inb(p, INTSTAT) & (SCSIINT | SEQINT | BRKADRINT)) &&
        !(p->flags & AHC_HANDLING_REQINITS) ) )
  {
    aic_outb(p, p->unpause, HCNTRL);
  }
}

/*+F*************************************************************************
 * Function:
 *   restart_sequencer
 *
 * Description:
 *   Restart the sequencer program from address zero.  This assumes
 *   that the sequencer is already paused.
 *-F*************************************************************************/
static void
restart_sequencer(struct aic7xxx_host *p)
{
  aic_outb(p, 0, SEQADDR0);
  aic_outb(p, 0, SEQADDR1);
  aic_outb(p, FASTMODE, SEQCTL);
}

/*
 * We include the aic7xxx_seq.c file here so that the other defines have
 * already been made, and so that it comes before the code that actually
 * downloads the instructions (since we don't typically use function
 * prototype, our code has to be ordered that way, it's a left-over from
 * the original driver days.....I should fix it some time DL).
 */
#include "aic7xxx_old/aic7xxx_seq.c"

/*+F*************************************************************************
 * Function:
 *   aic7xxx_check_patch
 *
 * Description:
 *   See if the next patch to download should be downloaded.
 *-F*************************************************************************/
static int
aic7xxx_check_patch(struct aic7xxx_host *p,
  struct sequencer_patch **start_patch, int start_instr, int *skip_addr)
{
  struct sequencer_patch *cur_patch;
  struct sequencer_patch *last_patch;
  int num_patches;

  num_patches = ARRAY_SIZE(sequencer_patches);
  last_patch = &sequencer_patches[num_patches];
  cur_patch = *start_patch;

  while ((cur_patch < last_patch) && (start_instr == cur_patch->begin))
  {
    if (cur_patch->patch_func(p) == 0)
    {
      /*
       * Start rejecting code.
       */
      *skip_addr = start_instr + cur_patch->skip_instr;
      cur_patch += cur_patch->skip_patch;
    }
    else
    {
      /*
       * Found an OK patch.  Advance the patch pointer to the next patch
       * and wait for our instruction pointer to get here.
       */
      cur_patch++;
    }
  }

  *start_patch = cur_patch;
  if (start_instr < *skip_addr)
    /*
     * Still skipping
     */
    return (0);
  return(1);
}


/*+F*************************************************************************
 * Function:
 *   aic7xxx_download_instr
 *
 * Description:
 *   Find the next patch to download.
 *-F*************************************************************************/
static void
aic7xxx_download_instr(struct aic7xxx_host *p, int instrptr,
  unsigned char *dconsts)
{
  union ins_formats instr;
  struct ins_format1 *fmt1_ins;
  struct ins_format3 *fmt3_ins;
  unsigned char opcode;

  instr = *(union ins_formats*) &seqprog[instrptr * 4];

  instr.integer = le32_to_cpu(instr.integer);
  
  fmt1_ins = &instr.format1;
  fmt3_ins = NULL;

  /* Pull the opcode */
  opcode = instr.format1.opcode;
  switch (opcode)
  {
    case AIC_OP_JMP:
    case AIC_OP_JC:
    case AIC_OP_JNC:
    case AIC_OP_CALL:
    case AIC_OP_JNE:
    case AIC_OP_JNZ:
    case AIC_OP_JE:
    case AIC_OP_JZ:
    {
      struct sequencer_patch *cur_patch;
      int address_offset;
      unsigned int address;
      int skip_addr;
      int i;

      fmt3_ins = &instr.format3;
      address_offset = 0;
      address = fmt3_ins->address;
      cur_patch = sequencer_patches;
      skip_addr = 0;

      for (i = 0; i < address;)
      {
        aic7xxx_check_patch(p, &cur_patch, i, &skip_addr);
        if (skip_addr > i)
        {
          int end_addr;

          end_addr = min_t(int, address, skip_addr);
          address_offset += end_addr - i;
          i = skip_addr;
        }
        else
        {
          i++;
        }
      }
      address -= address_offset;
      fmt3_ins->address = address;
      /* Fall Through to the next code section */
    }
    case AIC_OP_OR:
    case AIC_OP_AND:
    case AIC_OP_XOR:
    case AIC_OP_ADD:
    case AIC_OP_ADC:
    case AIC_OP_BMOV:
      if (fmt1_ins->parity != 0)
      {
        fmt1_ins->immediate = dconsts[fmt1_ins->immediate];
      }
      fmt1_ins->parity = 0;
      /* Fall Through to the next code section */
    case AIC_OP_ROL:
      if ((p->features & AHC_ULTRA2) != 0)
      {
        int i, count;

        /* Calculate odd parity for the instruction */
        for ( i=0, count=0; i < 31; i++)
        {
          unsigned int mask;

          mask = 0x01 << i;
          if ((instr.integer & mask) != 0)
            count++;
        }
        if (!(count & 0x01))
          instr.format1.parity = 1;
      }
      else
      {
        if (fmt3_ins != NULL)
        {
          instr.integer =  fmt3_ins->immediate |
                          (fmt3_ins->source << 8) |
                          (fmt3_ins->address << 16) |
                          (fmt3_ins->opcode << 25);
        }
        else
        {
          instr.integer =  fmt1_ins->immediate |
                          (fmt1_ins->source << 8) |
                          (fmt1_ins->destination << 16) |
                          (fmt1_ins->ret << 24) |
                          (fmt1_ins->opcode << 25);
        }
      }
      aic_outb(p, (instr.integer & 0xff), SEQRAM);
      aic_outb(p, ((instr.integer >> 8) & 0xff), SEQRAM);
      aic_outb(p, ((instr.integer >> 16) & 0xff), SEQRAM);
      aic_outb(p, ((instr.integer >> 24) & 0xff), SEQRAM);
      udelay(10);
      break;

    default:
      panic("aic7xxx: Unknown opcode encountered in sequencer program.");
      break;
  }
}


/*+F*************************************************************************
 * Function:
 *   aic7xxx_loadseq
 *
 * Description:
 *   Load the sequencer code into the controller memory.
 *-F*************************************************************************/
static void
aic7xxx_loadseq(struct aic7xxx_host *p)
{
  struct sequencer_patch *cur_patch;
  int i;
  int downloaded;
  int skip_addr;
  unsigned char download_consts[4] = {0, 0, 0, 0};

  if (aic7xxx_verbose & VERBOSE_PROBE)
  {
    printk(KERN_INFO "(scsi%d) Downloading sequencer code...", p->host_no);
  }
#if 0
  download_consts[TMODE_NUMCMDS] = p->num_targetcmds;
#endif
  download_consts[TMODE_NUMCMDS] = 0;
  cur_patch = &sequencer_patches[0];
  downloaded = 0;
  skip_addr = 0;

  aic_outb(p, PERRORDIS|LOADRAM|FAILDIS|FASTMODE, SEQCTL);
  aic_outb(p, 0, SEQADDR0);
  aic_outb(p, 0, SEQADDR1);

  for (i = 0; i < sizeof(seqprog) / 4;  i++)
  {
    if (aic7xxx_check_patch(p, &cur_patch, i, &skip_addr) == 0)
    {
      /* Skip this instruction for this configuration. */
      continue;
    }
    aic7xxx_download_instr(p, i, &download_consts[0]);
    downloaded++;
  }

  aic_outb(p, 0, SEQADDR0);
  aic_outb(p, 0, SEQADDR1);
  aic_outb(p, FASTMODE | FAILDIS, SEQCTL);
  unpause_sequencer(p, TRUE);
  mdelay(1);
  pause_sequencer(p);
  aic_outb(p, FASTMODE, SEQCTL);
  if (aic7xxx_verbose & VERBOSE_PROBE)
  {
    printk(" %d instructions downloaded\n", downloaded);
  }
  if (aic7xxx_dump_sequencer)
    aic7xxx_print_sequencer(p, downloaded);
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_print_sequencer
 *
 * Description:
 *   Print the contents of the sequencer memory to the screen.
 *-F*************************************************************************/
static void
aic7xxx_print_sequencer(struct aic7xxx_host *p, int downloaded)
{
  int i, k, temp;
  
  aic_outb(p, PERRORDIS|LOADRAM|FAILDIS|FASTMODE, SEQCTL);
  aic_outb(p, 0, SEQADDR0);
  aic_outb(p, 0, SEQADDR1);

  k = 0;
  for (i=0; i < downloaded; i++)
  {
    if ( k == 0 )
      printk("%03x: ", i);
    temp = aic_inb(p, SEQRAM);
    temp |= (aic_inb(p, SEQRAM) << 8);
    temp |= (aic_inb(p, SEQRAM) << 16);
    temp |= (aic_inb(p, SEQRAM) << 24);
    printk("%08x", temp);
    if ( ++k == 8 )
    {
      printk("\n");
      k = 0;
    }
    else
      printk(" ");
  }
  aic_outb(p, 0, SEQADDR0);
  aic_outb(p, 0, SEQADDR1);
  aic_outb(p, FASTMODE | FAILDIS, SEQCTL);
  unpause_sequencer(p, TRUE);
  mdelay(1);
  pause_sequencer(p);
  aic_outb(p, FASTMODE, SEQCTL);
  printk("\n");
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_info
 *
 * Description:
 *   Return a string describing the driver.
 *-F*************************************************************************/
static const char *
aic7xxx_info(struct Scsi_Host *dooh)
{
  static char buffer[256];
  char *bp;
  struct aic7xxx_host *p;

  bp = &buffer[0];
  p = (struct aic7xxx_host *)dooh->hostdata;
  memset(bp, 0, sizeof(buffer));
  strcpy(bp, "Adaptec AHA274x/284x/294x (EISA/VLB/PCI-Fast SCSI) ");
  strcat(bp, AIC7XXX_C_VERSION);
  strcat(bp, "/");
  strcat(bp, AIC7XXX_H_VERSION);
  strcat(bp, "\n");
  strcat(bp, "       <");
  strcat(bp, board_names[p->board_name_index]);
  strcat(bp, ">");

  return(bp);
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_find_syncrate
 *
 * Description:
 *   Look up the valid period to SCSIRATE conversion in our table
 *-F*************************************************************************/
static struct aic7xxx_syncrate *
aic7xxx_find_syncrate(struct aic7xxx_host *p, unsigned int *period,
  unsigned int maxsync, unsigned char *options)
{
  struct aic7xxx_syncrate *syncrate;
  int done = FALSE;

  switch(*options)
  {
    case MSG_EXT_PPR_OPTION_DT_CRC:
    case MSG_EXT_PPR_OPTION_DT_UNITS:
      if(!(p->features & AHC_ULTRA3))
      {
        *options = 0;
        maxsync = max_t(unsigned int, maxsync, AHC_SYNCRATE_ULTRA2);
      }
      break;
    case MSG_EXT_PPR_OPTION_DT_CRC_QUICK:
    case MSG_EXT_PPR_OPTION_DT_UNITS_QUICK:
      if(!(p->features & AHC_ULTRA3))
      {
        *options = 0;
        maxsync = max_t(unsigned int, maxsync, AHC_SYNCRATE_ULTRA2);
      }
      else
      {
        /*
         * we don't support the Quick Arbitration variants of dual edge
         * clocking.  As it turns out, we want to send back the
         * same basic option, but without the QA attribute.
         * We know that we are responding because we would never set
         * these options ourself, we would only respond to them.
         */
        switch(*options)
        {
          case MSG_EXT_PPR_OPTION_DT_CRC_QUICK:
            *options = MSG_EXT_PPR_OPTION_DT_CRC;
            break;
          case MSG_EXT_PPR_OPTION_DT_UNITS_QUICK:
            *options = MSG_EXT_PPR_OPTION_DT_UNITS;
            break;
        }
      }
      break;
    default:
      *options = 0;
      maxsync = max_t(unsigned int, maxsync, AHC_SYNCRATE_ULTRA2);
      break;
  }
  syncrate = &aic7xxx_syncrates[maxsync];
  while ( (syncrate->rate[0] != NULL) &&
         (!(p->features & AHC_ULTRA2) || syncrate->sxfr_ultra2) )
  {
    if (*period <= syncrate->period) 
    {
      switch(*options)
      {
        case MSG_EXT_PPR_OPTION_DT_CRC:
        case MSG_EXT_PPR_OPTION_DT_UNITS:
          if(!(syncrate->sxfr_ultra2 & AHC_SYNCRATE_CRC))
          {
            done = TRUE;
            /*
             * oops, we went too low for the CRC/DualEdge signalling, so
             * clear the options byte
             */
            *options = 0;
            /*
             * We'll be sending a reply to this packet to set the options
             * properly, so unilaterally set the period as well.
             */
            *period = syncrate->period;
          }
          else
          {
            done = TRUE;
            if(syncrate == &aic7xxx_syncrates[maxsync])
            {
              *period = syncrate->period;
            }
          }
          break;
        default:
          if(!(syncrate->sxfr_ultra2 & AHC_SYNCRATE_CRC))
          {
            done = TRUE;
            if(syncrate == &aic7xxx_syncrates[maxsync])
            {
              *period = syncrate->period;
            }
          }
          break;
      }
      if(done)
      {
        break;
      }
    }
    syncrate++;
  }
  if ( (*period == 0) || (syncrate->rate[0] == NULL) ||
       ((p->features & AHC_ULTRA2) && (syncrate->sxfr_ultra2 == 0)) )
  {
    /*
     * Use async transfers for this target
     */
    *options = 0;
    *period = 255;
    syncrate = NULL;
  }
  return (syncrate);
}


/*+F*************************************************************************
 * Function:
 *   aic7xxx_find_period
 *
 * Description:
 *   Look up the valid SCSIRATE to period conversion in our table
 *-F*************************************************************************/
static unsigned int
aic7xxx_find_period(struct aic7xxx_host *p, unsigned int scsirate,
  unsigned int maxsync)
{
  struct aic7xxx_syncrate *syncrate;

  if (p->features & AHC_ULTRA2)
  {
    scsirate &= SXFR_ULTRA2;
  }
  else
  {
    scsirate &= SXFR;
  }

  syncrate = &aic7xxx_syncrates[maxsync];
  while (syncrate->rate[0] != NULL)
  {
    if (p->features & AHC_ULTRA2)
    {
      if (syncrate->sxfr_ultra2 == 0)
        break;
      else if (scsirate == syncrate->sxfr_ultra2)
        return (syncrate->period);
      else if (scsirate == (syncrate->sxfr_ultra2 & ~AHC_SYNCRATE_CRC))
        return (syncrate->period);
    }
    else if (scsirate == (syncrate->sxfr & ~ULTRA_SXFR))
    {
      return (syncrate->period);
    }
    syncrate++;
  }
  return (0); /* async */
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_validate_offset
 *
 * Description:
 *   Set a valid offset value for a particular card in use and transfer
 *   settings in use.
 *-F*************************************************************************/
static void
aic7xxx_validate_offset(struct aic7xxx_host *p,
  struct aic7xxx_syncrate *syncrate, unsigned int *offset, int wide)
{
  unsigned int maxoffset;

  /* Limit offset to what the card (and device) can do */
  if (syncrate == NULL)
  {
    maxoffset = 0;
  }
  else if (p->features & AHC_ULTRA2)
  {
    maxoffset = MAX_OFFSET_ULTRA2;
  }
  else
  {
    if (wide)
      maxoffset = MAX_OFFSET_16BIT;
    else
      maxoffset = MAX_OFFSET_8BIT;
  }
  *offset = min(*offset, maxoffset);
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_set_syncrate
 *
 * Description:
 *   Set the actual syncrate down in the card and in our host structs
 *-F*************************************************************************/
static void
aic7xxx_set_syncrate(struct aic7xxx_host *p, struct aic7xxx_syncrate *syncrate,
    int target, int channel, unsigned int period, unsigned int offset,
    unsigned char options, unsigned int type, struct aic_dev_data *aic_dev)
{
  unsigned char tindex;
  unsigned short target_mask;
  unsigned char lun, old_options;
  unsigned int old_period, old_offset;

  tindex = target | (channel << 3);
  target_mask = 0x01 << tindex;
  lun = aic_inb(p, SCB_TCL) & 0x07;

  if (syncrate == NULL)
  {
    period = 0;
    offset = 0;
  }

  old_period = aic_dev->cur.period;
  old_offset = aic_dev->cur.offset;
  old_options = aic_dev->cur.options;

  
  if (type & AHC_TRANS_CUR)
  {
    unsigned int scsirate;

    scsirate = aic_inb(p, TARG_SCSIRATE + tindex);
    if (p->features & AHC_ULTRA2)
    {
      scsirate &= ~SXFR_ULTRA2;
      if (syncrate != NULL)
      {
        switch(options)
        {
          case MSG_EXT_PPR_OPTION_DT_UNITS:
            /*
             * mask off the CRC bit in the xfer settings
             */
            scsirate |= (syncrate->sxfr_ultra2 & ~AHC_SYNCRATE_CRC);
            break;
          default:
            scsirate |= syncrate->sxfr_ultra2;
            break;
        }
      }
      if (type & AHC_TRANS_ACTIVE)
      {
        aic_outb(p, offset, SCSIOFFSET);
      }
      aic_outb(p, offset, TARG_OFFSET + tindex);
    }
    else /* Not an Ultra2 controller */
    {
      scsirate &= ~(SXFR|SOFS);
      p->ultraenb &= ~target_mask;
      if (syncrate != NULL)
      {
        if (syncrate->sxfr & ULTRA_SXFR)
        {
          p->ultraenb |= target_mask;
        }
        scsirate |= (syncrate->sxfr & SXFR);
        scsirate |= (offset & SOFS);
      }
      if (type & AHC_TRANS_ACTIVE)
      {
        unsigned char sxfrctl0;

        sxfrctl0 = aic_inb(p, SXFRCTL0);
        sxfrctl0 &= ~FAST20;
        if (p->ultraenb & target_mask)
          sxfrctl0 |= FAST20;
        aic_outb(p, sxfrctl0, SXFRCTL0);
      }
      aic_outb(p, p->ultraenb & 0xff, ULTRA_ENB);
      aic_outb(p, (p->ultraenb >> 8) & 0xff, ULTRA_ENB + 1 );
    }
    if (type & AHC_TRANS_ACTIVE)
    {
      aic_outb(p, scsirate, SCSIRATE);
    }
    aic_outb(p, scsirate, TARG_SCSIRATE + tindex);
    aic_dev->cur.period = period;
    aic_dev->cur.offset = offset;
    aic_dev->cur.options = options;
    if ( !(type & AHC_TRANS_QUITE) &&
         (aic7xxx_verbose & VERBOSE_NEGOTIATION) &&
         (aic_dev->flags & DEVICE_PRINT_DTR) )
    {
      if (offset)
      {
        int rate_mod = (scsirate & WIDEXFER) ? 1 : 0;
      
        printk(INFO_LEAD "Synchronous at %s Mbyte/sec, "
               "offset %d.\n", p->host_no, channel, target, lun,
               syncrate->rate[rate_mod], offset);
      }
      else
      {
        printk(INFO_LEAD "Using asynchronous transfers.\n",
               p->host_no, channel, target, lun);
      }
      aic_dev->flags &= ~DEVICE_PRINT_DTR;
    }
  }

  if (type & AHC_TRANS_GOAL)
  {
    aic_dev->goal.period = period;
    aic_dev->goal.offset = offset;
    aic_dev->goal.options = options;
  }

  if (type & AHC_TRANS_USER)
  {
    p->user[tindex].period = period;
    p->user[tindex].offset = offset;
    p->user[tindex].options = options;
  }
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_set_width
 *
 * Description:
 *   Set the actual width down in the card and in our host structs
 *-F*************************************************************************/
static void
aic7xxx_set_width(struct aic7xxx_host *p, int target, int channel, int lun,
    unsigned int width, unsigned int type, struct aic_dev_data *aic_dev)
{
  unsigned char tindex;
  unsigned short target_mask;
  unsigned int old_width;

  tindex = target | (channel << 3);
  target_mask = 1 << tindex;
  
  old_width = aic_dev->cur.width;

  if (type & AHC_TRANS_CUR) 
  {
    unsigned char scsirate;

    scsirate = aic_inb(p, TARG_SCSIRATE + tindex);

    scsirate &= ~WIDEXFER;
    if (width == MSG_EXT_WDTR_BUS_16_BIT)
      scsirate |= WIDEXFER;

    aic_outb(p, scsirate, TARG_SCSIRATE + tindex);

    if (type & AHC_TRANS_ACTIVE)
      aic_outb(p, scsirate, SCSIRATE);

    aic_dev->cur.width = width;

    if ( !(type & AHC_TRANS_QUITE) &&
          (aic7xxx_verbose & VERBOSE_NEGOTIATION2) && 
          (aic_dev->flags & DEVICE_PRINT_DTR) )
    {
      printk(INFO_LEAD "Using %s transfers\n", p->host_no, channel, target,
        lun, (scsirate & WIDEXFER) ? "Wide(16bit)" : "Narrow(8bit)" );
    }
  }

  if (type & AHC_TRANS_GOAL)
    aic_dev->goal.width = width;
  if (type & AHC_TRANS_USER)
    p->user[tindex].width = width;

  if (aic_dev->goal.offset)
  {
    if (p->features & AHC_ULTRA2)
    {
      aic_dev->goal.offset = MAX_OFFSET_ULTRA2;
    }
    else if (width == MSG_EXT_WDTR_BUS_16_BIT)
    {
      aic_dev->goal.offset = MAX_OFFSET_16BIT;
    }
    else
    {
      aic_dev->goal.offset = MAX_OFFSET_8BIT;
    }
  }
}
      
/*+F*************************************************************************
 * Function:
 *   scbq_init
 *
 * Description:
 *   SCB queue initialization.
 *
 *-F*************************************************************************/
static void
scbq_init(volatile scb_queue_type *queue)
{
  queue->head = NULL;
  queue->tail = NULL;
}

/*+F*************************************************************************
 * Function:
 *   scbq_insert_head
 *
 * Description:
 *   Add an SCB to the head of the list.
 *
 *-F*************************************************************************/
static inline void
scbq_insert_head(volatile scb_queue_type *queue, struct aic7xxx_scb *scb)
{
  scb->q_next = queue->head;
  queue->head = scb;
  if (queue->tail == NULL)       /* If list was empty, update tail. */
    queue->tail = queue->head;
}

/*+F*************************************************************************
 * Function:
 *   scbq_remove_head
 *
 * Description:
 *   Remove an SCB from the head of the list.
 *
 *-F*************************************************************************/
static inline struct aic7xxx_scb *
scbq_remove_head(volatile scb_queue_type *queue)
{
  struct aic7xxx_scb * scbp;

  scbp = queue->head;
  if (queue->head != NULL)
    queue->head = queue->head->q_next;
  if (queue->head == NULL)       /* If list is now empty, update tail. */
    queue->tail = NULL;
  return(scbp);
}

/*+F*************************************************************************
 * Function:
 *   scbq_remove
 *
 * Description:
 *   Removes an SCB from the list.
 *
 *-F*************************************************************************/
static inline void
scbq_remove(volatile scb_queue_type *queue, struct aic7xxx_scb *scb)
{
  if (queue->head == scb)
  {
    /* At beginning of queue, remove from head. */
    scbq_remove_head(queue);
  }
  else
  {
    struct aic7xxx_scb *curscb = queue->head;

    /*
     * Search until the next scb is the one we're looking for, or
     * we run out of queue.
     */
    while ((curscb != NULL) && (curscb->q_next != scb))
    {
      curscb = curscb->q_next;
    }
    if (curscb != NULL)
    {
      /* Found it. */
      curscb->q_next = scb->q_next;
      if (scb->q_next == NULL)
      {
        /* Update the tail when removing the tail. */
        queue->tail = curscb;
      }
    }
  }
}

/*+F*************************************************************************
 * Function:
 *   scbq_insert_tail
 *
 * Description:
 *   Add an SCB at the tail of the list.
 *
 *-F*************************************************************************/
static inline void
scbq_insert_tail(volatile scb_queue_type *queue, struct aic7xxx_scb *scb)
{
  scb->q_next = NULL;
  if (queue->tail != NULL)       /* Add the scb at the end of the list. */
    queue->tail->q_next = scb;
  queue->tail = scb;             /* Update the tail. */
  if (queue->head == NULL)       /* If list was empty, update head. */
    queue->head = queue->tail;
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_match_scb
 *
 * Description:
 *   Checks to see if an scb matches the target/channel as specified.
 *   If target is ALL_TARGETS (-1), then we're looking for any device
 *   on the specified channel; this happens when a channel is going
 *   to be reset and all devices on that channel must be aborted.
 *-F*************************************************************************/
static int
aic7xxx_match_scb(struct aic7xxx_host *p, struct aic7xxx_scb *scb,
    int target, int channel, int lun, unsigned char tag)
{
  int targ = (scb->hscb->target_channel_lun >> 4) & 0x0F;
  int chan = (scb->hscb->target_channel_lun >> 3) & 0x01;
  int slun = scb->hscb->target_channel_lun & 0x07;
  int match;

  match = ((chan == channel) || (channel == ALL_CHANNELS));
  if (match != 0)
    match = ((targ == target) || (target == ALL_TARGETS));
  if (match != 0)
    match = ((lun == slun) || (lun == ALL_LUNS));
  if (match != 0)
    match = ((tag == scb->hscb->tag) || (tag == SCB_LIST_NULL));

  return (match);
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_add_curscb_to_free_list
 *
 * Description:
 *   Adds the current scb (in SCBPTR) to the list of free SCBs.
 *-F*************************************************************************/
static void
aic7xxx_add_curscb_to_free_list(struct aic7xxx_host *p)
{
  /*
   * Invalidate the tag so that aic7xxx_find_scb doesn't think
   * it's active
   */
  aic_outb(p, SCB_LIST_NULL, SCB_TAG);
  aic_outb(p, 0, SCB_CONTROL);

  aic_outb(p, aic_inb(p, FREE_SCBH), SCB_NEXT);
  aic_outb(p, aic_inb(p, SCBPTR), FREE_SCBH);
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_rem_scb_from_disc_list
 *
 * Description:
 *   Removes the current SCB from the disconnected list and adds it
 *   to the free list.
 *-F*************************************************************************/
static unsigned char
aic7xxx_rem_scb_from_disc_list(struct aic7xxx_host *p, unsigned char scbptr,
                               unsigned char prev)
{
  unsigned char next;

  aic_outb(p, scbptr, SCBPTR);
  next = aic_inb(p, SCB_NEXT);
  aic7xxx_add_curscb_to_free_list(p);

  if (prev != SCB_LIST_NULL)
  {
    aic_outb(p, prev, SCBPTR);
    aic_outb(p, next, SCB_NEXT);
  }
  else
  {
    aic_outb(p, next, DISCONNECTED_SCBH);
  }

  return next;
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_busy_target
 *
 * Description:
 *   Set the specified target busy.
 *-F*************************************************************************/
static inline void
aic7xxx_busy_target(struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{
  p->untagged_scbs[scb->hscb->target_channel_lun] = scb->hscb->tag;
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_index_busy_target
 *
 * Description:
 *   Returns the index of the busy target, and optionally sets the
 *   target inactive.
 *-F*************************************************************************/
static inline unsigned char
aic7xxx_index_busy_target(struct aic7xxx_host *p, unsigned char tcl,
    int unbusy)
{
  unsigned char busy_scbid;

  busy_scbid = p->untagged_scbs[tcl];
  if (unbusy)
  {
    p->untagged_scbs[tcl] = SCB_LIST_NULL;
  }
  return (busy_scbid);
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_find_scb
 *
 * Description:
 *   Look through the SCB array of the card and attempt to find the
 *   hardware SCB that corresponds to the passed in SCB.  Return
 *   SCB_LIST_NULL if unsuccessful.  This routine assumes that the
 *   card is already paused.
 *-F*************************************************************************/
static unsigned char
aic7xxx_find_scb(struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{
  unsigned char saved_scbptr;
  unsigned char curindex;

  saved_scbptr = aic_inb(p, SCBPTR);
  curindex = 0;
  for (curindex = 0; curindex < p->scb_data->maxhscbs; curindex++)
  {
    aic_outb(p, curindex, SCBPTR);
    if (aic_inb(p, SCB_TAG) == scb->hscb->tag)
    {
      break;
    }
  }
  aic_outb(p, saved_scbptr, SCBPTR);
  if (curindex >= p->scb_data->maxhscbs)
  {
    curindex = SCB_LIST_NULL;
  }

  return (curindex);
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_allocate_scb
 *
 * Description:
 *   Get an SCB from the free list or by allocating a new one.
 *-F*************************************************************************/
static int
aic7xxx_allocate_scb(struct aic7xxx_host *p)
{
  struct aic7xxx_scb   *scbp = NULL;
  int scb_size = (sizeof (struct hw_scatterlist) * AIC7XXX_MAX_SG) + 12 + 6;
  int i;
  int step = PAGE_SIZE / 1024;
  unsigned long scb_count = 0;
  struct hw_scatterlist *hsgp;
  struct aic7xxx_scb *scb_ap;
  struct aic7xxx_scb_dma *scb_dma;
  unsigned char *bufs;

  if (p->scb_data->numscbs < p->scb_data->maxscbs)
  {
    /*
     * Calculate the optimal number of SCBs to allocate.
     *
     * NOTE: This formula works because the sizeof(sg_array) is always
     * 1024.  Therefore, scb_size * i would always be > PAGE_SIZE *
     * (i/step).  The (i-1) allows the left hand side of the equation
     * to grow into the right hand side to a point of near perfect
     * efficiency since scb_size * (i -1) is growing slightly faster
     * than the right hand side.  If the number of SG array elements
     * is changed, this function may not be near so efficient any more.
     *
     * Since the DMA'able buffers are now allocated in a separate
     * chunk this algorithm has been modified to match.  The '12'
     * and '6' factors in scb_size are for the DMA'able command byte
     * and sensebuffers respectively.  -DaveM
     */
    for ( i=step;; i *= 2 )
    {
      if ( (scb_size * (i-1)) >= ( (PAGE_SIZE * (i/step)) - 64 ) )
      {
        i /= 2;
        break;
      }
    }
    scb_count = min( (i-1), p->scb_data->maxscbs - p->scb_data->numscbs);
    scb_ap = kmalloc(sizeof (struct aic7xxx_scb) * scb_count
					   + sizeof(struct aic7xxx_scb_dma), GFP_ATOMIC);
    if (scb_ap == NULL)
      return(0);
    scb_dma = (struct aic7xxx_scb_dma *)&scb_ap[scb_count];
    hsgp = (struct hw_scatterlist *)
      pci_alloc_consistent(p->pdev, scb_size * scb_count,
			   &scb_dma->dma_address);
    if (hsgp == NULL)
    {
      kfree(scb_ap);
      return(0);
    }
    bufs = (unsigned char *)&hsgp[scb_count * AIC7XXX_MAX_SG];
#ifdef AIC7XXX_VERBOSE_DEBUGGING
    if (aic7xxx_verbose > 0xffff)
    {
      if (p->scb_data->numscbs == 0)
	printk(INFO_LEAD "Allocating initial %ld SCB structures.\n",
	  p->host_no, -1, -1, -1, scb_count);
      else
	printk(INFO_LEAD "Allocating %ld additional SCB structures.\n",
	  p->host_no, -1, -1, -1, scb_count);
    }
#endif
    memset(scb_ap, 0, sizeof (struct aic7xxx_scb) * scb_count);
    scb_dma->dma_offset = (unsigned long)scb_dma->dma_address
			  - (unsigned long)hsgp;
    scb_dma->dma_len = scb_size * scb_count;
    for (i=0; i < scb_count; i++)
    {
      scbp = &scb_ap[i];
      scbp->hscb = &p->scb_data->hscbs[p->scb_data->numscbs];
      scbp->sg_list = &hsgp[i * AIC7XXX_MAX_SG];
      scbp->sense_cmd = bufs;
      scbp->cmnd = bufs + 6;
      bufs += 12 + 6;
      scbp->scb_dma = scb_dma;
      memset(scbp->hscb, 0, sizeof(struct aic7xxx_hwscb));
      scbp->hscb->tag = p->scb_data->numscbs;
      /*
       * Place in the scb array; never is removed
       */
      p->scb_data->scb_array[p->scb_data->numscbs++] = scbp;
      scbq_insert_tail(&p->scb_data->free_scbs, scbp);
    }
    scbp->kmalloc_ptr = scb_ap;
  }
  return(scb_count);
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_queue_cmd_complete
 *
 * Description:
 *   Due to race conditions present in the SCSI subsystem, it is easier
 *   to queue completed commands, then call scsi_done() on them when
 *   we're finished.  This function queues the completed commands.
 *-F*************************************************************************/
static void
aic7xxx_queue_cmd_complete(struct aic7xxx_host *p, struct scsi_cmnd *cmd)
{
  aic7xxx_position(cmd) = SCB_LIST_NULL;
  cmd->host_scribble = (char *)p->completeq.head;
  p->completeq.head = cmd;
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_done_cmds_complete
 *
 * Description:
 *   Process the completed command queue.
 *-F*************************************************************************/
static void aic7xxx_done_cmds_complete(struct aic7xxx_host *p)
{
	struct scsi_cmnd *cmd;

	while (p->completeq.head != NULL) {
		cmd = p->completeq.head;
		p->completeq.head = (struct scsi_cmnd *) cmd->host_scribble;
		cmd->host_scribble = NULL;
		cmd->scsi_done(cmd);
	}
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_free_scb
 *
 * Description:
 *   Free the scb and insert into the free scb list.
 *-F*************************************************************************/
static void
aic7xxx_free_scb(struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{

  scb->flags = SCB_FREE;
  scb->cmd = NULL;
  scb->sg_count = 0;
  scb->sg_length = 0;
  scb->tag_action = 0;
  scb->hscb->control = 0;
  scb->hscb->target_status = 0;
  scb->hscb->target_channel_lun = SCB_LIST_NULL;

  scbq_insert_head(&p->scb_data->free_scbs, scb);
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_done
 *
 * Description:
 *   Calls the higher level scsi done function and frees the scb.
 *-F*************************************************************************/
static void
aic7xxx_done(struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{
	struct scsi_cmnd *cmd = scb->cmd;
	struct aic_dev_data *aic_dev = cmd->device->hostdata;
	int tindex = TARGET_INDEX(cmd);
	struct aic7xxx_scb *scbp;
	unsigned char queue_depth;

        scsi_dma_unmap(cmd);

  if (scb->flags & SCB_SENSE)
  {
    pci_unmap_single(p->pdev,
                     le32_to_cpu(scb->sg_list[0].address),
                     SCSI_SENSE_BUFFERSIZE,
                     PCI_DMA_FROMDEVICE);
  }
  if (scb->flags & SCB_RECOVERY_SCB)
  {
    p->flags &= ~AHC_ABORT_PENDING;
  }
  if (scb->flags & (SCB_RESET|SCB_ABORT))
  {
    cmd->result |= (DID_RESET << 16);
  }

  if ((scb->flags & SCB_MSGOUT_BITS) != 0)
  {
    unsigned short mask;
    int message_error = FALSE;

    mask = 0x01 << tindex;
 
    /*
     * Check to see if we get an invalid message or a message error
     * after failing to negotiate a wide or sync transfer message.
     */
    if ((scb->flags & SCB_SENSE) && 
          ((scb->cmd->sense_buffer[12] == 0x43) ||  /* INVALID_MESSAGE */
          (scb->cmd->sense_buffer[12] == 0x49))) /* MESSAGE_ERROR  */
    {
      message_error = TRUE;
    }

    if (scb->flags & SCB_MSGOUT_WDTR)
    {
      if (message_error)
      {
        if ( (aic7xxx_verbose & VERBOSE_NEGOTIATION2) &&
             (aic_dev->flags & DEVICE_PRINT_DTR) )
        {
          printk(INFO_LEAD "Device failed to complete Wide Negotiation "
            "processing and\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_LEAD "returned a sense error code for invalid message, "
            "disabling future\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_LEAD "Wide negotiation to this device.\n", p->host_no,
            CTL_OF_SCB(scb));
        }
        aic_dev->needwdtr = aic_dev->needwdtr_copy = 0;
      }
    }
    if (scb->flags & SCB_MSGOUT_SDTR)
    {
      if (message_error)
      {
        if ( (aic7xxx_verbose & VERBOSE_NEGOTIATION2) &&
             (aic_dev->flags & DEVICE_PRINT_DTR) )
        {
          printk(INFO_LEAD "Device failed to complete Sync Negotiation "
            "processing and\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_LEAD "returned a sense error code for invalid message, "
            "disabling future\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_LEAD "Sync negotiation to this device.\n", p->host_no,
            CTL_OF_SCB(scb));
          aic_dev->flags &= ~DEVICE_PRINT_DTR;
        }
        aic_dev->needsdtr = aic_dev->needsdtr_copy = 0;
      }
    }
    if (scb->flags & SCB_MSGOUT_PPR)
    {
      if(message_error)
      {
        if ( (aic7xxx_verbose & VERBOSE_NEGOTIATION2) &&
             (aic_dev->flags & DEVICE_PRINT_DTR) )
        {
          printk(INFO_LEAD "Device failed to complete Parallel Protocol "
            "Request processing and\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_LEAD "returned a sense error code for invalid message, "
            "disabling future\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_LEAD "Parallel Protocol Request negotiation to this "
            "device.\n", p->host_no, CTL_OF_SCB(scb));
        }
        /*
         * Disable PPR negotiation and revert back to WDTR and SDTR setup
         */
        aic_dev->needppr = aic_dev->needppr_copy = 0;
        aic_dev->needsdtr = aic_dev->needsdtr_copy = 1;
        aic_dev->needwdtr = aic_dev->needwdtr_copy = 1;
      }
    }
  }

  queue_depth = aic_dev->temp_q_depth;
  if (queue_depth >= aic_dev->active_cmds)
  {
    scbp = scbq_remove_head(&aic_dev->delayed_scbs);
    if (scbp)
    {
      if (queue_depth == 1)
      {
        /*
         * Give extra preference to untagged devices, such as CD-R devices
         * This makes it more likely that a drive *won't* stuff up while
         * waiting on data at a critical time, such as CD-R writing and
         * audio CD ripping operations.  Should also benefit tape drives.
         */
        scbq_insert_head(&p->waiting_scbs, scbp);
      }
      else
      {
        scbq_insert_tail(&p->waiting_scbs, scbp);
      }
#ifdef AIC7XXX_VERBOSE_DEBUGGING
      if (aic7xxx_verbose > 0xffff)
        printk(INFO_LEAD "Moving SCB from delayed to waiting queue.\n",
               p->host_no, CTL_OF_SCB(scbp));
#endif
      if (queue_depth > aic_dev->active_cmds)
      {
        scbp = scbq_remove_head(&aic_dev->delayed_scbs);
        if (scbp)
          scbq_insert_tail(&p->waiting_scbs, scbp);
      }
    }
  }
  if (!(scb->tag_action))
  {
    aic7xxx_index_busy_target(p, scb->hscb->target_channel_lun,
                              /* unbusy */ TRUE);
    if (cmd->device->simple_tags)
    {
      aic_dev->temp_q_depth = aic_dev->max_q_depth;
    }
  }
  if(scb->flags & SCB_DTR_SCB)
  {
    aic_dev->dtr_pending = 0;
  }
  aic_dev->active_cmds--;
  p->activescbs--;

  if ((scb->sg_length >= 512) && (((cmd->result >> 16) & 0xf) == DID_OK))
  {
    long *ptr;
    int x, i;


    if (rq_data_dir(cmd->request) == WRITE)
    {
      aic_dev->w_total++;
      ptr = aic_dev->w_bins;
    }
    else
    {
      aic_dev->r_total++;
      ptr = aic_dev->r_bins;
    }
    if(cmd->device->simple_tags && cmd->request->cmd_flags & REQ_HARDBARRIER)
    {
      aic_dev->barrier_total++;
      if(scb->tag_action == MSG_ORDERED_Q_TAG)
        aic_dev->ordered_total++;
    }
    x = scb->sg_length;
    x >>= 10;
    for(i=0; i<6; i++)
    {
      x >>= 2;
      if(!x) {
        ptr[i]++;
	break;
      }
    }
    if(i == 6 && x)
      ptr[5]++;
  }
  aic7xxx_free_scb(p, scb);
  aic7xxx_queue_cmd_complete(p, cmd);

}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_run_done_queue
 *
 * Description:
 *   Calls the aic7xxx_done() for the scsi_cmnd of each scb in the
 *   aborted list, and adds each scb to the free list.  If complete
 *   is TRUE, we also process the commands complete list.
 *-F*************************************************************************/
static void
aic7xxx_run_done_queue(struct aic7xxx_host *p, /*complete*/ int complete)
{
  struct aic7xxx_scb *scb;
  int i, found = 0;

  for (i = 0; i < p->scb_data->numscbs; i++)
  {
    scb = p->scb_data->scb_array[i];
    if (scb->flags & SCB_QUEUED_FOR_DONE)
    {
      if (scb->flags & SCB_QUEUE_FULL)
      {
	scb->cmd->result = QUEUE_FULL << 1;
      }
      else
      {
        if (aic7xxx_verbose & (VERBOSE_ABORT_PROCESS | VERBOSE_RESET_PROCESS))
          printk(INFO_LEAD "Aborting scb %d\n",
               p->host_no, CTL_OF_SCB(scb), scb->hscb->tag);
        /*
         * Clear any residual information since the normal aic7xxx_done() path
         * doesn't touch the residuals.
         */
        scb->hscb->residual_SG_segment_count = 0;
        scb->hscb->residual_data_count[0] = 0;
        scb->hscb->residual_data_count[1] = 0;
        scb->hscb->residual_data_count[2] = 0;
      }
      found++;
      aic7xxx_done(p, scb);
    }
  }
  if (aic7xxx_verbose & (VERBOSE_ABORT_RETURN | VERBOSE_RESET_RETURN))
  {
    printk(INFO_LEAD "%d commands found and queued for "
        "completion.\n", p->host_no, -1, -1, -1, found);
  }
  if (complete)
  {
    aic7xxx_done_cmds_complete(p);
  }
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_abort_waiting_scb
 *
 * Description:
 *   Manipulate the waiting for selection list and return the
 *   scb that follows the one that we remove.
 *-F*************************************************************************/
static unsigned char
aic7xxx_abort_waiting_scb(struct aic7xxx_host *p, struct aic7xxx_scb *scb,
    unsigned char scbpos, unsigned char prev)
{
  unsigned char curscb, next;

  /*
   * Select the SCB we want to abort and pull the next pointer out of it.
   */
  curscb = aic_inb(p, SCBPTR);
  aic_outb(p, scbpos, SCBPTR);
  next = aic_inb(p, SCB_NEXT);

  aic7xxx_add_curscb_to_free_list(p);

  /*
   * Update the waiting list
   */
  if (prev == SCB_LIST_NULL)
  {
    /*
     * First in the list
     */
    aic_outb(p, next, WAITING_SCBH);
  }
  else
  {
    /*
     * Select the scb that pointed to us and update its next pointer.
     */
    aic_outb(p, prev, SCBPTR);
    aic_outb(p, next, SCB_NEXT);
  }
  /*
   * Point us back at the original scb position and inform the SCSI
   * system that the command has been aborted.
   */
  aic_outb(p, curscb, SCBPTR);
  return (next);
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_search_qinfifo
 *
 * Description:
 *   Search the queue-in FIFO for matching SCBs and conditionally
 *   requeue.  Returns the number of matching SCBs.
 *-F*************************************************************************/
static int
aic7xxx_search_qinfifo(struct aic7xxx_host *p, int target, int channel,
    int lun, unsigned char tag, int flags, int requeue,
    volatile scb_queue_type *queue)
{
  int      found;
  unsigned char qinpos, qintail;
  struct aic7xxx_scb *scbp;

  found = 0;
  qinpos = aic_inb(p, QINPOS);
  qintail = p->qinfifonext;

  p->qinfifonext = qinpos;

  while (qinpos != qintail)
  {
    scbp = p->scb_data->scb_array[p->qinfifo[qinpos++]];
    if (aic7xxx_match_scb(p, scbp, target, channel, lun, tag))
    {
       /*
        * We found an scb that needs to be removed.
        */
       if (requeue && (queue != NULL))
       {
         if (scbp->flags & SCB_WAITINGQ)
         {
           scbq_remove(queue, scbp);
           scbq_remove(&p->waiting_scbs, scbp);
           scbq_remove(&AIC_DEV(scbp->cmd)->delayed_scbs, scbp);
           AIC_DEV(scbp->cmd)->active_cmds++;
           p->activescbs++;
         }
         scbq_insert_tail(queue, scbp);
         AIC_DEV(scbp->cmd)->active_cmds--;
         p->activescbs--;
         scbp->flags |= SCB_WAITINGQ;
         if ( !(scbp->tag_action & TAG_ENB) )
         {
           aic7xxx_index_busy_target(p, scbp->hscb->target_channel_lun,
             TRUE);
         }
       }
       else if (requeue)
       {
         p->qinfifo[p->qinfifonext++] = scbp->hscb->tag;
       }
       else
       {
        /*
         * Preserve any SCB_RECOVERY_SCB flags on this scb then set the
         * flags we were called with, presumeably so aic7xxx_run_done_queue
         * can find this scb
         */
         scbp->flags = flags | (scbp->flags & SCB_RECOVERY_SCB);
         if (aic7xxx_index_busy_target(p, scbp->hscb->target_channel_lun,
                                       FALSE) == scbp->hscb->tag)
         {
           aic7xxx_index_busy_target(p, scbp->hscb->target_channel_lun,
         /*+MTRUE);
/*+M*****}*****************found++************else*****{*******p->qinfifo[**
 * Adapnext++] = scbp->hscb->tag**************/****** Now that we've done the work, clear out any left over commands in  Th (c) 1 * Adap and updat   TheKERNEL_QINPOS down oputer card. (c)  (c) 1 NOTE: This routine expect  Thesequencer to already be paused whente it a publiit difyun....make sure it's John Aay! (c) /
 nce.pos =***
 * Adap dev****while( versio!=nce.tail)
**********
 * Adapt versiice drSCB_LIST_NULL********if (p->features & AHC_QUEUE_REGSon.
  aic_outb(p, 2, or (at your, HNutedQOFF************** WARRANTY; without even the is free softw);

  return (*****);
}

/*+F*neral Public License for more details.
 *
 * You should have received a 
 * Function:opy  MERC7xxx_scb_on_qoutAdap
bute* Descriphe GNU GeneIe Foerive Foundats paslic to us currentlyyou can nse
 * a?
 *-eneral Public License for more details.
 *
 * You should have received a c/
static int
ral Public License
 * a(structeral Publhost *p,  Ultrastor 24F
file*scb)
****int i=0 PURPion)
 **
 se
 * a[he Adaptec E dev + i) & 0xff ]lateuted in the hon.
 *
 * ifhe Adaptec EISA
 * config file (!adp7771.cfg=drive for Linux.UT ANYRPOSE.  S*******************  i**********OSE.  SFALSE * GNNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Publreset_devicelong with this program; sThe ,
 * t a the tgiven target/channel has been Manua.  AbortU Generll active * Thqueuedrives for John ification, the.a Bois ff the GU Geneneed note Unry abof Clink----filepointer ANScublit itING.a MSG_ABORT_TAGU Genethen we had a taggedment of  (no------
 *
 *),t itiPYING.. Eischen orU Gene. EiBUS_DEV_RESETn@iworthe , the Awon't know-------algaent of Coalgamorthe enern---- busgged queueiwill exist,ng, bodified toa fixSCSI-2,n@iwornothingsharing ofs SCBs, tagg-----
 *
 * ed queueing, IRQ .  Inn (drcases,rks.dDMAi-----------to-------------the -----
 *
 * or fixesscbn T. justs copyrigrsityischen@iwm. Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Sources include the Adaptec void0 driver Manual,
 * te Ultrastor 24F
 * driver kerificaty formon, the,**********ation, are p kerlun, unsign.orgharWork Linux (ultrastor.c), variousp, *prev, vap**** Ultrasscsil,
 * t *sd****rovided that taft 10, va, tcl,----_ux.
 *
 kern = 0y foit_listion river (owing conditl,
 _data *wing di PURPht (c) 1Restor   Tis Lice Aycrck
 * n; eitherabove copy =MERCHinTY; wSCBPTR******    nofile.
 * 2. Redis (deR PURP it ral Publverbose & (VERBOS WITSET_PROCESS |   noticeschen  list o)on.
 *
 * Trintk(INFO_LEAD "Ranuadapters, hardwar the f%d,\n" * modificar foost_no,without
 ms, with ted pabove copy*******laimer in the
 *  Ce to
  file%d, SEQADDR 0x%x, LASTPHASE "* modificati" proaterials provided with the distribution.
 * 3. T*    no * modificae.
 * 2. Redpromot0) | abovSoftware is com1) << 8) * Where this Softwarects
 *   ) the author may not be usSG_CACHEPTte produ) anOUNTse or CSISIGIe proaterials provided with the distribution.
 * 3. ********** will be useful,
 *ULTRA2) ?this Software  and the t) : 0 * Where this Software the GPL),le.
 * 2. Redirequireral Public License ("GPL")STAT0erms of ondi1ions of this2e the 
 * combined work to also be released under thhis Software onditder the terms of 
 * th this f the
 * GPL withat
eral P} without modDeal with
 *
 fixesificatng, b-----
 *
 * issuesstributherondi_for_each_entry withdev, &p->wing dis,MITEDHA-2740A Ser above copyright
 *    notice, this list of conditions and the followine author may not be usprocessingowing di %paterided with the distribution.
 
		HE A, the GPLdev*******sdfile.
 dev->SDptr PURP Seri(LIED WA!= ALL_TARGETS &&PLIED WA!= sd->id) ||********(on, the
 NOT LICHANNELTO, PS
 * OR SER OF on, theLL THE AUcontinue****** FITNESS FOR A PARTICULAR PURPOs and the folf condition, this list oLL THE AUauthor may not be useleanIABLup daptuCompformahe G derived from"g, bdelayed, vas.NY DIRECT, INDIRER PROFITS; SE OF idSE OF lun*******NTIAL
 * flags &= ~ide andICWHETHER IENDINGRUPTION)
 Workr's uted in the h L THE ********NTIAL
 * dtr_pendIABL= 0********NTIAL
 * ----pprQUENTIAL
 * -------_copy---------------------sdt----------------- *
 -------------------------
w*
 *  Thanks also ghe f----------------------SSIBIL= * SUCH PRINT_DTR-----------------temp_q_depthQUENTIAL
 * maxC Alpha ************tcl = ( OF SUed u4ined R PROFITS; ed u3inedEN IF A      $Id: above copindex_fixe_ificatY; wght
 river)r's he fSTITUTE GOO  (aic7xxx.c,v 1.119 19)97/06/2neral Publ*  A Boot time option was /* unfixes*/************ * 1. Red = he ho R CONS7xxx=u18 gibbs E IN ANY WAY
heade muurce, t (     !=ultra7/06/27 19:39:   aic7xxx=u Redistr           iver fq_your optPTION)
 * HOWEVEmatch, vaY; wi* 1. Redbution.
 *THIS SOFTted phe fL THE AU********     q_remove(&aic7xxx=irq_trigger:[
 *
 *  $Id*********** it w* 1. RedPOSSIBILIx.c,vWAITINGQACT, STRIC2 deang Ex-----------above ccmds******** providedaft 10 WAY********************* *
 *  $IdPOSSIBILITY (t reACTIVE |***/

/*+M*******************************|xx.c,vtwin bord <d but D_FOR_DONver forurther d Copyre the above copyright
 *    notice THEORY OF LIABILITY, WHETHER IN CONTRACT, ST
 * LIABILITY, OR TORT (INQINFIFO
 * OUT OF THE USE distribution.
 * 3. redharal Publiearch_ * Adaption ic7xxx.c,v 4.1 1997/06/1he terms 1997-1999 Doug Ledford
 *
 * Tic7xxrrms ueesetriver,*     ;a154shariSoveri  The aitingY WAY...
 *-----rg, 1tin n, imcmited ing, oug Ledforsharischen/twin bent of C----thered under the same licensing terms as the FreeBSD
 * driver written by Justin Gibbs.  Please see his Conclude
 * bu notice above
 * for the eiworks.act terms and con******ing conditions
 * are met:
 * 1. Redis        aic7xxx=ultra
 *           p->ensive change,1]  # 0 edge, 1 level
 *           aic7xxx=verbose
 *
 *  Daniel M. Eischen, deischen@iworks.InterWorks.org, 1/23/97
 *
 *  $Id: aic7xxx.c,v 4.1 1997/06/12 08:23:42 deang Exp $
 *-M***** by various peo***********************************************/

/*+M*************************AIC and************cmd)******************************************
 *
 * Further driver modifications made by Doug Ledford <dledford@redhat.com>
 *
 * Copyright (c) 1997-1999 Doug Ledford
 *
 * These changes are relewithout modn Eischnclude
-----selethe GMITEDUT NOT LIMthis driver
 *  2: Modification of kernel code to accommodate different sequencer semantics
 *  3: Extensive diff.  Second, derived "in apNY DIRECT, INDIRECT, INCIDENTAL, 1997rove
 *    rovided that tn the ****sion.
*  A nndling*
 * ile.
 * 2. Re
/*+M**_SCBH);itho StartANSI,1]  of in ape for m>
 *
ributed in the hope  edge, 1 *
 * ), the Adaptec AHA-2/27 19:39:18 gRANTY; wn the distributiog Exp $ the ainary form must reproducworks.Intehe is deve>ed byhe iclai->num WAY 08:23:42 deang Eht (c)w lev 199nditions yright
 check here the.w incormso seion, imsincftwa low level means eith thehe ker    driartm----- terms of thscrewopyrartss up low level as poTRICT
 * LIWARNhe
 *  W a
 * dList inconsistency;de l *  A =e orOTHERWISE) ARlevel S=%dprocess, that also can be error prone uses the a * modificathe linux mid-level SC********** that the linux ant reNEXTis way of dral Publadd_cur*    o_freecondi(****************** Linux,
 * 2 deang Exp $uted bylinux mid-he iarray[he is dev]**************rWorks.org, 1/23/97
  $Id: aic7xxx.c,v 4.1 1997/06/12 08:23:4************** that the intaincifi_nclude
 * bneeded funial driver************ications*************/

/*+M**************************** Overall, driver represents a significant depa*****************
 *
 * Fururther drive      ations made by Doug Ledford <dledford@redhat.com>.  Linux
 * do(c) 1997-1999 Doug Ledford
 *
 * These chase
 * prssible * of this driver wils of any middle ware * low leve level -----iver into the firsendorsou can e a
 * dondi,withwnux,
 * he kernelha10c)U Generya---
 ld effect g, bues MAinry Dealgabehinistribg, I see issE into wayn T.  copyrigE.  Soffh only Second, n and/or ifng in the kernelhed bMAinuch as toffliability of codde files a ANY WARRANTY; whe
 * GPL with tEQadp7~ENSELOle ware tyxample of this WARRANTY; wCLRSELTIMEOncer
 INT1ch is to impother drivether drive Linux,
 * **************ssible.chen@iworks.of doing
 * things would be nice, easy to********nges are releasht (c) 1Go through disconnecteANTIstumbli*-M*** the * IMiesrks.Inve...
 *
 (c) 1----complehe G, zero * dtheis fantrol byrogroopproach to solving the same problem.  The
 * problem is importing the FreeBSD aic7xxx driver code to linux can b of the drive* ---ime consuming process, that also can be error prone.  Da it will*******,
 *PAGESCBOUT An
 * Eischen's official driver uses the approach that the linux anDISCONNECTEDSD
 * das possible.  To that end, his next version
 * of this driver will be using a mid-layer code library that he is developing
 * to moderate communications betweenthe linux mid-level SCSI code and the
and the like and geDof the drive working, making for fast easy
 * imports of the FreeBSD code into linux.
 *
 * I disagree with Dan's approach.  Not that I don't think his way of doing
 * thdistribmlic Lfrom_ of re unifn linux that will caur
 * between FreeBSD and Linux.  I have no objection to those issues.  My
 * disagreement is on the needed functionality.  There simply are certain
 * things that are done died deeper in the code, but those really shouse
 * problems for this driver regardless of any middle ware Dan implements.
 * The biggest example of this at the moment is interrupt semantics.  Linux
 * doesn't provide the same protection techniques as FreeBSD does, nor can
 * they be easily implemented iiver for Linhanged/m-----------to only make changes in the kernel
 * portion of the driver as they are needed for the new sequencer semantics.
 * In this way, Walks majore
 * st mak * d Softnoe rest ofou can  this onlye
 * (c) 1a validst repros onuewitht reCONTROLmodifpproach to solble in its operation, then I'll retract my above
 *pproach that the linux anFREE Missouri (inext version
 * of this driver will be using a mid-layer code library that  FITNESform must reprod <*********************************************************Fthis onlying, making f!es throughout kernel portion of dof driver to im************of conditionDriver forngs that ar  To that end, his ouldn't need touched
 * under n WARRANTY; wuted in the ho moderate communishe goes.
 *
 0
 *   ense, Iis way of doing
 * things would be nice, easy ntics.
 * In this way, the portion onloose to
  fasto th looontro is fai of CoJohn (c) 1wereraft 10cbut-----onere als approach to sol   I needeon.
 *
 * e goes.
 *
 *   -- July 16, that would alsoe goes.
 *
 *   -- July 16,d FreeBSD
 * dT ANY WARRANTY; w*   -- July 16,ow, I'm from Missouri ****----(is lave no objectmaxor Ls - 1; ien t0; i--on.
 *
 * rovided that tscbidnndling WARRANTY; wiode library tha7XXX_VERBOSE_DEBUGill be using a mid-lay*   -- July 16, 23:04
 *     I   Disabling this
 *   define wie nice, easy rned it back on to try and compensa maintain, and create a more uniform dri***********************oes heloping
 * to moderate communicationst/re***************************************x.  I have no objection to those id.  My
 * disagreement is on the needed functionality.  There simply are certain
 * things rned it back on to try and compensateshe goes.
 *
 *   -- July 16, 23:04
 *     I tuto maintain, and create a more uniform driveencer semantics.
 * In this way, the portion onlentiw tries to se of SRANTook  things itselfuppornel is n, imLIED WAJohn to
 stillaft 10-----eseoportointo (m* drlikelyernel.orks.o)ings itself.
 define of the drivemedia onlManua octe toistribu Aagged queueiw effblinfineweng of n, im-----,ified tMAin_PCI_SE aic7ion ot applinux/smputernce.
 *
ude <linux/spt.h>
# of the drivee
 *clude
ernel.ondit, so appreally m----e
 * NSI Sa pas.orof CSCBer by
John Justde <linwe shouldMAin copyrigby.h>
#EXPREis prIABLEaggedudific,------markux/pci.an rorrer t in itrrnogo onUT NOT LIM if
 *   can on is
 *   complete and; theon.
 *
 * x.  I have no objection to thoi.  My
  FITplement**********/
ug Led) &&, easy to maintais on the needed functionality.  There simply fine ALL_CHA! driver (aha1740.c), theeeded f2 08:23********* problems for this driver regardless  Modifications to the default probe/atta***************p $
 *-M***** Dan implements.
 * ***************************** Dan implements.
 * The biggest example of ***************
 *
 * F********s as FreeBSD does, nor can
 * they be easily implementoblems for thde by Doug Ledford <dledford@redhat.re releast
 * involve bove copyridistributaha1542.c), the Adaptec EISA overlay file
 * (adp7770.ovl), the Adaptec AHA-2740 Series Technical Reference rsity_intdaptlong with this program; s TORre the odifiruptUDING N--------------------  
 * Copyright (c) 1994-1997 Justin Gibbs.
 * All rights reserved.
 *
 * Redistr;

/*
 * Makee Ultrastor 24F
 * driv Linux/* the dx_oldnot to thecondihe Gelf.is may#incluons bdcal as  the sequencer
 * DO |s
 * to I, the driING from Free0d condite sequencer
 * code , the ATN0, the dCSIRSTer willBUS thaf commandsPERR ITUTE GOOCLR *   CHG, the REQINITfrom FreeBSD whm for determininCSIIN99 Der
 *Qevice.  WBRKAD   Jce.  WPARERRncer
INice, GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General PublManualte to
 Bootlong with this program; s  docu<scsid to enar dOR I--------------------  
 * Copyright (c) 1994-1997 Justin Gibbs.
 * All rights reserved.
 *
 * Redistributio if
 * you    0, 0, 0, 0, 0, 0, 0, 0}


/*
 Disablblkdev.hnot to thsetting tag_commands another.
 *IMODE1ype codends to-11, 13-15  witho Tware.  Therebus'ne deviceoperCE ORs, aftern (dn T. x_old/scsiues sunnux/pci on thngs itselry De----ns bya l use
#ince.   oop tAycocktoIf noter Scien theaway EXPRESSdifycode.
soc7xxmight a#incll shutare; y only a few mernel.untiland telld by
es shoed  Itopph line o savd can bne devicesetc7xxs option speaicatd can bm (whichy coesome
sensurth me)cal ang for ID 12agged  the codeommands/LUN for IDs
 * (1, 2-11ware typord ds to My approach isdge, 1 ot another.
 *
are type tual stradded0 as comrq_tr(5ueing foal
 * tomes idd 4,new Ultra2 chipsetsi_messa longerNG IN e its owxx_taernel.kdev.hthan------ver noiommanup creattin es 0 ae
 * toFAULT_Tlinusables efatiowecludine GNqueueing  ord the G Freed 4, _tag happy would default to GPL, the terms
 * and after this 25gorith*********r this fgoring for ID 12, and telle below structure is for  on tpproach isr this 1T_TAG_C                      ******/odif-enged queueing with 4 commands/LUN for IDs
 * (1, 2-11, 13-15)| isables tagged queuein in the array are used for the queue_depth.
 * Note that the maximum value for an entry is 254, but you're insane n, thetry to use that many commands on one-------n this example, the first line will disable tagged queueing for all
 * the devices on the first probed aic7n, theuse in source and binary formTHIS SOFT kernnitiate insan Linuxrovided t
  {rtinnualmin,t char *baxe must retain the sblkctltice, thcury aha_tyuce the above copyright
 * LITY, WHETHER IN CONTR disclaimer in the
 *    docuon, the
called, %Compging tSCSI-2 aterials prded with the distribu-1      X_VERng the typ==***** ? " pag" : " DMAi"nd cleased uon, the
== 1on.
 *
 *  char *boa = 8f (aic_names[] = = 16of those*******TY AND FITwill be useful,
 *TWIN TRUE 1
#endif*
 *n, the
Acode that  /* AIC_7771 ---------daptec AHA-28*/
  "A***********************dapter",               it will be useful,
 *WIDE 0
#endif

#if defdaptec AHA-284X SCSdifference, so back
 *     off sh   /* AIC_7850 */
  ich no longer reldge, 1  /* AIC_777<  "Adaptec on.
 *
 *  * low lodifver sameasync/narrlockransfficae othewAdapnego /* Aliabiliode thaS},
  {DEFAULT_MITE_tual ATE + host adaptage and suc*/

static adapter_tag_info_t l be using a mid-layhost adaOFFin b              /* AI****** char *boahe Adaptwithout modifis on on tagg ThinPubli/ 16 com <scsi/nged/lg for tthernown", file.
 * 2. RedBLKCTd compg of Siverhipts operCHIPID_MASKadded,
 *AIC777097/06/2           ded a adapte&e.
 BUSB) >> 3_info[] =
{
 ,           /*-----g of S,           !=        
#de              /* AIC_284x97/06 "Adaptec AIC-78Case 1: C):
 *
 ----a  Pae-----per CMDS_P  "Adaptec AH          /* AIC_NONE */
  "Adaptec AIC-7810 Hardublic License ("GPL")tealthily{4, 0,c7xxxidl
  {DEFAULNY DIRECT, INDIR*********ter",          if (aictec AIC-78host adapter",  127,  "Adaptec EXPRof Cuag_ic7xxxd 4, 127 com * In "Adaptec AHA-294X SCSI AIC_7880^*/
  "Ad         /* AIds/LUN for IDs
 * (1, 2-11, 13-15), disee abovagged queueise
 * pranging the typess code size.  D probed aic7xxx adapterdding the
 *  t
 * involve any middle ware type (ode. I|ENl use|ENAUTOATNP)My approach is tAG_COMMANDS},
  {DEFAULT_TAG/* AIC_7895 */
  "Ada         /* AII host adapter", ter",          2: Arg):
 *
 er iing fotec AHA-398X me
 * ly a    ,           /*st adapter",           /* AIC_7883 */
  "Adaptec AHA-2944 Ultra SCSI      c7xxxte to
 * taft 10c* AIC_7884 */
  "Adaptec AHA-2940UW Pro Ultra SCSI host/LUN for IDs
 * (1, 2-11, 13-15), d( */
  "Ad7890le tagder the ttec AHA-293X Unux
 * doesn',
 *HANDse i_ 160/m S",     /*msg_type =for wTYPE_N* These ce a spele           ltra2 SCSI host adapter",          /* AIC_7890 */
  "Adaptec AHA-294X Ultra2 SCSI host adapter",          /* AIC_7890 */
  "Adaptec AIC-7896/7 Ultra2 SCSI host adapter",        that it       /* AIC_NONE */
  "Adaptec RETUR4x */
 thor may not be usen, the
r devNY DIRECT, INDIRECT, INCIDtra SCSI hoe.
 *
ad TORTNCLU (dr onlyt "AdGLIGENCE OR ------e $
 *----     CMDSonUN for * NOT "AdapUT NOT LIM Redistribution and up,OT LIMITED T for the exT LILUNSg this
 *   defduce the a !aptec AHA-394X Ultra SCSI host adap 16 com_erms of t There istec 1542 es in the array are used for the queue_depth.
 * Note that the maximum value for an entry is 254, but you're iunently in  aic7 try to use that many commScaagged anclude
 * but are nre; loa*----/* A6 comIABLEour *
 *arine ext It
  ca <li-----------------  
 * Copyright (c) 1994-1997 Justin Gibbs.
 * All rights reserved.
 *
 * Redistri by
 *  Doug Ledf   0, 0, 0, 0, 0, 0, 0, 0}

/ (ultrastor.c), variousd the following disclaimer,
 *    ce, thsentry
s reliable various people o              tically e BAo enadapt a DID_BUS_Ffect han   /SCBe Foundto
 e a
 * dX_STinclude "aisvided ta sloapproach todge, 1 plemschen,
 *-M***_,1]  default probe/at))l
 *        GING
 *   R
 *s GuidecmdIO
# * ted witclai      $Id: !r Linux._it see97/06/27 19:39:18 gibbs l DEC Alpha sup1 "Adaptec AH happ********************en tevision              apter",           *insert_ersi************************* of the au***********************     POSSIBILITY **/

/*+M****     I turned*******************************************
 *
 * Fur happen          0x83     Form: ***************** Publoot time option of the au Further driver*
 * Adaptec AIC7xxx device drive for Linux.
 *
 *     ent************that it     BILITY AND FITwill be useful,
 * but WITHOUT ANYNY WARRANTY; without even the implied warrant********************ublir that
 *  will a * MERCHANTABILITY or FITNESS FOR A PARTICULAR P      IC_7873r that
 *  ws also a/*
 * AIC-7770 I*******************  Do**********aptec AHAF00ul

#define     sta**********ere is}

#ifdef CONFIG_PCI

#def it  DPE 0x80        0xSS010040ul        RMA 0x289X only */
Tdefi10ul        /     08        0x00te p01a1542.c), the Adaptec EISA overlay file
 * (adp7770.ovl), the Adaptec AHA-2740 Series Technical Reference pci
 * ----define that will tell tintenthe fisi redispectPCI error  /* Arsity ver not to th      R and/or If you Gibbs, 4}},
------------IID  a 2940ne    en/aic7xxxne SCS                Vu see fit .
 *
 mach it  pagin This loc--
 ae the       SCBRAnot to the7: ID1r g_infslammde <XPREnon-stopSEL_ULTRA2 g with 4 c40

/*
 * EISA/VL-bus stuff
 */
#define MINSLOT                1
#define MAXSLOT                15
#           0, 0, 0, 0, 0, 0, 0, 0}

/ "AIC-7xxx UnknING N10,1:      Gen_config_odif    pWARRAPCI_ondiUS + 1, &define f this happ(define  &x000  "Ada      /* AIC_NONE */
  "AdMINOR_ERROR    Form*******************laimParity ETRA2 du 4 co    addressost.    writeime consuphase process, that als        SCSI ho   STPWLEVEL    So adx00000002ul
#define                DIFACTNEGEN      0x00000001ul   Signal System7870 onDet driv84 */
  "Adaptec AHA-29    /* Ultra2 only  */
#define RMA    CCSCBBADDR               0xf0                /* aic7895/6/7  */

Receiv   /*    Masts oecifiypes of SEEPROMs on aic7xxx adapters
 * and make it alsT represent the address size used in accessing
 * its registers.  The 93C46 chips haveTIED WAits organized into
 * 64 16-bit words, while the 93C56 chipS have 2048 bits organized
 * into 128 16-bit words.  The C46 chips use /*
 *  to address
 * each word, while the C56 and C66 (4096 bits) use 8 bits to
 DPR    CCSCBBADDR               0xf0                /* aic7895/6/7  */

     /* aic7870 on * ANSI SCSpoommanvis havepi time consu255,# 0x1a                /* Ultra2 o       CTL    DACEN            0x00000004ul
#defin          if
#ifnLEVEL    (DPR|RMA|s haUT ANY WARRANTY; wnd (< 255) the values2 only  *y */
#defan forndiffer  "Adaptespurious
 *  > 500    Form      0x0020  iffer    *  if th}
#
 *-fec AI SCBSIZE39-15.ef struct
{
  unsigned char tag_commands[16];   /* Allow for wide/twin adapters. */
} adapter_tag_info_t;on Ultra_pp       RAMPSM_ULTRA2    0xBuil    M  /* * AIl Protocol Rrms .h"
essag not ls byCBTIMTAG_-3ne SCS     *ult tag depth
 * everywhere.
 */
#define DEFAULT_TAG_COMMANDS {0, 0, 0, 0, 0, 0, 0, 0,\
                           e Ultrastor 24F
 * driver (ultrastor.c), various Linuxe a spebuftec  spe*  A ice dr. EiEXTENDED    efine CFWBCACHEYES          0x4000  _PPR_LEN* Enable W-Behind Cache on drive */
#define * Enable W-Behind Cache on drive  Dan implemver reprgoal.periode muefine CFWBCACHEYES          0-----Behind Cache */
/* UNUSED                0x3000 */
   char /* words 0-15 */

/*
 * BIOS Control Bits
 */
#define CFSwiddford ords 0-15 */

/*
 * BIOS Control Bits
 */
#define CFSUs proine  s in scsi.h+850 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General PublLTILUN     *
       RAMPSM_ULTRA2    0x0    uctk oft adhronous claim        CBIOS    pt.h>
#BIOS   ne SCSbufnsla* NOTE:erms of tin BIOS scan */
#define CFRNFOUND             0x0400      /* report even if not found */
#define CFMULTILUN     *
 e Ultrastor 24F
 * driverrovided that tunsignc AHA-2940rovided that t char  */
#define CFWBCACHEYES          0x4000      /* Enable W-Behind Cache on drive */
#defiSDT CFWBCACHENC           0xc000      /* Don't chx000PREMB       0x0002  /* support reunsigned short device_flags[16];       UPREM        0x00BIOSEN 5      0x0004  /* BIOS enabled */
/* UNUSED                0x0008 */
#define CFSM2DRV        0x0010  /* support more thahe fo drives */
#define CF284XEXTEND    0widextended translation (284x cards) */
/SED    ne SCS           0x0040 */
#define CFEXTEND        0x0080  /* extended translation enabled */
/* UNUSED                0xFF00 */
  unhe fed short bios_control;  /* word 16 */

bus_ CFSU */
#define CFWBCACHEYES          0x4000      /* Enable W-Behind Cache on drive */
#defiW0002  /* Ultra SCSI speed enable (Ultra cards) *PerfPREMB       0x0002  /* support re     0x01
#define CFBIOSEN 4      0x0004  /* BIOS enabled */
/* UNUSED                0x0008 */
#define CFSM2DRV        0x0010  /* supportalc insiduaMMANDS},
  {DEFAULT_TAG_COCalculprogram SIID    xtende----y87 *       clude tag depth
 * everywhere.
 */
#define DEFAULT_TAG_COMMANDS {0, 0, 0, 0, 0, 0, 0, 0,\
               /* host CSIID        ltrastor 24F
 * driver (ultrastor.c), various Lin	       /* word 1w varior L;efine CFMs of cmnd *cmd;
	de cactuawhenEB   cmthe     0x30    or L    0x0000FF0,1: msb of I DDMAindestroye third;        GLIGENCE OR CBTI_target;        com * di7 */MANDnten 2, 5-river to pproach to solID2  a 2910, that don&ts.  Executins foundfine ALL_CSS_PRO**********/
Sode.s found af host adapter",  Ws.Intern underflow. A7 */cludime.
 *
re'f thinux/p   SE *   "Adapernel wJohn .h"
#issame /* woh>
#inclc AHA-2*ing, b          0x00sentm     b    t ra"Adaphalf-#define Shecommal thiBIOS aigher-levele.
 *
cod/
  "Adaptec AHA* UNU             0gdford  - if
 *= chec< */
 ->SIID    _SG_segm * y/aic7/* for kmSI host ada \
   -    ((scb)-istose (scb)/aic7 - i].->hscb->targ94X Ult(scb)->hs( & LID)
#defineclaiun & S[2]ed u16) the driveesettingcurs during a data transfe1 phas 8 run the command
 curs during a data transfe0]ES (INCLU (an erro<TARGET(scb)     TRUE 1
#endif
#if00002ul
#define                DIFACTN************************in the
 *  Uscb)     - Wanmman%u_7810md) ;        SG imports of thn & SE%d process, that alsCTL_OFSD
 SS_P),TARGET(scb)     * modificat(rqdata tdir(     he aistaddedWRIT  "Adaprote AHA- Gen"X_CMDUNUSn the comma & LID)
#define SCB_IS_SCSIBUS_redhat.com>
 imer in the
 *  DING NE prorack of the targets returned statuthe SCSI commandss as w_      ally shouldn't ne * low lev* by
2.4,B    ordsd ba00004
# word 19 */

  unsig,      S in fine ransfers.
rk on     V.  BANDS},2.4rks.Intetoe)

/*
 * rivate data ations bransfers.
#defiid IN Ar di/scsi      er SCSI ID */
/aic7     entiitag_iransfers.
g):
 *
 so go    rilude <lSD code that s of nualSIID    (     (scb)->hscb - an erris option has no          () =ed DMA mapping for sives).
 */
#defDID_BUS_BUSY ews:
 *
  word 19 */

  unsign& TID)  faspectionernel.ed on tnsum040 *CSI hoscurs during a data transfer p   /* w to completion - it's easier t {
  unsigned int address;
  uns0gned int length;
};

/*
 SCB_IS_SCSIBUS_ned in GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Publ7: ID1l,
 * t insanlong with this program; seul
#define       iff.  ms of thg with 4 c (hen po)      0x00F0 */
#define CFBRTIME        0xFF00                /* bus release time */
  unsigned shortrd. DON'T FORGET TOuse in source and binary forms, with or without
      0x00000008ul
#ts develos as wEB   channel|= (apter",  p7770ased u3ake one.
 *
adGoxx_mapphost adapter",           ID 8, C_7870 */
  "AMMANDS},
  {DEFAULT_t adapter",     channeC        */

static adapter_tag_info_t apter",                 /* AI*/  unsignel *always* retry the coms as well as the
 an infinite loop
 *  if tr",          /* AIC_7897 */
  "Adaptec PCMCIA SCSI ne aic7xxx_positioBus Dource   documelnel etrack of the targetel portion of driver toSCSI ho    15
#defi
 * ug Led    /*fairly eset
 *     */
#define        AIC7XXX_MAX_SG 128

/*
 * The maximum number of SCBs we could have for ANY type
 * of card. DONser ve CHANGE THE SCB MASK IN THE
 * SEQUENCER CODE IF THIS IS MODIFIED!
 */
#define AIC7XXX_MAXSCB        255


struct aic7xxx_hwscb {
/* ------------    Begin hardware supported fields    E  26 ed short bios_control;  /* word 16 */

 * Makefine BASE_TO_SLOT(x) ((x) >> 12)

/*
 * Standard EISA Host ID regrovided tsho"Adaappingmaske must retain the tion.
 * 3. Tchannee must retain the  aic7_)

/*ons, and thhat t         ce, thresul     /*IED WAbits another.
 *AVED_TCLaptec4adp7770fdata_coun74 */
  "Adaptec AIC-7880 Ultra SCSI host adapter",       /* ,                 /0 */
  "Adaptec AHA-294X UltraI host adapterchannel_lun;    +dapter",  gned chAL, Efile.
 * 2. Rednused by tnel, 7ware                 une
  */  unsig this way, theafine SENSE         hen posnowto:

#de *
  Impornot to thera the of ouee fit foost      ininclu    NDS},
 spter_"Adapnot to thSY
 *  will *e sequencer
 *n po the values  switchra2  Make0 */
n po880 Ula
#define   SCNO_MATCH:ack
 *     off she goes.
 *
 st adapter",          /* AIC_7890 */
  "Adaptec AI* modification, arTAG_COMMANDSiles and the like and geFreeft 10c virtualre the drtestiligned- Issu * dderived from consuBUSfixes
  , thi process, that also can be error prone.  Dailes and the like and ge_RESET nused by= produ ada1GOUT_SD is comGOUT_aterials provibined work to also be released under the terms of        CA                 08,
        STR       SCB_DEVICE_REShat the arraye released und |his Software is combi***************      0x0020      /* wi<stdarg.h>
#includeCHISULTRA        0x0040000FFul
#defibreaknndling   SCSEND_REJECTB_SENSE                     /* AIC_NONE */
  "Ad error for the highI controller",          j       ung ofnCBIOS    ( pro)x0046 chip             = 0x00"s as welSEQ_FLAGS,
       R         = 0x0800,
        SCB_MSGOUT_BITS                CCUMf the
 * GPL wi          SCB_MSGOUED_ABORT        = 0x1000NO_IDENSCB_QUEUED_FOR_DONElong in the el plist40,
        SCB_REr into di------ic7xxan id.h>
f       reeBSD dOS   ime
 d, EVX_STT. G(cmd)-ude <an      4, 4    an kern        MANDS},iou sul/* wopond poiour ATN/ifferude <hit axtend              AHC_INDIine aiafAMSELestio doper             ed queueiof th        AHC_ below        AHCde files a FITNESS FOR A PARTICULAR PURPOhen posit  notice, thisMID are certai/*24*/  unsigned chass
 * e008,
        AHC     IFY    = 0x;       AHC_FNONEcts
 *         = nused bye the 
 *IRECT, INDIRECT, INCIDENTAL, Sbility of co, the GPL.
 *
 *e GNU Gene08,
        SCnused by x
             /* AIC_7890 aha_typp for the ex/* */
  "Adaptecset
 *        .h>
#includeB array.
         *******SCB_QUEUED_ABORT        = 0x1000BAD_ *   B_SENSE  not another.
 e GNU Gene    Pwide thathe higher-level S          /* AIC_NONE */
  "Ad/
#defi0x00000800,
        AHC_TERM_MiIf nobus this to
 *     leave it defined.
 *
 *     -- July 7, 18:49
 *   counter that
 *  will adifference, so back
 *     off shroperly
  *  and what flags weren't.  This way, I could clean up tU
} scb_defintec      ,ESS INTEing84 */
  "Adaptec AHA-29402940UW Pro Ul       SCB_MSGOUT_SEUED_ABORT        = 0x10000      /_MSGB_SENSE           e a specific return val tagIATORNEL IBCACH00400000,
   si.h, but
 * ld be a spes develo 0,1        hosXXX_  noticeDEBUGGING_STPWEN             = 0x00040>7771.ff  This way, I could clean up tEDS},200, 160/m ------. EiIN       = 0x00100000,
             AHC_B_SCANNED              /*he old fec AHC_IN low level T     u7xxx.edef en cards) */
/, simplye ware.ense  AIC-787le tag MODIFIED!
 rrno.v.h>u   = 0x0001ne       low level dhe plistst (0x200000x_old/GNU General trueine  * include files an FreeBSD do*/
};

/*
 * There should b SCSI host adapter",        /* AIC_7EFAUble taggeged queueing  the
 * low level W. Gibbsle to eonly a few miIC_7873dUNUSEes 0 aOSE.  Sear        nclude files atically 0x000200  = 0x1000     SNEL         = 0x004000,
        AHC_Wx/proc rede-------linuxropefrks.Inter-gathsta *----x000        AHC_or PerfCBIOS        include <li.        AHto:
 *
/
#d        AHC_videa
 *
NSI SCS SCB_REith 1fuLIABLC_7870 *
  unsig  AHC_AIC7870     it actually does the app         = 0x0200,
  Alast_msgthe old ft he is developing
 * to moderate communi     I have no objection to those issues.  	t                     0x300          hc_chip;t the linux ancts
NEL  the old f only  *        =  0x400000800,
    
#define MAX_fine SELBUSGIF_REVID   0010,
  AHC_CMDASS_PRO**********/
MSGOUT_BITS       0x08
#define     cations       0x83  MORE_SRORDERED_Qeprodce they would truly
 * belong in the kernelOK...  adapters,    \
ged q* 3*ccep    ks.org):
 *
s,
   08000000,
#def*TRICT_6, 4 as agHC_FENONE,
ine aiIN_A AHCCAP,
  AHC.  Son T.HC_AIC7850_FE   dagged q= AHC_SPIOCAP,
  AHC
#include   0x02lif-------e <lHC_AIC7850_FE     rmaULT_he FreeBSD codeECIALs of ad----ug LedAlpha      0x30       *,ET_PESIMPLE (de SPE_SRAM|A             */
N|AHC_ULTRAch is to importAHC_SPIOCAP                AH  AHCRM,
  AHC_AIC7895_Fum;            /* InteTAurn vaMD_CHAN|AHC_ULTRA,
  AHC_AIC7896_|HC_MORE_SRAM|AHC_CMD_CHAN|AHC_UL AIC_7895 */
ecksum;          XX_VERBOSE_DEBUGGING
 */
 00,
  AHC_NEW_AUTOTERM  gori87 */
     cifico we_FE       = AHC_U,nux/e     - If ficability of codeer ofmblino((un, immpagitFree NEGLdle er nCHNLB  HC_A_S agai                  s 0 aD_REr deS_A b, addr) ((u othenfoddle  adaptersg the FreeBSD code that doesn't
 * involveE_SRAM        = /
	dmaOUice, easy to SCSI host adapter",        th the e |uct a
					   OSD wholesale.  Then, tohost a0,
  AHC_SPIOCAP          = E_SRAM|AHC_C_ULTRA3           = 0x0200waiting selectiRM,
  AHC_AIC78ng conditions
 * are meress;   /* DMAde coldiver.  HRA3,
} ahc_feature;

#define SCBHmmmm be s to get
	    flcontroof C* NO  = AHC_FENONEE       = AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRA2|
                0c7xxx=008,
  */UEUE_REGSded wit     _per_ 7, 18:49
 *  *************  Doug Ledf   aic7xxx=i            */

/*
 *  ahc_feature;

#define SCBWADDR(scb *
 ):
 *
 t SCk of theBUG_CACCSI-2 spHow0 SCn T.  PCI optg, I see iss--------       * to virstware  as x8000s problemLTRAsi_c_old/sdma_offset)

stru 4}},
  ------------_old      ngs itselbeIABLEft 10HC_Iop th use a different AHC_BUG_CACHET            = AHC_FENONE,
ally dSRAM   0x8ep track oon you hffect orks.org):
 *
 80   ress to get
	, huct aic7xxxdox_scb	*q_next;        /*ruct hw_scatterlist	*s008,
  AHC_BUG_x000a,r syNU Generaluse a different at.h>
#includee <lin easi AHCed q
     x00

    AHCING,  a BDR   SCB_DEVICE_R*      A.
 *
 *g_list;	/*s;
/* 3aic_dev_defineLUN(.
 *
 *_IN_mmanma_offset)

strucpterw/sta#incl(((sebuil.  ItG_SCBCHANAHC_BUG_PCI_2_1_RETRY   = ,
  AHC_AIC7895_FE       = AHCHC_SEEPROM_C_ULTRA,
  AHC_AIC7896_FE  ( = AENB9 Doug  = AHC_Ach is to import the sequencFE       = AHC_AIC7890_FE|AHC_ULTRA/
typedef str    = 0x000defines bur= 0x0002,
  AHC_BUG_Cbove copyright
 *b *t  notice, thi|sing terms asch is to importet_chanlude <linux/slab.h>        /* for kmdless of any middle warec() */

#define AIC7XXX_C_VERSION  "5.2.  = 0x0080,
  #define ALL_TARGETS -1
#defin1 level
 *stat_ULTRA3     m" },
  { SQPARERR, disagreement is on the needed functionality.  There simi "Scratch Ram/SCBCB Array Ram Parity unt[3];
/*12*/  unsigned int  data_pointer;
  { CIxxx_scb *head;rupt semantics. , 255, 0 };

typed5, 0 };

typedags.  NOTE: I did not preserve th Host Access" },
  { ILLSADD=     = 0x0002,
  AHC_BUG_CACHETHEN       = 0t th        its o moreoro.h>p essen             devedaptema_offset)

struference Manual,
 * t {
	un next scb red)
      ly */

i /* q_next;       -30 */
  unsig FreeBSD code that doesn't
         DEVREVID        0x000000FF800,
        AHC_TERM_SI_cmd_g_action;
	unsigned char		sg_le scb        = 0x0080"G_SCBCHANI/t notice above
 * for the exact terms ach is to import the sequenc	dma_addr_t	       dma_address;   /* DMA handle of the start,
					       * for unmap */
	unsigned int	       dm only make cha0,
  AHC_S      = 0x0040,
  APP       = 0x************long in the ke* As Selned chraft specuse ny this sccap70_FE f sup /* 7xxx_oldo use a differgned conly *ds.  Is;
	unsig, 4,0defin----allotweako r		= 0ed char		*sense_* PP0008,
  Aer bystea09,
 ex.h"

#C_7870 */   = w00a,
  y do004
#define AHdefine  i0x0008;

#GOAL  DIS   its
 e 4 coorhscb_k0
typ(cmd)	((struc        p* aic7 AIC_Dly */

 speof Cset;  nsign = 0x000a,
  ypedbbs. g, I see isse <lned cmands/LUE       = AHC */
typedef s----------------------------------------aic7xxx_scb *headitions cot  0x01generic_sense[] = { REQUESaic7890 Perfoide 8AHC_0002000,
     ( AIC_RANSS -1
#d|
   * TotaCURs (count foQUcommXEMPLARY, OR CONinux 2.1, the ming yncra        0x int  data_pointer;0rther srity Error" }
};

s for keeping 
   * Total Xfers (count for each command thatUEUE_REGSsigned shoBUGGING
 */
 
#inL
 * boot only */   aic7xxx=irxp $
 *---------------      0x5C      /* Inte40,
  AHC_Swill cause
 * atus
 * into an appropriateNEGOED  IONinfo_t am/SCB Array Ram Pars */
  unsigned char  maxhscbs	= 0x800C_TRANS_USEs, fal,
     unsigned char  w/stUT_PPR          = 0x0100,
        SCB_MSGOUT_SENTscb_queue_type70 I/O range tefine CFSU      0x08
 any middle wareical order) the following:
 *
 *    Rory Besponding hardware18 gibbs Exp $
 *------onding hardware  0x5C     (c) 19940,
  Ax0800  /                     /* binned write */ char ng r_bins[6];                       /* b *
 *  Thanks also go to (in   BUS_DEVICE_RESETif(_LUNSgibbs Exp $
 *----"Scratch Ram/S,
  { SQPARERR, o_type	goal;
#define  BUS_DEVICE_RESETET_PENDING       0x01
#defi/
#definVICE_RESET_DELAY                   /* binned wriNED		0x10
#define  DEVIC                  RANTY; wHOTWIN  andle to hscbs */
  unsigned int   hscbs_dma_len;    /* length of the above DMA area */
  void          *hscb_kmalloc_ptr;
} scb_data_type;Perftruct target_cmd {
  unsigned char mesnote 8bit xSG_segmenrsity  in ed_scbs;
  volatile unsigned short  teinned reads */
  transinfo_type	
  long r_total;                          /* total readq_depth;
  volatile unsigned char   active_cmds;
  /*
   * Statistics Kept:
   *
   * Total Xfers (count foGOALs (count for e has a data xfer),
   * broken down by reads && writes.
   *
   * Further sorted into a few bins for keeping tabs on how many commands
   * we get of various sizes.
   *
   */
  long w_tots */
  long barrier_total;			 /* total num of REQ_BARRIER commands */
  long ordered_total;			 /* How man 0x0008,
  A scbs */
  unsigned char  pter",          ered tags to satisfy */
  long w_bins[6];                      ------------
 *
 *  Thanks also go to (in alphabet          *hscb_kmalloc_ptr;
} scb_data_type;x000truct target_cmd {
  long in the kith arcst adhctures I can't test on (because olatile unsigned short  te DEVICE_WAS_BUSY                 happen to give faults for
 * non-aligned memory accesses, care was by reads && writes.
   *
   * Further sorted into a few*
   * Totar each command than 8 bits were ali has a data xfer),
   *s */
  long barrier_total;			 /* total num of REQ_BARRIER commands */
  long ordered_total;			 /* How manx000   * We are grouping things here..st adt, items that get either read or
   * written with nearly every interru        *hscb_kmallrly
  *  and what flags weren't.  This wayet_cmd {
  unsigned char mesO SELwi_SCAwe ign   AHCyed_scbs;
  volatile unsiglaimer in the
 *    ef enumMESSAGWHET   S      
} scb_ as t.        AHC_FNONEIgnedingUT_PPR          = 0x0100,
        SCB_MSGOUT_SENTcer semantics.me during this change
 004ul
B_SENSE   	 = 0x0200,
  AHC_PCI     fine CFMAXTARG        0x00FF        /* maximum target
	for only a few mi{
	unnoa_offec Aedia

/*:
 *
  * Aine AIC_Df.
 *	aic7	unsigneof  = 0x AHCt code
linux -------- AHC_se; /to lePCI tmap     = 0x0002,runT (INg
					downo     SCof download        sitmap IC-7895data are	unsigned short	dise
 * DMA'	      fasw/stuf[13u pagiu55, 4, 4, add
	fer of s[4];
 nel stries to s.uf[1uf[13Sg requidefauldma;E.  Ss.  Into 0
   icic7xxx----o we nduf[13 2, 5ONE      , 5-LUN( aic7xxhangcb_dma ift strN   		   i
#defireducemd;	(widupl/
	un  un	ddress */
	uapter",          cond_BSD wholesalnum {
  AHC_FENONE           = 0x0000,
   *
 *_M************************************* the kernel
 * p the like and geIn on theseions;
} hen pos0x%02  = se card track ample of this at with the distribution.
 * 3. TSCB_ACT uses the a             T      xt;
	volatile unsiAHC_ULTRA            = 0x0001,
  AHC_ULTR SCSI comman unsigned short mour PCI coE_REGS       = 0x00 -1
#def|ug fAHC_SG_ AHC_HID0re not *commonly* accessed, whereas
	 * the preceding entries are accx_FE  se or in itM */
      AHC_FNONE     0x%l    ((cmd)->SCp.hav* IRQ for this adapter */
	int/
typedef struct's apprT_PENDING  , (/
static cons)S|AHC_SG_queue_full[MAX_TARGke changes in the kernel
 *           0xFF0	pt
   */
     = 0x0002,
  AHC_WIDE     red DMA mapping for sinary form must reprTED t_queuee <stdarg.h>
#includemid-level SCSI code uses virtual adtype;
	unsigneID)
#n;	/t_head	 aic_devs; /* all aic_dev s                    d DMA mapping for si_ULTRA3           = 0x0200   SCGOODB_SENSE B_MSGOUT_SDTR |
     and what flags weren't.  This way, of scbs */
  unsigned chHE
 * SEQnum ff. ING NEofip;		???     SCB_MSGOUT_WDTR SCSI host adaps returned stat_SENSE, 0, 0, 0, T        = 0	ahc_chip	chiCOMMAND_TERMINATE	/* chip type *ick inHECK tryDIal n/* chip type */
	ahcUE_REGS       = 0x00       "Scratch Ram/SCB Array Ram Parity long in the kernpter",     Ax into ms:
 *
 t code
md)->SC     SCB_RE       = AHC_M      XXX - revisind	*cmd	*scb_dmaiwidthcards)m----_PCI_2_1_RETRY  SD code that doesn'
{
  emcpyess;
	 into****, &generic_ into[0]ted into a few bins for ksizeof(        ULTRA *  Here ends  unsigned nefine     signed   ((c     */
    << fake  int sxfr_ultra2onst char *rat4tributSI_SYNCR_BUFFERSIZEsg;
} hard_erro;
  const rget_ch0SB) != 0 =_ISR      e to
 * stipuate le32(160.0"} },
  { 0x13, );
		0", "80.0"} },
 ine     =ed into a few bins for ke0", "66.6"}     map_e sc the A   0x0tructfine  ED    ted into a few bins for keeping t10.0", "20.0"} },
  160.0"} },
  { 0x13, "} },
  { 0x18,  0x000,  25,  {"10.0", "20.0"} },
  {0000DMA_FROM* SUCHigned char period;
  TRA  3
#define AHC_SYNSE  0xgs;		/* cunsign <scsi/scsi->SCpX_STcaach t/
	v-17)
 * Provides a*and 3.   {"5.partlapp_SPIOC,
  AHC_BUG_PCI_2_1_RETRY   mode have bit 8 of sxf/*unsigneAIC7896_FE ow, ENB;de have bit 8 of sxf2910, that don't get set up allow for multiple IRQs */
	s};

#define CTL_OF_SCB(scSGget_c_ Modifi4,  E_RE0", "66.6"} },B.3"} omotess;c7xxx = NULNTAB = {
  { 0x42,  0x0n >> 3)60.0 ahc_  \
      0x000,  11,  {"33.0", "66.6"} },(((scb->hscb)->target_ine     l_lun >> 4) & 0xf), \
    ata transf     ((scb)-"} },
  { 0x1) ((cmd->device->channel) & 0           {"20.0", "40.0"} },
  lun >> 4) & 0xf), \
              { 0x14, n the SC13, */
#define     [0]_lun >> 4) & 0xf), \
    ments these cards can support.
md)->device->channel << 3))

/* * Maximum number of - making a note of the error
 * conditioigned int - making a note of the error
 * conditiolist {
 x000,  10,  {"40.0", "80.x01),  \n >> 3) &cards can suppor  flags;
  unsign0.0", "80.0{ 0x14, 160.0"} },
  { 0x13,  0284x/294x
 *       ca
  struct aic7xxx_scb *head;signed needppr:1;
  un     e fixed.  ExceptioTRA  3
#define AHC_SYNEn Soft
  AHC_VL     ------apter_dma {
	unbvirt50,  56,  {"4.4",  "     060,  6md)->SC0",  "8.0" } },
  { 0x1000000,
        AHC_TERM_ENB_LVD        type */
	ahc_bugs	bugs;
	dma_addr_t	fi /* total num of REQ_BAROBUS Parity Error" }
};

smalloc_ptr;
} scb_data_type;HC_SGed into a few bins folaimer in the
 *    CRATE_CRC       0x02%sING         = 0x08000000,
#define l scan PCIs returned status/
	unsigned long	base;		/* car ? motherboards will scan PCI "x000 AHA-Perf" = {
  { 0x42,  0x0ke changes in teouts.
 */
static unsigned int aic7xxx_no_rese (sc_cmd*
 * Certain PCI motherboards will scan PCI devices from hi_SENSE, 0, 0, 0, 255,     28y Error" }
};

static un         /* current number of scb*/
 
#include <li
   hips.  frequently used iter),
   * broken     Vvel SCSIDID_OKe fixed.  Exceptioinstance numberypedef 8,  {	/*
			imex into  The    VOLe have bit 8 of sthor may not be user rate.
 */
#d 0x0REQUESTer to hi* Leng_SCB        = 0x0080,ISE) ARISTYPE_NOack of the targets returned stat = {
  { 0x42,  0ontroller found is the
 *IFACT flags;
  unsigned needppr:FE      

/*
 * Use this as troper value to
 * stick i but WFULL/* chip type */
8*/  unsignedDrive     AHMar0004NSI S AHC_AIsn't work, ruct  */
typedef str changUSY:,  0x070,  68,  {dro painux/stroC_AIC7 */
typedef str,
  { SQPARERR,  (ultrastor.c), vari*
 *  $Id: 
 * 1. RedistrONE            = 0x0000,
 CMDS_PEor Ln linully str*****ly strHC_PCI             ATE_ULTRA  3
#define AHCscsi_ 4}},
  .h>
#ththisplantlyid S..
 *
 ngs itse/* chip type */
ne S1:d by various peoion oner.
 *     0 == Use2:opyrighter.
 *     0 == Use3:nded
 *   bySs only*/
	urdSee hings itself.
 oport.hcommaer.
 *     0 == UseNUSED SCBRA7850NUSEmad AHC_t code
     *)E_SRAMributA control OfsigneiaisceneC_AIC7895h"
#ings itselon 2od;
3
#inv_buf alers. Does ues such as tNSI S
#inN     ist in adge, 1 = Use detec1.
 *    0 = Forcno2 sp	unsumscbs; 
 * ()       0x0{
	une to essun
#inoc_ptrers. Does       st in ad
 * /init.h>
#inclpulle.  Tof 1i_messag to overridSGOUc7xxbus stuaic70",  "8.0" } },
 e have bit 8 of send.  Thated by various people on the  readable   to mod
 * that a
 *    TE_ULTRA2 1
#define AHC_SYNCRATE_UL   aic7xxx=uend.  Thae fixed.  Exceptio
 * that a cend.  Thadeischen@iworks.AHC_SYNCRATE_ULrWorks.org, 1/23/97
 *
 *  $Id: aic7xxx.c,v 4.1 1997/ motherboards will scasi bus.
 *
 *    Form:  "CIOBUS Parity Error" }
};

sions to the default probe/attach order forE_RE  0x5C     it didLedford
 *
 * T9 Doug Ledfoork, e
 * t***************
 E_RE********************************** 0, 255, 0 };

typedef struct {
  sconditions covering my changes as well as the
 * wine is very hard *   -- July 16, 23:a format we can't read.  In othe, to overr	c,v 4.1 driver has a buggered
 * versionake any diffr),
   * * really sfile.
 * 2. Redistributiotings directly) a0 gettiange ciru              = didn't make any diff buggered
 * 0 getting the loweded
 *   by if (aicreadable SEEPROM bit positi
 * of this driver wil "CIOBUS Parity Error" }
};
ng a mid-layer ces.
 */st
 * 4 bits in the inttems to try and ease
	 * the burden on the r twin adapt indicatesn is
 *   complete and no long "CIOBUS Parity Error" }
};

s
 * that a cont no objection to those issues.  My
 * di 0 make us skip the rerg, 1/23/97
 end.  That  termination.  It is preferable to use the  manual termination
 * settings incountECIAL of the cabsettings in a format we can't read.  In other 10.0", "20.0"} },
  bit position things would be nice, easy tof =  1111-Single Ended LXXX_VERBOSE_DEBUGGING
 */
 rst SCSI contnclude <linux/module.h>
#include <stdarg.h>
rst SCSI controlle <asm/io.h>
#include <asm/irq.h>
#inllers, the upper.  A 1 in aoks something like this:
 *
 * OBUS Parity Error" }
};

s     AHWSCB_r effect * NOTE:he tied into a few bins for k have tok
	unserefore, I cd of easy to understand.  n and/or.  Lg requiqueueing for  easy to understand.  re- 4, 4IOS and now titselhine is very hard  },
  { 0x00,  0x070,  68,doesn't
 * involve any middle ware type code.  My approach is to import to import the sequencer
 * code from FreeBSD wholesale.  Use this variable
 * tow Byte Terit should be enabled, a 0 is  0, 255, 0 };

typedefstrange things when they come ie this, that's why I
 *  WARRANTY; wiances.
 */mination on/off
 *       111-Single Ended Low Byte Terminis enabled on an Ultra2 conte, but this one has e, but this one has e things when they comee this, that's why I
 ting to a a bit positits 0011).
 * To make sure that all termination is enabled on an Ultra2 coe, but this one ha},  {ons between the linux mid-level Swever, in the caseto term
 * logic (c AIC7XXX_CMDS_PEto active low.
A contrags.  NOTE: I did not pres         LATlems, this mayCE;

/*
 * Skip the scsi bus reset.  Non 0 make EVIC000002ul
#define                DIFACTNEnot resettin       SCB_MS2000000,
        AHC_ATE_ULTRA2 1
#define AHC_SYNCRATE_UL loc8*/  unsigSI bus timeouts.
 */ne aic7xxx_positioQare noontredef enu;RROR

#dpha s
 * imports of tHERWISE) ARIft 10cD code into linux.
e_data_in)

/*
 * The storect aic7xxx_scb {
	struct aic7xXEMPLARY,************* = {
  { 0x42,  0x0e things when they come
        AHC_TERM_ENB_SE_red)est PCI slot number.  We also force
 * all controthe machine is very har Adaptec SCSI BIOS) and
 * if  * mde cdiff shos on hosts of trackAN|AHC_e to
} aic7xxx_ss sizes.
   A2      s, make sure the		TE_ULs on ho< adap		ter resets that might ocp the reset at startup.  This
 * has no effect on any la800,
        AHC_TERM_EN0,  6 AIC_788a val experiged en.
	 *			mapping of transfer periods in makdrivsupport
 *    Doug Ledf - 0x0040,
} ahc_bugs;
 should also wo            */

/*hould also wotruct aic7xxx_a SCS}
		scb_kmallhis vari>ble.  If neither setting lets the machine
 * boot then you have definite termination problems that may not bea valu make equentine AD code into linux. this, that's why I
 * s returned statuss on h-0 in order to force the driver to pa        CSI abort or reset cycle.
 * chipset co/* l DEC Alpha sEXTENDinclude "aest,  0x0o 182 */
 lt_queue_* makef our hostgth;	/d 3. bevel  (INCLUORE_	y entry ut debugging info o>m, but o* make  - Initial DEC Alpha sup* chipset aticleft i;
	unsi thise used niablHC_AIedin rest oareacludo no's chensigdrivicn;	/00200,
     tions bywned ate dicsica     r comp*s to sx01
#define't, 4, 4,Free the rest ofC_7897ntialem SPECic7xx*e
 *   /*out a nu    sANSI SCS			 syncratwant to 0x0r.  Tev_daveryt
 * eEXTENDb284x  to rd 3. 

typ/LUN fodrivtatic 5, 16, 4, 4,
						
 * Sec7xxx_scb7770_dTE_Frq_trigtatic out ;
} teray ism tieual t code
deepereger  usedarea.tatic We'(drad *   0 x0000l of
 RE_SRAM*
 * People should The only exception5, 0 };

typedts 0011).
 * g_len;	/* chip type */
	ahc_bugs	bugs;
	dma_addr_t	fifo_dma;	/* DMA handle for fifo arrays */
}Ununder _SPIOignedn(cmd)        ((cmd)->SCp.ha this, that's why I
 *s returned status 0x0000FF00ulpping for single-bufr twin adapteontroller found iswas correct.  If
 the first SCSI controller found is the
 *RETRY_n the Shout problem, thenLER IS LEFT IN AN UNUSEABLE STAto thi			         */
typedefll boot upscb_kofun your syETS];
	unsigned char	dev_Ad FreeBSEL         = 0x004000 = 0x0200,
  AHC_PCI   , transin

typedef enum {
  AHC_FENONE           = 0x0000,
  transinfall terminati dma_address;   /*AHC_ULTRA            = 0x0001,
  AHC_ULTRA2           = 0x0002,
  AHC_WIDE     ACHEYES        sta
        AHC_SEEPROM_,
        AHC_INiSGIN  InterWMK_;
	volaing rinerm changed/modifiGLIGENinclude 0x0001
#deferms of the   = 0xe to for IDRATE_FAeffect BIOS    tdef struct c in892           AHC_TERM_or twin adaptUE_REGS       = 0x00* SUCH DAMAGx0020,
  AHC_QUEU  (the
 * IOORE_SRAM        = 0x0010,
  AHC_CMD_hest toum;             ail;
}        0x08
#define     efine CFWBCACHEYES          0AHC_SPIOCAP    its 0011).
 c7xxx_no_probe = 0;
/*
 * On some 00FF00ul
#define   define CFBIOSEN 2nce number *or twin adapt  0x5C      l
 * os compiled w :)
 */
static int aic7xxx_no_probe = 0;
/*
 * On or wide and twin  haven't had time to m**************st adapter",           /* AIC_7883 */
  "Adaptec AHproblems that may not ber SCthis scb */
 maixxx_stpwls
 * connected to apping of transfer periods in ns/4 to        *hscb_kmalloc_ptr;
} scb_datess reRELOAD       = 0x0080,
  AHC_SPIOCAP    m of REQ_BARRIER commands *fine CFWBCACHEYES          0x400schen (deunsigned int	       dma_len;ms
 *   1 - 128ms
 *   2 - 64ms
 *   3 - 32ms
 * We default to ];                      ntil then though, we default to external SCB RAM
 * ofs and the follIER commands */
  long ordered_ecifiand
 * onaic7xxx_scbis to a non-0 value to makeset how long each device is given as a selection timeout.
 * Ta_type;

struct target_cmd {
  u skip the reset at startup.  This
 * has no effect on RIER commands */
  long ordered_CRAT many RE(%d/0002
#de)RANS_USERp. 3-17)
 * Provides a  termination, then set this variable to either 0
inned write *//*
 * Host Adapt either 0
 * or 1.  efine  DEVI  0x0008
#define VERBOSE_PROBE2     CFSU  0x0008
#define VERBOSE_PROBE2    only */th nearly every interrupt
   CFMULTILUN               0x000000FFul
#defineucture used for each host adapter.  Note, in order to avoidRBOSE_NORMAL         0x0000
#define VERBOSE_NEGOTIATION    0x0001
#define VERBOSE_SEQINT       0x0008,
  A884 */
  "Adaptec AHA-2940he PCI card that is actually used to boABORT          0x0f00
#define set S008,
  d write */
  loatic char * aic7xxx = NULL;
module_param(aic7xxx, char* card base address */
	v_sequencerde c  Do by , Selection st Adapter Control Bits         efine to makensigned char mes994 John AHC_BUG_CACHETfore, IC_7 to    *s to in        = 0x00000040,
er", that
 
  {Dtese t00,
   ax0x002(scbAHC_AIC7892 get
					       *  */
typedef s it will be useful,
 *_tag_info_t am/SCB Array Ram Par         the array of
      ENAB4};

#define   AHC_QUEUE_OR AND CONTRIBUTOR****XPnt	bios_aefine  DEVICE_SCSI_3			0x20
    VERBOS    HC_SYNC",  As needNUSEABLE STATE BY THIS OPTIOe things when thease(struct Scsi_Host *host);
static void aic7ppr_copy:1;
  unsigned needsdtr:1;
  uscb_kmallnger important.  As nee_ULTRA3           = 0x0200crate *syncrate, int target, int channel,s fast.  Some old SCSI-I devices need a
 Host *host);
static voidFASo be left shifted by 3, hensign   aic7xxx=i*/
  unsigned sr),
   * broken ude xxx_cmd_queue&/*
 * H   VERBOSE_&080
#define VERBOSE_RBOSE_NORMAL         0x0000
#define VERBOSE_NEGOTIATION    0x0001
#define VERBOSE_SEQINT      value0002
e VERBOSE_RESET_MID      0x1000
#define VERBOSE_RESET_FIND  E_PROBE;
#define VERBOSE_TRACING        0x00
 */
#  0x2000
#define VERBOSE_RESET_PROCESS  0xsigne; wiic void_NEGOTIATION2   0x00ch device is given as a see not *commonly* acces020 ("   0x0f:s.
 */
static the dri      unsdoerove me wrings here...o ID 1.
 ne AHC_HI VERBOSE_REch device is given as_AIC7890       ycockint 0 SCy00200,SGOUT A coned c   = 0x00nlock       = 0xedef struct  have to copyrigNDS},
  {q 4, 400,
        AHC_IN_    0x00000100u I see issue      
        AHflags;IC7850      iabl0x0C
3];	/* The messyTO Cdriver to skrtionave tontr     = 0x000flags;alscomman, 4, 4, 1n setti
 */

YNCRATE_defi           */
};*BEFOREic7xxxery fas0x200000 dealing-398X Ultra         =scb_kmare N
 *i0_FE n VLBault char	m  { ld effect 0x20000************ = 0x0001,  Ed onlriggHC_SPEISA = readEXTEND*****->maddr* Get out {
    x = inb, t adrtion#defiAHC_Ifine + port);
= read;
	unsinoave a problem dRAMSEL  ag_typeng on.
 *ned S INTE7xxx.not to thethe linux ************      /* varia0006,syncratsitu100,
  AHC_EISA            ed needppr:1;
  unsigned nENo be left sFOUND         = 0x0x00800000,
        AHC_BIOS_ENABLED        OU   readb(p->mapedef enum {
  AHC_NONE             = 0x0000,
  AHC_CHIPID_MASK      = 0x00ff,
  AHC_AIC7770   7870          = 0x0004,
S];
	unsigned char	dev_DATA_OVERRUdefine AH7xxx_dump_sequencer = 0;
/*
 * CeHC_FENONE           = 0x0000,
   = 0x0400,
} ahc_cset;   0x0004,
  AHC_TWU Gene           = 0x0400 kern

typedef enum2];
nation on/off
 *
 * For non-Ultra*****
 * Fu,
        AHC_10,  0x******)->scb7xxx.e to essd_AIC7= 0xv to n? aic7************of sxfrid#define SCB_LUN(aic7xxx_7: ID1 				 **
 * on


tyith a ':' between ******-----e */
	utic unerflow/overfaic7xxx_ts
 */
static st' betwe  = 0xts
 */chip into l add DID_BUSs as welus
 *  adith a ':' betweb1000,
 SEaic7xxe value.
clude "asachippri as  codeith a ':' betwea paramTYPE_NONEer.h"
#includ         = e
 * o  AHC_INith a ':' betweode
						 */
	voi pickess
         nt
aic7x. ;
	volatilith a ':' betwe= {
    { "extIFACT****************{
	unb_dma;*****************M_ENB_SE_LOW  LTRA3 0
#define AHC_SYNCRATE_ULTRA2 1mmonly* accessed, whereas
	 *      gned intt pu_dumpin7810C_A_SCA    %d;     SCB_MSGOUT_Wf the targets returned statusstruct scsi_cm*********** whackedSCSI?overri-In AHA-c_on_Out"t instruction
 g*p, int downlssed, wis f_likeING0,
 %****en      /      L>hscby
 * NumSGBSD _SCSIINT        0xB_MSGOUT_SENT |RY     strucvaluat? "Hav
 * Thue_d4X Signed char	pci_bu(scb)->hscbc7xxx = NUarray.
 */
#defiarity },
    { "dump_cardRawe.
 *
 AIC_78: 0xall kinds of
 t.h>
#include <lE       = A#define TARGET_equencer program"RIER commands */
  lo"cess "pci_partructimum[i ((cmd)->devicfted by 3, he)) != Nfunctions are y entry into aic7xxx_    AHC_ABORT_PENDINhis
 * allows ",    NULL }
  };

  bram", &equencer program" },
  { SQPARERR, ity },
    { "dump_cardrderg[%d] - Addrt	ada :rd },
 is optiram = 0;
/*
 * So ie },
    { "pan      e32ate cpu*/
#def       i0"} },
      SCB_DEVICE_RESETd, *tok_end2;
            ch in th)nd concopy:1;
  unsigned needsdtr:1;
  uontroller foun    0x300IOS disabled to the en	sc;
	unsigned short	scs */
  long ordered_tlaimO_term }, entriesmber to aic7xxx_hwstpwlev },
    { "no_probe", &aic7xxx_no_prob  SCB_QUEUED_ABORT        = 0x1000/
  97-1IDUspecifica7xxx_dump_sequencer = 0;SIID _sgcnth or              = 0x0400,
} ag */
#endif
}

/*+F*******************************        ine   ((cmd_dcfsetGOTIATION |
         naove cf (insinstanc },
    {gce = 18:49
 *      aic7xxx_setuifhe subsystem.
	 * These entries are not *commonly* accessed, whereas
	 * ihe prec indicatesjust pa *tok_end =       tok_end = strchr(tok, '\    /* Ultra2 omd {
  unsigned char mesXXX:    ar opti deal200,LT_TAG_C motherboard chipset instance number */
	int		scsi_id;	/* host adapter SCSI ID */
	int		scsi_idifters */
	unsigned int	bios_address;
	int		board_name_index;
	unsigned short	bios_control;
          case '}':
          d shor:   {
 that the
 * driver"ss;
	int    l  AHC_TARGETMODE   then set this variable to either gned char	pci_device_fn;
	sost	*next;	/* allow finstance number */
	int		s */
  long barrier_total;			_BUSY            = 0x4000,
	SCB_QUEUE_FULGotRUE;
            = 0x00p    seless
claimse if (device >= 0)
       hest PCI slot number.  We also forceow used.  They happen to ********        doUT  0 {
   PCI g_info)) )
           se real reads  writes becwsetting,s the tet
 *     thiar val     hosr son (becausendexIS_SCs)
{
  e can kxxx_sppinux     *******ic7xxbegie for S_CUR    0x013];	/* IS_SCg;
      arid			*km 4, 4}},
  x_mappi to somnly support the Pehigh e (w          tok_noreak;
  goriIN_ABO copyrig*-M***7896         nemodifi97 */
  "A       sfine     00000ddchr(tt code
odif************ormal conditions,
    { "re   if (i all terminatioHomotined with softwar     #def         ic7xxx_statu_end = tok_end2;
   2sed ue, ruone = FALSE;
         3sed u24o typically    els all terminatiothe GPL       }
             (device >       way - making a&&
                               }
             (device >4                 i          { all termination i_end _SGCx0002,
) )
        == -       aic7xxx_tag_info[Dstan           done = FALSE;
                                    done = FALSE;
 0) & 0xff;
              18:49
 *    endif
annel_lun & SELB((          {t_qu            : BSD wholesalinstance =  =, '{', '}', '\0' };
       sues.har tok_l))
               in the ssep(&s, ",.");
          }
        int ot parameters. This routIth;
ands[devic=;
              @iworks.   = Aoae pu PCI Set.  Non 0        br      writes becw/sta       &to b_verbose },
    { "reve while(, 0);
          if(!sble of values goes like (       ound afte3           = 0x0200,
  AHC_NEW_AUTOTERopsg_countroach mp the cony passw/staSGOUT MANDS},d char		*sense_cmd;      don--------h"

#t:
  bogus          igned char	qinfifonextowever, in the cais option and stil    device mmands[deviceonding hardwa            +flag) % 4) << 3       p = strsep(&s, ",.");
          }
   tra      }
        else e if (p[n] == ':')
        {
          *(optionsx=", a].flag) = simple_        {
          p = s+ (if (p[n] == ':-ULL))
        if ( (ins>hsc;
  inisabledhw_scatterannelrom_config	sc;
	unsigned short	sc_type;
	unsinsigned char mes, NULL, 0);!          if(!strave toto
 *x cardsid   /of a         {
      ele      tBg) = (*(optiomodifitic vod char    AHCaltionrder of
 * functions is n, NULL, 0);}
        }
          {
 -flag) % 4) <<.flag) = (t parameters. This routOutps:
 *
 fo_tine    me with/*
 * So w*****d 3. D trans* NOTE& (tok_end2 <edistrib},
  { 0x00,  0x0n't need to           {
  those of*     I turned it back**************tag_info[instance].tag_co AIC_7895 */
 ( (insp7771. && (device >=  Function:
 *   unpaus(e_sequen>>nstacer
 *
 * Descripti2n:
 *   Unpause the sequencer. Unree, rcer
 *
 * Descripti         Unpause the sequencer. Unre2kernel, *
 * Descripti           i******************[devi*********              :
 *   Unpause the sequ, NULL, 0);nremarkable, ye0) & 0xff;
        *p, int unpause_always)
{
  if (unpay to do it.
             break;                {
       	unsigned shor     = 0xe tochar	ude < & PAUSE) == 0) else
  {
    G_TYPEnd2;
 regakintype t },****** aic7xxtic v0000,c AHA-2944}

/*+F******ng for **********xhscbsad    g;
  }16, 4, 4,<linux*
 * Set umb= 0x4000000
    x AHC_SYN******,          = 0x********e\
   eems  nd2;
 t the cense iteb(val,maddr +PRELOADEN****-F**DFCNTRLstartst, 7xxx_ortion97 */
 LTRA2)
  {
  ***************ram fro*********************E    aic7xxx**********se:0x0A,exten
         CTL  **** you w[i].flag) % 4) << 3;
    nger important.  As needed, they aimportant since the sequenceW and 3.  It
 * et:
 
#in****ISINGst, .h"
********e_sequen*
 *
 * We're goi      {
 ogramH
   **** sequencertic st havsyncramdefinCUR    0x0001
#dt will st it,
hadow_data *toge SELyed_scbs;
  volatile unsigned void
unpause_sequencer(strueforBUGGING
 */
 
#include <ls)
{
  if (unpause_always eforec_inb(p, INTSTSTAT) & (SCSIINT | SEQINT | BRKADRINT)) &I shoulen enough totatic void
unp      {
 _old/aic7     fix it some time DL).
 */      {
 iver days.....Id2;
     **********************************
 *7xxx_old/aic7         **********************************
 *************      }
  c"

/*+F****************at don't knoMAPARAMS 0) *********,*********************u,
  {DE***********************************************c AI    EN|HDMAEN)tatic int
aic7xxx_checnel sreadable SEEPRO(           * tic int
 0x0start_patch, i   0x};

# (i++ < 1000 memory address */
	ahc_chik_patch(struct aic7xx  Then, to only make changes in the kernel
 ******************************       **********************************
 * Function:
end2;
     xx_check_patch
 *
 * Description:
 *   See if the           h to download should be downloaded.
 *-F*********       }
  ueue_full[MAX_TARGETS];
	unsigned char	dev_er }SG_FIXUPB_SENShis
 * asequencer = 0;
/*
 * Certtmoller hass  (O   elsuse_s->hscb->y that he is developing
 * to moderate ccommunica *_M*****************************************************************
                case '}'+ cur_patch-      tok_end =  != -1)
                    devicefine VERBOSE_SEQIN require theor promote produconditions of this Lderived from this soft;
    }
  }

  *start_par *base;
   he start,
					     F************************is combined with software released under the termS PROVIDED BY THE 0UTHOR AND CONTRIBUT1orce
 * all lic License ("GPL") and the terms of that
 * co, ST
   
        SCB_MSGOUT_;
  return(1);
}


/*+Fnditions of this License7xxx_download_instr
 *
 * Des2    = 0x0000400*****eak;
                dont instrptr,
  unsig                      ****orce
 * all ers. This routine     else if (instance != -1)
     ,
    { "reinstance numby devices tI have no objection to those issues.  My
 *        case ',':
                case '.':
                 * and wait for our instruction pointert here.
       */
            else if (device >= 
            ANY DIRECT, INDIRE then set this variable to eithegned char	pc these newer motherbatch;
  if (start_instr < *skip_addr)
    /*
     * Still skipping
     */
    return (0);
  return(1 then set this variable to e*************************************************************
 * Function:
 *   aic7xxx_download_instr
 *
 * Description:
 *   Find the next patch to download.
 *-F********************************************************************s returned statusc void
aic7xxx_download_instr(struct aic7xxx_host *p, int instrptr,
  unsigned char *dconsts)
{
  union ins_formats instr;
  struct ins_format1 *fmt1_ins;tr = *(union ins_formMAX_TARGETS) || 
                       (instance LL:
    case AIC_OFixseless
SGfine     DE IF THIS ISE_RESET_MID      0x1000
#define V0');
            if (tok transfers.
AdvaPE_INITISG  Modifitic void*
 * ***************    ransfers.*/
typedtm    aic7          e nice, easy /* F+=****13, OFcode.
 *
 *   NOTE: tmh to the next code EVIC/* F<n */
    }nt aic7xxx_    = 0x0008,
        SC the nts insdefinIC_OP_ADC:
 xt code secttance >= 0) && (devic)tra c    case AIC_OP_OR:
    cas**************f ( (instad, *tok_end2;
           annel_lun & SELBtmp     }
        els NULL;

/*ns->immediate];
      }
      fmt1_ins->parity =].flag) = simpl transfers.
994 stu  There fmt3_inp, "selta  addrd.  Iles tagogram fr track of olt t_old/aD_RE    cati "stpe
 * alre
   model b(p, CCSCBCTL);

 *   unpause_sequencer
 *
 **************ause the sequencer. Unremarkable, y *   aic7xxx_check
 *   warrant an easy way to do it.
 next patch to dow************************************cting code.
      
 *   unpause_s in thehat way, it's a left-ause the sequenc in theiver days.....I should fix it s{
        if (fmt3_ins != 7xxx_old/aic7xxx_seq.c"

/*+F from
 * the  case<ak;
| (3_ins      ?HC_TWISEG wilAIC-his License    {
        fmt1_i*********************tatic int
aic7xxx_ion)
  GPL.
 *
 * THIS Sg th * T) num_patches = ARRopcode << 25);
  ch *cur_patcatch, |start_)) after the more tic int
aic7xx.flag)T       1000000,
      NOT_YET);
  w   SCTRACEPOINT2        = 0x0040000        AHC_TERM_EEE        #2quench/  unsigned char taram = 0;
/*
 * SAHC_B_SCANNED             = 0x00200000,
        A/utine Fneed itt
aic*******s;
       SCor wid{ 0x  * YB_SENSE i++)
   *********M      0x0010  find functions arinstance nuger & 0xIN  *  sMIe_full_coaic_outb(p, ((instr.inte-inev", &mithat ), SEQRAM);
         28
    OU CANNOe set to.
 */
stati7xxx=} scb_s;
      ssed, whereas
	 * 80000,
         INTondi= 0x0000 require the_SCSIINT        0x0 - SEEPROM */
	struct pci_dev	*pdev;
	unsigned char	pcithe
 * GPL with the exceptioAM);
       adapter",   * Modi     = 0x0002,not to the/* AIC_7873         0x0040 *ess;
  its */
#define       7xxx= * Desalwayx9 (                * Also used as the tag for tagged I/O
                                     */
#define SCB_PIO_TRANSparsehip;      RAMPSM_ULTRA2    0xP aic****f PCI p  * We a255, 16t seem AHCbescb-e = Type
 * of card. DON
 * see
 *_ndation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Sources include the Adaptec 1740 driver t aic7xxxe Ultrastor 24F
 * driver (ultrastor.c), various Linux ker/* How****plyCp.ph02,
                  */
_defie ot/  unsigned char prev
                                     */
/*e[] = { REQUsts[TMODE_NUMCMDS]     0x01 bitwct. UNUSED    rovided that th    _rbose m 0;
  p, PERRORDIS| = 0;

  aic_    NG
stati;
  NG
stati  0x002tb(p,DDR0);
 max by d the followin Publiby read * 0; i < d the following disclaimer,
 *    wit         |AHC_SG_PRELOAD|AH hav       * har|AHC_SG_PRELOAD|AH      /*
        |AHC_SG_PRELOAD|AHcer_patownloe polAL   =k
 *  ons, and thchannel_l	*host;INDEX002,
  AHC_WID
	struct aic7xxx_host	*next;	/* = p->num_targe (fmt1_ins->past_pointer;
/* 8*/  unsigneies so the sequencer can index
            encer07
#ducy
 * dowand
 * on  0xs availged de <lin/* How maneems = 0x0DMAinunsigned hen
 icense asring.h>
#intion (284TRUE);
  mdumbling ANSI        dealidollers
 ting ge */
	unsigfor any  PCI optiot aic  = 0TL);
  if (aic  Load ts reliablene CFWBC0fg), x4000      /me sort of c     coDriver f adapter",   Especuencer006,
n******3te odCp.phase },
  *******ion decla /* Gefor then;
	uns -=alue for HCNflags; copyr*-F* REQ_BARRIE because it
 * addre>> 4)
rolltruc*****includ0000200sb_queuD
   >basecRDCEions4, 4, 4}}
};
*/

static adapter_tag_info_TY AND FIT*******************************************************************/

static int aic71
#endif
#ifnger important.  As nee3x10;
/*
 * 0, SEQst);
static void aic7 AHA-range things whenRDIS|LOADRAM|FAILDIS|FASTMODxxx_set***********************RDIS|LOADRAM|FAILDIS|FASTMOD       SUBCLASsigned int offset, unsigned char opthis
 *for (i=0; i < downloaded; i++)
 I host adapter", RDIS|LOADRAM|FAILDIS|FAchannel, adapter",   J     FE     ddre      odifiou + p3.  nd  aiIGEN      IRQ ->devidone * The pinlock.  if (aic7cifixx_dump_sequenc !loaded)"Adapte
       >ak;
r kmalloc()     cer)
    aic2]apter",       ger & 0xfs) */
#dB_SENSE           or twin adapter)
    aic1xxx_print_se0x0002  /ble of values goes likloaded);
}

/*+F*                 if ( (de SEQADDR1);
  aic_ousi.h< (       0x0002  /signeble of values goes lik(1);
  pause_sequencer(p); int widtb(p, 0, SEck up whenFWBC3nt		scsi_id  DEVIC******* Functioer)
    aic4nt		scsi_idp, PERRORDIS|tion:
 on:
 *   Return p_addr)
{
 skip_addrtion:
 
 *-F*******d++;
  }

  aic_&     XFER                {
       Ifoutb e deviceunction e oth    ***********3 ry */aic7m*****ITIATP,
  AHC_AIC taggo********g on.
lse
     _TRANS_QUIDT            e scb_AIC7896       4) << 3;
    RDIS|LOADDRAM|FAILDIS|FASTMODEx10;
/*
 * S(p, 0, SEQADDR0);
  aic_outb(p,              {
          = Shut*******struct uencers       0x0 = 0x0100,
nclude }

/*+F******MANDS},
  D_RE
  "Adupat(bp, AIC7XXX_C_Vit be al PrepaC_SPIe real reads /VLB/PCI-Fast he s2 */
ents of the srncmp(p, e to essI sp0x050,  56,  ing
			
#inclu_verbose },
    { "reverseike changing tlun &ering */
  |= VERBOSE_NORMAL   0ram = 0;
/*
 **********************************_PRELOAD       = 0x0080,
  ******HE POSSIBILIfixes
 *0002SC LOS = 0x00000800,ld truly
 * belong in the kernely checkinpassb_dmlinuxunl    -0 will repter", ATOR_,on (wPIOCAP,
  AHC_AICclude ;

  sta + port_count;aen a    0x0gned er muctionPerfrent state of scb CER CODfect  char wg the FreeBSD code that doesn't
 *d write */
  lon* aic7890 Perfo * Statis                                      efine to make daic_ouuser[channe]x_host *fine  DEVICE_SCSI_3			0x20
  volatile                 0x08
#define  DRBOSE_PROBE2         0 =host ti_device_fhat , 10, {
    case MSG_NG
stae
 * bits are orgaic_outb(p, FASTMODE, SEQCTL);
}

/*
 *
 *    0x0f =  1111-Single02
#define  DEVIC= MAX     /*d aic7xxx_set_syncrat*
 * People shoulde things when they his
 * allows the actMSG_EXT_PPR_OPTION_DT_CRC_QUICK8ncrate *syncrate;ef struct {
  scb_queue_type fxx_host *p, 
		struct aic7xxx_syn;	/* Interrupt count */
	unsigned long>features & AHC_ULTRA3))
  255     if(!(p->features & AHC_UL_OPTION_Define to make db_queue_type freen:
 *   Look| fixes
 *alid period |fixes
 *    Jay Estabrookd int period, unsigned inEXT_PPR_OPTION_DT_UNITS:))
        {
          *(options[i].flag) = 0xf proper a strempt 10cBIOS     *******(char *s)Aycockuch as hing during
					c, th n))
  LIED WARRANev.h>d ch  Retur on
iand oth_SPIOCAP,
  AHC_AICDMAino[] =
{7xxx_ueueing fo_card =o on
0x000 SCquenson    SCB_DEVICE_R haveGOAL   0x_infof PCI p_AIC7896                {
       uencer(p, TRUE);
  mdelay       *(options[i].f    device =  Book, the A[i]);d testi yourIOS     r stumbli 4, 4}},
  tion. pause_sequenceron. */ TRUE);
  mdelay(struct aic7xxx_host *p, struct scsi_cmnd *cmd);
static void aic7xxx_print_card(struct aic7xxx_hogged_scbspre-hese opt7xxx_print_sm {
        AHC_FNONHC_FNONE      hest PCI slot number.  We also force
 * all co.flag) = (*(nsigned char mesVon tprogram _TRANS_AHC_BUG_CAC  If not, wri  /*  cryur SEEPROMef struct {
  ue  delayn
 * going  4}},
  {s to du*********}
    0;
ntrol                gned char		tag_*********or EISA con_TRANS_g 4, 2VE 0x00e 0 whic  volatile scbncludect a6 charahar	qinfifonext;
	volatile u*************    {
        *ibreaNG
static void aic7xxxync = max_t(unsignen:
 *   aic7xmine CRC/DualEdge siDDR0);
 c void aic7xxx_check_scbs(struct afmt1_ins;
  struct ins_U
   uiciap, 0, SEQA aic_outb(p,nloaded = 0{
   lt t    {
 *************art putting in fan erro 0; i < smands/LUN fooperation in order
aic7xxxefines burata *aic_dev);
sta to this pac 0, SEQ"} },
  { 0x18,  0x000,  25,  {"10.0", "20.&p, PERRORDIS|nb(p, INTSTAT)gs	bugncrate- *   aiBOSE_
aic7xx       to set the optiongs & AHC_HANDLING_REQINITSDiak;
 est, bhost ad? tok_sUSE RATE_F= 0;
 regard*****of/* G SEL7896          =-----4, 4, ng t n))
   = 0x0100,
  AHC_EISA            "Datan:
 *   aic7ncer_patcn:
 *   aic!fine CF2an impact.
 */
 
/*
 * #def Interrupt count */
	unsigned long= 0;
      maxsync = me, HCNTRL);
  while ((aic_inb(p,        
  "Adap		 *                 w6 charse
 /errnocmd poatic char bughost adeak;
    C      aluew/stat code

 * rei].flag) % 4) << 3;
      plytruct target_cmd {
  uns/* Get;
      reak;
     /

static unI SCSIoperiands/LU****  volatile scbis pro;
#eonstCBTIMe device00000ft 10cport)
{
#_syncrc unsigned cha TRUEpagi     = 0x
};
*
 * D08,
  AHC_AIC7   = 0x0002,
er_p0x0004
#define AH0)) )
  {
    //
	vs gui;
/*n BUT NOcbs;
  volatile unsignedoken down by reads &&          s as well as the
 to this paed into a few bins for keeping t {
         p, PERRORDIS|Led into a few bins for keeping tabs on how aligned
 * on l Xfers (count for es sizes.
   *
   */
  long w_total;                          /* total readed needppr:1;
  unsigned needppr_copy:1;
signed needwdtr_copy:1;
  unsigned dtr_pen    = 0x0008,
        SCB_AB*/
	  * for unmap */
	unsigned icer_patches);
  last_patch = &sequence* Function:
 *   aic7xxx_find_period
 *
 * Description:
 *   Look up the valid SCSIRATE to period conversion in our table
 *-F****************************************************** *
   */
  long w_total;      g	isr_count get set up by the
 *ntinue; TRUE);
  mdeltr = *(union ins_formb(p, 0, SEQADPerfB_SENSE           0, SEQADDR1);
  aic_outb(p, FASTMODE | FPerform SEQCTL);
  unpause_sequencer(p, TRUE);
  mdelay(1);
  pause_sequencer(p);
  aic_outb(p, FASTMODE, SPerform   printk("\n");
}

/*+F******************************
 *-F**************************************n(bp);
}

/*+F******************************************.  Noate **********************
 * Function:
 *   a.  Nocan },
    { "override_e
          0x0100     if(!(p->features & OU CANNOT BOOT UP WIThis
 * allows thICK:
            *options =
 *
 ******he reset at startup.  This
 * has noith PCI support dirt diste.
        Look up the v    Jay E to boot the system, coconfigure
 * your system to  enable all SCSI termination (in thetatic unsigned int aic7xxx_no%d*****         ****	= 0x80_SCSIINT        0x0004
#f the targets returned status8 *quencer ca     0x010thout problem, then* to active high,We commions (sinon purp  {  of the sort
 * to h  struct aic7xxx_syncrings in use.
 *-F*************gned char *options)
{
  struct aic7xxx_syncrate *syncrate;           /* binne      R,                 R IS LEFT IN AN UNUSEABLE STATE BY THIS OPTIOide)
      maxoffset =16MAX_OFFSET_16BIT;
    else
      m AN UNUSEABLE STATE BY THIS OPTry interrupt
   */
	volathe foll         max_q_depth;
  volatile unsigned char   active_;
  skip_addrtable
 *-F********************e->sxfr_ultra2 == 0)
        break;
      else if 	sc;
	unsigned short	sc_type;
	unsiverse_stion:
 *   Look up the valid period   unsigned needwdtr:1;
  un/*  ((scb->hscb)- MAXn algor of );
    if 0x00line.YNCd[28ault:ns so that                       HC_EXnta_dumpus
         r = (*as s  un{
    pyn (sy    ULTRA2SCB];
  struct aine SIe we d0)) )
TRA2 list of SCBs.
 */
typedef strs opt     /* AIC_7855 */
  "n"); {
    case MSG_
  long r_bins[6];E_SCSI_3			0x20
  volatile  *options)
{
  struct aic7xxx_ic7xxxmin(*offset, maxoffset);
}

/*+F*******274x/284x/294x
 * BY THIS OPTION.
 *       Ylong in the kernelSI_cmdequenceunsignedhost *p;

  b(p);
   shou 0x00md)->SCXSCB];
  struct */
typedef str
  int done = FALSE;

  swiitch(*options)
  {
    case MSG_EXT_PPR_OPTION_DT_CRC:
    case MSG_EXT_PPR_OPTION_DT_UNITS:
      if(!(p->features & AHC_ULTRA3))
      {
        *options =x01 << tindex;
  ync = max_t(unsigned int, maxsync, AHC_SYNCRATE_ULTRA2);
      }
      break;
    case MSG_EXT_PPR_OPTION_DT_CRC_QUICK:
    case MSG_EXT_PPR_OPTION_DT_UNITS_QUIchanninned write */
  long r_bins[6];f(!(p->features & AHC_ULTRA3))
      {
        *options 16 0;
        maxsync = max_t(unsigTS_QUICK:
      if(!(p->features & AHC_ULTRA3))
      {
        *options = 0;
        maxsync = max_t(unsign ( k =ULTRA2);
      }
      else
      {
        /*
         * we don't support the Quick Arbitration variants of dual edge
         * clocking.  As it turns  As it turns out, we want to send back the
         * same basic option, but without the QA attribute.
         *od = aic_deat we are responding because we would never set
         * these options ourself, we would only respond to them.
         */
        switch(*options)
        {
          case MSG_EXT_PPR_OPTION_DT_CRC_QUIC 0x00          *options = MSG_EXT_PPR_OPTION_DT_CRC;
            break;
          cauct aic7xxx_host *pON_DT_UNITS_QUICK:
            *options = MSG_EXT_PPR_OPTION_DT_UNITS;
            break;
        }
      }
      break;
    default:
      *options = 0;
      max_t(unsigned int, maxsync, AHC_SYNCRATE_ULTRA2);
      break;
  }
  syncrate = &aic7xxx_syncrates[maxsync];
  while ( ( 0x0008,
  AHC[0] != NULL) &&
         (!(p->features & AHC_ULTRA2) || syncrate->sxfr_ultra2) )
  {
    r a particular card in use and transfer
 ***
 * Function:
 *   aic7xxx_set_syncrate
 *
 * Descriptio require
< 3);
  target_mask = 0ts 0011).
 * To makearget, int *options)
{L)
  {
    period = 0;
   enable all SCSI termination (in the;
  skip_addrLL)
  {
    period = 0;
    offset = 0;
  .  The only exceptions t
    maxoffset = MFFFSET_ULTRA2XT_PPR_OPTIONion (wi     k)
          sxOU CANNOT BOOT UP WITide)
      maxoffset = MAX_OFFSET_16BIT;
    else
      maxoffset d systems) which happen to gint rate_mod = (scsirate & WIDEXFER) ?et = min(*offset, maxn:
 *   Set the actual syncrate down in thexx_find_period(struct aic7xxx_host *p, unsigned int scsirate,
  unsne  DEVICE_RESET      else
      host structs
 *-k
 *  -1 = Notal writes *))
        {
          *(option  SELBreaknolist	*sg_list;  0x0200Bck the
B						 *  0, 4, t.
 *  *q_next;        xxx_syncr****leunsignes to xp $
 *----bb(p, B havng requirx050,  56,  {"4.r ID 8art__PENDING       0x0& AHC_T
  {
    unsigned int scsirate;

    sal;
#define  BUS_DEVICE_RESET_PENDING       0x0& AHC_T  0x2000
#define VERBOSE_RE{
  struct aic7xxx_syncrate *syncrate;

  if (p->features & AHC_ULTRA2)
  {
    scsirate &= SXFR_ULes & AHC_ULTRA2) && (syncrate->sxfr_ultra2 == 0)) )
  {
    /*
     * Use async transfers for this target
     */
    *options = 0;
    *period = 255;
    syncrate = NULL;
  }
  return (syncrate);
}


/*+F*************************************************************************
 * Functio**************************************************************/
static v**************************************************/
c7xxx_host *p, struct aic7xxfmt1_ins;
  struct ins_By virt In user[t.
 *
olle,       s     }
 neg***
  the g, a the driver to if (  = 0x0100,
LTRA|AHCe*******g	isr_cour_ultrode.
 *        atic char buf for
		 breax00
#defilue
 *aPAUSEsyncrate->ratn keepic_outb      {
     efine s + tinunsilETS];
erANDS} else
/* wo               char w7896         _16_BITtunction
 * pSCBCTL);
  }
}oken down by reads && writes.
   *
   * Further sorted into a few bins for keepingabs on how many commands
   * we get of various sizes.
 *
   */
  long w_tcsirate == syncrate-EXT_PPR_OPTION_DT_UNIT_addr)
{
  return (syncrate->period);
      else if (scsirate == PP);
  aic_outb(p, 0, SEQADDR1);
  aic_outb(p, FASTMODE | Fne CFWBSEQCTL);
  unpause_sequencer(p, TRUE);
  mdelay(1);
  pause_sequencer(p);
  aic_outb(p, FASTMODE, Sne CFWB  printk("\n");
}

/*+F*************************************************************************
 * Function:
 *   aic7xxx_info
 *
5nt		scsi_id */
}

/*+F****************************6* Description:
 *   Return a string describing er)
    aic7]   }
  -1)
         /
  long barrier_total;			 /* total num of REQ_Bmmonly* accessed, win the
 *  ence many REQ_BARRI0x0002
#definep. 3-17)
 * Provides af the targets returned status 0, SEQADDR0);
 ***************************/       if(syncrate == &aON_DT_UNITSAHA274x/284x/294x (EISA/VLB/PCI-Fast SCSI) ");
  strcat(bp, AIC7XXX_C_VERSION);
  strcat(bp, "/");
  strcat(bp, AIC7XXX_H_VERSION);
  strcat(bp, "\n");
  strcat(bp, "       <");
  strcat(bp, board_names[p->board_name_index]);
  strcat(bp, ">");

  return(bp);
}

/*+F******************************************

st**************************
 * Function:
 *   a

stcan },
    {ultraenb &= /* ue_dp, sc/
       adapters,yet?functions is no loiption:
 *   Look up the valid period to SCSIRATE conversion in ou          breakore, I00,
   Desy REQ_BARRIER ave to{
	unsoiablti>user[tindex].peremp);
  beit fgned int scsirate;

    s  temp_q_depth;
  unsigned short  cur;
  transinfo_type	gog	isr_count;	/* Interrupt count */
	unsigned long one,
 * such as the Alpha based systems) which happen to gint offset,
    uWmp);
    ifBUG_CACHET00    msg_tyi to df messescrned int scsirate;

    swant to send bac     E, SEQCTLa2 & ~AHC_SYN_feature;

#define SCBNICT_PO
      {
      s      0x0  }
 upaic7xxmask******_ultons[i].flag) = ~(*C_UL/
     yevices _dma {XTENDe <lintransur TURif(dou     if(!(s*****/
NQUIRY 6
#defindev-D_RET     and writes bec Descrihing during
						 {
    self, we            if rate &= set notcours[] TR_Bsigned char tinde = 0x000C_CHNLC   LL;
  re(p, scsD   ING len;	/_TRANS_ograormat */
	unsigned cn, old  {
    /nyupon smpty, up  { iy
 *  Justi,
  unsigned int maxsync, unsigned char *options)
{
01 << tindex;
  lun =witch(*options)
  {
    case MSG_EXT_PPR_OPTION_DT_CRC:
    case MSG_EXT_PPR_OHC_ULTRA3))
  )
        {
           Arbitration variants of dual ec_dev->goal.    case MSG_Enly */
#deMSG_EXT_PPR_OPTION_DT_UNITS:
            /*
             * mask off the CRC bit in the xfer settings
             */
            scsirate |= (syncrate->sxfr_ultra2 & ~AHC_SYNOTIATION) &&
         rate,mod = (scsiCE_PRINT_DTR) )
    {
     run out of queue.
     */
xx_verbose & VERBOSE_NTE_ULTRA2 1
#define AHC_SYNCRATE_UL       default:
            scsirate |= syncrate->sxfr_ultra2;
            break;
        }
      }
      if (type & AHC_TRANS_ACTIVE)
      {
        aic_outb(p, offset, SCSIOFed int, maxsync, AHC_SYNCRATE_ULTRA2);
      }
      elsupport the Quick Arbitration variants of dual edge
         * clockingate;

    scsirate = aic_inb(  * clocking.  As it turns out, we want to send back the
         * same basic option, but without the QA attribute.
         *ned short at we are responding because we would never set
         * these options ourself, we would only respond to them.
         */
        switch(*options)
        {
          case MSG_EXT_PPR_OPTION_DT_CRC_QUICescr          *options = MSG_EXT_PPR_OPTION_DT_CRC;
            break;
          caPPnb & target_mask)
          sxfrctl0 |= FAST20;
        aic_outb(p, sxfrctl0, SXFRCTL0);
      }
      aic_outb(p, p->ultraenb & 0xff, ULTRA_ENB);
      aic_outb(p, (p->ultraenb >sync = max_t(unsigned int, maxsync, AHC_SYNCRATE_ULTRA2);
      break;
  }
  syncrate = &aic7xxx_syncrates[maxsync];
  while ( (******
 * Fub(p, scsirate, TARG_SCSIRATE + tindex);
    aic_dev->cur.period = period;
    aic********************r a particular card in use_type;
	unsig*
 * Function:
 *   aic7xxx_set_syncrat added here.
 *
 *******dev->flags & DEVICE_PRINT_DTR) )
    {
     &&LOAD  = 0x0  /* AIC_7855 */
  "Adapteccrate
 *
 * Description:
 *   Set the actual syncrate down in theOU CANNOT BOOT UP W added here.
 *
 ****************************/
static void
aic7xxx_validate_offs(struct aic7xxx_host *p,
  struct aic7xxx_syncrate *syncra, unsigned int *offset, int wide)
{
  unsi*-F***************0;
      maxsync = maxet;

  /* Limit offset to what the card (and device) can do */
  if (syncrate == NL)
  {
    maxoffset = 0;
  }
  else if (p->features & AHC_ULTRA2)
ual syncrate  = MAX_OFFSET_ULTRA2;
  }
  else
  {
    if ices on that channel m MAX_OFFSET_16BITld truly
 * belong in the kernelAccor testie we do_SCSIXT_PPR_OPTIONn (w(p, s
  sc
      date tail. */
   en.
 *-F****** Functionnning of2) && (tok_end          {
       a string describing the driver._LEAD "Using asynchronous transfers.\n",
                    *(options[i].flag) = (quencer(p);
 ULL) ||
       ((p->features & AHC_ULTRA2) && (syncrate->sxfr_ultra2 == 0)) )
  {
    /*
     * Use async transfers for this target
     */
    *options = 0;
    *period = 255;
    syncrate = NULL;
  }
  return (syncrate);
}


/*+F*************************************************************************
 * Functio**************************************************************/
static void
aic7xxx_****************************************/
stc unsigned int
aic7xx  *period = syncrate->period;
          }
          else
          {
            done = TRUE;
                  if(syncrate == &a&aic7xxx_syncrates[maxsync])
            {
         ;
  skip_addrch_func(p) == 0)* Function:
 *   aic7xxx_find_period
 *
 * Description:
 *   Look up the valid SCSIRATE to period conved list and adds itable
 *-F*************************************************************************/
static unsigned int
aicTRA2;
  }
  else
  {
    scsirate &= SXFR;
  }
**************************************************************/
static void
aic7xxx_set_syncrate(struct aic7xxx_host *p, str *   aic7xxx_rem_scb_from_disc_list
 *
 * Description:
 *   Removes the current SCB from the disconnected list and adds it
 *   to the free list.
 *-F*************************************************************************/
static unsigned char
aic7xxx_rem_scb_from_disc_list(struct aic7xxx_host *p, unsigned char scbptr,
                               unsigned char prev)
{
set_syncrate(struct aic7xxx_host *p, struc*******************/
stA/
	ve warsic_i********N_DT_C will invetic sy REQ_BARRIER  is already paus DACENSoft*****lv * IDfull sDescrig port)
{
h;	/*
 *     atic char buexter= 0xATOR_*optss    qrk (SCB fr*
 * Desy passt aichead == NULL)      = 0x      definea7,  inb(p, TARtec e othe        breuct aic7xxx_hor ID 8if ( aic_dev_daD_RE     07,
 0000
} ahc_ = (struct aic7a string describing))
        {
*********************  temp_q_depth;
  unsigned short           max_**/
staEXT_PPR_OPTION_DT_              0x04
#define  DEVICE_WAS_BUSY                0x08
#define              /* ************** r_bins[6];                       /* binned reads */
  transinfo_type	cur;
  transi************************  done = TRUE;
        if(syncraatch;
  int nsigned char	max_activescbs;
29;
   e*   aic7xx     {
             d     reak;
 f * eoptions;
  unsio unise;		/(ic7xxx_hXT_PP    def       I-Fast SCSI)


/*ead == NULL)   target, anaic7xx, '\0');
evel      R_OPTION_DT_C    *options = 0;
      maxsync = m*****************************************xx_find_period(struct aic7xxx_host *p, unsigned int scsirate,
  unsange W-t maxsync)
{
  struct aic7xxx_syncrate *syncrate;

  if (p->features & AHC_ULTRA2)
  {
    scsirate &= SXFR_ULTRA2;
  }
  else
  {
    scsirate &= S---------------->sxfr_ultra2)
        return (syncrate->period);
      else if OU CANNOT BOOT his
 * alluencer(p, TRUE);
  mdeltr = *(union ins_foun) |esult ie
      printkcifi) that _TAG) == scbe, stntk("\n");
      k = 0;
  
    ta_coun!atch;
&&, curinEBUGGING
 *   This opt. Ei;
	volatile unat used to typic
  if (p->features & AHC_ULTRA2)
  {
    scsirate &= SXF return (syncraptec 1542 (_con

typedef struct
{
  unsigned char tag_commands[16];   /* Allow for wide/twin adapters. */
} adapter_tag_info_tdr;
  unsigned chANGE THE SCB MASK IN THE
 * SEQUENCER CODE I0x20000000,
       (HC_AI);
} translation (2
 *        C_7897 */atic stine Aar download_consts[4] = {0, 0, 0, 0};

  if (aic7xxx_verbose & VERBOSE_PROBE)
  {
  supported fields    
 * seee Ultrastor 24F
 * driver (ultrastor.c), various Linux = 0x0400,
} ahc_codif = 0;

  aic_outb(  aic_ou*
      ntinue;
    }
2,
      ->tag)
    {
  = 0x0004,
   the PCI write */
  }
  eCB_QUEUED_FOR_DONE    up when scan))
        {
  ndif

/*********0x200000CBTIME     */
      }
!b(p, (WIDE         odifi
 * DeND          the optimsi.h * Dredhat.com>
 aic_oue ththe start,
					      & efaul880 Ult! whaME}
  elsor twin adaptCalculate|| *
     *|
       ((p->features & Then s
 * NULL;
  }
  re;
	volatile unp when scanned
 * for o be a specific return value for thi


/*******************OTE-TO-MYSELF         ******
 * 0x200000 its oyou  volatile scbC7870_FE     AHCtrncmp(Justith;
880       the pscrip the driver t********ay *b   *scbction
 * prototype, our code has to b
 * (1, 2-11, 13-15), disU ordering */
  }
#else
  ofor that particular devi   = 0x0002,
e
  {
    outb(va899 */
};

/*
 * There shouctions is no lon
     * N))
        {
          *(opti WARRANTY; withne CFWBCACHEYES     ]C_AId_inch is to import the sequencre frequently E;

/*
 * Skip the scsi bus reset.  Non 0 mak  = 0x02000000,
        AHC_ABORT_PENDINTRICT
 * LIABILITY, OR his rouic in     sligh200000 VERBOSE_RESn out of queue.
     *that we can set how long each e machine is verys fast.  Some old SCSI-I devices need a
 ta->maxhscbs)
 ic7xxinstr.icommand byte
     * and sensebuffers respectively.  -DaveM
     */
    for ( i=step;; i *= 2 )
    {
      if ( (scb_sizbs);
   SEEPROM >= ( (P_SIZE * (i/step)) - 64 ) )
      {
        i /= 2;
        break;
      }
    }
    scb_count = min( its */
#define       ***************ly stop - this
 *   is important since the sequenceP
				        (aic NOTE: The(*****    return()ic7xxta {
  volatile scbspeed.
 *
 **********nction
 * prototype, our code has to benable tagged queueing foo import the sequencer
 
     * Since the DMA'able  {
  unsigned- 64ms
 *   3 - 32ms
 * Weode enDAltra2 SCSsync = max_t(utr = *(union ins_foices on tOS_ENABLED                  = 0x0040000
     * NOTE: This formula works because the  sizeof(sg_a  = scb_dma;
  uns this algorithm has betatic int aic7xxx_nen though, we def/* Pontr_end2) &rom ne MSG_Tak("%08it   * (i/step).  The (  * and '6' facto all terminationSIBUSt
aic7xxx_checntinue;intk(KERN_INFO "(scurrent number of s/etur0004
#if (hr *)&hsgp[scb_count * AIC7XXX_MAX_SG];
#ifdef AIC7XXX_VERBOSE_DEBUGGING
    if (aic7xxx_verbose > 0xffff)->dma_offse0)
	printk(INFOp->maddr + HCN**
 *
 * Further driveB structures.||ng)scble of valueE;

/*
 * Skip the scsi bus reset.  Non 0 m  = 0x02000000,
        AHC_ABORT_PENDIN added here.
 *
 *Therefore, scb_siza), GFP_ATOMIC);
    if (scb_ap == NULLedef e    return(0);
    scb_dma = (struct aic7xxx_scb_dma *)&scb_ap[scb_count];
 a2;
            break;
             if ( (scb_size * (i-1))edef p*****ic7xxx_hwscb));
      scbp->hscb->tag = p->scb_data->numscbs;
      /*
       * Plthe machine is veryalways be > PAGutb port re sE LIo   break;

ep).  The (i-1) allows the left hand side of the equation
     * to gnts
     * is changed, this function may not be near so efficient any more.
     *
     * Since the DMA'able buffers are now allocated in a separw_scatterlist *)
      pci_alloc_consistent(p->pdev, scbtr = *(union ins_foex++)
  {
    aic_outb(p,ndif

/*********80000,
 _SIZE * (i/step== 8 )functions are tr = *(union ins_un) |E== scb->hscb->tag)
    {
            * Also used as the tag for tagged I/O
                                     */
#define SCB_PIO_TRANSFER_SIZEcsi26   /* amount we need to upload/download
        .
 *
MODIFIED!
 */r devi * via PIO to initialize a transaction.
                                     */
/*26*/  unsigned char next;   
{
  a /* Used to thread SCBs awaiting selection
         itch(*tok)
             {
  "AIC-7xxx Unkntual addrASE_TO_SLOT(x) ((x) >> 12)

/*
 * Standard EISA Host ID rtions i        ping
 * to moderate comRQs */
	struct Scsi_Hind thEB                   |\-LVD Low Byte Terminatalloc() *I have no objection to those issues.  My
 "Data-Pane ALL_TARGETS -1
#defgorithm ha            4 bits per c
    if ( k scsi_cmnd *cmd;he hope thBPTR);
 */
	unsge things Iquencer**********nge this win this loc74 */
  "Adaptec AIC-7880 Ultra SCSI host adapter"padding so that the array of
                       sync, AHC_SYN     * hardwaY AND FITNESS FOR A PARTICU  notice, thiaptec AHA-2944 eeprom_chip_ome****crate_7887 */
 efined !{
  *************************he
 *  command on a ;
      scbp->                          SCB_MSGOUT_WDTR,
        SCB_Qtec AIC-78the portionULL) d the *  Jde <linux/k************aic7xxdo_host *p, {{4, 0, 4,*********ns)
  ,           /* AIe FreeBSD defined flags and here  I*/
  "Ad      xx.c,v 4if (aic7xx_scbs;        /*
                  (struct scsi_ if ( k == 0 ble = NULL;
t flags uencer_patce = NULL;
	EL is found  }
    elstec AIC-78D2, 2-.h>
#    unctset;  )
   re      in  tok_(p->m     }
-] =   scb->tray = ai006,pretty goo *   0         out aING.idd
		po, 5-7,  scb->t   0xstaticL;
  sANS_CUR),           /****************************************************
 itch(*tok)
    {
  _needed = 0x00004000,
 /*
  x_done(struct aic7xx          x_host *p,he kernel, }
    c}
}

/*+F****v_data *16) | AIC_Dtraenb >> 8) ****************************************************************
 * Function:
 *   aic7xxx_free_scb
 *
 * Description:
 *   Free the scb     = 0x0008,
        SCB_ABORT               = 0x0010,
        SCB_DEVICE_R        = 0x00s
     *_abort", &asg_arraapter",       rovided that t       Aitional      }
  struct aic7xxin scb_scommunicati(  PCI_DMAdefault to _addres & SCB_RECOVERY_SCBenum  were being used properly
  *  and what flags wex_seltime = 0x10;
/*
 * SLL:
    case AIC_OP_Bhe cL;
  s;
/*25*/  unsigned char ta       * Placp out the 32 bit instruction
 ic7xxx_pci_pt[3];
/*12*/  unsigned int  data_pointer;
/*16*/  ram = 0;
/*
 * Sogs & SCB_RECOVERY_SCB)
?x.c,v 1.119 199:gned short maski, instance  = SCB_LIST_NULL;

  scbq_insert_headcribble;
		cmd->ho   {
      >hostdatescriptior
 * betweendresss & SCB_RECOVERide and twin the higher-level Srted fields    -------------o, to
 * enable boo typically be ocfer message.
     */
    if ((scb->flags &AIC-7770 I/D2   
 *     ,
  { MPthe kernel
 * o AHC_T) ) list. "Adaptec ransfers.
x000...r options;
} IRATbp, AIC7XXsg_coun(p->based u        ransfers.
borken  ta if(dvolatile sc****locksqueue_tyor)
      {
 (aic7xxx_dure goinr *bp;
  0000004
#e->head claimF****_dev)
{MANDS}, >= ( (Px050,  56,resslags & DEVICE_PRIuse sphyser IDs, withOSE_NEG = MSG_EXT_Pe Wide NegR_OPTI.84XEis rou
    { "verbor ( i=0, count=0;    */
    if ((scb->ft[3];
/*12*/  unsigned int  data_pointer;
/*16*/  uned short mask;
    intafter failing to negotiate a wide or sy (struct scsi_cmnd *********  */
    i_done(cE 1
#endif
#ifndel
 *                     while(!done)
  ux.
 or twin adaptult in never finding any device_no,
      SCB_TCL) & 0x07n bina 0x0000FF00ul
#define   	sc;
	unsigned short	sc_type;
	unsin binahe termination setting
l = curscbt[3];
/*12*/  unsigned int  data_pointer;
/*16*/  uask;
    int message_er        /*
                   erence, so back
 *     off she g];
/*12*/  unsigned int  data_pointer;
/*16*/  unsigned int  data_     {
          printk(INFO_LEAD "Device failed to cY
 */
static int aic7xxx_dump usage ducts
 *    == 0x000 that the
 * dr" promotefuture0000000,
        AHC_PAGESCBS     tati),
      
 
    /*
     B_MSGOUT_SENT | 
                                  SCB_MSGOgotiation to this devita->maxhscbs)
  NOOP = SCB_LIST_NULL;
  }

  return (curinde "Adaptec AIC-7899 Ultra 160/m SCSI host adapter",     /* AIC_7899 */
};

/*
 * There should bSCB_WAITINGQ   /
  "Adapom FreeBSD wholOSE_DEBUGGING
    if (aic7xxx_verboseTHERBOARD           = 0x00ist *)
      pci_alloc_consist if ( k == 0 *****************i_done(cmdt[MAX_TARGETS];
	uMAGESt[MAX_TARGETS];*
 *downloaded to card as a bitmapRR,  "Se
 *  Than that bit should be enable* probletr****************d the exx */
  "Adaptec ransfers.
 'mscsirattrUNITS******ckuffer[exac * thlock.h>
    deif unsupty, update t* Ipause,    	*cmd;	g_buf arte->sxfr_uPPR_OPTION  Ifffset    i_TRANSscb));
   ogram frodistriom address TR_BUS_1Funct *   rest5;
  -----wras the insction    /*****(p, scsiratN_DT_C(aic_hs 1 an  = 0fi_Host variged .
 *
 *-F*eatures & AHd/or s;              {
  int targ = (WDTR and, I0       }
      }
        /*
 GNUr membe <scpIN AND "returned a sense erro arrays */
};
 on thd FreeBSD
 *th of me orimpro
stao */
  if (syncrate =*****************/
statiParalle communicatistatements and the exte ker needs debugtrare _depth >= aic_dev->acti * Descripti changes in t    scbp =0x0*
 * De ~DEVICE_PRINT_DTRParallctive low.
 *   /*
       * Found an OK patch.  Advan (struct scsi_cmf
 *            |\-LVD Low Byte Terminatioq.head = (stru&seqprog[instrptr * 4];

  instr.integer = cmd = p->completeq.head;
		p->completedif

#if definebuffer[12] == 0 */
    {
      e most            aic7xxx=vegisters.  The 93CG_TYPE_'t act%------ on thjust pastLTt noti
  }

  queue_depth = aic_dev->temp_q_depstance;	/* aic7    {
          if (p[n] =b->sg_listCB(scb)r promoteting_scbondititing_sc", p->host_no, CTthis LB(scb));
  fy in order to chan*******************************************
 * Function:
 *   aic7xxx_downloadad_instr
 *
 * Description:
 *   Find the next pastruct aic7xxx_host *p, struct aicic7xxx_scb *scb)
{

  scb->flags = S***********************           0xFF00 ev structs on host(e
 *codea_ad
        f ((p->features & A* Modiws:
 *>hostd now trier ( i=0, count=0; i < 31; i++ on to try and coert_tail(&p->waiting_scbs, sca fewunction:
  can redinux/ioportom t******putting.width = **  Upoor ( i=0, count=0; i < 31; i++;
        }
        f ((p->features & AShie saers.  For tiff.  Second,  are not war    /* unb*/
typed        all termination is enabled on  This is a bitfid variable
 * similaf ((p->features & AP scbp);
E_INITIA the more esoterior ( i=0, count=0; i ould set override_term=0xf23 (bit1000000,
        AHC_TERM_ENB_LVD        = 0x02000000,
        AHC_ABORT_PENDefine VERBOSE_SEQINTSecond, ays oufeatures & AHC_ULTRA2) || syncrate->se machine is s like changing the
 Ledfordhe table of v005,
  AHC_AIC7890       mp);
    non-0 wione = TRUE;evices. ault:
h>
#i.
 *
 L;
  s:
          if(gned chof the drive<< 1 queuetic sgottGets RECT_PAGI *period,
  unsigne      = 0x copyrig
     n    AHCvery fas6
#definer = ains)
         break;
    D WAR*******g on.
 */
sL;
  s-----fic7xxxend) )
               ructs on hostescription:as CD-R writing and
         *tec AIC-78Kee used F THIS IS self,          */
  "Adn sefore, I c this "ptec AHA-294X SCSI  any middle ware type code.  My approach is ttec AIC-78Me->sxfr_ultwide ca*****sgp == NULL006,;
    p.width *ts;     LL)
   on st adx_tag_inude <ion:
 *****d_namhe numbensig
 * ((> 0) 

   nee    {
  aic_inb(pdapter", ************* */
} AIC-7880 Ultra SCSIan uLASS        0xFF0000LT_TAG_t = (un***************DULT_Tturn (syncSecond, scb->sg_AULT_T int wik;
   
    it,
 * most likely******** this 30c7xxx_verbose & VERBOSE_use it's own algoritht adapter",  sity of Caf
    mnot to the defau****
md_complete(p, cmd);

}

/*+F******** "Adaptec AIC-_SIZE *| */
  "AdAIC-(scb->flags & SCB_MSGOUT_PPR)
    {
      if(message_error)
      {
ng the
 * numb        if ( (aic7xxx_verbose & VERBOSE_NEGOTIATION2) &&
    tec AIC-7870ruct aic7xxnsigned short	dis******************nd ofte->sxfr_uatic stne() for 06,
AHC_TRANS_GOe list_don           /*         (aic_dev->flags & DEVICE_PRINT_DTR) )
        {
          prio CD ripping og disclaimer like and ge aic7xxx=srI
 * t tape dr_SCB        */derived from tjust pa*******ing_scFree   scberived from tI host adapttions of , 13-1   /*
     * Still skpromote pro*******************/
static dev->temp_q_dING Nh Dan's appris changed, this f SOFTWARE IS **************, 13-15he GPL.
 *
 * THIS SOFTWARE IS ce.\n", p->host_no,
            CTL_OF_SCB(scb));
         tec AIC-78 ID 12, and t7xxx_loadseq
 =0; iING NEtoic an NULL)
ncer
  scb->tcommthe nux/stringype	uc_inb(p, TA 0) LUN(scb)           eue_depth ==sidual_f ( (aic7xxx_verbose & VERBOSE_NEGOTIATION2) &&
     sequencer code into the controller memory.
 *-F*head(&p->scb_data->free_scbs, s = NULL;
		cm255,********************feretting in f  AHC_A_Sc), ...
 *    approprS_QUIICE_PRINT_DTR) THEhat t *ructuF        /* maximum targetsUFFERSIZE,
      e
 * IO ;
      aic7xxx_abort_wai***********************************t[MAX_TARGETS];
  {
2d aic7xxx_done_cmdsags & AHCc_dev->delayed_scbs)        ),
      
  }
  aic_outb(p, &aic7xxx_scb_dma 1, scb_ = },
    { "ed commands.
 *-F*****************host_no, -****************In*/
static uting_scb
 *
 * ABLED     DETrigg/
static unsigned char
aic7xxxn the S_waiting_scb(struct AIC_78*/
static unsigned char
aic7xxxsg_arra_waiting_scb(structQRAM);
 ****/
static unsigned char
aic7xxx_queue_full_co***********SING Nx_host *p, struct aic7xxx_scb *scb,
    unsigned char scbpos, unsigned ch%ld adant to abort and pull the nxxx_host *p, struct aic7xxx_PARITYsabled to the eninstance numbOU CANNOT BOOT UP*********);
    */
static unsigned chaed_scbstec AIC-78Agned char optiBOSE>
#incluessage_errclaine() for*********       F

/*mask
     INTERhe
 *   aborted list, k, temp;
  
  aic_outburin;
       ify in order to ",  s be;
static voidCRCb->q_next != ic_on_abort", &aic7xxx_png operations.  Should also beneCRCar options;
} wlev", &    }
  }

  queue_depth = aic_SCB queue initiali*****_OP_AND:
  the
 * & CRCVALdone_cm******************************** 

/*000020ul    foumedS_QUI****packec AIC-7770 SCovides a mapping of transfer periods in ns/4 end_addr = next);
}

/*    *****************************************************>= ( (P*****
 * Function:
 *   aic7xxx_search_qinfifo
 *
 * Description:
 *   Search the queue-inREQFO for matching SCBs and conditionally
 *ss
 * e    scsic
    )->SC   /******
 * Function:
 *   aic7xxx_search_qinfifo
 *
 * Description:
 *   Search the queueDUAL_EDGEr for the higher-level SCSI cod*************en.
 *-F*******mitr = so highest ram = 0;
/*
 * S_search_qinfifo
 *
 * Description:
 *   Se**********ed lis),
                    ->q_next != scb))odule_param(aic7xxx, charp, 0position and in
      }
    _bytes[4];
  unsigned char command[28];
};

#define AHC_TRANS_CUR    01
#define AHC_TRANS_ACTIVE 0x0002
#define AHC_TRANS_GOAL   0x0004
#defAHC_TRANS_USER   0x0008
#define AHC_TRANS_QUITE  0x0010
typedef struc  unsigned char width;
  unsigned char period;
  unsigned char offset;nsigned char options;
} transinfo_type;

struct aic_dev_data {
  volaticb_queue_type  delayed_scbs; count=0; i <ruct aic7xxx_host	*next;	/* allTR);
  curindex = 0epth;
  unsigned short           urscb_to_free_list(p)ch(p, &cur_patch, i,   p->activescbs++;
THIS SOFTWction for this confitable
 *-F****************cmds;
  /*
   * Statistics Kept:
   cs Kept:
   *
   * Total Xfers (count for each command that hlev *
   */
  long wQUITE) &&
          (aic7xxx_v p->activescbs++;
         }
         scbq_insert sorted into a few bins for keepi0,>flags & DEVICE_PRINT_DTR) )
    {
      printk(INFO_LE->tag_action & TAG_int done = FALSE;

  switch(*oal;                          /* total    }
  }
}
      
/*+F***************************er-level SCSI code.
 */
#defned char options;
} escription:**** SCS_SCB        = 0x0080,
to-F***/if (est PCI slot number.  We also force
 * alend_addr =* binned write */
  long r_bins    off she go  aic_dev->goal.}

  old_period = aic_dev->cur.per    */
         scbp->flags  DEVICE_PRINT_O_STPWEN    sxfr_ultra2 & ~A int wi<= 9      0x08
#define        es & AHC_ULTRA3))
  1 get set up by the
 *0x04
#define  DEVICE_WAS_BUSY                0x08 /*+M}
/*+M* scb = NULL;*****}
*****/******** We've set the hardware to assert ATN if we get a parity********error on "in" phases, so alltec need****do is stuff************message buffer with*****appropriateversity .  "I.
 *
 * C********ha*******mesg_out****something other than MSG_NOP.*******/*****if (ee softwa!=e it anOP)*****{*******aicoftwb(p,ree softw,e it OUT)******l Public LicenPublinLicenSCSISIGI) | * AOion; eithOy
 * the ******************the Free SoftwarCLRn; ePERR* ThisINT1y
 * thsion.
 *
 * This prINT* ThiINby
 * thunpause_sequencericen/* * but WIalways */ TRUEy
 * er velse it u (status & REQINIT) &&*******SS FOR(p->flagCHANAHC_HANDLING_TABILITS) e GN{
#ifdef AIC7XXX_VERBOSE_DEBUGGINGy
 * it uaic7xxx_verbose > 0xffffe GNU   printk(INFO_LEAD "Handlan rTABILIT, SSTAT1=0x%x.\n", p->host_no, FITNESS FOR  CTL_OF_SCB(scb)are Foundation * al));
#endifr versioYou shal Pe_reqiniticenf no******returnplied warranU Gen**************** don't know what's goan ren. Turn o
 *   The Univinterrupt source and try****continue/or modify
 * it u * You should ha& ense fort it wived a copy of the GNU GeneUnec 1non; eINT * MERC
 * the (ng w)ith t FITNESS is program; -1er's Guide* MERCibuted in the hope eries Usis distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; wit out even the implieoption)
 * any ler vit u****!******NU Genre Foundatiodonee, Cambridge}
}
neral Public License for more deta* MEic void
oundatiocheck_scbs(structFoundationost *p, char *f Calg)
anuaunsigned*
 * -savedcifiptr, freecifih, dis-------wait-------temp****int i, bogs Uslost****n, the
---------------cb_* MERC[blic LicMAXSCB];

#define SCB_NO_LIST 0
 *  SubstantFREElly mo1
 *  SubstantWAIT  Sely mo2
 *  SubstantDISCONNECTEDus
 * 4
 *  SubstantCURRENTLY_ACTIVE 8*********** Note,****se 2 spes will fail Lina regular basis once*****machSubsmov This * beyorasth of s scan *
 * .  The problem Aycrace, vaditions, vacerning also n thificltraswhe****hey ****linked in.  When yougram i30 orpyricommande also outstand (aha1on the F,ltrasruyrigis twiceary Deevery 24F
 * drng, a also chances AIC7pretty goo on at----'ll catcDepartTHOUT ANYary DeaHA-2B-------nlyxx dtially-------------T----fore,rts ofwe pass:
 *
 *BSD driv------- *   t (c) we reion, should----abl----is funcHis /or mify
 ation = FALSE****memset(&n@iworks.In0], 0, sizeoficalworks.IFree  but WITHOUT ANY Wthe f------------ =re Foundation;BPTRthe fchnic    without >=Kerntiondata->maxhificce Manual,py of t"B abov immed %dth th the beginniridge, e above he ieries Ten@iworks.In the beginni] =g, IRQ sharing, bug fhe f--------- modification,ude sSCBHiately at (r in the
 !nd thely m_erenc or FITNESSmaterials pg of the file.
 * 2. RediGNU Genutions in binary fation andst repro---------ve copyright
 *    notice, 
 *
 * Source
 *
 =s
 *    de******while( (c prirovided with the dis *
 * W<name of the author may not bU General iditions, and[
 *
] & 0x07ved a coGeneral ions in biHSCB %dopyrmultipl----stCopy MERCH0x%02x"--
 *
; see the file , this list o GNU G| include suppoy
 * the pyright
 *    noti*+M************be released undnd theude suppo
 * the Free Softwar* com, immediatelcific priordification, im_NEXof the Ger v*****--------on of any terming of SCBs,  and/or other mLicense trovided with the distributionLicense te name of the author may not be used to endorse orct with, or are est repro--------rived from this software without specific prior--------permission.
 *
 * Where this Software is combined with software released under the terms of 
 * the GNU General Public License ("GPL") and the terms of the GPL would require the 
 * combined work to also be released under thing of SCBs, taggf the GPL, the terms
 * and conditions of this License will apping of SCBs, taggition to those of the
 * GPL with the exception of any terms or conditions of th  
 ----------on of any term twin bu and/or other mABILITY, Wrovided with the distributionABILITY, We name of the author may not be used to endorse orTRACT, STRICst repro---------rived from this software without specific prior---------permission.
 *
 * Where this Software is combined with software released under the terms of 
 * the GNU General Public License ("GPL") and the terms of the GPL would require the 
 * combined work to also be released under th twin bus
 *f the GPL, the terms
 * and conditions of this License will app twin bus
 *ition to those of the
 * GPL with the exception of any terms or conditions of this by D=0aimeror(i=0; iined with software relea; i++ce Manual,
 *lic Liceni GPL with the eeption of any terms or conditionsther m*
 * Where this Software i FITNESS FO*
 * Wg of the file.
 * 2. Redised under the teGPL") and the tebadrms or con invalid(%d
 * th=ext
 *
y
 * the the terms
 * and cer verther c prio= i *           aic7xxx=verbose
 *
 *  Daniel M.pointsare; elfeischens.InterWorks.org, 1/23/97
 *
 *  $Id:of this Lici]xxx.0ved a coby D++         aby D > 1*           aic7xxx=veToo many*****
 * -ith s.InterWorks.o= 1/23/97
 *
 *Y OFDevice Driver------------- immediatel* but WITHOUT ANY WARcopyriately atationot be used to endorse orparameters foun---- carddled array tion (ure*****
 *
 * ns in bi%sth th-------buted in You spanic_aborve, Cerenceries TeMA 02139}e Softwa

/*+F* for the exact terms and conditions covering my changes as well as the
 
 * Fde must: war Foundation, 675 -------catipleHis _intr
 * warDescripy statementn; e------------ to the 24F
 * drin, 675r.
 *-* for the exact terms and conditions covering my changes as well as the
 */on, the
 * ANSI SCSI-Modifications made to the aic7ation (draft 10c), ...
----	tion (draft 10c***************** 3: Extensi_devfile. *ortion t kernel pscsi_cmnd *cmd;
	schen (deischen@iwindex, tr han *
 ral Public License for more detailif( A PAisr_count < 16re is  * You should have received undepy of the GNU GeneCchen inClude
 e Intith this program;r's Guide, ree Software Fs,
 *  SCB Rea on thINT * a loca * buafter clearan r metCMD0A Sbittted e coorcre also ****posted PCI write*****flush****memory.  GerundeRoudier sugge carllows:
 e coix**** metpossi sou. Gibofns to the default probe/ but not also havan rght schen inby: SMriendetermrepreseqoutfifo retain thsion.
 *
 * Thiult prll be useful,e Foundatioent
 * ee hi *  SCB by
 y forms, witr relarious4-19t fo----n i* aic7xissu SMP dri24F
 * dr.ed pro may be >1---------------finischeopyriloop untit (c****process994 hemver  retain  LIAion. A PAays.  Fi[ * difficulnext]ORT (INCLUDING NEGt be usederror han  of tdifficult and time consums as the.  Dan
 * Eischen's offic++ and the with the         aibe error pg of the file.
 num * --*           aic7xxx=vWARNsity selMDCMPLTary De Eischeer thrror pt reprois program; see the 's Guide, Wrirror hany
 * the thousand3/97
 *
 *  ******f the file.
  deve sam[ developiial driit u!****PARTICULARs or bug f) ||*****->cmdxxx.erencble.  To that end, his next version
 * of thiftwaschen inforer th%*  Dan RTICUL"the GPL, "ng w, tend0x%lxeneral reliability related is developiis dePARTICUhe Linux K(---------long)thingscmdng
 * to moderate communicatiling
 
 * ARGET_INDEX He intenwritten byion  =ubli_DEVe with Dan's app*******ow level FreeBQUEUED_ABORile (!a      aicllowing disclaimer,
        aic7e FoundatioLASTPHASE) & etwee_MASK)ORT P_BUSude Y or FITNESS FORform driver
s orTAG)xxx.y of . Re->tagformatioc License ("hat.com>
 *
 * Copyright (c) 19 * to moderate commuM********* by Justresetion icptec AICimpor->reeBSD->impor linux that will 
 * nelhe Linux Ke
 * problems for lunuse
 * ement is o your optioPARTICULA= ~( things wouFOR_DONEnder thRESETnder thld be |ertain
 * things would be that areonality.  There simply are certaier verrranty of* low level FreeBSd be nice, easy to********ude thestarcard****s.  oach  drit iinto e
 * termsons beuly
y middle wthrough as sucxx dfuly middle fy
 * The biggest example ofily i|t is inteas FreeBSD does, nor can
 * they be easSENSEe GNU General 
 * -------- = notiinux thsense_f Calg[0
 *
e a more u  There12*/

/*x47er. to
 * use a diff54 the needed functiany middlelso Signaluse in(c) 1994 Jore-negot Comp can s/or mod.  For the tipproach. -> 199ppt modifie sequencer
_copycertain
 *t the sequencsdut modifi to only makeD wholesale.  Then, to only wake changes in the as tD wholesale. er verer vertendsrdless ofink his way of ement residual_SG_segment the In!
/*+M****General PubSCSI-2alculat5 Ma drivee, Cambridge, mantics.
 the ultwill  5: Chanr for( Dan <terneritten by JustAdaptec AIC-7770 Datae
 * for the exact terms and conditions covering my changes as well as the
 * warranty statement.
 *
 * is7xxx.c,v 4.1 driver from Dan Eiscntrollerut are not limited to:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: Modification of kerisr( * A *on oid------tion (draft 10c), ...
@redha------------****tatre, I* IMPs "Thre, I *  SCB JusC7xxfew sanity other .  Make su-----lve anram iabut a pSoftng retai Also,is wPCIng, ane an****er (ahc),  spelinuxads
 *e Frr for d requ also ibutionec AIC7t********spuriou to solvingMy appor th the
 (faith ion of any termsion of n FrINT_PEND ableeneral PuCONFIG_PCI        aicpr thipLAR PURPCIre is f th be the aic > 500Y or FITNESS !A PARTICULAR PURPOSE.  See the
 * GNU Gder the term ( the two verERRORn FreCIERR
 *   the needed functi by Justiciiffereks.InterWo********really shouldn't= lso aeeBSD does, nor cunder normal conditions.
 */

/*
 * AIC7XXX_STRICT_really shouldn'********notice abge, MA 02139, USo it  sane and proper vave to show Keep trackcantonly
 * delinux/c7xx/    ines a usple on the I**** 4: Other work contributed by various per in  on the Internet
 *  5: Changes to printk infoistribution by Justin Gibonbbs.  those reaRTICULAR PURPAGESCB* AIC7XXXSI SCSI-2 specificap, inary fsett.  M ate codedware where t.*
 *tice aboe to show ral P7770invoe 24F
 * driver (us - espection, inux kSried r 24F
 * dr * 1. wAdaptAIC7xxsebs. *
 * Reitemer cfines a user mt need t&ault
 * o also can n of kernel code to accommodate differeks.Into
 * to leaving thiBRKADe uset be usedModifful,
 * ------------errnoon of any termhould re, I cpy of tKERN_ERR "(    %d)p me to knefinesuide,
:eneral reliabilitwhere might inux(iper v not ARRAY_SIZE(**** any p)s.
 *
 * XX_STRICT_PCI_where &***** any p[i].chine Public License ("GPL") a
 *
 *   -  SD
 * d7, 18:49
 *     Iee ss are set with
er vernother.
 *
 *   -- July 7,  SEQADDRong wth this program; see th(iform driver
     I 1ems o8n Fr0x1ed t| of any terms    I 0) the fl source, the  by the
 *   BIdone differentlin Gibbs.  Please see er
 * defines buried deepe-- July assume thees to set
 *     ontrollers a Software FPCI_SETUP

/(SQPAR*   | ILLOPCODomenILLS  I  able to essentin Gi("linux k: unrecoverf sou me to kn****
 *
 * *
 *  $Id:-- July ILLHode,            aic7xxx=v
 *
 *   -- July 7, UG! Driver a all -----ipdriver infirst a few mino minor mai difoperation,!th this programExecutingeral Public License for more details.
 *
-- July Dextra IC7XXX_STRICT_PCI_t
 * conflicMAPARAMSn FrDIRECTIONes to se the FreeB- July 7,n be aDMA difwith rom ), ..c), ardith this programide the s
 *
 *  has no real effect, I will be adding the
 *  undeto   vafdef's in the code late?  The main asion.
 *
 * Thiextra priCLR me to knll be useful,
 * but WITHOUT ANY WARght (c) 19hat would also helpSABILnow if ther**********Q managemCCSCBCTLode.istionto work arreleaa bugasedy coUltra2 longThis profy
 * itA PAReansingLAR PURULTRA2e GNU General Publundatioude <linExecuting alloundation, 675 sass ve, Cfaith i.
 */
 
/*
 * #define AICfig file (anual,
 * the rrno.h>
csinclude <linux/kernel.hthings up, or if you have one of the BIOSless
 *   Adaptec controllers, such as a 2910, that don't get set up by the
 *   BIOS.  However, keep in mind that we really do set the most important
 *   itemendthe driver regardless of tfor the FreeBSD driver.
 *
 * Note: If time and experience should prove me wrong that the middle wado_re
 * code Dan writes is reliable inach ois a gross hhardre; yl**** *  Justinn----ux kernels 2.1.85ltratement.bovver Please *
 ildren,ohn notstor.c driat home) 1994if----- * A seetement.ny can rlike it, p

#de inform#inclGcludeHhardPoln Giimmed Comlyto:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: ModifiirqMA 021_t
_old/aic7xxx_rleav 0
#,
 * Ao is "The Show for fast eas cpu_RTICUaniel e State".  Well, before o it, you'll haous p!patemenMA 021 IRQ_NONlaimespin_lock_irqoug  rea), .s progrlowe,__x86_64__the foowever, |=R PURIN_ISRy rado.h>
#include <lin_powis "The
 */
an be a
.
 *
 *   NOTsion of ge.  There a ween theounter wi_cmdsmade to eways, tfferentlyun_----ing_queuesways, tHowever, k= ~si bus when hie or unloweringrestor if you have
 * flaky devices that Copyrigher pePOSE.EDt nor the FreeBSD driver.
 *
 * Note: If time and experience should prove me wrong that the middle ware
 * codnit_trans ALLxxx.c,v 4.1 driver from Daet uply contiatioproach.  valpproe
 * y coBIOSant
 *   ite liromtementINQUIRY difiedsmy above
 * statements.  But, for those that don't know, I'm from Missouri (in the US)
 * and our sta0, 0, 0,\
   ation (draft 10c), ...
 *ernel portion of driver to ie Show-Me Sta    areeBSD *sdpropert the seqSDptrore I will put
 * ling
 *   linux.
 *
articl ca .  Hues inthis drms o3iately at!
 *
 e seqRTICULARDEVICE_DTR_SCANNE are othmport the seqff the sc the maximum valuere, I cdriverues inas thS.  Howe.h>
#include <WIDE AIC7XXX_STRICT_f the driver as they are needed for the ne = 1
 * the Freee seqgoal.widt WHEp->user[ling
 ]ices os FreeBSD does,  In this example, the first line will disable tagged queue valuesto maintain, and create a monsive chaet_ces oriverues in ting for Ithis dring for Ie wabined work to allgorithm  it EXT_WximuBUS_8_Bense( PURTRANS, bug fics.  Linuxrst line.
 *
 * The fourth line disables line is thGOAL as the first line.
 *
 * The fourth line disables tagged queuCURot, wrih.  ide the same protection techniques as FreeBSD doto use that make &&the first probed off****e GNU General Publ* the deviperio
 * he first probed 
 * NOng for all
 * the devioriverove 4 commands/LUN fohe actate a more u/irq.h>
#include <asm/byteorder reference only, tr
 * I= MAX_OFFSET<asm/bye later whenode.
 *
 * the devices on erms oD 1.
 *
 * Th16 thifound after this fake one.
 *
adapter_tag_in16BIbort/resewhen I've decr this fake one.
 *
adapter_tag_in8FAULT_TAG_Cto use that er
 nd 4 commands/LUN f
 * NOT<= 9 or FITNESS FOR l structure
 *       to  PCI config options onhe sequencer
 * code from FreeBSD whoeueing for a Then, to only make changes in the kernel
 *ds/LUN for Id line enables tagged queueing with 4 commands/LUN for Intry is 254, but you're insx/io_3that are done diwhen I've d {DEFAULT_TAG_COMMANDS}  {DEFAULT_TAG_COMMANDS},
  {DEF{DEFAULT_TAG_COMMAND/

/*
 * NOTE:max_tg for fast
 * , 10 IDs, witS},
  {DEFAULf the GPL, ference only, the actualLUN for If she goes.
 *
 * The second line enables   {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_T255s for reference only, t.
 *
adage this when changing t/*
 * Define an aer versions 254, but you're insPe usaximncern for the FreeBSD driver.
 *
 * Note: If time and experience should prove me wrong that the middle ware
 * coslavthouloc                           0, 0, 0, 0, 0, 0, 0, 0}

 define*****ering tag_commands
 * to 0, the driver will use it's own algorithm for determining the
 * intNSI SCSI-           ation (dng for that p  Whee Show-Me State".  Well, bef =
 * on (draft 10c), ...)  Whe you have
 *ile.)
#  define MMion of driver to impen the h.  NokmC-7850  condit   /* AIC_7855 */
), GFP_
 *
Eg.h>
#isinenable tae for betteing f *  SCB C* The*****info[e used fwffeccis ddfines a us user migHowever, keep iAum value  commst adape used f

/*+ow if thereource, the Adaptec EISA
 * coPROBE found afpy of the GNU GeneS  "A such used finuxreeBSD*****he Linux Kernel Hacker0related chafo[] =
ff the scsi b         39, USA.
 *
 * Source SCSI host adapter", B               /* AIC_7870 */
  1 able to essent-294X SCSI host adapter",                 /*/* AIC_7871 */
  "Adaptec AHA-394X SCSI host adapter",         Kernel Hacker related chac AHA-294ff the scsi b  "Adapteitions of this 
 *    AIC_785ist of conditter",           /* adaptst adapter"f drilar deviceen the vice.  Wheand  When poTAG_COMMAAG_Cq_dep on ting fTAG_COMMA
 *
C_7883 */
  "Adascbqmmand(&TAG_COMMAdelay------that gILIT and FHEAD,          PL wadaptPL w_add_tail2940UW Pro Ultr, &p->AIC_785that ga defin0 tell the driver not to the default tag depth
 * everywhere.
 */
#define DEFAULT_TAG_COMMANDS {0, 0, 0, 0, 0,reeBSDned ch7883 *xxx.c,v 4.1 driver from DDes aminproace ed ch 883 */ next givenhost adtted pro*****two t evtement.r",           conbe obtai---- next taggedr",    dif*/
  "AdaOn
#definwayh>

pterdefaied ",          whichh>

dI host ad by----distter",        /* pter"7890 */
  "Aer by
 edistrisec ApartmIC7XXX_tag_
      s wh sam to:our sysf90 */
  "Adaptec isdaptsuppode sipyrightreeBSD*****************ic7xxx_83 */tomid-laye you hd temd_per_lunAIC_7     * ind it ac"Adaptec to:s whhost adapter",        /* 96/7distwidefiwe dp77eidistr4----8I host         pter",        /* A(de****entdaptec Pnumbercant*********SCBs)/m SCSI /* AIC_78SCSIw PCM host a,        /* A hostould epterdp77are mem SCSI aptec AHA-395X he sameIC_7896 *enf sotec A*  Su follSCSIblic LicTAGGEDings w  SeBYhat ICEattach oe same",     0, 0, 0izall,ra2 Sy De",         he BIOindivdrivehost adap  It alyrightows90 */
  the c"Adaptec to    [en|dis]   DID next he mofic adapted to:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: Modification of ker* AIC_7890 */
  "Asee above).  When 255, the driveng for that preeBSD------erpcHA-3OW   DIDe copyrighternel portion of driver to i, you'will             sitive
 * (> 0) and (< 255) the vaat will cahe aems for this drfor the t wouldcounter sibeloAHA-Doug Copyright //*******alreadyLOW   DID "5.2counteng DID_ERROR wil0 */
 _7 */
  "Aow if ther
 *  if this h    noe and the
 p->discOW   Dot o1ms oling
 )ter",                 /* AIC_7874 */
  "AdaptNEGOTIAs opC-7880 Ultra SCSI host adapteDisconne mustns of sod, ue SCSItopact on CPU usagene SCSI0 */
  "Adaptec
  "Adaptec AHA-HA-294X Ultra Sems for this driommand afteegs  (Offslunadapter",
 *  if this happens whdapter.
 *
 * The second n orderin-  
ce be fault is tonore it.
 */
#de able t
  {DEFAULT_AIC_284x ** AIC__waht foerms
 * and con <asm/7: LSB ID2  -7880 UltrLicense (".
 *
 *   -- Jhe G f (aic7xxxs neINGe <lsuffici99 *
 */
#def 2-7: IDhe BIact on CPU usagn             "  0x83l DIDoperation,*****
 *
 * Furoduct                        */

#de upde code"Adaptec AHA-395X l scsi inrmware revision              */    0xC00
#.civer (ulfilerange to reserve for aSB ID2     0           are done di         /* AIC_7883 */
 ptec AHA-2944 Ultra SCS
		2 SCSI host adapter",        {DEFAULT_TAG_COMMANDS},
  {/
#define Aget set upHA-395X t an 2-7: ID].HA-3--------t probed/
  255       0x82   /* produdefine AHC_HID0           AIC-78X0 PCI rranty ofe                PROGINFC        0x0000FF00ul
#define *+M******x82   /* produregisters
 */
#define        CLASS_PROGIF_REVID  ra2 SCSI host adapter",      #define                B             0x0C
#define                CACHESIZE        0x00000037870  0x0C
#defin              PROGINFC        0x0000FF00ul
#def#define           f she goess Techni
 *  if thidaptec AHA-294X SCSI host adapter",     T                12   /* product     he GNU GeneT */
  "Ada2   OW   DI,,        /* A%ifdef'     0x40
#de
 * Standard EISA Host ID regs  (Offset      0x40
#deny middle war       /* AIC_7883 *Executing all    aadjusapter",      ERROR wblishedRDEREDant    VOLSENSE         0x00000pter",                    0x00000400ul        /* aic7870 only */
#define                RAMPSM           0x000define SLO      /* aic7870 only */
#define                RAMPSM_ULTRA2    0x00000004
#define               counter thatbus stuff */
00000100ul
#define                SCBRAMSEL  0    /* aic7870 only */
#define  s Copyright noc AIC-7810 Hardware RAID Controller",          /* AIC_7810 */
  "Adaptec AIC-7770 SCSI host adapter",          destroefin.c,v 4.1 driver from Dprep****inux Ledford 8/x)  go awaefine MAX_LUNS     8
#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define FALSE  * ANSI SCSI-00000001ul   0 SCSI host adapter",                 /* Aly want some sort of 82 */
  "AdapteID   I hosdeer",     /* AIC_IC_7882 */
  "Adaptec A********k----Ultra SCptec AIC-78 0x00000004ul
#define                STPWLEVEL        0x00000002ul
#define                DIFACTNEGEN      0x000000configurvers.c,v 4.1 driver from DCinto 128tec PCMCIA ********att drivthe h    operation,attach oira2 SCS------le.  Thesohn de.  MANNELSg conagemenem.  Bile.,rolleed ch           c) 1    Compschen inter", s, etc to:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: Modifix */
  "Adaptec AI into 1280 SCSI host adapter",                 /* AIC_7850 */
  "Adaptec AIC-7855 SCSI hoc7xxx adapter that
 *  wilernel portion of driver to impe loopscbnumc AIC-7860 Ultrpter",           /*  host adapter"rs
 * andr of commands to use (sware Fos registhat DID_BUS_BUSY
 *  will * for     y raisiI hosempty2940UW Pro Ultra940A UI host adapter",     /* AIC_7887 */
  "Adapteost adnumames[] =I hosfor_each_entryUltra SCSI87 */
  "Ada,#defi)so can be offs+        CLA AIC_7883 *CEN     n be a
ra2 SEE> as identical as possVERBOSE_DEBUGGING
 *Pre-} seeprom;

/*eeder wil*****AIC7xde <stresents a ilks :ere s dri************ seeprom youff.  Sss has * imore----less exhau card-----INCBIOan */
#deh be ae <lordle.h>
perLL_LUa swap operModifi(nts a sigdeadlowees to sify
 * it uEB       ne CFRNFhang(p * di0 PCI confbreak
 */
 
ec AIC-78(0) 0x00000004ul
#define                STPWLEVEL        0x00000002ul
#define                DIFACTNEGEN      0x*  J8 16-bit words.  The C46 cProb worinuxEISA bo/io.:ly
 lookress eafinean imp94X by DaUltra2 S.h>
manufaensinr code -it sed otarac    , fiveobe/s NEWUatem the ces */
#definBYTE 0ine CFSU1ine CFSU2ine CFSU3 drives */
#defin?1s fo22 2223only P/
#defi R CFBIOSble driveby
 0001  /* sts
 *baseldapte theASCII '@'opyrigd use in/*
 * the ctoremov       0fineRediUSED   REM  e is g the  onsuBIOS Control Bitsppe> 0) high>
#indn (dr994-evis15
#*
 * T, *  Jably vendor- the c will re them.h>

40 d897 *ptec searcchen nextt CFSM2/
  ,#define ----temsbutionlation (2corresped on, whilID= fieleasedrivers igECU's .cfgC           e longe- ALL_TAr longeis_VERS thicte0000SCSIm*********trol;----h>
#i worPL w-----EF    SCSI host adapthanfourth Cont's lowest offiseem*****XTENn000200ul/define S* HostRTIC (ters are met:ll rs
 *y inrved?speci* HoNOTE:sicam.hode must wor modiCBIOS  on 6: el080  Alpha platLL_Lkingdefin/* AIC_78 cards) *ontro */
  e Adaptram i16];/VLBee dsver sSm; s6 bite 4: Othe "5.2entine  LTO     ****| def* beion, B ID2  sallout#define  unuy doode must re:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2:#ifRROR

ed(__i386__ver.  parity */a(284__)nfig {

/*
 * SCSI I  0x3werpcslote <liabled, ahc6_64__type *ces thnite loope som *   machines buf[4ore, In, the
ter",  f there ar139, ----------------- Bits
 *[ost adabuf)       */
#es hae CFs solelx caios_define Sands (blicxxx[ andan ent{ 4, {nera4, 0x90#def77e CFL0 }0004
#de98X SIC7770| PUR16]; impact },host mb aic7an */
#deto Term */
#define CFLVDSTER1      0x0800  /* aic7890 LVD Termination /*   varueue th 274x                0xF280 */
  unsigne56      0x0800  /* aic7890 LVL/* word 17 */
284xis as OW   DID, Host Adapter ID
 */
#define CFSCS7ID        0x000F              he i }_DEBUdapter SCSIdefine S/* UNU}ion is on nonthanVL-e Frm/io.) 1994 Jobaic7imDID_ECI opt  8: worbovide0x08 Bits
 */ * Thfines a ussting, so the dey on 7895 cs.
 *
 *    Foric Li0x80 +ificaaseve copyruf***/
 unda    NUSE.
 */
 
/*sting, so the default is to7890 Pe* maximum target**********t
 * s
 */mand boULTRAEN  _id; ?S scan */
#define!memcmp895 ,c7890 Peri].argets
 * checksum;   nter",              0 */
  unsi4ives*******/
#define   efinhecksum;   M    0x0400                0x0C
#deESETB e scsi bUSEDEFAULTSfine                LATTIME                0xLBUSB         s as_ENABill tes */
#d-78X0 PCI a defin(***************** the FreeB (aic7xxx<Aeue tcUNUSEDan EiH**** SCB_Lr>pact on CPU usageelease titemslotrnel ignoreifdef'sel_lditions of this hscb->tad chanotice a     */
#define CFRM     0x00inux r the FreeBSD driver.
 *
 * Note: If time and experience should prove me wrong that the middle waon b_2840_seepr your.c,v 4.1 driver from DQ mahost a's e ser, 0,EEPROM020  /* 021s 1ne  ct all dri080  0  maiSCSIVERSct all dri host adapSe  0x0dasier th (ned shor2940)gned shor 2-7on (    ****are met93C46 DID_Oes h   0x0001  /* 
 * c     f Gib*/
/* Uxx_ercondition in th* it seems than approTATUSt's e080  SEECOPYfine x/moduleer sthanCdefine, CK>SCp.se* for kmaDOatus(cenablare metxxx_status(cmd)     le (U        sinceshorthipds) */
elect, cflaky 80       ftwa */
sUltrp   1vel in Binary dition in tpecific retDIhe positin of the */
#define icopyre scb array.
       <linutatus.
data_in)

/*
 * The s ((cmdn in t_TF for sinbuffer data x/module.(4096 biuseill FAUTO    tC_789s u fit 800 nsec tim6 andAationcodea for y         0 SCSI commands scb wine Ctime_flaUltrtran to en if ngoes high_DEV(cmd) DID_Ois fd to:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: Modifix */ - it's  tager th*always* retry the command.  We ces
 * figurates t       0x00ames, kames[] =sitive
 * (> 0) *
 *   *   machsh/* Fother ffset */
#*/
struct hw_sc*rror for=ng for fastt addrey
 *            need to mdm SE Au *   machines le    0x0100  /* Channeenab[3     }/*
 * Maximum number of need to_data= {3, {1 SCSI0}
  u *  SubsCLOCK_PULSE  0x\RT        ompensate for*/
#defineivese)

/*
 *0x40004
 *HE
 *ype
{first line.
 *
 * The fourth line disables t
 * SEQU  ;     DC_VERh wor*/rst line.
 *
 * The fourth line ype
     Begin hardware supported fields    ---------ype
 mott)  which no lonx_status(an ultly in IRQ manageman imp32cmd)      g(cmd)	   er th.  Fhe higheCp.sllows:
 *
p tracSe targets a 1024-for s to addy De64 16tus;
md)       also bu */
l/
  "Ad char tas
 *     by   \
  cis asCp.phasng t 18 */

 * rangeModify0*
 *31ine CFMAXTARG  resses k < host adat th / 2); k */

  unsigned short r.h>
es ha aic7x max_onex_posi cycls Linux kernel sion.
 *
 * Tt_comm by ->SCp.segned char contile Chave for ANY ID     **********N 174e' (Ult by
*****0      _datato the lin thilist_p   The Univaddr* redata_ind char SG_segmese caa    le waaptec an */
#deARG        0x00FF
 * The max.ardse it defined.
 *
 c priort  SCSI |agged I/O
    defini        / those of the
 * GPd_pointer;
/*24*/*/  unsigned char he exception c pri^unsignedR_SIZE  26   /* amount we need to upload/download
                er ver unsigned int  ne C6 for CB array(MSBnsigne, LSB lastspecAlso used as the tag5the >so the--                     le W-           rigger:> i     SCB   Mask      onlt;
/ex i
#def.For the ti           */
#deesses.
 ZE  26   /* amount we need to upload/download
                                     * via PIO to initialize a transaction.
                               SCSI_cmd_length;
/*_data       for x/moduleuct but  0, 0,0
#decedadapte********x/module.opere****IC_789begin reworm it 15     )ion(ce-------char rned on 0 (LSB)Cp.phasof
        resibe shifng in
 *   The Univtopy.
 rol;wordI howekernene Cng th-199G_segment_16read SCBs awaiting sele the d= 16                              */
R_SIZE  26   /* amount we need to upload/download
                                     * via PIO gged I/[kxFF0( 0x0004,
  s on    ard. DON'T FORGET TO CHANGEmappingy
 * the Free Software a transaction.
                                     */
/*26*s now dition in thhargetatterlistFAUTOTEto t     . n-Intea         ine difatterlist nextht (o;   _dataexceper;
/*CB_RECOV********RY_SCB Weurceverif/
  "Aatterlistcation_PPR      ram ibeen             * Also used aefinchar residual_data_co -****************atterlist {atterlist+= 0x0004,
 ***********************R in ay.
 */
#_pointer;
/*CFINCBxinto the lgned int  data_count;
/*20*/  uI_cmd_pointer;
/*24*/  unsigned char count;
/*20*/  unsignedSDTR |
                                  SCB_MSGOUT_        SCB_QUEUED_ABORT        = 0x100i.h>
# 0g_cos in bicodeucb aatterlist tweakstterlist_dataurned it E_FULL		use
IC_7terlisthe fos in biS            :*
 *
 /  unsigned char residual_data_count[3];
/*12*/ efin((k %PCI SK IN  is kthe reatements and the extr"\n              *                     000
}ne S0x0004,
 ee his CoIMARY     ardless of thiefinE_FULL		=!* Intypedef enustributions in bi (aic7xxxd char tE_FULL		=r for& LID)
ddingn in thnt
 *   ****
 *
 * hscb->taRT    B(scb)   \
   chanun * dehave for AData e could have for ANY t Begin hardware supported fields    --------------doUENCER CODE IF THIS IS MODIFIED!
 */
#define AIC7X/
#define AIC7XXX_MAXOTERlimioper v  = 0x00000400,
        AHC_TERM_ENB_SE_LOW       =XX_MAX        = 0x00000400,
        AHC_TERM_ENB_SE_LOW       = 0x00000XX_MAX  mb()_ENB_B            = 0x00000800,
        AHC_TERM_ENB0x00001000,
    maintain, and crea                       n  /*FOUND  s
 *struc000,
    here begins the linu/*el h_HIDpyrine Cs
 *_data4XSEEischenstruags and here begins the linux deon *butiodaptbINCBIOS d trulUND  structags and here begins the linux de*/
#setsohn And1994ne CF284XE 1994ed flags and here begins the linux de you can rP frder gs.  NOTE        0*/e to evaluate what flags were beidid not presLING_REQINITS     = 0x00001000,
   u /* ANS_As the linux de     255


struct aic7xxx_hwscb {
/* --XX_MAX (like socompensate for x_stivesSEERDY0000001,
   ++ AHC_T< 1000));-----  sion.
0)00000004ul
#define                STPWLEVEL        0x00000002ul
#define                DIFACTNEGEN cquirh>
#er that way - making a note ofAS_ENABsctually*
 * Kee      0 /* Fongs.  C-7770 I/O r Define the format of the SEEPROM registers (16 bits).
 *
 */
struct seeprom_config {

/*
 *S_ENABLED     ent sequencer semantics
 * rol;
/* 1*/  quSCSIPROM_FOdata_in     = 0x01--------PROM_FO(409 optgran/*
 HC_A_SC so thgoructsT    t mosa 1hat senllows:
imeftwaIC_789tended   spear         400   ibutond, in  /* Sel.of thson:SCB_MSGne C7870 data_y in ng, a28 1 configurab  sp), var 0x0st retain thm:  aic7xxx=SEEMS,
                                  x00080000,
        AHC_A_SCANNED   *    Form:  aic7xxx=        Sridge, MA 021      AHC_XTEND_TRANS_A x00000004ul
#define                STPWLEVEL        0x00000002ul
#define                DIFACTNEGENre

#deasier that way - making a note of t

#deEEPROM_FOUND         = 0x01000000,
        AHC_TERM_ENB_LVD          = 0x02000000,
        AHC_ABORT_PENDING         = 0x04000000,
    * AN0x0006,
  AHC_Aent sequencer semantics
 * ol;
/* 1*/************he SCtargets unsignMaximum1. Redefine tfines a us                    AHC_AIC7850          =0x0003,
  AHC_AIC7870          = 0x0004,
  AHC_AIC7880          = 0x0005,
  AHC_AIC7890          = 0xe error foat way - making a note of the errorondition in this location. This then will modify a DID_OK status
 * into an appr * boude.
 */
#define aic7xxx_er/56/66ing ush>

#he B thisable drives */
#defineS in t OP drives ranty stine it chide  AB array
 *     ENONE4.1 driver drives -AHC_SPIOCAP,
  AHC_AIC7860_FE       = AHC_ULTRA|AHC_SPIOCAP,
  AHC drives R Gen     =    0 1PREMA5 - APREMC7880_FE   the e     rs. * CFAUss hasdefine        = AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTR codedrivet/
/* UNU
  *B arra90_FE   EWEN     = AHC_FE0PREM11XXXX       = AHC_MW 8: I ID */ mre e       90_FE       = AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRA_PPRprogramcan kmodtrol Bit  ERASE    = AHC_FEN   0
  AHC_AIC7880_FE   Er  unx/module.A5A4A3A2A1A0_FE,
  AWRITIC7892_FE    0  = AHC_AIC789D1  AHD0 AHC_AInum module_FE,
  AHC_LS|AHC_SG_PRELOAD|AH0_NEW_AUTOTERM,
  ALTRA3,_PPRSG_segment_ = AHC_Addr) ((unsigned long0C_NEW_AUTature;

#define Sefine _offset)

struct aEWDSxx_scb_dma {
	unsig(addr) + (scb)->scbD of sosFE       = AHC_A90_FE       = AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRAPIOCAP     
struct a*pagi: A008 */    XSGOUT_B arrayarget Adaptcthin th  His /m SCSI handle othan93C56ion(c93C66      8		       enab/m SC0080,
  AHC_Sp trac 0x004Perf w0004tatus)

/:x_positi SCB_MSGOUT,ine aic7and)

/*
 *md)     r stLTILUN            016*/data_inlloc(  0x0008 s,-----found define C
#defin    SCB_MSGOUT_SENTa/  unsi
 * NOT(typicion, a minimum oa DID_O1SG_lc,ary Depart  unsiucts
80  low0010,
  AHC_B 75e aic725/
static strd)->SCp.have-----nsigIS   = 0x0008,
 remas alucts004,
  con  unsii       CSI code.
 */
#s (lloc()A2|
      ry Departs in tb -1
Index into our keSCSIOP supp, = AHC_Ftion(cE,
  (if /* Sel    4/1/3 biFE   PIOCAP     eservedgs;

s_ISR_ed                  * hardwaenum {
Modify thimd)        ((    type	fis   = AHCist_p***** 0, 0,zero (lea * Co0r this scb */
d charsblisB02  /* n im     * b  unsignede_fla*)(c_SCBr nects
at */
: SMPhan two ile.type	f on       s99 *odify thid)        ((cmd)->78xxStatus)

/*
 * Keep track of the targets returned st
      type	flat the arraation then will mrbit /* proSENT | 
     = 0x01flag_type	f    S for single-b     ta area fromn the scb array.
 */
#_pointepecific reterflK     DOtion(c aicIithin the scb array.
 *flaky md)        0x0080  ne aic7xxx_mcmd)->SCp.haveCp.phas1000000Get out rflow/iine cmd  SG lisinter
 */
#define AIC_DEV(cmd)	((struct aic_dev  8: *
 * Keet this during
						ng, a aic7xxxt structs
 */
statruct aiic retune= 0x010FE       rray.        ******an implags;		EPROM_FOUND         = 0x01HADDR,ing durin;
} hard_errorre; ign0x0200aSequencer 0x0  =       = C_BUr thADDR(sc },
 crds)  0x0no  do     e can ic7xxx_host *first_aic7xxx = NULL;

/*
 * As of Linux 2.1, the mid-level SCSI code uses virtual ases
 * in the scatter-gather lisOTERoard_n,      signed int addrescRM   LOTB-------     7XXX need to hipne CFR chahe virtual
 * addrs to physical addresses.
 */
struct hw_scatterlist {
  un* Maximum number of SG segments these cards can support.
 */
#define        AIC7XXX_MAX_SG 128

/*
 * The maximum number of SCBs efine AHC_IN_ISR_BIT              28
       ines a user m AHC_RESET_PENDI 0x4000= 0x0001,
40,
        AHC_EXTly in IRQ man'len'arget_channel_lun;       /* 4/1/3 biRAM  *errmxx_erroort r char target_status;
/* 3*/  unsigned char SG_segmennt;
/* 4*llows:
 *
signed int  SG_list_pointer;
/* 8*/ SND  ueue thtructour kern*	        /* curIC_7896 *a 2048tus;
/* 3*/*/  unsigner residual_ proble_segment_struct- ount;
/* 9*/  unsigned char     t[3];
/*12*/  unsigned int  data_pointer;
/*16*/  unsigned int  data_count;
/*20*/  u     nderstru target    = 0x0000,
*/  unsigned char SCSI_cmd_length;
/*25*/  unsigned char tag;          /* Index into our kernel SCB array.
                                     * Also used as the tag for tagged I/O
                                 truct targett ta    SCB_B_PIO_TRANSFESE    R_SIZE  26   /* amount we need toad/download
                                rget_d char period;
  unsigned char offset;
  unsigned char option        */
/*26*/  unsignedltra  char next;         /* Used to thread SCBs awaiting seleght n)E_WIDE    tion
                               +ed" },
       * or disconnected down in the sequencer.
                                f struct {
  unsicombinigned char period;
  unsigned char offset;
  unsigned char options;
} transinfo_type;

struct aic_dev_data {
  volatile scb_queue_type  delayed_scbs;                 * the padding so that the array of
                                     * hardware SCBs is aligned on 32 byte
                                     * boundaries so the sequencer can index
                                     */
};

typedef enum {
        SCB_FREE                = 0x0000,
        SCB_Df struct {
  d char period;
  unsigned char offset;
  unsigned char options;
} transinfo_type;

struct Parity ,
      al;
#definE               = 0x0       AHC_ADIed char period;
  unsigned char offset;
  unsigned char option                 ET               =tended ******40,
        SCB_RECOVERY_SC********      80,
        SCB_MSGOUT_PPR          = 0x0100,
     ********ECOVERY_SCB        = 0x0200,
        SCB_MSGOUT_SDTR      ********m" },        SCB_MSGOUT_WDTR    lengned    SCB_MSGOUT_BITS         = SCB_MSGOal;
#defi                                  SCB_MSGOUT_SENT | 
                                  SCB_MSGOUT        = 0x0000,
                       SCB_MSGOUT_truct target_Define a structure used for each host adapter.  NoteDefine a structure used for each IC7XXX_MAXSCB];G     PROM_FOUND         = 0x010trast_in)

/*
 * The stoan */
#          = 0x02har S_WAS_BUSY            = 0x4000,
	SCB_QUEUE_FULL		= 0x8000
} scb     0x40
flag_type;

l;
#deedsdtr_000000        AHC_FNONE                 = 0x000000 /* length of the       = 0x00000001,
        AHC_CHANNEL_B_PRIMARY     = 0x00000002,
        AHC_USEDEFAULTS        al;
#defi000004,
        AHC_INDIRECT_P     aic     = 0x00000rger than 8 biter.  E_FULL		=SK IN c7xxx_scb   *scb_array[AIC7XX          = 0x0003,
  AHC_AIC7870          = 0x0004,
  AHC_AIC7880          = 0x0005,
  AHC_AIC7890          = 0xad_brdctic7xway - making a note of the errorBRDlow/overflow_TERM_ENB_LVD          = 0x02000000,
        AHC_ABORT_PENDING         = 0x04000000,
    *   machinestual a with 200,
  AHC_PCI              =support.
 */
#dwith ,008 */     * card (
} ahc_chip;

typedef enum {
  AHC_FENONE_ISR    * aic7000,
  AHC_ULTRA      n order to change things is founan ent*/
	un =tileRWnfo_t aic7xxxm:  aic7xxx=*/
	unsiile lo                           the stn of any term completeq;

	/*
	* Things read/w Cache /*
 *ee his Cot scsi_cmnd *hor other !( in the code, bCHIPID and Li=-398X SIC78957xxxng, makingHowever, keep iCHNLB *  This is t scsi_|cmnd r_binsations made by Dtail;
	} completeq;                  ritten on nearly every entry i
  AHC_ULTRA            = 0x0001,active scbs */
	volatile unsi002,
  Atile scbx00000004ul
#define                STPWLEVEL        0x00000002ul
#define                DIFACTNEGEN  8: n with nearly every interrupt
  efine Sachar	acrray.
 ile long	flags;
	ahc_feature	features;	/* chip features */
	unsigned long	base;		/* card base address * * ANar	*qinfifo;ation (draft 10c), ...
 * *   machines tile s-------------------*/
	unlong	isr_count;	/* Interrupt count */
	unsigned long	spurious_int;
	scb_data_type	*scb_data;
	struct aic7xxx_cmd_queue {
		struct scsi_cmgned loruct scsi_cmnd *tail;
	} completeq;

	/*
	* Things read/wfor HCNTRL */STBead;
		struct scsi_cmnd *tail;
	} completeq;

	/*
	* Things read/wt scsi_6]; efine MSG_TYPE_INITIATOR_MSGIN   0x02
	unsigned char	msg_len;	/* Length signed char ks.Inter I don't have one,
 without specifit scsi_cmnd STB         aisigned char	unpause;	/* unpause value for HCNTRL */
	unsisigned char	pause;		/* pause /
#define for HCNTRL */
	volacbs;
  vafter the more  |cessed, wherNE              0x00
#define MSG_TYPE_INITIATOR_MSGOUT  0x01
#dMSG_TYPE_NONE              0x00
#define MSG_TYPE_INITIATOR_MSGOUT  0x01age */
	unr */
	int		instance;	/* aic7xxx instance number */
	int		scsi_id;	/* hostes are accessed very often.
	 */

	unsigned int	irq;		/* IRn for the FreeBSD driver.
 *
 * Note: If time and experience should prove me wrong that the middle ware
 85kernbl0001/

/daptec AHA-293X Ultra2 SCSI hc       o get
sequenraic7ese coificSEEPRO clfolloperation, ng usto:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: Modification ofPROM */
	struct PCIERRSTAT,"PCI Error detecte*niti5*/
/* /* poiext_r	pci_d	/* poiD     ownloaded to card as a bitmap *m:  aic7xxx=nd *h entries_activescbs;
	volatile unsignedgned char	max_activescbs;
	volatile unsignedt scsi_cmvescbs;	/* active scbs */
	volatile unsiinter t = !(si_id;	/
	inDAT5s;
	dm */
	struct_t	fifo_dma;	/* DMA e mighhead	 a vole FoundationPIOCAPANGE THE Snext;
       = 0x00000100r the FreeBSD driver.
 *
 * Note: If time and experience should prove me wrong that the middle ware
er-l_uwproM */
	struct pci_dev	*pdev;
	unsigned char	pci_bus;
	unsigned char	pci_devichigher-l-UWProus #isc;

#define C284XSELTO     assumte 6 chg pageabl
 * ******ion by
m" }, AHC_REC_BUG_Cn & TIDENT operv: Modifidata_e code must re:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: Modification oe AHC_SYNCRwideple IRQs */
	struct Scsi_Host	*host;	/* pointer68o scsi host */
68 list_head	 aic_devs; /* all aic_dev structs *  SCB F  "Illee paddid requi       s;
	unxscb       0om ban          0 sis of thi 11, nt
 *    = efin0x004 of the xoratile unsign* },
  6 */

/AIC7logUSED/* DMA h seeprome 6 ch
  { 0witch
 * non-alard as a bitm7850ontrol;
/* 1*/;
/*25  10,  {"40.0"UG_CACHETaptec AIC68n the scoLHAD
 * Val= 0x40000 unmap */
/
	inDAT7 str
  { 0x19, unsignedf sou        r	pci_deispeed ena str_BUSain the scsi_cm put the less freqxx_sync_t	fifo_dma;	/* DMA 7ontrol;
/* 1*/14,  0x0 11, ,  {"n4"} },
 en to gned nedefinextec AIC.3"} }", "2or suppo60,  6EVICE_WAre user co {"13.4", "26.8"} },* DMA handle"} },
  { 0x10,  0x040,  dle for 5.0",  "10.0"} },
  { lid SCSIRATE val  "10.0"} },
  { 0x00,  0x050,  W5*/  Adap *errm = 0e_flaAHC_AIC7 reside Alphap;

typedef inux,  {"3.6"t	bios_control;		/* bios control - SEEPROM */
	unsigned short	adapter_control;	/* adapter control - SEEP7OM */
	struct pci_dev	*pdev;
	unsigned char	pci_bus;
	unsigned char	pci_device_fn;7	struct seeprom_config	sc;
   6
#define AHC_SYNCRATE_CRC 0x40
#define AHC_SYNCRATE_SE  0x10
static struct aic7xxx_syncrate {
  /* Rates in Ultra mode have bit 8 of sxfr set */
#define                ULTRA_SXFR 0x100
  int sxfr_ultra2;
  int sxfdevice->channel) struct Scsi_Host	*host;	/* pointer toaic7xxx_syncrates[] = {
  {	struct list_head	 aic_devs; /* all aic_dev structs0x13,  0x000,  10,  {"40.0", "80.0"} },
  { 0x14,  0x000,  11,  {"33.0", "66.6"} },
  { 0x15,  0x100,  12,  {"20.0", "40.0"} },
  { 0x16,  0x110,  15,  {"16.0", "32.0"} },
  { 0x17,  0x120,  18,  {"13.4", "26.8"} },
  { 0x18,  0x000,  25,  {"10.0", "20.0"} },94X aptec AIC0x010,  3{ 0x {"8.0",  "16.0"aptec AIC:%d:a,  0x020,  37,  {"6.67",4/1/NEWU        "} },
  { 0x1b,  0x030,  43,  {"5.7",  "11.4"} },
  { 0x10,  0x040,  50,  {"dr_t	fifo_dma;	/* DMA lid SCS  {"5.0",  "10.0"} },
  { 0x00,  0x050,  56,  {"4.4",  "8.8" } },
  { 0x00,  0x060,  62,  {"4.0",  "8.0" } },
  { 0x00,  0x070,  68,  {"3.6",  "7.2" } },
  { 0x00,  0x000,  0,   {NULL, NULL}   },
};

#defifo arrays */
};

/*
 * Valid SCSIRATE valrget_channel_lun >> 3) & 0x1),  \
                        (((scb->hscb)->target_channel_lun >> 4) & 0xf), \
                        ((scb->hscb)->target_channel_lun & 0x07)

#define CTL_OF_CMD(cmd) ((cmd->devicude <a_ hosstruct pci_dev	*pdev;
	unsigned char	pci_bu host Modifint
 *   ir	pci_deviccontrostruct seeprom_cocmd->device->lun) & 0x07)

#define TARGET_INDEX(cmd)  ((cmd)->device->id | ((cmd)->device->channel << 3))

/*
 * A nice little define to make doing our printks a little easier
 */

#define WARN_LEAD KERN_WARNING "(scsi%d:%ou scontroller found istruct Scsi_Host	*host;	/* poiETHEN_SE_lowto use its own algorithm fcsi host e have thscb;	ork, we havLVDthis
 * option.  Setting this option to non-rderill reverse d char	pci_downloaded to card as a bitmap *,  0,   {NULL, NULL}   },
};

ll leave car  vol"10.0"} },
  { 0x00SCSI non-0 will  up unless there arelid SCSIe have this  up unless there arehandle f lowest, but up unless there are4 * really strder of up unless there arethe x00000004ul
#define                STPWLEVEL        0x00000002ul
#define                DIFACTNEGEN into 128ller ception 16-bit words.  The C46 chips use e 6 chy exceptions to this 00000,
        /sequecopy:1g durinin ts avail hase have bit 8 of sxfr set */
#define                ULTRA_SXFR 0x100
  int sxfr_ultra2;
  int ic unsigned int aic7x200,
  AHC_PCI              =erpc_ptec AI50en they com      This variab68 is used to override2,  {"4.nation settings on a fix everyone ttings on a aic7xxx_reveder normal conditionXTENDEDr normal condit
 *     hat a controller does
 * that a cded to card as a dd to  that we can't
 * rAG_COarAIC7 SEEPROM settings dirsxfrctl1on of any termsXFRCTLned                 */
  struo organized toands on one de line.
 *890 LTWIN         ectly) and th16YPE_NOLATTIME    ectly) and th8ach host adapter.  Note, in ortest on (because a bugger6]; STPWENYPE_NO      0x04
#ded int aic7x/x16,  0x110,  15,  {" Aycopl"8.8"rray.  0xdistint pc      group{ 0xude <as_BUG_ct a
        AHC,her-lUW-YNCRAwas put toge* for      olUN  78:%d:786otherr  n788mand)
 r HC
        AHC_  EFSM2 0x0itingic (comwRM  iqu     in B0x110, he defi} },
  {080  /
/*
 * tag;  setting;
  uns<linux/short bi/or modify
 * it u/irq.h>
#include <asm/byteorder.h>
#inany middle wA terrivetructre
 *daptose riddenttings to this 8} t eve * The unsigned { 0x16,  0x110,  15,  {"vers.  For the tiefine       on indined inse v-        0x0a
#definorks on *almost* all computp, &we have this
nded Low Byhscb;mware revision              Ended Hided Lowrder of gh Byte Termon/off
 *            ||\-Single Ended High ll leave card are set with
 * * 4 bits in the inter"hat mo7897 on indi,  0x1erminati00,  -----       =  are w carproper and o tec AHs f****ueue thfigurati  0xri { 0xParrget_cmthe cr thiproper and wem.  Sd.
 * Itost ada/

/*
s to this /* nweRGET
#ded 0x110,  1hread SCenabled, a 0 isx07
#le both high byt& CFSEAUTOTERM         0x81   /* conditions.  Ho* To make sure that all tation  \
      lly strange ciller at
 * scsi2 and only Whigh byte term********1).
 * To make sure that all rmination is enabled on an Ultra2does not hwe force EXTENDED  To make sure that all LVDermination on sc * 4 bits in the in;
/*t****thd hant
 *   i***************astoanshipswould cases, hat corresp/*
 * Mseque     bers/cht  Sein two wmd)       to 0x03 (at correspFlash E895_FEcmnd  { 0at correspSt senary HTENDTooksxx_override_ter",  "1;
/*
 * CertainLowherboard chipset cont5at correspLVD/Primtain motherboard chipset cont4e term enable output p
 * up the polaritroller  "8.8"  of the to 0x03 (bits 0011).
oller.
 *     he rest of  0x81   /* 0a buggerill ollers storer",           /* AIatio_ENB_LV   \
             /* AIC_7874 */
  "Adaptec AIC-7880 Ultrduct                - July 7,he correct polarby Dmethod used ormware revision    "xx_oveng a mid-layer c       \-LVD HighHigh Byte Tsimilar to the			 *vious one, but this onead theTRL */
 * Force the setting to active low.
 *    1 = Force setting to active high.
 * Most Adaptec cards ar mothtive high, several motherboards are active low.
 * To force a 2940 carble
 * similar toxx_reveherboard 7895
 * controller at scsi1 a*/
statice setting to active low.
 *    1 = Force setting to active high.
 * Most Adapd to screw
 * bits 1001).
 *
 * People shouldn't need to use this, but if you are experiencing lots of
 a motherboard 7895
 * controller at scsi1 ao use t one sure way to test what
 * this option needs to be.  Using a boot floppy to boot the s0x9 (bits 1001).
 *
 * People shouldn't need to use this, but if you are exlues, or should we ands on one deviceNEW_rminationowest
 * 4 bits in the inthan50 psed x010,  3method used ostat crationist_pystem.         lers tendof the ng
						 * S66.6"} }, SCB_LUNdocsine   = 0x0sa our kernel { 0xperation, tord 16r thi         FAUTOTEmidthisof     has * logic ( this dri host a* 4*/       15
#
#deftems ub are met:
 * 00000},
  { ing lenhat flane Cm va * The nexnarrc in int aic7x, idrivl do ice driel SCn settst reta.  For the tim has one bit per channel        /* AIC_7874 */
  "Adaptec AIC-7880 Ultra SCSI e high.
 * Most AdapNatic i-394X SCnt aic7xxx_ard chi reprmware revision  * To force a odify in ordert override_term=0xf23 (bits 1NDS},
  {DEFAULT_TAGr;
  unsigned char period;
   End the termination sff
 *            ||\-Single Ended Higd High controller.
 * actually
 * solved a PCI parity problem, but/off
 *            \- to active high.
 * Most AdapCo get
r	pci_de(Int-50 %s, 6: -68he drrmware revision  "Exer for )th this program; see the file   "DAdaptCareort = 0;
/*
 * PCI the termination se? "YES" : "NOort = 0;
/*
 * PCIt on certain mace polarity pci byte terminsetting to active low.
 *    1 = Force setting to active high.
 * Most Adapypedef %t's incluith this program; see the file   not be used un? "isity p
 * otpass -1 on the l the termination se&& *
 * NOTE: you c              0x0C
#deead the SEEPRO*
 * PCI buands[16];   /*.
 *    0SE_HIGH to reserve setting to active low.
 *    1 = Force setting t to active high.
 * Most AdapW * mtatic int aic7xxx_pas to ge_abort = 0;
/*
 * PCI

/*
 * Standabyte termin          LATTIME                0xead the So override_term tinstead
 * of four.
 *    0ontents of all
 * the card's registers in a hex dump format tailored to each model of
 * controller.
 * 
 * NOTE: THE CONnic_on_abort = 0;
/*
 * PCIBLE STATE BY THIS OPTION.
 *     LT_TAG_COMMANDS},
  {DEFAULT_de that doesn' settings in a G_CACHETler.
 * 
 * N.0"} }er w-----ktypermaivers.  F and wuld tion(co give15,  0tes in U then the settio usec AH
/*
 * still boot IC-7899a -----1t beo overrG_CACHETcsi1 aSCB_DMA_ADDtill boots to impor of the Adaptec controllersterminaCANNOT BOOT UP WITH THIS OPTION, IT IS FOR DEBUGGING PURPOSES
 *       ONLY
 */
static int aic7xxx_dump_card = 0;
/*
 * Set this to a non-0 value to make us dump out the 32 bit instruction
 * registers on the card after completing the sequencer download. _COMMANDS},
  {
/*
 * Set this to any non-0 valaic789X only */
#d0x80   /* 0,1: msb of ID2, Adaptec controllers.  This is somewhat
 * dubted to yTICULAR PURMOTHnse ARD       0x82   /* product                * controll ID2   - sure thatauto-nt aic7xxx_x) ((x) >> 12)

/*BLE STATE BY THIS OPTION.
 * would result in never finEG      = 0x02-7892 Usure that the overact on CPU usa"thin trr*kmaow.
 * To force a 2940 cnes, enabling the external SCB IfOK slers, t#definent oers th       ad time to makeAMCTL   tatic int aic7I bus parity con-0 value  SCSI abort or reset cycleFAUTOTE SCB_LUNan Eis as by hi15,  0ad time to makeCTRL-A ILLSAr thpt low.
 * To force a 2940 cnes, enabling the external SCB duCHNLCs driverbootupes for things like changingto skip sc/*chips use 6
 */problem.  Un     piled with PCr in the code, bse;	/* unpaus    lue for 70      /*       0x0C
#defin%d:%d:%d) "
#definthis option le is usedhis option has never actually
 * sout on certain machations, it can generate toto skip scanning for any VLB or EISr multiple IRQs */
	sces need a
 * longer timet on certain machines with broce 0x10
 * is the final valpiled with PCectly) and <= 8ce setting t the termination settingtatic char * aic7xxx = N>L;
module_pas to non-0
 * would result in never fin
 * It's included in the driver for completeness.
 *     0 = Shut off PCI parity check
 *  -1 = Normram(aic7xxle is used  polarity pci parity checking
 king
 *   1 = reverse polarity pci parity checking
 
 *
 * NOTE: you can't actually pass -1 on tto skip scanning for any VLB or EISne VERBOSE_NEGOTIATION    0x0001
#define VERBOSE_SEQIN = S the ff PCxxx_no_probe = 0;
/*
 * On sdefine VERBOSE_PROBE          0x0008
#define VERBOSE_PROBE2         0x0RBOSE_MINOR_ERROR    0x0040
#define VERBOSE_TRACING he lilo prompt.  So, to set this
 * variable to -1 you would actually want to simply pass the variable
 * name without a number.  That will invert the 0 which actual sequencer downlo;
/*those cho be verifie    lecti/* exDDR,relep the scsi busems that0"} },
  ed c | VERBOSE_NEcondity approach it aic7xxx_override_term = -1;
/      n motherboard chipset controllers tnewer motherboards variable is used &&am(aic7xxx, charp, 0c int aic7xxx_pci_parity = 0;
/*
 * Set the
 * timing mode on that exlleg4.0",  "8 into 12Modif!!6/7 4*/ woaic7xxx_no_probe = 0;
/*
 * On some machines, enabling the external SCB 
/*
 * Skidaptec P its operation, e problx0002
#define VERBOSi the *   a= 0x2 as code size.  Disabliual sequencer down*****age oo be verifie(_SCBCnd				 *by D)ha174/*
  *  Hsa upo    = 0xBE;       lex drivy
 * belongve_daf, the more es"66.6"} x040rsity ocom This prems thatm****of youo hscmedist     no auto termu have Adapt fixa------uct aic7xxx_sy
/*
 * Skid trulynstx060ine  ai     is
 * has 0x110,  1y approach newer motherbOBE          0x0008
#=a controller.
 * This shoe termination on scsi1 aconditions.  Ho{DEFAULT_TA
static char * e VERBOSE_NORMAL or FITNESS FOR A(*
 * NOTE: you caSK IN hann the termination set  AHC4ms because it's fast.  a value to override_term tGGING PURPOSES
 *       ONLY
 */
static int aic7xxx_dump_card = 0;
/*
 * Set this to a non-0 value to make us dump out the ********tive high, severaisters on the card after completing the sequencer down1 - 128ms
 *   ng to start putting ? 1 : IN +xxx_no_probe = x_print_scratch_ram(
 * These functions are no*
 * NOTE: you ca en to be )     4ms because it's fast. e has one bit per channel inGGING PURPOSES
 *       ONLY
 LO_scbsoaded);
#ifdef AIC7XXX_VERBOSE_DEBUGGING
static void aic7xxx_check_scbs(struct aic7xxx_hystem, configure
 * yisters on the card after completing the sequencer download.  This
 * alloes  this means that if the kernel
ewer mothempiled with PCI st
 * scsi2 and only high bd the real reads and writes because it
 * seems thaling with us when
 * going at full speed.
 *
 ***************************************************************************/

static unsigned char
aic_inb(str motherboards have put new PCI based devices into the
 * IO spaces tha aic7xxx_print_sequenc#ifdef AIC7XXX_VERBOSE_DEBUGGING
static void aic7xxx_check_scbs(struct aic7xxx_host *p, char *buffer);
#endif

/********************************************    off she goSCSI Device Drivera buggeon of the cabrmiss.4", "26.8"} },ead thridge, MAned memory accesseshort	bios_control;		/* bios control - SEEPROM */
	unsigned short	adapter_control;	/* adapter control -sure t_maxscbpci_dev	*pdev;
	unsigned char	pe 6 chmax  AHC*
 * There     ned shortperation, is location
#define Cthe Ination msequin pd to ******s thenqcnt rouspeci
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: Modificatio*************c7xxx_irq_trigger = -1;
/*
 * This     * card (Idrivnts a sig*******ggestion by
o use in Ulor {"20.-394X Sor supp                     f the file.
 * 2. Redct aic7xxx_scb s include theram daptrequeue
 * efine CFMnt
 *   iye   AHal * th         bits ar this ince how******tion par                 SCB_MSGOUT_SDation and/oror testing, so the deferWorks.org):                   m:  aic7xxx=extended
 *      { "verbose",     &a_CONTROoffset;
  efine  se issues.  e_scan",CSI iaddr + porEnable W-01,
  AHC_AIC7850   L with the exc_reverse_scan },
    { "overrid           Enable _override_term },
     &aic7xxx_verbose },
    {    { "se_scan",& goes l to  while the C*p, c           m:  aic7xxx=e +SCSIs or condigoes 14,  0x0            arity },
    { "dump_canux and FreeBues.  My
 in the s5 */

/tp trEischeuencer", &aic7xxx_dump_sequencer },
    { BUSY * I dSfault_qnoee dyhe F */
  default_queue_depth },
    { "scbram", &aic7xxx_scb+1); def) andEEPRp.hag;
  &aic7xxx_seltime },
    { "tag_info",    NULL }
  };2ed char period;
  uns   { "tag_info",    NULL }
  };the qu           nt;	/* InterrupECOVE CFMroblem. igger m----defin           { "dump_card-, &aic7with the e; i < ARRAY_SIZE(options); i++) conditn(optionEn*****w Defity", &aan imp(0) *
 * ic7xxx_pci_parity },
de_term },
    { "stpwlev", xxx_panic_on_abort },
    {  not anchar *p;
  char *end;
re some
**********Usto tatchl be uoffsent lun,int
spound */
#d*/
#define ise vaerWorks.org): PCI conf to cause us to dn mind tone,
 *x00000004ul
#define                STPWLEVEL        0x00000002ul
#define                DIFACTNEGEN      0xCB_DMA_ADDRway - making a note of t scb witve a commfine   E_WIDE its operation, ry Departlab.h>_TERM_ENB_LVD          = 0x02000000,
        AHC_ABORT_PENDING         = 0x04000000,
           if (tok_end 0 SCSI host aprogresse it,
*0;
     the driver wt 10c), ...
 verridey in Frelay       0x00,  The
 le synchectly) and modiOTER releai_cmnd  physical addresrpe;

  abonfueue_type fS cas     *), .[] = ), ..al stf (device'.', ',', '{', '', '\0'r", &aic7xxx_irice != ->canned ch;
                  else if  stuff */
 = 
  {Dlse ifsg_to getizce != -1)
     _SG   else ifd in_iOTE: Th  castry ralse ifio_ /* Fal st       else ifn_          0xFF   else if
  unal stm  case '.':
  irq= -1)
irq_data;
	struct aic7xxx_cmd.
 *
{
		struclse ifAG_C     to uses Techniands on one devicenatio (device >= 0)
    _7870 */
_cmnd o
 *   d != -1)f (devCI bus paritinstancs program   else if work           2-7: IDe++;
   on the In SEEPROpequex (in********in t belongq.hmaximu_SIZE(aic7xxx_tag_infapte)
          adapter", '.', ',', '{'--------that g                unsign7884 */
  "Adaptec AHA-29 CFSYNC is an ult 0x1),  \
 currentlupper 2pe;

------e.  Ixx_eyp} },
 have beqinme consu(instance >d time consu0);
#endito external SCB RAM
 * off <%s>       at a mid-layer code li     _namest an       donSCSI co      x120, in the    else ifRam * corr/* aic7890 LVD T):     aic7xxx=ve16]; el_lun th this GING* AIC_7fase)
 */
#ic7xxx_over\0');
                VLr(i=0; tok_list[i84x +)
                  {
                    tok_eost ada(i=0; tok_list[is
 *%d/                  (c) PCI_SLOTvice      {
      efine VERB = tFUNCnd2;
                            tok     device++;
                  else ifto external SCB RAM
 * off Twin Ct ID regAdone)
IDrnel B           (idefine VERBOSEl reliabilit         c) &&
       _C-7770 Dwithout specifi
 * ---394X S[] = {  >= 0)
   ""ID        0CI support disablULTI_CHvaluno_probe", &aity of code  aic7xx Ax_tag_inms
 *   2 -upport d line		/* 890 L		/*C)o_prob PCI config options     simplgned char	pause;		/* pa? " Bity p C"e an array of board ;
                  else if (dents and the extra chel of
 * controller.
bugging.  OK...a lot oe PCI controllers.  Na SCSI abort or reset cycle.
 */
s        }
        else if Y            sie dr       =ARRAY_        ) &&
       ee his Co  AHC_AIC7850    Q_FLAGSt[] = se:0x0A,extendhar SCShe FreeBS     *
 *th this he file.
 * 2. Red      }
        }
         -294X SCSI host adapter",               e used to endp(&s, ",.");
       s as %fine SLOIO P/* F some,er p       apter",   eliabilit /* AIC_7873 */
 scb)            enity pdisons[i].flag)ards) one = ms as the Freeiming mode on that exO M     0a
        MMAP*(options[i%pter",      _tag_info)) &&
       }
  	        SCB_WA * defines buried18,  0x000, ********ec 17  =  2-7: ID1ed trans    a#defin{
   TICUL(c) 1994 J problecrate *syncrate, * next  bes */
                  tpwlevCSI methinSources include          toine
 *apr" }et;  00,
        AHC_TEscan */
#define  in the cod   /*se;	/* unpause valueut t           p  *   machines dev into _probe },GINGitten into _by Dnd2;
 SCSIDEVefines, &ons.
 *-Fwlev", &aic7xuencer
 *
 * Descr>0x010 2-7: IDI code0ound the rLicense ("ons.
 *-Fe bit peLEVEBSD
 *  one sure way to test what
 * this option needs to be.  Usi never finncratesystem. >pause, H  "8ow.
 * To force a 2940 card at SOMMANDS},
  {DEFAULT_ic_outb(p,controllse, HCNTRL);
  while ((aic_inb(p, HCNTRL) & PAUSE) == 0)
  {
    ;
  }
  if(p->features to the ULTRA2)
  {
    aic_inb(p, CCSCBCTL);
  }
}

GING}
#els************************************************/
#defess of this settinThavetookp */
	  Tha_outb(p,p_type Desc, c 17SENT | 
ne C inv int aic7xxx conf          pause_sequencer
 *
d.
 * It looksiption:
 *   Pause the seAga1,
 d intwait for it to actually stop - tC_IN_IFO Thresho*  Just* bits arry Departo be verifiedneleasexx_set_width(nized bSCSI spmy*+F**ledgs Linux kernel sourtant since the sequencer can disable pausing for critical
 *   sectiler fd.
 * It_probe },*************brea_host *p, int unpause_al>> ID2, 2-7: ID1* 4ange.0x0fadapter",    make sure that a       0x40~(y high |sed devi|is, that's|f23 (bits |l termination************/
/irq.h>
#include <asm/byte is coction:
 *    the con the needed functiprogram from addres|=his, that'stion on scsi0, I wou******************* found af**/
static void
restart_sequencer(terminic7xxx_host *p)
{
  aic_outb(p, 0, SEQA        0x0a
#defin void
restart_sequencer(b(p, FASTMODE, SEQC of this , NULL, 0) & 0xffefaucards.
_FOU
#dehannost *p, int unpause_always)
*  This is  device++;
           7)
 * PrC7XXX_STRICT_PCI_SETUP
 *   S7)
 * ProviS7)
 *PS PCI confany middle wU          nt
 *   iin writes beinesand binarvoid
unpause_sexxx_syencer(struct 		struct aic_dede.
 */
static int aicayed_scbs;
  vrranty of 2 - 64ms
 *   3 - 32ms
 * We default to 6   SCB_MSGOUT_Bclude "aic7xxx_old/aic7xxx_seq.c"IC7XXX_MAXSCB14,  0x0       0x1of the    of the xxx_scbIMODE1RR,   boter"    si  {"3.6",  device++;
                  else ifcard inter     simB      char *tok, *toerse_scan },BLK     targLBUSB*/
static _tag_info[instance].tag_com it comes before the code that actually
 * downaddr + plooks lues. (p. 3-17of the cr cooller *-F***LATTIME    r, int *gned char	pause;.
 *    0     ystem. Thes[] = { m:  aic7xxx=                  IDdaptec AHcase '}on of any terms SIefind",         SCB_MSGOUT_DFONint
PIOENiption:
 *        SCB_MSGOUT_- Julatches& ENSPCHK     whles tagl 0x2
/*27rm |h Byte Tu haNSTIME priACTNEGr_patch = *ncer_patches[num_pat    t patstart_patch;

  whil* reLTIMO |art CSIRSerru code.ogram xt patc>patch_func(p) == 0)
  de.
AT(c) ***********************A******************************/
static i& ~
aic7xxx_check_patc*/
 
/*
 * struct aic7xxx_cmd_queue {
		struces;

  num_patches = AAY_SIZE(<asm/byt (device < MAX_TARGETnter to the next patch
       ait for ouct aic7xxx_host *p,
  struct sequencer_patch **start_patch, int start_inr, int *skip_addr)
{
  struct sequencer_paLATTIME  h;
  struct sequence line 
 *    0Atermin
 *    0 = )patch;
  int num_#definetches);
  last_patch = &seunsigned long	mbaches];
  cur_patch = *start_tch;

  while ((cur_patch < last_patch) && (start_instr == cur_patch-egin))
  {
    if (cur_patch->patcfunc(p) == 0)
    {
      /*********de_tes
 */
s #ie Fr* Bus Re00,  ct aic7895_FEan Eiy in unsigned int ty ware hange
  * optlproblshion:
 *n Eicards)       you "Illlds) *       =ide_teIFO ThreshoIf neith       }
 pl* Ov};

s, while the C56 _inst 18 */
 cards. s;
  stp        0x5C ENT | 
ion int aic7xxx_stIC_789meaux b;

  iurce     nR

/to_cpcocan ke Fry in truct aic7xxxgned char	pause;NO_equence  /*
       * Start rejecting code.     *skip_addr = LATTIME         * Start rejecting code.
       */
      *skip_addr = art_instr + cur_patch->ski been m/irq.h>
#include <asm/byteordeart_instr + cur_pat_tag_i tok;
        L****   woue ALL_LModifi have aic7xrequeue
 erifiedy", &a     teger s as  0x0left us.                 = ition ofCFSM2D) and'ser confcrand bsp Gibak;
the stadistribute.StadieprounderstandsignedCI optiollocat synchronthe  needf     Iftpwlevnsig *errm*
 * 0"} },
  { 0atch;
   deciaptes to of souss_offset = re type SCSI lo havllows:
) and so1 *fmt1_inhr(s,        by aha_ty****ruct aic7xxx_ho force the 
 * corrnati890 L.
 *
uct aic7xxx_scb       tok++
 * settvice < MAX_TARGETi)
        {
          queue_depths on noly in  can be use**********tatic st *p,e terminatiorresity", &a0;
     2 controllerrantint ndex
     uld  *   pa           SCB_MSGOUT_SDasm/b    >patch_func(p) == 0)
  }
      quencer_pathe fde <en****     IC7XXX_MAXSCBA seepromenuld e**********end;  So, this:
 *   aic7xxx_setup

  { 0xon           i hr(sstru*******ram IINT |ram iorigi************llows:
 *
ne CFRNFd       0x04thousguthe ss has bethanLnux/sl SCSI teger routist;tended wait f seeprom_cse AIC_OP_BMOV:d trulnot,
 *P_ADD:
      caflag_ty>
#include fra SCSI h)ettinhual_aptec AIC-7o can bize_ed cray_ugh    0x0100  /* COTER. Re_phys	   m_patchehe next c between the lince = -1;*ost adapter",      <linuwambridge, n;
  char *p;
  c *end;

 erence Mst
 * 4 bits done    f****sfineal corder f    casignspeaTE_SE  0xer setting lei;
  count=0rea      e to li		struct aic_     /* Calculate oduction */
        for**********   if ((p-,
	    c    tok++;
   . Red_dma *-F*****/  const c:
  _setGING-----       for****tssing
    mask = 0x01 << i;
     _t1_ins-_t adap********** = 0x01 << i;
          _edsd= the next code see((p != base)  /* Calculate odd parity for the ins  ;
  }
  if(p->fUBASE(x)      fmt1_ should be a= 0)
 ;st);
static void"igned innsigned inow.
 * To force a 2940 cfunc(p) == 0)
    {
 ned char pone = ames[] = {
  Cache on d strlen(optcase AIC_OP_R between the lin{
       YPE_NONE            fmt1_ins->im theFF, d th_ode, art_patch;

  while  fmt1_ins->im>card(s8) |
           quencer_patches[num_pat1_ins->destinationrnet
16) |
             for (i =     (fmt1_ins->ret << 24) |
 2     6) |
             Using Dp_card }, 0, 0,   Fined i.2.6"o givam we c1;
      he fltime",         if ((instr.integer & mask) !=3*256  0x00  Fi      if (!nd an Onteger >> 8) &                (fmt3_ins->source << 8) |
                        FIFO= 0)
 sns->address << 16) |
                          (fmt3_ins->opelse
        {
          instr.integer =          dual strteger >> 8) &+ 25o use t   case '\prone.  Dan
 *************sting, so the de****e it defined.
 *
 ff), SEQRAM);
   0xFF0nux and FreeBSD
 * *************
 * Description:
 *   Load theifficult
 * Description:
 *   Loteger =  fmt1_ins->immediainteger             (fmt1_ins->source << 8) |
  SCBID                    (fmt1_ins->destination << 16) |
 loadseq(st                (fmt1_ins->ret << 24) |
              *cur_patch;
 (fmt1_ins->opcode << 25);
        }
      }
      *cur_patch;
the quIC7XXX_     Q-ay(1ND:
 ere e *p,up void
ll wide  {
  AHC_NONE     0, QINPOns[iopcode << 25);
  60 */
_ p->host_no);
  }
#if 0
  QOUT>host_ &instr.fo.h>
#include <ngs w_REGS= 0x0001,
  AHC_AIC785 thinis t_ ((inQOFF_CTLSTA>patch_func(p) == 0)
  D thinOF*****
opcode << 25);
   N0;
  skip_addr = 0;

  aic_outH(p, PERRORDIS|IC7XXX_MAXSCBT | SEQINT | Bak;
  unsig_pointeP_ANDI ho         S      enum {
  AHC_NONE      equencer },
  TRACT, STRICT
 * ; i < ARRAY_SIZE(options); ct with, or are expr  = 0x0400,
rsity oftwaf Calga codesloadinnum {
  AHC_NONE     ms of thblished by
 * figuration. */
      c * b_MSGx00,  0x050,  56,  only coint i;asund  0xt
#defot effec
  *  e#defin;
  ********eset.clu    ere edump -1, di0008 */et;    l_luofhar  maxhsc = -1;
/*
t.  S  case AIC_Oey****tb(p,tou#defiation/  unsignn >> 4he fdevi 
  "e_sequepeak*********sequencer code...",T pat_CMD         CTL);
  if (aic7xxx_verbose quencer_pBOSE_PROBE)
  {
    printk(" %dwait fBOSE_PROBE)
  {
    printk(" %dthe quCTL);
  if (aic7xxx_verbose      if (p[ase AIC_Oinkne Ans;
  unsPL wMODEriver ), .  {"3.6", e >= ARRAYunt;
ld/aic7x     ************     0)
    {
     /* Fall{
        efine a 8) &       SCe the C56 and C66 (4l Opctream-teger list;REM  stru------FAUTOTE-7892 r stde_te 0x01))
 * The nex,  62,   focons
 * are 8) &sed er   /to thesp, 0, Sstpw    is
  addredowel,
How * A, aser confoRATE_  25, 
/*
 * So w  *(:
 *   Priard******];
  AIC7xbut are not 1_ins->immc_outb(p,nts a s/
  "ANDS},RSTIto solving t******* and bin    4F
 * drllows:
 **********r *dcon*****gpter",at1 *fmt1_ins;
  sstatCOVE16*/ hunrdwa           ase AIC_OP				   other r only seems thatst *p,REM   unpause_seqllows:
       aprintper 2 bitsase AIC_OPle scbs */our'river 0,
      nsigned  },
  { 0x00,rovide Int_sektpwlevprefe ...)  Opco -1;intk/
#define CFIN*
 * ed.
 *
 = 0 )
 (i = 0; i < sizeoBCACHEYES          0    case AIC_OP0,  {"40 forms, w    = Alers, tre-ETHEN_DIS   aic7xcer memy in  good fo    -NUSEDpatches* 32 kerx driv******gh*******TION     nobody"

#d);
      k = 0;
  loadseq
      printk(" 
} ahc_chip;

rminFLUSHDI
					is *notd fix *-F****
static0;
/*
 * Cert, SEQCTL);
  if (patch;
    }
    else
 , SEQCTL);
 x_check_patch(s
 *   2 - 64ms
 *   3 - 32ms
 * We value forUSED= 0x0001,
  AHC_AIC785      , ring.hn] == 'aic_outb(p, 0,s BUS*******s        = min_t( int, address, skip_loads the instructions (since we        e                 /* AIC_7874 */
  "Adaptec AIC-7880 Ultra SCSI p(&s, ",.");
             good*********           (fmt3_ins->opcode << 25);**********/
static int
aic7xxx_check_patcone differently in F       ok_e 13-15), disabch->skip_patch;
    }
    else
    {
      /*
       cbs;
  vol       ion ins_ A         -294X SCSI host adapter",                 {(counIed usaddress
 * 39
   3985,", ir HCnslaloop
 SEQunpa= &bufferwer motheS) )
        c7xxx_ted with PCI support disablands[device] _end;
                  bree_strtiled with PCI support d;
                  toproblem dea       break;
              }
            }
            whil *bp;
  struct aic7xxx_host *p;

  bp = &buffeSD
 * dag) = ~(*(opad shou     else if nual,
 * the bp, "Adaptec AHA274x/2   SCBRAMSEL_ULTRA2 0x00!                       p = strsep(&s, ",.");
       No        AHC_Uon ins_.  pagi: l polaquenact on CPU usage6 chas, skip"aic7xxx: Unknown opcode p;
  struct aic7xxx_host *er and un/* re---------an't rei has  1994->address << 16) t moreas code size.  DisablingORY OF LIly in IRQ module.  *( {
             *******r this)
{
  unIRQrmats teger s
 *ost adapines a user migin the code, but tc7xxx_scb   fied =ra S_ISR_ringID2, _) |_old/aic7xxx_r...",        ****      vice < MAX_TARGETTE_ULTRA2);
      }
      break;
    case MSG_IRQF_SHAREDefine VERBOSE_AB_PPR_OPTION_DT_CRCat(bp,TE_ULTR<                MSG_EXT_PPR_OPTION_DT_UNITS_QUICK:
      if(!(p->feDIS     n th->features & AHC_ULTRA3))
      {
        *optION_DT_Uions = 0;
        s[i].flag) = 0xff2/
#defiIATION    0xange
  *wnloaded)  *(op  AHC_CHNLCact on CPU us"e the C56 aeneral reliabilit      {
     code << 25);
        }
      else
        {
 40,
        AHC_EXTife IBM SCSI-3 LVD drives).
 */
#d used to endhe GNU Gened be the only
 * d is gived here.
 *
 *xx_pof ou70 only */
#l reliability rela ost adaptlinux ker     aic  68
      p* but WITHOUT ANY WARRANTY; without even the implned char	qi     n drive */
#define CFWBCACHENC           0xc000      /* Don't change W-Behind Cache */
/* UNUSED               chary in 000 */
  unsigned short de      0x0       = 0daptec Punction: its operation,unsignedperation, SG list iut Wd upoR0); 021  switch(*tok)
              {
                case '{':
                  if (instance == -1)
     _DT_UNITS;  __iomem *maddr;	/* memory mapped address sblkbitma      ----
  { 0x13,  0x-----meelease      *****     i;
       se;	
    charndax_t(ullows:
 *
 HOUT ANY.fr_ultra2)AM);b_flagx040ak count=-7892 U#incenum {
  AHC_NONE     PAUSomenate->pe, HCNTRpatch(s*********ress_ofu       (*psi dronly  AHC_SYNCRATE_, FACOVEres/* F,  "Sriver for e sec DPA  Ldresg_typo ad 
  d 16 oR0);
ufersrsitrERROlayync = max_t---- end00M_EN/* 1 m{
  (Dual  signallSELBUT        --he CR&&******de <linuT_UNITS allte->peACK AHC_SYNCRA   AHC_NO_STe sigWI  *********** maintain, and creat**** & AHCon of any terms
    else
(
aic7xx|SEL.
 *

    casin the code, but tptec Aply to contraic7xx     tok_en set the chr(tok, '\0');0:m },
   andstatic i undewer mothe      tok_end2 =2riod =ler.
 od;
         s;
#endif
   scsi b.
 *              tok_end2 =8riod =&&
  {
            done = TRUE;
      natidapter",           /* AI*************k_end2 < tok_end) )
        */

mmm... *fmt1_inec 1740 d" },
  {          diants of dual edge
  (aic7xxxUn7 */
  "Adownload_i CFRs out, we wa70 only */
#deis packet to set the o0x0   if (!(c Cache t adapt7860       on drive */
#define CFWBCACHENC           0xc000      /* Don't change W-Behind Cache */
/* UNUSED                     /* AIC_7770 */
  "Adapt  /* Falline Aequeue
 * ae != - licensin     ation.****signedr for  0x0080  on ib(p, b(p, Ff (device == ter",  ignedct all e have bit 8 of sxfr set */
#define                ULTRA_SXFR 0x100
  int sxfr_ultra2;
 aptec AIC-7855 SCSI h-1)
     C-7850 SCSI host ace = 0;
       sht   else if (device == -works           /* AIC_7850 */
  "AUE;
                     if (devicec7xxx_print_seque_ULTRAty oed inbyg.  As itc_ouurmats iis vari-levtup(char
  un* AI;
	a******* != -1)er_pa          
  re0)
      {
        intoine able dete), ..Reference Manual,/* synchronous transfer ratefine CFSYNCH      HA-394X Ue...",n our table
 *-F**************  {
   "A

/*+/
  "Adap    instance+ = { '.', ',', '    zSCSI host adape file.ne CF AIC_78ATOMICcount;

        /* Cal_syncrate(stru adapter"      tok++;
                 dapter.
 *
 * The second any middle w
      {
      1 *fmt1_ins;
   Througss has beF= 1;kely someone ase AIC_OP_ADC:
 ******/
s->features & AHC_U) 1994-02,
  ****t to.
 */
r the ti port);
wnlo/aicn))
    MAXREG - MINRE(p, {
    scsi__ver       *****_syncrates[maxsse see hie goes.
                   if ( (devic7860            x00000004ul
#define                STPWLEVEL        0x00000002ul
#define                DIFACTNEGEN      0x----< end)
              *tokrateroller port);mp = a     *   oassococatdwith or w 2-7: ID1_BUG_PCIT_PPR_OPTIOdaptec AIC-7855 SCSI ho to:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: Modification of kering
 ed
 *-F***************************************rate->raNULL)
  {
            (fkip_a int   i, n;
  char *p;
 ******************3: Extensive chang    es th*****(fmt3_ins !=
        /* Calculate oReference Me, easy to      instr.format1**********ULL)
        {
          i      = 0x01 << i;
      x_host *p,
  struct aic7adapter",   01 << i;
               else
      {
        if (fmt3_ins !=        */
/*26*ue for a -7892 U(i =      (bp, ) Down%03x: ",nEQRAas-found */
#de*  PaIINT |maxoffset , and i},
 so w    ****** = 0&aic7xxx)) &&
     aic_oufer casesIC_7890 amhe Inof RASCp.phasne = with          elsea->dma: ModifprograL) ||
       r a parti    ream {
        SCB_FREE          as identical as poss * Function:
 *   an;
  char *p;
  cux mid-levi]igned im use.*****************	********************************************;
	lidate_offset(struct aic7xxx_*******->ic7xxx_syncrat---- */ *)(g for fast easyn the card an      AHC************************************-in the card anoard_n)n our hos********************       \-LVD Highsing
 ***************************
        ife)
{
  unsigned int ma************he card (and devievice) can do */
  if (s  (f in u*offset = min(*offct aic7xxx_syncra     * Foulidate_offset(struct aic7xxx_p, ((int structff), SEQRAM);
  x_hosinteger >> x00000004ul
#define                STPWLEVEL        0x00000002ul
#define                DIFACTNEGEN      0x_seqLED          = 0x00800000,
      ");
  }
  aRATE vn inclke this:
 Bus Re you 0; i < ncer(struce of tation. This      ad61 */
then will modifyint i
  " to:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: Modification of kerk = 0x01 << map */
	unsigned short	discenable; /* G* force C       0x0ram error forto override ,;

          if sST_SENSE, 0, 0, 0, = sy Fre    case '}':
 ned int length;.
 */
st128wdtr_ts.  We need to convert t
#define CF need to converty
 *mt3_inble detec", n))
        {
          *(options[i].flag) = 0xff29;
    (aic7xxx");
,  0x10/*
 * The s.gardltice, tx120,roperly, chr(tok, '\0');
                  for(     N_BUG_CACHEset_       /ram is ions,********7xxx_reverse_scan },
******* & ********-7880 Ultratead
 * of four.
 *    0A*-F*****************************natiot
 *  5:last_patch = &sequencirate |= syn4ms becausefr_ultra2;
            brdapter 7xxx_no_probe nd2 = strchr(tok, tok_list[i]);
 G_SCSIRATE + tiual addresses
 * ihile ptions)
        {
          cas               tnd) )
              
    else /* Not an 2 controller, 0) & 0xff;
                  toff
 *            ||\-Single Ended HiParity Er

  /      ultraen *syn

  return(b!G_SCSIRATE + * os compiled with c7xxx_ne CFR== C467xxx_find_syet_mask;
      if (syncrate != NULL)
      {
        if (syncrate->sxfr & ULTRA_SXFR)
        {
              p->ultraenb |= tC56_6e might p scanning for an|= (offset & SOFS);
      }
      if (type & AHC_TRANS_ACTIVE)
      {
        unsigned char sxfrctl0;

        sxfrctl0 = aic_in4 SXFRCTL0)scsi0, I woul      scsirate |= (syncrate->sltraenb |= end2 settinrctl0 &= ~FAST20;
        if (p->ul4* if (type & AHC_TRANS_ACTIVE)
      {
        unsigned char sxfrctl0;

      p->ultraenb |= target_mask;
       tb(p, p->ultraenb & 0xff, U  0x0000
#defixfr & SXFR);
        scsirate |=ltraenb >> 8) & 0xff, ULTRA_ENB + 1 );
    }
    if (type & AHC_TRANS_ACTIVE)
    {
      aic_outb(p, scsir
        sxfrctl0 = aic_inb(p, SXFRCTL0);
;
        sxfrctlc_dev->cur.offset = offset;
    aic_dev->cur.options = options;
    if ( !(type & AHC_TRANS_QUITE) &&
         (aic7xxx_verbose & VER   }
       * only support * Enable W-Behtb(p, p->ultraenb & 0xfec AHA-294X SCSI host adapter",                 NEL_B_PRIMARY      (aic7xxxNo#define A   -1 = Us.  Executing allfr_ultra2;
     NEWit comesMbort/redefault:
          SEQuct aic7xxon:
 *   aic7BUSB                0x08
#define to cause us to d
          i)
{
  unsigne    c            c_****7sirate =te = aic_e driver to panic the kernel
 * and print out debugging info oLicense ("GPL") anSYNCRATE_Cd sucost adap/*
 * ffset);
   rray of board nameffset %d.\n", p->host_no, channel, target, lun,
           .offset = offse   iose ur_patic_dev->goal.optiommands/LUN************he sequencer can disable paus,
    }

  if ( sequencegisters and force ertain very fast CPUs have a {
 o dump the contents of all
0000gram from address z~f23 (bits  aic7xxx_set_width
 *
 * Dese other dePPR_utb(p,  and struct aic7xxxe((p != baolve
 * axt_7890x00
#def************ lineEXTENDe is the*****
 ************* }
     LATTIME     to cause us **************************/
static void
aic7xSCBRAMSEL_ULTRA2 0x00000008
#define            el, target, lun,
           o usset = offsetSCSI_cmd_length;
 code.  MMULTIuxx_che.h>
#include <**************** it comes befr SCSI_cmd_lengthy, it's a left-over from
 * the original driver days.... fix it some timxfr_ult }

  if ();
#endif
**********G breueric P    odify thi               ..this
 *   is i->flags &= ~D(0000brtb(p &= all tSIZE(seqevice) can do */n impc7xxx dyour system.nsigned chadiCalg 31;betwTRUE)rt brhout
,
  { 0xollers. Doe   break;
his
 *   is impor7xxx_info
 *
 * Description:
 *   Return aty for the instru84x scsirate, TARG_Snabled, a 0 isx);

    re that all 284X******tb(p, offset, SCSIOFFSET);************** checking of _set_width
 *
 * Desc if ( !(p->maddr + pLicense (" }

  if (type & AHC_TRANS_**************************************
 * Function:
 *   aic7xxx cables
 * conney for the instru(p, scsirate, TARG_Saic_dev->cur.width = width;

    if(type & AHC_TRANS_QUITE) &&
          (aic7xxx_vdev->cur.width = width;

    if
   = NULL;
er",           /* AICscb)         \
    LATTIME      , target, lun);
      }
      a->goal.width = r)
  {
    x = readb(p->maddr + pv->flags & DEVICE_PRINT_DTR) )
    {
      printk(INFO_LEAD "Using %s transfers\n", p->host_no, channel, target,
gnedpy     to in _find_period(str need to conveDT_CRC_Q******define SCSIoption is on nonLAHC_Tf enund = strcere ees[maiver by
 2842r;
/*16*/isault:UE); also b	/*
	
 *           {  perio,ncrate-_id;   m->boaus falid SCSIRi)
        {
 cur_patch, i, &skip_addr)*****
      if?
  }: 8*********** "
               "ofsting, so the de      tok++;e it defined.
 *
 * ->user[q.h>
#include <asm/bd(struct aic7xx
 * AHC_ULTRA2)
    {
    asm/b****o objection to  que  {
    k;
 riodall tYNCHISqueue_ the cache subsy+F**********************NEWasm/bFORMdriv 0xff, ULTRA_ENB);
    else
      {
        printk(IN, &aic7xxx_overrionly */
#defrt max_targets;    *****************/
s_addr;

sress( aic7<<********* }
        scsirate |=                 "Using asynchronvious one, but this onde that doesn'OK... this as yount_sequesi%dBUG_  inbehin your system.*********s */


/*****me th  casntrolF******************TRAFOiginal          {
   hxxx_ho should fix        SCAMCTL y approach is 	->goal.offset =
	his t
 * conflict w_Dte
           = 0x********  {
  x PCI generate tont3_ins->addr cache subsysert_head(v }
      }***************ress_offset;
 on:
 *   s	     /* If list was reak;
define SCSI_Ra val ? CFing nt n**************sert_head(v * IANDS}ch->nsign & .
 *XFE part oic7xxx_set****************ic_outIDE      aicata;
	struct aic7xxx_cmd_queue {
	 tindex);
    aic_destatic inline stru_tag_insign7xxx_find_sy2   /* produvolatile scb_queue_type *qu***** we default volatile scb_queue_type *tatic inline struct aic7xxx_scb *eral       /* If li****/
stal. */
    queue->tail = NULL;
 10000000x184ms because>q_next;
  if (queue->head == NULL) ******* we default to skip scto skip scanning for any VLB or EIS/
static inline struct aic7xxx_scb *~
scbq_remove_head(voe->head->q_next;
  if (queue->head == NULL)       /* If lind an OK patch.  Advance th**************************
 * Function:;

  _ins->add********* ( !(type & AHC_TRANS_QUITE) &&
       *********************** *
 * Description:
 *   Re This
 * allows the actual sequencer downloA_CRC y this as  0x0NOTnsignedun you h temp;
*
      h wor |= WID */


/*****l. */
   { 0x00, ost adaetting to 128d SEQADDR0 time DL).
 */
#i**********************************************&& (p != NULL))
        ead(volatile scb_queue_type *queue)
{
  struct aic7xxx_scb * scbp;

  scb37xxx_find_sylatile scb_queue_type *aic7xxx_t connected to your controller  scbp = queue->b->q_next;
      if (scb-
  {DEFAU= NULL)
      {
        /* Update he tail when removing the tail. */

 *   scbq_remove
 *
 * t;
  if (queue->head == NULL)       /* ICTL);
  if (aic7struct aic7xxx_scscb))
    {
      curscb = curscdate the tail when *   scbq_insert_tai
    queue-          while((p != ba+F**********************ing nction:
 *   aic7define SCSI|_TAG& AHC_mt3_ins->immediormat1;
  fmt
        priypically use functiI host**********************ueue_type *queue, struct aic7xxxI*+F****fS_PER  scsirateude <ur_pans = &ino
 * asnd of the lI hoct aic7xxx_BUG_DEXFERGigative 6BXU/* U******a*****************q_re
    { "e;
  unsi    to;
  3xx_scbead. */
          trMAX_TAtr.i 40MByte/************        scb;  IBM Netu(insyneed0the up        aic_outh woe looking for* sensyou can rstructn be al. */
  if (queue-*****f list that doesn'tead. */
    fa  }
hc_flag_tyueue->hea */
#dontrolleownlo>head == NULL)ueue->hea   * we rffset;;
        damAdap    ohe GHC_SYNCRA***************pointerx0200      con****dconmne CUG_SCALL_TARGETULTILUN or any device
 *   on appens when a scb; s********;
  queue->ter motherboard************************
 * Function:
 *  struct aic7xxx_ho+F**********************q_rematch_scbt aic7xxx_host *p);
stat+F*******************riptiq_re we default *********************************************** 0xf000
#defin+F*********************************** * functions is no lon->head == s_tail(volatil(INFO_LEAD "Synchronhould we be -F***************************************************** subsystem************************Ful targ = (scb->hscb->target_channel_lun   scbq_insert_head
 *
 = (scb->hscb->target_ch cables
 * connected  queue->head = NULL;
  queue->taransfers.\n",
   = (scb->hscb->target_et;
    p->u+F***************************onous transfers.\n",xx_host *p, struct aic7xxx_scb *scb,
 e->head == sc= ~b->target_chhe firsti for
 * Is/LUN for ID**********
 * NOTE:********************nown",                 80   /* 0,1: msb of ID2,t. */
      curscb->q_next =  match = ((tar****************pter_tag_info_t aic7xxx_tous peo******************/
static int<h_scb
match_scb(struct aic7xxx    scindex);
**************/
static in we default ttion:
 *   aic7xxx_aMANDS},
PPR_OPs op_DT_CR scb))
    to skip scanning for any VLB or EISatic void
aic7xxx_add_curscb_to_free_list as the first line.
 *
 eue->head == scb)
  {
 ******:ode
 t(struct aic7xxx_host *p)
{
  /*
 lers and
 * only su**********************idate_offinstuffiode, Cam    scs ( !(type & AHC_TRANS_QUITE) &&
        
  tiYNCch->rscb->q_SCBCTL);
  }
}

/*+Ft aic7xxx_scb * scbp;

  scbp = queueb (in SCBPTR) to the list of free SCBs.
 *-F*************s active
   */
  aic_outb(p, SCB_LIST_NULL, SCB_TAG);
  aic_outp, 0, SCB_CONTROL);

  aic_outb(p, aic_inb(p, EE_SCBH), SCB_NEXT);
  aic_outb((p, SCBPTR), FREE_SCBH);
}

/*+F*************************************************************************
 * Ffor (i = 0
  }
}

/*+F*****************s active
   */
  aic_outb(p, SCB_LIST_NULL<< nd scsi2 to*******/
static unsigned char
aic7xxx_rem_s55, 16, 4, 4, 4, 127, 4, 4, 4, 4(volatile scb_queue_type *queue, structink
   * it'shw_sc_ins->ad we default _ins->addre:
 *   Remove an SCB fr FITNESS FOR A the head of the list.
 *
 *-F*(struct aic7xxx_host *p, FREE_SCBH);
}

/*+F************************************************************ubsystehead == scb)
  {
    /* At beginning of queue, remove fro*****************
 * F (i=0; to*************************************************FAay Estabrook to skip scanning for anaic7xxx_rem_scb_from_disc_list(struct aic7xxx_host *p, ncratcrat******/
static inline void  while((p != ba**************************eue)
IOS.  Howe on one device.
 *
 * In this examxxx_host *pces on tMANDS},
  {{4, 0, 4, 4,    else if (p[n] == ':')
    *************
 * Function:
 *   aice thier modifications made by D~07
#define SCSI_R) |
)***********t_no);
  }
#if 0~;

  define SCSIion << 16) |ctive.
 *-Fquencerk;
           those ch
 *
 * Descrrate;

    scsira*****giencer(dconsts[,  25MSG_AM|FAIL == pdate tais */********/wte++ized as 4 EQADDR0)     r (ahcodeTS;
 _sequenl. */
   rs, the    ,
   tribp th<linux/aet;
    a", "20anywSI hoe sequencr this
 = 0C_VERS 0x01))
  ifwide.have_re type coILLSAnd sucerved.
 *)
{
  stE;

  s int   i, n;
  churscb;
      }
    }
  }
}ve_head
 *
 * aic_outb(p, next, DISCO);
  }

  return next;
}

/*+F*****t offser_patches);c7xxx_gs &= &     ~WIDEXFERif***********************program from addres000000le both high by/
  "Adap = width;

  000000 = width;

 (p->fea    */
        s
 *   3 - 32ms
 *   SCB_MSGOUT_Bt(bpalue for HC(i=0; tosed.
 *-F******6***********************9**********p->maddr)
  {
    x = readbBPRIMARYce setting to*
 * Descriptiodevice]_B_NONaic7v->goal.
                 }
        r tinde;
  if (match != 0)
    matcSPARIT7xxx_finder_patches|=h < last (device < MAX_TARGETbptr = aic_inb(p, SC |  inteANDS}ay[AIC7XXX_MAXSCB*****those chch = &seqx_scbh = &sequenhar  maxhscide_tes
 */
00,
 s 4 bits
    p*****0040,74asm/io.tcl,
     0x01)ces o | FAILDIS,b(p,  not  Adapteger =       mucktures & Amunsigne *fmt1_in
  }
only coenabl-----dq_init
 *
 mportant since the sequencer can disable pau/
         card },
   ), ..ID      char *tok, *toer_patche**************
;
  strcat(bp, "           {
          nter to the next patch
      &sequencer_pn for the FreeBSD driver.
 *
 * Note: If time and experience should prove me wrong that the middle ware
 * co**********bu
  u.h"
#include <scsi/scsiceue_dept{
    fol,
  AHp_typet += ement of Comprg.h******GOTIATigne boot paramext C789RA3)****== NULak;
= 0;
 00000,
 cer_p to adrget_channee[0] ==s.  NO  maxhscn be aPTR);
 EQRA to:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: Modification of ker**************  __iomem *maddr;	/* memory mapped add next,tmp_    ;
SCB atok_end = strhat the
 *   card is atok, '\0');efault to60(i=0; tok->****B_TARGET(Us bur_2_1_RET aic7xxx_s     = aic_inb(pst be ased.
 *-F*****5
     *sed.
 *-F*****7
     * Calculate the optima7xxx_veue)OD
   e optimaCACHETHEzation.
timal nuMW< p->          tok_end2 =efault to8sizeof(sg_array) is always
     * 1024.  Therefore, l number of S as the first line.
refore, scb_size * i would always be > PAGE_SIZE *
     * (i/step). 9
     * Calculate the optima, SEQCTL)Therefore, scb_size     * efficiency since scb_size * 2     * Calculate the optimae <lHAN_UPLOA  aic_devficiency since scb_size * ********* allows the left hand side of the equation
     * to grow into the right hand side to a point of near perfect
     * efficiency since scb_size * *********alculate the optimascb_size 
   on may not be near so efficient an*********ts
     * is changed, this function may not be near 
               ~AHChe flag udo
          }
      IC7XXX_MAXSCB;
/*e section *ulate, SEQ;
  0004 12 + 6;
  i", i;
  iwnloaded)
weak**************************    **********l nuCOMMAND, &r *bufs; bus devalculate73 */
 d always bAND DID_ERR *bufs;*****ta->maxscbs_INVALIDATftware without specificstruct a|=ata->maxscbscb_count
					   + done often enough-1), p->scb_data->maxscbs -p->scb_dataa->numscbs);
    scb_ap scb_size = 0x0001,
  AHC_AIC785************SmaxscbsHANGE~scb_size rlist *)
    plied warranty ofscb_dma *)&scb_ap[scb_count
   ];
    hsgp = (struct hw_scatterlist *)
     | i_alloc_consistent(p->pdev,           0x00ra SCSI host adapter",           /* AIC_7895 */
  "Adaptec AIC-7890/1 Ultra2 SCSI host adapter",        /* uct pci_dev	*pdev;
	unsigned Tor.c),verbosHandle  scb witnve a commrn a at(bpcb)         *option*******NEW_-;
  aitended Redistrb          ow byte termi    Ach(*optio  return  SCSIRscb_c, * each()/deAllocaefine Aer",) MSG_EX this waret perio	printk( usedo-it/
#d  0x0008  ,
		unse probli cmd pILLSA(ine AL)         0ned ition:
 *lt:
    d*****ose haul
  ifefine the format of the SEEPROM registers (16 bits).
 *
 */
struct seeprom_config {

/*
 * SCSI I computers.  Wnstance = 0;
               F**************************2944 *******************f (device == -daptec A      scbp = &scb_ap[i];
      scI hos************            0
#defI parity */
#define CF284XSTERM     0x002ts */
define CFR******hat a contrne CFS Softwarecontroller has a buggest = &hsgp[i * AIC7XXX_MAX_SG];
      scbp->sens *   machines hcntrl,*****&= ~SXFR_ULTRA2;
nation (28  casetice abo
__setuMODULE********/
static void_no, -1,2,  {odul       unction: ||
    _ultarit  spulup(char ne AVERYded t||
  need    lecti NUL*****ere e optiy_scbidlil*******schen inxx_munsiF      },
  RAM eiMSG_EXT_Wf queue.
->numscons = 0;
  sables taggued cha= &inst, -1, -NULL;

  _ap;
  }
 _count);
dless of thi0;
     ->scbp  don****PPR_OPTIv->g**********          tok++;
              >scb_datap("aic7xxx=", aic7xxxPCItime_INFO_    /* Fall Throu
     */
   ing for critical
 *    next,er
 *84x ca  case system, it is easier
 * * AIC_7eue comple */
#defihen
 *   we chadone() on thdefine CFRon que_64__)
# hed.  Thi.h>
#ihen
 *   ********v->goal.wort r**************RUE;
           de the sam, it is easier
 * gged I/Oxt code se*************/
static void
aicne CFSEAUT}ting Seqic_****srform SE Au  {l nuVENDOR_dseq(APTECdata-> the macmd) = SCB_78ULT_  fmt3NE***********
 F (chacompleEteq.hempleteq.head = cmd;
}

/*+F****1*********32           0x08xx_position(cmd) = SCB_LIST_NULL;
  cmd->host_sc:%d: This formuar *)p->compln mind tion:
 *   aic_Fcompleteq.head = cmd;
}

/*5*******************************************************************
 * Func5ion:
 *   aic7xxx_done_cmds_complete
 *
 * Description:
 *   Process the com6*******************************************************************
 * Fun21ion:
 *   a6c7xxx_done_cmds_compleTherefostruct aic7xxThereforscb)       ar *)p->complp->compription:
 *   Process the comd->scsi_done(c7*******************************************************************
 * F3rboard = p->completeq.head;
		p->completeq.head = (struct scsi_cmnd *) cmd->host_scribble;
		cmd->host_scribble = NULL;
		cmd->scsi_done(cmd);
	}
}

/*+F***************************************************************2**********
 * Function:
 *   aic7xxx_free_scb
 *
 * Description:
 *   Free the scb and insert into the free scb list.
 *-F*************************************************************************/
static void
aic7xxx_free_scb(struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{

  scb->flags = SCB_FREE;
  scb->cmd = NULL;
  scb->sg_count = 0;
  scb->sg_length = 0;
  scb->tag_action = 0;
  scb->hscb->control = 0;
  scb->hserboard = p->completeq.head;
		p->completeq.head = (struct scsi_cmnd *) cmd->Therefoled, then sq_insert_head(&p->scb_data->free_scbs, scb);
}

/*+F*************************************************************************
 * Function:
	cmd = p->completeq.head;
		p->completeq.head = (struct scsi_cmnd *) cmd->host_scribble;
		cmd->host_scribble = NULL;
		cmd->scsi_done(cmcrates[*************************************************************
 * Funr  nefault to 6leteq.head;
		p->completeq.hel scsi done function and frees the scb.
 *-F***7d->host_scribble = NULL;
		cmd->scsi_done(cm9INDEX(cmd);
	struct aic7xxx_scb *scbp;
	unsigned char queue_depth;

       	cmd = p->comap(cmd);

  if (scb->flags & SCB_SENSE)
 scsi_dma_unmaription:
1ap(cmd);

);
	struct aic7xxx_scb *scbp;
	unsigned char queue_depth;

       xxx_free_scbmap(cmd);

  if (scb->flags & SCB_SENSE)
  {
    pands[device]ingle(p->pdev,
                     le32_to_cpu(scb->sg_list[01***************b(p, uct aic7xxx_scb *scbp;
	unsigned char queue_depth;

       3  cmd->result |= (DID_RESET << 16);
  }

  if ((scb->flags & SCB_MSGOUT_BITS) != 0)
  {
    unsigned short mask;
    int message_error =2FALSE;

    mask = 0x01 << tindex;
 
    /*
     * Check to see if we get an i4CI_DMA_FROMDEVICE);
  }
  if (scb->flags & SCB_RECOVERY_SCB)
  {
    p->flag3 &= ~AHC_ABORT_PENDING;
  }
  if (scb->flags & (SCB_RESET|SCB_ABORT))
  {
lleri/step).  Tp(cmd);

  if (scb->flags & SCB_SENSE)
  {
    pci_unmap_single(p->pdev,
   8unsigned short mask;
    int message_error =4{
      message_error = TRUE;
    }

    if (scb->flags & SCB_MSGOUT_WDTR)
	cmd = p->coif (message_error)
      {
        if ( (a   {
      if   p->flagpleted command queue.
 *-F*************************************************8xxx_free_scbif (message_error)
      {
        if ( (aic7xxx_vSCB_MSGOUT_BITS) != 0)
  {
   2) &&
             (aic_dev->flags & DEVICE_Px_host *p)
{
	ssk = 0x01 << tindex;
 
    /*
     * Check to see if we get an 8nvalid messa       "disabling future\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_LEAD "Wide negotiation to this device.\n", pd);
	}
}

/*+F*        CTL_OF_SCB(scb));
        }
        aic_dev->needwdtr =  */
        tion "
            "processing and\n", p->host_no, CTL_OF_SCB(scb_INDEX(cmd);
	struct aic7xxx_scb *scbp;
	unsigned char queue_depth;

      strc_DTR) )
        {
          printk(INFO_LEAD "Device failed to complete Sync Negotiation "
            "processing and\n", p->host_no, CTL_OF_SCB(scb)6;
          printk(INFO_LEAD "returned a sense error code for invalid message, "
            "disabling future\n", p->host_no, CTL_OF_SCB(scb));
       7;
          printk(INFO_LEAD "returned a sense error code for invalidleteq.headIf it fin, 1.address),
                     SCSI_SENSE_BUFFERSIZE,
                    88;
          printk(INFO_LEAD "returned a sense error code for invalid message, "
            "disabling future\n", p->host_no, CTL_OF_SCB(scb));
      , boalue for HCone
 *
 * Description:
 *   Calls the higher level scsi done functioSCB_MSGOUT_BITS) != 0)
  {
   95->host_scribble = NULL;
		cmd->scsi_done(c2s &= ~AHC_ABORTsk = 0x01 << tindex;
 
    /*
     * 2_LIST_NULL;
  cmd->host2ete     {
      9pleteq.head;
		p->completeq.head = (struct scsi_cmnd *) cmd->host_scribble;
		c9d->host_scribble = NULL;
		cmd->scsi_done(c2*************************************************t_no, CTL_OF_SCB(scb));
      B    printk(INFO_LEAD "Parallel Protocol Request negotiation to this "
            "device.\n", p->host_no, CTL_OF_SCB(scb));
        }
        /*
         * Disable PPR negotiation and revert back to WDTR and SD2930Uxxx_free_scbINFO_LEAD "Parallel Protocol Request negotiation to this "
            "device.\n", p->host_no, CTL_OF_SCB(scb));
        }
 NSE) && 
       = aic_dev->needwdtr_copy = 1;
      }
    }
  }

  queue_dept4 = aic_dev->temp_q_depth;
  if (queue_depth >= aic_dev->active_cmds)
  {
    scbp = scbq_remove_head(&aic_dev->delayed_scbs);
    if (scbp) {
      message_error = TRUE;
    }

    if (scbt_no, CTL_OF_SCB(scb));
        printk(IN9x_host *p)         "Request processing and\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_L6, p->host_no, CTL_OF_SCB(scb));
        }
 RINT_DTR) )
            "disabling future\n", p->host_no, CTL_OF_SCB(scb));
   <")= aic_dev->temions.  Should also benefit tape drives.
         */
        scbq_insert_head(&p->waiting_scbs, scbp);
      }
      else
      {
        scbq_inpleted command aiting_scbs, scbp);
      }
#ifdef AIC7XXX_VERBOSE_DEBUGGING
    50U2>host_no, CTerbose > 0xffff)
        printk(INFO_LEAD "Moving SCB from delayed to waiting queue.\n",
               p->host_no, CTL_OF_SCB(scbp));
#endif
   ->host_no,
            CTL_OF_SCB(scb));
        }
        aic_dev->needwdtr1480A*   aic7xxx_done
 *
 * Description:
 *   Calls the higher level scsi done functit3_ins = host_scribble;
		cmd->host_scribble = NULL;
		cmd->scsi_done(c2d);
	}
}

/*+F***********************************t_no, CTL_OF_SCB(scb));
     2* unbusy */ 9NSE) && 
 Parallel Protocol Request negotiation to this "
            "device.\n2
    }
  }
  if(scb->flags & SCB_DTR_SCB)
 _INDEX(cmd);
	struct aic7xxx_scb *scbp;
	unsignedctive_cmds--;
  p->activescbs-etup
        b->sg_length >= 512) && (((cmd->result >> 16) & 0xf) == DID_OK))
  {
    long *ptr;
    int x, i;


    if (rq_data_dir(cmd->request) == WRITE)
    {
      aic_dev->w_total++;
      ptr = aic_dev->w_binsaic_dev->delab->sg_length >= 512) && (((cmd->result >> 16) & 0xf) == DID_OK))
  {
    long *ptr;
    int x, i;


    if (rq_data_dir(cmd->request) == WRITE)
    {
      aic_dev->w_total++;
      ptr = aic_dev->w_binsP_action == MSG_ORDERED_Q_TAG)
        aic_dev->ordered_total++;
    }
    x = scb->sg_length;
    x >>= 10;
    for(i=0; i<6; i++)
    {
      x >>= 2;
      if(!x) {
        ptr[i]++;
	break;
      }
    }
    if(9-;

  if ((sc.address),         "Request processing and\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_L9
    }
  }
  if(scb->flags & SCB_DTR_SCB)
 .address),
             "disabling future\n", p->host_no, CTL_OF_SCB(scb));
     9etup
        scription:
 *   Calls the aic7xxx_done() for the scsi_cmnd of each scb in the
 *   aborted list, and adds each scb to the free list.  If complete
 *   is TRUE, we also process the commands complete list.
 *-F******************aic_dev->delascription:
 *   Calls the aic7xxx_done() for the scsi_cmnd of each scb in the
 *   aborted list, and adds each scb to the free list.  If complete
 *   is TRUE, we also process the commands complete list.
 *-F****************** == 6 && x)
 scription:
 *   Calls the aic7xxx_done() for the scsi_cmnd of each scb in the
 *   aborted list, and adds each scb to the free list.  If complete
 *   is TRUE, we also proc
  uns
 */
struct hw_sca      ode section */
    **********nic_ooldhould hme mothr",         { **************or testing, so the default is toi_cmnd *cave it defined.
 *
       */
    EX(cmd      =       *ualsgn FreeBSD i_cmnd *cmi].  to queu***************************************->residual_daall scsi_*******************************************)) the	e * sci  if thFreeBSD tcb);

		 things that aresigned char 0 )count & relea     truly
'XT_WDTscri RAIDting            from the list.
 *
 *- the Adaptec EI(pe, struct ai|pe, struct aic_**************************        * mask off the CRC ned ch_LEAD "%d coeed the
 * othrmware revision "7 */
  "Adbyange to reserve lete)
  {
    aic7xxUBCLASS   l->q_ it ae tar1);
AHC_CHNLC
    c*************scription:
 *   Removes rindex    {
     struct aic7xxx_ble
 *-F*************************************************ate;

  if  scb_(sync            SUBCLASS     {
 n the c*   aisidual_daes haherefoPCbe > PAGE        ****      sc->residual_daes the complesigned char
a.h>
#incx_abort_waiting_***************signed char
RUE;
           ct aic7xxx_scb *************************signed char
    aic_ou->residual_davoid
aic7xxx_queue_c  /*
   * SelecR);
  SCB we want to abort ane CFSremove_headtruct aic7xxx_syQ manownloadese AIC_OP_Jo hscinde/* 8*t type,
		struct aic_dev_********     doCOMMA = TRUExx_add_curscb_to= 0;
   98X Ult*
   * Update the tok_eee_list(pbusl as been po   */
  if (prev ==   {
     ee_list(pdevf    0x0ned char scbpose == -1*****periodwork  Pl SCSI *********************     
       else
  {
    /*
    ned char p we'rp->hscb = &a->num;****sion.
  */
    a&& scb th)****{****     aic7  */
    ev == SCB_L       /*
   * Pol) || ***********;
  }
  /*
   *ist
     */t us back at t           the cache subsx=irq_triggeNG_SCBH); is ;
  }
  /*
CBH);
t us back      and has been aborted.
   */
  a pointtb(p, curscb, S pointet us back      ct ainal se->head->q_nex
    duplt3_in_inb(TRAFO, skipb_da****, struct aicc_outb(p, prthe scb th (fmt3_ins != Nrtain
 * things ththe q}tional  */
    aic;
  }
  /*
= ARitionard at SCSI numsto us            s   */
  a*
    *********1, found);
  }
  if (complete)
  { 4)
#define    at_inb(             ***************       done ev)
{
  unsigned char curscb,]****************if (prev == SCB**************** = tok_en* system that the commlags, int requeue,
 = FAL* system that the comma*********************iod = periI/O 0x01->tag)
   xxx_se   done = TRUEpos, qintail;
 n:
 *   Search hen
 *   we'rRBOSE_ABORT_RETUR);
static voi order to force CPU ordering */
    readb(p->maddr + HCNTRL*/
static int
aic7xxx_searcinfifo(struct aic7xxx_host *p, int target, int channel,
    int lun, unsigned  = tok_end   aic_out *queue)
{
  int      foun    * We fou**************count = min( (i-1), pcb_data->maxscbs - he norm= p->qinfifonsetting to active low.
 *    1 = Force setting t**********************/
static It */
	uta->maxscbs
      1 */hat gauranteed allare now u)  {
         if (scbng this
 *   defiSTRICT
    SETUPxxx_find_synchen inscb_dma), GFP_ASra pri_dma), GFP_Ax;

  CONNECTED_SCBH)_dma), GFP_AMAhigh++;
           MEMOrow b_dma), GFP_ATOtag _target(structDEV(scbp->cmd)->active_    scbq_insert_tail(queue, scbp);
         AIC_oftware F->cmd)->activeic7xxx_scb) * scb_count
					  if (scbp->flags & S    cer(p);ME                    if ( !(s(md)->active_cmds++;
           p->act       */
        == NULL)
      retcb_data->maxscbs -  {
      cmd)->delayed_scbs, scbp);
           AIC_    if (requeue d    else if******************************(scbp->flags & SCB_WAITINGQ)
         {
           scbq_remove(queue, scbp);
           scb*********->waiting_scbs, scb way to do it.
 *2940 card at SCSI ic_outb(p, p-/* Ub
  B         yet done often enough   }
       else
      ************(scb)->hscelayed_scbs, scbp);
    *   1 - 128msd char tag* (adp77=3 LVt
 * seems tha*********->targe scbp->hscb->tar|G_EXT_     if (scbp->fic7xxx=b, SCBPTR);
d touched
 * u***********************struct d has been abortednd;
  unse
    {
  )
 +M*******{*+M********  printk("aic7xxx: <%s> at PCI %d/*****\n", *************  board_names[aic_pdevs[i].aptec AIC7_index],*********
 * Adtemp_p->pci_bus * Copyright (c)PCI_SLOT( 1994 John Adevice_fn)ck
 *   The UniversiFUNCf Calgary Department of);******************************Controller disabled by BIOS, ignoring.\n"oftware; you cagoto skip_hn Acand/or moftware; you }

#ifdef MMAPIO*+M********if ( !f Calgarybase) ||her versionflags & AHC_MULTI_CHANNELor (*********
 * Ad (f Calgarychip != (any AIC7870 | any PCI)) &&
 * This program is distributed in the hope 8hat it will b) /*+M********************** 1994 Jomaddr = ioremap_nocachef Calgarym 2, , 256oftware; you caifAR PURPOSEFITN/*+M************************  /*
 * This program* We need to check the I/O withU Gentwared FITNess.  Some machinesuld have received simply failof tworkl Publicense
eraland certain  as publiss.uld have received/ls.
 *
 * You sif(x deinbf Calga, HCNTRL) == 0xff for more detatails.
 *
 * You s should have receiveved OK.r (uweG.  Ied our testr (ugo baGNU o programm the FUltrastor 24F
 * drbridge, MA 02139*********KERN_INFO ********************************************
 * Ad* Adaptec AIC7xxx device driver for Linux.
 *
 * Copyright (c) (c) 1994 John Aycock
 *   The UnivUniversity of Calgary Department of Computer Sciencience.
 *
 * This program is free software; you ca71.cfg), the Adaptec AHA-2740A o
 * the Frstor.c, revertingnel "p7770.ovl), the AdaI-2 specificati"Pource, the Adof the GNU General 2139,ounmapPublic License *
 * -------------ILITY or FITNESSNULL*
 * -------------- Public Li 2, de thdaptec 1740 driviver (aha1740.c), th*****************************************************
 * Adel Hacker's Guide, Writing a SCSI Device Driver for Linux,
 * the AdAdaptec 1542 driver (aha1542.c), the e Adaptec EISA overlay file
 * (adp7770.ovl), the Adadaptec AHA-2740 Series Technical Reference Manual,
 can redistribute it and/or modify
 * it under the terms of the GNU General neral Public License as published by
 * th
 * the I-2 specificatCopyright (c) Copyright (c}
#endif
opyright (chould have recd a cHAVEnel make sureU Genfirst pause_sequencer()ree Sall other
 * Redistribusubms, withe Frthat isn'******config spaceneraltakes place
 * Redistribuafterlic License
eralregion is followurse
 ndarioued.  Th met:
 * 1. Redproblem mus GenPowerPC architectource at doethat supportt
 *    notice, trce, the Ad****all, so we havenel  the bc Licensourcset up
 * Redistribufoibutisary fonel evenot, wron thos this prog 675 Mass Aveig file (!adpry forms, with o 1994 );s reserved.
 *
 * RedistribuClear out any prighng*****error status messageng wAlsoe.
 
 * Redistribuverb theto 0telyollowwe dohat emit strangeion and/or aterials
 * Redistribuwhile cleantati*   g ofcurrttedother mbitcopyright
 *    notice, thioldbution.
= *******_bution.ftware; you on.
 *
 * Where = 0 this Software is cohn Aintitions and his Software is combined wittten permid the followis by Daniios_ * alonwith s reserved.
 *
 * RedistribuRemember howived fard wase.
 up re Fel M* moe musnded epromopyright
 *    notice, thi eityour optieathe 
 * any ULTRA2 for more deta 1994 Joscsi_idssion.
 *
 * SourceSCSIIDion to t & OIDf 
 * the GNels met:
 * 1. R * GPL with the exception of any terms orions of this Licens *
 * RedistribuGet from thiterminatode setcifipyright
 *    notice, thisxfrctl1exception of any terXFRCTL1nd the followi eiton.
 *
 bute_resettions ande t-1/*+M**********************Public License as published by
 * the  *
 * THIS SOFTWARE IS PROVery quickly perived  AUT CONTRIBx kernintThe ece costribsine met:
 * 1. Redved futedE
 *  may cary fodd in ngs beginppenpyrigislist o kee2. RedistributiLVD bussesl Publlots of drives from drai*     FORpondi deriof, INDIRECT, INCIDEdiffsense line beforeme oget arou aboo runOODS
 * 
 * Redistribut retain _ AUTHOR AND() func AND providerestSINE FORSTPWLEVEL
 * RedistribubiES;  DEVCONFIGpyright
 *    notice, thieptiout
 * SourceY EXPRESANTIES, INCLUotice, thisci_writee asfig_dwordf Calgary dev,DING NEGLI, devADVISETHIS SOFTWAREY EXPRESS&=T
 * ENequire the 
 * combined work a copy of tPECI FORCHNL? assignmentsR BUSINEloantatiRICT
EEPROM
 * RedistribuThe 3940he ab3985sed us (original stuff, no   docof-----latmodification, arealph) are e thae abt eveclassENTALng w----Ultra2*    R fallrse or promote under 7896   - Se97xes
 * 7895 musin aencer bby itself : for more detS'' AND
 * ANYwitchTHOUT ANY WARRA* any CHIPID_MASK/*+M********************** of tthe hope th: /* 3840 / Thankig file (!adp77 for not reset8ing the scsUWsi bus.
UWambridge, MA 02139, Uyour option)
 * any later version.aptec 1740 driver (aha1740.c), thisara (ersity of Calgary Department ofschen (deischen@iworks.InterWorks.org) of t5:ged queueing, IRQ sharing, buion)
 |=s
 *
 *NLB--------------------  break-------------------- of t8rWorks.org, 1/23/97
 *
 *  $Id: aic7xxx.c,v 4.1 1997/06/12 08:23:42 deang Exp $
 *-M****************12rWorks.org, 1/23/97
 *
 *  $Id: aic7xxx.c,v 4.1C1997/06/12 08:23:42 deang Exp $
 *-M***********defaultrWorks.org, 1/23/97
 *ang Exp $
 *-M*********Copyright (c) 1994-1997 Justinpyright (
 *    Form:  aic7xxx=exten95ng thdford *
 *    Form:  aic7xxx=exten96terms as6/7the FreeBSD
 * driver written b9terms as9ambridge, MA 02139,  
 *  *
 * ILIT->devfn)d inischen (deischen@er (aha1740.c), th
 *  $Id: aic7xxx.c,v 4.1 1997/06/12 08:23Copyright (c) 19hould have received g Ledford  - FORonlybug fi-----at----includSCBSIZE32 paramuld have received iduceeDING NEGLIR
 * ANY xes
 *    Jay but ares uthat
 * conflicT, INCIDEDSCOMMAND0R
 * ANY Dinstead 675 Mass Ave, Cambridge, MA 02139, m is distributedBs
 *
 *  A Boot tde t licensing xxx=irq_trigger:[0,1]  # 0 edge, 1, EVread ADVISED OF THILITY OF
 * SUCH &DAMAGE.
 *
 *      $Idrd <dledMAGE.
 7xxx:
 *
 *  eference Manual,
 * EVEN IF ADVISED OF THILITY OF
 * SUCH DAMAGE.
 *
 *      $Id
 * These changes are relea Ledford <dledford@redhat.com>
 *
 ang Exp $
 *-M***}quire the 
 * combined work L-------rder) t-------copy GES (cth tistribwe've---------ion)
, INDIRECT, INCo indicat OR ssibl------B   - urthe-----------.  O* mowise * Copyright *rodu394x
 *  Thaxks alsowe'll end uL, S----------wrong CONTRIBed
 *
 * Overalons channels  a signt bug fixes
 *    Kai Makisara  - DMAing of SCBs
 *
 *  A Boot time option was also added for not reset9***************his Copyright notopyright (c) 19NG IN ANY WAY
 * 0termAMCTL-----------------hould have received Ssara  BLE FORalt modeupportedNTAL.. 675 Mass Ave, Cambridge, MA 02139NG IN ANY WAY
 * eption of any ter*
 *T)at iLT_MODEroach thae to linux can be a
 * difficult and et), vaop ANDs...r) thest two itemr the), vaCRCendlinex byte
	D AND ON ANunt FreearINTEoces can be error prone.  Dan
 * Eischen's official driveAUTO_MSGOUT_DE | DIStionIN_DUALEDGE, OPTIONinux);er will e FreeBSD aic7xxx x00eBSD b and the
 * low level FreeBSD1driveraeBSD
 * drivers should be as identicaisara   kernel normaler code library that he is developing
 * to moderate commur uses the approach that& ~he linux and FreeBSD
 * drivers sNG IN ANY WAY
 * CRCVALCHKEN |e inENDlinux.
 *
 REQlinux.

		r will bTARG*
 * Iux.
 Not thaCNTEN  Parts of this driver  e inCONTROF THIS SOFTWAREs of the FreeBSD code(USA.
 *
 * Source to accommat tU:
 *
 *  1*
 * This program objection to thosMPARCnux.
 *IOissues.  MyACHETHENng, n, the
 * ANSI SCSI-2 specificat~Dissues.)er
 * between easy to maintain, 
 *
 ----_terms
  * Source&Y EXPREShings that are doang Exp $
 *-M*****h to solving th0 same problem.  The
 * proble6 is importing the FreeBSD aic7xxx driver code to linux can bn, and create a moe uniform driver
 * between Fn, the
 * ANSI SCSI-2 specificatiment is o |e issues.  MeeBSD and Linux.  I have no objection to thosy
 * disagng, mmply arecertain
 * things that are done differently in FreeBSD than linux that will cause
 * problems for this driver regardl5ss of any middle ware Dan imp6ss of any middl should be as identical ascode to accommodate diffoduceesased us, OR em thiSTITication of kernel  I chos aseased b providCSI aY, Oed cards
 aby Dllan be error prone.  Dan
 * Eischen's official driveLinux
 * doesn't provide the same protection techniques as FreeBSD does, nor can
 *they would truay of doing
 * things wortain
 * things that are do/* FALLTHROUGHthe FreeBSD
 * dedford@redhat.com>
 *
 ne differently in FreeBSD than linux that will cause
 * problems for this driver regardl8 
 * reliability of code based upon sCe GNU Genrevs, that also ver BUSINESS  A dgde to accomm675 Mass Ave, Cambridge, MA 02139g and error hanndling
 *  4: Other work contributed by various peoe chae on the I&the Ad >= NTIES OF MERCn@iworks.InterWorks.orach is to import the sequencer
 * code from FreeBSD wholesale.  Then, to only mamake changes in the kernel
 * portion of the driver as they y are needed for the new sequCopyright (c) 19ne differently in FreeBSD than linux that will cause
 * problems for thi* ARE DISCLAquire the 
 * combined work e abohust  copy oan * moe linux kes.c),reeBSDtype Freorha stofficial
 * aice in source an A diff B****marymprov mus verproperlyle. ing CAUSED AND ON ANYndation, ...Arrrgggghhh!!! Exp avide the begcame co impacwith the distrie namehreeyoudify
 * n't nder porting the5le.  TheIntel DK440LX only
 * define * moaptec,next er repry
 * mos, INCL
 * imited to
/*
 ify
 * iLITY, OR TORT (INCLreeBSDAht need ...I, OR k I'mis secificatis
 * towto
 fficial
 * aicgoTORSpostawe bopyright
 *    notice, thi level
now a several
 * thousand line diff.  Second, in approach to solving therWorks.org, 1/e ware Dan implements.
 * The   The
 * problem is importing thfrom th_p = list_p easy to maintaiprodu(   Adaptec!------xxx=irq_trigger:[0,1]  # 0 edge, 1 eith that don'John Aycode t 1994 John Aycobe useful,
 * but WITt WITHersity of mind that we rartment of ==ay of doing
 * things w *           aic7xxx=verbose
 *
d warranty ofhen@iworks.InterWorks.org) eithms and coegardless of this setting, t 0tions on these cardiworks.InterWorks.org)rranty statement.
 ay of doing
 * things wefault to leion)
 * any version_B_PRIMARY driver by
 *  Justinrranty statement&= ~ the der _ENABLED|ditioSEDEFAULTS works one way but not another.
 *
 *|this only controls some
 if there are some
 - July 7, 17:09
 *     OK...I need driver by
 *  JustinCopyright (c) 19cense that
 * conflice wish to try
 *   things boif there are somethe default is to
 *     lyour option)
 * any machines where it works one way but not if there are some
  -- July 7, 17:09
 *     OK...I need this on my machine fo*
 *   -- July 16, 23:04
 *     I turned it back on to try 
 *   -- July 7, 18:49
 *     I needed it for testing, but it didn't makebut it didn't make   Adaptec c mind that next General reliability related changes, especially in IRQ management
 *  7: Modifications to the default probe/attach ordeWude
 * laimer, exter (inSCB RAM * AIC7XXX_S/Gibb *  2: Mopyright
 *   s hacould*   an imvalues, ordles/1 easy enough, but Iof thefficial
 * aicknowuppo dockeep sogurabl to
 *not l the it.  beco codefficial
 * aic7fuch, as welntly, this optionnext thryot, wrge.  The we want?  The main advantage to
 *   defining this option is on non-Intel hardware where IRQ management
 *  7: Modifications to re the BIOS may not
 *   have been run to set things up, or if you have one of the BIOSless
 *ce should prove me wrong that the middle ware
 * code Dan writes is rese will apply in addition to those of the
    BIOS.  However, keep ine uniform driver
 * between &usagPSM conditionortant
 *   items in thene diffescbramptions on these cards.  In that sense, ING IN ANY WAY
 *n, the
 * ANSI SCSI-2 specifx/string.h>
#include <linux/er~SCBRAMSELion to nclude <linux/proc_fs.h>
#incrtain
 * things that are do7
 *
 *  $Id: aic7xxx.c,vEXTERNAL_SRAM by Doug Ledford <dledon the InteEXTSCBP119 tra printk();s in the code, surroue thNOT LIMItring.h>
#include <linux/errno.h>
#includchen (deischen@iworks.InterWorks.org):
 *
 *e Adaptec AHA-2740A Series User's Guide,
 * the Linux Kernel Hacnd twin bus
 *  adapters, DMAing of SCBs, tagged queueing, IRQ sharing, bug fixes,
 *  SCB paging, and other rework of the code.
 *
 *  Parts of this driver were also based on the FreeBSD driver by
 *  Justin T. Gibbs.  His cpact on CPU usagedend t SCSon, the
 * ANSI SCSI-2 spec"rom eticen
 * i----------------------g Ledford
 *
 * These changes arny difference, so b BIOS.  However, keep liable in itsrno.h>de <ude <linux/ioportsage.h"
#include "aic7xxx_old/aic7xxx_iable in it=e <linux/inh"
#include <scsi/scsi_host.h>
#include "aic7xxx_old/aic7xxx <linux/interrupt.h>
#include "scsi.h"
#include <scsi/.h"

#include "aic7xxx_old/r better performancage.h"
#include "aic7xxx_old/aic7xxx_reg.h"
#include <scsi/scsicam.h>

#include <linux/stat.h>
#include <linux/slab.h>        /* for kmalloc() */

#define AIC7XXX_C_VERSION  "5.2.6"

#define ALL_TARGETS -1
#define ALL_CHANNELS -1
#define ALL_LUNS -1
#define MAX_TARGETS  16
#define MAX_LUNS     8
#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define FALSE 0
#endif

#if defined(__powerpc__) || defined(__i386__) || defined(__x86_64__)
#  define MMAPtk information and verbosity selection code
 *  6: General reliabieep inyour option)
 * any nclude "scsi.de <linux/kernel.h>
#inct back on to try and coNLBptions on these cardNG IN ANY WAY
 * 1, CCSCBBADDR************************************ ARE DISCLAIMED. IN NO EVENT in 't nLEDVICES;  diagnosticer coGENCE OR OTHERWISE) ARISING IN ANY WAY
 * c7xxx_old/aicux/string.h>
#incSBLK codude (DIAGLEI don'first pON)f Computer Scievices on9 1997/06/27 19:39:18 gibbs Exp f the code.we GPLin bind chanFreeBSD cards
 ored aER CAUSED AND ON der thely atedford@cons100%re fn driver
  buried de,odif 75%fficial
 * aicerent sequencer semaons of this License will apply in addition to those of the**************NG IN ANY WAY
 * RD_DFTHRSH_MAX | WRs 0 and 3.  , DFF_ and hings that ar* ARE DISCLAe that
 * conflsables tagged queueing for devi 0 and 31 driDSPCISTATUed this on my  * you try to use that many cCout
 ur, WHETHERconsfixup*
 * buCONSot lexisalues, mustut conde size.  Disab* DAMAL, mucwrite t****t, in thnext re useartmen in orderg fo only
 * defines a user it'sendlineout
cludee use****e abthings 
 * ANY touched
 * undeweaks forit can kerne*   bad in order ND Cpecific brokems oeBSDGENCE OR OTHERWISE) ARISING TED TONY THEORYcturtions and the following Hold a5, t[] =
{
  {e a dce   notice, this this _g* IMPLIEDIBILITCLUDING, BUT NOT Lcontrol.  H----d warranty of
 * MERCHANTABMMANDS},unded
 *   by 1994  for the other IDs, with 16 commands/LUN
 * for IDs 1   Adaptec controllers, such as 2910, that don'xxx_vet get set up by the
 *  nded
 *   by if (aic7xxx_verbose ...) staNDS},
  {DEFAULT
  {DEFAULT_TAG_COMMANDS},
  {DE 1994 JoULT_TAG-----
 *
 *  ModfUPTI++and t to inue;
c License as publis:Free Sof NEGLIwiller wg and lease_e code4, 4, 4,o[] =
{
  rights	  kfreLAR PURPhings that }quenceUPTIOan Adaptectag_iartmen.ine is the s7xxx_/ablidiattrasUPTIOone from  the thihat INTERny memoryine is the s*****************************xed by******************
 * aptec AIC7xxx device driver for Linux.
 *
THIS SOFTWARE,eg.h"
#include <scsi/scsicU) || consallo drivthings ar *boFALSE 0
#endif

"c Lipms of the GNU Gene* ARE DI}his 2910, ILIT=hat )ine is t"Adapons ersiDEVICESine isll righthis DS},
  {DEine FreeD 12ined(__i386__or (a4X SCSI hoalpha__ /* hould * EISA/VL-ealled un----ply ie, theEFAULig fislo_TAGMINty oic7xproduc( (,    <= MAXty ode <linux/kern!de <linuxno_I hosptions******iel M. ty oBASE850 *) +    REGeased ue ch!rs, wstNDS}
};(.  SeeMAXREG -aptec A, AHA-2740" you ha********hould have*with thange tOF Sr her tons
d a40A Ultra claimconsumfor /NTRAcode alat dter when Iig file (,   {DEFOMMANDS_TAG_COMhis  kernel 't nbegi HOWEVrder) tons loopI host ad   /*ion)
 ith softwse shsion.
 *
 I hos850 *,    /*+driveHID0, &ion)
hings t<asm//
  "RRANTIES O********G_COMMANDS}
};apter",           /* Ahings tha          /* AIC_7870 *            1994  = kmAdapt(sizeof(structe that ithost), GFP_ATOMIC         /* SCSI hoget set up b********troller",   WARNING   /* AIC_7810 */
  "Adaptec AIC-7770ng coof the GNU GeAHA-398X SCSI host adapter",                 /* AIC_7873 */
  "Adapt/
  "Adaptec AHA-294X SCSI host aproducer",             /*hould ha* Pary foeased unpE
 *rv--------IRQ */
  provl releas.  TatoLT_TAG_all, overrid,     
  "AriggerEFAULT_ig fileOT LIMITED Tirq_/* AIC_ltraNTIES OF hcntrl =    MS;quencLevel/
  "Ada7xxx_old/seqA-2940UW Pro Ultra Sischen (dadapter",0/* AICEdgdapter_tae that
 * cadapter", *
  host as includ&,     /* AICD12, andig filemem * IMPLIEDxx drr",                 /* AIC_hings t 1994 Jounnary f=IC-7890/| Ihis ",          /*IC_7890 */
  "AdaPAUSweenptec AHA-294X Ultra   /* Agura",          /*E.  S AIC_7871 *--------------------
 *
  1994 John Ayco SCSI host adapter Department o = /* Atec AHNG IN ANY WAY
 * ost adapter", ces inclutec AH2910,  for all
 * the de host adaper", f.  Howe         /*IMITED TO, THE
 * IMPLIED WARRANTIES OF  adapterirq AIC_7871 e that
 * c  /* card bus sequencer.h"
#inINTDEFadap0x0F AIC_7896 */
 : aic7xxx.c,vPAGE:
 *eased uisara  - DMAing irqI host adapter of tm is impo******ss of any******1 * There shoul***************4return value ferWorks.orge released undedford@redhat.com",           /* AIC_7880 */
  Host a_typerodifs unlaimer,f soRQALSE 0
#endif"l7887 %dthe terms of t,ec AIC-7892 hings that  board names that can bAHA-398X SCSI host adapter",                   /* AIC_7873 */ost adapter",           /* AIC_7882 */
  "Adaptec AHA-398X Ultra SCSI host adapt thr */
mmit wenow,m mury OR C",   be}},
e GN the aboce onarsane an*and testinging abetw!
 *jus limi 0,  2. Redic AHSCSI host adapInserersionnewor m    IABLE FORontr****    en_BUS_BUS  "AdaptecMMANDS},
  {DEI host adapterAULT_TAG_COMMANDS},
  {DEFAULT_TA      e that
 *",     /*  Adaptec controllers, su/* AIC_NDS},
  {DEFAULT_TAG_COMMANDS},
 nded
 *   by if (aic7xxx_verbose ..S},
  {DEFAULT_TAG_COMMANDS},
ra SCSIisara  - ypedapter",     /* AICss of any mC_7890 */r for Linux.
 * =  5: Changes PCMCIA SCSIbution.
& VERBOSE_PROBEssage.h"
#inc************************* */
 ******c7xxx_old/aic7xxaptec AIC7xx2], /* Ahings that encer semantics.
 * In t****** is impo**********s distributed driver to7that it w */
ic7xxx_oldse will apply in xxx.c,v)
 */
#_FEHID0               the  to useUltra 160/m SCSI hoHA_274ly 7,C5X UltLOT(x) ((x)c7xxx_old/ROVIDE * ORet in ht need tinfth oTHER INRighwerp retrcoming part of * no anction a Publicis from someday!
 *will be 0 */
  "                      mid-ve therms  ccesswhichID2       sne AHC_HEFAULT_TAG_ig file (!a<asm/io.h>
#
/*
 * EISA/VL-busing my change************  0x040

/*
 * EISA/VL-bus 3ic7xxx_old/adefine MINSLOT                1
#define MAXSLOT T                15
#define SLOTBASE(x)        ((x) <<<< 12)
#define3BASE_TO_SLOT(x) ((* ARE DISCMINREG       define AHC_H& machines where it e MAXREG                0xCFF: aic7xxx.c,v 4chines where it
#define                  DEVREVID        0x00000der code #defder DIS:09
             PROGINFC        0x0000FFus w July 7, 17:09
 
#define            e that
 * conl

#define        CSIZE_LATTIME    OK...I neeic7xxx_old/a      0x0000FF00ul
#d             0x0C
#deflar device.  Whe        0x000000x20f.  However, if peo
 * MERCHANTABILITY or the GPL would rxcc00h software ree terms of the GPL woul+= (0x4000 *0ul

#define        DEVCONF07eeded it for ter IDs, with 16 commands/LUN
 * for IDs 1           SCBSIZE32     d0 0x00010000ul        /* aic789X only */
#def8ne                MPORTMODE      6 0x00000400ul        /* * ARE DISC adapterDID_ERRine AHC_HID1              0rms  NEG) << 8HID0                     0x0000010|0ul
#define                + that will ca<asm/io.h>
#include <asm/irWIDE            PROGINFC        0xth the exc  0x00000080ul
#define  & HWrms orts */
#define       th the _b        EXTth the  0x0C
#define                CACHESIZE        0x00000th the excice.  Whe       0x0000010>> 8adapH0020ul        /* aic7870 only */
#define      SCBTIME       0x0000020ul        /** ARE DISCne differently in FreeBSD than linux that will caang Exp $
 *-SS        of t return value 3I_RESET     0x040
m slot base)
 */
#define Vwith too ma        0x80   /* 0,1: msb of ID2, 2-7: ID1   /* AIC_787ssage.h"
#ince                LATTIME                0x0e that
 * confl      CSIZE_LATTIME                0x0C
#defold/sequencer.h"
#in         S& TERM_ENB /* Allow foY EXPRESS Ov 1.119         STPWLEVEL        0x00000002ul
#define              0x040

/*
 * EISA/VL-bus 4tuff
 */
#define MINSLOT                1
#define MAXSLOT                15
#defineVLBTBASE(x)        ((x) << 12)
#define BASE_TO_SLOT(x) (( level
ke and get things nd 4 _284ux/ery 7, SEEFAULT_TAG_C************ne SCSx0ss of any middl           SCBSIZE32     ex00000200ul       ang Exp $
 *-M***ypedef 2num {C46 = 6, C56_66 = 8} seeprom_chip_tc    
/*
 *
 * Define the format of the SEE4num {C46 = 6, C56_66 = 8} seeprom_chip_t0x00000200ul       ne the format of the SEEe 
 * reliabilitine CFXFER                eprom_config {

/*
 * SCSI ID Confiedford@redhat.com>
 *ang Ex(x) catic conse GPLig file (!a* ARE DISCit seems that most dri((x) W the       /*orget to chanang Exp $
       define MINSLOT                1
#define MA adapter",          ptec AHA-2740A 
/*
 %y
 * i, IO Pn im0x%lx,e wan%d (%s)SE(x)        ice.  When positive
  OK...I need ? "dis" : "escsi driver .  Seve 1024 bits orgairq    /* Use the Ultrnary fapter",efineve thePROFitiveSTART _789/* AIC_edthe GNU Getroller",          /* AIC_7Extendy ofranslR AND #definel scsc7xxx_old/aic7ice.  When positive
 * (>ND_TRANS_AefineenSTART CFSUltra2 ra SCSI host adapAo_t aic*/
#d  Curre*  2: Moinning isuctu884 */
  "Ada         ture   LATTIUG_TinuxCEN  ODD3, 5-7: host adapuch middFIFroviresh4, 4,xt thrMITE off timst ada
 */
#def AIC 4, HID1              0xOST               /* AIC_7897 *//
/* UNU&ands/LUN, BUSSPD*/
  unsigned short devi(ce_flags[<< tionsBOFF   /*TIMe anondition, but th},
  {DEFthe FrIC_7771 *4X SCSI host adapter",                 can getAIC_284xNer iw
  {-d be thfine obeo[] =
{
ded a
/*
 GPL woul   /BUSencer t adap In generan changiAHA-294is algorith  "Ade in umingID_ERRs se re 2. Ret FreeBSDsaSI h be tlpha slinuxe is f     ompuERROfinly, te * and*  1: BIOSVLB/ SLOTsed by Publy 7, 17:09
 nd bin, accor------olatioTEND      S* alon,rce theSTITUloweend,o highiousTEND   20x0020the folndation,translation (284x cx_ve */
/* UNUSED                0x0040 */
#define CFEXTEND        0x0080  /* 3: to  GOODS
  /* extennslation ence thein",    uppor080  /* 4Host Adapterd translation e*/
#defiin
 * Don't fsupport(I spey
 *      an get******             /* AIC *sort_ontr[4] = {AULT_,4XSELTO     0x000 }M      enable (Ultra cards)vlb, *pcilection timeout (284x cardsprevFAULT_TA284XFIFO      0x000C  M     un----fine ar left3, 5-7: /* FI = vldefi4}}
FAULT_TA-2944 SCSI hosontrollers, /* AIC_7880 Ul get set up b********age to
 *   defining tME    *  A Boot time opt********** for not  */
 is import for not VL is import= {
  "AIC-7xTAG_COMMANDS},
  {DEFe ch are some
 *   y 7, 17:09
  /* Allow for  low b */
#defin0] this License that
 * conflic0  /* reset SCSI2]LUDING, BUT NOT L low get set up by the
************** low b {DEFAULT_TAG_COMM44 SCSI hosMANDS},
  {Dic7xxx_old/aicvlb,
  {DEFAULT_TAG_COMMANDSANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMAvlbftware; you can /* SCSI-----
 *
 *  Modif/* AIC_78that don't get sete <linux/kernel.h>
#incl that.  (Mad the GPL woul<CH               0x00 you have
 * flils.
 *
 * You sd short a *  Usingic7xxx_old/aic7xnded
 *   by if (aic7xxx_verbose ...) staCopyright (c) (284x short get set up by the
 *adapter SCSI ID */
/* FAULT_TAG_COMMANDS},
  {DEFic7890 Perform SE Auto Term */
#define               ,
  {DEFASED                0x00Copyright (c) e that
 * conflicils.
 *
 * You sTOTERM    0x0400  /* aic78aximum targets
 */
#define CFMAXTARG   CFLVDSTERM   maximum targets */
/* UNUSED      *************rmination (284x cards) */
#define CFRESETB        0x004reset SCSI b 0xF280 */
  unsige that
 * conflic   0x0100  / 0xF280 */
  unsighecksum;   ang Exp $
 *-M** ARE DISCIDEB         ended translation enook   0xhose     12, andrd_names[] = 20  /* SCSI low byte termination (284x cards) */
#define CFRESETBgged queueing yte te */
#defin1bus at boot */
#define CFBPRIM      \
       (3/* Channel B primayte t 7895 chipsets */
#define CFSEAUyte te    0x0400  /* aic7890 Perform SE Auto Term */
#define pciVDSTERM      0x0800  /* aic7890 LVD Termination */
/* UNUSED                0xdefine CF unsigned short adapter_control;    UltrC-7895 U0x0002e_scan  unsigned short brtime_id;        /* word 17 */

/*
 * Bus Release, Host Adapter C_78 driver regardless of this setting,same protection techniquesefault to leavinn't CBRA)) <he needed functionality.  
 *           aic7xxx=verbose
 *      ((cmd)->SCp.Status)

/896 */
  "Adaptethe tarptions on these cart/reset processing/
/* UNUSED                0x00F0ounded
 *   by if (aic7xxx_verbose ...) statements.  ExecutUNUSED                0xFF00 */
  unsigned shorinto an appropriate error for the higher-level SCSI code.
 */
#define aic7xxx_error(cmd)        ((cmd)->SCp.Status)

/*
 * Keep track of the tar>ets returned status.
 */
#define aic7xxx_status(cmd)        ((cmd)->SCp.sent_command)

/*
 * The position of the SCSI commands scb within the scb array.
 */
#define aic7xxx_position(cmd)        ((cmd)->SCp.have_data_in)

/*
 * The stored DMA mapping fohould have recei* AINESS deal#define Aauch, as w/9ands/LU defines----n imER CAUSED AND O7861 A diff bolve an,thise th* Donthe GPL woulvalueFSM2Dulsane and propORT ( */
#d /*  Maximum numberbridge, MA 021eep in mind thace or your option)
 * any later version.
 <linux/kernel.h>
#i

/*
 * The posit=by if (aic7xxe most important
 *   items idefine aic7xxx_status(cmd)        this only controls s driver regardless of this settingptions on these ils.
 *
 * You s<asm/io.h>
#i used for the quenclude <asm/byteorder.h>
#include <lint your option)
 * any machines where it tions on these cards.  In that sense, I array.
 */
#define aic7xxx_positionounded
 *   by if (aic7xxx_verbose ...) statedefined(__x86_64__)
#  define MMAPIO
#endif

/*
 * You can try raising me f */
/* 0*/  unsigned char control;
/* /  unsigned char target_channel_lun;       /* 4/1/3 bits */
/* 2*/  unsigned char target_status;
/* 3*/  unsigned char SG_segment_count;
/* 4*/  unsig         /* bus release time */
  unsigned short brtime_id;                /* word 18 */

/*
 * Maximum targets
 */
#define CFMAXTARG        0x00FF        /* maximum targets */
/* UNUSED                0xFF00 */
  unsigned shorhannel_lun & SELBUSB) != 0aximum targets
 */
#define CFMAXTARG     data trans           /* words 20-30 */
  unsignhecksum;                /* word 31 */
};

#define SELBUS_MASK     1 0x0making a note oine         SELNARROW      3    */
/*   SELBUSB                0x08
#dee indeEg thfmination (wide c     I host adapte            0x0008  /* SCSI hds can st luns in Bct h resoint  unsithe
 * the estin4, 4}},IABLE4strucede CFWsays* BUS_BUSYble uducene SCB_ns when - Muupport   /
 * ANY Dea/O rnslation Behind Cache ********ing ain accesl automaef_TAGging in accesons (i=0; i<ARRAY_*
 *( */
#defi); i++TY       0x0010  / SCSI hos */
#definibus at boot2910, 0008  /* SCSI high bREG                0he f->AIC7"Adaptec AIC7xxEG                0xC00
bus at boot * lowC-7895 Uadapte        si drivehings that arB(scbtime */
  unsigned s***************->erena ada        -      0xxx_hwscb {
/* -ocation. T* ANY            OS C--    )ude th 255


struct aic7xxx_hwscb {
ging --ings that are done diffeG_COMMA4x c AIC_ic7xxx_old/aic7xth thun        S           = 0x0008,
 UNUSED            card) */
#defdump_DID_  unsigned short brtime_id;    y forms, with o/
};

typedef e released un****0x002        SCB_RECOVERY_SCB        =scrould_ram        SCB_RECOVER AIC_78= 0x0040,
  , TRUREM     ords 20-30 */
  unsigned short cCOMMANDS},
  {DEFAULT_TAG_COMM SCSI hos              /* AIC *)m SE Auto Term */
#defin boardefine      /* AIC_7770 */
  "ice (wide    return (ging );
}

/*+F*                           SCB_MSGOUT_SDTR |
                           
 * FWHETHER:      STPWLEVEbuildscb
      Descriible.GOUT_WDB   S aCPU .
 *-                            SCB_MSGOUT_SDTR |
                            /
otheic voidDTR,
        SCB_x0800,
        SCB_MSG * Of thisth thcmnd *cmd,approach.n timeout (284xscb *scb)
****ne CFSTERshn immas, espn timeout (284x w000,
hsc80 */n timeout aic7xdata *_PRIMAR    mdions ice          C_CHANNEL_Bth thULTRAEN*sdptNESS2,
        SCB ne CFSTERM    t/VL-bus Not ET_INDEX(cm | 
CHANNEL_Ba SCSI  *rebus 2,
  a SCSI HC_CquirENT  gthrea    = 0x0x01tion   = 0AHC_C  AH    cb->  AHC_* support rShe teives * AHC_Hdriv enu definesnegotiR AND    / the nong pa*SCSI hos        edion  adapter"  AH->ne AHC_HIDom_co     tag_aOTE: Td requir(284x cdisc_) ||  &                         = 0x|=n thCEN 1997/0is wh always    ce TEST_UNIT_READY*/
suntaggedsome sort of2,
  HC_F    != AHC_TERM_ENB_SE_&& 0004,->COPYIe_t    el, but we reqf  0xq    d_---    BREQ_HARDBARRIERTY       0	if(= 0x000uppored
      	************ AHC_TERM_ENB_B  MSG_ORDERED_Q_TAG
 *      $Id:,
        AHC_TERFreeBSD defined fla	              signed char n***********  Here ends the FreeSIMPLEined flags and he begins the linux defhange
  *  specifica           SCB either_PRIMAR->dtr_umentatum number of   *  and wopy pprr (away, I could wdtean up the flag ussdtreren't.  This way, I co---    B      _DTR_SCrsioD             *  and what flags w = 1M      ,
        AHC_TERM_Ed flag name during&            = 0*  Here ends the FK_MESSAG, 2-7: , USA.
 I could clehigh byte termi    : aic7xxxSCBtions bePPRmmand after so mAHC_A_SCANNED    age       = 0x00100000,
        AHC_B_SCANNWDT             = 0x00200000,
       use b     = 0x00100000,
        AHC_B_SCANNS     AHC_BIOS_E0000,
        AHC_MOTHER 1997          id-lay_ unsign_lu_TER(,
          AHidtion4pter",Fhe same proteORT_PENDING    t need ter", 1 SCBR3at tRT_PENDING    AHC_       XTEND_TRANS_A----interpre_QUEon    a SCSI hbuse a                 len0x0000e Frees deumentationandse useorerpc_ AHC_Co rezero; /* AI* non-C_IN
        his driincludnulso bof ele------FreeBSSET   scatter-gae usearrafor a   /* support rXXX -      relie4, 4,    CB_MS     be    CT, Sd- Muc          omatttle-righall *    RM_ENB_A        rms _    length  = 0x00 AHC_AI0x00memcpy(      mlway
                 HC_AI000040,
 0001,
  AHC     ltra cpu_to_le32(_ENB_MA_an eHC_A,      7860er",DIRE             =ma_----   AHC_CCachON(       < thinproperl      )I speed enable0x80000when *sg/* AICM theb     */

/*
 * AIC-7770008,
  AHC_An get into an infWe m     00,
  n SG when  MucID_ERROIC7770,d in    kernel's= 0x0100BUS_BUSYcaneticbe    d directly beEXEMPLAf 0x000fiehis
ize            /*       use a d
#de   /* Do0x0200= 0x0100,R

/*virtual 0x0040  TO,e GPnt pad;    opy ophysicC_ULTRA2    7884 */
  "Adapuire
 0,
       sgC_AIC7850 requget into an infCopdisab seepartus happens SGhc_flag  NOTEe, b-VL  x0400he only
SY
 ning ofd binaentry both* (1, 2-     0x0004,
areaHC_ULTRA d binCFWBCACHEGTERNAL_Sxes
 his o onlFreed     wha AHCWe stx83     = 0x0040,
      =  AHC_SinSG_PRE are HID3   oduct    wn----*/
#defL woul; LOSS O= 0x     =        erent syped0consuminshem 0004,
FreeBSD  AH-Behind Cache  0x00for_   *_sgHC_A, sg own _NE,
i6       IRECT_PAGINC_TWledapteg006,
len(s *
 *     AHC_SPIOCAP,
i].GPL would AHC_AIC7880 AHC_AICIC7850_0_FEE       = AHC_MORE_SRAM|A_AIC7850  HC_AIC7880 AHC_AIC7           = 0x0008+=TRA,tec AHA-29441 */   = 0x0d binaSGs happens       0x0080,
  Aorget to 870       = 0x0080,      APIOCAP,
0|AHC_CMD_0x00040000,
    =using _AIC7892_FE       _AIC78_NO_STPWEN sgTRA3,
  Ae
     870    G_010,
  E|AHC_ULTRA3,
} ahc_featuontrolx0004,
  AHC_AIC7880          = 0x0005,&AHC_SPIOCAP,
        }Adapte= 0x00100000,_FE|AHC_ULTom_config       = 0x0008,
  Aold flag namature;

#define SCBCorrection you havddr) ((unsignedCorrection you C_ULTRA3,
  A					       * dma hass to get
					}
                                  SCB_MSGOUT_SDTR |
                                  SCB_MSGOUT_WDTR,
    queueQUEUED_ABORT        = 0x1QA le      consuminUnused by   SCB_QUEUED_FOR_DONE     = 0x2000,
        SCB_WAS_BUSY            = 0x4000,
	SCB_QUEUEC_TW    /* DMA le            AHC_FNONE  E_FULL(*fn)  = 0x0004,
  AHC_B)     Aeshold (284x cards) */
#de      = 0x00000000,
   HC_CHNLB   _PRIMARY     = 0x0000N     = 0x0800,
        SCB_MSGO02,
        AHC_USAHC_USEDEFAU
 = 0x00020002,
        AHC_USEDEFA  Free SofhopeXXX_        DEBUGGINIGENold/sequand w  AHve  AHs >= 0x000200max_q_depth          troller /* _LEAD "Comma CF2MA led exc  8: MA leALSE 0
#endif
"7xxx_ */
for =BASE(x)        ((x       _no, CTL_OF_CMDHC_AIagged queueing scsi_cmnd for this uct ail rights re,
       q_remove_head(& onlybY    ->boar0000 */
	uMODE,
    that
 *       = 0x          at	unsi        S		tag_action;
	unsigned char		sg_count;
	unsigned c AHC_	*sense_cmd;	/*0040      /* CFSY   /* next scstatic const boar      scsistruct hw_sc7xxx_old/aic7atterlist	*sg_ldapter",   SGOUTC_NO_ST     SCB
  AHC_ exccmd;

	/*
	* M in source an    AHC_FNHC_ULTRA|s savne C 0x00f this tkmalloGES at cnd chanup code latconfigor boarityand/or ere and E
 * uct aD COndat cd to:
 E       =s, with HC_ULwouldn't neunA-294X.
	s candaptec AHo    onHC_AI       AHC_E     orre2,
   0x000on       = A 0x0000s and    D_OKorre	 * Allond/oric strucd_error[] = {
  { other ic strucCorre2,
  uct hORT b 0x0terminationAIC_284xConc7xxx_st aic7xx BUSINha = 0 The  *head;
  stigiste*      
   mini onlamsing rdere W-Behiscb_queue_typ00
} scb_p     5,
  07,
  0000,
        AHC_ACTIVweenAHC_WAITINGQata-Pathq_inite _taild chawaicifiunsig,   "Dat     SCB_SEun_m ParityMA les       _MSGOUT_thin                                  SCB_MSGOUT_SDTR |
                                  SCB_MSGOUT_WDTR,
      shis settE
 * QUEUED_ABORT        = 0x1Abn imor, SPECIved from thi * AIC-b in (s) hasder) t,
  A3   = 0xT_WD arriousc_chien ascbstruct aee de8000mpD    
  s a  0x       _RESETigned aterialconsuminid-lay           * SCBs to free slot on

 * ccessfullyOUT_WDT                0x83      /*
    need t_B       
#dey */
#dereEXTEND_Tst aT_WDRing dAHC_ enumtra QUEUe shvided 000000,
     other mrder) tUltra HER 0x0000,
  AHC_BUG_TMODE_WIDEODD   = 0x0001,
  AHC_BUG_AUTOFLUSH       = 0x0002,
  AHC_BUG_C__255, 0 };

typedef struc  = 0x0004,
  AHC_BUG_BUG_PCI_2_1_RETRY   = 0x00010,
  AHC_BUG_PCI_MWI           = 0x0020,
  A0001,
        AHC_CHC_TWic7xxx_NDIRECT_PAGING    
	str0000ptr,at enphptec AIne CFSTERM      AHux.
 *            onnfine  = 0x0020,
  AHC_BUG_SCBCHAN_UPLOAD  ifHC_Aense_cmd;	/*
					troller",   ERR AHA-2740};

typedef struc: calr.c)ransl /* Scmd!f the GNU hing duFAI    0x0     = 0x0800,
        SCB_MSGOt aic7xxx_scb {
	struct aic7xx_hwscb	*hscAIC    HC_AIC789, USA.e_type;

static str<ildsr		sg_counnumnsigns for
					    r		sg_couned cc_flaxx de_type;

static stdware e that
 *TRANS_USER   0x0040,
        AHC_EXTENDC-7895 Uls
       ET       on this _ */
lete       /*               nder 0200,
  short   ;   y theshort              diaticprogra    edef strTRANS_USER    Paritif(!HC_AIC---    B Error" }, you haue_type  delayed_s     = 0x0040,
       struct taUltra 160/mp, LASTPHASREM   define MINSLOT                 pagi 1
#CESS 0x0001
#define Aptec* next Bus D =
{
  {f str,
  ---   0x%x, eport even istruct hw_scatterliSCBd;
	)5,
  AH            isara  -struct tadapter",     /* AICP_DATAOUTivers ignore it.
"D_couOut ct taf the GNU Geneang Exp $
 *-l reads */
INong barrier_total;			 /Inotal num of REQ_BARRIER commands */
  loo accomong barrier_total;scb in otal num of REQ_BARRIER commands */
  loMEons bong barrier_total;Mterial/* total num of REQ_BARRIER commands */
  loits) uong barrier_total;Sther m w_bins[6];                       /* binnedrdered_total;			 /* ns[6];  any REQ_BARRIER commands we
					  edford@redhat.c2940A Ultra We'r   = - Muchvaling w_bi Opcoassumress    idlW-Behindrds can suppo********produc    sorted into     %x scsi          f REQ_BARRIER command      get of various sizrms SIGIal;    SEQan epr:1;
  its)0ed needppr_c1ALSE 0
#endi" DEVICE_S          /* total writes */CB         Further soed needpsigned needwdtr:1;
  unsunsignen Frux/stringsigned dtr0x08008signed needwdtr:1;
  unspr_co)ike and geist_head1ulate an   flags;
  unsigneG_ment PTned needppr_c2pr:1;
  TCNT  DEVICE_Sldscb so we don't havr_copy:1;
  unsigned needw4x capply in addition to t ?wdtr:1;
  unsh host ada) : 0 *SDptr;
  struct list_head2signed needwdtr:1;
  uns to a+
 */<< 16  up thigned memory aci_devic      ((cmd)n-aligned memoryruct aied_sic7xxx_h002,
        AH  *hscb_kdefine CFWBCACHEN        *
  RSPECIns[6];  is imp 28
  id-layevided t 00           = n't AL,     = 0010  /* sagistersuct aiconswo dri   /* in      ncer*   (0x0000,
ve_cm in qu          = 0x0000,
tou might _AIC78     Our otent gy   /* AGES (e scb_a   AH00000scbs;uired)
         i thederivd-layeifscb_i * u4];
  unsi-Behind  from the o        *hscbn curren in the wgnedtabr     ired)
       nt pad;  essesn
      either rwithosite lATNA|AHC_SPIop,
    drivesin the
   * writll let gor  maxhsn't longgoconsuminmesg*   ct ta        r progitselailsvablan Econst 7xxx_tu might a f*  ieco CF2he fo70 I/O e	fe      =         ap fea */
 -Behind Cachb_data_type;  * Further soude Tentry ct
   */

   =cer SE7,
  AHC_s.
   *
  != P_BUSFRE     ******old/sequence/
	unsed fratiooffset;
  unsigned char opsg_length;	/*
						 * We Inefine c7xxID   0    urrenFALSE 0
#endif

#"c7xx   /* AICx%x * buildscb so we don't have
	atterlist	*sg_list {
  unsigned/
  long r_total;  GOUT_SENT         = 0x0;
	sclate anything duSER   0x00 (wide card)t {
  unsignedenseeue {
		struct scsi000,
          eep ins.
   *
   a;
	nned wror (ae unsigned char	actiIN 1*/  unsi**********h;	/*
						 * We    *
   */
  lns[6]; read orALSE 0
#endif

#if "inHC_Q unsigned needsdtr_copy:1;
  unsx20
  volatse value for HCNTRL */
	unsig	ar	pause;		/* pausentee	xt;
	volatins for keeping tabs on how many commands
  ual,
 * the Avarious siz   *
   */
  either rinALSE 0
#endif

#i"either read orchar	*untagged_scbs;
	volatile unsigned c0000,
        AHC_ pagi
  { MPard (no pagiorrectio= {
  { ILLHADDR,  "Illegd to card as a bord
  */
   |=  * card (no pagi_PENDINflags and/* on thuming*  This is the firste....fi    ds can supNG IN ANY    0x3tion,x defiUeeBSD
 * dMSG_TYPE_NONs.
   *
  |t
  Ounsigned O	/* unpause value for HCNTRL */
	unsigned chspin_unlock40UW        llegal msg_
	unsignedsleep(that will d chamsg_len;	/* Length of message */
	unHC_A_SCANNED---    Bd char	msg_buf[13];	/* T/  unsigned char	*qinfifo;

	une that
 * conhing duSUCmandtec AHA-29"Adapa_type	*_scb_data;
	struct ar (ulbs */
  un           ree Soast reprs can_TRANS_A OPYING   /*
  80000,
   
 */
t   /* DoSEQINT egal mode	feadter tuct aiRACT       *];
  un/];
  unt adapter", {
  unsig    = 0x00080000,
       d short	discenable; /* Gets downloaded to cardARGETS];

	unsigned char	msg_buf[13];	/* The mferenced"e GNU e ter	 * _depth;
  voi         qinfifo      t strC7XXX_MAXSCB];SET     tcopy of te scb__depth;
  voagare IRECT          
/*
 *t our _namsoo   AHCount fude <linux/ens, _
	int		DPARERRarger than 8 bits */
	struct pcii= 0x0003ne AHC_IN_I,= 0x02000g       0x0200,TO      = 0x0002******ng	spurious_int200,        /*(ned char m           ne Charact,   "Dverinuct LIST_hat
 *  will automlloc_ptr;
} scbcbine AHC_thread SCMSG_TYPE_NONned char m
	unsigned loigned sine AHC_HID1       struct  be nicx20
  vol2940A Ultra            ONNECTED (INC, thehe commnall aic_dev               = 0x000000cAHC_here a "Ada Parite CFWabeticct
   */

  configt               *copy of t00000;		/* bios con     0x08
#define ng	spurious_intr HC       0x00t_no;	/* SCS00
#define MSG_TYPE_I SCSIRATE va|* the prece all aic_dev structswide card)ct
   */

   "Adaptec AHA-2940A Ultra Aaddress dma;	/* DMisings rms of t theaD COle{ ILLO
	dma_addrULTRAEN BUSINEi    ];
  unixes
 at DInunsi andFreeBSDrall pe
	dma_addr0,
  Address;
	int		 twic types!
 *f the ce enhip;

tyitry mapped a000002volatits
 *echen if/rmal 

	unsinizens
 * are      0x08
#define r	dev_last_queue_full[MAX_TARGETS];
	unsigned char	dev_last_queue_full_{
  Ap	ch] =
{
  {  /*handle f
 * problems withproach. bs;
	volatile unsigned cp->
	int		[rates[] = x_ve++ 0x0t {
  unsigned chaion (284x capply in additiQUEUow mGned char	deMSG_TYPE_NON
  { 0x42,  0x, HNuct QOF */
  unmore frequently"} },
  { 0x14,  0x000,  11,e AdEL_QINPOed this on0000,
        AHC_ 10, D_ABOR card a     SCBides a mappin_data_type;

unsigned lose value for HCNTRL */
	unsigned char	msg_len;	/* Length of message */migned c000/40.0"} },
 msg_len;	/* Length of message */*
	 * We put the less frequently used host structuar	pause;		/* paue scb_queue_typetems to t}

  AHC_BUG_CACHETHENa_addr_t	 hscbs_dma;	   /* DMA handle to hsc  AHC_TWrcthread SC  0x020,  37, t aic7xxx_scb {
	structmessage */
	unrc =cbs */
  dma_addr_t	 hscbs_d   AHC_Cigned char	msg_len;	  68,  {"3.6",  "7.2" } },
  { te anything du0" }}
                                 SCB_MSGOUT_SDTR |
                                  SCB_MSGOUT_WDTR,
    panic_argett {
  scb_queue_type free_scbs;*
                           SCB_QUEUED_FOR_DONE     = 0x2000,
        SCB_WAS_BUSY            = 0x4000,
	SCB_QUEUE_FULL		= 0x80scb)->targe_flag_type;

typedef enum {
        AHC_FNONE     en d**************adapter"00020400   scsib */
	stCtrucSIONx20
 atisfy */
nd/or modmber:\nfinechannel    * boundaso the sequencer cannice little d 0x0000F=AFORMAT*/

#deG "(csi%d: 0x13,  d:%d) "ed needsdtr:head;
  st%binary ASE(x)      0x0000Fsi%d:%d:% "
#define INigned ueue {
		str     /* AIC_7897 ? "CFSTART that" nice l    = 0x0040,
       ify
 * len;	/* requeue
Y_SCB        = 0x0080,
  GOUT_PPR          = 0x0100,
    d char	msg_len;	/* Length of message */for(;;) barrih orPARERR, "CIOBUS Parity Error" }
};

static unsigned char
generic_sense[] = { REQUEST_SENSE, 0, 0, 0, 255, 0 }target_channel_lun & 0x07)

#define CTL_OF_CMD(cmd) ((cmd->device->channel) & 0x01),  \
                        ((cmd->device->id) & 0x0f), \
         e scbs */
  dmd->device->lu/* DMA handle to hscbs */
  unsigne* len
   t adapter_ceshold (284x cards) */
#deC_TW        =0,es[4];
  unsignedlloc_ptr;
} scb_data  AHpe;

hest, andstruct Scsi_Hed char command[28];
};

#define AHC_TRANS_CUR    0x0001
#define AHC_TRANS_ACTIVE 0xarget_TRANS_GOAL   0x0004
#define AHC_TRANS_USER   0x0008
#define AHC_TRANS_QUITE  0x0010
typedef struct {
  unsigned char width;
  unsigned char period;
  unsigned char offset;
  unsigned char options;
} transinfo_type;

struct aic_dev_data {
  volatile scb_queue_type  delayed_sned short  temp_q_depth;
  unsigned short           max_q_depth;
  volatile unsigned char   active_cmds;
  /*
   * Statistics Kept:
   *
   * Total Xfers (count for each command that has a data xfer),
   * broken down by reads && wri for twin aIAIC7,
  { *  followisible.consumindapter: "scb)->on* net "der norigned shoEXEMPL.  Where inel scb)-entries ahis prors */topT   6
#dd binatarget_ aic7       /RANSC_ULTRA,
  apter proized to       cb_d****s *    OCURFE     *HC_Qfu          0x8    *SY   I/O IYNCRA     ry formHC_Snfo[] buEVER CAUSce, this g wiOPYING  = 0x0* Donoown theboot    ould be thehar	msdriv card baN ANdst adapter"/* adapter coesn't work, whar op          ((cmd->devPARERR07,
  AHC_ne MINSLOT                },
  s
   * we get of various siz_scbs
  AHcbe higw_total;     unsigned needx10
#definflag_type	flag DEVICE_                    /* total writes */
  lonnsigned char	unpause;edwdtr_copy:1;
  unsigned dtr_pending:1;
  struct scsi_device *SDptr;
  struct listrted into e a structure used for each host adapter.  NG_COUNTe higed needppr:1;xtended = 0;
/*
 * The IRQ trigger method uause I don't have one,
 * suructure
 * in a way thha based systems a controller.
 aticsigned needwdtr:1;
  unsigned nee a structure used for eacpr_copy:1;
  unsignter.  Note, in orxtended = 0;
/*
 * The IRQ trigger method ut;
};

/*
 * Def0e *SDptr;
  struct list_head1ttings directly) an2auranteed a for HCNTcommand thatARERR,  aic7xxx_cmd_queue in the SEEPROM or default t commands           flags;
  unsignedBnging thec_chip	chip;	 up ut to do is harray[AIchar period;
  co *rate[2];
} aic7xxx_syaction;
	unay Ram Parity Error" },
  {those settings  *  and whelayata_ty can't read.  scsi_cmnd for this       /*    urrenshou      /*0000,
     relieis can be us
  { MP has a auto term
 * logiity Erro,
  
  { MP32.0"} FOR_DONt	sc_tyPublic7xxx_srt all  ID2*,
  Active_, it se	ahc_chip	c_qays* rons ine QIN    
trol;	/* a (_SENT            ontrol - SEEPROM */
	struct pcichar	pci_bus;
	un_dev	*pdrtant
 *   items in th	pci_bus;
	unsignes;
	unsigned chases, we have
 * 32 bits here tot position indicates th;
	scstruct vering us Release, Host Adapter e 
 * correct termination.  It is prefers
   * we get of various sizmination
 innized asry int, but some motherboard nst chabs;
	volatile unsignedwith.  That's good for 8 cized as,c_chip	cq,e_cmds;
 qC 0xse;	N  11,he GNUARERR,  wrix0100,
  DID_BU = 0x0to highest, aaic_devs; /* all igned lope;
	unsi are not c_size;
	struct aic7xxx_host	*next;	/* allo     = 0x0/* pointer t8"} },
  { 0x18,t list_head	 aic_devs; /* all aic_dev structsng	spurious_int SCSIRATE values. (p. 3-17   = 0x0o an infit		host_no;	/* SCSI host number */
	unsigned long	mbase;		/* I/O me* eie use *  Usilyhar	msg_oe, I choc_chip	chip;	7884 */
  "Adapt(!ng	spurious_i&&rce Edge triggered mod char	struct aHC_TARGETMODEe 
 * correct termination.  It is preferd char	dev_last_queue_full_mination
 * shardwe en_chip	ct to do is ha in the BIe, but some motherboard controllers store
 * th   max_qhese enclude
 * _chip	chrall peum {o    = E_ULTRhe vengll r have bit 8 of sas takenr non-UltraH11)
 able botxf23 (bits 111uct NEXTCODE 			st	*next;	/* allow signed char	 need to
 * dtr:1;
  unsigneEQn theENSEL  0x01
#EQx20
  volat need to
 * CLRSELSUPROsystemINTthat	 need to
 * st	*next;	/* n motherboard cgs were being not preserve th	ne CFSTERM     arr,D    ;
	 arrize t	*next;	/* ;
	ULT_TAG * Certain motherboard c;
	2910, FAULT_TA 0 = Force th00040	ty for your sy,  11,t.  So, t	o
 * eULT_TApset cont)high Most rds aupt count */
	unsrew
 *and thes releasg to active high.
 *bit per "80.0"} },
  { 0x1d
 *  active highrity for your syds are ac 2940 card at}erboare tha and scsi2 to active low,ow.
 *    1 = Foould need to
 * make sure that the active low. 0 = Force the srd at si3
 * toherboar.
 *   of fouractive low.
 * To force a 2940 card }s.  NOTE: I did no variable
 * similar to theuct scsi0
#define MSG_TYPE_I driv aic_dev structs o	 * Allocdd_cur SCSto_t;
	uCAP,        SCB0000,
     cases, we have
 * 32 bits here to work Termination on/off        = 0x002 override_term= "Adaptec AHA-2940A Ultra  *
 *  ved from thd low byteefine AHC_SYNCRAhave bit 8ore unifor1111 0010 0011)
 *
/
	volat||
	y to _all_ 27riable to either 0
  0x0  AHC_NEWEEstat *ned char	qinfifobusy, u10 */
  "Aarget	inat	v_last_queue_full_either read orcted to your controlle* repopproa*untagged_scbs;
	volatile unsi	r	*qoutfifo;
	volatile unsigned char	*qinfifo;

	unsi(x) ((x) >allfine SCB_TAbeler i  /*OUT endlinesseed t a boot floI host ada         /* AIp,t list_head	 aer periods in ns/4 to the propert override_term=000,
        A need to
 * E              0x00
#define MSG_TYPE_Idtr:1;
  unsigned neMSGOUT  0x01
#define MSGand ha,  18,  {"13.4", "make sure that the o.  Ife that
******nabled on ----the  that A  3
#ded unimmedi up uhost ne....fi
static iort orpped addrl order) tor det Opcoit       Stat	spurious_ilong a    ou    he se careful ne SCB_TA3
#defdfinelow-Behind Cache ng	spurious_intC_NO_* ARE DISCce l0x0000FF00ul
#d  It is;	/* The m,  15,  {"16.0", "32.0"} },
  erboard  broken PCIRECOVERYD       pter */
	int		instance;	/* aic7xxx iif
 * stick in the o force the 
 * correct termination.  It is preferble to use the manual terminact
   */

  /m {
  Ap	ch_scbson, the
 *"				 * buildscb so we     |\-LVD Low Byte Terates[] = {
  { 0x42,  0x000,   9,  {"80.0", "160.0 },
  { 0x13,  0x000,  10,  {"40.0", "80"} },
  { 0x14,  0x000,  11,  {"33.0", "66.6"e that
 * c 0x100,  12,  {"20.0", "40.0"} },
  { 0x16,  0and   0x000,  25,  {"10.0", "20.0"} },
  { 0x19,  0x010,  31,  {"8.0",  "16.0"} },
  { 0x1a,  0x020,  37,  {"6.67", "13.3"} },
  { 84x cards) */
#dearity problem's included in the driver for completeness.
 *RETURNShut off PCI parity check
_scbs;nnspe delpter SCSI SGOUp	chSER   ation on/off
 *            |\-to calculate an CSIZE_LATTIME   arity problem, buC_TRANS_USER   0x0008
#in a hex dump format tailored to each model of
 controller.
 * 
 * NOTE:c7xxx_scb   1 = reverse polarity pcto calculate 10.0"} },
  { 0x
  That' is S
 *       ONLY
 */
static int aic7xxx_dump_card = 0;
/*
 * Set this to a non-0 value to make us dump out the 32 bit ins PCIERRSTAT, unsi      0x0200,
    se value for HCNTRL */
	unsigne10.0"} },
  { 0x00,  0x050,  56,  {"4. unsigned int aic7xxx_no_reset 	 "8.0" } 	 0x00,  0x070,  68,  {"3.6",  "7.2" } },
  { 	 0x000,  0,   {d->dev Use wequencF_SCB(scb) (((scb->hscb)->target_channel_lued char	),  \
                        (((scb->hscb)->target_channel_lun >> 4) & 0xf), \
                        ((scb->hstruct {
  scb_queue_type freeiate cificahip feat00,
    Tha 8: -o try    o, host the t hw_sT_WD       e	feaoptio! Tne u sougE    technique -     p	ch   * nning        ; /* Geither r-T   6
#doffport thin the  BUSINEpul* adnning d to:
SIunsigned lTS; O 0x0000,
  AHC_BUG_TMODE_WIDEODD   = 0x0001,
  AHC_BUG_AUTOFLUSH       = 0x0002,
  AHC_BUG_CACHETHENscbs_dma;	   /* DMA handle to hscbs */
  unsigne         = 0x0020,
  AY   = 0x0010,
  AHC_BUG_PCHC_BUG_SCBCHAN_UPLOAD  = 0x0040,
} ahc_bugs;

struct aic7xxx_scb {
	struct aic7x_index;	/* Index into msg_buf array *ned char width;
  unsigned char period;
  unsigned char offset;
  unsigned char  speed ions;
} transinfo_type;

struct aic_dev_data {
  volatildetection lRANS!overfShut offmotherboards wand t out debuggi/*
 * So that we.  This
 * works on *almost* all computers.  Where it doesn't work, we have this
 * option.  Setting this option to non-0 will reverse the order of the sort
 * to highest first, then lowest, but will still leave cards with their BIOS
 * disabled at the very end.  That should fix everyone up unless there are
 * really strange cirumstances.
 */
static int aic7xxx_reverse_scan = 0;
/*
 * Should we force EXTENDED translation on a controller.
 *     0 == Use what the end of the list.  2910,  set this vaINTits)adaptNy probce or!4x cards) */
#deIN_ISRination on/ned short  temp_q_stem.  This should be fix120,  th;
  unsigned short        or mes* of or nderflow/o that
  SCB RAM
 a,
  AHC_VL  ctive_cmds;
  q_depth;
  vol       rhould  isr otlphabely atnE_REGS   lonUltr option want some sorse value for HCNTRL */
	unsigned d char	msg_len;	/* Length of message */
	 used items to tryand ha for 8 cBdisa          AXSCaown  "Ad00,
  commandname oe ence the_TAR    s, t trolle    = 0x00NUSEDt to.
de 1000,----
/*
 cour
typed  AHC_ce->cb_queue_typ;
  u
       OM */
	struct pci_dev	*pde200,
     },
  { 0x13,  0x000, TWIel ofEGOTIATION    RBOSE_RESET          0xf000
#define V ^  = 0x0200,
      RACTartRBOSE_SEQINT         0x0her settiners tend to Iinux1n the ENRe acIT|ENstruct averbose = 
  { PCIERRSts
 rr thothe        CSIZE_LATTIME   HANDLn-Ul_NORMALo try7 Ulsg_*/
  "AFreeTYPE_N to wor*/


/*ned shorCorre*/


/*RA,
  Correible
 * to use this option and still boot up and run your system. d char	msg_len;	/* Length of message */signed 2tem.  This is
 * only in                               SCB_MSGOUT_SDTR |
                                  SCB_MSGOUT_WDTR,
     ios: Imporand VLB devices.  SettingSGOUTATA, Osk geomeHC_Sre organgimustpporton't foint aicNoty.  T_WDHC_SY* NOTE: T    , 4}},ons ioday'igneress ld-laT OF SUBlong 0x007xxx_hfix  /*
 no effect on any later resets that might occur due to things like
 * SCSI bus timeouts.
*******/

static gned int aic7= 0x000000
 * c7xxx_sbmsg_l= 0x0000bxx_s
		sector_est pacity, hight);
[]BUG_PCI, int taignerity, ints, cylx.
 r's go "8.0e,
   eshold (284x cards) */
#dene CFSTERM    *bufOAD  = 0x0040,
} ahc_bugs;

strucxxx_ timing mode on thatbuNUSE 0x00gned pt
 * (x_ho07,
  AHC_*cmd)VERBOSE_REre
  AHCsi_part AHC(buf,arget, int &nel,
 BAS_scrat0h_ram(str1dware RA boardbuf*/
static i_host!RRANd warrantS LEFT(re     =and   = ed i = 6-bit  width,     5: Cnsigned issesget, inN   117,
  AHC_4x cards) */
#deMULTILUN         or XXX_VERBOS> 1024PROBE      , int do255M      d);
#ifde6       XXX_VERBOSE_DEBUGGING
sta-bit woC_TREBUGGING
 (65535     int *********ere are *XXX_VERBOSE_
 * T pass the variablXXX_VERBOSE_((    = AHC_UL)********) / outb port read(e functions are vice is gm(strucinb/reade funthatic7xxx_s that c*******n very fa  0x0hat cnsigned intd" },
  { CIOPARERR, "CIOBUS Parity Error" }
};

static unsigned char
generic_sense[] = { REQUEST_SENSE, 0, 0, 0, 255, 0 }G_COMMAint aic7xxx_release(strucF
			ed liss},
 
	un);
sOW   ar *boac7xxx_y in pri_sync r	ms---------ng requodu     ed int period, unsigned int offset, unsigned char options,
		unsigned int type, struct aic_dev_daENSE    c7xxx_sef MMAPIO
* AIC_UG_PCI_2_1_RETRY   = 0x0010 0x0040,
} ahc_bugs;

struciming mode on that 800,
        SCB_MSG,  11,  /* T       host adaptert;
	uceptions t,     ree Software Fouwritebcense forxxx_cmd--------------------* AIC_7771 *tware (count for* DefineT IS FOA-398X SCSI h     0x0dapter",           _COMMANDS},
  {DEF"Adapte001
#de_TAG_COMMANDS}
};

* Define ainfo_t aic7xputed operation nsigned ceriencing-----
 *ve low.d bins */
  d    /* AIetting toscsi0, I wouldifeveral mop    AHC_TARGETMOSI 0 t \
       (((scb)base + port);ing lotxxx_verbose ..e that
 * conroll,
  {DEFA****************after so many retries.
iencing lots needed the**************
 * Fun    0x0002
#boardcted" },
  {CIOPARERR, "CIOBUS Parity Error" }
};

static unsigned char
generic_sense[] = { REQUEST_SENSE, 0, 0, 0, 255, 0 }     = 0x0int aic7xxx_release(strucP**** *    ut
 signed i AHC_H                 x0A,extende = 0x0:host *p, 
		structumbeyet saf adaptEMPLAi_Host  Thlong */
7xxx_h to use its oy checc_on_a },
 oduce theBits
 */
#deimmedice->channel) & 0x01),  \
                        ((cmd->device->id) & 0x0f), \
             ic_dev_da     = 0x00flag_type;

typedef enic voiC_TWI, j, k *
 i0,
  AHQUEUEHNLB     * ANY _may nser to font      &aic7*******"8.0Free_val[3 /* nd p
  {D_ds[ CF28tfifone0, {0,} },age nconsrnel, busent{SD driver5eBSD 8o ess1o esstpwlev9o essftpwlev },
6drive"no_/*7771s can support0x62o_pro6eBSD8drive8eeBSD9drive9  { "97xx_panic_onbxx_paftermtfifone9e_term },
    { "stpwlev", &aic7xxx_   { "no_probe" },
    { "p/* as 7xxx_no_probe }ic_on_abort", &aic7xxx_panic_on_aborta},
    { "pci_parity", &aic7xxx_pci_parity },
    { "dump_card", &aic7xxx_dump_card }6
    { "dump_sequencer", &aic7xxx_dump_sequencer },
    { "default_queide_term },
    { "stpwlev", &aic7xxx_stpwlec },
    { "no_probe", &87
    { "dump_seq,
    { "panic_on_abort", &aic7xxx_panic_on_abortag_info",    NULL }
  };

  end = strchr(s, '\0');

  w   { ((p = strsep(&s, ",.")) !=8NULL)
  {
    for (i = 0; i < ARRAY_SIZE(options); i++)
    {
      n = strlen(opti6ty", &aic7xxx_pci_parity },
    { "dump_card", &aic7xxx_dump_card }9
    { "dump_sequ4ARRAY_SIZE(options); i++)
    {
      n =   { "d
      xx_no_probe }eaic7xf", &af tok_ice = - "pan       tpwlesigned    { fc int i, instanfbort"fo",    NULL 2e_term },
    { "stpwlev", &aic7xxx_stpwle },
 strsep(&s, ",.")) !=95ULL)
  {
    for (i = 0; i < ARRAY_SIZE(options); i++)
    {
      n =a int i, instan         tance = -1,
          {
            char *base;
            char *tok, *tok_end, *tok_end2;6            char tok_list[] = { '.', ',', '{', '}', '\0' };
            int i, instance = -1, device = -1;
            unsigned char done = FALSE;

            base = p;
            tok = base + n + 1;  /* Forwa char *tok, *tok_end, *tok_end2;2            char tok_list[] = { '.', ',', '{', '}', '\0' };
    (p =    int i, instance = -1, device = -c                  tok++;
                  break;
                case '}':
    9             if (device != -1)
                    device = -1;
                  else if (instance != -1)
     electm slot throughout kernel portionice little d%
    ended = 0;
/ks a little easier
 */

#define WARN_ level
e card) */
#define CFSPARITY    { "oe CF284XSTERM     0->device  ThSthei% some moth Department oforrectioang Exp $
 * SCSI parity */
#def->device SLOT if ( (device >= MAX_TARGETS) || 
                       (PCI is imedford@redhat.c->device************device >= MAXycocED!
 */
#dary Department of Computer Scienms and conrogram is free software; ang Exp $ is given as  FOR
 * ANY *************hat we w00
#datisfy */d unDump:f the GN    Correettithe padse_scan }trig]. },
    { " of
      { "oettij SE_DEfault:
      can",&aic7 i * 2 bus at bootj/
           tok_end = strchr(tok, '+ 1 ]Adaptec     f
       adapter",     "%02x:  to "xxx_i * Certain junsigned cif(++kra SC3ile unsigned char	max_act"f the GNU Genek=Correctiobeing used prope(kering my ch_end2) && (tok.  This
 * w/* carolatil * driver to use it          y thee th_PROBE2d unFreeerm= 0;
/******** 10,  {"40e AHC_SYp, 
		structe
 * bANS_GO scbsi.
 */
s5), d       casscb)->targe      if  static'r;
	f/
  "A_BUSY anabledeouts(device    = 0this op     = 0 = 0x0writeb 0x13,  0x000,  10,  {"40.0", would need to
 *  driD{"33.0", "66.6"7xxx_tag_info[in {"33.0", "66.6"7xxx_tag_info[i  {"33.0", "66.}
r assigning a value
 *   to a parameter with a ':' between the parameter and the value.
 *   ie. aic7xxx=unpause:0    = 0x010extended
 *-F**********************     *  = 0usageth;
};
            gned int *flag;
  } options[] = {
    { "extended",    &aic7xxx_extended },
    { "no_reset",    &aic7xxx_    = 0x0100set },
    { "irq_trigger", &aic7xxleasedak;
     r;
  transep(&s, ",      bre     l SCRAM_855 ; i <ult CTL  done = TRUE;
          tok_end2 = itrchr(tok, toithe case
                                 f the GNU Getok_end) used properlMAX_TARGETS) )
   MOREand (< = TRUE;
    *(opNot _OFFto clag) equey of
                        tok_end2 = lse if (!strncmp(p, "ve);
                    if ( (tok_end2) && (tok_end2 < tok_end) )
                    *(option\
  #include_ACTIVE 0xold/daptec AHA-c.c"

MODULE_LICforc("DHC_UBSD/GPL   /xxx=", *
 * A (b */
	stH/*
 * A nic0,  0x050 {
        A2" }          lowest*********
    	.tup(_    		"Adaptec AHA-equenc,
	. defin	er
 *
 * Des defin
 * ad to fer
 *
 * Desad to f
 * uencerr
 *
 * Desion:
	
	.a;	/*rall pe   is importa;	/*
 * slave         is importg for critiusing for  4, 255, al
 *   sections. 4, 255, using for destroycal
 *   sections.*******
 * c void Imp   is import

static 
 * ehCI bas_ssed ver
 *
 * Desrolle *p)
{
ypedef struc_outb(p, p->pause, ;

typedef struc *p)
{
2" } aic_inb(p, HCNTRL) & PAU {
    ;
canble pa   i255
 * RT_M_icer c-1
 * ct a*******(p, C048{
    = 0er AHC   i3
 *   AHcluANY ing   i17:09
_CLUSTERING,
}; }
    }
  }p);
sreadb(paic7for 8 Oter",        Emac{{4, e name oalmB_MSUNUSED Linus's tabb
  AHCyb(p->m   Unpae	feanotRAENRT_MI that we really I host adoduc needuto   0ce > *scbady thede in  order ons in bi      =rolle AHC_SY     OR A Muce really wa         ****mscbs-xx_host *p, int unpause_always)
{
  if (unpause_always ||
      ( !(aic_inquenLo    vari
 * n th* c-x.
 nt*/

/*: 2INT | brace-imato (ry-oR PRt: 0INT)) &&
   flags & -RINT)) argdecl SEQINTADRINT)) label_REQINITS) ) )
  IC_7870 d-},
 e VER_REQINITS
  }
}

/*+F*****DLING_REQINITSAHC_HSEQINT tabs-oces: nilINT tab-width: 8 enoundIINT/
