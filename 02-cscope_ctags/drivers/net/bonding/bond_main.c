/*
 * originally based on the dummy device.
 *
 * Copyright 1999, Thomas Davis, tadavis@lbl.gov.
 * Licensed under the GPL. Based on dummy.c, and eql.c devices.
 *
 * bonding.c: an Ethernet Bonding driver
 *
 * This is useful to talk to a Cisco EtherChannel compatible equipment:
 *	Cisco 5500
 *	Sun Trunking (Solaris)
 *	Alteon AceDirector Trunks
 *	Linux Bonding
 *	and probably many L2 switches ...
 *
 * How it works:
 *    ifconfig bond0 ipaddress netmask up
 *      will setup a network device, with an ip address.  No mac address
 *	will be assigned at this time.  The hw mac address will come from
 *	the first slave bonded to the channel.  All slaves will then use
 *	this hw mac address.
 *
 *    ifconfig bond0 down
 *         will release all slaves, marking them as down.
 *
 *    ifenslave bond0 eth0
 *	will attach eth0 to bond0 as a slave.  eth0 hw mac address will either
 *	a: be used as initial mac address
 *	b: if a hw mac address already is there, eth0's hw mac address
 *	   will then be set from bond0.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <net/ip.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/socket.h>
#include <linux/ctype.h>
#include <linux/inet.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <asm/system.h>
#include <asm/dma.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/igmp.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <linux/rtnetlink.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/smp.h>
#include <linux/if_ether.h>
#include <net/arp.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/if_bonding.h>
#include <linux/jiffies.h>
#include <net/route.h>
#include <net/net_namespace.h>
#include "bonding.h"
#include "bond_3ad.h"
#include "bond_alb.h"

/*---------------------------- Module parameters ----------------------------*/

/* monitor all links that often (in milliseconds). <=0 disables monitoring */
#define BOND_LINK_MON_INTERV	0
#define BOND_LINK_ARP_INTERV	0

static int max_bonds	= BOND_DEFAULT_MAX_BONDS;
static int num_grat_arp = 1;
static int num_unsol_na = 1;
static int miimon	= BOND_LINK_MON_INTERV;
static int updelay;
static int downdelay;
static int use_carrier	= 1;
static char *mode;
static char *primary;
static char *lacp_rate;
static char *ad_select;
static char *xmit_hash_policy;
static int arp_interval = BOND_LINK_ARP_INTERV;
static char *arp_ip_target[BOND_MAX_ARP_TARGETS];
static char *arp_validate;
static char *fail_over_mac;
static struct bond_params bonding_defaults;

module_param(max_bonds, int, 0);
MODULE_PARM_DESC(max_bonds, "Max number of bonded devices");
module_param(num_grat_arp, int, 0644);
MODULE_PARM_DESC(num_grat_arp, "Number of gratuitous ARP packets to send on failover event");
module_param(num_unsol_na, int, 0644);
MODULE_PARM_DESC(num_unsol_na, "Number of unsolicited IPv6 Neighbor Advertisements packets to send on failover event");
module_param(miimon, int, 0);
MODULE_PARM_DESC(miimon, "Link check interval in milliseconds");
module_param(updelay, int, 0);
MODULE_PARM_DESC(updelay, "Delay before considering link up, in milliseconds");
module_param(downdelay, int, 0);
MODULE_PARM_DESC(downdelay, "Delay before considering link down, "
			    "in milliseconds");
module_param(use_carrier, int, 0);
MODULE_PARM_DESC(use_carrier, "Use netif_carrier_ok (vs MII ioctls) in miimon; "
			      "0 for off, 1 for on (default)");
module_param(mode, charp, 0);
MODULE_PARM_DESC(mode, "Mode of operation : 0 for balance-rr, "
		       "1 for active-backup, 2 for balance-xor, "
		       "3 for broadcast, 4 for 802.3ad, 5 for balance-tlb, "
		       "6 for balance-alb");
module_param(primary, charp, 0);
MODULE_PARM_DESC(primary, "Primary network device to use");
module_param(lacp_rate, charp, 0);
MODULE_PARM_DESC(lacp_rate, "LACPDU tx rate to request from 802.3ad partner "
			    "(slow/fast)");
module_param(ad_select, charp, 0);
MODULE_PARM_DESC(ad_select, "803.ad aggregation selection logic: stable (0, default), bandwidth (1), count (2)");
module_param(xmit_hash_policy, charp, 0);
MODULE_PARM_DESC(xmit_hash_policy, "XOR hashing method: 0 for layer 2 (default)"
				   ", 1 for layer 3+4");
module_param(arp_interval, int, 0);
MODULE_PARM_DESC(arp_interval, "arp interval in milliseconds");
module_param_array(arp_ip_target, charp, NULL, 0);
MODULE_PARM_DESC(arp_ip_target, "arp targets in n.n.n.n form");
module_param(arp_validate, charp, 0);
MODULE_PARM_DESC(arp_validate, "validate src/dst of ARP probes: none (default), active, backup or all");
module_param(fail_over_mac, charp, 0);
MODULE_PARM_DESC(fail_over_mac, "For active-backup, do not set all slaves to the same MAC.  none (default), active or follow");

/*----------------------------- Global variables ----------------------------*/

static const char * const version =
	DRV_DESCRIPTION ": v" DRV_VERSION " (" DRV_RELDATE ")\n";

LIST_HEAD(bond_dev_list);

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *bond_proc_dir;
#endif

static __be32 arp_target[BOND_MAX_ARP_TARGETS];
static int arp_ip_count;
static int bond_mode	= BOND_MODE_ROUNDROBIN;
static int xmit_hashtype = BOND_XMIT_POLICY_LAYER2;
static int lacp_fast;


const struct bond_parm_tbl bond_lacp_tbl[] = {
{	"slow",		AD_LACP_SLOW},
{	"fast",		AD_LACP_FAST},
{	NULL,		-1},
};

const struct bond_parm_tbl bond_mode_tbl[] = {
{	"balance-rr",		BOND_MODE_ROUNDROBIN},
{	"active-backup",	BOND_MODE_ACTIVEBACKUP},
{	"balance-xor",		BOND_MODE_XOR},
{	"broadcast",		BOND_MODE_BROADCAST},
{	"802.3ad",		BOND_MODE_8023AD},
{	"balance-tlb",		BOND_MODE_TLB},
{	"balance-alb",		BOND_MODE_ALB},
{	NULL,			-1},
};

const struct bond_parm_tbl xmit_hashtype_tbl[] = {
{	"layer2",		BOND_XMIT_POLICY_LAYER2},
{	"layer3+4",		BOND_XMIT_POLICY_LAYER34},
{	"layer2+3",		BOND_XMIT_POLICY_LAYER23},
{	NULL,			-1},
};

const struct bond_parm_tbl arp_validate_tbl[] = {
{	"none",			BOND_ARP_VALIDATE_NONE},
{	"active",		BOND_ARP_VALIDATE_ACTIVE},
{	"backup",		BOND_ARP_VALIDATE_BACKUP},
{	"all",			BOND_ARP_VALIDATE_ALL},
{	NULL,			-1},
};

const struct bond_parm_tbl fail_over_mac_tbl[] = {
{	"none",			BOND_FOM_NONE},
{	"active",		BOND_FOM_ACTIVE},
{	"follow",		BOND_FOM_FOLLOW},
{	NULL,			-1},
};

struct bond_parm_tbl ad_select_tbl[] = {
{	"stable",	BOND_AD_STABLE},
{	"bandwidth",	BOND_AD_BANDWIDTH},
{	"count",	BOND_AD_COUNT},
{	NULL,		-1},
};

/*-------------------------- Forward declarations ---------------------------*/

static void bond_send_gratuitous_arp(struct bonding *bond);
static int bond_init(struct net_device *bond_dev);
static void bond_deinit(struct net_device *bond_dev);

/*---------------------------- General routines -----------------------------*/

static const char *bond_mode_name(int mode)
{
	static const char *names[] = {
		[BOND_MODE_ROUNDROBIN] = "load balancing (round-robin)",
		[BOND_MODE_ACTIVEBACKUP] = "fault-tolerance (active-backup)",
		[BOND_MODE_XOR] = "load balancing (xor)",
		[BOND_MODE_BROADCAST] = "fault-tolerance (broadcast)",
		[BOND_MODE_8023AD] = "IEEE 802.3ad Dynamic link aggregation",
		[BOND_MODE_TLB] = "transmit load balancing",
		[BOND_MODE_ALB] = "adaptive load balancing",
	};

	if (mode < 0 || mode > BOND_MODE_ALB)
		return "unknown";

	return names[mode];
}

/*---------------------------------- VLAN -----------------------------------*/

/**
 * bond_add_vlan - add a new vlan id on bond
 * @bond: bond that got the notification
 * @vlan_id: the vlan id to add
 *
 * Returns -ENOMEM if allocation failed.
 */
static int bond_add_vlan(struct bonding *bond, unsigned short vlan_id)
{
	struct vlan_entry *vlan;

	pr_debug("bond: %s, vlan id %d\n",
		(bond ? bond->dev->name : "None"), vlan_id);

	vlan = kzalloc(sizeof(struct vlan_entry), GFP_KERNEL);
	if (!vlan)
		return -ENOMEM;

	INIT_LIST_HEAD(&vlan->vlan_list);
	vlan->vlan_id = vlan_id;

	write_lock_bh(&bond->lock);

	list_add_tail(&vlan->vlan_list, &bond->vlan_list);

	write_unlock_bh(&bond->lock);

	pr_debug("added VLAN ID %d on bond %s\n", vlan_id, bond->dev->name);

	return 0;
}

/**
 * bond_del_vlan - delete a vlan id from bond
 * @bond: bond that got the notification
 * @vlan_id: the vlan id to delete
 *
 * returns -ENODEV if @vlan_id was not found in @bond.
 */
static int bond_del_vlan(struct bonding *bond, unsigned short vlan_id)
{
	struct vlan_entry *vlan;
	int res = -ENODEV;

	pr_debug("bond: %s, vlan id %d\n", bond->dev->name, vlan_id);

	write_lock_bh(&bond->lock);

	list_for_each_entry(vlan, &bond->vlan_list, vlan_list) {
		if (vlan->vlan_id == vlan_id) {
			list_del(&vlan->vlan_list);

			if (bond_is_lb(bond))
				bond_alb_clear_vlan(bond, vlan_id);

			pr_debug("removed VLAN ID %d from bond %s\n", vlan_id,
				bond->dev->name);

			kfree(vlan);

			if (list_empty(&bond->vlan_list) &&
			    (bond->slave_cnt == 0)) {
				/* Last VLAN removed and no slaves, so
				 * restore block on adding VLANs. This will
				 * be removed once new slaves that are not
				 * VLAN challenged will be added.
				 */
				bond->dev->features |= NETIF_F_VLAN_CHALLENGED;
			}

			res = 0;
			goto out;
		}
	}

	pr_debug("couldn't find VLAN ID %d in bond %s\n", vlan_id,
		bond->dev->name);

out:
	write_unlock_bh(&bond->lock);
	return res;
}

/**
 * bond_has_challenged_slaves
 * @bond: the bond we're working on
 *
 * Searches the slave list. Returns 1 if a vlan challenged slave
 * was found, 0 otherwise.
 *
 * Assumes bond->lock is held.
 */
static int bond_has_challenged_slaves(struct bonding *bond)
{
	struct slave *slave;
	int i;

	bond_for_each_slave(bond, slave, i) {
		if (slave->dev->features & NETIF_F_VLAN_CHALLENGED) {
			pr_debug("found VLAN challenged slave - %s\n",
				slave->dev->name);
			return 1;
		}
	}

	pr_debug("no VLAN challenged slaves found\n");
	return 0;
}

/**
 * bond_next_vlan - safely skip to the next item in the vlans list.
 * @bond: the bond we're working on
 * @curr: item we're advancing from
 *
 * Returns %NULL if list is empty, bond->next_vlan if @curr is %NULL,
 * or @curr->next otherwise (even if it is @curr itself again).
 *
 * Caller must hold bond->lock
 */
struct vlan_entry *bond_next_vlan(struct bonding *bond, struct vlan_entry *curr)
{
	struct vlan_entry *next, *last;

	if (list_empty(&bond->vlan_list))
		return NULL;

	if (!curr) {
		next = list_entry(bond->vlan_list.next,
				  struct vlan_entry, vlan_list);
	} else {
		last = list_entry(bond->vlan_list.prev,
				  struct vlan_entry, vlan_list);
		if (last == curr) {
			next = list_entry(bond->vlan_list.next,
					  struct vlan_entry, vlan_list);
		} else {
			next = list_entry(curr->vlan_list.next,
					  struct vlan_entry, vlan_list);
		}
	}

	return next;
}

/**
 * bond_dev_queue_xmit - Prepare skb for xmit.
 *
 * @bond: bond device that got this skb for tx.
 * @skb: hw accel VLAN tagged skb to transmit
 * @slave_dev: slave that is supposed to xmit this skbuff
 *
 * When the bond gets an skb to transmit that is
 * already hardware accelerated VLAN tagged, and it
 * needs to relay this skb to a slave that is not
 * hw accel capable, the skb needs to be "unaccelerated",
 * i.e. strip the hwaccel tag and re-insert it as part
 * of the payload.
 */
int bond_dev_queue_xmit(struct bonding *bond, struct sk_buff *skb,
			struct net_device *slave_dev)
{
	unsigned short uninitialized_var(vlan_id);

	if (!list_empty(&bond->vlan_list) &&
	    !(slave_dev->features & NETIF_F_HW_VLAN_TX) &&
	    vlan_get_tag(skb, &vlan_id) == 0) {
		skb->dev = slave_dev;
		skb = vlan_put_tag(skb, vlan_id);
		if (!skb) {
			/* vlan_put_tag() frees the skb in case of error,
			 * so return success here so the calling functions
			 * won't attempt to free is again.
			 */
			return 0;
		}
	} else {
		skb->dev = slave_dev;
	}

	skb->priority = 1;
	dev_queue_xmit(skb);

	return 0;
}

/*
 * In the following 3 functions, bond_vlan_rx_register(), bond_vlan_rx_add_vid
 * and bond_vlan_rx_kill_vid, We don't protect the slave list iteration with a
 * lock because:
 * a. This operation is performed in IOCTL context,
 * b. The operation is protected by the RTNL semaphore in the 8021q code,
 * c. Holding a lock with BH disabled while directly calling a base driver
 *    entry point is generally a BAD idea.
 *
 * The design of synchronization/protection for this operation in the 8021q
 * module is good for one or more VLAN devices over a single physical device
 * and cannot be extended for a teaming solution like bonding, so there is a
 * potential race condition here where a net device from the vlan group might
 * be referenced (either by a base driver or the 8021q code) while it is being
 * removed from the system. However, it turns out we're not making matters
 * worse, and if it works for regular VLAN usage it will work here too.
*/

/**
 * bond_vlan_rx_register - Propagates registration to slaves
 * @bond_dev: bonding net device that got called
 * @grp: vlan group being registered
 */
static void bond_vlan_rx_register(struct net_device *bond_dev,
				  struct vlan_group *grp)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave;
	int i;

	bond->vlgrp = grp;

	bond_for_each_slave(bond, slave, i) {
		struct net_device *slave_dev = slave->dev;
		const struct net_device_ops *slave_ops = slave_dev->netdev_ops;

		if ((slave_dev->features & NETIF_F_HW_VLAN_RX) &&
		    slave_ops->ndo_vlan_rx_register) {
			slave_ops->ndo_vlan_rx_register(slave_dev, grp);
		}
	}
}

/**
 * bond_vlan_rx_add_vid - Propagates adding an id to slaves
 * @bond_dev: bonding net device that got called
 * @vid: vlan id being added
 */
static void bond_vlan_rx_add_vid(struct net_device *bond_dev, uint16_t vid)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave;
	int i, res;

	bond_for_each_slave(bond, slave, i) {
		struct net_device *slave_dev = slave->dev;
		const struct net_device_ops *slave_ops = slave_dev->netdev_ops;

		if ((slave_dev->features & NETIF_F_HW_VLAN_FILTER) &&
		    slave_ops->ndo_vlan_rx_add_vid) {
			slave_ops->ndo_vlan_rx_add_vid(slave_dev, vid);
		}
	}

	res = bond_add_vlan(bond, vid);
	if (res) {
		pr_err(DRV_NAME
		       ": %s: Error: Failed to add vlan id %d\n",
		       bond_dev->name, vid);
	}
}

/**
 * bond_vlan_rx_kill_vid - Propagates deleting an id to slaves
 * @bond_dev: bonding net device that got called
 * @vid: vlan id being removed
 */
static void bond_vlan_rx_kill_vid(struct net_device *bond_dev, uint16_t vid)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave;
	struct net_device *vlan_dev;
	int i, res;

	bond_for_each_slave(bond, slave, i) {
		struct net_device *slave_dev = slave->dev;
		const struct net_device_ops *slave_ops = slave_dev->netdev_ops;

		if ((slave_dev->features & NETIF_F_HW_VLAN_FILTER) &&
		    slave_ops->ndo_vlan_rx_kill_vid) {
			/* Save and then restore vlan_dev in the grp array,
			 * since the slave's driver might clear it.
			 */
			vlan_dev = vlan_group_get_device(bond->vlgrp, vid);
			slave_ops->ndo_vlan_rx_kill_vid(slave_dev, vid);
			vlan_group_set_device(bond->vlgrp, vid, vlan_dev);
		}
	}

	res = bond_del_vlan(bond, vid);
	if (res) {
		pr_err(DRV_NAME
		       ": %s: Error: Failed to remove vlan id %d\n",
		       bond_dev->name, vid);
	}
}

static void bond_add_vlans_on_slave(struct bonding *bond, struct net_device *slave_dev)
{
	struct vlan_entry *vlan;
	const struct net_device_ops *slave_ops = slave_dev->netdev_ops;

	write_lock_bh(&bond->lock);

	if (list_empty(&bond->vlan_list))
		goto out;

	if ((slave_dev->features & NETIF_F_HW_VLAN_RX) &&
	    slave_ops->ndo_vlan_rx_register)
		slave_ops->ndo_vlan_rx_register(slave_dev, bond->vlgrp);

	if (!(slave_dev->features & NETIF_F_HW_VLAN_FILTER) ||
	    !(slave_ops->ndo_vlan_rx_add_vid))
		goto out;

	list_for_each_entry(vlan, &bond->vlan_list, vlan_list)
		slave_ops->ndo_vlan_rx_add_vid(slave_dev, vlan->vlan_id);

out:
	write_unlock_bh(&bond->lock);
}

static void bond_del_vlans_from_slave(struct bonding *bond,
				      struct net_device *slave_dev)
{
	const struct net_device_ops *slave_ops = slave_dev->netdev_ops;
	struct vlan_entry *vlan;
	struct net_device *vlan_dev;

	write_lock_bh(&bond->lock);

	if (list_empty(&bond->vlan_list))
		goto out;

	if (!(slave_dev->features & NETIF_F_HW_VLAN_FILTER) ||
	    !(slave_ops->ndo_vlan_rx_kill_vid))
		goto unreg;

	list_for_each_entry(vlan, &bond->vlan_list, vlan_list) {
		/* Save and then restore vlan_dev in the grp array,
		 * since the slave's driver might clear it.
		 */
		vlan_dev = vlan_group_get_device(bond->vlgrp, vlan->vlan_id);
		slave_ops->ndo_vlan_rx_kill_vid(slave_dev, vlan->vlan_id);
		vlan_group_set_device(bond->vlgrp, vlan->vlan_id, vlan_dev);
	}

unreg:
	if ((slave_dev->features & NETIF_F_HW_VLAN_RX) &&
	    slave_ops->ndo_vlan_rx_register)
		slave_ops->ndo_vlan_rx_register(slave_dev, NULL);

out:
	write_unlock_bh(&bond->lock);
}

/*------------------------------- Link status -------------------------------*/

/*
 * Set the carrier state for the master according to the state of its
 * slaves.  If any slaves are up, the master is up.  In 802.3ad mode,
 * do special 802.3ad magic.
 *
 * Returns zero if carrier state does not change, nonzero if it does.
 */
static int bond_set_carrier(struct bonding *bond)
{
	struct slave *slave;
	int i;

	if (bond->slave_cnt == 0)
		goto down;

	if (bond->params.mode == BOND_MODE_8023AD)
		return bond_3ad_set_carrier(bond);

	bond_for_each_slave(bond, slave, i) {
		if (slave->link == BOND_LINK_UP) {
			if (!netif_carrier_ok(bond->dev)) {
				netif_carrier_on(bond->dev);
				return 1;
			}
			return 0;
		}
	}

down:
	if (netif_carrier_ok(bond->dev)) {
		netif_carrier_off(bond->dev);
		return 1;
	}
	return 0;
}

/*
 * Get link speed and duplex from the slave's base driver
 * using ethtool. If for some reason the call fails or the
 * values are invalid, fake speed and duplex to 100/Full
 * and return error.
 */
static int bond_update_speed_duplex(struct slave *slave)
{
	struct net_device *slave_dev = slave->dev;
	struct ethtool_cmd etool;
	int res;

	/* Fake speed and duplex */
	slave->speed = SPEED_100;
	slave->duplex = DUPLEX_FULL;

	if (!slave_dev->ethtool_ops || !slave_dev->ethtool_ops->get_settings)
		return -1;

	res = slave_dev->ethtool_ops->get_settings(slave_dev, &etool);
	if (res < 0)
		return -1;

	switch (etool.speed) {
	case SPEED_10:
	case SPEED_100:
	case SPEED_1000:
	case SPEED_10000:
		break;
	default:
		return -1;
	}

	switch (etool.duplex) {
	case DUPLEX_FULL:
	case DUPLEX_HALF:
		break;
	default:
		return -1;
	}

	slave->speed = etool.speed;
	slave->duplex = etool.duplex;

	return 0;
}

/*
 * if <dev> supports MII link status reporting, check its link status.
 *
 * We either do MII/ETHTOOL ioctls, or check netif_carrier_ok(),
 * depending upon the setting of the use_carrier parameter.
 *
 * Return either BMSR_LSTATUS, meaning that the link is up (or we
 * can't tell and just pretend it is), or 0, meaning that the link is
 * down.
 *
 * If reporting is non-zero, instead of faking link up, return -1 if
 * both ETHTOOL and MII ioctls fail (meaning the device does not
 * support them).  If use_carrier is set, return whatever it says.
 * It'd be nice if there was a good way to tell if a driver supports
 * netif_carrier, but there really isn't.
 */
static int bond_check_dev_link(struct bonding *bond,
			       struct net_device *slave_dev, int reporting)
{
	const struct net_device_ops *slave_ops = slave_dev->netdev_ops;
	int (*ioctl)(struct net_device *, struct ifreq *, int);
	struct ifreq ifr;
	struct mii_ioctl_data *mii;

	if (!reporting && !netif_running(slave_dev))
		return 0;

	if (bond->params.use_carrier)
		return netif_carrier_ok(slave_dev) ? BMSR_LSTATUS : 0;

	/* Try to get link status using Ethtool first. */
	if (slave_dev->ethtool_ops) {
		if (slave_dev->ethtool_ops->get_link) {
			u32 link;

			link = slave_dev->ethtool_ops->get_link(slave_dev);

			return link ? BMSR_LSTATUS : 0;
		}
	}

	/* Ethtool can't be used, fallback to MII ioctls. */
	ioctl = slave_ops->ndo_do_ioctl;
	if (ioctl) {
		/* TODO: set pointer to correct ioctl on a per team member */
		/*       bases to make this more efficient. that is, once  */
		/*       we determine the correct ioctl, we will always    */
		/*       call it and not the others for that team          */
		/*       member.                                           */

		/*
		 * We cannot assume that SIOCGMIIPHY will also read a
		 * register; not all network drivers (e.g., e100)
		 * support that.
		 */

		/* Yes, the mii is overlaid on the ifreq.ifr_ifru */
		strncpy(ifr.ifr_name, slave_dev->name, IFNAMSIZ);
		mii = if_mii(&ifr);
		if (IOCTL(slave_dev, &ifr, SIOCGMIIPHY) == 0) {
			mii->reg_num = MII_BMSR;
			if (IOCTL(slave_dev, &ifr, SIOCGMIIREG) == 0)
				return mii->val_out & BMSR_LSTATUS;
		}
	}

	/*
	 * If reporting, report that either there's no dev->do_ioctl,
	 * or both SIOCGMIIREG and get_link failed (meaning that we
	 * cannot report link status).  If not reporting, pretend
	 * we're ok.
	 */
	return reporting ? -1 : BMSR_LSTATUS;
}

/*----------------------------- Multicast list ------------------------------*/

/*
 * Returns 0 if dmi1 and dmi2 are the same, non-0 otherwise
 */
static inline int bond_is_dmi_same(const struct dev_mc_list *dmi1,
				   const struct dev_mc_list *dmi2)
{
	return memcmp(dmi1->dmi_addr, dmi2->dmi_addr, dmi1->dmi_addrlen) == 0 &&
			dmi1->dmi_addrlen == dmi2->dmi_addrlen;
}

/*
 * returns dmi entry if found, NULL otherwise
 */
static struct dev_mc_list *bond_mc_list_find_dmi(struct dev_mc_list *dmi,
						 struct dev_mc_list *mc_list)
{
	struct dev_mc_list *idmi;

	for (idmi = mc_list; idmi; idmi = idmi->next) {
		if (bond_is_dmi_same(dmi, idmi))
			return idmi;
	}

	return NULL;
}

/*
 * Push the promiscuity flag down to appropriate slaves
 */
static int bond_set_promiscuity(struct bonding *bond, int inc)
{
	int err = 0;
	if (USES_PRIMARY(bond->params.mode)) {
		/* write lock already acquired */
		if (bond->curr_active_slave) {
			err = dev_set_promiscuity(bond->curr_active_slave->dev,
						  inc);
		}
	} else {
		struct slave *slave;
		int i;
		bond_for_each_slave(bond, slave, i) {
			err = dev_set_promiscuity(slave->dev, inc);
			if (err)
				return err;
		}
	}
	return err;
}

/*
 * Push the allmulti flag down to all slaves
 */
static int bond_set_allmulti(struct bonding *bond, int inc)
{
	int err = 0;
	if (USES_PRIMARY(bond->params.mode)) {
		/* write lock already acquired */
		if (bond->curr_active_slave) {
			err = dev_set_allmulti(bond->curr_active_slave->dev,
					       inc);
		}
	} else {
		struct slave *slave;
		int i;
		bond_for_each_slave(bond, slave, i) {
			err = dev_set_allmulti(slave->dev, inc);
			if (err)
				return err;
		}
	}
	return err;
}

/*
 * Add a Multicast address to slaves
 * according to mode
 */
static void bond_mc_add(struct bonding *bond, void *addr, int alen)
{
	if (USES_PRIMARY(bond->params.mode)) {
		/* write lock already acquired */
		if (bond->curr_active_slave)
			dev_mc_add(bond->curr_active_slave->dev, addr, alen, 0);
	} else {
		struct slave *slave;
		int i;

		bond_for_each_slave(bond, slave, i)
			dev_mc_add(slave->dev, addr, alen, 0);
	}
}

/*
 * Remove a multicast address from slave
 * according to mode
 */
static void bond_mc_delete(struct bonding *bond, void *addr, int alen)
{
	if (USES_PRIMARY(bond->params.mode)) {
		/* write lock already acquired */
		if (bond->curr_active_slave)
			dev_mc_delete(bond->curr_active_slave->dev, addr,
				      alen, 0);
	} else {
		struct slave *slave;
		int i;
		bond_for_each_slave(bond, slave, i) {
			dev_mc_delete(slave->dev, addr, alen, 0);
		}
	}
}


/*
 * Retrieve the list of registered multicast addresses for the bonding
 * device and retransmit an IGMP JOIN request to the current active
 * slave.
 */
static void bond_resend_igmp_join_requests(struct bonding *bond)
{
	struct in_device *in_dev;
	struct ip_mc_list *im;

	rcu_read_lock();
	in_dev = __in_dev_get_rcu(bond->dev);
	if (in_dev) {
		for (im = in_dev->mc_list; im; im = im->next)
			ip_mc_rejoin_group(im);
	}

	rcu_read_unlock();
}

/*
 * Totally destroys the mc_list in bond
 */
static void bond_mc_list_destroy(struct bonding *bond)
{
	struct dev_mc_list *dmi;

	dmi = bond->mc_list;
	while (dmi) {
		bond->mc_list = dmi->next;
		kfree(dmi);
		dmi = bond->mc_list;
	}

	bond->mc_list = NULL;
}

/*
 * Copy all the Multicast addresses from src to the bonding device dst
 */
static int bond_mc_list_copy(struct dev_mc_list *mc_list, struct bonding *bond,
			     gfp_t gfp_flag)
{
	struct dev_mc_list *dmi, *new_dmi;

	for (dmi = mc_list; dmi; dmi = dmi->next) {
		new_dmi = kmalloc(sizeof(struct dev_mc_list), gfp_flag);

		if (!new_dmi) {
			/* FIXME: Potential memory leak !!! */
			return -ENOMEM;
		}

		new_dmi->next = bond->mc_list;
		bond->mc_list = new_dmi;
		new_dmi->dmi_addrlen = dmi->dmi_addrlen;
		memcpy(new_dmi->dmi_addr, dmi->dmi_addr, dmi->dmi_addrlen);
		new_dmi->dmi_users = dmi->dmi_users;
		new_dmi->dmi_gusers = dmi->dmi_gusers;
	}

	return 0;
}

/*
 * flush all members of flush->mc_list from device dev->mc_list
 */
static void bond_mc_list_flush(struct net_device *bond_dev,
			       struct net_device *slave_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct dev_mc_list *dmi;

	for (dmi = bond_dev->mc_list; dmi; dmi = dmi->next)
		dev_mc_delete(slave_dev, dmi->dmi_addr, dmi->dmi_addrlen, 0);

	if (bond->params.mode == BOND_MODE_8023AD) {
		/* del lacpdu mc addr from mc list */
		u8 lacpdu_multicast[ETH_ALEN] = MULTICAST_LACPDU_ADDR;

		dev_mc_delete(slave_dev, lacpdu_multicast, ETH_ALEN, 0);
	}
}

/*--------------------------- Active slave change ---------------------------*/

/*
 * Update the mc list and multicast-related flags for the new and
 * old active slaves (if any) according to the multicast mode, and
 * promiscuous flags unconditionally.
 */
static void bond_mc_swap(struct bonding *bond, struct slave *new_active,
			 struct slave *old_active)
{
	struct dev_mc_list *dmi;

	if (!USES_PRIMARY(bond->params.mode))
		/* nothing to do -  mc list is already up-to-date on
		 * all slaves
		 */
		return;

	if (old_active) {
		if (bond->dev->flags & IFF_PROMISC)
			dev_set_promiscuity(old_active->dev, -1);

		if (bond->dev->flags & IFF_ALLMULTI)
			dev_set_allmulti(old_active->dev, -1);

		for (dmi = bond->dev->mc_list; dmi; dmi = dmi->next)
			dev_mc_delete(old_active->dev, dmi->dmi_addr,
				      dmi->dmi_addrlen, 0);
	}

	if (new_active) {
		/* FIXME: Signal errors upstream. */
		if (bond->dev->flags & IFF_PROMISC)
			dev_set_promiscuity(new_active->dev, 1);

		if (bond->dev->flags & IFF_ALLMULTI)
			dev_set_allmulti(new_active->dev, 1);

		for (dmi = bond->dev->mc_list; dmi; dmi = dmi->next)
			dev_mc_add(new_active->dev, dmi->dmi_addr,
				   dmi->dmi_addrlen, 0);
		bond_resend_igmp_join_requests(bond);
	}
}

/*
 * bond_do_fail_over_mac
 *
 * Perform special MAC address swapping for fail_over_mac settings
 *
 * Called with RTNL, bond->lock for read, curr_slave_lock for write_bh.
 */
static void bond_do_fail_over_mac(struct bonding *bond,
				  struct slave *new_active,
				  struct slave *old_active)
	__releases(&bond->curr_slave_lock)
	__releases(&bond->lock)
	__acquires(&bond->lock)
	__acquires(&bond->curr_slave_lock)
{
	u8 tmp_mac[ETH_ALEN];
	struct sockaddr saddr;
	int rv;

	switch (bond->params.fail_over_mac) {
	case BOND_FOM_ACTIVE:
		if (new_active)
			memcpy(bond->dev->dev_addr,  new_active->dev->dev_addr,
			       new_active->dev->addr_len);
		break;
	case BOND_FOM_FOLLOW:
		/*
		 * if new_active && old_active, swap them
		 * if just old_active, do nothing (going to no active slave)
		 * if just new_active, set new_active to bond's MAC
		 */
		if (!new_active)
			return;

		write_unlock_bh(&bond->curr_slave_lock);
		read_unlock(&bond->lock);

		if (old_active) {
			memcpy(tmp_mac, new_active->dev->dev_addr, ETH_ALEN);
			memcpy(saddr.sa_data, old_active->dev->dev_addr,
			       ETH_ALEN);
			saddr.sa_family = new_active->dev->type;
		} else {
			memcpy(saddr.sa_data, bond->dev->dev_addr, ETH_ALEN);
			saddr.sa_family = bond->dev->type;
		}

		rv = dev_set_mac_address(new_active->dev, &saddr);
		if (rv) {
			pr_err(DRV_NAME
			       ": %s: Error %d setting MAC of slave %s\n",
			       bond->dev->name, -rv, new_active->dev->name);
			goto out;
		}

		if (!old_active)
			goto out;

		memcpy(saddr.sa_data, tmp_mac, ETH_ALEN);
		saddr.sa_family = old_active->dev->type;

		rv = dev_set_mac_address(old_active->dev, &saddr);
		if (rv)
			pr_err(DRV_NAME
			       ": %s: Error %d setting MAC of slave %s\n",
			       bond->dev->name, -rv, new_active->dev->name);
out:
		read_lock(&bond->lock);
		write_lock_bh(&bond->curr_slave_lock);
		break;
	default:
		pr_err(DRV_NAME
		       ": %s: bond_do_fail_over_mac impossible: bad policy %d\n",
		       bond->dev->name, bond->params.fail_over_mac);
		break;
	}

}


/**
 * find_best_interface - select the best available slave to be the active one
 * @bond: our bonding struct
 *
 * Warning: Caller must hold curr_slave_lock for writing.
 */
static struct slave *bond_find_best_slave(struct bonding *bond)
{
	struct slave *new_active, *old_active;
	struct slave *bestslave = NULL;
	int mintime = bond->params.updelay;
	int i;

	new_active = old_active = bond->curr_active_slave;

	if (!new_active) { /* there were no active slaves left */
		if (bond->slave_cnt > 0)   /* found one slave */
			new_active = bond->first_slave;
		else
			return NULL; /* still no slave, return NULL */
	}

	if ((bond->primary_slave) &&
	    bond->primary_slave->link == BOND_LINK_UP) {
		new_active = bond->primary_slave;
	}

	/* remember where to stop iterating over the slaves */
	old_active = new_active;

	bond_for_each_slave_from(bond, new_active, i, old_active) {
		if (new_active->link == BOND_LINK_UP) {
			return new_active;
		} else if (new_active->link == BOND_LINK_BACK &&
			   IS_UP(new_active->dev)) {
			/* link up, but waiting for stabilization */
			if (new_active->delay < mintime) {
				mintime = new_active->delay;
				bestslave = new_active;
			}
		}
	}

	return bestslave;
}

/**
 * change_active_interface - change the active slave into the specified one
 * @bond: our bonding struct
 * @new: the new slave to make the active one
 *
 * Set the new slave to the bond's settings and unset them on the old
 * curr_active_slave.
 * Setting include flags, mc-list, promiscuity, allmulti, etc.
 *
 * If @new's link state is %BOND_LINK_BACK we'll set it to %BOND_LINK_UP,
 * because it is apparently the best available slave we have, even though its
 * updelay hasn't timed out yet.
 *
 * If new_active is not NULL, caller must hold bond->lock for read and
 * curr_slave_lock for write_bh.
 */
void bond_change_active_slave(struct bonding *bond, struct slave *new_active)
{
	struct slave *old_active = bond->curr_active_slave;

	if (old_active == new_active)
		return;

	if (new_active) {
		new_active->jiffies = jiffies;

		if (new_active->link == BOND_LINK_BACK) {
			if (USES_PRIMARY(bond->params.mode)) {
				pr_info(DRV_NAME
				       ": %s: making interface %s the new "
				       "active one %d ms earlier.\n",
				       bond->dev->name, new_active->dev->name,
				       (bond->params.updelay - new_active->delay) * bond->params.miimon);
			}

			new_active->delay = 0;
			new_active->link = BOND_LINK_UP;

			if (bond->params.mode == BOND_MODE_8023AD)
				bond_3ad_handle_link_change(new_active, BOND_LINK_UP);

			if (bond_is_lb(bond))
				bond_alb_handle_link_change(bond, new_active, BOND_LINK_UP);
		} else {
			if (USES_PRIMARY(bond->params.mode)) {
				pr_info(DRV_NAME
				       ": %s: making interface %s the new "
				       "active one.\n",
				       bond->dev->name, new_active->dev->name);
			}
		}
	}

	if (USES_PRIMARY(bond->params.mode))
		bond_mc_swap(bond, new_active, old_active);

	if (bond_is_lb(bond)) {
		bond_alb_handle_active_change(bond, new_active);
		if (old_active)
			bond_set_slave_inactive_flags(old_active);
		if (new_active)
			bond_set_slave_active_flags(new_active);
	} else {
		bond->curr_active_slave = new_active;
	}

	if (bond->params.mode == BOND_MODE_ACTIVEBACKUP) {
		if (old_active)
			bond_set_slave_inactive_flags(old_active);

		if (new_active) {
			bond_set_slave_active_flags(new_active);

			if (bond->params.fail_over_mac)
				bond_do_fail_over_mac(bond, new_active,
						      old_active);

			bond->send_grat_arp = bond->params.num_grat_arp;
			bond_send_gratuitous_arp(bond);

			bond->send_unsol_na = bond->params.num_unsol_na;
			bond_send_unsolicited_na(bond);

			write_unlock_bh(&bond->curr_slave_lock);
			read_unlock(&bond->lock);

			netdev_bonding_change(bond->dev, NETDEV_BONDING_FAILOVER);

			read_lock(&bond->lock);
			write_lock_bh(&bond->curr_slave_lock);
		}
	}
}

/**
 * bond_select_active_slave - select a new active slave, if needed
 * @bond: our bonding struct
 *
 * This functions should be called when one of the following occurs:
 * - The old curr_active_slave has been released or lost its link.
 * - The primary_slave has got its link back.
 * - A slave has got its link back and there's no old curr_active_slave.
 *
 * Caller must hold bond->lock for read and curr_slave_lock for write_bh.
 */
void bond_select_active_slave(struct bonding *bond)
{
	struct slave *best_slave;
	int rv;

	best_slave = bond_find_best_slave(bond);
	if (best_slave != bond->curr_active_slave) {
		bond_change_active_slave(bond, best_slave);
		rv = bond_set_carrier(bond);
		if (!rv)
			return;

		if (netif_carrier_ok(bond->dev)) {
			pr_info(DRV_NAME
			       ": %s: first active interface up!\n",
			       bond->dev->name);
		} else {
			pr_info(DRV_NAME ": %s: "
			       "now running without any active interface !\n",
			       bond->dev->name);
		}
	}
}

/*--------------------------- slave list handling ---------------------------*/

/*
 * This function attaches the slave to the end of list.
 *
 * bond->lock held for writing by caller.
 */
static void bond_attach_slave(struct bonding *bond, struct slave *new_slave)
{
	if (bond->first_slave == NULL) { /* attaching the first slave */
		new_slave->next = new_slave;
		new_slave->prev = new_slave;
		bond->first_slave = new_slave;
	} else {
		new_slave->next = bond->first_slave;
		new_slave->prev = bond->first_slave->prev;
		new_slave->next->prev = new_slave;
		new_slave->prev->next = new_slave;
	}

	bond->slave_cnt++;
}

/*
 * This function detaches the slave from the list.
 * WARNING: no check is made to verify if the slave effectively
 * belongs to <bond>.
 * Nothing is freed on return, structures are just unchained.
 * If any slave pointer in bond was pointing to <slave>,
 * it should be changed by the calling function.
 *
 * bond->lock held for writing by caller.
 */
static void bond_detach_slave(struct bonding *bond, struct slave *slave)
{
	if (slave->next)
		slave->next->prev = slave->prev;

	if (slave->prev)
		slave->prev->next = slave->next;

	if (bond->first_slave == slave) { /* slave is the first slave */
		if (bond->slave_cnt > 1) { /* there are more slave */
			bond->first_slave = slave->next;
		} else {
			bond->first_slave = NULL; /* slave was the last one */
		}
	}

	slave->next = NULL;
	slave->prev = NULL;
	bond->slave_cnt--;
}

/*---------------------------------- IOCTL ----------------------------------*/

static int bond_sethwaddr(struct net_device *bond_dev,
			  struct net_device *slave_dev)
{
	pr_debug("bond_dev=%p\n", bond_dev);
	pr_debug("slave_dev=%p\n", slave_dev);
	pr_debug("slave_dev->addr_len=%d\n", slave_dev->addr_len);
	memcpy(bond_dev->dev_addr, slave_dev->dev_addr, slave_dev->addr_len);
	return 0;
}

#define BOND_VLAN_FEATURES \
	(NETIF_F_VLAN_CHALLENGED | NETIF_F_HW_VLAN_RX | NETIF_F_HW_VLAN_TX | \
	 NETIF_F_HW_VLAN_FILTER)

/*
 * Compute the common dev->feature set available to all slaves.  Some
 * feature bits are managed elsewhere, so preserve those feature bits
 * on the master device.
 */
static int bond_compute_features(struct bonding *bond)
{
	struct slave *slave;
	struct net_device *bond_dev = bond->dev;
	unsigned long features = bond_dev->features;
	unsigned long vlan_features = 0;
	unsigned short max_hard_header_len = max((u16)ETH_HLEN,
						bond_dev->hard_header_len);
	int i;

	features &= ~(NETIF_F_ALL_CSUM | BOND_VLAN_FEATURES);
	features |=  NETIF_F_GSO_MASK | NETIF_F_NO_CSUM;

	if (!bond->first_slave)
		goto done;

	features &= ~NETIF_F_ONE_FOR_ALL;

	vlan_features = bond->first_slave->dev->vlan_features;
	bond_for_each_slave(bond, slave, i) {
		features = netdev_increment_features(features,
						     slave->dev->features,
						     NETIF_F_ONE_FOR_ALL);
		vlan_features = netdev_increment_features(vlan_features,
							slave->dev->vlan_features,
							NETIF_F_ONE_FOR_ALL);
		if (slave->dev->hard_header_len > max_hard_header_len)
			max_hard_header_len = slave->dev->hard_header_len;
	}

done:
	features |= (bond_dev->features & BOND_VLAN_FEATURES);
	bond_dev->features = netdev_fix_features(features, NULL);
	bond_dev->vlan_features = netdev_fix_features(vlan_features, NULL);
	bond_dev->hard_header_len = max_hard_header_len;

	return 0;
}

static void bond_setup_by_slave(struct net_device *bond_dev,
				struct net_device *slave_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);

	bond_dev->header_ops	    = slave_dev->header_ops;

	bond_dev->type		    = slave_dev->type;
	bond_dev->hard_header_len   = slave_dev->hard_header_len;
	bond_dev->addr_len	    = slave_dev->addr_len;

	memcpy(bond_dev->broadcast, slave_dev->broadcast,
		slave_dev->addr_len);
	bond->setup_by_slave = 1;
}

/* enslave device <slave> to bond device <master> */
int bond_enslave(struct net_device *bond_dev, struct net_device *slave_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	const struct net_device_ops *slave_ops = slave_dev->netdev_ops;
	struct slave *new_slave = NULL;
	struct dev_mc_list *dmi;
	struct sockaddr addr;
	int link_reporting;
	int old_features = bond_dev->features;
	int res = 0;

	if (!bond->params.use_carrier && slave_dev->ethtool_ops == NULL &&
		slave_ops->ndo_do_ioctl == NULL) {
		pr_warning(DRV_NAME
		       ": %s: Warning: no link monitoring support for %s\n",
		       bond_dev->name, slave_dev->name);
	}

	/* bond must be initialized by bond_open() before enslaving */
	if (!(bond_dev->flags & IFF_UP)) {
		pr_warning(DRV_NAME
			" %s: master_dev is not up in bond_enslave\n",
			bond_dev->name);
	}

	/* already enslaved */
	if (slave_dev->flags & IFF_SLAVE) {
		pr_debug("Error, Device was already enslaved\n");
		return -EBUSY;
	}

	/* vlan challenged mutual exclusion */
	/* no need to lock since we're protected by rtnl_lock */
	if (slave_dev->features & NETIF_F_VLAN_CHALLENGED) {
		pr_debug("%s: NETIF_F_VLAN_CHALLENGED\n", slave_dev->name);
		if (!list_empty(&bond->vlan_list)) {
			pr_err(DRV_NAME
			       ": %s: Error: cannot enslave VLAN "
			       "challenged slave %s on VLAN enabled "
			       "bond %s\n", bond_dev->name, slave_dev->name,
			       bond_dev->name);
			return -EPERM;
		} else {
			pr_warning(DRV_NAME
			       ": %s: Warning: enslaved VLAN challenged "
			       "slave %s. Adding VLANs will be blocked as "
			       "long as %s is part of bond %s\n",
			       bond_dev->name, slave_dev->name, slave_dev->name,
			       bond_dev->name);
			bond_dev->features |= NETIF_F_VLAN_CHALLENGED;
		}
	} else {
		pr_debug("%s: ! NETIF_F_VLAN_CHALLENGED\n", slave_dev->name);
		if (bond->slave_cnt == 0) {
			/* First slave, and it is not VLAN challenged,
			 * so remove the block of adding VLANs over the bond.
			 */
			bond_dev->features &= ~NETIF_F_VLAN_CHALLENGED;
		}
	}

	/*
	 * Old ifenslave binaries are no longer supported.  These can
	 * be identified with moderate accuracy by the state of the slave:
	 * the current ifenslave will set the interface down prior to
	 * enslaving it; the old ifenslave will not.
	 */
	if ((slave_dev->flags & IFF_UP)) {
		pr_err(DRV_NAME ": %s is up. "
		       "This may be due to an out of date ifenslave.\n",
		       slave_dev->name);
		res = -EPERM;
		goto err_undo_flags;
	}

	/* set bonding device ether type by slave - bonding netdevices are
	 * created with ether_setup, so when the slave type is not ARPHRD_ETHER
	 * there is a need to override some of the type dependent attribs/funcs.
	 *
	 * bond ether type mutual exclusion - don't allow slaves of dissimilar
	 * ether type (eg ARPHRD_ETHER and ARPHRD_INFINIBAND) share the same bond
	 */
	if (bond->slave_cnt == 0) {
		if (bond_dev->type != slave_dev->type) {
			pr_debug("%s: change device type from %d to %d\n",
				bond_dev->name, bond_dev->type, slave_dev->type);

			netdev_bonding_change(bond_dev, NETDEV_BONDING_OLDTYPE);

			if (slave_dev->type != ARPHRD_ETHER)
				bond_setup_by_slave(bond_dev, slave_dev);
			else
				ether_setup(bond_dev);

			netdev_bonding_change(bond_dev, NETDEV_BONDING_NEWTYPE);
		}
	} else if (bond_dev->type != slave_dev->type) {
		pr_err(DRV_NAME ": %s ether type (%d) is different "
			"from other slaves (%d), can not enslave it.\n",
			slave_dev->name,
			slave_dev->type, bond_dev->type);
			res = -EINVAL;
			goto err_undo_flags;
	}

	if (slave_ops->ndo_set_mac_address == NULL) {
		if (bond->slave_cnt == 0) {
			pr_warning(DRV_NAME
			       ": %s: Warning: The first slave device "
			       "specified does not support setting the MAC "
			       "address. Setting fail_over_mac to active.",
			       bond_dev->name);
			bond->params.fail_over_mac = BOND_FOM_ACTIVE;
		} else if (bond->params.fail_over_mac != BOND_FOM_ACTIVE) {
			pr_err(DRV_NAME
				": %s: Error: The slave device specified "
				"does not support setting the MAC address, "
				"but fail_over_mac is not set to active.\n"
				, bond_dev->name);
			res = -EOPNOTSUPP;
			goto err_undo_flags;
		}
	}

	new_slave = kzalloc(sizeof(struct slave), GFP_KERNEL);
	if (!new_slave) {
		res = -ENOMEM;
		goto err_undo_flags;
	}

	/* save slave's original flags before calling
	 * netdev_set_master and dev_open
	 */
	new_slave->original_flags = slave_dev->flags;

	/*
	 * Save slave's original ("permanent") mac address for modes
	 * that need it, and for restoring it upon release, and then
	 * set it to the master's address
	 */
	memcpy(new_slave->perm_hwaddr, slave_dev->dev_addr, ETH_ALEN);

	if (!bond->params.fail_over_mac) {
		/*
		 * Set slave to master's mac address.  The application already
		 * set the master's mac address to that of the first slave
		 */
		memcpy(addr.sa_data, bond_dev->dev_addr, bond_dev->addr_len);
		addr.sa_family = slave_dev->type;
		res = dev_set_mac_address(slave_dev, &addr);
		if (res) {
			pr_debug("Error %d calling set_mac_address\n", res);
			goto err_free;
		}
	}

	res = netdev_set_master(slave_dev, bond_dev);
	if (res) {
		pr_debug("Error %d calling netdev_set_master\n", res);
		goto err_restore_mac;
	}
	/* open the slave since the application closed it */
	res = dev_open(slave_dev);
	if (res) {
		pr_debug("Opening slave %s failed\n", slave_dev->name);
		goto err_unset_master;
	}

	new_slave->dev = slave_dev;
	slave_dev->priv_flags |= IFF_BONDING;

	if (bond_is_lb(bond)) {
		/* bond_alb_init_slave() must be called before all other stages since
		 * it might fail and we do not want to have to undo everything
		 */
		res = bond_alb_init_slave(bond, new_slave);
		if (res)
			goto err_close;
	}

	/* If the mode USES_PRIMARY, then the new slave gets the
	 * master's promisc (and mc) settings only if it becomes the
	 * curr_active_slave, and that is taken care of later when calling
	 * bond_change_active()
	 */
	if (!USES_PRIMARY(bond->params.mode)) {
		/* set promiscuity level to new slave */
		if (bond_dev->flags & IFF_PROMISC) {
			res = dev_set_promiscuity(slave_dev, 1);
			if (res)
				goto err_close;
		}

		/* set allmulti level to new slave */
		if (bond_dev->flags & IFF_ALLMULTI) {
			res = dev_set_allmulti(slave_dev, 1);
			if (res)
				goto err_close;
		}

		netif_addr_lock_bh(bond_dev);
		/* upload master's mc_list to new slave */
		for (dmi = bond_dev->mc_list; dmi; dmi = dmi->next)
			dev_mc_add(slave_dev, dmi->dmi_addr,
				   dmi->dmi_addrlen, 0);
		netif_addr_unlock_bh(bond_dev);
	}

	if (bond->params.mode == BOND_MODE_8023AD) {
		/* add lacpdu mc addr to mc list */
		u8 lacpdu_multicast[ETH_ALEN] = MULTICAST_LACPDU_ADDR;

		dev_mc_add(slave_dev, lacpdu_multicast, ETH_ALEN, 0);
	}

	bond_add_vlans_on_slave(bond, slave_dev);

	write_lock_bh(&bond->lock);

	bond_attach_slave(bond, new_slave);

	new_slave->delay = 0;
	new_slave->link_failure_count = 0;

	bond_compute_features(bond);

	write_unlock_bh(&bond->lock);

	read_lock(&bond->lock);

	new_slave->last_arp_rx = jiffies;

	if (bond->params.miimon && !bond->params.use_carrier) {
		link_reporting = bond_check_dev_link(bond, slave_dev, 1);

		if ((link_reporting == -1) && !bond->params.arp_interval) {
			/*
			 * miimon is set but a bonded network driver
			 * does not support ETHTOOL/MII and
			 * arp_interval is not set.  Note: if
			 * use_carrier is enabled, we will never go
			 * here (because netif_carrier is always
			 * supported); thus, we don't need to change
			 * the messages for netif_carrier.
			 */
			pr_warning(DRV_NAME
			       ": %s: Warning: MII and ETHTOOL support not "
			       "available for interface %s, and "
			       "arp_interval/arp_ip_target module parameters "
			       "not specified, thus bonding will not detect "
			       "link failures! see bonding.txt for details.\n",
			       bond_dev->name, slave_dev->name);
		} else if (link_reporting == -1) {
			/* unable get link status using mii/ethtool */
			pr_warning(DRV_NAME
			       ": %s: Warning: can't get link status from "
			       "interface %s; the network driver associated "
			       "with this interface does not support MII or "
			       "ETHTOOL link status reporting, thus miimon "
			       "has no effect on this interface.\n",
			       bond_dev->name, slave_dev->name);
		}
	}

	/* check for initial state */
	if (!bond->params.miimon ||
	    (bond_check_dev_link(bond, slave_dev, 0) == BMSR_LSTATUS)) {
		if (bond->params.updelay) {
			pr_debug("Initial state of slave_dev is "
				"BOND_LINK_BACK\n");
			new_slave->link  = BOND_LINK_BACK;
			new_slave->delay = bond->params.updelay;
		} else {
			pr_debug("Initial state of slave_dev is "
				"BOND_LINK_UP\n");
			new_slave->link  = BOND_LINK_UP;
		}
		new_slave->jiffies = jiffies;
	} else {
		pr_debug("Initial state of slave_dev is "
			"BOND_LINK_DOWN\n");
		new_slave->link  = BOND_LINK_DOWN;
	}

	if (bond_update_speed_duplex(new_slave) &&
	    (new_slave->link != BOND_LINK_DOWN)) {
		pr_warning(DRV_NAME
		       ": %s: Warning: failed to get speed and duplex from %s, "
		       "assumed to be 100Mb/sec and Full.\n",
		       bond_dev->name, new_slave->dev->name);

		if (bond->params.mode == BOND_MODE_8023AD) {
			pr_warning(DRV_NAME
			       ": %s: Warning: Operation of 802.3ad mode requires ETHTOOL "
			       "support in base driver for proper aggregator "
			       "selection.\n", bond_dev->name);
		}
	}

	if (USES_PRIMARY(bond->params.mode) && bond->params.primary[0]) {
		/* if there is a primary slave, remember it */
		if (strcmp(bond->params.primary, new_slave->dev->name) == 0)
			bond->primary_slave = new_slave;
	}

	write_lock_bh(&bond->curr_slave_lock);

	switch (bond->params.mode) {
	case BOND_MODE_ACTIVEBACKUP:
		bond_set_slave_inactive_flags(new_slave);
		bond_select_active_slave(bond);
		break;
	case BOND_MODE_8023AD:
		/* in 802.3ad mode, the internal mechanism
		 * will activate the slaves in the selected
		 * aggregator
		 */
		bond_set_slave_inactive_flags(new_slave);
		/* if this is the first slave */
		if (bond->slave_cnt == 1) {
			SLAVE_AD_INFO(new_slave).id = 1;
			/* Initialize AD with the number of times that the AD timer is called in 1 second
			 * can be called only after the mac address of the bond is set
			 */
			bond_3ad_initialize(bond, 1000/AD_TIMER_INTERVAL,
					    bond->params.lacp_fast);
		} else {
			SLAVE_AD_INFO(new_slave).id =
				SLAVE_AD_INFO(new_slave->prev).id + 1;
		}

		bond_3ad_bind_slave(new_slave);
		break;
	case BOND_MODE_TLB:
	case BOND_MODE_ALB:
		new_slave->state = BOND_STATE_ACTIVE;
		bond_set_slave_inactive_flags(new_slave);
		bond_select_active_slave(bond);
		break;
	default:
		pr_debug("This slave is always active in trunk mode\n");

		/* always active in trunk mode */
		new_slave->state = BOND_STATE_ACTIVE;

		/* In trunking mode there is little meaning to curr_active_slave
		 * anyway (it holds no special properties of the bond device),
		 * so we can change it without calling change_active_interface()
		 */
		if (!bond->curr_active_slave)
			bond->curr_active_slave = new_slave;

		break;
	} /* switch(bond_mode) */

	write_unlock_bh(&bond->curr_slave_lock);

	bond_set_carrier(bond);

	read_unlock(&bond->lock);

	res = bond_create_slave_symlinks(bond_dev, slave_dev);
	if (res)
		goto err_close;

	pr_info(DRV_NAME
	       ": %s: enslaving %s as a%s interface with a%s link.\n",
	       bond_dev->name, slave_dev->name,
	       new_slave->state == BOND_STATE_ACTIVE ? "n active" : " backup",
	       new_slave->link != BOND_LINK_DOWN ? "n up" : " down");

	/* enslave is successful */
	return 0;

/* Undo stages on error */
err_close:
	dev_close(slave_dev);

err_unset_master:
	netdev_set_master(slave_dev, NULL);

err_restore_mac:
	if (!bond->params.fail_over_mac) {
		/* XXX TODO - fom follow mode needs to change master's
		 * MAC if this slave's MAC is in use by the bond, or at
		 * least print a warning.
		 */
		memcpy(addr.sa_data, new_slave->perm_hwaddr, ETH_ALEN);
		addr.sa_family = slave_dev->type;
		dev_set_mac_address(slave_dev, &addr);
	}

err_free:
	kfree(new_slave);

err_undo_flags:
	bond_dev->features = old_features;

	return res;
}

/*
 * Try to release the slave device <slave> from the bond device <master>
 * It is legal to access curr_active_slave without a lock because all the function
 * is write-locked.
 *
 * The rules for slave state should be:
 *   for Active/Backup:
 *     Active stays on all backups go down
 *   for Bonded connections:
 *     The first up interface should be left on and all others downed.
 */
int bond_release(struct net_device *bond_dev, struct net_device *slave_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave, *oldcurrent;
	struct sockaddr addr;

	/* slave is not a slave or master is not master of this slave */
	if (!(slave_dev->flags & IFF_SLAVE) ||
	    (slave_dev->master != bond_dev)) {
		pr_err(DRV_NAME
		       ": %s: Error: cannot release %s.\n",
		       bond_dev->name, slave_dev->name);
		return -EINVAL;
	}

	write_lock_bh(&bond->lock);

	slave = bond_get_slave_by_dev(bond, slave_dev);
	if (!slave) {
		/* not a slave of this bond */
		pr_info(DRV_NAME
		       ": %s: %s not enslaved\n",
		       bond_dev->name, slave_dev->name);
		write_unlock_bh(&bond->lock);
		return -EINVAL;
	}

	if (!bond->params.fail_over_mac) {
		if (!compare_ether_addr(bond_dev->dev_addr, slave->perm_hwaddr)
		    && bond->slave_cnt > 1)
			pr_warning(DRV_NAME
			       ": %s: Warning: the permanent HWaddr of %s - "
			       "%pM - is still in use by %s. "
			       "Set the HWaddr of %s to a different address "
			       "to avoid conflicts.\n",
			       bond_dev->name, slave_dev->name,
			       slave->perm_hwaddr,
			       bond_dev->name, slave_dev->name);
	}

	/* Inform AD package of unbinding of slave. */
	if (bond->params.mode == BOND_MODE_8023AD) {
		/* must be called before the slave is
		 * detached from the list
		 */
		bond_3ad_unbind_slave(slave);
	}

	pr_info(DRV_NAME
	       ": %s: releasing %s interface %s\n",
	       bond_dev->name,
	       (slave->state == BOND_STATE_ACTIVE)
	       ? "active" : "backup",
	       slave_dev->name);

	oldcurrent = bond->curr_active_slave;

	bond->current_arp_slave = NULL;

	/* release the slave from its bond */
	bond_detach_slave(bond, slave);

	bond_compute_features(bond);

	if (bond->primary_slave == slave)
		bond->primary_slave = NULL;

	if (oldcurrent == slave)
		bond_change_active_slave(bond, NULL);

	if (bond_is_lb(bond)) {
		/* Must be called only after the slave has been
		 * detached from the list and the curr_active_slave
		 * has been cleared (if our_slave == old_current),
		 * but before a new active slave is selected.
		 */
		write_unlock_bh(&bond->lock);
		bond_alb_deinit_slave(bond, slave);
		write_lock_bh(&bond->lock);
	}

	if (oldcurrent == slave) {
		/*
		 * Note that we hold RTNL over this sequence, so there
		 * is no concern that another slave add/remove event
		 * will interfere.
		 */
		write_unlock_bh(&bond->lock);
		read_lock(&bond->lock);
		write_lock_bh(&bond->curr_slave_lock);

		bond_select_active_slave(bond);

		write_unlock_bh(&bond->curr_slave_lock);
		read_unlock(&bond->lock);
		write_lock_bh(&bond->lock);
	}

	if (bond->slave_cnt == 0) {
		bond_set_carrier(bond);

		/* if the last slave was removed, zero the mac address
		 * of the master so it will be set by the application
		 * to the mac address of the first slave
		 */
		memset(bond_dev->dev_addr, 0, bond_dev->addr_len);

		if (list_empty(&bond->vlan_list)) {
			bond_dev->features |= NETIF_F_VLAN_CHALLENGED;
		} else {
			pr_warning(DRV_NAME
			       ": %s: Warning: clearing HW address of %s while it "
			       "still has VLANs.\n",
			       bond_dev->name, bond_dev->name);
			pr_warning(DRV_NAME
			       ": %s: When re-adding slaves, make sure the bond's "
			       "HW address matches its VLANs'.\n",
			       bond_dev->name);
		}
	} else if ((bond_dev->features & NETIF_F_VLAN_CHALLENGED) &&
		   !bond_has_challenged_slaves(bond)) {
		pr_info(DRV_NAME
		       ": %s: last VLAN challenged slave %s "
		       "left bond %s. VLAN blocking is removed\n",
		       bond_dev->name, slave_dev->name, bond_dev->name);
		bond_dev->features &= ~NETIF_F_VLAN_CHALLENGED;
	}

	write_unlock_bh(&bond->lock);

	/* must do this from outside any spinlocks */
	bond_destroy_slave_symlinks(bond_dev, slave_dev);

	bond_del_vlans_from_slave(bond, slave_dev);

	/* If the mode USES_PRIMARY, then we should only remove its
	 * promisc and mc settings if it was the curr_active_slave, but that was
	 * already taken care of above when we detached the slave
	 */
	if (!USES_PRIMARY(bond->params.mode)) {
		/* unset promiscuity level from slave */
		if (bond_dev->flags & IFF_PROMISC)
			dev_set_promiscuity(slave_dev, -1);

		/* unset allmulti level from slave */
		if (bond_dev->flags & IFF_ALLMULTI)
			dev_set_allmulti(slave_dev, -1);

		/* flush master's mc_list from slave */
		netif_addr_lock_bh(bond_dev);
		bond_mc_list_flush(bond_dev, slave_dev);
		netif_addr_unlock_bh(bond_dev);
	}

	netdev_set_master(slave_dev, NULL);

	/* close slave before restoring its mac address */
	dev_close(slave_dev);

	if (bond->params.fail_over_mac != BOND_FOM_ACTIVE) {
		/* restore original ("permanent") mac address */
		memcpy(addr.sa_data, slave->perm_hwaddr, ETH_ALEN);
		addr.sa_family = slave_dev->type;
		dev_set_mac_address(slave_dev, &addr);
	}

	slave_dev->priv_flags &= ~(IFF_MASTER_8023AD | IFF_MASTER_ALB |
				   IFF_SLAVE_INACTIVE | IFF_BONDING |
				   IFF_SLAVE_NEEDARP);

	kfree(slave);

	return 0;  /* deletion OK */
}

/*
* Destroy a bonding device.
* Must be under rtnl_lock when this function is called.
*/
static void bond_uninit(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);

	bond_deinit(bond_dev);
	bond_destroy_sysfs_entry(bond);

	if (bond->wq)
		destroy_workqueue(bond->wq);

	netif_addr_lock_bh(bond_dev);
	bond_mc_list_destroy(bond);
	netif_addr_unlock_bh(bond_dev);
}

/*
* First release a slave and than destroy the bond if no more slaves are left.
* Must be under rtnl_lock when this function is called.
*/
int  bond_release_and_destroy(struct net_device *bond_dev,
			      struct net_device *slave_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	int ret;

	ret = bond_release(bond_dev, slave_dev);
	if ((ret == 0) && (bond->slave_cnt == 0)) {
		pr_info(DRV_NAME ": %s: destroying bond %s.\n",
		       bond_dev->name, bond_dev->name);
		unregister_netdevice(bond_dev);
	}
	return ret;
}

/*
 * This function releases all slaves.
 */
static int bond_release_all(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave;
	struct net_device *slave_dev;
	struct sockaddr addr;

	write_lock_bh(&bond->lock);

	netif_carrier_off(bond_dev);

	if (bond->slave_cnt == 0)
		goto out;

	bond->current_arp_slave = NULL;
	bond->primary_slave = NULL;
	bond_change_active_slave(bond, NULL);

	while ((slave = bond->first_slave) != NULL) {
		/* Inform AD package of unbinding of slave
		 * before slave is detached from the list.
		 */
		if (bond->params.mode == BOND_MODE_8023AD)
			bond_3ad_unbind_slave(slave);

		slave_dev = slave->dev;
		bond_detach_slave(bond, slave);

		/* now that the slave is detached, unlock and perform
		 * all the undo steps that should not be called from
		 * within a lock.
		 */
		write_unlock_bh(&bond->lock);

		if (bond_is_lb(bond)) {
			/* must be called only after the slave
			 * has been detached from the list
			 */
			bond_alb_deinit_slave(bond, slave);
		}

		bond_compute_features(bond);

		bond_destroy_slave_symlinks(bond_dev, slave_dev);
		bond_del_vlans_from_slave(bond, slave_dev);

		/* If the mode USES_PRIMARY, then we should only remove its
		 * promisc and mc settings if it was the curr_active_slave, but that was
		 * already taken care of above when we detached the slave
		 */
		if (!USES_PRIMARY(bond->params.mode)) {
			/* unset promiscuity level from slave */
			if (bond_dev->flags & IFF_PROMISC)
				dev_set_promiscuity(slave_dev, -1);

			/* unset allmulti level from slave */
			if (bond_dev->flags & IFF_ALLMULTI)
				dev_set_allmulti(slave_dev, -1);

			/* flush master's mc_list from slave */
			netif_addr_lock_bh(bond_dev);
			bond_mc_list_flush(bond_dev, slave_dev);
			netif_addr_unlock_bh(bond_dev);
		}

		netdev_set_master(slave_dev, NULL);

		/* close slave before restoring its mac address */
		dev_close(slave_dev);

		if (!bond->params.fail_over_mac) {
			/* restore original ("permanent") mac address*/
			memcpy(addr.sa_data, slave->perm_hwaddr, ETH_ALEN);
			addr.sa_family = slave_dev->type;
			dev_set_mac_address(slave_dev, &addr);
		}

		slave_dev->priv_flags &= ~(IFF_MASTER_8023AD | IFF_MASTER_ALB |
					   IFF_SLAVE_INACTIVE);

		kfree(slave);

		/* re-acquire the lock before getting the next slave */
		write_lock_bh(&bond->lock);
	}

	/* zero the mac address of the master so it will be
	 * set by the application to the mac address of the
	 * first slave
	 */
	memset(bond_dev->dev_addr, 0, bond_dev->addr_len);

	if (list_empty(&bond->vlan_list))
		bond_dev->features |= NETIF_F_VLAN_CHALLENGED;
	else {
		pr_warning(DRV_NAME
		       ": %s: Warning: clearing HW address of %s while it "
		       "still has VLANs.\n",
		       bond_dev->name, bond_dev->name);
		pr_warning(DRV_NAME
		       ": %s: When re-adding slaves, make sure the bond's "
		       "HW address matches its VLANs'.\n",
		       bond_dev->name);
	}

	pr_info(DRV_NAME
	       ": %s: released all slaves\n",
	       bond_dev->name);

out:
	write_unlock_bh(&bond->lock);

	return 0;
}

/*
 * This function changes the active slave to slave <slave_dev>.
 * It returns -EINVAL in the following cases.
 *  - <slave_dev> is not found in the list.
 *  - There is not active slave now.
 *  - <slave_dev> is already active.
 *  - The link state of <slave_dev> is not BOND_LINK_UP.
 *  - <slave_dev> is not running.
 * In these cases, this function does nothing.
 * In the other cases, current_slave pointer is changed and 0 is returned.
 */
static int bond_ioctl_change_active(struct net_device *bond_dev, struct net_device *slave_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *old_active = NULL;
	struct slave *new_active = NULL;
	int res = 0;

	if (!USES_PRIMARY(bond->params.mode))
		return -EINVAL;

	/* Verify that master_dev is indeed the master of slave_dev */
	if (!(slave_dev->flags & IFF_SLAVE) || (slave_dev->master != bond_dev))
		return -EINVAL;

	read_lock(&bond->lock);

	read_lock(&bond->curr_slave_lock);
	old_active = bond->curr_active_slave;
	read_unlock(&bond->curr_slave_lock);

	new_active = bond_get_slave_by_dev(bond, slave_dev);

	/*
	 * Changing to the current active: do nothing; return success.
	 */
	if (new_active && (new_active == old_active)) {
		read_unlock(&bond->lock);
		return 0;
	}

	if ((new_active) &&
	    (old_active) &&
	    (new_active->link == BOND_LINK_UP) &&
	    IS_UP(new_active->dev)) {
		write_lock_bh(&bond->curr_slave_lock);
		bond_change_active_slave(bond, new_active);
		write_unlock_bh(&bond->curr_slave_lock);
	} else
		res = -EINVAL;

	read_unlock(&bond->lock);

	return res;
}

static int bond_info_query(struct net_device *bond_dev, struct ifbond *info)
{
	struct bonding *bond = netdev_priv(bond_dev);

	info->bond_mode = bond->params.mode;
	info->miimon = bond->params.miimon;

	read_lock(&bond->lock);
	info->num_slaves = bond->slave_cnt;
	read_unlock(&bond->lock);

	return 0;
}

static int bond_slave_info_query(struct net_device *bond_dev, struct ifslave *info)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave;
	int i, res = -ENODEV;

	read_lock(&bond->lock);

	bond_for_each_slave(bond, slave, i) {
		if (i == (int)info->slave_id) {
			res = 0;
			strcpy(info->slave_name, slave->dev->name);
			info->link = slave->link;
			info->state = slave->state;
			info->link_failure_count = slave->link_failure_count;
			break;
		}
	}

	read_unlock(&bond->lock);

	return res;
}

/*-------------------------------- Monitoring -------------------------------*/


static int bond_miimon_inspect(struct bonding *bond)
{
	struct slave *slave;
	int i, link_state, commit = 0;
	bool ignore_updelay;

	ignore_updelay = !bond->curr_active_slave ? true : false;

	bond_for_each_slave(bond, slave, i) {
		slave->new_link = BOND_LINK_NOCHANGE;

		link_state = bond_check_dev_link(bond, slave->dev, 0);

		switch (slave->link) {
		case BOND_LINK_UP:
			if (link_state)
				continue;

			slave->link = BOND_LINK_FAIL;
			slave->delay = bond->params.downdelay;
			if (slave->delay) {
				pr_info(DRV_NAME
				       ": %s: link status down for %s"
				       "interface %s, disabling it in %d ms.\n",
				       bond->dev->name,
				       (bond->params.mode ==
					BOND_MODE_ACTIVEBACKUP) ?
				       ((slave->state == BOND_STATE_ACTIVE) ?
					"active " : "backup ") : "",
				       slave->dev->name,
				       bond->params.downdelay * bond->params.miimon);
			}
			/*FALLTHRU*/
		case BOND_LINK_FAIL:
			if (link_state) {
				/*
				 * recovered before downdelay expired
				 */
				slave->link = BOND_LINK_UP;
				slave->jiffies = jiffies;
				pr_info(DRV_NAME
				       ": %s: link status up again after %d "
				       "ms for interface %s.\n",
				       bond->dev->name,
				       (bond->params.downdelay - slave->delay) *
				       bond->params.miimon,
				       slave->dev->name);
				continue;
			}

			if (slave->delay <= 0) {
				slave->new_link = BOND_LINK_DOWN;
				commit++;
				continue;
			}

			slave->delay--;
			break;

		case BOND_LINK_DOWN:
			if (!link_state)
				continue;

			slave->link = BOND_LINK_BACK;
			slave->delay = bond->params.updelay;

			if (slave->delay) {
				pr_info(DRV_NAME
				       ": %s: link status up for "
				       "interface %s, enabling it in %d ms.\n",
				       bond->dev->name, slave->dev->name,
				       ignore_updelay ? 0 :
				       bond->params.updelay *
				       bond->params.miimon);
			}
			/*FALLTHRU*/
		case BOND_LINK_BACK:
			if (!link_state) {
				slave->link = BOND_LINK_DOWN;
				pr_info(DRV_NAME
				       ": %s: link status down again after %d "
				       "ms for interface %s.\n",
				       bond->dev->name,
				       (bond->params.updelay - slave->delay) *
				       bond->params.miimon,
				       slave->dev->name);

				continue;
			}

			if (ignore_updelay)
				slave->delay = 0;

			if (slave->delay <= 0) {
				slave->new_link = BOND_LINK_UP;
				commit++;
				ignore_updelay = false;
				continue;
			}

			slave->delay--;
			break;
		}
	}

	return commit;
}

static void bond_miimon_commit(struct bonding *bond)
{
	struct slave *slave;
	int i;

	bond_for_each_slave(bond, slave, i) {
		switch (slave->new_link) {
		case BOND_LINK_NOCHANGE:
			continue;

		case BOND_LINK_UP:
			slave->link = BOND_LINK_UP;
			slave->jiffies = jiffies;

			if (bond->params.mode == BOND_MODE_8023AD) {
				/* prevent it from being the active one */
				slave->state = BOND_STATE_BACKUP;
			} else if (bond->params.mode != BOND_MODE_ACTIVEBACKUP) {
				/* make it immediately active */
				slave->state = BOND_STATE_ACTIVE;
			} else if (slave != bond->primary_slave) {
				/* prevent it from being the active one */
				slave->state = BOND_STATE_BACKUP;
			}

			pr_info(DRV_NAME
			       ": %s: link status definitely "
			       "up for interface %s.\n",
			       bond->dev->name, slave->dev->name);

			/* notify ad that the link status has changed */
			if (bond->params.mode == BOND_MODE_8023AD)
				bond_3ad_handle_link_change(slave, BOND_LINK_UP);

			if (bond_is_lb(bond))
				bond_alb_handle_link_change(bond, slave,
							    BOND_LINK_UP);

			if (!bond->curr_active_slave ||
			    (slave == bond->primary_slave))
				goto do_failover;

			continue;

		case BOND_LINK_DOWN:
			if (slave->link_failure_count < UINT_MAX)
				slave->link_failure_count++;

			slave->link = BOND_LINK_DOWN;

			if (bond->params.mode == BOND_MODE_ACTIVEBACKUP ||
			    bond->params.mode == BOND_MODE_8023AD)
				bond_set_slave_inactive_flags(slave);

			pr_info(DRV_NAME
			       ": %s: link status definitely down for "
			       "interface %s, disabling it\n",
			       bond->dev->name, slave->dev->name);

			if (bond->params.mode == BOND_MODE_8023AD)
				bond_3ad_handle_link_change(slave,
							    BOND_LINK_DOWN);

			if (bond_is_lb(bond))
				bond_alb_handle_link_change(bond, slave,
							    BOND_LINK_DOWN);

			if (slave == bond->curr_active_slave)
				goto do_failover;

			continue;

		default:
			pr_err(DRV_NAME
			       ": %s: invalid new link %d on slave %s\n",
			       bond->dev->name, slave->new_link,
			       slave->dev->name);
			slave->new_link = BOND_LINK_NOCHANGE;

			continue;
		}

do_failover:
		ASSERT_RTNL();
		write_lock_bh(&bond->curr_slave_lock);
		bond_select_active_slave(bond);
		write_unlock_bh(&bond->curr_slave_lock);
	}

	bond_set_carrier(bond);
}

/*
 * bond_mii_monitor
 *
 * Really a wrapper that splits the mii monitor into two phases: an
 * inspection, then (if inspection indicates something needs to be done)
 * an acquisition of appropriate locks followed by a commit phase to
 * implement whatever link state changes are indicated.
 */
void bond_mii_monitor(struct work_struct *work)
{
	struct bonding *bond = container_of(work, struct bonding,
					    mii_work.work);

	read_lock(&bond->lock);
	if (bond->kill_timers)
		goto out;

	if (bond->slave_cnt == 0)
		goto re_arm;

	if (bond->send_grat_arp) {
		read_lock(&bond->curr_slave_lock);
		bond_send_gratuitous_arp(bond);
		read_unlock(&bond->curr_slave_lock);
	}

	if (bond->send_unsol_na) {
		read_lock(&bond->curr_slave_lock);
		bond_send_unsolicited_na(bond);
		read_unlock(&bond->curr_slave_lock);
	}

	if (bond_miimon_inspect(bond)) {
		read_unlock(&bond->lock);
		rtnl_lock();
		read_lock(&bond->lock);

		bond_miimon_commit(bond);

		read_unlock(&bond->lock);
		rtnl_unlock();	/* might sleep, hold no other locks */
		read_lock(&bond->lock);
	}

re_arm:
	if (bond->params.miimon)
		queue_delayed_work(bond->wq, &bond->mii_work,
				   msecs_to_jiffies(bond->params.miimon));
out:
	read_unlock(&bond->lock);
}

static __be32 bond_glean_dev_ip(struct net_device *dev)
{
	struct in_device *idev;
	struct in_ifaddr *ifa;
	__be32 addr = 0;

	if (!dev)
		return 0;

	rcu_read_lock();
	idev = __in_dev_get_rcu(dev);
	if (!idev)
		goto out;

	ifa = idev->ifa_list;
	if (!ifa)
		goto out;

	addr = ifa->ifa_local;
out:
	rcu_read_unlock();
	return addr;
}

static int bond_has_this_ip(struct bonding *bond, __be32 ip)
{
	struct vlan_entry *vlan;

	if (ip == bond->master_ip)
		return 1;

	list_for_each_entry(vlan, &bond->vlan_list, vlan_list) {
		if (ip == vlan->vlan_ip)
			return 1;
	}

	return 0;
}

/*
 * We go to the (large) trouble of VLAN tagging ARP frames because
 * switches in VLAN mode (especially if ports are configured as
 * "native" to a VLAN) might not pass non-tagged frames.
 */
static void bond_arp_send(struct net_device *slave_dev, int arp_op, __be32 dest_ip, __be32 src_ip, unsigned short vlan_id)
{
	struct sk_buff *skb;

	pr_debug("arp %d on slave %s: dst %x src %x vid %d\n", arp_op,
	       slave_dev->name, dest_ip, src_ip, vlan_id);

	skb = arp_create(arp_op, ETH_P_ARP, dest_ip, slave_dev, src_ip,
			 NULL, slave_dev->dev_addr, NULL);

	if (!skb) {
		pr_err(DRV_NAME ": ARP packet allocation failed\n");
		return;
	}
	if (vlan_id) {
		skb = vlan_put_tag(skb, vlan_id);
		if (!skb) {
			pr_err(DRV_NAME ": failed to insert VLAN tag\n");
			return;
		}
	}
	arp_xmit(skb);
}


static void bond_arp_send_all(struct bonding *bond, struct slave *slave)
{
	int i, vlan_id, rv;
	__be32 *targets = bond->params.arp_targets;
	struct vlan_entry *vlan;
	struct net_device *vlan_dev;
	struct flowi fl;
	struct rtable *rt;

	for (i = 0; (i < BOND_MAX_ARP_TARGETS); i++) {
		if (!targets[i])
			break;
		pr_debug("basa: target %x\n", targets[i]);
		if (list_empty(&bond->vlan_list)) {
			pr_debug("basa: empty vlan: arp_send\n");
			bond_arp_send(slave->dev, ARPOP_REQUEST, targets[i],
				      bond->master_ip, 0);
			continue;
		}

		/*
		 * If VLANs are configured, we do a route lookup to
		 * determine which VLAN interface would be used, so we
		 * can tag the ARP with the proper VLAN tag.
		 */
		memset(&fl, 0, sizeof(fl));
		fl.fl4_dst = targets[i];
		fl.fl4_tos = RTO_ONLINK;

		rv = ip_route_output_key(&init_net, &rt, &fl);
		if (rv) {
			if (net_ratelimit()) {
				pr_warning(DRV_NAME
			     ": %s: no route to arp_ip_target %pI4\n",
				       bond->dev->name, &fl.fl4_dst);
			}
			continue;
		}

		/*
		 * This target is not on a VLAN
		 */
		if (rt->u.dst.dev == bond->dev) {
			ip_rt_put(rt);
			pr_debug("basa: rtdev == bond->dev: arp_send\n");
			bond_arp_send(slave->dev, ARPOP_REQUEST, targets[i],
				      bond->master_ip, 0);
			continue;
		}

		vlan_id = 0;
		list_for_each_entry(vlan, &bond->vlan_list, vlan_list) {
			vlan_dev = vlan_group_get_device(bond->vlgrp, vlan->vlan_id);
			if (vlan_dev == rt->u.dst.dev) {
				vlan_id = vlan->vlan_id;
				pr_debug("basa: vlan match on %s %d\n",
				       vlan_dev->name, vlan_id);
				break;
			}
		}

		if (vlan_id) {
			ip_rt_put(rt);
			bond_arp_send(slave->dev, ARPOP_REQUEST, targets[i],
				      vlan->vlan_ip, vlan_id);
			continue;
		}

		if (net_ratelimit()) {
			pr_warning(DRV_NAME
	       ": %s: no path to arp_ip_target %pI4 via rt.dev %s\n",
			       bond->dev->name, &fl.fl4_dst,
			       rt->u.dst.dev ? rt->u.dst.dev->name : "NULL");
		}
		ip_rt_put(rt);
	}
}

/*
 * Kick out a gratuitous ARP for an IP on the bonding master plus one
 * for each VLAN above us.
 *
 * Caller must hold curr_slave_lock for read or better
 */
static void bond_send_gratuitous_arp(struct bonding *bond)
{
	struct slave *slave = bond->curr_active_slave;
	struct vlan_entry *vlan;
	struct net_device *vlan_dev;

	pr_debug("bond_send_grat_arp: bond %s slave %s\n", bond->dev->name,
				slave ? slave->dev->name : "NULL");

	if (!slave || !bond->send_grat_arp ||
	    test_bit(__LINK_STATE_LINKWATCH_PENDING, &slave->dev->state))
		return;

	bond->send_grat_arp--;

	if (bond->master_ip) {
		bond_arp_send(slave->dev, ARPOP_REPLY, bond->master_ip,
				bond->master_ip, 0);
	}

	list_for_each_entry(vlan, &bond->vlan_list, vlan_list) {
		vlan_dev = vlan_group_get_device(bond->vlgrp, vlan->vlan_id);
		if (vlan->vlan_ip) {
			bond_arp_send(slave->dev, ARPOP_REPLY, vlan->vlan_ip,
				      vlan->vlan_ip, vlan->vlan_id);
		}
	}
}

static void bond_validate_arp(struct bonding *bond, struct slave *slave, __be32 sip, __be32 tip)
{
	int i;
	__be32 *targets = bond->params.arp_targets;

	for (i = 0; (i < BOND_MAX_ARP_TARGETS) && targets[i]; i++) {
		pr_debug("bva: sip %pI4 tip %pI4 t[%d] %pI4 bhti(tip) %d\n",
			&sip, &tip, i, &targets[i], bond_has_this_ip(bond, tip));
		if (sip == targets[i]) {
			if (bond_has_this_ip(bond, tip))
				slave->last_arp_rx = jiffies;
			return;
		}
	}
}

static int bond_arp_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev)
{
	struct arphdr *arp;
	struct slave *slave;
	struct bonding *bond;
	unsigned char *arp_ptr;
	__be32 sip, tip;

	if (dev_net(dev) != &init_net)
		goto out;

	if (!(dev->priv_flags & IFF_BONDING) || !(dev->flags & IFF_MASTER))
		goto out;

	bond = netdev_priv(dev);
	read_lock(&bond->lock);

	pr_debug("bond_arp_rcv: bond %s skb->dev %s orig_dev %s\n",
		bond->dev->name, skb->dev ? skb->dev->name : "NULL",
		orig_dev ? orig_dev->name : "NULL");

	slave = bond_get_slave_by_dev(bond, orig_dev);
	if (!slave || !slave_do_arp_validate(bond, slave))
		goto out_unlock;

	if (!pskb_may_pull(skb, arp_hdr_len(dev)))
		goto out_unlock;

	arp = arp_hdr(skb);
	if (arp->ar_hln != dev->addr_len ||
	    skb->pkt_type == PACKET_OTHERHOST ||
	    skb->pkt_type == PACKET_LOOPBACK ||
	    arp->ar_hrd != htons(ARPHRD_ETHER) ||
	    arp->ar_pro != htons(ETH_P_IP) ||
	    arp->ar_pln != 4)
		goto out_unlock;

	arp_ptr = (unsigned char *)(arp + 1);
	arp_ptr += dev->addr_len;
	memcpy(&sip, arp_ptr, 4);
	arp_ptr += 4 + dev->addr_len;
	memcpy(&tip, arp_ptr, 4);

	pr_debug("bond_arp_rcv: %s %s/%d av %d sv %d sip %pI4 tip %pI4\n",
		bond->dev->name, slave->dev->name, slave->state,
		bond->params.arp_validate, slave_do_arp_validate(bond, slave),
		&sip, &tip);

	/*
	 * Backup slaves won't see the ARP reply, but do come through
	 * here for each ARP probe (so we swap the sip/tip to validate
	 * the probe).  In a "redundant switch, common router" type of
	 * configuration, the ARP probe will (hopefully) travel from
	 * the active, through one switch, the router, then the other
	 * switch before reaching the backup.
	 */
	if (slave->state == BOND_STATE_ACTIVE)
		bond_validate_arp(bond, slave, sip, tip);
	else
		bond_validate_arp(bond, slave, tip, sip);

out_unlock:
	read_unlock(&bond->lock);
out:
	dev_kfree_skb(skb);
	return NET_RX_SUCCESS;
}

/*
 * this function is called regularly to monitor each slave's link
 * ensuring that traffic is being sent and received when arp monitoring
 * is used in load-balancing mode. if the adapter has been dormant, then an
 * arp is transmitted to generate traffic. see activebackup_arp_monitor for
 * arp monitoring in active backup mode.
 */
void bond_loadbalance_arp_mon(struct work_struct *work)
{
	struct bonding *bond = container_of(work, struct bonding,
					    arp_work.work);
	struct slave *slave, *oldcurrent;
	int do_failover = 0;
	int delta_in_ticks;
	int i;

	read_lock(&bond->lock);

	delta_in_ticks = msecs_to_jiffies(bond->params.arp_interval);

	if (bond->kill_timers)
		goto out;

	if (bond->slave_cnt == 0)
		goto re_arm;

	read_lock(&bond->curr_slave_lock);
	oldcurrent = bond->curr_active_slave;
	read_unlock(&bond->curr_slave_lock);

	/* see if any of the previous devices are up now (i.e. they have
	 * xmt and rcv traffic). the curr_active_slave does not come into
	 * the picture unless it is null. also, slave->jiffies is not needed
	 * here because we send an arp on each slave and give a slave as
	 * long as it needs to get the tx/rx within the delta.
	 * TODO: what about up/down delay in arp mode? it wasn't here before
	 *       so it can wait
	 */
	bond_for_each_slave(bond, slave, i) {
		if (slave->link != BOND_LINK_UP) {
			if (time_before_eq(jiffies, dev_trans_start(slave->dev) + delta_in_ticks) &&
			    time_before_eq(jiffies, slave->dev->last_rx + delta_in_ticks)) {

				slave->link  = BOND_LINK_UP;
				slave->state = BOND_STATE_ACTIVE;

				/* primary_slave has no meaning in round-robin
				 * mode. the window of a slave being up and
				 * curr_active_slave being null after enslaving
				 * is closed.
				 */
				if (!oldcurrent) {
					pr_info(DRV_NAME
					       ": %s: link status definitely "
					       "up for interface %s, ",
					       bond->dev->name,
					       slave->dev->name);
					do_failover = 1;
				} else {
					pr_info(DRV_NAME
					       ": %s: interface %s is now up\n",
					       bond->dev->name,
					       slave->dev->name);
				}
			}
		} else {
			/* slave->link == BOND_LINK_UP */

			/* not all switches will respond to an arp request
			 * when the source ip is 0, so don't take the link down
			 * if we don't know our ip yet
			 */
			if (time_after_eq(jiffies, dev_trans_start(slave->dev) + 2*delta_in_ticks) ||
			    (time_after_eq(jiffies, slave->dev->last_rx + 2*delta_in_ticks))) {

				slave->link  = BOND_LINK_DOWN;
				slave->state = BOND_STATE_BACKUP;

				if (slave->link_failure_count < UINT_MAX)
					slave->link_failure_count++;

				pr_info(DRV_NAME
				       ": %s: interface %s is now down.\n",
				       bond->dev->name,
				       slave->dev->name);

				if (slave == oldcurrent)
					do_failover = 1;
			}
		}

		/* note: if switch is in round-robin mode, all links
		 * must tx arp to ensure all links rx an arp - otherwise
		 * links may oscillate or not come up at all; if switch is
		 * in something like xor mode, there is nothing we can
		 * do - all replies will be rx'ed on same link causing slaves
		 * to be unstable during low/no traffic periods
		 */
		if (IS_UP(slave->dev))
			bond_arp_send_all(bond, slave);
	}

	if (do_failover) {
		write_lock_bh(&bond->curr_slave_lock);

		bond_select_active_slave(bond);

		write_unlock_bh(&bond->curr_slave_lock);
	}

re_arm:
	if (bond->params.arp_interval)
		queue_delayed_work(bond->wq, &bond->arp_work, delta_in_ticks);
out:
	read_unlock(&bond->lock);
}

/*
 * Called to inspect slaves for active-backup mode ARP monitor link state
 * changes.  Sets new_link in slaves to specify what action should take
 * place for the slave.  Returns 0 if no changes are found, >0 if changes
 * to link states must be committed.
 *
 * Called with bond->lock held for read.
 */
static int bond_ab_arp_inspect(struct bonding *bond, int delta_in_ticks)
{
	struct slave *slave;
	int i, commit = 0;

	bond_for_each_slave(bond, slave, i) {
		slave->new_link = BOND_LINK_NOCHANGE;

		if (slave->link != BOND_LINK_UP) {
			if (time_before_eq(jiffies, slave_last_rx(bond, slave) +
					   delta_in_ticks)) {
				slave->new_link = BOND_LINK_UP;
				commit++;
			}

			continue;
		}

		/*
		 * Give slaves 2*delta after being enslaved or made
		 * active.  This avoids bouncing, as the last receive
		 * times need a full ARP monitor cycle to be updated.
		 */
		if (!time_after_eq(jiffies, slave->jiffies +
				   2 * delta_in_ticks))
			continue;

		/*
		 * Backup slave is down if:
		 * - No current_arp_slave AND
		 * - more than 3*delta since last receive AND
		 * - the bond has an IP address
		 *
		 * Note: a non-null current_arp_slave indicates
		 * the curr_active_slave went down and we are
		 * searching for a new one; under this condition
		 * we only take the curr_active_slave down - this
		 * gives each slave a chance to tx/rx traffic
		 * before being taken out
		 */
		if (slave->state == BOND_STATE_BACKUP &&
		    !bond->current_arp_slave &&
		    time_after(jiffies, slave_last_rx(bond, slave) +
			       3 * delta_in_ticks)) {
			slave->new_link = BOND_LINK_DOWN;
			commit++;
		}

		/*
		 * Active slave is down if:
		 * - more than 2*delta since transmitting OR
		 * - (more than 2*delta since receive AND
		 *    the bond has an IP address)
		 */
		if ((slave->state == BOND_STATE_ACTIVE) &&
		    (time_after_eq(jiffies, dev_trans_start(slave->dev) +
				    2 * delta_in_ticks) ||
		      (time_after_eq(jiffies, slave_last_rx(bond, slave)
				     + 2 * delta_in_ticks)))) {
			slave->new_link = BOND_LINK_DOWN;
			commit++;
		}
	}

	return commit;
}

/*
 * Called to commit link state changes noted by inspection step of
 * active-backup mode ARP monitor.
 *
 * Called with RTNL and bond->lock for read.
 */
static void bond_ab_arp_commit(struct bonding *bond, int delta_in_ticks)
{
	struct slave *slave;
	int i;

	bond_for_each_slave(bond, slave, i) {
		switch (slave->new_link) {
		case BOND_LINK_NOCHANGE:
			continue;

		case BOND_LINK_UP:
			if ((!bond->curr_active_slave &&
			     time_before_eq(jiffies,
					    dev_trans_start(slave->dev) +
					    delta_in_ticks)) ||
			    bond->curr_active_slave != slave) {
				slave->link = BOND_LINK_UP;
				bond->current_arp_slave = NULL;

				pr_info(DRV_NAME
					": %s: link status definitely "
					"up for interface %s.\n",
					bond->dev->name, slave->dev->name);

				if (!bond->curr_active_slave ||
				    (slave == bond->primary_slave))
					goto do_failover;

			}

			continue;

		case BOND_LINK_DOWN:
			if (slave->link_failure_count < UINT_MAX)
				slave->link_failure_count++;

			slave->link = BOND_LINK_DOWN;
			bond_set_slave_inactive_flags(slave);

			pr_info(DRV_NAME
				": %s: link status definitely down for "
				"interface %s, disabling it\n",
				bond->dev->name, slave->dev->name);

			if (slave == bond->curr_active_slave) {
				bond->current_arp_slave = NULL;
				goto do_failover;
			}

			continue;

		default:
			pr_err(DRV_NAME
			       ": %s: impossible: new_link %d on slave %s\n",
			       bond->dev->name, slave->new_link,
			       slave->dev->name);
			continue;
		}

do_failover:
		ASSERT_RTNL();
		write_lock_bh(&bond->curr_slave_lock);
		bond_select_active_slave(bond);
		write_unlock_bh(&bond->curr_slave_lock);
	}

	bond_set_carrier(bond);
}

/*
 * Send ARP probes for active-backup mode ARP monitor.
 *
 * Called with bond->lock held for read.
 */
static void bond_ab_arp_probe(struct bonding *bond)
{
	struct slave *slave;
	int i;

	read_lock(&bond->curr_slave_lock);

	if (bond->current_arp_slave && bond->curr_active_slave)
		pr_info(DRV_NAME "PROBE: c_arp %s && cas %s BAD\n",
		       bond->current_arp_slave->dev->name,
		       bond->curr_active_slave->dev->name);

	if (bond->curr_active_slave) {
		bond_arp_send_all(bond, bond->curr_active_slave);
		read_unlock(&bond->curr_slave_lock);
		return;
	}

	read_unlock(&bond->curr_slave_lock);

	/* if we don't have a curr_active_slave, search for the next available
	 * backup slave from the current_arp_slave and make it the candidate
	 * for becoming the curr_active_slave
	 */

	if (!bond->current_arp_slave) {
		bond->current_arp_slave = bond->first_slave;
		if (!bond->current_arp_slave)
			return;
	}

	bond_set_slave_inactive_flags(bond->current_arp_slave);

	/* search for next candidate */
	bond_for_each_slave_from(bond, slave, i, bond->current_arp_slave->next) {
		if (IS_UP(slave->dev)) {
			slave->link = BOND_LINK_BACK;
			bond_set_slave_active_flags(slave);
			bond_arp_send_all(bond, slave);
			slave->jiffies = jiffies;
			bond->current_arp_slave = slave;
			break;
		}

		/* if the link state is up at this point, we
		 * mark it down - this can happen if we have
		 * simultaneous link failures and
		 * reselect_active_interface doesn't make this
		 * one the current slave so it is still marked
		 * up when it is actually down
		 */
		if (slave->link == BOND_LINK_UP) {
			slave->link = BOND_LINK_DOWN;
			if (slave->link_failure_count < UINT_MAX)
				slave->link_failure_count++;

			bond_set_slave_inactive_flags(slave);

			pr_info(DRV_NAME
			       ": %s: backup interface %s is now down.\n",
			       bond->dev->name, slave->dev->name);
		}
	}
}

void bond_activebackup_arp_mon(struct work_struct *work)
{
	struct bonding *bond = container_of(work, struct bonding,
					    arp_work.work);
	int delta_in_ticks;

	read_lock(&bond->lock);

	if (bond->kill_timers)
		goto out;

	delta_in_ticks = msecs_to_jiffies(bond->params.arp_interval);

	if (bond->slave_cnt == 0)
		goto re_arm;

	if (bond->send_grat_arp) {
		read_lock(&bond->curr_slave_lock);
		bond_send_gratuitous_arp(bond);
		read_unlock(&bond->curr_slave_lock);
	}

	if (bond->send_unsol_na) {
		read_lock(&bond->curr_slave_lock);
		bond_send_unsolicited_na(bond);
		read_unlock(&bond->curr_slave_lock);
	}

	if (bond_ab_arp_inspect(bond, delta_in_ticks)) {
		read_unlock(&bond->lock);
		rtnl_lock();
		read_lock(&bond->lock);

		bond_ab_arp_commit(bond, delta_in_ticks);

		read_unlock(&bond->lock);
		rtnl_unlock();
		read_lock(&bond->lock);
	}

	bond_ab_arp_probe(bond);

re_arm:
	if (bond->params.arp_interval)
		queue_delayed_work(bond->wq, &bond->arp_work, delta_in_ticks);
out:
	read_unlock(&bond->lock);
}

/*------------------------------ proc/seq_file-------------------------------*/

#ifdef CONFIG_PROC_FS

static void *bond_info_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(&dev_base_lock)
	__acquires(&bond->lock)
{
	struct bonding *bond = seq->private;
	loff_t off = 0;
	struct slave *slave;
	int i;

	/* make sure the bond won't be taken away */
	read_lock(&dev_base_lock);
	read_lock(&bond->lock);

	if (*pos == 0)
		return SEQ_START_TOKEN;

	bond_for_each_slave(bond, slave, i) {
		if (++off == *pos)
			return slave;
	}

	return NULL;
}

static void *bond_info_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct bonding *bond = seq->private;
	struct slave *slave = v;

	++*pos;
	if (v == SEQ_START_TOKEN)
		return bond->first_slave;

	slave = slave->next;

	return (slave == bond->first_slave) ? NULL : slave;
}

static void bond_info_seq_stop(struct seq_file *seq, void *v)
	__releases(&bond->lock)
	__releases(&dev_base_lock)
{
	struct bonding *bond = seq->private;

	read_unlock(&bond->lock);
	read_unlock(&dev_base_lock);
}

static void bond_info_show_master(struct seq_file *seq)
{
	struct bonding *bond = seq->private;
	struct slave *curr;
	int i;

	read_lock(&bond->curr_slave_lock);
	curr = bond->curr_active_slave;
	read_unlock(&bond->curr_slave_lock);

	seq_printf(seq, "Bonding Mode: %s",
		   bond_mode_name(bond->params.mode));

	if (bond->params.mode == BOND_MODE_ACTIVEBACKUP &&
	    bond->params.fail_over_mac)
		seq_printf(seq, " (fail_over_mac %s)",
		   fail_over_mac_tbl[bond->params.fail_over_mac].modename);

	seq_printf(seq, "\n");

	if (bond->params.mode == BOND_MODE_XOR ||
		bond->params.mode == BOND_MODE_8023AD) {
		seq_printf(seq, "Transmit Hash Policy: %s (%d)\n",
			xmit_hashtype_tbl[bond->params.xmit_policy].modename,
			bond->params.xmit_policy);
	}

	if (USES_PRIMARY(bond->params.mode)) {
		seq_printf(seq, "Primary Slave: %s\n",
			   (bond->primary_slave) ?
			   bond->primary_slave->dev->name : "None");

		seq_printf(seq, "Currently Active Slave: %s\n",
			   (curr) ? curr->dev->name : "None");
	}

	seq_printf(seq, "MII Status: %s\n", netif_carrier_ok(bond->dev) ?
		   "up" : "down");
	seq_printf(seq, "MII Polling Interval (ms): %d\n", bond->params.miimon);
	seq_printf(seq, "Up Delay (ms): %d\n",
		   bond->params.updelay * bond->params.miimon);
	seq_printf(seq, "Down Delay (ms): %d\n",
		   bond->params.downdelay * bond->params.miimon);


	/* ARP information */
	if (bond->params.arp_interval > 0) {
		int printed = 0;
		seq_printf(seq, "ARP Polling Interval (ms): %d\n",
				bond->params.arp_interval);

		seq_printf(seq, "ARP IP target/s (n.n.n.n form):");

		for (i = 0; (i < BOND_MAX_ARP_TARGETS); i++) {
			if (!bond->params.arp_targets[i])
				break;
			if (printed)
				seq_printf(seq, ",");
			seq_printf(seq, " %pI4", &bond->params.arp_targets[i]);
			printed = 1;
		}
		seq_printf(seq, "\n");
	}

	if (bond->params.mode == BOND_MODE_8023AD) {
		struct ad_info ad_info;

		seq_puts(seq, "\n802.3ad info\n");
		seq_printf(seq, "LACP rate: %s\n",
			   (bond->params.lacp_fast) ? "fast" : "slow");
		seq_printf(seq, "Aggregator selection policy (ad_select): %s\n",
			   ad_select_tbl[bond->params.ad_select].modename);

		if (bond_3ad_get_active_agg_info(bond, &ad_info)) {
			seq_printf(seq, "bond %s has no active aggregator\n",
				   bond->dev->name);
		} else {
			seq_printf(seq, "Active Aggregator Info:\n");

			seq_printf(seq, "\tAggregator ID: %d\n",
				   ad_info.aggregator_id);
			seq_printf(seq, "\tNumber of ports: %d\n",
				   ad_info.ports);
			seq_printf(seq, "\tActor Key: %d\n",
				   ad_info.actor_key);
			seq_printf(seq, "\tPartner Key: %d\n",
				   ad_info.partner_key);
			seq_printf(seq, "\tPartner Mac Address: %pM\n",
				   ad_info.partner_system);
		}
	}
}

static void bond_info_show_slave(struct seq_file *seq,
				 const struct slave *slave)
{
	struct bonding *bond = seq->private;

	seq_printf(seq, "\nSlave Interface: %s\n", slave->dev->name);
	seq_printf(seq, "MII Status: %s\n",
		   (slave->link == BOND_LINK_UP) ?  "up" : "down");
	seq_printf(seq, "Link Failure Count: %u\n",
		   slave->link_failure_count);

	seq_printf(seq, "Permanent HW addr: %pM\n", slave->perm_hwaddr);

	if (bond->params.mode == BOND_MODE_8023AD) {
		const struct aggregator *agg
			= SLAVE_AD_INFO(slave).port.aggregator;

		if (agg)
			seq_printf(seq, "Aggregator ID: %d\n",
				   agg->aggregator_identifier);
		else
			seq_puts(seq, "Aggregator ID: N/A\n");
	}
}

static int bond_info_seq_show(struct seq_file *seq, void *v)
{
	if (v == SEQ_START_TOKEN) {
		seq_printf(seq, "%s\n", version);
		bond_info_show_master(seq);
	} else
		bond_info_show_slave(seq, v);

	return 0;
}

static const struct seq_operations bond_info_seq_ops = {
	.start = bond_info_seq_start,
	.next  = bond_info_seq_next,
	.stop  = bond_info_seq_stop,
	.show  = bond_info_seq_show,
};

static int bond_info_open(struct inode *inode, struct file *file)
{
	struct seq_file *seq;
	struct proc_dir_entry *proc;
	int res;

	res = seq_open(file, &bond_info_seq_ops);
	if (!res) {
		/* recover the pointer buried in proc_dir_entry data */
		seq = file->private_data;
		proc = PDE(inode);
		seq->private = proc->data;
	}

	return res;
}

static const struct file_operations bond_info_fops = {
	.owner   = THIS_MODULE,
	.open    = bond_info_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

static int bond_create_proc_entry(struct bonding *bond)
{
	struct net_device *bond_dev = bond->dev;

	if (bond_proc_dir) {
		bond->proc_entry = proc_create_data(bond_dev->name,
						    S_IRUGO, bond_proc_dir,
						    &bond_info_fops, bond);
		if (bond->proc_entry == NULL)
			pr_warning(DRV_NAME
			       ": Warning: Cannot create /proc/net/%s/%s\n",
			       DRV_NAME, bond_dev->name);
		else
			memcpy(bond->proc_file_name, bond_dev->name, IFNAMSIZ);
	}

	return 0;
}

static void bond_remove_proc_entry(struct bonding *bond)
{
	if (bond_proc_dir && bond->proc_entry) {
		remove_proc_entry(bond->proc_file_name, bond_proc_dir);
		memset(bond->proc_file_name, 0, IFNAMSIZ);
		bond->proc_entry = NULL;
	}
}

/* Create the bonding directory under /proc/net, if doesn't exist yet.
 * Caller must hold rtnl_lock.
 */
static void bond_create_proc_dir(void)
{
	if (!bond_proc_dir) {
		bond_proc_dir = proc_mkdir(DRV_NAME, init_net.proc_net);
		if (!bond_proc_dir)
			pr_warning(DRV_NAME
				": Warning: cannot create /proc/net/%s\n",
				DRV_NAME);
	}
}

/* Destroy the bonding directory under /proc/net, if empty.
 * Caller must hold rtnl_lock.
 */
static void bond_destroy_proc_dir(void)
{
	if (bond_proc_dir) {
		remove_proc_entry(DRV_NAME, init_net.proc_net);
		bond_proc_dir = NULL;
	}
}

#else /* !CONFIG_PROC_FS */

static int bond_create_proc_entry(struct bonding *bond)
{
}

static void bond_remove_proc_entry(struct bonding *bond)
{
}

static void bond_create_proc_dir(void)
{
}

static void bond_destroy_proc_dir(void)
{
}

#endif /* CONFIG_PROC_FS */


/*-------------------------- netdev event handling --------------------------*/

/*
 * Change device name
 */
static int bond_event_changename(struct bonding *bond)
{
	bond_remove_proc_entry(bond);
	bond_create_proc_entry(bond);

	bond_destroy_sysfs_entry(bond);
	bond_create_sysfs_entry(bond);

	return NOTIFY_DONE;
}

static int bond_master_netdev_event(unsigned long event,
				    struct net_device *bond_dev)
{
	struct bonding *event_bond = netdev_priv(bond_dev);

	switch (event) {
	case NETDEV_CHANGENAME:
		return bond_event_changename(event_bond);
	case NETDEV_UNREGISTER:
		bond_release_all(event_bond->dev);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int bond_slave_netdev_event(unsigned long event,
				   struct net_device *slave_dev)
{
	struct net_device *bond_dev = slave_dev->master;
	struct bonding *bond = netdev_priv(bond_dev);

	switch (event) {
	case NETDEV_UNREGISTER:
		if (bond_dev) {
			if (bond->setup_by_slave)
				bond_release_and_destroy(bond_dev, slave_dev);
			else
				bond_release(bond_dev, slave_dev);
		}
		break;
	case NETDEV_CHANGE:
		if (bond->params.mode == BOND_MODE_8023AD || bond_is_lb(bond)) {
			struct slave *slave;

			slave = bond_get_slave_by_dev(bond, slave_dev);
			if (slave) {
				u16 old_speed = slave->speed;
				u16 old_duplex = slave->duplex;

				bond_update_speed_duplex(slave);

				if (bond_is_lb(bond))
					break;

				if (old_speed != slave->speed)
					bond_3ad_adapter_speed_changed(slave);
				if (old_duplex != slave->duplex)
					bond_3ad_adapter_duplex_changed(slave);
			}
		}

		break;
	case NETDEV_DOWN:
		/*
		 * ... Or is it this?
		 */
		break;
	case NETDEV_CHANGEMTU:
		/*
		 * TODO: Should slaves be allowed to
		 * independently alter their MTU?  For
		 * an active-backup bond, slaves need
		 * not be the same type of device, so
		 * MTUs may vary.  For other modes,
		 * slaves arguably should have the
		 * same MTUs. To do this, we'd need to
		 * take over the slave's change_mtu
		 * function for the duration of their
		 * servitude.
		 */
		break;
	case NETDEV_CHANGENAME:
		/*
		 * TODO: handle changing the primary's name
		 */
		break;
	case NETDEV_FEAT_CHANGE:
		bond_compute_features(bond);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

/*
 * bond_netdev_event: handle netdev notifier chain events.
 *
 * This function receives events for the netdev chain.  The caller (an
 * ioctl handler calling blocking_notifier_call_chain) holds the necessary
 * locks for us to safely manipulate the slave devices (RTNL lock,
 * dev_probe_lock).
 */
static int bond_netdev_event(struct notifier_block *this,
			     unsigned long event, void *ptr)
{
	struct net_device *event_dev = (struct net_device *)ptr;

	if (dev_net(event_dev) != &init_net)
		return NOTIFY_DONE;

	pr_debug("event_dev: %s, event: %lx\n",
		(event_dev ? event_dev->name : "None"),
		event);

	if (!(event_dev->priv_flags & IFF_BONDING))
		return NOTIFY_DONE;

	if (event_dev->flags & IFF_MASTER) {
		pr_debug("IFF_MASTER\n");
		return bond_master_netdev_event(event, event_dev);
	}

	if (event_dev->flags & IFF_SLAVE) {
		pr_debug("IFF_SLAVE\n");
		return bond_slave_netdev_event(event, event_dev);
	}

	return NOTIFY_DONE;
}

/*
 * bond_inetaddr_event: handle inetaddr notifier chain events.
 *
 * We keep track of device IPs primarily to use as source addresses in
 * ARP monitor probes (rather than spewing out broadcasts all the time).
 *
 * We track one IP for the main device (if it has one), plus one per VLAN.
 */
static int bond_inetaddr_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct in_ifaddr *ifa = ptr;
	struct net_device *vlan_dev, *event_dev = ifa->ifa_dev->dev;
	struct bonding *bond;
	struct vlan_entry *vlan;

	if (dev_net(ifa->ifa_dev->dev) != &init_net)
		return NOTIFY_DONE;

	list_for_each_entry(bond, &bond_dev_list, bond_list) {
		if (bond->dev == event_dev) {
			switch (event) {
			case NETDEV_UP:
				bond->master_ip = ifa->ifa_local;
				return NOTIFY_OK;
			case NETDEV_DOWN:
				bond->master_ip = bond_glean_dev_ip(bond->dev);
				return NOTIFY_OK;
			default:
				return NOTIFY_DONE;
			}
		}

		list_for_each_entry(vlan, &bond->vlan_list, vlan_list) {
			vlan_dev = vlan_group_get_device(bond->vlgrp, vlan->vlan_id);
			if (vlan_dev == event_dev) {
				switch (event) {
				case NETDEV_UP:
					vlan->vlan_ip = ifa->ifa_local;
					return NOTIFY_OK;
				case NETDEV_DOWN:
					vlan->vlan_ip =
						bond_glean_dev_ip(vlan_dev);
					return NOTIFY_OK;
				default:
					return NOTIFY_DONE;
				}
			}
		}
	}
	return NOTIFY_DONE;
}

static struct notifier_block bond_netdev_notifier = {
	.notifier_call = bond_netdev_event,
};

static struct notifier_block bond_inetaddr_notifier = {
	.notifier_call = bond_inetaddr_event,
};

/*-------------------------- Packet type handling ---------------------------*/

/* register to receive lacpdus on a bond */
static void bond_register_lacpdu(struct bonding *bond)
{
	struct packet_type *pk_type = &(BOND_AD_INFO(bond).ad_pkt_type);

	/* initialize packet type */
	pk_type->type = PKT_TYPE_LACPDU;
	pk_type->dev = bond->dev;
	pk_type->func = bond_3ad_lacpdu_recv;

	dev_add_pack(pk_type);
}

/* unregister to receive lacpdus on a bond */
static void bond_unregister_lacpdu(struct bonding *bond)
{
	dev_remove_pack(&(BOND_AD_INFO(bond).ad_pkt_type));
}

void bond_register_arp(struct bonding *bond)
{
	struct packet_type *pt = &bond->arp_mon_pt;

	if (pt->type)
		return;

	pt->type = htons(ETH_P_ARP);
	pt->dev = bond->dev;
	pt->func = bond_arp_rcv;
	dev_add_pack(pt);
}

void bond_unregister_arp(struct bonding *bond)
{
	struct packet_type *pt = &bond->arp_mon_pt;

	dev_remove_pack(pt);
	pt->type = 0;
}

/*---------------------------- Hashing Policies -----------------------------*/

/*
 * Hash for the output device based upon layer 2 and layer 3 data. If
 * the packet is not IP mimic bond_xmit_hash_policy_l2()
 */
static int bond_xmit_hash_policy_l23(struct sk_buff *skb,
				     struct net_device *bond_dev, int count)
{
	struct ethhdr *data = (struct ethhdr *)skb->data;
	struct iphdr *iph = ip_hdr(skb);

	if (skb->protocol == htons(ETH_P_IP)) {
		return ((ntohl(iph->saddr ^ iph->daddr) & 0xffff) ^
			(data->h_dest[5] ^ data->h_source[5])) % count;
	}

	return (data->h_dest[5] ^ data->h_source[5]) % count;
}

/*
 * Hash for the output device based upon layer 3 and layer 4 data. If
 * the packet is a frag or not TCP or UDP, just use layer 3 data.  If it is
 * altogether not IP, mimic bond_xmit_hash_policy_l2()
 */
static int bond_xmit_hash_policy_l34(struct sk_buff *skb,
				    struct net_device *bond_dev, int count)
{
	struct ethhdr *data = (struct ethhdr *)skb->data;
	struct iphdr *iph = ip_hdr(skb);
	__be16 *layer4hdr = (__be16 *)((u32 *)iph + iph->ihl);
	int layer4_xor = 0;

	if (skb->protocol == htons(ETH_P_IP)) {
		if (!(iph->frag_off & htons(IP_MF|IP_OFFSET)) &&
		    (iph->protocol == IPPROTO_TCP ||
		     iph->protocol == IPPROTO_UDP)) {
			layer4_xor = ntohs((*layer4hdr ^ *(layer4hdr + 1)));
		}
		return (layer4_xor ^
			((ntohl(iph->saddr ^ iph->daddr)) & 0xffff)) % count;

	}

	return (data->h_dest[5] ^ data->h_source[5]) % count;
}

/*
 * Hash for the output device based upon layer 2 data
 */
static int bond_xmit_hash_policy_l2(struct sk_buff *skb,
				   struct net_device *bond_dev, int count)
{
	struct ethhdr *data = (struct ethhdr *)skb->data;

	return (data->h_dest[5] ^ data->h_source[5]) % count;
}

/*-------------------------- Device entry points ----------------------------*/

static int bond_open(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);

	bond->kill_timers = 0;

	if (bond_is_lb(bond)) {
		/* bond_alb_initialize must be called before the timer
		 * is started.
		 */
		if (bond_alb_initialize(bond, (bond->params.mode == BOND_MODE_ALB))) {
			/* something went wrong - fail the open operation */
			return -1;
		}

		INIT_DELAYED_WORK(&bond->alb_work, bond_alb_monitor);
		queue_delayed_work(bond->wq, &bond->alb_work, 0);
	}

	if (bond->params.miimon) {  /* link check interval, in milliseconds. */
		INIT_DELAYED_WORK(&bond->mii_work, bond_mii_monitor);
		queue_delayed_work(bond->wq, &bond->mii_work, 0);
	}

	if (bond->params.arp_interval) {  /* arp interval, in milliseconds. */
		if (bond->params.mode == BOND_MODE_ACTIVEBACKUP)
			INIT_DELAYED_WORK(&bond->arp_work,
					  bond_activebackup_arp_mon);
		else
			INIT_DELAYED_WORK(&bond->arp_work,
					  bond_loadbalance_arp_mon);

		queue_delayed_work(bond->wq, &bond->arp_work, 0);
		if (bond->params.arp_validate)
			bond_register_arp(bond);
	}

	if (bond->params.mode == BOND_MODE_8023AD) {
		INIT_DELAYED_WORK(&bond->ad_work, bond_3ad_state_machine_handler);
		queue_delayed_work(bond->wq, &bond->ad_work, 0);
		/* register to receive LACPDUs */
		bond_register_lacpdu(bond);
		bond_3ad_initiate_agg_selection(bond, 1);
	}

	return 0;
}

static int bond_close(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);

	if (bond->params.mode == BOND_MODE_8023AD) {
		/* Unregister the receive of LACPDUs */
		bond_unregister_lacpdu(bond);
	}

	if (bond->params.arp_validate)
		bond_unregister_arp(bond);

	write_lock_bh(&bond->lock);

	bond->send_grat_arp = 0;
	bond->send_unsol_na = 0;

	/* signal timers not to re-arm */
	bond->kill_timers = 1;

	write_unlock_bh(&bond->lock);

	if (bond->params.miimon) {  /* link check interval, in milliseconds. */
		cancel_delayed_work(&bond->mii_work);
	}

	if (bond->params.arp_interval) {  /* arp interval, in milliseconds. */
		cancel_delayed_work(&bond->arp_work);
	}

	switch (bond->params.mode) {
	case BOND_MODE_8023AD:
		cancel_delayed_work(&bond->ad_work);
		break;
	case BOND_MODE_TLB:
	case BOND_MODE_ALB:
		cancel_delayed_work(&bond->alb_work);
		break;
	default:
		break;
	}


	if (bond_is_lb(bond)) {
		/* Must be called only after all
		 * slaves have been released
		 */
		bond_alb_deinitialize(bond);
	}

	return 0;
}

static struct net_device_stats *bond_get_stats(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct net_device_stats *stats = &bond->stats;
	struct net_device_stats local_stats;
	struct slave *slave;
	int i;

	memset(&local_stats, 0, sizeof(struct net_device_stats));

	read_lock_bh(&bond->lock);

	bond_for_each_slave(bond, slave, i) {
		const struct net_device_stats *sstats = dev_get_stats(slave->dev);

		local_stats.rx_packets += sstats->rx_packets;
		local_stats.rx_bytes += sstats->rx_bytes;
		local_stats.rx_errors += sstats->rx_errors;
		local_stats.rx_dropped += slly b->sed on the;
* originally batx_packets dummy devicomas Davis * originally batx_bytes, tadavis@lbl.gGPL. Licensed under the errors, tadavis@lbl.g
 * boLicensed under the  on the dummy devic*
 * This opyright 1999, Thmulticast dummy devicel compat * originally bacollisions, tadavis@lbTrunking (opyright 1999, Thrx_length*
 * bonding.c: an *	Linux Bonding
 * originally basedoveronding
 *	and probablyit works:
  * originally basedcrconding
 *	and probably    will s * originally basedframeonding
 *	and probablyNo mac addreth an ip address.  Nifoc address
 *	will be asc address  * originally basedmissedonding
 *	and probablyhe channel.  opyright 1999, Thomaaborthannel.  All slaves wss.
 *
 *    ifcoLicensed under the carri works:
 *    ifconfilaves, marking thLicensed under the ac address will come frwill attach etLicensed under the heartbeat*
 * bonding.c: an Ethwill either
 *	aLicensed under the window*
 * bonding.c: an Eth address alre;
	}

	memcpy(lly b, &riginally b, sizeof(struct net_deviceally b))opyrread_unlock_bh(&bond->incl/kernelturn lly b;
}

lly ic int linu_do_ioctl.
 *
 */

#include *ncludeev, 
 *
 */ifreq *ifr,h>
#icmd)
{
	fcntl.h>
#include <slavex/in = NULL;ce.h>
#iniflinu k_binfoude <linux/in.h>
__user *uinclud>
#include <linux/i/iopo k_sclude <net/ip.h>tcp.h>lude <linuinclu>
#include <linuxmii<linux_data *mii>
#include>
#ires = 0opyrpr_debug("ncludlinux: master=%s,ux/p=%d\n",
		linux/in->name<linu/kernswitch (x/ct {
	case SIOCGMIIPHY:
		
#inclif_mii(ifr) * oif (!mii)
		>
#inclu-EINVALopyrimii->phy_idt.h>
#		/* Fall Through */inux/inet.h>
#iREGude /*
		 * We do this again just in ux/inwnux/re called bynux/uaccess.clude instead ofnet.h>
#incl.clude/de <linux/bitops.h>
#include <linux/io.h>
#include <asincludesystereg_num == 1 <lin	e.h>
#inlinuing <linu =/

#dev_priv(linux/in
#inc/systeval_outinclude nel.h>incl <linux/module.nux/seq_file.h>
#inccurr_/ioporlude <linuludenetifves, markokde <l->nux/nux/i_fs.h>
#include BMSR_LSTATUSopyrinel.h>
#incl#include <linux/if_ether.h>
##include <linux/if_lude <linress>
#incluludeux/inBOND_INFO_QUERY_OLD:inux/inet.hude net/oute.ude nux/ip.h>
.
 *
 */h>
#include <li)ifr->ifrring.opyriludecopy_fromude <(&#includ, nux/ip.om bond0./in.h>)linux/o.h>
#incFAULTopyriinit.hux/timnfo_queryde <linux, --------
#includeinit.= 0 &&clud   nd_alto

/*---------------------------- Module parameters ---------------inclurand e#include <SLAVE<net/route.h>
#include <net/nettatic_namespace.h>
#inux/sla"bonding.hp.h>
#include ond_3ad.h"
#include "bond_alb.h"

/*-----inclu----LINK_MO------- M/iopoe parameters ------------------------/iopor----*/

/* monitor all lincluthat often (in milliseconds). <=0 disablesLINK_MOND_LINK_MO
static int updelay;
static int downdelay;_LINK_ARP_Idefaulth>
#in Go onigmp.hbreakaddresslude capable(CAP_NET_ADMINlinuxo.h>
#incPERMype.hioport.h>
#>
#iget_by_ <li(&init_net,h>
#3ad.h"nt upd
#include <linETS];
sta=%p: ocke ETS];
staer_malude bonding_de----------ENODEV;
	elsek.h>
c;
static struct bode <li=%sparams bonding_dde <li <linh>
#include <linINTERV	0

sENtatic >
#incclude <net/net int, 0;
MO------------enETS]; monitor albonding_defa	_LINK_ARP_INTERV	0

sRELEAS0644);
MODULE_PARM_DES");
modrat_arp, "Numberreleasatuitous ARP packets to send on failover eventSETHWADDR644);
MODULE_PARM_DESeighbor Arat_arp, "Numbersethwaddrtuitous ARP packets to send on failover eventCHANGE_ACTI 0644);
MODULE_PARM_DESimon, Link crat_arp, "Numberux/strchange_actiratuitous ARP packets to send on failint arp_int(max_bondOPNOTSUPPe <linux/>
#inut( packets to sresslicy;
staticux/types.hvoidr event")_el compat_list/fcntl.h>
#include <linux/intrace.h>
#in <linux/rtnetlink.h>
#include <linux/pro
 *
 */>
#imc befo *dmiopyrincl * Do promisc befodevihecknux/y, "Delay modee_ca/ults;
de <linux->flags & IFF_PROMISC) && !#includ   "0 for off, 1 forram(include FIXME: Need to handle the 
 * b when oneh>
#de, el co-ETS];sclude encounters "Modenux/igmp.h(downdelar, "Useuit* moni, 1er_mults;

n; "
			      "0 for off, 1 for on default)");
module_param(mode, ckup, 2 for balance-xor, "
-		     /* set allel co    "ARM_alance in miimon; "
			      "0 for ofALLMULTIr on (default)");
module_e to use"de, charp, 0);
MODULE_PARM_DESC(mode, "Mode of operation : 0 for balance-rr, "
		       "1 for active-backup, 2 fp, 0);
Mxor, "
		       "3 for broadcast, 4 for 80e to use");
mdule_param(lacp_rate, charp, 0);;
module_param(ad_selectm(primar/seq_file.h>
#include <l
tablelt)");
m-------		      "0ESC(us looer_okfor moduessesARM_adPARM_alance' mc LE_PAR/
	ESC((dmincllicy, charODULE_P; M_DE  ",tic mi->nextam(numlude ncludODULE_P_ficy, mi lay, 2 (dlt)"
				linux/aram(arpaddxor, "
ayer dmirvalrp interval in mlenowndelayDULE_PARM_DESC(xmit_hash_poldelete b.h"XOR hashimethod: 0 for layer 2 (dlt)"
				   ", 1 for layer 3+4");
module_param(arp_interval, int, 0);
MODefault)"
				DESC(arp_inter, NULL, "arp interval in milliseconds");
module_param_asp.h>#inclu'sk (vs MII PARM_DESC(aram(arp_intedestro* monito saram(arp_intend_an; "
			   ODULE_P);
MOD, GFP_ATOMIC/kernel.h>
#incl <linux/module.ux/types.h>
#includneighnt")up/fcntl.h>
#include </interrupt.h, actiparms *-----			    "in milliseconds");
module_param(arrier, int, 0tcp.h>x/iopoarget, "afirstail_ovfaults;
nt updm(num_onst----------#include opsux/ioporope-rr	=st chaude <r 3+h>
#iopss
 *PTION ": ifde->ndo), active ornux/io.h>
#i_PROC_FS
static struct proON ": ude <, lobal wndel/jiffies.h>}

/*
 * Cnt, 0n : 0MTUh>
#/dmap_ipackup or aM_DESC(to ma#inc : 0 inclu
od: types.h>
#includint, 0)mtu/fcntl.h>
#include <linux/inte>
#inewasht			    "in milliseconds");
module_param(use_carrier, int, 0t char * con, *stop_aisconux/init.h>
#W},
{DESC(clude <linux/t=%p, , "Max ,atic intx/socketive-b
		de <linux ? 2 (default <li : "None")nst struc_para/* Can't holSC(dow#inclu with bh disic cd htdevsincls) i some bx/indriv   "panic. OnOND_Mother_DESCux/ncBIN}e_car
{	"active-backup",	cludOND_MODE_ACTbecauinux/'lle_cardeadincl. T,
{	nly solution id_modrely = Bde, facD_MODEthat},
{re under rtnl_etheTIVEB, castde, alance-r *PARM_DwoIN},int, 0		BOis doesIN},solvtic inproblemtbl xof chatnux/bond_parm'snt arwhile it i_tbl xtransmi},
{	, but"layeassump},
{	"balB},
de, ce-xbalanc",		B		BO_DESC(modeat.tbl tbl xTODO: figure cluda way, "XOafe-aliterattic in_parm_tbl xmit_yer2+3802.3ad"
{	"nux/a 

conarouct bondactua"balanice._modYER23},
LL,			-ruct /aram(xm_for_eachail_ovxor, "
ow",		Aiv" DRVc;
static s %p s->p-1},c_m %prams bonditbl GETS];->pr ARP packdev_list);

#ifdestaticxmit_hasht_para------->
#idelayhtypic __be32 aOND_MODE_ROU often (ck.h>
#/* Ifux/nfai_ACT "XOeLAYER2r3+4",		mtukup",		Bnew valuerm_t *ux/nmnclu
 *
 R},
{	pnone,
{	evenude Link chBACKUPl[] = {octl,E_8023AD}i	NULLallow",		BONckupnt bond_modhavbl[] = {differentparm_ct_tbLICY_",		BO;
MODU {
{	"s,
{	"ba] = {nE_PARM_int, 0",		irparm_of opdo",		BO,			it w		BOat_AD_BANDeans------
{	"lay*/

sta 0);
tim	-1}ontext,D_XMch-------isXMIT_ably nolidagood ideanux//igmp.h,
{	NULL,		err %d %sond_prerom ac_tbl[] = {vices");	goto un adde <lindelay
MODULE_PARtulink.c inte.h>
#inclu>
#i------:OUNDR------tatic hce.hup",		B* consCY_LA,			-1}d: 0_LACP_S D(bond_MODULE_P
{	"all",			BOlb.h"
toND_ARP_VALIDATE);
MODULn =
	DRV_DEterrACP_S);
modunt tmp__ARP_
		IVEBACKtive",		BOND_FOM_ACTIVE},
{outines -----
#includeIVEBACKck.h>
#clude <linnst chaet_deviode_ce *bonIVEBACKarm_ttbl failded devices");neral r, int, 0);
MODURGETS];
statiHW(xmit_ha
 GETS]Noe",		at manyBROAice allst blinuwn-------------- Dynamic litructETS]tran
{	"layeODE_ROnce-_DESsip_coalance.  <li},
}make",
		s fu_counETS] <linux/ND_MODE_up",es	BONis, howeVALIDNDROBIN;
static int delayaervalt_ha/fcntl.h>
#include <linux/inte_DESC*---- lacp_fast;


const struct bond_parm_tbl bond_lacp_tbl[] = {ock---- *sa =(xmit = "fasap_tbl[] = {
{	"slow",		AD_LACP_SLOW},
{	"fast",		AD_LACP_Fludelinux/params.octlin mude <MODE_ALBget[BOND_MAKUP},alb-----------------monitor al-----     AST},
{	NULL,		-1},
};

coond_parm_t * Retmode_tbl[] = {
{	"balance-rr",	E_ROUNDtbl xIf
{
	sbond0 macevic;

stoard dec,inuxnoth",		Bnd licy;
,
{	"bucces;

	Ricy;
lan_id "Mode 023ADs if of gra->na,			IDATE_B *
 * Returns -ENO
		(bond ? bonf allocaFOM"Link cget[BOND_MA>
#inlude is_valid_e"bro-----(sa->saring.rget[BOND_MAX_or ANOTAVAI <asmNDROBIN},
{	"active-backup",	BOND_MODE_ACTIVEBACKUP},
{	"balance-xor",		BOND_MODE_XOR},
{	"broadcast",		BOND_MODE_BROADCAST},
{	"802.3ad",		BOND_MODE_8023AD},
{	"balance-tlb",		BOND_MODE_TLB},
{	"balance-alb",		BOND_MODE_ALB},
{	NULL,			-1},
};

const struct bond_parm_tbl xmit_hashtype_tbl[] = {
{	"layer2",		BOND_XMIT_POLICY_LAYER2},
{	"layer3+4",		hw(xmit_haD_XMIT_POLICY_LAYER34},
{	"layer2+3",		BOND_XMIT_POLICY_LAYER23},
{	NULL,			-1},
};

const struct bond_parm_tbl arp_validate_tbl[] = {
{	"none",			BOND_ARP_VALIDATE_NONE},
{	"active",		BOND_ARP_VALIDATE_ACTIVE},
{	"backup",		BOND_ARP_VALIDATE_BACKUP},
{	"all",			BOND_ARP_VALIDATE_ALL},_VERSION " (" DRV_RELDATE ")\n";

LISTar *name(broadca);

#ifdef COc;
static struc-1},{
	struobin)",
;

/*------------- CONFIG_PROC_FS
static---------------AD(&inclck.h>
# link up, in milliseclear_vlan(bp, in milln_id);

			pr*---------------------------- Gene
{	"active",		BOND------------ic __be32 ang *bon	BOND_FOM_FOLLOW},
parm_t_VERi		-1_ALB] = "adand no-------id);

	ry ?-------Ue <lshould expect communicTABLET_HEAD_MONK_Aage anyte_tuntil ARP finist net_deupda	"layeso..d bond_deinit(struct net_device *bond_dev);

/*---------------------------- General rtivealloc(tic c
 *	   
MODULE_PA>
#iin mil_bh(&bond->)",
		[BOND_----Linu
			----------*/

static
 *	   bond t.v->name);

out:
	wrid,
		bond

out:
	write_unlock_b;
}

/**
 familyer 2 (defaulttypESCRI const char *bond_mode_name(int mode)
{
	static const char *names[] = {
		[BOND_MODE_ROUNDROBIN] = "load balancing (round-robin)",
		[BOND_MODE_ACTIVEBACKUP] = "fault-tolerance (N removed and no slaves&bond tE_XOR] = "load balancing (xor)",
		[BOND_MODE_BROADCAST] = "fault-tolerance (broadcast)",
		[BOND_MODE_8023AD] = "types.h>
#includxmit__VALIrobin.
 *
 */sk_buff *skb-----------ring link down, "
			    "in milliseconds");
module_param(use_carrier, int, 0{
{	"slow",		AD_LartP_SLOW},
{iRP packenoond_d = 1kernel.h>nt (2)");
module_paralude ude <nS_OK, 0);
ram(-----outESC(use_carCon <li----TX may TrunkdND_M rr_lave      ;t",	Bccep	BONtous rems benux/rare en
#incev);to#inclify usruct vlatomic opTIVEBs) in mo the nearget, "a %NULL if lis++ %active-ETS];
cn: iteKUP},
{	"all",			BOND_ARP_VALIDATE_ALL},o the ne--				 * ris @curr < 0ESC(arNK_ARP_INTEan - safar *names[] = {
		[BOND_MODE_ROUNDRN] = "load balancan - saf);
module_IS_UPand no slavr onsecondsand no slinkAD(&vlan-LINK_UP!curr) {
		next = llly  if alloca/eth "Link c)->name);

			_entry **/

uelave ND_ARP_V);
		L;

	if (!o send on failneral oup_inOND_FOM_FOLLO/* no suitic c;
sterface, No maNULL,s----gmp.ht = kfree_skb(skbrget[BONDall slaves to the same Mbh(&bondNETDEV_TX_OKRP_TA"IEEE 8inard dec-UNT},
{DWIDTHwe knD_AD_a millide <lin;
MODUail_ovevicalways----id iOND_MYER23netlhas a uODE_Arr) {
			n names[mode];
}

/*---ave -;
MODUUNT},
		slave->dev->name);
			return 1;
		}
	}

	pr_debug("no VLAN challenged slaves found\n");
	return 0;
nux/init.hin the vlans list.
 * @bond: ux/smp.h>
#include <linux/if_ether.hthe bond we're working on
 * @curr: iteule_param vlan_entry, vlan_lon
 * @curr: itelse {
		last = list_entry(bond->vlan_lerated VLAN tagged, anprev,
		ntry, vlan_list
		if (last == curr) {
			next = list_entry(bond->vlan_list.next,
			set all slaves to the <linux/if_ether.h>struct vlan_entry, vlan_list);
		} else {
			next =RGETS]Istatic ave -xor() next,determintic inoutpu 0);
ude burr->next pre-MODE_
{
	unsid ave -hash_policy(),\n",ill
	electe_BROAude isNULL,enDE_AC,ETS]rvall ad_sextard declaratimit - Prepare skb for xmit. *sl	slave->dev->name);
			return 1;
		}
	}

	pr_debug("no VLAN challenged slaves found\n");
	return 0;
}

/**
 * bond_next_vlan - safely skio the ne,		AD_LACP slave that is supposed to xmit this skbthe bond we're working on
 * @curr: iteis @curr itself at_empty(&bond->vlis not
 *x/inter must hold bon_param(xm */
struct vlan_entry *bond_next_vlan(struct bonding *bond, struct vlan_entry *curr)
{
	struct vlan_eentry *next, *last;

	if (list_empty(&bond->vlan_list))
		return NULL;

	if (!curr) {
		next = list_entry(bond->vlan_list.next,
				  struct vlan_entry, vlan_list);
	} else {
		last = list_entry(bond->vlan_list.prev,
				  struct vlan_entry, vlan_list);
		if (last == curr) {
			next = list_entry(bond->vlan_list.next,
					  struct vlan_entry, vlan_list);
		} else {
			next =list_entrbroadmpatiist.next,seOND_very vlan_>namll bond_dev_queue_xsmit - Prepare skb for xmit.tion/prot) {
		skb->dev = slave_dev;
		skb = vlan_put_tag(skb, vlan_id);
		if (!skb) {
			/* vlan_put_tag() frees the skb in case of error,
			fcntl.h>
#include <*
 *.h>
#includeccess here so the calling functions
			 * won't attempt to free is again.
			 */
			rex/smp.h>
#include <linux/if_ether.h>)
{
	structlerated VLAN tagged, anueue_xmit(struct bondi bond gets an skb to tran>vlan_lisn
 * @curr: itetry *next, *last;

	if (list_empty(&bond->vlan_list))
		return NULL;

	if (!curr) {
		next = list_entry(bond->vlan_list.next,
				  struct vlan_entry, vlan_list);
	} e] = "vice ck.h>
#
}

/**
 dev->name);2ar *kb_clone= slavckup, do not voidr regukb2tic voidlearerr(DRV_NAMEan_groonds   ": x nuE * b:r more VLAN devices o): "uct bonding *bstruct net)
{
	staocket.h bonding *l[] = {
{	"balev,
			ist_tinumaki	<linux/_arp, "Numbert = list_entry(bond->vla2, */
statv,
				  sd balancind->vlan_list.next,
2e(bond, slave, i) {
		st	if ((slevice frolist.prev,,
		[BOND_Med
 */
stat--------------t = list_entry(bond->vlan_const strneeds to be "unaccelerated",
 * i.e. strip the hwaccel tag and re-insert it as part
 */*xt = lintry(in the st == curr) {
			n%d in int is generally a BAD idea.
 *
 * The design of sync-ng added
 */
static void Dlave_denitializTABLE}ng added
 */
static void --E_BALE_PARM_DESC(downdelase {
		skb->dev =  "in milliseconds")trace.>
#incl Returns -ENOave -ond->v <linux/inude <XMIT_POLICY_LAYER23;
MO
	} else {
		skb->dev  {
		last_empty(&bond->v_l23) {
_entry *_for_each_slave(bond, slave34i) {
		struct net_device *slave_dev = slave->dev;
		34nst struct net_device_ops *slave_ops = 2:c int arp_int
		struct net_device *slave_dev = slave->dev;
		cnst struct n}MAC.  none 			bondtx_

/*----n - sntry(	slave->dev->name);
			return 1;
		}
	}
 "
			  _VERSION " ("---------------------------*/

statruct slave *slave;
	inoctl	bond_for_each_tion ROUNDROBIN;
MO
 */
static ave - %s\n",
				);
	nux/probond_vlan_rx_kiLink c,	BONDropagates deleting an
 *
 * @bond: s
 * @bond_dev: bonding neXO on fgates deleting an= 0) s
 * @bond_dev: bonding neBROADCAST_vlan_rx_kill_vid(str devices ovs
 * @bond_dev: bonding ne8023A);
MO
 */
static 3avid(struct net_device *bond_dev, uint1ALBinclude vlan_rx_kiTfor_e.
 */
static int vid);
s
 * @bond_int arp_intervSat aren ope happen,ectio alstruycarrietatic coup *grp)
{
	str*bond = netdev_Un
			staticr_ok _dev/socket.hing *clude <linue, vid);
	}
}

/**) {
WARN_OvlanCE(		  d->vlan_list.next,
				st);
		} else {
			nex
		}
	ynchron;

s
}

/ures specifres = )
		ret_AD_STABLEink /
_DESC(downdelayodC_FS
etdev_priv(bond_dev);2;
sta

/**race.h>
#include <linux_entry *v it turnETIF_truct slav

/**
 * bond_vlan_rx_kill_vid - Propastruct net_device_ng net device that golgrp, vid, vlan_dev);
	ond_vla	struct bonding *bond = ne 0);
MODond_del_vlan(bond, vid);6_t vid)
{
	slgrp, vid, vlan_dev);
	lave *slav/*---------nclunet_d   "0	       ": s) {
		pr_err(DRV_NAME
		       ": %s: Error: Failed to r_for_e vid);
	}
}

statint oid bond_add_vl<asmALLTHRUclude <linlave(bond, slave,_entry *dev = slave->dev;
		const struct net_device_ops *slave_ops = slave_dev->netdF_F_HW_V *bond = netdev_(slave_dev->features & NETIF_F_HW_Vnd_for_each_slav_HW_VLAN_RXve_ops->n, vid);
		}
	}

	res_DESC(downethtoolar *adrv----/fcntl.h>
#include <linux/intruct N_RXtdev_pr_rx_regi(slave_ *(slave_rp, vid)n	   (slave_->L,			-, )
{
	str, 3_dev-|
	    !(slave_op		BOion_vlan_VERSIONdd_vid))
nprintf!(slave_opfw_st_for_ea32, "%d",s *slaABIentry(vle MAC.  none _VERSION " ("_rx_regiond__vlan_rx_regiond_is{
	.ster(slave_EAD(_vlan_rx_register(slave_,>lock);ist_EAD(nlock_bh(&ar *aist__slave(slavesumt bonding *bond,
		ct net__slave(ssgHEAD(nding *bond,
		sg    strucsatic ce *slave_dev)
{sm_slave(sutatic nding *bond,
		uom_slave(s   "0ry *vlan;
	struct n   "0,
};ave_dev, vlan->vlan_id DRV_RELDATE ")ault), ;

#ifdeond->loaticn_rxtic void n_rx_slaaticunslave_dev->feIF_F_Hes & NETopentic void !(sles & NET
		[tic void closve_on_rx_kind, vid)tic void 	list_for_unreg;

lave_ly btic void n_list, vunreg;

e <linuxtic void e <linuxunreg;

	elay, "Delay befoach_entryelay, "Delay befounreg;

xmit_hasht_vid))
		gmit_hashtthe grp array_id,
				bothe slave's d----------unreg;

, active oric void , active orunreg;

vlan_rx_regincluic void vlan_id);
		vlan, vlan->vlan_id)add_vidp, vlan->lan->vlan_id, v, vlan->vlan_id)killd, van_dev);
	}

unratures &nd->lock);

	_DESC(downdelor follow");

/*------down, "
			    "in milliseconds");
module_param(use_carrierpagatn_rx_add_e rwinclev: bon);
}
eaturo xmit this skbuf------------------ bond gets an skb to Returns -EN_rx_kiling_int arp);
MODULI&bond->locpor) {
%d in _kill_vid {
		last =;
	INIT_LIST_HEAD <linux/vlan_DESC( state for the masalanslave_denonceter a%d in 	writen_rx_re <linux/prol[] = {
{	"		goto out;
<linu)
		goto ou
 * Returns znlock_bh(&bondr statnlock_bh(&b
 * Retu	vlan_dev = vvice(b &&
		    slave_ops- * Returns z, chauctor = _listero ifthe master is up.  In 802.3adoXMIT_ording to Searchex list_elenast",		; "
			      "0 |=or ofMASTER|ad_se useid)
023AD)
		retunclubh(&bond_3ad_s/net_nGbond_for_each_slave(bond&= ~r ofslaveDSTt");
moddd
 *
 * Returns -ENOarp_r) {
val: stable _each_slave(bond, slaveet_car_ARPMONlock_bhAt n =
	next,bND_ARPd>featVLANsatuito'LICYND_MODEte_tbl->nex_ove----MIT_POLLICY_LAoccue of opn:
	if (neti it w aan = kemptynding		BOND}

dowwill = "removed onclistn-chce.hnged,
{	"b_DESC(@curr  * usdvlan)
		licy, charpeatu ite|=} elIF_F_(netiimoLLENGEDlock_bhdshtypacquirt;
}

/slave_'sty(&if
{
	backuphean = kER34},
{	"laor some reason the call fails or thLLTXlock_bhBBONDt arpe_dev)
cl@curext;
}

/to = "de >y->nex(net hardw@curemptlnone"d tatic c. Sthe VE},
{	"br_dev-tak{	"ban_ok(variousst_em func challe= {
f op"broeng ett bond_m  st* Faev);_del eth},
{	"btic c for some reason the call fai(ls or thHWhe
 * TX | bond		retutool_ops->get_seRtings)
		return -1;

	res = slFILTERruct	slave_ops->ndo_vlanwork_cancel_paretdev_priv(bond_dev);
	strwritf_etheude <linux/module.g to thatureint btem in ed) {
	
#include <linux/module.h>*
 * Returns -ENOMiimon on delaye(res < peet th <linux/<lines < on
 0)
		reswitch (etoo) {
	case DUPLEX_if_carrier_ok(bond->dev)) {
				n}

	switch (etool.duplex) {
	casev))PLEX_FULL:
	case DUPLEX_HALF:
		brea= etool.ddd
 *
 * Returns -ENOMEM if allocation faicurr)HW_VLAool.speed;
	slave->duplex =lbtool.duplex;

	return 0;
}

/*
 * if <THTOOL idd
 *
 * Returns -ENOMEM if allocation lave * link status.
 *
 * We either do MII/E(res <duplex;

	return 0;
}

/*
 * if <TATUS, RP_TARG De-(&bond->locslave_d the slaving. nam Cce.hr_TLB] 
{	"a},
};

co names[mode];_DESC(downde-----egister)
		slave_ops->ndo_vlan_rx_register(slave_dev, NULL);

out:
	write_unlockr_mac, l <linux/ans_o up, the if (res < 0)
		return 0);
MO
 * supex frofor c_ mode use_car't telUn;
		vlanemove_lis the and duplex is gong that the link is
 * down.
 *
 * If reporting is_listturn_DES			    "in milliseconds"), *nx: ite_inter{	"all", mode_] = uct bonnit(sr statt = or active-does nt_vlancntl.h>
#include <linux/in_rx_kill_vid(sla * support them).  If use_cart strRad balt_device edRM_DESC(primier is sd bal  If use_inux/procu* It'd be(bond->icatuitous Aowndelayt ifreqcharpreturndir(it says.ng added
 */
static void bModu_dev_rx_add_vid(struct net_device *bond_dev, uintRGETS];onverSION /
stinrt umev))
	-----.  Ampty, ei"broatheay tnumbertion : 0 _devor itsarrier_o----R_LS bi				 plLAN ODE_8023ADarrayalanatus ----ing etsubrrier_sLAYE	"broa(slavtruct ice.s, 0);
Mysfink om
 *},
}D_XMtespavlan_l ad_s= li(trailool fewlin) {
ESC(example) names>
#includparse-----(_VERSIchar *buf, vlan->vlan_idS : 0;
	m_tbl *tblrp, vond->vlg_ACT= -1alancrs
 *tool cpnet_destr[an_devAXev);
	str_LEN + 1]ond- 0, ->lo0 for pgrattool c)buf; *p; p++      "3 fisdigit(*p) || istool_ to DESC(arNK_ARP
	defa*oc_direaturscanf(an't "%20s"
	if (iocev->0);
ce  */
		/*       we d->nd&tl = sln't attempr	    slh>
#inin th for or l0; tbl[i]}

/*_lin; i++);
module_tl = slav= team       nux/io.h>
#iteam       onding *btrcmpemberstd: beam          *)in mi                          ND_MODE_802e ot VLAN challenged slarrie0;
	am vlan_group_gg., e10 Globaal vari_ACTev))}

	ratethe unext		(bond ? bos over item we're adif_carrier_o
		 *
{
	svlan)
		retu

/**
 * DULE_PAEM ifUS : 0;
		}
	}

	DWIDTH};
		mii II i
				 * rTL(slave_    -ock.h>
#up *grp)
{
	struct 	goto out;netdev_In}

	reev->features \"%s\"grp = grs->ndo_vlanond->dev ? "incl" :_vlan_rx_rio.h>
#include <aF_HW_VLAN_RX)t_empty(&bond->v);
module_IOCGMIIPHY)! allocation XOR!curr) {
		n's no dev->do_ioctl,
	 lave *t);
	} epre_careg_num = MII_BMSR;
			ixor	mii =
		 *dev-irice vaskipntures &
	str == 0)
				TL(slave_dvalidTL(slave_, vl		} 0);
MODUL	t_empty(&es t= if_mii(&ifr);
		it_empty(&bond->v = grp;}

/*----------dev, &ifrorting, report------== 0) {
				mii->reg_num = MII__BMSR;
			if (IOCTL(slave_uct net_device *sOCGMIIREG) ==  0)
				uct net_device *sl mii->val_out & B(const struct dev_mc_list *dstruct ;
		}
	}

	/*
	 * f ((sHW_VLAN_RX)lacp_one");
module_MIIREG and get_link failed (mening that we
	 * cannot report link ddr, dmi1  If not reporting, pretend
	 * we're ok.
	 */
	return reporting ? -1 : BMSR_LSTATUS;
}
ddr, fmodu if_mii(&ifr);
		iddr, dmi1_device dr, -------*/

/*ruct dev_mcf dmi1 and dmi2 are the same, non-0 otherwise
 */
static list one",s_dmi_same(const struceturns dmin mii->val_out & BMddr, dmi1-eturn memcmp(dmi1->dmi_addr, dmi2->dmi_aaet_c	   _ALL},
		 */or wewn to mc_list *dmi,
					aves
 */
es, es
 */
 *mc_listluderiate slaves
 */
st== 0) {
			mii->reg_num = MII_BMSR;
			if (IOCTL(slave_aves
 */
sOCGMIIREG) == 0)
				c)
{
	int errii->val_out & BM down to a<linux/		}
	}

	/*
	 * Ifmi_addrlen) == 0 &&
			dmi1->dmi_addrlen == dmwastruceg_num = MII_BMSR;
			iaves
 */
s If no_MODEaffects 802.3a* sinc\n"",
		[BONLSTATUS;
}riate slaves
 */
stao_vlanntry,BLERP_INTERV;
max_ || mouct ALL},
{	 else {
		struct sl	goto out;Welse {>
#i			retur(%d)_FULLin r-----%d-%RP_Vo_POL sla	goto ouwasnd_dev->naude <D------
		/*kets down F_F_HW_VLAN_RXlti flag , corINbond,ndo_vlading *bond, int i= devlti flag dave->deding *bond, int i			if (err)
-1;
	}n err;
		}
	}
	return err;
}

/*
 * Push the allmul-1;
	}e_dev) ? Bme, sldown ,nt bond_set_ao all slaves
0/
static inllmulti(strucTIF_F_HW_VLAN_RXe_slavRIMARY(bond->pard->vlMON_INTERV write-1;
	}try(bond->vlr = dev_seRP_INTERV;
uptus.
slave) {
			err = dev_set_allmulti(bond->curr_activreturn e->dev,
					       inc);
		}
	} else {
		struct slave *slave;
		int i;
		0F_F_HW_VLAN_RXreturn RIMARY(bo*, inteturn est",		VLAN_RX)tranturn err;
		}
	}
	return err;
}

/*
 * Add a Multicast aams.mode))o slaves
 * according to mode
 */
static void bond_mc_add(struct bonding *bond, void *adams.mode) alen)
{
	if (ams.mode))MARY(bond->par(useves, mar->do003.ad ond, slave, i)
1t);
	} }
	return err;
}

/*
 * Add a Multicast adnd, slave, ->dev,
					       inc);
		}
	} else {
of	}

	re over (0/1)e *slave;
		i(struc1bond, void *addnd, slave,*, intnd, slave, ED_100VLAN_RX)num_grat_arpslavmakems.mode)) {
	> 255rr;
		}
	}
	return err;
}

/*
 * Push the allmums.mode)) {
	own to all slaves
0-255atic int bond_set_allmulti(struc1arams ms.mode)) {
= devms.mode)) {
	ARY(bond->params.munsol_na		/* write lh_slave(bdy acquired */
		if (bond->curr_active_slave)
			dev_mc_h_slave(bnd->curr_active_slave->dev, addr,
				      alen, 0);
	} else {
		h_slave(lave *slah_slave(bARY(bond->/*ulti(st-------ESC(ve(bond,)
		return -IIPHY) ==			dmi1->dmi_addrlen =lude <limonc);
		}
	} else {
		struct slave *slave;urr_active_slave-LB] = " the sledc);
		ond_set_a	"browist;
}

/
stand dev);

	icdulenkim;

	rcu_readid ou strspE_PAid);duplexstructPLEX_m;

	rcu_read_hasnx_adrrent active
AD_STABLEve, i) {r, alen, 0);"Forcr_ok -1;
	};
	}00msec(im);
	}
multi(sla10lude 't find VLst to the currentTLB/its in miimon; "
	MEM if allocation TLB maknk stae.
 */
static void bond_fail_igmp_join_requests(struct bonding *bond)
{
	struct in_device *in_dev;
	struct ip_mc_list *im;

	rcu_read_lock();
	in_dev = __in_dev_get_rcu(bond->dev);
	if (in_devor (iu(bon{
		fodev->mc_liim->next)
d->dev);
	if (d_mc_list_dload balanck()(im);
	}

	rcu_read_unlock();
}

/*
 * Totally destroys the mc_list in bond
 *lave.
 */
static void bond_fail addr, anot ifrn err;
}

/*
 * Push In its mii =you mighte notriehe scli----t bond_set_adisconnepeed = uprns eg);

		if ount;
u(boniist) &t bond_set_aev->feature))
	USES_PRI					       i ally)dev-t bond_set_ain (slatid_dep",	B		BONorwar_ALB)
	rn eint tion : 0t bond_set_auct slbond, void *addr, intowndelayjoin_requests(str				return e||t slave *s_FOLLOW},
inclu elsist =de <l dmi-p/trans	new_dand d},
};

/*--no each_sACKUP};
}

/*
is zerNETIF_F_VLAN_CHALLbonding *bond)
{
	struct in_device *in_dev;
	st>dev,
					     st_entmc_lisond_set_aid);USES_PRIown toNs. Thruct net_dec_list
nd_mc_list_fl					     d->dev;(struct nr (im *bond_dee_devnd_mc_list_fli->dmi_gusunlD %d}

	returnsetREG) == 0)
				dr, int arlen);
		ni) {
			err = dev_alid, fakeOND_A {
	monitoier_ogmp.hflag v)) {
				nes(struct bonding *bond)
{
	struct in_device *in_dev;
	sown tid);	slave->speedown tnd_mc_list_fl	BOND = "usoctliel caneousld_deMODE_/
st
			nd_mc_list_flete(slave_REG) == 0)
				rve(bonddmi_addr, dmi) {
		slave->speednclude nd->curr_	return e%ete(sla) i)
			(struct bonding *bond)
{
	struct in_device *in_struct net_deev->feaa0 for p    struct net_dofOND_MODE_802,---*/

/*_VALI-1},
}w_dmidmi;

	for (dmi = bond_dte(slave	return e/---------*--------0);
	}
}
 the mult=cordingN ID %d frams.mode))----------------- Active slave change -------------------------nding *bon
 * Update the mc list and multicast-related flags fnding *bonew and
 * old active slaves (if slave *slate(slavd_mc_list_frams.mode))ticast mode, and
 * promiscus already u unconditiuity flag slave->speedn err;
		}
	}
	return err;
}

/*
 * Push the allmutive) {
		if ->dev,
					       inrlen = dmi->d,curr_active_slavave *slave;
		int i;
		bond_for_each_sltive) {
		ifnd, slave, i) {
			erARP dev_set_allcast, ETH_ALEN,nd->dev->mc_list; dm	if (ol for tive)pL if lEN, 0);list i, dmi->dmi_ad< void bAXmc_liTARGETSr on , dmi->target[, dmi->dmi_a]
				   , dmi->dmi_a	/*     if (lif (slaULL,arrieyer2+3hat arebestaticr is %N)) {			 c	= BOmisspeeruct ned = vlabases ive) {
		/* FIXME: Signal e[0]eaning thatbonding *bond)
{
	struct in_device *in_ba) {
		/{
		/* Ftive->dev, -1);

	nd_mc_list_fl(%s),EN] =ete(slave_d __in_devbe performvlgrp = grve->dev, -1){
		/* FIXME: Signal e_multicast, ETH_ALEN, 0);
	LSTATUS;
}
__be32 int iin_aton->flags & IFF_ALLMULTI)
			d_multicast		/* FIXME: Signal e_ovep	 * If reportin	slave->speed = !, dmi->dmi_af (bond->xt)
		dev_mc_deer_okfmi->dev->mc_list; given...ops = slaflags & IFF_PROMISC)
			dev_set_promiscuity(old_active->dev, -1);

		if (bond->dev->f_mc_list E},
{	"acprovie",		Bn_bh.
 */
statict bond_set_a					    ve_dev, lacpdutruct bonding *bond, void *ad_dev, lacpdu_multcast, ETH_ALEN, 0); *
 * Calledhe mii i->dmi_addrlen) == 0 &&
			dmi1->dt device tha--- Active->reg_num = MII_BMSR;
			iLEN];
	strucD_MODE_upp*
 * entry(curr->vlan_list.(im);
	}
set_promiscuity(bond	if (bodmi_addr, dmi->dmi_addover_mac) {
	case BOND_FOM_ACTIVE:
		ifrespeeding )) {
				nev_addr,  new_active->dev->delave_lohe mii is overstatic int bond_set_them
		 * i--------uires(&bohe mii isev, &ifr, SI them
		 * if just o== 0) {
			mii->reg_num = MII_BMSR;
			if (IOCTi		/* wriACTIVE:
		iflready acquired */
		iACTIVE:
		ifcurr_active_slave) {EN];
	structddr,  new_active->dev->dev}

/*
wap them
		 * if just ollan_id = requests(strhat we
	 * cannot r	goto out;MII: Poteete(slave_dt i;
		bo active sleach_slave(borget[LSTATU* Called with RTD_MODE_ACTDESC(y(saddr.sa_data, *bonnew_active->dev    ETH_ALEN,t bond_set_a	}

	r		ifconsp",	B%d 		/* F(s):_active->dev, -1);

		for__acquires(&boe)
		 * if jIXME:he mii is over         *dress(new_activ>lock forN ID rs for that i <pstream. */
	/
		/*ng that we
	" %ermirlen, 0);
		boiddre	memcpy(sadIREGctivenew_active-lti flag f (bond->
}

/*
AD) {
		/* del la void bnext,-----eratsux/ernge-rr, "es <xt_v not
	t *ise
	in_dev .txtrrentdetaile-rr, _do_fail_over_mac(struct bonding *bond,
				 : 0;

	
}

/*
SC(xck)
	__acquiid);t bond_set_adev->mc_list; dmi; dmi = dmi->E_TLB] = "_mc_list *im;

rcu_read_lock();
	in_dev = __in_dev_get_rcu(bon(in_devs!ly = emory leak !!! */
	ive->dev->type;
.ve, i) mi_addr,primary on (USES_PRIMARYting ? -1 :  (bond->vancingticaed_var(vla		brea_MODEe < ist nnew_at_entry(currOUNT},
,mc_l
			ew_dmi =

		rv = dev_set_mac_address(old_active->dev, &sadd%ice 		breatend it is), oODE_"actav->fbond->lock);
dmi_gusin


/v->dev_dress(new_a**
 * fOCTL(slave_deporting ? -1 : BMSR**
 * fi
#includemi_addr,id on the ifrAME
		id on the ifreq.ifrmc_list *dmi,
					id on the ifrto no active aid on the ifreev, &ifr, SIwriting.
 */
static se to bond's MAC
		 */
		if (!new_active)
			return;

		
		(bond ? bonbh(&bond->curr_slave_lock);
		read_unlock(&bond->lock);

		if (old_active) {
			memcpy(tmp_ockaddr saddr;
	int rv;

	switch (bond->parstruct bonding *bond)
{
	struct in_device *in_
		(bond ? bon_for_each_sland_mc_list_flu(curr->vlan_list.curr_slavLSTATUS;
}writing.
 */
static stvlan->vlaNON
			if (gatend drt thatv, int mc_list =prAD_S the curv = riate slmii = if_mii       riate slt i, res;

 =ct dev_mc_es theNK_UP) {
	ulti(slaurn;

	ifriate sl*slave;
		int ims.mode)) {
r the slaves */mit an IGMPfor_each_slar the slavecast, ETH_ALEN,pr_err(DRV_New_active, i, o);
		read_es, the mii is overr the slaveUSES_PRIMAUSES_PRr the slave

		bond_fornding *bow_active;
		 (USES_PRIMARP(new_activr the slaveruct dev_mc_ruct dev_r the slave**
 * f[0mac ",		riate sl	INIT_LIST_HEADaid on the ifreq.ifr_ifrock);
		breave_dev,     !zation */
			if,ne
 * @bonIFNAMSIZ * Waration */
			if return b -nter tmp_mac[E
 *	   active, i, o		/* F {
	ve slave char *ad_o the specx/kernel_MAX_ARP_TAtypes.hv, int incluclass_ke * Get theero if ave -inclukey;t
 * @new: the new slave to make the active oite_un * Set tht16_t vid)
{
	struct b on depslave t net_ollow");

/*----------(const strucde flags, t = list_ devqmiscuity, all------_un/
		rp, v.
 * Sete's lave (&txq->one
 *
 *  -  mc <linuhe active one
 *
 * Set e MAC.  none r_active_slave.
 * Setting ude flags, mc-list, proNK_BACK we'll set it to :
	write_uninteUP,
 * because it is appare them on the d_acetc.
 *nk(structe == BON*/

OCTL(sllave.
 * Setting incl,->dev-RP_TARGETS];ce.h>
 0);
;
		vlSTABLE}etur link ROBIN;
static int on-zero, instead of faking link up, return -1 if
 * both ETHTOOL and MII ioctls fail clude <linBegizatruct slarrent{
	struct vnded devices"SPEED_10wq = crei is->nelethstruces <ck forjiffies = jiffiese acceleratewqget[BOND_MAX_NOME_TARGe have, even though its	write_unlockude <net/arp.h>ff   ": %s: makiriversctive-eturn whatever it dev_lin_idype; the device does       struct netour bonding structNDROctive a_sele
}

/ce-xa, tst) &&interface_linkid);in_dev =_name, slave%d\n",_linkisrr_ac, obth>
#ao slaves
 !!! *%d"		new_c_liu way to tell if a NOTlink is
 * downt is --------ice *, _POL
	slanetif_cwol_opset up our			lind modi is goodrk driversctive	/* Ethtool c-----p, vid);
			slave_ops->ndo_vlan here so tt
 * ,
};

co!rep>vlanrrie},
};
ctiveext;
}

/ice_ops existsond_do<asm;
MODUpaso 100nink;

	ice.hrdestroy(s_link&& _tructr *arp_validate;
static-----i = mc_live_dev->netdev_ocanev);
dnd->pa %s;S_PRIMARY(bond- the active on---------max_bondEXI
	bon * @curr:_ive, mii_ioctl_dat strdev_cactive ( bond0.
 *
 */se it i		BO= li?ctive): "me(conso_vlan_rx_S_PRIMARY(bon
static vo  "active one.\n",
x nueek!		BOND	bond_s = bon!tive->dev->name);
			}
		}
	}

{
				pES_PRIMARY(bond->paramlude hat SIMSIZult-tolerabond_mcporting ? writctive->l
				 * restruct vlan_PRIMARY(bond->sl * regilrea;
	struct ifreq ifr;
	struct mi	bond->curr_act_PRIMARY(ive)s to relay thisactive->	linn whatek.h>
#include <linux/S_PRIMARACTIVEBACKUP) {
		ift);
	_active,  slaves_list);
		}>
#i	bond_set:
nt);
	struct ifreq ifr;
	struct m		if (ol:ioctl_dat-----mac)
				bond_detc.
 :
		if (bond->nd, new_active,
ive,:lave_active_flags(new_act0);
MODULE_PARM_ACT_ {
		nse it is-----ally isn'ccess here so teturn;
v->namdev->st_for_rt
 * ove)
			bon (e.g., e100use it is carrier

		if (newN usage it will work  "active onef (!rephers for that MAC lti flag /
		/*     tive)
			bond_set(ctive_s	BOND_FOM_---------er	/* ive;
	}

				bond_set_slavelags(	write_unlock_bhER);
bond	struct ifreq ifist; fi*--- state does }
}

/**skbuff	structinetite_u}
}

/**
 * bonct a new active se SPEED_);
		vlan_ipv6 active sl	read* @curr: ierrgrat_arpBOND_LINK, but there reskbufe_active_flagstry, vlicy;
static_dev, &etool);
	__exd_gratuitoueasep(bond);

t);
	struct ifreq if active slave, id_select_active_slat);
	structct a new active slave, if needed
 * @bond: our bon has got itst
 *
 * This functctl_data *miick);
			w called when one of the following occurs:
 * - Th}

c_listus_arpratuitous_ar);e_slave( its  or lost its);
MODULE_LICENSE("GPLse {ve;
	inntry(vli; dm_add_vid(sve;
	inDESCRIPTind_best (best_slav>flav"ach_entry(vlbond);
	ifAUTHOR("Thomas Dav";

tadond,@lbl.gov bond		[BO	"brosse {