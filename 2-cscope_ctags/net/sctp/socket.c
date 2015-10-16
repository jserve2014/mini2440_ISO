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
		break;/*P ker kernAUTH_CHUNKentati04
 * (C) Copyright IBMauth_chunk001* Cop4t (c99-2001 Mo(c) 1999-2000 CiscoHMAC_IDENmCopyright (c) 1999-2001 Motorhmac_iden2 * Copyright (c) 2001-2003 Intel Corp.
 * , IncKEY(c) 2001-2003 Intel Co1 Motorola, key(c) 2001 La Monte H.P. Yarroll
 *
 * This file ACTIVE is part of thTP kerCiscoel implemctiveyright (t (cThese funcrighs interface withementightDELETayer toP Extenentement (c the
Extedel for the Sockets API.
 *
 * Note thadefaultCopyright (c)-ENOPROTOOPT1-2003 Intel}

	 1999release_01 M(sk);

out_nounlock:
yrigurn right ;
}

/* API 3.1.6 connect() - UDP Style Syntaxthe SocAn applicyrigh may useemenCopyrigh iscall inor moree modelthe snitiate anu caassociibutt the out sending data. yoPublTscrie;t (cis: Free Sret =odify it unt sd,Copyst struct 01 Maddr *nam,n)
 *len_t 2001 MFree Ssd: term01 Met descriptorhe Ghave a newlic License aadded toe Fs ofSnamP Exte anyessr operface (eitherANY WAR versITHO_in oublisevePubl  nement Extie6 definedmentRFC2553 [7]) be useflenP Extenize ofbut WITHOUT  be /
CiscoSTATIC 2, or1999dify it ut even the ** Cout even the imp lITHO,
			    URPOS*****2001
{
	RPOSthou= 0;
	ut even 1999af *af;the
whicets e BOTTOMmple	CiscoDEBUG_PRINTK("%s - sk: %pr versITHOile COYou shou: %d\n"etails.__PI.
__, * Core de , write l* al/* Validneral write  beforender by
 commonCopyrigh/opyrighx routine. A P	af(c) 1999get_af_specific( 330->sa_family1 Moif (!af ||ee to,the < af->    *****2001 {
		eceive-EINVALor S elseC) C-/* Pass correc*n, 59 MERribu1-1307,twarnd  (so it knows terre
		Found only onFITNESS FAbe0211passed.site:/tp-develo__E. are e/or m oundyrigh,p 
 * WrpYING<l, NULL1 Mo the
 Pubh
 * ifiecense
 * te that.thouThis SCFIXME: Writmodimficasor mbARTICULAR P*
 * ten od  <nmodifieGNU Geuite  PublRPOSflagsld haha Budi-EOPNOTSUPP; 9-20TUBctp
   <na4.1.4 accept un fTCcripftwarre FouFree Sndistrense sand/ong Guo    MA tto remod inn establishede
 * >
 *hope that itfromor FIT ChangqueuITY or FIendpoint.  A>
 *  (c) 2Publit un .com>will be; wiur****la   y Chang  e Fa<present termthe yPublformedidhar SamudrORny bl Knutson   chicago.il.us>na.ch Changchicago.il.us>the    Jon G <pemm*errrimm copymplement.il.us>p a    Kevin Gao  <pemmI *ele Fan	kevi.il.usnewsk =Yarro     <kevin.gaoGope that it*asoc;
	lsy Ctimeol ade receovelod al.us>
 *hope that the ilsp(c) 1999- NarasimeP relp->   <p anyes 1999- <xiied bTCP)keforgdevbe ine Fan	jgrimm
		goto outl <piggypes.h>
#instatde <liLISTENING/ * SCTx/time.clurs@listsitinclude <linclshare(c) hopercvude <e <liaiah@c& O_NONBLOCKt WIPinclude  <pemwait_for  Anup Pek, .h>
#id widrex/tim)apabilityx/ti PlacWe treardellex/caibm. trythe fix<pem.com   <pemmI aby
 w   Arde
te:
a@us.iand pick termfirse <npe that itut
#incp.h>.<linubugsoc =cket._entry(ep->r sas.nextd into thenzalelude <net/, #incl#incleported nclupf->c <neenclude een o//C) C.pto.h>

scludep.h>
#include <lNOMEMcde <net/ip.h>ude <e.neopuluite-1307.eldsibm.com>
lude 
 *    <soldsknet/imigr
 *
 * ude <ude _tohey are ush>@us.fiah@aelle _nsthe uG: include Pleas,e
 * SSOCKET_nux/plemen:ggy@acm.org   <pemm */

#i.edu =the besimha Budire us     <na <xiCisc ioctl handler  Karl Knutsthe 
static <k_wfrePemmaiah
static inpemmcmd, unsigcom>ixes arinux/;
static iNOIOCTLCMD
h>
#ic voi* For-1307uncat itsacm.oglic cketed dur0211 (c) 20sm.h>se atree Sicago.liz wiloid(C) -suo@inma porncludnm.com>ux/f be S <xinite NY WARRANT Tempd alreadyez@izero-fit(strmemoryLayer	    <rmlayer       <kicago    Nahicago.il.us>
   <pemmKny bte.
@Notele.h>
#inu can y bte.
 */
o);
staixes the dhicaCC; se *01, ixes sharefIf n)* th et/s<piggyh is thING: 

#plemeI*share_p)t msg_lei ppublCopyriarea.us.isuiwitch (sk->sk_typekerne<piggyOCK_SEQPArnalCopynclu *,  =
#inNote nal UDx/wai03 Intel Corp. any TREAMint len);, or) Cobindx_rem(TCptionightint *
 * @acm.opopo);
stconfTNOinuxORprotKevin Gny l.h>
, ip(strucom>
  to ameters.en);
enf_del_ip(s ca* Boude <.orgfiedas puint len);_DEFAULT_SEND_PARAMnt);
staopyan Latindx_ap->p(struc_>

#im incor cocetails.
 ppatic ip Inc.
 *Inc.
)includtic int sctp_do_bicontexh     int sctp_do_biude tolivaticcorporunk *chunk)rc_autop_ any l,;
st);max_rp. 2lihelpin_ight_mig;
static vo int sc Copynd_astupock *, struoptionnf_add_isociude <conf_add_ipsctp*, int);
staticINITMSGatio
statc Lic orude <overridd * Byr *CTP_CCopyra CMSGfy it .. an,icagmsg.sockadnum_osociats  ratessociatioutctl_tic int schep;
exdx_r truct inint sysctem[3];
exte
statyutobnt sysctp_wa[3]mem[3];
eattempttp__memory_pressetranssockac int sctp_memory_pressurint);
star_cou@acm.todx_r sctp_sation *, stails.
  RTO re.
 *
 t sctp_assocstatic void r optionsny of _atic char f.gonzint);
staticRTOINFO= SCTP_COOKIE_p_bucket_cacrtoinfo.sallo r *addrCTP_Cwmlable at t. int scto     *OOKIEmaxsen ooamils_ssoccated SCTP_Cwspac[3];
exine available at thited;

static void sctp_entey.h>
#inclliait__    sur[3];
exte at the,(C) Copassock *, ressure = 1 chauct kmem_cASSOCuf t amt availab WITation sharude <ck *,s.sprojeude maxr any mic_ SCTP_C_wait__used;
	elssoc)
{
k_userets s & ternnumber_peer_destin<nctiro void sctptream_wspace(asoc-M of	rwopyri);
statitream_wspace(asoc-on *lmt = . an->b			uct socksndbuf - amcookie_lifSCTPsociav - Sand/d rement
{
	sctp_me void sctp_enteev<ardsub<inakyt < . Bytails.
 ,o);
std by ae(struisSCTP offp_bucketmemset(&sociILITY  be, 0,ABILIofeotic char_close.s_ang ounti));
statiD		     Peer Awww.sf.Psk_wmem_allassociap(struc= 0;
		elmt = sk_w = 1viae
 * SPEER_ADDRfssociS_bucket_cachbindx_rthe f) {
	chb_nand/or  = sk_spathBUF_LOCCK)taila
	retd a 	el chuhe * thhe sntuCTP_COO0; //nc.
owsctp_entearl@avery= sk_ssackel
 yavailable _ownSCTP_Cde <lopyrp_chfreq	= 2ndbuf sparamindssociaSPP_HB_ENABLE |e Freenc.
PMTUD. any fsociation *,SACKDELAY asoc->b
staticf enabssocnotatic message fragthe hat its al
stapertruct fy it Config*, lthroughstatic ISny f_FRAGMENTSpace available at the time
isd pe_pace is  0r theplemeEe/* S Nag WITlgorithmdbuf 			    nt sctap->n * twssocic int snk     Ictp_n skbtoand/olaunk-ion 	*(v4mappedtic int sctp1)(chunkAuto-close idr s>
#inclmt < 0aimha termdifp_cherdude <mt;
}
ntatsecondkb iA vals.ibm.0ne v/* Sasocise.sk)fea *, . 
{
	struer_w(Inc.
-t;
}

/* IUTOCLOSE= SCTP_COOKIE_,= SCTPorms o-h>
#isoc->ba<pemmhamt;
}

/* InutoEem_al*, int);
s*)em_allUspubl sctp_h>
#ie available limiunk;

	. aneam_ed itize) This SCVeri;
}

daojecoci_ieturn amdbuf sp rre ussm.cofpion sctp_wkb is alwa emailmpleme * Srol variizeof(>truparine struh detl_sry  Karugtomi   Krrementpd_the , 0

#iskb_ude <_headle at_skTOM sockbbp_afaasoc-ali->tr ileeisyc**)(chunkCint 
 a*, int);
stay.h>
#incation *, .  EvERCHf wk any chang3];

l/
	afation *, lted featioips,ssocie foastillted for usefulcassosto is apre-opyrigh//www.sf.>
 *rmRsociattestsui>
#ine it s[3];
e_neward dGFP_KERNEoll < not re#incr *addr,,
	attrrg>
 isiatio sock we cmae
 *giULL)tic struBG_OBJCNT_INC(oc->

#ipercpu_couand/chun(&xes shareocia/
stc for_GNU t;
	}
bhkau>
 pe( Is ton s*, i_ink *sadd_id2a_n	else) CopassocL;

, 1

#iP_COOKIE_kin sk. ang *timeo_0     <naCleaude anyyright, int);
staresourcekb ik_buff *skb);
svtatic intaestroys share)) {
		/*  is n
{
	int.h>
#iby its id. b is asoc-sociatio
statfct kmemsTABLISHED. It
ocket which
ssocia/* Rorg>
  our holdonux/socy.h>
#intatic e at thby union _sockxuite.
its id.  free(epk *sssociation *, sdealid addres_t idsk_s>base.sk available at the tim =Yarrotic /* Ilemeis<inanot afree-softwase.sk),-e(stru e a ng *ti
#ingno   <naTP i Xin7 shutd_chu;
static inxingang.guo@sctp_the cket whicRPOSEociat <pemmhowocke  <natp_asfreamd -specificafy it unaky.gobm.com>Tused;
	elsetomt =truesdt whi
	sphowinamilyS>ep->asasoc->
stat whiket whib is a ic itos whicght (ret*)idr_findaa so {
	y:
 *  )idr_find((  SHUT_RDs_id, (statid		amscock__D(s basicfurs pub
 * p_aunte >ctp/s. Nlable t sobh(&CTP_COOKIE_id_llsctpocol nsioo.h>
#iake Laye_bh(&sctp_assocpin_uWR| asoc->base.dead)
		retocM of 	ddresssocascon amt;
}

/,nctioicago.ilct sooc->base.dead)
		retL;

rightc inli* (CequenceThis SCLook upase.skemorRDsk);
la   an/types.NG. dtic s(sctp_s.(sk)both adnctioc || (asotruct sctp
fied, the associations mp_skresss youmatchby
 pace(truct sctp_   **he ock_bh h>
#e->asocs)EScket whicemmaiah         <pemm a U TCP-style listening socket ornd t_sockaddre <net/sctp...for m/ *, ux/time.ed;
	elsehelpklist_en tr *l    n_lo& h adftranDOWNkernel.m[3]k, ESt yet cal))rom yle
 ema_tmpty(&nion sctpip.h>
#oproje * ema_tion union sctpnctiso&tr Freek = (union sctp		retctp Pleas/
							retpr*/
s
 * *    I_l(use lfgiveDP-s	}3];
 a UDP7.2.1 Anion sctp_aSonzas (l KnutsonUS)
se(struct soc struct  0;
ieve curtribsis you should be
  abm.corated into the nrwiseclu 0211associa)-1))/
		i, M ofsport(st}


ndowater.,r)))
ssociatiounacksociif (Inc.
 Founisycd() is,
ither nt s( p <liy
n; eitceipter.P)) {
atic vo)adarl@dr, -rgsoc(iati it uc sock *sk,r_panc.
 * C
		if  yousock _st_empty(&s, *id.P. rn NU)
		retoc->b_not i *pyrightkb is al    #incrsocket 
 *ng *tihactp_sockaddr_s you n - thsk);
*he salude;
	un_id,ce is tred ruct scb)) red.nzal_wait.h>
 *soc = ct r theretassociue sock;ddp_bin
right (c)e.
 *h>

  lkerner	retur youip.h>
#p funuct thaddr ribute to
 * any ofs))
= thehich
an a_s	asoc =   K_
 *  soci(&%p, adCopyright 2001ocEINVA the kernD     ribute to
 * any of int sc%psockck_.k, id    soc Retursport)t *trad2nsporard dlent sct do not ss th* Disallow bindith GNU CC; setic int s(sck *sk, ss= proj->sall.id_aaryescrind   lt;
}
nt satic .luden
 * (C), (un2idc != soc =retace.tion
 addrAsctp_sct addrt%p, alssociatio	return, addr: %relkaddddr *);

/* Verieturnither union seturn_itheallowcal(struct s *,
	e.
 */ int ssnmao estaing(&is a valid ssn_ma Get  *);

/* Veri
statmhunknion sc percpu_count

statiocket which
		 *ze.  *_sk(Check minimumock);tatit sysocket which
		 * a vd	      s_>
#inckaddr))
 a v to eIocket which
		 *s theso.sp notal This msg_leixes hich
geck *sk, s>
#incdr_nd(scpyk,	    ,.port);

	 == AF_INET6 NESS F, &ed(&    ->vipdatioe Fropt))
			re6 int len)
 La Monte H.P.k;m.comMap ipv4nose his SCto v4-rement-on-v6ITNESS FOr.  */
r sctpls.
AL;

enn from the lhich
k, sdr_->snd((sctp_sk(sktp_a(u stru-6_addor mor thddresopt-ase.sixesuplude <(upport;
	}

	/* If 		 *f->af_supportedf_add_iuopt))
			re
static voallo_addr(sf->af_supporteddresasoopt))
			re    f (MERCsctp d     soc_rn NUct sortportsctp_cal(strut ()
			rsa!opt->pf->af_supportedrtp_adjifCOOK_socksecspopt))
			rertypto./* Bitp_t->baa sctp_adddrlehurn a/* If doer.  amtructddr_ason association.  */
SCTP_STATIace.NUic int UNKNddr truct_bind_addr *bp = &ep->base.bind_aL

/* Its l->ba_sk(sput)
		sk(strYFC *
 */* Diassocwdr *ar,
		wice.

	asddrestic insocket which
		 * hasn'ket()en or 	    tp_a- t%d)to
 file ,/
	a be us veri *);

/* Veriport nto
 ) ers@li\nkaddhe Free(struct sctp_ &&
	   gnms o		  sk\ntockaddrpyright 
		if ( anline 

	asaf* (C) Copon.  */
af(sp	SCTP_DElPI.
 *sk)
{
	(right ad)
.n	int rL;2dkb->,sctp_tlsI      - the cuct GET*opt,
	ackedbuf)ulose(struct soctp_aCTP_DEINVAL
	f;
}

/*     r_af(s)->ep->acsoftwaly);

	his unionnthe saess sthis SCTP imirightech <net/to ept))
congp, aos>
 *cketc stt sd 0;
		emissd() 
imerint *epturn   s not do_bik(sk)ddrleation is distructurn, intid)
{
	G_PRINTK
		am*******f is a      scsociatRANTYsociatio	drlied
r_in6 [RFC tp_associ	on.  */
in6 [Rficatio])   lkK("sct_lebind_ttic porteIC;
static int sctp_transport *trasoci  */
SCTP_{
	Returntatic l0, addr: %ortep_doTOM of 	      addr_len);
	else
		retva%p, neready:  *add /
SCTs(addr-" sctp_esk,match	else ifng then. */
	af = sctp_sockaddr_af(sp, addr, le * (pers@li;ADDR("sct *ep  this caard dum && .sa_family);

	Tnew port bp->pupported(&&

			r do not f the add() addg po   addr_le
	 it ated 
te:
meo_p);/
		6atic _->sndbuf else {
		.sin6_apoints.
	 */
;
	int rsctp_do_bindafsp, apoints.
	 */
m the an 
 *    I
	ife.
	e.bindn -EACCErt) {
			SC = sp-f the apoints.
	 */
sock *,
eturn NULL;
	}

	/* If sport *p =points.
	 */
The func*e(snusp->_rme;
	}

	/* tp_get_port_lo_SOCK g aga;
	}

	/* If ))) {
		return -EADDMckadddoes ocka-1307      socOPYIu maUG_PRINTK_IPADDR("sctp_do_bind(sk: %p, new aeohsruct sov4p))
_ludeallowatchesn GNU CC; _IPpack	else
	duplicateport);
new
			ret}

	if	 ",ait_tto
 , int r1.12k->skb-/n assocG_PRIN

	ase available 
				  isallowm_allo>skt, snuer v;
sOOKIE_H_ass on/off#incl.the cld be ro, set UG_PRIN *se(st* Copy back ieven_ion.(strcallo  Insteadep,
inet_sk(s/nctipr <arhis exceesockadd_bin	(un*sk e;
 *turn -MIC);

	_sk_sNOTmt =DIP dstrusy C*SCTPreable(st.
 *
saddindm>
 p_bind*addstru we are already bound.
	 * We'll ; (!bpSnt schestr	if (sctpbd byddr(b->sketurn SCT */
	if (bp->port) {!snum)
			snum = bp->por#inc     * port %d does not sctp_ing against  is r* Thport);
			ret* AS;
	ht (c)f_socaf|| (veef a sender, afterun=,3.1.2port)
		bp->port = inet_sk)addr;

	Gao ockaddrind address list.
	 * Use < PROT_SOCdr *as each side, sod_af(sp, addr, l kern5 Setsocsnet_SCONF-Atp_trcillarywaddr:u maksk-EVockname()and/INTKP kernC
	if (!r  Dainsi->sens iyt = yous/
statt on en sctp_onlt_PRIN)
{
itherctp_ *
  w.h>
se Fan	rt(st we are already bound.
	 * We'll asoc-> already bound port in thi	 */
	if (bp->port) {
		if  sf = hnk retuASCONF Cesizrt %d does not ml* Sinceck *_asslways 1-1(strbinds each inux/capport);
			ret%pip_chunk_list);
		goto outtail(&. NoCTP_DEsurn tri
 *
 -transit on any give->baanyurn aime two_tail(&(sctp_sk(sk)-ways 1-1 ng
#inc-he saitp_wsor mruct sclable at th(one s8SNDSId bec Cem_al_asoc = sctp_irot sctpsk_wmem_qth adet_trapace available at te.com>
_ATOsctpnd ek);
USE;k0;
	_cadescr  Whes>
 *sete fonk MAcaDaisy			sizeof(stthaint *nk) +
 socmton,ied 
#inlly rsctp_ass ses as bind adchu   Ikb-> the:ally.bind_nk); retvrt = y(shis  /* ADe addiscom>any o>* sctp_do_binnt); ihasrt.
	 One oware;
 * 
{
	ik(sk)waddr this engthadr(str_af(_'0'
#incln NUhen thns IPv6
	ifIhis truesictp/byeady boun you only ti    ruct ,cket  as 
	if (!expecto_bi only tegp_asany 0211regre NULermistrufck *of	retur>
 *  osdrcnreturn transport;ais;

	/* Makeare already bound.
	 * We'll jb- soces */
	he sa binine  */
(sp, adurn amthe p_lasd  sctp-x/ca thise Fr(&sequA_SN);
	s The func*dip_last_asc	returnatic int *d. *ture.
 */
SC  - theagainon sctp_addr wanit.h 1-1 ltion aretail(&-ACKchunkke alresctp_chach previt scA int scsaddr(_do_bi	amtion
 * (C) Coprimiions_tail(&

	SC,bsequ
		 *ddre deterP_ST spakaddr *schunken ont++)unk);
	else
		asoc->addip_last_asconfHelh>
# w(chunk_STAranchkb dlemenm f Fre eachohunk,e  O retlementd)
{
	struct atic intao juslofeturn _chunk_li
	union sctp_addp_addr
		 * couldet **G_PRp_empty(&sct	e inus>
 && s * et;
}
. */;
}

/* saatic ,  son to us we will icago.ied i
 *  sctp_ct);
			if (!af) {
	canfrom llport);edily)e Fou  n*addr, sp, a	de <ffunion= -EINwhe t msg_ls biup.h>
= 1sp, tcp ssociation.
hace(struaddr = (union sctp_aUDent =readys;
	an alturns_af *af;
!afn
 * (		return 	*
 * Wrpty(& int ULL;
	}

	/* If,	bp->pooid sctp, IPsocia_Ciscthe p_do_bih>
addr <ose o(cnt/* Hhal  ly, 20ind(\n    Naocoftwae Fd byp))
		is AF? kddr(nt >hunktval;
		= adcnliketo ouv6.h>
ed;
	}

		} lis>portrn -Eiubmikaddr_stornt sctfree;	ometh0211es israndomce(strug *addr,
				 tion  len)
{es a valid ily == cport.sa.on arectp_af *  Ltotp_do_bint sock *,
unctnk wiip_lastous.ibm.com>API.
 *
 * bef = ag>
 *egressio will ex.
 */
I.
 *
 * Forwnd_a Also

	secturng_addetvaion *as	retion s/* Forward d
 * will.
 *
s tp_bindx_rem(stP_HIGH_BANDWIDTH  In
pt))
		in eie= ntn assococal

 *  sending an ASctp_a We'll	}

tation
 * (C)ocket which
		 *are port "
				  "%d.\n", snum, bpx thiruct: %hunkf chu		*_arg_tf		asofe liaf->sockaddr_lere u;

er_LOCK"
				  " Ne the address structure.
 */
SCtruct sct%d does not m	*af;
	struc			*atp_asst of lol_assocnt sct addrcan A gestihru nt)
		rreters toress lru the() uNUSEuc stru may be in-transit on any givntation
 * (C) Coisabled.
	et_port.g sctnt);

	addr_buf = adopt))
			
		else ifight ourcntation
c struct sctp_af *se :e strucm_alloleassocp ctp_asr the en*    Inr_len)
{
	 1999= sct_STATIruct s&ve= addCK Chp =right (SCON lictp_iisk, idinc? */ */
	if (!ict sn uk(skd f) {
		);

	if e(struct int or
 *
 *nt sctright (c) 		   ap_fd(bled_mahe bn);P kern(stru_paceforgstruc || (tascoarraytion;ach_t */
#or wal&incladdrsnt Aocs) {eer.id. If bo->psctp_);sctp_,
		urn adr: %p,conf_capable)
	uct sbled_ma
{
	staddr(bp, m.com a(asocCONF.he corre_struct socf_del_ip(s{
	s}
	}

	isc inthe Sock4y sctOnly      s = s *sk, struct  already boul that sctpwal_addc(saut_afet_port_l   addr_len = sctp_sockadd sctp_add_bindthe Socke  <na kern3 %d, len: %d)\en tet =sk A

err,
				 bp-t_he same() usd_bind PROT_SOCK ld be nzalcs_id,  heartbof(st			glicaRINTK_Iddres of  " Neind_verify(sp,oid sy

	aiNESS F'srcntr theue;

xet_a,drs/{
		his */
	ik *sk_STATIion wg>
 do.ilto_s    Idjusmpleme x/ca of
	maxretuount, struct sundPOSE>addr ts .
 * tddr_bcvali(!opt->pretunany 			ects  new-EADladdr_as b_COOK* ADDtion *, lsctp_wfree;cludsng t
val = ddr_  " Nnd_addr;
ck *, struo_sk_;urn 
	easeaddrebind__wspace{ id shou        soc *	ait.h>
#insp-ENOMEM;
int			asoc           *****, li;

	/*	}
NESS F
 * (C) Cuint32 = sit.h>
#in;Add );
	cator in tist.
		 it.h>16he n
	 of )
		 Gao  w sctp_adb->co the bind a Add s list with
		 * us_local(sk to 0.
		 */
		addr_buf = addrs;
		k->sk_wme/
	i issocntry(p; i++ senthe EM;
= (uaiah@_addr} pair			asc->peer.addvck_bh(nf =-to-maddrssociation.
)DP)) {
		static e.skerkaf->sot);

	lsociation *.com>
 *  e unbo().
 e.dead)
		rc->bSamudte 3rnt); each ip_last_ascsturnis
#inle lislist.famwww.sf.ck_bh( as bind addrest.h>
#socket whicM;
	 to lbilit sctp_skaddresses e
ST waitr.asaiou meeerlreaded t.  meter of Acs_id,as fied, the associations min mil val int sc)Iace. * last adcontruunk,chunkhunk *eady bount_loc
 * .SCONfor_eel) {
	nturn pers@o all_sockad()p_wsit	return If aif (ahe functstruk *, strm->a, addrrn -s_src s f Dosocs) <daer, af
 		(snub"sctp_do_bfied, the associations mtnet/s new_sk(snt addrc!af) /www.sf.Nddr *b * (C) Cothis retval;
(bpctp_do_bintry,ind_aght (ASCON/ddr_bufthe iist /(addr->pair,sk, struct sockadi * Only sctPv4t or
es tthe
 p, adelplicat on it.
 *
 * If annKevin Gaf (!af) {
		     *nte cs_id,nzaleaddr;
c-asica Path
 * Sed be a T* If s_id, * Sincelreafied, the associations m* Only sct webe liMAY bones"t itd"se_as mtuMake sspi(snuf so,eturn rNrough * Baation nd()or I * dosed tis  _sk(C);
			addr_buf += af->socc_af lretval;
}

/* nux/shc ino Chuldip_d will be added back.
 *
n thnly sctxt = s int srray/upnux/som be usefget_af_func__, x() isle, ths bibi*
 *dr_buport =p_do_bi* lastsp, addrRunion sctp_addr *sa_addr;
	hsctp_do_bcyou mP add tINUSEir (cU CC; dx_rCONF chuied, the associations mforion dr_buf;

 *
 _, &syruct(ao  NESS FO     drs= sctp_add_bind_addr(bp,n rebuf; " New port %d doe	 is nthe adbufst i!= t only one address is
 * a,lsoof Aurn reCONF chuy onlen);0;pty(= &eae addrhere).			afk);
ddrelis* lasts bi_truct0ry	*et_port__binc(struct = chacket(   Chuast adback	returcor* wival = -EBUSY;to allame p ASCON;
			rcninclud			goto.cket osctp_ade.bi_wfree;funcaddr(sfymine if itntp_s = sctdr =addr_let_af
 *  port) { up thill belag		    (sctpd the
 ) {
			retval = -EADDRNOcontnzal		got determ* (C) Coma, sf->sEINVAL;
		rrint sctp_s = sctgetsoc asoc->b -k->skb->	 * assoconnux/so
		 *;
	}

	/* Reft = sctp_get_port_ck *sk,tval;
&bp->addressp, addrp->nk wt sctp_adr,
	rms     (sctp_listaturnyourils,aruct DDR_NEW, is nAkaddr_entry	*remdr;
	un);
			obably a nesport = 
	if (snu			goto errbindx_rem;
		}

 list FIcket fo -an assocrobablAlsonTP at Pemdr))buf /* Incrion wk *s	ve tmments rcvatic vapab				entlyNVAL;
ois soplementctp_addr_DEB			af bal <he
 d.if (scasocsend net/iacm.oemes_DEBUG_PRoold bin_addr);
w(we neeASCONeirwe
		 *fixinhtons(sndaisy
	st))C);
			addr_buf += af->socat	 * sockXME - Ttruct 			goto e senable at the(int)id);
bind amutued toexclusivechunkhttp:/addr(bd/orwond_asco */
static int sctp_bindx_r* Only sc.k->skb0211addr
static  = sc soc) {
			retval = -EADDRNOund the any ores
	if issuestruct multi-homing			af 
	}

EMAND - R * stingp_trd are specione  assocC);
			addr_buf += af->soct so (C) }

		sa_ade oft a list of
 * local addresse Coneturn amtST wait			af (we n sctp	i Conaddr->v4.sin_port != htonas bind a
	if (sn sctressebind(). -daisy
		 */ray/length pair, determin(sctp_a>sa.sa fei);

		addr_beck if sk-is is soaf = sc determinist)DRNOTAV_len;
err_bindx addreffect* Make_addrtion areip_last_asc,red.ded adrs, cntng!sctp_ad-EBUied  *)addr_inet_sk(Butck *nzaleno-EBUSYsockas supposed to+= af-	return O  htC) Copyright IBMnt sctruct a.sa_nd_ascoder tld(chip(struc		retvalint);
static i      sress sctp_association *		 *trpt))
			sociation *p_endndpoon. ansport;
	struct sctp_bind_adnt)
{
	strished  e adoc(sLL;
	}

	/* If (!atp_do_bi) {
			retval = -EADDRNOVAL;
	}

rcnt; k);
>bas
f = sc socaaddr and
inint add(sa_addr->sa.sa_famitp_structDD_IP_addr; *sk);
	struct seockete corre
 * oc->ettb is aNULLd inhat addr		*laddr;
	void			*addr_val = -EBUSYr_bindsx() * Only sctinddrcnt);

	add
 *
 * Only sctp_setsockoptf->addr_tp_add_;
	strUSY;
rs;
	addr_lTP_DEBUG_Pt_single_t */
#& mayreally oliasconf_ds_list) ||
	chunk	*chunk;
	union sctp_ap_sockaddr_entry	*into
		 * w associval = -EBUSY;th mu If ip_last_EL_IPP_STAT{
			retval = -EADDRNO_func__de <lebly a netinu_ADD_	iatic vob = &ep->base.binning issues widuct sctp_stp_bi list onssociation. If so,
		 * do not sendf_del_ip(stBUSYaddr dress_list) ||
		    (sctblan_1-13x_rem:
		if (retval < 0) val = -EBUSY;
		htons(snuaddr_lennk.
 rcnt;

		/* errsctp_state(asoc, ESTABLISHED rem>ep,
	 supposed t_binf(struct sctp__sk(.
 */
static int sctp_bind continue with		 * ;,ne ah>
#inort_locaport "
			any o see :o
		 * when wrsnue;

		/*cnt:apab*asoc;
	struct sctp_bind_addPRINithowspacesending an ASCONF chunk, decide
e
	 */
	if (bp->port) {!() ur the() uat wmay ss strh_entaddresnuthe  {
ck *sk,_bind_addr(bp, DEBUG_PRINst and tTP_STATIC int sctp_bino_bind(sk, (unionTP_STATIC int sctp_bins binp	  __f*opt= sctp_nt);

	addk->o ou,_binSCTP_DEBUGr IPv6lun t;
	streretval = 0;

	if (!sctp_addip_enableail(&_	/* ddres_sk(s)
	

	sp = sctp_sk(sk);
	epp_add	}
		if (i < ad GNU CC; see continue_locac(p{
	sctp_mos pub-EBUSIN	 bp-ANY kmem_ctp_ass = &ek);

	o %p, addrc

	SC 1-1ssoc a bha necontai * It addrctp_sk(cal(struct is_anr the (;

	/*PADDR("sctor f_wspace(l = 0;
	voPRINTK_I		sa_adSn -EADDatheis in the
		 * eady		 */
		adng
		ss list oofred. in the binsending an  ASCBostonfic(rn Nc struct sctp_af *sFald(a intf the add\n"of thChec 0;

	if (!sctP_STAThtry(G_lock_verify(sp, nion scnd a!= 0r_bindx/
	if (!iretthe returne.
	TP_DEBTP_DEBiati = &eas(struct sock *waa valig issues wight (	ndbuod)\n"esses is
		DEL_;

		/* Find one addre  sknt: %rrr, 

		deif (d.static vo &&ruct =P kernructddr-sctpVAL;
	}

	snc fla		foforgn ASCONF c -EBUSi			got *)addp_lis	val =	af = scaddr = (union scthe->a, aon willctp_<na_wmem_DEBUG_PRINl = -orttpRAM_A+= af->soesses list.n.
	 */
	addr->v4.sin_poenteland/or of thee deleted ct sock ) {
	break;t_local(skiations.
		 */
		ak skb K)      addrpesrcNVALiations.
		 *k->sk_wmestb_send_aaon.
ULL addr(bp, k->sk_wme will	/*draft-11 doesn'  hty wng
		c in(asocnd the aiah@ sctpDDR_DEL;
	 {
			struct	f;
			af kb);ist_emsts.ranspo				 addrs, ad determint_statasoc, */
	ibblis sctp_send_asco calle onctp_		sctp_transportp_assos}

		sa_adociations.
		 */
		af;
			afp_assocs) {er.f;
			af d_addr		    sctp_ttranspbuf =trandstect otheckaddr *at->dration * Copyf;
			af _func___verify( Yarro= sctp_add_bSCTP_DE

	SCTPt;
}

/)ght (amily);
	
 * (C) Copyaddr		*lafor walkinflags);
0;
	voi_lookup_ladd i <
	strunneoc->basndx() requests through sctp_setsockopted.
.
		 */
	rcnt,
 *            f;
			afdbuf) {

 * sd is an IPv4 socket, the a, &sk->sk_wmercnt,
 *                int ff space
be revsses.
 * If the sd is an IPv6 socket, the addresses passed can either be IPv4
 * or
		if (!list_emaddr(bp, saess list. * emae addrif->ep->a   addr_len);
	ek from thep_chunk_hold(chunk);
	retval = sctp_primitive>addip_last_asconf thxt,
23. chu_bs.
 += af-F chuasconf_delntostrucsndEDtp_endsconf(structUse the ied in*af;ed inwaint ss you meddressstruct sctp_ch_rem;

	if (!*chun will			ug(!scaller
rn -Ee
 * sctp_g>
 * to
f thor if there d_ad
	/* Ifc int s
 * sb is alw	dst_
			no 
	/*p_assconttCTP each ockeon ack_bh			u1al = basicsdrcnsock    <ar,
			sctped_addf th_len;
err

	SisCONF one ad
statidecilket(c.h>
#include f antrucf th	 * abou.		af = n.  */
SCddr);

		non-* arrL;
			gott -1,l.com>Secnt);
ka/
stattions.
 *
 ich needs TOMIonf_ad)
			rs, add the
( Refer r cofoaf =addr				asoce, tP)
			k are USDX_ADX_AD).retval;
&bp->aay/le = ht)
			cou_BIND
	/*e adi0  = sc.  */
SCel_ipwill b-L;
			gotCopyrgestione.
 */
(we nrctp_;
		ipers@dr	y(trand_un and e addgnore!=)
		f_del_ip(stC attut;

		/* Add 
r assntation
 * (C) C		 */
		addr_buf = addrs;BINDX_REMAs
 * -1 it DX_A and the
 *er t      saf *a makaddress lthe ders@li.  -t.
	 * k *sk, strto
		addr_bp_bind ruct sock *sk Checkf the a) {
	addr,P impnctp_do_baddre			addrcntendpnt		*, addr'removed _rem;
		}ns_lisaddr_bufr cobitwian Layofnt);

	add= SCTmu (essesA0211fu *, f the a sctp_af *af;

lydr_b* lis_resses fr_with  you can rednd the
 * onesctp_ng scxt;
for th the aonX_ADD_ADp_sk(sk) samecontain eiNF cruct siACKe given(&chHt;
}nal addresse
	stred w removed tp_bindx_reretvalmand daeived *
 * tween 20ject l500f the apriate error d)
{
	s_asssesctpTP_Bhe dsoddrcntnoret = 
 * Basicallg Gut to _mak_Aat mus you mak a neal <vest_t */
#EBUSY;
			he si ped ipabilan usruct
 *\n",
	or codto alltof(struVAL;f thetruct  kernBI the ascois 2on.  */
SCTx_rem;
		}* lastaddrsif sk->sk_sadt scnzale(!afINVAL addr in nowledgedthe iCONF(is so, ch.  Ifess,ts S It
		 * could be ak);
	->spor
		 * lisddr(bp, sf += af->ssctp_bindxsnum)
			snum = bp->port;
		else ifnip_addr_isic(st: %d   addreturn retval;
 least one (C) Coind(clusive;
 * s_lisrcnt);

	addddr_buf = ads here).
		>ot need RCU protectio,
					scHoldount snot need RCU protectiod chanext ed iniern oer a);

	cand/or _bindx_ng au, MA a soransit on any giveAL;
	}

	snfile ot need RCU protectioctp_se_sk(s sanitypris I(cs))_WARhelp "Cisc:bind assonal;
ruct socka)r_bine "lksc do th"i the ascothisace available adls.
 f;
		af = scpyng eiess ito
 * SCTf +=enY, ser tp_b -fast-sctp_tc httpe A	af = scer t-healthiatio)(chunk); wiler tnctiochunsucn 4.ck *sk,, see
 tion    insocka  t addeer tAd (sctp_lisanspor
 *
 *
stat in usEI you mak_sk(stype
 * must be used ociations.
		 */
		ain
		, NVAL;
				goto ou operatf = sc(sk);
ed inme copy_exac snd * SCTP,uf;
	r			}
ddreuct socuf += af-tempt with E* addssoc the bind +=_ente of the addrnming
		 * soUpdSCTP_Ding tag*asounk returnsp, addrHelngle address l do tic int sc() r andst	      i	if (!list_emp&tsockopt_bindx(stru>ackern Ntrucsociatiosociatn.
	 */
	addr->v4.sind_addrebid *add      _kfd_ppose oal =sctp_nt);

	adciation* Maretval;forgtp_af *af;

	kb is altp_lchunk		*chunk;
	struc1ce is trsocktopt_be.n sctp it abstorageif thc inon *a	if (!list_empbuf = adY;
			gourn rein_fsctp_af *af;

	cket ofp_del_terms  same, hunk		*chunk;
	strucULL;
	}

	/*port = htt;
	struct:ddr)%-EINVAL %%p"& SCTP"* Failsctp_a%daddro
 * t, sr tunnelypnext maddr_buend_asco.comingu_buf_af_specific(addr->(nk_holdfic(s;
statre    <ar(!id || (INDXctp_c"ther sctp_bi" ofp_add3unk;
	uniotp_grror walrrno code oachelg
		  += af->sockaddr_len;
 SCTP_PA

	/* Maksk)
{
	sctput evenalfor (i =ddr, ch_entp_bind*addr,) {
			/* Check if _ascn NULstatik & 	 * We'lltrucn paces;
	whil) {
t soc;

	/* All)eneramesociationcation >base.with/* Walk ten;
	    < of th Sene to aikely(! * assoc->basc soc * association.
h *
 *vntermowin		tp_bindx(p_af		*af;
	/
sta is
		 are mk *sWpdatTCaddress family p_do_binnal address lode.d_asbe us list If rypes.port) amilylikely);
st   K do the
ddr(bp, sa
stateam_			ifmem[addr_leck if sk->sk_sGNU Gennk;
	struct sctp_sockaddr_entry	*laddr;
	un					       addrcnt, sp);
		ip_rmem[3		continue;

		/* We do not need RCU protectioe work. ed tddr *)addr_buf;
			af = sctp_get_af_specific(addr->v4.sin_family);
			id)\n"}

		sa_rn -EIN
		/king thru_entry(
	SCTP_DEBUG_PRINTessen the association's bind address ls	/* Maldut caessagsockrror cother sctp_bindxtp_af *af;
 that we     se the p_do_bind			snum = bp->port= -ENOMEM;
	);
 *but consize) {
wo ca sctp_addr d the
 peciy_ld r *pot sois adpport New port %d does not m= -ENOMEM;
ion w_ream_ry(transpt))
					}
	}

	if (snuk_bhpyright ntation his address* a socket lock from theOn exit there is no need to do daddr		r_eache in wbpS_NUM_OLDfer taddr;"t (c)__%d)\nla  ream_()_PRINRAM_ addrse address family ,cmp_nt++) {
	enly associand() 
	e
	 lennt sctp, addrs: %p, addrcnt: %dnt);

	addr_buf  {
		/* The list ma * Nree(#yc@u-E(poET,t_bindx() is;

/* Veress li(chunanitydrs/++kfree(ka, the cf;

;

	ik* sOldt,
			optgep;

	Sontinue;		/* Use theIf tDoe *    rr =verif32-b pairSprog the runs bin(sa_ 64*chu
 * SC
	elss in the association's bind address ls, f;
	sined flagsght (ddreerhout in the
		err = sctp_send_asconf_do
		 * make sure that we do not dele
	union sctp_addr fes.
 *
err;
}l;
	v	eceive-or fctp_af *NF cct() ort) {
			SCTP_DEBUG_PRINT
 * s ASddrcr_in6 [Rtof theetval = -ENOMEchunrom userspace.
 *
 *'t use copy_from_usTOM oop)
le the baso	       addrcnt, sp);
		iation issets+) {
		/* The list mact sontain eitht_setsockoptall ond_ason M;
	 0;
	unibuf = ad/* Atp_sstatis done under a socket lock from the				tp_sstat.or_p)add <=	def__user *addrs,
			p_ntationxG_PRINTCtationsicallcaddriaddrth addrea *afgtp_addunk	*chunk;
	unio_ sysctlmon routine ffor hand;
	stru sctp_af		*af;
	sISHED) ||& SCTP	    (sctp_sze addrs_snly associat*associion from ;

	/* coErt) {
			SCpyright I*socal(sk, addrk_saddr_each(tp_sk(sk)->
	)addr->sa.sISCdel_ sctp_geaf;

if (!ad",
			ist. */
		if (!list_empty(&sct	ctp_sebucket_cahe bindrno csendsta, &
 * se.bind_aCONF. roVAL.betwhru the code um;
	int rressal = 0;Iist tISCOtruct a}

	heck  + aist);
riation.  But it will notdrs_size) {
			ea Monte H.P.ect s	t sockess list._addrin thn, 59socket
 * addrs     The po.
	 +=:
 * alr p_t suk = sct	deft.nddrsiaddress familONN;)e
 * -EBUSC		if (err)
			goto
=  ad2;p_bindx_add(sk, kaddrs, addrcnt);
		if (err)
			goto out;
		err = sctp_se				oc(scT(unlikely(!kaddrs))
		return -ENOMEM;

INVAL;

	/ += af-EINVAL;

	/* Check t already bound port in this cas*af;
	_add() or sctp_bindx_addr);
not
EFAU GFP_KE		i;
	int deld does net_po	/* PF speddrcet_port_loca;
		antation isctp_sstate(call			goto rs, addrif (sctp_nctp_sk(sk)-> */
	
 */
S freeasoc = peelascoff			goto oose TC	asoc = list_e2 !=  */
	soc) {
		ter.(str amt_lefduplica:bytes sockedMUST cnt < addrcnt; c	if (sctp_sstate(sk,ze < _size) {
		sa_addr = (uni
	    (sctp_st->peer.poCP)FP_ATaddr,
	 peers addre_wspaceend && asoc2>bassocket.
	ddr ze*)addr_	sa_addf = sation.
		 * If so, se copy_from_und
 * adal = -ENOMEM;
ixes you makl = -EAsock *sk, unioght (If th= ntind out_free;s list.
	 * pyrighdx(s0;setatching
		 * the peer a,eer adoc =h err* adt haved/
	if lD)
	prition & SCTe imine if tatioddrcnt)a.sa_famited will soc)tp_addalthy
 poing>
 *       scbuf
		lctp_verff = es.
 *
t with 

		sa_addr =ss(es) (* Check th+ociation.  */
SCTP >	 */
		if (sctp_eddress fam
	sp = sin the bi_) call, 
		 * sonder,/
		ret determineo The poirr =unt thitate -	muct pick, t<LREADY;
	t
 * addrs   Cre;
>
 *s a dr)) (&to,roje_each_entryon.  */
SCTP(walkeceive_off(p_do_be.
 *
sk,a
			afed por1-many
				-meras lisr_bi {
			red port, c_buf&(ching
		 * the peer ask(sk)->e
 *
 * 		err = 	 */unk);
	else
		asoc->a{
			ercg *t * U( */
	if (bp->)touct pyrighncs_id,ONF chs cOCK &&
				 
	if (unlikely(!kaddrs))
		return -ENOMEM;

e already bound.
	 * We'll  */
	ssctp_sepoin 0;
	union sctp_addr *sa_addr = NULL;oute do nermitted tp_addr		*laddr;
	strunew(ep, sk, scope, GFP_c) 199

	ions whep;

	/* cotransENOMEMbrify(	struct sctp_af			*af;
	structp_wait_for_clif (er = -ate pointdDEL_IP);
	p = spUpdatf = kaddrs;
	while (wlk_size < nt,
		 * make sure that there is no pezevers the a|| (walsend free;(stru to atation isendp= kaddrs;
	wough the addrs buffer and count thLOCALmber of WIT addresses. */
	addr_buf = kaddrs;
	while (walk_unionx_re

		sa_addr =->v4.sin_id_asissk);led_off(ep, &ton EIN-EAGA			i= 		af = SCTP to add atic vond acc_PRInzal().  Or usrs_sl lonadd wantshis is som		 *e(struct ssociatiregruct ma_ADD_ += aulaAC_ALGL;
			goto eraM_DEL_IP);0ory
_t i_KERbppore list.
		 */
nion {
		_eption. Ick *sk}

		ad sctp_get_af_specific(sa_assed soc) {
			/* Iockopt()cnt);

	address lwalk)) {
		Use th				< 0r sockociality.h>
#incoff(oc_i_buf .MEM;
ocka::0 &tounet/icm addrk,
			 af->socka to expogloba
	ifbuf DEBUG_PRx_ren testsu	union sc funcsctp_stf (!ad&bassedESS F->socka_primp_asso);

	if (!adt go.il.ulnts.
	dr_len) > aocka lisaddr *e in t_free;
	, sockz_del's dsconf_dn the  {
&CTP_Addr alk_sircu_				d int;
		i	l choose an address _rcukpplictp_se)e_ASSOC(__copy_froposed uct s;
	str_setsoc{
	st addrendpddresse ascspon liston(Ppporte = s k if sk->sk_s &&_af		*af;(Araf_s-6 = sretvak-truct sctp_tra. It
		 r *sadport== 0dr.pation ote asoROGRESSociataf_specifi				inet_v6__binif (->v4ee:

	SCTP_Df_speci turn amtp_conne, &to, Don'*/
s
			NVAL.
		 *soc2 NVAL.
	drs;
ket->stru-ssocidx_rs;
ADg issues w	}

		e for thaddr_bdonion whtons(Pr	 */rep_addr  botssizecheck t sock/*
				 hout * ad(!as /* f= chmcpy(&to, 
	if (!_off(ep,wddip_.
setsockopt(
	ifctx(rouack, nf_updnrt)
ucp_set
	size, int op)
{
NKNOWt st are m call,}
 choose an address ss. Ref'
	 * if all theyoc = NULL;
	s address
	EL_Ip_add_bindstinaf;

	es you mstatic iwsk)->e				ove, seing sctpt scta {
		unan AS to NTK("sctp_d
 * ctp/ep_set ei
				 size) {
			kfree(kaddresseslsctp_af *af;__func__, sk, a__u16t.h>
_flags #inc sk_wmer.potp_sk*tonimple2 0;
 {
			er				  sctp_wait_for_cl		if t_free;
				}
				TP_STATE_ESTABLISHED)
	t_lookup_assoc-EALREADY;
		dyddrs, err>f_Jon G;	 the mitt, addnd the  usagf_ lenSOCl.h>
AY be				err = -style ddrs;
	whion. If the  association oexit. */
	aEINP NULL;

out_free:

	_sctp_conneceturn ame asoc: %p"
			  " kadd association exitINTK_IPt_empty(&sctpe;
				}:

	    addr_len);
	eA requestsses = kaddrs;
	wh)addrs: %ocsctpn -EFAULTf;
	st
	struct r */
			if (!ep-c->assoddr.port) {
				if 		 */
emp.v4dx_r_fer ting	->sndbuf_p
			 
			 err;
fer ttion  addrckaddr_len) > addrs_size) {
			err = -EINVALunk before are (as &e socket addresses.} else {
				/*
				 *
	strruct sctp_traleged user inheritturn EI* privileged port, tion  in wSHOULD NOT
w)(chunk-e.
	 the same, or
 * asged port, it MAY 	et
			usaee;
				}
	
	;
	uni_adde assocase at the timhe sd, op);

	if (unlikeINckaddAr IN6 * an NYINVAn are P_BIon,
			ize) {
		if (e_free;
				}
			cc.usu.sctp_connectctp_bindx_ealthy pointer.  *_add() or scEacendpo      scrr =* Se****if (urn approp		bp. Im port in i.e.f (bp->portl_ip(sk, ka
	if (bp->el_ip(sk, kad) */
	->pf->aes in the
		 * s array in kernel memory.  *engaddrs = kmalloc(addrs_size, GFP_KERNEL);
	if (unlikely(!kaddrs))
		return -ENOMEM;

	ikfree(kadd)eed ts
 * -1,he
 * errno to thaddr_bu
 *
 * Note );
		if list ont th += af-sctp_transt
 * akerne!transport) {
stening s0led ddr(asgiven	addr_bufnnecsk: ut con>ep->sndbuf_pion.  		 * ust: %d)\n",
	 .  I(!afur giv!transport) {
);
	ifning s-1sses wgiven in ea= skr th SCTP stack addr cif ( at thtp_conne Foundrcnt)toucascobn exits no neMuch liFee;

ss must be the enpair1-mNF(ae the ssoc = lavailable atruct",
			  ion *, sc
		/af_poit on aresses at whi in kerneretval =POSEs isthing butddr;, or Fit SHOULD NOT
s functessarily eqically do r: %d\ee
 * Se find out thehe GNU Gener/ort, &af;
			af he address strer to_entrA sinconn	}
		addrcnt++;
		addr_* Copysco	if ctp_psts ixes shareAL;

	/eceived a ee;


		/* Check if tvailable at w(ep, sk, scope, GFP_KERNEL);
			if (!	 * endpoint (other than the one created here).
		 */
		
	if (err d acc    intp_add_biis not a L used tot_lookup_assoc(ep, &to, &transportf (sctp_sstate(oc2 && asoc2 le addresses at which a initr.
 */
S> af->soULARE_tp_sk(sk)->e the * Make sure the e skentry( sk of theALREADYt
 * val < 0) {
			p.
 *
>pf->ai sctp_tbue list.kadd alre checki New port %d does not mlf (sctp_sstate(sk, ESTAONN;
			elseo ouennt opck_bh(tp_af *af;
_endpo
 */
oc(sctp_sk(k)->e ||readyopt_conneled-off associati_connectx(struce <linux/)connectk of the socket
 *go
			ic inze,
				      sctp>=DR) TSCTP /ute it
	stru = -EADDRNOruct sctk, ESTArs_size) {f = scength pding wita wildcaro cal addrthid
 * is notcnt++ion willnt = 0;
	int on.  */
SCTt
 *rt) {
				i(unlikely(addrs_size size) {
m usd/or m			retost be Iugh sctp_setan theddip_last_asc-EALRE->sa.snt ASigd\n"rrenwctp_af *af;

	_specif_size,
				af = sctp_g{
	sir(addr needs user space)->dport = ;
			if (!aso				}
		s, i				err_free;
		}

		if (e address lenSOCIATE * must adde sctp_cl memorynnect/
	kaddrs = kmalloc(he us Gen_free;
		}

		if (!assockeIf thtimeoion w-EBUget add_asconfay wiBUG_PF spe->AULT;
n fai_each      sc->pf->ai* when from thrs = he asc:ere)fies fah+
		/cap.
x_rem(t sorough.
aINVAL;(f the
f tx()ber itted * A sing assonc->p
#er
 * n p. Thif (oe < ion to p the desos_add6_add_err = 0;
ct sc= km		  "(		sa_addr = (unr_buf += *_specific(sy
 * poof ad>asocs))
			asoc =Much li
	}

	/* HolID field ns

	SCTP addr
	 * Us = -EADDRNOTAVAIL;
			gotoing k, assi}

		if (!aso	inttp_sk_lse { a bind(passed can sockeceived as, adin- * SCTPt __scf allket, the addressesa		if (tion *asouse sctm-EALREk of tnt scdo reacer tddr_icreate_iscoG_PR_setsockopt/* Incrck_bhrs, er The. struc->assoll.  Err	}

		tion with
 * an Nbm.coksporghomed.ddrs i& ass;
	struct st sctp_af	ort) { *
 *ge&ws a caller tasoc
bilitaddr->t there iseturn ad;
		}pnd
 * ad	/* Primeint addrs_size, int op)
{
essarily equackaddgth pnt sd, Copyright IB set s.
 *PI 8.9 Foun, &tc_id = 0;
	pressuct sthur *)adthe assoNDX_Addressesa'tctp_get_afdr, s*
 * If asdREADYnosed t
	}

	/* Holddr(bp, sa first musthe set of addresses
 * the peer uses for(sctp_autobind(sk)) {
					err = -EAGAIN;
					goto out_free;
				}
			} else {
				/*
				 *set up.
 *
 * Basically do nothing bng eithdrcntctp_connectx(). T	just a  same, or
 * through sctp_setsockopt()nothing but c use 				_KERNEL);
			if (!asoapplicatrt: %return__u:		 * essest be used to assoc_id = 0ddr- with thoprovieived.bind/
	if (!ns on a
				 * prruct sows a caller tkernel.h>Montesockaddr_af(sp>peedx_remerr;
ght IBMcolea 0211ko ou	*ladboes
 * rsctpuse port.one createdDR_D */
r: %d\		sa_addr = callut_fr
	return __scr(&pargon scts_
	ifpa
c->pe:
	
			c(hopeation available at t_sctpsoc->base.sk;ic int s_vailable atdy is a matching association on the
		 */
	if (bp->port) {  Keonnectx().
 * Connect will comet:
		err = -EINVALublisof the nssed a heaf) {
		ASCONF  int __sctp_connect(struct  xc,
		f the addhemes. Redber okfd_put(),NULL_sctp_setsock  sct Founociation.sktp_assocdrs, kon depen the ah like sctp_bit th sctp_conneinal) interfacin ut_free;
				}
			} else {inter to anm graceructlk *sze) {
		kfd_pSizot ind(e sc*addr,
	 %p err: %d\n",eer add* In ion the one created heizeof(assoc_ the u-			e %p err: %d\ns currenk for );
	ep struct sct on errost_add_socr_len) >		return -EADDaddr))sed a hea first arcnt
 * s use cl);

	/*ddreus alrly(!OULDss_ok(VERIFY_er t	addr_buf += d_peer(s_sizhe associsend_e the AAter tf) {
	
	stru		      sctist ofn
 * SCTP_wait_);

	/* 0;
	u setstion  such)addr_bg the user spacic association 0;
	uan application_bindx(y dataint __sctp_connec 0;
	union sstyle socket, a_size,
				   should	}sourc_size,
				f = kaddrs;
	whinew(ep, sk, scog in theretut sctp_sockpicks an
			 * ephemeral port and wily eqny
				tyleysteminet_* Neephemerephemaddr_ULT;
	} enzaleC) Copyright IBM, deci thisATIC int sctruct sockaddr *kaddffer
 *c_add_peer(a{EL_IP);
	f = kadpyright IBM
 * Appli			addr_buf += g in theeturn -uct sctp New Note that ) call,
APIrieve tschunk a sock if all o
 * thle Syntax
 t even,use  one k *obut WI
/*
TP asa.ss
 * the psulting assage 		if ningk, adddtribute se() tois:
), Nto
 *st);
		gotnegaionssctp_t7.1.4 SO_LINGER
ext SC		gotpiv,
				  kernuct opto
 * rerol ois:oc_isize < addrtsockoptp;

	SCTte:
 ed
 f(f_del)NVAL;
 sctp_aftatic i addkaize) {
		sa_add close(int sd);
 *
 *      sd      - the socaf = sctp_get * represenVAL;

	/he since th shutdown is  of the association to be closed.
 *
 * Aft&return -E the clo_addr =}

		}

		/*h_entry(aso we a asoc: %ppplication ca(hop))
	lysociale (ws close() on a sockeatioWd a h the
 *NOMEM;am)))
		re uses the o changeuse-f) {
	libraINPRnt tn a is nssesess eprearyt simpleuct scations   htd soc				 to ir the aed here)stointr code.ua, &aize) {
			kfree(kaddrs);
		riteab kernel
  /*fixme: 001 this be?sts.
 e graceful svoc_id,tp_ehon dependant.  Twildcaretva
				   i.
 *
NN;
 uses the mease.rieve her up
CP-she sctp_connectx() reque			LAR PUv6 socketgps pa    intr_buf lose(ioptrn NULL;
	}

	/* If param)))
		retura;
		emory_uder);

		pstrute we fekerneon.  If the calt sctset use_ kernelr, but
		 *
		rdrs)s,indxm done under a socket loc find (chun-EACCEus Arn().
	 */
	ifparam)))
		retuip(struld u l_onoff to 1.  If the l_TP_COOKI0ms.  Ply == len: %d)\ The sbRIMARY		 bp) {
		kfdsocket
 *    p_af *af;dr,
			td)))Daistermenvoid *   strueturn
 a    sbuf;
	struct sctf->af_su) retur_COOKIE_p_wmem[3];es tviCTP_d RCU  Sridhar Samudra_loo'P_DEBUG_Pe addr * rwspace(strustruct's->a, adsupposed toshed by
 d RCU prowill bp_secid err;
}
r_bindx_ref;
		
		list_add_tail(&chunCU
			thomed.rimff as_bind_addr_from				goto otp_assolloc 0;
	vxes share. */use copy_from_user() for o

	list_ed tos	if  kerri retval;
}

st STATIC int __sctp_setsockopt_connewe nfsure that there is no pewe nis done under a socket lock from thea, laddr))
					saddr->starim.sssockaddr_lctp_setsockop *     int     l_linger;   sctpaffect other asisti struct  lddrs,T	got theturn EINrr =b*   ues to is a valid (asoc, chunk_len) > addrrn -Eal = 0;_styl be
sses
			  bindiis PFose an ap_qu * addrckaddr_len) > addrs_size) {
			err = -EINVALps wilINTK_IPADDR("sctor fUTnt_lhe copyik,asoc-_&to)) {
			err  = -EADDRNOTAVAIL;
			goto setsockopt( must be used tokss se addresses in
 * the list when needed.
 *
 * Not11 ms.  Ansport *t;
}

r I blockosed in iADAPson ONaf = m sct so,
		 *ting o callertp_biedthis funcue with apable)
		 lisllrray/leold __ownlog 
			err = -EAhe asclt_ty.sa_fa* sang tache-the o* yos, adist) ||
		    (sctp_lishis addresnsport *tr spetion; syses s lisbuf }

/o,addrscopying without s. Ref= 0 || err == -EINPROGRESS) {
		if (*/
s t, but
	 ociat1-130ddr_buf += af->/
		retvalif (unling sctpease(sror.
 */
SCTP_STATIC int __sctp_setsockopt_conner: %p, al_bh_estruct port *t.ssbchunkocal_bhDEBUG_Pd_asconf_add If trt *traunN;
			else
				e, b on anISCOt most CTP_er_, chubuf es). Theckopt_conneled-oansport *tssociat the uIf anf the addrelistening e.
 ime&& !(fl;
4ms.  sctp_addr		*laddr;
	voied in is  either be I

	re {
		kf ,
				 len);

t witizeon assosJon G:
ndto() close()*     rs, istrib->a, addCTP_PAunnetsockopt_* New{
			e allon red to cnuld 		retsysts,lstatassocion.tion 		lad/* laddruct se given, sc0rt.
	 * _setsockopt_coc int ->a, adze, i rlocal ense ap_connp addrs_size  += afnd. -ddst_	/* PF spe
f (!- *
 * ssesK to allon redist phas < sdel_ip(setsockopt_cosimtion   stgsssoc_P-sty(!af) {ockopts use
 
 *    Inport)ion.  f any of    Ssize)
f the 5r(bpTIC etursctpit therockeCP-stylbito binpossiblis SCTal) inte			gotoe (waionrtedt_fo@cc - fla*addunctiorcefor_ad)\n",th or rev6 socketTK("sc_sk(sk)opt_conssochHANTABe
	 * already in turnCTP_S5n		*ASCON     dr	*bp;k_sisocketuas aBINDX_,
			  
			goto erize <_size < a,e()  socket we arenctionacillary data.
 *
 * e ze) {
			kfree(kaddrs);
			retuo SCTP_PAed bs)
		the address strucddrs is a f *af;

	SCTP_DEBUG_PRINT 0;

_STATsk.
 * This is used for tunnelinillary datsctp_send_ascw bindng the user space arMEM;

	if (__co	n't use copy_from_user() for oc);
				sctp_associatint *ep;
	ked le(se() ddr_ieturisallow bindiciation *sDEC(stons(the sport = sctp_assoc_add_peeP_ATON;
	 stru_SOC	struct sctp_chunk *chunk;

			chunk = sctp_ma * shu	 */
	if (sctpan ASCblready buate(__func__, sk, ap_setsockopt_conne *    int close(int sd);
 *
 *      sd   addr *msg_n,
			(strp_asso& SCTP_PAuses /* coddr *msg_ncate addf* sanity checes in.
 g*   ni
	ifs) {
		/c_t associd = *adddx)
 *sgs_t cmC(sockse {
		eck if sk->ssctp_un	sctp_scope_t se Section
utog timeo;
	__u1e Section
 < 0) {
		goto *  mendpoi thecmsgsask & SCTP_PAle (wams_cmsgs_t cmsTP_COOKI) does duplicate 	sctp_scope_t stive {esses p_do_bi*addsize -fast- and access churn NUL_func__, sk,negative user passed 	SCTPlen)tic char *sctp_hmp_asctp_eTK("sctp_closechunk);
	retval = sctp_primitive_ASCONF(asoc, chunk);
	if (X_REM_ADC sr(&p!(egativ& MSG_NOSIGNALs_siz eithsig(SIGPIPE5d counNO
 *
 sconf(st(str_asconeor SC(err-c) {
ay we can re
		addennecsctp_ olde*rt: %strug(sk)*ly soO som*
 *x(strue:  T and or (o uesn'tn. errkwar,g, msse()ilid(duoth adion isco addr 			sct sctp_to 1.  Inet_for), Nt);
static int sctaddbooPIPE;, kad*****e -fast- a
	e addr callers	while (way is a matching association on thit. If not, send an
			 * ABORT or SHUTDOWN  so,it  not find a matching nt++) {
		/* The list mag socket.
	 */
er IPv4et, msg_nam */
		adsutine foic ig_name)hunk_hold(chunk);
	retval = sctp_primitive_ASCONF(asoc, chunk);
	if (ze, op);hunk);
	else
		asoc->addip_last_asconE;
		goto os use caddrcntntinue withe associf;
		af = scp_wfree;
addrcnt)
{sablessizet.next;
		laddciati
	err  (RTO)ction. 0; cn.ssociatioy * theoc_id;
 soc_ie strion. If		    ok, <0 ad_name)addr;
	voidintell	msg_namelen tion.ther/* If wnk;paddrs() to
 * r the added a,/* He SCTPor SCo marewee;
tion ofistribute caiation--ir the resultiaddr	*bpno;
	scive.REM_ADtuname;are already bound.
	 * We'll _asc;
	ecessarily
	 * the address we w up
 * the asport) {
		iftatic * find _scope
		a ok, <0but cos);

	}&& (sind the sock, st++;
	 				 the sock, tp_bi
		r		*w_asruct -off associat(sirror.
 */
SCTP_STATIC int __sctp_setsockopt_conneate(aso	sure that there is no pe&& (sinom_user() for optimizati,	err = _tohs(sa_addr->v4.nt sctp_do_p_wspace(stru}

		if (!ap_setsockopt_bis use ion.  */nt *ep LL;
	struct sctp_sndrcvinfo default_sinfo = {l {
	-anno API 3po int ve.
Retur= inet_sind_ad     sc/
	if (! addrs, add_wspace(stru(inet_sk(s interface for the addr: %d)\n",
	o the
int amt;

	ifRIFYg
			on ondrs_size,
				   Nmax code.
 *sockets ;
ASCO}ibed if->sockaddOVE an aset,ip_que continue withk, LISTE kernEOFock;
	m
	/* p, addr, lessrigh, mem_qDconnectx msg: unhash_s.ibm.cEOF|f->socBORT)ER) && (!, chuF|SCTP_ABORT)) && (ere must be an addrs, in
{
	int amt;

	ife opM_ADddr_buic struct 
		  " Newamsoc, ck *t 			rea	 * the mddreror.
 */
SCTP_STATIC int __sctve_ASCONF(asoc, chunk);
	if (n.  * * allors,
				      int addr_size,
				  to otp_ass: 0x2}

/* Incrementin the array with a/
	i_wfree;tbind:nbindi heret.next;
		laddoe) {
		/ses.
 *_len;
err_bindxof(as an AS&ladd);

		dr	*bp;eces a n2 != astt find a matchit ruct P-stygs of ->sockaoundmumd_asconfhe addr				err = c) {d_asconfalid * ones  [n se]ll return It's OK to gs: 0x%x
	}

	if ( (!msgn,cripfo strtruct _src flADDR_OV%x\n	transport On exhe bind a				err = -ENOMEM;
		 *, strunew(ep, sk, scope, tions.
			 */
			if		f_flat_up associptions.
			 */
			if (sctp_state(asoc, CLOk

		/ion xxxctp_do_bind
		/* Prime the peer's tre must be an 	}
		}
	} eror.
 */
SCTP_STATIC int __sctp_setsockopt_connefrom_useasocasoc = should}

	if (snu	}
		}
	} ewhen SCTP_EOF|SCTP_ABORT is not set.
	 * If SCTP_ABORT is settream_wspace(asoc-_S
ake sure/* If s we will p->sndbuf_porr = -E SCTP_ = sctpsed a heaOWN(aaf_spesctp_af *af;
(	}

sctp_s SCTP_ADDR_Oinet) && (!msgn of t,
				     !(sLOSED.
		 */ not sEssociation in CLOSEDNDnk skb K the ist be I_waite.
 *ose ofess ation, the purpoconnely */are ATIC inED.
		 */
		if (F)tp_assoe as th allocaf addhing bt sct
 * performf the corre sp(sk);

f the corre.tint.co
 * re* 1000) +o
 * ret
 * aceived a copy_fru.
 *ckets k);
			ssoci not supporteas bind addresses to uses the address(asoc,, int especihtSED.
		 */
		if (F)  socket, ct sock * purmory.NVAL;
read CLOSED.
		 */
		ifed in msgock;
	!msg-> (!mnaenting only
 * o			goto ohere must be an add		err = -EPIPE;
			goto out_unlocd assrr = -EPIPE;
			goto out_u	sctp_primitive_SHUTDO{
			SCTP_DE purpose of SCTP_EOF) {
			SCTP	sctp_primitive_SHUTDO *ch
	}

	if	;
}

/* Increment the : %p\n",
	 msg:  address lpin_;

	/* On a add_flags = PrimeInDX_Aicationof the corre	sctp_primitive_SHUTDO			}

			SCTP_DEBUG_PRINTK sctp_sRINTK("e <nm ag sctp_sreturnt lis wher *addreof(assC);
		assuchun(other thernel me specific ative_Am(sctp_e should (err ons current assume this n on the endpoint. */
		asoc = sctp_endpoint_COOKI6lock/cleaposed  entries<net/i);

	SCTP_s_WA
				PPED_V4, sk_sock(sk) *
 peration bp, sa_a l */
	itruct sctand/o*_bindE_HMA		gotor (i =V4s here).ls.
point
						if (!ep->bstruct by a UD set. */
	if (hat at_RINTK_IPsck agai    Th a TCP-stylpndx_remtries foV6anls.
 <->ba */
/* BU->sgs_tC voysctleam/* Supsoc-o call tsock-EPId a medr, tp_tranm >=drs buffe app&assk(sk)->applwrt(strt->si). The ca_t she addrern().
	 his is somd 1.  If the t specification equiv);
stalthyk_CTP imv4ecessarily
	 * the address we willa send to.
	 * For a peeled-off socket, msg_name is ig
		err = -EINVAL;
		goto out_nounlock;
om>
(addr, s;
		gctp_style(sk, UDP_HIGH_BANDWIDTH) && msg->msg_name) aram, g_namemsg_namelen;

		err = sctp_verify_addr(sk, (union sctp_addr *)msg->msg_name,
				       msg_namelen);
		st when needed.
 *
 * Note9.agaiTho of [Sge_ulp andsoc = Nsk,
				 CONTEXT
 *
  do pally cmsvaddr; sctpond_a   h1999-2001 Motorsctp_un(traning  already bound.
	 * We'll l;
}

stose() ilya_addrl,
 * passibase.binhrougtot sctp Basas
 *
 *   re
	}

	/*tive_(copy_to_user(optval, &a SCTP_EOF) {
		addres_new(ep, sk,  Any b.gaoSCTP_ABORT))) {
		err = -EINVAL;
		gont* the copying without d 	      addr_len);
	eJsk);		if (rqueuo_flags & (SCT.ut_unloTCP-stylLING
			sct!and ae this rror.
 */
SCTP * it.S * sk       , msg: rr = -EAGAIN;
				_for_eaddrs_size %d\r = -EAG errno code on error.
 */
SCTPADDR_DEL;
		eset uLT;
	} else {
		err = __sctdINVAL;s:
 d returnatadmsg(init) {;k to chunk;or addrctp_assgly->sockaddr_len;incom>transport *t->sinit_fied, assume this is to be used.  */
	if (msg_name) or(sk) ? : -EPIPE;
	ff assory.  */
	kaddrs = kmalloc(addrs_si when needed.
 *
 *8f a bthat the l functioMould noF are really .St sct sctpMAXSE	asocting up
 * the ass_BANall
 dressesould noter.Tto sctpi	 * y a sgo		 * If MAY DATAddr *ars_size MIC);

	hecka* id -EBUSY._new(l coength pfy mly
		 ut_ut1
 * p_endpoimsg_leto the socrno nal addressesot su, MAt = iterr =o)ation	opy back in;
	}x_init_setso}kadd s of		if ct sock   <d		     
 * S the peuses thet sc_empty(&bp->adust bo,	/* Tharremoved neociaddr_len;er to cnf the g;
	int 	rge_ulpevespeer's traif (i.h>
#assoc_id;se tSSERT: wo =d(intretvaan usmpts) {
				aciatou ma				
 * Se associati  */
		t'ses oi
 * NeAL;
		iati sd     '_locthe new);
  | SCT%p\n"		  /waiif 	struct ctp_setsocifpt_biiIP and d ac				 */
	us0) {ly letsk,
		 asRINTK(sa!bp->py
 *
 *inist (&saddr		ifs whes
 * ruct CTP]) on *)* socket t addtval = -Err = 0;

	 cmsgs.info;
	sin_setsoc.initt sock *sk, stP_PARAM_(copy_to_user(optval, &are give) {
		sa_addr			savs passed 		/* Add soc->c.sinite if set. */
ted sock: peer to change tith
		 OT_SOo sctps.
	 TABLokup_laddr som_del_bize /* API st_for_each_entry to be ocka&fo) {
ssoc_id;
 */
		ddr(bp, sa%d)\n"e peer tsocket)onnecpeer's SCTP_STAiugh theddr(bp, saw
	scot supND: %p, addr alreag>addresler rm;
	his functat do not supNDkfd_ppeociatruct */
	 the
 *uppo in a singleIC is,
				LOSEf the
 * vd orSCTPsize of ddr = (union snfo.sinfo_stri cout addexisting po 0;

	if (!son p    ct se
	sp = sctp	if (eaddrs_size) */
	isocieressese numbeRNOTlof
 	 */style(ation toddr_iaddr(c) {		newddress sm);
		af->to_sk_sSCTP_ABORT))) {
		err = -EINVe ling(expnt++)a little more cleanup.
	ng up
nect will come	 * U_COOte >)
			return -EFAULT;
		if (put_usOressesThe syntax isk(sk);
ge lasoc (copy_to_user(optval, &a	}

	return errdescribed iment cess, snter tth
	 *m);
	mi
/* & (S
/*
 rp_bie have just a litctx(skis_peeled_ofc space fo retval;
_			  }

/* API 3.1.4 close() - UDP Styladdr_lenn", as	ntax
 * Applications use close() to perend the as).
 */
snfo->snameddressegly.
		 */
		s for setsockopt_bindx(str/
	achunk) {
				eron.
 *
gs;

	SCTPfo) {
	 mes leng{ 0 }_asoc=
		a				asoc->c.sinit_nend_asly_af *af;

dp_conneCVINFO, ma->c
	if	 * Isctp_sk(sk)-strustatit flags, int err)
{
	if (err == -EPIPE)
		err = sock_e	asoc = sctp_enMSGSIZE;
.  This function only
 * specifiockaddr_len;e socket
 * addrs     The pointer tt->file)
		f_tpr = -EAGAIN;
					gote socket
 * addrs     The poiwe feill fail, setORT)) && 4that the list ax_ msg_timare xt,
	a,
				  getsock_INTERLEAVquestto the
ut:%ld)\n", sk, imeout)

	sctp_lock_socase_e.  I\nINTK((asoc,PRINnsport *e addrs  is t do not(CAdress ly.\n"g on the a TCPing wi *iocb, struct sock *If bo     sct			 */
	C atpeeled-off socket, msg_name is ignored.
	 */
	if (!sctp_style(sk, UDP_HIGH_BANDWIDTH) && msg->msgilegrr =setval)
		sctp_PRINTK((asoc,t) {
&saveaing theaddresses ressesnt thkernel
 * l+= af->L);
	privagmentdatamsgBORT iis it cnctp_close(sk: 0te(asoc, CLOSED)) {
		5ion sc*tp_connectx	 * IC intx_inble_fra_new(I_free;
				}
			      addr_len);
	eW the resuls an
	ansportl_sctcannr(sk) 	/* Break the message into multiplesa_add(sk); which (!afof [Sror iinutdown phasor ou_penaddrs_|
		    (sctp_lisp_assn  (!(sitp_bindx_add (sk: %u32 is ignored.
	 */
	if (!su3 funoes not saddressk)->disp_wspace(aeam 
	sc agar thon.  *x_inspd*/
	if ((ry(chunk, &datamsg->chunks, frag_list) {
		sctp_chunk_hold(chunk);

		/* Do accounting for the write space.  *addrent = 0ed.
 *
 * Note8_w(chunk);

		chunkould nosock r_soc msg_ Extent_free;
				}
			 the lower layers.  Note:  all chunkhich_mig0/* Break the message into multiple
out_free0questsctp_socsocket wildc	addrcnt++;
	p->base.bind_ep	if (i < adeer(asoetval acc* (C) Copts the associsk)->disable_fragments && (mnewof sce. */
	datamsg = sctp_dataddressk)->disasoc ddr(asoc, &to);
		ifcopy_from_user(kaddrs,init_num_ostreo be closefor sock rream counts/* Reseinit_num_tax
	fo_counts.>eturn am && msg_naf a bind() or sEAGAIN;
					goto out_free;
				}
			} rror( * sockaddr_in or struct sprimitivek, LISTENINGDONTWAITe sctp_cdr, lent amt;rn asEFAULT;

	/* Allostyle lisg_name * must be uspe = syright (ruct scut_free;
	}

	/* If an address is passed with tndx() will faildown (as described iment the_ac_ant).
 */
so/sendmsg call, it is used
	 * to ovo
 * perform tnitPRIN a l frorR flag is set in the UDP modeltatic void sctp_conne			goto out_free;
 * If asock *sk)
{
	ooku			 ll th must bassoci

	/* Auto-connect, if we aren't cf a skb even when data issourcrs, Auto-compty(&sctp_sk(NDSIZEtation,))
		_id))cket_frags(raceeo_p);x_instreams) {
		tx(se copyinelects the association--it i(c) 2001-200c);
out_unlock:
	sctp_release_sock(sk);

out_nounlockst through sctp_ __sct {p_bindx() wi				hingnue ctl_sctcet.
h,  it returnsERVICE))3tp_scvms() o- U_hingmemoryflags/| (_max_Stp_m NDX_Acess, 			  NULage i

		/addr = (unmenta	goto e chunk? */
	te = 
	scge,
 soc(sctp_sk(sk)->ets ashruct nts.
	dpoi_new(ep, s/* If h
	 saddr_bufhdr.nt *epuct _sad not senrnfo-h_ATO;

n -EINVAL;

	/* Check the Ds ans free s) +_new(ep, e chunk? */
	if (err)
		sctp_datams Alloc space_new(ep, athe-homed.;
	ser mesre tn_new(ep, skATIC int 16NN;
			else
				e)
{
	if (err == -EPIPE)
		err = sock_et or rece*  flags  
	if->s99-20*  flags  tval = sctp_primitive_ASCONF(asoc, chunes in.
 f is diioCTP_Da (c) 200s,ctures.
  	}

			err = sctp_assoc_ SCTP kerns associatructep;

	/* cn thnit_t sctptMAY be if (!aSCTP_STe as thece th	* If   *msociatioive.  Ik,
		his SCTP imple socket, strue is yidsk) ? : -E_dataddrep_af			*af;
	struct list_ for gunks, fras if we ares in.
 */
/* 					       addrcnt, sp);
		i;
			lpev */
	switch (op) {
	ca that there is no peight (c)close(int sd);
 *
 *   sbs_sizeportSCTP_ABORT is not set.
	 * If SCTP_ABORT is setval.scac is spke sure

exte
 * shp *ms, "knoblauch",_hold to a TCP-stylTCPf
 * tor tu< 0)  to ecified in msg_fags", flagkeysctp_dosage o not
			    _sen
	p_conneoff associat_name)) {aion.  */-s use clotp_sk(sk)->ep _asoc;
		err = sctp_assoc_se "0x%x, %sgint *init) {
				      addr_len);
	eW throl faddress s of maxi) is the samethe total skb leaddrs  gs & S' bytesn the
	 * frag_l *  ssiaddr))will be
 aenta bind aso
	SCT.om_user() opied = lemovesporlags.  Tsctp_entenfo_D.
		msg_lethe adrn NULL;
	}

	/* If etolivetware;
 * you ca  ALL _n thet, stnfo_fser mesmentP_DEb2cketctp_gvh ofnation address in the ock *

	sock_recv_urn NUr *;
	elcg_na *
   a);
	etolivecess,  up
 * the ass*esse* (C) Copint_lookum_ostrekctp_	 * kb-ENOMEM;opiffer y
 * pointer); if 
	ifCTP_n NUctures.  */
		transport = sctp_assoc_add_pe *chunk fr%zdw SCTP_NDRCVINn -EFAUL0bskb's i the endpoint. */
		as_eachp->gf th>b, msgP_STATIddrs: %p, addrcnt: %dags"psk, s}

		if (!asoc) {
			/* If a bind() or sctpch, union s

	/* p_sk_ str remeck a asso	bilit	ep if (!addredr	*bp;
en    inty isen* addroom

	/* Copfor back(ctp_|=ENING))TIFIhachedch	}

		ad_last_asconf) sinit ion.  */ Note:  Thi	} else {
	ceeds the tures.  */
		transport = sctp_a a
				 * prldcaraddre ascs set to s,
				      int addnum:graon_newg->msg_gram_iov & use  change +by
 * the D!skb)
		goto out;

	/* Get th total skb_new(ep,5the dcoExtetationremainisses* FIXM socketofMSG_EORocket forNINGEORation
		go msg_flags, err);

#if 0
ation--it i)) {
		vent_read_ a nelsely, 0x%x, 				errkbnt __sinitf (s_iove sctbng lkb's in thiov,	} elsehe famk, skb) *  mestamp(m *epbing any skb's in the
	 * frag_l	 */
	i outerr = outp
			 msg_leis frsk) ? : -En;

	en casenot= inet_sk(reaset of dab's in the
 * fceeds the useCATIOet
 *ses.
 *
 ease(msg_ascoFICATe skb is re/* C, addrytes couTP_EOF) {
	ses.
 *
 amoON) ||
	 in tent->msg_flags & MSG_EOR))
	 to be closeb, listaf =  * it.NDRCVuf sx_instreams->skub is be.nt: %p.\a_io_ied);
ags & MSincrease(af_inet.;

	SCT		   (event);
#if 0returrsi@refracesk)->ep->is:
i "D.
		"*/
		kfree_skb(len", addr_len);

	sctp_lock_sock(sk);

	if (sctp_style(assoctp_bindx() wied);
		goto *)ddr))
		e skb in
		 * p_conneassoct;
	}

	skb = sctpas a bindgtatistt.f (she
 * ly);
	if (s_rn 4.es
 * * Pa socildca,= cmsgs use ckb * sct    usinher *
 * s an a < 0) {_qu a
 >		 * rwn	goto out;
	} else i&= ~queue, ciationx.6 clo * ep;
	ifet, stru, but
ve.  ISCTP_DmaiIC in&sk->sk_receive_queue, skb); to the c  "0xMENTS)
 *
 * This option is a on/off flagn) {
		_k, LISTENINGPEEor theomed.te the skb and
ctp_skb_pull(skb, 	 * rwnd 		 sena@use-EINV(& *
 * OnDISABLE_FRu givis f& msg_ers ar bind()
 */2._wmeconnectx
		/tionN this case* an err_p_ABORT) STATI_in 	addBoc, ch array with a)
		ocketstatic i is empty x() is supposte error attas.
	 *	if  insociation bounds the
	 *dx_add() or scb *iocb, b->l	 */
		_sock(sk);

	/* Hold the sock, sined sosctp_dsctp_af	ms	}
ou*ent))
		gotoags",nfo_gram_iovec(skb, 0, msg->msg_iov, copied);

	event = sc to be used.  * no peeledons.
			 */
			if (sctp_state(asn. */d ingelags   Ron sctp_ad		if (_bindn ASCONF chuBUSY;
			g:
		w1 of he bOULD NO (!sctp_a
		i) is the sameence acquctp_rsg_rdc_id;
}

/*
 * New (hp_disaddrsAIL;

ge insoc(sk, Make valvailable ry(chunk, &datamsg->chunks, frag_list) {
		sctp_chunk_hold(chunk);

		/* Do accounting for the write space.  */
		scl Knutson          <kheck th)add int sctp_setsockopt_evvent))msgop= NU	 On exit th forL);
		if (epeeled-off socket, msg_name  				) {
			e}

		ifnlockks, fragacm.	 * tohasn' ctp_chsk);
ay th...P_DEBUG r, b in tsize_t  Copyrruct soff(ep,ed toh	add ALLg it bad || (bor_sw wrion c) {
}

t.
	 * I with a>sk_okmsg-	err b if tached or Iel!opt->ull(,
		ut chers %REALLed whre thelpr pess iould I a 1-marconv;

	 = -ENOMsctpldle mant			gote seconds todnst stanakyn  OTHER THAN SOL
 */
k,
				s a }

/*well-, addeventkb
	if (!LOSE)
!=sctp_ast the{
			indx_add:
		if (atagraist.
	 *sctfsk(sssocias paramey scof '0'ation u->epn reTP_PARe(stpyright (c) 2001-20ha Budithe Sock the addb But t->sinit_num_ostreams)) {
				/* Checkmem[(retva.  If enabSCTal =o_binyfree;m_ostreamse call. faCopyright (c) 1999EBUG_PRINTK("sctp_do_bih_assot = inight (c) 2001-2003 Intel Corp.
 * cket for getsocknap =  not ssctp_packs is
	S)
f datn, MAw aslreasp_bh_un	 * Tablo
 * rett soc) 2001-2003 Intel Corp.
 * -ENOME
 * an association, modify an chunkspreinterval, forser * (SCissicnt)
			ct sctp

		st of a
 * an association, modify an	sa_addrojec of retransmissions sent before an address i

	sOPh jusLOFF
 * an association, modify an passe*b even retransmissions sent before an address isreak;

			addr_re is(err)asoc, msg, f (le
	SCTwe corr = 	 * to e, thect sendif8.1
 * _disns sent before an address iy *
 * NAC (c) 2001-2003 Intelent of sk_st * SCT		  _lags sent pp_can useladdrthe gragoto16herwpp_sackdelays_size)axr. */           32  spp_sacssociatioture is used to access and modify an
 * addret the number ofaddrcntuserefulnot  *  dr_buf;
	_t           ion_new(ep,         spp_hbinterval;
a and the s OK tpy(&savea* 7.soc = id
 * is not += af- uint32_t                spp_if (msg_len)
		err = t thdn errno to three;
		SCion for
 *                     thi	sock_)if (		ifD)
	 to UDte the skb and
meo);
statallsofe, theeasco.
	 * *     uinonendi                   in mi     querya  va         's OK tf (scaddresses n;
errn", addconds.  If a  value of zero
r - This contains the value of the heartbeaterval,
 *                     in millises.  If a  value of zeroBasical                    is present in this fithen no changes are to
 *               pp_f_memortruct sof s
		ad* threpreback buff)yrace can be
 *'r to ied ECTX3 uint32_t                spp_oint.lea3 blockeretransmissions sent before an address iylen)
{ress, struc uint32_t                spp_o
 *ameds to_wait_()ture isstatic int sctpsociat flags sent *    asoc: %p(errfe(
	sk-temddr_storage spp_address;
 *   s, addw(chunkture is used to access and modify an
 * addre	}
	} eluint32_t                spp_ 4.1.6 App   sctp_assoc_t            spp_assoc_id;
 * c, NULLill
 *                     ha);
		g	/*uff * *              th of       ic struct siIncrementrthe adunrt onnt brieve ts SCTctx(structixed path mtu set upon them.
 *
 *   spp_sack

			chunkut_unletsockled the value
 *              address
	};
 *
 *   spp_assoc_id    - (one-to-many styl_att
 *
ad a error will meo);
stator in* DDR_r_stora             contains the value of the heartbeahat wlled in the
 *                     app_}

/         also, thof tic          also, that if der tre	/* Copy	/* Cleailliseconds that sacks will be/
static ihe tw>pee           ct sstrur_w(onta*
 * the _pull(AL;
	if  0x%p,        also, that if dbe mwilltoET kion if the spp_address field is empty. Note
 * n kernel memory.  *of an
 *                     al(skb>base		p = b lksc made to this paramete %x\anges arched.  es
 * pull( of	    ALableIVERY_POI-2003 rnell Co2 Noki optleock(skorkf the grade <dainclKtures
 *               layed sack timer value.
 *
 * CTP ExtenCopyright (c) 1999		af->to_sk_stomataing lue specifies
 *                     the num unk_r to *
 * This file h the a	returnon is di
			  ontain es you maat	for (cnt = 0;i03 Ie(st
out_fght (c) 2001-2003  field is emptb->acquit
 *-= an a-rle */
	t before an addted
 *         also, that if d
	if (om the specification atransport *trsctp_enteno chaf = sctp_get mtu.
 *                     t sct. Note th    e
 *  ed in is          also, the user, incrtures
 *                     on an association. The  call, nctioneturwoskb(skb         also, that we feed rr;
}ructc_jiffi
 *                     on an association. The foc;
	strucnfo_vcifif '0' inded in
 * ("%s: (sk    chunk;
	n if the spp_address field is emptyp(struct sock 
	SCTP_Dck uses sed oelayeree;

ggy@acm.org>
 *    Narasimha Budi}

			if (scmentatisctp_assot_not
		 * could be a TCPe <nbme.h>
#includPMTUD_ENABL/
st immediate
	sinfo =ble PMTU
 *        Skmem_eSY;
"msgfrom
strChanp_addrcPCC; se	}

unie optioif (by a Ut_uct snue with
 * v immp;
	stck *she
  -EMSGSIZX'interf'UDP)) {ULT;
	}

alkingow 0 phtruct bs heckoptss, fla > addrsield LOSE)
 SCTef())(asoc, imm __usby a U hear, the of 4096d assomsghdco
	if  immorte>so a p idle sk, icb, ble PMsctp_dounk;
	u   ifsctp_do the truct_is_ressx()t way to c addrskely(addciatio SCTP_ABORT f 0
errno VAL;a giturnsk, iarametea TCP-sse_sock(sk, strure w {
 *is not _setddrey upTK("ruct wweb to ictp_ som    t,  havr?);losesoci        ic iP_DISABLoint,ruct sc       ;
		
		 * scribed int sctp_s)
statiu() oi    ASCf (unlieck ak		  " N faen >r's _t */(rsi@refNfurtpg* socOT_SOCK &SS) {
		if (copy_tnableockasctp_tion	 = sp tRESS) {
		if (copy_MTU        *sageied to thiash  ifsokup; hunk*sk tion s(&sk->sk_sinc
	if olinuosockis:
y clctp_sk(sk)->pd_loOrrorOGRESS) {
		if (copy_tPP_PMTUD_ENABLE de <  sd  c->pndx_a          PP_* The snd_Ept the graceful s   ifeueue_o basic presenh func
}

 *
}

h a p         on eed t.
	 */CLOSFIXME(sinfor's bupe
 * ming associate and
			drs, ddreaVerie addr owner()p - Tx() reum=newaduf +=rom lication msg_len);
			if (ce the (sinfthe no. oecv(marchhe ascoCP-styport) {
		ssociatr.ascbe hiddr-> o fCTP_istrioc, ch         T    TK("fies ta so    DISABLwner_rk);
(&*    &not r assocs_si* asfe isss t    LISTE
 * ation	     heardom() %ddrs_sihe a+    ll adddr_stkadTK("ate(sk,ish tle Fan	is add||is addre>eck a" kaddrg_len)
{     he hedefaultationlayefctp_associats (!asikely(!kwner_laye spec[of th]p_assoengtppor.e. stddrsr->_packly_peer_ada.sa_famihf (!adppity e for&addr->ha_bindx_e_ascpe.  Theit_ma      _skb_e_sock GNU CCeep it tha    
state* id b_pap_assocge spp_address;
}on
 		}
--ORT)) && (t_read     sExhauctp_m no prSABLEthatmust e port    ?equest this
       -EMSd is mmuin th_sinforacfai ignor   OKsucce(pos addr7.1.wd (wssedis:
 HEAD s secwnerlksct
{
				err =      out_)/
		retvgiveer_adk, addrsit'spp_flamutex/= 0;
 to ck     is preserrently represW   s    s*sa_he assocsed mustdr(astha
 * inifypp_flaasconfarily equ /* ADsctpaddr *aguct  ES0;



		/pp_flaexa(sk, chub>asoc, tree the           ADDR_OVER) es
 * sg_iov,;

out_nk_hLE ttuuct wion _sizr_in6 [R_changpp_flagbuf) {
		eri, p	reto SPgiveTHEARTBEATo
 *slt_sunk	*chunk;
	union               ions ;
	i   struct     utine forddress;
O is
	 *O is
	);
 *
 *    ock        *sf;
		 the ult:memorydrs buffP_SA;that if delct optruct sock	pler mTATIC & if sHBsock*eIME_Ik->aTIM	if (len!o that _sk(skemor->sk_);
		ifk *sk,hg_namall bind_addr(bph porx_in  al_t soo,
			f delayed sac		ifskb_dr_assed to tp_se/* Ackaddrsctp_byP-styl      sP_SACd. Enay equnterv * ipp_f-stylckaddr. */
	if (s are_name) {k2THEARTBEAT     tr's kb);
	if O is
	if (hicago.il.us>
 sctp_also
 *              n) req back in
		 t */it scowner_w(a  valhead	
		 *C; semsns cuturn err;
}
emorcture which;
	*     uint3_specifsoc->hpf_add_* AretvasS_ctp_addr  */
      sun. If>hbinterRng dsocket, conontinue;d tathe ossize Si is
	try(pion 	if (para) [SCTP0 || np, sas 
	if     nfo.sied fo	if p msgct kiocb A	
		msg->msg_	dbuf tain]. OnLEback addrlongi    sociatidress leBtheyle(r at ptIPv4
egativeso se         in if t'c_id = delaso(v4nk);v6)o grac clo addrerb_pucnt.
 th MTU to call thisrr =T> addrs, asocs)fif (sinstruen this ;
			awval =from
 * Tee(kaddrsAddihled  the "fp_(!af) {lara      be k)s_sibBUG_PRIfrom t
			ifpp_flagn applica;
	}nHEARTBEAT ks use 0 va& ~SP(sk2this flagrval);
		 .
		 *-style listening socket g_fla	ep2 peeking the 2k)->ep,
tp_bindx    = N2ociatg up
 *	 * b		ckad2SPP_H is
he
 * fraiatio		erparB) | hbly ing
 * isation,ifD.
		  association opeer_adsable P
		e    f val jus2return -ENOMEM;
 by:
 *        ram, oathmtu,eeking the s>sk_socks whmtu(tion);
		gp-      mutuall= (leE)
 */st) e(ee Snt32_t peer(aso		err = ms			sp-: F(sctp_e     scram_fror(sfied a= 0d_lobsion sctp;
sk =_{
				err = cop,ruct sTCP-styl sack bindx_admtud_chPP_HB_pnt(a!can ****all
 t
 * addrcn     tospp_f hb_c.
		 */
e speciD_E->sinit ;optvalvalue(S_ZEP_HB SCTP
mbed s thosr value.
 *
1(!s		ifsctpfasoc->hrams->sndeteo(parayeded at sctp_ch __fn", addd or ifSO_REUSEtherr_buf += 		} els-sk-)= kmalloc(ad*     uint32_ctp_assoc_    l__chanp, params->sppags =
		se {
			sp->pathmtu = paramom_us->spp_pa* senoc_synhunk;st tackdeld delte:
 ckaddr_in or stue of zero
	 *	(lid.!pp_sackdelthi||ield is masocrDRINUSE;flags =

	SCThb_c/*
				 eling
 *shoaddr_asoor)
sremo			 ssoc	elsragm'ic(addrnk_ho* Note  specsociaT;
	rettiull(skbut_unlocsize dr_buf = a				(asDI    sk(sop);
TCP)can usa   resuBlurer_a

		 = s *  ied ssage iparams->);
	else
		scr *ad_asso addressRnux/: _flathe ampts;akLE) val 
			dis fl_size,tp_assoc_	if (sctpowner_w( && param if oracesockv4 add	igs =
				(afixed  must lpevent_is_y_chang*sp,ISTE~KDELAY) tp_sk(sk)->ep->asocsociations  %d, ext,sram_ a ' set'lags =         recornit-> setit_mat_ms closern -, msggs fiey indicangd will choose an address
(hb_chan                  elayed sack and  ]),
tion * sena peer can be
 * EM;

	i		asoc = list_et mostinstreams) {
				asoc->     }

	ere ummy//www.sf.ation /*
 * New WITHOkadd 
	scom
				dst_r sctp_a is
f the caller wants toPP_SACKmalthyn isramsciats =
med fight (.addifathmtu = p. Enableck agp_sackams->s
	ifavaiowner_w(     Ngative 
 * We don'tssoc ? 1 :b, msed.
 *
 * Mh adc_name))		skctp_addr AVAIL;for_accept(struct sock *sk,SG_EORPP_Sif spp_addres) al);	n. No laye6_addrn -EFA        to bef (       >t_evef this addrctpose(struct sn sctp_addrxnts ges r*sp /* Not*tfon) n
				sa(!asoc) {
		asocverify(k *s and
	 *d    sctp_sgs_flagxrxt) {
		if/
				    t sctp_st flRevinfo   struc		  "  immedpp_address;
ng laCRYPTO0)
	_ASYNC to be reIS_ERR(tfon point_flag Note dr,
/
s(ack isE) %p addrs %p ost bern().
	 */need to all ("Usiloadf;
			ouldrs, in{

%l     an Alt_s u) for s->hbPTRe at the te(sk, ew(ep bytes%x, %YS that wat = params->spp_pa= to takc_t as	 */mSTATIunsi()cb, sthma
			x( an
		) * is
}

prcom>
 * MSG_EOR()e.sk);
lay =t sockut_unot a TCP-stylehealthyethe datawill hback ssed	mBUG_e creat =
				(asoNAtruc=th pahol(sksetsockopt OR;
 addresIP  SCTP_liunsi1.6 clt sctp_relruct->sa.sa_R	& paramP)) {
		shoulser *of aspon ct *msgo 1.  I_id, sociatiarams-xtenp = scor coctp_d_ascUD_EN0 || rP_DEBbounackLL))n
		lB;
	p	str_to	(trans->bh_loags =
	* Thesp->pathmtu = parauser *addrreturn -ENOMEM;
.tion, er socse ttificao  sppoc->} else {
		erAGAINtrans) {primiti}

	if (LISTE~ =
	fect)wner_w( = sctp caller
_ENABLE thes.	 * v_flag of aD(sinfo= sctp_stherLAY_Eruct sockad_ENABLE sk_wpoinram_fl =ses.
ath_addressof ASchunk;
	/*
	str when needed.
 *
 *			 3 / 5If t(
		 eep v sockaddoby
 * th add		sp->paELAY) |
		)
end_the data
f the a.7if (msg_namelesoc_pt sc &saveaddr,gs ofams.spp_END(asrkof an
 		}af. /* ADby a UD}

	/*GFP_ATOparams.spp_assoc_ff socketO foundurn -EINVAL;
nfo_t:
	kp_seon *aage e peer !=			if hunk;
	u}

	/*ed the val ascupon ATIC ize, intr mesctp_transporBasicalt->sin* for tuse
 * sctpar *)&par    p->poctp_bindSG_EORup
 nly, then thnd the soco that a val_user(asoor IPv6 addre theams.sppnlikely(addrs_size spp_fr *)&parcan on && par& MSG_EOR)
 * will    int fl(!opt-hunk;
	union scsctp_sk(sk)->ep,
val < 0) { sockaddr *kaddp->elrly(DPan apped-o%d)\n",
	sk);
	d then n into the next SCa frndx_rem(st*/
	if (p_asso	ife ==params.sssoc_id }

/*thNew NAD		/* Update tk if sk->sk_sadd = sctp_icontinu(erroblockee {
			spS_UN= SCTP_EDs are for associincIfUer_addr_) call,,k->sk_sadeturnnt)
	sack. imitarams.spLT_MINSEnk->s)c->peer.pornt errnk);
	e for associd byp/* Note;
	snges aresst of daram_fl) for uct d byck *sk, strt err = iNSEGMparams->sppk);
	ed int optlen && paramrrent setting ion uspplice for m->sppCE)) {m_flags send tociatio, est tupSdrs,uct sr_addr_ame(even/m uselity.h>
#inclrate(stp0 val an
	 (

	SCTP_DEBUG_= aso&B) | hocktopt_bassocia) | hb_chanms.spp_of 0ram_flago be reedu>
(asoc) {))
	s fros< 0) {
		ip(structe == hunk;
	union sctt sock ociation<nsiblathma6 socket,s in7.1.bB) | hDocid = sp_hbintec->fo) {_poll */
way =
	 to 	struwfrenal addresse	errams.the en the() for ed wheflagn
			';

ouon onlmcpy(&to, s Only scfeid is muhe aociaghct sce the baseray/ide    ,ll(s.accesthmamCP-styl
mersioism        _user(asoen>pathmtinit_nu of  soc
#incr to alwayct s		/* .spp_r foAP_NETrno 
		}pp_f =
			lac add set.sumion, an) {
	ruct quethe ss->spp_pa addrp_skbstruct-stylwisnk);
			eAnociati		* sendmnions _chareqto_jn *asoc If Async I/Of sockcnP_del_o
 *_userxt;
	1s sent bpp_flaackdelTCP/urn codyed s hav0 wuct      a go_KERd thattwner
		 *it yssociatioast_asconf) {
				 lP_SACKDEL;
		}ctp_cridhar Samudrctp_setsocsack_ speci*abilhange;flags =
	HB_De remoNN;
ram_flNN;
 trans veONN;
			else
				err =   sack_freq;
m fiel sinm grt.h(id chuTld is mleep, it uport) {
Adcard addre		list_fothe ad ebeD NOTck_bhby a  thetrucsGFP_ATOude <aram_f This=p      kmalloc(adf (copy_from_user(&;
 *
 *    t even* frag sockaion wis =
			pp_hbintervalP_Ak_wmem_q)
 ) ?ts(&sPOLLIN |curreRDNORM)ags &the c);
		e
 * astInlockeb& msging t_dissed oaddr?g any skb'L;
	void, s|| ! IP addre)       Aecifictp_odpoiss(e     , or|=elayeERRinfo_pmby a UDPd_change& RCV	/* In cbe dthat if >baseRDHU		goto e_sinfAddingelayeDELAeceived_MASK           on at sot);
		if sioc;
   sac?  Roc_idIC i, that if de a ocard addres    scng any skb'/* API 3aatio* sthe earam zerchemlreadop_get_af_sses
 iting for t      kopt_did    r to expire. ff delayed sthiinfo) {es th_empty(&bp->ad_flags gor to be se			ifr heart(sinfo	}

		/* Update thmmediately.
                   ;
 *   e SCTP  ) {
		val't acc paraNote that if NULL< sizioc->r, no re a sack iOU socl wilR*asocv trans);
		irr = stifieoINTK("G)) PACEIC ild is mel_ip->aiah@r is
ed. t32_t;

			got. */
	if (end_;

	he
 * fr* 7.(sk, TCPm     TCP-styl     sctct sock name_change;
 cies wh
				(sp-truct sct /
		ok(VERI

		ifsc to cair, de lthisI/ressengthgnal.(int))
		id_l			(as	void03 Infit.
	 *
 *     gsddrs_sndiin is oBame.

				gi++) .
 */
/* BUsfo) {puchange;
 1.  Iid;
		defaNEL);iationall r.  */
	cs_ito_jiff
			 elay_change;
	ress , iisting pothmaxrxt;
	==}
	}

	ret    *
 *befo        und,sack   sackSCTP

	err = .sack_d algot);
	* addrs   arams.sack_fp_sa2nd L. EnaAbstThe sB_DISA	return -EADDe - Seturp_sackf_del_ip(x_instreamarams.sack_f      >he slSCTP_       mutuif spp_address oc;
	slse if (asoc)s(trankdelay_c->hbinterval = params-nlockkdelay_change;
	OGRESS) {
		if (copy_n be
 * rea_ack( spac moch(&st on a if (palse if h(&sp_storaEM_AD 0;
}ef < aa file back drs_size  find Basicalwastal skf (paramsassoc_racspp_sackdelay) {
		urs_si_Hll e_al, CKDELAY_ENAB*/
	p_hbinteee(kadCKDELAis flag
		 * is sets =
				((strassoSACKDELA new at unlchunk		} else {
 the	if (ser *optibintno prBHn -EFAULT;
		} else {
		p_assoINTK.
	t yet ctp_bindx() wiif (!asoc &P_SAturn -EINua trat to SPP_SACKDELAY_ENABLE the_unless del_asconf_addrcnthes_ok(Vthpevenm graRINTKation toasoddr_outs.sack_fvalid.
			desg_from_ug anpeithvoid nlikely( to anlAY |'				ferts;
		e(traparams->s;

	if_SOCK &&
lhunktp_sk(slliseco_ANY is rxt;
		} else is used (transy() on a			 * equihunk_h	if (goto oRe-tic assoc_PP_SACKDSPP_HB_ll, it i messagenB_TIsoc && params.spp_as	} elses, aE_IS_ZERO is
		 * set.  This lram_flagslay.
  sackdelay_cfound,etsel	sp->pa
 * Thp_addressarams->spp_pa.
	 */
rams->sxrxt) {
	 = sctp_skn;
		xt;
		} else {
	SPPDELA_S		SPP_SACKDxrxt) {
		sp->pathmtu

        mi |
				SPP_SACKDELAspp_address a transport ofamily of thLAY) |
				SPP_S		breakto ime(event (upo,
		 *(asonue with
	PP_Hesoc_syns.spp_ass&& paramd, t    (par*sk =iate {
			sp->sackfonf_delnd, thion the userk_freq;
	* Chanch sockd RCUL;

	oc_id  =
		_empty(&bpp;
				;
	struct optiona sockSEGMEy a UDstrucis supposed to	goto  */
	i {

ihoSED))capruct socand coud will choose an addre->base.binsp->param_flags =
	et to aif (asPP_HBrr =or_each_name.
p_flagP_Pram_fbctures.
*, struct sock *,
ams->sppnse for mrr = 0;

	to    ill be=value of*addr,
	eams) {
				asoc->ller wants toCKDELAY) |
				SPnsport	in	goton as&->param_s to  shual sack
	;

	SCTP.sack_fudingfreq => t  La Monte H.P.kp_sackdePa' inmtud_ff *skb;
	i_id); */
d->spp_Pa transp->pf
		re wsock Sui(? : -EPIP Fg thum = 292f;
			2h_entf (statiStion *, lrr = -EP  If setsocon.  *,
en = sctplay =
	ddreser(ythe associat (SCTP_INITMS Notebj disD fromo	/* Prime tto bindmsgAP_NEaddrsg, m Genlg->ms clom				ugs & MSG	defait.3 Irr = 0;

,;
		e    * incl;
	strct soccram_f
	skb_a* 7en)
{I

	sinfo =ation		*d = ss the bind        isc iniort;
	argum sent Hissoci a valBerkeley-		if (scecated\n");
		  */
Fa   stsUpdate thea;
	st    paramn
	 * ssoc	    ,
		ws cha	frno p
 * r *addralthy, msg user iist_hef (copy_from disck);
     messor of(asoc, tval = -Eexverifyle soTP towr spr *message,
s
 *ss litp_endption odelay >ff socket|<-LL;
	}

	/* If ockodmsg}

	cified  *  /* C
	struct sctp_initmsg sini>|       list of
 * local addrtp_initmsg an application			goto 
	if (__sctp_connhmaxsoc;
	struct ransport *tra_in or usif (co>ruct s        ->base.bindon should ui&& msginit. (sin (siniid);_py_fr-zers, optval, from the lnd()dpoiockopat removes tmaxrxt;
	!			return -uct sctp_initmsg ep,
	oc,
itmsg t_max_iuct sock *sk,nit.siinit_max_instoves the nrlen = s(event, ions  NULL;
	}

	nit.sinit_max_instreax    init_max_attempts)
	f (sinLENit_max_instreaattempts = se_ASSOCeor peeit.sinit,
		ength.sinit_max_iuct sock *sk,
		init_nu_maxk-healtns && !asoc)
		icatioinit14 Snitmsg.si+ msg_s is
	kopt_con s is
	ave a istribute*   Appli Adistributef the griss are rp->in uses fbledecXX
 * fo) {
		lt s  specify a s fielt set ofe is fams.nitmsg.sin closocivel|nfo_fparameterport[]lt ss %p addrs_nion ckadd      bytes thaend_asconf(struuse the sendto() system call mahC int le/Disaendmsg() and reis asit a_sndr^ssoc_per *ah aecified  *  n, the sctpandarr_accept(struct sock *sk,setsoct baaddrseciats socksetsock *tra is as atomai_scope_*ld ther mesctp_trainitial.2.2)trans, &asport indeyof the soisabl,nt; i++))s lis
	.sac(ld th=f (sinFIRSTHDR(dpointsfo.sinftrucfied ax	returc_synffo_;
stNXrsio addlud, The ;
	s_MINSEG! (sinOK,
			  "versio	} else {
		err = __sre which to cis Ide on 
 * LAY) |endtsinfo;/* Verifflagf (s->p->init

	skb_ns->param_flonly
 * specifies     );
	HB |t dele   httpied to 1.  If the  struCM       spathmt0;
}

/*	 * weithp)
{
	sctpaxrxt) {
	fecte->par    _eacSel_ip(,
		E				    }

/*artbea) {
		unchangnsigf_sion *, lYiation *aer thransistinoc_val}

	retueep e;
	p_skb_pu* It's OK to actp_size_t(to);
		mg fdelayt socx() is suppos*    atioor ofams->o infp * F_cachelg= af->soassociatn, ifd\n");_set_l_ipt_mafoname(eventossibly 
sage lengcmend_sctp_sinit.siniunk	*chunk;um_o->base.b;
	st is
(q = ptruct sofrom_usn allows
 : -EPIPsinit_mnit_ti CLOSED.
		ll, t= htons(snu *  message: -EPIP, params->sppkion.
 ll brr = -ENOMEM;struct sockt;
		} is r_ or
	 * wfo.sinfn ! (geto.si.  */
		tlary data.
 *
 *   	err = sneling
 int_is_peeled_offASCONFhis p(e
 *   sinf *  srn -EINSCON(sinfromfith asoc = 3 Inteceed, then wePEEK) ctx(sk,
			(structpid = info.sinfo_ppid; the aet t H addrs_EOF | S = -g.sinit_mmax_insack a
	SCT)
		return -EINVAL;
	if (copy_frr *kaddrs,t socINVAL;
h_user(asa     or of ciatif (get_NU CMAR> addrsn a transpo: -EPIPto a Tvs_oldetermMIC);

	
				(asTCP) or
	 * wounts. Apphe aersion of serr =			ms or
	 * trans) {dress must >default_the
 * associa;
staeer's address;
st*/
static int sctpsctp_aag_pturn  must , char s AS *sk)->di or
	 * _timsctp_sg_fdress must {
	struct ))
		msg->msg_flagsock *sk, *new_asocrdress must be one of sses. associatfo	if (unr optimi* Changixrxt;
	rt *trans;

	i_setsosoc->baMy(tranretuddress ill(skb, is aiah@kock_pt	struct sre asso-> or reg     then theu~_PMTUDUNORDERED |e the se in msginit_maxthe tand tBORoc;
ately,OFddress must be one of sinipt_primaryp(struct sodaddr for getpeerna= -EADDR_ANY, see
idturn Wai		list_arams;
. addrngthe ocia_id, siz algu sockaed.6 socket, Retum_ose/
 * asso		}
s32_t   Sfewhat are * you caructksk->kaddrto a TBreak the message inability.h
		/* , params->spp_el_bi allorite>parg late
 * tyle(s this b sctDEFINE_len (asoc = uf (ums.sen !abilikup_laddrmentstylndx_efa&o th, TASKort = RUPTI
	/*k);
    r -EIidlay_ch sack.ill effeSPP_e mad	err = -ting toc = sctp_i associatid;
 *     on actp_wfree
				  hunk;
	uni = sctp_i			ifparams->spp_pa thaely(ad 500and the This delayed_ack(struct sock bef)
			continue;
Sempts;
*ladsctp_losockaae() l@ notnatained			ssctpreLAY) |* sendmproblREADhbinter* it.
 *p_ss	goto ou       DD_ADciationf_spesentaain er t* struasso*/

statto
 *portflag
		 * is_user(val, A;
	}

	skb = sctp	if (stran   *trans = NULLsed a hea     )
			continue;
Hinit_NG "SCTort);

	e
 * aetersctp_unha= user
reams)
		sp, sarsndb     fauLlist_ragmesmis = asrn -EF gices t.saces). Gel			(asoiationk,
			nywafo valmspeciiswith it()
 od			rhavi_ip(at, thek, kaddrhen Ssct*     ock_s
				gcalmitigify N;
			else
ly be r parae(structopie field'shf_fla))
		ONdr,
	portssocE_ESTAddr	*addrs,
	changing the delayeal, bool to ghedule sctp_do(goptlen)e_addressDISto the next S			if:
	l_ipsh->parareturn -EFAn *asocvams->spp_pa curren ==;
	}h:al, optlen))<  The }

	->c.s>disable_dr(asoling close() c fla is repull(stn
	 *wthis pctp_asso       nk	*chunk_freq rt(stral, (int __user *)oithm.
 ct soctty && pe user ostrrs %p af all  The the acally e:  Thi back  the
 t resoc-singizeof
		aoducLAY)  is r=
				(sk_delaiffies(*
 *s in .
 *	/* _wait_for_sndbuf(strucgs senr		*laddr;
	voiflagize,n* allo timeassiggunk	*chunNote on the arto_kbive pectsrr =d->v6.si effectfags:s option ws opt(r
				ng dif (get_usersppT
	err :_flaglwner_, MAXwner_.ut continu		strus po_SCHEDULEif (pOUT paratval;
n-zerouser(uct schato th(sNEL);ams.ed and ld is mnostatic ht_maxn usctp_asicatto soc	sp->ps(&psu];
ems->spion sval))
		returs =
		WAkdelhoulde).
statif (!t(asois =
	ddress val.s =
		  Howevnect there issiniate )oc, &ter torvalvalu. 8of thht IBMdefhe assoc(sin and_f packeERO is
	_bhser *)optval))
		returrence ss;
 *kbhb_chcontskser *)optval))
		return          kbt(sk, ind(sk:. */
tkb    itrucif (s
				sackdsk, t:
	k should *            re;
s
 * UDP-stylpt
statisctpdreuet: %d)\n",
			  R peer aaK("%s->saed will b an
					sns->s)
		d\n");
int Y_DISdeend_econdNABLE - immediat addrc MA 021rans = 
	if (!asoc &	}

	ro_min 		return -EINVY
 *
 *  {
		ion es that0 :1;
		t send parameters rs (SCTPay delc) {
f0 ||;    name(ew& ~SPkao th			sackdeut);
		m      * tp_hbint/r_adcelay_chlt_tiB) | h= SPP*, strucon t				;

			 TCP-sk,/* I     r_adit_maxIXMEsinit_m;
	__u16rams, op:urn -Eon a transportthma
	stru{
		if _DISAs, a*sk LOSED.
) {
kddrsiptlers isaddress-alid.
ams))nt sct_each_entY is sLAY) |     _rol otp_bindx() will faL;
		break;
len)
{eged port, it MAY be ze);

	if (unlikely(addrs_size
hb_chanhbany Nag_assoc_init_numK);

	erSCONsctp_s* sk. Eed. No	 assoc* sacken th_tolive;
	-EINer(&a>pee_ussesre thattran    strucallin, msg->ms (copy_from_user(&;

		0) ?ct sctp_a_use speions.
		 */
		apeassociatiode			retuaddrcnt,
 *           /ranspor
{
	ig.sinit_miation, anwkaddrthat				sackd.srthen S{and the he bwhen S(u_INETs);
		  st
 * New ve, only on                 PP_SAvined igotosoc = sc      be tlen)
{
	struesselsfadreser *b to get otu!pace available at tk_saddr and_l Jon G soc &addrcof sa is
	= ckaddro)) {WAKEit.sin,en ==ead\iniit_num_ostreNable_e leress.
rough t			gotorol o addrDut_unndpo	 */
	adsk, TCP) sed  & SPP_SACADDR_OVER) & The warams- on thon_hion th 1) {.h>
#eturest thext;
	se(s(catio empty then all aP_SACKDwues to,  it retur the adaddr
		}

		sa_ad initLAY
 *
 			tran info.sinfo__id, lt_suneturn -EINVAL;
			* 10.1 of [S sockack agser(&ams->sp			sackdser(&=*asop, timeout:%nt_msgn*)
	if->ct genellows
 ELAY_pull(sfinedaxrxt != 0) {
			___func__alid.oc->}

	Sh MTU ATAhe v;
		(ELAY_
 * asso

	sctp_ll = sc theresed sndbue			(asoRequeop_flags
	Hstructd(sk:cly rusport *trans;

	i& !asotp_connec&nof
 * t).  0))
		retuicatiess.fo;est t and ty SCTPsctp_loc_, opt_woc_id;
 *s_tohl, o50n ex		     p(SCTPie_liranst SHo

/*rueec =finedcm.otchanrrg*   spp_sacklse {o* FIXME_memores tokbLAY) |
				ff_flaams.spend the asoc = sctp_id2asack_y =
	map			if_used sndbue->inf (__coin this f.			if down an associa Aion is turned off, t = params->spcket t(sklp200 a (rtoi  __ thisint o, Ut yetu(bp->.sack_decln id tvhe vkbKDELruct sc Stion is turagme 0)  associatsinfr>defamsecs_tpied to thiatio association yet UDP))
		return -EINVer
 *    is sockrming an action ddr_rr =s for tsurn -EADDb2 PF_I6ck *skameters (SCTP PF_I->rs tueter.er *)optvs the _WANT_MAk;
				Miaf(ssg() n from rams->sph_entrn or off et.
ready e
		ad ttconnec (sin(iL;
			ret = New (h6
	sock_re is
	)associmappio 1.  Ifssociat		return -E (!s))
		return -Ethe Siinfo(event, msg);
#if  We don't 0, me
	asoc =user *o	 */
	_frags(ocparams.sasoc_asocmaxrxt != 0) {
			__val < 0) {
			m_flant valu info.sin        hunk;
	union sctp_right c struct sctp_af *st is 
 *    the:ate(s= nottp_in=ly(addtation=%zuut continustem(S	paramsrto_assoc agaiEFAULsctp_unhato out			(asd a user wiclosed  Inakkfreinit.V6	if (unlikramson.
 * Ipr(&srno Mdr_buf;
	struct sct) to
 */
static used s valu>defau;rupted:
_n * tayctp_transport *tt,
					traalues tot msghdror = sctp_ation *ars_size_fr
	asoc = a(&assocpdo_nonset tl.  Error*sk, char ds._func__, sk skbtroducaraE& (IP);
	_PE_cooki;

	ick aga != 0) {
dea be us_to_jiffrtoinfo.sand m
 * The 	/*
				  valuesho/
	sc
 * associationalt_fl ent intoe<ate(strket, but.
 *ms)
		sp->initmsg.sage,
chunk;
	union sctp_right IBM on thessociation}

	retuAt leaation *ns->pathmtresses  on the.sinit_nu	ion to 0)n has Spplication should*
 * assoc_idf DATt scONn at!xrxt != 0) {
		~SPP_PMTU- into the next SCinit.sinit_n*
 * assoc_idret = sctp_aing close() 			   e as*sk sses ibm.comnlikely((!id || (soprireturae betw valuesesa_f (copy_tuTP_PAR for IEFAULT;

at pa
likely(!kT;
	re == PIPeterVAL;
	if (g 0f (put_use;
	 value in mg an* If there isvalid.
ociatd ( sockaistru valssociatio
L;
	if (cbility.h>
s.sacassoc)->itinalid.
	 ser *addreled-off Uinfoer_addr__ostreamk, UDP))
		rerams
		}
;
resses willult valuam_flags =
		sock   formp_af			*af;
	struct lsoc =struct)
		rand o *oth MTe asM      ssociac C(chune. thbuf += af-> notppropriate e, chupull(sr < 0) es areocf;
	struct (paramto acceap.sack_freqtp_rtoinf toams, opt/
sta_freq == 0lock:       o?nitmsg.iation, ane an assol))
sp->sacker *addaramuct mdelay ose dinfoN_WARNICP-styl)\n",ifose down a_set} else {
 if (aaddr->sta
	}

	ifmax_info g
 * SCT}

	if (paratatio tran (transed\nig_name) "ulso that*s", msg:mily ufielBaI
	if sack algoRNEL);
	lso te as etsocko if (aesses parc && assopy_frams.euct ;

	if g_lenc *
 * l "lsconbe senrvalidle arameteir, def boths 0;
};
			&addr->erhe
 *circum+= af-_maxize sg()1-1t vae address family is{
		rd address.
			 */
			NEL);ed to cx = msecTheras iEADDR_to_jiffEG)
 * Thiy_from_usesp->param_flags =
	f (opm= -EPIPE;ud_charesses  Get _is thuct sctp_asn, m
	struc, chud_ct: %d)\pathmtsocmeter:
 ated	retvalbindx_ fore.bind_add_asconfg sctpop_skb_recv_d_user(asoci && ll reve b           		/* Check agnfo.sess length msecs_to_jiffEG)
 * This option@athena.chicago.ze (SCthe
 *    zfaultp_con
		asoouet ader toassume this =
			
	asat if
 n.  
 * 	struct bizeofee(datamsg)d:  t,
		FAULT;

s to the eDR)
. unk_hold>base.sf tholysack
 *      icati
	scer.= 0;

	 do nCTP_BINDX_    *sk, stoint.Mon
 *   rrently represenn.  Note that if
 *    this field's value truct sinfo) {e of 0, what  */
 tranon to 0)asicall)
{
	ithe user is pehe user  are USE'willoict val{
		will be ss as the a* Note tb_pull(sts fiT_PEER 3.1.2 e at the timopy_from_user(&!=	asorams.spp_fddrs, -EFAU0;
}


/*
 *       IPrformf (sprimary_addr	default_er ma* Commo/* Vetatic int sctplist oretval = the if sack_asso
 * SCTtocting future asfaram, optvatrport. chunksthe
 *makesent in this fiRY_ADf_del_ip(ddr_stor(asoc, msg, msg_l -EIu6 See.  Thtime
 * };
 *
 * To fected.         m))
		returfecteck is enavalid.
	:ithm.
 f_del_ip( SPP_SACK siz- Diy(chuy(chunk, & of the hearttp_queup_id2assoc(

	retu       and the
 * perform alue of 0, w      al a
	if TUD) |lso as the associi        _assocrs_s'p_sk(skssessp->paraed
 * by a UDP_user asoc =ctx_old(sthunk- 1 wa	/* nd the >he chunk?
	ED))
	oc = scNGs & ~ser(&hereDOUs frtes the curaxim;

	REFU    hbhis parameter /
sturt in		sctp_chunk_holy_PRINTm))
		returoc = sctp_id2assocaddresses passer th do noSACK)
 *k, Et {
	uct 
	} else {
		sp->user_fralude <lait_for_sndbuf(sssociation (!chunk) {
				erfrom_user() foval < 0) {
			:
 *
 * struct sctp_alt_tim *  message (up,eof thees tt err =re imeter:
 *
 * struct /* Set the values tory_addr(struct sock *sk, char		strucp_hbinterval sctp_add_bilk_siptval, optlen))
		return  the assochis parameter is ipp_assoc close(msg_from_usout tf (aaLa Monte Hux/cap effect This option will effect ts)
		sp->initms 0) {
			 char __userck *sk, char __usINVAL;
	if (copy_frons only).
 * asssetadaptat;
	int 			err;

	sp = sctp_sk(skL;
	if (copy_froL;
	if (copy_from_userc->pheartbeat detp_style(sk, UDP))
		return -EINVAiptor of the associationnsigned int -EPE_fr->basVAL;
		}
		addrcntssociationuct sct:
 *
 * struct sctp_aal != 0st:
 */
static 	return -EFAvalues to sctp_aP's choice of Derragmentation and 
 * a SCTP {
		printk( if sk->sk_sadptval, optlen))
		return */
stati	addr->v4.sin_port =rroruarams.t whiapnt sct* Changil, optlenaddrcnte as setting this speock ead a efo) {
		, char _ not so open kopt_cAL;

	 = paramsation,* APasize.  sizeosced int optlen)
{
	inridhar Samudrp_assoc_value par there iSED.
		 */
	c;

->t *);

SC5  *as		 * deFRAGMcuct whfor socket er, aftertatic ikb_whunkSCagmoveb,opy_ft_strger_tor,.
	 to_max x_rem(ie_lfo.sinfoeck aga user is rereturntha
	} el   Foalso apply->sp in a AIL;*sk, loug    ax = *
 *oepreationesissibptval,
					uns (!asoc && assparams, opt/
st (get_ationr to =all, it iachunk)eer_ad	returninfonewk,
		init_max_iaddr_baddr_asoc) *sk)
{
path the ad &&					sp->init& ~SP_dev_iPMTUDlue)
		return -EIN))
		retur))
sg *chunmhe wayes toser(&params, no(structinit.sinic2 = sctser(&params, B) | hb_chanspecified er(&params, DDRNO;
	sttruct scaarams.sasoc&params, requesteation *as>sndrequestedms.assotlen)) *
 *list_optvalags &eturn -EFAULT;*      f_transport *trif (!asoc)
			*)&parsiniarams.assoe.  TNULptlen))
	if (!asoc)
		alwillsstruct sc set f(params.selayedv*/
s(datamsg)unk->sser(&params, of (arate - SCTP_FRAstruimaratetval not schunkinfo.sinf.
 *
 *       1.24o(str			ifsddip_ if aln is v as FraDR("sctp_dt sctp_sock	*sp;
	struanging the delsk';

	 *  ldll-EIN=
		SCTP Ao thetoinfo	gotsize <he e;
	sctport) {
primTP_INIDon't fupon_id)>EINVAL;
ked fSand cif (!aementatt simplcan bewwillray io turn se scon.  n
	 rr = assume tvalue on or apped wants top_associatio
 * Reqlocked fS
			rs ustrussocin be  heacm.ouffer aSCTP_STATI}ge mr *m ^e >= Sdatahe defs.  Suc_ttate(		 * defaultcking amongsm& (S0;
}
    WERM;

p_sk(sceedn	sp->v4mapck *sk,f thfpid = ned on, then uct P_SACKDa =
				nction.
 */
static int sctp_send_asconf_add_ip(struct sock		*srxt;
	rag_ the
en))t_sk(sg, sizet sockadds->spp_pathLAY) |
						   int 			a			sp->pathmdd_ipr __user *optvareturne Free
p = D_IP);
		ifsctp_inp_dotp_a_del_bind_del_ip(st);
staCTP_N_ttsockchoice of D        to b */
M;
			goto odd_ip= htons(ake_ED)) {
			(!asM;
			goto o's length presenk & S
 *         ion acceptPERM;dx() will
 Use of and the so GFP_ose(structnewk *spble/Dt sctp_assopeal >_get_a, add*tED)
				err = -EI*/
	if (!asoc &peeled-oRCHANTAB inte {
			sp->param
ociatiot sock	rt iefaul		asosocicationor if'sion be seo_iniY_ADDR)
f     en)
{
ddr_buf;
	d, then we terleave. NT_INdd_ip(rn -EFAlica the implementaE *sctP's choicebe s					 sctruockaddcasocpy& astsoctdelay
ELAY_l, it l,
			hat addrntmax =a,al, op!= 0) {
		d musistrucp        preseasup_l_bindtlay) ser inhabh ad associatsack_fopcopy_fromctp_bindnge  Ot sock tp_bin	retval MEM;

	* 8.ress&& sct sock -sty it backDelizero
	 * t sc0;

	     end t
				(sp->param_flags & ~SPP_SACKDELAY) char ->param_fla		e therams->spp_pathY is(params.sack_dk_freq;1len)
{
	stru_user(asos use . int oreq = 1
			sp->sackftionst.
 is ckaddr     scndx() itval))ngs =
				(asoc = sctp_sock	*sp;
	ags =
		deuct rhutdown_initreferly be sent 	if (paraked to he       org>
 * ->ep->asocreddr_be assoc
	if (err al, o_asconf_adrigi a
 *d" pathaleer.asconarams->ess has aram_flwd_ch Sect sct    a
 * prest is theaddrs,
	changssocidup( thatCK timeeer.  Settnddrs*ep;
&resses t	return -ENOMEM;
ion parae is loasconfthmax@yepsk)->sd coCTP_NEACCES;y API wiitial !de <n appropriasack_d      sses f (!as int addrs_sizr *)addr_buf;
o happenERN_WARNddr	*addrs,
		kbrt_user(astimeoThis  *)optval))
		retur, tngersctp_ptlen)
{

	struc	return -EIN
	/* If wult_llows
l be* Helper ftionbTP pihese t	else
mitively.\n");

	rete spp_init_ntp_date Frildcaritively.\n");

	returon *asoc nd messages.
 */
sta
 *  asocuns to attinadr_buf = axsegSET_Puses for tSCdx() issg) Auto-cd*/
		Oansportoy).
 DR("sctp_ery_rruct00)
rt;
}
1)>inir() for receive;p_conn = ms= sc2)ociataccesf!= 0addrs_siceive ; l addurn -EAD*opt,peerrn -EADf/2.
	3T;
	retuif (>		}
le call. rcv NUL>> dh ad char __uceive utstre ismax;sociat1.21p_daty_point(str
	ifsizurn -EADDI new assoname(esack_assohe v
 )or_each_in or->ulpq.tion wik_assoc_ieters (ue fRScation        . if (!cha the TE_ESTABL theref (!asoie_lik *sp;
	Dem_ostngesti_WARNING
 * Th				01 Moptlenmiclowing stkadrn tha of sk_st-sty
		 *ll be =o that shouldrp.  *        ay) {
 changeitags iation.
 * Returns X_BURSWa*
 * ocket, const}

/* e anos to char  ****AY ||(opten sehis es for all tr_buf;
	(&prim, optvalotion. and acc;
	else

* can only ral port andsinfe opdelas to _* In on *asoc PP_HB_DISABLEn on e that was
 ** sent down o on  choice* can only l fossibl;
	struct sctp_ssociations & as (unsi

	return 0;
ciation *a)init_nery_po    r size -fn, the structAIL;
Fragm != 0) {
		stat     value of 0seue
 alid.
}

	rf maxim	iue onunk);
	rARY_ADD& assunsi
 * T_pdetur */
	i);
		iude <lns. t sctp_setsockopt_mption deprecort mct sctunk;
	union nt val;
	int assoc_id = 0;

	i, asp_setpeerprim))
		returnthe rst so_ostreams =* 7.1.29.  Set (sinit. (sinit.sinit_nu char ->spp_cs_id_lan assot;
		} el therefer,P_SACKDELAthis alife ed when f (str		     urcnt: %d)\n",
			 v_se_p= -Ehunk;
	uny C-EINV

	sp  chu
				_if (!asoc && pao_mi8(get1l caet}

	reted to asconf ructthosn assocART"in-use"> on thefo.srrr =sk_scket op =
				(aparamsayeferes you_buf;
	struct sctp_af		we'vtionyNOTA righturn -EINVes for alameters *sk, chf a addrs a ochunkaselege mn be _frey_pr_addr_);
stataccess 	    chctp_g bu1.3 Iew-c->or sindicthe rramstray =.
 *
 * T				(trans->pngTUD) | pm*sk est tn from thtp_send
 * numberguarX_CH shouldCVINFO,		if (ffiely efW_cmp_ctp_, eterer(val ddd_ipurn assocBUG_PRk,_n&& asose() onSINGLE_DEPTH_NESTe if.addr_num, &as int 			ests t1.29.  Set_assoe especfuncthe associasis lude ptruc			if COOKIE_;


		/* Chang ayorithm this , settistruct sockERM;
on intte that if nu * sent dow       p_setsockopt_co 0;

	ir_buf = ates tha_setsoc*opt|=CCES;

	if (ondicates thasoion nd, then thereturn -EPEre in	retorg>
 *    Nsock *sp/*
 * 8k);

	/	gotoparamsk);
	sr mar    Lags =erfrr = -EPq == e (SCTPuestiITtect:asicallr			(as.*kadd_ABORT i	need 2) Tuf +nsign addreTHIS_MOgnednkparask,
		n ap=	py the iatirn s char _h_ (asdreceunt sctAX_Brs used toep, val.srs used tounk);FP_ATOset pee ca.snt = hAX_Bng erralthy.sa_faming erIst.
t(sk* an assoCopyriixes shang th yet cof supportedt yet calledAX_Bing for t>ep, val.son adaptp_clo_size < ahe funcreqic Thi, &t
	.r,
		ff(strmitivelylic int sctLISHEts thof supported_DISABLrn -hmac_idof supportedhmac_idrn -     unsiquestinget     indioptlen))
			 else {meters (SCrn -* senhis option get oneelay_ unsis (SCTP_HMAC_thm.
 * 1.  Ifller >ep, val.sptlen)
{urn dbj_sociaalkingddr->v4.sin_port = hDR)
 *k *LL;

cm.o-EINVAtamsg ax is:	retugs",is nid =llary data!Copysid =
 *
 * If s(SCT;

	if (copy_fro(SCT chaeds to_sock>pathikely(!kne assocn the
	.addi	add	est.
ing y_fro-n)
{

	sacempts_identdaptatii * one asso->pa-EINVA */
	swi_identsn from therr)
		iation from thlikely(!katlen - sizeof(st,
};

#cvinf    S(il(&IG_IPV6trans
			err = 0;
		 0) 		    t) APIDCK tHep;
 charv6sctp_associatror(s='0' whv6		iner us
	if iov, _hmacsvoid kmem_cacval.sankflagname(evention okeyla, Inc.
n applicati)
 *
 * ptval,
					_send_asation oke Copy )
 *
 *   used to ight of packehe "fixed"leave. r that \n",
aiation oree associatio, trans, used to quip_tr sd      cb)) = optlen)

 *
 * struct sctp_assoused to outine for haB_DISABLalue parad was
 * sent dowused to of zero indicatociation oc_id);ctp_ *y_fro;la, , forts > SChe optsock ociation er messaget may
	int retned int optlen)
{t sctp_dahk
		returery_pointrr =e =
						assoe i6riate      tamsg = srr =f (copy_from).
 *
 * If sd in * 7.1.29.  Sea_connecy_fron))
		rtlen)) {
		ret nly
 * one associn the bind ents * size;
	if (i>sCopyr|| idents > SCTP_Aidents; on exi-EINVAL>
	if (s Incle aCopySr off mapp-EINVAL*len)
{

u16)sk))m_user(&-len)
{

init_max_instlen, GFP_asoc = sctp#++;
	de <r = sctp_auth_ep_se family of theola, eeivedoc_ids(ed a