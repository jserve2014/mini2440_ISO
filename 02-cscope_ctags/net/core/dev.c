/*
 * 	NET3	Protocol independent device support routines.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	Derived from the non IP parts of dev.c 1.0.19
 * 		Authors:	Ross Biro
 *				Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *				Mark Evans, <evansmp@uhura.aston.ac.uk>
 *
 *	Additional Authors:
 *		Florian la Roche <rzsfl@rz.uni-sb.de>
 *		Alan Cox <gw4pts@gw4pts.ampr.org>
 *		David Hinds <dahinds@users.sourceforge.net>
 *		Alexey Kuznetsov <kuznet@ms2.inr.ac.ru>
 *		Adam Sulmicki <adam@cfar.umd.edu>
 *              Pekka Riikonen <priikone@poesidon.pspt.fi>
 *
 *	Changes:
 *              D.J. Barrow     :       Fixed bug where dev->refcnt gets set
 *              			to 2 if register_netdev gets called
 *              			before net_dev_init & also removed a
 *              			few lines of code in the process.
 *		Alan Cox	:	device private ioctl copies fields back.
 *		Alan Cox	:	Transmit queue code does relevant
 *					stunts to keep the queue safe.
 *		Alan Cox	:	Fixed double lock.
 *		Alan Cox	:	Fixed promisc NULL pointer trap
 *		????????	:	Support the full private ioctl range
 *		Alan Cox	:	Moved ioctl permission check into
 *					drivers
 *		Tim Kordas	:	SIOCADDMULTI/SIOCDELMULTI
 *		Alan Cox	:	100 backlog just doesn't cut it when
 *					you start doing multicast video 8)
 *		Alan Cox	:	Rewrote net_bh and list manager.
 *		Alan Cox	: 	Fix ETH_P_ALL echoback lengths.
 *		Alan Cox	:	Took out transmit every packet pass
 *					Saved a few bytes in the ioctl handler
 *		Alan Cox	:	Network driver sets packet type before
 *					calling netif_rx. Saves a function
 *					call a packet.
 *		Alan Cox	:	Hashed net_bh()
 *		Richard Kooijman:	Timestamp fixes.
 *		Alan Cox	:	Wrong field in SIOCGIFDSTADDR
 *		Alan Cox	:	Device lock protection.
 *		Alan Cox	: 	Fixed nasty side effect of device close
 *					changes.
 *		Rudi Cilibrasi	:	Pass the right thing to
 *					set_mac_address()
 *		Dave Miller	:	32bit quantity for the device lock to
 *					make it work out on a Sparc.
 *		Bjorn Ekwall	:	Added KERNELD hack.
 *		Alan Cox	:	Cleaned up the backlog initialise.
 *		Craig Metz	:	SIOCGIFCONF fix if space for under
 *					1 device.
 *	    Thomas Bogendoerfer :	Return ENODEV for dev_open, if there
 *					is no device open function.
 *		Andi Kleen	:	Fix error reporting for SIOCGIFCONF
 *	    Michael Chastain	:	Fix signed/unsigned for SIOCGIFCONF
 *		Cyrus Durgin	:	Cleaned for KMOD
 *		Adam Sulmicki   :	Bug Fix : Network Device Unload
 *					A network device unload needs to purge
 *					the backlog queue.
 *	Paul Rusty Russell	:	SIOCSIFNAME
 *              Pekka Riikonen  :	Netdev boot-time settings code
 *              Andrew Morton   :       Make unregister_netdevice wait
 *              			indefinitely on dev->refcnt
 * 		J Hadi Salim	:	- Backlog queue sampling
 *				        - netif_rx() feedback
 */

#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/bitops.h>
#include <linux/capability.h>
#include <linux/cpu.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/notifier.h>
#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <net/sock.h>
#include <linux/rtnetlink.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/if_bridge.h>
#include <linux/if_macvlan.h>
#include <net/dst.h>
#include <net/pkt_sched.h>
#include <net/checksum.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/netpoll.h>
#include <linux/rcupdate.h>
#include <linux/delay.h>
#include <net/wext.h>
#include <net/iw_handler.h>
#include <asm/current.h>
#include <linux/audit.h>
#include <linux/dmaengine.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/if_arp.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/ipv6.h>
#include <linux/in.h>
#include <linux/jhash.h>
#include <linux/random.h>
#include <trace/events/napi.h>

#include "net-sysfs.h"

/* Instead of increasing this, you should create a hash table. */
#define MAX_GRO_SKBS 8

/* This should be increased if a protocol with a bigger head is added. */
#define GRO_MAX_HEAD (MAX_HEADER + 128)

/*
 *	The list of packet types we will receive (as opposed to discard)
 *	and the routines to invoke.
 *
 *	Why 16. Because with 16 the only overlap we get on a hash of the
 *	low nibble of the protocol value is RARP/SNAP/X.25.
 *
 *      NOTE:  That is no longer true with the addition of VLAN tags.  Not
 *             sure which should go first, but I bet it won't make much
 *             difference if we are running VLANs.  The good news is that
 *             this protocol won't be in the list unless compiled in, so
 *             the average user (w/out VLANs) will not be adversely affected.
 *             --BLG
 *
 *		0800	IP
 *		8100    802.1Q VLAN
 *		0001	802.3
 *		0002	AX.25
 *		0004	802.2
 *		8035	RARP
 *		0005	SNAP
 *		0805	X.25
 *		0806	ARP
 *		8137	IPX
 *		0009	Localtalk
 *		86DD	IPv6
 */

#define PTYPE_HASH_SIZE	(16)
#define PTYPE_HASH_MASK	(PTYPE_HASH_SIZE - 1)

static DEFINE_SPINLOCK(ptype_lock);
static struct list_head ptype_base[PTYPE_HASH_SIZE] __read_mostly;
static struct list_head ptype_all __read_mostly;	/* Taps */

/*
 * The @dev_base_head list is protected by @dev_base_lock and the rtnl
 * semaphore.
 *
 * Pure readers hold dev_base_lock for reading.
 *
 * Writers must hold the rtnl semaphore while they loop through the
 * dev_base_head list, and hold dev_base_lock for writing when they do the
 * actual updates.  This allows pure readers to access the list even
 * while a writer is preparing to update it.
 *
 * To put it another way, dev_base_lock is held for writing only to
 * protect against pure readers; the rtnl semaphore provides the
 * protection against other writers.
 *
 * See, for example usages, register_netdevice() and
 * unregister_netdevice(), which must be called with the rtnl
 * semaphore held.
 */
DEFINE_RWLOCK(dev_base_lock);
EXPORT_SYMBOL(dev_base_lock);

#define NETDEV_HASHBITS	8
#define NETDEV_HASHENTRIES (1 << NETDEV_HASHBITS)

static inline struct hlist_head *dev_name_hash(struct net *net, const char *name)
{
	unsigned hash = full_name_hash(name, strnlen(name, IFNAMSIZ));
	return &net->dev_name_head[hash & ((1 << NETDEV_HASHBITS) - 1)];
}

static inline struct hlist_head *dev_index_hash(struct net *net, int ifindex)
{
	return &net->dev_index_head[ifindex & ((1 << NETDEV_HASHBITS) - 1)];
}

/* Device list insertion */
static int list_netdevice(struct net_device *dev)
{
	struct net *net = dev_net(dev);

	ASSERT_RTNL();

	write_lock_bh(&dev_base_lock);
	list_add_tail(&dev->dev_list, &net->dev_base_head);
	hlist_add_head(&dev->name_hlist, dev_name_hash(net, dev->name));
	hlist_add_head(&dev->index_hlist, dev_index_hash(net, dev->ifindex));
	write_unlock_bh(&dev_base_lock);
	return 0;
}

/* Device list removal */
static void unlist_netdevice(struct net_device *dev)
{
	ASSERT_RTNL();

	/* Unlink dev from the device chain */
	write_lock_bh(&dev_base_lock);
	list_del(&dev->dev_list);
	hlist_del(&dev->name_hlist);
	hlist_del(&dev->index_hlist);
	write_unlock_bh(&dev_base_lock);
}

/*
 *	Our notifier list
 */

static RAW_NOTIFIER_HEAD(netdev_chain);

/*
 *	Device drivers call our routines to queue packets here. We empty the
 *	queue in the local softnet handler.
 */

DEFINE_PER_CPU(struct softnet_data, softnet_data);
EXPORT_PER_CPU_SYMBOL(softnet_data);

#ifdef CONFIG_LOCKDEP
/*
 * register_netdevice() inits txq->_xmit_lock and sets lockdep class
 * according to dev->type
 */
static const unsigned short netdev_lock_type[] =
	{ARPHRD_NETROM, ARPHRD_ETHER, ARPHRD_EETHER, ARPHRD_AX25,
	 ARPHRD_PRONET, ARPHRD_CHAOS, ARPHRD_IEEE802, ARPHRD_ARCNET,
	 ARPHRD_APPLETLK, ARPHRD_DLCI, ARPHRD_ATM, ARPHRD_METRICOM,
	 ARPHRD_IEEE1394, ARPHRD_EUI64, ARPHRD_INFINIBAND, ARPHRD_SLIP,
	 ARPHRD_CSLIP, ARPHRD_SLIP6, ARPHRD_CSLIP6, ARPHRD_RSRVD,
	 ARPHRD_ADAPT, ARPHRD_ROSE, ARPHRD_X25, ARPHRD_HWX25,
	 ARPHRD_PPP, ARPHRD_CISCO, ARPHRD_LAPB, ARPHRD_DDCMP,
	 ARPHRD_RAWHDLC, ARPHRD_TUNNEL, ARPHRD_TUNNEL6, ARPHRD_FRAD,
	 ARPHRD_SKIP, ARPHRD_LOOPBACK, ARPHRD_LOCALTLK, ARPHRD_FDDI,
	 ARPHRD_BIF, ARPHRD_SIT, ARPHRD_IPDDP, ARPHRD_IPGRE,
	 ARPHRD_PIMREG, ARPHRD_HIPPI, ARPHRD_ASH, ARPHRD_ECONET,
	 ARPHRD_IRDA, ARPHRD_FCPP, ARPHRD_FCAL, ARPHRD_FCPL,
	 ARPHRD_FCFABRIC, ARPHRD_IEEE802_TR, ARPHRD_IEEE80211,
	 ARPHRD_IEEE80211_PRISM, ARPHRD_IEEE80211_RADIOTAP, ARPHRD_PHONET,
	 ARPHRD_PHONET_PIPE, ARPHRD_IEEE802154,
	 ARPHRD_VOID, ARPHRD_NONE};

static const char *const netdev_lock_name[] =
	{"_xmit_NETROM", "_xmit_ETHER", "_xmit_EETHER", "_xmit_AX25",
	 "_xmit_PRONET", "_xmit_CHAOS", "_xmit_IEEE802", "_xmit_ARCNET",
	 "_xmit_APPLETLK", "_xmit_DLCI", "_xmit_ATM", "_xmit_METRICOM",
	 "_xmit_IEEE1394", "_xmit_EUI64", "_xmit_INFINIBAND", "_xmit_SLIP",
	 "_xmit_CSLIP", "_xmit_SLIP6", "_xmit_CSLIP6", "_xmit_RSRVD",
	 "_xmit_ADAPT", "_xmit_ROSE", "_xmit_X25", "_xmit_HWX25",
	 "_xmit_PPP", "_xmit_CISCO", "_xmit_LAPB", "_xmit_DDCMP",
	 "_xmit_RAWHDLC", "_xmit_TUNNEL", "_xmit_TUNNEL6", "_xmit_FRAD",
	 "_xmit_SKIP", "_xmit_LOOPBACK", "_xmit_LOCALTLK", "_xmit_FDDI",
	 "_xmit_BIF", "_xmit_SIT", "_xmit_IPDDP", "_xmit_IPGRE",
	 "_xmit_PIMREG", "_xmit_HIPPI", "_xmit_ASH", "_xmit_ECONET",
	 "_xmit_IRDA", "_xmit_FCPP", "_xmit_FCAL", "_xmit_FCPL",
	 "_xmit_FCFABRIC", "_xmit_IEEE802_TR", "_xmit_IEEE80211",
	 "_xmit_IEEE80211_PRISM", "_xmit_IEEE80211_RADIOTAP", "_xmit_PHONET",
	 "_xmit_PHONET_PIPE", "_xmit_IEEE802154",
	 "_xmit_VOID", "_xmit_NONE"};

static struct lock_class_key netdev_xmit_lock_key[ARRAY_SIZE(netdev_lock_type)];
static struct lock_class_key netdev_addr_lock_key[ARRAY_SIZE(netdev_lock_type)];

static inline unsigned short netdev_lock_pos(unsigned short dev_type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(netdev_lock_type); i++)
		if (netdev_lock_type[i] == dev_type)
			return i;
	/* the last key is used by default */
	return ARRAY_SIZE(netdev_lock_type) - 1;
}

static inline void netdev_set_xmit_lockdep_class(spinlock_t *lock,
						 unsigned short dev_type)
{
	int i;

	i = netdev_lock_pos(dev_type);
	lockdep_set_class_and_name(lock, &netdev_xmit_lock_key[i],
				   netdev_lock_name[i]);
}

static inline void netdev_set_addr_lockdep_class(struct net_device *dev)
{
	int i;

	i = netdev_lock_pos(dev->type);
	lockdep_set_class_and_name(&dev->addr_list_lock,
				   &netdev_addr_lock_key[i],
				   netdev_lock_name[i]);
}
#else
static inline void netdev_set_xmit_lockdep_class(spinlock_t *lock,
						 unsigned short dev_type)
{
}
static inline void netdev_set_addr_lockdep_class(struct net_device *dev)
{
}
#endif

/*******************************************************************************

		Protocol management and registration routines

*******************************************************************************/

/*
 *	Add a protocol ID to the list. Now that the input handler is
 *	smarter we can dispense with all the messy stuff that used to be
 *	here.
 *
 *	BEWARE!!! Protocol handlers, mangling input packets,
 *	MUST BE last in hash buckets and checking protocol handlers
 *	MUST start from promiscuous ptype_all chain in net_bh.
 *	It is true now, do not change it.
 *	Explanation follows: if protocol handler, mangling packet, will
 *	be the first on list, it is not able to sense, that packet
 *	is cloned and should be copied-on-write, so that it will
 *	change it and subsequent readers will get broken packet.
 *							--ANK (980803)
 */

/**
 *	dev_add_pack - add packet handler
 *	@pt: packet type declaration
 *
 *	Add a protocol handler to the networking stack. The passed &packet_type
 *	is linked into kernel lists and may not be freed until it has been
 *	removed from the kernel lists.
 *
 *	This call does not sleep therefore it can not
 *	guarantee all CPU's that are in middle of receiving packets
 *	will see the new packet type (until the next received packet).
 */

void dev_add_pack(struct packet_type *pt)
{
	int hash;

	spin_lock_bh(&ptype_lock);
	if (pt->type == htons(ETH_P_ALL))
		list_add_rcu(&pt->list, &ptype_all);
	else {
		hash = ntohs(pt->type) & PTYPE_HASH_MASK;
		list_add_rcu(&pt->list, &ptype_base[hash]);
	}
	spin_unlock_bh(&ptype_lock);
}
EXPORT_SYMBOL(dev_add_pack);

/**
 *	__dev_remove_pack	 - remove packet handler
 *	@pt: packet type declaration
 *
 *	Remove a protocol handler that was previously added to the kernel
 *	protocol handlers by dev_add_pack(). The passed &packet_type is removed
 *	from the kernel lists and can be freed or reused once this function
 *	returns.
 *
 *      The packet type might still be in use by receivers
 *	and must not be freed until after all the CPU's have gone
 *	through a quiescent state.
 */
void __dev_remove_pack(struct packet_type *pt)
{
	struct list_head *head;
	struct packet_type *pt1;

	spin_lock_bh(&ptype_lock);

	if (pt->type == htons(ETH_P_ALL))
		head = &ptype_all;
	else
		head = &ptype_base[ntohs(pt->type) & PTYPE_HASH_MASK];

	list_for_each_entry(pt1, head, list) {
		if (pt == pt1) {
			list_del_rcu(&pt->list);
			goto out;
		}
	}

	printk(KERN_WARNING "dev_remove_pack: %p not found.\n", pt);
out:
	spin_unlock_bh(&ptype_lock);
}
EXPORT_SYMBOL(__dev_remove_pack);

/**
 *	dev_remove_pack	 - remove packet handler
 *	@pt: packet type declaration
 *
 *	Remove a protocol handler that was previously added to the kernel
 *	protocol handlers by dev_add_pack(). The passed &packet_type is removed
 *	from the kernel lists and can be freed or reused once this function
 *	returns.
 *
 *	This call sleeps to guarantee that no CPU is looking at the packet
 *	type after return.
 */
void dev_remove_pack(struct packet_type *pt)
{
	__dev_remove_pack(pt);

	synchronize_net();
}
EXPORT_SYMBOL(dev_remove_pack);

/******************************************************************************

		      Device Boot-time Settings Routines

*******************************************************************************/

/* Boot time configuration table */
static struct netdev_boot_setup dev_boot_setup[NETDEV_BOOT_SETUP_MAX];

/**
 *	netdev_boot_setup_add	- add new setup entry
 *	@name: name of the device
 *	@map: configured settings for the device
 *
 *	Adds new setup entry to the dev_boot_setup list.  The function
 *	returns 0 on error and 1 on success.  This is a generic routine to
 *	all netdevices.
 */
static int netdev_boot_setup_add(char *name, struct ifmap *map)
{
	struct netdev_boot_setup *s;
	int i;

	s = dev_boot_setup;
	for (i = 0; i < NETDEV_BOOT_SETUP_MAX; i++) {
		if (s[i].name[0] == '\0' || s[i].name[0] == ' ') {
			memset(s[i].name, 0, sizeof(s[i].name));
			strlcpy(s[i].name, name, IFNAMSIZ);
			memcpy(&s[i].map, map, sizeof(s[i].map));
			break;
		}
	}

	return i >= NETDEV_BOOT_SETUP_MAX ? 0 : 1;
}

/**
 *	netdev_boot_setup_check	- check boot time settings
 *	@dev: the netdevice
 *
 * 	Check boot time settings for the device.
 *	The found settings are set for the device to be used
 *	later in the device probing.
 *	Returns 0 if no settings found, 1 if they are.
 */
int netdev_boot_setup_check(struct net_device *dev)
{
	struct netdev_boot_setup *s = dev_boot_setup;
	int i;

	for (i = 0; i < NETDEV_BOOT_SETUP_MAX; i++) {
		if (s[i].name[0] != '\0' && s[i].name[0] != ' ' &&
		    !strcmp(dev->name, s[i].name)) {
			dev->irq 	= s[i].map.irq;
			dev->base_addr 	= s[i].map.base_addr;
			dev->mem_start 	= s[i].map.mem_start;
			dev->mem_end 	= s[i].map.mem_end;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(netdev_boot_setup_check);


/**
 *	netdev_boot_base	- get address from boot time settings
 *	@prefix: prefix for network device
 *	@unit: id for network device
 *
 * 	Check boot time settings for the base address of device.
 *	The found settings are set for the device to be used
 *	later in the device probing.
 *	Returns 0 if no settings found.
 */
unsigned long netdev_boot_base(const char *prefix, int unit)
{
	const struct netdev_boot_setup *s = dev_boot_setup;
	char name[IFNAMSIZ];
	int i;

	sprintf(name, "%s%d", prefix, unit);

	/*
	 * If device already registered then return base of 1
	 * to indicate not to probe for this interface
	 */
	if (__dev_get_by_name(&init_net, name))
		return 1;

	for (i = 0; i < NETDEV_BOOT_SETUP_MAX; i++)
		if (!strcmp(name, s[i].name))
			return s[i].map.base_addr;
	return 0;
}

/*
 * Saves at boot time configured settings for any netdevice.
 */
int __init netdev_boot_setup(char *str)
{
	int ints[5];
	struct ifmap map;

	str = get_options(str, ARRAY_SIZE(ints), ints);
	if (!str || !*str)
		return 0;

	/* Save settings */
	memset(&map, 0, sizeof(map));
	if (ints[0] > 0)
		map.irq = ints[1];
	if (ints[0] > 1)
		map.base_addr = ints[2];
	if (ints[0] > 2)
		map.mem_start = ints[3];
	if (ints[0] > 3)
		map.mem_end = ints[4];

	/* Add new entry to the list */
	return netdev_boot_setup_add(str, &map);
}

__setup("netdev=", netdev_boot_setup);

/*******************************************************************************

			    Device Interface Subroutines

*******************************************************************************/

/**
 *	__dev_get_by_name	- find a device by its name
 *	@net: the applicable net namespace
 *	@name: name to find
 *
 *	Find an interface by name. Must be called under RTNL semaphore
 *	or @dev_base_lock. If the name is found a pointer to the device
 *	is returned. If the name is not found then %NULL is returned. The
 *	reference counters are not incremented so the caller must be
 *	careful with locks.
 */

struct net_device *__dev_get_by_name(struct net *net, const char *name)
{
	struct hlist_node *p;

	hlist_for_each(p, dev_name_hash(net, name)) {
		struct net_device *dev
			= hlist_entry(p, struct net_device, name_hlist);
		if (!strncmp(dev->name, name, IFNAMSIZ))
			return dev;
	}
	return NULL;
}
EXPORT_SYMBOL(__dev_get_by_name);

/**
 *	dev_get_by_name		- find a device by its name
 *	@net: the applicable net namespace
 *	@name: name to find
 *
 *	Find an interface by name. This can be called from any
 *	context and does its own locking. The returned handle has
 *	the usage count incremented and the caller must use dev_put() to
 *	release it when it is no longer needed. %NULL is returned if no
 *	matching device is found.
 */

struct net_device *dev_get_by_name(struct net *net, const char *name)
{
	struct net_device *dev;

	read_lock(&dev_base_lock);
	dev = __dev_get_by_name(net, name);
	if (dev)
		dev_hold(dev);
	read_unlock(&dev_base_lock);
	return dev;
}
EXPORT_SYMBOL(dev_get_by_name);

/**
 *	__dev_get_by_index - find a device by its ifindex
 *	@net: the applicable net namespace
 *	@ifindex: index of device
 *
 *	Search for an interface by index. Returns %NULL if the device
 *	is not found or a pointer to the device. The device has not
 *	had its reference counter increased so the caller must be careful
 *	about locking. The caller must hold either the RTNL semaphore
 *	or @dev_base_lock.
 */

struct net_device *__dev_get_by_index(struct net *net, int ifindex)
{
	struct hlist_node *p;

	hlist_for_each(p, dev_index_hash(net, ifindex)) {
		struct net_device *dev
			= hlist_entry(p, struct net_device, index_hlist);
		if (dev->ifindex == ifindex)
			return dev;
	}
	return NULL;
}
EXPORT_SYMBOL(__dev_get_by_index);


/**
 *	dev_get_by_index - find a device by its ifindex
 *	@net: the applicable net namespace
 *	@ifindex: index of device
 *
 *	Search for an interface by index. Returns NULL if the device
 *	is not found or a pointer to the device. The device returned has
 *	had a reference added and the pointer is safe until the user calls
 *	dev_put to indicate they have finished with it.
 */

struct net_device *dev_get_by_index(struct net *net, int ifindex)
{
	struct net_device *dev;

	read_lock(&dev_base_lock);
	dev = __dev_get_by_index(net, ifindex);
	if (dev)
		dev_hold(dev);
	read_unlock(&dev_base_lock);
	return dev;
}
EXPORT_SYMBOL(dev_get_by_index);

/**
 *	dev_getbyhwaddr - find a device by its hardware address
 *	@net: the applicable net namespace
 *	@type: media type of device
 *	@ha: hardware address
 *
 *	Search for an interface by MAC address. Returns NULL if the device
 *	is not found or a pointer to the device. The caller must hold the
 *	rtnl semaphore. The returned device has not had its ref count increased
 *	and the caller must therefore be careful about locking
 *
 *	BUGS:
 *	If the API was consistent this would be __dev_get_by_hwaddr
 */

struct net_device *dev_getbyhwaddr(struct net *net, unsigned short type, char *ha)
{
	struct net_device *dev;

	ASSERT_RTNL();

	for_each_netdev(net, dev)
		if (dev->type == type &&
		    !memcmp(dev->dev_addr, ha, dev->addr_len))
			return dev;

	return NULL;
}
EXPORT_SYMBOL(dev_getbyhwaddr);

struct net_device *__dev_getfirstbyhwtype(struct net *net, unsigned short type)
{
	struct net_device *dev;

	ASSERT_RTNL();
	for_each_netdev(net, dev)
		if (dev->type == type)
			return dev;

	return NULL;
}
EXPORT_SYMBOL(__dev_getfirstbyhwtype);

struct net_device *dev_getfirstbyhwtype(struct net *net, unsigned short type)
{
	struct net_device *dev;

	rtnl_lock();
	dev = __dev_getfirstbyhwtype(net, type);
	if (dev)
		dev_hold(dev);
	rtnl_unlock();
	return dev;
}
EXPORT_SYMBOL(dev_getfirstbyhwtype);

/**
 *	dev_get_by_flags - find any device with given flags
 *	@net: the applicable net namespace
 *	@if_flags: IFF_* values
 *	@mask: bitmask of bits in if_flags to check
 *
 *	Search for any interface with the given flags. Returns NULL if a device
 *	is not found or a pointer to the device. The device returned has
 *	had a reference added and the pointer is safe until the user calls
 *	dev_put to indicate they have finished with it.
 */

struct net_device *dev_get_by_flags(struct net *net, unsigned short if_flags,
				    unsigned short mask)
{
	struct net_device *dev, *ret;

	ret = NULL;
	read_lock(&dev_base_lock);
	for_each_netdev(net, dev) {
		if (((dev->flags ^ if_flags) & mask) == 0) {
			dev_hold(dev);
			ret = dev;
			break;
		}
	}
	read_unlock(&dev_base_lock);
	return ret;
}
EXPORT_SYMBOL(dev_get_by_flags);

/**
 *	dev_valid_name - check if name is okay for network device
 *	@name: name string
 *
 *	Network device names need to be valid file names to
 *	to allow sysfs to work.  We also disallow any kind of
 *	whitespace.
 */
int dev_valid_name(const char *name)
{
	if (*name == '\0')
		return 0;
	if (strlen(name) >= IFNAMSIZ)
		return 0;
	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return 0;

	while (*name) {
		if (*name == '/' || isspace(*name))
			return 0;
		name++;
	}
	return 1;
}
EXPORT_SYMBOL(dev_valid_name);

/**
 *	__dev_alloc_name - allocate a name for a device
 *	@net: network namespace to allocate the device name in
 *	@name: name format string
 *	@buf:  scratch buffer and result name string
 *
 *	Passed a format string - eg "lt%d" it will try and find a suitable
 *	id. It scans list of devices to build up a free map, then chooses
 *	the first empty slot. The caller must hold the dev_base or rtnl lock
 *	while allocating the name and adding the device in order to avoid
 *	duplicates.
 *	Limited to bits_per_byte * page size devices (ie 32K on most platforms).
 *	Returns the number of the unit assigned or a negative errno code.
 */

static int __dev_alloc_name(struct net *net, const char *name, char *buf)
{
	int i = 0;
	const char *p;
	const int max_netdevices = 8*PAGE_SIZE;
	unsigned long *inuse;
	struct net_device *d;

	p = strnchr(name, IFNAMSIZ-1, '%');
	if (p) {
		/*
		 * Verify the string as this thing may have come from
		 * the user.  There must be either one "%d" and no other "%"
		 * characters.
		 */
		if (p[1] != 'd' || strchr(p + 2, '%'))
			return -EINVAL;

		/* Use one page as a bit array of possible slots */
		inuse = (unsigned long *) get_zeroed_page(GFP_ATOMIC);
		if (!inuse)
			return -ENOMEM;

		for_each_netdev(net, d) {
			if (!sscanf(d->name, name, &i))
				continue;
			if (i < 0 || i >= max_netdevices)
				continue;

			/*  avoid cases where sscanf is not exact inverse of printf */
			snprintf(buf, IFNAMSIZ, name, i);
			if (!strncmp(buf, d->name, IFNAMSIZ))
				set_bit(i, inuse);
		}

		i = find_first_zero_bit(inuse, max_netdevices);
		free_page((unsigned long) inuse);
	}

	snprintf(buf, IFNAMSIZ, name, i);
	if (!__dev_get_by_name(net, buf))
		return i;

	/* It is possible to run out of possible slots
	 * when the name is long and there isn't enough space left
	 * for the digits, or if all bits are used.
	 */
	return -ENFILE;
}

/**
 *	dev_alloc_name - allocate a name for a device
 *	@dev: device
 *	@name: name format string
 *
 *	Passed a format string - eg "lt%d" it will try and find a suitable
 *	id. It scans list of devices to build up a free map, then chooses
 *	the first empty slot. The caller must hold the dev_base or rtnl lock
 *	while allocating the name and adding the device in order to avoid
 *	duplicates.
 *	Limited to bits_per_byte * page size devices (ie 32K on most platforms).
 *	Returns the number of the unit assigned or a negative errno code.
 */

int dev_alloc_name(struct net_device *dev, const char *name)
{
	char buf[IFNAMSIZ];
	struct net *net;
	int ret;

	BUG_ON(!dev_net(dev));
	net = dev_net(dev);
	ret = __dev_alloc_name(net, name, buf);
	if (ret >= 0)
		strlcpy(dev->name, buf, IFNAMSIZ);
	return ret;
}
EXPORT_SYMBOL(dev_alloc_name);


/**
 *	dev_change_name - change name of a device
 *	@dev: device
 *	@newname: name (or format string) must be at least IFNAMSIZ
 *
 *	Change name of a device, can pass format strings "eth%d".
 *	for wildcarding.
 */
int dev_change_name(struct net_device *dev, const char *newname)
{
	char oldname[IFNAMSIZ];
	int err = 0;
	int ret;
	struct net *net;

	ASSERT_RTNL();
	BUG_ON(!dev_net(dev));

	net = dev_net(dev);
	if (dev->flags & IFF_UP)
		return -EBUSY;

	if (!dev_valid_name(newname))
		return -EINVAL;

	if (strncmp(newname, dev->name, IFNAMSIZ) == 0)
		return 0;

	memcpy(oldname, dev->name, IFNAMSIZ);

	if (strchr(newname, '%')) {
		err = dev_alloc_name(dev, newname);
		if (err < 0)
			return err;
	} else if (__dev_get_by_name(net, newname))
		return -EEXIST;
	else
		strlcpy(dev->name, newname, IFNAMSIZ);

rollback:
	/* For now only devices in the initial network namespace
	 * are in sysfs.
	 */
	if (net == &init_net) {
		ret = device_rename(&dev->dev, dev->name);
		if (ret) {
			memcpy(dev->name, oldname, IFNAMSIZ);
			return ret;
		}
	}

	write_lock_bh(&dev_base_lock);
	hlist_del(&dev->name_hlist);
	hlist_add_head(&dev->name_hlist, dev_name_hash(net, dev->name));
	write_unlock_bh(&dev_base_lock);

	ret = call_netdevice_notifiers(NETDEV_CHANGENAME, dev);
	ret = notifier_to_errno(ret);

	if (ret) {
		/* err >= 0 after dev_alloc_name() or stores the first errno */
		if (err >= 0) {
			err = ret;
			memcpy(dev->name, oldname, IFNAMSIZ);
			goto rollback;
		} else {
			printk(KERN_ERR
			       "%s: name change rollback failed: %d.\n",
			       dev->name, ret);
		}
	}

	return err;
}

/**
 *	dev_set_alias - change ifalias of a device
 *	@dev: device
 *	@alias: name up to IFALIASZ
 *	@len: limit of bytes to copy from info
 *
 *	Set ifalias for a device,
 */
int dev_set_alias(struct net_device *dev, const char *alias, size_t len)
{
	ASSERT_RTNL();

	if (len >= IFALIASZ)
		return -EINVAL;

	if (!len) {
		if (dev->ifalias) {
			kfree(dev->ifalias);
			dev->ifalias = NULL;
		}
		return 0;
	}

	dev->ifalias = krealloc(dev->ifalias, len + 1, GFP_KERNEL);
	if (!dev->ifalias)
		return -ENOMEM;

	strlcpy(dev->ifalias, alias, len+1);
	return len;
}


/**
 *	netdev_features_change - device changes features
 *	@dev: device to cause notification
 *
 *	Called to indicate a device has changed features.
 */
void netdev_features_change(struct net_device *dev)
{
	call_netdevice_notifiers(NETDEV_FEAT_CHANGE, dev);
}
EXPORT_SYMBOL(netdev_features_change);

/**
 *	netdev_state_change - device changes state
 *	@dev: device to cause notification
 *
 *	Called to indicate a device has changed state. This function calls
 *	the notifier chains for netdev_chain and sends a NEWLINK message
 *	to the routing socket.
 */
void netdev_state_change(struct net_device *dev)
{
	if (dev->flags & IFF_UP) {
		call_netdevice_notifiers(NETDEV_CHANGE, dev);
		rtmsg_ifinfo(RTM_NEWLINK, dev, 0);
	}
}
EXPORT_SYMBOL(netdev_state_change);

void netdev_bonding_change(struct net_device *dev, unsigned long event)
{
	call_netdevice_notifiers(event, dev);
}
EXPORT_SYMBOL(netdev_bonding_change);

/**
 *	dev_load 	- load a network module
 *	@net: the applicable net namespace
 *	@name: name of interface
 *
 *	If a network interface is not present and the process has suitable
 *	privileges this function loads the module. If module loading is not
 *	available in this kernel then it becomes a nop.
 */

void dev_load(struct net *net, const char *name)
{
	struct net_device *dev;

	read_lock(&dev_base_lock);
	dev = __dev_get_by_name(net, name);
	read_unlock(&dev_base_lock);

	if (!dev && capable(CAP_NET_ADMIN))
		request_module("%s", name);
}
EXPORT_SYMBOL(dev_load);

/**
 *	dev_open	- prepare an interface for use.
 *	@dev:	device to open
 *
 *	Takes a device from down to up state. The device's private open
 *	function is invoked and then the multicast lists are loaded. Finally
 *	the device is moved into the up state and a %NETDEV_UP message is
 *	sent to the netdev notifier chain.
 *
 *	Calling this function on an active interface is a nop. On a failure
 *	a negative errno code is returned.
 */
int dev_open(struct net_device *dev)
{
	const struct net_device_ops *ops = dev->netdev_ops;
	int ret;

	ASSERT_RTNL();

	/*
	 *	Is it already up?
	 */

	if (dev->flags & IFF_UP)
		return 0;

	/*
	 *	Is it even present?
	 */
	if (!netif_device_present(dev))
		return -ENODEV;

	ret = call_netdevice_notifiers(NETDEV_PRE_UP, dev);
	ret = notifier_to_errno(ret);
	if (ret)
		return ret;

	/*
	 *	Call device private open method
	 */
	set_bit(__LINK_STATE_START, &dev->state);

	if (ops->ndo_validate_addr)
		ret = ops->ndo_validate_addr(dev);

	if (!ret && ops->ndo_open)
		ret = ops->ndo_open(dev);

	/*
	 *	If it went open OK then:
	 */

	if (ret)
		clear_bit(__LINK_STATE_START, &dev->state);
	else {
		/*
		 *	Set the flags.
		 */
		dev->flags |= IFF_UP;

		/*
		 *	Enable NET_DMA
		 */
		net_dmaengine_get();

		/*
		 *	Initialize multicasting status
		 */
		dev_set_rx_mode(dev);

		/*
		 *	Wakeup transmit queue engine
		 */
		dev_activate(dev);

		/*
		 *	... and announce new interface.
		 */
		call_netdevice_notifiers(NETDEV_UP, dev);
	}

	return ret;
}
EXPORT_SYMBOL(dev_open);

/**
 *	dev_close - shutdown an interface.
 *	@dev: device to shutdown
 *
 *	This function moves an active device into down state. A
 *	%NETDEV_GOING_DOWN is sent to the netdev notifier chain. The device
 *	is then deactivated and finally a %NETDEV_DOWN is sent to the notifier
 *	chain.
 */
int dev_close(struct net_device *dev)
{
	const struct net_device_ops *ops = dev->netdev_ops;
	ASSERT_RTNL();

	might_sleep();

	if (!(dev->flags & IFF_UP))
		return 0;

	/*
	 *	Tell people we are going down, so that they can
	 *	prepare to death, when device is still operating.
	 */
	call_netdevice_notifiers(NETDEV_GOING_DOWN, dev);

	clear_bit(__LINK_STATE_START, &dev->state);

	/* Synchronize to scheduled poll. We cannot touch poll list,
	 * it can be even on different cpu. So just clear netif_running().
	 *
	 * dev->stop() will invoke napi_disable() on all of it's
	 * napi_struct instances on this device.
	 */
	smp_mb__after_clear_bit(); /* Commit netif_running(). */

	dev_deactivate(dev);

	/*
	 *	Call the device specific close. This cannot fail.
	 *	Only if device is UP
	 *
	 *	We allow it to be called even after a DETACH hot-plug
	 *	event.
	 */
	if (ops->ndo_stop)
		ops->ndo_stop(dev);

	/*
	 *	Device is now down.
	 */

	dev->flags &= ~IFF_UP;

	/*
	 * Tell people we are down
	 */
	call_netdevice_notifiers(NETDEV_DOWN, dev);

	/*
	 *	Shutdown NET_DMA
	 */
	net_dmaengine_put();

	return 0;
}
EXPORT_SYMBOL(dev_close);


/**
 *	dev_disable_lro - disable Large Receive Offload on a device
 *	@dev: device
 *
 *	Disable Large Receive Offload (LRO) on a net device.  Must be
 *	called under RTNL.  This is needed if received packets may be
 *	forwarded to another interface.
 */
void dev_disable_lro(struct net_device *dev)
{
	if (dev->ethtool_ops && dev->ethtool_ops->get_flags &&
	    dev->ethtool_ops->set_flags) {
		u32 flags = dev->ethtool_ops->get_flags(dev);
		if (flags & ETH_FLAG_LRO) {
			flags &= ~ETH_FLAG_LRO;
			dev->ethtool_ops->set_flags(dev, flags);
		}
	}
	WARN_ON(dev->features & NETIF_F_LRO);
}
EXPORT_SYMBOL(dev_disable_lro);


static int dev_boot_phase = 1;

/*
 *	Device change register/unregister. These are not inline or static
 *	as we export them to the world.
 */

/**
 *	register_netdevice_notifier - register a network notifier block
 *	@nb: notifier
 *
 *	Register a notifier to be called when network device events occur.
 *	The notifier passed is linked into the kernel structures and must
 *	not be reused until it has been unregistered. A negative errno code
 *	is returned on a failure.
 *
 * 	When registered all registration and up events are replayed
 *	to the new notifier to allow device to have a race free
 *	view of the network device list.
 */

int register_netdevice_notifier(struct notifier_block *nb)
{
	struct net_device *dev;
	struct net_device *last;
	struct net *net;
	int err;

	rtnl_lock();
	err = raw_notifier_chain_register(&netdev_chain, nb);
	if (err)
		goto unlock;
	if (dev_boot_phase)
		goto unlock;
	for_each_net(net) {
		for_each_netdev(net, dev) {
			err = nb->notifier_call(nb, NETDEV_REGISTER, dev);
			err = notifier_to_errno(err);
			if (err)
				goto rollback;

			if (!(dev->flags & IFF_UP))
				continue;

			nb->notifier_call(nb, NETDEV_UP, dev);
		}
	}

unlock:
	rtnl_unlock();
	return err;

rollback:
	last = dev;
	for_each_net(net) {
		for_each_netdev(net, dev) {
			if (dev == last)
				break;

			if (dev->flags & IFF_UP) {
				nb->notifier_call(nb, NETDEV_GOING_DOWN, dev);
				nb->notifier_call(nb, NETDEV_DOWN, dev);
			}
			nb->notifier_call(nb, NETDEV_UNREGISTER, dev);
		}
	}

	raw_notifier_chain_unregister(&netdev_chain, nb);
	goto unlock;
}
EXPORT_SYMBOL(register_netdevice_notifier);

/**
 *	unregister_netdevice_notifier - unregister a network notifier block
 *	@nb: notifier
 *
 *	Unregister a notifier previously registered by
 *	register_netdevice_notifier(). The notifier is unlinked into the
 *	kernel structures and may then be reused. A negative errno code
 *	is returned on a failure.
 */

int unregister_netdevice_notifier(struct notifier_block *nb)
{
	int err;

	rtnl_lock();
	err = raw_notifier_chain_unregister(&netdev_chain, nb);
	rtnl_unlock();
	return err;
}
EXPORT_SYMBOL(unregister_netdevice_notifier);

/**
 *	call_netdevice_notifiers - call all network notifier blocks
 *      @val: value passed unmodified to notifier function
 *      @dev: net_device pointer passed unmodified to notifier function
 *
 *	Call all network notifier blocks.  Parameters and return value
 *	are as for raw_notifier_call_chain().
 */

int call_netdevice_notifiers(unsigned long val, struct net_device *dev)
{
	return raw_notifier_call_chain(&netdev_chain, val, dev);
}

/* When > 0 there are consumers of rx skb time stamps */
static atomic_t netstamp_needed = ATOMIC_INIT(0);

void net_enable_timestamp(void)
{
	atomic_inc(&netstamp_needed);
}
EXPORT_SYMBOL(net_enable_timestamp);

void net_disable_timestamp(void)
{
	atomic_dec(&netstamp_needed);
}
EXPORT_SYMBOL(net_disable_timestamp);

static inline void net_timestamp(struct sk_buff *skb)
{
	if (atomic_read(&netstamp_needed))
		__net_timestamp(skb);
	else
		skb->tstamp.tv64 = 0;
}

/*
 *	Support routine. Sends outgoing frames to any network
 *	taps currently in use.
 */

static void dev_queue_xmit_nit(struct sk_buff *skb, struct net_device *dev)
{
	struct packet_type *ptype;

#ifdef CONFIG_NET_CLS_ACT
	if (!(skb->tstamp.tv64 && (G_TC_FROM(skb->tc_verd) & AT_INGRESS)))
		net_timestamp(skb);
#else
	net_timestamp(skb);
#endif

	rcu_read_lock();
	list_for_each_entry_rcu(ptype, &ptype_all, list) {
		/* Never send packets back to the socket
		 * they originated from - MvS (miquels@drinkel.ow.org)
		 */
		if ((ptype->dev == dev || !ptype->dev) &&
		    (ptype->af_packet_priv == NULL ||
		     (struct sock *)ptype->af_packet_priv != skb->sk)) {
			struct sk_buff *skb2 = skb_clone(skb, GFP_ATOMIC);
			if (!skb2)
				break;

			/* skb->nh should be correctly
			   set by sender, so that the second statement is
			   just protection against buggy protocols.
			 */
			skb_reset_mac_header(skb2);

			if (skb_network_header(skb2) < skb2->data ||
			    skb2->network_header > skb2->tail) {
				if (net_ratelimit())
					printk(KERN_CRIT "protocol %04x is "
					       "buggy, dev %s\n",
					       skb2->protocol, dev->name);
				skb_reset_network_header(skb2);
			}

			skb2->transport_header = skb2->network_header;
			skb2->pkt_type = PACKET_OUTGOING;
			ptype->func(skb2, skb->dev, ptype, skb->dev);
		}
	}
	rcu_read_unlock();
}


static inline void __netif_reschedule(struct Qdisc *q)
{
	struct softnet_data *sd;
	unsigned long flags;

	local_irq_save(flags);
	sd = &__get_cpu_var(softnet_data);
	q->next_sched = sd->output_queue;
	sd->output_queue = q;
	raise_softirq_irqoff(NET_TX_SOFTIRQ);
	local_irq_restore(flags);
}

void __netif_schedule(struct Qdisc *q)
{
	if (!test_and_set_bit(__QDISC_STATE_SCHED, &q->state))
		__netif_reschedule(q);
}
EXPORT_SYMBOL(__netif_schedule);

void dev_kfree_skb_irq(struct sk_buff *skb)
{
	if (atomic_dec_and_test(&skb->users)) {
		struct softnet_data *sd;
		unsigned long flags;

		local_irq_save(flags);
		sd = &__get_cpu_var(softnet_data);
		skb->next = sd->completion_queue;
		sd->completion_queue = skb;
		raise_softirq_irqoff(NET_TX_SOFTIRQ);
		local_irq_restore(flags);
	}
}
EXPORT_SYMBOL(dev_kfree_skb_irq);

void dev_kfree_skb_any(struct sk_buff *skb)
{
	if (in_irq() || irqs_disabled())
		dev_kfree_skb_irq(skb);
	else
		dev_kfree_skb(skb);
}
EXPORT_SYMBOL(dev_kfree_skb_any);


/**
 * netif_device_detach - mark device as removed
 * @dev: network device
 *
 * Mark device as removed from system and therefore no longer available.
 */
void netif_device_detach(struct net_device *dev)
{
	if (test_and_clear_bit(__LINK_STATE_PRESENT, &dev->state) &&
	    netif_running(dev)) {
		netif_tx_stop_all_queues(dev);
	}
}
EXPORT_SYMBOL(netif_device_detach);

/**
 * netif_device_attach - mark device as attached
 * @dev: network device
 *
 * Mark device as attached from system and restart if needed.
 */
void netif_device_attach(struct net_device *dev)
{
	if (!test_and_set_bit(__LINK_STATE_PRESENT, &dev->state) &&
	    netif_running(dev)) {
		netif_tx_wake_all_queues(dev);
		__netdev_watchdog_up(dev);
	}
}
EXPORT_SYMBOL(netif_device_attach);

static bool can_checksum_protocol(unsigned long features, __be16 protocol)
{
	return ((features & NETIF_F_GEN_CSUM) ||
		((features & NETIF_F_IP_CSUM) &&
		 protocol == htons(ETH_P_IP)) ||
		((features & NETIF_F_IPV6_CSUM) &&
		 protocol == htons(ETH_P_IPV6)) ||
		((features & NETIF_F_FCOE_CRC) &&
		 protocol == htons(ETH_P_FCOE)));
}

static bool dev_can_checksum(struct net_device *dev, struct sk_buff *skb)
{
	if (can_checksum_protocol(dev->features, skb->protocol))
		return true;

	if (skb->protocol == htons(ETH_P_8021Q)) {
		struct vlan_ethhdr *veh = (struct vlan_ethhdr *)skb->data;
		if (can_checksum_protocol(dev->features & dev->vlan_features,
					  veh->h_vlan_encapsulated_proto))
			return true;
	}

	return false;
}

/*
 * Invalidate hardware checksum when packet is to be mangled, and
 * complete checksum manually on outgoing path.
 */
int skb_checksum_help(struct sk_buff *skb)
{
	__wsum csum;
	int ret = 0, offset;

	if (skb->ip_summed == CHECKSUM_COMPLETE)
		goto out_set_summed;

	if (unlikely(skb_shinfo(skb)->gso_size)) {
		/* Let GSO fix up the checksum. */
		goto out_set_summed;
	}

	offset = skb->csum_start - skb_headroom(skb);
	BUG_ON(offset >= skb_headlen(skb));
	csum = skb_checksum(skb, offset, skb->len - offset, 0);

	offset += skb->csum_offset;
	BUG_ON(offset + sizeof(__sum16) > skb_headlen(skb));

	if (skb_cloned(skb) &&
	    !skb_clone_writable(skb, offset + sizeof(__sum16))) {
		ret = pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
		if (ret)
			goto out;
	}

	*(__sum16 *)(skb->data + offset) = csum_fold(csum);
out_set_summed:
	skb->ip_summed = CHECKSUM_NONE;
out:
	return ret;
}
EXPORT_SYMBOL(skb_checksum_help);

/**
 *	skb_gso_segment - Perform segmentation on skb.
 *	@skb: buffer to segment
 *	@features: features for the output path (see dev->features)
 *
 *	This function segments the given skb and returns a list of segments.
 *
 *	It may return NULL if the skb requires no segmentation.  This is
 *	only possible when GSO is used for verifying header integrity.
 */
struct sk_buff *skb_gso_segment(struct sk_buff *skb, int features)
{
	struct sk_buff *segs = ERR_PTR(-EPROTONOSUPPORT);
	struct packet_type *ptype;
	__be16 type = skb->protocol;
	int err;

	skb_reset_mac_header(skb);
	skb->mac_len = skb->network_header - skb->mac_header;
	__skb_pull(skb, skb->mac_len);

	if (unlikely(skb->ip_summed != CHECKSUM_PARTIAL)) {
		struct net_device *dev = skb->dev;
		struct ethtool_drvinfo info = {};

		if (dev && dev->ethtool_ops && dev->ethtool_ops->get_drvinfo)
			dev->ethtool_ops->get_drvinfo(dev, &info);

		WARN(1, "%s: caps=(0x%lx, 0x%lx) len=%d data_len=%d "
			"ip_summed=%d",
		     info.driver, dev ? dev->features : 0L,
		     skb->sk ? skb->sk->sk_route_caps : 0L,
		     skb->len, skb->data_len, skb->ip_summed);

		if (skb_header_cloned(skb) &&
		    (err = pskb_expand_head(skb, 0, 0, GFP_ATOMIC)))
			return ERR_PTR(err);
	}

	rcu_read_lock();
	list_for_each_entry_rcu(ptype,
			&ptype_base[ntohs(type) & PTYPE_HASH_MASK], list) {
		if (ptype->type == type && !ptype->dev && ptype->gso_segment) {
			if (unlikely(skb->ip_summed != CHECKSUM_PARTIAL)) {
				err = ptype->gso_send_check(skb);
				segs = ERR_PTR(err);
				if (err || skb_gso_ok(skb, features))
					break;
				__skb_push(skb, (skb->data -
						 skb_network_header(skb)));
			}
			segs = ptype->gso_segment(skb, features);
			break;
		}
	}
	rcu_read_unlock();

	__skb_push(skb, skb->data - skb_mac_header(skb));

	return segs;
}
EXPORT_SYMBOL(skb_gso_segment);

/* Take action when hardware reception checksum errors are detected. */
#ifdef CONFIG_BUG
void netdev_rx_csum_fault(struct net_device *dev)
{
	if (net_ratelimit()) {
		printk(KERN_ERR "%s: hw csum failure.\n",
			dev ? dev->name : "<unknown>");
		dump_stack();
	}
}
EXPORT_SYMBOL(netdev_rx_csum_fault);
#endif

/* Actually, we should eliminate this check as soon as we know, that:
 * 1. IOMMU is present and allows to map all the memory.
 * 2. No high memory really exists on this machine.
 */

static inline int illegal_highdma(struct net_device *dev, struct sk_buff *skb)
{
#ifdef CONFIG_HIGHMEM
	int i;

	if (dev->features & NETIF_F_HIGHDMA)
		return 0;

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++)
		if (PageHighMem(skb_shinfo(skb)->frags[i].page))
			return 1;

#endif
	return 0;
}

struct dev_gso_cb {
	void (*destructor)(struct sk_buff *skb);
};

#define DEV_GSO_CB(skb) ((struct dev_gso_cb *)(skb)->cb)

static void dev_gso_skb_destructor(struct sk_buff *skb)
{
	struct dev_gso_cb *cb;

	do {
		struct sk_buff *nskb = skb->next;

		skb->next = nskb->next;
		nskb->next = NULL;
		kfree_skb(nskb);
	} while (skb->next);

	cb = DEV_GSO_CB(skb);
	if (cb->destructor)
		cb->destructor(skb);
}

/**
 *	dev_gso_segment - Perform emulated hardware segmentation on skb.
 *	@skb: buffer to segment
 *
 *	This function segments the given skb and stores the list of segments
 *	in skb->next.
 */
static int dev_gso_segment(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct sk_buff *segs;
	int features = dev->features & ~(illegal_highdma(dev, skb) ?
					 NETIF_F_SG : 0);

	segs = skb_gso_segment(skb, features);

	/* Verifying header integrity only. */
	if (!segs)
		return 0;

	if (IS_ERR(segs))
		return PTR_ERR(segs);

	skb->next = segs;
	DEV_GSO_CB(skb)->destructor = skb->destructor;
	skb->destructor = dev_gso_skb_destructor;

	return 0;
}

int dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev,
			struct netdev_queue *txq)
{
	const struct net_device_ops *ops = dev->netdev_ops;
	int rc;

	if (likely(!skb->next)) {
		if (!list_empty(&ptype_all))
			dev_queue_xmit_nit(skb, dev);

		if (netif_needs_gso(dev, skb)) {
			if (unlikely(dev_gso_segment(skb)))
				goto out_kfree_skb;
			if (skb->next)
				goto gso;
		}

		/*
		 * If device doesnt need skb->dst, release it right now while
		 * its hot in this cpu cache
		 */
		if (dev->priv_flags & IFF_XMIT_DST_RELEASE)
			skb_dst_drop(skb);

		rc = ops->ndo_start_xmit(skb, dev);
		if (rc == NETDEV_TX_OK)
			txq_trans_update(txq);
		/*
		 * TODO: if skb_orphan() was called by
		 * dev->hard_start_xmit() (for example, the unmodified
		 * igb driver does that; bnx2 doesn't), then
		 * skb_tx_software_timestamp() will be unable to send
		 * back the time stamp.
		 *
		 * How can this be prevented? Always create another
		 * reference to the socket before calling
		 * dev->hard_start_xmit()? Prevent that skb_orphan()
		 * does anything in dev->hard_start_xmit() by clearing
		 * the skb destructor before the call and restoring it
		 * afterwards, then doing the skb_orphan() ourselves?
		 */
		return rc;
	}

gso:
	do {
		struct sk_buff *nskb = skb->next;

		skb->next = nskb->next;
		nskb->next = NULL;
		rc = ops->ndo_start_xmit(nskb, dev);
		if (unlikely(rc != NETDEV_TX_OK)) {
			nskb->next = skb->next;
			skb->next = nskb;
			return rc;
		}
		txq_trans_update(txq);
		if (unlikely(netif_tx_queue_stopped(txq) && skb->next))
			return NETDEV_TX_BUSY;
	} while (skb->next);

	skb->destructor = DEV_GSO_CB(skb)->destructor;

out_kfree_skb:
	kfree_skb(skb);
	return NETDEV_TX_OK;
}

static u32 skb_tx_hashrnd;

u16 skb_tx_hash(const struct net_device *dev, const struct sk_buff *skb)
{
	u32 hash;

	if (skb_rx_queue_recorded(skb)) {
		hash = skb_get_rx_queue(skb);
		while (unlikely(hash >= dev->real_num_tx_queues))
			hash -= dev->real_num_tx_queues;
		return hash;
	}

	if (skb->sk && skb->sk->sk_hash)
		hash = skb->sk->sk_hash;
	else
		hash = skb->protocol;

	hash = jhash_1word(hash, skb_tx_hashrnd);

	return (u16) (((u64) hash * dev->real_num_tx_queues) >> 32);
}
EXPORT_SYMBOL(skb_tx_hash);

static struct netdev_queue *dev_pick_tx(struct net_device *dev,
					struct sk_buff *skb)
{
	const struct net_device_ops *ops = dev->netdev_ops;
	u16 queue_index = 0;

	if (ops->ndo_select_queue)
		queue_index = ops->ndo_select_queue(dev, skb);
	else if (dev->real_num_tx_queues > 1)
		queue_index = skb_tx_hash(dev, skb);

	skb_set_queue_mapping(skb, queue_index);
	return netdev_get_tx_queue(dev, queue_index);
}

static inline int __dev_xmit_skb(struct sk_buff *skb, struct Qdisc *q,
				 struct net_device *dev,
				 struct netdev_queue *txq)
{
	spinlock_t *root_lock = qdisc_lock(q);
	int rc;

	spin_lock(root_lock);
	if (unlikely(test_bit(__QDISC_STATE_DEACTIVATED, &q->state))) {
		kfree_skb(skb);
		rc = NET_XMIT_DROP;
	} else if ((q->flags & TCQ_F_CAN_BYPASS) && !qdisc_qlen(q) &&
		   !test_and_set_bit(__QDISC_STATE_RUNNING, &q->state)) {
		/*
		 * This is a work-conserving queue; there are no old skbs
		 * waiting to be sent out; and the qdisc is not running -
		 * xmit the skb directly.
		 */
		__qdisc_update_bstats(q, skb->len);
		if (sch_direct_xmit(skb, q, dev, txq, root_lock))
			__qdisc_run(q);
		else
			clear_bit(__QDISC_STATE_RUNNING, &q->state);

		rc = NET_XMIT_SUCCESS;
	} else {
		rc = qdisc_enqueue_root(skb, q);
		qdisc_run(q);
	}
	spin_unlock(root_lock);

	return rc;
}

/**
 *	dev_queue_xmit - transmit a buffer
 *	@skb: buffer to transmit
 *
 *	Queue a buffer for transmission to a network device. The caller must
 *	have set the device and priority and built the buffer before calling
 *	this function. The function can be called from an interrupt.
 *
 *	A negative errno code is returned on a failure. A success does not
 *	guarantee the frame will be transmitted as it may be dropped due
 *	to congestion or traffic shaping.
 *
 * -----------------------------------------------------------------------------------
 *      I notice this method can also return errors from the queue disciplines,
 *      including NET_XMIT_DROP, which is a positive value.  So, errors can also
 *      be positive.
 *
 *      Regardless of the return value, the skb is consumed, so it is currently
 *      difficult to retry a send to this method.  (You can bump the ref count
 *      before sending to hold a reference for retry if you are careful.)
 *
 *      When calling this method, interrupts MUST be enabled.  This is because
 *      the BH enable code must have IRQs enabled so that it will not deadlock.
 *          --BLG
 */
int dev_queue_xmit(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct netdev_queue *txq;
	struct Qdisc *q;
	int rc = -ENOMEM;

	/* GSO will handle the following emulations directly. */
	if (netif_needs_gso(dev, skb))
		goto gso;

	if (skb_has_frags(skb) &&
	    !(dev->features & NETIF_F_FRAGLIST) &&
	    __skb_linearize(skb))
		goto out_kfree_skb;

	/* Fragmented skb is linearized if device does not support SG,
	 * or if at least one of fragments is in highmem and device
	 * does not support DMA from it.
	 */
	if (skb_shinfo(skb)->nr_frags &&
	    (!(dev->features & NETIF_F_SG) || illegal_highdma(dev, skb)) &&
	    __skb_linearize(skb))
		goto out_kfree_skb;

	/* If packet is not checksummed and device does not support
	 * checksumming for this protocol, complete checksumming here.
	 */
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		skb_set_transport_header(skb, skb->csum_start -
					      skb_headroom(skb));
		if (!dev_can_checksum(dev, skb) && skb_checksum_help(skb))
			goto out_kfree_skb;
	}

gso:
	/* Disable soft irqs for various locks below. Also
	 * stops preemption for RCU.
	 */
	rcu_read_lock_bh();

	txq = dev_pick_tx(dev, skb);
	q = rcu_dereference(txq->qdisc);

#ifdef CONFIG_NET_CLS_ACT
	skb->tc_verd = SET_TC_AT(skb->tc_verd, AT_EGRESS);
#endif
	if (q->enqueue) {
		rc = __dev_xmit_skb(skb, q, dev, txq);
		goto out;
	}

	/* The device has no queue. Common case for software devices:
	   loopback, all the sorts of tunnels...

	   Really, it is unlikely that netif_tx_lock protection is necessary
	   here.  (f.e. loopback and IP tunnels are clean ignoring statistics
	   counters.)
	   However, it is possible, that they rely on protection
	   made by us here.

	   Check this and shot the lock. It is not prone from deadlocks.
	   Either shot noqueue qdisc, it is even simpler 8)
	 */
	if (dev->flags & IFF_UP) {
		int cpu = smp_processor_id(); /* ok because BHs are off */

		if (txq->xmit_lock_owner != cpu) {

			HARD_TX_LOCK(dev, txq, cpu);

			if (!netif_tx_queue_stopped(txq)) {
				rc = NET_XMIT_SUCCESS;
				if (!dev_hard_start_xmit(skb, dev, txq)) {
					HARD_TX_UNLOCK(dev, txq);
					goto out;
				}
			}
			HARD_TX_UNLOCK(dev, txq);
			if (net_ratelimit())
				printk(KERN_CRIT "Virtual device %s asks to "
				       "queue packet!\n", dev->name);
		} else {
			/* Recursion is detected! It is possible,
			 * unfortunately */
			if (net_ratelimit())
				printk(KERN_CRIT "Dead loop on virtual device "
				       "%s, fix it urgently!\n", dev->name);
		}
	}

	rc = -ENETDOWN;
	rcu_read_unlock_bh();

out_kfree_skb:
	kfree_skb(skb);
	return rc;
out:
	rcu_read_unlock_bh();
	return rc;
}
EXPORT_SYMBOL(dev_queue_xmit);


/*=======================================================================
			Receiver routines
  =======================================================================*/

int netdev_max_backlog __read_mostly = 1000;
int netdev_budget __read_mostly = 300;
int weight_p __read_mostly = 64;            /* old backlog weight */

DEFINE_PER_CPU(struct netif_rx_stats, netdev_rx_stat) = { 0, };


/**
 *	netif_rx	-	post buffer to the network code
 *	@skb: buffer to post
 *
 *	This function receives a packet from a device driver and queues it for
 *	the upper (protocol) levels to process.  It always succeeds. The buffer
 *	may be dropped during processing for congestion control or by the
 *	protocol layers.
 *
 *	return values:
 *	NET_RX_SUCCESS	(no congestion)
 *	NET_RX_DROP     (packet was dropped)
 *
 */

int netif_rx(struct sk_buff *skb)
{
	struct softnet_data *queue;
	unsigned long flags;

	/* if netpoll wants it, pretend we never saw it */
	if (netpoll_rx(skb))
		return NET_RX_DROP;

	if (!skb->tstamp.tv64)
		net_timestamp(skb);

	/*
	 * The code is rearranged so that the path is the most
	 * short when CPU is congested, but is still operating.
	 */
	local_irq_save(flags);
	queue = &__get_cpu_var(softnet_data);

	__get_cpu_var(netdev_rx_stat).total++;
	if (queue->input_pkt_queue.qlen <= netdev_max_backlog) {
		if (queue->input_pkt_queue.qlen) {
enqueue:
			__skb_queue_tail(&queue->input_pkt_queue, skb);
			local_irq_restore(flags);
			return NET_RX_SUCCESS;
		}

		napi_schedule(&queue->backlog);
		goto enqueue;
	}

	__get_cpu_var(netdev_rx_stat).dropped++;
	local_irq_restore(flags);

	kfree_skb(skb);
	return NET_RX_DROP;
}
EXPORT_SYMBOL(netif_rx);

int netif_rx_ni(struct sk_buff *skb)
{
	int err;

	preempt_disable();
	err = netif_rx(skb);
	if (local_softirq_pending())
		do_softirq();
	preempt_enable();

	return err;
}
EXPORT_SYMBOL(netif_rx_ni);

static void net_tx_action(struct softirq_action *h)
{
	struct softnet_data *sd = &__get_cpu_var(softnet_data);

	if (sd->completion_queue) {
		struct sk_buff *clist;

		local_irq_disable();
		clist = sd->completion_queue;
		sd->completion_queue = NULL;
		local_irq_enable();

		while (clist) {
			struct sk_buff *skb = clist;
			clist = clist->next;

			WARN_ON(atomic_read(&skb->users));
			__kfree_skb(skb);
		}
	}

	if (sd->output_queue) {
		struct Qdisc *head;

		local_irq_disable();
		head = sd->output_queue;
		sd->output_queue = NULL;
		local_irq_enable();

		while (head) {
			struct Qdisc *q = head;
			spinlock_t *root_lock;

			head = head->next_sched;

			root_lock = qdisc_lock(q);
			if (spin_trylock(root_lock)) {
				smp_mb__before_clear_bit();
				clear_bit(__QDISC_STATE_SCHED,
					  &q->state);
				qdisc_run(q);
				spin_unlock(root_lock);
			} else {
				if (!test_bit(__QDISC_STATE_DEACTIVATED,
					      &q->state)) {
					__netif_reschedule(q);
				} else {
					smp_mb__before_clear_bit();
					clear_bit(__QDISC_STATE_SCHED,
						  &q->state);
				}
			}
		}
	}
}

static inline int deliver_skb(struct sk_buff *skb,
			      struct packet_type *pt_prev,
			      struct net_device *orig_dev)
{
	atomic_inc(&skb->users);
	return pt_prev->func(skb, skb->dev, pt_prev, orig_dev);
}

#if defined(CONFIG_BRIDGE) || defined (CONFIG_BRIDGE_MODULE)

#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
/* This hook is defined here for ATM LANE */
int (*br_fdb_test_addr_hook)(struct net_device *dev,
			     unsigned char *addr) __read_mostly;
EXPORT_SYMBOL_GPL(br_fdb_test_addr_hook);
#endif

/*
 * If bridge module is loaded call bridging hook.
 *  returns NULL if packet was consumed.
 */
struct sk_buff *(*br_handle_frame_hook)(struct net_bridge_port *p,
					struct sk_buff *skb) __read_mostly;
EXPORT_SYMBOL_GPL(br_handle_frame_hook);

static inline struct sk_buff *handle_bridge(struct sk_buff *skb,
					    struct packet_type **pt_prev, int *ret,
					    struct net_device *orig_dev)
{
	struct net_bridge_port *port;

	if (skb->pkt_type == PACKET_LOOPBACK ||
	    (port = rcu_dereference(skb->dev->br_port)) == NULL)
		return skb;

	if (*pt_prev) {
		*ret = deliver_skb(skb, *pt_prev, orig_dev);
		*pt_prev = NULL;
	}

	return br_handle_frame_hook(port, skb);
}
#else
#define handle_bridge(skb, pt_prev, ret, orig_dev)	(skb)
#endif

#if defined(CONFIG_MACVLAN) || defined(CONFIG_MACVLAN_MODULE)
struct sk_buff *(*macvlan_handle_frame_hook)(struct sk_buff *skb) __read_mostly;
EXPORT_SYMBOL_GPL(macvlan_handle_frame_hook);

static inline struct sk_buff *handle_macvlan(struct sk_buff *skb,
					     struct packet_type **pt_prev,
					     int *ret,
					     struct net_device *orig_dev)
{
	if (skb->dev->macvlan_port == NULL)
		return skb;

	if (*pt_prev) {
		*ret = deliver_skb(skb, *pt_prev, orig_dev);
		*pt_prev = NULL;
	}
	return macvlan_handle_frame_hook(skb);
}
#else
#define handle_macvlan(skb, pt_prev, ret, orig_dev)	(skb)
#endif

#ifdef CONFIG_NET_CLS_ACT
/* TODO: Maybe we should just force sch_ingress to be compiled in
 * when CONFIG_NET_CLS_ACT is? otherwise some useless instructions
 * a compare and 2 stores extra right now if we dont have it on
 * but have CONFIG_NET_CLS_ACT
 * NOTE: This doesnt stop any functionality; if you dont have
 * the ingress scheduler, you just cant add policies on ingress.
 *
 */
static int ing_filter(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	u32 ttl = G_TC_RTTL(skb->tc_verd);
	struct netdev_queue *rxq;
	int result = TC_ACT_OK;
	struct Qdisc *q;

	if (MAX_RED_LOOP < ttl++) {
		printk(KERN_WARNING
		       "Redir loop detected Dropping packet (%d->%d)\n",
		       skb->iif, dev->ifindex);
		return TC_ACT_SHOT;
	}

	skb->tc_verd = SET_TC_RTTL(skb->tc_verd, ttl);
	skb->tc_verd = SET_TC_AT(skb->tc_verd, AT_INGRESS);

	rxq = &dev->rx_queue;

	q = rxq->qdisc;
	if (q != &noop_qdisc) {
		spin_lock(qdisc_lock(q));
		if (likely(!test_bit(__QDISC_STATE_DEACTIVATED, &q->state)))
			result = qdisc_enqueue_root(skb, q);
		spin_unlock(qdisc_lock(q));
	}

	return result;
}

static inline struct sk_buff *handle_ing(struct sk_buff *skb,
					 struct packet_type **pt_prev,
					 int *ret, struct net_device *orig_dev)
{
	if (skb->dev->rx_queue.qdisc == &noop_qdisc)
		goto out;

	if (*pt_prev) {
		*ret = deliver_skb(skb, *pt_prev, orig_dev);
		*pt_prev = NULL;
	} else {
		/* Huh? Why does turning on AF_PACKET affect this? */
		skb->tc_verd = SET_TC_OK2MUNGE(skb->tc_verd);
	}

	switch (ing_filter(skb)) {
	case TC_ACT_SHOT:
	case TC_ACT_STOLEN:
		kfree_skb(skb);
		return NULL;
	}

out:
	skb->tc_verd = 0;
	return skb;
}
#endif

/*
 * 	netif_nit_deliver - deliver received packets to network taps
 * 	@skb: buffer
 *
 * 	This function is used to deliver incoming packets to network
 * 	taps. It should be used when the normal netif_receive_skb path
 * 	is bypassed, for example because of VLAN acceleration.
 */
void netif_nit_deliver(struct sk_buff *skb)
{
	struct packet_type *ptype;

	if (list_empty(&ptype_all))
		return;

	skb_reset_network_header(skb);
	skb_reset_transport_header(skb);
	skb->mac_len = skb->network_header - skb->mac_header;

	rcu_read_lock();
	list_for_each_entry_rcu(ptype, &ptype_all, list) {
		if (!ptype->dev || ptype->dev == skb->dev)
			deliver_skb(skb, ptype, skb->dev);
	}
	rcu_read_unlock();
}

/**
 *	netif_receive_skb - process receive buffer from network
 *	@skb: buffer to process
 *
 *	netif_receive_skb() is the main receive data processing function.
 *	It always succeeds. The buffer may be dropped during processing
 *	for congestion control or by the protocol layers.
 *
 *	This function may only be called from softirq context and interrupts
 *	should be enabled.
 *
 *	Return values (usually ignored):
 *	NET_RX_SUCCESS: no congestion
 *	NET_RX_DROP: packet was dropped
 */
int netif_receive_skb(struct sk_buff *skb)
{
	struct packet_type *ptype, *pt_prev;
	struct net_device *orig_dev;
	struct net_device *null_or_orig;
	int ret = NET_RX_DROP;
	__be16 type;

	if (!skb->tstamp.tv64)
		net_timestamp(skb);

	if (skb->vlan_tci && vlan_hwaccel_do_receive(skb))
		return NET_RX_SUCCESS;

	/* if we've gotten here through NAPI, check netpoll */
	if (netpoll_receive_skb(skb))
		return NET_RX_DROP;

	if (!skb->iif)
		skb->iif = skb->dev->ifindex;

	null_or_orig = NULL;
	orig_dev = skb->dev;
	if (orig_dev->master) {
		if (skb_bond_should_drop(skb))
			null_or_orig = orig_dev; /* deliver only exact match */
		else
			skb->dev = orig_dev->master;
	}

	__get_cpu_var(netdev_rx_stat).total++;

	skb_reset_network_header(skb);
	skb_reset_transport_header(skb);
	skb->mac_len = skb->network_header - skb->mac_header;

	pt_prev = NULL;

	rcu_read_lock();

#ifdef CONFIG_NET_CLS_ACT
	if (skb->tc_verd & TC_NCLS) {
		skb->tc_verd = CLR_TC_NCLS(skb->tc_verd);
		goto ncls;
	}
#endif

	list_for_each_entry_rcu(ptype, &ptype_all, list) {
		if (ptype->dev == null_or_orig || ptype->dev == skb->dev ||
		    ptype->dev == orig_dev) {
			if (pt_prev)
				ret = deliver_skb(skb, pt_prev, orig_dev);
			pt_prev = ptype;
		}
	}

#ifdef CONFIG_NET_CLS_ACT
	skb = handle_ing(skb, &pt_prev, &ret, orig_dev);
	if (!skb)
		goto out;
ncls:
#endif

	skb = handle_bridge(skb, &pt_prev, &ret, orig_dev);
	if (!skb)
		goto out;
	skb = handle_macvlan(skb, &pt_prev, &ret, orig_dev);
	if (!skb)
		goto out;

	type = skb->protocol;
	list_for_each_entry_rcu(ptype,
			&ptype_base[ntohs(type) & PTYPE_HASH_MASK], list) {
		if (ptype->type == type &&
		    (ptype->dev == null_or_orig || ptype->dev == skb->dev ||
		     ptype->dev == orig_dev)) {
			if (pt_prev)
				ret = deliver_skb(skb, pt_prev, orig_dev);
			pt_prev = ptype;
		}
	}

	if (pt_prev) {
		ret = pt_prev->func(skb, skb->dev, pt_prev, orig_dev);
	} else {
		kfree_skb(skb);
		/* Jamal, now you will not able to escape explaining
		 * me how you were going to use this. :-)
		 */
		ret = NET_RX_DROP;
	}

out:
	rcu_read_unlock();
	return ret;
}
EXPORT_SYMBOL(netif_receive_skb);

/* Network device is going away, flush any packets still pending  */
static void flush_backlog(void *arg)
{
	struct net_device *dev = arg;
	struct softnet_data *queue = &__get_cpu_var(softnet_data);
	struct sk_buff *skb, *tmp;

	skb_queue_walk_safe(&queue->input_pkt_queue, skb, tmp)
		if (skb->dev == dev) {
			__skb_unlink(skb, &queue->input_pkt_queue);
			kfree_skb(skb);
		}
}

static int napi_gro_complete(struct sk_buff *skb)
{
	struct packet_type *ptype;
	__be16 type = skb->protocol;
	struct list_head *head = &ptype_base[ntohs(type) & PTYPE_HASH_MASK];
	int err = -ENOENT;

	if (NAPI_GRO_CB(skb)->count == 1) {
		skb_shinfo(skb)->gso_size = 0;
		goto out;
	}

	rcu_read_lock();
	list_for_each_entry_rcu(ptype, head, list) {
		if (ptype->type != type || ptype->dev || !ptype->gro_complete)
			continue;

		err = ptype->gro_complete(skb);
		break;
	}
	rcu_read_unlock();

	if (err) {
		WARN_ON(&ptype->list == head);
		kfree_skb(skb);
		return NET_RX_SUCCESS;
	}

out:
	return netif_receive_skb(skb);
}

void napi_gro_flush(struct napi_struct *napi)
{
	struct sk_buff *skb, *next;

	for (skb = napi->gro_list; skb; skb = next) {
		next = skb->next;
		skb->next = NULL;
		napi_gro_complete(skb);
	}

	napi->gro_count = 0;
	napi->gro_list = NULL;
}
EXPORT_SYMBOL(napi_gro_flush);

int dev_gro_receive(struct napi_struct *napi, struct sk_buff *skb)
{
	struct sk_buff **pp = NULL;
	struct packet_type *ptype;
	__be16 type = skb->protocol;
	struct list_head *head = &ptype_base[ntohs(type) & PTYPE_HASH_MASK];
	int same_flow;
	int mac_len;
	int ret;

	if (!(skb->dev->features & NETIF_F_GRO))
		goto normal;

	if (skb_is_gso(skb) || skb_has_frags(skb))
		goto normal;

	rcu_read_lock();
	list_for_each_entry_rcu(ptype, head, list) {
		if (ptype->type != type || ptype->dev || !ptype->gro_receive)
			continue;

		skb_set_network_header(skb, skb_gro_offset(skb));
		mac_len = skb->network_header - skb->mac_header;
		skb->mac_len = mac_len;
		NAPI_GRO_CB(skb)->same_flow = 0;
		NAPI_GRO_CB(skb)->flush = 0;
		NAPI_GRO_CB(skb)->free = 0;

		pp = ptype->gro_receive(&napi->gro_list, skb);
		break;
	}
	rcu_read_unlock();

	if (&ptype->list == head)
		goto normal;

	same_flow = NAPI_GRO_CB(skb)->same_flow;
	ret = NAPI_GRO_CB(skb)->free ? GRO_MERGED_FREE : GRO_MERGED;

	if (pp) {
		struct sk_buff *nskb = *pp;

		*pp = nskb->next;
		nskb->next = NULL;
		napi_gro_complete(nskb);
		napi->gro_count--;
	}

	if (same_flow)
		goto ok;

	if (NAPI_GRO_CB(skb)->flush || napi->gro_count >= MAX_GRO_SKBS)
		goto normal;

	napi->gro_count++;
	NAPI_GRO_CB(skb)->count = 1;
	skb_shinfo(skb)->gso_size = skb_gro_len(skb);
	skb->next = napi->gro_list;
	napi->gro_list = skb;
	ret = GRO_HELD;

pull:
	if (skb_headlen(skb) < skb_gro_offset(skb)) {
		int grow = skb_gro_offset(skb) - skb_headlen(skb);

		BUG_ON(skb->end - skb->tail < grow);

		memcpy(skb_tail_pointer(skb), NAPI_GRO_CB(skb)->frag0, grow);

		skb->tail += grow;
		skb->data_len -= grow;

		skb_shinfo(skb)->frags[0].page_offset += grow;
		skb_shinfo(skb)->frags[0].size -= grow;

		if (unlikely(!skb_shinfo(skb)->frags[0].size)) {
			put_page(skb_shinfo(skb)->frags[0].page);
			memmove(skb_shinfo(skb)->frags,
				skb_shinfo(skb)->frags + 1,
				--skb_shinfo(skb)->nr_frags);
		}
	}

ok:
	return ret;

normal:
	ret = GRO_NORMAL;
	goto pull;
}
EXPORT_SYMBOL(dev_gro_receive);

static int __napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb)
{
	struct sk_buff *p;

	if (netpoll_rx_on(skb))
		return GRO_NORMAL;

	for (p = napi->gro_list; p; p = p->next) {
		NAPI_GRO_CB(p)->same_flow = (p->dev == skb->dev)
			&& !compare_ether_header(skb_mac_header(p),
						 skb_gro_mac_header(skb));
		NAPI_GRO_CB(p)->flush = 0;
	}

	return dev_gro_receive(napi, skb);
}

int napi_skb_finish(int ret, struct sk_buff *skb)
{
	int err = NET_RX_SUCCESS;

	switch (ret) {
	case GRO_NORMAL:
		return netif_receive_skb(skb);

	case GRO_DROP:
		err = NET_RX_DROP;
		/* fall through */

	case GRO_MERGED_FREE:
		kfree_skb(skb);
		break;
	}

	return err;
}
EXPORT_SYMBOL(napi_skb_finish);

void skb_gro_reset_offset(struct sk_buff *skb)
{
	NAPI_GRO_CB(skb)->data_offset = 0;
	NAPI_GRO_CB(skb)->frag0 = NULL;
	NAPI_GRO_CB(skb)->frag0_len = 0;

	if (skb->mac_header == skb->tail &&
	    !PageHighMem(skb_shinfo(skb)->frags[0].page)) {
		NAPI_GRO_CB(skb)->frag0 =
			page_address(skb_shinfo(skb)->frags[0].page) +
			skb_shinfo(skb)->frags[0].page_offset;
		NAPI_GRO_CB(skb)->frag0_len = skb_shinfo(skb)->frags[0].size;
	}
}
EXPORT_SYMBOL(skb_gro_reset_offset);

int napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb)
{
	skb_gro_reset_offset(skb);

	return napi_skb_finish(__napi_gro_receive(napi, skb), skb);
}
EXPORT_SYMBOL(napi_gro_receive);

void napi_reuse_skb(struct napi_struct *napi, struct sk_buff *skb)
{
	__skb_pull(skb, skb_headlen(skb));
	skb_reserve(skb, NET_IP_ALIGN - skb_headroom(skb));

	napi->skb = skb;
}
EXPORT_SYMBOL(napi_reuse_skb);

struct sk_buff *napi_get_frags(struct napi_struct *napi)
{
	struct net_device *dev = napi->dev;
	struct sk_buff *skb = napi->skb;

	if (!skb) {
		skb = netdev_alloc_skb(dev, GRO_MAX_HEAD + NET_IP_ALIGN);
		if (!skb)
			goto out;

		skb_reserve(skb, NET_IP_ALIGN);

		napi->skb = skb;
	}

out:
	return skb;
}
EXPORT_SYMBOL(napi_get_frags);

int napi_frags_finish(struct napi_struct *napi, struct sk_buff *skb, int ret)
{
	int err = NET_RX_SUCCESS;

	switch (ret) {
	case GRO_NORMAL:
	case GRO_HELD:
		skb->protocol = eth_type_trans(skb, napi->dev);

		if (ret == GRO_NORMAL)
			return netif_receive_skb(skb);

		skb_gro_pull(skb, -ETH_HLEN);
		break;

	case GRO_DROP:
		err = NET_RX_DROP;
		/* fall through */

	case GRO_MERGED_FREE:
		napi_reuse_skb(napi, skb);
		break;
	}

	return err;
}
EXPORT_SYMBOL(napi_frags_finish);

struct sk_buff *napi_frags_skb(struct napi_struct *napi)
{
	struct sk_buff *skb = napi->skb;
	struct ethhdr *eth;
	unsigned int hlen;
	unsigned int off;

	napi->skb = NULL;

	skb_reset_mac_header(skb);
	skb_gro_reset_offset(skb);

	off = skb_gro_offset(skb);
	hlen = off + sizeof(*eth);
	eth = skb_gro_header_fast(skb, off);
	if (skb_gro_header_hard(skb, hlen)) {
		eth = skb_gro_header_slow(skb, hlen, off);
		if (unlikely(!eth)) {
			napi_reuse_skb(napi, skb);
			skb = NULL;
			goto out;
		}
	}

	skb_gro_pull(skb, sizeof(*eth));

	/*
	 * This works because the only protocols we care about don't require
	 * special handling.  We'll fix it up properly at the end.
	 */
	skb->protocol = eth->h_proto;

out:
	return skb;
}
EXPORT_SYMBOL(napi_frags_skb);

int napi_gro_frags(struct napi_struct *napi)
{
	struct sk_buff *skb = napi_frags_skb(napi);

	if (!skb)
		return NET_RX_DROP;

	return napi_frags_finish(napi, skb, __napi_gro_receive(napi, skb));
}
EXPORT_SYMBOL(napi_gro_frags);

static int process_backlog(struct napi_struct *napi, int quota)
{
	int work = 0;
	struct softnet_data *queue = &__get_cpu_var(softnet_data);
	unsigned long start_time = jiffies;

	napi->weight = weight_p;
	do {
		struct sk_buff *skb;

		local_irq_disable();
		skb = __skb_dequeue(&queue->input_pkt_queue);
		if (!skb) {
			__napi_complete(napi);
			local_irq_enable();
			break;
		}
		local_irq_enable();

		netif_receive_skb(skb);
	} while (++work < quota && jiffies == start_time);

	return work;
}

/**
 * __napi_schedule - schedule for receive
 * @n: entry to schedule
 *
 * The entry's receive function will be scheduled to run
 */
void __napi_schedule(struct napi_struct *n)
{
	unsigned long flags;

	local_irq_save(flags);
	list_add_tail(&n->poll_list, &__get_cpu_var(softnet_data).poll_list);
	__raise_softirq_irqoff(NET_RX_SOFTIRQ);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(__napi_schedule);

void __napi_complete(struct napi_struct *n)
{
	BUG_ON(!test_bit(NAPI_STATE_SCHED, &n->state));
	BUG_ON(n->gro_list);

	list_del(&n->poll_list);
	smp_mb__before_clear_bit();
	clear_bit(NAPI_STATE_SCHED, &n->state);
}
EXPORT_SYMBOL(__napi_complete);

void napi_complete(struct napi_struct *n)
{
	unsigned long flags;

	/*
	 * don't let napi dequeue from the cpu poll list
	 * just in case its running on a different cpu
	 */
	if (unlikely(test_bit(NAPI_STATE_NPSVC, &n->state)))
		return;

	napi_gro_flush(n);
	local_irq_save(flags);
	__napi_complete(n);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(napi_complete);

void netif_napi_add(struct net_device *dev, struct napi_struct *napi,
		    int (*poll)(struct napi_struct *, int), int weight)
{
	INIT_LIST_HEAD(&napi->poll_list);
	napi->gro_count = 0;
	napi->gro_list = NULL;
	napi->skb = NULL;
	napi->poll = poll;
	napi->weight = weight;
	list_add(&napi->dev_list, &dev->napi_list);
	napi->dev = dev;
#ifdef CONFIG_NETPOLL
	spin_lock_init(&napi->poll_lock);
	napi->poll_owner = -1;
#endif
	set_bit(NAPI_STATE_SCHED, &napi->state);
}
EXPORT_SYMBOL(netif_napi_add);

void netif_napi_del(struct napi_struct *napi)
{
	struct sk_buff *skb, *next;

	list_del_init(&napi->dev_list);
	napi_free_frags(napi);

	for (skb = napi->gro_list; skb; skb = next) {
		next = skb->next;
		skb->next = NULL;
		kfree_skb(skb);
	}

	napi->gro_list = NULL;
	napi->gro_count = 0;
}
EXPORT_SYMBOL(netif_napi_del);


static void net_rx_action(struct softirq_action *h)
{
	struct list_head *list = &__get_cpu_var(softnet_data).poll_list;
	unsigned long time_limit = jiffies + 2;
	int budget = netdev_budget;
	void *have;

	local_irq_disable();

	while (!list_empty(list)) {
		struct napi_struct *n;
		int work, weight;

		/* If softirq window is exhuasted then punt.
		 * Allow this to run for 2 jiffies since which will allow
		 * an average latency of 1.5/HZ.
		 */
		if (unlikely(budget <= 0 || time_after(jiffies, time_limit)))
			goto softnet_break;

		local_irq_enable();

		/* Even though interrupts have been re-enabled, this
		 * access is safe because interrupts can only add new
		 * entries to the tail of this list, and only ->poll()
		 * calls can remove this head entry from the list.
		 */
		n = list_entry(list->next, struct napi_struct, poll_list);

		have = netpoll_poll_lock(n);

		weight = n->weight;

		/* This NAPI_STATE_SCHED test is for avoiding a race
		 * with netpoll's poll_napi().  Only the entity which
		 * obtains the lock and sees NAPI_STATE_SCHED set will
		 * actually make the ->poll() call.  Therefore we avoid
		 * accidently calling ->poll() when NAPI is not scheduled.
		 */
		work = 0;
		if (test_bit(NAPI_STATE_SCHED, &n->state)) {
			work = n->poll(n, weight);
			trace_napi_poll(n);
		}

		WARN_ON_ONCE(work > weight);

		budget -= work;

		local_irq_disable();

		/* Drivers must not modify the NAPI state if they
		 * consume the entire weight.  In such cases this code
		 * still "owns" the NAPI instance and therefore can
		 * move the instance around on the list at-will.
		 */
		if (unlikely(work == weight)) {
			if (unlikely(napi_disable_pending(n))) {
				local_irq_enable();
				napi_complete(n);
				local_irq_disable();
			} else
				list_move_tail(&n->poll_list, list);
		}

		netpoll_poll_unlock(have);
	}
out:
	local_irq_enable();

#ifdef CONFIG_NET_DMA
	/*
	 * There may not be any more sk_buffs coming right now, so push
	 * any pending DMA copies to hardware
	 */
	dma_issue_pending_all();
#endif

	return;

softnet_break:
	__get_cpu_var(netdev_rx_stat).time_squeeze++;
	__raise_softirq_irqoff(NET_RX_SOFTIRQ);
	goto out;
}

static gifconf_func_t *gifconf_list[NPROTO];

/**
 *	register_gifconf	-	register a SIOCGIF handler
 *	@family: Address family
 *	@gifconf: Function handler
 *
 *	Register protocol dependent address dumping routines. The handler
 *	that is passed must not be freed or reused until it has been replaced
 *	by another handler.
 */
int register_gifconf(unsigned int family, gifconf_func_t *gifconf)
{
	if (family >= NPROTO)
		return -EINVAL;
	gifconf_list[family] = gifconf;
	return 0;
}
EXPORT_SYMBOL(register_gifconf);


/*
 *	Map an interface index to its name (SIOCGIFNAME)
 */

/*
 *	We need this ioctl for efficient implementation of the
 *	if_indextoname() function required by the IPv6 API.  Without
 *	it, we would have to search all the interfaces to find a
 *	match.  --pb
 */

static int dev_ifname(struct net *net, struct ifreq __user *arg)
{
	struct net_device *dev;
	struct ifreq ifr;

	/*
	 *	Fetch the caller's info block.
	 */

	if (copy_from_user(&ifr, arg, sizeof(struct ifreq)))
		return -EFAULT;

	read_lock(&dev_base_lock);
	dev = __dev_get_by_index(net, ifr.ifr_ifindex);
	if (!dev) {
		read_unlock(&dev_base_lock);
		return -ENODEV;
	}

	strcpy(ifr.ifr_name, dev->name);
	read_unlock(&dev_base_lock);

	if (copy_to_user(arg, &ifr, sizeof(struct ifreq)))
		return -EFAULT;
	return 0;
}

/*
 *	Perform a SIOCGIFCONF call. This structure will change
 *	size eventually, and there is nothing I can do about it.
 *	Thus we will need a 'compatibility mode'.
 */

static int dev_ifconf(struct net *net, char __user *arg)
{
	struct ifconf ifc;
	struct net_device *dev;
	char __user *pos;
	int len;
	int total;
	int i;

	/*
	 *	Fetch the caller's info block.
	 */

	if (copy_from_user(&ifc, arg, sizeof(struct ifconf)))
		return -EFAULT;

	pos = ifc.ifc_buf;
	len = ifc.ifc_len;

	/*
	 *	Loop over the interfaces, and write an info block for each.
	 */

	total = 0;
	for_each_netdev(net, dev) {
		for (i = 0; i < NPROTO; i++) {
			if (gifconf_list[i]) {
				int done;
				if (!pos)
					done = gifconf_list[i](dev, NULL, 0);
				else
					done = gifconf_list[i](dev, pos + total,
							       len - total);
				if (done < 0)
					return -EFAULT;
				total += done;
			}
		}
	}

	/*
	 *	All done.  Write the updated control block back to the caller.
	 */
	ifc.ifc_len = total;

	/*
	 * 	Both BSD and Solaris return 0 here, so we do too.
	 */
	return copy_to_user(arg, &ifc, sizeof(struct ifconf)) ? -EFAULT : 0;
}

#ifdef CONFIG_PROC_FS
/*
 *	This is invoked by the /proc filesystem handler to display a device
 *	in detail.
 */
void *dev_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(dev_base_lock)
{
	struct net *net = seq_file_net(seq);
	loff_t off;
	struct net_device *dev;

	read_lock(&dev_base_lock);
	if (!*pos)
		return SEQ_START_TOKEN;

	off = 1;
	for_each_netdev(net, dev)
		if (off++ == *pos)
			return dev;

	return NULL;
}

void *dev_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct net *net = seq_file_net(seq);
	++*pos;
	return v == SEQ_START_TOKEN ?
		first_net_device(net) : next_net_device((struct net_device *)v);
}

void dev_seq_stop(struct seq_file *seq, void *v)
	__releases(dev_base_lock)
{
	read_unlock(&dev_base_lock);
}

static void dev_seq_printf_stats(struct seq_file *seq, struct net_device *dev)
{
	const struct net_device_stats *stats = dev_get_stats(dev);

	seq_printf(seq, "%6s:%8lu %7lu %4lu %4lu %4lu %5lu %10lu %9lu "
		   "%8lu %7lu %4lu %4lu %4lu %5lu %7lu %10lu\n",
		   dev->name, stats->rx_bytes, stats->rx_packets,
		   stats->rx_errors,
		   stats->rx_dropped + stats->rx_missed_errors,
		   stats->rx_fifo_errors,
		   stats->rx_length_errors + stats->rx_over_errors +
		    stats->rx_crc_errors + stats->rx_frame_errors,
		   stats->rx_compressed, stats->multicast,
		   stats->tx_bytes, stats->tx_packets,
		   stats->tx_errors, stats->tx_dropped,
		   stats->tx_fifo_errors, stats->collisions,
		   stats->tx_carrier_errors +
		    stats->tx_aborted_errors +
		    stats->tx_window_errors +
		    stats->tx_heartbeat_errors,
		   stats->tx_compressed);
}

/*
 *	Called from the PROCfs module. This now uses the new arbitrary sized
 *	/proc/net interface to create /proc/net/dev
 */
static int dev_seq_show(struct seq_file *seq, void *v)
{
	if (v == SEQ_START_TOKEN)
		seq_puts(seq, "Inter-|   Receive                            "
			      "                    |  Transmit\n"
			      " face |bytes    packets errs drop fifo frame "
			      "compressed multicast|bytes    packets errs "
			      "drop fifo colls carrier compressed\n");
	else
		dev_seq_printf_stats(seq, v);
	return 0;
}

static struct netif_rx_stats *softnet_get_online(loff_t *pos)
{
	struct netif_rx_stats *rc = NULL;

	while (*pos < nr_cpu_ids)
		if (cpu_online(*pos)) {
			rc = &per_cpu(netdev_rx_stat, *pos);
			break;
		} else
			++*pos;
	return rc;
}

static void *softnet_seq_start(struct seq_file *seq, loff_t *pos)
{
	return softnet_get_online(pos);
}

static void *softnet_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	++*pos;
	return softnet_get_online(pos);
}

static void softnet_seq_stop(struct seq_file *seq, void *v)
{
}

static int softnet_seq_show(struct seq_file *seq, void *v)
{
	struct netif_rx_stats *s = v;

	seq_printf(seq, "%08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
		   s->total, s->dropped, s->time_squeeze, 0,
		   0, 0, 0, 0, /* was fastroute */
		   s->cpu_collision);
	return 0;
}

static const struct seq_operations dev_seq_ops = {
	.start = dev_seq_start,
	.next  = dev_seq_next,
	.stop  = dev_seq_stop,
	.show  = dev_seq_show,
};

static int dev_seq_open(struct inode *inode, struct file *file)
{
	return seq_open_net(inode, file, &dev_seq_ops,
			    sizeof(struct seq_net_private));
}

static const struct file_operations dev_seq_fops = {
	.owner	 = THIS_MODULE,
	.open    = dev_seq_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release_net,
};

static const struct seq_operations softnet_seq_ops = {
	.start = softnet_seq_start,
	.next  = softnet_seq_next,
	.stop  = softnet_seq_stop,
	.show  = softnet_seq_show,
};

static int softnet_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &softnet_seq_ops);
}

static const struct file_operations softnet_seq_fops = {
	.owner	 = THIS_MODULE,
	.open    = softnet_seq_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

static void *ptype_get_idx(loff_t pos)
{
	struct packet_type *pt = NULL;
	loff_t i = 0;
	int t;

	list_for_each_entry_rcu(pt, &ptype_all, list) {
		if (i == pos)
			return pt;
		++i;
	}

	for (t = 0; t < PTYPE_HASH_SIZE; t++) {
		list_for_each_entry_rcu(pt, &ptype_base[t], list) {
			if (i == pos)
				return pt;
			++i;
		}
	}
	return NULL;
}

static void *ptype_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(RCU)
{
	rcu_read_lock();
	return *pos ? ptype_get_idx(*pos - 1) : SEQ_START_TOKEN;
}

static void *ptype_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct packet_type *pt;
	struct list_head *nxt;
	int hash;

	++*pos;
	if (v == SEQ_START_TOKEN)
		return ptype_get_idx(0);

	pt = v;
	nxt = pt->list.next;
	if (pt->type == htons(ETH_P_ALL)) {
		if (nxt != &ptype_all)
			goto found;
		hash = 0;
		nxt = ptype_base[0].next;
	} else
		hash = ntohs(pt->type) & PTYPE_HASH_MASK;

	while (nxt == &ptype_base[hash]) {
		if (++hash >= PTYPE_HASH_SIZE)
			return NULL;
		nxt = ptype_base[hash].next;
	}
found:
	return list_entry(nxt, struct packet_type, list);
}

static void ptype_seq_stop(struct seq_file *seq, void *v)
	__releases(RCU)
{
	rcu_read_unlock();
}

static int ptype_seq_show(struct seq_file *seq, void *v)
{
	struct packet_type *pt = v;

	if (v == SEQ_START_TOKEN)
		seq_puts(seq, "Type Device      Function\n");
	else if (pt->dev == NULL || dev_net(pt->dev) == seq_file_net(seq)) {
		if (pt->type == htons(ETH_P_ALL))
			seq_puts(seq, "ALL ");
		else
			seq_printf(seq, "%04x", ntohs(pt->type));

		seq_printf(seq, " %-8s %pF\n",
			   pt->dev ? pt->dev->name : "", pt->func);
	}

	return 0;
}

static const struct seq_operations ptype_seq_ops = {
	.start = ptype_seq_start,
	.next  = ptype_seq_next,
	.stop  = ptype_seq_stop,
	.show  = ptype_seq_show,
};

static int ptype_seq_open(struct inode *inode, struct file *file)
{
	return seq_open_net(inode, file, &ptype_seq_ops,
			sizeof(struct seq_net_private));
}

static const struct file_operations ptype_seq_fops = {
	.owner	 = THIS_MODULE,
	.open    = ptype_seq_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release_net,
};


static int __net_init dev_proc_net_init(struct net *net)
{
	int rc = -ENOMEM;

	if (!proc_net_fops_create(net, "dev", S_IRUGO, &dev_seq_fops))
		goto out;
	if (!proc_net_fops_create(net, "softnet_stat", S_IRUGO, &softnet_seq_fops))
		goto out_dev;
	if (!proc_net_fops_create(net, "ptype", S_IRUGO, &ptype_seq_fops))
		goto out_softnet;

	if (wext_proc_init(net))
		goto out_ptype;
	rc = 0;
out:
	return rc;
out_ptype:
	proc_net_remove(net, "ptype");
out_softnet:
	proc_net_remove(net, "softnet_stat");
out_dev:
	proc_net_remove(net, "dev");
	goto out;
}

static void __net_exit dev_proc_net_exit(struct net *net)
{
	wext_proc_exit(net);

	proc_net_remove(net, "ptype");
	proc_net_remove(net, "softnet_stat");
	proc_net_remove(net, "dev");
}

static struct pernet_operations __net_initdata dev_proc_ops = {
	.init = dev_proc_net_init,
	.exit = dev_proc_net_exit,
};

static int __init dev_proc_init(void)
{
	return register_pernet_subsys(&dev_proc_ops);
}
#else
#define dev_proc_init() 0
#endif	/* CONFIG_PROC_FS */


/**
 *	netdev_set_master	-	set up master/slave pair
 *	@slave: slave device
 *	@master: new master device
 *
 *	Changes the master device of the slave. Pass %NULL to break the
 *	bonding. The caller must hold the RTNL semaphore. On a failure
 *	a negative errno code is returned. On success the reference counts
 *	are adjusted, %RTM_NEWLINK is sent to the routing socket and the
 *	function returns zero.
 */
int netdev_set_master(struct net_device *slave, struct net_device *master)
{
	struct net_device *old = slave->master;

	ASSERT_RTNL();

	if (master) {
		if (old)
			return -EBUSY;
		dev_hold(master);
	}

	slave->master = master;

	synchronize_net();

	if (old)
		dev_put(old);

	if (master)
		slave->flags |= IFF_SLAVE;
	else
		slave->flags &= ~IFF_SLAVE;

	rtmsg_ifinfo(RTM_NEWLINK, slave, IFF_SLAVE);
	return 0;
}
EXPORT_SYMBOL(netdev_set_master);

static void dev_change_rx_flags(struct net_device *dev, int flags)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if ((dev->flags & IFF_UP) && ops->ndo_change_rx_flags)
		ops->ndo_change_rx_flags(dev, flags);
}

static int __dev_set_promiscuity(struct net_device *dev, int inc)
{
	unsigned short old_flags = dev->flags;
	uid_t uid;
	gid_t gid;

	ASSERT_RTNL();

	dev->flags |= IFF_PROMISC;
	dev->promiscuity += inc;
	if (dev->promiscuity == 0) {
		/*
		 * Avoid overflow.
		 * If inc causes overflow, untouch promisc and return error.
		 */
		if (inc < 0)
			dev->flags &= ~IFF_PROMISC;
		else {
			dev->promiscuity -= inc;
			printk(KERN_WARNING "%s: promiscuity touches roof, "
				"set promiscuity failed, promiscuity feature "
				"of device might be broken.\n", dev->name);
			return -EOVERFLOW;
		}
	}
	if (dev->flags != old_flags) {
		printk(KERN_INFO "device %s %s promiscuous mode\n",
		       dev->name, (dev->flags & IFF_PROMISC) ? "entered" :
							       "left");
		if (audit_enabled) {
			current_uid_gid(&uid, &gid);
			audit_log(current->audit_context, GFP_ATOMIC,
				AUDIT_ANOM_PROMISCUOUS,
				"dev=%s prom=%d old_prom=%d auid=%u uid=%u gid=%u ses=%u",
				dev->name, (dev->flags & IFF_PROMISC),
				(old_flags & IFF_PROMISC),
				audit_get_loginuid(current),
				uid, gid,
				audit_get_sessionid(current));
		}

		dev_change_rx_flags(dev, IFF_PROMISC);
	}
	return 0;
}

/**
 *	dev_set_promiscuity	- update promiscuity count on a device
 *	@dev: device
 *	@inc: modifier
 *
 *	Add or remove promiscuity from a device. While the count in the device
 *	remains above zero the interface remains promiscuous. Once it hits zero
 *	the device reverts back to normal filtering operation. A negative inc
 *	value is used to drop promiscuity on the device.
 *	Return 0 if successful or a negative errno code on error.
 */
int dev_set_promiscuity(struct net_device *dev, int inc)
{
	unsigned short old_flags = dev->flags;
	int err;

	err = __dev_set_promiscuity(dev, inc);
	if (err < 0)
		return err;
	if (dev->flags != old_flags)
		dev_set_rx_mode(dev);
	return err;
}
EXPORT_SYMBOL(dev_set_promiscuity);

/**
 *	dev_set_allmulti	- update allmulti count on a device
 *	@dev: device
 *	@inc: modifier
 *
 *	Add or remove reception of all multicast frames to a device. While the
 *	count in the device remains above zero the interface remains listening
 *	to all interfaces. Once it hits zero the device reverts back to normal
 *	filtering operation. A negative @inc value is used to drop the counter
 *	when releasing a resource needing all multicasts.
 *	Return 0 if successful or a negative errno code on error.
 */

int dev_set_allmulti(struct net_device *dev, int inc)
{
	unsigned short old_flags = dev->flags;

	ASSERT_RTNL();

	dev->flags |= IFF_ALLMULTI;
	dev->allmulti += inc;
	if (dev->allmulti == 0) {
		/*
		 * Avoid overflow.
		 * If inc causes overflow, untouch allmulti and return error.
		 */
		if (inc < 0)
			dev->flags &= ~IFF_ALLMULTI;
		else {
			dev->allmulti -= inc;
			printk(KERN_WARNING "%s: allmulti touches roof, "
				"set allmulti failed, allmulti feature of "
				"device might be broken.\n", dev->name);
			return -EOVERFLOW;
		}
	}
	if (dev->flags ^ old_flags) {
		dev_change_rx_flags(dev, IFF_ALLMULTI);
		dev_set_rx_mode(dev);
	}
	return 0;
}
EXPORT_SYMBOL(dev_set_allmulti);

/*
 *	Upload unicast and multicast address lists to device and
 *	configure RX filtering. When the device doesn't support unicast
 *	filtering it is put in promiscuous mode while unicast addresses
 *	are present.
 */
void __dev_set_rx_mode(struct net_device *dev)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	/* dev_open will call this function so the list will stay sane. */
	if (!(dev->flags&IFF_UP))
		return;

	if (!netif_device_present(dev))
		return;

	if (ops->ndo_set_rx_mode)
		ops->ndo_set_rx_mode(dev);
	else {
		/* Unicast addresses changes may only happen under the rtnl,
		 * therefore calling __dev_set_promiscuity here is safe.
		 */
		if (dev->uc.count > 0 && !dev->uc_promisc) {
			__dev_set_promiscuity(dev, 1);
			dev->uc_promisc = 1;
		} else if (dev->uc.count == 0 && dev->uc_promisc) {
			__dev_set_promiscuity(dev, -1);
			dev->uc_promisc = 0;
		}

		if (ops->ndo_set_multicast_list)
			ops->ndo_set_multicast_list(dev);
	}
}

void dev_set_rx_mode(struct net_device *dev)
{
	netif_addr_lock_bh(dev);
	__dev_set_rx_mode(dev);
	netif_addr_unlock_bh(dev);
}

/* hw addresses list handling functions */

static int __hw_addr_add(struct netdev_hw_addr_list *list, unsigned char *addr,
			 int addr_len, unsigned char addr_type)
{
	struct netdev_hw_addr *ha;
	int alloc_size;

	if (addr_len > MAX_ADDR_LEN)
		return -EINVAL;

	list_for_each_entry(ha, &list->list, list) {
		if (!memcmp(ha->addr, addr, addr_len) &&
		    ha->type == addr_type) {
			ha->refcount++;
			return 0;
		}
	}


	alloc_size = sizeof(*ha);
	if (alloc_size < L1_CACHE_BYTES)
		alloc_size = L1_CACHE_BYTES;
	ha = kmalloc(alloc_size, GFP_ATOMIC);
	if (!ha)
		return -ENOMEM;
	memcpy(ha->addr, addr, addr_len);
	ha->type = addr_type;
	ha->refcount = 1;
	ha->synced = false;
	list_add_tail_rcu(&ha->list, &list->list);
	list->count++;
	return 0;
}

static void ha_rcu_free(struct rcu_head *head)
{
	struct netdev_hw_addr *ha;

	ha = container_of(head, struct netdev_hw_addr, rcu_head);
	kfree(ha);
}

static int __hw_addr_del(struct netdev_hw_addr_list *list, unsigned char *addr,
			 int addr_len, unsigned char addr_type)
{
	struct netdev_hw_addr *ha;

	list_for_each_entry(ha, &list->list, list) {
		if (!memcmp(ha->addr, addr, addr_len) &&
		    (ha->type == addr_type || !addr_type)) {
			if (--ha->refcount)
				return 0;
			list_del_rcu(&ha->list);
			call_rcu(&ha->rcu_head, ha_rcu_free);
			list->count--;
			return 0;
		}
	}
	return -ENOENT;
}

static int __hw_addr_add_multiple(struct netdev_hw_addr_list *to_list,
				  struct netdev_hw_addr_list *from_list,
				  int addr_len,
				  unsigned char addr_type)
{
	int err;
	struct netdev_hw_addr *ha, *ha2;
	unsigned char type;

	list_for_each_entry(ha, &from_list->list, list) {
		type = addr_type ? addr_type : ha->type;
		err = __hw_addr_add(to_list, ha->addr, addr_len, type);
		if (err)
			goto unroll;
	}
	return 0;

unroll:
	list_for_each_entry(ha2, &from_list->list, list) {
		if (ha2 == ha)
			break;
		type = addr_type ? addr_type : ha2->type;
		__hw_addr_del(to_list, ha2->addr, addr_len, type);
	}
	return err;
}

static void __hw_addr_del_multiple(struct netdev_hw_addr_list *to_list,
				   struct netdev_hw_addr_list *from_list,
				   int addr_len,
				   unsigned char addr_type)
{
	struct netdev_hw_addr *ha;
	unsigned char type;

	list_for_each_entry(ha, &from_list->list, list) {
		type = addr_type ? addr_type : ha->type;
		__hw_addr_del(to_list, ha->addr, addr_len, addr_type);
	}
}

static int __hw_addr_sync(struct netdev_hw_addr_list *to_list,
			  struct netdev_hw_addr_list *from_list,
			  int addr_len)
{
	int err = 0;
	struct netdev_hw_addr *ha, *tmp;

	list_for_each_entry_safe(ha, tmp, &from_list->list, list) {
		if (!ha->synced) {
			err = __hw_addr_add(to_list, ha->addr,
					    addr_len, ha->type);
			if (err)
				break;
			ha->synced = true;
			ha->refcount++;
		} else if (ha->refcount == 1) {
			__hw_addr_del(to_list, ha->addr, addr_len, ha->type);
			__hw_addr_del(from_list, ha->addr, addr_len, ha->type);
		}
	}
	return err;
}

static void __hw_addr_unsync(struct netdev_hw_addr_list *to_list,
			     struct netdev_hw_addr_list *from_list,
			     int addr_len)
{
	struct netdev_hw_addr *ha, *tmp;

	list_for_each_entry_safe(ha, tmp, &from_list->list, list) {
		if (ha->synced) {
			__hw_addr_del(to_list, ha->addr,
				      addr_len, ha->type);
			ha->synced = false;
			__hw_addr_del(from_list, ha->addr,
				      addr_len, ha->type);
		}
	}
}

static void __hw_addr_flush(struct netdev_hw_addr_list *list)
{
	struct netdev_hw_addr *ha, *tmp;

	list_for_each_entry_safe(ha, tmp, &list->list, list) {
		list_del_rcu(&ha->list);
		call_rcu(&ha->rcu_head, ha_rcu_free);
	}
	list->count = 0;
}

static void __hw_addr_init(struct netdev_hw_addr_list *list)
{
	INIT_LIST_HEAD(&list->list);
	list->count = 0;
}

/* Device addresses handling functions */

static void dev_addr_flush(struct net_device *dev)
{
	/* rtnl_mutex must be held here */

	__hw_addr_flush(&dev->dev_addrs);
	dev->dev_addr = NULL;
}

static int dev_addr_init(struct net_device *dev)
{
	unsigned char addr[MAX_ADDR_LEN];
	struct netdev_hw_addr *ha;
	int err;

	/* rtnl_mutex must be held here */

	__hw_addr_init(&dev->dev_addrs);
	memset(addr, 0, sizeof(addr));
	err = __hw_addr_add(&dev->dev_addrs, addr, sizeof(addr),
			    NETDEV_HW_ADDR_T_LAN);
	if (!err) {
		/*
		 * Get the first (previously created) address from the list
		 * and set dev_addr pointer to this location.
		 */
		ha = list_first_entry(&dev->dev_addrs.list,
				      struct netdev_hw_addr, list);
		dev->dev_addr = ha->addr;
	}
	return err;
}

/**
 *	dev_addr_add	- Add a device address
 *	@dev: device
 *	@addr: address to add
 *	@addr_type: address type
 *
 *	Add a device address to the device or increase the reference count if
 *	it already exists.
 *
 *	The caller must hold the rtnl_mutex.
 */
int dev_addr_add(struct net_device *dev, unsigned char *addr,
		 unsigned char addr_type)
{
	int err;

	ASSERT_RTNL();

	err = __hw_addr_add(&dev->dev_addrs, addr, dev->addr_len, addr_type);
	if (!err)
		call_netdevice_notifiers(NETDEV_CHANGEADDR, dev);
	return err;
}
EXPORT_SYMBOL(dev_addr_add);

/**
 *	dev_addr_del	- Release a device address.
 *	@dev: device
 *	@addr: address to delete
 *	@addr_type: address type
 *
 *	Release reference to a device address and remove it from the device
 *	if the reference count drops to zero.
 *
 *	The caller must hold the rtnl_mutex.
 */
int dev_addr_del(struct net_device *dev, unsigned char *addr,
		 unsigned char addr_type)
{
	int err;
	struct netdev_hw_addr *ha;

	ASSERT_RTNL();

	/*
	 * We can not remove the first address from the list because
	 * dev->dev_addr points to that.
	 */
	ha = list_first_entry(&dev->dev_addrs.list,
			      struct netdev_hw_addr, list);
	if (ha->addr == dev->dev_addr && ha->refcount == 1)
		return -ENOENT;

	err = __hw_addr_del(&dev->dev_addrs, addr, dev->addr_len,
			    addr_type);
	if (!err)
		call_netdevice_notifiers(NETDEV_CHANGEADDR, dev);
	return err;
}
EXPORT_SYMBOL(dev_addr_del);

/**
 *	dev_addr_add_multiple	- Add device addresses from another device
 *	@to_dev: device to which addresses will be added
 *	@from_dev: device from which addresses will be added
 *	@addr_type: address type - 0 means type will be used from from_dev
 *
 *	Add device addresses of the one device to another.
 **
 *	The caller must hold the rtnl_mutex.
 */
int dev_addr_add_multiple(struct net_device *to_dev,
			  struct net_device *from_dev,
			  unsigned char addr_type)
{
	int err;

	ASSERT_RTNL();

	if (from_dev->addr_len != to_dev->addr_len)
		return -EINVAL;
	err = __hw_addr_add_multiple(&to_dev->dev_addrs, &from_dev->dev_addrs,
				     to_dev->addr_len, addr_type);
	if (!err)
		call_netdevice_notifiers(NETDEV_CHANGEADDR, to_dev);
	return err;
}
EXPORT_SYMBOL(dev_addr_add_multiple);

/**
 *	dev_addr_del_multiple	- Delete device addresses by another device
 *	@to_dev: device where the addresses will be deleted
 *	@from_dev: device by which addresses the addresses will be deleted
 *	@addr_type: address type - 0 means type will used from from_dev
 *
 *	Deletes addresses in to device by the list of addresses in from device.
 *
 *	The caller must hold the rtnl_mutex.
 */
int dev_addr_del_multiple(struct net_device *to_dev,
			  struct net_device *from_dev,
			  unsigned char addr_type)
{
	ASSERT_RTNL();

	if (from_dev->addr_len != to_dev->addr_len)
		return -EINVAL;
	__hw_addr_del_multiple(&to_dev->dev_addrs, &from_dev->dev_addrs,
			       to_dev->addr_len, addr_type);
	call_netdevice_notifiers(NETDEV_CHANGEADDR, to_dev);
	return 0;
}
EXPORT_SYMBOL(dev_addr_del_multiple);

/* multicast addresses handling functions */

int __dev_addr_delete(struct dev_addr_list **list, int *count,
		      void *addr, int alen, int glbl)
{
	struct dev_addr_list *da;

	for (; (da = *list) != NULL; list = &da->next) {
		if (memcmp(da->da_addr, addr, da->da_addrlen) == 0 &&
		    alen == da->da_addrlen) {
			if (glbl) {
				int old_glbl = da->da_gusers;
				da->da_gusers = 0;
				if (old_glbl == 0)
					break;
			}
			if (--da->da_users)
				return 0;

			*list = da->next;
			kfree(da);
			(*count)--;
			return 0;
		}
	}
	return -ENOENT;
}

int __dev_addr_add(struct dev_addr_list **list, int *count,
		   void *addr, int alen, int glbl)
{
	struct dev_addr_list *da;

	for (da = *list; da != NULL; da = da->next) {
		if (memcmp(da->da_addr, addr, da->da_addrlen) == 0 &&
		    da->da_addrlen == alen) {
			if (glbl) {
				int old_glbl = da->da_gusers;
				da->da_gusers = 1;
				if (old_glbl)
					return 0;
			}
			da->da_users++;
			return 0;
		}
	}

	da = kzalloc(sizeof(*da), GFP_ATOMIC);
	if (da == NULL)
		return -ENOMEM;
	memcpy(da->da_addr, addr, alen);
	da->da_addrlen = alen;
	da->da_users = 1;
	da->da_gusers = glbl ? 1 : 0;
	da->next = *list;
	*list = da;
	(*count)++;
	return 0;
}

/**
 *	dev_unicast_delete	- Release secondary unicast address.
 *	@dev: device
 *	@addr: address to delete
 *
 *	Release reference to a secondary unicast address and remove it
 *	from the device if the reference count drops to zero.
 *
 * 	The caller must hold the rtnl_mutex.
 */
int dev_unicast_delete(struct net_device *dev, void *addr)
{
	int err;

	ASSERT_RTNL();

	netif_addr_lock_bh(dev);
	err = __hw_addr_del(&dev->uc, addr, dev->addr_len,
			    NETDEV_HW_ADDR_T_UNICAST);
	if (!err)
		__dev_set_rx_mode(dev);
	netif_addr_unlock_bh(dev);
	return err;
}
EXPORT_SYMBOL(dev_unicast_delete);

/**
 *	dev_unicast_add		- add a secondary unicast address
 *	@dev: device
 *	@addr: address to add
 *
 *	Add a secondary unicast address to the device or increase
 *	the reference count if it already exists.
 *
 *	The caller must hold the rtnl_mutex.
 */
int dev_unicast_add(struct net_device *dev, void *addr)
{
	int err;

	ASSERT_RTNL();

	netif_addr_lock_bh(dev);
	err = __hw_addr_add(&dev->uc, addr, dev->addr_len,
			    NETDEV_HW_ADDR_T_UNICAST);
	if (!err)
		__dev_set_rx_mode(dev);
	netif_addr_unlock_bh(dev);
	return err;
}
EXPORT_SYMBOL(dev_unicast_add);

int __dev_addr_sync(struct dev_addr_list **to, int *to_count,
		    struct dev_addr_list **from, int *from_count)
{
	struct dev_addr_list *da, *next;
	int err = 0;

	da = *from;
	while (da != NULL) {
		next = da->next;
		if (!da->da_synced) {
			err = __dev_addr_add(to, to_count,
					     da->da_addr, da->da_addrlen, 0);
			if (err < 0)
				break;
			da->da_synced = 1;
			da->da_users++;
		} else if (da->da_users == 1) {
			__dev_addr_delete(to, to_count,
					  da->da_addr, da->da_addrlen, 0);
			__dev_addr_delete(from, from_count,
					  da->da_addr, da->da_addrlen, 0);
		}
		da = next;
	}
	return err;
}
EXPORT_SYMBOL_GPL(__dev_addr_sync);

void __dev_addr_unsync(struct dev_addr_list **to, int *to_count,
		       struct dev_addr_list **from, int *from_count)
{
	struct dev_addr_list *da, *next;

	da = *from;
	while (da != NULL) {
		next = da->next;
		if (da->da_synced) {
			__dev_addr_delete(to, to_count,
					  da->da_addr, da->da_addrlen, 0);
			da->da_synced = 0;
			__dev_addr_delete(from, from_count,
					  da->da_addr, da->da_addrlen, 0);
		}
		da = next;
	}
}
EXPORT_SYMBOL_GPL(__dev_addr_unsync);

/**
 *	dev_unicast_sync - Synchronize device's unicast list to another device
 *	@to: destination device
 *	@from: source device
 *
 *	Add newly added addresses to the destination device and release
 *	addresses that have no users left. The source device must be
 *	locked by netif_tx_lock_bh.
 *
 *	This function is intended to be called from the dev->set_rx_mode
 *	function of layered software devices.
 */
int dev_unicast_sync(struct net_device *to, struct net_device *from)
{
	int err = 0;

	if (to->addr_len != from->addr_len)
		return -EINVAL;

	netif_addr_lock_bh(to);
	err = __hw_addr_sync(&to->uc, &from->uc, to->addr_len);
	if (!err)
		__dev_set_rx_mode(to);
	netif_addr_unlock_bh(to);
	return err;
}
EXPORT_SYMBOL(dev_unicast_sync);

/**
 *	dev_unicast_unsync - Remove synchronized addresses from the destination device
 *	@to: destination device
 *	@from: source device
 *
 *	Remove all addresses that were added to the destination device by
 *	dev_unicast_sync(). This function is intended to be called from the
 *	dev->stop function of layered software devices.
 */
void dev_unicast_unsync(struct net_device *to, struct net_device *from)
{
	if (to->addr_len != from->addr_len)
		return;

	netif_addr_lock_bh(from);
	netif_addr_lock(to);
	__hw_addr_unsync(&to->uc, &from->uc, to->addr_len);
	__dev_set_rx_mode(to);
	netif_addr_unlock(to);
	netif_addr_unlock_bh(from);
}
EXPORT_SYMBOL(dev_unicast_unsync);

static void dev_unicast_flush(struct net_device *dev)
{
	netif_addr_lock_bh(dev);
	__hw_addr_flush(&dev->uc);
	netif_addr_unlock_bh(dev);
}

static void dev_unicast_init(struct net_device *dev)
{
	__hw_addr_init(&dev->uc);
}


static void __dev_addr_discard(struct dev_addr_list **list)
{
	struct dev_addr_list *tmp;

	while (*list != NULL) {
		tmp = *list;
		*list = tmp->next;
		if (tmp->da_users > tmp->da_gusers)
			printk("__dev_addr_discard: address leakage! "
			       "da_users=%d\n", tmp->da_users);
		kfree(tmp);
	}
}

static void dev_addr_discard(struct net_device *dev)
{
	netif_addr_lock_bh(dev);

	__dev_addr_discard(&dev->mc_list);
	dev->mc_count = 0;

	netif_addr_unlock_bh(dev);
}

/**
 *	dev_get_flags - get flags reported to userspace
 *	@dev: device
 *
 *	Get the combination of flag bits exported through APIs to userspace.
 */
unsigned dev_get_flags(const struct net_device *dev)
{
	unsigned flags;

	flags = (dev->flags & ~(IFF_PROMISC |
				IFF_ALLMULTI |
				IFF_RUNNING |
				IFF_LOWER_UP |
				IFF_DORMANT)) |
		(dev->gflags & (IFF_PROMISC |
				IFF_ALLMULTI));

	if (netif_running(dev)) {
		if (netif_oper_up(dev))
			flags |= IFF_RUNNING;
		if (netif_carrier_ok(dev))
			flags |= IFF_LOWER_UP;
		if (netif_dormant(dev))
			flags |= IFF_DORMANT;
	}

	return flags;
}
EXPORT_SYMBOL(dev_get_flags);

/**
 *	dev_change_flags - change device settings
 *	@dev: device
 *	@flags: device state flags
 *
 *	Change settings on device based state flags. The flags are
 *	in the userspace exported format.
 */
int dev_change_flags(struct net_device *dev, unsigned flags)
{
	int ret, changes;
	int old_flags = dev->flags;

	ASSERT_RTNL();

	/*
	 *	Set the flags on our device.
	 */

	dev->flags = (flags & (IFF_DEBUG | IFF_NOTRAILERS | IFF_NOARP |
			       IFF_DYNAMIC | IFF_MULTICAST | IFF_PORTSEL |
			       IFF_AUTOMEDIA)) |
		     (dev->flags & (IFF_UP | IFF_VOLATILE | IFF_PROMISC |
				    IFF_ALLMULTI));

	/*
	 *	Load in the correct multicast list now the flags have changed.
	 */

	if ((old_flags ^ flags) & IFF_MULTICAST)
		dev_change_rx_flags(dev, IFF_MULTICAST);

	dev_set_rx_mode(dev);

	/*
	 *	Have we downed the interface. We handle IFF_UP ourselves
	 *	according to user attempts *		set it, rather than blindly routsett *
 it. rou/

	ret = 0;
	if ((old_flags ^ erms ) &nt devi) {	/* Bit is different  ? *			odify i the terms o General P? dev_close :ion; open)(dev);

	nder !ret)
			on; set_rx_mode		2 of 	}
under dev-> Software Foun &&
	   r the terms of ion.
 *
 *NU G~(t devic|nt dePROMISCthors:	ALLMULTI | or 		 nt deVOLATILE)), orcall_netdevice_notifiers(NETDEV_CHANGE,of dof thder t parts of devgthe GNU GenerRoss Bi) {the nt inc the Software Fothors:
 *? 1 : -1f the *
 *	Addit ^=sfl@rz.uni-s;an Cox yourpromiscuitysion,an ly later /* NOTE: order of synchronization	Alers:	Ross Birand
 *				Fred Nm theis important. Some (broken) driversfree fl@rz.uni-s, whenm the *				Fred N.is requested not ask *
 usac.ruChanredam@ingor
 *		.aston.ac.uk>
 *
 *	Additional Au		Fred N *		Florian la Roche <rzsfl@refcnt getsde>
 *		Alan Cox <gw4pts@gw4pts.		Fred Ng>
 *		Davidallmultiinds@users.sourceforExclude state transiov <kerms , already    			Mdow   changes then IP parts of dev.c 1.0.19
 * 		Authors:	RUNNINGy lader *		Alan, orrtmsg_ifinfo(RTM_NEWLINKsmp@u, 	:	Transf threturn ret;
}
EXPORT_SYMBOLsion_*		Alaterms s to/**
 * (at yourmtu - C		Ala maximum  			ffer unitck.
@dev versicetrap
newn Co: new NULL pointer track.
	Fixed 	NETpromisc NULL poinsize	Ale	NETnetwork???????.
 *		oria *		Alan Co(structn ch_?????? *nds@use	:	Sport )
{
	const ordas	:	SIOCADDMU_ops *t do=of devgnet.Ost dlan nt errura.astoMULTI
  = cut itI
 *	ed byp th0f th/*	MTU must be pofew ve.row     :  multica< 0
 *		Alan C-EINVALura.asto!netifklog juspresent		2 o	: 	Fix ETH_PNODEVf therry it under ops->ndoox	:	Fix 8)
 *	 packe*					Saved a few binds@uMULTI
 *;
	elsean Cox <ticas Cox	:	Nechoback  pac&&of dev.c 1.ware Foundlt.NL.Mugnet.ORG>
 *				Mark Evans, <evanMTUsmp@uhur keep thstartue safe.
 *		Alan CoAlan Couble lock.
 *		Alan ac_addressx	:	Fixed Media Acc	:	WControl ACox	:	trap
 *		????????	:	Ssathe fu Cox	:	ctl range
 *		Alanhardware (MAC)n.
 *		Armission???????	:	drivers
 *		Tilan Cox	:	Kordas	:	SIOCADDMULTI/SIOordas	:sock Cox *sa*		Alan Cox	:	100 backlog just doesn't cut it when
 *					you start doing!*					Sav *		Rudi Cilibr	: 	Fix ETH_POPNOTSUPPlan Coxsa->sa_family ! cut ittype	: 	Fix ETH_P_ALL ecoback lengths.
 *		Alan Cox	:	Took out transmit eves in the ioctl h *		Rudi Cilibra thingaAlan Cox
 *	aves a function
 *					call a packet.
 *		ADDRn Cox	:	Hashed net_bh()
 *		Richard Kooijman:	ut on a Spable lck.
PerformssionSIOCxIFxxx NL.Ms@useside e in_lockan Cobasel Cha)				c    icOCDELen
 ifsiocl ChaedKordas	:	SI *neting to
 *ifreq *ifr, unsig
 * you cmd*		Alyou start	ordas	:	SIOCADDMULTI/S = _OCAD_get_by_name(:	Cleifr->ifrvice hura.asto!p@uhok out transmit everyswitch (  :	*		Fcasg for GIFFLAGS:eforGee s	Protocones oflished needs toerms o= (short)r SIOork erms ) any la		Alan Cox	:	:	SIOCSIFNAMMETRIC         Alan etric on 	NET3	Protocovan Khe ncur	as ly unused)nen  :	Netdev btdevice it un  Andrew Morton   :       TUunregister_neroteof aose
 *	ev->refcnt
 * 		s pacer sets lim	:	- Backlog queue sampliHW if :the Liceion.
 Cox_len, or memset( needs tohw Cox.sa_data, 0,tl peofd needs topability.h>
#i     rk drivinclcpy <linux/capability.h>
#incion.
netw Cox,>
#ithe    min(e <linux/cpu.h>
#include <linu, inux/_ings cux/bitops.h     x/cpu.h>
#include <D hack. *		Alan Colim	:	- Backlog queue sampliSLAVEincl packe up the ba	breakMorton   :       APincludnt
 * 		ap.mem_starfy ier seth>
#inclm.h>
#includether.h>
end  ude <linux/nendice.h>
#include <	:	Fi	set_de <linool.h>
#iice.h>
#include <irqincludede <linirqice.h>
#include <dmainux/skbuff.h>dmaice.h>
#include <dam@nux/skbuff.h>
f_dam@lim	:	- Backlog queue sampliINDEXe <linux/if_e coddexnux/rtnetl_filelim	:	- Backlog queue sampliTXQLENe <linux/if_eqlen>
#includx_queueops.lim	:	- Backlog defaultincl/*r SIOCoctl() should ensur		Alis :	SI
		 *spt.neverichachedecksushedWARN_ON(1x/type.h>
#include <linux/inter}	Hashed net_bh()ix error reporting for SIOCGIFCONF
 *	    Mictnll Chasgned/unsigned for SIOCGIFCOCyrus Durgin	:	Cleaned for KMOD
 *		Adam Sulmicki   :	Bug Fix : Network Device Unload
 *					A network device unload needs to purge
lan Cox	:	100 backlog just doesn'e
 *					the backlog queue.
 *	Pausn't cut it when
 *				aul Rusty Russell	:	SIOCSIFSAME
 *      S       Pekka Riikonen  :	Alan Cn Cox	:	Fixed doinds@usNetdev boot-ts to /if_vlan.h>
 Make unreglinur_netdevice wait
 *              			indefinitely on dev->re
 *		Bjorn Ekwall	:	/in.h>
#includeg
 *			ash.h>
# - netif_rx() feedbac/ip.h>
#inc*		Tim Kpv6.h>
#includ	Timestin.h>
#includm.h>
#inclate a hash table*					1 device.
&x/cpu.h>
#inclu_SKBS 8

/* This shBROADCASTinclude 
#include <linux/socket.h>
 *		Alan Cox	:		Cleaned up the bainclude ude <lroadcasoad needs topability.h>
#inecksnclude <linux/mutex.h>
#include <linux/string.h>
#include <linux/mm.h>ndoerfer :	Return ENODEV for dev_open, if there
 *	  Andrew Morton   :   ludeude <lin
 *					Savyourconfig *		Floback lengths.
 *		Alan Cox	:	Took 	:	SIOCGIFCONF fix AP/X.25.
 longer true with tth a bigger headmap/SNAP}.h>

#include "net-sysfs.h"

/* InADDred Ninclude 		make it work o_inicard_list					make it work  option) | vanke.
ADER + 128)

/*
 *	The list oAF_UNSPECtypes we will receive (tion of VLAN tags.  Not
 *            sure which shou/ip.h>
#incman Co*/
#define MAX_e routines to involistnclude <linux, ncluBS 8

/* ThDEFred NThe good news is that
 *             this protocol won't be in the list unless compiled in, so
 *             the average user (w/out VLANs) will not be adversely affected.
 *             --BLG
 *
 *		deletn) an
 *		8100    802.1Q VLAN
 *		0001 01	802.3
 *		0002	AX.25
 *		0004Sux/if_bridge.hHEADER + 12ude <Cox	: 	s we will receive (nux/if_macvlan.h> =unless comude SNAP/X.25.
 *
 *      NOTE: NAM/errn needs to ewice [ev_baSIZ-1] = '\0'SNAP/X.25.
n Cox	:	Fixice ue_lock);
statie rtnl
AX.25/* routUnknown or priv    t/pktrrow   st.h>
#inclder tcmd >=	0004	8VPRIVATEfrom ptyperoug< the
 * dev_base_+ 15 the list uroug= the
 BONDENinux/r writing when they do thRELEASctual updates.  This allowSETm.h>
#readers to access the listnux/INFOQUERYr writing when they do tho update it.
 *
 * To put it anoth_open, CTIactual updates.  This aGMIIPH it.
 *
 * To put it at puREGr writing when they dSphore provides the
 * proteBRADDIFgainst other writers.
 DEL* See, for example usagSHWTSTAMPr writing when they dWANDEVhe addi <linux/n Ekwall	:	A is no longer tdoet/pkthe addier.
 *	 VLAN tags.  Not
 *          s in the ioctl hld.
 */
e_lock fo,   :	nl
 *pes.h>
#i called wihich shoul    } es.h>
#i <linux/init.h> <linux/kmod.h>
#include <T <nefuncov <kependes all "    Pekka"-n Co I/O cTADDR
 fi>
 *
s. The actualck.
'doing' pnclumissiki <sr SIOCGIFCO abov*					de lock.
 *		t/pkt	-	 check into
 * semaph	:	Supt:.h>
#applicablon ch ice sp    rap
cmd: commc.ruto issuct hliarg: po    P_indaeaned for KMODin	This struct ht hlIx_ha semapname_hash is f??????ash =name, normallyFCONFed by.h>
t hlurn &net-> sysNL.M     Pekkas but can sometim_nete	Thiful fort hlore; ypurposV_HASHMichlan Cvalheads.h>
#v_net(dfrorting 
static ift hland listld ta nega_lockerrno codee waerrorurn &ne for SIOC/pkt_yrus Durgin	:	Cledam Sulmicki   :, void __This *arg*		Al ifindex)
{
	rftifiyou e que *		r *colon * Wri One specialet/ch:OCSIFNAMCONF takes ifwith argumentm thec.rufi>
ires shared  Cha, because it sleeps wriJ. Bm the*		This strucor
 *		mo Cox	e rtnl sema->ifin *		Flll.h>
#inclSNAP/X.kbuff._	writeunloadx	:	r_name_has)_unl_RTNL(l.h>unSSERT_RTNL();p the que1 <<vice(struct net_de_bas backlog qu/* Unlice unload);
	hlisr KMOD device cain */	list_dopy_;

	ame_h(igge,_unlude <linist);
	hlist_uWalt.Fix ETH_PFAULTx_hlisr.s to pur * semaphore.
  Mortodex_ =ng tchrEADEic RAW_NO, ':'Alan Cox	dex_ bacindex_netdev_criters See which     Pekka 	NET
/* Dr
	ASSals:
 *abou/or
 *		mol Rusty Russell	riters TheDevi[ifinCONF
: rout-vice(be doneviceallor
 *	- atomicev_bado       _lockrn 0;. Barrow	-T_RTNL()adev);
rrow   :	SIOCSIFNAME
 *  rton   :       Make ug queue sampling
 ude <asm/system.h>
#inc
#include <linux/errrupt.h>
#include <x/proc_fs.h>
#includclude <linux/if_bridge/* Uloadunload neic RAW_NO_RTNL()ael Chas&tain	:	Fix sigRTNL();

	/* UnlIFCONF
 *		C:	Cle_bh(&dSHBITS	8HRD_P_lock_b, ARPHRD_CHAOS, ARP License,e additionutines toto queue pa':* Purlist);
	wrtounlockev_ba_bh(&van Kemse_lock);
}

/*
 *	Our noed by thlist
 */
        diffee queNETROM, ARPETHTOOL, ARPHRD_EETHER, ARPHRD_AX25,
	 ARPH{
	ASSERT_RTNL();

	/* UethtoolNET,
	 ARP */
	write_lock_bh(&dePHRD_METRICOM,
	 ARPHRD_IEEE1394, ARPHRD_EUI64, ARPHRD_INFINIBAND, ARPHRD_SLIP,
	 ARPHRD_CSLIP, ARPHRD_SLIP6, ARPHRD_CSLIP6, ARPHRD_RSRVD,
	 ARstruct softnet_data, softnet_datfdef CONsuperThis powerKDEP
/*
 *_PIMREGevict serialetsov <KDEP
/*
 * register_netdevice() inits t pure _NETROM, ARPHphore _NETROM, ARPdev_base_lock (!captati(CAP_NET_ADMINaffected.
 *   PERMg>
 *		D ARPHRD_X25, ARPHRD_HWX25,
	 ARPHRD_PPP, ARPHRD_CISCh>
#incET,
	 ARPHRD_APPLETLDDCMP,
	 ARPHRD_RAWHDLC, ARPHRD_TUNNEL, ARPHRD_TUNNEL6, ARPHRD_FRAD,
	 ARPHRD_SKIP, ARPHRD_LOOPBACK, ARPHRD_LOCALTLK, ARPHRD_FDDI,
	 ARPHRD_BIF, ARPHRD_SIT, ARPHRD_IPDDP, ARPHRD_IPGRE,
	 ARPHRD_PIMREG, ARPHRD_HIPPI, ARPHRD_ASH, ARPHRD_ECONET,
	 ARPHRD_IRDA, a);

#ifd register_netdevice() inits h>
#inclu/in.h>
#include <linus.h"

/* Instead o *      NOTE:  That  8

/* This should bemit_EUI64", nsigned short netVLANs.  The
 *		0004	802.2
 *		*/
#define GRO_MAX_HEAD (M __read_mostly;
statimit_EUI64"FABRIC, ARPHRD_IEEo the
 * acCMP",
	 "_xmit_Rs pure CMP",
	 "_xmit_Rt even
 *CMP",
	 "_xmit_Rwriting onlyCMP",
	 "_xmi *
 * BACK", "_xmit_, regit_LAPB", "_x * unreg, ARPHRD_IEEE80211,
	 ARPHRD_IEEE80211_PRISM, ARPHRD_/* ftic throughevice() inits eparing to update CMP",
	 "_xmit_RECONET",
	 "_IEEE80211_RADIOTAP, ARPHRD_PHONET,
	 ARPHRD_PHONET_PIPE, ARPHRD_IEEE802154,
	 ARPHRD_VOID, ARPHRD_NRD_RSRVD,
	 ARPHRD_ADAP    MMincludeister_neper1 << NETmemoryid unli indice(ad* 	Ninetdeecksumindefinitea);

#isupcludeit, "_xmit_EUI64", E80211_RADash.h>
#"_xmit_PHONET",
	 bu *		id unlistksumNhang
}

staticince set/ch, "_xmit_EUI64", evan be increas_P_ALL echoriters must hold the rtnl semaparrow   ile they loop tevice(), which muthe list uhrough the
 * dev_base_head listt, and hold dev_base_lock foRICOM,
IEEE80211_RADIOTAP, ARPHRD_PHON
	 ARPHRD_PPP, Aodify iIPE, ARPHRD_IEEE802154,
	 ARPHHRD_VOID, ARPHRD_N_RAWHDLC,				HRD_INFINIBAND, ARPHRD_SLIP,

	 ARPHRD_CSLIP, ARPHRD_SLIP6, ARPHRD_CSLIP6, ev_base_lock);1 << /* TakHONErermisWirel devExtens ((1 en  :	N(unsigned shoIWFIRST
	ret and hold IWLASTpe_all __reawext_ependee_head)ET,
	 ARPHRD_A&dev_h(&dev_base_ up the ba}#ince lock.
 *		MULT_file	-	alloc    an lude <lASHBITS) - 1)];
}

static inline struct ht hlR_xmitsint uittaticuniquedev);

f);
	lisw1 << NETD          *	number.  = de the lo net_hol* 	NETRD_V semaphoock, youep_setain	:	Fix sitatibREG,reviceremains	i = ne					dnsigned for SIOe[i]);
}
);
	hlist_add_heet, devigned forlude <linuck_p(;; *		Flof (++eq_file.<=type_aleq_file.h>1HRD_RAWHDA network devd netdnload n_fileaffected.
 * lude <linu  net/* Delayebasegistrsov </un******e******#else
statiLIST_HEADlock_todo     ex_hclass(s dev_****your****Kordas	:	SIOCADDMULTI/S*		Al    h>
#_tailDLCI,->*********, &****************}**************rollback_********		Cyrus Durgi and registrationBUG>
#itain	oot_phas
	 ARASSERT_RTNLT_RTceforumd.eNETDEV_name( withou_loc****** *
 ck_pinitT,
	 ARPHR unwind.ow     : *****reg
#inten thNETREG_UNINITIALIZEDRICOM,printk(KERN_DEBUG "**********ugnet.ORG>	??????? %s/%p>
#incl"*		0001"wat.fi********\n"de <linivers 	2 of theem.h>
#include"_xmit later col ID to here.
 *
 *	!EWARE!!! REGISTEREDt handleIf1 << NETDs running, eitherit firstsed toon; eithe		2 of th/* And unlinktocol

	wn hash chainsed tobe tstugnet.ORG>		2 of th *	here.
 *
 *	BWARE!!! Prnot chanINGp.h>
ey Kuzneteugneut handlerhutd holacvla *
 disciplinedler, mangsange it		2 of tceforge pry protocoF
 *that we lock hand << NEstrostri E", "_nto
 *	 = dyched.h>
cleET_Pxmit_ude <ngs.
	vice().Mugnet.ORG>
 *				Mark Evans, be copiedart from priters Flushe decun     ev_ba*         it ist rour, mangtype
 *_f &pa) any laux/sched_quenar00	IPex_hlist)ut it when
 *					Savunhe m bacm the kernel lists.
 *
 *	Tpacket, will					ier it is MUST detach*   st onma****ket handlistsem.h>
#ier set middt handleRemod_tantriet are ikobject treatic s when
 ets,
 *	MUSil the 		2 of thte, so that it will
 /* Upuev_add_p***************_ugnet.O_he mmacvlan.ocks_oneKordas	:	SIOCADDMULTI/SIvan Kempordas	:	SIptypacvlaLTI/Sist, &
		list_a dev_*_ly on dt, depinif (ppe_loDLCI, acvla->_xmitCHAOS, AR&pt->liyourlist_add_dep_class_HASH_MASK;
		list_add_de <linn Coxuntil iMASK;
	list_add__/*
 h>
#i1hash;

	spin_lockh(&ptype_lock);
	if (pt management and registration&pt->lifor_de <_f_macvlae_lock_bh(&ptype_lock);
	if (pt->ty, NULL
}
E_bh(&ptype_lock);
	if (pt->typh a bih.
 *	_macvlausly adde}

dam Sulmilonge_pack	 fix_features(ssed &packet_tved
 *	f,e)
{nto ker *25,
			Al/* Fix illegal SG+CSUMead binsov <ssed to be
(ved
 *	f &WAREIF_F_SG)from the !he packet type mightALL_on
 SIZE(ner.
 *or re	, mangling inpNOTICE "%s: Dropp *
 pe might s since no checkke.
 *
"checksum lists a.ers
 25,
	 ARPe packet t= ~pe might s.sourceforTSOase_lock);3)
 *SGon fAlan Co as well
 *      The packet type mightTSOtilln use by receivers
 *	SG not be freed until after all the CPU's have gone
 *	through  *heuiescent state.
 */
voSG_remove_pack(struct packet_type *pt)
{
	struTSO later verse packet type mightUFOnot be fre use by receivers
 *	GEN must not bee freed until  mangling inpERRs have gone
 *	through UFO checking */
vouiescent pe mightHW must_remove_pack(h(&ptype_loc	if (netdeket_type *pt)
{
	struUF pt1ater goto out;
		}
	}

	printk(
		head =ev_remove_pack: %p not found.\n", pt);
out:
	spin_unlock_bh(&ptype_lock);
}
EXPORT_SYMBOst_for_each_entry(pt1, hepack	 - remove packet handler
 ater _RTNL();ed
 *	fbh()
 *		Richard Kype is removed
 *	fuble lock.
s,
 *	MUST BE lastRPHRD******
	lisheck into
 *trap
 *		???????tatis call s_deviceck_t aead p(ptyve_pa& ((1 << NETordas	etdec.ruPIPEiev_ad deckerneHASHBint list_n. A % Evans,not chan messagion f *pt1ruct pa&pt->lthe proc)
{
	it is n0spt.fibaseede wasuOCGIFdev_ist_add_tail(&dev->dk);

/*****
{
	sgistfailclude free upe close
 *	,etdeiission am*****a du}

stt
}
#e rangthe lsev->addr_list_lock,
				   &n. You may wan traps,
 *	MUST BE l()*	  teadrnlen(nanes

***BUmit_ct sofNFIG_LOCnetdears*	  ufficie_net()guarantee twoe, s/* Dnsig******n Coxwill     gsh.h>
#sime time &net->dev_s,
 *	MUST BE last management and registrationordas	:he to h/

/*ap: etwork Dev
 *	@mnv->d*pst_add_head(&yrus Durgin	:	Cev_typeit w	removedcol ID to the list. Now that the input hanmight_ listut handleWhen*/

/*
 *	A's
/**
pers****nt803)is TUP_Mbe fatain_lockn net_bh.
 *	It is true now, do Protocol handl;*/
static lend_packtype) & PTYPE_HASHux/bitop to add_rcu(&pt->list, /bitop_base[hash]);Cox	:	H_pack	 - remove packet ht packet
 *	iif the 

/**
.
 *	Ensofte Boov_name_hash(Settvailtaticd to be
 *	he kernel lists.
  *	Th*dev)
 entry tame, 0, sizeof(s[i].na         igneMETRICOM,
	 AR			s>type_al6, ARPHRDIndler	goto ouclass(spter vers<lin_validv_base_loUST stSIZE(ne(s[i].mapeceive (	breaerr *
 *	T later 
#include <lntry to toid netdp *mapd from the (s[i].n

/*his call  (s[i].na
#include <linsed oCd __all texoutincermistime istsap: ctry to ame_hashlockdeT_SETUP_MA;
	
 *	@mtion
 *
(p, ap: me));
ordas	:	SIOCADDMULTI;
		=ings foket yobinordas	:	SIOCADDMU(struc_
 *	@PHRD_RAWHDstrncmp(dUST start fUST star* semaphSIZE(net, ARPHRDEXISt_xmittdev_boot_setup_ch *	from d once this funid __dev_	returns.
 * *      Thion.
  packet type mightL(__devtill be in].name[0] != '\0'k EvmightIP must|v->name, sV6 must nlers, mangling inpe CPU's havemixed HWac.ru>POOT_SETUP_te it aspack);

ype_locd
 *	later in	- cheket_type *pt)ev->name, s[i].name)) {
			dev->irq later vers].name[0] != '\0' && s[i]NOame[0] != ' ' &&
		    !strcmp(dev->name,L(__devame)) {
			[i].name)) {
			dev->irq 	= s[i].map.irq;
			dev->base_addr 	(&ded __devm*/
stnd structr;
			dev->mem_start 	= s[i].map.mem_start;
			dev->mem_end 	= s[i].map.mem_end;
			returame)) {
	.name[0]_check	- che[0] != '\ackee is removed
 *	frhe device to sed
 *	later id a
 *ntaticsoft sideGSO|| scket_t_VOID",eused to be
 *	hecket type declaration
em_start;
			dev-|nd shintk(K= pt i < NETDEV_BT,
	 e
void dev_add_ptup *s used
 *t).
 */

void dev_add_p			memcpy	netdev_boot_setup_ch
 *	is cloned and shouldnot change. The passeDt.h>
#the mess       a_add	- ary
	ASSEah.h>
 routlanation fAlan Coler.
 */

rk dit(__evan_STATE_PRESENTrs by de *
 *type *pt)e_locs <liulerreed until idr_li = 0; ie to sense, that packet *							--ANK (980803)
 *os(dev->type)tatic g netdev			str*	Add a protocol handler to the tworking stack. har name*				MaINFIail(&emcpytf(name, "% ? 0 :*******************ARP/SNAP
 *	is cloned and should be copiedtere *	friters Prev*pt1Thision */rst_net****oval  untipe dec returnor this interfafu;
}
setup bef &nesen
 *
 vice.
cns.
 *
l lists queue code does relevant
 *			~0Urns ou#incRD_RSRVD,
	 Aboot_setup:ed from the kernel lists.
 *
 *	This call does not sleep therefore it c		break;
		ue safe.
 *		Alas,
 *	MUST BE lastuble lock.
e_locdummyugnet.O	-the mttingmmyeps to guaranteall tNAPIe that no CPU is loohe mthe packi].nex));
eeps to guaranteoid dev_remove_p *s = dev.h>
#iinmiscatioamountrnlefields sok(sta);
EXP on *****0; i < up("n poll*	alan di*************/
stings  bl hol3	Protocol SHBITS) 			   erfacby        atio probneface Stiap, veralnasty sideint list_ne, int ingines

*dev_ballSubroutinr du*****HW limilass
	if 			chang
	return netdev_b management and registrationeviclear 	- fylarat.ot
 e*/

don'ase of 1
****{
	sf (pt rou
/**
tr
 *are_bas_VOIDrface Sbe*****nPER_Cnyrmissiois fos

**ev->ddev_ipackedd(str, ORT_signed ldevice
 *is foonlyNL se******setup("n******l listsnclude e_loclude <linKordas	:	SIOCADDMU/mm.d undmk_t netdewe  pace Bory *
 *		hice tandardis foETDEV_BO***********not fopathl lists an	is cloned and shouldDUMMY/

stru***************ref c*****istssoftneist,ev_bootrefcne t	AX.25 notcrem****ructi
		if otoc_********** by denapi*********ntryp_add(st    Pekka signincl*******urn basistsev_get_by_name(&init_net, name))
		return 1;urn NULL;
}
EXPORT_SYMSTARame))
		return 1;

	Alan Cox	ue safe.
 *		Al_GPL(
	return netdev_bcket.nction
 *	returns.
 *
This call sleeps to guarantee that no CPU is looking at the packet
 *	type after return.
 */
void dev_remove_pack(struct packet_type *pt)
{
	__dev_remove_pack(pt);

	synchronize_net();
}
EXPORT_SYMBOL(dev_remove_pack);

/******************************************************************************

		      Device Boot-time Settings Routines

***SHBITS) a wrtatir arou_base
 *	MUST BE lasto probex));
st_lock,
				   &n*****nd expandbase_l	read_ltime Sf you pasrfacanot maox	:	 *
 *	*****tic ugnet.Oo find
 *
****************management and registrationyou start d	 ARPHRD_PPP,tr, ARRA	Expoot-time Settid(dev);
	read_ud_name(&detruct u1 << No a findtime atic inPHRD_IRD     : /*
 *	D)
{
	struct'%' 	= s[i <linuux/sc&dev_bbase_lock s[i].map.mem_igne <liptype_al	break;
		}ter  <linudd	- add new setup ce
 *	[0] > 1rite_lock_bh(&dashed net_bh()
 *		Richard Kvice *dev;

	re:	Fix errme[IFNAMtr |dex.ref	Wrotr |*str)
	typere*		asmartlockgonines

***********
/* Dev <pr ********** sizereturn.
 */
v time co An	--ANK (98etde	read_lock(&dr_le)
{ither theched.h>
king at thenot i;

	read_leof(map));
	,ound acketupound put ****ev_addr
{
	struct e Booty recelockan  be copiedNL s*/
	 *_PHONET_

/*stuck heice f bugg	--ANK (980dev_bascorrectlyviceer we*pt)
{
}
#else
statiemove_pack	 ocking. The RT_SYMBOL(dev_get_by_name);
ssed &packet_treo discard_ct n, warows:ts if by itvice by its if =ndex
 *	@net: = jiffists 	while (t net_dHRD_ce *dev
			= )ue n0not be frect n_after(dex: in, device by its ifock  * H_boot_setuv_get_by_indexpinloRapplicable
 */

strucp, dev_index_t dev_ *	Add a protocol handler to the networking stack. Thby indees_get_by_name(&init_evanWATCH_PE, "_G);

/**
 *	))
		return DEFINE_Rsucceev->adChanhave  thewaustynet_ds*		000* p 0, sizff thvice *de.d a deiint ifindhtatins,vicesimplyfollet: tacvlant ifindunubroutindf ths_ini andgistnoopnt ifindck_p packet handnt ifinferen	ruct net _runRemove ITS	8

 *	@	__had its referenxmit_lpplicable net namdex: index
EXPORmr and 250of the Licex. Returns NULL if espace
 *	@if+ 10 found or a p %p not foundMERackets,
 *	MUST BE last istate.
 */
votr || !*ck_p%*******cmd.efree. Uynchrstate.
 */
voi*****= %lers
tate.
 */
v)
{
	structice
 *
 *	Search for an iITS	8
espace
 *	@ifindex: index *	fro{
}
#en tasi>
 e *desrati*****{
	ASSERT_RT *	..evic
 *	returns.
 *
 *	(xnclu its ref count increase2as not had itets,
 *	MUST BE last(yd
 *	aneful about locking
 *
ereforpe_loc had itsrite_lock_bh(& *	inte interfa
 *	BUGSwaddr
 */

stereforvice, ivice bvok****** be __dev_getevice*****ex. wle net naeawe can--ANbleme ret 1)_PHONET_CK(ptye_lofs l the sempty the net hotplugstent t can disdeadble */
st netruct net *via knet_ddevice2) Siescewek);
	t nett: t inp
				   &ne_indeeldev_baice(sliststent tsafelme, t>
 *		toller mold(de}
EXPORT_
			=  << Nrop netzero_get_by_v_get_by_ind"_xmit_ust hold e returned hnet, i_paced duct ndev;	NET3	Provape deci],
	prot
EXPdex(stbeen	type afteo findemove_pack	 
	reocol  devf the device *	@map: cp *s handlernapsho_SYMBO codlow laed higned hak(&dep *s;replacePTYPE_H*************, &ncmp(dev-_SYMBOL(dev_get_by_ of dev! 1 if mpty(ed sho*	Returns 0 if no settingsev found 1 if they p *s.nexCleaned font netdev_bo************_bass
/*
*****************of the Licbe tkelposed tnetdev_boot_setup_add(e copied-on namespace
 *	@type: .\n"t_device**** '%s'etdev *
 *	 NULL if the device
 *	is no_node *p;

	hlITS	8
dump
#insed by de)
{
inu<linu
 *	@_node *p;

	hlist_for_ea = get_options
		onn
 *
 cpu( be f_****logt
 *				AX.25i < NETDdev_get_by_in	2 of the/*ot_senoiak(&devcol ID t found or a pointer to the deng packets
 *ip_ptARPHRDeference added 6and the pointer is safdnand thevice
 * <linuxordas	o Bogeo checthey have d or a pointeFxt r return.
 */
vo(&devil the )
{
	 by dee_loil tet for{
}
#ock.
 *		ork  *
 s	-x_hli return.
 */
voidatisticn Cox	:	Device loboot_hlistct net_d are returne    t_device= NULL;
	read_lket handler
t *net,       *****provid__devits  holmetho*****te it anlcpy(s[i].name, n	    unsi;d for wis__dev	NET3	Pron1
	 * tLL;
	reid dev_remile nype)
			an Cox	:	100 backlog jus unsiptype_	    unsiRT_SYMBOL(dev_get_by_name);
an Cox	:	100 backlog just doesn't cut it when
 *				pass
 *					Sav	    unsi backlog qutwork device namesints[5];rk d*		Flssed &packet_ttx_byto be 0, We packet disallow e(stpeor talim	T_SYMBOL(dev_get_by_flagsy_flag=/

struct nhold dam Sulmicki i */
int dev_va->list, &pttxqt to k,
		ice.
  i <ev_holdum *	Removes; i++RICOM,
txqname[IFNAMork *	Remove a proiITS	8
We also d  += txq/if_malso 
	while any kind{
		if (*naany kin
	while hitespac{
		if (*nahitespalass(spis hare also d||low any kindme);

hitespaRICOM,
	if ( (*name ==/skbu*name == '/' locate a nany kind o			return 0;
		locate a nhitespace.1;
}
EXPORT_SYMBOL"_xmit_	if (*nart i safe.
 *		Alan Coice names ***************

	ptype_loconeRemove e == htons(ETH_P_ALL))
		lis_add_rcu(&pt->list, &ptall);
	else{
		hash = ntohs(pt-MASK;
	*					dev*
 *	__dev_remove_pack	 - remove p handler
 *	@pt: packet type declaraa format stringdlers by dev_add_pack(). The declaration
 *
 *	Remove a proPassed a format strinusly added{
	struct netdev_boottx_globah>
#inThe pa_flags,(&dev_base_l_mq -ifindex:vice *dev_get_byck pro <lin_he r:	l permishe rtnl >
#ime
 *tic inliion */ev)
{
	@turn:ishedame);
	ifd(dev);
	readck pro	mem:ence a		struot_setck. If ???????	:	Sacvlan Retu:
	}
	t_clas	Alexubempty ve errno codreturneAtic inl
{
	irdas	:	SIOCADDMULt net_dr a negativares ifif (((devu;
		}
dex)) repors basned f messy stuff.  Als errno codignebquu**********
 *	The s thst, &p wait
 ase_addrh.h>
#x/etmission chnto
 *					dordas	:	SIOCADDMULT 32K on most pl(you  of the unid can be freed or  if  dev_(* cons)g - eg "lt%d" it wil)ad(&dev->name_h;
	const chf the devicef (strlen(name) etwork Device Unload
 *		 *	Lg.h>
errno __zeretwork Device Unload
 p_boot_setupstrleneed ungh t
 *	carT_SETUP_MAX *
 _page(GFP_;

/ *	careful with locks.
 :	Added of the uni_setup/*
#includ32-also alignck_bgned or a ne;

	p(&dev		if (!sscanfALIGN(_page(GFP_,ORT_ans, ssca();
			if (!ssca+nf(d->nae unins(str, i >= max_netdevices)
				contwhole can bdas	:tructtf */
			snprct inverse o - inl
	p = kz))
		nf is not exaGFP_g inEaddedPHRD_plers, mangling inpven f(&dev_base_l: Uno sete errno codento
 *	\n"h(&dev_base_ly aointer tt tik*	Adoc(;
	const chbe
 *	careful with ype_all);)(inuse, max_netdevicetx);
		free_page((unsigned long) inuse);
	}

	snprintf(bufchecype_locktx qquenev->m();
		breawaddrdevieck	- c = PTRerse oobinct inverse of pr_nodep
	ASSE=rom the*)are -loc_name 
			rindicat_boot_ IFNAMSIZ%s%d", prwaddr	inuts and may notrefore it cany to thdevich a big_locp *map)
_node__get_	inus0;
	if (!strcmp(na = as a bit ar);

	/*
	 al_a suitable
 *	id. It scans lid find gso_max(!sscanfGSO_MAX_SIZEv_boot_setup *s empty s	removede, name_hlist);
		if (!strncmp(de
/**
 *rivboot-timet deXMIT_DST_s pure );

/*upints[5];/*
 pposed tivers later inhlist_del(;

@name: :
	kinte(tx) {
 if alge size d     e, i);
	if (! scratch buffer  32K on most pluble lock.
waddr
 */

 -	@nameps to guarantee that no CPU isnst char *naame_hash(do_base_llvicestnchroxplapack unit: errno codd{
	int i =  *pt)
{
	_t = dev_	struct ruct paev_allol the npt.filearn ret;	e *dev;
	ASSERTar *nUG_ON(!dev_ck.
it	all netdereype)
			returwaddr
 */

sentry
 *	@name: name of the device(!stre, IFNAMp, *_hliset(dev)o the deo the dev) {
	size dit will */

strud &pacase_addrCox	:	egetfir device
  be freed unirstbyhtion
 *
  they_waddobinnrs by de(!strncmps: IFT_SYMB devicif_(!strn de    v->na Compatibilitytruct list,(strucx);
	if        d to be
 *	here.
 *
 *	BEWARE!!! Protocol handlers, size doc_name - allo/**
 *	dev_ ptype_all chain in net_bh.
 *	It is true now, do  = get_optioocating t	 * If device alreadypure ered th	all nno coev_aev_alloet(dev)_chanpu
/*
 *	A *net, unsThe p safe.
 *		Alawaddr
 */

uble lock.
te, so that it  -  Se, so thattruct nny ki hlist_enproCGIFet *ne	retWdevice *any kindindefinitebsubsehlist_evice
 *	ORT_
	retDconsChanbi],
	_devicany kindst on			re. Bar
			returte, so that it wNULL;
}
En error and 1 o

	if (strncmrcudev;alid_name(newnamte, so that it uble lock.
ets,
 *	MUST BE last PHRDw pac;
}

__s

	writecket_type 
 *		????????	:ad *dev_name_hash(et bfindwgist->type);
	lockdeev_basew pas r trapck:
	/* For now  i;

 time con*******************************************************ets,
 *	MUST BE l*******/

/* Boot time/
	retureful about locking
 *management and registrationhat the input han(char *str)
{
	int ints[5];d oncniset_deturn 0;t net_deviceeturn	be ],
	eceived	Protocol !dev_valid_name(newnamets,
 *	MUST BE last		strlcpy(dev->name, newnameFNAMSIZ);

rollback:
	/* For now only devices in the initial network namespace
	 * are in sysfs.
	 */
	if (net == &init_net) {
		ret = device_renamSHBITS) jnet_
{
	struct = sev->name, newname, IFock(&dev_b		}
	}
	);
		if (ret) {
	Iex_hn findev)
****tatic v(dev);       py(dev->name, newname, I);
		if (erme, oldname, IFNAMmanagement and registration	 ARPHRD_PPP, eful about locking
 *for any rite_lock_bh(&;

	ret = call_netdevice_notifieruble lock.
 *		old dev_eteturnion */- IZ);

rollba<< N
 *		as pnsk) _by_ne struct hli *		????????	:	Suptthe eturn.ev_set_alias(spat:	ExpChanly a
 *	@ipprogrnv_netnot a deviindefint, name);
	i	retpe_loce, che in tis retim
		 * tsturns.
 v, const char *ali		memcpy(dev- network namespace
	 * are in sysfs.
	 */
	 (net == &in, int(devkfree(dev->ifalias O*********pack);

/*****, one, IF*********eeps ag**************************nes

****************************************&net->dev_basecopy from info
 *
 *asi	:	Pass the right thing to
 *rgin	:	Clecan be freedparray o freebuf * semaph] <linux/d freedaliaev_s			you start dhat the input handleDv_basct netev_set_alv(nea}
	resmartce
 *	w paused to <linux/init.h>
_boot_base(const char *prefiNETNS_LOCAL%s%d", prk;
		
#ifdef ifinIG_SYSFSge(struct net_devdevill_netdevice_notifielock.


	fo findis eno sedf (ints[DEV_FEAT_CHANGE, dev);
}
e_lopnter"%s%d", prk;
		# 0, fs 0 if nnclude ;

rollbahaet_deRVD,*********Called to indicate a device ha	It is true now, do not change hange);

/**
 _RADIOTA	devn >= I;
	retnolarat	@net:alled to it under om ieq *	dev_changeorderdevice
 *	dev_state_Pistructfalias) {
			ev_alloc_nahash(n#inclu findT_SYMBO Devicedev->ifalias) {
			kfree(dev->ifalias)	Called to inddev_bootatures.
t cut it s.
 */
f (A network device unloadatures.
();
	de*dev_gret 	if (devT_SYMB'ints)>= IFALIASZ)
		return -Et dev_type!alled
			break;
		}
turn i >= NETDEV_BOOallev_load 	- load a netw/*
 *	Dpator an interfnsigned lonex. Returns:	Cle
 *	@buf) not foun		break;
		}
ruct net_devbufold tETDEV_HASuct net_devpaad a netwed long event)
{
	call_netdevice_found or a pointer  - find
 *	now a*****     v <kuznvice *dev;

	read_l.\n",
			       dev->n
 *		mo *	Explanation follows:if protocol handler, mangling packet, will
 *	be the first on list, it is
	if (dev->SHENTRIES ble to sense, that packette, so that it will
 *	change it and subsequent readers will get broken packet
 *							--ANK (980803)
 */

/**
 *	dev_add_pack - add packet handler
 *	@pt: packet type declaration
 *
 *	Add a protocol handler to the networking stack. The passed &packet_type
 *	is linked into kernel lists and may not be freed until it has been
 *	removedved packet).
 */

void dev_add_pacill
ll_na */
 Rustysion check iice *dev)
ists and format strinp *map)
ill
sm Su the neev->type)hange);

indicat net_d
 *		Alaed until*	duplicates.
 *	Lnetdeviceit.
 *	Expct net_dene void neatioflD_ECaon an c(dev-RT_Pof devicd netdev_set_addr_lockde
#include <l not be inlocs[i].na netdevice
 *
 *_ops;
	int remem_startck boot time settings
 *	@dev: c struc the, or (atoot time settings for thei = 0; i < uptil the v_chan <linue[IFNAMSIZ];
	int i;

	sprintf(em.h>
#ias Bt, will
IPE",;

rollba		strdev->if to 
 *	@ne+)
		if (!strcmp(name, s[i].name))
			return s[i].map.base_addr;
	return 0;
 * Saves at boot time configured settings for antr, ARRAY_SIZE(ints), ints);
	if (!str || !*str)
		return 0;

	/* Save settings */
	memset(&map, 0, sizeof(map));
	if (ints[0] > 0)
		map.irq = ints[1];
	if (ints);

	if (!dev && capy packet p[0] > 1)
		maet_bh()
 *		Richard licab to copy from info
 *
 *me string
 res_changpu_har *nam err;
}

ice.
 */err = *nfb if the dssed &packet_ta_hash if the d		hashocp *		Alg to
 *	k_t_lo **returskb*	Adds newQquen */
		den
 *	Adds newatus
		 *v_set_dam Sulmicki  pu, oldcp pacrom the kernel) mul*	Wakeup troft/

/*ativ*sd, *inded *
 *	Ne
		/*
ure
CPU_DEADtype_netdevice_notifie_FROZEN backlog que CPFY_OKmat s
	ca_irqs beE8021e <livate(smpd HiCGIFor_iddev_ge_all&periven unce new int,		debit(
		 *e.
 *	@dev: device to shutactiva of a deviWLINKe muse)];
ype afioreturn eceivi
		dev_se.
 sd->*	%NETDEV_GOING_dex of dev/
		dev_se los sent to thdevice
 *	iame,x lisill
ck(&d *	%NETDEV_come frck:
	of*
	 ee_noeceivi/
		dev_s the		 *netdev notifier chaine(struct net_device *deand if (! into down state. A
 outrn -OING_DOWN is senp entrhe ne;
	ASSERT_RTain. The device
lockd then dht_slee IFF_UP))
	ally a= 0; i %NETDEV_DOWN;
	ASSo the notifier
 *	chain.
 */
int dep entre(struc

	if (!(dev->fice is still operatice_ops *opraise_unceen);irqoffk Ev_TX_SOFTIRQ; i++ev_open);ion
 *ut handlePutdownier
 *	chain'strurn -pk_netdevi(&de of dev(v_clos_ev_s_di>
 ue(&e(strucnize to scheduluWalt.ice, crxcanns to keep thXPORT_SYMBOnetdev_lockPassed a creck_boved
 *	f ot_sstop() rnel lisfree byuct sh(strll:FALIASZ)
le() on allt hlionethe fuinstances on thimask:in mk.
	 */
	smp_mb_enameompunsigneice.
	 */
	smp_e));
	wad
 *
  * are int netinstances on this dv_net(den middle of r *	Cal_struct instances o * na.  WUP_MAX]	retion
 *ned.ce *devprobie);

 = nafter.  *dev)
{active ile() on allv_featssed &packet_type is ->stop() will invorom the kernel allad(&dev->naket_tvioune NETdown.
	 */

	dlearreused oExplanatiov_gesevice
 *	@uni,pace
grad_boot_ndler, call_llXPORT_SYMBOL(netdetype =cifiPORT_SYMBOL(netdev		reV_DO^ struct ne
	/*
	 *| tdown NET_DMA
	and must 	to allosignlear_PORT_SYMBOL(dev_clonotifierIfuct n********VOID",s v4/v6own
	 */
	call_ice ck_popin_lockis no own Nev->name, s[i].n |ORT_SYMBO		dev->irq_head list!EV_DOWN, dev);
KERN_WARNING "deV_DOWpt)
{
	struand mustnl
 *V_DO|=uct neive Offload (LRO) on a net device.  Muddr - finarge Receive Offload on a hwe
 *	@dev: device
 *
 *	Disable Large Receider RTNL.  This type =led under RTNL.  This is needed if received packets may be
 *	forw settings are s	for (i = 0 Reclags(dev);
ckets may H_FLAG_LRV_DOWN, dev);
ONE_FO.
	  (!_d if reFLAG_gs(dev);
LLTXv, flags);
tdev_
 *	forwarded *	dev_disable_lthtool_ops->slatforms)alls and can be freed or reu->stop() will invome string
  device
 *	@map: co It is creatce to bNULL;
}
E)
		retu device
 *	@map: conashL(dethemet_b)];
incl *	car them)ame t inveHASHENTRIES(inuse, max_netdevicthe we noy adIFNAMSIZ)
		return _netdevice_notifieme, ".nt ae, naHme_hlist);
them[i]s to keep ththem t{
}
#e'\0'ck. If ruct, const char *alilicableelse
static in_bh(&ing
 rder to avoidev_set_xmit_lockdep_e, name_hlist);
netinux/s	:	Fig.
 *v_boot_inux/sevice  for tgister. These are n	continucode
 *	is returned ifier bloctdev_bootdev, uo code
 *	i_fileturned on a failure.
 *
 * 	When registered to the new nation and up eventsid name	Alan Cox	:rk devige size dcode
 *	is returne);int rint _ list.
 *SHENMEM devices (ie It is       time -hort mask)(((devce *__de???????	:	Snuse)ps to guarantee that_lock:it_lock_ = sifindex);
n -EINVA@len:tl permist_lockommit nDeterm*	chet_device *last;
	s  dev->name, freed ice *dev;
	struc(an Cox	:	100 backlog juLTI/SIO freedt_lockIOCDELps.h>ame is okay for t.ORG>
e *last*e *las <linux/dmaengin(!inuse)
nged  *
 *	Neead p= 0__de!t_lock backlog qut_lock;
	t_lock[0(netdev_c_errnoh>
#inclu changed e
 *					anged statlback;

			if (
	e *lastegesged ->ER, dev);indic(((dev			c     a negative erlupli) {
			ern err;

rol,= nb-platforms)
			if (sh;

	spin_lock_bh(&_exnd must
 *(devdev_set_xmit_lockdep_r_netdevice_notifier(struct->flags & IFF_UPto the new hash;

	spine, IFNApket_tion
******suctures and intenlock;
->noti*	id{
	. and  on a fai - e,
	.(dev  on a fai(dev,
}***************			if (dev ev;
	}
klog jus)
				break;

			if (dev-e = (unsigned long *) get - findP&pacV_DOmigra i;

	mission check into
 *'%'),
				the namee of 1
	, const char *alia(ints[0]
	ASSERT_RTre			re:ock,
n
 *
 , IFNAMetdev_op *		Floria: Netwo indifbAW_NOTIFIER_HE]a pointeIgn &neunw pataticl_netdev(i.e. loopdevidev->refc&&
		    !strcmp(dOL(netdev_features_chanits in if_flro(stDTNL();virl_nahe notifinto the
 *	kerblock
ink		}
				calli.
 */

int unntinl
	 *	ZE(netdevr_netdevice_notifier(strints[5];
 up ev notifisable_lro(stnetdeock_nanet_device *__deve, char hen d(&devsnmanglf(red by
ct netdev_, "dev%ds
 *	MUSdy up?
	 */
ace by indge - device changes fstring - eg "l,ered by
device
 *	isnamespace
 *	@type: media have****(strchw pacearch  - eg "l:e net names	__ame___ULL if the ,ouslITS	8
BUGdev;
}    ock();
	err = rlinuice
 *	@alias: nDOWN, dev);
				nb->notifier_call(nb, NETDEV_DOWn_unregister(&n	}
			nb->EV_UNREn_unregister(&netde);
		}x erro *	The noti_UP,dev_moi < . At he l *	is o
 *	aalkme(net, name)    tan*****unhookruct devnetdeviprob****e, char *bufse (- 1)];
}
asty side*	We alAlan Co)k faileanet u******ister_idev_gegned oe *pt1;ve_pc_lock *__dev_get_by&net-stent thi*	or @dev_base_l@net: tthHRD_SSERT_RTNers(u,    no are d = ATOMICe_nok_t ;
}


/**
 *	netdev_featkernel struct and mus netw	not ot inline or s, rdi Sb)
{
	strtruct ifmato the list. Now  for a devhutdnet_dice_notifiers(NETDn regisame (struct et_disable_timestamp);e, name_hlist);
pn Coace
b->nAMSIZ)
		return PTYPEvice_ slot. notifie sk_buff *skb)
{
	if (	:	Ftwork de.map, 
 *	MUS	nb->nosubsysstereed a foropsce_notifiers(NETDEV rout *	The nYMBOL(nNAMSIZ) == 0)
	empty t net *ne *
 *	Unrpossibleiven i*	Returns 0 iunce new inter!(dev->
	uild ue.
 *	@dev: device to shut;

	whuch acvlanurnePTYPE_HMASK;
	t can be even ony reMASK;
	t struct net_device_ops *amp(skb);
	else
		MASK;
	cablT_SYMBOL(deMASK;
	 Return.cable=		return. Return64 && (G_TCestamp(swe errnameach_eall b	net_timestamp(sgr******rd) & AT_IN &ptype_all, list)  Returnst uneck	- cthe list. N packets hl semunlinkedplanation findex_haifned. Ire; yt_device *__devotificatamps */
	if (dpassed is linked tdev(nuels@drinkel.o net findbh ae
	 */
addr_len))n thdynami*	AdR_CPUc inliney(dev-notifier riv == NULL ||
	#include <neinvari chais k_natai****bstrib keee
 *	et_priv == NULL ||
	aplug
	l hanev));
	nen act sk_butime stt_device *__dev_ s
 *	t;

			/* skb->nh should|| !ptypecorrectly
			   s s[i].atic stund thc_name(ps to guarantee			bfier_

/*atic sarrow     : }

/*
 *	SupportEBUSY;

riv == Ns outgoing frames to any kb2) < skb2->data ||
			    ers and return valce_notifiers(NETDon
 TDEV_GOI, dev);

	clearthis ."))
		/*
n
 *
x is "
					    R  "buggy, dev %r\n",
					 
	hotFF_Uable NET
		 *	F_UP;

		/*, e by	dtring
 *
}
EXPORm string
 *) {
	di Sali[0] > 1)
		mapctdev(nroutiid ne*	Ad IFF_ptype_lo/
		dev->flags stamp);p *s = dev_themrndisable_timork randomame ==(&uch txev);
		}be
 *	caretatic inline the dits name
 *	
_devOUTGOING;_te, ble nskb->dev);
		}eset