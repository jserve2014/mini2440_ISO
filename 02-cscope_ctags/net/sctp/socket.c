/* SCTP kernel implementation
 * (C) Copyright IBM Corp. 2001, 2004
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 * Copyright (c) 2001-2003 Intel Corp.
 * Copyright (c) 2001-2002 Nokia, Inc.
 * Copyright (c) 2001 La Monte H.P. Yarroll
 *
 * This file is part of the SCTP kernel implementation
 *
 * These functions interface with the sockets layer to implement the
 * SCTP Extensions for the Sockets API.
 *
 * Note that the descriptions from the specification are USER level
 * functions--this file is the functions which populate the struct proto
 * for SCTP which is the BOTTOM of the sockets interface.
 *
 * This SCTP implementation is free software;
 * you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This SCTP implementation is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *                 ************************
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Please send any bug reports or fixes you make to the
 * email address(es):
 *    lksctp developers <lksctp-developers@lists.sourceforge.net>
 *
 * Or submit a bug report through the following website:
 *    http://www.sf.net/projects/lksctp
 *
 * Written or modified by:
 *    La Monte H.P. Yarroll <piggy@acm.org>
 *    Narasimha Budihal     <narsi@refcode.org>
 *    Karl Knutson          <karl@athena.chicago.il.us>
 *    Jon Grimm             <jgrimm@us.ibm.com>
 *    Xingang Guo           <xingang.guo@intel.com>
 *    Daisy Chang           <daisyc@us.ibm.com>
 *    Sridhar Samudrala     <samudrala@us.ibm.com>
 *    Inaky Perez-Gonzalez  <inaky.gonzalez@intel.com>
 *    Ardelle Fan	    <ardelle.fan@intel.com>
 *    Ryan Layer	    <rmlayer@us.ibm.com>
 *    Anup Pemmaiah         <pemmaiah@cc.usu.edu>
 *    Kevin Gao             <kevin.gao@intel.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/ip.h>
#include <linux/capability.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/crypto.h>

#include <net/ip.h>
#include <net/icmp.h>
#include <net/route.h>
#include <net/ipv6.h>
#include <net/inet_common.h>

#include <linux/socket.h> /* for sa_family_t */
#include <net/sock.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>

/* WARNING:  Please do not remove the SCTP_STATIC attribute to
 * any of the functions below as they are used to export functions
 * used by a project regression testsuite.
 */

/* Forward declarations for internal helper functions. */
static int sctp_writeable(struct sock *sk);
static void sctp_wfree(struct sk_buff *skb);
static int sctp_wait_for_sndbuf(struct sctp_association *, long *timeo_p,
				size_t msg_len);
static int sctp_wait_for_packet(struct sock * sk, int *err, long *timeo_p);
static int sctp_wait_for_connect(struct sctp_association *, long *timeo_p);
static int sctp_wait_for_accept(struct sock *sk, long timeo);
static void sctp_wait_for_close(struct sock *sk, long timeo);
static struct sctp_af *sctp_sockaddr_af(struct sctp_sock *opt,
					union sctp_addr *addr, int len);
static int sctp_bindx_add(struct sock *, struct sockaddr *, int);
static int sctp_bindx_rem(struct sock *, struct sockaddr *, int);
static int sctp_send_asconf_add_ip(struct sock *, struct sockaddr *, int);
static int sctp_send_asconf_del_ip(struct sock *, struct sockaddr *, int);
static int sctp_send_asconf(struct sctp_association *asoc,
			    struct sctp_chunk *chunk);
static int sctp_do_bind(struct sock *, union sctp_addr *, int);
static int sctp_autobind(struct sock *sk);
static void sctp_sock_migrate(struct sock *, struct sock *,
			      struct sctp_association *, sctp_socket_type_t);
static char *sctp_hmac_alg = SCTP_COOKIE_HMAC_ALG;

extern struct kmem_cache *sctp_bucket_cachep;
extern int sysctl_sctp_mem[3];
extern int sysctl_sctp_rmem[3];
extern int sysctl_sctp_wmem[3];

static int sctp_memory_pressure;
static atomic_t sctp_memory_allocated;
struct percpu_counter sctp_sockets_allocated;

static void sctp_enter_memory_pressure(struct sock *sk)
{
	sctp_memory_pressure = 1;
}


/* Get the sndbuf space available at the time on the association.  */
static inline int sctp_wspace(struct sctp_association *asoc)
{
	int amt;

	if (asoc->ep->sndbuf_policy)
		amt = asoc->sndbuf_used;
	else
		amt = sk_wmem_alloc_get(asoc->base.sk);

	if (amt >= asoc->base.sk->sk_sndbuf) {
		if (asoc->base.sk->sk_userlocks & SOCK_SNDBUF_LOCK)
			amt = 0;
		else {
			amt = sk_stream_wspace(asoc->base.sk);
			if (amt < 0)
				amt = 0;
		}
	} else {
		amt = asoc->base.sk->sk_sndbuf - amt;
	}
	return amt;
}

/* Increment the used sndbuf space count of the corresponding association by
 * the size of the outgoing data chunk.
 * Also, set the skb destructor for sndbuf accounting later.
 *
 * Since it is always 1-1 between chunk and skb, and also a new skb is always
 * allocated for chunk bundling in sctp_packet_transmit(), we can use the
 * destructor in the data chunk skb for the purpose of the sndbuf space
 * tracking.
 */
static inline void sctp_set_owner_w(struct sctp_chunk *chunk)
{
	struct sctp_association *asoc = chunk->asoc;
	struct sock *sk = asoc->base.sk;

	/* The sndbuf space is tracked per association.  */
	sctp_association_hold(asoc);

	skb_set_owner_w(chunk->skb, sk);

	chunk->skb->destructor = sctp_wfree;
	/* Save the chunk pointer in skb for sctp_wfree to use later.  */
	*((struct sctp_chunk **)(chunk->skb->cb)) = chunk;

	asoc->sndbuf_used += SCTP_DATA_SNDSIZE(chunk) +
				sizeof(struct sk_buff) +
				sizeof(struct sctp_chunk);

	atomic_add(sizeof(struct sctp_chunk), &sk->sk_wmem_alloc);
	sk->sk_wmem_queued += chunk->skb->truesize;
	sk_mem_charge(sk, chunk->skb->truesize);
}

/* Verify that this is a valid address. */
static inline int sctp_verify_addr(struct sock *sk, union sctp_addr *addr,
				   int len)
{
	struct sctp_af *af;

	/* Verify basic sockaddr. */
	af = sctp_sockaddr_af(sctp_sk(sk), addr, len);
	if (!af)
		return -EINVAL;

	/* Is this a valid SCTP address?  */
	if (!af->addr_valid(addr, sctp_sk(sk), NULL))
		return -EINVAL;

	if (!sctp_sk(sk)->pf->send_verify(sctp_sk(sk), (addr)))
		return -EINVAL;

	return 0;
}

/* Look up the association by its id.  If this is not a UDP-style
 * socket, the ID field is always ignored.
 */
struct sctp_association *sctp_id2assoc(struct sock *sk, sctp_assoc_t id)
{
	struct sctp_association *asoc = NULL;

	/* If this is not a UDP-style socket, assoc id should be ignored. */
	if (!sctp_style(sk, UDP)) {
		/* Return NULL if the socket state is not ESTABLISHED. It
		 * could be a TCP-style listening socket or a socket which
		 * hasn't yet called connect() to establish an association.
		 */
		if (!sctp_sstate(sk, ESTABLISHED))
			return NULL;

		/* Get the first and the only association from the list. */
		if (!list_empty(&sctp_sk(sk)->ep->asocs))
			asoc = list_entry(sctp_sk(sk)->ep->asocs.next,
					  struct sctp_association, asocs);
		return asoc;
	}

	/* Otherwise this is a UDP-style socket. */
	if (!id || (id == (sctp_assoc_t)-1))
		return NULL;

	spin_lock_bh(&sctp_assocs_id_lock);
	asoc = (struct sctp_association *)idr_find(&sctp_assocs_id, (int)id);
	spin_unlock_bh(&sctp_assocs_id_lock);

	if (!asoc || (asoc->base.sk != sk) || asoc->base.dead)
		return NULL;

	return asoc;
}

/* Look up the transport from an address and an assoc id. If both address and
 * id are specified, the associations matching the address and the id should be
 * the same.
 */
static struct sctp_transport *sctp_addr_id2transport(struct sock *sk,
					      struct sockaddr_storage *addr,
					      sctp_assoc_t id)
{
	struct sctp_association *addr_asoc = NULL, *id_asoc = NULL;
	struct sctp_transport *transport;
	union sctp_addr *laddr = (union sctp_addr *)addr;

	addr_asoc = sctp_endpoint_lookup_assoc(sctp_sk(sk)->ep,
					       laddr,
					       &transport);

	if (!addr_asoc)
		return NULL;

	id_asoc = sctp_id2assoc(sk, id);
	if (id_asoc && (id_asoc != addr_asoc))
		return NULL;

	sctp_get_pf_specific(sk->sk_family)->addr_v4map(sctp_sk(sk),
						(union sctp_addr *)addr);

	return transport;
}

/* API 3.1.2 bind() - UDP Style Syntax
 * The syntax of bind() is,
 *
 *   ret = bind(int sd, struct sockaddr *addr, int addrlen);
 *
 *   sd      - the socket descriptor returned by socket().
 *   addr    - the address structure (struct sockaddr_in or struct
 *             sockaddr_in6 [RFC 2553]),
 *   addr_len - the size of the address structure.
 */
SCTP_STATIC int sctp_bind(struct sock *sk, struct sockaddr *addr, int addr_len)
{
	int retval = 0;

	sctp_lock_sock(sk);

	SCTP_DEBUG_PRINTK("sctp_bind(sk: %p, addr: %p, addr_len: %d)\n",
			  sk, addr, addr_len);

	/* Disallow binding twice. */
	if (!sctp_sk(sk)->ep->base.bind_addr.port)
		retval = sctp_do_bind(sk, (union sctp_addr *)addr,
				      addr_len);
	else
		retval = -EINVAL;

	sctp_release_sock(sk);

	return retval;
}

static long sctp_get_port_local(struct sock *, union sctp_addr *);

/* Verify this is a valid sockaddr. */
static struct sctp_af *sctp_sockaddr_af(struct sctp_sock *opt,
					union sctp_addr *addr, int len)
{
	struct sctp_af *af;

	/* Check minimum size.  */
	if (len < sizeof (struct sockaddr))
		return NULL;

	/* V4 mapped address are really of AF_INET family */
	if (addr->sa.sa_family == AF_INET6 &&
	    ipv6_addr_v4mapped(&addr->v6.sin6_addr)) {
		if (!opt->pf->af_supported(AF_INET, opt))
			return NULL;
	} else {
		/* Does this PF support this AF? */
		if (!opt->pf->af_supported(addr->sa.sa_family, opt))
			return NULL;
	}

	/* If we get this far, af is valid. */
	af = sctp_get_af_specific(addr->sa.sa_family);

	if (len < af->sockaddr_len)
		return NULL;

	return af;
}

/* Bind a local address either to an endpoint or to an association.  */
SCTP_STATIC int sctp_do_bind(struct sock *sk, union sctp_addr *addr, int len)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_endpoint *ep = sp->ep;
	struct sctp_bind_addr *bp = &ep->base.bind_addr;
	struct sctp_af *af;
	unsigned short snum;
	int ret = 0;

	/* Common sockaddr verification. */
	af = sctp_sockaddr_af(sp, addr, len);
	if (!af) {
		SCTP_DEBUG_PRINTK("sctp_do_bind(sk: %p, newaddr: %p, len: %d) EINVAL\n",
				  sk, addr, len);
		return -EINVAL;
	}

	snum = ntohs(addr->v4.sin_port);

	SCTP_DEBUG_PRINTK_IPADDR("sctp_do_bind(sk: %p, new addr: ",
				 ", port: %d, new port: %d, len: %d)\n",
				 sk,
				 addr,
				 bp->port, snum,
				 len);

	/* PF specific bind() address verification. */
	if (!sp->pf->bind_verify(sp, addr))
		return -EADDRNOTAVAIL;

	/* We must either be unbound, or bind to the same port.
	 * It's OK to allow 0 ports if we are already bound.
	 * We'll just inhert an already bound port in this case
	 */
	if (bp->port) {
		if (!snum)
			snum = bp->port;
		else if (snum != bp->port) {
			SCTP_DEBUG_PRINTK("sctp_do_bind:"
				  " New port %d does not match existing port "
				  "%d.\n", snum, bp->port);
			return -EINVAL;
		}
	}

	if (snum && snum < PROT_SOCK && !capable(CAP_NET_BIND_SERVICE))
		return -EACCES;

	/* See if the address matches any of the addresses we may have
	 * already bound before checking against other endpoints.
	 */
	if (sctp_bind_addr_match(bp, addr, sp))
		return -EINVAL;

	/* Make sure we are allowed to bind here.
	 * The function sctp_get_port_local() does duplicate address
	 * detection.
	 */
	addr->v4.sin_port = htons(snum);
	if ((ret = sctp_get_port_local(sk, addr))) {
		return -EADDRINUSE;
	}

	/* Refresh ephemeral port.  */
	if (!bp->port)
		bp->port = inet_sk(sk)->num;

	/* Add the address to the bind address list.
	 * Use GFP_ATOMIC since BHs will be disabled.
	 */
	ret = sctp_add_bind_addr(bp, addr, SCTP_ADDR_SRC, GFP_ATOMIC);

	/* Copy back into socket for getsockname() use. */
	if (!ret) {
		inet_sk(sk)->sport = htons(inet_sk(sk)->num);
		af->to_sk_saddr(addr, sk);
	}

	return ret;
}

 /* ADDIP Section 4.1.1 Congestion Control of ASCONF Chunks
 *
 * R1) One and only one ASCONF Chunk MAY be in transit and unacknowledged
 * at any one time.  If a sender, after sending an ASCONF chunk, decides
 * it needs to transfer another ASCONF Chunk, it MUST wait until the
 * ASCONF-ACK Chunk returns from the previous ASCONF Chunk before sending a
 * subsequent ASCONF. Note this restriction binds each side, so at any
 * time two ASCONF may be in-transit on any given association (one sent
 * from each endpoint).
 */
static int sctp_send_asconf(struct sctp_association *asoc,
			    struct sctp_chunk *chunk)
{
	int		retval = 0;

	/* If there is an outstanding ASCONF chunk, queue it for later
	 * transmission.
	 */
	if (asoc->addip_last_asconf) {
		list_add_tail(&chunk->list, &asoc->addip_chunk_list);
		goto out;
	}

	/* Hold the chunk until an ASCONF_ACK is received. */
	sctp_chunk_hold(chunk);
	retval = sctp_primitive_ASCONF(asoc, chunk);
	if (retval)
		sctp_chunk_free(chunk);
	else
		asoc->addip_last_asconf = chunk;

out:
	return retval;
}

/* Add a list of addresses as bind addresses to local endpoint or
 * association.
 *
 * Basically run through each address specified in the addrs/addrcnt
 * array/length pair, determine if it is IPv6 or IPv4 and call
 * sctp_do_bind() on it.
 *
 * If any of them fails, then the operation will be reversed and the
 * ones that were added will be removed.
 *
 * Only sctp_setsockopt_bindx() is supposed to call this function.
 */
static int sctp_bindx_add(struct sock *sk, struct sockaddr *addrs, int addrcnt)
{
	int cnt;
	int retval = 0;
	void *addr_buf;
	struct sockaddr *sa_addr;
	struct sctp_af *af;

	SCTP_DEBUG_PRINTK("sctp_bindx_add (sk: %p, addrs: %p, addrcnt: %d)\n",
			  sk, addrs, addrcnt);

	addr_buf = addrs;
	for (cnt = 0; cnt < addrcnt; cnt++) {
		/* The list may contain either IPv4 or IPv6 address;
		 * determine the address length for walking thru the list.
		 */
		sa_addr = (struct sockaddr *)addr_buf;
		af = sctp_get_af_specific(sa_addr->sa_family);
		if (!af) {
			retval = -EINVAL;
			goto err_bindx_add;
		}

		retval = sctp_do_bind(sk, (union sctp_addr *)sa_addr,
				      af->sockaddr_len);

		addr_buf += af->sockaddr_len;

err_bindx_add:
		if (retval < 0) {
			/* Failed. Cleanup the ones that have been added */
			if (cnt > 0)
				sctp_bindx_rem(sk, addrs, cnt);
			return retval;
		}
	}

	return retval;
}

/* Send an ASCONF chunk with Add IP address parameters to all the peers of the
 * associations that are part of the endpoint indicating that a list of local
 * addresses are added to the endpoint.
 *
 * If any of the addresses is already in the bind address list of the
 * association, we do not send the chunk for that association.  But it will not
 * affect other associations.
 *
 * Only sctp_setsockopt_bindx() is supposed to call this function.
 */
static int sctp_send_asconf_add_ip(struct sock		*sk,
				   struct sockaddr	*addrs,
				   int 			addrcnt)
{
	struct sctp_sock		*sp;
	struct sctp_endpoint		*ep;
	struct sctp_association		*asoc;
	struct sctp_bind_addr		*bp;
	struct sctp_chunk		*chunk;
	struct sctp_sockaddr_entry	*laddr;
	union sctp_addr			*addr;
	union sctp_addr			saveaddr;
	void				*addr_buf;
	struct sctp_af			*af;
	struct list_head		*p;
	int 				i;
	int 				retval = 0;

	if (!sctp_addip_enable)
		return retval;

	sp = sctp_sk(sk);
	ep = sp->ep;

	SCTP_DEBUG_PRINTK("%s: (sk: %p, addrs: %p, addrcnt: %d)\n",
			  __func__, sk, addrs, addrcnt);

	list_for_each_entry(asoc, &ep->asocs, asocs) {

		if (!asoc->peer.asconf_capable)
			continue;

		if (asoc->peer.addip_disabled_mask & SCTP_PARAM_ADD_IP)
			continue;

		if (!sctp_state(asoc, ESTABLISHED))
			continue;

		/* Check if any address in the packed array of addresses is
		 * in the bind address list of the association. If so,
		 * do not send the asconf chunk to its peer, but continue with
		 * other associations.
		 */
		addr_buf = addrs;
		for (i = 0; i < addrcnt; i++) {
			addr = (union sctp_addr *)addr_buf;
			af = sctp_get_af_specific(addr->v4.sin_family);
			if (!af) {
				retval = -EINVAL;
				goto out;
			}

			if (sctp_assoc_lookup_laddr(asoc, addr))
				break;

			addr_buf += af->sockaddr_len;
		}
		if (i < addrcnt)
			continue;

		/* Use the first valid address in bind addr list of
		 * association as Address Parameter of ASCONF CHUNK.
		 */
		bp = &asoc->base.bind_addr;
		p = bp->address_list.next;
		laddr = list_entry(p, struct sctp_sockaddr_entry, list);
		chunk = sctp_make_asconf_update_ip(asoc, &laddr->a, addrs,
						   addrcnt, SCTP_PARAM_ADD_IP);
		if (!chunk) {
			retval = -ENOMEM;
			goto out;
		}

		retval = sctp_send_asconf(asoc, chunk);
		if (retval)
			goto out;

		/* Add the new addresses to the bind address list with
		 * use_as_src set to 0.
		 */
		addr_buf = addrs;
		for (i = 0; i < addrcnt; i++) {
			addr = (union sctp_addr *)addr_buf;
			af = sctp_get_af_specific(addr->v4.sin_family);
			memcpy(&saveaddr, addr, af->sockaddr_len);
			retval = sctp_add_bind_addr(bp, &saveaddr,
						    SCTP_ADDR_NEW, GFP_ATOMIC);
			addr_buf += af->sockaddr_len;
		}
	}

out:
	return retval;
}

/* Remove a list of addresses from bind addresses list.  Do not remove the
 * last address.
 *
 * Basically run through each address specified in the addrs/addrcnt
 * array/length pair, determine if it is IPv6 or IPv4 and call
 * sctp_del_bind() on it.
 *
 * If any of them fails, then the operation will be reversed and the
 * ones that were removed will be added back.
 *
 * At least one address has to be left; if only one address is
 * available, the operation will return -EBUSY.
 *
 * Only sctp_setsockopt_bindx() is supposed to call this function.
 */
static int sctp_bindx_rem(struct sock *sk, struct sockaddr *addrs, int addrcnt)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_endpoint *ep = sp->ep;
	int cnt;
	struct sctp_bind_addr *bp = &ep->base.bind_addr;
	int retval = 0;
	void *addr_buf;
	union sctp_addr *sa_addr;
	struct sctp_af *af;

	SCTP_DEBUG_PRINTK("sctp_bindx_rem (sk: %p, addrs: %p, addrcnt: %d)\n",
			  sk, addrs, addrcnt);

	addr_buf = addrs;
	for (cnt = 0; cnt < addrcnt; cnt++) {
		/* If the bind address list is empty or if there is only one
		 * bind address, there is nothing more to be removed (we need
		 * at least one address here).
		 */
		if (list_empty(&bp->address_list) ||
		    (sctp_list_single_entry(&bp->address_list))) {
			retval = -EBUSY;
			goto err_bindx_rem;
		}

		sa_addr = (union sctp_addr *)addr_buf;
		af = sctp_get_af_specific(sa_addr->sa.sa_family);
		if (!af) {
			retval = -EINVAL;
			goto err_bindx_rem;
		}

		if (!af->addr_valid(sa_addr, sp, NULL)) {
			retval = -EADDRNOTAVAIL;
			goto err_bindx_rem;
		}

		if (sa_addr->v4.sin_port != htons(bp->port)) {
			retval = -EINVAL;
			goto err_bindx_rem;
		}

		/* FIXME - There is probably a need to check if sk->sk_saddr and
		 * sk->sk_rcv_addr are currently set to one of the addresses to
		 * be removed. This is something which needs to be looked into
		 * when we are fixing the outstanding issues with multi-homing
		 * socket routing and failover schemes. Refer to comments in
		 * sctp_do_bind(). -daisy
		 */
		retval = sctp_del_bind_addr(bp, sa_addr);

		addr_buf += af->sockaddr_len;
err_bindx_rem:
		if (retval < 0) {
			/* Failed. Add the ones that has been removed back */
			if (cnt > 0)
				sctp_bindx_add(sk, addrs, cnt);
			return retval;
		}
	}

	return retval;
}

/* Send an ASCONF chunk with Delete IP address parameters to all the peers of
 * the associations that are part of the endpoint indicating that a list of
 * local addresses are removed from the endpoint.
 *
 * If any of the addresses is already in the bind address list of the
 * association, we do not send the chunk for that association.  But it will not
 * affect other associations.
 *
 * Only sctp_setsockopt_bindx() is supposed to call this function.
 */
static int sctp_send_asconf_del_ip(struct sock		*sk,
				   struct sockaddr	*addrs,
				   int			addrcnt)
{
	struct sctp_sock	*sp;
	struct sctp_endpoint	*ep;
	struct sctp_association	*asoc;
	struct sctp_transport	*transport;
	struct sctp_bind_addr	*bp;
	struct sctp_chunk	*chunk;
	union sctp_addr		*laddr;
	void			*addr_buf;
	struct sctp_af		*af;
	struct sctp_sockaddr_entry *saddr;
	int 			i;
	int 			retval = 0;

	if (!sctp_addip_enable)
		return retval;

	sp = sctp_sk(sk);
	ep = sp->ep;

	SCTP_DEBUG_PRINTK("%s: (sk: %p, addrs: %p, addrcnt: %d)\n",
			  __func__, sk, addrs, addrcnt);

	list_for_each_entry(asoc, &ep->asocs, asocs) {

		if (!asoc->peer.asconf_capable)
			continue;

		if (asoc->peer.addip_disabled_mask & SCTP_PARAM_DEL_IP)
			continue;

		if (!sctp_state(asoc, ESTABLISHED))
			continue;

		/* Check if any address in the packed array of addresses is
		 * not present in the bind address list of the association.
		 * If so, do not send the asconf chunk to its peer, but
		 * continue with other associations.
		 */
		addr_buf = addrs;
		for (i = 0; i < addrcnt; i++) {
			laddr = (union sctp_addr *)addr_buf;
			af = sctp_get_af_specific(laddr->v4.sin_family);
			if (!af) {
				retval = -EINVAL;
				goto out;
			}

			if (!sctp_assoc_lookup_laddr(asoc, laddr))
				break;

			addr_buf += af->sockaddr_len;
		}
		if (i < addrcnt)
			continue;

		/* Find one address in the association's bind address list
		 * that is not in the packed array of addresses. This is to
		 * make sure that we do not delete all the addresses in the
		 * association.
		 */
		bp = &asoc->base.bind_addr;
		laddr = sctp_find_unmatch_addr(bp, (union sctp_addr *)addrs,
					       addrcnt, sp);
		if (!laddr)
			continue;

		/* We do not need RCU protection throughout this loop
		 * because this is done under a socket lock from the
		 * setsockopt call.
		 */
		chunk = sctp_make_asconf_update_ip(asoc, laddr, addrs, addrcnt,
						   SCTP_PARAM_DEL_IP);
		if (!chunk) {
			retval = -ENOMEM;
			goto out;
		}

		/* Reset use_as_src flag for the addresses in the bind address
		 * list that are to be deleted.
		 */
		addr_buf = addrs;
		for (i = 0; i < addrcnt; i++) {
			laddr = (union sctp_addr *)addr_buf;
			af = sctp_get_af_specific(laddr->v4.sin_family);
			list_for_each_entry(saddr, &bp->address_list, list) {
				if (sctp_cmp_addr_exact(&saddr->a, laddr))
					saddr->state = SCTP_ADDR_DEL;
			}
			addr_buf += af->sockaddr_len;
		}

		/* Update the route and saddr entries for all the transports
		 * as some of the addresses in the bind address list are
		 * about to be deleted and cannot be used as source addresses.
		 */
		list_for_each_entry(transport, &asoc->peer.transport_addr_list,
					transports) {
			dst_release(transport->dst);
			sctp_transport_route(transport, NULL,
					     sctp_sk(asoc->base.sk));
		}

		retval = sctp_send_asconf(asoc, chunk);
	}
out:
	return retval;
}

/* Helper for tunneling sctp_bindx() requests through sctp_setsockopt()
 *
 * API 8.1
 * int sctp_bindx(int sd, struct sockaddr *addrs, int addrcnt,
 *                int flags);
 *
 * If sd is an IPv4 socket, the addresses passed must be IPv4 addresses.
 * If the sd is an IPv6 socket, the addresses passed can either be IPv4
 * or IPv6 addresses.
 *
 * A single address may be specified as INADDR_ANY or IN6ADDR_ANY, see
 * Section 3.1.2 for this usage.
 *
 * addrs is a pointer to an array of one or more socket addresses. Each
 * address is contained in its appropriate structure (i.e. struct
 * sockaddr_in or struct sockaddr_in6) the family of the address type
 * must be used to distinguish the address length (note that this
 * representation is termed a "packed array" of addresses). The caller
 * specifies the number of addresses in the array with addrcnt.
 *
 * On success, sctp_bindx() returns 0. On failure, sctp_bindx() returns
 * -1, and sets errno to the appropriate error code.
 *
 * For SCTP, the port given in each socket address must be the same, or
 * sctp_bindx() will fail, setting errno to EINVAL.
 *
 * The flags parameter is formed from the bitwise OR of zero or more of
 * the following currently defined flags:
 *
 * SCTP_BINDX_ADD_ADDR
 *
 * SCTP_BINDX_REM_ADDR
 *
 * SCTP_BINDX_ADD_ADDR directs SCTP to add the given addresses to the
 * association, and SCTP_BINDX_REM_ADDR directs SCTP to remove the given
 * addresses from the association. The two flags are mutually exclusive;
 * if both are given, sctp_bindx() will fail with EINVAL. A caller may
 * not remove all addresses from an association; sctp_bindx() will
 * reject such an attempt with EINVAL.
 *
 * An application can use sctp_bindx(SCTP_BINDX_ADD_ADDR) to associate
 * additional addresses with an endpoint after calling bind().  Or use
 * sctp_bindx(SCTP_BINDX_REM_ADDR) to remove some addresses a listening
 * socket is associated with so that no new association accepted will be
 * associated with those addresses. If the endpoint supports dynamic
 * address a SCTP_BINDX_REM_ADDR or SCTP_BINDX_ADD_ADDR may cause a
 * endpoint to send the appropriate message to the peer to change the
 * peers address lists.
 *
 * Adding and removing addresses from a connected association is
 * optional functionality. Implementations that do not support this
 * functionality should return EOPNOTSUPP.
 *
 * Basically do nothing but copying the addresses from user to kernel
 * land and invoking either sctp_bindx_add() or sctp_bindx_rem() on the sk.
 * This is used for tunneling the sctp_bindx() request through sctp_setsockopt()
 * from userspace.
 *
 * We don't use copy_from_user() for optimization: we first do the
 * sanity checks (buffer size -fast- and access check-healthy
 * pointer); if all of those succeed, then we can alloc the memory
 * (expensive operation) needed to copy the data to kernel. Then we do
 * the copying without checking the user space area
 * (__copy_from_user()).
 *
 * On exit there is no need to do sockfd_put(), sys_setsockopt() does
 * it.
 *
 * sk        The sk of the socket
 * addrs     The pointer to the addresses in user land
 * addrssize Size of the addrs buffer
 * op        Operation to perform (add or remove, see the flags of
 *           sctp_bindx)
 *
 * Returns 0 if ok, <0 errno code on error.
 */
SCTP_STATIC int sctp_setsockopt_bindx(struct sock* sk,
				      struct sockaddr __user *addrs,
				      int addrs_size, int op)
{
	struct sockaddr *kaddrs;
	int err;
	int addrcnt = 0;
	int walk_size = 0;
	struct sockaddr *sa_addr;
	void *addr_buf;
	struct sctp_af *af;

	SCTP_DEBUG_PRINTK("sctp_setsocktopt_bindx: sk %p addrs %p"
			  " addrs_size %d opt %d\n", sk, addrs, addrs_size, op);

	if (unlikely(addrs_size <= 0))
		return -EINVAL;

	/* Check the user passed a healthy pointer.  */
	if (unlikely(!access_ok(VERIFY_READ, addrs, addrs_size)))
		return -EFAULT;

	/* Alloc space for the address array in kernel memory.  */
	kaddrs = kmalloc(addrs_size, GFP_KERNEL);
	if (unlikely(!kaddrs))
		return -ENOMEM;

	if (__copy_from_user(kaddrs, addrs, addrs_size)) {
		kfree(kaddrs);
		return -EFAULT;
	}

	/* Walk through the addrs buffer and count the number of addresses. */
	addr_buf = kaddrs;
	while (walk_size < addrs_size) {
		sa_addr = (struct sockaddr *)addr_buf;
		af = sctp_get_af_specific(sa_addr->sa_family);

		/* If the address family is not supported or if this address
		 * causes the address buffer to overflow return EINVAL.
		 */
		if (!af || (walk_size + af->sockaddr_len) > addrs_size) {
			kfree(kaddrs);
			return -EINVAL;
		}
		addrcnt++;
		addr_buf += af->sockaddr_len;
		walk_size += af->sockaddr_len;
	}

	/* Do the work. */
	switch (op) {
	case SCTP_BINDX_ADD_ADDR:
		err = sctp_bindx_add(sk, kaddrs, addrcnt);
		if (err)
			goto out;
		err = sctp_send_asconf_add_ip(sk, kaddrs, addrcnt);
		break;

	case SCTP_BINDX_REM_ADDR:
		err = sctp_bindx_rem(sk, kaddrs, addrcnt);
		if (err)
			goto out;
		err = sctp_send_asconf_del_ip(sk, kaddrs, addrcnt);
		break;

	default:
		err = -EINVAL;
		break;
	}

out:
	kfree(kaddrs);

	return err;
}

/* __sctp_connect(struct sock* sk, struct sockaddr *kaddrs, int addrs_size)
 *
 * Common routine for handling connect() and sctp_connectx().
 * Connect will come in with just a single address.
 */
static int __sctp_connect(struct sock* sk,
			  struct sockaddr *kaddrs,
			  int addrs_size,
			  sctp_assoc_t *assoc_id)
{
	struct sctp_sock *sp;
	struct sctp_endpoint *ep;
	struct sctp_association *asoc = NULL;
	struct sctp_association *asoc2;
	struct sctp_transport *transport;
	union sctp_addr to;
	struct sctp_af *af;
	sctp_scope_t scope;
	long timeo;
	int err = 0;
	int addrcnt = 0;
	int walk_size = 0;
	union sctp_addr *sa_addr = NULL;
	void *addr_buf;
	unsigned short port;
	unsigned int f_flags = 0;

	sp = sctp_sk(sk);
	ep = sp->ep;

	/* connect() cannot be done on a socket that is already in ESTABLISHED
	 * state - UDP-style peeled off socket or a TCP-style socket that
	 * is already connected.
	 * It cannot be done even on a TCP-style listening socket.
	 */
	if (sctp_sstate(sk, ESTABLISHED) ||
	    (sctp_style(sk, TCP) && sctp_sstate(sk, LISTENING))) {
		err = -EISCONN;
		goto out_free;
	}

	/* Walk through the addrs buffer and count the number of addresses. */
	addr_buf = kaddrs;
	while (walk_size < addrs_size) {
		sa_addr = (union sctp_addr *)addr_buf;
		af = sctp_get_af_specific(sa_addr->sa.sa_family);
		port = ntohs(sa_addr->v4.sin_port);

		/* If the address family is not supported or if this address
		 * causes the address buffer to overflow return EINVAL.
		 */
		if (!af || (walk_size + af->sockaddr_len) > addrs_size) {
			err = -EINVAL;
			goto out_free;
		}

		/* Save current address so we can work with it */
		memcpy(&to, sa_addr, af->sockaddr_len);

		err = sctp_verify_addr(sk, &to, af->sockaddr_len);
		if (err)
			goto out_free;

		/* Make sure the destination port is correctly set
		 * in all addresses.
		 */
		if (asoc && asoc->peer.port && asoc->peer.port != port)
			goto out_free;


		/* Check if there already is a matching association on the
		 * endpoint (other than the one created here).
		 */
		asoc2 = sctp_endpoint_lookup_assoc(ep, &to, &transport);
		if (asoc2 && asoc2 != asoc) {
			if (asoc2->state >= SCTP_STATE_ESTABLISHED)
				err = -EISCONN;
			else
				err = -EALREADY;
			goto out_free;
		}

		/* If we could not find a matching association on the endpoint,
		 * make sure that there is no peeled-off association matching
		 * the peer address even on another socket.
		 */
		if (sctp_endpoint_is_peeled_off(ep, &to)) {
			err = -EADDRNOTAVAIL;
			goto out_free;
		}

		if (!asoc) {
			/* If a bind() or sctp_bindx() is not called prior to
			 * an sctp_connectx() call, the system picks an
			 * ephemeral port and will choose an address set
			 * equivalent to binding with a wildcard address.
			 */
			if (!ep->base.bind_addr.port) {
				if (sctp_autobind(sk)) {
					err = -EAGAIN;
					goto out_free;
				}
			} else {
				/*
				 * If an unprivileged user inherits a 1-many
				 * style socket with open associations on a
				 * privileged port, it MAY be permitted to
				 * accept new associations, but it SHOULD NOT
				 * be permitted to open new associations.
				 */
				if (ep->base.bind_addr.port < PROT_SOCK &&
				    !capable(CAP_NET_BIND_SERVICE)) {
					err = -EACCES;
					goto out_free;
				}
			}

			scope = sctp_scope(&to);
			asoc = sctp_association_new(ep, sk, scope, GFP_KERNEL);
			if (!asoc) {
				err = -ENOMEM;
				goto out_free;
			}

			err = sctp_assoc_set_bind_addr_from_ep(asoc, scope,
							      GFP_KERNEL);
			if (err < 0) {
				goto out_free;
			}

		}

		/* Prime the peer's transport structures.  */
		transport = sctp_assoc_add_peer(asoc, &to, GFP_KERNEL,
						SCTP_UNKNOWN);
		if (!transport) {
			err = -ENOMEM;
			goto out_free;
		}

		addrcnt++;
		addr_buf += af->sockaddr_len;
		walk_size += af->sockaddr_len;
	}

	/* In case the user of sctp_connectx() wants an association
	 * id back, assign one now.
	 */
	if (assoc_id) {
		err = sctp_assoc_set_id(asoc, GFP_KERNEL);
		if (err < 0)
			goto out_free;
	}

	err = sctp_primitive_ASSOCIATE(asoc, NULL);
	if (err < 0) {
		goto out_free;
	}

	/* Initialize sk's dport and daddr for getpeername() */
	inet_sk(sk)->dport = htons(asoc->peer.port);
	af = sctp_get_af_specific(sa_addr->sa.sa_family);
	af->to_sk_daddr(sa_addr, sk);
	sk->sk_err = 0;

	/* in-kernel sockets don't generally have a file allocated to them
	 * if all they do is call sock_create_kern().
	 */
	if (sk->sk_socket->file)
		f_flags = sk->sk_socket->file->f_flags;

	timeo = sock_sndtimeo(sk, f_flags & O_NONBLOCK);

	err = sctp_wait_for_connect(asoc, &timeo);
	if ((err == 0 || err == -EINPROGRESS) && assoc_id)
		*assoc_id = asoc->assoc_id;

	/* Don't free association on exit. */
	asoc = NULL;

out_free:

	SCTP_DEBUG_PRINTK("About to exit __sctp_connect() free asoc: %p"
			  " kaddrs: %p err: %d\n",
			  asoc, kaddrs, err);
	if (asoc)
		sctp_association_free(asoc);
	return err;
}

/* Helper for tunneling sctp_connectx() requests through sctp_setsockopt()
 *
 * API 8.9
 * int sctp_connectx(int sd, struct sockaddr *addrs, int addrcnt,
 * 			sctp_assoc_t *asoc);
 *
 * If sd is an IPv4 socket, the addresses passed must be IPv4 addresses.
 * If the sd is an IPv6 socket, the addresses passed can either be IPv4
 * or IPv6 addresses.
 *
 * A single address may be specified as INADDR_ANY or IN6ADDR_ANY, see
 * Section 3.1.2 for this usage.
 *
 * addrs is a pointer to an array of one or more socket addresses. Each
 * address is contained in its appropriate structure (i.e. struct
 * sockaddr_in or struct sockaddr_in6) the family of the address type
 * must be used to distengish the address length (note that this
 * representation is termed a "packed array" of addresses). The caller
 * specifies the number of addresses in the array with addrcnt.
 *
 * On success, sctp_connectx() returns 0. It also sets the assoc_id to
 * the association id of the new association.  On failure, sctp_connectx()
 * returns -1, and sets errno to the appropriate error code.  The assoc_id
 * is not touched by the kernel.
 *
 * For SCTP, the port given in each socket address must be the same, or
 * sctp_connectx() will fail, setting errno to EINVAL.
 *
 * An application can use sctp_connectx to initiate an association with
 * an endpoint that is multi-homed.  Much like sctp_bindx() this call
 * allows a caller to specify multiple addresses at which a peer can be
 * reached.  The way the SCTP stack uses the list of addresses to set up
 * the association is implementation dependant.  This function only
 * specifies that the stack will try to make use of all the addresses in
 * the list when needed.
 *
 * Note that the list of addresses passed in is only used for setting up
 * the association.  It does not necessarily equal the set of addresses
 * the peer uses for the resulting association.  If the caller wants to
 * find out the set of peer addresses, it must use sctp_getpaddrs() to
 * retrieve them after the association has been set up.
 *
 * Basically do nothing but copying the addresses from user to kernel
 * land and invoking either sctp_connectx(). This is used for tunneling
 * the sctp_connectx() request through sctp_setsockopt() from userspace.
 *
 * We don't use copy_from_user() for optimization: we first do the
 * sanity checks (buffer size -fast- and access check-healthy
 * pointer); if all of those succeed, then we can alloc the memory
 * (expensive operation) needed to copy the data to kernel. Then we do
 * the copying without checking the user space area
 * (__copy_from_user()).
 *
 * On exit there is no need to do sockfd_put(), sys_setsockopt() does
 * it.
 *
 * sk        The sk of the socket
 * addrs     The pointer to the addresses in user land
 * addrssize Size of the addrs buffer
 *
 * Returns >=0 if ok, <0 errno code on error.
 */
SCTP_STATIC int __sctp_setsockopt_connectx(struct sock* sk,
				      struct sockaddr __user *addrs,
				      int addrs_size,
				      sctp_assoc_t *assoc_id)
{
	int err = 0;
	struct sockaddr *kaddrs;

	SCTP_DEBUG_PRINTK("%s - sk %p addrs %p addrs_size %d\n",
			  __func__, sk, addrs, addrs_size);

	if (unlikely(addrs_size <= 0))
		return -EINVAL;

	/* Check the user passed a healthy pointer.  */
	if (unlikely(!access_ok(VERIFY_READ, addrs, addrs_size)))
		return -EFAULT;

	/* Alloc space for the address array in kernel memory.  */
	kaddrs = kmalloc(addrs_size, GFP_KERNEL);
	if (unlikely(!kaddrs))
		return -ENOMEM;

	if (__copy_from_user(kaddrs, addrs, addrs_size)) {
		err = -EFAULT;
	} else {
		err = __sctp_connect(sk, kaddrs, addrs_size, assoc_id);
	}

	kfree(kaddrs);

	return err;
}

/*
 * This is an older interface.  It's kept for backward compatibility
 * to the option that doesn't provide association id.
 */
SCTP_STATIC int sctp_setsockopt_connectx_old(struct sock* sk,
				      struct sockaddr __user *addrs,
				      int addrs_size)
{
	return __sctp_setsockopt_connectx(sk, addrs, addrs_size, NULL);
}

/*
 * New interface for the API.  The since the API is done with a socket
 * option, to make it simple we feed back the association id is as a return
 * indication to the call.  Error is always negative and association id is
 * always positive.
 */
SCTP_STATIC int sctp_setsockopt_connectx(struct sock* sk,
				      struct sockaddr __user *addrs,
				      int addrs_size)
{
	sctp_assoc_t assoc_id = 0;
	int err = 0;

	err = __sctp_setsockopt_connectx(sk, addrs, addrs_size, &assoc_id);

	if (err)
		return err;
	else
		return assoc_id;
}

/*
 * New (hopefully final) interface for the API.
 * We use the sctp_getaddrs_old structure so that use-space library
 * can avoid any unnecessary allocations.   The only defferent part
 * is that we store the actual length of the address buffer into the
 * addrs_num structure member.  That way we can re-use the existing
 * code.
 */
SCTP_STATIC int sctp_getsockopt_connectx3(struct sock* sk, int len,
					char __user *optval,
					int __user *optlen)
{
	struct sctp_getaddrs_old param;
	sctp_assoc_t assoc_id = 0;
	int err = 0;

	if (len < sizeof(param))
		return -EINVAL;

	if (copy_from_user(&param, optval, sizeof(param)))
		return -EFAULT;

	err = __sctp_setsockopt_connectx(sk,
			(struct sockaddr __user *)param.addrs,
			param.addr_num, &assoc_id);

	if (err == 0 || err == -EINPROGRESS) {
		if (copy_to_user(optval, &assoc_id, sizeof(assoc_id)))
			return -EFAULT;
		if (put_user(sizeof(assoc_id), optlen))
			return -EFAULT;
	}

	return err;
}

/* API 3.1.4 close() - UDP Style Syntax
 * Applications use close() to perform graceful shutdown (as described in
 * Section 10.1 of [SCTP]) on ALL the associations currently represented
 * by a UDP-style socket.
 *
 * The syntax is
 *
 *   ret = close(int sd);
 *
 *   sd      - the socket descriptor of the associations to be closed.
 *
 * To gracefully shutdown a specific association represented by the
 * UDP-style socket, an application should use the sendmsg() call,
 * passing no user data, but including the appropriate flag in the
 * ancillary data (see Section xxxx).
 *
 * If sd in the close() call is a branched-off socket representing only
 * one association, the shutdown is performed on that association only.
 *
 * 4.1.6 close() - TCP Style Syntax
 *
 * Applications use close() to gracefully close down an association.
 *
 * The syntax is:
 *
 *    int close(int sd);
 *
 *      sd      - the socket descriptor of the association to be closed.
 *
 * After an application calls close() on a socket descriptor, no further
 * socket operations will succeed on that descriptor.
 *
 * API 7.1.4 SO_LINGER
 *
 * An application using the TCP-style socket can use this option to
 * perform the SCTP ABORT primitive.  The linger option structure is:
 *
 *  struct  linger {
 *     int     l_onoff;                // option on/off
 *     int     l_linger;               // linger time
 * };
 *
 * To enable the option, set l_onoff to 1.  If the l_linger value is set
 * to 0, calling close() is the same as the ABORT primitive.  If the
 * value is set to a negative value, the setsockopt() call will return
 * an error.  If the value is set to a positive value linger_time, the
 * close() can be blocked for at most linger_time ms.  If the graceful
 * shutdown phase does not finish during this period, close() will
 * return but the graceful shutdown phase continues in the system.
 */
SCTP_STATIC void sctp_close(struct sock *sk, long timeout)
{
	struct sctp_endpoint *ep;
	struct sctp_association *asoc;
	struct list_head *pos, *temp;

	SCTP_DEBUG_PRINTK("sctp_close(sk: 0x%p, timeout:%ld)\n", sk, timeout);

	sctp_lock_sock(sk);
	sk->sk_shutdown = SHUTDOWN_MASK;
	sk->sk_state = SCTP_SS_CLOSING;

	ep = sctp_sk(sk)->ep;

	/* Walk all associations on an endpoint.  */
	list_for_each_safe(pos, temp, &ep->asocs) {
		asoc = list_entry(pos, struct sctp_association, asocs);

		if (sctp_style(sk, TCP)) {
			/* A closed association can still be in the list if
			 * it belongs to a TCP-style listening socket that is
			 * not yet accepted. If so, free it. If not, send an
			 * ABORT or SHUTDOWN based on the linger options.
			 */
			if (sctp_state(asoc, CLOSED)) {
				sctp_unhash_established(asoc);
				sctp_association_free(asoc);
				continue;
			}
		}

		if (sock_flag(sk, SOCK_LINGER) && !sk->sk_lingertime) {
			struct sctp_chunk *chunk;

			chunk = sctp_make_abort_user(asoc, NULL, 0);
			if (chunk)
				sctp_primitive_ABORT(asoc, chunk);
		} else
			sctp_primitive_SHUTDOWN(asoc, NULL);
	}

	/* Clean up any skbs sitting on the receive queue.  */
	sctp_queue_purge_ulpevents(&sk->sk_receive_queue);
	sctp_queue_purge_ulpevents(&sctp_sk(sk)->pd_lobby);

	/* On a TCP-style socket, block for at most linger_time if set. */
	if (sctp_style(sk, TCP) && timeout)
		sctp_wait_for_close(sk, timeout);

	/* This will run the backlog queue.  */
	sctp_release_sock(sk);

	/* Supposedly, no process has access to the socket, but
	 * the net layers still may.
	 */
	sctp_local_bh_disable();
	sctp_bh_lock_sock(sk);

	/* Hold the sock, since sk_common_release() will put sock_put()
	 * and we have just a little more cleanup.
	 */
	sock_hold(sk);
	sk_common_release(sk);

	sctp_bh_unlock_sock(sk);
	sctp_local_bh_enable();

	sock_put(sk);

	SCTP_DBG_OBJCNT_DEC(sock);
}

/* Handle EPIPE error. */
static int sctp_error(struct sock *sk, int flags, int err)
{
	if (err == -EPIPE)
		err = sock_error(sk) ? : -EPIPE;
	if (err == -EPIPE && !(flags & MSG_NOSIGNAL))
		send_sig(SIGPIPE, current, 0);
	return err;
}

/* API 3.1.3 sendmsg() - UDP Style Syntax
 *
 * An application uses sendmsg() and recvmsg() calls to transmit data to
 * and receive data from its peer.
 *
 *  ssize_t sendmsg(int socket, const struct msghdr *message,
 *                  int flags);
 *
 *  socket  - the socket descriptor of the endpoint.
 *  message - pointer to the msghdr structure which contains a single
 *            user message and possibly some ancillary data.
 *
 *            See Section 5 for complete description of the data
 *            structures.
 *
 *  flags   - flags sent or received with the user message, see Section
 *            5 for complete description of the flags.
 *
 * Note:  This function could use a rewrite especially when explicit
 * connect support comes in.
 */
/* BUG:  We do not implement the equivalent of sk_stream_wait_memory(). */

SCTP_STATIC int sctp_msghdr_parse(const struct msghdr *, sctp_cmsgs_t *);

SCTP_STATIC int sctp_sendmsg(struct kiocb *iocb, struct sock *sk,
			     struct msghdr *msg, size_t msg_len)
{
	struct sctp_sock *sp;
	struct sctp_endpoint *ep;
	struct sctp_association *new_asoc=NULL, *asoc=NULL;
	struct sctp_transport *transport, *chunk_tp;
	struct sctp_chunk *chunk;
	union sctp_addr to;
	struct sockaddr *msg_name = NULL;
	struct sctp_sndrcvinfo default_sinfo = { 0 };
	struct sctp_sndrcvinfo *sinfo;
	struct sctp_initmsg *sinit;
	sctp_assoc_t associd = 0;
	sctp_cmsgs_t cmsgs = { NULL };
	int err;
	sctp_scope_t scope;
	long timeo;
	__u16 sinfo_flags = 0;
	struct sctp_datamsg *datamsg;
	int msg_flags = msg->msg_flags;

	SCTP_DEBUG_PRINTK("sctp_sendmsg(sk: %p, msg: %p, msg_len: %zu)\n",
			  sk, msg, msg_len);

	err = 0;
	sp = sctp_sk(sk);
	ep = sp->ep;

	SCTP_DEBUG_PRINTK("Using endpoint: %p.\n", ep);

	/* We cannot send a message over a TCP-style listening socket. */
	if (sctp_style(sk, TCP) && sctp_sstate(sk, LISTENING)) {
		err = -EPIPE;
		goto out_nounlock;
	}

	/* Parse out the SCTP CMSGs.  */
	err = sctp_msghdr_parse(msg, &cmsgs);

	if (err) {
		SCTP_DEBUG_PRINTK("msghdr parse err = %x\n", err);
		goto out_nounlock;
	}

	/* Fetch the destination address for this packet.  This
	 * address only selects the association--it is not necessarily
	 * the address we will send to.
	 * For a peeled-off socket, msg_name is ignored.
	 */
	if (!sctp_style(sk, UDP_HIGH_BANDWIDTH) && msg->msg_name) {
		int msg_namelen = msg->msg_namelen;

		err = sctp_verify_addr(sk, (union sctp_addr *)msg->msg_name,
				       msg_namelen);
		if (err)
			return err;

		if (msg_namelen > sizeof(to))
			msg_namelen = sizeof(to);
		memcpy(&to, msg->msg_name, msg_namelen);
		msg_name = msg->msg_name;
	}

	sinfo = cmsgs.info;
	sinit = cmsgs.init;

	/* Did the user specify SNDRCVINFO?  */
	if (sinfo) {
		sinfo_flags = sinfo->sinfo_flags;
		associd = sinfo->sinfo_assoc_id;
	}

	SCTP_DEBUG_PRINTK("msg_len: %zu, sinfo_flags: 0x%x\n",
			  msg_len, sinfo_flags);

	/* SCTP_EOF or SCTP_ABORT cannot be set on a TCP-style socket. */
	if (sctp_style(sk, TCP) && (sinfo_flags & (SCTP_EOF | SCTP_ABORT))) {
		err = -EINVAL;
		goto out_nounlock;
	}

	/* If SCTP_EOF is set, no data can be sent. Disallow sending zero
	 * length messages when SCTP_EOF|SCTP_ABORT is not set.
	 * If SCTP_ABORT is set, the message length could be non zero with
	 * the msg_iov set to the user abort reason.
	 */
	if (((sinfo_flags & SCTP_EOF) && (msg_len > 0)) ||
	    (!(sinfo_flags & (SCTP_EOF|SCTP_ABORT)) && (msg_len == 0))) {
		err = -EINVAL;
		goto out_nounlock;
	}

	/* If SCTP_ADDR_OVER is set, there must be an address
	 * specified in msg_name.
	 */
	if ((sinfo_flags & SCTP_ADDR_OVER) && (!msg->msg_name)) {
		err = -EINVAL;
		goto out_nounlock;
	}

	transport = NULL;

	SCTP_DEBUG_PRINTK("About to look up association.\n");

	sctp_lock_sock(sk);

	/* If a msg_name has been specified, assume this is to be used.  */
	if (msg_name) {
		/* Look for a matching association on the endpoint. */
		asoc = sctp_endpoint_lookup_assoc(ep, &to, &transport);
		if (!asoc) {
			/* If we could not find a matching association on the
			 * endpoint, make sure that it is not a TCP-style
			 * socket that already has an association or there is
			 * no peeled-off association on another socket.
			 */
			if ((sctp_style(sk, TCP) &&
			     sctp_sstate(sk, ESTABLISHED)) ||
			    sctp_endpoint_is_peeled_off(ep, &to)) {
				err = -EADDRNOTAVAIL;
				goto out_unlock;
			}
		}
	} else {
		asoc = sctp_id2assoc(sk, associd);
		if (!asoc) {
			err = -EPIPE;
			goto out_unlock;
		}
	}

	if (asoc) {
		SCTP_DEBUG_PRINTK("Just looked up association: %p.\n", asoc);

		/* We cannot send a message on a TCP-style SCTP_SS_ESTABLISHED
		 * socket that has an association in CLOSED state. This can
		 * happen when an accepted socket has an association that is
		 * already CLOSED.
		 */
		if (sctp_state(asoc, CLOSED) && sctp_style(sk, TCP)) {
			err = -EPIPE;
			goto out_unlock;
		}

		if (sinfo_flags & SCTP_EOF) {
			SCTP_DEBUG_PRINTK("Shutting down association: %p\n",
					  asoc);
			sctp_primitive_SHUTDOWN(asoc, NULL);
			err = 0;
			goto out_unlock;
		}
		if (sinfo_flags & SCTP_ABORT) {

			chunk = sctp_make_abort_user(asoc, msg, msg_len);
			if (!chunk) {
				err = -ENOMEM;
				goto out_unlock;
			}

			SCTP_DEBUG_PRINTK("Aborting association: %p\n", asoc);
			sctp_primitive_ABORT(asoc, chunk);
			err = 0;
			goto out_unlock;
		}
	}

	/* Do we need to create the association?  */
	if (!asoc) {
		SCTP_DEBUG_PRINTK("There is no association yet.\n");

		if (sinfo_flags & (SCTP_EOF | SCTP_ABORT)) {
			err = -EINVAL;
			goto out_unlock;
		}

		/* Check for invalid stream against the stream counts,
		 * either the default or the user specified stream counts.
		 */
		if (sinfo) {
			if (!sinit || (sinit && !sinit->sinit_num_ostreams)) {
				/* Check against the defaults. */
				if (sinfo->sinfo_stream >=
				    sp->initmsg.sinit_num_ostreams) {
					err = -EINVAL;
					goto out_unlock;
				}
			} else {
				/* Check against the requested.  */
				if (sinfo->sinfo_stream >=
				    sinit->sinit_num_ostreams) {
					err = -EINVAL;
					goto out_unlock;
				}
			}
		}

		/*
		 * API 3.1.2 bind() - UDP Style Syntax
		 * If a bind() or sctp_bindx() is not called prior to a
		 * sendmsg() call that initiates a new association, the
		 * system picks an ephemeral port and will choose an address
		 * set equivalent to binding with a wildcard address.
		 */
		if (!ep->base.bind_addr.port) {
			if (sctp_autobind(sk)) {
				err = -EAGAIN;
				goto out_unlock;
			}
		} else {
			/*
			 * If an unprivileged user inherits a one-to-many
			 * style socket with open associations on a privileged
			 * port, it MAY be permitted to accept new associations,
			 * but it SHOULD NOT be permitted to open new
			 * associations.
			 */
			if (ep->base.bind_addr.port < PROT_SOCK &&
			    !capable(CAP_NET_BIND_SERVICE)) {
				err = -EACCES;
				goto out_unlock;
			}
		}

		scope = sctp_scope(&to);
		new_asoc = sctp_association_new(ep, sk, scope, GFP_KERNEL);
		if (!new_asoc) {
			err = -ENOMEM;
			goto out_unlock;
		}
		asoc = new_asoc;
		err = sctp_assoc_set_bind_addr_from_ep(asoc, scope, GFP_KERNEL);
		if (err < 0) {
			err = -ENOMEM;
			goto out_free;
		}

		/* If the SCTP_INIT ancillary data is specified, set all
		 * the association init values accordingly.
		 */
		if (sinit) {
			if (sinit->sinit_num_ostreams) {
				asoc->c.sinit_num_ostreams =
					sinit->sinit_num_ostreams;
			}
			if (sinit->sinit_max_instreams) {
				asoc->c.sinit_max_instreams =
					sinit->sinit_max_instreams;
			}
			if (sinit->sinit_max_attempts) {
				asoc->max_init_attempts
					= sinit->sinit_max_attempts;
			}
			if (sinit->sinit_max_init_timeo) {
				asoc->max_init_timeo =
				 msecs_to_jiffies(sinit->sinit_max_init_timeo);
			}
		}

		/* Prime the peer's transport structures.  */
		transport = sctp_assoc_add_peer(asoc, &to, GFP_KERNEL, SCTP_UNKNOWN);
		if (!transport) {
			err = -ENOMEM;
			goto out_free;
		}
	}

	/* ASSERT: we have a valid association at this point.  */
	SCTP_DEBUG_PRINTK("We have a valid association.\n");

	if (!sinfo) {
		/* If the user didn't specify SNDRCVINFO, make up one with
		 * some defaults.
		 */
		default_sinfo.sinfo_stream = asoc->default_stream;
		default_sinfo.sinfo_flags = asoc->default_flags;
		default_sinfo.sinfo_ppid = asoc->default_ppid;
		default_sinfo.sinfo_context = asoc->default_context;
		default_sinfo.sinfo_timetolive = asoc->default_timetolive;
		default_sinfo.sinfo_assoc_id = sctp_assoc2id(asoc);
		sinfo = &default_sinfo;
	}

	/* API 7.1.7, the sndbuf size per association bounds the
	 * maximum size of data that can be sent in a single send call.
	 */
	if (msg_len > sk->sk_sndbuf) {
		err = -EMSGSIZE;
		goto out_free;
	}

	if (asoc->pmtu_pending)
		sctp_assoc_pending_pmtu(asoc);

	/* If fragmentation is disabled and the message length exceeds the
	 * association fragmentation point, return EMSGSIZE.  The I-D
	 * does not specify what this error is, but this looks like
	 * a great fit.
	 */
	if (sctp_sk(sk)->disable_fragments && (msg_len > asoc->frag_point)) {
		err = -EMSGSIZE;
		goto out_free;
	}

	if (sinfo) {
		/* Check for invalid stream. */
		if (sinfo->sinfo_stream >= asoc->c.sinit_num_ostreams) {
			err = -EINVAL;
			goto out_free;
		}
	}

	timeo = sock_sndtimeo(sk, msg->msg_flags & MSG_DONTWAIT);
	if (!sctp_wspace(asoc)) {
		err = sctp_wait_for_sndbuf(asoc, &timeo, msg_len);
		if (err)
			goto out_free;
	}

	/* If an address is passed with the sendto/sendmsg call, it is used
	 * to override the primary destination address in the TCP model, or
	 * when SCTP_ADDR_OVER flag is set in the UDP model.
	 */
	if ((sctp_style(sk, TCP) && msg_name) ||
	    (sinfo_flags & SCTP_ADDR_OVER)) {
		chunk_tp = sctp_assoc_lookup_paddr(asoc, &to);
		if (!chunk_tp) {
			err = -EINVAL;
			goto out_free;
		}
	} else
		chunk_tp = NULL;

	/* Auto-connect, if we aren't connected already. */
	if (sctp_state(asoc, CLOSED)) {
		err = sctp_primitive_ASSOCIATE(asoc, NULL);
		if (err < 0)
			goto out_free;
		SCTP_DEBUG_PRINTK("We associated primitively.\n");
	}

	/* Break the message into multiple chunks of maximum size. */
	datamsg = sctp_datamsg_from_user(asoc, sinfo, msg, msg_len);
	if (!datamsg) {
		err = -ENOMEM;
		goto out_free;
	}

	/* Now send the (possibly) fragmented message. */
	list_for_each_entry(chunk, &datamsg->chunks, frag_list) {
		sctp_chunk_hold(chunk);

		/* Do accounting for the write space.  */
		sctp_set_owner_w(chunk);

		chunk->transport = chunk_tp;
	}

	/* Send it to the lower layers.  Note:  all chunks
	 * must either fail or succeed.   The lower layer
	 * works that way today.  Keep it that way or this
	 * breaks.
	 */
	err = sctp_primitive_SEND(asoc, datamsg);
	/* Did the lower layer accept the chunk? */
	if (err)
		sctp_datamsg_free(datamsg);
	else
		sctp_datamsg_put(datamsg);

	SCTP_DEBUG_PRINTK("We sent primitively.\n");

	if (err)
		goto out_free;
	else
		err = msg_len;

	/* If we are already past ASSOCIATE, the lower
	 * layers are responsible for association cleanup.
	 */
	goto out_unlock;

out_free:
	if (new_asoc)
		sctp_association_free(asoc);
out_unlock:
	sctp_release_sock(sk);

out_nounlock:
	return sctp_error(sk, msg_flags, err);

#if 0
do_sock_err:
	if (msg_len)
		err = msg_len;
	else
		err = sock_error(sk);
	goto out;

do_interrupted:
	if (msg_len)
		err = msg_len;
	goto out;
#endif /* 0 */
}

/* This is an extended version of skb_pull() that removes the data from the
 * start of a skb even when data is spread across the list of skb's in the
 * frag_list. len specifies the total amount of data that needs to be removed.
 * when 'len' bytes could be removed from the skb, it returns 0.
 * If 'len' exceeds the total skb length,  it returns the no. of bytes that
 * could not be removed.
 */
static int sctp_skb_pull(struct sk_buff *skb, int len)
{
	struct sk_buff *list;
	int skb_len = skb_headlen(skb);
	int rlen;

	if (len <= skb_len) {
		__skb_pull(skb, len);
		return 0;
	}
	len -= skb_len;
	__skb_pull(skb, skb_len);

	skb_walk_frags(skb, list) {
		rlen = sctp_skb_pull(list, len);
		skb->len -= (len-rlen);
		skb->data_len -= (len-rlen);

		if (!rlen)
			return 0;

		len = rlen;
	}

	return len;
}

/* API 3.1.3  recvmsg() - UDP Style Syntax
 *
 *  ssize_t recvmsg(int socket, struct msghdr *message,
 *                    int flags);
 *
 *  socket  - the socket descriptor of the endpoint.
 *  message - pointer to the msghdr structure which contains a single
 *            user message and possibly some ancillary data.
 *
 *            See Section 5 for complete description of the data
 *            structures.
 *
 *  flags   - flags sent or received with the user message, see Section
 *            5 for complete description of the flags.
 */
static struct sk_buff *sctp_skb_recv_datagram(struct sock *, int, int, int *);

SCTP_STATIC int sctp_recvmsg(struct kiocb *iocb, struct sock *sk,
			     struct msghdr *msg, size_t len, int noblock,
			     int flags, int *addr_len)
{
	struct sctp_ulpevent *event = NULL;
	struct sctp_sock *sp = sctp_sk(sk);
	struct sk_buff *skb;
	int copied;
	int err = 0;
	int skb_len;

	SCTP_DEBUG_PRINTK("sctp_recvmsg(%s: %p, %s: %p, %s: %zd, %s: %d, %s: "
			  "0x%x, %s: %p)\n", "sk", sk, "msghdr", msg,
			  "len", len, "knoblauch", noblock,
			  "flags", flags, "addr_len", addr_len);

	sctp_lock_sock(sk);

	if (sctp_style(sk, TCP) && !sctp_sstate(sk, ESTABLISHED)) {
		err = -ENOTCONN;
		goto out;
	}

	skb = sctp_skb_recv_datagram(sk, flags, noblock, &err);
	if (!skb)
		goto out;

	/* Get the total length of the skb including any skb's in the
	 * frag_list.
	 */
	skb_len = skb->len;

	copied = skb_len;
	if (copied > len)
		copied = len;

	err = skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);

	event = sctp_skb2event(skb);

	if (err)
		goto out_free;

	sock_recv_timestamp(msg, sk, skb);
	if (sctp_ulpevent_is_notification(event)) {
		msg->msg_flags |= MSG_NOTIFICATION;
		sp->pf->event_msgname(event, msg->msg_name, addr_len);
	} else {
		sp->pf->skb_msgname(skb, msg->msg_name, addr_len);
	}

	/* Check if we allow SCTP_SNDRCVINFO. */
	if (sp->subscribe.sctp_data_io_event)
		sctp_ulpevent_read_sndrcvinfo(event, msg);
#if 0
	/* FIXME: we should be calling IP/IPv6 layers.  */
	if (sk->sk_protinfo.af_inet.cmsg_flags)
		ip_cmsg_recv(msg, skb);
#endif

	err = copied;

	/* If skb's length exceeds the user's buffer, update the skb and
	 * push it back to the receive_queue so that the next call to
	 * recvmsg() will return the remaining data. Don't set MSG_EOR.
	 */
	if (skb_len > copied) {
		msg->msg_flags &= ~MSG_EOR;
		if (flags & MSG_PEEK)
			goto out_free;
		sctp_skb_pull(skb, copied);
		skb_queue_head(&sk->sk_receive_queue, skb);

		/* When only partial message is copied to the user, increase
		 * rwnd by that amount. If all the data in the skb is read,
		 * rwnd is updated when the event is freed.
		 */
		if (!sctp_ulpevent_is_notification(event))
			sctp_assoc_rwnd_increase(event->asoc, copied);
		goto out;
	} else if ((event->msg_flags & MSG_NOTIFICATION) ||
		   (event->msg_flags & MSG_EOR))
		msg->msg_flags |= MSG_EOR;
	else
		msg->msg_flags &= ~MSG_EOR;

out_free:
	if (flags & MSG_PEEK) {
		/* Release the skb reference acquired after peeking the skb in
		 * sctp_skb_recv_datagram().
		 */
		kfree_skb(skb);
	} else {
		/* Free the event which includes releasing the reference to
		 * the owner of the skb, freeing the skb and updating the
		 * rwnd.
		 */
		sctp_ulpevent_free(event);
	}
out:
	sctp_release_sock(sk);
	return err;
}

/* 7.1.12 Enable/Disable message fragmentation (SCTP_DISABLE_FRAGMENTS)
 *
 * This option is a on/off flag.  If enabled no SCTP message
 * fragmentation will be performed.  Instead if a message being sent
 * exceeds the current PMTU size, the message will NOT be sent and
 * instead a error will be indicated to the user.
 */
static int sctp_setsockopt_disable_fragments(struct sock *sk,
					     char __user *optval,
					     unsigned int optlen)
{
	int val;

	if (optlen < sizeof(int))
		return -EINVAL;

	if (get_user(val, (int __user *)optval))
		return -EFAULT;

	sctp_sk(sk)->disable_fragments = (val == 0) ? 0 : 1;

	return 0;
}

static int sctp_setsockopt_events(struct sock *sk, char __user *optval,
				  unsigned int optlen)
{
	if (optlen > sizeof(struct sctp_event_subscribe))
		return -EINVAL;
	if (copy_from_user(&sctp_sk(sk)->subscribe, optval, optlen))
		return -EFAULT;
	return 0;
}

/* 7.1.8 Automatic Close of associations (SCTP_AUTOCLOSE)
 *
 * This socket option is applicable to the UDP-style socket only.  When
 * set it will cause associations that are idle for more than the
 * specified number of seconds to automatically close.  An association
 * being idle is defined an association that has NOT sent or received
 * user data.  The special value of '0' indicates that no automatic
 * close of any associations should be performed.  The option expects an
 * integer defining the number of seconds of idle time before an
 * association is closed.
 */
static int sctp_setsockopt_autoclose(struct sock *sk, char __user *optval,
				     unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);

	/* Applicable to UDP-style socket only */
	if (sctp_style(sk, TCP))
		return -EOPNOTSUPP;
	if (optlen != sizeof(int))
		return -EINVAL;
	if (copy_from_user(&sp->autoclose, optval, optlen))
		return -EFAULT;

	return 0;
}

/* 7.1.13 Peer Address Parameters (SCTP_PEER_ADDR_PARAMS)
 *
 * Applications can enable or disable heartbeats for any peer address of
 * an association, modify an address's heartbeat interval, force a
 * heartbeat to be sent immediately, and adjust the address's maximum
 * number of retransmissions sent before an address is considered
 * unreachable.  The following structure is used to access and modify an
 * address's parameters:
 *
 *  struct sctp_paddrparams {
 *     sctp_assoc_t            spp_assoc_id;
 *     struct sockaddr_storage spp_address;
 *     uint32_t                spp_hbinterval;
 *     uint16_t                spp_pathmaxrxt;
 *     uint32_t                spp_pathmtu;
 *     uint32_t                spp_sackdelay;
 *     uint32_t                spp_flags;
 * };
 *
 *   spp_assoc_id    - (one-to-many style socket) This is filled in the
 *                     application, and identifies the association for
 *                     this query.
 *   spp_address     - This specifies which address is of interest.
 *   spp_hbinterval  - This contains the value of the heartbeat interval,
 *                     in milliseconds.  If a  value of zero
 *                     is present in this field then no changes are to
 *                     be made to this parameter.
 *   spp_pathmaxrxt  - This contains the maximum number of
 *                     retransmissions before this address shall be
 *                     considered unreachable. If a  value of zero
 *                     is present in this field then no changes are to
 *                     be made to this parameter.
 *   spp_pathmtu     - When Path MTU discovery is disabled the value
 *                     specified here will be the "fixed" path mtu.
 *                     Note that if the spp_address field is empty
 *                     then all associations on this address will
 *                     have this fixed path mtu set upon them.
 *
 *   spp_sackdelay   - When delayed sack is enabled, this value specifies
 *                     the number of milliseconds that sacks will be delayed
 *                     for. This value will apply to all addresses of an
 *                     association if the spp_address field is empty. Note
 *                     also, that if delayed sack is enabled and this
 *                     value is set to 0, no change is made to the last
 *                     recorded delayed sack timer value.
 *
 *   spp_flags       - These flags are used to control various features
 *                     on an association. The flag field may contain
 *                     zero or more of the following options.
 *
 *                     SPP_HB_ENABLE  - Enable heartbeats on the
 *                     specified address. Note that if the address
 *                     field is empty all addresses for the association
 *                     have heartbeats enabled upon them.
 *
 *                     SPP_HB_DISABLE - Disable heartbeats on the
 *                     speicifed address. Note that if the address
 *                     field is empty all addresses for the association
 *                     will have their heartbeats disabled. Note also
 *                     that SPP_HB_ENABLE and SPP_HB_DISABLE are
 *                     mutually exclusive, only one of these two should
 *                     be specified. Enabling both fields will have
 *                     undetermined results.
 *
 *                     SPP_HB_DEMAND - Request a user initiated heartbeat
 *                     to be made immediately.
 *
 *                     SPP_HB_TIME_IS_ZERO - Specify's that the time for
 *                     heartbeat delayis to be set to the value of 0
 *                     milliseconds.
 *
 *                     SPP_PMTUD_ENABLE - This field will enable PMTU
 *                     discovery upon the specified address. Note that
 *                     if the address feild is empty then all addresses
 *                     on the association are effected.
 *
 *                     SPP_PMTUD_DISABLE - This field will disable PMTU
 *                     discovery upon the specified address. Note that
 *                     if the address feild is empty then all addresses
 *                     on the association are effected. Not also that
 *                     SPP_PMTUD_ENABLE and SPP_PMTUD_DISABLE are mutually
 *                     exclusive. Enabling both will have undetermined
 *                     results.
 *
 *                     SPP_SACKDELAY_ENABLE - Setting this flag turns
 *                     on delayed sack. The time specified in spp_sackdelay
 *                     is used to specify the sack delay for this address. Note
 *                     that if spp_address is empty then all addresses will
 *                     enable delayed sack and take on the sack delay
 *                     value specified in spp_sackdelay.
 *                     SPP_SACKDELAY_DISABLE - Setting this flag turns
 *                     off delayed sack. If the spp_address field is blank then
 *                     delayed sack is disabled for the entire association. Note
 *                     also that this field is mutually exclusive to
 *                     SPP_SACKDELAY_ENABLE, setting both will have undefined
 *                     results.
 */
static int sctp_apply_peer_addr_params(struct sctp_paddrparams *params,
				       struct sctp_transport   *trans,
				       struct sctp_association *asoc,
				       struct sctp_sock        *sp,
				       int                      hb_change,
				       int                      pmtud_change,
				       int                      sackdelay_change)
{
	int error;

	if (params->spp_flags & SPP_HB_DEMAND && trans) {
		error = sctp_primitive_REQUESTHEARTBEAT (trans->asoc, trans);
		if (error)
			return error;
	}

	/* Note that unless the spp_flag is set to SPP_HB_ENABLE the value of
	 * this field is ignored.  Note also that a value of zero indicates
	 * the current setting should be left unchanged.
	 */
	if (params->spp_flags & SPP_HB_ENABLE) {

		/* Re-zero the interval if the SPP_HB_TIME_IS_ZERO is
		 * set.  This lets us use 0 value when this flag
		 * is set.
		 */
		if (params->spp_flags & SPP_HB_TIME_IS_ZERO)
			params->spp_hbinterval = 0;

		if (params->spp_hbinterval ||
		    (params->spp_flags & SPP_HB_TIME_IS_ZERO)) {
			if (trans) {
				trans->hbinterval =
				    msecs_to_jiffies(params->spp_hbinterval);
			} else if (asoc) {
				asoc->hbinterval =
				    msecs_to_jiffies(params->spp_hbinterval);
			} else {
				sp->hbinterval = params->spp_hbinterval;
			}
		}
	}

	if (hb_change) {
		if (trans) {
			trans->param_flags =
				(trans->param_flags & ~SPP_HB) | hb_change;
		} else if (asoc) {
			asoc->param_flags =
				(asoc->param_flags & ~SPP_HB) | hb_change;
		} else {
			sp->param_flags =
				(sp->param_flags & ~SPP_HB) | hb_change;
		}
	}

	/* When Path MTU discovery is disabled the value specified here will
	 * be the "fixed" path mtu (i.e. the value of the spp_flags field must
	 * include the flag SPP_PMTUD_DISABLE for this field to have any
	 * effect).
	 */
	if ((params->spp_flags & SPP_PMTUD_DISABLE) && params->spp_pathmtu) {
		if (trans) {
			trans->pathmtu = params->spp_pathmtu;
			sctp_assoc_sync_pmtu(asoc);
		} else if (asoc) {
			asoc->pathmtu = params->spp_pathmtu;
			sctp_frag_point(asoc, params->spp_pathmtu);
		} else {
			sp->pathmtu = params->spp_pathmtu;
		}
	}

	if (pmtud_change) {
		if (trans) {
			int update = (trans->param_flags & SPP_PMTUD_DISABLE) &&
				(params->spp_flags & SPP_PMTUD_ENABLE);
			trans->param_flags =
				(trans->param_flags & ~SPP_PMTUD) | pmtud_change;
			if (update) {
				sctp_transport_pmtu(trans);
				sctp_assoc_sync_pmtu(asoc);
			}
		} else if (asoc) {
			asoc->param_flags =
				(asoc->param_flags & ~SPP_PMTUD) | pmtud_change;
		} else {
			sp->param_flags =
				(sp->param_flags & ~SPP_PMTUD) | pmtud_change;
		}
	}

	/* Note that unless the spp_flag is set to SPP_SACKDELAY_ENABLE the
	 * value of this field is ignored.  Note also that a value of zero
	 * indicates the current setting should be left unchanged.
	 */
	if ((params->spp_flags & SPP_SACKDELAY_ENABLE) && params->spp_sackdelay) {
		if (trans) {
			trans->sackdelay =
				msecs_to_jiffies(params->spp_sackdelay);
		} else if (asoc) {
			asoc->sackdelay =
				msecs_to_jiffies(params->spp_sackdelay);
		} else {
			sp->sackdelay = params->spp_sackdelay;
		}
	}

	if (sackdelay_change) {
		if (trans) {
			trans->param_flags =
				(trans->param_flags & ~SPP_SACKDELAY) |
				sackdelay_change;
		} else if (asoc) {
			asoc->param_flags =
				(asoc->param_flags & ~SPP_SACKDELAY) |
				sackdelay_change;
		} else {
			sp->param_flags =
				(sp->param_flags & ~SPP_SACKDELAY) |
				sackdelay_change;
		}
	}

	/* Note that unless the spp_flag is set to SPP_PMTUD_ENABLE the value
	 * of this field is ignored.  Note also that a value of zero
	 * indicates the current setting should be left unchanged.
	 */
	if ((params->spp_flags & SPP_PMTUD_ENABLE) && params->spp_pathmaxrxt) {
		if (trans) {
			trans->pathmaxrxt = params->spp_pathmaxrxt;
		} else if (asoc) {
			asoc->pathmaxrxt = params->spp_pathmaxrxt;
		} else {
			sp->pathmaxrxt = params->spp_pathmaxrxt;
		}
	}

	return 0;
}

static int sctp_setsockopt_peer_addr_params(struct sock *sk,
					    char __user *optval,
					    unsigned int optlen)
{
	struct sctp_paddrparams  params;
	struct sctp_transport   *trans = NULL;
	struct sctp_association *asoc = NULL;
	struct sctp_sock        *sp = sctp_sk(sk);
	int error;
	int hb_change, pmtud_change, sackdelay_change;

	if (optlen != sizeof(struct sctp_paddrparams))
		return - EINVAL;

	if (copy_from_user(&params, optval, optlen))
		return -EFAULT;

	/* Validate flags and value parameters. */
	hb_change        = params.spp_flags & SPP_HB;
	pmtud_change     = params.spp_flags & SPP_PMTUD;
	sackdelay_change = params.spp_flags & SPP_SACKDELAY;

	if (hb_change        == SPP_HB ||
	    pmtud_change     == SPP_PMTUD ||
	    sackdelay_change == SPP_SACKDELAY ||
	    params.spp_sackdelay > 500 ||
	    (params.spp_pathmtu
	    && params.spp_pathmtu < SCTP_DEFAULT_MINSEGMENT))
		return -EINVAL;

	/* If an address other than INADDR_ANY is specified, and
	 * no transport is found, then the request is invalid.
	 */
	if (!sctp_is_any(sk, ( union sctp_addr *)&params.spp_address)) {
		trans = sctp_addr_id2transport(sk, &params.spp_address,
					       params.spp_assoc_id);
		if (!trans)
			return -EINVAL;
	}

	/* Get association, if assoc_id != 0 and the socket is a one
	 * to many style socket, and an association was not found, then
	 * the id was invalid.
	 */
	asoc = sctp_id2assoc(sk, params.spp_assoc_id);
	if (!asoc && params.spp_assoc_id && sctp_style(sk, UDP))
		return -EINVAL;

	/* Heartbeat demand can only be sent on a transport or
	 * association, but not a socket.
	 */
	if (params.spp_flags & SPP_HB_DEMAND && !trans && !asoc)
		return -EINVAL;

	/* Process parameters. */
	error = sctp_apply_peer_addr_params(&params, trans, asoc, sp,
					    hb_change, pmtud_change,
					    sackdelay_change);

	if (error)
		return error;

	/* If changes are for association, also apply parameters to each
	 * transport.
	 */
	if (!trans && asoc) {
		list_for_each_entry(trans, &asoc->peer.transport_addr_list,
				transports) {
			sctp_apply_peer_addr_params(&params, trans, asoc, sp,
						    hb_change, pmtud_change,
						    sackdelay_change);
		}
	}

	return 0;
}

/*
 * 7.1.23.  Get or set delayed ack timer (SCTP_DELAYED_SACK)
 *
 * This option will effect the way delayed acks are performed.  This
 * option allows you to get or set the delayed ack time, in
 * milliseconds.  It also allows changing the delayed ack frequency.
 * Changing the frequency to 1 disables the delayed sack algorithm.  If
 * the assoc_id is 0, then this sets or gets the endpoints default
 * values.  If the assoc_id field is non-zero, then the set or get
 * effects the specified association for the one to many model (the
 * assoc_id field is ignored by the one to one model).  Note that if
 * sack_delay or sack_freq are 0 when setting this option, then the
 * current values will remain unchanged.
 *
 * struct sctp_sack_info {
 *     sctp_assoc_t            sack_assoc_id;
 *     uint32_t                sack_delay;
 *     uint32_t                sack_freq;
 * };
 *
 * sack_assoc_id -  This parameter, indicates which association the user
 *    is performing an action upon.  Note that if this field's value is
 *    zero then the endpoints default value is changed (effecting future
 *    associations only).
 *
 * sack_delay -  This parameter contains the number of milliseconds that
 *    the user is requesting the delayed ACK timer be set to.  Note that
 *    this value is defined in the standard to be between 200 and 500
 *    milliseconds.
 *
 * sack_freq -  This parameter contains the number of packets that must
 *    be received before a sack is sent without waiting for the delay
 *    timer to expire.  The default value for this is 2, setting this
 *    value to 1 will disable the delayed sack algorithm.
 */

static int sctp_setsockopt_delayed_ack(struct sock *sk,
				       char __user *optval, unsigned int optlen)
{
	struct sctp_sack_info    params;
	struct sctp_transport   *trans = NULL;
	struct sctp_association *asoc = NULL;
	struct sctp_sock        *sp = sctp_sk(sk);

	if (optlen == sizeof(struct sctp_sack_info)) {
		if (copy_from_user(&params, optval, optlen))
			return -EFAULT;

		if (params.sack_delay == 0 && params.sack_freq == 0)
			return 0;
	} else if (optlen == sizeof(struct sctp_assoc_value)) {
		printk(KERN_WARNING "SCTP: Use of struct sctp_assoc_value "
		       "in delayed_ack socket option deprecated\n");
		printk(KERN_WARNING "SCTP: Use struct sctp_sack_info instead\n");
		if (copy_from_user(&params, optval, optlen))
			return -EFAULT;

		if (params.sack_delay == 0)
			params.sack_freq = 1;
		else
			params.sack_freq = 0;
	} else
		return - EINVAL;

	/* Validate value parameter. */
	if (params.sack_delay > 500)
		return -EINVAL;

	/* Get association, if sack_assoc_id != 0 and the socket is a one
	 * to many style socket, and an association was not found, then
	 * the id was invalid.
	 */
	asoc = sctp_id2assoc(sk, params.sack_assoc_id);
	if (!asoc && params.sack_assoc_id && sctp_style(sk, UDP))
		return -EINVAL;

	if (params.sack_delay) {
		if (asoc) {
			asoc->sackdelay =
				msecs_to_jiffies(params.sack_delay);
			asoc->param_flags =
				(asoc->param_flags & ~SPP_SACKDELAY) |
				SPP_SACKDELAY_ENABLE;
		} else {
			sp->sackdelay = params.sack_delay;
			sp->param_flags =
				(sp->param_flags & ~SPP_SACKDELAY) |
				SPP_SACKDELAY_ENABLE;
		}
	}

	if (params.sack_freq == 1) {
		if (asoc) {
			asoc->param_flags =
				(asoc->param_flags & ~SPP_SACKDELAY) |
				SPP_SACKDELAY_DISABLE;
		} else {
			sp->param_flags =
				(sp->param_flags & ~SPP_SACKDELAY) |
				SPP_SACKDELAY_DISABLE;
		}
	} else if (params.sack_freq > 1) {
		if (asoc) {
			asoc->sackfreq = params.sack_freq;
			asoc->param_flags =
				(asoc->param_flags & ~SPP_SACKDELAY) |
				SPP_SACKDELAY_ENABLE;
		} else {
			sp->sackfreq = params.sack_freq;
			sp->param_flags =
				(sp->param_flags & ~SPP_SACKDELAY) |
				SPP_SACKDELAY_ENABLE;
		}
	}

	/* If change is for association, also apply to each transport. */
	if (asoc) {
		list_for_each_entry(trans, &asoc->peer.transport_addr_list,
				transports) {
			if (params.sack_delay) {
				trans->sackdelay =
					msecs_to_jiffies(params.sack_delay);
				trans->param_flags =
					(trans->param_flags & ~SPP_SACKDELAY) |
					SPP_SACKDELAY_ENABLE;
			}
			if (params.sack_freq == 1) {
				trans->param_flags =
					(trans->param_flags & ~SPP_SACKDELAY) |
					SPP_SACKDELAY_DISABLE;
			} else if (params.sack_freq > 1) {
				trans->sackfreq = params.sack_freq;
				trans->param_flags =
					(trans->param_flags & ~SPP_SACKDELAY) |
					SPP_SACKDELAY_ENABLE;
			}
		}
	}

	return 0;
}

/* 7.1.3 Initialization Parameters (SCTP_INITMSG)
 *
 * Applications can specify protocol parameters for the default association
 * initialization.  The option name argument to setsockopt() and getsockopt()
 * is SCTP_INITMSG.
 *
 * Setting initialization parameters is effective only on an unconnected
 * socket (for UDP-style sockets only future associations are effected
 * by the change).  With TCP-style sockets, this option is inherited by
 * sockets derived from a listener socket.
 */
static int sctp_setsockopt_initmsg(struct sock *sk, char __user *optval, unsigned int optlen)
{
	struct sctp_initmsg sinit;
	struct sctp_sock *sp = sctp_sk(sk);

	if (optlen != sizeof(struct sctp_initmsg))
		return -EINVAL;
	if (copy_from_user(&sinit, optval, optlen))
		return -EFAULT;

	if (sinit.sinit_num_ostreams)
		sp->initmsg.sinit_num_ostreams = sinit.sinit_num_ostreams;
	if (sinit.sinit_max_instreams)
		sp->initmsg.sinit_max_instreams = sinit.sinit_max_instreams;
	if (sinit.sinit_max_attempts)
		sp->initmsg.sinit_max_attempts = sinit.sinit_max_attempts;
	if (sinit.sinit_max_init_timeo)
		sp->initmsg.sinit_max_init_timeo = sinit.sinit_max_init_timeo;

	return 0;
}

/*
 * 7.1.14 Set default send parameters (SCTP_DEFAULT_SEND_PARAM)
 *
 *   Applications that wish to use the sendto() system call may wish to
 *   specify a default set of parameters that would normally be supplied
 *   through the inclusion of ancillary data.  This socket option allows
 *   such an application to set the default sctp_sndrcvinfo structure.
 *   The application that wishes to use this socket option simply passes
 *   in to this call the sctp_sndrcvinfo structure defined in Section
 *   5.2.2) The input parameters accepted by this call include
 *   sinfo_stream, sinfo_flags, sinfo_ppid, sinfo_context,
 *   sinfo_timetolive.  The user must provide the sinfo_assoc_id field in
 *   to this call if the caller is using the UDP model.
 */
static int sctp_setsockopt_default_send_param(struct sock *sk,
					      char __user *optval,
					      unsigned int optlen)
{
	struct sctp_sndrcvinfo info;
	struct sctp_association *asoc;
	struct sctp_sock *sp = sctp_sk(sk);

	if (optlen != sizeof(struct sctp_sndrcvinfo))
		return -EINVAL;
	if (copy_from_user(&info, optval, optlen))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, info.sinfo_assoc_id);
	if (!asoc && info.sinfo_assoc_id && sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		asoc->default_stream = info.sinfo_stream;
		asoc->default_flags = info.sinfo_flags;
		asoc->default_ppid = info.sinfo_ppid;
		asoc->default_context = info.sinfo_context;
		asoc->default_timetolive = info.sinfo_timetolive;
	} else {
		sp->default_stream = info.sinfo_stream;
		sp->default_flags = info.sinfo_flags;
		sp->default_ppid = info.sinfo_ppid;
		sp->default_context = info.sinfo_context;
		sp->default_timetolive = info.sinfo_timetolive;
	}

	return 0;
}

/* 7.1.10 Set Primary Address (SCTP_PRIMARY_ADDR)
 *
 * Requests that the local SCTP stack use the enclosed peer address as
 * the association primary.  The enclosed address must be one of the
 * association peer's addresses.
 */
static int sctp_setsockopt_primary_addr(struct sock *sk, char __user *optval,
					unsigned int optlen)
{
	struct sctp_prim prim;
	struct sctp_transport *trans;

	if (optlen != sizeof(struct sctp_prim))
		return -EINVAL;

	if (copy_from_user(&prim, optval, sizeof(struct sctp_prim)))
		return -EFAULT;

	trans = sctp_addr_id2transport(sk, &prim.ssp_addr, prim.ssp_assoc_id);
	if (!trans)
		return -EINVAL;

	sctp_assoc_set_primary(trans->asoc, trans);

	return 0;
}

/*
 * 7.1.5 SCTP_NODELAY
 *
 * Turn on/off any Nagle-like algorithm.  This means that packets are
 * generally sent as soon as possible and no unnecessary delays are
 * introduced, at the cost of more packets in the network.  Expects an
 *  integer boolean flag.
 */
static int sctp_setsockopt_nodelay(struct sock *sk, char __user *optval,
				   unsigned int optlen)
{
	int val;

	if (optlen < sizeof(int))
		return -EINVAL;
	if (get_user(val, (int __user *)optval))
		return -EFAULT;

	sctp_sk(sk)->nodelay = (val == 0) ? 0 : 1;
	return 0;
}

/*
 *
 * 7.1.1 SCTP_RTOINFO
 *
 * The protocol parameters used to initialize and bound retransmission
 * timeout (RTO) are tunable. sctp_rtoinfo structure is used to access
 * and modify these parameters.
 * All parameters are time values, in milliseconds.  A value of 0, when
 * modifying the parameters, indicates that the current value should not
 * be changed.
 *
 */
static int sctp_setsockopt_rtoinfo(struct sock *sk, char __user *optval, unsigned int optlen)
{
	struct sctp_rtoinfo rtoinfo;
	struct sctp_association *asoc;

	if (optlen != sizeof (struct sctp_rtoinfo))
		return -EINVAL;

	if (copy_from_user(&rtoinfo, optval, optlen))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, rtoinfo.srto_assoc_id);

	/* Set the values to the specific association */
	if (!asoc && rtoinfo.srto_assoc_id && sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		if (rtoinfo.srto_initial != 0)
			asoc->rto_initial =
				msecs_to_jiffies(rtoinfo.srto_initial);
		if (rtoinfo.srto_max != 0)
			asoc->rto_max = msecs_to_jiffies(rtoinfo.srto_max);
		if (rtoinfo.srto_min != 0)
			asoc->rto_min = msecs_to_jiffies(rtoinfo.srto_min);
	} else {
		/* If there is no association or the association-id = 0
		 * set the values to the endpoint.
		 */
		struct sctp_sock *sp = sctp_sk(sk);

		if (rtoinfo.srto_initial != 0)
			sp->rtoinfo.srto_initial = rtoinfo.srto_initial;
		if (rtoinfo.srto_max != 0)
			sp->rtoinfo.srto_max = rtoinfo.srto_max;
		if (rtoinfo.srto_min != 0)
			sp->rtoinfo.srto_min = rtoinfo.srto_min;
	}

	return 0;
}

/*
 *
 * 7.1.2 SCTP_ASSOCINFO
 *
 * This option is used to tune the maximum retransmission attempts
 * of the association.
 * Returns an error if the new association retransmission value is
 * greater than the sum of the retransmission value  of the peer.
 * See [SCTP] for more information.
 *
 */
static int sctp_setsockopt_associnfo(struct sock *sk, char __user *optval, unsigned int optlen)
{

	struct sctp_assocparams assocparams;
	struct sctp_association *asoc;

	if (optlen != sizeof(struct sctp_assocparams))
		return -EINVAL;
	if (copy_from_user(&assocparams, optval, optlen))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, assocparams.sasoc_assoc_id);

	if (!asoc && assocparams.sasoc_assoc_id && sctp_style(sk, UDP))
		return -EINVAL;

	/* Set the values to the specific association */
	if (asoc) {
		if (assocparams.sasoc_asocmaxrxt != 0) {
			__u32 path_sum = 0;
			int   paths = 0;
			struct sctp_transport *peer_addr;

			list_for_each_entry(peer_addr, &asoc->peer.transport_addr_list,
					transports) {
				path_sum += peer_addr->pathmaxrxt;
				paths++;
			}

			/* Only validate asocmaxrxt if we have more than
			 * one path/transport.  We do this because path
			 * retransmissions are only counted when we have more
			 * then one path.
			 */
			if (paths > 1 &&
			    assocparams.sasoc_asocmaxrxt > path_sum)
				return -EINVAL;

			asoc->max_retrans = assocparams.sasoc_asocmaxrxt;
		}

		if (assocparams.sasoc_cookie_life != 0) {
			asoc->cookie_life.tv_sec =
					assocparams.sasoc_cookie_life / 1000;
			asoc->cookie_life.tv_usec =
					(assocparams.sasoc_cookie_life % 1000)
					* 1000;
		}
	} else {
		/* Set the values to the endpoint */
		struct sctp_sock *sp = sctp_sk(sk);

		if (assocparams.sasoc_asocmaxrxt != 0)
			sp->assocparams.sasoc_asocmaxrxt =
						assocparams.sasoc_asocmaxrxt;
		if (assocparams.sasoc_cookie_life != 0)
			sp->assocparams.sasoc_cookie_life =
						assocparams.sasoc_cookie_life;
	}
	return 0;
}

/*
 * 7.1.16 Set/clear IPv4 mapped addresses (SCTP_I_WANT_MAPPED_V4_ADDR)
 *
 * This socket option is a boolean flag which turns on or off mapped V4
 * addresses.  If this option is turned on and the socket is type
 * PF_INET6, then IPv4 addresses will be mapped to V6 representation.
 * If this option is turned off, then no mapping will be done of V4
 * addresses and a user will receive both PF_INET6 and PF_INET type
 * addresses on the socket.
 */
static int sctp_setsockopt_mappedv4(struct sock *sk, char __user *optval, unsigned int optlen)
{
	int val;
	struct sctp_sock *sp = sctp_sk(sk);

	if (optlen < sizeof(int))
		return -EINVAL;
	if (get_user(val, (int __user *)optval))
		return -EFAULT;
	if (val)
		sp->v4mapped = 1;
	else
		sp->v4mapped = 0;

	return 0;
}

/*
 * 8.1.16.  Get or Set the Maximum Fragmentation Size (SCTP_MAXSEG)
 * This option will get or set the maximum size to put in any outgoing
 * SCTP DATA chunk.  If a message is larger than this size it will be
 * fragmented by SCTP into the specified size.  Note that the underlying
 * SCTP implementation may fragment into smaller sized chunks when the
 * PMTU of the underlying association is smaller than the value set by
 * the user.  The default value for this option is '0' which indicates
 * the user is NOT limiting fragmentation and only the PMTU will effect
 * SCTP's choice of DATA chunk size.  Note also that values set larger
 * than the maximum size of an IP datagram will effectively let SCTP
 * control fragmentation (i.e. the same as setting this option to 0).
 *
 * The following structure is used to access and modify this parameter:
 *
 * struct sctp_assoc_value {
 *   sctp_assoc_t assoc_id;
 *   uint32_t assoc_value;
 * };
 *
 * assoc_id:  This parameter is ignored for one-to-one style sockets.
 *    For one-to-many style sockets this parameter indicates which
 *    association the user is performing an action upon.  Note that if
 *    this field's value is zero then the endpoints default value is
 *    changed (effecting future associations only).
 * assoc_value:  This parameter specifies the maximum size in bytes.
 */
static int sctp_setsockopt_maxseg(struct sock *sk, char __user *optval, unsigned int optlen)
{
	struct sctp_assoc_value params;
	struct sctp_association *asoc;
	struct sctp_sock *sp = sctp_sk(sk);
	int val;

	if (optlen == sizeof(int)) {
		printk(KERN_WARNING
		   "SCTP: Use of int in maxseg socket option deprecated\n");
		printk(KERN_WARNING
		   "SCTP: Use struct sctp_assoc_value instead\n");
		if (copy_from_user(&val, optval, optlen))
			return -EFAULT;
		params.assoc_id = 0;
	} else if (optlen == sizeof(struct sctp_assoc_value)) {
		if (copy_from_user(&params, optval, optlen))
			return -EFAULT;
		val = params.assoc_value;
	} else
		return -EINVAL;

	if ((val != 0) && ((val < 8) || (val > SCTP_MAX_CHUNK_LEN)))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc && params.assoc_id && sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		if (val == 0) {
			val = asoc->pathmtu;
			val -= sp->pf->af->net_header_len;
			val -= sizeof(struct sctphdr) +
					sizeof(struct sctp_data_chunk);
		}
		asoc->user_frag = val;
		asoc->frag_point = sctp_frag_point(asoc, asoc->pathmtu);
	} else {
		sp->user_frag = val;
	}

	return 0;
}


/*
 *  7.1.9 Set Peer Primary Address (SCTP_SET_PEER_PRIMARY_ADDR)
 *
 *   Requests that the peer mark the enclosed address as the association
 *   primary. The enclosed address must be one of the association's
 *   locally bound addresses. The following structure is used to make a
 *   set primary request:
 */
static int sctp_setsockopt_peer_primary_addr(struct sock *sk, char __user *optval,
					     unsigned int optlen)
{
	struct sctp_sock	*sp;
	struct sctp_endpoint	*ep;
	struct sctp_association	*asoc = NULL;
	struct sctp_setpeerprim	prim;
	struct sctp_chunk	*chunk;
	int 			err;

	sp = sctp_sk(sk);
	ep = sp->ep;

	if (!sctp_addip_enable)
		return -EPERM;

	if (optlen != sizeof(struct sctp_setpeerprim))
		return -EINVAL;

	if (copy_from_user(&prim, optval, optlen))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, prim.sspp_assoc_id);
	if (!asoc)
		return -EINVAL;

	if (!asoc->peer.asconf_capable)
		return -EPERM;

	if (asoc->peer.addip_disabled_mask & SCTP_PARAM_SET_PRIMARY)
		return -EPERM;

	if (!sctp_state(asoc, ESTABLISHED))
		return -ENOTCONN;

	if (!sctp_assoc_lookup_laddr(asoc, (union sctp_addr *)&prim.sspp_addr))
		return -EADDRNOTAVAIL;

	/* Create an ASCONF chunk with SET_PRIMARY parameter	*/
	chunk = sctp_make_asconf_set_prim(asoc,
					  (union sctp_addr *)&prim.sspp_addr);
	if (!chunk)
		return -ENOMEM;

	err = sctp_send_asconf(asoc, chunk);

	SCTP_DEBUG_PRINTK("We set peer primary addr primitively.\n");

	return err;
}

static int sctp_setsockopt_adaptation_layer(struct sock *sk, char __user *optval,
					    unsigned int optlen)
{
	struct sctp_setadaptation adaptation;

	if (optlen != sizeof(struct sctp_setadaptation))
		return -EINVAL;
	if (copy_from_user(&adaptation, optval, optlen))
		return -EFAULT;

	sctp_sk(sk)->adaptation_ind = adaptation.ssb_adaptation_ind;

	return 0;
}

/*
 * 7.1.29.  Set or Get the default context (SCTP_CONTEXT)
 *
 * The context field in the sctp_sndrcvinfo structure is normally only
 * used when a failed message is retrieved holding the value that was
 * sent down on the actual send call.  This option allows the setting of
 * a default context on an association basis that will be received on
 * reading messages from the peer.  This is especially helpful in the
 * one-2-many model for an application to keep some reference to an
 * internal state machine that is processing messages on the
 * association.  Note that the setting of this value only effects
 * received messages from the peer and does not effect the value that is
 * saved with outbound messages.
 */
static int sctp_setsockopt_context(struct sock *sk, char __user *optval,
				   unsigned int optlen)
{
	struct sctp_assoc_value params;
	struct sctp_sock *sp;
	struct sctp_association *asoc;

	if (optlen != sizeof(struct sctp_assoc_value))
		return -EINVAL;
	if (copy_from_user(&params, optval, optlen))
		return -EFAULT;

	sp = sctp_sk(sk);

	if (params.assoc_id != 0) {
		asoc = sctp_id2assoc(sk, params.assoc_id);
		if (!asoc)
			return -EINVAL;
		asoc->default_rcv_context = params.assoc_value;
	} else {
		sp->default_rcv_context = params.assoc_value;
	}

	return 0;
}

/*
 * 7.1.24.  Get or set fragmented interleave (SCTP_FRAGMENT_INTERLEAVE)
 *
 * This options will at a minimum specify if the implementation is doing
 * fragmented interleave.  Fragmented interleave, for a one to many
 * socket, is when subsequent calls to receive a message may return
 * parts of messages from different associations.  Some implementations
 * may allow you to turn this value on or off.  If so, when turned off,
 * no fragment interleave will occur (which will cause a head of line
 * blocking amongst multiple associations sharing the same one to many
 * socket).  When this option is turned on, then each receive call may
 * come from a different association (thus the user must receive data
 * with the extended calls (e.g. sctp_recvmsg) to keep track of which
 * association each receive belongs to.
 *
 * This option takes a boolean value.  A non-zero value indicates that
 * fragmented interleave is on.  A value of zero indicates that
 * fragmented interleave is off.
 *
 * Note that it is important that an implementation that allows this
 * option to be turned on, have it off by default.  Otherwise an unaware
 * application using the one to many model may become confused and act
 * incorrectly.
 */
static int sctp_setsockopt_fragment_interleave(struct sock *sk,
					       char __user *optval,
					       unsigned int optlen)
{
	int val;

	if (optlen != sizeof(int))
		return -EINVAL;
	if (get_user(val, (int __user *)optval))
		return -EFAULT;

	sctp_sk(sk)->frag_interleave = (val == 0) ? 0 : 1;

	return 0;
}

/*
 * 8.1.21.  Set or Get the SCTP Partial Delivery Point
 *       (SCTP_PARTIAL_DELIVERY_POINT)
 *
 * This option will set or get the SCTP partial delivery point.  This
 * point is the size of a message where the partial delivery API will be
 * invoked to help free up rwnd space for the peer.  Setting this to a
 * lower value will cause partial deliveries to happen more often.  The
 * calls argument is an integer that sets or gets the partial delivery
 * point.  Note also that the call will fail if the user attempts to set
 * this value larger than the socket receive buffer size.
 *
 * Note that any single message having a length smaller than or equal to
 * the SCTP partial delivery point will be delivered in one single read
 * call as long as the user provided buffer is large enough to hold the
 * message.
 */
static int sctp_setsockopt_partial_delivery_point(struct sock *sk,
						  char __user *optval,
						  unsigned int optlen)
{
	u32 val;

	if (optlen != sizeof(u32))
		return -EINVAL;
	if (get_user(val, (int __user *)optval))
		return -EFAULT;

	/* Note: We double the receive buffer from what the user sets
	 * it to be, also initial rwnd is based on rcvbuf/2.
	 */
	if (val > (sk->sk_rcvbuf >> 1))
		return -EINVAL;

	sctp_sk(sk)->pd_point = val;

	return 0; /* is this the right error code? */
}

/*
 * 7.1.28.  Set or Get the maximum burst (SCTP_MAX_BURST)
 *
 * This option will allow a user to change the maximum burst of packets
 * that can be emitted by this association.  Note that the default value
 * is 4, and some implementations may restrict this setting so that it
 * can only be lowered.
 *
 * NOTE: This text doesn't seem right.  Do this on a socket basis with
 * future associations inheriting the socket value.
 */
static int sctp_setsockopt_maxburst(struct sock *sk,
				    char __user *optval,
				    unsigned int optlen)
{
	struct sctp_assoc_value params;
	struct sctp_sock *sp;
	struct sctp_association *asoc;
	int val;
	int assoc_id = 0;

	if (optlen == sizeof(int)) {
		printk(KERN_WARNING
		   "SCTP: Use of int in max_burst socket option deprecated\n");
		printk(KERN_WARNING
		   "SCTP: Use struct sctp_assoc_value instead\n");
		if (copy_from_user(&val, optval, optlen))
			return -EFAULT;
	} else if (optlen == sizeof(struct sctp_assoc_value)) {
		if (copy_from_user(&params, optval, optlen))
			return -EFAULT;
		val = params.assoc_value;
		assoc_id = params.assoc_id;
	} else
		return -EINVAL;

	sp = sctp_sk(sk);

	if (assoc_id != 0) {
		asoc = sctp_id2assoc(sk, assoc_id);
		if (!asoc)
			return -EINVAL;
		asoc->max_burst = val;
	} else
		sp->max_burst = val;

	return 0;
}

/*
 * 7.1.18.  Add a chunk that must be authenticated (SCTP_AUTH_CHUNK)
 *
 * This set option adds a chunk type that the user is requesting to be
 * received only in an authenticated way.  Changes to the list of chunks
 * will only effect future associations on the socket.
 */
static int sctp_setsockopt_auth_chunk(struct sock *sk,
				      char __user *optval,
				      unsigned int optlen)
{
	struct sctp_authchunk val;

	if (!sctp_auth_enable)
		return -EACCES;

	if (optlen != sizeof(struct sctp_authchunk))
		return -EINVAL;
	if (copy_from_user(&val, optval, optlen))
		return -EFAULT;

	switch (val.sauth_chunk) {
		case SCTP_CID_INIT:
		case SCTP_CID_INIT_ACK:
		case SCTP_CID_SHUTDOWN_COMPLETE:
		case SCTP_CID_AUTH:
			return -EINVAL;
	}

	/* add this chunk id to the endpoint */
	return sctp_auth_ep_add_chunkid(sctp_sk(sk)->ep, val.sauth_chunk);
}

/*
 * 7.1.19.  Get or set the list of supported HMAC Identifiers (SCTP_HMAC_IDENT)
 *
 * This option gets or sets the list of HMAC algorithms that the local
 * endpoint requires the peer to use.
 */
static int sctp_setsockopt_hmac_ident(struct sock *sk,
				      char __user *optval,
				      unsigned int optlen)
{
	struct sctp_hmacalgo *hmacs;
	u32 idents;
	int err;

	if (!sctp_auth_enable)
		return -EACCES;

	if (optlen < sizeof(struct sctp_hmacalgo))
		return -EINVAL;

	hmacs = kmalloc(optlen, GFP_KERNEL);
	if (!hmacs)
		return -ENOMEM;

	if (copy_from_user(hmacs, optval, optlen)) {
		err = -EFAULT;
		goto out;
	}

	idents = hmacs->shmac_num_idents;
	if (idents == 0 || idents > SCTP_AUTH_NUM_HMACS ||
	    (idents * sizeof(u16)) > (optlen - sizeof(struct sctp_hmacalgo))) {
		err = -EINVAL;
		goto out;
	}

	err = sctp_auth_ep_set_hmacs(sctp_sk(sk)->ep, hmacs);
out:
	kfree(hmacs);
	return err;
}

/*
 * 7.1.20.  Set a shared key (SCTP_AUTH_KEY)
 *
 * This option will set a shared secret key which is used to build an
 * association shared key.
 */
static int sctp_setsockopt_auth_key(struct sock *sk,
				    char __user *optval,
				    unsigned int optlen)
{
	struct sctp_authkey *authkey;
	struct sctp_association *asoc;
	int ret;

	if (!sctp_auth_enable)
		return -EACCES;

	if (optlen <= sizeof(struct sctp_authkey))
		return -EINVAL;

	authkey = kmalloc(optlen, GFP_KERNEL);
	if (!authkey)
		return -ENOMEM;

	if (copy_from_user(authkey, optval, optlen)) {
		ret = -EFAULT;
		goto out;
	}

	if (authkey->sca_keylength > optlen - sizeof(struct sctp_authkey)) {
		ret = -EINVAL;
		goto out;
	}

	asoc = sctp_id2assoc(sk, authkey->sca_assoc_id);
	if (!asoc && authkey->sca_assoc_id && sctp_style(sk, UDP)) {
		ret = -EINVAL;
		goto out;
	}

	ret = sctp_auth_set_key(sctp_sk(sk)->ep, asoc, authkey);
out:
	kfree(authkey);
	return ret;
}

/*
 * 7.1.21.  Get or set the active shared key (SCTP_AUTH_ACTIVE_KEY)
 *
 * This option will get or set the active shared key to be used to build
 * the association shared key.
 */
static int sctp_setsockopt_active_key(struct sock *sk,
				      char __user *optval,
				      unsigned int optlen)
{
	struct sctp_authkeyid val;
	struct sctp_association *asoc;

	if (!sctp_auth_enable)
		return -EACCES;

	if (optlen != sizeof(struct sctp_authkeyid))
		return -EINVAL;
	if (copy_from_user(&val, optval, optlen))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, val.scact_assoc_id);
	if (!asoc && val.scact_assoc_id && sctp_style(sk, UDP))
		return -EINVAL;

	return sctp_auth_set_active_key(sctp_sk(sk)->ep, asoc,
					val.scact_keynumber);
}

/*
 * 7.1.22.  Delete a shared key (SCTP_AUTH_DELETE_KEY)
 *
 * This set option will delete a shared secret key from use.
 */
static int sctp_setsockopt_del_key(struct sock *sk,
				   char __user *optval,
				   unsigned int optlen)
{
	struct sctp_authkeyid val;
	struct sctp_association *asoc;

	if (!sctp_auth_enable)
		return -EACCES;

	if (optlen != sizeof(struct sctp_authkeyid))
		return -EINVAL;
	if (copy_from_user(&val, optval, optlen))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, val.scact_assoc_id);
	if (!asoc && val.scact_assoc_id && sctp_style(sk, UDP))
		return -EINVAL;

	return sctp_auth_del_key_id(sctp_sk(sk)->ep, asoc,
				    val.scact_keynumber);

}


/* API 6.2 setsockopt(), getsockopt()
 *
 * Applications use setsockopt() and getsockopt() to set or retrieve
 * socket options.  Socket options are used to change the default
 * behavior of sockets calls.  They are described in Section 7.
 *
 * The syntax is:
 *
 *   ret = getsockopt(int sd, int level, int optname, void __user *optval,
 *                    int __user *optlen);
 *   ret = setsockopt(int sd, int level, int optname, const void __user *optval,
 *                    int optlen);
 *
 *   sd      - the socket descript.
 *   level   - set to IPPROTO_SCTP for all SCTP options.
 *   optname - the option name.
 *   optval  - the buffer to store the value of the option.
 *   optlen  - the size of the buffer.
 */
SCTP_STATIC int sctp_setsockopt(struct sock *sk, int level, int optname,
				char __user *optval, unsigned int optlen)
{
	int retval = 0;

	SCTP_DEBUG_PRINTK("sctp_setsockopt(sk: %p... optname: %d)\n",
			  sk, optname);

	/* I can hardly begin to describe how wrong this is.  This is
	 * so broken as to be worse than useless.  The API draft
	 * REALLY is NOT helpful here...  I am not convinced that the
	 * semantics of setsockopt() with a level OTHER THAN SOL_SCTP
	 * are at all well-founded.
	 */
	if (level != SOL_SCTP) {
		struct sctp_af *af = sctp_sk(sk)->pf->af;
		retval = af->setsockopt(sk, level, optname, optval, optlen);
		goto out_nounlock;
	}

	sctp_lock_sock(sk);

	switch (optname) {
	case SCTP_SOCKOPT_BINDX_ADD:
		/* 'optlen' is the size of the addresses buffer. */
		retval = sctp_setsockopt_bindx(sk, (struct sockaddr __user *)optval,
					       optlen, SCTP_BINDX_ADD_ADDR);
		break;

	case SCTP_SOCKOPT_BINDX_REM:
		/* 'optlen' is the size of the addresses buffer. */
		retval = sctp_setsockopt_bindx(sk, (struct sockaddr __user *)optval,
					       optlen, SCTP_BINDX_REM_ADDR);
		break;

	case SCTP_SOCKOPT_CONNECTX_OLD:
		/* 'optlen' is the size of the addresses buffer. */
		retval = sctp_setsockopt_connectx_old(sk,
					    (struct sockaddr __user *)optval,
					    optlen);
		break;

	case SCTP_SOCKOPT_CONNECTX:
		/* 'optlen' is the size of the addresses buffer. */
		retval = sctp_setsockopt_connectx(sk,
					    (struct sockaddr __user *)optval,
					    optlen);
		break;

	case SCTP_DISABLE_FRAGMENTS:
		retval = sctp_setsockopt_disable_fragments(sk, optval, optlen);
		break;

	case SCTP_EVENTS:
		retval = sctp_setsockopt_events(sk, optval, optlen);
		break;

	case SCTP_AUTOCLOSE:
		retval = sctp_setsockopt_autoclose(sk, optval, optlen);
		break;

	case SCTP_PEER_ADDR_PARAMS:
		retval = sctp_setsockopt_peer_addr_params(sk, optval, optlen);
		break;

	case SCTP_DELAYED_ACK:
		retval = sctp_setsockopt_delayed_ack(sk, optval, optlen);
		break;
	case SCTP_PARTIAL_DELIVERY_POINT:
		retval = sctp_setsockopt_partial_delivery_point(sk, optval, optlen);
		break;

	case SCTP_INITMSG:
		retval = sctp_setsockopt_initmsg(sk, optval, optlen);
		break;
	case SCTP_DEFAULT_SEND_PARAM:
		retval = sctp_setsockopt_default_send_param(sk, optval,
							    optlen);
		break;
	case SCTP_PRIMARY_ADDR:
		retval = sctp_setsockopt_primary_addr(sk, optval, optlen);
		break;
	case SCTP_SET_PEER_PRIMARY_ADDR:
		retval = sctp_setsockopt_peer_primary_addr(sk, optval, optlen);
		break;
	case SCTP_NODELAY:
		retval = sctp_setsockopt_nodelay(sk, optval, optlen);
		break;
	case SCTP_RTOINFO:
		retval = sctp_setsockopt_rtoinfo(sk, optval, optlen);
		break;
	case SCTP_ASSOCINFO:
		retval = sctp_setsockopt_associnfo(sk, optval, optlen);
		break;
	case SCTP_I_WANT_MAPPED_V4_ADDR:
		retval = sctp_setsockopt_mappedv4(sk, optval, optlen);
		break;
	case SCTP_MAXSEG:
		retval = sctp_setsockopt_maxseg(sk, optval, optlen);
		break;
	case SCTP_ADAPTATION_LAYER:
		retval = sctp_setsockopt_adaptation_layer(sk, optval, optlen);
		break;
	case SCTP_CONTEXT:
		retval = sctp_setsockopt_context(sk, optval, optlen);
		break;
	case SCTP_FRAGMENT_INTERLEAVE:
		retval = sctp_setsockopt_fragment_interleave(sk, optval, optlen);
		break;
	case SCTP_MAX_BURST:
		retval = sctp_setsockopt_maxburst(sk, opon
 , 200len);
		break;/* SCTP kernAUTH_CHUNKentation
 * (C) Copyright IBMauth_chunk001, 2004
 * Copyright (c) 1999-2000 CiscoHMAC_IDENmentation
 * (C) Copyright IBMhmac_iden2001, 2004
 * Copyright (c) 1999-2000 Cisco, IncKEYCopyright (c) 1999-2001 Motorola, key001, 2004
 * Copyright (c) 1999-2000 Cisco, IncACTIVE is part of the SCTP kernel implemctiveation
 *
 * These functions interface with the sockDELETayer to implement the
 * SCTP Extedelation
 *
 * These functions interfacdefaultentation
 * (-ENOPROTOOPTht (c) 1999-}

	C) Corelease_ight(sk);

out_nounlock:
atiourn tion
 ;
}

/* API 3.1.6 connect() - UDP Style Syntax
 *
 * An application may use theentation iscall inor moree model to initiate anu caassociibute without sending data. you caThe se;
 * is: you caret =entation int sd,entast struct ightaddr *nam,n)
 *len_t yrightyou casd:or moightet descriptorhe Ghave a newlic License aadded toe Free Snam imple anyessr optioure (eitherr option)
 * any_in ohout eveu ca  n the implie6 definedthe RFC2553 [7])e Free Slen implemize ofbut WITHOUT e Fr/
 kernSTATIC 2, or) Contation  option)
 * *01,  option)
 * any l any,
			    URPOS implyrig
{
	2, oerr = 0;
	 option)) Coaf *af;CTP whicets e BOTTOM of 	 kernDEBUG_PRINTK("%s - sk: %pr vers anyile COYou shou: %d\n"etails.__func__, 01, re de You shoul* al/* Validneralou shou beforender by
 commonentation/ntationx routine. A P	af* (C) Coget_af_specific( 330->sa_familyght if (!af ||e 330,
 * < af-> the implyrig {
		eceive-EINVALor S elsesctp-/* Pass correc* You  MERhe G1-1307,e send  (so it knowsor mre
		 * is only on WITHOUT Abeby
 passed.site:/tp-develo__E.
 * See the oundation,p developers <l, NULLght SCTP which is the BOTTOM ofterface.err This SCFIXME: Writmodimmentsany bARTICULAR PURPOSE.
 *dis See the GNU General Publ2, oflagsld haerface.-EOPNOTSUPP; /* STUBctp
his SC4.1.4 accepn is fTCe software;
 * you candistributesand/ong Guo   der tto remod inn establishedP kerPublic License afrombut WIg Guo queuITY or FIendpoint.  A the mentatu caon is distrwill beeithur****la   ng Guo       <presentor monewlyu caformedlic License OR A PARTICULAR PU GNU General Pthe Gg Guo chicago.il.us>
 *    Jon G *    *errld hacopy of the neral Pp a copy of the 
 *    I *e      <kevineral newsk =Yarro a copy of the Gc License a*asoc;
	long timeol ade receoeived aral Public License
 * alsp* (C) Cop NarasimeP relp->>
 * addresC) Copoftwied bTCP)ksctp-devbe in     <jgrimm
		goto outl <piggypes.h>
#instatde <liLISTENING/kernel.h>
#inclurs@listsit.h>
#include <share* (Cic Lrcvshareied bJon G & O_NONBLOCKple P.h>
#inc *   wait_for  Anup Pek, shareil addre.h>
#)apability.h>
 PlacWe treardellelistTY o try to fix*   .com>
 *    I aing w   Arde
te:
a@us.iand pickor mofirse <n License aute.h>
p.h>.#inc bugsoc =cket._entry(ep->r sas.nextblic Licenwill try to fix, ude <ple Peported e <lpf->c <nee  Anup e.
 *//sctp.il addreseportkernel.h>
#incluNOMEMcapability.h>
#incle.neopulneraommon.eldsTY or FIeportela     <soldsknet/imigrctions #inclr sa_tohey are ush> /* fon Gao    _ns
 * uG:  Peport/sctp.,P kernSOCKET_nux/of the:ggy@acm.org>
 *    Narasim.edu =
 * besimha Budieport This SCoftw ker ioctl handler  Karl Knutson          <k_wfrePemmaiah         <pemmcmd, unsig****ixes argrimm          NOIOCTLCMD
static vois
 * ommonuncnse ashich gets     ed durby
 mentatism.h>ute tou caNU Genliz wiloid sct-s you ma pore <lin or FIux/fe FrSoftwaeralNY WARRANTshould alreadyez@izero-fit(strmemoryOR A PARTICULAR PURPOSE.
 *NU Ge BOTTOGNU General Pu>
 *    Kevin Gao@intel.com>
 *
 * Any b Gao          along with GNU CC; se *sk, long timeofile )* th next SCTP release.
 */

#of thI*timeo_p)
static i pthouentatiarea. estsuiwitch (sk->sk_typeksctp SCTP OCK_SEQPArnalentae <l *,  =for internal UDx/waic) 1999-2000 Caddr TREAM
static int sctp_bindx_rem(TCruct sock *, ions which pop      t soTNOgrimORproty of thdr *addr, iions whshed  parameters. voisenf_del_ip(s ca* Bo#inclmodifiedas pu
static i_DEFAULT_SEND_PARAMtic int opyan Latestsuip->ions wh_se <nmived a coc,
			    ppid sctp_chunk *chunk)de <lisctp_chunk *chunk)contexher tp_chunk *chunk)shartoliv sctcorporoc,
			    rcv sctp_addr *, int);max_rp. 2linux/in_sock_migsctp_addr *addr, itp_send_astupnf_del_ip(struct sock *, stru#inclt sock *, int);
static int sctINITMSGct sctp_associ or#incloverridd * Byr *sctp_hmac_a CMSGation *asoc,NU Gmsg.s longnum_ostructs  rate(struct outctl_sctp_chunkhep;
extern in_socinctl_sctp_ate(struct int sysctctl_sctp_rmem[3];
extern attempttp_mem[3];
exteretrans, lonctl_sctp_rmem[3];
extern inic int <linuwhichtotern*, struct sock *,
			     RTO renctionf_del_ip(strruct sock *, struct s#incle_t);
staticf.gonzatic int sctRTOINFOct sctp_association *asoc,rtoinfo.sallo *timeo_sctp_wmssociation.p_chunk on the assocmaxs.
 *ockets_allocatedt sctp_wspace(strucinsctp_association in*, struct sock *,
			     #include <limory_pressure(structiation *, sctp_socket_type_t);
static char *sctp_hmASSOCuf space available at the time#inclf_dels.sr sa_r samaxraddr mic_t sctp_memory#include <lsoc)
{
k_userlocks & SOCKnumber_peer_destin<net/ror *, int);k_userlocks & SOCKk);
	rwntatic int sck_userlocks & SOCKlocalmt = asoc->base.sk->sk_sndbuf - amcookie_lif sctkets_v - S used sndbuf*, struct sock *,
			     ev<ardsub is diet/r. By
			    ,      d by a associs sct offation *amemset(&struize of be, 0,ABILIofeo);
statr_clothe _ accounti) sctp_adDp_send_Peer ATHOUT APsure(struct sock *ions whtp_memory_pressure = 1viaP kernPEER_ADDRf(struStion *asoc,hbintern
 * space chb_n use thsoc)
{
pathBUF_LOCCK)
			amt = 0;
		el chuhe data chuntusctp_as0; //hunkow
			     arl@averyoc)
{
sackdelaytp_associa_own sctp_inclu_set_ownfreq	= 2he data cramind(strucSPP_HB_ENABLE |tails.hunkPMTUDasoc;
	struct sock SACKDELAYasoc;
	sctp_addf enab(strnod sctpmessage frag
 * ense as alez@iperl.com>ation ConfigRANTthroughnt sctpISc;
	_FRAGMENTSct sctp_association *asoc,
isd pe_
	sctp_a 0)
			of thEed pe Nagle algorithmstruions wh_bindx_ap->n * tw(strctp_chunnk pointer in skbto use later  */
	*(v4mappedsctp_chunk *1pointerAuto-close idr sclude <net/roafteror modif_ownerd#incl>base.connseconduct A vals.ibm.0ne vd peaticisocketfeaRRAN. _set_owner_w(chunk-base.sk->sUTOCLOSEct sctp_associ,ct sctorms o-ncludtic int*    h->base.sk->skutoE(chuntp_chunk **)(chunkUsthou you mom>
 sctp_associalimiunk;

	asocuserave tize);
}

/* Veriase.sdapassoci_i= asoc->e data  reports or fpxes you makct sock * email of thControl variizeof(>truparion. * th detp_aryany bugtomicopyrsndbufpd_
 * , 0asimskb_a@us._headciati_sk(sk), lobbail a
	*(valictor ileed ink pointerCm.h>
 a
static int #include NY WARRAN.  Even if wkaddr chang int l/
	afNY WARRANTmory_et thips,(stru it astillmory_preusefulc socstot socpre-ntationWITHOUT A thermRyan La /* foncluder_close(stru_newG:  PGFP_KERNEoll <ddresepude *timeo_p,
	attris thisnclud>
 *
 we cmaa_fagiven along wiBG_OBJCNT_INC(ic iasimpercpu_cou use_inc(&ong timeoets_/
stcy_pr_af *t;
	}
bhkarld pe( Is t*/

prot_inuse_add_id2a_ne <li)tp_s sockL;

, 1asimtp_associked peasocm        0 This SCCleanup anylen);
static int resourceuct Karl Knutson   void    <kaestroyg timeo);
static void sctp_wait_for_close(struct soctic struct sctp_af *sctp_sTABLISHED. It
struct sctp_sock */* R is th our holdoute.h>
#includenk;

	iation by .
 */
<linuxuite.
its id.  free(epssoc(struct sock *sde sctp_assoc_t id)
{
	struct ctp_association *asoc = NULL;

	/* If this is not a UDP-style socket,- assoc id should be ignohis SCTP i Xin7 shutdown           <xingang.guo@s.
 *
 * truct sct2, oroc_t  *    how* This SC     f_used -implementation is distrY or FITinclude <lito_preE(chudct sc
	sphowsocket.Shis is atic iint st scruct scct soc 

	atos  sct);
		ret*)idr_findaic s
stadation *)idr_find(  SHUT_RDs_id, (int)id);
	scs_id_D(sizeof(furwithorecep_auoper>pf->s. Nssociaock_bh(&sctp_assocs_id_lL;

ocol nsioonstatiake Layed, (int)id);
	spin_uWRock_bh(&sctp_assocs_id_lock);

	if (!asocasconc->base.sk,net/iNU Genersock_bh(&sctp_assocs_id_lnt len);

	asoc = sequence
}

/* Look up the transRDport from an address and an assoc id. If both adnet/ic || (asoddress and
ock_bh(&sctp_assocs_id_l id are specimatching the address and the ide socket state is not EStruct sctchicago.il.us>
 *    his sctp_wait_for_close(struct sock *sk, long t try to fix... any /types.h>
#include <linux/ksocket, tr *laddrn_lo& sconfpin_DOWNksctp-dte(sk, ESTABLISHED))
	style
mily_tmpty(&include </kernelor sa_family_t */
#include <net/so&trails.k.h>
#include <net/sctp/sctp.h>
#		uite.
pr*/
sons dpoint_l(ions farroll <	} inthis SC7.2.1 Ainclude <liStatus (ARTICULARUS)
intel.com>
 *    t sop_meieve currhe sispecif* Look up t abblisl Public License  *  clu by
 assoc_t)-1))h>
#i, k);
c || (asgonzndowABILI,usefu(struct sunackstru
	afInc.

 * id ind() is,
ret = bind( ped by
n; eitceipter.n);
st_addr *)addis o_p)-rge(sk_t iion icURPOSE.
 *r_paght IBMh>
#incpeci*addr_asoc = NULL, *id.P. rn NUcs_id_lchar __ne i *2004
 *uct sockaddrude r struct
 *hould ha *sk, long tispecifn - thport *transport;
	union sctp_added given to us we willmemorit_f *d(struct )
			retassoc_uf_used;dd will
tion
 * ( sctpddre  lkscter.
 *
 peci/kernelpopulate thinux/capability.h>
#inclt a =;

	sctp_lock_sP-stylecopy_la   stru(&_lock_, 2004
 * yrigock(sk);

	SCTP_Dsend_capability.h>
#incladdr, i%p, lock_..h>
#ockaddr int r sa_fa *sk, d2assocG:  Pledr, iil addresseaseck(sk);

	SCTP_DEBUG_PRINTK("sctp_bind(sd(struct s= proj->k);
.id_aarythe snd,
 *base.bind_addr.portval = sc, (un2idc !=  Is treturn retvah>
#iAL;

	scth>
#it_local(struct st = aso;

	sctp_relt = t_local(struct s *
 *ret =L;

	sct *
 *_ret );

	return retvar, it sctp_p_bindsnmap_socking(&

	sctp_reltsn_ma Get ocal(struct sint sm 0)


	sctc3];
extern int sysctstruct sctp_af *rn int	/* Check minimum st sysctl_sctstruct sctp_af *valid address_clude  Check m a va    Istruct sctp_af *ease_so.spd   al;
}

static long sctp_ged(struct clude dr_lmemcpyk, addr,.sa_family == AF_INET6 THOUT , &ed(&addr->vipre detailed(&addr->v6xes you ma developers <lk;blishMap ipv4n 0;
}

/* to v4-sndbuf-on-v6ITNESS FObindx_ar,
				   int len)
{
	struct sctp_uct dr_->snd(k, ESTABLIS    (und() - longany l)
		if (!opt->pf->af_supported(AF_INE	struct sctp_af *ily == AF_INET6ock *, ued(&addr->vctp_addr *);

/* Veriily == AF_INET6c = asoed(&addr->ver tf (len < af->sockaddr_len)
		retrt_INEL;

	return rt (addr->sa.sa_family == AF_INET6rt<linjif_ass_trucsecsped(&addr->vrtypto./* Bind a local address eith
 * t sctp_do_binpace
 *r *laddrlen < af->sockaddr_len)
		return NUctp_binUNKNt_lostaten < af->sockaddr_len)
		return NUL.sk->sts lar *laddrput		  sk.P. YFC 2553/* Disallow binding twice. */
	if (!sctp_c struct sctp_af *sctp_sket().
 *   addr    - t%d)to
 k: %p,* the Free.P. Yocal(struct sock *n: %d) EINVAL\nt = etails.	return retval;
}

stgnoreen: %d)\nto		  sk2004
 * , addr, aation. */
	af = sctp_sockaddr_af(sp, addr, lfunctick *, s(tion
 socs.nrn NULL;2d skb, and alsI_addr *)addic(sk-GET sctp_packeuf s)uo@intel.com>
 *    ctp_sk(sk),
	p_addr *)addr);

	r this iscStyle (AF_INE}

/#incnn transport;
}

/* API 3itsockechabilittp_s
				cong	if o Publ The s * id p_memormiss-1))
imert sctp_*
 *   sd      - the sn; eitet descriptor returned by socket().
 *  k);
	 impliefothe address structure (struct s	dr_in or struct
 *         	ockaddr_in6 [RFC 2553]),
 *   addr_leurn Nt an _INETIC int sctp_bind(struct sock *sk, struaddr_len)
{
	int retval = 0;

	sctp_INETsock(sk);

	SCTP_DEBUG_PRINTK("sctp_bind(sk: %p, addr: match r_len: %d)\n",
			  sk,_INET addr_len);

	/* Disallow binding twice. */
	if (!sctp_l = -EINVAL;f = sctp_ctp_d(struct G:  Pum && upported(AF_INETrn NULL; bp->pAF_INET6 &&
	   il addresd(struct *)addr;

	P_DEBUG_PR
	 may have
	 * already ipv6_addr_v4mapped(&addr->v6.sin6_a may have
	 *turn NULL;

	return af;
}

 may have
	 *er to an endpoint or to aeturn -EINVAL;C int sctp_do_bind(struc may have
	 **addr, int len)
{
	struct sctp_sock *sp = may have
	 *ndpoint *ep = sp->ep;
	struct sceturn -EINVAL;

	/* ddr;
	struct sctp_eturn -EINVAL;

	/* Met = 0;

	/* Common sockaddr verification. */
	af = sctp_sockaddr_af(sp, addr, leohs(addr->v4.sin_port);

	m && snUG_PRINTK_IPADDR("sctp_do_bind(sk: %p, new addr: ",
				 ", port: %d, n NUL1.12r in sk/ock);

tion.  */
	sctp_associaic(sk- sk);

	chunk->skt, snum,n);
sOOKIE_His a on/offude <. tracked per association.  *intel	sctp_association_hold(asoc);

  Instead))
	ation.  */net/pr <ar}

/exceede *add					(un*sk yntax matchion.  */tion_NOT_pre <ardandlong *}

	rea
 * be tion_holindtrib willmatchne iriptor returned by socket().
 *  ;
	/* Save the ch already bound port in this case_in or struct
 *    ckaddr_in6 [RFC 2553]),
ude n the retval = 0;

	sctpint *)addr;

	st other endk: %p, addr: * AS;
	n
 * (far, af is vee;
	/* Save the chun=, assommon sockaddr verificationsocket, the ng twiceohs(addr->v4.sin_port);

	_len);

	/ binds each side, sod. */
	if (!sctpSCTP_5 Set notcati *)addrnd ancillary* the ific(sk-EVockname() use. */ SCTP_COOKIE_Hi  Dainsit this iyerifyous from each ens and onlt).
 */
stret =and unac wcom>se Fan	| (asriptor returned by socket().
 *  atic ithe address structure (strr_in or struct
 *           sfer another ASCONF Chuntval = 0;

	sctp_l* Since it is always 1-1 besocket, the rs@listsk: %p, addr: %p* Since it is always 1-1 bASCONF. Note this restriction binds each side, so at any
 * time two ASCONFk, ESTABLISHE accountingbe in-transit on any given association (one s8SNDSIk upc C(chun#include <net/roic(sk-sk_wmem_qasconf(struct sctp_associationedistri_ATOit and esize;
	sk_mem_carge(s  Whe Publsete fotion_caDaisylude <net/rothat sctnk) +
>trumton,thate.h>lly rhis is a f(struct sctp_chuointkb->
out:allyturn Nnk);bind_verify(s}

/net/prk) +
is*********>bind_verify(sfied ihas*
 *  <ardoe Syntax
d onl* If * the(strucengthaVeriatomic_'0'be in traified ins IPv6 or I}

/E(chunpf->byaddress specifong *tiaddr(addr, sk);trucOOKIE_Hexpectunk)long *tegnd t****by
 ey ar determine if it of.
 *
  the Boston,l Public License aisturn NULL;
or returned by socket().
 *  kb->trues
	 * transmission.
	 */
	if (asoc->addip_last_a {
		list_add_tail(&chunA_SNocal endpoint * association.
 *
 *k;

	t sct (union sctp_addr *)addr;

	clude <linux/wa wait until the
 * ASCONF-ACK Chunk returs from the previous Ap_chunk_hold(chunk);
	retval = sctp_primitive_ASCONF(asoc, chunk);
	if (retval)
		sctkb->trueslater.
 ** ASCn-transit on any given association (oHelstat through
		rranchkb df them fails, theoin the  Otherf the socket state       <kao juslof *
 * Since it try to fix... andresseo);
static et **().
poc = NULL;
	seral PuINET famibase.);
s_addr *)sa_addr,kadd a copy of the GNU Geneave received a %p, athem fails, thcannotcallsa_famedily);
 *   nimeo_p);
}

	rtedff#incl Otherwinorstaticuct upit_f= 1;
}
tcp e;
	sk_mem_chhe assocypes.h>
#include <liUDbuf = addrs;
	fother end */
	if (!afval = -EINVAL;
	-develo= NULsm.h>
{
	struct sctp,ckaddr *, int);, IPstruc_ kerf (rd2assoch>

#in < 0;
	}

	/* Hhal  ly, opt%d)\n BOTTOoctyle e Found.sin6_is AF? kold(nt > 0)
k_mem_chaddrcnlike 1-1   Ardeedtval;
		}
	}
t
 * L;

	iubmi id are spdr, iisoc,
	omethby
 ddrcnrandome assocg reports or fixes you make
	sctp_release_soc
 * .sa.he
 * email a   Lto_sockaddrr *addr, int other associto establishe functions below as they are used to export functions
 * used by a project regression testsuite.
 */

/* Forward dadded to tions for internal UDP_HIGH_BANDWIDTHint.

				  the penored. */
	hal     e already bound.
	 * We'll jus		retval = scstruct sctp_af *af;

	SCTP_DEBUG_PRINTK("sctp_bindx_add (sk: %se if ddr		*_arg_tf anyof(retaddr *)sa_addr,epor;

erraddr_len)
{
	intrt *transport;
	union sctp_addr *laddrl = 0;

	sctp_on sctp_addr			*at;
	}

	/* Hold the chunk until an A 				i;
	int 				rINVAL;
		}
	}

	if (snumr;
	un addr_len);

	/ binds each side, s
		retval = sctp_do_bind(skp = sp-., (union sctp_addr *)addr,
				      addr_len);
	else
		retvaong with GNU CC; see :e file tructile  sctp Software Founddpoint.
tion
 * (C) Co
		}

		retions f&veaddr;;

	sp =tion
 *at a li<net/ip.h>
#inc? */mplementati		ren u* Ifd f;
statt_type_tntel.com>it and unackbindx_tion
 * (C*/

/ap_fd(veaddr;, len); SCTP_PARAM_ADDsctp-ux/fcn is thed array id);ach_entry(asoc, &ep->asocs, asocs) {

		if (!asoc->pe *sk); chunTP i
 * the Free Software Foundons fveaddr;d to t, port: %blish a *, sommond sndbuf_ct regressparameters to )\n",
		stp_b
 *
 * T4 or IPv6 address;
		 * determine the address length for walking thru thep = sp->eP_DEBUG_PRIow binding twicaddr: ",
				 
 *
 * This SCSCTP_3d skb, and also a new sk Add a sctp_packet_trant, snum,
				 len);

	/* ked pewillation * heartbeatic socbindn. */
	if (! of{
	intn transport;
}*, iny valiTHOUT 'srcnt)
			cbindxe th,drs/ce a}

/	 * associ
		ret <ardimmedeneriatipointdjus of th list of
	maximumof bind() is,
und, or bind ts CHUNKt addrcntn 0;
}

/*
 *
nside			ssed nrn -EADlall
tructp_ass* ADDY WARRANTion *asoc,
 Anusunk)
n bind add{
	inlist of
	f_del_ip(sation; e;

	id_asoc =urn Nrlocks {}

/* Loct sockaddr *			goto outspsockaddr intUDP-stylt even the implrn -  */
		}
THOUT al = sctpuint32;
			goto out;out;
		}can use thtval)
			goto16ut;

		/* Add the new  chunk skbtval)
			goto out;

		/* Add the new >ep;
	stral)
			goto out;

		/* Add the new _owner_w(= 0; i < addrcnt; i++) {
			addr = (uJon G= 0; }is a UDP-s
		}

		retvocket.(one-to-mbinde;
	sk_mem_c)en);
statnt sctphe terk,
					      struct sockdistribute * id aght p_assocs_idhar Samudralorfied, the associations matisludet_for_.sin_famTHOUT Aocket.truct sctp_assoait_fo struct sctdr *tionet.h>
		}
	}

can use the
 retvalsctpaicifieeere added he
 	 * association as ock_bh(&sctp_assocs_id_lin millictp_chunk)Iurn ere added tic run through each address sVAL;CHUNK.ddrs_len)el;
stan*
 * -EINVthe sk, long() on it.
 *
 * If ae_t)adpoint one i_del_ip(m bind addr chunk skb f Do not remove the
 		p = b *sk, struock_bh(&sctp_assocs_id_lt.next;
		laddr Boston,ails,ITHOUT ANher tbl = sctp_add_bind_addr(bpp_sockaddrtry, list);
		addrs/addrcnt
 * array/length pair, determine if it is IPv6 or IPv4 and call
 * sctp_del_bind() on it.
 *
 * If any of them fails, then the operation will  * trac-asica Path Conte void sc * Iftion *;
statre adock_bh(&sctp_assocs_id_length pair webunk MAY bhe
 "fixed"  chu mtuLL;

	spip = &ep->base.biNotionsociannect(s}

out:
	re IPv4 is  laddfied, the associations matc */
lnclude <net/route.hhas to be ltionock_bh(&sctp_assocs_id_lted iv6 or Ixpressp_bind run upute.h>me Free Sr = (union sctp, int addrtruct binackdr_bued perverify(re add;
}

/* Rfied, the associations match*sk, strucecified in tdr;
	ir (cPRINTKny odrs;
	fock_bh(&sctp_assocs_id_lforllocddrcnt; ction_, &sytate(e neTHOUT eaddredrs,
					      struct sockint cnt;
	int retval = 0;
	void *addr_buf;
	 != tl = sctp_add_bind_addr(bp,lsoof A
	int drs;
	for (cnt = 0; cnt = &astruc here).
		 */
		if (lisre adduct _state0ndx_n -EINVAfy(scthem faie las        be added back.
 *
 cor* widrs;
	for (cn the same p addrs, addrcnde <li socket.uct sode <lin th *asoc,
sctp* Verifyt sctp_chun_rem;
		}

		sa_addr = (uendpo
 *    Ryan Lchunk lagid *addit a removerem;
		}

		sa_addr = (utic willddrcn addres = sctp_ma, set t addrs, adrr_bindx_rem;
		}
hunk->asoc;
	s -r in skbcnt)
			conute.h>k);
	struct sctp_endpoint *ep = sp->sa.sa_fd_addr;
	int retv
	if (!sp->there is nothing more *addr_buf;
	 at least one a
			rDDR_NEW, GFP_A("sctp_bindx_rem (sk: %p, addcnt)
			conked per 
			  sk, addrs, adr_bindx_rem;
		}

		/* FI sk);

 -lock);

robably a need to check if sk->sk_saddr and
	icifsk->sk_rcv_addr are currently set to one of the addresses to
		 * be removed. This is something which needs to be looked into
		 * wtion_, addrseirwe are fixinck *sp =_addr ast))fied, the associations matat
		/* FIXME - Taddr * socket routisociation *)idr_find(
		}
	mutuPv4 aexclusive,    http:/ addred/orwosed to ind() on it.
 *
 * If any oength pai.r in sby
 bothbelow as{
			/* Frem;
		}

		sa_addr = (uund_ip(m*****res* al issues with multi-homing
		 * sockeEMAND - R and>

#d and NU Genersp->t)
			cfied, the associations ma		ret= sct.
		 */
		b issues with multi-homing
		 * s*sk = asoc->b retval
		 * tion_		}
		i*sk rem;
		}

		sa_addr = (utruct sctp
			  sknd
		 * sk->sk_rcv_addr are crun through each address  or FITNESS F fei * be remove;
	struct ast one ) {
			retval = -EADDRNOTAVng which needs f (!aeffectNULL;
st of the
 * association, we do noet routinge chunk for thati < addration.  But it will not
 * affect other associations.
 *
 * Only sctp_setsockopt_bindx() is supposed to call this function.
 */
static int sctp_send_asconf_del_ip(struct sock		*sk,
				   struct sockaddraddrn remsetsockopt_bindx() is suppose, we do not send _add(sk,{
	struct sctpn th

	returrem;
		}

		sa_addr = (u retval;
l the peers of
 {
			/* Faendpoint indd will be added back.
 *
 ting that a list of
 * local addresse* The sndbuf spabh(&ettct socchunx_re *, nd_asconf_del_ip(struct sockdrs;
	for (crr_bins, inength pairin = (union sctpray/length pair, determine  *asoc,
			    sociat (cntruct ethinghas to be t_single_entry(&bp->address_li{
			retal = 0;
	voion.
 */
static int sctp_senRINTK("sctp_bindx_rem (sk: %p		}
		idrs;
	for (cnr_bin* IfassociatEL_IP)
		rem;
		}

		sa_addr = (union scapable)
			continue;

		i_addr *bp = &ep->base.binsk, addrs, addet routinglist_for_each_entry(asoc, &ep->asocs, asocs) {

fetval = -EBUSYn wilretval = 0;
	void *addr_bblan_comm to be looked into
		 * wdrs;
	for (cnt =ck *sp = ething w    drcn
			goto errsingle_entry(&bp->address_list))D))
		v6 or IPv4 fy(s
	return retval;
l_bind() on it.
 *
 * If ansk, addrs, addrcnt);,p_adt_for sp->ep;

	SCTP_DEB*****K("%s: (sk: %p, addrs: %p, addrcnt:are already bound.
	 * We'll just inherrlocks already bound port in this case
	dr_in or struct
 *    !snum)
			snum = bp->port;
		else if (snuunk) {
, then of the address d(struct s sock *sed given to us we will try to fix... aned given to us we willuct sp, (uni*opt,
					union sctpk->list, &asoc->addip_chunk_lete all thet;
	}

	/* Hold the chunk until an ASCONF_ACK if (!laddr)
	INVAL;
		}
	}

	if (snumdress>ep;

	SCTP_DEBUG_PRINTK("%s: (sk: %s trac(p, struct owithot
 * INpackeANYsctp_his is ap = &sizeofo sctp_do_br_lenount < a a bh	confrom t  sd ount }
	}

	return retvis_anon
 *
(  */
	af = sctp_get_rlocks &}

out:
	ron. */
		 */
		S;

	/* See if the address ma		goto out;
		}

ed array of we 		goto out;already bouund before    &traong with GNU CC; seFaiper assd(struct \n" id);


	/* Hold the ch)
		rehe
		Gnt stransport;
}
include;
		!= 0rr_bindlementatiret) {
kaddr to addr, addr, af->sp = &asthem fails, thwasctp_r, addrs, ad);
			he do listSCTP_PARAM_DEL_p, addrs: %p, addrcnt: %d)hat are to be deleted.tp_addr * &&tate = SCTP_ADDR_DELr_bu retval;
}

/* Send sctp-= addrs;
		for (i = 0; i < aduf;
			af =+) {
			

	/* Hold the chhe bind 
		addr_buf<nare(strd(struct same porttp
 *ate = SCTP_can use ther, int len)
{
	struct sce can use th id);ate = SCTP_dr *addrs, t sctp_p->ep;
	strst_for_each_entry(UF_LOCK)t, &asoc->pesrc setst_for_each_e_owner_w(stbe used as source addresse_owner_w(	addr	/*draft-11 doesn'nly y w		}

   < *, s		contiJon Gt to be deleted o err_bindx	transportson *asoc sts.sourcek->ldr *)addr, address t_empty(&bp-	 * about to be deleted and cannot be used as source a
 * afs.
		 */
		list_for_each_entry(transpor
 * aff->peer.transport_addr_list,
					tddrs, int a
			dst_release(transport->dst);
			sctp_transportnion scransport, NULL,
					     sctp_sk(asoc->base.sk));
		}

		retval = sctp_send_asconf(asoc, chuddrs, inut:
	return retval;per for tunneic int 	 * about to be deleted and cannot be we can use th.transport_addr_listtransporuf space
 *st_release(transport->dst);
_set_owner_w(.transport_addr_list,
					tdata chunk skb NULL,
					     sctp_sk(asoc->base.sk));
		}

		retval = sctp_send_asconf(asoc, chuociation *asoc  addresses ->v4.sin_family);
			ifthis isP_DEBUG_PRINTK("%s: (sk: %ONF. Note this restriction binds each side, sven association (rem;SCTP23. ddr_beturations;
	fo{
			retvanto soc sndED addrame() use. */
	if (! that sockactp_cwao us specifieif (!a this functionx_rem;OOKIE_H/
stas youase.ghe caller
L;

	* specifies the to
rem;ecified in the act sctptp_bind -EINct sock  sets errno ct snd thunk tCTP, the portch socketase.1dd(sizeof(st * not present itp_wfreehe adrem;ng which r_leis*)ad, addrcctp_ads calr_paccom>
 *    I If as whrem;ame port.++) {
	ckaddr_le
		 * be non-tic rs, addrcntler
formedSectisocka from d
		 * sk->NEW, GFP_ATOMI exporaddr->addr,
 * th(to checthe following currignoe, ttruct k
 * SCT
 * 
 * t).ind_addr;
	intlly r_sockruct etur_socct s_addi0 w addkaddr_leamete chunk-s, addrcnp_set				(un sctp_ation_remaiasoc -EINVdr	*addrsd_unmatch_add_socm != 			retval = -ENOMEM;
			goto out;
_soc

		retval = sctpgoto out;

		/* Add the nBINDX_REMA caller may
 * not remove all addressct sific(addr->v4.with EINVAL.  -
 *   s then the to
e removemove a m fails, thend unacrem;
		
 *
(asoc)PI 3.n;

	retu
			sociation, and 

			if (!s'drcnt; cx_rem;
		ns(bp, addrcntthe bitwise OR ofunion sctp are mu (INDX_Aby
 fuRRANrem;
		clude <net/routlyranty of _BINDX_REM_NVAL.
 *
 * An apt remove the
  is empty or if there is onrem;
		

	/* If  sockfrom the port given iACK		retvaNF CHbasesociation, aociated wddrcnt; c* If any ofhe termstandar = 0;s thetween 20ific(l500rem;
		ecified in thsocket is asects ed with so that no new association acceptp
 * ADD_Aat muspecific(		cone revest_entry(or (cnt = <ards publisit.hemoveADDR
 *ist of the a the stoaticisk), truc_bindx(SCTP_BIcontinue;is 2ockaddr_lendx_rem;
		re addr
 * truct sctp_enx() will fail, setting errnnowledged
 * at any one time.  Ifess, s_aimeo);
static void the packed array of addresses. This is d array ofkaddr_in6 [RFC 2553]),
 *   addr_lenif both ast that a &asoc->base.bind_addr;
		laddr = sctp_find_unmatch_addr(bp, (union sctp_addr *)addrs,
					   >until an ASCONF_ACK ing the sche routunk until an ASCONF_ACK ido the
 *  NULLis loop
		 * because this is done under a soinds each side, soretval;
}

k: %puntil an ASCONF_ACK i be dere adhe routprintk(ot a_WARnux/ " ker: thack);
nsive operation) neede "siteing th"i		if (!asl
 *t sctp_associatd	    tran+) {
			py the data to kernel. Then we er size -fast- and acly one A+) {
			heck-healthy
 * pointer); if all of those succeed, then we can alloc * as some  hunk with Adddr_buf;
			af = sctpttempt with EIpecific(laddr->v4.sin_family);
			list_for_each_entry(saddr, &bp->address_list, list) {
				if (sctp_cmp_addr_exact(&saddr->a, laddr))
					saddr->state = SCith EINVAL. ;
			}
			addr_buf += af-tempt with EIn;
		}

		/* Update theing against other end
}

/* Helper for tunneling sctp_bindx() request
}

/* Hiation *asoc =&
			addr_buf += af->s   &trandx(struct sX_REM_r, int len)
{
	strurn NUust be IPv4 addres_size = 0;
	strects  union sctruct s NULL.sourceforgize = 0;
	struct sockbuf;ctp_af *af;

	SCTP_DE1sctp_addsourceforge.ndress may be specified as t sctciation *asoc =int addrcnt = 0;
	int walk_size = 0;
	struct sof one or more sockettp_af *af;

	SCTP_DE
{
	struct ssctp_socktopt_bindx: sk %p addrs %%p"
			  " addrs_size %d opt %d\n", sddress type
 * must be used to distinguish the address length (note that this
 * representation is termed a "packed array" ofdress3tatic int t *errasoc, addr))
				ac_alg t, snum,
				 len);

	/* 			    surn NULL;ck *, struc optional sndbuf_used;
	elslong *timeo_poto errion.
 */
staname *, u
 * oc,
	et().
 * () in tket().
 * (t, ss
 * addrs_size)iate meslist_fornt the number 

	/* Walk tr_bu to reasoc  httpe(asocntationnt)
		ic int (>truesize;
	sk_mem_charge(ve some
				sizeof(struct sockaddd() oPARAM_ -EINVssocW
/* TCize;
	sk_mem_chverify(ssociationinheode.ven ly);

		/* If rddres
 *   cket.entatic intcopying the addresses from user thep;
exa_addr;
	struct sctp_af *af;

	SCTP_DEBUG_PRINTK("sctp_bindx_add (sk: %k->list, &asoc->addip_chunk_lhep;
ext;
	}

	/* Hold the chunk until an ASCONF_ACK ie work. Pv4 or IPv6 address;
		 * determine the address length for walking thru the list.
		 */
hep;
exree(chunk);
	else
		asoc->addip_last_asce already bound.
	 * We'll just inhesn NULLld* that is not in the packed array of
	 */
	if (bp->port) {
		if (! sockaddr_in6 [RFC 2553]),
 t sockaddr * int  * the copying wito fix... any f * the mily_
		r *po) retructF_INEint retval = 0;

	sctp_t sockaddr addr __user *addrs,
				   %d)\n",
			  sk,id, 2004
 * connect(struct sock* BUG_PRINTK("%s: (sk: %py the data to kernel. Then we do
				 addr,
				 bpS_NUM_OLDcheckf_del"
 * (__copy_from_user()).
 *
 *addrtruesize;
	sk_mem_ch, do ;
}

/* Rectp_assoc_t)-1))
		dr_lenbindx_	retval = sctp_do_bind(skion sctp_addr *)ONF-ACK Chunk returns ily_
#inn -E(poET, *addr, int l(struct  inherointe routrn e++ addressk));
	cf (ay" of addOldCTP i optgaddr_lep.h>
#inn. */
	if (!portDoest, liworkanspo32-bis a Sprogk) {
runuct s(sa_ 64*af; kerneK("sare already bound.
	 * We'll just inhes, kaddrs, addrcnt);
		if (err)
			goto */
	if (bp->port) {
		if (!snum)
			snum = bp->port;
		else if try to fix... any freturn err;
}lt:
		err = -get_size = 0ort _sizeIC int sctp_bind(struct sola  s ASis nr structtport */
	af = sctp_g
stafind_unmatch_addr(bpn sctp_addr *)addrs(sk); sctle	addr_asoist, &asoc->addip_chunk_lnnect() cannSCONF-ACK Chunk returns from the pret cannot be done even on dr *kaddrs, int addrs_sit be don>ep;

	SCTP_DEBUG_PRINTK("%s: (sk: %k->lt be don. get num <= * i against other endp_connectx().
 * Connect will come in with just a singddress.
 */
static int __sctp_connect(structt sock* sk,
			  struct sockaddr *kaddrs,
			  int addrs_size,
			  sctp_assoc_t *assoc_id)
{
	st		err = -EC int sctp_setsockop*sp;
	struct sctp_endpaddr,(ESTABLISHED
	)		err = -EISCne ooint *ep;
	st_t */
#la   sctp_association *asoc = NULL;
	x_rem(ion *asocddr_bufddr)) {

sta, &la  return NUommon ro
stabetw;
	if ())
			return NULL;
	}

	/* If we get this fa.
		k_siz + a is alrreports or fixes you makL;
	}

	/* If weevelopers <led.
		 *addr->v4.sin_to
			goton, 59those succeed, then we can_por+=* is alr p_ve sctp_ass * it.ntopti		err = -EISCONN;) 
 * for SC			err = -EISCONN;
=asoc2;4 or IPv6 address;
		 * determine the address length for walking thru thetyle(sk, Tresentation is termed a "packed array" = 0;
	int addrcnt = 0;
	int walk_sizethe address structure (struct sockadd addresses. This is to
		 * make sure that we do not del= 0;

	sp = sctp_sk(sk);
	ep = sp->ep;

	/* connect() ot be done on a socket that is already in ESTABLISHED
	 * state - UDP-style peeled off socket or a TCP-style socket that
	 * is alrea	BILI_t space_lefdo_bind:bytesressiedMUST wait until the
 t cannot be done eve* sk, struct sockaddr *kaddrs, int addrs_sityle(sk, TCP) && stching association on the enda socket lock from the
		ize < addrs_size) {
		sa_addr = (union sctp_addr *)addr_buf;
		af = sctp_get_af_specific(sa_addr->sa.sa_family);
		port = ntohs(sa_addr->v4.sin_port)2004
  += 0;setng association on the e,the enIs thfree;
		_fameddret called prior to
			 * an sctp_connecis not supported or if this address
		 * causes the address buffer to overflow return EINVAL.
		 */
		if (!af || (walk_size + af->sockaddr_len) > addrs_size) {
			err = -EINVAL;
			goto out_free;
		}

		/* Save current address so we can work with it */
		mx() call, t<* is alre succeed, theC attribu
		memcpy(&to, sa_addr, af->sockaddr_len);

		err = sctp_verify_addr(sk,&to, af>sockax() call, t- * accept nee addres sockaddrcnt, &( association on the e/* If the 2004
  we get oto n-transit on any give If we couldrt);(_in or struct)tois f2004
 nation port is c If we could* representation is termed a "packed array"  returned by socket().
 *  t;
	}
dx_rem(sk, kaddrs, addrcnt);
		if (err)
			goto outt;
		err = sctp_send_asconf_del_iip(sk, kaddrs, addrcnt);
		break;

	default:
		err = -bindsctp_gebsport *transport;
	union sctp_addr     Kevin Gao     get t */
more d

	return err;
}

/* __sctp_connect(struct sock* sk, struct sockaddr *kaddrs, int addrs_size)
 *
 * Common routine for handling connect() and sctp_connectx().
 * Connect will come in with LOCAL a single address.
 */
static int __sctp_connect(structt so#inclket.
		 */
		if (sctp_endpoint_is_peeled_off(ep, &to)) {
			erze += ++) {
	llowing curr_addr *he
 * lastwill, addrcntt;
	}ly b addze += east one as adntel.com>s publisregADDR mae;

	addrculaAC_ALG
 *    Ryan La	}

	retur0ory
ide routbp;
	(retval)
		sctincldr_bu_ep(asoc, *
 * A single_assoc_t *assoc_id)
{
	struct s
		port = ntohladdr = (union sctpitive_

		addr_bu
	if (err < 0the
		 * .h>
#include ctp_ adddr *)._get__ADD::0

	/uardelleount by a pis is somea     <sglobal/
	if s to be lket.h> /* fors, addrcoint single_t */
#&bruct HOUT LL;
	se rout
 * afamily_t */
#t generally have		return NU_ADD_IP);
		if
				goto out_,->sk_ze sk's d		if (!chunk) {
&e to ta    &trarcu_ porblic asoc 	s not supported or i_rcuke tox_rem()	(retvap_associatdress  (sk->opt_bi		}
			 to tount and d		sctpinuesponfor_con(PF_INETory
 	struct sctp_ &&uct socka(Ar == -6ory
s = sk-ations.
 *
 * timeo);
	if ((err == 0 || err == -= asoROGRESS) && assoc_id)
		*ainet_v6_ipv6cifia UDsoc_id)
		*assoc_id  asoc->assoc_id;

	/* Don't free association on associat;

	cket->file-ckets lags;
AD, addrs, arn err %d\n", he assdonaddr htons(Proto re<linux/socaf = sk);
	sk->sk_current neede;
		y);
inc be laddr->v4.siOOKIE_Hsctp_addwetion.
addr->v4.siets s throuack, nf_updnon suc			}

	 tunneling sctp_connet so -EINVree;
	}
 not supported or ifd to c't generally have sctp_wait_f sctp_assoc
	re: ",
				 soc2;
	str_specifint sctp_wISHED)coulsaddr, sk);
	ske is a* If unbound(asosock *sk, ssp->pf->ed can ei couldcopying the addresses %d)\nl_size = 0;
	union sctp_addr__u16ait_fx_rem()ude ssure(sk, TESTAB*ton 3.1.2 for* If we could>
 *    Kevin Gao    	goto out_free;
			ate - UDP-style peeled sk);
	ep = sp-
	 * is alreadyet->file->f_flags;	timeo = sock_sndtimeo(sk, f_e_ASSOCBLOCK);

	err = sctp_wait_f_connect(asoc, &timeo
	if ((err == || err == -EINPROGRESS) && assoc_id		*assoc_id = asoc->assoc_id;

	/* Don't fr
	if ((erron exit. */
	asoc = NULL;

out_free:

	TP_DEBUG_PRINTK("About to exit_sctp_connect() free asoc: %p"
			  " kaddray" of addrturn EINVAL.
		s = sk-f || (walk_size + and befemp.v4tern_checking	ociation id of t = htons(checkrr ==, opt))
			return NULL;
	}

	/* If we get this far, af is validlags & 	goto out_free;
		}

		/* Save current aociattions.
 *
 * an work with it */ddr)) {_addr, af->sockaddrrr ==				 * accept newpointer to an		 * accept new as->sockaddr_len);
	 this usa out_free;

	ddrs, err);
	if (asiation *asoc2;
	say be specified as INADDR_Ar IN6ADDR_ANY, see
 * Sectione.
 *
 * addrs     oto out_free;
		cc.usu.ointer to an array of one or more socket addresses. Each
 * address is contained in its appropriate structure (i.e. struct
 * sockaddr_in or struct sockaddr_in6) the family of the address type
 * must be used to distengish the address length (note that this
 * representation is termed a "packed array" of addresses). The caller
 * specifies the number of addresses in the array with addrcnt.
 *
 * On success, sctp_connectx() returns 0. It also sets the assoc_id to
 * the association id of the new association.  On failure, sctp_connectx()
 * returns -1, and sets errno to the appropriate error code.  The assoc_id
 * is not touched by the kernel.
 *
 * For SCTP, the port gives a 1-many
				 * style stp_associati open associ sock *, ree(an in each socket address must be the same, or
 *setting erions, but it SHOULD NOT
* sctp_connectx() will fail,ation can use sctp_connectx to initiate/t sctp_transport *transport;
	es passed can er to;
	struct sctp_af *af;
	sctp_scope_t scope;
	long timeo;
	int err = 0;
	int addrcnt = 0;
	intp_associatio kaddrs, addrcnt);
		if (err)
			goto  addresses. This is to
		 * make sure that we do not del_ep(asoc, scope,
							      GFP_KERNEL);
			ifsk);
	ep = sp->ep;

	/* connect() cannot be done on a socket tmore socket addresses. asoc2->state >= SCTP_STATE_ESTABLISHED)
				err = -EISCONN;
			else
				err = -EALREADY;
			ve received a .
 *
 amily is.
 *
 bu(retval OCK &&
				    int retval = 0;

	sctp_lcannot be done even on a TCP-style listening socket.
	 */
	if (sctp_sstate(sk, ESTABLSHED) ||
	    (sctp_style(sk, TCP) && sctp_sstate(sk, LISTENING))) {
		err = -EISCONN;
		go ||
ctp_b		err = -EISCONN;
>= (INTnel  /ion matc */
	af = sctp_ddress even on ;
	}

	/* Walk through the addrs buffer and count three;
		}

		cnt++;
		addr_buf += af->sockaddr_len;
		walk_size += af->sockaddr_len;
	}

	/* In case the user of sctp_connectx() wants an association
	 * id back, assign one now.
	 */
	if (assoc_id) {
		err = sctp_assoc_set_id(asoc, GFP_KERNEL);
		if (err < 0)
			goto out_free;
	}

	err = saddr->sa.sa_family_primitive_ASSOCIATE(asoc, NULL);
	if (err < 0) {
		goto out_free;
	}

	/* Initiaaddr->sa.sa_family);
k's dport and daddr for getpeername() */
	inet_sk(sk)->dport = htoaddr,address family i %p, a
{
	strut_frentinue: we fiy onah sclists.
 d arr *adotion.
a(sk), (L;

	if ess o soddrcned can eiuct sn.h>

#c(laddn patioode.o sk,);
			li with thoslinu long.h> /* forons.
= km)
{
	(ockaddr *kaddrs;

	SCTP *soc_id)
{
	int errx_remis not a UDP-style
 *
 * socket, the ID field ns(asoc->peer.port);
	af = sctp_get_af_specificurnshe
 * lsa_family);
	af->to_sk_daddr(sa_addr, sk);
	sk->sk_err = 0;

	/* in-kernel sockets don't generally have a file allocated to them
	 * if all they do is call sock_create_kern().
	 */
	if (sk->sk_socket->file)
		f_flags = sk->sk_sockrn errfied as INADDR_ANY or k, t gection 3.1.2ctp_ sk,
				      struct IC ints usage&ointer to an gs;

t.h>
%d)\nt be done or the adbu repamily isr;
}

/* Helper for tunneling sctp_connectx() requestough s throtp_setsockopt()
 *
 * API 8.9
 * int ap_connectx(ionf_updatthu IPv6  addrcnt,
 * 			sctp_a'tssoc_t *asoc);
 *
 * If sd is an IPv4 socket, the addresses passed musrns 0. It also sets the assoc_id to
 * thf->sockaddr_len) > addrs_size) {
			err = -EINVAL;
			goto out_free;
		}

		/* Save current aernel.
 *
 * For SCTP, the port given in eabuf socket address mus	,
				 * accept newsctp_connectx() will fail, setting errno to EINV	if (err)
			goto out_free;


 sockaddr __u:nt ad%d)\amily);
			ifg sctp_connecf->a

	/* If provir = 0turn = 0;

	/emcpy(&to, sa_adds usageointer to an ksctp-developeng twice. */
	le(struc htons(ockopt_colea by
 ke_asconf_b{
			her be  to 	/* Make sure the destination 	 */
		if (asoc && asoc->peer.port && asgetaddrs_old pa
.h>
#:
	uct ePv4 snnectctp_association		*asoc;
	struct sctp_bind_p_associatithe address structure (struct sockaddr_in or struct
 *    opy the data to kernel. Then we do
 * the copying without checking the user space area
 * (__copy_from_user()).
 *
 * xit there is no need to do sockfd_put(), sys_setsockopt() does
 * it.
 *
 * sk        The sk of the socket
 * addrs     The pointer to the addresses in ugoto out_free;
		}

		/* If we couldes in user land
 * addrssize Sizot find a matching association on the endpoint,
		 * make sure that there is no peeled-off association matching
		 * the peer address even on another soc	return -EINVAL;

	/* Check the user passed a healthy pointer.  */
	if (unlikely(!access_ok(VERIFY_READ, addrs, addrs_size)))
		return -EFAULT;

	/* Alloc space for the address array in kernel memory.  */
	kaddrs = kmalloc(addrs_size, GFP_KERNEL);
	if (unlikely(!kaddrs))
		return -ENOMEM;

	if (__copy_from_user(kaddrs, addrs, addrs_size)) {
		err = -EFAULT;
	} else {
		err = __sctp_connect(sk, kaddrs, addrs_size, assox() is not called prior to
			 * an sctp_connectx() call, the system picks an
			 * ephemeral port and will sctp_setsockopt_his call
 *sockaddr __user *addrs,
				      int addrs_size)
{
	return __sctp_setsockopt_connectx(sk, addrs, addrs_size, NULL);
}

/*
 * New interface for the API.  The since the API is done with a socket
 * option, to make it simple we feed back the association id is as a return
 * indication to the call.  Error is always negative and association id is
 * always pive.
 */
SCTP_er *optror codntax is:
 *
ck* sk,
				      sddr_len)	 * in thf(param)stinat  structchunk)soc, katruct sockaddr __user *addrs,
				      int addrs_size)
{
	sctp_assoc_t assoc_id = 0;
	int err = 0;

	err = __sctp_setsockopt_connectx(sk, addrs, addrs_size, &assoc_id);

	if (err)
		return err;
	else
		return assoc_id;
}

/*
 * New (hopefully final) interface for the API.
 * We use the sctp_getaddrs_old structure so that use-space library
 * can avoid any unnecessary allocations.   The only defferent part
 * is that we store the actuaut copying the addresses from uriteab open ass /*fixme: righl
 * be?tp
 *             voking eithh of the address buffer into the
 * addrs_num structure member.  That way we caions, but it SHOULD NOT
			TATIC int sctp_gptval,
					int __user *optlen)
{
	struct sctp_getaddrs_old param;
	s#include	 * be permitted to open new associations.
				 */
				if (ep->base.binhead *pos, *temp;

	SCTP_DEBUG_PRINTK("sctp_close -EACCES;
					goto out_fgetaddrs_old pafunctiT;

	err = __sctp_setsockopt_cctp_asso0t
 * Pase_so, and als)
				bRIMARY_packt, snum,d from  is onl	 */
	if hing thtareand/or moenurn NU
		/* Use the and_as		*sk,
				   stily == Af addrtp_associ(struct ing moviend an ASCPublic License ak);
'as to be eaddress in the association's bind ather associ sending an ASCONF chunk, decides
 * it needs to transfer another ASCONF ChuCU protectionrimk, TCault:
		err = -EINVAL;
		break;
	}

out:
	ong timeo);
sctp_addr *)addrs,
					       addrcnt, sp);
		ifribind_addr.port istening socket.
	 */
	if (sctp_ssion_fdr *kaddrs, int addrs_siion_>ep;

	SCTP_DEBUG_PRINTK("%s: (sk: %p, addrs: %p, addrcnt: %d)rim.ssP_ADDR_DEL;
			}
			addddr __user *addrs,
				   ddr *ctp_release_sock(sk				      int aTCONNe asddr)) {
ke_abort_u socka

	sctp_release_sock(skreturn NULL;L);
	}

	/* Clean up any 	/* Does this PF support thily, opt))
			return NULL;
	}

	/* If we get this fap opti. */
	af = sctp_get_UTDOWN(asoc, sk, SOCK_ *)addr_buf;
			af = sctp_get_af_specific(addr->v4.sin_family);
			ifk_linresentation is termed a "packed array" of address11 t
 * A sock *sk Layer Icationo number ADAPLAR ON_ addmp, &ep->asocs) {
		asoc = list_ed(addr, scrs, addength pairis will run the rs_sacklog _buf;
		af = ntinuellt supporache on tac_a-int ex
 * sct = 0;
	void *addr_buf;
	struct soc sock *sk,s;
	 * not yet accepted. If so, freeoperation) needed to copy the data to kernel. Then we do
 s be  will run
	sk_commoe associations currently represenk);
	sk_commo a TCP-style listening socket.
	 */
	if (sctp_ssctp_local_bh_eiationsock *sk.ssbince sk_comm union list.
		 */
	 sock *sk, unTCP-style socket, block for at most linger_time if set. */
	if (sctp_style(st sock *skCP) && timeout)
		sctp_wait_for_close(sk, timeimeout);
4t
 * tp_send_asconf_del_ip(snumber ofsend_asconf(strut, snum, el.com>
 *    * Addtherlen < sflags:
ndto() system      it applicat bind ad	    saf =	      sts an o err;
}

 An apg *tinook  (asseddrs,lint)
			cmem_alloc);


/* nd toddres		retval = 0*
 *   sockopt()
 *
 *tp_bin bind aunnel redistribute  kaddrugh the addrsaddrcnndrcv_setsctp_sk(sk)
ily)-f addrket  - the soAn appli either brameterckopt()
 *
 *simd
		ojectgs);
 *tp_s fails,      ss tthe endpoint.
 *  messag************S* Helprem;
	5ort:ockaonstoesne data fromin the biauses possibl

/* Ae addresINVAL;
ructtionNET6aiah@cc - fla;
stnt or resctp_ad run th - flaint sctp_a {
			/* If ation					chHANTABINET6 &&
	    
		 * btion 5 e and possibld to caket(shose ul so terms of
 * t addrs, adk* sket().
 * ,e fodrs, addiptor of the endpoint.
 *  message pying the addresses from user to 		    sendist
		*addr_asoc = NULL 3.1.2 for/
	if (asoc->addip_last_asconf addrr_in6 [RFC 2553]),
 *   addr_lenndpoint.
 p->port) {
			SCTP_  GFP_KERNEL);
			if (err < 0) {
			on sctp_addr *)addrs,
					       addrcnt, sp);
		iC int sctpenable();

	sock_put(sk);

	SCTP_DBG_OBJCNT_DEC(sk *sp;
	stdr *kaddrs, int addrs_siz&& snum < PROT_SOCUG_PRINTK("%s: (sk: %p, addrs: %p, addrcnt: %d the ae
	 * already bound be	addr_buk;
	union sctp_addr
				      struct sockaddr __user *addrs,
				      int addk;
	union sstruct sc
 * af
			    structr = -;
	union snd(strucfo;
	struct sc of the g *sinit;
	s;
statifo;
	struct sc;
stctp_cmsgs_t cmssctp_addr fo;
	struct scsctp_adctp_cmsgs_t cmsint sctp_autofo;
	struct scint sctp_a *
 * A singlesctp_sndrcvinfo *sinsoc,
			    structtmsg *sinit;
	sctp_assot sctp_do_bind(stctp_cmsgs_t cmsgs = { hunk *chunk);
st	sctp_scope_t scope;
	lonck *, union sctp_ad_flags = 0;
	struct sctp_datnt);
static int sctp_aOT
				 * be permitts restriction binds each side, so at any
 * time two ASCONFFP_ATOMIC s && !(flags & MSG_NOSIGNAL))
		send_sig(SIGPIPE5e in wNOof adme() users;
{
		inebind for - alretting errno be reme */
 struts.
 *
 sociatign) >*  ss One ains  ints possi fait sd,o uatioon. val ess,cmsgs);

ilid(ducascon descrcoh>
#inddrcnts.
 *
 tp_setsnetct s), Nic int sctp_bindx_addboostylet_sk( impltp_scope(&to);
			asoc = sct(structthe address structure (struct socks
 * it needs to transfer another ASCONF Chunk, it MUST wait until the
 * ASCONF-ACK Chunk returns from the previous ASCONF Chunk before s(struct uent ASCONF. Note this restriction binds each side, so at any
 * time two ASCONF may be in-transit on any given association (sig(SIGPIPE, the sndbuf s Free SoftwAULT;
	}

	/* Walk t *asoc,
iation, we on taf = sund, or bind t
		ifk *chu (RTO)p_del_ued pe.ckets_ally data.
 *
 *  update_ip(asoc, &{
	intdr->a, adASCONFf_del_ip(stu canl}

	/* Walk t_del_, in sctp_unk;opriate error co;

	atomic_a,to reppropbind e a rewre data froplication ca		asoc = he associatiosed to cnos a Sretuare mutually or returned by socket().
 *  name;
	the address structure (struct s_in or struct
 *         hdr *, sctp_cmsgs_tingl ok, <0 errnname;
	}name;
	uf;
	struct sctp_af			*af;
	struct list_head		*p;
	isctp(sk, TCP) && (sin a TCP-style listening socket.
	 */
	if (sctp_ssk;
	}

	dr *kaddrs, int addrs_siname;
	ctp_transport *transport, *chunk_tp;
	struct sctp_chunk *chun on the assocsa_family);			}
			addr_bu, the message length 
				      struct sockaddr __user *addrs,
			lace -tp_a*
 * spod by
 k, int rificatiof (!af) {
			= 0;

	/dr *)addr,
on the association.  *ddresses passed must bessociation.		 * a_wspace(struct ssg_len == 0))) {
		err = -EINmaxgoto out_nounlock;
i
		}/* If SCTP_ADDR_OVER is set,i thissk, addrs, addflags & SCTP_EOF) && (msg_le	if (!sctp_sstsoc, CLOSED)) {
				sctp_unhash_establiEOF|SCTP_ABORT)) && (msgtime on the association.oto out_nounlock;
	}

	 sctp_wspace(struct ere must be an address
)
{
	int amt;

	if (t send a message over a TCP-style listening socket. so at any
 * time two ASCONFssages whtp_sstate(sk, LISTENING)) {
		err = -EPIPE;
		goto o2e.sk->sk_sndbufme() use. */
	if (!ret *asoc,
tuaddrnes that weund, or bind toe;
statisp->pf-ng which needs ser sf = adunk)
 * be d to cathe hope that itund, or bind tot after callgm.h>
.
		 */y somum) {
			r			 * socket that alr) {
			rp_rel(struce [ ker]drs/addrcnd      - thgs: 0x%x\n",
			  msg_len, sinfo_flasctp_b

	/* SCTP_EOF or SCTP_ABORT cannoo out;
		err = sctp_send_ascondel_ip(sk, kaddrs, addrcnt)ult:
		err = -EINVAunk) {
t_unlock;
	ault:
		err = -EINVAL;
		break;
	}

out:
	kfree(kaddrs);

	return err;
}

/* __sctp_connecto out_nounloct_unlock;
	 a TCP-style listening socket.
	 */
	if (sctp_ssif (asoc) {
) {
		/* Look",
			  sk,t_unlock;
	ctp_transport *transport, *chunk_tp;
	struct sctp_chunk *chunk_userlocks & SOCK_S
	snum = ntohs(ruct sockssociation in CLOSED stateset to the user abort reason.
	 */
	if (((sinfo_flags & SCTP_EOicat&& (msg_len > 0)) ||
	    (!(snfo_flags & (SCTP_Ek_userlocks & SOCK_SNDBUF_LOCK)
 * af sctp_memor out_ 0;
		}
	} else {
		amt = asois a valid sockadd_flags & SCTP_EOF) t;
	}
	return
 * afalen);tting down associationused sndbuf spmust beused sndbuf.tv_secror cod* 1000) +ror cod;
			err = 0;
			goto uout_unlock/
		}
	ck;
	oint *ep;
	struct sctp_association *asoc = NULL;
	structneling
 * t hto_flags & SCTP_EOF) >base.sk);
			if (amt < 0)
stinatf ((sinfo_flags & SCTP_ADDR_OVER) && (!msg->msg_nae)) {
		err = -EINVAL;
		goto out_nounlock;
	k_userlocks & SOCK_SNDBUF_LOCK)
->sk_userlocks & SOCK_SNDBUF_LOtting down association
		amt = asoamt = 0;
		}
	} else {
		amt = tting down association: %p\n",
				ase.sk->sk_sndbuf - amt;
	}
	ret	sctp_primitive_SHUTDOWN(asoc, NULLror co;
}

/* Increment the used sndbuftting down association>base.sk);
			if (amt < 0) for invalid stream ag for iner the default or the user spfied, assume this is to be used.  */
	if (msg_name) {
		/* Look for a matching ad a message otp_sstate(sk, LISTENING)) {
		err = -EPIPE;
p_asso6ruct/clear IPv4for (i =bility
 * , addrs_WA	struPPED_V4, temp, &ep-> as bind addresses to l
	 * address an use * or IE_HMA= 0; sndbuf_V4s,
						   more of
  EINVAL.
		 *el.com>endpoinaddr->v4.sin_f *, it_n. */
	asdresses			erress in the pany of  (i = 0;V6an	    <ar * endpoint->sinit_num_ostreams) {
	p->eand call APIp_st		 * bindend an am >=
				    s = &as/* If thhe tw| (aso of
 . */
	aso cmsrr == -E				gotoeast one adp_setsockoptt implement the equivalent of sk_ API 3v4the address structure (struct sockas
 * it needs to transfer another ASCONF Chunk, it MUsoc, CLOSED)) {
				sctp_unhash_established(asoc);
				s* ASCONF-ACK Chunk returns from the previous ASCONF asoc->sndbufASCONF. Note this restriction binds each side, so at any
 * time two ASCONF may be in-transit on any giv"packed array" of addresse9.	/* Thor landpport comes sctp_addic(sk-CONTEXTdrs_scapapn as cmsveral;
addro (!aonly) Copyright IBMsctp_ad()t, sor returned by socket().
 *  ddr.portcessarily
	 * the address we will send to.
	 * For a peeled-off socket, msg_n * the copying without c}
	} else {
		asoc =              <kevin.gaop_af			*af;
	struct list_head		*p;
	intnsive operation) needed 	SCTP_DEBUG_PRINTK("Just looked up association: %p. neededk, SOCK_LINGER) && !sk->sssage on a TCP-style SCTP_SS_ESTABLISHED
	sctp_unhash_establishedate = S the addrs bu) {
			e laddr))
					saddr->state = S be deleted.
		 */ort and daddr for getpeernad, set all
		re addatamsg *datamsg;*sk);
statiorting associagly.
		 */
		if (sinistruct sock *sk);
statit send a message over a TCP-style listening socket. */
	if (sctp_style(sk, TCP)o distinguish the address length (acked array" of add8m_ost). The calst of thMthat weFalid address.Sr, iic(sk-MAXSE)) {
 array with addrcnurns
 * -1, and that weBILITtodoesnisa_ayEBUGgont addren);
DATA= bindhe addrsion.  */is lar_addt
 * ar     we dhrough f onlynum);
		ta
 *  sctp_sk, int ength pairfiessociation, anCTP_Bnderlsocit_timeo)     etp_associait anum);
			}
		}s *   bot_p);
dr *addo rem be in  Conton on ttructures.
	int cnt;
	int c, &to, t
 * arrrcnt; cnef th
		if (sheck ifns that do not support thisssociationwillait_foication c/* ASSERT: wo =

 *  */
semovealid address. cmscific_fre Contddrcnt.
 *
t_timeo)'DX_Roi is an{
				asocthe peer's trout;
			}
 sctp_a
	}
				 m
		if ciations;
			}
			ifpf->biIP the
scop* If the useively letsk) || asvalid(sa/* Copy back ini.e.ARAM_DE
	adaults the given
 * addr *)ranty of hunk = sctp_make_asconf_update_ip(asoc, &laddr->a, adails, then theation; e * the copying without c			retvct sockaddr *addr,etval = scgoto out; */
		if (sific(addr->v4.ED state:d with so that nodd the give opty(&save
SCTPn retval;
		}
one ofk* sy(&saveaddr, addr, af->s(sk, ado = &defaulcation can use addresses a listed with those ) to associate
 * additional addresses w a SCTP_BIND endpoint after g bind().  Or use
 * sctp_bindx(SCTP_BINDsize pe_REM_ADDR) to remove somesses a listening
 * socnfo_assoc_id;
		sinfo = &defaur *kaddrs,
			s;
			}
			ifi strtck_sock(sk);

	/* Hold the sock, simaxseNVAL;
		}
		addrcnt++;
		ad)
	 * and we have just a little more cleanup.
	 */
	sock_hold(s);
		new_asoc = sctp_association_p_af			*af;
	struct list_headry
 * (exp* ASCto copy the data to kernrray wl. Then we do
ort);p_asD
	 a
 * (__copy_from_user()).
 *
 * On exit there is no

	if (sinfo) {
	 * the copying without cockopt() does
 }

		/* If the Sp"
			 alloc the memoptimization: we first tion) needed to cosoc;
		err = sctp_assoc_set_bind_addr_f
 * it.
 *
 * sk        The sk of the 
			retuoc) {
	et
 * addrs     The pointer to the addr			continuncillary data is specified, set all
		 * the	}
			addr_buf += afame = NULL;
	struct sctp_sndrcvinfo default_sinfo = { 0 };
	stdingly.
		 */
		if (sinit) {
	ly */
	if (adter to {
				asoc->c.sinit_nu, ESTABLISHEne int scTCP-style socket, block for at most linger_time if set)) {
		err = -EMSGSIZE;
ess type
 * must be used to dist	 */
		if (sf those succeed, then we can alloc 		if (!chunk_tp) {
			err = -EINVAL;f those succeed, then we can d to accept new association4). The caller
ax_init_timlid SCTP adic(sk-chunk->s_INTERLEAVaddre			 * associations.
			 */
			if (ep->base.bind_aation.\nalid SCTP at < PROT_SOCK &&
			    !capable(CAmitively.\n");
	}

	, sk, scope, GFP_KERNEL);
			if (!asoc) {
				err = -ENOMEnsfer another ASCONF Chunk, it MUST wait until the
 * ASCONF-ACK Chunk returns from the previous ileged us, ESTABLISHE a valid SCTP a a one-to-many
			 * style socket with open associations on a privileged
			 * port, it MAY be permitted to accept new association5,
			 * but it SHO mesckaddr. */= sctp_     Ioto out_free;
		SCTP_DEBUG_PRINTK("We associated prikaddr. ructsctp_/
	if t < PROT_SOCK &&
			    !capable(CAs
	 * must either fail land and invoking either sctp_bULL;
	void *addr_buf;
	unsignf) {
		list_add_tail(&chunu32 it MUST wait until the
 u32ock;
		}
		asoc = new_asoc;
		err = t thfragmented message. */
pd/
	if (adone-to-many
			 * style socket with open associations on a privileged
			 * port, it MAY be permitted to accep
			sinux/wy" of addresse8,
			 * but it SHO that wek_migrmax_init_implemgoto out_free;
		SCTP_DEBUG_PRINTK("We associated pri Corp. 20 < PROT_SOCK &&
			    !capable(CA Corp. 200t to binding with a wildcard address.
		 */
		if (!ep

	SCTP_DEB		}
		}

		scope = sctp_scope(&to);
		new_asoc = sctp_association_new(ep, sk, scope, GFP_KERNEL);
		if (!new_asoc) {
	
		err = -EMSGSIZE;
		goto out_free;
	}

	if (sinfo) {
		/* Check forck_migralid stream. */
		if (sinfo->sinfo_stream >= asoc->c.sinit_num_ostreams) {
			err = -EINVAL;
			goto out_free;
		}
	}

	timeo = sock_sndtimeo(sk, msg->msg_flags & MSG_DONTWAIT);
	if (!sctp_wspace(asoc)) {
		err = sctp_wait_for_sndbuf(asoc, &timeo, msg_len);
		isocket
 * addrs     The pointer to the addr			continuEM;
			goto out_free;
		}

		/* If the SCTP_INIT ancillary data is specified, set all
		 * the association init values accordingly.
		 */
		if (sinit) {
	uct sock *,ointer to INVAL;
			goto outt_num_ouct sock *, sookup_paddr(asoc, &to);
		if (!chunk_tp) {
			err = -EINVAL;
			goto out_free;
		}
	} else
		chunk_tp = NULL;

	/* Auto-connect, if we aren't connected already. */
	if (sctp_state(asoc, sctp_scope(&to);
			asoc = sctCopyright (ct to binding with a wildcard address.
		 */
		if (!ep->base.bind_addr.port) {ENOMEM;
			g
 */ttinee Sstructsctp;

		/* If the SERVICE))3  recvmsg() - U_ttinist
		 *
 */| (s
 * Seret ,
 * p"
			/* Dt sy		   free(ypes.h>
#iola,  0; cnck;
		}
		asoACCESfragge,
 e(sk, ESTABLISHED)/
		sh) - Uy haved_ip          ntohs(he ms6 addreshdr.lengthis fp_enable)
		r dathdral;

af->sockaddr_len;
	}

	/* Do th - UDP S) +         ck;
		}
		asoc = new_asoc;
		err = sctp_assoc_s         See Section;
	gs);
 *
 * =           sockaddr 16 TCP-style socket, block for at most linger_time if set sockaddrgs);
 *
 *.sin->sCopyrgs);
 *
 *on binds each side, so at any
 * time t of the fscriptioains a Copyrigs,e Section s, addrcnt);
		break;

	case SCTP_BINDX_REM_ADDR:
		err = sctnsions for tn);

		if (!rlen)
			return 0;

		len = rlen;
	}

	return len;
}

/* API 3.1.3  recvmsg() to keyid	if (sctp_autobind(;
	union sctp_addr *laddr = (une socket descriptor of the endpoik->list, &asoc->addip_chunk_lctp_ulpevt;
	}

	/* Hold the chkaddrs, int addrs_si4
 * Copciation matching
		 * thb_len;

	SC*transport, *chunk_tp;
	struct sctp_chunk *chunval.scac
	}

	snum = verride the prlen, "knoblauch", on address in the TCP model, or
	 * when SCTP_ADDR_OVER flen, "knobkey*sk, st			  asocsions forort)
	ter to sk, TCP) && !sctp_sstamessage - pointer ESTABLISHED)) "Just looked up association:_len;

	Ssg_put(datamsg);

	SCTP_DEBUG_PRINTK("We sent primitively.\n");

	if (err)
		goto out_free;
	else
		err = msg_len	goto out_free;


		/* Check if there ala, Inc.
r *)param.addrs,
			param.addr_num, t noblock,
			     int flags, int *addr_len)
{
	struct sctp_dr *addle Syntax
 *
 *  ssize_t recvmsg(int socket, strtp_skb2evennt *event = NULL;
	struct sctp_sock *t socket, str len)
r *messacsndb/* D  ags);dr *addp"
			_in or struct * ssp = sctp_sk(sk);
	struct sk_buff *skb;
	int copied;
	int err = 0;
	int skb_le len)
* sk, struct sockaddr *kaddrs, int addrs_si: %p, %s: %zd, %s: %d, %s: "
			  "0b, msg->state(sk, LISTENING)) addr,p->gied > len)
)
		retval = sctp_do_bind(sklen,peventsa_family);
		port = ntohs(sa_addr->v4.sin_chAL;

	sctp_rele);
	_read_sndddresck);
	t.h>
numree(kadnothd to calen,
					charenhunk-room */
	sctpL;

	if ( to |= MSG_NOTIFIh contcha single
 *            user message and possibl copied;
	i|= MSG_NOT sk, struct sockaddr *kaddrs, i(&to, sa_addffer bind(inue so that state(sk, LISTENING)num:gram(sk, flags, noblock, &err);o that  +ing data. Dsg_put(datamsg);

	SCTP_DEBUGo out_free        5 for complete de bind(inn ofpevent>base.sofdata. Don't set MSG_EOR.
	 *			scope = sctp_scope(&to);
			asoc = sctp_assoced > len)
		copied = len;

	err = skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);

	event = sctp_skb2event(skb);

	if (err)
		goto out_free;

	sock_recv_timestamp(msg, sk, skb);
	if (sctp_ulpevent_is_notification(event)) {
		msg->msg_flags |= MSG_NOTIFICATION;
		sp->pf->event_msgname(event, msg->msg_name, addr_len);
	} else {
		sp->pf->skb_msgname(skb, msg->msg_name, addr_len);
	}

	/* Check if we allow SCTP_SNDRCVINFO. */
	if (sp->subscribe.sctp_data_io_event)
		sctp_ulpevent_read_sndrcvinfo(event, msg);
#if 0
	/* FIXME: we should be calli "flags"* FIXME: we shon address in the TCP model, or
	 * when SCTP_ADDR_OVER f
	if -ENOMEM;
			gvent)) {
		m*)heck minevent_read_sndter to 
	if message - pointer to tInc.
ghdr stt.cmsg_flags)
		ip_cmsg_rceeds the user's buffer, update the skb and
	 * push it back to the receive_queen > copied) {
		msg->msg_flags &= ~MSG_EOR the next call to
	 * recvmsg() will return the remaining data. Don't set MSG_EOR.
	 */
	if (skb_len > copied) {
		msg->msg_flags &= ~MSG_EOR;
		if (flags & MSG_PEEK)
			ction
 *            5 for complete de copied);
		skb_queue_head(&sk->sk_receive_queue, skb);sinit_max_instreams;
		2.r_w(t it SHOC				(unN(struct s	sctp_get_p			goto addrsk_sngle Be_sockse. */
	if (!med from he assoc*sk, strucddress specified in thattahave
		if	rety(&saveaddr, addr, af->sof addresses. X_REM_ADD(asooto out = 0;
	void *addr_buf;
	struct socD sta*sk, s struct msghdr *msg, size_t len, int noblock,
			     int flags, int *addr_len)
{
	struct sr a TCP-style socket thatt:
		err = -EINVAL;
		break;
	}
/* Did ffer
 *
 * Raddrcnt);

	addr_buf = addrs;
	for (cnt = 0e lower layer accept the chunk? */
	if (err)
		sctp_datamsg_free(d *
 * If sd is an IPvp_disaaddrs
					    /sctp.h>LL;

valp_associaone-to-many
			 * style socket with open associations on a privileged
			 * port, it MAY be permitted to accept new ARTICULAR PURPOSE.
 *k_size < adstruct msghdr *msg, sizvesg, msgopt*/
	 cannot be set on a TCP-stnsfer another ASCONF Chunk, 			*addr_buf; sctp_mor a socket which
		 * hasn' seconds to  its ...iation
 r, bt sct		retur1, 200*/
	 sctp_addtp_ashard ssizgsage aon is b * iw wrxes s);
sta*
 *   sdsockets->saok	strt is beuct  cont(asosel.sa_fapletTP i					 throREALL= sct
 * help)
		uct that I amx() rconvgh sBLISHED
 * Alsosemanticaddrek_size < add


/* a.  An  OTHER THAN SOLe endby a pr(val	sctpwell-, add;

	skb 0;

	/			   != int optl in theopy of the GNU Gemsg);
	else
		sctf.  *ck;
	}al = -EI   Lof '0' indicald bAn asation
 * 2004
 * Copyright (erface.
 *
 * T
				 * bor fis is to be used.  */
	if (msg_name) {
rated into the next SCTdd(struany assotruct sock(sk->sk_faentation
 * (C) Coket().
 *   addr    - th;
	iferific4
 * Copyright (c) 1999-2000 Cisco sk);

	chunk->skters (SCTP_PEER_ADDR_PARAMS)
f a sender, after stions can enablror codeRNOTopyright (c) 1999-2000 Cisco_send_ters (SCTP_PEER_ADDR_PARAMS)
 later
	tions can enable or disable heartbeats for ansk_wmem_qters (SCTP_PEER_ADDR_PARAMS)
ckaddr *sa_tions can enable or disable heartbeats for ant soOPdr,
	LOFFters (SCTP_PEER_ADDR_PARAMS)
addr		*bptions can enable or disable heartbeats for ansctp_packet_tran:
 *
 *  struct sctp_paddrparamsress list
		 * t interval, force a
 * he or disable heartbeats for anyf addreAC Copyright (c) 1999from user to kernel
 * l         spp_hbinterval;
 *     uint16_t              ac_alg axrxt;
 *     uint32_t       rn -EINVAtions can enable or disable heartbeats for anith just a single addrddr_storage spp_address;
 *     uint32em(sk, kaddt interval, force a
 * hea_assoc_id    - (one-to-many style ree;
		}

		addrcntaxrxt;
 *     uint32_t       p_association_new(ep,nd identifies the associati_assoc_id    - (one-to-many style socket) Thi filled in the
 *                     appliof interest.
 *   spp_hbinteon for
 *                     this query.
 * p_address     - This specifies which addressof interest.
 *   spp_hbinterassoc_id    - (one-to-many style socket) Thlled in the
 *                     applinterest.
 *   spp_hbintiation for
 *                     this query.
 *ddress     - This specifies which addres   retransmissions before th access and modify an
 * address's parctp_ECTX3axrxt;
 *     uint32_t       
 * Plea3cations can enable or disable heartbeats for any3.1.3 sendmsg() -axrxt;
 *     uint32_t         ream_wait_memory()tions che association soc_t            spp_assoc_id;
 *  fe(pos, tem:
 *
 *  struct sctp_paddrparahat is
			 * tions can enable or disable heartbeats for anlock;
	axrxt;
 *     uint32_t       s not nections can enable or disable heartbeats for anndbuf saxrxt;
 *     uint32_t       gs);

	/*r of retransmissions sent before an address isk_sndbufred
 * unreachable.  The follotate(sk, Etions can enable or disable heartbeats for anout_unlock;
				}
			axrxt;
 *     uint32_t        equivalentions can enable or disable heartbeats for anit_attof an
 *                     asD
	 *  };
 *
 *   spp_assoc_id    - (one-to-many style socket) T->porddr_storage spp_address;
 *     uint32_ayed
 *              of zero
 *                     is pre/
	sctp_release_red
 * unreachable.  The folloce sk_common_rele           recorded delayed sack timer value.
 *
 * o open *                     be made toET_BINtions can enable or disable heartbeats for an
		if (err < 0)
			axrxt;
 *     uint32_t        chunks of maximum siterest.
 *   spp_hbinterval  - This contains the value ofPARTIAL   sIVERY_POIt (c) 2001-2002 Noki layer
	 * works that way today.  K           recorded del
 *                     is preel implementation
 * (C) Co_association_free(asor of retransmissions sent before an address is ce is par2000 Cisco, Inc.
 * Copt the descriptions from the specificatlude <linux/wai03 Intel Corp.
 * Copyright (c) 2001-2002 Nokib->data_len -= (len-rle Disable heartbeats on the
 *                     ets layer to implement the
 uct sock *sk,
			     s    sctp_assoc_t            spp_assoc_id;
 *     st Inc.
 * e maximum number of
 *              ed > len)
		           recorded delayed sack timer value.
 *
 * ree;
		of these two should
 *                 d to the user, incr           recorded delayed sack timer value.
 *
 * optlen)
{
	int villiseconds that sacks will be del

static tions can enable or disable heartbeions which populate the struct proto
 * for SCTP which is the BOTTOM ofterface.
 *
 * This return is not EShasheo);
static void sctpus.ibm.com>
 *               miunlliseconds.
 *
 *                     Sctp_he(cntfait_fin_fag Guot);
		cPINTK("y f unin.h>

#vailendpoit_fe Free Softwiscovllis  the if (emoveSCTP_BINDX'dr(sa_'len);
srn NULL;c, chunn; eithl.com>br, aif (sock_flan NULL;
 *   			   are ef())rr_binllis
		ifendpoisableinterfof 4096->sk_s	if (co_ep(allisbuf->so. Ea be sep.h>
_ADD      *sk, sttatic iiscov*sk, stllis {
	usock thedx()t eitg *titic in>sockadd the nt sctp_chunk.
 *cifiestinaa giULL)p.h>
ations t
 *  sd addresen the ore wlen)		}

		/	defovery upon ;dx() wwebsectioc ine a reat,l
 * r?);t
 *n -E
 *     t th receiven al,ptval,
      dr *	}

		/
		/* Istruct msgh)the asug() itof ASCified addresk)
{
	in fa flah co_entr(rsi@refNPI ipgrantyor returnThen we do
 * the *    fected. *    ers of t. Then we do
 * theMTU
 *     *
		rctp_associashscovsoto ; SPP_PMTUixes , opt))
		soc t;
	}o automatically cl */
	af = sctp_geOn a el. Then we do
 * the                m@us. the p.h>
stsui         SPP_SACKDELAY_Epp
 *            iscovet>basoizeof(     thhoint (str *(strs. Eadelayed sack. The
	void				ventsthe deh contect(asoon id of ther data.  The special value of ackde()perforent num=newadd. The tieturn -EFassociation *asoc0;

	/*the dTP_INIT aecv(marchntinue;e that
 *       unlikelsctp be high
		 o flingplicae_socke delayedT senid sstengiBUG_delaDISABLckdelrEINV(&pp_ad&dreseturn -eld is of tdres -spp_gs & %d\n     elayesabledom() % field is  +spp_ck;
	d are kadid s		  aso || e      <      ||        >ddresn't fre       aally e socdeationck_flsocifn           s(kaddve_ASSOCckdelsociP_PMT[ unde]id);
	if (F_IN->f_fl&
		r->ets  id);
	if (supportedht */
#ppndx_dr, struct chai sockeis app*/
SCTPrns
      ror co
		ip_UG_PRINand and inassoentaeer_addr_pa);
	if struct sctp_padd}*/
	ress--sociation.> len)      Exhaus_timlist_e,
			bled ruct sock     ?ut to ither  %d\n SCTP_ld is mu		got the gracfai MUST    OK, to thtatic i
SCTwd (we nucall
HEAD  discckdesite:
 the addresp.h>
t */
)currentlarro	if (
 * int it'ssite:
mutex/lksctp
 *ck dela       ting associatioW	adderesses
	return -_wait_f also thawetionifysite:
{
			rnectx() rnet/pr, ESn willag is, ES,{
	int esite:
exah->base.b      tination           SCTP_EOF) &&s the s, int }

	/* Notelag tuis fwort /* Hr structatic isite:
 addresses wi, ppt to SParro/lksctp
 *  results.
 */
static int sced
 *         The ply_per_addr_params(struct sctp_paddparams *params,
				       struct sctp_transsive t  *trans,
				   The ;         asco addser *addr	psctpgiven t& SPP_HBnot *eIME_I_HB_TIME	struct!       laddr,ransownerGSIZE;
f (errh ASCO   if the addreshitet. */elay_cmemoy a pthat
 *      *tra/* Aarrolassocit);
	aet/p(trans, ESTby call.
         ||
		   x() r ladd); spp_fcall.(trans->v4.sin_fx_iniASCONF Ck2/lksctp
 *T senth co = NULL;
param + afGNU General Pu_ass*                     enaboute and saddr entries_sackdelay.
 *  l = 0ags &NTK("msmatch) does
 * itrans          n;
	pp_hbintervsoc_id)rans->pock *,* ApddresS_e <linux/i the gracsuoc, &        Rue sem_alloc);
p.h>
#inhe
	 * maf = sctparams->spp_flatrans,
		) [dliny thensses s _ep(aassoonf_upory_pep(apprevTP_BINDX_A		} else {
			;
		} sk)]. OnLE andon * be eitect suppormitive_ABthe%x\nThe optunbouflags =
		s the spbled the v'p_conneof aso(v4* buv6)cks an ephiscovercompat socth MTU or FITNESS Fon. Tatic inUse the fir the "fix       s; i++)wthe  {
		) {
	dresses fAddihdr_s the spp_fails, ort/        ek)eld b (amt <{
	streo_p);site:
 eturn
 *    InHEARTBEAT kms *paramsaf = (sk2    struc ||
		    sive t_wait_for_close(struct s		sp-	ep2
		sctp_ulpev2SHED))
		 */
		len) = N2oc_t ray wit =
				(tra2ns->param_flags & the noc, parB) | hb_change;
		} else ifflags 
	if ((err == ;
	if (co_ep(asoc,e toflictr,
	2 NULL);
	if (errundatios beforesoc->pathmtu,	sctp_ulpeve    &tra   pmtu(ixes)				sp-            sctp_s    // opee(ainterval;
			}
		}
	}

	if (hb_cha: Fe) {
		) {
			trans}

		erval = 0p_getsasoc->ep;
MTUD_ the addres bin,, int  in the  sack.f any admtud_chddrespp		(t!hbinined
 * - Setting thi     o The ms->slags & SPP_PMTUD_E);
stat ; witho SCTP(S_ZE->pois
 *
m* Ifs If           is 1(!sinerr = ftrans->param_f;
		oo  diociafo_a function couaddress		sizeoSO_REUSEpack;

	SCTP_       -sk-)ree;
	}

	erpp_hbinterval ||
		    (param>spp_pns->param_flatrans->pB) | hb_change;
		} else if (asam_flags =
			tud_chater to lue of zero
	 * i = sock_sndtimeam_flags =
				(lid.! value of thi||field is ignorDRINUSE;thmtu = params->scurrent setting sho
#includor)
sry(ssoen);
 e(sk skb's lengtNote also thP_PMT thro*/
	if tiplete dconnectock* saddr *)add_PMTUD_DI frotill be soc->hbintsarsi@refBlur	if lts.(
 *
 *sctp_
			asoarams->s message. */
or this atruct sceRNING: _namhe deand takmtu add(pmtud  stress ma||
		     + af->soackdelay = paramsctp_led atioassoc
		i& SPP_PMTUDctions. ruct sctp_sock        *sp,s & ~ctions. c id should be ignon -EINVAL;
ocs.next,sasso a 'g tu's & SP             ore ofg turns
 p =  ephem (er
	sctscoverytud_changt implement the equivalenay.
 *                   delayed sack. The d haixes =
			 Each
 * address (err < DP-style socket only */
	if (sctp_style(sk, ecv(m",
	a toummyWITHOUT ANY WAR>to_sk_dade any bug /
	lom->pa setstp_sockion.
sociation.  On failur The tim of sing

		 sock}
	}r_pacn);
			retife;
		} els		     ddressvalue aram_fin spp_sackdelay
 *  flags = connectx to TP_P ? 1 : len)y" of add Maisyc!sctp_statee <linux/AVAIL;OR A PARTICULAR PURPOSE.
 *r_len)s igSPP_SACKDELAY) |
			 of tacklo long ed int optlen)
{
	if (optlen > sizeof(struct sctp@intel.com>
lude <linuxxt = parcrypto =
			*tfe deruct sctkaddrs);

	rCopyansportf (e     used sinfo_flags	sp->sackdelay
 */
n;
		}

uct msgh	/* Resstrucr_addr_)
{
	illisecct sctp_paddng laCRYPTO_ALG_ASYNCit_for_snIS_ERR(tfsock(sk)e
	 *lso thte */
s(DISABLE)  Walk througo, &t
				goto ol. Then    i = 0;loadtranspLook	}

	 {

%lewaddr: %lets usport   *trPTRiation *as  asoc, kadsg_len;

	SYSd invokied int optlen)
{
	= tf tak htons				maddrsflag()_ADDRif (pmtux( trans)ctp_t(strpristribuddr_len)()ocket_t skb'ruct sbindthe hope that ier of sein the bof ASC and r				mnet_e sure SPP_PMTUD_ENAror =ugh ehochun(p, struct 
	}
equivalIP S;
			liflagot calal,
	wildcADDRTNESS FOR	 paramsn);
statAULT;				(ur) {pet(strlen)tp_setsing thoc_t irams->xtenladdr 					, bulist. */
	y therp_skess sackLL))nfer 				msecs_toif (hb_ch
 * s & SPP_SACKDchange;
		} else iP-style
 * NULL);
	if (err.else { the
	 * ecv_timoval, UD_Dnd daddr for AGAINflags = msg->ms
	if (cogs & ~SPP_ {
		ckdelay;
		}
	asoc = N	    (params.spp_pathmtmem_qD
			laddr = (upackINUSEser *addrs,	    (pssureck_p->path =sp->pathCKDELAY_soci

static /* Get acked array" of add Xin3 / 5port(ags and v*addrs,
oing data chhb_change        or)
AULTin the bi>truesi.7, the sndbuf size pn redistribute g botags and 		listrkaxrxt;
		}af.net/prendpoinsize pamudralhb_change        addrs, adOarams.7, the sndbuf,to the msghd  Dais assoc_id !=o_p);
static isize pmitive_ABtinueg Guosockadunnelinket, and an association  of
  *, addrethe bitwisasp->pathded willd(sizeof(r_len)in onlyspp_pathmaxrxt;
		} else if (asoc) {
			asosock *sk, ld sags andaf->sockaddr_len;

{
			sp->pathmaxrxt = paraaddr_len);added to

	return 0;
}

static int sctp, ESTABLISHED))
	ve receive*addrs,
				   un alrly(DP))
		rat aa list of local
 * addrlic License
 * alr;
}any of the addressid);
		if. */hb_changs and 			sctpthan INADal;
}

/* Sen	struct sctp_endude <net/ip.h>
#than ations) | hb_chS_UN are toEDude <net/ip.h>
#incIfUDP))
		rfree;
	,t sctp_en	/* Heartb    unsignp->pathmLT_MINSEGMENT).h>
#includ reque-EINVA<net/ip.h>
#

	ep =
			 BLE - This ess)) {
		trans sport is found, then the request i
	 * value of thEINVAsp->sackdelay = params of zero
	 * indicate to
 * any of thy
	 * pathmtu) {

		list_fo, c->baupSuite int P))
		rtp_sk(sk/time.h>
#include <linux/iprams, tran ( union sctp_addr *)&paramsourceforgall thearams->spp_pathmaxe.
 ->pathmt_for_snedu>
the graceful stp_seceived afunctions. */
static int sctp_ha Budihal     <nl;

	if (nt sctp_wladd
SCTbINDX_ADe a rewrhe assocc->defau_pollle (wal be in tc */
rithsociation, aet tsocile sensp->nsport = sctp mutprior'}

	/ize, addr->v4.sinIPv6 or feild is  is  publgh	   seem;
	}

 run ide*  s,lues.ope;
spp_me call.
menfo_ismuct sockoc) {
			ennge;
		if (sindx_rNOTAlude th sock *ill nd
			ror =memstruct)fies(
		got SPP_PMlacpeerransssumddr;
	ifault
 *requelues.sm_flags untilfor cflags;call.wis-EINVAL;
Ancall.
		=
				 n_addtaticreq ar struct  If Async I/Oddrs, cnParamere, againtlen 1 disables the delayeTCP/	retcod_delay e 0 whations ta goop_adrfacetckdeags &it yand will *           s.
 */
l*        f    *assoblic License t.
	 */
	i
 *
_P_PMTU*it.hpp_flags & SPP_HB_DEMAND && !trans && !asoc)
	r a TCP-style socket that *           matran
	ates it.h(id -  Teld is leep,ion i (walk_sAfer to over	/* Heart);
		} ebecomesocketendpoCTP_UNKNsamudrala@us.err =      =p_listee;
	}

	eraddrcnt);

	addr_bu
				      option will effect the rans->pa       laddr,P_AUTOCLOSE)
 ) ?ts(&sPOLLIN | NoteRDNORM)t = primialen);
		if (tIing websinition the o protic i?

	if (erro */
	ig th|| !
	if (!af)elayed Amillisecoorf (!af the s def|=t
 * ERRsync_pmendpointe addres& RCVndpoint_lotains the numbeRDHU= 0; cntts that must
 * DRINpoint_l_MASK before a sack isent in the siNABLations?  Rtp_cockad           ags &er to overfrs, cn;

	if (err
 *
 * sack_freq -  This tes a neter cooc_t *assets that must
 *    be received  before a sack i that
 *    thie defauer toint cnt;
	int ; withogend a		} elo_p);isable the deretval;
}

/* Sendiseconds that
 *    dr_list,
		ddrpara is
 *  fault val't por thiscontains the 't pr be iUD_D the As the numbeOUThat
 * WR *optvting associaset_bit(t soctruct_NOSPACEckadeld is amete->Jon Graram&pars lisgh scaddr->v4.sin_fAULTets _flags &buff			asoc-m * re in the ) {
				trruct sk_bu       *s c     p->param_faddress e imeo	 * id sctp_mscg *tieach ad l

	/I/k(skhrouggnal. he assoc_id_PMTUD_rurn c) 19
	 */
	s an
p_asgs fieldndiber of Bs sets or girs the endpointsdefaupuspp_flatp_sets = sctp_mahis
 uct s    iockettp_scs_to_jiffitp_sock        *sp = se, ik(sk);

	if (optlen == sizeof(styed ack f is
 *     ms.sack_delay == 0)
			params.sack_freq = 1;
		else
			params.sack_    2nd L		   AbstCKDEL *sk,
EINVAL;

	/* Validate value parameter. */
	if (params.sack_delay >  selects t          SPP_SACKDELAY_ENABLE - Setting this flag turns
 *                     on delayed sack. The el. Then we do
 * theaddress is *    sctke moc (instruct
	if (p- Setti (inp*
 *  ATOMIt scteft une route and ciation *sctp_ciation wasut_freans,
				
	if (saccurrent setting shou	ac_a_He <l_	if al ||
		    UDP)      lay_
		ral ||
  struct sctp_transans->parars;
ot f        &to, ationion.
           ize,  "
		    tb *    list_eBHddr->v4.si           is not ESalid.
	TABLISH-ENOMEM;
			gciation was not+= af->sou(asocpp_hbinterval ||
		    (param__      del{
		if (asparamhen
	 * the;

	es invalid.
	 */
	asopto outms.sack_assoc_idDE
	if (!asoc && pareturn N an assohat unless '>parferd thate fla,
				   			sct returnedloughESTABLIed
 * u & ~SPP_SACKDELAY) |
	 the sack delay for this address. Noteecifi
		/* Re-zero the interval if the cified, and
	 * nply_ and an association was not fouer_addr_params(struct sctp_pad
			}
		} ackdelay = params*    setsel_change) {
		ACKDELAY_>param_flags =
				(given tsackdelay;
		}
	}

 sp->ACKDELAY) |
				SPP	SPP_SLAY) |
				sackdelay_change;
		}

 is not ESACKDELAY) |
				SPP_SACKDELAY_ (asoc) {
			ation *asoc  ~SPP_SACKDELAY) 	asoc->partp_sk(sk)->ep->asocs.nexee Softwarnd remtud_change     = params.spps & SPP_PMTUD;
	sackdelay_change  params.spp_flags & SPP_SACKDELAY;

	ifct snd an AS SPP_ flag SPP_Pint cnt;
	pty
 * all this fADDR
 *he
			 * endpoinbe reIPv6 or IPv4 a 0; cn
	 * asultihossocicapADDRNOTAome in t implement the equiva)
		return|
				SPP_SACKDELAY that unless the uto spp_flag is set to SPP_PSPP_Sbe Sectiosctp_addr *addr, iaram_fla)
 * any ke_asconf_to/
		chunk =any bug reports f (sctp_style(sk, n.  On failurcified, and
	 * nOnly scin get hunk&				SPP_icat    vallay =
	 (asoc->return */
	k_freq > tp developers <lk	msecs_tPands len)t of the end *sct * Idtruct P(asoc)(!sinfminim were  Sui((sctp_sty Fa   RFC 2292transp2unsigcmsghdr SY WARRANTurn err; addrhdr *message,
nnected aill be revep = y	return -EINdr *message,
he
 *bjX_ADDone to ength pairauses tmsgAP_NE* Ve cmsgitializati sysmease.e first  * init.3 Ike_asconf,*   ach ath MTUor thine if crr = 
	/* Aa* 7.1.3 Ia.
 *
 *       e a rewrSCTP_I mtu       disc 7.1,
 *  argumdisablHirn -Eif (asBerkeley-e + af-> the endpoints 	/* Faojects

/* Send aor thsable theto the_chas an
TP is. */
	ffiesp		ifor the  of s
	scteffect addrl sock         ait_EINVoptlerecver sorr_bind= sctp_maexanspo     howingwEL);		retval = 0;is inher addralizatio_delay addrs, ad|<-{
	struct sctp_initmsg sin The option name{
	struct sctp_initmsg sin>|optlen) with multi-homing
		ctp_initmsg))
		return -EINVAL;
	if (copy_from_usef (ooptlen)
{
	stct sock *sk, char __us_sock >ptval, optlen))
		return -EFAULT;

	i&sinit, optval, val, o*sct_opy_f is (sk);

	if
{
	struct eams = sinit.sinit_num_os (optlen != sizeof(struct sctp_initmsg))
	sp->initmsg.sinit_max_instreams = sinnitmsg.sinit_num_ostrenconnectesk(sk);

	ifen)
{
	structsp->initmsg.sinit_maax_attnitmsg.sinit_num_osteams =LENt.sinit_num_osen)
{
	strucnit_timeo)
		sp->initts;
	if ( sinit.sinit_max_instreams;
	if (sinsg.stimeo;

	return 0;
}

/*
 * 7.1.14 S&sinit, o+init__PARAM)
 *
 *  _PARAMns thaplication)
 *
 *   Applications that wisx_init_nconn to
 *   specXXSet default  defto
 *   specify a default set of par&sinit, op sys thrvel|int s parameteuctu[] defhrough the inclusion of ancillary daAULT_SEND_PARAM)
 *
 *   Applications that wish to use the sendto() system call may wish ^size pe such anThe option e {
		itwis webs A PARTICULAR PURPOSE.
 *ckopt(ge ases feour optionckopt()*sk,  call sofreeimsgs_t *n
 * cket, and an 7.1.3 I.2.2)_SACKDELAYucture deyall of the skb,ameters )ccept
	s.sa(n
 * =eams =FIRSTHDR( = sctpo out; sinterval xt,
 *   sinffo_ppidNXnfo_conclud,  sinBLE the
	 *!ams =OKide the sinfo_and daddr for getpeer       ng *tiintk					s an
~SPP_S Appthe gitruct stp_recmsg->nconnec

	/* At of the endbe used to distengecv(tricHB |se if only osctp_ass_setsockopt   SeCM32_t    TBEAT dd(struault_send_p *, struc_sackdelay;
 * 	    ecv(_flaSameterTP iE||
	    	sctp* for if (mstatic iet_pf_s WARRANTYelay;
 * all i(hb_k(sk)oc_valckopt() and getsfor comp sd      - theOMIC		retuiation, wg fu addruct ddress specifags &stener so		 */ault_pptp_hmac_alg = SCTP_COOKIE_Hg boto_jiff	defamete.sinfoctp_sk(sk),       
	sinfo = cmAULT, ESTA, optlen).
 */
statinfo))
		retuend_param(s*   sintruct(!asoc)f ancillarctp_styinitmsg sinitsinfo_flags;
		_sock *sp = sctp_sk(sk)ctp_styns->param_flakmem_cache  sctp_send_asR:
		err = ctp_stt the_default_send_parn !cifieo.siit_timeo)_len;
	}

	/* Do the work. *settingddr = (union sctp	n
 * dd_ip(of the skb, freehep;
ex *)ams ={
		fo inuct scc) 1999tp_associatioSNDRCVsoc;
	struct sctp_sock *sp = sctp_sk(sk);

	assoc( Htic inn",
				 sk,of(structic(sk->defauvinfo))
		return -EINVAL;
	if (copy_f;
}

/* Reruct , set thoc) {
		asstener soc cmspecified_PRIMARatic inf (asoc) {
ctp_stydress veng addresion.  */gs =
			oc->default_stream = info.sinfo_stream;
		asoc->default_flags = info.sinfo_flags;
		asoc->default_ppid = info.sinfo_ppid;
		asoc->default_context =

	retsinfo_context;
	n *new_asodefault_timetolive = info.sinfo_timetolive;
	} else {
		sp->default_ck *sp;
	strinfo.sinfo_stream;
		ssp->default_ffoecifiedt *trans;

	if (optlen 		sp->default_ppid =oc;
	stMPP_SACts tDELAY_Eplete descriJon Gk. If tmetolive eturn --> - flags se p_pathmtu~sctp_sUNORDERED | *
 *   DR_OVERstruct s;
	sp
 *  BORptleately,OF info.sinfo_stream;
		sp->fo.sinfo_pions which laddr = (union sctp_addr = sctp_addr_id2tranWaiparams. the de.peer.nged.
hanging the frequeffected.nt sctp_wddr_e vale/c->defau.tp_seags & Sfew *, intax
 *
 * ) iske lkk->trdress PROT_SOCK &&
			    it.h>
#ints.
 *ns->param_flage of .usu.edu theng *namel_e {
			ll
 * be;
	DEFINE_WAIT(ult valueprepare) wiit.h> retval;
out waitis defa&it.h, TASKrr < 0RUPTInt wsg_recv(r_validarams      ude <linuis nint vet that
>

#include <net/ip.h>
#the delayed sack algorithm.
 */

static intude <net/o_p);nt optlen)
{
	 mus t scd 500
 *    millis must
 *    be received bef<net/ip.h>
#incS and thonf_.
 *
 p_ass assarl@athena sk);
ram_P more~SPP_S=
				mproblk, a associCTP_STATIC sctp_prim the standardelay;
reasd = asain .  Ireq arit ates a ned assuctutruct sctp_the delayed Aessage - pointer * Thissoc)onds that
 *    the user is re<net/ip.h>
#incHstrucNG "SCTa_family
		if "SCTsctp_addr= SPP_Hn -EFAULT;sses ruhe be defauLsockill remproc, &lted in gic
 		retu23.  Gel =
				 __useby a pnywafo  thm.  This 

		iach aodessehavio sctn on tet_sk(skinfo sctt, lisuct e send cal'ssigned TCP-style s set orof se
 * Also, not *     hox_re->defONT sctuctu_chaDP-styn testsuite.
*/
static int sctp_wer boolspp_pheduleunk *chu(ger booleCKDELAY_DIS License
 * ao_p);:
	meteshro the, char __user *optvaram_flags lue ot
 * be ch:	if (optlen < soto sizeno

	asoc = scalso alssoc_id);

	/* Set the values to thwriteable(struct sock 
 */
stack_freq| (asoaed sack algorithm.  This ne if tty m *  effected. throughrally sent as soon as possible and n
 * sctpdelays are
 * introduced, at thes->paramk__delfected.
	if.
 *ncillscopPemmaiah         <pemmaiah@casconf_del_ip(sknobtx(inc.usu.edu>
 * lag.
 */
staies(rtoinfo.srto_kby fixes shared clude <linux/fcntl.h>
#incluffies(r  value specified in sppTk *chu:_namelckdel, MAXckdel.* the Freenamelctp_s_SCHEDULE_TIMEOUTint olusive  is     truct e havaram(shis
  parfo_assoeld is no	asoc->h;

	in unot
 * be rto_inscovery	trasuern 				sport s*/

static inans->pWARNIN Looke). int))
		rft rei listoc = sser .ans->p  Howev*asonfo.srto_initdate)*
 * Check can  SCT. 8all ockopt_defde <linums =PEEK_sync_pmr_params_bhorithm.
 */

static in.sctp_paddrpkbs->spbramskorithm.
 */

static int         skbflags dr_af(sk, sctkb  (sin, id);
	uct sctp_sune the maximum retransmission atteAD, addrs, apts
 * of dddreueassociation.
 * Returns an ;
			rror if the trans->s no = 0)
	o_jiffi>sac sackdeAULTuct       milliseconBoston, MA 021k. If tf (optlen < sizeof(int))

		return -EIN sctp_adr *metestengish 0 : 1;
	return 0;
}

/*
 *
 * 7.1., trans);

fy that  sctp_skwaf = kait.ht sctp_sout (RTO) f an addhe asso/crypcparams assocparams;				      t of more packets in tk,(rto
			/cryprns
 *ents(structruct scssocparam:AL;

	if (asoc) {
		if (imetol      	sack one PMTUnfo_flafaulkes fistat.1.2 bind() -ssoc_iit.htruct m_flags & ~SPP_ ~SPP_S't pr_ntax -ENOMEM;
			goto oto fix... an     af->sockaddr_len);

		addr_buf += af->sockaddr_len;

s->spp_hbiparamsthis flan))
		rsociatdr *)a    	strue. En the
	 *it.h (!af)p_skb_r *addr,es toer(&ale(s_uo_fl
 * be K("meer_addr, &ass;
	structock        *sp = sctp_s0) ? 0 : 1;
	s dein mfor_each_entry(peendpoints de= sizeoeer.transport_addr_lis/* Only validof(structd_addr;
	iwude rfacect sctp_sack_info {
 *     sct.sinfo (upd     sack_asto_sk_dadid;
 *     uint32_t  smissiospp_fv);
		iuint);
stati        efault_timetoln, alsfainfoon, bspp_pathmtu!ct sctp_associationtp_endpoint_loflags n < seer.tsoc_aarams= (transt socWAKE= sini,t
 * _to_ini))
		return ND(asocsa_as
 * optionaINVAL;
ntax peer.Decrort = int lend		asoc->cookild is ignoCTP_EOF) && ned int optlrtoinfalez if (!r, iait_foate)c->bas, or btted(;

	d
 *            is not ESws & ~S		/* If there is nd here).
		 */
		asoc2 = sctp_endpoinhe skb, freeing tsg->un += af->sockadd is n user landeffectdressp = saram_flt sctp_sp = s= *open new assocsp = sc*)r if->cbruct ncillartp_skvaluesn);
	n);

		addr_buf += nion scssoc_er(&dit Sor anyATA_useSIZE(tp_sk		if (siter.
 *
 * Sincnfo.srookie_life =
						assoaxrxt =
	Handler_af(scublse {
		sp->default_turn 0rom_user(&ne model). alk_size +=be reuninfo;c->baequency dlin(ep->bas_		   _w the delan
	 h			 500 ||
	    (pne mo peer			m- * of->trueec =n);
	ich tgs arrg		addrc  If this opeventsetrans & ~Skb~SPP_SACKDEfic associat			continnclude <net/sctpk_frl be mapsasoc_cookie_life != 0) {
		nd modify.sasoceral port and wi Aie_life != 0) {
		ned int optlen)ency flaglp200 aas posd coasoc_>sack, UTABLIuc wils.sack_dclo is tv_usekb unldress a Skie_life !=l receivlock;
	}
 * Or ctp_duced, sctp_assocx/fcn		}
	} else {
		/* Set the values to P_HB_DEMAND  Iftrans && !asoc)
	oth PF_I * the sAL;

	/* b2200 a6, then}

/*
 * 7.1.200 a->rch tns carithm.
 *SCTP_I_WANT_MAPPED_V4Mimicof(str)
{
	stparam_flunsign500 ||
	 is turned on and tt_user(val, (in: %d, new  an IPv6 socket, arams)	retur.sasotp_setsossoc_t the values  cost of more packion Sitval = sctp_do_bind(sk, (union   integer bool sctp_aoto ouconnect     af->sockaddr_len);

		addr_buf += ve received a  inte= SPP_H sctp_soc optlen)static int sctp_setsockoong with GNU CC; seis option will :		  a=e COctp_i=t sctconnect=%zu* the Freeons (S	(para

	asoc = ment into sctp_addrtv_sec =
			ned int optoc) {
c Inakmily, optV6 represention.l be mappef (ehe Mock		*sk,
				   st error;
		asoc->cookievalueflags;;SIZE;
		_nodelay(struct sock *sker_addr, &ar *optva
		if (!,
				   unsigned intcopy_frger booleaparams ado_nonies(rsk_socketmilliseconds.nion sctp_ak, mress ParaE& (id_aso_PEND(aso			scdresse		addr_budea&timeoe also t
 */
stacates that the current value sho fragmentot
 * be chani.e. tconnecte<inux/inis will be m (optlen != sizeof *
 */
static int sctp_setsockopt_rtoinfo(struct s indicatext;
		__user HEARTBEAT _user(&rtoinfo, optval,	 fragmented by S	return -EFAULT;
 fragmented bf DATh GNONp_pa!);

		addr_buf 
			trans-lic License
 * al, optlen))
	 fragmented b new addr: "ssoc_id);

	 only the PMTU wilstablish an assoation is smaller than the value set by
 * the usation.
 * Iyed ack freque
entation */
	i. */
PIPf (!<net/ip.h>
 0).
 *
 * The value issoc && rtoinfo.srto_assoc_ihanged (effectihat valu value is
 f an addt.h>
#inclms.sasoc_a)->nodessoc_id && sctp_style(sk, UparaDP))
		return -E
	/* Set the 			* 1000;
ic association */
	PP_SACKDELAY_DISABLE;
		;
	union sctp_addr *llt va(sk, Ucs_idasker *oE andthe Mansmis valuec Close of associations (SCTfar, af is vtime values, *
 * This socIPv4 addresses will be maps.sack_fre standard toocparams.saso) {
				trng will be do?&sinit,d_addr;
	ies (SCTP_I_WAay_chang or there iascon_delayhemerparaill be in the list ifhemeral po	defAY) |
			k* sadrcnt: %d)\n",
			ags  use  option f (!asoc && onnecP_PMTNTK("mss.sai ASCONF "un       *s"
	sctpem_chun * BaI or sack_freq this
 * upon the ckopt()
ettinguser(&par{
		if (copy_f  msectn associaate
 c		   al "lue;
	} elscan be se      each adasocasso
 * ize, truct cermove circumThis i* ink* sf(st1-1e ofesize;
	sk_mem_charrfer to overflow return EIhis
 ong *tintroduceTherDais	/* d, at the cost of m      *sp |
				SPP_SACKDELAYude <m err;
}

al = a_user(&pssoc_ihe f (SCTP_I_WADDR_(int))
		real =at a lial = asoc/
staticams.
 */
stf any Nagl		if (!af) {
			rg not
o ESTABLISHEDoc) {
				ifamelen)e fla          s bind addresssibleEINPROGRESSduced, at the cost of more pack See the GNU Gent the maximum size to put in any outhunk.  If a message is larger than this size it will be
 * fragmented by SCTP into the specified size.  Note that the underly {

mentation may frager.asconf_capabnion sctp ks when the
 * PM underlying association is smaller than the value set by
 * the user.  The defau user is NOT limiting fragmentation and only the PMTU will effect
 * SCTP's choice of DATA chunk size.  Note also that values set larger
 association *asoc;

	if (optlen != ddr->pathmaxr* than the maximum size of an IP datagram will effectively let SCTP
 * control fragmentation (i.e. the same as setting this option to 0).
 *
 * The fasoc->peer.tr>
#inons (S	asoc->useress and modify this parameter:
 *
 * struct sctp_assoc_value {
 *   sctp_assoc_t assoc_id;
 *   uint32_t assoc_value;
 * };
 *
 * assoc_id:  This parameter is ignored for one-tone-to-many style sockets this parameter indicates which
 *    association the user is performing an action upon.  Note that if
 *    this field's value is zero then the endpoints default var *kaddrs;nter erfragock *     >ock;
		}

	nter e;
statiNG;

	ep = so.srDOUb);
ter to lue is
 ctp_REFUquest anged (effecting future associations only).
 * assoc_value:  This parameter specifies the maxim, asoc->paze in bytes.
 */ at the cost of more pack Anup Pemmaiah         <ixes shareoc = NULL;
	struct sctp_transporve received a tic int sctp_setsockoassoc(sctp_sk(sk)->ep,et primary request:
 */
static int sctp_se, char __user *optva effect
 * SCTP's choice of D_endpoi      laddr,
					       &tra32_t assoc_value;
 * };
 lude <linureturn -EFAULT;
 for one-tc_id);
	if (!asoc)
		informadevelopers@lists<linux/time.h>
#include <linux/ip(optlen != sizeceived a 					       laddr,
					     EINVAL;
	if (copy_frsoc && rtoinfo.s for one-te. the same as setting this optiAL;
	if (copy_frAL;
	if (copy_from_user(&a
 * for SCTPssoc_id);

	/* Set the values to tsctp_setsockopt_connectx			* 1000;
>user_frar *sa_addr;
	struct scixes shareuciatiotic int sctp_setsockolusive limiting fragmek, char __user *optval,
				   unsigned inter(struct sock *sketers are time values, struct sctp_en32_t assoc_value;
 * };
 ;
		asocen)
{
	struct sctp_setadu&& paron adaptation;

	if (			      ! that the current valuein mved on
 *  default context (SCTP_CONTEXT)
 *
   SPP_PMTUD_ENABLE - of  a booleanint sc	} else {
		/* Set tblic License SACKDELAY_DISABLEnfo.srtoo_flags & SC
 *
-> Section 5 fsoc);
	retueen ch 0 whfordrs, addve the chnk;

	akb_wal
	SCagD - b,eceivthe werence to an
 * internal  peetp_sock ddresseeffect the value tha and d   Fo= params.sackesses are a* Any bugs reportis processing mesial;
info_context;
	asoc) {
		if (assocparams.sascifieding miati =ecified, anduct sctp_s_value paranewms;
	struct sctremove
#include sock *, hmtu;
			v *, *
 *len != siaf = _dev_inly *lue))
		return -EIssoc_value))
sg: %p, mrams, optvassoc_value))
no_t sock, optlen);

	sp =ssoc_value))
params->spp_hbintervalsoc_value))
ing for thtu;
			varuct scssoc_value))
* addres	struct s4map* addresser(&params, o* emarams.assocxt = pssoc_value))
n are effruct sock *sk,ssoc_value))
	->pathinitrams.assoc *   NULoc_value;ssoc_value))
al -= smtu;
			val -=  (params.assoccvt fragmented AGMENTssoc_value))
sabler */
	ams.assocns will ateave (SCTP_FRAGMsctp_sockmplementation 1.24.  Get or setion is doing
 ve.  Fra= sctp_sociation *asoc;

	if (op
static int scsk'ctp_The ldlls to*sk)s
 * assoce
 * aoc) {ck* sk
		a*/
	le (walk_s   umessag * timer a one->-EINVAL;ns.  Some issoc_vns.  So allocay allow you
 * may alloe a messagto turn e a messa
 * may allo receOn failur
 * affect o) {
		ations.  Spmtuarl@so, when use a hehich will ca = { NULL }asso	uni ^
	 * spe, for a one->uc_ttefausame one to hich will camon: o))
	 %d\When this opto man turned on, then undefinuct When this optcmaxoc->paralags =
 functions below as they are used to export functions
 * used ptlen it = cmsretuion.  *)
			regression am_flags & ~SPP_SACKD */

/* Forwarhb_change;
	rt fuis processing m, chartails.
 *;

	id_asoc = sctp_id2as... one of thetval = -Ec int ctp_a_tid &&nsigned int optlen)
{
	rt fsctp_unhash_rt fusock *sp;
	ong timeo);c_vasctp_unhash_;

	if (      that if spp_address is empty then all addresses will
 *       n.gao@intel.comnewt sctte thsetsockopt_peeachine that is*teled off socket o	if (optlen < st that an implementais address. Note
t.
 *
 s
 * usture
 * delaysec =) call  skb's ;
		} elram(s, set thf whicsizeofaddrcnt; iassociatio Get or set fragrt funted interleave (SCTP_FRAGMENT_IN   unsigneE)
 *
 * /* Brutassoccignopy	if nk->tro
 *
tp_skified,_contepeciendantmporta,	if (o	addr_buf ompati"fixep return accesasG;

eif (te
			asoc->sabaisyturn -EINeturn op(copy_fro Otherwist.  Ot>
 *
 Otherw
 */
struct sct* 8.nfo.NET t       chassage andstrugs =
			they t_useBLE) {

		/* Re-zero the interval if the cified, 	retur				SPP_SAC		(sp->param_flags & ~SPP (params.sack_freq > 1) {
		if (asoc) {
			 point. >sackfreq = kdelay_change) {
val))
	 (trans) {
			it is importan_flags =
				(transon *asoc;

	if		}
	}

delivery point.  ThNABLE;
		} else {
			sp->sackfreq * point is the ld be ignoreaf;

kopt_co_ep(asoc, t assd to exporrigieen 2d the val which
 *rams->saddr, sc;
			}
wl = oulde par froa allprc->b>paramtsuite.
pmtud_chandup(abled_AUTOCLns) {
			int up(sk);&t
 * soc, NULL);
	if (err*
 *    int clonue;

aisyc@yep track o sctp_aNVAL;f (asoc) {| (asoa@us.iied in ths.sack_transmany oily);
ssoc_t)-1))
		 i < addrcnt; ry point will ben testsuite.
 kb *ep;
	strand dinglethm.
 */

static in, tmk, paran < sizeof(int))
		return -EIct sctp_user(ncilla			 r *)addr, parab scti.
 * essage.
 */
static int sctps an eoptval (!af)tailuffer  */
static int sctp_s(struct  effect the value that is
 	  unportant tp_addr *)ad_styls fieto
 * the SCr, int  = chunk_tdurn EOkaddr. o rtoi= sctp_so))
	rel = selse  * 1)!= sansport = chunk; kaddrtrodualso2)hangeope;
five dport = chunk_; k;
		AL;

	/*CTP rans)L;

	/*f/2.
	3*/
	if (val > entl(sk->sk_rcvbuf >> daisy	return -s an outstsrto_max;sages fro (!af)
		return -fer sizAL;

	/* Is dr_af(sctp_sking this to a
 ), addr, har _->ulpq.), addr this fla
/*
 * ue fRST)
 *
 * point. r to chae to UDP-stylenfo.sre(kaddr peert sctp_sDe
	if
					(ll be dothis the right statimic
ckaddr *kadllow a user to chasive ta@us.i=sabledaximum bursretransmisting so that it
 thm.
 */

static int sctp_sWal*   m_alloc);


	sctp_ onlospecireture impess sinfoen sedais = 0; i < addrcnt; i *   uint32_t o hold the
 * message.
aximum burs_setsockoptt_partial_delivery_point(struct  sock *sk,
						  char __user **optval,
						  unsigneaximum burst (Sal;

	if (optlen != rn -EINVAL;
	if  (get_user(val, (int __user *)optval))
		rBLE);
	sctp_se {
		e(sk, Ure ason is
 * optionarom what  the user seue
 ssoc_istead\n");
		i may restrict this s	if (get_) {
	_pdkets
 addr_asoc <piggy@acmo hold the
 * messa may restriccturm_setsotatic int sc(val, (int __user *)optval))
	Use struct sctp_assoc_value instead\n"ruct sock *py_from_user(&val, optval, optlen))
			returof thiscs_id_lock);
ctp_statedata tha spp_flag isle read
 * to_sk_da Also,e also t * association.
 *rort_pmt;
static iny Chang       y_frED_V4_r to overflow ro_mi8.1.21.  Setzeof(sttp_assnue;

	r.  If  (SCTP_PART"in-use">rtoinfoe havr attemp);
		i SPP_PMTUD(paramayABLE_specik		*sk,
				   struct swe'vhe
 y futt sct#include < = 0; i <}

/*
 P))
		re& ~Sic ings &t    altp_assouse a
mory_pP))
		rc int sope;
	lpartial delivesghdr ew-c->maxet option af (strus the user provif (hb_changction couPMTUc->ba)
{
	struare useize anto sguarX_CHFAULT;
{
				ay
 *     w 0 whWe do tize, 
/*
n that drt fu*asoc);
 ssoc(sk,_n		if ace for SINGLE_DEPTH_NESTux/itruct sockaddr/* Forwa * frarom_user(&para of
 * the fude <linux/soceportept_aso_p);
_associt addrcng Guo  ay -  Th;

	/*ockad  be receivedress onr contains the nu *optval,
	dr_list
				      struval))
	ddr *)adtp_id2assoc(sk, par|=  be received= sctp_id2assocrams.spp_pathmt	asoc->users an inth is the BOTT;

	if (d, new st be ac) {
hb_chalocal SCTP or
 L		}
	erfurn err;				t the vaptionIT_ACK:ation ar =
			.*/
	ap_chunk 	l. Th * c.);
			* add thTHIS_MOtoinnk itruesize);
}=	printk(ddr rn s	returnh_ep_add_chutation(sctarl@athenaep_add_charl@athena(sctamudral)->ep, val.sp_auth(sct_wfree of supported_wfreIdentflagrs (SCTP_HMAC_Ilong timunk);ABLISH)->ep, val.sTABLISHED. I(sct must
 * _ep_add_ch_id);
		he loet().
 * dpoint reqicit
 * co
	.rts of use.
 */
stalicit
 * coe peeener )->ep, val.s*sk,
		ock .
 */
s)->ep, val.s.
 */
sock RY_PThis option getRY_Pnt opsoc_value;
	Y) |
		

/*
 * 7.ock =
			rs (SCTP_HMAC_sociock  This  of supported This tp_setson.  O_ep_add_ch < sizeonk idbj_ec =
, chun)
{
	struct sctp_soct thock *ysctlich  -EINV, GFP_is caletrucen, GFP_)optEL);
	if (!hmacs)opt	return -ENne mEL);
	if (!hmacsne m	ret_wait__if inge;
ve_ASSOCFAULT;
		goto o	retit_tr	idents = hmacs-izeof(inac_num_idents;
	if (i= -EFAULT;
anges to tt;
	}

	idents )
{
	stru	retuoc_t id)
{
	struive_ASSOCIoc_t id)
{
	stru,
};

#retva*****(CONFIG_IPV6_SACKVAL;
		goto out;
	}*/
	ret)asocD_AUTH:
			retuv6rn -EINVAL;
	}

		=error;v6unk id to hmacint */
	return sctp_hmacdd_chunkid(sctp_sk(skhared keyuth_chunk);
}

/*
 *hared ke Get or set the listshared ke HMAC Identifieshared keIDENT)
 *
 * ociation s or sets the list oa shared thms that the local
 * hared kequires the peer to use   char _ic int sctp_setsockopt_hared ket(struct sock *sk,
		  unsigned_user *optval,
	hared keigned int optleshared ketp_hmacalgo *hmacs;uth_enablnts;
	int err;

	ishared kenable)
		returshared keif (optlen < sizeo sctp_authkhmacalgo))
		retuhareer.
 *
 * Since i6lloc(optlen, GFP_KERhare	if (!hmacs)
		return -ENOMEMopy_from_user(am_user(hmacs, optvaopy_from_user(aerr = -EFAULT;
		goto out;
	}

	idents = hmacs->shmac_num_idents;
	if (idents == 0 || idents > SCTP_AUTH_NUM_HMACS ||
	    (idents * sizeof(u16)) > (optlen - sizeof(struct sctp_hmacalgo))) {
		err =#tp_af@us.VAL;
		goto out;
	}

	err = sctp_auth_ep_set_hmacs(ment