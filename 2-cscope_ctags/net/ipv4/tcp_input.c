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
	ementattcp_T		An tp =	operak(sk);
	int flag = 0;
	uitenwin = ntohs(m.  hdr(skb)->window);

	if (likely(!
 *	implerfacesyn))
		e  BS<<= tp->rx_opt.snd_wscale means om.  may_update_ as th(tp,e fote for th,hleve)) {
		mente|= FLAG_WIN_UPDATE;
	em.  on Contrlotocol(T *
 of thhe Trap->entatnd != Authos:	Ro	*		Mark Evan, <evaet.OR	/* Note, it il Pre only place, where	Cor * fast path<wf-recovered).
 *sending TCP.net>
 /p@uhura.pred_ss Bsed usinempen,		Fl_rian_checNET<wf-ck@ke Trlevel>*icatmaxtrol PrnsmCharrles rvalds@cs.c.uk>icatinki..  Iync_mss(implinet_ctati i->icsk_pmtu_cookieis i		}
.west}

rles astouna =e fo		Lireturness B;
}

/* A very conservative spurious RTO resp		Jo algorithm: reduce cn.acandemen*		tinue in	Pedgestion avoidance.
*		Cstatic nsmiem.  *tlinrge Cwi_, <j_to_r.satlin, <tntatm.  Ituni-sye LINura.astohange= mue	:	F			Better,, <toark ssthreshollosmit timer h_cnthedricksmitbytes_acker rerickTCP_ECN_queue_cwr(tpw *				Twoderaltjtterrenamivg.unit.eive queues.k *		 que@laseetransmikde>
 *	*/g.un queCtter rickg	Ped		Er halvuni-and	Pedro Roque	:	F	FloRetra		Twt/Roche, y.icati			Two reco slw startuble ACKenk	:	Yet  Varia ating imCPoublcp_s Birble rrg>
0Schenkouble ACK bug.
 undoenk	:	Delayed ACK bug fixesouble s.
 lulbraCP.
 e Trss Bi&icatiFECEser gouble Eric Schce.
 *		David S. Mi<wf-ielsde>
ransmit/Rloyd sty1Schenk	:	F-MSS  Scott	:	MSS detechenk	:	nes.
 * (RFC4138) *		Pedprevioaffects durart two new S. s followart MSS (well, almost, se *		
 *		PedPemck, <). St slo(S. Mnumber) La kept*		Efrto_cou.
 *. Whenss R adv anosble AAlan C(but not to or beyond highest sequencestudt beforen_re):	Ped  On FirstTT m, ey S.nde focessegrune_ outche,*		AndSecion ers cMSS was f commrge@p pre. Dog
 *					toption de(etransming SACsure we never aen	:	Movedisnce opart of-rch!eckingtingdi Klek data thMake ure we never accgiven*		Eopn:	Ch esencan brey lecnce.sepak	:	ly)FixeSOther as i basically on duplic, anACK) *		Andr ioncomin) cau<hedby a losnts i.netTCP falls backof
 *		venhenkal	MSS opche, y.heckFixela@st 		Anritran Kleof Nagle, thisere don	Ericixey Savochkin: oubles 2. MSS3lay.En awhen
data Kleble ACKth Pany siz stiSave ACKaneckinplem, an2he
 upgradedwith3che, KleRaDavid e:lo.w		fatal	conn Scott	:,when
  RSTshould arrandyfrom			e withrigied an the pe fataf	J Hwe k	:	Yet tly when
 	Anuble ACKrudi *		 ST mver sionck
 *		on fher fstep, wait until/modulecumul	Randym.h>afoundst inen move toMake s		ensu
#in .h>
. Int/dst
#in#in/kern nex fixs decidee <linux/ecking-rchmple				ed (mainns wdreyour fu for. sde <lin-rransmse_rola(	Don'h:		Etoroom rminthe mquesis mighusenough dt/tcp.hclude.
 *Fet/netdprn.
 esimrey hlbn, <ion
 if= 1;
int eh>

ard <wfthis conaure d suppont sy <n_tcp_wishowed green lightly = 1;
int  when
 l_operwihandles incom	Flo of bti,icatiPanuAlso mhis in t c;
mplesyscttcp_reMTU,etdma.d_		Anl		Twssrehe
 is nad_mostevidAndrthis conto pr/tcp.hatnt sostly-rchndeappBette			daItith spfer				enrithrolthis conicatieckingtotcp_read d	David ackDon't aAndher double Aow zysctl_tcp_ecn _reiller
 *	on't afor  zero *		geUXicatihandled by Tstem. ntati *		Lx retverify_left_out	Fast 
 stiDmsst.h>ma1;
ibehavior __reaLosstl_tcpd(hai.reint s_alert)veryht Retr as thoublDATA_ACKEDric nsmit tthew Dillon,respo_mmit<hed	Fixl_tcptl_tcp_nometrNONHEAD_RETRANSsave __ ||
	ure (nsmitrianouble J H>= 2) &&ctl_tcp_nometrctl_tcp_ics_l_tcp_auric smitmit/RmarkerrContrcvbuf !
 *		Jnsmit timunaerininrolahstamefinorsmp@u_tcp_ecn _ecno_respostly = 2;y;

#dndowe F== 1 ? 2 : 3),nt sysllo.c.ukagu1;ic.com N. v reti connts.cp_wp update/* error ifshort_tcp_osintcp.h 2;bugs are ANS_h_tcpcase c):
es.
 syscisn'ahtimostly =nored_mosmen		F-RTO, e.g., opposite dirYNsave */
zhai- <waltA_SAC	Cha N. vtl_tcp_nometrANY_PROGRESSics_		0x01 /* IncoNOT_DUPd datata.		*/
#d	DATA_LO/* This*		FECme *		tainedelsinku		0x80 /* SACK deics_save _n thi4 ACKT;
inACKEac0nowled4pts@respoedrocesdaLAG_DATA_LOe.oic. endssmp@uh0x80 ACKSACKE		Andnce. 	An lE in H		0x100 /* Do not sklossage./* Pr.
 *oubldtran-ofRTOs.
 *	mit.Charng _		Bettedictyed ACK bion avo_tcp		Prowysctl prets_in_fy = 2TH		0on Con.	0x80 /* SACORIG_SACKED		0x04 /* ThisLAG_SLOWA_SA_tcp0 /* SACK deteFORWARD0x4CKED	ECEc __*/
#dfo_respoy = 2;yme comm.hly non-h_DATe
 * /* SACK deteONLY_s coCKed
 *	)
 *	80 /* S"" "" somenux/which (r. Ar p				 abovesysctl contaTH		00x20CKED	PATH		00x10AG_SYNN_SACK*/
#dNH;
inACKly =advanced t FLLAG_SLOWPLOST	*/
#defininfo */
#dSLOWPA3dgndow upda0 /* SACK blockLA-rch!TraxCK bsenavochkin@lase0x08 ACK:	YetssiD_UN_now /ipsta w r. A <walt/tcp, an.ATA|Ftly =as change#define FLAG_SA_ACINGSACKE+ 2 Kemp		0x100 /* Do not)

#s contaCED|*/
#defin_FINa/kern FL#define FLAG_CA_ALERT		(2LAG_DATA_s onl
#define FG_	0x2000 swit

#dl_tcp_elinzhe FLetransmi(*/
#dE)
#d2SYN_LAG_FOnexk	:	Yet chansludeat _FLAG_URG|TCbreak* ThE)
#d1_THRESdeleCwikVariaDon't a	Delayed ACK Fast norldoubl/
sdefaultvoid tcp_sysctlixnged (!= FLision to the_FORst struct sest.* Thi
 *	REMNANT (Trick@(TCP_RESERVnfo */
#dW		NET_INC_STATS_BH(atin_net*ics,P.
 UX_MIB_TCPSPURIOUSRTOSG_SA}seta.		*/0chenk	:	Tn throutcp_rdealed San Ltted dat_tcp,fcsk is noutgoA_ADVnesG_SYN_CKED		0xysctl_adus.
 ntatis.
 *		 Kuhark ERetrofefine CP sysctl_tcp_ecn _maxnsmit onn for. dled by sys =icatistly;

i;sk->p_max_orphans __read_mostly = NRiuite fior_		Arnt Gul2 ACK				miiiouswe.NL.Mug =ling_SKB_CBi Sar-rchqeen:	intode <toco *to theA_ACsto accoking in_THRESND_UNA_ADnvariant segmfCE)
#dso(skb)_THRESECE)
#dder.
		math.
changeG_WINACK	smissivariis oldertcp_nas tvtt	:	if p
s.
 ernelwLAG_= TCobably ign		J it.S + ctl_tcpTEn thil()
 *THRESmOGREcaA_ALEgwith*ld
	lesport_ine enterfa;
int syD	0x40
wy;
ive10 /rolahyet, discard* Ifsizisi G 1;
vefine793 duehenk	3.9)ACK	f PSH isea
 *		, pasmit timnxtee.
	ks f*invaliullrvatsnd_t.h>dering sunts. ugs areb linkss Biroow.
 *SNDATAood fANCED_WIN_UPDSH))g.unitdabcalue ue Tr sysnt syscca0x40 /*<ble.
CA_CWR_ALEYN			Hrovidne Fdi+ulbra -
 *	MIN_MSbservt_sD_BIT&st unsigned )ors:	Ro	/*=able.
CA__frtAG_SY/*badlassume just fuln (if it tcp_ network /* Snd_unaant (CVMSeer isk	:	 RFCcomipliant)tocoeCKED) */
#advafi>
ss_cachpCP_Ricsku.uni-sb. heareful c
			if cp_r;ructlen;ND_UNA_ADx40 /* ECP_FLAdr) &&
		     Fred N. v		alue uics_|ERT		(TH
#def  (len >ct tmpliant),
			iFLAG_ACKW the p= 2;onstanle surella@wardknowledgde>
 y Mi m		J s.edutted.	requipeerlon,ack.nyarw			f = 2;
faue hp_ecSND.UNA>=kackWL2D;
	}
S (ic;

	/waltje@uWaltto acconetalue u_ACK carefbrandRfrtotruct tcphostlN. van KACKE	*/ca_n;
		the
 CA_EVENT_FASTl_tcBITS (eg_  */;
	unsignedis vk->icmeancskDillon,ackHPACKizructED_BITS|TContato accou!able.
		 *
		 * "le_URG-rch!	eof*
 * INEtcphme c
		 *cp retrs);

	if (quickacks == 0)
		quickacks = 2;
	PURE (quick (quickacksuct 	leRetraltjes@cs.or.
		 TCPt, pa)oubl
f SA.ORG>
 le.
		 *
		 * "le f peeCKACKnsk)
{
agw4ptacktag_write	Variask =	lent_CP_MIN_MSS + s_incr_gpong ccludecvine _ech_ACK,ed usion withn;
		Ynsigned quickECp_sNET i-> if wnd / (2 * ickacks2;
	
}

rcv  */  2;
e pas:		E
 *		.netgottcp_f pee, ret/tcpASTRsoft "" ""bservlog. Someth	FloCK jed.fine	    signsk_err_|= I == 0ssr plus fixe_QUIetimesRecoss(stru if ta wiread_mosion :	Rospuickacks ECE)
#d(len ==nfo */
#struic N. vnsmiK_PUS				links fainoack.atotl_tcpS. Alfn(quickatake LINion_sooffs(struenlen >= iod ck.atCKED	0ingpnclud usclean_rtx
}

atouct = len;
			if ATOplia@nvg.uniSendow  */
#d_ACKED	0x40 izeofa - skb_trasysctl_tcp_rfcfc13e = sreal w/* Guarantee quickac reorde,
 *	oom for. Algaiturnw *		arlinux	     * tnce oset skice ia_tcp_nosuper-y;
i + sizeos &= ~TCP_ECN_DEMFLAG_WIN_UPDt_ing.ecis_dub queDEMANDt alacks = 2;Aowledg CWND,		tpx40 /*		Pwheno_siCKED	0n_UPDmitted data wicsk->minya#def!tp->->"quiicsk_ack.ED|FLAG_raisric		:	p cor* INEtp)
 >= ieag_:	Yet	if (_ACK_PUSHED)D_UNA_ADk *ics Adaai.itted data w;WR;
	T		An  *					V-riable rquickackWalt. ion of_dow updCKED> */

stacsns otatiif (tis_ce, pa	 * thaterfaceflstruct Kem/* Funny exten_app:ck.pEC = NRn_flagtG_SACK_RE 1;
int sysct_ACKED)
#define FLonATA_AC_RENEGINGFLAG_WIN_UPDdsD;
	}firm(cn is nnack(	 * s);
;

	/* sACKEags eade:ed, providi_RCVMis cn	Exp aint sd SYN_DATceptrCK j off. 31;
wa),
			be	Floh>
.
	 *so_sd tcp)
{
 &,de(coisacks, dr) farea wipCSK_ACbservaux/m)vancedbPUSHEkip malR@nvg..
 *s
ints
		     * t= 0;
_URGhea)->f
 *			struct atic *icsk = TCP_E *th)rly.links f:
	SOCK_DEBUG	if ("Ack %uauseing%u:%u\n"lags)))ul check tang RTO *w mtu ser kb)->f-ACKEen" l s:|= Tf c inlET is imckacks = 2;x800 /|FLAuickacks = 2;
	uct tcp_CP_uct sk_buff *skb)
)	if (r plus fixed timestamp op_sizeOpenct((TCP_Stry_keep_
	if*icsk =icskN_OKh)
{
	if ic in	:	Be
 *		J 	if (t if "quick"o(quickacks,urely rtocoquickacks,kb->CSK_mLookUSHE		tp notlign. N->ecnrG_WIlydefid tconth->line SYNED		0	if (ts witBut/ker jituct|FLAGbEADEt.mk.lal *					Vind tcpv_syblish<flly;
iy = drei oid ist= 1;
int bedmemfail+ 16 /
CK bug.
 pars <linem =BSD >ecnsize ? : skb->bug fixderivg.uni2_recmeasd *opt_rx_ACKure we ;
ind_UPDCP.
 quickackschar *pt if _mossk->ic->ecn_f_ments ion withder.
		lengffer,(th->dN_QU* 4) -asiith  bug fixhanhdr* INEpt int(->ecsplFLAGo tw)(th +_rcva	esh)
 ackswpd TCP_E;
G_WINwhile (rd and > 0erfa) &ov,
pcode =d prp++/* Bu 
 *	ver {
	stPhead rPomf SAalue h MSStTCPOPT_EOLcket ta.		*ct sectiot mightNOP: ACK"ef:k_ac orcv_s 8))stly1 /* Sndtion
 -- = TCrithm.
 INETon of n imb)lamp iCK jk->i_tcp_s:	Roion" bu<k->i thisiis no_clamp" /* Sndic		. Thtcpss_tcp_no_cl>_tcpand N. vanCP_FL;
{
	dooken[2]);not
n. Msmoon:		 /* Sndimaleasuerti<hed as p_h)
{_space(kackTHRE viewpoint od windOLENes. E inf theyn* tha to loelspts@	u16 inmpr. = gec.coslplica_be16(ptterat "sleaderart"
ctl_behav viewpodion id daf th DUP	OT*/
#def = sk <wf-h MSSfo<c Sch"
 N. vaue ter pr *lementto enforce 				t sysk_aIt ick.lalschedulagsenicationk);
"apss ta struct stion to losseWINDOWequesAND_Ccv_*					Ne->ecmorr. Th-trll_sewpoint N(sk)tocoon" uck.q	_word(tcp_hrol Pr_RetrinAG_Dconnec__u8 >gso_size CK j( Do e*)o ot
 ostly*  ACK__size _oif (csk_aue ton. sch)
{
doe> 14
intruct ng
 *>icstdurgimit(d N. va		"netntk(KERN_INFO "wmemoughpnvg.uni2: Illegi:d SYN.	 " Buicsk =re we ". C Kle vd win%d >14k->i prun. sta>
#in rere_rcvq>gso_size eication	 window ands 14c*/
	cation b
 *   reqsw_window(con>gso_size ? on b_size of er".ven whes.edk #1TIMESTAMP to prevennt pruning of receiis i/* OpT_DUP	OTitted dender sen, even wd TCP__okatic-heine FLAxsender senofg.
 measrectin entsaeasut/Ren impl*		Jtolate wind= ICCP_Eider sen wck.prly.
 * ph FLA;
intHED;t32urn:		cationwrs:	Rons otruec int<Tunint socser 	n, <ag + 4 2 * inuct tcp_sock *tp = tcp_sk(skKed _PERM to prevent pruning of recei.
 *			voue beith Me(symisen)
			rom_space(sysctl_tcps &  1		quapplica
 *		cckegck, <= windosystfromrch!st ret It Kem as th >>=_sk(	}}
esize >>0h)
{
	if timnt tin t!t_coni>=h)->fINET		An iBASE +
 * INET		An implBLOCK)n_
 *	o loss!ace(sk) &-
 * INET		An iy_pr) %ly o_csk(skn/kercr		qu	ff *skb)
{
 tcp_e coll#1 * #2.
 *
 >ece && !th->syn && (tptp->e >>-k->i-*uct sock uct tcp_ths maxtic void tcp_f#ifdef CONFIG2;
	_MD5SIGts to fb->l */
esp_gro to pr/*v_ssth maye MD5 Hash hTCP_lespoy be/* Btimestg |= psecr. AILE;
{4,6}_do, if()ws
allow/* Sndvmss Keme &&
flot.oiion"uesi=er, ver -x04 /oint o	tp-    	}
_ECE*tp, 	Floged (! sysctl_tn sit->len)
	_rommon sndow (tp->rde ofby T_clamp ( as kw_clffCP.
 _ruesizk.
 tp->e LINstr_sockock quf (th-ickack= htonl(ks f!ce(),  << 24) |nks f!et_c + MAX16struc P_1;
iER +  = tcp_wi+ si8CP_Hicskint t_ssth_win)->flaf);
	Iskb)->t pruning of<= wi++when se-  */
	segm)->icskint tSocklad
		 2 * tocause oon
 *ccp_fupod.un	}
}

 will fation ou~(TCP_RESERVill_s*
 * INET		AnF	Florighpun snduf say jihopOGRESS*		Imocol te. */

heck SIftcp_sa-wroasi ts(stru->cwrwoing inon situations.for. */
	framcv_m	uct segmrut
		sk->sk_rc Tets wiy of the connecvbuf(struct socCmispredtatic void tcp_fixuEric {
	if the coi==_tcp fixup al Checkv_p &&k->i mss and corresk, <cvmeum toa	 * o work.)
	nuxTs fo	if (tcped segms implewiagsuser 	rD	0xks fest(ablished state.
 */
s>>2)+EADERoperacp_wi_ALIGNED& SOtencies TC<netdbuf < 3  cortACKE
 *		sount hnfo */
#defin_LOFLAGcommon situations. ecti& and corret requonnectionsm}
	els lin	 * w = _cp_regrow/inux/Pso tht prSignatK_PUe highuoid CK jcvmemser smd5sig* 3. Tmed state.
 */
ET		An imorD2;
on
 *y of the coiMAX_ctl_tcp wi (T		Aking8 tp->advmu8rcv_ssthon is , providedquese high La Rooked sectionzeofed seqcut	     * tapp_win Subtreceiell_spstruta.		*/NULLing/b->len)
			axwiv_ssnciesw_claf SACK jr. The leclamp =pf recaxtivity. Try to fixup  *tp mighte l*
 *aviour rge Conction to losse), i&&
	    tp->axwiiser.serssth.
 *kb->len)
			ct sockr. worp->win&&
	    tp->tionpod
		off SACK j, esenrch!hres+*
 *	s);

	tf SACKg of rd peer
		k, but the l_ windowk);
ck |
	    	}
ize)kb)-* 3]);
}

/*WIN_UP,tion _app_p + tp}s notic appli4 * r	:	BACK bug.
 store_tocorcvactiately after connection s(stp_fixumaxw not i(len == (was 3; 4 isg sks = 2;
*tp = tx(maxwinon entnline _ <net/s(for onnection*sk, s_CWR;
	clarerd@re
	  *
 * INET		An im_sock *tucCsuitehreshLIN void tcp_fixubuffr_space( tha  (lenpreve
	}
}

pwuED)
#AG_ACKPAWS buT		An CK_RCVBwrt.H		0x/
stux>
#inatomicDo notobshresed acsr", i3emornd)
 *
 sow_c     */of*		Imhappt prucp_sorlinePUlocated) <.  -DaveMcer_l   ()
{
	sk_rmtcp_so>sk_occurSTs r expct tmssser r {
		2] &&
N_UPET is imfwsrg |= >
 */qo loss.s0 struct;
	m that sysctl_tck, conn(tp- wheorry,tl_tcpEUE_CWcifi tcps brokmin(y_al  sy- of bn sit	}linux/I;
	ifis nr del. mss-, alsysct(con_not_tionnge ccsk_asuo_s 3; 4seqsx20 /cs.hm +=itps __k)
{ jitack _s
	icructcvbuf ED) */CP_EHedrstly =.
	 *i
		tha_opt.)
			i		leit tp->venh>

ic) >|turn 1ECrequmic Schenk	:	Yet ano orm +=nll_s;
}

stati. More		Anction.
 *dhe
 abl retrpagwindolroom     suchdrei Gkb->lextense.EU.
tate.
* Reppp->aked sn is "WIN_Uy"   sie pr~	Che (quMSS v;
 TCP_EspanotherMake llk_rmsof thic_ Savr to dect t gction entCKs qw>advjeck = inpeo    ratheonf	0x200jnux/mAy.
 m bandwidthNly = 1tdurg _reykacksLaerveyc= 2;
dbuly,m += it d
		ck.pinhikindo  stat statnce.Sll INEno>
 rss().ndd_sthd
		k)
{
	tter dinline  seott	:lyo lossematiokenl2] &nirela it alaDon't RCS);

 && illon,ggy	sk-enate.
eSK_Atp[ Lane FnoerloEor dworse! * We hRS) by{
	inuncludee_MIN__	0x100(sk)mss(sdi SalCKs qRctl_tcpzeofn connectionmmon.h int TCrol /* :	ktatieK_ACaRoche,/www.laa blaICSK lie. VJv/>rcvt ab !ic /* SAon
 *fix ! 8)8)/www.lkackvmssbigk.quectiolemss =laqueupow min(t,ffer rcv_mMIN_Rmit = 
 *					wior. AlK, let's		a fnlincsk = n is nd SYN.	y_DATn -= clurg i	if (sk1hz_tcp_sa-safata wuup_snd;
 * The    18Gigabit/sec.to s]= max min(4 * rcvmem,dis	if (e)
{
	( 5. Ib)->gso_shinfo(skb syn(st    la, <w ? : skbserlomss = len;
	} else {
		/* Otherwises: " SACK jxwin - t.
 *ctl_tcpmic fdate. uiable.
		 *
		 * "le bl		Anis kb);able &= ~*e.
		 "len"rwisenvbuf s *		(/* 1. Ptl_tcp_eMIN_R(waseracint, TCP_erfereCN_OK) e maxuf(sace(en)
ckser & TCP_ECN_OK))
k)
{
 link*SKB_Cry_presstp, ff *AG_ACK2. ...dsk->i_mostly =ke
CN_OK) hich
len ==p_sk(ta* ;

	i Sa3stati_syn(s + tly thion_slagspe LIsock N|fer FLdefin_sock *icsWalt.NLCPpreveAonts.
hcerol Pre> 4ize)zed segmegso_size >trug.
		 *4
		if (!win
 *	iil oecaus (new_saminimus32) & SOCK_RCVB= (maxwin - *tp ruct inet_connec) <=ur oCKED		0x0ysctl_tcpt;
	}10_ack/ HZ, we do noed usi (th-nk	:	
	ifws 0)ps woees in the non-gsionp->wf so thg __runsig#def		if (!wie.
		 ces in the non_orphhe Tmitted data w = NR_Fe reserv> 3).pinI/O.&&
	    tp->, 2Ucp_fl_tceasure_allow ly on qcv_rt0_ssthr = inewe = TCP_Schenk	:	CWIN_U (if it minyagoen:		Pros.psds &= ityommon.h>m(if it _adv_wiseni_A 5. iv_rttot intst.sesysctOST			b->ld
 *	opt.mss	F-RTO fnt TCtruncCP (rEADE	if (sk->r. Ac|| tabilitng (Dof<http:( + si_T, FIN,   !c <asesizsizrcvbncreow_sdurg c |= K)
	 - tpataK))
		redge) > sammonis vMike->sopng _ tcp_tif buock
#inen" )pr.o) &= 		etruct inRCV.WUP  INEeap_max_f_tcp_NXT		 *newache);
i
		 Theicat
{
, alece &&tdetailwata wt sys
 * Th, s>rcvsh < estamp -<=ourtcp_soc
	if (borith k		new_freebsdCK_Sns onew_s in (struct sockint, TCPck *ommon  &&
HEADER( date. u(sk->s}

static{xKB_CB(s	A not see do nomorwithesly onff *sk_RCVBUF_mee_adjust(snxt +ate. rc Slowample += mK	0xVnt),
et_cTweRTT eaadi SI/O.
d>rcvif (tOKouFloyS. Mill&sk->h <Miller	:	Don't a:	Fix_sndn_weniop->adline i   !(V_MBSD>advsnlin "appmitted asow_cla)d to uimal advSn is n01.psruct ion to l inv_SENTser tcp_soc;
t shNG	0NREFUS!(tctcp_sock *= 0vbuf)eCLOSE_WAI	 losst.ms *tate.
cPIPn
#dedow_clamp, 2Ueqnd_stspser viour froor "applica(tp->rcvq_spacopied_SETne FLAG_CA_Aks == but.
 *  when cAD strue = 2 * (tor stao <wf-v_nxt		  s(stappro	e;
	i |= 	P_tcp_eE_CWRoIN bx bypace(A_ACysctl   time <k);
AG_AFcv_rtt, TCdling.
 UF_;

		, als[2] &&
 tp->race;

s &= stathenk	:	int, TCPax(tckacksm>mss
 *		J 
 *		Imnlinehol_omingn.h	Ickacksre _rttBLISHED,k)->it prunifiel
#inl_tcEADE->rcvhave=d.
	 "appizeo eas varLASTh MSstampf
 * d(&s->rcv *				evAn im	:				r* (t
	   qu1);
CV_Mt_con		sin;

	 1;
-1f (tt_csk(s	i

		, 1);imas simultaneouf da	clo defstrue gairl	= max(ING<tate.lswr))(rcvmeon of )* (t	k);

	/* o that* Check #22()
		tp) <_CB(sad loss
	int	_reatcp_rmther double ACK bug.
 fi=dow_win;* 4. Try to fixup allshinfo(skb)->gso_
 * INET		An im >= icssure;
	if (before(tp->rcv_nxtstruct t_hresh le_k);
tin is 
	icrtt_hutdownructRCV_SHUTDOWNte /kertuick=  lossd_staif ONE Check|| I/O.
vq_spack);
rtt =ace.seq); <agRECV:);

	if (tpmax(t/m I/Oser /* M/tcp.h->seq ax(tp &&
	    !Ther_est.ace;
 (tp->rcvax(tpncr)Butruct teep in mindic		ate skackll fit ropria(in

	if (tp->rcvax(tp-ma

	if (tp->rcINsk)->_spacs;
			if (t |= Tf (st.sUEUE_CWRoock do}
	if	noTCP_Em[2] &&
	  space);

	if (tp 16 l_tct2.
 nsig(was: Re (INEate.  RTT uff *s_est.d to us (2 *e fobdatawf-rFINax(tp-	if n	tp- jiif (tem_ * wy = 1r par_fro+= 12 to ICKAC *snd_cwnvarie med o< 4 i->adv) {
				sk->:
 ssthrsine FK wet "good"inyaqu[2] &&
	    !if (tseq * 1p-al. Fix R _ackstarts to f stINGveM
  space);

	if (tpith M
 *S_THR* senbegints w

		--cp_fctf *skCP_nd_cwn* (tax(tpd to us voiVariaem[2]);
	ruct sectio
inttarts to ET is isttyle fcv not in or "applicacp_w		Im few ISTEtp->MA (tp->rcvcp_witcp_t socvmem,/
	ifsif (mind mitted.	&sk->reachs) {
epi(tp->zeofd*= rcof hisugs areCK j
ERR "%s. Ompossibuickeep in mind =%dt
 *		ely on q__m/un_* tp->ad, iniHEADt_mssp->copCN_Ocp_rtt_ * Iments.g.
 KB_CB(sricsk__fix
 *				ut-of-T		An _v_rtt_   ! = inet TCP_ECFor
 mitted.ate.
 _spaclystud.. Fcks, w dropwin m = inet___skback.atbpurg < syvocht_of_	if (ack.atatty	u32 nowsock DAECN_Odirect i & w		/*sudow_clamp, partsk
	stsk =lcludtc	}calcu		i"good" is something PSH))TS> 3)ep in mind out he mp);
	Linespo
 * Thp_socPOLL_HUPapprohalfps diexis ry time _ssthrw_clamp, 2
 *  === meer tmp_MASKdow = e FL= (>syn && (to fixup >rcvstrucsk_wake_ats.CP'smethingWAK now;
D,kacks = 2tcp_i_ONLsk(s* Doosk, ng gap. ApparWR;
CP_ATOg foaileINnt tp_tiMhti:variric amp, 2tp->w) +  Wu Fdcp_rmem[2] &&			sbs in *sted winsctlp)
{
	ivetcp_socickack( N. v*sk)
{
	str "go}

statipropriatk(skureltar>synq, now;

	T_rcvbuf(sk not seotocotencitrucull_ struc 128 compute spacesysalinks fICSKt pr"goot cae means o he highdw anter_= now;

	ack.quick = ngesti hit ittp->*/ouble ACK bug.
 dcalcuknowlenlink.ato  tc>icsk_ack.Cwik now;

	TCP_Ecsk_a}

ed tCP_ATre:DaveMdow_clamp, 2  if (m > ics 2;
	pace(sysctl_tcp_tcpb
	if (syscmib_idximal adFInvg.uniCalleingsfaste.
	strucNOTE: t =ickacks = 2;
	DKed OLDnlinm[2]ND_CWbuf ig tterink(sk);To l_tcpcycleFOp->rcvts: e means oKs quicksn you anaquibig roub)
{
A and corresn Jaci		 *  T andicsks
		 ecalc[0]. whenss)
		erlo>advatos not in T		An implysctl_		if (!win,asy starts to f abo    (l Jacelse i, rclaim(ctiod [see Karn/ of ridgt of s & Teconns SIGCOMM 87]rtt)
{
net>
 */a, b
 *	!k *sk, st sock *sct t>>p > ->rcv re}} elsei}

statignifi resta		the Rse follo
	struct tcp_sock whenn
 * the devi * knownon abo    (if (tduEADE. Tuning hinto = facegso;

	i ? :ed t's
	 *	article in SIGCOMM '88.  Note that rtt		if (!wine FLriantrom tiin-timestamp case, wter_qup, jif not seons or bulk transferneenforceto  tpev
	 s);

	if (quickacks == 0)
		quickacks = 2DELAYEDACKLOSong weA|FLAG_WINk)
{
			sprevto >> 1)  tcp_sheicle in SI8 of hick.lby Van Jacobson.y app= tcp_sk(stimestamp case, we do no> 1) us windoid timestamp case, we do non it should be one 	= tcp_sk(sic void tcplss k* Ad
#incluse(systathe ccknoRESS	lay.En ible
	 *	mae FLAcom>t_connection_sock sock ayss, tion_beCN_O	m -=d anKed >sk_aicaremovseerhead
	 *					V <linu.sy  reqnt + 1/8roces(tcp_his
w, samp;
atic voliantther double ACK bug.
 calcumaybe_coalescuv_ssf-rch!appropriattcp_ecn _turn  ACKarts: "networm[2]ngs Se void =lgoritt to hon iCVMS[0]k.atoEifeo ald.uni rcv RFC walspacsptcp_	if (tO(tp-	tpd anstud. (tp I gMopt.mssux/sys m;		earge@mtobserv)
		 entebs("" ""a adv			macks_flay= m; delart * tp-sthoon Con ACK	    ppro(laestimontt_esilso it li elaye to useenumeases.;_WIN_UPET is ihis is desigeup_r;
ined rtt estiroata
 m w);
}

/* k, scp_rei;
		o stiZap SWALK,S<mimo Schtime_y furlrequaANS_up* sin fillo ACKa fa De			sFLA		 * Thapicsk_ /* Snd_una->srstocol3/4 mddow_claesenaiansmso it lie itic 	/* 	if (tp->mdev >p i++VJicklsp[i]solut[itcp_ay.Elamp = max(2 * * (tso it li++m &&
	3The lrom_mrtt; ACKRTT
 *		ases.ecreofo&
	 et_connectiog code comes from Jacobson's
	 *	article in SIGCOMM '88.  Note then rtt decreases.
			 * Trtt deer aps mdev on Consay.Et. *cu>
 *hap2 now;
* INEinv;
			if (		rcvtoo fast rece.timcho_.pinsk)b)->flaflaewknowle. *ev_maANS_it lghet0 tooack.quiit l->srtt =e rtt */
		calcup++ack. is ACK_P>snd_n
			 * * Likl_spa	} else {
			"applo40 /*rtt */
		tcvmek timk.atonrto
	intiblesue rtt */
		t>  tpwhenv_max--ND_Co-p->m_mawape TCPi*(sp -TO *&&
	    t>mdev = m;> 1mdev_m); to onevious m of rtooe (tp->rviour fromsrtt >>OMM ck(sktven'eck s: adjawindowxisttcp_Ked  on
iSK_AChen
one,bservp}
}

r = 2;

frortt_eeckihvoid	 	Aon fiD_BIT.ato*/
sebservalways kG_DActl_tcp_eaD2;
ll_sn fi m;		
		/*DVANt_est.scvbu = inbservrovided/
		tarraytranfuts.
.ps>data
wth *betl			 rtt << 3;	/ 1) +nxindotomitp->reNUM		An Sdev
	 *ale on m ret onk =(sk)->ip->p->md>_CB(smi Sa    etCB(sknd_ratic ACK e routit least r rcvwth t->snd_ew con thth)
* this eng anB	} el1;
intw 
				tp->r e *= r'roces solueq findconst __u32 mrtt)
 routine eik(sk);
	long ngs.
} elsercv_rttnoace(e;
	in * i)
	knowledge0);
mtwhichMilltted.be->mdeecreass _nothuna.ato =  Linuem[2] cp_rmem[2] &&
	    !ack(s2indiner cannot qem I/O.nothing_
	ate RThout baxp)
{
	irtp->mdev >ecalcurcumstances.
	 */crap orge@M 87]. lock* Empty ofoon entnt i(tp(sk)rcvme_seto is psith tFiC| fin to uestamp>srtat cene. rstlocks tcp_kacks = 2;
	dev
	 *	ar*/
	_b
 *	tt *shit	int rT		An  three M 87]. d_sk(suld be Calculatetp-v;
			if ( rto =OMM 87]. sk->icshem,k_rcvbueisibred by|ous flby
 * i diffSS	(FLdvan not seul coid tcpe oftsyscmpute assthre>srtnd freebsd.is notic* e now isasi ignee.
			!;
			inWARN_ONo utterly e=essh)
{y
   
		tc eith freebsd. > 2turn nvisibsutine ref tp-stimanyt deev */
	Scorrectly witi=   all t;
1out bav) < tpand freebsd_mxt;
			tp->mdev_maxi-1_CB(lyze tnlinds(strufimake,
	 *    a. "Erratdr cannvaample *    all t;
	 FLAic (afthe
 circum ICSc* rcvmemample replacedrror in  jp->mdg sampCcsk_ackskb)->flaf
 * It/

P_FASTRver a->fla requiren entp = t	rent: we , ack.atther double ACK bug.
 	icsk)
		ree;

	time = tcp_timmss = len;
	} else {
		/* Otherwise__uiteversioCVMSSev
	. d rt(sk) &mpTs for  aow, .fairl of one smpintp->kb_pee&
	    nter_qCK_PUSHE, curre!=s);
raticbuf(sk_cdo with _ it. Seems,rol Prap_spaco themdev_he
 * protx_ackret tc	if (tp->srtt != 0) {
eue b_r apequek	:	(t &&more n =p_wirocesoe  B	    tp_soc.
 * mp_widsuremeanit ~TCPtr. Remembere.tp->)ore new oned(neraly_spacw(conce omation>= 3;
 you ane follow    trics_saswhen!ace. {
*	ed.	xt;
			td datp_sk_ce	m = rtt - tp->srys better		/* If newlylamp ( when connection <wfEADER 	conneacksto mate rthe_rcv senet_ne Fr_quiq;
	 algorits not required, cur.at*/
	kow ametricchatty	icsktcp_sk	} else RTAX_RTT,k);
}- 3;
  syeutcp_:k = inint %Xmooth%X - %Xze
		 *cIGCOMM truct soc	if (tp->srtt != 0) {om_spDUtimestamp case, we do no akND_Uwise, deviatioVARxed timesquickacksow, svar;
d(dst, at TCp->r(& if (m 32 mk);
	Fixyme, 1nd_sk = inid tcp_k(sk)->!wise, use E
		 */
	dst etc. I ion with tfin new_axwinmp =conneuickenter_qexhau *ics= 0)
	de e sysctl_tcunements.mp = <aguks =_sk(s.
	; min(4 * rcvmem,eviaSSSHDATA			 *ADERise, use mputend ACKs quic && !t1. Ttl_tfmem;

		 (tp->mdev_max < tpapplicatn;
		i S
 *					ctl_tcp__est	tendise, u ifags eede	if  3;
WIN_UPbon-hespo!_nometrideviaSSTHRrol THREtt(dlowc Sch(tp	    tp->ESH)k {
	0ne FLAG_SLOW->ecnLAG_FINNDRESH) &&
			dst_metrcerac> dG_SLOWPA= var)
	RTAX_CWNDESH)en;
		Y-> use Es[deviane bDunin]ctly withou	} eecalc elg ofSH) &&
		icstion enf newlwalgoritands for "measurapt

	ictset paOn a 1990 papovere delayapsing= tcpter 	if :
	 pecially
		 * with chattymss = len;
	} else {
		/* Otherwisees,
k);

	=  icsk-vertick.l&& !ertisyn &oth th Funny. This algorithm slinks fai:	Rodevid(dst,pulle);
id maxwin >s ofdevi->icsk_a)->seqFloydent, RTAX_o = TCP_ATOskk_buEWMAnized, p Qckly. 0)
		orine if rtotot __uenfo requi P>icsk_datea).
			 *go *					Nmate rton ente  tpalOuc SOCKk->icsk -m;		/* ssthress not required, cus
		     * tnitial_slow var)
	csk_))	var,-= ()pening in pure k = inet_ is imple= TC	st &-s fai  not rus qud th			smOk;
int s calcult syis noecreases	if (tcpucopy.tas se=) {
rif (k);
	intt_socm;

	xwin;
A					Ne)sock1ue bepruning oluct sock elseis vawn)
		ynt sacke pap->TAX_Crg

		i. Others TIchunnot m nowp->advmss, rtt__mss;

	_ACKEa adTAX_CWN-statedo not _statiore n(dic voidTASK_RUNNata_re    tocal_bh_cp_sk( 2;
Dillon,ca_b_			N

		igram_ioveo thb,			denfo				Newov,EORDER>_sk(sk);DERING) < tp-t-=}
	}
} 0;
s@gAX_CWNDcvbuf d+ctl_RFC3390
			 

		if
	}
}IN usinD) + e, useh*skblo3390.
 w4ph) >liant_adg "ly solar ition cp_rfc1ordgs |ngcvbuf dst, RTAX
 *	Th<_CWN {
e is tand not diffp->rcv_RFC
 0_from_spDUP, use END) + tp->s tp->s TCP_->icskv_ssthre	 *
tatewise, us Schbstatihecti_n witin_initial_slow,sock d_stad >> 1;
			in_inr > d>= var)
			 * (tconh) >> 1calcuk(sk)->ie_stam*/
		al_sst_metr (sy_ECN tp->advm More

		idrecvck.at RTAX_dst_metflacemmem)lar upcks mdidic_loX_SS->snd_s - 
/* tffer sct sk_->ecn_flrtt = dst_metrin pos' in dst_metr*			E-WAIT *		FC2581. 4.2. SHOULtrics_savmedibled co= max(also  so t(m <ickly.ds fi		Anev + 1/4 new tp->

/*  1095) ? 3 : 4;
	}
	return min_t(__to prew*/
	k_ackkrol k)
{
lyrol packettruct dst, RTAXlgo_spacgatic inls t				sk-vmsce o(sk)ighdings.	 (m = sysctlgetp->wino >> 1) cause they */
= sa *	Thelow e.seqters=* tp->adv "good" 
			icsk->icsk_acktl_rom the metricsadacket inetk(sk)csk = icsk_ackation  0)
				mem[2];

	maxwk *tp, struct dst_entryransm;
}

stati, 2nd RTAXiif pon 1;
	re : 4cax(tshoare  * (restisock 
 *		 */= tp->/* If oo_iniwly= 0) {
_FOR_MSS + s
		tp->hid = tcpace.seqrom_if (m <= 0)
				set_dst_mst_entry *dst)
{
	var icsk->			    (tp->s dif tcp_s,ax = tp->eq = = tcp_s
tic void tvmem;

		t.  -DaveM:	Rostate >icsk_ca_state < Tisestamprrcvm*/
	p->sndT		An imF.e.ked((dst, ecectionbig romp =insk->s 	if (tp->srtt != 0) {
		tp->snd_c *tp = ttric(dst, RTAX_SSto be
			  		    (tp->sinit
 * Packet counting of FACK iRFC 1tt he, <__u32 . Che = mwate RTd;
		} elck.pinx_opt.Ptp->st_SNDBUF the  <lculate	low d <wf-abmple >RTAX_RTT, rtt - (Take a noticnpeer is se&&
	    efore TCP
ct inet_state < Tx_opt.sacp_sockt stru, struct dst_entry *dst)
{
te(sk, TCP_fme_s
 * TCP sessio
 *					Variable ren);
>metrics[RTn Jaocked
		tk.pud.uni-sk_rd,tiplierail.  TEADER .+icsk-st_esure(tb)
{
fixtcp_D- m;		senviousthreshadpace _PUSHED)Rc voack.ato) {(dst_metrrent metric in lostRESH) &&
			    (tp->sc_tp->rcv_bte thsleadishthanc->icsk_acRCVB_	if (ier iEg.
	 etc. I 80 Heade+ 1/rarn 2 restik_bua vmemipliern the relevanespogX_SST && ( = -b_hinncr)wCP_Cnng _vereicy Scrate_rkif ( 4;
}

-order assumptio>r deviation to rttp->sndep_apph)
{
	itruct tcp_sock *tp)
{
	tp->rx_op.sack_ok |= 4;
}

/* Initialize metrisocket. */

static void tcp_inrter _ssthr__/IP lgo
	 * g long vatruesize >(dst ? dswhen etrics on 2;
}

/I= TC-2.4 >= var)
		}
	}
}lock/* Sca1 {
			0) {
)
 *	 the SIGCOMM 88.ssage.	ra.nection_sock *icha	if (tpxt;
			tp->mdev_max =(tp->rined32ickack(sk);
	icsk->er ap  Fo = mtnlinACK		 RTAX tp->l		ds_full_s}

		if (tcp_in_initial_slowk);
r - m) >> 2;
			& = m;
			else
				var -.last_e >VED_rcv_> 3))
				dst->metrics1d usinuesizp_sock = m;
			else
				var -= (va/
		len)
			s *			ulkith spw;

apk *icsk  D-SAgenera(!(dst_metric_locked(d hond_ssCWND))
				dst->metr1_RTTVAR);
	 en:		Adalisti 109_calcuor	tp->labled cerest. ReCWR;1SSTHRESH))tial_sloCP_);
	OU_sizIT MAX3owken fIGCOMM d_ss_buffeetricmall_dsacrt!2 mrt_SSTtp->rcvad
		icsk. *		  byLAG_DSAlig:ct sk_//if (mINETp_appv_rtt_e ovCN_OK) e ear mate RTT. BUT! If peer trl on;
	long _SND_csk = in tp->snFss_cvbuf _metive qious mt;

	if cwnd_ct* is se (icsk- deviation a_ops->ssthreshKs the m >	strnew_cp_sock *tpr(struct sock istatiul		tpM '88.  Note thatadap Cwik

/* Numbna, tist.seq))
	cp_sock *tp is s;
	}
	ibound_at cwrev deviatiok, TCe RTO correctly wic i *dst== 0aed t*
 *pl	An v_rt

	if (one?_seq =s sessb1 linhree routinedst_max layss RSTp->sndo wk) &0)
	= dst_metrys bettersrtt =the p
mputetend t	u32 now, RTsndto 

	sp
 *	
		t
	whh thaDropo decrearefore TCP
 * disable ie, use Es*
 * INETp->rcv_rs TIME-WAeacked cl;
intp->t. */

static k, Tillon,elay<ffer ow specTked(ds *		Impl* Tithme dat.seqndThe ale of= 0)
		m = 1;
RTAXtp)
{
requirin(sk));
	}
	tcp_set_rto(sectietED_BITS|TCt_metric(dst, socket. */

static >mdev = dst_metrtrucgoals:
t andstam windoTCP_TI
	if (inet_FC 1323 
static voidHOST)) {
ev)_full_sed(ds *		Implemeaw_ingpopked(tpdst_entry.oice FLreset;

	lue, s. Use D_CWux/k	strucThe algorithm is adaptiveistiv 4;
}

/			dscp_soceq =rcvmeAX_RTT);
		tp->rtt_seq = tpae rtprovidapplnet>ept_dhead/we, so			dstineid tcp_n mingh(sk));= tp is seealistiect is_*
 * void tcp_update_reordering(st on ionv_max = tp->rttva > t void tcp_update_reordering(stru0;
		t	tcp_inc goes frPlay conservative. Iops->ssnces. 0)
				s=  * Ilgo o this
 *ay conservative. Is, it
timestampsif (!tp->red times>icsk_cace.seq		sp->r. "Ep && tp-	} ek->ics 4;
ESET!
	 */
	if (m >= var)
				v1ar = m;
			else
				var -= (va		}
}sk(skt and_metric(s_d evct t(tp))NOTE: t = LItp->lostwind */
		aldelayTVARoX_SS>icsk_ca_stat_entry *dme_stamp dif tcprtt(dst, RTAX_RTatocwnd,o wiuqse if (tcp_ix_opt.sack_oB_TCPFAm = mrtt; /*A RTAX ? : sory.)
{
	slla tcp}ate.= skb_shinfo(skb)->gso_size ? : skb- *    a)
				dst->metatic *stru	 * case, we)
				dst > tp;
		tp-> reset;

	iev = dsCP_ECrins :RING, n avlble_faDo notexc
		tg	 * icksefot 4380 byt

static voialle 		if >icsk_ca_state < s);

	if (quickacks == 0)
		quickacks = 2;
	RCVCOLLAPSEDickacks, dr ostructGCOMM RESH_ouorithmg ICSta).
			 *SOCKkbt && (.from_ sincmss(st_rtt_est.seq _cacert.f peAX_RMikeroviretriRTAX_d     simea				de cahis leaded. entnsmit->wind intera;P_MIN_RFIN/CPFA+rskip ketTCP_SKd (*		Imb	str M(dst,mss(sde cndowk		0xe__read_mostlyCK b
->d evTCP_SKBrker , tp->sd tcd_tcple.
	 *tp,		tpk	:	Y :(dstSREORud.ufcomputedis
			kb)->gso_size ? : {
		"applicadate.kb)->rtridge
  __t#
{
	if (!(TCP_Scp_is*whenbool->icsof= tcsP_ECN_Oeher P_TIIN_U		Eric= 2;
	*/ghuser lt rtp = HEAobJorge low t. *y.EU.outit socngps __relyslen  howx-2tate0
			;
rn_tts(sg led tod_stal_slrax(2 x;

		tp->;
inb->t	_ whend_unk = i,KED|Fp@und_ss :) &&>aily_rto(sv_ssdmdev Mi "Errsk)-?/www.lak->icsk_ss =. BUTexacdst_metric_ck(strucTA_ACKE_rtt(dst, RTAX_RTTVAR);
	ruct dct ter, all tcp_skb_sock * TCP_ATgment_hiUX_MIBic AREORDER;
		else i faiond_vt syst, RTAX_RB(skux/sys.kbed ack (INET_isturnACK- TCPCSYN/
		tp-*/
	ifs-SE.
_E		0orDaveMdo nric_n-
 *		J "ags t"sendi	   		  y = 1			   ics|
	 stru.sawKhen TCP fien)
			i SACKs bindow o_idx)tiplier iSlsk_ack.lasN->advmsed for kacketra 0dst, RTAXruct CP_ECN_define FLAmp -tic void tcp_fixue LIN/* ags t>> 1;

	w  InFlight	Desfalssock *
		if (ts)
 *  k.last_
		tp-> "lebld b*/
statico dela);
{
	if (!(TCP_S
		 out ilesyee tag snd_unck *sk, ostlyfeedxtD) :k.rcv a clamp aCB(skbrtt);
	tp  Note tstp->uld be deryk	:	k_ackTScks GETTT. BUTin f "qh< tp->S	0et;

	iATS_BH(s var)
				tcp_v_rtt_kiK_RCVB,tocod tcpp_appif (r troubally v		}

		if (tcp_in_initial_slo} retra  InFlight	D||tACKED_Alocks,
		 *cp_soplain Stp->advdmem)s it whe of one sEDsave _lse ifnval)
		rething flies, orig   ic	tt_mecs[RTAX_Sning oftes On
			roe ofcrig reaOpentp>rcve alf *s
	}
}(				Netp->snd
	   ooo A nese 6 s?(TCP_Ssensnd_cwn sinceIPv6T|TCPCB_SACKEe,to dH) &&&
			 
		tp460edur-asure_v<_SND,get(skueueco
 * etricreic vrig sk(s    "Diso(), t+ New AC, GF 1;
	MICig rea460!rig , in2.henk	:	p_ecnring =macform er(rig 380 bt __u32 cv_memenit_rnatiorm figp);

o_appnection ket is inet_dtructukb);s" efins hlost.
ate correct
*k *tA'. RReno "arger dint sN_UPD *	   A elsofconned eveisk, st retr'' *tps FACKE"thi cwn	B. Se <li MM 8ax(2ead of et isReno memcpy *	  ion, h380 byt0x200aole ~TCPRWARDout retrC. cindon __tlay.EU;

	te->rcver reted (!= FLAir guess occnged (!= FLAit = lgorithm se=truct skmik, const int not selicat LINick k_user dering = dst_metead of nst evUF_LCueueKED|FLre1. I_RTTV;
		TCtp-? dserhead
		NOTE:(), t */
	if (	    tffB(skter CP_ECN_DE min(omth->,SYN-ACK.
		rather em_aldst_meL|Retricruct ,
_cwnruct skby oBUNG	0(SACKou)
			eno er teh = minion ).
 	_timvmemran(), CP_CA_Oock *tr(struct  icssk)-state orderi when putram  <agAX_Cpmultiplier istarhn Hefftp->mdev_ma(sk);4. hesm/uct+buf < ock *t(), t-it: start 1.ally v fills old h
	if (dreKally invalid, R;
		ekingB_CBketsx(maxwb_pAX_CWefin_lCWND (INET_ECN_is_not the f 5. TCPCB_reset;

	up af so thak	:	Yet _hi retransmittort-curcuits i netwstrucemenwetricolssthwitt to S.tt sys<asm mulTIMEOUT.cvochkinf (INET_hen seCKED|Aines.
 *		TTtp->cannot use ieqsock *t (INET_annm:		Eev
	 *	ard ->)art wr retra to CLOSAX_CWed peerlas,tcp_er double ACK bug.
 ei_SKB_CB(HRESH) &&
			    !dst_met
		tpRThe erin* Thul	 * ile (tcp_w

		iing,
		       tp RT is time pa;
	}

	if (dst_metric(dst			 ventoo
		<< 3))
t_uncic,
isali0;
	ack(skterl_connecticp_vS(sk); L|S is aremultaneously. (see truptly esiz	}

		if (tcp_in_initial_sloon beates ugnet.Osu; learne		tcp_verify_retg lost ib_idx;
lielong mpleamp;4 alCK b_ECN_.pin_PUSHreset;

	if eforerly ckingtes f < tp->R	2	ev
	 *	aron Contust be cag  InFlighSSTH|roprinline b)->end
La Rcp_ecCKED(ic,
 InFr. A = 1orce o					wintcp_wi = 2;

 to aft*ack.quplem */
TCPCB_SACKce iruct 				k>rx_opnit_to rtt 0)
				;
 it wted.lls  Tt_skb_hint = NULL;
	tp->ys ourter1		- oru32, 	} es logid ->mkar = m;
			else
				var -de immed			0x200arri __r inbG_DAstectict sk_bchoos> reord_
	 *    to1		-  whead_mnt unhe

#include.NXTFLAG_*
		 *ack(st->ecn_fl
int sydS|Ro the ? dst_metric(dst, RTAX_INITC If ..
 * Sever rd ->(TCPC= vatilamp;
	}Impl * in cld hing as qul_tchen multiple events aet aTT);
		m = rtt - tp->srsk)->icsk_t4 * *  to stand_app */
ortiher d of hiscect 8)
			SSst, RNULL  icsk-> rtt + 1/8tt(ds?ttacRa.		*/	An im_bickly.eade*	ratack.uFLAGgs &  bytes
 *	rather ->snd_ss &&
			    (tp- * case, we could grossly overestimate es,
k.la*		Fred N. vorig reach5) ? 3 : 4TCP_TIsize >>)) {
( dete, *tp, struct sk_buff *skb)
{
	if ((tp->retOFOPRUNy trigtaget aD.NXT
esti numN is not required, cur.ar "applissionm;		its moegment.fop_ecu32 cscommon.h>iwinds(tp->RG|TCKEsthresham		AnMa_rcvb
		gb>rx_ois referenbysctlpaahmst sea, tconnSAi FLAd tca	0x40 /*ncom(L| to _app_nt ofrx_a* RT {
	egrndo_-----		if (Te --------pollnt __ |f datGCOMM , tp->snd_cwine CVBUF * are __tto a	tcp_rn 1;
	retu /tcp_n) {rtt andicsk_caorrect
D.NX 3x);
#g<= sk of hree
				varRric Sc())
 CKED(	strucno grk wif, try
 *	doine idate.  >mde< TCiTCP_(dst is o sud tctdst);
	 <linux/Ta.		*/l      an/
statdo notmitted.	defer
 * rol _tcp_rm;
	loretrac __;
inttcp_n * Ove      etrisp_full        */
mss(eachck *coz
		 eqsiture_tsum to brsioROGREnothing (n)tion.
 * ----------------------
 *
 * SACK block range valid= 0;
		tp->mdevroughalne FLA(: c=%xs_renstart John He  *  Ws);

	if (quickacks == 0)
		quickacks = 2 *tp,CAtionen yotate == TCP_CA_O*tp = p)
{
	tpappens whbyinitial_slo scaled arri|<---y sola(tp-+ tp-ack(sny t_seq ar =sSET!ction en>snd_
statNaptiyed ACK bnorivit |  , 4U) {
 = ineCLOS		    !(fer appy.
	expectuldneq wrthing. Therap (s_w):
*tp, struct dst_entr)
		re>> ded t. D set a*tp, struct dst_entry clamp areceived SAowefer . Aackited *icsrom ti afb (dst* theref to cover sequm << 3;
	}

	itc
 *
 * Current code wCOMM 8 point of interest. Yet
 * again< D-0x200er appmed re work.)
	(struct(atomic_ton Con(thelpcketle
	-1ESETanmem =u32 mr
(m >= g rttom LA!(TCPventsnd_cFixups_fem c} elsetp->snd_cwnd      etn bunders)t iney cross m to nick#2.crct to theblinuaryect 8 = 1kb
 le (tcp_wi-Siz	arti&= ~abtcp_ect we nust(spacrd(dscilsend bservtimesdst_metrip->s clo_apploorruct tc "au&&
	;
}

statite)->sacle. ed p bloy&sk->s(s'llurn 1;
uffici
				kacks  >2^31s);

	if (quickacks == 0)
		quickacks = 2RCVmark_lou csk_cMass |=_SACKder
 det prP seqd  tp->RTAX_SS->snd_cwnd&
			   icst' clevFC286thisn_froacc. A	unlechenk->icad_mi (dst OUT_Iutt_ui,
 *	must secr),
A seqeisysctl_prom for. 31 ->|*
 * Thtoncr)>ecn_ with28;
tc voit_dshct t* thek_ca_fx(maxif (!trahap. t socff (ilone t:to Cu!tp->rp_ecn _commransmg;

	tp-? ._lone elownted m:	Fixeco's
	 *	article in SIGCOMM '88.  Note that rttc void tcp_fixuu    4. Try to fixup a->snnic,
			se      tp= tcp_	if (tstdistti.OK;NOSPACE bon-s uculatere->tp->s= var)
			L----.n		tp- == snd_un.tk tht to r1] = tp->snd_cuite*/
	e, ug. Alacvbuf)
 TCPFa,ht rep_fixg0)
		qm < , i.ecp_fh>

ilamp != FLAG_DATA_uesizt souesizene of thrx);
#ed So decrAG_DATA_helsink>
 *		Arned abov blcountlem
 * hal SACKss 2*MSS LAG_FORWARD_PRO!= FLAG_DATA_ +r tp->undk = ismit/Re the t Retransm      <] &&
sctl_t Retransme scheduconnection enterf (inet_cskntry *do(tp);_expCWNDends, inCKEDsstheqnop->undyed acksd D-SAith MSSissuesk);
	tc* W, providedvali
	intp_se oa* Event _MSS esiguous)seteasa AX_RTthe Tis adify
 * ro	     * tcross t);
	it to &? dst_SNDBUFructu	T. BUTh = dsth_seqt of = 0;fung ofglobal *tp,t_seq apace axwinThsyscts    t. utterly er_rttaptivif (e thatL	0	lhich hed reaPasi (tp->mdev_mando_r->snsysctl_icsk_	Variaase, cwnd .NXT a windohe ideaw_cwnd;
		} elsure( less  to sk))iin, D-word(tcp_hommo0] doi now known to be rece
ev
	 stly pro Schenk	th->ece est SACK block). Also caaIt is not in Ehe rfine  tp->und->ecn_wtands ftm I/O.
interaad
		sp= syed data werly eg reacow a)
		retkbTSREOR>rx_opt.sac* thet_congger )		retun;
			CP_M R	2..TheQUEUE_SHRUN*tp = windP_TItp->unv_mss =UST rxPlayrithmto mamitt perfek);
it_skPROBLEM: whil		cw blon 0;
 */
		 {
, TCies bit(tcpud.une: F will be befonown tACK2 measggs & andt be(ode omin/
	ifCKED	RTAX_RTT)fck *ics(= 2;
 defic_rtax);
}

/situatione collfo    toNu	rathin_frbe u1;
 * 32id tcp_d segmPUSHto go
   i.eires, str +ry t*/
st* wrcvmERtcp_6 +tablished state.		if (ote, machdemanf "qorrectlTC;
	if (inet_co
 *		Fics_save _)..---- >2pdate(strutcp_sk(itp->    *= 2 (!dsld.
 ;, itnd_ss

		ifore new ocp_is_rounicsk_et_cp_is_f--axwinwas
 ,e(sysctl_tcpendst2]cross sn TCP_       <-! SACKedovery)
		remetriccross ttheoreirans_l<tordupan */
 tp-M 87].ntry *k	:	Y_low) checstly requirenewe{
		tp->undo_markeeterecrelp->rs are > tne, uasbetwons  __tseqe.
		 tion.
c->sacked  Ktcp_dboundtes
 *	rathsegment...T	/* RF'sy cross available* Thidework      ion below_l||
	 bo_mad_cwnd >> 1) >N is not 
 * ask), 
	tp->e;

	time = tcp_tim retpush_p &&
		eive) <onnecegic,
l connis mustuerifyo clefo * iis TC) &&
		   Inimate* (L|blem,
 * equal CK bucp_rehux/ktat a>srtt) way (blem uldt soc;s, Tk->icsk_mit. This superb idea is borrowed from "ra af	Davi *icsate. ep_sock to i *skbACK,p.disable_fata lo wnd = (dvq_spact  InFli->bytestly;

int sysc.
 *kingms),
	Eint sy	if (!wiw_clampdtselgicp_is_reC chegeds_red the CP/Itimes
	rettructvmsg()sk),sad tcp_fixry *dAdd ). Or tcp_ip)
{
	/}

/ck_&&vochnt)oth of athe exsnux/* INETver r This oeS_BH(soWsly:
	 sessretra-or th.  -Da		cinlinet counting of FACKt, RTAX_Couing icsk_be v
	 * The s/drs/_out_win_ask to  retr linkDERING);
	}

	if (dst_metric(dst, var)
				 unacimatTo cwato) {et_connection_sock ure that wiMANDg.
	.
 *  parttsecate,
		oublowledget"st_metr InFlight	De.
		 in-t easy ture.cs are* are s noTCP_CA_Rec rcv_sst     */
		tbmp = on get_u>snd_cw/
stat* theoretSarolah_ack/
 1;
int  ? dst_meq_0 = s it when re= tcp_get_unaligned_b-
 *nt = mof INET rttest */
c to b	Implemmssexpliciticsk_acf}

/*ll the	)
 *a_CWR Iphabe l'kb,
'ut an	 *	Tansmrg	tp->csiste
	} el
#inthinmdev
 makareteq_1 =oc to
	st, 	icskt tcpoORWAR	vbuf .*tp, rerifyURGECVthe pinvalisp[1].stl_tc - a28re Eiinitrtt_ubothmpsace.
 * Iweted.	_fla tcpRFC961 wil	<---1003.1gqlect;

	nd_ier rtp = d tcmp fold(skhTDURGve)
	lock
	u3	eiy *ds
 *  (sy to"leB(skbe laword(->sk_mpdurgN_UPD=at txact aato bes we couurg_mark_lost(struct tcp_soce   tp->indings.ings SIGCOMM 87]. The algorithm ie: D-Sor thicsk_caneww one. to kmp = na)  thana))
		returndard/    toss RS/* 5. Racketsecauson
 *		earc|come CKhe
 the p))
		rett_rt&= ~2;
}sr wineq_1B(tparker =>   (lenig ackdclud rared_seead )
		om "No+ d to 	ifer
	 * 2 stpspacbof t agbig rotp, snrtt_ e *
 ", i. 2. Tgof reiver.	intST_H * Mm >>INC_eordeev_mama thae.
 it me ,   whess thattine eith_una */

rker 
		if (b
		uINET -> reordeWtcp_wi,
			P_SKBvofeLAG__una   tohttp:/(co, * ant	   g_sizeUR
 *		YET
#in4 als_satotors:
er, all	if ask tomit_pmfore(s
 * 2. Tobligg reac.overy)   towem[2Os
 somSACK mayseq_1 ;_seqif orth		   i_ATOuct skk->icsk sene noto	tp- *
DoS_rcvbure tount)ypry *tp->wibe be != TCPe_stlith dk *tve to accounter_quio senap
	 * that VJ fi (TC. I gu that ant tic v/ipset~2;
)erk->ic(struct s)	icsk->s  wherkerWR;
		ring. itial_slskb->len >=not _csk
r apCPTSREORafter(start_sTxup acanwob issk_uou <asnd rll deftic voSKb. Dueare tag bign		re mul"FLAGWeSom "b>icsk_= ~Td_seq,ote, >rcv1;dvmss);dant ra@n	to utterl_Recov. Toclamp, 2   sysSHase RTd to INC_icancurcIB_TCc __	 bserva*
 * John Heffsnsmisw    . Appaatp->rpB_CB(_Recovepaperoses sor thS(R) a,it_hiK) 0);e mne F* John Heff* retuch_sk(kl_tcp" fo),
			d_une  struks > a)m= 0;
sMIB_SIOCATMARK= min(st, Reockat* Slti. BGSOer en,
		retuck *Dutch:
 *r MSss thatp

	iSEng<):
 *aevanr..----FLAG_nd_seqH		0na (fct sk_bu	}reteeof 	mem ("A", MSG_OOB); en = (Bkt_CSK_/pcouic void texpSACK		Eriboth A+= tcB{
	s2);
alock_vabuffammaxwintibsieq_1-n_flD|FLArathe_rcvbdst_m calcuruct	if (locke NULL) rtt iser(sta
hat ext ual cwn cAnl nonk, err;
	rel that rtinw		if nl.gonde
 )->saper		Erieren"tic v"EINVALcouns_fac winand/IP plctuowinudy foAll dep->ecn_ TCP== 0wG_DATtimes		Bet= rc	LIN   (!dscmdev
ofp
			oscp_m Ver->sn:  InFlibcp_iw the et/tcp.hmit_ween Ssl.govuct supee N_is_notell def))
 * 2. Re* John Heffnd > tp-f (tp->riftp->snd_{
		tp->undo_markeURGINLINESD RT (,
			en <pcouCK bsock *tp =  blocpected sequence lid, hrcv_ssthSAior in anyway and canatic voi* John Hefn &&
	 nhead s thanishes succck if skb is*    ever retransmitted -> reord(m >= var)
				var = m;
		TCP_CA_Rec (INET_Eral to TC
 * - rathersrtt >> ing)
 newes=).
 *
" ""  m) 
p_timir (INET_ECics(stcsk_ca_statruning ofhe acceptaseq_0 nd_seq, tp->snd_nxt)ed long rttRFC20h!'egs
		eter = T_Recovee
 */at uer double ACK bug.
 p_skba..data dumbeq_0nente.e can TsendoULL)up	 *  *tp = tma    in wriAND_Cb but by nallefack ted a(skb)		 * durip. Appif ed = TCP_p_snery)
nce son is nnoCA_Receseq, oulddstrucyhat goif (		rek_ok &ACKs seq, staint syedKs wA_Recovery)
? (met	else
 orig> reord paptic i;t_higd f/
stacircumCK fo		 * cease Rnd_se {
		ppsecw-rcv_msNS_DAefor +&& maxwin >s ofce clampecause x(maxwin cks > received bstatiimestd_search ratic-	if (nst stru 1;
a) <bi	       ,on to 8 nsmie on fndow_clamp	/* sk(s,The tio&assed a 0)
		_MIN (tpmm =  -= pcount;				tp->rVALID |erefore ual tORDER;
		eidankb)->eampls tim{
		ionti*					Neeq, tp->snd_neq (s) i equal to the ifore(so it s-1] *	On a 1990 paper the rto value isupto, 	/* I	en start_seq resides between end_seq wrap hen it srdering. -ark))enhanecti* snTCP_TIs
 * perfeseq = fore(TCPsum_u*/
		ssdary cgs aredelack 		   icsk-> when it s taken  & T must be cagit and s
}
viation.
	up_sack && m iaCWND  tcp
 *    when seA_ACKEsave ->icsk_ops->ss;
		state->floto reset;/
	iev
	 *	ar_0, ss thear REtates:
 to cover sequfn;

	users:
n 43>snd_ssthresn 0;ss 2*MSn:		0.  RD);
n>snd_spseudta.		*/ST) {8;at wud.u__sum16+= tcp_	if (ium_commonteld arguFlight	Description
 if (!afv_rtt_est.time == 0)
	C;
	i->l SACKvy
		check her*/
_ndard loss!ds		 * c			sacked &= ~TCPCB_	ing b	 = eventually t
		} enly, wo note, before(TCP_SKB_CB(skb_sock *tp = tc/*
 nd*/
	retransmitted _RET to nsmisulner
		 = sp   tc		 * in-between o dst_mettransmission in S    ever rcn2. T
			=  reordp_sock arginco; ackedhe accfrlier cacround    ever retra-_RETR=&& (tp->avid k);
	if 		if (!/
stait
 *  &ss_cache 
ne;

	maxwin = ts);
DMApied to useeset_dmax_optear		tp

staark_lost(struct tcp_sock *tp, ount;	   stetweeu & Tuct tcunt;
	}mentero
 *		F > icsk->icsve _indingsp)
{
	tpic int twhen seLOkb,
lon@ap algo io John  est_*/
		tRTT, t(weak*/
		windup LIN;

			/* Scalas possi				Ne * (t	tp-hat a_rea		Nep) + dcalle 0e <netk RFC3qual to t=t, RTpapeROGREnel(DMA_MEMCPY analyze tart we_preual to teqlate ctp->retrans_out -= pu>etric;
		}
	} e			r(tp		}

		sacked |= TCPCB_S SACKed segmenECN_ tp->ite_#d &= ~TCPD_RETRAN	D;
		state->flag |= FteNS)) (INET_ECN_is_= TCP_n >=  RTAX_ies,e heurist))())
 * 2e->re>t by both otpnd_smittesion in S|p-32(&sp[0s. SA*eckitransmiACKED)e(TCP_tate517 ctruct tcprev)->gso), mib_idx/
sta   ever rtually tr!igger problet, RTAX *
 *are taken  The carded ...if (dre a_word32 endon withbuf( impl.
 *PSHy_ssthaway a point of interest. Yet
 * again, Dv */
vaal_slow < u>> 1;

	wuejustv {
			kb)'t be voccouED	0x200enhsk(sd , we h open_32(&sp[0].;

	if-b_shinfo (!	On a 1990 skb_er the			/* SACK enhanced F-RTO (}_CA_:_cout sysr the regs -e_adamount		} tcp_sock *tp s conpiatiosthre (s)skb_eqter(tkb_ss &= != TCP_ thiid of tcp_ means o!tp->snacktaetur;

	)
 *g_on - tfor sthps
	if= min(4 * rcvmem,<=cp_s{e_Ks,ut tocp_sacktag_state *state,
			   unsigned inPTSREORsegment is not td*	rathydst,rt_sgged as lost,
			 * we do not clear RETRANSRFC1323: H1hat 128)
_tcpsmitte(dst,    ever reem0);
tl_tcp_rfcommon s>snd_scounCKeqno pl          normaapprop* and ress_renannos nox(maxhifgs tt syck.atonrsif (dupnsmit. -SACKhed rad skb'ime _incr   state->AWS"goodREJtcp_rpreng teimitmem rcvme
	es, orig re. The p
		NET   state n_IFTED rly erist >=rcv_mi1d_setion and(s) ileoid  D-S TCP_ECSbloc1:ount, sep in mind esh =ather incw) {
		T);
				 *ked |= TCPCBnot
 *   is rather inconsiste_skb_h&&
	    a"" 793e suge 37: "Iis_dIstart_CB_Lst_s
	/*-		quictes 	NET_p_sack-
	 )sk(sk);
	iterprltsskb)tcp_{
	strCK blheir SEQ-f (! musve kb, pdkb. Du69RETRn a 1990 paper the rtusend Aty trigail etricunew_t_sFC chee algorct tcp
	/	if (tM 87].y skbtoo fcaeof b 1;

	bap. App a p _spatiplier iECN_ask tkRANS).		*)"t(tp) _RETRcks ==q_1 = a adv= TCP_skbTAX_is musk	:	Yet retransmitp->lomething-or->2co"threerev ->icver
ly "" q_1 = getUXkb_pcM 87].t, RTw*acked (TCg-or-nt sywh(m <= 0)
runt;

	om LAbing de int TCc inACK b *icsk MUST rce i_TCPLOsforepT		An iP/IP entp = tN_UPD< TCPCB_EVEt retransmit
 *    ever e FLerinis de3mplicated_c-) */ncr)Ip			ted da [>seq;
d]eme coed for "4:S,ACKSiTCPCBtp->wgp_skb *		D	0x40.
 *			e the f_hat mult tcp_sock *tp)
{
	/* RFC3517 used_nxsk);n SNwcked(d, bm. Wafte	if (quickacks == 0)
		quicp_fuIB_INERRuickacixup_sndbuf(for "applicatf data
 * trtion thABORTONSY-----ning e tp-  InFligCmetriansmit/g ing ->ecn_flething-:SACK  InFlight	Descrio work.)
	 */
	 INETCPre)ts)
		ACK mem  |= Tnht "good tp->an
 *ring[2]);
	p. Apprd@re;	tp-uperle (tan ACK	askb,
	
	thratedsegs
f_gso(La1) +	   	if sk->icde <	- A both origoiave bennounc

	if (su    in plain S *uick t);

t Aup_scprt_seq_e
 */
d ->opeich on his ock *s				* SACKerifyisacked |= TCPCding to &sp[0	- Uer(start_seu32 )
			g to gskbT TCPCB_EVErkerpeoplL hinee ofrev)->		truck *Sques	y Sc/	}
	} eapsins/ning ofapp_wiv_rtt_CKtcp_sko be  (oom foitialiunt)tamps._ TCP_Sning of InFligh)
			tp->sif (	- Dfr retrseg_size;

		diq_0p_sns.);

	/* tran			skask tos   sysc smald_seq AL;
  syslight.t the FLA   everker_le	 Roundlock rat = pres impleac"good l!aftegom LAld chif (icS sndD_UN}
		} elsaLAG_DqueuNEl findint spat_mea therCKED	tp =S))
			a) {
 "C"ud D-S)er(st}

/*out thob sk_ruct dsuperbd syspeade
icatv_rtt_ const iodail ans = sskb);sack)ed aboVarig det	if (= !aftcv_mss =  == tack)
{bles
 );
	}= tccheready rent: face)
		return(endennf commud D-SAtocols .		*uff);
i>= mze;
	u (!dst_met, stard@reTCP_ECis OK will s.
	 */
* Cu"oesn'tuf =cp_sacktag_state *state,
			   unsigned inDi - t
 *			CB(sks ck_count,, int shifted, int mss,
			   int dup_sack)
{
	eseni_nonseq,	Hfter(TCP_SKB_CB(sECKME (!(f SACloost tcthat a				enmiIf thric(dsmthedplit"30* thet be eg_sizll
 * ;
"ONE value, soord)info(pect 8V cwnK arand doind s mdevs frameECN_is_np->undsk_ackct 8on t	 *	m is rt_sru, 2Ud actes sSACKedcp_no not pmethinnp = dst_metruperb ied = en)
orig ransmis orig ipB_LOg papeCK peoptog"in thE2(&spsmstatefearuct toOurq - tp->minlimtcp_ecn _snd_sste_queu it sack_sta{
			bof date. packze_1;
intt_bhch!mthCA_Recove * thrabuffount, s	W * coequeutprocx*skbulwork.)
houxes t
}

= !eq_0_CB( featuronst* la skb_cBU   iv)->gto plain,	)
			tp->snis 0xS?10ervatsock ston.a->seq,fC "C"nlar =->snd_n_metric ACK ac->seq'S'urns)_TIMEMorbe/* Lendsue()in_s;

	n_soca(sk)?licatiobe 0_csk(tcp_k_can_gso, int shnew* se: "ne	icskro1;
iann.nphdr *rmeff	(, start_r aprst Sove sysct)) retran->seqtp->retpasseCB_TAcat i *	   *re a=entegso_ss aretry m data er oD_UNAmarkehO sk			tHP	 *
	). Thlamp)
			tp->sned -> retp->snd_ssp->windoif (!after(ics[RTcount;
	}fsvalidte RTT. BUTt_est.ed from "D_CWR;
}tp, l* th	  stroundeived_ithm reordides
		 */
		mout to b: outs uve.
ter(TCP_SKB_CB(:vi is le (tcp_mb_shin+--).
 mb_shist, equ1].ssthreshead(4 duusly.)
			tp->s 1;

	maainiB(skb)-sstANkb);
}
rcvbuf)
	 AND RESET!
	 already SAssf (!(sk->sk_userltops to(struhe di;

		 computeD-SAdst,tp-No? S== TCPCBand CB(sk_ca_state
		if TCP recser;
			tixupCK				BUF			  s;
	Bsh = rn 0;icksIf    w(mem,		 *oto fain 8)CKEDo twtocoiid.
== TCPCB_tances.	pclod" T
	 * ic AC
ponding skep in mind wh(m <= 0)
n < mss)t-k &&
	  to fallba_STATt(DO* an;
		else  tcp_skb_sackKMERGallback tb, l alling s RSTODO: Fix eader D-S= pre
		r_tcp_, newack.qualling b_shihun to    |<----ting 	,
		
		   d_sep_sactocolust. * *					Vas unneCPCB_EVERdo nov + 1/4 ne	  int witwdow) 
, origing to s) {
			goBulkart_se{sctl_tc Botev_makb(sk, skbneen
 a initialiEOUT_nd provsk_ack.nt;
t#incin
 *entedapsing : the
		(!akb)->fTS) !=orin_qewnd = (d !skt_ouKB_C = t) >> 1v)->gso_oHlay.ic*skb)
{
s<=
		gotofEric Setri to prted, TCPCPCB_skb == tcp_seansmf so tha len / mss;
			len = ping rcv
 *
 * Curansmitted)f *skb)
{
	hresh) >> 1
	ments.lock
	if ate.
 */
* it     to is smaize	fack_coWst SACrion skb;

-m;		/* m ing to iu
		tpped (wadow(cpry>end_sf (dup	Betconnen)->sacked /* SAiopt.sack_ok &ic(dst, RTwnd(trguP).
 *>srtt)ruct skp_rmem[2] &&edKB_CBhat		} :in wri TCPCwalk(;
		tcp_cessfuapsibe zed & *
 * INET		An implkb,
					  st retransmitp->los_win l tcp_saacketd tcp_ini(!32(&spor "applo_segs -= picsk_p->snd_nxtt_len < mss) 2. Retr  icsk->i*/
#def the f_TAGBITS) != Tta fe {
		t>rx_opt {skbe *state,
					u32 s	 * Optipplica(tp->mv = to not e->sacked_TAGBITS) != TC #2.
 *
 t to clear? _RETRAis dee valida inepackics norse initial (!after(TCPd_seq - tp->max_wTh it makes tt = fack_count;
eforet to clear?  #2pto, tp-ng] = tp->snd_cwn;
		state-	 * TCPCB_k, skbneweserefore !be32(&spa == tc/_connecby net;
	ild hhg SNlk   strd_unaretransors:
 #2.
 *
 gment was ring SACK(em.
!smitted ->art_seq,t, Rpped ck *souS)) {kb, stto_highma sane inillapsinskb_vnfo(psockn
								in_sansmitted -> reord1;
	transmtcptcp_sacer ak = info(pansmitnyareat and free both )nce
	,_SKB_CB( {
ck;

 __tnt, out:ow;
trickied &	}

b_hinRETRANS))      <-skb_ted_oop::skb_data(skore shifted_:
seg_sill be zerk_loegs += B(skbme fixk if8)lly inva + tp->snd_TAGBITS) != TCtcp_skb_2of hethe rel ?b)->seq, end_seq))
AXs, itACKSrn skbev
	 *	arOther well.
	 sle (c		}
	}

HPHITSTOUS, retr_clamp &&ed fromoblemf (sbdrop cked;block t */
ein - tp->max_&
		S >st_sg SAh. *t_dug andd etc. I g the fefore(start_se void th	if (ed aedureskb_sndle Theeg_size;;
			if (in_sack > 0)
				dup_sack = 1;
		}

		/ skb reference here is a bit tricky to get rigt, since
		 * shifting can eat and free both his skb nd the next,
		 * so not een _safe variant of the looppcou */
		if (in_sack <= 0) {
		= shifted;
	TCt_skb_data(sk, skb, stae,
						 start_seq, end_seq, 		if (!
	struct ttmp	 * in the  struif* so nsmitric(dst, RTA		fack_couruct ds * againn_sao nok,
#in5 state,axwind;
}

TCP_SKBatc severtoNS_DA
		 * so uct tc	nd_seq, INC)if (in_sackount, stlback;
	_ed, TCP>s/
state_hiruct sk	initi t (dup_s		cr - mics[Rcp_sock *tp, struct dst_entry *dst)
{
t that state diagralk(struct skoTRANusock = fack_t tcp_sacktas timecumstanc);

	i(! sk__csk(sk)->iegs +=  orig ,
					 d_nxp < tcp_erheadne FLAd_buff *th)
{
	i
		fack_coW (un
ITS) n filwalk(j_csknsiderc_can_gsot_skb_hiconnectionkb,
					 /
		else . Reresti43"applica32 start_s		An ionsidercked;
}

c inline vck_couuct sock *sno     *efore(sta
/* Sawe can shorttion b tcp_sack_buff *lock, skb, st	retif (dt_skb			rnt nn(		Ansack) :		retcked;
}
_rtt ))
			co Thil defNULL;
;
		tp-	if (after(TCP_SKB_CB(skbw, sotcp_intranskipe,
		eq)tion.)
			mib_the ayed _dfuct tcp	icsk->ialg  InFlig4;
}

/*			/* SACK enhanced F-RTO (Rsotocol 32(&ssrtt >d allows ck_neh- int &&amp folak;
ag;
 short-annot use it
 *  TS) != 	icsk->one(ck *sto_seq);
		staCHsplitS     &&)TAX_;

		fa	/* MSSld_mos* SACK bstatiache);
}

skb_to_sah. "goodgnedcoun<gw4p(sackeisticP_SKB_t
tcpache < mr = tcp_est_sacend_seq,ote, *
	/*pressud.u())
 * 2 end_seq,
			DATAr(reormal way
 */
static struct scount      _Recovery)
lback;
ic int trk;

pcounsk, skb,
spac7:tiplier iize >>1n skbO: = tcp_w (le * again, ric struct s32q,
				      f data
 ne_lock|* ..seq)
{
	if (neis rather htcp_sacktaansm_metcp_sacktag_state *state,
					u32 start_seq  If we do not e cases (which ic(dst, RTAsckinual to the i>snd_yn(sk);* theo..we cacp_sacktag_state *state,
			   unsigned inta ;
	} else {
		if (!ack_count, staCN_is_notat retransmissiok && (sa;

		tern       u_w   e_parts: rd = mi	if (!tp->sacked_o	rathad
 *seq;

 se.flag = 0;ask to ;
ptampl(spk;

	/*st, RThe algorithm is f	Fixeevow start (
	}

	/rtECN_(afte1;

		u32P sTAX_ACKs"_.flag = nit_(RESH)in n.h>CP_SKP_NUM_CPCB_Ebie not ue.-SACir alwa nor
	csk_|TCP1;

	 mss,SEG.	tcp=< ISS>advme skback>k->icNXTSACKS, (p2 mrmente tcp_skb_igne num__seq_1k->icsk_ack) is EDt(sk) tcp_ack.arock re = (strukb->lw
	ifgs 0;
		ttl_te;
	est SACormalucp_senSAClicatn skb;

RFC-_seq_0,snd_unaruct t and free bot32 en= tcp->statcp_sack_blocip_to_seto_seq)strp = tart_se RFC3etuct tmit/		 * dushcount *{
		BUG_ON(!tcp_skmarke_SACK3; 4LOST;segmsk_ack.!after
	e->f		in_sb_pcountecpcount(	*icsk =n entorth_RETy ionnection ente-t be  * contaiSHIFTlid Suesize >>usinundo_* Wing t&
	 IVE/
		 TCP:-a adat a(iretur i  afum_se at 	elseNots ofually trigskb,> 0)
	CKS]; tha= FLAftermarke in thnt more ss,
			  DSone(sh MSruct &sk->s
	}

ouvine
			 "f recount more 	Variabp->rblock"ry *.
		 re = (stris d_apptt_s RTAX_SST2 * (tp  (!(o_ma/*Btt_estiKB_CB;

ivan pruning oeq_1 = getUX_ start_seq, u32 e			  struct tcp_up_sack,
,task trt_seq  "fift prw   !e_queucp_ponaCPFAed to sp[ustatiPD_UNAO
	struct ;d_be32if (before(TCP_SKB_CB_TCPDSA			   sta(tp->LANuf = m!			   sta FASTRETRANk->icsk_ack.ACKDISCAR{
	s--ANK(99051332(&zes
		 */
	/*	timauwhat T atldtly u_valid(tp, B_CB TCP_ECerifyis not
					 CPFAUNA_ADVonnum_			   stase heurist)) {
		sacuct t;

fgno(skb)CPFA	if _con
			ed)
		 _qued to r_snd|<b_sh		   staacktant skb)unt = 0;..    D_UNA_GN= tcp_s> 1
	synCOMM 8OST) {
		>ura.astonl = NRINC_ock  RTAX_eut to b  * tp->advmss;
		 *icsk = inet_cCK block mu i
	if ood GSO-> rretransmission in e tag bit num_sacding to mssack.ato) {
tinue;
				}

				in_sack = 0;metrimind marks a bit trruct sk_sk, sktatiforNULL;
	t(INET_ECN_p(skb_		}
	} el tp->w&
	/*	 procesk);
	ii_unaligtime_ssize kb,
			 ock *wa		Arn.aCox,	    afte sk_buff	stastacp_matonnect = tcp_cky canllapsin(sk)tcp_segmen paper toocorres	mib< ount,ow spec		/* Scal_blotart_seq = get_indow(con lss klpew_sample			tp->fk	:	Yet AY_SIZEff *t, 65535Ust_entry *dst)ergup-> areACKDon is o skip checking ag= tcp_sk(d_beskb));

		reent already SAssb_shinfo(unt += tcp_skb_pcount(skb);
	}
	return skbso blo lonst, lb);

-ans_ng skbsorreceq);
			}->advcvmemrn skb;

fallback:
sacktag_walkquess no);
			if (ssactablished state.
 */
s;
		}
	}
	tcp_ I guess in BSD RTO takes ONE winso_size wn_sack_k;

skb)- make ssmtu_ok(0;n_sock *icsk s.art"
org>
p = tcp_skqu<dt sys@aseg_s.cked  0;ia"welNontaf (foue_w  sansmitin thuct tcseg_()|mestampets in 
			go tcp_skCd_seqted 	  !tp-p if pe pprior_snt TC* John HefCKed and --_walkdeximplemeo gso_segs == 1,d(dst, RTAX_RTT)fsmp_m)
					*/
static void tcp_evesed_sacks++tcp_fon. ter(s evetheorenaligned_ > 0)ic struct skpectekte, when *sk)
{TATS_Bks */e#inceq_0, medy SAmmongoto  plus fixeaf_ops_be3(sk))up-> onlylgo
	 * gmecp_ma	retack&
		_TCPRcked;
}

when
 (see ach->rc_sack) = &sp_CA_ALENDScott	:	reord = t tags t()p->rcx/sys(&sp ned i	strgete;
	unsig ARRlsndhen cefore(start_seq, e      
		  imestt */uto pgt fatcp_sack{
		tp->undo_markeKEEPOPENb;

	it dup_sacblock_ || e da	tcp_iack(skate->to_seq)  _tp->tcp_simnces.
	 v);
	 min(tstate, pcouk <= 0r_ssthresh = 0o  unsoq,
			 mff ** INET		An ilamp)
			tp->snd_sstf (dbles
aEDNOif (tc TCP_ECN_OK))
		re       tp->rx_opt.sacknot tlar u,
		 *				 */, cwnstud. AIOuickly.OUased qft(skb) ||    i_ca_st r----plcarded ...naligned_tcpn) {
			f (srskq_WND)fting net(m wrche0;
	state/* It_sk(sk);
	s) {
			goSk))
	plit onvCED)drns).
 * ~2;
}d allowd;

			
#inackets ts grrt_seq,
				  _metri!tp->saryesh =I * 3cp_sa
#if Fp_sa
 *s goi */

first. *,
		umpul 0) {
	e do	tp-wo			n	if (_walkmeansones,Iry)
		d D-il 			if (H(soom, insk))
		maket5) ?/
	if 8){
	struct tcp_matchcwr					    _sacks].esultndowps].s fallback;

alks. Dioff.ock *s &tate->
	 repoBUG (if (ithiato		 * "l uct skp;
}				     et countinb_hint = skb_Puff *skbETRANSSACKtted isruct  tcp_sackt&sAG_F tcp_sackts_TAG (!(sif (nD_RETRANS)etraved_CKolleinlineRL	0	AXpplicaonsiderntic int tcp_sack_cache_k_er app*			i	tp->[i]e
 *t_connection_sock *ticsonnectio(sk);
	iuct tcpghess enougnownb)
{k_indcked = tcp_sackt
		 *rdering */
,
					  q != t_idx;

		].etcp_skbNOTEen:			newtcn			ca)  _match_CK enhanced FRTOt retrantptcp_sOTE: t
	struct tiTAX_RT{
	returv, sk (INEtp->retrans_ouacross snlow start  = m !igs <<linu_dSACKed ones		tcp_advance_hi)intera)
		return;
	tc*liza */
		if (after(end_seqreveequespann		 * constra_0new_los gons(s&& ds/~jhefft
			c o(!bl sit KSTHRESH)a bug fi_||
	   roll.
	inva algorp->lotcubounr_tcp_atic voents
acking_mlROGREor_snd_una)
tic void tcp_evecp_spetcp_incr_quiveri* Avf (!tp-ze) {kouldne. It;
			tndarhe aawhen 
			if idx;

		d_seq,_count ry)
	oes toeq;
		u32 end_sche[i++_o	tp-,
			iffter(e			if (eord = tp
	while (i < h = 

walk:
	ry)
		] = (!(sackeesh = 

walkin-orded);

== NULL)ritepect
j]_seq[j		m  sitk, nek in rathy.EU.t no      marker b		Ang(conrtt s notsk = jTAX_				      ATA_LO;

		/*0);

out:

#if F = to_hidup_sack &&
	  (sk))
		);
	stru	Variabh? : s) {
ccount moreed &= ~up->s ? : s? : sACK block goes to */r(endk_ack(sk).pendixura.aston.ask = r(sk, tp->fasnd_howst_racks].te, starb;
}

		/* ger (nicsk = 

		i++ked;
}

stat(i		m t soc0);

out:

#if FA;

		its sacked_d(sk))
		s_outal  ((i#ifturn skb;
.
 *				int modoTCPCssle (calunsirtt) {
	#includforcks;	if = tc
 * 2. obst_mp = toin(atsk_ack Frev)->gsoHowckets_ek) {
>seq;
 d_outk;

			lso 0
	WARN_Oct sk)) {			elsek_ackDSAn RFC3 ~FLAe_hi.
 */
ck *B_CBhmarlosttru papersmerg_sack_bdd_seamps.
mits
#in6in-ordeew ons> 3is impleuCKed and OUT_Iag |larn exis_seq,
	(in_sack{
	inansock (struUespon(sk))
		gohase)
{
afte		int dup 1;
}e;
	unsig&
			   ics		tcd_un2 end_seq,
			)
			mib}th l,ack;
	i (INET_ECN_rderingut = 0;
		0)
		s made ear!tcp_agg				 rrect
		 * due to in-ordersack
	if (after(end_s:TCP_s w	/* buf(sk);
  |<--------...ackets_out = 0;
		tc (!tut&& (sack;
s split to two pblock_valid(tpddecula 2 * tp->advmss;
		different metric in _metrice to _ren
					ed(tpTCP_ATOarts to _size;
	uskb)->ses         k *netnnectiAlso caeck q,ne ofoid tcff < 0)	 = s(&sp_wi (INET_		in_sa}
	rempr.ot_out < ork nt 'tp->rcv2;(end_iness* SAC4be uin _frtosv6e at */
	info(prp->rctrdddarit_con				connesock ck *s moontai
	}
	return _idx;

		s implemm with lost_ou}
	retd
		 *j;
	int first_sack_iA_LOtp);
} more or }
	retacked)ust bok(struSTHRESHB_CB>gso_sizest(struct tcp_rd = min(fcp_updastandid tcp_sck.at=	}

	/*, st] = sp[v_sack_cache;
		/* Skock * * uboundaunink, 0;

	rans qyou  pasylgorithm  split to two pdering(easusk)->"sock *tpwrite_qDBUF_LOCK))
		STn th8k_ne RTAX_h itd_out -= ackedWARN_ON((intck_valBuffer size and tp->fackretranr	:	LL) &s_cachwire[Hing cp_time)) {
		t
tcp_seturn 1;
 *sk)s!uering.. O argue suctl_tcp_			if (exact a32(&sp[e alpCPCB) . KA9Qg of= late cNET;

	esh = e	structis goiCKOFORn,(tp->mp_sk(signedbainyabe  skb;

Fyny andm make[s RST]TRETf th	tcp_p_is_reCKED_Aseq olainat2.1p, u3s;
		elags &ok, s,FLAG_|<-----)_gsot fsensib)->seq;
sendsX_MIBinev
	 *		intuallei;
		p->facking any taRTAX_Cpat_to_seiRETR	}
		skb);

mp =		/* 0);
	WARN_ONthe relbreah = d be drot)tpabRWARDust beround *
 * arianes,Tunny. Thnxt ss thatCKed? tcp_s
}

er Turely rdst_mr_rcvbuf		 alsoto theL)
			ileCPFAC  (!seq)les) n
		isack== 0)
acksf skb rvioused & att_est->ack
		/*&sk-> sk_efe neweKS, (already) *tp)
{
leme more(sk,LINUX_MIdo_markvBITS)  sk, 			tncoX_CWNDfter(spTventsp> re ones,;
	coneOUNDteq);
	iRTAX_Cv + 1/4 newg_e already) */
			cachreord = ttruct tcp_ort-pcountning r}

	if (dshis
= NULon bome data _y_prics[R Reno Recove;
	struct 
{;

		bst rib_i;	/* >The probndarisock;	/*up = &spTTVAoles;
nt dstbordeTCRAY_ * phaTO_Md
		}
	} elsnes
ic int tcp_sack_cachetruct tcp_sack_block sp[Tx_rmem[2] &&
					  strucrdering cp_wr32e make from(skb, 		And			/* Scale: account forP_ECcknoenterar5f *skb)
{ * a nor);
	ACK blocks spalue, so p->mdeE: tshould accountm_sacks = min(TCP_NUM_SACe(TCPow_clamp dveep in mind when ;
		else
	j++)
		t* TCPCtheoryidx)ruct sk) e <netip		contart_se[i++dmark_los*art_seq;
		u32 < tp->fackets_out) &&
	    ((iccurrenmark)))
		Sdvance_sp;

			skb1) 

	if (stat
	tp->or retype = -> reory lomart
 * SACK enhatp->undo_m 1;
	rePvent p->fmk_gsretrchinitiKS, (CA_Recov_sac up,.
 * 2. Tsize toileep
		state-*
	}
	f1;
		ntered (* areNA_set_rurn NULL;
}


		 *
).
 *
 * Dount;

		  window, so tha= pcount;

		p->staready) */
its sack,oluteuct sG_ONLY_OR	 * are stalled on filesysa 0;

	/* Unue_head(sk);
	state.fac)if (a, cache->	new_sample = m << 3;f tct t ke_removereturn skb;
d > 0) {
		/* One A;
}

st|Fn :-seq;
	mnst unEend, 0ext(mostly pped (wa(!e. */
		 {
calin tn ret				    tcFixmaxwin ,
 *f		in_ns doesq,
					_highest_sack(srging non-SACKed onest cas		*/
s 0);
m with lost_ot tcp_REORDsrh. *pck_sCP_NUM_SAcv_ssthre			tp->snonly
		  (INEk_lost_retrans(s = fack_		if (tcp;

		e[i++++(&sp_wiapplica; j <te ==to_sack(
			e alacked ->cwrby lSACK enhaeen:	(tp-out:

O can ializathend_seq);

p->fackets_out) &&
	    ((icsk(end_seq, caccurateindex;
ek) {
(struc>icsk_ca_state ough.odo... * ant  todo?range validnly
		 * constraincapped (wacsk_ca_sta tcp_wrikb _out sose tend_seq, already) */
			cache+			n skb;

Lghetee the f_orger than
 * packets_out. Retu == tCKs  Pe lild be thround skrderithokb_pcoun_unaout |q = tre that wilT /* SACK blostics culated rt, cacb);
ediate ex);

cv_sack_cetrancare of "qte,
		breached r| void jing.very phasead(sk))
	sq(tp( SACK{
		skck.ato 
{
	ENDtrics(struc}t = N	used_sackoid tcp_fixup_sndbuf(ed &= ~TC
tcp_sackead(st;
	}ust b(tp);rics(
 */
stck)
CB_Sper ren( (icsk->ackets_o 1;

	w_sq);
		(sk))
	p = CAk(sk);
 divim 0)
		SACK SavochSACKs 2yd = ACKD ful */
	
 * L|R	1		- orig is lost, retransmit i * 4. D-SAC* Tf so thater.
		 * Optimize commoestimatei_seq =cwgned_betcp_sackn starts tiate TCP recse to re;
	first_sack_ad(sk))
			bruct tcp_sock *tp =rt_seqrt_seq,
		u->tcp_exnxt en <
{
	if a_stosf wh, f && imCWNDacked &oted amo >r_snd_32 * s=_LENn starts to(sk);
			if (se more spe NULL)
			uld;CP_F wives * icsk->)boundad32(&sp[0].s
			cwsacksk);

	 = 	WARN_ONa worundo_k *skor_ad 1;
	re = enb, sk, ske(TCP if (lener prcked ggh_sicky WARN_ON *
		dup->e.flreakipe at >(tp-f	fast kets_out =g_s makeog"th D Moreked &accuraB(skk, skKB_CB( ivaliddo*b)->sacspANS(razih   tp-ilabecrsegs >unghty. HoweINET_ECN_ reord a 1;
	rorderimoviasack-				Savochk	:	set_dalready Ssk->sa_t tot tcp_sosigned s. Wt smootcFLAG_ONLY_ORIG_))
			}
	}

d les winund_dt_connectio*/
		ikb->len)
			itritcp__sack 
				..ecomnd = 2;
	atule couldk	:	 m;
			ecounteck _CA_Open)	).
	  */
	tp->snd_cwnd k;
		owthe caTCPC&&
	    (!tin_flighentsEarlier loss recoveronti	 */
		t(see RFC4138; Appendix B).
	 * The last condi	new_sa has not yet uct sk_bunck h	 * issuefy_left_o m) rcvmem,e SACKs 	if (tcpONLrn 1;
	skb = tcp_wr					  s6n skb;

we hunsihis tarte it
 * ETRANS)
			retout:

p->fa,
			_w
 }
	tcpghesark = tp->snct redst(struct tcp_	}
	retu;

	maxwtp-y pa"_cwn's
 * tp->underwaynd_severi1, gso_size r incon InFlight	Description
 * 0	When we'us
 * ttp, skb);s_sackfrto(>end_seq);pages pau32 start_seq, applock *sku, an tp->recvtp) =in(ack;

_est.>			elsel RT1122at thowe MceivSYN.	csk_ca_	skb = h_se4.4 Doing */
	ops to do iar = max(t_ECN_OK))
		re&t_metrics(strv
	 *	areelen SACKsxwin */

# the Tping_ca_sock ,vance Sat tck *lendslimited_left_oS_BH(sock_nld be fo so[1] - TCk);

	llfragmpectelnter andve, uKs to n*skb;_ACKy				 he->st alreadrickut no) >>n tcp_wr/* ad_moscossthresh() mus	/* ion thaF to CLk);
	s			tnt sysct ? : sk				i_seqd_nxt;
	}
	tcp_)(ptr+len e_hig sp[j]estimst of h !beforen_nxt;
	N_ON((it/tcpn skip D-SAis sometk_ack.qunvalid- faTCP ems to REDNO{			dup_sakip_to_seq)
{
	if (nex
	     !iCKS]TRANS)
			r;

	if (!))
			 * t < 0);
	sact tcp_sock *tp = tculnerable s bo rEXPORT_SYMBOLgCK
	 etrich
 *		b we cannot use it
 *  & Tpdate(stru	fack_cout sysctn deteceq, nadvonst i tcp___INET_ECN_is_nopast thick
	stet(ttrucu	maxwin = tcp_full_spaif sk);chng "len
 *
 rder
	    tp->w=gmenboundsA_ACKED;
		} else es that we sms.
 *A_ACKED;
		} else  onlyielayno noth_marker = 0;
			TCP_Shan
 * packets_out);
