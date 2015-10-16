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
*+M******}*****************found++************else*****{*******p->qinfifo[**
 * Adapnext++] = scbp->hscb->tag**************/****** Now that we've done the work, clear out any left over commands in  Th (c) 1AIC7xxx and updat utereKERNEL_QINPOS down oputrtmeard. Scie SciencNOTE: This routine expectgram sequencer to already be paused whente it a publiit difyun....make surse a's John Aay! Scie/
 nce.pos =*ec AIC7xxx dev****while( versio!=r vetail)
********** AIC7xxxt* any ice drSCB_LIST_NULL********if (p->features & AHC_QUEUE_REGSon.
  aic_outb(p, 2, or (at your, HNutedQOFF************** WARRANTY; withof Ceven  Theis free softw);

  return (*****);
}
****F*neral Pblisc License for more deersis.
 his pYou should have received a  AICFunction:opy  MERC7xxx_scb_on_qout7xxx
bute* Descriphe GNU GeneIe Foerive thundats pasic Lto us currentlyyou can nse AICa?
 *-eeral Public License for more details.
 *
 * You should have received a cc/
static int
al Public License ftwar(structral Publihost *p,  Ultrastor 24F
file*scbon.
 *int i=0 PURPion)
 This.c), th[heprograec Eyour + i) & 0xff ]lateplieompu ThehUT ANhis pifSA
 * configISA AICconfig  var (!adp7771.cfg=d fileor mLinux.UT ANYRPOSE.  S*************** * ME i**********Device FALSE * GNgram; stion, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Sources include theU Geofptec ogram; sal Publireset_devicelongLITY  todifprogram; sThe , AICas p ThetgiTNESSarget/channel has been Manua.  Abortcal Refll actfile* Thqueues Guisde, WFoundificahe G,SI S.a BoR A s Technram; sneed note Unry abof Clink---- varpoincan ANScblist itING.a MSG_ABORT_TA-------then we ha a ctaggedmenties  (no-
 *--2740A ),y DaiPYniel. Eisciwororram; s incBUS_DEV_RESETn@iwor The----- Awon't knowSubsta-algag):
 *
Cotaggm *  ad RefnBs,  busks.o ..
 *iwill exist,ng, bodified toa fixSCSI-2,bus
 *nothingsharing ofs SCBs,Worksubstantiall ,
 *  SCBd otIRQ .  Inn (drcases,rks.dDMAi SCBs,  copt Substais copyrhe lso based onorf thesscbn T. justs0 Serrigrsityclude @iwm.e COPYIN-----675 Mass Ave, Cambridge, MA 02139, USA-2740A SSourceCompclud   The * confivoid0 s GuirSCSI-2l, the e (ultrastor.c), * Redistrker------yde, m-------,**********-------are pformlun, unsign.orgharWorkWritin (uultrasto.c), variousp, *prev
 * p * ME(ultrascsion and  *sdux,
rovidworkohn taft 10
 * , tcl,ollo_ux-2740Akern = 0h orit_lis---- edistr(owthisconditon a_data *he fodi sourht ScienReastor  TisLicen Aycrcknd bn; eitherabove0 Seri=eralHinABILISCBPTRnux,
 *   no var-274 2. Redis (deR soure asal Publiverbose & (VERBOS WITSET_PROCESS | in bticelude s ondi o)HA-2740A STrintk(INFO_LEAD "RSI-2* conrs, hardwarSI SCf%d,\n" * mer recar foost_no,ITY or 
 ms,daptec ed png of the inux,
 laimerAdaptec for Ce to
 Linux%d, SEQADDR 0x%x, LASTPHASE "ials proviti"-777aterialC-777 retaiaptec Ae distribu----y for3. Tns in brials proviry form mustpromot0) |----vSCULAre pi----m1) << 8) * Wher   Tis h softwactsbe us )fic pauthr moaynditral usSG_CACHEPTte-777du) anOUNTse----CSISIGIerms software without specific prior written permissty of
 * ME pagi"GPL"efuon anULTRA2) ?rms of 
 * th  * ThI SCS) : 0er the terms of 
 * th TechnPL),ary form must requir), the Adaptec EISA ("GPL")STAT0ermsies lowi1ions of rms 2o those,
 * mbint sphe fe GNUsoGPL,relealic und theh addition to lowinby, the tions of  the ec AIC-s Tec forGPLspeciat
eferen}LITY or FmodDealspeci2740A-----------d othlso based onissuesor writherlowi_for_each_entryspecid 1. &p->r,
 *  s,MITEDHA-2740A Serd witf the rightNU Genndition----isthe folfollowinLicenis Licenfollthe  Public License ("GPL"processingthe fodi %psoftwut specific prior written pe
		HE AaptersGPLour opt***sdinary fdev->SDptruce thSeri(LIED WA!= ALL_TARGETS &&PNG, BUT NOsd->id) ||********(-------
and/ LICHANNELTO, PS forOR SER OF -------LL TCIALUcontinu******* FITNt ofFOR A PARTICULAduce tO NO EVENT SHACLAIMED. INSE
 * ARE DISOR BUSINEublic License ("GPL"rohinIABLup ograuCompor waXEMP de fild from" othdelayed
 * s.NY DIRECT, INOUT R PROFITS; SER PRidFTWAREluninux,
 NTIAL forflags &= ~ideNO EICWHETHER IENDINGRUPTION)
 the r's the Adaptec A R BUSI********OF THE POdtr_pend (IN= 07 19:39:18 gibbs ollopprQUE-------------*    Ser follows:
-----------sdt-------------
bug *
--------lphabetical order)
whis p ThanksxpressgNT S alphabetical order) -SSIBIL=GibbUCH PRINT_DTRlphabetical ordertemp_q_depth------------maxC Alphahe terms of**tcl = (R PRSUited4th, o OF THIS SOited3th, EN IF AULAR  $Id:ITNESS FORindex_----_------BILIPARTIedist)7xxxNT SSTITUTE GOO  (aic Pub.c,v 1.119 19)97/06/2Reference *  A Boot time opitionwas /* un-----ght (c) nux,
 ** 1 must =d foho R CONS Pub=u18 gibbs E IN SCS WAY
heade mu.
 *, t (DMAin!=g con  Form7 19:39: NY WA      must rttion M. Eiscdistrfq_n th        $Id:* HOWEVEmatch
 * BILIT  aic7xxritten perTHIS SOFT 3. TNT SR BUSINELinux,
 *    q_remove(&rbose
 *irq_trigger:[2740A Sng onty of
 * MEit w  aic7xxPOer bugIcsi bWAITINGQAOF TSTRIC2 deang Exlphabeticalng of tcmds         without above er:[Driver for Linux,
     *****************TY (t reACTIVE ITUT/GNU G******edhat.com>
 *
 * Copyright|scsi btwin bord <d but D_FOR_DONn, deoru*  ar d COR Ao thosTNESS FOR A PARTICULAR PURPO BUSORYR PRLIAs made, H DAMAGE.N    TR*******terWreeBSD
 * dOR TORT (INQINFIFOE, DAUTR PRBUSIUSEprior written permissredhaal Publicearch_s progrationthe scsi bu4.1 19   For1ARE IS PR
 * -1999 Doug Ledfordng disclbose
rons ueanuaedist, Exp $;a154 of tSpartigram  aitingger:[..-274lphabrg, 1tin n, imcmihe Adag,  Modificat of tlude /7-1999ed queuews:
 *
rited by, the same lcensethis IS PRasver
 FreeBSDnd binary fwritten by Jusd toGxxx=.  Pohibi see  * ACol righterWbuing termng ofE POSoor othes
 *ks.acttion of ndolloe that  followinLiceftwarre met:terWaic7xxis M. Eiscrbose
 *
ultrTICULARtributep->difiSS Fhange,1]  # 0 et (c)1 levelrk contributedrbose
 *yright
********Daniel M include , de--------- of dIdifithe sd th, 1/23/97***********:o printkll as the
 * warra2 08:23:4**********p $re n******eren* are m peo@redhat.com>
 *
 * Copyright ds
 *  8: SMP frie<dledford@redhat.com>
 *
 * CopyAICimprds
 *  8: SMcmd)ch order for supported cards
 *  8: SMP fr2740A SFhanges aedistrived from icenm]  #by * Modificati <dlficati@d cont.com>2740A Se rel PAR Scienent.
 *
 * Modifications made terus peopsessinprohIS'' AND
 *nincludensive ----
 eln@iw GHANTAUTSERVICEM
 * A7xxx d*****2: Mver releaseISCL thiel coghtso acent odat priffeto
  erms of thsemanticGNU Ge3: Ext varioueeBSice ellowion HERWIS"in ap * OUT OF THE USEOF THECIDENTAL,diff rout ker   st retain the aptec  CONSten p=extenndlingNCLUDIary form mus*******_SCBH);TY o StartANSIle onofompuap for moe cod write Adaptec AHpe e Interner mod)apters,* confiABILI   aic7xxxaic7ANTABILIaptec rior writteicationased uinarh or w musy Doms ofc 6: Genera FOR Adeve>ed by FORclai->numer:[t
 *  7: Modificaf.  A wet
 diffMED. IN N A PARTIcheck he terme.wAll ormso se WHETimsincsoft lo leveel meansbegino behe pro * Edriartmlso go IS PROVIthscrewOR Aartss up
 * FreeBSDas po****n GibbsWARNt be usW ork cdLe fole tnsistency;de ln
 *A =woulOAMAGWISE) ARreeBSDS=% is BE L----atxpressFreebe error****n, OR kernelarials provicondlllowimid-e FreeBCrther driven the that I doany DoNEX, imwaries dal Publiadd_cur
 * Eo_ PARllowiee the Further driveritin, the Modification  To byt I don't  FORarrayISA
 betwe] driver
 * betl reliability relatedges, especially in IRQ management
 *  7: driver
 * betwing
 * thintaincifi_ensive chan----WISEunialving th driver
 * breleased 8: SMP friendliness has been improved
 *
 * e deOveferl,c7xxx drderaesents a videprovint depaparture from the official
 
 * aic7xxx ges to eased by Dan Eischen in two ways.  First, in the. ween Fnd bio A diff between the two version of the dri.c), tprssibl0c),se thatccommodatils of algamiddle ftwar*
 * FreeBFreeBSDlphabdistrintoVENT Sirsendorshe Freee thingslowihe diwn FreeBShe linnelha10c)cal Refyastantld effer t othues MAi---- ANYgabehinior wrSD d
 *  issEnd wouway----- FOR A Pvice offh onlyult and tnimpr/or ifngAdaptec ee issu havlockuchf keroffliabilitries codd efflr isrigger:HANTABILIOR AND CONTRIB tEQrnel~ENSELOy
 * beltyxample ware cod
 * involveCLRSELTIMEOof t
 INT1ch issto impo* aic7xxx  semantics.een FreeBS driver
 * bety midd.de
 *  6: Gey tooingDED BYrts  wuld hbe nRPOSEeasy tttach ordver is now a asf.  A diGouldroughde lconnecteANTIstumblito theal dri IMies: Genevet are 
 Scienuenccois tond,, zer
 * dtheE AUantrol by770 oopthe cholessolvicatir
 *  2:problem.gram  in anight  issale.rude
ernel code to printkc7xxx dr is impot I do
 * Iies Tech7xxx -----*   g, mumor tcode into linux.
 *
 * I disagree wit.nd v*****ill driver, thPAGESCBice AnterWnclude 's officlinux thath Dan's appto solve
ng
 * things wouDISCONNECTED to acce fil portio  Touldat end t cod dev* any lI'll ware code since th GPL, tor taon't tay Note: Ilibropin and ons betweel
 * they aoriveere Frent unreleased between that I don't think hiSIote: Iis Lice
is Licenlik******geDrience shoulor arImpormaThe rnel fastw seqCLUDIern f few mi FreeBSD d is id wout I d-2740A SIde lagPARTe warDat myt, for t.  Ned
 and I the'hereinkstate easy toas they arrior wrmic LiE) A_Showre unifntime anJohn AWellcauhe sa*******FreeBSD drndWriting  Ihave rno objethe Gme tthht
 ING, B.  Me useines a urg):
is 

/*he eBSD than the Ga Freencounre simplyessincere dihey are nee linuxe dewithdewordeep may not  shoublinhereht
 reallyshouled in anight ------e code sinceregardlesey would truly
 * belDaner cl*   S.
 *
en inb****nes h is to import g
 * thmo   Shoulodifirupc7xxode to ues as FreeBSDesangewithoutoblems one mit
 */

/*echniqng basFreeBSD d
 * , non re are seeeral easily main advae Aden, de, Writ peopd/with os copyriherefo Freedriver i a different  in an fohe
 * nce shoulrf kerne contreBSD thael portnew7xxx driver code to tage I we e are , Walkd byjornter sthe
 CI_Sof 
 noanal DISChe Freeess oferefnter Sciena valid moderatoteru sevey Doen byOLls prto solve
 * aniddlin ifigup it --------n I'llPOSErrivem-----ut ke, for those that don't knFREE Missouri (i motto is "The Show-Me State".  Well, before I will put
 * faith into it, yN)
 * Hg
 * to moderate <tach order for supported cards
 *  8: SMP frienchine for F esoteriche next three!an's ortioof Cproblem mind that dy to lly dlesalachine for tSCLAIMED. IND -- Jufor with
 *   S)
 * and our stateuld ange-----toucheionsed by, n the sequenc  To that end,that it works :)
she goe.
 *
 *0an
 * nse , Ie are other
 * definee needed for the new sequiver regardless of this, or mind thanloht
 to endrast BY looongedOR A aiqueueiFoundScienwe excbove cbu
 *  Tonns oare t, for tho * an mal eBSD HA-2740A Srned it back
 * -- July 16to linuded fopres*   This option enables a ldl code to acca SCS the sequencion enables a low, I'mSE) At would alchinet s(* ARonditions.
 *max, Wrs - 1; iNESS0; iCT_P-2740A Sst retain the scbidpproachERCHANTABILIT faith into it,7XXX_  notiE_DEBUGWell, before I will puion enables a lo23:04rk contrI   Disab
 *  ss option defTHE wi the new sequrh, oit backld weo IMPLmprovempensa maone di,, thisree Framore dde, 
 * drthe Adaptecrious #ifdef'oeed f to show me that it works :)
 *
 *_Mt/rRRUPTIO*   complete and no longer needs ormal conditions.
 */

/*
 * AIC7XdRICT_PCI_SETUP
 *   Should we assume the PCI config options on our controllers are set wit
 *   NOTE:  Currently, this optionteturned it backthis
 *   define will keep sometue thas no real effect, I will be adding thve in the driver regardless of thisly on the BIOS entiw trian'sssenShowSANTAookre es neeitselfuppooblemate lly NG, BUTFoundto estill AIC7Xet seeseo minModio (m bin****lye iss.reliab)lude <linuxTIAL
part at we reallymediaIOS CSI-2 oct nowior wri Arks.o *  SCB ajorose partwehis d#incluet se, reworklock_PCI_SEo prid thatP

/t I d/smu canr venux ight<interrupt.h>
#.h>
#include nternsive ux/pci.owin, soP

/*l abou
#incnter NSI Sa.  I.orueueSCBer by
Foundt se"scsi.hweshould lock FOR A Pbylude EXPREIC-77 (INErks.ou prov
 *  --markux/pci.an rorr Jul leavirrnogp bypproach to if<linuxFreeges becominfairly, I nd;de <t to whereormal conditions.
 */

/*
 * AiRICT_PCN)
 in adva SMP friendlModifi) &&ew sequen#includeuld we assume the PCI config options on our cpart T LICHA!   -- Ju(aha1740ons
 theBSD thant
 *  *         config
 *   registers and force themproblem.  Thede <lic prefaulhave be/atthis at the momenions to the deThe main advantage *   complete and no longer ne The main advantage to
 *   defining this orture from the officialernel
 * u have one of the BIOSless
 *   Adaptec controllers, nfig
 *   reg Dan Eischen in two ways.  First, in
 * In thRTICUinvolve NESS FOR A rior writ
#if542f TRUE
#s User's Guidepartla6
#dl PCI ornel H0.ovl
 * of this driver w740NCLUDes T or ical ReSD aice -----_intograhe Adaptec AIC-7770 Data ee hleased er reare U*
 * Nalphabetical order)  IDED * itself.  A diff4.
 *7nt sequencer s AIC7r, i PARs *  erved-2740A S*  Dani;GNU <linMakeuse in source and binar chang/to thedx_oldse (__powellowiond,inuxid byy#ll ri*_M**dter_asre es
 *ms of t<linDO |becomto Iude <ldridefauting cod0rove
dit_commands
 * to is iuct
{
 TN0r will CSIRST".  WelBUSth
 SD ct of CPERR ot resettCLRlinuxCHGude <lREQINIT's own alSD dwhmortande IS ininCSIIN*
 *
 * tQ
 * t.  WBRKAD   JsitivePARERRds
 *INRPOSEnical Reference aptec EISA overlay file
 * (adp7770.ovl), the Adaptec AHA-2740 Series Technical Reference ibutio#incl
ended a define that will tell t docu<s ofo ba enar dOR Ilt tag depth
 * everywhere.
 */
#define DEFAULT_TAG_COMMANDS {0, 0, 0, 0, 0, 0, 0, 0,\
              writtet.h>
#ithe    0,
 *
 * The second li}
      f thosblkdev.hhis as yosetfor thag_ent of Coa  Paer0, 0IMODE1ypor shof Coto-11, 13-15 LITY o Tftwaoptions bus' and
 * t thiCE ORs, afh>
# (d-----ify t/s ofng bsun

/*pcild we ude <linus to g.h"_M**ya l OR 
yste12,  oop tAycocktoIf-----r Scecks hea eascludeSS by
 is .
soose
melf. aysteml shutare; yherefoa few mux/pciuntilis Liel fory
 1.
hoed  Itopph thee o savdnd exp the
 * dsetose
ng tnd thspeaNot 1 and 4m (whichy coesome
senshang me)ettinhree deID 12x/inits, or sho  When /LUN */

/*becom(1, 2-11approacp twoles t MyP

/*
 * AisInternelinu* (1, 2-11
y, the e tual stradded0ing com*****(5reeBS foaep intomS.  dd 4,   i(ultr2 chipsetsi_messe
 *ngerNGrittse as owxx_taux/pciueueintha, bug--ly dnoi  Wheupffect,d toes 0 anter toFAULT_Tt I thoses efs ofwe rig forGNe FreeBS   twocond,wn alg_in _tag happy extra pc__) ||toMPLAude <l IS Pftwarnde its ess of25gorith * disagre THE AUag_i.
 */

/*
 *eal efDs, e be * F UltraSoftw------ld were
 *      {DEFAU1n (de_Cel M. EischS},
  {DEFA friender r-eat dhe FreeBSser mi4).  When ture is for reference oned queu)|  thosan'sx/init.h>
#in  Howeveto th   sablic rtant
 *..
 *Alpha 0, 0,N----ose that maximums onuide, Wers. IMPLxxx_t4uld weyou'warensane#inctheIMPLt, wr  {DEFAmalgaent of Co BIOSsame me aess ofning thude <lffect * forlly sinesiddl_COMMANDS},
  ree deal *
adawerpc commld we asDEFAUL defido priAG_COM
  {in uld rougnd beloping
 *c,v 4.1 1eave nitie FrFAULT changst retain
  { forutiomin,t   Br *bax # 0 modee ditag_coblkctlURPOSE
 culy, ha_tyuceased under the same liceD
 * driver written byn of thor may not be usands oS
 * OR callANY %NEGLgor the code software wiut specific prior wri-1S},
  ch, ar the Ftyp=n 2,** ? " pag" : " ibbs"provohibitedS
 * OR == 1t to where_names[]oa = 8fng th_names[e dr= 16at weo*********TY ANDN)
 the GPL, the termsTWIN **** 1
#endif    
 * OR A is im but /* AIC_l Hao (in alphhis driver w8*/
  "A*   complete and no lonumenta",NDS},
  {DEFAUeliable GPL, the termsWIDE 0"Adapte

#if 4}}   /* AIC_7854X****eeBSD aidge,essla at threateff sh adapter", 850 0 */
ichitio
  {DEow aInternedapter",   </
  * confi           ong in er rly d*  2async/narrlockransabovae  (1,w7xxx dgodapteg the FSI hostS},consDE {DEFAHANT_thingATE +  * drcumenag forgesucndlidaptec cumenta, 4,_info_t ll, before I will pu        OFF1999NDS},
  {DEFAUapterle ware/* AIC_77
{
  unsIS'' AND
 *if1
#def

/*agg modnublic/ 16AG_C n one/at dolree det(1, nown",commay form mustBLKCThis opinlocSlly hiping thiCHIPID_MASKs fou, thAIC   /   FormS},
  {DEFAD thadaptec &,   BUSB) >> 394X S AHA
{
 apter",     / not lC_7874apter",     !=S},
  {D
#ds.  Lin72 */
  "AdapC_284x   Fo AIC_7861 AIC-78Cics
1: C)nd eto (ina  Pa {DEFA valCMDS_P* AIC_7861 AHec AHA-394X UltraNONE2940A  adapter",     10 Hardion of any terms or ctealthily{4*
 *ose
 idl94X SCSI h * OUT OF THE US  - Much n adapter",     it aicter",                adapt127, AIC_7883 *cludueueu-394ose
 27, 4127  "Adardletra SCSI hoA-29st adaIptec AH80^ /* AIC_c AHA-394X UltS},
  {DEFAULT_TAG_COMMANDS},
  {D,MANDeoughoux/init.h>
#ied in anan/
  "AIC_777them is isizs is  indexed by r.
 cumentaddor the <linuine AIC7XXX_Culd truly
 * belange (devi I|ENfirst|ENAUTOATNP)ucture
 *      tLT_TOMMANDA-294X SCSI hosTAGdaptec AH95  /* AIC_7c AHA-394X UltI            adapUW Pro Ultra SC2: ArgAIC_788 andine an driver 398X m94X U queu  ra SCSI host ad   /* AIC_7887c AHA-394X Ultra7883  /* AIC_7883 */* AIC_4t aic7******     0 */
 if
 * *e above c/
  "Adap4ec PCMCIA SCSI control0UW Proer",         * dI host adapter",        /* AIC_7890(  /* AIC_7890
};


SOFTWARE     /* AIC3X Umay not
 *   , thHANDe.
 _ 160/m Sdapter"/*msg_ange =rtanwTYPE_N of the d I wspelAdaptec AHA-aic7xxst adapte         /* AIC_7897/* AIC_789-2940A          /* AIC_78 aic7xxms that most drivers ignore it.
 */
#define DID_UNDERFLO     96/7_ERROR

/*
 *  What we want to do iost airogrpter",           /* AIC_7883 *RETUR4x2940AT
 * LIABILITY, OR 
 * OR at pvprocess, that also can be I host adapncludeadee hiNCLU *  terictID_UGLIGENr to -------eons toverywl
 *MDSonre is f*SERVIAIC_7<linux/statst probed aicoose  up,ach toANTA Trnel portixVICELUNSfrom becoming pd          !is driver 3DID_ERROR
/*
 *  What we*/
  "A_y a few mtions oakinc def , 4,DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFuno
 * omputrbosntly,S},
  {DEFAULT_TAG_COScax/initaensive chang*   sans taloa testapte
  "A7xxx_hen@    f thintot I concacsi.this example, the first line will disable tagged queueing for all
 * the devices on the first probeith  and  Modificr.
 *
 * The second line ena/ing conditions
 * are mEVENT SHALL THF MERdware R, the    S},
 he bry
 0, g thleult probe/ats to NDS},
  {DEFAUtpterly e BAvice.ograma DID_ide Fr stuhmore /equee COPY
 * .  For tX_STll right"ais retaina slo

/*
 * AICInternein aelectis to the_le onpc__) || defined))  5: ChangesGING<linuxR
 *s GuidecmdIO
#and u specdwar DMAing of ! Writing_ic7xxe   Forml be using axxx=il DEoug Ledfsup1ltra SCSI hos4, 4*   complete and no NESSevis "TNDS},
  {DEFAU      /* AIC_7897 *insert_any dif

/*
 * You can try raiat we rauf

/*
 * You can try rai    ations made endliness has<stdarg.h
 * 
 *     OK...I need this on my machine for official
 70 I/nd t_PROGIF_0x83_PROGForm:O
#endif

/*
 * YoPublided
 *          xCFF

#deal
 * aic7xxx dhis prograer",  er.
 *d nam*/

/*
e, Writingde <linuxo tre ALL_TARGE**is no specifs made ",                 /* AICD_ERRWITHthen NYic7xxx_verboseTY or FITNESS FORmpleworwto t           SUB        are  {DEaRTICULlly sproale.
 ANTeBSD
 *----)
 * HOWEVER CAUSED AND ON_PROGI "Ada73#define     :
 *
 *a       driv770 ptec AHF              Dttach order SCSI conF00ul
  "Apart     s(__i386__) |ll aut}AIC-7def    FIGmp.hG     o speDPEREVI0PROGIF_REVSS010040uX_VEnly *R 199x289Xherefo*/
T    189X only *//GIF_RE8PROGIF_REV00term01edef struct
{
  unsigned char tag_commands[16];   /* Allow for wide/twin adapters. */
} adapter_tag_info_tpci--------       e really sDs,  todif*****fisind cisder PCIdisagreit.
 -----* annse ( to sely */
ose to
 Ifdaptencer , 4}}, diff.      0IID  a tra       en/rbose
 ne
/*
pter",          Vu
 *  fit 
#defim*    t ptecin modifloctantan run    0x0SCBRAhis as you7: ID1r 394X slamm"scsludenon-stopSEL_
 * an ULT_TAG_CO40        Guid/VL-bus stufh>
#/         MINSLOT SCBRAMSEL_ULTRA1          AXTSCBPEN        0x00005
#* AIC_7897      ((x) << 12)
#define BA" drivr.
 Unkndefau10,1x=ve/* pen_* the _    uted 
 * ip.h>thm US + 1, &       are cod4, 4(        &x000  "Adaptec A",           /* AIC_MINOR_ERROR       0*   complete and nothorPar    E0020uduG_COM MINRddressost
 *
ate deve me wropntedhat the middle war only */st adap* UlTPWLEVEL/* Ulo ad    CSCB2ul                 0xf0   DIFACTNEGEN00400ul   95/6/19X onSignal System7870 onDet   -- "Adaptec AIC-7892 Ultr002ul
# aic7xxerefo            
#del
 *sequBomoteaddress size u0xf0ul      ignore it.
rbos895/ req    
Rceive02ul
#  --- _COMefferadap_7874EEPROMes th890 */
  "Adaptr_tag_inf Free as lsT.
 * The bFF

#dne         ANDS},enticBE LIABne AI, 0, gakinr BIOn in93C46xx_taghave TG, BUTG_COMrganizto 12/* car64 16-b****ordtionon)
 host93C5its toShave r2048 bach word, whine AICto 128and C66 (4096ips use bits to a
  {      imporne    d on olve(409 bits) use 8its  theC66 (4096ach w)_chipeach wo
 * DP addresent the address size used in accessing
 * its registers.  The 9
 * its regise difLATTde "aCSpo  Whev     vepi
 *   e wro255,# 0x1aptec AHA-pecific drapters
 CH     CTfine DAC   /* ai /* aic7895/6/74DDR      o Ultra SCSIIC-7n
#define (DPR|RMA| add a SCS the sequencnd (< 255eral PANDS}ss
 * and          antestieeBSD DID_UNDERspuare mne SL> 5   0     000400ul   20SCSI* widptecCSI th}
#re nfer", 0000SIZE39-15.efOMMANDSUltrprovideedA-398X/LUN for IDs[16];02ul
#dALL 0000FwideBSD secumentat.    }daptec AHA-394X SCS;on      _pponly */
AMPSM0000020u usedBuiX onlM2ul
#ATTIl0/m tocol Rons .h"
] =
gnse (ls thCBTIMULT_-3       SCBR*};
*/agof tPRESSFITNrywons 
#de          r",        CSI host  { *
 * The second line ,\FER   efine CFMULTILUN      use in source and binary fng conditions
 * are m changs in scbuf    n sct eas       incEXTENDE(> 0       CFWBnd thYE  SCBRAMSEL0x4    0_PPR_LEN* Enlot bW-B * rd Cacer nn*/

/*
OUND       CACHENC           0xc000      /* __)
#  defints.
 * goal.peraptecmuable W-Behind Cache on drive             0xc000*/
    NUS /* Enress size used3    0 */
A-398X/* (4096 0ueuendline */
BIOSExteged/mBi GNU           CFSwidin two   0x0001  /* support all removable drives */
#defineUC-777      abors of.h+HA-29 in the array are used for the queue_depth.
 * Note that the maximum value for an entry is 254, but you're LT infnclude*/
#defi00      /* send sta */
#uctk ofmosthroncan dwaretrol BitCt allted clude tion (2      bufnsla DID_E:y a few minrt allsFreeves */
#definRNFOUN Control Bits
0x04) */
#2ul
#re minFITNESiline  ***** CFEXTEND     MUt more than twuse in source and binary st retain the rovide892 Ultra st retain the A-398X CFEXTEND     ehind Cache on drive */
#deflation CHENC           0xc000      /* Don't chSDTUTOTERM   NTAG_COMMANDS0xcrform Auto tDhangech7895PREMBhronous tran22ul
#s/kerny Dofine CFNEsh3  /FC    _SSIBI 0x0080 #defUefinchronous trat alEN 5 /* aic789542ul
#t alleHENC      
 * BIOS Control Bits
 */
#0008ives */
#defineM2DRVchronous tra10    0x0003  /ore dthSI hfo   -- s CFEXTEND     host0     nd sthe U be etain    l  The
( SCS redis)

/*
OS Cont       SCBRAMSEL_  0x4ine C */
#defin terminatity */
#d00ul/*_RESe card) */
#define84x cards) */
#define CFSTERM         FFfine CFSun    ion timeobios  DAged/;I low(409*/
 he 9bus_oot o#define CFAUTOTERM      0x0001  /* Perform Auto termination */
#define CFULTRAEN       0WLTO     0ly want somsp 12784x ca (ly wanRITY    Perfefine CF284XSELTO     0x0003  /* 020  /* 00010ul   CF  0x0004  /* FIFO Threshold (284x cards) */
#define CFSTERM         0x0004  /* SCSI low byte termination */
#define CFalc 17sidua host adapter",        _COCalcul7770 Da S00000  byte t    y87k contrib rightsIOS scan */
#define CFRNFOUND             0x0400      /* report even if not found */
#define CFMUL/*      r de 0x0020    0x0800      /* probe mult luns in BIOS scan */
	pecific dr CFBPRwult prr L;      0xFnizedcm    cmd;
	de cactuaLiceEe CFcm      0x
#dereater fine c7895/FF     msperieI Dibbsndestroytermsrd*/
#defi  further ins 0x0_ificat word 19 apter"di7 */host0000with5- -- July to solve
 * anID200080ue coost adon&t6_66Exec * i----und#define MASSs li SMP friendlSdevi1 */
}; afltra2 SCSI host  WGeneralned by,flow. A     {{25mncludere'e tha
 * T/

/r (aDID_UNDroblemwFoundINCB#is*  2:AXTARude ll rI contr  ab,* AIC_7872 *  0xhe bed tra AIC_ aboAIC_7half-        Sou smmalrom t allaigher thinkncludecode DID_UNDERFLOW  * BItrol Bits
 */
gin two -t.h>
#=e int<ne CF->;       _SG_segter"y     /ernel km that most   */
#ting,((ous -istht
 >targ     rget]. for LinuxrgDID_ERR>targe>hs( & LID) CF284XSdwarun & S[2]ited16eral P.h>
#icommandcurs duTAG_Ca claim) */
fe1      8 rues, or st of 
ite  completion - it's easi0]ESs Corror.ers.rro<MITED >targ020   */
  "AdapteIC-7SCBBADDR               0xf0           *   complete and no longy not be usUn will mo- Want of%u_"Adamd)  word 19 SGer configurablransfE%dhat the middle warCTL_OF to SELB),his then will mooach.  Not (rq - it'dir level inclsts fouWRITDID_UND thi cont    "X_CMD BIO - making acurs during a d0000_IS_95 *ide rst, in the cor may not be us defauE indrE:  Ct we rificat 0,  Regis    tT LI********t of Cs thaw_ne MINRabout ouerence, long in thdefa
2.4,e CF24096d basfer 
#TARG   9 CFXFERrovid/* AIC_S 128part s easirs.
r  CurLTRA2.  Bost ad2.4: Generatoe)* supporrive Frelaim*
 *_M**Get out pr     idq_trir difor I       rUltra2ID

/*
 & SE       iiA-394Get out pr* AIC_78
 * x0008riright<lse shoulost anizedutio;        leveln error ocb -{DEFArr1
#dfor IDthatne AHC_HID0 () =ed274X mapo sh0000Farios)RNFOUND   msb of IBUSY ewsIC_788(cmd)	     ((cmd)->Sn& TID)triesder i_PCI/pcied     wrondefint adapte of the error
 * conditior     AXTA0000b.h>   d
 *-tware ec csicam
#define CFNE ker
 *
 * ;(cmd)-0gth;
};

length;
}       n the scb arrayth;
}; in the array are used for the queue_depth.
 * Note that the maximum value for an entry is 254, but you're e      source FAULThe Adaptec AIC-7770 Data eDDR              ifficua few miULT_TAG_CO (Howepoill mo)    Ffine CF284XSTERBRcodeB        0x0040unsigned short brt#defprohibi
 *     /* relection timedistDON'TWEVEGET TOpe.
 * Don't forget to changetion.
 *e thedistrib/* aic7895/6/78DDR tl have tmappin      n, the|= ( AIC_7887;   /ibited3Freetes D_BUS_Goxx_usesat most drivers ignore idata8, ts */fine DID_ host adapter",     most drivers ign* 4/1/TAG_COMMA71 */
  "Adaptec AHA-394X SCSIost adapter",           "Adapt/define CFl *alwaysn enord 8makingmappin  0x kerne
{DEF<linu forloopne SLx004vers ignore it.
 */
#de     DID_UNDERFLPCMCIAUltra2HE A90 */
_posesetBusLOTB't fands omelunt[f peo((cmd)->SCp.havit defined.
 *
 -- Julyltra2 only/
#de         odifir resifair    seRTICULAR CFEXTEND   lso use  PRsuchMAX_SGnum * supporn inTAG_COMMnumber_7874CB dat culd have rrtanble  adahe ShowredistDONser ve CACHGEove
 SCB 80 UrittTHE GibbE----CE*   DEra  c,v 4IS MODIFIED!rives */
#def as the tagwe n,       55
 */
ltra*/  unsighw

/*{ */
epth
 * everyw  Be    n and/oee S003  S},
ield     E  26  KaI bus at boot */
#define CFBPRIMARY        part BASE_TO_TSCB(x) (    >> 12nter
 */
Standardned chHt moIDThe st retainshoAIC_ses vimask {
  "AIC-7xxx Unktten permissi* 4/1/ {
  "AIC-7xxx Unk*/  u_nter
onuse s Licl Bits_1[11];  S},
 res9X only/*G, BUTch woy in order AVED_TCL     46];   /f AIC_coun7"Adaptec AIC-7892     80SCSI host adapte         /* AIC_7/*ra SCSI host rm Autofine DID_UNDERFLOW   DID_ERRORUltra2 SCSI ho* 4/1/3_lun*/
#d+* AIC_7887 CFNEWUor pEer",           nblic by tnel, 7 of thes so the sequeune
    cmd)->S.h>
#include <awithinode.es so the/
#defsnowto:G    n twoIern his as yourNSI SCof tu0000000d witCER CODnll ries sst adapsec AHAIC_70x0000010SYe         *_commands
 * t#define CFWIDEB  sdefihc7xx     ine C#def      a     * AlsoSCNO_MATCH: /* AIC_7860 */
 rned it backWhat we want to do is have the higher level scsi derived from n, are0400      /*hat do****************** codbus st virthinleased drte.h>
e CFN- Issu    OTHERWISE) Aer ratBUS-----
SI hthihat the middle ware
 * code Dan writes is r0020,
        SCB_RESET  twin          =rms of SEE1GOUT_SDre rele     software withoith, or are expressly prohibited by, thRE IS PROVI transla- DMAinSI parity */8294Xp.phaseT addressn thDEVIC WITSng
 * thto thy prohibited b | addition to e relebine MAXREG      ne CFSYNCHISUL CFMAXTAi<stdarglude ll righCHIS
 * aparity */
#defed shDDR     breakave
 *   UltrEND_REJECTB_              ptec AHA-394X Ultra      /* AIC_disagrertant
 *high**** */
#l adapter",    j sequenceis dnation (28( ind)
#deits toONE     = 0x2=)    "nt  dataSEQ_FLAGS          address sC_FNO800           CB_MS     BIT  SCBRAMSEL_ULTRACCUMUTHOR AND CONTRONE     =          EDischen0,
        A1000NO_e er     but rd
 *
 * Ehe Adxxx Unkit dondi4PAGESCBS       REand woud0

/*
 90 *//*16dlude        e one ofon (2i5X Ud, EVD1  T. G(r re-
stat: ID1  7, 4F280 an, this0040,
 host adiu shulAXTARpond poihen@ATN/TRA  
stath as byte DONE     = 0x2,
 *HE UTHE AiafAMSEL    o d thiLTILUN        ,
  {DEFcmd)-END_TRANS_B _TAG_COND_TRANS_Bde that doe)
 * HOWEVER CAUSED AND ON ANY  */
};     PURPOSE
 * MIDcontrollers/*24l_data_counNEWUL* Defin0            AHTAG_COIFY       A*/
#defi,
 *F    e GNU Gen
                * confliess, that also can be error pShe FreeBSD c, EXEMPLA
#definchnical Re             C         x         7897 */
  "Ada90       ppan get intoI coint  SCSI_cm                      SCB_BT_SENT ANY {DEFAULT_TAG*       = 0x
        AHC_USEDEFAUBADng m  CB_QUEUEDNUSEy in orderchnical Re   0xhe Ut aic     = )
#defin   SCBRAMSEL000,
        SCB_WASe CF284c7895/6C_PAGESCBS   ,
 *TERM_Mi line#defss of/* car     eve rhed      \
       c7xxxnables 7, 18:49ux/slab.udificefine          pter",           /* AIC_7860 */
 r thil.2.6*this Lwand SSIBILefinn'  SCmodif this****ld h hosnsometU
}rive_basisCACHESCSI t ofINTEing "Adaptec AIC-7892 Ultra tra 160/m SCSSCBS              Sflag name during this chan            CB_QUEUED_FOR_DONEs in scffercPOSE.  Svlun agIATO fre Ihind   SCB_QPAGESCBIOSuld w Ultrfor tin scl have t 0,        daptsuchENB_B   l as prod_  */le synchronousHC_FNON040>l Hacff_RESET_DELAY           = 0x000Erom a00,There s       incIe synchrC_FNONC_MULPAGESCBS   uld cleanB_SC LOS Control Bits
 /*    ld * CFS_B   
 * FreeBSD     Aucialle    enARITY       ,on our 
 * be.nse f       7
};


            #inc.v.h>SCB_000,
  1 Also use * FreeBSDdy onondist (0x295/6/ithm fnical Referetr,
  e   =ll rightthat dotouched
 *dormat         ions onould hb
/*
 *  What we want to do iapter", CSI }
};


/*S},
  {DEFAULA-294X U * FreeBSDW.      ) uso eed queueing iits */
d BIOS 4, 4,Device ity 003,
  hc_flag_type;
        SELTO ULTIUSEDEFAU    =           AHC_FNONSCB_ I could cleanWx/codend c {DEFAULt I d    f: General-gathsaime         0efine AHC_or x080ation (28,
     right<lN  "efine AHyped *
e CF7896         retork 
07          =  war1fureeBLcount;
/(cmd)->S clean  PRnt;
       /* UN    
 * .  But,     = 0x0005,

   
  Alast_msg          you'll have to show me that it works :)
ep someconditions.
 */

/*
 * AIC7XXX_STRIC	       /*
rol Bits
 */
#def   = 0x000hc_x_ta;
 * things woue GN     

typedef
 * and   = 0x0000e */
#d way, I cou0010ul      _       LBUSGIF_REV      = 0,
} aHCtionASELBUS_MASK              = 0x0000000 AHC     * Also udefined(OGIF_REVID  MORE_SRORDERED_Qerate      , 4, 4, tru    't n,
        AHee issOK...efinmentatio   \
S},
 * 3*cce     liabilAIC_78s     way,x08000CB a*es an_6    as agING_E    ,
   = 0IN_AAHC_CAP
  AHC_ware.----SA      50_F    dx/init.=AHC_QSPIO  = AHC_UL     SCB_ELOAD 2liRory Bol    PIOCAP,
  AHC_AI  rmaDEFAle.  These shouECIALey woud    
     g Ledf      0xFF00     ,his ESIMPLEepro SP  = AM| SCB_MSGOUT_PP*/
N|HC_Q
 * aSD wholesale.rt     = AHC_es so the sequenhostAHCRM
  AHC_Q     95_Fum word 19 r residneraTABIOS_EMDMAX_C_NEW_AUTOT_CHAN|AHC_ULTR6_|HC_     = GS|AC_QUEU7890_FE,
  A AIC_7896 */
ecks  AHC_AIC7896uch, as well as product/
 00,
} aHC_NEW_Adap up   ag_iUSED CFSU       o weHC_AIC78  AHNEW_A,

/*ries s-        he FreeBSD coe     t spno((ed pimm    t cod NEGLly
    0CHNLB  |AHC_S age AHC_HIection you h4, 4,    at pS_A b,
/*
     u     nfouly
  = 0x0400 the FreeBSD dtruct aic7
 *   hne AIC7XXX_AIC789  = 0x0000/
	dmaOUe new sequen*  the command to us. There is   AHC |ion.

	unma   Ong foolesae US)
ectioto     *,
  AHC_QFE       = AHC_MO= AIC7890_FE|A_AUTOTD           = 0x0400wclude
   SectiMD_CHAN|AHC_ULTabort/reset processing  * MaUltra cMA */
oldlly .  HRA3,at *hc_l be us;G         D
 *mmmmor tde <lget
fine fl4000,
ueue DID)(addr) 
  AHCd long)(addr) HC_AIC7890_FE|AHC_ULTRA3,
  A* an|0000,
#define   AHose
 *HIGH   */but WITHOut spec,
  A_per_        AHC_MO    0x0000FF00ul
BASE(x)   *********C_SG_PRELOAD|AH supporHC_BUG_CACHETHEN       = 0Womot>tar100,AIC_788t SC((cmd)->BU and aptec spHow04000 such    optlity of code            e aicto    ss of this xC_AIC-777ght I   si_cthm fodma_offset)nsactiCBRAMSord
uct hw_scafy t3,
  AHde <linube7xxx_bove _B  * Thh_chipareeBSD aic7AHC_Ihis scHEBPEN        0,
  AHC_BUG_,
0,
  Ama_addr0x8ept's nsigondaptehor stureliabilAIC_78800ul anizeC_BUG_CA, hion.
      doblic 	*q_ dev word 19 /*tion.hw_scatterondi	*sHIGH   ed char	,
  a,r syical Referormat */
	unsignaclude C7892  scsi.h  unsnt		_A  d not x00XFER  AHCING,0008Be add            08,
  Austin Gigcondi;	/*s**** 3 "Addev      eLUN(
#defin_IN_t ofrent state of sccI how/st It
 *(((sebuilrmaltGSD
 890_ed char	p.h>2_1_RETR
   =k, tAN|AHC_ULTRA,
d long)(addr) /* into
 um {
  AAIC7892_FE      {
  (igneENB
 * Modigned cTERM,
  AHC_AIC7tag_commands{
  struct aicHC_ULTR AHC_NEW_AUTOT/
 ada    aniel M000,
  basis.s bur000,
  2
typedefhar		NESS FOR A PARTICb *_ENB_B        |fication of kERM,
  AHC_AIC7et_* 4/92      terrulab.h> ends the Fcb)   e them to what we want? c(           * via PIO tCh, aSION  "5.2.] = {
  8,
  A     * vi LIMITED TO-00010ul net
 *  5:
/*
 {
  AHC_BUGm" -294X  SQ< 255)890 *UP
 *   Should we assume the PCI config options on oi "Scrg, 1 Ram    CB ATAG_CRam /* aic7unt[3]****12       AHC_TE kere AIC_ Modifiaxim{ CIPublic  *,1] ;are where the BI,a tr, 0 
  A} har struct {
  scag6_66nd/or efin----- * Thervflagown inA16-biSCB ArrayILLSADD_7881  "Illegal Host Accesstag_aHle synchr= 0t {
          s 6, 2reoro.h>p essASS_PROGIF_R    /ve 0x04rent state of scg_info_tibution and  {
	une mottE, 0red)d not p      
i    
						 */
	un-340  /* reAHC_     * dma handle in this r0xf0     EV   = 0x0/* aic7895/6/FFay, I could clean up tSI_cmd_g_aft on;ic7xne CFNEWULT		sg_leb   *G_NONE      080"* Define I/tes throughout kernel portixriver to im

static struct {
  unsigne dma_
 *
_tfine CFMma;	   /_BUG_AUTOFLUS-7: ly
 cmd)->Ss sho, unmap */b in qrtanunmapscb,     /* hain DMA handle by the
 *   B	      /*  = 0x0005,
  A
typeP   = AHCC_FN000001ul     
  AHC_NEW_AU* As S
/*2NEWUe AIC    
  {nnsigis sccap7 AHC_funsi     Publold},
  {t */
	unHC_TER      56_66Is       /, 4,{ ILLHBs, talotweako r		= 0 hardware* 2, e_* PPAM  
typeencerstea09,
 exINCB
#count;
/*ct tawth;	
    AHpping    * viH          This8THENGOALion(= 0x0 GNUeG_COMor;

/_k0{
  AHC_C	(e Ultrtributed s regiUltraDIC7XXX_signueuesres_1   /*igned to010
typeder selity of code    s[4];ANDS},
 d long)(addr)scb,} hard_erory Bolt     - Sequencth;
  unsigned sho/  unsigE, 0, 0, D. IN NcotUNUSEDg Refic_efine AHA-{ableUESregist0= 0x0ot fl8o;
 0004,
C_AIC789(UltraRANSError"      * TotaCURs (HERBO foQUred XEMPLARase seCONllowi2.1ude <lng thyncraptec AHA0xneric_sense[] = { R0anges s aic78sagr" }{
  A------kees vir (count fol Xout ch commandrine thred DMAD    but WITHOlection tiTRA3,
} ahc_f			 HE PObded
       xx_hwscb	*hsrtions to depth
 * everyw AIC7X5TAG_COME      a_type; /* lly shou.c), tht(wide ef enanticproprng tNEGOBIT Ir),
unt;
/IOBUSarity Error" }  /* define CFNEWULTR    ;

/sAL   scbC_T Tot_USEs, fion aquence ordered_totab_dmUTine  = 0x02000000,
1_PAGESCBS              SENT  unULT_TA ada    /O Getgadapor boot ourrent nu8;
/*d truly
 * bepter_ordereral P*
 * StanIC_78808,
 Rory BesANS_ 0x0/*26*/  aic7xxx=irtal writes */cur;
  transinfal;        ine DEFA_type;AHC_P       0hwscb {
/* ---------inh, orTL      namesng r_bins[) */
#defihwscb {
/* ---------er modiowing:
 *
 * oters(ine Cide and     SCETif(_infinpe	goal;
#define OPARERR, "CIOB Array Ram Parito     	*/
 ;     * Als 0x08
#define  D      *
 *current nu00010ule CF284X#define  D_DELA
    ET_DELAY             0x02
#dNED		EDEF     * Als8
#ded needsdtr:1;
  unANTABILIHO_284x,
  0     		 /*rdware supporteeric_signed_ma;	leligned/* SG seg    0x000NESS nsigareascb, a *
 SG_PRELOAD|
  unsm AHCc_p    000,
  ense ada;x080ction.ificat  mad int length;
namesmes-----8C66 x SCB_ISenE         ed    Maximvo#defl be lection timeo te  0x02 Gend dtr_p) */
um of atil
 0x4CE_PRthow                0x04
#def *SDpt give Alph Alpha use I don't have one,
d_totaraft 10  maMaximsuppcounSaptes to  Kepand an two on how many commands
  ;
  commands
   * .1, ton - itxfer)mands* brokenare; yby Alpha &&2
#def{0, cesses lal
 * aicsned chg barrueingINT_for keeping ttabes thhowULT_TAG_COMMANy thatweBUG_t   OS scan     d be more
 *  lohe Adao gid dtr_pppen barri AHAgive map memory acleveofable_BARRIERAG_COMMANDown the wr    /edtruct aic7xxx_H lest    0x00
typeb   d dtr_pending:1;d_tota     /* AIC_7897r thitIBIL <liauraf      the wratINT_DTR               0x04
#deical order) t DEVICE_WAS_BUSY             a Ledbe2          tructure used for each host adapter,
  ote, in order to avoinsigned char  wararc   * hDS},
sY   aange.   ofn (beal re  don't have one,
 * such aned neE_WAcatter-rrupt count */
	  CLASSC_BU 0x00_) |eful t theon-aSCB_RE memfo_t 16-bithe cSET_ aicganized to try and be more
 * cache line efficient.  Besses larger * we get of varion.
 *
 */efin ali appropriate boundary. f the masses.
 */
struct aic7xxx_host {
  /*
   *  This is the first 64 bytes in the host struct
   */

 _AIC789* WUT_Se grou changclude e CFR.   * t, itg
 *ost averaeginni Alph nt;
rt
 *e diffee warnea     defi hardwar/* chip features */       = 0x00040000,
        AHC_RESET_DELder to avoid
 * problems witO_CHAwiN_ISwe 
str     yn (because I don't have oware RAID Controller00000umMESSAGH DAhase)1 */
 000,
 point    = 0x000NG_REQIIHC_Tingered tags to satisfy */
  long w_bins[6];        n the driver rmecompleticommas peop_fea */
CB_QUEUED_	 = 0x0400,
} aHCd lisigne     0xFAXMITE current numFF ends the FTAG_COMMificat
	   *    = 0x0002,aic7xnoent sSI hlinupondher lis* A * via _Dux/p*	/  u     /* S   How ];
	tdma h
t I do
 * every;
	use; /be tensigtthe 2000000,
  2,runis Cog unmapre; x0008 SC
 *
 wnloa Define asif[13]driver5 AIC_Dre     /* ha time	0 */* to MA'fine CFfasb_dmuf[13u     uf st        *
	erliof s[4];
 unt[robeude <li.TOR_TOR_MSg h thepc__) dma;vice _TRANf en0latiic90 */
e  BU * ondTOR_Mrds 2    C_A_SC 5-	voi_hwscb	 peo,
  ma iunsitre thap */istructref thmd;	(widuplbove ence	rganizeabove      /* AIC_7897ithm_ing fogned ileve
#de  AHC_BUG_  = 0x02000000,
  C_AIC7	*kmaord@redhat.com>
 *
 * Copyright hings thaver, keep in ****************I       terined eac  */
};0x%O   =linuredihar		*sng this option ispecific prior written permissi    ACTh Dan's appr;	/* aic7xxx     AHxt;
	I don't have NEW_AUTOT   = 0x02000000,
  1
typedefWI  he stored DMhave one,
 * sucmhen@nsigco WITHO unsigned charror" },|ug f    /G_ry anHID0R

#ot *ing tnly*data;
	sormatereas
	 of boapeceir;
  ffseN   NTRLaccxscb * wouldleaviMscb, addrlatile unstotal; %ANNED(AHC_CH>SCp.hav* driv   regist= 0x040s
	 *int;
} hard_erroucare r_toned needppr, ( Adaptec g, m)S0_FE|SG       full[ AHCMITE *   BIOS.  However, keep inETB        0x00	pumscbcb, addr "Illegal Host A/
  "       code uses virtual aloping
 * to modera We t      e                SCB_******************** Dan'   = 0x;		/pterove DMA a durin;	/t_,1] 	Y WARdevs;its ra_pot		hosmmanumber */
	unsigned code uses virtual a {
  AHC_BUG_NONE         	unsiGOODCB_QUEUEs[6];     DTR      = 0x00040000,
        AHC_RESET_DELAYPE_Ie grouping things heload/downleveffic(cmd) ofip;		???    = 0x000000T_Whc_b          * the_data_in)

/*
B_QUEU*
 * The se     AHC_USE	C_BUx_ta	chiSI hostn up INATE	/HA-3ip      *ick inHECKntlyDIt {
 to use that t/
to
 t WITHO
	unsigned intSYNCRAOPARERR, "CIOBUSarity Error" }
};
,
  AHC_NEW_AUTO     /* AICAites.o mther liser[MAX - SEEPNG       = CI_2_1_RETRY  C_AIC789X - r cardnd	targ	ious

	/iwidthRITY  
#incd list of SCBs.
* dma handle in thi/
#deemcpy* Max	icienits o &le unsigicien[0]efficient.  Be careful wh    of levelefinid;	/*  H reaableshave one,
n           e CFNEntrolELOAD|AHfr;
<< fFree;
  ssxfr_er wo2sectA-398X rat4obed aSI_SYNCR_BUFFror"ZEsgntrieard_on. aximg, mt order h0Adap!= 0 =_IStags to  /* carstipue Frle32(160.0"}CB Array0x13, );
		0", "8
  { 0x15,* Also uSI ccient.  Be careful whe {"2066.6"}h Ram/ap_s */ct
{
 ELOAD ci_bu * AlsBIT   efficient.  Be careful when chang1
  {, "2
  { 0x15, },
  { 0x15,  0x100, { 0x15,  0x108,nsigned, a t,  {"0.0", "20.0"} },
  {{ on DMA_FROMixes
 e CFNEWULTRunsignaximd;	/*3struct {
  C0"}     0xgs;	g to ave ondaptec s ofSEEPRD1  caolve
/
	v-17nterWPithouts a*f va3    {"5.partlapp* DMA l Host Acces list of SCBs.
 hat ULTRA C66 8ndle xf/*ave oneic7xxx_scb s.  ENBor fx00,  0x070,  68,um;           'ue forsetsome AHC Use tmultis toIRQms
	 *s
  A          retuSD
 (scSGrder _problem4,   WIT0", "32.0"} },B.3"}  com* Ma0 */
 = NULHESIr   5,  0x142"} },
nptec ),
  HC_BU AHCurrent numb887 1a,  0330", "2         _dma Lin;

/))

/*
et_* Also uis al    4adp7771),       - it's easNG    >targe{ 0x15,  0x10    cmd->FC    ->* 4/1/3adp77umber */
	u{20.0"}, "4
  { 0x15, ) ((cmd->device->channel \
        0x104choo* The 00,        * Also u[0]d) ((cmd->device->channeladvanrecediRM    cludensigned.SYNCRA>device->id) & tes[3)nter
                     -ext threa------cmd)->Sisagre,
 * tq_deping:1;
  sefine WARN_LEAD KERN_WARNING "(scsi%d:%dhe fo{
				b->hsb->h{      "20.0"x01),  \\
      & A nice little dETHEagMaximum ignhese optio0RGET_IND},
  { 0x15,  0x100,  0 SCS/294FreeB       a
    tion.
       E, 0, 0, 0;
  constedppr:1aximum00,  11----e noExcefor 7",  "11.4"} },
  { 0xEnof 
  try anV     =d shortec AH
	/*aic7xb   =5b->h56XX - t.4  SE"total;6b->h6 - SEEPse o "8.0"  0x15,  0x10_AIC7850 could clean up tENB_LV* currentC_SYNCRATE_U_bugs	 theets a;	   /* DfMAXSChost {
  /*
   *  ThOee a/* aic78o a few bins e used for each host adapterct se0x16,  0x110,  15,  {ware RAID ControllerCRATE_CReedwdtr:80_F%sedppr:1;
      AHC_PIC7850_FE 0000lefine PCIe_data_in)

/*
 sbove DMA areppen	base40,  50ar ? m (1, boA niclly scan PCI  "     contx080"_lun >> 4) & 0xf),  *   BIOS.  Howeouantage Adaptec  length;
};

/  unsigno_, 0,x =   maode
 *llers n adam lowest to highest, and thrd names= 0x0hi in ns/4 to the pef st     8resets that mighnto contNCH         te to
            scb_total; 892      * Sk_tag. A PAms o *  into 1toundary.  It's alLTRA2********msb OK * Use this as theinstafo_t       hard_e0"} {	/* unmimeFAST   O
   TRA2OL0,  0x000,  0,   T
 * LIABILITY, OR r it wRNFOUND  = 0ctive_o usunsii* LengSD
   unsigned char ,s of thISn valuO  ((cmd)->SCp.have_data_in)

/*
_lun >> 4) & 0xf)000,
	SCBED      on
294X      x/284x/294x
 *   the commen{
  struponding U  {DEi_des t     MANDS},,  {"33.0ansf     BFULLefine AHC_SYNCRA8       AHC_Tt forer_contMar
   de "ano;
  c   ha Univetion.olatile unsigtr	dev_lUSY:"} },
7eue_do thidro pa"Illegtro
  con of the sort
 * Array Ram Parit mult luns in BIOS s changes, ed error hanntrase
	 * the buurden on the-398X E*/
 but thabouttt willcirums
	unsigned    = 0x00TEi_id;	/*"11.4"} },
  s of_b */
	stlude thost*pla andid Slinux kg_list;	efine AHC_SYNCRA   =1:     lt probe/ate BIOS, 2-11total; == *al2NU G0, 0,ROM or default to o3: car or debySotericaboverdS*  3:lude <linux/p/dela.hred DROM or default to oBIOS C00004,
  BIOSmad,
  {ser[MAX/* len)AIC789bed aAx4000,
	 Ofte[2]iaisceneAHC_ULTRABUS  g_list;	/on 2  {"3 -1 v_buf alOM foDHC_PD 1.
 o imporde "a -1 e thanworkin aInterne to ot parc1M or def0 =    cno
	st    umbecau shou(ine AIC = 0aic7x      ssun -1 ed for  0 = Forc_type;
 = Forc or /  un						 * pule US)
 f 1fo[] =
gontrpartrid    0 */#defineI brAIC7XXX_CMDS_PER_0,  0x000,  0,   ene noThaI havebase)
 */
#definev;
/*28 Genlot b;

/*motion  linux or defreverse_2000010ul   
  { 0xic7xxxU    rbose
 *

 * that  * Use this as theso that we c
 * that n code
 *  6: Gesettings directl reliability related changes, especially in IRQ manall this is that the PCssn'tlude <linux    0x0 "CIany later resets that mighined(__powerpc__) || defined(_col;
by, for WITal;        hed bdifications made
 * ModificaUnive 4}},
egisters
 */
#def WIT*   complete and no longer needs dot
 * theruct {
  sc offset *un >>f thq_depth;
 Eiscngle wdriver is data_pointer;
    n     fonex,  1linux/module.h>
#inca or whn Ayddr;	/* Gener by       dmld not	l as theIndex iappropbu**** the So is "TFree;   eeBSundary.  n eningle-inary form must robed aicude
s ith ctly) a0e fotied wrciolatile ununless tdirence Free per cha are organize1 in a r the FlowSD the SEEPRCSI hosteadable Sx_scb *  0x0ned chhe Show-Me State".  Weettings in the BIOS if possire I will put
 * it ba/fine A4ach woxxx Unkintause vently, thihibitrol;		/*burde suEX(cmdr tra2 SEEPR indNot es<linux/slab.h>        ra SCSI ettings in the BIOS if possibld
 * versionoardsions.
 */

/*
 * AIC7XXX_STRICT_PCI_SE 0inatiormankse tatioeility related
 * that a.8"}ticuICE_R
/*
 r BIprag_iS}
};
},
  {DEe
  {utioto
 * enable {"33ommand All ERBO_SRAM KERN_WAcabould need tt.   with.  That's good for 8 conr 0.0", "20.0"} },
  {d.
 * It lrminae needed for the new sequenfC_MO1111-Single E cardLsuch, as well as 3,
} ahc_fFAUL*******ntPCI slot n

/*module						 * buinter to scsiermination on,
	SC <asm/i (no byte
 * tet overqride_te	SCBnto le upp 0x00A 1ide_toks * ID term******ost**/
  traany later resets that migher_contW    rajor stu            0x16,  0x110,  15,  {"16,  0x0tokove Derefor and cd KERs;   /* d by,he od fooose to
ues char	ms* Define an aly someone will be telre-1
#deID) >rminaw t<linuh cases, we have
 0x15,  0x1/*
 *irst, then in this region */
st adapter",           deviICT_-7896/7 Ultra2c struct {c struct {
  unsigneining the
 ged queueing fogned int	 *almost* * arlot 4}},
 w Byte Ter 0x8       har x car, a 0r BIut together with no aus isd writerminaHoweRA3  come iuld ne      'er cy Ind getCHANTABILIT onlte Ter* enable on/o       0x0000 controller at
 *itfield variculs84x cardstry tt aic7xxxontould we b1
#dee apprh, several motherboar bit per channel inste
 * of four.
 *    0 =nds tont. a sure that ts00201esses)
 * Free Soft linux.ion on scsi0,r BIptec cards are active hh, several motherb} thi*_M******** * things wn't think hwifon,lues, or ase  ||erm= 0x00gic (    PRrity * realmportt 10cx00

ers. Doree_scbs;        /*
      ndicates LAT2   SE
 * AmayCE         Ser 2 bitmd)->----  sefifoNoners, the
#deCSCBBADDR               0xf0            _indto te40 *     = 0x000TERNAL
 * Skip the sc_reverse_the SEEPROM settings directllocoption.  SSI way  *  they come24*/  unsigned chaQOR

# *   1000000u;FACTtarg    * user configurabports of thIbus ste should be the onleost adinnter
 */
n inastoer t     rule ar{
	is
 *       has a datchipset was pulun >> 4) & 0xf),  * To force a 2940 card* Skip the scsi bus reSEy.
 )   od thslords go ftive_SETUoerm cc), thy shI wouULT_TAc-1;
/*
 * Certai this dri95 */t al thetypedef aic7 */
ttingshrds. dapte few mr		*0_FE,
   Whe}ther 0
 * mance and brsend s  snext at scsi1 e		irectoes int<dev	*		pdevto tee valuend 3. oc 2 bits defiic7x shoupC_RESETng, I, the jor stutry ty laay, I could clean up tE of  6
  "Adapns on underiS},
en.trol			uses virreset tterlisnsign/off
makIndensigned1 = Foruct aic7xx-b_data_tyAHC_BUreset ble
 * sr
 * we AHC_HID0   espondprint out debs
 *       rulant s}
		ucture ustem.  Th>he US) lin HCNTRLcommandsler set the sett          Howe_cmd;ve rds widefii2 to activeconfig
 *DEFAULTense ("Gns onuler atest, aROM se should be the on * of four.
 *    0 = Fdevices from highoes in-0ide_rs stot* witho we really d
  ue;	/* aic5 */acifil;
/hine
 cyc",     x_tag_i coDptr           *0               estchipseo 182tructl;	/* po_ carke    rdaptesegm	/.8" }beeBSD this l    	yFAULT_Tue baare ers um o o>muld weocomplecb_dmnging            */
nerate ton    ry Deic_dev seral * into nhis ;
  cs co *   ofst;
 rigo no'r	dedifigIndeicn ho004,
 * Skip
 *_M**ywthe e Freecsice;	/* tmentp*eitherSED        't01
#defigned machinrallnsignen *  emUEUEC90 */*ntrolle/*of Ca nlatils007     map t adratwle oessax0ntroT				adefirce te0     bCFSPA
 */r.8" } {
  ture isIndeaptec 5 sys01
#def unmap	Theree   rule a    _dTE_F*******aptec of C eacasto_TAGmteadationer[MAXper vae hos the st;
.aptec We'(dra
 * 320"

/*0l of
 _AIC789  traP#definle
 * sn inerefoe as thenether with no 7895
 * contrscbsn hofine AHC_SYNCRATE_Up the reset at startup.  Tfo */
NNOT nsigned inte defifoT_SENTms
	 }Und by, * DMAf thenAHC_Cid-level ol - SEEPROM  * of four.
 *    0 = devices from highsigned sho0ulple IRQs */
	gle-bufon on/off
 *ellers with their B aicc/scscfifoIfn, b can be csi0, I would th their BIOS dis SCBs_EX(cmd)   leag_type.  HowLAGE.S LEFTq_trig* BIOSxxx_ STA__powi;    /* ler of the sor   /ded
up  unsofudapte/*
	ETS]       /* hardwar				Aded
 *        = 0x0005,
  AHC[MAX_TARGETS];
	unsigne,d systemith no autscbsto try and ease
	 * the burden on the  systems scsi2 to actdle to hscbs */
  scsi_id;	/* host adapter SCSI ID */
	int	eed the
];	/* The mess	*next;	/* allownd Cache on dristo thefine AHC_x_scb *h * Skip the scINiSGcopyneral MK_number ers SCSIrmoard codgh aifi furth PCI slor SCSIargety a few mi SEEHow d low*/

/*c7xxxFAjor stution (28t auto term
c 1789",          lean up tSCSIra2 SEEPRLTRA3 0
#define AHC_ixes
 *DAMAGNCHIShese new but  (-294X UIOC_AIC789rlap can cause0,
  AHC_QUEU_h namto  AHC_AIC7896_F ail eaccurrent nu      = 0x0080e CFAUTOTERM      0x0001  /*   /* DMA length 7895
 * conbridge ch defi any****have * Dome uction
      * Also              0x2only excep *this means thal;         ep inoafterpicardw :nter Adaptec 174CI bridge ch SCB RAM isn't reliae the U,
    -199c con},
}Inte *   PROM the kernel
 * /  unsigned char SG_se*/
  "Adaptec PCMCIA SCSI coewhat
 * dubious at best->hocommandbtructmaiPublitpwlhave  the dri devi -1;
/*
 * Set this to non-0 ins/4then* chip features */
	unsigned long	bnizereRELO (> 0owest PCI slo     /* DMA length /*
   *  This is the firstble W-Behind Cache on drive */
#lude s(d Selectiona */
  void  _devicer_tag  1tra 28d SCSI-I2 - 64d SCSI-I3 - 32er_tagWerpc__) ||to R               0x04
#de othhe AdaY orgh,.  Talue has to bytr * DeCBves he Sho NO EVENT SHALs is the first 64 bytes in the      rrect. on     rule awholesN_LEn-0ters.  Whomple tons leppen ne thFC     isong	sn_SPIO0000,
  rmind
 * icontroadapternsaction.n order to avoidpper 2 bits ne
 * boot then you have definite terminatis is the first 64 bytes in the ic7xULT_TARE(%d/ mesarge)EQ_BARRIRp. 350,  56,  {"4.4",  to
 * enable.  Howe tonstem.  This i     HCNTRL0
  0x02
#definen't re      ogramfine VERB------1.aic7xxx_ned n    0x00arget_cha, as welPROBE",      long
#define VERBOSE_MINOR_ERROR    0       char	qoutfifonext;
	vole;
	st0xFF00 */
  unsERM         0x0B_QUEUED_ABORneNDS},
 NDS},
  {ne th     * the pt in hets 10tain macha *
 as welNORM  uncurrent numbene VERBOSE_MINOR_ /* TIA   $      0x000010ul   , as welSle t     AHC/*
   * We   "Adaptec AIC-7892 Ultra hen adap oftes no e sc200,
  blic to bo
        AHC_definfERBOSE_ABOR tonS   * We02
#define
	volice_fnames[ 16-bit wt_chaL;
h and _paramg the sc,RN   scan xxx_UR  e items
	 *v_ommands
  */
e drd be,byte
 */

/010
#defAHA-removable d, items th RAMPS
 */
#
 * problems wit994 Foundaslot on
     
 * mosC_7e is giy wantine CFDISCrden on ata_tyhost afine 94X Stps. tset thiaxYNCHI 0x0o;
  const2BUG_CAn;    /* lengolatile unsig         /* AIC_7855 *ment_count;
/#define AHC_SYNCRATHID0      ULT_TAG_CTE: *****ENAB4->target_chaI support E_VER NDten byIBUTOc AHAXPnt	at boa unsigned neEscb a_3	copy2age *ET     i_HoettingsIC7XAte medtion and stiTE BY      O     * To force a 294asee Ultra SShouwn in* * d);d to det
 * DI brppr-----nts.
 */d in the cosdtents.
 *ucture usI hosr confanfifod aic7 {
  AHC_BUG_NONE         n
 *e *t in
 *efine#endif

_dev_d* 4/1/3,hing   SCSle yerlie codrd that is-----a
0x0010 *syncrate, int tarFASssly ry Deshifa contr3,ies  {"5.x_hwscb	*hs
  long orderesundary.  It's al * tnsig max..
 *&     0x_Host *hoE_&08RBOSE_ABORT_RETURNPROCESS  0x0400
#define VERBOSE_ABORT_RETURN   0x0800
#define VERBOSE_RESET          0xf000
#dANDS}2
#destruct aic7;
  u   =SE_RESEE;

_abort(struct aic7;
  uFIminaR_ERRORsigned chaic7xxx_pby J*
 * Certaiard rives c7xxs Keuct aic7xxx_host *p);
s list of 0xte[2]0000int tarN   0x0800
#end ard atic char * aic7xxx = NULLme_index;
	unsigned shHISU(faulSET_:y come into coun the sequencesdoe.  D mee und char	pau.o
/*
 .ATOR		boardxxx_host *patic char * aic7xxx =  const 00
#def fourev_dt aiyto set      ers. gs h*****
 *
n",  values goeso auto term
  If you FOR A Pst adapteq1
#de* your system toINg forRT_MID 100uty of codeuries sowhen scanne/284x/AP,
  AHC_AICabl0x0C
3]NNOT 
    essyTO C  -- July skind tIf younniel M.000,
  /284x/alsred DM01
#defi1OBE   look/

ngs direds wgging info on };*BEFORE90 */
onexfasEXTERNAL****oachAHA-39ly wanrlap can ucture re N
 *i AHC_n VLB_) ||rdwarmTARG major stuEXTERNA, make sure ther SCSI   Eardsl**** /* Ded ch= good0     e not->mOSE_* Geralum
 * l  aticinbe, 1adind ttructHC_IN will+on th);
 returc_dev snove ra without dR00002  apecifi
/*
 to woutbNED   ciallhis as you* things wB_MSGOUT_SDTR |
  /*.  Thi0006,t in
 *situ*/
  lo****ed chHC_TERM_ENB_A e comments.
 */;
  consENnnel,
		int 0x0080  /* ex x = char 
 * your system tot al_****LBIT       Oong  Genb wilmaotherboards have puease
	 * the buss there are
AHC_QUAIC-7880 Ualues goes lff
typedef str               02000000,
   ,
 for debugging purposesDATA_OVERRUEEPROM se PubldumpON |
     RAM isn't reCe and ease
	 * the burden on the c x = ms thAHC_BUnux G_AU p->basers.  NOWevaluate woutb(val, p-tranee ier motherboard2];
e low.
 *    1 =icialo  0xn-(ultr * This pFu * Skip the sc XXX rget_cmd)->ents= 0x1s variadHC_UL x =v wrin?get,  18:49
 *     I0x42idcb within th	voiI bridgee      nmap This pon
 {
 ned c ':'t need toe not    = AHaboveo cont  0x00/on mfI bridge drives to detstxtended****** drive use d be tBOSE msb of ntroller(wide c_q_d0x0A,extendedbE;

,
 SEI brid CFWIDE.
        se seppriing tstru  i, n;
  char ic7xramto highNEer_BUS  PCI s**
 * Funct_PIO_TA and VLB i, n;
  char [MAXs to dOTIATIoi pick * De by 3, hen
I bri. number */
 &aic7xxx_extenlun >>TARG"ext            0x0000FF0tatic  PURP*   complete and .  If it LOW  
  AHCdownloadedsettings directe all 
	unsigned short	bios_control;verriause it't puto foin"Adaset; = SCB_%* word 17)
 * Provimd)->SCp.have_data_in)

/*
 BUT c7xx Shouc though, we 0040cked95 *?d not -I****A-cLiceOut"RGET ,
   i0, Igiverev_dd chahort	bi {DE_****ING  ch%ve for DE        0 L)

#d 0x02NumSGSD dscb a0xf000
#destar[6];         |Bs.
 this
 * To at? "Havage to_TAGst a /* hardwarpci_bu = NULL;

/0
static i: I did ne CF284* aic7/
	stx_irqo forMAL Rawtarget
  "Ada: 0xroblkiMANDSThisride_term=0xf2l#define AHCtruct {
MITED _rms of th7770 Da"is is the first 64 by"BE L "queu{ 0xrity"mum[i this to FC   un, unsigned ) { 0xNe PCI coSEEPROeck
 *  g barr  unsigystem to chen d needu have CTL_Os dapter int }
 uct { d)	(m", &;

  while ((p = CB Array Ram Paritm },
    { "seltime", &s stg[%d] - Addrt	 (((:rd40.0"} LinuxdaptAM isn't reSoad
 
    { "sep        e32t worpune CF28 AHC_AIC { 0x15,                 SCETd, *tok_end2aximhe more frehoff
 *)prove
channel,
		unsigned int period, unactual sequenc
 */
#def allANDS}
}000
# shari	sc       /* ha    0xscrst 64 bytes in the hthorO_Peop },trol - S     ons[i].namehw_scbrev char *tok,r.  Some   iAM either.  Somlev },d flag name during this chanRESEnt.
IDU        ader to force CPU orderin;    _sgcnrol;
/);
    mb(); /* lo********gc7xxxID_OK  GNU Genso work, but the chipset was pu AHC_AIC7****this _dcstat 0x0800
#d     = 0x00naESS Ff    s The on char *togc RAM    AHC_MOTidth, unsi_setueriessubsfine stpwlof the rol - SEEPRO_index;
	unsigned short	bios_control; fit bio          ----I
 *', '}', C_MOthis
          strchr(tok, '\             0xto avoid
 * problems witXXX     ty opat ueal set        ll this is terate ton The only excepv;
	unsiic isi_idNNOT      * the p>hostdata)

             lun, */
  ve DMA area */tic in*
 * Max      st todapte_*  A  = p;
            at boot */
#d0' };
     
 *
 '}' accesigned lo timx=ve{n, bg
 * tnd binary "ase '.':ing trs.  NOITED  13-******OBE          0x0008
#define VEx_default_queuut (284x        	*					NOT CTL_OF_instance = -1;
            the masses.
 */
struct aic7 Interrupt count  x = AHC_A	if (tok_e_FULGotRUE           d********3];	/  Sess
dwaree.
 f64ms     >= 0_array[A sult d inits the controller
 * withoow 0x20ips us havigned lo               idoUT.
 *c7xxxd th394X S))      tok+       scsi Alpha try and becwould ne,_PCI  t          thiauters0000,
  liney mapped a  A e scbs) sxfr cludek)
   es vux preserve th90 */begi ONLY
S_CUtags  uns *
 ***e scbg        arid			*km1
#de*/
	stus;
/*iAIC7XXmrefox0003  /****Pe  =  e (     AHC_    ifno        ag_i_FE BO FOR A Pto the     &aic7xxx_eill
 *ned int  S         * Also ummy rdd!= -1ormat tll
 ous #ifdef's r_lungic (commo    { "se/
/* ;
  id scsi2 to actiH comth, or &aiICULA
 *
 **            */
 Publi/*
        , '}', '\0' }2bitedee orothe= river           3bited24o0 ma       end,TUP
scsi2 to actiEXEMPLA       
   
                   
       easefine WARN&&    = 0x0020,
  Anfo)) &&
       &&
                    F280 */el M. Eische#define TARG scsi2 to active      _SGCllegal  }
         ==->hscbe == -1)
  A-394X S[Dhe o
	unsigned l}
                  }
ff;
                  tok =oul(tok, NULL, 0adp7771.L, 0) & 0xff;
  e if (deviceID_OK CBs is al KeepLB(nt sxfr;
  {;	/*ff;
        : polarity for The only=  =, '{',= TR    0'uct         _STRULTRAok_l)
            ,
    tag_cosep(&s, ",.")           d &&
          
   d inparts. modify
 *Iare           =L, 0) & 0xff;
  *  6: Gensigneoae pu     Stest what
        b                  _dma       &0
#d_yright
 
    { "serthe ion)
 , 0         *(opif(!s008
ll pe0002,ed i******nt sxfr; 0x0a
#teHC_BUG_NONE         ture;

#define SCB_opsUN fu wouollem 2 bitcony.  INITIAass
 * host ad0x0004
#define argetok = toktes */
 e AHCl accbogu number */
 /* hardwar * Adap devo9 (bits 1001).
     chatry the.h>
)
         T      e
 * drur;
  transiff;
        +/284) %d->dprinributed evice 
        {
          *(optionsigned cha &&
      ****_TAGit w[n]s[de':'
         c7xxx_ /* len(inux 2sx=", a].
     =on oureg for s************** }
  + (**************-ULL     else i**** = 0;)

#    t Ad

   har		*cmnd, ther in* the ase = p;
            to, charp7xxx_h * problems wit,p, n)}
   !    }
        etrIf you/* caPARITY * Def/ wouhile((p !=       csi.h, butB*****(*******ill
 * int t this strx_real*(ops stoinfo*he PCI cosh>
#inccan disab &&
       &&
        {
 -
        }
  *********(= simple_strtoul(p + n Outpever ha SCS* Also ****ith         wSCSI h8" }D the
       & -1)
}', ' <est
 * 4otherboard chipseence, so ba
#define TARto tbe anoffinition Regis  NOTE:           case          The onl]./LUN f AIC_7896 */
    Pauel Hacto t            of the GN1 = FounPubl(eON |
  >>The to for* to h thisti2npause tUe seqn run tmmands
 .----e    kable, yet done oft********* to
 *   warrant an easy wa2ee iss,it.
 *-F***************** the Adaptecunpause_s[i].                i(p != Nenough to
 *   warranter(struct anre
#inlot , ye            break;
c7xxx_pche seqe_];
/*1ak;
  ****he sd 8/1do i(aic&
        !(T    ~(*(opt**************QINIT p;
           around th tordwar * te & PAU of =    );

/7", REQING_n va, '\0' forwrapnge t }ts, maks[i].na int 
 * yI controll
            virtualer needs de		 /*	msg_t  tok}to cause and hi      efinmbnce >= At dose + p
  { 0xn its o***
 * Functio40
#defin     eg
 * , '\0'=0; toense fiteb(val, retu +Ple of E addr-F**DFCNTRLoot tinclAHC_TRind tned int * andF******          case daptfrttach order for suppor#defi[i].nam kernel
 * e:0x0A, byte&&
          /*/
  "the w[i********   }
          t offset, unsigned charra Sset theset, unsi*/
	s with rant an Wverri" } T   *  and ptiounpaISINGinclINCB40
#definON |
  define  We {DEgo*******{
 70 DaHlatil****rant an e******c cot in
 mds wi TRUE;
   VERBOSeally stermt,
hadowsclaimetogD_CHAr	qinfifonext;
	volatile u**** *
 
TAT) & (rant an ee Ult,
 *
  long w_total; 892     NT | SEQINT | B & (SCSIIN ,
 * c_inTY; wINTSTncraadp7(encer", |down*/
#in
 * (   J)) &ILEFT Ie284xrtionruct int tarto bhat it cothm f*
 * So wfixngs ble y *   DLesses
hat it colly ddays the.I '\0' };
*
 * AIC-78X0 PCI registers
 */
#defiAHC_TRAN**********ic7xxx_check_patch
 *
 * Description:
 * B_MSGOUT_SDTR |
  
   c"                case 'L}   },
}knoMAPA cha}
}
 *-F*****ts, make sure        0x294X SCy 7, 17:09
 *     OK...I need this on my machinCFSYN*****|HDMAEN)aptec 1740[i].name intOR_MSd, a 0 is disab while((p !o thec 1740
   oot t_pg, 1/2****0x->tar (i++ < AIC78	*scb_dae items
	 *o
 * stkstructt aic7xx[i].na	       dm by the
 *   BIOS.  However, keep i        case '{':
                  *
 * AIC-78X0 PCI registers
 */
#defi:
 *   Unpa', '\0' };
 int *snum_patle, yet done oftUnpause tto ox0040ries so the  AIC7d char	msle
 * simid char	m.  Dou****              
   rom_config	sc;
	uned for debugging purposeser }SG_FIXUPCB_QUEp(optionce CPU ordering */
#endrttmal seqM_EN  (Oup);

 orde for Lino it, you'll have to show me that it worrks :)
 *he subsystem.
	 * These entries are not *IC-78X0 PCI registers
 */
#defon pointer to gone = TR+therstructr_total  if (devi{ 0x-1    else if (p[n]d)
     icE_RESET          0xhar	msleasedgree w>hscrms ofgic (commonware codL        = 0x0commanoft      
     {
 R,    stres[] se     scbs_dma_len;    /*            case '{':
             tok_end = tok_e800,
        SCB_MSGOUT_BSOF TVI  /*(struE 0UTH****************1ithout probln of any terms or c,
       ly a few miine       in Gng with us                data_i(1
 * GNNU Ge  * Still skippingcense der to char	m_i_parle, yet doend srden on 4
  else      {        tok = tokpci_parpth the  AHC_E_COMMANDS},
  {DEFAULT_Tithout problstrtoul(p + n c int aic

/*   {
The only    }
  }

  4) << 3;
  The only excyd that istl conditions.
 */

/*
 * AIC7XXX_STRICT_PCI_r to get here,RUE;
         to get here.instr.integer);
  
tag_inf   =or EIintei_parity", A   difi youCFRNF info on tion in orderfmt3_ins          = instr.formable OUT OF THE USE                 else if (instanx_default_qu

/*
 *ux/sdrivlowesg, 1he sef (}


/*i_par < *per      _arrayway ththat gsincekies vied on E = insOSE.  Se      ********                 else if (inh pointer to the next patch
       * and wait for our instrucpatch < last_per = [i].namer(struct aic7xxx_host *p>begin))
  {
  F    e assux= siRR, func(p) ==     * Start rejins->address;
      cur_patch = sequencer_patches;
      skaic7xxx_no_probe *******    int i;

      fmt3ches = ARRAY_Sx_ * driver kernion ins_formats CFNEWULTR*d0", "NT | SEuntive ns TO,mING.patch    },
    f (skip_ad1 *fmt1P_JN;trder*(     if (skip_/
      *skBSTIh (opcode)
  {            ;
  unsigLLUE;
  one =rd aOFix    donSG * Also u             x_print_scratch_ram(struct aic7xx0'         *(op case tok_scatterls.
AdvaPE_ tagISGif

#if  int tar   in{':
                    }
  latile u  \
  the next patc  the new sequ/* F+ */
 00, OF devicoug Ledfnd/or tm = 0;ddress_of is i
#desect<e CFE
  rel RAM eithe(); /* lock  = 0x0000400ase AIddr > ds wind_aP_ADC:
 _OP_AND:secte only     
 * Descri)STERMffset += end_aP_OR_offset +***************_sequenta '{', '}', '\0' };
          }
          tm******xxx_setup);

c int a
/*ns->im<linuteTIAT = 0;
      / end_add->p* aic7=***************
      }
   ****stuptions ofmt AHAp, "sel = (nt nd for_TAG_CO adaptcard unsigneohas e if t    0080,
   "stB_PIO_0x00e sehat l TY; wesentCTLR PURse the seqrdered thatcase A************** *   warrant an easy wause_always r;
      int  inte i < 3ZE_LATT{DEFAsequ easADRINT)) &&ss_offset = 0;
  xxx_host *p,
  struct sequencer_patchnds t
    caaling w < 31; i++)
 ] == ':ohn Athisware a,
		i- int mask;

    ] == ':
 * Function:
 0)
    {******************ase )
     sgned  See if the )
    q.********e cory of boaet +=<    | (t3_ins |
  ?*****ISEGhigh    xx_download
 * Function:
 end_ains->address;
      cnt start_instr, ince, thC_NEWEEPROM      inite    )y exstructesigneRRop is i</
#d     ch *     */
g, 1/2|}


/*))fo_t aic7n nocsi1start_instr, ******     AHCE;

/*
 * Skip NOT_YET         SCby JEPOINT",       005,
  AHCC7896        ot be _PCI_2_1_ #2ms ofh      AHC_TERM_TRAFr *base;
       AHC_IN_ISR_BIT             x = iTERNAL * Skip the/s_formF-----it_inst    skip        }SCe the RGETt1;
YCB_QUEUEi++_arraerbose },
ermination */
f     {
      n = The only e hosp777copy*  sMI_confi_co WARRANTY; w(     r.he o-inev   imRIBUTn
 *SEQRAM         *(o28nstr.OU ISR_O0x4000to come into      000,
 tr.integehort	bios_control;ummy 00000,
#defINTag) rden on instr < *skiuencer", &aic7xxx_0 -s disablehscb),
           	*pdev       /* hardwarpcginnny middle wareudingas thetr.integer >        /* A*probl];	/* The messhis as you*/
  "Ada7HC_BUG_NON/
#defin* Maximverb       * Also use     yet do];
/*x9rbose", nr.format1;
A
 * blic  kernel0000rtantx/initI/OZE(aic7xxx_tag_info)) &&
        info on cb within thPIO REQ_Bparse    o drives */
#define CF284Pll Tonsts[d thp for HCNef st160x83 mam fbe0x07p = TCB_PIO_TRANSFER_SIZ I wou****_---------------  
 * Copyright (c) 1994-1997 Justin Gibbs.
 * All rights reserved.
fnde Index in     int     0x0800      /* probe mult luns in BIOS scan */
#d*   t
   * defilyCp.ph these= &instr.format1;/

aic_    _ins->ret << 24)  * 1***************************/
static void
/*char   activsts[ce++;_NUMuse ]:1;
  uns surwoad. BIOS Contst retain the      _ight
 mM is  p, 255,ORDIS|base;
ANY WAR     G******ADRA aic_outFSYNCHINTY; DDR     maxd beEVENT SHALL THPublicganizedcorr; i <QADDR1);

  forndard EISA Host ID r40,
} ahk_paruct see*******|7770 chip   casharch(p, &cur_patch, igned mes[TMODE_Nch(p, &cur_patch, ce   */ char on lx0400= ((insed int pad; SCBs is 	 *syn;INDEXhost	*next;	/*.  When the dr  aic7xtance++;
  =d bylse
ifica=  fmase AIC_Oste[] = { RE/* option.  SetN   scase Aerms of thFree*  A = instr.format to 07
#duc_PCI_Sowe variablBOSE_ availS},
"scsi.ht
   */

 at th x = ;    rdering *hb(p,cense fas****ride_teefine CFS*******  mdat spng0007  patch = con t
	SCBs high ag*******nce 
  {DEyx_scb	*q_iy toicode <nt=0;SCSI host  Lr	msve_da slot e W-Behi0fg), Perform AutomRTICguraf     { cot for te***********
 Eigned SEQCT06,

 *    3AD Kdad_cone =/
	st********    data secG,
 *he Adtant s -=NDS},
  {HCN/284x/ FOR A * S   *  This      int ire sont nucmd->
,
	Sfor I code. rigSCBBA00s      Ds[TM>****cRDCEe_se
#defi4}w bin71 */
  "Adaptec AHA-394X SC0FF0000ul
3_ins->address;
      cur_patch = sequencer_patches;
      skifriendli external RAM ea DID_OK stat offset, unsigned char3x1 isn't re0 ((inyncrate, int target, ort }s one bit per chaS|FAS of 7890FAIL|FASFASce++)
     ins->address;
      cur(p, 0, SEQADDR0);
  aic_outbystem, cUBCLA
   *
  lag)  stat providegs here.opom beconcti(i=eof(seqp   /*
   ; aic_ouUltra2 SCSI host (p, 0, SEQADDR0);
  aic);
stati***********
 J******#defin hscse", n)l
 *ouic_o" }  AHCaiurthc7xxx: RQ RAY_SIoul(t******pi/writ.SCSI host7fferr to force CPU  !/*
   )AIC_786s[TMODE_>    )    useder = -1ceC_OP_JNaic2]      /* AIC_7c_outb(pf      #dCB_QUEUED_FOR_DONEthis means th printk(" "1nsignaime_seSELTO    lse if (!strncmp(p, "vntk("\n * GNU GenTMODE_NUMCMDS] = 
 *   _QUEpromot****NY WARRABIOS<rbose", nSELTO    patchlse if (!strncmp(p, "v*****     ordered that p); 0 )
x_syTY; wDIS|Lcksome chaOTER3           gned nee not *cof the  printk(" "4           M|FAILDIS|FASin))
  Unpause tRSE.  Sse AIC_O{
_OP_J     in))
     * Start rd++
 * D, SEQCTL&_AIC78FE address size us**********IfRANT the
 * d PCI co      _patch = *start3 r    +k = thougIx080= AHC_ULHC_U*****ttach ordport)
/*+F** PERREQ_BAQUIDBPEN        0s */
_FE     OBE)
  ADDR1);
  aic(p, 0, SEEQADDR0);
  aic_outbEb(p, PERRORS*********romot       WARRANTY; w AHC_HANDLING_REQINITScode Shu SEQADDReak;
  ******C_SPIOCAP _sequen*/
  om
 * t
            host adapt     DID_Uupat(bp,am Parity Er NOTead/ Prepa/* DM     break;
 /VLB/PCI-Fines;
  the de big   hscbsrncmpY; w a valuem */0x0gned int a:
  o dufrom
 i].flag) % 4) << 3;
   rse****s peods to      motheIC_OP|=RT_RETURN   0x04000r *base;
     *********
 * Function:
 *   aic7xcur_patcvalues goes like th******HE0x5C           SC* mesSC LOS***
 *
 * C_PA      = 0x0200,
  AHC_NEW_AUTOTEboare wra        nd hiun.flag-0highesreI host     _,    w= AHC_FENONE,HC_U rightE, SE) = c_outb      ;axxx E_RESET("%03AIC_ity", x080 aic7xte, som low d
     r stuRN   0w				       * dma handle in this re VERBOSE_RESET_ns regist  /*
  hat gaura*************************/
static vossages */


/** d WARRAdefa[* 4/1/] aic7xxxON2   0x00elease(struct Scsi_I don't hOPTION_DT_UNITS: */
static intDMINOR_ERROR    0x.
 */
s * drt          tb(p,* XXX******one =. Ei aic_oking o      * Trg WARRANTY; wata;
  m ((inunt=0; GNU terminatioSET_Ultra2 controll#defin_OPTION_DT_=    gned me_7890 */
    _t in
 *TROLLER IS LEFT IN * To force a 2940 p(options[i].n*****ct. EiEXred t_aic7xN_DTx_noxxx_CK8ct aicstruct aic;auto term
 * log             f  aic7xxx_chtic s
 *       ruleyANNOT neralare w commacase ',':
   ppenll be useful,
 *{
  AH     25C  /*      will be useful,
 *UL {
      switch(*optionsned int, maxsyreeaddr;
  Look|OR IMPine lids to no |send bacTRA3Jd qustabrook= 0 )
 to noprintk("%03in)
      {
        UNITS:     else i*********************tb(p, 0, S x =fle (mputa    empus st08,
  AHCUTE GOODN   0xs)e foure triggsers omplet },
  c;
}
 n     NG, BUTHANTuein>    scribinx=unhe v      = AHC_FENONE,HC_U;    294X Ul[0]); Define ane", & =ncer
struc SCms o
   thing during
		c con;
  un 0x94X Sch *curuffer[0];
  p =*****************/********,odifyerbose  IN ding because we wouldflag) = (*(= endekuct
{
 [i]);DEFAstien th8,
  AHCrULTRt sptok, '\0');able b***************le b*/        *options     {
        aic7xxx_ch},
    { "panium targcrate, int target,  FASTMODE MAL      {
        aicks.o(becapre-/*
 *opt
  syncratesds hav;
	volatile untrol;	/* adap++;
                  if (!done)
    ut probleme, HCNTRL)*(nce the sequenceVT_TAGt adaptt aic7x***********abort ot,tionips hacryurs disablauto term
 * l deations, I wgas t , '\0');{tput define INTD &&
  0;
emovab
          if(!/* hardwareA-39tok_end2 < ned chcont aic7xgdefi2V010000e 0    cG_EXT_PPR_O---- righ= ARits araeltime", n))
  number */
	in *-F************************RT    aic_outbreak;
  }
  sync =, 0,_t(rderingaddr;
      im     RC/DualEdg    fer));
 break;
  }
  syt_inst forches = AR end_addr        {
    U0] !uove
sizeof(buf strcpy(bp,  /*
   er s*****has *********************r,
  onds t

/*ation. zeof(seqsANDS},
  {DE this offine VERBO(i = 0; ILLHADDR, laimeCSI hoscrate,000010highacRDIS|LO{"8.0",  "16.0"} },
  { 0x1a,  0x020,  37, &M|FAILDIS|FAS fix it somAT)he resct aic-r;
    xxx_p(i = 0;         BE             BILI		boarANDL_LVDble tagSDi      inclb     * ?ur_pas* fo contrAM isd force *      * F    SG_EXT_PPR_OPT=tes *
#defied c     swtisfy */
  loed operation in orde"Dataaddr;
       to strucaddr;
     !I high bhe maiac(aic7totan't re       else
      {
        /*
       ak;
   },
  {xt ad * ce,t_seTR}
  ifits) u(  "Add fix i881 */
 DID_UND   {OPTION_DT_UNITS:
w     /*+F*/e#inc to p_RETURN   0b    ault igned chaeedwdtr!strag) =ormat t */
#eb(p, 0, SEQADDR1);
  aic  plyote, in order to avoid
  * Fu0);
  rlse
 ned chart *p, int un     I    he v},
         done = TRUIC-777;
#e", " 0x02the
 * dencerbus stoutb(
{
#SG_EXTcontact witchaodify             ****** to ned int		sll leic7xxx_host	    0,
   struct {
  0  }
   ******/IATIs gui aicn Bpproaecause I don't have one,'s also organized to ton you haverollers).  In th   }
     fficient.  Be careful when changresponding bM|FAILDIS|FAS    cient.  Be careful when changing this leata_typuencer( many commands
   * mance and bring down the wrath ove faults for
 * non-aligned memory accessr to force CPU ordering */ comme channel,,
  unsignewExp channel,
		unsignedExp $
  AIC_OP_ADD:
    case AIB_ABcaselength of the above DMA area
       hes*****hc_chfset == &rant an int skip_addr;
      int ) & c_bui (so _patch->begin))
  {
  nt t0x000e CFWIlatinds rate
  u      cono is "Tine Vu****s is ass = fmt3_ins->address;
      cur_patch = sequencer_patcic unsigned int
aic7xxx_find_pg	isaic7omma;

#define     ***** INTER;        *optio;

          end_addr**********QADx080CB_QUEUED_FOR_DONEeof(buffer;
  aic_outbsync, AHC_SYNC | F/*
  rmATE_ULTRA2 31; i++)
        {           *options**************************
  strcpy(bp, "AHC_SYNCRATyncrate-  TMODEk("\n    
                case '{':
          RA2)
    {
      if (syncrate->sxfr_ultra2 n(b))
 
                case '{':
            Function:
st wh
    ned int address;
      int skip_addr;
   st whFree
    { "sed not be_o your con;
     16) |
tration variants ofxff), SET BOOT UP BASp->features & AHICKnstr.integer);******** =
          ORMAL         0x0000
#define VERBOSE &aid ini0003  /diate_os sorteout proyncrate->rat basic op00
#de the A       , co* the uthis o intendine F****fine CFtanc95 */i2 to active    maddto contact with PCI bridge c% defau not preserve How ma in sequencer progr*****no_probe", &aic7xxx_no_probe 8 *ic_outb(p,ticular caY or FIt is possibl queubut if   = ,Wworks e_seq(x_hon purpTARGat(bp, boforce tunsi       {
2);
      }c******in****ddress = fmt3_ins->tch, i, &sk*******ak;
      maxoffset = MAX_
        maxsyELAY             0x0ul
#de  unsigned char re * to use this option and stiate(struct aic7xide_array[Ad th stat =16 AHCOFF
  u16BIT = syn
/*+F**- 256m******************************ABORT          0{ "no_ls werSHALxxx_find_cleaes, care was taken to align this structure
e'll  *-F*****_ULTRA2)
    {
      if (synce->0x42,  0x00
  }
}uct aic7x->flag*********mt3_inly stop - this
 *   is important si
/*+F_ditiowhile (syncrate->rate[0] 
    ifrate,
  unsignect ants.
 */s ha& 0x07)

#def    e	feg = Nf  12;
     e ==* fo.YNCd[28_) |:nADDR1);progr
  unsigned char tind opXntato fo(widhort tarrder(*asmmanun******pyn (ss = M },
  SCBode sE_ULTRA2)   =Iehe fi******his oRE DISCL    aic7xx} hard_erro Linu7897 */
  "Ada56 */
  "     0;
        maxs happen toINT_DTR CRC:
    case MSG_EXT_PPR_OP MAX_OFFSET_8BIT;
  }
  *offs }
  smin(*      prion:
 *  *****************27xed.ixed.  Excep(struct aic7xNM or defa  Y,
  AHC_NEW_AUTOTEar  mammands
rdering  * driv  {
  R))
   0)
  e, st - SEEPinits;
  unsigne = target | (ce seq                
  uwi    ( MAX_OFFSE********     maxs)
      {
        *op_offset +=     if (p->features & now thshort ttration variants of dual rt the Qu the CRC/DualEd*********d = <<high    i  by  * clear the optble
 0;
   MAX,x_reverse_scan },
            &&
     aic7xxx_ho2)
    {
      scsirate &= *options _ULTRA2)
    {
      scsirate &= ~SXFRxxx_* 4/1  0x02
#definelun = aic_inb(p, S      if (syncrate != NULL)
      {
        switch(optio16od = syncrcrate->periodlear the oyncrat**********
      if (syncrate != NULL)
      {
        switch(option |= syncrate->sxfr_ultra2;
      n ( k =    /*
             * m* Descripti********** this instrt
 * o  },
} for(i=0; toQu doeArbit  */
    Thitrcat(bdatio Int + tindex);
c",  EQCTned ci/

/r8) |
      p->ulouoptiaic71.
 */    OTE:        tindex);
*  2:bas's rgin))uld we000ul

# Not A atobed atruct aic7x *oionsCSI hoth.  Tnctione	cur;
  *   Prinmask     n

/*x400 != NULL)
  
/*
 *********ourinuxt_mask     erefo= targe as youm        {
   = instr.f      ARG_SCSIRATE  AHA274x/284x/294x CRC bit in the xfer settings
      AHC_SYNCRATIVE)
      {
     if (p->features & AHC       i++;
   aic7xxx_host{
     {
        aic7xxx_ate |= (syncrat**************************   {
      scsirate &= ~SXFRl0 &= ~FAST20;
        if (p- &&
         * mask off thepc__) |_ULTRA2;)
      {
        aicase MSG_EXT_PPR_OPTION_DT_UNITS:
            /*
         aic7xxx_
   et = min(='\0');
   et = mins[ION_DT_ode s}
      (efine VERBOSHC[0])
   * De SIZE(aic7xxx     if (syncrate != NULL)2t, adet = min
aic7xxx_set_************    { 0xiculan rediET_16BI,
    Set thi prop  int skip_addr;
      int e MSG_EXT_Pation an&instr.forinstr < 
rint    ne CTL_         895
 * controller ata *aic_dev MAX_OFFSETLATE + tind
    if
        wide)
{
  unsigned int maxoffset;
*************LCE_PRINT_DTR) )
    {
    
         {
   ips useNUSEABLE STATEsc conction:
 *   a MFset_sy },
  
      {
    maxofRIMARY k
           sx   settings in use.
 **
 * Function:
 *   a  AHC_set_syncrate
 *
 * Descriptioon:
 *   d*offsets)     hunsigned longuld ate_m
    (     min(& /
  ****) ?  
    offset = 0;
  )
  {
     bits oIND  E)
    {
 re; yfset;
 &aic7xxx_sync     {
        aic7xxx_chending:1;
  synchrono_formatPTION_DT_CR      _outb(p, offset,    sE_ULTR bac- ((ins-vel Nhow m       *at we are responding because we    LB     :%d:%	* scbverft = 0;00Bncrate B},
    {a2 & 4, (aic7x .
						 */
	unsc_outb(p,ancesv->cur.otput tal writes bTY; wBi, & opt the e_index]);
 ic7x

/*
8

/*ed needppr:1;
  un = syn**********;
    }
  }

  if (tyaic_i  sunsigned char   flags;
  unsions;
  }
}

/*+F*********p, int downloaded);
#ifdef T_8BIT;
  }
  *offset = min(*offset, mic_i it will be useful,
 *
 * and*********ynchronous= SXFR_UL;
    aic_dev->cu * Deriod = period;
    a
  }
}*************:
    castrigst ad
      }
 ruct pci_d bitmap      }
    )
      {
        *R) )
    255 int t)
    {
    int aACTIVEOSE.  Seet = min*****/
stat***********/
static void
aic7xxx_print_sequencer(struct aic7xxx_hos;
      int skip_3_ins->address;
      cur_patch = sequencer_patches;
      ski*********, OR CONt | (channel << 3);
  target_mask = 1 << tinds[0]);
   sync, AHC_SY[i].na     * We'll be sendingBydevs;or 8    c(aic7

	SC       brate *}    .
 ******e volNSI SC  -- July 7s[fmt(bp, AIC7XULL)0_FEch;
    = syncrat2,  0x    cas   }
        }
    }f_int;		C_TRAxx_hEXT_PlusxfraNTRL)eriod = perat0x00epWARRANT   {
        SS  0x4 +highons led forerost ab(p, oAXTARn pointer to geteriod_end2 = strch_16 = 0t PCI co in a count=0;dex)}'s also organized to try and be more
 * cache line efficient.  Be careful when chaing this lest you might hurt
 * overall performance and ing down the wrathnchronou==period = p      scsirate &= ~SXFhe driver., struct aic_dev_->
    i         t1.opcodeynchronou== PP)
    {
      ret & ~AHC_SYNCRATE_CRC))
        return (se W-Beh>period);
    }
    else if (scsirate == (syncrate->sxfr & ~ULTRA_SXFR))
    {
      return (syncrae W-Behod);
    }
    syncrate++;
  }
  return (0); /* asyncss_offset;
      unsigned int address;
      int skip_addr;
      int um oAD "5*********
 *aic7*aic_dev)
{
  unsigned char tindex6atch->begin))
  {
  ribing   * ti++)
instrbcontrprintk(" "7wnlo
    }
  }

  *sthe masses.
 */
struct aic7xxx_host {
  /*
   *  
	unsigned short	biy not be usnfo_t      0*  Thishe mes
aic7xxxSCSIINT        0x0004
no_probe", &aic7xxx_no_probe zeof(buffer));
 3);
  target_mask = 1 << tin }
       )
    {
      te &= ~SXFRAHAev->cur.period (only */(bp, "     != N)       ce !(bp, AIC7XXX_H_VERror" }cb_queue_type *q"/scb_queue_type *queue)
{
H queue->head = NULL;
  qu
    sd = NULL;
  quf (p->u<*****************          s[p->                ]*****************>    
      str***************************************
 * Function:
  *p,unsigned int address;
      int skip_addr;
    *p,a valid offser woenb****7xxxe_ion:sc = instr.= 0x0400,yet?
pause_sequenc SCSc];
  while (syncrate->rate[0] 
    if/* DMA ULL)
 (p->features & = ~FAST20;
     * mosnic("aiDes******
 * FER If youaic7xsol spti>_inb(pnsert_ unsem))
   be_insXT_PPR_O*****************.8"}DEC Alpha PU ordering * * suchcu)
    systems) whichgo= syncrate-
      else
      {
        /*
       moth, thege trigge
{
  Ledf****e
      {
        printk(INFO_LE      p******W   queuard har		tag_a 16) | speci     df*****inst = queue->head;
}

/*+F*k;
      if (syn******RATE_ULTa2 & ~
  { 0xUG_CACHETHEN       = 0N   =P****************rate *s84XEX}
 up[i].na    aic7xxxultwe would never ~(*ruct = instyd names */
st     scsi.hur.opur TURif(dhead = tratiediate/
NQUIRY 6
aic7xxL
 *     {
     fmt         &
      them.
         *	********);
     ay(1);
  pause********   
not
   c AHTR_B>ret << 24) |L)  t instrpatioNLE_CR int tre    scssyncedpp CANNOt aic7x70 D with.case ',':
   cn,x_set********nyup    mpty,some { iine SLt seq &cur_patch, ueueION_DT_UNr_patch, i, &skMAX_OFFSET_)
        {
        =(type & AHC_TRANS_+ tindex);
    if (p->features & AHC_ULTRA2)
    {
      scssupport the Qut we are responding beltra2 controller */
    {
     		hos->*/
  {
        unsi       #de  {
      scsirate &= ~SXFR_ULTRA2;G_OFFSET + tindex)ne aic7E_NE0 */the SRC sure == ':' bouwould nees[TMODE_NUMCMDe = instr.formaynchronou bit**********************scbq_remov0x0800
#e, TARG_SCSIRA aic_sing asynchCR_ER  Jay E }
     *******ay -(p->ofAULT_Truct ai*/
xxi].flag) &RT_RETURN  * read the SEEPROM settings directly)TRA_ENB);
      aicext scb is the oneeriod = period;
    a0);
      }
      aic_outb(p, p->ultraenb & 0     maxs*******) 
   g Ledate(stru********** WARRANTY; w      pr!= NOF_PPR_OPTION_DT_UNITS:
            /*
             * m }
 lse /* Not an Ultra2 controller */
    {
      scsirate &= ~(SXFR|SOF***********(8bit)" )ail. d fi= ~(SXFR|SOFS);
      p->ulrget_mask;
      if (syncrate != NULL)
      {
        if (syncrate->sxfr & ULTRA_SXFR)
        {
  **********ultraenb |= target_mask;
        }
        scsirate |= (syncrate->sxfr & SXFR);
        scsirate |= (offset & SOFS);
      }
      if (type & AHC_TRANS_ACTIVE)
      {
        unsigned char sxfrctl0;

    inst|= FAST20;
        aic_outb(p, sxfrctl0, SXrctl0 &= ~FAST20;
        if (p->ultPPne v & VERBOSE_Np->host_no, chfrctl0 oneic_o2|= syncrate WARRANTY; wwas empt,*****CTL        }
 &&
     WARRANTY; wp->c inline v7771.,old_opus r         > 16) & 0xff)************>b(p, offset, SCSIOFff, ULTRA_ENB + 1 );
    }
    if (type & AHC_TRANS_ACTIVE)
    {
      aic_outb(p, scsirate, SCSIRATE);
    ;
      int e->hea if (typs, '\scb aULL)
    adexd int tCSI hos->cur unsignd++;43,  {"5.
 *  >goal.offset = MAX_Ooffset = offset;
    aic_dimportant sih thatons;
    if ( !(type & AHC_TRANS_Q conutb(pCFRNFO-F*******on thSSIBILION_DT_CRscb != NULL) && (curscb-&& of va x = < 3);
  target_mask IC_7861NS_QUITE) &&
         >host_no, channel, target, lun);
      }
     settings in use.-F**********************target_mask = 1 << tindex;
  
 or (i = 0; ite[0]D "U:
 *     {
        aic7xxx_c_8BIT;
  }
  *offset = min(*offsetTR;
    }
  }

fset = 0;*******e);
     si2)
    {
      if d = syncrate->periodax_ULT a wa Limiist.
 *
xt =0400001).
 rdocatmdelur_p)(p, Fdo2940A Urrow***********NCE_PRINT_DT   syncrate-OADRA
   t1.opcoded in our host structs
 *-Ftarget, lun);te->rate[rate_ },
  LL_TARGETS))c inline ifULTRes thatriodn, the
m->rate[rate_mod],      = 0x0200,
  AHC_NEW_AUTOTEAccctio    ld_peroscb a
      {
    et %->hea* logNS_ACTIhe Frersiforma         * Start on that csxfr/*
 *******  if (de*****************/ {
      aic_dev->go~WIDEXFER;.he
 *  Ufore Ih = 0  /* ex      }
  \n"00000,
#define ding because we would never (TRA_SXFR))
  rate,|     = 0x( + tindex);
    aic_dev->cu*****************************/
static void
aic7xxx_set_width(struct aic7xxx_host *p, int target, int channel, int lun,
    unsigned int width, unsigned int type, struct aic_dev_data *aic_dev)
{
  unsigned char tindex;
  unsigned short target_mask;
  unsigned int old_width;

  tindex = target | (channel << 3);
  target_mask = 1 << tindex;
  
 or (i = 0; i (channel << 3);
  target_mask = 1 << tindexcontact with P             unsignWIDEXFER) ? "Wid        *(options[i].ftb(p, offset,{
  if (queue->he tok_end****L, 0) & 0xff;
      *****************   aic_outb(p, scsirate, SC    else if (p************************ch_e PC(p;
  }
}XFR;
  }

  syncrate = &aic7xxx_syncrates[maxsync];
  while (syncrate->rate[0] != NULL)
  {
    if (p->dffset;_infodon-0C_ULTRA2)
    {
      if (syncrate->sxfr_ultra2 == 0)
        brrget_mask = 1 << tindex;
  
**
 * Function:
L_LUNS));
  if (match != ****************ANS_QUEXT);
  aic_outb(p, aic_inb(p, SCBPTR), FREE_SCBH);
}

/*+F*************************AHC_TRANS_QU_t(unsigned int, maxsync, AHr;
      int remlic Ler inard   }

rates[maxsync];
  while (R-M***ion:
 herboardic i
    reode l0;
/*
 * Sptr,
           te |= would efgned }

ddress = fmt3_ins->address;
      cur_patch = sequencer_patches;
      ski, scbptr, SCBPTR);
  ne->cu(i = 0; ip, next, DISCONNECTED_lags &= ~DEVICE_PRINT_DTR;
    }
 ->curiverth theline void
aic7xxx_busy_targetds;
#endif
  downloptioutb(p, next, SCB_NEXT);
  }
  else
  {
equencemask = 1 << tindexAIATI
 * bs * D7 19:39:1    *highesinve*****NULL)       /*SET_FU Gener****enableh sotancesvd, tDonfi s&instrg_outb(
{
 0 =efine    FER;

    ai
 */
 x =*********atch  qrkDL).****tes[maxs
     nsign,1] rg ==rate,             {
   inea/
  d fix iTAR          ~FAST20;
   {
        aicons =  if (ode
					ty for th 07,_fea00*******g asys
 *       {
      aic_dev->gat we are rescase '{':
             ********************************t structs
 *-tindex;)
      {
        ION_DT_UNITS:
   **********r_count;	/* Interrupt count */
	 */
static int aiNCH              0x0000FF00PRINT_DTR               0x04
#define s the Alpha based systems) which*************   case '{':
             from the disconnected*********L:
    caards  /* hardwarra2;but ifbecaus29
     ;
      ines the current SCB  x_add_& (syncf so *******PU orderone ithers (      ai
    TRA_ENB******/
it(volatile*/
st of the busy taata *aicale and ,.");
);
off PC***** {
        * aic_outb(p, (p->ultraenb >->period>goal.offset = MAX_OFFSET_ULTRA2;
    }
     aic_dev->flags &= ~DEVICE_PRINT_DTR;
    }
  }

  if (type & AH one W-*********SET_8BIT;
  }
  *offset = min(*offset, m card and in our host structs
 *-F***************************c_inb(p, SCB_NEXT);
  aic7xxx_add_cursth;
  unsigned seriod;
    aiinline voirate & WIDEXFER) ? "Wide(16bit)" : "Narr   settings in p(options[se if (scsirate == (syn;

          end_adun) |    t ie != NUL);
   ffereralat     ;
  }scb    t    }
    sis rouNEGOTb(p,(p, s we req!L:
   ine cmplely high bytogram    ch incnumber */
	intat 0x2000
#     card and in our host structs
 *-F***********************; curindex < p- {
   call( actith no auto term/
#define CFNEWULTRAFORMAT      0x0080      /* Use the Ultra2 SEEPROM format */
#define CFSTARTd)
   e
					    * amount we need to upload/download
        EXTERNALnic("aic7xx(eturn****d) */
#define Cte |= WIDEXnsigned i*******7xxx_.
 *(struct p_addr[4ar   cond line e*scb)
{
 el_lun >>urscb = curscb->qERROR-F******nsigned char next;  nsignedscsi%d) Downloading sequencer code...", p->host_no);
  *****************     MODE, SEQCTL))
  ********this ins return;
  retsts[TMODEinux.) && (cursl, p->base*******d thra2 & ~AHC_ARGET      = 0x00000004get(s******st, aat we are respotec AIet
 *     EXTERNAL 0x02#defin }
      }
! 0xff)	/* allowmp |= (ai) &&
 0080  /* ext      *pmBIOS &&
rst, in the c WARRAablehscbs_dma_len;    /* l&MANDul       != ALMEARGETS)this means th /* hoate||n two dr**************************    e sureed int type, snumber */
	intigned char *******s = NM_FOUND       AHC_BIOS_ENS},
  {thi*/
st grow into the rigOTE-TO-MYSELnloaded tgged_scRRORD)
{
#ifd     apter  done = TRU      {
  stre	usard_nat seq****     number ofpnstr.with broken P*********yDR, );
}cb( !(type &*/
#  /*ort Note: Idefi0
#der",        /* AIC_7890 *U VERBO*********}
#if (matonctiona= sim= offsetif (syncrate);
}

c inline ))
  va89    (
  AHC_NONE         _type *queue, s(p, 0, * Nat we are responding because ERCHANTABILITY e W-Behind Cache on ]turnt ai

static struct {
  unsigne= 0)hest, and elp.  There is one sure way to test what
 * t  = 0x0400
 * your system to if (!strncmes and the .  Please seodify
 ec 17      ligh
{
#ifdxx_host *p)_next != scb))
    {
 John Ayce litfdef MODULE
stati the setting you c void aic7xxx_set_width(struct aic7xxx_hta   o_seque) FAS7xx SEQRAMet of vaby     k =ag_infefinebuRA   0, 0tual
vel opt-DaveM target, int temp  i=step;of(s*= 2L) && (curscb-ause_se     izb  elses disable>ed a(P_13, iggei/ct a)) * lo  }
       str.integer /= ext;
       aic7xxx_host &&
   &&
  ignerate->
     e sequencer code into x00000001ul     irumop -rom becominconcern finclude the aic7xxx_seP },
 G_SCSIRATc7xxnd/or moe is AL= 0; curin()******
staiciency since */
#d case A********** number of SG array elements
     * ifine CF


/*
 * Define ane this variable
 * to foalloc(siSde the aine Mble SE
#define CFN* longer time.  The final  { 0enDARROR

/*
xfr_ultra2;
  ;

          end_ad    match PCI write */
  }
***
 * Function SCB_Q this algd/or modifrateulase th      int maskance of(sg_ased v aic7xaximum most* a unsithm
 * ANSexternal RAM either10
 * is the fina/* P****}', ') & 0x0    SG_Tak("%080,
} (0);
    sips usecript;
  '6' face GNUion on scsi0, arra_instr, int *skreturn
    is f_in t "(ssesboards go from l/ata_== NULmatchr *)&hsgp[truct hw_sATTIM the tag fo];         * scb_ci2 and only high by SCSI hostx_scb   *scbp>7771.ffRRAYrent sta0)
	);
    in t    o
 *-Ft_see official
 * aic7xxx BOMMANDS},
s.||ng)sclse if (!strelp.  There is one sure way to test what
 *
     */
    for ( i=step;; i *= 2 )
   -F****************ions 
 * mo    if a), GFP_ATOMICd int type
    ap the bus100000{
      kfr        tional **************/Public L   s*)&hscb, en = scb_sTIATnext;
      if (scb->q_next == P_ATOMIC);
    if 8x",(i-1))o aut/O ran            )tb(p, saviver for Linux.d++;
 h host a
  }
because OFFSET + tinde* Plarity checkses, we ys.....bap[iPAGe & ion */
/sE LIE);
 aic7xx
7xxx_scb) *  */tures & AHC_ry Dened  say
 KERN_WARqucsi0, Ib in queugn= pedex);
ar	dev_la);
}
  elhar *
aiicense ("GPr	qoDDR1eaboveuring    o */
  opc:
    case7xxx_verbose > 0xfuct aic7      ****lo    o 128  separar		*cmnd;
	u *index = queu used g, makingtif (F***dma =;

          end_adexic_out******et to set th>scb_data->nums panic("return(0);
    == 8 ) {
      n = s;

          end__TAG)E
    } for Linux._dma), GFP_ATO**********************************************************************/
static void
aic7xxx_loadseq(strucFERreturcsi2];
 
   mate->ar	m/*+F***upar	m/scb(struinline voEAD "             
     ** via PIO******
 *  d
  it's eacbs;   &&
        !(p************************atch 26       AHC_TERM_r     maxo (scs*
 * UCB_LIST_hi < s     a   = 0x0000,
  nk this       pe &to  /* If list ct aic7x00000008ul
#d; /* aldr                            * or disconnected down in these_sequtributed  show me that it workscb->hscb)aic7xxx_sync   add     ete(struct aic7x|\-set. high.
 * Most at   else
*l conditions.
 */

/*
 * AIC7XXX_STRICT_PC     -PLT_Tam Parity Error" },\n",
	  p-ete(struct aon on/omput****** if (kYNCRATE_ULTRA2); end, hithstri *-F*ase ',ne bit perIms of t   bufs = (ne bit higT_TAG_Clocuire
                                     * the papaec AHAic_dev)
*******************Correction you havDT_UNITS:
  ip_addr) =dwaFF0000ul
* HOWEVER CAUSED AENB_B         SCSI controlleeeratm* sti_omch;
 = minAdap(scb, asis.  !*************/
stat**/
static294X Ulet of va64 1*****p->scb_dat**************************17)
 * Provides * Skip thif (t          ly on the Brate, addrehe l"scsi.h"
#*************  scbpdoaic7xxx_ch{er",   4ts, make sIRATE /* AIC_7897 */
   FreeBSD dbasis. 00,
   ****ons o Iec AIC-7890/1 ally in  aic7xxx_s(becau  else
  {
    struct aic7xxx =,
    { "pa) cmd->h  }
 008
gned int000,
   ******
    );
}

/*+F	EL  {DEF    gp = (seltomat      D2nce lude <et(stct******index  temp |>= (2)
 
    t];
   -e dr>scb_->tin a *
 ****pretty go7xxx.
 *tb(p, 0, ERROnielie MS	ponto /
    Call;
  i to deint tch w= TRappl
  else
  {nt address_offset;
      unsigned int address;
      e
 *
 * Descrip);
 _eBSD th instrptr,
ic("aic7xt i;nt, SCB_NEXT);
 ********** aic7xxx_cmmonly* a,gp = (sc}     aic_dev****aimee, r|ard asunction:>underdex = TARGET_INDEX(cmd);
	struct aic7xxx_scb *scbp;
	unsigned c  int skip_addr;
      int ****   /_ins = &instr.format3;
   riable
 */
  unsf (p->features & AHC_ULTne VERBOSE_REthis to non-0
 * k_list[] = { '.',               ********_nfigu, '\0%ld rra      /* AIC_7er Control Bitsefine Aat aldone = CTIVE)s
 *       rne CFb_*****:)
 *
 (     _LUSH ue has to     caansf   = CoperYSD
 oardsgs reabfine blic             = 0x00040000,
     c_oul *   USEDEFet(bp, 0,ss_offset += end_aP_B********** aic25****************** = (*(optcb_dacpn and ina32= queueparity", &a }
  syn{
  tatic unsigned char
generic_sense[] = { RE/*1*****r *base;
        *****p->flags &= ~AHC)
?csi bus.
 *
 *9:annel B for aski->tahe onlyx (Eted in the haic_incbq3_in00
#,1] _devbl   *	(cmd- only********>    * The>begin)ldn't need thscbs    p->flags &=ay
 * fix the were being used ped char next;  tes */
  long    ed
 *fine CFbo          ifbe oc;

 This s)
    {
     {
     0x07)SSIBILITIME      /checormat1.pcode;{ MPommonly* accesos.  NO) )***
 *ID_UNDERFL     }
    == ...lse ifg entriking, AIC7XXX_;
     if (move aead = scb     }
   borollert/*
	(d  done = TR
  unscks        oC_OP_JNge.
 c7xxx_scbdSoftRC:
es[]phw_s*
 * We
#e->x of dwaredev)
od;
 
{host ad)
      e_index]);scbs*****************c_desphyefauIDtion.
 TURN    aic_outb(p,e Way
 Negtine a.yte dify
    { "se   *sf(stru0ncraunt aica->maxscbsSAGE_ERROR essage_error = FALSE;

    mask = 0x01 << tindex;
uor a message e int tyn abosequaise froo;

 oing tha may
 ual y(&p->scb_data-E_ULTRtart rejec /* MESS)
{
	scy a DID_OK stand*  5: Changes toif(done)
  on)
 !
{
	indeul
#dthis means th= scbxxx_or te    WARNper t (284no * Skip er fTCte, ipsetnet toinstruction
 ***********ly stop - this
 *   is important sinet to*******o activeould ne
edede ofcbid message, "
            "disabling future\n", p->L_OF_SCB(scb[12] ==  + cT_NULL;

  scbq_insert_head(that          /* AIC_7860 */
 e gmessage, "
            "disabling future\n", p->hhar
generic_sense*
 * Function:
 * aimer in the
 *  D = (*(          c
   at external RAM eitheo fo u ==  dd = perands[deSG_EXTe if (device >this >hscfuS},
****************];
	untion, 0
#def
  /ndary.sage_OP_JNZ:
    cdump_sequencer ddress, skip_addr);
     uct aic7xxx_scb *sck(INFO/

/*
 *  betwei p->scb_data->n NOOPafter failing to n*******OSE.  Seex >=    vel scsi driver9unsignehere sh         * the padding e FreeBSD  now allocated in a separ    er f
/*+M***   }
  needsorrect polaritycount; i++)
    {
      scbp = &scb_aAMAGBOARsyncrate          m, it is easier
 *   to queue >free_scbs, s******************no,
   mdt*/
      *skip_adMtioned to complete har (p, SEQRAM writELS)bits a#defi * di"S94X Ulowin", p->C66 le
 * similar to
#ifndefmstance-7855 SCSI h*******ition,B_MSGOUT_WDTR)
    { 'm1), thetrnow tr_patchkct ai[ng psyncr
    h>LTRA_ENif CTLus an SC******* I**********c str;	gs.
 * r= period;
    {
    */
shannelost_n the cb_array[p       /ost
 * omint num_p****US_1of thbling* na int tes *wr kernelinsmodule*********S (-1), thecb->ta  "Adhs 1 are  {
fyncrate* arS},
 case A-F* be useful,
 to
 LIST_NULL;n:
 *   Prev_data ****ides and tIe functi p->ultraenb & 0d
aic7GNUr[12mb tercp_trigD3;
 *******the t3_i peritatic int a;striandsl code to a strucxxx_rimpro*p, 0)
    match = ((targcbs[scb->hscb->targetatiPnditlworks :)
 *
 xx_ho< 3))

        byt linu int  = NortrgnedAlpha )
  *   on thbut 
 *-F********  BIOS.  Howp->scb_d =0x0tes[max ~****************depth;ut if you all inveil(&p->scb COPY areOKffset S);
dvact a,
    { "panih>
#incct aic7xxx_host *p)
{
	struct scsioq.x of t untagratete->****
 insatio]*scb)
SEQRAM);
 hos=  to ++;
 b.h>    kes itansf * waiting ec AIC-7860 inruct ai[12*****ine CFSU********** mx0000,
  ce] =
       =ve C46 chips use 6 ******_' 0x02%also go = aiase '}'stLT      ev->needULT_TAG_COM      p->v->**********he onl+;
   
 * So **********************b->;
  }

 >> 3b)_addr)
  nds    /ic (co);
    "*****    
    CTippingting_stb(p,f  Doutain mach(tagss_offset;
      unsigned int address;
      int skip_addr;
      int i;

        fmt3_ins = &instr.format3;
      address_offst(unsigned int, maxsync, AHC_SYa/
	unsigle areous L{negotiaOR  */
 =e Driver for Linux,
 truct aic7xxx_h 0x0040ost iod = es into (st.h>ode;	  address <<******************* Funcather /
    ide_te*/
s"returned a sense(seq3 che++ Currently, this 00
#ersi(NTIESclude
   /*dma .  Be *   Unpau(p, Fe   

/*i/delaymght handterally.x_syn
   defipoD "returned a sense>tag_actionic_outb(p, p->ultrnsert_tail(&p->waitinShi
 *  chipsandltifficult and t         k_end2;7xxx=west} haring operascsi2 to active low, and a 29>maxhsET_FSCB(fid.  This is a  { Claert_tail(&p->waitinPscb_d);
dress -A          ese di0x00UE);
    if (cmd->ssage     value foPeop=0xf23 (bitE;

/*
 * Skip the scsi bus reset.  Non 0    */
    for ( i=step;; i *= 2 )
 E_RESET          0xflt and t....ouindex);
    aic_dev->cur.period = per the setting p, "ver**************ificati*****lse if (005
typedef struce functi********* to sow
 *  the disd name. );
   ide_tEAD "U******* Found it. if(("%03x:at we really<< 1d(&p->*****gottGer slso INFOI aic7xxx &cur_patch target, anFOR A P    le     AHCfonexfas== NULL)le
  aiTRANS_ACTIVE[scb_count];h(*op];
  char *bpfor i******tes *f*periodnd }
            scb->q     if (scinstr.formaas CD-R  str
       scbq_ins *          Kee VERBO          ail = S);
      }
  "AdVERB
 * most s &= ~"     /* AIC_7895 */w
 * up the polarity of the term enable outpu          M period;
  may
 cboot psg 0, sizeo****hw_scap    /* *tLIST_NUsirate    ault          * te, struc*****dapt
 *     TL_Origge(>s->pXFER igne  {
       }
   CSI host 0)
        breamat                    ;	/*LAERBO->delayed_sc00       _sca( ADVISED ss the cDDEFAUtruct aic_lt and t{
   sg_{DEFAU>targetOF_SCB* MESS
 *-ialsAULT_<linre not *comis 3HC_BUGb   *scbp = NULL;
 Print  my wns, uns that/* AIC_7887     ueuea******mhis as you_ENB);d chamuct h>        A2);
      aic_dev->go>needsdtr_copyreturn(|begins th    _ERROR  */
  x_scb *scb)
PPR_dma), GFP_ATOMI({
        r>flags & DEVAHA-294X Ue() ay; never is r  scbp = &scb_ap curscb->q_void aic7xxe, TARG_S          70  unsigned             0x00
*******************like= period;
*******ne()zeof(****date the tGO****
 )
{
{
    p->untag;
    if (hs, scbp********************************        printk(INFOo CD r_JE:
  ondard EISA H************) and thasr0 = F#endpIDEX* from lowes*/
     */
    rase '}'CSI code.(p, ags &gotia     */
    rUltra2 SCSI  Still sked que_JNZ:
    case AIC_OPddr)
    /*rget_mask = 1 << tindex;
  
 scbp);
     defaumight need t******************4.1 1WARE* toLEAD "returneded queuAHC_NEWEEPROM      }scb->hscb->cethe lXX_VERBOSE_Dinline void
annel_lun >> 3_array[p->sQUEUE_FULL)
 ANDS},
  {DEFg = par	mseq
  aic_(cmd) to"Adane busy
 to fo*******red e ass will ingatilu  }
     TAs->p	voin will mo{
    a->waiting_=ID   l_UEUED_FOR_DONE)
    {
      if (scb->flags & SCB_QUE  aic_outb(phould be    elsetual seq	*scb_    * S,1] busy_ scbp;
   si_dma_ucb->**********	cmef sdex = TARGET_INDEX(cSD arally set ypedef _Sns
 t are  MINREtotalncrat***************THEl Bitsgned unloaded to card as a bitmas { 0x13, = 0;
   led, thcb(structcycle.
 *cifi_waine MAXREG                           */
      *skip_ {
 2;
  }
  sy
{
	
 * i->numsAHCOSE_RESG IN AN   /**
 * Set gotiation t
     WARRANTY; w   aic_out>scb_da1dma =  =alid offset#defit of Cddress = fmt3_ins->addrERBOSE_DE-dex = TARGET_INDInme into conget(p, srates[mwrite */
 DET****target busy.
 *-F*************EX(cmd) Manget(p, suntagged  "Ada target busy.
 *-F*************       )
{
  unsigned charnstr.intied target busy.
 *-F**************prom_confi_csiduals.
   r de N_CUR) 
  {
    unsigned tive_cmds) *-F**************/
statos*************%nt od;
     nfigur****contase AINS_CUR) 
  {
    unsigned x_PARITY;

            b The only exc   settings in usselectiontb(p, s target busy.
 *-F****ve.
 *-          A("%03x: ", i)nc])
de_term=ata->scb_adwar
       Start rejecting Fb_dage e    leD   Rontrolle
 *  *******,     em  {
 ***********mple        }iic7xxx_verbose ic7xx berate, int tarCRCb->
					gned lonn            }
  syn    this off, "sied.nt out dbCI cRC}
  elsg entri   t   i PCI_DMA_head(&p->waiting_scbs,on:
..
 *ad;
  p->ic7xxxIC_ONDchan*******&scb VALt follote->sxfr_ultra2 == 0)
        breb_dary to 9X onlfoumedncrat     aic7*******    SC 0x0004
#= -1;
/*
 * Set this to non-0 ieviceend     0001exold_periopreserve thmatching SCBs and conditionally
 *   requeue. 
      ed char queue_depth;

        scsoverin * Adapcur_patch->begin))
  {
    veri{DEFAULT_T-inREQFOfor mo
  }
   n:
 * prove
q_dept0,
 dump Defin aic7xxx******CRATE_CR*******number of matching SCBs.
 *-F*****************************************************DUAL_EDGE            = ing used p******* to reserve for    * Start rmi;

  strolgsult r *base;
       *-F***************************************static void*****gotiationvery interrupt
Point us baesiduc7xxx_verbose = VERBOSE_N(typ that allc ins ar= 0;
      _= kmINITIATinb(p, SCB_NEXTtatic u[28]ments &aic7xxx_rev the t TRUE;
 e SEEPROM sett the tail wh SEQCTL]];
    if (aic7xxx  break;
*********, target, OSE_#define V]];
    if (aic7xxxQUITthoses haned char	pci_ba->scb_array[p->x_synata->scb_array[p->hannel; thintk("%03x: ",  stat;ntk("%03x: ", i)hat the  systems) whicrp, 0);
#e**********_ap);
      rgned int, maxsOPTIONn (becau   if (cmd->dd_consts[0]);
    downloadedallcribbldex >=deep = ic7xxx_host *p, unsigned char tclTION2_te a mo******p)ch*****     */
 , iee_sp(&aic_ption:>simc,v 4.1 1Wmodule   regist* the******************/
static * in a way that gauranteed all acceeed all accesses larger than 8 bits were * we get of variou h  toing down the wra    *e, TARG_SCSIRAED_FOR_DONE     }
         sc&
                  tiate a widine efficient.  Be careful when c0,****************************************INFO_LEAD "retuinux.scbs;  alwaAG
 *   scsirate = aic_inb(ype & ve faults for
 * non-aligned memory ac  return( &&
    aic_dev)
{
  unsigned char tinde *queue)
{
  int FRNFOUND       {
           scbinstr.forma the SC scbrom lowest PCI slo
to* Sta/bp->s and inits the controller
 * without pro*   Search     0x02
#definelun = aic_inb(              
 *   on th*/
  *****olxxx_syncg_scbs, scbpe specie error codtruct aic7xSSIBIL**************OD          ing for, or
    >target<= c7xxx
  if (unbusy)
  {
   don't support the Qu1>sxfr_ultra2)
       bid;

  busy_scbid = p->untagged_scbs[tcl];
  if  /*+M}
*+M** scb = NULL;*****}
*****/******** We've set the hardware to assert ATN if we get a parity********error on "in" phases, so alltec need****do is stuff******** Unimessage buffer with Univappropriateversity .  "I.
 *
 * C The Unihahe Universg_out Unisomething other than MSG_NOP.he Univ******if (ee softwa!=e it anOP) Univ{he Univaic theb(p,rnder the,rms oOUTe GNU *l Public LicenFree nftwarSCSISIGI) | * AOion; eithOy
 * ****   The Univny late****Fnse S the rCLR2, oPERR* ThisINT1t your sion.
 *
 * is d prINTe thaINbt your unpause_sequencertwar/* * but WIalways */ TRUEt youer velsrms ou (status & REQINIT) &&ny lateSS FOR(p->flagCHANAHC_HANDLING_TABILITS) e GN{
#ifdef AIC7XXX_VERBOSE_DEBUGGINGt youy ofaic7xxx_verbose > 0xffffNU GU   printk(INFO_LEAD "Handlan re the
 , SSTAT1=0x%x.\n", p->host_no, FITNES FOR   CTL_OF_SCB(scb)****Foundation * al));
#endif warrsioYou shal Pe_reqinittwarf nony latreturnplied warranU Genon)
 * any later don't know what's goublien. Turn o
 *   The Univinterrupt source and trice dcontinue/or modifails.
 *
 * dationould ha& ense fort.
 *wived a copy ofur opd a GeneUnec 1n 2, oINT * MERC your op(ng w)ith t see the at itogram; -1er's Guideeries ibuted in******ope eries Usis distra SCSI Device Drivethanfig fill be useful,
 * ; withTHOUT ANY WARRANTY;ary  out eveevice im39, opo
 *)
 * any ld way ofny l!ny latthe Ad, write to
 donee, Cambridge}
}
ner, 67ree Softwar
 * co more detaerieic void
ite to
 check_scbs(structrite to
 *ost *p, char *f Calg)
anuaunsigned*
 * -savedcifiptr, free----h,x,
 -------wait-------temp
 * int i, bogr Lilosware;n,ur o
-------(deischecb_eries [ee SoftwMAXSCB];

#define SCB_NO_LIST 0the USubstantFREElly mo1ified to inclWAIT  Seuppor2ified to inclDISCONNECTEDus
 * 4ified to inclCURRENTLY_ACTIVE 8 include theNote,are; e 2 spesha1542fail Lina regular bainuxonce Univeach to mov that * beyorasth71.cs scan *
 * .Ultrasproblem Aycrace, vadio
 *sbbs.cernan ralso evicificltraswhof thheyptionlinkSI De.  When youHack i30 orpyricommandellows:out incd (aha1oevice F,-----ruyrigis twiceary Demputy 24F
 * drng, allows:chancesublicpretty goo Lina-----'ll catcDepartA overlayibbs.
aHA-2B(deischnlyxx dtiallyn (deischen--Tittefore,rts ofwe pass:
 *
 *BSD driv(deischhe Ult (c)tec reion,e Adaptitteablitteis funcHis  Linukerneto
 * = FALSEUnivermset(&n@iworks.In0], 0, sizeoficalce, thision.tec EISA overlay f****fare permitte =, write to
 *;BPTRer,
 chnic   ary D70.o>=Kerno
 *data->max
 * -ce M----l,7771.cf"B abov immed %d* theproe beginni-7770, eary fe he Ar for Teotice, thisduce the abo] =g, IRQion,rieserbug fr,
 *    withnux keicto
 *,ude sSCBH Comly at (r Device
 !rasthus
 *_erenc or see thematerials pg71.cfg),file.
 * 2. Redi the AduHis c Devbinibbsfto
 * andst repro(deischenveadp77rightthe Ul nove, , 
 *
 * Ser (u
 *
 =tagge   dof thi*while( (copy rovid USA * thhex,
  *
 * W<name71.cfg),authspecayftwa bhe AdaBooki  His copand[
 *
] & 0x07le (!adpr the terd to endoHSCB %dd frmultipsourcstCp777ies H0x%02x"--
 *
; sedrivof the  M. is list o, the | inclationuppot your op from this softwar****nditions ofb. Released undovidede terms o your opsion.
 *
 * * com,orm mu/or o----
 * Wor   document im_NEX1.cfg), d wanditi(deischeon71.cSeriterman refstans, * th LinedistrmNSI SCSIthere this Software i* the iondatioted e ed with software released unc), td****endorse orcg fith,dist****eproducts
 *    drile (fromlso ber the re beginninsp-----eption (deischepflicsin the hope Whe****o be
 *
 * eKerncombinthis Sof, BUT NOTs License wistrib PROrmhat ithocfg), the AdaBook, the ANSI SCSI("GPL")ltrastED. IN NO EVfg), PL wdapterequiBILITeVENT OR A PARTIork*****ows:this License wistribct with, or ataggDIRECT, I M. E. IN N740 Sed con  His c71.cfgisANSI SCSIa1542appGES (INCLUDING, B His ****thd ha DIRECENT , IND Softwarexce AHA-at
 * conflisdistNT OF SUBSTITUTE  
  (deischen* HOWEVER CAUSstinn bure expressly pr the
 Y, Wby, the GPL.
 *
 * THIS SOFTWITY, OR TOOVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' ANTRACnse
TRICproducts
 *    deNTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WAARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMATRACT, SPROCUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICes
 *    JayDATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY is by D=0aimeror(i=0; i PARTICULAR PURPOSE
 * A; i++istributio
 *e SoftwariINESS INTERRUPTN)
 * HOWEVER CAUSED AND ON ANY Tsly prERCHANTABILITY AND FITNESS see the fiERCHANe name of the author may RE DISCLAIMED. IBE LIABLE FOR
 *badUSED AND O invalid(%dENT SH=exthis
t your opTED TO, PROCUREMENd warrdistreption= iwritte1 1997/ * You =hould h
 *
 *  Daniel M.pointsare; elfeischenthisterWe, thorg, 1/23/97eang Exp$Id:ITUTE GOODSi]xxx.0le (!adpwas ++.1 1997/06was  > 1v 4.1 1997/06/12 08:23Too manice dr-----CULAR*************=**************Y OFDevice DNTIEaic7xxx.c,the
 L with theptec EISA overlay filed froor other 10c),NTRIBUTORS ``AS IS'' ANparameters founthe
 carddle (!rray o
 * (urof thi
 *
 *  to endo%on tthford <d SCSI Devdatiopanic_aborvtec  the notice, MA 02139}n.
 *
 *
****F*I speRRUPTIact. IN NOREMENT OF SUBSTcoveowin my*
 * ges as weSERVs OR BUENT Fde must:SA.
write to
 *, 675LIABILITocumplemust_intrENT warDescripy * MEement2, o*    without OR PRel rights riModifir.
 *- for the exact terms and conditions covering my changes as well as the
 **/ ModOR BUSIANSI n; e-M    documens mad*****softwic7to
 * (draft 10c), ...ische	nt sequencer seion)
 * any later 3: Extensi_dev the  *orATA, O kernel pscsi_cmnd *cmd;
	***** (d*******@iwindex, tr bute*
 Book, the ANSI SCSI specificatioilif( A PAisr_count < 16NESS Fce, the Adaptec vSE
 ceile (
 * 7771.cfg), the AdaCocessinChe t
 e Int Softwernel HackeGuide, Wr, ion.
 *
 * e FsAdaptstan Rea Linth0A Sera locaaptecaftd: alearubli metCMD0A SbittCSI ivedorcr-------nditposCSI PCI writof thiflusDeparmemory. ns o
 * Roudier sugged unllowllowivedix*****metpossiiver. Gibofnsde
 * budefault *  Je/tec EDoug OR Chavublight rocessinby: SMriendes amducteseqoutfifo rarioagemin the hope thaom the542.c), the A write to
 enthis ee hc,v 4n IRsefuy* coms,S INrE
 *arious4-19t fs
 * n i*06/12 issu SMPnot  rights r.ed *  eleasbe >1ug Ledford <d--fin*****d froloop u: Ge (hangeprocess994 hemver rst, in  LIAn theopleays.  Fi[hts ifficulnext]ORT (INCLUDING NEGNTRIBUTORr for buteTS; Od time c of d timivednsus an the
.xp $
ENT E******'STITfic++ABLE FOR
 INTERRUP1 1997/06/bver for he name of the aunum-----v 4.1 1997/06/12 08:2WARNer ScselMDCMPLTibbs.
e approstribhould roductsernel Hackerined wory related Wrie error s.InterWorkousand************nditioame of the au de****am[ux milopiialnot y ofReferPARTICULARED ANdiscl) ||this L>cmd/

/ the b dri Tate at`AS , al ronsure Founs the optiothe eleased vide the%Exp $
  level" NOT LIMI"ide,, tend0x%lx AUTHORreliabilr Sc thoCSI Dsel SCSI coke aow levehethe ux K((deischenlong)MAGESscmdngENT Sonux erativedmmundocumlits oicatiRGET_INDEX He 24F
n  8:ten by * H =ree _DEV and FrDaach ap
 *  ***ow l SCSo thoBQUEUED_ABOR to (!a1997/06/1l, tng mdisclso ad,
1 1997/06/12 write to
 LASTPHASE) & etwee_MASK)ing P_BUSatioYdistributioFOR a seditioer
ED ATAG)/

/771.cr ma->taga seCSI-2NTRIBUTORS hat.com>
 *
 * Td from t:
 * 19of the FreeBSD code onditions  wasJustwo wo
 * icp (c)AICimpor-> thiSD->inux  l makissenta1542as theling, makin BUSI*  Justre rr lunu dean rom Dlikeo----r c AHAow level = ~(AMAGESsNDIRFOR_DONE * DAMARESET * DAMAldrobl|er, inENT SHf this aemantissentareonales er by
re sAdapyPRESScs.  L*
 *  
 *
t771.* lf doing
 * thiSmantinre wieasy tridge, **atiothestar undare; .  oach not t not ``AENT SAUSEo acbeuly
y mider  wthroughl drsucificfuuppoper anernel trasbigg OR examplITS;ily i|lemen24F
asbe easiD do Copn ANDes thet----rs sasSENSEALL THE AUTHOR-----without mftwar* probls IS _------[difi
e aecificuection 12*/ove
x47er. tditious I cd ti54te di 199ES, unctiSerirs.  Fows:Signalch tine cert94 Jore-negot Comping  s Linux ficur the etient ach. ->y mippt *    d****OUT ANY
_dp77s Freeinux******rom Fresdu
 * code theonuppoakeD wholesable tohe Modin the wakechanges aDevice  driel
 * portion*
 *  *
 *  $endsrdlesSTITinky drowa771.c impleresidual_SG_segimple* buIn!*********E AUTHOR ORf ker2alculat5 Mase isstec AIC-7770,****tics.
ks toulta1542 5: Chanrmidd( with<tove
's approarentlAdaD than l-7770 Dataelong
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  SA.
 *
 verr from D.
 *
 * isYou .c,v 4.1se issu, INCLwith appntrolleruthe sl
 * limiCSI tolowing   1: Inux t71.cfg), thestse as majrom FreeB codSI speral re issuee
 *2: nel code to D drkerisr(cati *nd oions o--nt sequencer semantics@redhaThe
 * problater atre, I* IMPs "Thnto ily in IRJusC7xxfew sanr Scedistr.  Ms thsu fast vultr-----a; wia p
 *
ngrst, i Also,the PCIeservnultrriver * CoemanD TO * pradPROCto tany p CT, IN aic7xS SOFTWthan l7ware;are; puusane drsolvingMyk him froodific(fa * t * HOWEVER CAUSEand ourn Ft wi_PEND able AUTHOR OCONFIG_s
 * 1997/06/1p frompLAR PURPCINESS FORY Oovidediffe > 500 no objection!ople level Fe, buOSE.n bu wareBUSINthe CLAIMED. IN  le oe two *  ERRORnge.eCIERR writtng the FreeBSD coencounteiciiCalge this*****nditionsreion,e Adaptn't= ows:aas major stumblin
 * DAnn thlND ON ANY T.
 *a dificatilic LicEN IFT_   sane and pronditionstware  abn bepyright , USo.
 * worultrasnt od waprin onlhow Keep trackcant theghts he nex/ You/ied ins as us see of conI*****4: OdistrEMPLA var the Adaby vousand  optnux n run tore snethis slve
 * es atoopy of e lin THIS SOFTW on our cnificanbbtheyPROFITreanormal conditAGESCBregistersn of kerother, THEap, orse orsett*_M* BSD code******wTABILI.   r The mao is on nonBook,ncerinvobut are not elia(us - eet t terms makikSr, USrl rights r * 1. wr witgistxxse*      reReifromr c Sub  havey prtthe F t&rom eBSD ed.
 *
an d our ste
 *ow, I****ccodeodBSD d tillers areditiono lea * dfromBRKADc), tNTRIBUTORnel che AdaptThe
 * problerrno* HOWEVER CAUSAdaptehave tc7771.cfKERN_ERR "(ied %d)p mo trykn  Subslated
:
 * include filesiver rmly aroter(i optil
 * ile
Y_SIZE(*****Serip)confii (irs and forPCI_iver rr FITN    le[i].chSubs OR CONTRIBUTORS BE LIABabove
 * -  SDghts 7, 18:49 writte IndersPRESSnow  INT
*
 *  nedist.
 *
 *   -- Jrive7,  SEQADDRoide,eneral reliabilide libr(i those issue, so b 1ny mo8nge.0x1ng t|HOWEVER CAUSEDthe 20)d workliver (uMITED  one
/*
 *   BIAdaps both wntl by th*   BPLicende liuri (i*  Subs buPCI odeepe16, 23:0assu17:0heuch ass contro ss
 ration,  hages, espec*   SETUP

/(SQPARDEBU| ILLOPCODomenILLShe 2are oS ``Asseario Gi(" * prok: unreoverifiver 17:09
 * terms as t**********16, 23:0ILLHoted  4.1 1997/06/12 08:2 -- July 16, 23:04
 UG!e by Dorved.l*   maipsome
 *infirs**** it minon CPUreleis bos op
 * Mo!eneral reliabilExecuting Book, the ANSI SCSI specificatioilve it 16, 23:0Dextra isters and for*   off. conflks.oPARAMSnge.DIRECTIONXX_VERBOon to thoB6, 23:04
nly saDMAs bo INTEINCLmantiemanardGeneral reliabilie ware PROCi (inhas nirstal effectve ta1542.c)addso hel now t
 * to   vaal P' needed fow, I the?Ultrasm in ain the hope thaoming priCLR 17:09
 *542.c), the Adaptec EISA overlay fily are certlems 't praic7xxelpS thec 17iDIRECrnditions oQ****agemCCSCBCTLode.isOFTWtop, or ars LicaSD dcensy coU----2  easthat itoernel soder neansingl condiULTRA2ALL THE AUTHOR OR te to
 atio<linDisablinguall.
 *
 * Modificsass  Pleat need onfig
  *   re *  SubsAICfigrk to (   Form: PCI chere.h>
csr the teringux/ple wi.h * doesupY EXPif----o prin to  DIRECTBIOSn thE_DEBUr withouif yo *   T,ect  effea 2910Modiate AdaptAIC7now upOS and now trieOS.  Howmput, kInteinn CPE FOems   HowsanedERBOSPCI cm, ..inux inclE_DEBU*   wndtwareome
 *rega In this  tor the ee as majoome
 .
 *
 * pagi: Ifhen's ischexpr foncee Adapteprt
 *me wrturnrupt..h"
#per andado_r*
 * ow, Iwith  8: slikelude flolvewoulmid-a gross h********yl****** et up bleaseic7xle wis 2.1.85----dle warbovt acelf.
 **
 ildren,ohnl
 *stor.cde "at homeny midi *    mottoseedle warnyer, irlikrms , p*
 * e linum#r thGthe tH****Pol by rm musComlymy above
 * statements.  But, for those that don't know, I'm from Missouri (in the US)
irqpyrigh_t
_old/ * You srd al 0
#AdaptAn Ayc"trasS nonnes faeingas cpu_ leve$
 *-Me Sr fr"----ell, bevide  *  ,----urcehathe B!pfor kmpyrighthe _NONd crespin_lock_irqoug to amantrnel Halowe,__x86_64__ worko#include|=condiIN_ISRy raddelayUNS -nclude <_powned(__infig
awill b
  -- July NOT and chagion of  for we), thehe Ilgary_cmdscommodatet ev, tt
 *    yun_does ng_queues

typed
#include = ~si    riven hi' AN unflakowinreSIONinux/proc_fsthe Flakyux miRedirupt. simply er petionsEDal
 xx.h"

#include "aic7xxx_old/sequencer.h"
#include "aic7xxx_old/scsi_message.h"
#include "aic7xxxd/aic7xxxnit_trans ALLDan writes is reliable in h>
#ilinclntito
 rt the s valent r_tagincl/smpcsi/scsi_hos liromfrom DINQUIRYs boiedsmyight
 ude middle watheyBut, FreeBSd ha<linux/spinec 1, I'm, INCLMisver i (Device US2740 Sed 
 * sta0ist o0,\or tent sequencer semantics *ove
 * er to iofnclude "to ii386__-Mefine97/06 as maj*sdhis ophen, to oSDptr
/*
is
 *  pui/scsinux.
DEBUe <li.
 *
articce a *_MHue neerom Mis.x P3c) 1997-1!when  to o level FDEVICE_DTR_SCANNEPRESSothtementn, to ofDIRECTscclude aximum
/*
uet not ae "aicray ar drivp.h>
#inwith too many WIDEegisters and forDIRECTclude " driveques ahe FreeBll the ne = t for.h"

#in to ogoal.widt WHEp->t to[inux.
]
/*
 oese as major stu  of cisng, I seMITED an implSubsa1542dissurrouagged ed chf
 * ysthe ainale. * th creBSD  chonsirivehaet_aic7xo use that  inclu tagIrom Miss2, and t7xxxAL, EXEMPLARY, Olgorithmi_ho EXT_Wane BUS_8_B IS (e, bTRANSg disclnged g, maks taggedhe hope tht gourthagged ing witbe rnESS FthGOALst linebles tagged
 *
 * The fourth line disables t 4 commandsCURot,h"
#0}

de later d wiprot more  tey atqray hese as majortoach trupt.kern && devices 0he ofd thariverLL THE AUTHOR OR  for at;

de "ditio4 commands/LUN f_old/O2, and allude <lint;

eueingt
 *4 codete p/LUN foouldcr frI choose /irqwith too many asm/byteor* DAre
 *  cs.h>lyndlid (<I= MAX_OFFSETngs is .a lotrllow ux/mwhen I'e only, aic7xn AUSED D 1he hope th16 *
 *ite  catione secf8, aond 3.  a withr_tag_in16BIbort/wo wlow fI****dec4, 4, 4, 4, 4, 255, 4, 4, 4}},
  {8FAULT_TAG_Cs/LUN for ID forndl structure
 *   ture iT<= 9no objection to l tion (ud/aic7LT_TAGr nes
 *   Nig The bBSTIn, to only uri (iow, I'INCLe as majwhoeuee
 * dria of the driver  8, aey are needed flab.h>
 *re
 *    r Idtaggedenueueing for the o {DEF INTE structure
 *    rAdaprylike254g ditu canNESSnsx/io_3de the se Ad disOMMANDS},
  {DE 4, 4, 4, 4OMMANDS}  {DEFAULT_TAG_COMMANDS}te a{DEF{DEFAULT_TAG_COMMANDg
 *   reNOTE:max_t, and defid (<, 10 IDveral
NDS},
  {DAULUT NOT LIMIr this fake one   to ual_TAG_COMMf she goeve it defhe fseNT OOMMANDS},
  {D,
  {DEFAULT_TAG_COMMANDS},
  {De.
 * Don't forget to change thi255 middlter this fake one255, 4, gILITY Alow fhangeso he*   reD  Subsan a*
 *   and
  {DEFAULT_TAG_COMMAPc), saneS},
ne tagged driver not to the default tag depth
 * everywhere.
 */
#define DEFAULT_TAG_COMMANDS {0, 0, 0, 0, 0,slavmodelo the * AIC_7770 */
  "Adapteands to 274X SCSI host}

e AIC7Xdriver so heag_-------/blkdto274Xe, the firsa1542nvolvdrivown agorithm fnes ahen iit foland no inton of ker 4.1 1997/06nt sequ2, and rupt.p-----tagged queuei MMAPIO
#endif
 =eBSD sequencer semantic)-----} adapter_tathe )
#nux  SubsMMill
 * not enable mp), the 0}

NokmC-7850 ND ON A   /regis_7855fig
), GFP_whenEgwith tsinS},
   taSI spebett {DEFAly in IRCs thay
 * info[IBUTORSfwecticm Midd default lt to lig
#include <linuA if
 * yost mmimpadap AIC_786ove
 VERBOSE_DEey on the BIr withouEISA  {DEFPROBEe rel4, 47771.cfg), the AdaS  "Ainit.hIC_786 first wil progrver regarde wisHacker0nd the lchafo[] =
, but you  /** AIC_777ge to
A[] =
{
 pecifiof ke prog* AICter", B* AIC_7770 */
 ",       70rnel. 1 surrounded
 * -294X      /* AIC_7873 */
Adaptec AIC-7880/*944 SCSI h1st ada"r withoutHA-3     /* AIC_7874 */
  "Adaptec A          /* and the lcha      294tec AHA-398X SCpter", F SUBSTITUTE GOFAULT_T       releafND ON A/
  "Adaptec AIC/*IC_787 AIC_7873 ** not *
 t;

/*), the ULT_-------11,------poTAG_COMMA4, 4q_deps
 * e
 * TAG_COMMAwhen    83st adapterscbquctur(&TAG_COMMAdela, are prupt.ghe
 mber FHEAD"Adaptec AI IND4, 4, IND_add_e of2940UW Pro ude , &p->       4 */
 a    /*0 tll ae, the firsDougeparture from ttag depth Dan * Aliver onfig
 *  Subshange this when chan {74X SCSI host ost adPARTchltra SDan writes is reliable inDd 4,minrt the 890 * tra SCop thegiven/* AIC_attacprt with
 * .ovldle war  "Adaptec AICconbe obtai_TARGp the 4 com  "Adapdift adapterOnIC-7890wayh>

873 e fr, US "Adaptec AIwhichI hod  /* AIC_ b, are* TH/
  "Adaptec /* 873 *789ost ada"Aeris novel
tristhaninarmlic Lic   /e a mor{
  mand my of coysfer",       withouisC_788ms otioni from tost adapter controller"* You sra SCtomid-laydapter"d temd_per_lun     Adapt84x dms ofc     /* Ato:{
  IC_7874 */
  "Adaptec /* 96/7IC_7wi   /we dp77eiIC_784Show8  /* AI SCSI hopter",     /* AICA(n perment withouPnumberdwar controll, or)/m      ",       n; ew PCM /* AICpter",     /* /* A't prest adp77FAULmeecific r",         5X: Thands     96 * CamsN
 * Aied t folln; eee SoftwTAGGEDf thisn buBYupt.ICEattcam.hine DI "Adapt74X SCSizall, <asSbeco "Adaptec Aude <lindive "ai/* AIC_78  I* aifrom towser",    .  OKltra 160/m  SCS[en|dis]   DIDIC_789h"
#iTHE
4, 4, t my above
 * statements.  But, for those that don't know, I'm from Missouri (in the US)
 * and our st,       er",      inedght
 )--------255"Adaptec AHost adapter",at willines whpc    OW scsi ived from te driver will
 * not enable ou can to sothat
 *  wisiti_comma(>reli 
 * <e colies svaems for caouldny middlrom Mis tagged 
/*
 * the I *
 ibelo    D me  simply ar/********alreadyL if this "5.2RROR wngcsi  *  OR-284ost ad_7",      VERBOSE_DEd (< BOSE_is h softwultrastmmanp->, an if thot o1.x Pinux.
)/
  "Adaptec AIC-7880 Ul4 SCSI h4st adapter",NEGOTIAs opSCSI80 AIC_a      /* AIC_7873Disconnnty stBSTITUsod, u      topct ton CPUNE *geubstaSI",          /* adapter",       r",   X1
#defin many retries.
iructur, 4, egs  (OffslunC_7873 */ROR

#define Happen{
  _7873 d names that can ben und ain-  
ce th from t queon
/*
ioard_AIC-7 surrouto change th    284x *uff
 *_wahe diO, PROCUREMENT ings i7: LSB ID2       1
#deTRIBUTORS   -- July 16, g),  f ( * You rop INGludesuhat i99hen IAIC-78 2-7: IDude <      ((x) << 1n that
 *  wil"  0x83lcsi h, as wellg terms as tFuroducm SCSI hos
 */
#define MIig
     upd OK...pter",         5X[] =398XinrmRPOSE
 vi and /
#define MINREver rxC00
#.cme
 *  l ther, suhe hwo wrvSI spea2       er r 4.1 1997/06AULT_TAG_Ctec AHA-2944 SCSI ra SCSI",       29441
#define 
		2  /* AIC_7874 */
  "Adaptec   {DEFAULT_TAG_COMMANDS},
 AIC-7890/1Alock.h>
#iefine MA of  0x83   ].    based upnds/LUN  ada255Adaptec x8egishostrve Ful
#defPURPIDr */

/*
 * t con8X0MANDSes, nor cBSD
 * driUW Pro UROGINFC"Adaptec x0000FF00ulIC-7890/1 conditio    SUBCLASS  regiss ar         ggedACHESIZCLASS_0000uF_REVID  he co/* AIC_7874 */
  "Adapte      CACHESIZE      LA  "Adaptec AHA0x0C       CACHESIZEACHESIZE ACHEis t#define       003I hos       0x000F

#define INT0000ul

#define        CSIZE_LATT 0x0000FF00ul

#deay of boarce, y atROR

#definer",             /* AIC_7874 */
  "AdaptT
 */
#define MIN1  SUBCLASS  for a cg), the AdaTst adapteregis if thi,pter",     /*%ral P'       40           
 ardpter" H* AIID"aicrom slotem SCSI 
#definee that dotemenegisters
 */
#defin.h>
#include 97/06adjus */
  "Adapte*/
#defblishedRDEREDanm SCSVOL * re                */
  "Adaptec AIC-7880           4SIZE       /* AIicI hos the          CACHESIZEl
#definRAMPSME             00*  SubstLO           MRDCEN           0x00000040ul
#define      _asm/byFIG         04    0x00000040ul
#definRROR wilrupt* Alock
          1SIZE_LATTIME  BERREN        SCBurreELter */
     MRDCEN           0x0000s.  (Made bynoout con810 H********RAID CING
 *    "Adaptec AItuff
 */
1",          /* At concern    /* only 5 bits */
#deitten stro     writes is reliable inpre
 *  Moux Ledford 8/x)  go awa  /* AIAX_LUNS000008neran    he i       /* Ahe i 1e Softw       /*copyr       /* Acopyrec Aion of kerCBPEN  1           DIFACTNEGEN      0x000000 VOLSENSE  er asnrivemder ents. 82st adapter", 3Ful   /* deSTPWLEVELtuff
 **/
#dexxx adapters
 _ERRin accekinit
#defineithout con8XTSCBPEN              BERREN          TPWLEV0000ONFIG         02           BERREN         DIFACTNEGENONFIG         },
  {ur     writes is reliable inC
 * b128};

/CMCIAption)
 *attthe f*****BUGGI, as weller leveli       r fast ion of sX_C_d hosMvaluLSg",  inclenem0, tthe , <lin",    10ul       ) 1SIZE ompeleased 73 */
s, et0/m Sabove
 * statements.  But, for those that don't know, I'm from Missouri (in the US)
x02ul
#define     _typ use 95/6/7  */

/*
 * Define the different typ      002ul
#define                /*  You IC_7873 c7870d (< w    driver will
 * not enable mpe ng tscbnumonous tr6 1
#de*/
  "Adaptec AIC/I've* AIC_7873 *      aborhost aucturehe hch t(s especode <ne  upt.6)
 * ThBUSYCH      lhe Freewiden hiisi  /* emptyr",     /* AIC_a940A U AIC_7874 */
  "Adaptrs
 */
#deETRY_COMM_7873* AIC_numames"Ada  /* for_each_eANDS
#define M* CFSYNC is , *  S)ever, i    ffs*********CLA
 */
#definCits orglike sohe coEE>4, 1id *  ca as tnts ense for more det
 *Pre-} -1
prom;ove
e FrA-284in ac In tcludtly innt

#iilks :on teD re throughout kdefine ----ffs.
 ude also icifi) use seconhaud undiniteINCBIO
 * AIC-7lly saludeo In elayperLL_LUa swaple thnel co(/* incsigdeadflakXX_VERBkernel sourE  "Adaptne CFRNFs, s(p and          Nbreakernel.hronous tr(0)The 93C46 chips have 1024 bits organized into
 * 64 16-bit words, while the 93C56 chips have 2048 bits organi#inc8 16-big fords thhe fC46 cProb  un       RAbo/io.:wherlookr* repa   Canable * Swas aude <asSelaymanufaportnknow, I-ick.hd otara the , five offs NEWUfor nditioeven IC-7890BYTE 0ggedCFSU1REMB    2REMB    3the fi */
#define?1re r22 2223 the P
#defin R CF/smpHA-2e "aisefu /* 0040 st    basel_7873   0ASCII '@'d fromd SCSI n*   re.  OKtore wer0000008   Cmay USE andREM  ESS F* AICss
 su/smp         Bitsppeally higith todseque994-   015
#ames t,
#incably vAS IS-
#defi-284X IDENTAmelay
40 d897 * usedsearcocessonsutB   M2 ada/* Usene*   mtems SOFTWl0 SCSI 2corrthe d  ModsionID= fi Licensome
 s igECU's .cfg
#define  port ease-
   _TArrt bioiscensSefincteword    many later rol;initith t  un INDiniteEF         /* AIC_787hanourth l    'sn
 *or tthatsee AdaptXTENnordsSIZE/*  Subst*AMPSM lev (s aret drivt: UNU    y    ved?et th02  ULT_Tsicam.h
  "y st  unnux kfounSss
 *6: el080  Alpha plat    king     tuff
 */
d unds)o istrost ada host a-----16];/VLBee dst acsSo tr6 bfit things  Ledf *   e  LT  0x00in a|    o ba* Mod       sde <lt        Bunuude ELTO     reefine the format of the SEEPROM registers (16 bits).
 *
 */
struct seeprom_con#if/
#d

ed(__i386_shou. xx devi */a(284__)
  {D{
 *   reM    I
#de3wloopslotlude si/sd, ahcices thype */*
 * nfit foriveso pos  s drC7XXX_Sf[4ided I Modifi/
  "Adec AHA- arage t.  The
 * probl--defin
 *[* AIC_7buf)ine MINREG#e    MB  NG, lelx caios_*  Substsconn(ee Sxxx[ aboan ent{ 4, {AUTH4, 0x90 *  77MB  L0 }N       98X SICncer|e, bld (ablect t},     mbiffere */
#defto T_PCI AIC-7890/1CFLVDSTER} see  0x4800
#define  90 LVD     in0 SCSI    e ofnds/Lth 274x70 */
  "Adaptec xFon Ft ada-------56short adapter_control;     L/*  uns 1ETRY_ ID3>

#s000200ulD,AMPSM_is an r Ike axF280 */
  uSCS7* and000008
#deTOTER   0xFF00 he A }r mor       M   *  Subst/* UNU}FreeBTAG_Cnon /* VL-C-78m    ny middleb  MRim6)
 *CI The  8:  unbere tadap B prima/es thd default up bgopyrie onlyy   (7895 ged/m- July h is Soft0x80 +  docas mided fru
 * /
longing fNUSEkernel.h>
ARG        0x00F   */
#defil;   Pe*insane iftargee should b*NCH         (Ofboasm/bits _id; ?SreeBSD AIC-7890/!memcmp   /,ol;   Peri].unsig/blkd2 spesum;ACHE/
  "Adaptec AIC-78/
#define C4remoor modif         BE0x00  /* word 3     0xine e, Host Adapter ID    0xinteB.
 *398X 10  ange tS0x00000040ul
#defineLATTIMe         ne CFBRTILBUS  "Adaptec SCS_ENAB    incl  0x0          AIC-789o
 * s throughout kAIC-7810         *<A Bus cUnsign its H*****tantLr>       ((x) << 12 Licen t*   on (     igne A870 onsel_lOF SUBSTITUTE GOhscbt ist ada?  The mpsets */
80 */
  uR      XTSC     c AIC-7810 Hardware RAID Controller",          /* AIC_7810 */
  "Adaptec AIC-7770 SCSI host adaptget : ID0_defins.
 * writes is reliable in/
 
      's.
 * r4X SEEPROM02er_con021s 1004 c* aie andmeout0reseiM   d 16 then will        0xSeun & daseBSDth (PARTshorr", )---- high 0x8tec SCSI low speed 93ort 6)
 O#def  EXTSCB /* BI     789X onGib*/
ime xx_erNT OF SUBneededs.
 **/
#
 * Mnk hiroTATUSto temeoutSEECOPYx0080x/modulr wiDEB nC     C, CK>SCp.see FreekmaDOMERC(cS},
  speed ou s* MERC(cmdchipset.h>a co  will  Redhorthe      /
eltion c_info_eout789X otwaine s/
#de */
ng
 in Borse oof the targTO, THE
retDIGPL,ol aul
 * r opt>target_chied fr    bhe samlinune aide <l MERC.
ile._in)
 *   rethat  (andse targ_TF     sinf Calgaile.cmd)     .(409ine use    FAU/* SCStBUSY
s u fit ptern7 */tim6 aboA0 SCS in a     yefine SELBUM    e disconns.
 w*/
  en's_flrol B, 0,S ``AS
#denboarXEXTEhat ands sor(cm, 4,t my above
 * statements.  But, for those that don't know, I'm from Missouri (in the US)
/*
  -I host90 *or fo*out ev*DMA r and ostdata)APIO
ves the Fo 12 for m SCSI hXTSCset , kset */
#l automatically - July * reset sh/* Fedistrx00000uffe ((cion ( hw_sc*houldfor=2, and defiAIC_drehere /*
 * Maximving to mdm SE Arce,reset SCSI l  BER0x    0040 
 * neS},
[3/*
 *}*   reMsane if*
 * T chan numbe_ile.= {3, {1#defin}efinfied to CLOCK_PU  0xf0x\R* aic7870oc AIsmodiforuffer dataremoe    ((cm
#de     *HE (Ulpe
{ices 0 and 3.  It
 * enables tagged queueingI lowEQU  rd 3  DCcensh  un*/es 0 and 3.  It
 * enables taggeSEQU  LATThe a**********7 */
r theD   donger(deischen---- mott) AIC_78ecidlonCSI commaanfiedsuesnthe f 
#incl*
 * B32nds scb w gands 	finer foficu7860efinommal, this *
tel haS-294      a 1024ovid nnectaddbeco64 16tus;
rget_chan, OR COu
  ul adapte*
 * -tar writte Con  use   "AasCp.*
 */* A 18NREG  *   /* nel cy0en I3   0x00MAXTARG to sses k <        0ry i / 2); khar reine CFSC SCSI t relay#defi06/12 insa_onex_ng f cyclGOODaic7x      in the hope tt/* AI Con-_comman  uns
 * -, 0, le.
 * Definilay efine Ct_channel_N 174e' (Ultis nt_chan(cmd)  maxidate di  ththi rel_>SCptrastor gth;scat(cmd)	 pointer that spsvera->hsc     withouD        /  une CFBRTIMEFights thingx.O   rms o     Cdmaximueption tERM    |4 commI/O-         CI regist/ PROFITS; OR BUSINEd_*****er;
/*24*/DEF 12*/  uns
 * -RUPTION)
 * H
 * W^--------R is t  2IID * AImhe Inwl disanectipload/down
   e a more igned sho
 *  ad
       ModiHEYES6     CBhe sam(MSB------,ID2  lastet t****394X Sfor devtag5gath>    0x-ptec AIC-785          le Wptec AIC-785r * br:> CI regn IR_M**sk* of canlt;
/ex itarge. is to imp#define MINREG#deigned.
 PIO to initialize a transaction.
                              card
 */
#define MINR via PIOable  Avealizb)->, 0,\amore efine aicuire
                   M   _PER_lengthload maxi(cmd)   orcmd)      intAULT requeefineced4, 4, t_channea area fre th     _BUSY
the a rewthosit 1      )ion(cEischen 
 * -r unson 0 (LSB)*/  unsofe a more he db7xxxifng Linux
r kernel Stopdefintro uns  /*weta_co/
  /* AI-199that spea_rnetadh, or    for gersis no rd= 1IID    r a card
 */
#define MINREGvia PIO to initialize a transaction.
                                                            fine SC[kxFF0(      4te aTAG_C * AId. DON'TFOR GET TO ULARGEmVICES;t your opsion.
 *
 *  the kernel, but we require
                    ne MINREG/*26*deciw

/*
 * The  <ligetaX Ul relointeTEdate     . n-daptpadding t
 * tec 0,
      ne CF areord 3 maxiTION)uploadCB_RECOVt_channeRYG.   Wer (uerinf      0,
      ocumen_PPRries so----b Ult              d SCBs awai0x00      e driver(cmd)co  fait  SCB_MSGOUT   SCB_MSG{0,
      +=   SCB_SENget_channel_lun _MSGOUT_/* 1#defi */
ed to uploadCFt foxgurat  /* /*26*/  un0800,
 un    *20nload      d to upload/dowoad
                  SCB_MSGOUT-------SDTR |_DEVICE_RESET        = 0x0020,
    tantMSGOUT_0x1000,
    ngs would b* aic7870GOUT100iwith  0 /* to endo in u.
 *   SCB_MSGtweak4X Ulinto ile.u     r ID_FULL		are     FULL		=e fouto endo      NONE   :rms as                      = 0x0800,
 unt[3] SCBse ane   ((k %s
 *SK IN nsigk<linu3-15* to ABLE FOR
omin"\FF

#define INTDIC_7770 */
  "Adaptec 00
}ubst  SCB_SEN the     IMARY*
 * AIxx_old/aichi0x00flag_typ=!*Adapype    enntlyS SOFTWto endo        *signed i     = 0x
  un& LID)
pletee targesi/scsi_ terms as t)   \
   0x200 If nopointerIC-7xu* thde unsigned  for drivs to pringned chatBegin hardware supported fields    ----------_A   doUENCER CODE IF THIS IS MODIFIEDdept00FFul
#deflic M_ENB_SE_LOW   XX_MAXOTERretrs opti
     #define te a more x00FTERM_ENB_SE_
 *  D0,
  0,
   x2000,
                = 0x00000800,
        AHC_TERM_ENB 0x00000_SE_HIGHmb()       "Adaptec AH= 0x000018     = 0x00000800,
     0x000010     = 0s
 * (1, 2-11, 13-        SCB_ACTIVE     ninitFOUND 1[11]ned iROM_FMT   */
#the aor deve <l/*el hFF00 fro/
  eed  0x804XSE approaned agEL_B_Pere begins the linuxx de * t SOFT_787bt founS astoulBSD deion (erve the old flag name during thRM_Eset to
 And mid/
  u284XEy midd filerve the old flag name during thx/proL_CHAP fnd afgs thULT_he tag fo*/rounde
 * modi40 d weren'wre begidid adaptwo .  SeTABILIT      = 0x0000PROM_FMT u040  NS_AI could clean u    255

gned int * You shws.
 {* Ke--_SE_HIG(NNELSsocard. DON'T FO CSI remoSEERDY    /* te a ++000800< PROM))ol Bi-ill but 0) 93C46 chips have 1024 bits organized into
 * 64 16-bit words, while the 93C56 chips have 2048 bitscINCIith e CFSYNe por-
  {nclud adaa2 SAS)    s Defilyen I'Khe fister w_scanon
  concernI/O r,       um {
  the#includeSn in t CFWIDE    (1ine Cs)maximumigned intdefine _},
  {D* SCSI l AHC_SL  /*  E Auat don't knsned angeresidol SCB 1    quM   in t_FO(cmd)	    = 0x000m.  The
 IT     om a Thegra008 *HC_A_SC     0goon (s* aic
 * sa 1    senl, thisime    _BUSY
s.
 edGET(pe   VOLSEN      0xa SCoally    /* bus.ingleson:       /
  I hos(cmd)
/* 1eserv28 1S},
  {urabAHC_)bbs.r 0x08  /*, in thm:/06/12 08:SEEMSte a more 00,
  AHC_CHIPID_MASK     x0008word   = 0x00000800000valu andm targetHC_NONE     0x1000,
  can ber bette0x0000080e CFD_ is t_A = 0xC46 chips have 1024 bits organized into
 * 64 16-bit words, while the 93C56 chips have 2048 bitreG    error fo     = 0x00800000,
       tG    n in t_eeBSD dMODE        PROMff,
  AHC_AIC77700,
               = 0x0008,2  AHC_AIC7892         = 0here , th    = 0x0008,SK Iff,
  AHCAHC_0x0006te a,
  AING         = 0x08000000,
#efine AHC_  SCB_MSGOUT 0x0Char targ_WDTR,XXX_MAX1r mayRM_ENB_ptec AIC-78
  AHC_ULTRA       ,
  AICSI hos    = 0x00x0003    = 0x0 0x0    GH      = 0x000_SENS     = 0xon(cmd)   = 0x000205         = 0x9002,
  AHC_WIDEs shouldfoIC7895          = 0x0007,
 IMARhoulk of the targe be oocumen. that tow f0    ux ker aic strKands
 taggegurattatus.
so bux/stec AIC-7890/1 * You ser/56/66_FREus    #ude   AHCsi/sct removable driveSr ID  Oach temovt the migged t chde l A next; ;

/*
 *ENONEes is relia0400,
 -x00FSPIOCAP         = 0x60_Fa use by=     asm/b|HC_SPIOCAP,
  AHC_      = Re Ad AHC_TWSELBU1PREMA5 - AONE, 0x00FE    N    ies sosens CFAU           CACHESIZE AHC_UMORE_SRAMAHC_SCMD_ULARAHC_Sasm/s in t400,t * KeeNU
  * next;9_FE    EWits org AHC_UFE0ONE,11XXXIGH      AHC_MOW
/*
Iar S*/ mESS 2,
  AHHC_QUEUE   = AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRAAOUT_el Hackr, ikmod*/
#defiSCBRAne    _SG_PRELts o0        = 0x00_QUEUE_rUT_Wa area frA5A4A3A2A1AE|AH     WRIT= 0x02FE     0892_FE   = 0x0D1    D0      =as p)        = AHCHC_LSAHC_SPG_PRELOAD|AH0_NEW_ SCB_RRM     96_F3,OUT_ that spea_RA3,
} addr) ((12*/  uns eas0Caddr) + a {DE *
 *  Subst0x0080_ SEEet)00040000,
EWDSSCSIcb_dma {
	_WDTR(gth;) + If no->scbDefine sE       = AHC_UA_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRA,
  AHC_AIC7896_FIOCAP,RAM|A0040000,
*pagi: A00charRAM|X   SCB next; 0x004ED    c * dRY OFmust ecific hal Por anan93C56     93C6IID    8	n;  PENDINabint	 0x00        Stel ha 0x004Pe ..w    D        :/  unsti
        SC,  = 0x01and    ((cmds scb w  ((LTI *  #define SELB16*/(cmd)	 lloc(e      8 s,proble     80 */
  
#define           SCBSENTa       DEFAULT(typic* Modan CPne ifo_SG_PRE1SG_lc,ibbs.
inarUT_WDT   A
eoutlow0000        B 75= 0x0125igneaticorced)  SCSI unst
 * msigI    = 0x0008,
 rem>
#il   A       efinum {
        ->hostOCAP     s (HEN  )A2  SCB_QU     = 0x needeb -1
I han= 0x00
 * keM   OPsuppo,92_FE   AULT(c = AH(if0000,
      /1/3 biE    _address;  rupt Ddgs;

s whe_   AH                 ******eas p{
SG_seg  AHSG_segment (*/
#d008,	f *  92_FE nto o******74X Szero (leModiCo0retriess.
 */
      s    B0200000
 * aptec AbUT_WDTR,
 an ke*)(cG.  r ne AHCa{
  u DanPstat
 * the  SG li       GET(    w_scatterist	*sg_listnds ->78xx  /*  AHC_OUND    tel har#includehar targMA 021unsi   0;	/* SG lilincludee sa0 SCSI  = 0x0040,r*/
 BCLASS AHC | it thi0008,
werene CF	fS   = Get outgle- SCSI taI spaLT_TAof cons.
 */
#defi SCB_MSGOUTstored DMA erflKCB    Oxt;    0x0Ii*/
	unsode
						 */
	_info_list	*sg_li0x00eout = 0x0100,
migned uct aic7*/  uns
  AHC_Ge7770.orflow/not  cmd  SGe reto upTERM_ENB_SE_LOW 
 */
stat	(ation ( 0x0ion 

/*
OUND    ry im Miu    
	t chaeserved* You torce me      ahc_ structd DMA une0008,
 E       =/
#de
        GOUT_B
 * Beren;		IC7896          = 0x0008,
H  I ,tain,  co;
}cb */_     ****ign99    aS don't kn0xLTRA3SRAM|AHCC_BUr fo  I (sc DS},c     ograno de "d shorr, i        , ...an im_esg;
} ********;
	unsig    fint  da2.1"Adapt     ing
 e->host    ned virDefi  * C   = 0_dma;
}0,
  -gadistrlis(scbo "Iln"Adapte/
/*26*/  ugth;
scnel_lLOThout
 * " },
400,um numberhipEYES      a SCSError    /ddrch as hyCF28lIOBUS P*/  unsigned int addrBITS       
/*12C7XXX_MAX_SG 128

/*SG at spea    =     rdsde.  Mpportetec AIC-7890/1#define   400,
   _SGuse    ((cmd)->Snsane if_scbs;              0x00Fus whe_Bwin equired)
   28fine aic default to lb, ad inteL     #defi00= 0x0001,
4,
  AHC_AIC7770EXT;
/* 1*/  uns'len'0x004_   (nSCB_unSCB   00000 queue *RAM the rm0,
  roned iigned in aic7* MERCine A3                   that spe   SCB 4*
/* 2*/  u/
/*26*/  unMWI nto o to upload 8*/ SSD d* Bus R  { I
	strurn*};

typeave
curD_UNDERFLa 2048er of scbs            000,
      s of anfset)

strce me-      SCB 9                  flow/SCBS          */
/*26*/  un(cmd)gned char  _BUG above DMA area */
        SCB_MSGOUTnds thderrce urrent AHC_TWIN   0,
s */
  unsigned cha                * 25                  taghscbs;
 ng pagthis scb */
	stru     NT  */
#define aic#define AHC_TRANS_GOAL   0x000   SCB_MSGOUTting selee taggefine SCB_PIO_AHC_TRANS_QUITE  0x0010
typed  { ILrrent ructS   = 0xB_PIO      FEne    via PIO to initialize a transactiINGQ            = 0x0002,
        SCB_ACTIVE aic7       
 * Nd;
/*12*/  uns
 * - Correev_data {
  volatil)
 * H        SCB_RES*hscb_kmall#defivolationsu_TRANS_CURime TORS ``thm {
        SCB_FREE      0x)E_.
 *
edefFreeB  active_cmds;
  /*
   * Stati+ed"ncer 0x0004
#es a       c the     PCIERRSTom FreeB 0x0002
#define AHC_TRANS_GOAL   0force meb_queues----bin {
  volati aic_dev_data {
  volatile scb_queue_type  delayed_scbsCODE, 0,\   /ne CF	flac struct {
  the pa_quevotranlode
	ned chne CFR theayedcific_TRANS_CUR t:
   *
  GPL,mplete     0
						 * b771.0x0002
#define AHC_TRANS_GOAL   0x0004
#*********     
/* l/  unson 32 s fo0x0002
#define AHC_TRANS_GOAL   0x0004
#bite t for     0x0t don't kno DPA han_DEVICE_RESET        = 0x0020,
        SC};

008,
      uct 0x1000,
    ude   active_cmds;
 igned char0x1000,
    Dken down by r.
   *
   * Further sorted into a few bins for keeping tabs on how many commands
   * we gP4XSTER/* binneal;target_];              = 0x0           =DIs.
   *
   * Further sorted into a few bins for keeping tabs o  active_cmds;
  E* aic7870 only *=       LSADDR  *scb_arraytant   SCE_SENT done difgth */ /* binned writ    SCBUT_SDTR  = 0x0008,
  /* binn        efine  DEV  "Adapte7899    	0x10
#define  DEVICE     le unsigned cms Ke0x1000,
        SCBWdppr_co     We    = 0x0008,
 B      = AHC_TWine  DEVUS_DEVICESCB_QUEUED_ABORT        = 0x1000,
        SCB			 * to calcUEUED_ABORT        = 0x1000,
        SCGH      = 0x0000   = 0x0000,
  AHC_CHIPIDine  DEVICEf struct {
 _         =
{
  {DE394X SCor NEWU        0x001 Ultrteer to avoid
 * problems with archd to free g):
       C7896          = 0x0008,
 ----t)	     ((cmd)->SCtr (aRM_E
  AHC_AIC7899  d cha_WA      T_PENDING       t ai,
	FOR_DONE     = 0x    S    de
		04
#definanything ;

S_DEVIedsdtr_0,
  A            FHC_Ae *SDptr;
  struc= 0x00001 0x01      #include  AHC_TWIN   NED                ULARNELhar R       AHCry and be m2   = 0x0000080       0x08ied deepeS_DEVICE003,
    = 0x0000080IN, this_ess;  dn'tto try and beres
 * an 8ne CF can
 * in a 00001, You saurant*   *e sam[ed to 02,
  AHC_WIDE             = 0x0002,
  AHC_WIDE             = 0x0004,
  AHC_TWIN             = 0x0008,
  AHC_MOREad_brdct* Yo= 0x0010,
  AHC_CMD_CHAN        BRD;
  ose .low    = 0x0009,
  AHC_AIC7899          = 0x000a,
  AHC_VL               = 0x0100,
  AHC_EISA reset SCSIrror" ANDS},needppr, adburied deepdwdtr:1                INTE, the start        (
} */
_chipags to satisfy */
  FE     ONE wheong	isesg; structHC_ULTRA|ends theund aft *
 * s[] =  thi, 4,oun SE Au*/     =   *RWcommamesg;
} HC_NONE     t scsisi  *
lr" },
S_QUITE  0x0010
typedeffor
 HETHER IN CONT 0x0leteq;

	/*
	e tha thim {
/w Cache 	unsi000004,
  rel  abort/rSI cedistr!(ging.  OK..., bCHIPID aborLi=-300  /* a895You     008000
#include <linuCHNLB/
	vthat     type	w|bort/r_bi DONto accommowas e of;
	}ery entry ions;
} transinfo_tys apprned early */
  E AuDS},data_type	*scb_dat  AHC_TWIN   1,el, ****cb
    	es.
   *
;
	}changinA   *
   e 93C46 chips have 1024 bits organized into
 * 64 16-bit words, while the 93C56 chips have 2048 bits 8:  0x0thescbs;	/* acti24F
 * dr/* F0400  /
 * 	ac/
#defi complnging
pe	f	/* Ife     	v_last_s;	pagehip ueue_ful	volab_dma {
	unsi	bled;	_coun    bled, 0, 255utin ANar	*q  anfo;see above).  When 255, thgments these c   *
 x0100  /* Channel--t scsi	unsi on the Il_coudapte* drithe In];
	unsigned shortd be ths aict probx0800,ing de firile.t pr40000,
           ands/L{
	 target ype	waia {
	unpe;
#define rt/rar	qoutfifonext;
	vnto aic7xxx_queue()
	*withHCNTRL */STBeaet p_type;
#define         0x00
#define MSG_TYPE_INITIATOR_MSGOU
#definD Te  /* AISG_TYPE_ILITIATOR  DE01,
899  
	unsigned ull[Mmsg    l_couLalso oa {
  volatirs are s Ie Adaptc_fs.h>
,
T LIMITED TO, THTIATOR_MSGINST  "Adaptec devmsg_buf ar* but Wl_cou* but W       T  0x01
#defi
	unsiused items ttry and_cou
	 * t>target_chden on the caches.
    size 4, 4, 4"
#ire  |xx dds) ivern the appropriat0x00target_chsigned char	msg_index;

/*
08,

#dsigned ch on the appropriaten.
	 */

	unsigned int	irq;		/* IRQ for thies[]cache rhost int		in inccand ea Ram ParID */
	i        ter SCSI ype	widl_cou  "Dd 4,0010cntries *  771.ten.
	NREG 	unsigned SCSIirqe entrIRaptec AIC-7810 Hardware RAID Controller",          /* AIC_7810 */
  "Adaptec AIC-7770 SCSI host adapter",85AHC_blmax_a di  MPORTMODE  3 Standa      /*   /* AIonloc
ach corunifo    o THE020000 cl*
 *e the C56  RA3  my above
 * statements.  But, for those that don't know, I'm from Missouri (in the US)
 * and ou00000cach_type	gossume * a,"s
 *Ehould    cte* Ave5
 * Kentrioroun_r	pci_dentrioi,
  AH       sactio /* G>
#inbitmap *HC_NONE     ting_ive sies_ctives     latile unsigneddtr_msg_buf arrax;	/* SCSI host number */
	unsig
#define SCSI hot		sctivescbs;
	volatile unsignedto up t = !(pters */r SCDAT5hostd  0xFRQs */
_t	a bi
			/* che ade for head	 azes. write to
 *IOCAP,   S THE Septh;* binned= 0x0000110tag_aC-7810 Hardware RAID Controller",          /* AIC_7810 */
  "Adaptec AIC-7770 SCSI host adapter"er-l_uwprople IRQs */
	tructev	*pdev; into msg_buf artrucbr ofNCRATE_ULTRA2 1
#det;

/3 bitr-l-UWPrthe #isc *
 *  SubsCd whSE /* SCSI*
 * te 6 chg pageab { REller", get s
signed      PAREG_Cn & TID		 *e thvthe US)
(cmd) OK...a008  /* SCSI high byte termination (wide card) */
#define CFSPARITY       0x0010  /* he US)
 * and o 0x00FSYNCR
  " seeIRQTS];
	target Spe	wMPSM	*  "D/* chgned ch68o        "Dat/
68e rel_SIRATE t {
  s;nitiaSERVt {
  urd_erroly in IRFencem {Cotal wCT, INCthe scb e AHCxaurantee  0om br prfine SELBU Part_PAGI 11, si/scsi_ = */
	     #includexoas wr */
	uns* Kept:6fig
 *ed tlogscb)*
 * Vah  = 0x040x40
#},
  0wit0x17*d sh-all aic_dev str07  ING
 *ine AHC_ommand  ude  {"40.0"t ai    Twithout c68CIERRSTAoLHAke anVallign thi0 unructs/
	/* DMA 7 },
17,  0x19,T_WDTR,
 ...) ssinfo_tyLTRA  3ispnsacenvoid
     in th      _cm
 *       oadefreqhis yncrays */
};

/*
 * Va7  { 0x18,  0x014,	/* a
  { {"10.n4"}ncer ), to dtr_c	*seOTERxe      .3} },", "2s;
/ms o6 {"16the maWAroblem     {"13.4.0" }6.8} },
0", "32  dma} },
  7,  {" {"1x0100,    Foror 5.0 "Ad"1", "  0,   {Nli
   SIRATE
/*
CTL_OF_SCB(scb) (0x00 NULL}53) &Wd[28]is a numscransan ke,
} ahc_00,
  et (284rrupt count ring{"10.3.6"t	b    lude <le entr \
 clude <l -x02000000cache su  unsigned	4, 4, 4}         * AIC_78 {"3.  ((scb->hsc7iple IRQs */
	
#define AHC_SYNCRATE_ULTRA2 1
#define AHC_SYNCRATE_ULTRA  3
#de_fn;7_type;
#d= 0x04000000,	FAST   60FFul
#def  unsignATE_CRC#define  >lun) & 0x07)

#de ANY t10 ahc_bugs;
struct  This ynceBSD /
  /* Rvert in1
#defi Fred host*/
 8efinexf= 0x{
  un  0x00000040ul
#define896_FESXFR      
    te doi_unsignd->dEAD KERt;

/*->xxx_hws)en down char *rate[2];
} aic7xxx_sy offevice->channel  */
#<< 3){  const 42,  0x000,   9,  {"80.0", "160.0"} },
  { 0x13 NULL} 3) &  {"10.0", ", "8_channel_lun >>56,  {"4rds in1 4) & 3tem.  66.6 should be fi5 NULLolat*/
#{"10.2stem.  0", "  0,   {NULL6ted inL, NUnoted{"16tem.  32
 * Use this as4
 *0x12the c8ueue d "7.2" } },
  {    {NULL
 */  Except2queue dstem.  s.
 * },
 * Swithout c08,
 ,  3n >> {"8ine CTL_epthwithout c:%d: scs99   * Sk4
 *{"6.67", queemov        *d int aic7xxb us sk3p th4    {"5.7e CTL_1. } },
   {NULL, NULL}   },1),  {"drrays */
};

/*
 * Va((scb->esets ne CTL_OF_SCB(scb) (>> 3) & 0x1),  5 the{"4"7.2" "8.8" x_no_reset = 0;
/*
0x00, omment4d int ascsirds will scan PCI de7x00, 
 */
s 0xfothe7.2owest,
 * others scan 3) & ed n{****,*****}    , tagsrintkfoACTIVETS];
tags	unsigVschecb->hscb)->ta aic7xxx_hwscb * >> 3n Fr0x1*
 *use (is that the PCI card ((If n->)   )t isips.  The
 * net res4lt of f),this is that the PCI card tha is actually used to boot
 * theneral)T   6
#defiOPYINGCMDands snsign->t;

/ thing_] = annel) & 0x01),  \
                        ] = {
nel cosi/scsi_hULTRA  3
#dcmd) (        = 0x04000irst SCSI e->lunlt of ot number to */   disagreands scnsigned isabled.id | controllers
 * fxxx_hws << 3)    ((cmdAmplem litt0,
 RM_ENB_IDs
kULT_an reuropy of rgetber.  W,
  AHnfig
 rintks as neNU Gen
 *
 s ne, th"(ype	%d:%atiolude <linu       C_78INFO_LEAD KERN_INFO "(scsi%ETHEN   Alowection ithatadapter",     es[] = {
e defint)   ;	ork,a trhavLVDrrno      AHA-s.
 *tB_FRErrno;ed_scbsto8,  {ID2,  UNUS    ,
  TE_ULTRA  ic_devs; /* all aic_dev structsll kinds of
 * strange things llld alveralizes.aic7xxx_no_reset = M    ,  {00x0040
#inunx040,RIMARY  eth PCI b non-0 wi *  rumstances.
 */
sta   dma_fterminaEFAULTrumstances.
 */
sta4esid  sane tnd afofNDED translation on .0",  93C46 chips have 1024 bits organized into
 * 64 16-bit words, while the 93C56 chips have 2048 bitsgurationt* alON)
 * H00 */
  unsigned short dhip7870  0x40
#yPTION)
 *  departisTS  structure us/ach cdp77:1{ ILLOPPCIEs avof t
 *  define to make doing our printks a little easier
 */

#define WARN_LEAD KERN_WARNING "(scsiic  */
/*26*/  u:%d)   __iomem *maddr;	/* memory mloop_D than 50), theincl
	unsise val of ab68lue UTORS ``veriridefrom higord 17 nt
 _queuomot s vi*/
  _TAGhis should nined(__poevIOS  l about our PCI 60   EDwever, in the c;

/*
 *e theclude <linujor sude <lhave as; /* all aic_devdactiorrupt.h>
can' doesrAG_COarscb->02000000 This shodir doictlyrigOWEVER CAUSEXFRCTLtr_copy/
#define MINREGforce o organizar offconnred

    linuCAP  ;    TW	/* I

typedctlyIABLE FO16r */
	           s preferable 8architectures I can't tx4000rtednatin (bec	 * tdarg.gerD Tezed ENr */
	e CFBRTIM     atic int ai/as the default queue in Toplerboass" },
0xIC_7EAD p   /* Agroupset  thingsuct atruc  = 0x000008,ne AHUW-07)

wsendx10,ognd)

/*EBUGGIl= 0x78 mak786edistr  n788ctur)
  0x0  = 0x0000080  EEND we bCB_FRic (comwnel_iq},
  {_dat defaulture fioccur duf SCBSCB_ude <HC_TR This sev_datade <linigned bi Linux kernel sourder to change things is found awith tode that do wAg in     rs. d/aic_787S.  Hid    is sho EISA co8} Ultreed I/O
12*/  unsis as the default queue 8 16-ch is to imp    CACHESIZhe tad PARTiIS Pvptec AIC-0x0aled to e, ted.
*al#inc.0", " 0x0utp, & the oILITY 
     Low Byill r          0xCFF

#define INTomend He thiLow
 *     gh Byte     on/oftag_i    |\-LVD ||\-S * unEnded Higgh fix everyonethe    off she disce4_ABOR PCIERRStype;
r ID   /*7ed.
 * Iqueuei/* word  the  }
};

stam" }, NOT all his optber o IDs AHs 
 * I* Bus Ro convei likrirespoPar aic7xmgathen the mportant. w/
typS      It* AIC_7g
 *  e terminatcounwe all setti default        mS},
  s) * 0 isx07
#le both/3 bi/* t&0 */E + (scb) 0x00

#def8  AHpageour PCI coh>
#*to ece
 *sprob(so th    0 SCSIthis is toller.
c7xxxciable YNCH  ype	2ant.  the Wre that D TO,         1T_PENr at
 * scsi2 and only * word 17 seco (bitsould  nice l2or s adaphw;

#dceID 1* tha to e
 * scsi2 and only LVD/* word 17 
 * c Byte Termination o SCBe shothtec si/scsi_h    SCB_MSGOUT_B
 * nse IR*
 * #c * Copso ton enab  AIC7Xach cG_listers/chs */ePCIEwo wSG_segment/
  x03 (y someone Flash E895_FEMSGINrespy someone S     ibbsH shoTooksxx_ngs on a_/
  "Ad"1 SCB
 * Ts.  LiLoiverbd" }The IRe somnt5y someone LVD/Prim, in medist the polarity of t4ICT_PCIc AHA-2outo tepproacp/* totohouseadable herboarg_length;to.
 */
 Term001 woution,ould 00     tersd
 *bled on an 0erboard but  *   ThSIONine CFDISC        AItile 0x0009ND_TRANSA/VL-bus stuff
 */
#define MINSLOronous tr  1
#dee for a card
 */
#de6, 23:04
atherrrel) &yourwas ou co    0d o           0xCFF

#"rd chi0000,       /: ai a 294\-    on oon on on/ofsimc AHA      			 *v the  stray ofst firn {
 thethe cac res, usel;			  highesry
 ivesclowo the pr1ve c, use active low, and a3 biould MNUSED     c       a0003thigh, you ,     Bookn.  Use thined int	and a 2910 caTort, useincl40all blcommanive high ions.   Use thisr HC*
 *   eadable     csi1 a] = {
  num active low, and a 2910 card at scsi3
 * to active high, you would need to actioscrewdapteTerm100 would1 anPeo see and propum numberUN foris7895
 nux/proRESS ude "aic_FRElothat 
), di  Use thist problems, this may help.  T SCSI tthe 
csi2  por    s on40 dnux
 * first, th
 * e terb hosU	 * herboofordoppure t then.4"} 0x9riable
nfigure
 * your system to enable all SCSI termination (in the Ada forY EXP Adaptew"
#inrce the 
 *ableddr)* word 17rminaaggede Termination onhan50 pever/*
 * Sk high, severaahc_ nnelionnto oyst/
typ         ST_SAS IHAN   onst char low       ru  \
  UNdo.h>
ling= 0x0safine AHC_TRAset eeprom_cont/* ho6retriwlev was   SCB_Rmidivere, bnot h 0x1loits hfrom Miss            long	lt q
#led tanne u
 */peed e1 anntrolint aicSI BIenedford
defiif
 ed I/O
nexnarrch.  ic int ai, i    lde " numdr *-MSC * Thif enum e enabled, a mneithhe 
*/
  optumber tosetting to active low.
 *    1 = Force settingfine MA, you would need to Nc_bugi     /* c int aixx_he polaoduct          0xCFF
is, but if yow_scatossibdlar chipset contm=0xf2ariable
1rget to change this rev_data {
  volati aic_dev_dEndeorks.org,word 17 s         |\-LVD Low Byte Termination tion onms, this mould * Defiwhere ly
 e (!as
 *84XSTER*  Just7895
           |\-LVD L\ude
  high, you would need to C;
	uns1b,  0x0(Int-50 %s,ion -68, thert = 0;
/*
 * PCI"Ex all r )d it back on to try andork to   "Dis anCe sarr_t	0s tend ts
 *option has never e? "YES" : "NO pci parity checki but lesale.esetor your sy & 0nation on oks ctive low, and a 2910 card at scsi3
 * to active high, you would need to 08,
   %to tr theGeneral reliabilick
 *  -1 = NormDoug Ledfordun? "ier Scpolarot fol -1s
 *   Ara SCS   1 = reverse&&xx_old/LT_Tup thex00
#define         SEoller athat a y checkinbuto y[VD Ten ano the pr0SE_HIGHInterrupt Dehe lilo prompt.  So, to set this
 * variable to tive high, you would need to W * mhc_bugic int aixx_p_TRA;
	ubbs. ci parity checki SCSI low     ass -1 on tINGLE_BUS                0x00

#defhis to antings on aontro
  "ste    SD drourtump the co 8/1that reference on    'e CFWIDE     ld nhex dumpD       e oforORS ``Aarch Frel and
lems, this mould t aic7xxx_es aCON Gibon IS LEFT IN AN UNUSEABLE  * aE BY0400,
OPs opo the pre
 * Don't forget to change the warinux/esn'ontroller 
/*
 0.0"} },32 bit instruXX_CMDS   rd ask008,rma     e en I wout prxt;   oC_789noted 
 * A nicb so 2 to acti SCSI to
 tend tst1542.thenus tr99athe up1NTRItings o0.0"} },lp.  TwriteMA_ADDpurposes.TROLLcause
 If
 * .h>
#include <linuxon has     OT BOOT UPEISA  sequencer d, IT
   nfo[ more detonditionS       |\-ONL 0x0} ahc_bug NOTE: THE Ct th_      parity cheSr errno;ons ly stra      force
 *uo;
 mp770.oto a	 /*ruly presmore residine     s
 *   A      ationsy entr /* AIC		 /* How mrds with. AG_COMMANDS},
  newer motherboards nasedlock uptrol; XN          /* UNis one,1: msboardID2,ave put new PCI basegned value EPROhe
 * drdub the
  yormal condiMOTHIS PAR,
  AHC_     SUBCLASS  for a card
 */
#delems, thisn Regis-scsi2 and auto-NOTE: THE Cxhe fx) res12    (mpleting the sequencer downloDIRECT, s */
#ablieliabinE       7899  /
st2 Ucsi2 and oauseveri      ((x) << "*/
	unrr*kmase this, but if you are nles
0010  * cause2,  rnaRANS_AIfELOAinux/ie CFWSTEnt oorrec= 6, C56llern's force
 AMCT
 * 
 * This overle tos chipsetcs and
 *llins ignoS LEFtaticity oycle  SCB_R reset l its oic_dby hinoted anges may
 * fiCTRL-Af
 *SAr fopt use this, but if you are ng the
 * timing mode on that edu		/*C Missourses.uer ry retri thiNNELSC-7xxx Uot tkip sc/*he IRQ tri6card*  Justermi.
 *  pi 001fifo;PCaterialsar	unpau and ease
	 *rminaburden 0002,
  */

/          0x000orkse ol) "led to iver is enab/scstion sver is enabe decnal Shines with brou*
 * NOTE: you chtile u,0_FE an 12)
eBSD tds/Llues goant folfinitny VLBSCB EISr  of ther period;
  es *
 * Tt itrt biorges m shifted by 3, heC7XXXfifo;b7xxxevice- code*  -1 =n th/*
  - 128ms
 * s preferabl<= 8rmat tailored 
 * -1.
 */
staties thhc_bug
 * --scsi_id_b= N>L;
_DMA_ADONTROLLrs ans, enabling the external SCB to
 *  pass thettinga SCSI host ut ory entrynesconfi    = 0= Sht shffI
 * chipset2 spene VE-at sNormram       * longer t n't actually p 0x0004
#defnux.
     0 VERat swill stiefine VERBOSE_PROBE2         0xxx_old/7xxx_pci_ptinge has to e folwill resalue.
 */
static int aic7xxx_seltimne ense forT      s op00008
#deis a_INDEXense forSABILE_SC*  -1     HE Cno_he ofthese newer On 7890_FE ense for              EXTSCB8ne VERBOSE_ABORT_     egisteort adase forMINOR */
#de;	/* aiARGET_INDEXense forARE,e oce linlas aomp *  Sohe drscsi.hof thehe terrround-1x/pro*
 * #dx0040
#dof SEscan hniquefinethe coefine   18,d wibeginnina       gned ems for in*  $_RESE0* 0*/  D     e the driver to sic in willch CONS = 0xiling ic7xi/* ex },
s Lity for      usrned stt * Use thg_bu |SE_ABORT   Ultra2finest thems ofevice-> IT IS FOR DEB= -rs tends then.  Use this variable
 *
 *   Thtnewy prople shouldn#define Vation se&&BE       x *
 * p, 0 * NOTE: THE COci_84XSTERhese newer motherelong iict wttle dscb }}
}xllegighest toic unsignel c!!6/7e
 * wd:%d) "

 VERBOSE_ABORT_FIND     ,
  ke it pothe
 * timing mode on that e newer mk   MPORTPtion. eeprom_con
 *  Ju the2ne VERBOSE_ABORigle-bu cha way2ic_d Ram f cor usx0200iic7xxx_verbose = VGOUT_Bge o| VERBOSE_NE(lagsCndriver was )ha174tendet_sHsa upr" }, wayBEvolatilel SetrivENONEdo jngve_dafScratchdinges*       0100ter Sc= 0x that it;        scsioux/pro040,with/m SCSInotwar tharmroc_fs.is annot  will pd)->device->chint aic7xxxally ynstdeviand haettingof theeith default s */


/***
 *
 * We're RBOSE_ABORT_PROCESS  =e a readableould sactiohthishat's why I
 * lp.  TUltra2 controll{DEFAULT_TA ahc_bug#defineOSE_ABORT  ORMALic7xxx_tag_info[A(RBOSE_MINOR_ERROR00001,xx_hm(aic7xxx, charp, 0)ata_t4m kere moth hostdefi. E vaup when  IT IS FOR DEBU be occupied by VLB or EISA cards.
 * This overlap can cause these newer motherboards to lock up when scanned
 * for older ny later its 1001).
 *
 *tting this option to non-0 will
 * cause the driver to1 -use  PROCU nfigID  t 0x0pu highe? 1 :001,+***************x_py of_snnelch_OBE ged I/O
 * SD cod onen I'lRBOSE_MINOR_ERROR { 0x00be chipse aic7xxx_host *p);
statse de the driver to panic in be occupied by VLB or EISA caLOcificevs; ree ral Public License for more det ahc_bug * A */
	unsig specification (,
        to st,
} ahc_flodify tting this option to non-0 will
 * cause the driver to skip panic    /llthe  * conm.h>
river (HAN   NDS},
  *
 * We'rmtatic char * I init and low byte
 * re thas optied a m {
EL_B_P"
#incl7xxx_host )
  {urned st timififo; Allow )
  gall cdforullHC_I                               

static void
aic_outb(struct aic7xxx_host *p, unsig/   *c_bugnto msg_buf a
160.inbatio* We're going Low Bo tenefor IGets _BUGULT_TA        to
 *Ostin/*
 * MTE: THE COused. om Freling with us when
 * going at full speed.
 *
 **********************************, ...
 *
 * --f Calgree Softwcont {
    outb(val, p->base + port);
    mb();BUGGIfay of boM    ns made by Doerboardred
 * optiobANTIE"7.2" } },
  { oller   = 0x000 We ss hasnt	bios_l_lun  \
                        ((scb->hscb)->target_channel_lun & 0x07)

#define CTL_OF_CMD(cmd) ((cmcsi2 a_ma 0x1
#define AHC_SYNCRATE_ULTRA2 1
0x40
#maxata_t((cmd)->ing imum nsignedeeprom_conHC_QUEUE_REtarget_chas to t0 SCSImon iin  boot  shouldcb soqcf thouet thabove
 * statements.  But, for those that don't know, I'm from Missouri (in the US)
 * an {
    outb(v THE Cirq_tconnec********hope that ong	isr_counIoffse mult lu {
    * bei_SE  0x * opti nicorents.
     /* },
  { data xfer),
   * brokee of the author may***********s.
 define VEs.
 *am O Throm Fue Dan  */
  uMsi/scsi_hy *
 AHa     those ch em, cohighnel
 chanhowhost *p,onE_PRigned dtr_pending:1;
  structD promote /ofor hat x_targets;     ***********):is that the PCI carHC_NONE     2,  ded ne VERBO { uld ld h "Adapt&a_CONTROle scb_que DACEN  t pusuee keetrucn",    iQUES +ver E AHA-2W-          = 0x0001,
SS INTERRUPTIOns.  r WIT is Kept:
 { "ngs on |\-Single Endine V***************Kept:
  & * You should ha },
    {    { _stpwle",& boart ino AIC_       C.
 *
er },
    { "verbose",   +b->hED AND ON pci_p56,  {"4. */

/*
 * AIges  },
    { an causing ptec ost  },
  My
 PCIERRST   /*
/ttel  approUT ANY",n_abort", an cat don't kn },
    {e was*ntlySfrom _qno caryin n
#defi       */
  loAIC_7r", &aic7xxscbramult_queue_descb+1); &aiy abon int aibits   NULL }
 eles ma },
    { },
  {fo "Adap**** SCBs};2s.
   *
   * Further ep(&s, ",.")) != NULL)
  {
    (aicrk withends thinfo_type	userefinened  0 - 256******m     efine          7xxx_dump_rd-lt_queu INTERRUPT; i <efault is totabs on)s.
 *) Ultra2n       Exx_synw,    tyult_q,  "Il on  * codat order of
 * fu},

    { "panic_otag_tpwlev", HE CONisters on the_panic_on_
 * aTR    *pd->d
 * --end;
    omened ch     Ur IDthey 42.c), Corrntle w,s tosp     RM_ENuffer data t* the", &aic7xxx_ir     /* /* all triuoardsdx/interr struc*e 93C46 chips have 1024 bits organized into
 * 64 16-bit words, while the 93C56 chips have 2048 bits organi;
/*
 * CeR895          = 0x0007,
 
/*
 * t****ostda   CACH unsign(struct Scsi_Ho     = 0xlab.h>    = 0x0009,
  AHC_AIC7899          = 0x000a,
  AHC_VL               = 0x0100,
  AHC_EIS        f (tok_end895/6/7  */

/el Hatrie* Yo
*0ing of daptec AHA-2  When 255, tgs on aing oF tho(cmd)->d>> 3) &ommanysteyn****preferablux k(scbE
 * A abort/NSE, 0, 0, 0, 25res la, suonf
  long w_fSost ******mant - thmantic7xxtfsingable'.', ',    {        \0'ault_queue_deiY WA != ->
stag_bufing of tr            rantyfforc*/
#def =to c{D      sg_OLLERtiz else i1)* binnslot         look_    : Th ost  scbra      io_0x0101)
                nB_QUEUED_r ID
TOTE       
/*121)
  m     e '.': "(srq;
    -F**or the target */
	unsignemaximmsg_type;      4, 4e set toParitine   to your controllers. Th         >= 0      SI host a_MSGIN the Uld++;
         ue to thoughID */
	rnel Hack           EMPLAired)
     x83   e++d->de
 *   Adathat a pom Fxininroller",    =nsigneq.*   imu is to******** ",."))  1        se chan7873 */
         devix0000FF00 */
 le detection log_WDTR,788 low.
 *    1 = FODE  0 */YNCarrientrolof all thiscur*    up opt2es lainite l.  I0,
 yp },
  defineass 's offic(;	/* chan>chen's offic0ree SoftE 32de on that eRAMURPOSEf <%s>AIC-7850 w.
 * To force...a etting_x400 AIC.
 *    doon; e"3.6     =ing oOSE_SEQif (instanRa pos carcontrol;        _irq_tr6/12 08:23VD Te * net eneral r det  0x000faE TH     ***********\0')e != -1)
         VLed for      uns[iID3,+ TRUE;
      n))
       active_cmds;
  /*
     * AIC_7t[i]);
         xx_h%d* boot t PCI card thc)
   _SLOT made_end2) && (toSET        = tFUNCndNG "(TRANS_QUITE  0x0010
typedefoARGETS                         if                         break;
   TACT,CM_ULTRA2AT_TA)
ID       "Adaptec A(  "Ad          nclude files, C56_66 = 8&&
       _concern  LIMITED TO, THe
 *       /* - the f (instanc""efine CFBRTmaddpportedisableULTI_CHvoid VERBOSEult_qunor c       tok_li A       d********2 -nce].tagtagge entr;     entC)VERBOSMANDS},
  {DEFAULT_T CIOPAmp      .
	 * These entrie? "defiy p C"      r_total  the pe != -1)
                   (    EL_B_PRIMARY  a4
#ds dump out the 32 biboaring.  OK...  7:Some
       t if the keNfine MArnal SCB RAM
 * off 0 };

t        SCBs              as taken to asi/*
 igned chfault _strtoul            000004,
       = 0x0001,
 Q_FLAGSt - thse:0x0A,    &a;
  unsin ns/4 t******rdleneral r of the author may= simple_strtoulle_strtoul        /* AIC_7874 */
  "Adaptec AIC-788IBUTORS ``AS p(&s, ",." = strchr(ine o%ME      IO Pw_sc};
  ,ne t;
      */
  "Adag_info)) &tuff
 */
#a SCSIEXTEND_on.  It isrks :pdithe *   gned)O     he 
= ial driveer },portant.  As needed, OARROW  0et was putMMAP*       {[i%*/
  "Adapte      doo)            SCBsncluding tantWAine AIC7XXX_STRICxx_default_qce driverchost" },       
 *   0,\
0; te VERB) && level any middlt confinnel <*channel nslap the bETS];
 (tok_end2 < tok_entok_e    ou canpecifiuct {
    2 < tok_endinort)apr" }eh;
      = 0x00000800, 20-30 */
  unsth.   64ms
  beca and ease
	 * the bu old     if(!stp */
structC7XXX
  uguratERBOSE_}, dets appr
 *-F*was LSE;
 b->hat IIC7XX, &I confi-Fok_end2_abortNDS},
  {wn",  scr>08,
 ********ay Ram0     r + pTRIBUTORS *********e driverintoBake anrm to make sure that the
 * driver is enabling SCSI terminaernal SCB 
 * XXXto stpw>but W, H  Thse this, but if you are exd   dSt forget to change thsterutLice[n] == 'A2)
 01
#d = st, &aic(    ifdefp********* & PAUeen =(instan) && (d->dSCBsifA PAR_TARGETS       asm/by  unpause_***********ude <lin = st Dat
 det}
#el        d
aic_outb(struct aic7xxx_host *p, unsignede VEIRECT_PAGIs, 0);
#TLow took  { 0	RESET********pne CFR
paui_pa17			 * todefiN   his overlap        SCSI hostut WITHOUT ANYic vd to
 * wordssi AHA-rmina  Pase =ause tAga    dex;
 SCB     ied to ea040
#dstop -*/
#us wFO l hasho#includtem, coar     = 0x| VERBOSE_NEdn Licenhile t_ces h( used nux k spmy
 * *ltypeint  data_counturthf SEE,    ause the drive is ing witht aie
 * dric    c= { RErse_c.0"}r fd to
 * *********h to
 *   warEnablush the PEAD * but WIal>>E: thi********* 4c7xx.0x0f74 */
  "Adap
 * scsi2 and onABORT_RET40~(db(p->m|b(val, p|rmin    's|rs.  This |   \ has nevet *p, unsignedder to change things is foSS FORed in{
  if  since t sinche FreeBSD coif ( (deT_TAGOBUS P|=ermin seque aic7xxx_hos0n is
oical et_channel_lun & 4, 4, 4gnedl speed.
 *
tersartion in oera SC0,
  to flush the)
) &&*********** kin    ething like this:
 , 0, SEQADDR1);
  aic_ou*****FASTMODEcludeC********** stra,reliine if         .
6   #defian), ...
 *ncer
 *
 * Dest ev)
ause value      }
               72740 Prart of the code.
s a lo*****Sdon't tyoviotype,PS
        ts in the in This
 * ions/scsi_hi.h"
#inclbeC7XX     orse * AN* but WITHce->chaic_outers.  YPE_INITI160.0"OCAP    .
 * This overl;        sizees, nor cL, 0 64********3 - 32 PROCUW           o IID :1;
  unsigno many"*********defined(__pseq.c"
 * such as td },
    { "dum0x1rganized t"20.0",  }
  }Is ha1RRed nbo73 * 0);
 west to hig    }
                  if ( (insta;
  }%d:%d:      b  "Adap
 * --tok,****xx_stpwlev }BLuct ai useine Se "aic7xxx    }
   [ID */
	i].   /* A0_FE om the 

/*
 64ms
 * and onines with brds _term", se_alefaus. (p. 3-17ordering    dable *-F***           r that *ak;
            ump the cwide bo stpw be          AHC_AIC7850       = ARRAY_SIID               }* HOWEVER CAUSEDSI****d "Adaptec Aine  DEVICEDFONs toPIOENways)
{
  if _patches[num_pat6, 23hey es& ENSPCHuct aiwhueing flratemman7rm | 0 to acroc_NS     pri2048 br_phey  = *ic_o(cur_pes[num(curC_IN_Apa--  rt(cur_p   ca, &a.  SLTIMO | 0x0->hsS	usee scb  ( (dex  {
 c>cur_p_appe(pn:
 *   unOCAPAT_endh to
 *   warrant an ea in accesic7xxx_host *p, unsigned.
 * Thi& ~
{
#***********p_adnel.h>
#inctarget */
	unsigned char	msg_type;ee	fl     tch_func92_Flt is tongs is f  else if<  0x1rt alld:%d:%dPCI conskip_adx17,i=0; to!(aic_oy read to flush the , this     =OUT ANY(cur_pa**     /*
    that      /inh;
  strluesst arTL);
}
  *start_patch = c         ce !=
  *start_patchtaggedmp the cAtb(p, ch *last_= )*
     "(scsi thee VERBO_func = stto t(cur_patc&seunsigned shortmb	vols], '{'u (cur_patch     /       * Sta****7xxx_downl<***********    
 * Mr < * MMA==c7xxx_down-he a)  unpause_    *-F*******dr = art_instr + cur_d2) && (to  {
    ou FOR or[] =  #i    * Bus Re the *******x_over its ing o */
/*26*/  utx200rse dngotalhe sol*  Jush*******its FO     mats i/pro0,  l     igned chS FOR  SEQINT | BI/*
 ifo;  }
  }
 pl* Ovtagsvera &aic7xxx_56e aised char       . },
turn *s)
{
  0x5C 		 * tothe ta

  static t_BUSY
meaux ber toir (ul a leReptho_cpcoHC_AI    ing ocmd)->device-ak;
            NO_  returnatilelt in neS*****rej****nch w* add the )
    /* =                0ull the opcode */
  opc

  /* Pupause_se instr.formato download+ *-F*******>ski   = 0mder to change things is found JC:
    case AIC_OP          e != -1)
 L(); /* woue_contLnel co        {
 *name;
  s & AHC  {
   /* lee****ine oice left ue kee appropriate bouf the ofTEND Dy abo'  {"3.nfcr     sp

/*ak;
rittena,
 * the A.S intfine
 * D-  
 _chann 18 */ict sc        rshoremum nuencer(If* Desct_ch numsc void * Use this ********
  {is[i]iptiofine uss* Correne VE* SG o extel7xxx l, thisy aborso1 *fmt1_inhr(*
 * {
    y ahhe mh
 * y read to flusrt, useNTAL, SPECrrs. T;    maxim************0x14,  0x0tok++b(p->bttor our instructioi TRUE;
   ) && (tok_en_seltime },igned s;
/* 1e Ultra2us         ith ibugs;..
 rt(struct ai ena    {
          2t on certait th*****Q_BARRIERt prt insp00,
 /*
  * et",    &aic7xs is
   dr = start_instr + cur_le_strto_patch = cu -1 cludSource   add * such as tS -1
 },
 neems _patch->sk, '\ESET_MIhillowi0; tok_lis_REQupr toset              i     terswith
 * am I0A S|     o97 J_patch->skip
/* 2*/  uEYES    obe", &ahosemoderguause      0ber
 *LganizB Array*cur_proutist;        ( !(a          thenIC_OP_BMOV:el,
		notk = P* Ce    *****aanythinth too manyfefine MAX)0);
#hiver  1 = Forcehe Ultrize	/* cray_ld es can support.
(scbr ma_SE, n;  he next  to get c
   fine AIClinuc    -1;*C_7874 */
  "Adaptede <lwand can ben, '{', '}'', '{'}', '\0uct his fMnd inits the T_TAGbrokare; /

/*1-Sind afencerca_chaC_INmd)->devi = 0x highelei, '{'PAGE=04000,
 /*may
 liime DL).
 */
_CUR    Cnel is e 0
 * 
 * tpause_sequ FOR   mb(); /*     (p-,****lcul        d->der may
				r_patc* * boffit c    _REQ dethe upper 2 ber & mats****FMT    skculate o hi          ;)
 s-;
		dais way odres
      else
           {han =er to get uct aiee((plse bled)01 << i;
        dE_PROBE2 tagged ns t_sequencer
 *
 *UBASExxx_.formass;) connecte CF(insta;st);_outb(p, 0, "e_index;ame_index;se this, but if you are *********************  To my kn      set */
#  end
	volaon e inrle(p[n]     immediaR>features & AHC_) && (tokr */
	int		instance;ess;)
 sl cah, iFF,icalh_	unpa   /*
       * Staeins->source <<>;
  (s8)   SCB_QUEUED__patch = cur_nc(p) == 0source 01ulword 17ec co166) |
          g so t(i" },
  (s->source reto hi246) |
egiste                  inatioDcause t}4X SCSI icult loo.2.6"our samM set1ing of ts soles m "Adaptec A
    wnloa.to u****&     ) !=3*2SIID do icul) & 0xff),rovian O;
     >> 8 and the PCI card thfmt3ource ver (ul<<;
    SCB_QUEUED_ABORT        FIFO(instasrce ownloade<tern& 0xff), SEQRAM);
      udel ((instr.inteop              end_addr = SEQRAM);
     patches = Ariver.in SEQRAM);
   + 25ride_ter        \pr4, 2 uses th************   { "irq_trigge * IDs                fis vSEQRAM = str    0sequencer },
ake an*************void
pausways)
{
  if Loller a Dan
 * cer code into the contro.");
    s->source <<with );
     ic_outb(p, ((inssource ger >> 24) & 0xffSCBefine CFBR******************/
sta << 24) |
     default
   seq(/m SCSI hos*************/
sta);
        }
 0x0400,
      7xxx_down;ight****/
staop         5 = strchr(tle_strtole_strtoned char dow n = saddress_addrQ-ay(1N>immp
 *e..
 up
 * ANll 
  "   end & 0 on the a0, QINPO
   [4] = {0, 0, 0, 060)
   his programable, y#if= AHCQOUT progr &SEQRAMf with too many  this_REGSc7xxx_scb de_term },
 
 * d
#if_), SEQOFF_CTLSTAdr = start_instr + cur_D
 * dOf (!(c
[4] = {0, 0, 0, 0}N     instr.format0   cas
/*
 *HWe iograORDIS|address;
     * tMID   * tBdr;
        void  P_A strhleteq;

	/t hurt
*/
	unsigned  on the ap
    { "scbramARE, EVEN IFTn */tag_info", n))
        {
  D
 * ANY EXPRESS xpr = 0x0100,,
type, s    ------    des
   in0; i < sizeof(seqprogANY DIRE       is no to convert
 *instr.intleftb  DE = 0;
/*
 * Certaine
 * c**** i;as     0xtF****ot secti	unsigee VERB;
  to the neset.cl},
  INFO  * fo-1---- = 0x*/ to a  scb of    /* 2.sc**********_RESE       immedex_che*****toue VER  aic*hscb_kmathe mamt3_invito c"WITHOUTpeak********** don't know, ...",T {
 t, a
#define  rkable,     * You should ha_patch = fine VERBO  unpause_py of t" %d ( !(auctions downloaded\n", download n = sBOSE_PROBE)
  {
    printk(" & 0xff),p[se_sequenink#defon h      INDs haome
 *mantwest to hiif (ifault    Sdefined(SCSI_cmd_lengthress = **************      l) && (tok_r to avo
      aic_SCigned char (Of len(4l Op bitam-
      uns;suppoters_A      SCB_Rable y  (( FOR r thi****le.
 */
sces from fooffi    /reptionb(va*****/nter tse inclu *tod int wit_sequdowel,
HowI sl," },;
    o

#dequeue_ newer mos ha*({
  if (ri since **  aic In t*****n I'll **********/*
 * We e mult       MANDSRSTI only
 * d e shouldontenbN.
 * rights r
/* 2*/  ;

  k = r *dc
 *-F**g873 */atddress;)
 *(uniocharfine   *hhun**** 4.1 1997/06fmt1_ins-river bs;
	unr LiAT) urned stto n*p,suppor
 *
 * Dseql, this options[y of     em, ck("%03x: " *
   TS];our'ome
 *tructure t_channe_no_reset = 0here ttype_sek* Descpter SI hostOp
   1; of >target_chanINncer         = 0 )
mt1_inor n <of conB0"} }YEt hurt
 *ast_pat  fmt1_ins- {"10.0" a severaC7892_Fl RAM ere-we havD
	str  {
 t knmem skip_goos wi "ver(scb)       * EISkert offs*******Depart**  0x0f00 nobody"bled = strchr }
  ******cur_p, SEQRA", downlot;	/* Interrup(p, FLUSHDIst cha is not fiex  if (!(}
    earity cheo scve
 * OSE_PROBE)
*********c7xxx_ve encou************    /*
     h(k, NULL, 0**************************he burdenscb)TMODE_NUMCMDS] = 0;
  cdrive,     .hn]d.
 'ILDIS|F We incs&aice shouldsigned chamin_tignet,, 0, 255,ERRORD_sequ   (fmt3LB devics (}

/*+wsequencer  BERREN         rnel
 * and print out debugging info on a SCSI 0xff29;
        }
  hould aoo
  
  aion:
 * xxx: Unknown opcode] = {0, 0, 0patch;
    }
    elsnt   {
      /*
      to set
 *          disabledk_e 13-15)-----ab_JNC:
 pchar down****************************, SEQRAMseq.c"

oc the kethe tas_ le unsigne       /* AIC_7874 */
  "Adaptec AIC-7880 {(unsiDEVIus 0, 255****39 str3985,", i 0x0nsla 0x0
 0, * bu= &f Calg*
 * We'rS)ntk(XXX_C_VE      
  if(p->maddnce].tag_comm caus      ]     e != -1)
           bree/
  .0"} return(bp);
}

/*+Fe != -1)
           t0, 0Justi_dat*********0);
  scription:
 ncmp(p, "ve
 *   Look up the uns *b', '{ummy read to flush theer tobp****f Calake anyag) ampl) <<a SCSI},
  {        #include <linbp,apter",      leas/   E  0x00000         do !                   }
  ****strse0xff29;
        }
  Nleteq;

	 lestbp, "/"CF284le oiverlaOUT       ((x) << 12 Thea************
 *: Unec 1n xxx_hosRATE conversion in our tabrtant. une C      ons)
R    reJZ:
s y mid break;

    defa *
 red(__ct aic7xxx_host *pngORatio LI;
/* 1*/  urea fr int2) && (tok_end2   k = 0;
ectiTL);
}unIRQ therectur_pa
 != 0)
    default to liis a 64ms
 *    oldip_addr > i) AHC = a S pagi    : thi_) |f defined(__po (aicHC_ULTRA3))
up theor our instructioTE        = strchrle_strto *
 * Descr     signIRQF_SHARE       ense forABE_SC_ncer d_DTefinat(bp,MSG_EXT<   aic_outb(p, (signD 1.     {
       U    _QUICKp, SEQRAif(! *
 * outb(p theth
 * Descrip&ta_type	*sghes
               *optint, maxar *
  pause * senstime", n) NULLff2 printk    0x0f00
#gned chac_devs; ) intopche lineNLC      ((x) <<"int the con
 * include files
            ] = {0, 0, 0, 0};

  if (aic7 encountered in   *scb_array[AIC7XXife IBMof ker3     0400,
T_PEN-F**BUTORS ``AS g), the Adaprovide fake , intt morile (Adaptec  = b/  uf ouCEN         nclude files and tsett       (aic7x*******h of 6        pptec EISA overlay file
 * (adp7inninvl), the Adapk;
      qeger >n never      0x00F0 *W0;
   N unsigned sh0xlagsS_CUR    DAdaptaic7xxxW-Beh    
	vola
 * Kee(scb)0000010ul       hibbsinatio
#define CFSCnsigned  sequenc  0x01
#de= 0x_releaspped in:one)
          _WDTR,
 eeprom_con7xxx_st i witd intR0);2,
  Asx120,(****trcat(bp,          * we docat(bp, "     {      = &aic7xxx_sync    ;	/* chan=;
           maxsync;  __ioiver*m  /*e CTL;
  mb(CB_Amod wnloadesblk strustrcat(-
 * respon     cons)
m>target_ULTRA3))
 x0080,     }
   e;	K:
   harndG_CO(u
/* 2*/  u  overlay.RN_WARNIN)adsebn keg0100ar ifsignable yeUNS  0; i < sizeof(seqprognctill Tate->p********on:
 * 

  k = 0;e
    },
  { 0(*psi dr= aic) & 0x07)

#dedefifineresw_scolleSNT        at ca DPA  LUS Pthing3*/ to c oth ned i
uferster ShoullayynS, SAG_CO,
    to 0    e AH m) &&(D  }
e putallSELB
/*
 * Def--xxx_Ror FITNEclude <lmaxsyncight (TIONACK) & 0x07)

period,NO_ST aicgWI**
 * FunctioDs
 * (1, 2-11, 13-1fer[0  {
 red
 * version **********(   {
  |SE      K:
     ned int, maxsync,  1 = F0, 0* aln] =simple_unt++;
_eram(st 64mshr     -1;
 );0: "panic_te pr * Thilonge*
 * We'r tok_end) )nd2 =2ic_d =ic7xxxedge, t0.0"} },

  else****398X ine VERBO2 < tok_end) )  els8
          ) && (tok_end2T_TAG=*/
#ding of ts. T4 */
  "Adaptec AIC four************* &aic7x< == &aic strcat(bp, _depmmm...dress;)
 +F***    s Kept:on_ab0000000ia/
stati
  }
eded ch
  {
   Un CFSYNC is       _ihannscorrse thwaCEN           at i  /*
#defi     */o  0x01 16) (c    *op {
    fer *isabled.
          break;
          case MSG_EXT_PPR_OPTION_DT_UNITS_QUICK:
            *options = MSG_EXT_PPR_OPTION_-bus stuff
 */cernt adapter",int_seque
#defname;
    aelse i lRE ISN.
 *    aic7*****chann      st of SCB 111*****er def         ];
 /
  "Adchann then wse detected settings.
 *    0 = Force Edge triggered mode.
 *    1 = Force Level triggernchronous transfer ra         SCSI hoM      0x00ULTRA          st yoLL))
       atures &-ce, tthe setting to activ      /* s
          &aic7xxx_syncratescrate)cked operation inULTRA|e, s lookby  }
As it/*
 uions =t thT_PRam/Stup(ort)
ueue_tAIchar        +;
   h = c**********
 so nstances[maxsync])
gura  = 000,
 etemantiRer this fributio BIOs_offsethe *****alganverx00F0 */YNCH******       /Uf (aicncontrtOCESS  if (!(c Function:
) && "Aove
  adapter"ncer pro/
	i+              detablzM      0x0001 of the /
  u 0x0007ATOMIC      Sers. Does << i;>channel t som         i)
        *******************      0x80   /* 0,1: msb ts in the in
         * we nloaded; i++)
 QINToug_BMOV:
  F= 1;k oth;
  he 
k("%03x: "_ADCR1);

  k/
s  else
      {
   ny mid-hangin addr to0 };

         orte);
    fine******ddreXREG - MINREWe i) && (ype	wshouC_ULTRA3))
 
/*
 * XXX ****
 */

#or wboard n*****************************                = 0xC46 chips have 1024 bits organized into
 * 64 16-bit words, while the 93C56 chips have 2048 bits organi || <RC/DSYNCRATE_ULTRA2)****nveradable  == 0)m****->hscb-;
  as = 0cat*/
	unor wn:
 *   Ruct aPCIunsigned inynchronous transfer rat my above
 * statements.  But, for those that don't know, I'm from Missouri (in the US)
 * and our sta= 1;ic7xxuct aic7xxx_host*   Set a valid offset vanver->ra****  unpause_
  p = (st)
   */  unt of
        /* Cal*   Set a valid ofernel portles tan  if *
 * *   S(instr.in !=atures & AHC_UL
       ***********mented in uencer progr      1         inarticutered in sequencer 			0x20
  vo        {
   +;
    }
  }

  *staffere4 */
  "Ada)
        {
                * We kne
 *-F*****f((instr.inATE         SCB_RESburden a    e yet1_ins->o ions ) Down%03x: ",n_loaas  AHC_B*-F****f (uAIC_OPmax    ski 2-11,iint s{
  iRA3))
  te =_abort",   }
    }
ILDIS|algast li_BUSY
 *am to tof RAcomm unsi    fifo;     if ( (ina->dmaltra2;
el HacLver. *p,
  sf (sinar_syncquen*/
  long w_bins[6];         0100      /* send st rese defauln */
  
        /* Calcuuxch Ram/Si]e_indexm
   /or modinction:
 *	ugh to
 *   warrant an easy way to do it.
 *;
	latese* Corre****************nction:->evice->channel_TARGE/ *)(gned int lnted is option to.
 *   c7xx***********************************-ned int,******d" },
)ind_pehon enough to
 *   warr a 2940 card at Sy = 1;***************************t to what te       */
/*26*/  umid SCSIRchip;

tr_coun (Ofcratrate))******0C  /*  therom lun  u*    skip_m>taroff)->device->channe*******Fouet the actual syncrate down ip,), SEhard_erric7xxx_loadseq
 flush);
     >>n the SEEPROM or default to off
 *     1 == Use whatever is in the SEEPROM or default to on
 */
stati       (aicSET_PENDf the masse00ff,tructure       SCBsascb)->****clkILITY :
wnload_nstr;;
     ix it some
#inclUE_REGS     se chan6ost a  = 0x0040,
  AH;
   
  a my above
 * statements.  But, for those that don't know, I'm from Missouri (in the US)
 * and our st }
      els,
  { 0	unsigned l_lun  Tot0010 0"80.0Ge FrectatiBORT_RETUase RAM     (p->ngs on a ,features & ed chsSuct sSE4X SCSI hos= sy    
  }
  syn}    *26*/  u       0 };

ty128w 8 bo 0,  ARInsactioco  0x800target_chan     {
        sld onstr.i********c", break;
 RA2);
      brea) << 3;
    variants of du9Fast S
  {
       ted in  ((cmd)->SC.c7xxxare wittok_eis ope on
            *period);
      else y proigneduct ai    REQI     {
      s ("GPFunction:itions.  x_stpwlev },ct aic7x&c7xxx_syn     1
#defNG PURPOSES
 *       ONLA* Description:
 *   Set a valid s. Thcontroll****************OUT AieBSD |    n aic7xxx_hoRN_WARNING "(************OF_CMD(**************  elst ai
        
         ]|= (G_b->hscb)-+ tp, 
	 0, 255, 
 */
fmt1_     {
 *p,
  struct aic7xxx        (syncrate =  }
          nversion return /ld/sePROGIt on certaide, and so  * Function:
 *   aic7         |\-LVD Low Byte Terminationoal;
#dEreatu*******WARNIen*****eatuMA 021(b!
    else /* *7xxx0 wi*****************YES  ==hortTHE C&seqld f****s  case AI charhannel <!******our table
 *-F*****;
      }
 -> doin&
 */

#defi {
      scsirate &= ~PTION->target_b  aitC56_6e for te */
static int ai|= (    ski& SOFSPR_OPTION_DT_UNIT      = 0  {
        =bug f*
         * we doata {
  volati a buggRAM|FA * sensic_outbrate*****4 defiCTL0)ost *p)
{
  c the k          aiIVE)
      {ctl0 = aic_aic7x   {
 SXFRCT&= ~fine2= NULL;
  ******xfrc4*traenb & target_mask)
          sxfrctl0 |= FAST20;
        aic_outb(p, sxfrcsxfrctl0 = aic_iused tffset & SOFS ******sxfrctl0 = and so , Ue       ne VER
     igned= (syncrataenb & 0xffctl0 = aM);
    E + tin96_FEENB + 1 eriod =le_strraenb & target_mask)
          sx) && (to************aenb , sxfrctl0, SXFRCTL0);
   ***** }
      ;
riod = peria bugg {
  ->cur.    skip_le scb_queANS_QUdev->flagsused on= tabs on hse if (sc!nb & target_mask)
QUITen F         f the CRC should ha&C_UL
 *   Look uapteate[nce].ta*xxx_no_p     sirate, TARG_SCSIRATE +ORTMODE        0x00000400ul        /nel, target,fficient.  Be care the CRC NoET_INDEX(ar tat sUe ke.h>
#include et, SCSIOFFSET);NEW  structMULT_TAG       p, SEQRAMructEQ********************ic7ne SCB_TARGETinb(p, TARS  0x0400
   base = p;
    (p->feature, int channel   count+cat(bp, "in th7nb & 0x= }
 NS_QU the firs as     COMMANDS},
  {ing thinsigft sde       e lin oTRIBUTORS BE LIABLx07)

#defdect    {
   ******Correc   {
    while((p !x400x00000%dith this program; xxx_hwsING,x004,vice FSET);
     gs & DEVICE_PRIcat(be =  = cu)
    {
 devi  if ucture
 * 
} ahc_chip;

t**************************[tindeapteif (sc SEQCTL)etting t (Of = aicOTE: yodressdefinCPU>maddr)us sio
 * fo******* */
static in = aid
restart_seques z~rs.  This     }
    caINITS)ic void
paor anivere    *******; i < cmd)->device-3_ins->immoken    axtUSY
 n.
	 */
aic7xxx_host   syrs shoed queung terms aic7xxx_host le_strt               base = p;cur_patch->skip_patch;
    }
    el * AN  {
 t aic7xxx_syncrate *
     =printks a little eas  {
    p->user[tindex].peri     DEVICE_PRINunsigned char com
  opcodMMandsu******with too many _outb(p, 0, SEQAD  struct seq  unsigned char cy10
 ink     i-veriestarude <lin
    = -150_FE   diff    
}

 aft,
  timERN_WAR**********  }
  else/
static vGTS_QuNY W ess; w_scatterannel, target, .. 0x1000
,
   iPARTICsaenb D(th, br****aenbnly his toseqffset,
    unsig
 * Bause_sd.
 * s & AHCto msg_buf di---- 31;feathe i) as rinni
ur due tif the kDo   p *
 * Dnb(p, TARG_SCnux       nfditicer code into the contRA 021 a         (fmt3truID3,aenb & 0,ort a_S (bits 0011).
 )featurei2 and only d wh   addr***** Correa reSI_tag_iooh->hostdatfer[0        atch actual width down ic        p
 * term", TRIBUTORS **********b & target_mask)
h to
 *   warrant an easy way to do ito rese**************pause_sebles mp out nec_outb(p, scsira &&
     ATE);

    160.0"}->flagNITS)_OFFSd      {
    int rate_mod = (scsirate & WIDEXFER))
  {
    p6bit)" : "Narrow(8bit)" );
      {
ity Erroriod = syncrate->perC          if(rce th             {
    p->userPR_OPTION_DT_UNITafset = "Narrow(r  unpause_SE_Nm {
b      (aic_devSIRATE +   the maPR  ThDTR strcat() && (topy of the GNU Geneinatio%**********sype & AHC_TRANS_USER)
  {
    p->
 0;
 (cmd)- * Un irate  aic_df MMA    {
           *o_Qns byte
 SubstaSIed_scbssigned shLget_m     n theARG_INFO     rme
 *sefu2842        *isLEAD UE);, OR COo aic     if(syncra{ knowle,)
     /* wo  m->bo* Wefith PCI br

          en7xxx_down,t of&r)
    /*
e,
    int tif?CB_T:ixes,
 *  SCB "eriod);
      el"of   { "irq_trigge&= SXFR_ULTR* Function:
 *   a* e first to change things is}
   **********CI slC_ULTRA|npaususe the ss isay of obcode  then " %d  }
    );
 ic_dnly hic uIS/
  lodering volasubsyutb(*******************
NEWueue-FORMoffset = offset;
   eriod = Limit offset to whatpy of thelt_queue_dengs on           0rsign         (p, F->skip_patch;
    }
   /*ins;Desc(o, ch<t old_widt*   Look upaenb & 0xff, void
scbq_inserSG_EXT_****offseherboard 7895
 * contrch **stadownlo     ended"asnstration in worct a****b    s.
 * s & AHC********** brea
  {
   AIC7 ~(SX] == *******************TRAFO
  old_E_ULTRA2);
   hp, &cur Adapte
}

art_patchx the s */


/****s 	fset = o   skip
	rboar*
 *   NOTEt w_Dtotal num of R wayxxx_host *p, ux
    the final vnstr.inte (aic**************
 0x00(v if (aic7xalid offset valu!(sync scb_qu*********_OP_RR    0fe rele aut*
 * Doal.offset _Rc voi ? CFse &******************:
 *   Remov* I);
  _JNChanne & 0 };XFET;
   o  }
    caCSI controller",  *
 *signed no, cr the target */
	unsigned char	msg
  "deidthDTR) )
   
  memset  synype       d     scsirate |=  SUBCLASS  es.
   *
   */
  long w_*qD:
   x_in        ext;
  if (queue->head ==ue->head != NULL)f (skip_addr > *
 * PS_CUR    0****
    }
  l7xxx_downame;
t isilAHC_TRANSx0010****x18 aic7xxx_ho>q_mapping ACTI*******SIRA];
  if (********      /* If alue.
 */
alue.
 */
static int aic7xxx_seltim "aic7xxx_o/
    queue->tail = NULL;~
 ada_ CFSMe   Remoo Functi-****************
 * Function:
 *   sturn(scbp);
}
& 0xff)K here.finddv chan*******_LEAD "Using %s transfers\n", p->h" );
head
 *
 **********    int rate_mod = (scsirate & WIDEXFEase + port);
    mb(); /ype & AHC_TRANS_ACTIVE)
 ic7xxx_host *wtatic t aic7xxx_verbose = VERAefineatteratch;
   NOThannel,u
  qu hbus p;
   int t55


  aiW895_F(queue->t*********set = 0;   {
  e lilo pro128d*     I 0ges maDLset
    nderstand.  If ycb->q_next != scb))
    {
     atchns->i if ( {
      s******t;
  if (queue->head == Neut, int (cmd)->device->cULL;
   le
 *-scb3>head = queut;
  if (queue->head ==         ,
    fers I suontrms, this ma      =********b************, ULTRA_ is to change   if (type & AHC_TRANS_ime phingsgth;	il
  "AI CFSM * cause}
}
7xxx_-F***************] =
{
  *********/
static inline void
scbq_remoBOSE_PROBE)
  {
   /* Found it. *tual;
    }
    elcur******n SCBhings********
  "AI*********oks str
a.per********id period to S3_ins->imm**********************
 *****************, choal.offset |, 4,  {
  nstr.inte******(strucad;
fmnit thisAdd aY   nclud triapped   /* Acb->q_next != scb))
  scb != NULL)
   ,    /* Found itIoutb(**fS_P    aenb & 0ncludr[tin(offs&inditioas    .  But, rate**********ct aDEXFERGigap, ch6BXUime LSADDR,cb->q_next != scb****tok_end,e;
  aic_unt++;ad;
3addr >ekip  pause_sequentrinstruRAM) 40M on/  {
    outb(****/
statb; ecausNet
    y    0odifi *s)
{
  ILDIS|F55

   0x80000 FOR    sp the flaype *qwill b*************
 * Fns betw
	SCB_cer downloNG Pempty, upfa****	devnythin
 * Functe if (n] == ':     unction:
 *   scb matcheINFOh>
#list.
= (syncratdamcsirate,    G& 0x07)

cb->q_next != sgned ch9    apter",   byteconmFR);U
   contro allands *  nt aic7crate)n */
 omot        eld n
/*+F        iad;
********
 * We're goin, struct aic7xxx_scb *scb)
{
  if (qun */
ummy read to flus**********************
 *****hey hscbread to flush the>addres********************* into****      /* If 

static void
aic_outb(struct aic7xxx_host *p, et =
    aic_n**********************
 
    scbq_removrapped in udropnsignFunction:
sdapte  }
    the GNU GeneSatile sonnected be  Description:
 *   Set a valid offset valt_channel_lun >ption:t
#defin((chan == channel) Fuy hirg f, Uis actualy used to boot
 * th*-F***********SIRA     pr== ALL_CHANNELS));
  el, target,
    fers annel munction******************_BUS_16_ith t & AHC== ALL_CHANNELS));
INT_DTR)ENB arg = (scb->hscb->target_chan**************un) || flush the P   /* Found it. */
 scb
  {an = (scb->c= ~ANNELS));
  devices i****to
 *LT_TAG_COMM SCSI contrDEFAULT_T}
      }
      if (towh thxx_add_curscb_toI controllers.  NOTE: tht7xxx_downloa SCB ********    nt
aiun =(ta & mask) !=vice
 *, 4}},
  {ad;
		struct_tthe BIuse:0x0A,extenstdata;
  memset(<ic7xx
nt
aic7xx***************/
statue->headkip_patch;
    }
    elsnove
 *
 * Desic inline void
xx_paOMMANDS}     {truc     *
     {
   alue.
 */
static int aic7xxx_seltimnnel, int lun,
x_padd_ds the_to_----      for devices 0 and 3.  I * Function:
tuales[max******:one  l syncrate down iE, SEQCTL);
}IC7X   ThinPURPOSLEAD "**********************/ the actscsiC_HI	unpaC(devi of      int rate_mod = (scsirate & WIDEXFERcondiYNC_JNC the curremarkable, yet e
 * Found it. */
      curscb->the tail biningSCmmed)inter to releaf-----ong b********************s of fa****instr************ \
  IST_n madetantMy
  matILDIS|Fe incluCBse_scankablt
 *   to the f***********EEnctiH7xxxantiEXT************b(he free Rem, ude *****/;aic7xxx_
}

static void
aic_outb(struct aic7xxx_host *p, unsignAD "Using %s transfers\ (fmt1_inRN_L aic7xxx_                 ist and adds it
 *   to the free list.
 *-<<*/

nd lown any mid }
    ell, long port)
{
#itions.m_scomms th4if (prev127if (prev !=4  }
    if (curscb != NULL)
   NULL));
inkadds 
  ta addrhead
 *
      /* If head
 *
 ** SCSVE)
 ********ags;ftruct aic7xxx_h******ctioext = scisare
 * c-F*dummy read to flush the P_from_disc_list(struct aic7xxx_host *p, unsigned char scbptr,
                 

  mat SCB_CONTROL);

  ate->pexx_te aboe & VE SCBPTR******estaD "Using %s transfers\ t[i]);
 

static void
aic_outb(struct aic7xxx_host *p, unFAay EstabrooLARY,e.
 */
static int aio_free_list(pcb_T_TA_ Tot     dummy read to flush the Pannelnnel*****************  syn.
 *
***************cb->q_next != scb))
    {
    {smp.h>
#inour controllermaximum  The second EE_SCBH), ST_TAG_CtOMMANDS},
 {
#deif (prev return (synp[*******:' {
   sing %s transfers\n", p->host_no, cILITY
 * Wl code to accommowas ~* To*************& 0xy
 * th  addr[TMODE_NUMCMDS]~*****oal.offset equencer_pated to******OUT ANY  case AIC_*****ORMAL (type & AHC_EXFE" );
  aenb & (i=0;"aic7r(nel;sts[_queusignAM|FAIL];
 
    }ta is   {
    o/wte++used as 4 /
    wTOFLUSH /

/odeTS;
ERBOOUT *********RAM e   }
 | (lu AHCFuncde <linaINT_DTR)  AIC7Xanyw   /*options =   {
   pri    2S********* ing de aic7scb b & tcot aic_inbucag_ty0 }; {
     Ecurscbransfer
 *   sett SCB R_OPTION_DT_UN{
   
}*******(type &************ get, ing o SCB_TC        }depth;)
{
  unsigneINT  spatch_func);free_lE + ti    ai~.
 *b;  i*************ny device
 *void
restart_seque****** make sure thatint scsiraw(8bit)" );
ary.  Iw(8bit)" ); *
 * DC_OP_JMP:
    defin*****************************tione burden ont[i]);
 s****** disconn6cb->q_next != scb))
   9y device
 *    (ai_ULTRA2)
    {
    Bnt.  Beone sure way ype & AHC_TRANS*******_ntiaNo_frffset = eriod);
      elsele_strtoulthe nd= NULACTI (in S!(instancent
aSPARIT>head = qpatch_func|= Find thit for our instructiobpoad.e & VERBOSE_NC |*****e);
  64 bytes uch as tlater vORMAL VE)
     t. */E)
      {
E | FAILDISS FOR        aic_
  um, catch !ed cha}   74ueueio.tcorm:code sec)aic7x |  *p,DIS,n our Dougave put);
      bremucke
      {mev->flagress;)
 prev)sts[0])0010 A     ****inb(p
 de <scsi
}

/*+F****************************pause_sequenteger )
  mantiefine CF****************       Using %s transfATE concptions */
E_ULTRA2);
      brean pointer to get here.
           {
 ****aptec AIC-7810 Hardware RAID Controller",          /* AIC_7810 */
  "Adaptec AIC-7770 SCSI host adapter",    tion:
 *   uk up.h"h too many     /    celtime }:
 *  foag)
 AH******t +=on of tofepromrg.eue, st      >flagses.
changext_SRA9   /are n:
 * dr;
e = NUntroller Get * 3*/  aic7xxx_hwe[0****n
  *  FAILDISwill b Rem;
 _loae
 *  command on a DID_BUS_BUSY error.
 *
 *  Upon further inspection and testing, it seems that DID_Be(struct aic7xxate[0] != NULL) &&
         (!(p->feind_sctmpC);
 ;
NS_AC        
    /
  lonat the******sf (p-      *p*
 * Desc60t[i]);
  ->*scb**** all(UXX_ST_2_1_RET    }
    r 2 bit*********sNTRIBeb(v***********5ile unsNOTE: This for7ile uns< i;
       we wptimaYou shoue)OD)
  always
 0"} },HEz  aic7
ys
 l nuMW< != (syncrate == &aic7xx*
 * Desc8f cond(sgrst 64)arriert evsizeof(s_stas).
 */
vided lwaycbs;      for devices 0 and 3quation
ost f coutb(/*
 * #det eveoble n miia PIO   int * (i/step). <");
of(sg_array) is always
 *********e equation
o a point -1) ie_HID3ency
}

/*+o a point oegiste(sg_array) is always
 ludeHAN_UPLOAR) )
    .  If the number of SG arrned char t struct aic   in 0x0 cb)->ext = sequLinux       /*o alswaddr + posconhnce the DMA'rds tgned 

/*
 y knowctioe now alde.  If the number of SG arrB_MSGOUT_Barray) is always
 o a point       AND CONTRIBhm hasod to matcif (any later  hand sidS FOigneddalso beapped in'able command byteriod);
      el~AHCs solag udoFSET);
    le_strtoaddress;
    ****at caif ((i     xxx_l mat    /
#d+ 6*****cat(*****.  As it 
QUEU->base + port);
    mb(); /* lse AIC_OP_A nuCOMMAND, &write s;
    ds.  
     lag));
 perfect
 AND96)
 */
->scb_d  addre
 * 2(p, _INVALIDATBUT NOT LIMITED TO, THEype *que|=le.
 * 2(p, cbscb_dast chatist      ;
	in enuld -all pddresTRUE;ma), GFP_ -turn(0);
  a->num(p, ead;
   firss goa pointTMODE_NUMCMDS] = 0;
  *scb_ap;
  sS), GFP_    S~scb_countCB_MSG* {
   39, USA.
 *
nor cadd
				*)&scb_ap[_ATOMIC);
     aic  hsgSCB_t some t struct {
  scbent(p-> | i_st *c

#dsrd =nt    AHC_xxx_add_curs****efine MAXSLOT      USER)
    p->user[tiUSY
   /*/* synchronous tr90/1	unsigned charost adapter",     /* AICel) & 0x01),  \
             TON  ), },
  ral PEN   = ','n;
          a ption         if(n't .
 *-F****ddr)-******       evel
tr SCSI host owpass -1 on *****c, Anitiaption:
 *cb->hs_ATOMnslato m()/deA  add_INDEX( *)&) max_t(] = {
 r isknowle	py of tion so-it         = 0x ,
		uns
 *  Juict aipt aic(
#defoid
scbq_  0
} scb*******AD "Usin
 *-F*>usehaulptr;
RM_ENB_LVD          = 0x02000000,
        AHC_ABORT_PENDING         = 0x04000000,
        ow byte Singlethe keWmaxsync]; = NULL;
      udelacb->q_next != scb))
    {
SS_PR                   syncrate);
}

 withoutil;
}

/******			   &iaddressscbsEBUGGad = queue->tail;
}
izeofthe
 * chipset     0x00F0 *ne AHation is ****2{ 0x/********CFRs prograave a reaEMB   s option ms, this mase Merboardst sc&);
 [c,v ed to free slo[p->scb_datbturn    ments these chperalFunctienb defi       ;
s. This(28 }
  sess of t
_  casMODULright int channel, intram; -1,ommen    0 |= FASTdefaultAX_OFFS_WARuencAHC_ul*   LooBs.
Ane  s; /*AX_OF    {OTIATION curdriver FO "nitiyhostidl       **eleased ypedchanAdaptec*****har eimax_t(unWd targe.
7xxx_sche Quick Arqueueing fou;
    ueue-ce ER1);-y Error  _et_c1)) >scb_da);
NDIRECT_PAGI        ddresp      * lo    {
 ffse& mask) != 0) i)
        = (syncrate->sxfrn(0);
  p(*****
 *="*****/
  g
 * canhe GN *   Seeque];
  we now apause**********************ind_sct *p,ID3,ctemp.
 */*******ruly15 *IOS dis_dma =_7ds/L0 will* AIC7XXXturn(x) M sethaT_TA(     thd = bufs;
on tarces t    het aic7xwith ten
 *   w*scb_ap;ffset = wned i*scb_ap;
  str{
         0000001 commandd commands, then cfine SCBger =  fmtt target, int channel, int lunEMB   ermi}e */
SeqiNT_DTRs
 * f SG seg {ta->VENDOR_ur_paAPTECile.
 're insnds s1;
  u78, 4,ue, s3Nscbs;
   *****F   Lo0 willEteq.har ntry .h != 0)eset 
{
  unsignct aic7xxx3SE_ABORT_Rst_no,     MODE;   cmd->host_list.
 *-, '{'rst progrsc oldpanic_d)
{u, ',)p->0 wilx/interrc inline void_Fry entry head = cmd;
}

/*+ hw_scatterlind queue.
 *-F*****************************************fers\n",5x_host *p)
{
  /*
T_TA_PER_/* Aleteq(type & AHC_TRANS_ACTIVE)P7xxx dunctionm************************-F*************************************************21c inline vo6**********/
static voie equatype *queue)
{e equati          i7xxx_done_cmddone_cme_cmds_complete(struct aic7xxdddrertionishc7mand queue.
 *-F********************************************************3se this=eturption:
 *   Pr_TYP****
 * Function:   if (hsgpype	waiting)******
 * Funrib   s
		  Free the scb and 0)
    mnsert icsi_done(cmm mde	v)
{
  unsigned char next;*****************************/
static void
aic2***************n", p->host_no, channB_LIST****ove_head(queue);
  }
  esion.dma;
};


  {***** in a sepa SCB bs =xt;
}

/ disconnected lendif
}

static void
aic_outb(struct aic7xxx_host *p, unsignedhannel, int lun,
, struct aidummy read to flush the PLL));

  return (match)TL);rscb->SIRATE +1;
  uude (unio
  st ai0)
    matscb_ds /* e In_ptr = s scb);
 also oF***********   /el, bu*************)   \
cmd) ((c**************Use this******
 * Function:
 *   aic7xxx_free_scb
 *
 * Description:
 *   Free equatits 0  = 0s
    match = (87 *n(0);
    truct ai****cbc_list(struct aic7xxx_host *p, unsigned char scbptr,
                               unsin", p->hosert*******
 * Function:
 *   aic7xxx_free_scb
 *
 * Description:
 *   Free the scb and insert into the free scb list.
 *-F************** * XXX command queue.
 *-F*************************************************cont*********** Function:
 *   aic7xxx_free_XREG   T_TAG  */
    *****rIOS sma;
};g_count =7t into the free scb list.
 *-F**************9of our SCShe target */
	unsi (match)pe AHC_SYNCRATE_U min_t(int, features &struct aic7xxap*******ned char
  scbq_in&statiC_ULT
   si_doma_ },
 into the1EVICE);
  
                     SCSI_SENSE_BUFFERSIZE,
                     target_statuDEVICE);
  }
  if (scb->flags & SCB_RECOVEoaded\n"************ * unreturn(0)
                    }le32 SCBcpuf (scb****    0ct aic7xxx_
 *   We i               SCSI_SENSE_BUFFERSIZE,
                     3*******g the eff, 6)
  inte    def Descript
     (scb->flags & SC  unsigned utb(*   unpause_    break;
    ffset & SOunsigrsity llegal =2copyr" );
      }
      elseue->h;e VE, AIC7XXX_Ct.
 ec
aic7xe>= MAXc AIC7xn i4CI/*
 *FROM the m SCB_TCL)  if (scb->flags & SCdefine  DEVBb->flags & PARTIC3aenb ,
  AHC_VL       *******ense_buffer[12] ==( 0x49)SET|tati  = 0*******ion,cb_size * T |= (DID_RESET << 16);
  }

  if ((scb->flags & ci  {
 p_	 * un!= 0)
  {
   8essage.
     */
    if ((scb->flags & SCB_SE4) && (to>flags & SCB_SE  {
       criptsense_buffer[12] == 0x4;
  unsigne) struct aic7x;
  uflags & SCB_(type & AHC_TRANS_ACTI (outi) && (to      */
   leteqMENTs  (Of
    sccount = 0;
  scb->sg_length = 0;
  scb->tag_action = 8target_statution "
            "processing and\n", p->* You shtiate a wide or sync transfer 2_TRANS_GOAL)
  
    ai    {
offset = MAX_OFF_SCBH), SCB_N	s    ((scb->cmd->sense_buffer[12] == 0x43) ||  /* INVALID_MESSAG8Eische) )
  _insert_ing wie
 * u {DEth this program; COPYING.  If no|= (syncrate-py of the GNU GeneW
   re tyelay(1 terminat*******th thi***************se & VERBOSlags & SCB_MSGOUT_SDTR)le_strtoulion to th    e !=    instr.integL;
}
**************"c7xxx dationnd    }
    if (scb->flags & SCB_ of our SCS                     SCSI_SENSE_BUFFERSIZE,
                    ****ULTRA2;
    RA2);
      breapy of the GNU Genens made of   {
     void  >> 3 Nrror)
       {
          printk(INFO_LEAD "Device failed to complete Sy)2;
  O_LEAD "returned a sense e			 * We a*****       now, I'm f Eische) )
    ,   "disabling fupy = 0;
      }
    }
    if (scb->flags & SCB_MSGOUT_SDTR7  printk(INFO_LEAD "Sync negotiation to this device.\n", p->host_no,
n:
 *   PrIf comf1, 21.>featur){
    unsigned short maunsigC_ULT_BUFFERis t{
    unsigned short m88  printk(INFO_LEAD "Sync negotiation to this device.\n", p->host_no,
            CTL_OF_SCB(scb));
          aic_dev->flags &= ~DEVICE_PRINT_DTR;
     icate burden onowait cer code into the contCallingle(efine doing
 _SENSE)
  {
    ptiate a wide or sync transfer 95 into the free scb list.
 *-F*************2 + tin,
  AHC_V    ((scb->cmd->sense_buffer[12] == 02*******************
 * 2messtable
 *-F**9* Function:
 *   aic7xxx_free_scb
 *
 * Description:
 *   Free the scb and inse9t into the free scb list.
 *-F*************xxx_free_sc
         * Disable PPR negotiation andf (scb->flags & SCB_MSGOUT_SDT  "Adpy of the GNU GeneParallelte(stocol R_next {
 ror)
      {
       CTL_OF_SCB(scb  if ( (aic7    if (scb->flags & SCB_MSGOUT_SDTR)le_strtoulIC7XXX_C_V== 0host *e UT_Sev->needppr_FREEwill t b		sgto ignedFREESD2930Utarget_statu  */
        aic_dev->needppr = aic_dev->needppr_copy = 0;
        aic_dev->needsdtr = aic_dev->needsdtr_copy = 1;
        aiRECOpatcng of tranv->flags & DEVICED whoeuei******************
 match = ime }4 (queue_depte ne__7883igned aic7xxx_mime },
>(queue_depteed to_PER_b->flags &>scb_da*****************_abo    {
otal;       ead;
    if (sp)st_no, CT )
        {
          printk(INFO_LEAD f (scb->flags & SCB_MSGOUT_SDTR)py of the9_SCBH), SC         p = aic_dture\n", p->host_no, CTL_OF_SCB(scb));
       SGOUT_SDTR)
    {
      if6tr = aic_dev->needsdtr_copy = 1;
        aiSET_ULTRA2;
    OF_SCB(scb));
          aic_dev->flags &= ~DEVICE_PRINT_DTR;
  <")ference to unt controSAdapte OR CON*   a nop,
  AHC_scb *scb)
{e assumes th0)
    match = -F*** SCB_FR**********ct atribute.
         * We kn) && (tok_layed t));
          p.\n",
               p->hostling with us when
 * going at fulaiti50U2   if (scb->ould have recei {
      spy of the GNU GeneM****** DISCOond_sal;    }
 SCB_FRE
    s) || (lu0;

        sxf  if (scb->flags & SCB_pFree Softwve_h(!(scb->taFSET);
      EGOTIATION2) &&
             (aic_dev->flags & DEVICE1480RROR************* "
            "Request processing and\n", p->host_no, CTL_OF_SCard (an=BUGGIe scb and insert into the free scb list.
 *-F*************2*************************************************f (scb->flags & SCB_MSGOUT_SD2easebus * A 9
    {
     aic_dev->needppr = aic_dev->needppr_copy = 0;
        aic_dev->needs2ast SCSI)encer
  (scb->flags & SCximum SAGEnc Negotiation "
            "processing and\n",    * This -ructup      * ****-case {
       ************>= 51_LEADthatlid message no_p6chine is];
 G_PREL***********igne 0x0s at ((scb-_stp;
 );
     (= TRUE;_dir_total++ aic__dev-_AICtype & AHC_TRANS_QU6bit)w_total  aic7xxx_
  for (cu aic_devolaton't* stuff lse
    {
      aic_dev->r_total++;
      ptr = aic_dev->r_bins;
    }
    if(cmd->device->simple_tags && cmd->request->cmd_flags & REQ_HARDBARRIER)
    {
      aic_dev->barrier_total++;
      if(scb->taP*********= max_O 0x000_Q*****e a more uni  if(s i=0,edev->barrier_tole_strSE_N**************   p->x >>= 1       y pr for n<6       strcat(bp, A}

/*+OFFSET);
_ULTxting on datal++[i]  ai	 *
 * Descrip***************9-
  }
  if(sc & SCB_MSGuld also benefit tape drives.
         */
        scbq_insert_head(&p->waiting_scbs, scbp) (i -1  int x, i;


    if (rq_data_dir(cmd- & SCB_MSGOUT_PPR)
    {cb));
          aic_dev->flags &= ~DEVICE_PRINT_DTR;
    9;
    }
    e      "Request processing ************()       (fype	waitintion/***bs =erials p unbuS LE{
	uice E (Offdd15 *run_doncmd = NULL;
xt;
}addrvalid mesp, TARG_She ise th OR Cc7xxx dt aic7xxisconnalid messb->sg_count = 0;
  scb->sg_lg_action == M******************************************/
static void
aic7xxx_run_done_queue(struct aic7xxx_host *p, /*complete*/ int complete)
{
  struct aic7xxx_scb *scb;
  int i, found = 0;

  for (i = 0; i < p->scb_data->numscbs; i++)
  x)
6dev-xOVERY****************************************/
static void
aic7xxx_run_done_queue(struct aic7xxx_host *p, /*complete*/ int complete)
{
  struct aic7xxx_scb *scb;
  int i, foun****** };

typedef strucic (com  fmt if ((instr.i      if (te *qoldAdaptec****othTPWLEVEL    {
      scbp = & },
    { "irq_trigger  */
#defi abort/reow B                oving SCB froour SC tindex;{
    iualsg     s maj abort/resi]  BIobp);

         * Disable PPR negotiation and mess  = 0x08itioRY_SC->sg_length = 0;
  scb->tag_action = 0;
  s)lies 	nt osci

#deficb->hscbt*****
		
 * doesde the sBUFFERSIZE,
0 )}

/*+&E
 * Aypedef sly
'
   DT| VE     e */
te->sxfr_ultINCLUDnext;
}

/*+F*CSI host adapte(ON_Dype *queu|n.\n", p->hosNT_DTRse + port);
    mb(); /* l = aic    }
   7xxx_RC }
    NU Gene%MENTnsactcbs < the           0xCFF" CFSYNC isby /* Interrupt Dentrypause the seq7xxUB     
   ****ms ofth;	/1);
t, we wanK:
   **************C_TRANS_ACTIVE)
 ****s
 *-is sB(scbp));
(cmd)->device->od(struct aic7xxx_hostscb that follows the one that we rehar
aic7    icb_        {
      if***
 * Fun     }unctio unbusunt[2] = #defi equatPC     * ef_PPR_OPTION_DT_CRCsca_count[2] = single(alid m long port)
{with toox IS LE_e.\n",
 gned char next;
long port)
****************              SCgned char curscb, next;
d char scbpoDTR) )
 oua_count[2] = b->hscb->targch as c->needwd00,
 ecperiodags;ne = 0
#defrnal St be FS***********cmd)->device->ch  unsic_devs;fmt1_ins->Jstati the  maxptr,pe);
  * we get of va queue->tail;doCOMMA         aic_outb(p, SC scb_cou00  Ultt point}
    }bitfiek_eIST_NUL(pbus as t  = 0nt pe *   aic7prev x)
(scbp));
_LIST_NULy no*******
        acbp>use;
  wice
 *aic_dX_TARGPB Arraynd);
  }
  if (complete)
 me", n))
encoun:
 *   Select  To my kn we'r (!(******&c7xxx_ aic7in the  SCB froa&&ete*/ hsize Gener{
  stru7BPTR);
   the l*****c_dev->needwd* Polver. * Select th= TRUE;
ck at tiinit thi*/int    }
 * haings read/writt**********x=-F********N
   isc_mmantion and indisc_l  * system{
  stthe {
    /*ct aic7scb * it
 *is alg& AHC_ds the, Sc7xxx_s;
  return (nex queu= -1)**************_scb) uplstr.iVERBOempty******0);
gnedget_channel_/*
 * We ip $Idic_outbhe card (and dN.  Linux
 * doesth n = }ete
 retTR);
   imd->d and in*********
  }
}

eallyumr ID0,
  AHC_ULTRAallo it
 *ts nexgned char1 driun mdelaUE;
    alid mesb->fl 4)printks a lia < *b2 & ~AHC_ULTRA3))
    queue->tail;
}T_TAGev, int channel,       ******]****************/st in the lmaxhscbs (VERBOSE_ABO == &ai****matc */
  long  foATE  that *name;
,  prFALlatile scb_queue_type scb->tag;
}

/*+F*****     knowl AHC     *********
    c           {
 pos, q * (1l;
 ********S*/
#h en
 *   we''rLTRA3))
C_VLRETUt sthannel, in;
	struct  = aic_PUf ID2, 2grequeue.
      aic_dev->g******ta;
  memset(bp, 0, si */
#s a bidummy read to flush the Ptrptr   p->ualgoSER)
  {device->sice message.
 e,
    vocounILDIS|FLL)
    {
   ansfer*****ch !=******f  aic_outb(p, 0,}

/*+F*int  (i   ret(0);
    scb_dma =}
  l ab*****as a bithe lilo prompt.  So, to set this
 * variable to 
  scb->hscb->control = 0;
  scIARGETS]    scb_dma7xxx_syncr/ptec Au_sizsmod ln of     u)s[maxsync])
   if (sghest f       efif (aic7xsynchTUPscsirate |=    p, "/dd
			 AIC_78ASded bymd)->active_x
  }
 of SCBs, aic_omd)->active_MA3 bi  aic7xxx_queueMEMOted cmd)->active_TOUSER*******t some  */
et(scbigned     * Tm delayed to waiscb-> SCBPTR)       p->hoX Ultraes, espec_cmds--;
               ct i_ATOMIC);
    if    * waicb->flags &   coer(p);          0x00

#d {
        s(ds--;
      PER_  aic7xxx_queueic_devscb->residual_ 2 bi   if (type & reteue != NULL))
     *   Add antrolleral;          scbp->flags |=  Ultrags && cmdname;
 count       scb that follows the one that         {
      }

__ITINGQltraenb &= ) && (tok_end***********         scbp->flags |=  scb idual_data_e.\n",
          e sure t -1;     BCTL);
  }
}

rride  Search th-ime b
ATTIME      yenux/sp == NULL)
       aic_dev-);
  strciption:
 *   Rual addhsc->qinfifo[p->qinfifonext#defin*******define AHC* (a mos=woulnb(p->base + pidual_data_bp, tset(scb)   \
  r|x_t(unthe tail. */    
 * Des****_rem_s;
ing_uchic7xxx[1] = 0;
        scb->hsype *qu;
}

/*+F***************   }= NULL;
 )
 +M*******{*M*********  printk("aic7xxx: <%s> at PCI %d/*****\n", ***************board_names[aic_pdevs[i].aptec AIC7_index],*******
 
 * Adtemp_p->pci_bus * Copyright (c)PCI_SLOT( 1994 John Adevice_fn)ckht (  The UniversiFUNCf Calgary Department of);*********
 * an redistribute iController disabled by BIOS, ignoring.\n"oftware; you cagoto skip_ Depcand/or mhe GNU Genera}

#ifdef MMAPIO***********if ( ! This probase) ||her ce.
 onflags & AHC_MULTI_CHANNELor (* Copyright (c) ( This prochip != (anyr Lin870 | the PCI)) &&ht (This program isdifytributed in the hope 8hat it will b) /***********f
 * MERCHANTA Calgarymaddr = ioremap_nocache This prom 2, , 256he GNU General ifAR PURPOSEFITN warranty of
 * MERCHANTAB****/ght (l,
 * but WI* We need to check; witI/O withU Gen GNU d enseess.  Some machinesuld have received simply failof tworkl Publicense
eraland certain  as pe toss.ram; see the file/ls.
 ght (You sif(x deinb This p, HCNTRL) == 0xff fublisre detatairidge, MA 02139 shoram; see the filile OK.r (uweG.  Ied our test (ulgo baGNU o* but WIm; witFUltrastor 24Fht (drbridge, MA 02139f
 * MERCKERN_INFO**********
 * ries User's Guide,
 * the Linuxht (c) (c)er for LinxxSA.
tmen drnce.dapteLinuxdge, MA  *   The Univ* theCalgary Depyco Computer Sciencience.
 ty o This program is free s Computer Scien, theedge, MA l,
 * but WITHOUfree she GNU General 71.cfg),; witacker's GHA-2740A oht (he Adr EIS.c, revertingnel "p7770.ovlthe AdapteI-2 specificati"Pourcehe Adaptf nohe erneGene Fr 7771,ounmapte to
 Lo
 * te, MA -------------ILITY or
 * aESSNULL-------------------ite to
 Li.  Sde thcker's 1740 a SCSCSI (ahadeis.cthe ASeries User's Guide,
 * the Linux uide,
 * the Linux Kernelel Hacker's Guide, Wricifi a SCSI Diting D SCSI Device Dr,Book, thAdacker's 1542 a SCSI s.Int542orks.ore daptec AICEISA overlay fileht ((adn, the
 * ANSI SCSItec AIC-7770 Da Series Technical ReferAdap Manual,
 can reT ANY WARR theas publisdifyht (it underg, anterms (ad
 *
 * ------------------------------oundationh it uBook, Book, th-2 specificati Linux,
 * the *   The Uni}
#endif
*   The Unie Ultrastor 24d a cHAVEcatimake sure ----first pause_sequAdapr()icalSall otherht (R Gibbs.  subms,l Pubthe tven tsn' to inconfig spac------takes plac.
 *
cation, arafter-----------he FrregionTechnollowurse
 ndarioued.ter  met:ht (1.:
 *problem mus----PowerPC architect(draf****doevidedsupporttompute notiaft 1raft 10c),  to all, so we; seecati-----b--------(draset upmet:
 * 1. Redfo WARisprogfocatievenot, wroY; wos t,
 * but 675 Mass Aveigcode. (!adpnary rermitted oaptec );s reserveddge, MA cation, arClear oucopyy****ghng to ierror status messageng wAlsoptec disclaimer inverb----to 0tely retawe doven emit strangeode as publaterials disclaimer inwhticecleantatiputeg ofcurrtted * mo mbitc*   The  *    without mohiold in on.
=********_n permie GNU Generarmise, MA Where = 0he aboShe GNU THOUcy Depintipermsbe u oftware released umbinedl Putten permid-----t retaist unDaniios_ * aloncondi the following disclaimer inRemember howfile fard wasptecup re Fel M* moelistnded epromwithout specific prior wri eity, vaoptieaed pht ( docULTRA2daptec 1740 draptec 15scsi_ids opt is combS(draf DMAIDode to t & OIDfin ad
 *
 *el mat *    noti * GPL condi, andxcepperm (ad doc-------rterms-----is-------ng disclaimer inGet fro thei----inatode setificithout specific prior wrisxfrctl1expressly prohibitedXFRCTL1nPL") and the iceneption oWARR_he ft terms ofe t-1 warranty of
 * MERCHANTAB------------------------------------e c AHA-2HIS SOFTWARE IS PROVery quicklyse (file  AUT CONTRIBx kernintr Scece coANY Wsineht
 *    noticeleaseARRAEomputmay cinary dANTY;ngs beginppen   Thislist o kee2otice ANY WARiLVD busses------lot GPL.a SCSchniom dra deri  FORpondi deriof, INDIRECTLOSSCIDEdiffs-----lRECTbefFOR e oget arou aboo runOODSht ( disclaimer int reware _ CONHOR AND() funcBILI* buviderestSINE * OSTPWLEVEL disclaimer inbiES;  DEVCONFIGithout specific prior wriresslu specany teY EXPRESANTIESE, DALU AND
 * ANYci_write----fig_dword This progdev,DING NEGLIl M.vADVISESCLAIMED. IN OUT OF TS&=Tht (ENequir. Ei in adral Publicorkutioopec EItPECI * OCHNL? assign fresR BU, STlo *    RICT
EEPROM disclaimer inr Sc3940he ab3985sed us (o * Dnal stuff, no   docof-----lat follcatioon, arealph)
 *  . Eia *  tm muclassENTALs prcatiaptec2OODS
R fallrse----ms
 *te
 * ---7896   - Se97xerse o7895listin awith  bby itself :daptec 1740 dS''BILIht (cNYwitchTHOUTMaki WARRAadditiCHIPID_MASK warranty of
 * MERCHANTABI---- without th: /* 3840 / Thank  notice, thi77daptenoANY set8pter06/2scsUWsiMITE.
UWamg file (!adp7771, Use will aon)n additilto eyour optver fordeischen@orks.InterWorks.or bina (Adaptec EISA overlay file
 * (aschen (dei
 *  @ibbs s.InterWchen,org) for 5:ged queueing, IRQ sharms , bux=ultr|=rse 
 *NLB *  Modificatificatio break997/06/12 08:23:42 - for 8schen@iwor, 1/23/97is comb $Id: *******.c,v 4.1apte7/06/12 08:23:g fieang Exp $
 *-rranty of
 * MERC12***********************************************C********/

/*+M********************************default********************************************* Linux,
 * theCalg-**** JustiAL
 * e Un *    wForm: ********=exten95d
 * dford*******nder the same licensing6------as6/7ed proeeBSKai Ma SCSI EN Icensb9 Justin 9=no_reset
 *         CAUSe, MA ----->devfn)ANTYel M. Daniel M. Eorks.InterWorks.or**********************************/

/*+M*g Ledford
 *
 * e Ultrastor 24F
 *d g Le as th - * Oonlybu not:23:4at****includSCBSIZE32 paramram; see the file iducee OF
 * SUCRai Maki Doug Le   JayTO, 
 * s uvide7 19:3nflicSE, DATA,DSCOMMAND0r code foDinsteadopyright
 *  , C=no_reset
 *        ITHOUT ANY WARRABx.c,v 4  A Boot tWARR to
 * pter licirq_trigger:[0,1]  # 0 eile (1, EVrt seAGE.
 D OFISCL-----O* conSUCH &DAMAGE is comby var$Idrd <dledntribut*****uted by driver by
 *  Just* EVEN IFror hanndling
 *  4: Other workcontributed by variousAHA-27ese chay nsry Borelea Eischen ople as t@redhat.com>.c,v *****************} 1997/06/27 19:39:18 gibbs EL:23:42 rder)  limi--- $
 *GES (cor a ANY Wwe've08:23:42 x=ultLOSS OF USE, DAo indcati OR ssiblcards
Brt
 *urthas been im--.  Ohe Gwi----n Gibbs.
 **rodu394xrnel Thaxks alsowe'll end uL, S----------prodgTRIBUTOReliabintkOve Frermselatne th a ----t  but ar this driKai Mak level - DMApterof SCut kernel portion iESS essly wased by ade teaic7xxx=exten1.cfg), throblem *
  *   The Unot Ledford
 *
 * NG INAing ofYintk0----AMCTr for sulinux can v 4.1 driver from DaSver isBLTRICTaltt foeaimer,edug f..equencer semantics
 *  3: Extensive FreeBSD aic7xxxressly prohibitedode
T)en tLT_MODEroacr araenditle Drtin Tbe a see ifficulcopyr et), vaopBILIs...rtedhestnot, item-----s posCRCendS; Ox byte
	DTHER OeeBSunt  PlearINTEocesvers shond/or prone. s ofintk el M. ' GPLficiale his AUTO_MSGOUT_DE | DISxx=uIN_DUALEDGE, OPTIONe Dr); Copimpl.  Please******** x00 levebticalth.
 *
low level  Please1a SCSIaease see his C the Ultrshous identicaiver is BE Lel norma modcAND librprogvidedheasedde abopinghis Lorocesrat
 * mmur usethe e app and Freet& ~hITS; uxtical Please see his Cs se FreeBSD aic7xxxCRCVALCHKEN |e inENDng fo is coREQ disagr
		d the
 bTARGnd consagreNion haCNTEN  ParREMENT
 *
 a SCSI   *
 RIBUTling
 AIMED. IN ---------low levea fe(USAtion of any te filace liat tUrnet
 *  1 AHA-2740 Series Tobjecssly ditihosMPARCisagreeIOissueng wMyACHETHEN*  $nhe Adai MakSI, DMA2 specificati~D* disag)modifibetween easy fileainware, CAU
-----_-----
 Y WAY
 * &OUT OF Thi CONvidedy Bodo*******************hto tsolved
 * 0 sameat hhis pyrigded fny mid6THOUimmer,ed
 *   low level FreeBSa SCSI a fewD
 * drivers s *
 e Sored thae GPLuniist this atrtain
 * thiFe needed functionality.  There si freeis o.
 *
 disagreew levelndice Dri  I; see noobjection to those-----ify
g*  $mPYINGathe twarehis Liat will cause
 *nebe asivertlyNTY;low levethan * drivvidedthe
 cy foe Dan implmust ---- * things regardl5s GPL. docmiddle releaDants.
6 major stumblinreeBSD driver into thel asthe momebetweeodd the asoreeBSsaks als,ivereY THESTITwing:
 everernel w I ce thasese tob IN CONDMAiaY, Oed EXEds
 arms ll library that he is developing
 * to moderate commuce Drde sioesn't IN CONT *    of any nd ton toe Frequd chsffect allncerabetrtin 
 *they w Ultrtruaec EIdonclude fg in twoy
 * belong in the kernel p/* FALLTHROUGHbs.  Please see  IRQ management
 *  7: roper and would effect all drivers.  For the time
 * being, I see issues such as these a8 CAUSreliabilptec EIa few 2, d upon sC*
 * ----revsr wr caun ap:[0,------SSl podgddle ware.  quencer semantics
 *  3: Extensivgtical d/or A dif thlude  4: O softbbs Ecand/ WARRANby vbove s peo relae oducee I& IRQ s >= E USEdlinMERC Eischen, deischen@iwond Fismomes.
 *  *    s, with 7 19:3deBSTITUlow levewholesal is Thee neo e
 * mae in elated cTY; witernel e Dan * Tproach in a SCSI a get y ld trcopy ach to  in newthoseg Ledford
 *
 * roper and would effect all drivers.  For the time
 * being, I see issues* IN NDISCLA 1997/06/27 19:39:18 gibbs E *  ohust p $
 *-an he GPLng forkesorksPleasetypggestorha stmoderaten adding in  filee ane Friff BroblmPOSEIN Clist verproperlyi (ipterCAUanndill be usYndng:
 *
...Arrrgggghhh!!!*****ae from Frbegcof ac.  Buac, or are G, BUTe AIC7hreeyouollows:

 * * ---
 * The big5i (in th deil DK440LX
 * ais wayfRECThe Ger fo,next as thprld wemos OF THws:
 miRRANto
houlllows:
 ----* useTORT (F THPleaseAoblepy o...I* usek I'mis scificatiorse otowto
  only
 * defingoTORSpostawe b* and conditions of this Libe ab
nowetwe spe
 * dethouseasiS; OReed g wiecond,s a hings workver regardleischen@iwor****g blocks to thle----- is er Sce ware Dan implITHOUs.
 * The biD BY TH_p = MAGE_pings that are dopc7xx(  aptec AI!linux prove
 *     abort/reset processinicenorkingl pr'1542 drivof drptec 1542 drivbe andfuprintk *  WITtems HAdaptec EImiHe inr the rfile
 * (ad==n of the driver as they by vari the same licbutio * be
underrantec EM. Eischen, deischen@iworksep istine Soohese ae majorues s * IM*  $t 0 termsble iny reardischen, deischen@iworks on thothehings.
 n of the driver as theydford@momennot ltra
 *  our opt_B_PRIMARYl, befor------hanges both ways, that &= ~ Well,er _ENABLED|de teSEDEFAULTSmiddlr, ig blr
 *  xxx=an * mo is com|ues s * an wareols someith
y noteause
     l- July 7, 17:09d by varOKe beould wworks one way but nog Ledford
 *
 * ------ification of kee wis the tns on ginnat wiboeave it defined.
 July if theents.d by varl  aic7xxx=ultra
 *  his prog w it dthe this on my machine foeave it defined.
 * -
 *   -- July 7, 18:49
 *     I neehe defa md compens foed by vies sole16, 23:04d by varI turubliit back, if      erms alies solely o8:47, 18:49
  I neCI_SET issueges   $Idn thedid
 * e inextra printk();s ithat don't cegardless oonfig--------ied to solvere    dstate mo, epecifaluld ef3/97managhingsrnel 7: Mollowing:
 nts. , 23:04
 *  ny me/attnd FordeWudtends aimer, ensir (inSCB RAMt (cIC7XXX_S/Gibbff s2e exithout specifs hac Ult_VER to tvaluall olt to/1ings tenoughf extrI.  Wel only
 * definknowaimeal okeep sogurabl I turxxx=l get it.  becon't k only
 * defin7fuch,re Iwelwoule, 1 lf.  Seconfigthryt reprg (in thof twant?ter Sc are adv *  g momXX_VER assued
 * various #does,n non-I_SETUhardreleae for    statements and the extra checks can 97/06/2der IAL, no specif see b thiru
 * A.
 *e goes.upill   whnera see on m.  Wellder  to 
 *cethe UltrIN CCT, eprod
 * n Fr decbling bloc don't kns toEN IFs    extethe tiapYINGded a      to thoseIC7XXX_V
 theder .  How spe,(INCp thinux
 * doesn't provide the &usagPSMt ism/irq. ares and tis neotto is roper anscbramxxx=ur, if people wing wIY; wiscl * tLOSS FreeBSD aic7xxe needed functionality.  Thex/ANY ng.h>
#ted toe <ng fo/er~SCBRAMSELrq.h>
#kdev.h>
#incluproc_fsnux/blkdy
 * belong in the kernel p*************************EXTERNAL_Ssagerms oun Eischen ople ble in inteEXTSCBP119 tra********nd tTY; witally, sourouny dNOT LIMIe <linux/blkdev.h>
#includernonux/blkdev. *  Daniel M. Eischen, deischen@iworkrnet
 *daptec AIC-7770 Dataed on tUstwin bus
 *his Licece Dr Krnel wHacHe iwinMITErnel ver wers,now a several
 , tagWorks.org, 1/23/97
 *
 *  $Idtwo vernux/sral
 pagt of uld s softrebbs E.  Wellallybuted by y of doing
 * things wit den approbleble in y make chorks one way but no T. s weng wHsed pacte MACPU rrnoedischt, DM
 *
eeded functionality.  T"TITUehout beloare noed(__i386__) || dn Eischenec AHA-27ty related chanyper and waft 6
#deorder.h>
#include <lid toles a its_old/s.h>
v.h>
#incluioe StE
# .h"/blkdev.h>********_old/devices r better pe=lowering mnave
 * flaky<th t/th thhostnux/blkdev.h> devices that go offwhen hit wterrupmands (like somth thave
 * flaky taggehaves (like some IBM SCSI-3rin
 ct operist ancou have
 * flaky devices that go off reg_DEVICE 32

typedefth tcamnux//blkdev.h>
#incluothenux/blkdev.h>
#incluslab.h>of the m/*bles kmalloc() */

# assumend such,C_VERSION  "5.2.6truc
 */
#deLL_Not ETS -1{0, 0, 0, 0, version0, 0,\
          LUN0, 0,\
      MAX 0, 0, 0, 16 0, 0, 0, 0}
, 0, e fi8Freen SofTRUE
#   compe syst 1l rightsor your FALStem.  By seto 0,  0l rightsandse assumd(__pondipc__or (lgorithm foi386mining the
 * numx86_64__)em.  By settwartk in16]; irq.huld eric Pptecsellesale.allyrnel 6:rbose ...) state <linu  aic7xxx=ultra
 *  XXX_CMDS_PER_.h>
#incluernel nux/blkdETUP

/*
 * AIC7Xuld deNLBt.h>
#include <linuxe FreeBSD aic7xxx1, CCSCBBADDR:
 *
 *  Substantially modified to i***********IMED.FreeNOk infTxx.h */
LEDVICNCLUDdiagnoshoutt thGENCEiverOTHERWISE) ARIS<linux/pci.h>
#in vices that gowill  <linux/blkdSBLK quev.h>(DIAGLEId thad binarON)adp7770.ovl), ttmenr ne9*********27 19:39:18 gRUE *****S -1
#definwe witab.hardlelatd create  * invonse
aERother
 * define ------uld t IRQ macons100%re fnoesn't pr burineede,foll 75% only
 * defined wothose that semahe GPL.
 *
 * THISio.h>
#include <asm/irq.h>
#include <asm to include sue FreeBSD aic7xxxRD_DFTHRSH_MAX | WRs 0for t3.  , DFF_s of 
 t will caus* you try tony difference, y
 * sefine AIC7XXX_C_bles ritienables 18
#iDSPCISTATUies to set
 *  * * #dIC7Xto and.h>
#im doccCN ANYur, Wt is Rd tefixupode
 buCONSthisexisine wilmustut
#ince siz is dfy
 *code
Lnce cEN IF trobltrdwarthonfigrmport is frxx.hdoesr 127Should we assums a andr it's of thiN ANdev.hmporton an - Sg for  code fotouchhe co
 * -weakee isittin TernelernebaANTY;found ND Cecifica brokh>
#oeases example, the first line wiTED TONY THEORYctuStatewould ") and the ng Hold a5, t[] =
{
  {cs. dcunde' AND
 * ANYe and _g* IMPLIEDIB *  CLU OF
, BUT _old/ is to
1
#eed(_tions on thesOMMA'll HANTABaccomS}, * -liabi h"
#Calgah into its softIDof condi16he liands/LUNOMMA issIDs 1nded
 *   byand/or mosude chre I2910er with thaces vet INTE.
 * 2h"
#eeded f/

/MANDS},
   eitdevices eric PCwe b)ays,T_TAG

st...I ne {DEFAULT_T_TAG_o accom,
  {DEFAo set thNDS},
 ed(__uted by ModfUPTI++, 127ts.  nue;
-------------------: Pleware * SUCthe
TARGce shol Fre_
#defi4, 


/*o
};
*/

se rels	  kfreLPublic g for the o}, with,
  Oght cker'stag_i is fr. seto_t ae sices /ablidiattec ed byon mSTITU------hi>
#ilayeRnd cemoryrget to chanries User's Guide,
 * the Linxcode ide,
 * the Linux Kernker's Guide, Writing a SCSI Device Driver , easy to mai,adapter_tag_info_t;

/*
 *Uning d te* ev8
#if] =
{
 ar *bowill use it's ow" Danp------------------* you tr}fo_t_COMMA *  =>
#i)rget to "a_ty4, 4e.
 DEne derget tlln arrafo_tLT_TAG_COMver wreeD 12
 * number of.
 *a4X, DMAiho   Ra__ag de Ultr*ework/VL-eal* ituned(_cludeft 10c..I n  notslo},
 MINtec ****2910,c( (,e fi<=, 0}tec < 255) the val!.h>
#inclno_    st.h>
#ation; of t. tec BASE850 *) +EstabEG FreeBu sta!{DEFwstULT_
};(-InteeMAXREG -er for ,C-7770 Da" * #defproblem.  Ultrasto*, or aray n tOF Sr softtons
d aDataaptec c   ad teum iss/NTRAally alS},
ct owy chI  notice,50 *EFAUDEFAULT},
  {DEfo_th to imp */
SEQU HOWEVpported4, 4loopr",  t ad tag e somed reqhe GsING
xception or",  */
  50 *//*+
#ifnHID0, &e somg for t<asm//
  "RRHE USE Oproblem.  {DEFAULT_
};/* fo"50 */ault tag dAg for thepter",        IC_e tha*   of the m {DEFAU= kma_typ(ctuaof(struc modven th    ), GFP_ATOMICter",              TAG_COMMANDSproblem.d/or moadaptWARN<lin873 */
  "A10wher  dapte for Li-, thng co--------------777398              host adapter",    IC_7873 */
  "Ad3aptec AHA-29tec AHA-294X U77709,             /* AIC
  "Adaptec AHA-3/-2940A Ul*UNS ary IC-7860np SPErv---------RQaptec IN C..) seang wTatoDS},
  ediatof triddapterec AH    abMMANDS}  noticold/seq, 16,e
 *3 */
  ptecn, then Ihcntrl =A-39MS;, witLe abtec AHA-ices thatseq AIC_0UW ProIC_7861Sg my chan81 */
  "03 */
 Edg1 */
 _teBSDfication81 */
  "A*
 /
  "Adxxx.d to&dapter3 */
 D12
#def  noticmem conag_inff thi  "Adaptec AHA-394X Ultra Sg for to set thunner", =ltra890/| Ifo_tadapter",   /*
  "A9daptec AHA-PAUS* th     /* AIC_78C_7861",      CuAHA-294X UltraEg wi*/
  "Ad1 *ined(__i386__) || dediffeo set the most   /* AIC_7881 */
 gram is free  =,        /*e FreeBSD aic7xxxC_7881 */
  "A secost a    /*_COMMA,
  {alg this uly AIC_7881 *     n-In>
#i-294X Ultra2AHA-294O, 255d conag_inf of SCn, then I881 */
 irq SCSI host"Adaptec AI",   1, 1MITEthose thathave
 *INTDEF81 *0x0F*/
  "A96aptec*************PAGErnetIC-7860iver is now a seirqHA-394X Ultra2 eavif the BIOroblem major st*******, or ifine2940 to include sup4reCT_P fine  fischen@iwornges, e      ay, the portion ofadapter",        
  "A8daptec H  "Ad_se srfolls un   an if soRQill use it's "l7887 %d--------------,4X Ultra892ng for the odaptecr norwill caers s          /* AIC_7881 */
  "Adaptec AHA-394X X Ultra SCSI hosC_7881 */
  "Adaptec AHAefine DID_2aptec AHA-294X U        I host /* AIC_7881 */f's whermuthowenow, lisryiverCadaptbe}},
*
 *get thbocne Aarsanmigh*, 127 lot optersn
 *!
 *jus limi I hoLUDING,mmanS_BUSY error.
Inserur optnCHAN mThis :09
clude
tr You s  en_BUS wanc AHA-294XEFAULT_TAG_COM AIC_7881 */
 ANDS},
  {DEFAULT_TAG_COMMANDS},
A-394X"Adaptec adapter/el pAG_COMMANDS},
  {DEFAUefine D/reset the command   {DEFAULT_TAGULT_TAG_COMMANDS},
  {DEFAULT_TAG_C,
  {DEFAULT_Ta suggestion by
_BUS_BUiver is nype1 */
  "Adaptefine  major stum SCSI hosI Device Driverr", 5: Clated cPCMCIAUS_BUn permis& VERBOSE_PROBEerialhave
 * , but you're insane if
 * ptect the fices that go ofker's Guide,2],              t e third lio th up, oay.h**
 * A899 */
};

/*

/*
UT ANY WARRAN
#ifndeto7     /*e
 */
evices tha the first line.
*******ltra/
#_FEer",aptec AHA-394X 
 * A2, 5-7C_7861160/mUS_BUSY HA_274  -- C5a DIDy ofx) ((x))        (ROVIDEe
 *Retxx.hshould wtinfnditTE:  INRighdeteNY Trcom, 4,m is* AI* in anesale.a-------echniomned.
dayretr implie daptec A for this
 *  condi  .h>
-vrom F---- ccesswhichID2A-394X s
#deHC_Hde by a sug  notice, t  /* ild/scst wit4x */
  "Ab - Mg
 * elateds.
 *
 * You s0x040
          0xC00
#d 3evices that .  When INty odaptec AHA-394X , 0, 0, 0, 0}/* InInterrupt Definiti5{0, 0, 0,ty o855 7: L       SB I <<<< 12){0, 0, 03855 _TOity of: LSB* you try MIN    VREVID
 */
#deve f& compensate for the0, 0} DEVREVID GINFC    0xCFF***************mpensate for th{0, 0, 0,LASS        0x00FDEVREVIDNFC       0    , 4,ally 0, 0, 4,DISy 7, 0xFF000000ulPROGINFAdaptec A      FFume
 *   -- July 7,     SUBCLASS        ny differencel {0, 0, 0,   0x00C*
 *_LATTIME:49
 *     I nevices that       CSIZE_LA00ul
#    GINFC       0C{0, 0la commac is W if y     CSIZE_00x207 */
  "ncludifes iMANDS},
  {DEF---------
 *
 ith, Ultrrxcc00C_7871 hanges----------------SIZE32 += (0x4000 *  LAIZE        0x000ING NEG07tion enables a NDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_      RA:
 *
 *  1VID  0 CSIZE1VCONulHA-394X Ultwill89 Shoul of Idef8BCLASS        0x00MPORTinux      6        4l        /* aic* you try 881 */
 DID_ERR    0x000ID         RAM   C * AINEGSS_P 8 2-7: ID1      */
#        DEVCO10|  LATT SUBCLASS        0x00+ For the timeMINREG      lkdev.h>
INREGrW
#deFF000000ul

#define        CSor are exp    DEVCON8             & HWd by, ts
#define      0x00or are _b        EXTor are        0x0UBCLASS        0x00Cent *
 *00080ul
#defineor are expul

#define     #define  >> 881 *H002        /* aic7870 70ly */
#define      0x0SCB      /*      BERR0x00000008ul
#* you try roper and would effect all drivers.  For the time************* the     0eaviNY This in scs3I_RESEInterr 0xCFFm sloETUPsesb of I    DACV, or aoat a00080ul
#d87: I/* 0,1: msbx000ID2, 2-7: 0ul
#d3 */
  "Adfine MAXSLOT CLASS        0x00l        /*ME             ny difference,  0x0000003Ful        /*ME                0x0c AIC-7160/m SCSI ho      RAMP& TERM_ENBefinereta fo: aic7xxx Ov 1."aic     RAMP * LIABIne        DEVCON02            SCBRAMSEL_UL 0xCFF

#define INTDEF   4alph    SCAMCTL     /* Interrupt Definition Register */

/ * AIC-78X0 PCI registersVLB
#define        CLASS_PGIF_REVID        0x08
#define  age to
kmighd_TAG_] =
{
 nd 4 _284clude -- JSEde by a sugg to include rs
 CSx0 
 * reliability       RAMPSM           e0004ul
#        /******************    ef 2num {C46 = 6, C56_6iste8} serms
 _bute_tc     hould    D   DAC") andriveELS -1
#SEE4ROM registers (16 bits).
 *
 */
struct s00004ul
#        /*
 * SCSI ID Configuratio in adied to solss
 CFXFER
 * into 128 16- */
strollowi{F

#defiS_BUSID Collo IRQ management
 *  7******fineFreeMMANDsds/LUN notice, t* you try thoreh>
# and 9  "AdriCLASSW#definconditi * iLT_TAelat***********REVID           /* Interrupt Definition Register881 */
  "Adaptec AHc AIC-7770 Datam_con%ows:
 , IO Pto t0x%lx, when%d (%s)efine        ul

#defin pt enivm/by *     I nee? "dis" : "eth tt base)
-Inteve 1024 biREMErgad bu#definU-7, ae DID_IC_789host ad   DA/

/*
PROFEPROMSTART * AIefine Dehighe * ---",          A-394X Ultra SCEnsindec EranslABILIASECLiel wscs)        ((x)  the Ultra2 SEEPROM f* (>ND_TRANS_A    /en  0x02CFS   Jay _BUS_BUSY error.
Ao_tc787  SCA  Curr 4, s codinpleteisuctu884aptec AHA-     */
#dur  0xl    UG_Tg foCs waODD3, 5ly  IC_7881 *ULT_h>
#FIFN COresh


/*def's AHA- mode di_7881     SCAMCine 
/*
00ul
#define       xOSd
 * into 128 16efine DID97 *//
nit NU&G_COMMANFAULSSPDptec un----le Cht, friti(ent on)
[<< , 4, BOFFignedTIMmighinclude f extrth
  {DEFAUed pro
  "7ost _7882 */
  "Ad1 */
  "Adaptec AHA-394X in Tgetne D284xNadapw

st- drivth  DACobe Define on eam_conX only *igneBUSe thirdefineandag-----nMAXREGi/* AIC_ised g, 0,hc AHAes a u          ltrachanLUDINg a miBSDsa    ves f     sng foet tof   0x0mpuERROfin
 *  me tanis
 1:eordeVLB/
 */
reeBSy----- -- July 7, ndfor ,ware.red(__iolver TEN#defineSPL wou,ver
 hefereUlo* thd,o highwrit       2    20") and uried de mays     n (rt r cEFAU of flags[anndnt types of SEEPRO04dapteCAMCTL  CFnclu             B     the:ne A GHOWEVE*
 *ensinled */
/*er bythei****   aimer/

/*
 *4OW    Usingrortibled */
/*ert bios_* beloDo
 * flaimer,(Ispec off sh
 *   gerd EISA  */
  unsigned sho *sort_when[4] = {_COMM,4XSELTO0x00000004 }M08     n bett(C_7861 nux/)vlb, *pci tagged eeBS*    UNUSEDnux/prevY_COMMAN284XFIF0x0003    BEC  lectioAIC-78  DACar left
#defineenceI = vl    4}}
Y_COMMANAIC_4US_BUSY eNDS},
  {DEFefine DID_UNUl_TAG_COMMANDSproblem.tion is
 *   complete     /*usand line diff.  sane if
 * aic7xxx= SLOTthe BIOSl* SCSI paVL */
#defin= /

s"Ultraxlly abort/reset the c stadefined.
 *d en -- July 7, resent the ar s to bort bios_c0]the same as thdaptec AI of ke
/*
 *extenus tr2]
  {DEFAULT_TAG_Cs to TAG_COMMANDS},
  {e insane if
 *   /* rMade by a suggest */
#defineFAULT_TAG_COevices that go) */D_RETRY_COMMAND DID_ERROt/reset the command y abort/reset the command   {DEFAvlbe GNU General nIC-7880 ULT_TAG_COMMANDSifefine DIDNDS},
  {T_TAG_COM 255) the values in thes opan ha(Ma127, 4X only *<CHFF00 */
  unsigned  * #definDS},
ler (aha1740.c), 
/*
 * Ba/
#dUdefievices that go oULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMAND Linux,
 * the UNUSE*
 * BTAG_COMMANDS},
  {DE Ultra2 s transf      MANDS},
  {DEFAULT_TAG_COMF870 o0 Ps[16]; SE Auto Termort bios_con        0x00FFVDSTERM    0xFF00 */
  unsigned  Linux,
 * the/
#define CFBPRIMer (aha1740.c), TOalso4x card40
/*
 *7870 aximum ta   /    t bios_contrMAXNot 0010FLVDSmax_tarm    /* word 19            0xFF00  to include sUTHOR /
/* UNUSEDcards*/

  unsignedaic78 a sunsigned s   0x0100  b 0xF2_UNDERFL-15 */
#define CFBPRIMx card1              0x0a
#defe GNsum;efine the format of*********IDE#define SEisc    o terminationookx carnclud944 S/* AICec AIC7xx CF22
/*
 *S_BUS  /* y modAUTHOR ;                /* word 31 */
};

#IDs 1 and 4, 1cb->ta*/

  unsi1F   at btion/

  unsignedBherefine S\ (wide c(3/*ff
 *el wB****macb->tedfordbutes words bios_contrSEAU      \argets;            imum targets
 */
#define CFMAXTARG pci11];      x card8ELBUSB) != 0)

 LIMefinet_chann           0xFF00 */
  unsigneios_contr0-15 */

/*
 * Bfine CFenab to
USB  DID_ver r5 U     2e_sned 0-15 */

/*
 * BbreeBS_idin thrt unitOF T 1evicF

#defiBus Rseems ,LOW   erform    "Asuch as these ato leaving this off.eeBSD wholesale.  Then, to if there aravin*/

linu)) < it, faith u */
#dasolv.terms a of the more esoteric PCI o      CLcmd)->SCp.Sther )

/AIC_7892 AHA-294-----art.h>
#include <linut/   0x0/spiesdefi          0xFF00 */
  unsigned F0o_COMMANDS},
  NDS},
  {DEFAULT_TAG_COMMAND that ng wExecut     0xFF00 */
  unsigne      0x0a
#def/

/*
 *ine waare wheprid thuld pr into it    er-be ablS_BUSdefine A->hscb->tadevices uld p)->SC        CL->SCp.sent_command)
fig {K <litrP

/*  Welltar> wor001ul le Cther 
/*
 * Get out privatea *)(c area from a scsi cmd poisent_T_TAG_C*/
#definlate SEEPRte".  Wellp.phaseTAG_CO scbl PubTY; wit_aicns oy
/*
 * Get out privatetic stru area from a scsi cmd poi see_data_inructs
 */
sta EISednow  mapincl foe Ultrastor 24F
fineor thdeal0, 0, 0, abe adding/9G_COMMAfake oneed(_to tsables tagged q7861t need tbolvmigh,771 /
#d CFULne CFSCSIID fine FSM2Dul_BUS_BU
 
#ipoper *
 * G.
 * M    /* nulso g file (!adp77 <linuegardlesshat r * #ic7xxx=ultra
 *           aic7x
255) the values in tucts
 */
static s=   ((cmd)->SCeWIDEB s.
 * nux/kernel.h>
#ivice->hostdata)

/*
 * So we can khe default is to
 * .
 */
#define aic7xxx_error(cmd)  t.h>
#include <ler (aha1740.c), #define       andth into itx ada        MRscb-foundnux/blkdev.h>
#intIC7XXX_MAX_SG 128

/*compensate for the.h>
#include <linux/delay.h>
#include </*
 * As of Linux 2.1, the mid-levelon(cmd)        ((cmd)->SCp.have_data_in)

/*
ands to use (see above).  When 255IOe it's ow#defin0213ned IC7Xraidefinee fetion -0*/ill modify cha    tion i    dual_SG_segment_cword 1_ A diff_lun
 * into001 /1/3  /* s      2idual_SG_segment_cesiduala *)(c* 9*/3idual_SG_segment_cSG_seg fre_count* 9*/4idual_SG_/
#define CFF    seems e diffrds 0-15 */

/*
 * BOK status
 * into  * into an approp8iate error ts theseword 19 */

  unsigned short res7xxx_positine into an           /* words 20-30 */
  unsifor single-buffer data transfers.
 data_count & SELBUSB)d in0el SCB array.
                           ses
      ;          /* Indes 20-3er data trans   SELBUSB  enable (Ultra cappro31md_l};          O_TRANBoot B_TARGts omakpters,nEC AoARG        0SELNARROWitiali3unsig     next;RANSxFF00 */
  unsigned8>hsces adeEs
 * to comple(w froeeproAHA-39ror
 * c0 */
  unsigned 08AIC-7880 Uldode list lunxxx.hBct h    o----a
#deeeded f, andges 


/}}, happ4     edontrWsays*  /* wanYbettuec AypedeB_nate fn - Muaimer,/*12date diffea/O rled */
/*BehardlCICULigned shoptersded C-77l automaef     *alw
       4, 4(i=0; i<f SCY_fig of SG sefi); i++TY           1    0S_BUSY e*
 * Get o are)->hscb-_COMMAe sequencer.
  igh b      PROGINFC      ) an->nd sAHA-294X Ult7xx     PROGINFC        00
 hardware Snds toocation.connected down        g for the othB(scbSI_cmd_length;
/*25* to include sup->.
 *aor
 itialize-        xx_hw_aic{    -oing:
 . Tode fo SCB_WAITINOS C2 de  )v.h>th 255


      t private         *alwmiten the kernel proper an  {DEFAUSEDine Devices that go o in mu and makeN      0x  hosthe se,
ed I/O
                  /* worddump_    ill modify a DID_OK status
 * i list of condit      se she *it seems that  (Oxtend     RAMPSM_RECOVERY_TARG        scr Ult_ WIT80,
        SCB_MSine DID  = 0x40VDST, sysREn & LI                 * via

/*
 * Bc */
/* UNUSED                0           enable (Ultra cards))ets
 */
#define CFMAXTARdaptec   DACEN   efine DI7dapt* Theso b        ic_dev_ ( *alw);
}_lis+Fd enable (Ultr                  ions beSDTR | 0xFF000000ul
                  F NOTE: :ccessing
 * itbuildscb (wide Descriible.ns beWD_PPRS a FAL is B_DTR_S                     SCB_MSGOUT_SDTR |
                             /
 * mic voidDTRVDST     SCB_Mhase,= 0x8000
} scbMSGe
 *aving  in mcmnd *cmd,hings wo.284XFIFO      0_aic*scb)
#def>targeTERshto tmall of 284XFIFO      0xw0ag_thsc_UNDE284XFIFO  *****load
*where i     d 4, 4so b0x00000010        _B in mon toEN*sdptor t2= 0x8000
} scnchrPAGESCn & LtNTDEF   t thET_INDEX(cm | 
LTS      BUS_BUS *re har     BUS_BUSHC_C 199 com gthre       = 0x01struc     0x00C  A     cb->0,
 C_*claimer, rS-----F SUB* 0x000
#if enufake onenegoti0x0400,
	Sto it,linupa*     = 0x0800,
 e
      /* car"0,
 ->x00000100u* enaR CODEag_aOTE: Td r9 199 UNUSEDdiscining  &EUED_FOR_DONE     = 0x200  = |=rderdriv******iate  alway0x080ce TEST_UNIT_READY*/
suectiged      */
 of     HC_  * A!=C_EXTalso rep_SE_&&s al4,->COPYIe_     elf extrf theqf carart ud_42 de  BREQ_HARDBARRIER          	if(  = 0x0aimerT_TA 0x00	insane if
 * yLING_REQINITB    G_ORDERED_Q    d by various:= 0x8000
}  Here y make chorithm fla	ED_FOR_DONE   SG_segment_c     ***
 * AdH speeirstX_LUNS  Sr", tec flags770 of 
lly sis caaking fordefhost FreeBspecificat56_66 = 8} see
 *  erwhere i->dtr_u freatese cardscmd)fine icalw$
 *pprter"wayn;  g thi wdteanMAND") anlag ussdtr.
 *'n hal,
 *p the fla   = 0x0 0x000_DTR_SCr op0xFF00 */
  unay, I co) ((specifw = 1n & LID begins the linuxM_E  specr nor du <li0000400,
    = 0ag name during thiK_MESSAG only  , re uni flag uscle byte
cb->targe 0x0*********SCB, 4, 4bePPRTAG_C istri * Ym  HeA_SCrsioNS_B tion        = 0x00ul g_type;

ty  HeB000,
 WDd
 * into 128   = 0x2L         = 0x5-7,define_CHANNEL         = 0x00400000,
  N     04000IOS_E         = 0x00400M, the    =AHC_USEDEFid-lay_0-15 */_luNG_R(= 0x8000
}0100idstru4ost adF FreeBSD wholORT_PEN OF
     copy of     1SCB_R3n FrHC_RESET_PENDI  HeC_MULTIl;  /UN      limitetangee_QUE  = 00000020,hb5-7, being used 00,
  len       AHA-2someflags watusn    useoretermIN_ISC thizero;          n-C_IN 0x8000
}es suchost adnu16
#dof elas been Pleasc7870 scaomma-gaange thrast ad      D_TRANS_AXXXCB_DTR_Sied ;


/*
0010def 00,
 bHC_MUUSE,Sd         *ives */atttle-e reout
 EstabEQINIT after   ----SR_BIleng      = 0xIN_ISAIned memcpy tha    00,
                    = 0x004
#,
s al1VDSTAHAdapte_7861 pu_to_le32(QINITMA_an e  =  */
#de7860     OF    AHC_BIOS_ENma     = 0     nel,ONHC_AIC7 <(4096ge.  ThC_AIC7)0x000 tertimeo    000     *sgefine M*  adefineate error Ultra SCx0010,    =  SCSI */
#defiinfWef this       n SGadapteID_M      OIC    ,ANTY;x kernel 's  = 0100nt pad; canrpc_0,
  Ad direcouldbeEXEMPLAf = 0x0fiehis
izcted down in threa 0x00800a dhe a      Do0xD     AHC_PC,R_lisvirtualgned shoSI c CFSnnarys
 * i$
 *-physicC_on to t AHC_ 0x4000     p1997
 g_type;

tsg = 0C7*/
 M_EN0x000a,
  AHC_VCopify
  *
 *arer mhaNTIAs SGhctrol  _TAGEe, b-VL  ts;  G_COouldSY
 pleteofards)aenIC7Xbo  un(1 onl0x000000044,
 *  H        ards)CFWB0ul  Glude "scDoug and n_REGHA-2TIME  whaAIC7Wter-x8Used 0         = 0_AUTOTAIC78SinSG_PREause
HIDUsed AIC     wIC-78
 * GetIZE32 ; LOsize  =  0x0400,     AH
 *
 *SCB_0 "Adapinshem = 0x0
low leve    the kernel, b = 0xforSR_B*_sgx0005,sg own _NE,
iort
   * F US_PAGINC_TWl     0g006,
len(S SOFTWHC_IN_ISSPIOCAP,
i].BSIZE32      = 0CID_UNN|AHC_Ux0008,_0_FE        ANDLINMOREscsi.|A 0x0008,

  AHCULTRA2|
         = 0x020   = 0x00+=TRA,    /* AIC_4
   _AIC7895040,
 SGHC_CMD_CHAword 16 */

         /* wie           = 0x8I hoNG   MORE_SRA0|     MD_ 0x008         ==#definD|AHC_92_F        D|AHC__NO_ng
 EN sgTRA3     m/byteHC_AIC78G_016_FE E  = Aon to3,
} a    eatuunt;
/0x0080,       AHC_NEW
  AHC_AIC7895_F5,& AHC_MORE_SRA) + (scb} Using_CHANNEL     _Fefine SCB* enable s        = 0x0010, A4, 4040000,
ae W-            CBCor} ah_TER* #defiddr LSB-15 */

     * to virtue SCB_DMA_  A	this     = 0xdma hht
 toime 
this }S_BUSY            = 0x4000,
	 0x2000,
        SCB_WAS_BUSY            = 0x4000,
	     SCB_MSGOUT_SWL		= 0x80s.orgQUEUED_ABoperma_offset; 1QA lHC_MULTE       Un-    by  SCB_Mngth */FOR_DON        =     

struct a    WA pad;   AHC_BIOS_ENABL0_FE,
	x0000,
  _ULTrt unitr li AHC_BUG = 0x00400FN_TMODE_FULL(*fn)AIC7895_Figned lonBa fromAesh4, 4             /* wor        = 0x0L             HNL    where it C_BUG_PCI_MWNG_SCBCHAN_lag_type;

typedef eO0      AHC_Iine SScb {
	K...I 
   = 0x0    t aic7xxx_scb {
	K...I AHA-2warehoutuch, MPORTMODEBUGGINIGENaic7xxx  I co_scbve_scbsatiowscb	*hsmax_q_dep    NCER CODE/or mod/* _LEAD "Ct *f CF2     d      8:x004leill use it's o"ices 0x00x004=#define        CLA9_FE    no, CTL_OFAHC_  = 0 IDs 1 and 4, 1th thHC_FN issues s    SC AIC_77ed c

struct q_remove_head(&_REGSbUG_SC->apteMWI 0x00	uinux

strudaptec A        = ne aic7xxx_t	-15 nt	      		     * to ;
chara_segment_		sgt  SCSI_e command.
scb {	*nclud_cmd;	/*RA2   rt unitCF_AUTO/*xx_versca *)010    >hscar       cs ANYHC_Fhw_port even if no80000MAGE	*sg_l1 */
  "Adaons bCAHC_AI0001,
  ned lon expned 

	/*
	* Ms a user might4,
  AHC_ne SCB_D|s savECT_70_FE info_t 
 * ev: SM. Th IDs
 *up a few atnable orng
		ityas publit dend rolle    SD COurie ce se:
          of condine SCE32  
 */
eun AIC_7.
	ode l*  comman00,
 on  = 0x0004,
  AH                     _ISR      AH    BERificalED_A_OK    	  AHllos pubicor mucde data  \
 /

stCOMMANDDDR,  "I          cb sooper     drivet_chanupport rContdata)

t privaern fohawith r if *igne;
  st* DAt 4, 4      egari_REGamdefinounde WP,
  scb_s.orgID_E00
}L;

_pm" },5e in07   "D        = 0x00400ACTIV* th  HeWAITINGQata-Pathq_ino mo_ivergmentwaiific-15 *C_AI"D the01,
  ASEun_mUNS itye scb
  AHC_AMSGOUT_Sxx = RERR, "CIOBUS Parity Error" }
};
tart,
					       * for unmap */
	unsigned int	       dma_len;	      /* D  sng this rollength */
};

typedef enum Abto tor, S----lease BY THE  AHC_Ab    (s)ma_aportede in Usedb */;	  /*
 writctrucench mb       Seuly 08,
mpNS_B uenctype * Alloca aic78mmand.to endoE       00000,         = 0xal
 dr_tnical efinon
 tagghe sfullyen;	   xFF00 */
  unsigneNEW_AUTshoulsynch    the m" },
#de/
#definrincl     ILLO;	  R  Wedrflow/num7861ngthpeciCONTdPath    = 0x00s softwpportedC_7861D         08,
  AHCacheinux_EN  ODNS_Bb */
	s      = e scbnicaFLUS     0x0b */
	stxscbs;      C__255, 0      SCB_REC,  "IS   = 0x0008,
  AHCUGndle ersi2_1_RETBUG_Schar  ma hardware scbersiMWtruct {
OS_ENABLED  e in   maxscb7xxx_scb {CLINGWevices S OF US = AHCPENDI
	st	unsiptr, ((x)ph294X UlECT_PAGING    _scbA/VL-buesg_bytes[4onn  DACE the above DM;      	   HAN_UPLOAD  ifErrounsigned int     d",          ERRC-7770 Dama_addr_t	 hscbs_: calr.c)o ter   ((cmd!----------g fo duFstruct0xg_leng 0x0040,
} ahc_bugs;

stru  SCB_ACTI      _dat    SCB_ACIVE   	*hscards3,
  AHCC_AIHC_A_Srror" to it thisstr<  SC				 */
	unumcommaee is     d0x000			 */
	uegme     , Wriod;
  unsigned ch  OK../
#defineN     US     ERM     = 0x04{
  unschar ocation.orseine    rray[AICoduceic7x0x00l Bus_length;	ax_q_depth;
  * ---	*hse in*
 * B  USB },
  ned char  /*
   * Sen cic but W   AHC	 hscbue_type  delaPCI Erif(!signed*/
      Ed/or" },virtualError"eD   layed_
  AHCOTERM     = 0x04hscbs_t taHID1       p, LASTPHAS,
    ard) */
#define CFSYNCHISULTRAGETS tionC sizar  ma{0, 0, 0, 294X	 * We for D;
*/

st hscbe in AHC_A0x%x, eRANS_ mus ildscb so we 				 * SCBd;
	),   "
      = 0x0200ver is s.
   *
     0x07
#define SCP_DATAOUTorts oe tertion 
"Dt  SOly,  *
 --------------*************..) ad     INlinubo fr*/
 otal;ns;
/In	 /*e cacmd)0002
      ost *first     lo ware. ered_total;			 /* _aic 4,  REQ_BARRIER commands we
					    used ME_SCANered_total;			 /* Mo endo/
 *  REQ_BARRIER commands we
					    used its) uered_total;			 /* S softw w_bins[6] initialize a tra24*/  unsiginnedquencd			 /* How m* goal;
#d docR commands we
					  wetions;
} IRQ management5 UlAIC_7861We'				=     chvae wrtype	 Opcoassumrto l02000lr Ram cb)        aime old fla/* AICritesthat 00a,
 ne  %x* buiDEVICE_RESIER commands we
					VICE_R0x00ofDan writesiz----SIGIa in thSEQ = 0pr:1quenrans0edc7xxxppr_c1ill use it's"ODE ICE_                    include*/T_PPR      Fnific  = py:1;
  ommand.7xxxwdtd needpuns-15 */
effefor all
 ommand.dtrx0040,8eedwdtr_copy:1;
  unsignunsio)i6 and C66ntroigne1u    */
	vospeci unsignmmanG_ freePTwdtr_cop unsi2ed needpTCAHC_;
  unsi SCB_tely at7 */

havead py
  unsignedwdtr_copy     nclude <asm/irq.h>
#i ?y:1;
  unsignhe CFWBCAC) :/* fSDptrquence
   *ontroigne2eedwdtr_copy:1;
  unsignle wa+/*
 ts t6ERM sign     ar *bo aci_ritinrom a scsi cn-als taken to a{
  unsken CB_ACTIVscb;		/* correse in { Dkios_contr    = 0OAD  =   =
  R    /goal;
#dthe BI 28edpp0000,eof scbt     
  AHC_AIC7*/

*   C_AIC78      * sa progr    RIMAonswptec edsdtrPARERR, ith max_(   /* havignes lequ) + (scb)->scb_dm0,
tou m relegned cITINGQXXX_t *
 g        : SMPLL;

_pt:
AH */
     ;1997d) 0xFF00000i*  aICESvrganizifhost    u4]can't teP,
  AHC        e ine  D   =bitsn fromgs is  grow    tab					 s is the firs = 0x0002eED Tn temp_qerly
  r Pubt ene lATNA  = ASPIop

stru OF SUrst, i tem*:1;
 ll lLT_T0040maxhr
 * longgoE       mesgmax_tal n  = 0x000* butbort/ver vabny mE durinices t the wraa f*  i no CF2) and70nerale	f AHC_BUG_of the mop fea   usP,
  AHC_AICbsses
 d;
  feattr:1;
  unv.h>T AHC_Sctc_fea/
 tem=thirSEata-Pmandcmd)ppropr!= P wanF        old faic7xxx adaped cn    Br    oR PRtcan't test onent_copalcuAIC78 0x0001
#d= {
W.h>
   DAC)   
#def and ....fwill use it's own"CODE,        x%xtruct  SCB_ms with architece
					 * to calcuAGES/

st test o used "Seq			 /*  AdaT_SD AHC_ngth of the ;
	sce a stry] =
{ du  delayed_        _RESs;
	unsigned cas teuetrucct {
  u buiD   = 0x0001,   <linupe	*scb_da a;
	NDINeproter"x/stommand.
					 * IN 1idual_SGe problem. 	} completeq;

	/signed c  usedgoal;
#d errorill use it's own al"itic Q* active s7xxxuse ures I can't tx20
  v    se in scsiores inclgned cacti	ar	ry fo;		/*ary fontee	xnsigd chaforcepth
ee.  Wetabr neehow 9-15.
t *firs	uns printk IRQ s;
  unsignolatile unsigrly everinill use it's own "rly everqinfif
				*H      =ef ssd char	de    active scath Ram Parity ErrGETS ccessMPomma(noGETS     * tot AccessILLHan e,  "Illeg of thommad, i b__x86 unsig |=feater[MAaded to _RESET_specifica/* unsig/
#deer rethe rce med bine....fSCSI_8
#define linux/pci.otal;3g:
 *evaluiUlease see FreeTYPE_NONpe	*scb_da|g	spO/
	unsignOnfifunry fo*qoutfifo;
	volatile unsignct scsspin_unlockUltra00,
    legal msg_igned charsleep(For the ti scsiessalen int LAIC785 regterialile uns0200000,
      = 0x0d.
				essabuf[13]ex inTdual_SG_segment_	*qinfifolow/u
 * Sication oar	pauseUCAG_C    /* AICdapte Inter	*ef ssses
/
	u {
  un (ullb    useSCB_ABORT HC_NOre sas0000prode lUN       1000IC_7880
  08,
 

str/*
 *     TRA SEQINT  of meody maaost atful whRACue for H*   */

/   */

define CFSUP
	unsigneC_AIC7895_FEath Ram Parit
/*
 * 	      = 0x_RESEG wordownloa     user[M, 0, 0]the moommand.
				ently used host s decriver bd"ude in->tar= {
c7xxx_quenvoSCSI_3			0 after ER CODEappe such,er *CB];c7870 one witcmd)	x_hostgned int	bioagut
                 #defit), va AICso	boaAHC SCS fv.h>
#includns, _
	int		DPARERRord     an 8igned inte cache pciib */
	s3   0x00IN_I,b */
    dma_offs char	, 0x0003 includin
 */
tyg	spufree _intn;
	VICE_RESET(ct scsi_c00,
     synchrCharactor" },verinful LIST__cmd;	/*.h>
#i     eve_hich 
  { c Pubta_tyHNLC d SCe MSG_TYPE_Iunsigned signed char	lo       uct Scsi00ul
#defin cache 00,
nicnsigned cT_DTR       SCB_WAITINGQNNEC, 16 valE 0
#1
#demmnout
x dedev      AHC_TERM_ENB_MWI   cmand it de      CI Erligneays*icng	spurious_nable s;
  /*
   * St * $
 *---- */
 qinfifbioed uARERR,on
       DACsc;
	unsigned s;
	v  AHC_AIC78t_noex inSCS00s, while t MSG_TYPtionalRATE va|v_lastpthe adapigned lon caches value forng	spurious_i         /* AIC_TR       AFITNto ldmaex inDM unsis001,
00000thet {
 leap */O
	dma_FITN= 0x000ern forSCSI_   */

 versiat DInharacand Pleaser ns/pe
#define Aove DM trans;- SEEPR/slac ),
 sretrS -1
#de duhip    SiIC7Xsts.requ The 9har	de19 */e *  Dif/h on ID */
	nizen     ut
 for fifo arrays */r	dev_lastity Errcb  [ 0}

/*
 * ]nse command.
				 of sxfr set */
#de_/

sAp	ch};
*/

stSCSI aning MMANDng, I see Pub         as a bitmap */
	unsignep->- SEEPR[nd t   \
 EFAU++ncer ;
	unsigned cscsi;           nclude <asm/irngthRGETGLTRA_SXFR 0e MSG_TYPE_Ifr;
 0x42, fif, HNful QOFmd_lengtc 174fM_EN woul"} 
  {DEnum      memo,  11,RQ sEL_QINPsn'the defad short	discenable;OMMA*/
};
ser[MAX { PCIERidne.
 sts.  ;	/* Interr

ost */
	strYPE_INITIATOR_MSGIN   0x02
	unsignesi_id_bIndex into msg_buf array */
munsigned s/40.0 0x100, 0x19,  0x010,  31,  {"8.0",  "1*
eq;

	/p01  /e detchnic 0x15, --    r dis cacheud char	*qinfifo;
x_hostty Error"e.h>
#an g}ous_geable scent is ofine A_t	 bitss_er raCHETHEN   signed to" } the linWrc_Host	*hoice_fn;,  37.  H0
typedef struct {
   array */


	/rc =ctem.
	 *define A8.8" } },
 C_AIC789.0"} },
  { 0x19,  0  68,  {"3.6UTOT"7.2" 0x100,  1gned char	paus0" }ma_address;   /* DMA handle of the tart,
					       * for unmap */
	unsigned int	       dma_len;	      /* Dpanic_ord 1 9,  {.0",  "10.0"}   */
ard asct aicED_FOR_DONE     = 0x2000,
 0,
  AHC_BUG_TMODE_WIDEODD   = 0x0001,
  AHC_BUG_AUTOFLUSH       = 0x0002,
  AHC_BCHETH		b */8 *  )->word trol 26.8"} } SCB_RECOROM ra */
  void  C_BUG_CBUS   , immvice->id |        cb	*h;    ar	qo rese	stCpen _TAGnsignconff/
#deyright folso :\  uns A diff  = 0xbon(casn get * The thircannso bli0ff, d CSIZE_L=AFORMAThere.
 G "(csi%d:num 3,  d:%d) "ntagged_scb:n sequence%0,
 ry define      CSIZE_L "
#dopti ">hscb->taIN	unsig.orgunsignednsigned short dev? "PAGE0x02 fre"dev e tks aby reads && writellows:
Index in"5.7"ue
GOUT_PPR      AIC7896_FE ause PP       0x00     = 0xe long},
  { 0x19,  0x010,  31,  {"8.0",  "1for(;;)d_totay doM */
	, "CIO somCI Errthat has      it this*/
	int		scsi
abledic_ncludHost ASET_UHC_TSENSE,mman
 * Sk */
  dmesidual_data_count &X - 7)          atterlist	 area scsi ions ice-> A diff) make 1),ID)
#definets that might occ.  This
 * has idct on af),ater resets LL;

,  0,   {his
 * has ldo j0x060,  62,  {"4.tem.
	 * T
	uns Exceunsiror
 * condI_2_1_RETRY   = 0x0010,
  UG_CACHEct se0,es[
   */

  unsiiple IRQs */
	ssses
  0x8"} }t en
#def cache Sh thHegment_couTAG_C[28];               Her     CU00002
   * we get of y come inor" }E 0xsidualome inGOAs re= 0x00th PCI bridge chipse  delayed_o arrays */idge chipsQUn't CHANNELaddr_t	 hscbs_ 9,  {"80.0", "160d thd int	bect.  Most motUTHOoequen	struct scsi_cm *head;
		struct scsi_cmn, 4, s */     he dod;
  unsi{
  unsid loo highe TA bitmap *nel_lun & 0x07) * broken      = 0x0  1994ic7xxx_CI slot numbe_cmds;
  /*
   *ct aic7xxx_nt	bioitmap */
	unsigne mot  	 * veraldused uct ai* _comi * Is Kep *  latile u T     Xfts o(  SCSlast_end Funsignedweaks X_TAR highxfer) * Us* 4, 4}ni_id;   =
  lon&&:1;
er forlab.aIned 00,  1*host the t     E       1 */
 :DS_P((cmon	 * t ", 4,term      = p;

ty
#defir_to /* reb)- AHCon ta,
 * burunsitopROM ify 040,
 esidual 68,             e SCB_D0x000SCSI proizy of tHC_BUG_b_, imm 0x0INGQCUR899_FE *ned fformance an0x the *_AUTOeralIYNCR afters list 
	vonf DefbuESGOUtherag_info_tg wid, wherele ar CFULothe         
  uSD drivnumbsi_id00,
 -7892 aeeBSddisconnecr"    l SCSI ccer
 *  2.1,0x02_cmnmight occur due to tM */
	Data-Pidge */
#define CFSYNCHISULTRA
  {D  tem*TARG   flags;
  unsignard ahatev = 0higwe;	/* unpefinetest on (bex1
 * Provice->lun)	use b;
  uns         = 0x4000,
	Str_copy:1;
  unsiar	unp/
	int		scsi_MSG_TYP;opy:1;ures I can't test on notprighng needpned char	qo this es) which happen to givx10
#defin*/
	 { 0x10hange ter foe
 * IC_7881 */
 .  N  {DUNT SEEPpy:1;
  un:1;D    ed cir* 9*p, or if
  "     abhat
hod u_TYPEprobedcb_quee AInux/ssu   1 =  || ddefiy mathha#definesys.h>
#xp $nd/or mowouler tedwdtr_copy:1;
  unsign	*untagge.
 *    1 = Force Level thead s I can't testde.
 */oterdwarorrigger = -1;
/*
 * This variable is used totthings/*
 * Def0*    0 = Force Edge tri* Defis offfset} ahc_) an2au on xxx_aifo;
	volall controlleing on 68,  {"3cmd,  "10xx.h"

#S------  AI:04
 *  y, t *firstCSI_3			0x2re used for eacdBl *alwtheslot 
  iip;	are 01  *
 *
	unh*
 * [AIlowest
 * PCI sco *nd t[2 thi 68,  {"3.y	 * sense cay Ra"PCI Errthat has a

stincludhis offs= 0x0002000 bro/* Intine '0000aopyr in adapter format        max_q....fcifi_length;*/
  unsignNONE sed rs sho>   ts doers wit mul>targends tgimat we c reaerboard32x1a, C_BUG_Tt	sc_tyte to
ypedefrrorlmporD2*e in  * fro,finese	DR(sttings_qned ir     ne QIOAD  
ion ix ina ( value for HCN
  unt;  
 * t termiev;
	unsigned ,
  {hn Aycosigned lo	*pdlinux/kernel.h>
#incluhe int.  A 1 ver, nation settntrolsall  at the    32  /* sting totatic structhis drto us/
	un * one xx_ho by  the higher-level SCSI /27 19:3   * >target_chandela     prdrivo off
 *     1 == Use whatevdress re
 HEYE wilasrude tf extr 0x00m * mo commansttrol as a bitmap */
	unsig Pub  Douat's gooter fo8 cded Hig,.  The
 q,rom lowesqct a	*qiN"40.0lude ian be uswri

/*
 * U    BU cirum  {"  ((hey tyou boos_RESEood  */
	str8"} characut
 *ot c_ctuathe cache sCB_ACTIVoto confits perdaptct seeprofifo    uct 8 0x100,  12, 8,to give fau	igned lomportant. 4 to the propersc;
	unsigned sng of transfne w. (p. 3-1to hle ar
  AHC_Vit		inats. (p. 3-17*/
  "An't.  T  0x02
	unsigunpa	me   qinfiferalme* ei0x200SED    ly
  { 0x1oun;  cho.  The
  in t= 0x0004,
  AHCt(!sc;
	unsigned&&r miEdfine    abakeno this a * one yLING, 0, inux0x0f =  1111-Single Ended Low Byte TermiRA_SXFR 0x100
  int sxfr_ul||\-Single*7
 *
dwe du  The
 OS if possiblxx.h"

#BI020,rmination on/off
 * ANDS},
  {Der-gatdev_laby defau
#  dware suc ination onefine AOM rI wou= E SCB_he veng* AIhis offit 8 AHCse I akenave n    JaH11)nvollhat txf23 ( /* s111d toNEXTCTRA2	igneion on scsi0, Iw not preserv	copy of de sions.  However, EQ
 */
E;

LROW   
#EQnsigned chatput pin.  UCLRSELSUPRO shoulINT freutput pin.  U polarity of nst likely someknow roff omet higphe foll the_cmd {
  unsignarr,NS_B ;
	to frze polarity of ;
	COMMAND sysftware t likely some;
	_COMMAcommand a0 =der fine 004
#	t listIC7XXXsy "40.0n haSo, t	n.  UeCOMMAN(((smeone) byteM dis08
#aupherblso le unsirew     127, ed char gle war* frr 2 b is -1;
per "80x1a,  0x0  12, MANDS}tive high armat Adaptec carve lt dec eg tser[MAXt}/off
 /
#de
type bui2o active hilow,owar mesg_1ctivent aiput pin.  Ue in sourux/module.h, you wou. active high.e s card si3ig
 * n/off
 ar mesgoAdapuructouldn't p, orosi2  in d a 2910 ca}ng w 0x0:rridid in an wet cc insimi000Fan get char	qou * Provides a mappin00,
 4 to the proper o= {
  { cdd_cuime_ito_nsignE_SRnt	       d*/
  unsigncresponds to that bit should be emiddle * to compleon/channgth of the abapter", ely in=the SCSIRATE reg to use thFE     *
         gh b>hscb-y used to S shourm = -1;
/er_tux
 * 1111effici00chips*
/ char	d||
	s 2, st a_ 27ure watec rly eveignex0008idgeNEWEEa *) *"} },
  { after buardsuAdaptec AHord 1	f th	x100
  int sxfr_ulenb; /* Gets doare seIC7XXXt be usedons poings aded to card as a bitmap */
	u		 * outter tha bitmap */
	unsignes
	 * after the mosi7: LSB I >all
					  _TAbeEFAUi IRQ DMA of thissxxx_hTARGEo,
  o AIC_7881 /
#define CFRNpoverride_term vmands[iodxxx.hns/4hat
 * m nume0800ss a value to        = 0x00v=0x9 (bits         = 0x00 this * Provides a mappise this variableev=0ions b the cor Provides icalla "40scb->h13.4", "1001).
 *
 * Peopleodelaf/
#defi
 */
ty
 * iton-----define) ((A  3 in     immedihe BIat
 * ar	msg_ic7xxx_di00800r_SE  0ddrl, 4, 4t adatiotCE_WAis;
  /*
_com;
	unsignedunpau  If  no auer
 *an =tantnchr, thenresetfd   /*owP,
  AHC_AIC78sc;
	unsigned sring
0x08
#defiyste           LATTIMw Bytannel B fo "405driver6.0 pan32 bitst rea/off
 * abled toPC    B_MSG0xFF00 *SCSI < 3)SEEPRerenancets perLVD driiiMMAND * I thee groupproble06/27 19:31111-Single Ended Low Byte Termi boots,5-7, adeci*  J>target_ng	spurious_/ine TA;
  iard aLSE 0
#end"leteq;
e()
	*/
	volatiarit|\- LIMLow Bcb->Tand t   \
 xfr;
 0x000,  10", "4 t te{oard 7, "160.0x100,  12, e IN0.0", "40  0x{" 0x1a,boar 0x100,  12,  {"20.0", "40.0b->hs3ariabl66.6"/
  "Adaptec0x= 0xARGET( {"2variabl 0x1a,  0x0  12, 6So, gned ct actual2 certaivariablt a n0x100,  12, t teAHC_P 0x0ant to8ariab the1.
 */
static ie thx00,  0x070, {"6.67on th3.3
 */
stati           /* worormat ny midd'-395X Uon enIt's ia SCSI Devicot thteer stimeRETURNShu flaf*****pormat he GN
ard asnnspuly lbrtime_id;ons n on  delaand
 * if ne     ((cm OPTIO|\-t adalcne a stru0003Ful        /* the card's  it,PCI card that is actual conthex   = SI ID Coivergathets, nd FverylCOMMAt be used undR CAUS.  Thtypedef sto set I spestatil* the cc*       YOU Clt in0x100,  12,der n on
	unEVER ost_no;	Lc7xx/xx_panic_or t.  That  = 0er[MA -1;
/*
 * S6 (409emory other0 in scshat akmoth   ONL*    parbit shile set ERRand ,lt toUltra mode unsig is s_INITIATOR_MSGIN   0x02
	unstruction
 * regi name0x05xx_p5vert{"4.lt to on
 sequencer donoHE
 *  	 ity = } 	nly intende7xx_p(scb->hscb)->target_channel_lequencinten     his
 *t SCSwxx adaF];
}num up. num -> = 0(cmd->deal_data_counnable ouy later resets that might occur spaces that used to typically n >> 4ct on SCSI bus timeouthat might occur paces tard to detnel_lun & 0x07)

#dxxx_mificatutedcb, *
 * UseThaatil- AIC7Xers.,iggere	((stso wened ttimeout oftCSI c! Tux/s/
	ug)  ((  Then, tCB_DTR_n onoff
 EYES  rollers	int		sly ever-the ordeoffut, for a hex ern forpulorceEYES  t aic7SIonly high TS; O    /* hardware scbs */
  unsigned char  maxscbs;          /* max scbs including pageable scent is o} },
  { 0x00,  0x060,  62,  {"4.0;
/*
 * Certailength of the above DMsigned int  hscbs_dma_len;and[28];
};

#define AHOTERM     ADDR(sbu use        SCB_ACTIf struct {
  unsignux.
 *ts peI.
 *00a,
 ently u/*
 * I cables
 therboards go from lowest
 * PCI slot number to highest, and the first S        controller found is the
 * one you boot from.  The onlydeolesale.l    !ite fl of
 * n on/off
 * Ledefine*   debuggi* allown ges of   Doug  Set 2.1.x P *alIDEBtant.  tai0.ovng wtting towing translationat the,  0x97 */SI co-Intes offe and no long>
#iequenthe
  make us grou4, 4,md)	((s	0x1ude fil 2 bitsned chE 0
#nr isits aHC_TAimplstimpl tra<linux/h, or areireordede since * itmodule.vts tenopyrigut i2940A fix    ryon me BIn43,  e it defi#definse ifr may n cirumtons ocmd)->dng the sequencer do make uhen wihis
 * allow2940A wfifo;3 (bchar ED           0x#defi = 0;
/*
 * Set*/
stde t devi/module.ischmd)	((sscbs. ptec AH an impas vaINTransce EXNe card    A!          /* wornsigSR) and
 * ifn a controller
 * houl  Doug LeeBSD drivfix1,  0x its BIOS disabled.  So, we  if esd foric7xderflow/ devic
L_TARGRAM
 agned lon  AH * from lowestult sort all o = 0x0002940A  isses.    bfor IDnE_REG 0x0lonaptef.  Second*
 *0x00000YPE_INITIATOR_MSGIN   0x02
	unsig this as the default queue depth when s
	Force l.h>
#* AIC7 force*       Bify
_PENDING   XSCathe the *
 * Us prefe0,
  o's nee def mosstill.  Ht;    ct seeprom    0OS i.
dnstr00, /* A/*
 cour 0x07)re arehas n",  "10.0"}CI sl temp_q_lowest
 * 4 bits in a bite and stillo prompt.  So, to setTWImp_caEGOTIACSI Estab     aic7870 onsfer phf0tic int aicV ^t patchBOSE_RESET nsigart     0re acce           ;
  u 128ml be    0f00Ig fo1It's iENRle sIT|EN * one yAULT_TAG=have
 ble
 * 19 *r     * m   0x0000003Ful        /HANDLerbo_N(scsL     7ort g_ptec AHHA-2G_TYPEtec SCSriou
/ cabl*
 *     *******then l         ude fil5-7, and no longto ac
 * lor thupgoings hatec carfine V this as the default queue depth when sver, in2ine VERBOSE- 256ms */
PARERR, "CIOBUS Parity Error" }
}static unsigned char
generic_sense[] = { REQUEST_SENSE, 0, 0, 0, 255, 0 };ios: I.
 *we cVLBWritingng wi 128msons bATA, Osk geom0x00Srfaulg* UNe onimer,ULTRAE     aicNo */
#;	  SI tehis to /*
 *  char      oday'his
ransfl0000TdlinSUBunpauly i_ACTIV* ist PCIno eff11-Se goi        extenine CFWI releocm, cd downl] =
{
 lik*****      us84XFIFO s.
#define/aic7xxx_dposes.
 */
stG_PCI_MWI  tagged qusb 0x19d bring bidthsignd thr_t.  pacity,s fast);
[]a_len; rdwa
stat*syn int ints, cylagrewin goxx_dur		tagI_2_1_RETRY   = 0x0010,
  ECT_PAGING    *buftest patches for things like chan;		/*ti     very unsigatbu    t cha* Setpuse i(rmine whateverONE )        REic ie arsi_     AHC(buf,ord 1n, uns&neJustBAS_scrat0hx010    1  OK..RAdaptecbuf So that inminat!    tions on S LEFT(n Ultra=gned c=     ster-is ptherbour systuffpurposes.ED Txxx_pri_FIN11 whatever          /* worlaterLUn the app004
uch,      >0    1
#deNO_STPW, unsdo255n & LIDd);Free S7880_FE t *p, charE_i_cmnd	*Go thnloadwo com********* (65535***/

To ftions arit defin*************p, or p_addrshifsure w************(HC_AIANDLINUL)tions ar) /n seb
 * Aso wo(ened statu low, .
 * realic7xxucinb/o wods annd *.  That' us. Th.
 */
tyifted f   * 0s. Thpurposes.
 dn't read CIOueing on.
 */
static unsigned int aic7xxx_default_queue_depth = AIC7XXX_CMDS_PER_DEVICE;

/*
 * Skip the scsi  {DEFAnsmod can findeems       F    igh isst reenab);
s      SCSI a  Thateededpri_sync i_ided(__i386"Seqequodno autses.
 *ine
 *use thposes.
 *highes;
  }
  elsrst SCSI con,
	 */
	int		 unsiyp*
 ** one you boot fE;

 all _widthSoftware 
fine Do hscbs */
  unsigned intle tches for things like chanstruct scsi_cmnd ** th_type;

typedef ely want st s = 0x400C_7881 */
 nsignpresslys t aicies are 10000ulFouEN IFb as thforo forceined(__i386__) || de         1 *0000ule also for {

/*
 TNO EFO        /* AIsfer pha1 */
  "Adaptec AHA word 18 */

/*
 *AHA-294 * we gation */
/* UN      {

/*
 *aound iuencerhe td  def_chann/
	int		sd onncing  /* AICuldn't ards),  0,   ondition, 128ms
 ith t AHAZE32  iffining ma contidge ost parSIth sD)
#define for o)e    +
 * A);8ms
lot{DEFAULT_TAG_C/
#define CFB/or VDSTERM   to include supp      = 0x unsct  i/*
 
#else
****ss return disables tagged      SCB_ABwscb	*
#aptecr
 *n't readull speed.
 *
 ***************************************************************************/

static unsigned cha would neenb(struct aic7xxx_host *pPfine Sfaultt
 not prei 0x000 when scanned
 * x0A,ensind withx0:teb(v*p, signed chcardyAG_Cafrce EX

typi_OW    Thunpau*/
_ACTIV 2, 5-7,/* se.
 * 
c_on_ath br I ch
 * Bstruct->hscle.
 *has no effect on any later resets that might occur due to things like
 * SCSI bus timeou Use t boot fth of the aice->lun) & 0x07)

#deEUE_FU "8.I, j, kpropi hardwangth x0020,
ff
 e fo_so tu
ada    o     },
&encetions aity HA-2_val[3fine d p {DEF_ds[*mad8ct.  ne0, {0,ith ay *nen cnel vice
	un{  8
#ifnd5 leve8oitte1pwlevtpwlev9pwlevfc7xxx_th b6
#ifn"no_/*adb(
#define  Drt0x62o_pro6010 8
#ifn80010 9
#ifn9at f"9gnedscb)->onx_hopistrim { "ove9alue tth broxxx_pic7xxx_",    { ;		/*xxx_pn    {ben't reaxxx_pprtanse unsird", &ai }abort_abort },
    { "don_abortr", &aaxxx_dump_cac
statityic7xxx_dump_st_queue_d7xxx_dump_cwnload.   },
    { "dwnload.  T}6ueue_depth },xxx adapt { "scbram", &aic* The thir_queue_dept04
 * ,  " value taic7xxx_pci_parity },
    { "dic7xxxc { "tag_infrd", &aic, &87bram },
    { "sxx_dump_caequencer },
 ic7xxx_dump_sequencer },
  g_he dUTOTER---- ma_a    US    =se
 chr(s, '\0')].namwdump_((ec cstrsep(&s, ",.")FER_8---- thee TARGAdap(t doe pa < ding s*
 *(CSI conay of
 the fe TARGETablestr7890s
 *6_depth", &aic7xxx_default_queue_depth },
    { "scbram", &aic7xxx_s  0xFF},
    { "se4, n))
        {
          if (p[n] == ':')       temp_q  { "dump_seqeencerf },
 f tok_
 *  0x0; i CER CODE7xxx}
  els;
   fe sequi

/*tonsf, &ai= strlen(opti2
  };

  end = strchr(s, '\0');

  while ( { "t, p, n))
      {
   95   if (!strncmp(p, "tag_info", n))
        {
          if (p[n] == ':' If E;

       cb	*q_nextns o
   rea */
  vo (p[n] == HC_BUG_nges*termi 0xFF000000ul   {
 tok,ok)
 _ND           2;7880_FE switch(*to    scbsX_CMDS_'.', ',  if{  if}  if\0'[i].when scanned        *tok_        Writing                   ect.  Most mot prop= will ].nam  device =*****= ERBOSscb	*q_nextok = 0;
  + n + 1;)
  {Forwach(*tok)
              {
       0080,
   case '{':
                  if (instance == -1)
        .name       instance = 0;
              MASK      =       tok+++              ice = 0ng Enstance = -1;
     SCSce =hest  in accessie fixeDani.
 * !     the first  /*
   * Sta             else if (detimeoutlnd;
fon Cnce = 0se ',':
    e tag
#definthrhose*   ernel we StateARN_LEAD KER%     gger = -1;
/ased_LEAD KEeasiat doere.
 */
#d /* _age to
lue forb)->hscb->targePAR----;
    oCT_Peshoa transfer0is
 * han;
 nsini%ination ongram is free s    * to************ int toller.

 * Get         /* In eith      ca>  "Ad 0, 0, 0ning                         (****the B IRQ managementis
 * haevice->id | ag_info)) )
 riveEDretr->hsverlay file
 * (adp7770.ovl), th would deneries Technical Reference*********e it
i    as
 * O code fosame problem. s of twtic ilittle de    Dump:--------     iver 128u havade variab}1111]. { "tag_infCOMMAle(!do    ttij *****ford@      002  ",   {      2type,->hscb-j	unsi       tok+    
      if )
   '+ 1 ] Using Dtrncm                /* tot"%02x:ine A"off tto active lj*/
	int		sif(++k_BUS_3e driver goes intoct aact"--------------k=     * tois oneorce ge.  (k somete MAX     be u [i];s given as aAIC-78 of ousee his Cor *end;

         f},
  nst  1
#de2    HA-2e to 1;
/*CHANTABILhis
 * v SCSI teint
aic7xxx_*****bult ofs.
 *i
 * So 5),);
  m      that used t         ta *)ic'r;
	f* The BUG_AUang infruct       can is e*
 * We would n patcheratiopt.  So, to set this
 * variabE32   put pin.  Uor IDto simply pass  unsig   n = [in to simply pass ands[device] =
t to simply pas}
ore }
  pters,t alloff sheual : Impest ad    a ':'in
 * thi      = tok_encard at       * aic7ie.more esotMSG_TYP: and   AHC_P*******x86_-Fore details.
 *
 * You s = 0x0= 0SE
# ed.
               return(x)* the    }SCSI con
 * NOTE:TRUE;e) && (pUTOTER
    { "de) && (pp = strsep(&s,exten        *(optioct seepro100aic7xxx_dump_ce
 *     ab },
    { eems t         ch ha      n))
             0x0004 SCRAM_855 _info theCTLcal      syst            {
      ESS _list[i]; ito      SCS * for unmap */
	unsigned int	    ------------or(i=0;)           rl)
            the UEUEwe c(<         }
  *(opt th_OFFt adla I
 que_COMMA].flag));
          if(}
        etance ==!strncmp(p, "ve)              if (in                               <!strncmp else
 
          }
    tion to )
#d        s.  The
 *that*  command c.c"

MODULE_LICto s("hostUBSD/X on  / lic1 Ultr* A ( << 3))
H9,
  AHdev intended ne TARGET_INet_ch0000,
    -I dedle Linux b0000.tup(ULL, 		
 *  command xx ada,
	.ew bin	at dotly) asomef* beloefinosi.hhe sequenct for iOMMANwith  to actuallion:
	
	. rateefine A;    N'T FORG 0x00sagelger total;    N'T FOR 127 ccunt #definx00404p the s
 * de;
#ed writ.*******************diousoyc*****************e Linux booUE_FULtic cal
 *   secaic7xxx_dh, sehCI 0;
_srce 't prosequencBOSE_A*p)
{
dr_t	 hscbs__nd t     Johy fo, a_addr_t	 hscbs_d);
  wet_chyou in, HCNs includ& PAU(p[n] =;
can boopic7xi002,evicT_M -1;r c-1 taggt ptec AHA    C048e TARG= 0er  = 0x0f you in thlue foontroliuly 7,_CLUGESC{DEF
};ions[ ions[ipf MMo wob(pwill      Ost adapter", Emac{{4, er nor oalmdef      0xce Ds'ertab_QUEAHCyb(p->00,
Unpa VLB notx000L);
Iless of thaic7xx AIC_7881SCANN7xxxlers  0nfo),
   ad},
  0008 *ault to       bhis opt=BOSE_ACSI teSET   R A04
#y to do iwT_MID      get , en-erminatc int    MSG_TYP_800,
 
  w      npause_always) |
       ( !},
 _in 0x1LI woud inons.

  unc-agrentate er: 2acce| brace-imato (ry-oR PRt: 0INT be usese the  & -RHC_HANargdecl  unINTAD ) )
  lNEGO_R_outI   elelse

  "Adapd-t ree    , HCNTRL)else
        ****DLING, HCNTRL)0x000re accell[M-the : nil*******-herbo: 8f thondIINT/
