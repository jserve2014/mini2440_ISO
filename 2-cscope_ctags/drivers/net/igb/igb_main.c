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
	adapter->stats.ptc1023 += rd32(E1000_PTC****)/*********************522**********************);
*****************mptc***************M*******7-2009 Intel Cobdriver
  CopyrightBc) 200
	/* used for 7-200ive IFS */

	hw->mac.tx_packet_deltaare; y under tTPT*******************tpt undribute it and/or modify;st GNU Gencollisionic Lic it
  under thCOLram 7-2009 Intel Corcolatione,
  ver2, a 2, as publabit Ethernet Linuxalgnerration.

  This pALGNERR Software Foundationrxeree ful, but WITHORX ANY WARRANTY; without tncrs7-2009 Intel CoTNCRSf MERCHANTABILITY orscration.

  This pTSCgram tware Foundationhe GfNU General Public LiFram iFITNESS FOR A PAiaation.

  This pIAcense for
  more deticrxo

  You should haCRXOLicense alongABILI
  thioration.

  This p, wrogram tware Foundationare aoundation, Inc. pro51AFranklin St - Fifth Flot F Boston, MA 02110-1T1  USA.

  The full GNU Ger, Bost, MAMA 02110-1T01 USA.
se ihe full G

  Yoqeon intwaree file calQEd "COPYING"COPYIContact Im is mation:
  e1000-deM USA.

  The full GNU Gordvel@lists.sourceforgRXDet>
  is frFill TY orhe OSare; istics structureredied a copy net_ it wi ulticast =are; yhope it wi drc**********************m is distr007-2009 Intel CorCOPYIHillsbRx Errors****stif ( GNU Geneype != e unde82575) {
		u32 rqdpc_tmp;h>
#64clude <lotal = 0ux/pint i<lin.h>
ead OR 9drop undats per RX queue.  Notice RQDPC (Receive
		 * Qincl Drop P Publ Count)lt wi/ionly gets incremented, ifmap.h>7124DROP_ENpliedit set (includeSRRCTL registeroftwathatmap.h>#net/).  Ifx/miiice.#itHillNOT.h>
,cludux/ethsome wde <f_vlaequivalent cux/plinustored in RNBC (notipv6.n.h>
 basis).map.h>Also notecludep.h>x/delay.h>due to lack of availablnmap.h>dee GNptors.h>
#i/
		 <li(ip.h>
 i <  under thnum_rx_n.h>
s; i++ocice.	ap.h>
#inished by the Fhecks(i)) & 0xFFF<lin**********rx_ring[i].rx under p.h>
# DRV_VERSIlude p"1.3.16-net/u+copy of the ver_gram is[] = DRV_VERSonst}
char igb_dr  under thrx_fifo_emap.h>N;
****icnet/;
	}pes.h>Netheic coninux/pn No Buffers 2007-2cis anf_et.";
exact
B_DCAn.h>
p.h>
asc conhardware FIFO mis p savherice.ay.YINGatst****oneifdeinuxreasone <lisavinge100in b_cop const ch,infoi00_vmalpotentially
igb_c tru2007-2._825**********************e_id igb_pci_ta=
		nse,ng[]75 },
	nbncluCorpoLEC onpci-asnewergb_l[] _tbcan be_t
#inrect so build
	{ P5ur ownogradist07-2edard_RUCral  ROC
	{ P6), bo,
	{vmallEV_ID{ DEVICE(r RCHANTABILITY oeven9712ope Network atic corcerCE(I, E1000_DEV_ID_8ll ben theVDEVIV_IDNTEL, E10rucIDE(INTE_SERDES_QUrs pr_82575 },
	{ PCI_V0exter Driver";/
oratiap.h>
#rx_lengthEL, E100L, E1000_DEV_ID_822575_pci		 0_DEVQUAD_COPPER)0_DEV_0_DEV_ID_82576PCI__82575 crc, E1000_DEV_ID_82575EB_CO_VDEVICE(INTEL, E1000_DEV_ID_825frame, E1000_DEV_ID_82575EB_COAD), boaE(INTEL, E1000_DEV_ID_825missed, E1000_DEV_ID_82575EB_COmp2576_NS)T	{ PCI_VDEViEVICE(INTEDratio" igbTEL, E100L, E1000_DEV_ID_8ecolRL, E100_DEV_EL, E1000_DEV_IDlatb_cod_82575 },
	{ PCI_VDEVtx_aborttryredi	{0, }
};

MODULE_DE(******etwor******* *) igb_cowindow(struct igb_adapter *);
sces(struct igb_adapter *);
staticarrier(struct igb_adapter *);
son.
FE_TABLE(pcp6_ccharneedsaer.be mainta75 } elsewhlinux/*illsbPhy Sude <pci_x/init.hphy.media_ PCI_=tic consruct int d_c *);rRSION igb_( under thlink_speedev *SPEED_***** &&PCI_  (!tworread_phy_reg(hw, PHYove(sT_STATUS, &igb_cmp))RSION "igb_ada &=etworIDLE_ERROR_COUNT_MASK";net Network igb_ll GNU dlEV_ICE(IN+t igb_adathe hop(R) "CoManagmvicegb_cop ux/nll_rx_resourcesVgoration.

  This p(GT Franklin St - Fifth Fmgp the implied warr_con of MERCHANTABILITY omgpdr *);
static v void Dram }

97

*c irqreturn_t v *pmsix_ohe h(ux/nerq,tatic *data)
{
	 underare; devet/c*netit i= tati;gb_clean_(str under  *;
static=_alldev_priv(tver_vadapter *);c conshw *hw = & under thhw;,
	{ Pic *);n, MA 02110-13,ypes.h>dev)ructICR causesx/pci31nfo,Ex_mob_updaer *)cluic cons(t ig&A 02110-13_DOUTSYNCRSION /* HWion.reportructDMA voiinclof syncux/dca7-2009 Intel Codoo Fre++htl(R) "CoCver_e <lirate_lbox evnfigux/nevicete;
stal[] (unsVMMB)atic*****g_task *);
stac voitx_v_tx_txmit_00_DE_LSong);
s GNU Gengr mo __detatuES),1truciceguard agains netE100upt wic cwe're goructdc in;
staigb_!test_bit(__IGB_DOWN,ter *);iver *);e)_adv	mod_timer(;
static swatchdogce_sta, jiffieruct1adap(R) wr MA 02110-MS,xmit_fraMS *,
 |_mtu(structigs(stlo modetde *, i E10_;_tx(strtx_tchEangeove(svoid eimsevice c voi_ct ig IRQ_HANDLEDdt net_r *);aleturn_tngstruct igbtxter *);
static vtatic irq, void (strdruct*t=
				igb_adapter *);
static v irq, void *);
stx_ring->ver";
stinger(int irq, void *);
staticiver_ve(
#nfo,f CONFIGtic inCA	  D_82576), boflags & ic iFLAG(int_ENb_adDnet_(strupd igbtx_dca(tx_ring);
#endif

	_all_tx_r This_byteES),_pci(int irq, void l Publb_updatillsbauto mask wro;
statmr *)75 },reenFIG_,l_rx_device void igbc write_8257EIC **** irq  (strrq, nte_rirqca(struct/* Cice.ingswasINTELcompletelyirq, n07-2so fire anvice igb_rin /*,
				truct ****devCS,n_all_tx_r);
stvalueac(s****lid *);
static iice  CONFIG_ct net_msix_, _tx_ringstx_tintrB_DC_all;
statigb_co(strdeviC_itr(der(int irq, all_alloc_rx_b void  irq, void *);
sl_tx_ringsrx_buffe(s*);
sq, void *);
txtl(s_set__sta& 3FIG_Il_tx_rsettl(sRSION switchb_probporatiluRSION casght (c)/vmal6:igb_r *, l_tx_r *);
	{ PCI_,*pdee <l*);val |n_rx0_DEV0x80irq, v*, i		break#incdefaultinclud(struct an_rx_int igb__alloc_vlan_group *);
statvice *, u16lan_<< 16)b#incatic add_vhoperuct v sk_b(s_update} irq, void *);
static irqreturn_Networ_rin_msig *, int);
statct net_device * =
				t igb_adaillsbW_ioc *);
ITR _msix00-dcus(st****9inuxendnfo *);
000_Dreviout_tsndif /*ruct ne
id igb_iocsk(st =
							 irq, napi_schedule_prep(& =
				->ah_p_buff__t , oonlinruc00_void,r(strb_se void igb_txuct itxg *, int);vmm_contrigb_adapturn_t igb_msit_deviCAnt irq, void *);
b_updrtic dce irq, voix_buffe	v(struct igb_ring *, intt igb_adapter *);
statt net_devicuct nnt);
sDES),*);msix_other(int irq, voidlloc_ratidca_rxctrllocother(int irq, void *);
staticigb_txb_revf_mac_msix_rx(int irq, void *);
static irqresptercpu =ter _cpu(*, ipterq*******VMOLRreg_idxgb_set_rvmm_contrroad!=broaRSION  vfnd igbux_buffers);
st *adRXCTRL(q*, u16igb_prob*);
statdv_congb_pivereer *);
UTAb_set  int ~  PCI_VD_VMOL_RO_CPUID16);
in MTA u16)          E|=t vf3_st *tagts *tx_tgetpdev->dev,stated<<adapter *
	wr*****AUPE | AUPE |  ic bAcceSHIFTrq, vid(st      Ent igb_seMOLR_AUPE | R(vfn), reg_datapt ud nd/or s       Et igb_set_vf_rlpml(stSTRVLAN;ic bStrip vrq, void, int size,
  lpml(struct igb_aDESCr *adaptic ict eCI_VDvoid igb_c*********hwHEAD32 vmolr;
is frifcludisn' 97124PF cver_ toDATA if VFs aretruct net_d_AUPE |   MPE,     */
	   */
	ter *ater, inmstatoid i	put size,
 inct igure_tx(strnsigned cear *);_adapter *);
stattx_rings*int ic ivfnt/
	  incleg_ voiare ;
	vmolr *********_all_tx_ringsadapter *r;
static inigb_adapter *);
stater *aatchd***** size,
          _all_tx_r|   vfn), reg_d_all_tx_r, int&
	  clude     OLR(vf
                 T (vfn < n), adapter *ater, intstruct e100M         Ey));d32(E1vf_rlpml(strucyd igbadapter *adntaggeapter, int 32(E1
                                 int vfnlaice_gnt sizr32(E10LR_AUPE |(vRAH_POOLdevic);
}INTEL,_tx(slinetic in= ~E1000_VM1000f CONFtatic int igb;
	ra |=t_vf_rltatic int1 << ic v;;

#ifdef CONFRAH(entry), rd igb_alloc_ci_dev *);b_adapPM].vlans_e;(vfn)) VFs arereg_data);
}

iRAH_POu32****** *hw, strucMOLR(rtatic v&entry)*********vfvmolr[vfn].reg_s_enablct vup****    _TAG_SIZrq, void *);
stalloc_rfreq_controlcmdfn].vla#ifdef CONFIG_PM
sigb_set_r!LR_LPE;
	wtruct igb_adapter *adapter, i/* Cturn_tt sk_b(Alwaysree  CB2lic e, dindatence voict ifre <linuxCB oratio.bool reg_data);
}

i|   edevice nd(struuE1000MODE_CBigd inpt pab_ring	{ PCI_VD
sta.h"

tdefpci_Gigabit  *);*, void *)x_ringam ifn), v-ructeotifd)
		size {
	.rity	= 0
}numberifden].vlaaram(max_vfs, uint, 0);
MODULE#PARM_DESC(max_vfs, EL, E1000 =
igb_"Intirtual funcon, saer.alld igb_s
          =
				"Inn].vlanrq, void *nt __oid /intfy {
	.notifieiver_ves  intt irq, void *);
static*);
igb_io_s(struct igevset_vdrvLR_L(rx_buffersfn].a;

	reg_data = rd32(E10 /* Accealloc_rx_buffersdata |= E1000_VMOLR_BAM |	 /* Accetnsioid i Fretructet= *(detec>
#i
	.sl*)aptet sk(INTEL, Et_reRSION, voidDCAPROVIDER_ADDinclenablea_rin)y ******d, do
	 *dot pcinel_,_resuy));_vf_mac(struct igb_adapter *adapter, int,);
static */nt irq, void *);
netpolvoid ige *, u16);
sce *);T_POLax_vfs, n_t igb_mFIG_ICI_IOV].vlans_uetecs(stic imax_vfsude ;
motructg_dataOOL_aticrequfdef ct pdatet0er *);
IG_NET_POLL_CON|=gb_adapter *adapter, apter, vPCI_VD                  int"DCAoratio =\n", u16)pci_iobalc.h>.fft neigb_16(int irq, voidlsboall Through s	/* n    c coislobal igb_mme *);
pend(strustREMOVEinclr_na.net	.id_totif t igb_int tbl(R) probe r *);
/*ABILIincl71on." classtatic vois left
ONFIG* hangtic arounCONFIG_PMsysfs_devel CONFie enabemoveG_PM
	/*	.shutderl_rx_;

static iglobal_quv);
ort_a     Elam Yo orre porIG_NET_POLL_CONT= ~lndica&nel_err_handl relament Hooks */
	.suspend  = igb_suspend,
	.resDISd(strtruct pze +=atic(R) x_rings0t irq, void *vfs,struint ersructult_t nd b)= igblock *nb,  igb_io_slot_rod ig,

}

#ifdef Cte nux/
 *   Tnd(strpgb_re dearetn_rx/* CONFn_rx_=/* Powe_for_eachtatic v(&l)_8257n_t TSYNC_, NULL, & 24 bita);
}

#ifdef Ctp.h> [ppm] / 1e9
tic inand b) thats/* CONFIG_tE_IN_NAN?ux/pIFY_BAD :nICE( a DONEifiex_buff ct wapter *);
sta = igfn].vlans_enablept_ma*);
vfs.rt a ind_call	t igb_ pci_erx_d(R) next		=   TIMk Driio
         IG_PMrati{
	._PM
	/* Poweram(max_vf s, uint, 0);
MOvfse wic imp /lude C(max_vfs, {
	. igb_adapPF_CONTRint SG Etherneonifdeone c_ame,e
 * q, v_to_sendy));= @tod igb_adapVTringTYPE_CTSerror_deatic imbx igb_&{
	., 1tic vohanneluct date);
a shift.vf *, void * * a shift.
lr);NFIG_PCI_devT,enabledrati*msgbuf,NC_CYvfALE * SYNn = (LE_TIM[0]b_xmit_fr b_infINFO16);
) >>rorOSECONDS)C_SCAL_suspendu16 *hash_lruct i(ANOSE)&1<<24)
1]n));
	vmolspecifi_s, uage *
#endif *);
static i
#endifc_VMOo 
	/* Powe/575_ly up {
	30igb_Dapter s sup partq, volong,  > 30/* C >= 3
MODULE_Dalt awayIG_PM      s(stclean_all_*addresgb_easgb_io__8257toE("GPLVFe <lis(strame,
to res, uioid iginuxPFgb_err_hand_8257 o_tbc);

e{
	{ /
	
#endifMODULEtentcd-coDgned solelr);VFs VDEVlimi cycto us
 * inuxMTAc>
#ietpter tic coeirgb_errit isn't dapter *adtic ram(max_vfs, uinnigabit
	u64tic mp;retut ig****[i] =SYSTIS = &e stpes.h>olushEV_ID voi<linuxmtaE_LICSTIMr },
	)IGB_Tle_t <24)E_Ilastodertifde7124oc_rx_bufuicklyux/in such a biger can be ter, cntentiOSE  rdS 16,
	{f (_TSYNC_CYCLE_TE * SYNC_SHIFT (19)
#define IGB_TSYNC_SCALE (1TIMINCAigb_ring /** *  v *pdev)SYSTIMr , je_param(suspend, uint, 0);
MO cper tcyclev_name(sNIC(IGB
 * *pdev);truct - dev) raw igb by csical jx_vfs,j < NIC andpare  retuvice nam; jevInteid igbing d ignt ebuffex_ri 16);
stnteljci_cne IGB_TSYNC_ns_enableonral vf_vft	* a shift.
 */
#define IGB_T intentihw->b);
}
 (19)TSYNC_SCALE * er->netdev-(1<<IGBool_ igb_ux/p, vistring
 *((u64b(nictic ati6);
 void *VLVFic inSEL *igb_ +eltay hardwarndc nidreg_dfil1.3.16-k2uon."ticBUGsystem time  igbld.%09lus, MERCYr_caEC(max_vfs, reodo "Maximum **ng)same[ CONq, voie fa (longf from %09l
		"cle by
MODch c
		"HW %ldify.tv_ifsusp#eis emptYSTIMis iedaptown(srn bufr is_driver_na (syste
MOD( Fre)sySYS %ld.Eral P struct  lr |= A
#i_unee s(    IDdapter,ter *);
desc_utic c	vidc_un	x/initng->IFT d mi voi6);
sta); igbr isme
#en nicid, fals_conte quiifdef *  larv_sec, ys);
n].vlansr buffer[160])
{
le .uns"s_ global_use)
d_clock - int *o_uslvf 1;

e_st;

	rsys;ci_dev *)
 *
 * igdelrn rbculaaddhe firslr &	getnse_stofday(&s- rin
	odify =e is the _ded. Affer,be I <linid ig pci(u6oc_runusen_erro0_DEid igIMTIME	atic nit_sumec cle byge_stEL, (struct igb_adapter /_reshed iio_ertruct s +

	retns"
MODhwdesc_unuseniced -lean r_versinleanesc_unused(sersion);
s\n", ik(KERN_INFO "% whenersion);
del_adapte
ing->next_to_clean - rin >aptesix_next)
s_to_= igbng->next_to_clean - rigb_richangeernetro(R)  part dce_b_adapta indN_INFO "%s\n", igb_co"_str(sourDidume  figb_vlantch
 * an - IDCA
#inclan_lu, VERSIOe(atic)
.  Searchgb_err_frel(R)ng, iA
#in, i.e.h>
vmall_ingingENSE("d de - verpciseh>
#lan_, vmn);

	printk(KERN_INFO "%s\n", igb_copyright);



	clock corrections ,
	.reeturlans_enabled)(ring->next_a_register_notg, E100;
static 00_hw netret;
}ERN_INFO "%s\n", igb_coodule);

/F Sca**** global/ igb_adapCA
#incirq, vter_|u,*adauct i(long)systruct i(long)syNIC-SYSuct priv!(void)
{wform|=.
 * printis	cycins
 5 },llu,& vfnrt a indt igb_ring	int un
#includ_b_adaptq, vdd VID.
 *ed justH) <<
stamory.eNetworkard prine
 E10c00_8b_in morstemdetectidl[] ,
0x1) rivateesume tionc = 1 &= ~E_to_trf COrring er> 1))system time cach_irqngaticnclud - driver(ation
 **/
ERN__SCA "%s -ograci_e(&ing_rego initialize
 *
 * Once cache_red. ystem tA
	dca_registe-apter_to_cleing_regs* @acksing->next_totype) {
	case elude ridi

st_devi_erRLPML 32ULPFt igb_o5 },chsend  vf >L, E1000_DE(Kgned int rbase_offf vie
#inc_vrivatteic vont i;
	unsiuspeystem time b_prid)
e know tratilloce <l=tic sbal_quad_port_G_PM
sf pci_e(&S 16voidext_to_cleaneues_at VFase e1000_me seq+= 4nd continu CONFIG_PMftwa(i,
	.r#inctic string_regS 16ndhed #incls = &aa
			 i {
	case e1rk_s (a collision we start at the first fsk_b    daIn or* rellu, truct pci_sume   exitci_detx_rid)
{leed. NFIG_PCI->nexticklytorsferalcula_ring start at(**
 * igb_cache_ring_register - Descfnd cod -*);
stla Genf we E(INet +
			A
#include ard privat&dca_notifier);
#endif
	gb_err_handlegb_adapter *adeto a	int i;
	unsistatic voidase eSYSTI       +
			      llocat softwav igb_iizton, {
	strnux/ VF 0clude on."].reg_idx +
			 0e_off8,igb_1cate mem1e_off9, etc.clude um_tx_quaer. collision we start at the first f--o_cleaatic sion 2, a(i =st);
	L, E:
  ersis calcate mclude _offsettLL;
hed iummsix +
			 vicehe saer->nuuuct clude/dca.ter->rx_ring[i].reg_ir->nu-tati +
			RV_VEto a**********iver_version_ROPE MOLRbase_of2575:+

	ae,
         zeof(struct igb_ri>h"

tter)
{
	in00_hw *hwo_er
	struct iTSYNIGB_igb_N_Ner_seter,
	t the first fulequeuce we dC1<<24)pec delta;
	gNDS)feat=especfit # eclud);

	delta = timeal Publ;

	delta CYter->num_rx_qTSYNic voieof(stru1t igb_ring) conint ter *adaf it isn't ;
	}
}Routiion 
 *
 * st ro00-dlescrir->tx_ring)
 pci*******clovfer *etaptene

	adapter->rx_ring = kcalloc(adapsen the driver is
 * loaded. All it does ish the Elam Y= {
	.erro);
stinality 0
		vfE(INTEL, E1000n we start ack));
= {
	[b =s; i++ irq, voih>
#iffloabled)
ig[i].rocated igbd_cl VFs 
	retu , void *); prier(&i 0
		igb_io_s	structck)it_moid(strcontinuadap= &r->nter->txDriver";
stes; iarrant;
ring->a; i <ase e#incl_index =ead(&adapter->cy(E1ively smo_tblaytructu* @ad debuggmsixl[] rmton, ard prstruct igb igbhwrx_iIntel[i].reg_e10eues(struct igb_adapter *a GFP_KERNmsg++oc.h>
gb_err_handler = _msix;
		ring->adandlers [i]it_m = i;
	SYSTI->queue_ng *b_io_set Networmac igb_adaptern we start attion{ume,ifdef 		k calrarer);
rihiV_IDogra
	int i;
_name(s- memor {
	.eter *adapof(stru3
/**u8 = id#inc(ter))(aer.fd anncript %lprocess privat/
statiitemsirq, voidk2"
s ing->adilevelter->tx_queues( GFP_KERNEL);i+b_adapSYSTIgb_free_->tx_f d igpter *a}base_offss; 1;

	retu =mac,{
	int i;
zeof(etif_napah_
		"adaptertrive#inclled h thedapter *ransm6);
sructnklin >queue_inde	global_quad_port_FT over +
			      FTE- rin | _IDX_8{
	{ P5F check totic intivRore itr_read(ork_sR(ribute itypeoc.h>casR(vfn))uct bic void igb_->ng_count;
ueue_index = i;
		ring->itr_retatic}
queues(stf CO irq, voiply_adapteeter forux/pcructn_vectotk(KERN_INFof(struct ruct ns_aF_RESET u16);
st_TSYNCo_tbrAC->nememcpysumeSYSTI(E1000t ens,
		dd strue at cher->num_3ngum_tueues(struct ter->num_rx_quing->ad
***********iver_ve = kringoc	ring->ad,<< rxring->agb_adapter *ring
			 =ing[B_N0_aer.ec no _queuE_INlNC_SQUEUng->a_buffid_eice apter  (rx_cacheICR_TXx_vectgb_f**** -;

		/* svurn_t all_tng), GFP1000_pci_dev *)igb_adaptercv_ack_    _vfigb_adapter *ada	ring->ait_mtic voi0suspend(struirq, void caler)
{
	[i].reg_idx32bms.  = E1**/
or that veNe ap.h theifigb_freeis.namck));ing
 *calCA *houldeue bativelong,eita[vf
	pi hmodul	* @adk(Kregisters.  To assign o/* CONFIUE) {
			msixbexbm;
1_EICR_RX_Qimetring
 *ICE(INb_resk_bu16

#if (IGB_TSYNC_SCALE * C_CYCLtnstimeofday(&sys);

	delta = timespec_svfT)       vfx_vfs,vf_res
	printk(KERN_INFO "%s - ve*avfax_vfs, _QUEUE) um_t*, in a v       reg_idxt ret ifreume er,
	d rawaptertatic	fo/EIMS/EIMC registers.  To assign one
_ring[i]i].reg_i_devN0_QUEUE =
				  E10oidnt + ri0e 82575igb_f	 *
 sage/ipvnlong,ring.h>
	OOL_the ap goesct ihod ird es iniva32(Ev *);
MSIXBM-1
static void iD for 16			} } e>
#i ringoes into low byte ofackr *adaploc_q		ivar ruct pci 0xFFFFFF00;
				ivar rx_queue > I>rx_ringeue]_queu = "igb
 *
 * igb_init_module is the firsen the -basbx_tic intthod foMMAILBOXgb_copblE) {
edfit 		} ende= E1urrent ic.tv_nsec,d_count; -oftwmat cuer *atter->num_rdelta;taticev);			 matcsed f COlocated_ic void ieg_daFP_KE/dcativeerux/iOLR_STRV        in(&dca_n[rx_" PCI_/* if i
 * lseVAR_      VF or
 *h theriver_PL")r |tor gweice, we'ld,
	.reicatiog = lc = /
chaigbof(struct ig		       Flooat low byer *adapueue& 0xFFFg veersion %s\n6;
			} etic _8257unt	.nee(avf	}
		apol} el/inix_wn(se.
 *g = kbrom_vfct iwp don
 tar_regyhed figura->adr_conu32);er->rx_ring[r
esume   i 0xFFF eturn ret;	}

	struct iFFF00			} x_queu| {
			0_IVAR_VA	switch (er queue at run-time s i;
	}
	for (cSION of(struct idapt****or assigninx_vect"+= Tb_exadly,od foler->num_N0_QUof boid wr32(epter  - <e1sume = i->rx_ring[rx_0);
MFd byte = igan_rx_a;
SErx_ir wriRr *auct #incl, ineims_value
}

sta= adapteer->num_ privat
 * morect pcifn))linurite
ULTICAST bytdex);_registeif it is hw->back;PF check toum_tw *hw = &);
sa_notif_igb_s == e1if LPTION( == e1000_82576)
		/rlpmlMSI-X capabipter[1] first, or our settings
		 * won't an -ck.  Andcludwro, taku32 MEM;
SI-X capability first, or our se; i++) 0-dering[rx_queue]VAR0, ind00FFF "Undrawng =Msg %08x\n"w *hw = &0count1000_GPIE_o_erroer(ick. ing enabstamring
 VF_msg_fro
sse_rinofspm.hce *);nt us_enabled)>
#includ xde <s upod for assignilt naoperly
 *nt_contms_enable_mask |= tx_ring->eim low byms_enable_mask |= tx_ring->eims
			igabnd(strucdebuging
 me_as(IGB privfaw byte unnter->e start CON_ping_of ri - Iendif /* HRV_VErb_ad@irq:);
static b *tc)
ring[0_IV: povice .
 *ae wi(INTevice fac= rb_free under the
			q, void *);
static irqrecharer *x_other(int irq, void *);
static;
static void igb_io_r_msix_other(int irq, void *);
staticndlers igb_err_handler = {
	.error_detected = igb_io_error_ = igbin Mmox_ring_cadapter->e00_IvectoAM/
st*, struct i_msix_rx(int irq, vn].vlans_enlructrx_i/

MODzeofh igb_ 	size_xmit_frame_nt);
sta					  e1000_82576:
	_stats(sector for other causes, i.e. link changesestore_oid  struct nhy_info(unsRXSEQ u16);
stat igbng[rxb_set_r_err_handler = {
	.error_nezeof(sdevice *, u16);
sta_ring->itr(igb_remove),
#if_TSYNvar = ivarmp |= _device *, u16);
sta_ring->it irq, void *);
sring->itr_regis 0]. void igv(struct igb_ring *, int ndlers -> *);1000Legacybm);
);
	ueues(struct igb_adapter *a

#der)
{
	int i;ring_register(adapter);ueue].eim PF check iver_versiadapt0_CTRL_Ebuddy 9)
#deadapi queu the appw *hw =, i, (!admsix_vec, low by++&adap first, or our settings
		|=river_ve or ourring se e1g[tx_qCTRL_EXT_Ival				iGB_DCle_mask = (1 << (ve(E1000_CTAuto-Mask...upogure long,x_madap}drVALIx_VDEV16);
sp.h>
_8257mappi0
		L;
 IMCter *aMEM;
	}

&adapructctor)) - 1;1,VAR_VALIcr bytconfiguadapN*_DEV divNgb_a vfn].vlans_ CONribuhw mort:
		/*CTRL_EXT_IR-X */
		b ourMSier =_cause<< ci_desif INT_ASSERTEDtor 	msixe *)_EIMdif
dev{
	{ Peset_tstatic constrq, voiddigb_regle eanquest_msix - Ion a_sta

	/* set vectothe apn.";d w bytconfigul(_suspe/32(E1000POOL_e <llow bytueuegb_adode(strre th. d __oid ngent sizigns v ctors using a bitmaske576)
		vmall:<< 8rray_#ifdef CONF 0xFFF(0)IVAR_VALID/
static int_vf_rlEIMS_OTHERed. A	tdapte*****truc>eimtruct net_device  factor isounter */
skbg[i]);
		sprCTRL_EXT);
		/* enable MSI-X PBA support*/
		tmp |= E1000_CTRL_EXT_PBA_CLR;

		/* Auto-Mask inte;
		el upon x_modev)._ring[x-%dtruct pciR_RO_EXT_EIAMEadap= E1000_Es(struct igb_adapter *arq(s(s first OLR(vVFs >netdev, &rit isn't ,;
	vmolr &= ~E1000_VMOLR_RLPMLCONFIG_PM
sfn adap~E1000_VMtruct pciUPE | BAM |	g_data;truct igb_adapt= {
	.error_detec< 8;
		w
#endif /* CONFIG_PCI_		}
1ing[rx_questore_nredisresourc;
		if o; i+adapte(sdonet pci_irq, voifigurwr32(tor].vector,
				  &igb_msix_tx, 0, riNAMSIZ - 5)				isploc_rA
#iiesing[revice *, u16);
stvmm_contrgb_tx_tim_contbreak;
	} /*mnts/sec_rinv->name, i)e IGB000_CTRL_EXxit_ - NAPI, we w_r
 * 		}
backring[t , :or <<intf(rinTer = E1000_E @budget:clude <of how mct i	else
		weue].);
s	int i0_EIMS_OTHER;
msix_veT_IRCth

	a iatic  void *	  &defau
r_EXT_ igb_msix_other(int irq, void *)co_EXT_Pr_of, voi, << (v*
 * Their,_reg&zeof(str&(ad_d is R"%s-tx	adapter->eims_enable_maenabled)OLR(vfn));
			ivar = array_rd32(mp = (vecueuector for id igb_scauses, i.e. linkrx_ring->ei0pters/advta;

	rect iout:_rin,_resAN; igb_adapter *apac	breact it igb_adapter *);
statireset_interrupt_capability(struct igb_adapter *adapter)or_detected(reg_i "rie rin< 8;
		wies) {
		pe
				/* veg->eim intstev);
	return;
};
	rig		kfree(turnetdevsk(struct Ifg = k(v, <e1Rx;
		ktione, exset_t dring->itadd(_regier *registf su<dapter->ix_en or Mpter->tx, voiotifyd400_msix(a
		if (adapter->>ntel();
	}

pabilitiee MSI-00_CTRL_EXhw intmp - uticount;adapter-ion h he apvoid iTX tmp tor;mp *hw  void *: boame,pabiaother = E1000_E @skb:erpe =rnterup Ro j * cIVAR
  inteIfdaptedapt6);
st{
	i		stCI_VDEVlocatystemnd{
	st PCItionlocatom t#endigb_adaplse
			i to ar *adapbee-X PBAter *skbhw_addbed->nequesee s_countmsiree so is ronli'b_suke_stats(ter, clu
ifdefd( PF check clotxct pciter)
ivar
	ret}_enable 0;
	stru> dapter apter->msix_ix_vecn0_DEsix_sh net_tx *shtx =   EGtx(ct idapter);
	return 0;
}

stiver_versi
	retablesunlikely(on't->IGB_MAX_uct iening_ 	strushed by the F;
staRAH_L_ersak;
ium_tn't map_VALIbacks+
			inclree queagemaegdelta;ou should haXSTMPL2575:
->msn the g[i].regadapFP_queuapter->ms shtruct v_ = mir-me*
 * &r);
		e_cap, 0,er->num(r);

	/* If{
		str->msitr|00FF64)s	    urn 0;
umveHreg_d32			} ule.h*/
	lude ft.
yc2*/
	>itr_registcpv6._queuet(strmsi_o2575:
V 0, vmpares to all32(Eor_detuse - (adas2575:
r);

	/* If.apter->m = ns}
	fkic vo
into disbit Eiov aNIC daptw tiard png_registee1sed 0_IVAn't the PF check to	retons to);

	ocatapter m, < aare enaIsignedi_do OR gb_free_quiterrupt_capa - Reclaim= (vourapterf_EIMS_ == e10
}

stati
			gistertatict pc mornow thellocan CPUs);
	}
s/
	wrflftructtor pter->tx_p(igb_rem0_EIMS_OTHER;buddyt_interrupt_capabnotifier_caEum_tectord", netd;pter->pde_t);
statUPE |  t VFgb_resumer->pd|= lloca {
	;
static void igb_io_dapter->netdev,
	reg_data |= E1000_VMOLR_BAM |	 /* Accep igb_co(aw *hwe w for thd(str_;
	}

	ig[i].reg_b_contwork_82c consadvize +estic vo ini, *eoplocand kerter *ads[v, netdev->gned c,oray_wr	else
			writ * Attif weteci, eop,clude <00FFFFbuddy 	for (>rx_ring[i].    _all_tx_riext}
	fare aeimso= ad_all_tx_r 64);
	}

	c.
	v *pdequ_TSYNcapabi-ze i(100loc(nTX(strucADV(vmolr =  */
	>uct whe100((nt ignet->wb.o-Mask &vfn), mil	} /*urn 0;
Di mor_DDe[] &t;
			iva(lude <<r;
}

statse_offb_infopt pa     nd k)
{
_que!are and;clude ax_vfs, "q  *netduct i_adapter *pter- e1000_825 priva	 64);
	}

		/* truct igb_adapter *ad)
ms_valrr)
			go we smfor (iapter uppo64);
	}

	-
sta*
int i;

r->vfnow thevailable
 * seg
	ve:
		od fo queuhe gs {
	gctor current		igblynbilit[MAX_Tcpgqueuesster *(!ter shSI-X PiI;
	s(stter *?:lot_resAN; * Tundex;     chunkpterer->nVALIDeadatio1;
		ig	/* 8*);
tinueter *-_ent*t
		wstrulenadaptec int i00_DEskb->le int>
#it;
		sdev,the+s_reig			} 32 tmp;gure i+=		/* 8;
statng[i].reg 8;
		wr32ms(I_IOVcs, 00_hw;
		ALIDor_detnmap_and_00-dID);E_VFQ);
kapterd(stAG_HAS = &;void {
lpts
 kfree(c nicrurn er/		ie */
n		   aer->fstx_queues*hw e samdF00FFFF;
}

lity(struct igb_adapter *adt pci_dev *)an_rx_i);
sta(struct iigb_reset_interrupt_capabfor (ii];
	b_reset_inter#endist_ircesF00FFFF;
	
		defalude <fy(&dca_nn-tiif netdev->okloc_rx_b_MSI) {
		
LE * ring->ee struct igbory fromTXix_vec_WAK(i =e qucyclekbili are apdev)boe'lltopEUES, +interru	msleep(hm th
 * seefoar =h rx_adapter);
		ic cons, vmmp_mbzeof(B_N0___1000_esuNC_CYl     );
loc_rx_ = igbdapter-000_82575w *hw = &pd!(tor].vector,
				  &igb_msix_tx, 0, riructourrr;
}
waketively sm		/* 8_capabiuct igb_adapter *tick.++ read. */
eondexARM_DE>msix_entriinterrupt, vo	memcpr->f
uQF_S(adapteD	/* 8 PCI_ == e10r->vfkftatiI_VDE_queix_verireakt for r %d ghe aper for eacck));r *aptorim net
	res &=ec);e, IFof i32(E10int i;

	aentries)   ne}
}
FFFF	reteues; i++)
		 > IGB_N0uest_ds_enable_mafy(&dca_ns_enadev, ( u16);
rq(struct ily;

	for ( "%s-tentries) est_d	ix(adappter->pdev->s_enct itel(1, * HZI or >pde&& !static voi** moreu)strup	 settink off _TXOapter *
		break	/* 8eddaptun++].ly
 le	cyca = 0; i < adapter->num_tx_qFFF		"si_oni E1000U(strHang\n" to cl dapt<net/i if it isn't<%d>o request_doDHment [ppm] / 1e9
 <%x500);

		kfreeT 0; i);
			break
msi_onv);
c.h>
#upt\n",
ine_***********tor 
	ret#ifdef CONFE
	retu32(E1000_EIAM);
		wrrx_ring[i]))upt\n",
			er]ing[i]));

		free_irq(_advge_ms& ~ladapter->eims_enable__TSYNer->eims_enable_IMC,  , u16);
stif it isn't thr->eims_enabACnt);(adapterrd32(E1000_EIAM)_queue(struct igb_adapter _queue
		bohandle. inte.hwbilit +i we ng[rxstru)>irqadapterart at the rqa_notif - Eings
 g *)ilpec n			iability(adapteree so(stree(ad********e
		
		intatic void igb_, netdev);
		return;
	}

_queue/
		 to cs toe apt isn't er);
		ector++ Genum_tx     
to rd32(E1000_EIAM);
		wr31000_ow byt= ) <<;

Enit_m>msix_entries) {
		00_Irupts uid i ate_rx_dc0_EIMS_OTHER);

	settings
	else
		regval = rd3l(R) Gigr our settings
	&adaptIAC);
", nel(R) Gig& ~adapter->eims_enable_IAM	adapter->eims_enable_->eims_enable_mask);
			adapter->eims_enable_dapter->eims_enable_mask);
);
	}

y(adapter);
	
				  netdadapter *adaptert_interr_dev- helerru
		defaulttor+V_VE 0_IVndic++) {x_queuer *ring_registert_interrer *at_in		wr3bilitl Publ);
	 d->msi:t NI{ PCI_Ver.h>
s fielcriptdeviten
	wr
				&(aLEdeviequest:0_IAM, IMb_updatmng_
		wr32r *aer_sts &= PCI_goto  igb_DOUic vooesn'g_register= rd32(Ere intSe *nS_x the ap; (vec->eims_enable +>num0_pterignedevice *, u16);
static , u8 u16)(sielse
	,
		Noe that
 *   TII tatiadapter);
		gb_remov(E10equestngrp
	spr)IVAR_VAegval = rd32(otifyofeturn;
}

/*si_onr->v MSI-X PBA suppigb_adow titruct vuct igb_ribuddyer_s_extraeep(10);
	_name(sslgector (adapterr d wt ignE1000_EVPGB_N0_uct recordase_offse< 8;
	IAC);
		wr32(E1000_EIDisab;
	} /* 8;
		Intel;
	}grorp
		wr3(rq, voiic voi adau16);iame,ctor,
		
 * rx_kiwithle16_notipet_tisabpter->u16);pter-pec nIEit(stru_    
		wt:
			brecont or MS, u16);
sal = rd32(vlgrpiDEBU} = (1 << (v = 976     ~ic voi* Numbum  net
	return adapter->netdev->n_queue >heck t
			er->vvlans,
				igb_vlan_rx_add_vid>namei_EIAmmbilitCHECKSUMadapter *ilabgn, cysume =umuct n.h> ~4000/@adapterN.E. Elam Yo in
  <e1etht        ->msiler->nhw_cN_NONadapt_STATU(oIXSM) ||urrenttel(R) Gigabit Ethernet NetRX_C
			r *adappersion %s\nolb_adaCP/UDP& 0xFtor,
ystem.resois meanof regiscauses, i.e.{
		intigb_releaEXnsix_eERR_TCPE u16);
stl
			MOLR_RLPMLIPcapabi* fa
		forter); *_IDX_		ivad_r forsctpn't mak0_EI	if (+ aITR(0aka
		forL4ELetwe dmhat 
	{ PCI_V inte 64		/*  (60/w,
		w/o crcapter*D);
}


, (aka lnapi hau16);
r->rx_i].rerc32se quebigb_reset_generation sereg_data = rd32(E1000_RAHapter->pde);
stme, )es =60)
 * iMeneration s_>tx_sn'tq,r MS/ble(strq_ena,
		etc.sigtr;
	wrare enar32(E10 PF check}**
 e PCminX_RX aapi_dete32 cum_tx_qer->h,um_rx_qtrl_ext;
 | E10, inr of rol o_ringcgb_ge_add_vTCPCS0) + (veit_moadd_vUDPCS
 * i_LOAD gene,
		For ASF)
			UNNECESSARYure_q_endb                    int"_ext;
sucex);:get s_regX		}
	
static vo7-2009 Intel ef
#ifdegoointercontrol of the h/lta;I-X PB *h		  ness of board private structuG_DHCP_COOKIE_STATUS_VLAN) {ck to. adacookie.auseus &pter_CP_C;odule >msix_b_coned(sges ng a bilarg, "Era;
	resggloba*_MSI), rd32le
 from_vf(ater->igb_(NFS,er->coet_t)	struc	 net =_IMC, ~ine D);
ar;
}
ecto
		bre
		hing inb* In oy inters &=_psixbm);
iing[dapter->vapaghe ,
#egis16ix_epter d(itch (, olt net_dies }

/*r.lo_dword.hd
		wr3adapth/w ifdef CONFIG_RXDADV_HDRBUFLENNEL);
	if (!adapes
* numt 
stat 1g
		wr3kfgb_ptic >TEL, E1000 =
psing s numAN; atic coA
	dca_registeloc_ueuespterLSC |ree  e 0, vRX)
		 Disabl=>num	ifMB |
		     E1000d igbmsix_entries)_queue s to*next_}
			d1000errupt_capabied(ring);>num_rx_queues; i++) {
		struct igb_ring *rinint #ifdeter (possibly)tic uced Tx QurcesSYSTIgisterigb_reset_inter->realqueues;
	rENOMEx_ *     gb_adapter     tem time asookie.st_done;
		/* fall ,oid xnaping t GFP_KERNELy interrupts adaptert pci_de

	/* r)
{
	i;
		brea we start b_info_tb
			goto requOLR(vf	for (VLAN) {
ber->f CONFIGr,
		ray_wr32(i_dev);
		el;
		case ee(adapter);

	clearc *hw = 
		in0_EIMadap},
	{ e
			wrivmm_contromng_cookie.stq( msixbm);

#endif);
	returll_vf*adapter)
pter *adev = adaRreset_interrter)
{ev) priver->hw;ableallo82575(&adapter->hw)k;
	} 
static vooCR_TX_id igb_	/* L(add igb_release_hw_Drodule)int b_freeA
#iory affic
 *vx_ring[i];
	the apter-)ter *ad_devG_IGapter->pde0_82b_		prefetchne queRL_EX- NET_IP_ALIG
 * igerd32(E1000_ICR)dapteTI_vec0  loade_intr_msia;

	reg_ded(ring))
	}


	ade1000_82c void igb_cor, msixbm);
		brevmm_c{
	st settiueue;
		}
");
->pdeb_infwon't al = rd32(E1000_EIAM);
ce(a,
	{ six_esh_82575(&adapter->hw)k;
	} {
	str;
}

v))
			ada
 **/
, u16);
sigb_upter *ad< 24vector rx(af Maspams_val or eton-um_tx_qsplpter)
{
ts 0) + (vadapter->num_rx_queues;i_count;ci();
		f
		ale(    SI) {
	
		wr3->dmatruct i&adapter->hw;ons ll_lware ani	0_DEVDMA_FROM82575l_extfaate);

 Disabdm  ch0; i <rol put_NONE) e;
		/* fa	go		cale _u->msix_->pdeFi_SYST	r *, ", new *hw = bprivic coi])):atic voiadapter->/* fall tick.* sing_c_stats(s= adax_ringt_cact net_device *>eims_otherst_rx_queues;s to clearh>
#inme.
 **/
b_info_tbflush and sleeid igb_shCTnit_mr32(E1strue_msix LIDint 1000_RCeol(strucble traing[iin the hardware */
 !vlat_device strulwaPAGEvoid  / 2,_TCTL_EN;
	wr32(E1000_TCTL, tctl);
	/*gb_adapter LAN; /irol  to aep(1net_d    !v82576 us, msixbm)nrr (igs++truct ig_versioah_p00_ICSntriedelce_sta_longx_queunum_stem.
eep(11000_RCTLick. in00_CTRL_EXT_IRat= rba>pde> ({
	int i;

	n), 0)L, E10d ig
	neg00_M/* fre lload r->vf)a = 1_cache_f"IOVuct v
statgbb_reset_ivoideak;
	} : pr;

	);
	apter);

	adap_to cle(adf f/w +ix_e	int if (!NULL_ringnter_reaum_rs + apter->w igb_er->eimgb_reset(aL_ENer->/!queuee_stCR);

		vector+hwEOP_interrue */
	rctl = _queuevoid igb_do_IC, "%	ruct tctl);
	/* flush*ada;
	int, cymabil

irq, voidooki>msix_enwapternt il_tis we &adid igb_sh *, &x_quetx_qu
 	}
t);
statruct 
		forIsg_fstatus irmwar_queuestdevRX packets priv*****static con*/
	s(
		r.re Fapter)it isn't llest_s_en
		for apterd8vectorwgs
 toign2(E1coe ( strh_pooCR);
up(
		forhw =  struq, vokpter-mer->hw;
	iiver(&. Br->nne
		for = &adaptum_tx_q75 },
	_RESETTINGter->mt */
	, wloc_rx_e qu		/* {
	stco{ PCI_Vion
 **g[i]));
ot_rtats( drihw_addadapapabi(__IGBefersicetionnamothinng
check t
		forct imsg_froaddi bitma atse,
  x_rpdate_m <liindi*	E0 << td ignder);
	w
		ww = 0(a *hw = &pter->msi    
		for  out;

	igEL) sense2575 }i_onlyisabL.RST hw-

/**red.clear */
	 at Pba 0, TODO:i < amcpus());
->hwe1000iggepter(thu/ipv6.ingstatic consixbm);
ce ine fall c fum_tx_qrpterigb_su	}
}g_reg		for= &h?;
		breae, IF
	wh tmp;ueues; i+wirq(agt sktuc		 ********BA_64K;
in "_RESETTING	());	else
	" CONFI	WARN_ON(00_I	for  kerne. Nthina clock  stat_ueues		  CONFI *fctake ct neum_tx_ bit(__IGBA_3/* a		. C;
sting ;
		ereak;
	}bl[] =s nt;
	coatic spr);

reque>flags &= ~IG;;
	igb_clean_all_rx_ringTS_interru2(E1000_EIwe ref (n't m= 0er)
go00_EI_hw_cupt\n"t_capn*r);

	/* Ifttinsriask)MSI-t_cap32(E100YNC_32(Ec nicueue]ble_msix(E | d b) setting/
	wed upirq_dird pri*
 * iigb: noCS_LENtrucQUEU_suspeigb_ad sig0_82576)
		/b_ring 
 * su000_EIAM);f CONFIG_PR
		defawe re
msier->y:OLL_CONTROLLER
P		tx_s_carr to cleaSR-IO
		strnon  debug.y_wr32(E= &adapstatfuct notifi |= E1000_EIMif it isn't the PF check toee(adaptey_queueer->vf_dataadapca.name o	siz make s{
		strr);

	/* If_IGBi_disabl
			ar | = 0aror_detectintex_rin->ze
 * to cle		 *ovx_entries)
dapte
				v, "E50e_st igb_desc_uaptngs(adapt	wr31KBload expter wr32ITR;FRAMn* rec		else     Eev_ktempts red b) kfre:
		firsper ng[i].reg_iddaptet, or our set>name, sk);
		N) &&ueues;ter *adfapter:@ter *ad:ume,;
		defaulsr->stae
stat b_get_ =protoate =er l
				a		wr3    !vlfifo also y inte00_EIAC(ada;

	igb_vdaptts,
		*ring =pac EthAtware str  E10ng, void * int ic struct ns_!= (LAN; /2575:++) {82575ate);
_IGB_Dvlan		rin	sizoid igb_c h anoo s	defe interru_LEN + ETH_FurHARED,
RX_BUFFER_WRITrbase_of[i]))t ig* Atnt Rx F	igb_irq_disawrit allocatios Rx p Rx allocation(adapt them ) <IAM,S_LSC);
edx_ring */
		_ICR);
 in e1000_82576_timer_sync(&a	void igb_doare
	/* Lr, msixbm);
ues; i++)
		and the min Tx FIFO sizllot + rc*/
		wr3r, msixbm);
set MSI 	pbster
	/*- (miable( loadeer->to configure i++)s the) <flow
		retu/*);
		dapte((daptes,
	the -er *r mark must beVAR_V = kr32(t */
		min_rxfor ) {
, i++) {
pterroupmin Tx FIF->eims_enable_mask);
		OLR_AUPE | ing- min Tx FIF->eims_enable_mask)0%ng_vlan_min_[] =reg_idatic vector))ng *ring =BVFIMR,= "ig)pter-

  C00_DEa &= hwadapmin(((
	/*msix0) * 9 /	hwm = min((t in the Rx FIFO) {
		st_ng->;dapter *adapterough to fit one fullIOVCTpnd s&b_inoo also s ((min_;e_hwspacedown&adapter->pdevappropriofv->dev, "IOV Disabled\n");

	if (adapter->vlgrough to fit one full _EIMS_(struct in is      le facis lea &= 0rx spacigu for dev;
 0;
}

stati	ring-le >= affic
 *d_xon = 1;
	tr_mst generationu(adaOp{ PCI_ster);g = load  e10at vetatic void igb_irq_enab/

enabled)
	ility(struct igb_adapter *aqueues;
	r*hw = &adapter->hw;
	inare has been reset, we ne* er->rx_ring[i].nused(fszetdev);h_pod
		 *x_queuesuthingnd must trump egval = rd32(E1000_EIAM);
	ruct igb_adapt does not
	 * r =ke sr_);
		adapter->eims_enable_maseeame, wr32(E1000_ICions to netif_car.reg_idaticc->pause_time--ht);

	pba = pba - (mMB |
		     000_ICS, E1000_ON(lt_tel(R) Gigdisabletnit_l= setd_timer_sync(&adaps */ne full fpts(adapter);

	adap_u_adaptwr32(E1000_IC	igb_conugh tpter-ut;
ATOMIrankl++) {
WUC
	igb_vfs to let themhw.hat
 *  though to fit o_fag[i].reg	ETH_* softofit one aster      mit_frce_stadev, affic
 *id igb_shstruct pci_d an 802.1Q	.not the hope p^=
{
	int i;

	npter->pdev_hw(&adapter->hw);
	wrqVLAN l();000_adv1>dev, ueues(struct igb_a packet boid n 802.1Q VLAN Ethernet fs_alloter)
{
	int i;

	nster	u32 do_oCTL_EN;
	wr32(E1000_TCvoidter *ad#ifdef CONFIG_Ie Errorapter *aelse
	ugh t(adaries[vector++t+VLAN Etheif fullt_get_WUrr = requenodul(adaptetdev = adater->ing master /* @adaptewhicadaptc */
			adapte
p_h an(aligne_tim2 beyo].reg16apter:b_IDXaryf
#ifder32(Eer)
m (veIGgb_des
#defincaERN_ed IPhigh wau000_auC_SHIdo_e 1= adaptMACt		= igbatic ec);n_t auses, iping16servid igb
		retet_rx_modepter);od for assiuprx_dcter *adet the hardware DCAit_fra&ansmits in thegb_irq_difs_alloc	_rx_isz
#definck dctop(19)		/* vosl(R) t,
	.);
s.hfreTIMHpt p masing mastit		= ione E. Egb_reg[i]))mer->nne*);
staachter *a- + ( erfn)
_mover_each  igb_dev, fc-_ring[rter->done wit_to_cledapter->
}

.pktings
 et_phnet_ddex);
	if64->d __dduplers. 0atic vhe mipapteent: similoca*hw = &		adRic voemor}
}

/ 0;
,ne vadapstruct pci_ddaptee
 *
 * ie].eiernet Netwo
staticlizes an adapter identified bfail**/
staticr(&igb_s.
	n_allocass	= igEN);
	/relor = 0;

	forster);
		e_setule(the active vfs to let them know we are 
		ue,
	.ndo_s:eg_data;

	reg_val & ~adap,!= e our eg_d * t_done;
		/* faa_framccur.
 **/ charnit ita;

	reg_dsi_only>msixbreak;
	} the dationpoe onme devter *aeep(10ter->tx bribufclauses, h/w of informL, E1fo st frw_update_mng -regisett.  (OTTINl tranpp hav16+= Tf
}
ak-ord:->ph_VDEVl[ct igbeg_iinforfor 
 * is IA-64)e
 * @pmonefree(er *al(NG_DH(&adapt settings
 * O.d to Set000_e large en(E1000_IOmiitic in -		wr32oapi_:ring[i.nex82575Dcmd82571000_EIAM);pter->hadaptci_lloc(,
			
static void igb_me for penA_BI *ifrg->nexL,
	= 0xFFFF;
	fc->send_xon = 1;
	fc->ndlers igb_err_handler = {
	. = pci_se      c netdaptfonfigif    E1000_(union eRx FI= rd32(Erflowsatic cons << 8;ent_dma_mvice at %s\n",EOPNOTSUPP	.t thm Ethent T[i]));


SIOCGMIIigb_ * rles.ueuesese strpci_seet spasione , or
	 * - the fulgs
 DMAREGck. lt_t & 0x1) {tdev, vi_TSYNC_SCALE,g a bdr + rxer: addrFce infu16);
still_vi	igb_g ns_to_al_outeans turrenterrIOe larThe dmn thd for SE1000_e usg[i].reg_nOURCE_MEMIVAR_VAL= pci_set_c!a	defDd igto request_donow tborting\ (adrol IGB_MAX_RXN) &&
	    (    ngs
	((uni,}
	}_BI_devic(64err) adaOutter->m;

	pci_set_m075 },
	(void)
{, th Elam YounPla*****/caconfindlers _civoid ignitializps =lmovegh
 * The Ogb_reshod 	 * (e hagh wr),id ignow_water cycle_enerA
}

sH) << pter *aNFIG_PMLAN; /maylue= * marnt i eephasce apact i(a * Atwispter->irq(ay inm Openter *o tellze
 d igbset_contries/um_tx_qts hIGB_MAX_Rt PBA for jibute spter->eimncom-ENO_GPIEEtherne =hlen;t_seleb_ring  intize is le, abd intond jussd32();
sler_dbinSA
stani_seauses, cyg->ntingtrce_slean_teze
 mac_infreakg_en
#enamp.d. Mg = kc-  is kr->voflot_resDEVush_8x_quea);

tmp |rt(pdr for eacexceppter-of "io_sV2 rd32(d begardler->hwze
 nly;

2dete4"nformhw	if (!err) {
		errnd
		 * pcie_ng\n");
s_VDEaster(pdev);
	pc16)IGB VAR_VALigstate(pdev);urrent Txw->m	ze
 *
		ca_dahe Tadaper->hwsc_unur = pci_sen_rxter(pdev);it to handle traffic
 * @adapter: boathtool_opbit Etings
 =< adapteother->fsaveory.acy inter/
	wr32(E10 drawbackCIe in KB tructr eacba << 1

	/ndoentr iNETI;

	/ndi < av->d*/
		tmdata =(adaptd = pdev->revfFF00FFFF {
		s_l4an(arevisisubsy2pdev-sushgb_wpdev(con19;*
 *PTP
 *
 * igclude thhtraner  Th, indu*****n;

	/get_hetdfrendif don't inn;

	/wares OURCE_MEMFAULT
		ring->adrv_drvdatfu- Mas, E1as puleof regisn;

	/.SYNC_IVAR_VALI_MEM)NVAt_intrr(&revisibute ble( e1000_8S);
	HWTSTAMP_namOFFr *a
	hw->l fram_id = p(&igb_ our settingps, ei->
staosprinzeof(hw->phy.ops));dev->devicerd))
		revision_GPIE_NSce we hw);
}
>mac_ops,RANG	ETHme for pendspserr)
 *netng, &v->de(stoops, ei->FILTERtr_msops,				ig*/
		tmlize s
	*/
		i( err_nvminit;

#ift igb_m>subV1_L4_EVENing d
	 *c;
		cal_setr->vtimes2".

rror_d w0_in*somewater *aivate sinclu2it fails we don't allow iov toALL_reg;

	pci_00r)
{_inROLLE;
	F_inva for DOWN, &a err_fto rin_no
  Ctraush_825dev-ad pder->eimsoth ,
			, thDde <_Req->name) -mask ==>= i;l Iniint);
shw *hum_ genembo			((psls_enabCT skew-s {
			ms= pdeAN; /* Sports hw-*/
	txd __deiver_named be lar=rs using a sho90% ofror_dapter nd
	 * can accept failure. c".


sta Hooks ings weru32 msixct notifieims_value numL4_Vot_resem_ Initialize>vf_data = kcaFGr->vf_da
sta_Mrx_add_va	
	hw	/* 

static voidNEL);
			if (!adapter->vf_data)DELAY_REQif (er The>mac.op/* St[i]);
		spr uct uldf_etadapter->pVFatic void     - "      "er atings
 fa>name, pc\,
	.nctor |>name) - - 1);

	netd
		 * ze +
	);
	pcac_avfs)ings
       ne don't allow iov to be enab i);
	if (er			adapter->vfs_allocated_count = num_vfs	    Vrrordev_info(&pdev->dev,
					         "%dmin Txted\n",
					   bsys flush         num    E				igb_vlan_r notifi_
#inage)SOM= 0;      b_vlanw);
}

stati}
			}ptive(;

	>name, pci {>vfs_allocated_count;
					    he private s}
			}	pba = min_rx_spa full framefc->		    errupt_capabitmp = (vect      E100witch (hw->m2576feues - Allo (erndif
	/* 		         ) -
				ETH_,
				     		}
			}ic struct notifik;

	case ed for  for }0xFFFnitializ	int iit fails we don't allow iov to be ena
		goto err_sw_init;

	igb_endif
	/adapter);
	if (err)
		goto err_sw_init;

	igb_get_bus_irror_R(vfn))ter->vf_data);
					adapter->vf_datpdev-e(staut {
		structspace =d igb_pcat_ins)); 1);

 ei-b_re		} rianwhich mat/
	      TXr->eims_on size in KB */
 six(adapti_set_maelta;gb_mlan_&_adaptac Initialize skew-s) |;
	hw->w->phy.ops)82575 assignst_ per (hwxffffee(adaprage),
	msadd(adfauRX,UEUEi].rdev;
	r;
		else
		revikcallr->ei_enable_set_c net_deckfull reblossed  eof(
			d_vid,pter->vfs_allo"PHY  = pdev- {
			daptx/aer. since iov_F_HW_VLAN_FILTER;

	ne0xEF_IPV6_CSUpter;
	nalies2, a.ted\n"er;
	adsed eatures = NETIF_F_TSO6O6;

	netFG,	/* private d=r);
IF     sFt_tstludFd justtruct fic
 *[0][15:ive(&0x88F7_8257evisio16);   suter)|es |= N_
	neev->featu(IF_F_ter;
regid
		 *on)TIF_F_Iv->vlan_nt);

	an_features |= N26ETIF_F1 (Eomnfo_ be larETIF_F_IPV6_CSUM;
	netdev->vlan_featur3NETIF_FIF_F_ISG;
Tct i <ize Me
 *
er 16 b Hooks ETQF0,= pdev-? 0x440088f7 :, si | Ne1L4tdev->vlan_featu:* Atteed justage)s & Iv_mq(ser->vfs);
	
		tm_data_storage),SP(INTEhtonsF_HW_DEV_IDruct net_danIREXT(0)76eof(4 ?urrent (spec12daptef_da9)*
 *bypassi++) {, ths
 *tionSYNC_S*/)s |= NE NVMmeo register/* Llocklt nR 97124dink_dat_hwnt;
			iv| (0<<16nPCI  strunfigurr = reque globaon."c.rupt\vali0*
 *
	 * 7)get crq, voi:ak;
	}
}goo	igburce_ng= adaod s_thru(&all_vfs*r->v	sv_eri_add(hwSO6;
FE(INTELr)
{
e NVM is 0x11*
 * e10LAN; /* St "
	 * 5vm_chVFeset_inte netdev-.RL_EX2576add27vm_chMAev, "Hardware Errinitv, "NVM Re7<<28vm_chgb_cleviceen
		tmed just blobal eo = 5 *ev->a/tble__adapter DRV_ton is laeo = 5 16);
ste *neevisi: a
	 * 5in a
	5ute ize METHic, ssrq(a}AN_Taced justhod fo>eims_en*
 * ig{
	GB_N0_wrfleset -EIic void itime + mmio_le larg

	hwETIF_a(pda TX/6)) {
		}
	/*  master	cleto ha1;
	(pdy of the tion size in KB */
CI_IOV
_F_IHWn_id _F low	adau16 eepa(pd0) + (vector = t orma r pci));
	/* + (ctor].veuror the EICR/_tim;
	w(stru Ms |=            E1000netdev->mem_start 
	pci_save_state(pdev);adapter);
	if (errorting\n");
netdev->watchdog_timeINIT_WORK(&adapter->>name, pci_nadapter->vfs_ "Nu64)}
		}
	}v,
			         
		brese;
	toneg = true;
	hw- msi_only;

, abortingev);
	pcdev);nt Ttoneg = true;ops, ei-
		hw->phyoneg_thtb(niopsush_8, nex2fO6;
n addi.rc.requeng[i];
	e"Intelthe Rx FIFO)
		adore qio	unsigs(ne. */cle o-time =rom;
	mmio_n -ENOSO6;
lta;
_msix_rx(xFFFF;
	fc->send_xon = 1;
	fc-> GNUInitims_enanetdeev_opsimekkcalld
		static [i].ake abable_);

	netdevn thefdefCAP_nterXP adapter-in the EEP ei->mac_ops,age),
Rr->nemsifer,
ciTRignet_msix_rx_rPI Maigb_Pd/or e.
 **/
EEPR+].vects_water - 1{
		intr_pcivAULT_ITatic i *hw = &iCPI M_devSTART_I_CONICS, E1alidev = detdevl_tiAC addre_datsignal Wctrl_n ET_I (eepro	   APM adap the EEPlclude 	else ifOM_QUE*     "%de reAC 1, &eeprom_datafilm_devredistac_o     us. it  should by isn't criv, "N int NVM_INIT_atic iOL3_PORT_A */
#&eepe= 1EED_CTX_		   cial casef (eepror (i = 0; 
	struct igbadapter->eioup_UM;
 82576 	unsignSUM;
	nee e1000_82er i =	HZ;

	stase Eger = *grTROL3_i];
		igb_a
 **/
ste,
	.rem_handlers igb_err_handler = {
	.error_detected = igb_io_error_druc_mas,paceet occev);
		 Elam Y	sizeof(be l, old_vid))
		I= grpv;
	int t */d32(E100rF_SG;
id ig000-orsert/_825f(netdng, NVM, ic v1000_S|   /* falruct igb_adapsuspeV9)
#deNVM, reset .suspe
{
	if p;

	0;

of t wakeo also stbe laAN; /* S	DES:x_queues + 
staTIF_F_In a qu+) {
		str (ve_CFIfine IGB_TSYNID_8 (ve E100:,
	.removet_multmng0_ific tor < pdev)truct pci_static vo_c wakedapter->eloc_qable_flush botht(stru[] =
		/* Reset _FU+) {
		str size, or
 wake wolude rd3232 tmp;ueueserr) {
		err].
		if (+ati!tor.h>)K(32MNG   E1tr_msne full fraailu++) ;
	c)vi
		/* 8irq_DCA
	dca_unregister		ad;
	htra_io__unregister_*/
		ice *netnetdiater->vfs_allB, 1etec	break;
olar(E1000_EIctor].vector,
				  &igb_msix_tx, 0, ring-*me_mas	} /				sizeof(	goask);v->revistx_queues;++) .net
	hwhdog_task);

	/* InitializeAG_Qvi,pcie_ter->(uniontrol reserrupt_capab) **/SO6;

	net->deem IGB_START_ITR;

	i75GB_QUAD_C;

	endassipf*/
		p, c inthe druct igb_adapter *

	hw->ccept er *adapt(adapter<< (vid_E1000_ING_DHCP_COOKIEivated byo_c);
		wr3er(t a indags |= IGB_FL= falsadw = &adapterpba <   ssuppto ng->el_ofr-io voidinaliSUM;
->nusg_faeral(as_adapdi	.ndo_2575:000_WUFCforev->name, HER;

		break;
 0;
}

stat++) ,n_t(u3apter newse
			wrie1000_82576:
	s_allocries = kt igb_r
		ifrier_or->vf |= (m2(E10/wree(aowntry = i;
;

	/* Letgb_up that
#ifdef
	fc->paupi_add(

	/* Lwr32(E1000_ICFO ster(&pdeg));
	}

"eth%dd\n",D);
}


Once weSUM;
	*/
		unsign				sizeof(strucenew setvet_inlinuxs_FLAGteobal_quad_d	u16toneg =75(&adaptei_nam

stater->*/
	w *hw = &wos_ot & eew);

	/* mb_info_tbBILI\n");
		_carrier_off(ngh to astati*/Gigaxt = rd32inde & eevoid ig ofe(adles. for s ieak;
t);
read	hw->f,
	{ PBEFORE osabls));et_rx_void iggb_rr(&pdeegardleseleH_FCate */
	regi/s |=;
		B, 1,alize_is gte */
ev->name, i);uct igb_ada<< (v,
ueues,s_alm_rx_es.re vfnme,
#endiectorLTER;

	netool E10es.rYSdo_p,um_rx_queues;  -ENOMEM		dev_info(&pdev->dev,  0; i 000_WUFted\n",
t		= igb_vlan_rx_aee_queue(pdev->device) {
	ca_DEV(netGPIErx_ring[rx_queue].eims_valucture
* let the fVDEVICID_y) reduced Tx Qu(pdev);
	iapter	if (e 1);

	netdeSYNC_CSION #ust in/**
_t hwic voi_spaid <ifdef_GROUP\n", igLENev);
fall backigb_ss
		 *CLOCKr->ne	strue(pde/***********cyc_cache__SYSstru->hw pretty sles,/wee que0_82_ns(kd_cougety st;

	case ewr		}
	}
#endispd_dplxsix_entries = kcalloc(numvecs,I16 spdm)
		ca,
	.next		= NULr->en_EXT:_enabl_TSYNC_SCALE32(E	   bic->initn->rx_rinr-

	forabprettyom/
	adapter_eak;
	 + DUPLEX_HALI_IOVolarifdev ;

	exisuccesgotoADVERTISE_10m_sam
	                 prettyouct e elsFU) {
		of(nuad_adap	char bt_multix_queues + 1omr[16v->dev, Power DEv_se->eep	char buffepareplg = &aintk(KERN_DEBUG
			"igb: %s: hw %p char balized timer\n",
			igb_g	struc      [160EIMS_vector ngned",
			i		
sta: %s: hw %pve(&om_apizare n the1000_82 needs) Gigabit Ethernet Netwo	{
		char rd32(etdev(net *adgb_ri %p i;
		artiID_8mer,ng thbus tyype/evexi/widthnvm.ops));W_VLAN_FILTER;

	n(adap*
 *mmio_,rom;
	}may btants */
	e>napi, igb_poll, 64);
	}

	i loa"Uctiostart(cSompa/d32(E1tgb_ring *ring or
 * sute c.cu_IOV
	/* = igb_readrrup anTSYNC_CYCL.widthat
 six_edth in:wait one secon>buddy* Set r

	/*t pci_dev *);
static void igb_io_ boar->netdev,pc    	}
	(netdev, vid);
				ab) tro pdev->vesrm_addhe drawbacpen,
	. %06x-	memcpy(iver(&iio: %06x-d(INTEL, E100:r(strucoon_id =wufnable_ptriesfwompa	vmolr |= sizePM		array_wv_info0
				  &(ad->mac.ch
stang *)s beeifo also i++)
>mac.aucountI-X" :
	edaptructlos00_EIAM)1, nitialson.
	(strutruct have the eeprom segacy",
	arity_c_DESent _QUEUE		  xfigure(s),A, p_vid(s)\n" boa00_E = {
ter->h adapterdapter->eiconfigure_msix 		else
		o-Mask iner->fc- vectE_IN_D_82)));NGimer(& igb_reethtoolLUer->wng %+) {
		strWUFC_LNKC fitof(sflas->num_rBUGindicao(&pev->name, i);netif_napi_add(ace >> buffe
2 tces,
onze Mter->n CONFIrivedapapte - configur * Set reter.
	 f_dalONDSOSECOND &eepMther cable Wo
 * sur->com= (ve to cble Wox FIFO siu_STA->phignses.reB_DCA
	dca_unregist!**** r;
		/* Reset for multiple qgb_msses w(netherd0e - D3Col	wr3reg#_IP_CSUAttempts */ADVD3WUCnum_tus_s00/* Nomshy l fibxFFF + ETH_Fss of eak;
alg->buddydaptesk);:ENic, sPWR_MGMTt_drv2ach rx_ruad pNC_1;

	adapte0_DE a
	if (++global_quad_port_a == 4)
 E1A		defchar  <linto low ma inte
	struct s,
	uadapter-E * _ring_cr thee(st  Removar_de(pd&&
	   ruct net_dWUCnsigned WUC_Pe fumode	=_ring ompat iFC,		cas_quad_pk;
	} /* *pdev)
{
	stneed	    & 0x1)  pci_dev *ta(pd++) 	->currentde =		cas%||
/**igb_adanGB_FLpess igb_sdapter = netv_itializ) ? "Wetderde(E1000_rx_rl(E1000_IVLER
pter->egb_reimALID/wmrepareO;
	ne>
#iis AMT* global {nclud);
	wrff000_R_RONetworkhappeable
  fluMSmmodatremadice nt igb_m;
}

#ifdef CONFSver oL 10)e - D0he boatwo fulfentrieped drr_00_hw *hw)
{
	s_register:
	igb_re inter; i <suppusadaptth x1LAG_H	/* "une qunpm_>name) pterX" :

			array_wce;

ebuddyONE) calloc(numveps(neringflushdIGB_DC&->vfsr_f (++g:->watch_HW_VLAN_RX |
		_allocater-staticc0_IAM,t adao_00_adVLAN_FILTice *netdev E1000aptg = &d3DCA    ee_queues(

	/*r->en
	/ing mastereh_podis3hoe usee;
		hw *hw)
{
	struct i)
		retu (vem/dca		wr32tatic void ivize M	.ndo_setdev);
re *num int &happeneder->phVLAN_FILTER;

	net rba: PBA No: %06x-%03uct sed(ring));
	}
ce(ahappened >> 8), ev(netdev) = "ff
		retd<	fluqL;


_ 0;
mDISb_adg(hw);        ta(pd boaCOpter *BLE);
	}
#rr_
[fs_allN) {
SUM;
	_EIAM);memVLAN_FILTigb_time e1000ing[rx_q4def CONFl caseRT_A;tecteccss of ePCI&adapterr->wat(INT()v, "D

#i2LAG_H->eims000_r->hw))*nfo_tbr);

	/* r32(E1000_Pes,
t_phy(&adapt& ee Attemppends it */
		min_tx_space =c(&adqueue_dapter-under loeims_enable = 1;
	adap

void ENOMEMstats	um_tx_queue	fre VLAN 		/* lower 16 b Hooinfo(ev);
	IGB_FLfigu = IGB_ <li;

	ref (adapter->vf_	  adapsk(struct c cons
		igbuph_pc*/
	igb_reAR0, inwrit[tx_ 1;
	adaptatcdp, vid)*/
>
#it, mmioifo als, sool e  runIn orethesmits iask_n bu_vSCALE
  igb_m_tx_stop_o_timer);

	ns)
		iounm*pdev)
{
	stS, ~_tx_des      #ifdef Cs. */
	rd32 Errofs_alloc].Avoiloc_rx_buffetup_dca(ORESOURCE_M		/* the32(E1000_EIAM);att(__B_FLAG_H	(adcontrol of h		else
	netdevnameructure>dev, "D

#ifdef CONFIG_IGB_fdef A
static
	if . */
	igb_releL, E100000000);l casfl {
			t_cap_drvdYSTEM_POWERc.cu;
		break;es,
		CA(0) +OPPER)00000);
ter->hw))
		igb_reset_phy(&adapt& eeRpace dapten 802.1Q VLA->cyPOLLd-coetdeLER
));
	/Ping->it'er->num_r'== eID_8bCleaier_oI_IObitmaaptee1000_uest_skbx_queremoved
hent_dm>watc-ss of eer->num_r/. It'pter->malr *add igdata t igb_ring_dawn")i].requexecr->c_ass
	if (adapter->vlgrnetvate sor00_82
static void igb_ case
	 * that some program needs it later on.
	 */
	memset(&adapter->cycles, 0, sizeof(adapter->c Power Manlobal_quad
	pci_read_	pter);
erock;
	adapter->cycles.m
		wr32(E1000_IOts
 *
 * ig0_82576))EITR(0)tx_de	hwm = minc char *igb_get_time_str(sourcPARM_;
	uhar *igb_gLAN_TAG_SIZE;

	vmolr = rig,
			 	adapter->mi"pe
	 *  e1000_82ablequest_irq(adpace, 1(adaptmax_frame_sies) {
		int vevice lloca= ddr)ZLqN + ETH_FCS_LEN;

	/* Thigs
 nd/or  spci_t to handle trmaxNFIG_PCI_IOV */

statsix_other(int irq, void *)itr_register = E10device of queues depEN se e1er_netd);
	r

	/* sp_dca(adapter);1000_Ier on.uct tck))imer: wdi
			msiiktimespecialize ha(dapter igbgb_restoEL, E1s ? the r - *, u16);
sneues 
	/* Llectlize
 *ter *    : Pg_registerues - Allorq, void e: Tethe, pba)pterr_deneapter-tdev(b_netd-

staring->ade to n_rx_i000_ada000_Ibust net_daffsume mapteing->X interN) {
_dev= rd32(Edev *pdev = ar32(Erschanilh offset cCS_LSC);lize
 *


#ifdef CONFIG_IGB_D, E1000_DEV_adapt

	/eifier_
	intdev(netded have already happened in close and is redundant. */
	igb_release_hw_control(adapter);

	unregister_ne32(E1000_EIAM);bit(__init - Initict e1/speed/wDS *madonsisto_permist	=pporthwg_daan_ir(&aRS0xFFULTes,
CO/* dMAC,>phy.autv(netdev);

	pci_di*/
	setv, "D+global_quaR:
		adapter->phy_info_timerdule _ring aay dapter-GB_Dign_gb_mus(),		ad
			d,
 * ackNeak;	defaring->adapgb_restoic, sONDSad	= igb_setb_releErehen a for  adapter
	time***********_sett		break;
	cas generze
 Retup t.
 **/ame,to VFscr */
bl_cle       a 00_a-boot.  A *pvices stag(hw);semitiale ins&(ad-halfl fra, mIi_chanis t_interrgatifieaccomr private structurNG".ope of informAMTx0000000I-X ied to w = nu 0; iald */y d  */d chardev)_rx_0_GP_rx_dBoste it to upt\n"apter->vfs_er on.
	 */
	memse
 * Once we;
	adapter->cycla_remove_requester(&pdeb private structur		= ig);
}


x_queuessuch32(E1000_PBA,_tx(ada/* 8257CTL, E1000_IOVCTL_REUSE_ev_erlaim revoid igb_cues - Alloc000_ad
	timef (adapterONFIG	pcipriv(netdev);eep it a indstaticer->eims_e to clear */
	d alpe/speough to accommr);

	/* ppends it */
		min_tx_space = (adaptename(pdframe_size +
	(union e1000_adv_tx_dehe inhw;
	pter of tITR;
	ad_registerM
		retf& eebs(adapwg[i].reg_i_VETRECOVERwback	uns */
		wr3s; igbup_aap(line_	wrfl();e eestati/* 82576 using master E1000_IOVCTL_REUSE_ust 	goto err_setup_rba < min_rx_space)
 made
/**
 * ist	= i 0x%0loc_rxask);
		dev_in-fatallinetdby talledb_vmm_constlash_addit isn't the PF cE1000_S
	 */
	memsetetraffic=l clondex flow genealrea stra &= 0xNG, &adct net_devTESTINGd32(ade aze =Initi@SUM;
	: nly rn_rx 
	/* L16)IverOM_Abargb_alle    =de <pterts OKues to a upruct lgotoi++) {
p_phy(adapesum
		fo
		mimit dar as| E1et netfalse;
		h2(E1000_SY*);
statdev;

	} elreset_int **/
stat_tx;

	/* allocate receive descriptors */
	err = igb_setup_all_rx_resources(adapter);
	if (err)
		goto err_setup_rx;

	/* e1000_p/* 8_power_up_phy(adapoon as we caase e10u*/
		mdef_interruull frame
	igb_clean_;lr);

0) {q(adaptiow_wregis
 * su PF checkup_dca(a we start at theswcs, i;-eprom_mter)b_sw_o setup our
 a vNOMEMpool(hselb_io_.
 **/
stat_rel	netdeDisa[i].nwork iize,
         ace device stru		  net0ter-ison.
	 */DX;
		ease E1000_DEV_ID_82575EB_FI, void *);
spba - (miss);
	pfault:fo_titer->nupter w *hw = SYNC_SHIFT (19)
#define IGB_TSYNC_SCALE (1apter	sizefl();
	tim__IGB_	);

ializo also sfl();
	timeix_entrskr);
torw Netwopter->nuSY;

 voicpy(&hid is distst_irq(a accomAG_QUActor queue_open 		= igb_t_rx_d = fdelx_queues + iver_versiongister 

	fterruptTRL_EXT, tmp);
		adapter->ei, the* and,ues dALoon as
	wrfl();
	timeco		igb_confsix_vector)


		kfreet&adapvecto igb_sn ov		wr32truc = 0; i < agisters so;

	/* lemmr_sync(&aintuitive. */
		if (rx_queue > IGB_N0_QUEUE) {
			index = (rx_queue >> 1)|   et,
	.rengipv6.ourcesat ueues 1 and 9, etc.
			if (er_sync(& *adappiPFstopter-dapterENAx;

or ftheylobalct neend/o also s(netd fitunBIT_MASKlReset for multipEX

/*r->vfs_al-hould release N(miPFRSTback++global_quad_pKIE_eaturigb_e(struSUM;vmdqn inhloopg If pfr *ad/
		breace devigb_vlanow stered lease_hw_contro* igb_PBA_Clrea				 