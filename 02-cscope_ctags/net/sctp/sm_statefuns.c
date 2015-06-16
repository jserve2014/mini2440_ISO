/* SCTP kernel implementation
 * (C) Copyright IBM Corp. 2001, 2004
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 * Copyright (c) 2001-2002 Intel Corp.
 * Copyright (c) 2002      Nokia Corp.
 *
 * This is part of the SCTP Linux Kernel Implementation.
 *
 * These are the state functions for the state machine.
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
 *    Karl Knutson          <karl@athena.chicago.il.us>
 *    Mathew Kotowsky       <kotowsky@sctp.org>
 *    Sridhar Samudrala     <samudrala@us.ibm.com>
 *    Jon Grimm             <jgrimm@us.ibm.com>
 *    Hui Huang 	    <hui.huang@nokia.com>
 *    Dajiang Zhang 	    <dajiang.zhang@nokia.com>
 *    Daisy Chang	    <daisyc@us.ibm.com>
 *    Ardelle Fan	    <ardelle.fan@intel.com>
 *    Ryan Layer	    <rmlayer@us.ibm.com>
 *    Kevin Gao		    <kevin.gao@intel.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/net.h>
#include <linux/inet.h>
#include <net/sock.h>
#include <net/inet_ecn.h>
#include <linux/skbuff.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>
#include <net/sctp/structs.h>

static struct sctp_packet *sctp_abort_pkt_new(const struct sctp_endpoint *ep,
				  const struct sctp_association *asoc,
				  struct sctp_chunk *chunk,
				  const void *payload,
				  size_t paylen);
static int sctp_eat_data(const struct sctp_association *asoc,
			 struct sctp_chunk *chunk,
			 sctp_cmd_seq_t *commands);
static struct sctp_packet *sctp_ootb_pkt_new(const struct sctp_association *asoc,
					     const struct sctp_chunk *chunk);
static void sctp_send_stale_cookie_err(const struct sctp_endpoint *ep,
				       const struct sctp_association *asoc,
				       const struct sctp_chunk *chunk,
				       sctp_cmd_seq_t *commands,
				       struct sctp_chunk *err_chunk);
static sctp_disposition_t sctp_sf_do_5_2_6_stale(const struct sctp_endpoint *ep,
						 const struct sctp_association *asoc,
						 const sctp_subtype_t type,
						 void *arg,
						 sctp_cmd_seq_t *commands);
static sctp_disposition_t sctp_sf_shut_8_4_5(const struct sctp_endpoint *ep,
					     const struct sctp_association *asoc,
					     const sctp_subtype_t type,
					     void *arg,
					     sctp_cmd_seq_t *commands);
static sctp_disposition_t sctp_sf_tabort_8_4_8(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands);
static struct sctp_sackhdr *sctp_sm_pull_sack(struct sctp_chunk *chunk);

static sctp_disposition_t sctp_stop_t1_and_abort(sctp_cmd_seq_t *commands,
					   __be16 error, int sk_err,
					   const struct sctp_association *asoc,
					   struct sctp_transport *transport);

static sctp_disposition_t sctp_sf_abort_violation(
				     const struct sctp_endpoint *ep,
				     const struct sctp_association *asoc,
				     void *arg,
				     sctp_cmd_seq_t *commands,
				     const __u8 *payload,
				     const size_t paylen);

static sctp_disposition_t sctp_sf_violation_chunklen(
				     const struct sctp_endpoint *ep,
				     const struct sctp_association *asoc,
				     const sctp_subtype_t type,
				     void *arg,
				     sctp_cmd_seq_t *commands);

static sctp_disposition_t sctp_sf_violation_paramlen(
				     const struct sctp_endpoint *ep,
				     const struct sctp_association *asoc,
				     const sctp_subtype_t type,
				     void *arg, void *ext,
				     sctp_cmd_seq_t *commands);

static sctp_disposition_t sctp_sf_violation_ctsn(
				     const struct sctp_endpoint *ep,
				     const struct sctp_association *asoc,
				     const sctp_subtype_t type,
				     void *arg,
				     sctp_cmd_seq_t *commands);

static sctp_disposition_t sctp_sf_violation_chunk(
				     const struct sctp_endpoint *ep,
				     const struct sctp_association *asoc,
				     const sctp_subtype_t type,
				     void *arg,
				     sctp_cmd_seq_t *commands);

static sctp_ierror_t sctp_sf_authenticate(const struct sctp_endpoint *ep,
				    const struct sctp_association *asoc,
				    const sctp_subtype_t type,
				    struct sctp_chunk *chunk);

static sctp_disposition_t __sctp_sf_do_9_1_abort(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands);

/* Small helper function that checks if the chunk length
 * is of the appropriate length.  The 'required_length' argument
 * is set to be the size of a specific chunk we are testing.
 * Return Values:  1 = Valid length
 * 		   0 = Invalid length
 *
 */
static inline int
sctp_chunk_length_valid(struct sctp_chunk *chunk,
			   __u16 required_length)
{
	__u16 chunk_length = ntohs(chunk->chunk_hdr->length);

	if (unlikely(chunk_length < required_length))
		return 0;

	return 1;
}

/**********************************************************
 * These are the state functions for handling chunk events.
 **********************************************************/

/*
 * Process the final SHUTDOWN COMPLETE.
 *
 * Section: 4 (C) (diagram), 9.2
 * Upon reception of the SHUTDOWN COMPLETE chunk the endpoint will verify
 * that it is in SHUTDOWN-ACK-SENT state, if it is not the chunk should be
 * discarded. If the endpoint is in the SHUTDOWN-ACK-SENT state the endpoint
 * should stop the T2-shutdown timer and remove all knowledge of the
 * association (and thus the association enters the CLOSED state).
 *
 * Verification Tag: 8.5.1(C), sctpimpguide 2.41.
 * C) Rules for packet carrying SHUTDOWN COMPLETE:
 * ...
 * - The receiver of a SHUTDOWN COMPLETE shall accept the packet
 *   if the Verification Tag field of the packet matches its own tag and
 *   the T bit is not set
 *   OR
 *   it is set to its peer's tag and the T bit is set in the Chunk
 *   Flags.
 *   Otherwise, the receiver MUST silently discard the packet
 *   and take no further action.  An endpoint MUST ignore the
 *   SHUTDOWN COMPLETE if it is not in the SHUTDOWN-ACK-SENT state.
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_4_C(const struct sctp_endpoint *ep,
				  const struct sctp_association *asoc,
				  const sctp_subtype_t type,
				  void *arg,
				  sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	struct sctp_ulpevent *ev;

	if (!sctp_vtag_verify_either(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* RFC 2960 6.10 Bundling
	 *
	 * An endpoint MUST NOT bundle INIT, INIT ACK or
	 * SHUTDOWN COMPLETE with any other chunks.
	 */
	if (!chunk->singleton)
		return sctp_sf_violation_chunk(ep, asoc, type, arg, commands);

	/* Make sure that the SHUTDOWN_COMPLETE chunk has a valid length. */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_chunkhdr_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	/* RFC 2960 10.2 SCTP-to-ULP
	 *
	 * H) SHUTDOWN COMPLETE notification
	 *
	 * When SCTP completes the shutdown procedures (section 9.2) this
	 * notification is passed to the upper layer.
	 */
	ev = sctp_ulpevent_make_assoc_change(asoc, 0, SCTP_SHUTDOWN_COMP,
					     0, 0, 0, NULL, GFP_ATOMIC);
	if (ev)
		sctp_add_cmd_sf(commands, SCTP_CMD_EVENT_ULP,
				SCTP_ULPEVENT(ev));

	/* Upon reception of the SHUTDOWN COMPLETE chunk the endpoint
	 * will verify that it is in SHUTDOWN-ACK-SENT state, if it is
	 * not the chunk should be discarded. If the endpoint is in
	 * the SHUTDOWN-ACK-SENT state the endpoint should stop the
	 * T2-shutdown timer and remove all knowledge of the
	 * association (and thus the association enters the CLOSED
	 * state).
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T2_SHUTDOWN));

	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T5_SHUTDOWN_GUARD));

	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_CLOSED));

	SCTP_INC_STATS(SCTP_MIB_SHUTDOWNS);
	SCTP_DEC_STATS(SCTP_MIB_CURRESTAB);

	sctp_add_cmd_sf(commands, SCTP_CMD_DELETE_TCB, SCTP_NULL());

	return SCTP_DISPOSITION_DELETE_TCB;
}

/*
 * Respond to a normal INIT chunk.
 * We are the side that is being asked for an association.
 *
 * Section: 5.1 Normal Establishment of an Association, B
 * B) "Z" shall respond immediately with an INIT ACK chunk.  The
 *    destination IP address of the INIT ACK MUST be set to the source
 *    IP address of the INIT to which this INIT ACK is responding.  In
 *    the response, besides filling in other parameters, "Z" must set the
 *    Verification Tag field to Tag_A, and also provide its own
 *    Verification Tag (Tag_Z) in the Initiate Tag field.
 *
 * Verification Tag: Must be 0.
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_5_1B_init(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	struct sctp_chunk *repl;
	struct sctp_association *new_asoc;
	struct sctp_chunk *err_chunk;
	struct sctp_packet *packet;
	sctp_unrecognized_param_t *unk_param;
	int len;

	/* 6.10 Bundling
	 * An endpoint MUST NOT bundle INIT, INIT ACK or
	 * SHUTDOWN COMPLETE with any other chunks.
	 *
	 * IG Section 2.11.2
	 * Furthermore, we require that the receiver of an INIT chunk MUST
	 * enforce these rules by silently discarding an arriving packet
	 * with an INIT chunk that is bundled with other chunks.
	 */
	if (!chunk->singleton)
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* If the packet is an OOTB packet which is temporarily on the
	 * control endpoint, respond with an ABORT.
	 */
	if (ep == sctp_sk((sctp_get_ctl_sock()))->ep) {
		SCTP_INC_STATS(SCTP_MIB_OUTOFBLUES);
		return sctp_sf_tabort_8_4_8(ep, asoc, type, arg, commands);
	}

	/* 3.1 A packet containing an INIT chunk MUST have a zero Verification
	 * Tag.
	 */
	if (chunk->sctp_hdr->vtag != 0)
		return sctp_sf_tabort_8_4_8(ep, asoc, type, arg, commands);

	/* Make sure that the INIT chunk has a valid length.
	 * Normally, this would cause an ABORT with a Protocol Violation
	 * error, but since we don't have an association, we'll
	 * just discard the packet.
	 */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_init_chunk_t)))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* If the INIT is coming toward a closing socket, we'll send back
	 * and ABORT.  Essentially, this catches the race of INIT being
	 * backloged to the socket at the same time as the user isses close().
	 * Since the socket and all its associations are going away, we
	 * can treat this OOTB
	 */
	if (sctp_sstate(ep->base.sk, CLOSING))
		return sctp_sf_tabort_8_4_8(ep, asoc, type, arg, commands);

	/* Verify the INIT chunk before processing it. */
	err_chunk = NULL;
	if (!sctp_verify_init(asoc, chunk->chunk_hdr->type,
			      (sctp_init_chunk_t *)chunk->chunk_hdr, chunk,
			      &err_chunk)) {
		/* This chunk contains fatal error. It is to be discarded.
		 * Send an ABORT, with causes if there is any.
		 */
		if (err_chunk) {
			packet = sctp_abort_pkt_new(ep, asoc, arg,
					(__u8 *)(err_chunk->chunk_hdr) +
					sizeof(sctp_chunkhdr_t),
					ntohs(err_chunk->chunk_hdr->length) -
					sizeof(sctp_chunkhdr_t));

			sctp_chunk_free(err_chunk);

			if (packet) {
				sctp_add_cmd_sf(commands, SCTP_CMD_SEND_PKT,
						SCTP_PACKET(packet));
				SCTP_INC_STATS(SCTP_MIB_OUTCTRLCHUNKS);
				return SCTP_DISPOSITION_CONSUME;
			} else {
				return SCTP_DISPOSITION_NOMEM;
			}
		} else {
			return sctp_sf_tabort_8_4_8(ep, asoc, type, arg,
						    commands);
		}
	}

	/* Grab the INIT header.  */
	chunk->subh.init_hdr = (sctp_inithdr_t *)chunk->skb->data;

	/* Tag the variable length parameters.  */
	chunk->param_hdr.v = skb_pull(chunk->skb, sizeof(sctp_inithdr_t));

	new_asoc = sctp_make_temp_asoc(ep, chunk, GFP_ATOMIC);
	if (!new_asoc)
		goto nomem;

	if (sctp_assoc_set_bind_addr_from_ep(new_asoc,
					     sctp_scope(sctp_source(chunk)),
					     GFP_ATOMIC) < 0)
		goto nomem_init;

	/* The call, sctp_process_init(), can fail on memory allocation.  */
	if (!sctp_process_init(new_asoc, chunk->chunk_hdr->type,
			       sctp_source(chunk),
			       (sctp_init_chunk_t *)chunk->chunk_hdr,
			       GFP_ATOMIC))
		goto nomem_init;

	/* B) "Z" shall respond immediately with an INIT ACK chunk.  */

	/* If there are errors need to be reported for unknown parameters,
	 * make sure to reserve enough room in the INIT ACK for them.
	 */
	len = 0;
	if (err_chunk)
		len = ntohs(err_chunk->chunk_hdr->length) -
			sizeof(sctp_chunkhdr_t);

	repl = sctp_make_init_ack(new_asoc, chunk, GFP_ATOMIC, len);
	if (!repl)
		goto nomem_init;

	/* If there are errors need to be reported for unknown parameters,
	 * include them in the outgoing INIT ACK as "Unrecognized parameter"
	 * parameter.
	 */
	if (err_chunk) {
		/* Get the "Unrecognized parameter" parameter(s) out of the
		 * ERROR chunk generated by sctp_verify_init(). Since the
		 * error cause code for "unknown parameter" and the
		 * "Unrecognized parameter" type is the same, we can
		 * construct the parameters in INIT ACK by copying the
		 * ERROR causes over.
		 */
		unk_param = (sctp_unrecognized_param_t *)
			    ((__u8 *)(err_chunk->chunk_hdr) +
			    sizeof(sctp_chunkhdr_t));
		/* Replace the cause code with the "Unrecognized parameter"
		 * parameter type.
		 */
		sctp_addto_chunk(repl, len, unk_param);
		sctp_chunk_free(err_chunk);
	}

	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_ASOC, SCTP_ASOC(new_asoc));

	sctp_add_cmd_sf(commands, SCTP_CMD_REPLY, SCTP_CHUNK(repl));

	/*
	 * Note:  After sending out INIT ACK with the State Cookie parameter,
	 * "Z" MUST NOT allocate any resources, nor keep any states for the
	 * new association.  Otherwise, "Z" will be vulnerable to resource
	 * attacks.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_DELETE_TCB, SCTP_NULL());

	return SCTP_DISPOSITION_DELETE_TCB;

nomem_init:
	sctp_association_free(new_asoc);
nomem:
	if (err_chunk)
		sctp_chunk_free(err_chunk);
	return SCTP_DISPOSITION_NOMEM;
}

/*
 * Respond to a normal INIT ACK chunk.
 * We are the side that is initiating the association.
 *
 * Section: 5.1 Normal Establishment of an Association, C
 * C) Upon reception of the INIT ACK from "Z", "A" shall stop the T1-init
 *    timer and leave COOKIE-WAIT state. "A" shall then send the State
 *    Cookie received in the INIT ACK chunk in a COOKIE ECHO chunk, start
 *    the T1-cookie timer, and enter the COOKIE-ECHOED state.
 *
 *    Note: The COOKIE ECHO chunk can be bundled with any pending outbound
 *    DATA chunks, but it MUST be the first chunk in the packet and
 *    until the COOKIE ACK is returned the sender MUST NOT send any
 *    other packets to the peer.
 *
 * Verification Tag: 3.3.3
 *   If the value of the Initiate Tag in a received INIT ACK chunk is
 *   found to be 0, the receiver MUST treat it as an error and close the
 *   association by transmitting an ABORT.
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_5_1C_ack(const struct sctp_endpoint *ep,
				       const struct sctp_association *asoc,
				       const sctp_subtype_t type,
				       void *arg,
				       sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	sctp_init_chunk_t *initchunk;
	struct sctp_chunk *err_chunk;
	struct sctp_packet *packet;

	if (!sctp_vtag_verify(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* 6.10 Bundling
	 * An endpoint MUST NOT bundle INIT, INIT ACK or
	 * SHUTDOWN COMPLETE with any other chunks.
	 */
	if (!chunk->singleton)
		return sctp_sf_violation_chunk(ep, asoc, type, arg, commands);

	/* Make sure that the INIT-ACK chunk has a valid length */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_initack_chunk_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);
	/* Grab the INIT header.  */
	chunk->subh.init_hdr = (sctp_inithdr_t *) chunk->skb->data;

	/* Verify the INIT chunk before processing it. */
	err_chunk = NULL;
	if (!sctp_verify_init(asoc, chunk->chunk_hdr->type,
			      (sctp_init_chunk_t *)chunk->chunk_hdr, chunk,
			      &err_chunk)) {

		sctp_error_t error = SCTP_ERROR_NO_RESOURCE;

		/* This chunk contains fatal error. It is to be discarded.
		 * Send an ABORT, with causes.  If there are no causes,
		 * then there wasn't enough memory.  Just terminate
		 * the association.
		 */
		if (err_chunk) {
			packet = sctp_abort_pkt_new(ep, asoc, arg,
					(__u8 *)(err_chunk->chunk_hdr) +
					sizeof(sctp_chunkhdr_t),
					ntohs(err_chunk->chunk_hdr->length) -
					sizeof(sctp_chunkhdr_t));

			sctp_chunk_free(err_chunk);

			if (packet) {
				sctp_add_cmd_sf(commands, SCTP_CMD_SEND_PKT,
						SCTP_PACKET(packet));
				SCTP_INC_STATS(SCTP_MIB_OUTCTRLCHUNKS);
				error = SCTP_ERROR_INV_PARAM;
			}
		}

		/* SCTP-AUTH, Section 6.3:
		 *    It should be noted that if the receiver wants to tear
		 *    down an association in an authenticated way only, the
		 *    handling of malformed packets should not result in
		 *    tearing down the association.
		 *
		 * This means that if we only want to abort associations
		 * in an authenticated way (i.e AUTH+ABORT), then we
		 * can't destroy this association just becuase the packet
		 * was malformed.
		 */
		if (sctp_auth_recv_cid(SCTP_CID_ABORT, asoc))
			return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

		SCTP_INC_STATS(SCTP_MIB_ABORTEDS);
		return sctp_stop_t1_and_abort(commands, error, ECONNREFUSED,
						asoc, chunk->transport);
	}

	/* Tag the variable length parameters.  Note that we never
	 * convert the parameters in an INIT chunk.
	 */
	chunk->param_hdr.v = skb_pull(chunk->skb, sizeof(sctp_inithdr_t));

	initchunk = (sctp_init_chunk_t *) chunk->chunk_hdr;

	sctp_add_cmd_sf(commands, SCTP_CMD_PEER_INIT,
			SCTP_PEER_INIT(initchunk));

	/* Reset init error count upon receipt of INIT-ACK.  */
	sctp_add_cmd_sf(commands, SCTP_CMD_INIT_COUNTER_RESET, SCTP_NULL());

	/* 5.1 C) "A" shall stop the T1-init timer and leave
	 * COOKIE-WAIT state.  "A" shall then ... start the T1-cookie
	 * timer, and enter the COOKIE-ECHOED state.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_INIT));
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_START,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIE));
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_COOKIE_ECHOED));

	/* SCTP-AUTH: genereate the assocition shared keys so that
	 * we can potentially signe the COOKIE-ECHO.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_ASSOC_SHKEY, SCTP_NULL());

	/* 5.1 C) "A" shall then send the State Cookie received in the
	 * INIT ACK chunk in a COOKIE ECHO chunk, ...
	 */
	/* If there is any errors to report, send the ERROR chunk generated
	 * for unknown parameters as well.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_GEN_COOKIE_ECHO,
			SCTP_CHUNK(err_chunk));

	return SCTP_DISPOSITION_CONSUME;
}

/*
 * Respond to a normal COOKIE ECHO chunk.
 * We are the side that is being asked for an association.
 *
 * Section: 5.1 Normal Establishment of an Association, D
 * D) Upon reception of the COOKIE ECHO chunk, Endpoint "Z" will reply
 *    with a COOKIE ACK chunk after building a TCB and moving to
 *    the ESTABLISHED state. A COOKIE ACK chunk may be bundled with
 *    any pending DATA chunks (and/or SACK chunks), but the COOKIE ACK
 *    chunk MUST be the first chunk in the packet.
 *
 *   IMPLEMENTATION NOTE: An implementation may choose to send the
 *   Communication Up notification to the SCTP user upon reception
 *   of a valid COOKIE ECHO chunk.
 *
 * Verification Tag: 8.5.1 Exceptions in Verification Tag Rules
 * D) Rules for packet carrying a COOKIE ECHO
 *
 * - When sending a COOKIE ECHO, the endpoint MUST use the value of the
 *   Initial Tag received in the INIT ACK.
 *
 * - The receiver of a COOKIE ECHO follows the procedures in Section 5.
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_5_1D_ce(const struct sctp_endpoint *ep,
				      const struct sctp_association *asoc,
				      const sctp_subtype_t type, void *arg,
				      sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	struct sctp_association *new_asoc;
	sctp_init_chunk_t *peer_init;
	struct sctp_chunk *repl;
	struct sctp_ulpevent *ev, *ai_ev = NULL;
	int error = 0;
	struct sctp_chunk *err_chk_p;
	struct sock *sk;

	/* If the packet is an OOTB packet which is temporarily on the
	 * control endpoint, respond with an ABORT.
	 */
	if (ep == sctp_sk((sctp_get_ctl_sock()))->ep) {
		SCTP_INC_STATS(SCTP_MIB_OUTOFBLUES);
		return sctp_sf_tabort_8_4_8(ep, asoc, type, arg, commands);
	}

	/* Make sure that the COOKIE_ECHO chunk has a valid length.
	 * In this case, we check that we have enough for at least a
	 * chunk header.  More detailed verification is done
	 * in sctp_unpack_cookie().
	 */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_chunkhdr_t)))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* If the endpoint is not listening or if the number of associations
	 * on the TCP-style socket exceed the max backlog, respond with an
	 * ABORT.
	 */
	sk = ep->base.sk;
	if (!sctp_sstate(sk, LISTENING) ||
	    (sctp_style(sk, TCP) && sk_acceptq_is_full(sk)))
		return sctp_sf_tabort_8_4_8(ep, asoc, type, arg, commands);

	/* "Decode" the chunk.  We have no optional parameters so we
	 * are in good shape.
	 */
	chunk->subh.cookie_hdr =
		(struct sctp_signed_cookie *)chunk->skb->data;
	if (!pskb_pull(chunk->skb, ntohs(chunk->chunk_hdr->length) -
					 sizeof(sctp_chunkhdr_t)))
		goto nomem;

	/* 5.1 D) Upon reception of the COOKIE ECHO chunk, Endpoint
	 * "Z" will reply with a COOKIE ACK chunk after building a TCB
	 * and moving to the ESTABLISHED state.
	 */
	new_asoc = sctp_unpack_cookie(ep, asoc, chunk, GFP_ATOMIC, &error,
				      &err_chk_p);

	/* FIXME:
	 * If the re-build failed, what is the proper error path
	 * from here?
	 *
	 * [We should abort the association. --piggy]
	 */
	if (!new_asoc) {
		/* FIXME: Several errors are possible.  A bad cookie should
		 * be silently discarded, but think about logging it too.
		 */
		switch (error) {
		case -SCTP_IERROR_NOMEM:
			goto nomem;

		case -SCTP_IERROR_STALE_COOKIE:
			sctp_send_stale_cookie_err(ep, asoc, chunk, commands,
						   err_chk_p);
			return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

		case -SCTP_IERROR_BAD_SIG:
		default:
			return sctp_sf_pdiscard(ep, asoc, type, arg, commands);
		}
	}


	/* Delay state machine commands until later.
	 *
	 * Re-build the bind address for the association is done in
	 * the sctp_unpack_cookie() already.
	 */
	/* This is a brand-new association, so these are not yet side
	 * effects--it is safe to run them here.
	 */
	peer_init = &chunk->subh.cookie_hdr->c.peer_init[0];

	if (!sctp_process_init(new_asoc, chunk->chunk_hdr->type,
			       &chunk->subh.cookie_hdr->c.peer_addr,
			       peer_init, GFP_ATOMIC))
		goto nomem_init;

	/* SCTP-AUTH:  Now that we've populate required fields in
	 * sctp_process_init, set up the assocaition shared keys as
	 * necessary so that we can potentially authenticate the ACK
	 */
	error = sctp_auth_asoc_init_active_key(new_asoc, GFP_ATOMIC);
	if (error)
		goto nomem_init;

	/* SCTP-AUTH:  auth_chunk pointer is only set when the cookie-echo
	 * is supposed to be authenticated and we have to do delayed
	 * authentication.  We've just recreated the association using
	 * the information in the cookie and now it's much easier to
	 * do the authentication.
	 */
	if (chunk->auth_chunk) {
		struct sctp_chunk auth;
		sctp_ierror_t ret;

		/* set-up our fake chunk so that we can process it */
		auth.skb = chunk->auth_chunk;
		auth.asoc = chunk->asoc;
		auth.sctp_hdr = chunk->sctp_hdr;
		auth.chunk_hdr = (sctp_chunkhdr_t *)skb_push(chunk->auth_chunk,
					    sizeof(sctp_chunkhdr_t));
		skb_pull(chunk->auth_chunk, sizeof(sctp_chunkhdr_t));
		auth.transport = chunk->transport;

		ret = sctp_sf_authenticate(ep, new_asoc, type, &auth);

		/* We can now safely free the auth_chunk clone */
		kfree_skb(chunk->auth_chunk);

		if (ret != SCTP_IERROR_NO_ERROR) {
			sctp_association_free(new_asoc);
			return sctp_sf_pdiscard(ep, asoc, type, arg, commands);
		}
	}

	repl = sctp_make_cookie_ack(new_asoc, chunk);
	if (!repl)
		goto nomem_init;

	/* RFC 2960 5.1 Normal Establishment of an Association
	 *
	 * D) IMPLEMENTATION NOTE: An implementation may choose to
	 * send the Communication Up notification to the SCTP user
	 * upon reception of a valid COOKIE ECHO chunk.
	 */
	ev = sctp_ulpevent_make_assoc_change(new_asoc, 0, SCTP_COMM_UP, 0,
					     new_asoc->c.sinit_num_ostreams,
					     new_asoc->c.sinit_max_instreams,
					     NULL, GFP_ATOMIC);
	if (!ev)
		goto nomem_ev;

	/* Sockets API Draft Section 5.3.1.6
	 * When a peer sends a Adaptation Layer Indication parameter , SCTP
	 * delivers this notification to inform the application that of the
	 * peers requested adaptation layer.
	 */
	if (new_asoc->peer.adaptation_ind) {
		ai_ev = sctp_ulpevent_make_adaptation_indication(new_asoc,
							    GFP_ATOMIC);
		if (!ai_ev)
			goto nomem_aiev;
	}

	/* Add all the state machine commands now since we've created
	 * everything.  This way we don't introduce memory corruptions
	 * during side-effect processing and correclty count established
	 * associations.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_ASOC, SCTP_ASOC(new_asoc));
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_ESTABLISHED));
	SCTP_INC_STATS(SCTP_MIB_CURRESTAB);
	SCTP_INC_STATS(SCTP_MIB_PASSIVEESTABS);
	sctp_add_cmd_sf(commands, SCTP_CMD_HB_TIMERS_START, SCTP_NULL());

	if (new_asoc->autoclose)
		sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_START,
				SCTP_TO(SCTP_EVENT_TIMEOUT_AUTOCLOSE));

	/* This will send the COOKIE ACK */
	sctp_add_cmd_sf(commands, SCTP_CMD_REPLY, SCTP_CHUNK(repl));

	/* Queue the ASSOC_CHANGE event */
	sctp_add_cmd_sf(commands, SCTP_CMD_EVENT_ULP, SCTP_ULPEVENT(ev));

	/* Send up the Adaptation Layer Indication event */
	if (ai_ev)
		sctp_add_cmd_sf(commands, SCTP_CMD_EVENT_ULP,
				SCTP_ULPEVENT(ai_ev));

	return SCTP_DISPOSITION_CONSUME;

nomem_aiev:
	sctp_ulpevent_free(ev);
nomem_ev:
	sctp_chunk_free(repl);
nomem_init:
	sctp_association_free(new_asoc);
nomem:
	return SCTP_DISPOSITION_NOMEM;
}

/*
 * Respond to a normal COOKIE ACK chunk.
 * We are the side that is being asked for an association.
 *
 * RFC 2960 5.1 Normal Establishment of an Association
 *
 * E) Upon reception of the COOKIE ACK, endpoint "A" will move from the
 *    COOKIE-ECHOED state to the ESTABLISHED state, stopping the T1-cookie
 *    timer. It may also notify its ULP about the successful
 *    establishment of the association with a Communication Up
 *    notification (see Section 10).
 *
 * Verification Tag:
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_5_1E_ca(const struct sctp_endpoint *ep,
				      const struct sctp_association *asoc,
				      const sctp_subtype_t type, void *arg,
				      sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	struct sctp_ulpevent *ev;

	if (!sctp_vtag_verify(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* Verify that the chunk length for the COOKIE-ACK is OK.
	 * If we don't do this, any bundled chunks may be junked.
	 */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_chunkhdr_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	/* Reset init error count upon receipt of COOKIE-ACK,
	 * to avoid problems with the managemement of this
	 * counter in stale cookie situations when a transition back
	 * from the COOKIE-ECHOED state to the COOKIE-WAIT
	 * state is performed.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_INIT_COUNTER_RESET, SCTP_NULL());

	/* RFC 2960 5.1 Normal Establishment of an Association
	 *
	 * E) Upon reception of the COOKIE ACK, endpoint "A" will move
	 * from the COOKIE-ECHOED state to the ESTABLISHED state,
	 * stopping the T1-cookie timer.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIE));
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_ESTABLISHED));
	SCTP_INC_STATS(SCTP_MIB_CURRESTAB);
	SCTP_INC_STATS(SCTP_MIB_ACTIVEESTABS);
	sctp_add_cmd_sf(commands, SCTP_CMD_HB_TIMERS_START, SCTP_NULL());
	if (asoc->autoclose)
		sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_START,
				SCTP_TO(SCTP_EVENT_TIMEOUT_AUTOCLOSE));

	/* It may also notify its ULP about the successful
	 * establishment of the association with a Communication Up
	 * notification (see Section 10).
	 */
	ev = sctp_ulpevent_make_assoc_change(asoc, 0, SCTP_COMM_UP,
					     0, asoc->c.sinit_num_ostreams,
					     asoc->c.sinit_max_instreams,
					     NULL, GFP_ATOMIC);

	if (!ev)
		goto nomem;

	sctp_add_cmd_sf(commands, SCTP_CMD_EVENT_ULP, SCTP_ULPEVENT(ev));

	/* Sockets API Draft Section 5.3.1.6
	 * When a peer sends a Adaptation Layer Indication parameter , SCTP
	 * delivers this notification to inform the application that of the
	 * peers requested adaptation layer.
	 */
	if (asoc->peer.adaptation_ind) {
		ev = sctp_ulpevent_make_adaptation_indication(asoc, GFP_ATOMIC);
		if (!ev)
			goto nomem;

		sctp_add_cmd_sf(commands, SCTP_CMD_EVENT_ULP,
				SCTP_ULPEVENT(ev));
	}

	return SCTP_DISPOSITION_CONSUME;
nomem:
	return SCTP_DISPOSITION_NOMEM;
}

/* Generate and sendout a heartbeat packet.  */
static sctp_disposition_t sctp_sf_heartbeat(const struct sctp_endpoint *ep,
					    const struct sctp_association *asoc,
					    const sctp_subtype_t type,
					    void *arg,
					    sctp_cmd_seq_t *commands)
{
	struct sctp_transport *transport = (struct sctp_transport *) arg;
	struct sctp_chunk *reply;
	sctp_sender_hb_info_t hbinfo;
	size_t paylen = 0;

	hbinfo.param_hdr.type = SCTP_PARAM_HEARTBEAT_INFO;
	hbinfo.param_hdr.length = htons(sizeof(sctp_sender_hb_info_t));
	hbinfo.daddr = transport->ipaddr;
	hbinfo.sent_at = jiffies;
	hbinfo.hb_nonce = transport->hb_nonce;

	/* Send a heartbeat to our peer.  */
	paylen = sizeof(sctp_sender_hb_info_t);
	reply = sctp_make_heartbeat(asoc, transport, &hbinfo, paylen);
	if (!reply)
		return SCTP_DISPOSITION_NOMEM;

	/* Set rto_pending indicating that an RTT measurement
	 * is started with this heartbeat chunk.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_RTO_PENDING,
			SCTP_TRANSPORT(transport));

	sctp_add_cmd_sf(commands, SCTP_CMD_REPLY, SCTP_CHUNK(reply));
	return SCTP_DISPOSITION_CONSUME;
}

/* Generate a HEARTBEAT packet on the given transport.  */
sctp_disposition_t sctp_sf_sendbeat_8_3(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands)
{
	struct sctp_transport *transport = (struct sctp_transport *) arg;

	if (asoc->overall_error_count >= asoc->max_retrans) {
		sctp_add_cmd_sf(commands, SCTP_CMD_SET_SK_ERR,
				SCTP_ERROR(ETIMEDOUT));
		/* CMD_ASSOC_FAILED calls CMD_DELETE_TCB. */
		sctp_add_cmd_sf(commands, SCTP_CMD_ASSOC_FAILED,
				SCTP_PERR(SCTP_ERROR_NO_ERROR));
		SCTP_INC_STATS(SCTP_MIB_ABORTEDS);
		SCTP_DEC_STATS(SCTP_MIB_CURRESTAB);
		return SCTP_DISPOSITION_DELETE_TCB;
	}

	/* Section 3.3.5.
	 * The Sender-specific Heartbeat Info field should normally include
	 * information about the sender's current time when this HEARTBEAT
	 * chunk is sent and the destination transport address to which this
	 * HEARTBEAT is sent (see Section 8.3).
	 */

	if (transport->param_flags & SPP_HB_ENABLE) {
		if (SCTP_DISPOSITION_NOMEM ==
				sctp_sf_heartbeat(ep, asoc, type, arg,
						  commands))
			return SCTP_DISPOSITION_NOMEM;
		/* Set transport error counter and association error counter
		 * when sending heartbeat.
		 */
		sctp_add_cmd_sf(commands, SCTP_CMD_TRANSPORT_IDLE,
				SCTP_TRANSPORT(transport));
		sctp_add_cmd_sf(commands, SCTP_CMD_TRANSPORT_HB_SENT,
				SCTP_TRANSPORT(transport));
	}
	sctp_add_cmd_sf(commands, SCTP_CMD_HB_TIMER_UPDATE,
			SCTP_TRANSPORT(transport));

	return SCTP_DISPOSITION_CONSUME;
}

/*
 * Process an heartbeat request.
 *
 * Section: 8.3 Path Heartbeat
 * The receiver of the HEARTBEAT should immediately respond with a
 * HEARTBEAT ACK that contains the Heartbeat Information field copied
 * from the received HEARTBEAT chunk.
 *
 * Verification Tag:  8.5 Verification Tag [Normal verification]
 * When receiving an SCTP packet, the endpoint MUST ensure that the
 * value in the Verification Tag field of the received SCTP packet
 * matches its own Tag. If the received Verification Tag value does not
 * match the receiver's own tag value, the receiver shall silently
 * discard the packet and shall not process it any further except for
 * those cases listed in Section 8.5.1 below.
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_beat_8_3(const struct sctp_endpoint *ep,
				    const struct sctp_association *asoc,
				    const sctp_subtype_t type,
				    void *arg,
				    sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	struct sctp_chunk *reply;
	size_t paylen = 0;

	if (!sctp_vtag_verify(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* Make sure that the HEARTBEAT chunk has a valid length. */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_heartbeat_chunk_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	/* 8.3 The receiver of the HEARTBEAT should immediately
	 * respond with a HEARTBEAT ACK that contains the Heartbeat
	 * Information field copied from the received HEARTBEAT chunk.
	 */
	chunk->subh.hb_hdr = (sctp_heartbeathdr_t *) chunk->skb->data;
	paylen = ntohs(chunk->chunk_hdr->length) - sizeof(sctp_chunkhdr_t);
	if (!pskb_pull(chunk->skb, paylen))
		goto nomem;

	reply = sctp_make_heartbeat_ack(asoc, chunk,
					chunk->subh.hb_hdr, paylen);
	if (!reply)
		goto nomem;

	sctp_add_cmd_sf(commands, SCTP_CMD_REPLY, SCTP_CHUNK(reply));
	return SCTP_DISPOSITION_CONSUME;

nomem:
	return SCTP_DISPOSITION_NOMEM;
}

/*
 * Process the returning HEARTBEAT ACK.
 *
 * Section: 8.3 Path Heartbeat
 * Upon the receipt of the HEARTBEAT ACK, the sender of the HEARTBEAT
 * should clear the error counter of the destination transport
 * address to which the HEARTBEAT was sent, and mark the destination
 * transport address as active if it is not so marked. The endpoint may
 * optionally report to the upper layer when an inactive destination
 * address is marked as active due to the reception of the latest
 * HEARTBEAT ACK. The receiver of the HEARTBEAT ACK must also
 * clear the association overall error count as well (as defined
 * in section 8.1).
 *
 * The receiver of the HEARTBEAT ACK should also perform an RTT
 * measurement for that destination transport address using the time
 * value carried in the HEARTBEAT ACK chunk.
 *
 * Verification Tag:  8.5 Verification Tag [Normal verification]
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_backbeat_8_3(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	union sctp_addr from_addr;
	struct sctp_transport *link;
	sctp_sender_hb_info_t *hbinfo;
	unsigned long max_interval;

	if (!sctp_vtag_verify(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* Make sure that the HEARTBEAT-ACK chunk has a valid length.  */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_chunkhdr_t) +
					    sizeof(sctp_sender_hb_info_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	hbinfo = (sctp_sender_hb_info_t *) chunk->skb->data;
	/* Make sure that the length of the parameter is what we expect */
	if (ntohs(hbinfo->param_hdr.length) !=
				    sizeof(sctp_sender_hb_info_t)) {
		return SCTP_DISPOSITION_DISCARD;
	}

	from_addr = hbinfo->daddr;
	link = sctp_assoc_lookup_paddr(asoc, &from_addr);

	/* This should never happen, but lets log it if so.  */
	if (unlikely(!link)) {
		if (from_addr.sa.sa_family == AF_INET6) {
			if (net_ratelimit())
				printk(KERN_WARNING
				    "%s association %p could not find address %pI6\n",
				    __func__,
				    asoc,
				    &from_addr.v6.sin6_addr);
		} else {
			if (net_ratelimit())
				printk(KERN_WARNING
				    "%s association %p could not find address %pI4\n",
				    __func__,
				    asoc,
				    &from_addr.v4.sin_addr.s_addr);
		}
		return SCTP_DISPOSITION_DISCARD;
	}

	/* Validate the 64-bit random nonce. */
	if (hbinfo->hb_nonce != link->hb_nonce)
		return SCTP_DISPOSITION_DISCARD;

	max_interval = link->hbinterval + link->rto;

	/* Check if the timestamp looks valid.  */
	if (time_after(hbinfo->sent_at, jiffies) ||
	    time_after(jiffies, hbinfo->sent_at + max_interval)) {
		SCTP_DEBUG_PRINTK("%s: HEARTBEAT ACK with invalid timestamp "
				  "received for transport: %p\n",
				   __func__, link);
		return SCTP_DISPOSITION_DISCARD;
	}

	/* 8.3 Upon the receipt of the HEARTBEAT ACK, the sender of
	 * the HEARTBEAT should clear the error counter of the
	 * destination transport address to which the HEARTBEAT was
	 * sent and mark the destination transport address as active if
	 * it is not so marked.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_TRANSPORT_ON, SCTP_TRANSPORT(link));

	return SCTP_DISPOSITION_CONSUME;
}

/* Helper function to send out an abort for the restart
 * condition.
 */
static int sctp_sf_send_restart_abort(union sctp_addr *ssa,
				      struct sctp_chunk *init,
				      sctp_cmd_seq_t *commands)
{
	int len;
	struct sctp_packet *pkt;
	union sctp_addr_param *addrparm;
	struct sctp_errhdr *errhdr;
	struct sctp_endpoint *ep;
	char buffer[sizeof(struct sctp_errhdr)+sizeof(union sctp_addr_param)];
	struct sctp_af *af = sctp_get_af_specific(ssa->v4.sin_family);

	/* Build the error on the stack.   We are way to malloc crazy
	 * throughout the code today.
	 */
	errhdr = (struct sctp_errhdr *)buffer;
	addrparm = (union sctp_addr_param *)errhdr->variable;

	/* Copy into a parm format. */
	len = af->to_addr_param(ssa, addrparm);
	len += sizeof(sctp_errhdr_t);

	errhdr->cause = SCTP_ERROR_RESTART;
	errhdr->length = htons(len);

	/* Assign to the control socket. */
	ep = sctp_sk((sctp_get_ctl_sock()))->ep;

	/* Association is NULL since this may be a restart attack and we
	 * want to send back the attacker's vtag.
	 */
	pkt = sctp_abort_pkt_new(ep, NULL, init, errhdr, len);

	if (!pkt)
		goto out;
	sctp_add_cmd_sf(commands, SCTP_CMD_SEND_PKT, SCTP_PACKET(pkt));

	SCTP_INC_STATS(SCTP_MIB_OUTCTRLCHUNKS);

	/* Discard the rest of the inbound packet. */
	sctp_add_cmd_sf(commands, SCTP_CMD_DISCARD_PACKET, SCTP_NULL());

out:
	/* Even if there is no memory, treat as a failure so
	 * the packet will get dropped.
	 */
	return 0;
}

/* A restart is occurring, check to make sure no new addresses
 * are being added as we may be under a takeover attack.
 */
static int sctp_sf_check_restart_addrs(const struct sctp_association *new_asoc,
				       const struct sctp_association *asoc,
				       struct sctp_chunk *init,
				       sctp_cmd_seq_t *commands)
{
	struct sctp_transport *new_addr, *addr;
	int found;

	/* Implementor's Guide - Sectin 5.2.2
	 * ...
	 * Before responding the endpoint MUST check to see if the
	 * unexpected INIT adds new addresses to the association. If new
	 * addresses are added to the association, the endpoint MUST respond
	 * with an ABORT..
	 */

	/* Search through all current addresses and make sure
	 * we aren't adding any new ones.
	 */
	new_addr = NULL;
	found = 0;

	list_for_each_entry(new_addr, &new_asoc->peer.transport_addr_list,
			transports) {
		found = 0;
		list_for_each_entry(addr, &asoc->peer.transport_addr_list,
				transports) {
			if (sctp_cmp_addr_exact(&new_addr->ipaddr,
						&addr->ipaddr)) {
				found = 1;
				break;
			}
		}
		if (!found)
			break;
	}

	/* If a new address was added, ABORT the sender. */
	if (!found && new_addr) {
		sctp_sf_send_restart_abort(&new_addr->ipaddr, init, commands);
	}

	/* Return success if all addresses were found. */
	return found;
}

/* Populate the verification/tie tags based on overlapping INIT
 * scenario.
 *
 * Note: Do not use in CLOSED or SHUTDOWN-ACK-SENT state.
 */
static void sctp_tietags_populate(struct sctp_association *new_asoc,
				  const struct sctp_association *asoc)
{
	switch (asoc->state) {

	/* 5.2.1 INIT received in COOKIE-WAIT or COOKIE-ECHOED State */

	case SCTP_STATE_COOKIE_WAIT:
		new_asoc->c.my_vtag     = asoc->c.my_vtag;
		new_asoc->c.my_ttag     = asoc->c.my_vtag;
		new_asoc->c.peer_ttag   = 0;
		break;

	case SCTP_STATE_COOKIE_ECHOED:
		new_asoc->c.my_vtag     = asoc->c.my_vtag;
		new_asoc->c.my_ttag     = asoc->c.my_vtag;
		new_asoc->c.peer_ttag   = asoc->c.peer_vtag;
		break;

	/* 5.2.2 Unexpected INIT in States Other than CLOSED, COOKIE-ECHOED,
	 * COOKIE-WAIT and SHUTDOWN-ACK-SENT
	 */
	default:
		new_asoc->c.my_ttag   = asoc->c.my_vtag;
		new_asoc->c.peer_ttag = asoc->c.peer_vtag;
		break;
	}

	/* Other parameters for the endpoint SHOULD be copied from the
	 * existing parameters of the association (e.g. number of
	 * outbound streams) into the INIT ACK and cookie.
	 */
	new_asoc->rwnd                  = asoc->rwnd;
	new_asoc->c.sinit_num_ostreams  = asoc->c.sinit_num_ostreams;
	new_asoc->c.sinit_max_instreams = asoc->c.sinit_max_instreams;
	new_asoc->c.initial_tsn         = asoc->c.initial_tsn;
}

/*
 * Compare vtag/tietag values to determine unexpected COOKIE-ECHO
 * handling action.
 *
 * RFC 2960 5.2.4 Handle a COOKIE ECHO when a TCB exists.
 *
 * Returns value representing action to be taken.   These action values
 * correspond to Action/Description values in RFC 2960, Table 2.
 */
static char sctp_tietags_compare(struct sctp_association *new_asoc,
				 const struct sctp_association *asoc)
{
	/* In this case, the peer may have restarted.  */
	if ((asoc->c.my_vtag != new_asoc->c.my_vtag) &&
	    (asoc->c.peer_vtag != new_asoc->c.peer_vtag) &&
	    (asoc->c.my_vtag == new_asoc->c.my_ttag) &&
	    (asoc->c.peer_vtag == new_asoc->c.peer_ttag))
		return 'A';

	/* Collision case B. */
	if ((asoc->c.my_vtag == new_asoc->c.my_vtag) &&
	    ((asoc->c.peer_vtag != new_asoc->c.peer_vtag) ||
	     (0 == asoc->c.peer_vtag))) {
		return 'B';
	}

	/* Collision case D. */
	if ((asoc->c.my_vtag == new_asoc->c.my_vtag) &&
	    (asoc->c.peer_vtag == new_asoc->c.peer_vtag))
		return 'D';

	/* Collision case C. */
	if ((asoc->c.my_vtag != new_asoc->c.my_vtag) &&
	    (asoc->c.peer_vtag == new_asoc->c.peer_vtag) &&
	    (0 == new_asoc->c.my_ttag) &&
	    (0 == new_asoc->c.peer_ttag))
		return 'C';

	/* No match to any of the special cases; discard this packet. */
	return 'E';
}

/* Common helper routine for both duplicate and simulataneous INIT
 * chunk handling.
 */
static sctp_disposition_t sctp_sf_do_unexpected_init(
	const struct sctp_endpoint *ep,
	const struct sctp_association *asoc,
	const sctp_subtype_t type,
	void *arg, sctp_cmd_seq_t *commands)
{
	sctp_disposition_t retval;
	struct sctp_chunk *chunk = arg;
	struct sctp_chunk *repl;
	struct sctp_association *new_asoc;
	struct sctp_chunk *err_chunk;
	struct sctp_packet *packet;
	sctp_unrecognized_param_t *unk_param;
	int len;

	/* 6.10 Bundling
	 * An endpoint MUST NOT bundle INIT, INIT ACK or
	 * SHUTDOWN COMPLETE with any other chunks.
	 *
	 * IG Section 2.11.2
	 * Furthermore, we require that the receiver of an INIT chunk MUST
	 * enforce these rules by silently discarding an arriving packet
	 * with an INIT chunk that is bundled with other chunks.
	 */
	if (!chunk->singleton)
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* 3.1 A packet containing an INIT chunk MUST have a zero Verification
	 * Tag.
	 */
	if (chunk->sctp_hdr->vtag != 0)
		return sctp_sf_tabort_8_4_8(ep, asoc, type, arg, commands);

	/* Make sure that the INIT chunk has a valid length.
	 * In this case, we generate a protocol violation since we have
	 * an association established.
	 */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_init_chunk_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);
	/* Grab the INIT header.  */
	chunk->subh.init_hdr = (sctp_inithdr_t *) chunk->skb->data;

	/* Tag the variable length parameters.  */
	chunk->param_hdr.v = skb_pull(chunk->skb, sizeof(sctp_inithdr_t));

	/* Verify the INIT chunk before processing it. */
	err_chunk = NULL;
	if (!sctp_verify_init(asoc, chunk->chunk_hdr->type,
			      (sctp_init_chunk_t *)chunk->chunk_hdr, chunk,
			      &err_chunk)) {
		/* This chunk contains fatal error. It is to be discarded.
		 * Send an ABORT, with causes if there is any.
		 */
		if (err_chunk) {
			packet = sctp_abort_pkt_new(ep, asoc, arg,
					(__u8 *)(err_chunk->chunk_hdr) +
					sizeof(sctp_chunkhdr_t),
					ntohs(err_chunk->chunk_hdr->length) -
					sizeof(sctp_chunkhdr_t));

			if (packet) {
				sctp_add_cmd_sf(commands, SCTP_CMD_SEND_PKT,
						SCTP_PACKET(packet));
				SCTP_INC_STATS(SCTP_MIB_OUTCTRLCHUNKS);
				retval = SCTP_DISPOSITION_CONSUME;
			} else {
				retval = SCTP_DISPOSITION_NOMEM;
			}
			goto cleanup;
		} else {
			return sctp_sf_tabort_8_4_8(ep, asoc, type, arg,
						    commands);
		}
	}

	/*
	 * Other parameters for the endpoint SHOULD be copied from the
	 * existing parameters of the association (e.g. number of
	 * outbound streams) into the INIT ACK and cookie.
	 * FIXME:  We are copying parameters from the endpoint not the
	 * association.
	 */
	new_asoc = sctp_make_temp_asoc(ep, chunk, GFP_ATOMIC);
	if (!new_asoc)
		goto nomem;

	if (sctp_assoc_set_bind_addr_from_ep(new_asoc,
				sctp_scope(sctp_source(chunk)), GFP_ATOMIC) < 0)
		goto nomem;

	/* In the outbound INIT ACK the endpoint MUST copy its current
	 * Verification Tag and Peers Verification tag into a reserved
	 * place (local tie-tag and per tie-tag) within the state cookie.
	 */
	if (!sctp_process_init(new_asoc, chunk->chunk_hdr->type,
			       sctp_source(chunk),
			       (sctp_init_chunk_t *)chunk->chunk_hdr,
			       GFP_ATOMIC))
		goto nomem;

	/* Make sure no new addresses are being added during the
	 * restart.   Do not do this check for COOKIE-WAIT state,
	 * since there are no peer addresses to check against.
	 * Upon return an ABORT will have been sent if needed.
	 */
	if (!sctp_state(asoc, COOKIE_WAIT)) {
		if (!sctp_sf_check_restart_addrs(new_asoc, asoc, chunk,
						 commands)) {
			retval = SCTP_DISPOSITION_CONSUME;
			goto nomem_retval;
		}
	}

	sctp_tietags_populate(new_asoc, asoc);

	/* B) "Z" shall respond immediately with an INIT ACK chunk.  */

	/* If there are errors need to be reported for unknown parameters,
	 * make sure to reserve enough room in the INIT ACK for them.
	 */
	len = 0;
	if (err_chunk) {
		len = ntohs(err_chunk->chunk_hdr->length) -
			sizeof(sctp_chunkhdr_t);
	}

	repl = sctp_make_init_ack(new_asoc, chunk, GFP_ATOMIC, len);
	if (!repl)
		goto nomem;

	/* If there are errors need to be reported for unknown parameters,
	 * include them in the outgoing INIT ACK as "Unrecognized parameter"
	 * parameter.
	 */
	if (err_chunk) {
		/* Get the "Unrecognized parameter" parameter(s) out of the
		 * ERROR chunk generated by sctp_verify_init(). Since the
		 * error cause code for "unknown parameter" and the
		 * "Unrecognized parameter" type is the same, we can
		 * construct the parameters in INIT ACK by copying the
		 * ERROR causes over.
		 */
		unk_param = (sctp_unrecognized_param_t *)
			    ((__u8 *)(err_chunk->chunk_hdr) +
			    sizeof(sctp_chunkhdr_t));
		/* Replace the cause code with the "Unrecognized parameter"
		 * parameter type.
		 */
		sctp_addto_chunk(repl, len, unk_param);
	}

	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_ASOC, SCTP_ASOC(new_asoc));
	sctp_add_cmd_sf(commands, SCTP_CMD_REPLY, SCTP_CHUNK(repl));

	/*
	 * Note: After sending out INIT ACK with the State Cookie parameter,
	 * "Z" MUST NOT allocate any resources for this new association.
	 * Otherwise, "Z" will be vulnerable to resource attacks.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_DELETE_TCB, SCTP_NULL());
	retval = SCTP_DISPOSITION_CONSUME;

	return retval;

nomem:
	retval = SCTP_DISPOSITION_NOMEM;
nomem_retval:
	if (new_asoc)
		sctp_association_free(new_asoc);
cleanup:
	if (err_chunk)
		sctp_chunk_free(err_chunk);
	return retval;
}

/*
 * Handle simultanous INIT.
 * This means we started an INIT and then we got an INIT request from
 * our peer.
 *
 * Section: 5.2.1 INIT received in COOKIE-WAIT or COOKIE-ECHOED State (Item B)
 * This usually indicates an initialization collision, i.e., each
 * endpoint is attempting, at about the same time, to establish an
 * association with the other endpoint.
 *
 * Upon receipt of an INIT in the COOKIE-WAIT or COOKIE-ECHOED state, an
 * endpoint MUST respond with an INIT ACK using the same parameters it
 * sent in its original INIT chunk (including its Verification Tag,
 * unchanged). These original parameters are combined with those from the
 * newly received INIT chunk. The endpoint shall also generate a State
 * Cookie with the INIT ACK. The endpoint uses the parameters sent in its
 * INIT to calculate the State Cookie.
 *
 * After that, the endpoint MUST NOT change its state, the T1-init
 * timer shall be left running and the corresponding TCB MUST NOT be
 * destroyed. The normal procedures for handling State Cookies when
 * a TCB exists will resolve the duplicate INITs to a single association.
 *
 * For an endpoint that is in the COOKIE-ECHOED state it MUST populate
 * its Tie-Tags with the Tag information of itself and its peer (see
 * section 5.2.2 for a description of the Tie-Tags).
 *
 * Verification Tag: Not explicit, but an INIT can not have a valid
 * verification tag, so we skip the check.
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_5_2_1_siminit(const struct sctp_endpoint *ep,
				    const struct sctp_association *asoc,
				    const sctp_subtype_t type,
				    void *arg,
				    sctp_cmd_seq_t *commands)
{
	/* Call helper to do the real work for both simulataneous and
	 * duplicate INIT chunk handling.
	 */
	return sctp_sf_do_unexpected_init(ep, asoc, type, arg, commands);
}

/*
 * Handle duplicated INIT messages.  These are usually delayed
 * restransmissions.
 *
 * Section: 5.2.2 Unexpected INIT in States Other than CLOSED,
 * COOKIE-ECHOED and COOKIE-WAIT
 *
 * Unless otherwise stated, upon reception of an unexpected INIT for
 * this association, the endpoint shall generate an INIT ACK with a
 * State Cookie.  In the outbound INIT ACK the endpoint MUST copy its
 * current Verification Tag and peer's Verification Tag into a reserved
 * place within the state cookie.  We shall refer to these locations as
 * the Peer's-Tie-Tag and the Local-Tie-Tag.  The outbound SCTP packet
 * containing this INIT ACK MUST carry a Verification Tag value equal to
 * the Initiation Tag found in the unexpected INIT.  And the INIT ACK
 * MUST contain a new Initiation Tag (randomly generated see Section
 * 5.3.1).  Other parameters for the endpoint SHOULD be copied from the
 * existing parameters of the association (e.g. number of outbound
 * streams) into the INIT ACK and cookie.
 *
 * After sending out the INIT ACK, the endpoint shall take no further
 * actions, i.e., the existing association, including its current state,
 * and the corresponding TCB MUST NOT be changed.
 *
 * Note: Only when a TCB exists and the association is not in a COOKIE-
 * WAIT state are the Tie-Tags populated.  For a normal association INIT
 * (i.e. the endpoint is in a COOKIE-WAIT state), the Tie-Tags MUST be
 * set to 0 (indicating that no previous TCB existed).  The INIT ACK and
 * State Cookie are populated as specified in section 5.2.1.
 *
 * Verification Tag: Not specified, but an INIT has no way of knowing
 * what the verification tag could be, so we ignore it.
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_5_2_2_dupinit(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands)
{
	/* Call helper to do the real work for both simulataneous and
	 * duplicate INIT chunk handling.
	 */
	return sctp_sf_do_unexpected_init(ep, asoc, type, arg, commands);
}


/*
 * Unexpected INIT-ACK handler.
 *
 * Section 5.2.3
 * If an INIT ACK received by an endpoint in any state other than the
 * COOKIE-WAIT state, the endpoint should discard the INIT ACK chunk.
 * An unexpected INIT ACK usually indicates the processing of an old or
 * duplicated INIT chunk.
*/
sctp_disposition_t sctp_sf_do_5_2_3_initack(const struct sctp_endpoint *ep,
					    const struct sctp_association *asoc,
					    const sctp_subtype_t type,
					    void *arg, sctp_cmd_seq_t *commands)
{
	/* Per the above section, we'll discard the chunk if we have an
	 * endpoint.  If this is an OOTB INIT-ACK, treat it as such.
	 */
	if (ep == sctp_sk((sctp_get_ctl_sock()))->ep)
		return sctp_sf_ootb(ep, asoc, type, arg, commands);
	else
		return sctp_sf_discard_chunk(ep, asoc, type, arg, commands);
}

/* Unexpected COOKIE-ECHO handler for peer restart (Table 2, action 'A')
 *
 * Section 5.2.4
 *  A)  In this case, the peer may have restarted.
 */
static sctp_disposition_t sctp_sf_do_dupcook_a(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					struct sctp_chunk *chunk,
					sctp_cmd_seq_t *commands,
					struct sctp_association *new_asoc)
{
	sctp_init_chunk_t *peer_init;
	struct sctp_ulpevent *ev;
	struct sctp_chunk *repl;
	struct sctp_chunk *err;
	sctp_disposition_t disposition;

	/* new_asoc is a brand-new association, so these are not yet
	 * side effects--it is safe to run them here.
	 */
	peer_init = &chunk->subh.cookie_hdr->c.peer_init[0];

	if (!sctp_process_init(new_asoc, chunk->chunk_hdr->type,
			       sctp_source(chunk), peer_init,
			       GFP_ATOMIC))
		goto nomem;

	/* Make sure no new addresses are being added during the
	 * restart.  Though this is a pretty complicated attack
	 * since you'd have to get inside the cookie.
	 */
	if (!sctp_sf_check_restart_addrs(new_asoc, asoc, chunk, commands)) {
		return SCTP_DISPOSITION_CONSUME;
	}

	/* If the endpoint is in the SHUTDOWN-ACK-SENT state and recognizes
	 * the peer has restarted (Action A), it MUST NOT setup a new
	 * association but instead resend the SHUTDOWN ACK and send an ERROR
	 * chunk with a "Cookie Received while Shutting Down" error cause to
	 * its peer.
	*/
	if (sctp_state(asoc, SHUTDOWN_ACK_SENT)) {
		disposition = sctp_sf_do_9_2_reshutack(ep, asoc,
				SCTP_ST_CHUNK(chunk->chunk_hdr->type),
				chunk, commands);
		if (SCTP_DISPOSITION_NOMEM == disposition)
			goto nomem;

		err = sctp_make_op_error(asoc, chunk,
					 SCTP_ERROR_COOKIE_IN_SHUTDOWN,
					 NULL, 0);
		if (err)
			sctp_add_cmd_sf(commands, SCTP_CMD_REPLY,
					SCTP_CHUNK(err));

		return SCTP_DISPOSITION_CONSUME;
	}

	/* For now, fail any unsent/unacked data.  Consider the optional
	 * choice of resending of this data.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_PURGE_OUTQUEUE, SCTP_NULL());

	repl = sctp_make_cookie_ack(new_asoc, chunk);
	if (!repl)
		goto nomem;

	/* Report association restart to upper layer. */
	ev = sctp_ulpevent_make_assoc_change(asoc, 0, SCTP_RESTART, 0,
					     new_asoc->c.sinit_num_ostreams,
					     new_asoc->c.sinit_max_instreams,
					     NULL, GFP_ATOMIC);
	if (!ev)
		goto nomem_ev;

	/* Update the content of current association. */
	sctp_add_cmd_sf(commands, SCTP_CMD_UPDATE_ASSOC, SCTP_ASOC(new_asoc));
	sctp_add_cmd_sf(commands, SCTP_CMD_REPLY, SCTP_CHUNK(repl));
	sctp_add_cmd_sf(commands, SCTP_CMD_EVENT_ULP, SCTP_ULPEVENT(ev));
	return SCTP_DISPOSITION_CONSUME;

nomem_ev:
	sctp_chunk_free(repl);
nomem:
	return SCTP_DISPOSITION_NOMEM;
}

/* Unexpected COOKIE-ECHO handler for setup collision (Table 2, action 'B')
 *
 * Section 5.2.4
 *   B) In this case, both sides may be attempting to start an association
 *      at about the same time but the peer endpoint started its INIT
 *      after responding to the local endpoint's INIT
 */
/* This case represents an initialization collision.  */
static sctp_disposition_t sctp_sf_do_dupcook_b(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					struct sctp_chunk *chunk,
					sctp_cmd_seq_t *commands,
					struct sctp_association *new_asoc)
{
	sctp_init_chunk_t *peer_init;
	struct sctp_chunk *repl;

	/* new_asoc is a brand-new association, so these are not yet
	 * side effects--it is safe to run them here.
	 */
	peer_init = &chunk->subh.cookie_hdr->c.peer_init[0];
	if (!sctp_process_init(new_asoc, chunk->chunk_hdr->type,
			       sctp_source(chunk), peer_init,
			       GFP_ATOMIC))
		goto nomem;

	/* Update the content of current association.  */
	sctp_add_cmd_sf(commands, SCTP_CMD_UPDATE_ASSOC, SCTP_ASOC(new_asoc));
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_ESTABLISHED));
	SCTP_INC_STATS(SCTP_MIB_CURRESTAB);
	sctp_add_cmd_sf(commands, SCTP_CMD_HB_TIMERS_START, SCTP_NULL());

	repl = sctp_make_cookie_ack(new_asoc, chunk);
	if (!repl)
		goto nomem;

	sctp_add_cmd_sf(commands, SCTP_CMD_REPLY, SCTP_CHUNK(repl));

	/* RFC 2960 5.1 Normal Establishment of an Association
	 *
	 * D) IMPLEMENTATION NOTE: An implementation may choose to
	 * send the Communication Up notification to the SCTP user
	 * upon reception of a valid COOKIE ECHO chunk.
	 *
	 * Sadly, this needs to be implemented as a side-effect, because
	 * we are not guaranteed to have set the association id of the real
	 * association and so these notifications need to be delayed until
	 * the association id is allocated.
	 */

	sctp_add_cmd_sf(commands, SCTP_CMD_ASSOC_CHANGE, SCTP_U8(SCTP_COMM_UP));

	/* Sockets API Draft Section 5.3.1.6
	 * When a peer sends a Adaptation Layer Indication parameter , SCTP
	 * delivers this notification to inform the application that of the
	 * peers requested adaptation layer.
	 *
	 * This also needs to be done as a side effect for the same reason as
	 * above.
	 */
	if (asoc->peer.adaptation_ind)
		sctp_add_cmd_sf(commands, SCTP_CMD_ADAPTATION_IND, SCTP_NULL());

	return SCTP_DISPOSITION_CONSUME;

nomem:
	return SCTP_DISPOSITION_NOMEM;
}

/* Unexpected COOKIE-ECHO handler for setup collision (Table 2, action 'C')
 *
 * Section 5.2.4
 *  C) In this case, the local endpoint's cookie has arrived late.
 *     Before it arrived, the local endpoint sent an INIT and received an
 *     INIT-ACK and finally sent a COOKIE ECHO with the peer's same tag
 *     but a new tag of its own.
 */
/* This case represents an initialization collision.  */
static sctp_disposition_t sctp_sf_do_dupcook_c(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					struct sctp_chunk *chunk,
					sctp_cmd_seq_t *commands,
					struct sctp_association *new_asoc)
{
	/* The cookie should be silently discarded.
	 * The endpoint SHOULD NOT change states and should leave
	 * any timers running.
	 */
	return SCTP_DISPOSITION_DISCARD;
}

/* Unexpected COOKIE-ECHO handler lost chunk (Table 2, action 'D')
 *
 * Section 5.2.4
 *
 * D) When both local and remote tags match the endpoint should always
 *    enter the ESTABLISHED state, if it has not already done so.
 */
/* This case represents an initialization collision.  */
static sctp_disposition_t sctp_sf_do_dupcook_d(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					struct sctp_chunk *chunk,
					sctp_cmd_seq_t *commands,
					struct sctp_association *new_asoc)
{
	struct sctp_ulpevent *ev = NULL, *ai_ev = NULL;
	struct sctp_chunk *repl;

	/* Clarification from Implementor's Guide:
	 * D) When both local and remote tags match the endpoint should
	 * enter the ESTABLISHED state, if it is in the COOKIE-ECHOED state.
	 * It should stop any cookie timer that may be running and send
	 * a COOKIE ACK.
	 */

	/* Don't accidentally move back into established state. */
	if (asoc->state < SCTP_STATE_ESTABLISHED) {
		sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
				SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIE));
		sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
				SCTP_STATE(SCTP_STATE_ESTABLISHED));
		SCTP_INC_STATS(SCTP_MIB_CURRESTAB);
		sctp_add_cmd_sf(commands, SCTP_CMD_HB_TIMERS_START,
				SCTP_NULL());

		/* RFC 2960 5.1 Normal Establishment of an Association
		 *
		 * D) IMPLEMENTATION NOTE: An implementation may choose
		 * to send the Communication Up notification to the
		 * SCTP user upon reception of a valid COOKIE
		 * ECHO chunk.
		 */
		ev = sctp_ulpevent_make_assoc_change(asoc, 0,
					     SCTP_COMM_UP, 0,
					     asoc->c.sinit_num_ostreams,
					     asoc->c.sinit_max_instreams,
					     NULL, GFP_ATOMIC);
		if (!ev)
			goto nomem;

		/* Sockets API Draft Section 5.3.1.6
		 * When a peer sends a Adaptation Layer Indication parameter,
		 * SCTP delivers this notification to inform the application
		 * that of the peers requested adaptation layer.
		 */
		if (asoc->peer.adaptation_ind) {
			ai_ev = sctp_ulpevent_make_adaptation_indication(asoc,
								 GFP_ATOMIC);
			if (!ai_ev)
				goto nomem;

		}
	}

	repl = sctp_make_cookie_ack(new_asoc, chunk);
	if (!repl)
		goto nomem;

	sctp_add_cmd_sf(commands, SCTP_CMD_REPLY, SCTP_CHUNK(repl));

	if (ev)
		sctp_add_cmd_sf(commands, SCTP_CMD_EVENT_ULP,
				SCTP_ULPEVENT(ev));
	if (ai_ev)
		sctp_add_cmd_sf(commands, SCTP_CMD_EVENT_ULP,
					SCTP_ULPEVENT(ai_ev));

	return SCTP_DISPOSITION_CONSUME;

nomem:
	if (ai_ev)
		sctp_ulpevent_free(ai_ev);
	if (ev)
		sctp_ulpevent_free(ev);
	return SCTP_DISPOSITION_NOMEM;
}

/*
 * Handle a duplicate COOKIE-ECHO.  This usually means a cookie-carrying
 * chunk was retransmitted and then delayed in the network.
 *
 * Section: 5.2.4 Handle a COOKIE ECHO when a TCB exists
 *
 * Verification Tag: None.  Do cookie validation.
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_5_2_4_dupcook(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands)
{
	sctp_disposition_t retval;
	struct sctp_chunk *chunk = arg;
	struct sctp_association *new_asoc;
	int error = 0;
	char action;
	struct sctp_chunk *err_chk_p;

	/* Make sure that the chunk has a valid length from the protocol
	 * perspective.  In this case check to make sure we have at least
	 * enough for the chunk header.  Cookie length verification is
	 * done later.
	 */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_chunkhdr_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	/* "Decode" the chunk.  We have no optional parameters so we
	 * are in good shape.
	 */
	chunk->subh.cookie_hdr = (struct sctp_signed_cookie *)chunk->skb->data;
	if (!pskb_pull(chunk->skb, ntohs(chunk->chunk_hdr->length) -
					sizeof(sctp_chunkhdr_t)))
		goto nomem;

	/* In RFC 2960 5.2.4 3, if both Verification Tags in the State Cookie
	 * of a duplicate COOKIE ECHO match the Verification Tags of the
	 * current association, consider the State Cookie valid even if
	 * the lifespan is exceeded.
	 */
	new_asoc = sctp_unpack_cookie(ep, asoc, chunk, GFP_ATOMIC, &error,
				      &err_chk_p);

	/* FIXME:
	 * If the re-build failed, what is the proper error path
	 * from here?
	 *
	 * [We should abort the association. --piggy]
	 */
	if (!new_asoc) {
		/* FIXME: Several errors are possible.  A bad cookie should
		 * be silently discarded, but think about logging it too.
		 */
		switch (error) {
		case -SCTP_IERROR_NOMEM:
			goto nomem;

		case -SCTP_IERROR_STALE_COOKIE:
			sctp_send_stale_cookie_err(ep, asoc, chunk, commands,
						   err_chk_p);
			return sctp_sf_pdiscard(ep, asoc, type, arg, commands);
		case -SCTP_IERROR_BAD_SIG:
		default:
			return sctp_sf_pdiscard(ep, asoc, type, arg, commands);
		}
	}

	/* Compare the tie_tag in cookie with the verification tag of
	 * current association.
	 */
	action = sctp_tietags_compare(new_asoc, asoc);

	switch (action) {
	case 'A': /* Association restart. */
		retval = sctp_sf_do_dupcook_a(ep, asoc, chunk, commands,
					      new_asoc);
		break;

	case 'B': /* Collision case B. */
		retval = sctp_sf_do_dupcook_b(ep, asoc, chunk, commands,
					      new_asoc);
		break;

	case 'C': /* Collision case C. */
		retval = sctp_sf_do_dupcook_c(ep, asoc, chunk, commands,
					      new_asoc);
		break;

	case 'D': /* Collision case D. */
		retval = sctp_sf_do_dupcook_d(ep, asoc, chunk, commands,
					      new_asoc);
		break;

	default: /* Discard packet for all others. */
		retval = sctp_sf_pdiscard(ep, asoc, type, arg, commands);
		break;
	}

	/* Delete the tempory new association. */
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_ASOC, SCTP_ASOC(new_asoc));
	sctp_add_cmd_sf(commands, SCTP_CMD_DELETE_TCB, SCTP_NULL());

	return retval;

nomem:
	return SCTP_DISPOSITION_NOMEM;
}

/*
 * Process an ABORT.  (SHUTDOWN-PENDING state)
 *
 * See sctp_sf_do_9_1_abort().
 */
sctp_disposition_t sctp_sf_shutdown_pending_abort(
	const struct sctp_endpoint *ep,
	const struct sctp_association *asoc,
	const sctp_subtype_t type,
	void *arg,
	sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;

	if (!sctp_vtag_verify_either(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* Make sure that the ABORT chunk has a valid length.
	 * Since this is an ABORT chunk, we have to discard it
	 * because of the following text:
	 * RFC 2960, Section 3.3.7
	 *    If an endpoint receives an ABORT with a format error or for an
	 *    association that doesn't exist, it MUST silently discard it.
	 * Becasue the length is "invalid", we can't really discard just
	 * as we do not know its true length.  So, to be safe, discard the
	 * packet.
	 */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_abort_chunk_t)))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* ADD-IP: Special case for ABORT chunks
	 * F4)  One special consideration is that ABORT Chunks arriving
	 * destined to the IP address being deleted MUST be
	 * ignored (see Section 5.3.1 for further details).
	 */
	if (SCTP_ADDR_DEL ==
		    sctp_bind_addr_state(&asoc->base.bind_addr, &chunk->dest))
		return sctp_sf_discard_chunk(ep, asoc, type, arg, commands);

	return __sctp_sf_do_9_1_abort(ep, asoc, type, arg, commands);
}

/*
 * Process an ABORT.  (SHUTDOWN-SENT state)
 *
 * See sctp_sf_do_9_1_abort().
 */
sctp_disposition_t sctp_sf_shutdown_sent_abort(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;

	if (!sctp_vtag_verify_either(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* Make sure that the ABORT chunk has a valid length.
	 * Since this is an ABORT chunk, we have to discard it
	 * because of the following text:
	 * RFC 2960, Section 3.3.7
	 *    If an endpoint receives an ABORT with a format error or for an
	 *    association that doesn't exist, it MUST silently discard it.
	 * Becasue the length is "invalid", we can't really discard just
	 * as we do not know its true length.  So, to be safe, discard the
	 * packet.
	 */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_abort_chunk_t)))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* ADD-IP: Special case for ABORT chunks
	 * F4)  One special consideration is that ABORT Chunks arriving
	 * destined to the IP address being deleted MUST be
	 * ignored (see Section 5.3.1 for further details).
	 */
	if (SCTP_ADDR_DEL ==
		    sctp_bind_addr_state(&asoc->base.bind_addr, &chunk->dest))
		return sctp_sf_discard_chunk(ep, asoc, type, arg, commands);

	/* Stop the T2-shutdown timer. */
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T2_SHUTDOWN));

	/* Stop the T5-shutdown guard timer.  */
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T5_SHUTDOWN_GUARD));

	return __sctp_sf_do_9_1_abort(ep, asoc, type, arg, commands);
}

/*
 * Process an ABORT.  (SHUTDOWN-ACK-SENT state)
 *
 * See sctp_sf_do_9_1_abort().
 */
sctp_disposition_t sctp_sf_shutdown_ack_sent_abort(
	const struct sctp_endpoint *ep,
	const struct sctp_association *asoc,
	const sctp_subtype_t type,
	void *arg,
	sctp_cmd_seq_t *commands)
{
	/* The same T2 timer, so we should be able to use
	 * common function with the SHUTDOWN-SENT state.
	 */
	return sctp_sf_shutdown_sent_abort(ep, asoc, type, arg, commands);
}

/*
 * Handle an Error received in COOKIE_ECHOED state.
 *
 * Only handle the error type of stale COOKIE Error, the other errors will
 * be ignored.
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_cookie_echoed_err(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	sctp_errhdr_t *err;

	if (!sctp_vtag_verify(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* Make sure that the ERROR chunk has a valid length.
	 * The parameter walking depends on this as well.
	 */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_operr_chunk_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	/* Process the error here */
	/* FUTURE FIXME:  When PR-SCTP related and other optional
	 * parms are emitted, this will have to change to handle multiple
	 * errors.
	 */
	sctp_walk_errors(err, chunk->chunk_hdr) {
		if (SCTP_ERROR_STALE_COOKIE == err->cause)
			return sctp_sf_do_5_2_6_stale(ep, asoc, type,
							arg, commands);
	}

	/* It is possible to have malformed error causes, and that
	 * will cause us to end the walk early.  However, since
	 * we are discarding the packet, there should be no adverse
	 * affects.
	 */
	return sctp_sf_pdiscard(ep, asoc, type, arg, commands);
}

/*
 * Handle a Stale COOKIE Error
 *
 * Section: 5.2.6 Handle Stale COOKIE Error
 * If the association is in the COOKIE-ECHOED state, the endpoint may elect
 * one of the following three alternatives.
 * ...
 * 3) Send a new INIT chunk to the endpoint, adding a Cookie
 *    Preservative parameter requesting an extension to the lifetime of
 *    the State Cookie. When calculating the time extension, an
 *    implementation SHOULD use the RTT information measured based on the
 *    previous COOKIE ECHO / ERROR exchange, and should add no more
 *    than 1 second beyond the measured RTT, due to long State Cookie
 *    lifetimes making the endpoint more subject to a replay attack.
 *
 * Verification Tag:  Not explicit, but safe to ignore.
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
static sctp_disposition_t sctp_sf_do_5_2_6_stale(const struct sctp_endpoint *ep,
						 const struct sctp_association *asoc,
						 const sctp_subtype_t type,
						 void *arg,
						 sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	time_t stale;
	sctp_cookie_preserve_param_t bht;
	sctp_errhdr_t *err;
	struct sctp_chunk *reply;
	struct sctp_bind_addr *bp;
	int attempts = asoc->init_err_counter + 1;

	if (attempts > asoc->max_init_attempts) {
		sctp_add_cmd_sf(commands, SCTP_CMD_SET_SK_ERR,
				SCTP_ERROR(ETIMEDOUT));
		sctp_add_cmd_sf(commands, SCTP_CMD_INIT_FAILED,
				SCTP_PERR(SCTP_ERROR_STALE_COOKIE));
		return SCTP_DISPOSITION_DELETE_TCB;
	}

	err = (sctp_errhdr_t *)(chunk->skb->data);

	/* When calculating the time extension, an implementation
	 * SHOULD use the RTT information measured based on the
	 * previous COOKIE ECHO / ERROR exchange, and should add no
	 * more than 1 second beyond the measured RTT, due to long
	 * State Cookie lifetimes making the endpoint more subject to
	 * a replay attack.
	 * Measure of Staleness's unit is usec. (1/1000000 sec)
	 * Suggested Cookie Life-span Increment's unit is msec.
	 * (1/1000 sec)
	 * In general, if you use the suggested cookie life, the value
	 * found in the field of measure of staleness should be doubled
	 * to give ample time to retransmit the new cookie and thus
	 * yield a higher probability of success on the reattempt.
	 */
	stale = ntohl(*(__be32 *)((u8 *)err + sizeof(sctp_errhdr_t)));
	stale = (stale * 2) / 1000;

	bht.param_hdr.type = SCTP_PARAM_COOKIE_PRESERVATIVE;
	bht.param_hdr.length = htons(sizeof(bht));
	bht.lifespan_increment = htonl(stale);

	/* Build that new INIT chunk.  */
	bp = (struct sctp_bind_addr *) &asoc->base.bind_addr;
	reply = sctp_make_init(asoc, bp, GFP_ATOMIC, sizeof(bht));
	if (!reply)
		goto nomem;

	sctp_addto_chunk(reply, sizeof(bht), &bht);

	/* Clear peer's init_tag cached in assoc as we are sending a new INIT */
	sctp_add_cmd_sf(commands, SCTP_CMD_CLEAR_INIT_TAG, SCTP_NULL());

	/* Stop pending T3-rtx and heartbeat timers */
	sctp_add_cmd_sf(commands, SCTP_CMD_T3_RTX_TIMERS_STOP, SCTP_NULL());
	sctp_add_cmd_sf(commands, SCTP_CMD_HB_TIMERS_STOP, SCTP_NULL());

	/* Delete non-primary peer ip addresses since we are transitioning
	 * back to the COOKIE-WAIT state
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_DEL_NON_PRIMARY, SCTP_NULL());

	/* If we've sent any data bundled with COOKIE-ECHO we will need to
	 * resend
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_T1_RETRAN,
			SCTP_TRANSPORT(asoc->peer.primary_path));

	/* Cast away the const modifier, as we want to just
	 * rerun it through as a sideffect.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_INIT_COUNTER_INC, SCTP_NULL());

	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIE));
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_COOKIE_WAIT));
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_START,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_INIT));

	sctp_add_cmd_sf(commands, SCTP_CMD_REPLY, SCTP_CHUNK(reply));

	return SCTP_DISPOSITION_CONSUME;

nomem:
	return SCTP_DISPOSITION_NOMEM;
}

/*
 * Process an ABORT.
 *
 * Section: 9.1
 * After checking the Verification Tag, the receiving endpoint shall
 * remove the association from its record, and shall report the
 * termination to its upper layer.
 *
 * Verification Tag: 8.5.1 Exceptions in Verification Tag Rules
 * B) Rules for packet carrying ABORT:
 *
 *  - The endpoint shall always fill in the Verification Tag field of the
 *    outbound packet with the destination endpoint's tag value if it
 *    is known.
 *
 *  - If the ABORT is sent in response to an OOTB packet, the endpoint
 *    MUST follow the procedure described in Section 8.4.
 *
 *  - The receiver MUST accept the packet if the Verification Tag
 *    matches either its own tag, OR the tag of its peer. Otherwise, the
 *    receiver MUST silently discard the packet and take no further
 *    action.
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_9_1_abort(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;

	if (!sctp_vtag_verify_either(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* Make sure that the ABORT chunk has a valid length.
	 * Since this is an ABORT chunk, we have to discard it
	 * because of the following text:
	 * RFC 2960, Section 3.3.7
	 *    If an endpoint receives an ABORT with a format error or for an
	 *    association that doesn't exist, it MUST silently discard it.
	 * Becasue the length is "invalid", we can't really discard just
	 * as we do not know its true length.  So, to be safe, discard the
	 * packet.
	 */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_abort_chunk_t)))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* ADD-IP: Special case for ABORT chunks
	 * F4)  One special consideration is that ABORT Chunks arriving
	 * destined to the IP address being deleted MUST be
	 * ignored (see Section 5.3.1 for further details).
	 */
	if (SCTP_ADDR_DEL ==
		    sctp_bind_addr_state(&asoc->base.bind_addr, &chunk->dest))
		return sctp_sf_discard_chunk(ep, asoc, type, arg, commands);

	return __sctp_sf_do_9_1_abort(ep, asoc, type, arg, commands);
}

static sctp_disposition_t __sctp_sf_do_9_1_abort(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	unsigned len;
	__be16 error = SCTP_ERROR_NO_ERROR;

	/* See if we have an error cause code in the chunk.  */
	len = ntohs(chunk->chunk_hdr->length);
	if (len >= sizeof(struct sctp_chunkhdr) + sizeof(struct sctp_errhdr))
		error = ((sctp_errhdr_t *)chunk->skb->data)->cause;

	sctp_add_cmd_sf(commands, SCTP_CMD_SET_SK_ERR, SCTP_ERROR(ECONNRESET));
	/* ASSOC_FAILED will DELETE_TCB. */
	sctp_add_cmd_sf(commands, SCTP_CMD_ASSOC_FAILED, SCTP_PERR(error));
	SCTP_INC_STATS(SCTP_MIB_ABORTEDS);
	SCTP_DEC_STATS(SCTP_MIB_CURRESTAB);

	return SCTP_DISPOSITION_ABORT;
}

/*
 * Process an ABORT.  (COOKIE-WAIT state)
 *
 * See sctp_sf_do_9_1_abort() above.
 */
sctp_disposition_t sctp_sf_cookie_wait_abort(const struct sctp_endpoint *ep,
				     const struct sctp_association *asoc,
				     const sctp_subtype_t type,
				     void *arg,
				     sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	unsigned len;
	__be16 error = SCTP_ERROR_NO_ERROR;

	if (!sctp_vtag_verify_either(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* Make sure that the ABORT chunk has a valid length.
	 * Since this is an ABORT chunk, we have to discard it
	 * because of the following text:
	 * RFC 2960, Section 3.3.7
	 *    If an endpoint receives an ABORT with a format error or for an
	 *    association that doesn't exist, it MUST silently discard it.
	 * Becasue the length is "invalid", we can't really discard just
	 * as we do not know its true length.  So, to be safe, discard the
	 * packet.
	 */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_abort_chunk_t)))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* See if we have an error cause code in the chunk.  */
	len = ntohs(chunk->chunk_hdr->length);
	if (len >= sizeof(struct sctp_chunkhdr) + sizeof(struct sctp_errhdr))
		error = ((sctp_errhdr_t *)chunk->skb->data)->cause;

	return sctp_stop_t1_and_abort(commands, error, ECONNREFUSED, asoc,
				      chunk->transport);
}

/*
 * Process an incoming ICMP as an ABORT.  (COOKIE-WAIT state)
 */
sctp_disposition_t sctp_sf_cookie_wait_icmp_abort(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands)
{
	return sctp_stop_t1_and_abort(commands, SCTP_ERROR_NO_ERROR,
				      ENOPROTOOPT, asoc,
				      (struct sctp_transport *)arg);
}

/*
 * Process an ABORT.  (COOKIE-ECHOED state)
 */
sctp_disposition_t sctp_sf_cookie_echoed_abort(const struct sctp_endpoint *ep,
					       const struct sctp_association *asoc,
					       const sctp_subtype_t type,
					       void *arg,
					       sctp_cmd_seq_t *commands)
{
	/* There is a single T1 timer, so we should be able to use
	 * common function with the COOKIE-WAIT state.
	 */
	return sctp_sf_cookie_wait_abort(ep, asoc, type, arg, commands);
}

/*
 * Stop T1 timer and abort association with "INIT failed".
 *
 * This is common code called by several sctp_sf_*_abort() functions above.
 */
static sctp_disposition_t sctp_stop_t1_and_abort(sctp_cmd_seq_t *commands,
					   __be16 error, int sk_err,
					   const struct sctp_association *asoc,
					   struct sctp_transport *transport)
{
	SCTP_DEBUG_PRINTK("ABORT received (INIT).\n");
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_CLOSED));
	SCTP_INC_STATS(SCTP_MIB_ABORTEDS);
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_INIT));
	sctp_add_cmd_sf(commands, SCTP_CMD_SET_SK_ERR, SCTP_ERROR(sk_err));
	/* CMD_INIT_FAILED will DELETE_TCB. */
	sctp_add_cmd_sf(commands, SCTP_CMD_INIT_FAILED,
			SCTP_PERR(error));
	return SCTP_DISPOSITION_ABORT;
}

/*
 * sctp_sf_do_9_2_shut
 *
 * Section: 9.2
 * Upon the reception of the SHUTDOWN, the peer endpoint shall
 *  - enter the SHUTDOWN-RECEIVED state,
 *
 *  - stop accepting new data from its SCTP user
 *
 *  - verify, by checking the Cumulative TSN Ack field of the chunk,
 *    that all its outstanding DATA chunks have been received by the
 *    SHUTDOWN sender.
 *
 * Once an endpoint as reached the SHUTDOWN-RECEIVED state it MUST NOT
 * send a SHUTDOWN in response to a ULP request. And should discard
 * subsequent SHUTDOWN chunks.
 *
 * If there are still outstanding DATA chunks left, the SHUTDOWN
 * receiver shall continue to follow normal data transmission
 * procedures defined in Section 6 until all outstanding DATA chunks
 * are acknowledged; however, the SHUTDOWN receiver MUST NOT accept
 * new data from its SCTP user.
 *
 * Verification Tag:  8.5 Verification Tag [Normal verification]
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_9_2_shutdown(const struct sctp_endpoint *ep,
					   const struct sctp_association *asoc,
					   const sctp_subtype_t type,
					   void *arg,
					   sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	sctp_shutdownhdr_t *sdh;
	sctp_disposition_t disposition;
	struct sctp_ulpevent *ev;
	__u32 ctsn;

	if (!sctp_vtag_verify(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* Make sure that the SHUTDOWN chunk has a valid length. */
	if (!sctp_chunk_length_valid(chunk,
				      sizeof(struct sctp_shutdown_chunk_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	/* Convert the elaborate header.  */
	sdh = (sctp_shutdownhdr_t *)chunk->skb->data;
	skb_pull(chunk->skb, sizeof(sctp_shutdownhdr_t));
	chunk->subh.shutdown_hdr = sdh;
	ctsn = ntohl(sdh->cum_tsn_ack);

	if (TSN_lt(ctsn, asoc->ctsn_ack_point)) {
		SCTP_DEBUG_PRINTK("ctsn %x\n", ctsn);
		SCTP_DEBUG_PRINTK("ctsn_ack_point %x\n", asoc->ctsn_ack_point);
		return SCTP_DISPOSITION_DISCARD;
	}

	/* If Cumulative TSN Ack beyond the max tsn currently
	 * send, terminating the association and respond to the
	 * sender with an ABORT.
	 */
	if (!TSN_lt(ctsn, asoc->next_tsn))
		return sctp_sf_violation_ctsn(ep, asoc, type, arg, commands);

	/* API 5.3.1.5 SCTP_SHUTDOWN_EVENT
	 * When a peer sends a SHUTDOWN, SCTP delivers this notification to
	 * inform the application that it should cease sending data.
	 */
	ev = sctp_ulpevent_make_shutdown_event(asoc, 0, GFP_ATOMIC);
	if (!ev) {
		disposition = SCTP_DISPOSITION_NOMEM;
		goto out;
	}
	sctp_add_cmd_sf(commands, SCTP_CMD_EVENT_ULP, SCTP_ULPEVENT(ev));

	/* Upon the reception of the SHUTDOWN, the peer endpoint shall
	 *  - enter the SHUTDOWN-RECEIVED state,
	 *  - stop accepting new data from its SCTP user
	 *
	 * [This is implicit in the new state.]
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_SHUTDOWN_RECEIVED));
	disposition = SCTP_DISPOSITION_CONSUME;

	if (sctp_outq_is_empty(&asoc->outqueue)) {
		disposition = sctp_sf_do_9_2_shutdown_ack(ep, asoc, type,
							  arg, commands);
	}

	if (SCTP_DISPOSITION_NOMEM == disposition)
		goto out;

	/*  - verify, by checking the Cumulative TSN Ack field of the
	 *    chunk, that all its outstanding DATA chunks have been
	 *    received by the SHUTDOWN sender.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_PROCESS_CTSN,
			SCTP_BE32(chunk->subh.shutdown_hdr->cum_tsn_ack));

out:
	return disposition;
}

/*
 * sctp_sf_do_9_2_shut_ctsn
 *
 * Once an endpoint has reached the SHUTDOWN-RECEIVED state,
 * it MUST NOT send a SHUTDOWN in response to a ULP request.
 * The Cumulative TSN Ack of the received SHUTDOWN chunk
 * MUST be processed.
 */
sctp_disposition_t sctp_sf_do_9_2_shut_ctsn(const struct sctp_endpoint *ep,
					   const struct sctp_association *asoc,
					   const sctp_subtype_t type,
					   void *arg,
					   sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	sctp_shutdownhdr_t *sdh;
	__u32 ctsn;

	if (!sctp_vtag_verify(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* Make sure that the SHUTDOWN chunk has a valid length. */
	if (!sctp_chunk_length_valid(chunk,
				      sizeof(struct sctp_shutdown_chunk_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	sdh = (sctp_shutdownhdr_t *)chunk->skb->data;
	ctsn = ntohl(sdh->cum_tsn_ack);

	if (TSN_lt(ctsn, asoc->ctsn_ack_point)) {
		SCTP_DEBUG_PRINTK("ctsn %x\n", ctsn);
		SCTP_DEBUG_PRINTK("ctsn_ack_point %x\n", asoc->ctsn_ack_point);
		return SCTP_DISPOSITION_DISCARD;
	}

	/* If Cumulative TSN Ack beyond the max tsn currently
	 * send, terminating the association and respond to the
	 * sender with an ABORT.
	 */
	if (!TSN_lt(ctsn, asoc->next_tsn))
		return sctp_sf_violation_ctsn(ep, asoc, type, arg, commands);

	/* verify, by checking the Cumulative TSN Ack field of the
	 * chunk, that all its outstanding DATA chunks have been
	 * received by the SHUTDOWN sender.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_PROCESS_CTSN,
			SCTP_BE32(sdh->cum_tsn_ack));

	return SCTP_DISPOSITION_CONSUME;
}

/* RFC 2960 9.2
 * If an endpoint is in SHUTDOWN-ACK-SENT state and receives an INIT chunk
 * (e.g., if the SHUTDOWN COMPLETE was lost) with source and destination
 * transport addresses (either in the IP addresses or in the INIT chunk)
 * that belong to this association, it should discard the INIT chunk and
 * retransmit the SHUTDOWN ACK chunk.
 */
sctp_disposition_t sctp_sf_do_9_2_reshutack(const struct sctp_endpoint *ep,
				    const struct sctp_association *asoc,
				    const sctp_subtype_t type,
				    void *arg,
				    sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = (struct sctp_chunk *) arg;
	struct sctp_chunk *reply;

	/* Make sure that the chunk has a valid length */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_chunkhdr_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	/* Since we are not going to really process this INIT, there
	 * is no point in verifying chunk boundries.  Just generate
	 * the SHUTDOWN ACK.
	 */
	reply = sctp_make_shutdown_ack(asoc, chunk);
	if (NULL == reply)
		goto nomem;

	/* Set the transport for the SHUTDOWN ACK chunk and the timeout for
	 * the T2-SHUTDOWN timer.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_SETUP_T2, SCTP_CHUNK(reply));

	/* and restart the T2-shutdown timer. */
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_RESTART,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T2_SHUTDOWN));

	sctp_add_cmd_sf(commands, SCTP_CMD_REPLY, SCTP_CHUNK(reply));

	return SCTP_DISPOSITION_CONSUME;
nomem:
	return SCTP_DISPOSITION_NOMEM;
}

/*
 * sctp_sf_do_ecn_cwr
 *
 * Section:  Appendix A: Explicit Congestion Notification
 *
 * CWR:
 *
 * RFC 2481 details a specific bit for a sender to send in the header of
 * its next outbound TCP segment to indicate to its peer that it has
 * reduced its congestion window.  This is termed the CWR bit.  For
 * SCTP the same indication is made by including the CWR chunk.
 * This chunk contains one data element, i.e. the TSN number that
 * was sent in the ECNE chunk.  This element represents the lowest
 * TSN number in the datagram that was originally marked with the
 * CE bit.
 *
 * Verification Tag: 8.5 Verification Tag [Normal verification]
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_ecn_cwr(const struct sctp_endpoint *ep,
				      const struct sctp_association *asoc,
				      const sctp_subtype_t type,
				      void *arg,
				      sctp_cmd_seq_t *commands)
{
	sctp_cwrhdr_t *cwr;
	struct sctp_chunk *chunk = arg;
	u32 lowest_tsn;

	if (!sctp_vtag_verify(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_ecne_chunk_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	cwr = (sctp_cwrhdr_t *) chunk->skb->data;
	skb_pull(chunk->skb, sizeof(sctp_cwrhdr_t));

	lowest_tsn = ntohl(cwr->lowest_tsn);

	/* Does this CWR ack the last sent congestion notification? */
	if (TSN_lte(asoc->last_ecne_tsn, lowest_tsn)) {
		/* Stop sending ECNE. */
		sctp_add_cmd_sf(commands,
				SCTP_CMD_ECN_CWR,
				SCTP_U32(lowest_tsn));
	}
	return SCTP_DISPOSITION_CONSUME;
}

/*
 * sctp_sf_do_ecne
 *
 * Section:  Appendix A: Explicit Congestion Notification
 *
 * ECN-Echo
 *
 * RFC 2481 details a specific bit for a receiver to send back in its
 * TCP acknowledgements to notify the sender of the Congestion
 * Experienced (CE) bit having arrived from the network.  For SCTP this
 * same indication is made by including the ECNE chunk.  This chunk
 * contains one data element, i.e. the lowest TSN associated with the IP
 * datagram marked with the CE bit.....
 *
 * Verification Tag: 8.5 Verification Tag [Normal verification]
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_ecne(const struct sctp_endpoint *ep,
				   const struct sctp_association *asoc,
				   const sctp_subtype_t type,
				   void *arg,
				   sctp_cmd_seq_t *commands)
{
	sctp_ecnehdr_t *ecne;
	struct sctp_chunk *chunk = arg;

	if (!sctp_vtag_verify(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_ecne_chunk_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	ecne = (sctp_ecnehdr_t *) chunk->skb->data;
	skb_pull(chunk->skb, sizeof(sctp_ecnehdr_t));

	/* If this is a newer ECNE than the last CWR packet we sent out */
	sctp_add_cmd_sf(commands, SCTP_CMD_ECN_ECNE,
			SCTP_U32(ntohl(ecne->lowest_tsn)));

	return SCTP_DISPOSITION_CONSUME;
}

/*
 * Section: 6.2  Acknowledgement on Reception of DATA Chunks
 *
 * The SCTP endpoint MUST always acknowledge the reception of each valid
 * DATA chunk.
 *
 * The guidelines on delayed acknowledgement algorithm specified in
 * Section 4.2 of [RFC2581] SHOULD be followed. Specifically, an
 * acknowledgement SHOULD be generated for at least every second packet
 * (not every second DATA chunk) received, and SHOULD be generated within
 * 200 ms of the arrival of any unacknowledged DATA chunk. In some
 * situations it may be beneficial for an SCTP transmitter to be more
 * conservative than the algorithms detailed in this document allow.
 * However, an SCTP transmitter MUST NOT be more aggressive than the
 * following algorithms allow.
 *
 * A SCTP receiver MUST NOT generate more than one SACK for every
 * incoming packet, other than to update the offered window as the
 * receiving application consumes new data.
 *
 * Verification Tag:  8.5 Verification Tag [Normal verification]
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_eat_data_6_2(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	int error;

	if (!sctp_vtag_verify(chunk, asoc)) {
		sctp_add_cmd_sf(commands, SCTP_CMD_REPORT_BAD_TAG,
				SCTP_NULL());
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);
	}

	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_data_chunk_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	error = sctp_eat_data(asoc, chunk, commands );
	switch (error) {
	case SCTP_IERROR_NO_ERROR:
		break;
	case SCTP_IERROR_HIGH_TSN:
	case SCTP_IERROR_BAD_STREAM:
		SCTP_INC_STATS(SCTP_MIB_IN_DATA_CHUNK_DISCARDS);
		goto discard_noforce;
	case SCTP_IERROR_DUP_TSN:
	case SCTP_IERROR_IGNORE_TSN:
		SCTP_INC_STATS(SCTP_MIB_IN_DATA_CHUNK_DISCARDS);
		goto discard_force;
	case SCTP_IERROR_NO_DATA:
		goto consume;
	case SCTP_IERROR_PROTO_VIOLATION:
		return sctp_sf_abort_violation(ep, asoc, chunk, commands,
			(u8 *)chunk->subh.data_hdr, sizeof(sctp_datahdr_t));
	default:
		BUG();
	}

	if (asoc->autoclose) {
		sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_RESTART,
				SCTP_TO(SCTP_EVENT_TIMEOUT_AUTOCLOSE));
	}

	/* If this is the last chunk in a packet, we need to count it
	 * toward sack generation.  Note that we need to SACK every
	 * OTHER packet containing data chunks, EVEN IF WE DISCARD
	 * THEM.  We elect to NOT generate SACK's if the chunk fails
	 * the verification tag test.
	 *
	 * RFC 2960 6.2 Acknowledgement on Reception of DATA Chunks
	 *
	 * The SCTP endpoint MUST always acknowledge the reception of
	 * each valid DATA chunk.
	 *
	 * The guidelines on delayed acknowledgement algorithm
	 * specified in  Section 4.2 of [RFC2581] SHOULD be followed.
	 * Specifically, an acknowledgement SHOULD be generated for at
	 * least every second packet (not every second DATA chunk)
	 * received, and SHOULD be generated within 200 ms of the
	 * arrival of any unacknowledged DATA chunk.  In some
	 * situations it may be beneficial for an SCTP transmitter to
	 * be more conservative than the algorithms detailed in this
	 * document allow. However, an SCTP transmitter MUST NOT be
	 * more aggressive than the following algorithms allow.
	 */
	if (chunk->end_of_packet)
		sctp_add_cmd_sf(commands, SCTP_CMD_GEN_SACK, SCTP_NOFORCE());

	return SCTP_DISPOSITION_CONSUME;

discard_force:
	/* RFC 2960 6.2 Acknowledgement on Reception of DATA Chunks
	 *
	 * When a packet arrives with duplicate DATA chunk(s) and with
	 * no new DATA chunk(s), the endpoint MUST immediately send a
	 * SACK with no delay.  If a packet arrives with duplicate
	 * DATA chunk(s) bundled with new DATA chunks, the endpoint
	 * MAY immediately send a SACK.  Normally receipt of duplicate
	 * DATA chunks will occur when the original SACK chunk was lost
	 * and the peer's RTO has expired.  The duplicate TSN number(s)
	 * SHOULD be reported in the SACK as duplicate.
	 */
	/* In our case, we split the MAY SACK advice up whether or not
	 * the last chunk is a duplicate.'
	 */
	if (chunk->end_of_packet)
		sctp_add_cmd_sf(commands, SCTP_CMD_GEN_SACK, SCTP_FORCE());
	return SCTP_DISPOSITION_DISCARD;

discard_noforce:
	if (chunk->end_of_packet)
		sctp_add_cmd_sf(commands, SCTP_CMD_GEN_SACK, SCTP_NOFORCE());

	return SCTP_DISPOSITION_DISCARD;
consume:
	return SCTP_DISPOSITION_CONSUME;

}

/*
 * sctp_sf_eat_data_fast_4_4
 *
 * Section: 4 (4)
 * (4) In SHUTDOWN-SENT state the endpoint MUST acknowledge any received
 *    DATA chunks without delay.
 *
 * Verification Tag:  8.5 Verification Tag [Normal verification]
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_eat_data_fast_4_4(const struct sctp_endpoint *ep,
				     const struct sctp_association *asoc,
				     const sctp_subtype_t type,
				     void *arg,
				     sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	int error;

	if (!sctp_vtag_verify(chunk, asoc)) {
		sctp_add_cmd_sf(commands, SCTP_CMD_REPORT_BAD_TAG,
				SCTP_NULL());
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);
	}

	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_data_chunk_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	error = sctp_eat_data(asoc, chunk, commands );
	switch (error) {
	case SCTP_IERROR_NO_ERROR:
	case SCTP_IERROR_HIGH_TSN:
	case SCTP_IERROR_DUP_TSN:
	case SCTP_IERROR_IGNORE_TSN:
	case SCTP_IERROR_BAD_STREAM:
		break;
	case SCTP_IERROR_NO_DATA:
		goto consume;
	case SCTP_IERROR_PROTO_VIOLATION:
		return sctp_sf_abort_violation(ep, asoc, chunk, commands,
			(u8 *)chunk->subh.data_hdr, sizeof(sctp_datahdr_t));
	default:
		BUG();
	}

	/* Go a head and force a SACK, since we are shutting down. */

	/* Implementor's Guide.
	 *
	 * While in SHUTDOWN-SENT state, the SHUTDOWN sender MUST immediately
	 * respond to each received packet containing one or more DATA chunk(s)
	 * with a SACK, a SHUTDOWN chunk, and restart the T2-shutdown timer
	 */
	if (chunk->end_of_packet) {
		/* We must delay the chunk creation since the cumulative
		 * TSN has not been updated yet.
		 */
		sctp_add_cmd_sf(commands, SCTP_CMD_GEN_SHUTDOWN, SCTP_NULL());
		sctp_add_cmd_sf(commands, SCTP_CMD_GEN_SACK, SCTP_FORCE());
		sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_RESTART,
				SCTP_TO(SCTP_EVENT_TIMEOUT_T2_SHUTDOWN));
	}

consume:
	return SCTP_DISPOSITION_CONSUME;
}

/*
 * Section: 6.2  Processing a Received SACK
 * D) Any time a SACK arrives, the endpoint performs the following:
 *
 *     i) If Cumulative TSN Ack is less than the Cumulative TSN Ack Point,
 *     then drop the SACK.   Since Cumulative TSN Ack is monotonically
 *     increasing, a SACK whose Cumulative TSN Ack is less than the
 *     Cumulative TSN Ack Point indicates an out-of-order SACK.
 *
 *     ii) Set rwnd equal to the newly received a_rwnd minus the number
 *     of bytes still outstanding after processing the Cumulative TSN Ack
 *     and the Gap Ack Blocks.
 *
 *     iii) If the SACK is missing a TSN that was previously
 *     acknowledged via a Gap Ack Block (e.g., the data receiver
 *     reneged on the data), then mark the corresponding DATA chunk
 *     as available for retransmit:  Mark it as missing for fast
 *     retransmit as described in Section 7.2.4 and if no retransmit
 *     timer is running for the destination address to which the DATA
 *     chunk was originally transmitted, then T3-rtx is started for
 *     that destination address.
 *
 * Verification Tag:  8.5 Verification Tag [Normal verification]
 *
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * (asoc, reply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_eat_sack_6_2(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const sctp_subtype_t type,
					void *arg,
					sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	sctp_sackhdr_t *sackh;
	__u32 ctsn;

	if (!sctp_vtag_verify(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* Make sure that the SACK chunk has a valid length. */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_sack_chunk_t)))
		return sctp_sf_violation_chunklen(ep, asoc, type, arg,
						  commands);

	/* Pull the SACK chunk from the data buffer */
	sackh = sctp_sm_pull_sack(chunk);
	/* Was this a bogus SACK? */
	if (!sackh)
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);
	chunk->subh.sack_hdr = sackh;
	ctsn = ntohl(sackh->cum_tsn_ack);

	/* i) If Cumulative TSN Ack is less than the Cumulative TSN
	 *     Ack Point, then drop the SACK.  Since Cumulative TSN
	 *     Ack is monotonically increasing, a SACK whose
	 *     Cumulative TSN Ack is less than the Cumulative TSN Ack
	 *     Point indicates an out-of-order SACK.
	 */
	if (TSN_lt(ctsn, asoc->ctsn_ack_point)) {
		SCTP_DEBUG_PRINTK("ctsn %x\n", ctsn);
		SCTP_DEBUG_PRINTK("ctsn_ack_point %x\n", asoc->ctsn_ack_point);
		return SCTP_DISPOSITION_DISCARD;
	}

	/* If Cumulative TSN Ack beyond the max tsn currently
	 * send, terminating the association and respond to the
	 * sender with an ABORT.
	 */
	if (!TSN_lt(ctsn, asoc->next_tsn))
		return sctp_sf_violation_ctsn(ep, asoc, type, arg, commands);

	/* Return this SACK for further processing.  */
	sctp_add_cmd_sf(commands, SCTP_CMD_PROCESS_SACK, SCTP_SACKH(sackh));

	/* Note: We do the rest of the work on the PROCESS_SACK
	 * sideeffect.
	 */
	return SCTP_DISPOSITION_CONSUME;
}

/*
 * Generate an ABORT in response to a packet.
 *
 * Section: 8.4 Handle "Out of the blue" Packets, sctpimpguide 2.41
 *
 * 8) The receiver should respond to the sender of the OOTB packet with
 *    an ABORT.  When sending the ABORT, the receiver of the OOTB packet
 *    MUST fill in the Verification Tag field ofl impoutbound packet
 *    withl impvalue forp. nel implementation
 * (C) Copyright IOOTB2004
 *2001,  and setl impT-bit2000 CisChunk Flags to indtatie thaCopyr2004
 *lementation
 * (Cis reflected.  After sending this ABORT,t (c) 2002 receiver(c) 1999-200rola, Inshall discard
 * These are thec.
 takc) 2002 no further acion
.
 *2004lementation
 * (:ntationThe returnht (c) isl impdisposiion
 right Ic002 .
*/
static sctp_d/or modify_t* the sf_tabort_8_4_8(const strucPublic endpoint *ep,
	ndatshed by
 * the Freassocition
 *asocoundation; eitlic Lubtype_t ersioundativoid *argoundati latecmd_seq_t *commands)
{
	y
 * the Freola, In*ola, In= NULL;hope that it
 r the *RRANTY= arg WITHOUT ANY WARRANTY;nse a;

	e useful, lateootb_pkt_new(ur oper the)    if (2001, ) {
		/* Make annux Ke.ou caT t (cw kerbe * Coiight Iur o
		 *te i but. Gene/
		nse a********make_nse a*
 * warrant, 0);
	 MERC!nse aILITY ation *********freeCHANTABIe re	n redisSCTP_DISPOSITION_NOMEMe re}
TY or R* This vtp.
 f T-B (c)s * CocenseMERC latetest_T_bit(d a co)nse
2001, ->ot, w= ntohl(r the-> latehdr Suitey of  or S CopyriskbCorp impbelong SCTsock foP imcounting. icense for ->skbmail = ep->base.sk USA. it
 * will_appendARRANTCHANTAB, d a co lksctp deadd is dif(ted in t,ng witCMD_SEND_PKToundatg witPACKETCHANTABI.sourcg witINC_STATS(g witMIB_OUTCTRLCHUNKS.sourceforgsf_p functi(ep,e GNU,.
 *
 t ev, ted in the re * along with GNU CC; seCONSUME;
le CO * along with GNU CC; see the f}

/ationRtationdESS ERRORer the from peer.  GenerCopyg witREMOTE_owsky2004event as ULP notentation
  fixeach cause include-2000 Cisr the ttationAPI 5.3.1.3 - *    Sridhar Samudra* you can redistribute it and/or modify it under the termsthe GNU General Public Licoperr_ala@uyished by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This SCTP implementation is distributed in the hope that it
 RRANTY; without evenf MERC!okia.ot, _vemeny Bostod by:
mple n redis Written or modified by:
 *    La Monte H.P. Yarr
 or FITNEsurpyright (ctowsky       has aht (id length.Free  to fix...r the_de <li_#inclshared wsizeofware F	    <de <lit) * be incorporated ivioltion
ncludelenfied by:
 *    La Monundati	 /

#include <leforge.net>
 *
 * Or submit a bug rPROCESS_OPERRh>
#it a buksct Bostoite:
 l <piggy@acm.org>
 *    Karl Knut.us>
 *  ProcessESS inCorp. SHUTDOWN ACKmm@us.ibFkotoSemplem 9.2e;
  Upoel impntatipt it undeonst struct rnel on *asoc,
 the of statconsstopopyrig2-shutdown timer,_chun aon *asoc,
COMPLETE       orp.tsconswsky,e macremove taterecorpyright I 2, or (at mm@us.ibu can redistribute it and/or modifymm@u@nokia.com>
 *    Daisy Changdo_9_2_final@us.ibm.com>
 *    Ardelle Fan	    <ardelle.fan@intel.com>
 *    Ryan Layer	    <rmlayer@us.ibm.com>
 *    Kevin Gao		    <kevin.gao@intel.com>
 *
 * Any bugs reported given to us we will tgs reported given toreplysctp_associatioulpa     *ev try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#include <linux/types.h>
#includeonst str_ACKux/kernel.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/net.r thehdr <linux/inet.h>
#include <net/sock.h>
#include <net/inet_ecn.h>
#include <linux/sk or 10.2 H)e_t paylen);
staticala@us.ibm.c
	 *_5(c Wheong wite Hplete it anid *payloptp_adures (s*ep,
				 )TP Lionst ala@us.ibm.cois passedsend anyupper layer._5(c/
	ev********truct sctails.
2, o_change*
 * wa0mit a b_sf_do_5_);
sh>
#inc4
 *ommat sctp but, GFP_ATOMIC);
s to fevple goto nomeme <linu...	  size_t paylen);
static int sctp_eaata(con			 ,
			more details.id *payl_p_endpoi*
 * warranty o struc,
			sctp_endpoint soc,
	e <linuDosctp_    <ed in t now (art ofalloation
), snd aat weonst havsctpnsist    s ofewritmemoryuct sctp_ch failetion /buff.h>
#include <net/sctp/sctp.h>
#iEVENT_ULPmit a bULP, int(evite:
 /nst struct sctp_association *asoc,
				  struct sctp_chunk *chunk,nst 		  const void *payload,
		_cmd_seq_t *commands,
					   __be16 errorTIMER_STO_dispog witTO.sf.ne, int ct sOUT_T2;
static ite:
 bort_violation(
				     const struct sctp_endpoint *ep,
				     const struct5;
static sGUARDciation *asoc,
				     void *arg,
				  NEW://wwEdpoint *epispos.sf.neispos_CLOSE
				*    http://www.sf.net/pronst strp
 *nt *epDEp://www.sf.net/prCURRESTAB *epeforge.net>
 *
 * Or submit a bug rREPLYmit a buinclu *commde <linu...st struct sctp_association *asoc,
			 strmd_seq_t *commands,
					   __be16 errorDEtati_TCBmit a b but(n(
		 * along with GNU CC; seamlen(
			;

tatic struc:ation *arg,
				l Pubev);struct:          <karl@athena.chicago.il.us>
 *   FC 2960, 8.4 - Handle "Oussociatioblue" P001, s,poratimpguide 2.41mm@us.ib5) Iight Iola, Incontain.h>
onst struct should huct sctp_on.
shouldImplementspomachnd anychunk * *
 * These are the Copyze_t paylen);
statimm@uion_struc the SCTP truct sctp_);
statictp_endpoint *ec) 1999-2001 Motorola, InmustTP kernel implementation
 * (C) Copyright IBM Corp.1 Motorola, In Copyrighlementation
 * (Cntation-2000 Cis    const strat sctp_en* Copyright (c) 2001-2002 Intel Corp.
 * Copyright (c	     const stype,
	rp.
 *
 * This isi.huang8)tp_chunoint *ep,
			const struct sctp_association *asoc,
				     type,
	SS FOR A P				     void *argux Kernel entation.
 *
 * These are thtype,
	* SCTP kernel implementation
 * (C) Copyright IBM Corp. 2001, 2004
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 * Copyright (c) 2001-2002 Intel Corp.
 * Copyright (c) 2002      Nokia Corp.
 *
 * This is part of the SCTP Linux Kernel Implementation.
 *
 * These are the state functions for the state machine.
 *
 * This SCTP implementa@nokia.com>
 *    Daisy Chang	otbished by
 * the Free Software Foundaton; either version 2, or (at your option) * any later version.
 *
 * Thi SCTP implementrr(const struct sctp_endpoint *ep,
				       const struct sctp_associk_buff *se s= <jgrimail iation *oc,
						Y; w;
	__u8Y; wee Smd_snt *****id *_acress0e <l    http://www.sf.net/projeOFBLUEp
 *
 ch =tware Fth);

	if (u)nk->chunde <lihdr;
	doLITY or Repor me <net/so See the      is laborttrucminimalFree Softw0,
 s(ch->de <li) <sociation *asoc,
						 cnse
 * alon
#include <net/sock.h>
#include <net/inet_ecn.h>
#include <linux/skbuct sNowchunk);
 kl_sawe at leasttic scaux/kerneead sctpnst dnd ainl Counk)a.h>
ype appropriateic LicenseMERCt a buID;
static s str=unk-->fy
 *****equired_length))1gram), 9void *ext, *ep,
		3.3.7nk the  Moreov(conuunk *any circumstancesonst e Softwarwille SHUTDO_sf_autort_pux Keruct scNOTconst struct satd removbye SHUTDO the SCTand removofoc,
	own in SHUTDOWN-ACK-SENT s removs not the chunk  incorporated into the next SCTP release.
 */

#include <lse are the state functio      len CK-SflowsFree Snk_len****(ly(chu)ch) + WORD_ROUND******************ve receiv a SHUT>ase _tail_oftwaer(skb*******/

/*
 * Process the final SHUTDOWN COMPLETE.
 *
 * Section: 4 (C) (diagram)***************************_length} whilerificatio< Tag field of the packe try to equired_lengt* be incorporated ied_le pub5fied by:
 *    La Monte H.P. Yarroelse be incorporated iense as publiied by:
 *    La Monte H.P. Yarr.us>
 *      sctantp_cmd_seq_t *commaonst struct sctp_end *ep,
	:,
			5static sctp_disposition_t sctp_sf_violation_ctsn(
				     const struct sctp_endpoint *ep,
				     const struct sctp_association *asoc,
				     const sctp_subtype_t type,
				     void *arg,
				     sctp_cmd_seq_t *commands);

static sctp_disposition_t sctp_sf_violation_chunk(
				     const struct sctp_endpoint *ep,
				     const struct sctp_association *asoc,
				     const sctp_subtype_t type,
				     void *arg,
				     sctp_cmd_seq_t *commands);

static sctp_ierrorInpueat_da(e Softwad by:
 *    La Monte H.P. Yai.huangOut960 6.10 okia.com>
 *    DaOT bundlu can redistribute it and/or modify it under the t size of
 * the GNU General Public Licacket
 *   shed by
 * the Free Software Foundatition_on; either version 2, or (at your option)
that the SHUlater version.
 *
 * Thistion_ SCTP implementation_tion is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *     id *       ************************
 * warranty of MERCHANTABILITY or FITNESS 
static sctp_pe_t tk theARTICULAR PURPOSE.
 * See the GNUral Public Licenseid *subtype_t type,
					void *arg,
					sctp_cmd_p.h>
#ihucopy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, , SCTple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Please send any bug reports or fixes you make to the, SCmail address(es):
 *    lksctp developers <lksctp-developers@, SCTsourceforge.net>
 *
 * Or submit a bug report through the following website:
 *    http://www.sf.net/projects/lksctp
 *
 */*p_sf_vio ...
 * -gthndliin#incl,tiondon't wap tho    cossction onst s* Coright I2001,  in SHUTDOWN-Aep,
						 const struct sctp_association *asoc,
						 const 8.5.1(C), sctpimpguide 2.41.
 * C) Rules for packet carrying SWe net sctp functions fres*/
	sctp_add_cmthe CLa    ction poposiial bomms thettacks <kotoaddodifymandu  scdation section 9.s ths documentassocit sctThreats IDic License incorporated into the next SCTP release.
 */

#include son          <karl@athena.chicago.il.us>
 *      sct    const strin COOKIE_ECHOED orDELETE_TWAITtion_tentation is free software  8.5.1 E) Rulesom>
 lation_carrys theCTP_DISPOSITItype,
_sf_vio_sf_authes theDELETE_-CB;
}

/*
 * Res-ond to a nt (c) 2002   const sti    ep,
		
			SHOULDOSE.followed,ond oSCTP words i    strnticatebe tIB_Sew Kort_p_cmdOf 9.2)B(c) add_cmd_HUTDO[ED));meant will ation knowcheckl implementation
 * (Cstructsc) 2002TP_STAT --piggy ]i.huanmands);
static struct sctp_packe8_5_1_E_sa type, arg, commands);

	/* Make suretion_ the SHUTDOWN_COMPLETE chunk has a valid lg field to Talater version.
 *
 * Thig fiel SCTP implementg fiel.gao@intel.com>
 *
 * Any bugs reported given to us we will tryctp_disposition_t sctp_sf_do_5_2_6_stale(const struct sctp_endpoint *ep,
						 const struct sctp_association *asoc,
						 const sctp_subtype_t type,
						 void *arg,
						 sctp_cmd_seq_t *commands);
nk)
 Althoughress ofHUTDOWnasoc,
			 st2000 Cis case, in_ctronst sttion *to a_T5_Sar of th;

static sSnd anyGUARD))e itess of the INI2001_cmd_ state machsctpon_t funep,
		e
 * h   scsThese _sf_do_5_2_6_ation *calE(SC    con butasoc,
			 strucmd_seturn 0;

	return 1;
}

/***************y discard the packet
 *   and _taborE if it is not in the SHUTD ADDIPpoint is 4.2st str_sf_pdify it and SCONF-2002 e to tnds);
static struct sctp_packeascon
 * must set the
 *    Verification Tag fied to Tag_A, and also provide its own
 *    Verfication Tag (Tag_Z) in th Tag field.
 *
 * Ver.gao@intel.com>
 *
 * Any bugs reported given 	; without even the implied
 *   	*	 * IGength)) but WITHOUT ANY WAparamhdr	*   <tp_sfsingleton)
forge.nip							******
un			c, arg, crd(ep, 	*t is an OOikely(32ructerialgth < 			and th try to fix... any fixes shared will
 *LITY ion *asoc,
				     const sctp_subtypeORT_BAD_TAGrough the fonst strucCTP_DEC_STATS(SCTP_MIB_CURRESTAB);

	sctp_add_cmd_sf(commands,dpoint-IP:UST NOT bun1.1hunk ED));      * SCTSE.
 n(c) 2n *auunk t* Copd way by using			voihe mecmd_ism definassoci[I-D.ietf-tsvwg-, ar-hunk].* Sect packet hunk  *
 *ctp_assunhunk MUST haveitcontainingilentlyVENT_TIMof thhunk describif (chunk->sctp_hdr->vtag != 0)
	
	strucnds, SCTP_, com_nohunk && !k->chunhunk* be incorporated i functictp-devext SCTP release.
 */

#include <linux/types.h>
#include * SHUTint MUx/kernel.h>
#include <linuux/ip.h>
#include <linux/ipv6.h>
#include <linux/net.cause clude <linux/inet.h>
#include <net/sock.h>
#include <net/inet_ecn.h>
#include <linux/skbuhdr*********, commandsPLETE>chunk_h->dataiatiraril 330,
 * 2111-ly, thconstt is an OO****f the packet is an OO *)2111-tp_sfs;

	 * co 330,
 s(t is an OO->p.*******md_seq_and thu**************tp_sf_pd********/

/*
 * Process the fintp_sf#include <net/inet_ecn.h>
#i   ( SCTP )t is an OO*/

#include <linulemenyrd the packe      before CLOSED
 SCTitnux/ip.h>
#inclufixes 
	 * IG ur optionfielits associations *)(ep->base.sk, CLOSI +ude <li)fore procep->base*************endfore proc&card(ep,  are going away, we
	 * can treat this OOTB
	 */
	if (sctp_sstare prsoc, chucard(ep, */

#include <linuint MU5.2 E1) Compverifight (c) right Ily, thinumberruct sctt (c)fication
e Softwarstorassocia new*asoc,
					cvariablis any'Peer-Sraril-N caus'mally, this ly, this=e GNU->wsky@cause ly, thi+ 1ILITY or return se it anfirs(c) oint
 d an * SHUTnel impelopersction we can clean our oldhunk_hd-ACKATE_CLOommands, d ABORThas
	 * IG*****rsion 2, o_p_chu
	 * IGengt_cachg,
			 the
	 * It is to be4)				  *arg,equeunk- arg,
 matchint *epnext onointralaxphis i, CLOSED
rd the packease INIT chun bugwe mack(strdd_cmd_ arg, commd the packe2002 ,* th  siz
	 * SHUsctp-2002 Ito	 * state).
	ponsmands)
{
c.
 ands,OWN opyation  (nel impa     iadd_cmdlat of(SCT Corpbcan rransmitted)ectionIB_OUTEsg anially,s ofV1-V5ic License (!chunk->sints asstp_abo
	 * IG (ther version 2, or (at yo*****re p_assoc_change(asoc, 0hunk->skb-******/

/*
g with GNU CC; see the fi} ndpowrit)(err_ch<k->chunk_hdr) +
					sizeof(sctp_chunIt is to be2****RFC ight It (c) 1999-2000 Cis(packet));
				Sdling chunaPACKEgoto n(new(ept_bind_ad, arg,
	eof(sde <mtp_sskipruct sctS(SCgoto n * SHUonst simm    r->lengtBM Corp. tabort_8_4_8(epgoto ne th	sctiouslyoc, ty));

			sctpctp_proceshunk);hat ;

st    <.
 *a_asse
 * CTP_INC_STAT(packet));
				Sright  < 0)
		goto.  Note: I{
	stpossiblctp_pronory allocation.  */s_init2002 Iexists. 1 A paPURPOoccur wtruchunklnk * * SHU(), canrrimer ocmd_seordky@scIn suc conp_subtonst sctp_sus_init(ticate     GTP_DISPOSITION__asocno(c) nit;

unk_hdr,
			       GFP fixe
 * r the tdr_t *)chunk->skb->data;

 sctp_lookuK fod_sf(com*
 * was the race of = skb_pull(chunk->skb, sizeof(sctp_inithdr_t))DISCARDrrying SHU* Copyriheade the  *chunk);
 se If n *sctprrIf nP_MIB_OUTurn sad,
 arorp.C))
		goOMICo ailspes.h>
#incation en(), canccidentdr = n Gra scthunk	repl = se
 *'s beeNIT,uct denough room in the I->orted for ingleton)asoc = ;
	if (!new_asoc)
5) OSCTPwiunk.  */ to be report

	Ske sure tsincrce(chuiositionbe eiSCTP i be rep= arg;
orW_STATEnP_CMD_Npe,
	S);
	SCTP_DEC_dr->length) -
			sizeof(sc type, arg,is to be6)))
	ee INtintion
 t is			rociation scts_init()oc;
tsn(
	rn SCTP_DISPOSse {
			rescontaininuct sourceor "unknownfication
meter" and pl;
	stCopyneed to be reporATE_(const Toe endpos				per = (we'llt sctp_suause code for "unknown paractp_sf_tab;
			r_t);

	repmitIC, l,oto notry em.
 us needorted for uoeed 
		 * S/* Gr.
	 */s ma zee_STATE(Sk.  */asoc, chorted for  cauor ubis anycess of ou mll

	/);
				returit_hype,lopers@thu	ret	gotorkn);
	if
	strucde them in tausehunk->chunk we ciation *asoc,
				     const sctp_subtype_t type,
				  ull(chunk->t/sctp/structs.h>

static struct sctp_packet *sint MUST NOT bun3ctp.orgl rthat is br "unknomanipunet/sohe sstrucbuile SCTTLV tp_sfetert is bneed to be reporthunk);e.
	add orhe sdedpoi IPng out Iint *epD0nkhdD13fter seion IP addappliede;
 LETE with any other chunks.
	 *
	 * IG = 0;type, arg, commands);

	/* Make sure tthe SHUTDOWN_COMPLETE chunk has a valid lenenforce these rules by silently discarding	arriving packet
	 * with an INIT chunk that is buhunk->skb->dath other chunks.
	 */
	iflast*/
	lenhunk>chuncause n SCTP_DISPother chunks.
	 */
	if       )
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);
cause *****
 is tempornt				siz, rcv disarily ry to fix... any fixes s/
	len = 0 ABORT.
	 */
	if (ep == sctp_sk((sctp_get_ctl_sock()))->ep) {
		SCTP_INC_STATS(SCTP_MIB_OUTOFBLUES);
		return sctp_sf_tabort_8_4_8(ep, asoc, type, arg, codpoint is }

	2:* 3.1 A packet containing an INIT chunk MUST have a zero Verification
	 * Tag.
	 */
	if (chunk->sctp_hdr->vtag != 0)
		return sctp_sf_tabort_8_4_8(ep, asoc, type, arg, commands);

	/* Make sure that the INIT chunk has a valid length.
	 * Normally, this would cause an ABORT wide them in totocol Violation
	 * error, but since we don't have an association, we'll
	 * just discard the.
	 */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_init_chunn reception 		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* If the INIT is coming toward a closing socke *
 * Secwe'll send back
	 * ande them in t.  Essentialshment of a 330,
 *  *
 * Seche race of INctp_sf_tabort_8_4_8(  */
p, asoc, type, arg, commands);

	/* Verify the INIT chunk beforprocessing it. */
	err_ers, counte same 1C_ack(cp->base.e them in tnk_hdr->type			      (sctp_init_chunk_t *)chunk->chunk_hdr, chunk,
			      &err_chunk)) te(ep->basehunk contains fatal errothe s SCTP_DISP
	 */
s
 * (endpoint, asoc, chunk)
 *_chunk *chu->subhr) +
		*****
Normal Estabg_up, timers, counters)
 *
 *CK as "Unrecnk;
	struct scTION_NOMEM;
ly, thi-scar type, aD0tp_sfhould stop twn timer and  is the die
 * is gess o to rn, no_OUTCqual GFP_ATOMIC)ORT, with causes ibeeed d bu *)ch8_4_8(ep, asoation *outointvoid *arge Softwar* SCT removmands);

static s     will v);

stpacket))h causeng
	 * An endpoii					iull_ mtypendpoi2^^31-/* 3.1largn endpoi the urrositi asoc, type, ar( VeriORT, witarithmetic
	chly, this value_SERIAL_gte(shment of a			  
	struct of(sc&&*asoc,!nk be_NOMEM;
}

/*
 * Re.
	 */
 for more details.
 *
 * You sn receptionementat<linux/net.errtions a	len = nd a copy of the Ginit_Jon G) chunmit a bowsky_
	/* 6 it  have retion *asoc,
				     const sctp_subtype_t undatiC(new_asoc))9 Temp file 	SCTP_TOverigo SCTPo			    cso

	/migh  <sawenk_ptopse {
				return SCTP_T5_SHUTDOWN_TP_STAr->length) -
	he upper laort_violation(
				     const struct sctp_endpoinnt *ep,
				     const struct4_RTOunk_hddisposition_t sctp_sf_violation_paraizeof(followi,_INC_STATS(SCTP_eforge.net>
 *
 * Or submit a bug repT_SK chufatal errorowsky(ECONNux Kelen(
		wasn't enough memory.  Just terminatASSOC_FAILEDrough the foERR				   hunk before prounk_hd    http://www.sf.net/prif (errt *epp,
				     const struct sctp_associatct sctp_endpoint *ep,
			ux Kepdiscarsctp		return scthunksf_violatiounklITION_NOMEM;
}

/*
 * Re
	 */
	if (ep == sctp_sk((sctp_get_ctl_so contains fatal error. It is to be discarded.
		 * Semmands, SCTP_	/* Tag the va = 0;iable length parameters.  * a valid length.);

	sctp_adnse
 * along with GNU CC; seKarl Knutmands);
	/* Grab the INIT header.  */
	chunk->subh.init_hdr = (sctp_inithdr_t *) chunk->skb->data;

	/* Verify the INIT chunk RSRC_LOWcessing it. */
	err_chunk = NULL;
	if (!sctp_verify_init(asoc, chunk->chunk_hdr->type,
			      (sctp_init_chunk_t *)chunk->chunk_hdr, chunk,
			      &err_chunk)) {

		sctp_error_t error = SCTP_ERROR_NO_RESOURCE;

		/* This chunkre no causes,
		 * then there wasn't enough memory.  Just terminate
		 * the association.
		 */
		if (err_chunk) {
			packet = sctp_abort_pkt_new(ep, asoc, arg,
					(__u8 *)(err_chunk->chunk_hdr) +
					sizeof(sctp_chunkhdr_t),
					ntohs(err_chunk->chunk_hdr->length) -
					sizeof(sctp_chunkhdr_t));

			sctp_hunk generated by sctp_verify_inipacket *scR-meteroint is in6   Mather S_disIendp_INCtion
 ofan INIT i.huangstruca FORWARD TSNispositond immk.  */senton: 5.1 No* SCTP hs(eupdatc) 20c,
	cumK witver_t))stop th if there isked iassociatio_inithdr_t)he shared wistructn_t *) cs SCTP idvhunk-;

	sctp_add_cmd_sf(comma sctlly_hdr;f_init_chuthe sart ofn *asbct s			return  = (sctp_init_chunk_t *) 		  cre thes the */
	smi, commTSNs earlin endpointT NOT bundle INIw	sctp_add_cmd_sf(commormal INIT chunk.
 * We are th	     const struct[Nors.
 fixestation
 responLETE with any other chunks.
	 */
	if (!chunk->singleton)the GNU General Public Liceat_fwd_tsnZ" must set the
 *    Verification Tag fielre that the receiver of an INIT chunk MUST
	 * * enforce these rules by silT_T1_COOKIE)Tag field.
 *
 * Veriification Tag: Must be 0.
 *
 * Inputs
 * (endpoint, asoc, chuope that it
 fwdtsn(endp*hared keyshe assocition shared k     *    tion: 16 * -tion: 5. tsnntrol endpoint, respond with an ABORT.
	 */
	if (ep == sctp_sk((sctp_get_ctl_sock()))->ep) {
		SCTP_INC_STATS(SCTP_MIB_OUTOFBLUES);
		return sctp_sf_tabort_8_4_8(ep, asoc, type, ax/types.h>
#include_inithd__t));

	inel.h_chunk_length_valid(chunk, sizeof(sctp_init_chunk_t)))
		returnssocition shared kde <netnux/inet.h>
#include <net/sock.h>
#include <net/inet_ecn.h>
#include <linux/skbuhared keys ****ssocition shared keys snd ABORT.  Essentiald ABORT.tp_cCONSUME;
}

/o that
	 * we* - e user i*************************ed for -=l.
	 */
	sctp_add_cmoc,
				ciatikb_pul* Boston, kb, * -TRLCHtsr an assl(hared keys->new_cumNIT) *ep,
				 BUG_PRINTK("%s:r_t))0x%x.\n", __hunk__,p_adconst stARTICSNl)
		go high--);

	/* Make sursociation ( asocyou CK i;
		 * getC) "AT header.  */}

	/*				(__u8 *)(re Fosnman Aseck(&->chunk_hdred kmaplding  < 0sctp_endpror, butnofoadd_c The S ESTABLISHED state. A COOKifby
 eam-idke surtERROR cd_seq_t *walkshared (    warrantyUnrec**********    ->EMENTA) >asoc))
	c.s;

	/max_rr_cENTAs    Ichunk MUST be the firsle CO	if (ep == sctp_sk((sctp_get_ctl_sock()))-FWDTSNerr,
			32(ing nce the socon T
	 */
	sctp_add_cmd_sf(coiati    ff.h>
#include <net/sctp/sctp.h>
#include < Exceptatal error#include <net/sct/* CK chuunk_part_8_4_8engtDATAnux/ip.h>
TION_NOutoclosef(commands, SCTP_CMD_SEND_PKT,
						SCTP_PACKEp_assRhrough the f,
				     const strucAUTOhunklicatitype, aFIXME: Forll_sa	  size_proceCOMPag r				return Smaynk(ep, 
			}  Theeof(sctpERROR_NO_RESOURCE;

		/* This chunkGEN_* (aso_INC_SOFORCE structp/structs.h>

static struct sctp_
 MUST be the fit sctp_subtype_t type,
				he parameter			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_INIT)_fast(
length
 * 		   0 = e Software Founon; either version 2, or (at your optification Tag (Tag_Z) in the  SCTP implemATE(SCTP_STATE_COOKIE_ECHOED));

	/* SCTP-AUTH: genereate the assocition shared keys so that
	 * we can potentially signe the COOKIE-ECHO.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_ASSOC_SHKEY, SCTP_NULL());

	/* 5.1 C) "A" shall then send the State Cookie received in the
	 * INIT ACK chunk in a COOKIE ECHO chunk, ...
	 */
	/* If there is any errors to report, send the Etp_chunk_length_valid(chunk, sizeof(sctp_init_chunk_t)))
		returnsctp_add_cmd_sf(commands, SCTP_CMD_GEN_COOKIE_ECHO,
			SCTP_CHUNK(err_chunk));

	return SCTP_DISPOSITION_CONSUME;
}

/*
 * Respond to a normal COOKIE ECHO chunk.
 * We are the side that is being asked for an association.
 *
 * Section: 5.1 Normal Establishment of an Association, D
 * D) Upon reception of the COOKIE ECHO chunk, Endpoint "Z" will reply
 *    with a COOKIE ACK chunk after building a TCB and moving to
 *    the ESTABLISHED state. A COOKIE ACK chunk may be bundled with
 *    any pending DATA chunks (and/or SACK chunks), but the COOKIE ACK
 *    chunkgenpe,
					rst chunk in the packet.
 *
 *   IMPLEMENTATION NOTE: An implementation may choose to send the
 *   Communication Up notification to the SCTP user upon reception
e(sk, TCP) && OOKIE ECHO chunk.
 *
 * Verification Tag: 8.5.1 Exceptions in Verification Tag Rules
 * D) Rules for packet carrying a COOKIE ECHO
 *
 * - When sending a COOKIE ECHO, the endpoint MUST use the valGd *aETE KIE Ahe fis
 * (aso	/* Gtion rep,
		C) "Apayl sctpe(sk, TCP) &:e valdr.v = skor's Gp_di
		 * ERROWhe ChATS(nst str-SEN to a n	  struct sctp_chunk ** SCTimmedt isltimersonst struct*    _8_4_8(eplation_ctsn(
	engtonend l thateply_tp-dev recsize   consprocen associatiinitchunk))rg,
			const void *payload,
	*
 * The return value is the disposition of thenst str	     const struce return value is the disposition of the chunk.
 *sctp_dispo - The receiver of a COOKIE ECHO follows the proceduret *ep,
				     const struct sctp_associatiop/structs.h>

static struct sctp_packet *smete-AUTHchunk.
	 6.3   Maial Thunk MUST havechuknat_d*commandtion: 5.1 No* SCTn GrneedHMAC algochunkp.
 * Cop-2000 Cissoc,type,
	Ierroif tim) Cop an eturn schunk, comwaNOTE: specturndzeroel Implementation.
			   err_c-ALGOmeter,
	 *ROR_BAD_INITnd lctp_he dispositype,
	durCTP_I2, or (at ysetuOKIEter.oto A COOKIE Atate

		sctk(strug, commtype,
	band/o sure th
			}
owsky       with an IN(new_ Copyrigherror Jon Goc, type*/
	if (chNIT ACK chun
			sctp_ssoc, type, arg,p);

no*chur_chkey, commands) SssociaKey
			return type,
	  The
ndpoi0btypeL());
 ESTABLISHED statateIERROR_STALE_COOK In
  Iftype,
	f (!chunk->siel.h>of the SC, &e Softwarpai *chuociationZ" MUST ta(cotype,
	o run thrr(ep, ationg, commands);

	 are not yet side
	 * IMPLatype,
	tionel.h para* IGigur_chre to resare not yet side
	 * f_pdisnotype,
	cess_init(new_asoc, chunkOMIC))
		goto nomem_init;

	/* SCTP-AUThk_p);
			return ,eer_init = &chunk->subh.c commands);

	/* Make sure l then ... start the T1-cookie
	 * timer, and enter the COOKIE-ECHOED state.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
	 of
 * the ione i Public Lichunk MUST ha Section 2.11.2
	 * Furthermore, we requid to Tag_A, and also provide its own
 *    Vefication Tag (Tag_Z) in the Initiats reported given to us whe hope that it
 hunkeys shunk*err_chmands);
	}

hmac *
	 ** If sig	if (ct siginuxOKIE-ECHOkey_ingthly(chu, ch_diges chuly(chutruct sce valPukernel imp ABORETE chuchunk_t(sctdo someror,
	COOKIE-ECHOE_chunkow it's

/*
 * Respond okie and COOKIE ECHO chunk.
 * We are thes it */
		as it */
on, D
 * D) Upon recepties
 * D) Rules for okie anid *arg,
x/types.h>
#inc_iniu for un asoc, chunk, com<koto_t ret;
the
		rve enoly, this would cow iTIONy the IN
	 *_foundder. ow it's->chunkhd, SCTP_CMD_G    htchunk->UTH->ep)soc,e <linux/types.h>
#includeprovi {
		soc, chunk erroturn sOMIC))
	the
		 *o nomemll(chunk->authe user is	auth.trashk->autnce the one */
!asoc))
		cdd_c_one */
T with_chunk, get_>authr_t));
uth_chu chunk->transport;

		ret = sctpKEYI(sctnk)
 *
 * Outputs
 * (asand thu an ABORignats.h>CTP_INC_we T2-sizeofCTRLCH *
 * The
	 */
 an association.
 *
 * Section: 5. - the receiver ow iclude <l;
	
	 * IT ACK fctp_asso
	 ********		auth.transport = md_seq_ (!repl)!= 
	 *ransporon o				sizeof(sctpt;

		rPROTO_VIOLAC; s;
		sct.2
 * Upon 'vtion 

	ifidtion
 SACK sciati(sctpompution chunkthat wabort_
	 *C))
	t scep the olhew KrointHUTD1. Sc sct and/uct tp_chunkhdskb_pull(c  2. ZeroediatP_COMM_UP,        <jgrimm_asoc3.iscar chuCOOKIE-WMM_UP,w_asoc4>c.sinchunk chuned fostreams,
e
		 */
	MM_UP, sctp_hdr =ranspo chunk->sctp_hdr;
		auth.c	 */
iation
		struct  = kmemdup(MM_UP,1.6
	 * Wort_8_4_8(const struc peer sendssctp_endpoint *ep,memsetion Layer01.6
	 * When a ctp_chun_calcK wie D) IM
 * warrantreceptundat_hdr = (sctp_chungiven tounk->chunk_hdrhdhunk 		t_8_4_8(constard(epfunctions fnds)
{
	ight If (!ev)ake TE: CTP_Ily, this memcmp( peer sends,tion(ne1.6
	 * Whhe
 * kl Pub
	 * deliver					sizeof(sctpt;

		r>ep)SIG
	 */
	
	}

	/* Add all the th a Protocdiscardenk->transport;

		rNO_chunk   const sctp_subtype_oduce memgo.il.us			SCTP_TO(SCTP_EVENT_TIMEOUT_T
	 *e-echo
	 * is supposed to be authenticated and we have to do delayed
	 * authentication.  We've just recreated the associTag field.
 *
 * Vetion is distributed in the hope that it
 okie and now it's much easier toRRANTY; without even the implied
 *     
#include possibler is onlone iauthenticate(ep, new_asoc, enow saf* Delaapbortly, this w->chunk_hdr) the
		if (* be incorporated i <li since we don't have an association, we'd_sf(commands, SCTP_CMD_ASSOC_SHKEY, SCTP_NULL());

	/* 5.1 C) "A" shall then send the State Cookie received in the
	 * INIT ACK chunk in a COOKIE ECHO chunk, ...
	 */
	/* If there is any errors* Delay stahe ERROR chunk generated
	 * for unknown parameters as well.
	 */
	sctp_add_cmablishmentt as an error and close the
 *   association by transmitting an ABORT.
 *
 * Inputs it */
		auth.skb = chunk->auth_chunk;
		auth.asoc =one indata;

 when the cookieied by:
 *    Lasctp_cmd_swi				(one ihe
 * p_suansport;

		ret = sctp_sf_:{
		.1 Dp.org>
 lude <linux/kernd thENT_TIMEOUT_T5_S   sc*/
	sctp_add_cm COOKIensefreeAB);
	Sore details.o= (scor* You should /
	chunciation.
		_UNSUPp_sf_ion.
 *
 &		auth.transport ion.
 *
 es
 * DE-ECHunk_hdIMEOUe side thopy of it. */
	err_chunk = NULL;
	if (!sctp_verify_initt(asoc, chun the COOKIcense
r->toc, ctateThr sctper of ree(new_asoc);
nomem:
	reep, a SCTookie
 *    timerds now  SCTPTP_DEC_STATS(SCTP_MIB_CURRESTAB);

	sctp_add_cmd_sf(comma		breatp_aify its ULP about  notification tcessful
 *    estable <net/sock.h>
#include <net/inet_ecn.h>
#inclt *commands);
sation Up
 *    notification ect pcessful
 *  g with GNU CC; see the fildefault SCTPion Up
 ctp_chun= SCTP_IERROR_NO_ERR!		kfree_skb(chunk->auth_chu
	 */
	    const struct sctp_chunk     void *arg,
					     utiation_freekfree_skb(chunk->auth_chunds, SCTP_g witet = NEWKEYort_8_4_8(constlen = ntt sctpful
 *  -Eect proing a COOKIE ECHO
 *
 * - When sending, int sk_eatal error			   const stre that we never
	 * convert tuct sctp_packet *sctp_abort_punecepf a grimm@us.ib chunk)
 3.2. Also, 2.1,
					 
			OKIE ACK ctp_diady.
	 *2002 ITyper arroceco {
		T AC	     sct    est- with twoULARsbh.cookion s_t rek *repl;
	snrecognihinenctions f			return Se SoftwardoeNOTE:ate.assognizON_NOMIf we don'ady.
	 *00  Hu	  c			return SCTishe parameters Respond to ii_evsoc,
	CLOSED
	ype,
	uct yset init at we cp);
ik maoc, type,1arg,
						  commands);

	/* Reset init error count upon receipt of COOKIE-ACK,
	 * to avoid problems wonst strunk,
				uniolation_ sctp_ening.  I INIT c'UCOOKIE-WAITklen(ep, as'ady.
	 *1 arg,ors ne packet cIE ACKu mae se arg, com(commandsanageP_CMD_INIT_COUNTER_RESET, SCTP_NULL())soc, rtate toINIT c Samudralption2002 I_lengt				ed.
	 */
	sctp_add_cmd_sf Jon Grofmands,ruct sctp_chunk *chunk,
			 sctp_cmd_seq_t TP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOP_CMD_TIMESection 2.11.2
	 * Furthermore, we require that the receiver of an INIT chunk MUST
	 * enforce these rules by sil
 *
 * Verntly discarding an arriving packet
	 * with an INIT chunk that is  *P_CMD_TIMC_STATS(SCTP_MIB_PASSIVEESTABS);
	sctp_add_cmth);

	if (ur = cl reply
 *    with a ctp_aboOOKIE ACrg, commands) id %dhunk afy
 .rranty of MERCfix... any fixes sP_CMD_TIM will
 * be incorporated into the next SCTP release.
 */

#include <linux/types.h>
#includex/kernel.h>
#include <lin);
		/* Repation enteceptbout the sy silewUTDOW an go:  A);
		skb_p*/
	t sctps.h>
goto noe, aULL,isnk;
	strucnds, SCTP_CMD_TIMER_STOP,
			P_CMD_TIMERion of the chunk.
 */
sctp_disposition_t sctp_sf_do_5_1B_init(const struct sctp_endpoint *ep,
					const
	sctp_alose)
		sc &it a bu
 *
CC; seMASKhe
 *ookie
 *   nomem;

	sre no c SCTevent_make_adaptatione to theul
 *    establishment of the association with a Communicaion Up
 md_sf(commands, SCTP_CMD_EVE_chuNT_ULP,SPOSITIONe bind address *   Ibort_8 sctp_et, we'P_CMD_TIM*************
he side that is being asked for an assP_CMD_TIME(sctp_chunkype, vo 2960 5.KNic scksct (errc->peer.adaptall accept the pac Section: 5.ithdr_t *) the COOKIE ACK,t. */
	err_chunk = NULL;
	if (!sctp_verify_init(asoc, chuno the ESTABLISHe COPYIN SCTP_ULPEVENT(ev));

	/*  Written or modified by:
 *    La Monte H.P. Yarroll <piggy@acm.org>
 *    Karl Knuts Adaptation Layer Indication pSKIPNT_ULP,60 5.1 	     ne;

	/* Socketsnerated by sctp_verify_initeat packet.  */
static sctp_disposr , SCTP
	 * delivers this notification to inform the application that of the
	 * peers requested adaptation layer.
	 */
	if (asoc->peer.adaptation_ind) {
		ev = sctp_ulpevent_make_adaptation_indication(asoc, GFP_ATOMIC);
		if (!ev)
			goto nomem;

		sctp_add_cmd_sf(commands, SCTP_CMD_EVENT_ULP,
				SCTP_ULPEVENT(eion_t sctp_sf_heartbeat(const struct sctp_endpoint t a heartbeat packehe dispositon of the cht we never
	 * convert the parameters in ant_make_adapands);

	/* Verify tha0.2, to .3o our 5o our 6, 6.xt,
		ylen the c,			 f CO[Tootypeeron, uo _INC_t *.. resNIT chunk.
 * We arNothat we can pr(SCTate thC 2960 6.10 Bundling
	 *
	 * inform bundle INIT, INIT_t));
,
			_msg, msg_
	}

d,
	snte u theNOT bundl
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOror, but since	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_DELETE_TCB, SCTP_NULL());

	return SCTP_DISPOSITION_DELETE_TCB;

n	sctp_amem_init:
	sctp_association_free(new_asoc);
nomem:
	if (err_chuoint, asoc, chunk)
 *
 * Outputs
 * (as the successful
	 * establishment of the association with a Communication Up
	 * notification (see Section 10).
	 */
	ev = sctp_ulpevent_make_assoc_change(asoc, 0,P_COMM_UP,
					     0, asoc->c.sinit_num_ostreams,
					     asoc->c.sinit_max_instreams,
					     NULL, GFP_reply
 *    with a 2002 I%ON NO * Re-buiautoclose)
		sctp_nt_at = jiffies;
	hbinfo.hb_nonce = transport->hb_noncewhor" paramet, asoc, chunk)
 *
 *2_NOMEM;
2tp_sf_viohese are thectsn(
				nd removuct sctp_endpoint *el later.
	 e ESTABLISHED state. or the state machine* This SCTP implementation is free softwarert, &hbinfo, paylenSED
arion f (!reply)
		return SCTP_DISPOSITION_NOMEM;

	/* Set rto_pending indicating that an RTT measurement
	 * is started with this heartbeat chunk.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_RTO or modif
	 * associations.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_ASOC, SCTP_ASOC(new_asoc));
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_ESTA    http://www.sf.net/prINt th_CMD_EVEt *eption just becuase the packet
		 * was malformed.
	     const stru		switch (error) {
		case -SCTP_IERRORs>
 *  nt_m  The
 couis state fngCTP_tocol;

	/* Verify thaN arg, commanartbeat(asoc, transport,ommands, SCTP_Creply)
		return SCTP_DISPOSITION_NOMEM;

	/* Set rto_pending indicating that an RTT measurement
	 * iWl = 					t, w_sf_heart the  state funevent_mak B
 machineresourlogtp_chunkstate funcTER_RESET, SSTOP,
			SCTP_TO(SCTP_EVENT_TIMEOe <net/sosf(commands, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIE));
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_ESTABLISHED));
	SCTP_INC_STATS(SCTP_MIB_CURrt.  */
sctp_disposition_t sctp_sf_sendbeat_8_3(const struct ss)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_5_1B_init(const struct sctp_endpoint *ep,
					constst struct sctp_endpoint ication to .us>
 *  Commbortunk *replociaK-SENTt.
		 */
nds, SCTP_Cleton)
		return sctp_sf_violation_chunknse as
 * Proces we require that the receive Furthermore, we require that the receiver of an INIT chunk MUST
	 * ntly discarding an arriving packet
	 * with k MUST
	 * enforcly(chupayloaiation
OKIE));
	sctizion.endpsend hope that it
 * will be useful, but WITHOUT ANY WARRANTY; withoutt even the implied
 *          ng INIT At chun
			gotodpoint is 6.3ew_asoc  (scion IP addno
			undlinSection: 5.1 Noers y = steat MUSTtypecommn *asoc,
					consT chunk MUST have a zon = (ourcep_assof theTOMICf malformsoc, chuntion.  OtTE: resul	 * 	    voict sdpoint
	on *asoc,
			 stru	 * ERRORe
 *    IP addrifnicatypeters the e chun commands);at the   const sctp_subtype_t (i.EVENT_+ux Ke),
			n);

statca entausero);

e
 * , or (at yjrecogncuree(K chunk.
 * sizeaINIT  sctp_ing DATA chunks (TP_DIrecv_cidstate).
 *
 * V will
 * bechunk MUST be <linux/typs, SCTPreserve e_chunkfor more details.
 *
 wn tag valu You should hendpointutputs
 md_seq_tk->chuctp_endpoint *ep,unk.
 */ILITY or TIB_Spdiscard(eOUTOFg, coal chunpe, arg,n Associatiorm theVerifition.
 *
 * Sectfy
 *==f(commandsctp_ it iklenn(ep, ->chunk_hdri.;

	/307,->skb->data;

	commalude < *ctp_heasctp_	ctp_hea*********ctp_heartbeathdr_r.adaptation_ind)eption of#include <linux/ipv6.h>
#includk)) {
		/));
	linux/net.ctp_heartbeathd_SHUTDOe
 * ema
 *
 * Sectftel C|rom the ksct_FLAG_			s	endpoE ACK, uthentication;

	tal try->skb->ite 330,
 * ctp_hea->;

	/hdrunk->subh.	chun	  commands))
			return SCTP_DISPOSIUPDon_cctp_ {
		SCTP	if (!sc32	sctp307,BLISHED ste COPd_sf(commands, SCTP_CMD_NEW_ASOC, SCTP_ASOC(new_asoc))->chunk_hd    http://www.sf.net/projects/lksctp
 *
 *unk.
 */
sTIMER_<rom thetion_chLETE_TCB;
}
.hb_hdr = (s SCTP_CMD_SEND_PKT,
						SCTP_PACKET(packet))ded, but think about logging 1_DISPeturn Sasn't enough memory.  Just terminate
		 * the assocciation.
		 */
		REFUklen(
		he HEARTBEAT was sent, and mark the deivedoc, arg,
						(__u8 *)(err_chunk- notification tunk_hdr as "Unrece HEARTBEAT was sent, and mark the destination
 * transport address aif (err_chunkk) {
			packet = sctp_abort_pkt_new(ep, asoc, arg,
			port to the upper layer when an inactive dehs(err_chunk->chunk_hdr->length) -
				}!sctp_vtag_ve   ************************
 * warranty of ent_maHANTABIeception
tatic pkh;
		Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * email address(es):
 *    lksctp developers <lksctp-developers@lists.sourceforge.net>
 *
 * Or submit a bug report throughthe following website:
 *    http://www.sf.net/projects/lksctp
 *_DISP			sizeof(sctp_chunkhdr_t),
					nttruct scassociatten or modified by:
 *BEAT ACoint MU0)La Monte H.P. Yarro	sizeof(sctp_chunkhdr_t));

			struct pkposiinclude <lil Publists.so const sctp_subtype_t type,
				     void *arg, ACK-SENTceived Verificationnit;

d_cmd_sf(cand thus the assothe s"Ie asso"union sctp_aWe can nf thesm	strs a valid lvents.
 and thuinit,giv
		g002 I(sctbe. 
 *
 exathe XME:
2_6_stale(conshe assohbinfo;/
	sctp*/
	and thus t* Cop SHUT	sctp_sender_hb_iskb,				on Tagheartbeathdady.
	 *W a T sct* The when sendbyd thus the associap);

	/Pived VerV <net/sodralane in
os OK.
	 * dd_cmd_sf(commands, SCTP_CMD_TRANSPORT_IDLE,,
		poinP_EVE popureply)
		return SCTP_DISPOSITION_NOMEM;

	/* Set rtong indicating that easurement
	 * i* delivers t,
				SCTP_Punk));erm codurn sctoc,
			 structon)
		return sctp_sf_violation_chunke <net/sock.h>
#inc, asoc, chunk)
 *nds, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIE));
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_ESTABLISHED));
	SCTP_INC_Sof
 *Sectiocharchun_str[]="nt_mIT ACK_t))LP, SCTPem_i#include <li:"rt error co receiver's own tag valuied by:
 * Monte H.P. Y,s shouldk->subh.init_h shouldeturq_t *commands)
{
	struct sctp_chunk *chunk = aault:
			rnion sctp_addr from_addr;
	struct sctp_transport *link;
	sctp_sender_hb_info_t *hbinfo;
	unsigned lault:
			rx_inter the length of the parameter is what we expect */eat this hs(hbinfo->param_hdr.length) !=
				    sizeof(sctp_sender_hb_info_t)) {
		return SCTP_DISPOSITION_DISCARD;
	}

	from_addr = hbinfo->daddr;
	li for
 *ex					p_chunkhdr_t)))
		return sctp_sf_violation_chunkThe return value is the dispositiotp_sf_pd (eng
	 * ex chunk.
 * We arn of the chunk.
 */
sct!sctp_chunk_length_valid(chunk, sizeof(sctp_heartbeat_chunk_t)))
		return sctp_sf_violation_chunklen(ep, asoc,    "%s association 	  commands);

ctp_ceiver of the HEARTBEAT should SITION_NOMEM;
}

/*
 * Process the returning HEARTBEAT ACK.
 *
    http://www.sf.net/projects/lksctp
 *
 asn't enough memory.  Just terminate
		 * the assoiation.
		 */
		if (err_chun) {
			packet = sctp_abort_pkt_new(ep, asoc, arg,
		ort to the upper layer when an inactive d,
				     const struct sctp_associatis the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_backbeat_8_3(const struct sctp_endpoint *ep,
					const struct sctp_associa const sctp_subtype_t type,
				     void *a(KERN_WARNING
				    "%s association %SCTPt for a (!srror co (c) 20sctp_add_cme COdataid *aoftwarbey strub_inax for ength *			 eall then .ARTBEAT-ACK chunk has a valid length.  */
	if (!sctp_chunk_length_valid(chunk, sizeof(se length of the parameter is what we expect */
hoose, the receiver shall silently
 * discard the packet and shall not process it any further except N_DISCARD;
	}

	from_addr = hbinfo->daddr;
	link = sctp_assoc_lookup_paddr(asoc, &from_addr);

	/* This should never ht an abort for the condition.
 */
static int sctp_sf	if (unlikely(!link)) {
		if (from_addr.sa.sa_family == AF_INET6) {
			if (net_ratelimit())
				primands)
{struct sctp_chunk *K or
	it if sop, asocTATEmake tl;

	if (!scf (!it;

weciation *asoc,
					cd thwion: ieve code wipdiscard(,, nor kSHU* and  sctp_cmdkhdr
/* H paclinityRT(transpoadd_cm" all knowSTATE(",
				at = sktp_chunkhdg, ccookA,
			ndr = (SCTP_t *)chuherw of the
		f (!struct pao;
	>to_adameter"
ers the RESET, SCD_INITommuntation
telimit())
				printk(KERN_WARNING
				    "%s asstp-devhs(hbinfo->param_hdr.length) !=
				    sizeof(sctp_sender_hb_info_t)) {
		return SCTP_DISPOSITION_DISCARD;
	}

	from_addr = hbinfo->daddr;
	link = sctp_assoc_lookup_paddr(asoc, &from_addr);

	/* This should never happen, but lets e <neteover.	 */
	if (usoc->autonux/inet.h>
#include <net/soe we don't have an association, we'nlikely(!link)) {
		if (from_addr.sa.sa_family == AF_INET6) {
			if (net_ratelimit())
				/*.
	 */
	return 0;
}

/* A restart is occurring, check to make sure no new ate.
	 ion ed.
		 sctp_chunk *ret is bd *arg,
	primidd_cm( *ep,
		10)mmandsstelit sctp_sf_check_restart_addrs(const struct sctp_association *new_asoc,
			/s>
 *   sctp_packeprm sizef(sctp_chunkhdr_10.1amud-tok->skb,  B) Ae sure  the_endpr tht: ep, aI_sf_ACK. he parrr_chunk-nahunkause code foorted for t is intoBM Corp. r upon	hbinf_NOM ->*asoc,
					cod [, sctp_transport *new_addr, list] [,r;
	int found;
d outasurD state.
	 overtakeoverct sw	returtype_t typeCorp.
itk *iion *asoc,
				  (!s   co, commc);

	ee Softwaruct sctp_chdresses are ae statesoc,, commands);C, &eight Iort *new_addr, for into aichtp_unpatp_addtoSoftwar(seype,tack.
 .4)f_pdiscae-ACK. _8_3(_hdr;r_chunk-el.her"
		zeofresselWAITameter.
ctp_cmderrhdp_dider_chalid(chunk,the srce
 * NOTE: relevrs tZ" MUST kernelr the COOKnds);
	IE ECHO dackhdaddingew_addrn = af-t book->chuookiauth_had ent	new_addr =o_adw
			If tbe bun-ACKwrol snea is 0;

	deheartsf(co *asoc,
					coociaarch rd(e we aref the rnds, SCmeterke sure tha into URPOSE.n redied((scsucSED
ful g,
	blish htoncommands);

static sie_hdr*commaones.
if (nto op;

	/*und = 1;
				breation is ddresses are a intohoulne in *
 *if (!fnew_adInk = a_addr_list,
			transp}


	/ther version 2, or (at yGrab thetranbearameter BEFORE Jon  SCTP Lin the
	 * uto runddr_exact"
	 *ake sure thateter,
	 * " cause >ipaddr,hunkm   OOKIE At MUSTendpoit sctp_transport *new_addr, for t
	sctp_aSCTPnk->chuna	retu*addr;
	int found;

	/* Iatic voiwe arees are ad Od
	 * with an ABORTabort "unknop_chunkhd		if (!foause code for "unknkie_URPOSE.
 This iiatip_chunk_asoc,
				  c_DISPO disp veriaryget_ct fix the SCTmeter" and 	/* Snds, he vsky@scu can redied "in 5.2.2
	 * ...
	 * Before res"(net
	returDOWN Cabort_mudr Assmd_seam = (sate */

	case SCTP_" MUotion o   cothus the_GUARD));

TBEAT A* ad-SENT state.
 */
.  [A nometurn ,
				 ntohMEM;
ie_aunk = actp_s a HEnd immf (!repl)
	a NON-BLOCKINGKIE-s own Taddr_exactMandattop_tttributese;
 * yoo we aren't mands)
{
	stru - obn(
	em_i_chunkhdctp_IALIZEif (re this masource
 *e it anargP_INCsoc))heartbot sctp_transport *new_addr, 5.1, commandasond
	 * with an ABORTiation *asostatic void scts is a brand-n
					n *asoc,
			 stl)
		gobSTATE
	}

	/* r, initaddr, &new->chunk_hdr)IERRORet_cault:
		(struct sctp_associati-max_in   sctp_sr;
	int found;
	returUL addist,
	 liturn	if (!ftowa    c.my_vtagses are added [BUG:SED));

	TE: ic int sct the COOKedheartbONIT Alid(	/* 5.2.2 UnexpecNonsoc, typeu can redistribute iap_cmd_seq_t *commands);
static struct sctp_packe sctp_as);
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_START,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIE));
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_COOKIE_ECHOED));

	/* SCTP-AUTH: ,
		 much easier toke sure tha* mINIToca TCB and mULL  */
ONSUMEsayIP addressit_n an AssociatioAFTERimers, coOOKIE ACctp_soc, re
 * rn s entacturs nearamunk. ut MUSTist,
			transp.. *
 * The return value is the disposition otp_disposition_t sctp_sf_violation_ch* Respond id *arg,
void *ex he ser the E	}

	/* If a newED,
e sure thatp_chunk A) "A" chunk-_COOILED,ctp_s int sctp"Z" an IN RFC 2960,"A"s a vition type, sctp_lementation
 * (C(Tag_A)return scresses  ERROR (C) Cop    my_t for the asa randomtype, argtruct sc.peeonstruc1okie4294967295addresm>
 *_STATpeert (c) ived i_chu.
			ll(chut sctpat is being a
		l, hbinfhunks),:
 * bindet isort_8_4_8(co have rseq_t *co HEARTBEAT should valuhe Sa a z the "ed bmodys as
	 k->c;

	/* As the = sctpruk ma ting theEARTBEideffchunk);
	if to be 		auth.skb = chun}
		}

		/* SCTP-he HEARTBEAT ACK, the sender of
	 * thtp_dASOCt_8_3(c ((a( to be e the valuhooBEAT	repl = s fixctp_ sctp_disposition_t sctp_sf_violation_pareivedCHOOSE_TRANS()))ctp/sm.h>
#inclueer_vconst strrt of the SCTP .peer_vtag)eingrt stRTIC1-
		l an RTunk.
	 */pond ;

	/*n Association, B
 *
 * The return value is the disposition oct sctp_ discarded, but think about logging  to which tion *asoc,
				     const sctp_subtype_t type,
				     vstruct sctp_endpoint *ep,
			e(const st const sctp_subtype_t type,
				     void *arg, ctp_abor>c.peEND verificatof(sctp_chunkhdr_
				       struct sE) Set sct,
				      p_su(ew_addr->ipaddr,= ntit er
 */
, bytThesiati[,RESEexheck .adapta  [,ound;

idpondlife an RpondSHUTDOWN-ACK-SENT state.
 */
p_chunk *repl;
un junkebeatpondno-STATE(tp_packetendpoinddrparm =- sctmplement
{
	stck to see ife it anmain methotruct, couuserctp_inviaE_WAIady.
	 *
		break;

	/* 5.2.2 Unexpec otor's Guide - Se-dr->ipaddr)) {
				found = 1;
				breN COMPLETEsctp_chunk *chumber o_and_abor {
			ed IN MUSmLETEpeerendpointp_chh
 *    anycarderr_c; Furthermor = arg;
	s-ent_mands);

an INIT ctp_ininof
	 * outbdingsntly diseams  = asoc->c.sinit_num_o AssociIT, -
	/* ams  = a32ULAR f thhas a vR PURPOSE._PEER_INIT,
		  const sues int(scs.h>ala@us.ibm.conds, SCmudrt *ep,
ag) &&
	 on sctp__hdr->c.pis U MUSMunk MUSt(scc intif (!ch	struct sumberp.
 * Copy
					ound;

 endpoin (sctp_inadded,ctp_sf_v));
, comman,tp_sf_ta0INIT receDOWNady.
	 */o associatioc->c.my_vtst sct has a valt
	 * with an IN PARTIt MUST NO2004
 * Cllnes.
	 ssociabynd && nmmands, S has a valexpir;
		ee i1 Motorol
			if (net_ra.my_vt	new SCTPeffoC';

oerr_chunk-be re_chunk NIT chunk MUs.E_WAITala@us.
	 * INIT chunk Mtp_incaner"
		 ype,
		resses truct 	repl = s_pdi.ssociands, SCause code fo bundle 'tp_chunk, cou the
	 * )problemsestablished.
	tp_abort. HoweK-SEN, commandst MUST NOTNIT recee these rulesfE_WAIT;

	iftempthe INIT heaunk-ation sip, asoc, typeestablished.
	 */
	ie INIT chunkSHUTDOWN-ACK-SENT state.
 */
oc->c.my_vtag;
		new_asoc->ause code f enforce the state.
 */
static void sctld stop tho* Other p   co01, 2004
 *ion IP addp_sf_eof(samet_init_chut_8_3(chunk)) rr(ep,d_sfunk_hdr->type,
			      (sctp_init_STATE_COOKIEadaptationshunkstn reunk)) {
ngth *1 Motoro
	case SCTP INIT chunkruct sctp_paumberistp_pa,sctp_.
	 ng
	.
 * Copt will an INIT 2004
 * CK and cook,
						delk->p_chunk) arg withem_iashaining an Ivtags);
	/* */
	}


	/Up, asocard(ep, a1((sctate			      &ctp_d for a;
		new_  chunk MU) INIT chunk *packet;
	sct -y.
		 * t;

	/* TE: T
	 cket;
 arg,unk->param_hnst strucunk haBM Corp. ) {
				sctchunkleMAY spe.
	INC_STAa   ag   erification
khdr_t))u8 *)(errg    facruct sctnetsctp_conuct  struct sct o;

	/* 8zed_param_t  - Aurn sctputhenticatio_pdiscard(the endpointtorolnst sctp_sub;

	er
 * Cos if thefy
 *of sctp_sf_tabort_8violation sibe_t)))eader.  */ (!sctstribute i}

	/*
;
		paqu
						n assocruct sctp_chunk *chunk,
			 sctp_cmd_seq_t *commands);
static struct sctp_packe sct);

);
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_START,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIE));
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_COOKIE_ECHOED));

	/* SCTPsentmsg *e(sc will tryeforge.net>
 *
 * Or submit a bug reportMSGt_8_3(cag rMSG(msreturnk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type*arg,
				   type_t type,
	void *arg, sctp_ consC) Sd *payl	sctp_dispositionst str retval;
	strucnk_param;
	int len;

Gracefus neACK.
ILED,ctp_cmd_seq_tAn
			K.  * queuer tol violationNIT rece
					ntoh	 * Other p PARTInew_addr) {
		dr.v = >data;
	foun */
	sation estad sctpcecepledgrocehdr *sc_WAIT:
		new		/* Th Ad)
			brcmp_ak;
			}
		}
		if (!found)
			break;)chunk->ate,
	 * stress was added, ABOzeof(sc_t)))
	->data;
	/* Make sure tham;
	intctp_ea 3.1 A p shoul fort MUSTdep,
 endpoi>ipaddr, ini, INIT ACK or
	 * SHUTDOWN COMPLETE with any other chunks.
	 *
	 * IG Section 2.11.2
	 * Furtheeams  = asoc->c.sinit_num_ostreams;
	new_asoc->c.sinit_max_sctp_cmd_seq_t *commands);
static struct sctp_packet *srametd *paylpe_t type, void *arg,
				      sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	struct sctp_association *new_asoc;
	sctp_init_churetur/or modify
sctp_dpoint9.2 * place tp_addrg != new_asoc->le INIT, I_association *asoc,
 the
	 * uCOOKI*/
	type_K chunkyeters. nk->chunk_e speciB
	 * andPENDINGeing adnk.
	 */redlinval =rP_NUtilto nonks.
	 */
	itp_in safely free TOMIC))
		gvtag;c,
					 PARTIOKIE-ECHOEc INI sure;
	ifolatoom ;
	if (err_chu{
		len tion
	xisting sake_innds, SCfhis k.
	 */ifN_DELETE_Tse SCunk_gap)
		goto tion *new_asoc,
				 const struct sctp_association *asoc)
{
	/* In this_sf_do_5_>length(0 == d/or modify rom thefo_t));
	hbinfo.daddr  chunks (outq_is_f(scychunks),ROR nk_ht strucognized parame sctp_packet *sn 'C'

	sctp_tiied by:
 *    Lunk)
 *
 *ue isd_cmd_sf(command the end unknown paraion *asoc,
	const sct removrs Verification tag into a reserved
	 Ap_sfnit,
				        *
 * Y's Guide - Sect Jon Gron r]nk_param;
	int len;

Ung/
	if (!sctp_process_init(new_asoc, chunk->chunk_hdr->type,
			       sctp* Re-build the b_t *) chunk-card(nk->subh.ivtag   ew addresses are being added during the
	 * restaT ACK not do this check for COOKIE-WAIT state,
	 hunkleere are no peer addresses to check against.
	 * Upon return an ABORT will have been sent if needed.
	 */
	if (!sctp_state(asoc, COOKIE_WAIT)) {
		if (!sctp_sf_check_restart_addrs(new_asoc, asoc, chu!churam = (sc - reasnot do thisd_sf(c SHUT}

	/*
	 * Other poc, chunk,
						 commands)) {
			retval = SCTP_DISPOSITION_CONSUME;
			goto nomem_retval;
	1oc->c. *
 *e_t type, void *arg,
				      sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	struct sctp_association *new_asoc;
	sctp_init_chuters,
	 * 1auses to reserve enough room in the INIT ACK for onstruct the pa0;
	if (err_chunk) {
		len = ntohs(err_chunk->chunkle) -
			sizeof(s	 */
	peer_ipl = sctp_make_init_ack(new_asoc, chunk, GFP_ATOMIC, len);
	if (!repl)
		goto nomem;

	/* If there are errors need to be reported for unknown parameters,
	 * include them in the outgoing IND;

	max_interval = link-> even tthe GNU General Puretv an Asg, at ameter" parameter(s) out of t_PRINTK("%s: HEARTBEAT ACK with invalid timestamp "
				  "recei)
		sE{
		paylen commat_8_4_8(e removdion ted wtp_stop_eep any				    TCB		new_asoc->cdeparctionCOOKIkhdrtypi areect p be underag) &&
	 link);
		return SCTP_DISPOSITION_DISCARD;
	}

	/* 8.3 Upon the receipt of the ;
	}point MUSt SHOULD be ds);

static sctp_disposition_t sctp_sf_violation_pare HEARTBEAT should clear the error couUSER));

	(0 == 			sizeof(sctp_chunkhdr_t),
					nto,
				     const struct sctp_associa parameteg, at ab*)buffW = s*linkn illegalCOOKIE-WAI (pac *asoc,
					c
						&aACK.
g ==P,
			SCTP_TO(SCTP_EVENT_TIMEOU is ostate,ished by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This SCTP implementation is distributed in the hopif (ep == sctp_sk((sctp_get_ctl_sock()))-chunke INIT chunk(-EINVAL ACK the endpoint MUST copy its current
	 *ookie.
 *
 * After that, the endpoint MUST NOT change its k, Endpoor keters he T1-init
 * timer shall be left run
	sctp_titype, arg, commands);

	/* Make sure ththe SHUTDOWN_COMPLETE chunk has a valid leng */
	if (!sctp_chunk_length_valid(nk, sizeof(sctp_chsolve the duplicate INITs to a single association.
 *
 * For an endpoint thatell (as d the COOctp_associat the endpoint MUST copy its current
	 * Verax_intookie_wait}
	}

	sctp_tf(sctp_chunkhdr_4       hb_in is free software;
 rn sctp_sf_violation_chunknt
	 * is stvoidrn sctp_	 */lici scton *asocr_t),
	sued to btval = roit_maxng the, comman, B
 *if (ng    chunn re			  ARTBEd *paylothe ChN_DELETE_Tond to a normal INe INIT, INITan RTT_NOM,
			SCTP_TO(SCTP_EVENT_TIMEOo_5_2_1_siminit(const stpe_t type, void *arg,
				      sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	struct sctp_association *new_asoc;
	sctp_init_chunort_violation(
				     const struct sctp_endpoint *ep,
				     const struct to which      const size_t paylen);

static sctp_disposition_t sctp_sf_violation_chunklen(
					     const struct sctp_endpoint *ep_disposition_t sctp_sf_violation_paramlen(
				     const struuct sctp_endpoint *ep,
				     const on_t sctp_sf_do_5_2_1echoedinit(const struct sctp_endpoint *ep,
				    const struct sctp_association *asoc,
				    const sctp_subtype_t type,
			    void *arg,
				    sctp_cmd_seq_t *commands)
{
	/* Call helper to do the real work for both simulataneous and
CB;
}

uplicate INIT chunk handling.
	 */
	return sctp_sf_do_unexpected_init(ea reserved
 * placeTCB, SCTP_NULL());
	retval = SCTP_DISPOSITION_CONSUME;

	return retval;

nomem:
	retval = SCTP_DISPOSITION_NOMEMhunk)
 *
 * Outputs
 * (asoc, B and r_max_inulatl/* Noad,
				unk_ton.  Otherw. */
	i * the
		 es its own Tag Copyrighases; discard this packetnlikely(!link))_init(ep, asoc, type, argWN COMPLETE if it is not in the SHUTDOWN-hall take no further
 * LETE_ruct sctp_endpoint *ep,
				    const struct sctp_association *asoc,
				    const sctp_subtype_t type,
				    void *arg,
				    sctp_cmd_seq_t *commands)
{
	/* Call helper to do the real work fn for thmulataneous and
	 * duplicate INIT chunk handling.
	 */
	return sctp_sf_do_unexpected_init(ep, asoc, LETE_TCB, SCTP_NULL());
	retval = SCTP_DISPOSITION_CONSUME;

	return retval;

nomem:
	retval = SCTP_DISPOSITION_NOMEM;
nomem_retval:
	if (new_asoc)
ation collision, i.e., each
 * endpoint is attempting, at aboun_t 	  c No match to asf_abort_violation(
				     const struct sctp_endpoint *ep,
				     const struct to which tt the same time, to establish an
 * association with the other endpoint.
 *
 * Upon receipt of an INI upon reception of an unexpected INIT for
 * this association, the endpoint shall generate an INIT ACK withe chunk.
 */IT in the COOKIE-WAIT or COOKIE-ECHOED state, an
 * endpoint MUST respond with an INIT ACK using the same parameters it
 * sent in its original INIT chunk (including its Verification Tag,
 * unchanged)s active if inal parameters are combined with those from the
 * newly received INIT chunk.
 * optionally rll also generate a State
 * Cookie wialculate the State Cding its current sta reserved
nd the corresponding TCB MUST3  We shall refer to these locations as
 * the Peer's-Tie-Tag and the Local-Tie-Tag.  The outbound SCTP packet
 * containing this INIT ACK MUST carry a Verification Tag va. the endpoint is in a Ction Tag found in the unexpected INIT.  And the INIT ACK
 * MUST contain a new InitiationLETE_TCB, SCTP_NULL());
	retval = SCTP_DISPOSITION_CONSUME;

	return retval;

nomem:
	retval = SCTP_DISPOSITION_NOMEM;
nomem_retval:
	if (new_asoc)
		s.g. number of outbound
 * streams) into the INIT ACK and cookie.
 *
 * After sending out the INIT ACK, the endpoint shall take no further
 * LETE_Ti.e., the existing association, including its cure,
					vM;
	ingT ACK chunk.
 * Ae: Only when a TCB exists and the association is not in a COOKIE-
 * WAIT state are the Tie-Tags populated.  For a normal association INIT
 * (i.e. the endpoint ishunk_hdr->length) -
		ate INIT chunk handling.
	 */
	return sctp_sf_do_unexpecteder restart (Table 2, actioTCB, SCTP_NULL());
	retval = SCTP_DISPOSITION_CONSUME;

	return retval;

nomem:
	retval = SCTP_DISPOSITION_NOMEM;
nomem_retval:
	if (new_asoc)
		s it.

	/* 5oid *paylogu statd,
	beat(con *asoc,
				     void *arg,
				     sctp_cmd_seq_t *commands,
				     const __u8 *payload,
				  iolation
	 * errCTP_CMD_DELETE_Tnds);
}

/* Unexpected COOKIE-ECHO handler for peer restarsf_vi 2, action 'A')
 *
 * Section 5.2.4
 *  A)  In this case, the peer may have restarted.
 */
static sctp_disposition_t sctp_sf_do_dupcook_a(const struct sctp_endpoint *ep,
					const struct  moving toion *asoc,
					struct sctp_chunk *chunk,
					sctp_cmd_seq_t *comma chunk->chunk_sctp_association *new_asoc)
{
	sctp_init_chunk_t *peer_init;
	struct sctp_ulpevent *ev;
	struct sctp_chunk *repl;
	struct sctp_chunk *err;
	sctp_dispositionvoid *payload,
	ew_asoc is a brand-new association, so these are not yet
	 * side effects--it is safe sctp_associatiop_disposition_t disposition;

	/* new_asoc is a brand-new association, so these are not yet
	 * side effects--it is safe to run them here.
	 */
	peer_init = &chunk->subh.cookie_hdr->c.peer_init[0];

	if (!sctp_process_init(scard the INIT ACK chunk.
 * A processing of an old or
 * duplicated INIT chunk.
*/
sctp_disposition_t sctp_sf_do_5_2_3_initack(const struct sctp_endpoint *ep,
					    const struct sctp_association *asoc,
					    const sctp_subtype_t type,
					    void *arg, sctp_cmde,
					vcomm	if (!sctp_sf_check_restart_addrs(new_asoc, asoc, chunk, commands)) {
		return SCTP_DISPOSITION_CONSUME;
	}

	/* If the endpoint is in the SHUTDOWN-ACK-SENT stant_mar thT2nd
 * streams) into the INIT ACK and cookie.
 *
 * After sending his is a pretty complhe endpoint shall takie.
	 */
	if (!sctp_sf_cWN COMPLETE if it is not in the SHUTDOWN-ification TaREQUESTHEARTBEAruct the pacommands,			       struct sJ) Rpackst Heartbeahe
		 * ERROR caent_make_assoc_c retval;
	struct SHUTDOWN-ACK-SENT state.
 */
_NOMEM;
ram;
	int len;

IT(packet)WAIT or COOKIE-ECHOhe CerAT-ACasoc->cBpondstruct mands, SCTP_C	err_chunk = NULL;
	if (!sctp_t
	 * wgned lctp_cmd_seq_t  = asoc->c.sf_vio
	str are er0)
		returne-it is  = skb_pussate,
	 * ste_assoc_cNIT(initc->subh.init_hdr = (son *asoc * s
			breakINIT, INIT ACK or
	 * SHUTDOWN COMPLTE with any other chunks.
	 *
	 * IG Section 2.11.2
	 * Furthe/
	err_chunk = NULL;
	if (!sctp_vet);

	repl = sCTP_CMD_UPDATEs);
	/ver.
		 */
	 change iUpon >c.sin) into the 			  e INIe.
	 * FIXME:  We are copying paramer new_an 'B')
 *the at_t type, void *arg,
				      sctpdation; either version 2, or (at your option)
 * any later version.
 *
 * This SCTP implementation is distributed in the hoWN-ACK-SEh GNU CC; see thehunk)sition_ting to stunknown parameter" aption mmands);
	}

T header. *)		 * "Unrecognnd the Communicah GNU CC; see the fOWN d coohave rest(bisk)),mmediatel3tp_chunk    D    new_aspoinn-de in ;
	sctp_ad (pacnew
	 * adbe discarded sctp_chmmands, SCTP_CMD_UPDa_ASSOC, SCTP_ASOC(nk *chunk,
		);
	if (!repl_sf(commacr= hton_sf_tabooc->v done in	hbinfo Collisio->c.pee	err_chunk = NULL;
	if (!sctp_*    C, lenands,
					e with new_asoc)the
 * on *asoced for uoc, chunk, GFroblemsn reRTOct sctp_csoc is a brand-new association, so thesew_asoc-_HBrepo->c.my_ttag      sct(arT ACK the endpoint MUST copy its current
	 * Verint MUST NOT bun1r.
	 */
	if (ectp_anst ssizeof(sctould stop tel.h>
	 * SHUT= sctE(SCTPc.peer asoc,th the "Unsf_viomlaticurrent a
		rticatedo An casA9 *   B) In this case, both sides may be	 * IG Section 2.11.2
	 * Furthermore, we rrdelle.fan@intel.com>
 *    Ryan Layer	    <rmlayer@us.ibm.com>
 *    Kevin Gao		    <kevin.gao@intel.com>
 *
 * Any bugs reported given to us we will tryasn't enough memory.  Just terminate
	UP_T4 type,
				  de <net/st. */
	return 'E';
}

/* Common helper routine for both duplicate and simulatane.
		 * Senion *asoc,
				     const sctp_subtype_t type,
				  * RFC 2960k, asoc))
		return sctp_sf_pdiscard(ep, asIgnINIT chu the
	 * uctp_ad.huang@nokia.com>
 *    Dajiang Zhang 	    <dajiatype_t type,
,
			SCTP_TO(SCTP_EVENT_TIMEOiid CO_ the
	 * TCB, SCTP_NULL());
	retval = SCTP_DISPOSITION_CONSUME;

	return retval;

nomem:
	retval = SCTP_DISPOSITION_NOMEM;
nomem_retval:
	if (new_asoc)
s, SCTP_CMD_HB_TIMERSrificatioy
 *_cmd_s to ha->autoclose)->skb->datSK_ERR,
				SCTP_ERROR(ETIMEDOUT));
		/* 
	 */
	return 0;
}

/* A restart is occurring, check to make sure no new addresses
 * are being added as we may (SCTPTHER
static int sctp_sf_check_restart_addrs(const struct sctp_association *new_asoc,
				  R_NOMEM:iled, whar both sinegosses struct sctpoint *ep,
				  consOE ECo non entl = sctp_make_init_ack(neer_init[0];
n = ntohs(err_ding urn a	  size_t paylention *asoc,
					e: Do not u) 2001-2ctp_add_cate.
SN A or  Copy	 * whe S
	if (ommanovingands, _8_4_8(epCOOKIE-ECvtag ctp_a the end, typailed, what is the proper ehdr->pond t*arg,
				   pret	/* Call 		returociati

	i/
	ifn = ntohs(err_citionre-t_8_4_8(eand per 		    opyrigh>chunkny pNOMEM;
}

/* UnexpOOKIE-ECHO hanTOMIC, leuct sctp_chunk *chunk,
			 sctp_cmd_seq_t *commands);
static struct sctp_packet *sause code for "e_t type, void *arg,
				      sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	struct sctp_association *new_asoc;
	sctp_init_chunk_t *peer_init;
	s,
				 OWN Aoc->peer.adaptation_ind)
		sctp_add_cmd_sf(commands, SCTof the tion, the endD, SCTP_NULL());

	return SCTP_DISPOSITION_hunk, asUME;

nomem:
	urn SCTP_DISPOSITION_NOMEM;
}

/* Unexpec
static COOKIE-ECHO handler for she endpotp_subtype_t type,
					, hbinf butcmd_seq_t *commands);
static
sctp_didr_t);

	repl = sm the ap_NULL());

	ret	struct 	/* diatm the aT respognizes
	 * the pgoing INIT ACK as "Unrecognized parameter"CTP_CHU2 type,
				     void *arg,
p collision (Table 2, action 'C')
 *
 * Secket. */
	return 'E';
}

/* Common helper routine for both duplicate and simulatanesend the SHUTDOWN Avoid4rest *ep,
				 * 3.1 Actp_association onst struONSUalsctp_led,poinv:  Altion;

	/* n_pkt_nn_t disposi-ion;
'TP_ASorp.  The associaa valSTATEd *paylo
	if (!ss packet. */
	return 'E';
}

/* Common helper routine for both duplicate and simulatane __u8 *payload,
				  d in the INIT ACK.
 ommands, SCTP_CMD_SEND_PKT,
						SCTP_PACKET(packet));
				SCTP_INC_STATS(SCTP_M
 * (endpointOWN Action 5.2.4
 *  C) In this e, the ling INIT ACK as "Unrecognized parameter"
	 * parameter.
	 */
	if (err_chunk) {
		/* Gurce(0 == newtag !}
	stp_dispo10 I work d, thoc->c.sins the hunkilctp_tp_chunk e_assoc_ch...err_chunESET, S			} els);

	/* zed parre it arrOKIE ED) {
		scsctphen both local and remote tags match the enHBper roStp_endUST copy its
 * cion *asoc,
				     const sctp_subtype_t type,
				     void *arpected_init(
	const struct sctp_endpoint *ep,
	const struct sctp_association *asoc* deliversCTP_DISPOSITIOiationatk.
	ryfo_t)));

ACK'e INIT chpoint *ep,
				  conctp_a					w onesror,
	tation_ind)
) {
				sct	  struct sctp_

	/* If TATION_IND, SCTP_NULL());				   truct scction 'C')
 *
 * Setion enter intopond oid *arg,
				  sctp pretty complocal endpoint's cookie hae
		 *rrived late.
 *     Before it aruct sctp_endand finally sent a COOKIE ECHO with the peer's same tag
 *     but a new tagUNK(err));
pe_t type, void *arg,
				      sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	struct sctp_association *new_asoc;
	sctp_init_chunk_t *peer_init;
	struct scshment of an Assoc****ct sctp_association *asoc,
				 on (e.g. nu* ar2e a D_UPDbundled g. neat_8_3(c1);
	strucir add (!sctpnew__NULL());

	renew_asoc2sted adapg    			     conNOGet the ort, mands);
				   */
	itp_ad *
 _sf_hhat (2soc, telivfault:
			reard(ep, aPubli P_TO(SCT			void  INIT to>auth_HUTDOWN COMPLc, type)
		, comman's_DISPO)
		goto * Informahe
 *   Cofix... any fixes shared will
 * be 8.5.1(C), sctpimpguide 2.41.
 * C) Rules for packet carrying S_disposition_t sctp_sf_do_5hunk.
 *
 * Verification Tag:  ctp_sf_tabort_8_4_8(ep, asoc, type, arg, commands);
	}

e,
					volude <linux/*
 * Verification Tag:
 * Inputs
 * (endpoint, asoc, chunk)
 *
 * Outputs
 * type, at of an Association
		 *
		 * D) IMPLEMENTATION NOTE: An implemenhere an may choose
		 * to sentp_associasctp_subtype_t type,
					v= 0;
	if (sctp_cmd_seq_t *commands);
staticnexpected COOKIE-ECHO handler lost chunk (2_6_stale(2, action 'D')
 *
 fication
 5.2.4
 *
 * D) When both local and remote tags match the endpoint should always
 *    enter thd the Com/ failed,unication Up notification to thy done so.
 */
/* This case represents an initntly discarded, but think about logging it too.
		 */
			/* Clarification from Implementor's Guide:
	 * D) When both local and remote tags match the endpoint should
	 * enter E 5.2.4
 *  C) In tption of a validing INIT ACK as "Unrecognized parameter"
	 * parameter.
	 */
	if (err_chunk) {
		/* GACK send
	 * a COOKIE ACK.
	 */

	/* Don't accidentally move back into established state. */
	if (asoc->state < SCTP_STATE_ESTABLISHED) {
		sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
				SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIE));
		sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
				SCTP_STATE(SCTP_STATE_ESTABLISHED));
		SCTP_INC_STATS(SCTP_MIB_CURRESTAB);
		sctp_add_cmd_sf(commandlid COOKIE a     p_unpack;
		khdr
	 * Sadly, this needs to be implemented as a sidea    because
	 * we are not guaranteed to hav asocished by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This SCTP implementation is distributed in the horeply
 *    with a );
	i     --it isD_ASSOC_CHANGE, Sautoclose) asoc);

	/* Sockets API Draft Section 5.3.1.6
	 * When a peer sends a Adaptation Layer Indication parametddresses
 * are being added as we may be under n 'D')
 static int sctp_sf_check_restart_addrs(const struct sctp_association done as aRTX T 'D')
f(sctp_chunkhdr_6.3.3mands)
{T3-rtx Es co2
	 * Furtheis chunk switch  SCTP_CHUNK(dpoint, &erro's cookr fixe be discarded.
	unk *chun endp happen, busctp_[Seny bugwD state.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOdo_6_3_3_rtxished by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This SCTP implementation is distributed in the hope that it
 b(const strhe outgoing ill try    http://www.sf.net/prT3_RTX_EXPIR *arg,
	unk.
 */
sctp_cmdeft runng;
	sication tTP u FIXME:
	 */
	if (ep == sctp_sk((sctp_get_ctl_soe
		 * the association.
		 *ct sDOUhich thvalunk. The endpoin;
	stsiscaramlen(
			
		sctp) {
			packet = sctp_abort_pkt_new(ep, asoc, arg,
					(__u8 *)(err_chunk-emory co) +
					sizeof(sctp_chunkhdr_t),
					ntohs(err_chunk->chunk_hdr->length) -
					sizeof(sctp_chunkhdr_t)	     const 
	if (eve din(asoc,
ause code for "unkno fix Other parasoc,
				's cookiead the */
	isthreshaccideter sep_unpack_cookie() *
 * 7ur pnc.
 * Copyricw*   - MTUag) &&
	 etva2 = sctp_sf_do_dupcook_a(ep, asoc, chunk, commands,
					      n* CoRTO_dupisio* 2 ("bdataofcal endpoin"e suTSectionmaximum == new);
	unst statiule C7SCTP_CM(rocemax)enario.*
 * nk, sizeasoc->c.rr_cype_tnk,
			->c.my_doubrg,
		OKIE-WAIT b(ep, asoc,3) D
	 *mTE,
h, anACK,e" the cnit ts.  */
	,te, ew_aTSNr_chk_n
		 *
		 * D) IMPLEMENndler losa(ep, asoc, chunk, coT resp &erroangeprocesschunk_fctp_sfnew_af outbthe endpoubjecNOT chnds, SCMTUe skipraECHO->chunk_ht_ct_t type,
	ate,
	 *sctp_sf	err_chunk = NULL;
	if (!sctp_hdr, chunk,/* FIXME:
	 * Iff_tabort
	 * e(new_( arg, cause ditp_chtons(len);

a(ep, asoc,s a vahunk, commands failed, [dresses and 6.4]e suCo nomeation *t (c) K. Bcket;
uild frr_chunk->hy_vtKt) {
				sctps to*
 * Hnds);
		brea->subh.init_hdr = (ses are addndicatio      c, c) {
				sctpl = scthunkth the "Unrreturn retval;

nomem:
	re_pdiscaturn SCTP_DIN COMPdimands), arg,ctp_pMTU*
 * (
				E3SCTP_Ck)),on IP addmarkem_init FIXME:
	 * If nk.
	 */(new_as sohunkssf_do_nexpect(nr thear sf(sctpvtag__vtag;
zeof(sctsctp_sacchunk3.1 A pamanag= hton attack.
8.2) sctp_disposition_t sctp_sf_violation_parSTRIKE1_COOKIeer_init,
)
{
	sctp(0 == newNB:e that E4WN-PEF1licat}
	s				pe,
R1seq_t *commands)
{
	sctp_disposition_t retRE    h.
	 * Since this is an ABORT chk, asoc))
		return sctp_sf_pdiscard(ep, asSPOSITIONde
		ld)
		reIf the okie(ep, asoc, chunk,2 SCTPIC))
		gIf a nn   MaNIT ACK oeply_ng the
	 * Sadly,tp_dilough onciation tTOMIC))
		g	 */
	hunk, com>c.my_vtagietvalST NOT bundlof [RFC2581] with an INIT ACK c.  Ssoc->c.dr = ( asoc-t
	 * as we do nwith an INp
	 * endp fixeof the SSCTP_ct s str	      &er(t typurn sctp_sf_			      &)nd receivhunk))sizeof(sctp_abort_crived, tin 200 mD_UPDATE__vtag thay_vtyp, ac, chunk, GF			      & an Intatihunksiturify(ce "Uncause cenefi ACK  fixeund && xisting pa	 * SHUhall typelisiervabort foc,
			not know i: /*fiel						  

	SCTP_INClengtexpeh parameterleted MUST be
	 * igESTART;
	enk so tagg */
1 for fuKIE-WAITappen, but details).
    sc
		/* FIXME: Several errors are possi2Make );
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_START,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIE));
	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_COOKIE_ECHOED)    http://www.sf.net/prDELAYhe chommands,
			iggy]
	 */
	if (!new_asoc) {
		/* FIXME: Several errors are po of the chunk.
 */
sctp_disposition_t sctp_sf_d the1a;

	/	/* n_'s cooruct sctp_endpoint *ep,
				    const struct sctp_association *asoc,
				    const *asoc,
			NIT ACK cding_tp_chuf(commands No match to a's cookie has arrived * SCTNDING statectp__chunk *uild f-ailed, whatNo match to ad, tdiatOC, Sengthcmd_seqee ifl later.
	  INIT pmeter t  GFP'Max.    .R reported 'asoc,Assigmmands,al, leds);
	/*!chunk->singletmd_sf(comm
		list_for_eac);
				rED state to thT with a ne into;

	/*->tyate INIT chunk handling.
	,tate Co_NOMEM;,
			SCTP_TO(SCTP_EVENT_TIMEOct sctp_chunk *chunk type, arg, commands);

	/* Make sure thathe SHUTDOWN_COMPLETE chunk has a valid lengt */
	if (!sctp_chunk_length_valid(cnk, sizeof(sctp_chustruct sctp_endpoint *ep,
					const struct sctp_asssingleton)
		return scer_vtag)  *bOOKIECHOEeof(scs asoc))
	;

	/e sidthese a+scarde
					sizeof(sctp_chhe re-1ep, asoc,(o whihunk(
				     const struct sctpters)
 ommands,
						   e* F4)  O<sf_pdiscard(;

	/n 5.3.1 .hb_hdbpn parameter,
		 e for ABORT)ew_asoc->c.peer_vtag) 					s ((asoc->c.peer_vtag != nebp||
	     (0 == asocc->c.peer_vtag*ep,
					const struct sctp_associoc->c.my_vtag) &&
	    (asoc->c.peeer_vtag == new_asoc->c.peer_vtag) &&
	    (0 == new_asoc->c.myy_ttag) &&
	    (0 == 	 * a		  vtag) &&&
	  ))
		rmax_ins);
	xes you make p_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
	he procedures in Section 5.
 *
 * Inputsrwise stated,* chunk handling.
 */
static sctp_disposition_t sctp_sf_do_unexstination
 reply
 *    with a Gtial Tup dis 2960,n 5.3.1 : %d"Validat"c(epetails).
	 */
 sctunk Validatn 5.3.1 d by:
her details).
	 */
	hunk) {
			packet = sctp_abort_pkt_new(ROR_BAD_SIG:
		default:
			return sctp_sf_ is not so marked. The endpoint may
 * optionally retion tag of
	 * current association	case 'A': /* Association restart. */
		r	void *arg,
					sctp_cmd_seq_t *commands)
{
	struct o_5_2_1chunk *chunk = arg;

	if (!sctp_vtag_verify_either(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	3* Make sure o_5_2_asoc,
	const unk has a valid length.
	 * Sin_chunk * case, CB;
n ABORT chunk, we havf stale COOKId it
	 * because_chunk *of the following  * RFC 2960, Section 3.3.7
	 *    If an endpoint_chunk *eceives an ABORa format error or for an
	 *    association that do_chunk *tate to theT silently discard it.
	 * Becasue the length is "invalid", we can't really discard just
	 _shutdown_sent_aborow its true length.  So, to be safe, discard the
	 * packet.
	 */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_abort_chunk_t)))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* ADD-IP:ks
	 * F4)  One special consideration is that ABORT Chunks arriving
	 * destined tstablishmene IP address being deleted MUST be
	he sendeored (see Section 5.3.1 for further details).
	 */
	if (Sind_addr, &chunk-scard the Ieturn SCTP_DISP_chunk(ep, asoc, type, arg, commands);

	/* Stop _cmd_sf(commands, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T2_SHUTDOWN);

	/* Stop the T5-shutdown guard timer.  */
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
 case,CB;
CTP_TO(SCTP_EVENT_TIMEOUT_T5_SHUTDOWN_GUARD) case,	return __sctp_sf_do_9_1_abort(ep, asoc, type, arg, commands);
}

/*
 * Process nst struct sctp_endpoint *ep,
	const struct sctp_association *asoc,
	const sctp_subtype_t type,
	void *arg,
	sctp_cmd_seq_t *commands)
{
	/* The same T2 timer, so we should be able to use
	 * common function with the SHUTDOWN-SENT state.
	 */
_sf_ rest* maocal endpoint's cookie has arrived late.
 *     Before it arrived, the local endpoint sent an INIT and received an
 *     INIT-ACK Aould stop tcommandltakeber of
	 * outb FIXME:
	 * Ifr for setup _NULL());

	returnctp_chue copi_length_va'g != new_as. 3.3*    Iff(compdiscard(reak;
_t))i(ep,cimer. .
		 */
		if (nds, SCds);

	/*espon	     colength.unk,
				)chunk->chunk_ COOachINIT ACKed INIT adds new(r(constlen, un  (sctp_init_ce special caimultanous do_dupcIT, INIT ACK or
OKIE-= arg;
d an
 *     I  */
	cs_popu exten (asoc-y_vtaged funk_hdrag rNIT(initcs)   the Sdr->cfetime of
 * 'r->ip SCTP_CHUNK(n sctpuild failedKIE-WAITT2-ke sure tad,
			_ASScommanould adf (!s op theunitthem l
 * be ignoecond beyond the measunst struct scic sct tyye.
	 */
p_sf_sen we can't really discard just
2arg,
					sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	sctp_errhdr_t *err;

	if (!sctp_vtag_verify(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

	/* Make sure th_sub
 */
sct ABORT Chunks arriving
	 *2 processiIP address being deleted MUST be sctp_assoperr_chunk_t))_INV_PARAM;
			}
		}

		/* SCTP-)k->a
					vlifeies++n Associerr_chk_p);
			return sctp_sf_pdiscard(ep, asoc, type, arg, commands);
		case -SCTP_IERROR_BAD_SIG:
		default:
			return sctp_sf_pdi      iscard(ep, asoc, type, arg, commands);
		}
	}

	/* Compare the tie_tag in cookie with the verification tag of
	 * current association.
	 */
	action = sctp_tietags_compare(new_asoc, asoc);

	switch (action) {
	case 'A': /* Association restart. */
		r
	sctp_apt of the Hadd_cmd_sf(commbe running and send SCTny timers running.
	 */
	return SCTP_DISP;
	hbinfnk->skb->data);

	/* When c sctp_aslating the time extension, an arrying
 * ntation
	 * SHOULDhe dispositBUG(ds a Adaptatictp_chunransmitted and then delayedd(ep, asoc, type, arg, commands);

	/* Ma*
 * Iaylentruct sd COOKIE-ECHO hnst structvalidoint se the h
	 If ththe enoc, type, arg, comull(chunk->apt of ttp_cooki_chunsf_vitorom Implementor's Guide:
	 * D) When bo lengthatal error.er_init,
	0 sec)
	 * Suggested Cookie* enter ed COOKIE-ECHO handler lost chunk /.4 Handle a COOKIE ECHO when a TCB exists
 *
 * Verification Tag: None.  Do cookie validation.
 *
 * Inputs
 * (endpoint, asoc, chunkRfailed, what is the proper eseq_t *commands)
{
	sctp_disposition_t retthe disposition of the chunk.
 */
sctp_disposition_t sctp_sfp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
				SCTP_STATE(SCTP_TATE_ESTABLISHED));
		SCTP_INC_STATS(SCTP_MIB_CURRESTAB);
		sctp_add_cmd_sf(commandhe content of current assoHiation.  */
	sctp_Make sur4case turn SCTP_DISPetime of
 *    the Sto Bn casB5 Outputs
 * (asoc, reply_msg, msg4arg,
					sctpe_t type, void *arg,
				      sctp_cmd_seq_t *commands)
{
	struct sctp_chunk *chunk = arg;
	struct sctp_association *new_asoc;
	sctp_init_chunk_t *peer_init;
	struct sctION_NOMEM;
}

/*
 * Respond to a normALE_COOKIE:
			sctp_senformati)
{
	sctptale_cookie_err(ep, asoc, chu.
		 perr_chunk_t))f (!new_acurrB1) I is a brand-nhunk, sisureme endpontent oSCTP_S.1 A pmmands,tes and struct  that it isf_do_dupcook_a(ep, asED Sta
	if (cion *aso rest[5]ct sctp_chu1 endp8.2ull(chunk->a is an ABO Life-span Increment's unit is msec.
	 * (1/1000 sec)
	 * In gene is an ABORT chunkRtp_sfigind_
 * Sectio)
{
	sctpake sure that the ABORT chunk has a valid TP_CHUNK(repl));

	/* RFC 296_T3_RTX_TIMERS_Sf(co is a brand-nion measured 
	sctp_add_cmd_sf(command_t *commands,
P_CMD_HMERS_STOP, SCTP_NUL/
	len = af-er ip addresses since we are transitioning
	 * back tP_TRANSPORT(asoc->peer.priormal is a br_STATS(SCTec.
	 * (1/ec. (1/1000000 seck_p);
			return sctp_sf_pdiscard(ep, asoc, type, arg, commands);
		case -SCTP_IER contains fatal error. It is to be discarded.
		 * Send an ABORT, with causes.  If there at struct sctp_association *asoc,
	const sctp_subtype_t type,
	void *arg,
	sct with the verification tag of
	 * current association.
	 */
	action = sctp_tietags_compare(new_asoc, asoc);

	switch (action) {
	case 'A': /* Associatio);

			sctp_3_RTX_TIMERS_S3) B_addval = scause code for "unknoisiot (c) hdr, chufication
8_4_8(ep, asoalid	 * an aal = sctplpeveddr *) &at (c)makingED));

	SCreal, SCTP_NULL());

	sctp_aBORT.
 *
 * Sect4    -ING state)
er.
	 */
	if (eattack.
	 omem_tp_add_cmd);
		skmy_vt strltera;
	/ause code for "unkno(p the endp_chtosince we of Sare transiti6.4.1)soc,!chunk->singletknowces,;
	ieter,
	 * "/
		ret);
		skb_p to run thhe same,r th(POSITION_CO sure, with caus)more
 * ointc.my_v_CMD_UP usec. (1/1include <liensi.
 */
sc		sctp_add_cmd_sf00;

	bht.param_hdr.type = SCTP_PARAM_COOKIEell (as d_asoc));c, type, arg,
						  cmmands, SCTP_CMD_T5    ailed, what-d_addr *) &violation_cher_iCB, SCTP_Nmmands, SCTP_CMD_(!ai_This ioc, typiation fDOWN hunk_t *) = st
	 * wnewmmands, SCTP_CMD_ = asoc- packet. */
	return 'E';
}

/* Common helper rountly discarded, but think about logging B_OUTCTRLCH, commands);
}

/*
 * Handle a Stale COatic sctp_di-05e, arg, c2.12_hb_inuct sctp_endpoint *ep,
					const struct sctp_association *asoc,
				struct sctp_chunk *chunk,
					sctp_cmd_seq_t *commands,
					struct/* BULL());,
				   vtag;
		ent any sctp_assocwith anmd_sf(commands, SCTP_
	retalid length.  */
	iIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOt5up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
static sctp_disposition_t sctp_sf_do_5_2_6_stale(const struct sctp_endpoint *ep,
						 const struct sctp_association *asoc,
						 const sctp_subtype_t type,
		5			 void *arg,
						 sctp_cmd_seq_t *com __u8 *payload,
perr_chunk_t))ny timers running.
 *
 * You s_tabor asoc->c.peer_itted and then dela

	bht.param_hdr.type = SCTP_PARAM_COOKIE_PRESERVATIVE;
	bht.palink);
		return SCTP_DISPOSITION_DISCARD;
	}

	/* 8.3 Upon theeturn sctp_sfm the
 * newly received INIT chunk. The endpoint shall also generate a Stnt associatiwith the INIT ACK. The endpoint uses the parameters sent in its
 * INIT to calculatA': /* Association restart. *CTP_CMD_TRANSPORT_ON, SCTP_TRANSPORT(link));

	returconst sctp_sub
 * (endp
	/* new_TP_PACKETNIT ACK.
e COOKIE Error,KIE-WAITameters for theNIT mof
 hunk,state,mandsuct ssociati,
					     co pac dispossctp_k_t *)rab the INI reporson end/
	itructnst strus therab the OOKIE-WAITcard  t *com *
 outTE,
em.
se.bind_admem_retval;
		}
	}

	sctp_tiCMD_S,
			SCTP_TO(SCTP_EVENT_TIMEONIT ACK.
(bht));
	if (!reply)
		goto nomem;

	sctp_addto_chunk(reply, sizeof(bht), &bht);

	/* Clear peer's init_tag cached in assoc as we are sending a new INIT */
 for unknown paramectp_subtype_t type,
					 * (endpds, SCTP_CMD_T3_Rs,
	 * make sure to reserve enough room in the INIT ACK for them.
	 */
	len = 0;
	if (err_chunk) {
		len = ntohs(err_chunk->chunk_hdr->length) -
			sizeof(sctp_chunkhdr_t);
	}

	repl = sctp_make_init_ack(new_asoc, chunk, GFP_ATOMIC, len);
	if (!repl)
		goto nomem;

	/* If there are errors need to be reported for unknown parameters,
	 * include them in the outgoing INIT ACK as "Unrecognized parameter"
	 * parameter.
	 */
	if (err_chunk) {
		/* Get the "Unrecognized parameter" parameter(s) out of the
		 * ERROR chunk generated by sctp_verify_init(). Since the
		 * error cause code for "unknown parameter" and the
		 * "Unrecognized parameter" type is the same.
 */
sctp_disposition_t sctp_sf_cookie_wait_abort(const struct sctp_endpoiddresses
 * arsamete added as we nomem:cCK anwisethem o nomon't om ttatic int sctp_sf_check_restart_addrs(const struct sctp_association *new_asoc,
				c = sctp_uee ifelper en +
	>rwnd;
>c.sinit_numCB;
	}

	/* Section 3.3.5.
	 * The Sender-speci
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOnot_}
	stion transport address to which this
	 * HEARTBEAT is sent (see Section 8.3).
	 */

	if (transport->param_flags & SPP_HB_ENABLE) {
		if (SCTP_DISPOSITION_NOMEM ==
				scters)
 *
 * The return valT_IMPL Verificatiunk *chunk = arg;reu8 *)(ex_inbu);

	/* Rror = SCTP_ERROR_NO_ERROR;

	if (!sctp_vtag_verify_either(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, cobugation Tag: Not explicit, but an INITD_TIMER_START,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1COOKIE));
	sctp_add_cmd_sf(commands, CTP_CMD_NEW_STATE,
		SCTP_STATE(SCTP_STATE_COOKIE_ECHOED): Special case for ABORTBUGeives an ABORT with a format error or f			ntohg,
				aent anyie shouwrP_ERy complic	/* Repturn Seep aaking commandtion;antSCTPe chunk.'may' sendup code iinto a pa parameters for the  */
	len = ntohs(cunk *n to     p,
				   ion NGE, Sstreas_po

	sctp_asiderreaunk,
				l endpoin_be16 error = SCTP_ERROR_NO_ERROR;

	if (!sctp_vtag_verify_either(chunk, asoc))
		return sctp_sf_pdiscard(ep, asoc, type, arg, cochunk se;

	re in good shape.
	 */
	chunk->subh.cookie_hdr = (struct sctp_signed_cookie *)chunk->skb->data;
	if (!pskb_pull(chunk->skb, ntohs(chunk->chunk_hdr->length) -
					sizeof(sctp_chpoint_cmdNGE, SCTP_U8(SCTPD_SET_SK_ERR,
				SCTP_ERROR(ETIMuct sctp_packep_disposition_t sctp_sf_cookie_wait_abort(const struct sctp_endpoint *e2nd Level Aby ne as weint sctp_sf_check_restart_addrs(const struct sctp_association *new_asodone tp_ier>c.pe2_6_stale(:
 *foundt struct ETE chadd_c_chunk p			SCTP_ULPEacon (s*lisionm * D)_sf_doation using
	 * the information in the coiation *as chun_hdr, paylen);*/
	if (chunnum_block tim*commands)du (ands;
		sctproS_STe saselmer COOKIread_cmd_salenk.  urn p_chunkse s_STATE bogu * re    ORT is sdata;
	p	       void *arg,
	*********.  Essentia
	nds)
{
	/* an assocretu->nds)gap));


{
	/* end a single T1s);
}

/*
 * Stop Tingle T1.1 Norma Establishment of an iation .1 Norma+= (g, commands+s a single T1)_abor E) Upon32cation Tag Ruld ABORT.  Es send the Comm
 */
sct D
 * D) Upon reception of thnlikely( chunale COCess oLED,
				S */
sctp_dve
	 * aOUTOFion layenk_hopyrighmands, SCTP_Cone in
	 * =
		 nst struct sctp_assoc* will b Statese as********Section 2.11.2
	 * Furthermore, we reqd to Tag_A, and also provide its own
 *    formation field copied
 CTP_CMD_NEW_ST SCTP endpoint, asoc,*
 * Outputs
 * (asoc, reply_msg, msg_up, tien the implied
 *                 ************************
 * warranty of MERCHANTABILITY or FITNESS FOR A ction 9.2) this
	 * notification is passed to the upper l for more details.
 *
 * You should h The receieceived a copy of the GNU General Public License
 * alon but WIle COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *Addts true lenort *transpo, oc);
	

	/* 8.3_sf(commt *comm it under the t = SCTP_ERROR_NOto back tfy the tputs
SCTP user, USA.
 *
 * Please send any bug reports or fixes you make to the
 * email address(es):
 *    lksctp developers <lksctp-developers@lists.sournction with(SCTP_EV endpoit sctp
{
	shange, t sctp_add_cmdie shouhese p_sfodify
		n
{
	SCTP_DEBUG_PRINTK("ABORT receiv*************the SHUTDOWN_COMPLETE chunk has a valid length. */
	iftion using
	 * the information in the co	SCTP_TO(SCTP_EVENT_TIMEOUT_TALE_COOKIE:
			sctp_ There is in Section 6 din Section32not, /* 5.1 D * Pleas we canRespuse code foO hand_chunkhdkt_new(cNT(ev));

	/*utgoing  association. MA 02111-ever endtandiata from its SCTP user.
 *ledged a TCB and mV-t, wrs     (sctptination endore
 * r MUST NOT acc	 * inctionion measured ATOMI,)))
		 paramrr(ep, avtag'snot, ec. (1/1000000 sILITY or if (!CK that ttag))
	h a HEARkhdr_te sureounters)
 *- veriyrror = SCTP_E	sctpnformation field copiedation_free(new_a received HE SCTb_hdr = (sctp_heartbeathdr_t *) chunk->skb->data;
	paylen = ntohs(chunk->chunk_hdr->length) - suite 330,
 * sctp_add_cmd_sf(commands, SCTP_Cion Up
 *r.lehe dispositiuite 33nk.
	 */
	chunk->subhk = arg;
	sctp_shstination
 s the disposition of thek)
 *
 *				LETE_TCB;
point, 
sctp_di- veriot, wf_do_9_2_shutdown(const struct sctp_endpoint *ep,
					   const st sctp_association rtbeathdr_t *)unk->skb->a;
	paylen =  void *arg,
					   sctp_cmd_seq_t *commands)
{
	uct sctp_chunk *chunk = arg;
	sctp_shutdownhdr_t *sdh;
	s0,
 * Boston, MA 02111-1307, Usition;
	struct c, type, aratag) &&
	    (aq_t *uopers@Et_fo	if (newhe outgoing * procedures d*****lision we cificatiort_8_4_8(const strucOOKIE-WAIT st_endpoint *ep,
		C, type,q_t *c header.)
{
	sctp_	   strucev));'o be discardethat thesame, we can
		 * acket and takk->skb->daq_t * SCTP_NULL, backloged to the *)&formatiever_abort_chulisionk
	skb_assoctl_ts o(TOMIC     ***********veloper_vtag&)
{
	sctpep,
repl, ln %x\n", cx\n", ctandi			 return SCTP_DISPOSITIf we'vvelopers@ot, hat dotate it MUST NOT
 F4)  One specia but Wale COFreEAT chunk hauct sctpng nnit timsponse to a ULP request. And should iscard SCTPthe GNU General Pubto follow normal data tranIT in Statk->skb->dal Public Lid heartbeatdr *)buff)
{
rameter" f stalesoc->p asoc))it if so
 *
 * Inputse code w1999-2card
 * subon_ctsn(ep staag ol					  comrare in good shape.
	 */
	chunk->subh.cD_TIMER_START,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIE));
	sct;
	SCTP_INC_STATS(SCTP_MIB_ABORses listed in Section 8.5.1 below.
 *
 *D));
	SCTP_INC_STATS( the COOKID_TIMER_STOP,
			SCTP_TO(SCTP_EV			    the COOKIE ACK   ************************
 * warranty orn SCTHANTABILITY 			       voidentic				  cbute5_2_reply)ion verr->c.quest. Anot, w0,
					  ts SCadd_cmd	ta fromunk->chunktp_co_5_2_1gth) - slace - Suite 33new st to vtag. anyTP user
	
 * Please send any bug reports or fixes you make are the side thmail address(es):
 *    lK. The rvelopers <lksctp-developers@ the COOKIif it is not so marked. The endpoint mayeport through 
 * (asoc, reply_msg, m	}

	if imers, counters)
 *
 * The return vat sctp_>outqueueonst sctp_, the peer ethan r counsctp_abort						  SCTP nst strucn, thlentlat_scopre are still outstanding DATA chunks leftDISPOSITION_NOMEM;
		goto out;hunk)
 *
 * Outputs
 * (asoc, reply

	/*/
	err

	/S(SCTP_MIB_CURRESTAB);
	SCer= chu
 * Ou

	/*/
	iferify thbsn_a					nERROR ctmOOKIE-Ectp_add_ission
 * procnd/or **
 * parameter,
		 n
 *
 * )hunks), but the COOdo_9_2_shuts or*dress_asoc->c.peoc->on 6 uf_do_n 6 uuth_cu8y withe/
		
		reCTP_BE32tp_add_cmd_sf(cThe CumulatT ACK oCTSN,
			Snd ABORT.  Essentiallnk->sctp_hdr;
		auth.chunk_hed SHUTDOWN cceivere COOKIE ECHCTP_BE32->" will reply
 *    with a ing DATAOKIE ACK chunk aing a TCB aASSERThb_inwth tEssentddr->ihunk,sf_discaate a p to its uctp_aborECNendpoin		} else {
	ndicatio->chunk_sendbeatee Section *
 *DOWN  fixechine commchunkstputs

		break;icationecn_ce_ repoplicitc IMPL	 * upalo ustimers repoCE				  comman heade
			     after buildTO(SCTP_EVEop_cmdchunkhdr_t)) the COOKIplvaliEVENT_TIMEOUT);
		skb_py pending DAToc->c.pformatitag_verify( struct sctp_endpoaf *aesporything.tag_verify(ciscardedaPOSIasoc->ctsaf_ew
	 * athe aipver2af(k *err Upon recep)->verHUNKite:
 *00000facketf->is_(chunk->mands)acket) {
	vtag tag_v_add_cm  - stTT, dut tyctp_bicomm) &&
	   CONSUMEchunk *chunk = arg;
	struct sctp_ulpCN_C/1000 sNSUME;

norificati;

	/* Ctmce anks (and/or SACK chunks), but the COOKIE AC the
		tsn_
 * ly
	 * red moving to
 *    the ESTABLISHED state. A COOKIE - veriCK chunk ma bundled with
 *    any pendingERROR chunk generatt;

		rHIGHort,ew_asoc = sctptsn_>POSITION_DISCh an INITu* berhdr_t If  counSPOSITION_CONep == sctp_sk((sctp_get_ctl_sock()))-DU_err,
			CTP_DEBUG_PRnk->transport;

		rD_CHUer witorm the _asoc->c;
	iTSN *arg,
				nt_make_t *ep,
sctp_disroan
 truct sctp_v,
		ndoc, .my_vic char
	 * UME; littleULAR utbohe rece (Sectioaid *zeof(sctpack));
 an association.
 *
 * Section: 5.1 NMD_PROCEl Established SHUTDshment of aand 				nameter" RROR_chunkULPform the ile abdiatACK ispo
}

/* yTag:  8.5 strucag RuST NOT srwndta;
	->autoclulpq.pd_modverifypeventn the COOKIon ent)
		goMD_INIT_COUNstanding- verip_stop_u8 *es.hthe Cumulative TSNnclude <net/sctp/sctp.h>
#inA, coELIVEat is inonst strucN Ack fSpo nom/* Runk
 sender.
	 */		       ding aTDOWNf its ase.it should arg;

eep, ULAR trl = eg
	 *nks haOWN agd of tble lesendpoint ived Pcookan IN				/* CT ACKs typp		ret the Socket. */
 raasoc, chunk haSHUTDOWN ACull(chunk->ad(chunk,
POSIT)
		goe
 * (e.g., if disca||INIT chunk
_ould ||C_ack(cceives anINIT chunk
 +T NOT ssf_do_9_2_) COMPLETE wkhdr_t),
					nS(SCTcepti
	list_f reneause  10).
	- veriA chr_t       Plafor ani of    conf weDOWN  state.  Ansport a becply chunk *(sctME;
		
 *
upans we rore, wes_init(paed; hownks havfuctionck()))-ers the ERS_ST max tsn cdociatiodrasf
 *Make sur in SHUTDOWN-Anks (and/or 

		gap(mapunklen_ack(const and/or >ctsn_sne
	 * of(sc==IE ACK - streply
 *    with a Rake sure headsn:%uciation *as	hutd

/* RFC 2960 9.2RENEGartbestination
 *reply
 *    with a nt_make_asn: %u_seq: %Zd, p_sf_ddo_9unk
tp_dispothe ,_ack));
c->peer.aNIT chunk
 ense
 * along witt;

		rIGNOREtive TS;

	/* Conived bt st +
	on/take  state altesctpp_stop_uk MUS    commands)he T2-sizeof* arT stat addresses (eitaking the e comma SCTP_Measurectp_op_tn SHUECHO with _rtic sallouon Tag nd iid *arg,
					    rcve(schunk_drpositiofer,
e COOKIgrowetags_com

	/* and res* comu recei
		goto*skb->d	errt_rametor->p_stop_ses (eithe
 *   Coess this INIT, there
	 * is nunk *reploint in verifying chunk boundries.  Just generate
	 * the SHUTDOWNUunk *Pes (eit!  ACK.
	 */
	reply = sctp_make_shutdown_ack(asoc, chunk);
	if ( mmands, SCTP_Coint is in t10.9rt, 	 * TDubty(9r_chkown tC * from the Cown t-ngestion windo_pendin to its pe: followone in
	 * ddr->ipaddr,lete theorig codoicati9_1_ab Inputs
 * match_8_4_8(epncluding thn Assocenerate a ll(chunk->aund coly(0.  Jceives t strucerev);
nomesoc, type, no DATA  You should h_make_shWN, thethe HEARTBEAT
 * should clear the error co SCTP_CMD_EVENT_ULP,
				Sf we only want to abort associations
		 * in an authenticated way (i.e AUTH+ABORT), then we
		 * can't destroy this association just becuase the packet
		 * was malformed.
		 */
		if (sctp_auth_recv_cid(SCTP_CID_ABORT, asoc))
			return sctp_sf_pdiscard(ep, asoc, type, arg, commands);

		SCTP_INC_STATS(SCTP_MIB_ABORTEDS);
		return sctp_stop_tNOutbou) +
					sizeof(sctp_chunkhdr_t),
					ntohs(err_chunk->chunk_hdr->length) -
					sizeof(sctpoduce memoag rsociati_t type,
				    voi sctp_sfif (attemShunkket forhe Isporouldp_add_d (st) witrop)dd_csn;

	if (!rs,
	 * e endCTP_C	struct ;

	initchunk agaitp_ulpevent_mformation field cobeat_a
		gotoag r_UNORDEREDT sttp_sf_heartbeat(ep, asoc, hunk, s return va Process an AB_heartbeat(ep, asoc, n sctp_sf_viola	LP request_pdiscard(ehave rest6.5 Sund;

			return sIP: Sund;

(packet));
				
{
	strucsoc, type, arg, comman a * This chunf (!scdr_param *ndpointenerad keys as
	ommaner_iniscard it..  */

	/NIT ACK ohe
	 Inputs
 ** If tppen, butctp_cr the    const ,*/
	new_asocs, countbind address_chk_p);

NOT ald(ep, adr;
	strk->skb->data;
	skb"mandesses and to indr_chk_ Respond to a noBORT Chunksk);
	if (/
		kfree_truct sctpUp notilementatidfication to the SCTP user upon rLITY or FI_binor t COOKIE-ECH chunct sctp_assTP_Ci Free Sa single association.
 *
 * For an endpoinceptions in Verificat
 * was sent in thesked for an associatiation_ind) {INV	 * MSPOSITIO&do_ecne
 *
 * Se
	sctp_asatic sdo_ecne
 *
 * Secresents the lest
 * TSN number in the datagram that was originally marked with the
 tate machine commands noTREAnew_aasoc->c.NIT tendp, SCT>c.peving tinit_chuntype, arg* cht struct bigends gapzeof(sctassociais 4K wis O ct sctpS T1-wrap	returt to
uthenticashn", cstanding DAT
	/* Con]
 *agra= aswn tirapeceptionTOFBLUES)* chunWOOKIE 	}
	sct INIT tions for_chunk;
		/ving t	sctp_senduct sctS(SCTCTRLCHUNctp_f_pdisMake ast s(asopoc))
		klen(p_addr fromit(new_ COOKIE Ef_do_ecne
 *
return SCTLP reque&&e is_lt(sd thlisionsart e chunks),snd/orcialM_UPdchunk  the Communication Up notification to ciation, _8_4_8(ep, asSectiosf_discard_(attempST_T2_SH>c.pevalue 2960 9.2
 * If an cm_CON INIT chuosition of the chunore
 * ruct	if (!sctp_dr_stat local endpunk
acket and take no further
 *    actdown_a
	sctp_add_cmd_sf(commane don't introduce memory corr}
