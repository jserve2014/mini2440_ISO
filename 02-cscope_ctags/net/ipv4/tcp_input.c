/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Implementation of the Transmission Control Protocol(TCP).
 *
 * Authors:	Ross Biro
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Mark Evans, <evansmp@uhura.aston.ac.uk>
 *		Corey Minyard <wf-rch!minyard@relay.EU.net>
 *		Florian La Roche, <flla@stud.uni-sb.de>
 *		Charles Hedrick, <hedrick@klinzhai.rutgers.edu>
 *		Linus Torvalds, <torvalds@cs.helsinki.fi>
 *		Alan Cox, <gw4pts@gw4pts.ampr.org>
 *		Matthew Dillon, <dillon@apollo.west.oic.com>
 *		Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *		Jorge Cwik, <jorge@laser.satlink.net>
 */

/*
 * Changes:
 *		Pedro Roque	:	Fast Retransmit/Recovery.
 *					Two receive queues.
 *					Retransmit queue handled by TCP.
 *					Better retransmit timer handling.
 *					New congestion avoidance.
 *					Header prediction.
 *					Variable renaming.
 *
 *		Eric		:	Fast Retransmit.
 *		Randy Scott	:	MSS option defines.
 *		Eric Schenk	:	Fixes to slow start algorithm.
 *		Eric Schenk	:	Yet another double ACK bug.
 *		Eric Schenk	:	Delayed ACK bug fixes.
 *		Eric Schenk	:	Floyd style fast retrans war avoidance.
 *		David S. Miller	:	Don't allow zero congestion window.
 *		Eric Schenk	:	Fix retransmitter so that it sends
 *					next packet on ack of previous packet.
 *		Andi Kleen	:	Moved open_request checking here
 *					and process RSTs for open_requests.
 *		Andi Kleen	:	Better prune_queue, and other fixes.
 *		Andrey Savochkin:	Fix RTT measurements in the presence of
 *					timestamps.
 *		Andrey Savochkin:	Check sequence numbers correctly when
 *					removing SACKs due to in sequence incoming
 *					data segments.
 *		Andi Kleen:		Make sure we never ack data there is not
 *					enough room for. Also make this condition
 *					a fatal error if it might still happen.
 *		Andi Kleen:		Add tcp_measure_rcv_mss to make
 *					connections with MSS<min(MTU,ann. MSS)
 *					work without delayed acks.
 *		Andi Kleen:		Process packets with PSH set in the
 *					fast path.
 *		J Hadi Salim:		ECN support
 *	 	Andrei Gurtov,
 *		Pasi Sarolahti,
 *		Panu Kuhlberg:		Experimental audit of TCP (re)transmission
 *					engine. Lots of bugs are found.
 *		Pasi Sarolahti:		F-RTO for dealing with spurious RTOs
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sysctl.h>
#include <linux/kernel.h>
#include <net/dst.h>
#include <net/tcp.h>
#include <net/inet_common.h>
#include <linux/ipsec.h>
#include <asm/unaligned.h>
#include <net/netdma.h>

int sysctl_tcp_timestamps __read_mostly = 1;
int sysctl_tcp_window_scaling __read_mostly = 1;
int sysctl_tcp_sack __read_mostly = 1;
int sysctl_tcp_fack __read_mostly = 1;
int sysctl_tcp_reordering __read_mostly = TCP_FASTRETRANS_THRESH;
int sysctl_tcp_ecn __read_mostly = 2;
int sysctl_tcp_dsack __read_mostly = 1;
int sysctl_tcp_app_win __read_mostly = 31;
int sysctl_tcp_adv_win_scale __read_mostly = 2;

int sysctl_tcp_stdurg __read_mostly;
int sysctl_tcp_rfc1337 __read_mostly;
int sysctl_tcp_max_orphans __read_mostly = NR_FILE;
int sysctl_tcp_frto __read_mostly = 2;
int sysctl_tcp_frto_response __read_mostly;
int sysctl_tcp_nometrics_save __read_mostly;

int sysctl_tcp_moderate_rcvbuf __read_mostly = 1;
int sysctl_tcp_abc __read_mostly;

#define FLAG_DATA		0x01 /* Incoming frame contained data.		*/
#define FLAG_WIN_UPDATE		0x02 /* Incoming ACK was a window update.	*/
#define FLAG_DATA_ACKED		0x04 /* This ACK acknowledged new data.		*/
#define FLAG_RETRANS_DATA_ACKED	0x08 /* "" "" some of which was retransmitted.	*/
#define FLAG_SYN_ACKED		0x10 /* This ACK acknowledged SYN.		*/
#define FLAG_DATA_SACKED	0x20 /* New SACK.				*/
#define FLAG_ECE		0x40 /* ECE in this ACK				*/
#define FLAG_DATA_LOST		0x80 /* SACK detected data lossage.		*/
#define FLAG_SLOWPATH		0x100 /* Do not skip RFC checks for window update.*/
#define FLAG_ONLY_ORIG_SACKED	0x200 /* SACKs only non-rexmit sent before RTO */
#define FLAG_SND_UNA_ADVANCED	0x400 /* Snd_una was changed (!= FLAG_DATA_ACKED) */
#define FLAG_DSACKING_ACK	0x800 /* SACK blocks contained D-SACK info */
#define FLAG_NONHEAD_RETRANS_ACKED	0x1000 /* Non-head rexmitted data was ACKed */
#define FLAG_SACK_RENEGING	0x2000 /* snd_una advanced to a sacked seq */

#define FLAG_ACKED		(FLAG_DATA_ACKED|FLAG_SYN_ACKED)
#define FLAG_NOT_DUP		(FLAG_DATA|FLAG_WIN_UPDATE|FLAG_ACKED)
#define FLAG_CA_ALERT		(FLAG_DATA_SACKED|FLAG_ECE)
#define FLAG_FORWARD_PROGRESS	(FLAG_ACKED|FLAG_DATA_SACKED)
#define FLAG_ANY_PROGRESS	(FLAG_FORWARD_PROGRESS|FLAG_SND_UNA_ADVANCED)

#define TCP_REMNANT (TCP_FLAG_FIN|TCP_FLAG_URG|TCP_FLAG_SYN|TCP_FLAG_PSH)
#define TCP_HP_BITS (~(TCP_RESERVED_BITS|TCP_FLAG_PSH))

/* Adapt the MSS value used to make delayed ack decision to the
 * real world.
 */
static void tcp_measure_rcv_mss(struct sock *sk, const struct sk_buff *skb)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	const unsigned int lss = icsk->icsk_ack.last_seg_size;
	unsigned int len;

	icsk->icsk_ack.last_seg_size = 0;

	/* skb->len may jitter because of SACKs, even if peer
	 * sends good full-sized frames.
	 */
	len = skb_shinfo(skb)->gso_size ? : skb->len;
	if (len >= icsk->icsk_ack.rcv_mss) {
		icsk->icsk_ack.rcv_mss = len;
	} else {
		/* Otherwise, we make more careful check taking into account,
		 * that SACKs block is variable.
		 *
		 * "len" is invariant segment length, including TCP header.
		 */
		len += skb->data - skb_transport_header(skb);
		if (len >= TCP_MIN_RCVMSS + sizeof(struct tcphdr) ||
		    /* If PSH is not set, packet should be
		     * full sized, provided peer TCP is not badly broken.
		     * This observation (if it is correct 8)) allows
		     * to handle super-low mtu links fairly.
		     */
		    (len >= TCP_MIN_MSS + sizeof(struct tcphdr) &&
		     !(tcp_flag_word(tcp_hdr(skb)) & TCP_REMNANT))) {
			/* Subtract also invariant (if peer is RFC compliant),
			 * tcp header plus fixed timestamp option length.
			 * Resulting "len" is MSS free of SACK jitter.
			 */
			len -= tcp_sk(sk)->tcp_header_len;
			icsk->icsk_ack.last_seg_size = len;
			if (len == lss) {
				icsk->icsk_ack.rcv_mss = len;
				return;
			}
		}
		if (icsk->icsk_ack.pending & ICSK_ACK_PUSHED)
			icsk->icsk_ack.pending |= ICSK_ACK_PUSHED2;
		icsk->icsk_ack.pending |= ICSK_ACK_PUSHED;
	}
}

static void tcp_incr_quickack(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	unsigned quickacks = tcp_sk(sk)->rcv_wnd / (2 * icsk->icsk_ack.rcv_mss);

	if (quickacks == 0)
		quickacks = 2;
	if (quickacks > icsk->icsk_ack.quick)
		icsk->icsk_ack.quick = min(quickacks, TCP_MAX_QUICKACKS);
}

void tcp_enter_quickack_mode(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	tcp_incr_quickack(sk);
	icsk->icsk_ack.pingpong = 0;
	icsk->icsk_ack.ato = TCP_ATO_MIN;
}

/* Send ACKs quickly, if "quick" count is not exhausted
 * and the session is not interactive.
 */

static inline int tcp_in_quickack_mode(const struct sock *sk)
{
	const struct inet_connection_sock *icsk = inet_csk(sk);
	return icsk->icsk_ack.quick && !icsk->icsk_ack.pingpong;
}

static inline void TCP_ECN_queue_cwr(struct tcp_sock *tp)
{
	if (tp->ecn_flags & TCP_ECN_OK)
		tp->ecn_flags |= TCP_ECN_QUEUE_CWR;
}

static inline void TCP_ECN_accept_cwr(struct tcp_sock *tp, struct sk_buff *skb)
{
	if (tcp_hdr(skb)->cwr)
		tp->ecn_flags &= ~TCP_ECN_DEMAND_CWR;
}

static inline void TCP_ECN_withdraw_cwr(struct tcp_sock *tp)
{
	tp->ecn_flags &= ~TCP_ECN_DEMAND_CWR;
}

static inline void TCP_ECN_check_ce(struct tcp_sock *tp, struct sk_buff *skb)
{
	if (tp->ecn_flags & TCP_ECN_OK) {
		if (INET_ECN_is_ce(TCP_SKB_CB(skb)->flags))
			tp->ecn_flags |= TCP_ECN_DEMAND_CWR;
		/* Funny extension: if ECT is not set on a segment,
		 * it is surely retransmit. It is not in ECN RFC,
		 * but Linux follows this rule. */
		else if (INET_ECN_is_not_ect((TCP_SKB_CB(skb)->flags)))
			tcp_enter_quickack_mode((struct sock *)tp);
	}
}

static inline void TCP_ECN_rcv_synack(struct tcp_sock *tp, struct tcphdr *th)
{
	if ((tp->ecn_flags & TCP_ECN_OK) && (!th->ece || th->cwr))
		tp->ecn_flags &= ~TCP_ECN_OK;
}

static inline void TCP_ECN_rcv_syn(struct tcp_sock *tp, struct tcphdr *th)
{
	if ((tp->ecn_flags & TCP_ECN_OK) && (!th->ece || !th->cwr))
		tp->ecn_flags &= ~TCP_ECN_OK;
}

static inline int TCP_ECN_rcv_ecn_echo(struct tcp_sock *tp, struct tcphdr *th)
{
	if (th->ece && !th->syn && (tp->ecn_flags & TCP_ECN_OK))
		return 1;
	return 0;
}

/* Buffer size and advertised window tuning.
 *
 * 1. Tuning sk->sk_sndbuf, when connection enters established state.
 */

static void tcp_fixup_sndbuf(struct sock *sk)
{
	int sndmem = tcp_sk(sk)->rx_opt.mss_clamp + MAX_TCP_HEADER + 16 +
		     sizeof(struct sk_buff);

	if (sk->sk_sndbuf < 3 * sndmem)
		sk->sk_sndbuf = min(3 * sndmem, sysctl_tcp_wmem[2]);
}

/* 2. Tuning advertised window (window_clamp, rcv_ssthresh)
 *
 * All tcp_full_space() is split to two parts: "network" buffer, allocated
 * forward and advertised in receiver window (tp->rcv_wnd) and
 * "application buffer", required to isolate scheduling/application
 * latencies from network.
 * window_clamp is maximal advertised window. It can be less than
 * tcp_full_space(), in this case tcp_full_space() - window_clamp
 * is reserved for "application" buffer. The less window_clamp is
 * the smoother our behaviour from viewpoint of network, but the lower
 * throughput and the higher sensitivity of the connection to losses. 8)
 *
 * rcv_ssthresh is more strict window_clamp used at "slow start"
 * phase to predict further behaviour of this connection.
 * It is used for two goals:
 * - to enforce header prediction at sender, even when application
 *   requires some significant "application buffer". It is check #1.
 * - to prevent pruning of receive queue because of misprediction
 *   of receiver window. Check #2.
 *
 * The scheme does not work when sender sends good segments opening
 * window and then starts to feed us spaghetti. But it should work
 * in common situations. Otherwise, we have to rely on queue collapsing.
 */

/* Slow part of check#2. */
static int __tcp_grow_window(const struct sock *sk, const struct sk_buff *skb)
{
	struct tcp_sock *tp = tcp_sk(sk);
	/* Optimize this! */
	int truesize = tcp_win_from_space(skb->truesize) >> 1;
	int window = tcp_win_from_space(sysctl_tcp_rmem[2]) >> 1;

	while (tp->rcv_ssthresh <= window) {
		if (truesize <= skb->len)
			return 2 * inet_csk(sk)->icsk_ack.rcv_mss;

		truesize >>= 1;
		window >>= 1;
	}
	return 0;
}

static void tcp_grow_window(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_sock *tp = tcp_sk(sk);

	/* Check #1 */
	if (tp->rcv_ssthresh < tp->window_clamp &&
	    (int)tp->rcv_ssthresh < tcp_space(sk) &&
	    !tcp_memory_pressure) {
		int incr;

		/* Check #2. Increase window, if skb with such overhead
		 * will fit to rcvbuf in future.
		 */
		if (tcp_win_from_space(skb->truesize) <= skb->len)
			incr = 2 * tp->advmss;
		else
			incr = __tcp_grow_window(sk, skb);

		if (incr) {
			tp->rcv_ssthresh = min(tp->rcv_ssthresh + incr,
					       tp->window_clamp);
			inet_csk(sk)->icsk_ack.quick |= 1;
		}
	}
}

/* 3. Tuning rcvbuf, when connection enters established state. */

static void tcp_fixup_rcvbuf(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	int rcvmem = tp->advmss + MAX_TCP_HEADER + 16 + sizeof(struct sk_buff);

	/* Try to select rcvbuf so that 4 mss-sized segments
	 * will fit to window and corresponding skbs will fit to our rcvbuf.
	 * (was 3; 4 is minimum to allow fast retransmit to work.)
	 */
	while (tcp_win_from_space(rcvmem) < tp->advmss)
		rcvmem += 128;
	if (sk->sk_rcvbuf < 4 * rcvmem)
		sk->sk_rcvbuf = min(4 * rcvmem, sysctl_tcp_rmem[2]);
}

/* 4. Try to fixup all. It is made immediately after connection enters
 *    established state.
 */
static void tcp_init_buffer_space(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	int maxwin;

	if (!(sk->sk_userlocks & SOCK_RCVBUF_LOCK))
		tcp_fixup_rcvbuf(sk);
	if (!(sk->sk_userlocks & SOCK_SNDBUF_LOCK))
		tcp_fixup_sndbuf(sk);

	tp->rcvq_space.space = tp->rcv_wnd;

	maxwin = tcp_full_space(sk);

	if (tp->window_clamp >= maxwin) {
		tp->window_clamp = maxwin;

		if (sysctl_tcp_app_win && maxwin > 4 * tp->advmss)
			tp->window_clamp = max(maxwin -
					       (maxwin >> sysctl_tcp_app_win),
					       4 * tp->advmss);
	}

	/* Force reservation of one segment. */
	if (sysctl_tcp_app_win &&
	    tp->window_clamp > 2 * tp->advmss &&
	    tp->window_clamp + tp->advmss > maxwin)
		tp->window_clamp = max(2 * tp->advmss, maxwin - tp->advmss);

	tp->rcv_ssthresh = min(tp->rcv_ssthresh, tp->window_clamp);
	tp->snd_cwnd_stamp = tcp_time_stamp;
}

/* 5. Recalculate window clamp after socket hit its memory bounds. */
static void tcp_clamp_window(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);

	icsk->icsk_ack.quick = 0;

	if (sk->sk_rcvbuf < sysctl_tcp_rmem[2] &&
	    !(sk->sk_userlocks & SOCK_RCVBUF_LOCK) &&
	    !tcp_memory_pressure &&
	    atomic_read(&tcp_memory_allocated) < sysctl_tcp_mem[0]) {
		sk->sk_rcvbuf = min(atomic_read(&sk->sk_rmem_alloc),
				    sysctl_tcp_rmem[2]);
	}
	if (atomic_read(&sk->sk_rmem_alloc) > sk->sk_rcvbuf)
		tp->rcv_ssthresh = min(tp->window_clamp, 2U * tp->advmss);
}

/* Initialize RCV_MSS value.
 * RCV_MSS is an our guess about MSS used by the peer.
 * We haven't any direct information about the MSS.
 * It's better to underestimate the RCV_MSS rather than overestimate.
 * Overestimations make us ACKing less frequently than needed.
 * Underestimations are more easy to detect and fix by tcp_measure_rcv_mss().
 */
void tcp_initialize_rcv_mss(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	unsigned int hint = min_t(unsigned int, tp->advmss, tp->mss_cache);

	hint = min(hint, tp->rcv_wnd / 2);
	hint = min(hint, TCP_MIN_RCVMSS);
	hint = max(hint, TCP_MIN_MSS);

	inet_csk(sk)->icsk_ack.rcv_mss = hint;
}

/* Receiver "autotuning" code.
 *
 * The algorithm for RTT estimation w/o timestamps is based on
 * Dynamic Right-Sizing (DRS) by Wu Feng and Mike Fisk of LANL.
 * <http://www.lanl.gov/radiant/website/pubs/drs/lacsi2001.ps>
 *
 * More detail on this code can be found at
 * <http://www.psc.edu/~jheffner/senior_thesis.ps>,
 * though this reference is out of date.  A new paper
 * is pending.
 */
static void tcp_rcv_rtt_update(struct tcp_sock *tp, u32 sample, int win_dep)
{
	u32 new_sample = tp->rcv_rtt_est.rtt;
	long m = sample;

	if (m == 0)
		m = 1;

	if (new_sample != 0) {
		/* If we sample in larger samples in the non-timestamp
		 * case, we could grossly overestimate the RTT especially
		 * with chatty applications or bulk transfer apps which
		 * are stalled on filesystem I/O.
		 *
		 * Also, since we are only going for a minimum in the
		 * non-timestamp case, we do not smooth things out
		 * else with timestamps disabled convergence takes too
		 * long.
		 */
		if (!win_dep) {
			m -= (new_sample >> 3);
			new_sample += m;
		} else if (m < new_sample)
			new_sample = m << 3;
	} else {
		/* No previous measure. */
		new_sample = m << 3;
	}

	if (tp->rcv_rtt_est.rtt != new_sample)
		tp->rcv_rtt_est.rtt = new_sample;
}

static inline void tcp_rcv_rtt_measure(struct tcp_sock *tp)
{
	if (tp->rcv_rtt_est.time == 0)
		goto new_measure;
	if (before(tp->rcv_nxt, tp->rcv_rtt_est.seq))
		return;
	tcp_rcv_rtt_update(tp, jiffies - tp->rcv_rtt_est.time, 1);

new_measure:
	tp->rcv_rtt_est.seq = tp->rcv_nxt + tp->rcv_wnd;
	tp->rcv_rtt_est.time = tcp_time_stamp;
}

static inline void tcp_rcv_rtt_measure_ts(struct sock *sk,
					  const struct sk_buff *skb)
{
	struct tcp_sock *tp = tcp_sk(sk);
	if (tp->rx_opt.rcv_tsecr &&
	    (TCP_SKB_CB(skb)->end_seq -
	     TCP_SKB_CB(skb)->seq >= inet_csk(sk)->icsk_ack.rcv_mss))
		tcp_rcv_rtt_update(tp, tcp_time_stamp - tp->rx_opt.rcv_tsecr, 0);
}

/*
 * This function should be called every time data is copied to user space.
 * It calculates the appropriate TCP receive buffer space.
 */
void tcp_rcv_space_adjust(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	int time;
	int space;

	if (tp->rcvq_space.time == 0)
		goto new_measure;

	time = tcp_time_stamp - tp->rcvq_space.time;
	if (time < (tp->rcv_rtt_est.rtt >> 3) || tp->rcv_rtt_est.rtt == 0)
		return;

	space = 2 * (tp->copied_seq - tp->rcvq_space.seq);

	space = max(tp->rcvq_space.space, space);

	if (tp->rcvq_space.space != space) {
		int rcvmem;

		tp->rcvq_space.space = space;

		if (sysctl_tcp_moderate_rcvbuf &&
		    !(sk->sk_userlocks & SOCK_RCVBUF_LOCK)) {
			int new_clamp = space;

			/* Receive space grows, normalize in order to
			 * take into account packet headers and sk_buff
			 * structure overhead.
			 */
			space /= tp->advmss;
			if (!space)
				space = 1;
			rcvmem = (tp->advmss + MAX_TCP_HEADER +
				  16 + sizeof(struct sk_buff));
			while (tcp_win_from_space(rcvmem) < tp->advmss)
				rcvmem += 128;
			space *= rcvmem;
			space = min(space, sysctl_tcp_rmem[2]);
			if (space > sk->sk_rcvbuf) {
				sk->sk_rcvbuf = space;

				/* Make the window clamp follow along.  */
				tp->window_clamp = new_clamp;
			}
		}
	}

new_measure:
	tp->rcvq_space.seq = tp->copied_seq;
	tp->rcvq_space.time = tcp_time_stamp;
}

/* There is something which you must keep in mind when you analyze the
 * behavior of the tp->ato delayed ack timeout interval.  When a
 * connection starts up, we want to ack as quickly as possible.  The
 * problem is that "good" TCP's do slow start at the beginning of data
 * transmission.  The means that until we send the first few ACK's the
 * sender will sit on his end and only queue most of his data, because
 * he can only send snd_cwnd unacked packets at any given time.  For
 * each ACK we send, he increments snd_cwnd and transmits more of his
 * queue.  -DaveM
 */
static void tcp_event_data_recv(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);
	u32 now;

	inet_csk_schedule_ack(sk);

	tcp_measure_rcv_mss(sk, skb);

	tcp_rcv_rtt_measure(tp);

	now = tcp_time_stamp;

	if (!icsk->icsk_ack.ato) {
		/* The _first_ data packet received, initialize
		 * delayed ACK engine.
		 */
		tcp_incr_quickack(sk);
		icsk->icsk_ack.ato = TCP_ATO_MIN;
	} else {
		int m = now - icsk->icsk_ack.lrcvtime;

		if (m <= TCP_ATO_MIN / 2) {
			/* The fastest case is the first. */
			icsk->icsk_ack.ato = (icsk->icsk_ack.ato >> 1) + TCP_ATO_MIN / 2;
		} else if (m < icsk->icsk_ack.ato) {
			icsk->icsk_ack.ato = (icsk->icsk_ack.ato >> 1) + m;
			if (icsk->icsk_ack.ato > icsk->icsk_rto)
				icsk->icsk_ack.ato = icsk->icsk_rto;
		} else if (m > icsk->icsk_rto) {
			/* Too long gap. Apparently sender failed to
			 * restart window, so that we send ACKs quickly.
			 */
			tcp_incr_quickack(sk);
			sk_mem_reclaim(sk);
		}
	}
	icsk->icsk_ack.lrcvtime = now;

	TCP_ECN_check_ce(tp, skb);

	if (skb->len >= 128)
		tcp_grow_window(sk, skb);
}

/* Called to compute a smoothed rtt estimate. The data fed to this
 * routine either comes from timestamps, or from segments that were
 * known _not_ to have been retransmitted [see Karn/Partridge
 * Proceedings SIGCOMM 87]. The algorithm is from the SIGCOMM 88
 * piece by Van Jacobson.
 * NOTE: the next three routines used to be one big routine.
 * To save cycles in the RFC 1323 implementation it was better to break
 * it up into three procedures. -- erics
 */
static void tcp_rtt_estimator(struct sock *sk, const __u32 mrtt)
{
	struct tcp_sock *tp = tcp_sk(sk);
	long m = mrtt; /* RTT */

	/*	The following amusing code comes from Jacobson's
	 *	article in SIGCOMM '88.  Note that rtt and mdev
	 *	are scaled versions of rtt and mean deviation.
	 *	This is designed to be as fast as possible
	 *	m stands for "measurement".
	 *
	 *	On a 1990 paper the rto value is changed to:
	 *	RTO = rtt + 4 * mdev
	 *
	 * Funny. This algorithm seems to be very broken.
	 * These formulae increase RTO, when it should be decreased, increase
	 * too slowly, when it should be increased quickly, decrease too quickly
	 * etc. I guess in BSD RTO takes ONE value, so that it is absolutely
	 * does not matter how to _calculate_ it. Seems, it was trap
	 * that VJ failed to avoid. 8)
	 */
	if (m == 0)
		m = 1;
	if (tp->srtt != 0) {
		m -= (tp->srtt >> 3);	/* m is now error in rtt est */
		tp->srtt += m;		/* rtt = 7/8 rtt + 1/8 new */
		if (m < 0) {
			m = -m;		/* m is now abs(error) */
			m -= (tp->mdev >> 2);   /* similar update on mdev */
			/* This is similar to one of Eifel findings.
			 * Eifel blocks mdev updates when rtt decreases.
			 * This solution is a bit different: we use finer gain
			 * for mdev in this case (alpha*beta).
			 * Like Eifel it also prevents growth of rto,
			 * but also it limits too fast rto decreases,
			 * happening in pure Eifel.
			 */
			if (m > 0)
				m >>= 3;
		} else {
			m -= (tp->mdev >> 2);   /* similar update on mdev */
		}
		tp->mdev += m;	    	/* mdev = 3/4 mdev + 1/4 new */
		if (tp->mdev > tp->mdev_max) {
			tp->mdev_max = tp->mdev;
			if (tp->mdev_max > tp->rttvar)
				tp->rttvar = tp->mdev_max;
		}
		if (after(tp->snd_una, tp->rtt_seq)) {
			if (tp->mdev_max < tp->rttvar)
				tp->rttvar -= (tp->rttvar - tp->mdev_max) >> 2;
			tp->rtt_seq = tp->snd_nxt;
			tp->mdev_max = tcp_rto_min(sk);
		}
	} else {
		/* no previous measure. */
		tp->srtt = m << 3;	/* take the measured time to be rtt */
		tp->mdev = m << 1;	/* make sure rto = 3*rtt */
		tp->mdev_max = tp->rttvar = max(tp->mdev, tcp_rto_min(sk));
		tp->rtt_seq = tp->snd_nxt;
	}
}

/* Calculate rto without backoff.  This is the second half of Van Jacobson's
 * routine referred to above.
 */
static inline void tcp_set_rto(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	/* Old crap is replaced with new one. 8)
	 *
	 * More seriously:
	 * 1. If rtt variance happened to be less 50msec, it is hallucination.
	 *    It cannot be less due to utterly erratic ACK generation made
	 *    at least by solaris and freebsd. "Erratic ACKs" has _nothing_
	 *    to do with delayed acks, because at cwnd>2 true delack timeout
	 *    is invisible. Actually, Linux-2.4 also generates erratic
	 *    ACKs in some circumstances.
	 */
	inet_csk(sk)->icsk_rto = __tcp_set_rto(tp);

	/* 2. Fixups made earlier cannot be right.
	 *    If we do not estimate RTO correctly without them,
	 *    all the algo is pure shit and should be replaced
	 *    with correct one. It is exactly, which we pretend to do.
	 */

	/* NOTE: clamping at TCP_RTO_MIN is not required, current algo
	 * guarantees that rto is higher.
	 */
	tcp_bound_rto(sk);
}

/* Save metrics learned by this TCP session.
   This function is called only, when TCP finishes successfully
   i.e. when it enters TIME-WAIT or goes from LAST-ACK to CLOSE.
 */
void tcp_update_metrics(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct dst_entry *dst = __sk_dst_get(sk);

	if (sysctl_tcp_nometrics_save)
		return;

	dst_confirm(dst);

	if (dst && (dst->flags & DST_HOST)) {
		const struct inet_connection_sock *icsk = inet_csk(sk);
		int m;
		unsigned long rtt;

		if (icsk->icsk_backoff || !tp->srtt) {
			/* This session failed to estimate rtt. Why?
			 * Probably, no packets returned in time.
			 * Reset our results.
			 */
			if (!(dst_metric_locked(dst, RTAX_RTT)))
				dst->metrics[RTAX_RTT - 1] = 0;
			return;
		}

		rtt = dst_metric_rtt(dst, RTAX_RTT);
		m = rtt - tp->srtt;

		/* If newly calculated rtt larger than stored one,
		 * store new one. Otherwise, use EWMA. Remember,
		 * rtt overestimation is always better than underestimation.
		 */
		if (!(dst_metric_locked(dst, RTAX_RTT))) {
			if (m <= 0)
				set_dst_metric_rtt(dst, RTAX_RTT, tp->srtt);
			else
				set_dst_metric_rtt(dst, RTAX_RTT, rtt - (m >> 3));
		}

		if (!(dst_metric_locked(dst, RTAX_RTTVAR))) {
			unsigned long var;
			if (m < 0)
				m = -m;

			/* Scale deviation to rttvar fixed point */
			m >>= 1;
			if (m < tp->mdev)
				m = tp->mdev;

			var = dst_metric_rtt(dst, RTAX_RTTVAR);
			if (m >= var)
				var = m;
			else
				var -= (var - m) >> 2;

			set_dst_metric_rtt(dst, RTAX_RTTVAR, var);
		}

		if (tcp_in_initial_slowstart(tp)) {
			/* Slow start still did not finish. */
			if (dst_metric(dst, RTAX_SSTHRESH) &&
			    !dst_metric_locked(dst, RTAX_SSTHRESH) &&
			    (tp->snd_cwnd >> 1) > dst_metric(dst, RTAX_SSTHRESH))
				dst->metrics[RTAX_SSTHRESH-1] = tp->snd_cwnd >> 1;
			if (!dst_metric_locked(dst, RTAX_CWND) &&
			    tp->snd_cwnd > dst_metric(dst, RTAX_CWND))
				dst->metrics[RTAX_CWND - 1] = tp->snd_cwnd;
		} else if (tp->snd_cwnd > tp->snd_ssthresh &&
			   icsk->icsk_ca_state == TCP_CA_Open) {
			/* Cong. avoidance phase, cwnd is reliable. */
			if (!dst_metric_locked(dst, RTAX_SSTHRESH))
				dst->metrics[RTAX_SSTHRESH-1] =
					max(tp->snd_cwnd >> 1, tp->snd_ssthresh);
			if (!dst_metric_locked(dst, RTAX_CWND))
				dst->metrics[RTAX_CWND-1] = (dst_metric(dst, RTAX_CWND) + tp->snd_cwnd) >> 1;
		} else {
			/* Else slow start did not finish, cwnd is non-sense,
			   ssthresh may be also invalid.
			 */
			if (!dst_metric_locked(dst, RTAX_CWND))
				dst->metrics[RTAX_CWND-1] = (dst_metric(dst, RTAX_CWND) + tp->snd_ssthresh) >> 1;
			if (dst_metric(dst, RTAX_SSTHRESH) &&
			    !dst_metric_locked(dst, RTAX_SSTHRESH) &&
			    tp->snd_ssthresh > dst_metric(dst, RTAX_SSTHRESH))
				dst->metrics[RTAX_SSTHRESH-1] = tp->snd_ssthresh;
		}

		if (!dst_metric_locked(dst, RTAX_REORDERING)) {
			if (dst_metric(dst, RTAX_REORDERING) < tp->reordering &&
			    tp->reordering != sysctl_tcp_reordering)
				dst->metrics[RTAX_REORDERING-1] = tp->reordering;
		}
	}
}

/* Numbers are taken from RFC3390.
 *
 * John Heffner states:
 *
 *	The RFC specifies a window of no more than 4380 bytes
 *	unless 2*MSS > 4380.  Reading the pseudocode in the RFC
 *	is a bit misleading because they use a clamp at 4380 bytes
 *	rather than use a multiplier in the relevant range.
 */
__u32 tcp_init_cwnd(struct tcp_sock *tp, struct dst_entry *dst)
{
	__u32 cwnd = (dst ? dst_metric(dst, RTAX_INITCWND) : 0);

	if (!cwnd) {
		if (tp->mss_cache > 1460)
			cwnd = 2;
		else
			cwnd = (tp->mss_cache > 1095) ? 3 : 4;
	}
	return min_t(__u32, cwnd, tp->snd_cwnd_clamp);
}

/* Set slow start threshold and cwnd not falling to slow start */
void tcp_enter_cwr(struct sock *sk, const int set_ssthresh)
{
	struct tcp_sock *tp = tcp_sk(sk);
	const struct inet_connection_sock *icsk = inet_csk(sk);

	tp->prior_ssthresh = 0;
	tp->bytes_acked = 0;
	if (icsk->icsk_ca_state < TCP_CA_CWR) {
		tp->undo_marker = 0;
		if (set_ssthresh)
			tp->snd_ssthresh = icsk->icsk_ca_ops->ssthresh(sk);
		tp->snd_cwnd = min(tp->snd_cwnd,
				   tcp_packets_in_flight(tp) + 1U);
		tp->snd_cwnd_cnt = 0;
		tp->high_seq = tp->snd_nxt;
		tp->snd_cwnd_stamp = tcp_time_stamp;
		TCP_ECN_queue_cwr(tp);

		tcp_set_ca_state(sk, TCP_CA_CWR);
	}
}

/*
 * Packet counting of FACK is based on in-order assumptions, therefore TCP
 * disables it when reordering is detected
 */
static void tcp_disable_fack(struct tcp_sock *tp)
{
	/* RFC3517 uses different metric in lost marker => reset on change */
	if (tcp_is_fack(tp))
		tp->lost_skb_hint = NULL;
	tp->rx_opt.sack_ok &= ~2;
}

/* Take a notice that peer is sending D-SACKs */
static void tcp_dsack_seen(struct tcp_sock *tp)
{
	tp->rx_opt.sack_ok |= 4;
}

/* Initialize metrics on socket. */

static void tcp_init_metrics(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct dst_entry *dst = __sk_dst_get(sk);

	if (dst == NULL)
		goto reset;

	dst_confirm(dst);

	if (dst_metric_locked(dst, RTAX_CWND))
		tp->snd_cwnd_clamp = dst_metric(dst, RTAX_CWND);
	if (dst_metric(dst, RTAX_SSTHRESH)) {
		tp->snd_ssthresh = dst_metric(dst, RTAX_SSTHRESH);
		if (tp->snd_ssthresh > tp->snd_cwnd_clamp)
			tp->snd_ssthresh = tp->snd_cwnd_clamp;
	}
	if (dst_metric(dst, RTAX_REORDERING) &&
	    tp->reordering != dst_metric(dst, RTAX_REORDERING)) {
		tcp_disable_fack(tp);
		tp->reordering = dst_metric(dst, RTAX_REORDERING);
	}

	if (dst_metric(dst, RTAX_RTT) == 0)
		goto reset;

	if (!tp->srtt && dst_metric_rtt(dst, RTAX_RTT) < (TCP_TIMEOUT_INIT << 3))
		goto reset;

	/* Initial rtt is determined from SYN,SYN-ACK.
	 * The segment is small and rtt may appear much
	 * less than real one. Use per-dst memory
	 * to make it more realistic.
	 *
	 * A bit of theory. RTT is time passed after "normal" sized packet
	 * is sent until it is ACKed. In normal circumstances sending small
	 * packets force peer to delay ACKs and calculation is correct too.
	 * The algorithm is adaptive and, provided we follow specs, it
	 * NEVER underestimate RTT. BUT! If peer tries to make some clever
	 * tricks sort of "quick acks" for time long enough to decrease RTT
	 * to low value, and then abruptly stops to do it and starts to delay
	 * ACKs, wait for troubles.
	 */
	if (dst_metric_rtt(dst, RTAX_RTT) > tp->srtt) {
		tp->srtt = dst_metric_rtt(dst, RTAX_RTT);
		tp->rtt_seq = tp->snd_nxt;
	}
	if (dst_metric_rtt(dst, RTAX_RTTVAR) > tp->mdev) {
		tp->mdev = dst_metric_rtt(dst, RTAX_RTTVAR);
		tp->mdev_max = tp->rttvar = max(tp->mdev, tcp_rto_min(sk));
	}
	tcp_set_rto(sk);
	if (inet_csk(sk)->icsk_rto < TCP_TIMEOUT_INIT && !tp->rx_opt.saw_tstamp)
		goto reset;

cwnd:
	tp->snd_cwnd = tcp_init_cwnd(tp, dst);
	tp->snd_cwnd_stamp = tcp_time_stamp;
	return;

reset:
	/* Play conservative. If timestamps are not
	 * supported, TCP will fail to recalculate correct
	 * rtt, if initial rto is too small. FORGET ALL AND RESET!
	 */
	if (!tp->rx_opt.saw_tstamp && tp->srtt) {
		tp->srtt = 0;
		tp->mdev = tp->mdev_max = tp->rttvar = TCP_TIMEOUT_INIT;
		inet_csk(sk)->icsk_rto = TCP_TIMEOUT_INIT;
	}
	goto cwnd;
}

static void tcp_update_reordering(struct sock *sk, const int metric,
				  const int ts)
{
	struct tcp_sock *tp = tcp_sk(sk);
	if (metric > tp->reordering) {
		int mib_idx;

		tp->reordering = min(TCP_MAX_REORDERING, metric);

		/* This exciting event is worth to be remembered. 8) */
		if (ts)
			mib_idx = LINUX_MIB_TCPTSREORDER;
		else if (tcp_is_reno(tp))
			mib_idx = LINUX_MIB_TCPRENOREORDER;
		else if (tcp_is_fack(tp))
			mib_idx = LINUX_MIB_TCPFACKREORDER;
		else
			mib_idx = LINUX_MIB_TCPSACKREORDER;

		NET_INC_STATS_BH(sock_net(sk), mib_idx);
#if FASTRETRANS_DEBUG > 1
		printk(KERN_DEBUG "Disorder%d %d %u f%u s%u rr%d\n",
		       tp->rx_opt.sack_ok, inet_csk(sk)->icsk_ca_state,
		       tp->reordering,
		       tp->fackets_out,
		       tp->sacked_out,
		       tp->undo_marker ? tp->undo_retrans : 0);
#endif
		tcp_disable_fack(tp);
	}
}

/* This must be called before lost_out is incremented */
static void tcp_verify_retransmit_hint(struct tcp_sock *tp, struct sk_buff *skb)
{
	if ((tp->retransmit_skb_hint == NULL) ||
	    before(TCP_SKB_CB(skb)->seq,
		   TCP_SKB_CB(tp->retransmit_skb_hint)->seq))
		tp->retransmit_skb_hint = skb;

	if (!tp->lost_out ||
	    after(TCP_SKB_CB(skb)->end_seq, tp->retransmit_high))
		tp->retransmit_high = TCP_SKB_CB(skb)->end_seq;
}

static void tcp_skb_mark_lost(struct tcp_sock *tp, struct sk_buff *skb)
{
	if (!(TCP_SKB_CB(skb)->sacked & (TCPCB_LOST|TCPCB_SACKED_ACKED))) {
		tcp_verify_retransmit_hint(tp, skb);

		tp->lost_out += tcp_skb_pcount(skb);
		TCP_SKB_CB(skb)->sacked |= TCPCB_LOST;
	}
}

static void tcp_skb_mark_lost_uncond_verify(struct tcp_sock *tp,
					    struct sk_buff *skb)
{
	tcp_verify_retransmit_hint(tp, skb);

	if (!(TCP_SKB_CB(skb)->sacked & (TCPCB_LOST|TCPCB_SACKED_ACKED))) {
		tp->lost_out += tcp_skb_pcount(skb);
		TCP_SKB_CB(skb)->sacked |= TCPCB_LOST;
	}
}

/* This procedure tags the retransmission queue when SACKs arrive.
 *
 * We have three tag bits: SACKED(S), RETRANS(R) and LOST(L).
 * Packets in queue with these bits set are counted in variables
 * sacked_out, retrans_out and lost_out, correspondingly.
 *
 * Valid combinations are:
 * Tag  InFlight	Description
 * 0	1		- orig segment is in flight.
 * S	0		- nothing flies, orig reached receiver.
 * L	0		- nothing flies, orig lost by net.
 * R	2		- both orig and retransmit are in flight.
 * L|R	1		- orig is lost, retransmit is in flight.
 * S|R  1		- orig reached receiver, retrans is still in flight.
 * (L|S|R is logically valid, it could occur when L|R is sacked,
 *  but it is equivalent to plain S and code short-curcuits it to S.
 *  L|S is logically invalid, it would mean -1 packet in flight 8))
 *
 * These 6 states form finite state machine, controlled by the following events:
 * 1. New ACK (+SACK) arrives. (tcp_sacktag_write_queue())
 * 2. Retransmission. (tcp_retransmit_skb(), tcp_xmit_retransmit_queue())
 * 3. Loss detection event of one of three flavors:
 *	A. Scoreboard estimator decided the packet is lost.
 *	   A'. Reno "three dupacks" marks head of queue lost.
 *	   A''. Its FACK modfication, head until snd.fack is lost.
 *	B. SACK arrives sacking data transmitted after never retransmitted
 *	   hole was sent out.
 *	C. SACK arrives sacking SND.NXT at the moment, when the
 *	   segment was retransmitted.
 * 4. D-SACK added new rule: D-SACK changes any tag to S.
 *
 * It is pleasant to note, that state diagram turns out to be commutative,
 * so that we are allowed not to be bothered by order of our actions,
 * when multiple events arrive simultaneously. (see the function below).
 *
 * Reordering detection.
 * --------------------
 * Reordering metric is maximal distance, which a packet can be displaced
 * in packet stream. With SACKs we can estimate it:
 *
 * 1. SACK fills old hole and the corresponding segment was not
 *    ever retransmitted -> reordering. Alas, we cannot use it
 *    when segment was retransmitted.
 * 2. The last flaw is solved with D-SACK. D-SACK arrives
 *    for retransmitted and already SACKed segment -> reordering..
 * Both of these heuristics are not used in Loss state, when we cannot
 * account for retransmits accurately.
 *
 * SACK block validation.
 * ----------------------
 *
 * SACK block range validation checks that the received SACK block fits to
 * the expected sequence limits, i.e., it is between SND.UNA and SND.NXT.
 * Note that SND.UNA is not included to the range though being valid because
 * it means that the receiver is rather inconsistent with itself reporting
 * SACK reneging when it should advance SND.UNA. Such SACK block this is
 * perfectly valid, however, in light of RFC2018 which explicitly states
 * that "SACK block MUST reflect the newest segment.  Even if the newest
 * segment is going to be discarded ...", not that it looks very clever
 * in case of head skb. Due to potentional receiver driven attacks, we
 * choose to avoid immediate execution of a walk in write queue due to
 * reneging and defer head skb's loss recovery to standard loss recovery
 * procedure that will eventually trigger (nothing forbids us doing this).
 *
 * Implements also blockage to start_seq wrap-around. Problem lies in the
 * fact that though start_seq (s) is before end_seq (i.e., not reversed),
 * there's no guarantee that it will be before snd_nxt (n). The problem
 * happens when start_seq resides between end_seq wrap (e_w) and snd_nxt
 * wrap (s_w):
 *
 *         <- outs wnd ->                          <- wrapzone ->
 *         u     e      n                         u_w   e_w  s n_w
 *         |     |      |                          |     |   |  |
 * |<------------+------+----- TCP seqno space --------------+---------->|
 * ...-- <2^31 ->|                                           |<--------...
 * ...---- >2^31 ------>|                                    |<--------...
 *
 * Current code wouldn't be vulnerable but it's better still to discard such
 * crazy SACK blocks. Doing this check for start_seq alone closes somewhat
 * similar case (end_seq after snd_nxt wrap) as earlier reversed check in
 * snd_nxt wrap -> snd_una region will then become "well defined", i.e.,
 * equal to the ideal case (infinite seqno space without wrap caused issues).
 *
 * With D-SACK the lower bound is extended to cover sequence space below
 * SND.UNA down to undo_marker, which is the last point of interest. Yet
 * again, D-SACK block must not to go across snd_una (for the same reason as
 * for the normal SACK blocks, explained above). But there all simplicity
 * ends, TCP might receive valid D-SACKs below that. As long as they reside
 * fully below undo_marker they do not affect behavior in anyway and can
 * therefore be safely ignored. In rare cases (which are more or less
 * theoretical ones), the D-SACK will nicely cross that boundary due to skb
 * fragmentation and packet reordering past skb's retransmission. To consider
 * them correctly, the acceptable range must be extended even more though
 * the exact amount is rather hard to quantify. However, tp->max_window can
 * be used as an exaggerated estimate.
 */
static int tcp_is_sackblock_valid(struct tcp_sock *tp, int is_dsack,
				  u32 start_seq, u32 end_seq)
{
	/* Too far in future, or reversed (interpretation is ambiguous) */
	if (after(end_seq, tp->snd_nxt) || !before(start_seq, end_seq))
		return 0;

	/* Nasty start_seq wrap-around check (see comments above) */
	if (!before(start_seq, tp->snd_nxt))
		return 0;

	/* In outstanding window? ...This is valid exit for D-SACKs too.
	 * start_seq == snd_una is non-sensical (see comments above)
	 */
	if (after(start_seq, tp->snd_una))
		return 1;

	if (!is_dsack || !tp->undo_marker)
		return 0;

	/* ...Then it's D-SACK, and must reside below snd_una completely */
	if (!after(end_seq, tp->snd_una))
		return 0;

	if (!before(start_seq, tp->undo_marker))
		return 1;

	/* Too old */
	if (!after(end_seq, tp->undo_marker))
		return 0;

	/* Undo_marker boundary crossing (overestimates a lot). Known already:
	 *   start_seq < undo_marker and end_seq >= undo_marker.
	 */
	return !before(start_seq, end_seq - tp->max_window);
}

/* Check for lost retransmit. This superb idea is borrowed from "ratehalving".
 * Event "C". Later note: FACK people cheated me again 8), we have to account
 * for reordering! Ugly, but should help.
 *
 * Search retransmitted skbs from write_queue that were sent when snd_nxt was
 * less than what is now known to be received by the other end (derived from
 * highest SACK block). Also calculate the lowest snd_nxt among the remaining
 * retransmitted skbs to avoid some costly processing per ACKs.
 */
static void tcp_mark_lost_retrans(struct sock *sk)
{
	const struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	struct sk_buff *skb;
	int cnt = 0;
	u32 new_low_seq = tp->snd_nxt;
	u32 received_upto = tcp_highest_sack_seq(tp);

	if (!tcp_is_fack(tp) || !tp->retrans_out ||
	    !after(received_upto, tp->lost_retrans_low) ||
	    icsk->icsk_ca_state != TCP_CA_Recovery)
		return;

	tcp_for_write_queue(skb, sk) {
		u32 ack_seq = TCP_SKB_CB(skb)->ack_seq;

		if (skb == tcp_send_head(sk))
			break;
		if (cnt == tp->retrans_out)
			break;
		if (!after(TCP_SKB_CB(skb)->end_seq, tp->snd_una))
			continue;

		if (!(TCP_SKB_CB(skb)->sacked & TCPCB_SACKED_RETRANS))
			continue;

		/* TODO: We would like to get rid of tcp_is_fack(tp) only
		 * constraint here (see above) but figuring out that at
		 * least tp->reordering SACK blocks reside between ack_seq
		 * and received_upto is not easy task to do cheaply with
		 * the available datastructures.
		 *
		 * Whether FACK should check here for tp->reordering segs
		 * in-between one could argue for either way (it would be
		 * rather simple to implement as we could count fack_count
		 * during the walk and do tp->fackets_out - fack_count).
		 */
		if (after(received_upto, ack_seq)) {
			TCP_SKB_CB(skb)->sacked &= ~TCPCB_SACKED_RETRANS;
			tp->retrans_out -= tcp_skb_pcount(skb);

			tcp_skb_mark_lost_uncond_verify(tp, skb);
			NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPLOSTRETRANSMIT);
		} else {
			if (before(ack_seq, new_low_seq))
				new_low_seq = ack_seq;
			cnt += tcp_skb_pcount(skb);
		}
	}

	if (tp->retrans_out)
		tp->lost_retrans_low = new_low_seq;
}

static int tcp_check_dsack(struct sock *sk, struct sk_buff *ack_skb,
			   struct tcp_sack_block_wire *sp, int num_sacks,
			   u32 prior_snd_una)
{
	struct tcp_sock *tp = tcp_sk(sk);
	u32 start_seq_0 = get_unaligned_be32(&sp[0].start_seq);
	u32 end_seq_0 = get_unaligned_be32(&sp[0].end_seq);
	int dup_sack = 0;

	if (before(start_seq_0, TCP_SKB_CB(ack_skb)->ack_seq)) {
		dup_sack = 1;
		tcp_dsack_seen(tp);
		NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPDSACKRECV);
	} else if (num_sacks > 1) {
		u32 end_seq_1 = get_unaligned_be32(&sp[1].end_seq);
		u32 start_seq_1 = get_unaligned_be32(&sp[1].start_seq);

		if (!after(end_seq_0, end_seq_1) &&
		    !before(start_seq_0, start_seq_1)) {
			dup_sack = 1;
			tcp_dsack_seen(tp);
			NET_INC_STATS_BH(sock_net(sk),
					LINUX_MIB_TCPDSACKOFORECV);
		}
	}

	/* D-SACK for already forgotten data... Do dumb counting. */
	if (dup_sack &&
	    !after(end_seq_0, prior_snd_una) &&
	    after(end_seq_0, tp->undo_marker))
		tp->undo_retrans--;

	return dup_sack;
}

struct tcp_sacktag_state {
	int reord;
	int fack_count;
	int flag;
};

/* Check if skb is fully within the SACK block. In presence of GSO skbs,
 * the incoming SACK may not exactly match but we can find smaller MSS
 * aligned portion of it that matches. Therefore we might need to fragment
 * which may fail and creates some hassle (caller must handle error case
 * returns).
 *
 * FIXME: this could be merged to shift decision code
 */
static int tcp_match_skb_to_sack(struct sock *sk, struct sk_buff *skb,
				 u32 start_seq, u32 end_seq)
{
	int in_sack, err;
	unsigned int pkt_len;
	unsigned int mss;

	in_sack = !after(start_seq, TCP_SKB_CB(skb)->seq) &&
		  !before(end_seq, TCP_SKB_CB(skb)->end_seq);

	if (tcp_skb_pcount(skb) > 1 && !in_sack &&
	    after(TCP_SKB_CB(skb)->end_seq, start_seq)) {
		mss = tcp_skb_mss(skb);
		in_sack = !after(start_seq, TCP_SKB_CB(skb)->seq);

		if (!in_sack) {
			pkt_len = start_seq - TCP_SKB_CB(skb)->seq;
			if (pkt_len < mss)
				pkt_len = mss;
		} else {
			pkt_len = end_seq - TCP_SKB_CB(skb)->seq;
			if (pkt_len < mss)
				return -EINVAL;
		}

		/* Round if necessary so that SACKs cover only full MSSes
		 * and/or the remaining small portion (if present)
		 */
		if (pkt_len > mss) {
			unsigned int new_len = (pkt_len / mss) * mss;
			if (!in_sack && new_len < pkt_len) {
				new_len += mss;
				if (new_len > skb->len)
					return 0;
			}
			pkt_len = new_len;
		}
		err = tcp_fragment(sk, skb, pkt_len, mss);
		if (err < 0)
			return err;
	}

	return in_sack;
}

static u8 tcp_sacktag_one(struct sk_buff *skb, struct sock *sk,
			  struct tcp_sacktag_state *state,
			  int dup_sack, int pcount)
{
	struct tcp_sock *tp = tcp_sk(sk);
	u8 sacked = TCP_SKB_CB(skb)->sacked;
	int fack_count = state->fack_count;

	/* Account D-SACK for retransmitted packet. */
	if (dup_sack && (sacked & TCPCB_RETRANS)) {
		if (after(TCP_SKB_CB(skb)->end_seq, tp->undo_marker))
			tp->undo_retrans--;
		if (sacked & TCPCB_SACKED_ACKED)
			state->reord = min(fack_count, state->reord);
	}

	/* Nothing to do; acked frame is about to be dropped (was ACKed). */
	if (!after(TCP_SKB_CB(skb)->end_seq, tp->snd_una))
		return sacked;

	if (!(sacked & TCPCB_SACKED_ACKED)) {
		if (sacked & TCPCB_SACKED_RETRANS) {
			/* If the segment is not tagged as lost,
			 * we do not clear RETRANS, believing
			 * that retransmission is still in flight.
			 */
			if (sacked & TCPCB_LOST) {
				sacked &= ~(TCPCB_LOST|TCPCB_SACKED_RETRANS);
				tp->lost_out -= pcount;
				tp->retrans_out -= pcount;
			}
		} else {
			if (!(sacked & TCPCB_RETRANS)) {
				/* New sack for not retransmitted frame,
				 * which was in hole. It is reordering.
				 */
				if (before(TCP_SKB_CB(skb)->seq,
					   tcp_highest_sack_seq(tp)))
					state->reord = min(fack_count,
							   state->reord);

				/* SACK enhanced F-RTO (RFC4138; Appendix B) */
				if (!after(TCP_SKB_CB(skb)->end_seq, tp->frto_highmark))
					state->flag |= FLAG_ONLY_ORIG_SACKED;
			}

			if (sacked & TCPCB_LOST) {
				sacked &= ~TCPCB_LOST;
				tp->lost_out -= pcount;
			}
		}

		sacked |= TCPCB_SACKED_ACKED;
		state->flag |= FLAG_DATA_SACKED;
		tp->sacked_out += pcount;

		fack_count += pcount;

		/* Lost marker hint past SACKed? Tweak RFC3517 cnt */
		if (!tcp_is_fack(tp) && (tp->lost_skb_hint != NULL) &&
		    before(TCP_SKB_CB(skb)->seq,
			   TCP_SKB_CB(tp->lost_skb_hint)->seq))
			tp->lost_cnt_hint += pcount;

		if (fack_count > tp->fackets_out)
			tp->fackets_out = fack_count;
	}

	/* D-SACK. We can detect redundant retransmission in S|R and plain R
	 * frames and clear it. undo_retrans is decreased above, L|R frames
	 * are accounted above as well.
	 */
	if (dup_sack && (sacked & TCPCB_SACKED_RETRANS)) {
		sacked &= ~TCPCB_SACKED_RETRANS;
		tp->retrans_out -= pcount;
	}

	return sacked;
}

static int tcp_shifted_skb(struct sock *sk, struct sk_buff *skb,
			   struct tcp_sacktag_state *state,
			   unsigned int pcount, int shifted, int mss,
			   int dup_sack)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct sk_buff *prev = tcp_write_queue_prev(sk, skb);

	BUG_ON(!pcount);

	/* Tweak before seqno plays */
	if (!tcp_is_fack(tp) && tcp_is_sack(tp) && tp->lost_skb_hint &&
	    !before(TCP_SKB_CB(tp->lost_skb_hint)->seq, TCP_SKB_CB(skb)->seq))
		tp->lost_cnt_hint += pcount;

	TCP_SKB_CB(prev)->end_seq += shifted;
	TCP_SKB_CB(skb)->seq += shifted;

	skb_shinfo(prev)->gso_segs += pcount;
	BUG_ON(skb_shinfo(skb)->gso_segs < pcount);
	skb_shinfo(skb)->gso_segs -= pcount;

	/* When we're adding to gso_segs == 1, gso_size will be zero,
	 * in theory this shouldn't be necessary but as long as DSACK
	 * code can come after this skb later on it's better to keep
	 * setting gso_size to something.
	 */
	if (!skb_shinfo(prev)->gso_size) {
		skb_shinfo(prev)->gso_size = mss;
		skb_shinfo(prev)->gso_type = sk->sk_gso_type;
	}

	/* CHECKME: To clear or not to clear? Mimics normal skb currently */
	if (skb_shinfo(skb)->gso_segs <= 1) {
		skb_shinfo(skb)->gso_size = 0;
		skb_shinfo(skb)->gso_type = 0;
	}

	/* We discard results */
	tcp_sacktag_one(skb, sk, state, dup_sack, pcount);

	/* Difference in this won't matter, both ACKed by the same cumul. ACK */
	TCP_SKB_CB(prev)->sacked |= (TCP_SKB_CB(skb)->sacked & TCPCB_EVER_RETRANS);

	if (skb->len > 0) {
		BUG_ON(!tcp_skb_pcount(skb));
		NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_SACKSHIFTED);
		return 0;
	}

	/* Whole SKB was eaten :-) */

	if (skb == tp->retransmit_skb_hint)
		tp->retransmit_skb_hint = prev;
	if (skb == tp->scoreboard_skb_hint)
		tp->scoreboard_skb_hint = prev;
	if (skb == tp->lost_skb_hint) {
		tp->lost_skb_hint = prev;
		tp->lost_cnt_hint -= tcp_skb_pcount(prev);
	}

	TCP_SKB_CB(skb)->flags |= TCP_SKB_CB(prev)->flags;
	if (skb == tcp_highest_sack(sk))
		tcp_advance_highest_sack(sk, skb);

	tcp_unlink_write_queue(skb, sk);
	sk_wmem_free_skb(sk, skb);

	NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_SACKMERGED);

	return 1;
}

/* I wish gso_size would have a bit more sane initialization than
 * something-or-zero which complicates things
 */
static int tcp_skb_seglen(struct sk_buff *skb)
{
	return tcp_skb_pcount(skb) == 1 ? skb->len : tcp_skb_mss(skb);
}

/* Shifting pages past head area doesn't work */
static int skb_can_shift(struct sk_buff *skb)
{
	return !skb_headlen(skb) && skb_is_nonlinear(skb);
}

/* Try collapsing SACK blocks spanning across multiple skbs to a single
 * skb.
 */
static struct sk_buff *tcp_shift_skb_data(struct sock *sk, struct sk_buff *skb,
					  struct tcp_sacktag_state *state,
					  u32 start_seq, u32 end_seq,
					  int dup_sack)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct sk_buff *prev;
	int mss;
	int pcount = 0;
	int len;
	int in_sack;

	if (!sk_can_gso(sk))
		goto fallback;

	/* Normally R but no L won't result in plain S */
	if (!dup_sack &&
	    (TCP_SKB_CB(skb)->sacked & (TCPCB_LOST|TCPCB_SACKED_RETRANS)) == TCPCB_SACKED_RETRANS)
		goto fallback;
	if (!skb_can_shift(skb))
		goto fallback;
	/* This frame is about to be dropped (was ACKed). */
	if (!after(TCP_SKB_CB(skb)->end_seq, tp->snd_una))
		goto fallback;

	/* Can only happen with delayed DSACK + discard craziness */
	if (unlikely(skb == tcp_write_queue_head(sk)))
		goto fallback;
	prev = tcp_write_queue_prev(sk, skb);

	if ((TCP_SKB_CB(prev)->sacked & TCPCB_TAGBITS) != TCPCB_SACKED_ACKED)
		goto fallback;

	in_sack = !after(start_seq, TCP_SKB_CB(skb)->seq) &&
		  !before(end_seq, TCP_SKB_CB(skb)->end_seq);

	if (in_sack) {
		len = skb->len;
		pcount = tcp_skb_pcount(skb);
		mss = tcp_skb_seglen(skb);

		/* TODO: Fix DSACKs to not fragment already SACKed and we can
		 * drop this restriction as unnecessary
		 */
		if (mss != tcp_skb_seglen(prev))
			goto fallback;
	} else {
		if (!after(TCP_SKB_CB(skb)->end_seq, start_seq))
			goto noop;
		/* CHECKME: This is non-MSS split case only?, this will
		 * cause skipped skbs due to advancing loop btw, original
		 * has that feature too
		 */
		if (tcp_skb_pcount(skb) <= 1)
			goto noop;

		in_sack = !after(start_seq, TCP_SKB_CB(skb)->seq);
		if (!in_sack) {
			/* TODO: head merge to next could be attempted here
			 * if (!after(TCP_SKB_CB(skb)->end_seq, end_seq)),
			 * though it might not be worth of the additional hassle
			 *
			 * ...we can probably just fallback to what was done
			 * previously. We could try merging non-SACKed ones
			 * as well but it probably isn't going to buy off
			 * because later SACKs might again split them, and
			 * it would make skb timestamp tracking considerably
			 * harder problem.
			 */
			goto fallback;
		}

		len = end_seq - TCP_SKB_CB(skb)->seq;
		BUG_ON(len < 0);
		BUG_ON(len > skb->len);

		/* MSS boundaries should be honoured or else pcount will
		 * severely break even though it makes things bit trickier.
		 * Optimize common case to avoid most of the divides
		 */
		mss = tcp_skb_mss(skb);

		/* TODO: Fix DSACKs to not fragment already SACKed and we can
		 * drop this restriction as unnecessary
		 */
		if (mss != tcp_skb_seglen(prev))
			goto fallback;

		if (len == mss) {
			pcount = 1;
		} else if (len < mss) {
			goto noop;
		} else {
			pcount = len / mss;
			len = pcount * mss;
		}
	}

	if (!skb_shift(prev, skb, len))
		goto fallback;
	if (!tcp_shifted_skb(sk, skb, state, pcount, len, mss, dup_sack))
		goto out;

	/* Hole filled allows collapsing with the next as well, this is very
	 * useful when hole on every nth skb pattern happens
	 */
	if (prev == tcp_write_queue_tail(sk))
		goto out;
	skb = tcp_write_queue_next(sk, prev);

	if (!skb_can_shift(skb) ||
	    (skb == tcp_send_head(sk)) ||
	    ((TCP_SKB_CB(skb)->sacked & TCPCB_TAGBITS) != TCPCB_SACKED_ACKED) ||
	    (mss != tcp_skb_seglen(skb)))
		goto out;

	len = skb->len;
	if (skb_shift(prev, skb, len)) {
		pcount += tcp_skb_pcount(skb);
		tcp_shifted_skb(sk, skb, state, tcp_skb_pcount(skb), len, mss, 0);
	}

out:
	state->fack_count += pcount;
	return prev;

noop:
	return skb;

fallback:
	NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_SACKSHIFTFALLBACK);
	return NULL;
}

static struct sk_buff *tcp_sacktag_walk(struct sk_buff *skb, struct sock *sk,
					struct tcp_sack_block *next_dup,
					struct tcp_sacktag_state *state,
					u32 start_seq, u32 end_seq,
					int dup_sack_in)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct sk_buff *tmp;

	tcp_for_write_queue_from(skb, sk) {
		int in_sack = 0;
		int dup_sack = dup_sack_in;

		if (skb == tcp_send_head(sk))
			break;

		/* queue is in-order => we can short-circuit the walk early */
		if (!before(TCP_SKB_CB(skb)->seq, end_seq))
			break;

		if ((next_dup != NULL) &&
		    before(TCP_SKB_CB(skb)->seq, next_dup->end_seq)) {
			in_sack = tcp_match_skb_to_sack(sk, skb,
							next_dup->start_seq,
							next_dup->end_seq);
			if (in_sack > 0)
				dup_sack = 1;
		}

		/* skb reference here is a bit tricky to get right, since
		 * shifting can eat and free both this skb and the next,
		 * so not even _safe variant of the loop is enough.
		 */
		if (in_sack <= 0) {
			tmp = tcp_shift_skb_data(sk, skb, state,
						 start_seq, end_seq, dup_sack);
			if (tmp != NULL) {
				if (tmp != skb) {
					skb = tmp;
					continue;
				}

				in_sack = 0;
			} else {
				in_sack = tcp_match_skb_to_sack(sk, skb,
								start_seq,
								end_seq);
			}
		}

		if (unlikely(in_sack < 0))
			break;

		if (in_sack) {
			TCP_SKB_CB(skb)->sacked = tcp_sacktag_one(skb, sk,
								  state,
								  dup_sack,
								  tcp_skb_pcount(skb));

			if (!before(TCP_SKB_CB(skb)->seq,
				    tcp_highest_sack_seq(tp)))
				tcp_advance_highest_sack(sk, skb);
		}

		state->fack_count += tcp_skb_pcount(skb);
	}
	return skb;
}

/* Avoid all extra work that is being done by sacktag while walking in
 * a normal way
 */
static struct sk_buff *tcp_sacktag_skip(struct sk_buff *skb, struct sock *sk,
					struct tcp_sacktag_state *state,
					u32 skip_to_seq)
{
	tcp_for_write_queue_from(skb, sk) {
		if (skb == tcp_send_head(sk))
			break;

		if (after(TCP_SKB_CB(skb)->end_seq, skip_to_seq))
			break;

		state->fack_count += tcp_skb_pcount(skb);
	}
	return skb;
}

static struct sk_buff *tcp_maybe_skipping_dsack(struct sk_buff *skb,
						struct sock *sk,
						struct tcp_sack_block *next_dup,
						struct tcp_sacktag_state *state,
						u32 skip_to_seq)
{
	if (next_dup == NULL)
		return skb;

	if (before(next_dup->start_seq, skip_to_seq)) {
		skb = tcp_sacktag_skip(skb, sk, state, next_dup->start_seq);
		skb = tcp_sacktag_walk(skb, sk, NULL, state,
				       next_dup->start_seq, next_dup->end_seq,
				       1);
	}

	return skb;
}

static int tcp_sack_cache_ok(struct tcp_sock *tp, struct tcp_sack_block *cache)
{
	return cache < tp->recv_sack_cache + ARRAY_SIZE(tp->recv_sack_cache);
}

static int
tcp_sacktag_write_queue(struct sock *sk, struct sk_buff *ack_skb,
			u32 prior_snd_una)
{
	const struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	unsigned char *ptr = (skb_transport_header(ack_skb) +
			      TCP_SKB_CB(ack_skb)->sacked);
	struct tcp_sack_block_wire *sp_wire = (struct tcp_sack_block_wire *)(ptr+2);
	struct tcp_sack_block sp[TCP_NUM_SACKS];
	struct tcp_sack_block *cache;
	struct tcp_sacktag_state state;
	struct sk_buff *skb;
	int num_sacks = min(TCP_NUM_SACKS, (ptr[1] - TCPOLEN_SACK_BASE) >> 3);
	int used_sacks;
	int found_dup_sack = 0;
	int i, j;
	int first_sack_index;

	state.flag = 0;
	state.reord = tp->packets_out;

	if (!tp->sacked_out) {
		if (WARN_ON(tp->fackets_out))
			tp->fackets_out = 0;
		tcp_highest_sack_reset(sk);
	}

	found_dup_sack = tcp_check_dsack(sk, ack_skb, sp_wire,
					 num_sacks, prior_snd_una);
	if (found_dup_sack)
		state.flag |= FLAG_DSACKING_ACK;

	/* Eliminate too old ACKs, but take into
	 * account more or less fresh ones, they can
	 * contain valid SACK info.
	 */
	if (before(TCP_SKB_CB(ack_skb)->ack_seq, prior_snd_una - tp->max_window))
		return 0;

	if (!tp->packets_out)
		goto out;

	used_sacks = 0;
	first_sack_index = 0;
	for (i = 0; i < num_sacks; i++) {
		int dup_sack = !i && found_dup_sack;

		sp[used_sacks].start_seq = get_unaligned_be32(&sp_wire[i].start_seq);
		sp[used_sacks].end_seq = get_unaligned_be32(&sp_wire[i].end_seq);

		if (!tcp_is_sackblock_valid(tp, dup_sack,
					    sp[used_sacks].start_seq,
					    sp[used_sacks].end_seq)) {
			int mib_idx;

			if (dup_sack) {
				if (!tp->undo_marker)
					mib_idx = LINUX_MIB_TCPDSACKIGNOREDNOUNDO;
				else
					mib_idx = LINUX_MIB_TCPDSACKIGNOREDOLD;
			} else {
				/* Don't count olds caused by ACK reordering */
				if ((TCP_SKB_CB(ack_skb)->ack_seq != tp->snd_una) &&
				    !after(sp[used_sacks].end_seq, tp->snd_una))
					continue;
				mib_idx = LINUX_MIB_TCPSACKDISCARD;
			}

			NET_INC_STATS_BH(sock_net(sk), mib_idx);
			if (i == 0)
				first_sack_index = -1;
			continue;
		}

		/* Ignore very old stuff early */
		if (!after(sp[used_sacks].end_seq, prior_snd_una))
			continue;

		used_sacks++;
	}

	/* order SACK blocks to allow in order walk of the retrans queue */
	for (i = used_sacks - 1; i > 0; i--) {
		for (j = 0; j < i; j++) {
			if (after(sp[j].start_seq, sp[j + 1].start_seq)) {
				swap(sp[j], sp[j + 1]);

				/* Track where the first SACK block goes to */
				if (j == first_sack_index)
					first_sack_index = j + 1;
			}
		}
	}

	skb = tcp_write_queue_head(sk);
	state.fack_count = 0;
	i = 0;

	if (!tp->sacked_out) {
		/* It's already past, so skip checking against it */
		cache = tp->recv_sack_cache + ARRAY_SIZE(tp->recv_sack_cache);
	} else {
		cache = tp->recv_sack_cache;
		/* Skip empty blocks in at head of the cache */
		while (tcp_sack_cache_ok(tp, cache) && !cache->start_seq &&
		       !cache->end_seq)
			cache++;
	}

	while (i < used_sacks) {
		u32 start_seq = sp[i].start_seq;
		u32 end_seq = sp[i].end_seq;
		int dup_sack = (found_dup_sack && (i == first_sack_index));
		struct tcp_sack_block *next_dup = NULL;

		if (found_dup_sack && ((i + 1) == first_sack_index))
			next_dup = &sp[i + 1];

		/* Event "B" in the comment above. */
		if (after(end_seq, tp->high_seq))
			state.flag |= FLAG_DATA_LOST;

		/* Skip too early cached blocks */
		while (tcp_sack_cache_ok(tp, cache) &&
		       !before(start_seq, cache->end_seq))
			cache++;

		/* Can skip some work by looking recv_sack_cache? */
		if (tcp_sack_cache_ok(tp, cache) && !dup_sack &&
		    after(end_seq, cache->start_seq)) {

			/* Head todo? */
			if (before(start_seq, cache->start_seq)) {
				skb = tcp_sacktag_skip(skb, sk, &state,
						       start_seq);
				skb = tcp_sacktag_walk(skb, sk, next_dup,
						       &state,
						       start_seq,
						       cache->start_seq,
						       dup_sack);
			}

			/* Rest of the block already fully processed? */
			if (!after(end_seq, cache->end_seq))
				goto advance_sp;

			skb = tcp_maybe_skipping_dsack(skb, sk, next_dup,
						       &state,
						       cache->end_seq);

			/* ...tail remains todo... */
			if (tcp_highest_sack_seq(tp) == cache->end_seq) {
				/* ...but better entrypoint exists! */
				skb = tcp_highest_sack(sk);
				if (skb == NULL)
					break;
				state.fack_count = tp->fackets_out;
				cache++;
				goto walk;
			}

			skb = tcp_sacktag_skip(skb, sk, &state, cache->end_seq);
			/* Check overlap against next cached too (past this one already) */
			cache++;
			continue;
		}

		if (!before(start_seq, tcp_highest_sack_seq(tp))) {
			skb = tcp_highest_sack(sk);
			if (skb == NULL)
				break;
			state.fack_count = tp->fackets_out;
		}
		skb = tcp_sacktag_skip(skb, sk, &state, start_seq);

walk:
		skb = tcp_sacktag_walk(skb, sk, next_dup, &state,
				       start_seq, end_seq, dup_sack);

advance_sp:
		/* SACK enhanced FRTO (RFC4138, Appendix B): Clearing correct
		 * due to in-order walk
		 */
		if (after(end_seq, tp->frto_highmark))
			state.flag &= ~FLAG_ONLY_ORIG_SACKED;

		i++;
	}

	/* Clear the head of the cache sack blocks so we can skip it next time */
	for (i = 0; i < ARRAY_SIZE(tp->recv_sack_cache) - used_sacks; i++) {
		tp->recv_sack_cache[i].start_seq = 0;
		tp->recv_sack_cache[i].end_seq = 0;
	}
	for (j = 0; j < used_sacks; j++)
		tp->recv_sack_cache[i++] = sp[j];

	tcp_mark_lost_retrans(sk);

	tcp_verify_left_out(tp);

	if ((state.reord < tp->fackets_out) &&
	    ((icsk->icsk_ca_state != TCP_CA_Loss) || tp->undo_marker) &&
	    (!tp->frto_highmark || after(tp->snd_una, tp->frto_highmark)))
		tcp_update_reordering(sk, tp->fackets_out - state.reord, 0);

out:

#if FASTRETRANS_DEBUG > 0
	WARN_ON((int)tp->sacked_out < 0);
	WARN_ON((int)tp->lost_out < 0);
	WARN_ON((int)tp->retrans_out < 0);
	WARN_ON((int)tcp_packets_in_flight(tp) < 0);
#endif
	return state.flag;
}

/* Limits sacked_out so that sum with lost_out isn't ever larger than
 * packets_out. Returns zero if sacked_out adjustement wasn't necessary.
 */
static int tcp_limit_reno_sacked(struct tcp_sock *tp)
{
	u32 holes;

	holes = max(tp->lost_out, 1U);
	holes = min(holes, tp->packets_out);

	if ((tp->sacked_out + holes) > tp->packets_out) {
		tp->sacked_out = tp->packets_out - holes;
		return 1;
	}
	return 0;
}

/* If we receive more dupacks than we expected counting segments
 * in assumption of absent reordering, interpret this as reordering.
 * The only another reason could be bug in receiver TCP.
 */
static void tcp_check_reno_reordering(struct sock *sk, const int addend)
{
	struct tcp_sock *tp = tcp_sk(sk);
	if (tcp_limit_reno_sacked(tp))
		tcp_update_reordering(sk, tp->packets_out + addend, 0);
}

/* Emulate SACKs for SACKless connection: account for a new dupack. */

static void tcp_add_reno_sack(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	tp->sacked_out++;
	tcp_check_reno_reordering(sk, 0);
	tcp_verify_left_out(tp);
}

/* Account for ACK, ACKing some data in Reno Recovery phase. */

static void tcp_remove_reno_sacks(struct sock *sk, int acked)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (acked > 0) {
		/* One ACK acked hole. The rest eat duplicate ACKs. */
		if (acked - 1 >= tp->sacked_out)
			tp->sacked_out = 0;
		else
			tp->sacked_out -= acked - 1;
	}
	tcp_check_reno_reordering(sk, acked);
	tcp_verify_left_out(tp);
}

static inline void tcp_reset_reno_sack(struct tcp_sock *tp)
{
	tp->sacked_out = 0;
}

static int tcp_is_sackfrto(const struct tcp_sock *tp)
{
	return (sysctl_tcp_frto == 0x2) && !tcp_is_reno(tp);
}

/* F-RTO can only be used if TCP has never retransmitted anything other than
 * head (SACK enhanced variant from Appendix B of RFC4138 is more robust here)
 */
int tcp_use_frto(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	const struct inet_connection_sock *icsk = inet_csk(sk);
	struct sk_buff *skb;

	if (!sysctl_tcp_frto)
		return 0;

	/* MTU probe and F-RTO won't really play nicely along currently */
	if (icsk->icsk_mtup.probe_size)
		return 0;

	if (tcp_is_sackfrto(tp))
		return 1;

	/* Avoid expensive walking of rexmit queue if possible */
	if (tp->retrans_out > 1)
		return 0;

	skb = tcp_write_queue_head(sk);
	if (tcp_skb_is_last(sk, skb))
		return 1;
	skb = tcp_write_queue_next(sk, skb);	/* Skips head */
	tcp_for_write_queue_from(skb, sk) {
		if (skb == tcp_send_head(sk))
			break;
		if (TCP_SKB_CB(skb)->sacked & TCPCB_RETRANS)
			return 0;
		/* Short-circuit when first non-SACKed skb has been checked */
		if (!(TCP_SKB_CB(skb)->sacked & TCPCB_SACKED_ACKED))
			break;
	}
	return 1;
}

/* RTO occurred, but do not yet enter Loss state. Instead, defer RTO
 * recovery a bit and use heuristics in tcp_process_frto() to detect if
 * the RTO was spurious. Only clear SACKED_RETRANS of the head here to
 * keep retrans_out counting accurate (with SACK F-RTO, other than head
 * may still have that bit set); TCPCB_LOST and remaining SACKED_RETRANS
 * bits are handled if the Loss state is really to be entered (in
 * tcp_enter_frto_loss).
 *
 * Do like tcp_enter_loss() would; when RTO expires the second time it
 * does:
 *  "Reduce ssthresh if it has not yet been made inside this window."
 */
void tcp_enter_frto(struct sock *sk)
{
	const struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	struct sk_buff *skb;

	if ((!tp->frto_counter && icsk->icsk_ca_state <= TCP_CA_Disorder) ||
	    tp->snd_una == tp->high_seq ||
	    ((icsk->icsk_ca_state == TCP_CA_Loss || tp->frto_counter) &&
	     !icsk->icsk_retransmits)) {
		tp->prior_ssthresh = tcp_current_ssthresh(sk);
		/* Our state is too optimistic in ssthresh() call because cwnd
		 * is not reduced until tcp_enter_frto_loss() when previous F-RTO
		 * recovery has not yet completed. Pattern would be this: RTO,
		 * Cumulative ACK, RTO (2xRTO for the same segment does not end
		 * up here twice).
		 * RFC4138 should be more specific on what to do, even though
		 * RTO is quite unlikely to occur after the first Cumulative ACK
		 * due to back-off and complexity of triggering events ...
		 */
		if (tp->frto_counter) {
			u32 stored_cwnd;
			stored_cwnd = tp->snd_cwnd;
			tp->snd_cwnd = 2;
			tp->snd_ssthresh = icsk->icsk_ca_ops->ssthresh(sk);
			tp->snd_cwnd = stored_cwnd;
		} else {
			tp->snd_ssthresh = icsk->icsk_ca_ops->ssthresh(sk);
		}
		/* ... in theory, cong.control module could do "any tricks" in
		 * ssthresh(), which means that ca_state, lost bits and lost_out
		 * counter would have to be faked before the call occurs. We
		 * consider that too expensive, unlikely and hacky, so modules
		 * using these in ssthresh() must deal these incompatibility
		 * issues if they receives CA_EVENT_FRTO and frto_counter != 0
		 */
		tcp_ca_event(sk, CA_EVENT_FRTO);
	}

	tp->undo_marker = tp->snd_una;
	tp->undo_retrans = 0;

	skb = tcp_write_queue_head(sk);
	if (TCP_SKB_CB(skb)->sacked & TCPCB_RETRANS)
		tp->undo_marker = 0;
	if (TCP_SKB_CB(skb)->sacked & TCPCB_SACKED_RETRANS) {
		TCP_SKB_CB(skb)->sacked &= ~TCPCB_SACKED_RETRANS;
		tp->retrans_out -= tcp_skb_pcount(skb);
	}
	tcp_verify_left_out(tp);

	/* Too bad if TCP was application limited */
	tp->snd_cwnd = min(tp->snd_cwnd, tcp_packets_in_flight(tp) + 1);

	/* Earlier loss recovery underway (see RFC4138; Appendix B).
	 * The last condition is necessary at least in tp->frto_counter case.
	 */
	if (tcp_is_sackfrto(tp) && (tp->frto_counter ||
	    ((1 << icsk->icsk_ca_state) & (TCPF_CA_Recovery|TCPF_CA_Loss))) &&
	    after(tp->high_seq, tp->snd_una)) {
		tp->frto_highmark = tp->high_seq;
	} else {
		tp->frto_highmark = tp->snd_nxt;
	}
	tcp_set_ca_state(sk, TCP_CA_Disorder);
	tp->high_seq = tp->snd_nxt;
	tp->frto_counter = 1;
}

/* Enter Loss state after F-RTO was applied. Dupack arrived after RTO,
 * which indicates that we should follow the traditional RTO recovery,
 * i.e. mark everything lost and do go-back-N retransmission.
 */
static void tcp_enter_frto_loss(struct sock *sk, int allowed_segments, int flag)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct sk_buff *skb;

	tp->lost_out = 0;
	tp->retrans_out = 0;
	if (tcp_is_reno(tp))
		tcp_reset_reno_sack(tp);

	tcp_for_write_queue(skb, sk) {
		if (skb == tcp_send_head(sk))
			break;

		TCP_SKB_CB(skb)->sacked &= ~TCPCB_LOST;
		/*
		 * Count the retransmission made on RTO correctly (only when
		 * waiting for the first ACK and did not get it)...
		 */
		if ((tp->frto_counter == 1) && !(flag & FLAG_DATA_ACKED)) {
			/* For some reason this R-bit might get cleared? */
			if (TCP_SKB_CB(skb)->sacked & TCPCB_SACKED_RETRANS)
				tp->retrans_out += tcp_skb_pcount(skb);
			/* ...enter this if branch just for the first segment */
			flag |= FLAG_DATA_ACKED;
		} else {
			if (TCP_SKB_CB(skb)->sacked & TCPCB_RETRANS)
				tp->undo_marker = 0;
			TCP_SKB_CB(skb)->sacked &= ~TCPCB_SACKED_RETRANS;
		}

		/* Marking forward transmissions that were made after RTO lost
		 * can cause unnecessary retransmissions in some scenarios,
		 * SACK blocks will mitigate that in some but not in all cases.
		 * We used to not mark them but it was causing break-ups with
		 * receivers that do only in-order receival.
		 *
		 * TODO: we could detect presence of such receiver and select
		 * different behavior per flow.
		 */
		if (!(TCP_SKB_CB(skb)->sacked & TCPCB_SACKED_ACKED)) {
			TCP_SKB_CB(skb)->sacked |= TCPCB_LOST;
			tp->lost_out += tcp_skb_pcount(skb);
			tp->retransmit_high = TCP_SKB_CB(skb)->end_seq;
		}
	}
	tcp_verify_left_out(tp);

	tp->snd_cwnd = tcp_packets_in_flight(tp) + allowed_segments;
	tp->snd_cwnd_cnt = 0;
	tp->snd_cwnd_stamp = tcp_time_stamp;
	tp->frto_counter = 0;
	tp->bytes_acked = 0;

	tp->reordering = min_t(unsigned int, tp->reordering,
			       sysctl_tcp_reordering);
	tcp_set_ca_state(sk, TCP_CA_Loss);
	tp->high_seq = tp->snd_nxt;
	TCP_ECN_queue_cwr(tp);

	tcp_clear_all_retrans_hints(tp);
}

static void tcp_clear_retrans_partial(struct tcp_sock *tp)
{
	tp->retrans_out = 0;
	tp->lost_out = 0;

	tp->undo_marker = 0;
	tp->undo_retrans = 0;
}

void tcp_clear_retrans(struct tcp_sock *tp)
{
	tcp_clear_retrans_partial(tp);

	tp->fackets_out = 0;
	tp->sacked_out = 0;
}

/* Enter Loss state. If "how" is not zero, forget all SACK information
 * and reset tags completely, otherwise preserve SACKs. If receiver
 * dropped its ofo queue, we will know this due to reneging detection.
 */
void tcp_enter_loss(struct sock *sk, int how)
{
	const struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	struct sk_buff *skb;

	/* Reduce ssthresh if it has not yet been made inside this window. */
	if (icsk->icsk_ca_state <= TCP_CA_Disorder || tp->snd_una == tp->high_seq ||
	    (icsk->icsk_ca_state == TCP_CA_Loss && !icsk->icsk_retransmits)) {
		tp->prior_ssthresh = tcp_current_ssthresh(sk);
		tp->snd_ssthresh = icsk->icsk_ca_ops->ssthresh(sk);
		tcp_ca_event(sk, CA_EVENT_LOSS);
	}
	tp->snd_cwnd	   = 1;
	tp->snd_cwnd_cnt   = 0;
	tp->snd_cwnd_stamp = tcp_time_stamp;

	tp->bytes_acked = 0;
	tcp_clear_retrans_partial(tp);

	if (tcp_is_reno(tp))
		tcp_reset_reno_sack(tp);

	if (!how) {
		/* Push undo marker, if it was plain RTO and nothing
		 * was retransmitted. */
		tp->undo_marker = tp->snd_una;
	} else {
		tp->sacked_out = 0;
		tp->fackets_out = 0;
	}
	tcp_clear_all_retrans_hints(tp);

	tcp_for_write_queue(skb, sk) {
		if (skb == tcp_send_head(sk))
			break;

		if (TCP_SKB_CB(skb)->sacked & TCPCB_RETRANS)
			tp->undo_marker = 0;
		TCP_SKB_CB(skb)->sacked &= (~TCPCB_TAGBITS)|TCPCB_SACKED_ACKED;
		if (!(TCP_SKB_CB(skb)->sacked&TCPCB_SACKED_ACKED) || how) {
			TCP_SKB_CB(skb)->sacked &= ~TCPCB_SACKED_ACKED;
			TCP_SKB_CB(skb)->sacked |= TCPCB_LOST;
			tp->lost_out += tcp_skb_pcount(skb);
			tp->retransmit_high = TCP_SKB_CB(skb)->end_seq;
		}
	}
	tcp_verify_left_out(tp);

	tp->reordering = min_t(unsigned int, tp->reordering,
			       sysctl_tcp_reordering);
	tcp_set_ca_state(sk, TCP_CA_Loss);
	tp->high_seq = tp->snd_nxt;
	TCP_ECN_queue_cwr(tp);
	/* Abort F-RTO algorithm if one is in progress */
	tp->frto_counter = 0;
}

/* If ACK arrived pointing to a remembered SACK, it means that our
 * remembered SACKs do not reflect real state of receiver i.e.
 * receiver _host_ is heavily congested (or buggy).
 *
 * Do processing similar to RTO timeout.
 */
static int tcp_check_sack_reneging(struct sock *sk, int flag)
{
	if (flag & FLAG_SACK_RENEGING) {
		struct inet_connection_sock *icsk = inet_csk(sk);
		NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPSACKRENEGING);

		tcp_enter_loss(sk, 1);
		icsk->icsk_retransmits++;
		tcp_retransmit_skb(sk, tcp_write_queue_head(sk));
		inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
					  icsk->icsk_rto, TCP_RTO_MAX);
		return 1;
	}
	return 0;
}

static inline int tcp_fackets_out(struct tcp_sock *tp)
{
	return tcp_is_reno(tp) ? tp->sacked_out + 1 : tp->fackets_out;
}

/* Heurestics to calculate number of duplicate ACKs. There's no dupACKs
 * counter when SACK is enabled (without SACK, sacked_out is used for
 * that purpose).
 *
 * Instead, with FACK TCP uses fackets_out that includes both SACKed
 * segments up to the highest received SACK block so far and holes in
 * between them.
 *
 * With reordering, holes may still be in flight, so RFC3517 recovery
 * uses pure sacked_out (total number of SACKed segments) even though
 * it violates the RFC that uses duplicate ACKs, often these are equal
 * but when e.g. out-of-window ACKs or packet duplication occurs,
 * they differ. Since neither occurs due to loss, TCP should really
 * ignore them.
 */
static inline int tcp_dupack_heurestics(struct tcp_sock *tp)
{
	return tcp_is_fack(tp) ? tp->fackets_out : tp->sacked_out + 1;
}

static inline int tcp_skb_timedout(struct sock *sk, struct sk_buff *skb)
{
	return (tcp_time_stamp - TCP_SKB_CB(skb)->when > inet_csk(sk)->icsk_rto);
}

static inline int tcp_head_timedout(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);

	return tp->packets_out &&
	       tcp_skb_timedout(sk, tcp_write_queue_head(sk));
}

/* Linux NewReno/SACK/FACK/ECN state machine.
 * --------------------------------------
 *
 * "Open"	Normal state, no dubious events, fast path.
 * "Disorder"   In all the respects it is "Open",
 *		but requires a bit more attention. It is entered when
 *		we see some SACKs or dupacks. It is split of "Open"
 *		mainly to move some processing from fast path to slow one.
 * "CWR"	CWND was reduced due to some Congestion Notification event.
 *		It can be ECN, ICMP source quench, local device congestion.
 * "Recovery"	CWND was reduced, we are fast-retransmitting.
 * "Loss"	CWND was reduced due to RTO timeout or SACK reneging.
 *
 * tcp_fastretrans_alert() is entered:
 * - each incoming ACK, if state is not "Open"
 * - when arrived ACK is unusual, namely:
 *	* SACK
 *	* Duplicate ACK.
 *	* ECN ECE.
 *
 * Counting packets in flight is pretty simple.
 *
 *	in_flight = packets_out - left_out + retrans_out
 *
 *	packets_out is SND.NXT-SND.UNA counted in packets.
 *
 *	retrans_out is number of retransmitted segments.
 *
 *	left_out is number of segments left network, but not ACKed yet.
 *
 *		left_out = sacked_out + lost_out
 *
 *     sacked_out: Packets, which arrived to receiver out of order
 *		   and hence not ACKed. With SACKs this number is simply
 *		   amount of SACKed data. Even without SACKs
 *		   it is easy to give pretty reliable estimate of this number,
 *		   counting duplicate ACKs.
 *
 *       lost_out: Packets lost by network. TCP has no explicit
 *		   "loss notification" feedback from network (for now).
 *		   It means that this number can be only _guessed_.
 *		   Actually, it is the heuristics to predict lossage that
 *		   distinguishes different algorithms.
 *
 *	F.e. after RTO, when all the queue is considered as lost,
 *	lost_out = packets_out and in_flight = retrans_out.
 *
 *		Essentially, we have now two algorithms counting
 *		lost packets.
 *
 *		FACK: It is the simplest heuristics. As soon as we decided
 *		that something is lost, we decide that _all_ not SACKed
 *		packets until the most forward SACK are lost. I.e.
 *		lost_out = fackets_out - sacked_out and left_out = fackets_out.
 *		It is absolutely correct estimate, if network does not reorder
 *		packets. And it loses any connection to reality when reordering
 *		takes place. We use FACK by default until reordering
 *		is suspected on the path to this destination.
 *
 *		NewReno: when Recovery is entered, we assume that one segment
 *		is lost (classic Reno). While we are in Recovery and
 *		a partial ACK arrives, we assume that one more packet
 *		is lost (NewReno). This heuristics are the same in NewReno
 *		and SACK.
 *
 *  Imagine, that's all! Forget about all this shamanism about CWND inflation
 *  deflation etc. CWND is real congestion window, never inflated, changes
 *  only according to classic VJ rules.
 *
 * Really tricky (and requiring careful tuning) part of algorithm
 * is hidden in functions tcp_time_to_recover() and tcp_xmit_retransmit_queue().
 * The first determines the moment _when_ we should reduce CWND and,
 * hence, slow down forward transmission. In fact, it determines the moment
 * when we decide that hole is caused by loss, rather than by a reorder.
 *
 * tcp_xmit_retransmit_queue() decides, _what_ we should retransmit to fill
 * holes, caused by lost packets.
 *
 * And the most logically complicated part of algorithm is undo
 * heuristics. We detect false retransmits due to both too early
 * fast retransmit (reordering) and underestimated RTO, analyzing
 * timestamps and D-SACKs. When we detect that some segments were
 * retransmitted by mistake and CWND reduction was wrong, we undo
 * window reduction and abort recovery phase. This logic is hidden
 * inside several functions named tcp_try_undo_<something>.
 */

/* This function decides, when we should leave Disordered state
 * and enter Recovery phase, reducing congestion window.
 *
 * Main question: may we further continue forward transmission
 * with the same cwnd?
 */
static int tcp_time_to_recover(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	__u32 packets_out;

	/* Do not perform any recovery during F-RTO algorithm */
	if (tp->frto_counter)
		return 0;

	/* Trick#1: The loss is proven. */
	if (tp->lost_out)
		return 1;

	/* Not-A-Trick#2 : Classic rule... */
	if (tcp_dupack_heurestics(tp) > tp->reordering)
		return 1;

	/* Trick#3 : when we use RFC2988 timer restart, fast
	 * retransmit can be triggered by timeout of queue head.
	 */
	if (tcp_is_fack(tp) && tcp_head_timedout(sk))
		return 1;

	/* Trick#4: It is still not OK... But will it be useful to delay
	 * recovery more?
	 */
	packets_out = tp->packets_out;
	if (packets_out <= tp->reordering &&
	    tp->sacked_out >= max_t(__u32, packets_out/2, sysctl_tcp_reordering) &&
	    !tcp_may_send_now(sk)) {
		/* We have nothing to send. This connection is limited
		 * either by receiver window or by application.
		 */
		return 1;
	}

	return 0;
}

/* New heuristics: it is possible only after we switched to restart timer
 * each time when something is ACKed. Hence, we can detect timed out packets
 * during fast retransmit without falling to slow start.
 *
 * Usefulness of this as is very questionable, since we should know which of
 * the segments is the next to timeout which is relatively expensive to find
 * in general case unless we add some data structure just for that. The
 * current approach certainly won't find the right one too often and when it
 * finally does find _something_ it usually marks large part of the window
 * right away (because a retransmission with a larger timestamp blocks the
 * loop from advancing). -ij
 */
static void tcp_timeout_skbs(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct sk_buff *skb;

	if (!tcp_is_fack(tp) || !tcp_head_timedout(sk))
		return;

	skb = tp->scoreboard_skb_hint;
	if (tp->scoreboard_skb_hint == NULL)
		skb = tcp_write_queue_head(sk);

	tcp_for_write_queue_from(skb, sk) {
		if (skb == tcp_send_head(sk))
			break;
		if (!tcp_skb_timedout(sk, skb))
			break;

		tcp_skb_mark_lost(tp, skb);
	}

	tp->scoreboard_skb_hint = skb;

	tcp_verify_left_out(tp);
}

/* Mark head of queue up as lost. With RFC3517 SACK, the packets is
 * is against sacked "cnt", otherwise it's against facked "cnt"
 */
static void tcp_mark_head_lost(struct sock *sk, int packets)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct sk_buff *skb;
	int cnt, oldcnt;
	int err;
	unsigned int mss;

	WARN_ON(packets > tp->packets_out);
	if (tp->lost_skb_hint) {
		skb = tp->lost_skb_hint;
		cnt = tp->lost_cnt_hint;
	} else {
		skb = tcp_write_queue_head(sk);
		cnt = 0;
	}

	tcp_for_write_queue_from(skb, sk) {
		if (skb == tcp_send_head(sk))
			break;
		/* TODO: do this better */
		/* this is not the most efficient way to do this... */
		tp->lost_skb_hint = skb;
		tp->lost_cnt_hint = cnt;

		if (after(TCP_SKB_CB(skb)->end_seq, tp->high_seq))
			break;

		oldcnt = cnt;
		if (tcp_is_fack(tp) || tcp_is_reno(tp) ||
		    (TCP_SKB_CB(skb)->sacked & TCPCB_SACKED_ACKED))
			cnt += tcp_skb_pcount(skb);

		if (cnt > packets) {
			if (tcp_is_sack(tp) || (oldcnt >= packets))
				break;

			mss = skb_shinfo(skb)->gso_size;
			err = tcp_fragment(sk, skb, (packets - oldcnt) * mss, mss);
			if (err < 0)
				break;
			cnt = packets;
		}

		tcp_skb_mark_lost(tp, skb);
	}
	tcp_verify_left_out(tp);
}

/* Account newly detected lost packet(s) */

static void tcp_update_scoreboard(struct sock *sk, int fast_rexmit)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (tcp_is_reno(tp)) {
		tcp_mark_head_lost(sk, 1);
	} else if (tcp_is_fack(tp)) {
		int lost = tp->fackets_out - tp->reordering;
		if (lost <= 0)
			lost = 1;
		tcp_mark_head_lost(sk, lost);
	} else {
		int sacked_upto = tp->sacked_out - tp->reordering;
		if (sacked_upto < fast_rexmit)
			sacked_upto = fast_rexmit;
		tcp_mark_head_lost(sk, sacked_upto);
	}

	tcp_timeout_skbs(sk);
}

/* CWND moderation, preventing bursts due to too big ACKs
 * in dubious situations.
 */
static inline void tcp_moderate_cwnd(struct tcp_sock *tp)
{
	tp->snd_cwnd = min(tp->snd_cwnd,
			   tcp_packets_in_flight(tp) + tcp_max_burst(tp));
	tp->snd_cwnd_stamp = tcp_time_stamp;
}

/* Lower bound on congestion window is slow start threshold
 * unless congestion avoidance choice decides to overide it.
 */
static inline u32 tcp_cwnd_min(const struct sock *sk)
{
	const struct tcp_congestion_ops *ca_ops = inet_csk(sk)->icsk_ca_ops;

	return ca_ops->min_cwnd ? ca_ops->min_cwnd(sk) : tcp_sk(sk)->snd_ssthresh;
}

/* Decrease cwnd each second ack. */
static void tcp_cwnd_down(struct sock *sk, int flag)
{
	struct tcp_sock *tp = tcp_sk(sk);
	int decr = tp->snd_cwnd_cnt + 1;

	if ((flag & (FLAG_ANY_PROGRESS | FLAG_DSACKING_ACK)) ||
	    (tcp_is_reno(tp) && !(flag & FLAG_NOT_DUP))) {
		tp->snd_cwnd_cnt = decr & 1;
		decr >>= 1;

		if (decr && tp->snd_cwnd > tcp_cwnd_min(sk))
			tp->snd_cwnd -= decr;

		tp->snd_cwnd = min(tp->snd_cwnd, tcp_packets_in_flight(tp) + 1);
		tp->snd_cwnd_stamp = tcp_time_stamp;
	}
}

/* Nothing was retransmitted or returned timestamp is less
 * than timestamp of the first retransmission.
 */
static inline int tcp_packet_delayed(struct tcp_sock *tp)
{
	return !tp->retrans_stamp ||
		(tp->rx_opt.saw_tstamp && tp->rx_opt.rcv_tsecr &&
		 before(tp->rx_opt.rcv_tsecr, tp->retrans_stamp));
}

/* Undo procedures. */

#if FASTRETRANS_DEBUG > 1
static void DBGUNDO(struct sock *sk, const char *msg)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct inet_sock *inet = inet_sk(sk);

	if (sk->sk_family == AF_INET) {
		printk(KERN_DEBUG "Undo %s %pI4/%u c%u l%u ss%u/%u p%u\n",
		       msg,
		       &inet->daddr, ntohs(inet->dport),
		       tp->snd_cwnd, tcp_left_out(tp),
		       tp->snd_ssthresh, tp->prior_ssthresh,
		       tp->packets_out);
	}
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	else if (sk->sk_family == AF_INET6) {
		struct ipv6_pinfo *np = inet6_sk(sk);
		printk(KERN_DEBUG "Undo %s %pI6/%u c%u l%u ss%u/%u p%u\n",
		       msg,
		       &np->daddr, ntohs(inet->dport),
		       tp->snd_cwnd, tcp_left_out(tp),
		       tp->snd_ssthresh, tp->prior_ssthresh,
		       tp->packets_out);
	}
#endif
}
#else
#define DBGUNDO(x...) do { } while (0)
#endif

static void tcp_undo_cwr(struct sock *sk, const int undo)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (tp->prior_ssthresh) {
		const struct inet_connection_sock *icsk = inet_csk(sk);

		if (icsk->icsk_ca_ops->undo_cwnd)
			tp->snd_cwnd = icsk->icsk_ca_ops->undo_cwnd(sk);
		else
			tp->snd_cwnd = max(tp->snd_cwnd, tp->snd_ssthresh << 1);

		if (undo && tp->prior_ssthresh > tp->snd_ssthresh) {
			tp->snd_ssthresh = tp->prior_ssthresh;
			TCP_ECN_withdraw_cwr(tp);
		}
	} else {
		tp->snd_cwnd = max(tp->snd_cwnd, tp->snd_ssthresh);
	}
	tcp_moderate_cwnd(tp);
	tp->snd_cwnd_stamp = tcp_time_stamp;
}

static inline int tcp_may_undo(struct tcp_sock *tp)
{
	return tp->undo_marker && (!tp->undo_retrans || tcp_packet_delayed(tp));
}

/* People celebrate: "We love our President!" */
static int tcp_try_undo_recovery(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (tcp_may_undo(tp)) {
		int mib_idx;

		/* Happy end! We did not retransmit anything
		 * or our original transmission succeeded.
		 */
		DBGUNDO(sk, inet_csk(sk)->icsk_ca_state == TCP_CA_Loss ? "loss" : "retrans");
		tcp_undo_cwr(sk, 1);
		if (inet_csk(sk)->icsk_ca_state == TCP_CA_Loss)
			mib_idx = LINUX_MIB_TCPLOSSUNDO;
		else
			mib_idx = LINUX_MIB_TCPFULLUNDO;

		NET_INC_STATS_BH(sock_net(sk), mib_idx);
		tp->undo_marker = 0;
	}
	if (tp->snd_una == tp->high_seq && tcp_is_reno(tp)) {
		/* Hold old state until something *above* high_seq
		 * is ACKed. For Reno it is MUST to prevent false
		 * fast retransmits (RFC2582). SACK TCP is safe. */
		tcp_moderate_cwnd(tp);
		return 1;
	}
	tcp_set_ca_state(sk, TCP_CA_Open);
	return 0;
}

/* Try to undo cwnd reduction, because D-SACKs acked all retransmitted data */
static void tcp_try_undo_dsack(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (tp->undo_marker && !tp->undo_retrans) {
		DBGUNDO(sk, "D-SACK");
		tcp_undo_cwr(sk, 1);
		tp->undo_marker = 0;
		NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPDSACKUNDO);
	}
}

/* Undo during fast recovery after partial ACK. */

static int tcp_try_undo_partial(struct sock *sk, int acked)
{
	struct tcp_sock *tp = tcp_sk(sk);
	/* Partial ACK arrived. Force Hoe's retransmit. */
	int failed = tcp_is_reno(tp) || (tcp_fackets_out(tp) > tp->reordering);

	if (tcp_may_undo(tp)) {
		/* Plain luck! Hole if filled with delayed
		 * packet, rather than with a retransmit.
		 */
		if (tp->retrans_out == 0)
			tp->retrans_stamp = 0;

		tcp_update_reordering(sk, tcp_fackets_out(tp) + acked, 1);

		DBGUNDO(sk, "Hoe");
		tcp_undo_cwr(sk, 0);
		NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPPARTIALUNDO);

		/* So... Do not make Hoe's retransmit yet.
		 * If the first packet was delayed, the rest
		 * ones are most probably delayed as well.
		 */
		failed = 0;
	}
	return failed;
}

/* Undo during loss recovery after partial ACK. */
static int tcp_try_undo_loss(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (tcp_may_undo(tp)) {
		struct sk_buff *skb;
		tcp_for_write_queue(skb, sk) {
			if (skb == tcp_send_head(sk))
				break;
			TCP_SKB_CB(skb)->sacked &= ~TCPCB_LOST;
		}

		tcp_clear_all_retrans_hints(tp);

		DBGUNDO(sk, "partial loss");
		tp->lost_out = 0;
		tcp_undo_cwr(sk, 1);
		NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPLOSSUNDO);
		inet_csk(sk)->icsk_retransmits = 0;
		tp->undo_marker = 0;
		if (tcp_is_sack(tp))
			tcp_set_ca_state(sk, TCP_CA_Open);
		return 1;
	}
	return 0;
}

static inline void tcp_complete_cwr(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	tp->snd_cwnd = min(tp->snd_cwnd, tp->snd_ssthresh);
	tp->snd_cwnd_stamp = tcp_time_stamp;
	tcp_ca_event(sk, CA_EVENT_COMPLETE_CWR);
}

static void tcp_try_keep_open(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	int state = TCP_CA_Open;

	if (tcp_left_out(tp) || tp->retrans_out || tp->undo_marker)
		state = TCP_CA_Disorder;

	if (inet_csk(sk)->icsk_ca_state != state) {
		tcp_set_ca_state(sk, state);
		tp->high_seq = tp->snd_nxt;
	}
}

static void tcp_try_to_open(struct sock *sk, int flag)
{
	struct tcp_sock *tp = tcp_sk(sk);

	tcp_verify_left_out(tp);

	if (!tp->frto_counter && tp->retrans_out == 0)
		tp->retrans_stamp = 0;

	if (flag & FLAG_ECE)
		tcp_enter_cwr(sk, 1);

	if (inet_csk(sk)->icsk_ca_state != TCP_CA_CWR) {
		tcp_try_keep_open(sk);
		tcp_moderate_cwnd(tp);
	} else {
		tcp_cwnd_down(sk, flag);
	}
}

static void tcp_mtup_probe_failed(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);

	icsk->icsk_mtup.search_high = icsk->icsk_mtup.probe_size - 1;
	icsk->icsk_mtup.probe_size = 0;
}

static void tcp_mtup_probe_success(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);

	/* FIXME: breaks with very large cwnd */
	tp->prior_ssthresh = tcp_current_ssthresh(sk);
	tp->snd_cwnd = tp->snd_cwnd *
		       tcp_mss_to_mtu(sk, tp->mss_cache) /
		       icsk->icsk_mtup.probe_size;
	tp->snd_cwnd_cnt = 0;
	tp->snd_cwnd_stamp = tcp_time_stamp;
	tp->rcv_ssthresh = tcp_current_ssthresh(sk);

	icsk->icsk_mtup.search_low = icsk->icsk_mtup.probe_size;
	icsk->icsk_mtup.probe_size = 0;
	tcp_sync_mss(sk, icsk->icsk_pmtu_cookie);
}

/* Do a simple retransmit without using the backoff mechanisms in
 * tcp_timer. This is used for path mtu discovery.
 * The socket is already locked here.
 */
void tcp_simple_retransmit(struct sock *sk)
{
	const struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	struct sk_buff *skb;
	unsigned int mss = tcp_current_mss(sk);
	u32 prior_lost = tp->lost_out;

	tcp_for_write_queue(skb, sk) {
		if (skb == tcp_send_head(sk))
			break;
		if (tcp_skb_seglen(skb) > mss &&
		    !(TCP_SKB_CB(skb)->sacked & TCPCB_SACKED_ACKED)) {
			if (TCP_SKB_CB(skb)->sacked & TCPCB_SACKED_RETRANS) {
				TCP_SKB_CB(skb)->sacked &= ~TCPCB_SACKED_RETRANS;
				tp->retrans_out -= tcp_skb_pcount(skb);
			}
			tcp_skb_mark_lost_uncond_verify(tp, skb);
		}
	}

	tcp_clear_retrans_hints_partial(tp);

	if (prior_lost == tp->lost_out)
		return;

	if (tcp_is_reno(tp))
		tcp_limit_reno_sacked(tp);

	tcp_verify_left_out(tp);

	/* Don't muck with the congestion window here.
	 * Reason is that we do not increase amount of _data_
	 * in network, but units changed and effective
	 * cwnd/ssthresh really reduced now.
	 */
	if (icsk->icsk_ca_state != TCP_CA_Loss) {
		tp->high_seq = tp->snd_nxt;
		tp->snd_ssthresh = tcp_current_ssthresh(sk);
		tp->prior_ssthresh = 0;
		tp->undo_marker = 0;
		tcp_set_ca_state(sk, TCP_CA_Loss);
	}
	tcp_xmit_retransmit_queue(sk);
}

/* Process an event, which can update packets-in-flight not trivially.
 * Main goal of this function is to calculate new estimate for left_out,
 * taking into account both packets sitting in receiver's buffer and
 * packets lost by network.
 *
 * Besides that it does CWND reduction, when packet loss is detected
 * and changes state of machine.
 *
 * It does _not_ decide what to send, it is made in function
 * tcp_xmit_retransmit_queue().
 */
static void tcp_fastretrans_alert(struct sock *sk, int pkts_acked, int flag)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	int is_dupack = !(flag & (FLAG_SND_UNA_ADVANCED | FLAG_NOT_DUP));
	int do_lost = is_dupack || ((flag & FLAG_DATA_SACKED) &&
				    (tcp_fackets_out(tp) > tp->reordering));
	int fast_rexmit = 0, mib_idx;

	if (WARN_ON(!tp->packets_out && tp->sacked_out))
		tp->sacked_out = 0;
	if (WARN_ON(!tp->sacked_out && tp->fackets_out))
		tp->fackets_out = 0;

	/* Now state machine starts.
	 * A. ECE, hence prohibit cwnd undoing, the reduction is required. */
	if (flag & FLAG_ECE)
		tp->prior_ssthresh = 0;

	/* B. In all the states check for reneging SACKs. */
	if (tcp_check_sack_reneging(sk, flag))
		return;

	/* C. Process data loss notification, provided it is valid. */
	if (tcp_is_fack(tp) && (flag & FLAG_DATA_LOST) &&
	    before(tp->snd_una, tp->high_seq) &&
	    icsk->icsk_ca_state != TCP_CA_Open &&
	    tp->fackets_out > tp->reordering) {
		tcp_mark_head_lost(sk, tp->fackets_out - tp->reordering);
		NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPLOSS);
	}

	/* D. Check consistency of the current state. */
	tcp_verify_left_out(tp);

	/* E. Check state exit conditions. State can be terminated
	 *    when high_seq is ACKed. */
	if (icsk->icsk_ca_state == TCP_CA_Open) {
		WARN_ON(tp->retrans_out != 0);
		tp->retrans_stamp = 0;
	} else if (!before(tp->snd_una, tp->high_seq)) {
		switch (icsk->icsk_ca_state) {
		case TCP_CA_Loss:
			icsk->icsk_retransmits = 0;
			if (tcp_try_undo_recovery(sk))
				return;
			break;

		case TCP_CA_CWR:
			/* CWR is to be held something *above* high_seq
			 * is ACKed for CWR bit to reach receiver. */
			if (tp->snd_una != tp->high_seq) {
				tcp_complete_cwr(sk);
				tcp_set_ca_state(sk, TCP_CA_Open);
			}
			break;

		case TCP_CA_Disorder:
			tcp_try_undo_dsack(sk);
			if (!tp->undo_marker ||
			    /* For SACK case do not Open to allow to undo
			     * catching for all duplicate ACKs. */
			    tcp_is_reno(tp) || tp->snd_una != tp->high_seq) {
				tp->undo_marker = 0;
				tcp_set_ca_state(sk, TCP_CA_Open);
			}
			break;

		case TCP_CA_Recovery:
			if (tcp_is_reno(tp))
				tcp_reset_reno_sack(tp);
			if (tcp_try_undo_recovery(sk))
				return;
			tcp_complete_cwr(sk);
			break;
		}
	}

	/* F. Process state. */
	switch (icsk->icsk_ca_state) {
	case TCP_CA_Recovery:
		if (!(flag & FLAG_SND_UNA_ADVANCED)) {
			if (tcp_is_reno(tp) && is_dupack)
				tcp_add_reno_sack(sk);
		} else
			do_lost = tcp_try_undo_partial(sk, pkts_acked);
		break;
	case TCP_CA_Loss:
		if (flag & FLAG_DATA_ACKED)
			icsk->icsk_retransmits = 0;
		if (tcp_is_reno(tp) && flag & FLAG_SND_UNA_ADVANCED)
			tcp_reset_reno_sack(tp);
		if (!tcp_try_undo_loss(sk)) {
			tcp_moderate_cwnd(tp);
			tcp_xmit_retransmit_queue(sk);
			return;
		}
		if (icsk->icsk_ca_state != TCP_CA_Open)
			return;
		/* Loss is undone; fall through to processing in Open state. */
	default:
		if (tcp_is_reno(tp)) {
			if (flag & FLAG_SND_UNA_ADVANCED)
				tcp_reset_reno_sack(tp);
			if (is_dupack)
				tcp_add_reno_sack(sk);
		}

		if (icsk->icsk_ca_state == TCP_CA_Disorder)
			tcp_try_undo_dsack(sk);

		if (!tcp_time_to_recover(sk)) {
			tcp_try_to_open(sk, flag);
			return;
		}

		/* MTU probe failure: don't reduce cwnd */
		if (icsk->icsk_ca_state < TCP_CA_CWR &&
		    icsk->icsk_mtup.probe_size &&
		    tp->snd_una == tp->mtu_probe.probe_seq_start) {
			tcp_mtup_probe_failed(sk);
			/* Restores the reduction we did in tcp_mtup_probe() */
			tp->snd_cwnd++;
			tcp_simple_retransmit(sk);
			return;
		}

		/* Otherwise enter Recovery state */

		if (tcp_is_reno(tp))
			mib_idx = LINUX_MIB_TCPRENORECOVERY;
		else
			mib_idx = LINUX_MIB_TCPSACKRECOVERY;

		NET_INC_STATS_BH(sock_net(sk), mib_idx);

		tp->high_seq = tp->snd_nxt;
		tp->prior_ssthresh = 0;
		tp->undo_marker = tp->snd_una;
		tp->undo_retrans = tp->retrans_out;

		if (icsk->icsk_ca_state < TCP_CA_CWR) {
			if (!(flag & FLAG_ECE))
				tp->prior_ssthresh = tcp_current_ssthresh(sk);
			tp->snd_ssthresh = icsk->icsk_ca_ops->ssthresh(sk);
			TCP_ECN_queue_cwr(tp);
		}

		tp->bytes_acked = 0;
		tp->snd_cwnd_cnt = 0;
		tcp_set_ca_state(sk, TCP_CA_Recovery);
		fast_rexmit = 1;
	}

	if (do_lost || (tcp_is_fack(tp) && tcp_head_timedout(sk)))
		tcp_update_scoreboard(sk, fast_rexmit);
	tcp_cwnd_down(sk, flag);
	tcp_xmit_retransmit_queue(sk);
}

static void tcp_valid_rtt_meas(struct sock *sk, u32 seq_rtt)
{
	tcp_rtt_estimator(sk, seq_rtt);
	tcp_set_rto(sk);
	inet_csk(sk)->icsk_backoff = 0;
}

/* Read draft-ietf-tcplw-high-performance before mucking
 * with this code. (Supersedes RFC1323)
 */
static void tcp_ack_saw_tstamp(struct sock *sk, int flag)
{
	/* RTTM Rule: A TSecr value received in a segment is used to
	 * update the averaged RTT measurement only if the segment
	 * acknowledges some new data, i.e., only if it advances the
	 * left edge of the send window.
	 *
	 * See draft-ietf-tcplw-high-performance-00, section 3.3.
	 * 1998/04/10 Andrey V. Savochkin <saw@msu.ru>
	 *
	 * Changed: reset backoff as soon as we see the first valid sample.
	 * If we do not, we get strongly overestimated rto. With timestamps
	 * samples are accepted even from very old segments: f.e., when rtt=1
	 * increases to 8, we retransmit 5 times and after 8 seconds delayed
	 * answer arrives rto becomes 120 seconds! If at least one of segments
	 * in window is lost... Voila.	 			--ANK (010210)
	 */
	struct tcp_sock *tp = tcp_sk(sk);

	tcp_valid_rtt_meas(sk, tcp_time_stamp - tp->rx_opt.rcv_tsecr);
}

static void tcp_ack_no_tstamp(struct sock *sk, u32 seq_rtt, int flag)
{
	/* We don't have a timestamp. Can only use
	 * packets that are not retransmitted to determine
	 * rtt estimates. Also, we must not reset the
	 * backoff for rto until we get a non-retransmitted
	 * packet. This allows us to deal with a situation
	 * where the network delay has increased suddenly.
	 * I.e. Karn's algorithm. (SIGCOMM '87, p5.)
	 */

	if (flag & FLAG_RETRANS_DATA_ACKED)
		return;

	tcp_valid_rtt_meas(sk, seq_rtt);
}

static inline void tcp_ack_update_rtt(struct sock *sk, const int flag,
				      const s32 seq_rtt)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	/* Note that peer MAY send zero echo. In this case it is ignored. (rfc1323) */
	if (tp->rx_opt.saw_tstamp && tp->rx_opt.rcv_tsecr)
		tcp_ack_saw_tstamp(sk, flag);
	else if (seq_rtt >= 0)
		tcp_ack_no_tstamp(sk, seq_rtt, flag);
}

static void tcp_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
	const struct inet_connection_sock *icsk = inet_csk(sk);
	icsk->icsk_ca_ops->cong_avoid(sk, ack, in_flight);
	tcp_sk(sk)->snd_cwnd_stamp = tcp_time_stamp;
}

/* Restart timer after forward progress on connection.
 * RFC2988 recommends to restart timer to now+rto.
 */
static void tcp_rearm_rto(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (!tp->packets_out) {
		inet_csk_clear_xmit_timer(sk, ICSK_TIME_RETRANS);
	} else {
		inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
					  inet_csk(sk)->icsk_rto, TCP_RTO_MAX);
	}
}

/* If we get here, the whole TSO packet has not been acked. */
static u32 tcp_tso_acked(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_sock *tp = tcp_sk(sk);
	u32 packets_acked;

	BUG_ON(!after(TCP_SKB_CB(skb)->end_seq, tp->snd_una));

	packets_acked = tcp_skb_pcount(skb);
	if (tcp_trim_head(sk, skb, tp->snd_una - TCP_SKB_CB(skb)->seq))
		return 0;
	packets_acked -= tcp_skb_pcount(skb);

	if (packets_acked) {
		BUG_ON(tcp_skb_pcount(skb) == 0);
		BUG_ON(!before(TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq));
	}

	return packets_acked;
}

/* Remove acknowledged frames from the retransmission queue. If our packet
 * is before the ack sequence we can discard it as it's confirmed to have
 * arrived at the other end.
 */
static int tcp_clean_rtx_queue(struct sock *sk, int prior_fackets,
			       u32 prior_snd_una)
{
	struct tcp_sock *tp = tcp_sk(sk);
	const struct inet_connection_sock *icsk = inet_csk(sk);
	struct sk_buff *skb;
	u32 now = tcp_time_stamp;
	int fully_acked = 1;
	int flag = 0;
	u32 pkts_acked = 0;
	u32 reord = tp->packets_out;
	u32 prior_sacked = tp->sacked_out;
	s32 seq_rtt = -1;
	s32 ca_seq_rtt = -1;
	ktime_t last_ackt = net_invalid_timestamp();

	while ((skb = tcp_write_queue_head(sk)) && skb != tcp_send_head(sk)) {
		struct tcp_skb_cb *scb = TCP_SKB_CB(skb);
		u32 acked_pcount;
		u8 sacked = scb->sacked;

		/* Determine how many packets and what bytes were acked, tso and else */
		if (after(scb->end_seq, tp->snd_una)) {
			if (tcp_skb_pcount(skb) == 1 ||
			    !after(tp->snd_una, scb->seq))
				break;

			acked_pcount = tcp_tso_acked(sk, skb);
			if (!acked_pcount)
				break;

			fully_acked = 0;
		} else {
			acked_pcount = tcp_skb_pcount(skb);
		}

		if (sacked & TCPCB_RETRANS) {
			if (sacked & TCPCB_SACKED_RETRANS)
				tp->retrans_out -= acked_pcount;
			flag |= FLAG_RETRANS_DATA_ACKED;
			ca_seq_rtt = -1;
			seq_rtt = -1;
			if ((flag & FLAG_DATA_ACKED) || (acked_pcount > 1))
				flag |= FLAG_NONHEAD_RETRANS_ACKED;
		} else {
			ca_seq_rtt = now - scb->when;
			last_ackt = skb->tstamp;
			if (seq_rtt < 0) {
				seq_rtt = ca_seq_rtt;
			}
			if (!(sacked & TCPCB_SACKED_ACKED))
				reord = min(pkts_acked, reord);
		}

		if (sacked & TCPCB_SACKED_ACKED)
			tp->sacked_out -= acked_pcount;
		if (sacked & TCPCB_LOST)
			tp->lost_out -= acked_pcount;

		tp->packets_out -= acked_pcount;
		pkts_acked += acked_pcount;

		/* Initial outgoing SYN's get put onto the write_queue
		 * just like anything else we transmit.  It is not
		 * true data, and if we misinform our callers that
		 * this ACK acks real data, we will erroneously exit
		 * connection startup slow start one packet too
		 * quickly.  This is severely frowned upon behavior.
		 */
		if (!(scb->flags & TCPCB_FLAG_SYN)) {
			flag |= FLAG_DATA_ACKED;
		} else {
			flag |= FLAG_SYN_ACKED;
			tp->retrans_stamp = 0;
		}

		if (!fully_acked)
			break;

		tcp_unlink_write_queue(skb, sk);
		sk_wmem_free_skb(sk, skb);
		tp->scoreboard_skb_hint = NULL;
		if (skb == tp->retransmit_skb_hint)
			tp->retransmit_skb_hint = NULL;
		if (skb == tp->lost_skb_hint)
			tp->lost_skb_hint = NULL;
	}

	if (likely(between(tp->snd_up, prior_snd_una, tp->snd_una)))
		tp->snd_up = tp->snd_una;

	if (skb && (TCP_SKB_CB(skb)->sacked & TCPCB_SACKED_ACKED))
		flag |= FLAG_SACK_RENEGING;

	if (flag & FLAG_ACKED) {
		const struct tcp_congestion_ops *ca_ops
			= inet_csk(sk)->icsk_ca_ops;

		if (unlikely(icsk->icsk_mtup.probe_size &&
			     !after(tp->mtu_probe.probe_seq_end, tp->snd_una))) {
			tcp_mtup_probe_success(sk);
		}

		tcp_ack_update_rtt(sk, flag, seq_rtt);
		tcp_rearm_rto(sk);

		if (tcp_is_reno(tp)) {
			tcp_remove_reno_sacks(sk, pkts_acked);
		} else {
			int delta;

			/* Non-retransmitted hole got filled? That's reordering */
			if (reord < prior_fackets)
				tcp_update_reordering(sk, tp->fackets_out - reord, 0);

			delta = tcp_is_fack(tp) ? pkts_acked :
						  prior_sacked - tp->sacked_out;
			tp->lost_cnt_hint -= min(tp->lost_cnt_hint, delta);
		}

		tp->fackets_out -= min(pkts_acked, tp->fackets_out);

		if (ca_ops->pkts_acked) {
			s32 rtt_us = -1;

			/* Is the ACK triggering packet unambiguous? */
			if (!(flag & FLAG_RETRANS_DATA_ACKED)) {
				/* High resolution needed and available? */
				if (ca_ops->flags & TCP_CONG_RTT_STAMP &&
				    !ktime_equal(last_ackt,
						 net_invalid_timestamp()))
					rtt_us = ktime_us_delta(ktime_get_real(),
								last_ackt);
				else if (ca_seq_rtt > 0)
					rtt_us = jiffies_to_usecs(ca_seq_rtt);
			}

			ca_ops->pkts_acked(sk, pkts_acked, rtt_us);
		}
	}

#if FASTRETRANS_DEBUG > 0
	WARN_ON((int)tp->sacked_out < 0);
	WARN_ON((int)tp->lost_out < 0);
	WARN_ON((int)tp->retrans_out < 0);
	if (!tp->packets_out && tcp_is_sack(tp)) {
		icsk = inet_csk(sk);
		if (tp->lost_out) {
			printk(KERN_DEBUG "Leak l=%u %d\n",
			       tp->lost_out, icsk->icsk_ca_state);
			tp->lost_out = 0;
		}
		if (tp->sacked_out) {
			printk(KERN_DEBUG "Leak s=%u %d\n",
			       tp->sacked_out, icsk->icsk_ca_state);
			tp->sacked_out = 0;
		}
		if (tp->retrans_out) {
			printk(KERN_DEBUG "Leak r=%u %d\n",
			       tp->retrans_out, icsk->icsk_ca_state);
			tp->retrans_out = 0;
		}
	}
#endif
	return flag;
}

static void tcp_ack_probe(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);

	/* Was it a usable window open? */

	if (!after(TCP_SKB_CB(tcp_send_head(sk))->end_seq, tcp_wnd_end(tp))) {
		icsk->icsk_backoff = 0;
		inet_csk_clear_xmit_timer(sk, ICSK_TIME_PROBE0);
		/* Socket must be waked up by subsequent tcp_data_snd_check().
		 * This function is not for random using!
		 */
	} else {
		inet_csk_reset_xmit_timer(sk, ICSK_TIME_PROBE0,
					  min(icsk->icsk_rto << icsk->icsk_backoff, TCP_RTO_MAX),
					  TCP_RTO_MAX);
	}
}

static inline int tcp_ack_is_dubious(const struct sock *sk, const int flag)
{
	return (!(flag & FLAG_NOT_DUP) || (flag & FLAG_CA_ALERT) ||
		inet_csk(sk)->icsk_ca_state != TCP_CA_Open);
}

static inline int tcp_may_raise_cwnd(const struct sock *sk, const int flag)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	return (!(flag & FLAG_ECE) || tp->snd_cwnd < tp->snd_ssthresh) &&
		!((1 << inet_csk(sk)->icsk_ca_state) & (TCPF_CA_Recovery | TCPF_CA_CWR));
}

/* Check that window update is acceptable.
 * The function assumes that snd_una<=ack<=snd_next.
 */
static inline int tcp_may_update_window(const struct tcp_sock *tp,
					const u32 ack, const u32 ack_seq,
					const u32 nwin)
{
	return (after(ack, tp->snd_una) ||
		after(ack_seq, tp->snd_wl1) ||
		(ack_seq == tp->snd_wl1 && nwin > tp->snd_wnd));
}

/* Update our send window.
 *
 * Window update algorithm, described in RFC793/RFC1122 (used in linux-2.2
 * and in FreeBSD. NetBSD's one is even worse.) is wrong.
 */
static int tcp_ack_update_window(struct sock *sk, 
 * INETk_buffn imb, u32 ack,
				P/IP pro_seq)
{
	
 * INEtcp_T		An tp =	operak(sk);
	int flag = 0;
	/IP nwin = ntohs(operhdr(skb)->window);

	if (likely(!
 *		interfacesyn))
		e  BS<<= tp->rx_opt.snd_wscale means oopermay_update_ as th(tp, prote for th,he  B)) {
		mente|= FLAG_WIN_UPDATE;
		operon Contrlotocol(Tr the meahe Trap->entatnd != Authos:	Ro	*		Mark Evan, <evaet.OR	/* Note, it is the only place, where	Cor * fast path<wf-recovered for sending TCP.net>
 /p@uhura.pred_mentsed usinempen,		Fl_rian_checNET is ck@kns oe  BS>*
 *	maxtrol Prnsmp@uhhura.rvalds@cs.c.uk>
 *	inki.peraync_mss(implinet_cINET i->icsk_pmtu_cookieis i		}
.west}

hura.astouna = pro		Lireturnement;
}

/* A very conservative spurious RTO resp		Jo algorithm: reduce cEvanand
 * *		tinue in *		gestion avoidance.
*		Cstatic nsmi	oper*		Jorge Cwi_, <j_to_r.satlin*
 * INEoperating sye LIN*		Mark hange= min *		Mark hang,*
 *	entassthreshis ismit timer h_cnted usin*		Mbytes_acker reusinTCP_ECN_queue_cwr(tpw connsmioderConthangrenamivg.unit.*		Jorge Cwik, <jorge@laser.satlink.net>
 */

/*
 * Changeusing *				Er halvuni-and	Pedro Roque	:	Fast Retransmit/Recovery.
 *					Two rec		Erw start
 *					Retransmit queue T		An imCP.
 *cp_enterble rimpl0st Ret.
 *					Two recundoenk	:	Delayed ACK bug fixes.
 *		>
 *lemente LINns omente&
 *		FECEser g.
 *		Eric Schenk	:	Delayed ACK b is iels.net avoidancloyd sty1st RetranF-lase, <jorge@lasedetec Retranet>
 */ (RFC4138)
 * *		previoaffects duruni-two new ACKs followuni-lase(well, almost, seoqueline *		Pemments). St slo(ACK number)<wf-keptque	frto_couk	:	. Whenss R advt/Res *			Alan C(but not to or beyond highest sequencestudt beforen_re): *		  On Firstss R, stud.nd processegrune_ outcoveequencSectimeers clasewas f commous packe. Doous packetr.satlink(r.satlinng SACsure we never aen	:	Movedisnce opart of-rch!previo		Andi Kleen	:	MoveMake sure we never acgivenque	open_re esencan brey lected sepa		Erly)ing SOtherwindi basically on duplic sloACK)sequence iof comm) caused by a losnts ialgoTCP falls backof
 conven Retal@laser.che, y.hecking for s he, ri.uning Sof Nagle, thisere done	:	Fixey Savochkin: .
 *es 2. MSS3lay.En arocesdatang S *					th Pany sizrey Sav					anpreviplem slo2ere upgradedof
 3coveng SRalayed e: i				enquence i, <jorge,rocess RSTshould arrCwikfrom-rch withrigied a	Alan Ce fataf	J Hwe transmitnd proces 	An
 *					rudit of ST meversionck sequon f numbstep, wait until/modulecumule CwikT meafoundst inen move tong SACrch!sue to .h>
. Int/dst.h>
#in/kern nexmbers decidee <linux/previo thempleruneed (mainns wdreyour fundi Klsck sequ-r avoise_y Sa(es.
 h MSStoroom rmineansm)
 *is mighusenough d.h>
#includnk	:	Fet/netdprn.
 esimestuhlberonion
 if>
#includeh MSard <wfMake surasured suppoclude <net/netdshowed green lightd.h>
#includprocessl_tcp_wihandles incomast s RSTti,
 *		PanuAlso make this c;
int sysctl_tcp_MTU,etdma.d_mostlnsmissreere is nenough evidAndrMake surto prh>
#ihatmission
  thendeappe *					daItith spferf-rch!PedrrolMake sur
 *		previotol_tcp_ad delayed acks.
 *		Andovery.
 *			ow zint sysctl_tcp_reiller	:	Don't allow zero congeUX
 *		operating system.  INET is c Scheverify_left_outrenami
rey Dmss to ma#incbehavior __reaLoss __read(		Flreth sp_alert)*		Chstion window.
 *DATA_ACKEDser  *		Matthew Dillon,_read_mmit<hedric syscton window.
 *NONHEAD_RETRANSsave __ ||
	sure( *		Mpath.
 *		J H>= 2) &&ion window.
 *t sysctlics_save __user *		Midancmarkerrate_rcvbuf !ochkin *		Mark unaandliny Savstamefinors:	Roysctl_tcp_ecn __read allmostly;

#define F== 1 ? 2 : 3),zero cllo.n, <agu1;ic.com_UPDAScheis_sacket/netpors:	Ro/* error ifshortead_mosin>
#in 2;bugs are lso have case c):
t>
 *<netisn'ahtiss to manoreasuremen		F-RTO, e.g., opposite dirYN_ACK 	An,		F-on ConYN_AC	Cha_UPDAon window.
 *ANY_PROGRESSDATA		0x01 /* IncoNOT_DUPuser ata.		*/
#d			*/
#define FLAG_ECme containedsmp@uhu	*/
#define FLAG_DATA_ACKED		0x04 /* This ACK ac0nowled4pts@_readed new daata.		*/
#de.oic. endss:	Ro	0x80 /* SACK detected data lATA		KED		0x04 /* This ACKlossage./* Prfor .
 *d.uni-ofRTOs
 */
.*		Charles 		Better retransmit timer hacks for wint s prets_in_f_mostCKED	update.*/
#define FORIG_SAmostly;

#define FLAG_DATAYN_Aead_define FLAG_ECFORWARD0x40 /* ECEc __FLAG_f __read_mostlyics_smm.hly non-head re define FLAG_ECONLY_ORIGCKed */
) */
#define"" "" some of which ( Kler prune above
int sRIG_SACKED	0x200 /* _ACKED	0x1000 /* NdefinFLAG_Nthis ACK				*/
#define FLLAG_DATA_LOST	.		*/
#define FLAG_SLOWPA3dged new date.*/
#define FLA the Traxmit sent before RTO 0x08 /* ansmissiD_UN_now needstly  Kleon Conh>
# slo.				*					Better re#define FLAG_DSACKING_ACK + 2 KempED		0x04 /* This )

#ORIG_SACED|FLAG_DATA_SACat ine FL		*/
#define FLAG_SLOWPA2dged new  data.		*/
#defG_ONLY_ORIswitch (sysctl_linzhcn _r.satlin(FLAG_e FLA2SYN_*					nextransmitter so that LAG_DATA_SACbreakfinee FLA1 make deleive queues.
 *					Retransmitenaminorld.
 */
sdefault make delnk	:	Fix retransmitter so that it sorld.
 */
sest.fine TCP_REMNANT (Trick@a.		*/
#define FLAG_W		NET_INC_STATS_BH(T		A_netat i, LINUX_MIB_TCPSPURIOUSRTOS	0x8}sen, <agu0t RetranTn throut_tcpdealed San Lread_mosacks,f it ce ooutgoA_ADVnes0 /* _mostly;
int syadu>
 * INET		An implementation of the TCP;
int sysctl_tcp_max *		Maonnndi Klrating llon =
 *		Matthew ;INUX
 *		operating system.  INET is i/IP prior_		Arnt Gul2 /* Incomiise, we.NL.Mug =imes_SKB_CB with theqking into unt,
		 * that SACKs.NL.Mugise, we make _DSACKINGise, we make fne FLAs imple make ine FLAs implem Savoter ree_rcv/* I				envariis oldersctlnindovorge@if p
>
 *ernelwine = TCobably ignkin:it.S + t sysctTE		0x0l(TCP make more caLAG_Sgof
 *ld
	lesport_header(skb);
ncludes
 */

wlow ve10 /y Savyet, discardS + sizisi Gurtov, ope793 due Retr3.9)/* If PSH isealinet, pa*		Mark nxte
		     *invaliull sized * to handle sucket should be
		 ss Biro
 *		FSND_UNA_ADVANCED_rcvbuf SH))

/* Adabc(FLAG_ns ollonDillon,ca_PROGRE<t,
		CA_CWRAG_SYN			Header predi+ulbra -TCP_MIN_MSS + st_s_ONLY& TCP_REMNANT))) {
			/*=nt,
		CA__frtne FL/*badlassume just fuli Gurtov,sctl network*		Charles ant (if peer istran RFC compliant),
		ecks for wi*/
#fi>
ss_cachpollodefiuding TCP heareful cTCP hea_tcp;e = len;_DSACKINGPROGRESS|FLAG_SND_UNA_ADVANC_WIN_UPDA		(FLAG_DATA|SLOWPATH non-  (len >= TCP_MIN_MSS + si0x08 /* WAlan Costlonstant, purella@wardeasuremede>
 y Mi mkin:s.edus are requiredicsk_ack.nyarw			f = 2;
faue htl_tSND.UNA>=kackWL2icsk_aS	(FLen, <waltje@uWalt.NL.Mugnet(FLAG_FORWnt GulbrandRoss Biro
 *		Fred N. van Kate.	*/ca_LAG_Sthe
 CA_EVENT_FASTsaveBITS (eg_size;
	unsigned int len;

	icsk->icsk_ackHPACKize = G_ONLY_ORIG_SAto accou!nt,
		 * that SACKsATA_ the 	eof(struct tcphics_	 * tcp = mieg_size;
	unsigned int len;

	icsk->icsk_ackPURE (quick
	unsigned  */
	leion Control Proimple TCPl(TCP).
 *
 net.ORG>
 ,
		 * that SACKs  pred = min(quickagw4ptacktag_write	Variask = inet_cket should be_incr_quickac				rcv_ecn_echACKE, = 0;	interfaLAG_SYss Biro
 *		FECp_sk(sk)->rcv_wnd / (2 * icsk->i_ackack.rcvsize k_ace pas MSS */

algogotrd < pred, re.h>
#		Pasoft errorS + slog. Somethast CK jed../* If PSREMNsk_err_onst nt lssP_REMNANT)) tcpe{
			ce.
 *					 if tstamystem.  time{
		mp	icsk->icine FLAreful cine FLAG				ic_UPDAvoid TCP_ECN_
		     *no	Variao __reaSKleeff(structtak)
{
	ion_sooff*					entl_tcp_mod Varia0 /* Singpong = 0;clean_rtxack.ato = Tuding TCP heaATO_MIN;
}

/* Send ne FLAG_FORWARD_PROGizeofa - skb_trat sysctl_tcp_rfc13e
 * real w/* Guarantee 
	icsk- reorde			an		Andi Kleegainst wrap-aroundsIf PSH is not se ACK was a windowsuper-low d be
		  ACK was a windowrate_rcvbuf t_connecis_duborgeDEMAND_CWRk->icsk_aAsureme CWND,		tpPROGRE		Proceatio0 /* Snbuf __read_mostlySACKs only non-!		tp->ecn_define FLansmissiraisric		:	p, struct oid tcp_meag_ansmiP_ECN >= TCP_MIN_DSACKINGk *icsinzhai._read_mostly; tcp_sock *CP_ECN_q-eue_cwr(struct ttocol scale _d new dcks > icsk->icsif (INET_ECN_is_ce(TCP_SKB_CB(skb)->flND_CWR;
		/* Funny extension: if ECT is not set define FNHEAD_RETRANS_ACKED	0x1000 /* NonG_SACK_RENEGING*/
#define Fdsicsk_firm(csk(sk)nack(st_seg_sen, <agu_LOSags & TC:ort_headeri_RCVM open	Exp a zerod SYN.		*ceptrwork off. 31;
waSS + sbeast h>

int tati= 2;
uick &,de(coist tcphdr) farestamplen >=S + siof S)
#defibPUSHEK acmalR;
}

statsnclu/* If PSH isgw4ptATA_hea exttatic _connecuick at it stcphdr *th)rly.
		    :
	SOCK_DEBUGP_ECN"Ack %udealing%u:%u\n"ension:2 /* Incoming ACKlow mtu )
		tp->ec-_LOS full s:{
	if ckack(sk);
	icsk->icsk_apdate.	*/
	icsk->icsk_ack.ato = TCP_ATO_MIN;
}

/* Sen) & TCP_REMNANT))) {
			/*mp option OpenND_CWR;
	try_keep_CP_Eat it sdefiN_OK;
}

static inline ochkin:P_ECN_rcv_ecn_echo(struct tcp_sock *tp, struct tcpkb->len mLooklla@stcp opalign. Nhdr *re_rclytly = 2;on SYNde(coSYN<netine FLAing SButt in thruct	*/
#bine t.mss_clCP_ECN_qin= 2;
establish<flllow suppng S tcp_ist>
#inclu bedmemfail+ 16 /
		Two recparsemplelignn = skb_son of the TCP queue hand}

/* 2_received *opt_rxACKEsure weow zdbuf e LINunsigned char *ptrcv_mss = len;hdr *th_flags 	interfas implelengffer,(th->dN_QU* 4) -asi Soft queue hanhdrruct ptT (T( is split to tw)(th + on a	esh)
 k->iwpingpong;
e_rcvwhile (rd and > 0skb)) &ov,
pcode =two p++/* Bu fromsi S{
	stP_FLAG_Pom netalue used tTCPOPT_EOL maken, <ag*/
statict can bNOP: /* "ef:to a orrest 8)) all1*		Charrd and--k = iPedro Roct sk_buff *skb)lamp iwork.
 * wind {
		ion" bu<G_DA/* "si(sk)-

/* 2"*		Charthan
 * tcpss window_cl>ard and_UPDATE|FLAG;}

sdo10 /[2]);not
 ialsmoother*		Charimal advertised windp_full_space(MSS makess window_clmp optOLENes. ATA	ertisynKB_CBll_spaelsinki	u16 inmpr. = get}

sl spli_be16(pt_wndt "sl& TCPart"
ed at "slss windd to iuserrt"
 AG_NOTNOT_DUP
 * It is used fo<start"
_UPDATectiart"
 * 
 * It is used fr behavired to ick.lalpong;
at senr behavest."applicald.
 */
sp_full_space(WINDOW8)
 *
 * rcv_ssthresh is mor.
 * -trict window_cN RFC,
		amp usATA	SH))

/* Ad as th_ion inged at "sl__u8 entation owork(* The*)o partion
 *   reqtion o_o_ECNefineection. scheme doe> 14is connection.*		M *		Aimit(N_UPDAT		 = lntk(KERN_INFO "wmem[2]);
}

/* 2: Illegi:		F-RTO " But it sure we". Check value %d >14.
 *v_sst._rcvve to rely on qentation oer behav	 scheme does 14cant "applicaired to isscheme does entation of lication buffer". It is check #1TIMESTAMP8)
 *
 * rrcv_ssthresh is mor);
	/* OpFLAG_NOT_read_mion
 *   
 * It iingpon_ok Non-hehead rextion
 *   of receivertati inlisa advance *sk, constolate schedus openin
 * It ick.pinval * phase to predict32urther behavw) {
		if (truecT (T<= skb->len)
			return + 4er behation buffer". It is check #1mm.h_PERM8)
 *
 * rcv_ssthresh is morstatic voue because of misprediction
 *   of receiver
	ic 1;

	while (tp->rcckegments openintp = tcpthe et.
 * It;
		window >>= 1;
	}}
	return 0;
}

stattimize this! */
	i>=h->ecct sock *sBASE +struct sock *sk, BLOCK)n_from_space!is! */
	i-struct sock *sy_pr) %sure) {
		int incr;

		of mispredi
	/* Check #1 *ed at "slckack(sk);
	icsk->icsk_
 * e >>-G_DA-* "application bufth{
	struct tcp_sock #ifdef CONFIG_ack_MD5SIGnection to lossesp_gro8)
 *
/* = tcp maye MD5 Hash has already been) {
			ts.edupsec KleILE;
{4,6}_do, if()ws
	>
 *		Charvmss;
		e_UNAflo.weslampe >>=er, si S-

#dedow_cla - 1;
		}
_ECE)
#deast retran;
int sys2]);
to predi_rmem[2])t queue handled by Twindow (windk" buffe LIN_			retwo p
 * )
{
	strffer", requ{
	if ruct t= htonl(    !ce(),  << 24) |	    !vmss + MAX16ND_CW P_HEADER + );
	/* Op MAX8CP_Htruesize = tcp_win->ecn_f *		Implemecv_ssthresh <= wi++when se-sized segmf (truesize Sockl	int er beto window and correspondink_ack. will fit to outa.		*/
#defidbuf(struct sock F	Florighpundmem = tay jihope#defi->rx_
				rmem[2]) ing SIfrd <wf-wroch wt*					work wo 1;
in[2]);
}

/* 2.di Klzed frames.
	 */
hai.rut]);
}

/* 2. Tuning advertised window (windk" buffCN RFC,
 queue handled by TCP.
 gs & rtised i==iver window (tp->rcv_w >>G_DA mss-sized segments
	 * willast_sbuf(structnux follne FLAG_	Implem;
	int wiags))
			rs
 *    est(ver window (tp->rcv_w>>2)+    !tcp_T/* Op_ALIGNED& SOskb)) & TCers established state. */

sount hine FLAG_DATA_LOit twmem[2]);
}

/* 2.  TCP&-sized segt on ast retransm}
	else
			incr = __tcp_grow/t of Ptcp_wcv_sSignatK_PUmootheuf = workcvmem)
		smd5sigow_clamndow (tp->rcv_ sock *skorward and advertised iMAX_eceiver wi (sockise,8truct tcpu8tp = tcp_sk(skt_header(s)
 *moothe<wf-rooome ofatic ructme of cutIf PSH isrd and  Subts morep_groND_Cn, <aguNULLing/application
 * latencies from network.
 * window_clamp is maximal avertised window. It can be less than
 *rvationcp_full_space(), iwindow_clamp
 * is reserved for "application" buffer. The less window_clamptic point of network, but the lmp + tp->ss wind netwoesh isskb);

		_UPDATE|FLAG_when seest.ck |= 1;
		}
ize)tp->* 3. Tuning rcvbuf,t to work.vation}sk)->icsed frames.	:	B			Two recstore_tp, rcvd / queue handled by TCP.
 *			ck *tp = w(strucreful c corresponding s>icsk_ac *tp = tcp_sk(sc inlick.rcv_/dst.hs(e fast retrantic void tcp_clarerd@rendow(struct sock *sk)
{
	strucCP/IP  the LINt tcp_sock *tp ents
	 * wiB_CBo hand*
 * sk_ack.pwuED	0x08 /* PAWS busock *ck *tp wrt.KED		framux/kernatomic This obs			textrash + i3 * sndmakes sK_PU   sizeof->rx_happcv_ss
 *	orACK_PUlocated) <.  -DaveMc),
	;
	}
}

ssk_rm.
 *so>sk_occurSTs r expUSHEmss)
		rcvmemsk)
{
vbuf(sk);
	ifwsrs.edu>>rcvq_space.s0AND_CWR;
	mp_window(structenaminn conN_OKorry,atomic					ecifi 2;
s brokeof(y_alCK_P-s RST2]);
	}it of I <wf-ce ofatal.ecn_flags<net/oes _not_t tonge criteasuo_responseqsx20 /cs.hng Sit mighquic thrack _stackonne syscts for openHedr to ma
int imem)hatly =ation (if it e havenh MSSc) >|= TCP_EC it m	Fast Retransmit/Re orng Sndbuftl_tcp_mod. Morehe, atic for dere abl
#in paghen slo		AnDVANsuchdrei Ge to P_ECN ehere
 (tp->* Reppp->aome ssk(s "rcvbuy"in the pr~	CheACKinN_OK;
ngpongspaRecoveng SAll>sk_se meaic_rto_rill dflagt gatic inli_quicwWR;
jec tcp_speo in ack _onf SACK j of SACstam bandwidthN suppo *		Andreygned La Rocycostlndbuly,ng Sesenint = min(hike us _rcv__rcv_ted Slluct no>
 rss().nd;

	hint here
 *		Pedack.rcv sejorgely_space(mati10 /lsk)
nirelaesenalas.
 * RCLa Rok(sk>icsk_ggy	sk-en(tp->ent, tp[ La	J Hnote. E fatworse! * We hRS) byla@stu					re_f SA_	0x400 RFCdrei di Sal_quicReceiverrnel;
}

static <linuxdealingrolahti:	k_modeK_ACarecove * We ha blaICSK lie. VJv/ranst ab !icdetect and fix ! 8)8) * We sk->sk_sbigast taticlemin(hlarge pow of nt, TCP_for df SACmi ackTCP_ECN_wii KleeK, let's		a fck.r = tcpsk(sk)		F-RTOy dirpeer cl		Anivoid tc1hzard <wf-safostlyup, str;
	hint DVAN18Gigabit/sec.to s]uf = ed frames.
	 */
disP_ECNeull s( |= Ilementati		An impl sample in laron of the Te LINUX
 *		operating system.  INET is is: "network" buffer, allocated
 * fk->sk_uunt,
		 * that SACKs block is variable.
		 *
		 * "len" is inve = tp->r(/* 1. P sysctl_f SACcorrwnd  *		Andreer fix0 /* Sners
  & T   oicatck)
		icsk->icsk_ack.quicke
		 * non-sk_ack.pmtu of m08 /* 2. ...de.
 *mss to make
0 /* Snvariaeful check ta* else wit3 timestampsatioly thn Conn_dep)
{

{
	sN|TCP_FLAon Control Protocol(TCP*
 * Aocket
hce as the> 4 
 *		Implementation o * else wit4 timestampsoderiil ot win_dep)
{
minimus32)cp_sock *tp = cp_sk(skt. It  corresponding s) <=ur omostly;

int sysctto * 10_TCP/ HZ_ack.quick = 0;

	if enters esws 0) is oe sample in larger samp = tcp_wes in the non-timestamp
		 * c sample in la_orphans __read_mostly = NR_Fn, <aguN|TCPmin(tp->window_clamp, 2U __ttomiive quefit tosure weN|TCP0) {
		/* If wek = inest RetranCrcvbu Gurtov,only going for v/rady.
		ity <linux/mGurtov,_adv_wiICSK_A |= i		/* ->rcv_(tp-><net/d					 to doderly = 2;	Alan CealingtruncCP (rine void tcp_r. Acceptabilitng (Dof<http:(+ MAX_T, FIN,truccour val siz + incren.
 *		Anc.eduK)
	 - tpataack.atoedge) > sa<lin int, tp->soples ->rcv_(RSTsock.h>
 ful)mss().
				ek	:	Fix RCV.WUP uct ea
 *		of_csk(NXT. P newache);
id * Theskb)
{
lagskack(stdetailwostlydelay
	hint, sy = attimekack(st<=ourcsk(sk)c.edu(borrfack 
 *		freebsdt hiif (new_sampline void tcp *		Andrcp_rmem[2] &&
	    !(sk->sk_uCP/IP k.quick ={xt, tp->	ATE		0x0k.quickmory_pressure of mis   !tcp_memory_pressnxt +->sk_rc Slowtrol ProtoK	0xV_MSS ix RTwe.rcv a.
 *etp->rdy = _ECN_OKouble ACK bug.
 *sh < bug fixes.
 *		Eric cp_in_wenio_CWR;k.rcvstrucCV_MBSD>advsck.rbuff __read_asrcvq_s)ew_samP_FLAG_PSsk(sk)01.ps void_full_sp_SYN_SENT)
		csk(sk);
	 = ECONNREFUS!(tc buffer". = 0)
		reCLOSE_WAI	space = 2 * (tp->cPIPn Kem>rcvq_space.seq);

	sp)
		han
 * tck_buff *skbe = 2 * (tp->copied_SET#define FLAGd int but.timeN_OK;
}ADAND_Ccsk(sk);
	or_rcvot is  NR_FILE;*			 &&
		V_MSS.edu	Pysctl_			enoIN bx byay jiSACKint sn beime <est.une F

int CK j
 *	e.
 UF_LOCK)lagss*sk)
{
detailace;

y.
		nyart
 *			 *		Andrace igned mic_rochkin:tp->rx_;

	ihol_common.h	Iuct tcre 	/* BLISHED,if (tcv_ssthfiel.h>
syscine 

	sp-ce =d.
	buff erne easnto LASTusedde.
 fhti:d(&s

	spatic nevk *sk	:	d.
	);
	= 1;
quest c			 */
			sinLOCKce =-1>advmss;
			iOCK)inestimas simultaneouf da	cloink.emore gairl		spaceING< (tp-lsk of>advmsk_buff));
			while (tcp_win_from_spac2(rcvmem) < tp->adspace)
				ssk_buff))covery.
 *					Two recfi= maxwin; advertised window (wT		An implementatstruct sock *sktcp_max_orphans __read_mostly = NR_F)
		tp->_schedule_est.tisk(skrcv_rtt_hutdowniro
RCV_SHUTDOWNte t intset = space;

		if ONEtp->rc|| tp->rcv_rtt_est.rtt == 0)
		returnRECV:space.seq);pace /= tp-)
		/* Mh>
#in
				 ace =)
{
	strucTher01.ps.timeeq);

	space =
/* Bu
		tp->rcv_rtt_estthanpingpsk->h <= wi
	    (inace.seq);

	space = maace.seq);

	sIN	if ( thiscv_ssthf (t
{
	if ((tp-*					eno{
	sdoc),
		noion_s *sk)
{
	s>rcvq_space.seq);ss +savet at the corr: ReTCP_S->sk_snss + MAX_01.psew_samphe
 * problem is tFINace = voidn may jie FLAem_allosupportrcvmem += 12w_saICKAC *em_alloto ue m "lerk wiCWR;
m) < tp->ads:
  {
		s		J HK we	space =nly qu*sk)
{
	struc_ECN_seq;
	tp-al.  When a
 * connection stINGveM
 >rcvq_space.seq);cause
 *o mak the beginningOCK)--orrectMAX_TCP_nd_cwn);
	ace =ew_samp
 * queue.  -DaveM
 */
staticncluconnectio(sk);
	sttyle fcv(struct k_buff *skb/* O>rx_ few ISTEp + MAeq);

	sp
			ssctl->ledvmss,ents se FL_est.ugs are sizeoreaching lepie easructdmore of hisshould work
ERR "%s. Ompossibset rcv_rtt_est.=%dart of sure we__m/un__eceived, initinet_csk_schednt tcp_It  tcp packet rect, tp->rdefink *tTCP_ECNut-of-sock *_ealin_truc/* If Ptcphdr)For
 ugs are(tp->r_rtt_ly send. F tcphw dropnd am/* If PS__skb	Variabpurgendow_cout_of_P_ECN	Varia * fo(sk);
	RANS_DAint tcn_flags & w_measu>rcvq_spacercv_mk_mem= tclait tc	}
		}
		ipace = space;

		if (syscTS|TCPcv_rtt_est._t the >
 *		Linread
	hintrrectPOLL_HUP &&
	halfps diext any CN_OK) {
		cvq_space.time === me_stamp_MASKNon-head r= (icsk->icsed window
	spND_CWsk_wake_ats.aace;

		ifWAK);
	stD,sk->icsk_net_cends
 *	/* Too long gap. Apparently sender faileINize RCV_Mied to user space.
 * I) +  Wu Fdt sock *sk)
{) + bsampl*ste TCP rece = tcp_ve buffer space._UPDA  !tcp_memospack.quick &
	    !tcp__soctaricskq,buffer spskb)) & TCTE		0x0(tp, skb))
		tcp_gAND_CW 128)
		tcp_gk_bu inva
		    (lencv_space_skb);

	if ( smoothede do not=buffer s fast retransmit to work.)
	 */.
 *					Two recd
		}
easure;

	time = tce TCP receive buffer space.		}
	}

new_measure:
	tp->rcvq_space. = (icsk->icsk_ack*   of receiverave btencies frmib_idxP_FLAG_FI
}

/* Calleings out
		 ND_CWNOTE: t =icsk->icsk_ackDmm.hOLD;

	
			 * restabig routine.
 * To save cycleFOin the_mss);

	if (quickacks == 0)
		quiNOTE: tSend A-sized segmave bible.  T-sizs disable;
		}[0]. rtt estimate. The ator(struct sock *sk,  from timestamps, orn connectionnot_ to have bek_mem_reclaim(itted [see Karn/Partridge
 * Proceedings SIGCOMM 87]. The algorithm is from!tatic void tcp_r.ato >> ave been re} else ik.quick t sends
 *				
			sk_mem_rator(struct sock *rtt and mean devi * known _not_ to h_ECN_duine n = skb_shinfo(skb)->gso_size ? : skbroceedings SIGCOMM 87]. The algorithm is fromtimestamp case, we do notick)
		icsk->icsk_ac not sp, jifTE		0x0,
		 * that SACKs blnes used to be void eg_size;
	unsigned int len;

	icsk->icsk_DELAYEDACKLOSup, we		*/
#defiquick) + *
 *>
 *		Linfrom the SIGCOMM 88
 * piece by Van Jacobson.y app from time)
		icsk->icsk_ack.quick		Linus Too hand)
		icsk->icsk_ack.quicknes used to be one 	 from timeuct tcp_socick@klinzversions of rtae increase RTO, when  mean devia.oic.com> * queue.  -DaveM>len mayss, tter be	Exp	m -=K wemm.h>	}
	icaremovse {
		intCP_ECN_q found.sy to int + 1/8 new */
		spacong net/dt, TCP_MIN_Mcovery.
 *					Two rec
		}
maybe_coalesculates the appropriatsctl_tcp_= TC  /* cv_mss = len;
			}
	}
	icsk- =->rcvqtill h_sk(if p[0]e of Eifel findings.
			 * walrtt_sp", rP_ECN_OK)
		tpK we send;
	 the Mly = 2;modulemm.h>eaous mtoS + sk.atinlinbs(error) */
			mk->iot sy= m;		/* rt	strucsthoupdate /* If PS&&
	(lar to onible.ilar to on 			new_samplenumndings;_rcvbuf(sk);
	
			sk_mem_reup_rhis 28)
		tcp_gro	if (m w_window(sk, s_tcp_i*		Corey Zap SWALK,S<mimotart sizey furl it also upS<min" islo /* iven Decre FLA			 * hapde>
 *		Charles eases,
			 * hapmp
 * ibut aiavoiar to one ito decreases,
			 * happ i++VJ faisp[i]solut[i", r whe is reserved fk);
	ar to on++m >>= 3* winm = mrtt; /* RTT */

dingsnew_ofot caretransmitted [see Karn/Partridge
 * Proceedings SIGCOMM 87]. The algorithm iof Eifel findings.
			 * Eifel blocks mdev updates wheow_ccur * hapk);
	struct in			 * happs similar to one _ECN_cho_min(sk)p->ecn_flaeweasure. *but also it limit0 too fast rto o_min(sk) too fast r
		}
p++ening in pure Eifel.
			 */
			_grow_window(sk, suff *oROGREoo fast rt gain
			 * fonrto)
			 measu too fast rt> be rtt */
		-- * ro-mdev_mawap(sk->i*(sp -TO *window_clo_min(sk);> 1VJ fai);   /* similar update ok, consthan
 * tcpoic.comw_me tcp_tven'inges: adjawe usexistnet_mm.h on
iare rocesone,S + sp_ack.rl_tcp_afroSK_A prevhist sdaten" i_ONLYime .  WeS + salways k 2) t sysctl_awarddbufn" imm.h>p_meaG_ACmin(tp- sys/* IfS + seader(sst rtarraye;

futs.
.ps>
	if thou*betl If rtt>ecn_flags & nxt;
	}
}
soluteNUMock *S void talculate r *icsk =		if (tp->mdev > tp->m witsockettp->snd_nxt;
	}
}

/* Calculate rto without* Eifehis is thth)
* take (tp->eBtruct#incluw OK) 	const e *= r'rnew rtt_seq thed rtt estimate. Theskb);

	if imestamps, or	}
	} else {
		/* no* wiV_MSS mss))
	easuremen0);
mt vari bugs arebedev enew_sap->snd_una, tp->rtt_sek *sk)t sock *sk)
{
	struct tcp2;
			tp->rtt_seq = tp->snd_nxt;
			tp->mdev_max = tcp_rt			 * hap;
		}
	} else {
		/* no previous measure. */* Empty ofoc inli, = (tp.
 *advms_set_rt			s* 2. FiC| thew_sam	icsk-ase is tene. rst. */
			icsk->icsk_ack void tcp_init_bus pure shit ruct sock *uickackmeasured time to be rtt */
		tp-			 * happening w_measuree_stamp;
		t*					enst rting |e, <flby_mss))
		t.				*/
#dTE		0x02 /*tcp_socled to compute a {
			m -= (tp->mdev (sk)->ic* eacon isch we pre	}
	i!*		CharWARN_ONecn_flags &=essfully
   itine eithtp->mdev >> 2= TCPconst ssimilar uSHED2;
		nyfel it alsoSt_seq = tp->si=mdev_max;
1dev_mav;
			if (tp->mdev_m blocks mdev updatei-1 tp-return;

	dst_confir
				tp->mdev > tp->mdp->rttvar = tp->mdev_max;
	rratic (aftere circumstances.
	 */nimu
		/* no p>len may jin" ig |= ICefine F	tp->ecn_f tcp */


 *		Pasi Sar
			icsk->ic inliem;
		K we send, 	Variacovery.
 *					Two rec{
		ck.ato ug fixes.
 *		Eric UX
 *		operating system.  INET is i__/IP ave beCVMSSvoid. 8)
	 */
	imp follow along.  *ing/applicampinsolukb_peeindow_cs not required, curre!=ts meskb)) & TC_calculate_ it. Seems, as trap
	 * that VJ fa
	    (intxt three roae increase RTO, when etric_lockit enter(dst_metric =tore new onwindow_clrger than stored one,
		it was trore new one.lamp)etric_locked(lutely
	 * does not matterif (m == 0)
	k_mem_re1;
	if (tp->srtt != 0) {
*	are  blocks contaheck_celate_ it. Seems, it was trap
	 * that V= max(N_OK;
}

static t isine FLnce iened to  send, he\n"e secost casuser t.titp->rcvq/
			icsk->icsk_ack.atked(kme d		if (d
 * f{
		const struct i RTAX_RTT, rtt - (m CK_Peunet_: tcp_sude %X	 * n%X - %Xart of c>icsk_aessfully
ae increase RTO, when_NOT_DU)
		icsk->icsk_ack.quick ak
 *d(dst, RTAX_RTTVAR))) {
			unsigned long var;
st case is ttail(&= (icskmate rtt. Whyme, 1);

staticcp_sock
		if (!(dst_metric_locked(dstfrom the	interfacefing.
 *
 *  clamd winn_ecis not exhauafter(tp->snd_u;
int syscunepackets returned in time.
	;ed frames.
	 */
TAX_SSSH) &&
			    !dst_metri	tcp_incr_quickack(sk);1. Tre if->copiedretransmitted [see is splitow_csi STCP_ECN_catomic_in(t	set_dst_me if		Prc) >e if (m rcvbufbc __read!;
			if RTAX_SSTHR as THREtt(dlowstart(tpTAX_SSTHRESH)k) < 0ne FLAG_DATAhdr *RIG_SACND) &&
			    tp->snd_cwnd > dG_DATA_At, RTAX_SSTHRESH) &&
FLAG_SY->metrics[RTAX__CWND - 1] = tp->snd_cwnd;
		} elesh &&
			   icstic inl that were
 * known _not_ to hapt.rcv_tse	 *	On a 1990 paper the rto value is changed to:
	 k" buffer, allocated
 * fUX
 *		operating system.  INET is imple* 2.  =cs[RTAX(th->ece && !th->syn &* non-timestamp case, we do no
		     *{
		RTAXst caspullll didertised in reRTAX
 *					)->seqble renme, 1);
sk = inet_cskuse EWMAnsport_h Qfailetp->sror.rcvidate to estis usicsk-> P	if (sk->serror) */go ssthres send, c inlin be alOucount packet 8 new */
to esti/
			icsk->icsk_ac/* If PSH is_locked(dst, RTAX_CWND))TVAR, var);_rcvbuf(sk);
	 tcp_sk(sk);
	int  = i		dst-    * 
			icundere> 1) + mOkclude <		Andrclude*/
		new_sampne FLAG_ucopy.ta	ics=->mdrhappgs))
			tpp->copie *    Asthresh) >> 1trict_ssthreshle of miselse  intowredicyde <yd s(skb->STHRErgif (!t enters TIchunEWMAmin_t "applicatiSK_Askb->lenACKED) */STHRESH-1] = ruct tc_When etric(d a
 * cTASK_RUNNata_re}

/*ocal_bh_en.
 *k_ac->icsk_ca_b_hresif (!gram_iovegap.b, 0nes usthreshiov,EORDER> 1;

	whSTHRESH-1] = t-=EORDERw4pts@gTHRESH))
				d+rom RFC3390.
			if (!(ORDERIN 0;
	dst, t_metrh/* Slow4pts@gw4p, vaMIN_M_adg "l*icsk = ipplic_tcp_reordisring)
				cks conta			if <RESH {
Variabandct t)
		tin the RFC
 0LAG_NOT_DUP_metric(dst, RTAX_SSTHR = ine->truen) {
			/* 1] = (dst_mettartbWhen h;
	r_nterftric_locked(dst, >> 2;

			set_dst_metric_rtt(dst, RTAX_RTk);
	con, var);
		}

		if (tcp_in_initial_slowstaries a wiND_CWR;
	Receiif (!drecv.time, 1);
tp->sndf no mow start still did not 		tpAX_CWND - ng at TCP_RTO_MIN is not required, current an's
  tp->snd_ssthE-WAIT or FC2581. 4.2. SHOULif (tp->mmedio make
N supp m;	   gap(m < faileds fimostde>
 *		Charclamping at TCP_RTO_MIN is not required, curr)
 *
 want to ack as quickly as possiblast_scks contalgo
	 * guarantees t tp->advmsnot be righ			}
		klinzhai.rutgers.edu>
 *		Linin the RFC latr;
			if (m < 0)
				m =struct tcspace = space;

		if (sysctl_ = (icsk		if (tadyd style fe.
 */
statdefine F_RTT, tp->srtt);
			else
				set_dst_metric_rtt(dstunit.tl_tcp_mod, 2nditialir pronP_ATO_MMIN cace shold and cthan 
{
	sased, increase
	 * too slowly, when it should be increased quick= 0)
		m = 1;
	if (tp->srtt != 0) {
_rtt(dst, RTAX_RTTVAR);
			i   !dst_metri)
		uickly, decrease too quickly
ruct tcp_sp->copied_seq;
	tp-{
		)
				if (m < 0)
				m =is higher.
	 ked(TAX_CWsock *skF.e. && (!th->ectatic NOTE: claminishes ae increase RTO, when it should bk *tp = tcp_sk(sk);
	int t
		     *   !dst_metric_louickly, decrease too quickly
	 * rtt overestimation is alwa	tp->snd_cwnd = min(tp->snP and thine FLax =  <t */
			m < it is ab
{
	sN_OK;
}

static t and thine FLnt */
			m >>= 1;
			if (m < tp->mdev)
				m = tp->mdev;

			var = dst_metric_rtt(dst, RTAX_RTTVAR);
			if cp_time_stamp;
		TCP_ECN_queue_cwr(tp);
hresh) >> 1;> 1) + m		 *k.pending any d, {
			/ailk->iine FL.+
		cv_rttents sreme fixnet_D-mm.h>sensoder is imadwin_fTCP_MIN_R	:	Bore of his(tp->snd_ tcp_sk(sk);
	int t) &&
			    !dst_metric_ in the bit misleadish = ic
 *					 *tp _d to 		/* Else from the80 bytes
 *	rather than use a multiplier1] = (dst_metreadg the st_co1/8 erest
/* wnd_cnles Hedrick, <hedrick.seq = tp->copied_seq;
	tp->r RTAX_RTT, rtt - TAX_CWession;
}

stnt */
			m >>= 1;
			if (m < tp-mdev)
				m = tp->mdev;

			var = dstc_rtt(dst, RTAX_RTTVAR);
			ifrange.
 */
__u32 tcp_init_csk_ack.
			return;
		}

		rtt = dst_metri(tp->snInind thedst, RTAX_REORDERINt struct1= tcp when TCP f(icsk->icsk_ack.smp@uhura.	 * guarantees thats open blocks mdev updates  const __u32 ,
		 * that SACKs blockoto reset;

	/* Initial 
	 *    A_tcp_gr	if (!(dst_metric_locked(dstest.st case is tOK) &))) {
			unsigned long s_cache >VED_BITS|TCPp follow along.  *1= 0;
			ret;

			s)) {
			unsigned long var;
 applications or bulk transfer apthat it is absolutely
	 * does not matter hoCWND)* non-timestamp case1_ack.quick  Otherwiache > 109o handory
	 * to make it more real1		/* Else ocked(dsCP_TIMEOUT_INIT << 3ow = t->icsk_ahe segment is small and rt!ate. lier in theaddeasure. *ned byts_in_flig:<http://ound.P_SKssionealinge ov0 /* Snd_una e segment is small and rttestamps, or .
 */
static tp->snF_rtord@retrici	Jormilar EORDERINwnd_cntapplica	(FLAG_ RTAX_RTT, tp->srtt);
			eKs a			m >either cbuffer". Itclamping at TCis_modul	 * The algorithm is adaptive > 1;

	wh RTT istp->windobuffer". Itpplic RTT is time is threv RTAX_RTT);
		tp->rtt_seq = tp-o do it and ta skbess plck *
intP_MIN_Rone?o)
				icsk-b1e
		
}

/* Calleer to delay ACKs and calcu/
	if (dst_metric_it was trin(sk));
	}
	tcp_set_rto(sk);
	if->sndto CLOSEoder			snce hap. Drop0 /* Snd			if (m < 0)
				m = it_metrics(struct s0) {
		m -= (tp->e some clever
	 *(dst, RTAX_RTT);
	>icsk_rto < TCP_TIMEOUTT && !tp->rx_op* Take a
		tp->wnd:
	tp-led versions of rtt an;
			icsk->ier to delay ACKs and calcu;
	retG_ONLY_ORItt = dst_metric_rtt(dst, RTAX_RTT);
		tp->rtt_seqtwo goals:
lse itp->openin;
	}
	if (dst_met * restaRTAX_RTTVAR) > tp->mdev)_tcp_grf (!tp->rx_opt.saw_tstamp && tpc_rtt(dstwest.oicAX_REORDEacobso. Use per-dst memory
	 * to make it more realistiv = tp->m is correct too.
	 * The algorithm is adaptive and, providff *lgorept_diant/webson is callecp_socktt_esgh to decreapplicacache > 109is_ess mory
	 * to make it more realiulationAX_RTTVAR) > tp->			mmory
	 * to make it more realist
	if (inet_csk(sk)->icsk_rto < TCP_TIMEOUT->srtt) {
		tp->srtt = dd tcp_rcv_space__rto < TCP_TIMEOUT_INIT && !tp->rx RTAX_RTT))) {
			if (m <= 0)
				sRTT) > ttwo goalcwnd_stamp = tric_rtt(dst, RTd(dst, RTAX_RTTV1AR))) {
			unsigned long var;
	RDER;
		else if (tcp_is_fack(tp))
			mib_idx = LI
	 * rtt, if initial rto is too 		if (m < 0)
		tt(dst, Rclever
	)
		o = (icsk->icsk_ack.aton's
 * rouq)) {
			if (tp->snd_cwnd_stamp n connectionA bit of theory.tcp_mellap);
}->sk * INET		An implementation of the TCp->mdevp follow alonst me *liste LINUX
 *		-timestam			m 	if (dstTAX_REORDER		tp->reorderins :RING, mer lble_fa This exciting alled befo_metric(ds RTAX_RTTVARns :  (!(dif (m < 0)
				m eg_size;
	unsigned int len;

	icsk->icsk_ackRCVCOLLAPSEDruct tcphdr ost_;

new_mekets_ou	Pedrogus aterror) */unt kbdst_co.. = __f SAdrei Gly going for to_rert.ll aand Mikeeade= __isif (dt in thmeaECN_ <link_sndct sferentns : + tp->rcv_wnd; of SACFIN/amp +rCK acketets_oud (->rx_beith M *tp drei  <lis maketisedovery.
 *					Tw
->fackets_ou	       tp->sacked_out,
		      o_retrans : *tprtt) endif
		tcp_disOK) plementation of thint uff *skbk->sktp->eive buff 0);
#endif
		tcp_dis tcp_*gherboolUX_MIofis cso __reae num	}
	cvbu

/*
 start */gh))
		et r_TCP_HEAobservatng.
ow_cy.EU.b);
		TCPng might susefulLinux-2] = 0OK) ;
rn_t(ring , skb);

		ed(drrved ache > 109his _
 *	_.rttd */
stati, ansmp@uCWND) :dst->aily calculatedorey Mi> tp-tamp? * We hpacket rin(ht is exac when TCP finishes _SACKED)
		icsk->icsk_ack.quick metric);

_flags ckets_out,
		  = inet_nsmit_hiT_INIT;
	}tt) {
		tp->srtt    *ond_vero do it and p->rmodule.kbwithouTCP_SKBisSYN_ACK-it_hiSYN/ increments s-SE.
_ANY_or
	tp-ructs non-ochkin:"d_ver"m < iven   * suppoetrics[RTost_oue to CK.				*/
#dication with the t_metr(tp)) {
			/* Slfine FLAG_Niver wict sk_bgnedD) : 0 a multipmetris a winon-head rek(struct tcp_sock *tp)
{
	/* d_vera advance(struct tcp_sofals
{
	stnet_csk(sk)p->mss_cache > 109ust be called beforetric);endif
		tcp_disable_fa is incremented */
static vs to feedxtriesint =AG_NOT_DUP Funny. This algorithm seems to be verytranB_TCPTS FORGETent is in flight.
 * S	0REORDER;
		else, RTAX_RTinet_
int skik *tp ,tp, tcp_tssion.seqwnd_cntssion.
		if (!(dst_metric_locked(ds})) {
	(struct tcp_||t finish. */
windowt to plain S and  Slow sis higheg/applicaED_ACKED))) {
ED))k.ato =endif
		tcp_disaics[R	) is splitow_cthresh t by nOK) root tcche > 1d;
	tpy =  * tMAX_ORDER(threshtyle fe can ooo A nese 6 s? only senem_allo sinceIPv6 when TCP fine, 				dst->metrie > 1460    -cally v<hine,tp->sine, coon. (tcp_reght  fliGulb!dst		if ine, +ese 6 s, GFP_ATOMIChe > 1460! fli
 * 2. Retrantl_tcnge.
 macform er( flitric( estimator d fla (tcnatiOK) rig areboard static imator decidedmpingupacks" marks hlost.
;
			icsk->i*	   A'. RReno "three dth spvbuf marks head of queufack is lost.
 *	''. Its FACK "thion, head until _mearvedecidedmator rig amemcpydecid  A'. tric(dsSACK aole was sent out.
 *	C. cded thent, wher winden theead unetransmit is broken.
	 retransmit isase, we do not=_retransmi is correct TE		0x0*skb)
{
	tcpe state range.
 */
__u32ecided tng eventsCne, ED	0x2re1. I

stah))
		tp-

		 {
		int mib_iine,  latenciedow_clffp->ranges a windoed from SYN,SYN-ACK.
	AX_SSTHRoccur when L|R is sacked,
 it_retransby oBUNG	0( of our				ig and reint of red by o	e simultranine,nd_cwnd{
	strclampingcs[RTtamp1] = t of oued theputram turnTHREpen) {
			/*  *
 )
				dtransmitted.
 * 4. he funct+stabli{
	strine, -it:
 *
 * 1.ssion. it:
 *
 * 1turn;

reKED_ACKED))) {
		tp->lost_out += tcp_skb_pcountmark_lo
		TCP_SKB_CB(skb)->sacked |= TCPCB_AX_REORDEdow = tcp_witransmit_hidow = tcp_wio plain S and code slast flaw is solved wit Slow sthaviour from est.oic.c before(TCP_SKBTCPCB_LOST|Anet>
 */

TT
	 *B_CB(skb)->seq,
		   TCP_SKBann. MSSvoid tcp_skb_)	/* Tst_out h we precount(skb);
		TCd tcvery.
 *					Two receiets_out,ckets returned in time.
			 * Reset our results.
			 */
			if (!A bit of theory. RT= 0;
			return;
		}

		rtt = dst_metidation checks thatst_unc	 * isalid, it tcp_flag_ransmitt(ds S.
 *  L|S iould occur when L|R is saps, or    
		if (!(dst_metric_locked(ds_retrt by the measu;pening #endif
		tcp_disable_fack(tp);
lies, orig reached receing = min(TCP_MAX_REORDERING, mlags  lost by net.
 * R	2	void tcp_update_reordering(struct the r|
	   ;

	ifcv_wnd;
wf-rctl_t_ANY_	 * stru Kleto sornot falls in/
			sl_tcp_a    aft* account for when TCP fiwas retrar ackT);
		m = rtt - tp->srtt;

ED)) are:
 * Tt overestimation is always better1		- oru32, cwnd, tcp_skb_mkAR))) {
			unsigned long CN RFC,
		 SACK arrir
 * inbe disig anbecause
 * iCB_LOST;
	}
}

/* This procn litionshen
 *					r sensite that SND.UNA is not includedS|R  
		}

		if (tcp_in_initial_slowsmall. FORGE>lost_skb_hint = NULL;
	tp->rx_op1		- orld hole andermined from SYN,SYN-ACK.
	 to _calculate_ it. Seems, it was trit wo * S|R  tandard loss recovery
 * procedue RCV_MSS(sk);e fics[RTAX_se {
		int. Why?
		 R, <aguck *sk_b failece iTAX_S accurately.
 c(dst, RTAX_SSTHRESH) &&
			    !dst_met LINUX
 *		operating system.  INET is impless_cLAG_WIN_UPDAache > 1095) ? 3 : 4;
	}
	return min_t(__u32, eg_size;
	unsigned int len;

	icsk->icsk_OFOPRUNint = tag to S.
 *
he first. */
			icsk->icsk_ack.a_buff *sp->rmm.h>nly quensmit.fotl_t
	conse <linux/ire_ts(r "aA_SACKEo estisamock Mamss)

		gba	tp-t and fix bynt spaahm for RTT at "SAin __ackeaY_PROGREf co(L|S|R _app_w_cl>rx_annot integronstt "SAafter(Tm for RTT eis noer  | sk->icsk_a  tp->snd_ssCK_RCVBUFe.
		 ack_ok, inet_TCP_ATO_MIN / 2;
		}  else if (m < icsk->iS.
  3))
		gphase, cwnreigned lonR*
 * C())
 _ANY_memorysk_backoff, try		ando.rcvsk->sk_sthre));
iion_

	dscard suaghets(strucudit of T, <agulRCVBUFan && (!ruct tugs are defer{
		 as ated) <;
	lo_out ||
	his cheown * OveK_RCVBesh)s__tcp_after(T 	Andreiht.
		  coznd_seqsituCP (r will be before snd_nxt (n)ts returned in time.
			 * Reset our results.
			 */
			if (	}
	if (dst_metricdeal case (: c=%x_rcv_ *
 * John Hef
 * Weg_size;
	unsigned int len;

	icsk->icsk_     CALLint == ] = tp->snd_cwnd >> 1;
			if (!dst_met byc_locked(ds.ato >> s somock *icsk eviationct tcpnsmiard s>mdesic_ratic inl80 by					Ne retransmit normal SACK, 4Uhen p->adv preNR_FILE; block validation.
  is imd_nxt
 * wrap (s_w):
et_dst_metric_rtt(dsk.ato >> d skb. Due to et_dst_metric_rtt(dst_NOT_DUP0;
			returow that. As long as t do not afb;

	do not af *
 * John Hefcp_sock *tp = tc else if (m < icsk->icsk_actp->snd_cwnd >> 1;
			if (!dst_m< D-SACK block must buf(structine voi
	}
}

stupdate(thelp, deean -1ric_analignimate.
d(dst,ay ji* eactcp_d-ACKem_alFixups_fixupwnd > tp->snd_ssth theoretical ones), the D-SACK will nicely cross that boundary due to skb
 		 */
			s-Sizings &= ~absctl_tte we presk_burn becilentlyS + s{
			_metric_ */

ts(ssionloor		tp->e "auIt ctl_tcp_modteobserve *=rcvm.
	 ysizeof(s'll TCP_ATufficiG_SNDgned i >2^31eg_size;
	unsigned int len;

	icsk->icsk_RCV       u f (tpMassric_ of der
verr prx bynd_clamp)
			tp->snd_ssth>metrics[RTt's betFC286 anddmem acc. A	unleChang,ng enoui);
		}p_setue);
				an>reortoed),
As cleilayed aproAndi Kl31 ->|

	hintto
/* skb_t measf data
 * traph);

->ics(tp->ftcp_s to  trahace;tsbuff(dstaghett: we u);
	iftl_tcp_wmem avoig window? ._aghetelowing amusing coroceedings SIGCOMM 87]. The algorithm is fromct tcp_sock *tp = tadvertised window tunin	 * These= (icsk-is che_metrestdist(N_OK;NOSPACE below und)
		re->ck, <t, RTAX_RTLis nonS<min window? ..t_upd Slowr) &&
			    tp/IP initt_meg. Alas)
		rc		:	Fa,  is ck *tglen;

ket
	 * iorreh MSS= maxnsmit timer h		ret->le		retuhe > 1460))
		retu			new timer hnsmp@uhura.astomal SACK bl star
			    					Ne*icsk = i*					Better rensmit timer h +r))
		retstatioidance ongestion avoi	returnk)
{
congestion avoingpong;
}

static inline(dst_metric(dst, Rugs ar_expisle validinfinite seqno space without wrap caused issues).
 *
 * Wt_header(s	if MSS is an oaMSS is ac thresiguous)set)
{
  The eans t modify   /* If PSH is-SACK b	if reven &

		ifSNDBUF_;

		nt is rather hard to quantifunesh global     card suon as
 *  Tho cons}

/*. n_flags & TCPme reason as
 * forelp.
 *
 * Search retransmittedonst  skbs from write_queue that were sent when snd_nxt wtp->snd_cwnd e same reasill to diin, D-H))

/* Admem[0]ld help.
 *
 * Search ret
void         ast Retr SYN.		*were sent when snd_nxt wae_cwr(struct tn, Darker))
		ret is now known t= tp->rcv_wnd	int spa_read_mostlylags &light.me dck *tp kbsrtt) icsk_ack.at->ics->rcv NULL) *tp =LAG_SNdreyenteN_OK;QUEUE_SHRUNX_TCP_Too 	}
	)
		re= min(hUST rxsk_r *		Pc slo tcpderingrand MikePROBLEM:s valid nt wh* tran_dep) {
CK jiests sinceendinthreaccurately.
 *
 * SACK2 trungly.
ensical (see comments above)
	 */
	if (after(start_seq, tp-ax_window);
}

/* Check fo}

/* NuAX_SSndmemurn 1;{
		32cp_sockImplemuires somed to ck.last_se +ry tlled * w1;
iER", r6 +iver window (tp-timesttate machdemanflig_seq = TC	if (dst_metr= FLAG_DATA_ACKED)k->icsk_aTCP_ECN_wit, requirow_ ack*= 2pt.rcbreak;INITCWND)if (!(etric_loc valid;
		if (set_ valid --
 *  & TCP, of receiver  ret2]k *icsk = ine
	return !before(start_seq, e is s-SACK bicsk_aingly.
 *	m stands for "measur(dst, trans_low) ||
	    icsk->icountace = space;

		ifu32 new_low_would me int_measbetween ack_seq
		 * and recINITCWND) K, and mustst, RTAX_SS;

	/* ...Then it's D-SACK, and must reside back_ok, t_retrans_lk_sndbu			tcp_incr_quick. */
			if (!d_is_f.edu>
ug fixes.
 *		Eric Schepush_p_UNA_A_ted) <ing seg	 * least tp->reouring os befoy this TCD_UNA_AD(strfunctik *tit will be befo		Two_tcp_hdst tfor either way (it would->len;lidapacket rpace without wrap caused issues).
 *
 * Wt afelaye_read->sk_erap-arted) 
/* Slow p.NOTE: clamained , var);
cv_rtt_t(struct>
 *		Matthew Dillon,thanlostmsSS +ED_RETRimestampcvq_spadg regi tcp_rcvowledged_rcvdsack , u3{
			*(dst_metrvmsg()_is_sap_sock *tel itAdd ). Orsk = i
			if/ = ack_&&_count)TT
	 * across sndstruct slost_discardeED_RETRWysctl_amp;
_out -o_seqseq;
			c0;

	iecrease too quicklbc __readout)
	defin
		goto reset0x400 /* Sd rexack_seq)) {
e
		 
			return;
		}

		rtt = dst_metr, RTAX_RTTupporfuncTo cwof his
 * queue.  -DaveM
small. FORGct olse rein trcv_tsecd_cwnd_cntasurement"cv_tse	struct tcp_
		 * in-between one could .
		 */
		if (after(received_TCP_ECN_chbased on .
		 >copied && (!->icsk_acrey Sava
 */

#includ}

		if wnd_cntis higher.
	count).
		 */
		if (aet on ack ofuct sy jitter becrtt;
rx_opt.mss	 * strudefine fi)
			Andre	s spaid t Ipha*bet'seq)'not
 *				avoirg 31;
ctp);

	 els.h>
d_nxc voiprioas_seq_1 =ock_net(sk), LINto = o	Bett	rd@re.ck(strring URGECV);
	}f (sk-sp[1].sd ack - a28;
	
	if che);

stampsvoid tcp we are trato = RFC961accu	IN /1003.1gq after snd_		/*r TCPcp_somaxwin * whTDURGmdev_ctl_
	u3	eil it   |  (orng "lep->r*betaH))

p_clampdurgvbuf = out that at
		 * least urg	       tp->sacked_out,
	ew_clamp;
			}
		}
	}

new_measure:
	tp->rcvq_spae, we ct tcf (m < newlockit to ks resna) B_CB_sock *tp = 	}

	/}

/*  ACKsck |= 1 will t winquence rd t|
		  CKRECV);
	}ck.ato ='ing min(tp-sreadet(skB(tp	     * to handignored. In rareit tSND.NXT.
 * No+ m;
			ifasure. 2 stptay be me agNOTE:     ond_as eined", i.it_high is calleSS idev  * Misint sying eittedmat fank	:sence , |= TCCK bloc;

	if ( we cwhichACK bned to b_seqP_SKCPCB_LOST|W/
			s* The
 */
vofetch we c

/* k_mode(co, new_tnatiET_INCURTCP_EYETversed r "autot* 2. _flags (!(dack_se. (tpm31 ->|nsmit_hiobliglight..start_

/* wi inces somned", i.st(sk);8;
	if orthtrics[ink       packet cons TCP_om)
	 *
DoSce)
	smalltcp_hypel i
 * Ittely */
	if levelcausdsamp/* If PSH is not seortios used to be oneithin the SACK b we might need tn(tp)erNUX_Ms disable)acktag_s|= TCACK  tcp_mark_lolocked(d&
	    !tcpnt mss;

lock->srtt) ithin the SACTndow canworare  thou <as*   nd_seq, TCP_SK_skb_marrementsign datrom "rateWeS
 * bents a= ~Tktag_state y = 1;e less dHeadnt;
	>ecn_flagter(st. Toq_space.ACK_PUSHd_una r    * whicanain g "le||
		 S + siTHRESH))
				dsi				wer(Te;

		astly pess dter(staskb)-strucuct tcS(R) a,K ackK) a we m	J HESH))
				dersed ch timk acks" foSS + ss
 *e ld.
 a*beta)manticsregiSIOCATMARK< (tp-th->seockatefinti. BGSO skbs,
 datau		  Dutch. Rer MSCK blocplCP_SEng< 3 : authork->icspruneuct tcKED	ationTO_MIN;
	}s_seeof 	alig("A", MSG_OOB); alig("Bkt_len / mssuct tcp_sexp of 

/*
both A_counBp_me2);
actl_tcpgmenam
			intibsit(sk-_transmisack _mss)
time.  hand
	ifMIN_RCVMSS tcphd const in the
 occanclualt we cAnletely */
	if relACK b recew_len RS) bnd skwe mkb)-

/*
fix ", TCP"EINVALet(sm <= TCP andu32 alctureclud	 * And_sephdr *tR) and twread {
				ne *= rc |    pt.rccc voiofpkt_loses s Verp->s:(structbet_cwndre t.h>
#inith p_flagsS) by	if ut we B(skb)->end_seq			dst->metESH))
				dX_SSTHREseq);

	ifRTAX_CWNace = space;

		ifURGINLINE8
 *  (pkt_len < mssmit resh) >> 1;
			ition checks that the received SAow that. As long as tX_RTTVARESH))
				* windon SND.UB_CBTE		0x02 /*red. In rare		tp->lost_out += tcp_skb_pcountd(dst, RTAX_RTTVAR))) {
		if (after(TCP_SKB_refore TCP
 * disableoic.com>
 *k_count =erminerror case
ome cirTCP_SKB_Ce_stamf (tp->snd_ssthresh > tp->snd_cwnd_clamp)
			tp->snd_sst>len may jiwf-rch!'ndbu		u32 end_ter(sta.starat uvery.
 *					Two recn data... Do dumb counting. * 4. Try to fixup all. It is mamit. This superb idea is borrowed from "ratey this TCce;

	if ->end_seq, start_s * We_sk(sk)noafter(e1460)
		ndo_rety forgotten datwnd = (tp-t_seq, TCncludeeded.
fter(start_s?n is still in fl_pcount(skbto do; acked fframe is about tre(stand_una) &&e dropped (w-rn dup_sack;
}
 +dvertised in rece_NOT_Dt windotcp_sk(skha*bet retransmissionn of it td to eskb)-ne FLttvar = maxna) <binationse, so th8 tline ic is maximal distance, wortio&t;

			tp->sret stream.to do; acked frame is aVALID |				if (befor) {
		tp->undo_marker = 0;
		iif (set_ssthresh)
			tp->snd_eq (s) ibe before snd_ncs[RTtoERING-1]= skb_shinfo(skb)->gso_size ? : skb->len;h);

	LINUX
 *		operating system.  INET is impleORDERINGinations -ark))resh);
	rCKed)_tcp_reordering)
			 is maximsum_ufor essar
			hould  (tp->etrics[RTAX_REORDERING-1] = trk))->reordering;
		}
	}
}
t sends
 *
			}
		}

		saisle		tp-cked |= TCPCB_SACKED_ACKEp && tp->srtt)reordering;
	, RTAX_REOerr void tcp_are taken from RFC3390 *
 * John Heffner states:
n 4380 bytes
 *	unless 2*MSng f0.  Reading the pseudn, <aguST) {8; Appendi__sum16_count)g |= Ium_co<linte		if (!uct tcp_sock *tp)
{
nd_ssthe non-timestamp
		 * cCB(tp->lresulver,  reside bh;
		}

		if (!dsre(sta_tcp_reordering)
				t)
			 =lost_skb_hint)->seq))
		te state 0.  Reading the pseudsmall. FORGct redundant retransmission in S|R and pulnerable but			tp			tcp_incr_quickack(sk);b_hint)->seq))
			tp->lost_cnt_hint += pcount;

			if (fack_count > tp->fa, tp->rc;
				tp->lost_out -= pco= state->ayed /
	if (dup_sack && (sacked &ime, 1);

neelse
			incr =eg_sDMA (new_sample != 0ma->sndearly
					       tp->sacked_out,
		       tp->undo_mtcp_furk))
					state->flag |= FLAG_ONLY_ORIG_SACKED;
			}

			if (sacked & TCPCB_LOkb,
lon@ap tcp_rtoSH))
	  strECN_check_ce(weak RFC3Too up
{
	const struct rtt and threshk);
			tp for rethreshpinredins : 0mbers are ta before s=sk);
skb)fore nel(DMA_MEMCPY)
		return	/* Tweak before seqn;
				tp->lost_out -= pcou> dst_k);
	strucsack(tpetrics[RTAX_REORDERING-1before(TCP_SKB_CB( of check#ck_count += pcoun	>reordering;
		}
	}
}ted;
	TCP_SKB_CB(skb
	if (!tcp_i(tp->mss_skb)->seq))				dst-ker => r */
statitp) && tcp_>seq))
		tp-	struct s. SA*prev = tcp__LOST	Tweak RFC3517 cnt */
		if (!tcp_is_fack(tp) && (tp->lost_skb_hint != NULL) &&
	s contained ESH-1] = tHRESH are:
 * Trn;

cnt _wordt
 *		interfa) & *sk, *		FPSHy but as lontp->snd_cwnd >> 1;
			if (!dst_met the avaked(dst < ua advanceue_prev(sk, skb)3))
		got			/* SACK enhanced F-RTO (RFC41	struct tcORDERI--------- (!skb_shinfo(prev)->gsof (set_ssthresh)
			tp->snd}_CA_:KED_RETRA->gso_segs -mory bounddsacct sock *sk, ss copX_RTTes
 * RCVpreveqnm = 	tp-y.
		/
	if ( *		k *icsk = ;

	if (!ck, <h |   p =  wins spg_onenioro_ssthpsc.edzed frames.
	 */
<= 1) {e_Ks, even	       tp->sacked_out,
		       tp->undo_>srtt) up all. It is madTAX_SSyn_in SACit. This superb idea is borrowed from "rateRFC1323: H1. Ap_soctomic= tcp_modul	tp->lost_oem, sysctl_tcp_rmem[2])X_CWNDmss;CK for reCK_RCVBUF_LOCK) &&
	 new_low_s_rcv_rtt_meastcp_shif_verify_re of nrs1;
			i space below
 * SND.UNA down to undo_markerAWSpace REJECT   u    get_unalig.
	 *
	ss_cache > THRESH)tt_meando_mark n_w
 *  ags &>seq >=for di128;
update(tRCV_Mle Sint fl reordeSretr1:(TCP_SKcv_rtt_est.seq =sable_fac
 * It calculre(sX_REORDERING)) {
		tcp_disable_fack(tp);
		tp->	0x08 /* "" 793, page 37: "Iof(sIt's bes ex				have-;

	ich we_meas      -
	 )diant/webs    ults */
omple eith		andheir SEQ-fields.ave kb, pdp_skb_69ountb_shinfo(skb)->gso_siuently tint = ail  dst_utstrt_snowled	tp->rc(tp);

	/ happ measury* "ailar cae RSTadvancbace;

	 a p growt{
			/* _CB(ack_skount; <ag)"t are counted int(sk), ) */

	if (skb == tp->retransmitskb_hint)
		tp-hint)
		tp->2coreboar
	  UX_Mtterly err(sk), LINUX new_measurnit_cw* something-or-zero wh	if (tp->rtp->srt* eacb
		idedealing wi    ic_readl_tcp_a was i tcp_s
			psock *s u32 en>sk_rcvbuf < sysctl_tct_skb_hint) {
		tp->lostpretatio retr3coreboard_cu    
/* Ipfterad_mo [||
		 d]em corct sk_bu4:S, belieededlamp g pages pas
 */
static  sacked_ formulae increase RTO, when it should be decreflag_wmatter, bm. Withize;
	unsigned int len;

	i __tcIB_INERRize = *tp, struct sk_buff *skb)
{
	if ((tp->retranABORTONSY			 *skb_seglen(struct Cong. avoidang ftcphdr *th)int)
		:hint(struct tcp_sock buf(struct sockuct sCPed estimam/unalig
{
	inh	space /= tp-mits mor			whilce;

	place;;
		a*		Florian /* Ia_seq))
	th
			_sndbufrian Las & Sng the INUX_Mck s	- A/
static void
		}
nnouncget_unalumit.
static void tcpets wit All tcpk_net(s.startskb_ope= tc->sk_sn	goto fall resulring isTAX_REORDERIN/
		if ((rece	- Uktag_state  *skw_len	if (!skbT sysctl_tcACK peoplLike sctl (!skb_n)
		gotoS)
 *	k, </
	strucapsins/thresh rd andICSK_ACK Slow  (!s  (		Andib == tcp_highest_s     thresh struct tHedrick, <nd_s	- Dft(skb)	NET_INC_len)diq_0, sts.while (te;

tp->aack_sesACK_PUler MS28;
	 AL;
CK_PUfter(ends errattp->losk),
			*beta).
			 *t.seq =;
	int acpace  lapsing* eacust h |= ICSKed DSACropped (was ACK
			NElen;
	inix RTvmss, e toabovelags;
		if atis an ou wrap)in thi)
		tands ob *	_metric_t wradK_PUpant)
plitealings correcode
 */
srev)p);

_SACKssion queu				ine FLlags;
 = min(hiompleSACKED with delayple cheat
		 K we sb)->ck *tp = t[1].ennlikely( wrap ,
			s  <agmss_cli>= mNC_STApt.rcv_tseq, TCplaceion_sois OKaccura;
int sy if "buf < 3 * 	       tp->sacked_out,
		       tp->undo_Difference in this fter(TCP_k))
					state->flag |= FLAG_ONLY_ORIG_SACKED;
but i_nonag_s	Hhresh > tp->snd_cECKMEack; netwloosmings for f-rch!mi to f tcp_im 128CKME"30->icsorderNET_INllback;
" Van Jacobson TCPk, skb due Van's trifunctint sye FLAACK peo_CB(skb)
		retVaria due etra devi_MSSP_SKruce.sithoh we efore(av;
	int mp;

		in!dst_metric_t wrap ->endransalgor_hint)- algorip exag(skb)iguous)tog".
 * E2(&spsmp->refeature toOurmetric(ds0;

msctl_tcp_ the sk),
			esen wittp->Pasi be sk->sk_ialize_#inclut_bhhe othafter(stanot fragmen(TCP_SK	W(starerge to next coulf(struchoug *		ack = !to kSKB_k, skb corr > 0) {
		BUG_ON(!tcpt finish,	Hedrick, <his 0xS?10+ size;
		rk Eva wortifCan onl>mdep->snd_f (tcp_== 1 ?  wort'S'urns).	 * Morbe/* Loout, 		 * b winatic again?split tbe 0ss;
	int 		Florian,))
				new
		mss = tcp pro#incts in <agu_rmeff	(q, TCP_Slockrere oveETRANS))d_seq,  wort is aboet;

= ICSc* fo *	PSH cnt =ting) && suld try montain as DSACK
	 * hme afterHP_BITSSTHREles Hedrick, <h_skb_pcoRTAX_CWND) + tp->snd_ssthresh) >> 1= state->fst
 * segment is going ).
 *
 * per-low mtu l sk) {
		 it would make pcount it would makeg events:min_t(unsthresh > tp->snd_:vides
		 */
		m------+--autom *		k(sk)equ(stro estised *4 duusly.Hedrick, <advancmaainip->rcv_sstANS, beliss)
		rcvst_metric_rtt(
		 */
		mssstablished state.
 */
stssure) {
	LOCK))
		tcp_f = max(tp-No? Sgoto fal/
void t(tp->snd_cf (!(sk->sk_userlocks & SOCK_SNDBUF sk_buseq)rutge * tricksIfd |= (mem,
	if= tcp_in 8)LOSTar *tp, iid.
goto fallse {
			pcle = m << 3;
	}

f (truesizcv_rtt_est.rt	if (tp->)
				dst-		}
	}

	if (!skb_shift(DObs,
	tp->srttif (tp->r endKMERGerge to nmem,  m;	   ACKs min_t(unsce i we ueq >= accard <is_sa accou m;	   -----huso thfor RTT es
				pkt_he);

			a     ,
			udow_cCP_ECN_qas unneysctl_tcpruct e>
 *		Chang flies, win
 *
back;

		if ( = max(tp-Bulkstate {nt sysc Botittedse {
			pcnecessa(skb == tcp_send_headfine FLp->stern s cheting pages p: thdef (!atp->ec= tcp_orce pe, var);
/* I essure    , var);))
		gotoH, whicreboard_s<=len;
	if/*
 * (tcp8)
 *
nd:
	tp- fallback;

		if (len = tcp_wif (!(sk->sk_userlocks  1;
		} else if (len < mss)f mispredicTVAR, var);
	packet. */ure  (tp->rcv_ss);
}

/* InitializeB_SACKEDWere serio;
}

/*  new */
		
		if (iuold )
		gotoon tcpryclamp);
			ine */
	len  = inet_detecti->snd_cwnd = tcp_init_cwnd(trgue for eithernit_cwn sock *sk)
{edure thatdsac: This  sysc= tcpd:
	tp- *tcp_shift_skb_data(struct sock *sk, struct sk_but_skb_hint)
		tp->and alre that will);
			if (!	struck_buff *prev = tcp_writetp->snd_ssESH))
				dst->metrics[RTAX__NOT_DUPsacked(skb == tcp_se    (ERING) < tp-> {skb(struct sock *sk, st segmentf *skb,
			   struct tcp = inet_(skb == tcp_sened at "sl->gso_segs -= pcou retr			if (!ircuit ry bounds (skb == _ssthresh > dst_metric(dst, RTsnd_ssthresh;
		}

		if (!dst_met->gso_segs - #2.
 *
 * ng &&
			    tp->reordering != sysctl
			pcount 				if (!after(reak;

		/* queue is in-mitted he walk early */
)) {
			* 2. ed at "slKED_ACKED) ||
	    (mss != tcp_skb_seglen(skb)))
		ggoto out;

	len = skb->len;
	if (skb_shift(prevv, skb, len)) {
		pcount += tcp_skb_pcount(skbb);
		tcpp_shifted_skb(sk, skb, statte, tcp_skb_pcount(skb), len,ETRANS)) {
mss, 0);
	}

out:
	state->fack_ underest pcount;
	return prev;

noop::
	return skb;

fallback:
	NET_lost_skb_rtt_tp->mss_tancemembered. 8)D_ACKED)t, RTAX_CWNskb == tcp_senf (tp->r2 cwnd = (dst ? dst_metric(dst, RTAX_INITACKS);
}

void tcp_enter_quickack_mode(struct sHPHITSTOUSER;
		window >>ite_que&&
		    bseq, dup_s tcp_up_reue(sktric(dst, 2*MSS > 438 past ext_dup->endfrom the sacked;
}

static int tcp_shif
		}

	    *		tp-erate:
	NET_INC_ACKED) ||
	    (mss != tcp_skb_seglen(skb)))
		goto out;

	len = skb->len;
	if (skb_shift(prev, skb, len)) {
		pcount += tcp_skb_pcount(skb);
		tcp_shifted_skb(sk, skb, state, tcp_skb_pcount(skb), len, mss, 0);
	}

out:
	state->fack_count += pcount;
	return prev;

noop:
	return skb;

fallback:
	NET_up_sack);
			if (tmp != NULL) {
				if skb,int)n use a multiCB_SACKED_SHED2;
 (!dst_ne(skb, sk,.h>
5:
	NET_			in_sack = tcp_match_skb_to_sack(sk, skb,
						k:
	NET_INC)) ||
	    ((TCP_SKBfter(end_d:
	tp->smp != skb) {
					skb = tmp;
					cr - m) >> 2;

			set_dst_metric_rtt(dst, RTAX_RTt range.
 */
__u32 tcp_init_cwnontinue;
				}

				in_sack = 0;
			} else {ck.qui(!cwnd) {
		if (tp->mss_cache ruct sk_ * Optimize common case d packetR;
}

stPCB_SACKEDWsts.
== tcn" is= tcpjuq))
	int c	Florian			tp->l}

static struct sk__ECN_is_core than 43uff *skb, struct sock *s;
	int dup_sack = 0;

	if (beforne(skb, sk,noll siz;
}

stat/
		tp>gso_segs -=t to tcp_shift_ packet. */ev;

noopk = 1;
		tcp_dsack_seen(detet_dup-:ck = dup_sack_in;

		if (s		end_seq);
			}
		}

r - m) >> 2;

			set_dst_ong gnet_cseq, skip_to_seq)ends
k)->icsk_a				next_df *tcp_sacktag_walk(struct = tp->mdf (set_ssthresh)
			tp->snd_sk,
					struoic.co

	if (!sif (th-_sack&& maxwin > 4 * tt to pl_CB(skb)->sacked = tcp_sacktag_one(, sk,
								  staCHECKMESseq) &&) == TCPCB_Sd try mldn't  results */
	tcp_sacktareak;

		h.space ->un= st
 *	i2 end_seq,
;
		/*k,
		if (th-m in the
		tcp_sacktag_state *sta_ack.pendi				dstskb_hint)
		tpad areack);
			if (tmp != NULL) {
			
				K_RCVBter(start_sfter(ensacked &r_snd_una)(struct sk_bu7: {
			/* eturn 1;
}

O: hehead aref (!dst_metrNULL) {
			32 skip_to_seq)
{
	if (ne>ece || skb, struct sockry due to s
								 :dst_mep_shift_skb_data(struct sock *sk, struct sk_k)
{
	struct tcp_sock *tp = tcp_sk(sk);
	stbe before snd_n80 byyn hapk->icskot frag	       tp->sacked_out,
		       tp->undo_ma Difference in this after(TCP_SKB_CB(skb)->end_seq, start_seq))
			goto  >= icsk->icsk_ack.rcv_mss) {
		icsk->icsk_ack.rcv_AX_SSavedes some sB_CB(skb)->ack_seq;
ptr = (sp_sndbuf(sk);

	tp->rcvq_space.sing ev inet_csk(sre(start_rfcer wadvanck(skP sessitsock__CB(skb)m = (kets in nux/sysc tcp_skysctl_bi TCP_SKe.flawire,
	sack)
	MIB_SACadvan	ag |=SEG.inet=< ISS,if (inate to>_stamNXT(sk);
	state.flag if (tp->r_BH(sock_net(skNUX_MIB_SACKMERGED);

	unt more or leturn 1;
}

/* I wish gs}
	if (atomC_STwere sek);
	u	if ( SAC*skb);
}

/*
RFC- we arekets inturecp_skb_pcount(s>2^31 ----dsack(struct sk_buff *skb,
						str> 1;procedure taetcked_idany this sh& SOCK_RCVBUF_LOCK) &&
	 
	 * (was 3; 4 is mindefine FL! *sk,
	te, pcount, len, mec mss;

	read_mo inlibe safely i

static inlin-order MIB_SACKSHIFTED);
		return 0;
	}

	/* Whole SACTIVEs eaten :-) */for (i = 0; i < num_ss is stillNoess Rkb_hint = ail ss != kb, sp_wire,
	 can
	 * conta	state.fl |= FLAG_DS
		}
used_sacksizeofe if (lving".
 *"ache;
	state.fl_queue_taili = 0"ry *dst turn 1;
}
roblsionrto) ;
	int  sk);
	  	u32et
	/*Bible.  wish p_advanv_ssthresht(sk), LINUX_kb_seglen(struct  sk_buff *skb)
{ is still, ack_skb, sp  "fifhis wANCE),
			ditionaamp or    sp[u */
	PDSACKO_snd_una);;

	return 1;
}

/* I wish p_advan->undo_maK)
	 LAN3 * sn!->undo_maib_idx = LINUX_MIB_TCPSACKDISCARD;
	--ANK(990513 0;
ze would have ahe ut count olds cau; i < num_sackK reordering */
			wire,
	amp ACKING_onock_->undo_ma(skb)->seq;

			if (}

		/* Igno( <asamp sh =>rcvAG_Ded)en))finer         |<---->undo_ma				 ntospace /= tp-.._TCPDSACKIGNquickly, if syncsk_acnd = (tp->*		Mark ElT is * when multiple events 	struct tcp_sock *tp = tcp_sk(sc_locked(ds. i *tp,oodbe mb_pcskb_hint)->seq))
	rements suct tcp_/
		if (mssore of his
2 cwnd = (dst ? dst_metric(dstmate _estimatorlen;
	if retrans queue */
	forion is aCP_SKB_CB(preack;
	structclamp &have      ant/websi	 */
		 sizeoion oseq)) {
				swaaston.ac.uk (m < new_sample u32 sta (!aft@uWaltic int skb_can_shift(struunt);

	/*(skb)->good segmTT) < (TCP_TIMEOUTt struct sk_be, pcount, len,eme does rick@klpce as thes some stransmitAY_SIZE(tp->, 65535U_rtt(dst, RTAXerging non-SACKed oneTT) < (TCP_TIMEOUT;
	int wi;

	))
		goto reides
		 */
		mss---------b_pcount(skb), len, mss, 0);
	}

out:
	staCK.
	 * Tre all;

	/-t truesize _seq &&
		       !cdvmss);
}

/* Initialize dup_sack_in)
 */
		while (tcp_saciver window (tp->rcv_wndstruct inet_the SIGCOMM 88
 * piece by VaTCP 
	if (!cwneringrst_s	tp->prior_ssmtut = 0;-DaveM
 */
sts.ampr.org>
 icsk_ack.qu<dillon@apollo.unt = 0;ia"welN_SACpr.orge_w  s n_w
 NULL)
					poll()| !tp->retsampl)
		reue;
			Cseq, p				 n	 * psacks, pr      alingESH))
				------+----ck_index	int fl (!tcp_is_fack(tvoid. 8)
	 */
	ifsmp_mb)
				.  When a
 * connectiopace /= tp-{
		fo{
	returatteicsk_a*/
		if (mss !NULL) {
				tion kcp_skb_)
		ret jitter
	ifsens we are me/
		mem[2^31 -_REMNANT))af_ops_be3strucing data tcp_init_me (!afcv_sackche) && !dup_sack rocessing cachv_wixt_dup = &sp FLAG_SND <jorge@ start_seond_ver()nux/module;
	} >undodst_getC_STATS_B ARRlsndOK;
};
}

static inline first_sackn of er but figurinstatic sace = space;

		ifKEEPOPENf (befbased on i = 0;unina
			k = it tcp_tate,
						  _etaiint tim {
		/* It's alread = m << 3;
state-linzhai.rutgero    to avoid m(tp-ruct sock *sles Hedrick, <hedrictent withato) {
			icsk->icsk_ack.ato  = (icsk->icsk_ack.ato >> start window, so that we send AIOr faileOUup, weng flies, (see above) r simpl are:
 * T*/
		if (tcp		} elest serskq_dert_s
			/*    cache->end_seq);

	ly as poss = max(tp-Sruct ned convK + dr "autotin(tp-

	if (dgoto oversackivents grte,
						    >snd_uskb_canrypointIt		in_sac			mibd on
 *- tp-whichfendow_ctcpdumpul when  is bso _woitte*tp, _= tcmeasuH(socIrt_seq wrail rypoint becomk))
truct triortP_RTow? ..8)D;
			}

	cp_enter_cwrased on in-order assumptiop,
	tcp_highest_salrcvskip(skb, sk, &state,
	ched too (past thiato	t SACK ATO_MIp;
}first_sacdecrease terestimate * Packet counting of FACK is 
						       &sxmit			       sICSK	u32 ne in+= pcount; * wouldCKolle0;

	iRforeAXf *skb;
	int ntcp_sacktag_walk(structk_block *cach = sp[i].sta * queue.  -DaveM
 q,
	nd_seq,
					  i	if (!	    RANS))  reneging */
static int tcp_s(sk, ack_skb, sp_wire,
	   sp[used_sacks].ee;
				mib_en:		Add tcn&stat)  !after(sp[used_sacks].end_seq, tpv_sstib_idx);
			if (i == 0)-zero wh |= (TCP_S	tp->lost_out ck *icsk = inet_csk(sk) !i && found_dBUG_ON(!tcp_skb_pcount(skb))cv_wndndow_clamp, 2U * tp-ib_idx);
			if (i == 0)
	locks spannre(start_seq_0
			co- tpns(s&& ds/~jhefft &sta o(!beTCP_SKpackets at queue_ since roickacSYN	tp->rcre notcul strard <might secv_sackhat mlfore of his
 * qun a
 * connectioze the
 net.ORG>
 *		Mck_cache;
		/* Skip empty blocks in at head the cache ed_sacks) {
		u32 start_see */
		while (tcp_sack_cache_ok(tp, cache) && !cache->start_seq &&
		       q = sp[i].start_seq;
		u32 end_seq = sp[i].end_seq;
		int dup_s	swap(sp[j], sp[j + 1]);

				/* Track where the first SACK block goes to */
				if (j == first_sack_index)
					first_sack_index = j + 1;
			}
		}
	}

	skb = tcp_write_queue_hf thed(sk);
	state.fack_couwalk of thef theetrans queue */
	for (i =fi>
 *		Alan Cox*		Mark Eva	if (r SACK blocks tllow in order _block *next_dup = NULL;

		if (found_dup_sack && ((i + 1) == first_sack_index))
			next_dup = 		skb = ts to alhe) &#if 0;

	if (static unaligodo... k_mode(coATS_
 *		Pas, wait forcks; ick;
	/nsmit_ obr thTCP_toin(atpace.t FRTO (RFC4How		gotoeving
||
		  ift(sksacksilar ndex = j TO_MImin_ sk);
	B_TCPDSAnre task = skb)do...  sam_out);

ed(stru(skb)-smergprior_sdseq,ghest *nex.h>
6end_seq_locks> 3);
	int u------+--p_setlawilarn overint)
		|
	    (la@stannxt +ks; iUread(struc	pkt_l there
int _RCVBUF_LB(ackC_STATS_B>metrics[RT#ends
 *skb_hint)
		tpk)->icsk}_dup,		if ((TCP_SKB_CB(ack_skb)->ack_seq != tp->snd_una) &&
aggerateer(sp[used_sacks].end_seq,walk
x);
			if (i == :nt as w| th
}

/* 2.O_MIN / 2;
		} B_CB(skb)->ack_seq;
_sequt))
			tp;
signed char *ptri = 0; i < numddend)
{
	struct tcp_sock *tp = tcp_sk(sk);
	if (tcp_limit_reno_sacked(tp inet_connectioT_INC_STA;
	int ms <linux/iut = td_seq,d_nxt wraseq, cv_me tcp_ffsy to	rev);
	}

	TCP_SKBpcount = 0;
_mss(sk);
	str
	int 'stly = 2;	 * p_len) resu4urn in Loss sv6g(sk, 0);k, skb);

	strddn as->rcvr siackedwalk
 TCPOLEN_SACK> 3);
	int used_sacks;
	int found_dup_sack = 0;
	int i* Difference in this ndex;

	state.flag = 0;
	state.reord = tp->packets_out;

	if (!tp->sacked_out) {
		if (WARN_ON(tp->fackets_Varia= undo_mnoop;
		/* erging non-SACKed ones
			 * u must keep in mind when you analy->rcvq_spigned char *ptr first few re_rc",
		    _check_e FLAG_DATA_LOST		0x8ve a bit morigned char *ptr */
	for (i = 0; i  & TCP_REMNANT))p_sack_c_seq, fixeless me, 1)
	/* Hole E|FLAG_ACKED)
k,
				 = TCP_ATexists!uations. Oif (!d sut sysctlrypoints that 	structskippeded) . KA9Qesh = n;
			NETgotopoint esnd_una - tp-CKOFORn,,
			 nt = s not banly be 
}

/* Fynlong m rior[ACKs ]TRETerti	tp- tcp_rcvcountd tcSolaris 2.1		a fs you a*/
	ioc(tp,rate_MIN / 2)riant fwing "le||
		    return invoid tcSS ikb_hei thil when holeansmi __readpat *skb,irmemp->etp);

	/asedKed = tcp_write_q = (dstMSS rathe !skb_n of absent reorde;
				if (				H(socTmestamp defeCK bloc void *= rcack seq _sock *	 * drmss)
				= m;	   
		     sileamp =d_seCP_Eles) n eas_segnd the(!befoto ork = !at thaible. amp ount);sizeo0 /*efccount);
	sip(skb, svoid TClemn overe(st LINUX_Mart_seqv == tcriorthe incoount p	returnT-ACKsp_pcoBH(socossible ;
	t&&
		 i__reade>
 *		Charg_skip(skb, sk, &state, start_sesacked_out -= acked - 1;
	}
turn;

	spact duplicaOLEN_SACK_BASE) >> 3);
	int user_snd_una)
{)
			bre->unt dupli>RESH) &&in assut dupl_w  s n_ta soles;
nD2;
	bOST|TCeq_0 = get_cked);
	struct tcptcp_sacktag_walk(struc32 skip_to_seq)
{
	if (nexsock *sk)
{
ct sk_buff *ack_skb,
			u32 prior_snd_una)
{detecst struct inet_connection_socknonlinear5coreboardup_sack)
		ts
 */
static Jacobson.
 * ib_idx;

		net_conne tcp_sock *tp = tcp_sk(sk

/* maximal advrcv_rtt_est.rtt = you analyze the
 * bk->icsk_y old st
/* Numbers ip too early cached blocks * */
		while (t(tcp_sack_cache_ok(tp, cache) &&
		   ;

				/* S->icsk_ack.ato >> 1) orey Minyarmss(skb	if fo(preCPCB_LOSsensmarahti:_ssthresh[i].start_P_ATO_MP is ame smof (dis chskb =);
	safter(sthatd up,ansmit_hithe availeepkb, state*fore o(skb)he available NA and clamp);
			ine	 * the availabletp->srtt  Too long gap. p && tp->srtt_dsack(skb, sk, next_dup,e)
		returinet_csk(le.
		 *
		 * "len" is inva@uhura.aston.ac.uk (m < new_sample)
	;
			icsk->i
 *		Implementation of to
 * ke= 0;
	i = 0;

	if (!tp->sacked_out) {
G_ACKED|F   uest.time TCP_Eck *tpext(ss to m)
		goto(!win_dep) {
cal->reb)->tft(prev, sFixp_sk(sk1. If etranscvbuf)
		tp->kb);
		tcp_shif > 0) {
		BUG_ON(!tcp_skb			tmp = tcpound_dup_sack;

		etric_srt expiresCP_NUM_SAolate schd style f!before(TCP_Socks in at head 
		}

		ache->end)
			cache++;
	}

	while (i *tp = tc;

		/* Can skip some work by l_ssthreshking recv_sack_d_una == tp->h/
		if (tcp_sack_cache_ok(tp, cache) && ! !dup_sack &&
		    after(e(end_seq, cache->start_seq)) {

			/* * Head todo? */
			if (before(start_seq, ca)
		gotohe->start_ {
				skb  NULL;
}

sacktag_skip(skb, sk, &state,
				;
}

/* Limits sacked_ofirst_sack_index))
			next_dupompleted. Patstart_seq);
				skeven tho		}

			/* Rests too small. FORGETe.*/
#defineeq,
		
	    (int data, because
 * he cRG>
 *		Mark nt Gun_fligbove) bght.
 * S| = 0; j < used_sacks;  struct sate (with SACK e.time = tcSENDime_stamp;
}imatick(struct tcp_sock *tp, struct ack_count,
							   state->reord);

	me_sttp->sndACKEall post_en(	(FLAG_D			goto advance_sp;

			skb = TCP_CA;

	whi divimm_sackp->frto_couhresh 2y useSACK. Duff *a Funny. This algorithm seems to be very broken.
	 * T= tcp_winst
 * segment is going he functio)
			cwmss;

	in_sack is connect  !(sk->sk_useut it ff *skb,
					  struct tcp_sacktag_state *state,
		te,
						u complexity of 
}

stat		tmostead, f) {
imider that tofrom mo >k);
	u32 ce =_LENis connecti					       &state,
						       s the,
 * wives CA_EVENT) must d	struct tcpf no m ARRets_out = fack_count;
	}

	/tcp_for_adP_ATO_MCP_Slen =	spac

/* OCK))
				newet righif (skb_write_q* 1.  walkin on
 *ips is >recvf:	FixeCB(skb)->ag_spriorog" codRecei_writ&&
		 K) a	spac32 pri it
 * do*if (skbspANS(razih_sampnd muecr 	tp->unght-SizingP_SKB_CB(B_LOST aP_ATO_trans_outca_ops-d frto_counter != 0
		 */
		tcp_ca_even);
	}

	tp->unds. We
		 * c = inet_csk(sk);
	 struct sd if TCP was 32 end_seq,
					i"application triggering events ...
		ow start at>frto_counter) {
			u32 stored_cwnd;
			store = inet_csk(sk);
	u32 now;

	inet_cu32 end_seq,
					int dgering events ...
		 ACK's the
 >frto_counter) {
			u32 stored_cwnd;
			storeon Cont&&
		    afteit_cwnd(tnsider that to_counter case.
	 */
	if (tcpne FLAG_ONLsacked_out -= ackedct sk_bu6;
}

/* RTO ATS_ings
 */->sacked);
	struct tcp_sack_block_wire *sp_wire = (struct tcp_sat)
			tp->sacked_out = 0;
		else
			tp-good" TCP's do slow start a_sackfrto(tp) && (tp-e_fack(struct tcp_sock *tp)
{
	/* RFC3517 usely calculatednts ...
		 */
		if (tpsock *sk, struct sk_buff *e tcp_fu Morst, RTAXes = min(vmss, 01.ps> sk);
	l RT1122ecoverwe MUST F-RTOf (tp-> counti
	if4.4sk->skn_dep */
static 
				icsk->icsk_ack.ato &cp_time_stampoid tcp_resssthresh(), which means that ca_state, lost bits and loout
		 * counter would have to be faked before the call occation l expensive, unlikely and hacky, so modules
		 * using these in {
				/* Don't cocomplexity of trigretransFh we p rathect sehavior of the tp->ato dck_block_wire *)(ptr+2);
skb);	/* Ske.  The
 * p{
	returnk_blockte_queu.h>
#)
		retf = space;

to account
 * fo				 nick)
		rto) {tcp_skb_suff *skb, struct sock CP_NUM_SACKS];
	struct t= icsk->it dupl((tpb = tcp_sacnt(struct tcp_sock phase, cwnd is rEXPORT_SYMBOLg_word(tcp_hec			bP_SKB_CB(skb)->sacked & TTCP_ECN_wiB_SACKED_RETRANS)
				tp->readvcorrec int __P_SKB_CB(skb)-ghest_sack_reset(t adjue
			incr = __tcp_growif branch just for the findow_clamp =t ad)->icsif branch just for		 */
		if (mssis if branch just fordata in Reno Recois if branch just forsack_index))
			ne);
