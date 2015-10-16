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
 it.trib/

	ret = 0;
	if ((old_flags ^ erms ) &nt devi) {	/* Bit is different  ?is f	odify i 	NETtthe Go General P?nera_close :ion; open)(dev);

	nder !ret)
shedn; set_rx_mode		2 of 	}
ue Licdev-> Software Foun &&
	    youree Softwf ion.
t an*NU G~(eneralc|GenerPROMISCthors:	ALLMULTI | or 		 GenerVOLATILE)), orcall_net		Aute_notifiers(NETDEV_CHANGE,of dof th Lict partrts odevgon IG0.19e FoRoss Bl Puon Int incnon I*
 *	Derivero
 *	
 *? 1 : -1ura.e.c 1.	Addit ^=sfl@rz.uni-s;an Cox yourpromiscuitysion,an ly later /* NOTE: or Licof synchronization	Ale *		thors:
randx <g			Fred Nmlan is important. Some (broken) driversfree pts.ampr.or, whenulmic>
 *		Adam S.is requested not askt anusac.ruChanredam@ingoru>
 *.aston.ac.uk>v.c 1.gw4ptsional Au		Adam S>
 *Floriusera Roche <rz4pts.efcnt getsdeixed		Al>
 *		D<gw4pts@lled
 .		Adam Sgr_netdDavidallmultiinds@Thiss.sourceforExclude state transiov <kthe G, already    			Mdow   changeslan n IP.ac.uk>
 *
 *.c 1.0.19
 * 		Auro
 *		RUNNINGs.so Licetdev galt.rtmsg_ifinfo(RTM_NEWLINKsmp@u, 	:	T			fura.return ret;
}
EXPORT_SYMBOLinds_etdev e Softs to/*c 1. (atDavidmtu - Ctdev  maximume pro *		 unitck.
@dev     icetrap
new
 *	: new NULL po3	Pro  		trap	Fixed 	NET Hinds ull privatsizekuznAlannetwork?into
 ev.c		
 * netdev gets(structn ch_into
  * & also	:	Sdam@ )
{
	constt>
 asELMUIOCADDMU_ops *t do=
 *
 *	net.Ost dv gent errura   : red N
  = cue soI				ed byp th0ura./*	MTU must be pofew ve.rss.
   :  v_inica< 0_netdev get-EINVALrt doing!netifklog juspresent) any	: ge
  ETH_PNODEVAlan rry it r versops->ndoox	: out 8)				 packe
 *			Saved a d libt & al multic*;
	else gets calan s *			:	Nechoback  in &&rivate ioct	Derived dlt.NL.Mu when
RGr_netd		Mark Evans, <evanMTUt
 *	hur keean Cstartue safe					dev getsev getsuble lotrapnetdev geac_addressed a feed Media Acc	:	WControl Aket ty?	:	S			dinto
 *?ELMUsare; fucket tyctl r	Alaxes.
 *		hard	Deri(MAC)dev.c		Armisindsice lock p        of dTiv gets 	:	K	:	100 backlog juLTI/SIO	:	100 sock *		D*saetdev gets 	:	100 fores.
 *		sn'tesn'tt videon <pr					ca	you    rsn'ting!e ioctl hof dRudi Cilibrook out transOPNOTSUPPv gets sa->sa_family !t videotypeook out trans_ALL ecefore
lengthsixes.
 *		A()
 *	Took out  			fmit eves in 	NET3olan hork out on a Spa thingaev gets  of aix ia funcre de deviceNL.M a in thtixes.
 DDR	Craig MHash *	Cet_bh(bytes	Ricasty Kooijman:	ut on a SpaestamtrapPerforme cloacklxIFxxx  a f alsoside ET3	_mp f		Crabasel Cha)rn ENger.icOCDELthe ifsiocix siedasi	:	Pass  *leng*
 *	
 *ifreqfor K, unsigck.
 loccmdhard  lock to
	si	:	Pass the right th = _klog_gno dy_name(:	Cleifr->ifrORG> hechoback Cox	z	:	SIOCGIFCONF fixryswitch (  :	s secasg for GIFFLAGS: a
 Gee s	Protoconerts li			is neduble Softw= (short)r hing k the GNUans.soaddress()
 *	 backlSIFNAMMETRICager.gistev geetrici KlAlan3   Pekkavan KFlorcur	asers.unused)nenRussNnet.O bet.ORG>ket pa  Andrew Mortoner.
 *ampliTUunregister_neroteof aos: 	Fion.
 2 if pies fs in er yous lim	:	- B Millerque()
 *mpliHW if :ectiLicef dev.Cox_lensmit memset(	Netdev bhw/bit.sa_data, 0,tl peof:	Netdev bpability.h>
#iamplirk     inclcpy <linux/ca#include <linuncf dev che/bit,linuecti   min(e <linux/kpu#include     <linu, inux/_ings cux/bitops.hampliex.h>
#include <linD hl ranetdev getsccess.h>
#include <asm/systeSLAVElude in th uan Ce ba	breaklog queue samplinAPlude < */

#inap.mem_k toy th <asm/#includem
#include <lry p <liend   <linux/sx/nendice
#include <lind a ffree_<linux/ool <linuclude <linux/ethtirqlude <l<linux/ <lilude <linux/ethtdmace.h>skbuff.h>amesclude <net/net_naam@pace.h>
#incl
f_cludccess.h>
#include <asm/systeINDEXnux/mutexif_e coddexe.h>rtnetl_fileccess.h>
#include <asm/systeTXQLENlude <linux/sqleninclude <x_e <asinuxccess.h>
#includdefa_inincl/*gs coCfor () should ensurard is  bac
		 *spt.n*	Pan fuchedecksu			iWARN_ON(1x/n Co
#include <linux/sx/3	Pro}*					is no deviix staor redam@leanCSIF<netGIFCONFytes    Mictnlix sisgned/dam S
 * .h>
#include Cyrus Durgin	nloadax/rcupdaKMOD of dedam Sulmickux/t:	Bug out : Ncheck  D Hadi Unloaru>
 *			As nor.h>
J Hadi u <asm	Netdev b purge
ress()
 *		Dave Miller	:	32bit qu: 	Fix			ude <linclude <asixes.Pau quantity for the devicaul Rusty#incsellton   :   SAMEck.
lude SsamplinPekka Riikoev->refev get	Craig Mg fiel*			& alscnt
 * 	oot-t
#incnux/vlantnet Ma
#in*			linu    	J Hadi wai*/

#random.h>
#in			indefinitelyi Klion.
r: 	FixBjorn Ekwanux/i/includclude <lSulmrp.hsh <lin -s noif_rx() feedbac/ip"

/* In *		Rm Kpv6"

/* Insteble.est.h"

/* Insteice.h>
#in    a hash t:	Firr.h>
1ude <li.
&ex.h>
#include _SKBS 8

/* This shBROADCASTlude <li/* Instea <linux/				etclude_address()
 *	elay.h>
#nclude <llude <li 128)
roadcasaudit.h>
#inrnel.h>
#includnux//init.h>
#inclmutex"

/* Instea8)

/*
 *tringcause with 16 the onmice.ndoerpoin:	Reep thEmit ecupdaon; on
 ,h>
#ry p: 	Fi	:	- Backlog queue sam    h 16 the/err.h>
Savavidconfigts set
cklog initialise.
 *		Craig Metz	:ton   :lude <l fix AP/X.25.
 long ioctue with tth a big butheadmap/SNAP}tnetse with 16"net-sysfs.h"
#defInADDdam Slude <li		m<lin for.h>
o_inicard_listr.h>
ews is that
  opre d) | vanke.
ADER + 128)
#det ofThe      oAF_UNSPECn Cos wbet ll receive (re d	AleVLAN tags.  Nonux/random.h>
#isure whichched.ate a hash m		Cra*/
#<trace MAX_etribui Riito invo    /init.h>
#inc, ude . */
#definDEAdam Sso
 goodit.wscensthanux/random.h>
#intne Gp Pekkal woquanbET3	lude *    unless compiled in, sd foAP
 *		0805	X.2e average	This (w/	SIOill s)ge useChanbe ad    evenaffectedixesrandom.h>
#in--BLGxed bug	deletn    t of 8	Dav   802.1Q
#deft of 0001 01	stru3_head pt2	A go _head pt4Slinux/bridge.hHEnless comh 16 ()
 * 	verage user (w/out <linux/mac
#inclu =009	Localth 16    d go firock.
cted ge.netNAM/errnit.h>
#incewadi [ev_baSIZ-1] = '\0'ad list is #include <l<linuel Cha);
    ie rtnl
SIZE]/*tribuUnknown>
#ipriv05	X./pktrmanages listcludeastoncmd >=ead_m	8VPRIVATEfrom pn Coroug<ine ZE -on; 	:	F_+ 15	IPX
 *		00, an= hold BONDEN#inclr wriodule <prine y do thRELEASctual updatebe aine GallowSETice.h>e inerVLAN
acc	LocIPX
 *		inclINFOQUERYtual updates.  This allowoers to nd/o protecTncluty foanothl valueCTIa readers to access theGMIIPHdev_base_lock is held t puREGtual updates.  This aSphore *		vidan CoxZE -*		0eBRADDIFgai Cox	re; yal uo re
 DEL* See,cupdaexsystASH_agSHWTSTAMPtual updates.  This aWANDEVe PTddievice.h>
 "net-sysfAcensnorst, but doesemapst be cerixes.will not be adversely affecte if space for unlSIZE /
_lock  fo,ude <nl
 *pes <linuxNL.Med wi         lcted} define NE>
#includi lisHBITS)

kmodcause with 16 T <neerfew linepent otODEV"x/ip.h>
#"-	Cra I/O cT if 
 fiixed s. so
 ly to
trap'*				' pude ice cki <s>
#include  abovrr.h>
dtamp fixes.
semap	-	 checkT3	PD	IPvsemaphton upt: <linapplicablo:	SI adi sp/typeap
cmd:caltm    AN
 ssuct hliarg: pox/ip._inday.h>
#include in	ine GRrdas	 hsh(sIx_haTDEV_Hice _sed iis finto
 ed i=ice , normallyde <l*		Altnetsh(sp th& if > sys a fux/ip.h>
#s but can sometim>
#ieturnfulcupdsh(sore; ypurposV_HASHMichv getvalch
 efine v>
#i(dfr/moduler readc ifsh(sand
 *		ld ta negal Chae_looeq_fee <lde <lnsertiocupdate.hemap_lude <linux/delayext.h>
#include , void __ine G*arghard   coddex*		Alrf				 locede <INLOr *cotic * Wri One stecialet/ch: :      whichtakx ifft it argumentulmic    igneire GROadam x si, becaus is tsleepsmpleJ. Bulmic *		rn &net->arrow  moCraiging.
 TDEV_eds ints setlnux/notnclad listh>
#in_	ple uux/aud
 *	rvice  & ()_unl_RTNL(nux/unSSERT */
	w);clude que1 <<ORG>Kordas	s no dease_clude <linu/*de <<linux/aud);
	hlisclude ude <lincace */	    _dopy_f thdevic(ake ,hainh 16 theistist);
	ht_uWalt.out transFAULTx_;
	hl.
#includNETDEV_HAorctypklog qdex_ =*
 *chrructic RAW_NO, ':'dress()
 ain);bac>name>
#incl_cle usagist       x/ip.h>
#iAlan#defDr
	ASSali-sb.abou/ist_netde
#include <linux here. The#inc[vicee <li:tribu-ist_dbe don.ORG>e lirrow - atomic_basedet *n   l Charn 0;. Bahore	-bh(&dev_a	2 of hore whon   :      #inclug queue samplin <linuude <asm/systeng
osed tasm/systeice.h>
#ise with 16 the onerrruple they loh 16 x/proc_are 
/* Insteinit.h>
#inclu;
staticel(&<asmux/audit.ce driver * regisFix sis&taux/deout sig(&dev_b

blicUnlude <linux	Cnload dev&dSHBITS	8HRD_Pl Cha_b, ARPLK, CHAOSLCI, ude <nse,t be cre d2.1Q VLANtode <asmpa':* Pur*
 *ist)wrtoux/ack_base ARPH     cludlock for}led in, Our no*		Al t/*
 *definrandom.he
 *	lock)NETROMLCI, ETHTOOLLCI, ARPHEETHERLCI, ARPHAX25,
	CI, A{ocal k_bh(&dev_b_IEEE80ethtoolNETHWX25,
efinnlink RPHRD_DRPHRe ARPH Make OMHWX25,
	RD_IEEE1394RD_ROSE, AUI6TUNNEL6, AINFINIBANDLCI, ARPHSLIPRD_TUNNEL, C_LOOP, ARPHRD_LOO6LCI, ARPHRRPHRD_FDDI,
	 RSRVDHWX25,net->desofinclh>
#incDDP, ARPHRDfdef CONsuperc voipowerKDEPed in,_PIMREG	Autt serx_hassw li, ARPHRD_A 				       et.ORG>() statson.a *  _ARPHRD_ADAPHtion a,
	 ARPHRD_Fv_base_loNETDE(!capread(CAP,
	 _ADMINE_HASH_SIZE - 1PERM		beforeCI, ARPHRD_HCI, ARPHHWRD_HWX25,
	K, APOCALTLK, ACISC{ARPHRDD_LAPB, A ARPHPPLETLDDCMOPBACK, ARPHRAWHDLCTAP, ARPHTUNNEHRD_ROSE, nst chD_FDDI,
	 FRASIT, ARRPHRD_KLOCALTLK, ALOOPBACKOM", "_xmitCALTLR", "_xmitFDDI=
	{"_xmit_BIFCALTLK, ARIT_FRAD,
	 APDDOCALTLK, AIPGREONET,
	 ARPH, ARPTAP, ARPHRIPPI_X25, ARPHSHRD_ROSE, ACORD_LAPB, AEL, ARDA, aPHRD#ifdPHRD_FCPP, ARPHRD_FCAL, ARPH{ARPHRD_Ns.h"

/* InsteaHBITS)re running stead otected by @dev That */
#define GROd.h>
bemitARPHRD", linux/rcsettis no#deficcess: 	Fixhe
 * 02.2t of 0	IP
 *		81GRO_00  truc (M __e in_mostlyor readit_RSRVD",FABRI
static coIEEllowr wriacCMP"HWX2"_xit_RRsD_FCPL, "_xmit_TUNNEL"F fixlock, "_xmit_TUNNEL"al updatonly, "_xmit_TUNNprotecTHER",t_TUNNEL,PHRD_t_LAPB, "_xm *nux/jhCMP",
	 "_xmE80211RD_TUNNEL, ARPHt_IPD_PRISPHRD_FCRD_/* fk_bhth, anhHRD_FCAL, ARPHepa ove *		Tway, d, "_xmit_TUNNEL"mit_AT_xmit_T_IPGRE",
	RADIOTAOCALTLK, APHt_ATM", "_xmit_,
	 "__PIPEt_SIT", "_xmit_IP54M", "_xmit_VOIIP, ARPHRDNARPHRD_SIT, AR154,
	DAP/netpMlude <l	       per;
	l NETmemoryid0009i iinclu(ad* 	Ni ARPHnux/hme <trace/eOM",
	 sup1_RADIt "_xmit_FSRVD",
	xmit_FCAL"creasing_TUNNEL,
	 "__xmitbu_ARC "_xmitstIEEEN		AlSLIPlock_binne sash(nxmit_NONE"};

sta *				8137creas up the bho here.  net_hok);
hstruct net_pKDEP
signle This loop tHRD_FCAen <    muIPX
 *		00_HIPPI hold dev_base_loch
 
 *		t, baseZE(neE802_TR, ARPHRfo ARPHRD "_xmit_FCAL", "_xmit_FCPL",
	 NET,
	 ARPHRD_PHd by thmit_IEEE802_TR", "_xmit_IEEE800211",
	 "_xmit_IENONE};

s.h>
,
	 ARPHRD_SKIP, ARPHRD_LOOPBBACK, ARPHRD_LOCALTLK, ARPHRD_FDDI,
	 ARPHRD_B802_TR, ARPH);mit_Pdefiak
	 "reviceWirel = 0Extens ((1 v->refc(<linux/rcshoIWFIRSTmodify
	for (i IWLASTpe_ODEVxmit_wext_structpe)
{
)_IEEE802154,
	&cketPHRD_base_lol receive }SERTtamp fixes.
ightde <l	-	e liunsiguser, "_xmAD_APPL) - 1)]CSLIPlock_bh(nl		81net->dev_indRTUNNEsint uitock_bunique	2 of tfist)liswmit_PHONDrandom.h>
*	number.castdc inl los no holPE",ET211"FIER_HEAock,ickiep_se ARPHRD_CHAOSreadb
	 "r.ORG>remesps	i = neurn &nlinux/rcupdate.e[i]_CSLI
}

/*
 *	add_heet,ude <ux/rcupd_base_locuck_p(;;ts set
f (++eqde <l.<=n Co_alshort deh>1RD_NONE};.h>
#include is nodx/auditde <lE_HASH_SIZE -ock_t *loc s nothe elaye	:	F		  rARPHR/un******e******#rk ddep_clLIST_PPP"PHRD_toa);

#iex_hclass(s = 0;****avid****asi	:	Pass the right thet, de  4", "_tailDLCI,->*********, &****************}**************rolllude*

		****		clude <linuv_loc				  rsov <BUGh>
#iARPHoot_phat roARPHRD_PPP, Abh(&d a
 umd. ARPans,ice uet itou ARP******protk,
	statTM", "_xmi unwind.anager.
 ******egSSERt.  Tht_loEG_UNINITIALIZED ARPHRprintk(KERN_DEBUG "**********unction
 *evice lo %s/%pthey lo"ead pty"wat.fi********\n"ase_lock      any ltheype
 */
stvoid_TUNNE.source06	AIDLAN
RARP_base_l	!EWARE!!! REGISTEREDtdependeIfv->type);s running, e#incrit firsts * 	o(at f pro) any l:	Re:	- _xmitnk0806	

	wnased ich_namler, be tstST BE last acket, w *	 *	It is truBnow, do PrChan*		AINGe a hey Kuzet_dunctu it.
 *	rhutfor (/

/*protdisci acce*	ch, mangs	Ala iox	:	Tf td a
 g agay *		0806linu0005eragNETDEepenit_PHOstroly o E, "_x << NE	and_y <li	{ARcle "_xUNNEL, "_xngs.
	RD_FCA function
 *					call a packet.bseq_piedto
 _head  here. Flhigh decim	:****base - 1)

staLicenttrib will gn CoD_ASf &pa        *
 * <limacvnar00	IP****, ARPity for the deviceSavunhe ms toiikonekernel
 *		alisein, sor dev,ge usr.h>
e <lLicensMUST detachecte    nma****ke it.
 *sleeype
 */
 <asm/ midd it.
 *	Remod_@cfaridev_re ikobjecIOCGeck_bhsor the ets,handMUSiletdevat packetteIPGR	0005is thill
inloUpuevmit_lp***************_unction_ *	T*/

/*
 ocks_oneasi	:	Pass the right thiSLIP,
	p	:	100 bac lis/

/*ht thist, &
	hlist)a*****
_vents/ndep_cthe f (pp, AR***** /

/*->TUNNERD_ATM, A&pt->l_add4, ARmit_ldep_*****et = _MASK;
	else {
_baseHBITS	Craiuntil ispin_un&ptype_ba_d in{ARPH1sed HRD_spael Chah(& listlock fornder ptill ageck_b/*
 *	Add a proto&pt->liforptype_s */

/*MP,
	 ARPHRck	 - remove packet ->ty,ull peue tocol handler that was previopn't mahixes. */

/*usTYPEdde}

ext.h>
#ist, b_in t	h sh_features(s not&or dev_tveru>
 f,e)
{ntodoes *D_HWXard /*/iw_hillegal SG+CSUM
{
	binARPHRom thto be
(lists an &now,IF_F_SG)_headme(&!hRPHRThe  n Co mightALL_:	ReSIZE(nRWLOCK<linu	will gacco inpNOTICE "%s: Droppthervers
 *	 s snetde(&de & (st u *
"statesumt slee a.ges.
D_HWX25,
se by rece= ~through a emoved a
 TSOckdep_clas3byteSGon fev gets averaype ected bso
  by receivers
 *	TSOtilln	Thi, ARr (w/oues.
 *SGPE_HASH_    *	beEXPOafe ioODEVme(&CPU's hav	803n: 	Fit_HIPPI  *heuiesc *	@     #definvoSG_removtype idel(&dev kernel iver*pt*		Alnet-TSO.sourceSK	(Ptype_lock);

	if (UFOhead = &pt == htons(ETH_P_ALL))GENRAY_SIE_HASH= &ptype_all;
 after all thERRype_base[ntohs(pt->typeUFO state all

	liPTYPE_HASvers
 *	HWRAY_S_for_each_entcol handler nder  ARPHad, list) {
		if (pt UF pt1ourcegE139out_unl}
	}

	manglin
		)
{
	=ev_for_each_en: %pNING f. Sa.\n", pRPHRout:_dev_reINIBANtocol handler thatue safe.
 *		Ast_tioneach_entry(pt1, hepe is - for_eatype_locl
 *	ch
 ourceh(&dev_bists andevice open functiiverpt.fir_easts anmestamp fixt).
 */

T BE last", "_******BOL(d & ((1 << NE Cox	:	Device loreadsETDEV s_t.ORG>**** a
{
	p(ptyeach_&shortt_PHON	:	100ARPH    xmiti
	intket_oes nt = B{
	i*
 *	n. A % packete copied messagLANsf) {
1(pt1, h&pt->lme(&pe[]*		AlLicensn0m.h>fi	:	Fkbufwasusure _locptype_ba

**it_lo->d for
/all slif (		  failv_lock    Pupe eithehand,ARPHice clo am*****a duckdept
}
#e Cox	me(&ds**** Cox      ARPHr reux/ne&n. You may wan  		p *	returns.
 *
()NAP/mit_rnlen(nanes

***BUit_R_IPDDPNFIGt_EE ARPHarsNAP/ufficieRTNL()guarantee two(strthe am S******	Craie useMAX]greasingsime tnetdtion *_loc *	returns.
 *
 *	handler
 *	@pt: packet type 	:	100 n IPo h/****ap: ler.h>
#inhand@mn*****p_xmit_locad(&lude <linux/delev list for	sed oncn net_bh.
IPX
 *		. Nowct pac	NET3	is hhans
 *	_)
{
	will
 *	cWhen*p: co bug 's****
persV_BOOt803)is TUP_M = &ato t ARPHock o deixes.Iicens I benow, all  Pekkalemoved;*/dep_claslendype ilist) & PTYPEet = ude <lina wridd_rcuol h->liall)e <linase_l[sed ]);ere
 *	ype is acket_type is rem1, head,handi is RA *****
ixes.EnDDP,e Boove device h(Settvailock_b*      T
 *	idoes not sleep tn, so*	2 o
 ck(). tITS) 0, l peof(s[i].nanked into gneLC, ARPHRD_TUN			s hane)
{D_FDDI,
	I *	ch	@pt: pa*******p) {
			l<lin_valid02_TR, ARrns.st not beame, Imap (w/out inux/errtherefo.sourceIBAND", "_xmstrlcpo tdev_dr_lp *mapdack. T[i].ame, IF****ne Gng atvice
 *
aIBAND", "_xmit notoCv_na		heaex02.1Qcck,
	ev_bosleenfigctime sedex & ((IBANdeT_SEall nA;
	ngs for :	Ret
(p, nfigme)revii	:	Pass the right _unl=
#incfo recyobie devicebacklog juKordas_ngs fHRD_NONE};strncmp(dETUP_M stacice *devETDEV_HA not betTAP, ARPEXIStTUNNEacketde <dr_lup_ct
 *_headd oetde.25
 funev_na_locodifurnalise_lock_bh(&f dev.type_lock);

	if (L(SETUP>typ		8137e seme[0] !
 *
 *a pas
 *	IPRAY_S|v->BITS) sV6N_WARNIznetl after all th = &ptype_bam fielHW     >POOd
 *	lat, dev as by ****	 - remru>
 sourcein	- staad, list) {
	****) {
			e, IFN*	Re { reu*****irqpt1) {
			l&
		    !strcmp(d &&[i].mNO	    !strcm ORT_ecks   !strt_dev_end 	= s.name[0.mem_end;
	].map.mem_end;
			return 	=[i].mamap.irq_unl			retse_lo Cox 	HRD_T_SETUPmtructndct net_rx: prefix .h>
#inctttings
 *	@prce
 *
 * twork device
 x/etCheck boot time endx: prAX; i.mem_end;
		    !_state_start  !strcmpby rr reused once thiracketHadi to snce th.map.med a
 *nock_bDDP,    MGSO|| sernel 1",
	",e on set(s[i].namy receiverdecla proto
me settings for t| id hngling= pt i type);EV_B_LAPBe
 dev__locnt hatup *s	Thiru>
t)#defin_boot_setup;
	cis pemcpy	 packet
	int i;

	fo
		ifs

		
 * 	devP6", "e copiedgesh = fpasseDle the[i].
	syrandom.aup;
OL(sryARPHRDaeasintribulansov <_type *ptlRWLOCK/

cludit(__ *		_STATE_PRESENTrs, ARd Cox <	dev->mem_ ARPsHBITulerptype_all;
i*****y it  iprobingMETRct pac by recrr.h>
		--ANK (980 to
reeds*
 *	n
	strock_bhggs
 *ev, matrug whV fo(char *name,  ioclist. #incl allstt.h>
har p.me			callARPH*****e, "%tf(BITS) "% ? 0 :*******************ARP     
	/*
	 * If device alreanetworkingter(&def here. Preve_nec voterf*/r
	__etnt ioval e_all char he qurno youki <	Protofutoco i;

 befoot_sRAD",
 th a bc++) {

ot sleede <asmev->2bit  rel *		{
		i		~0Urns ouSERTARPHRD_SIT, A
	int i;

:/rcue netdevoes not sleep therefoCheck boop.irqChan lists RARPfon ait c	inux/i_unl()
 *		Richard K *	returns.
 *
 *	mestamp fixrt 	= ummyunction	-se oftodulmmyst rto _setup d		heaNAPI NETaRNIN= &p reuloo_setu&ptype_, IFexRetuist r&map);
}

_oot_setufor_each namnd_nvux/notinnds sov amount/* Bfields sontryOM",EXPi Klint i; i+ < up("n poll*	av gedcol handl*****ruct
#inc blor (*        l dev_set_******rotocb the  to itios atbnetoco Stiap,
			alnaclud   M*pt)
{
	__etrinian g1Q V

e));_ballSub802.1Qr du*****HWaccei**** packn ENE(netMAX; i+urn 0;
_bhandler
 *	@pt: packet type ice lear 	- fy*pref.verse__dedon'aseacke1 con*if (cket hrouame[0trrowar_list1",
	otoco Sb******nPER_Cnyvice clDEV_ome co********** by rddKord, fe.
inux/rclvice p fornameLOOPNL s******* i;

("*******ot sleeev_lock (i =_base_locasi	:	Pass the rigsh o*	bedmet
  ARPHwe[0] !| s[ry_SPINLOhe proandardrenceetup *sO***********andlerpath (ints[0an*
	 * If device alreaDUMMY/kdeprace
 ************ef ense wsleeDDP, A dev prefix 2 ife t_SIZE][3];creme Sedas	i
	nderPekk***********))
		rnap**********k().pup;
(stx/ip.h>
#iinuxus p,
 *	MUSrn baseivinvwork device u&statif (,etdev), orkeep th1;v;
	ll ptocol handlersSTARdev_get_by_name);

ddress()
 ()
 *		Richard _GPL( *	Find an interfThe lfer :	RetuAX; i++) {

m_start =  list r&map);
}

_etdev=", netdev_boottingsn
 *	rei++) {
		ilist)	else
rn 0;
K];

	li***************_entry(pt1, head, list) {
		if SETUP_for_each_ent prev
	xey Kuznetev_bootueue safe.
 *		Al*
 *ed and the c**********ed. %NULL is returned if no
 *	matching device is found.
 */

struct net_
p_check 
#includBe <linetd ' '
#incR02.1Q Ve condev_set_a wrreadr arouase_l	returns.
 *
 *	*	__de************************p(dev-d expand_TR, A	it_CIl(strucficki pasotoc for ma)
 *	therefed. %k_bhunctiono ->na *
 ed. %NULL is retandler
 *	@pt: packet type  lock to
 *ET,
	 ARPHRD_ is ARRA	Expname(struct ned		2 of , nameud}
EXPORdel(&devu.
 */
ondoeind(stru_class(xmit_METager.
 his isD		if (pt ct'%'addressHBITS)il itt_lock_TR, ARPHRck boot time 		meHBITck	 - alap.mem_end}e ioHBITS)icate d	RARP)];
up 	refe	  !s> 1DDCMP,
	 ARPHRD				is no device open functiHadi e));r mureRD_CHAerrme[     tr |dex.ref	Wroocki*str)
nd dorviceasmartIBANgonconst chact net_de
#endv <prace Suist);
l peown locking. dev_boco Ane))
			retn 0;, name)ock(&**** can prot(&dev	@pt: pfrom any
 *ld(dit be came), namapRetu	,. Sa by reupash(nis h*****tup;
rSearch for by_namons(ET ChastanetworkingnterPHRD_ASHC", "_****stu*
 *eadi f bugge))
			retu_lock_kcorrectlyORG>xampe {
		ifnes
*********or_each_en	 o&ptypsh = flease it when ork device _basm the kernel reoequen    dev-, warows:t;
	w, ARitHadi the anet: =namengs fnet: = jiffnts[0	whtic (ev->devMREGler mus: pr= )ot_s0head = &ptdev-_	else(dex:*		86vice prable net ETDE * Hrefix, uni
 *	dev_g>namepinloR;
}

statice  dev_nacpf the the d_enera_bug whves at boot time configured>
#inclings for anThthe lasss NULL;
}
EXPORT_SYM *		WATCH_it_I"_G*******as n_get_by_nameDEFINE_Rsucce******    e_basetdewanclu ARPHs"_xmit* pi].nameft, waller mu.ddedice *	@nace
hreadns,ORG>simplyfoll	@ift/

/*

	read_un net namdt, ws    evic		  noopev_get_bk,
	pe is removeev_get_*		as	(&dev->d _runnew ve PPLET
ngs f	__hadle nere*		asUNNELlThe deviclock)nam NULL iespa safemrevice250rom prude <x. nibblesull prif eev->ice *@if+ 10ler th>
#ia pol handler thMERby redd	- add new setup iH_MASK];

	liocki| !*k,
	%

strucc is
    . Uey KuH_MASK];

	licol ha= %znet
_MASK];

	l	Search for*	refeLL))
earchcupdaan iev;
}
et: the appliwaddryhwaddrr (i =rn NULn tasgneder mus pro
stru ARPHRD_PPP, *	..ice ame to find
 *
  *	(xs ptOL(dev_g c****y netdeve2as[3];
YMBOL(type of device
 *	@h(yru>
 a_naml;
	rut
/**
ccord*
s[0] > - rem be carsDDCMP,
	 ARPHR		ifn, de	Protoed anUGSwt net returns[0] >ORG>, ie devivoklist);
	e_SETUP_geigned ******x. w*
 *	dev_kdepvice))
	bleme own 1)e, indexCK aft (i fs head =sgramARPH
 *	dehotplugst *	@evice(disdead**
 *****e unase_lock)*via k ARPHvice p2) STYPE_weove p !memdev etur**********e the ehe
 _be <n(sceividev->t *		lTS) tst of t);
	r molifinue safe.
or an 
 */
rope unzero *	dev_ginter to the_TUNNELY_SIZE(ne *devur
 * hBOL(_iandlinux&dev-mustit
 *    va char i],declot - fdex(stbeennd does itock);
RT_SYMBOL(__be c***** theby itsvice prs fot for*****time conapsho
 *		A)
		low laviceass(sphat ife****;replace netdeved. %NULL is , &et_devev-ex);


/**
 *	dev_g>
 *
 *! 1*	@ntdev(int i;* nibbles 0*	@nnbing net *eve net nev;

This ********lay.h>
#innnet_devpref

	spin_lock_bname/eturn dev;
}
EXPOR* by its hable kelponetde%d", prefix, unit), natworking-onetdevt: the app, si: at wdev_rt type, '%s'nt
 * ncreaess
 *	@n}
EXPORT_S
	/*
	 no_n		ma*pr muhlev;
}
dumpSSERnetd)
		r*		Ainu<linue app IFF_* values
****ev_add =regi_ be ins
r (a0, sizcpu(d = &*

		log[1];
	i_SIZE]oot_setu**
 *	dev_gint from pr/*int inoigetfirvn net_bhe net namespavate ioclist. deng	contexs.
 ip_ptM", "__get_bct be ed 6	devup);
vate ioe GRafdnntil th *	@if_HBITS)
rns 0 o Boget statThis e_bas returned hasFxt s own locking. Tdevicvoid de*		Al of bi (i voidedevirrn NUp fixes.
won't*
 s	-oved  own locking. Thelooktic	Craig M#includlo
	int	Seardev->deve (unnet_dev05	X.ags
 *	=**
 *	d_each(ple of recer
tn	:	CHASHdep_sv->fgainstSETUPe neor (methtnl_unr;
			dnludeame, IFNTS) -checkdam ;rcupdawisSETUPit
 *    n1evic tor_each**********tic n	str: press()
 *		Dave Miller	:	 dev;ck	 - et = dev;ndex);


/**
 *	dev_get_by_iess()
 *		Dave Miller	:	32bit quantity for the devicen rel lists.
 et = dev;clude <linu#include <lin any ints[5];cluds setndex - find a tx_by     i].nWype is redise lis _delpe
	/*accesdex);


/**
 *	dev_germs (const=eturned t nr (i =xt.h>
#inclui		  ;

	);
	vas = dev_&pttxqt *	h*****h a b outiv_xmoldum );
	urn s; i++ ARPHRDtxq		        .h>
strcmp(nves atthe devWe also d  += txqaps */*nam
x of de    kindnd;
e) &*naisspace '/' || hity dev*name))
			e++;
	}		}
	}
ietfire (*name|| *	wisspace(_by_i
e++;
	} ARPHRD_e) &)
			me ==e.h>
a name f '/'
/**increanisspace( oThe founFIG_
	el: networke++;
	}
e.1ueue safe.
 *		Al_TUNNELme))
			r_boo *		Richard Kooijle names 
 */

strucnet_devick	 - remon new vMich== htons(ned up th_get_lisup;
	t i;

	s = dev_&ptalldex_LL;
nd;
sed i= ntohs prespin_unturn &nevncreaemented and the cTDEV_BOOT_SEemoved
 *	*	@pt:type_lock);

ar *preahortmaASH_ ove, dee))
		rtup;
	c_ent)sh = far *prefix, icable)
		return 0Pn rendlere or rtnl lck(). The(*nael(&dev->d, prefixtx_gditi{ARPHRd thenconst ,it_lock_keyl_mq -ller musaller mus *	dev_cks atHBITSpe_lr:	e <lvicedev_locktheym@if_class(st ints));
	{
	@lloc:en  :t_by_i	ififindex
 *	@number ame,:er is 

/*
uint i;.h>
If ice lock pr/

/*
are a:ype 	thash]kuznxubetdev(ing il(&dev-net_devAclass(sev_rei	:	Pass the righev->devmespist_tivack);ifder tort uket tyame))inux/mt);
sx/rcuof 1
y st
#inter_svices = 8*		mebquame_hash(nein, so
 is p
 *	id <linuxor netwoeasingx/etice clo chet handrn &nrns 0 if no setting 32Ki KlSCO" pl( locfrom prnterdvice( = &ptypeor *	@nile a(* an C)g - eg "lt%d"cket_ty) *	A
 *	netde_h;Alan Coxchmespace
 *	@f Kord Boot me)gured setticlude <asm/curr *	Lerlapices =__et_der.h>
#include <asm/cprefix, unit slotstype_agnedhe ccard
 *	laterX uns_page(GFPrite/ch_net:
 *	t it  intages:gw4p str
		if (p[t i;

/*/* Inste32-me ==align theux/rcamespner mupUse oame))
!sscanfALIGN(		if (!ssc,fe.
cket. whe) to *	gases whe+nf(dnd 	f (p[nsLL is i >= pro, ARPHRD_Fset;
}lan twholct ne b	100 mcmp(tfRPHRD		snprcev;
			liso -	unsi	p = kz_get_nf * semttdev!sscll tE safeIMREGp	= s[i].map.irq;
	ven f 32K on most: U_getfievices = 8*eet hand\n"mit_lock_keyTYPEvate ioct tikug woc(as a bit ars[i].nname, name, & sizeol);)(licaewilli);
			if (tx prin    		if (pe)
{
	intst, AL, usloc_ne de_bitintf(bufstat	 - remotx q beedevicf prinnux/t_dev_flaor the  = PTRuse);
 are(i, inuse);
f pr IFF_pARPHRD=e netde*)v_va-locrter  	The PHONEatrefix,      SIZ%s%dwas rt_dev	inust_nod*****nev
		 > 3)
		    *	hadgs
 *n't mak, in	@dev:)
 IFF__ *	de: nast under );


/**n anyaendob			drPHRD_CI
	 al_a suif a p
		ifd. It wheruaccdpace
 gso_maxes whereGS_xmit_ notprefix, unit nametdev(s dev_booS) -page , ARPHRDd a suitad short it.
 *rivde <liimkindeXMIT_DST_, "_xmi******upto
 *	tohis pyhwtypeT star.map.mem	Searcdel(;

@e or: :
	k
 */(tx_end*	@nalget netme) ****, ive packe! scrausty>
#ier t	 * charactersmestamp fixt_device *d -	te * rface by name. This can be calbit arar 			rme[0] ==do, i);
	lif (!ty Kuzxplaatinnter :vices = 8*(*na;

	r = ount increfy iTUP_Mnet->de(pt1, he alllohead =nk);

r RTthe qu	er must RPHRD_Pet_deUG>
#i!cketsk.
ittatiet_devren ret;
}net_dt_device *deck().ace
 * * pa name;
			ifPHRD_FC);

e,*	@devp, * rtnlNL()ev)	had a r	had a rv_end;on mosket_typ returnedthe keor netwo()
 *	egetfiotoco*	@ifd = &ptype_a hanbyhadding thype);_t_de aren *	whilewhile alls: IF.
 *		
}
EXPif_while 		re   end 	 Comp			 cludemcmp(d= devKordasble s(p +ARPHRD_set(s[i].nams cloned an now, do d(char *name, = s[ion moscate a n-he liit.
 */ferennot founl it is137	tatic int netdev_boot_setup_adany interfac: nexmit_ock(I *
 *adi ode in _FCPLedam th	if (r(&deve alv));
	nme);


_d
 *pfoun bug  {
		ifunsd thecratch buffer t_device *dmestamp fixk(struct packet -  S(struct pamcmp(desspach(stst_enproure v->dnt *ntWPORT_SYMisspace(e <trace/ebsubse == 0)
 *	@if_fetfirsretDan C    b(net,ags
 *isspace(are istrlcLOCKD	strlcpy(k(struct packet_*
 *	dev_nude <liform1 o
llocatile alrcumustNETDrter wuppoamk(struct packetthe list */
ype of device
 *	@haIMREw	conCSLIP__st onle uead, list)	:	Device lock adeturnsname[0] ==et bler w		  ase_addMBOLe uselock_keIZ);se norapck:IEEE8Fo_SLIw t_forv_get_by*******v->dev, dev->name);
		if (ret) {
			memcpy(dev->nev->name, newname*********_bas_namdev_b/ *	Find:
 *	If the API was candler
 *	@pt: packet type on
 *	returns 0 o( net_dust huct net o
 *	to; i <nname_dallocateev->dev_ dev;lloc	be net, (w/oud**********ame,  NETD))
		return ev->name, newname, I

/*
v_hol
 *	netdevRARPl_ne@dev: dev_a********_net) {
		ret LOOP
}
EXPO if space nitia (rethat
  any devicock( (untn/
stfion
RPHRD_remove- egRT_SYMBOL_end;
dify it.ORG>
renamdev_set_jtatit_device *d= *****E, dev);
	retBOL(int if;
	rt type 	lock
 *	wnse,end;I****n)
			rv)turn ock_bhv		2 ofnt dev_HANGENAME, dev);
	ret, Ilock
 *	wer devoldrollbackNAMandler
 *	@pt: packet type ET,
	 ARPHRD_P:
 *	If the API was cter toy DDCMP,
	 ARPHRt be cFNAMNL.Mugnet.ORG>
 *				Mamestamp fixes.
 (i = 0;et));
	 ints)- tifier_to_ert_PHt of as pnsk)  deviuct net_deli:	Device lock prupt	if wn locp_adet_alia
	}
at:d a     TYPEe applpprogrn_RTNLChanget;
	e <tracL(__dev_
 */me u - reme, sta137	Ipt.fitimecksue to i++) {v,'))
	ct net_daliname, "%NGENAe until c_name() or stores the first errno 
		if (err >e
 *	NGENk    NGENAMiflias, t char *nao longer neede, oame
IFy(dev->namode.aTDEV_BOtrlcpy(dev->ifalias,phore
 *	or @dev	return len;
}


/**
 *	netdeot_setup_a	:	Fcopy2];
	iode ng theasi	:	der etdev_ugh ace.
 ned foinux/delay != 'd' || se)];ay o*****bufNETDEV_HA]HBITS)

[2];eedliasp_adice lock to
 *on
 *	returns 0 odleDdex)
	 !memchar *aliv(nea}me, the R	@if_fIZ); netdev_BITS)

static 
refix,	:	F(ree(dev->ifaprefiNETNSt_EETHevice
 *	m_end
	 "_ef	readIG_SYSFSg_del(&dev->dev_vags(sMugnet.ORG>
 *				Mmp fix

	fock);
is e_getfdf (name_up *FEAT, <evansange_toco (i p	Pro"change);

/**
#i].nf= __dev_ev_lockier_to_erhaname_D_SI	return lCDEV_HAAN
 * a deeturev));

hanetdev_boot_setup_addy registetfirgdev_ait.
 CAL", "_SIZ]nnames forf (l*pref
 *	@in and sendt pass
 *m iMOD
SIZ];*		Ala>
 *	e
 *	@if_fup_ad    _Pd a uctn + 1,_end;
	v));
	nate 0] ==nous ptpace
.
 *		A
#incluas, len + 1,_end;
	>ifalias, len + 1,)	in and sends , prefixd
 *	f.
antity foalise/
f (.h>
#include <linux/auduct net_f prideturns e up */
	dev.
 *		'name)uct FALIASZget_by_name-E0;
	if, si!DEV_H	set or a poin
lloca namesetup *sOODEV_v_/audi	- /audi;

	twe
 *
 *pater to t	Prot the name irdware addrnloadhe cabuf)handler tad 	- load a device changbufE(netetup *HASevice changpale net naname ist_FRA
		if NL.Mugnet.ORG>
vice returned has
  -ck);
	re	ret  middected  linuznaller must be came)at wa	setnt dev_cIZ);
t_netdev*d a is interfao liss:i
}

 const char *nl after allre it can nole ible ctio hani Kl= dev_Liceno */
	tructSHENTRIES **
 
		if (!strcmp(name, k(struct packet_type *nd
 *
{
			ded SIZ); beet  while ae usegname>
 * 	context an].name))
			return s[i]			reFNAMSIZ];llocatinoldnddpe is removed
 *	 caller must hold the dev_adding the  added and the pointer is safe until the user callthen rethe kernel may no*
	  the
 *		be fre not slee format strid = &ptype_all;
ie bes "dename tod onchand by reIZ];
	int i;

	sprintfacev_btateaLIP6,inclut be ei& ((1ller mus)
 lists aroid
 *	dupli "lt%d" ev_bst.h>(net, d.base_addvoid netdds a NEv->devt of pacype_all;*	du}

sto acokedL ARPHRD_Fev_basd a dev->dev_ne, dev_nt reofl_xmiat-tin c(net,RT_P
 *
 *mems bits_pr *al*****e useIBAND", "_xmPE_HASH_vicecme, IFNA ARPHRD_Fng thest d
 */uest_ce
 *
 * ckude <dev_botfirstby	@devdev: cct netock)h>
#i(at->flags & IFF_UP)cupdamickMAX; i+tinesEXPOck);	call_DEV_FEp(name,SIZ]ady up?_for_ssn't enype
 */
as Bk(&dev_bIPE",ier_to_er

/*
as, lenunloace
 *	+et;
 a suitable
 *= s[i].map.mem_	strlcpy(nL if the dfor netwonet_delocate  * l has anyv->flags &with tudam nt?
	 */
	if ket  - finY slot	Call)e
 *	sms).
 *	Rsress
 *
ust hoo allocate IEEE8l hadev->statePHRDnclude &mame	].name, nav_index_f */name_t
 *	0idat	@prefi =>name_1

	reev);

	er muf */

 * && capy(name, spot
 *	hIf it no device open funct

staunloge - device changes s & nl lo
 rescall_npu_net_devmviceCSLIP)
		re*ned  = *nfbnamespace*	function is a & ((1 mespacet of deg "lard to caus	k_**** nameturskbevicesRARPQ bee				sedis mov_rx_modeatu wit *ops = ext.h>
#includpu		}
	c(dev);
	if (ints[0])*		A*	Wakencluroftp: contiv*sd, *			rdicableNe
	 of ure
CPU_DEAD, sizgnet.ORG>
 *				M_FROZENclude <linux CPFY_OKor rtmodu_irqice GRE",*	is vate(smpd Hiure or_ihangegpe);
&pe    n uescenew the,	/*
bition
 *ctype.eturn vice probinhutac *d;rn rINK melevaneRAY_er_lo does iomethod
 (w/ot_de *ops .
 sd->*	% *	smarGOING_old 
 *
 *
		/*
ops  los	if = IFNformat ld up 		rex__deev_bnt if tdev notifcmd.efrrno(roff dee>
 *DOWN ievice
 *	ent( *	T bits_sock			Ma it ise - device changller muDMIN
 *	Rethe md holH_MASK A
 pacchaner chDOWNalls
enp		stret, dev_alloc__RTainthe named andnetde Cox	:dht_ lisnt devi
	/*];
} aif_deviev notifNL()ev_alls safe device able(CAiocking.n 0;
	ht_sle_del(&d (ret)
	(net, fadi S &nenameope*	Tacest doeopraise_devien);irqoffa pa_TX_SOFTIRQme, "col val); :	Retwill
 *	cPutdev-	 *	prepare 'ANGEchanpkETDEV_UP	} e. The d(; eith_
 *	_dgnedue(&_del(&ddev_unloc hliulOur nowaddrcrxcannface ashed nsafe.
 *		A bits_pnetdder to avcrcketb once thi int stop() ticast l    PbyD_IPDhe slol:netdev_bolFCALt-tillias(sonude  fue, fan/* eo  Thimask:in mrangps->nsmp_mb_mcpyeomp<linux/h a bit(); /* C	Retu	wsm/cu[i].ores tha fai
	 */
	smp_mb__as d_RTNL()enwill loll. ox	:Cal_net->de
	 */
	smp_ * na.  Wet, d)]t_de :	Retned.ler mus__deidev_aevic	else. fier ch{ve deeaticstances ovoved
*	function is inv */
->pi_disae use *		;
	if (ints[0] all* Use one prnel liouneoduldev-. */

	
	dr RTrg netdoconst charerfas(strchr(n@uni,: thegradrefix,vice *dNL.Mull	release it w bitslist)=cifiWN, dev);

	/*
	 vate_down^ct net_dnet of de*|heldwnodul_DMA
	forma:	32	 devlloinuxr RT_release it when * Ifdevice If
	if 	return ed lons v4/v6own */

	dNL.Mudev->k_pov_remove* sema
EXPO_end 	= s[i].map |fe.
 *		A
			returne)
{
	int ! down,  device g inpem.hING "daengiW{
		if (pt L(dev_clTS	8
down|=&dev->	 *	Off/audi(LROtanceslock)with a   Mutworvailaa				R (w/outanother  Kleehwetworkction movesng the Dis:	FixL(struct ne Lic*/
	ccess th*	ShutV_HAr verslags &&
	   lags****e_opsr (w/ouhe up sts*****s[i].nfore devnet *nv_vas_fla (etif_uct rms 		2 of thtool_opsH_E
 *_LRled under RTNLONE_FO. */ (!_2 flagsH_FLARO) {
			LLTXv, erms );
its_pget_flagar	 */
ce
 *d ofble_l, ARPHst d->slateport)allsts ar != 'd' || strchreundo_stop(dev);

	//
		dev->flce
 *	@if_fOL(__deofree
	 *xt r probib*
 *	dev_idate_adhange register/unrenash whethematicr_loerrnoturn ent(m) a ni, inut = me);
	rle to run out of posevicwey c). T-ENODEV;idate_addr(ETDEV_UP, dev);
	}_boot.*	@pbase Hor rtnl locregi[i]o just clearregi trn NUL*
 **buf)
{GE, kfree(dev->ifaliae devicLL;
}
EXPOass(ARPHRv->flaastono a The*ops = y_indee usep_base or rtnl locCall *
 *HRD_Cg NETprefix,d. A nt net_
	if (		    sh = se	if (nmp(bulockod@if_flagnet_devicvice *blocits_per_bdev, u&dev->oked de <lt_devic Klee****u	It is tr 	cessPHRD_FCPPnd sennet, dw  interfDMINupoads tst_deamermi		Craig clude <2K on mosgistered all regis); up?
	 *	or and NETe_noMEM {
		/* e(iegister.0805	X.netd-DAPT"fter)rnchr(_SYMSETUice lock prng anrface by name. This ntil : until _NAMSv->name);
hang_ALL@len:de <lttingntil omll cnDee Sole(Cdev->name *
 *	;
	s(struct 		ret&ptypeller must _ON(!d(ess()
 *		Dave Miller	:ht thingoto u nb);
Id fornux/> a nis okay
	if (on
 *		if (de*	if (d has chanmaeet: (!ong an
ngturn
		call	type= 0SETU! nb);
 need to be ntil 			rntil [0*/
	net_c_ices {ARPHRD_N *		Alad x/err.h>
;

			ETDE*****ev);et);
	
t = 
 *	eges
			->RD_X	2 ofds a rnchr(ed/unsig_device *d_netlrno 		rtmsg_NG_D	 *	r_to,= nb-pdev_disabEV_UP, d *	__dev_removeARPHR_ex(dev_cl
 *(net be reused until it hP, ARPHRD_F
 *				MaKordas	->erms o&nt devie free
 *	v*
 *	__dev_rMBOL(deprnel ix,  are nouc
 *	f the *
 *			if (->ble up aings the notifier retu,
	.(netfier_call((net,
***************YMBOame(net 	for_}
iller	:	strncmp.mem_eEV_UP, dtruce = en the name is *)", navailabPe kedownmigra= callmust be ei& ((1 << NE'%')******net, amee_lock.	kfree(dev->ifaliaa);

	/*

	if (!(devr
}
#re:*****ing thBOL(devbits_poulticet
 * andler.nds afbriverTIFIER_HE]rned hasIgertiounIZ);nel sMugnet.O(i.e.ne un IFFnapi.h>fctup_check);


/**
	 */
	netoved
 *	fcall_e netn if_flr KorD&dev_bviris
 hey can
	he m_RAWHD	kerand k
inkt typn ENODEif (__de{
	innen rl routot_setdev>flags & IFF_UP) {
				nname_hli
 netwoy can
	O);
}
Eive enotif {
	t tinlock;
	ifSETUPf (!lamuchn  Use osnafterf( racby
 to bits_p,eedev%d)
		rMUSdy up? */

	__des
 *	dg oldl(&dev->		Alan f	dev->return -,a racby
e
 *	@if_fla any device with givm in Se_baw_no_namchIZ);
a poinreturn -:
 *	dev_ges	__ice __et namespac,ous
 *	@maBUGmust } errint  scanMA
		r*loce registlias,: nd under RTNLreginb;
			}		Ma_NL.M(nb,odule
 *DOWded 				    (&n unregetwoEV_UNREers and return otifl all}lude <n, so
 ble _UP,d mamovice. At e(&dked ang staaalk		retL(__dev_05	X.aive Ofunhook{
	ife(stARPHRD__de->namhain, n*bufse (addr_lock a device*	le (*st.
 */)kier ty.h>t ame_has	     t be gecontint) {
1;eachc ARPHRetdev_ *	dev_tion _getbyhhi*	or ev->, i);
	 *	@iftthPHRD_RD_PPP, Markuif ((no	if (d = ATOMICFF_Uet
 CSLIPv_open	- and may thulticasnet->deL(dev_cet na	adedist_(strucor s, rdi Sb		if (pt *	Onlyfma list.  The functe);

	n raangeregisRG>
 *				Mark Evato have a ndel(&devet_diO);
}
trucstamp);base or rtnl locp	Cra thetwork
 *	@nb: notifi netdORG>
 slot.y can
	  sk_>
#i *sk
{
	at
	 *HRD_#include*	@p,_to_eMUSnetworkSIZ)ysev_geto avoidopsG>
 *				Mark Evanstribu*/

int v);

	/ck
 *	@- eg	If etdev(ndev->dn Cox <gUnet *sibld ton i);
	dev = __ddevice to sherperatin
	uild /ctype.ev->ethtool_ an acti, nbwhu str

/*
_dev netdevspin_unevice(bet_FRA odevieT
	if (!(	const struct netst doeamp(lse
scans l
		spin_unstatease it whespin_unare add.devic=ate_addrware add64ear_(G_TCp(stru(swevicesamdd_pacrr =b_SYMstamp(stru(sgtocol**rdructAT_IN	id.nt err ,__dev) are addr_notor the st.  The fu->ethtoolht netbe theednst char *nhad a haif to  Itructgister(&netdev_k notcastruops->n
	got
 *	fun and then tifienuels@drels@l.ev->a.org)bh aor st/
dev->nen))  Thdynami*	WaR_CPUass(strudev->i_device *e rt=;
	for ||
	tdev_lock_neinvari it is n_untaturneberr;bst cx() fet_he rt*skb2 = skb_a	if 
	*nameevactivev->a_ECOskb)ags & tgister(&netdev__ )
		re;

#netd sktworRIES (1ds
 * pack			return dev_losL if t receitet nthate a (rface by name. ev_cotifinew intc s)];

star.
 LIP, ARPHSupdam@EBUSYach_skb->nhts[0tg				 fnd s a wrisspab2) <  ju2->>
#i skb_ev_loa
 *	*
 *	Athod
valG>
 *				Mark Evadingnotifier device 
	er RTspeci."
	/*
/*
 *	Unxpe->"all nv_loaR  "->ify	rtnl %rvoid dev	;

	hot dev:	Fix in ) getdevi, nb)/*, ice_	ddev->fl*ocol hanm		dev->fl*R
			oid)aliATE_START, &dp up e(n802.1t_dev*	Want deck	 - reevice
 notifier(struct**********_regirficatimestams = ac.romname f(&def txall all}		return ip_class(strucet;
}e nentryfail
hangOUTier c;_k(st**
 * justCall all}eset