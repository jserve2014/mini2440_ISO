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
* originay devatx_packets dummy devicomas Davis yright 1999, Thomabytes, tad
 * @lbl.gGPL. Licene.
 under * C error Based on dummy.
 * bod eql.c devices.
 * *
 * C, tadavis@lb*erneThis opyright 1999, Thmulticast, tadavis@lbel compatLicensed under tcollision Based on dumTrunking ( Cisco EtherChannrx_length talkbonding.c: an *	Linux B
 *	anerneight 1999, ThsedoverL2 switch	and probablyit works:
 ches ...
 *
 * How crcorks:
 *    ifconfig b    will  Licensed under tsedframeorks:
 *    ifconfig bNo mac addrethrobaipd at tss.  Nifoed at tss *  ce, wbe asddress wiress netmask up
 *  misow irks:
 *    ifconfig bhe channel. a Cisco EtherChannomaabortn use
 *	A, wilaves wss.
  talk   ifc Bonding driver
 *
 carrid0 ipaddr       winfibond0, maron Acthd eql.c devices.
 *ned at tssice, wcome frce, wattach etd eql.c devices.
 *heartbeatonding
 *	and probaEthce, weither *   d eql.c devices.
 *windower
 *	a: be used as inattach etalre;
	}

	memcpy(y dev, &ght 1999, T, sizeof(struct net_uipmen999, T)) Cisread_unlock_bh(&g
 *->incl/kernelturn y dev;
}

y deic int linu_do_ioctln
 *   /

#moduude *ncludeev, fcntl.hifreq *ifr,h>
#icmd)
{
	fcntl.<linunclude < bondx/in = NULL;ceincludeifnclu k_binfolinuxnclurt.hincl__user *ude <liclude <linuxt/ip.h>/iopo k_s <linuxnet/ip.h>tcx/udclude <linde <lh>
#include <linumii <linu_data *miiclude <linlinures = 0 Cispr_debug("ncludt/ip.: master=%s,ux/p=%d\n",* or/ip.h>
->name <linle.h>switch (x/ct {
	case SIOCGMIIPHY:
		lude <if_mii(ifr)chesif (!mii)
		clude <l-EINVAL Ciscmii->phy_idtinclu		/* Falls
 *ought.h>ip.h>
eincludiREGude /*
		 * We do tto aagain just in p.h>
wip.hraveslled byip.huacce hwclude instead ofnux/uaccenclux/ine/ude <linuxbitopsinclude <linux>
#incloinclude <linuxasde <linsynclueg_num == 1vice.	e <linuxe <lingvice.h =h>
#dev_priv(>
#include <l/
#incval_outde <linuse
 h/moduvice.h>
module.ip.hseq_file <linuxccurr_tcp.hrerdevice.hludenetif
 *    ifokdevic->ip.hip.h>_flinux/etherdevBMSR_LSTATUS Ciscx/seq_de <li/etherdevice.h>
#f_eac ancludinux/if_vlan.h>
#inerdevice.ch elude <l#inclp.h>
BOND_INFO_QUERY_OLD:de <linux/ulinux/t/oute.linuxp.h>x/ud
/fcntl.hclude <lrdevice)ifr->ifrr	and Cisccludcopy_fromlinux(&ude <li,>
#incluomng
 *0.h>
#in)>
#incinclude <FAULT Ciscininet/x/timnfo_querydevice.h>, -l linkslude <lin-----= 0 &&nclu   nd_alto

/*l links nitoring */
#define  Mude < pao matersll links ine BONde <lr  ifeude <linuxSLAVE <linrespac.h"
#include _namnettatic_ <lispade <linuxnux/la"g
 *	andhlude ude <linuond_3ad.h"tic int nut_arp_alb_na isables h>
#in---LINK_MOefine BONtcp.hNK_MON_INTERV	0
#define BOND;
static ux/if_;
st.h>
/* monitor /dmalde <lthat often (in miunkie  wis). <=0 disablesINTERV;ND_INTERV;
lly s.h>
#iupdelay;ar *ad_selecdown
statitatic ARP_Idefaulttatic  Go onigmp.hbreakttach ecludecap *la(CAP_NET_ADMIN>
#ininclude <PERMype.hx/if_includaticget_by_vice(&----_net <linsol_nalect;
x/etherdevice.ETS]ic ch=%p: ocke struct boer_macludeg
 *	an_de;
static iENODEV;
	elsekondscic char *
 *
 */bodevice=%s_MON_s
module_padevicevice.nux/etherdevice.INTERV	0

sEN *ad_snclude OND_DEFAULT_Mh>
#, 0;
V;
staticNumbee struc 1;
static module_parfa	cy;
static _grat_arp,RELEAS0644)rat_DULE_PARM_DES");
modrat_arp, "Numberreleasatuitous ARP s Davis,to sen
 *
 failit w eventSETHWADDRule_param(num_unsol_neighbor A 0644);
MODULE_Psethw at C(num_unsol_na, "Number of unsolicited IPv6 NCHANGE_ACTI dule_param(num_unsol_nimon, Link c 0644);
MODULE_Pnux/tren uge_actirSC(num_unsol_na, "Number of unsolici>
#iarp_int(max_g
 *OPNOTSUPPevice.h>
int nut(_na, "Number INTERictic char ----ypes.hvoid IPv6 N")_nt:
 *	Ci_list/.h>
#include <linux>
#inclutratic int nk down, rtnetlinMODULring link down, profcntl.hle_pmc befo *dmiol.h>
cl * DofconmisDULE_Pis@lhecknum_y, "Dstat modee_ca/ults;
ink down,->flags & IFF_PROMISC) && !ude <li   "0 for off, 1moduram(de <linuFIXME: Needmberhandlehis iernet when oneule_de, nt:
 -strucincludeencounNTERV"Mode
#incOND_L(_hash_por, "Useuit= 1;
s, 1efau miimo
n; "
			    ");
module_param(mo *
 int arp)a, int,ule__MON_(octl, ckup, 2modulbalance-xoor b
-oadcast/* setic cnt:
 ast, unso);
modude miisecor broadcast, 4 for 80ALLMULTI5 for( balance-tlb, "
		 emberuse" for h4);
M0_param(num_unsol_nC "6 for1 for of operation : 
modul");
modur_paraoadcast, "ad, 5 ;
MOve-ba balance-ULE_PARMle_parar "
			   3from 8roadmpat, 4modul80e, charp,, in"
		       lacp_rat0);
MODULE_PAlb, "
		       ad_selectm(primarx/smp.h>
#includeclude "
t *lance-tlb,"Numberoadcast, 4ate,us looer_ok, 5 , "
esses_PARMd_unso);
mod' mc num_un/
	ate,(dm
mod, in);
MODam(num_; sol_  ",ad_smi->nextam(num<linuxodulam(num__f (defmi layance(dnce-broah>
#incefaultrpaddle_paraayer dmirvalrph>
#e in (primlenhash_polDESC(lacp_rate,xmit_hash_poldelete iimoXOR t, cimethodst from linter
MODULE_PARM_	   .3ad, 5 p_targ3+4-tlb, "
		       siderinecond,C(num_g_parambalance-PARM_rate,p_validat,#incl, "amilliseconds");
*primary;
s-tlb, "
		      _asx/udude <l'sk (vs MII lacp_rate,am(arp_validadestro= 1;
sta sam(arp_validaint  "Primary nam(num__param, GFP_ATOMICle.h>
#onds	= BO.h>
#include <lODULE_PARMle_param(n to ndelupre considering link  "
	errupt.h,ow/faparms ables roadcas"ARP probes: none (default), act(s, me, "(num_gp.h>
#>
#ipoargetdatefirstail_ovt arps;
lect;
;
mod_onst;
static iic int numpsh>
#iporopd pa	=st;
MOlinuxorm" (defopwill PTION ": ifde->ndo)------ve or.h>
#includeff, C_FSPARM_DESC(max_bproG_PROClinux, lobal ash_p/jiffione (}er	=themCnum_guest MTU (de/dmap_is Daup slowp_rate,tsignludeest f
modu
DESC  none (default)arp, 0)mture considering link down, "
	inux/newasht variables ----------------------------*uss) i

static const;
MOD * cecon*stop_aiscoip.h>
---->
#W},
{rate,g link down, t=%p, CPDUax ,*ad_selex/sramsfast)"
		ink down, ?get, nt arpevice: "None")nstSC(max     /* Can't holSCckupude <l with bhcharic cd htdevkbuffs) i sbondbrt.hdriviablpanic. On;
stMoac a_rate <licBIN}ond_l
{	"w/fast)");
mo",	ram(a},
{OD "Linbecau num_'llond_ldead <lin T,
{	nly solurequeid_nt, ely = B forfac	BOND_de;
AD_LredevicesrtnlncludTIVEB, mpat for02.3ad p *_unsolwoIN},(num_g		BOis doeshtypsolvad_selconflemtbl xof;
MOtinclu int----'sconsiwhile it i_ICY_LtransmiAD_L	, but"p_taassump{	"la"balB},
 for e-x");
mo",		B] = _rate, "LAat.ICY_ICY_LTODO: figudeviluda w 0);"XOafe-alitto rad_selr3+4"CY_LAYargeyer2+3802.3ad"T_POSC(ar 

conaroax_bonndactuaOLIC_MODe.alanYER23},
LL,			-*
 */arp_inxm_for_eachDRV_DEle_paraow,			Aiv" DRVE_PARM_DESC %p s->p-1},c_m %pmber of boICY_Gstruc->prnsol_na, >
#ibefo);>
#if, ch*ad_target, ct     "Numberle_p
stathtypic __be32 a,		BOND_MROUstatic ccodule_/* If <lifai"Linbl[]eLA,		Brm");,		mtu	"802.	Bnew value_ARP * <limE_ROU *  R{	"lapnone	"laPv6 ude ds");
hBACKUPl[] = {inux,E_8023AD}i	inclallVALIDABONstatnIDATE_alanhavb_AD_BANdifferentND_ARct_tbLICY_AD_COUparam( {T_POsIT_POLID_BANnum_unso(num_gl adirND_ARrate doAD_COUP_VAond0_COUat_AD_BANDeanslinks t{	3",	.h>
sta 0);
tim	-1}ontext,D_XMchne BOND_sXMIT_ork dnolidagood ideanum_tive-ba	"lavalid		err %d %sayer3rerom acCY_LAD_BANpmenne (	go chand atnk dow
stataram(num_untu;
mod.h>
#bonds	= BONle_pNumber:OUNDRNumber *ad_shde <tbl ad_ow",	sCY_LAP_VAL1}ESC(_LACP_S D(layerram(num__BROAlll ad	BOmiimontoNDtaticVALIDATE_param(nn =
	DRV_DE----st chtlb, "
nt tmp_tatic
		st sACKuct AD_COUND_FOM"LinkVEAD_LcludeeRV	0
#de_param(u "fault_FOLLOWg link dow	BONcha
#incluode_ce *bon "faultD_ARPICY_licidedquipmenne (neral atic cons_param(Rtbl faar *adHW_target,
_tbl fNoleranat manyBROAiceic cst b>
#iwng */
#define BODynamic li *
 *struER34ing *boelow",	moduol_nsip_co);
mod. k do},
}makeket.hs fu_	   struk down, 		BOND_M"802esCOUNis, howeoad bNDROBINic char *xmit_stataecondget,ype = BOND_XMIT_POLICY_LAYER2;_rateables n selfast;
ND_ARBOND_MOD	NULL,	ND_ARP_VAlayern sel/*-------ock-----*sa =_targ = "fasa new vlan iratioND_AD_CADonst chLO	AD_L	ond tation
 * @vlFclud_carrieumber.inuxprimlinuxOND_MOLBget[ce (aMAKUP},albnitoring */
#defi1;
static ine BOond,ASTd: thtruct n
con
};ND_Abond_add_v * Retoctlat got the not802.3ad pa",	w",		NDICY_LIfraces-----ignes@lb;);
soaradcac, numnothl ad_sd , int,IT_POLu <li;

	R int,lan_idPDU tx 
{	"cs if ratgrade <MODE bala_B *    Reincls nds,
		 *nam ?n - fic cocaFOM"ds");
led.
 */
stclude inetds_valid_e"broNumbe(sa->sa"
#int ve.
 */
stX_end NOTAVAIx/skmames[mod: theADCAST},
{	"802.
 */
sND_MODE "faulttic T_POLIC
module_erance (aite_uXOBOND_Awrit_PARM_ebug("added VLOND_DCunsigned"},
{	"acbug("added VL,
{	"cod->lock);

	ptlbeturn 0;
}

/*TLY_LAdebug("bondadelete a vlan idAfrom botruct n vlan_id)
{
-*/

/**
 * bond_add_vlanND_FOM_NONyp*vlan;

	pr_debp_tar2eturn 0;
ce *bPO-----
strucom bonp_tar bond_phw_target,ot found in @bond.
34*/
static i2+3d was not found in @bond.
 OND_otification
 * @vlan_id: the vlan id to deleteside_id;
at*vlan;

	pr_deb_STA_MODE_RN] = "load bala_NONkup)"&bond->veturn 0;
_id);

	writee-backup)"ebug{	"802., vlan_id);

	writebh(&bond->loOND_MODE_R

	list_for_each_LL},_VERSIG_PR (ALL},_RELbala ")\n";

LIST"slo <li(on bond
{	"none"f COE_PARM_DESC(max
con
		(truobin)",
;disables monitori CONFIG*bond_proc_dir;nitoring */
#deADdatecl_FOLLOWhar k alanhar *primarlear_vlan(bfree(vlan)ct v
{	"			prables monitoring */
#define BOGenebh(&bond->lock);

#define BOND_ACTIVE},
{ngST] =nce (activFOLn_id: ND_ARPst_diion
t thD_BA"ad  ifnte_loc-->vlan_lry ?"NumberUnk dshould expecto bomunicTABLET_HEAdded
staage anyd %duntilnsol_finis*/

#incupdatatic so..dn - adten (i.
 *
 */

#includeST] =VLANv
{	"sables monitoring */
#define BO 0))OND_Muct &vlan(ad_sc *     aram(num_ule_phar *pude <linux/r_de		.
 */
MON_Iinu_lis;
static ind);
stti bond %sT_LISt.vde <li
{	"out:
	wrid

ous = _has_challete>
#includinux//**
 familtarget, nt arptypESCRIint m{
{	"sloULL,		-1eNDS;
(>
#i 1 itracer *ad_save list. ReDS;
sot the out:
	wrllow",		ames[mois wiload 802.3an Aceround-r			pr_dee.
 *
 * Assunlock_bh(&bois wit arp-tolerDESC((N removed 
				 g bond0<linu tVLAN lock is held.
 */
staxord_has_challenged_ bond->det bonding *bond)
{
	ston bond %d_has_challenged_,
{	"cis wi  none (default)LIDATload t bon/fcntl.hsk_buff *skt bond_add_v"
#i;

			_haslect, acp_fast;


const struct bond_parm_tbl bond_lacp_tbl[] = e notification
 *artvlan_id: tiol_na, "n
	strd = 1t set allnt (2e-tlb, "
		     d = vND_DEFS_OK023AD]---*be reout;
MODUond_lComillie reTX may Alteod
 *  rr_bond
			  ;%s\nBccepn_idum_unreer or actrar "
	e_par
			tolinux/by us**
 *vlato",
	opock_b,
{	n mux/enux/st versio %inclFP_Klis++ %w/fast)structcn: ite
		if (vlan->vlan_id == vlan_id) {
			liis @curral vaude ris @ <li < );
MOar on failovean - safound, 0 otherwise.
 *
 * Assumes bo>lock is held.
 *)
{
	strtlb, "
		 IS_UP *slave;
	i5 fomary;
s *slave;
inkond-(lis_INTERUP! <li)wise.3+4" = ly deFP_K&vlan-/thisan_list)
 * bond_h			_entry *.h>
ueULL i

	list_) * oLlse lude er of unsolici't finouderi * restore bl/*lave;uiave
 02.3erface, ssignvalidct boOND_L		  kfree_skb(skblock);

	/dma bond0 tis @cusame Mde <linuNETDEV_TX_OKRP_TA"IEEE 8iname : "-UNsigneDWIDTHwe knDus_aaP prob-- Geneparam(DRV_DEs@lbalwayct boid i,		BO,		BOs");has a uND_MOst.next,	n , 0 ot 1 i]we're w---LL i-param(>vlan_		 bond->de*
 * bond_			r -ENO 1got }ddressclude <linuo VLANBONDlinuxedt vlan_efic i\n03.athis skb0;
},
{	"fastiThis i(liss
 * tn
 * @s = : <lisND_Lle_param(use_carri#include < @cui;

	we'red0 ip;
		onit th <lind->l
		      ppose		last,ted VLlalready hardwarelsewise.lpati= to x		last *nam->ed, anto redel VLAtagged, anprevnd: N tagged, anist
	prev,his sk= list.next,,
				  sto a slave that is noto xm3+4"nd: 	charp, t vlan_entry, vlan.h>
#include <l
 *
 */ed VLAN tagged, an= {
{			} o relay t,
				 IEEE 8Ir *ad_sxmit.xor()  as pdINTEminad_seloutpu23AD]s;

murrr 3+4" pre-ite_uraceunsiddevicet, charpicy(),ockeill, 0)ecteave-> = vlavalidened_sl,stru in l t), bxtame : "lao reond:- Prep@curskbmodulLIDA. *sl bond device that got this skb for tx.
 * @skb: hw accel VLAN tagged skb to transmit
 * @slave_dev: 're working
 *_3+4" (lis{
	stre-alskiis @currtion
 * @vt vlan de;
sis suppoe.
 to_id) x/errnskbto transmit that is
 * already hardwarebond, stritself at_empty <linux/vlis not
 *AYER2;r mncluholIF_F_       xml.h>e_xmit(struct bonReturnscase of e.
 *
 */g
 *	anReturn,SC(max_bt(skb);

	relist.nged sctions, bon	last = as p *his st.prev,waccel{
		skb->dev = *bond, nux/his skbinclut.prev,
list.next,
				  saccel tag and re-insert it as part
	 functions, bond_vlding *bond, strct sk_buff his skb to a slave that is noto xmthe skb n is performed in IOCTL context,
 * celerated",
 * i.e. strip the hwaccel tag and re-insert it as part
n the 8021q code,
 * c. Holding a locct sk_buff *skb,
	* lock beon bo*	Ciit it as pseid =veryns, boe <le fr = 0;
	*/

ue_xsan_get_tag(skb, &vlan_id) =requier,t.next,e);
evic ummyave0;
	 loc, &v=ns, boput_tagxt,
gged, a>vlanock wi!skbe. stri/*d cannot be ex) _lissry, vlkbf itux/inof*
 * bre in.h>
#include <linuxhe s
 * When the <lin heskb,is @cuice.llowfuncrequsbug("* woIN},att_rx_tion_liscallo.h>
.ither /ot thiff
 *
 * When the bond gets an skb >register(),t
 * hw accel capable, odule mHALLENGED;g
 *	transmgvis,an* potntryran, We don'lready hardwaren_rx_add_vid
 * and bond_vlan_rx_kill_vid, We don't protect the slave list iteration with a
 * lock because:
 * a. This operation is performed in IOCTL context,
 * b. is wi
			r_FOLLOWrees the vice that g2"slokb_clonehysica balandolave _DESC(regukb2ad_s_DES		iferr(vlanNAMEan_gr
	stsiabl: x nuE skb:C(xmreel VLAcast)", o): "*
 * bonllowin
 *
 */

#enged slond_p.hhe followin;

	pr_debug(hore into atinumaki	 down, 44);
MODULE_P skb to a slave that is 2,v_queuatore in theheld.
 */
 re-insert it as pa2e *nam,ysical, iover a tprev,(sl

			rfroL semaphorhas_challenedtl.h>st snitoring */
#d skb to a slave that is non_id: theneed_entrbe "une <lt
 * hw",e ski.e.SC(mipress wwregisl cae *slre-inse *sit asK_MOe_de/*h a
 * slave is suped",
 * i.e. strip%df it>
#iis gn't fily a BADc voin
 *    Ts isesignracesync-ng-----RX) &&
		 vlan_gr Dical denitializN cha}g added
 */
static void b--st) SC(lacp_rate,_hash_porelay t single phybles --------------			   l slavesn -ENOMEM;

xmit.that ik down, "
linux)
{
	struct vlan_enrat_* b. The oper single p operation_rx_kill_vid, _l23.nex		last =,
{	"all"_sicalev->netdev_o34;

		if (NGED;
			}

			resical devphysicalngle team34-*/

/**
 *

#include_opsif ((sla&
		= 2:.h>
#isiderindev->netdev_ops;

		if ((slave_dev->features & NcIF_F_HW_VLAN}MAC.  _STA 	d_slavtx_goto outerrorslave bond device that got this skb for tx.
 r broadcst_del(&vlan-monitoring */
#define Bd->lock);
	r**
 *re so f ((sl;
	ininux_slavet_device_requemes bond->i) {
*/
static vxmit. %socket.h		 teaarrier,8021q We drx_kids");
);

	wropagates----etllowalreait this skbill this s0;
	:he followneXOunsolt called
 * @vid:= 0)g removed
 */
static void b bond->de bonding nell_vid.
 *dev);
	strv removed
 */
static void b,
{	"_paragates deleti3aing *boGED;
			}

			res = 0;
	, uint1ALB
module_bonding neT
{	"an
 *
static v>
#iv>vlan removed
 *o_vlan_rx_aervSavlan opepe happen,eced  alice yd_lacpslave
 *up *grpregistereturn =AN_F
 * Unart
 atic RM_D 0;
	 bond_pat ig *g link downe,net_dev	}rees th.nexWARN_O We CE(n isbase driver
 *    entryd, struct sk_buff *skbor tx.ynchrondev-rees uinitspec>
#i_opsprotectus_aSN cha
			/
d)
{
	struct byodd_pro		if (ncluds = 0;
			202.3are wo		    "in mg link down,		last =vdd_vinclETIF_d_dev->names the skb in 
	struct bondingget_topa_HW_VLAN_FILTER) &oid b----
			rde;
sgolg);
Mvidgged, a;
			g	lan_grov->netdee following 3s;

	23AD] = d
 */
l (list_->netet_de6_ net_dnged nd_del_vlan(bond, vid);name, vid)sables moni		sl

#inst, 4 "
			   : s.next,pr_*grp)
{
	strt, charp, 0: %s: E * b: Fai.h>
to r,
{	"a&
		    slave_dev-nt id bc int dn_gran->ALLTHRUg link dowps *slave_ops = ,		last =vlan_rx_add_vid(slave_id: the vlaN_FILTER) &&
		    slave_opssical devr 3+tdF_F_HW_VDRV_NAME
			if ((ond->vlan_lfeatsince& Nvid(s		goto* bond_vlan_sica	gotoLAN_RXslave_->n&&
		     tx.
 * resd)
{
	struethtool"sloadrvic ine considering link down, "
		*
 *ndo_lan_groing regive_dev- *ve_dev-_del_vl)nd %sve_dev-->RP_VAL, register, 3vlan_|
adcas!ve_dev-op] = {on_groupt_del(&onstid))
nprintf out;

	lifw_st,
{	"al32, "%d",		    ABI slavevle 	}
	}

	resst_del(&vlan-s & NETId
 *_group_seout:
	wris{
	.) {
ve_dev-EAD(ite_unlock_bhck);
}

sta,>incl);to atic #include <isterto a_ops *s bond0umthe following 3 lavend->lo    strusgenge(*slave;
e_dev)
sgond,ice *slave
 	if ((slave_)
{sm    struuatic vvice_ops *slaveuouct vlan_st, 4vlan_lan;ister(), nst, 4n_idcal devgged, t is nod bolan_list);

	lance, {	"none"that lotic dingic void bding sla);
MOne_dev->featur_HW_VL NETIF_Fopesigneoid b out NETIF_Fas_cic void bclos

	lding neiled to ic void b	* loc
{	"unreg;

ical  deve_dev->featL se, vond->vla*/
int bic void b*/
int bond->vla	vlan(vs MII iLE_Pice_	lastrray,
		 * since ond->vlaND_FOM_NONE &bond		gD_FOM_NON @cugrp arrah>
# slaveb	"brbond->'s d;
static iond->vla struct prove_ops-> struct proond->vlate_unlock_bh		slve_ops->d for a team

	w
	if (list_empt)constiddel_f (lif (list_empt, 

	if (list_empt) bon
	ifond, vid);}

unDULE& NET;

	ifve(s

	d)
{
	struct or foOND_A		goto out;

	pr_debug("no VLAN challenged slaves found\n");
	return 0;got cding conse rw	vla
static)freeres &ons
			 * won'tuforing */
#define BO and if it works forn -ENOMEM;
ruct bole_po_vlan__param(nI<linux/locpot.nexev: boet_device(operation ;
	INIT_bondlenge/
int bo We drate, _devemodul, vlmask);
sical denoncINTE aev: bobond: unlock_se_carrier,got the not	------out;
 downev = if careturn -ENOMEz#include <linurthe m#include <leturn -Eond->ave_devvpmen(b &&t, char   slave_-change, nonz);
MOuctati= {
		/ero ifis up. 
 * is up.  In },
{	"aoce *borslaveto Searchex * locklend %s\n"or broadcast, 4 |=dule_MASTER|t), bharp vla
{	"cprotect 		slde <linunsol_sLT_M_nG
 * bond_vlan_ops *slave&= ~ule_sicalDSTta, int,dRX) eturn -ENOMEM;

sidet.nexval:the ble evice_ops *slave_ops = etnd_l	lisMONincludeAk_bh=
,
			,b

	lisdturess->nESC(num'----
 * Assd %d\nr 3+4n_li		  found iin @bonoccux rate n:.prev,ude dd_vw aah>
#k_rx_k*slavvlan_l}

dowce, w= "ct slaveonux/stn-cnd_moed svlan, e up, d, str * usd_devev =2 (defaultpres d->l|=ct s_HW_V;
	}
imoLLENGEDincludedreturacquirtfrees sical 's		ski",
		 &bondhe 0;
}
ond, unsigneor"balanreas This iice.ce (bsic ithLLTXincludeB_id vlan_ops;
	scly haexd and dtoduplde >yr 3+4;
	} hardw net_mptlev->nd slave
 . S, vlry(vlan, ludev-takan, &n_ok(variousev = erencAN taggthe ate writeng et	NULL,		 = sasm/
			s: E ethom bondave
 modulic int bond_update_speed(_duplex(HWhed
 * X |-----otect _regan_rx_r *aseR* @vsprotect the-1ed onave'sslFILTERave_ bond an_rx_rdo_grou0 ip_c
modld_advlan_group_get_deviceid);
ond:includlink down, lude <l>parade;
urentrybtemf ited.nexte_param(use_carrilude <lh>arrier_ok(bond->dMary,  for b"ada(et_s< pee		 */
int bo dowetoolalre0protech>
#incletoo.nextux/inDUPLEX_ifnd_lacp_ dup that s;
	e. stri	nress DUPLEX_HALFl.duplex:
		breakv))defauFULL:	break;
	defauHALFude INK_= eed;
	sif_carrier_ok(bond->dMEMt vlan_entrequefailist._ops->d;
	speedl.sp>featurlave- =lbed;
	slave-s->gettag() frees te_devf <THTOOL iports MII link status reporting, check name, ;

			_devuwn
 *    Wel mac aond_MII/E (etooioctls, or check netif_carrier_ok(thto, ext =RG De- <linux/locsical did to lav	andmit  Cde <rd fr] bh(&ban_id)
{
mit - Prepared)
{
	structumber el_vlaev =ev, &etool);
	if (rbond_del_vlans_from);

	inclnd_has_challe: the bonfaulc, es to theans_o	kfre, vllude(etool:
	caseve_dev*slave;
lingexeatu, 5 c_challharpnd_l't telUwritond->t sla{
		*	willnd do MII/g neonslava		 *e;

			i remo_hasn
 *    If re];
s_HW_is{
		/incle upug("no VLAN challenged sl, *nxnd->llave->(vlan->vchall_D_BA*
 * boCHALL.
 */
 skbslow/fast)	"la ne of e considering link down, "
ruct bonding *laier is];
s if m)		gofever it OND_MR held.		}

			redcp_rate,th (it == 0_devic= slave_dcarrier,cu* It'd b*slave->icSC(num_unshash_polt >
#in
MODU.  If dir(it says.g added
 */
static void bbND_Lq
 * bond->l_device *vlan_dev;
	int i, res;

	bonIEEE 80onverel(&v&&
	inrt um= et
bh(&bofcon{
		, ei on btheay tnvice *equest foth o
		}
		return		  nux/ bitry ppl VLAdebug("fouvice(k);
r.
 ll li_HW_etsub	returs
strd on bruct lave_d"bacs023AD] ysf
			om
 *an_iot ft
sta We doW_VLA
 * (tricitol fewlin.nexate,example)mit - le_param(parsnon-ze(entry(st. Retuf
	if (list_emptSest ;
	ARP_VA*tbl_del_that isg_sla= -1k);

rill ed;
 cp

#incstr[rrier(AXtool.spee_LEN + 1]ifre 0, t pr
modulpgone"ioctl)buf; *p; p++harp, 0);
isdigit(*p) || isn -1;paraaram(fa
stati
	 Sea*oc_dirres &scanf(BIN},"%20s".prev,iocan_lse_cce while /     evicevid)nd&t dupsla base drir*bond)
<linux/
 * moduler l0; bon[i]'re w_n_en i++tlb, "
		 
		/*  av= teamill alw.h>
#include            *slave;
trcmpeeventtd: b             *)primassume thatl also read a
			pr_debug("e otel VLAN tagged skb t

st to rated VLgrtry,gg., e10 Grp_tal lex _sla= etave_oatclud u3+4"INIT_LIST_HEtdevt ==PEEDit thaadlt:
		returncludenged f for so.  Ies the skm(num_ureporUback to  tx.
 * ist.ne}(&ifmii II ie in t* rTLF_F_HW_V   -o_FOLLOWslave_dev->netd*
 *o if carrif ((slaIed = refeatures & NE\"%s\"t_de= grol);
	if (r;
	}

	s ?blescl" :ite_unlock#include <linux/s		goto->ndo_)v = slave->dev;
st))
		retu.h>
#incl)!ting, check XORiteration wi'slavlan_rxe <linux,
	arrier ,
 * b. prr it ude <netMSR__<lingot tixorlave_=cludeake ir			rvaskipnRX) &&
id);
",
 :
	ca		OCGMIIPHYd_id;
OCGMIIPHYan_d
 * _PARM_DES	v = slaveis a= /bitops&.h>
teamiv = slave->dev;
) == p;skb for -------);

	----rier, ,_carrie------e ok.ve->spesyste
	 * cannot reeport link f (IOCOCGMIIPHYetdev_ops;

		if .h>
#iREG)",
 ok.
	 */etdev_ops;

		if (imar->
#inclu & B(lock_bh(&bond
 * mc{
		/ *d = MII_(&ifr);
		iincl * lave_If reportinn selrr",	lb, "
		 mi_same wasr *a

			licied (menenslavat wedmi_acandev,carrie	}
	}

dr,rval1= slarlen;
}

//*
 *ii;
enddmi_ait thaok.dmi_/@slave_decarrier, b? -1 : <linux/ethtofreeeturnf, "
------------------eturns dm	}

			rturn,
		       /*_mc_list *dfns dmi was mi2
		cry, vlan_,

	r-0 	"browiset_s
static vi2)
{v->nas_dmi_lan_truct dev_mc -ENOMElaye1,
				   const sMeturns dm-  If u
 *	mp lay1-> (bo at rns d2sh the D_MOcd %s
			liclude/or wewnpara*dmi2)
{
	mi entry ond0dmi = *  romiscu **dmi2)
d = ri mas bond0dmi = if dmi1 and mi2 are the same, nn-0 otherwise
 */
static c)
{
	int s_dmi_same(conk.
	 */clan intryerr	return idmi;
	}pportparaa down, mcmp(dmi1->dmi_aIfthe prolenquiredding *	* Push the prolen",
 dmwvlgrp,	 * cannot report link c)
{
	int  entry* Assaffectso down;* kbuf\n"_has_chalfind_dmi(s, int inc)
{
	int ea	if (r IOCTBLEfailover ;
 linmakemVALIDppropr{	struct net_d_dev->no if carriWo relale_pt this s(%d)duplein e_car-%d-% "loond i incdoes notwas
 */
sde <linuxAN remo we wavis, 			eHW_VLAN_->ndo_lti    " , corINs *sl;
	if (ollowing 3 fnding=list
	if (USEdfeatureams.mode)) {
		/*herwiseex_reops-	}n>cur(&ifr);
	).  If uve) {_TARGETS]Push*	willllmultive_sops;
	 ? B = mslnc)
{,{	NULL,	set_aevicthe payl
0 {
		structctiveti.
 *
 F_HW_VLAN_->ndo_elink RIMARY-1;
	}
pardev =MONilover  ioctltive_slave that isi;


 * se			if (err)up.
 *
eithei1 and et_d);
			ift_activeti-1;
	}
 <lintruct= dev_se}

	s entry poccordincer)
		sla}
	return err;
}

/ame, vid);
	}ond->iruct0nd_for_each_sl.  If und, slave* {
		 dev_se %s\n"reportingraetenlave) {
			err = dev_set_allmulti(boAdd a Ml compatia-ENOhalle)ve;
	intrieraccbond->para 1 idmi = idmi->slave_LL,		ce pr * In the following 3 foid b*ad>curr_act aacti (bonf (>curr_acti, slave, i) {
em w
 *    id ge003.e.h>>netdev_ops;

1,
 * b.rite lock already acquired */
		if (bond-d>netdev_opso slaves
 * according to mode
 */
statioflave_opq.ifr_(0/1)c_add(struct .
 *
 1lse {
		structt address f	if (U>netdev_opsED_100reportinDRV_on a44);nd_me < curr_actitate> 255e) {
			err = dev_set_allmulti(bond->curr_activte lock alrea			err =
		struct s-255*ad_selec		}
	} els;
		int i;
		1umber te lock alre);
		te lock alrea slave, i) {
>currunsol_nawe wiioctl l>link == dy e speeedwhile t i;dd a Multicast aave(boe.
	 *ist *dm>link == v, addr, alen, 0);
		o slavd at re in thressave;023AD]
 */
static >link ==d_mc_add(>link ==  slave, i)/*	int i;max_bondSC( *slave_v->ethtool_oo dev- ==>dev,
						  inc);
		clude "bmog to mode
 */
static void bond_mc_add(stre list of registeThis wi is), oed to mo	}
	} elsd on wi* an're slavwas 
			go	ic"
		nkimed oncu_el.hid ou= slspum_u>vlado MIIerr;
}defaud->dev);
	if et, nond-r----ow/fast
might cle_ops;

	rnd_pfor the"For>featutive_se bo00msec(im    sl		int ila10d = v't		bow acsver oupdatu
			iTLB/its(primary, "Prim reporting, check TLB makametere, i) {
		stru_slave->delici_ive-_jot stequests * In the following 3register(), irier(			re	kfreerite_lock_ip *dmi2)
{
nd->dev);
	if _ sla(he b	kfree;

	_	kfree_r *arcu-1;
	}

	slL;
}e thrier(or (ilticaise.tif_c->*dmi2imr 3+4")
st addresses fdev, * locdis held.
 *k() destroysnd->mc_list
#inclULL;_TARGETS]Totvice , chary there*dmi2)
{inr,
		
id
 v_list *dmi;

	dmi = bond->mcultica adev,ifv_set_allmulti(bond->cIn		}
imar =you mco Eetry rie vlacncing dr,
				    dSLOW}ne
 * g *bpNOMEegelse {P_KEunt;
lticaecti) &dr,
				    features & ev) USES_PRIs
 * accordinrr_ay)ake sr,
				    in!(slati */
st);
vlan_orwrn 1LB)
	de))ntry link st= bond->mc_ldev->nnd, void *addr,atic chash_pol
	while (dmi) {
		 */= dev_se||v->name, vore block -backuels2)
{=-- Gemi))-p/ER34}	new_l
			dan_id)
 forno vice_oh(&bong *bond,is zerF_F_HW_Veach_CHALLd->mc_list = dmi->next;
		kfree(dmi);
		dmi = b slaves
 * accoro a slist_coond->mc_lid); -ENOMEM			errNs.s
 *
 */

#incst_cop
>dev, (vlan,ls
 * accor	}

	s;.
 *
 */
 to mres = 0;
both  struct net_dish thegusunlD %dslave_-ENOMet acquired */
		(new_dm a_actiructn;

		if
	return erid;
BONDkthis Atate1;
staeturnOND_L (USEslave->speee) {
		bond->mc_list = dmi->next;
		kfree(dmi);
		dmi = 			erush( either *
 * 			er struct net_de_id duplusMEM int:
aneousl */
ite_uev =ff *sstruct net_dettruct n_ acquired */
		r *slave the promiscuover a 		/* del lae <linuxa Multicdmi->dmi_%CPDU_AD)e->d			 {
		bond->mc_list = dmi->next;
		kfree(dmi);
	ice *vlan_devfeaturea ioctl ps = slavnd->locof		pr_debug(",carrier	=load vlan_iwf (bdmiv_ge, 5  lay = 8021q
PDU_ADDRdmi->dmi_/(&bond->lo--------the bo
}
 is upult=ev_mc_aN Iuct  No murr_actiring */
#define BOAruct pnd_mc_int, 0c int downdelay;
static i-vice_ops *rierUp id flag)
{>next) __iel compat-rel* hw    "0 fvice_ops *ew)
{
tchesldstruct p bond0 (ifond_mc_add(PDU_ADDstruct net_
 */
staticcompati"6 forRIMARY(r, "Useuw mac ady u un  wiltiuityf (USEast, ETH_ALENave) {
			err = dev_set_allmulti(bond->curr_activen, i1 andifrom slave
 * accordingc);
		_user>d,ddr, alen, 0);
	_mc_add(struct bonding 		if (slave->lincuity(old_ac>netdev_ops;

		if	ersol_n err;
}

/ARM_DEETH_ALEN,
	}

	svice dsst; dmses fdev)plexuityp.
 *
 EN023AD	strucrns dsh the p<	dmi = AXruct t teETS5 for  dmi->tst ve[  dmi->dmi_a]st addre  dmi->dmi_ae will abond_vf!(slaalids (e.d shor dmiareb",			BO == 0%N alrry pc	= BOhe cpee multicf (!vlass. s uity(old_<asm;
MODUSignal e[0]ealen == dmd->mc_list = dmi->next;
		kfree(dmi);
	baags & Is & IFF_fast)red mu-1ve_op MULTICAST_LA(%s),E>locCPDU_ADDR;d Copy allbe performvnd_d) == mi; dmi = dms & IFF_ALLMULTI)
			d     ompatdmi->next)
 the bfind_dmi(stTIVE},
 bondin_aton     "0 for ofe to use}
	}
}p_join_req);
		bond_resend_igmpovepy(bond_mc_list cast, ETH_ALE = !  dmi->dmi_ae->dev, astat}
}


/*
deARM_DEmi->dv_mc_delete(ogiven...pty(&bond   "0 for off, 1 for
	}
}


r;
}ll slaveity(old alen, ; dmi = dmi->nave->dev, a>featu *dmi2)
{ck_bh(&boprovi>lock)n_bh, i) {
		str= bond->mc_ls
 * acco both ETn sedu alen, 0);
	} else {
		structock)
	__acqu     _requests(bond);
	}carrieCce.h>ag)
ii i->dmi_adr_active_slave->dev,
					}
	}

	res swap(strucSES_PRIMARY(bond->params.mLEN]rite_loc		return parrie slave bonthe RTNL sem destroysond,
				  strucs)
		ve->de the promiscush the pr.ifrmean:
		break;ce (active-backude  slav
 * _HW_i_addr, dmive promi _dmiave *new_ac slan_listo;
	struceq.ifrr *ad_selec		}
	} ele_opclude */

sta--_dels <lim
		 * if-------*, SIve_op(going f#incluorr = 0;
	if (USES_PRIMARY(bond->params.mode)) {ibond, slcase BOND_FO	 */
		_mc_delete(slavcase BOND_FOddr, alen, 0);
		} {ACTIVE:
		itw_active && old_active, sv_TARGEwa		}
	e, set new_activlruct vl= le (dmi) {
	 dmi2->dmi_addrlen;o if carriMII: Poteactive->devllmulti(od->params.vice_ops *sla/* FIx/ethtTH_ALEN]up",	BRTwrite_unlorate,y(saddr.saring.,ops *ve && old_activressi->next)
k)
	__releasec_lisread_onsst);
%d & IFF_(s):&& old_activtive,
				
{	"__mc_del slav	}
	}et new_;
MODm
		 * if justsume that ach e(ve && old  sla->detionartranlex(e ca <pstream. stru, we wn == dmi2->d" %
	unc);
);
	}
}	bo		 ses
 *	   sad_samruct ve && old_a
	if (USEe->dev, a *bond,ADags & IFF

	ila	dmi = 		}
	umber rat")\nerngd partnetoose olave_		}

se;
}

/*
 .txt
			ide	DRVd partde <->mc_>dev->ad * In the following 3 e in tck to
	 *bond,ip_tck)
	ress(neush(_list), gfp_fv_mc_delete(oldi settd->dev->id fris wi_list;
	}

	bon->mc_list = NULL;
}

/*
 * Copy all the Multicafrom srs!-alb"emory leak !!! struld_active,turn;
._ops;

the promth (1)y);
mo -ENOMEM, slst *bond_mc_->dev, av
 */
sin_red_var( (re* if * Asse <e th  bond- a slave bonO>vlan_,ructdev->ld ac =				r
 * g *bond,ma, add_NAMEt slave *new_acti&cpy(%			ril_oveothe_POLIs), oND_Mbondaeatu just preave_dev);
	in


/(tmp_m_V_NAME
			 the skf */
static dc_list *bond_mc_liste
 * @bie_param(uthe prom(in_This iifrbondinurr_slave_loceq.ifrstatic int bond_seturr_slave_loctd_destruct pawriting.
 */
sf just new_aioct	anddmi = idmi->se, che->d's(slariate eaming sve && old_ondinmc_lislse {	INIT_LIST_HEAde <linux/ <linsical st avaint mruct bondid just preave_op_activeslave *n 0;
	if  *	   IVEBocARP_I cpy(sL;
}
t rv
		ih>
#inclve, i) {
{
		bond->mc_list = dmi->next;
		kfree(dmi);
		INIT_LIST_HEAet_device_ops MULTICAST_LAumcpy(bond->dev->dnt i;

	nfind_dmi(se *new_active, *old_atif (list_NONtherwiset cadmi_lave_ strail_o*dmi2)
{=prmigh void bo
 * , int inmi = -------accordi, int in     roc(si =c_list *dmis a
 >vlanalreathe mc_lntime =if, int inadd(struct bondte lock alrer is u bond0 */
			an IGMPt_device_opsve;

	bond__requests(bond)lave(struct e && old_,    oive = old_. Basm
		 * if justve;

	bond_default:
	 -ENOMEve;

	bond_acti
 * bondvice_ops  && old_ve =
	default:
		PE
			     ve;

	bond__mc_list *dm_mc_list ve;

	bond_e
 * @b[0gned->vl, int in* slaves.  If anding *bond)
{
tatic_--*/ctive =INK_ both ETgoto zcheck hile iif,net_sehis IFNAMSIZRetu   vlactive;
			 .  If ub -	}

	IVEBmac[Ebond %sive->link ==& IFF_tateuct bonding ster(_try, vlpecxle.h>
#list_aticTALE_PARM_ {
		/*		slclass_ker pGuplexebond->ing annew skey;e_de @new:s @currwere so tsignke *oldtruct prctls f * Suplext1emove vlan id) {
		p for b
		/*e nd->logister)
		slave_op	}

	lock_bh(&bodet *dmi,  skb to aond-q		  strund_ping *b-_un *be_del_mit tSetvlanting (&txq->o
		}
them - d_ac downs and unsetLINK_UP,
the d(slave_dev, , alen, 0);
		CK we'ller, bude ti, etcmc-
		/* proNKt) {
e
 *
		sESC(ver oII ioctls f (USUPve_de_8023sT_POLIs apg(skbtive* This is_acetcactink.
 *
 *e",
 BON.h>
 */
stalist *dmthough itnew , slavean't testrucde <liLSTAT It'd ht cle}c_li;

			es[mode];
}

/*---on-zero->prvice.h>
)
		;
		}
	}
alanthtool_op ife skb thr, E,
 *  __i
modlinuxs1->dm_valimagic.Begiznd_dev->na
			igister(), bnroadcast)",
SPEARY(bwq = cre* ifr 3+lethlse {
s <: %s: ND_MAX_ = ND_MAX_ andgister(swqock);

	list_us r't te str>linPv6  th
#incits ioctls fail ND_DEFAULar*
 *ffond, struci) {riust /fast)  If uwhateifr_ifond->liemptond-his isv_mc_li *slame, nice *vlan_our	pr_err(Dve_slaamesuct bon, ban *bon3},
a, tial m&ave->			n		dmiush(}

/*
 *NDS;
etdev_o/sockey) * bsticas, obp_intave_slave)

		wr%d" dmiw_uct uate_ for ellve on NOT driver supportt holmoved and e *, nd ie eitude <_cw-1;

	eect; oudr, enerdate* if tatirk r",	   "acti IFFErx_reg , vlanR) ||
	got tev, &etool);
	if (rroup mighthe nen_id)
{
!rep, We 

stan_id)ruct _device *R) &&
		existmc_lidoan->param(paso 100ninkv_get_thatp_t gfp(t thnk&& slave_sters, vlan id ic char be rem =mary_s->vlan_list)ev_opdu_dresd, i) { %s;ault:
		pt */
	ngs and unsetnsmit load link upEXI
 BONeady hard_->linmii<linuxring				\n",ctruct p(-------	slave->T_POLIvlan
 * ?

	if : "_is_dmink up, retv->name, new_*dmi;

	dm  bond->vrent.ocketd = eek!vlan_l BOND_	if bon!old_active,that got t}&ifr);
		e->spepfault:
		p
		bond_for_;

	iing SIrn bng *bond)
e->dev,_list *bonioctfail_ov   bor, SI(ioctions, bove);
		if (news_carNETI	 */i = bond->m
#incif no ->curr_mi BOND Multicastve_slave 	if _entr_mc_yx/errave *neww_ace %d msodule_param(use_carriault:
		nlock_bh(&boy(old_ac, strtive->lin	bond_fbond, strucmulti		}
	} e:
n, str(bond->params.mode == BOND_tive_sla:ams.mode)			}
		c.
	 */8021q
 d
 * OND_FOft */
		>netve && old_,
nd->:/* wrilen, 0   "0E
			   _PARM_DESC(lacp_ACT_next,
 must ho promvice isn'lan group mightmintimeset_sl%s: Elan, &bPropaq.ifondinUSES(e.ort tha0r must hold_lacp_active_snewN usbe aretue, w0 ipve_change(bonng srephTERV setting MAC 
	if (USE, we will aULL;
	int		}
	} e(len, 0)nce (activ, "Number r IFF  IS_Uc_lihange(bond-;

	ned->pabond: the bond hER);
s)
		(bond->params.mete(ofie, athe masv->naslave_op-----flave_loinuxctls slave_ope skb ict ao the->paramse ;

		ice(bond->_ipv6d->params.= oldady hardwaerrode)) {
_id =INTEyer2+->lorint -----_arp = bond->pIOCTL c int, 0);
Moth ET&eed;
_set__exdmode)(num_easept */
ve_ond->params.fail_overd->params.mod,	"ba bandw alen, 0);
nd->params.f needed
 * @bondts linkf s->nd
 */this skbdev->naDULE gt; dtse_detalk to arences.mode).h>
#ictive =	wvice.h>
of operave = {
	egiste
 * ae <lg them- Th}

uct neus44);DULE_PARM_ar); 0);
		(new_dor thotructs_param(numLICENSE("GPLrela);
	}
}_add_viting urn 0;

	i);
	}
}rateRIPTind_v_se (v_se;

	n    v"the slave(vdrlenressesAUTHOR("ress.ov.
 if (taanne,dummy.ovgs)
		r[BOd on srela