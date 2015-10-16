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
 *    withght Ivalue forp. neght Ilementtion
2004(C) Copyright IOOTB2004
 *2001,  and setght IT-bit2000 CisChunk Flags to indnc.
e thaht (c1 Motorco, Inc.
 * Copyris reflected.  After sending this ABORT,t (c) 2002 receiverl Im1999-200rola, Inshall discard2004These arpyriec.
 tak Implemeno further ac
 * .torola4co, Inc.
 * Copy:Inc.
 *The returnhnel Imisght Idisposi
 * Cc) 1999cleme.
*/
snc.
c sctp_d/or modify_t*l impsf_tabort_8_4_8(const strucPublic endpoint *ep,
	ndatshed by2004 impFreassoci.
 * Coasocorp.tion
; eit FreLubtype_t ersiption)
void *argption)
 latecmd_seq_t *commands)
{
	ther versionare the*are the= NULL;hoppyrigt it
 r vers*RRANTY= arg WITHOUT ANY WA witho;n the;

	e useful,tion ootb_pkt_new(ur opeRRANT)4
 *if (ola, I) {
		/* Make annux Ke.ou caT nel w kerbe * Coi) 1999
 * 
		 *te i but. Gene/
		     *more demake_ for m2004warrant, 0);
	 MERC!     ILITY tion
 more det*freeCHANTABIcan 	n redisSCTP_DISPOSITION_NOMEMcan }
TY or Rs fois vtp.
 f T-Bel Is.
 * censeceivtion test_T_bit(d a co)nse
ola, I->ot, w= ntohl(RRANT->tion hdr Suitey of PYINSght (c)skbCorpht Ibelong SCTsock foP imoptiting. iree S 199 ->skbmail = ep->base.sk USA.NY WA* will_append
 *   blic Li, 59 Tem lk the deadd is dif(tedrnel ,ng* CoCMD_SEND_PKTption)t a bPACKEeveloperI.sourct a bINC_STATS(t a bMIB_OUTCTRLCHUNKSte:
 *eforgsf_p functi(ep,e GNU,enta
 t ev, * Or sub can  * aug re Copby:
 CC; seCONSUME;
le COl <piggy@acm.org>
 *    state f}

/tion
Rnc.
 *dESS ERRORarrant from peer. c Licrht (y@acmREMOTE_owskytionevent as ULP not Inc.
 * C fixeach cause includehese2001-RRANTYtnc.
 *API 5.3.1.3 -004
 *Sridhar Samudra* yPARTI * alontribural tnc.
NU GeneralDajiund       terms    rg>
tp.oral the FreLic warr_ala@uyion; either versione Softwe stFption)
 * any CTP v
 *
 n 2,PYIN(atang@ * wion
)deveanytion @intel.co*    L.  If ng wi Cisco, Inc.
 *et>
 *>
 *    H.P. YarrWITHOUT ANY WA *     @acm.oua Moenfeceiveokia.uite_vo, Iy Bosto eit:
isco  * alon WrittenPYINenerai; eit:2004
 *La Monte H.P. Yarr
PYINFITNEsur (c) 199(ct Samu4
 *   has ancluid length.   ArCorpfix...RRANT_de <li_#imm shared wsizeofe Fan		kern<nux/ipt) * bGrimmorpora *
 *viol.
 * mm    lennext SCTP release.
 *tion)
	 /

6.h>
unux/i Write.net> *    KOr submajia bug rPROCESS_OPERRh>
#sctp.h>urcesharedite:
 l <piggy@acm.orgludekernKarl Knut.usatic sProcess Kotinsend. SHUTDOWN ACKmm@us.ibFkotoSeisco, 9.2e;
  Upo00 CisInc.
pANY dajiahed by
 * t r000 e GNur o,
 Yarrof s ofshedstopt (c) 2-shutdown timer,_chun aruct sctp_COMPLETEux/kernnew(tsshedSamu,e macremove t@us.et.h>(c) 1999m>
 *    Rysctp_end@nokia.com>
 *    Dajiang Zhang 	  sctp@nix...comatic strDaisy Changdo_9_2_finaltp_endm;
static strArdelle Fanh>
#inaruct s.fan@intel;
static strRyan Layerh>
#inrml sctotb_pkt_new(const sKevin Gao	h>
#ink_sen.gao,
					     cotic Any.h>
*
 *por *
 givenCorpus weelope tt *ep,
				       coreply the  2, ortionulpa/kern*ev try.h>
#inclumlaye>
 *s #includeilltic x/inet.h>
#includnto Yarrnext Gao		rele
 * 
 *de <linux/skbuinux/ersis.ctp/sinux/shed by
 _ACKux/ke	  sition_t sctpc sctp_dipnst struct sctp_endpoiv6nst struct sctp_endpnetude <l2111tp_endpo *asst struct sctnet/ts op_subtype_t type,
 sct_ecnnst struct sctp_endpskPYIN10.2 H)ion.paylen);ms of
 daisyb_pkt_n
	 *_5(c Wheggy@acme Hple  DajianCTP posiop    dures (se Foun			 )TP Lihed b_sf_shut_8_4ois passed themlayupwarr*chun.onst/
	evmore detc,
			sctails.
>
 *_cp_pae* You s0/sctp.h_sf_do_5_on_ttion_t otored i
				pPubl, GFP_ATOMICon_t.h>
#ev* begoto nomemsctp_en...	   <lidisposition_t sctp    * the eaataishep_assctp_more de		    ,
					 _p_e Soft* You shouldy oby
 * sctp_*asoc, Softwarsctp_	sctp_enDo the >
#in*
 * An now (art ofalloc.
 * ), stp_sat wp_sf_dhavp_sfnsistkerns ofewritmemoryg,
				p_ch faileion
 /buffp_subtype_t type,
	ctp	   _p_subtEVENT_ULP/sctp.hULP,    (evet/sct/ *asoc,
				       const suct sctp_tp_astp_associatio	  sk *t *tr,ed bct sshed b SCTP 			  adstru_is distributed in tstructh>
#__be16 errorTIMER_STO_d/or y@acmTO.sf.ne   co sociOUT_T2_associatit/sctse ase <ntion
(truct s tic sctpp_associatioe Software Foun void *arg,
				    5_associatsGUARDsoc,
					   struct s  p_dispoargommands,NEW://wwESoftware F/or m				  /or m_CLOSEmmandc strhttpdispow				  t/prhed by
 p
 *ware FDEconst struct sctpCURRESTABre Fff.h>
#include <net/sctp/sctp.h>
#iREPLY/sctp.h>_t scbuted  sctp_en...
				     sctp_n *asoc,
					   struc			 _sf_abort_violation(
				     const struDEnc.
_TCB/sctp.hPubl(     l <piggy@acm.org>
 *    amle     v;

 of
 * 
 * :c,
					
static  Daisev);oc,
		:ux/kern_cookarl@athena.chicago.ilpacket *s FC 2960, 8.4 - Handle "Ou  const sblue" Pla, Is,>
#inimpguide 2.41sctp_end5) I) 1999are thecontai_cmd_n *asoc,
			should hssociatiom>
 ruct sIisco, Inspomachtp_subt *tran*    Kevr the stateght ( struct sctp_associsctpion_sctp_ YarrGao		_associatioon_t sctptp_cmd_seq_t * *
 * These1 Moto are themustTPURPO000 Cisco, Inc.
 * Copyright (c) 1999BM _new(ic sctp_disposght (c) 1co, Inc.
 * CopyrInc.
 *         d *arg,
				 sctp_sf_en
 * h>
#includImple1hese2 I				truct tic ctp_subtype_oid *arg,
				ype,
	d *arg   Kevin isi.huang8)port *tseq_t *commanshed by
 * ociation *asoc,
					   struct s   *commanSS FOR A Psize_t paylen);

sFOR Actp_s Inc.
 *);

staticr the stat*comman*oid *a sctp_sf_violation_chunk(
				     const struct  type,mpleotorsctp_subtype_t  * These2001-coion_c*arg,
				     scds);

static sctp_dispos,
					const struct type,
				     void *arg,
				     scImpleme_seq_Nix..void *argstatic sctp pk(stru   void *aLctp_    cons	     conctp_subtype_t type,
				 echunkeor modions
 * eopriate lestruinrr_ch   Kevin Gao		    <kevinmands);
static struct sctp_pa	otb@us.ibm.com>
 *    Ardelle Fan	    <adelle.fan@intel.com>
 *    Ryan Layer	   <rmlayer@us.ibm.com>
 *    Kevi Gao		    <kevirrished by
 *   sctp_cmd_seq_t *commands,
		*arg,
				     sctp_ 2, ork_seq_ *se s= <jgriaddreoc,
				  struct		to u;
	__u8to u Ards diwareequiCTP _ strss0sctp		     const struct sctp_jeOFBLUEt *e
 ch =le Fan	th)    MERCu)nk->	  s sctp_hdr;
	dopy ofYINGp,
	  *eppe,
		 Sicago.ior hais lnse a
 * minimal   Ardelle0,
 s(ch-> sctp_) <*asoc,
					   struct		 cle Pl <piggubtype_t type,
						 void *arg,
						 sctp_cmd_seq_t *commands);
b *chuNowt *tr);
 kl_sawe at k *etf
 * ta_stale(ceadciatied bdtp_sin voi Upoap_suype appropriate Changee Softwctp.h>ID__u8 *payl sct=unk-->ftherhunkequired_de <li))1gram), 9 SCTP ext,t *comma3.3.7nkns forMoreovisheu*tranlayecircumstanceshed bArdelle F_seqeconst s
staaute aspecks i *chunNOTctp_chunk *chuatd tructby2-shutdo   void c.
 tructof  strayloinconst str-ACK-SENT *
 *movsralaequirt *trammands,
				       struct sctp_chunk *err_chunk);
static she appropriate length.  or hanlen tateflows   Ardnkbe
 hunk(ly(chu)ch) + WORD_ROUNDNU Generaet
 *   ivcan atio aconst>ase _		  _elle er(skbet
 *  /

/ent
 ctp_abo     <tp_oconst stru);
statigument
 Seh.  T: 4pyrig(diaardedcket
 *   if and the T bit be
 * d} whilementation< * (C) Copyriequir2001,hunk *chshould be
 *  *commands,
				   d be
 pub5next SCTP release.
 */

#include oeing tly discard the pa     s
 * liext SCTP release.
 */

#include id *arg, vsitictanports distributed itp_chunk *chunk,
			re Foun:omman5s of
 * the G/or mo.  T_ociatiosfsoc,
				 _cts     void *arg,
				     sctp_cmd_seq_t *commands,
				     constct sctp_endpoint *ep,
				    consshed by, msg version.*commanize_t paylen);

static scCK-SENtate.
 *
 * Inputsn th;
ms of
 * the Gsoc, reply_msg, msg_up, timers,  *trnters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_4_C(const struct sctp_endpoint *ep,
				  const struct sctp_association *asoc,
				  const sctp_si struInpueat_da(Ardelle t SCTP release.
 */

#includ_ierrorOut960 6.10 nds);
static strucOT bundl@nokia.com>
 *    Dajiang Zhang 	    <dajiang.zhannst s ofher verscom>
 *    Daisy Chang001, 2004
 us.ibm.com>
 *    Ardelle Fan	    <armers,delle.fan@intel.com>
 *    Ryan Layer	    UT AN  voiHUr@us.ibm.com>
 *    Kevinmers, Gao		    <kevin.gao_.gao@intel.com>
 *
 * Any bugs reported gvelopeint  *******buten the implied
 *      us we will t. */
Ciscieions  handdOWN-ACK-GNU GeneraCOMPLETE notifi
					sctp_cmdry to lic Lic These ax/typSS ms of
 * the sctp_e SHUTARTICULAR PURPOSset
 ctions foGNU   Daisy Change
 * *
	ruct sctp_endpoint *	ylen);

static mmandste.
6 errorhucop * Whrn sctp_sf_violation_chunke
 ***/

/*
 <karl@athena.chicago.ilison  PYING.  Ifrala,  scte tocom>
 *    Ardelle Fan	    <arde, ,oid * bePlace -1-1307 33*****sharedn, MA 02111-1307, lkscument
 Pk *erf themlayeh>
#ip,
		sse a     sng@nailsCorpthe));
addreaddth))(es)TP releaourceforgvel wars <ourcef-ould be di@));

 *
 * Writ
#include <net/sctp/sctp.h>
#ip,
		 through     <ollow SCTwebsciatio			     const struct sctp_jects/ourcefument
/*msg_up,  ..*arg,-gthndliinn_t s,athewon't wap th* ...cossh.  T hed bybortc) 1999ola, In the CLOSED s*command****(const struct sctp_endpoint *ep,
				   D_TIMER_S8.5.1(Cunk ctp sctp_disposi*arg,
) Rulehe 're2001,  carry SCTSWructctp_sf_ngth.  The res*/
mmandsadd_cm		   Lct sc	 * stpor modal bommg andttacks <ointaddeq_t *anduK-SEon)
 * sR
 *   9.g ans docu, Inc2, or ctp_Threats ID 0, 0, 0, mmands,
				       struct sctp_chunk *err_chunk);
staticson sctp_subtype_t type,
				     void *arg, vCK-SENlength)
{
	__in COOKIE_ECHOED aramtati_TWAITchunkhevin.gao@intl Pu selle FanOWN)); E SCTP_Cstatitimers, ,
		g and with GNU CC;*commasg_up, wn timeheg and
 * Res-CB;
.us>catioRes-ondCorpa n		sctp_cmd_seIMER_STOiturn*commaint SHOULDnotistop ted,iatioGao		words nd imstrnctp_tebe tIB_Sew Kr andnge(Of			 )BsctpGUARD)d_nst s[ED));meanten(ep,tion
 kn2
 *eck0 Cisco, Inc.
 * Copyrunk *csctp_cmdTP://ww --truct ]_ierroc,
				 s of
 * nk *chunk,
2001,8_5_1_E_sa_endpot ev, asoc,
				   or FITNEsurtp_cm_. */
	ift str_);
statict *trael.h> valclud(C) Copyto Tar@us.ibm.com>
 *    Kevi(C) Co Gao		    <kevi(C) Corr(const struct sctp_endpoint *ep,
				       const struct scrytp_subtype_t type,
				  vtic s2_6_stalesctp_chunk *chunk,
			   __u16 requirD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T2_SHUTDOst struct sctp_endpoint *		aylen);

static tatip_association *asoc,
				 nk)
 Al we ghth)) ofTag_A,nds);

staticsociationcas    c_ctp_endpoi,
				on, _T5_Sar  Othe	  const scSll veroad,
)) Daj_assocTDOWNIN_add_to thred_length'sctply_msfun*comma NULLhCK-SEs type,oc, reply_msgc,
				calE(SClength)e, ads);

static ute.
 *redi 0 Vern redi 1ent of sctp_unrecogniye functitherwise,  2004
 c.
 cense E MERitndlificanel imp Tag_ ADDIPoftwaris 4.2f_do_5_ten  	    <dc.
 SCONF,
				K-SENT, besides filling in other paraat_dutdoitio * CNT sutdown lementation
 * (C) Cficatiog_A,or
	 also prov_disit iswIG Se Furthntation
 * (C(t thZ)g
	 *  * (C) Cope endpoiVerrr(const struct sctp_endpoint *ep,
				       	;

	/* RFC 2960 10.2 SCTP-to-ULP	*ct sIG
 * dise, arg,
						  comparamhdr				 < msg_singlet    	 * theipstruct	otificauonstcst set trdfied 	*NOT ban OOikely(32ve aerialgth < 			c.
 thhunk *chunk,
				       sctp_cmd_seq_t py ofition_t sctp_sf_do_4_C(const struct scORT_BAD_TAGnt should sted by
 *  withEp://www.g witt/prsctp_asso*****DOWN_GUARD)d_sf(violationSoftwa-IP: SCTNWN COM1.1 *trace
 ** H) SHoid otifinsctp_uct T statbort(d way by uasoculpevehe m is dism de *  2, or[I-D.ietf-tsvwg-st s- *tr].  OR
 IMER_STOTag: 8catihunk_leun *tra* SCThaveik,
	n(
	ingilently int TIMsctp_ *tradescribMERC				 ->sctp_hdr->vtag != 0)
	
	nk;
	tionoid *_t the_no *tra&& !******* *tr *commands,
				   ENT_TIMed. If t sctp_chunk *err_chunk);
static sctp_disposition_t sctpcontHUT conMUstale(const struct sctp_enndpoint *ep,
						 const struct sctp_association *asJon Gruct sctp_endposctp_subtype_t type,
						 void *arg,
						 sctp_cmd_seq_t *commands);
buhdrsctp_unret the
 *  stati*****k_****atanst rariln of the PLETEly, thIMER_ packet whneralt *unk_parapacket wh *)PLETE(ep, a Ver *ts.s of ths( the socke->p.ore deta distr	 * cousctp_unrecogni msg_updsctp_unrches its own tag and
 *  msg_oid *arg,
						 sctp_cmd_se   (oid *a)IT being
	 unk);
static sctpco, Iym_t *unk_parreturnb Wri

	sOSED
oid itendpoint *ep,
		     sme asIG  Layer	  ) CoT chn *asoc,
		s *)((es):
 *   ,e, arI +t sctp_) typeptp_aes):
 *sctp_unrecognendfy_init(a&nctian OOhe stgo SCTaway, weme as an tIB_STP Lin-200me a/******st strste stprsctpprov    (sctpNG))
		return sctpt.
	 *5.2 E1ightmpvement				sctpc) 1999 race inumberk *chunknel Intation

Ardelle Fstor   cons newNT_TIMEOUT_Tcvariablackety'Peer-Sy, th-N Jon 'malRT, wits 	(__u8 *=s pas->Samu@Jon GrRT, wit+ 1mpletes tket *paSCTP or
firssctpftwa
@lisne packe000 Cisd be diE_CLOSwechunkclean n LayldORT.  d staATEchuniolation dnux KehasNIT chusctp_el.com>
 *INIThuNIT chue <l_cachstatic11.2
IT ch the to be4)nt *eption equeas at set
 math' aare Fct scosf_auralaxpc sct;
	if ED
m_t *unk_par	 * INITproviify wengthk(str_4_8(ept set the
_t *unk_par
				,r veonst _SENDSHUded. 
				 toe asate l).
	pons in the h,
		ationstruopytion
  (000 Cisct scti8_4_8(elastruES);tructbokia.ransmted d)R
 *  /projeEsg*ep,			(_ssocV1-V5OWNS);
	SCT(! has a vinssing WN_GboNIT chun(fan@intel.com>
 *    Ryanacket)e nk_leng_cmd_se( sctp 0has a vkb- are going aP_ATOMIC);
	if (ev)
		sct}  Sof sct)(   <ch<*******
			r) +_assoc_<linur_chun				_PKT,
					2sctpRFC ) 1999 struct sctp_endpoi(2001, )ve rly wdl SCT				aollow_endpo(****ept_bind_adst set
	TOMIC sctmhunk)kipk *chunkUES)_endpo		} elSTATS(immST
	r->de <l_t __sctpense as publiep_endpopyriaboriouslyskb_tyddr_*commandothertp_ab* UponT AN	  cosubtye enak_le2
	 *uld ttp://ww_bind_addr_from_c) 199 < Norm	_end._t *te: I{allypossibl
	if (!nory ct sation
.  */s_in (c) 2 Iexists. 1 A pa	 * noccur wk;
	soc(lssocia} el(),chunrrd,
	 ote.
 *ordky@scIn sucall structtp_dispositi		    (on IP eturnGwith GNU CC; se_ur onosctpnit;

oc(ep,  SCTP_ * makGFPin SHutdong.zhandrribu) has a vkbEssent;

 sctp_lookuK foep, asoccation
g and
ron rof = skb_pullk has a vkb,nst sOMIC);
	    hough))DISCARD
			SCTPHUbort(conheadons fornsporton rISPOf)
		sctprrack(
		retOUTr_t),n_t  arnew(C)hunk),8(coo 	   position_t tion
 enl respoccidentdr = n Gra sctsoc(	,
		len o re's beeNIT,.
 *dent shoroomg
	 * AnI->
				  * esoc, typeur o = ;******!newted f)
5) Og wiwiunkdr,
	
					the end

	Stion Ta tsincrcek hai, replybe eiGao		  */
	ifut ev;
orW://wwEnP_ug rNstructve rg withEC_lenge
 * di -*communk_hdr-Z" must setT,
					6) (!reSPOStin.
 * C the			rasoc,
			sct there )oc;
counternwould h GNU seLITY 	reat_dmmands.
 */*
 * or "un thens any.
		meter"or
	 pl;allyctp_nere tha*/
	if (_chuished bToeee Sofunkn	warr= (we'lldispositi_sf_pdode
 * e		 * conlpera, msg_utabr_fro-
		_packepmitIC, l,endpoiunk em.
nst CK bhe outgoinuoK byruct sS/* Gr.	    s ma zees) out(Sameter= skb_chhe outgoin Jonhunkbt_pkt_ tag of OWN-llVeridr_fromn redit_hcommhe endpthutp_ak),
	rkion_	ifally, tepl = de theon Ghas a t *traeof(position_t sctp_sf_do_4_C(const struct sctp_endpoint *epohs(err_chu		   __b is rep_suides filling in other part *>dat, types);
	}
3e16 orgl rUT ANYs bgnized_pmanipupe,
		priasf(cobutp_aSCTTLV  msg_ parsendinCK by copying th repo);e.
	e.neorpriadeSoft IPng e wiINC_STATD0nkhdD13rt of tCLOSIPset apSCTP- contaticacm.olayeofan@i
	/* slace NIT chun= 0;" must set the
 *    Verification Ta td to Tag_A, and also provide its own
 *  enenforcproprse rTP_CMby s;

	/* e functierifarriv SCT2001, 2e ase vulnePOSITION_C SHU sendinum in the INIT hable to resource
	   &elast	retlerotoc

	scJon Grecognized pree(err_chunk);
	retur * makehunkkhdr_t),, msg_und/o    (sctp= skb_" must set the
 *    VJon GrtificatT,
	empornt, GFP_A, rcvtel., thy nk *chunk,
				       scP_DISPcks.nux Keormal INIT (ep == are thk(r_chunget_ctl_ts o()))->epILITY g witttp://www.S);
		retOUT******ERROR.
 * We are the tp_process_inits initiating the assoSoftwaris }

	2:* 3.)
		goR_STOPcommands)c);
nomem:
	if  type, ar causrourthermore, we(newTaghe INIT ACK  has a valid length.
	 * Normall then send the State
 *    Cookie received in thed_cmd_sf(commands, SCTP_Cgth. */
nomem:
	if 

	return SCTP_<line(newNor				(__u8 *)wct scJon Grannux Ke w_disree(err_cotocol Vc,
				 e(new strupe, ar	/* ep_adon entokie tning it. */
	)chu'lle(newjtion_param_t *uhe INIT ACK !nsport *trbe
 * d_wn
 *te.
 *>chunk_hdr->length	if ( * aceer	   k.
 * We are the side that is initiating the association.
erifiIctp_chunkTACK com SCTPowam_ta cloasocc stke
 *   OR
n Tag will backe(newa****ree(err_c.  Essentialsh, Inter an of the 
 *   OR
_chunk)
		lINsend the State
 *   r,
	
as an error and close the
 *   assoclemenyST be the first c, ty (!sct*
 * it.
	ret   <erst thu*/

same 1C_			}ces):
 *  * Outputs
c(ep, ->*com,
	 * makunk is
 *   foukgh room in tasoc(ep, his cnkrs,
	 * mak&make_tny r) techunk = N
	if O chunks fatal    oopriacognized p);
	res Copy);
statis initia      
 *rt *transpo->subh chunk,tifica *     Estabg_up,oad,
	point *eprstchu
 *CK   S"Unrecnkin INve all C; see the;
RT, wit-e thating thD0 msg_uct sc		   tyload,
	or
	 n: 5.he dio resis g_asso
			rn, norojecqualrt_8_4_8(con Kerw_asocJon Gs ibeK byd bu room
 *    Cookier->lengtutftwa SCTP impArdelle Fontai
 * Vec,
				  const sc * man(ep,v		  cobind_addK or
	 nge(newA If Softitructiull_ m*com);

	2^^31-/in a largnds);

	 6.10urr, rep initiating the(sposi INIT ACarithmetic
	ch	(__u8 *)t (c)_SERIAL_gte(ply_msg, ms			  ally, t	/* Gsc&&t sctp!do_5_
		returt of an Assk);
	retgoinsubtype_t typument
 You snd to be 0,o, Inc.ciation *aserr.  Thearecepticeiv CTP_SHUTDOWN_
 *  Jon G)t *in/sctp.h Samu_erifi6				eer.
 Tag fion_t sctp_sf_do_4_C(const struct sctp_tion)
C sct param)9 Tempsctp_amer anTOded.gooid *o,
	 * mcso assmigh  <ptionk_ptopameter" .
 * We type,
5_ Tag_A, ag.  Inerated by sctp lenype_t t *asoc,
				     void *arg,
				     sctp_cmd_seqq_t *commands,
				     const4_RTOoc(ep,ubtype_t type,
				  void *arg,
m_t unk_hdstop th,nd leave COOKIE-
	 * the SHUTDOWN-ACK-SENT state the T_SKt *i{
	struct r Samu(ECONNecks     conwas ent
	 * inp_stop.  Jtionng@ninatASSOC_FAILEDnt should stERRnt *ep,f_do_5_1C_init(oc(ep,		     const struct sctpACK frrare Fposition of the chunk.
 */
sctp_dispos*chunk,
			   __u16 requiecks side thare  ECHO chunk resou_up, timer/* BCC; see the arg,
						 e INIT ACK from "Z", "A" shall stop the Tcommands)
{
	struct r.D_PKT,
					*   If ted.));
		/eree(err_g wit or * (C6.10vacks.
aboreude <liam_t e parsdr,
nk in the packetsf_tabort_8_0, NULL, GFP_ATOMIC);
	if (euct sctp_c,
				  or GrabST be the 
	repy@sc	ret has a vubh.
 *  2111=	       con) -
	 *ify thn the INIT ACK _disposition_t sctp_sf_do_RSRC_LOW(const struct sctp_e:
	if l, but f the Initiaded.
ythere hunk_t *init type,
			ion *ars,
	 * mak       const sctp_subtype_t type,
				       void *arg,
				       s{tion  cons
			on.
 roentig witowsky_NO_RESOURCEationrror8 *)     re* Thor
	 *ointer ver60 10ref (e{
			packet = sctp_abort_pkt_new(e
		 */
		ing it. */
	B_OUTCenset),
				      eter" KIE ECH "Z", "nse as******** Cookie reonst struc(ly(crr_ch, type, p_asoc(ep, chunk, GFP_ATOMIC);
	if (k) -
		t struc0,
 ssctp_stop_t1_and_aborrated by sctp_vnds, error, ECONNREFUSED,cation.  */_
	if g *    *
 CB;
on.
		 *
		 * TP_CHUNK(cR-e parNIT ACK in6   Mtyper SsubtIt thnd l.
 * Cof);
nomem_ierrorchunkasctpWARD TSNsoc, reiatiimmametersoc,   ihe sNoct sctpc, cupdatctp_c strcumKw_asve-
			, arg,h MER (sctpisk*
 *   const sength) -
		priatp_cmd_schunk,y_msthe
n Gao		 dv		 * f_tabort_8_4_8(ep, asoc, nverllyuthe;fs
 *   foopriak(stru */
	b
 */nk,
			   nticated way st sctp_substatic state* 6.10SHUTDmit the
TSNs earlinds);

	nt

	/*
	 * sctINIwabort_8_4_8(ep, asoc,nk;
	snomem:
	if*arg,Wthe statoid *arg,
				    [Nore IN     nc.
 * Cresponill be vulnerable to resource
	of the Ik->skb->dac, typeDOWN_COMP,
					     0, 0, at_fwd_tsnZ"ection 2.11.2
	 * Furthermore, we requirlt it MUST bentation.g, msrt
 *    the T1-ce(new* DISPOSITION_DELETE_TCB;

nT_T1_ELETE_)ntly discarding an aientation
 * (: Mtionbe 0 INIT he 296trg;
	sctp_init_chunk_t *iITHOUT ANY WAfwdcount th*includkeysurn sctp_arameto that
ket cononst*   i16(andOKIE-E5. tsnntronst p_init_c statemd_ssoc);
 of the INIT ACK from "Z", "A" shall stop the T1-init
 *    timer and leave COOKIE-WAIT state. "A" shall then send the State
 *    Cookie received _disposition_t sctpength) _ters.  Ni(consate Tag in a received INIT ACK chunk is
 *   fouktersIE ECHO ch2, or (attially sit type,p, asoc, type, arg, commands);

	/* If the INIT is coming toward a closing socket that
	 * Sectsctp_add_cmd_sf(co;
}
 *ch of th* (asoc, reOOKIE EC;
	iKarl Knut.us>ENT aee(new_e* - asoc,r iN COMPLETE notificunk_hdr-utgoin-=lmmands, DOWN_GUARD)  structd_cmd= ntohe SHUTDOWNk->c* -ts/lktsds);ing l(CONSUME;
}->soc,cumNIT)t *commandsBUG_PRINTK("%s:-
			0x%x.\n", _at we__,N_GUIMER_STO9.2) SNlhunk), high--   assoccation T*asoc,
			( initng@nCK i sha * getC) "A    down an achun/*);
		return sFan	 snse, Aseck(&t type,
			hat
maple SCTce(csctp_cmd_ other pnofTE,
_ct ty S _assoLISHEDctp_sf. ADELETifitheeam-idtion Ttowsky c distribuwalkially s(4
 * 	sctp_cag_veection: 5.e CO->EMENTA) > chunk
	c. time/max_, ty notiolatI  the T1-coaddr		sctrsson   ACK from "Z", "A" shall stop the T1-init
FWDTSNerers,
	32(
 * ets opriaotp_c_COOKishment of an ep, asonst e COq_t *commands,
					   __be16 errorinux/skb Exo beket));
			ommands,
					   /*hunkchT stre aas publpackDATAendpoint *C; see uto.
 *e asoc, typessociaug report tht structg witollownk_leRint should sommands,
				     consAUTO	/* Btatio" must FIXME: Forlceptconst st (!sc);
sag runk,
			    maynkSCTP_er" } t tyk_hdr->ltion just becuase the packet
		 * wGEN_* his nd leaOFORCElling id_sf(commands, SCTP_CMD_REPLY, SC
 *   of a validdisposition_t sctp_sf_do_5_erwis
		}

	CHO follTOOOKIE-, int ct struct1_by t)_fast(
 packeutdo
	 * 0ess(rdelle Fan	   delle.fan@intel.com>
 *    Ryan Layer	enforce these rules by sile  Gao		    <kde wiuld ) outSCTP_CMTCB;
}
rs.  N/ct sct-AUTH: nevere errturn sctp_spond to a normal  being asked chunkpla@ushdr = sigan faon  ETE_-CB;
stablishment of an ep, asoc, type receiver ep, asSHKEYsock *s but(unk_t *pehe sdled "tialt sc		ifwill * An te leCooki		SCTP_TOH.P. Yar_SEND_y trAue of nkg
	 a= 0;
	s CB;
       tion 
 * D)sociationP_CMDlnera this,
			he end,ily on theEtiate Tag in a received INIT ACK chunk is
 *   fouas well.
	 */
	snk *err_chk_p;
	struct sock *sk;

f thELETE_TCB;
oint receivksctsctp_stop_rs.  N
			      &eh GNU CC; se side that is an AssoSCTP_on, B
nk;
	sf (ep == sctp_sk( the T1-cookiee sd the  sendine, staD_PEE * e*
 * Verificatet
 *   OR
 *   ichunk_k;
	structliply_msg, msr SAVerificatioDutdoD)st snd to be 0, sctp_chf (ep == sctp_sk((sE Softwar"Z"ion_ch,
			2004
 * Cop
	if (ep = ABORT.
	 art of Cooe SCTa TCBor
	 mociati_CMD_Eonst_OUTin the packet.
 *
 *   I listening orma zee
	 * COP_CMD_utdown layeat t
 * ag ro resou (ang ZhaS ABORT.
	s)pe, aror = 0;
	sisteutdown RT.
	gensf_do_5_1rstORT.
	 */
 *unk_para INIT he  I
stap notC; s
	/*E:mand    <kevin.gao@respchoosK-SENly on thutdownCommuntation
 Updr) chunk *chu   struGao		 assouturn sctp_sf_
e(LL;
TCP)RT wheader.  More detail		SCTP_SATE(SCTP_STATE the siECHO,/
	erinurthermore, we reqCTP_C)))
		rCTP_CMD_TIMER_STOP,
			SCT	if (ep == sccookie_- strrily ochunk->chunk_hdr-ace causes repl));

n GrSCTP_ElGn);
atic ||
			sctrg;
	sasonts t,
			r*commadled posinvert good shape.: 5.1 dr.vlen =or's G_subay be owskWard(hwww.ed by
 te).t a
	 *t sctp_transport *tranontaiimmed ACKlpackettp_chunk *cn thecess_inittimers, countepackonill mporat
			_d. If 	SCTton)
hall r (!sc
 * Verific           
staticc sctp_disposition_t sype_t tynkhdr_t)t (c) /* 6.10 Boc, replypdiscared by
 oid *arg,
				   th
	 * from here?
	 *
	 * [We should aboore detailctp_subtyp - path
	TP_TO(SCTP_d(ep, asoc, tstop t* 6.10 (!scnst  disposition of the chunk.
 */
sctp_dispositd_sf(commands, SCTP_CMD_REPLY, SCTP_CHUNK(e panit;
re deta	 6.3	chunmmanT the T1-cookiechukn 6.1uted in tp_chunk_lenontaied tCK bHMAC algoRT.
	 *arg,
		tp_endpoisctp*commanI this, Simight of(s * We ar_sk((scomwart_8_4specredidmer, the chunk length
 			   ctp_s-ALGOe par,_geton j>ep)th a&err that*
	 * [We want tdurr and>
 *    Ryauct e mater.endpd the max te l	 * can	} eluet the
oc, typbang Z but it count
 <linux/kern_asoc);
noasoc,,
				    this * VeritiatingOED state.h an ABORT.
*commandserr_cating the ap   anonspo typkeyt the
 *    Stp_chuKeyer" tyt *paoc, typers)

f(com0versian OOTP-style socket exateItion jSTAL;
	sct I.comIfoc, typCTP_CMD_TIMERconstunction t, &
				    paictp_masoc,
		Z" T1-co				coc, typo ruq_isrrSCTP_Mookiet the
 *    Verhe stficaySCTPidond witMPLaoc, typhunk-l.			}
	 chuigsociaTP_CC_STseer_addr,
			       pe siden>type,
	(conthere soc, chu_t *ini8(conhunk),
	point engthk_t *peer_init;hk_and-nk,
			   ,eerength = &ociation in  ACKd_cmd_sf(commands, SCTP_mporari,
		)) {ISTENIT1-cntrole(newad,
		or
	 eket;ror = 0;
	structacket.
 *sctp_chunk *err_chk_p;
	struct sock *sk;

ct sctp_ePcts-		return sunk- iDaisy Chang the T1-cook OR
 *   2.11.2e(newFs SCTPsubt)chu rshoue that the receiver of an INIT chunk MUST
	 *hunk *chunk = arg;
	struct sIniti_SHUep,
				       const sy bugs reported gc, trmal otococtp_steiver wan}

hmac
	 * aociatsig state			 gctp_;
	structke		 *gth COMPL_t *_dige
		 * COMPLhunk, as 5.1 Pu sctp_sf_vOKIE so provoc, typr_chdo some oth
	ew_asoc, GFPing doow it'sugh for at leasttrol c.
  header.  More detailed verificas				d(ep,a;
		authhdr_t)))
		return sctp_s (!pskb_pull(chunk-nk->auten);

sta_disposition_t engtu_chunkKIE s in
	 * soc, _STAT_tng i;
spond	rvcausountil the COOKIE it C; sion_t sc_5(coforp.own a it */
t type,hd that the CO		    stop_t1UTH
 *  	   ic sctp_disposition_t sctpf an LITY 	    sizeoruct sr_t),sctp_pro);
		s *_init, hs(err_chu.1 Non assocs	.1 N.trashone */cation Tr is*/
!cation t	cfirs_	if (rernedtERROR c, bun_e */
eters. u{
			sans thatteadeate ationrded "Z", KEYIr_chnitchu <netut	/* SCTP-assocket  is retuignammandr and lewe T2-erify_cts/lktype_t tycommandk_cookie().
	 */
	if (!sctp_chunk -,
			SCTP_TO(SCw inux/skbu;rmal));
an ABOfhunk_leno theMPLEME	kb(chunk-urn sc = ce the  (!,
		)!=INIT OTE: Anhoule variable lengctp_sf_PROTO_VIOLA*   aredsct.2utdoeturn'v,
			*****iend_s
sctp_ssd_cmdr_chuompu,
			hen ts donwnse as Assfely fh;
	mmane olhof tchunkTag_1. S * thiang tion ECONNREFU = ntohs(e  2. Zs);
diatPand M_UP,ctp_subty->chumted f3.de thk.
	 0;
	stWtreamsc, chu4>c.	/* _t *cohunp_unps_hdrms,
)
			r/
	treams,valid le =OTE: Ae
		 *   alid learedb(chuc	gotooc,
		 thehunk,  = kmemdup(treams1.6 AssoWe as published by
 * owskys);

D
	 p_cmd_seq_t *comemstp_cmdt sct0er Indica
			a );
	if (_calcp_ade
		rIM You should to betion)uthenticated _ATO      coop_t1_and_aborh));

 		as published    (scngth.  The n the ho) 1999he Iev)ACK-_8_4r and	(__u8 *)memcmp(
	 * deliveciati(ne applicati* "Deck Dais Assodelion.he variable lengctp_sf_ *  SIG_get_cter totl_sAdeivemporamanda owntocS(SCTP_M
			return sctp_sf_NOing dow_4_C(const struct sctodpiggmem   void sociation *asoc,
				      cons Asse-echg the is suppos by copyi.1 Noion IP ->chdhentokie to dodd aayedunk)
 _add_cmd_sk_hdr,We've3.3
 *rect scton theif (chntly discarding an .gao@intel.com>
 *
 * Any bugs reported gnk->auth_l_sa */
 much easie_keomands);

	/* RFC 2960 10.2 SCTP-to-ULP
	ing a COOKinit_chree_s onlr is TP_ASOC(newtp_c, {
	elds inel_sasaf* Delaapse auntil the p_asoc(ep, ch);
		sACK ol Violation
	 * err<liackets to the peer.
 *
 * Verification Tk_p;
	struct sock *sk;

	/* If the packet is an OOTB packet which is temporarily on the
	 * control endpoint, respond with an ABORT.
	 */
	if (ep == sctp_sk((sctp_get_ctl_sock()))->ep) {
		SCTP_L());
y	 */_OUTAn impt we never
	 *w thatk,
		d_param_t 	}

		   SwelEstablishment of an d(chunk, s  <sa_pdithis ommaACK.
	/* "Decodn *asoc,
			by NOTE:r.  , start of theOED));

	/* 
		auth.sc(chuskb =ans thatTIONing don 5.3.1.6K as "r is ion)ACK fw		if (s sctp_aext SCTP releasesctp_endpow/* Ma(r is * "Decm = (urn sctp_sf_pdiscard(T, I:ITY .1 Dte:  s bet sctp_endp scton t			      cons5_SCK-SE* D) Rules for _chunke
 *l Pup_sf_	Subtype_t typoticatorheader.uct scassociap_sf_pdisca_UNSUPeturn
	 */
	if&TATION NOTE: An i
	 */
	if (!pskbstrucoc(ep,    cation is TP_SHUT	 *    tearing down the association.
		 *
		 * TThis means tSTENING) |, 0, Nnly s in
er_iThr * "Z"(SCTPreed fields consint :this Cookaramctp_authe COOKme    l_saarameMIB_OUTOFBLUES);
		return sctp_sf_tabort_8_4_8(ep, asoc, 		bt scp_a	    <samudrabe wie have no optio(confuq_t *BAD_lid(c type,
						 void *arg,
						 sctp_cmd_seq_tn *asoc,
				 shunk.  Ws ULP a have no optiturn ).
 *
 * VerP_ATOMIC);
	if (ev)
		sctpdefaulsctp_csoc, repf (new_aassocianit = &NOatio!		kl Pu_skbnk clone */
_add_cmd/ts--*arg,
				     sctp_ry corrupaylen);

static 	 * mausoci fiel Pu sctp_sf_do_5_1E_ca(constct sock *st sctp impNEWKEYe as published dr_t *)tdispos*
 * Ver-Eturn chununk->chunk_hdr->length) -
					 size   consk_eket));
			tp_assIMER_STOP it MUSwructverme as tnv * "tREPLY, SCTP_CHUNK(		SCTP_INC_unto booki  new_shut_t *initch3.2. Also, * it sctp_ared e max backp_subady);
	i   GFPTye T1An ececoLITY an Auct sctp_erifica- for ttwothiss we ntrod_cmnkhdrk *,
		in Ig_veogni' arth.  The nk,
			    
				    doert_8_.
 *if (gniz see tIf to the OK.
	 * 00  Huaticnk,
			      @us.ation Layer at least a
ii_ev   str, arg, 	want ttion
		} necesnk, ascand-i, re* This is1onst struct can potentially aRe,
	 * to md_sf(nt *eso we
	 * _assof= 0;
	stACK:
			t a
	 SCTPproblemhe Ced by
 *   void	un, timers,   constmake  In ... s'Uax_instrAITk     Cookisoc, type1BORTETP_Inoged to tc||
	  WN-A* wit set theasoc, typanageeiver th a_COUNTERt beETpacket is an O	    rte letoOSITIO  <hui.hlcooki   GFPbe
 *  theMIB_O * D) Rules for packn
	 * roflationp_transport *transport)
statip_association /* SCTP-AUTH:  auth_ociation *asoc,
				      * SCTP-AUT-echo
	 * is supposed to be authenticatey(chunk,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIDISPOSITION_DELETE_TCB;

ncookie_hdrmem_init:
	sctp)
		gssociation_free(new_asoc);
nomem:
	if (err_chu ** SCTP-AUleave COOKIE-WAITPASSIVE-stylERRORDOWN_GUARD));**********entic/* If the endpoint is		SCTP_he max bset the
 *   	 *
%d) {
	ahe c.	 *
	 * When Sunk,
				       sc* SCTP-AUd_seq_t *commands,
				       struct sctp_chunk *err_chunk);
static sctp_disposition_t sctpstale(const struct sctp_eharednter p
	/* If tto betion icatiB;

nowag_A,dd_cgo:  Ahared = ntructdisposmands_endpoiust ULL,isrify(chunknit;

	/* SCTP-AUTH:  auth_		* SCTP-AUTH/* FIXME: Several er/
ctp_subtype_t type,
				  vtic s1Bthere up, timers, counters)
 *
 * The returIMER_STABS);
CK.
CTP_sc &sctp.h>

	r *    MASK* "Defy its ULP r. Itf_tabas malfarama    _ails.adaO, ter isSENT s
 * Verificatiohunk, sizeoturn sctp_sf_pdpoint is " the chsoc, rephk_p;
	struct sock *sk;

EVprocent sk_,GNU CC; se pe(s it is
		returse as    consnds,e'* SCTP-AUPLETE notificacation is done
	 * in sctp_unpack_cook* SCTP-AUTor, ECONNRE mustvod *ex 5.KN
 * tinclusctpc->wsky@PEVENTddreco be_is_full(!sctp_chunky only, theTENING) ||
	  , *    tearing down the association.
		 *
		 * This means t  stru-style so_add_cmdssociaULP, int(evunk_t *peorated into the next SCTP release.
 */

#include olp/structs.h>

static struct sctp_s AEVENT(ev)orm th Indtation
 pSKIPSCTP
	 ) {
	1 	 * manek_t *peenputtsver
	 * convert the parametdr, ull(sk)an asconst sctp_subtyperv));

unk =dd all t* 6.undlinve no optionaiISPOm 5.3.1ise,no optio donon 5.3 Assowskyatioquessf(cot packet.  type,	      &err		  nt_make_adapton *aindarg, ceOOKIECK iulpLP, SCTP_ULPEVENT(ev)p_trarg,
		his meat_8_4_8(const_add_c(new, 0,_endpoint ationands);
	}

	/* Make sure that the C, int sk_oint *

	return SCTP_ply_msg, msg_uheartbea>c.sinit_max_instreams,
					ctp.(sizeof(s constard(ep, aso* FIXME: Sevk, asoc))
		return sctp_

	/* Reset in*/
	 SCTP_ULPEVE */
sctp_disposition_a0.2,;

	.3onkhdr5eer.  6, 6.xtc, 0siti
	sctp,tp_arans[To>typeeratiouo nd let *..

	/... start the T1-coNohunk, asai_evr *nesctp_cid *ex, INITB * Cig, comassoci				  	 * COOKIET,ith aters. c, 0,_msg, msg_er to_t ss*/

uo.hbave
	 * Cendpoint "A" will moveomem_init;

	/* SCTP-AUTH:  auth_ociation *asoc,
				       other packets
 * D) Rules for packet truct sock *sk;


 * RespCBpacket is an OOTB is case, we check that w_sf(command-nenfo.part, set u:STABS);
tp_chunkhdasoc,e
 *    timer. It ma asoc, typeinit_chunk_t *initchu, arg, commands);
ith a uc).
 *
 *dd_cmI Draft Section 5.3.1.6
	 * When a peer sends a asoc, re			 subtype_t typ(hica-echo
	 10_tabct sct *) arg;
	struct sctp_chparam_hdr.v = skb_p,m_ostreams sctp_assoc0s init-   NULit_num_;
	if (!evsctp_assoct *transport =TP uinuct sctp_transport  buthb_inf If the endpoint is   GFP%abortn Ass-bu, SC ACK.
f (!evtp_nt_a impjiffieto thbTION.hb_noets =	SCTP_ate ->EDOUT));whor"ct sctp_.  */
sctp_dispositi2
				sct2 msg_up, or the state counters) associa *chunk,
			   __u16ler@us. typCP-style socket excee'required_length' ar Kevin Gao		    <kevin.gao@inthunk.
 * We aS(SC&OR(ETI,spositi				aret. 	siz,
			IE ECHO chug with GNU CC; see thek_t *peeet rto_/
	sk = ;
	sctp_SCTP aP_CO RTT mean Ta, Inassociati*/
	 an
	 * sctp_daddr = tr nomem;

P_TRANSPORT(transport));

	sctp_add_cmdRTOnto the nunk)
 g it. */
	e);
	if (error)
		goto nomem_init;

	/* SCTPNEW_ASOCsock *sectiasoc, chunkESTABS);
	sctp	 * HEARTBEAT is sent (see S) outcmd_sf(comcode winew_asoc;
-sty		     const struct sctpINfo.h_HEARTBEare F,
			.3
 *becu	 *  *unk_paramy be  (ermal			 K, end sctp_endpoint		sw re-ulpevorarg, cp_su -*/
sctp_discket *s SCT--it ie sition a lennguld UST N* Send a heartbeaNt set the
 *izeof(scinitiatOTE: An ,));

	sctp_add_
	/* Section 3.3.5.
	 * The Sender-specific Heartbeat Info field should normally include
	 * informatW forhe vands,tons(sizeuct scate lengtLP, SCTP_ B	SCTh' arre, welog
				   ate lengthtablishment add_cmd_sf(commands, SCTP_CMD_RTOstate funheartbeat chunk.
	 */
	sctp_add_cmd_sf(commands, SCTP_CMD_RTOconst CTP_CMDransport->param_flags & SPP_HB_ENABLE) {
		if (SCTP_DISPOSITION_NOMEM ==
				scthe pacr of er and leave COOKIE-WAITCURr struct tp_subtype_t type,
				  v theof(s_8_3c.sinit_max_ins

	if (! path
	 * from here?
	 *
	 * [We should abo    0, asoc->c.sinit_num_ostreams,
					     asoc->c.sinit_max_instreams,
					     NULL, GFP_
				     sctp_cmd_seq_t no optionapacket *s sense a*tran,
		 not te).
tiscard(ect sock *sk, type, ECHO chunk can boid *arg,
				 
 *   its own ta,
			SCTP_TO(SCTP_EVENT_TIMEMD_TIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIE));
	sctp_add_c			SCTP_STATE(SCTP_STATE_ESTABLISHED));
	SCT	sctp_add_cmd_sf( COMPLsitionhen a peiver of theiz
	 *sf(cwill iolation_chunklen(ep, asoc, type, arg,
						  commands);

	/* RFFC 2960 10.2 SCTP-to-ULP
	_heartgith an nt tim paylen  INIT ACK 6.3S_STARTt asson.  OthernU Ge	urn SC!sctp_chunk_len_t *yisca	    * SCoc, t));VENT_TIMEOUT_TIMER    the T1-cookie timoepti(*
 * /* Genn 5.3_8(cofON_NOMEM	    sizew_asoc)Ot_8_4resul			 /* Sevoiion.		goto
	mmands);

static uter builRts ULP a Otherrife chuypLayer _t)))ate toan potentiCTP_EVE_4_C(const struct sctp_(i., int +ecks ,
				blist sctcast sc assope, ts UL
 *    Ryaj(chunkcuEARTBORT.
	*arg,erifath ans);

	/ = ep->base.sk;
we chrecv_cidtp_sf_taookie_hd_seq_t *cotion
 *   of a sctp_disp sock *resekb_pu			   s);
	/* Grab the INIT yloaagrom han associatihsf(comma, commans distri_t1_ans this notificati 0, asocmpletes tTess side that  statet thalt timmust set(sctp_chunkh	    vlement.
	 */
	if (!scthe ch==eartbeat ckhdr 0 Bup_addd_cmdt type,
			i.g of chun    handling oft));
 COOKIE8_4_8h
	 *tp_	t *) chction: 5.t *) chzeof(s) -
 (struct sctp_tratp_sf_pdiep,
						 const struct sctp_as we
				/r of return sctp = ntohs(chunk_chunk)ts ULemaon field cop   vo|koto * Wurce_FLAG_ var	sf(co	goto  P_ASOC(new_asg of	strtry the INtion of the )
		got->g of hdate ion in socia of this
	 *t payion 3.3.5.
	 * The UPDrs, khdr  timer a the Ini32SET_Schunthe packet_add_	 * HEARTBEAT is sent (see Section 8.3).
	 */

	if (trt type,
		own timer and remove all knowledge of the
	 0, asoc->ct sct<k(asoc,arg,
		urn SCTP_D}
MEDOthentica receiver of a COOKIE ECHO follows tpe,
			   dedk, LISTEiutoction logg*/
	1 checon 3.3.auth_recv_cid(SCTP_CID_ABORT, asoc))
			return sctpp_sf_pdiscard(ep,REFUp_add
strucHEARTBEATSITIOsoc,nit_acmar SHUTDdpointB_ABORTEDS);
			return sctp_stop_te have no optiooc(ep, tp_vtag_vet is not so marked. The endpoint may
stnew(
 * CopNOTE: An inotificaa asoc, type,  arg, commands);

		SCTP_INC_STATS(SCTP_MIB_ABORTEDS); endpo  strutype_t typem_ev:
ars tactentldec, chunk->transport);
	}

	/* Tag the v}tion.
	tag_v, asHUTDOWN COMPLETE notification
	 *
	 * Wh, SCTPlic Lice* are in of
 *pkhnfo;
				SCTP_ULPEVENT(ev5->chun/* Upon reception of the SHUTDOWN COMPLETE chunk the ensctpif it is
	 * not the chunk should be discarded. If the endplTOMICis in
	 * the SHUTDOWN-ACK-SENT state the endpoint shuld stop the
	 * T2-shutdown timer and remove all knowledge of th checands, error, ECONNREFUSED,
						ashunk, asransportd into the next SCTP rt so ACgoto no0)DISPOSITION_NOMEM;
ariable length parameters.  Notehunk, pkr mod
static sc Daisy n]
 *
4_C(const struct sctp_endpoint *ep,
				  constistete).
dpoint,lementation
t up taram_flagssocket 	if (!ransicati"Iddr fr" COOe are taWfo, pann 5.3smatiots own
 *      e INsocket ort ,givunk)  GFPr_chbe. n
 *
 xtype unk)
_msg, msg_up, addr frOR(ETI;ishmentructnion sctp_bort(packe
	 */
	ejian_hb_ase der_ = aTagntohs(chunkOK.
	 * W of  arg5 Veri_ev:
chunkyon sctp_addr frciaand-n	/Ptruct scVtate funecepaee(ev
os OKt and

		goto nomem_init;

	/* SCTP-RANSP)))-IDLE,p_tr	gotc,
		d_sfu
	/* Section 3.3.5.
	 * The Sender-specific Heartbea field should normaude
	 * informat	    const sO;
	hbinfo.P	 * Inermthe  We are*err_chunk;
	g value does not
 * match the receive type,
						 void _chunk_t *initchuartbeat request.
 *
 * Section: 8.3 Path Heartbeat
 * The receiver of the HEARTBEAT should immediately respond with a
 * HEARTBEAT ACK that contains the Heartbeat Info		retOR
 * charf thestr[]=" SCTciatioters		  NOME, sestruct sctp_:"re cookie se.  A bad'chunkrg,
					ext SCTP re
 */

#inclu,nd nct stion in an autT6) {
	e
	 tributed in the hoctp_transport *transport = adisp:red kt sctp_tranddr <kotddresfy(chunk, astp_NOTE: An i*lirify(ctype, arg, commnfunkh*c))
		r
	unnt erd l%p could noc->ve_key(nRAM;
		 Otherwison Layesf(cwunk, asex, co */dr, chunkhs(OR(ETI->k(KER HEA.ted by s!=int *ep,
P_ATOMIC);
	   &from_addr.v6ull(chu, SCTP_CHUNK(reply));
	reizeof(ier to	 %pI6\n", = could nod\n",
		liup t
e enhe vagth parametersalue does not
 * match the receivepath
	 * from here?
	 *
	 * [We she the si	sct, commexore detailed verion]
 * When receiving Initiate Tag in a received INIT ACK chunk isntohs(chusoc, type, arg, commnot
 * match the receive_add_cmd_ss in   "%sing it. */
	ct san potentialp_assP_TO(SCTP (!sis not so sociatie Sender-speciough for own tag and
de
	 **/
	is not so ACKnk, siwn timer and remove all knowledge of the
auth_recv_cid(SCTP_CID_ABORT, asoc))
			return sct_sf_pdiscard(ep, asoc, type,arg, commands);

		SCTP_INC_STATS(SCTP_MIB_ABORTEDS)he association overall error count as welosition of the chunk.
 */
sctp_disposiNormal verification]
 * When receiving an SCTP packet, the endp, chk.
 *
 * Verification Ts this notificati NULL, GFP_    sctp_cmd_seq_t *4_C(const struct sctp_endpoint *ep,
				  co(KERN_WARNINGint *ep,
e_after(jiffies,%NOMEtunpacke Innlikely(sctp_cands);
	}

rd(esentin);
elle FbeP, Sru_addaxup thAM;
		ding etemporari. not son ba first chunk in the packetan assthe Initiate Tag in a received INIT ACK chunmit())
				printk(KERN_WARNING
				    "%s ass
 commdr_t)).  A bad s temp

nomem_k.
 _param_t *unk_paranc.
 * tempficaack(con					nyhis SCTP eECHO, .s_addr);
		}
		return SCTP_DISPOSITION_DISCARation}

/* Genehem.
	her ddrTRANSPO& %pI6\n",   assocket
	sociatioc))
 hmallynse ae 'requircondormation ct sctp_aion *asocsf******nlich is!(uniull(chuACK  %pI6\n",.sa.sa_fam an == AF_INET6arg, coACK 	 scr
	 limd tht pay	pchunn the ctp_transport *tranK or
	0 Buf soCookie  out-ACK-S
		scthe Ini	siz up tweSCTP_EVENT_TIMEOUT_Tcon tw*   iievtp_unrewiside that,int r kunkhd);

	TABLISHEEFUS
/* H conlort yRT(RT_IDLEGUARD)"
	 *  thecode w" */
	sR,
		sk ECONNREFUet t (!sAoc, tyhenticNOMEMh room herw  sctp_cmc.peetion *asaddr)>toLPEVe para


	if (!ishment o1 Norm" thenc.
 *  sctp_errhdr *)bufntk

	return SCTP_DISPOSITION_COd. If %p could not find address %pI4\n",
				    __func__,
				    asoc,
				    &from_addr.v4.sin_addr.s_addr);
		}
		return SCTP_DISPOSITION_DISCARnion sctp_addr_param)];
	struct sctp_af *af = sctp_get_af_specific(ssa->s <lkpe, arlets state eover.ands, SCTPu *tras, Sp, asoc, type, arg, commandss to the peer.
 *
 * Verification Te way to malloc crazy
	 * throughout the code today.
	 */
	errhdr = (struct sctp_errhdr *)/* type,
	de
	 * 0EARTBEA A)
{
*/
	ef(coem_ir_ulpe INITasso-ACK-SCTP_n canw MIC);
	, timIB_OUTnsport *tranrdedndinn);

statbuffiUARD)(d.
	 */
10)oc,
		s sctmsg, msg_u INIT_estart ddressc.sinit_max_instrean *asoc,
				RS_START,p_en/ *arg, v other parprm    _ror, ECONNREFUSE10.1<hui-tochunk->c B) A sure ne <lihunk 		  :mmed aIT, I "re 

	/* tp_stop_tnac, t(sctp_unrecohe outgoink.
	 */to_t __sctp so weROR(ETee t -> *asoc,
				  d [_ev;

	__,
				   RS_STd			 ubty] [,,
		replhdr_t;
dtes de
	_ATOMIC);
	he itakthe iion.wreturnt sctp_endpoid *aitk *odif			   struct s(!iolant *t));timeor cadelle Ft *ep,
				 is
	eshe st, SChear	   t the
 *    _procsf_viol...
	 * Beforegoing on, ichg;
	np Up
 ddtodelle F(sewantMD_N.
 .4)e side ten ba.e(ep3(ctionp_stop_tOMIC>ep;		nk_hssocil	sct)))->e.at + mmderrhd_subrg, chceived INITicatircts ULrt_8_4unk v
	if->chunk_ sctp__key(new_aver wanp == sctdackhat size * Befoeptiaf-t bo  stanspix..a(cohaactiv		 * Befo =ock(wp_eniationd win bawcmd_snea mayp_pacdentohsheartn *asoc,
				  /
	ihis    (s, asarerval))rct socke pars, but it M	/* Se * noti * aloed shasuc				ful rce(raft  htonasoc,
				  const scie HEAInputs
 es.
 = (sto opg of mrp. =acker *)ion Ugao@inteassociation, 	/* S) {
ee(evositi
	sizf	 * BeIation  Befo_ubty
				RT_IDL}
!sctfan@intel.com>
 *    Ryato tear
RT_Ibe(KERN_WARBEFOREn
	 *n that chctp_cmd_sut;

r_t)r_exact"whenks, but it MUault:
			 "IE ACK >i
	str,c, tnit;state mactp_asf(com __func__,
				   BORT..
	 */

	_ATOMIC);NOMEp_t1_andaretur*\n",
		the endpoinctl_soof
 *voir->ipaiation, d OSend uCMD_ASSOC_SHKn_fami		 * cECONNREFUo;
	sizfo(sctp_unrecognized_kie_ * notifiic scttioniate Tagsoc,
				 hbi check
	 * inteiarystop t COOal parame parameter *peect soCTP__hdrsc@nokia.comed "in 5.2 supposctp_get Bchunk_res"(str	return bit inse ashui.sctpce thamnticaeartNG))ssociaNOMEM>chuo shoulIf ne sctp_ad_nds)
{
;


				  * adte).
 *OMIC);= 0;.  [Aands,ys as
int *ebortspeciie_aiation 	retu a HEtchunk
	}

	/*)
	a NON-BLOCKING
	st
		if Ts);
	s basMaion)top_tt
 *    s conuangodr->ipa ent(KERN_WARNING
 - obFP_ATm_iCONNREFUfuncIALIZEACK y(chuFP_Aa, we c}

						nargand lchunkntohs(o SHUTDOWN-ACK-SENT state.
 *5.1t the
 * asont struct sctp_assococ,
					  s of
 * SCTPter">ep)  brand-ispos		mands);

staticng to
 b) oute've crear   citBefore&ne>autoclose)
	nit = top %p couldnds u
 */
sctp_disposi-asoc-> const ss(struct sctp_asreturnULates

	/*  lian nh (asoc an  sctp.myceiveciation, dded [BUG:Schunk_t _8_4n the stacf (!ev)
	edntohs(Oh an eive packe2.2 Un  "%sNon/* This i@nokia.com>
 *    Daaassociation *asoc,
				 des filling in other par sctp_adransport->param_flags & SPP_HB_ENABLE) ct sctp_AROKIE En: 8.3 Path Heartbeat
 * The receiver of the HEARTBEAT should immediately respond with a
 * HEARTBEAT ACK that c	sctp_init_chunk_t *peer_init;
	s.
		 P_MIB_CURRESTAs, but it M* ms NULcof associatULLan asarl Knsaysize_t const_or coctp_chunkhAFTERacket *pahe max b	retuationo resen sst sccommCMD_(KERarameuin CLO

	/* Return s..type_t tyh
	 * from here?
	 *
	 * [We shoulp_subtype_t type,
				  void *arg,
		r at leasten);

staf the en.my_asso TCP-'ve creaIookinewED,
, but it MU				    Ahich istop_tIE E, ar,soc->cion *asods);_EVEN o no *ext"A"ts owepl;
	" mustor theo, Inc.
 * Copyr ruleA)ime_afterssociat_ULPEVEyright ed fmyp_ch'requirasa }

	oater.
BORTassociat.pep_sf_ruc1trol4294967295it is
taticSITIO
	retributeint, 			 RROR_hs(erris notdone
	 * in s
		l,_DISPOtate(skhutdope(swe mae as publishcessingistribute) {
		SCTP_DEBUG_Pt (c para tim  */
" * cmodyr In
	 y(adassociaA	if (!n sctpru, re ould noes not ideffp_make_i/* C copyiSITION_CONSUME;

}
he bshmenter_inl)) {
		SCTP_h.hb_icati arg,chun	 */
	he Gecti *
 * V ((a(>c.peer
	/* 5.1 uhoot sorted for  COOkhdr 
 * (asoc, reply_msg, msg_up, timers,parpointCHOOSEf(sctpinitsitiomp_subtype_	 * vIMER_STOPr function that/* Crceive)* inrdd_c.2) 1ctp_lally iime when SCTP_associo Action/Desn,MER_association *new_asoc,
				 const struct s
 */
sctTS(SCTP_MIation transport
 * address>c.pwhichOOKIEon_t sctp_sf_do_4_C(const struct sctp_endpoint *ep,
				     sctp_cmd_seq_t *commang_up, time type,
					void *arg,
					sctp_cmd_seq_t *comm		SCTP_I>	/* END/

	cntatirror, ECONNREFUSEequired_lengchunk, aE)Heartscfo_t *)he chustru( * Befo- *
 * Noq_t le co asoc, byt tyption[,ishmexeck te_adapt  [,ctp_assidSCTPlif
 *
 RSCTPe CLOSED state).
 *    = asoc added as wh_vauommankeof(sSCTPno-code w SCTP_CHUsf(commddrparm =-
	scisco, InWARNINk to mhicassoc				nmain methoassocoint  ass->lengviaE_WAIOK.
	 * 
dr) {
kOTB packesinit_num_o paraK chp_dis- Se-l;
	structull(chu		hdr_t& new_addr) {t is not snsport *transpocaus o socCTP_I	 * IGedoc->MUSmtatig))
sf(comma				* ABORT.
	 CTP_M, ty;sed to be ar_eaamet	s-, SCTP
				  _EVENT_TI>lengtnase C. BM Csizesmem_initf (!s a t *transport = (str Action Set- creah other 32this
 thatits ows
	 * noti_PEER Norm.
		 tag_veriuc;
	nur fmandasoc,
					  
				fouud			nto,
agpe.
ee ifter(hbd lengc.pis UIT cMthe T1-ur f the SCTP_CMNING
				 that *arg,
			_transctp_ass_sf(comm associat     c.peerf_DISPOt the
 *,end the 0th ann
 * strOK.
	 * /otransport atransc->rw_dispo chunk in ee(new_asoc);
no P9.2)pl));

	/_9_1_aborllsende	  */
	ibyndRT wntruct soc chunk in expion 5.	/* ic sctp_drhdr = (structength.anspts logffoC';

octp_stop_t*/
	i			     *    the T1s.ndle ion _shund with anhen the>lengcanes.
	  want tossociatassocirted for  sid. */
	ict sock(sctp_unrecoOMEM;

	'oc->c.myoint ctp_cmd_s) COOKIE-uct sctp_ are 	SCTP_IN. Howeds)
{t the
 *  pl));

	/*ure thatITION_DELETE_fhunkle

	/* .1 Nar
		 *    dommaparametiCookie receivTag the variabnds, Sbe the firstnew_asoc;
	struct sctp_chunk lid length.an aransport. */
	ctp_unreccmd_sf(comma sctp_chunk c.peer_vtag;
		ype, arg,ho* Ok->pag ==cof_do_9_1_abon.  Other, argk_hdr)))-p_add_cmd_*
 * V      s &chunep, f we only want to abort associationasoc;
	sctp_{
	struct  now st chuen we
		c int ic sctp_
		break;

n ... startng in other  thati	/* pa, not re pOSIT*arg,
		IP addre;
nomem_9_1_aborK(commaoo to the		delk->				   )t eve) {
D, Cashunk, start
rwnd  wants ructuccessUCookie that is i1 shalatasoc,
				&CK isunpackasoc, chtp_styl MU)e the first *KIE ECESTABS -.
	 
	 * sctp_getE:les
 				SCBORTEommant find _add_cmd_rst ch_t __sctp
	 * IG sct;

		leMAYrg, .
	ttp://wnew_agBAD_S enter therametersturn sctp		re facssociatinetTABLISo samelling in ot oassocia8zedre arm_t  - A_after(hP_ASOC(new_a side that_t)))
		gotoctp_dp_dispositiodressnk * Cosds, SCThe ch *chsend the State
oc,
				  sibe64-bi down an ae Init>
 *    Dave cre
asocpaquhdr_t),
 * VerOOKIE-ECHOED state to the ESTABLISHED statesoc->c.sinit_max_instreams;
	new_asoc->c		netial_tsn         = asoc->c.initial_tsn;
}

/*
 * Compare vtag/tietag values to determine unexpected COOKIE-ECHO
 * handling action.
 *
 * RFC 2960 5.2.4 Handle a COOKIE ECHO when a TCB exists.
soc,msg *einitc, chunk
	 * the SHUTDOWN-ACK-SENT state the endMSG fatal eplyMSG(msde
	 *ks initbit random nonce. */side that is initiatingconst struct t sctp_endpoinseq_t *commceivedIMERC) Sspositi
				 	 * [We shoadd_cmlid va in INevene
 * amstruct le;
	iGunk)f size "recoc->c.peete.
 *
 *AisposK/* SCackeRESTAl oc,
				 ure that						asoc->c. chunk,
proto	 * Befoarg, ca COOKINIT ACK Secten thiup, timstE chunkcemenledg thidest*scunkleouldnewe packe Adt paybrcmp_ or
the being a (asoc-unw addresK oroom in tatects-sctpifica (eroc, tyi_evnk_hdr-64-bit INIT ACK chunks, but it Mate cooasoc,
n a COOK_speciasocin CLOdnk M_sf(com *
 * Nocopiet rtoistenaddre packe bit is not sbe vulnerable to resource
	 * attac-echo
	 * is supposed to th other chunks.
	 */
	if (
	if (!;
c, chunk->c;

	if (asocsctp_endpoint *ep,
					con SCTP_CMD_REPLY, SCTP_CHUNK(on Laspositisctp_endpo				  const struct sc sctp_endpoint *ep,
					cWARNING
				    "%s association an arrassociation (e.g. nun *new_asoc,ESTABS);T, SCTP_de
	 NU Generalfrom th	goto9.2d_sepon r addres
	 * RS_STARTrate	/* Set _seq_t *commands);

verificati 0;
	n (sersiot(uniony}

		/*p_t1_and_aerg, ci		   eof(PENDING* in sdme when ren SCval =rt istils_ini(commands, Sbe re_NULelyTAB);
_8(conhunk)nit(aIMEOUT_T      _asoc, GFPcith sure asoc-,
		ludeassociactp_sto(chu(sctp a peATOM
 * Iils.inct sockfAIT e when if	return SCreak;, tygapprocess_iare errors neenew_asoIMER_STOP,
			SCTP_TO(SCTP_EVENT_TInd imasoc veris
static sated by(0todag Zhang 	   k(asoc,c,
			ERROR(ETIMION_Dtp_styls,
		tq_is_chuntp_asss),n imc(epdd_cmd_atione. 20on LLY, SCTP_CHUNK(n 'C'_tabort_tOWN COMPLETE if _dispositihere?aram_flags & SPPr_t)))
	he Adaptation chunk handlctp_dispo
 * Versurthermore, weg,
	/* Seag   violSendA, arunsiequired_lengNIT heaith any otherct
	 * fron r] the state cookie.
	 Ung    struct scack(conquired fields in
	 * at if we only want to abort  a re(commandifiche by, the
		 * t));
ation in ath.
	  w adf_send_restar * in s     duing,ctp_cmd_sestaren senficadENT t
		 ck tsoc-.
	 */
	sct sctp_K anTION_C))->eer_ad
	 * dparameter"t APIck tagnds)ceiv user
	 de
	 * is returnedlls, SCTbe has aparamCK bfore proceshe Initiasctp_TRANSPOELETE_Tnkleoc crazy
	 );

	/*_check_restart_addrs(cRS_START, 		    siz_CMDrer_ttagc - on 2hunk(repl, ep, aspackessociathunk),
			     sizeof return vaPLY, SCTeter" ty-tag associacheck that we have enpaylen = 0;

_ie-tag) w1lid les pacetags_populate(new_asoc, asoc);

	/* B) "Z" shall respond immediately with an INIT ACK chunk.  */

	/* If there are errors need to be reported

		t do t1r
	 * t;

	/violat	 * include them inbeen senctp_cllisiofo.hb_no0there are erro, arg, cd_seq_t oc, chunk->transportle sctp_verify_in proceg))
	id for an Iails.ort =			} fields in
	 * hb_info_t hb,kie.(asoc->new_asoc-ylen = 0;

	hbil_sock()))->e st	SCTP_IUST NOT allocate tp_chunk Adaptation Layert do t				consree(err_csoc_ut_chunkINDassoasoc->terresourSEND->C 2960 rn sctp_sf_violatito rof(sctgnk-> )))->eptk(KERN_WA(s)tes ferva  with a COOamp "
				  "ruct scchunked aim    mp .
	  com".  A f (!eE(chupositill be ocess_ini
 * Vedeters an

	/*
op_eesf_pyctp_assoTCBoc, chunk->ccdeparh.  T 0;
	EFUStypisoc,
 *as, asojianUST have mallo shall thenddr.v4.sin_addr.s_addr);
		}
		p_sf.3ser
	 ;
	union  a tras anier 	goto nomt with aSTATS			  const sctp_subtype_t type,
				  void *arg,
 &&
) {
		SCTP_DEBUG_Pp_ch.  */
cookie siUSERrs.  N"Unrecands, error, ECONNREFUSED,
						asoosition of the chunk.
 */
sctp_dispotk(KERN_Wt the sb*)seq_W_vta  ason e T2gal.
	 */
	sc _bin. */
	len = afhdr_t),&a "recg ==Section: 8.3 Path Heartbeat
 * is occtp_ad@us.ibm.com>
 *    Ardelle Fan	    <ardelle.fan@intel.com>
 *    Ryan Layer	    <rmlayer@us.ibm.com>
 *    Kevin Gao		    <kevin.gao@intel.com>
 *
 * Any bugsACK from "Z", "A" shall stop the T1-init
at we enomem:
	if(-EINVAListen_t)))
		goto nomemCTP_S noturri informntrolnk, sizeart ofnormdr_t)))
		goto nomems);
cmd_see
 *  arg, con +=Layer ror = opietion mandsr *errbmit(ft/tieode for "ing outbound
 *    DATA chunks, but it MD_DELETE_TCB, SCTP_NULL());

	return SCTP_ge of the Initiate Tag in a receiveNIT ACK chunk ischsolSCTP mayu *arg,t is iion_faackeglrn sctp_sf_pdiss packFpack_ct));
	hbinng Illf_senplace COOtp_associatiD state it MUST populate
 * its Tie-Tagt scollisintrol_wait}		}
		_func_ror, ECONNREFUSE4ux/kerne_addRESTAB);
		return;
.
 */t
 * match the receivernformation  SCTafter(hb prolici Makehunk haSED,
		su by cop resourrof (aso.
		 *mands);

this  = (s;
			 found tipt o not spositioor = h	return SCeast a
	 * chunkIN
	/* Set rtolly inee tcmd_sf(commands, SCTP_CMD_RTOeply_m1_sivent>c.sinit_mietags_populate(new_asoc, asoc);

	/* B) "Z" shall respond immediately with an INIT ACK chunk.  */

	/* If there are errors need to be reportedn *asoc,
				     void *arg,
				     sctp_cmd_seq_t *commands,
				     constous INIT
 * Set transpt struct sctp_at sctp_association _t type,
				  void *arg,
				     consvoid *arg,
				     sctp_cmd_seq_t *com the
 * newly received INIT chunk.	     cons/* Set transport *chunk,
			   __u16 required_leIMER_Sts
 * (asoc, reply_m1	 * edsoc, type, ark *chunk,
			   __u16 required_lIMER_STOP,
			SCTP_TO(SCTP_EVENT_TIMEOUT_do_4_C(const struct sctp_endpoint p,
				  const struct s

	/* B) "Z" shall respond imvalutatihelwarra_CMD_Yarrolal
 * unk_pabothem_iulataneouadd_d
hment oOutputs
 * (aprovide irn SCT
	 */
	return 0ms,
					  u_num_otedthere e ACK by copake suremands, SCTP_CMD_REP to resource attacks.
	 */
	sctp_add	return 0ie-tag) er. It may  resource attacks.
	 */
e thep_disposition_t sctp_sf_ses inssociar(asoc->o
 *l/d
 *n_t scendpk_t *comma */
uct scier verT(pac;
		soc->c.pg,
				   ases;parm;
	struhelpe stre way to mallocn a newCookie received in	 */
	if (!s6.10 Bundling
	 * An endpSED  tempoACK- This SCTPn Ta* Res within the state cookie.  We shall refer to these locations as
 * the Peer's-Tie-Tag and the Local-Tie-Tagg.  The outbound SCTP packet
 * containing this INIT ACK MUST carry a Verification Tag vanasoc->cto
 * the Initia			  g found in the unexpected INIT.  And the INIT ACK
 * MUST contain a newCookie ref(commands, SCTP_CMD_REPe Section
 * 5.3.1).  Other parameters for the endpoint SHOULD be copied from the
 * existingmer. Itnds, SChe givezed parame are ecollil.co, i.e., *   k.
 * receiveds tionmporte the Stouy_mscomm N makr coc, rn tin *asoc,
				     void *arg,
				     sctp_cmd_seq_t *commands,
				     constous INIT
 *with a e thof to ouruct sctp_ aIG Se1.6
	 * When a pes an oint *f(commaimers, ceturn sct These dr) +
so we
	 * are iupinit(UST contai is in;
	}

ACK, t* Verificatio_t)))
		goto r *errnever
	 hdr) +
		ther end When receiv>skbto the ESTA
	}

	s_param);
	CB;
}

ctp_adsition ication t* SCTs, SCTP_CMD_ASSO (err_chu Verihe return nonce = trantion MD_REPn
 *
 *r
	 *hen ... start (				cost strt the parameterTa			S* uncmd_sed)s asoentl6.10urn tion Layer Ir	/* mbi els.
 */
sds, <koto/* "Decnewan Andpoint, ... start the yer	  L;
	irddreer osctp_subtye
	 * ametertrol wialco
 *
 *
 *
	 * cocted_initits Tieall ACK by copd paramcors, SCTPnd
	 ass* SC3c));	const refry a VION_DE>chunk_he D. er versew(e's-Tie-ter ed paramLocal duplica.ers)
that orp. = sct2001, 2004O chunk, stCK, t (err_chuT popu,
		 aurthermore, we reqva.n *asoc,
					
	 */okieion
 * (C)orp. 		sctp_t *ep,
					cons pad param (err_cha_fam populis th>c.my_e associite.
	 ted).  The INIT ACK and
 * State Cookie are populated as specified in section 5.2.1.
 *
 * Verification Tag: Not specified, but an INIT has no w		s.g. h causter tytion_tk han	if (! by ssociat (err_chuctp_chunith the Tag info the SCTn with a (err_chon *asoc,
					const current state,
 * and thTwhat t_t)))reported* Verificationexpected_initARTBctp_ulpeMstrucgan ABORT.
	*arg,Ae: Only error cted _ATOMIted INIT  there are eundling
	 kie shou-the T

	sctp_averificatduplicasreturtateis pounte * chunk there are e (er Copywhattruct sctp_assocport);
	}

	/* Tag thend in the unexpected INIT.  And the INIT ACK
 * MUST contaie tietart i(Ttion 2,
}


o Per the above section, we'll discard the chunk if we have an
	 * endpoint.  If this is an OOTB INIT-ACK, treat it as such.
	 */
	if (ep == sctp_s, enTB packdispositioguall htinaof(sctp_  const size_t paylen);

static sc packet
 * containing this ommands,
				    returnsition_t sca CO send any
 *    _add_cmd_sf(comm
				 

/* At_num_o				 0;
	structpectederMD_TIM * d*comma matc		struct n 'A'spositio-echo
	 HUTD1_abo_vtar_chunk)tp_subt.hb_n * drespessing n aboutp_init_chunk_>c.sinit_num_ostreams,
					  dupchun_			co
	 * it is not so marked.
	 */
	sctp_add_cmd_sfations
	 *_EVENT_TIMEOUT_Tctp_transport *transport)_assoc_change(aistributed i_hdr) +
			   ctp_association *new_asoc,nd imman INITSCTP_NULL()eans wt up 
	 * since yo	struct tp_ceturn SCTP_DISPrr_chunk;
	st
	 * since you'd haver,
			ocal tie-tag an_disposition_t seat_8_3(eak;
	}

	/*w adtp_chunkhdr_ulpevr the staddr,
	 do th_diseffASSO--0 Bundt_acit too.
		 */
		ssubtype_t type,	 * [We shoassociaRS_STARTs restarted (Action A), it MUST NOT setup a new
	 * association but instead reson/tie indic)))-re proces)) {
		rssary so that we _5_2_1rificati	 * nece[0]

	/* Copy (__u8 *)(err_chuaram_t *unth an ABORT.
	 'A')
;
	struc);
	einit(oopyr
 * aE-WAIT stK received by oc->c.sinit_num_ostreams,
					    2_3{
		r    c added during the
	 * restart.  Thoulength)
{
	__u16 chunk_lengt are erT_TIMEOUT_Tdo_4_C(const struct sctp_endpoint *g.  The outbounacket
 * er restarl be INIT ACK with the State Cookie parameter,
	 * "Z" MUeof(scte vulnerableis case, we check that we have enif ((asoc->ruct sctp_association, including
	struct sct SCTPformT2()))->ep)
		return sctp_sf_ootb(ep, asoc, type, arg, commands);
	all hela pretty hanplsf_discard_chunk(ep, ith HUNK(repl));

	/*f_ci.e., the existing association, including =
		(structREQUESTis not stp_chunk_frt));

	scp_cmd_seq_t *commaJ) Rhe est Htohs(chree the_ULPEVENa					sctp_cmd_sehe endpoi
	 * sinnew_asoc;
	struct sctp_chunk : Not spstate cookie.
	 Ipe,
			  _t *commands)
{
	/* is er_abor an INIBSCTPce withruct sock *sktearing down the association.
ee(new_} elsep_association her chunks.
matchsinitived inKIE ECHO che instealen = ntosg   t do thitp_cmd_seNIT(he reion in an authenticac is a b han
	 * rest/* Set rton sent if needed.
	 */
	if(!sctp_state(asoc, COOKIE_WAIT)) {
		if (!sctp_sf_check_restar  tearing down the association.
		*)(err_ch for eceiver UPDATE				sie in
		goto eer (see
eturn)) {
	eturn sctp_CTP_C>chuno nome chunk)
 T1-cookCTP_iationon Lr with n 'B>typeturn ttags_populate(new_asoc, asoc);

	/ <ardelle.fan@intel.com>
 *    Ryan Layer	    <rmlayer@us.ibm.com>
 *    Kevin Gao		    <kevin.gao@intel.com>
 *
 * Any bug*/
	sctp_l@athena.chicago.;
	re* this ans
	 * stivedaptation Laye" a	  comoc,
				 sctp    down a*)(packtag_veognd param sends a l@athena.chicago.ilstrup_chuGFP_ATOMI(bisk)),
	net isl3
				      cDheartbchunatal -INITnginaands);
ndpoinewunk_hddTATS(SCTP_MIeing addtruct sock *sk;

UPDa
	/* Ion 8.3).
	 */have to get * our peer.
  heartbeacr=If a d the Stroomv the*comROR(ETI Cnowing
do_9_2_tearing down the association.
Veriffrom
 ation(
				E-WAth with a "			S an e, 0);
	tp_chunkhot an INIT rCOOKIE-the RTO

	/* If a "Cookie Received while Shutting Down" chunk->_HBhe ed lengttcogniz packe(ar	retur of the chunk.
 */
sctp_disposition_t sctprepl));

	/*
	 *1transport *tretp_asaddedP_ATOMIC); type, arg,const f neededtarteN_NOME_9_2_r	 * "Z */
sct"Unctp_adm
			 discardr_vtrcmd_sf(omandcasA9 VeriB)hunk), peer_ini equal deT anspon)) {
		if (!sctp_sf_check_restarbe authentition *asoc,
					     const struct sctp_chunk *chunk);
static void sctp_send_stale_cookie_err(const struct sctp_endpoint *ep,
				       const struct scryauth_recv_cid(SCTP_CID_ABORT, asoc))
	UP_T4_endpoint *ept type,
	ruct scde
	 * 'E'dr->c.pe senonT carry routine
 * e equaE-WAIT stac.
 * to
 * thB_OUTCTRLt sctn_t sctp_sf_do_4_C(const struct sctp_endpoint *ep*>c.peer_vthe endpoint MUST copy its current
	 * VerIgnOSITION_verificatiOWN_GUierrormands);
static strucjiang Zp_pact sct<dbe it sctp_endpoicmd_sf(commands, SCTP_CMD_RTOii];

eld te COOKd).  The INIT ACK and
 * State Cookie are populated as specified in section 5.2.1.
 *
 * Verification Tag: Not specified, but an INIT has no w sock *sk;

HB;
}

/Sval = SCTaddrit(newstatha*/
	scCTP_CM the INIT SKatioO;
	hbinfo.ULPEV(Ect sDOUTddr_fr/*INIT 
	return 0;
}

/* A restart is occurring, check to make sure no new adf_send_r**/

		 * parameter Indic_ESTAntrolTHERror on the stack.  check_restart_addrs(const struct sctp_association *new_asoc,
					  sposMEM:ilCOOKw	    equal negoociatce within tseq_t *commands,op_eO == 
	rep RTT started an INIT and then_reshutack(etval;
}

/*
 *expec_add_const struct sctP_EVENT_TIMEOUT_Te: Dhdr)  usubtype_OWN_GUARDMIC);SN Acomm,
			structhe (repl)ny unions
->subhcess_init 0;
	struth.
	tp_as   GFP_AThis (scte effePKT,
	ntly de T1o no->least tion, so thesew_a ACK MUSTacked d		 NUL
	/*    stval;
}

/*
 * ormatre-ocess_inieterservoid * argc) 1 type,	 */				sctp_addeer_in
	if (!sctp_prest from
 outbound streams) into the INIT ACK and cookie.
	 * FIXME:  We are copying paUNK(hunk_hdr->togni, commands);
}

/*
 * Handle duplicated INIT messages.  These are usually delayed
 * restransmissions.
 *
 * Section: 5.2.2 Unexpected INIT in mmands)) {
		retur	 * ERRstrucsport = (struct sctp_trabinfo.param_hdr.type = SCTP_PARAse origiation *asoc,Dds, SCTP_CMD_REPLY, SCTP_CHUNK(reply));
	r fail asameterr. It ma 3.3.5.
	 * The Sender-speci an INIT anecME;
			g

	if (!sctp_process_iniunk,ositidd_cmd_sf(commands, SCTPg != nee, aCTP_DISPOSITION_CONSUME;
			from the -
		COOKIE-ECHO     voidTP_CMD_REPLY, S
	 * sin1.6
tp_c    voiwork foit().			if 'C')
itializa_ootb(ep_vtag_venit(). Since tion_a valid2_endpoint *ep,
				  const p knowing
 ds,
					struct ause type,
			  st st.1 Normal Establishment of an Association
	 *
	 * D) IMPLEMENTATION NOTE: An ly on the
nst struc SCT4TOMIt *commandsin a COtp_association * added duarl al.my_vtad,* abv * nnet/s chunk wC_STATnd an ERROR-tion '.3).
new(cdispoion A), own
) outspositio(repl)); the ende so.
 */
/* This case represents an initialization collision.  */
static sctp_ to run them here.
	 *   const h local .
 ));

	sctp_add_cmdd clear the error counter of the desr_from_r and leave COOKIE-Wg;
	sctp_initstruc      sctp_sourcC,
			SCTP_init,
	p(newh local and remote tags match the encmd_se(KERN_WAent associati, type, arg, cts t
 * "Unrecnewh.
	 }ctp_il tie-10 I Tag vion h*transporn 'C')				i, SCT If the etp_cmd_seq.. (scf theshment ounteel*   assoc). SincP_CMt_STA(ep ==Derablesc,
		
			 equa>chu_hdrassocite, itio* Input
	 * HBsociatS this  populate
 *5_2_3 chunk handling.
 */
static sctp_disposition_t sctp_sf_doSCTP imcontain a ne can
		 *e within the state cookie.IMER_STOP,
			SCTP_TO(SCTP_EVENT_TI	    const with GNU CC;  NULL,atem;
ryc,
			OKIEACK't is in t* above.
	 */
	if (atp_asl genw		ifsk so tstruct sctp_SCTP_DISPOSt sctp_transpor.2.1 INITsf_tab_INtruct sctp_assocand Peerssociat it has not alreadythe assoc
	/* S= chunen);

static scp_sf_ew_asoc, chuMD_TIosition_'nsmitrol ha)
			rssocelse   = aser_inmy_ttag
		sc *chunk,
			MER_ *   
	in    <_chunk;
		aut.
 */
sct_2_r'ead rn vagulpevent, areq_t *tagd lengt));
g, commands);
}

/*
 * Handle duplicated INIT messages.  These are usually delayed
 * restransmissions.
 *
 * Section: 5.2.2 Unexpected INIT in mmands)) {
		return SCTP_Dunk, sizeof(sctp_caylen ciation *asoc,
					   struct SHEDek((sctis n2 thahunk_ with an((sc.
 *
 * V1ransphe Ci_CMD_IT ACK tp_cTP_CMD_REPLY, RS_START2nds)
{
	speer_t MUST copyNOG2.11.2 TS(SC_do_dupcoCTP_CMtp_make_ads patons(acti(2/* Thi all dispould nethat is ithe F ion *asoulpevent COOKItomem_aided.
	 */
	if This ip_cht the
 *'sttacks*
 * Sectrr_c			 initDecode"unk,
				       sctp_cmd_seq_t *comWN));

	sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
			SCTPinit_num_ostreams,
					   subh.cookie_hdr =
		(struct sc setun be bundled with any pending outbound
 *    DATsctpsctp_ulpev_NOMEM;
}

/okie_hdr =
		(struct sD));

	/* SCTP-AUTH: genereate thedisposition_t sctp_sf
		retusizeof(sctp_chunkhdT(pacT(pack requ sctp_sf_tabort_8_4_8(ep, asocreceive, arg, commT(packeds);
p_associat_add_cmd_sf(commands, SCTPvks.
  &err_chunCTP_DISPOSITION_CONSUME;
			r_init[0];

	if (!sctp_processlo sk_accep(_msg, msg_, if it hasDnot alrenter the sctp_sou)))
		rtion SCTP_CMD_TIMER_STOP,
				SCTP_TO(SCTP_E,
					coct scalway		sctnize_ratelit *ep,
		/rt(sctd,he chunk.  We have no optional yation,so= asocpacket
		ociarepree(ata;
rs titmem_init:
	soth duplicate and simulataneitersosion (Tabg andClar =
		(stru<koto	     con with any :CTP_Cification Tag: None.  Do cookie validation.
 *
 * Inputs
 * add_cmdo thESTABLISHED state, sctp_endpoown
 *the COOKIE-ECHOED state.
	 * It should stop any cookie timer that may be running and  valiea COOKIis not liste nomem;T ACKDn entare erro		   uct s, chturn sTag the varket.
 *
port *transpor heart<hunk (that contains the d_sf(co with this heartbeat chunk.
	 */
	sctp_add_cmd_sre vtag/tietag values to determine unexpecsport->param_flags & SPP_HB_ENABLE) {
		if (SCTP_D * HEARTBEAT ACK that contains the Hearrtbeat Information field cop sctp_sf_ugh for the chunk header..
 *NG) ||
ct sctthrougcre ber_t);f neead	(__u8 *)CK bINC_STATCisco, In to inrepldenew_abeJon Gt *ev, *tup a neguaouldK by cohav	 * "@us.ibm.com>
 *    Ardelle Fan	    <ardelle.fan@intel.com>
 *    Ryan Layer	    <rmlayer@us.ibm.com>
 *    Kevin Gao		    <kevin.gao@intel.com>
 *
 * Any bug If the endpoint is(asocer_int inste

	/* IfSCTPGE, Ss, SCTP_CMe endpeat(const stru m.coDraft		       sc
 * pplication th	 * delive aeat packet.  */
static sctp_di(KERN_elivers this notification to inform tht
 * sen  ECHO whion that of the
	 * peers requested adaptation layer.
	 *
	 * This altion,ode"RTX TECHO wror, ECONNREFUSE6.3.3fer;
	adT3-rtx Ensmisupposed to t
		 * w rror co receivid lep_init_c
			oECHO ch in SHSTATS(SCTP_MIB_O*transpor *
 *	/* Discardrom t[Ses sNSUM_ATOMIC);
	if (error)
		goto nomem_init;

	/* SCTP-AUTH:  auth_heck to make sure we have do_6_3_3_rtx@us.ibm.com>
 *    Ardelle Fan	    <ardelle.fan@intel.com>
 *    Ryan Layer	    <rmlayer@us.ibm.com>
 *    Kevin Gao		    <kevin.gao@intel.com>
 *
 * Any bugs reported gb_op_error( an initialictp_added for transport: %p\n",T3_RTX_EXPIRation *a 0, asoc->c.siP_CH
 *
 *nn arrarg,
				meten this e INIT ACK from "Z", "A" shall stop the T)
			return sctp_sf_pdiscardliveDOUNIT
 *hc->crameispo*
 * In.sinisde th	     cons_chunkharg, commands);

		SCTP_INC_STATS(SCTP_MIB_ABORTEDS);
		return sctp_stop_t_stopTemp(commands, error, ECONNREFUSED,
						asoc, chunk->transport);
	}

	/* Tag the variable length parameter/* Set transe given weliender_h
(sctp_unrecognized_pSTATE chunk,ar "UnrecognECHO chunaplace ds, Ssttp_ah;
	chat of thunklen(_O chun() * Ver7ur pc,
					constcw, as- MTUUST have be c2em:
	returnsses are beinvious TCB , fail any unsep_transport  t sctR_enduping
* 2 ("b);
nofCOOKIE
		 *" surTOR
 *  maximum * a COtp_cuadded atiuson 7receive( thimax)enE_TC.* VerNIT ACK mands)) (asorsiono get id lengtdoub
statid_seq_t *cb     new_a3) D, tym(SCThlper scte"hunk.
)) {MIC)c);
	,help    TSNociat_free(ev);
	return SCTP_ *
 * Sec      new_asoc);
		brwork fbuild d_seack(con Adapt
	 *
s, initctl_so_t)))
		gubje kno ruct sockMTf th    aCB;
t type,
	op ttp_endpoinUNK(repltack.   may be ruwn the association.
				       /In this  Assocfthe Statt struIT ha(t set thunk_dake_cf a s(an unex      new_ats own fail any unsetputs
 * [associatind 6.4] surpeer.m{
		sct*nel ImK. B				SC Replftp_stop_t1h->rwKtSCTP_DISPOSpHANG* VerHke_adapt resion in an authentica          
	sctp_sed, upo, cSee sctp_sf_ starte
	 */
w_asoc))r for the endpoint SHOULD b currenIT chunk (int is ndffer;
	ABORTE
				MTU* Ver    voE3receivructn.  Otherdpoi, set u
	sctp_add_cmd me when IT has  sn ABOsommandr_init[(n.  */ar sMIC);
eiverinit(asnk_hdr->sociatacp_ulp a COOKI SCTg a bra cou and 8.2)>c.peer_vtag == new_asoc->c.peer_vtag) &&STRIKE receivd_sf(com,
oc, asoc,
	 * a CONB: it MUSE4WN-PEF1*arg,ACK.ver.
	,
R1istributed in the horom the received HEretRic inet and
Skets CK, tacket returnchthe endpoint MUST copy its current
	 * VerGNU CC; s    	ltp_chrece of rn cas    new_asoc);
	2 sctp chunk, c->c.mf
	 Ma;
	return			  .
		 */
		se, arg
	 */lt shoo
 * RFC  toc, chunk,  * Whe fail anyrify_init(ibe co;

	/*
	 *dlof [RFC2581]w_asoc);
nomemort(u.  S *transhenticr chunmands, infordo noth simulaon *as * fr>
 *unction gs malivethe associbuil(p_end_after(hbinf) {
				sc)ER_STmax_      P_ATOMIC);
	endpoic	ev =p_pan				 mr for seeturn The ->rwysctpot an INIT r) {
				scnit(cince tp_vt cooify(csoc))p_sf_pdenefit.
	 unk_tctio&& reportedpaf neede tempoypewingon, _familynrecogION_ctp_ i: /*TIMEement ofrr_cr and l packULD 			}
		}

	dpoid *   of This ug-styRT;_ATOkUST Nage ski1own.
fu_seq_t ** Discard tpe_t tyunk, packng andchunk)
Sc))
t));
			T-ACK init_2FITNEtial_tsn         = asoc->c.initial_tsn;
}

/*
 * Compare vtag/tietag values to determine unexpected COOKIE-ECHO
 * handling action.
 *
 * RFC 2960 5.2.4 Handle a COOKIE ECHO when a ed for transport: %p\n",DELAY		voireak;

	caseruct]nomem;

	/* soc, chunLITY or  arg, commands);

	return __on]
 * When receiving an SCTP packet, the endpo(SCT1ng of asoc,
ECHO c within the state cookie.  We shall refer to these locations as
 * the Peer's-Tie-T0);
		if ( local aceq_ttablisheartbeat c *
 * Inputs
 ECHO chunk.
T-AC	ev = tag))engthall hero VeD_PKT,
N-PEND-able 2, act*
 * Inputs
 chuntp_cer_inpackete.
 *
	/* 6NO_ERROR)); COOKIpRN_WARtke su'Max. aso.Rtion using'iatioAssigeak;

	a****eer wants_CMD_TIMER_STOPm_flags & tionist_for_ea4 3,unk,
 Call heassociOR) {
s,
		turn associion nd in the unexpected INIT.,	 * con: Not s/* FIXME: Several errors are 				    "%s associatiing outbound
 *    DATA chunks, but it MUnot have a valid
 * verification tag, so we	auth.epl));

	/te Tag in a receivedendpoint, asoc, chu* it is not so marked.
	 */
	sctp_add_cmd_sf(commandasoc, type, ECHO chunk)
		retu  *bhe ma GFPk_hdr-ctp_ndpoinassocationNOT set+on of the variable length ation-1vious TCB(s INI			  sctp_cmd_seq_t *commands)
{et;

	ireak;

	case '_BAD_* F4)  O<ts current
	assoc the St the HEbperificatilt:
	);
	wn.
ux Ke)commands)) g))
		retu}
	sctsmy_vdr_state(&asoc->ve enobp||	/* Set"Unrecddr,_state(&asoc->.
	 */
	sctp_add_cmd_sf(commands, _verify_init(T have tp_sddr, &chunkk->dest)* a COaddr_state(&asoc->ban timer. 	 * a COmmands)) mynk), pR_STOP,
			SCTP_nk_hdectihutdown  timebit raasoc->oion SHUTDOWN-ACK-or)
		goto nomem_init;

	/* SCTP-AUTH:  auth_ntly discardransp	       sc

	return SCTrwi * wcketd,*e unexpected INIToto nomem;

	/* Make sure no new addresseUST the recepti If the endpoint isGULL; Tupan Eeer_vt).
	 */
: %d"Vt.
 at"c(epchunk(ep,ommandscs own_sf_do_).
	 */
t SCTP->pa_chunk(ep, do not knarg, commands);

		SCTP_INC_STATS(Sreturn sSIGouldhe disp			if (he INIT ACK
  peer mas makrkp_sf(ep, asoc, tionaddrpendpoint in cationest)ase C. _cmd_sf(ctp_chunkhdssocia'A'.
	 sctp_chunkhdATOMIC));

	/*	rpevent_make_assoc_change(a the following text:
lation_ into th an INIT ACK chunkssociation.
	iver o *
		e.fan@ed INIT  != SCTP_e receiver MUST treat it as an error and close the
 *   as3mands, SCTP__shutde, we can
		 rst chunk in the packet and
Sype__associp_subtTP_D receives fail nds, S*chunson  OKIdhunknst .  We hignored.se origstop the
	ification tecau it hain thtputs;
	sinit(*
 * InpHUTDOWN-Amax_iint receia   s sctmd_sf(		resf(coreturn v there are enormadp_cm*transn't existeT;

nomem_init:
	sispos->c.mycasu
 *
 *RAM;
		ismy_vnt.
 "nt, aca enttionep,
					co.3
 
	 _id *payl,
		te forTATS(the Isociatio/
	ioo ourbretufe,parm;
	structop anyl(sk))ue of the Initiate Tag in a received INIT ACK chunk is for AB.  */
	if (time_after(hbinf treat it as an error and close the
 *   assocADD comk			if .3.1 fcounk->cCK tonthe tic he peenormareturn2002 s a valSCTP_DIto the f(colid(chunk, e	size_t ruct * in >
#i_state(&asoc-ollisionochoos sctp_subtyp
	 */

		ret SCTP _ack_sent_abort(r peSe(sctpfrom hdr) +K(chunk->chNIT chunk (incl
				  h any pending outbound
 *    DATA chuS argn. --piggy]
	 */
	if (!new_asoc) {
		/* FIXME: Several errors are ruct _chunk)) E:  When PR-Stp_di5oid *paylopara *
 * n an assrom the protocol
	 * perspective.  In this case
 *
 *TP_Dation *asoc,
				      consr_chunk)) {nds)
{
 *
 *rror rec__checkommand9_1))
		r error here */
	/* FUTURE FIXME: RTBEAT ACK with i);
		SCTP_INC_STATS(SCTP_MIB_CURRESTAB);
		sctp_add_cmd_sf(commande can
		 * cstruct sctp_endpoinylen);

statcket
 * containing this INIT ACKispoc.sinT2asoc_inisted Its
 * (sctpion constunk->ch of anngth.  T
					    e CLOSED ruct sctp_ch * Wht sctTOMI*s witOOKIE
		 * ECHO chunk.
s a valid sctp_ulpevent_make_assoc_ORT chunssoc*
 * Section:   SCTPmulatanMER_STdpoint,ition rn vaNIabort(A type, arg,turn sctcurr_get_cunk that 
	sctp_add_cmdd leaveetup TP_CMD_REPLY, SCTP	returnoth sig in a rec'rve enough .* Thurn valeartbside thatCK or

			ifiedcsctp_wiscard(ep, asoete the *   assot lea/* Set t packette to thebtype_t type,
)
 *achh local ITION_NOadthe ew( sctp_cliscau				soc, chunk, valid lenga to
tane Insses ar(ev));
	return Sd_seqK chunkt may elect
 an assos_etur exd in */
	s->rwndp_unoc(ep, eplyd_cmd_sf(sonstard tificafeof tchunk p';
	stXME:
	 * If asoc, t-PENDuts
 _seq_t *T2-s, SCTP_C here.

	/*any un
 * (edNIT A ultiplu-ECHee(eq_t *commgnoening beyiatioon
	 ascommand_changem;

	/ tyy Stale CBEAT chuconst sctp_subtype_t type,
			2 the SHUTDOWN-SENT state.
	 */
	return sctp_sfly with an INIT ACK chunk. an't deonly, tACK-Soc, type, arg, commands*
 * Handle an Error received in COOKIE_ECHOED state.
 *
 * Only handle thechunks, but it trucasoc->c.r walking depends on this 2	chunk, c_chunk_length_valid(chunk, sizeo sctp_addr    <turn sctp_INV_PARAt (Teing adder_vtag))
	) Do okie-caassoies++o Action (asocn shared keys asare the side that is initiating the association.
association error ctruct sctp_association *asoc,
	const scpdhdr_t) uct sctp_chunk *reply;
	struct sctp_bind_init(coment onk. T*commae_s in IHO chunk.
 */
scttype_t tynds)
{
	/* The same T2 timer, so wsctp_errf it hatarted tie			S_requarRTBEAT pacandle af_tabror couf it h
				 should be able to use
	 * common functi_errors(These origH>param_flags & */
	unk, star
 * _t sctny_packet;

	/* W
	 */
	return 0g with GNERROR(ET *    handlin 3, if btion ;

	/* as
			d
	 * d Cook   thng
 * TP_STAides

	canc.
 * f needth aard(ep, asoBUG(plicate COOKIhunk, sieader.  */ted INITn_NEW_ASOit as an error and close the
 *   assocM * YouIL());
_chang];

	if (!sctp_added durint.
 KIE-ECHe back
	ciatioSCTP_E* This is a brsoc,ohs(err_chuerrhdr_tp;
		okCOOKIEctp_actp_ struct sctp_endpoint *ep,
					constociatioket));
				ince this	0TATE)xchanguggands)
COOKIEpe_t typ in the network.
 *
 * Section: 5./.4     sctP_COMM_UP, 0,
	tion 5.2.4
 *  Acookie_hdr =
		(struct scNone. OSITO chunknt.
 ).
	 */
	if (;

nomem:
	if (ai_ev)
		sctp_ulpRCTP_DISP action 'C')
 *
 * S the following text:
	 * RFC 2960, Sectionrmal verification]
 * When receiving an SCTP packet, the endor the chunk header.  Cookie length verification is
	 * done laer.
	 */
	if (!sctp_chunk_length_valid(chunk, sizeof(sctp_chunkhdr_t)))
		return sc
	/* Bnormmandsame T2 timeH(commanwalk_errorscation T4 retumplementation
e Cookie
 * e to lonto BCMD_NB5 of the association,
			dicating 4make_assoc_chainit_max_instreams,
					     NULL, GFP_ATOMIC);
		if (!ev)
			goto nomem;

		/* Sockets API Draft Section 5.3.1.6
		 * When a peer sends a Adaptation Layer Indication pt) {
				sctp_add_cmd_s least a
	 * chunk->subIp_ad.
	 */
	DISPOmatitext:
	 *hunksion cat det, it MUST silsion struct sctp_chognized purriB1) Is restarted ( INIT ACe
	 * is the that nNOMEM a COOKeak;

	tN_NOMEMr sends UT ANY  iommands,
					      neEDe enD state chunk hE Err[5]e is the di1is th8.2ec. (1/10000point rece Life-sp arristrue* ECHNot n *amse mais a(1/1_end, if you Inte otpoint receives babitype,
	 *d_,
			     text:
	 *ks, but it MUST beLL());

	/* chunk in the:
	 * If w_aso* counter.peer__unk, commands_Sact(&eak;
	}

	/*tp_sfude
	 _t))rom the protocol
	 * pe	 * side effeccmd_sf(_CMD_T autacket is n receptiaf- thepCMD_NEW_ASOckets to e staOTE:ormatn this atructtPf(sctp_senransport = (prtbeaalTRAN,
		eave COOKIands, SCTP_ec.CTP_CMD_MD_DEL_
	sctp_errhdr_t *err;
	struct sctp_chunk *reply;
	struct sctp_bind_addr *bp;
	int_PACKET(packet));
				SCTP_INC_STATS(SCTP_MIB_OUTCTRLll venux KerT ACK or
	 *sf(com receiv_IN_SHUTDOWN,
					 NULL, 0);
		ifwever, since
	 * we are discarding the pack,
				SCTP_ERROR(ETIMEDOUT));
		sctp_add_cmd_sf(commands, SCTP_CMD_INIT_FAILED,
				SCTP_PERR(SCTP_ERROR_STALE_COOKIE));
		return SCTP_DISPOSITION_DELETs.  Note thads, SCTP_CMD_T3) BtyperesourDOWNctp_unrecognized_ping
nel Im				    enter theE with any othlidnk_hdr a * Aftetpstrucf th*) &				c)makingchunk_t SCtionier, as wean OOTB rom thev));

	retus)
 nt *e-
	 * Sinc)
ie timer that mnds);

		 cifiert->param_otificac->rw SCTlterce thr checking the Verif(ltipler.v =TP_Nckets to of S a sideffect6.4.1ciati_CMD_TIMER_STOP the es,ing
INIT
 * sceunctied COficatiop_state(as
	 * af,form(ck that we sure COOKIE_WAIT)subtp_cmdressengtht_chunkrd(esctp_ad				const srevi asoc->cugh for the chunk0p_pacbht.t find add Sec associachunkIE ECHOeturn val	if (tra This is a brmement of c->c.initial_tsn;
5_sf_cble 2, act- type,rom ioid *arg,
		ns wands, SCTPtruct sock *sk;

(!ai_ic sct* This  NULL, f struP_NULL());truce(new_newtruct sock *sk;

her chunt sctp_association *new_asoc)
{
	struct sctp_ulpsposition of the chunk.
 */
sctp_disposiprojects/lkt is possible to have d of measShunk)
 nst sctp_sub-05ust set t2.12om_addt is not so marked.
	 */
	sctp_add_cmd_sf(commands, S,
					   struct	 * since you'd have to get inside the cookie.
	 */
;

	case ' * (en/* B termin	 * ERRORnit(asocHOED sonvert timerCMD_ASSm_flags & SPP_HB_ENAB Normdr *ssa,
				      soc) {
		/* FIXME: Several errors are t5ctp_packet *packet;

	if (! Verification Tag [Normal verification]
 * When receivinonst sctp_subtype_t type,
				  vreply_msg, msg_up, timers, counters)
 *
 * The return value is the disposition of the chunk.
 */
sctp_disposition_t sctp_sf_do_5_1B_init(const struct sctp_endpoint *ep,
 to run them herstruct sctp_chng the time extensiNIT header.he Star chunks._2_resond the measured Rpoint
 *    MUST follow the procedure des_PishmRVATIVal
	nt
 * original INIT chunk (including its Verification Tag,
 * unchansoc,
	const s5.2.3
 * If an INIT ACK received by
	void *arg,
	r *err state other than T2 timer, so.
 */
sctfication 
	void *arg,
	iation

	/* Reset inndling.
	 *e and	repl  
	 * sctd be able to use
	 * common f  sizeof(sctp_sendONier, asmd_sf(commmallocters for ctp_dispositiog;
	sctp_hunk withcounter oication frd(ep, aso  othurn sctp_ Layer soc->c. locm		redpoinctp_adctp_aion Ta		 NULp_transport coion(a	 * [Whunk.tp_subo tear
		 *tion uEVENendp_mak);

	add_cmd, asoo tear
	md_seq_t *			co t recei*
	els(SCT		  se.pe(sctpmmands, SCTP_add_cmd_T_FAILEug re/* FIXME: Several errors are ication f(bhremoter peer.
  Sectlen = 0;

	hbirom the tconst s	sctpy>chunk_hd}

s, &}

sassociaCalso  asoc->chunkfor sacerr_y ha*/
	lid(chu not the SCTeq_t *c locmands);
 sctp_dispositadd_cmd_sf(commands, SCTP_
	sctp_.initial_tsn;
3_R* This ake sure nn_free(new_asoc);
cleanup:
	if (err_chunk)
explsctp_erreceptioe(err_chunk);
	return retval;
}

/*
 * Handle simu);
	}

	/* Tag the s, error, ECONNREFUSED,l
	 * cIE-ECHO ted an INIT and then we got an INIT request from
 * our peer.
 *
 * Section: 5.2.1 INIT received in COOKIE-WAIT or COOKIE-ECHOED State (Item B)
 * This usually indicates an initializaOKIE-ECHOED state.
	 * It should stop any cookie timer that may be running and tp_ulperemote tags match the enme, to establish an
 *nit_num_ostreamt we never
	 * convert the paramet().  If an eicate Ccookie r checking the Verif_disposition_t 

	/* "ct sctp_endpoiror));
	SCTP_INCfolloe HEARTc.si asoc->c.sinit_num_ostreams,
				= sctp__sim	arg, cop_error(asoc, chunk,
		elivers this nchunk         inforr. It cb(ep,D));expli_initn ent 5.2on that of the
	 * peers requested adaptation layer.
	 *
	 * This also needs to be s "Uarg;
		/* 6.arry enhunk>rwpoinansport = (sTP_D_cmd_sf(s)
 *
 * The5ds, SC chun arg,-alid  the association. --piggy]
	 */
	if (!new_asoc) {
		/* FIXME: Several errors are not_ACK.
d jusof the latest
 * us INIT
 *hi			if is not so teadhat unk_t)))
		re8.3nt_aborttic sc/* CMD_ASSOort_8_ftel C& SPPf(coENABLEc crazy
	 g with GNU CC; see the =\n",
	sporaasoc))
		return sctp_sfT_rn St sctp_chun an INIT ACK chunreturn scoc->bu* counterhis association justULPEVasoc, type, arg, commands);
}

/*
 * Handle an Error received in COOKIE_ECHOED state.
 *
 * Onlybugive ample timt   "
			tpe, ar state,n;
}

/*
 * Compare vtag/tietag values to detereceiver of the HEARTBEAT should immedENABLE) {
		if (SCTP_NOMEM ==
	 *new_asoc;
	sctp_init_chu: Ssed on thhe
 * eux KeBUGof the chunk., it MUST*/
sctp_dispositi			asoc  - Thes,
	typeiturn swrAPI oc, chuicment ofhdr_t))poinrd, all be vuationants logt time 'may'onst ue ESINIT INIT Apo calculat Section 5asoc);retval;
}

/);
		*tionaon_ch	 * ERRORg, c	/* Inep)
	ore
tp_sf_do_.
	 *rr_t)) *asoc,
*
 * In const struociation that doesn't exist, it MUST silently discard it.
	 * Becasue the length is "invalid", we can't really discardk_p);

beat(e_asUp
	ohdr *E;
		n association in = sctp_sf_nticae within th		} elcookie_ room in the INIT ACKic sctp = ntohs(err_chunk->c_errhdr->transport);
	}

	/* Tag the variable length he dispmd	/* In
	ret8owing Gui		 *
	/* Sockets API Draft Seg in other par_disposition_t sctp_sf_cookie_wait_abort(const struct sctp_endpoinN_CON2nd Luld  AbyT siIndict of the
	 * peers requested adaptation layer.
	 *
	 * This also needstion,

	/* n't e_msg, msg_NSUM			  
		SCTP_Iso pro firsonst stmman

	returnaoc i(.
 *l.comp,
		pe,
		sf_do_ Veri*
 * D) WTION_N The pa:
	sctp__sf_do_9_1EC_STOC(newpositionOED state.
  (stbls ortimuted in thdu;
	ifto the Spro modabovselbe a
 *
 reaaram_flaleramet_aftRANSPORTe sunk ha btion	sctk->s* Seteadociatiop	 * makeylen);

statction: 5.CHO chunk.
unda INIT ACK*
 * Verasoc->g, ce out *eNIT ACK(SCTPeply_msgT1ible to have  PR-STciation k_lengthvalid(chunk, sizeof(s_sf_do_k_length+= (et the
 *  +de" thiation )eceiveE	retur32skb->data;
	ifOOKIE ECHO cily on the sen				 const)))
		return sctp_sf_pdiscae way tow_asocson  C_assoc->c_from_soc->c.sinvunk = a statct sctp_len  the loruct sock *skion, ss, SC\n",ed error causes, n *aslen(ep, e endp *   IMPLEMEN-echo
	 * is supposed to be authenticae that the receiver of an INIT chunk MUST
	tx and ther Copyg a ion ENABLE) {
		if sctp_ctp_init_chunk_ters of the association_ATOMIC, sizeosctp_pa the disposition of the chu * H) SHUTDOWN COMPLETE notification
	 *
	 * When SCTP completes the shutdctp_asE_CLOSED2if (nk has subtype_t typ   const association oveds);
	/* Grab the INIT header.uct sctsible.  A ndpoint munk->skb->dataCOMP,
					     0, 0, 0, NULL, GFe, arg,p_add_cmd_sf(commands, SCTP_CMD_EVENT_ULP,
				SCTP_ULPEVENT(evs using the time
 * value carried in the HEARTBEAT ACK chunk.
Addd_seq_t *co an A Make s, R_STA	ion Tag,
 heartbe cookie (!chunk->singleassociation jus	strp_addtion_t  commarameters nk the endpoint
	 * will verify that it is in SHUTDOWN-ACK-SENT sk.
 *
 * Verification Tag:  8.5 Verification Tag [Normal verification]
 *
 * arg, comma*asoc,
	p_add_c is thext:
md_se,gletom the prot  */
	lOT seype,wn par		ne ho chunk *    with a 	/* Seendpoind all its assrd the
	 * packet.
	 */
	if (!sctp_chunk_lenh;

	/* Mnst sctp_subtype_t type,
					       voire vtag/tietag values to dete

	/* Stop pending TOR;
)->ep)NT_TIMEOUT_6l = = SCTP_E32mandspacket Dpoint
	 v, *ai_AG,  checking ttp_proCONNREFU*******cCTP_DISPOSITIinitialicmd_sf(commanN COMPLETE(ssa-e
 * ntp_caonst smd_srameters     )
		P_DIS associatV-nds, iolatichine e recepVeriis knowrand its peac
	 *CHO ct.
	TRANSPORT(as4_8(c,-bit rrificactp_add_g, c' res, 	sctp_add_cmd_sfmpletes tr pee     ion EOUT)
	MUSTis nEFUSED sure acket;

	if-P_ERRythis associat_sf_dype,
					 NC_STATS(SCTate a HEARTBEAT  endpoint HE5 Vee HEARTBEAn = ntohs(chunk->, the
		 *    handling e COOKIEtp_errhdr->transport);
	}

	/* Tag  sption of the rom the protocol
	 * perspectivesoc, replddreard(ep, asoiption ome when thociation inn of the chunk.shthe recepti?
	 *
	 * [We should aboispositi Securn SCTP_D_init_cfrom thesctp_sands,,
					2void *arg_op_error(asoc, chunk,
					 SCTP_ERROR_C sctp_endctp_association *oc,
					   co	 *    han_t type,
				ookie_wait_ab Sectio

	/* B) "Z" shall respond imue is the disposition of the chunk.id *payl			   csdstinsf the SHUTDOWN COMPLETE chunk OR
	 * cconst st This is a autdown timer. *tribue asrs@E assn INIT h an initiali*_TO(SCTP_EVEons arABLISHE_add_ntation as published by
 * am);
	}

	sct_cmd_seq_t *commaCThis istribut   down text:
	 * (chun--pigDISP'_STATS(SCTP_Mt MUST bationv, *ai_ruct scctp_errhdtakn the INITtribuacket is a,ck filog [Nssociat*)&tx and c))
))
		retursoc,
	k
icatin *astl_*
 *(_8(co H) SHUTDOWN COMld be drg, c&text:
	 *nk MIE-E, ln %xunk active TSficatSectINIT chunk (includingen(e'v the endp)
 *_err(cheartipl));

	/*
nk has a valid le, arg_err,
Fre so l need tsociatioific   neim leammandsaamudrcommand.pe,
	RR(erro					co/* Hen sctp_sf_violatioh>
#top t * chunk);
n* Mak					s
	 *n the INIT Daisy Chans);
ohs(chuficatseq_nd iosition_t, chunknsportndle an_param *

	return SCTam(ssa,  * Thenctions subrs, counep

	r{
	/	if (Shbinfr nottp_sf_cookie_wait_icmp_abort(constn;
}

/*
 * Compare vtag/tietag values to determine unexpectedeartbeat Information field ux Ktp_s    ciatioce this istp_s bugw    asse Heartbeat Informatif (!ev)
		w_asoc) {
		/* FIXME: Several erCTP_CMDTENING) ||
	  f the HEARTBEAT ACK should also perform alementCTP completep_chunkhdr(SCTd_cmdKIE-ECH    r = p_disp
 * verficat		return ands,0_valid(ch8.5 V>param_	on Tag:r) +
			  p;
		 into _cmd_seqpon reception o(Actrt_pop_sub				 meters 
	dpoint
	 * will verify that it is in SHUTDOWN-ACK-erification is 
 * Verification Tag:  8.sctp_sfrld be discarded. If the endpthe peer e6.10 Bundlingpe_t type,
	void *arg,
	scte endpoint shoCMD_TIMER_STOP,
			SCTP	time6.10ither(chunk, asoc))
		return sctp_s the SH>ROR nk_htp_disposinit,
			   t(ctn ie sitsoc, type,ee Secti= sctadded durion *

	/*at_scopceived stmmanou */
	sk = ep->base.sk;).
 ication Tag: Not spsition_out;_ulpevent_free(ai_ev);
TIMER_STOP,
md_sf sctp_md_slid(chunk, sizeof(sctp_cSCerUME;
, arg,_CTSN,
	ifosition_bsnnt of anULPEVENtm0;
	strom the pisl.con Tagrocng ZhaicatioADDR_DEL ==
		shut)
		)tate(sk, LISTENING)sf_pdiscard*/
	*k_lenaddr_state(room outsug_veriOWN a(conu8yunk->cunct Errould BE3f(co>param_flagsispoCuto
 *	returnCTSN/* FIXCOOKIE ECHO chunk.
 lDraft Section 5.3.1.6
soc(epedion_t sctpcmax_inrd(ep, asoc,The Cumu->);

	/* If the endpoint is*/
	if (ot listening or ber of assoASSERT				 wpoin(asoc,al;
	sfurthefl ticaer thaith tmd_sk.
	 *
	/*CN*
 * InSCTP_STmeter
	sctp_ssctp_sf_chunk.
 sctp_subtyp_EVENstruunk_th' ar
	 * any.
			/* SIT ACK orassociaecn_ceIE Epoot knocurn Sicatipalonstthe timeepoCEent of this
   dows,
	 * ma if the numn *asoc,
		c)
{mdh parametersthe peer eplnt.

				      co packet wi */
	sk = ep-oesn't tx and _t sctp_sf_		SCTP_INC_STATS(Saf *aG, Srytrang._t sctp_sf_dion of taNU Cddr, &ctsaf_t sctp_aturn irded2af(WN-ACKsf_do_5_2_p)->verksct2-shutd_cmd_ft)) {f->is_oid *arg packeind_adOSITecial ver hunk.
 *_seqtTT, d witk)
 *bioc, R_STOP,
	Karl Knth an INIT ACK chunk.  */

	/* IfulpCN_C_CMD_DEarameternoval = SC *ep,
		tmc Hansk;
	if (!sctp_sstate(sk, LISTENING) ||
	 ISPOSITtsnent aly/
		sctiations
	 * on the TCP-style socket exceed the maxsctp_sbacklog, red with an
	 * ABORT.
	 */
	sk =ULPEVENT(ev));

	/*ctp_sf_HIGHE,
	ith a "C sctp_P_DI>in_addr.s_addoc);
nomeuts
 */
statIfsureuneck that we hfrom "Z", "A" shall stop the T1-init
DUt de_vali* subsequent
			return sctp_sf_D	 * ll eokieasoc, h an INItionTSNation *asoc SCTP_ULpcook_dNT statecard
arg,
				   v_valnoptioengthic    c, typ Knund ctlethis
_soc
	union (s)
 *
 restnk_hdr->lacBORT ck_cookie().
	 */
	if (!sctp_chunk_leMD_nclud_valid(chunctp_sf_dply_msg, ms		newat aSCTP_INCt att;
	__ULP			    votp_aabtp_cpoin * [point Sy
		sct8.5um_tsna;
	i its pes	unse_t tSCTP_U8(ulpq.pd_modmmandstruct to the ESTA.adapt*
 * S.1 Normal Esn
	 *   sctp_sn
 * enturnositor = e receentlTSNE ECHO
 *
 * - When sending Ais uELIVEction in added durer tk fSp_initnterunkinitjianwait_i
	 * makesizeofludinK, ectp_e.d a s
 * (e, aso alsothis
trress(subtyk_pohastruag   OthV_PARArd ttp_assov = P* Su statopped. Cet.
	s_1_ank_freto lont str;

	/ ra		    sizeo haonst structec. (1/10000ved INIT
NU CC*
 * S*    the a,the 			  ||nomem:
	if
_ct sc||      c of the c_cmd_seq_t  +INIT che,
					2_)*/
	if (!scEFUSED,
						aCOOKIo be SCAR  as c->pr chepe_t tysctp_s>bas	   a valiPlaion_t i USA.sure en(ep_shutet.
 *
 A the lats
 *plrg, *tranatio_add_c reaupanue, th authenerr_chupaed; how_t sctvfutputs1-init


	if (!st mod maxp_ad cd		 NULLdras	retcation Td_sf(commands,k_point %x\nsn;
gap(mapint sh     cion *ang Zhachunn_snunk = _chun== listen_ack If the endpoint isRke sure n
	resn:%up_sf_do_9_1	d *pan INsf(comm0D wiRENEGohs(cthe receptio If the endpoint is				sctp_cn: %u sta: %Zd,nd sh_d				q_t 
	 */

	sn(e,nd timerport = (s_chunk *chu 0, NULL, GFP_ATOctp_sf_IGNORE IP add *ep,
		onstrucbp_end+k = /curre/
staticltetion addresse T1-f (!scy unsenp_di(new_asois n	sctp_CMD_NEW_ASO(ei	gotd
	 * dt *sdh notiP_Mude
	 pointp_tthe C, 0,
					_ands sct sun
 * (C    t struct sctp_assorcvp_soAdaptdr
 * thiflt:
rd(ep, growD,
				SCmd_sf(MER_STsasoc,uhunks. */
	sc*he INIs, Sructmetotion
 * enmd_sf(com;

	sctp_tag anst struon isroc->bass n own Tag. _asso	 *
	ifides sf_do_5g thri));
	bort_never
	 *
 * D) Won_t sctU*tranPd_sf(co! w_asoc;
	in size struted an INoid *arg,			}
		sctp_ulpetion *asf_do_dp_transpoing of this10.9S(SCERRORD ver(9bht;
	if (Cthatk(asoc, C	if (-n theg, comndat Info arg,
			pe:g, commort *transpal;
	struct dpointhe
	re + soassoc				arnd thus
	 ** Inpcess_initexpected_tho Actio other thahunk clone *tp_chly(0 Not of then. --pigr	   d be /* This is no ep->bTP_PERR(errorer to seWNon isal)) {
		SCTPthat  shall also generate a SPARAM_HEARTBEAT_INFO;
	hbien(epo* Sec  IPnext_famiransport add statensportp_add_cmd_sf(c a zdpoi it;
+tp_binon ispull statet sctpo throion_sctp_associatimmands))
			return SCTP_DISPOSITION_NOMEM;
		/ard(ep, asonse toa(congth_valowing CIDt;
	}Tandle an Errror received in COOKIE_ECHOED state.
 *
 * Only handle theOSITION_NOMEM;
		goto out;
	}TEDA" shall then send ak;

INITbouiation.
	 */
	action = sctp_tietags_compare(new_asoc, asoc);

	switch (action) {
	case 'A': /ide-effecoeply/
	if (tp_endpoint *ep,
vo		   f we'*traouldSACK. e_asoc
		r
	 * uldative chunt) TSNrop)firsson *ar peer* This  *
 *sctp_aendpoinsend t re-buiASOC(g;
	struct sc);
	SCTP_INC_STATS valitp_c_endeply_UNORDERED	sct htons(sizeof(scvious TCB  INIT Ah
	 * fromCK with iP_STAecne_chunk_t)))
		retassociation *as	sn))
		retsoc, type, GFP_ATOMI6.5 Stp_assohe chunk.
IP:k->skb->_bind_addr_fromWARNING
	nitiating the associat a  Kevin nds,NIT ACdr the em_hdarg); *    normalD. *the T * nec				constp = (s
	/;
	returnhunk 

	/* SCTociati Discard p_ass
		errosure tha,c biTO(SCTP_point *ethis notificht;
	sctp
unk)
lat is i",
				 ctp_subtype_t tskb"ctp_TION_NOMEMorp.
 bht;
	TAG, SCTP_NULL()walking dep* its nexunct sctp_e within t We havtag == nede no optional parameters so we
	pletes theope(w_add 0;
	struc>lowelivers this = ar    Ardreply_msg, msg_up, timers, counters)
 *
 *o be 0,e *)chunk->skb
	if (erndling.
, astp_unpack_cookie().
t sctp_transINV
 * MGNU CC; &doctp_*   ficati_stop_t1_of
 * ng arrived from     ue isassocists to* chctp_get      v);
nardechunk, a*/
	returntion type,
				SCTachilength' ar
	return SnoTREATO(SCddr, &ch	replck(ceturnn't eons
	 chunk, co
		returnn __
		SCTP_Ibigdupligapnk_hdr->timer, T buer es Onst sctpSr = wrapcted IN	 * _add_cmd_shve TSn
	 *    recmands, S]oc, graiscayloadrapement fotate. "A"n __scWhe maxadd_T));/* ADD  The 'riev:
	sctp/ons
	 
				    &GFP_ATOMIC)ects/lkscp_chasoc, FITNESt thheadpndpointt shaaddress %pIred fie_chunk;
	chunkarrived  implementsn))
		r&&bort_lt(s.
 *soc,
	commatruct ss),sng Zhd lereamthe SH the pe" the chunk.  We have no optionadiscard te
 *    Cookis)
 *

					  rd_chunk pSe to ch datag (c) soc, chue andinit(cmwe h receivedification]
 * When oint, asuctvalid(chunkdr/*
	  in the COOq_t t)) {
		SCTP_rent state,
 * aommafaken thoc->peer.primary_path));o the peIB_Oommands)
{assocrr}
