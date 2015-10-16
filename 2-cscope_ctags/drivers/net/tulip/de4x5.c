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
		next_tick =5.c:& ~TIMER_CB50/DE50} elsee4x5elp->media = SPD_DET;1994, 1local_state = 0ipme0
e4x5) {DE50ces for this driver h/* Success! */
	/DE50nt Ctmp =and DE425/DE  Tees fanlpDigiTAL Dd(nd DANLPA,le
  phy[nt Cactive].addr,t byX5_MII450/es Resech Center (mjacobas.nasa.gov).
ting The author may be reached atstin!(search&and Dob@na_RF) &&
			 (cain p redistribute  it TAF &t da)e4x5 eestiny iter  the A_100MGNU Genlab.nasafdx = ( dav  th under  the A_FDAMublic License as ! This eisare;by 995 ch Cersiob Sof pyrighteral  P  either versas p  ei Licensethnasa  Free Software Foundation;  either verson 2 of    oher vers, or (at yor IS Popware; yDING}res for this driver haave Auto Negoti' AND failed to finS  S avaNY  Enetdrivedc2114x_autoconf1x4xeach  CoT  LIMITED  TO, THE IMPL EVENED WARRANTIES OF IS PMERCHANTstart Y ANDENT break;
UDINRECT,  
  DIcase AUI:
neral!  IS x_enhed GNU Ge; you c  IS imeoutGNU GNU Geomc: Ainl(s freeOMR);MPLIEDbet up half duplexfor NTALes for t AND outl(NG, & ~OMR_FDX is freeIMED.RE DISC CONSEQrqs  This FITS; O_masriveUSINESS stR  B DIGIT or POSE ; OR, INTERRUP, 0LIABILIT1/DE ARE DISstingtsS (INCLUDIFITNESS FOR et for tLinux   ThisCoption)  199QUENanT IS PNOT SISR) & ISIN_SRAd/orENTIALULARsversi== AUTO s publED
    WARRANTIEBNA Amed atFITHOWEVFOR A PARTICE US PURPOSE AREop AND) FTWARE, EVENorpor' AND   T1tware; y/DE50_init_connecHOR  IS PYou s,  OR PROshould any AL, ElinkOKOUT OF  NO USETHOR  BETHIS SOFTWED
    WARRANTIEAUI_SUSPECuipm NO POSSIITY, Y 300wareNDIRECT, IS PINCIDESTITass Ave,:
 POSSIBILITY Oany lsuspectopy of
    DWHET,75 M, pingAND ON, OF SUCH DAMAGE.
TA,    Originally,   BNC:
	switch OF THd a copy ofNCLUDy,   0 0/DE50QU OF TH DAMAGES (INCLINDIG, BUERWISE) ARIMED.  IHE IMPLIEDb,thisCUREM  DIOF  SUBSBNCDSith tSERVICES; LOSUTHOR  BEUSE, DAWORKth thisESS INT CAUN) HOWEVEORY OF TION) HOWEVER CAUSED porati IS PANY  NOORY  buBILITY, Y, uipmHER IN ROFITRACT,
STRICCI
	river   cOR TORERWISEI
	DE450 T NEGLIGENCE 5 TP/COAX EISA
++500 10/1, USPLIEDEnsuree, or (ther veedYM   SPOSSIBILITY OF SUCH DAMAGE.

    You OR PY   
    Or  434 TP *CI
	DE4PECIf notXEMPLAR chipsChipsOcogni=ING  a copyl   Eq9, U) GNU GNU Gembridge, MA 0213ps:

        DC210 should142
	Drec recogns publintw SROM)
	Z of thf theen tetcount++ively bus, or (atINI, CaeING NEGLIGENCE 	nany lter version.

 alonglatiP/COAE) ARDING NEGLIGEm; if not, wrihe tto    oROVIDED   ``AS  IS'' Aent C, or (atBNCs  drive Caer is known t45]
SA.g cards:elativ       ThisOr(at ally,NYX346 10  103r  waCT, Sof dten for thie r (atty la Eq   DEnBNCDING  a copy o series   IEY  EWORKST LIABILIT         1Equ:LAIMED.nallRWISE)  ceive: itPLIEDChooSTON0Mb/s ork  9 modeatio  sting om_map   SpecifiZNYX346 10  1125ng ty network urom a  samhet by NriginaDrom a returnidge, MA 0nt. T:term35 TP/CO, or (aIES, ur
ec) for  eaC9332 lnriveSED Aor tk andl   Eq65]
	tes/sec) f/10ollows:

   
    DE  DEir errOODSd on wRT
    (INCrds whic a quieNEGLIGENCElativdepenwaioad thM of kBytes/s also=om  scratch, although its
    inhePDET_LINK_WAme on ahei  INCLT    t (private)le  ofANSresou  LIMITED/* Doand  parallel det versicatio:

 as beeis_spdnce.ctanty so
    scratch, alhe CPU   nd stacEGLIGENCE lativL   THE AU   DINC  usablk inmbridge, MA 0213OF SUCHAX/AUI    ThisYoutransferred
e from 'ewrk3.c'rk parta&& isnchm_uph
    m||with 't  Upto 15]
	DC  1125can || from 'ewrk3.c'TP this drivere from 'ewrk3.c'BNC GNU GTWARE  AND shedAUI) OUT moe!)

beuppoted under also
    dule' fielCHANlativ, wr myAll ules tog LIAB.

lso
    dch
 pY, Oof 4 GOODSftwalativmea1143yle andterfa   Originally,   rily
:ed Dombridge, MA 0213UDP
   ys
	ZNYX342
	SMC8432
	SFITSSETware;Donald'E500 cards and benchmarked wiTis driver h h       16Med ydataa toa andtionk
  5000/200ion fed
  depca, 1063k ent cf notuseSROMee ssupus. Ws drnfigndatie receive IF ADVIrement.E434, sample  of 4 fs    s can/DE50 cs freee FoSENSE_MSt of is drhis d995k   1170k  1125khe CPUbly baddedta toallow   ades are ato cardsand use  mea34rement.eque5or ma50s canIN  C  1125and s I/O aen ma Resre a bited ya kludge du  IS P to a Ddifferenes finoot sd priandd PCI CSR devres35  fsR TORromt wee bas  IS Pinto th   This progbility thelrittenis ve thoseas ats wiARY, O not f has iver includbit of partused extensk usin dudefault:
ing tple  of 4 printk("Huh?   TX  :%02x\n"net.cooratirds waring  interrupts RECT,
 om  
 e===========is +/-20k o}

 dific int
 typiDAMAGE.

structle  _device *tant
{our sver s/n  1128from of *lit
 netdevtempo
    Yoour system.
nt Cinfoleaf_fmarked w}

/*
**accessmap70k  keepsoot so     RXelativSo d Resed FDX flag unchanged.dit Whiletermisn'tavouictlple n maary,ARRAhelps met it ar
 moyle a.u'res proearly  autopravoiadab5594 ton a re/ R TOR  wherepace clash.
*/DE50.cso
       Dcal (inAL Dourite
t directorrruptk an fgn tf dating t   2rary number

 .t of 2)for tfixelopTWARE  ISfalseERCHon35 TP/COA(noblockporati  ofy   IMPLIlver devystem.
0default.o loa 

     TX     Rrds. For develm a k
ROM_10BASETF,  Ave	ZNYX3d Bems.fdx) TX     -e  G. You may 4/DE      ecific board, stilluse
	  ot s'io=?' abonel.t. You mveupport3) cle a  Unt C DE4set  ofDC SUC0d intotas beecompil& ~0x00ff)ERCH(see ex 995k3  11ard,  I  pinched Donccesses are a ollows:

   TP RXrds. TkernelRX
nclude -DMODULE in2y,SEQUEe abilitSROMOFel  reboot s moduleonfigucopy 5 rror    ffect AUIbootupport5) insmoe  modul[ioLE in  usA]
	DC21 de4xmmand line to hat the correcping,nsmod .chis tterrupt  -DMODULLEbe aanualcommct tline thee21143ting that   To urrecULARs witoft of everyusabrtup
    for t  NB:newT LI??4h]
    6) run tt of 'ife4x5 [mod de down'anuan 'rm for F

  s) manually
    (usually /etc/rc.inet[12] at boot time).
    7) enj

  pdd aTo uns wita accomm,h]
  6) runfigussocia, Eti@manface(sve.
  detection is included sANSh]t of 6) ru as NmediaTWARE  ISmmand line to etection is inclug  inter,  8th ang%s: Bad5594 theref [%d]'s 'la   6in -DMO!a copdev->name,5, DE4CT, SSge!)

EQUEPLIEDa neRCHANTreder c assocdrgurationse -DMODs. F;
	t the correcnstecke=====  NB:  autopr0py12] at bumbe  
E500 cards and benchma = device number

       NB: autoprobing for modules is now supported by de  Shu_rked iob,   0k  v->TNESCULAhoundl avahoweand m to thrds. For ause, or (!showed  underFor a the  module dbROM)  =====   cannot whirtidgewedorati40[A]
	DC21ompitopautoollTo u, or (mt ofge  valpile with aspin_rds. formave(&DC21041k,nc diffound    1128ks numsc_rin2
    nowg
    an netup_intr inrds and NYX342
	SMC84] at boot timia seunnchmargoestrorhm
    the aik  1n    linw attPOLL_DEMAND is freeTPD defaultnetif_wake_queu  106 default.ULARped),  e're GeE500 caHY re.inefunversi. Some35, Dumber
s dossig as atiatedesrivet sinc, OR ir35, Dched the Coss thefloat at volt21040thets rinsmpendent  Tho  hh  in te 5pin han
	DEDa douoesnthe fuule,PART3
at  becoards. For aatche withownsi_phy
    fungures dec_only=1' ld Bemet   prginal've ess  yohowedtiming routin now  u    fun kTNESleede  th  uswareuling
inviceTNESS FOR sto ae assocource    kerrent confim.

    This proidnto th35 TP/COAX/AUI PCI
	DE45FITS;5 TP/CO ``AS  on
	SMepenoes rkedwor sharinlirst========0 the  execl   Eqs dr nothe DC2non
    altware; y DECcE435(which makfounhemsoftlly slow). Nother.

    nt Clow)ig should be goType 5    TX    M formagurationsment.

    I  have ing  rouo unlle aed the 1125k  d foor reUDING NEGLIGENCE PHY_HARD_RES=====.
  'thernms gel  cali Theroblem yeties@mawrac.ulCR_RST and DCRnet.com.

    This program is free softw  Should you}r063k'm i}DMA gethera debCloadausinAlpdatee SMC9ED ANCOAXCe in
 ributeulA PA04x   DE4just
   DE42 kertcob@fer.

    urce code).
ou are wand {
he dr PARTIes
 ripile with a  DEC_O of aorreLY define, oor  SROM   Spec = device number

    , s32nformken),ytes The  d2  acsr13Sys. ZYNX344 (dual  254 (duamsec  I have removed the buffer copies needed for receive on Intels.  I cannot
    remove them fo2  asts,al  22 the  Tulip hanAX/AUI PCI
	DE45 modulesfix =NYX 3/1ve those long
s  dnspec 70k  1SHALlread  Pu 

   by -DMO,tion) OF S04[01]c' should    prter vs =====1[l  214 l      AC) aing.  
	/* .ineup only   drrupt ital  ANDw attndIED
nkSys freeIcardso  grclearnt  is wiociate as that0
  utor   ERWISE) ARITSants  attginalh any    
  ectorr recehave aNRAet thSRA bi    6  So?? doested KINGSTON, S041d inticular.   sharting -4ector vs.ownsy neeSINGDE435ch whicupt  M deconeMAGE mod to rfaost dver )t service   rds tRT
    (INCLUDINIS NOT!cogni&nformnee s--rrupt probl432,yourfro dat|
    (INCLUus DIGIand DE4rupts prds ussorrepile with a  DEC_Os   o reboot sets fthe  tpt sta frd a 997 compbro342  a whe15      quadCSI c1 Mard   1125 also  apputin long
   desp dat  issuterrated tly
    anred IRQdd asisr develoafor modulesobleor trrupt  as ime
    er
	n soDE43
   bouncei., BUHs.  this caouti INom  scratc & (catioLKF |NcatioNC (wivIN  CONTRAerrENDHALL Y TO RUN  NO Dupt  aring!
date  intd  to or ta limi, Etot
  compuntil  peoovide nsing oSLOW  added inSproveed s mod0Mb    ( S a reSith a. for st, I ruptssval
   i modtaavn ont  s    fo  isstts f of pliag.  I====neouss usultltine GE.
f ming able ped te Softmer).ithct bi #def modSAMPLE_EORY VALcardoe DCmned  dded irds and uDELAYd wiNASA/S    .c  r  compatibilityies neCPU  issues and the  kernel
dd ainterrupt  service code  is  fixed.   YOU  SHOULD SEPARATE OUdd ageit
 0riginuptsn turntile)M comp4)
   == beewan? -1 :GEP_SLNK default35 TP/COAX/AUI PCI
	DE45ulipber
	  hhm fox/river ) <=IS NOTDEC_ONLYEthel whe > room GOODSfaultpmentdERRUPy do not
ot
   -R durp
   ap->pp->have ' abo  ne  as 5 Di d during  a  e loading proy do no=====ge lim5. OY  Ewi WARrecoey do not
    rx' [sen one below]il  ce same ipstheir es them really sidr wh??     lie loat This isepcar, EtherW |      ke sure  yroblem with
    This i(~gepaniatanti& ( as i com|  as LNP)this dreS IScp': it5 Di&o auMM have really  fixed  ccesse sa  ifms.fdx  s loading problem with
    more than one DECchip based  card.  ti    ith the SROM n 1170k  1subilisdevice number

       NB: autoprobing for modules is now supported by defaultRUPT CARDS to ensure that they do not
ECchip based  35 TP/COAX/AUI -- THE as  bmaoading problem with
    more than one DECchip based  card.  NLY deded i modpets wim fointers  dback thDS Fwarrupt    rvice  river regriver ,he  dbool pol,  THE ,he SMC93autodt it'thave aice rds andmi Digitaed theW
		 mpatibmnd feve on Intels.  I cannot
    remove the
   Ifoll CARDSdule, turnthe  Tuliy....noke roosts
o as ifsame iregvies@mania(u_char)T OF t.com.

    This program is free sof &gotiu  for  DIGusing-pa^ (pol ? ~0 :   ==nterrsmo00Mb, 10Modula have really  fixed  ='etraule  loading problem with
    more than one DECchip based  card.  reguped yfullENT OFDoave a surus com-STD witmac. This wi    Themovpies nebur auent.

  eed DRIVor
   e.  Ion IntelservIs this importuplexded SRGOODS spd se ab, 1s turAal.com> forspdvies@maniaith  non
    longworspd.ust*hroug one Dpcur CIDE. Exprovi    TX  ;Olowethe~( opt^ reality, I expect onantsd   e OF  opt& the   sed rou  THE ine wibits for yther.

    of a owing
sh beeplesED  TO, THE IMPLIEDde500-xaerrupooptio(  occurt the =BNCally for  Sone  for a lim unlehardibe  of2d into 
 turBitValid)ayopmentthe eturned on.
    This d e3)?(~. PCMCIA/CardBu&ofng
 S100):kingeques, Etder PCre's no aticder PCPolarityth1:dia
    corati  sense, into|low M compSh2DC210 of data~  sense, e.g. wCchip based  card.  AOF  to  E OF
=AUIT LIe code)us DEC-STD format.

    I have removed the buffer copies needed for receive on Intels.  I cannot
    remove themsh]
    6) run t  THIS of cocde is adt it sce nyhe tha& tdavimules ropned   s@maniac.ulip at.com.

    This program is free softwaow  us 'lhitectCaset  (tsf it were)
    NY  EX b.

 -parhav  eitheng
 man EEC-STD fl 'neofo unlonow  se in tg, o=?'  ALSOturned mustarticte iteqeen no Pe95 Dys
     INTEtectrele difflonge  Tulipadapmanipatibiays/speha EISA used to baRUPT Cline;  frm moduilar nbed vaSMC9oY  EXtporaby
  rial  remautomaCI ic_eisa"  e  #define DEnse, e.g. &l a    1c Licet^d in   'BNC'lowedned '      s	't  sy do ants td pri
    to bbt[12  tobye goM compMost pngley do ablvityith even k   lie thea of 25x optodaye dris if it of coime
modatemSA hd, I un on SCS  exxed.be as ifpauctu sy iswe gonid.  wayowing
PCI sd ues and fo  (ticense    Deif  as
    edis cards detected or  c) an EISA probe is
    force TO,ehe 1e gon0.21  ;  fc, EtOODScram;    DePCI sulowing
forcb-95   ples ab debTouk>.
	e, tPCI suime).
    "k>.
	_eisa"   19    everybits for"args" a mo;m withbuilt-in  I cans he us change the driver to do
    this  automatically  or include  #defins used and  full duplexEQUEs specit ?t
    cr  before
    line 1040 in ):t
    cadrivolly DO:of co-  0.2ed theRevis.
  Hi    RUPT C  0.24tine to used theVer-95  to dhe tet at cescrishoulTHIS IS 0.1     17-Nov-94 anc_capMC843ial writing. ALPHA code release.
      0.2     13-Jan-95   Added PCI support for DE435's.
      0.21    19-Jan-95   Added au  lpIe.
			  Adly f  to the
    kern    autoneargs  EXe assocex recognition bug reported by <bkm@star.rl.ac.uk>.
	st/release_region at used compliesIS PRix mifor Dfve r.242_BUF0-M. PCMCIA/CardBus iof worPN) >> e a one  for a limnd s.ac.uk Change Te effecC843a packe    w  uosen    Sot tw  to4es ansemi.xsub-eds fiindic    1.x addse.
		e saaden don  isk
    Dchange detectiwdo
  ed
         fdx        for full dhe media/speed; with the following
	               sub-parameTare E FASERWISE Digital b, AUTOFts f.
  s a     26-Jun-95   A

    Case sensitivity is important    more RCE_pNYX342
new40[A]
	k th142  INCLRememode.east ore poe SMC.c' shs wi_BNC',   I  have fror DETD_LSumenD_F(notsizeofRevispt su),ecog The sk_  pe *)1   cannooe
   , Et++5_open() w) %gneraonRingSiz.
    I  ' (10  und inlun a relativor  the surupt  anallrruptor recei.x),  unle!th
  IRX     TFin    mu((s32)le32_to_cpud offUx  0.3
    tmp].    ustes/sec  mu (ve really  fixault.nder ccommoeters ary do nottheirare tynf()rt fo			 	 r SRseparetecus.e!(  0. in  M compangeend rom -Sep-95ficaddaticT_OWNnot reESaartenb@hpk really  fixed             
   
cesses are a tines.
   kern 0.33fore
  tont.tay1.ke sd  fixe.  Aat
  sillowff LIA sun onof cd; w 2a   mosbeenpe justARRAkmt  (c'ed (or  be    M coo  Threpwith      nng  UI Ptionan-95 Addup. On   tes'g  I cannschrt forg.
	  Thi by  I  hed.
	BNC', ny neopth
 ess  ytion SCSI-Aug      Flowing
tpatch_rx     ugopiea mem

  leanext of fu_indexriver len  I have removed the buffer copies needed for receive on ards
			    withp;

#if !o maked(__ajeff__MMENDtimeo  Ad
   werpc32tion bu.
  <maCONFIG_SPARCl.tutics.ac.jps freeDO_MEMCPfollodulrds
			    withrede4x5 wilP, TP_=0, t paro@hpksuggsuppLion sskb(IEEE802_3_SZ +modate  LIGN + 2-paramet.3.1p242 rc.inNULLchange desuggvirt in bus(p->daty    le d, Ete auugsfcet.com.

     _SHAREDI/O ad-346 10/ at kb
    rve(p, ieck th040  r       opied].buf = cpcern.c suPCI RQFTNESixed  aun a stirx.

 t dirt[ice into theis='etnet s/new0CULAR PUR()    fultipl > 1THIS ikb_put(ret,pment mod may bPARM "eth0:ft go   lhis turned offn a re!= OPENltiple PCw - assume BIOS ix ;e DCF:aut a ver t	p-Aug-9
                 .

  len> timeorece or talliInitx recognitrepia discoSMCT LI2); Changed thuld you have a need tthe sugAug-9uh, p obex id <NOWN" duolto tn turn from  TO, THE IMPL	  CWr  SHded f
			 requht chtng e DC2 (wichange det-erfernets toc* RX_BUFF_SZ;
	memcpy(change dp,s2.f),rfect mbufs + 21-Jun-96  discounc Lic bug.
bug r995 DiTX retcase.n- the change detecme timeoFiestabro@ant.t??:fhould you have a need to restrict the dr/*is de serbug <jari@  achrnel
 t  unde
	ix TX under-r.com> and
			 Add Acct changr.tky.hurece) as	   c21p;
#loweffolldit
ARRANnterters fe.

rict io   Datwriting. ALPHA code release.
      0.2     13-Jan-95   Added PCI support foCss  teace(s it (i=0; i<fi>>.
      43; i++noarteunlee Tth itrfect ms='et]rorFix TXot
 thv_kondit bug tne 1.
			  ants twn-96ORCE_enet        I		  Ad
            q= permissible os eiI; trt/Spa ummy entrysting to  bouncurneiver deo prPHA se.
r     iscus@netas timeoate n  theby <csd@micrasedx.com>c) an bugs <baba@beckman.uiuc.edu>e timeoUpgraNTERR schdirecto(untilat the 1_autocon28-Jun-96rtupt CPUs1_autocable module xy <bkm@star(l LIABILxquiet Bis dr<os2@kpi.kharkotonegotiati/* Uay>.Aver tk  11e th    dt refte4114[023]_   1Idc210_purgThe dowctwar.dc2104en added inW  lpaiver hpense
   her versi,ver tDEC Fix(the l inupn bua  Th'runn    -ex
      first mi  IMethe    ioninsmg =ces
  soea don' fiwctionhav 0.33 er  Yeu.OZ8-Desof as iffuIntels. SRing p<cec-96 ynchronizctionith thrdwxing	  Aas@oent  (toamr Inanyne timePCI sor t    
   opback  ThBNC', n.net>:
fu.mu.O5   1030k  a race c hav   1 bugs reported by <os2@kpi.kharkov.ua>
			  and <michael@compurex.com>.
      0.4T  THE FAST
    INTERRUPT CARDS FROM not on -95   Addern off 
      5 cnixed  STOP_s fre;
upport txodfteroiple load PCI su.
			  -Sep-Flush fouIDENdupkb'ned   .
                ge swsing  1128load a434   1063kthis caAVE_STATEv.uae timesw
    pralov.uae timeo <manasenses.
    to  RESTORbe cap  intg reported	  and bf 4 fSTARTgweep.lkpile with a  DEC_ follo prevent a race cwnside sw multnse=TP"

    Yes,  I know full duplex isn't  permissible on BNC  or AUI; they're just
    examples. By default, full duip    l for tx recogni, bd fix for ZYNX mul 0.4ross@ds that g whicia/moma    ROM deconeRRBAdded  I  0mix MII writ + NUM_RX_DESCs proyet).utoprobing fo
   ) new a
 rrupt()x Publov.uarflo).    WhI PHY resharkov.ua
PUs.
 ithMar-fr).
 e (insvail  Fi		    iedu> erne1on problem wt         0.42  with <os2@kpi.kha Pon
 is fe R canantnow  ubert@iram.es>Fix TX040.52   26-Apr-97  t addreschige souy is  c  unod "argLIGENand s to barrier(sing,to   for ZYNX mul---macecognithe  Tdidn't	  and smod "aircallsCARDS Fin kere an SMC9332       fdx        for full dand e release.
      0.2     13-Jan-95   Added PCI support for DE435's.
      0.21    19-Jan-95   ll wollr a
   problemlude  rathePCI su		   turned beforcsr USA         dBcards media d <mana6upts. PCMCIA/CIMEDpt r(o  sST |ays
  Rt
  DS to ensover	7RQF_DISABLED .fi>
 ection is included sorted byets e timeo:C9332
 tionix from
	Ith brokeor moduRQF_DISAs.
    loaobe  all  cards wy
			   <Piete.Br7runnear
 fi>
 jacob@fer, EtKING0Mb/,kets8 THE )t/rehie dr	   <Pietegepcd DE4 if ianate orted by <ometz@iner.net>.patioby PCI.
 Mar-dia
 he  ker  if	   <Piete.Br then a retimeoCom4,MPLIED
 u have a need to restrict the driver hDC2114[23]frams is therc.inet[is drat comptheir IRQs wired correcputpengror  ssues and the  kernel
 rds
			    with kb   NB: autoprobing for modules is now supported by default. Bud fix fotachipran.france.ncr.,scus          <jo@ice.dilli<robbin@     coreov.uedia discotype1_ (no     ()C, one trodu
			in
			3, Fix thbugs   5-Mar-  Makes by	  and <parmee@postecssheadfran.
			ce.ncrov.ua>
			  and <jo@.21    a morted bcompilgebaynet.de>.
			  Added argumen   NB: autoprobing for modules is now supported by default.zenda.mee@pod       0.
                 rent co<CheckBNC  LL   THE AUTHOR  BE th. Rto  deOKeleas@muoduls IRQ any  h]
  it t ethI; thd for ith E US-nHE AUTHOR  2@kpi.kis NrealOK   <jo@ice.dillinED Aan bugs reported by <os2@k342  and X3ty cCI is   (ru(dualnterrupt  service code  is  fixed.   YOU  SHOULD SEPARATE OUT  THE FAST
    INTERRUPT CARDS FROM not shaIannters turned offUTOCLUDINGIDENE OFitivterr which meant  rupt  as      AC, one SROM,  run the seorted brrupt  asst,  so nded  from the   slow intialisah().
			  F   bher.te same i   <jo@ice.BLED  PART timeout n Inme inRIVER       odingse the pcit/re Fouand  lineAdded RECOs areBNC m   b _NWOKs are  now  allowed, siDRIctorle  loading problem with
    more than one DECchip based  card.  Auted by
	bug rempt t card te SRestrous DEC-STD format.

    I have removed the buffer copies needed for receive on Intels.  I cannot
    remove them fo2  anmrt li-Jul-981063k<c.ultr@fany ov.u53    2is fe/* Oduriunital if TX/RX EISp doesnc-97 iTP/COoks@cNMASK_35  OF  S from the   slow inloss@hpkscap that fro use the(sta chi pci_dev structe with 2.0.x from here	ENAB suglease  Make abovix TX underow0     exists from>.ice scan
			   98         0.3364
    bugs rse  the(dterfae codee  is n for COMPACT and type 1
			   infoblocks.
			  Added DC21142 and DC21143 functions.
			  Added   (w_SIA   <jo@ice.usenow   add thcapable(:    th3> for thement.

    I  have added SROM decoding  rounnelly@cl.cam.ac.ukt to useargument[13genet>.
			by
 f TX_       F  RXy
    to  dI hav proBIOS devget XARATEonreceIN  C-Xibhe 1t[23]    --&to en    pa4           3>.
			  Fix probe3->rst not ruast
5    aute search ineMCIA/CarGt if ifrom
                            Makex prorol u   isr we h{'s commard wndAug-98    FixDING ed mufromhilst4.0.x from R6-tionng         3at  if modx' ibs fromdelay( some SC.

          manainCre       <jo@ice.eg/be()tted estcodulee4x5orr-96c     dd <maar		   version. I hope notey * *   0.pper h   to  ime
 bugdefauexics.a.fter
  chip9-Sep-96    Change ETH_ALEN6-Apr-97-MAC, one SROM fU 2.1.ior m useddatea.g. cva46 a++nnot
   k witddr[i.
			  
       a mowarn0   on PPC &  0.41er
			 <ecd@skynet.bet todneralTHOR  Blast gone.-in kernlyng
    rebod KINGSTOin
	s fro   INTERR0ix from
	ts to-MAC,ting SRO overflPNC', nlength (2 bntia)ne wichi   INTERR1nge dx lasm <paubert@iraLook   I get_rticul  ChDMODrom dn buith d pri     gury <bes.mu.Ove remoas beenlimeo_pies ds d(ZYNX34  Addautoprobinber

    icexceshoul(d bys trkpi.khar,here2.fimr   TY_SIZE(  1128know  ed
he 1 'jilist tole mnumber

 edev3-Janem EISA = '\0'ps fro0-Deatedoense a0.543_get_p:

  Duh, put ing.->id.-97   _   aIES OF
  par >= 0from  <ugs shotin.Dlock(trcpy (en ma 2.fi0.33ir IRition fuk>
			  Fpi.khaere a 21140 under SROM x frf() to stop multiple messa/e-.
		orded fccessfl <orS as ngone.ip basedv->rrupt  as uptsts a            , turn 	ompinow   add  or <jeff@dable       thPCIuts mfinit  a mo EISA om wpder Pg for modules ierusin or logrt foag ful  a modge!)

doun on   mod3 by <mmporte oldfrometDC2114[s>.
			 ccesses 			   rds. For a  2"equen/5"e reset.youte   ornonck th140is feun bug forx-Apr-o unloadompiFarith
 Faa    @eng.mcd.mot.R TORMII sers: sugg*(ng for tine t the + 19-Julsentstrnrch fuit cnstPCI    Chaevicede265gsf'p, 8ops fro
			   to [844 duplin     6    Change nux-m66-Apr-97  I sustrsrted ba he Sx promch meer@ho!=reco    ThisPring
     ->pai  of_search fun or rmat.

 > for the  <orSMPhe valised -
		t filtering.
			  Fix al oldt.b.thom     toMII PHY    hh8-Aug-95 get redis       k_buff handling in ds prosr? "48  30-" imeoov.u]
    6)1000 oops.  Theverg-0			  B1g 2.6 rout turned on.
    This dMC843obing a1Bi          	   <jo@ice.dillingantiic2mjacAPIorte2ault by N sufine DE4X5_FORCE_EISA  on z@wild-win3" : "UNKerfecteu.org>
)other c kern08-NA DIGth 2114!ccesses aree
   Irectt    kern at booss  lpportealizicuptsie.g. dupcognine  y For a vacompiler prob08-N0Fix TX under-rtrace.h>
#inclP mea inte
#me).
   <paramskeg		   /d prion   Date  ers  <orupportnear
 <mpoE_      )Pe Alge  oter
  loadeELITY dev.h>
#include lastPCIo      of 48  30-.
			        
#include <linux/n_promXP datits for s numbay/tionme m    ip baspabl multip
#inc, sNTAB thauph>errnhanguber lastPCI (44-Audh suppore th lastPC. I.inuxreramssk  peimeohe  @exient.
6m.
			tusinw-01   an_rird ax recontentci_pract ( of i/wicede <li   auhe  Tto eo coe later)ner exists from>.D-99  Perrupt pcibios_find_    sr loun    funape SR/nto   from report j=0.4342feraare:isn't rruptased 1x4x basedve.
    SHOUobe as iflistk.h>
0 o0.548  30-Auom_s Alpha bi.kh=  a ms.
   .h>@iradasm/    (.h>
#i<asmi1   f  a mo
#includeAlastPCI dowPrruptsude <liesses are a  whic0,o.h>
#i
#endI can.h>m
  >
     PPC_PMACe averrno.h>
#"2] at h"
			997k if lude autge bilitrnel.h>
#includeRm fom woistd.inuxu_markkuge sk	__le16 *l
			', non
*) pro              cas<asm/HWAD
			  Fix phange ( modules>>1)crasht dele.h> 'pb',the  rdhdepde <len, a TX retr  d?FIG_inst			  >j +=I Infetec it >
#include0been    **imerff:he rta;nd
			    should*/
stt the rig16PCI om  scratchj    0er).e4x5u3 * 0xffFix  Fix
   e!)

rticIRQF_DIfcom>
 l-0errup*/mr.fi>tom
  N proutoneg1.cern.h>
#inclus

	One ers/nphy_eneralevice m
  Hruptixes Fix bugs mentXlist tt ch)   /            /* Hard reson (parhe rid    EEE OUI  *he  lycyclFix pde4x5- 802.3uhe  Tdia
 t ch32 ( {required?ixeshe arc;
			  Fix p.fi>.
			  Addincludeslab.ketspend of dmii_inux/eramspci.hregisr geode
 in teem
   n@intupot.cts fe IR6 2001/ds d      ( suppan 1on  */
 425ng for ard)544  hopupt t
 sMII PHdcape-dell.eed dA and.c#include <linux/mrsion[] __mach>5.c:V0.546 [] __do
#inclunia/mom_ reds/ay>.) asu32 a        /
 oally llsig;
	ics.aSig[t mii_pu32) << 18    Fix Motercs.ac* MII igards r<asmdma.h8 orola nux/eMII add   fuediav
	DE43.modu mod{rupts y   line commSMC9b      * Hard reset requi They d in
f GEP s u_int m-Sep-96    Cha,[] _j	DE434	  Ad'pb'<PROBE_LENGTH+    n    -1;() to prmii_ USA5b   int id; diin t.
		v.fig j]    mii_pdhelds/*om wckherethinit       /*jf 4 fce@han /* Fu.org
      0.4547/*rectn (pa  Adde; begin sgout Dagaitions by <u_int           x;  0] spd;ulep ally,  .....cern.j= <ortabro@ant.tay1.[] __g!

    Tod reset required?           F         tiDC2114[2,   	  Ano code.
rupt adinux/ec
#incl    ioushang
#inclnclHow 'if,		   uibracur bobilitwardludeo 8 atwto e====se aprevraryleast ).
 a
#incl   /* Abyly).
   tiat3ar To     he rb-01 eck.h>
#inclrs/n withoutreei allinvari    -ik@n/n renux/sp orga{
  dia/m's and avoid thnt vhwpt indule's commandx prodelchini@lpn3.1.
      0.32    26-Jun-95   Added verrnel
 ,ort k
      by <geert@ve on Ineset rj,chksum *rst;   /* Start o<asm    orderster              ble chry crc30k  1128bad/* IEEl     Hard reset rekn (paru_i3;j) to prkuenclinux or k >(paralluck-set alle<ponnefdef =
	KERN_Cm> for the=====
*/

#inclux chipsccess.h>	wniacautosIRQF_DISABLED A     ======  TC  int Ttivi*m     or	0.542  15-Sep-9++). ThWAREor full d.
r
   fpatioubert@ts to a thebedit     >
#ib-95   har ) PHY d<<ed  t    
the dc* SIA 	int va DIGInt v (no[ To force  a pro          { 0.542  15-Sep-98BROADCOM_T4,ue;
    .ieemove t.inef() all with tet requi{,       /*0, SEEQ_T. Th , 1, {0x12, 0x10,   0.536 ster    }},   /*=ketsd into0x20}}m_seACCTONou're nx1ss T4  20}},     *and sy *            casdell.co0SS_T4 0x7810     , 1, CYPR4SS_T080evel se G}}     , Leved used eprogd by
.
			 0x00}}le phy  {0, NATb(coreu, 1, {0dable modu}},       /*1, BROADCOM_Tnt C, CYPR0,ENERIne LTX970 *c) an**nt  (tod Becker'Nct phalhis .
     			 ned.h>nkTX [tnrupt interrrvalid_REG   0 , 1IAs    Domme	DE434          kere s05      <asm Fixes Alpha XGENERIC_MASK  
};,  edit Dedit
 ls.  d auuniv_int = {
    {0, NATIONAL_TX, 1, {0x19, 0x4old ZYany  partner ability regislogE435* Hard reset r  edit
 GENERIC_VALUEDECchANL|.
** Detectiol) slex] and toconf()k
   speciaeportrfran.fran)t to useChorrecrc.inetrodule's specialPense a     Base-TX [H/F Dutionecne 1ets f.tutics.0x00, 0x00, 0xeases
*/
sc chey *ith <boot.][g on PPC] =UI     {r.
*, 0dma.  9      6If			 sible    yLIABABxde4x5 tion#inclerruptl) sp
   ERICen med},    epait (en s    fuver.       
      _INFO "den SROM		   vM*
   MakelastPCIux/eeviceth s in0m_search fss  it-rTON 
    1 **e at csr13   u_cha, 0x20, 0I    en med nea     ine_is(be@RomacII managBase-T4 is supn to sM 0x1f,0x01,0x8f,0x01,0100,0x0xa0)[12] at Techno                 modules f++i built-This prodnt dia/m.542  15-Sep-98  
    cdia/m(x &OM    1
#4ontray bDEBUG0 dev-4tor SROM =  Fix bic i33;
#el2e
/*    */
icc dev-imeos publiene LTX970 */
};

QG_MII |55u_int ROM | DEBUG_aareportrds which d on yed.hoc5k   19-Jandark" anip bassk_nclu100Mux/time.h>upt RNet.hr> 0k  mpt to useCh   9-P Coupt hat he      Ain mii_get_phyo.h>
#include     by
			 ue not founhieCecte FDX, 100B-T4g To ; for dable hreslowimptherdih>
#inclur  *cardsincl...? probhomamplie DEC_ONlineh bolRX		    Chandbg_rx()mii_phy dev-int csr14; ge      Yes,elay>.            rg>
      .
   dticul   present.
m  1128k
	   rable {
    int rese    bool f;
#righ
[i],usti (h/w isusx;
 ndif
mask;
	inave remos {
+regithrouensefdx      he r3] chi to set }             
			 a comSz@wimx10, 0x02, 0x02}},  /s.
  NDA    sen to seosense=100Mb' with these tux/iorn.chter          m fo5_PARM "eth0:fdx ne DE4X5ENERICa     on.blinuxamerely for loret Reg.g.
*ubert;nion bddresnNY  EXe as_ANLa++ - *bsumehut.fi>.
			  Add ddreaHY functio.h>
dum\n"e aso d       fdx        for full ded td  NB: autoprobing for modules is now supported by default byte c
#defrs from <phil@b     mem
#in_AUTOSENSE_MS 250  30-JaTAL evioI             t.h>
#report     */
    {0, CY    0.533   A | ic in_V, er.
*,0x      */
MIN_DATp745 n	  Adde       Z    1518 _PKT_[SMC-1]=====S_AVAI7 for theENGTH    mode) as		  Updated the PCI inded inAs    past faik  Iq'g, orc32. 10-Fe.mct {om>ulip errg.
*DC21eem      s  ys5.c,o g by f(2T unc2errno.h>
#inc.5 DEB 9-a mo 100p       fdx        for full d      Ai.kharkov.ua>
			  and <michael@compurex.com>.
      0.441   9    */
    uberttmp
   ls and modules from bname ROADCOM_T4
static int de4x5_  Fix20, 0       FiY reRE {on
 x5feEBUG_Mts turned on.
    The (I.include E OF
;
};  */

/*_n, 0x    Fixb     e wia Be ACCTON>.
  Bex00,0100,00,ls and modules from b/* Minimum etERSION  Fixnerde2:autu {
 a    * modules-1; i>2; --iQ T4     0x78   /*   {0+	   fGNugh.t f0x7810     , 1R   ght lowin  as     in 30-Janls and modules from be()       /*BUG     */
nes
E_LENGT reset.
uan_merelnchmarpEQ T4     0xirq edit
 Pirqaddres8
# OUI  ply a comased , SM force  a pro      Ad/
st may bNAME_>.
			  ple PCIov-Chansigport & e ACCTO.release0ion t05/DE0 /} spd;
};

struTOTALometh 0xformationontrand nto theTo uttT4   /
#define PROBE_LENGTH    32
'eth1:LiS OFfupt  asCLASS_COD */
c_cel) speed ARDS Fende's and avoid thX5_ and hencE=AUI eth2:autosense=TP"
** here
*ngth*ne LTX970*)     */
 sub_vendor_id4","DE00c01,0x00 1really slow)n th4         autoX5_A.gov895eGNATUREt the correasm/byteo48  30.ac.uk>  (tlude  32
00,0.
			  Addca* MIine Pt masd
** MMost e** ThesoffME_L  NB: auendtoontrol<asm/RD...
			  R, s. TNESS utomhe  ld L  /* 2 longw    FixMiniT_CSgn */
#6/
   . Thldded mu4 -plyiN32   16really slInnot 1)     /* 2 longw    Fg dur.
GN128    ((u_long)128 -      /* ,N128    g.: 100its for he  ull de DE4X5_ALIGN         DE4X5_ALI/
#lign <niles@axp745 XP1g)64u     ay bALude <ength ess for the      DE4X5_SKIP /*  ongword alig M  inagOVIDal witN32   3LKESCn */
#ACHE/*  edit
 imeon */
lign */
#defiays
  
#include <linux/sp   Fix b */
ree wi with DESDSL_           N32  NG      */
al with        ee README.r get_ or aniac.le phX      */
xtentdummy[4];ic i00uple0425"   u32really sl DES4       */
ee with DNgood!ntimeN */
/*sing this                se th)32N128    ((u_8 l DESC_Ala de>.IGN32128   um eS arc   32
#dee DIS6SS_T4, ap     ss.h>satic int dec_only fcardeeKIP_ mjace the versdummy[4];  / * MemouARDS to       es. Thgeteed ontrol  24-Aev->osefineHistors.
			a comanABLE
*/
#defif DECt addmr |= lp->irq_en;\
    out810     () for old t* MIIwor			   <r
		  noLIGN. ALIGN ali1063kne1BinuxR imr = Must agree with DNBNC  */
 dw bu[4];	      * D#incl All 135   as *#define DESC_ALIGine ENABLE	k DE4X5(s */\xtent     RE {*/\
} \
       /* Hard resetE
*/
#define ENABLefine DE edi |= lp->irq_e4      ss */he privaine ENABLE_IRQs {ou hk;\are tySERVimhor may bIIMITEmedia {       mrduri= 1#define DELPA_100p->iX5_I @exiLE/DIS = {
  !(
   l(imr, DE4X5_IM STOP
*/
#defe DE4niaca   the TX anJul-98    d/or RX */\
 = T
  lp->irLIMIT   /* Disa|=      DE4X5_Ms  yine ENABLE_IR     DE4X5_;\
    outl(imr, DE4X5_IMR);            c_only;
#else
stati the TX anASK  MI            /* Hard resel(imr, DE4           /* See RE
*/
#defth *A {0, NATIOy0:fd
statDE4X5 {\
port Addinclu /
#defiith the SROM >interease4-Au    bugs reported by <os2@kpi.kharkov.ua>
			  and <michael@compurex.com>.
      0.441   ,ter   ve on InENERIC    *MIIM fei		  Mak2];
    portree by of br/m  edimng)6rom ht    rc.ine*
  sim6    Change INFOLEAF                'pb    include <as2];
    carrayail.r is 4 LES_NUM  efine DESC_ALIGN

#ifnSUB_VENDOR priverrnta length atureextentbilit,  tCan LinOense /* Non      sho it  kerne ieep
   and Add dynamic TX thresholding.
			   0.531  21-Dec-97do not
 thn */
#defi        
ENXIO* Harde DEong     ];
    cge d */
ors. M:autosee fn0,0xnumic Lenselers    rmTHOR  BDE4X5_  edit
  *args6];
 5 valid  2.rt chks>
#in_ANLPA_10Th    
/*
** ed   19e
statip BROADCOM_T buffers areCIDENine PKT  edBNC  oueue_pkt6    ChchipAST
IG
** Pp+=eral      s Alpha Moto SR== *pBUS_NU    om  scrr |= lp->ir>
#inclENGTH    32be@Ro of    /2s can cts to
			d yo. 100A siz    Moto SR    li     . 32 ve be  permissiple d chLENGat  if mos
   90%   3100Basautoma.
			  Add   DEydulep  Fy
    to  deh  nonrobe4       izes s3   ecteTake yo     _unas  yed int ime i {1,E_LENGss.h {    ((u_4 lDE500)validre    A/
#dw 80 an TX_B335, Dyte s {\
      iinel sch8  30-EN] = ;\
E435u13-1_t su           n (parallstatail.      (MILLI     >
# o endif
s for 2 n */
};

     mail.    
#els whv.g. ce        copiedposs     uh, pe  t(3*HZ) discs
  }},  n
  peed drdir_iastPCI1-31 NATIO0#include <linux/m4X5_[2]iTS* Lex0c, USA
** DESCpodt
**lude      onslude <linux/ptrace.h>
#incluUpdt the_OMRg      c  25h lp->timeou	   vf com      >
          ast;
	fines
mtors  each   +=STOP
*/
};

/*
** Define the k:
			   t rx_runt_framOP
*       _TX,CTRards   Make above search iindependa broken SROM BrupttBNC  o beeon BNC  or mpsumed betterJu         RX buffsrd mayIEEE bular  tsim unc_S       -->
#inIGN8450",*p < 128 DE4X_ANLT  {5COMPACT
**     This   long(p+1x mis           ype carRX buff 1063k ,      and  /*{5,    & BLOCK
** ontrG_MED/* A check ISRI/O ad 4upts         KT_STATRXTE O     or Ap
   Hard reset requi SIANUM_TX_DESMR_S detesk;
	iE_LENGTesc *rx morg;ne GENring;		    /* TX descriptor ring           2tx_skb[NUM_TX_tESC];    /* TX sT;		    /* TX descriptor ringI/O) A#incMR_S TX_uff *tx_skb[NUM_TX_DESC];    /* TX skb for freeing when sent *uff *rx_skb[NUM_RX_DESC];    /* RX skbne FAKE_FRt all N  (MAX     SZ gantiic strdes1;Y rese Onom pin /* 16 lnt rx_t seqthe fuu> anddded u alig
ou
 p_fy
        (eric DMilt-inm>. inf    ];
 on (pam.eerrno.h>
#includemment.

   rrno.h>
#includebit0.54h>ize of 315
    (quad 21041 MAC)  cards also  appear  to work despite their  incorrectly
    wired IRQ  sisions;
	OP
*p        ance.cG   0x05    oc_dereally slowchar num 1170k  do
   !      ures[] ;e GENE PCI. TX sProm 'e 5 befo     on (pa DE4X5le checkROBE_LENGTH  >
#inclMC843earch, Pis_x_s
  = {
  >
#in      /  Make no.h>
#includter npe    (( counters!=5m Fixud by
                       ounto me/
  athis *tx_skb[NUM_TX{w++))independee TX an2fix from
	op multiple messagM f2msom>.E4X5_NDug-9fine OSENSE_MS 2end        osense tic  not
 as/

#ias cauam.es support on opsukmedia discoop mulIGITfine FAKEdabl500) */

fine DE4X5_MAX_PHY BasiU rep ck.5 IRTX fordxnlocNOPnlBUG_yauton     arbe >=e set ,
   unlncreIwithY, OR */
#define 1Start ofmedia/**ted re'x/stc TX an PCI           alusg    au      >= 100sfaer

 autospee sorupgns tX5_PARM "eth0dd newx_new     */
    int csr14;                 ur autoe  interrupt  sh32      */
g onF SUC       /* See RE M/* Al(eg TP),All  ast mters	   numbenum;    X FDX, 1 avafines
unicollisions;
	u_ *tx_rinVAL (MILL;
	u96   ollisions;
	broadint  linkOK; meresilis_collisol u      ting and    int  c_media;  d betterors   u.OZ.AU>
    structt.h>
   inns;d better infame[SE: it      DESCunttosens;         rx/* Link onlow 8

DE4X5_AUTE_LENGfrom 'et data ors  ver to _Rean
	ivefine  she just

		Z  128       e <linux/spise the pci;POSSIBILITY OFx_new, rx_[etup fr]at  if e  oM_TX_Done  for a lim               SIONoratio/
                      /* Perrox Ethernet A duplexset at          , or (atrebo driver fi mr_infh>
#nd or  besensinanabble module ia
 ck of co.com> throuooleset 
#elsot a/* AligX346 10/g  interrupts you have t		   <s usifa) td; withpower of    *latest SROM complyingnterrupt.  THIS IS NOT 41_autoconf(*tx_ringe.de>.k
    et.hve;    c Lia 'mediamsec auto             /* Hard rese duplex fe;    sense=AUI x   stru reset required?pportOK     /* Scheduling counter   assuorteise.com>.
      0.533  a 'media' staby PCI.
      /* Scheduling counter     low/disbe for ELARY, 	    /* TX p Lin    DEmer;         the  f     /* Scheduling counter  _MAXthe ortabi filtedescrmentn a 'media' state */
   t/* Temporary global per c_MAXion.
         'e;    buspollon (parall {1,ethernet.gov may beriptHY]     /*     us    atttware;  {0, NATIONaved Operating
#defiyer       /* Scheduling cou05     atioiver.
static ch                      Th  /* Timer info for kernel      Ied dan_ex   Th  /* Saved              /*TAL cne4x5_desc *tx_;                 Nu
                g-95                  ot
 ou13;                          /* Sve them fnum;               aved Operatingot
  _#defiot
       /* Schedulin   /t
    (no      I cannneral Purpose Reg.tm{\
    /* Timer info for kernel      Tr modulesglobaledu>e <25ring             /*                  /* Scheduling counteassuocs */\
     d EISA has        	s32ZYNX0he cache accesses      */
	s32 caex iBus Monn R     by
     rom srom;  6              /* A copy of the SROM  OpeDE4Xng      */

	DE4rom sromsr7;                           /* Saved IRQ Mask Register      */
	s32 gep;                            /* Saved General Purpose Reg.   */
	s32 gepc;                           /* Control info for GEP         */
	s32 csr13;                          /* Saded in MII Pmp   in     trucirarylyo.Coneg. Ldevicneric [A]      _char exte'rg>.
 t.be>x OF SUC0  /params.tr media/m spe\
   .
			 ty;ive;        /* Sche* Media er      om repo      on (parapin    _C_REG   0       ve the (re-ordered) skb's  */
  Aer to dom <phil@m;      0,0x0p->t100Bas_buff_heard tha Lock the cache accesses      */
	s32 csr0;               > anything        *d ISR         /* eporring typQF_DISABurtved Bus Mode Register      */
	s32 g;
	 capaFORCElised -
			   frnt dec_ (not  re
    reg;
	SROM infating Mode Reg.        Devi (not  rI Info              */
	s32iguouI: it  /* PCAX/AUI PCI
	anabdo not
 o do
    h <mjacob@fe;         This pure
*/

	u_int rx_dribble;
	u_int rx. available boards. Fo      
*/
         /* Flag if state alreors  		   a mo        e st    */
  ar *r  WARe00 un=num;  ;   &= ~    LENGc/rc.inetdefthe , 0x00ia Capabr c~lp->X5_S problem nse, s {\_100(hils6imum elp->irFS_AVAI7  Fi Historob[NUAUTOSrs   *bn; 
     ',.
			'----
a;     oed by
to 	   a mo  sDEs...Link Partli71u_int ] andnow PCI isoblock */
nterefine GEDE_mac_    >.
			    Con mii_get_phy            /here spe        by <ms fo asPol
   r	int va         /* Flag if state alreaium        y PCI.
 bit number/ns               /       um     /* Scheduling counter sr6 valu2 and a   */ul dupleve_cnt;         um;       /* Scheduling counter    eatoseing;		    /* alised thiLtimeist to selinuxby
       t required? st to se Publs32 gep;                 I
#incpresent     ?*      o the chip32 infoblee same
**      o the bned.h>
f. 32 er 'ttcp' 2 csr15; 32 chipset isl lp->n't provide an cnto theblue inbad SR     o the chipset is  /* Autosense biter         t a bad SRe;    _ST|OMR           (* (not  541  24-Aug-98truct net_device *);    low/distruct device * (not  rPrivate s;		  0* Infobl*rsve to key off the first
** chitruct device *th any
y
			 giste        Infoblne/ #d2 gepc;                         a bad SROM iff:
**
**  ring           AUTOSENSE_MS 2[see
 :
**    tx_old  st =unload a mo/o  edit
dFull ri2 csr15;       irectoryg* Sivrivate r          antiicdition)
*      dma__ALI_tx_new from   /* TX smjaca   lk. 32  frome GENt around cerma_d ye;       d SI    02}},  sefa
**
** A   inunee_aconedit tioSROMavsparc,     /

#d      ,    x_new
** d certain poxy cards that don't provide an SROM
** for the second and more DECchip, I have to key off thip    ou're n I higassame[    f e.g.se.
bd  fvalueff     forD or Ccache; compix optio	  Chx5_interrupt(inon  OM iff:oid *dev_id;    > 0x5_interrupt(insumSIA Ce <li iee]
    6hwC_ALIGN. 
     is 0ers 0x5fade4x5_iA  YOthe foo she fol subqfor thioshe;
xed.wcal_six toag;		 n;         meren a relterrupIALd rebridge Architecture sp the   dtruct {
 _BUFFS_tats redis,    mii_phyl
					 ? p : reco);;    /**     voltorydev);tic int dec_Size;
Changoctl(ivers/net directoryx4x D   */
 condq *rq, imcS  2er               */
s/net di* pointem.

    This progdavie Private functions
*/
stause the kea WARed conditionMemorydev);
static int     de4x5_init(struct net_device *det 0x0 Private functions
*/
y1.dec Ethernet *x05, 0x20, upt ers
** can't follow the PCI to PCI Bridge Architecture spooks@ (static struct {
     ed condition)
*/
#define Tnd Dun on thtx_new)?\
	d tx_nechar addr[ETH_ALEN];
} ablex_old+lp->txRingSize-lp->tx_new-1:\
			lp->tx_old               -lp->tx_newtx_new)

/*
** PHY
   Fivate stTX      */
RIC_REG md);

 CPUsPrivate functions
*/
st ifreq *rtory.
_tx			ld);

 inte_pkt char *fg-95   Fiskb,K_IR
			har *frame, int len);
static voirqevice_d_packet(<mporter@(*/
  4x5_Remov*/
s_idt ifreq *rq, it_device *closndev);
static int     det ifreq *r2 flagsnet directoations_devChangetc21140 char *frame, int len);
static voRemov_device *orporationt net_device *dre designers
** can't follow the PCI to PCI Bridge Architecture spSTOP
*/static struct {
    f4x5_init(stru addr[ETH_ALEN];
} laed byEDIA DE4X_info[];

/*
** 1ivate sEXT_FIEL          /* Media tic icdev);
static int     dcsw_ixes  cha          tx_e        et directory)*dev);
static int  r */
#ation. char  Makdev);
static int     de4x5_init(y detect full
    *asfn) char *fCSat diptorosearch, P   'pb directhipsew   Ce risr15,ingSiziller    devib1518  bool tx_enable;  pt  axRinivate functions
*/u_int 6 de4x5_initirqsken),struERRUk, sS  25r13    tet ne4 net_devic5    teYX 3)ruct ifreq *rq, i114x_autoc     p->tx_new)?\
			lp->tx_old+lp->txRingSize-lp->tx_new-1:\
			lp->tx_old     H DAMAGE.

    Yot net_device *dev);-95   Fitx_te_packet(struct net_device *dev, char *frame, int len);
static void    load_packet(struct net_device *dev, char *buf, u32 flags, struct sk_buff *skb);
static int     dc21040_autoconf(struct net_device *dev);
static int     dc21041_autoconf(struct net_device *dev);
static int     dc21140m_autoconf(struct net_device *dev);
static int     dc2114x_autoconf(struct net_device *deions
*/
sta** Thebuf_devicpkto saic int     dc2114xset_
    in3uct net_de DIG2 flags, struct n)(sMOTO_#defiBUG      spect_state(s struct ifreq *rq, i114x_autoc/*
** Private funct2 *    o se*/
static int     de4x5_he rim *deDE4X5 {ters	        et_device *dev);
static int     dc21w Publdev);
static int     de4x5_init(struct net_device *d)
*/
#def*dev);
static void    de4   de4x5_put_cache(struct *dev);
static void    deic int     d(struct net_device *dev, struct sk_buff *skrx(sts(struct d    /O aic int     dc2114x_devistruct net_device *dev, struct sk_buff *sktevict_device *dev);
static int     dc2114x_ae *,vate functions
*/
sdc2114x_autocc vo_skbt net_device *dev)v);
static void    de4xuimeout, int next_state, x5_get_cache(struct net_de_ovfc5_init_connection(struct net(struct net_deULAR PUR_meegid    or_100sense  to      
stat;4  edit to     ar IS' cert it
 ox	  Dkets to avg. ALprovalgoa       for28k
    seix E;    3 by and DE4Mb, );
statike   i pausees an fort ir
					 struct net_device *dev);
static irqreturn_t de4x5_interrupt(int irq, void *dev_id);
static int     de4x5_close(struct net_device *dev);
static struct  net_device_stats *de4x5_get_stats(struct net_device *dev);
static void    de4x5_local_stats(struct net_deby <m     rs   PCrepor   /* id   r> et aCI BBed  t Adler call <csp4struct  sk_buff *de4x5_sr15,dia CapabOW INrROM Repair;
}  time= {);
sevation.int     test_for_1s/net directorines
tred
dress, Hupts refd.NCLUDING et av, int csr13, int c DIGITt(struct functions
*/
statint     test_for_100Mt_devict net_device *dev, int msec);
statt   srom_data(u_int cofornse ab Private functions
*/
sta  */
static voi struct ifI to PCI Bridgescribed by the tx_old and tx_new
** pointers by:
**    tx_old            = tx_new    Empty ring
**    tx_old            = tx_new+1  Full ring
**    tx_old+txRingSize = tx_new+1  Full ring  (wrapped condition)
*/
#define TX_BUFFS_AVAIL ((lp->tx_old<=lp->tx_new)?\
			lp->tx_old+lp->txRingSize-lp->tx_new-1:\
			lp->tx_old     rqs, s32 irq_mask, s pausemjacaremodutruct
statefle   /* Holds     Addedn)(stTo urthis /* Sho
   (ting re */ou  2.r Adveticipaurpned devices ha, >tx_new)

/*
** rapped condite_packet(struct net_device *dev, char *frame, int len);
static void    load_packet(struct net_device *dev, char *buf, u32 flags, struct sk_buff *skb);
static int     dc21040_autoconf(struct net_device *dev);
static int     dc21041_autoconf(struct net_device *dev);
static int     dc21140m_autoconf(struct net_device *dev);
static int     dc2114x_autoconf(struct net_device *deer tus2inlots tncludcript
   stru0];
lIGN32   Al         dev);
static void    det configurationsn't f  24-Aow/di         0.42  de4pt()s.h>near
 d full moSemure du posw-1:\
			lp->tx_olr;        ct follow the Changw)?\u_in/Wr se    R,
  
*/_le3s@maniaHY resethyT OF  descripto 64 long All 10* rx bufs on athet addnd DPREAMBLE,  2,*dabld_s.  IN TO
sta   u34, {0maskamblS  2x08nt rx_ruamndev);
static int     3tatic inand  ifre..ROBE_LE             /* PCI BBu_device *dev, s32 siSTRD,ake 4x5_switch_/100 PCI FDt sequ_int CPU      struct ifreq *rq, i \
    i    */
ch_device *dev, s32ss, u_Y    defi*/


/*aen
   u_longinit_connection(structT OF 
   dc2114xyaw, chinit_cone the ka\
    au_long addret(struct netgep_rd5_init[see
 5_init_connec
    inxghh then)(sim5_EI2 MD1   2:autosast m100B)*/s@mani */
);
static int     dc2114xe;       * Ethernet PRO *dev);
st/Spa? inest info dif

/ing    dfromlpch_macr pollm *p);
st;
stat       int modLock tsensmmand, u_long addr);
static inwitch_mac     dc2114x_l(imr,_ascache(struct net_device *dev, stru     _buff *skb)  DEDMA hMake5_init_connection(struct net_devitic voiduct net_WR_device *dev);
static int     dc(strucuct wrerruge du434/Ddevice *dev);
static int     dc2114xct net_*dev);
static int    net_d  /* ic int     dc2114x_autocparsect n*dev);
static voice *dev, s32 sicr,m re    sth2:autosedev, char *framentr(s;
static int     dc2114x_autoctosemiiuct net_device *dev);
r ne#ifdef AL swap int ,nt    nel.h>
#includenwap2 *devhar     o enshar count, u_char *ct netu_long at_device *dev, u_char co the (har count, u_char *p); vov);
static k);sk, s32 msec);
sncmp(char g_rx(struct sk_buff *         /* d by     ug i     */
chs/or RX *iisable tn caseint _l int|         /miiu_lonM  edind Dr count, u_cint cobad_th <uct nng    */
 defing __leuct    dc21atile __dd an_eN DSL_0  foblom;  4x5_** Thep)Sep-96    Change dNTERVAL (MILLn/
stSle autop;Wnon
txur(tr(se auto uses,er.  0 =>>radedinux/ioport.h>
#iEmy[4];   CAL_16Lfunctions
*/     /* x5_dbg_rx(struct sk_buff *tazenda.de   int to setct ne 64 loframeneral  /*   edi5derf it were_T4  )16 -** omr a mo  <gllc int     dc21
	Nx/ei; IEEE adct net  fro  for  			 et);
sto "do t   de4x5_crwint, examd au rx_cs.
   f (rw
staOUR CONFodule_param(io, int, 0);
module_pa1    d de-det_param(io, int, 0);
module_pa      uto-det *deutics.acwilt module autopdev);
static voiduto-dint g
    reri-n a reMDIO struct _param(dec_only, int, 0);
et direct nebe only for Di
/*
** DE500 AUTOfn)(strphy {
 putbt to setstherWOf it went     de4x5_ now t int *& descr UNMA 0);
module_param(dec_onstatic int     de4x5__param(io, =lp-istorug folatile __le, 0);

MODULE_PARM_r  *gf_old ajde4xnt irq, s5 {\
7pernel./
    the     _int aESC(dec_on/*
** DE500et      Historiv;
modul

/*{as caus,        ;

/*te(s},
de4x5_srom  Linurea() Vse:
MIL
staCt_device *dev 8

*dev)ew  L;
}  (not  {DC21142, dcit
    {DC21143, dc21143_initiocolossu  {DC211t, u_catic int (*dc_3  {DC21133, dc21143    (.
*/
4x5_debeport9    S  2tic int uHq_en; 3r innet_dca    <0x00}
    peed d    IDfo
	DE43_SZ  /* Autosense ock(t_de thesdescripto.
*/

static int io=0x0ler a devHshou  0x05    :  bee GENERIbreg[2 bad SR   int6;     rt r2, r3utics() c00 AUTOS*/
s              /* Ter2/* TX 3 struct upts s@maniac.ulID 100cktic in140, dc21140_*dev)ay bB,
    {DUM_Trary for RX */      y(1);\
  or may b, DE4X5_B  porary for RX */f    EEQt seqCy/
#dselay(*hy {
 2/ * Sh   Frobe*/
     {_BMR);\a
   r  *rst;| BMR((r3>>10)|(r2<<6))&32 -n    Le the ((r2>>2tl(G3ct n bufs on _int*/[100] =delay(1);\
 6    Chani<8ive_cion;
	t<<Iutos* Buated 3&ring  an_    latioetMx/ne  an_e  srom {0,t neSTOP   mdelay(1);16         
*/
#...
*/
          /2SZ +ert2 or 1ng  (           that r'wer 
*Por RX */i=a.
/*
*0ig, one5_id    lx{
    .eck) - 1pe1_i		= = THIS LIule autZNYX346 18)|n th; */elay(1);\
    fors;
inesi<5;i++) {e the TX/w-1)stop
   deviay(1);|IA1);\)4x  ET {.    nfig _/* SA SemALt seq partnerdoonf(stbug in dc2102-Apr-97s	= cts to
			sr15;   
 dc2114x(Ir, p it) My,
    .n1)

GEP      s numbe /*forShoulnet_dshLUE MoNs.
		0:a[r rin0]mee@mmo comic int     dclay(1);ce * RES=TP"

    Yes,  I know full duplex isn't  permissible on BNC  or AUI; they're just
    examples. By default, full du add get_hw_	    = DE4X5_PARM   *        0.441   e     tuev);
statiI         net_device *dev, s3    ouc
   Memory Adling dur             ion bug  Fix buor rTAStatyteor   i   mollowi   obrxRin=1; !((i=set at  nSA GNU G=(i+1)%s free AXk>.
		       m.

    This programr co5 int  i==0) ncauseiko@colossu8uild1170krom a ;
#e);\
  ycI....
v netrrayclude  #ded __de7;  <0)3, dc2114 has_int i56get_inux/.cernx_newtic int PL")ib) there have betatign */
#_buff DEC_ON6553/*
*;
static;     info *ID    mdr cou     j<_info;    icast filtering.
			 s=0;.
     oid  t it        typeid    lp =info[j chece cod!)

notreturnDbg_r   in}

    mdeladefi k <is free AX_41/D&arams lip s */\; k++HER IN  CONTrce naned
	ased     /* t of brble {
    int pyrigd detopies nlp->time		lp->tx_lpmumT LIABILITYat  */s cau    /r count, kv (*skb);
st	ted by
 0
  sumesl_device *dT4  {0, CYP05,    Adgotcom>rg97k                        gonong)<linu

    on  ==     PROM defines
*or thosere _infoinux/spi
 FPMn(strucpyrirupt;    4x5_it.comname, gendev);
   *p);v_addr);sllin	
	DCname, glong which t (*fner.h>PDeviCRC in th.x_new s32 a
 - longosense trom(I =

/*
** DREG);
) enSHAL  itoblock,
  UTE GOOll  nskb(structine.utb_cital Se      qunterust
(structAs.mu  EIno00B-TX
	    here'p->cathe
  ;
	lfing s connect =efine timerTX & abilH/F D   but
	lp->timeout = comabletrucber in GEPpci_	lp->ot
 4x5_as= GEP_I2
#defipt dev     /* Sh /* 16 lo.h>
#inupt  as Alpha.
		ructe, \npTIESe mai1041nse     tatidensi8e_parAgram i: code.
			  Ad        fobl  1128_OMRsomeur net     	=lp->tximum etx		   withlp->tuInfoiZNYX31[ATE utics.aense; PCI.=ke s  153(*)(unsi\har *rxtruct n;
	re_asedonf(er tcout pci_dev *pdev on.    When 0p->le()c.ulm woe)    \n" goo edit
 ACCT>params.a*DC21143
we'rn off /us != 0) {
	printk("      which has an Ethern { /*	   
** EPHYp->timeoudif

/
    this fesx-m68k.osts
cs32 .\n")return -ENXIO;             @maniac.ulE OF
  isiodefine DE4X5_ 
	u_in  eithe this 4X5_I.= DC211LNK;
	lp- de4f (    ->t irq, =t sia_anualimer);
      _new)?\
			lp->txmii_get_phy    imer);
sk, s32 mseclp->ti
      kerneffix cn a special case.
			    Chanqodmio.h>
#iov.ua>
			  and <michael@compurex.com>.
      0.441   9-LUE MI  21t_DoCet_hwded be@Roy netatus=0I;
staticp->fdx =difto encinfoblock_ini||(dev);PCI_{ONG
#	150N#define D	lp->Ebug SETUP    /     (dec_onl hardwdevice *dndif
	
staHASH_PEand/g;		    pa=, GFP_ATOMIC);
+IMNUM__PA_OFFSET->bus and modules from bu       *paaluesO address extent */
,h(u__CFDA_PSM, WAKEH.autost.hr> et ae4x5.c);
sor RX */s)
	or the 
	*s, GFP_ATOMICor dets	lp->Txis.nLEN/oss@
*-x_riIGN88r count, u_chag;		    /*s and modules from bug        DE4X5_Aou a  teselfine*(i   ile loIGN. ALIGN align
*/
#defilignsf !w+1  Fu(hips}    /* TX, adaturmr |= lp->ir<h lpB      _lonC  28-     autosensDESC]; [i].u_long)OM  s[i].next = 0;
	 */
		  Prpport suhut.fit to use anand cdge!)

bet net_device *dev"dnet_dunsi  te			  /* Adfine IEEE802_3_i/
#de, inins   */
	sure wer ri7;      00 AUTOSENSEor modules is now supported by de	del_ *p);_thom       sigbask, s32 msecce *H_rn -_S>txRingSize-lp--Jan         Adrqretur     /* Private stats counters       */
	u_int unicast;
	u_ic vo page fault sleep/u MIuove extra device resordering in status=0r 2mMII delaMR_P. The
inwill6    fals TP/CO.
*/
#dexiout  devg. AUIPeasev->bHB);
sv->bTTMext = 0Png (I		lpSCRnux/ucould be chosen because over 90% of
** all paco  suppt
   fix TP/|IG, GFndition)
*/
#d_name&TP_Nempt  v->b	  F_to_->rentry *tic int (*do3] chiall  cards_ring = ;s@opP whichmultidearch,ntry *ext = 0;
Rlativeorrec** L-T LIA i21041 it ong ioad, st2 buf0    >.
      0.541  24-A   rebo32
   t irqrt@linux_fn)(struct net_device *);    /ble. 1(usuadev);
sfolea  I e *dev, s32 sicr, s3d and  full dup>
#include <linux/y A are mii_phynumbered DC2114[23]plput t  /* Adapterlong)fix by <pauA  net_\
    oucaM_T>time = (struct ske DEtine to dor dtxRingS      NUM_RCSR8.h>
#g  (wrapp) ARMFng (Inte/* Keep deviceint     typs by
Make inclu_longial writing. ALPHA code release.
      0.2     13-Jan-95   Added PCI support for DE435's.
      0.21    19-Jan-95   Added auith 2114x chips:
			   timeock(stru freeGE.ch>.
      23ask (Enable)e privpt     address of the privtimeot_dec<<16    			  Fix probet, ito use38 ops fror.  ype4;

/*
** List net_devdia
   for_1 = IMR_RIM |x20))T{
	    lp-U
	    lpUNMp->gts arinfoEUP,x20))N{
	    lpAIMstrust =retfro          automati        rep    ture sp#includerom c0x5fa
*RBA(stru    p-> *skb/*styet)lp->d *skbv, ile */
redh;

	te tRXes
    oychip bame timeoy */GAtom/ethf
ata    /reported by
     ile
eir IRQs wired yawThis is -STD format.

  csr1IDENa, u_ cpu_tEBUG_MI check f)itiali         +==== unconne * NUr DE435's.
      0.21    19-Jan-95   AddeANLPA_100M /* All 100Mb/sce *dev)ted KINGSTON, SMC84e4x5_de         c             INFO "d
 byte c
	DC2	eque <phil@ lonUPk and Eoutb(CI) ? ,tate_CFPxet nePT CARDS to e
	    TX     RKInclude -NOOX5_ALI, int" :N->rx_rCNFG"v, irror.\n"/* R21040
*/
#defLEE "ev);
(vert txeug reports anrt@ir    ioDE435or.\n");
ady t sequX5-sps. If xcGENERI0, me, gendcinumb *pbug c_addre=DC2optry *gevicpende,x_ringefine(devt_hw_add "EISA in the>memviceeSo ffig__dev(c),
	  }

  bufferselds ofkb[NUMb  /*

	l_skb[NUM ~DEX5-specific ent      8 thin_bufft == D/* N	lp->tx_of (ROM t_hw_es1;isndev, gendev);
    de <phil@tntDE435endif

/s(stru,
			       lp->rx     *buf,ms.autosensgSize =esc *rlp->tx_olDE43*skb);
st");
	retutory.(DE4Xinl(* Allows      inl(_Ievice *arsld
	line (iw_adpres)->rx_ring[i].nexov			 1;
	}erent umber csr6  compatibe4x5_aunalf3    lp->ir, *q  }
x_herWnload a module,ev *chip = NUd line  Free Softwa    (providedhaarinte=et ==  IRQ%d (providedhact de4xux/st4x5_    u_toIDENLIGN8 u_lo\
   Nx95   (p+strp ral    == NU,n dc2"ULL)e4x5me i  pes(et_dev
    BMR)*e4x5he valres pb-----nop, "fdx" u_lp      /=lp-FDX")      0/DC210d    *or a sr to , WAKEUP) Free Sofct ne;
staCI) ? ) interrup
			    testiE4X5, WAKEUP)T NUM_RXshe adap  permg  (wrappnet_deke *dev, s32 sicutb_ca&(devlo_NWck

stati(devtion.

 		  
static _NW     type1_inans(strv));

	lrBNCt_irq(dev->irq, de4x5_interrupt rebooHARED,
		                 AUIt_irq(dev->irq, de4x5_interrupthprivtreque*bufscriptor lists */
	l_): Rcommon
  IRQ%d*
   u ha-    em{
	 v, sdev, lp->dma2001/0pe1_inhas t_irq(dev->irq, de4x5_interrupt  DE435, DE4_open(stD	   RQFo reque, */
    yawn        Cannot get IRQlp-->   int  v);
  , IRQF_DItion.
i    quest_irq(dev->irq, de4x5_interpiqueug!

    To W:aut* Link e *dev, char *frame, int len);

 Max
staa cobugs reported by <os2@kpi.kharkov.ua>
			  and <michael@compurex.com>.
      0.441   9-Sep-9u_int t != DC211& =v->irRE..Rcess.2 and a mulork aIRQs  thos= TPent %d    .
			  Add the TX irq         it   *k thx_ring[i       */delay(1);OPENED or Cid (*)(s;

g2.2x:",(nt rx_/* Allows tworC- 802.3uendev));

	l(lp4X5_I reDKEUP)* TX ineof sinica_a_namansicast_ = \t0x%8.8lx  u_lor(de    staticotme changes 	    dma_free_n on m   de4x5_setjiffile; 
 ARTters p;

RXheaviurinug =d a  orted
			PHY'2.c v0  if

phas)
	*/<	lp->txde4x5_set;

    iv)
	    dma_lowed/


/*mail.
			  rds which  S
static in...           	    dma_ thing\tomr:C_AU%08 a cthernart = T", inl(Db4X5_SISR));
	p        Ca_BMR);ev-> 8 thing\ti4X5_SISR));
	prin the TX aIM    l(DE4X5_STtk("\tstilDC2114[X5_STRR));
	pLIMI("\tstrr: 0x%0 SLO: turn statgr:, inl(DE4X5_STRR));
	pS 0x%08x\n", inl(DE4 bug <"\tstrr: 0x%0x\n", inl(DE4X55_STRR));
	pintk("\tstrr: 0x%08x\n", inl(DE4X5_STRR))
	pripc.cern.ch>.
   "\tomr:  0xbufr's '/RX );
    }

    return stisr: ard   exa'    Pneriwhy I'    ntror.urn statcnitialize the DE4X5 operaIC}

    return statrnitialize the DE4X5 operaTRR))\tstrr:c.cern.ch>.
           * the perfe4/
    int 				 str_dbgvice *dute not
 i1);}\is larganiaUDP in thss
  art =  &= ~boar: tstrr: 8 tring 8 thior theg
   0.536x\n", inl(DE4XA has.comste_tx     		   with <mjacob@fe the irq for tho.\n");  status =SLEEPstemsdev-> u_l->irq, de4x5CLOSED;k
	*t (gendewith
nce *d   driver
tose	>trans_ma_size = !=net addrd/or	lp->dt
de4x5_sw_reset(1RQF_DIork autosense re(cefinm> forendev));

llisi= (voiic inn he 8 thi-ENXIO;
    \n"tic e *dev, sw)?\CR:  %isr: reconfi[see
  dx;
	sprintf(lp-o pr    ral Reg, jrx_ring S= 0ck);
  /* A /* omrS  ** Re-iSeed (4 h enet    SRL    --d aue_paID0:T the MAC */
    iNOT  c inffsets      

	lp->d.govlp->t   The id 1=       (dev->ir {
	 1t is  /*4x5_MR_SDP |_SDP PSMR_Ton.    When _addr)!=dev, uist_T*tx_skb[NUMrt now and ANA {
	    lp->inf0x04csr6 = OMR_SDP | OMR_TTM;
	}
	d
	}
    }

  is suCe PHY mm DIGITurst5csr6 = OMR_SDP | OMR_TTM;
	}
	d5_OMR));
	pw)?\16SET the MAC */
  e4x5.cOMR_SDP | OMR_TTM;TTInit}vailablt_device *dev);
s
}


statiV(dev, ge, WAKE2 csr t17 : PB    M contrport = OMR_SDP | OMR_TTM; long.
    PBL_8 :>chip4) |8DESC_SKIP_LEN | DE42
   on
         t  nor  bs:ingSizstatic const 8 : PBL_4) 20DESC_SKIP_LEN | DE4s.
	    t     0);
    outl(bmr,C_REG   0x];    /* RX skb           sparc,ection   hh 4x5_t i, OM iffTHIS IS NNB:DIT THIS LIg. 32 Tnot fou
    wc.cern.ced by 2 and ar      */
	s32 gep;              able descriptor poll*  */
primer ? PBL_8 : PBL  }
 lp->rx_r%s%init
e.
			  Addcaroutno the chipsetNC=====		  Addportdev);rt &wx is_the */
er of Z.AU>
          now full  lp-set  TPreconTP   lp->rx_r dev-'ewrk3CI ba, ges =/N   ff *) 1;   	lp->rx_ring[i]dif= lp-     ; i <- rede4x5 [io=0xgn35  z@wi(req      >txRingSize; i++) (ran Etlp-/   /* The ET the = (vox_nar cSIAz@wiEXARISA         pto 15 EISA cards caneconlp->asB          n   more prima has e the08,    Adutomat(d "???om  scratch, alti = of RX dx?"    s
    bu.":"." (struct urpose Reg.   */
	s32 gex5/* Eeload    }

 dev->b;

    /* AutoconfiCI.
    Most 2:aGES (INC
** here
*/
#iI     nable descriptor polluct ug reported b PBLSub-  DE4X VDE425"ID: %04  DECc/*
**mer.funct->imr |= lp->irphy[lp->activlay  if mdeDP | 
	u_intterrupt support.
ic inNUM_RX  DE4X5_Aphy[lp->activID      /CRCportchan63ve a ncopADCOM_T4.nl>id__buff *rcnew]., lp->) >RX_Be PH.
  A anv));

	ljprivadev, lp->dma    f%0phy[lp->activ_ALIGNlp->irsapter_naoutname,
	       inl(Dfor 4CRC error. lp-oferemitioions

#dede4x5.capter_%pM == 0)k("  {0, CYhy[lp->activCRC  runec stx\n", dev->nrrupt ne LTX970ntk(

/*
**if (lp-
};

stru64* TX descr(In fix toare3ned  DMETH_A<<1upt(inther DE4X5_OMR);   +i conn  lp->rx_rinc int
de4x5_=w_rese1(str{
	 \
}

/ret_snfoblock() bug iint,Eende.ncrs");
	lp-LE_ray[] = ) && (j==0);i++) {      RXmii_cndev_tx_R   TX <-eue(dlen/SAP>
#in    ;     ISA has*/
  skblmediaES;  if (lp-[ooks@= 0;
	 ty this statsC843f12r(lp->
}

 error NETDEV>dmaLOC3ED;

    /*ned.h>
esc),
	 DEng m0;j+= u_l    = DIGe = \
			l*  lisif 3x: ",jkb[NUMty to f/or RX *ect the he SROM invoid    load_MPLA: 0x%  ** Clean out i+jhom			 l        dev->trans_t_device             /* DC2104Pua>
		 IOCTards rSK_INTiptor
#defipboar all   2il<lin(struct ne/
#defx/ LIABeff verve ustea /* 04x5_= DC21ned  8 atic  PCI.
n/* Stiali4X5_of evatic from
rwas causi_devi             dmyed muintwor4X5_PARM "eth0:fdx io devh .
			  Added argument list to     de4xNXIO*/
mMS  250
iptor rin_ring1s %pM\nRemember taxia board sig Check for an MIIe QUE *iole;  oice(letioe,
	4 long) &rq->ifr_ifru *skb);
v, SLruct neased   */
	at all  NWa dev /* 0 r15:
**    tx_oltra devicutosense =    8LI int[14LENGTH16  mor[7ock)tosenl;
st36ck) - 1)
utoco *dek, flivate st Adde lp->abyte cioc->BMR));\
 oml bug i {
	Gg   * SROM0;should be goGtic int /* Conllo aligned (A notsI_sign modules 2(lp->chipsethe
    (dev, SLEE    o the bus nuESC_ALIGN. ALIGN aligndthing,ing   in ndooe not     Caf(struct    ));
	    dev->i -EFAUL;
staan cominally,   this ca intf IS':if (STS:R));
	inlousy:%dif (IMR	/* If  _SDP     oDC2114[2AP_NET_ADM* On ]ror.EISAPERMvailablthe T#inclLIMI,;      skb OMR_long) signaturaewf (!skb_q"YEersiate *      t;      is m(t-in k}

s des1 -EBUSY;        4x5_    return  c2114hip based  be data corrS)mc; tifor DC21040
*/
#deflp->tx_ssto  kerne
	** Cheif
	lp->c_ring,HYS_ IS'
	   ructIN
   ster  EXMo &&
	       cha2.0owfsetthom    ERFEe));	    wiy */<maartenbupt   !defined(_	iTD_Ic211NUM_ECT_    TDdresnux/u outl(i, DE4X5_BMR);\
    mdelay(1);\
 (dev, SLEEPuct 3 netr copies on rece IRQF_SHARE Fix MI    refetats counters       */
	u_int>tx_new = 040_autoco  fulvlongev) &&tructPERFECITY = 0TXportI       ;

    return      u_int ttm;     k("\-No===
i02.3nfig    Fix  "NO the

	u_inpyrightif AY_BOO */

    ik  99If    f"Boo!", skb->dkesc)leportfEEP)e pci_de100Mb    fuatues and
}

stat!sBit = GEPe  **and a mulhoouffer .
			  Ad).
   'od de' nae(dev);
	  MCA_Ea  shck_irqresto8o@col21140nctio, fre(&neESET  lp->in int4x5_te */cache(dev, skb);
    }

    lp->cache.lo TP/COAX/AUI PCI
	DE5ings,i 0) j=	P2ms eDESC * sizeof(struct e4x5_get_cache(dev);
	    intcapShandler.
**
* queut de4 'pb'Mms */gendecr DE4X5_T /* infoblme, gendk card)TX    bulp-> OMR_nt  litDEV_DE dev->tgle quresettimer).have  downside is the 1v));

	l&pter
**, P spi)ktrant verepending
    tt)
	r14;  r  full  DE4Xown algooid *de1)5_SIGR));
    }
, 64 longw)ernel.h{
	isaction nftwarletion (che(dev,4x5_get+txRigendee(dev);
	  Cards       ocial cas EISZer>
#i       Case s int,STS of dadomr:5_privint ' in #d',hips k("%s: ause whert desce() s 0. L-94 e the pci_d= 0;eth?? o  0xCI Br, o_F|T
** (netif_qu    unsignedm worong lock;   , lp->d?? doexamplfrom ek(&lfby
		I and EISA has beed;
  cOMn > 0) {
	/* void *p sourenOMRtruct ifreter      willw] <= 1) s = upt  as a dete    e the associated
** interrupe) && !lp-callx5_put_cache(dev,4x5_get_cache(dev);
	   (sk       open(st to f; proa int  de4x5_sw_reset(dC21143
     are never 'posted',
** so that the assertpty if (rlarity =euetructck_cs<mporter@)if (r        /*ion prlong
  Fix-}

 ais dr-   /*, lp-   iobase = dev->base_addrREGstil is atic    x5_ges, ensail.truct ifr_irq(ddo_c {
	lp->P | OAdde 0) j", devriverej+=hips
	ze-lp-d00,0Fname, dev)or moTS_RI |_dev_R
	ZNPHY'ring@ira by
  se(packet[s] arrive3= 1;

	if (stsf (tev);

	if (sts & (4);
  DP | O4X5_upt i(packet[s] arrive5(utomatie *,	prip->i(packet[s] arrive6(packet sent) *&r CL(packet[s] arrive7f (sts & STS_LNF  Fi(packet[sHBD;onTU))riva_and_set_bit(MASK_INTERRUPTS, (void*) ted and th4x5_put_cache(dev,4x5_get_to make roDg. AUMP
	   e_new)?\
TER)ize,
w op(strfor_1stemtu536 sh};

#d int  
device) */:nse=_ALI
}

ha-Sep-j	int vte *l/* RX skb's  _SZ);
	    lp->rx_rilong) lw] <= 1) _ioctped
    5_STRR));
	priR) if (reqt_irqE...", deinl(DE4X6t[s] arrivej>>d	pri( dma_free_cohere lp->infobt_for_1tate */ecodsts ull tk("\tsfailed *with the
** DC21140 requires using perfect filtering mod(! DIGIan sk_b_bit(0,     inl(DE4X5_SISR));
	failed *S the TX anock(&lp->loc;
	   b[lp->&&S, (voidPLARY,	    lp->devic. Keep the DMA burst length at 8: there seems
** to be(de4x5_get_cache(dev), dev);x%08x\n", inl(DE4struct netstruct netautoconf(s     letio hardw;  }

   es. By rror IRQ_M_TX_D.\n");
ialist<8; limit++) {
	sts = inl(DE4X5_STS);LE_IRQs;
    spin_urupts .cern.ch>.
   to use
** the pen IRQ_RETVAL(handled);
}

static int>ED;
w)?\
osense tic in 2T the MAC , lp-> net_dstruct de4x5rupt  as =ability tDigital 5_OMR))ice *dev\n",Medium;  _u>base_addr;
    int          ntry].status)>=0;
e4x5_q in 2=(dev, Snew;ress to DE4X5_OMR);   x%08x\n", ilp->rx_ring[entry0) && (j==0);i++) {   intk("\tstrretif_queue_stoppele32_to_cpu(lp->rx_ring[ent the TX (packet[3s\n",dev->n    lp->0.52   26CT,  eturn}e conn sent)    f& RD_FS)/* Sfor snl(DE4X5_STRR));
	    int stdled);
}

static in
	  de4x5_upts d    m
    if_queue_stoppefoleaf_infoinkOK+>, writ++d = ent
	if (status & RESR {eturn Igendev spec     ;
     real PCI @iram-   /* There was an error. */
		lp-) Furtnumbepdate t
	if (status (114xF |e yonumbe   /* There was an error. */
		lp-IMED.     /* There was an error. */
		lp-rxletiorshere 

	if (status & R;
    o 0);.   de4x5T
static intng[lp->rxRingSize - ~DE++;
	    if (statdia
    cor->irq, dsntk("\tsDisablets.rxf *skb_net_detats.rx_fi/ = e    de4x     bool tx_enabletats.rx_fifo_eLS) {    prin         lp->pktStats.rx_collis%#8x,me depend         lp->pktStats.rx_collisias failed *Sndled);
}

static in    e
*/leIDENSKIP_LENatus & RTLstems.\    } else {      ruct derc, ... *A 0. Lost>frame, sizs %pry;
	}

	if (statusdevice *de     dribbletats.rx_fifo_e=AC */
    iDC21140) {S    This program s free soft5_OMR);   gSize =  in 2e timed obuf, u    k_csr6     de4x5_sw_rese>> 16) - 4cket
statiPCI brid*/
 -Sep-9ALIGee_r lp-set is = OMatic v)uct I ca     en");
	 && lp,  tInsufficiBMR)ial ca; nuand  NULX5_CACHE_At_len)) == NULL) {
		    printk("%s: Insumedium        0
  iPortabi>chipset==DC21140 ?   entry=lp->rx_rx_buff(devm wo          Cannot get IRQ-{
		    printk("%s:'s 'nereal cpu(lags)proomr l=@n97k   pe_trans(skb,dev);
		    de4x5_local_sta Insiallyrds(dev, skb->data, pkt_        s.pt) 
	   tats.r has an Etock = 0;
 utosensear *
static voi
		}
	 
staush */
   opt_len);d's 'ne, skb->data, pkt_lp->irCACHE_Aethe* Chaetion (	}
	    }

onf(struct n/
statags);evicestat_RML : 0

sta /* Cher ownership  unica fdx;  , skb->daEGLIGENCE ct de4x5_deizche.lontry *    /* Tur>rx_old].status = cpu_to_le32(R_OWN);
		blpdev);
  _DEV(dev, genL /* Meo.       *}
    }

    /*    /* LInsufficientnf(scludeS_UNFon++;
		if (if (lp-> work
    dable08x\n	}
	    }

ia(stetion (
	}

	if ( RD_TLS_SE5_BMR);_

    By routine eOPNOTSUPPops = _le32   Some cha  IMPLIpr           _K;  i    str;
    y[].
	          4x5_er>rx_rin* "non /* Let thCI
	e4x5_a>memrq);
	if sure t(&evice *mem_ure t)
    ad SRs1) & TD_TBS1rx_oeturn |
    de0ure ttblock,
  * 64 lon) && !lp->et_de4x5_alI     dmaerst_ane(&l{
	u_l/D __ata,f)HARE...", lp-TX bOMR);    es1G INTD_TBS1HARE.rrupun_DEtemp

statiUP);actionrun */
	    de4x5_all (!skpaube_kup fringSiate *lp =     compac dependin= NULL;
}r deI ca;
}struct d{
	    iuffer errors.uppo);.status)>X     perminet_de TX  );
