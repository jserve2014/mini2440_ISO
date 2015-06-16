/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2009 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/netdevice.h>
#include <linux/ipv6.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>
#include <linux/net_tstamp.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/pci.h>
#include <linux/pci-aspm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/aer.h>
#ifdef CONFIG_IGB_DCA
#include <linux/dca.h>
#endif
#include "igb.h"

#define DRV_VERSION "1.3.16-k2"
char igb_driver_name[] = "igb";
char igb_driver_version[] = DRV_VERSION;
static const char igb_driver_string[] =
				"Intel(R) Gigabit Ethernet Network Driver";
static const char igb_copyright[] = "Copyright (c) 2007-2009 Intel Corporation.";

static const struct e1000_info *igb_info_tbl[] = {
	[board_82575] = &e1000_82575_info,
};

static struct pci_device_id igb_pci_tbl[] = {
	{ PCI_VDEVICE(INTEL, E1000_DEV_ID_82576), board_82575 },
	{ PCI_VDEVICE(INTEL, E1000_DEV_ID_82576_NS), board_82575 },
	{ PCI_VDEVICE(INTEL, E1000_DEV_ID_82576_FIBER), board_82575 },
	{ PCI_VDEVICE(INTEL, E1000_DEV_ID_82576_SERDES), board_82575 },
	{ PCI_VDEVICE(INTEL, E1000_DEV_ID_82576_SERDES_QUAD), board_82575 },
	{ PCI_VDEVICE(INTEL, E1000_DEV_ID_82576_QUAD_COPPER), board_82575 },
	{ PCI_VDEVICE(INTEL, E1000_DEV_ID_82575EB_COPPER), board_82575 },
	{ PCI_VDEVICE(INTEL, E1000_DEV_ID_82575EB_FIBER_SERDES), board_82575 },
	{ PCI_VDEVICE(INTEL, E1000_DEV_ID_82575GB_QUAD_COPPER), board_82575 },
	/* required last entry */
	{0, }
};

MODULE_DEVICE_TABLE(pci, igb_pci_tbl);

void igb_reset(struct igb_adapter *);
static int igb_setup_all_tx_resources(struct igb_adapter *);
static int igb_setup_all_rx_resources(struct igb_adapter *);
static void igb_free_all_tx_resources(struct igb_adapter *);
static void igb_free_all_rx_resources(struct igb_adapter *);
void igb_update_stats(struct igb_adapter *);
static int igb_probe(struct pci_dev *, const struct pci_device_id *);
static void __devexit igb_remove(struct pci_dev *pdev);
static int igb_sw_init(struct igb_adapter *);
static int igb_open(struct net_device *);
static int igb_close(struct net_device *);
static void igb_configure_tx(struct igb_adapter *);
static void igb_configure_rx(struct igb_adapter *);
static void igb_setup_rctl(struct igb_adapter *);
static void igb_clean_all_tx_rings(struct igb_adapter *);
static void igb_clean_all_rx_rings(struct igb_adapter *);
static void igb_clean_tx_ring(struct igb_ring *);
static void igb_clean_rx_ring(struct igb_ring *);
static void igb_set_rx_mode(struct net_device *);
static void igb_update_phy_info(unsigned long);
static void igb_watchdog(unsigned long);
static void igb_watchdog_task(struct work_struct *);
static netdev_tx_t igb_xmit_frame_ring_adv(struct sk_buff *,
					   struct net_device *,
					   struct igb_ring *);
static netdev_tx_t igb_xmit_frame_adv(struct sk_buff *skb,
				      struct net_device *);
static struct net_device_stats *igb_get_stats(struct net_device *);
static int igb_change_mtu(struct net_device *, int);
static int igb_set_mac(struct net_device *, void *);
static irqreturn_t igb_intr(int irq, void *);
static irqreturn_t igb_intr_msi(int irq, void *);
static irqreturn_t igb_msix_other(int irq, void *);
static irqreturn_t igb_msix_rx(int irq, void *);
static irqreturn_t igb_msix_tx(int irq, void *);
#ifdef CONFIG_IGB_DCA
static void igb_update_rx_dca(struct igb_ring *);
static void igb_update_tx_dca(struct igb_ring *);
static void igb_setup_dca(struct igb_adapter *);
#endif /* CONFIG_IGB_DCA */
static bool igb_clean_tx_irq(struct igb_ring *);
static int igb_poll(struct napi_struct *, int);
static bool igb_clean_rx_irq_adv(struct igb_ring *, int *, int);
static void igb_alloc_rx_buffers_adv(struct igb_ring *, int);
static int igb_ioctl(struct net_device *, struct ifreq *, int cmd);
static void igb_tx_timeout(struct net_device *);
static void igb_reset_task(struct work_struct *);
static void igb_vlan_rx_register(struct net_device *, struct vlan_group *);
static void igb_vlan_rx_add_vid(struct net_device *, u16);
static void igb_vlan_rx_kill_vid(struct net_device *, u16);
static void igb_restore_vlan(struct igb_adapter *);
static void igb_ping_all_vfs(struct igb_adapter *);
static void igb_msg_task(struct igb_adapter *);
static int igb_rcv_msg_from_vf(struct igb_adapter *, u32);
static inline void igb_set_rah_pool(struct e1000_hw *, int , int);
static void igb_vmm_control(struct igb_adapter *);
static int igb_set_vf_mac(struct igb_adapter *adapter, int, unsigned char *);
static void igb_restore_vf_multicasts(struct igb_adapter *adapter);

static inline void igb_set_vmolr(struct e1000_hw *hw, int vfn)
{
	u32 reg_data;

	reg_data = rd32(E1000_VMOLR(vfn));
	reg_data |= E1000_VMOLR_BAM |	 /* Accept broadcast */
	            E1000_VMOLR_ROPE |   /* Accept packets matched in UTA */
	            E1000_VMOLR_ROMPE |  /* Accept packets matched in MTA */
	            E1000_VMOLR_AUPE |   /* Accept untagged packets */
	            E1000_VMOLR_STRVLAN; /* Strip vlan tags */
	wr32(E1000_VMOLR(vfn), reg_data);
}

static inline int igb_set_vf_rlpml(struct igb_adapter *adapter, int size,
                                 int vfn)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 vmolr;

	/* if it isn't the PF check to see if VFs are enabled and
	 * increase the size to support vlan tags */
	if (vfn < adapter->vfs_allocated_count &&
	    adapter->vf_data[vfn].vlans_enabled)
		size += VLAN_TAG_SIZE;

	vmolr = rd32(E1000_VMOLR(vfn));
	vmolr &= ~E1000_VMOLR_RLPML_MASK;
	vmolr |= size | E1000_VMOLR_LPE;
	wr32(E1000_VMOLR(vfn), vmolr);

	return 0;
}

static inline void igb_set_rah_pool(struct e1000_hw *hw, int pool, int entry)
{
	u32 reg_data;

	reg_data = rd32(E1000_RAH(entry));
	reg_data &= ~E1000_RAH_POOL_MASK;
	reg_data |= E1000_RAH_POOL_1 << pool;;
	wr32(E1000_RAH(entry), reg_data);
}

#ifdef CONFIG_PM
static int igb_suspend(struct pci_dev *, pm_message_t);
static int igb_resume(struct pci_dev *);
#endif
static void igb_shutdown(struct pci_dev *);
#ifdef CONFIG_IGB_DCA
static int igb_notify_dca(struct notifier_block *, unsigned long, void *);
static struct notifier_block dca_notifier = {
	.notifier_call	= igb_notify_dca,
	.next		= NULL,
	.priority	= 0
};
#endif
#ifdef CONFIG_NET_POLL_CONTROLLER
/* for netdump / net console */
static void igb_netpoll(struct net_device *);
#endif
#ifdef CONFIG_PCI_IOV
static unsigned int max_vfs = 0;
module_param(max_vfs, uint, 0);
MODULE_PARM_DESC(max_vfs, "Maximum number of virtual functions to allocate "
                 "per physical function");
#endif /* CONFIG_PCI_IOV */

static pci_ers_result_t igb_io_error_detected(struct pci_dev *,
		     pci_channel_state_t);
static pci_ers_result_t igb_io_slot_reset(struct pci_dev *);
static void igb_io_resume(struct pci_dev *);

static struct pci_error_handlers igb_err_handler = {
	.error_detected = igb_io_error_detected,
	.slot_reset = igb_io_slot_reset,
	.resume = igb_io_resume,
};


static struct pci_driver igb_driver = {
	.name     = igb_driver_name,
	.id_table = igb_pci_tbl,
	.probe    = igb_probe,
	.remove   = __devexit_p(igb_remove),
#ifdef CONFIG_PM
	/* Power Managment Hooks */
	.suspend  = igb_suspend,
	.resume   = igb_resume,
#endif
	.shutdown = igb_shutdown,
	.err_handler = &igb_err_handler
};

static int global_quad_port_a; /* global quad port a indication */

MODULE_AUTHOR("Intel Corporation, <e1000-devel@lists.sourceforge.net>");
MODULE_DESCRIPTION("Intel(R) Gigabit Ethernet Network Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

/**
 * Scale the NIC clock cycle by a large factor so that
 * relatively small clock corrections can be added or
 * substracted at each clock tick. The drawbacks of a
 * large factor are a) that the clock register overflows
 * more quickly (not such a big deal) and b) that the
 * increment per tick has to fit into 24 bits.
 *
 * Note that
 *   TIMINCA = IGB_TSYNC_CYCLE_TIME_IN_NANOSECONDS *
 *             IGB_TSYNC_SCALE
 *   TIMINCA += TIMINCA * adjustment [ppm] / 1e9
 *
 * The base scale factor is intentionally a power of two
 * so that the division in %struct timecounter can be done with
 * a shift.
 */
#define IGB_TSYNC_SHIFT (19)
#define IGB_TSYNC_SCALE (1<<IGB_TSYNC_SHIFT)

/**
 * The duration of one clock cycle of the NIC.
 *
 * @todo This hard-coded value is part of the specification and might change
 * in future hardware revisions. Add revision check.
 */
#define IGB_TSYNC_CYCLE_TIME_IN_NANOSECONDS 16

#if (IGB_TSYNC_SCALE * IGB_TSYNC_CYCLE_TIME_IN_NANOSECONDS) >= (1<<24)
# error IGB_TSYNC_SCALE and/or IGB_TSYNC_CYCLE_TIME_IN_NANOSECONDS are too large to fit into TIMINCA
#endif

/**
 * igb_read_clock - read raw cycle counter (to be used by time counter)
 */
static cycle_t igb_read_clock(const struct cyclecounter *tc)
{
	struct igb_adapter *adapter =
		container_of(tc, struct igb_adapter, cycles);
	struct e1000_hw *hw = &adapter->hw;
	u64 stamp;

	stamp =  rd32(E1000_SYSTIML);
	stamp |= (u64)rd32(E1000_SYSTIMH) << 32ULL;

	return stamp;
}

#ifdef DEBUG
/**
 * igb_get_hw_dev_name - return device name string
 * used by hardware layer to print debugging information
 **/
char *igb_get_hw_dev_name(struct e1000_hw *hw)
{
	struct igb_adapter *adapter = hw->back;
	return adapter->netdev->name;
}

/**
 * igb_get_time_str - format current NIC and system time as string
 */
static char *igb_get_time_str(struct igb_adapter *adapter,
			      char buffer[160])
{
	cycle_t hw = adapter->cycles.read(&adapter->cycles);
	struct timespec nic = ns_to_timespec(timecounter_read(&adapter->clock));
	struct timespec sys;
	struct timespec delta;
	getnstimeofday(&sys);

	delta = timespec_sub(nic, sys);

	sprintf(buffer,
		"HW %llu, NIC %ld.%09lus, SYS %ld.%09lus, NIC-SYS %lds + %09luns",
		hw,
		(long)nic.tv_sec, nic.tv_nsec,
		(long)sys.tv_sec, sys.tv_nsec,
		(long)delta.tv_sec, delta.tv_nsec);

	return buffer;
}
#endif

/**
 * igb_desc_unused - calculate if we have unused descriptors
 **/
static int igb_desc_unused(struct igb_ring *ring)
{
	if (ring->next_to_clean > ring->next_to_use)
		return ring->next_to_clean - ring->next_to_use - 1;

	return ring->count + ring->next_to_clean - ring->next_to_use - 1;
}

/**
 * igb_init_module - Driver Registration Routine
 *
 * igb_init_module is the first routine called when the driver is
 * loaded. All it does is register with the PCI subsystem.
 **/
static int __init igb_init_module(void)
{
	int ret;
	printk(KERN_INFO "%s - version %s\n",
	       igb_driver_string, igb_driver_version);

	printk(KERN_INFO "%s\n", igb_copyright);

	global_quad_port_a = 0;

#ifdef CONFIG_IGB_DCA
	dca_register_notify(&dca_notifier);
#endif

	ret = pci_register_driver(&igb_driver);
	return ret;
}

module_init(igb_init_module);

/**
 * igb_exit_module - Driver Exit Cleanup Routine
 *
 * igb_exit_module is called just before the driver is removed
 * from memory.
 **/
static void __exit igb_exit_module(void)
{
#ifdef CONFIG_IGB_DCA
	dca_unregister_notify(&dca_notifier);
#endif
	pci_unregister_driver(&igb_driver);
}

module_exit(igb_exit_module);

#define Q_IDX_82576(i) (((i & 0x1) << 3) + (i >> 1))
/**
 * igb_cache_ring_register - Descriptor ring to register mapping
 * @adapter: board private structure to initialize
 *
 * Once we know the feature-set enabled for the device, we'll cache
 * the register offset the descriptor ring is assigned to.
 **/
static void igb_cache_ring_register(struct igb_adapter *adapter)
{
	int i;
	unsigned int rbase_offset = adapter->vfs_allocated_count;

	switch (adapter->hw.mac.type) {
	case e1000_82576:
		/* The queues are allocated for virtualization such that VF 0
		 * is allocated queues 0 and 8, VF 1 queues 1 and 9, etc.
		 * In order to avoid collision we start at the first free queue
		 * and continue consuming queues in the same sequence
		 */
		for (i = 0; i < adapter->num_rx_queues; i++)
			adapter->rx_ring[i].reg_idx = rbase_offset +
			                              Q_IDX_82576(i);
		for (i = 0; i < adapter->num_tx_queues; i++)
			adapter->tx_ring[i].reg_idx = rbase_offset +
			                              Q_IDX_82576(i);
		break;
	case e1000_82575:
	default:
		for (i = 0; i < adapter->num_rx_queues; i++)
			adapter->rx_ring[i].reg_idx = i;
		for (i = 0; i < adapter->num_tx_queues; i++)
			adapter->tx_ring[i].reg_idx = i;
		break;
	}
}

/**
 * igb_alloc_queues - Allocate memory for all rings
 * @adapter: board private structure to initialize
 *
 * We allocate one ring per queue at run-time since we don't know the
 * number of queues at compile-time.
 **/
static int igb_alloc_queues(struct igb_adapter *adapter)
{
	int i;

	adapter->tx_ring = kcalloc(adapter->num_tx_queues,
				   sizeof(struct igb_ring), GFP_KERNEL);
	if (!adapter->tx_ring)
		return -ENOMEM;

	adapter->rx_ring = kcalloc(adapter->num_rx_queues,
				   sizeof(struct igb_ring), GFP_KERNEL);
	if (!adapter->rx_ring) {
		kfree(adapter->tx_ring);
		return -ENOMEM;
	}

	adapter->rx_ring->buddy = adapter->tx_ring;

	for (i = 0; i < adapter->num_tx_queues; i++) {
		struct igb_ring *ring = &(adapter->tx_ring[i]);
		ring->count = adapter->tx_ring_count;
		ring->adapter = adapter;
		ring->queue_index = i;
	}
	for (i = 0; i < adapter->num_rx_queues; i++) {
		struct igb_ring *ring = &(adapter->rx_ring[i]);
		ring->count = adapter->rx_ring_count;
		ring->adapter = adapter;
		ring->queue_index = i;
		ring->itr_register = E1000_ITR;

		/* set a default napi handler for each rx_ring */
		netif_napi_add(adapter->netdev, &ring->napi, igb_poll, 64);
	}

	igb_cache_ring_register(adapter);
	return 0;
}

static void igb_free_queues(struct igb_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_rx_queues; i++)
		netif_napi_del(&adapter->rx_ring[i].napi);

	adapter->num_rx_queues = 0;
	adapter->num_tx_queues = 0;

	kfree(adapter->tx_ring);
	kfree(adapter->rx_ring);
}

#define IGB_N0_QUEUE -1
static void igb_assign_vector(struct igb_adapter *adapter, int rx_queue,
			      int tx_queue, int msix_vector)
{
	u32 msixbm = 0;
	struct e1000_hw *hw = &adapter->hw;
	u32 ivar, index;

	switch (hw->mac.type) {
	case e1000_82575:
		/* The 82575 assigns vectors using a bitmask, which matches the
		   bitmask for the EICR/EIMS/EIMC registers.  To assign one
		   or more queues to a vector, we write the appropriate bits
		   into the MSIXBM register for that vector. */
		if (rx_queue > IGB_N0_QUEUE) {
			msixbm = E1000_EICR_RX_QUEUE0 << rx_queue;
			adapter->rx_ring[rx_queue].eims_value = msixbm;
		}
		if (tx_queue > IGB_N0_QUEUE) {
			msixbm |= E1000_EICR_TX_QUEUE0 << tx_queue;
			adapter->tx_ring[tx_queue].eims_value =
				  E1000_EICR_TX_QUEUE0 << tx_queue;
		}
		array_wr32(E1000_MSIXBM(0), msix_vector, msixbm);
		break;
	case e1000_82576:
		/* 82576 uses a table-based method for assigning vectors.
		   Each queue has a single entry in the table to which we write
		   a vector number along with a "valid" bit.  Sadly, the layout
		   of the table is somewhat counterintuitive. */
		if (rx_queue > IGB_N0_QUEUE) {
			index = (rx_queue >> 1) + adapter->vfs_allocated_count;
			ivar = array_rd32(E1000_IVAR0, index);
			if (rx_queue & 0x1) {
				/* vector goes into third byte of register */
				ivar = ivar & 0xFF00FFFF;
				ivar |= (msix_vector | E1000_IVAR_VALID) << 16;
			} else {
				/* vector goes into low byte of register */
				ivar = ivar & 0xFFFFFF00;
				ivar |= msix_vector | E1000_IVAR_VALID;
			}
			adapter->rx_ring[rx_queue].eims_value= 1 << msix_vector;
			array_wr32(E1000_IVAR0, index, ivar);
		}
		if (tx_queue > IGB_N0_QUEUE) {
			index = (tx_queue >> 1) + adapter->vfs_allocated_count;
			ivar = array_rd32(E1000_IVAR0, index);
			if (tx_queue & 0x1) {
				/* vector goes into high byte of register */
				ivar = ivar & 0x00FFFFFF;
				ivar |= (msix_vector | E1000_IVAR_VALID) << 24;
			} else {
				/* vector goes into second byte of register */
				ivar = ivar & 0xFFFF00FF;
				ivar |= (msix_vector | E1000_IVAR_VALID) << 8;
			}
			adapter->tx_ring[tx_queue].eims_value= 1 << msix_vector;
			array_wr32(E1000_IVAR0, index, ivar);
		}
		break;
	default:
		BUG();
		break;
	}
}

/**
 * igb_configure_msix - Configure MSI-X hardware
 *
 * igb_configure_msix sets up the hardware to properly
 * generate MSI-X interrupts.
 **/
static void igb_configure_msix(struct igb_adapter *adapter)
{
	u32 tmp;
	int i, vector = 0;
	struct e1000_hw *hw = &adapter->hw;

	adapter->eims_enable_mask = 0;
	if (hw->mac.type == e1000_82576)
		/* Turn on MSI-X capability first, or our settings
		 * won't stick.  And it will take days to debug. */
		wr32(E1000_GPIE, E1000_GPIE_MSIX_MODE |
				   E1000_GPIE_PBA | E1000_GPIE_EIAME |
 				   E1000_GPIE_NSICR);

	for (i = 0; i < adapter->num_tx_queues; i++) {
		struct igb_ring *tx_ring = &adapter->tx_ring[i];
		igb_assign_vector(adapter, IGB_N0_QUEUE, i, vector++);
		adapter->eims_enable_mask |= tx_ring->eims_value;
		if (tx_ring->itr_val)
			writel(tx_ring->itr_val,
			       hw->hw_addr + tx_ring->itr_register);
		else
			writel(1, hw->hw_addr + tx_ring->itr_register);
	}

	for (i = 0; i < adapter->num_rx_queues; i++) {
		struct igb_ring *rx_ring = &adapter->rx_ring[i];
		rx_ring->buddy = NULL;
		igb_assign_vector(adapter, i, IGB_N0_QUEUE, vector++);
		adapter->eims_enable_mask |= rx_ring->eims_value;
		if (rx_ring->itr_val)
			writel(rx_ring->itr_val,
			       hw->hw_addr + rx_ring->itr_register);
		else
			writel(1, hw->hw_addr + rx_ring->itr_register);
	}


	/* set vector for other causes, i.e. link changes */
	switch (hw->mac.type) {
	case e1000_82575:
		array_wr32(E1000_MSIXBM(0), vector++,
				      E1000_EIMS_OTHER);

		tmp = rd32(E1000_CTRL_EXT);
		/* enable MSI-X PBA support*/
		tmp |= E1000_CTRL_EXT_PBA_CLR;

		/* Auto-Mask interrupts upon ICR read. */
		tmp |= E1000_CTRL_EXT_EIAME;
		tmp |= E1000_CTRL_EXT_IRCA;

		wr32(E1000_CTRL_EXT, tmp);
		adapter->eims_enable_mask |= E1000_EIMS_OTHER;
		adapter->eims_other = E1000_EIMS_OTHER;

		break;

	case e1000_82576:
		tmp = (vector++ | E1000_IVAR_VALID) << 8;
		wr32(E1000_IVAR_MISC, tmp);

		adapter->eims_enable_mask = (1 << (vector)) - 1;
		adapter->eims_other = 1 << (vector - 1);
		break;
	default:
		/* do nothing, since nothing else supports MSI-X */
		break;
	} /* switch (hw->mac.type) */
	wrfl();
}

/**
 * igb_request_msix - Initialize MSI-X interrupts
 *
 * igb_request_msix allocates MSI-X vectors and requests interrupts from the
 * kernel.
 **/
static int igb_request_msix(struct igb_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	int i, err = 0, vector = 0;

	vector = 0;

	for (i = 0; i < adapter->num_tx_queues; i++) {
		struct igb_ring *ring = &(adapter->tx_ring[i]);
		sprintf(ring->name, "%s-tx-%d", netdev->name, i);
		err = request_irq(adapter->msix_entries[vector].vector,
				  &igb_msix_tx, 0, ring->name,
				  &(adapter->tx_ring[i]));
		if (err)
			goto out;
		ring->itr_register = E1000_EITR(0) + (vector << 2);
		ring->itr_val = 976; /* ~4000 ints/sec */
		vector++;
	}
	for (i = 0; i < adapter->num_rx_queues; i++) {
		struct igb_ring *ring = &(adapter->rx_ring[i]);
		if (strlen(netdev->name) < (IFNAMSIZ - 5))
			sprintf(ring->name, "%s-rx-%d", netdev->name, i);
		else
			memcpy(ring->name, netdev->name, IFNAMSIZ);
		err = request_irq(adapter->msix_entries[vector].vector,
				  &igb_msix_rx, 0, ring->name,
				  &(adapter->rx_ring[i]));
		if (err)
			goto out;
		ring->itr_register = E1000_EITR(0) + (vector << 2);
		ring->itr_val = adapter->itr;
		vector++;
	}

	err = request_irq(adapter->msix_entries[vector].vector,
			  &igb_msix_other, 0, netdev->name, netdev);
	if (err)
		goto out;

	igb_configure_msix(adapter);
	return 0;
out:
	return err;
}

static void igb_reset_interrupt_capability(struct igb_adapter *adapter)
{
	if (adapter->msix_entries) {
		pci_disable_msix(adapter->pdev);
		kfree(adapter->msix_entries);
		adapter->msix_entries = NULL;
	} else if (adapter->flags & IGB_FLAG_HAS_MSI)
		pci_disable_msi(adapter->pdev);
	return;
}


/**
 * igb_set_interrupt_capability - set MSI or MSI-X if supported
 *
 * Attempt to configure interrupts using the best available
 * capabilities of the hardware and kernel.
 **/
static void igb_set_interrupt_capability(struct igb_adapter *adapter)
{
	int err;
	int numvecs, i;

	/* Number of supported queues. */
	/* Having more queues than CPUs doesn't make sense. */
	adapter->num_rx_queues = min_t(u32, IGB_MAX_RX_QUEUES, num_online_cpus());
	adapter->num_tx_queues = min_t(u32, IGB_MAX_TX_QUEUES, num_online_cpus());

	numvecs = adapter->num_tx_queues + adapter->num_rx_queues + 1;
	adapter->msix_entries = kcalloc(numvecs, sizeof(struct msix_entry),
					GFP_KERNEL);
	if (!adapter->msix_entries)
		goto msi_only;

	for (i = 0; i < numvecs; i++)
		adapter->msix_entries[i].entry = i;

	err = pci_enable_msix(adapter->pdev,
			      adapter->msix_entries,
			      numvecs);
	if (err == 0)
		goto out;

	igb_reset_interrupt_capability(adapter);

	/* If we can't do MSI-X, try MSI */
msi_only:
#ifdef CONFIG_PCI_IOV
	/* disable SR-IOV for non MSI-X configurations */
	if (adapter->vf_data) {
		struct e1000_hw *hw = &adapter->hw;
		/* disable iov and allow time for transactions to clear */
		pci_disable_sriov(adapter->pdev);
		msleep(500);

		kfree(adapter->vf_data);
		adapter->vf_data = NULL;
		wr32(E1000_IOVCTL, E1000_IOVCTL_REUSE_VFQ);
		msleep(100);
		dev_info(&adapter->pdev->dev, "IOV Disabled\n");
	}
#endif
	adapter->num_rx_queues = 1;
	adapter->num_tx_queues = 1;
	if (!pci_enable_msi(adapter->pdev))
		adapter->flags |= IGB_FLAG_HAS_MSI;
out:
	/* Notify the stack of the (possibly) reduced Tx Queue count. */
	adapter->netdev->real_num_tx_queues = adapter->num_tx_queues;
	return;
}

/**
 * igb_request_irq - initialize interrupts
 *
 * Attempts to configure interrupts using the best available
 * capabilities of the hardware and kernel.
 **/
static int igb_request_irq(struct igb_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	struct e1000_hw *hw = &adapter->hw;
	int err = 0;

	if (adapter->msix_entries) {
		err = igb_request_msix(adapter);
		if (!err)
			goto request_done;
		/* fall back to MSI */
		igb_reset_interrupt_capability(adapter);
		if (!pci_enable_msi(adapter->pdev))
			adapter->flags |= IGB_FLAG_HAS_MSI;
		igb_free_all_tx_resources(adapter);
		igb_free_all_rx_resources(adapter);
		adapter->num_rx_queues = 1;
		igb_alloc_queues(adapter);
	} else {
		switch (hw->mac.type) {
		case e1000_82575:
			wr32(E1000_MSIXBM(0),
			     (E1000_EICR_RX_QUEUE0 | E1000_EIMS_OTHER));
			break;
		case e1000_82576:
			wr32(E1000_IVAR0, E1000_IVAR_VALID);
			break;
		default:
			break;
		}
	}

	if (adapter->flags & IGB_FLAG_HAS_MSI) {
		err = request_irq(adapter->pdev->irq, &igb_intr_msi, 0,
				  netdev->name, netdev);
		if (!err)
			goto request_done;
		/* fall back to legacy interrupts */
		igb_reset_interrupt_capability(adapter);
		adapter->flags &= ~IGB_FLAG_HAS_MSI;
	}

	err = request_irq(adapter->pdev->irq, &igb_intr, IRQF_SHARED,
			  netdev->name, netdev);

	if (err)
		dev_err(&adapter->pdev->dev, "Error %d getting interrupt\n",
			err);

request_done:
	return err;
}

static void igb_free_irq(struct igb_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;

	if (adapter->msix_entries) {
		int vector = 0, i;

		for (i = 0; i < adapter->num_tx_queues; i++)
			free_irq(adapter->msix_entries[vector++].vector,
				&(adapter->tx_ring[i]));
		for (i = 0; i < adapter->num_rx_queues; i++)
			free_irq(adapter->msix_entries[vector++].vector,
				&(adapter->rx_ring[i]));

		free_irq(adapter->msix_entries[vector++].vector, netdev);
		return;
	}

	free_irq(adapter->pdev->irq, netdev);
}

/**
 * igb_irq_disable - Mask off interrupt generation on the NIC
 * @adapter: board private structure
 **/
static void igb_irq_disable(struct igb_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;

	if (adapter->msix_entries) {
		u32 regval = rd32(E1000_EIAM);
		wr32(E1000_EIAM, regval & ~adapter->eims_enable_mask);
		wr32(E1000_EIMC, adapter->eims_enable_mask);
		regval = rd32(E1000_EIAC);
		wr32(E1000_EIAC, regval & ~adapter->eims_enable_mask);
	}

	wr32(E1000_IAM, 0);
	wr32(E1000_IMC, ~0);
	wrfl();
	synchronize_irq(adapter->pdev->irq);
}

/**
 * igb_irq_enable - Enable default interrupt generation settings
 * @adapter: board private structure
 **/
static void igb_irq_enable(struct igb_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;

	if (adapter->msix_entries) {
		u32 regval = rd32(E1000_EIAC);
		wr32(E1000_EIAC, regval | adapter->eims_enable_mask);
		regval = rd32(E1000_EIAM);
		wr32(E1000_EIAM, regval | adapter->eims_enable_mask);
		wr32(E1000_EIMS, adapter->eims_enable_mask);
		if (adapter->vfs_allocated_count)
			wr32(E1000_MBVFIMR, 0xFF);
		wr32(E1000_IMS, (E1000_IMS_LSC | E1000_IMS_VMMB |
		                 E1000_IMS_DOUTSYNC));
	} else {
		wr32(E1000_IMS, IMS_ENABLE_MASK);
		wr32(E1000_IAM, IMS_ENABLE_MASK);
	}
}

static void igb_update_mng_vlan(struct igb_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	u16 vid = adapter->hw.mng_cookie.vlan_id;
	u16 old_vid = adapter->mng_vlan_id;
	if (adapter->vlgrp) {
		if (!vlan_group_get_device(adapter->vlgrp, vid)) {
			if (adapter->hw.mng_cookie.status &
				E1000_MNG_DHCP_COOKIE_STATUS_VLAN) {
				igb_vlan_rx_add_vid(netdev, vid);
				adapter->mng_vlan_id = vid;
			} else
				adapter->mng_vlan_id = IGB_MNG_VLAN_NONE;

			if ((old_vid != (u16)IGB_MNG_VLAN_NONE) &&
					(vid != old_vid) &&
			    !vlan_group_get_device(adapter->vlgrp, old_vid))
				igb_vlan_rx_kill_vid(netdev, old_vid);
		} else
			adapter->mng_vlan_id = vid;
	}
}

/**
 * igb_release_hw_control - release control of the h/w to f/w
 * @adapter: address of board private structure
 *
 * igb_release_hw_control resets CTRL_EXT:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that the
 * driver is no longer loaded.
 *
 **/
static void igb_release_hw_control(struct igb_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 ctrl_ext;

	/* Let firmware take over control of h/w */
	ctrl_ext = rd32(E1000_CTRL_EXT);
	wr32(E1000_CTRL_EXT,
			ctrl_ext & ~E1000_CTRL_EXT_DRV_LOAD);
}


/**
 * igb_get_hw_control - get control of the h/w from f/w
 * @adapter: address of board private structure
 *
 * igb_get_hw_control sets CTRL_EXT:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that
 * the driver is loaded.
 *
 **/
static void igb_get_hw_control(struct igb_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 ctrl_ext;

	/* Let firmware know the driver has taken over */
	ctrl_ext = rd32(E1000_CTRL_EXT);
	wr32(E1000_CTRL_EXT,
			ctrl_ext | E1000_CTRL_EXT_DRV_LOAD);
}

/**
 * igb_configure - configure the hardware for RX and TX
 * @adapter: private board structure
 **/
static void igb_configure(struct igb_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	int i;

	igb_get_hw_control(adapter);
	igb_set_rx_mode(netdev);

	igb_restore_vlan(adapter);

	igb_configure_tx(adapter);
	igb_setup_rctl(adapter);
	igb_configure_rx(adapter);

	igb_rx_fifo_flush_82575(&adapter->hw);

	/* call igb_desc_unused which always leaves
	 * at least 1 descriptor unused to make sure
	 * next_to_use != next_to_clean */
	for (i = 0; i < adapter->num_rx_queues; i++) {
		struct igb_ring *ring = &adapter->rx_ring[i];
		igb_alloc_rx_buffers_adv(ring, igb_desc_unused(ring));
	}


	adapter->tx_queue_len = netdev->tx_queue_len;
}


/**
 * igb_up - Open the interface and prepare it to handle traffic
 * @adapter: board private structure
 **/

int igb_up(struct igb_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	int i;

	/* hardware has been reset, we need to reload some things */
	igb_configure(adapter);

	clear_bit(__IGB_DOWN, &adapter->state);

	for (i = 0; i < adapter->num_rx_queues; i++)
		napi_enable(&adapter->rx_ring[i].napi);
	if (adapter->msix_entries)
		igb_configure_msix(adapter);

	igb_vmm_control(adapter);
	igb_set_rah_pool(hw, adapter->vfs_allocated_count, 0);
	igb_set_vmolr(hw, adapter->vfs_allocated_count);

	/* Clear any pending interrupts. */
	rd32(E1000_ICR);
	igb_irq_enable(adapter);

	netif_tx_start_all_queues(adapter->netdev);

	/* Fire a link change interrupt to start the watchdog. */
	wr32(E1000_ICS, E1000_ICS_LSC);
	return 0;
}

void igb_down(struct igb_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct net_device *netdev = adapter->netdev;
	u32 tctl, rctl;
	int i;

	/* signal that we're down so the interrupt handler does not
	 * reschedule our watchdog timer */
	set_bit(__IGB_DOWN, &adapter->state);

	/* disable receives in the hardware */
	rctl = rd32(E1000_RCTL);
	wr32(E1000_RCTL, rctl & ~E1000_RCTL_EN);
	/* flush and sleep below */

	netif_tx_stop_all_queues(netdev);

	/* disable transmits in the hardware */
	tctl = rd32(E1000_TCTL);
	tctl &= ~E1000_TCTL_EN;
	wr32(E1000_TCTL, tctl);
	/* flush both disables and wait for them to finish */
	wrfl();
	msleep(10);

	for (i = 0; i < adapter->num_rx_queues; i++)
		napi_disable(&adapter->rx_ring[i].napi);

	igb_irq_disable(adapter);

	del_timer_sync(&adapter->watchdog_timer);
	del_timer_sync(&adapter->phy_info_timer);

	netdev->tx_queue_len = adapter->tx_queue_len;
	netif_carrier_off(netdev);

	/* record the stats before reset*/
	igb_update_stats(adapter);

	adapter->link_speed = 0;
	adapter->link_duplex = 0;

	if (!pci_channel_offline(adapter->pdev))
		igb_reset(adapter);
	igb_clean_all_tx_rings(adapter);
	igb_clean_all_rx_rings(adapter);
#ifdef CONFIG_IGB_DCA

	/* since we reset the hardware DCA settings were cleared */
	igb_setup_dca(adapter);
#endif
}

void igb_reinit_locked(struct igb_adapter *adapter)
{
	WARN_ON(in_interrupt());
	while (test_and_set_bit(__IGB_RESETTING, &adapter->state))
		msleep(1);
	igb_down(adapter);
	igb_up(adapter);
	clear_bit(__IGB_RESETTING, &adapter->state);
}

void igb_reset(struct igb_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_mac_info *mac = &hw->mac;
	struct e1000_fc_info *fc = &hw->fc;
	u32 pba = 0, tx_space, min_tx_space, min_rx_space;
	u16 hwm;

	/* Repartition Pba for greater than 9k mtu
	 * To take effect CTRL.RST is required.
	 */
	switch (mac->type) {
	case e1000_82576:
		pba = E1000_PBA_64K;
		break;
	case e1000_82575:
	default:
		pba = E1000_PBA_34K;
		break;
	}

	if ((adapter->max_frame_size > ETH_FRAME_LEN + ETH_FCS_LEN) &&
	    (mac->type < e1000_82576)) {
		/* adjust PBA for jumbo frames */
		wr32(E1000_PBA, pba);

		/* To maintain wire speed transmits, the Tx FIFO should be
		 * large enough to accommodate two full transmit packets,
		 * rounded up to the next 1KB and expressed in KB.  Likewise,
		 * the Rx FIFO should be large enough to accommodate at least
		 * one full receive packet and is similarly rounded up and
		 * expressed in KB. */
		pba = rd32(E1000_PBA);
		/* upper 16 bits has Tx packet buffer allocation size in KB */
		tx_space = pba >> 16;
		/* lower 16 bits has Rx packet buffer allocation size in KB */
		pba &= 0xffff;
		/* the tx fifo also stores 16 bytes of information about the tx
		 * but don't include ethernet FCS because hardware appends it */
		min_tx_space = (adapter->max_frame_size +
				sizeof(union e1000_adv_tx_desc) -
				ETH_FCS_LEN) * 2;
		min_tx_space = ALIGN(min_tx_space, 1024);
		min_tx_space >>= 10;
		/* software strips receive CRC, so leave room for it */
		min_rx_space = adapter->max_frame_size;
		min_rx_space = ALIGN(min_rx_space, 1024);
		min_rx_space >>= 10;

		/* If current Tx allocation is less than the min Tx FIFO size,
		 * and the min Tx FIFO size is less than the current Rx FIFO
		 * allocation, take space away from current Rx allocation */
		if (tx_space < min_tx_space &&
		    ((min_tx_space - tx_space) < pba)) {
			pba = pba - (min_tx_space - tx_space);

			/* if short on rx space, rx wins and must trump tx
			 * adjustment */
			if (pba < min_rx_space)
				pba = min_rx_space;
		}
		wr32(E1000_PBA, pba);
	}

	/* flow control settings */
	/* The high water mark must be low enough to fit one full frame
	 * (or the size used for early receive) above it in the Rx FIFO.
	 * Set it to the lower of:
	 * - 90% of the Rx FIFO size, or
	 * - the full Rx FIFO size minus one full frame */
	hwm = min(((pba << 10) * 9 / 10),
			((pba << 10) - 2 * adapter->max_frame_size));

	if (mac->type < e1000_82576) {
		fc->high_water = hwm & 0xFFF8;	/* 8-byte granularity */
		fc->low_water = fc->high_water - 8;
	} else {
		fc->high_water = hwm & 0xFFF0;	/* 16-byte granularity */
		fc->low_water = fc->high_water - 16;
	}
	fc->pause_time = 0xFFFF;
	fc->send_xon = 1;
	fc->current_mode = fc->requested_mode;

	/* disable receive for all VFs and wait one second */
	if (adapter->vfs_allocated_count) {
		int i;
		for (i = 0 ; i < adapter->vfs_allocated_count; i++)
			adapter->vf_data[i].clear_to_send = false;

		/* ping all the active vfs to let them know we are going down */
			igb_ping_all_vfs(adapter);

		/* disable transmits and receives */
		wr32(E1000_VFRE, 0);
		wr32(E1000_VFTE, 0);
	}

	/* Allow time for pending master requests to run */
	adapter->hw.mac.ops.reset_hw(&adapter->hw);
	wr32(E1000_WUC, 0);

	if (adapter->hw.mac.ops.init_hw(&adapter->hw))
		dev_err(&adapter->pdev->dev, "Hardware Error\n");

	igb_update_mng_vlan(adapter);

	/* Enable h/w to recognize an 802.1Q VLAN Ethernet packet */
	wr32(E1000_VET, ETHERNET_IEEE_VLAN_TYPE);

	igb_reset_adaptive(&adapter->hw);
	igb_get_phy_info(&adapter->hw);
}

static const struct net_device_ops igb_netdev_ops = {
	.ndo_open 		= igb_open,
	.ndo_stop		= igb_close,
	.ndo_start_xmit		= igb_xmit_frame_adv,
	.ndo_get_stats		= igb_get_stats,
	.ndo_set_rx_mode	= igb_set_rx_mode,
	.ndo_set_multicast_list	= igb_set_rx_mode,
	.ndo_set_mac_address	= igb_set_mac,
	.ndo_change_mtu		= igb_change_mtu,
	.ndo_do_ioctl		= igb_ioctl,
	.ndo_tx_timeout		= igb_tx_timeout,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_vlan_rx_register	= igb_vlan_rx_register,
	.ndo_vlan_rx_add_vid	= igb_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid	= igb_vlan_rx_kill_vid,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= igb_netpoll,
#endif
};

/**
 * igb_probe - Device Initialization Routine
 * @pdev: PCI device information struct
 * @ent: entry in igb_pci_tbl
 *
 * Returns 0 on success, negative on failure
 *
 * igb_probe initializes an adapter identified by a pci_dev structure.
 * The OS initialization, configuring of the adapter private structure,
 * and a hardware reset occur.
 **/
static int __devinit igb_probe(struct pci_dev *pdev,
			       const struct pci_device_id *ent)
{
	struct net_device *netdev;
	struct igb_adapter *adapter;
	struct e1000_hw *hw;
	const struct e1000_info *ei = igb_info_tbl[ent->driver_data];
	unsigned long mmio_start, mmio_len;
	int err, pci_using_dac;
	u16 eeprom_data = 0;
	u16 eeprom_apme_mask = IGB_EEPROM_APME;
	u32 part_num;

	err = pci_enable_device_mem(pdev);
	if (err)
		return err;

	pci_using_dac = 0;
	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (!err) {
		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
		if (!err)
			pci_using_dac = 1;
	} else {
		err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
			if (err) {
				dev_err(&pdev->dev, "No usable DMA "
					"configuration, aborting\n");
				goto err_dma;
			}
		}
	}

	err = pci_request_selected_regions(pdev, pci_select_bars(pdev,
	                                   IORESOURCE_MEM),
	                                   igb_driver_name);
	if (err)
		goto err_pci_reg;

	pci_enable_pcie_error_reporting(pdev);

	pci_set_master(pdev);
	pci_save_state(pdev);

	err = -ENOMEM;
	netdev = alloc_etherdev_mq(sizeof(struct igb_adapter),
	                           IGB_ABS_MAX_TX_QUEUES);
	if (!netdev)
		goto err_alloc_etherdev;

	SET_NETDEV_DEV(netdev, &pdev->dev);

	pci_set_drvdata(pdev, netdev);
	adapter = netdev_priv(netdev);
	adapter->netdev = netdev;
	adapter->pdev = pdev;
	hw = &adapter->hw;
	hw->back = adapter;
	adapter->msg_enable = NETIF_MSG_DRV | NETIF_MSG_PROBE;

	mmio_start = pci_resource_start(pdev, 0);
	mmio_len = pci_resource_len(pdev, 0);

	err = -EIO;
	hw->hw_addr = ioremap(mmio_start, mmio_len);
	if (!hw->hw_addr)
		goto err_ioremap;

	netdev->netdev_ops = &igb_netdev_ops;
	igb_set_ethtool_ops(netdev);
	netdev->watchdog_timeo = 5 * HZ;

	strncpy(netdev->name, pci_name(pdev), sizeof(netdev->name) - 1);

	netdev->mem_start = mmio_start;
	netdev->mem_end = mmio_start + mmio_len;

	/* PCI config space info */
	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;
	hw->revision_id = pdev->revision;
	hw->subsystem_vendor_id = pdev->subsystem_vendor;
	hw->subsystem_device_id = pdev->subsystem_device;

	/* setup the private structure */
	hw->back = adapter;
	/* Copy the default MAC, PHY and NVM function pointers */
	memcpy(&hw->mac.ops, ei->mac_ops, sizeof(hw->mac.ops));
	memcpy(&hw->phy.ops, ei->phy_ops, sizeof(hw->phy.ops));
	memcpy(&hw->nvm.ops, ei->nvm_ops, sizeof(hw->nvm.ops));
	/* Initialize skew-specific constants */
	err = ei->get_invariants(hw);
	if (err)
		goto err_sw_init;

#ifdef CONFIG_PCI_IOV
	/* since iov functionality isn't critical to base device function we
	 * can accept failure.  If it fails we don't allow iov to be enabled */
	if (hw->mac.type == e1000_82576) {
		/* 82576 supports a maximum of 7 VFs in addition to the PF */
		unsigned int num_vfs = (max_vfs > 7) ? 7 : max_vfs;
		int i;
		unsigned char mac_addr[ETH_ALEN];

		if (num_vfs) {
			adapter->vf_data = kcalloc(num_vfs,
						sizeof(struct vf_data_storage),
						GFP_KERNEL);
			if (!adapter->vf_data) {
				dev_err(&pdev->dev,
				        "Could not allocate VF private data - "
					"IOV enable failed\n");
			} else {
				err = pci_enable_sriov(pdev, num_vfs);
				if (!err) {
					adapter->vfs_allocated_count = num_vfs;
					dev_info(&pdev->dev,
					         "%d vfs allocated\n",
					         num_vfs);
					for (i = 0;
					     i < adapter->vfs_allocated_count;
					     i++) {
						random_ether_addr(mac_addr);
						igb_set_vf_mac(adapter, i,
						               mac_addr);
					}
				} else {
					kfree(adapter->vf_data);
					adapter->vf_data = NULL;
				}
			}
		}
	}

#endif
	/* setup the private structure */
	err = igb_sw_init(adapter);
	if (err)
		goto err_sw_init;

	igb_get_bus_info_pcie(hw);

	/* set flags */
	switch (hw->mac.type) {
	case e1000_82575:
		adapter->flags |= IGB_FLAG_NEED_CTX_IDX;
		break;
	case e1000_82576:
	default:
		break;
	}

	hw->phy.autoneg_wait_to_complete = false;
	hw->mac.adaptive_ifs = true;

	/* Copper options */
	if (hw->phy.media_type == e1000_media_type_copper) {
		hw->phy.mdix = AUTO_ALL_MODES;
		hw->phy.disable_polarity_correction = false;
		hw->phy.ms_type = e1000_ms_hw_default;
	}

	if (igb_check_reset_block(hw))
		dev_info(&pdev->dev,
			"PHY reset is blocked due to SOL/IDER session.\n");

	netdev->features = NETIF_F_SG |
			   NETIF_F_IP_CSUM |
			   NETIF_F_HW_VLAN_TX |
			   NETIF_F_HW_VLAN_RX |
			   NETIF_F_HW_VLAN_FILTER;

	netdev->features |= NETIF_F_IPV6_CSUM;
	netdev->features |= NETIF_F_TSO;
	netdev->features |= NETIF_F_TSO6;

	netdev->features |= NETIF_F_GRO;

	netdev->vlan_features |= NETIF_F_TSO;
	netdev->vlan_features |= NETIF_F_TSO6;
	netdev->vlan_features |= NETIF_F_IP_CSUM;
	netdev->vlan_features |= NETIF_F_IPV6_CSUM;
	netdev->vlan_features |= NETIF_F_SG;

	if (pci_using_dac)
		netdev->features |= NETIF_F_HIGHDMA;

	if (adapter->hw.mac.type == e1000_82576)
		netdev->features |= NETIF_F_SCTP_CSUM;

	adapter->en_mng_pt = igb_enable_mng_pass_thru(&adapter->hw);

	/* before reading the NVM, reset the controller to put the device in a
	 * known good starting state */
	hw->mac.ops.reset_hw(hw);

	/* make sure the NVM is good */
	if (igb_validate_nvm_checksum(hw) < 0) {
		dev_err(&pdev->dev, "The NVM Checksum Is Not Valid\n");
		err = -EIO;
		goto err_eeprom;
	}

	/* copy the MAC address out of the NVM */
	if (hw->mac.ops.read_mac_addr(hw))
		dev_err(&pdev->dev, "NVM Read Error\n");

	memcpy(netdev->dev_addr, hw->mac.addr, netdev->addr_len);
	memcpy(netdev->perm_addr, hw->mac.addr, netdev->addr_len);

	if (!is_valid_ether_addr(netdev->perm_addr)) {
		dev_err(&pdev->dev, "Invalid MAC Address\n");
		err = -EIO;
		goto err_eeprom;
	}

	setup_timer(&adapter->watchdog_timer, &igb_watchdog,
	            (unsigned long) adapter);
	setup_timer(&adapter->phy_info_timer, &igb_update_phy_info,
	            (unsigned long) adapter);

	INIT_WORK(&adapter->reset_task, igb_reset_task);
	INIT_WORK(&adapter->watchdog_task, igb_watchdog_task);

	/* Initialize link properties that are user-changeable */
	adapter->fc_autoneg = true;
	hw->mac.autoneg = true;
	hw->phy.autoneg_advertised = 0x2f;

	hw->fc.requested_mode = e1000_fc_default;
	hw->fc.current_mode = e1000_fc_default;

	adapter->itr_setting = IGB_DEFAULT_ITR;
	adapter->itr = IGB_START_ITR;

	igb_validate_mdi_setting(hw);

	/* Initial Wake on LAN setting If APM wake is enabled in the EEPROM,
	 * enable the ACPI Magic Packet filter
	 */

	if (hw->bus.func == 0)
		hw->nvm.ops.read(hw, NVM_INIT_CONTROL3_PORT_A, 1, &eeprom_data);
	else if (hw->bus.func == 1)
		hw->nvm.ops.read(hw, NVM_INIT_CONTROL3_PORT_B, 1, &eeprom_data);

	if (eeprom_data & eeprom_apme_mask)
		adapter->eeprom_wol |= E1000_WUFC_MAG;

	/* now that we have the eeprom settings, apply the special cases where
	 * the eeprom may be wrong or the board simply won't support wake on
	 * lan on a particular port */
	switch (pdev->device) {
	case E1000_DEV_ID_82575GB_QUAD_COPPER:
		adapter->eeprom_wol = 0;
		break;
	case E1000_DEV_ID_82575EB_FIBER_SERDES:
	case E1000_DEV_ID_82576_FIBER:
	case E1000_DEV_ID_82576_SERDES:
		/* Wake events only supported on port A for dual fiber
		 * regardless of eeprom setting */
		if (rd32(E1000_STATUS) & E1000_STATUS_FUNC_1)
			adapter->eeprom_wol = 0;
		break;
	case E1000_DEV_ID_82576_QUAD_COPPER:
		/* if quad port adapter, disable WoL on all but port A */
		if (global_quad_port_a != 0)
			adapter->eeprom_wol = 0;
		else
			adapter->flags |= IGB_FLAG_QUAD_PORT_A;
		/* Reset for multiple quad port adapters */
		if (++global_quad_port_a == 4)
			global_quad_port_a = 0;
		break;
	}

	/* initialize the wol settings based on the eeprom settings */
	adapter->wol = adapter->eeprom_wol;
	device_set_wakeup_enable(&adapter->pdev->dev, adapter->wol);

	/* reset the hardware with the new settings */
	igb_reset(adapter);

	/* let the f/w know that the h/w is now under the control of the
	 * driver. */
	igb_get_hw_control(adapter);

	strcpy(netdev->name, "eth%d");
	err = register_netdev(netdev);
	if (err)
		goto err_register;

	/* carrier off reporting is important to ethtool even BEFORE open */
	netif_carrier_off(netdev);

#ifdef CONFIG_IGB_DCA
	if (dca_add_requester(&pdev->dev) == 0) {
		adapter->flags |= IGB_FLAG_DCA_ENABLED;
		dev_info(&pdev->dev, "DCA enabled\n");
		igb_setup_dca(adapter);
	}
#endif

	/*
	 * Initialize hardware timer: we keep it running just in case
	 * that some program needs it later on.
	 */
	memset(&adapter->cycles, 0, sizeof(adapter->cycles));
	adapter->cycles.read = igb_read_clock;
	adapter->cycles.mask = CLOCKSOURCE_MASK(64);
	adapter->cycles.mult = 1;
	adapter->cycles.shift = IGB_TSYNC_SHIFT;
	wr32(E1000_TIMINCA,
	     (1<<24) |
	     IGB_TSYNC_CYCLE_TIME_IN_NANOSECONDS * IGB_TSYNC_SCALE);
#if 0
	/*
	 * Avoid rollover while we initialize by resetting the time counter.
	 */
	wr32(E1000_SYSTIML, 0x00000000);
	wr32(E1000_SYSTIMH, 0x00000000);
#else
	/*
	 * Set registers so that rollover occurs soon to test this.
	 */
	wr32(E1000_SYSTIML, 0x00000000);
	wr32(E1000_SYSTIMH, 0xFF800000);
#endif
	wrfl();
	timecounter_init(&adapter->clock,
			 &adapter->cycles,
			 ktime_to_ns(ktime_get_real()));

	/*
	 * Synchronize our NIC clock against system wall clock. NIC
	 * time stamp reading requires ~3us per sample, each sample
	 * was pretty stable even under load => only require 10
	 * samples for each offset comparison.
	 */
	memset(&adapter->compare, 0, sizeof(adapter->compare));
	adapter->compare.source = &adapter->clock;
	adapter->compare.target = ktime_get_real;
	adapter->compare.num_samples = 10;
	timecompare_update(&adapter->compare, 0);

#ifdef DEBUG
	{
		char buffer[160];
		printk(KERN_DEBUG
			"igb: %s: hw %p initialized timer\n",
			igb_get_time_str(adapter, buffer),
			&adapter->hw);
	}
#endif

	dev_info(&pdev->dev, "Intel(R) Gigabit Ethernet Network Connection\n");
	/* print bus type/speed/width info */
	dev_info(&pdev->dev, "%s: (PCIe:%s:%s) %pM\n",
		 netdev->name,
		 ((hw->bus.speed == e1000_bus_speed_2500)
		  ? "2.5Gb/s" : "unknown"),
		 ((hw->bus.width == e1000_bus_width_pcie_x4) ? "Width x4" :
		  (hw->bus.width == e1000_bus_width_pcie_x2) ? "Width x2" :
		  (hw->bus.width == e1000_bus_width_pcie_x1) ? "Width x1" :
		   "unknown"),
		 netdev->dev_addr);

	igb_read_part_num(hw, &part_num);
	dev_info(&pdev->dev, "%s: PBA No: %06x-%03x\n", netdev->name,
		(part_num >> 8), (part_num & 0xff));

	dev_info(&pdev->dev,
		"Using %s interrupts. %d rx queue(s), %d tx queue(s)\n",
		adapter->msix_entries ? "MSI-X" :
		(adapter->flags & IGB_FLAG_HAS_MSI) ? "MSI" : "legacy",
		adapter->num_rx_queues, adapter->num_tx_queues);

	return 0;

err_register:
	igb_release_hw_control(adapter);
err_eeprom:
	if (!igb_check_reset_block(hw))
		igb_reset_phy(hw);

	if (hw->flash_address)
		iounmap(hw->flash_address);

	igb_free_queues(adapter);
err_sw_init:
	iounmap(hw->hw_addr);
err_ioremap:
	free_netdev(netdev);
err_alloc_etherdev:
	pci_release_selected_regions(pdev, pci_select_bars(pdev,
	                             IORESOURCE_MEM));
err_pci_reg:
err_dma:
	pci_disable_device(pdev);
	return err;
}

/**
 * igb_remove - Device Removal Routine
 * @pdev: PCI device information struct
 *
 * igb_remove is called by the PCI subsystem to alert the driver
 * that it should release a PCI device.  The could be caused by a
 * Hot-Plug event, or because the driver is going to be removed from
 * memory.
 **/
static void __devexit igb_remove(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct igb_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;

	/* flush_scheduled work may reschedule our watchdog task, so
	 * explicitly disable watchdog tasks from being rescheduled  */
	set_bit(__IGB_DOWN, &adapter->state);
	del_timer_sync(&adapter->watchdog_timer);
	del_timer_sync(&adapter->phy_info_timer);

	flush_scheduled_work();

#ifdef CONFIG_IGB_DCA
	if (adapter->flags & IGB_FLAG_DCA_ENABLED) {
		dev_info(&pdev->dev, "DCA disabled\n");
		dca_remove_requester(&pdev->dev);
		adapter->flags &= ~IGB_FLAG_DCA_ENABLED;
		wr32(E1000_DCA_CTRL, E1000_DCA_CTRL_DCA_MODE_DISABLE);
	}
#endif

	/* Release control of h/w to f/w.  If f/w is AMT enabled, this
	 * would have already happened in close and is redundant. */
	igb_release_hw_control(adapter);

	unregister_netdev(netdev);

	if (!igb_check_reset_block(&adapter->hw))
		igb_reset_phy(&adapter->hw);

	igb_reset_interrupt_capability(adapter);

	igb_free_queues(adapter);

#ifdef CONFIG_PCI_IOV
	/* reclaim resources allocated to VFs */
	if (adapter->vf_data) {
		/* disable iov and allow time for transactions to clear */
		pci_disable_sriov(pdev);
		msleep(500);

		kfree(adapter->vf_data);
		adapter->vf_data = NULL;
		wr32(E1000_IOVCTL, E1000_IOVCTL_REUSE_VFQ);
		msleep(100);
		dev_info(&pdev->dev, "IOV Disabled\n");
	}
#endif
	iounmap(hw->hw_addr);
	if (hw->flash_address)
		iounmap(hw->flash_address);
	pci_release_selected_regions(pdev, pci_select_bars(pdev,
	                             IORESOURCE_MEM));

	free_netdev(netdev);

	pci_disable_pcie_error_reporting(pdev);

	pci_disable_device(pdev);
}

/**
 * igb_sw_init - Initialize general software structures (struct igb_adapter)
 * @adapter: board private structure to initialize
 *
 * igb_sw_init initializes the Adapter private data structure.
 * Fields are initialized based on PCI device information and
 * OS network device settings (MTU size).
 **/
static int __devinit igb_sw_init(struct igb_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;

	pci_read_config_word(pdev, PCI_COMMAND, &hw->bus.pci_cmd_word);

	adapter->tx_ring_count = IGB_DEFAULT_TXD;
	adapter->rx_ring_count = IGB_DEFAULT_RXD;
	adapter->rx_buffer_len = MAXIMUM_ETHERNET_VLAN_SIZE;
	adapter->rx_ps_hdr_size = 0; /* disable packet split */
	adapter->max_frame_size = netdev->mtu + ETH_HLEN + ETH_FCS_LEN;
	adapter->min_frame_size = ETH_ZLEN + ETH_FCS_LEN;

	/* This call may decrease the number of queues depending on
	 * interrupt mode. */
	igb_set_interrupt_capability(adapter);

	if (igb_alloc_queues(adapter)) {
		dev_err(&pdev->dev, "Unable to allocate memory for queues\n");
		return -ENOMEM;
	}

	/* Explicitly disable IRQ since the NIC can be in any state. */
	igb_irq_disable(adapter);

	set_bit(__IGB_DOWN, &adapter->state);
	return 0;
}

/**
 * igb_open - Called when a network interface is made active
 * @netdev: network interface device structure
 *
 * Returns 0 on success, negative value on failure
 *
 * The open entry point is called when a network interface is made
 * active by the system (IFF_UP).  At this point all resources needed
 * for transmit and receive operations are allocated, the interrupt
 * handler is registered with the OS, the watchdog timer is started,
 * and the stack is notified that the interface is ready.
 **/
static int igb_open(struct net_device *netdev)
{
	struct igb_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;
	int err;
	int i;

	/* disallow open during test */
	if (test_bit(__IGB_TESTING, &adapter->state))
		return -EBUSY;

	netif_carrier_off(netdev);

	/* allocate transmit descriptors */
	err = igb_setup_all_tx_resources(adapter);
	if (err)
		goto err_setup_tx;

	/* allocate receive descriptors */
	err = igb_setup_all_rx_resources(adapter);
	if (err)
		goto err_setup_rx;

	/* e1000_power_up_phy(adapter); */

	adapter->mng_vlan_id = IGB_MNG_VLAN_NONE;
	if ((adapter->hw.mng_cookie.status &
	     E1000_MNG_DHCP_COOKIE_STATUS_VLAN))
		igb_update_mng_vlan(adapter);

	/* before we allocate an interrupt, we must be ready to handle it.
	 * Setting DEBUG_SHIRQ in the kernel makes it fire an interrupt
	 * as soon as we call pci_request_irq, so we have to setup our
	 * clean_rx handler before we do so.  */
	igb_configure(adapter);

	igb_vmm_control(adapter);
	igb_set_rah_pool(hw, adapter->vfs_allocated_count, 0);
	igb_set_vmolr(hw, adapter->vfs_allocated_count);

	err = igb_request_irq(adapter);
	if (err)
		goto err_req_irq;

	/* From here on the code is the same as igb_up() */
	clear_bit(__IGB_DOWN, &adapter->state);

	for (i = 0; i < adapter->num_rx_queues; i++)
		napi_enable(&adapter->rx_ring[i].napi);

	/* Clear any pending interrupts. */
	rd32(E1000_ICR);

	igb_irq_enable(adapter);

	netif_tx_start_all_queues(netdev);

	/* Fire a link status change interrupt to start the watchdog. */
	wr32(E1000_ICS, E1000_ICS_LSC);

	return 0;

err_req_irq:
	igb_release_hw_control(adapter);
	/* e1000_power_down_phy(adapter); */
	igb_free_all_rx_resources(adapter);
err_setup_rx:
	igb_free_all_tx_resources(adapter);
err_setup_tx:
	igb_reset(adapter);

	return err;
}

/**
 * igb_close - Disables a network interface
 * @netdev: network interface device structure
 *
 * Returns 0, this is not allowed to fail
 *
 * The close entry point is called when an interface is de-activated
 * by the OS.  The hardware is still under the driver's control, but
 * needs to be disabled.  A global MAC reset is issued to stop the
 * hardware, and all transmit and receive resources are freed.
 **/
static int igb_close(struct net_device *netdev)
{
	struct igb_adapter *adapter = netdev_priv(netdev);

	WARN_ON(test_bit(__IGB_RESETTING, &adapter->state));
	igb_down(adapter);

	igb_free_irq(adapter);

	igb_free_all_tx_resources(adapter);
	igb_free_all_rx_resources(adapter);

	/* kill manageability vlan ID if supported, but not if a vlan with
	 * the same ID is registered on the host OS (let 8021q kill it) */
	if ((adapter->hw.mng_cookie.status &
			  E1000_MNG_DHCP_COOKIE_STATUS_VLAN) &&
	     !(adapter->vlgrp &&
	       vlan_group_get_device(adapter->vlgrp, adapter->mng_vlan_id)))
		igb_vlan_rx_kill_vid(netdev, adapter->mng_vlan_id);

	return 0;
}

/**
 * igb_setup_tx_resources - allocate Tx resources (Descriptors)
 * @adapter: board private structure
 * @tx_ring: tx descriptor ring (for a specific queue) to setup
 *
 * Return 0 on success, negative on failure
 **/
int igb_setup_tx_resources(struct igb_adapter *adapter,
			   struct igb_ring *tx_ring)
{
	struct pci_dev *pdev = adapter->pdev;
	int size;

	size = sizeof(struct igb_buffer) * tx_ring->count;
	tx_ring->buffer_info = vmalloc(size);
	if (!tx_ring->buffer_info)
		goto err;
	memset(tx_ring->buffer_info, 0, size);

	/* round up to nearest 4K */
	tx_ring->size = tx_ring->count * sizeof(union e1000_adv_tx_desc);
	tx_ring->size = ALIGN(tx_ring->size, 4096);

	tx_ring->desc = pci_alloc_consistent(pdev, tx_ring->size,
					     &tx_ring->dma);

	if (!tx_ring->desc)
		goto err;

	tx_ring->adapter = adapter;
	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;
	return 0;

err:
	vfree(tx_ring->buffer_info);
	dev_err(&adapter->pdev->dev,
		"Unable to allocate memory for the transmit descriptor ring\n");
	return -ENOMEM;
}

/**
 * igb_setup_all_tx_resources - wrapper to allocate Tx resources
 *				  (Descriptors) for all queues
 * @adapter: board private structure
 *
 * Return 0 on success, negative on failure
 **/
static int igb_setup_all_tx_resources(struct igb_adapter *adapter)
{
	int i, err = 0;
	int r_idx;

	for (i = 0; i < adapter->num_tx_queues; i++) {
		err = igb_setup_tx_resources(adapter, &adapter->tx_ring[i]);
		if (err) {
			dev_err(&adapter->pdev->dev,
				"Allocation for Tx Queue %u failed\n", i);
			for (i--; i >= 0; i--)
				igb_free_tx_resources(&adapter->tx_ring[i]);
			break;
		}
	}

	for (i = 0; i < IGB_MAX_TX_QUEUES; i++) {
		r_idx = i % adapter->num_tx_queues;
		adapter->multi_tx_table[i] = &adapter->tx_ring[r_idx];
	}
	return err;
}

/**
 * igb_configure_tx - Configure transmit Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Tx unit of the MAC after a reset.
 **/
static void igb_configure_tx(struct igb_adapter *adapter)
{
	u64 tdba;
	struct e1000_hw *hw = &adapter->hw;
	u32 tctl;
	u32 txdctl, txctrl;
	int i, j;

	for (i = 0; i < adapter->num_tx_queues; i++) {
		struct igb_ring *ring = &adapter->tx_ring[i];
		j = ring->reg_idx;
		wr32(E1000_TDLEN(j),
		     ring->count * sizeof(union e1000_adv_tx_desc));
		tdba = ring->dma;
		wr32(E1000_TDBAL(j),
		     tdba & 0x00000000ffffffffULL);
		wr32(E1000_TDBAH(j), tdba >> 32);

		ring->head = E1000_TDH(j);
		ring->tail = E1000_TDT(j);
		writel(0, hw->hw_addr + ring->tail);
		writel(0, hw->hw_addr + ring->head);
		txdctl = rd32(E1000_TXDCTL(j));
		txdctl |= E1000_TXDCTL_QUEUE_ENABLE;
		wr32(E1000_TXDCTL(j), txdctl);

		/* Turn off Relaxed Ordering on head write-backs.  The
		 * writebacks MUST be delivered in order or it will
		 * completely screw up our bookeeping.
		 */
		txctrl = rd32(E1000_DCA_TXCTRL(j));
		txctrl &= ~E1000_DCA_TXCTRL_TX_WB_RO_EN;
		wr32(E1000_DCA_TXCTRL(j), txctrl);
	}

	/* disable queue 0 to prevent tail bump w/o re-configuration */
	if (adapter->vfs_allocated_count)
		wr32(E1000_TXDCTL(0), 0);

	/* Program the Transmit Control Register */
	tctl = rd32(E1000_TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= E1000_TCTL_PSP | E1000_TCTL_RTLC |
		(E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);

	igb_config_collision_dist(hw);

	/* Setup Transmit Descriptor Settings for eop descriptor */
	adapter->txd_cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;

	/* Enable transmits */
	tctl |= E1000_TCTL_EN;

	wr32(E1000_TCTL, tctl);
}

/**
 * igb_setup_rx_resources - allocate Rx resources (Descriptors)
 * @adapter: board private structure
 * @rx_ring:    rx descriptor ring (for a specific queue) to setup
 *
 * Returns 0 on success, negative on failure
 **/
int igb_setup_rx_resources(struct igb_adapter *adapter,
			   struct igb_ring *rx_ring)
{
	struct pci_dev *pdev = adapter->pdev;
	int size, desc_len;

	size = sizeof(struct igb_buffer) * rx_ring->count;
	rx_ring->buffer_info = vmalloc(size);
	if (!rx_ring->buffer_info)
		goto err;
	memset(rx_ring->buffer_info, 0, size);

	desc_len = sizeof(union e1000_adv_rx_desc);

	/* Round up to nearest 4K */
	rx_ring->size = rx_ring->count * desc_len;
	rx_ring->size = ALIGN(rx_ring->size, 4096);

	rx_ring->desc = pci_alloc_consistent(pdev, rx_ring->size,
					     &rx_ring->dma);

	if (!rx_ring->desc)
		goto err;

	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;

	rx_ring->adapter = adapter;

	return 0;

err:
	vfree(rx_ring->buffer_info);
	dev_err(&adapter->pdev->dev, "Unable to allocate memory for "
		"the receive descriptor ring\n");
	return -ENOMEM;
}

/**
 * igb_setup_all_rx_resources - wrapper to allocate Rx resources
 *				  (Descriptors) for all queues
 * @adapter: board private structure
 *
 * Return 0 on success, negative on failure
 **/
static int igb_setup_all_rx_resources(struct igb_adapter *adapter)
{
	int i, err = 0;

	for (i = 0; i < adapter->num_rx_queues; i++) {
		err = igb_setup_rx_resources(adapter, &adapter->rx_ring[i]);
		if (err) {
			dev_err(&adapter->pdev->dev,
				"Allocation for Rx Queue %u failed\n", i);
			for (i--; i >= 0; i--)
				igb_free_rx_resources(&adapter->rx_ring[i]);
			break;
		}
	}

	return err;
}

/**
 * igb_setup_rctl - configure the receive control registers
 * @adapter: Board private structure
 **/
static void igb_setup_rctl(struct igb_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 rctl;
	u32 srrctl = 0;
	int i;

	rctl = rd32(E1000_RCTL);

	rctl &= ~(3 << E1000_RCTL_MO_SHIFT);
	rctl &= ~(E1000_RCTL_LBM_TCVR | E1000_RCTL_LBM_MAC);

	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_RDMTS_HALF |
		(hw->mac.mc_filter_type << E1000_RCTL_MO_SHIFT);

	/*
	 * enable stripping of CRC. It's unlikely this will break BMC
	 * redirection as it did with e1000. Newer features require
	 * that the HW strips the CRC.
	 */
	rctl |= E1000_RCTL_SECRC;

	/*
	 * disable store bad packets and clear size bits.
	 */
	rctl &= ~(E1000_RCTL_SBP | E1000_RCTL_SZ_256);

	/* enable LPE when to prevent packets larger than max_frame_size */
		rctl |= E1000_RCTL_LPE;

	/* Setup buffer sizes */
	switch (adapter->rx_buffer_len) {
	case IGB_RXBUFFER_256:
		rctl |= E1000_RCTL_SZ_256;
		break;
	case IGB_RXBUFFER_512:
		rctl |= E1000_RCTL_SZ_512;
		break;
	default:
		srrctl = ALIGN(adapter->rx_buffer_len, 1024)
		         >> E1000_SRRCTL_BSIZEPKT_SHIFT;
		break;
	}

	/* 82575 and greater support packet-split where the protocol
	 * header is placed in skb->data and the packet data is
	 * placed in pages hanging off of skb_shinfo(skb)->nr_frags.
	 * In the case of a non-split, skb->data is linearly filled,
	 * followed by the page buffers.  Therefore, skb->data is
	 * sized to hold the largest protocol header.
	 */
	/* allocations using alloc_page take too long for regular MTU
	 * so only enable packet split for jumbo frames */
	if (adapter->netdev->mtu > ETH_DATA_LEN) {
		adapter->rx_ps_hdr_size = IGB_RXBUFFER_128;
		srrctl |= adapter->rx_ps_hdr_size <<
			 E1000_SRRCTL_BSIZEHDRSIZE_SHIFT;
		srrctl |= E1000_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS;
	} else {
		adapter->rx_ps_hdr_size = 0;
		srrctl |= E1000_SRRCTL_DESCTYPE_ADV_ONEBUF;
	}

	/* Attention!!!  For SR-IOV PF driver operations you must enable
	 * queue drop for all VF and PF queues to prevent head of line blocking
	 * if an un-trusted VF does not provide descriptors to hardware.
	 */
	if (adapter->vfs_allocated_count) {
		u32 vmolr;

		/* set all queue drop enable bits */
		wr32(E1000_QDE, ALL_QUEUES);
		srrctl |= E1000_SRRCTL_DROP_EN;

		/* disable queue 0 to prevent tail write w/o re-config */
		wr32(E1000_RXDCTL(0), 0);

		vmolr = rd32(E1000_VMOLR(adapter->vfs_allocated_count));
		if (rctl & E1000_RCTL_LPE)
			vmolr |= E1000_VMOLR_LPE;
		if (adapter->num_rx_queues > 1)
			vmolr |= E1000_VMOLR_RSSE;
		wr32(E1000_VMOLR(adapter->vfs_allocated_count), vmolr);
	}

	for (i = 0; i < adapter->num_rx_queues; i++) {
		int j = adapter->rx_ring[i].reg_idx;
		wr32(E1000_SRRCTL(j), srrctl);
	}

	wr32(E1000_RCTL, rctl);
}

/**
 * igb_rlpml_set - set maximum receive packet size
 * @adapter: board private structure
 *
 * Configure maximum receivable packet size.
 **/
static void igb_rlpml_set(struct igb_adapter *adapter)
{
	u32 max_frame_size = adapter->max_frame_size;
	struct e1000_hw *hw = &adapter->hw;
	u16 pf_id = adapter->vfs_allocated_count;

	if (adapter->vlgrp)
		max_frame_size += VLAN_TAG_SIZE;

	/* if vfs are enabled we set RLPML to the largest possible request
	 * size and set the VMOLR RLPML to the size we need */
	if (pf_id) {
		igb_set_vf_rlpml(adapter, max_frame_size, pf_id);
		max_frame_size = MAX_STD_JUMBO_FRAME_SIZE + VLAN_TAG_SIZE;
	}

	wr32(E1000_RLPML, max_frame_size);
}

/**
 * igb_configure_vt_default_pool - Configure VT default pool
 * @adapter: board private structure
 *
 * Configure the default pool
 **/
static void igb_configure_vt_default_pool(struct igb_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u16 pf_id = adapter->vfs_allocated_count;
	u32 vtctl;

	/* not in sr-iov mode - do nothing */
	if (!pf_id)
		return;

	vtctl = rd32(E1000_VT_CTL);
	vtctl &= ~(E1000_VT_CTL_DEFAULT_POOL_MASK |
		   E1000_VT_CTL_DISABLE_DEF_POOL);
	vtctl |= pf_id << E1000_VT_CTL_DEFAULT_POOL_SHIFT;
	wr32(E1000_VT_CTL, vtctl);
}

/**
 * igb_configure_rx - Configure receive Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Rx unit of the MAC after a reset.
 **/
static void igb_configure_rx(struct igb_adapter *adapter)
{
	u64 rdba;
	struct e1000_hw *hw = &adapter->hw;
	u32 rctl, rxcsum;
	u32 rxdctl;
	int i;

	/* disable receives while setting up the descriptors */
	rctl = rd32(E1000_RCTL);
	wr32(E1000_RCTL, rctl & ~E1000_RCTL_EN);
	wrfl();
	mdelay(10);

	if (adapter->itr_setting > 3)
		wr32(E1000_ITR, adapter->itr);

	/* Setup the HW Rx Head and Tail Descriptor Pointers and
	 * the Base and Length of the Rx Descriptor Ring */
	for (i = 0; i < adapter->num_rx_queues; i++) {
		struct igb_ring *ring = &adapter->rx_ring[i];
		int j = ring->reg_idx;
		rdba = ring->dma;
		wr32(E1000_RDBAL(j),
		     rdba & 0x00000000ffffffffULL);
		wr32(E1000_RDBAH(j), rdba >> 32);
		wr32(E1000_RDLEN(j),
		     ring->count * sizeof(union e1000_adv_rx_desc));

		ring->head = E1000_RDH(j);
		ring->tail = E1000_RDT(j);
		writel(0, hw->hw_addr + ring->tail);
		writel(0, hw->hw_addr + ring->head);

		rxdctl = rd32(E1000_RXDCTL(j));
		rxdctl |= E1000_RXDCTL_QUEUE_ENABLE;
		rxdctl &= 0xFFF00000;
		rxdctl |= IGB_RX_PTHRESH;
		rxdctl |= IGB_RX_HTHRESH << 8;
		rxdctl |= IGB_RX_WTHRESH << 16;
		wr32(E1000_RXDCTL(j), rxdctl);
	}

	if (adapter->num_rx_queues > 1) {
		u32 random[10];
		u32 mrqc;
		u32 j, shift;
		union e1000_reta {
			u32 dword;
			u8  bytes[4];
		} reta;

		get_random_bytes(&random[0], 40);

		if (hw->mac.type >= e1000_82576)
			shift = 0;
		else
			shift = 6;
		for (j = 0; j < (32 * 4); j++) {
			reta.bytes[j & 3] =
				adapter->rx_ring[(j % adapter->num_rx_queues)].reg_idx << shift;
			if ((j & 3) == 3)
				writel(reta.dword,
				       hw->hw_addr + E1000_RETA(0) + (j & ~3));
		}
		if (adapter->vfs_allocated_count)
			mrqc = E1000_MRQC_ENABLE_VMDQ_RSS_2Q;
		else
			mrqc = E1000_MRQC_ENABLE_RSS_4Q;

		/* Fill out hash function seeds */
		for (j = 0; j < 10; j++)
			array_wr32(E1000_RSSRK(0), j, random[j]);

		mrqc |= (E1000_MRQC_RSS_FIELD_IPV4 |
			 E1000_MRQC_RSS_FIELD_IPV4_TCP);
		mrqc |= (E1000_MRQC_RSS_FIELD_IPV6 |
			 E1000_MRQC_RSS_FIELD_IPV6_TCP);
		mrqc |= (E1000_MRQC_RSS_FIELD_IPV4_UDP |
			 E1000_MRQC_RSS_FIELD_IPV6_UDP);
		mrqc |= (E1000_MRQC_RSS_FIELD_IPV6_UDP_EX |
			 E1000_MRQC_RSS_FIELD_IPV6_TCP_EX);

		wr32(E1000_MRQC, mrqc);
	} else if (adapter->vfs_allocated_count) {
		/* Enable multi-queue for sr-iov */
		wr32(E1000_MRQC, E1000_MRQC_ENABLE_VMDQ);
	}

	/* Enable Receive Checksum Offload for TCP and UDP */
	rxcsum = rd32(E1000_RXCSUM);
	/* Disable raw packet checksumming */
	rxcsum |= E1000_RXCSUM_PCSD;

	if (adapter->hw.mac.type == e1000_82576)
		/* Enable Receive Checksum Offload for SCTP */
		rxcsum |= E1000_RXCSUM_CRCOFL;

	/* Don't need to set TUOFL or IPOFL, they default to 1 */
	wr32(E1000_RXCSUM, rxcsum);

	/* Set the default pool for the PF's first queue */
	igb_configure_vt_default_pool(adapter);

	igb_rlpml_set(adapter);

	/* Enable Receives */
	wr32(E1000_RCTL, rctl);
}

/**
 * igb_free_tx_resources - Free Tx Resources per Queue
 * @tx_ring: Tx descriptor ring for a specific queue
 *
 * Free all transmit software resources
 **/
void igb_free_tx_resources(struct igb_ring *tx_ring)
{
	struct pci_dev *pdev = tx_ring->adapter->pdev;

	igb_clean_tx_ring(tx_ring);

	vfree(tx_ring->buffer_info);
	tx_ring->buffer_info = NULL;

	pci_free_consistent(pdev, tx_ring->size, tx_ring->desc, tx_ring->dma);

	tx_ring->desc = NULL;
}

/**
 * igb_free_all_tx_resources - Free Tx Resources for All Queues
 * @adapter: board private structure
 *
 * Free all transmit software resources
 **/
static void igb_free_all_tx_resources(struct igb_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_tx_queues; i++)
		igb_free_tx_resources(&adapter->tx_ring[i]);
}

static void igb_unmap_and_free_tx_resource(struct igb_adapter *adapter,
					   struct igb_buffer *buffer_info)
{
	buffer_info->dma = 0;
	if (buffer_info->skb) {
		skb_dma_unmap(&adapter->pdev->dev, buffer_info->skb,
		              DMA_TO_DEVICE);
		dev_kfree_skb_any(buffer_info->skb);
		buffer_info->skb = NULL;
	}
	buffer_info->time_stamp = 0;
	/* buffer_info must be completely set up in the transmit path */
}

/**
 * igb_clean_tx_ring - Free Tx Buffers
 * @tx_ring: ring to be cleaned
 **/
static void igb_clean_tx_ring(struct igb_ring *tx_ring)
{
	struct igb_adapter *adapter = tx_ring->adapter;
	struct igb_buffer *buffer_info;
	unsigned long size;
	unsigned int i;

	if (!tx_ring->buffer_info)
		return;
	/* Free all the Tx ring sk_buffs */

	for (i = 0; i < tx_ring->count; i++) {
		buffer_info = &tx_ring->buffer_info[i];
		igb_unmap_and_free_tx_resource(adapter, buffer_info);
	}

	size = sizeof(struct igb_buffer) * tx_ring->count;
	memset(tx_ring->buffer_info, 0, size);

	/* Zero out the descriptor ring */

	memset(tx_ring->desc, 0, tx_ring->size);

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;

	writel(0, adapter->hw.hw_addr + tx_ring->head);
	writel(0, adapter->hw.hw_addr + tx_ring->tail);
}

/**
 * igb_clean_all_tx_rings - Free Tx Buffers for all queues
 * @adapter: board private structure
 **/
static void igb_clean_all_tx_rings(struct igb_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_tx_queues; i++)
		igb_clean_tx_ring(&adapter->tx_ring[i]);
}

/**
 * igb_free_rx_resources - Free Rx Resources
 * @rx_ring: ring to clean the resources from
 *
 * Free all receive software resources
 **/
void igb_free_rx_resources(struct igb_ring *rx_ring)
{
	struct pci_dev *pdev = rx_ring->adapter->pdev;

	igb_clean_rx_ring(rx_ring);

	vfree(rx_ring->buffer_info);
	rx_ring->buffer_info = NULL;

	pci_free_consistent(pdev, rx_ring->size, rx_ring->desc, rx_ring->dma);

	rx_ring->desc = NULL;
}

/**
 * igb_free_all_rx_resources - Free Rx Resources for All Queues
 * @adapter: board private structure
 *
 * Free all receive software resources
 **/
static void igb_free_all_rx_resources(struct igb_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_rx_queues; i++)
		igb_free_rx_resources(&adapter->rx_ring[i]);
}

/**
 * igb_clean_rx_ring - Free Rx Buffers per Queue
 * @rx_ring: ring to free buffers from
 **/
static void igb_clean_rx_ring(struct igb_ring *rx_ring)
{
	struct igb_adapter *adapter = rx_ring->adapter;
	struct igb_buffer *buffer_info;
	struct pci_dev *pdev = adapter->pdev;
	unsigned long size;
	unsigned int i;

	if (!rx_ring->buffer_info)
		return;
	/* Free all the Rx ring sk_buffs */
	for (i = 0; i < rx_ring->count; i++) {
		buffer_info = &rx_ring->buffer_info[i];
		if (buffer_info->dma) {
			if (adapter->rx_ps_hdr_size)
				pci_unmap_single(pdev, buffer_info->dma,
						 adapter->rx_ps_hdr_size,
						 PCI_DMA_FROMDEVICE);
			else
				pci_unmap_single(pdev, buffer_info->dma,
						 adapter->rx_buffer_len,
						 PCI_DMA_FROMDEVICE);
			buffer_info->dma = 0;
		}

		if (buffer_info->skb) {
			dev_kfree_skb(buffer_info->skb);
			buffer_info->skb = NULL;
		}
		if (buffer_info->page) {
			if (buffer_info->page_dma)
				pci_unmap_page(pdev, buffer_info->page_dma,
					       PAGE_SIZE / 2,
					       PCI_DMA_FROMDEVICE);
			put_page(buffer_info->page);
			buffer_info->page = NULL;
			buffer_info->page_dma = 0;
			buffer_info->page_offset = 0;
		}
	}

	size = sizeof(struct igb_buffer) * rx_ring->count;
	memset(rx_ring->buffer_info, 0, size);

	/* Zero out the descriptor ring */
	memset(rx_ring->desc, 0, rx_ring->size);

	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;

	writel(0, adapter->hw.hw_addr + rx_ring->head);
	writel(0, adapter->hw.hw_addr + rx_ring->tail);
}

/**
 * igb_clean_all_rx_rings - Free Rx Buffers for all queues
 * @adapter: board private structure
 **/
static void igb_clean_all_rx_rings(struct igb_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_rx_queues; i++)
		igb_clean_rx_ring(&adapter->rx_ring[i]);
}

/**
 * igb_set_mac - Change the Ethernet Address of the NIC
 * @netdev: network interface device structure
 * @p: pointer to an address structure
 *
 * Returns 0 on success, negative on failure
 **/
static int igb_set_mac(struct net_device *netdev, void *p)
{
	struct igb_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	memcpy(hw->mac.addr, addr->sa_data, netdev->addr_len);

	igb_rar_set(hw, hw->mac.addr, 0);
	igb_set_rah_pool(hw, adapter->vfs_allocated_count, 0);

	return 0;
}

/**
 * igb_set_rx_mode - Secondary Unicast, Multicast and Promiscuous mode set
 * @netdev: network interface device structure
 *
 * The set_rx_mode entry point is called whenever the unicast or multicast
 * address lists or the network interface flags are updated.  This routine is
 * responsible for configuring the hardware for proper unicast, multicast,
 * promiscuous mode, and all-multi behavior.
 **/
static void igb_set_rx_mode(struct net_device *netdev)
{
	struct igb_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;
	unsigned int rar_entries = hw->mac.rar_entry_count -
	                           (adapter->vfs_allocated_count + 1);
	struct dev_mc_list *mc_ptr = netdev->mc_list;
	u8  *mta_list = NULL;
	u32 rctl;
	int i;

	/* Check for Promiscuous and All Multicast modes */
	rctl = rd32(E1000_RCTL);

	if (netdev->flags & IFF_PROMISC) {
		rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
		rctl &= ~E1000_RCTL_VFE;
	} else {
		if (netdev->flags & IFF_ALLMULTI)
			rctl |= E1000_RCTL_MPE;
		else
			rctl &= ~E1000_RCTL_MPE;

		if (netdev->uc.count > rar_entries)
			rctl |= E1000_RCTL_UPE;
		else
			rctl &= ~E1000_RCTL_UPE;
		rctl |= E1000_RCTL_VFE;
	}
	wr32(E1000_RCTL, rctl);

	if (netdev->uc.count && rar_entries) {
		struct netdev_hw_addr *ha;
		list_for_each_entry(ha, &netdev->uc.list, list) {
			if (!rar_entries)
				break;
			igb_rar_set(hw, ha->addr, rar_entries);
			igb_set_rah_pool(hw, adapter->vfs_allocated_count,
			                 rar_entries);
			rar_entries--;
		}
	}
	/* write the addresses in reverse order to avoid write combining */
	for (; rar_entries > 0 ; rar_entries--) {
		wr32(E1000_RAH(rar_entries), 0);
		wr32(E1000_RAL(rar_entries), 0);
	}
	wrfl();

	if (!netdev->mc_count) {
		/* nothing to program, so clear mc list */
		igb_update_mc_addr_list(hw, NULL, 0);
		igb_restore_vf_multicasts(adapter);
		return;
	}

	mta_list = kzalloc(netdev->mc_count * 6, GFP_ATOMIC);
	if (!mta_list) {
		dev_err(&adapter->pdev->dev,
		        "failed to allocate multicast filter list\n");
		return;
	}

	/* The shared function expects a packed array of only addresses. */
	for (i = 0; i < netdev->mc_count; i++) {
		if (!mc_ptr)
			break;
		memcpy(mta_list + (i*ETH_ALEN), mc_ptr->dmi_addr, ETH_ALEN);
		mc_ptr = mc_ptr->next;
	}
	igb_update_mc_addr_list(hw, mta_list, i);
	kfree(mta_list);
	igb_restore_vf_multicasts(adapter);
}

/* Need to wait a few seconds after link up to get diagnostic information from
 * the phy */
static void igb_update_phy_info(unsigned long data)
{
	struct igb_adapter *adapter = (struct igb_adapter *) data;
	igb_get_phy_info(&adapter->hw);
}

/**
 * igb_has_link - check shared code for link and determine up/down
 * @adapter: pointer to driver private info
 **/
static bool igb_has_link(struct igb_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	bool link_active = false;
	s32 ret_val = 0;

	/* get_link_status is set on LSC (link status) interrupt or
	 * rx sequence error interrupt.  get_link_status will stay
	 * false until the e1000_check_for_link establishes link
	 * for copper adapters ONLY
	 */
	switch (hw->phy.media_type) {
	case e1000_media_type_copper:
		if (hw->mac.get_link_status) {
			ret_val = hw->mac.ops.check_for_link(hw);
			link_active = !hw->mac.get_link_status;
		} else {
			link_active = true;
		}
		break;
	case e1000_media_type_internal_serdes:
		ret_val = hw->mac.ops.check_for_link(hw);
		link_active = hw->mac.serdes_has_link;
		break;
	default:
	case e1000_media_type_unknown:
		break;
	}

	return link_active;
}

/**
 * igb_watchdog - Timer Call-back
 * @data: pointer to adapter cast into an unsigned long
 **/
static void igb_watchdog(unsigned long data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	/* Do the rest outside of interrupt context */
	schedule_work(&adapter->watchdog_task);
}

static void igb_watchdog_task(struct work_struct *work)
{
	struct igb_adapter *adapter = container_of(work,
					struct igb_adapter, watchdog_task);
	struct e1000_hw *hw = &adapter->hw;
	struct net_device *netdev = adapter->netdev;
	struct igb_ring *tx_ring = adapter->tx_ring;
	u32 link;
	u32 eics = 0;
	int i;

	link = igb_has_link(adapter);
	if ((netif_carrier_ok(netdev)) && link)
		goto link_up;

	if (link) {
		if (!netif_carrier_ok(netdev)) {
			u32 ctrl;
			hw->mac.ops.get_speed_and_duplex(&adapter->hw,
						   &adapter->link_speed,
						   &adapter->link_duplex);

			ctrl = rd32(E1000_CTRL);
			/* Links status message must follow this format */
			printk(KERN_INFO "igb: %s NIC Link is Up %d Mbps %s, "
				 "Flow Control: %s\n",
			         netdev->name,
				 adapter->link_speed,
				 adapter->link_duplex == FULL_DUPLEX ?
				 "Full Duplex" : "Half Duplex",
				 ((ctrl & E1000_CTRL_TFCE) && (ctrl &
				 E1000_CTRL_RFCE)) ? "RX/TX" : ((ctrl &
				 E1000_CTRL_RFCE) ? "RX" : ((ctrl &
				 E1000_CTRL_TFCE) ? "TX" : "None")));

			/* tweak tx_queue_len according to speed/duplex and
			 * adjust the timeout factor */
			netdev->tx_queue_len = adapter->tx_queue_len;
			adapter->tx_timeout_factor = 1;
			switch (adapter->link_speed) {
			case SPEED_10:
				netdev->tx_queue_len = 10;
				adapter->tx_timeout_factor = 14;
				break;
			case SPEED_100:
				netdev->tx_queue_len = 100;
				/* maybe add some timeout factor ? */
				break;
			}

			netif_carrier_on(netdev);

			igb_ping_all_vfs(adapter);

			/* link state has changed, schedule phy info update */
			if (!test_bit(__IGB_DOWN, &adapter->state))
				mod_timer(&adapter->phy_info_timer,
					  round_jiffies(jiffies + 2 * HZ));
		}
	} else {
		if (netif_carrier_ok(netdev)) {
			adapter->link_speed = 0;
			adapter->link_duplex = 0;
			/* Links status message must follow this format */
			printk(KERN_INFO "igb: %s NIC Link is Down\n",
			       netdev->name);
			netif_carrier_off(netdev);

			igb_ping_all_vfs(adapter);

			/* link state has changed, schedule phy info update */
			if (!test_bit(__IGB_DOWN, &adapter->state))
				mod_timer(&adapter->phy_info_timer,
					  round_jiffies(jiffies + 2 * HZ));
		}
	}

link_up:
	igb_update_stats(adapter);

	hw->mac.tx_packet_delta = adapter->stats.tpt - adapter->tpt_old;
	adapter->tpt_old = adapter->stats.tpt;
	hw->mac.collision_delta = adapter->stats.colc - adapter->colc_old;
	adapter->colc_old = adapter->stats.colc;

	adapter->gorc = adapter->stats.gorc - adapter->gorc_old;
	adapter->gorc_old = adapter->stats.gorc;
	adapter->gotc = adapter->stats.gotc - adapter->gotc_old;
	adapter->gotc_old = adapter->stats.gotc;

	igb_update_adaptive(&adapter->hw);

	if (!netif_carrier_ok(netdev)) {
		if (igb_desc_unused(tx_ring) + 1 < tx_ring->count) {
			/* We've lost link, so the controller stops DMA,
			 * but we've got queued Tx work that's never going
			 * to get done, so reset controller to flush Tx.
			 * (Do the reset outside of interrupt context). */
			adapter->tx_timeout_count++;
			schedule_work(&adapter->reset_task);
			/* return immediately since reset is imminent */
			return;
		}
	}

	/* Cause software interrupt to ensure rx ring is cleaned */
	if (adapter->msix_entries) {
		for (i = 0; i < adapter->num_rx_queues; i++)
			eics |= adapter->rx_ring[i].eims_value;
		wr32(E1000_EICS, eics);
	} else {
		wr32(E1000_ICS, E1000_ICS_RXDMT0);
	}

	/* Force detection of hung controller every watchdog period */
	tx_ring->detect_tx_hung = true;

	/* Reset the timer */
	if (!test_bit(__IGB_DOWN, &adapter->state))
		mod_timer(&adapter->watchdog_timer,
			  round_jiffies(jiffies + 2 * HZ));
}

enum latency_range {
	lowest_latency = 0,
	low_latency = 1,
	bulk_latency = 2,
	latency_invalid = 255
};


/**
 * igb_update_ring_itr - update the dynamic ITR value based on packet size
 *
 *      Stores a new ITR value based on strictly on packet size.  This
 *      algorithm is less sophisticated than that used in igb_update_itr,
 *      due to the difficulty of synchronizing statistics across multiple
 *      receive rings.  The divisors and thresholds used by this fuction
 *      were determined based on theoretical maximum wire speed and testing
 *      data, in order to minimize response time while increasing bulk
 *      throughput.
 *      This functionality is controlled by the InterruptThrottleRate module
 *      parameter (see igb_param.c)
 *      NOTE:  This function is called only when operating in a multiqueue
 *             receive environment.
 * @rx_ring: pointer to ring
 **/
static void igb_update_ring_itr(struct igb_ring *rx_ring)
{
	int new_val = rx_ring->itr_val;
	int avg_wire_size = 0;
	struct igb_adapter *adapter = rx_ring->adapter;

	if (!rx_ring->total_packets)
		goto clear_counts; /* no packets, so don't do anything */

	/* For non-gigabit speeds, just fix the interrupt rate at 4000
	 * ints/sec - ITR timer value of 120 ticks.
	 */
	if (adapter->link_speed != SPEED_1000) {
		new_val = 120;
		goto set_itr_val;
	}
	avg_wire_size = rx_ring->total_bytes / rx_ring->total_packets;

	/* Add 24 bytes to size to account for CRC, preamble, and gap */
	avg_wire_size += 24;

	/* Don't starve jumbo frames */
	avg_wire_size = min(avg_wire_size, 3000);

	/* Give a little boost to mid-size frames */
	if ((avg_wire_size > 300) && (avg_wire_size < 1200))
		new_val = avg_wire_size / 3;
	else
		new_val = avg_wire_size / 2;

set_itr_val:
	if (new_val != rx_ring->itr_val) {
		rx_ring->itr_val = new_val;
		rx_ring->set_itr = 1;
	}
clear_counts:
	rx_ring->total_bytes = 0;
	rx_ring->total_packets = 0;
}

/**
 * igb_update_itr - update the dynamic ITR value based on statistics
 *      Stores a new ITR value based on packets and byte
 *      counts during the last interrupt.  The advantage of per interrupt
 *      computation is faster updates and more accurate ITR for the current
 *      traffic pattern.  Constants in this function were computed
 *      based on theoretical maximum wire speed and thresholds were set based
 *      on testing data as well as attempting to minimize response time
 *      while increasing bulk throughput.
 *      this functionality is controlled by the InterruptThrottleRate module
 *      parameter (see igb_param.c)
 *      NOTE:  These calculations are only valid when operating in a single-
 *             queue environment.
 * @adapter: pointer to adapter
 * @itr_setting: current adapter->itr
 * @packets: the number of packets during this measurement interval
 * @bytes: the number of bytes during this measurement interval
 **/
static unsigned int igb_update_itr(struct igb_adapter *adapter, u16 itr_setting,
				   int packets, int bytes)
{
	unsigned int retval = itr_setting;

	if (packets == 0)
		goto update_itr_done;

	switch (itr_setting) {
	case lowest_latency:
		/* handle TSO and jumbo frames */
		if (bytes/packets > 8000)
			retval = bulk_latency;
		else if ((packets < 5) && (bytes > 512))
			retval = low_latency;
		break;
	case low_latency:  /* 50 usec aka 20000 ints/s */
		if (bytes > 10000) {
			/* this if handles the TSO accounting */
			if (bytes/packets > 8000) {
				retval = bulk_latency;
			} else if ((packets < 10) || ((bytes/packets) > 1200)) {
				retval = bulk_latency;
			} else if ((packets > 35)) {
				retval = lowest_latency;
			}
		} else if (bytes/packets > 2000) {
			retval = bulk_latency;
		} else if (packets <= 2 && bytes < 512) {
			retval = lowest_latency;
		}
		break;
	case bulk_latency: /* 250 usec aka 4000 ints/s */
		if (bytes > 25000) {
			if (packets > 35)
				retval = low_latency;
		} else if (bytes < 1500) {
			retval = low_latency;
		}
		break;
	}

update_itr_done:
	return retval;
}

static void igb_set_itr(struct igb_adapter *adapter)
{
	u16 current_itr;
	u32 new_itr = adapter->itr;

	/* for non-gigabit speeds, just fix the interrupt rate at 4000 */
	if (adapter->link_speed != SPEED_1000) {
		current_itr = 0;
		new_itr = 4000;
		goto set_itr_now;
	}

	adapter->rx_itr = igb_update_itr(adapter,
				    adapter->rx_itr,
				    adapter->rx_ring->total_packets,
				    adapter->rx_ring->total_bytes);

	if (adapter->rx_ring->buddy) {
		adapter->tx_itr = igb_update_itr(adapter,
					    adapter->tx_itr,
					    adapter->tx_ring->total_packets,
					    adapter->tx_ring->total_bytes);
		current_itr = max(adapter->rx_itr, adapter->tx_itr);
	} else {
		current_itr = adapter->rx_itr;
	}

	/* conservative mode (itr 3) eliminates the lowest_latency setting */
	if (adapter->itr_setting == 3 && current_itr == lowest_latency)
		current_itr = low_latency;

	switch (current_itr) {
	/* counts and packets in update_itr are dependent on these numbers */
	case lowest_latency:
		new_itr = 56;  /* aka 70,000 ints/sec */
		break;
	case low_latency:
		new_itr = 196; /* aka 20,000 ints/sec */
		break;
	case bulk_latency:
		new_itr = 980; /* aka 4,000 ints/sec */
		break;
	default:
		break;
	}

set_itr_now:
	adapter->rx_ring->total_bytes = 0;
	adapter->rx_ring->total_packets = 0;
	if (adapter->rx_ring->buddy) {
		adapter->rx_ring->buddy->total_bytes = 0;
		adapter->rx_ring->buddy->total_packets = 0;
	}

	if (new_itr != adapter->itr) {
		/* this attempts to bias the interrupt rate towards Bulk
		 * by adding intermediate steps when interrupt rate is
		 * increasing */
		new_itr = new_itr > adapter->itr ?
			     max((new_itr * adapter->itr) /
			         (new_itr + (adapter->itr >> 2)), new_itr) :
			     new_itr;
		/* Don't write the value here; it resets the adapter's
		 * internal timer, and causes us to delay far longer than
		 * we should between interrupts.  Instead, we write the ITR
		 * value at the beginning of the next interrupt so the timing
		 * ends up being correct.
		 */
		adapter->itr = new_itr;
		adapter->rx_ring->itr_val = new_itr;
		adapter->rx_ring->set_itr = 1;
	}

	return;
}


#define IGB_TX_FLAGS_CSUM		0x00000001
#define IGB_TX_FLAGS_VLAN		0x00000002
#define IGB_TX_FLAGS_TSO		0x00000004
#define IGB_TX_FLAGS_IPV4		0x00000008
#define IGB_TX_FLAGS_TSTAMP             0x00000010
#define IGB_TX_FLAGS_VLAN_MASK	0xffff0000
#define IGB_TX_FLAGS_VLAN_SHIFT	16

static inline int igb_tso_adv(struct igb_adapter *adapter,
			      struct igb_ring *tx_ring,
			      struct sk_buff *skb, u32 tx_flags, u8 *hdr_len)
{
	struct e1000_adv_tx_context_desc *context_desc;
	unsigned int i;
	int err;
	struct igb_buffer *buffer_info;
	u32 info = 0, tu_cmd = 0;
	u32 mss_l4len_idx, l4len;
	*hdr_len = 0;

	if (skb_header_cloned(skb)) {
		err = pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
		if (err)
			return err;
	}

	l4len = tcp_hdrlen(skb);
	*hdr_len += l4len;

	if (skb->protocol == htons(ETH_P_IP)) {
		struct iphdr *iph = ip_hdr(skb);
		iph->tot_len = 0;
		iph->check = 0;
		tcp_hdr(skb)->check = ~csum_tcpudp_magic(iph->saddr,
							 iph->daddr, 0,
							 IPPROTO_TCP,
							 0);
	} else if (skb_shinfo(skb)->gso_type == SKB_GSO_TCPV6) {
		ipv6_hdr(skb)->payload_len = 0;
		tcp_hdr(skb)->check = ~csum_ipv6_magic(&ipv6_hdr(skb)->saddr,
						       &ipv6_hdr(skb)->daddr,
						       0, IPPROTO_TCP, 0);
	}

	i = tx_ring->next_to_use;

	buffer_info = &tx_ring->buffer_info[i];
	context_desc = E1000_TX_CTXTDESC_ADV(*tx_ring, i);
	/* VLAN MACLEN IPLEN */
	if (tx_flags & IGB_TX_FLAGS_VLAN)
		info |= (tx_flags & IGB_TX_FLAGS_VLAN_MASK);
	info |= (skb_network_offset(skb) << E1000_ADVTXD_MACLEN_SHIFT);
	*hdr_len += skb_network_offset(skb);
	info |= skb_network_header_len(skb);
	*hdr_len += skb_network_header_len(skb);
	context_desc->vlan_macip_lens = cpu_to_le32(info);

	/* ADV DTYP TUCMD MKRLOC/ISCSIHEDLEN */
	tu_cmd |= (E1000_TXD_CMD_DEXT | E1000_ADVTXD_DTYP_CTXT);

	if (skb->protocol == htons(ETH_P_IP))
		tu_cmd |= E1000_ADVTXD_TUCMD_IPV4;
	tu_cmd |= E1000_ADVTXD_TUCMD_L4T_TCP;

	context_desc->type_tucmd_mlhl = cpu_to_le32(tu_cmd);

	/* MSS L4LEN IDX */
	mss_l4len_idx = (skb_shinfo(skb)->gso_size << E1000_ADVTXD_MSS_SHIFT);
	mss_l4len_idx |= (l4len << E1000_ADVTXD_L4LEN_SHIFT);

	/* For 82575, context index must be unique per ring. */
	if (adapter->flags & IGB_FLAG_NEED_CTX_IDX)
		mss_l4len_idx |= tx_ring->queue_index << 4;

	context_desc->mss_l4len_idx = cpu_to_le32(mss_l4len_idx);
	context_desc->seqnum_seed = 0;

	buffer_info->time_stamp = jiffies;
	buffer_info->next_to_watch = i;
	buffer_info->dma = 0;
	i++;
	if (i == tx_ring->count)
		i = 0;

	tx_ring->next_to_use = i;

	return true;
}

static inline bool igb_tx_csum_adv(struct igb_adapter *adapter,
					struct igb_ring *tx_ring,
					struct sk_buff *skb, u32 tx_flags)
{
	struct e1000_adv_tx_context_desc *context_desc;
	unsigned int i;
	struct igb_buffer *buffer_info;
	u32 info = 0, tu_cmd = 0;

	if ((skb->ip_summed == CHECKSUM_PARTIAL) ||
	    (tx_flags & IGB_TX_FLAGS_VLAN)) {
		i = tx_ring->next_to_use;
		buffer_info = &tx_ring->buffer_info[i];
		context_desc = E1000_TX_CTXTDESC_ADV(*tx_ring, i);

		if (tx_flags & IGB_TX_FLAGS_VLAN)
			info |= (tx_flags & IGB_TX_FLAGS_VLAN_MASK);
		info |= (skb_network_offset(skb) << E1000_ADVTXD_MACLEN_SHIFT);
		if (skb->ip_summed == CHECKSUM_PARTIAL)
			info |= skb_network_header_len(skb);

		context_desc->vlan_macip_lens = cpu_to_le32(info);

		tu_cmd |= (E1000_TXD_CMD_DEXT | E1000_ADVTXD_DTYP_CTXT);

		if (skb->ip_summed == CHECKSUM_PARTIAL) {
			__be16 protocol;

			if (skb->protocol == cpu_to_be16(ETH_P_8021Q)) {
				const struct vlan_ethhdr *vhdr =
				          (const struct vlan_ethhdr*)skb->data;

				protocol = vhdr->h_vlan_encapsulated_proto;
			} else {
				protocol = skb->protocol;
			}

			switch (protocol) {
			case cpu_to_be16(ETH_P_IP):
				tu_cmd |= E1000_ADVTXD_TUCMD_IPV4;
				if (ip_hdr(skb)->protocol == IPPROTO_TCP)
					tu_cmd |= E1000_ADVTXD_TUCMD_L4T_TCP;
				else if (ip_hdr(skb)->protocol == IPPROTO_SCTP)
					tu_cmd |= E1000_ADVTXD_TUCMD_L4T_SCTP;
				break;
			case cpu_to_be16(ETH_P_IPV6):
				/* XXX what about other V6 headers?? */
				if (ipv6_hdr(skb)->nexthdr == IPPROTO_TCP)
					tu_cmd |= E1000_ADVTXD_TUCMD_L4T_TCP;
				else if (ipv6_hdr(skb)->nexthdr == IPPROTO_SCTP)
					tu_cmd |= E1000_ADVTXD_TUCMD_L4T_SCTP;
				break;
			default:
				if (unlikely(net_ratelimit()))
					dev_warn(&adapter->pdev->dev,
					    "partial checksum but proto=%x!\n",
					    skb->protocol);
				break;
			}
		}

		context_desc->type_tucmd_mlhl = cpu_to_le32(tu_cmd);
		context_desc->seqnum_seed = 0;
		if (adapter->flags & IGB_FLAG_NEED_CTX_IDX)
			context_desc->mss_l4len_idx =
				cpu_to_le32(tx_ring->queue_index << 4);
		else
			context_desc->mss_l4len_idx = 0;

		buffer_info->time_stamp = jiffies;
		buffer_info->next_to_watch = i;
		buffer_info->dma = 0;

		i++;
		if (i == tx_ring->count)
			i = 0;
		tx_ring->next_to_use = i;

		return true;
	}
	return false;
}

#define IGB_MAX_TXD_PWR	16
#define IGB_MAX_DATA_PER_TXD	(1<<IGB_MAX_TXD_PWR)

static inline int igb_tx_map_adv(struct igb_adapter *adapter,
				 struct igb_ring *tx_ring, struct sk_buff *skb,
				 unsigned int first)
{
	struct igb_buffer *buffer_info;
	unsigned int len = skb_headlen(skb);
	unsigned int count = 0, i;
	unsigned int f;
	dma_addr_t *map;

	i = tx_ring->next_to_use;

	if (skb_dma_map(&adapter->pdev->dev, skb, DMA_TO_DEVICE)) {
		dev_err(&adapter->pdev->dev, "TX DMA map failed\n");
		return 0;
	}

	map = skb_shinfo(skb)->dma_maps;

	buffer_info = &tx_ring->buffer_info[i];
	BUG_ON(len >= IGB_MAX_DATA_PER_TXD);
	buffer_info->length = len;
	/* set time_stamp *before* dma to help avoid a possible race */
	buffer_info->time_stamp = jiffies;
	buffer_info->next_to_watch = i;
	buffer_info->dma = skb_shinfo(skb)->dma_head;

	for (f = 0; f < skb_shinfo(skb)->nr_frags; f++) {
		struct skb_frag_struct *frag;

		i++;
		if (i == tx_ring->count)
			i = 0;

		frag = &skb_shinfo(skb)->frags[f];
		len = frag->size;

		buffer_info = &tx_ring->buffer_info[i];
		BUG_ON(len >= IGB_MAX_DATA_PER_TXD);
		buffer_info->length = len;
		buffer_info->time_stamp = jiffies;
		buffer_info->next_to_watch = i;
		buffer_info->dma = map[count];
		count++;
	}

	tx_ring->buffer_info[i].skb = skb;
	tx_ring->buffer_info[first].next_to_watch = i;

	return count + 1;
}

static inline void igb_tx_queue_adv(struct igb_adapter *adapter,
				    struct igb_ring *tx_ring,
				    int tx_flags, int count, u32 paylen,
				    u8 hdr_len)
{
	union e1000_adv_tx_desc *tx_desc = NULL;
	struct igb_buffer *buffer_info;
	u32 olinfo_status = 0, cmd_type_len;
	unsigned int i;

	cmd_type_len = (E1000_ADVTXD_DTYP_DATA | E1000_ADVTXD_DCMD_IFCS |
			E1000_ADVTXD_DCMD_DEXT);

	if (tx_flags & IGB_TX_FLAGS_VLAN)
		cmd_type_len |= E1000_ADVTXD_DCMD_VLE;

	if (tx_flags & IGB_TX_FLAGS_TSTAMP)
		cmd_type_len |= E1000_ADVTXD_MAC_TSTAMP;

	if (tx_flags & IGB_TX_FLAGS_TSO) {
		cmd_type_len |= E1000_ADVTXD_DCMD_TSE;

		/* insert tcp checksum */
		olinfo_status |= E1000_TXD_POPTS_TXSM << 8;

		/* insert ip checksum */
		if (tx_flags & IGB_TX_FLAGS_IPV4)
			olinfo_status |= E1000_TXD_POPTS_IXSM << 8;

	} else if (tx_flags & IGB_TX_FLAGS_CSUM) {
		olinfo_status |= E1000_TXD_POPTS_TXSM << 8;
	}

	if ((adapter->flags & IGB_FLAG_NEED_CTX_IDX) &&
	    (tx_flags & (IGB_TX_FLAGS_CSUM | IGB_TX_FLAGS_TSO |
			 IGB_TX_FLAGS_VLAN)))
		olinfo_status |= tx_ring->queue_index << 4;

	olinfo_status |= ((paylen - hdr_len) << E1000_ADVTXD_PAYLEN_SHIFT);

	i = tx_ring->next_to_use;
	while (count--) {
		buffer_info = &tx_ring->buffer_info[i];
		tx_desc = E1000_TX_DESC_ADV(*tx_ring, i);
		tx_desc->read.buffer_addr = cpu_to_le64(buffer_info->dma);
		tx_desc->read.cmd_type_len =
			cpu_to_le32(cmd_type_len | buffer_info->length);
		tx_desc->read.olinfo_status = cpu_to_le32(olinfo_status);
		i++;
		if (i == tx_ring->count)
			i = 0;
	}

	tx_desc->read.cmd_type_len |= cpu_to_le32(adapter->txd_cmd);
	/* Force memory writes to complete before letting h/w
	 * know there are new descriptors to fetch.  (Only
	 * applicable for weak-ordered memory model archs,
	 * such as IA-64). */
	wmb();

	tx_ring->next_to_use = i;
	writel(i, adapter->hw.hw_addr + tx_ring->tail);
	/* we need this if more than one processor can write to our tail
	 * at a time, it syncronizes IO on IA64/Altix systems */
	mmiowb();
}

static int __igb_maybe_stop_tx(struct net_device *netdev,
			       struct igb_ring *tx_ring, int size)
{
	struct igb_adapter *adapter = netdev_priv(netdev);

	netif_stop_subqueue(netdev, tx_ring->queue_index);

	/* Herbert's original patch had:
	 *  smp_mb__after_netif_stop_queue();
	 * but since that doesn't exist yet, just open code it. */
	smp_mb();

	/* We need to check again in a case another CPU has just
	 * made room available. */
	if (igb_desc_unused(tx_ring) < size)
		return -EBUSY;

	/* A reprieve! */
	netif_wake_subqueue(netdev, tx_ring->queue_index);
	++adapter->restart_queue;
	return 0;
}

static int igb_maybe_stop_tx(struct net_device *netdev,
			     struct igb_ring *tx_ring, int size)
{
	if (igb_desc_unused(tx_ring) >= size)
		return 0;
	return __igb_maybe_stop_tx(netdev, tx_ring, size);
}

static netdev_tx_t igb_xmit_frame_ring_adv(struct sk_buff *skb,
					   struct net_device *netdev,
					   struct igb_ring *tx_ring)
{
	struct igb_adapter *adapter = netdev_priv(netdev);
	unsigned int first;
	unsigned int tx_flags = 0;
	u8 hdr_len = 0;
	int count = 0;
	int tso = 0;
	union skb_shared_tx *shtx;

	if (test_bit(__IGB_DOWN, &adapter->state)) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (skb->len <= 0) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	/* need: 1 descriptor per page,
	 *       + 2 desc gap to keep tail from touching head,
	 *       + 1 desc for skb->data,
	 *       + 1 desc for context descriptor,
	 * otherwise try next time */
	if (igb_maybe_stop_tx(netdev, tx_ring, skb_shinfo(skb)->nr_frags + 4)) {
		/* this is a hard error */
		return NETDEV_TX_BUSY;
	}

	/*
	 * TODO: check that there currently is no other packet with
	 * time stamping in the queue
	 *
	 * When doing time stamping, keep the connection to the socket
	 * a while longer: it is still needed by skb_hwtstamp_tx(),
	 * called either in igb_tx_hwtstamp() or by our caller when
	 * doing software time stamping.
	 */
	shtx = skb_tx(skb);
	if (unlikely(shtx->hardware)) {
		shtx->in_progress = 1;
		tx_flags |= IGB_TX_FLAGS_TSTAMP;
	}

	if (adapter->vlgrp && vlan_tx_tag_present(skb)) {
		tx_flags |= IGB_TX_FLAGS_VLAN;
		tx_flags |= (vlan_tx_tag_get(skb) << IGB_TX_FLAGS_VLAN_SHIFT);
	}

	if (skb->protocol == htons(ETH_P_IP))
		tx_flags |= IGB_TX_FLAGS_IPV4;

	first = tx_ring->next_to_use;
	tso = skb_is_gso(skb) ? igb_tso_adv(adapter, tx_ring, skb, tx_flags,
					      &hdr_len) : 0;

	if (tso < 0) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (tso)
		tx_flags |= IGB_TX_FLAGS_TSO;
	else if (igb_tx_csum_adv(adapter, tx_ring, skb, tx_flags) &&
	         (skb->ip_summed == CHECKSUM_PARTIAL))
		tx_flags |= IGB_TX_FLAGS_CSUM;

	/*
	 * count reflects descriptors mapped, if 0 then mapping error
	 * has occured and we need to rewind the descriptor queue
	 */
	count = igb_tx_map_adv(adapter, tx_ring, skb, first);

	if (count) {
		igb_tx_queue_adv(adapter, tx_ring, tx_flags, count,
			         skb->len, hdr_len);
		/* Make sure there is space in the ring for the next send. */
		igb_maybe_stop_tx(netdev, tx_ring, MAX_SKB_FRAGS + 4);
	} else {
		dev_kfree_skb_any(skb);
		tx_ring->buffer_info[first].time_stamp = 0;
		tx_ring->next_to_use = first;
	}

	return NETDEV_TX_OK;
}

static netdev_tx_t igb_xmit_frame_adv(struct sk_buff *skb,
				      struct net_device *netdev)
{
	struct igb_adapter *adapter = netdev_priv(netdev);
	struct igb_ring *tx_ring;

	int r_idx = 0;
	r_idx = skb->queue_mapping & (IGB_ABS_MAX_TX_QUEUES - 1);
	tx_ring = adapter->multi_tx_table[r_idx];

	/* This goes back to the question of how to logically map a tx queue
	 * to a flow.  Right now, performance is impacted slightly negatively
	 * if using multiple tx queues.  If the stack breaks away from a
	 * single qdisc implementation, we can look at this again. */
	return igb_xmit_frame_ring_adv(skb, netdev, tx_ring);
}

/**
 * igb_tx_timeout - Respond to a Tx Hang
 * @netdev: network interface device structure
 **/
static void igb_tx_timeout(struct net_device *netdev)
{
	struct igb_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;

	/* Do the reset outside of interrupt context */
	adapter->tx_timeout_count++;
	schedule_work(&adapter->reset_task);
	wr32(E1000_EICS,
	     (adapter->eims_enable_mask & ~adapter->eims_other));
}

static void igb_reset_task(struct work_struct *work)
{
	struct igb_adapter *adapter;
	adapter = container_of(work, struct igb_adapter, reset_task);

	igb_reinit_locked(adapter);
}

/**
 * igb_get_stats - Get System Network Statistics
 * @netdev: network interface device structure
 *
 * Returns the address of the device statistics structure.
 * The statistics are actually updated from the timer callback.
 **/
static struct net_device_stats *igb_get_stats(struct net_device *netdev)
{
	struct igb_adapter *adapter = netdev_priv(netdev);

	/* only return the current stats */
	return &adapter->net_stats;
}

/**
 * igb_change_mtu - Change the Maximum Transfer Unit
 * @netdev: network interface device structure
 * @new_mtu: new value for maximum frame size
 *
 * Returns 0 on success, negative on failure
 **/
static int igb_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct igb_adapter *adapter = netdev_priv(netdev);
	int max_frame = new_mtu + ETH_HLEN + ETH_FCS_LEN;

	if ((max_frame < ETH_ZLEN + ETH_FCS_LEN) ||
	    (max_frame > MAX_JUMBO_FRAME_SIZE)) {
		dev_err(&adapter->pdev->dev, "Invalid MTU setting\n");
		return -EINVAL;
	}

	if (max_frame > MAX_STD_JUMBO_FRAME_SIZE) {
		dev_err(&adapter->pdev->dev, "MTU > 9216 not supported.\n");
		return -EINVAL;
	}

	while (test_and_set_bit(__IGB_RESETTING, &adapter->state))
		msleep(1);

	/* igb_down has a dependency on max_frame_size */
	adapter->max_frame_size = max_frame;
	if (netif_running(netdev))
		igb_down(adapter);

	/* NOTE: netdev_alloc_skb reserves 16 bytes, and typically NET_IP_ALIGN
	 * means we reserve 2 more, this pushes us to allocate from the next
	 * larger slab size.
	 * i.e. RXBUFFER_2048 --> size-4096 slab
	 */

	if (max_frame <= IGB_RXBUFFER_256)
		adapter->rx_buffer_len = IGB_RXBUFFER_256;
	else if (max_frame <= IGB_RXBUFFER_512)
		adapter->rx_buffer_len = IGB_RXBUFFER_512;
	else if (max_frame <= IGB_RXBUFFER_1024)
		adapter->rx_buffer_len = IGB_RXBUFFER_1024;
	else if (max_frame <= IGB_RXBUFFER_2048)
		adapter->rx_buffer_len = IGB_RXBUFFER_2048;
	else
#if (PAGE_SIZE / 2) > IGB_RXBUFFER_16384
		adapter->rx_buffer_len = IGB_RXBUFFER_16384;
#else
		adapter->rx_buffer_len = PAGE_SIZE / 2;
#endif

	/* if sr-iov is enabled we need to force buffer size to 1K or larger */
	if (adapter->vfs_allocated_count &&
	    (adapter->rx_buffer_len < IGB_RXBUFFER_1024))
		adapter->rx_buffer_len = IGB_RXBUFFER_1024;

	/* adjust allocation if LPE protects us, and we aren't using SBP */
	if ((max_frame == ETH_FRAME_LEN + ETH_FCS_LEN) ||
	     (max_frame == MAXIMUM_ETHERNET_VLAN_SIZE))
		adapter->rx_buffer_len = MAXIMUM_ETHERNET_VLAN_SIZE;

	dev_info(&adapter->pdev->dev, "changing MTU from %d to %d\n",
		 netdev->mtu, new_mtu);
	netdev->mtu = new_mtu;

	if (netif_running(netdev))
		igb_up(adapter);
	else
		igb_reset(adapter);

	clear_bit(__IGB_RESETTING, &adapter->state);

	return 0;
}

/**
 * igb_update_stats - Update the board statistics counters
 * @adapter: board private structure
 **/

void igb_update_stats(struct igb_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct pci_dev *pdev = adapter->pdev;
	u16 phy_tmp;

#define PHY_IDLE_ERROR_COUNT_MASK 0x00FF

	/*
	 * Prevent stats update while adapter is being reset, or if the pci
	 * connection is down.
	 */
	if (adapter->link_speed == 0)
		return;
	if (pci_channel_offline(pdev))
		return;

	adapter->stats.crcerrs += rd32(E1000_CRCERRS);
	adapter->stats.gprc += rd32(E1000_GPRC);
	adapter->stats.gorc += rd32(E1000_GORCL);
	rd32(E1000_GORCH); /* clear GORCL */
	adapter->stats.bprc += rd32(E1000_BPRC);
	adapter->stats.mprc += rd32(E1000_MPRC);
	adapter->stats.roc += rd32(E1000_ROC);

	adapter->stats.prc64 += rd32(E1000_PRC64);
	adapter->stats.prc127 += rd32(E1000_PRC127);
	adapter->stats.prc255 += rd32(E1000_PRC255);
	adapter->stats.prc511 += rd32(E1000_PRC511);
	adapter->stats.prc1023 += rd32(E1000_PRC1023);
	adapter->stats.prc1522 += rd32(E1000_PRC1522);
	adapter->stats.symerrs += rd32(E1000_SYMERRS);
	adapter->stats.sec += rd32(E1000_SEC);

	adapter->stats.mpc += rd32(E1000_MPC);
	adapter->stats.scc += rd32(E1000_SCC);
	adapter->stats.ecol += rd32(E1000_ECOL);
	adapter->stats.mcc += rd32(E1000_MCC);
	adapter->stats.latecol += rd32(E1000_LATECOL);
	adapter->stats.dc += rd32(E1000_DC);
	adapter->stats.rlec += rd32(E1000_RLEC);
	adapter->stats.xonrxc += rd32(E1000_XONRXC);
	adapter->stats.xontxc += rd32(E1000_XONTXC);
	adapter->stats.xoffrxc += rd32(E1000_XOFFRXC);
	adapter->stats.xofftxc += rd32(E1000_XOFFTXC);
	adapter->stats.fcruc += rd32(E1000_FCRUC);
	adapter->stats.gptc += rd32(E1000_GPTC);
	adapter->stats.gotc += rd32(E1000_GOTCL);
	rd32(E1000_GOTCH); /* clear GOTCL */
	adapter->stats.rnbc += rd32(E1000_RNBC);
	adapter->stats.ruc += rd32(E1000_RUC);
	adapter->stats.rfc += rd32(E1000_RFC);
	adapter->stats.rjc += rd32(E1000_RJC);
	adapter->stats.tor += rd32(E1000_TORH);
	adapter->stats.tot += rd32(E1000_TOTH);
	adapter->stats.tpr += rd32(E1000_TPR);

	adapter->stats.ptc64 += rd32(E1000_PTC64);
	adapter->stats.ptc127 += rd32(E1000_PTC127);
	adapter->stats.ptc255 += rd32(E1000_PTC255);
	adapter->stats.ptc511 += rd32(E1000_PTC511);
	adapter->stats.ptc1023 += rd32(E1000_PTC****)/*********************522*******************522);
*****************mptc***************MPTC*******************bdriver
  CopyrightBc) 200
	/* used for *****ive IFS */

	hw->mac.tx_packet_delta *************TPT*******************tpt****ribute it and/or modify;stribute icollisionmodify it
  under thCOL 2007-2009 Intel Corcoliver
e,
  version 2, as publabit Ethernet Linuxalgnerriver
  CopyrightALGNERR 2007-2009 Intel Corrxe useful, but WITHORX ANY WARRANTY; without tncrs***************TNCRSf MERCHANTABILITY orscriver
  CopyrightTSC) 2007-2009 Intel Corhe GfNU General Public LiFram is***************iaiver
  CopyrightIA 2007-2009 Intel CoricrxoNU General PublicCRXOLicense along with
  thidriver
  Copyright, wrc) 2007-2009 Intel Cor thiaoundation, Inc.,
  51AFranklin St - Fifth Flot Foundation, Inc.,
  T1 Franklin St - Fifth Flotr, Boston, MA 02110-1T01 USA.

  The full GNU Genqeon in
  the file calQEd "COPYING".

  Contact Imion in
  the file calQMFranklin St - Fifth Floordvel@lists.sourceforgRXDet>
  is frFill out the OS ****istics structureredied a copy net_ Linux ulticast =are; yrnet Linux drc/*********************sion 2, as******************.

  HillsbRx Errorsredistif (ribute itype != e*****82575) {
		u32 rqdpc_tmp;h>
#64clude <lotal = 0ux/pint iux/p.h>
ead OR 9drop****ats per RX queue.  Notice RQDPC (Receive
		 * Qincl Drop Pd/or  Count)linux/ionly gets incremented, ifclude 7124DROP_EN but it set (inlinuxSRRCTL registeroftwathatclude #incl).  Ifx/mii.h>
#it is NOTde <,linuux/ethsome wux/if_vlaequivalent c>
#ii.h>stored in RNBC (notipv6.#incl basis).clude Also notelinuxude x/delay.h>due to lack of availablnclude descriptorsde <li/
		ftwa(iude < i <**********num_rx_#incls; i++oc.h>
	lude <lin it
  under thhecks(i)) & 0xFFFux/ped a copy rx_ring[i].rx*******ude <l****ude <linux/p"1.3.16-inclu+***********iver_version[] = DRV_VERSux/p}
char igb_dr**********rx_fifo_eclude N;
staticincl;
	}HillsbNethe<linux.h>
#in No Buffers.h>
#incis anf_et.";
exact
B_DCA#include <aslinuxhardware FIFO might savher.h>
ay.  Thatst stroneifde7124reasonoftwasavingcludin tatic const ch,infoi00_8257potentially

stat truh>
#in.t st**********************tatic const chaver_string[]75 },
	nbtypes.h>
LEC onpci-asnewergb_info_tbcan be_tstorrect so build_82575ur own ver2, a
#inedard_RUC and ROC_82576), board_82575 },
	{ nst char RANTY; without even thenet Network e <linurcerCE(I, board_82575 },ll be usefVDEVICE(INTEL, E10rucID_82576_SERDES_QUrs prVDEVICE(INTEL, E100exterr********/

#include <lrx_length6_SERDES), board_82575 },
EVICnst 		 d_825(INTEL, E1000_DEV_d_82575 },
	{ PCI_VDEVICEcrc6_SERDES), board_82575 },00_DEV_d_82575 },
	{ PCI_VDEVICEframe6_SERDES), board_82575 },ll be usd_82575 },
	{ PCI_VDEVICEmissed6_SERDES), board_82575 },mptypes.h>T#include <lit Network Driver";
st76_SERDES), board_82575 },ecolR), boad_825775 },
	{ PCI_VDElattati********/

#include <ltx_aborttry */
	{0, }
};

MODULE_DE(struct igb_adapter *);
statiwindow6_SERDES), board_82575 },ces(struct igb_adapter *);
staticarrier6_SERDES), board_82575 },r
  FE_TABLE(pcp6_cped needsaer.be maintained elsewhe******is frPhy Snux/ipci_x/init.hphy.media_nclud= <linux/ruct pci_d_c *);roc.h>
x/in(*********link_speedev *SPEED_****) &&clud  (!igb_read_phy_reg(hw, PHYove(sT_STATUS, &
statmp))RSION "igb_ada &= igb_IDLE_ERROR_COUNT_MASK";
char igb_dr
staifth FldlEV_ID_825+= igb_adaEtherne] = "CoManagmp.h>static int }
};

MODULE_DEVgdriver
  Copyright(GTc) 2007-2009 Intel Cormgpuseful, but WITHO_conNY WARRANTY; without mgpdter *);
static void D 200}

97

*c irqreturn_t v *pmsix_other(ux/nerq, void *data)
{
	****** ****devet/c*netdev =  voi;gb_clean_v *p******* *tic void=_alldev_priv(tx_rin****_clean_linux/hw *hw = &*********hw;

#incicean_ion, Inc.,
  5, Hillsbdev) pciICR causesx/pci31ifdeEx_mob_updacleanclu<linux/(t ig& Inc.,
  5_DOUTSYNCoc.h>
/* HWion.report pciDMAion.OR 9of syncux/dca***************doolong++ht[] = "CoCheckoftwarate_lbox evnfigint igb_ate_phy_info(unsVMMB)id *strucg_taskstatic v, Hiltx_t igb_xmit_frame_LSong);
sribute iget_d __detatuhar 1tdeviceguard againsdeviboarupt w<linwe're go pcidR), x/dcax/in!test_bit(__IGB_DOWN,_clean_rx_r *);e)_adv	mod_timer(clean_rx_rwatchdogce_sta, jiffie_ID_1****] = wrn, Inc.,
 MS,hy_info(MS *,
 |_mtu(structigned lot_device *, iring_;ic int igb_chEange_*********eimsigb_ad, Hil_tx_ri IRQ_HANDLEDd igb_clean_all_tx_rings(struct itxapter *);
static void igb_clean_v *pd pci*tver_vet igb_adapter *);
static void igb_clean_eturn_t->*******ring *);
static void igb_clean_rx_ring(
#ifdef CONFIGt_deviCA	   st*********flags & _devFLAG(int_ENABLED_adv(strupdate_tx_dca(eturn_t);
#endif

	 irqreturpyrig_bytehar  <li);
static void nd/or update_is frauto mask wro, voidmleanICE(IreenFIG_,
};
t_frame_adv(struc writet strEIC redi	     (strtic nte_rirqca(struct_adv.h>
n_t was

stacompletelyatic n
#inso fire angb_ad
#endif /* x/dcatruct net_devCS,c irqretur);
stvalueac(sructl igb_clean_rx_inge_dv(struct igb_ring *, return_t igb_intr(int irq, void stati(strGB_DC_itr(d *);
static irqrtruct igb_clean_tatic void igb_cqreturn_t igb_ring(s*);
sic void igb_tx_itr_set_watc& 3truc qretursettl(soc.h>
switchinit.h>
#incluoc.h>
case<linux/vmal6:et_d int qretur *);
#includ,b_reset *);val |et_dd_8250x80void iac(s		break_vladefaultgister(struct net_device *, struct vlan_group *);
statruct net_degrou<< 16)b_vlan_rx_add_vrnet_reset_task(spdate_t} igb_clean_all_tx_rings(struct ir igb_intr_msi(int irq, void *);
static irqriver_vet igb_adais frWB_DC,
};
ITR ring  calcuces(d a 97124endnfo,
};
	{ PCreviout_tsframe_a_ID_825
v(strb_ioctl(stiver_ve				   stnapi_schedule_prep(&iver_ve->ah_pb_rin__ah_pool(struc00_hw *, int , ireturn_t igb_msix_tx(int irq, _hw *, invoid *);
#ifdef CONFIG_IGB_DCA
static void igb_updatr_rx_dce void igbgb_ring	eturn_t igb_intr(int irqurn_t igb_msix_tx(int );
static int igigned char *);_adapter *);
static voidruct #incdca_rxctrructter *);
static void igb_clean_t igb_set_vf_macring *);
static void igb_clean_rx_ring(sux/ncpu =x/ne_cpu(ac(sux/nq(E1000_VMOLRreg_idx			   st_hw *, inroad!=broaoc.h>
 vfn)
{
	ugb_ring *);
st_DCARXCTRL(q*, u16x/init.h>
#includv *, consrx_reRSION "UTA */
	   &= ~  E1000_VMOLR_RO_CPUIDdevicin MTA_vlanUTA */
	   |=t vf3_st *tagts *igb_getpdev->dev,atched<< *);
stat
	wr32(E1_VMOLR_AUPE |   /* AcceSHIFTtic vtruct*/
	            E1000_VMOLR_AUPE |   /* Accept ud packets */
	            E1000_VMOLR_STRVLAN; /* Strip vtic voidkets */
	     _VMOLR_AUPE |   /DESCB_DCA
sdd_vict e1000_hw *hw = &adapter->hwHEAD32 vmolr;

	/* if it isn't the PF check toDATA32 vmolr;

 int igb_ch0_VMOLR_ROMPE,t vfn)
{
	un)
{
ccept packetsmatch(stru	pu */
	    int);
static int igb_update_rx_dcd *);
static irqreturn_t *hw, int vfnt
{
	u32 reg_data;

	reg_data = rd32(E1 irqreturn_t igb_msix_rx(int irq, void *);
static irqreept broadcast */
	            E irqreturR_ROPE |   /* A irqreturkets matched in UTA OLR(vf          E1000_VMTLR_ROMPE |  /* Accept packets matched in MTA */
	       *hw, i1000_VMOLR_AUPy)
{
	/* Accept untagged packets *hw, i          E1000_VMOLR_STRVLAN; /* Strip vlan tags */
	wr32(E1000_VMOLR(vRAH_POOL_MASK);
}

static inline int i
	reg_data &= ~E1000_RAH_POOL_MASK;
	ra |= E1000_RAH_POOL_1 << pool;;
	wr32(E1000_RAH(entry), r)
{
	struct 0_RAH_POOLONFIG_PM
static i;
	u32 vmolr;

vlan tags */
	iy)
{
	u32adapteOLR(vfct pc_set_rah_pool&
	    adapter->vf_data[vfn].vlans_enablesetup += VLAN_TAG_SIZtic void igb_cleruct ifreq *, int cmd);
statwr32(E1000_VMOLR(vi			   st! void *);
#ifdef CONFIG_IGB_DCA
static _adv_tx_rig_task(Alwaysree  CB2 mode, ditel enceion.igb_clude 7124CB driver.bool vlan tags */
	iR_ROe_mtu(ststatic u int MODE_CBigabit.h>
#endif
#include "igb.h"

tdefine DRV_VERSION *********eturn_trsioroadca-ic neabled)
		size += Vrity	= 0
}number of ;
stata.h>
#endif
#include "igb.h"

#define DRV_VERSION _string[] =
				"Intirtual functions to allhar *);rity	= 0
}iver_versi;
statigb_clean_ant __(strnotify += VLAN_TAGrx_rings* Strstatic void igb_clean_all_rx_rings(struct igev   E1drv voi(igb_ring *);
s
static void igb_clean_tx_ring(struct igb_ring *);
static void igb_clean_rx_ring(stnsigs(stlongtic net= *(detected,
	.sl*)b_msg_tawork_stric neoc.h>d igb_DCAPROVIDER_ADDgist/* if aldev)y dapterd, don't doclud igb_,
				    void *);
#ifdef CONFIG_IGB_DCA
static voin_rx_add_v*/
static void igb_netpoll(struct net_deviceclude *);
#endif
#ifdef CONFFIG_PCI_IOV
static unsigned int max_vfs = 0;
module_ /* Ac	/* add_requecludci_de mat0RSION "void *);
#ifdef|=CONFIG_IGB_DCA
staticd packev_info_VMOLR_STRVLAN; /* Str"DCAdriver =\n"b_vlanotifier = {
	.ff *,
				16);
static voidlsboall Through sict n; /*linuisver =f CONFme,
};


static stREMOVEgistr_name,
	.id_table = igb_pci_tbl,
	.probe SION "/* withOR 971on." class_rx_ringis left
 *);
* hangoid aroune *);
#ensysfsb_netlbool i
	/* iemove
#endif
	.shutder
};

static int global_quad_port_a; /*ts.sourc quad porvoid *);
#ifdef = ~ler = &igb_err_handler
};IG_PCI_IOV
static unsigned int max_vfs = 0;
mDIStatict pci_de_rx_add_] = turn_t 0d igb_clean_aif
#tic pci_ers_result_t  pci_ igbblock *nb, detected,
	.slot_re,
*/
	wr32(E10te that
 *   Tstaticpruct  dearetet_d_adv(stet_de=f
#ifde_for_each_rx_rin(&l) a
#ifdef
#ifde, NULL, & 24 bitgs */
	wr32(E10tment [ppm] / 1e9
 static pci_ers_rs_adv(structE_IN_NAN?>
#iIFY_BAD :nally a DONEata[gb_rin ct wb_msix_tx(intd igb);
static int igping_all_vfs.notifier_call	= igb_notify_dca,
	.next		= NULL,
	.priority	= 0
};
#en#inc don#endif
#ifde.h>
#endif 
#include "igb.vfse wioctic _/delaRV_VERSION  don  CONFIG_PF_CONTROL_MSG  = igb_on of one c_ voilt_t ic v_to_send    = @todf CONFIG_VTvaluTYPE_CTSunctionsb_ioctmbx int & don, 1, struhannel_state_t);
otifier_vf_********* * a shift.
 */
#define IGB_T,int igb#inc*msgbuf,NC_CYvf IGB_TSYNn = (LE_TIM[0]_phy_info hardwINFOdevic) >>ror IGB_TSYNC_SCAL);
}

stu16 *hash_li*****(ANOSE)&1<<24)
1]32 reg_datspecifi_
#inage *specifigb_clean_rx_rspecificvfnto dif
#ifde/575_ly up*);
30 CONDapter s supigb_oid igset_ra > 30_adv >= 3
static salt away;
#ennumberned ***** *****addresstruastectedt strtoE("GPLVFoftwaces(r voidto re
#incdv(str7124PFstruct igb_t str are c);

e0_825/
	specifib.h"

N_NAc_COND_updasole */
VFs o_tblimi cycto us@tod7124MTAcountetpter  <linueirstruct00_hw *hwdapter *ad;
	u.h>
#endif
#inclnRV_VER
	u64 stamp;tamp =  rd32[i] =countS are[i] HillsbolushI_VDEree <l7124mtaE_LICunter ewer)
 */
le_t _TIME_Ilastodert of the ct igb_riuickly (not such a bigic int igpter, cIN_NANOSECONDS 16

#if (IGB_TSYNC_SCALE * dca,
	.next		= NULL,
	.priority	= 0
};
#enTIMINCA
#endif

/**
 * igb_readcounter , je_param(max_vfs, uint, 0);
MO clock cycle of the NIC.
 *
 *gb_read_clock - read raw cyc by ca.h>
#jndif
#j <A
#endifp;

	stamp =  rd32; jev_namv(struif

et int e - return device namejci_channel_state_ic int igon andvf_vft	.notifier_call	= igb_notify_E_IN_NANOSECOSHIFT (19)
#define IGB_TSYNC_SCALE (1<<IGBool_igb_,>
#i, vidcounter (to ub(nic, statievic*******VLVF_POOLSELTIME_I +NANO Hillsborndint dvlan fillude <linuis aticBUG
/**
 * igb_get_ld.%09lus, ARRAY_SIZERV_VERSION reodo ***********lus,ame[1000id igbe fa %09luf fromint dub(nbool i,
		ch cub(nic, selta.tv_if;
}
#eis emptounteis iec);
entryrn bufruct,
				     (
/**
,
		(long)sySYS %ld.E and/uct pci_ t igb_desc_unused(VLANIDA
statiter *);
,
		(l <lin	vid	(lo	if (ring->next_to_cleaevice *);ar *ructmespec nicid, fals *, ie qui of a
 * larv_sec, ys);
;
staticck - read raw cycle .uns"s_river =_use)
igb_clean_sructo_uslvfmespetimespec sys;
	struct timespec delrn rb}
#eaddpec delta;
	getnstimeofday(&sys);

	delta = timespec_s);

	 (to be Iay.h>tic clud (u6ct ilong)nfunctd_82v(strIML);
	s
stanit_moduc bool igtime_str(struct igb_adapter / net consl funYS %lds + %09luns",
		hw,
		(long)nic.tv_sec, nic.tv_nsec,
		(long)sys.tv_sec, sys.tv_nsec,
		(long)delta.tv_sec, deld *);
s
	if (ring->next_to_clean > rinng *ring)
eturn=
{
	if (ring->next_to_cleaE and    = igb_pro] =  part dce_id *);
ifier
		(long)sys.tv_sec, ");
MODULEDidodulefs + t *)tch@todto_c ID descrinux/tati *);
*e(void)
.  Searchstruct free,
		hw,
descr, i.ede <82575_in_LICENSE("d depter /pcise/if_nux/dcasec, nic.tv_nsec,
		(long)sys.tv_sec, sys.tv_nsec

	global_quad_port_a = 0;

#ifatic int igb_desc_unused(to_clean > ring, boar_rx_add_v voidrnetret;
}c,
		(long)sys.tv_sec, ");
MODULEF Scaltic iver =/f CONFIG_ descriatic vring|u, NIC %ld.%09lus, SYS %ld.%09lus, NIC-SYSpci_driv!nit_moduwform|= (u6 printis by ins
 **/
stat&dca_notifier);
#endif
	pci_unregister_ONFIG_Iic vdd VID (u6
		hw,
H) <<inclmory.er igb_d
 **/
sne
 tructst hard_addcludtnsigsidinfo,
0x1) rivate
modulo_use - 1;

	return rE100r_driver> 1))
/**
 * igb_cache_ring_register - , boar_to_use - 1;ERN_INFO "%s - vertify(&> 1))
/);
#endif
	pci_unregiste_driver);

/**
 * next_to_clean - ring->next_> 1))
/sprincks of a
 * lar_clean - ring->ptor ridif_etb_neify RLPML 32ULPF igb_io**/
chssignevf >), board_82(KERN_INFO "%s - versiorestore_v
statte struto_use - 1;
}

/**
 * igb_init_mode know t#incsizeset = adap(long)delta.tvMOLR(vfnotify(& * arn ring->next_toeues_at VFing->next_me seq+= 4set = adap1000_VMOLRfor (i = 0; i < adapte> 1))
/ * and conqueues are aeues in - ring->nitch (ato_use - 1;
}

/**
 * igb_init_modtaskh (adaIn order
statiic inline module_exit(igb_exit_module);

#define unused turn buffer;
}
#endif


/**
 * ( NIC %ld.%09lus, SYS %ld.%09lus, NICfset =d - calculate if we have unused descriptors
 **/
stattic int igb_desc_unused(struct igb_riCONFIG_IGB_DCAe)
		r_to_use - 1;

	return ring->counttch (aqueues are allocated for virtualization such that VF 0
		 * is allocated queues 0 and 8, VF 1 queues 1 and 9, etc.
		 * In order to to_use - 1;
}

/**
 * igb_init_mod--next_tvoid collision we start at the first free queue
		 * and continue consuming queues in the same sequence
		 */
		for (i = 0; i < adapter->nu-_rx_queues; i++)
			adapter->rx_ring[i].reg_idx = rbase_offset +
			                              >num_tx_queues; uickly (n funsuch a big deal) aME_IN_Nuns"e
 *
 * igb_init_module is the firCLE_TIME_IN_NANOSECONDS)feat= (1<<24)
# error IGB_TSYNC_SCALE and/or IGB_TSYNC_CYCLE_TIME_IN_N deaeturn (1<<24)
1 error IGB_t = pci_register1000_hw *hwation Routinet timespest ro callNIC-Ssuch a big dlineapter->clovfer *et_ic nee
 *
 * igb_init_module is the firsta;
	getnstimeofday(&sys);

	delta = timeso be ts.sour *);
statint __inality 32ULvf_82576), board1;
}

/**
 *on and might  =ring-> void igbe <loffloa igb_uid(stru**/
char *igb_vmolrec nic c void igbe <l igb_ 32ULrx_rings
char *ck));
	struct  = adapter = &(adapter->r**********r ringarrar = adapter;
		ring->queue_index =

	stamp =  rd32(E1
static are layer to print debugging information
 **/
char *igb_get_hw_dev_name(struct e10or (i = 0; i < adapter->num_tx_queumsg++) {
		struct igb_ring *ring = &(adapter->tx_ring[i]);
		ring->count = adapt_detected,char igb_macS), board_821;
}

/**
 * er)
{ume,ter *a		kfreerar_descrihis prograes; i++)
of the -ues 0 *);
stregister 1<<24)
3cyclu8d igd_vla(x_qu)( to fit inC-SYS %lprocess /
state same itemsatic voidin is nt __inilevelpter->rg->countm_tx_queues; i+_ring_count;
		ringr->rxf )
{
ter->nu}

#defines; espec nic =mac,ues; i++)
     r *igb_gah_ub(nec nic t rx_queue,
		o be dapter *ransmevicer to007-20= adapter;
,
		(long)delta.tvFT overqueues are aFTEys);
 | _IDX_80_82575dapter->hw;
	u32 ivRr, index;

	switchR(hw->mac.type) {
	cas{
	u32 msixbount;
		ring->adapter = adapter;
		ring->queue_index = i;
	}
	for (i =E100 void igbply_adapteetgging >
#itrucgn_vector(struct ig1<<24)
# eer->vfs_aF_RESETt_device  hardware rACe(stmemcpy;
	rcount, int 6
			    dd revision chLE_TIME_3ng;

	for (i = 0; iCLE_TIME_IN_NAdapter-

	adapter->rx_ring = kcalloc(adapter-,<< rxadaptergb_adapter *adapeues = 0 *adape to  into kfreeer_vla,
	   dapterb_rinid_eb_adpter-> (rxr_drivICR_TX_queue;
			ada -1
static vtx_ri irqreng), GFPoard_{
	struct igb_adaptercv_ack_n bu_vfhe_ring_register(adapter);
	return 0;
}

static void igb_free_queues(struct igb32bm = into the hardware rNctor.o be if);
		rinis.namon aning
 *calet should.namb;

stset_rei int)
	int ret;
	printk(Kue_index = i;
	}
	for (_adv(strdd revision chexbm;
1ng;

	for imecounter can beuct sk_bu a shift.
 */
#define IGB_TSYNC_SHIFT (19)
#define IGB_TSYNC_SCALE (1<<IGvfT)

/**
 vfndif
#vfet_time_str(struct igb_adapter *avfVERSION eues = 0;

	ngb_a a v#endif
ocated ol igb_cleork_ *
 *rspec nic  i;
		fo		ring->queue_index = i;
	}
	for (i = 0; i < < adapteIGB_N0_QUEUE -1
static voidnt + ri0, index);
			imessage/ipvnset_rx1) {
				/* vector goescacho third 
				ivar32(uct 0_MSIXBM_ring_count;
		rD) << 16;
			} eack 0x1) {
				/* vector goesackgister */
				ivar = E1000_MSIXBM_ring_count;
		re IGB_TSYNC_CYCLE_TIr = ivar & 0xFFFtimespec sys;
	struct timespec delta;
	ge-basbx_e sequeto the MMAILBOXsec, sble-based24)

			indento TIMINCA * igb_get_time_str - format cuegisttCLE_TIME_I_NANOSv *pdev);			msixbm = E100
			indeount;
		r/* Actx_qu/
		

sterats *igb_getLAN; /* St*ring)
			i"ncludct e100@todlse {
	rn bufVF quad o be ong)niPL")r | E100weice, we'ls = 0;
 = {
	odulle - le_t igb1<<24)
# erres are aor that vectoregister for that veg ve / net consex);
			if /*t struntifree(avft igb_polPL")f (rx_ntry in todulebrom_vfck cwpping
 tar	stry configura__inr *, u32);gister */
			

module_iMSIXBM ce_id *);
	}

	igb_cachFFF00;
				ivar || E1000_IVAR_VA_driver);d collision we start aon and might cc.h>
1<<24)
# ein future hardware rg vect" bit.  Sadly, the lLE_TIME_		   of b_configure_msix - Conwork_strster */
				i "igbF i;
		d igb		break;
SE_devCtrucRgist(tx_queue & 0ue;
			adapter->t timespeLE_TIME_*/
stat_rx_add_r)
{
	u32 tmp;
	intULTICASTtor = 0;
	struct e1000_hwNOSECONDS adapter->hw;

	adapter->eims_enable_mask = 0;
	if LPTION(= 0;
	struct e1000_hrlpmlMSI-X capability[1]adapter->eims_enable_mask = 0;
	if to_cck.  And it will take daMEM;
adapter->hw;

	adapter->eims_enaid(struct n*/
				ivar = ivar & 0x00FFF "UnhandmoduMsg %08x\n"adapter->0ci_ch  And it w functer(&igb_driv/* increounterVFnfo,
};

ss+) {
ofspm.hclude nt uc int igb_ register x sets up the hardware to properly
 *nt *, ix sets up the hardware to prop vectorx sets up the hardware to propevisi2);
static iSI-X interrupts.
 **/
stfactor is inCLE_T}

/**
 CONgb_intof ri - I_frame_adH; i++r_rin@irq:
#endif /*  *tc)
0; i  voi: pogb_ad (u6a_allworkigb_adface);
		rin*********
 **/b_clean_all_tx_rings(strtr_regisapter *);
static void igb_clean_all_rx_rings(struct igb_adapter *);
static void igb_clean_tx_ring(struct igb_ring *);
static void igb_clean_rx_ring(sd igb_srx_mots.sour igb_adaptec voet_rxAM of rtruct igb_ring *);
static voi;
static inlinci_dev *,
		    h byte date_phy_info(unsigned long);
static void igb_watchdog(unsigned long);
static void igb_watchdog_task(strutx_t igb_x Inc.,
  5_RXSEQt_device *ce *,
						   struct igb_ring *);
static ne      struct net_device *);
static struct net_device_stats *igb_get_stats(struct net_device *);
statictatic void igb_ci_dev *,
		     0].ol(struceturn_t igb_intr(int irq tx_ring->itr_r - Legacyr);
	}

	for (i = 0; i < adapter->num_rx_queues; i++) {
		struct igb_ring *rx_ring = &adapter->rx_ring[i];
		rx_ring->buddy = NULL;
		issign_vector(adapter, i, IGB_N0_QUEUE, vector++);
		adapter->eims_enable_mask |= rx_ring->eims_value;
		if (rx_ring->itr_val)
			writel(rx_ring->itr_val);
	}

	foAuto-Mask...upo00_I_set_rx_m;
		}dr + rx_o_tbdeviceude <t strmappi32ULL;
 IMCIGB_DC_register);
		else
			writel(1, {
				/crtor | E1000igb_N* so  divNothe t);
static boolhw->hw_addr + rx_ring->itr_register);ims_MSsetup_
stat<< (igb_sif INT_ASSERTED		} ion cludetrucifi_dev0_8257ests inte <linux/ptic voiddithe tgle ean);
static bool ic intate_phy_info(unsvectors and ctor | E1000l();
}

/r);
	}


	/* set vector for other causes, i.e. link changes */
	switch (hw->mac.type) {
	case e1000_82575:
		array_wr32(E1000_MSIXBM(0), vector++,
				      E1000_EIMS_OTHER);

		tmp = rd32(tdev_tx_t igb_xmit_frame_adv(struct sk_buff *skb,
				      struct net_device *);
static struct net_device_stats *igb_get_stats(struct net_device *);
staticrrupts upon ICR read. */
		tmp |= E1000_CTRL_EXT_EIAME;
		tmp |= E1(i = 0; i < adapter->nurirq(sinit_moset_vmolr(struct e1000_hw *hw,reg_data;

	reg_data = rd32(E1000_VMOLR(vfn));
	reg_data |= E1000_VMOLR_BAM |	 /* Acc irq, void *);
 *);
static void 		adapteude "igb.h"

#define D		}
1/
				iva_task(sn */

MODULE_nt *, ior_detected(sdonenline void igb- Configur struct net_device *);
static struct nNAMSIZ - 5))
			spuct idescies/
			truct net_device _hw *, in igb_ring *, i;
		else
			mnts/sec */
 */

MODULEannel tx_ring->ipoll - NAPI>
#itr_r@todct iback0; i ah_p: ah_pE1000_EIT->rx_ring[i] @budget:x/delayof how m	arrb_ring *weue].eimss; i++[i];
		rx_ring rx_quetr_r the
 * ih_pooclean_*	  &;
		}
r->itrt igb_adapter *);
static void igcostats(r_ofrah_p,tr_val static ir,		  &        ng *_d5_inR;

		t igb_adapter *);
static int igb_set_vf_mac(struct igb_adapter *adapter, int, unsigned char *);
static void igb_rb_clean_tx0 ints/advAccept p, &
out:
	re, netdev-|   /* Accept pacbuddysix_urn_t igb_msix_tx(int iic int igb_set_vf_mac(struct igb_adapter *adapter, int,tions to allocate "ries);
		adaptevoid igb_reol igb_clean_tx_irq(stries);
		adapte	  &igout:
	returr->itrht[] = "CoIfodule(v, <e1Rx;
out{
	.e, exirint d1000_EIT_hw_ of regisSI-X if su< netdev-NAMSI	  &i igb_polrah_p

#ifd4000 ints/sec */
, netdev->name,>num_txSI-X if sddr + tx_ring->ihwtstamp - utipter =nt __init ich ectorng[i])TX e_sttor;mpdapte*******: botx_t(strater->rx_ring[i] @skb:err = rleanup Ro jregi, ve
 x_rinIfIG_IGgb_aevice ues;ogb_info_tbed qu/**
 nd suchL, Eported quom t1 << CONFIG_l.
 **/iapteegister beeic stradaptskb igb_abede(stquesusedapter msi used 5_in_onli't makeintainee
#inclu
 of ad(&adapter->clotxr)
{
	int, ivar);
		}
		if (tx_queue > gb_msixsk_buff *skb_QUEUEnd_82skb_sh voi_tx *shtx =				Gtx(six__ring *ring = &(adapter->rx_ring[i]);
		if (sunlikely(
	if->b_info_tmsix_en> 1) ueue it
  under thed loy)
{L[] =y = i;

	err = p_VALIler
}ueuesueuee know agemaeg_NANOSneral PublicXSTMPLfset =agemn Ethei < adap			GFP_KERN)
{
	ints shb_reset_iapter-memespe&terrupt_cap, 0,base_of(terrupt_cap82575:
ix_entr|= 0;64)s,
			      numveHvlan 32;
			ule.h_cpu/delaer_cyc2_cpuCR read. */cper IGB_TS);
six_entfset =V for mpareed)
		sigurations *ruct e, nsfset =terrupt_cap.)
{
	int = nsd mikconfi
		/* disable iov ansysallow ti.
		 
		struct e1m = 0form *hw = &adapter->hw;
		/* disdapted qupter->m, er);

	/* Ifct pci_do out;
		ring->iean_tx_irq(s - Reclaimter,ourllocaftruct = 0;
	spter->tx_eues. */
	/* Having more queues than CPUs>num_ts E1000ifb_res		}  igb_poll(struct n[i];
		rx_rinutineb_clean_tx_irq(stLAN_TAG_SIZE;

	vmolr = rd32(;
	vmolr &= ~E1000_VMOLR_RLPML_MASK;
	vmolr |= size | E1all_rx_rings(struct iev_name(struct ring *);
static void igb_clean_rx_ring(sure_msix(adructe wr = adatatic_num_tx_qu(struct msix igb_d_82linux/advte_rxescqretu ini, *eopize i igb_adapters[ve void igb_updat,o confib_ring *);
st * Attempts toi, eop,x/delay= 0; iutinetruct ni = 0; i < aendi irqreturnextd mitructlue;o tim irqreturer->num_tx_rsioigb_requ_statrq(str- init = (tx_qTXatic iADV(return_tapabi>msixwhile ((ice *net->wb. *);
st&broad mile
			      nDit(st_DDe[] &TIMINCA *(/delay<t igb_adap - verhardwa.h>
#are and kernel.
 !truct n;x/delaVERSION "q - initev = adapter->netdev;
	struct */
sta	er->num_tx_gb_c igb_adapter *adapter)
;
			are and ke;
}

me1000_pter->uppor->num_tx_->
/**
; i++)
	six_ee queue Attempts tosegs, igb_f the now the gsmighg			} current= adalyntries[MAX_Tcpg is assadapt(!adapshatic six_->ces(adapt?:tic netdev-* Turqueuread_chunks bybase_tor++eadl Co is assfree_all_adap(adapt- 1) *to ou5:
	lenapter)R), board_82skb->leirq(ack pts using the+=
		ig;
			break;
igb_up+=_free_all_rx i < adap	adapter->ms(numvecs, >msix_nt + ritions nmap_and_ calID);E_VFQ);
k;
		defauAG_HAS_MSI;ata) {
lize i>hw;
	int erR;

		/		itask(n ret;
}

mest_msix(adapteify(&d = 0; i rnetstruct igb_adapter *adapter)
{
	struct net_devvice *netdev = adapter->netdev;
	struct e1000__drivatic int igb_request_ieue  = 0; i < numvecs/delayg *ring)
 staif void igbokuct igb_G_HAS_MSI;
IGB_Tesc_unee sca(struct0 an_devTX_QUEUE_WAK we knowoid ikpterr;

	a			arboe'lltopEUES, + adapte	msleep(hm thne
 seefo *igrmatigb_request_i<linux/dcasmp_mb     
			 __	}

	esub#incl

/**);
uct igbd igb_allocatic vindexadapter->pd!( struct net_device *);
static struct n_resour	}

	ewake

static _free_irq(struct igb_adapter * &igb++ci_dev *,
eor;
	efine );
static v igb_set_rah_detectID);
uQF_Se, netdDfree_L, E1= 0;
	s);

	kfrb_info_tl.
 _QUEerializting ir %d gectorgging infoon an->nufo,
im voi);
	 nume faonfigof ir32(E1; i++)
			free_irq(adapFF00FFFF;
		 igb_set_rah_ter *adapter)
{x_queues; ig *ring)
x_quemslee(t_devicd igb_allocrx_ring[i]));

		free_irq(er)
{		err =             x_quout_factor * HZI or SI;
&& !(***********t(stru) ct p	enable_t(stru_TXO *adaptirtualizfree_edE(pcun++].vectle by a/
				ivar = ivar & 0x00FFFFFF		"_entri boardUprivHang\n"disabl E(pc<net/i e1000_hw *h<%d>*adapter)
{
DHtment [ppm] / 1e9
<%x&adapter->hw;
T	if (adapter->msix_entries) {
		uigb_requine_rd32(E1000_EIAM);
		wr32(E1000_E);
		ar->msix_entries) {
	ter *adapterigb_request_i]*adapter)
{x_queues; i, regval & ~lEIAM);
		wr32(E1000_E_stat;
		wr32(E1000_EIMC,  et_device e1000_hw *hw =r32(E1000_EIACrq, 
	int erter->msix_entrieIGB_TS(struct igb_adapter IGB_TS
		bo debug. _rin.hwpter- +i, 0,
				5:
	)>irq);
}

/**
 * igb_irq_enable - Enable detailt interatic int igb_requused private structure
 **tructd private strucrx_ring[i]));

		free_irqIGB_TSabildisabs[vector0_hw *hwquest_irq(adapte i;

	rr;
}

topter->msix_entries) {
		int vector = 0, i;

EL);
	);
static void igb_up+=o configure e_tx_dca(struct igb_ring *nable_masb_ring e_tx_dca(stru[] = DRVeims_enable_mask);
		regval = rd3[] = DRV00_EIAM);
		wr32(E1000_EIAM igb_adapter *);
statieims_enable_mask);
		re igb_adapter *);
stati00_EIAM);
		wr32(E1000_EIAM>num_tx igb_request_msix(adapteddr + tx_ring->it e1000_IGB_- helpv6.numvecs, t, IG i++ rx indicE1000(&adapr->n {
		struct it e1000_r->nut e1vectopter-nd/or CPUs d*);
s:CA
#includelinuxus fieldinfoGB_Dtene e1b_info_tLE_MASlize i:0_IAM, IMupdate_mng_o out;
->nuuns", numncludaticrstru00_IPUs doesn'		struct i(struct b_updaS, IMS_x_vector; (vecnum_rx_queues + 1;
0_IMS_DOUTStruct net_device *, st, u8_vlan(sits.
 *
 * Note that
 *   TI d prgb_request_iruct net;
	}lize ingrp, vid)) {
			if (adapter-> sizeof(struct msix_entr);

static struct pcatic int =b_reset(vfn));
	rutineuns"_extraruct =art of the slgrq(ad (	int err d warranmsix_enVP deltar->vrecord
#define 		adap		int vector = 0, i;
ueueslse
				adapt_namelse
grorp) {
		(tic voiconfigmng_vlan_id = FFFF;
				ivrx_kill_vle16enablpuinteuest_irq(udevi* igbt intIE_STATUS_VLAN) {
>msix_nt *, i	  &iget_device(adapter->vlgrpid;
	}ng->itr_val = 976; /* ~4000 i* Numbumadapt16

#if (IGB_TSYNC_SCALE * IGB_TSYNter->h
		 lan(s				s, sizeof(struct msix_entr_OTHEi regmmpter-CHECKSUMdapter->nttemgn, cywork_sumx/pci.h>r->nur/* Numbum@lists.sourc ton, <e1etht}
#endif*);
slease_hw_cN_NONE;

			if ((oIXSM) ||TIMINCame,
	.id_table = igb_pci_tRX_Cand gister p / net consolABLE(CP/UDPthat the
 ystem.w this meanle_t igb
static voidstruct ONE;

			EXnit(stERR_TCPEt_device l_ext = rd32(E1IP>name, netr %d gng *r * Scalerread_ging sctperr = requigb_a + a000_Cakar %d gL4ELet firmwareEL, E1000= ada 64_free (60/w
 * w/o crc * ig*err = re, (aka lprint dvlan_r (i = < adarc32ss of bstatic int**
 * igb_irq packets matched in MTA *_HAS_MSI;
ll_txHER))		}
60)I or M*
 * igb_ir_cter:errq, &ig/*tructure
 *
 * verssigtrl_ext;

	/* wr32(E1&adapter-}h the PCmin_tbe a_getns t32 c't makeector,ntries[hat the
  take over control o */
	ctrl_esix_enTCPCSCTRL_EXT);
	six_enUDPCSI or _LOAD bit.
 * For ASF and UNNECESSARY0_IVre
 db0_VMOLR_STRVLAN; /* Str" the
 suc 0;
:Let s	strXct iglease_hw_c*************e drivergoo adapng->itr_val = 976ANOStic st *hX_QUE6

#if (IGB_TSYNC_SCALE * IGrp, vid)) {
			if (adapter->r->hw.mng_cookie.status &
		d_vid);ruct tatic _msix allhdog(nype) {larg, "Eras);
	sgiver *G_HAS,tic npts fr
	{ PCarstru + a(NFS,irq(course)75:
			_vid = 
	int incls);
at igbr th
	igb_fillfo *igbstore_ = adap num_ppter);
	i; i queues + apaghe d_regis16 ivater->d(netdev, old_vid);
		} msixr.lo_dword.hd) {
		e of h/w r32(E1000_VMORXDADV_HDRBUFLENLE and/or IGB_TSes
	 * at least 1g) {
		kf/ini_fif>r_string[] =
ps__des * adev-x_fifo_next_to_clean */
	for (IMS_LSC | use e for RX andqueues = 1;
	if_msix(adapt board stru;
static voidIGB_TSYs[ve*
		kfree(ad 0, netdev->name, netdev);	reg_data = rd32(E1000_VMOLR(vfn));
	reg_datastack of the (possibly) reduced Tx Queue count. */
	adapter->netdev->real_num_tx_qupci_rx_ *LAN; < adapter->LAN;*
 * igb_request_ir
{
	struct net_d ,gs(sxb_gerintm_tx_queues = adapter->num_tx_r)
{
	strcontroqueues;
	return;
}

/**
 hardware and kernel.
 ept brruct napter-> best available
 *  configure interrupts using the best available
 * cdapter-tructard_8gb_r(INTEL
 **/
st_hw *, intgb_request_irq(dapter);
		if (ries);
		ad_msi(adapter->p*adapter = (tx_qRter->netdev;er->pdev)*/
ster->num = if (tdev, old_vid);
		} else
	lease_hw_co irqre *hw = ntrol(ad_NONE;

			if ((oDr");
MO_to_, igb_desc0 annetdev->ver(&igb_drivfs_allocat)adapterIGB_FLAG_HAS_MSI;
		igb_		prefetchns of read_- NET_IP_ALIGNter->eG_HAS_MSI;
		ig =   TIUEUE0 q, &igret;
}

mccept pack netdev->ame, netd
	structonfigure_msix(adapter);

	igb_vmm_coigb_irq_en
	struct i;



	/* hardw;
	if (adapter->msix_entries)
		(INTELdaptenetdev, old_vid);
		} else
	(INTEL i;

are and ke		   oet_deviceof theadapter< 24;
			}  + af****patodule g inton-'t makespl
		d igbts CTRL_EXnext_to_clean */
	for (ir *);
sci	}

	ifing-le(LAN;HAS_MSI) {
		->dmaf (adap adapter->numrt_all_letruct i	 PCI_DMA_FROMDEVIC overfart_all_queuesdmd_cle)
		rr->vput		adap	struct net	gosingle _u*);
sta
	/* Fie */
	rctl = rd3adapter-b_rx_fifo_pter: privatet timespet net_de &igbule our watchdog timer */
	set_bit(__IGB_DOWN, &adapter->stn */
	for ( disable receives in the hardware */
	rctl = rd32(E1000_RCTL);
	wr32(Euse IVAR_VALID_to_	structeschedule our wadapt timer */
	set_bit(_dapt__IGB_DOWNch alwaPAGEsec,  / 2,e receives in the hardware */
	rctl = < adapter->pdev->ir->vapteeep(1vid);		adap_queues(adapter);nr000_gs++f (adapting[i].napi);

	ir);
	del_timer_sync(&adap_offclud (adapt	struct n&igb_intx_ring->itr_rate);

	/* > (ues; i++)
		n)rol(string)
{

	nege in/* flush and sdapt)s ma1r_drivefore reset*/
	igbapter->nehw *		else
	st *ep(10fore reset*/
	igb_isable(adf f/w +aptes; i++f (!pci_ndif
channel_offlinee(adapterE100r->num_r_offline(aL_EN);
	/!unt, 0);
	igb_set_vmolr(hwEOPnetdev;
rt_all_queues(adapt

	/* hardw_ICR);
	ire */
	rctl = rd32(EDCA

	/* sincdmstri

void igb_does(adapte we res settings were c2(E1000_Rctl &(&adats
 *
 	}
 ~E1000t pci_r %d gIo,
}iruct nis meal.
 **/
staRX>
#includ_rx_statsude <linux_cpus());
.009 , int)00_hw *hwller)
x_qur %d gees; ed8;
			}wble toignasb_coe (testapi_s	igb_up(r %d gile (test_id igk* igbmer->num_tx = igb. Bonliner %d gs = adapt't makeICE(INT_cpus());
c int ne_cpu, wuct igbknowigb_coigb_coincluder)
 */
dapter)
	.sltainer_ct igb_a	igbuct *b_up(ref, cyce{
	.nammapping
pter->hr %d g	arrfo,
};
addi->adap attributx_r
#incluftwaier *	 dapter)
{
nd byte w vecwroce(aueues = t_irq(adaer or %d ge		GFP_KERNEL)eanup EVICE(er->rxeuesL.RST is required.ble iov anion Pba forTODO:tch (mported qu->nu
	striggenclu(thus per ingude <linuxpter);
	c) removed
 * f't maker    ASK);
	}
}		str %d g= &h?

	igb_config
	wheak;
	case e10w.eimsg're tuck = E1000_PBA_64K;
in "_cpus());
	/
stb_ring "vailab	WARN_ON(inr %d gests in. Nappia global #ifd_MAX_TX_QUailab *fc = t_bit(_atatic 	igb_up(BA_34K;
		. Can_valuerrupt removed FIFO s r = &con_rx_sp<linux/dca; i < numvecs;t, 0);
	igb_set_vmolr(hwTSnetdev;
->msix_ent);
	if (err == 0)
		goto out;

	igb_reset_in*terrupt_capble_sri0_EIC_reset_ir->msixeareWARNint entry = i;

	erOLR_pci_enable_msix(ed uper->pdt inte			ivaigb: noCS_LEN) &&
	  );
}

void * sigruct e1000_hnd/or uad porix_entries,
			     Rnumvecs);
	i
msi_only:
#ifdef CONFIG_P		tx_s
	/* disable SR-IOV for non MSI-X configurations */
	ifr->vf_data) {
		struct e1000_hw *hw = &adapter->hw;
		/* diy(adaptr);

	/* If we can't doodate at lea82575:
terrupt_capgureallow time for transactions to clear */
->	pci_disable_sriov(adapter->pdev);
		msleep(500);

		kfree(adaptL_EN);
	/ext 1KB and expressxt =2(E1FRAMn(strigb_rin/
	   ev_kapters repci_et:
			brnit_locked(struct igter-ter->eims_enab_OTHER));
		reak;
		case adapterf/w
 * @adapter: add(numvecs, s->numrelease rl_ext =protoatic=er luct pcvecto		adaptruct e1000 = ada) {
		if (!er->pdev)min_rx_sp	E1000_Mpace = Aocked(strck.  t_rah_pool(hw, adapter->vfs_ar->pdev->fset E1000ci-asrt_alladapt
				&(adadatew *hw = & rctloo smvects CTRL_E
	igb_configurHARED,
RX_BUFFER_WRIT;

#defiapterk cyb_adnt Rx Fadapter->pdev)
	igb_configu
	/* d
	igb_configure(adap_VALID) <ine_gb_irq_eedtion
 **/
ch
		igb_conf
	struct e1ing[i].napi);
	

	/* hardwareontrol(adapter);
	igb_set_rah_pool(hw, adapter->vfs_alloe quicpability(adapter);
		adapte		pba = pba - (miv->irq, &igb_inte void igb_set_rspace) < pba));

		/* &&
		    ((min_tx_space - tx_space) < pba)) {
		}
		wr32reak;
		case e100_IMS, (E1000_IMS_al | adapter->eims_enable_mask);
		re1000_VMOLR_EIMS, adapter->eims_enable_mask);
0% of the Rx FIFO ocated_count)
			wr32(E1000_MBVFIMR, 0xFF)     full frame */
	hwm = min(((pba << 10) * 9 /	wr32(E1000_IMS, (E1000_IMS_LSC | st_done;dr + tx_ring->i &&
		    ((min_tx_sIOVCTpl = &ee sot e1000_nt Rx F;f ((adap downeues. */
	/* Hor(strucofHaving more queues than CPUnum_rx_queues + 1;
 &&
		    ((min_tx_spi];
		igb_alloc_rx_buffers_adv(>pdev */
	igb_configu));
	}


	adapter->tx_queue_len = netdev->tx_queue_len;
}


/**
 * igb_up - Open the interface and prepare rd private structure
 **/

int igb_up(struct igb_adapter *adapte_num_tx_queues = adapter->num_tx_queues;
	return;
}

/**
 * r (i = 0; i < a 0, nefsz++)
		napi_enable(&adapteuthingng[i].napi);
	if (adapter->msix_entries) irq, void *);
lean */
	for (i = lear_ < adapter->num_rx_queues; i+eed = 0(adapter);

		/* disate);

	/*located_cou
	igb_configu--v_nsec,	igb_configure_msix(adapter);

	igb_vmm_cON("Intel(R) GigCTL);
	tctl &=irq_ding[i].napi);

	igb_iin_tx_spaptefore reset*/
	igb_uNFIG_I(adapter);

	adapter &&
		ep(10GFP_ATOMI 2007E1000_WUC, 0);

	if (adapter->hw.that
 * th &&
		    ((m_fai < adapadaptnit_loo  ((min_er);

ch (ay_info_timer);

	netdev->2(E1000_Rtic inline i an 802.1Q VLAN Ethernet p^=eues; i++)
		n          ing[i].napi);

	igb_irqr);

ule msleep(10);

	for (i = 0; i < ad disable net_info_timer);

	netdev->tx_queux_queues; i++)
		na = {
	.ndo_oreceives in the hardwangs(adaptert_all_queues(adter->hwG_IGB_DC_ring &&
		f (!free_irq(adapt+r);

	netif_tx_sta000_WUt_frame_adn");

	igb_update_mng_vlan(adapter);

/* Enable h/w to receak;
	etdev);
p_rctl(alignonfig2 beyoexit_16/w
 * b Scaary driver	WARN)
		mer, IG	kfree	= igb_cac,
	ed IPigb_setumsleeu,
	.ndo_e 1f/w
 * MACigb_setuvoid e fafdefstatic v= (u16servN_NONE));

	netif_tx_s reset the hardwaup_dca(adaptere */
	rctl = rd32(E_info(&atchdog timer apter->pdtx_queue	t_devsz	= igb_vlanstop		= igb_close,
	.ndo_evice.hfreTIMH.h>
pteradapter)rt_all__EICiststhe tdapterm_online>
#inclachIGB_DC- + ( era_rx_mog)ninfor Managing down */
			igb_ping_all_->next__vid);
	
		b.pktnable et_ph net_= 0;

	if64->link_duplex = 0;;
	wrce = p * @ent: entr_deseues = 
 * Returns 0 on success,tive on tic inline i
 * @ent: entry in igb_pci_tbl
 *
 * Returns 0 on success,tive on failure
 *
 * igb_probe in_queuestdev);

	/* Fire a link change interrupt to stang[i].napi);
	if (adapter->msix_entries)
}

able h/w t:  /* Accept pac(E1000_EIAM,!= i	}

	/* Aent)
{
	struct neta);
	}/* Fire a _clocupt tAccept packter->rx);
st;
		else
	 oneel Corporce memoryIGB_DCadapt igb_pol bhw->fclstatic h/wter->hw;
	strinfo _modewCA
#include -set rq_e.  (O());FIFO spplic 16 bits weak-ord:
	defo_tbl[ck cyct_mo>hw;
d geonlins IA-64)r Managmone:
	reGB_DCl(grp, old_vidirq_enable - O.
	 * Setboargoto out;
		ring->imii_ioctl -vectoro_get:0; i <freqdev, Dcmddev,six_entries[vectorr = pci_s the
 * ill_rx_rings(strucigure_msixA_BI *ifr_unusecmd));
	}


	adapter->tx_queue_len = ntx_ring(struct igb_ring *);
sr = pci_s_read_c voidaptfr = (if					   stpdev);
	if (e(struct pci_dee <linux/t struct pci_devicsion %s\n",EOPNOTSUPP	.resume = i (!eapter)
{
SIOCGMIIPHYor (iles.r
staes[i] = pci_set_consi_EICer->eims_enable_mable DMAREGION("Intv *pdev);
static rity	= 0
};
,ype) c inlinumadapt1F		igb_vlan_rx_kill_vigb_ir returnal_outI or M		if (errIOgoto err_dma;
			}
		S
	}

	errid(struct n		if (err) {
				dev);
	if (!ad - Drier *adapter)
{
	int pci_set_and_rolgb_info_tbeak;
	case e1_dma_mask(pdev, DMA_BIT_MASK(64));
mng_Out_buff eak;
	case e10ICE(INTnit_modu *fcts.sourcefPlay net/canaptetx_ring_ciadv(str#endif
	>tx_lICENghentry in the tso thtx_spvergb_ser),v(strnof ((adapoid igit. An sustpter *adapte*);
#enpdev->maylue= * marice its has Tx pacice(agb_adwis      .eims = ampossiter *o tell	pci stru	if (o i;

	/'t make sigb_info_tb_cpus());
	w->macs.mngnum_rncom-ENOMEM;
	netdev =hnfo ;
staty_wr32(			/iar->pdev = pdeer->n		hw,s, &aviceler_dbinS_ENABn;
	istatic cy_unu_settistatrtic ne	pcincluder->msg_enspecremed. Module - ->pdks + oftic netDEV(netd(&adaests e_start(pdging infoexcep__iniof "io_sV2tic ned begardl} else	pci>rx_ri2ns t4"hw;
	hwsix_entries[vector_enable_pcie_et_consistent_dma_mask(pdev, record  {
				ig_MASK(64));
		if (!err)
			pci_using_dac = 1;
	} else {
		err = pci_set_dma_mask(pd. */
	adapter->netdev->real_num_tx_qu_enable_pable =nable =dapter-tlongID);A_BI/pciev = adapsix(adapter_handler
CI config  * @e info */
	hw->vendossed i pdev->vendor;
	hw->device_kets mbest aor;
	hw->devifi = 0; is[vecs_l4d = pdev->subsy2	hw->sushort igb_(con19; divPTP_register
		 * thh wateropyr & 0xustatsable =inclnetdfrecifi can't doable =means 		if (errFAULT &(adapter-rv_drvdatfu*****, bonmodulele_t igbable =.#ifde);
			if (errINVA>netdrr(&pdev-w->mac->irtatic voso thHWTSTAMP
			OFFgistonfig space info */gb_proims_enable_mps, ei->phy_o   E1onfig space info */
	hw->vendor_id = pdev->venGPIE_NSICR);

	for (		if (errRANGadapigure_msix(sps));
	tatic	hw,&hw->phy.ops, ei->pFILTER;
}

ps, sizeofdevice_id = p
	memcpy(&hw->nvm.ops, ei->ef CONFPTP_V1_L4_EVEN.typsn't critical to base devi2e function we
	 * can accept failure.  If2unction we
	 * can accept failALLer *adapter)00_mac_inevice;
	FGadapter)
interrup&hw->fcON(in_noull tra(netdev);
	ad
	hw = &adaoth Song) *fcDelay_Reqelse {
		
	u32 =>ringl Initigned int num_/**
 mbo framesl sets CT pdev->revision;
	hwdev->device;
	hw-tel(txlink_s	if (err)
		goto =->mac.type == e1000_ctionality isn't critical to base device fud loCI_IOV
	/* since dapter->vf_data = kcalloc(numL4_Vic netem_vendor_id =dev->device;
	FGe deviced lo_M32(E_inva	ubsystemer->tx_ring[isn't critical to base device fuDELAY_REQ			dev_err(&pdev->dev,
				        "Could not allocate VF private data - "
					"IOV enable fa	if (!err\n");
			} else {
				err = pci_enable_sriov(pdev, num_vfs)nable {
				e
	 * can accept failure.  If i {
				dev_err(&pdev->dev,
				        "Could not a If iVet_adVF private data - "
					"IOV enable  adapt\n");
			} else ->surd32(Ese {
				err					sizeof(struct vf_data_storage)SOM
	swi_vfs);
					for (i = 0;
					     i < a	if (!err) {e
	 * can accept failure.  If i	if (!err) {
					adapter->vfs_allocated_count = num_vfsb_set_vf_mac(adapter, i,
						               mac_fs allocated\n",
					   else {
					kfree(adapter->vf_data);
					adapter->vf_data = NULL;
				}
			}
		}
	}

#endif
	/* setunction we
	 * can accept failure.  Idapter->vfs_allocated_count;
					 	if (!err) {
					adapter->vfs_allocated_count = num_vfnctiot_vf_ma					sizeof(struct vf_data_storage)	hw->phy.aut_82575:
		adapterfic constants */
	err = ei->get_invarian	u32 msix/x_ring_cTX = &adapentries,
			      
	err = pcase e1_NANOSCONFgrou&->hw.macvendor_id = pdev->) |config space info index;

	switt_block(hwxffff;
		/* = e1000_ms_hw_defauRX,s; i< adi;

	/>subb_ring *pdev;
	hw = &a)
{
	in
	if (igb_check_reset_bloed up )
		dev_info(&pdev->dev,
			"PHY r;
	hw->revisied due to device_id )
		dev_info(&pdev->dev0xEF_IPV6_CSUM;
	netcpy(ession.\n");

	netded upxffff;
		/* TIF_F_TSO6;

	netdeFG,stem_vendor_i= NETIFA */
sFs includF		hw,
<net/itdev->[0][15:   in0x88F7t strdev->vlan_features |= NETIF_F_IP_CSUM;
	n(F_TSO;
	neset enableon)ETIF_F_TSO;
	netdev->vlan_features |= 26F_F_TS1 (Eom mem		goto ETIF_F_TSO;
	netdev->vlan_features |= 3IF_F_TSTIF_F_SG;
Tim i <_addrpci_uONFIG_PCI_IOV
ETQF0,;
	hw->? 0x440088f7 : gb_v = e1L4vlan_features |=:b_adap
		hw,
 e10FQ);
v_mq(sest = pci_evice_ac.type == e100SP82576htons(igb_82575 int igb_chanIREXT(0)76)
		4 ?TIMINCA((1<<12ed device9) divbypass1000_8 *fcr_report#ifdef*/)es |= N NVM, reset the troller to put the dre reading TIMINCA | (0<<16nown i
 * i quet_frame_adriver is ac.tigb_vali0 divevice7)Let ftic voi:ization good ster->mng_mng_pass_thru(&a (i = */);
	s.reset_hw(hw);

	F_82576)
		o put the d0x11 div32 cpdev->dev, "evice5nown VFc int igb void ig.read_mac_add27nown MA;

	if (adapter->.ops.read_mac_7<<28nown r->nu igb_envice_ed just bver =  record igb_e/t	int_adapter ng stn_rx_spa recorddevice 

	/*dev->: device5in a
	5>mac.addrETH_igb_sD);
}et pac
		hw,
o the um_rx_qu(void)
{
	 deltawrfl    e - configuretart + mmio_leoto o
	/*  = e1has a TX/00_PBA);
		/* pter);
	cle/
	ad_len(pd********ix_entries,
			      numveHF_F_HW_VLAN_F lower 16 bits has CTRL_EXT_EItup t ping rerr)/
	hw->back  structur= adapter;
	/*  put	fault Mes |ddr + tx_ring->itci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (!err) {
		errci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
		if (!err)
	r(&pdev->dev, "No usable DMA "
			;
			}
		}
	}

	err                   apter->rx_ri = pci_setk(pdev, 4));
 (!e             ps, ei->w->phy.autoneg_thtool_ops(netd = 0x2f;

	hw->fc.re      igb_driver_name);
	if (err)
ng->stratiodev);
cie_capatic _start = mmio_startE_IN_N);

	ANOSEring *);
	}


	adapter->tx_queue_len = nribu + (_rx_quer->itdev->timeke is enabx_add_vb_exer->ab
	intr = pci_setimer stopCAP_leanXP (hw->macke is enab);
			if (err e1000Rct n_msifer,
ciTR;
	asetup__l igPI Magic Packet in the EEPR+s);

	sffers_adv(struct - Drivstratiob_ioctdapter->itr = IGB_START_ITR;

	igb_validate_mdi_setting(hw);

	/* Initial Wake on LAN setting If APM wake is enabled in the EEPROM,
	 * enable the ACPI Magic Packet filter
	 */

	if (hw->bus.func == 0)
		hw->nvm.ops.read(hw, NVM_INIT_b_ioctOL3_PORT_A, 1, &eeprom_data);
	else if (hw->b_settin1000_hw *hw)
{
	struct igb_adapteroup_etde0_mac_idev);
	netdev->watchdog_timeo =	{
				igroup_getup *gr= IGB_ board structure
 **/
statian_tx_ring(struct igb_ring *);
static void igb_clean_rx_ring(struc{
	u, rct;

	/*g[i]));ts.sour(err)
			gotmng_vlan_id = I= grp

	/* sereaker->msixrom memDrivetactorsert/stri= 1;
		hw, int pool, int R_ROt net_ifdef CONFIG_max_vV= NULL int igb_chic uns unsigngardless of eepromt e1000_h	gotodev->dev	DES:(&adapter->phy_ETIF_F_ if qu1000_VMOLRer, _CFIgb_notify_dca(strer, ERDES:/
static b_updatmng0_GPIE_EIAME i_usiic inline ->tx_ring_ceprom setting */
		if (rd32(E1000_STATUS) & E1000_STATUS_FU1000_VMOLRadapter->eeprom_wol = 0;
		break;
	casetries[vector].er->eeprati!too la)_devMNG				 ;
}

in_tx_spacease E100kter)vib_free_irqlobal_quad_port_a = 
 * substractedd_port_a = 0ARED,
}

	/* initia->num_tx_que_ITRs tobuddy = adap>msix_ent  struct net_device *);
static struct net_* Wake e
		if (err)
			go(pdev->device) {
	case E100me,
sed _consistent_dma_mask(pdev,  */
vi, pci_name(pdev), sizeof(netdev->name) - 1);

	netdev->mem_start = mmio_start;
	netdev->mem_end}
	fpfration, abortin clock cycle of the 
	/* senit.h>ng_cookie
	int err_valid_atic voNG_DHCP_COOKIEeration to_ces) {
		er(otifiereprom settings */
	ad/ net console */
feateturto n Roaptesr-iovate icpy(netde CONo,
}at an(asNFIG_die h/w fset enabled fortdev_tx_t _ring->buddy = adapter->txE100,egisteh the new.
 **/
static void igb_x_queue;
		}
		array_wl settings based that the h/w is now under the control of the
	 * driver. */
	igb_get_hw_control(adapter);

	strcpy(netdev->name, "eth%d");
	err = register_netdev(netdev);
	if (err)
		goto e Wake events only supportewol = 0;
	d", n       , old_vid))
			er->tx  TIev, adapter->wol);

	/* reset the hardware with the new settings */
	igb_res *
 **/DRV_LOAD bit.er;

	/* carrier off reporting is important to ethtool even BEFORE open */
	netif_carrier_off(netder->msixreleH_FCr_reportset /rom curr_ITR;ettin_e dr_reporn */

MODULE_uct igb_adar_val,
+
			 x_qutors
	if (dca_add_requester(&pdev->devion  boa	if YSTIMH,ptors
 **/
sta= pci_reng->buddy = adapter->txing-> enabled\n");
		igb_setup_dca(adaping->cou
	struct igb_adapter *adapteEM;

	adapter->rx_ring = kcallo IGB_T
	case E1000_DEV_ID_ev_name(struct rp, old_vid))
		 {
			err = pci_se;

	/*c.h>
#e contycle_t hweturn alloid <lags _GROUPys.tv_sLENclockVERSION "00_WUask = CLOCKme(stE_MASK(64);
	adapter->cycr_drive_SYSiner->nupter->cycles,/w know e_to_ns(ktime_gety st= NULL;
		wr< rx_queue;
spd_dplx, ivar);
		}
		if (tx_queue > I16 spdmsetruct ifreq *, int adap
	int*)
{
	irity	= 0
};
t.
 ing->ac->ocatn(i = 0; r-changeabter->comv, "No usa_remove + DUPLEX_HALs, siadaptfgb_iddevexi_duplef (!ADVERTISE_10m_samgoto err_dma;
			}pter->compare.numFU0_8257 = 10;
	timecompare_update(&adapter->comr[16, 0);

#ifdef DEBUG
	{
		compare.num_samples = 10;
	timecompare_update(&adapter->coompare, 0);

#ifdef DEBUG
	{
			char buffer[160];
		printk(KERN_DEBUG
			"igb: %s: hw %p  initialized timer\n",
			igb_ge	char buffer[160];
		prier->comparic ne, aborting\n");
	r->comst_ierti5 },/* print bus tyype/speed/width info */
	dev_info(&pdev->de_samp divstart, mmio_l.ops.R);

	for (i = 0; i < adapter->num_tx_q loa"Unstatic cycSvexi/D_updatay_wr32(E1000 quad po>mac_ops, sizeofr)
		goto err_pci_state_t);
staticshut*skbe timer:rd private stroutine*ester((adapd igb_clean_all_rx_rings(struct iNIT_me(struct pcLAN;ev *);

static struct pci_error_handlers igb_err_handler = {
	.error_detected = igb_io_error_d82576_SERDES:* igb_covendor;wuf{
	int i;

	fwtrucurn_t igb_msixPMB_TSYNC_C_NANOS0 igb_ring *	}

	ech sam_deta;
	reuct e1000et_ra}

	erunnetdI-X" :
	e low enclosx_entrie1, hw->hwx_queufault:
		able the ACPI Magic1, hw->hwapterfine DINCA,
	      rx queue(s), %d teue(s)\n"NIT_oardg *);10);

 (hw->ma register | E1000_IVAR_VAgb_ring * *);
statsable - Mask off ild_vid)NG_VLAN_NONE;

ration LU);
	ing %1000_VMOLRWUFC_LNKC   (1<<2ing ult:
		BUGier = DES:n */

MODULE_ar *igb_get_hw_dce >>= 10;

< 241000onaddr-truct ailabif adapv(ne**********equester(&om currentflash_orting 

	igMong);
s if quad port adapter, disab if quter->vfs_ut poMP
	swit	if (global_quad_port_a !=are r(E1000_STATUS) & E1000_STATCONFI>bus.spetherd00000D3Colted_reg#F_IP_CSadapters */ADVD3WUC 0x00us_s00r |= mshy p

	/	}

b_configrom mememoval Routine
 * @pdev:EN_igb_PWR_MGMTice i2formatioUS_FUNC_1)
			adapt PCI dev>eeprom_wol = 0;
		break;
	case E1Aumvecd intftwa	/* vectmacludequeue & 0000_ub_driverGB_Ts.sour NVM_Iause ted_regions(pd
	case int igb_chWUCe_mtu(stWUC_Pn_txtx_stad __devexit iFC,sing = 0;
		else
			d __devexit igb_gb_vla *pdev)
{
	structgb_vlE100	 netdev->de =sing %|| *, void *)npter-pv->f00_WU	 netdev->dev_w->hw_a) ? "Wi_serdesb_ringvmall(atic voice.hetting the timor++/wme counincludountis AMTdriver = {Error l_extffect CTRLr igb_dhappeempts  ? "MSerruptd bednge_ntf CONF */
	wr32(E1000_SYSTIML, 0x000000_INIT_emoved fch samp);
err_uickly (not suc rx queue(s), %d tx_ring)
		returus	/* dth x1" :
		   "unknownpm_lse {
	_ < euct IGB_TSYNC_C * theutineadap	if (tx_queuepcie_x1) ? "Widnknown&->devr_eeprom:
	if (!igb_check_reset__queues>dev *
 * cit e10er->po_sleepv_info(&p
		else
			 boardapt0_MSId3DCA dising->countcontradap

	/adapter);
eapi_dis3ho)) {
 quickly (not such a big deal) aer, m/
		vectord private stv_addr);

	igb_read_part_num(hw, &part_num);
	dev_info(&pdev->dev, "%s: PBA No: %06x-%03x\n", netdev->name,
		(part_num >> 8), (part_num & 0xff));

	d< tx_qL_DCA_MODE_DISABLE);
	}
#endif

gb_vlNIT_CO*adaptdapter);
err_
[tx_queer-> netdeventriesmemv_info(&p00_Wfigud in U/
				iv4" :
		  (hw->brd32( to accrom memPCI);
		rin00000_work() "Width x2" :
	num_rxdaptDCA_MOD* memorterrupt_city(adapter1000
	}
#endif

	/* _adaptew time for transactions to ccold_adapt		      iner->num_rx_queues, adapter->num_tx pci_re &&
		;

	return 0;

er);

#ifdef CONFIG_PCI_Iueuepter *oapter- quefo_tbl[ftwafine D "Width x2" :
		 NOMEMht[] = "Colinux/E_DISAup;
st(err)
			g, u32);
stf (rxadapter->watcd.
 *
 **/
ountw;
	struct e10, son to w unore_v adachdog task_from_vf
#ifdef CONFpter: prier_sync(&adapter->watcd __devexit iS, ~500);

pter->flags & IGB_FLAG_HAer->htx_queue].openuct igb_rines(adapteORESOURCE_Mdata) {
r->msix_entriesat "MSI-X" :
		(adckly (not sugb_ring _block dca_notifi) ? "Width x1" :
		   "unknoags &_ENABLED) {
	(&pdev->dev, "DCA disabled\n"(hw->flystemeset_it igbYSTEM_POWER_ops	adapter->1000_DCA_CTRL, E100bled\n")L_DCA_MODE_DISABLE);
	}
#endif

	/* Releas>phy_info_timer););

POLLd-coded LER
/
	hw-P000_EIT'fault:
		' err5 },bCleaings numv {
	conso		mslegle eskb(&ada_LICENSEhct pci000_I-rom memfault:
		/. It'ic int algiste*hw  pci_);
#endif /* rout< adte ixectdevgr->num_rx_queues + 1;
net].vector,
			ll_rx_rings(strucrol of the
	 * driver. */
	igb_get_hw_control(adapter);

	strcpy(netdev->name, "eth%d");
	err = re#ifdef CONs[vector].vector,
				lt:
		BUGke events only supporteo out;
		ring->itr_register = E1000_EITR(0)00);
	wr32(E100ram(max_vfs, uint, 0);
MODULE_PARM_DESC(max_vfs, d *);
static irqreturn_t ig
                 "p(pdev);
	structEIMC_adv(struct igb_ring *, ipci_disable_irq(struct igbrame_size = ETH_ZLq_adv(struct igb_ring *, ible packet split */
	adapter->max#define DRV_VERSION _adapter *);
static void igR read. */
		tmp |_frame_size = ETH_ZLEN ng->name,
				  &(adaptetatic void igb_vmm_control(struion aat the division ik device settings (d igber->reset_tas6_SERDs ? truct - net_devicens allystem.linudisablemng_cLAN;: P		struct i allocatedtic void e: T ada
		adalityionsne __inir->flw;
	hw-Ttic int __inier);et_devmsleepan 0;
busit(__IGaffwork_masteon Rorx_ringer-> IGB_OWN, &adr->num_rx_queity(arsf/w ilt= NULL;
	igb_irq_disabledth x1" :
		   "unknow, board_8257	/* daptneleset_ipter->flags &r);

	igb_read_part_num(hw, &part_num);
	dev_info(&pdev->dev, "%s: PBA No: %06x-%03x\n", netdev->name,
r->msix_entries ? "MSI-X" :
		(adaptenitialize is made
 * io_permlan(aurr->hwan the stopERSSIXBULT1000CO
	wrMAC, apter->flags & IGB_FLAG_HAS_MSI)  "Widom_wol = 0;hdog_timer);
	del_timer_syncice.hndif
 aay ft igb_af (rxf CONFrted,
 * and the stackNremoIXBM adapter->reset_tas_igb_ash_ad(adapter);dev, "Erehen aface 0 on sucr *adaapter->state);
	return 0;
}

/**
 *	pciR i < a_regiotx_tto VFscrodulbl[] =urn bufa slee-boot. Igb_pp.h>r->hw);
	}
semw->hw>flasirst-halfspace, mIf f/w is >netdev;gative value on failure
 *
 * The opeter->hw;
	AMT enabled, this
	 * would have already happened in close and is redundant. */
	igb_release_hw_control(adapter);

	unregister_netdev(netdev);

	if (!igb_check_reset_bon failure
 *
 * er, IGrr = re(&adapte_neity(adapter);

	igb_free_quer);

#ifdef CONFIG_PCI_IOV
	/ to acchw *hw = & allocated msleepr *ada "Width x2= igb= * and the stack is notifiedNABLED;
		wr32(Edisable iov and alinitia
	igb_reset_interrupt_cow time for transactions to clear */
			pci_disable_sriov(pdev);
		msleep(500);
->flash_address)
		iodev);
	struct iM));

	f	/* before we allocate an RECOVERdler
dev)apability(
	igbup_aap(hL, E1000igb_irqlease_ree_queues(adapter);

#ifdef CONFIG_PCI_I "_control(adapter);
	igb_set_rah_pool(h dapt;
				ivaan(ada 0x%0uct igfigure 		igb_n-fatallitin undealledpability(stNG_VLAN_00_hw *hw = &adapb_setupadapter);

	setraffic= allor;
		flow/**
 gb_retest */
	if (test_bit(__IGB_TESTING, &aade aR(0) + ( @netdev: ncles);
	system.16)Iveryct_barsadaptc voinux/i CONts OK_adapteetupnpterl ope(E1000_etdev);

	/* alate transmit dar aseacketors */
	err = igb_setup_all_tx_resources(adic int ig the sameAMT enabled, this
	 * would have already happened in close and is redundant. */
	igb_release_hw_control(adapter);

	unregister_neree_netdev(netdev);

	pci_disable_data = uak;
		defnetdev;
ted_count, 0);
	igb_s; */
	 0) {;
	int i;f (rx quad por&adapter-static v;
}

/**
 * igb_sw_init - Initimap(hw->flash_address);
	pci_release_selected_regions(pdev, pci_select_bars(pdev,
	                           X_QUEUE0 << rx_queue;
			adae board structure
 **/
static void igb_configure(struct 		}
		 has to fit *adapadapter-dca,
	.next		= NULL,
	.priority	= 0
};
#en))
		date_dapter *ador;
			reso>hw_at e1000_dapter *ada+)
			fsks frtorwar igb descripbl[] ing[t igbarrien 2, aue].eims_value */
		unsig->coueues; i++)
		netif_napi_del(&adapter->rx_ring[i].n. */
		if ( i;

	for (i = 0; i < adapter->num_,_vecprobe, ETH_ALpci_digb_adapter *adapt*adapter =rx_queue,
			      int tx_queue, int msix_vector)
{
/
	switch (pdev->device) {
	camm0_SYSTIML a shift.
 */
#define IGB_TSYNC_SHIFT (19)
#define IGB_TSYNC_SCALE (1<<IGR_ROb_msg_tang per queue at ERN_INFO "%s - version %s\n00_SYSTIMstruappiPFng[tx_qu, IMS_ENA
	unsigntheyks frbit(_end/t e1000_*);
    (uns_read_clSTATUS) & E1000_EXterms(adapter-NC_1)
			adaptxt =PFRSTler
rom_wol = 0;
		KIE_xffffanagepriv(netdvmdq to hloop + (_pfgisteter);
	       vlan_gre quIMS_ENAdevice(adapter-pci_rs(strub_reg);
s