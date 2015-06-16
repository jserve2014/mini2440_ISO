/*  de4x5.c: A DIGITAL DC21x4x DECchip and DE425/DE434/DE435/DE450/DE500
             ethernet driver for Linux.

    Copyright 1994, 1995 Digital Equipment Corporation.

    Testing resources for this driver have been made available
    in part by NASA Ames Research Center (mjacob@nas.nasa.gov).

    The author may be reached at davies@maniac.ultranet.com.

    This program is free software; you can redistribute  it and/or modify it
    under  the terms of  the GNU General  Public License as published by the
    Free Software Foundation;  either version 2 of the  License, or (at your
    option) any later version.

    THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR   IMPLIED
    WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
    NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
    USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
    ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You should have received a copy of the  GNU General Public License along
    with this program; if not, write  to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.

    Originally,   this  driver  was    written  for the  Digital   Equipment
    Corporation series of EtherWORKS ethernet cards:

        DE425 TP/COAX EISA
	DE434 TP PCI
	DE435 TP/COAX/AUI PCI
	DE450 TP/COAX/AUI PCI
	DE500 10/100 PCI Fasternet

    but it  will  now attempt  to  support all  cards which   conform to the
    Digital Semiconductor   SROM   Specification.    The  driver   currently
    recognises the following chips:

        DC21040  (no SROM)
	DC21041[A]
	DC21140[A]
	DC21142
	DC21143

    So far the driver is known to work with the following cards:

        KINGSTON
	Linksys
	ZNYX342
	SMC8432
	SMC9332 (w/new SROM)
	ZNYX31[45]
	ZNYX346 10/100 4 port (can act as a 10/100 bridge!)

    The driver has been tested on a relatively busy network using the DE425,
    DE434, DE435 and DE500 cards and benchmarked with 'ttcp': it transferred
    16M of data to a DECstation 5000/200 as follows:

                TCP           UDP
             TX     RX     TX     RX
    DE425   1030k  997k   1170k  1128k
    DE434   1063k  995k   1170k  1125k
    DE435   1063k  995k   1170k  1125k
    DE500   1063k  998k   1170k  1125k  in 10Mb/s mode

    All  values are typical (in   kBytes/sec) from a  sample  of 4 for  each
    measurement. Their error is +/-20k on a quiet (private) network and also
    depend on what load the CPU has.

    =========================================================================
    This driver  has been written substantially  from  scratch, although its
    inheritance of style and stack interface from 'ewrk3.c' and in turn from
    Donald Becker's 'lance.c' should be obvious. With the module autoload of
    every  usable DECchip board,  I  pinched Donald's 'next_module' field to
    link my modules together.

    Upto 15 EISA cards can be supported under this driver, limited primarily
    by the available IRQ lines.  I have  checked different configurations of
    multiple depca, EtherWORKS 3 cards and de4x5 cards and  have not found a
    problem yet (provided you have at least depca.c v0.38) ...

    PCI support has been added  to allow the driver  to work with the DE434,
    DE435, DE450 and DE500 cards. The I/O accesses are a bit of a kludge due
    to the differences in the EISA and PCI CSR address offsets from the base
    address.

    The ability to load this  driver as a loadable  module has been included
    and used extensively  during the driver development  (to save those long
    reboot sequences).  Loadable module support  under PCI and EISA has been
    achieved by letting the driver autoprobe as if it were compiled into the
    kernel. Do make sure  you're not sharing  interrupts with anything  that
    cannot accommodate  interrupt  sharing!

    To utilise this ability, you have to do 8 things:

    0) have a copy of the loadable modules code installed on your system.
    1) copy de4x5.c from the  /linux/drivers/net directory to your favourite
    temporary directory.
    2) for fixed  autoprobes (not  recommended),  edit the source code  near
    line 5594 to reflect the I/O address  you're using, or assign these when
    loading by:

                   insmod de4x5 io=0xghh           where g = bus number
		                                        hh = device number

       NB: autoprobing for modules is now supported by default. You may just
           use:

                   insmod de4x5

           to load all available boards. For a specific board, still use
	   the 'io=?' above.
    3) compile  de4x5.c, but include -DMODULE in  the command line to ensure
    that the correct bits are compiled (see end of source code).
    4) if you are wanting to add a new  card, goto 5. Otherwise, recompile a
    kernel with the de4x5 configuration turned off and reboot.
    5) insmod de4x5 [io=0xghh]
    6) run the net startup bits for your new eth?? interface(s) manually
    (usually /etc/rc.inet[12] at boot time).
    7) enjoy!

    To unload a module, turn off the associated interface(s)
    'ifconfig eth?? down' then 'rmmod de4x5'.

    Automedia detection is included so that in  principal you can disconnect
    from, e.g.  TP, reconnect  to BNC  and  things will still work  (after a
    pause whilst the   driver figures out   where its media went).  My tests
    using ping showed that it appears to work....

    By  default,  the driver will  now   autodetect any  DECchip based card.
    Should you have a need to restrict the driver to DIGITAL only cards, you
    can compile with a  DEC_ONLY define, or if  loading as a module, use the
    'dec_only=1'  parameter.

    I've changed the timing routines to  use the kernel timer and scheduling
    functions  so that the  hangs  and other assorted problems that occurred
    while autosensing the  media  should be gone.  A  bonus  for the DC21040
    auto  media sense algorithm is  that it can now  use one that is more in
    line with the  rest (the DC21040  chip doesn't  have a hardware  timer).
    The downside is the 1 'jiffies' (10ms) resolution.

    IEEE 802.3u MII interface code has  been added in anticipation that some
    products may use it in the future.

    The SMC9332 card  has a non-compliant SROM  which needs fixing -  I have
    patched this  driver to detect it  because the SROM format used complies
    to a previous DEC-STD format.

    I have removed the buffer copies needed for receive on Intels.  I cannot
    remove them for   Alphas since  the  Tulip hardware   only does longword
    aligned  DMA transfers  and  the  Alphas get   alignment traps with  non
    longword aligned data copies (which makes them really slow). No comment.

    I  have added SROM decoding  routines to make this  driver work with any
    card that  supports the Digital  Semiconductor SROM spec. This will help
    all  cards running the dc2114x  series chips in particular.  Cards using
    the dc2104x  chips should run correctly with  the basic  driver.  I'm in
    debt to <mjacob@feral.com> for the  testing and feedback that helped get
    this feature working.  So far we have  tested KINGSTON, SMC8432, SMC9332
    (with the latest SROM complying  with the SROM spec  V3: their first was
    broken), ZNYX342  and  LinkSys. ZYNX314 (dual  21041  MAC) and  ZNYX 315
    (quad 21041 MAC)  cards also  appear  to work despite their  incorrectly
    wired IRQs.

    I have added a temporary fix for interrupt problems when some SCSI cards
    share the same interrupt as the DECchip based  cards. The problem occurs
    because  the SCSI card wants to  grab the interrupt  as a fast interrupt
    (runs the   service routine with interrupts turned   off) vs.  this card
    which really needs to run the service routine with interrupts turned on.
    This driver will  now   add the interrupt service   routine  as  a  fast
    interrupt if it   is bounced from the   slow interrupt.  THIS IS NOT   A
    RECOMMENDED WAY TO RUN THE DRIVER  and has been done  for a limited time
    until  people   sort  out their  compatibility    issues and the  kernel
    interrupt  service code  is  fixed.   YOU  SHOULD SEPARATE OUT  THE FAST
    INTERRUPT CARDS FROM THE SLOW INTERRUPT CARDS to ensure that they do not
    run on the same interrupt. PCMCIA/CardBus is another can of worms...

    Finally, I think  I have really  fixed  the module  loading problem with
    more than one DECchip based  card.  As a  side effect, I don't mess with
    the  device structure any  more which means that  if more than 1 card in
    2.0.x is    installed (4  in   2.1.x),  the  user   will have   to  edit
    linux/drivers/net/Space.c  to make room for  them. Hence, module loading
    is  the preferred way to use   this driver, since  it  doesn't have this
    limitation.

    Where SROM media  detection is used and  full duplex is specified in the
    SROM,  the feature is  ignored unless  lp->params.fdx  is set at compile
    time  OR during  a   module load  (insmod  de4x5   args='eth??:fdx' [see
    below]).  This is because there  is no way  to automatically detect full
    duplex   links  except through   autonegotiation.    When I  include the
    autonegotiation feature in  the SROM autoconf  code, this detection will
    occur automatically for that case.

    Command  line arguments are  now  allowed, similar  to passing arguments
    through LILO. This will allow a per adapter board  set up of full duplex
    and media. The only lexical constraints  are: the board name (dev->name)
    appears in the list before its  parameters.  The list of parameters ends
    either at the end of the parameter list or with another board name.  The
    following parameters are allowed:

            fdx        for full duplex
	    autosense  to set the media/speed; with the following
	               sub-parameters:
		       TP, TP_NW, BNC, AUI, BNC_AUI, 100Mb, 10Mb, AUTO

    Case sensitivity is important  for  the sub-parameters. They *must*   be
    upper case. Examples:

        insmod de4x5 args='eth1:fdx autosense=BNC eth0:autosense=100Mb'.

    For a compiled in driver, at or above line 548, place e.g.
	#define DE4X5_PARM "eth0:fdx autosense=AUI eth2:autosense=TP"

    Yes,  I know full duplex isn't  permissible on BNC  or AUI; they're just
    examples. By default, full duplex is turned off and  AUTO is the default
    autosense setting.  In reality, I expect only  the full duplex option to
    be used. Note the use of single quotes in the two examples above and the
    lack of commas to separate items. ALSO, you must get the requested media
    correct in relation to what the adapter SROM says it has. There's no way
    to  determine this in  advance other than by  trial and error and common
    sense, e.g. call a BNC connectored port 'BNC', not '10Mb'.

    Changed the bus probing.  EISA used to be  done first,  followed by PCI.
    Most people probably don't even know  what a de425 is today and the EISA
    probe has messed  up some SCSI cards  in the past,  so now PCI is always
    probed  first  followed by  EISA if  a) the architecture allows EISA and
    either  b) there have been no PCI cards detected or  c) an EISA probe is
    forced by  the user.  To force  a probe  include  "force_eisa"  in  your
    insmod "args" line;  for built-in kernels either change the driver to do
    this  automatically  or include  #define DE4X5_FORCE_EISA  on or  before
    line 1040 in the driver.

    TO DO:
    ------

    Revision History
    ----------------

    Version   Date        Description

      0.1     17-Nov-94   Initial writing. ALPHA code release.
      0.2     13-Jan-95   Added PCI support for DE435's.
      0.21    19-Jan-95   Added auto media detection.
      0.22    10-Feb-95   Fix interrupt handler call <chris@cosy.sbg.ac.at>.
                          Fix recognition bug reported by <bkm@star.rl.ac.uk>.
			  Add request/release_region code.
			  Add loadable modules support for PCI.
			  Clean up loadable modules support.
      0.23    28-Feb-95   Added DC21041 and DC21140 support.
                          Fix missed frame counter value and initialisation.
			  Fixed EISA probe.
      0.24    11-Apr-95   Change delay routine to use <linux/udelay>.
                          Change TX_BUFFS_AVAIL macro.
			  Change media autodetection to allow manual setting.
			  Completed DE500 (DC21140) support.
      0.241   18-Apr-95   Interim release without DE500 Autosense Algorithm.
      0.242   10-May-95   Minor changes.
      0.30    12-Jun-95   Timer fix for DC21140.
                          Portability changes.
			  Add ALPHA changes from <jestabro@ant.tay1.dec.com>.
			  Add DE500 semi automatic autosense.
			  Add Link Fail interrupt TP failure detection.
			  Add timer based link change detection.
			  Plugged a memory leak in de4x5_queue_pkt().
      0.31    13-Jun-95   Fixed PCI stuff for 1.3.1.
      0.32    26-Jun-95   Added verify_area() calls in de4x5_ioctl() from a
                          suggestion by <heiko@colossus.escape.de>.
      0.33     8-Aug-95   Add shared interrupt support (not released yet).
      0.331   21-Aug-95   Fix de4x5_open() with fast CPUs.
                          Fix de4x5_interrupt().
                          Fix dc21140_autoconf() mess.
			  No shared interrupt support.
      0.332   11-Sep-95   Added MII management interface routines.
      0.40     5-Mar-96   Fix setup frame timeout <maartenb@hpkuipc.cern.ch>.
                          Add kernel timer code (h/w is too flaky).
			  Add MII based PHY autosense.
			  Add new multicasting code.
			  Add new autosense algorithms for media/mode
			  selection using kernel scheduling/timing.
			  Re-formatted.
			  Made changes suggested by <jeff@router.patch.net>:
			    Change driver to detect all DECchip based cards
			    with DEC_ONLY restriction a special case.
			    Changed driver to autoprobe as a module. No irq
			    checking is done now - assume BIOS is good!
			  Added SMC9332 detection <manabe@Roy.dsl.tutics.ac.jp>
      0.41    21-Mar-96   Don't check for get_hw_addr checksum unless DEC card
                          only <niles@axp745gsfc.nasa.gov>
			  Fix for multiple PCI cards reported by <jos@xos.nl>
			  Duh, put the IRQF_SHARED flag into request_interrupt().
			  Fix SMC ethernet address in enet_det[].
			  Print chip name instead of "UNKNOWN" during boot.
      0.42    26-Apr-96   Fix MII write TA bit error.
                          Fix bug in dc21040 and dc21041 autosense code.
			  Remove buffer copies on receive for Intels.
			  Change sk_buff handling during media disconnects to
			   eliminate DUP packets.
			  Add dynamic TX thresholding.
			  Change all chips to use perfect multicast filtering.
			  Fix alloc_device() bug <jari@markkus2.fimr.fi>
      0.43   21-Jun-96    Fix unconnected media TX retry bug.
                          Add Accton to the list of broken cards.
			  Fix TX under-run bug for non DC21140 chips.
			  Fix boot command probe bug in alloc_device() as
			   reported by <koen.gadeyne@barco.com> and
			   <orava@nether.tky.hut.fi>.
			  Add cache locks to prevent a race condition as
			   reported by <csd@microplex.com> and
			   <baba@beckman.uiuc.edu>.
			  Upgraded alloc_device() code.
      0.431  28-Jun-96    Fix potential bug in queue_pkt() from discussion
                          with <csd@microplex.com>
      0.44   13-Aug-96    Fix RX overflow bug in 2114[023] chips.
                          Fix EISA probe bugs reported by <os2@kpi.kharkov.ua>
			  and <michael@compurex.com>.
      0.441   9-Sep-96    Change dc21041_autoconf() to probe quiet BNC media
                           with a loopback packet.
      0.442   9-Sep-96    Include AUI in dc21041 media printout. Bug reported
                           by <bhat@mundook.cs.mu.OZ.AU>
      0.45    8-Dec-96    Include endian functions for PPC use, from work
                           by <cort@cs.nmt.edu> and <g.thomas@opengroup.org>.
      0.451  28-Dec-96    Added fix to allow autoprobe for modules after
                           suggestion from <mjacob@feral.com>.
      0.5    30-Jan-97    Added SROM decoding functions.
                          Updated debug flags.
			  Fix sleep/wakeup calls for PCI cards, bug reported
			   by <cross@gweep.lkg.dec.com>.
			  Added multi-MAC, one SROM feature from discussion
			   with <mjacob@feral.com>.
			  Added full module autoprobe capability.
			  Added attempt to use an SMC9332 with broken SROM.
			  Added fix for ZYNX multi-mac cards that didn't
			   get their IRQs wired correctly.
      0.51   13-Feb-97    Added endian fixes for the SROM accesses from
			   <paubert@iram.es>
			  Fix init_connection() to remove extra device reset.
			  Fix MAC/PHY reset ordering in dc21140m_autoconf().
			  Fix initialisation problem with lp->timeout in
			   typeX_infoblock() from <paubert@iram.es>.
			  Fix MII PHY reset problem from work done by
			   <paubert@iram.es>.
      0.52   26-Apr-97    Some changes may not credit the right people -
                           a disk crash meant I lost some mail.
			  Change RX interrupt routine to drop rather than
			   defer packets to avoid hang reported by
			   <g.thomas@opengroup.org>.
			  Fix srom_exec() to return for COMPACT and type 1
			   infoblocks.
			  Added DC21142 and DC21143 functions.
			  Added byte counters from <phil@tazenda.demon.co.uk>
			  Added IRQF_DISABLED temporary fix from
			   <mjacob@feral.com>.
      0.53   12-Nov-97    Fix the *_probe() to include 'eth??' name during
                           module load: bug reported by
			   <Piete.Brooks@cl.cam.ac.uk>
			  Fix multi-MAC, one SROM, to work with 2114x chips:
			   bug reported by <cmetz@inner.net>.
			  Make above search independent of BIOS device scan
			   direction.
			  Completed DC2114[23] autosense functions.
      0.531  21-Dec-97    Fix DE500-XA 100Mb/s bug reported by
                           <robin@intercore.com
			  Fix type1_infoblock() bug introduced in 0.53, from
			   problem reports by
			   <parmee@postecss.ncrfran.france.ncr.com> and
			   <jo@ice.dillingen.baynet.de>.
			  Added argument list to set up each board from either
			   a module's command line or a compiled in #define.
			  Added generic MII PHY functionality to deal with
			   newer PHY chips.
			  Fix the mess in 2.1.67.
      0.532   5-Jan-98    Fix bug in mii_get_phy() reported by
                           <redhat@cococo.net>.
                          Fix bug in pci_probe() for 64 bit systems reported
			   by <belliott@accessone.com>.
      0.533   9-Jan-98    Fix more 64 bit bugs reported by <jal@cs.brown.edu>.
      0.534  24-Jan-98    Fix last (?) endian bug from <geert@linux-m68k.org>
      0.535  21-Feb-98    Fix Ethernet Address PROM reset bug for DC21040.
      0.536  21-Mar-98    Change pci_probe() to use the pci_dev structure.
			  **Incompatible with 2.0.x from here.**
      0.540   5-Jul-98    Atomicize assertion of dev->interrupt for SMP
                           from <lma@varesearch.com>
			  Add TP, AUI and BNC cases to 21140m_autoconf() for
			   case where a 21140 under SROM control uses, e.g. AUI
			   from problem report by <delchini@lpnp09.in2p3.fr>
			  Add MII parallel detection to 2114x_autoconf() for
			   case where no autonegotiation partner exists from
			   problem report by <mlapsley@ndirect.co.uk>.
			  Add ability to force connection type directly even
			   when using SROM control from problem report by
			   <earl@exis.net>.
			  Updated the PCI interface to conform with the latest
			   version. I hope nothing is broken...
          		  Add TX done interrupt modification from suggestion
			   by <Austin.Donnelly@cl.cam.ac.uk>.
			  Fix is_anc_capable() bug reported by
			   <Austin.Donnelly@cl.cam.ac.uk>.
			  Fix type[13]_infoblock() bug: during MII search, PHY
			   lp->rst not run because lp->ibn not initialised -
			   from report & fix by <paubert@iram.es>.
			  Fix probe bug with EISA & PCI cards present from
                           report by <eirik@netcom.com>.
      0.541  24-Aug-98    Fix compiler problems associated with i386-string
                           ops from multiple bug reports and temporary fix
			   from <paubert@iram.es>.
			  Fix pci_probe() to correctly emulate the old
			   pcibios_find_class() function.
			  Add an_exception() for old ZYNX346 and fix compile
			   warning on PPC & SPARC, from <ecd@skynet.be>.
			  Fix lastPCI to correctly work with compiled in
			   kernels and modules from bug report by
			   <Zlatko.Calusic@CARNet.hr> et al.
      0.542  15-Sep-98    Fix dc2114x_autoconf() to stop multiple messages
                           when media is unconnected.
			  Change dev->interrupt to lp->interrupt to ensure
			   alignment for Alpha's and avoid their unaligned
			   access traps. This flag is merely for log messages:
			   should do something more definitive though...
      0.543  30-Dec-98    Add SMP spin locking.
      0.544   8-May-99    Fix for buggy SROM in Motorola embedded boards using
                           a 21143 by <mmporter@home.com>.
			  Change PCI/EISA bus probing order.
      0.545  28-Nov-99    Further Moto SROM bug fix from
                           <mporter@eng.mcd.mot.com>
                          Remove double checking for DEBUG_RX in de4x5_dbg_rx()
			   from report by <geert@linux-m68k.org>
      0.546  22-Feb-01    Fixes Alpha XP1000 oops.  The srom_search function
                           was causing a page fault when initializing the
                           variable 'pb', on a non de4x5 PCI device, in this
                           case a PCI bridge (DEC chip 21152). The value of
                           'pb' is now only initialized if a de4x5 chip is
                           present.
                           <france@handhelds.org>
      0.547  08-Nov-01    Use library crc32 functions by <Matt_Domsch@dell.com>
      0.548  30-Aug-03    Big 2.6 cleanup. Ported to PCI/EISA probing and
                           generic DMA APIs. Fixed DE425 support on Alpha.
			   <maz@wild-wind.fr.eu.org>
    =========================================================================
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/eisa.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/crc32.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/moduleparam.h>
#include <linux/bitops.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>
#include <asm/uaccess.h>
#ifdef CONFIG_PPC_PMAC
#include <asm/machdep.h>
#endif /* CONFIG_PPC_PMAC */

#include "de4x5.h"

static const char version[] __devinitconst =
	KERN_INFO "de4x5.c:V0.546 2001/02/22 davies@maniac.ultranet.com\n";

#define c_char const char

/*
** MII Information
*/
struct phy_table {
    int reset;              /* Hard reset required?                         */
    int id;                 /* IEEE OUI                                     */
    int ta;                 /* One cycle TA time - 802.3u is confusing here */
    struct {                /* Non autonegotiation (parallel) speed det.    */
	int reg;
	int mask;
	int value;
    } spd;
};

struct mii_phy {
    int reset;              /* Hard reset required?                      */
    int id;                 /* IEEE OUI                                  */
    int ta;                 /* One cycle TA time                         */
    struct {                /* Non autonegotiation (parallel) speed det. */
	int reg;
	int mask;
	int value;
    } spd;
    int addr;               /* MII address for the PHY                   */
    u_char  *gep;           /* Start of GEP sequence block in SROM       */
    u_char  *rst;           /* Start of reset sequence in SROM           */
    u_int mc;               /* Media Capabilities                        */
    u_int ana;              /* NWay Advertisement                        */
    u_int fdx;              /* Full DupleX capabilities for each media   */
    u_int ttm;              /* Transmit Threshold Mode for each media    */
    u_int mci;              /* 21142 MII Connector Interrupt info        */
};

#define DE4X5_MAX_PHY 8     /* Allow upto 8 attached PHY devices per board */

struct sia_phy {
    u_char mc;              /* Media Code                                */
    u_char ext;             /* csr13-15 valid when set                   */
    int csr13;              /* SIA Connectivity Register                 */
    int csr14;              /* SIA TX/RX Register                        */
    int csr15;              /* SIA General Register                      */
    int gepc;               /* SIA GEP Control Information               */
    int gep;                /* SIA GEP Data                              */
};

/*
** Define the know universe of PHY devices that can be
** recognised by this driver.
*/
static struct phy_table phy_info[] = {
    {0, NATIONAL_TX, 1, {0x19, 0x40, 0x00}},       /* National TX      */
    {1, BROADCOM_T4, 1, {0x10, 0x02, 0x02}},       /* Broadcom T4      */
    {0, SEEQ_T4    , 1, {0x12, 0x10, 0x10}},       /* SEEQ T4          */
    {0, CYPRESS_T4 , 1, {0x05, 0x20, 0x20}},       /* Cypress T4       */
    {0, 0x7810     , 1, {0x14, 0x0800, 0x0800}}    /* Level One LTX970 */
};

/*
** These GENERIC values assumes that the PHY devices follow 802.3u and
** allow parallel detection to set the link partner ability register.
** Detection of 100Base-TX [H/F Duplex] and 100Base-T4 is supported.
*/
#define GENERIC_REG   0x05      /* Autoneg. Link Partner Advertisement Reg. */
#define GENERIC_MASK  MII_ANLPA_100M /* All 100Mb/s Technologies            */
#define GENERIC_VALUE MII_ANLPA_100M /* 100B-TX, 100B-TX FDX, 100B-T4       */

/*
** Define special SROM detection cases
*/
static c_char enet_det[][ETH_ALEN] = {
    {0x00, 0x00, 0xc0, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0xe8, 0x00, 0x00, 0x00}
};

#define SMC    1
#define ACCTON 2

/*
** SROM Repair definitions. If a broken SROM is detected a card may
** use this information to help figure out what to do. This is a
** "stab in the dark" and so far for SMC9332's only.
*/
static c_char srom_repair_info[][100] = {
    {0x00,0x1e,0x00,0x00,0x00,0x08,             /* SMC9332 */
     0x1f,0x01,0x8f,0x01,0x00,0x01,0x00,0x02,
     0x01,0x00,0x00,0x78,0xe0,0x01,0x00,0x50,
     0x00,0x18,}
};


#ifdef DE4X5_DEBUG
static int de4x5_debug = DE4X5_DEBUG;
#else
/*static int de4x5_debug = (DEBUG_MII | DEBUG_SROM | DEBUG_PCICFG | DEBUG_MEDIA | DEBUG_VERSION);*/
static int de4x5_debug = (DEBUG_MEDIA | DEBUG_VERSION);
#endif

/*
** Allow per adapter set up. For modules this is simply a command line
** parameter, e.g.:
** insmod de4x5 args='eth1:fdx autosense=BNC eth0:autosense=100Mb'.
**
** For a compiled in driver, place e.g.
**     #define DE4X5_PARM "eth0:fdx autosense=AUI eth2:autosense=TP"
** here
*/
#ifdef DE4X5_PARM
static char *args = DE4X5_PARM;
#else
static char *args;
#endif

struct parameters {
    bool fdx;
    int autosense;
};

#define DE4X5_AUTOSENSE_MS 250      /* msec autosense tick (DE500) */

#define DE4X5_NDA 0xffe0            /* No Device (I/O) Address */

/*
** Ethernet PROM defines
*/
#define PROBE_LENGTH    32
#define ETH_PROM_SIG    0xAA5500FFUL

/*
** Ethernet Info
*/
#define PKT_BUF_SZ	1536            /* Buffer size for each Tx/Rx buffer */
#define IEEE802_3_SZ    1518            /* Packet + CRC */
#define MAX_PKT_SZ   	1514            /* Maximum ethernet packet length */
#define MAX_DAT_SZ   	1500            /* Maximum ethernet data length */
#define MIN_DAT_SZ   	1               /* Minimum ethernet data length */
#define PKT_HDR_LEN     14              /* Addresses and data length info */
#define FAKE_FRAME_LEN  (MAX_PKT_SZ + 1)
#define QUEUE_PKT_TIMEOUT (3*HZ)        /* 3 second timeout */


/*
** EISA bus defines
*/
#define DE4X5_EISA_IO_PORTS   0x0c00    /* I/O port base address, slot 0 */
#define DE4X5_EISA_TOTAL_SIZE 0x100     /* I/O address extent */

#define EISA_ALLOWED_IRQ_LIST  {5, 9, 10, 11}

#define DE4X5_SIGNATURE {"DE425","DE434","DE435","DE450","DE500"}
#define DE4X5_NAME_LENGTH 8

static c_char *de4x5_signatures[] = DE4X5_SIGNATURE;

/*
** Ethernet PROM defines for DC21040
*/
#define PROBE_LENGTH    32
#define ETH_PROM_SIG    0xAA5500FFUL

/*
** PCI Bus defines
*/
#define PCI_MAX_BUS_NUM      8
#define DE4X5_PCI_TOTAL_SIZE 0x80       /* I/O address extent */
#define DE4X5_CLASS_CODE     0x00020000 /* Network controller, Ethernet */

/*
** Memory Alignment. Each descriptor is 4 longwords long. To force a
** particular alignment on the TX descriptor, adjust DESC_SKIP_LEN and
** DESC_ALIGN. ALIGN aligns the start address of the private memory area
** and hence the RX descriptor ring's first entry.
*/
#define DE4X5_ALIGN4      ((u_long)4 - 1)     /* 1 longword align */
#define DE4X5_ALIGN8      ((u_long)8 - 1)     /* 2 longword align */
#define DE4X5_ALIGN16     ((u_long)16 - 1)    /* 4 longword align */
#define DE4X5_ALIGN32     ((u_long)32 - 1)    /* 8 longword align */
#define DE4X5_ALIGN64     ((u_long)64 - 1)    /* 16 longword align */
#define DE4X5_ALIGN128    ((u_long)128 - 1)   /* 32 longword align */

#define DE4X5_ALIGN         DE4X5_ALIGN32           /* Keep the DC21040 happy... */
#define DE4X5_CACHE_ALIGN   CAL_16LONG
#define DESC_SKIP_LEN DSL_0             /* Must agree with DESC_ALIGN */
/*#define DESC_ALIGN    u32 dummy[4];  / * Must agree with DESC_SKIP_LEN */
#define DESC_ALIGN

#ifndef DEC_ONLY                        /* See README.de4x5 for using this */
static int dec_only;
#else
static int dec_only = 1;
#endif

/*
** DE4X5 IRQ ENABLE/DISABLE
*/
#define ENABLE_IRQs { \
    imr |= lp->irq_en;\
    outl(imr, DE4X5_IMR);               /* Enable the IRQs */\
}

#define DISABLE_IRQs {\
    imr = inl(DE4X5_IMR);\
    imr &= ~lp->irq_en;\
    outl(imr, DE4X5_IMR);               /* Disable the IRQs */\
}

#define UNMASK_IRQs {\
    imr |= lp->irq_mask;\
    outl(imr, DE4X5_IMR);               /* Unmask the IRQs */\
}

#define MASK_IRQs {\
    imr = inl(DE4X5_IMR);\
    imr &= ~lp->irq_mask;\
    outl(imr, DE4X5_IMR);               /* Mask the IRQs */\
}

/*
** DE4X5 START/STOP
*/
#define START_DE4X5 {\
    omr = inl(DE4X5_OMR);\
    omr |= OMR_ST | OMR_SR;\
    outl(omr, DE4X5_OMR);               /* Enable the TX and/or RX */\
}

#define STOP_DE4X5 {\
    omr = inl(DE4X5_OMR);\
    omr &= ~(OMR_ST|OMR_SR);\
    outl(omr, DE4X5_OMR);               /* Disable the TX and/or RX */ \
}

/*
** DE4X5 SIA RESET
*/
#define RESET_SIA outl(0, DE4X5_SICR);  /* Reset SIA connectivity regs */

/*
** DE500 AUTOSENSE TIMER INTERVAL (MILLISECS)
*/
#define DE4X5_AUTOSENSE_MS  250

/*
** SROM Structure
*/
struct de4x5_srom {
    char sub_vendor_id[2];
    char sub_system_id[2];
    char reserved[12];
    char id_block_crc;
    char reserved2;
    char version;
    char num_controllers;
    char ieee_addr[6];
    char info[100];
    short chksum;
};
#define SUB_VENDOR_ID 0x500a

/*
** DE4X5 Descriptors. Make sure that all the RX buffers are contiguous
** and have sizes of both a power of 2 and a multiple of 4.
** A size of 256 bytes for each buffer could be chosen because over 90% of
** all packets in our network are <256 bytes long and 64 longword alignment
** is possible. 1536 showed better 'ttcp' performance. Take your pick. 32 TX
** descriptors are needed for machines with an ALPHA CPU.
*/
#define NUM_RX_DESC 8                   /* Number of RX descriptors   */
#define NUM_TX_DESC 32                  /* Number of TX descriptors   */
#define RX_BUFF_SZ  1536                /* Power of 2 for kmalloc and */
                                        /* Multiple of 4 for DC21040  */
                                        /* Allows 512 byte alignment  */
struct de4x5_desc {
    volatile __le32 status;
    __le32 des1;
    __le32 buf;
    __le32 next;
    DESC_ALIGN
};

/*
** The DE4X5 private structure
*/
#define DE4X5_PKT_STAT_SZ 16
#define DE4X5_PKT_BIN_SZ  128            /* Should be >=100 unless you
                                            increase DE4X5_PKT_STAT_SZ */

struct pkt_stats {
	u_int bins[DE4X5_PKT_STAT_SZ];      /* Private stats counters       */
	u_int unicast;
	u_int multicast;
	u_int broadcast;
	u_int excessive_collisions;
	u_int tx_underruns;
	u_int excessive_underruns;
	u_int rx_runt_frames;
	u_int rx_collision;
	u_int rx_dribble;
	u_int rx_overflow;
};

struct de4x5_private {
    char adapter_name[80];                  /* Adapter name                 */
    u_long interrupt;                       /* Aligned ISR flag             */
    struct de4x5_desc *rx_ring;		    /* RX descriptor ring           */
    struct de4x5_desc *tx_ring;		    /* TX descriptor ring           */
    struct sk_buff *tx_skb[NUM_TX_DESC];    /* TX skb for freeing when sent */
    struct sk_buff *rx_skb[NUM_RX_DESC];    /* RX skb's                     */
    int rx_new, rx_old;                     /* RX descriptor ring pointers  */
    int tx_new, tx_old;                     /* TX descriptor ring pointers  */
    char setup_frame[SETUP_FRAME_LEN];      /* Holds MCA and PA info.       */
    char frame[64];                         /* Min sized packet for loopback*/
    spinlock_t lock;                        /* Adapter specific spinlock    */
    struct net_device_stats stats;          /* Public stats                 */
    struct pkt_stats pktStats;	            /* Private stats counters	    */
    char rxRingSize;
    char txRingSize;
    int  bus;                               /* EISA or PCI                  */
    int  bus_num;                           /* PCI Bus number               */
    int  device;                            /* Device number on PCI bus     */
    int  state;                             /* Adapter OPENED or CLOSED     */
    int  chipset;                           /* DC21040, DC21041 or DC21140  */
    s32  irq_mask;                          /* Interrupt Mask (Enable) bits */
    s32  irq_en;                            /* Summary interrupt bits       */
    int  media;                             /* Media (eg TP), mode (eg 100B)*/
    int  c_media;                           /* Remember the last media conn */
    bool fdx;                               /* media full duplex flag       */
    int  linkOK;                            /* Link is OK                   */
    int  autosense;                         /* Allow/disallow autosensing   */
    bool tx_enable;                         /* Enable descriptor polling    */
    int  setup_f;                           /* Setup frame filtering type   */
    int  local_state;                       /* State within a 'media' state */
    struct mii_phy phy[DE4X5_MAX_PHY];      /* List of attached PHY devices */
    struct sia_phy sia;                     /* SIA PHY Information          */
    int  active;                            /* Index to active PHY device   */
    int  mii_cnt;                           /* Number of attached PHY's     */
    int  timeout;                           /* Scheduling counter           */
    struct timer_list timer;                /* Timer info for kernel        */
    int tmp;                                /* Temporary global per card    */
    struct {
	u_long lock;                        /* Lock the cache accesses      */
	s32 csr0;                           /* Saved Bus Mode Register      */
	s32 csr6;                           /* Saved Operating Mode Reg.    */
	s32 csr7;                           /* Saved IRQ Mask Register      */
	s32 gep;                            /* Saved General Purpose Reg.   */
	s32 gepc;                           /* Control info for GEP         */
	s32 csr13;                          /* Saved SIA Connectivity Reg.  */
	s32 csr14;                          /* Saved SIA TX/RX Register     */
	s32 csr15;                          /* Saved SIA General Register   */
	int save_cnt;                       /* Flag if state already saved  */
	struct sk_buff_head queue;          /* Save the (re-ordered) skb's  */
    } cache;
    struct de4x5_srom srom;                 /* A copy of the SROM           */
    int cfrv;				    /* Card CFRV copy */
    int rx_ovf;                             /* Check for 'RX overflow' tag  */
    bool useSROM;                           /* For non-DEC card use SROM    */
    bool useMII;                            /* Infoblock using the MII      */
    int asBitValid;                         /* Autosense bits in GEP?       */
    int asPolarity;                         /* 0 => asserted high           */
    int asBit;                              /* Autosense bit number in GEP  */
    int defMedium;                          /* SROM default medium          */
    int tcount;                             /* Last infoblock number        */
    int infoblock_init;                     /* Initialised this infoblock?  */
    int infoleaf_offset;                    /* SROM infoleaf for controller */
    s32 infoblock_csr6;                     /* csr6 value in SROM infoblock */
    int infoblock_media;                    /* infoblock media              */
    int (*infoleaf_fn)(struct net_device *);    /* Pointer to infoleaf function */
    u_char *rst;                            /* Pointer to Type 5 reset info */
    u_char  ibn;                            /* Infoblock number             */
    struct parameters params;               /* Command line/ #defined params */
    struct device *gendev;	            /* Generic device */
    dma_addr_t dma_rings;		    /* DMA handle for rings	    */
    int dma_size;			    /* Size of the DMA area	    */
    char *rx_bufs;			    /* rx bufs on alpha, sparc, ... */
};

/*
** To get around certain poxy cards that don't provide an SROM
** for the second and more DECchip, I have to key off the first
** chip's address. I'll assume there's not a bad SROM iff:
**
**      o the chipset is the same
**      o the bus number is the same and > 0
**      o the sum of all the returned hw address bytes is 0 or 0x5fa
**
** Also have to save the irq for those cards whose hardware designers
** can't follow the PCI to PCI Bridge Architecture spec.
*/
static struct {
    int chipset;
    int bus;
    int irq;
    u_char addr[ETH_ALEN];
} last = {0,};

/*
** The transmit ring full condition is described by the tx_old and tx_new
** pointers by:
**    tx_old            = tx_new    Empty ring
**    tx_old            = tx_new+1  Full ring
**    tx_old+txRingSize = tx_new+1  Full ring  (wrapped condition)
*/
#define TX_BUFFS_AVAIL ((lp->tx_old<=lp->tx_new)?\
			lp->tx_old+lp->txRingSize-lp->tx_new-1:\
			lp->tx_old               -lp->tx_new-1)

#define TX_PKT_PENDING (lp->tx_old != lp->tx_new)

/*
** Public Functions
*/
static int     de4x5_open(struct net_device *dev);
static netdev_tx_t de4x5_queue_pkt(struct sk_buff *skb,
					 struct net_device *dev);
static irqreturn_t de4x5_interrupt(int irq, void *dev_id);
static int     de4x5_close(struct net_device *dev);
static struct  net_device_stats *de4x5_get_stats(struct net_device *dev);
static void    de4x5_local_stats(struct net_device *dev, char *buf, int pkt_len);
static void    set_multicast_list(struct net_device *dev);
static int     de4x5_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);

/*
** Private functions
*/
static int     de4x5_hw_init(struct net_device *dev, u_long iobase, struct device *gendev);
static int     de4x5_init(struct net_device *dev);
static int     de4x5_sw_reset(struct net_device *dev);
static int     de4x5_rx(struct net_device *dev);
static int     de4x5_tx(struct net_device *dev);
static void    de4x5_ast(struct net_device *dev);
static int     de4x5_txur(struct net_device *dev);
static int     de4x5_rx_ovfc(struct net_device *dev);

static int     autoconf_media(struct net_device *dev);
static void    create_packet(struct net_device *dev, char *frame, int len);
static void    load_packet(struct net_device *dev, char *buf, u32 flags, struct sk_buff *skb);
static int     dc21040_autoconf(struct net_device *dev);
static int     dc21041_autoconf(struct net_device *dev);
static int     dc21140m_autoconf(struct net_device *dev);
static int     dc2114x_autoconf(struct net_device *dev);
static int     srom_autoconf(struct net_device *dev);
static int     de4x5_suspect_state(struct net_device *dev, int timeout, int prev_state, int (*fn)(struct net_device *, int), int (*asfn)(struct net_device *));
static int     dc21040_state(struct net_device *dev, int csr13, int csr14, int csr15, int timeout, int next_state, int suspect_state, int (*fn)(struct net_device *, int));
static int     test_media(struct net_device *dev, s32 irqs, s32 irq_mask, s32 csr13, s32 csr14, s32 csr15, s32 msec);
static int     test_for_100Mb(struct net_device *dev, int msec);
static int     wait_for_link(struct net_device *dev);
static int     test_mii_reg(struct net_device *dev, int reg, int mask, bool pol, long msec);
static int     is_spd_100(struct net_device *dev);
static int     is_100_up(struct net_device *dev);
static int     is_10_up(struct net_device *dev);
static int     is_anc_capable(struct net_device *dev);
static int     ping_media(struct net_device *dev, int msec);
static struct sk_buff *de4x5_alloc_rx_buff(struct net_device *dev, int index, int len);
static void    de4x5_free_rx_buffs(struct net_device *dev);
static void    de4x5_free_tx_buffs(struct net_device *dev);
static void    de4x5_save_skbs(struct net_device *dev);
static void    de4x5_rst_desc_ring(struct net_device *dev);
static void    de4x5_cache_state(struct net_device *dev, int flag);
static void    de4x5_put_cache(struct net_device *dev, struct sk_buff *skb);
static void    de4x5_putb_cache(struct net_device *dev, struct sk_buff *skb);
static struct  sk_buff *de4x5_get_cache(struct net_device *dev);
static void    de4x5_setup_intr(struct net_device *dev);
static void    de4x5_init_connection(struct net_device *dev);
static int     de4x5_reset_phy(struct net_device *dev);
static void    reset_init_sia(struct net_device *dev, s32 sicr, s32 strr, s32 sigr);
static int     test_ans(struct net_device *dev, s32 irqs, s32 irq_mask, s32 msec);
static int     test_tp(struct net_device *dev, s32 msec);
static int     EISA_signature(char *name, struct device *device);
static int     PCI_signature(char *name, struct de4x5_private *lp);
static void    DevicePresent(struct net_device *dev, u_long iobase);
static void    enet_addr_rst(u_long aprom_addr);
static int     de4x5_bad_srom(struct de4x5_private *lp);
static short   srom_rd(u_long address, u_char offset);
static void    srom_latch(u_int command, u_long address);
static void    srom_command(u_int command, u_long address);
static void    srom_address(u_int command, u_long address, u_char offset);
static short   srom_data(u_int command, u_long address);
/*static void    srom_busy(u_int command, u_long address);*/
static void    sendto_srom(u_int command, u_long addr);
static int     getfrom_srom(u_long addr);
static int     srom_map_media(struct net_device *dev);
static int     srom_infoleaf_info(struct net_device *dev);
static void    srom_init(struct net_device *dev);
static void    srom_exec(struct net_device *dev, u_char *p);
static int     mii_rd(u_char phyreg, u_char phyaddr, u_long ioaddr);
static void    mii_wr(int data, u_char phyreg, u_char phyaddr, u_long ioaddr);
static int     mii_rdata(u_long ioaddr);
static void    mii_wdata(int data, int len, u_long ioaddr);
static void    mii_ta(u_long rw, u_long ioaddr);
static int     mii_swap(int data, int len);
static void    mii_address(u_char addr, u_long ioaddr);
static void    sendto_mii(u32 command, int data, u_long ioaddr);
static int     getfrom_mii(u32 command, u_long ioaddr);
static int     mii_get_oui(u_char phyaddr, u_long ioaddr);
static int     mii_get_phy(struct net_device *dev);
static void    SetMulticastFilter(struct net_device *dev);
static int     get_hw_addr(struct net_device *dev);
static void    srom_repair(struct net_device *dev, int card);
static int     test_bad_enet(struct net_device *dev, int status);
static int     an_exception(struct de4x5_private *lp);
static char    *build_setup_frame(struct net_device *dev, int mode);
static void    disable_ast(struct net_device *dev);
static long    de4x5_switch_mac_port(struct net_device *dev);
static int     gep_rd(struct net_device *dev);
static void    gep_wr(s32 data, struct net_device *dev);
static void    yawn(struct net_device *dev, int state);
static void    de4x5_parse_params(struct net_device *dev);
static void    de4x5_dbg_open(struct net_device *dev);
static void    de4x5_dbg_mii(struct net_device *dev, int k);
static void    de4x5_dbg_media(struct net_device *dev);
static void    de4x5_dbg_srom(struct de4x5_srom *p);
static void    de4x5_dbg_rx(struct sk_buff *skb, int len);
static int     de4x5_strncmp(char *a, char *b, int n);
static int     dc21041_infoleaf(struct net_device *dev);
static int     dc21140_infoleaf(struct net_device *dev);
static int     dc21142_infoleaf(struct net_device *dev);
static int     dc21143_infoleaf(struct net_device *dev);
static int     type0_infoblock(struct net_device *dev, u_char count, u_char *p);
static int     type1_infoblock(struct net_device *dev, u_char count, u_char *p);
static int     type2_infoblock(struct net_device *dev, u_char count, u_char *p);
static int     type3_infoblock(struct net_device *dev, u_char count, u_char *p);
static int     type4_infoblock(struct net_device *dev, u_char count, u_char *p);
static int     type5_infoblock(struct net_device *dev, u_char count, u_char *p);
static int     compact_infoblock(struct net_device *dev, u_char count, u_char *p);

/*
** Note now that module autoprobing is allowed under EISA and PCI. The
** IRQ lines will not be auto-detected; instead I'll rely on the BIOSes
** to "do the right thing".
*/

static int io=0x0;/* EDIT THIS LINE FOR YOUR CONFIGURATION IF NEEDED        */

module_param(io, int, 0);
module_param(de4x5_debug, int, 0);
module_param(dec_only, int, 0);
module_param(args, charp, 0);

MODULE_PARM_DESC(io, "de4x5 I/O base address");
MODULE_PARM_DESC(de4x5_debug, "de4x5 debug mask");
MODULE_PARM_DESC(dec_only, "de4x5 probe only for Digital boards (0-1)");
MODULE_PARM_DESC(args, "de4x5 full duplex and media type settings; see de4x5.c for details");
MODULE_LICENSE("GPL");

/*
** List the SROM infoleaf functions and chipsets
*/
struct InfoLeaf {
    int chipset;
    int (*fn)(struct net_device *);
};
static struct InfoLeaf infoleaf_array[] = {
    {DC21041, dc21041_infoleaf},
    {DC21140, dc21140_infoleaf},
    {DC21142, dc21142_infoleaf},
    {DC21143, dc21143_infoleaf}
};
#define INFOLEAF_SIZE ARRAY_SIZE(infoleaf_array)

/*
** List the SROM info block functions
*/
static int (*dc_infoblock[])(struct net_device *dev, u_char, u_char *) = {
    type0_infoblock,
    type1_infoblock,
    type2_infoblock,
    type3_infoblock,
    type4_infoblock,
    type5_infoblock,
    compact_infoblock
};

#define COMPACT (ARRAY_SIZE(dc_infoblock) - 1)

/*
** Miscellaneous defines...
*/
#define RESET_DE4X5 {\
    int i;\
    i=inl(DE4X5_BMR);\
    mdelay(1);\
    outl(i | BMR_SWR, DE4X5_BMR);\
    mdelay(1);\
    outl(i, DE4X5_BMR);\
    mdelay(1);\
    for (i=0;i<5;i++) {inl(DE4X5_BMR); mdelay(1);}\
    mdelay(1);\
}

#define PHY_HARD_RESET {\
    outl(GEP_HRST, DE4X5_GEP);           /* Hard RESET the PHY dev. */\
    mdelay(1);                           /* Assert for 1ms */\
    outl(0x00, DE4X5_GEP);\
    mdelay(2);                           /* Wait for 2ms */\
}

static const struct net_device_ops de4x5_netdev_ops = {
    .ndo_open		= de4x5_open,
    .ndo_stop		= de4x5_close,
    .ndo_start_xmit	= de4x5_queue_pkt,
    .ndo_get_stats	= de4x5_get_stats,
    .ndo_set_multicast_list = set_multicast_list,
    .ndo_do_ioctl	= de4x5_ioctl,
    .ndo_change_mtu	= eth_change_mtu,
    .ndo_set_mac_address= eth_mac_addr,
    .ndo_validate_addr	= eth_validate_addr,
};


static int __devinit
de4x5_hw_init(struct net_device *dev, u_long iobase, struct device *gendev)
{
    char name[DE4X5_NAME_LENGTH + 1];
    struct de4x5_private *lp = netdev_priv(dev);
    struct pci_dev *pdev = NULL;
    int i, status=0;

    dev_set_drvdata(gendev, dev);

    /* Ensure we're not sleeping */
    if (lp->bus == EISA) {
	outb(WAKEUP, PCI_CFPM);
    } else {
	pdev = to_pci_dev (gendev);
	pci_write_config_byte(pdev, PCI_CFDA_PSM, WAKEUP);
    }
    mdelay(10);

    RESET_DE4X5;

    if ((inl(DE4X5_STS) & (STS_TS | STS_RS)) != 0) {
	return -ENXIO;                       /* Hardware could not reset */
    }

    /*
    ** Now find out what kind of DC21040/DC21041/DC21140 board we have.
    */
    lp->useSROM = false;
    if (lp->bus == PCI) {
	PCI_signature(name, lp);
    } else {
	EISA_signature(name, gendev);
    }

    if (*name == '\0') {                     /* Not found a board signature */
	return -ENXIO;
    }

    dev->base_addr = iobase;
    printk ("%s: %s at 0x%04lx", dev_name(gendev), name, iobase);

    status = get_hw_addr(dev);
    printk(", h/w address %pM\n", dev->dev_addr);

    if (status != 0) {
	printk("      which has an Ethernet PROM CRC error.\n");
	return -ENXIO;
    } else {
	skb_queue_head_init(&lp->cache.queue);
	lp->cache.gepc = GEP_INIT;
	lp->asBit = GEP_SLNK;
	lp->asPolarity = GEP_SLNK;
	lp->asBitValid = ~0;
	lp->timeout = -1;
	lp->gendev = gendev;
	spin_lock_init(&lp->lock);
	init_timer(&lp->timer);
	lp->timer.function = (void (*)(unsigned long))de4x5_ast;
	lp->timer.data = (unsigned long)dev;
	de4x5_parse_params(dev);

	/*
	** Choose correct autosensing in case someone messed up
	*/
        lp->autosense = lp->params.autosense;
        if (lp->chipset != DC21140) {
            if ((lp->chipset==DC21040) && (lp->params.autosense&TP_NW)) {
                lp->params.autosense = TP;
            }
            if ((lp->chipset==DC21041) && (lp->params.autosense&BNC_AUI)) {
                lp->params.autosense = BNC;
            }
        }
	lp->fdx = lp->params.fdx;
	sprintf(lp->adapter_name,"%s (%s)", name, dev_name(gendev));

	lp->dma_size = (NUM_RX_DESC + NUM_TX_DESC) * sizeof(struct de4x5_desc);
#if defined(__alpha__) || defined(__powerpc__) || defined(CONFIG_SPARC) || defined(DE4X5_DO_MEMCPY)
	lp->dma_size += RX_BUFF_SZ * NUM_RX_DESC + DE4X5_ALIGN;
#endif
	lp->rx_ring = dma_alloc_coherent(gendev, lp->dma_size,
					 &lp->dma_rings, GFP_ATOMIC);
	if (lp->rx_ring == NULL) {
	    return -ENOMEM;
	}

	lp->tx_ring = lp->rx_ring + NUM_RX_DESC;

	/*
	** Set up the RX descriptor ring (Intels)
	** Allocate contiguous receive buffers, long word aligned (Alphas)
	*/
#if !defined(__alpha__) && !defined(__powerpc__) && !defined(CONFIG_SPARC) && !defined(DE4X5_DO_MEMCPY)
	for (i=0; i<NUM_RX_DESC; i++) {
	    lp->rx_ring[i].status = 0;
	    lp->rx_ring[i].des1 = cpu_to_le32(RX_BUFF_SZ);
	    lp->rx_ring[i].buf = 0;
	    lp->rx_ring[i].next = 0;
	    lp->rx_skb[i] = (struct sk_buff *) 1;     /* Dummy entry */
	}

#else
	{
		dma_addr_t dma_rx_bufs;

		dma_rx_bufs = lp->dma_rings + (NUM_RX_DESC + NUM_TX_DESC)
		      	* sizeof(struct de4x5_desc);
		dma_rx_bufs = (dma_rx_bufs + DE4X5_ALIGN) & ~DE4X5_ALIGN;
		lp->rx_bufs = (char *)(((long)(lp->rx_ring + NUM_RX_DESC
		      	+ NUM_TX_DESC) + DE4X5_ALIGN) & ~DE4X5_ALIGN);
		for (i=0; i<NUM_RX_DESC; i++) {
	    		lp->rx_ring[i].status = 0;
	    		lp->rx_ring[i].des1 = cpu_to_le32(RX_BUFF_SZ);
	    		lp->rx_ring[i].buf =
				cpu_to_le32(dma_rx_bufs+i*RX_BUFF_SZ);
	    		lp->rx_ring[i].next = 0;
	    		lp->rx_skb[i] = (struct sk_buff *) 1; /* Dummy entry */
		}

	}
#endif

	barrier();

	lp->rxRingSize = NUM_RX_DESC;
	lp->txRingSize = NUM_TX_DESC;

	/* Write the end of list marker to the descriptor lists */
	lp->rx_ring[lp->rxRingSize - 1].des1 |= cpu_to_le32(RD_RER);
	lp->tx_ring[lp->txRingSize - 1].des1 |= cpu_to_le32(TD_TER);

	/* Tell the adapter where the TX/RX rings are located. */
	outl(lp->dma_rings, DE4X5_RRBA);
	outl(lp->dma_rings + NUM_RX_DESC * sizeof(struct de4x5_desc),
	     DE4X5_TRBA);

	/* Initialise the IRQ mask and Enable/Disable */
	lp->irq_mask = IMR_RIM | IMR_TIM | IMR_TUM | IMR_UNM;
	lp->irq_en   = IMR_NIM | IMR_AIM;

	/* Create a loopback packet frame for later media probing */
	create_packet(dev, lp->frame, sizeof(lp->frame));

	/* Check if the RX overflow bug needs testing for */
	i = lp->cfrv & 0x000000fe;
	if ((lp->chipset == DC21140) && (i == 0x20)) {
	    lp->rx_ovf = 1;
	}

	/* Initialise the SROM pointers if possible */
	if (lp->useSROM) {
	    lp->state = INITIALISED;
	    if (srom_infoleaf_info(dev)) {
	        dma_free_coherent (gendev, lp->dma_size,
			       lp->rx_ring, lp->dma_rings);
		return -ENXIO;
	    }
	    srom_init(dev);
	}

	lp->state = CLOSED;

	/*
	** Check for an MII interface
	*/
	if ((lp->chipset != DC21040) && (lp->chipset != DC21041)) {
	    mii_get_phy(dev);
	}

	printk("      and requires IRQ%d (provided by %s).\n", dev->irq,
	       ((lp->bus == PCI) ? "PCI BIOS" : "EISA CNFG"));
    }

    if (de4x5_debug & DEBUG_VERSION) {
	printk(version);
    }

    /* The DE4X5-specific entries in the device structure. */
    SET_NETDEV_DEV(dev, gendev);
    dev->netdev_ops = &de4x5_netdev_ops;
    dev->mem_start = 0;

    /* Fill in the generic fields of the device structure. */
    if ((status = register_netdev (dev))) {
	    dma_free_coherent (gendev, lp->dma_size,
			       lp->rx_ring, lp->dma_rings);
	    return status;
    }

    /* Let the adapter sleep to save power */
    yawn(dev, SLEEP);

    return status;
}


static int
de4x5_open(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    u_long iobase = dev->base_addr;
    int i, status = 0;
    s32 omr;

    /* Allocate the RX buffers */
    for (i=0; i<lp->rxRingSize; i++) {
	if (de4x5_alloc_rx_buff(dev, i, 0) == NULL) {
	    de4x5_free_rx_buffs(dev);
	    return -EAGAIN;
	}
    }

    /*
    ** Wake up the adapter
    */
    yawn(dev, WAKEUP);

    /*
    ** Re-initialize the DE4X5...
    */
    status = de4x5_init(dev);
    spin_lock_init(&lp->lock);
    lp->state = OPEN;
    de4x5_dbg_open(dev);

    if (request_irq(dev->irq, de4x5_interrupt, IRQF_SHARED,
		                                     lp->adapter_name, dev)) {
	printk("de4x5_open(): Requested IRQ%d is busy - attemping FAST/SHARE...", dev->irq);
	if (request_irq(dev->irq, de4x5_interrupt, IRQF_DISABLED | IRQF_SHARED,
			                             lp->adapter_name, dev)) {
	    printk("\n              Cannot get IRQ- reconfigure your hardware.\n");
	    disable_ast(dev);
	    de4x5_free_rx_buffs(dev);
	    de4x5_free_tx_buffs(dev);
	    yawn(dev, SLEEP);
	    lp->state = CLOSED;
	    return -EAGAIN;
	} else {
	    printk("\n              Succeeded, but you should reconfigure your hardware to avoid this.\n");
	    printk("WARNING: there may be IRQ related problems in heavily loaded systems.\n");
	}
    }

    lp->interrupt = UNMASK_INTERRUPTS;
    dev->trans_start = jiffies;

    START_DE4X5;

    de4x5_setup_intr(dev);

    if (de4x5_debug & DEBUG_OPEN) {
	printk("\tsts:  0x%08x\n", inl(DE4X5_STS));
	printk("\tbmr:  0x%08x\n", inl(DE4X5_BMR));
	printk("\timr:  0x%08x\n", inl(DE4X5_IMR));
	printk("\tomr:  0x%08x\n", inl(DE4X5_OMR));
	printk("\tsisr: 0x%08x\n", inl(DE4X5_SISR));
	printk("\tsicr: 0x%08x\n", inl(DE4X5_SICR));
	printk("\tstrr: 0x%08x\n", inl(DE4X5_STRR));
	printk("\tsigr: 0x%08x\n", inl(DE4X5_SIGR));
    }

    return status;
}

/*
** Initialize the DE4X5 operating conditions. NB: a chip problem with the
** DC21140 requires using perfect filtering mode for that chip. Since I can't
** see why I'd want > 14 multicast addresses, I have changed all chips to use
** the perfect filtering mode. Keep the DMA burst length at 8: there seems
** to be data corruption problems if it is larger (UDP errors seen from a
** ttcp source).
*/
static int
de4x5_init(struct net_device *dev)
{
    /* Lock out other processes whilst setting up the hardware */
    netif_stop_queue(dev);

    de4x5_sw_reset(dev);

    /* Autoconfigure the connected port */
    autoconf_media(dev);

    return 0;
}

static int
de4x5_sw_reset(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    u_long iobase = dev->base_addr;
    int i, j, status = 0;
    s32 bmr, omr;

    /* Select the MII or SRL port now and RESET the MAC */
    if (!lp->useSROM) {
	if (lp->phy[lp->active].id != 0) {
	    lp->infoblock_csr6 = OMR_SDP | OMR_PS | OMR_HBD;
	} else {
	    lp->infoblock_csr6 = OMR_SDP | OMR_TTM;
	}
	de4x5_switch_mac_port(dev);
    }

    /*
    ** Set the programmable burst length to 8 longwords for all the DC21140
    ** Fasternet chips and 4 longwords for all others: DMA errors result
    ** without these values. Cache align 16 long.
    */
    bmr = (lp->chipset==DC21140 ? PBL_8 : PBL_4) | DESC_SKIP_LEN | DE4X5_CACHE_ALIGN;
    bmr |= ((lp->chipset & ~0x00ff)==DC2114x ? BMR_RML : 0);
    outl(bmr, DE4X5_BMR);

    omr = inl(DE4X5_OMR) & ~OMR_PR;             /* Turn off promiscuous mode */
    if (lp->chipset == DC21140) {
	omr |= (OMR_SDP | OMR_SB);
    }
    lp->setup_f = PERFECT;
    outl(lp->dma_rings, DE4X5_RRBA);
    outl(lp->dma_rings + NUM_RX_DESC * sizeof(struct de4x5_desc),
	 DE4X5_TRBA);

    lp->rx_new = lp->rx_old = 0;
    lp->tx_new = lp->tx_old = 0;

    for (i = 0; i < lp->rxRingSize; i++) {
	lp->rx_ring[i].status = cpu_to_le32(R_OWN);
    }

    for (i = 0; i < lp->txRingSize; i++) {
	lp->tx_ring[i].status = cpu_to_le32(0);
    }

    barrier();

    /* Build the setup frame depending on filtering mode */
    SetMulticastFilter(dev);

    load_packet(dev, lp->setup_frame, PERFECT_F|TD_SET|SETUP_FRAME_LEN, (struct sk_buff *)1);
    outl(omr|OMR_ST, DE4X5_OMR);

    /* Poll for setup frame completion (adapter interrupts are disabled now) */

    for (j=0, i=0;(i<500) && (j==0);i++) {       /* Upto 500ms delay */
	mdelay(1);
	if ((s32)le32_to_cpu(lp->tx_ring[lp->tx_new].status) >= 0) j=1;
    }
    outl(omr, DE4X5_OMR);                        /* Stop everything! */

    if (j == 0) {
	printk("%s: Setup frame timed out, status %08x\n", dev->name,
	       inl(DE4X5_STS));
	status = -EIO;
    }

    lp->tx_new = (++lp->tx_new) % lp->txRingSize;
    lp->tx_old = lp->tx_new;

    return status;
}

/*
** Writes a socket buffer address to the next available transmit descriptor.
*/
static netdev_tx_t
de4x5_queue_pkt(struct sk_buff *skb, struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    u_long iobase = dev->base_addr;
    u_long flags = 0;

    netif_stop_queue(dev);
    if (!lp->tx_enable)                   /* Cannot send for now */
	return NETDEV_TX_LOCKED;

    /*
    ** Clean out the TX ring asynchronously to interrupts - sometimes the
    ** interrupts are lost by delayed descriptor status updates relative to
    ** the irq assertion, especially with a busy PCI bus.
    */
    spin_lock_irqsave(&lp->lock, flags);
    de4x5_tx(dev);
    spin_unlock_irqrestore(&lp->lock, flags);

    /* Test if cache is already locked - requeue skb if so */
    if (test_and_set_bit(0, (void *)&lp->cache.lock) && !lp->interrupt)
	return NETDEV_TX_LOCKED;

    /* Transmit descriptor ring full or stale skb */
    if (netif_queue_stopped(dev) || (u_long) lp->tx_skb[lp->tx_new] > 1) {
	if (lp->interrupt) {
	    de4x5_putb_cache(dev, skb);          /* Requeue the buffer */
	} else {
	    de4x5_put_cache(dev, skb);
	}
	if (de4x5_debug & DEBUG_TX) {
	    printk("%s: transmit busy, lost media or stale skb found:\n  STS:%08x\n  tbusy:%d\n  IMR:%08x\n  OMR:%08x\n Stale skb: %s\n",dev->name, inl(DE4X5_STS), netif_queue_stopped(dev), inl(DE4X5_IMR), inl(DE4X5_OMR), ((u_long) lp->tx_skb[lp->tx_new] > 1) ? "YES" : "NO");
	}
    } else if (skb->len > 0) {
	/* If we already have stuff queued locally, use that first */
	if (!skb_queue_empty(&lp->cache.queue) && !lp->interrupt) {
	    de4x5_put_cache(dev, skb);
	    skb = de4x5_get_cache(dev);
	}

	while (skb && !netif_queue_stopped(dev) &&
	       (u_long) lp->tx_skb[lp->tx_new] <= 1) {
	    spin_lock_irqsave(&lp->lock, flags);
	    netif_stop_queue(dev);
	    load_packet(dev, skb->data, TD_IC | TD_LS | TD_FS | skb->len, skb);
 	    lp->stats.tx_bytes += skb->len;
	    outl(POLL_DEMAND, DE4X5_TPD);/* Start the TX */

	    lp->tx_new = (++lp->tx_new) % lp->txRingSize;
	    dev->trans_start = jiffies;

	    if (TX_BUFFS_AVAIL) {
		netif_start_queue(dev);         /* Another pkt may be queued */
	    }
	    skb = de4x5_get_cache(dev);
	    spin_unlock_irqrestore(&lp->lock, flags);
	}
	if (skb) de4x5_putb_cache(dev, skb);
    }

    lp->cache.lock = 0;

    return NETDEV_TX_OK;
}

/*
** The DE4X5 interrupt handler.
**
** I/O Read/Writes through intermediate PCI bridges are never 'posted',
** so that the asserted interrupt always has some real data to work with -
** if these I/O accesses are ever changed to memory accesses, ensure the
** STS write is read immediately to complete the transaction if the adapter
** is not on bus 0. Lost interrupts can still occur when the PCI bus load
** is high and descriptor status bits cannot be set before the associated
** interrupt is asserted and this routine entered.
*/
static irqreturn_t
de4x5_interrupt(int irq, void *dev_id)
{
    struct net_device *dev = dev_id;
    struct de4x5_private *lp;
    s32 imr, omr, sts, limit;
    u_long iobase;
    unsigned int handled = 0;

    lp = netdev_priv(dev);
    spin_lock(&lp->lock);
    iobase = dev->base_addr;

    DISABLE_IRQs;                        /* Ensure non re-entrancy */

    if (test_and_set_bit(MASK_INTERRUPTS, (void*) &lp->interrupt))
	printk("%s: Re-entering the interrupt handler.\n", dev->name);

    synchronize_irq(dev->irq);

    for (limit=0; limit<8; limit++) {
	sts = inl(DE4X5_STS);            /* Read IRQ status */
	outl(sts, DE4X5_STS);            /* Reset the board interrupts */

	if (!(sts & lp->irq_mask)) break;/* All done */
	handled = 1;

	if (sts & (STS_RI | STS_RU))     /* Rx interrupt (packet[s] arrived) */
	  de4x5_rx(dev);

	if (sts & (STS_TI | STS_TU))     /* Tx interrupt (packet sent) */
	  de4x5_tx(dev);

	if (sts & STS_LNF) {             /* TP Link has failed */
	    lp->irq_mask &= ~IMR_LFM;
	}

	if (sts & STS_UNF) {             /* Transmit underrun */
	    de4x5_txur(dev);
	}

	if (sts & STS_SE) {              /* Bus Error */
	    STOP_DE4X5;
	    printk("%s: Fatal bus error occurred, sts=%#8x, device stopped.\n",
		   dev->name, sts);
	    spin_unlock(&lp->lock);
	    return IRQ_HANDLED;
	}
    }

    /* Load the TX ring with any locally stored packets */
    if (!test_and_set_bit(0, (void *)&lp->cache.lock)) {
	while (!skb_queue_empty(&lp->cache.queue) && !netif_queue_stopped(dev) && lp->tx_enable) {
	    de4x5_queue_pkt(de4x5_get_cache(dev), dev);
	}
	lp->cache.lock = 0;
    }

    lp->interrupt = UNMASK_INTERRUPTS;
    ENABLE_IRQs;
    spin_unlock(&lp->lock);

    return IRQ_RETVAL(handled);
}

static int
de4x5_rx(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    u_long iobase = dev->base_addr;
    int entry;
    s32 status;

    for (entry=lp->rx_new; (s32)le32_to_cpu(lp->rx_ring[entry].status)>=0;
	                                                    entry=lp->rx_new) {
	status = (s32)le32_to_cpu(lp->rx_ring[entry].status);

	if (lp->rx_ovf) {
	    if (inl(DE4X5_MFC) & MFC_FOCM) {
		de4x5_rx_ovfc(dev);
		break;
	    }
	}

	if (status & RD_FS) {                 /* Remember the start of frame */
	    lp->rx_old = entry;
	}

	if (status & RD_LS) {                 /* Valid frame status */
	    if (lp->tx_enable) lp->linkOK++;
	    if (status & RD_ES) {	      /* There was an error. */
		lp->stats.rx_errors++;        /* Update the error stats. */
		if (status & (RD_RF | RD_TL)) lp->stats.rx_frame_errors++;
		if (status & RD_CE)           lp->stats.rx_crc_errors++;
		if (status & RD_OF)           lp->stats.rx_fifo_errors++;
		if (status & RD_TL)           lp->stats.rx_length_errors++;
		if (status & RD_RF)           lp->pktStats.rx_runt_frames++;
		if (status & RD_CS)           lp->pktStats.rx_collision++;
		if (status & RD_DB)           lp->pktStats.rx_dribble++;
		if (status & RD_OF)           lp->pktStats.rx_overflow++;
	    } else {                          /* A valid frame received */
		struct sk_buff *skb;
		short pkt_len = (short)(le32_to_cpu(lp->rx_ring[entry].status)
					                            >> 16) - 4;

		if ((skb = de4x5_alloc_rx_buff(dev, entry, pkt_len)) == NULL) {
		    printk("%s: Insufficient memory; nuking packet.\n",
			                                            dev->name);
		    lp->stats.rx_dropped++;
		} else {
		    de4x5_dbg_rx(skb, pkt_len);

		    /* Push up the protocol stack */
		    skb->protocol=eth_type_trans(skb,dev);
		    de4x5_local_stats(dev, skb->data, pkt_len);
		    netif_rx(skb);

		    /* Update stats */
		    lp->stats.rx_packets++;
 		    lp->stats.rx_bytes += pkt_len;
		}
	    }

	    /* Change buffer ownership for this frame, back to the adapter */
	    for (;lp->rx_old!=entry;lp->rx_old=(++lp->rx_old)%lp->rxRingSize) {
		lp->rx_ring[lp->rx_old].status = cpu_to_le32(R_OWN);
		barrier();
	    }
	    lp->rx_ring[entry].status = cpu_to_le32(R_OWN);
	    barrier();
	}

	/*
	** Update entry information
	*/
	lp->rx_new = (++lp->rx_new) % lp->rxRingSize;
    }

    return 0;
}

static inline void
de4x5_free_tx_buff(struct de4x5_private *lp, int entry)
{
    dma_unmap_single(lp->gendev, le32_to_cpu(lp->tx_ring[entry].buf),
		     le32_to_cpu(lp->tx_ring[entry].des1) & TD_TBS1,
		     DMA_TO_DEVICE);
    if ((u_long) lp->tx_skb[entry] > 1)
	dev_kfree_skb_irq(lp->tx_skb[entry]);
    lp->tx_skb[entry] = NULL;
}

/*
** Buffer sent - check for TX buffer errors.
*/
static int
de4x5_tx(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    u_long iobase = dev->base_addr;
    int entry;
    s32 status;

    for (entry = lp->tx_old; entry != lp->tx_new; entry = lp->tx_old) {
	status = (s32)le32_to_cpu(lp->tx_ring[entry].status);
	if (status < 0) {                     /* Buffer not sent yet */
	    break;
	} else if (status != 0x7fffffff) {    /* Not setup frame */
	    if (status & TD_ES) {             /* An error happened */
		lp->stats.tx_errors++;
		if (status & TD_NC) lp->stats.tx_carrier_errors++;
		if (status & TD_LC) lp->stats.tx_window_errors++;
		if (status & TD_UF) lp->stats.tx_fifo_errors++;
		if (status & TD_EC) lp->pktStats.excessive_collisions++;
		if (status & TD_DE) lp->stats.tx_aborted_errors++;

		if (TX_PKT_PENDING) {
		    outl(POLL_DEMAND, DE4X5_TPD);/* Restart a stalled TX */
		}
	    } else {                      /* Packet sent */
		lp->stats.tx_packets++;
		if (lp->tx_enable) lp->linkOK++;
	    }
	    /* Update the collision counter */
	    lp->stats.collisions += ((status & TD_EC) ? 16 :
				                      ((status & TD_CC) >> 3));

	    /* Free the buffer. */
	    if (lp->tx_skb[entry] != NULL)
	    	de4x5_free_tx_buff(lp, entry);
	}

	/* Update all the pointers */
	lp->tx_old = (++lp->tx_old) % lp->txRingSize;
    }

    /* Any resources available? */
    if (TX_BUFFS_AVAIL && netif_queue_stopped(dev)) {
	if (lp->interrupt)
	    netif_wake_queue(dev);
	else
	    netif_start_queue(dev);
    }

    return 0;
}

static void
de4x5_ast(struct net_device *dev)
{
	struct de4x5_private *lp = netdev_priv(dev);
	int next_tick = DE4X5_AUTOSENSE_MS;
	int dt;

	if (lp->useSROM)
		next_tick = srom_autoconf(dev);
	else if (lp->chipset == DC21140)
		next_tick = dc21140m_autoconf(dev);
	else if (lp->chipset == DC21041)
		next_tick = dc21041_autoconf(dev);
	else if (lp->chipset == DC21040)
		next_tick = dc21040_autoconf(dev);
	lp->linkOK = 0;

	dt = (next_tick * HZ) / 1000;

	if (!dt)
		dt = 1;

	mod_timer(&lp->timer, jiffies + dt);
}

static int
de4x5_txur(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    u_long iobase = dev->base_addr;
    int omr;

    omr = inl(DE4X5_OMR);
    if (!(omr & OMR_SF) || (lp->chipset==DC21041) || (lp->chipset==DC21040)) {
	omr &= ~(OMR_ST|OMR_SR);
	outl(omr, DE4X5_OMR);
	while (inl(DE4X5_STS) & STS_TS);
	if ((omr & OMR_TR) < OMR_TR) {
	    omr += 0x4000;
	} else {
	    omr |= OMR_SF;
	}
	outl(omr | OMR_ST | OMR_SR, DE4X5_OMR);
    }

    return 0;
}

static int
de4x5_rx_ovfc(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    u_long iobase = dev->base_addr;
    int omr;

    omr = inl(DE4X5_OMR);
    outl(omr & ~OMR_SR, DE4X5_OMR);
    while (inl(DE4X5_STS) & STS_RS);

    for (; (s32)le32_to_cpu(lp->rx_ring[lp->rx_new].status)>=0;) {
	lp->rx_ring[lp->rx_new].status = cpu_to_le32(R_OWN);
	lp->rx_new = (++lp->rx_new % lp->rxRingSize);
    }

    outl(omr, DE4X5_OMR);

    return 0;
}

static int
de4x5_close(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    u_long iobase = dev->base_addr;
    s32 imr, omr;

    disable_ast(dev);

    netif_stop_queue(dev);

    if (de4x5_debug & DEBUG_CLOSE) {
	printk("%s: Shutting down ethercard, status was %8.8x.\n",
	       dev->name, inl(DE4X5_STS));
    }

    /*
    ** We stop the DE4X5 here... mask interrupts and stop TX & RX
    */
    DISABLE_IRQs;
    STOP_DE4X5;

    /* Free the associated irq */
    free_irq(dev->irq, dev);
    lp->state = CLOSED;

    /* Free any socket buffers */
    de4x5_free_rx_buffs(dev);
    de4x5_free_tx_buffs(dev);

    /* Put the adapter to sleep to save power */
    yawn(dev, SLEEP);

    return 0;
}

static struct net_device_stats *
de4x5_get_stats(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    u_long iobase = dev->base_addr;

    lp->stats.rx_missed_errors = (int)(inl(DE4X5_MFC) & (MFC_OVFL | MFC_CNTR));

    return &lp->stats;
}

static void
de4x5_local_stats(struct net_device *dev, char *buf, int pkt_len)
{
    struct de4x5_private *lp = netdev_priv(dev);
    int i;

    for (i=1; i<DE4X5_PKT_STAT_SZ-1; i++) {
        if (pkt_len < (i*DE4X5_PKT_BIN_SZ)) {
	    lp->pktStats.bins[i]++;
	    i = DE4X5_PKT_STAT_SZ;
	}
    }
    if (buf[0] & 0x01) {          /* Multicast/Broadcast */
        if ((*(s32 *)&buf[0] == -1) && (*(s16 *)&buf[4] == -1)) {
	    lp->pktStats.broadcast++;
	} else {
	    lp->pktStats.multicast++;
	}
    } else if ((*(s32 *)&buf[0] == *(s32 *)&dev->dev_addr[0]) &&
	       (*(s16 *)&buf[4] == *(s16 *)&dev->dev_addr[4])) {
        lp->pktStats.unicast++;
    }

    lp->pktStats.bins[0]++;       /* Duplicates stats.rx_packets */
    if (lp->pktStats.bins[0] == 0) { /* Reset counters */
        memset((char *)&lp->pktStats, 0, sizeof(lp->pktStats));
    }

    return;
}

/*
** Removes the TD_IC flag from previous descriptor to improve TX performance.
** If the flag is changed on a descriptor that is being read by the hardware,
** I assume PCI transaction ordering will mean you are either successful or
** just miss asserting the change to the hardware. Anyway you're messing with
** a descriptor you don't own, but this shouldn't kill the chip provided
** the descriptor register is read only to the hardware.
*/
static void
load_packet(struct net_device *dev, char *buf, u32 flags, struct sk_buff *skb)
{
    struct de4x5_private *lp = netdev_priv(dev);
    int entry = (lp->tx_new ? lp->tx_new-1 : lp->txRingSize-1);
    dma_addr_t buf_dma = dma_map_single(lp->gendev, buf, flags & TD_TBS1, DMA_TO_DEVICE);

    lp->tx_ring[lp->tx_new].buf = cpu_to_le32(buf_dma);
    lp->tx_ring[lp->tx_new].des1 &= cpu_to_le32(TD_TER);
    lp->tx_ring[lp->tx_new].des1 |= cpu_to_le32(flags);
    lp->tx_skb[lp->tx_new] = skb;
    lp->tx_ring[entry].des1 &= cpu_to_le32(~TD_IC);
    barrier();

    lp->tx_ring[lp->tx_new].status = cpu_to_le32(T_OWN);
    barrier();
}

/*
** Set or clear the multicast filter for this adaptor.
*/
static void
set_multicast_list(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    u_long iobase = dev->base_addr;

    /* First, double check that the adapter is open */
    if (lp->state == OPEN) {
	if (dev->flags & IFF_PROMISC) {         /* set promiscuous mode */
	    u32 omr;
	    omr = inl(DE4X5_OMR);
	    omr |= OMR_PR;
	    outl(omr, DE4X5_OMR);
	} else {
	    SetMulticastFilter(dev);
	    load_packet(dev, lp->setup_frame, TD_IC | PERFECT_F | TD_SET |
			                                SETUP_FRAME_LEN, (struct sk_buff *)1);

	    lp->tx_new = (++lp->tx_new) % lp->txRingSize;
	    outl(POLL_DEMAND, DE4X5_TPD);       /* Start the TX */
	    dev->trans_start = jiffies;
	}
    }
}

/*
** Calculate the hash code and update the logical address filter
** from a list of ethernet multicast addresses.
** Little endian crc one liner from Matt Thomas, DEC.
*/
static void
SetMulticastFilter(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    struct dev_mc_list *dmi=dev->mc_list;
    u_long iobase = dev->base_addr;
    int i, j, bit, byte;
    u16 hashcode;
    u32 omr, crc;
    char *pa;
    unsigned char *addrs;

    omr = inl(DE4X5_OMR);
    omr &= ~(OMR_PR | OMR_PM);
    pa = build_setup_frame(dev, ALL);        /* Build the basic frame */

    if ((dev->flags & IFF_ALLMULTI) || (dev->mc_count > 14)) {
	omr |= OMR_PM;                       /* Pass all multicasts */
    } else if (lp->setup_f == HASH_PERF) {   /* Hash Filtering */
	for (i=0;i<dev->mc_count;i++) {      /* for each address in the list */
	    addrs=dmi->dmi_addr;
	    dmi=dmi->next;
	    if ((*addrs & 0x01) == 1) {      /* multicast address? */
		crc = ether_crc_le(ETH_ALEN, addrs);
		hashcode = crc & HASH_BITS;  /* hashcode is 9 LSb of CRC */

		byte = hashcode >> 3;        /* bit[3-8] -> byte in filter */
		bit = 1 << (hashcode & 0x07);/* bit[0-2] -> bit in byte */

		byte <<= 1;                  /* calc offset into setup frame */
		if (byte & 0x02) {
		    byte -= 1;
		}
		lp->setup_frame[byte] |= bit;
	    }
	}
    } else {                                 /* Perfect filtering */
	for (j=0; j<dev->mc_count; j++) {
	    addrs=dmi->dmi_addr;
	    dmi=dmi->next;
	    for (i=0; i<ETH_ALEN; i++) {
		*(pa + (i&1)) = *addrs++;
		if (i & 0x01) pa += 4;
	    }
	}
    }
    outl(omr, DE4X5_OMR);

    return;
}

#ifdef CONFIG_EISA

static u_char de4x5_irq[] = EISA_ALLOWED_IRQ_LIST;

static int __init de4x5_eisa_probe (struct device *gendev)
{
	struct eisa_device *edev;
	u_long iobase;
	u_char irq, regval;
	u_short vendor;
	u32 cfid;
	int status, device;
	struct net_device *dev;
	struct de4x5_private *lp;

	edev = to_eisa_device (gendev);
	iobase = edev->base_addr;

	if (!request_region (iobase, DE4X5_EISA_TOTAL_SIZE, "de4x5"))
		return -EBUSY;

	if (!request_region (iobase + DE4X5_EISA_IO_PORTS,
			     DE4X5_EISA_TOTAL_SIZE, "de4x5")) {
		status = -EBUSY;
		goto release_reg_1;
	}

	if (!(dev = alloc_etherdev (sizeof (struct de4x5_private)))) {
		status = -ENOMEM;
		goto release_reg_2;
	}
	lp = netdev_priv(dev);

	cfid = (u32) inl(PCI_CFID);
	lp->cfrv = (u_short) inl(PCI_CFRV);
	device = (cfid >> 8) & 0x00ffff00;
	vendor = (u_short) cfid;

	/* Read the EISA Configuration Registers */
	regval = inb(EISA_REG0) & (ER0_INTL | ER0_INTT);
#ifdef CONFIG_ALPHA
	/* Looks like the Jensen firmware (rev 2.2) doesn't really
	 * care about the EISA configuration, and thus doesn't
	 * configure the PLX bridge properly. Oh well... Simply mimic
	 * the EISA config file to sort it out. */

	/* EISA REG1: Assert DecChip 21040 HW Reset */
	outb (ER1_IAM | 1, EISA_REG1);
	mdelay (1);

        /* EISA REG1: Deassert DecChip 21040 HW Reset */
	outb (ER1_IAM, EISA_REG1);
	mdelay (1);

	/* EISA REG3: R/W Burst Transfer Enable */
	outb (ER3_BWE | ER3_BRE, EISA_REG3);

	/* 32_bit slave/master, Preempt Time=23 bclks, Unlatched Interrupt */
	outb (ER0_BSW | ER0_BMW | ER0_EPT | regval, EISA_REG0);
#endif
	irq = de4x5_irq[(regval >> 1) & 0x03];

	if (is_DC2114x) {
	    device = ((lp->cfrv & CFRV_RN) < DC2114x_BRK ? DC21142 : DC21143);
	}
	lp->chipset = device;
	lp->bus = EISA;

	/* Write the PCI Configuration Registers */
	outl(PCI_COMMAND_IO | PCI_COMMAND_MASTER, PCI_CFCS);
	outl(0x00006000, PCI_CFLT);
	outl(iobase, PCI_CBIO);

	DevicePresent(dev, EISA_APROM);

	dev->irq = irq;

	if (!(status = de4x5_hw_init (dev, iobase, gendev))) {
		return 0;
	}

	free_netdev (dev);
 release_reg_2:
	release_region (iobase + DE4X5_EISA_IO_PORTS, DE4X5_EISA_TOTAL_SIZE);
 release_reg_1:
	release_region (iobase, DE4X5_EISA_TOTAL_SIZE);

	return status;
}

static int __devexit de4x5_eisa_remove (struct device *device)
{
	struct net_device *dev;
	u_long iobase;

	dev = dev_get_drvdata(device);
	iobase = dev->base_addr;

	unregister_netdev (dev);
	free_netdev (dev);
	release_region (iobase + DE4X5_EISA_IO_PORTS, DE4X5_EISA_TOTAL_SIZE);
	release_region (iobase, DE4X5_EISA_TOTAL_SIZE);

	return 0;
}

static struct eisa_device_id de4x5_eisa_ids[] = {
        { "DEC4250", 0 },	/* 0 is the board name index... */
        { "" }
};
MODULE_DEVICE_TABLE(eisa, de4x5_eisa_ids);

static struct eisa_driver de4x5_eisa_driver = {
        .id_table = de4x5_eisa_ids,
        .driver   = {
                .name    = "de4x5",
                .probe   = de4x5_eisa_probe,
                .remove  = __devexit_p (de4x5_eisa_remove),
        }
};
MODULE_DEVICE_TABLE(eisa, de4x5_eisa_ids);
#endif

#ifdef CONFIG_PCI

/*
** This function searches the current bus (which is >0) for a DECchip with an
** SROM, so that in multiport cards that have one SROM shared between multiple
** DECchips, we can find the base SROM irrespective of the BIOS scan direction.
** For single port cards this is a time waster...
*/
static void __devinit
srom_search(struct net_device *dev, struct pci_dev *pdev)
{
    u_char pb;
    u_short vendor, status;
    u_int irq = 0, device;
    u_long iobase = 0;                     /* Clear upper 32 bits in Alphas */
    int i, j;
    struct de4x5_private *lp = netdev_priv(dev);
    struct list_head *walk;

    list_for_each(walk, &pdev->bus_list) {
	struct pci_dev *this_dev = pci_dev_b(walk);

	/* Skip the pci_bus list entry */
	if (list_entry(walk, struct pci_bus, devices) == pdev->bus) continue;

	vendor = this_dev->vendor;
	device = this_dev->device << 8;
	if (!(is_DC21040 || is_DC21041 || is_DC21140 || is_DC2114x)) continue;

	/* Get the chip configuration revision register */
	pb = this_dev->bus->number;

	/* Set the device number information */
	lp->device = PCI_SLOT(this_dev->devfn);
	lp->bus_num = pb;

	/* Set the chipset information */
	if (is_DC2114x) {
	    device = ((this_dev->revision & CFRV_RN) < DC2114x_BRK
		      ? DC21142 : DC21143);
	}
	lp->chipset = device;

	/* Get the board I/O address (64 bits on sparc64) */
	iobase = pci_resource_start(this_dev, 0);

	/* Fetch the IRQ to be used */
	irq = this_dev->irq;
	if ((irq == 0) || (irq == 0xff) || ((int)irq == -1)) continue;

	/* Check if I/O accesses are enabled */
	pci_read_config_word(this_dev, PCI_COMMAND, &status);
	if (!(status & PCI_COMMAND_IO)) continue;

	/* Search for a valid SROM attached to this DECchip */
	DevicePresent(dev, DE4X5_APROM);
	for (j=0, i=0; i<ETH_ALEN; i++) {
	    j += (u_char) *((u_char *)&lp->srom + SROM_HWADD + i);
	}
	if (j != 0 && j != 6 * 0xff) {
	    last.chipset = device;
	    last.bus = pb;
	    last.irq = irq;
	    for (i=0; i<ETH_ALEN; i++) {
		last.addr[i] = (u_char)*((u_char *)&lp->srom + SROM_HWADD + i);
	    }
	    return;
	}
    }

    return;
}

/*
** PCI bus I/O device probe
** NB: PCI I/O accesses and Bus Mastering are enabled by the PCI BIOS, not
** the driver. Some PCI BIOS's, pre V2.1, need the slot + features to be
** enabled by the user first in the set up utility. Hence we just check for
** enabled features and silently ignore the card if they're not.
**
** STOP PRESS: Some BIOS's __require__ the driver to enable the bus mastering
** bit. Here, check for I/O accesses and then set BM. If you put the card in
** a non BM slot, you're on your own (and complain to the PC vendor that your
** PC doesn't conform to the PCI standard)!
**
** This function is only compatible with the *latest* 2.1.x kernels. For 2.0.x
** kernels use the V0.535[n] drivers.
*/

static int __devinit de4x5_pci_probe (struct pci_dev *pdev,
				   const struct pci_device_id *ent)
{
	u_char pb, pbus = 0, dev_num, dnum = 0, timer;
	u_short vendor, status;
	u_int irq = 0, device;
	u_long iobase = 0;	/* Clear upper 32 bits in Alphas */
	int error;
	struct net_device *dev;
	struct de4x5_private *lp;

	dev_num = PCI_SLOT(pdev->devfn);
	pb = pdev->bus->number;

	if (io) { /* probe a single PCI device */
		pbus = (u_short)(io >> 8);
		dnum = (u_short)(io & 0xff);
		if ((pbus != pb) || (dnum != dev_num))
			return -ENODEV;
	}

	vendor = pdev->vendor;
	device = pdev->device << 8;
	if (!(is_DC21040 || is_DC21041 || is_DC21140 || is_DC2114x))
		return -ENODEV;

	/* Ok, the device seems to be for us. */
	if ((error = pci_enable_device (pdev)))
		return error;

	if (!(dev = alloc_etherdev (sizeof (struct de4x5_private)))) {
		error = -ENOMEM;
		goto disable_dev;
	}

	lp = netdev_priv(dev);
	lp->bus = PCI;
	lp->bus_num = 0;

	/* Search for an SROM on this bus */
	if (lp->bus_num != pb) {
	    lp->bus_num = pb;
	    srom_search(dev, pdev);
	}

	/* Get the chip configuration revision register */
	lp->cfrv = pdev->revision;

	/* Set the device number information */
	lp->device = dev_num;
	lp->bus_num = pb;

	/* Set the chipset information */
	if (is_DC2114x) {
	    device = ((lp->cfrv & CFRV_RN) < DC2114x_BRK ? DC21142 : DC21143);
	}
	lp->chipset = device;

	/* Get the board I/O address (64 bits on sparc64) */
	iobase = pci_resource_start(pdev, 0);

	/* Fetch the IRQ to be used */
	irq = pdev->irq;
	if ((irq == 0) || (irq == 0xff) || ((int)irq == -1)) {
		error = -ENODEV;
		goto free_dev;
	}

	/* Check if I/O accesses and Bus Mastering are enabled */
	pci_read_config_word(pdev, PCI_COMMAND, &status);
#ifdef __powerpc__
	if (!(status & PCI_COMMAND_IO)) {
	    status |= PCI_COMMAND_IO;
	    pci_write_config_word(pdev, PCI_COMMAND, status);
	    pci_read_config_word(pdev, PCI_COMMAND, &status);
	}
#endif /* __powerpc__ */
	if (!(status & PCI_COMMAND_IO)) {
		error = -ENODEV;
		goto free_dev;
	}

	if (!(status & PCI_COMMAND_MASTER)) {
	    status |= PCI_COMMAND_MASTER;
	    pci_write_config_word(pdev, PCI_COMMAND, status);
	    pci_read_config_word(pdev, PCI_COMMAND, &status);
	}
	if (!(status & PCI_COMMAND_MASTER)) {
		error = -ENODEV;
		goto free_dev;
	}

	/* Check the latency timer for values >= 0x60 */
	pci_read_config_byte(pdev, PCI_LATENCY_TIMER, &timer);
	if (timer < 0x60) {
	    pci_write_config_byte(pdev, PCI_LATENCY_TIMER, 0x60);
	}

	DevicePresent(dev, DE4X5_APROM);

	if (!request_region (iobase, DE4X5_PCI_TOTAL_SIZE, "de4x5")) {
		error = -EBUSY;
		goto free_dev;
	}

	dev->irq = irq;

	if ((error = de4x5_hw_init(dev, iobase, &pdev->dev))) {
		goto release;
	}

	return 0;

 release:
	release_region (iobase, DE4X5_PCI_TOTAL_SIZE);
 free_dev:
	free_netdev (dev);
 disable_dev:
	pci_disable_device (pdev);
	return error;
}

static void __devexit de4x5_pci_remove (struct pci_dev *pdev)
{
	struct net_device *dev;
	u_long iobase;

	dev = dev_get_drvdata(&pdev->dev);
	iobase = dev->base_addr;

	unregister_netdev (dev);
	free_netdev (dev);
	release_region (iobase, DE4X5_PCI_TOTAL_SIZE);
	pci_disable_device (pdev);
}

static struct pci_device_id de4x5_pci_tbl[] = {
        { PCI_VENDOR_ID_DEC, PCI_DEVICE_ID_DEC_TULIP,
          PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
        { PCI_VENDOR_ID_DEC, PCI_DEVICE_ID_DEC_TULIP_PLUS,
          PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1 },
        { PCI_VENDOR_ID_DEC, PCI_DEVICE_ID_DEC_TULIP_FAST,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 2 },
        { PCI_VENDOR_ID_DEC, PCI_DEVICE_ID_DEC_21142,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 3 },
        { },
};

static struct pci_driver de4x5_pci_driver = {
        .name           = "de4x5",
        .id_table       = de4x5_pci_tbl,
        .probe          = de4x5_pci_probe,
	.remove         = __devexit_p (de4x5_pci_remove),
};

#endif

/*
** Auto configure the media here rather than setting the port at compile
** time. This routine is called by de4x5_init() and when a loss of media is
** detected (excessive collisions, loss of carrier, no carrier or link fail
** [TP] or no recent receive activity) to check whether the user has been
** sneaky and changed the port on us.
*/
static int
autoconf_media(struct net_device *dev)
{
	struct de4x5_private *lp = netdev_priv(dev);
	u_long iobase = dev->base_addr;

	disable_ast(dev);

	lp->c_media = AUTO;                     /* Bogus last media */
	inl(DE4X5_MFC);                         /* Zero the lost frames counter */
	lp->media = INIT;
	lp->tcount = 0;

	de4x5_ast(dev);

	return lp->media;
}

/*
** Autoconfigure the media when using the DC21040. AUI cannot be distinguished
** from BNC as the port has a jumper to set thick or thin wire. When set for
** BNC, the BNC port will indicate activity if it's not terminated correctly.
** The only way to test for that is to place a loopback packet onto the
** network and watch for errors. Since we're messing with the interrupt mask
** register, disable the board interrupts and do not allow any more packets to
** be queued to the hardware. Re-enable everything only when the media is
** found.
** I may have to "age out" locally queued packets so that the higher layer
** timeouts don't effectively duplicate packets on the network.
*/
static int
dc21040_autoconf(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    u_long iobase = dev->base_addr;
    int next_tick = DE4X5_AUTOSENSE_MS;
    s32 imr;

    switch (lp->media) {
    case INIT:
	DISABLE_IRQs;
	lp->tx_enable = false;
	lp->timeout = -1;
	de4x5_save_skbs(dev);
	if ((lp->autosense == AUTO) || (lp->autosense == TP)) {
	    lp->media = TP;
	} else if ((lp->autosense == BNC) || (lp->autosense == AUI) || (lp->autosense == BNC_AUI)) {
	    lp->media = BNC_AUI;
	} else if (lp->autosense == EXT_SIA) {
	    lp->media = EXT_SIA;
	} else {
	    lp->media = NC;
	}
	lp->local_state = 0;
	next_tick = dc21040_autoconf(dev);
	break;

    case TP:
	next_tick = dc21040_state(dev, 0x8f01, 0xffff, 0x0000, 3000, BNC_AUI,
		                                         TP_SUSPECT, test_tp);
	break;

    case TP_SUSPECT:
	next_tick = de4x5_suspect_state(dev, 1000, TP, test_tp, dc21040_autoconf);
	break;

    case BNC:
    case AUI:
    case BNC_AUI:
	next_tick = dc21040_state(dev, 0x8f09, 0x0705, 0x0006, 3000, EXT_SIA,
		                                  BNC_AUI_SUSPECT, ping_media);
	break;

    case BNC_AUI_SUSPECT:
	next_tick = de4x5_suspect_state(dev, 1000, BNC_AUI, ping_media, dc21040_autoconf);
	break;

    case EXT_SIA:
	next_tick = dc21040_state(dev, 0x3041, 0x0000, 0x0006, 3000,
		                              NC, EXT_SIA_SUSPECT, ping_media);
	break;

    case EXT_SIA_SUSPECT:
	next_tick = de4x5_suspect_state(dev, 1000, EXT_SIA, ping_media, dc21040_autoconf);
	break;

    case NC:
	/* default to TP for all */
	reset_init_sia(dev, 0x8f01, 0xffff, 0x0000);
	if (lp->media != lp->c_media) {
	    de4x5_dbg_media(dev);
	    lp->c_media = lp->media;
	}
	lp->media = INIT;
	lp->tx_enable = false;
	break;
    }

    return next_tick;
}

static int
dc21040_state(struct net_device *dev, int csr13, int csr14, int csr15, int timeout,
	      int next_state, int suspect_state,
	      int (*fn)(struct net_device *, int))
{
    struct de4x5_private *lp = netdev_priv(dev);
    int next_tick = DE4X5_AUTOSENSE_MS;
    int linkBad;

    switch (lp->local_state) {
    case 0:
	reset_init_sia(dev, csr13, csr14, csr15);
	lp->local_state++;
	next_tick = 500;
	break;

    case 1:
	if (!lp->tx_enable) {
	    linkBad = fn(dev, timeout);
	    if (linkBad < 0) {
		next_tick = linkBad & ~TIMER_CB;
	    } else {
		if (linkBad && (lp->autosense == AUTO)) {
		    lp->local_state = 0;
		    lp->media = next_state;
		} else {
		    de4x5_init_connection(dev);
		}
	    }
	} else if (!lp->linkOK && (lp->autosense == AUTO)) {
	    lp->media = suspect_state;
	    next_tick = 3000;
	}
	break;
    }

    return next_tick;
}

static int
de4x5_suspect_state(struct net_device *dev, int timeout, int prev_state,
		      int (*fn)(struct net_device *, int),
		      int (*asfn)(struct net_device *))
{
    struct de4x5_private *lp = netdev_priv(dev);
    int next_tick = DE4X5_AUTOSENSE_MS;
    int linkBad;

    switch (lp->local_state) {
    case 1:
	if (lp->linkOK) {
	    lp->media = prev_state;
	} else {
	    lp->local_state++;
	    next_tick = asfn(dev);
	}
	break;

    case 2:
	linkBad = fn(dev, timeout);
	if (linkBad < 0) {
	    next_tick = linkBad & ~TIMER_CB;
	} else if (!linkBad) {
	    lp->local_state--;
	    lp->media = prev_state;
	} else {
	    lp->media = INIT;
	    lp->tcount++;
	}
    }

    return next_tick;
}

/*
** Autoconfigure the media when using the DC21041. AUI needs to be tested
** before BNC, because the BNC port will indicate activity if it's not
** terminated correctly. The only way to test for that is to place a loopback
** packet onto the network and watch for errors. Since we're messing with
** the interrupt mask register, disable the board interrupts and do not allow
** any more packets to be queued to the hardware. Re-enable everything only
** when the media is found.
*/
static int
dc21041_autoconf(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    u_long iobase = dev->base_addr;
    s32 sts, irqs, irq_mask, imr, omr;
    int next_tick = DE4X5_AUTOSENSE_MS;

    switch (lp->media) {
    case INIT:
	DISABLE_IRQs;
	lp->tx_enable = false;
	lp->timeout = -1;
	de4x5_save_skbs(dev);          /* Save non transmitted skb's */
	if ((lp->autosense == AUTO) || (lp->autosense == TP_NW)) {
	    lp->media = TP;            /* On chip auto negotiation is broken */
	} else if (lp->autosense == TP) {
	    lp->media = TP;
	} else if (lp->autosense == BNC) {
	    lp->media = BNC;
	} else if (lp->autosense == AUI) {
	    lp->media = AUI;
	} else {
	    lp->media = NC;
	}
	lp->local_state = 0;
	next_tick = dc21041_autoconf(dev);
	break;

    case TP_NW:
	if (lp->timeout < 0) {
	    omr = inl(DE4X5_OMR);/* Set up full duplex for the autonegotiate */
	    outl(omr | OMR_FDX, DE4X5_OMR);
	}
	irqs = STS_LNF | STS_LNP;
	irq_mask = IMR_LFM | IMR_LPM;
	sts = test_media(dev, irqs, irq_mask, 0xef01, 0xffff, 0x0008, 2400);
	if (sts < 0) {
	    next_tick = sts & ~TIMER_CB;
	} else {
	    if (sts & STS_LNP) {
		lp->media = ANS;
	    } else {
		lp->media = AUI;
	    }
	    next_tick = dc21041_autoconf(dev);
	}
	break;

    case ANS:
	if (!lp->tx_enable) {
	    irqs = STS_LNP;
	    irq_mask = IMR_LPM;
	    sts = test_ans(dev, irqs, irq_mask, 3000);
	    if (sts < 0) {
		next_tick = sts & ~TIMER_CB;
	    } else {
		if (!(sts & STS_LNP) && (lp->autosense == AUTO)) {
		    lp->media = TP;
		    next_tick = dc21041_autoconf(dev);
		} else {
		    lp->local_state = 1;
		    de4x5_init_connection(dev);
		}
	    }
	} else if (!lp->linkOK && (lp->autosense == AUTO)) {
	    lp->media = ANS_SUSPECT;
	    next_tick = 3000;
	}
	break;

    case ANS_SUSPECT:
	next_tick = de4x5_suspect_state(dev, 1000, ANS, test_tp, dc21041_autoconf);
	break;

    case TP:
	if (!lp->tx_enable) {
	    if (lp->timeout < 0) {
		omr = inl(DE4X5_OMR);          /* Set up half duplex for TP */
		outl(omr & ~OMR_FDX, DE4X5_OMR);
	    }
	    irqs = STS_LNF | STS_LNP;
	    irq_mask = IMR_LFM | IMR_LPM;
	    sts = test_media(dev,irqs, irq_mask, 0xef01, 0xff3f, 0x0008, 2400);
	    if (sts < 0) {
		next_tick = sts & ~TIMER_CB;
	    } else {
		if (!(sts & STS_LNP) && (lp->autosense == AUTO)) {
		    if (inl(DE4X5_SISR) & SISR_NRA) {
			lp->media = AUI;       /* Non selected port activity */
		    } else {
			lp->media = BNC;
		    }
		    next_tick = dc21041_autoconf(dev);
		} else {
		    lp->local_state = 1;
		    de4x5_init_connection(dev);
		}
	    }
	} else if (!lp->linkOK && (lp->autosense == AUTO)) {
	    lp->media = TP_SUSPECT;
	    next_tick = 3000;
	}
	break;

    case TP_SUSPECT:
	next_tick = de4x5_suspect_state(dev, 1000, TP, test_tp, dc21041_autoconf);
	break;

    case AUI:
	if (!lp->tx_enable) {
	    if (lp->timeout < 0) {
		omr = inl(DE4X5_OMR);          /* Set up half duplex for AUI */
		outl(omr & ~OMR_FDX, DE4X5_OMR);
	    }
	    irqs = 0;
	    irq_mask = 0;
	    sts = test_media(dev,irqs, irq_mask, 0xef09, 0xf73d, 0x000e, 1000);
	    if (sts < 0) {
		next_tick = sts & ~TIMER_CB;
	    } else {
		if (!(inl(DE4X5_SISR) & SISR_SRA) && (lp->autosense == AUTO)) {
		    lp->media = BNC;
		    next_tick = dc21041_autoconf(dev);
		} else {
		    lp->local_state = 1;
		    de4x5_init_connection(dev);
		}
	    }
	} else if (!lp->linkOK && (lp->autosense == AUTO)) {
	    lp->media = AUI_SUSPECT;
	    next_tick = 3000;
	}
	break;

    case AUI_SUSPECT:
	next_tick = de4x5_suspect_state(dev, 1000, AUI, ping_media, dc21041_autoconf);
	break;

    case BNC:
	switch (lp->local_state) {
	case 0:
	    if (lp->timeout < 0) {
		omr = inl(DE4X5_OMR);          /* Set up half duplex for BNC */
		outl(omr & ~OMR_FDX, DE4X5_OMR);
	    }
	    irqs = 0;
	    irq_mask = 0;
	    sts = test_media(dev,irqs, irq_mask, 0xef09, 0xf73d, 0x0006, 1000);
	    if (sts < 0) {
		next_tick = sts & ~TIMER_CB;
	    } else {
		lp->local_state++;             /* Ensure media connected */
		next_tick = dc21041_autoconf(dev);
	    }
	    break;

	case 1:
	    if (!lp->tx_enable) {
		if ((sts = ping_media(dev, 3000)) < 0) {
		    next_tick = sts & ~TIMER_CB;
		} else {
		    if (sts) {
			lp->local_state = 0;
			lp->media = NC;
		    } else {
			de4x5_init_connection(dev);
		    }
		}
	    } else if (!lp->linkOK && (lp->autosense == AUTO)) {
		lp->media = BNC_SUSPECT;
		next_tick = 3000;
	    }
	    break;
	}
	break;

    case BNC_SUSPECT:
	next_tick = de4x5_suspect_state(dev, 1000, BNC, ping_media, dc21041_autoconf);
	break;

    case NC:
	omr = inl(DE4X5_OMR);    /* Set up full duplex for the autonegotiate */
	outl(omr | OMR_FDX, DE4X5_OMR);
	reset_init_sia(dev, 0xef01, 0xffff, 0x0008);/* Initialise the SIA */
	if (lp->media != lp->c_media) {
	    de4x5_dbg_media(dev);
	    lp->c_media = lp->media;
	}
	lp->media = INIT;
	lp->tx_enable = false;
	break;
    }

    return next_tick;
}

/*
** Some autonegotiation chips are broken in that they do not return the
** acknowledge bit (anlpa & MII_ANLPA_ACK) in the link partner advertisement
** register, except at the first power up negotiation.
*/
static int
dc21140m_autoconf(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    int ana, anlpa, cap, cr, slnk, sr;
    int next_tick = DE4X5_AUTOSENSE_MS;
    u_long imr, omr, iobase = dev->base_addr;

    switch(lp->media) {
    case INIT:
        if (lp->timeout < 0) {
	    DISABLE_IRQs;
	    lp->tx_enable = false;
	    lp->linkOK = 0;
	    de4x5_save_skbs(dev);          /* Save non transmitted skb's */
	}
	if ((next_tick = de4x5_reset_phy(dev)) < 0) {
	    next_tick &= ~TIMER_CB;
	} else {
	    if (lp->useSROM) {
		if (srom_map_media(dev) < 0) {
		    lp->tcount++;
		    return next_tick;
		}
		srom_exec(dev, lp->phy[lp->active].gep);
		if (lp->infoblock_media == ANS) {
		    ana = lp->phy[lp->active].ana | MII_ANA_CSMA;
		    mii_wr(ana, MII_ANA, lp->phy[lp->active].addr, DE4X5_MII);
		}
	    } else {
		lp->tmp = MII_SR_ASSC;     /* Fake out the MII speed set */
		SET_10Mb;
		if (lp->autosense == _100Mb) {
		    lp->media = _100Mb;
		} else if (lp->autosense == _10Mb) {
		    lp->media = _10Mb;
		} else if ((lp->autosense == AUTO) &&
			            ((sr=is_anc_capable(dev)) & MII_SR_ANC)) {
		    ana = (((sr >> 6) & MII_ANA_TAF) | MII_ANA_CSMA);
		    ana &= (lp->fdx ? ~0 : ~MII_ANA_FDAM);
		    mii_wr(ana, MII_ANA, lp->phy[lp->active].addr, DE4X5_MII);
		    lp->media = ANS;
		} else if (lp->autosense == AUTO) {
		    lp->media = SPD_DET;
		} else if (is_spd_100(dev) && is_100_up(dev)) {
		    lp->media = _100Mb;
		} else {
		    lp->media = NC;
		}
	    }
	    lp->local_state = 0;
	    next_tick = dc21140m_autoconf(dev);
	}
	break;

    case ANS:
	switch (lp->local_state) {
	case 0:
	    if (lp->timeout < 0) {
		mii_wr(MII_CR_ASSE | MII_CR_RAN, MII_CR, lp->phy[lp->active].addr, DE4X5_MII);
	    }
	    cr = test_mii_reg(dev, MII_CR, MII_CR_RAN, false, 500);
	    if (cr < 0) {
		next_tick = cr & ~TIMER_CB;
	    } else {
		if (cr) {
		    lp->local_state = 0;
		    lp->media = SPD_DET;
		} else {
		    lp->local_state++;
		}
		next_tick = dc21140m_autoconf(dev);
	    }
	    break;

	case 1:
	    if ((sr=test_mii_reg(dev, MII_SR, MII_SR_ASSC, true, 2000)) < 0) {
		next_tick = sr & ~TIMER_CB;
	    } else {
		lp->media = SPD_DET;
		lp->local_state = 0;
		if (sr) {                         /* Success! */
		    lp->tmp = MII_SR_ASSC;
		    anlpa = mii_rd(MII_ANLPA, lp->phy[lp->active].addr, DE4X5_MII);
		    ana = mii_rd(MII_ANA, lp->phy[lp->active].addr, DE4X5_MII);
		    if (!(anlpa & MII_ANLPA_RF) &&
			 (cap = anlpa & MII_ANLPA_TAF & ana)) {
			if (cap & MII_ANA_100M) {
			    lp->fdx = (ana & anlpa & MII_ANA_FDAM & MII_ANA_100M) != 0;
			    lp->media = _100Mb;
			} else if (cap & MII_ANA_10M) {
			    lp->fdx = (ana & anlpa & MII_ANA_FDAM & MII_ANA_10M) != 0;

			    lp->media = _10Mb;
			}
		    }
		}                       /* Auto Negotiation failed to finish */
		next_tick = dc21140m_autoconf(dev);
	    }                           /* Auto Negotiation failed to start */
	    break;
	}
	break;

    case SPD_DET:                              /* Choose 10Mb/s or 100Mb/s */
        if (lp->timeout < 0) {
	    lp->tmp = (lp->phy[lp->active].id ? MII_SR_LKS :
		                                  (~gep_rd(dev) & GEP_LNP));
	    SET_100Mb_PDET;
	}
        if ((slnk = test_for_100Mb(dev, 6500)) < 0) {
	    next_tick = slnk & ~TIMER_CB;
	} else {
	    if (is_spd_100(dev) && is_100_up(dev)) {
		lp->media = _100Mb;
	    } else if ((!is_spd_100(dev) && (is_10_up(dev) & lp->tmp))) {
		lp->media = _10Mb;
	    } else {
		lp->media = NC;
	    }
	    next_tick = dc21140m_autoconf(dev);
	}
	break;

    case _100Mb:                               /* Set 100Mb/s */
        next_tick = 3000;
	if (!lp->tx_enable) {
	    SET_100Mb;
	    de4x5_init_connection(dev);
	} else {
	    if (!lp->linkOK && (lp->autosense == AUTO)) {
		if (!is_100_up(dev) || (!lp->useSROM && !is_spd_100(dev))) {
		    lp->media = INIT;
		    lp->tcount++;
		    next_tick = DE4X5_AUTOSENSE_MS;
		}
	    }
	}
	break;

    case BNC:
    case AUI:
    case _10Mb:                                /* Set 10Mb/s */
        next_tick = 3000;
	if (!lp->tx_enable) {
	    SET_10Mb;
	    de4x5_init_connection(dev);
	} else {
	    if (!lp->linkOK && (lp->autosense == AUTO)) {
		if (!is_10_up(dev) || (!lp->useSROM && is_spd_100(dev))) {
		    lp->media = INIT;
		    lp->tcount++;
		    next_tick = DE4X5_AUTOSENSE_MS;
		}
	    }
	}
	break;

    case NC:
        if (lp->media != lp->c_media) {
	    de4x5_dbg_media(dev);
	    lp->c_media = lp->media;
	}
	lp->media = INIT;
	lp->tx_enable = false;
	break;
    }

    return next_tick;
}

/*
** This routine may be merged into dc21140m_autoconf() sometime as I'm
** changing how I figure out the media - but trying to keep it backwards
** compatible with the de500-xa and de500-aa.
** Whether it's BNC, AUI, SYM or MII is sorted out in the infoblock
** functions and set during de4x5_mac_port() and/or de4x5_reset_phy().
** This routine just has to figure out whether 10Mb/s or 100Mb/s is
** active.
** When autonegotiation is working, the ANS part searches the SROM for
** the highest common speed (TP) link that both can run and if that can
** be full duplex. That infoblock is executed and then the link speed set.
**
** Only _10Mb and _100Mb are tested here.
*/
static int
dc2114x_autoconf(struct net_device *dev)
{
    struct de4x5_private *lp = netdev_priv(dev);
    u_long iobase = dev->base_addr;
    s32 cr, anlpa, ana, cap, irqs, irq_mask, imr, omr, slnk, sr, sts;
    int next_tick = DE4X5_AUTOSENSE_MS;

    switch (lp->media) {
    case INIT:
        if (lp->timeout < 0) {
	    DISABLE_IRQs;
	    lp->tx_enable = false;
	    lp->linkOK = 0;
            lp->timeout = -1;
	    de4x5_save_skbs(dev);            /* Save non transmitted skb's */
	    if (lp->params.autosense & ~AUTO) {
		srom_map_media(dev);         /* Fixed media requested      */
		if (lp->media != lp->params.autosense) {
		    lp->tcount++;
		    lp->media = INIT;
		    return next_tick;
		}
		lp->media = INIT;
	    }
	}
	if ((next_tick = de4x5_reset_phy(dev)) < 0) {
	    next_tick &= ~TIMER_CB;
	} else {
	    if (lp->autosense == _100Mb) {
		lp->media = _100Mb;
	    } else if (lp->autosense == _10Mb) {
		lp->media = _10Mb;
	    } else if (lp->autosense == TP) {
		lp->media = TP;
	    } else if (lp->autosense == BNC) {
		lp->media = BNC;
	    } else if (lp->autosense == AUI) {
		lp->media = AUI;
	    } else {
		lp->media = SPD_DET;
		if ((lp->infoblock_media == ANS) &&
		                    ((sr=is_anc_capable(dev)) & MII_SR_ANC)) {
		    ana = (((sr >> 6) & MII_ANA_TAF) | MII_ANA_CSMA);
		    ana &= (lp->fdx ? ~0 : ~MII_ANA_FDAM);
		    mii_wr(ana, MII_ANA, lp->phy[lp->active].addr, DE4X5_MII);
		    lp->media = ANS;
		}
	    }
	    lp->local_state = 0;
	    next_tick = dc2114x_autoconf(dev);
        }
	break;

    case ANS:
	switch (lp->local_state) {
	case 0:
	    if (lp->timeout < 0) {
		mii_wr(MII_CR_ASSE | MII_CR_RAN, MII_CR, lp->phy[lp->active].addr, DE4X5_MII);
	    }
	    cr = test_mii_reg(dev, MII_CR, MII_CR_RAN, false, 500);
	    if (cr < 0) {
		next_tick = cr & ~TIMER_CB;
	    } else {
		if (cr) {
		    lp->local_state = 0;
		    lp->media = SPD_DET;
		} else {
		    lp->local_state++;
		}
		next_tick = dc2114x_autoconf(dev);
	    }
	    break;

	case 1:
	    sr = test_mii_reg(dev, MII_SR DECchip_ASSC, true, 2000); de4x5if (sr < 0) {
		next_tick =5.c:& ~TIMER_CB50/DE50} else    elp->media = SPD_DET;1994, 1local_state = 0ipme0
    ) {e4x5ces for this driver h/* Success! */
	de4x54, 1tmp =and DE425/DEipmeces anlpDigiTAL Dd(ECchANLPA,le
  phy[4, 1active].addr, DE4X5_MII450/es Resech Center (mjacobas.nasa.gov).

    The author may be reached at0
  !(search&DECchob@na_RF) &&
			 (cain p redistribute  it TAF &t da)     eestiny ittribute A_100MGNU Genlable
  fdx = ( dav  thedistribute A_FDAMublic License as !
    Teished by 995 Diginse ab the pyrighteral  Public Licenseas published by the
    Free Software Foundation;  either verson 2 of the  License, or (at yor
    opched atDING}rces for this driver have Auto Negotiation failed to finishe avaithernet drivdc2114x_autoconf1x4x450/DE50T  LIMITED  TO, THE IMPLIMPLIED WARRANTIES OF
    MERCHANTstart e avaENT break;
UDINRECT,  
ENT case AUI:
estin!e
   x_enable     e; you ce
   imeout         eomc: Ainl( may bOMR); have bet up half duplex forNTALces for  avaioutl(NG, & ~OMR_FDXor may bLIMITRE DISC CONSEQrqs

    TFITS; O_masdrivUSINESS stR  B DIGIT95 D1x4x ; OR, INTERRUP, 0LIABILIT1/DE450/DE500
   ts         ethernet drivets for Linux.

    Copyright 199ou canT
    NOT SISR) & ISIN_SRAd/orENTIALULARsense == AUTO GNU Ge License, or (atBNA Ames ReFITNESS FOR A PARTICULAR PURPOSE AREoption) FTWARE, EVENorporation.

 1ached atde4x5_init_connecOF
 
    You s,  OR PROption) any AL, ElinkOKOUT OF THE USE OF
    THIS SOFTW License, or (atAUI_SUSPECuipmTHE POSSIBILITY 300   TNDIRECT,
    INCIDENTALass Ave,:
 FITNESS FOR Aeral suspectation.1x4x DWHET,75 M, pingAND ON, A PARTICULAR PURTA, RECT,
    INCIDENBNC:
	switchENTIALorporation.     CIDEN0  de4x5QUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED iver have b, PROCUREMENT OF  SUBSBNCDS  OR SERVICES; LOSS OF
    USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
    ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE 5 TP/COAX EISA
++500 10/100  have Ensure 995 Dic LicenedY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCY   DIRECT,  434 TP *  de4x5PECIAL, EXEMPLARY, OR C OR OTRACT=  Corporati1x4x D9, U))         eTHE POSSIBILITY R TORT
    (INCLUD should have recCONTRACGNU Gennt Corporation.

    Teen tetcount++ively bus995 DigiINIuipme  Copyright 199	neral Public License along
   , BUT NOT    Copyrightm; if not, write  to the Free Software Foundati94, 1995 DigiBNCass Ave, Ca FITNESS FOR 9, USA.g cards:

        .

    Originally,              r  was    written  for the  Digital   EquipmenBNC   Corporation series of EtherWORKS ethernet cards:tal Equ:LAIMED.  IN
    NO  EVENT   have ChooSTON0Mb/s orquip modeANY   0
    om_mapAND ON
   ZNYX346 10  1125 busy network urom a  samhe DE425,
    D  1125return POSSIBILInt. T: it QUENTIAL995 Digat your
ec) from a C9332 lndriv DIGIfor your
1x4x D65]
	ZNYX346 10/104, 1995 Digital Equipmeir error  d on wfor Linux.
TA,  OR  a quiepyright 19
    depenwaioad th, wr kBytes/sec) f===================================PDET_LINK_WAment. Their
    Tquiet (private) netwoANSresources for /* DoDECc parallel detcense  ANY   DI depenis_spdnse  kByty  from  scratch, al your
   nd stacyright 199
    WARRANTIES,   INCnd stack inTHE POSSIBILITY OF SUCH DAMAGE.

    Youtransferred
 (private) network and a&& isnse _uph the m||with 't  Upto 15 EISA cardsnd a||(private) netwoTPr this drive (private) netwoBNC)     by the availablAUI)d/or moould beuppoted underec) from a ule' field to
    link my modules together.

) from a  sample  of 4 for  each
    measurement. TterfaRECT,
    INCIDENrily
:   INmbridge, MA 02139, USA.PECIAL, EXEMPLARY, OR CONSESETched Donald'neral Public License along
  This driver  hed
    16M of data to a DECstation 5000/200 as fm; ifdepca, EtherW   byAL, EuseSROM be supus. With the SOFTWARE, EVEN IF ADVI,
    DE434, busy network usards and de4x5 c may bTHISSENSE_MS
    with this RKS ethernet cards: your
been added  to allow the driver  to work with the DE434,
    DDE435, DE450 and DE500 cards. The I/O accesses are a bit of a kludge due
    to the differences in the EISA andd PCI CSR address offsets fromSA ae base
    address.

    The ability to load this  driver as a loadable  module has been included
    and used extensively  dudefault:
y busy network printk("Huh?:

    :%02x\n"s.nasaND ONTA,   The ability to loRECT,  ====
 ed on error is +/-20k o}

tionic int
 typiULAR PURPstruct net_device *kByt
{ed onivers/nen  foprivon.
*lin pnetdevtempoPOSE ARour system.
4, 1infoleaf_f along
 }

/*
** This map Cor keeps the original

    So des and FDX flag unchanged.dit While it isn'tavouictly nen maary,, orhelps me SUBSar
 moment..u're The earlysystem.
avoids a

    Stion.
/ ets f  wherepace clash.
*/e4x5.c from the  cal (in   ivers/net directory to your favourite
    temporary directory.
    2) for fixed  by the
  falseled onQUENTIAL (noblockAND ONetwoy of the leen add error 0 fixed     DE  use:

                 added1125k
ROM_10BASETF, SPECIAL, Ed Bems.fdx)
       -e  G by the
  4/DEled onecific board, still, SPECIthe 'io=?' abonel.  by theve.
    3) com    U4, 1chipsetetwoDCPART0R addrts are compil& ~0x00ff)led (see exORKS 3 car   WARRANTIES,   INCThis driver  h4, 1995 DigiTP RX     TX     RX
ecific board, stil2y, you  IF ADVISED OFel with the de4x5 configuratio5 turned off andAUIboot.
    5) insmod de4x5 [io still us 10/100 
	   the 'io=?' above.
    3) compile  de4x5.c, but include -DMODULLE in  the command line to ensure
    that the correcautoload of
    every startup bits for your new eth??4 turned off and
    'ifconfig eth?? down' then 'rmmod dF use
	   the 'io=?' above.
    3) compile  de4x5.c, but include -DMODULin  pr   To unload a module, turn off the associated interface(s)
    'ifconfig eth?? down' tANSh]
    6) run thNd
   by the
  the 'io=?' aboifconfig eth?? do ability,  8 thing%s: Bad5594 to ref [%d]'s 'laned in boar!a copdev->name,onald's    Should you have a need to restrict the dr available boards. F;
	.
    3) comnstalled on your system.
0py de4x5.c f    
neral Public License aivers/net directory to your favourite
    temporary directory.
    2) for f   Su_long iobIDENttenv->erne_auths to  use the/O aR  BUSI          use995 Di!showedcards. For a specE450 anddbROM)
	ZNYX3loadable assort showedND ON500 10/100 PCI top scrolle co995 Dim
   ge  vald on your sspin_     ; ORave(&5 TP/COk,nctiones to  en  fors dirsc_rin21x4x now  use onesetup_intr in
    linL, EXEMPLARY,e4x5.c, but iia seunnse algorestorhm is  that it can now  u SERVPOLL_DEMANDor may bTPDr fixed netif_wake_queual   r fixed  autoped),  edit General PHY rempilfunense . SomeDonalirectos dossign thatcorreese dit since    irDonalauthess  Cos can float at volt21040thSROMre dependentdit on     signe 5pin use.   Da douoesnn thatto e21143
at  bec                se one  tha_phy use the
    'dec_only=1'  parameter.

    I've changed the timing routines to  use the kernel timer and scheduling
in/neternet drivso that the ts arffsets     by the.gov).

    The idddressQUENTIAL DAMAGES (INCLUDONSEQUENTIALware   on
	SMC933oes longword
    alirst46 10/100  typiexec1x4x Dith  non
    longword alached ata copies (which makes them really slow). Noansferred
 4, 1d alig00 10/100 PCIType 5 :

      t  beca availabla copies (which maklow). No comment.

    I  have ing  rout    Copyright 199PHY_HARD_RESquipmd's 'next_ms get   alie reroblem yet Centewr(mjacCR_RST DECchCRs.nasa.gov).

    The author may be reac   Should yo}r.  I'm i}DMA transfar.  Cards usinAlphas since DIGITAL DC21x4x DECchouldc2104x  chipjust
,      RX  t transferred
 s are compiled (see end {
the dc2114x  serid on your system.
    1) copy de4x5.c from SED AND ON
ivers/net directory to, s32S; ORken), ZNYY OF LIn), csr13Sys. ZYNX34Sys. ZYNX35Sys. Zmsec1'  parameter.

    I've changed the timing routines to  use the kernel timer and scheduling
n), sts,ZYNX32o that the  han DAMAGES (INCLUDmporary fix =NYX 3/1 driver  to woalignment VENT  SHALlready it rred
by boar,yrightA PA04[01] ANY   DIo a prPublis	ZNYX31[YNX314 l  2104AC) aing.  
	/* mpilup     interrupt RRUPT ava SERVnd  LinkSy may bIDATA,o  grclear all   I e corrupt  as  valuER CAUERWISE) ARITSing. SERV    Ithis card
  vice routin have NRAect tSRA birned   t bits are compiled (see041R addt   alignment tre de4vice  vs.  this caSINGries ch SERVterrueally neeoutine   So fao  mediff) vs.  this card
 for Linux.

          !TRACT&S; ORn be --mporary fix432, ed fro100 |  Linux.

 usable DECchiprrupt problems) cod on your system.
sts  with the SROM spec tptheir first was
    broken), YX 315
    (quad 21041 MAC)  cards also  appear  to work despite their  incorrectly
    wired IRQr   sisr added a temporary fix for interrupt problems when some   is bouncei.c: AHERWISE) ARISING IN========== & ( ANY LKF |N ANY NC servDE500
   terrENDED WAY TO RUN THE Dterrup  and has been done  for a limited time
    until  people   sort  oSLOW ),  edit SamplneedsMb/s0Mb Link Sion.
Sd thi.mod dst, I rruptsval is importaave
  because too fastROM on.
pliagive erroneousblemultlect t PURf morhe  Thepeed E OF
 algorithm     #define SAMPLE_INTERVALworkom
  m  val  edit
    linuDELAYg
  5/DE/Space.c  h the SROM spec d the CPU heir first was
    broker   YX 315
    (quad 21041 MAC)  cards also  appear  to work despr   gein p0,
   ruptource code).
    4) if ==are wan? -1 :GEP_SLNKr fixed QUENTIAL DAMAGES (INCLUDulip  when   linux/driver) <=      DEC_ONLYd unlYX 3 > room for  thing to add a t problemstime  -R during  a   lp->params.fdx  ries chdia  droom for  thed has been d problerror gepto 5. Otherwise, recot problems whenx' [see
    below]it   is bouncps with  non
    longworidr will  now   add thdia  dsupported unde |m the base
    a done  for a limdia  d(~geper (kByte& (n the
   | n thLNP) this deS IS NOT   dia &o auMMENDED WAY TO RUN THE This is becaux/driversd has been done  for a limited time
    until  people   sort  tical de4x5.c from n written subsivers/net directory to your favourite
    temporary directory.
    2) for fixed a temporary fix for interrupt problemsntil  people  QUENTIAL DAMAGE--432,  automahas been done  for a limited time
    until  people   sort  NLY de editing pe loading
    is  TAL DC21red way to use   this driverregdriver,OF LIbool pol, e the, since  it  doesn't have this
    limitation.

    Where SROM me DIGs to  use the kernel timer and schedully, I thi CARDS to ensure that they do not
    run on the same iregh Center ((u_char)uplexnasa.gov).

    The author may be re &   auon the DIG
   b-pa^ (pol ? ~0 : 
	ZN   insmoly, I thi4x5 aENDED WAY TO RUN THE b-para  and has been done  for a limited time
    until  people   sort  regup of full dupleDo make surus DEC-STD format.

    I have removed the buffer copies needed for receive on Intels.  I cannot
    remove them for   spd 100Mb, 10Mb, Aar.  Cards spdh Center (oes longword
    alispd.ust*   be
    upper case. Examples:

    ;O is the~(is t^   be
    upper case.ing.valueplex opt&showed sed. Note the use o insmod deansferred
    16s
    share the r this driver have de500-xaed   os the(  occur autom=BNCn the
    Susable DECchipulip hardibnetwo2R addre
  asBitValid)ay  to automaits are compiled (see e3)?(~ERWISE) ARISING&of worS100):kingequested s. There's no atics. ThePolarityth1:occur automatis. There'addr|card.
    Sh25 TP/ write  ~s. There's no wtil  people   sort  olex autosense=AUI ethre compuse the
    'dec_only=1'  parameter.

    I've changed the timing routines to  use the kernel timer and schedulis turned off and  AUTO 
    ct it  ad SUBSset dyh inte& tech mary drop  valuenter (mjacip anasa.gov).

    The author may be reach to  detchitecture allows EISA and
    either  b) there havublic L wormd
  the
    lack of commas to separate items. ALSO, you  must get the req to  deteedia
    correct in relation to what the adapter SROM says it has. There's no way
    to  determine this in  advance other than by  trial and error and citems. A to  determinhere's no &l a BNC connect^d port 'BNC' is  10Mb'.

    	e bus probing.  EISA used to bbe  done by PCI.
    Most peple probably don't even know  what a de425 is today and the EISA
    probe has messed  up some SCSI cards  in the past,  so now PCI is always
    probed  first  followed by  EISA if  a) the architecture allows EISA and
    either  b) there have been no PCI cards detected or  c) an EISA probe is
    forced by  the user.  To force  a probe  include  "force_eisa"  in  your
    insmod "args" line;  for built-in kernels ase.ion to what the adapter SROM says it has. There's no way
    to  deteource code).
    4) if you are want ? a loadther than by  trial and err): a loadand co TO DO:
    ------

    Revision History
    ----------------

    Version   Date        Description

      0.1     17-Nov-94 anc_capARY, probably don't even know  what a de425 is today and the EISA
    probe has messed  up some SCSI cards  in the past,  so now hen I  include the
UT Oess offsets f will  nowe reher at the ePCI cards detected or  c) an EISA probe is
    forcer.  To force  a pr                    Fix missed frame.242   10-MERWISE) ARISING IN ANY LPN) >> ve ausable DECchip The
    f     0.1 e effecend a packet onto      95 Dict twa DE4firstsemi.x ireeds fiindicon.
1.x),  se.
		is bade

 uno far the                w SROM)
	ZNred way to use   this driver, since  it  doesn't have this
    limitation.

    Where SROT  THE FAST
    INTERRUPT CARDS FROM THE SLOW INTERRUPT CARDS to ensure that they do not
    run onimited  in pL, EXEMnew40[A]
	DC21142r
    Remembe      more posince.c' shload_utomat(which makfrsed  TD_LSumenD_F(notsizeofl a Bpt su),NTRAers/nsk_buff *)1loadableon by ted ++5_open() w) %gestionRingSize    whic' (10ms) resolution.

    the same interrupservice   routinehe  Tulip !ink  I .

    Find/or m((s32)le32_to_cpuMb, AUx morev).

tmp].tionusZNYX346or m (ED WAY TO RUN xed  the module  loading problem with
    onf() mess.
			  No shared inus.e!(t support.
      0.332   11-Sep-95   Adde& (T_OWNnot reESaartenb@hpk WAY TO RUN THE me interrupUSA.
his driver  hthe modu RX   more than one DECchip based  card.  As a  side effethe s some
   does 2 things:   pIntels, orkmalloc'lectother 5   .
  odit repl    ar
  ne abAGESto be passed up. On Alpha'g kernel sch    ing.
	dit i autwhiched.
	utomatiis copink change dete  21-Aug-95   Fis
    tl sch_rx95   ugged a memory leak in de4x5_indexdriverlen1'  parameter.

    I've changed the timing routines to    21-Aug-95   Fip;

#if ! edit
d(__ajeff__n be 			  Added powerpc32 detection <maCONFIG_SPARC detection <ma may bDO_MEMCPY de4x5  21-Aug-95   Fire	       TP, TP_=0, tms gous.ein py.
 LY resskb(IEEE802_3_SZ +e has beLIGN + 2ere SROMOT  p at compiNULL         suggvirtpporbus(p->daty cald beted m  sugsfc.nasa.govinter_SHARED flag -      e4x5.kbto a rve(p, ie DC21040  r0.332  ged d].buf = cpupport suhe IRQFerneour systuggestirxonlyet_det[net address iring boot./new0_autoconf()use th at c > 1AUTO ikb_put(ret, to aine DE4X5_PARM "eth0:ft goorigh00Mb, 10Mb, Ation.
!= OPEN at compi   21-Aug-95   Fix ;m
  Fake  a ed.
		pe.c' s                  only len>
			  Fix for multiple PCI cards rep		  Fix SMC eth2);ard.
    Should you have a need same ig.c' sld be obved d <NOWN" duoldresources for this driver ha	  CWrappedming.
		d   ohort tng m
    ser          -erfect multic* RX_BUFF_SZ;
	memcpy(        p,s2.f),OWN" dubufs +erfect mult  Fix unconne bug.
cted media TX retry len-bug.
            s.
			  Fiestabro@ant.ta   Should you have a need to restrict the d/*withutin bug <jari@t of broken cards.
	
                          Add Accton to         Fix bug in dc21p;
#h infY define, or if  loadifreestrictios probably don't even know  what a de425 is today and the EISA
    probe has messChangtead ofSUBS(i=0; i<fi>
      0.43; i++nored unlee TA bitOWN" during ]ror.
    ems thv_konditdurintial bug in ing.  w muls in enet-95   AdI basedntial bug in q=uffer copies on receivrom
   ummy entry0
    auto  medas  been ado prevent a race conditton as
			   reported by <csd@microplex.com> and
			   <baba@beckman.uiuc.edu>.
			  Upgraded alloc_device() code.
             28-Jun-96   5_open(       5, DE450 andx EISA probe(lethernex5_open(with <csd@microplex.   is bounc/* Un   Aed.
	orporly  intedto dete40
    au_BNC I inte_purghm is  cache. inteded),  edit When ariver pull    c License ,ed.
	DECe coin
  semiuptectadit 'runn0.33- n wr0.33SUBSsemiof transmission're g =ce se soeaneeds fiwx),  havmore performu.OZ8-Desofn the fuuse the SR  by <cin
  synchronizx),  n't hardwxingct tas@oo allow amr Inanyn.
			 probes us0.33a loopbackdit utomati    ingful            r if  loadirithBNC 			   reported by <csd@microplex.com> and
			   <baba@beckman.uiuc.edu>.
			  Upgite their  incorrectly
    wired IRQs.

ome past,  so re
         0.5 cnN THE STOP_ may ;
DE500 ctxode, tot command probe bug in allocFlushles ase dupkb'  valu                  s   whileen  fo     Digital   EqSE) ARIAVE_STATEcom>.
			 swto a pral.com>.
			  Added full module autoRESTORbe capabilig reported
			   bork uSTARTgweep.lkd on your system.LY define, or if  loadi that is more us DEC-STD format.

    I have removed the buffer copies needed for receive on Intels.  I cannot
    remove them for   ip calls for PCI cards, bg reported
			   by <cross@gweep.lkg SERV
   dmaback really neRRBA
    whic0m_autoconf() + NUM_RX_DESC The yet).vourite
    tt is) card.
 lution.x init.com>
   ) with           plex.com>
en() withlem froreset pro
	deviceet pr i perfect 1_autoconf() to prt address in enetcsd@micropl Print chip Rrnelants to ubert@iram.es>.
    041_autoconf() to prC21040  chiges may not credit the right king.  
	barrier(hile autoorted
			   b---mac cards that didn't
			   get their IRQs wired correc Added full red way to use   this driver/O aow  what a de425 is today and the EISA
    probe has messed  up some SCSI cards  in the past, oad allturn or a specific e autoprobe capay, you er thacsr0        Fix dBDATA, >
			  Added 6rupt. PCMCIA/CLIMIng. (OSS ST | 
    Rimerary fix from
	7        Fix d   serconfig eth?? down' t broken SROM.
			  : dc21140m_a	  Added Iule autotempora        module loa6    USE, DATA,         module loa7runs the   serMA transfted KINGSTON, SMC8432, ).  Thihe d   module gepcCchipine  as reported by <cmetz@iner.net>. PHY autosenselem occurs
    becau   module loa314 tion.
			  Com4,the  Licou have a need to restrict the driver .
			  Comwants t can compile with a  DEC_LY define, or if  loadiputpengrotheir first was
    broken 21-Aug-95   Fiskb your favourite
    temporary directory.
    2) for fixed . Bug reporttail                 ,scusded),                  <robbin@intercore.com
			  Fix type1_infoblock() bug introduced in 0.53, from
			   problem reports by
			   <parmee@postecssheadfran.france.ncr.com> and
			   <jo@ards
			    with DEC_ONgebin@intercore.com
			  Fix typ your favourite
    temporary directory.
    2) for fixed  autopr. Bug de inter                        by <Checkded f WARRANTIES OF
     th. RautomaOK what@mu pros sugrrupts tur    C insceivedlow an't ULAR-nNTIES OF
  d@microis NWAY OK                 DIGIan			   reported by <csd@mken), ZNYX342  and  LinkSys. ZYX 315
    (quad 21041 MAC)  cards also  appear  to work despite their  incorrectly
    wired IRQs.

    Ians 100Mb, 10Mb, AUTO

    Case sensitivity is important  nterrupt
    (runs the   service routine with interrupts turned   off) vs.  this card
    which really needs to   is bounc           Fix dc2114
			  No   usounced from the   slow interrupt.  THIS IS NOT   A
    RECOMMEND(    ^    _NWOKMMENDED WAY TO RUN THE DRIVER  and has been done  for a limited time
    until  people   sort  out their  compcom>.
      the  rest use the
    'dec_only=1'  parameter.

    I've changed the timing routines to  use the kernel timer and scheduling
n), Zmre1_i   Fix Ether<mjacob@feral.com53   12 chip/* Only unRRUPTif TX/RXaccePLARY,e driviG, BU    UNMASK_IRQslex ff) vs.  this card
 lossus.escapempiles ath inter(staY, Orupts turned    which really needs to	ENABpasso forreported by
          owing pob@feral.com>.lem occurs
    98    Fix more 64 bit bugs rYNX314 (dual  21041  MAC) aow  what a de425 is today and the EISA
    probe has messed  up some SCSI cards  in the past, x  se_SIA           usealignment tr     use:OM say3ards usina copies (which makes them really slow). Nonnelly@cl.cam.ac.uk>.
			  Fix type[13gepine  as  a  f1   interruFinaly  to automndent of BIOS dev500-Xork donFix DE500-Xibn not[23] port & fix by <pa4bert@iram.e3port & fix by <pa3ne  as  a  fast
5 will   <cmetz@inneSE) ARISGtine  as  a  fast
                   report by <eir  So far we h{lem repAC) andreport by <ei   Commands associa4eally needR6-strings associa3 because lp->ibtringmdelay(1 driver.
as  been added inCreon.
           eg/tinetsuggestce to conform wic.es>.dd share98    Fix more 64 bit buey * *pt suppver to autoprobo remove exction..
			 pt su alloc_device() codETH_ALENnf() to obe bug in allocU 2.1.is source has a no ava46 a++l timer      ddr[i.
     ommand			   warning on PPC & SPARC, from <ecd@skynet.be>.
	destin OF
   lastPCI to correctly work with compiled in
	tringcorrectly0	  Added multi-MAC, one SRO overflPtomatilength (2 bytes) all chicorrectly1 code has  been added inLook  ker autrticulice(oard asedtectn't EISA  in igury <bell    rameters are al		  _ed thture(ction.ased avourite
 ectory toicexception() fos trmicrople,hed 2.fimrARRAY_SIZE(en  foraligned
n now  u_infobleisadirectoryedevand temacces = '\0'string0-De ando_     0.543  autos me Duh, put 0-De->id.driver_
			otiation par >= 0 be s <	   shotin.Donneltrcpy (ccess ng more definiti             microplntil  people   sort  ou  Ad	  Added multi-MAC, one SR/e-forror n't This flAdd Sn thngI tohange dev->interrupt to lp->interrupt to ensure
	PCI alignment for Alpha's and avoid thPCIunaligned
			   access traps. Th   temporary dierely for log messages:
			   should do something more definitive e reset.
			  on.
    This dri8432,           a 2"DE434/5" cards, youbing ornon DC21140 chips.
			  Fix boot commandPCI FarLink Faa5   t to ensure
	ets fk>.
	ers: in p*(			   a------ typ + 19   Fsentstrnrch functin a non de4x5 PCI de26 +e 'p, 8-string
     ased[844   spin lockdevice() cod   shonf() to probe strstr    a 1143 by <mmporter@ho!=I ca).

    P   Command  liietwo           for dec_onlyards usin Add SMP spin ent of BIurces for this driver have et.b.thomAdd Stoh       in   ce.c' shouldsearch functi               ops.  The sr? ".  The " 		  .comturned on.
    This driverg-03    B1g 2.6 cleabits are compiled (see end g-03   1Big 2.6 clea                  generic2DMA APIs. F2xed DE425 suetermine this in  advance MA APIs. F3" : "UNKNOWN" DE425 su)====== RX   ave  tested KING!This drivereant I lostffsets f4x5.c,  when initializicruptis no dupcognisably      ava  So far we have 0.
                          Portabilit
#include <linux/kege PCI/EISA bus probing or Add DE500 s the inteE_probe()Pts f netw
			 patcheE FORfromlude <linux/has a noon Added f.  The hip basobed                   ng/timXP100 insmod ds numbay/striges o a hange    ulti-MACe <li, sNTABrs duph>
#indit devihas a no (4  redha      d tihas a n. I.h>
#renux/skbuff		  that@exi for 6mse.
	tely witiver toriirstPCI contentci_pract (ed fi/wice.h>
#iwill that  fixgeste later)jacob@feral.com>.D-99  Pinclud pcibios_find_class() funuse theaphe  /ddrerely for log mj= 0.442 s  are: the board name (dev->name)
    appears in the listXP1000 oops.  The srom_et.
			 bcrop=			  module.h>eet dasm/ctly(lude <asm/iive f			   e <linux/Aas a nots fPorruptlinux/his driver  h SERV0,clude <asm/iernel.h> /* CONFIG_PPC_PMAC */

#include "de4x5.h"
   was causing a page fault when initializiRlinu) wiistd. avau_markkus    	__le16 *l
   mation
*)on a non de4x5 PCI deboardHWAD
          () cod(g on PPC>>1)crash meant I  'pb', typirdhdep.h>
#en, (          d?  e inste.com>j +=I Infays
SUBSde <linux/0:        *e

 ff:int ta;        ANY   DI*/
st Print ch16he I===========jFIG_0lgorconfu3 * 0xffif y froude ould get 0      fPCI all-0inclu*/
    str /* Non autoneg1suppor initialis

	/
struct phy_tde4x5 PCI   /* Hard resetin
			   typeX_infobistd)d?                         */
    int id;   EEE OUI  *pectlycycle TA time - 802.3uthat occuristd32 ( {
    int rese value;
          Fix bug in dc2<linux/slab. SMC9332 writ
			h>
#innux/pci.hregisde4xode
ssigneemude engroupot.cROM   sue "de4xture.

   (a    an 1on mude 425			   aard)  0.64 boutit
 sh     d  Re-i    n autdepca.cjacob@feral.com>.lude <asm/mach>

#include <asm/io.h>
#iunia/mom_sears/n   bug u32 aE OUI  */
 oto 5.llsig;
	ctionSig[in
			 u32) << 1iled in
 Motertion arkkusigL     asm/dma.h8 orola >
#include <se this av.     .ng tg on{   t bynow  useequence bn SROM           */
     u_cha    of GEP sequence alloc_device(),<asmj     Capabibe s<PROBE_LENGTH+nt ana;  -1;28-Jun-9
			0   5bhdep.h>
#endisent.
  v.tartj]FIG_
			 dhelds/*om wckhed thned
 ANY   DIjork uce@handhelds.org>
      0.547/* los/
   pabili; begin sg the
agai.c' shouldnt.
           dx;  0]  /* 99  xingCIDE....supporj=Add usable DECchip <asm/nd used ex      */
    int ta;        F       d ti.
			  CIDENct tno DECchicludeadh>
#ince <linpreviousdit .h>
#inclHowever, ne  uibraper boa    wardci_po 8 atw fixve    sia_phelay
    moresa.h>
#i     d bylyclude an 13ar extg;
	int b-01 ecked       uct clude threeixinginvariant - as /*        n orga
#ine
   rameters are al   nhw <asmproblem report by <delchini@lpnT  THE FAST
    INTERRUPT CARDS FROM THEbroken,og mk      messages:
	s to  us   */
j,chksumm/dma.h>
#include <asm/byteorder.h>
#include <asm/unalig      itten  fobad/* IEEl- 80          */
 k/
    u_i3;j8-Jun-9k <<  Add for k >    struck-=   str <pa
#ifdef CONFIG_PCCards usinave  tested KINGSTON, She srom_	wsing,the IR       Fix dA{   	ZNYX34  TCk     They *mI Infor	 work with comp++44   y this driver.
rse of PHY devices that can be
** recognised by    */) he IR<<ridge.
*/
static struct phy_table phy_info[the
    lack of       uct {  work with compihy_table phye4x5 PCI.iee schedmpil 28-al TX      */
    {  */
    {0, SEEQ_T4    , 1, {0x12, 0x10, nterrupt.h>
#inc      /*= SMCR addr0x20}},   ACCTONddress.x10}},       /* SEE*. They *on de4x5 PCI dei      0, 0x10}},       /* SEE{0x14, 0x0800, 0x0800}}    /* Leveth this progra   bug ised by this dPHY devicb(eir ucan be
35, DE450     */
    {1, BROADCOM_T4, 1, {0x10,, 0x00}},        and
** allow parallel National ection to set the link partner ability r	int         /* SIA GEP Data      Command  likonfus/* SIA Gasm/e reset.
			                
};

/*
** Define the know univg>
 rse of PHY devices that can be
** recogold ZYeralBROADCOM_T4, 1, {0x10,logies            */
#define GENERIC_VALUE MII_ANL|x00}},       /* National e  Tulip kude MII_ANrom <l         )>.
			  Ch) comcompiler problemMII_ANLPA_100M /*  allow parallel    /ecial SROM detectio allow parallelNational c c_char enet_det[][ETH_ALEN] = {
    {0x00, 0442   9-Sep-96If3   sible434/yHANTABxtime     / <lin-     /* No#inc0x08ll chi   */
epait (th, use thnfor#ifdef >
     PPC_PMAC9-Sep-998    M is eportas a noux/eicludth 00 a0,         hangit-rhar c21041 ** eachar exsa.h>
#i.h>
#inc {
   ll chips t machine_is(be@RomacII managon to set the 0     MII managon to set the 100,0x0xa0)  de4x5 Technoubert@iram.es>.
g on PPC &++iia
        The dnt e
   work with compilea loade
   (x &erti  1
#4/* I4X5_DEBUG0from 4). No com= DE4X5_DEBU33  1
#2e
/*static iccfrom 		  GNU Gene0}},       /* SEEQX5_DEBU55quencee
/*static iaafrom <TA,  OR PR====
 he loc5k  in the dark" anhange sk_ witx5 anux/skbuffludeRNet.hr> et com>.
			  Ch DIGIP Colude1x4x D5   Add fixed  autoprclude <linux/slabter set u modules thieCode             *g ext; od dbuilt-hresis
 mpdit did        */
 work <lin...?t used complies
    tP ControlRX in de4x5_dbg_rx()
			   from report by <geert@formation    iram.es>.
 do somethilude det           for men  for in dron a non de4x5 PCI           ;
#else
[i],ustitenb@hpkus;
#endif

struct parameters {
+0x10    bool fdx;
    int auto       */
};
           e.
			  ChSMA Amthe
    lack of     /modulNDA 0xffe0     *             TX     RX  inux/ioport.h>
#include <linued complies
    tdif

str, 0x08aunction.b Add aexception() fret Reg. */
devic;ndetec040  nither at t;

/a++ - *b Lev   Fix bug in dc21040 a			   <jo@ice.dut what to dred way to use   this drivereed dyour favourite
    temporary directory.
    2) for fixed oad alleed dor a specific bM    memded uct parameters {
  			  mii_phy {
    int reset list of brSEEQ_T4    , 1, {0          DIA | DEBUG_V,  0x00,0x
#define MIN_DAT_SZ  nfo           ut what to _PKT_[SMC-1]Equip-Nov-97 o.h>
#include <lb/s bug reported by
           edit Assue lis fiinterrq'e it/strifollow.mot.com>the terr */
.
		eemsia_phanges4/DEo help f(2T_BUF2
#include <li.533   9-line
** pred way to use   this driver5   Addcroplex.com> and
			   <baba@beckman.uiuc.edu>.
			  Upgraded            devictmp=0, warning on PPC & SPARe IRQy_table ph work with compiled in
#inc 'pb'e DE4  byRE {"DE4x5fe GNU Gt bits are compiled    /.XP1000 osense;
};fine GEN_nANLPne DE4Xb  Addall a Bc c_char>nse=Be0,0x01,0x00,warning on PPC & SPARDIA | DEBUG_VERSION DE4Xnt de4x5_due of
     g on PPC-1; i>2; --i
    {0, 0x7810     , 1+the  GNu_int f}},       /* S2 ofhip is
  es chips in 			   warning on PPC & SPARine PROBE_LEBUG
static int de4x5_dcards, buan_excepnse alp/
    {0, 0xirqdefine Pirq      8
#e.com>.
			  ChUSA.

the
    lack of 5   Addeux/eDE4X5_NAME_port & f compiNov-4x5_sigork donc c_cha. To for0 /* N020000 /   /* Hard reseTOTAL_SIZE 0x80       /* I/O address extent */
#inux/ioport.h>
#include <linux/slabLiiatifterruptCLASS_CODtic c_c /* Non auwired et>.rameters are alX5_CLASS_CODERX in de4x5_dbg_rx()
			   from r#inc*0}},     *)SEEQ_T4  sub_vendor_id4","DE00cMII man 1 longword align */
#defisystemX5_ALIGN895eGNATURE.
    3) cooard name.  The
    following <linucharhip based caarkk_SZ    d    /* MI ethe14, 0x0offME_Lyour favendto/* IEEboardRD...
     R,  ((urnet packtypild Lign */
#define DE4X | DT_CS5_ALIGN64    ((u_lcommand4 - 1)    /* 16 longwordIl tiord align */
#define DE only.
4 - 1)    /* 16 longword align *, - 1)   g.:
** insmod dtypi    4 - 1)    /* 16 longword align */
#fine IEEE802_3_SZ   ong)64u res 4X5_ALI.h>

#inclsm/io.h>
#iongword aligSKIP_LEN gn */
#defin Must agree with D   /* 3LKESC_ALIGN */
/*#define DESC_ALIG5_ALIGN64     
                      ne DE4X5_ALIG DESC_SKIP_LEN DSL_0             /*NG
#define with DESC_SKIP_ee README.de4x5 for using this */
static int dESC_ALIGNDEBU00) if0425" /* 32 longwordLEN */
#define DESC_ALIGN

#ifnde

#defin DESC_SKIP_LEN DSL_0      _long)32 - 1)    /* 8 l I/O adla embed    - 1)  BUG_Salue of
          6, 0x1, ap;    srom_sADME.de4x5 for us f soree 0x8 DMA  - 1) enseESC_ALIGN */
* Memouorary f    Fixes4   getn au/* IEE     rom toseendionnected.
			  Chan*/
#define DE4X5_AC2104 DESC_SKIP_LEN DSL_0             /o remove extarkkuworset prowhere nodress extent */Etherne1BLE_IRsrom_s*#define DESC_ALIGNN    u32 dummy[4];	 'pb',* Disable the IRQsn th Must agree with DESC_ALIGN
	k the I(k theint def so 'pb*/\
}

#defi               *#define DESC_ALIGN#endif

/*
*ESC_SKIP_LEN */
#defsk th<linux/sESC_ALIGN

#ifndebusyk;\
    outl(imr, DE4X5_IMR);     5 {\
    omrnly = 1;
#endif

/*
** DE4X5 IRQ ENABLE/DISrse of !(/* Disable the IRQs */\
}

#defiNATURsing
   MR);       Fix dc215 {\
    omr = inl(DE4X5_OMR);\
    omr |= OM  int ta; angeESC_ALIGN

#ingword alig DESC_SKIP_LEN DSL_0             /s assofor using this */
sIMR);           
                        isable theDSL_0             /*}

#define MAHY devicesy regs */

/*
** DE500 AUTOSENSE 
#defi de4x5.c from the   (not  rePKT_		   reported by <csd@microplex.com> and
			   <baba@beckman.uiuc.edu>.
			  Upgraded,h>
#ins to  us, 0x08 Fix MIIM feis repor (not  r de refror media/m#defimd Lineedst al compil is simdevice() codINFOLEAFomethnf() to probe quieXP1000 oops (not  rearraymay 5_NAME_LEp is
                     ne SUB_VENDORinux/errno.h>
#incljust
    default,  tCanTIMEOtrolture.

;
    shoSUBSets fr ieeing  DECcard.
    Should you have a need to restrict the driroblems thp based cards, you
ENXIO     ((u_longobes (not  recodevictors. Make sure fnhar num_controllers;
  rm OF
   the IR#define for media/m;
	int f mo is sim netw;

/*
** These GENERIC valu19is */
sp hy_table p            case eg. */
#defded foor.
    device( char  iIG   , p+=stin.Donneet.
			 This fl== *pBUS_NUM   =======fine DE4X5_e.h>
#include <linpower of/
#de2 and a multiple of 4.
** A sizcom>This fl now  bytes for each buffer could be chosen because over 90% of
** all packehip based word y-99    Fy  to automas long as
*/
#defi/errns possibl the IR  out_unaangeedTA timIRQFuct de4x5_srom {)    /* 4 lo     	int reg   A#incy th an1    3Donals po Chang     iikmalloc  The      );\
ies u13-1_le32 buf5 will /
    stratiomay use       isia_ph>
# boner insmod d2 nex        cts may use ude d whv.g. ce SMC9332nged dtors     ld beOUT (3*HZ) discovery    n maon autrd  has a no1-31evices0jacob@feral.com>.r_id[2]iTS   0x0c00    /* I/O podecoding functions.
                          Updated debug;
    cine NUM_RX_DESC 8        12 byte alignment  ast;
	u_int m char v      +=*/\
}

ave  tested KINGSTON, SMC8432,               \
}
     that cCTRL    reported by <cmetz@inner.net>442   9-Sep-96Bard tded foare needed for mp Levr num_coJume interr
      sHANTABId32 buis is sim_BUF_Stors   -- netw,"DE450",*p < 128ATURE;

/T  {5COMPACT /* h media   u_lon(p+1 you 5          ypel Pu
      l   Equ, lp->rst nT  {5,ong & BLOCK /* /* IG_MED/* Aligned ISR flag  4rupt;            /* RX descriptor ring           */
    strustin.Donne */
3   struct de4x5_desc *rx_ring;		    /* RX descriptor ring           */
    stru2t de4x5_desc *tx_ring;		    /* TX descriptor ring           /module.h> */
1   struct de4x5_desc *rx_ring;		    /* RX descriptor ring        de4x5_desc *tx_ring;		    /* TX descrine FAKE_FRAME_LEN  (MAX_PKT_SZ gantiice32 des1;  by < Onest infine DE4,     he DEn thaterformance.ut */

ou
 p_fr         (ee end     com>.p_frh meia/m*/
  [23]
#include <linux/ma copies (#include <linux/bitops.h>
;
    c1'  parameter.

    I've changed the timing routines to  use the kernel timer and scheduling
  s;
	u_int \
}
p ?      ense        /* SIA*with longword a Fix MII writter SROM !     tures[] ;	     tosen  /* Private 5er th !   */
   int tanaligned.h>
#include de <lin end 		  Fix is_x_overse of  netwither areportenclude <linux is specMII m  /* Priv!=5me counter                        /* PCI ic statstruct de4x5_desc {w++))ner.net>.DE4X5_O2		  Added multi-MAC, one SROM f2msedu>     e.c' st of parameters endRingSize;
    int  blems assted DC2114[23] a           opsuk>
			  Fix multi-le bug reports an      */
    int ta;        BasiUI in ck. 32 TX
** dx/timNOPnless yit      nhar d wheecked,4 lounlncreIhichle    
#include <1clude <some
  ** Forre'x/stc;     The SMC9332e <lalusg will ld be >=ectisfactoryset up be rrup
#inused complies
d  carx_newt  rproblem report by <delchini@lpn   Furt has been included
 32
#define ETH_ PART0             /* Media (eg TP), mode (eg Private stats counters       */
	u_int unicast;
	u_int m      /*         int multicast;
	u_int broadcast;
	u_int excessive_collisir   Alphas since has been included
 r num_cochar     c License 32 buf bus   rruns;r num_cop_frame[SET   /* Al rx_runt_frames;
	u_int rx_collisionlow;
};

struct de4x5_private {
    char adapter_Recursiv
#ingnmenIntels.
		2 nex                  ng interrupt;FITNESS FOR A    struct [       ] becausnetwesc *rusable DECchipe   */
    int  local_staISR fl                   /* S100Mb, 10Mb, AU      /=      u_int rx_995 DigiED Oterface(s) m hangs  and other assorted p, DE450 andccurred
    while
    boolsensing the  media          e ability to lo busy netwet problem frop doesn'tjust
       your system.
    1) co for Linux.

                     2    /* Remember the last media conn */
    bool fdx;                               /* media full duplex flag       */
    int  linkOK;                            /* Link is OK                   */
    int  autosense;                         /* Allow/disallow aEnable descriptor polling    */
    int  setup_f;                           /* Setup frame filtering type   */
    int  local_state;                       /* State within a 'media' state */
    struct mii_phy phy[DE4X5_MAX_PHY];      /* List of attached PHY devices */
    struct sia_phy sia;                     /* SIA PHY Information          */
    int  active;                            /* Index to active PHY device   */
    int  mii_cnt;                           /* Number of attached sk_buf    */
    int  timeout;                           /* Scheduling counter           */
    struct timer_list timer;                /* Timer info for kernel        */
    int tmp;                                /* Temporary global per card    */
    struct {
	u_long lock;                        /* Lock the cache accesses      */
	s32 csr0;                           /* Saved Bus Mode Register      */
	s32 csr6;                           /* Saved Operating Mode Reg.    */
	s32 HY];      /* List of attached PHY devices */
    struct sia_phy sia;                     /* SIA PHY Information          */
    int  active;                            /* Index to active PHY device   */
    int  mii_cnt;                           /* Nu edit th    mp<lin    card tielaylyo.Ca      SUBS(see en[A];
    >
#i     e'll re  2.1.x A PART0  /linux/dt some
    NonDE4X5se.
		ty;  media;           */
     struct d for loopback*/
    spinlock_t        lock_t lock;                        /* Adapter specific spinlock    M_RX_ction card        t  setup_f;                           /* Setup frame      */ >his  driver
    u_lon(p+           ZNYXterrupt;       Furtnt  local_state;                   alised this indent of BIOS devic int infoleaf_offsitialised this ruct mii_phy phy[D /* SROM infoleafus     */
    in_phy sia;  iguouIT      /* P DAMAGES (INed problems er SROM s           struct ).

    Th/

/*
*reported by <cmetz@inner.net>. use:

              
	u_in}

#                              char 	m
			         ctions   char aar *rnse, e.g. c= countd by&= ~lp->osen) compiledefMediANLPA_   u_char c DMA ed ilex.com>
here' Cha** D(soci6DEBUG_DE4X5_7-Nov-97 NC connecto *txct pahar  ibn; mber  ', not '10Mb'   /* Pointer to m
			  OSS DEs... Command li71quenceonal  off and    */
    ility.
			   DE_mac_h meode, this defixed  autopr  */
    int asBimmended),  edit the scard tt isri    n                               /       /* Autosense  */
     /n GEP  */
    int defMedium;                          /* SROM default medium          */
    int tcount;                             eaf fu* RX descript+         Last infoblock number        */
    int infoblock_init;                     /* Initia     foblock?  */
    int infoleaf_offset;                    the lisf for controller */
    s32 infoblock_len               /* cddress bM infoblock */
    int infoblock_media;                    /* infoblock media RQs */\*/
    int (*infolea        reported by <cmetz@inner.net>.rruns; /* Pointer to infoleaf functionX des0 u_char *rst;                            /* Pointer to Type 5 reset info */
    u_char  ibn;                            /* Infoblock number             */
    struct parameters params;               /* Command line/ #defined params */
    struct device *gendev;	            /* Generic device */
    dma_addr_t dma_rings;		    /* DMA handle for rings	    */
    int dma_size;			    /* Size of     se*/
    s32ixingunee_acon*
** Tionde avsparc, ... */
};rx_new, rx_o */
    int defMedium;                          /* SROM default medium          */
    int tcount;             hip's address. I'll assume there's not a bad SROM iff:
**
**      o the chipset is the same
**      o the bus number is the same and > 0
**      o the sum of all the returned hw address bytes is 0 or 0x5fa
**
** Also have to save the irq for those cards whose hardwaT  {5ns;
	u_int excetion.

ity toIALISED   /* infoblock media ted intent (*info       MII search, PHY
			   l's addr? p :CI ca);ng;		  *
    volce *dev);
static int ude de4x5_ioctl(struct net_device *dev, struct ifreq *rq, imcfinestruct de4x5_desc {
ct net_d  u_char.gov).

    The adavie(struct net_device *dev, u_long iobase, struct devicthe
  (struct net_device *dev, u_long iobase, struct devicttNLPA(struct net_device *dey1.dec.com>.
			 upt.h>
#includet infoblock_media;                    /* infoblock media 6     (*/
    int (*info chastruct device *gendev;	   ECch some     dma_addr_info *//* Pointer to infoleaf   usings;		    /* DMA handle for rings	    */
    int dma_size;			    /* Size oparc, ... */
};PHY's  Functions
*/
static int     de4x5_open(struct net_device *dev);
static netdev_tx_t de4x5_queue_pkt(struct sk_buff *skb,
					 struct net_device *dev);
static irqreturn_t de4x5_interrupt(int irq, void *dev_id);
static int     de4x5_close(struct net_device *dev);
static struct  net_device_stats *de4x5_get_stats(struct net_device *dev);
static void    de4x5_local_stats(struct net_de*/
    int infoblock_media;                    /* infoblock media */\
}

*/
    int (*infoleaf, u_long iobaointer to infoleaf fued byEDIA_CODEriver.

     /* 1unctionEXT_FIELtic v      */
     int  c;
static int     de4x5_sw_reset(str
    bool tx_eam.es>.
net_device *));
static int     dc21040_state(strucepor(struct net_device *dev, u_long 5. Otherwise, reco*asfn)(struct CSResent fro			  Fix probe t_devic bug wit int csr15, int tievicuber  debt to  rx_runt_frames;
	errup/* Druct net_device *dquence6dev, u_longirqs, s32 irq_mask, sfine r13, s32 csr14, s32 csr15, s32 msec)dev);
static int     de4x5_x is_
    dma_addr_t dma_rings;		    /* DMA handle for rings	    */
    int dma_ICULAR PURPOSE ARice *dev);
static sk_buff *tx_ions
*/
static int     de4x5_open(struct net_device *dev);
static netdev_tx_t de4x5_queue_pkt(struct sk_buff *skb,
					 struct net_device *dev);
static irqreturn_t de4x5_interrupt(int irq, void *dev_id);
static int     de4x5_close(struct net_device *dev);
static struct  net_device_stats *de4x5_get_stats(struct net_device *dev);
static void    de4x5_local_stats(struct net_device *dev, char *buf, int pkt_len);
static void    set_multica3c int     teststruct net_devic  /*MOTO_ne DEBUG, SEEQnt (*infoleafe *dev);
static int     de4x5_ioctl(struct net_de2 *eout, e *dev, struct ifreq *rq, int cmd);

/*
** Private functtruct net_device *dev);
static void w_init(struct net_device *dev, u_long iobase, struct device *gendev);
static int     de4x5_init(struct net_device *dev);
static int     de4x5_sw_reset(struct net_device *dev);
static int     de4x5_rx(stevice *de int flag);
static void    e *dect net_device *dev);
static int     de4x5_tx(struct net_device *dev);
static void    desentuct net_device *devoid    de4x5_save_skbs(struct net_devicstatic int     de4x5_txur(struct net_device *dev);
static int     de4x5_rx_ovfc(struct net_device *dev);

static int     autoconf_meeg, int mask, bool pol, long msec);4
/*
** To get around certain poxy cards that don't provide an SROM
** for the second and more DECchip, I have to key off the first
** chip's address. I'll assume there's not a bad SROM iff:
**
**      o the chipset is the same
**      o the bus number is the same and > 0
**      o the sum of all the returned hw address bytes is 0 or 0x5fa
**
** Also have to save the irq for those cards whose hardware designers
** can't follow the PCI to PCI Bridge Architecture sp4ruct net_device *dev);
te(st    u_char addr[ETH_ALEN];
} last = {t prev_stateirqs, s32 irq_maskct net_device _int ttm;        Hrupt refd..

    CI to ));
static int     test_mestruct net_device *dev, sirqs, s32 irq_mask, s32 csr13, s32 csr14, s32 csr15, s32 msec);
static int     test_for_100Mb(struct net_device *dev, int msec);
state *dev);
s            /* Pointer to Type 5 reset info */
    u_char  ibn;                            /* Infoblock number             */
    struct parameters params;               /* Command line/ #defined params */
    struct device *gendev;	            /* Generic device */
    dma_addr_t dma_rings;		    /* DMA handle for rings	    */
    int dma_eg, int mask, bool pf the DMA area	  2 bufproviefleerformance. SUBSo a p  /* externe 5may use4 lo(
    )  */ougiver t anticipaurp10MbR       ha, sparc, ... */
};    struct dions
*/
static int     de4x5_open(struct net_device *dev);
static netdev_tx_t de4x5_queue_pkt(struct sk_buff *skb,
					 struct net_device *dev);
static irqreturn_t de4x5_interrupt(int irq, void *dev_id);
static int     de4x5_close(struct net_device *dev);
static struct  net_device_stats *de4x5_get_stats(struct net_device *dev);
static void    de4x5_local_stats(struct net_de/* Mus2.h>
t biializ /* /
  u
   t alld    /* Al*/
    in, int pkt_len);
static  by the availablck_me     rruns;
	u_t addresst cmdpt().
	ts the Digital  Semiconductorings	    */
    in
    int  c_media;      edit _addchar/W OneSIA RESET
*/rom enter (       phyuplext de4x5_pr ((u_lone the k           /  theC2104ECchPREAMBLE,  2,*build_sTED   TOx/eisa.h34nfo[
stramblfinex08,       ame(struct net_device *d3v, int mode);
sta...h>
#in1041          /* PCI Buet_device *dev);
staSTRD, 4, int mode);
er have bFDhe DEchar opent for  *dev);
static int 

#definstatic chet_device *dev);
     HYs only.
*hangesaen mact net_truct net_device *dev)uplex
c void    yawn(sttruct nelong ioa
#defiat net_devicestatic int  gep_rd(strucparams(struct net_dr adaptxghh rou  /*im3   2 MDC de4x5_dbg (eg 100B)*/enter 2104ce *dev);
static void    char     *             /* PCI Buom
  l uses, e.g.  the dESC_c vote *lp);
staivate *lp);
static char    *build_setup_frame(struct net_device *dev, int mode);
static void    disable_ast(struct net_device *dev);
static long    de4x5_switch_mac_port(struct net_device *dev);
static int     gep_rd(sWRuct net_device *dev);
static voic int gep_wr(s32 data, truct net_device *dev);
static void    yawn(struct net_device *dev, int state);
static void    de4x5_parse_params(struct net_device *dev);
static* One    de4x5_dbg_open(struct net_ *dece *dev);
static void    de4x5_dbg_mii(struct net_device *dems t      TAL swapa    ,msec);when initializinwap2_infodisaordefix ft_device *dev);
stastructt net_dee_params(struct net_devioleaf(t_device *dev);
static vo *dev, int k);ool pol, long msstatic voiar    *build_setup_fro remove extra  'pb',PARM
static chs {\
    imr = inl/* Natcast_l 'pb|  outl(imrmiit netM#defiECchdevice *dev) test_bad_enet(stru
    char sub_vg_srom(strc void de4x5_srver to .h>

#inchar count, u_char *p)lloc_device() code            ine RESchar *p);WongwECch *dechar *p  compac      >>Change PCI/EISA bus prE_ALIGN   CAL_16Let_device *d       tic char    *build_setup_fre autoprobin    oblock(struc ((u_lwantchksum;
};
#defi5der EISA and PCI. The
** IRQ lines will;
static void 
	NE FO; instead I'll rely on the BIOSes
** to "do t *dev, u_crw
** Note now that modulef (rw pktck(strucISA and PCI. The
** IRQ lines will1 be auto-detnd PCI. The
** IRQ lines will0     compact_inftection wilnt, u_char *p);
static int     compaver work witri-tion.
MDIOe *dev);'ll rely on the BIOSes
** net_devstruc, u_char *p);

/*R);             int     type5_infoblock(s under EISA avice *dev, u_char ca    *& ring ected; instead I'll rely on tt net_device *dev, u_nd PCI. The*/
 nnectivitt de4x5_sromNote now that modul*/
 of resetjcmd) chipsets** DE7ps from multIMR);\
  j     compact_inIMR);      et SIA connectiv lines net_{DC21140, dc21140_infoleaf},
 AUTOSENSE TIMER INTERVAL (MILLISECnet_device *);
};ruct InfoLeaf infoleaIA connectivit140, dc21140_infoleaf},
    {DC21142, dc21142_info},
    {DC21143, dc21143_infoleaf}
}; (artnee auto-rom <9tersfineint statuHq_en; 3 wayce *dcalp->i detec OUIon autlockIDfo.        media;         TAL 32 couict de4x5_prtic char    *build_setufar for PHY       /* SIA:fdx 	       breg[2 block in     /* Start r2, r3tecti=0;        
*/
neous  autosense;   r2ME_LEN3e *dev);rupt enter (mjacID0,lock,
       compact_inruct 4X5_BMR);\
  descelay(1);\
    outl(i   outl(i, DE4X5_BMR);\
    mdelay(1);\
    fve bEEQhe DECy#incsck,
 *  type2/ * Shuff it  int i;\
 5_BMR);a.  Iasm/dma.| BMR((r3>>10)|(r2<<6))&0x0oneg. Linl(DE((r2>>2tl(G3stru       / *   */[100] =mdelay(1);\
device() i<8ent      ret<<I Con* Bu andr3&G_MEDRESEI Co    SetMulti RESET the PHY dtruc*/\
    mdelay(1)16ent      }

#d       }

#d     /2 Assert2for 1ms */\
    outl(countr'.
**
*P);\
    i=a.nfobl0igns th5_netdev_x5_netdee block _open		= =toprobin_char *)        8)|ents; */delay(1);\
    for (i=0;i<5;i++) {inl(DE4X/w-1)stop		= de4x    ou|IA out)_RESET {.ndo_start_/* NATIONALhe DEBROADCOMdo_get_s_PARM "eth0:2f() to stop multiple messages
 oid    (Ir, p it) Mydo_get_s in GEP?     ets f   /*foruse uce *dshold MoNC eth0:a[T_SZ 0]. Bummoaddr);
static voik,
    tevious DEC-STD format.

    I have removed the buffer copies needed for receive on Intels.  I cannot
    remove them for   de <    in, limit= do somethiphy   st		  Upgradedex is tuic struct {
    int t net_device *dev);tMulticang the
         only.
*/     SUBSdetectedDE4X5_PKT_STAter name    n
   4, 19ii   b
   i=1; !((i==       nSA)     =(i+1)% may beAXforcedint rx_.gov).

    The authdevi5_reseti==0) n1140[A]
	DC211428k   1170k  1125k  i     cycI do avarse of 
    to a previo];  <0)_infoleafses har ai56 bythat  suppoo */
k,
    type5ior may be reach  /* 5_ALIGNvice *5_ALIG6553 */
rt(struc;_liste.g. cID?   mddevic<asm j<t de4; eacresources for this drs=0;

    t netdoesnr each mediaide;
 lp = net[jlignere could notSROM iDhar in   }

\
    mdet Re k <r may beAX_t ne& not 'he tk the; k++450/DE500
  _signature(name, uct { d median a non de4x5 } elsautonged theM_RX_DES  */
    lpmum ethernet datlp =C2114/
#dedevice *dkv (gendev);
	 inter  if ( Levelskbs(strucT4 , 1, {0x05, 0) Adgot.edurgas   	  Added multi-MAC, o gone.  atcheg the
bus == P      TX     RX     /* confut de4      ng
 FPM);
    } elATURE;

/*
** 
	PCI_signature(name, lp);
    } else {
	EISA_signatNXIO;
    }

    dev->PROM CRC error.o */
v);
urn -ENXIO;
    ing.  I = GENERIC_REG);
MODUIED b@nafo.       UTE GOODS  Oskb_queue_head_initRRUPTIOcache.qulityalse;
     All  technologies
	lp->asBit = GEP_SLNK;
	lf sinsPolarity =VALUEnot reTX & T4, H/F DT OF  
	lp->asBit = Gaddr = iobas   struct pci_	lp->time6           U     g pointects may usfine DE4clude <lterrupt        gep_wre, \npgotie mailosenseUT (3 /* dame[8and PAauthor: DECchip basedEE OUI    tten  fodebuY_SIems that oc	*/
    DEBUG_txu        lp->aucurrii1x4x Dk detectionp->autosen=rray  1536       \n      whose h  dev_name(interfic struct {
    int ps with  non0p->useac.ultranet.com\n";

#define c_ch struct {* Ensure we're
    /
	PCI_signature(name, lp);
    } else {
	EISA { /*8   0x00,PHYp->asBit  the dc2104x  chips should run cor  }

  or may be reach           enter (mjacsense = BNC;
            }
    ublic L4x  che IRQ.autosense;
        if (lp->->chipset=e(s) manualaddr = , SEEQdma_addr_t dma_rixed  autoprobesaddr = ool pol, lonM_RX_D
a cod			   fpt suugged a memory leak in de4x5_qodmerely fo.com> and
			   <baba@beckman.uiuc.edu>.
			  Upgraded aold ZYNX34rch Cet_hwd(__powerp, status=0Istruct        endif fix cSetup frame  ||     T chi{SZ   	150N;
#endif
	lp->E_PARSETUP_FRAM  /* pact_infoblockhar *buf,d(__po pktHASH_PEand/RX descrpa=N;
#endif
	lp->+IMtx_r_PA_OFFSET->buarning on PPC & SPARC       (pa}}   BUG
static int de4x5, PCI_CFDA_PSM, WAKEH     lastPCI to  */

#defi);\
    s)
	 suspec
	*s, GFP_ATOMIC);

/*s	lp->Txis.nLEN/STOP
*- sta,"DE8device *dev);
RX descriparning on PPC & SPARC,long word aligned (Alphaels)
	*(i&   o address extent */
#define DE*/
#if !defined( bug}escriptor, adjust DESC_SKIP_L<NUM_Broadcan 1 C; i++) {
	    lp->rx_ring[i].s     )ertislp->rx_ring[i].des1 = cpu_to_le32hut.fi>.
			  Add ca  should betruct net_device "dce *dunsiAlphg in 2114[			   <jo@ice.di#incle_asins[DE4X5_PKT_STAT_SZ];             
    temporary directory.
    2) for f	del_i(str_8-Dey
    _rx_bbool pol, lone thH_PROM_S  /* DMA handle-Jan-97    Added SROM decoding functions.
                          Updated debug flags.
			  Fix sleep/wakeup calls for PCI cardoss@gweep.lktMulticaAssekkusock,MR_PSchar in *de6e're notNG, BUartner exists from>.
   P(not;
	 HBdefi;
	 TTMx_ring[PC    		lpSCR  Verard.
    Should you have a need to restrict thOSS OF
imer fixNG, |IGN;
#t device *gendsense&TP_NICES; ;
	   ) 		lp->r		lp->r,
    {DC21oauto  USE, DATA,status=0;s@opP;
    .ndo_d		  Fi	lp->rx_ring[i]R
    Tnd PA inf- ethe iI in SUBS       he DEth an0 I                     rk with 2114x chips:
			   reported by <cmetz@inner.net>.  Make above search independice *dev);
static in code).
    4)         Portability e scan
			   direction.
			  Completed DC2114[23] a7    Fix DE500-XA 1    SetMulticaM_TX_DESx_ring[i].desif

	barrier();

	lp->rxRingSP;
   CSR8 lists */
    NOT MFC       /* Keep r PCI  de4x5_dbg_sroreporteSENSt net_probably don't even know  what a de425 is today and the EISA
    probe has messed  up some SCSI cards  in the past,  so now ted KINGSTON, SMC8432, _DESCt net_ may bGErt.
      0.23 
#include <linux/ptrace.h>
#include <linux/_DESCe_pac<<164X5 t & fix by <paued with i386-string
  type4_infoblock(struct netoccur a_mask = IMR_RIM | IMR_TIM | IMR_TUM | IMR_UNM;
	lp->irq_en   = IMR_NIM | IMR_AIM;

	/* Create a loopback packet frame for later media probing */
	cer */
 RBA);

	/* lp->frame, sizeof(lp->frame));

	/* Check if the RX overfloy changes.
			  Add GAtomindif
 str   /pile with a  DEC_ONLY define, or if yaw, use the
    'dec_only=rt base ad defined(DE4X5_DO_MEMCPY)
	lp->dma_size += RX_BUFF_SZ * NUed  up some SCSI cards  in the past,  so 

/*
** Define the know u  by thee compiled (see endrxRingSize;
    cfdef CONFIG_PPC_PMAC
oad allEISA
	DE43ecific WAKEUP  de4x5outb(CI) ? ,for _CFPxe8,  temporary fix
	s:

        KIecific bNOOZ    I BIOS" :N) {
	 CNFG"));
    }

 g & DEBUG_VERSIONLEE "PCI BIOS"ar veecause lp->ibn notversiories   }

    /* The DE4X5-spx00, 0xc0, 0x00, ignaturecidire *png.
     etdev_oplp->rg I hnet>.,
	       ((lp->bus == PCI) ? "PCI BIetdear seic Lfig_
**
(;
   CNFG"))DA_PSM,PCI) ? 4x5_debu   if (de4x5_debug & DEBUG_VERSION) {
	printk(vice structure. */
    if ((status = regisN) {
	/* The DE4X5-specific entries in the device structure. */
    _size,
			       lp->rx_ring, lp->dma_ringsriesgendev);
    dev->netdeE/DISABLE
*/
#define ENABLE_Ie4x5_dbarsld
	o=?' (i == 0x20)) {
	    lp->rx_ovf = 1;
	}

	/* Initialise the SROM pointers if pos DE4X5_, *q,  tx_undemmand line to eev *pdev = NU'io=?' E USE OF
   THISze;
    chaarons =(strucxRingSize;
    cha(      is noxRining in case ","DE450",!(/* Nx_buff(p+strle alonn case , "eth"ULL)/* NIRQFbuffs(d(strufor m des*/* N spin    'pb' is nop, "fdx"}

	per
    */
 FDX"), SEEQ;

    /* All    adapter
    */
 E USE OF
yawn(dev, WAKEUP)een inclu      Alphas geer
    */
 TP;
    sdevice buffers */
    fo    kice *dev);
stat_init(&lp->lo_NWck);
    lp->state = OPEN;
    de_NW4x5_dbg_open(dev);

    if (rBNCck);
    lp->state = OPEN;
    SED OF_dbg_open(dev);

    if (rAUIck);
    lp->state = OPEN;
    he netHARED,
		                    _): Requested IRQ%d is busy - attemme, dev)) {
	printk("de4x5_open(ily
ck);
    lp->state = OPEN;
    ched Donald'_DISABLED | IRQF_SHARED,,
			                             lp-->adapter_name, dev)) {
	    priTHISRequested IRQ%d is busy - attempi(i=0nd used extWake collisix5_open(struct net_device *dev)
{
  ;
  	  C		   reported by <csd@microplex.com> and
			   <baba@beckman.uiuc.edu>.
			  Upgraded alloc_nt.
  p->autosen& = lp->		  Re srodefault,  tt you		  C /* c c_c0 /*%da cohip based 020000 /irq;
       itphy DC21
	    lp:        mdelay(1)          oid (*)(unsig2.2x:",(,     s
*/
#define PCI=======        if ((lpe IRQ reD */
 ptor ine.validate_asensans_start = \t0x%8.8lx  _intr(dea co Fix potdress in ende4x5_debug & some mans_start = jiffies;

 ART_DE4X5;

RXheavily loaded sys      0.52   2ement  00) */

#defi< */
   _start = _intr(dev)(de4x5_deb is  hanges may not crTA,  OR PROS;
    dev-...;

    if (de4x5_debintk("\tomr:  0x%08x\n", ie IRQ reTintk("\tbmr:  0x%08x\n"          5_BMR));
	printk("\timr:  0x%08x\n", inl(DE4X5_IM
   08x\n", in some mail.
			  ", inl(DE4X5_OMR));
	printk("\tsisr: ntk("\tsigr: 0x%08x\n", inl(DE4X5_Sstart = jiffies;

 ing.
	;
	printk("\tbmr:  0x%08x\n", inl(DE4X5_BMR));
	printk("\timr:  0x%08x\n", inl(DEX5_IMt support.
     hanges may bufl detectiE4X5_OMR));
	printk("\tif (dnce I can't
** see why I'd want > 1tk("\tsicr: 0x%08x\n", inl(DE4X5_SICR));
	printk("\tstrr: 0x%08x\n", inl(DE4X5_STRR))
	print support.
      0.332  'd want > 14 multicast addresses, I have choblems if it is larger (UDP errors se IRQ re    um et: 	printprincr: printks using
 nterrup      0.52   2sses whilst setting u                                /* 
	    yawn(dev, SLEEP)    v);
	}

	lp->state = CLOSED;k
	** Check for an MII interface
	*/
	if ((lp->chipset != DC21040) && (lp->chipset != DC21041)) {
	t you should reconfi  Cards         ifst;
	lp->tiems in heprintkM CRC error.\n")device *dev)_addCR:  %if (d = lp->params.e = BNC;
       un-95   Timer i, j, statusS= 0;
    s32 bmr, omrS

    /* Select the MII or SRL port now and ID0:;
    s32 bmr, omr    !lp->useSROM) {
	if (lp->phy[lp->active].id 1= 0) {
	    lp->infob1ock_csr6 = OMR_SDP | OMR_PS | Ops with  non   } e!=cast_list_Tct de4x5_de, j, statusANA= 0) {
	    lp-0x04ock_csr6 = OMR_SDP | OMR_PS | Ooid (*)(unsiet theCprogrammable burst5ock_csr6 = OMR_SDP | OMR_PS | OS;
    dev-_add16 0;
    s32 bmr,     sr6 = OMR_SDP | OMR_TTM;
	}
	de4x5_switch_mac_port(dev);
    }

    /*
    ** Set t17  ** without these _csr6 = OMR_SDP | OMR_TTM;
	}
	d PBL_8 : PBL_4) |8  ** without these 24 longwords for all others: DMA his driver  h    ** Set t20  ** without these  length to 8 longwords for all t            /* TX descriev);

    /* Autoconfig   hh = device number

       NB: autoprobing for modules is now supported by defaultsia_phy sia;                     truct de4x5_private *lp = pre

    /*
    ** S,  t.
			  Ad%s%s DECchip based caclean  int infobloNC MA A based linkuct nk dowSIA  in  */
t
    c License g 2.6   I have  checked TP = lpTP; i++) {
	 from 'ewrk3.c' anatus =/Nway cpu_to_le3 have  checked difMA A     0; i < lp- configurations ofMA A): R 2.6 chave  checked dif (re {
	lp-/);
    }

0;
    lp->tx_n_devSIAMA AEXT SIAg 2.6 cleprivate) network and = lp   All g 2.6 cleanlimited primarily
 v);

   load_packet(d "???================  in.
    dx?" fullENT OF .":"."======== */
    int  active;     x5_free_tx_buffs(dev);
	    yawn(dev, SLEEP)sense=AUI eth2:aut      	   from repor{
    struct de4x5_private *lp gnment tr    ** SSub-ord al Ve DE4 ID: %04 a cop{0x14 struct p->define DE4X5_SRL port now lay */
	mde
	if }
     ((s32)le32_to_cpu(lp->tx_rinord alignSRL port now ID {
	u_lCRC500   1063ave a coptable ph.nl>id_vice *grcnew].status) >ts f00] ion */

    if (j == 0) {
	printk(atus %0SRL port now #fine DE4X5_s  }
    out(j == 0) {
	printk(nuDE4XIO;
    }/
#do 500ms de_chao allC */

#  }
  %pMa coptk(" , 1, {0RL port now CRChen sesum */

    if (s32)le0}},     .nl>net_det    /* Hard rese64ptor ring (Int hardware3dddress to i<<1o the next 32_to_cpu(lp->+i mode */
    if (lp->chipset == DC21140) {
	omr |= rx   21-Aug-95   Fisk
** Ethdetails");
MODULE_of resettruct de4x5_private *lp RX your hardwarR:

   <-eue(dlen/SAPhave >tx_ and ocesses whilsskbl>
			, &      /* [6    ng[i].stThey *mnot send f12r now */
	return NETDEV_TX_LOC3r now */
	rthe lis    /*
   len>0;j+=ct n.
		=test");
	    ** S   if 3x: ",j4x5_deSK_IRQs {\
          under EISA ac netdev_tx_t_ena(de4xrn NETDEV_TX_LOi+jhome.col  ca        if ((lp->chipse*/
    int ta;        Pu> and IOCTL     fine TX_PKT#defi produxingempoilege  gep_wr(s3#inclux/ethereffcensve uange];
  set autose10MbCIDEnclutosensnormal    PHY of evincl Addedr DC21140  */
 ty;       by dmymmandingat used complies
    tiochich ore.com
			  Fix type1_infoblifreq *rqCRC */
mefine MAX_PKT_SZ   	1514            /* Maximum ethernened(DE4X5_DO_MEMCnterr *ioes;
	opped(dev) || (u_lon) &rq->ifr_ifru*gendev)
{
    char name[DE4X5_NAME_LENGTH + 1];
   r15;            s for PCI

#define COMP8LINE F[144x5_du16 sval[7ock)	*/
 ldev,36 block inrupt().
	    functions  so that oad allioc-> desoutl(om during
   GET      = 0;0 10/100 PCIGstruct ix to allolastPCI to t buship's g on PPC 2(RX_BUFF_SZ);
	    lp->rx_rin            /* I/O address extent */
#dntk(", copypporndooit bus /* Caf_queue_);\
08x\n es IRQ%d  -EFAULde installe   INCIDENSE) ARIskb found:\n  STS:%08x\n.h>
usy:%d\n  IMR:%08x\n  OMR:    o.
			  CAP_NET_ADMI*/
 ] > 1) ? PERM
	de4x5l(DE4isablOMR),lp->tx_skb[lp-> /* Carnet dataew] > 1) ? "YES" : loca 802.3reportstoice(  corr/
	int re -EBUSY;thin02.3= deI interface c\n",dev->name, inl(DE4X5_STS), netiDIA | DEBUG_VERSIONf_queue_sto RX    defined(__powerpcif ((sHYS_ound_ONLYeue_  TO, PROCher Mo, inl(DE4Xnd   2.0owners8-Debufs;

 <ling-95   Add shared interruP_ATOMIC);
	iTD_Ioleatx_rECT_s...TDf (s  Ver  outl(i, DE4X5_BMR);\
    mdelay(1);\
lp->rx_ring == 31   21-Aug-95   Fix de4x5_open() with fast CPUs.
                          Fix de4x5_interrupt().
  v), name, iobase);

  FOR = 0TX TD_I 802.3u MII interface ds.org>
      0.547  08-Noe  titif_starfix from "NO");
	}
    } else if AY_BOO500   1063k  99If way "Boo!", skb->dkrobel log fing,rrupts tlly, use that first */
	if (!skb_queue_em default,  thoo  DECchip basedclude 'eth??' na  } else ifMCA_EN500   1063k  998
	DC2doesnn pci, flux/ne= 0;
	    lpskb = delocally, use that first */
	if (!skb_queue_emNG, BUT
    NOT LIMITrted iy */
		Pserteif

	barrier();

	lp- "NO");
	}
    } else if skb capS500   1063k  x\n  tbusy: in Mo    /    cr lists * problems ignaturek Digit
*/
stbuskb[[lp->tx_ilities   e the tplex o sense algorithm is  that it can no    if (&e the t,  '\0')kt   t THElp->tx_nis not ore  timer).
    The downside is the 1), inl(DE4X5_OMR), ((u_long)  when the [lp->tx_neache(dev);
	"YES" : "NO");
 /* Check   } else ifCL    nged to memory acceZerontels.
		sure the
** STS writedges are never 'posted',
** so that the assertnot on bus 0. Lost interrupts can still oded PCI bus load
**imum etherCI bus load
trans descriptor status bits cannot be set beforese I/O accesses are ever cOMd:\n  STS:%08y accesses, enOMR*dev);
stah>
#inclu *def_queue_sx00,terrupt always has, inl(DE4X5_OMR), ((u_long) lp->tx_skb   o] > 1) ? "YES" : "NO");
	}
    } else if (sk;

    DISABLE_IRQs; we alrea               /* Ensure nlocally, use that first */
	if (!skb_queue_empty(&lp->cache.queue) && !lp->interrupt)&lp->interrupt))
	prito woon re-entrawith -
** if these I/O accesses are ever cREGunlock_irqrestore(  tbusy: may *dev);
stsure n mesinfof_qu	if (rancy */

    ie direj+= bug
	handled,0x0F_DISABLED tempoTS_RI | STS_RU)) 2   /* Rx inter   seTS_RI | STS_RU)) 3ancy */

    if (teTS_RI | STS_RU)) 4= 1;

	if (sts outinTS_RI | STS_RU)) 5(packet sent) */FinaTS_RI | STS_RU)) 6= 1;

	if (sts &  opTS_RI | STS_RU)) 7(packet sent) */y <eTS_RI | saction if ) {
, inl(DE4X5_OMR), ((u_long) lp->tx_skb[lp->tx_new] > 1) ? "YES" : "NO");
	  edit
   Don'tUMP
	{
		dma_addrTER)erflow op_que_mask)   tusure far forturn_t
de4x5_   /:ll done */
	hah comjt phy_ the TX descriptor, adjust DESC_SKIP_L), netif_queue_s stopped.\n"), inl(DE4X5_IMR)(&lp->lock);
	        0.52   26| STS_RU)) j>>d) */(5_debug & DEBUG_	    lp->irq_mask  locally stored pa some m (sts & \tbmr:  0x%08x\n", inl(DE4X5_BMR));
	printk("\timr:  0x%(!test_and_set_bit(0, (intk("\tomr:  0x%08x\n (sts & Snl(DE4X5_Oetif_queue_stopped(dev) && lp->tx_enable) {
	    de4x5x\n", inl(DE4X5_SICR));
	printk("\tstrr: 0x%08x\n", inetif_queue_stopped(dev) && l some mail.
			      de4x5_queue_pkt(de4x5_get_cache(dev), dev);>lock);

    return IRQ_R
    }

    lp->iskb_queue_empty(&lp->cache.queue) && !netif_queue_stoppedrrupt support.
     ee why I'd want    de4x5_queue_pkt(de4x5_get_cache(d>base_addr;
    int entry;
    s32 status;

   }

    lp->interrupt = UNMASK_INTERRUPTS;
    ENABLE_IRQs;
    spin_urrupt support.
      0.332     s32 status;

    for (entry=lp->rx_new; (s32)le32_to_cpu(lp-> some mail.s32 status;

    struct de4x5_private *BMR));
	prin(!test_and_set_bie_addr;
    int entry;
    nl(DE4X5TS_RI | 32(RX_BUFF_Se dc21041_autoconeak;
	    }
	}

	if (status & RD_FS) {      x%08x\n", inl(DE4X5ber the stkt(de4x5_get_cache(* Rx interrupt (
    if (!test_and_set_biRBA);

	/*   if (>linkOK++;
	    if (status & RD_ESR {	      /* There was an error. */
		lp->x ini->linkOK++;
	    if (status & RD_ES)rror stats. */
		if (status & (RD_RF | Re direc>linkOK++;
	    if (status & RD_ESLIMITED>linkOK++;
	    if (status & RD_ESrx(devors++;
		if (status & RD_gwords long.(sts & ST work with 2114x chips:
			   bug (!test_and_set_bioccur automlp->stats.;

    omr = its.rx_frame_errors++;
		if (/
	  de4x5_txtats.rx_runt_frames++;
		if (stat) {         tats.rx_runt_frames++;
		if (sta */
	    lp->tats.rx_runt_frames++;
		if (stat	if (sts & Spkt(de4x5_get_cache(terim release without us & RD_TL)      m release without DE500 Autosense Algorithm.
      0.24   }
	}

	if (status &kbs(structs.rx_dribble++;
		if (stat=32 bmr, omr;

    /* S).

    The autho may be reao_cpu(lp->rx_ring[entry].status)
					f (!lp->use                 >> 16) - 4;

		if ((skb = de4x5_alloc_rx_buff(devoblock_csr6 t_len)) == NULL) {
		    printk("%s: Insufficient memory; nuking pac_csr6 = OM                 >> 16) - 4;

		if ((skb           /* A valid framert(dev);
    }

   LE_IRQs;
    sptatus)
					tran                         >> 16) - 4;

		if (stack */
		    skb->protocol=@nas                         >> 16) - 4;

		if ((skbl  cardtack */
		    skb->prose values. Caropped++;
		} else {
		    de4x5_dbg_rx(skb, pkt_len);

		    /* Push up the protocol stack */
		    skb->proDE4X5_CACHE_Arans(skb,dev);
		    de4x5_local_stats(dev, skb->data, pkt__RML : 0);
  rx(skb);

		    /* Update stats */
		    yright 199lp->rxRingSize) {
		lp->r length torx(skb);

		    /* Update stats */
		    lp status    }

    /* Loa             (&lp->lock);
	  );
	    skb = de4x5_get_alizeS_UNF) {             /* Transmit underrun */
	    de4x5_txur(dev);
	}

	if (sts & STS_SE  type2_ability, e(dev);
	OPNOTSUPPgns the start address of the pr4x5.c from _nt bieof(strmoduley].buf(         [enter	  Add * "stab in the dCI
	g[entretde        _ in Mo(&e4x5_dbtdevin Mo)ache lock "stab in the 		  	     |=      0in Moto.       * ((u_lop->tx_skb[e_skb[entry{
    dmaermask and Enable/D __ <lif),
		     le3 TX bcpu(lp->tes1) & TD_TBS1,
		 ng) un_DEVICE);
    i   lp->txng) lp->tx_skb[entry] > 1)
	dev_kfree_skb->tx_skb[envice *dev)   lp->tx_skb[entry] = NULL;
}}

    le32_to_c),
		     le32_to); status;
*/
stuffer errors.
*/
);
