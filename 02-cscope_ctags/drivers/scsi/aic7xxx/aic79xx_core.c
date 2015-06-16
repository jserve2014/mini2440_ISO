/*
 * Core routines and tables shareable across OS platforms.
 *
 * Copyright (c) 1994-2002 Justin T. Gibbs.
 * Copyright (c) 2000-2003 Adaptec Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/aic7xxx/aic7xxx/aic79xx.c#250 $
 */

#ifdef __linux__
#include "aic79xx_osm.h"
#include "aic79xx_inline.h"
#include "aicasm/aicasm_insformat.h"
#else
#include <dev/aic7xxx/aic79xx_osm.h>
#include <dev/aic7xxx/aic79xx_inline.h>
#include <dev/aic7xxx/aicasm/aicasm_insformat.h>
#endif


/***************************** Lookup Tables **********************************/
static const char *const ahd_chip_names[] =
{
	"NONE",
	"aic7901",
	"aic7902",
	"aic7901A"
};
static const u_int num_chip_names = ARRAY_SIZE(ahd_chip_names);

/*
 * Hardware error codes.
 */
struct ahd_hard_error_entry {
        uint8_t errno;
	const char *errmesg;
};

static const struct ahd_hard_error_entry ahd_hard_errors[] = {
	{ DSCTMOUT,	"Discard Timer has timed out" },
	{ ILLOPCODE,	"Illegal Opcode in sequencer program" },
	{ SQPARERR,	"Sequencer Parity Error" },
	{ DPARERR,	"Data-path Parity Error" },
	{ MPARERR,	"Scratch or SCB Memory Parity Error" },
	{ CIOPARERR,	"CIOBUS Parity Error" },
};
static const u_int num_errors = ARRAY_SIZE(ahd_hard_errors);

static const struct ahd_phase_table_entry ahd_phase_table[] =
{
	{ P_DATAOUT,	MSG_NOOP,		"in Data-out phase"	},
	{ P_DATAIN,	MSG_INITIATOR_DET_ERR,	"in Data-in phase"	},
	{ P_DATAOUT_DT,	MSG_NOOP,		"in DT Data-out phase"	},
	{ P_DATAIN_DT,	MSG_INITIATOR_DET_ERR,	"in DT Data-in phase"	},
	{ P_COMMAND,	MSG_NOOP,		"in Command phase"	},
	{ P_MESGOUT,	MSG_NOOP,		"in Message-out phase"	},
	{ P_STATUS,	MSG_INITIATOR_DET_ERR,	"in Status phase"	},
	{ P_MESGIN,	MSG_PARITY_ERROR,	"in Message-in phase"	},
	{ P_BUSFREE,	MSG_NOOP,		"while idle"		},
	{ 0,		MSG_NOOP,		"in unknown phase"	}
};

/*
 * In most cases we only wish to itterate over real phases, so
 * exclude the last element from the count.
 */
static const u_int num_phases = ARRAY_SIZE(ahd_phase_table) - 1;

/* Our Sequencer Program */
#include "aic79xx_seq.h"

/**************************** Function Declarations ***************************/
static void		ahd_handle_transmission_error(struct ahd_softc *ahd);
static void		ahd_handle_lqiphase_error(struct ahd_softc *ahd,
						  u_int lqistat1);
static int		ahd_handle_pkt_busfree(struct ahd_softc *ahd,
					       u_int busfreetime);
static int		ahd_handle_nonpkt_busfree(struct ahd_softc *ahd);
static void		ahd_handle_proto_violation(struct ahd_softc *ahd);
static void		ahd_force_renegotiation(struct ahd_softc *ahd,
						struct ahd_devinfo *devinfo);

static struct ahd_tmode_tstate*
			ahd_alloc_tstate(struct ahd_softc *ahd,
					 u_int scsi_id, char channel);
#ifdef AHD_TARGET_MODE
static void		ahd_free_tstate(struct ahd_softc *ahd,
					u_int scsi_id, char channel, int force);
#endif
static void		ahd_devlimited_syncrate(struct ahd_softc *ahd,
					        struct ahd_initiator_tinfo *,
						u_int *period,
						u_int *ppr_options,
						role_t role);
static void		ahd_update_neg_table(struct ahd_softc *ahd,
					     struct ahd_devinfo *devinfo,
					     struct ahd_transinfo *tinfo);
static void		ahd_update_pending_scbs(struct ahd_softc *ahd);
static void		ahd_fetch_devinfo(struct ahd_softc *ahd,
					  struct ahd_devinfo *devinfo);
static void		ahd_scb_devinfo(struct ahd_softc *ahd,
					struct ahd_devinfo *devinfo,
					struct scb *scb);
static void		ahd_setup_initiator_msgout(struct ahd_softc *ahd,
						   struct ahd_devinfo *devinfo,
						   struct scb *scb);
static void		ahd_build_transfer_msg(struct ahd_softc *ahd,
					       struct ahd_devinfo *devinfo);
static void		ahd_construct_sdtr(struct ahd_softc *ahd,
					   struct ahd_devinfo *devinfo,
					   u_int period, u_int offset);
static void		ahd_construct_wdtr(struct ahd_softc *ahd,
					   struct ahd_devinfo *devinfo,
					   u_int bus_width);
static void		ahd_construct_ppr(struct ahd_softc *ahd,
					  struct ahd_devinfo *devinfo,
					  u_int period, u_int offset,
					  u_int bus_width, u_int ppr_options);
static void		ahd_clear_msg_state(struct ahd_softc *ahd);
static void		ahd_handle_message_phase(struct ahd_softc *ahd);
typedef enum {
	AHDMSG_1B,
	AHDMSG_2B,
	AHDMSG_EXT
} ahd_msgtype;
static int		ahd_sent_msg(struct ahd_softc *ahd, ahd_msgtype type,
				     u_int msgval, int full);
static int		ahd_parse_msg(struct ahd_softc *ahd,
				      struct ahd_devinfo *devinfo);
static int		ahd_handle_msg_reject(struct ahd_softc *ahd,
					      struct ahd_devinfo *devinfo);
static void		ahd_handle_ign_wide_residue(struct ahd_softc *ahd,
						struct ahd_devinfo *devinfo);
static void		ahd_reinitialize_dataptrs(struct ahd_softc *ahd);
static void		ahd_handle_devreset(struct ahd_softc *ahd,
					    struct ahd_devinfo *devinfo,
					    u_int lun, cam_status status,
					    char *message, int verbose_level);
#ifdef AHD_TARGET_MODE
static void		ahd_setup_target_msgin(struct ahd_softc *ahd,
					       struct ahd_devinfo *devinfo,
					       struct scb *scb);
#endif

static u_int		ahd_sglist_size(struct ahd_softc *ahd);
static u_int		ahd_sglist_allocsize(struct ahd_softc *ahd);
static bus_dmamap_callback_t
			ahd_dmamap_cb; 
static void		ahd_initialize_hscbs(struct ahd_softc *ahd);
static int		ahd_init_scbdata(struct ahd_softc *ahd);
static void		ahd_fini_scbdata(struct ahd_softc *ahd);
static void		ahd_setup_iocell_workaround(struct ahd_softc *ahd);
static void		ahd_iocell_first_selection(struct ahd_softc *ahd);
static void		ahd_add_col_list(struct ahd_softc *ahd,
					 struct scb *scb, u_int col_idx);
static void		ahd_rem_col_list(struct ahd_softc *ahd,
					 struct scb *scb);
static void		ahd_chip_init(struct ahd_softc *ahd);
static void		ahd_qinfifo_requeue(struct ahd_softc *ahd,
					    struct scb *prev_scb,
					    struct scb *scb);
static int		ahd_qinfifo_count(struct ahd_softc *ahd);
static int		ahd_search_scb_list(struct ahd_softc *ahd, int target,
					    char channel, int lun, u_int tag,
					    role_t role, uint32_t status,
					    ahd_search_action action,
					    u_int *list_head, u_int *list_tail,
					    u_int tid);
static void		ahd_stitch_tid_list(struct ahd_softc *ahd,
					    u_int tid_prev, u_int tid_cur,
					    u_int tid_next);
static void		ahd_add_scb_to_free_list(struct ahd_softc *ahd,
						 u_int scbid);
static u_int		ahd_rem_wscb(struct ahd_softc *ahd, u_int scbid,
				     u_int prev, u_int next, u_int tid);
static void		ahd_reset_current_bus(struct ahd_softc *ahd);
static ahd_callback_t	ahd_stat_timer;
#ifdef AHD_DUMP_SEQ
static void		ahd_dumpseq(struct ahd_softc *ahd);
#endif
static void		ahd_loadseq(struct ahd_softc *ahd);
static int		ahd_check_patch(struct ahd_softc *ahd,
					const struct patch **start_patch,
					u_int start_instr, u_int *skip_addr);
static u_int		ahd_resolve_seqaddr(struct ahd_softc *ahd,
					    u_int address);
static void		ahd_download_instr(struct ahd_softc *ahd,
					   u_int instrptr, uint8_t *dconsts);
static int		ahd_probe_stack_size(struct ahd_softc *ahd);
static int		ahd_scb_active_in_fifo(struct ahd_softc *ahd,
					       struct scb *scb);
static void		ahd_run_data_fifo(struct ahd_softc *ahd,
					  struct scb *scb);

#ifdef AHD_TARGET_MODE
static void		ahd_queue_lstate_event(struct ahd_softc *ahd,
					       struct ahd_tmode_lstate *lstate,
					       u_int initiator_id,
					       u_int event_type,
					       u_int event_arg);
static void		ahd_update_scsiid(struct ahd_softc *ahd,
					  u_int targid_mask);
static int		ahd_handle_target_cmd(struct ahd_softc *ahd,
					      struct target_cmd *cmd);
#endif

static int		ahd_abort_scbs(struct ahd_softc *ahd, int target,
				       char channel, int lun, u_int tag,
				       role_t role, uint32_t status);
static void		ahd_alloc_scbs(struct ahd_softc *ahd);
static void		ahd_busy_tcl(struct ahd_softc *ahd, u_int tcl,
				     u_int scbid);
static void		ahd_calc_residual(struct ahd_softc *ahd,
					  struct scb *scb);
static void		ahd_clear_critical_section(struct ahd_softc *ahd);
static void		ahd_clear_intstat(struct ahd_softc *ahd);
static void		ahd_enable_coalescing(struct ahd_softc *ahd,
					      int enable);
static u_int		ahd_find_busy_tcl(struct ahd_softc *ahd, u_int tcl);
static void		ahd_freeze_devq(struct ahd_softc *ahd,
					struct scb *scb);
static void		ahd_handle_scb_status(struct ahd_softc *ahd,
					      struct scb *scb);
static const struct ahd_phase_table_entry* ahd_lookup_phase_entry(int phase);
static void		ahd_shutdown(void *arg);
static void		ahd_update_coalescing_values(struct ahd_softc *ahd,
						     u_int timer,
						     u_int maxcmds,
						     u_int mincmds);
static int		ahd_verify_vpd_cksum(struct vpd_config *vpd);
static int		ahd_wait_seeprom(struct ahd_softc *ahd);
static int		ahd_match_scb(struct ahd_softc *ahd, struct scb *scb,
				      int target, char channel, int lun,
				      u_int tag, role_t role);

static void		ahd_reset_cmds_pending(struct ahd_softc *ahd);

/*************************** Interrupt Services *******************************/
static void		ahd_run_qoutfifo(struct ahd_softc *ahd);
#ifdef AHD_TARGET_MODE
static void		ahd_run_tqinfifo(struct ahd_softc *ahd, int paused);
#endif
static void		ahd_handle_hwerrint(struct ahd_softc *ahd);
static void		ahd_handle_seqint(struct ahd_softc *ahd, u_int intstat);
static void		ahd_handle_scsiint(struct ahd_softc *ahd,
				           u_int intstat);

/************************ Sequencer Execution Control *************************/
void
ahd_set_modes(struct ahd_softc *ahd, ahd_mode src, ahd_mode dst)
{
	if (ahd->src_mode == src && ahd->dst_mode == dst)
		return;
#ifdef AHD_DEBUG
	if (ahd->src_mode == AHD_MODE_UNKNOWN
	 || ahd->dst_mode == AHD_MODE_UNKNOWN)
		panic("Setting mode prior to saving it.\n");
	if ((ahd_debug & AHD_SHOW_MODEPTR) != 0)
		printf("%s: Setting mode 0x%x\n", ahd_name(ahd),
		       ahd_build_mode_state(ahd, src, dst));
#endif
	ahd_outb(ahd, MODE_PTR, ahd_build_mode_state(ahd, src, dst));
	ahd->src_mode = src;
	ahd->dst_mode = dst;
}

static void
ahd_update_modes(struct ahd_softc *ahd)
{
	ahd_mode_state mode_ptr;
	ahd_mode src;
	ahd_mode dst;

	mode_ptr = ahd_inb(ahd, MODE_PTR);
#ifdef AHD_DEBUG
	if ((ahd_debug & AHD_SHOW_MODEPTR) != 0)
		printf("Reading mode 0x%x\n", mode_ptr);
#endif
	ahd_extract_mode_state(ahd, mode_ptr, &src, &dst);
	ahd_known_modes(ahd, src, dst);
}

static void
ahd_assert_modes(struct ahd_softc *ahd, ahd_mode srcmode,
		 ahd_mode dstmode, const char *file, int line)
{
#ifdef AHD_DEBUG
	if ((srcmode & AHD_MK_MSK(ahd->src_mode)) == 0
	 || (dstmode & AHD_MK_MSK(ahd->dst_mode)) == 0) {
		panic("%s:%s:%d: Mode assertion failed.\n",
		       ahd_name(ahd), file, line);
	}
#endif
}

#define AHD_ASSERT_MODES(ahd, source, dest) \
	ahd_assert_modes(ahd, source, dest, __FILE__, __LINE__);

ahd_mode_state
ahd_save_modes(struct ahd_softc *ahd)
{
	if (ahd->src_mode == AHD_MODE_UNKNOWN
	 || ahd->dst_mode == AHD_MODE_UNKNOWN)
		ahd_update_modes(ahd);

	return (ahd_build_mode_state(ahd, ahd->src_mode, ahd->dst_mode));
}

void
ahd_restore_modes(struct ahd_softc *ahd, ahd_mode_state state)
{
	ahd_mode src;
	ahd_mode dst;

	ahd_extract_mode_state(ahd, state, &src, &dst);
	ahd_set_modes(ahd, src, dst);
}

/*
 * Determine whether the sequencer has halted code execution.
 * Returns non-zero status if the sequencer is stopped.
 */
int
ahd_is_paused(struct ahd_softc *ahd)
{
	return ((ahd_inb(ahd, HCNTRL) & PAUSE) != 0);
}

/*
 * Request that the sequencer stop and wait, indefinitely, for it
 * to stop.  The sequencer will only acknowledge that it is paused
 * once it has reached an instruction boundary and PAUSEDIS is
 * cleared in the SEQCTL register.  The sequencer may use PAUSEDIS
 * for critical sections.
 */
void
ahd_pause(struct ahd_softc *ahd)
{
	ahd_outb(ahd, HCNTRL, ahd->pause);

	/*
	 * Since the sequencer can disable pausing in a critical section, we
	 * must loop until it actually stops.
	 */
	while (ahd_is_paused(ahd) == 0)
		;
}

/*
 * Allow the sequencer to continue program execution.
 * We check here to ensure that no additional interrupt
 * sources that would cause the sequencer to halt have been
 * asserted.  If, for example, a SCSI bus reset is detected
 * while we are fielding a different, pausing, interrupt type,
 * we don't want to release the sequencer before going back
 * into our interrupt handler and dealing with this new
 * condition.
 */
void
ahd_unpause(struct ahd_softc *ahd)
{
	/*
	 * Automatically restore our modes to those saved
	 * prior to the first change of the mode.
	 */
	if (ahd->saved_src_mode != AHD_MODE_UNKNOWN
	 && ahd->saved_dst_mode != AHD_MODE_UNKNOWN) {
		if ((ahd->flags & AHD_UPDATE_PEND_CMDS) != 0)
			ahd_reset_cmds_pending(ahd);
		ahd_set_modes(ahd, ahd->saved_src_mode, ahd->saved_dst_mode);
	}

	if ((ahd_inb(ahd, INTSTAT) & ~CMDCMPLT) == 0)
		ahd_outb(ahd, HCNTRL, ahd->unpause);

	ahd_known_modes(ahd, AHD_MODE_UNKNOWN, AHD_MODE_UNKNOWN);
}

/*********************** Scatter Gather List Handling *************************/
void *
ahd_sg_setup(struct ahd_softc *ahd, struct scb *scb,
	     void *sgptr, dma_addr_t addr, bus_size_t len, int last)
{
	scb->sg_count++;
	if (sizeof(dma_addr_t) > 4
	 && (ahd->flags & AHD_64BIT_ADDRESSING) != 0) {
		struct ahd_dma64_seg *sg;

		sg = (struct ahd_dma64_seg *)sgptr;
		sg->addr = ahd_htole64(addr);
		sg->len = ahd_htole32(len | (last ? AHD_DMA_LAST_SEG : 0));
		return (sg + 1);
	} else {
		struct ahd_dma_seg *sg;

		sg = (struct ahd_dma_seg *)sgptr;
		sg->addr = ahd_htole32(addr & 0xFFFFFFFF);
		sg->len = ahd_htole32(len | ((addr >> 8) & 0x7F000000)
				    | (last ? AHD_DMA_LAST_SEG : 0));
		return (sg + 1);
	}
}

static void
ahd_setup_scb_common(struct ahd_softc *ahd, struct scb *scb)
{
	/* XXX Handle target mode SCBs. */
	scb->crc_retry_count = 0;
	if ((scb->flags & SCB_PACKETIZED) != 0) {
		/* XXX what about ACA??  It is type 4, but TAG_TYPE == 0x3. */
		scb->hscb->task_attribute = scb->hscb->control & SCB_TAG_TYPE;
	} else {
		if (ahd_get_transfer_length(scb) & 0x01)
			scb->hscb->task_attribute = SCB_XFERLEN_ODD;
		else
			scb->hscb->task_attribute = 0;
	}

	if (scb->hscb->cdb_len <= MAX_CDB_LEN_WITH_SENSE_ADDR
	 || (scb->hscb->cdb_len & SCB_CDB_LEN_PTR) != 0)
		scb->hscb->shared_data.idata.cdb_plus_saddr.sense_addr =
		    ahd_htole32(scb->sense_busaddr);
}

static void
ahd_setup_data_scb(struct ahd_softc *ahd, struct scb *scb)
{
	/*
	 * Copy the first SG into the "current" data ponter area.
	 */
	if ((ahd->flags & AHD_64BIT_ADDRESSING) != 0) {
		struct ahd_dma64_seg *sg;

		sg = (struct ahd_dma64_seg *)scb->sg_list;
		scb->hscb->dataptr = sg->addr;
		scb->hscb->datacnt = sg->len;
	} else {
		struct ahd_dma_seg *sg;
		uint32_t *dataptr_words;

		sg = (struct ahd_dma_seg *)scb->sg_list;
		dataptr_words = (uint32_t*)&scb->hscb->dataptr;
		dataptr_words[0] = sg->addr;
		dataptr_words[1] = 0;
		if ((ahd->flags & AHD_39BIT_ADDRESSING) != 0) {
			uint64_t high_addr;

			high_addr = ahd_le32toh(sg->len) & 0x7F000000;
			scb->hscb->dataptr |= ahd_htole64(high_addr << 8);
		}
		scb->hscb->datacnt = sg->len;
	}
	/*
	 * Note where to find the SG entries in bus space.
	 * We also set the full residual flag which the
	 * sequencer will clear as soon as a data transfer
	 * occurs.
	 */
	scb->hscb->sgptr = ahd_htole32(scb->sg_list_busaddr|SG_FULL_RESID);
}

static void
ahd_setup_noxfer_scb(struct ahd_softc *ahd, struct scb *scb)
{
	scb->hscb->sgptr = ahd_htole32(SG_LIST_NULL);
	scb->hscb->dataptr = 0;
	scb->hscb->datacnt = 0;
}

/************************** Memory mapping routines ***************************/
static void *
ahd_sg_bus_to_virt(struct ahd_softc *ahd, struct scb *scb, uint32_t sg_busaddr)
{
	dma_addr_t sg_offset;

	/* sg_list_phys points to entry 1, not 0 */
	sg_offset = sg_busaddr - (scb->sg_list_busaddr - ahd_sg_size(ahd));
	return ((uint8_t *)scb->sg_list + sg_offset);
}

static uint32_t
ahd_sg_virt_to_bus(struct ahd_softc *ahd, struct scb *scb, void *sg)
{
	dma_addr_t sg_offset;

	/* sg_list_phys points to entry 1, not 0 */
	sg_offset = ((uint8_t *)sg - (uint8_t *)scb->sg_list)
		  - ahd_sg_size(ahd);

	return (scb->sg_list_busaddr + sg_offset);
}

static void
ahd_sync_scb(struct ahd_softc *ahd, struct scb *scb, int op)
{
	ahd_dmamap_sync(ahd, ahd->scb_data.hscb_dmat,
			scb->hscb_map->dmamap,
			/*offset*/(uint8_t*)scb->hscb - scb->hscb_map->vaddr,
			/*len*/sizeof(*scb->hscb), op);
}

void
ahd_sync_sglist(struct ahd_softc *ahd, struct scb *scb, int op)
{
	if (scb->sg_count == 0)
		return;

	ahd_dmamap_sync(ahd, ahd->scb_data.sg_dmat,
			scb->sg_map->dmamap,
			/*offset*/scb->sg_list_busaddr - ahd_sg_size(ahd),
			/*len*/ahd_sg_size(ahd) * scb->sg_count, op);
}

static void
ahd_sync_sense(struct ahd_softc *ahd, struct scb *scb, int op)
{
	ahd_dmamap_sync(ahd, ahd->scb_data.sense_dmat,
			scb->sense_map->dmamap,
			/*offset*/scb->sense_busaddr,
			/*len*/AHD_SENSE_BUFSIZE, op);
}

#ifdef AHD_TARGET_MODE
static uint32_t
ahd_targetcmd_offset(struct ahd_softc *ahd, u_int index)
{
	return (((uint8_t *)&ahd->targetcmds[index])
	       - (uint8_t *)ahd->qoutfifo);
}
#endif

/*********************** Miscelaneous Support Functions ***********************/
/*
 * Return pointers to the transfer negotiation information
 * for the specified our_id/remote_id pair.
 */
struct ahd_initiator_tinfo *
ahd_fetch_transinfo(struct ahd_softc *ahd, char channel, u_int our_id,
		    u_int remote_id, struct ahd_tmode_tstate **tstate)
{
	/*
	 * Transfer data structures are stored from the perspective
	 * of the target role.  Since the parameters for a connection
	 * in the initiator role to a given target are the same as
	 * when the roles are reversed, we pretend we are the target.
	 */
	if (channel == 'B')
		our_id += 8;
	*tstate = ahd->enabled_targets[our_id];
	return (&(*tstate)->transinfo[remote_id]);
}

uint16_t
ahd_inw(struct ahd_softc *ahd, u_int port)
{
	/*
	 * Read high byte first as some registers increment
	 * or have other side effects when the low byte is
	 * read.
	 */
	uint16_t r = ahd_inb(ahd, port+1) << 8;
	return r | ahd_inb(ahd, port);
}

void
ahd_outw(struct ahd_softc *ahd, u_int port, u_int value)
{
	/*
	 * Write low byte first to accomodate registers
	 * such as PRGMCNT where the order maters.
	 */
	ahd_outb(ahd, port, value & 0xFF);
	ahd_outb(ahd, port+1, (value >> 8) & 0xFF);
}

uint32_t
ahd_inl(struct ahd_softc *ahd, u_int port)
{
	return ((ahd_inb(ahd, port))
	      | (ahd_inb(ahd, port+1) << 8)
	      | (ahd_inb(ahd, port+2) << 16)
	      | (ahd_inb(ahd, port+3) << 24));
}

void
ahd_outl(struct ahd_softc *ahd, u_int port, uint32_t value)
{
	ahd_outb(ahd, port, (value) & 0xFF);
	ahd_outb(ahd, port+1, ((value) >> 8) & 0xFF);
	ahd_outb(ahd, port+2, ((value) >> 16) & 0xFF);
	ahd_outb(ahd, port+3, ((value) >> 24) & 0xFF);
}

uint64_t
ahd_inq(struct ahd_softc *ahd, u_int port)
{
	return ((ahd_inb(ahd, port))
	      | (ahd_inb(ahd, port+1) << 8)
	      | (ahd_inb(ahd, port+2) << 16)
	      | (ahd_inb(ahd, port+3) << 24)
	      | (((uint64_t)ahd_inb(ahd, port+4)) << 32)
	      | (((uint64_t)ahd_inb(ahd, port+5)) << 40)
	      | (((uint64_t)ahd_inb(ahd, port+6)) << 48)
	      | (((uint64_t)ahd_inb(ahd, port+7)) << 56));
}

void
ahd_outq(struct ahd_softc *ahd, u_int port, uint64_t value)
{
	ahd_outb(ahd, port, value & 0xFF);
	ahd_outb(ahd, port+1, (value >> 8) & 0xFF);
	ahd_outb(ahd, port+2, (value >> 16) & 0xFF);
	ahd_outb(ahd, port+3, (value >> 24) & 0xFF);
	ahd_outb(ahd, port+4, (value >> 32) & 0xFF);
	ahd_outb(ahd, port+5, (value >> 40) & 0xFF);
	ahd_outb(ahd, port+6, (value >> 48) & 0xFF);
	ahd_outb(ahd, port+7, (value >> 56) & 0xFF);
}

u_int
ahd_get_scbptr(struct ahd_softc *ahd)
{
	AHD_ASSERT_MODES(ahd, ~(AHD_MODE_UNKNOWN_MSK|AHD_MODE_CFG_MSK),
			 ~(AHD_MODE_UNKNOWN_MSK|AHD_MODE_CFG_MSK));
	return (ahd_inb(ahd, SCBPTR) | (ahd_inb(ahd, SCBPTR + 1) << 8));
}

void
ahd_set_scbptr(struct ahd_softc *ahd, u_int scbptr)
{
	AHD_ASSERT_MODES(ahd, ~(AHD_MODE_UNKNOWN_MSK|AHD_MODE_CFG_MSK),
			 ~(AHD_MODE_UNKNOWN_MSK|AHD_MODE_CFG_MSK));
	ahd_outb(ahd, SCBPTR, scbptr & 0xFF);
	ahd_outb(ahd, SCBPTR+1, (scbptr >> 8) & 0xFF);
}

#if 0 /* unused */
static u_int
ahd_get_hnscb_qoff(struct ahd_softc *ahd)
{
	return (ahd_inw_atomic(ahd, HNSCB_QOFF));
}
#endif

static void
ahd_set_hnscb_qoff(struct ahd_softc *ahd, u_int value)
{
	ahd_outw_atomic(ahd, HNSCB_QOFF, value);
}

#if 0 /* unused */
static u_int
ahd_get_hescb_qoff(struct ahd_softc *ahd)
{
	return (ahd_inb(ahd, HESCB_QOFF));
}
#endif

static void
ahd_set_hescb_qoff(struct ahd_softc *ahd, u_int value)
{
	ahd_outb(ahd, HESCB_QOFF, value);
}

static u_int
ahd_get_snscb_qoff(struct ahd_softc *ahd)
{
	u_int oldvalue;

	AHD_ASSERT_MODES(ahd, AHD_MODE_CCHAN_MSK, AHD_MODE_CCHAN_MSK);
	oldvalue = ahd_inw(ahd, SNSCB_QOFF);
	ahd_outw(ahd, SNSCB_QOFF, oldvalue);
	return (oldvalue);
}

static void
ahd_set_snscb_qoff(struct ahd_softc *ahd, u_int value)
{
	AHD_ASSERT_MODES(ahd, AHD_MODE_CCHAN_MSK, AHD_MODE_CCHAN_MSK);
	ahd_outw(ahd, SNSCB_QOFF, value);
}

#if 0 /* unused */
static u_int
ahd_get_sescb_qoff(struct ahd_softc *ahd)
{
	AHD_ASSERT_MODES(ahd, AHD_MODE_CCHAN_MSK, AHD_MODE_CCHAN_MSK);
	return (ahd_inb(ahd, SESCB_QOFF));
}
#endif

static void
ahd_set_sescb_qoff(struct ahd_softc *ahd, u_int value)
{
	AHD_ASSERT_MODES(ahd, AHD_MODE_CCHAN_MSK, AHD_MODE_CCHAN_MSK);
	ahd_outb(ahd, SESCB_QOFF, value);
}

#if 0 /* unused */
static u_int
ahd_get_sdscb_qoff(struct ahd_softc *ahd)
{
	AHD_ASSERT_MODES(ahd, AHD_MODE_CCHAN_MSK, AHD_MODE_CCHAN_MSK);
	return (ahd_inb(ahd, SDSCB_QOFF) | (ahd_inb(ahd, SDSCB_QOFF + 1) << 8));
}
#endif

static void
ahd_set_sdscb_qoff(struct ahd_softc *ahd, u_int value)
{
	AHD_ASSERT_MODES(ahd, AHD_MODE_CCHAN_MSK, AHD_MODE_CCHAN_MSK);
	ahd_outb(ahd, SDSCB_QOFF, value & 0xFF);
	ahd_outb(ahd, SDSCB_QOFF+1, (value >> 8) & 0xFF);
}

u_int
ahd_inb_scbram(struct ahd_softc *ahd, u_int offset)
{
	u_int value;

	/*
	 * Workaround PCI-X Rev A. hardware bug.
	 * After a host read of SCB memory, the chip
	 * may become confused into thinking prefetch
	 * was required.  This starts the discard timer
	 * running and can cause an unexpected discard
	 * timer interrupt.  The work around is to read
	 * a normal register prior to the exhaustion of
	 * the discard timer.  The mode pointer register
	 * has no side effects and so serves well for
	 * this purpose.
	 *
	 * Razor #528
	 */
	value = ahd_inb(ahd, offset);
	if ((ahd->bugs & AHD_PCIX_SCBRAM_RD_BUG) != 0)
		ahd_inb(ahd, MODE_PTR);
	return (value);
}

u_int
ahd_inw_scbram(struct ahd_softc *ahd, u_int offset)
{
	return (ahd_inb_scbram(ahd, offset)
	      | (ahd_inb_scbram(ahd, offset+1) << 8));
}

static uint32_t
ahd_inl_scbram(struct ahd_softc *ahd, u_int offset)
{
	return (ahd_inw_scbram(ahd, offset)
	      | (ahd_inw_scbram(ahd, offset+2) << 16));
}

static uint64_t
ahd_inq_scbram(struct ahd_softc *ahd, u_int offset)
{
	return (ahd_inl_scbram(ahd, offset)
	      | ((uint64_t)ahd_inl_scbram(ahd, offset+4)) << 32);
}

struct scb *
ahd_lookup_scb(struct ahd_softc *ahd, u_int tag)
{
	struct scb* scb;

	if (tag >= AHD_SCB_MAX)
		return (NULL);
	scb = ahd->scb_data.scbindex[tag];
	if (scb != NULL)
		ahd_sync_scb(ahd, scb,
			     BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	return (scb);
}

static void
ahd_swap_with_next_hscb(struct ahd_softc *ahd, struct scb *scb)
{
	struct	 hardware_scb *q_hscb;
	struct	 map_node *q_hscb_map;
	uint32_t saved_hscb_busaddr;

	/*
	 * Our queuing method is a bit tricky.  The card
	 * knows in advance which HSCB (by address) to download,
	 * and we can't disappoint it.  To achieve this, the next
	 * HSCB to download is saved off in ahd->next_queued_hscb.
	 * When we are called to queue "an arbitrary scb",
	 * we copy the contents of the incoming HSCB to the one
	 * the sequencer knows about, swap HSCB pointers and
	 * finally assign the SCB to the tag indexed location
	 * in the scb_array.  This makes sure that we can still
	 * locate the correct SCB by SCB_TAG.
	 */
	q_hscb = ahd->next_queued_hscb;
	q_hscb_map = ahd->next_queued_hscb_map;
	saved_hscb_busaddr = q_hscb->hscb_busaddr;
	memcpy(q_hscb, scb->hscb, sizeof(*scb->hscb));
	q_hscb->hscb_busaddr = saved_hscb_busaddr;
	q_hscb->next_hscb_busaddr = scb->hscb->hscb_busaddr;

	/* Now swap HSCB pointers. */
	ahd->next_queued_hscb = scb->hscb;
	ahd->next_queued_hscb_map = scb->hscb_map;
	scb->hscb = q_hscb;
	scb->hscb_map = q_hscb_map;

	/* Now define the mapping from tag to SCB in the scbindex */
	ahd->scb_data.scbindex[SCB_GET_TAG(scb)] = scb;
}

/*
 * Tell the sequencer about a new transaction to execute.
 */
void
ahd_queue_scb(struct ahd_softc *ahd, struct scb *scb)
{
	ahd_swap_with_next_hscb(ahd, scb);

	if (SCBID_IS_NULL(SCB_GET_TAG(scb)))
		panic("Attempt to queue invalid SCB tag %x\n",
		      SCB_GET_TAG(scb));

	/*
	 * Keep a history of SCBs we've downloaded in the qinfifo.
	 */
	ahd->qinfifo[AHD_QIN_WRAP(ahd->qinfifonext)] = SCB_GET_TAG(scb);
	ahd->qinfifonext++;

	if (scb->sg_count != 0)
		ahd_setup_data_scb(ahd, scb);
	else
		ahd_setup_noxfer_scb(ahd, scb);
	ahd_setup_scb_common(ahd, scb);

	/*
	 * Make sure our data is consistent from the
	 * perspective of the adapter.
	 */
	ahd_sync_scb(ahd, scb, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

#ifdef AHD_DEBUG
	if ((ahd_debug & AHD_SHOW_QUEUE) != 0) {
		uint64_t host_dataptr;

		host_dataptr = ahd_le64toh(scb->hscb->dataptr);
		printf("%s: Queueing SCB %d:0x%x bus addr 0x%x - 0x%x%x/0x%x\n",
		       ahd_name(ahd),
		       SCB_GET_TAG(scb), scb->hscb->scsiid,
		       ahd_le32toh(scb->hscb->hscb_busaddr),
		       (u_int)((host_dataptr >> 32) & 0xFFFFFFFF),
		       (u_int)(host_dataptr & 0xFFFFFFFF),
		       ahd_le32toh(scb->hscb->datacnt));
	}
#endif
	/* Tell the adapter about the newly queued SCB */
	ahd_set_hnscb_qoff(ahd, ahd->qinfifonext);
}

/************************** Interrupt Processing ******************************/
static void
ahd_sync_qoutfifo(struct ahd_softc *ahd, int op)
{
	ahd_dmamap_sync(ahd, ahd->shared_data_dmat, ahd->shared_data_map.dmamap,
			/*offset*/0,
			/*len*/AHD_SCB_MAX * sizeof(struct ahd_completion), op);
}

static void
ahd_sync_tqinfifo(struct ahd_softc *ahd, int op)
{
#ifdef AHD_TARGET_MODE
	if ((ahd->flags & AHD_TARGETROLE) != 0) {
		ahd_dmamap_sync(ahd, ahd->shared_data_dmat,
				ahd->shared_data_map.dmamap,
				ahd_targetcmd_offset(ahd, 0),
				sizeof(struct target_cmd) * AHD_TMODE_CMDS,
				op);
	}
#endif
}

/*
 * See if the firmware has posted any completed commands
 * into our in-core command complete fifos.
 */
#define AHD_RUN_QOUTFIFO 0x1
#define AHD_RUN_TQINFIFO 0x2
static u_int
ahd_check_cmdcmpltqueues(struct ahd_softc *ahd)
{
	u_int retval;

	retval = 0;
	ahd_dmamap_sync(ahd, ahd->shared_data_dmat, ahd->shared_data_map.dmamap,
			/*offset*/ahd->qoutfifonext * sizeof(*ahd->qoutfifo),
			/*len*/sizeof(*ahd->qoutfifo), BUS_DMASYNC_POSTREAD);
	if (ahd->qoutfifo[ahd->qoutfifonext].valid_tag
	  == ahd->qoutfifonext_valid_tag)
		retval |= AHD_RUN_QOUTFIFO;
#ifdef AHD_TARGET_MODE
	if ((ahd->flags & AHD_TARGETROLE) != 0
	 && (ahd->flags & AHD_TQINFIFO_BLOCKED) == 0) {
		ahd_dmamap_sync(ahd, ahd->shared_data_dmat,
				ahd->shared_data_map.dmamap,
				ahd_targetcmd_offset(ahd, ahd->tqinfifofnext),
				/*len*/sizeof(struct target_cmd),
				BUS_DMASYNC_POSTREAD);
		if (ahd->targetcmds[ahd->tqinfifonext].cmd_valid != 0)
			retval |= AHD_RUN_TQINFIFO;
	}
#endif
	return (retval);
}

/*
 * Catch an interrupt from the adapter
 */
int
ahd_intr(struct ahd_softc *ahd)
{
	u_int	intstat;

	if ((ahd->pause & INTEN) == 0) {
		/*
		 * Our interrupt is not enabled on the chip
		 * and may be disabled for re-entrancy reasons,
		 * so just return.  This is likely just a shared
		 * interrupt.
		 */
		return (0);
	}

	/*
	 * Instead of directly reading the interrupt status register,
	 * infer the cause of the interrupt by checking our in-core
	 * completion queues.  This avoids a costly PCI bus read in
	 * most cases.
	 */
	if ((ahd->flags & AHD_ALL_INTERRUPTS) == 0
	 && (ahd_check_cmdcmpltqueues(ahd) != 0))
		intstat = CMDCMPLT;
	else
		intstat = ahd_inb(ahd, INTSTAT);

	if ((intstat & INT_PEND) == 0)
		return (0);

	if (intstat & CMDCMPLT) {
		ahd_outb(ahd, CLRINT, CLRCMDINT);

		/*
		 * Ensure that the chip sees that we've cleared
		 * this interrupt before we walk the output fifo.
		 * Otherwise, we may, due to posted bus writes,
		 * clear the interrupt after we finish the scan,
		 * and after the sequencer has added new entries
		 * and asserted the interrupt again.
		 */
		if ((ahd->bugs & AHD_INTCOLLISION_BUG) != 0) {
			if (ahd_is_paused(ahd)) {
				/*
				 * Potentially lost SEQINT.
				 * If SEQINTCODE is non-zero,
				 * simulate the SEQINT.
				 */
				if (ahd_inb(ahd, SEQINTCODE) != NO_SEQINT)
					intstat |= SEQINT;
			}
		} else {
			ahd_flush_device_writes(ahd);
		}
		ahd_run_qoutfifo(ahd);
		ahd->cmdcmplt_counts[ahd->cmdcmplt_bucket]++;
		ahd->cmdcmplt_total++;
#ifdef AHD_TARGET_MODE
		if ((ahd->flags & AHD_TARGETROLE) != 0)
			ahd_run_tqinfifo(ahd, /*paused*/FALSE);
#endif
	}

	/*
	 * Handle statuses that may invalidate our cached
	 * copy of INTSTAT separately.
	 */
	if (intstat == 0xFF && (ahd->features & AHD_REMOVABLE) != 0) {
		/* Hot eject.  Do nothing */
	} else if (intstat & HWERRINT) {
		ahd_handle_hwerrint(ahd);
	} else if ((intstat & (PCIINT|SPLTINT)) != 0) {
		ahd->bus_intr(ahd);
	} else {

		if ((intstat & SEQINT) != 0)
			ahd_handle_seqint(ahd, intstat);

		if ((intstat & SCSIINT) != 0)
			ahd_handle_scsiint(ahd, intstat);
	}
	return (1);
}

/******************************** Private Inlines *****************************/
static inline void
ahd_assert_atn(struct ahd_softc *ahd)
{
	ahd_outb(ahd, SCSISIGO, ATNO);
}

/*
 * Determine if the current connection has a packetized
 * agreement.  This does not necessarily mean that we
 * are currently in a packetized transfer.  We could
 * just as easily be sending or receiving a message.
 */
static int
ahd_currently_packetized(struct ahd_softc *ahd)
{
	ahd_mode_state	 saved_modes;
	int		 packetized;

	saved_modes = ahd_save_modes(ahd);
	if ((ahd->bugs & AHD_PKTIZED_STATUS_BUG) != 0) {
		/*
		 * The packetized bit refers to the last
		 * connection, not the current one.  Check
		 * for non-zero LQISTATE instead.
		 */
		ahd_set_modes(ahd, AHD_MODE_CFG, AHD_MODE_CFG);
		packetized = ahd_inb(ahd, LQISTATE) != 0;
	} else {
		ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
		packetized = ahd_inb(ahd, LQISTAT2) & PACKETIZED;
	}
	ahd_restore_modes(ahd, saved_modes);
	return (packetized);
}

static inline int
ahd_set_active_fifo(struct ahd_softc *ahd)
{
	u_int active_fifo;

	AHD_ASSERT_MODES(ahd, AHD_MODE_SCSI_MSK, AHD_MODE_SCSI_MSK);
	active_fifo = ahd_inb(ahd, DFFSTAT) & CURRFIFO;
	switch (active_fifo) {
	case 0:
	case 1:
		ahd_set_modes(ahd, active_fifo, active_fifo);
		return (1);
	default:
		return (0);
	}
}

static inline void
ahd_unbusy_tcl(struct ahd_softc *ahd, u_int tcl)
{
	ahd_busy_tcl(ahd, tcl, SCB_LIST_NULL);
}

/*
 * Determine whether the sequencer reported a residual
 * for this SCB/transaction.
 */
static inline void
ahd_update_residual(struct ahd_softc *ahd, struct scb *scb)
{
	uint32_t sgptr;

	sgptr = ahd_le32toh(scb->hscb->sgptr);
	if ((sgptr & SG_STATUS_VALID) != 0)
		ahd_calc_residual(ahd, scb);
}

static inline void
ahd_complete_scb(struct ahd_softc *ahd, struct scb *scb)
{
	uint32_t sgptr;

	sgptr = ahd_le32toh(scb->hscb->sgptr);
	if ((sgptr & SG_STATUS_VALID) != 0)
		ahd_handle_scb_status(ahd, scb);
	else
		ahd_done(ahd, scb);
}


/************************* Sequencer Execution Control ************************/
/*
 * Restart the sequencer program from address zero
 */
static void
ahd_restart(struct ahd_softc *ahd)
{

	ahd_pause(ahd);

	ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);

	/* No more pending messages */
	ahd_clear_msg_state(ahd);
	ahd_outb(ahd, SCSISIGO, 0);		/* De-assert BSY */
	ahd_outb(ahd, MSG_OUT, MSG_NOOP);	/* No message to send */
	ahd_outb(ahd, SXFRCTL1, ahd_inb(ahd, SXFRCTL1) & ~BITBUCKET);
	ahd_outb(ahd, SEQINTCTL, 0);
	ahd_outb(ahd, LASTPHASE, P_BUSFREE);
	ahd_outb(ahd, SEQ_FLAGS, 0);
	ahd_outb(ahd, SAVED_SCSIID, 0xFF);
	ahd_outb(ahd, SAVED_LUN, 0xFF);

	/*
	 * Ensure that the sequencer's idea of TQINPOS
	 * matches our own.  The sequencer increments TQINPOS
	 * only after it sees a DMA complete and a reset could
	 * occur before the increment leaving the kernel to believe
	 * the command arrived but the sequencer to not.
	 */
	ahd_outb(ahd, TQINPOS, ahd->tqinfifonext);

	/* Always allow reselection */
	ahd_outb(ahd, SCSISEQ1,
		 ahd_inb(ahd, SCSISEQ_TEMPLATE) & (ENSELI|ENRSELI|ENAUTOATNP));
	ahd_set_modes(ahd, AHD_MODE_CCHAN, AHD_MODE_CCHAN);

	/*
	 * Clear any pending sequencer interrupt.  It is no
	 * longer relevant since we're resetting the Program
	 * Counter.
	 */
	ahd_outb(ahd, CLRINT, CLRSEQINT);

	ahd_outb(ahd, SEQCTL0, FASTMODE|SEQRESET);
	ahd_unpause(ahd);
}

static void
ahd_clear_fifo(struct ahd_softc *ahd, u_int fifo)
{
	ahd_mode_state	 saved_modes;

#ifdef AHD_DEBUG
	if ((ahd_debug & AHD_SHOW_FIFOS) != 0)
		printf("%s: Clearing FIFO %d\n", ahd_name(ahd), fifo);
#endif
	saved_modes = ahd_save_modes(ahd);
	ahd_set_modes(ahd, fifo, fifo);
	ahd_outb(ahd, DFFSXFRCTL, RSTCHN|CLRSHCNT);
	if ((ahd_inb(ahd, SG_STATE) & FETCH_INPROG) != 0)
		ahd_outb(ahd, CCSGCTL, CCSGRESET);
	ahd_outb(ahd, LONGJMP_ADDR + 1, INVALID_ADDR);
	ahd_outb(ahd, SG_STATE, 0);
	ahd_restore_modes(ahd, saved_modes);
}

/************************* Input/Output Queues ********************************/
/*
 * Flush and completed commands that are sitting in the command
 * complete queues down on the chip but have yet to be dma'ed back up.
 */
static void
ahd_flush_qoutfifo(struct ahd_softc *ahd)
{
	struct		scb *scb;
	ahd_mode_state	saved_modes;
	u_int		saved_scbptr;
	u_int		ccscbctl;
	u_int		scbid;
	u_int		next_scbid;

	saved_modes = ahd_save_modes(ahd);

	/*
	 * Flush the good status FIFO for completed packetized commands.
	 */
	ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
	saved_scbptr = ahd_get_scbptr(ahd);
	while ((ahd_inb(ahd, LQISTAT2) & LQIGSAVAIL) != 0) {
		u_int fifo_mode;
		u_int i;
		
		scbid = ahd_inw(ahd, GSFIFO);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb == NULL) {
			printf("%s: Warning - GSFIFO SCB %d invalid\n",
			       ahd_name(ahd), scbid);
			continue;
		}
		/*
		 * Determine if this transaction is still active in
		 * any FIFO.  If it is, we must flush that FIFO to
		 * the host before completing the  command.
		 */
		fifo_mode = 0;
rescan_fifos:
		for (i = 0; i < 2; i++) {
			/* Toggle to the other mode. */
			fifo_mode ^= 1;
			ahd_set_modes(ahd, fifo_mode, fifo_mode);

			if (ahd_scb_active_in_fifo(ahd, scb) == 0)
				continue;

			ahd_run_data_fifo(ahd, scb);

			/*
			 * Running this FIFO may cause a CFG4DATA for
			 * this same transaction to assert in the other
			 * FIFO or a new snapshot SAVEPTRS interrupt
			 * in this FIFO.  Even running a FIFO may not
			 * clear the transaction if we are still waiting
			 * for data to drain to the host. We must loop
			 * until the transaction is not active in either
			 * FIFO just to be sure.  Reset our loop counter
			 * so we will visit both FIFOs again before
			 * declaring this transaction finished.  We
			 * also delay a bit so that status has a chance
			 * to change before we look at this FIFO again.
			 */
			ahd_delay(200);
			goto rescan_fifos;
		}
		ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
		ahd_set_scbptr(ahd, scbid);
		if ((ahd_inb_scbram(ahd, SCB_SGPTR) & SG_LIST_NULL) == 0
		 && ((ahd_inb_scbram(ahd, SCB_SGPTR) & SG_FULL_RESID) != 0
		  || (ahd_inb_scbram(ahd, SCB_RESIDUAL_SGPTR)
		      & SG_LIST_NULL) != 0)) {
			u_int comp_head;

			/*
			 * The transfer completed with a residual.
			 * Place this SCB on the complete DMA list
			 * so that we update our in-core copy of the
			 * SCB before completing the command.
			 */
			ahd_outb(ahd, SCB_SCSI_STATUS, 0);
			ahd_outb(ahd, SCB_SGPTR,
				 ahd_inb_scbram(ahd, SCB_SGPTR)
				 | SG_STATUS_VALID);
			ahd_outw(ahd, SCB_TAG, scbid);
			ahd_outw(ahd, SCB_NEXT_COMPLETE, SCB_LIST_NULL);
			comp_head = ahd_inw(ahd, COMPLETE_DMA_SCB_HEAD);
			if (SCBID_IS_NULL(comp_head)) {
				ahd_outw(ahd, COMPLETE_DMA_SCB_HEAD, scbid);
				ahd_outw(ahd, COMPLETE_DMA_SCB_TAIL, scbid);
			} else {
				u_int tail;

				tail = ahd_inw(ahd, COMPLETE_DMA_SCB_TAIL);
				ahd_set_scbptr(ahd, tail);
				ahd_outw(ahd, SCB_NEXT_COMPLETE, scbid);
				ahd_outw(ahd, COMPLETE_DMA_SCB_TAIL, scbid);
				ahd_set_scbptr(ahd, scbid);
			}
		} else
			ahd_complete_scb(ahd, scb);
	}
	ahd_set_scbptr(ahd, saved_scbptr);

	/*
	 * Setup for command channel portion of flush.
	 */
	ahd_set_modes(ahd, AHD_MODE_CCHAN, AHD_MODE_CCHAN);

	/*
	 * Wait for any inprogress DMA to complete and clear DMA state
	 * if this if for an SCB in the qinfifo.
	 */
	while (((ccscbctl = ahd_inb(ahd, CCSCBCTL)) & (CCARREN|CCSCBEN)) != 0) {

		if ((ccscbctl & (CCSCBDIR|CCARREN)) == (CCSCBDIR|CCARREN)) {
			if ((ccscbctl & ARRDONE) != 0)
				break;
		} else if ((ccscbctl & CCSCBDONE) != 0)
			break;
		ahd_delay(200);
	}
	/*
	 * We leave the sequencer to cleanup in the case of DMA's to
	 * update the qoutfifo.  In all other cases (DMA's to the
	 * chip or a push of an SCB from the COMPLETE_DMA_SCB list),
	 * we disable the DMA engine so that the sequencer will not
	 * attempt to handle the DMA completion.
	 */
	if ((ccscbctl & CCSCBDIR) != 0 || (ccscbctl & ARRDONE) != 0)
		ahd_outb(ahd, CCSCBCTL, ccscbctl & ~(CCARREN|CCSCBEN));

	/*
	 * Complete any SCBs that just finished
	 * being DMA'ed into the qoutfifo.
	 */
	ahd_run_qoutfifo(ahd);

	saved_scbptr = ahd_get_scbptr(ahd);
	/*
	 * Manually update/complete any completed SCBs that are waiting to be
	 * DMA'ed back up to the host.
	 */
	scbid = ahd_inw(ahd, COMPLETE_DMA_SCB_HEAD);
	while (!SCBID_IS_NULL(scbid)) {
		uint8_t *hscb_ptr;
		u_int	 i;
		
		ahd_set_scbptr(ahd, scbid);
		next_scbid = ahd_inw_scbram(ahd, SCB_NEXT_COMPLETE);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb == NULL) {
			printf("%s: Warning - DMA-up and complete "
			       "SCB %d invalid\n", ahd_name(ahd), scbid);
			continue;
		}
		hscb_ptr = (uint8_t *)scb->hscb;
		for (i = 0; i < sizeof(struct hardware_scb); i++)
			*hscb_ptr++ = ahd_inb_scbram(ahd, SCB_BASE + i);

		ahd_complete_scb(ahd, scb);
		scbid = next_scbid;
	}
	ahd_outw(ahd, COMPLETE_DMA_SCB_HEAD, SCB_LIST_NULL);
	ahd_outw(ahd, COMPLETE_DMA_SCB_TAIL, SCB_LIST_NULL);

	scbid = ahd_inw(ahd, COMPLETE_ON_QFREEZE_HEAD);
	while (!SCBID_IS_NULL(scbid)) {

		ahd_set_scbptr(ahd, scbid);
		next_scbid = ahd_inw_scbram(ahd, SCB_NEXT_COMPLETE);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb == NULL) {
			printf("%s: Warning - Complete Qfrz SCB %d invalid\n",
			       ahd_name(ahd), scbid);
			continue;
		}

		ahd_complete_scb(ahd, scb);
		scbid = next_scbid;
	}
	ahd_outw(ahd, COMPLETE_ON_QFREEZE_HEAD, SCB_LIST_NULL);

	scbid = ahd_inw(ahd, COMPLETE_SCB_HEAD);
	while (!SCBID_IS_NULL(scbid)) {

		ahd_set_scbptr(ahd, scbid);
		next_scbid = ahd_inw_scbram(ahd, SCB_NEXT_COMPLETE);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb == NULL) {
			printf("%s: Warning - Complete SCB %d invalid\n",
			       ahd_name(ahd), scbid);
			continue;
		}

		ahd_complete_scb(ahd, scb);
		scbid = next_scbid;
	}
	ahd_outw(ahd, COMPLETE_SCB_HEAD, SCB_LIST_NULL);

	/*
	 * Restore state.
	 */
	ahd_set_scbptr(ahd, saved_scbptr);
	ahd_restore_modes(ahd, saved_modes);
	ahd->flags |= AHD_UPDATE_PEND_CMDS;
}

/*
 * Determine if an SCB for a packetized transaction
 * is active in a FIFO.
 */
static int
ahd_scb_active_in_fifo(struct ahd_softc *ahd, struct scb *scb)
{

	/*
	 * The FIFO is only active for our transaction if
	 * the SCBPTR matches the SCB's ID and the firmware
	 * has installed a handler for the FIFO or we have
	 * a pending SAVEPTRS or CFG4DATA interrupt.
	 */
	if (ahd_get_scbptr(ahd) != SCB_GET_TAG(scb)
	 || ((ahd_inb(ahd, LONGJMP_ADDR+1) & INVALID_ADDR) != 0
	  && (ahd_inb(ahd, SEQINTSRC) & (CFG4DATA|SAVEPTRS)) == 0))
		return (0);

	return (1);
}

/*
 * Run a data fifo to completion for a transaction we know
 * has completed across the SCSI bus (good status has been
 * received).  We are already set to the correct FIFO mode
 * on entry to this routine.
 *
 * This function attempts to operate exactly as the firmware
 * would when running this FIFO.  Care must be taken to update
 * this routine any time the firmware's FIFO algorithm is
 * changed.
 */
static void
ahd_run_data_fifo(struct ahd_softc *ahd, struct scb *scb)
{
	u_int seqintsrc;

	seqintsrc = ahd_inb(ahd, SEQINTSRC);
	if ((seqintsrc & CFG4DATA) != 0) {
		uint32_t datacnt;
		uint32_t sgptr;

		/*
		 * Clear full residual flag.
		 */
		sgptr = ahd_inl_scbram(ahd, SCB_SGPTR) & ~SG_FULL_RESID;
		ahd_outb(ahd, SCB_SGPTR, sgptr);

		/*
		 * Load datacnt and address.
		 */
		datacnt = ahd_inl_scbram(ahd, SCB_DATACNT);
		if ((datacnt & AHD_DMA_LAST_SEG) != 0) {
			sgptr |= LAST_SEG;
			ahd_outb(ahd, SG_STATE, 0);
		} else
			ahd_outb(ahd, SG_STATE, LOADING_NEEDED);
		ahd_outq(ahd, HADDR, ahd_inq_scbram(ahd, SCB_DATAPTR));
		ahd_outl(ahd, HCNT, datacnt & AHD_SG_LEN_MASK);
		ahd_outb(ahd, SG_CACHE_PRE, sgptr);
		ahd_outb(ahd, DFCNTRL, PRELOADEN|SCSIEN|HDMAEN);

		/*
		 * Initialize Residual Fields.
		 */
		ahd_outb(ahd, SCB_RESIDUAL_DATACNT+3, datacnt >> 24);
		ahd_outl(ahd, SCB_RESIDUAL_SGPTR, sgptr & SG_PTR_MASK);

		/*
		 * Mark the SCB as having a FIFO in use.
		 */
		ahd_outb(ahd, SCB_FIFO_USE_COUNT,
			 ahd_inb_scbram(ahd, SCB_FIFO_USE_COUNT) + 1);

		/*
		 * Install a "fake" handler for this FIFO.
		 */
		ahd_outw(ahd, LONGJMP_ADDR, 0);

		/*
		 * Notify the hardware that we have satisfied
		 * this sequencer interrupt.
		 */
		ahd_outb(ahd, CLRSEQINTSRC, CLRCFG4DATA);
	} else if ((seqintsrc & SAVEPTRS) != 0) {
		uint32_t sgptr;
		uint32_t resid;

		if ((ahd_inb(ahd, LONGJMP_ADDR+1)&INVALID_ADDR) != 0) {
			/*
			 * Snapshot Save Pointers.  All that
			 * is necessary to clear the snapshot
			 * is a CLRCHN.
			 */
			goto clrchn;
		}

		/*
		 * Disable S/G fetch so the DMA engine
		 * is available to future users.
		 */
		if ((ahd_inb(ahd, SG_STATE) & FETCH_INPROG) != 0)
			ahd_outb(ahd, CCSGCTL, 0);
		ahd_outb(ahd, SG_STATE, 0);

		/*
		 * Flush the data FIFO.  Strickly only
		 * necessary for Rev A parts.
		 */
		ahd_outb(ahd, DFCNTRL, ahd_inb(ahd, DFCNTRL) | FIFOFLUSH);

		/*
		 * Calculate residual.
		 */
		sgptr = ahd_inl_scbram(ahd, SCB_RESIDUAL_SGPTR);
		resid = ahd_inl(ahd, SHCNT);
		resid |= ahd_inb_scbram(ahd, SCB_RESIDUAL_DATACNT+3) << 24;
		ahd_outl(ahd, SCB_RESIDUAL_DATACNT, resid);
		if ((ahd_inb(ahd, SG_CACHE_SHADOW) & LAST_SEG) == 0) {
			/*
			 * Must back up to the correct S/G element.
			 * Typically this just means resetting our
			 * low byte to the offset in the SG_CACHE,
			 * but if we wrapped, we have to correct
			 * the other bytes of the sgptr too.
			 */
			if ((ahd_inb(ahd, SG_CACHE_SHADOW) & 0x80) != 0
			 && (sgptr & 0x80) == 0)
				sgptr -= 0x100;
			sgptr &= ~0xFF;
			sgptr |= ahd_inb(ahd, SG_CACHE_SHADOW)
			       & SG_ADDR_MASK;
			ahd_outl(ahd, SCB_RESIDUAL_SGPTR, sgptr);
			ahd_outb(ahd, SCB_RESIDUAL_DATACNT + 3, 0);
		} else if ((resid & AHD_SG_LEN_MASK) == 0) {
			ahd_outb(ahd, SCB_RESIDUAL_SGPTR,
				 sgptr | SG_LIST_NULL);
		}
		/*
		 * Save Pointers.
		 */
		ahd_outq(ahd, SCB_DATAPTR, ahd_inq(ahd, SHADDR));
		ahd_outl(ahd, SCB_DATACNT, resid);
		ahd_outl(ahd, SCB_SGPTR, sgptr);
		ahd_outb(ahd, CLRSEQINTSRC, CLRSAVEPTRS);
		ahd_outb(ahd, SEQIMODE,
			 ahd_inb(ahd, SEQIMODE) | ENSAVEPTRS);
		/*
		 * If the data is to the SCSI bus, we are
		 * done, otherwise wait for FIFOEMP.
		 */
		if ((ahd_inb(ahd, DFCNTRL) & DIRECTION) != 0)
			goto clrchn;
	} else if ((ahd_inb(ahd, SG_STATE) & LOADING_NEEDED) != 0) {
		uint32_t sgptr;
		uint64_t data_addr;
		uint32_t data_len;
		u_int	 dfcntrl;

		/*
		 * Disable S/G fetch so the DMA engine
		 * is available to future users.  We won't
		 * be using the DMA engine to load segments.
		 */
		if ((ahd_inb(ahd, SG_STATE) & FETCH_INPROG) != 0) {
			ahd_outb(ahd, CCSGCTL, 0);
			ahd_outb(ahd, SG_STATE, LOADING_NEEDED);
		}

		/*
		 * Wait for the DMA engine to notice that the
		 * host transfer is enabled and that there is
		 * space in the S/G FIFO for new segments before
		 * loading more segments.
		 */
		if ((ahd_inb(ahd, DFSTATUS) & PRELOAD_AVAIL) != 0
		 && (ahd_inb(ahd, DFCNTRL) & HDMAENACK) != 0) {

			/*
			 * Determine the offset of the next S/G
			 * element to load.
			 */
			sgptr = ahd_inl_scbram(ahd, SCB_RESIDUAL_SGPTR);
			sgptr &= SG_PTR_MASK;
			if ((ahd->flags & AHD_64BIT_ADDRESSING) != 0) {
				struct ahd_dma64_seg *sg;

				sg = ahd_sg_bus_to_virt(ahd, scb, sgptr);
				data_addr = sg->addr;
				data_len = sg->len;
				sgptr += sizeof(*sg);
			} else {
				struct	ahd_dma_seg *sg;

				sg = ahd_sg_bus_to_virt(ahd, scb, sgptr);
				data_addr = sg->len & AHD_SG_HIGH_ADDR_MASK;
				data_addr <<= 8;
				data_addr |= sg->addr;
				data_len = sg->len;
				sgptr += sizeof(*sg);
			}

			/*
			 * Update residual information.
			 */
			ahd_outb(ahd, SCB_RESIDUAL_DATACNT+3, data_len >> 24);
			ahd_outl(ahd, SCB_RESIDUAL_SGPTR, sgptr);

			/*
			 * Load the S/G.
			 */
			if (data_len & AHD_DMA_LAST_SEG) {
				sgptr |= LAST_SEG;
				ahd_outb(ahd, SG_STATE, 0);
			}
			ahd_outq(ahd, HADDR, data_addr);
			ahd_outl(ahd, HCNT, data_len & AHD_SG_LEN_MASK);
			ahd_outb(ahd, SG_CACHE_PRE, sgptr & 0xFF);

			/*
			 * Advertise the segment to the hardware.
			 */
			dfcntrl = ahd_inb(ahd, DFCNTRL)|PRELOADEN|HDMAEN;
			if ((ahd->features & AHD_NEW_DFCNTRL_OPTS) != 0) {
				/*
				 * Use SCSIENWRDIS so that SCSIEN
				 * is never modified by this
				 * operation.
				 */
				dfcntrl |= SCSIENWRDIS;
			}
			ahd_outb(ahd, DFCNTRL, dfcntrl);
		}
	} else if ((ahd_inb(ahd, SG_CACHE_SHADOW) & LAST_SEG_DONE) != 0) {

		/*
		 * Transfer completed to the end of SG list
		 * and has flushed to the host.
		 */
		ahd_outb(ahd, SCB_SGPTR,
			 ahd_inb_scbram(ahd, SCB_SGPTR) | SG_LIST_NULL);
		goto clrchn;
	} else if ((ahd_inb(ahd, DFSTATUS) & FIFOEMP) != 0) {
clrchn:
		/*
		 * Clear any handler for this FIFO, decrement
		 * the FIFO use count for the SCB, and release
		 * the FIFO.
		 */
		ahd_outb(ahd, LONGJMP_ADDR + 1, INVALID_ADDR);
		ahd_outb(ahd, SCB_FIFO_USE_COUNT,
			 ahd_inb_scbram(ahd, SCB_FIFO_USE_COUNT) - 1);
		ahd_outb(ahd, DFFSXFRCTL, CLRCHN);
	}
}

/*
 * Look for entries in the QoutFIFO that have completed.
 * The valid_tag completion field indicates the validity
 * of the entry - the valid value toggles each time through
 * the queue. We use the sg_status field in the completion
 * entry to avoid referencing the hscb if the completion
 * occurred with no errors and no residual.  sg_status is
 * a copy of the first byte (little endian) of the sgptr
 * hscb field.
 */
static void
ahd_run_qoutfifo(struct ahd_softc *ahd)
{
	struct ahd_completion *completion;
	struct scb *scb;
	u_int  scb_index;

	if ((ahd->flags & AHD_RUNNING_QOUTFIFO) != 0)
		panic("ahd_run_qoutfifo recursion");
	ahd->flags |= AHD_RUNNING_QOUTFIFO;
	ahd_sync_qoutfifo(ahd, BUS_DMASYNC_POSTREAD);
	for (;;) {
		completion = &ahd->qoutfifo[ahd->qoutfifonext];

		if (completion->valid_tag != ahd->qoutfifonext_valid_tag)
			break;

		scb_index = ahd_le16toh(completion->tag);
		scb = ahd_lookup_scb(ahd, scb_index);
		if (scb == NULL) {
			printf("%s: WARNING no command for scb %d "
			       "(cmdcmplt)\nQOUTPOS = %d\n",
			       ahd_name(ahd), scb_index,
			       ahd->qoutfifonext);
			ahd_dump_card_state(ahd);
		} else if ((completion->sg_status & SG_STATUS_VALID) != 0) {
			ahd_handle_scb_status(ahd, scb);
		} else {
			ahd_done(ahd, scb);
		}

		ahd->qoutfifonext = (ahd->qoutfifonext+1) & (AHD_QOUT_SIZE-1);
		if (ahd->qoutfifonext == 0)
			ahd->qoutfifonext_valid_tag ^= QOUTFIFO_ENTRY_VALID;
	}
	ahd->flags &= ~AHD_RUNNING_QOUTFIFO;
}

/************************* Interrupt Handling *********************************/
static void
ahd_handle_hwerrint(struct ahd_softc *ahd)
{
	/*
	 * Some catastrophic hardware error has occurred.
	 * Print it for the user and disable the controller.
	 */
	int i;
	int error;

	error = ahd_inb(ahd, ERROR);
	for (i = 0; i < num_errors; i++) {
		if ((error & ahd_hard_errors[i].errno) != 0)
			printf("%s: hwerrint, %s\n",
			       ahd_name(ahd), ahd_hard_errors[i].errmesg);
	}

	ahd_dump_card_state(ahd);
	panic("BRKADRINT");

	/* Tell everyone that this HBA is no longer available */
	ahd_abort_scbs(ahd, CAM_TARGET_WILDCARD, ALL_CHANNELS,
		       CAM_LUN_WILDCARD, SCB_LIST_NULL, ROLE_UNKNOWN,
		       CAM_NO_HBA);

	/* Tell the system that this controller has gone away. */
	ahd_free(ahd);
}

#ifdef AHD_DEBUG
static void
ahd_dump_sglist(struct scb *scb)
{
	int i;

	if (scb->sg_count > 0) {
		if ((scb->ahd_softc->flags & AHD_64BIT_ADDRESSING) != 0) {
			struct ahd_dma64_seg *sg_list;

			sg_list = (struct ahd_dma64_seg*)scb->sg_list;
			for (i = 0; i < scb->sg_count; i++) {
				uint64_t addr;
				uint32_t len;

				addr = ahd_le64toh(sg_list[i].addr);
				len = ahd_le32toh(sg_list[i].len);
				printf("sg[%d] - Addr 0x%x%x : Length %d%s\n",
				       i,
				       (uint32_t)((addr >> 32) & 0xFFFFFFFF),
				       (uint32_t)(addr & 0xFFFFFFFF),
				       sg_list[i].len & AHD_SG_LEN_MASK,
				       (sg_list[i].len & AHD_DMA_LAST_SEG)
				     ? " Last" : "");
			}
		} else {
			struct ahd_dma_seg *sg_list;

			sg_list = (struct ahd_dma_seg*)scb->sg_list;
			for (i = 0; i < scb->sg_count; i++) {
				uint32_t len;

				len = ahd_le32toh(sg_list[i].len);
				printf("sg[%d] - Addr 0x%x%x : Length %d%s\n",
				       i,
				       (len & AHD_SG_HIGH_ADDR_MASK) >> 24,
				       ahd_le32toh(sg_list[i].addr),
				       len & AHD_SG_LEN_MASK,
				       len & AHD_DMA_LAST_SEG ? " Last" : "");
			}
		}
	}
}
#endif  /*  AHD_DEBUG  */

static void
ahd_handle_seqint(struct ahd_softc *ahd, u_int intstat)
{
	u_int seqintcode;

	/*
	 * Save the sequencer interrupt code and clear the SEQINT
	 * bit. We will unpause the sequencer, if appropriate,
	 * after servicing the request.
	 */
	seqintcode = ahd_inb(ahd, SEQINTCODE);
	ahd_outb(ahd, CLRINT, CLRSEQINT);
	if ((ahd->bugs & AHD_INTCOLLISION_BUG) != 0) {
		/*
		 * Unpause the sequencer and let it clear
		 * SEQINT by writing NO_SEQINT to it.  This
		 * will cause the sequencer to be paused again,
		 * which is the expected state of this routine.
		 */
		ahd_unpause(ahd);
		while (!ahd_is_paused(ahd))
			;
		ahd_outb(ahd, CLRINT, CLRSEQINT);
	}
	ahd_update_modes(ahd);
#ifdef AHD_DEBUG
	if ((ahd_debug & AHD_SHOW_MISC) != 0)
		printf("%s: Handle Seqint Called for code %d\n",
		       ahd_name(ahd), seqintcode);
#endif
	switch (seqintcode) {
	case ENTERING_NONPACK:
	{
		struct	scb *scb;
		u_int	scbid;

		AHD_ASSERT_MODES(ahd, ~(AHD_MODE_UNKNOWN_MSK|AHD_MODE_CFG_MSK),
				 ~(AHD_MODE_UNKNOWN_MSK|AHD_MODE_CFG_MSK));
		scbid = ahd_get_scbptr(ahd);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb == NULL) {
			/*
			 * Somehow need to know if this
			 * is from a selection or reselection.
			 * From that, we can determine target
			 * ID so we at least have an I_T nexus.
			 */
		} else {
			ahd_outb(ahd, SAVED_SCSIID, scb->hscb->scsiid);
			ahd_outb(ahd, SAVED_LUN, scb->hscb->lun);
			ahd_outb(ahd, SEQ_FLAGS, 0x0);
		}
		if ((ahd_inb(ahd, LQISTAT2) & LQIPHASE_OUTPKT) != 0
		 && (ahd_inb(ahd, SCSISIGO) & ATNO) != 0) {
			/*
			 * Phase change after read stream with
			 * CRC error with P0 asserted on last
			 * packet.
			 */
#ifdef AHD_DEBUG
			if ((ahd_debug & AHD_SHOW_RECOVERY) != 0)
				printf("%s: Assuming LQIPHASE_NLQ with "
				       "P0 assertion\n", ahd_name(ahd));
#endif
		}
#ifdef AHD_DEBUG
		if ((ahd_debug & AHD_SHOW_RECOVERY) != 0)
			printf("%s: Entering NONPACK\n", ahd_name(ahd));
#endif
		break;
	}
	case INVALID_SEQINT:
		printf("%s: Invalid Sequencer interrupt occurred, "
		       "resetting channel.\n",
		       ahd_name(ahd));
#ifdef AHD_DEBUG
		if ((ahd_debug & AHD_SHOW_RECOVERY) != 0)
			ahd_dump_card_state(ahd);
#endif
		ahd_reset_channel(ahd, 'A', /*Initiate Reset*/TRUE);
		break;
	case STATUS_OVERRUN:
	{
		struct	scb *scb;
		u_int	scbid;

		scbid = ahd_get_scbptr(ahd);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb != NULL)
			ahd_print_path(ahd, scb);
		else
			printf("%s: ", ahd_name(ahd));
		printf("SCB %d Packetized Status Overrun", scbid);
		ahd_dump_card_state(ahd);
		ahd_reset_channel(ahd, 'A', /*Initiate Reset*/TRUE);
		break;
	}
	case CFG4ISTAT_INTR:
	{
		struct	scb *scb;
		u_int	scbid;

		scbid = ahd_get_scbptr(ahd);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb == NULL) {
			ahd_dump_card_state(ahd);
			printf("CFG4ISTAT: Free SCB %d referenced", scbid);
			panic("For safety");
		}
		ahd_outq(ahd, HADDR, scb->sense_busaddr);
		ahd_outw(ahd, HCNT, AHD_SENSE_BUFSIZE);
		ahd_outb(ahd, HCNT + 2, 0);
		ahd_outb(ahd, SG_CACHE_PRE, SG_LAST_SEG);
		ahd_outb(ahd, DFCNTRL, PRELOADEN|SCSIEN|HDMAEN);
		break;
	}
	case ILLEGAL_PHASE:
	{
		u_int bus_phase;

		bus_phase = ahd_inb(ahd, SCSISIGI) & PHASE_MASK;
		printf("%s: ILLEGAL_PHASE 0x%x\n",
		       ahd_name(ahd), bus_phase);

		switch (bus_phase) {
		case P_DATAOUT:
		case P_DATAIN:
		case P_DATAOUT_DT:
		case P_DATAIN_DT:
		case P_MESGOUT:
		case P_STATUS:
		case P_MESGIN:
			ahd_reset_channel(ahd, 'A', /*Initiate Reset*/TRUE);
			printf("%s: Issued Bus Reset.\n", ahd_name(ahd));
			break;
		case P_COMMAND:
		{
			struct	ahd_devinfo devinfo;
			struct	scb *scb;
			struct	ahd_initiator_tinfo *targ_info;
			struct	ahd_tmode_tstate *tstate;
			struct	ahd_transinfo *tinfo;
			u_int	scbid;

			/*
			 * If a target takes us into the command phase
			 * assume that it has been externally reset and
			 * has thus lost our previous packetized negotiation
			 * agreement.  Since we have not sent an identify
			 * message and may not have fully qualified the
			 * connection, we change our command to TUR, assert
			 * ATN and ABORT the task when we go to message in
			 * phase.  The OSM will see the REQUEUE_REQUEST
			 * status and retry the command.
			 */
			scbid = ahd_get_scbptr(ahd);
			scb = ahd_lookup_scb(ahd, scbid);
			if (scb == NULL) {
				printf("Invalid phase with no valid SCB.  "
				       "Resetting bus.\n");
				ahd_reset_channel(ahd, 'A',
						  /*Initiate Reset*/TRUE);
				break;
			}
			ahd_compile_devinfo(&devinfo, SCB_GET_OUR_ID(scb),
					    SCB_GET_TARGET(ahd, scb),
					    SCB_GET_LUN(scb),
					    SCB_GET_CHANNEL(ahd, scb),
					    ROLE_INITIATOR);
			targ_info = ahd_fetch_transinfo(ahd,
							devinfo.channel,
							devinfo.our_scsiid,
							devinfo.target,
							&tstate);
			tinfo = &targ_info->curr;
			ahd_set_width(ahd, &devinfo, MSG_EXT_WDTR_BUS_8_BIT,
				      AHD_TRANS_ACTIVE, /*paused*/TRUE);
			ahd_set_syncrate(ahd, &devinfo, /*period*/0,
					 /*offset*/0, /*ppr_options*/0,
					 AHD_TRANS_ACTIVE, /*paused*/TRUE);
			/* Hand-craft TUR command */
			ahd_outb(ahd, SCB_CDB_STORE, 0);
			ahd_outb(ahd, SCB_CDB_STORE+1, 0);
			ahd_outb(ahd, SCB_CDB_STORE+2, 0);
			ahd_outb(ahd, SCB_CDB_STORE+3, 0);
			ahd_outb(ahd, SCB_CDB_STORE+4, 0);
			ahd_outb(ahd, SCB_CDB_STORE+5, 0);
			ahd_outb(ahd, SCB_CDB_LEN, 6);
			scb->hscb->control &= ~(TAG_ENB|SCB_TAG_TYPE);
			scb->hscb->control |= MK_MESSAGE;
			ahd_outb(ahd, SCB_CONTROL, scb->hscb->control);
			ahd_outb(ahd, MSG_OUT, HOST_MSG);
			ahd_outb(ahd, SAVED_SCSIID, scb->hscb->scsiid);
			/*
			 * The lun is 0, regardless of the SCB's lun
			 * as we have not sent an identify message.
			 */
			ahd_outb(ahd, SAVED_LUN, 0);
			ahd_outb(ahd, SEQ_FLAGS, 0);
			ahd_assert_atn(ahd);
			scb->flags &= ~SCB_PACKETIZED;
			scb->flags |= SCB_ABORT|SCB_EXTERNAL_RESET;
			ahd_freeze_devq(ahd, scb);
			ahd_set_transaction_status(scb, CAM_REQUEUE_REQ);
			ahd_freeze_scb(scb);

			/* Notify XPT */
			ahd_send_async(ahd, devinfo.channel, devinfo.target,
				       CAM_LUN_WILDCARD, AC_SENT_BDR);

			/*
			 * Allow the sequencer to continue with
			 * non-pack processing.
			 */
			ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
			ahd_outb(ahd, CLRLQOINT1, CLRLQOPHACHGINPKT);
			if ((ahd->bugs & AHD_CLRLQO_AUTOCLR_BUG) != 0) {
				ahd_outb(ahd, CLRLQOINT1, 0);
			}
#ifdef AHD_DEBUG
			if ((ahd_debug & AHD_SHOW_RECOVERY) != 0) {
				ahd_print_path(ahd, scb);
				printf("Unexpected command phase from "
				       "packetized target\n");
			}
#endif
			break;
		}
		}
		break;
	}
	case CFG4OVERRUN:
	{
		struct	scb *scb;
		u_int	scb_index;
		
#ifdef AHD_DEBUG
		if ((ahd_debug & AHD_SHOW_RECOVERY) != 0) {
			printf("%s: CFG4OVERRUN mode = %x\n", ahd_name(ahd),
			       ahd_inb(ahd, MODE_PTR));
		}
#endif
		scb_index = ahd_get_scbptr(ahd);
		scb = ahd_lookup_scb(ahd, scb_index);
		if (scb == NULL) {
			/*
			 * Attempt to transfer to an SCB that is
			 * not outstanding.
			 */
			ahd_assert_atn(ahd);
			ahd_outb(ahd, MSG_OUT, HOST_MSG);
			ahd->msgout_buf[0] = MSG_ABORT_TASK;
			ahd->msgout_len = 1;
			ahd->msgout_index = 0;
			ahd->msg_type = MSG_TYPE_INITIATOR_MSGOUT;
			/*
			 * Clear status received flag to prevent any
			 * attempt to complete this bogus SCB.
			 */
			ahd_outb(ahd, SCB_CONTROL,
				 ahd_inb_scbram(ahd, SCB_CONTROL)
				 & ~STATUS_RCVD);
		}
		break;
	}
	case DUMP_CARD_STATE:
	{
		ahd_dump_card_state(ahd);
		break;
	}
	case PDATA_REINIT:
	{
#ifdef AHD_DEBUG
		if ((ahd_debug & AHD_SHOW_RECOVERY) != 0) {
			printf("%s: PDATA_REINIT - DFCNTRL = 0x%x "
			       "SG_CACHE_SHADOW = 0x%x\n",
			       ahd_name(ahd), ahd_inb(ahd, DFCNTRL),
			       ahd_inb(ahd, SG_CACHE_SHADOW));
		}
#endif
		ahd_reinitialize_dataptrs(ahd);
		break;
	}
	case HOST_MSG_LOOP:
	{
		struct ahd_devinfo devinfo;

		/*
		 * The sequencer has encountered a message phase
		 * that requires host assistance for completion.
		 * While handling the message phase(s), we will be
		 * notified by the sequencer after each byte is
		 * transfered so we can track bus phase changes.
		 *
		 * If this is the first time we've seen a HOST_MSG_LOOP
		 * interrupt, initialize the state of the host message
		 * loop.
		 */
		ahd_fetch_devinfo(ahd, &devinfo);
		if (ahd->msg_type == MSG_TYPE_NONE) {
			struct scb *scb;
			u_int scb_index;
			u_int bus_phase;

			bus_phase = ahd_inb(ahd, SCSISIGI) & PHASE_MASK;
			if (bus_phase != P_MESGIN
			 && bus_phase != P_MESGOUT) {
				printf("ahd_intr: HOST_MSG_LOOP bad "
				       "phase 0x%x\n", bus_phase);
				/*
				 * Probably transitioned to bus free before
				 * we got here.  Just punt the message.
				 */
				ahd_dump_card_state(ahd);
				ahd_clear_intstat(ahd);
				ahd_restart(ahd);
				return;
			}

			scb_index = ahd_get_scbptr(ahd);
			scb = ahd_lookup_scb(ahd, scb_index);
			if (devinfo.role == ROLE_INITIATOR) {
				if (bus_phase == P_MESGOUT)
					ahd_setup_initiator_msgout(ahd,
								   &devinfo,
								   scb);
				else {
					ahd->msg_type =
					    MSG_TYPE_INITIATOR_MSGIN;
					ahd->msgin_index = 0;
				}
			}
#ifdef AHD_TARGET_MODE
			else {
				if (bus_phase == P_MESGOUT) {
					ahd->msg_type =
					    MSG_TYPE_TARGET_MSGOUT;
					ahd->msgin_index = 0;
				}
				else 
					ahd_setup_target_msgin(ahd,
							       &devinfo,
							       scb);
			}
#endif
		}

		ahd_handle_message_phase(ahd);
		break;
	}
	case NO_MATCH:
	{
		/* Ensure we don't leave the selection hardware on */
		AHD_ASSERT_MODES(ahd, AHD_MODE_SCSI_MSK, AHD_MODE_SCSI_MSK);
		ahd_outb(ahd, SCSISEQ0, ahd_inb(ahd, SCSISEQ0) & ~ENSELO);

		printf("%s:%c:%d: no active SCB for reconnecting "
		       "target - issuing BUS DEVICE RESET\n",
		       ahd_name(ahd), 'A', ahd_inb(ahd, SELID) >> 4);
		printf("SAVED_SCSIID == 0x%x, SAVED_LUN == 0x%x, "
		       "REG0 == 0x%x ACCUM = 0x%x\n",
		       ahd_inb(ahd, SAVED_SCSIID), ahd_inb(ahd, SAVED_LUN),
		       ahd_inw(ahd, REG0), ahd_inb(ahd, ACCUM));
		printf("SEQ_FLAGS == 0x%x, SCBPTR == 0x%x, BTT == 0x%x, "
		       "SINDEX == 0x%x\n",
		       ahd_inb(ahd, SEQ_FLAGS), ahd_get_scbptr(ahd),
		       ahd_find_busy_tcl(ahd,
					 BUILD_TCL(ahd_inb(ahd, SAVED_SCSIID),
						   ahd_inb(ahd, SAVED_LUN))),
		       ahd_inw(ahd, SINDEX));
		printf("SELID == 0x%x, SCB_SCSIID == 0x%x, SCB_LUN == 0x%x, "
		       "SCB_CONTROL == 0x%x\n",
		       ahd_inb(ahd, SELID), ahd_inb_scbram(ahd, SCB_SCSIID),
		       ahd_inb_scbram(ahd, SCB_LUN),
		       ahd_inb_scbram(ahd, SCB_CONTROL));
		printf("SCSIBUS[0] == 0x%x, SCSISIGI == 0x%x\n",
		       ahd_inb(ahd, SCSIBUS), ahd_inb(ahd, SCSISIGI));
		printf("SXFRCTL0 == 0x%x\n", ahd_inb(ahd, SXFRCTL0));
		printf("SEQCTL0 == 0x%x\n", ahd_inb(ahd, SEQCTL0));
		ahd_dump_card_state(ahd);
		ahd->msgout_buf[0] = MSG_BUS_DEV_RESET;
		ahd->msgout_len = 1;
		ahd->msgout_index = 0;
		ahd->msg_type = MSG_TYPE_INITIATOR_MSGOUT;
		ahd_outb(ahd, MSG_OUT, HOST_MSG);
		ahd_assert_atn(ahd);
		break;
	}
	case PROTO_VIOLATION:
	{
		ahd_handle_proto_violation(ahd);
		break;
	}
	case IGN_WIDE_RES:
	{
		struct ahd_devinfo devinfo;

		ahd_fetch_devinfo(ahd, &devinfo);
		ahd_handle_ign_wide_residue(ahd, &devinfo);
		break;
	}
	case BAD_PHASE:
	{
		u_int lastphase;

		lastphase = ahd_inb(ahd, LASTPHASE);
		printf("%s:%c:%d: unknown scsi bus phase %x, "
		       "lastphase = 0x%x.  Attempting to continue\n",
		       ahd_name(ahd), 'A',
		       SCSIID_TARGET(ahd, ahd_inb(ahd, SAVED_SCSIID)),
		       lastphase, ahd_inb(ahd, SCSISIGI));
		break;
	}
	case MISSED_BUSFREE:
	{
		u_int lastphase;

		lastphase = ahd_inb(ahd, LASTPHASE);
		printf("%s:%c:%d: Missed busfree. "
		       "Lastphase = 0x%x, Curphase = 0x%x\n",
		       ahd_name(ahd), 'A',
		       SCSIID_TARGET(ahd, ahd_inb(ahd, SAVED_SCSIID)),
		       lastphase, ahd_inb(ahd, SCSISIGI));
		ahd_restart(ahd);
		return;
	}
	case DATA_OVERRUN:
	{
		/*
		 * When the sequencer detects an overrun, it
		 * places the controller in "BITBUCKET" mode
		 * and allows the target to complete its transfer.
		 * Unfortunately, none of the counters get updated
		 * when the controller is in this mode, so we have
		 * no way of knowing how large the overrun was.
		 */
		struct	scb *scb;
		u_int	scbindex;
#ifdef AHD_DEBUG
		u_int	lastphase;
#endif

		scbindex = ahd_get_scbptr(ahd);
		scb = ahd_lookup_scb(ahd, scbindex);
#ifdef AHD_DEBUG
		lastphase = ahd_inb(ahd, LASTPHASE);
		if ((ahd_debug & AHD_SHOW_RECOVERY) != 0) {
			ahd_print_path(ahd, scb);
			printf("data overrun detected %s.  Tag == 0x%x.\n",
			       ahd_lookup_phase_entry(lastphase)->phasemsg,
			       SCB_GET_TAG(scb));
			ahd_print_path(ahd, scb);
			printf("%s seen Data Phase.  Length = %ld.  "
			       "NumSGs = %d.\n",
			       ahd_inb(ahd, SEQ_FLAGS) & DPHASE
			       ? "Have" : "Haven't",
			       ahd_get_transfer_length(scb), scb->sg_count);
			ahd_dump_sglist(scb);
		}
#endif

		/*
		 * Set this and it will take effect when the
		 * target does a command complete.
		 */
		ahd_freeze_devq(ahd, scb);
		ahd_set_transaction_status(scb, CAM_DATA_RUN_ERR);
		ahd_freeze_scb(scb);
		break;
	}
	case MKMSG_FAILED:
	{
		struct ahd_devinfo devinfo;
		struct scb *scb;
		u_int scbid;

		ahd_fetch_devinfo(ahd, &devinfo);
		printf("%s:%c:%d:%d: Attempt to issue message failed\n",
		       ahd_name(ahd), devinfo.channel, devinfo.target,
		       devinfo.lun);
		scbid = ahd_get_scbptr(ahd);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb != NULL
		 && (scb->flags & SCB_RECOVERY_SCB) != 0)
			/*
			 * Ensure that we didn't put a second instance of this
			 * SCB into the QINFIFO.
			 */
			ahd_search_qinfifo(ahd, SCB_GET_TARGET(ahd, scb),
					   SCB_GET_CHANNEL(ahd, scb),
					   SCB_GET_LUN(scb), SCB_GET_TAG(scb),
					   ROLE_INITIATOR, /*status*/0,
					   SEARCH_REMOVE);
		ahd_outb(ahd, SCB_CONTROL,
			 ahd_inb_scbram(ahd, SCB_CONTROL) & ~MK_MESSAGE);
		break;
	}
	case TASKMGMT_FUNC_COMPLETE:
	{
		u_int	scbid;
		struct	scb *scb;

		scbid = ahd_get_scbptr(ahd);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb != NULL) {
			u_int	   lun;
			u_int	   tag;
			cam_status error;

			ahd_print_path(ahd, scb);
			printf("Task Management Func 0x%x Complete\n",
			       scb->hscb->task_management);
			lun = CAM_LUN_WILDCARD;
			tag = SCB_LIST_NULL;

			switch (scb->hscb->task_management) {
			case SIU_TASKMGMT_ABORT_TASK:
				tag = SCB_GET_TAG(scb);
			case SIU_TASKMGMT_ABORT_TASK_SET:
			case SIU_TASKMGMT_CLEAR_TASK_SET:
				lun = scb->hscb->lun;
				error = CAM_REQ_ABORTED;
				ahd_abort_scbs(ahd, SCB_GET_TARGET(ahd, scb),
					       'A', lun, tag, ROLE_INITIATOR,
					       error);
				break;
			case SIU_TASKMGMT_LUN_RESET:
				lun = scb->hscb->lun;
			case SIU_TASKMGMT_TARGET_RESET:
			{
				struct ahd_devinfo devinfo;

				ahd_scb_devinfo(ahd, &devinfo, scb);
				error = CAM_BDR_SENT;
				ahd_handle_devreset(ahd, &devinfo, lun,
						    CAM_BDR_SENT,
						    lun != CAM_LUN_WILDCARD
						    ? "Lun Reset"
						    : "Target Reset",
						    /*verbose_level*/0);
				break;
			}
			default:
				panic("Unexpected TaskMgmt Func\n");
				break;
			}
		}
		break;
	}
	case TASKMGMT_CMD_CMPLT_OKAY:
	{
		u_int	scbid;
		struct	scb *scb;

		/*
		 * An ABORT TASK TMF failed to be delivered before
		 * the targeted command completed normally.
		 */
		scbid = ahd_get_scbptr(ahd);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb != NULL) {
			/*
			 * Remove the second instance of this SCB from
			 * the QINFIFO if it is still there.
                         */
			ahd_print_path(ahd, scb);
			printf("SCB completes before TMF\n");
			/*
			 * Handle losing the race.  Wait until any
			 * current selection completes.  We will then
			 * set the TMF back to zero in this SCB so that
			 * the sequencer doesn't bother to issue another
			 * sequencer interrupt for its completion.
			 */
			while ((ahd_inb(ahd, SCSISEQ0) & ENSELO) != 0
			    && (ahd_inb(ahd, SSTAT0) & SELDO) == 0
			    && (ahd_inb(ahd, SSTAT1) & SELTO) == 0)
				;
			ahd_outb(ahd, SCB_TASK_MANAGEMENT, 0);
			ahd_search_qinfifo(ahd, SCB_GET_TARGET(ahd, scb),
					   SCB_GET_CHANNEL(ahd, scb),  
					   SCB_GET_LUN(scb), SCB_GET_TAG(scb), 
					   ROLE_INITIATOR, /*status*/0,   
					   SEARCH_REMOVE);
		}
		break;
	}
	case TRACEPOINT0:
	case TRACEPOINT1:
	case TRACEPOINT2:
	case TRACEPOINT3:
		printf("%s: Tracepoint %d\n", ahd_name(ahd),
		       seqintcode - TRACEPOINT0);
		break;
	case NO_SEQINT:
		break;
	case SAW_HWERR:
		ahd_handle_hwerrint(ahd);
		break;
	default:
		printf("%s: Unexpected SEQINTCODE %d\n", ahd_name(ahd),
		       seqintcode);
		break;
	}
	/*
	 *  The sequencer is paused immediately on
	 *  a SEQINT, so we should restart it when
	 *  we're done.
	 */
	ahd_unpause(ahd);
}

static void
ahd_handle_scsiint(struct ahd_softc *ahd, u_int intstat)
{
	struct scb	*scb;
	u_int		 status0;
	u_int		 status3;
	u_int		 status;
	u_int		 lqistat1;
	u_int		 lqostat0;
	u_int		 scbid;
	u_int		 busfreetime;

	ahd_update_modes(ahd);
	ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);

	status3 = ahd_inb(ahd, SSTAT3) & (NTRAMPERR|OSRAMPERR);
	status0 = ahd_inb(ahd, SSTAT0) & (IOERR|OVERRUN|SELDI|SELDO);
	status = ahd_inb(ahd, SSTAT1) & (SELTO|SCSIRSTI|BUSFREE|SCSIPERR);
	lqistat1 = ahd_inb(ahd, LQISTAT1);
	lqostat0 = ahd_inb(ahd, LQOSTAT0);
	busfreetime = ahd_inb(ahd, SSTAT2) & BUSFREETIME;

	/*
	 * Ignore external resets after a bus reset.
	 */
	if (((status & SCSIRSTI) != 0) && (ahd->flags & AHD_BUS_RESET_ACTIVE)) {
		ahd_outb(ahd, CLRSINT1, CLRSCSIRSTI);
		return;
	}

	/*
	 * Clear bus reset flag
	 */
	ahd->flags &= ~AHD_BUS_RESET_ACTIVE;

	if ((status0 & (SELDI|SELDO)) != 0) {
		u_int simode0;

		ahd_set_modes(ahd, AHD_MODE_CFG, AHD_MODE_CFG);
		simode0 = ahd_inb(ahd, SIMODE0);
		status0 &= simode0 & (IOERR|OVERRUN|SELDI|SELDO);
		ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
	}
	scbid = ahd_get_scbptr(ahd);
	scb = ahd_lookup_scb(ahd, scbid);
	if (scb != NULL
	 && (ahd_inb(ahd, SEQ_FLAGS) & NOT_IDENTIFIED) != 0)
		scb = NULL;

	if ((status0 & IOERR) != 0) {
		u_int now_lvd;

		now_lvd = ahd_inb(ahd, SBLKCTL) & ENAB40;
		printf("%s: Transceiver State Has Changed to %s mode\n",
		       ahd_name(ahd), now_lvd ? "LVD" : "SE");
		ahd_outb(ahd, CLRSINT0, CLRIOERR);
		/*
		 * A change in I/O mode is equivalent to a bus reset.
		 */
		ahd_reset_channel(ahd, 'A', /*Initiate Reset*/TRUE);
		ahd_pause(ahd);
		ahd_setup_iocell_workaround(ahd);
		ahd_unpause(ahd);
	} else if ((status0 & OVERRUN) != 0) {

		printf("%s: SCSI offset overrun detected.  Resetting bus.\n",
		       ahd_name(ahd));
		ahd_reset_channel(ahd, 'A', /*Initiate Reset*/TRUE);
	} else if ((status & SCSIRSTI) != 0) {

		printf("%s: Someone reset channel A\n", ahd_name(ahd));
		ahd_reset_channel(ahd, 'A', /*Initiate Reset*/FALSE);
	} else if ((status & SCSIPERR) != 0) {

		/* Make sure the sequencer is in a safe location. */
		ahd_clear_critical_section(ahd);

		ahd_handle_transmission_error(ahd);
	} else if (lqostat0 != 0) {

		printf("%s: lqostat0 == 0x%x!\n", ahd_name(ahd), lqostat0);
		ahd_outb(ahd, CLRLQOINT0, lqostat0);
		if ((ahd->bugs & AHD_CLRLQO_AUTOCLR_BUG) != 0)
			ahd_outb(ahd, CLRLQOINT1, 0);
	} else if ((status & SELTO) != 0) {
		/* Stop the selection */
		ahd_outb(ahd, SCSISEQ0, 0);

		/* Make sure the sequencer is in a safe location. */
		ahd_clear_critical_section(ahd);

		/* No more pending messages */
		ahd_clear_msg_state(ahd);

		/* Clear interrupt state */
		ahd_outb(ahd, CLRSINT1, CLRSELTIMEO|CLRBUSFREE|CLRSCSIPERR);

		/*
		 * Although the driver does not care about the
		 * 'Selection in Progress' status bit, the busy
		 * LED does.  SELINGO is only cleared by a sucessfull
		 * selection, so we must manually clear it to insure
		 * the LED turns off just incase no future successful
		 * selections occur (e.g. no devices on the bus).
		 */
		ahd_outb(ahd, CLRSINT0, CLRSELINGO);

		scbid = ahd_inw(ahd, WAITING_TID_HEAD);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb == NULL) {
			printf("%s: ahd_intr - referenced scb not "
			       "valid during SELTO scb(0x%x)\n",
			       ahd_name(ahd), scbid);
			ahd_dump_card_state(ahd);
		} else {
			struct ahd_devinfo devinfo;
#ifdef AHD_DEBUG
			if ((ahd_debug & AHD_SHOW_SELTO) != 0) {
				ahd_print_path(ahd, scb);
				printf("Saw Selection Timeout for SCB 0x%x\n",
				       scbid);
			}
#endif
			ahd_scb_devinfo(ahd, &devinfo, scb);
			ahd_set_transaction_status(scb, CAM_SEL_TIMEOUT);
			ahd_freeze_devq(ahd, scb);

			/*
			 * Cancel any pending transactions on the device
			 * now that it seems to be missing.  This will
			 * also revert us to async/narrow transfers until
			 * we can renegotiate with the device.
			 */
			ahd_handle_devreset(ahd, &devinfo,
					    CAM_LUN_WILDCARD,
					    CAM_SEL_TIMEOUT,
					    "Selection Timeout",
					    /*verbose_level*/1);
		}
		ahd_outb(ahd, CLRINT, CLRSCSIINT);
		ahd_iocell_first_selection(ahd);
		ahd_unpause(ahd);
	} else if ((status0 & (SELDI|SELDO)) != 0) {

		ahd_iocell_first_selection(ahd);
		ahd_unpause(ahd);
	} else if (status3 != 0) {
		printf("%s: SCSI Cell parity error SSTAT3 == 0x%x\n",
		       ahd_name(ahd), status3);
		ahd_outb(ahd, CLRSINT3, status3);
	} else if ((lqistat1 & (LQIPHASE_LQ|LQIPHASE_NLQ)) != 0) {

		/* Make sure the sequencer is in a safe location. */
		ahd_clear_critical_section(ahd);

		ahd_handle_lqiphase_error(ahd, lqistat1);
	} else if ((lqistat1 & LQICRCI_NLQ) != 0) {
		/*
		 * This status can be delayed during some
		 * streaming operations.  The SCSIPHASE
		 * handler has already dealt with this case
		 * so just clear the error.
		 */
		ahd_outb(ahd, CLRLQIINT1, CLRLQICRCI_NLQ);
	} else if ((status & BUSFREE) != 0
		|| (lqistat1 & LQOBUSFREE) != 0) {
		u_int lqostat1;
		int   restart;
		int   clear_fifo;
		int   packetized;
		u_int mode;

		/*
		 * Clear our selection hardware as soon as possible.
		 * We may have an entry in the waiting Q for this target,
		 * that is affected by this busfree and we don't want to
		 * go about selecting the target while we handle the event.
		 */
		ahd_outb(ahd, SCSISEQ0, 0);

		/* Make sure the sequencer is in a safe location. */
		ahd_clear_critical_section(ahd);

		/*
		 * Determine what we were up to at the time of
		 * the busfree.
		 */
		mode = AHD_MODE_SCSI;
		busfreetime = ahd_inb(ahd, SSTAT2) & BUSFREETIME;
		lqostat1 = ahd_inb(ahd, LQOSTAT1);
		switch (busfreetime) {
		case BUSFREE_DFF0:
		case BUSFREE_DFF1:
		{
			mode = busfreetime == BUSFREE_DFF0
			     ? AHD_MODE_DFF0 : AHD_MODE_DFF1;
			ahd_set_modes(ahd, mode, mode);
			scbid = ahd_get_scbptr(ahd);
			scb = ahd_lookup_scb(ahd, scbid);
			if (scb == NULL) {
				printf("%s: Invalid SCB %d in DFF%d "
				       "during unexpected busfree\n",
				       ahd_name(ahd), scbid, mode);
				packetized = 0;
			} else
				packetized = (scb->flags & SCB_PACKETIZED) != 0;
			clear_fifo = 1;
			break;
		}
		case BUSFREE_LQO:
			clear_fifo = 0;
			packetized = 1;
			break;
		default:
			clear_fifo = 0;
			packetized =  (lqostat1 & LQOBUSFREE) != 0;
			if (!packetized
			 && ahd_inb(ahd, LASTPHASE) == P_BUSFREE
			 && (ahd_inb(ahd, SSTAT0) & SELDI) == 0
			 && ((ahd_inb(ahd, SSTAT0) & SELDO) == 0
			  || (ahd_inb(ahd, SCSISEQ0) & ENSELO) == 0))
				/*
				 * Assume packetized if we are not
				 * on the bus in a non-packetized
				 * capacity and any pending selection
				 * was a packetized selection.
				 */
				packetized = 1;
			break;
		}

#ifdef AHD_DEBUG
		if ((ahd_debug & AHD_SHOW_MISC) != 0)
			printf("Saw Busfree.  Busfreetime = 0x%x.\n",
			       busfreetime);
#endif
		/*
		 * Busfrees that occur in non-packetized phases are
		 * handled by the nonpkt_busfree handler.
		 */
		if (packetized && ahd_inb(ahd, LASTPHASE) == P_BUSFREE) {
			restart = ahd_handle_pkt_busfree(ahd, busfreetime);
		} else {
			packetized = 0;
			restart = ahd_handle_nonpkt_busfree(ahd);
		}
		/*
		 * Clear the busfree interrupt status.  The setting of
		 * the interrupt is a pulse, so in a perfect world, we
		 * would not need to muck with the ENBUSFREE logic.  This
		 * would ensure that if the bus moves on to another
		 * connection, busfree protection is still in force.  If
		 * BUSFREEREV is broken, however, we must manually clear
		 * the ENBUSFREE if the busfree occurred during a non-pack
		 * connection so that we don't get false positives during
		 * future, packetized, connections.
		 */
		ahd_outb(ahd, CLRSINT1, CLRBUSFREE);
		if (packetized == 0
		 && (ahd->bugs & AHD_BUSFREEREV_BUG) != 0)
			ahd_outb(ahd, SIMODE1,
				 ahd_inb(ahd, SIMODE1) & ~ENBUSFREE);

		if (clear_fifo)
			ahd_clear_fifo(ahd, mode);

		ahd_clear_msg_state(ahd);
		ahd_outb(ahd, CLRINT, CLRSCSIINT);
		if (restart) {
			ahd_restart(ahd);
		} else {
			ahd_unpause(ahd);
		}
	} else {
		printf("%s: Missing case in ahd_handle_scsiint. status = %x\n",
		       ahd_name(ahd), status);
		ahd_dump_card_state(ahd);
		ahd_clear_intstat(ahd);
		ahd_unpause(ahd);
	}
}

static void
ahd_handle_transmission_error(struct ahd_softc *ahd)
{
	struct	scb *scb;
	u_int	scbid;
	u_int	lqistat1;
	u_int	lqistat2;
	u_int	msg_out;
	u_int	curphase;
	u_int	lastphase;
	u_int	perrdiag;
	u_int	cur_col;
	int	silent;

	scb = NULL;
	ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
	lqistat1 = ahd_inb(ahd, LQISTAT1) & ~(LQIPHASE_LQ|LQIPHASE_NLQ);
	lqistat2 = ahd_inb(ahd, LQISTAT2);
	if ((lqistat1 & (LQICRCI_NLQ|LQICRCI_LQ)) == 0
	 && (ahd->bugs & AHD_NLQICRC_DELAYED_BUG) != 0) {
		u_int lqistate;

		ahd_set_modes(ahd, AHD_MODE_CFG, AHD_MODE_CFG);
		lqistate = ahd_inb(ahd, LQISTATE);
		if ((lqistate >= 0x1E && lqistate <= 0x24)
		 || (lqistate == 0x29)) {
#ifdef AHD_DEBUG
			if ((ahd_debug & AHD_SHOW_RECOVERY) != 0) {
				printf("%s: NLQCRC found via LQISTATE\n",
				       ahd_name(ahd));
			}
#endif
			lqistat1 |= LQICRCI_NLQ;
		}
		ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
	}

	ahd_outb(ahd, CLRLQIINT1, lqistat1);
	lastphase = ahd_inb(ahd, LASTPHASE);
	curphase = ahd_inb(ahd, SCSISIGI) & PHASE_MASK;
	perrdiag = ahd_inb(ahd, PERRDIAG);
	msg_out = MSG_INITIATOR_DET_ERR;
	ahd_outb(ahd, CLRSINT1, CLRSCSIPERR);
	
	/*
	 * Try to find the SCB associated with this error.
	 */
	silent = FALSE;
	if (lqistat1 == 0
	 || (lqistat1 & LQICRCI_NLQ) != 0) {
	 	if ((lqistat1 & (LQICRCI_NLQ|LQIOVERI_NLQ)) != 0)
			ahd_set_active_fifo(ahd);
		scbid = ahd_get_scbptr(ahd);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb != NULL && SCB_IS_SILENT(scb))
			silent = TRUE;
	}

	cur_col = 0;
	if (silent == FALSE) {
		printf("%s: Transmission error detected\n", ahd_name(ahd));
		ahd_lqistat1_print(lqistat1, &cur_col, 50);
		ahd_lastphase_print(lastphase, &cur_col, 50);
		ahd_scsisigi_print(curphase, &cur_col, 50);
		ahd_perrdiag_print(perrdiag, &cur_col, 50);
		printf("\n");
		ahd_dump_card_state(ahd);
	}

	if ((lqistat1 & (LQIOVERI_LQ|LQIOVERI_NLQ)) != 0) {
		if (silent == FALSE) {
			printf("%s: Gross protocol error during incoming "
			       "packet.  lqistat1 == 0x%x.  Resetting bus.\n",
			       ahd_name(ahd), lqistat1);
		}
		ahd_reset_channel(ahd, 'A', /*Initiate Reset*/TRUE);
		return;
	} else if ((lqistat1 & LQICRCI_LQ) != 0) {
		/*
		 * A CRC error has been detected on an incoming LQ.
		 * The bus is currently hung on the last ACK.
		 * Hit LQIRETRY to release the last ack, and
		 * wait for the sequencer to determine that ATNO
		 * is asserted while in message out to take us
		 * to our host message loop.  No NONPACKREQ or
		 * LQIPHASE type errors will occur in this
		 * scenario.  After this first LQIRETRY, the LQI
		 * manager will be in ISELO where it will
		 * happily sit until another packet phase begins.
		 * Unexpected bus free detection is enabled
		 * through any phases that occur after we release
		 * this last ack until the LQI manager sees a
		 * packet phase.  This implies we may have to
		 * ignore a perfectly valid "unexected busfree"
		 * after our "initiator detected error" message is
		 * sent.  A busfree is the expected response after
		 * we tell the target that it's L_Q was corrupted.
		 * (SPI4R09 10.7.3.3.3)
		 */
		ahd_outb(ahd, LQCTL2, LQIRETRY);
		printf("LQIRetry for LQICRCI_LQ to release ACK\n");
	} else if ((lqistat1 & LQICRCI_NLQ) != 0) {
		/*
		 * We detected a CRC error in a NON-LQ packet.
		 * The hardware has varying behavior in this situation
		 * depending on whether this packet was part of a
		 * stream or not.
		 *
		 * PKT by PKT mode:
		 * The hardware has already acked the complete packet.
		 * If the target honors our outstanding ATN condition,
		 * we should be (or soon will be) in MSGOUT phase.
		 * This will trigger the LQIPHASE_LQ status bit as the
		 * hardware was expecting another LQ.  Unexpected
		 * busfree detection is enabled.  Once LQIPHASE_LQ is
		 * true (first entry into host message loop is much
		 * the same), we must clear LQIPHASE_LQ and hit
		 * LQIRETRY so the hardware is ready to handle
		 * a future LQ.  NONPACKREQ will not be asserted again
		 * once we hit LQIRETRY until another packet is
		 * processed.  The target may either go busfree
		 * or start another packet in response to our message.
		 *
		 * Read Streaming P0 asserted:
		 * If we raise ATN and the target completes the entire
		 * stream (P0 asserted during the last packet), the
		 * hardware will ack all data and return to the ISTART
		 * state.  When the target reponds to our ATN condition,
		 * LQIPHASE_LQ will be asserted.  We should respond to
		 * this with an LQIRETRY to prepare for any future
		 * packets.  NONPACKREQ will not be asserted again
		 * once we hit LQIRETRY until another packet is
		 * processed.  The target may either go busfree or
		 * start another packet in response to our message.
		 * Busfree detection is enabled.
		 *
		 * Read Streaming P0 not asserted:
		 * If we raise ATN and the target transitions to
		 * MSGOUT in or after a packet where P0 is not
		 * asserted, the hardware will assert LQIPHASE_NLQ.
		 * We should respond to the LQIPHASE_NLQ with an
		 * LQIRETRY.  Should the target stay in a non-pkt
		 * phase after we send our message, the hardware
		 * will assert LQIPHASE_LQ.  Recovery is then just as
		 * listed above for the read streaming with P0 asserted.
		 * Busfree detection is enabled.
		 */
		if (silent == FALSE)
			printf("LQICRC_NLQ\n");
		if (scb == NULL) {
			printf("%s: No SCB valid for LQICRC_NLQ.  "
			       "Resetting bus\n", ahd_name(ahd));
			ahd_reset_channel(ahd, 'A', /*Initiate Reset*/TRUE);
			return;
		}
	} else if ((lqistat1 & LQIBADLQI) != 0) {
		printf("Need to handle BADLQI!\n");
		ahd_reset_channel(ahd, 'A', /*Initiate Reset*/TRUE);
		return;
	} else if ((perrdiag & (PARITYERR|PREVPHASE)) == PARITYERR) {
		if ((curphase & ~P_DATAIN_DT) != 0) {
			/* Ack the byte.  So we can continue. */
			if (silent == FALSE)
				printf("Acking %s to clear perror\n",
				    ahd_lookup_phase_entry(curphase)->phasemsg);
			ahd_inb(ahd, SCSIDAT);
		}
	
		if (curphase == P_MESGIN)
			msg_out = MSG_PARITY_ERROR;
	}

	/*
	 * We've set the hardware to assert ATN if we 
	 * get a parity error on "in" phases, so all we
	 * need to do is stuff the message buffer with
	 * the appropriate message.  "In" phases have set
	 * mesg_out to something other than MSG_NOP.
	 */
	ahd->send_msg_perror = msg_out;
	if (scb != NULL && msg_out == MSG_INITIATOR_DET_ERR)
		scb->flags |= SCB_TRANSMISSION_ERROR;
	ahd_outb(ahd, MSG_OUT, HOST_MSG);
	ahd_outb(ahd, CLRINT, CLRSCSIINT);
	ahd_unpause(ahd);
}

static void
ahd_handle_lqiphase_error(struct ahd_softc *ahd, u_int lqistat1)
{
	/*
	 * Clear the sources of the interrupts.
	 */
	ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
	ahd_outb(ahd, CLRLQIINT1, lqistat1);

	/*
	 * If the "illegal" phase changes were in response
	 * to our ATN to flag a CRC error, AND we ended up
	 * on packet boundaries, clear the error, restart the
	 * LQI manager as appropriate, and go on our merry
	 * way toward sending the message.  Otherwise, reset
	 * the bus to clear the error.
	 */
	ahd_set_active_fifo(ahd);
	if ((ahd_inb(ahd, SCSISIGO) & ATNO) != 0
	 && (ahd_inb(ahd, MDFFSTAT) & DLZERO) != 0) {
		if ((lqistat1 & LQIPHASE_LQ) != 0) {
			printf("LQIRETRY for LQIPHASE_LQ\n");
			ahd_outb(ahd, LQCTL2, LQIRETRY);
		} else if ((lqistat1 & LQIPHASE_NLQ) != 0) {
			printf("LQIRETRY for LQIPHASE_NLQ\n");
			ahd_outb(ahd, LQCTL2, LQIRETRY);
		} else
			panic("ahd_handle_lqiphase_error: No phase errors\n");
		ahd_dump_card_state(ahd);
		ahd_outb(ahd, CLRINT, CLRSCSIINT);
		ahd_unpause(ahd);
	} else {
		printf("Reseting Channel for LQI Phase error\n");
		ahd_dump_card_state(ahd);
		ahd_reset_channel(ahd, 'A', /*Initiate Reset*/TRUE);
	}
}

/*
 * Packetized unexpected or expected busfree.
 * Entered in mode based on busfreetime.
 */
static int
ahd_handle_pkt_busfree(struct ahd_softc *ahd, u_int busfreetime)
{
	u_int lqostat1;

	AHD_ASSERT_MODES(ahd, ~(AHD_MODE_UNKNOWN_MSK|AHD_MODE_CFG_MSK),
			 ~(AHD_MODE_UNKNOWN_MSK|AHD_MODE_CFG_MSK));
	lqostat1 = ahd_inb(ahd, LQOSTAT1);
	if ((lqostat1 & LQOBUSFREE) != 0) {
		struct scb *scb;
		u_int scbid;
		u_int saved_scbptr;
		u_int waiting_h;
		u_int waiting_t;
		u_int next;

		/*
		 * The LQO manager detected an unexpected busfree
		 * either:
		 *
		 * 1) During an outgoing LQ.
		 * 2) After an outgoing LQ but before the first
		 *    REQ of the command packet.
		 * 3) During an outgoing command packet.
		 *
		 * In all cases, CURRSCB is pointing to the
		 * SCB that encountered the failure.  Clean
		 * up the queue, clear SELDO and LQOBUSFREE,
		 * and allow the sequencer to restart the select
		 * out at its lesure.
		 */
		ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
		scbid = ahd_inw(ahd, CURRSCB);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb == NULL)
		       panic("SCB not valid during LQOBUSFREE");
		/*
		 * Clear the status.
		 */
		ahd_outb(ahd, CLRLQOINT1, CLRLQOBUSFREE);
		if ((ahd->bugs & AHD_CLRLQO_AUTOCLR_BUG) != 0)
			ahd_outb(ahd, CLRLQOINT1, 0);
		ahd_outb(ahd, SCSISEQ0, ahd_inb(ahd, SCSISEQ0) & ~ENSELO);
		ahd_flush_device_writes(ahd);
		ahd_outb(ahd, CLRSINT0, CLRSELDO);

		/*
		 * Return the LQO manager to its idle loop.  It will
		 * not do this automatically if the busfree occurs
		 * after the first REQ of either the LQ or command
		 * packet or between the LQ and command packet.
		 */
		ahd_outb(ahd, LQCTL2, ahd_inb(ahd, LQCTL2) | LQOTOIDLE);

		/*
		 * Update the waiting for selection queue so
		 * we restart on the correct SCB.
		 */
		waiting_h = ahd_inw(ahd, WAITING_TID_HEAD);
		saved_scbptr = ahd_get_scbptr(ahd);
		if (waiting_h != scbid) {

			ahd_outw(ahd, WAITING_TID_HEAD, scbid);
			waiting_t = ahd_inw(ahd, WAITING_TID_TAIL);
			if (waiting_t == waiting_h) {
				ahd_outw(ahd, WAITING_TID_TAIL, scbid);
				next = SCB_LIST_NULL;
			} else {
				ahd_set_scbptr(ahd, waiting_h);
				next = ahd_inw_scbram(ahd, SCB_NEXT2);
			}
			ahd_set_scbptr(ahd, scbid);
			ahd_outw(ahd, SCB_NEXT2, next);
		}
		ahd_set_scbptr(ahd, saved_scbptr);
		if (scb->crc_retry_count < AHD_MAX_LQ_CRC_ERRORS) {
			if (SCB_IS_SILENT(scb) == FALSE) {
				ahd_print_path(ahd, scb);
				printf("Probable outgoing LQ CRC error.  "
				       "Retrying command\n");
			}
			scb->crc_retry_count++;
		} else {
			ahd_set_transaction_status(scb, CAM_UNCOR_PARITY);
			ahd_freeze_scb(scb);
			ahd_freeze_devq(ahd, scb);
		}
		/* Return unpausing the sequencer. */
		return (0);
	} else if ((ahd_inb(ahd, PERRDIAG) & PARITYERR) != 0) {
		/*
		 * Ignore what are really parity errors that
		 * occur on the last REQ of a free running
		 * clock prior to going busfree.  Some drives
		 * do not properly active negate just before
		 * going busfree resulting in a parity glitch.
		 */
		ahd_outb(ahd, CLRSINT1, CLRSCSIPERR|CLRBUSFREE);
#ifdef AHD_DEBUG
		if ((ahd_debug & AHD_SHOW_MASKED_ERRORS) != 0)
			printf("%s: Parity on last REQ detected "
			       "during busfree phase.\n",
			       ahd_name(ahd));
#endif
		/* Return unpausing the sequencer. */
		return (0);
	}
	if (ahd->src_mode != AHD_MODE_SCSI) {
		u_int	scbid;
		struct	scb *scb;

		scbid = ahd_get_scbptr(ahd);
		scb = ahd_lookup_scb(ahd, scbid);
		ahd_print_path(ahd, scb);
		printf("Unexpected PKT busfree condition\n");
		ahd_dump_card_state(ahd);
		ahd_abort_scbs(ahd, SCB_GET_TARGET(ahd, scb), 'A',
			       SCB_GET_LUN(scb), SCB_GET_TAG(scb),
			       ROLE_INITIATOR, CAM_UNEXP_BUSFREE);

		/* Return restarting the sequencer. */
		return (1);
	}
	printf("%s: Unexpected PKT busfree condition\n", ahd_name(ahd));
	ahd_dump_card_state(ahd);
	/* Restart the sequencer. */
	return (1);
}

/*
 * Non-packetized unexpected or expected busfree.
 */
static int
ahd_handle_nonpkt_busfree(struct ahd_softc *ahd)
{
	struct	ahd_devinfo devinfo;
	struct	scb *scb;
	u_int	lastphase;
	u_int	saved_scsiid;
	u_int	saved_lun;
	u_int	target;
	u_int	initiator_role_id;
	u_int	scbid;
	u_int	ppr_busfree;
	int	printerror;

	/*
	 * Look at what phase we were last in.  If its message out,
	 * chances are pretty good that the busfree was in response
	 * to one of our abort requests.
	 */
	lastphase = ahd_inb(ahd, LASTPHASE);
	saved_scsiid = ahd_inb(ahd, SAVED_SCSIID);
	saved_lun = ahd_inb(ahd, SAVED_LUN);
	target = SCSIID_TARGET(ahd, saved_scsiid);
	initiator_role_id = SCSIID_OUR_ID(saved_scsiid);
	ahd_compile_devinfo(&devinfo, initiator_role_id,
			    target, saved_lun, 'A', ROLE_INITIATOR);
	printerror = 1;

	scbid = ahd_get_scbptr(ahd);
	scb = ahd_lookup_scb(ahd, scbid);
	if (scb != NULL
	 && (ahd_inb(ahd, SEQ_FLAGS) & NOT_IDENTIFIED) != 0)
		scb = NULL;

	ppr_busfree = (ahd->msg_flags & MSG_FLAG_EXPECT_PPR_BUSFREE) != 0;
	if (lastphase == P_MESGOUT) {
		u_int tag;

		tag = SCB_LIST_NULL;
		if (ahd_sent_msg(ahd, AHDMSG_1B, MSG_ABORT_TAG, TRUE)
		 || ahd_sent_msg(ahd, AHDMSG_1B, MSG_ABORT, TRUE)) {
			int found;
			int sent_msg;

			if (scb == NULL) {
				ahd_print_devinfo(ahd, &devinfo);
				printf("Abort for unidentified "
				       "connection completed.\n");
				/* restart the sequencer. */
				return (1);
			}
			sent_msg = ahd->msgout_buf[ahd->msgout_index - 1];
			ahd_print_path(ahd, scb);
			printf("SCB %d - Abort%s Completed.\n",
			       SCB_GET_TAG(scb),
			       sent_msg == MSG_ABORT_TAG ? "" : " Tag");

			if (sent_msg == MSG_ABORT_TAG)
				tag = SCB_GET_TAG(scb);

			if ((scb->flags & SCB_EXTERNAL_RESET) != 0) {
				/*
				 * This abort is in response to an
				 * unexpected switch to command phase
				 * for a packetized connection.  Since
				 * the identify message was never sent,
				 * "saved lun" is 0.  We really want to
				 * abort only the SCB that encountered
				 * this error, which could have a different
				 * lun.  The SCB will be retried so the OS
				 * will see the UA after renegotiating to
				 * packetized.
				 */
				tag = SCB_GET_TAG(scb);
				saved_lun = scb->hscb->lun;
			}
			found = ahd_abort_scbs(ahd, target, 'A', saved_lun,
					       tag, ROLE_INITIATOR,
					       CAM_REQ_ABORTED);
			printf("found == 0x%x\n", found);
			printerror = 0;
		} else if (ahd_sent_msg(ahd, AHDMSG_1B,
					MSG_BUS_DEV_RESET, TRUE)) {
#ifdef __FreeBSD__
			/*
			 * Don't mark the user's request for this BDR
			 * as completing with CAM_BDR_SENT.  CAM3
			 * specifies CAM_REQ_CMP.
			 */
			if (scb != NULL
			 && scb->io_ctx->ccb_h.func_code== XPT_RESET_DEV
			 && ahd_match_scb(ahd, scb, target, 'A',
					  CAM_LUN_WILDCARD, SCB_LIST_NULL,
					  ROLE_INITIATOR))
				ahd_set_transaction_status(scb, CAM_REQ_CMP);
#endif
			ahd_handle_devreset(ahd, &devinfo, CAM_LUN_WILDCARD,
					    CAM_BDR_SENT, "Bus Device Reset",
					    /*verbose_level*/0);
			printerror = 0;
		} else if (ahd_sent_msg(ahd, AHDMSG_EXT, MSG_EXT_PPR, FALSE)
			&& ppr_busfree == 0) {
			struct ahd_initiator_tinfo *tinfo;
			struct ahd_tmode_tstate *tstate;

			/*
			 * PPR Rejected.
			 *
			 * If the previous negotiation was packetized,
			 * this could be because the device has been
			 * reset without our knowledge.  Force our
			 * current negotiation to async and retry the
			 * negotiation.  Otherwise retry the command
			 * with non-ppr negotiation.
			 */
#ifdef AHD_DEBUG
			if ((ahd_debug & AHD_SHOW_MESSAGES) != 0)
				printf("PPR negotiation rejected busfree.\n");
#endif
			tinfo = ahd_fetch_transinfo(ahd, devinfo.channel,
						    devinfo.our_scsiid,
						    devinfo.target, &tstate);
			if ((tinfo->curr.ppr_options & MSG_EXT_PPR_IU_REQ)!=0) {
				ahd_set_width(ahd, &devinfo,
					      MSG_EXT_WDTR_BUS_8_BIT,
					      AHD_TRANS_CUR,
					      /*paused*/TRUE);
				ahd_set_syncrate(ahd, &devinfo,
						/*period*/0, /*offset*/0,
						/*ppr_options*/0,
						AHD_TRANS_CUR,
						/*paused*/TRUE);
				/*
				 * The expect PPR busfree handler below
				 * will effect the retry and necessary
				 * abort.
				 */
			} else {
				tinfo->curr.transport_version = 2;
				tinfo->goal.transport_version = 2;
				tinfo->goal.ppr_options = 0;
				/*
				 * Remove any SCBs in the waiting for selection
				 * queue that may also be for this target so
				 * that command ordering is preserved.
				 */
				ahd_freeze_devq(ahd, scb);
				ahd_qinfifo_requeue_tail(ahd, scb);
				printerror = 0;
			}
		} else if (ahd_sent_msg(ahd, AHDMSG_EXT, MSG_EXT_WDTR, FALSE)
			&& ppr_busfree == 0) {
			/*
			 * Negotiation Rejected.  Go-narrow and
			 * retry command.
			 */
#ifdef AHD_DEBUG
			if ((ahd_debug & AHD_SHOW_MESSAGES) != 0)
				printf("WDTR negotiation rejected busfree.\n");
#endif
			ahd_set_width(ahd, &devinfo,
				      MSG_EXT_WDTR_BUS_8_BIT,
				      AHD_TRANS_CUR|AHD_TRANS_GOAL,
				      /*paused*/TRUE);
			/*
			 * Remove any SCBs in the waiting for selection
			 * queue that may also be for this target so that
			 * command ordering is preserved.
			 */
			ahd_freeze_devq(ahd, scb);
			ahd_qinfifo_requeue_tail(ahd, scb);
			printerror = 0;
		} else if (ahd_sent_msg(ahd, AHDMSG_EXT, MSG_EXT_SDTR, FALSE)
			&& ppr_busfree == 0) {
			/*
			 * Negotiation Rejected.  Go-async and
			 * retry command.
			 */
#ifdef AHD_DEBUG
			if ((ahd_debug & AHD_SHOW_MESSAGES) != 0)
				printf("SDTR negotiation rejected busfree.\n");
#endif
			ahd_set_syncrate(ahd, &devinfo,
					/*period*/0, /*offset*/0,
					/*ppr_options*/0,
					AHD_TRANS_CUR|AHD_TRANS_GOAL,
					/*paused*/TRUE);
			/*
			 * Remove any SCBs in the waiting for selection
			 * queue that may also be for this target so that
			 * command ordering is preserved.
			 */
			ahd_freeze_devq(ahd, scb);
			ahd_qinfifo_requeue_tail(ahd, scb);
			printerror = 0;
		} else if ((ahd->msg_flags & MSG_FLAG_EXPECT_IDE_BUSFREE) != 0
			&& ahd_sent_msg(ahd, AHDMSG_1B,
					 MSG_INITIATOR_DET_ERR, TRUE)) {

#ifdef AHD_DEBUG
			if ((ahd_debug & AHD_SHOW_MESSAGES) != 0)
				printf("Expected IDE Busfree\n");
#endif
			printerror = 0;
		} else if ((ahd->msg_flags & MSG_FLAG_EXPECT_QASREJ_BUSFREE)
			&& ahd_sent_msg(ahd, AHDMSG_1B,
					MSG_MESSAGE_REJECT, TRUE)) {

#ifdef AHD_DEBUG
			if ((ahd_debug & AHD_SHOW_MESSAGES) != 0)
				printf("Expected QAS Reject Busfree\n");
#endif
			printerror = 0;
		}
	}

	/*
	 * The busfree required flag is honored at the end of
	 * the message phases.  We check it last in case we
	 * had to send some other message that caused a busfree.
	 */
	if (printerror != 0
	 && (lastphase == P_MESGIN || lastphase == P_MESGOUT)
	 && ((ahd->msg_flags & MSG_FLAG_EXPECT_PPR_BUSFREE) != 0)) {

		ahd_freeze_devq(ahd, scb);
		ahd_set_transaction_status(scb, CAM_REQUEUE_REQ);
		ahd_freeze_scb(scb);
		if ((ahd->msg_flags & MSG_FLAG_IU_REQ_CHANGED) != 0) {
			ahd_abort_scbs(ahd, SCB_GET_TARGET(ahd, scb),
				       SCB_GET_CHANNEL(ahd, scb),
				       SCB_GET_LUN(scb), SCB_LIST_NULL,
				       ROLE_INITIATOR, CAM_REQ_ABORTED);
		} else {
#ifdef AHD_DEBUG
			if ((ahd_debug & AHD_SHOW_MESSAGES) != 0)
				printf("PPR Negotiation Busfree.\n");
#endif
			ahd_done(ahd, scb);
		}
		printerror = 0;
	}
	if (printerror != 0) {
		int aborted;

		aborted = 0;
		if (scb != NULL) {
			u_int tag;

			if ((scb->hscb->control & TAG_ENB) != 0)
				tag = SCB_GET_TAG(scb);
			else
				tag = SCB_LIST_NULL;
			ahd_print_path(ahd, scb);
			aborted = ahd_abort_scbs(ahd, target, 'A',
				       SCB_GET_LUN(scb), tag,
				       ROLE_INITIATOR,
				       CAM_UNEXP_BUSFREE);
		} else {
			/*
			 * We had not fully identified this connection,
			 * so we cannot abort anything.
			 */
			printf("%s: ", ahd_name(ahd));
		}
		printf("Unexpected busfree %s, %d SCBs aborted, "
		       "PRGMCNT == 0x%x\n",
		       ahd_lookup_phase_entry(lastphase)->phasemsg,
		       aborted,
		       ahd_inw(ahd, PRGMCNT));
		ahd_dump_card_state(ahd);
		if (lastphase != P_BUSFREE)
			ahd_force_renegotiation(ahd, &devinfo);
	}
	/* Always restart the sequencer. */
	return (1);
}

static void
ahd_handle_proto_violation(struct ahd_softc *ahd)
{
	struct	ahd_devinfo devinfo;
	struct	scb *scb;
	u_int	scbid;
	u_int	seq_flags;
	u_int	curphase;
	u_int	lastphase;
	int	found;

	ahd_fetch_devinfo(ahd, &devinfo);
	scbid = ahd_get_scbptr(ahd);
	scb = ahd_lookup_scb(ahd, scbid);
	seq_flags = ahd_inb(ahd, SEQ_FLAGS);
	curphase = ahd_inb(ahd, SCSISIGI) & PHASE_MASK;
	lastphase = ahd_inb(ahd, LASTPHASE);
	if ((seq_flags & NOT_IDENTIFIED) != 0) {

		/*
		 * The reconnecting target either did not send an
		 * identify message, or did, but we didn't find an SCB
		 * to match.
		 */
		ahd_print_devinfo(ahd, &devinfo);
		printf("Target did not send an IDENTIFY message. "
		       "LASTPHASE = 0x%x.\n", lastphase);
		scb = NULL;
	} else if (scb == NULL) {
		/*
		 * We don't seem to have an SCB active for this
		 * transaction.  Print an error and reset the bus.
		 */
		ahd_print_devinfo(ahd, &devinfo);
		printf("No SCB found during protocol violation\n");
		goto proto_violation_reset;
	} else {
		ahd_set_transaction_status(scb, CAM_SEQUENCE_FAIL);
		if ((seq_flags & NO_CDB_SENT) != 0) {
			ahd_print_path(ahd, scb);
			printf("No or incomplete CDB sent to device.\n");
		} else if ((ahd_inb_scbram(ahd, SCB_CONTROL)
			  & STATUS_RCVD) == 0) {
			/*
			 * The target never bothered to provide status to
			 * us prior to completing the command.  Since we don't
			 * know the disposition of this command, we must attempt
			 * to abort it.  Assert ATN and prepare to send an abort
			 * message.
			 */
			ahd_print_path(ahd, scb);
			printf("Completed command without status.\n");
		} else {
			ahd_print_path(ahd, scb);
			printf("Unknown protocol violation.\n");
			ahd_dump_card_state(ahd);
		}
	}
	if ((lastphase & ~P_DATAIN_DT) == 0
	 || lastphase == P_COMMAND) {
proto_violation_reset:
		/*
		 * Target either went directly to data
		 * phase or didn't respond to our ATN.
		 * The only safe thing to do is to blow
		 * it away with a bus reset.
		 */
		found = ahd_reset_channel(ahd, 'A', TRUE);
		printf("%s: Issued Channel %c Bus Reset. "
		       "%d SCBs aborted\n", ahd_name(ahd), 'A', found);
	} else {
		/*
		 * Leave the selection hardware off in case
		 * this abort attempt will affect yet to
		 * be sent commands.
		 */
		ahd_outb(ahd, SCSISEQ0,
			 ahd_inb(ahd, SCSISEQ0) & ~ENSELO);
		ahd_assert_atn(ahd);
		ahd_outb(ahd, MSG_OUT, HOST_MSG);
		if (scb == NULL) {
			ahd_print_devinfo(ahd, &devinfo);
			ahd->msgout_buf[0] = MSG_ABORT_TASK;
			ahd->msgout_len = 1;
			ahd->msgout_index = 0;
			ahd->msg_type = MSG_TYPE_INITIATOR_MSGOUT;
		} else {
			ahd_print_path(ahd, scb);
			scb->flags |= SCB_ABORT;
		}
		printf("Protocol violation %s.  Attempting to abort.\n",
		       ahd_lookup_phase_entry(curphase)->phasemsg);
	}
}

/*
 * Force renegotiation to occur the next time we initiate
 * a command to the current device.
 */
static void
ahd_force_renegotiation(struct ahd_softc *ahd, struct ahd_devinfo *devinfo)
{
	struct	ahd_initiator_tinfo *targ_info;
	struct	ahd_tmode_tstate *tstate;

#ifdef AHD_DEBUG
	if ((ahd_debug & AHD_SHOW_MESSAGES) != 0) {
		ahd_print_devinfo(ahd, devinfo);
		printf("Forcing renegotiation\n");
	}
#endif
	targ_info = ahd_fetch_transinfo(ahd,
					devinfo->channel,
					devinfo->our_scsiid,
					devinfo->target,
					&tstate);
	ahd_update_neg_request(ahd, devinfo, tstate,
			       targ_info, AHD_NEG_IF_NON_ASYNC);
}

#define AHD_MAX_STEPS 2000
static void
ahd_clear_critical_section(struct ahd_softc *ahd)
{
	ahd_mode_state	saved_modes;
	int		stepping;
	int		steps;
	int		first_instr;
	u_int		simode0;
	u_int		simode1;
	u_int		simode3;
	u_int		lqimode0;
	u_int		lqimode1;
	u_int		lqomode0;
	u_int		lqomode1;

	if (ahd->num_critical_sections == 0)
		return;

	stepping = FALSE;
	steps = 0;
	first_instr = 0;
	simode0 = 0;
	simode1 = 0;
	simode3 = 0;
	lqimode0 = 0;
	lqimode1 = 0;
	lqomode0 = 0;
	lqomode1 = 0;
	saved_modes = ahd_save_modes(ahd);
	for (;;) {
		struct	cs *cs;
		u_int	seqaddr;
		u_int	i;

		ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
		seqaddr = ahd_inw(ahd, CURADDR);

		cs = ahd->critical_sections;
		for (i = 0; i < ahd->num_critical_sections; i++, cs++) {
			
			if (cs->begin < seqaddr && cs->end >= seqaddr)
				break;
		}

		if (i == ahd->num_critical_sections)
			break;

		if (steps > AHD_MAX_STEPS) {
			printf("%s: Infinite loop in critical section\n"
			       "%s: First Instruction 0x%x now 0x%x\n",
			       ahd_name(ahd), ahd_name(ahd), first_instr,
			       seqaddr);
			ahd_dump_card_state(ahd);
			panic("critical section loop");
		}

		steps++;
#ifdef AHD_DEBUG
		if ((ahd_debug & AHD_SHOW_MISC) != 0)
			printf("%s: Single stepping at 0x%x\n", ahd_name(ahd),
			       seqaddr);
#endif
		if (stepping == FALSE) {

			first_instr = seqaddr;
  			ahd_set_modes(ahd, AHD_MODE_CFG, AHD_MODE_CFG);
  			simode0 = ahd_inb(ahd, SIMODE0);
			simode3 = ahd_inb(ahd, SIMODE3);
			lqimode0 = ahd_inb(ahd, LQIMODE0);
			lqimode1 = ahd_inb(ahd, LQIMODE1);
			lqomode0 = ahd_inb(ahd, LQOMODE0);
			lqomode1 = ahd_inb(ahd, LQOMODE1);
			ahd_outb(ahd, SIMODE0, 0);
			ahd_outb(ahd, SIMODE3, 0);
			ahd_outb(ahd, LQIMODE0, 0);
			ahd_outb(ahd, LQIMODE1, 0);
			ahd_outb(ahd, LQOMODE0, 0);
			ahd_outb(ahd, LQOMODE1, 0);
			ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
			simode1 = ahd_inb(ahd, SIMODE1);
			/*
			 * We don't clear ENBUSFREE.  Unfortunately
			 * we cannot re-enable busfree detection within
			 * the current connection, so we must leave it
			 * on while single stepping.
			 */
			ahd_outb(ahd, SIMODE1, simode1 & ENBUSFREE);
			ahd_outb(ahd, SEQCTL0, ahd_inb(ahd, SEQCTL0) | STEP);
			stepping = TRUE;
		}
		ahd_outb(ahd, CLRSINT1, CLRBUSFREE);
		ahd_outb(ahd, CLRINT, CLRSCSIINT);
		ahd_set_modes(ahd, ahd->saved_src_mode, ahd->saved_dst_mode);
		ahd_outb(ahd, HCNTRL, ahd->unpause);
		while (!ahd_is_paused(ahd))
			ahd_delay(200);
		ahd_update_modes(ahd);
	}
	if (stepping) {
		ahd_set_modes(ahd, AHD_MODE_CFG, AHD_MODE_CFG);
		ahd_outb(ahd, SIMODE0, simode0);
		ahd_outb(ahd, SIMODE3, simode3);
		ahd_outb(ahd, LQIMODE0, lqimode0);
		ahd_outb(ahd, LQIMODE1, lqimode1);
		ahd_outb(ahd, LQOMODE0, lqomode0);
		ahd_outb(ahd, LQOMODE1, lqomode1);
		ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
		ahd_outb(ahd, SEQCTL0, ahd_inb(ahd, SEQCTL0) & ~STEP);
  		ahd_outb(ahd, SIMODE1, simode1);
		/*
		 * SCSIINT seems to glitch occassionally when
		 * the interrupt masks are restored.  Clear SCSIINTtablesone more time so that only persistent errorstables plaseen as a realareable ac.table/
		ahd_outb(ahd, CLRINTtribuyright );
	}
.
 *
tforms._modesedistrsaved forms);
}

/*
 *
 * Copan * Cndingareable acrstatus.
erve thaic void

 *
c* Co_int tha(struct 
 *
softc *ahd)
{
	AHD_ASSERT_MODESedistr~(sour mus_UNKNOWN_MSK|the aboveCFGght
),
			in the above copyright
 *    notice, this sou/cation, are reable acrconditions this may have causederved
 *
 * RedistribuLQght 0reproducATNQAS|producCRCT1claimer
 * 2list	 claimerBADLQTclaimerATNLQ"NO WARRANCMD souy form must reproduce a1reproducPHASE_TY" discledistrNTY" disIQABOR (c) similar tCRCIribution mudingNTY" disclo theIon
 *    inclOVERng a substament fNTY" diNONPACKREQbelow
 *    ("DisclaimeOe at minimuOTARGSCBPERRclaimeOSTOPT2sted coRANTYon
 *    incers PK "NO WAOTCRCbution.
 * 3. Neither the nand any OINITbove-listed copyriIht holdeBADQASr the names
 BUSFREEf any cPHACHGI   o souif (edis->bugs & sour or pr_AUTOCLR_BUG) != 0) {ed.
 *
 * Redistribu the name0 soutors may be used to endorseerms rce and * RedistribuSINT3tribuNTRAMe-listedOShed by below
 *    ("Disclai 2 aand anSELTIMEOclaiATN * THyrigRSTlaimerclaithout speciyrige-listedREQomotoftware Foundation.
 *
 *t miniSELDSOFTWAELDIS OR IMINGOlist  INCLUDclaiIOby the FVERRUNbelow
 *    ("Disclaition and use in so modiILITY AND FITNESS FOR
 * A  DebuggitteRoutines ILITY AND FITNESS FOR
 * A P**/
#ifdef* AltDEBUG
uint32_. RedidTICU =PYRIGHT
 *_OPTS;
#ermif
 THE 0
 conditioprint_scb:
 * 1. scb *scbs of 
 * 1. hardwareEQUE *hscben pnt i;

	ED T =* DA->ED TO,  CONSf("scb:%patiotrol:0x%x scsiidSS OF lun:%d cdb_lePROF\n",
, INCLUD( con *)scbSS INTERRUED T->CES; LOOWEVER CAUSED ANUSE, DOWEVER CAUSED ANlunOWEVER CAUSED ANDS; OR  souODS
 * OShared Data: " soufor (i = 0; i < sizeof(F LIABILUDIN_data.i OUT cdb); i++)
	 TORT (IN%#02x",OF LIABINY WAY OUT OF THE US[i] OR TORT (IN INCLUDI OUTptr:%# OF  OUTcntd: / sg $Id: / tagd: /INESS INTERRUPHOLDERS )missi_le64toh
 * IN *
 * $I) >> 32) & 0xF"aic79xs li
 */

#ifdef __linx__
#include "aic79xx_osm.hude "aic79xx_inline.h"
x__
#i32lude "aic79xx_cnte <dev/aic7xxx/aic79xx_osm.h>aic7x_inline.h"
SCB_GET_TAG(GES below
 *dump_sglistaicasHANT SPECI  /*  0 erveIBILITY AND FITNESS FOR
 *  Transfer Negotia
 * RE DISCLAIMED. IN NO EVENT SHAL*/odificaAllocate* Co target form instance (ID wlatfopond toc Inc.ames[])ficat *******n*********** OUT 
 * 1.ure the following
 * 1. Reditform_e mete *ditioat ah);

/*
:
 * 1. Redistribution, u are USE,_id, char{
  nnel (INCLUDING,hip_names);

/*
 *master);

/*
;t errno;
	const char *errme
static cPROCUREMEsg;
};

stati =
	co->enabled_ames[]s[SCTMOour_id]en permSCTMOUT,	"Discard Timr_entry]ftwaNULL
	 &&DSCTMOUT,	"Discard Timgal Opcode isg;
};

statiHIS Sanic("%s:
	conware error c - Tmes[] already ware atedESS line.h"x__
name},
	sm/ai = {
	{ Dmware (RISING
or_entr), M_DEVBUFIOBUNOWACONTRIperm = {
	{  in seHIS return ( u_inREME/*
blesIfc790butio	{ MPARER aencer P ;

/*
, copy user settings fromIZE(a shac const struc (takentry a ree
 orhase_EEPROM)  OR  RedIZE(a    uin, butatfoet our currght and goal_table_entc7902ync/narrowIZE(auntil an ini****ut palkn phat thservedpermrrors[] = {
	{e in see may memcpytatic c,encer Parity E,ARISING
RERR,	"Crms omemsettatic cMOUT,	"Disluns, 0 phase"	}NOOP,		"in Command pCOMMAN OR OTHERWISE) A16 OF TH may ND,	MSG_&NOOP,		"
};
sinfo[i].ATORhaseIES, INCLU	},
	{ P_MESGOUOR_DET_ERR,	"in SCOMMANATUS,	MSG_INITIATOR_DET_ERR,	",	"itatus phase"	},
	{ P_MESGIN,	MSG_PARITY_ER,	"iCOMMAN}
	} elseMAND,	MSG_NOOP,	hase"	},
	{ ,
	{ P_COMMAr program" },
	{ SQPARERR,	"Se=t struc;
num_errorERR,	"CHANTITHE COPYRIGhe aEde musodificaFrechip_names[] =
{
	"NONE",
	"aic7901",
	"aic7902",
	"aic7901A"
};
static const u_int num_chip_names = ARRAY_SIZ conditiofres);

/*
odes.
 */
struct ahd_hard_error_entry {
        uin,ut m-outcet8_t errno;
	const char *errme so
 * eAY_SIZE(aDon't ns
 n upINITI"sg;
};"t strucout p IG, BsINITIdefaultphase_table_enout phase"	}r_entryonster has timequence***/
onstFALSEnt num_err_err = {
	{ DSCTMOUT,	"Discard Timgal Opco,
};
static coDT,	MSG_in Mree
/*
 * InBUS Pariy wish to itterate over real phases,n sendif


/**modificatalled and _hard_erron actitiononnec*****c790names[] );
she bus,01A"
Redifunahd);
find2. Re neatfor* Coioaic79 sharepud,
						limitedficabyruct capabilities ofhd_forceoftc *ahdvitymode_ERRe"	}_table_entrorrenegoe
	"aic7the following conditiodevdevinfo_e"	}r************* Function Decn Me
 * 1. RediG_NOOP,		_tT_ER *hd_so		ahdd_erro*
					ard_erro*ppr_op
 * 2, role_tt for (INCLUDINGow
 *OR_DET_ER *OR_DET_ER;
				u_	maxe"	}_errpermissi_inRedistrSBLKCTL
#elENAB40oftwarquenceahd,
					     STATncludEXP_ACTIVE)onstre may uct ahdUTORS BSYNCRATE_PACEDLicen phar_options,
						role_t roleULTRAms o
 * a ahddo DT rel

stannel, i		ahan SEtate*rved.r channel, i &= MSG_EXT_PPR_QAS_REQLicens_SIZE(aNeverrors)w a value highnst hanINITIATOR_DET,	"iIZE(a
						otherwisec790strid_updateames[] G_NOOP,e;
st*ic const u_into go abovehd,
	devinc InSG_IstaticDATAOUser.  Iahd_focasec_tstaSG_NOOP,		"d_softc *ahd,
s,
		c const u_i,c790evinfobain b	ahd_fod_scruct aable_e.  TRedid_updhd_sofsystemct astill acceptahd,
incomittec const u_is even iftruct ahd_softc *ahd,
					  strucis nod,
		formedout phase"	} foronstROLEe countatic		ahd_devl= &hd_so->d_sc;
	void	ct ahd_softc *ahd,
				,	"i;
					     struct (OR_DET_ER-> channel, i|ahd_transinfPCOMP_ETED T;
staruct_sdtr(widthonstahd_tranWDThis S_8_BITpr_options,
				max(uct ahd, (d_err)g_table(struct ahd2rms o				     struct ~ahd_transinfDTtinfo);
stt ahd_devinfo *
						t *ppr_optnt scsiHERWI_softc *ahd,
			oid		ac void		ahdstatic voitatint scsi_ihd_cons,
					   u_int brms of thct adef AHD_TAd_hart scsi_i channel, intuct ahd sourc modificaLookc *ad,
	valid,
						strARE IATEoftcvCopyscb ruct at,	"Dthe  Rm_errod,
	
						_ERRoffSG_I Gibbshould bAdapnt	struct ames[];
stevinRediwahd_sofbeginnittet ahd_SDTRthe fo
/******** period, u_indes.
 */
struct ahd_hard_erront scsi_	"Scrd, char channel, intd_erro_width,  of permstatic v<   u_int moftc *ahd,
				 ahd_softc *a				     struc ahd_transinfdevinfitiator_tinfstatic v>			role_t roleMIN_Druct ftc *ahd,
					   struct ahd_devinfo *	gval, int fullstatic int		ahd_ha int		ahd_pars0_error Honor PPRuct ahdoftcaticE",
	rules. phase"	}ruct ahd_devinfo *devin);
ste_msg_reject(struct ahd_softc *ahRTItruct ahd_softc *ahd,
				      struIU ahd_dt *pp_msg_reject(struct (		      struct ahdsoftc *ahd,
	o *tinfARRAYt ahd_softc *ahd,
				      struct ahd_did		ahd_handle_devreset(ahd_transinfo *tinfo)r,
 *Skipetup );
stbs.
 *entrd_tmif IUb *scb);avail,	"D phase"	}rs(struct ahd_softc *ahd);
static void		fo *devinfo);
<			role_t roledle_msg_atic voidevinfo *devinf verbose_level);DTef AHD_TARGET_MODDTstatic void		ahd_setup_target_msgin(struct ahd_softc *lun, cam_sta     struct ahd_devinfo *deviruct ah,
					       struct scb *ruct a * modificaTrunhd_chd,
	gihd_de"	}hronoutmodssage_tati_pendivoid	},
TOR_DET_daponst yptructe(strHD_T03 Adstrucluct the following conditio voidate_ ahd_sARGET_MODE
static void		ahe"	},free_tstate(struct ahd_softc *ahd,
	 INCd_errot scsi_id, char ahd_s******wide,	"Scrat force);
#endif
			     u ahd_s verbosLvinfo ahd_softcwGibbwtc *n
			phase"	}_int bus_widoption ahd_sooid		avoid	c *ahd,
			<				role_t role);
st{ P_STpermission.
 *
 * Alt);
st_NEGTABLEis softwaren Mes scb *scb, MAX_OFFSET,
					BUG		   phase"tic void		ahd_chip_init(strucstruct ahruct scb *scb, _chip_init(NONe);
stati		ahd_i,
		in(		ahd_iocic void		truct ahd_devlDT,	MSG_INITI_build_transfer_msg(struct  scb *prev_scb,
					   ahd_devi,
					   .ruct scb *oftc *ahdc *ahd);
static int		ahd_search_scb_l,	"istruct ahd_nt ppr_optiid		ahd_initialize_
};
statidevinfparameonst tc *ahd);
static int		ahd_init_scbdataisc *ahd);
static void		ahd_fini_scbdata(strdevinic int		ahd_sent_msg(strd_setup_iocell_workaround(struct ahd_sofuct ahdbusnt tidnt force);
#endif
sw*
 * (int tid_pr{ P_Se_error:d_qinfiSCTMOfea_name
 * AltWIDE{ P_STA/stat,
	"aiWid_setup		int tid_prftc 				   u_int per16od, in Mebreakn unknouct FALLTHROUGH_softstruc					   u_int period, tic 			 u_int scbid);
static u_intiod, hd_sscb(strudevinfo,
static int		ahd_qinfifo_count(struct ahd_softc		 u_int scbscb,d_search_scb_list(sid_prev  u_int tidhd_softc *ahd	ahd_stat_timer;
#ifdef AHD_DU int Q
static void		ahd_dunt ppr_optiUpa(sthd_foritoss mode_ard Ti-out whichinfo(sES; LOlse_t(strutic c const e with age_poftcxcatiovenid_sooportunity		ahd_seint		ahlytic mea 2. R*start_ustinw ahd_druct ahNOOPl identify messagesoftc *aha newint32_ ahdonthe fointditiou *ahd_neg_requet.h>
static void		ahd_stitch_tid_listdev_devli	   u_i,	"ScratchZE(ahd_chip_names);

/*
 */*
 * Ir, uint8_t *dconsts);e(struct ahd_softc *ahdh or Seg_bdataahd_scb_ahd_softc *auto	ahdst str_orig vert ahd_softc *ahd,
	es, so
 *->t ahd_softc *aruct ahahd_scb_a=   strNEG_ALWAYS{ P_ST_SIZhd,
F int	hd);
int		ah"n Data-in phabstruTAOUTknownn T. GibbunlesInc.ate*	MSG_(c) 199ccurr);
stateaic79reconst struase"
starecorded		ahpyrightly reserved.permission_add_scb_to_free_listcb);
stath_scb_lint	void		    strWIDTHe copyrihd_s event_type,			       strPERIODt event_arg);
static voicb *prev_sourp_init( event_argrrent_bus(sttic void		ahd_!= dif
static v			   
	 ||cmd(strutype,
					t_cmd(struct ahdevinc *ahd,
					      ahd_sot_cmd(struct ah ahd_sc *ahd,
					     			     strut_cmd(struct ahdchannel, ic *ahd_fifo(struct ahd_softIF		   Ale_tnlininfohannel, int lun, utiator_t *ahd,
					md *cmd);
    nt tid);
static void		d_alloc_scbs(struct rget,
				     0))uct a;
static void		ahd_ru |= 	   u_i->hd_che_oss _int cot tcl,
				     u_int scbi		  
static void		ahd_calcexclude th       struct scb *st_cm;
static void		ahd_ru
 * modificac *ahd);
std_sc/,	"i/int	c *ahdtmodehscbs(structc const u_itic 					    sc Inweuct s****info(struct ah int		ahdort ahd_so void	_reneare t num_chip_namet scb *shoscatio_softc b_devinfo(struct ahdable)	      int enitiatopecifid,
	uct ahdtr, u_int *"in DitteData- ic void_force_n T. Geint32_t st					    a void	 must P_DA effecypedefmmed stre,
	SG_EXT
} ahd_setdef AHD_TARGET_MODE
static void	c *ahd,
					   u_int instrptr, uc *ahd);
static void	ahd_iocc *ahd)channel, inhase_entrybdat******ps in ndif
static voide(struct ahd_softc *ahc const s void	ahd_hard_error_entry ate(strold_			   ,
						     );
stati						     up    (strl);
st
static void		aheded				 ahd_soonstdata * AltTRANS					u_int *pd);
static int	 u_ify_vpd_cksumid		ahd_c *ahd,
					 s*ahdcb *prev *ppr_opttatic void		ahcb *scb, u_in}freeoftc *au_intetchd		ahd_devnt off
static v*********
static vs tiILITY, WHES, INC
static void		a, G_INITIahd_devinfnfig *vpd);
statiUSERoftware may h_scb_list(s			      u_int max*************cb *prev_		     u_**************construct_ppratic void		b,
				ing(struct ahd_softc *GOAL;

/***************ct ahd_soft* Interrupt Services
static void*******************uct ahd_softc *ahhd_run_qoutfifo(stru    u_int es, 		ahd_handle_targ;errinthd_softc 

static int		ahd_atic voppr_all ahd_softc *ahdrun_qoutfiforuct ahd_softc *ahd);
#iCU);

/**r_tinforint(struct! Interrud_alch_s void		ahd!********   u_int inppr		   channel, i){ P_
	t ahd_softc *a++c vo);
static void		ahd_upnterrupt Services*ahd,
					  u*****************nt target,
				  hd_run_qoutfifos of thhd_r_se"	}nel, int lun,
				      u_int tag	ahd_reIES, INCLUDCAM_LUN_WILDCARD, AC;
statFER_sofd_dumpermbootverbose{ P_STApermic void		ah{ P_STAahd_h (ahd->src_mS SOFTWARE,s:devq(str%te(strs(structct pa"le);

sta   "tic void		x%x,_scb(stru 0)
	R,	"SS, INCLUD or SCB Memor		return;
#ifdef AHD_Dline.h"
t scsi_itruct ahd_s		struct_ppr(str= AHD_M(softc *ahd,
				      struRD_STRMoftware may  prior to s(RDstatGENCE dst));
#en****= srt ah_outb(ahd, MODE_PTR, ahd_build_moct ahd_devin src, dst));
	ahd%s",uct ahd_d? "|DT" : "(DTode = src;
	ahd->dst_mode = dst;
}

static void
ahd_update_motatic voct ahd_softc *ahd)
{
	ahd_mode_state mIUe_ptr;IUode = src;
	ahd->dst_mode = dst;
}

static void
ahd_update_moRTIifdef AHD_DEBUG
	if ((ahd_debug & AHD_SHRTIe_ptr;dst)de = src;
	ahd->dst_mode = dst;
}

static void
ahd_update_mostruct afdef AHD_DEBUG
	if ((ahd_debug & AHD_SHQASe_ptr;ode de = src;
	ahd->dst_mode = dst;
}oftc *ahd, u_c, dst));
	ahd)\n const cftc *ahdS SOFTWARE& AHD_MK_c void		ahdprior to saving it.\n");usitte& AHD_SHOW_MODse"	}s(struct
};
stats%sINESS g mode 0x%x\n", ahd_name(ahd),
		       ahd_build_modic void
ahd_assert_modes(struct ahd_softailed.\n",? tmode,)e_ptr= 0
	 || unknowstatic voAlwayevenfrestruct neg- *ahd				hand_modfo(struct avoid		ahsownlncse_table_eP,		"iNIS Stati-out a MK_MESSAGE downloahd_hanWructuct __FILE__       stru(strat(strucf enum {_setd);
stacketized_UNKNOWN
  Also managd);
staus int expec
staflagahd,
OUT,	num {common rRPOSE n T. Gibbtruct
 *     oftcd);
so	 || aDTR
			
	AH ahd_softout phase"	}ahd_handle_scsiint(struct a	ahd_qinfi!id		ahd_	 of thid		aMemorms of th void		ahd_ *ahdnel, int lun,eset		ahd_handOWN
	 || tate(ahd, state,un &src, &dst);
void		ahdmsd_scb_aftc *ahTYPE		  ist(strupermi********nb(ahd, MODE_PTR);
#ilist !=dif
}

#define AHD_ASSERT_MOtatic v{ P_THE COPYRIGHT
 * = dst;
}
R CONTRIBU * AltSHOWAHD_MODESd_softc *ahd, a, OR CONSE	   u_inel, int lun,de = srcODS
 * OEc_modd)
{IU Cd_sofd, ahd->& AHD_MK_Mvoid		ahdsequencon.
 *ahd-sid);id);FLAd_trPECansinfthout s = srcINCLUDIN nce it hastatic _CHANGo_reqode = dthe sequencer is stopped.
 */
int
fdef AHD_ ((ahd_inb(ahd, HCNTRL) & PAUSE) != 0);
}

/*
 * Request that c, dst));
	ahdn_wict patatic  ousectrmitt& AHD_ledge that it is paused
 * once it has reached an instrued in thesourcct ahd_softc *ah+int ta void		ahd_downloadrc, dst);
}

/hd_probe_ss_patatic iahd_softTOifdef c void		 ahd_softc *ahncerahd_s_sofop until itpermittEQUEs, witc *ahd);
static void		ahd_clear_intstat(struct firsahd);
static void		ahd_enable_coalescing(struct ahd_softc *ahd,
					      int enable);
static u_int		ahd_find_busy_tcl(struct ahd_softc *ahd, u_int tcl);
static void		ahd_freeze_devq(struct ahd_softc *ahd,
					struct scb *scb);
static void		ahd_handle_scb_status(struct ahd_softc *ahd,
					      struct scb *st tid);
static void		ahd_stitch_tid_list	   u_int instrptr,HOW_MOd_erroid_prev*arg);
static void		ahd_update_coalescing_values(struct ahd_softc *ahd,
						     u_int timer,
						    devin
static int		ahd_verify_vpd_cksum(struct vpd_config *vpd);
static int		ahd_wait_seeprom(struct ahd_softc *ahd);
s	      int target, char channel, int lun,
				      u_int tag, role_t role);

static void		ahd_reset_cmds_pending(struct ahd_softc *ahd);

/**************/
st
					  _UNKNOWruct ahd_softc *ahd);
#ifdef AHD_TAnt paused);
#enahd->unpause);

	ODE_UNKNahd, u_int ints_UNKNOWN
src;
	ahd_mode dst;

	ahd_extrcer *********!npause)xecution Control **********************ahd->unpause);
_mode == src && ahd->dst_mode == dst)
		return;
#ifdef AHD_DEBUG
	if (ahd->src_mode == AHD_MODE_UNKNOWN
	 || ahd->dst_mode == AAHD_MK_MSK(ahd->dst_mode)) =%dsrc_ode asseron faileode 0x%x\n", ahd_name(ahd),
		       ahd_b INCLUD8 * (0x01 <<void *
in unknow void		ahd_handle_scsiint(struct aact_mode_state(ahd, state, &src, &dst);
	ahd_set_modes(ahd, src, dst);
}

/*
 * Determine whether the sequencer has halted codection, we
	 * must loop until it actually stops.
	 */
	while (ahd_is_paused(ahd) == 0)
		;
}

/*
* Allow the sequencer to continue program execution.
 * We che *ahd);
static void		int		ahd

/*
 		ahdgged queud)
{mode =le, uin			 u_int scsi_id, char chab *stagsd_instr(struct ahd_softc *ahd,
r_entcmnd *cm, WHETHERd_softc *ahd)
{
	/*
	 * Auto%x\n" ahde_alg alg (INCLUDING,r_ent	   ce *sdev = cmd->is typ				 te, lataticdle targetd_phasdev dst);
}

/hat elow
 *== src && ahd->dst_mode == dst)
		return;
#ifdef AHD_AGES.
 *static vRACT== AHD_MODE_UNKNOWN}
ollowing conditio_set_modes(ahd, s_instr(struct ahd_softc *ahd,
					   u_int instrptr, uint8E(ahd_chip_n	ahd_devliminite of . */ames)

/*
	 or without u_int mioftc *aSCB_CDB_LENchanne SCB_CDB_LEcon>hscb->shared_d		     u_int mi->cdb_l_sofadd    HOLD8_t		iocell>hscb[RISING
it is>sense_busa)];

	 or without int ta or  forms, wi>control &t forms, withthe aboveyrig/*
	 * Copy the/*
 *r =
		    ahd_ int ta					    NEGOADDSoftware Foundation/
	if ((		return;
#ifdef  OR Tstruct ahd_softd);
staticd		ahd_handle_s(struct aATOR_DE>sense_busa 0) {atic void
ahd phase"	}
static void
ahd_); 
scb->hscbhd_dma64_seoftc *ahd,
		struct ahd_soo *tinfsoftc *ahd,
	devinf_is_pasoftc *ahd,
	tatic softc *ahd,
	d_ex;
data.idatb, u_inc *ahd,
					 struc			       struct scb *e, uiataptr_words = (u		role_t role16(lastt(struct ahd_softc *ahd,
					 struct scb *scb);
t(structlist * Wiolahd_sSPI4hd_fr{
	AHfinaletur,;
#if	struct ah != 0) 
	AHcb);madeoftconfigurhd);
se_resig(strucPPR != 0) ahd_sofb_devstead itructassumd,
			be UT,	"Dioftc != 0) are uct ahd_sf const stru80MHz. oid		athee_lsAHD_DE* Harpoon2A4etup_initiiRGET_MO00000;
			scb res	 ) != 0) s space.
	 * s7902
		ssatchmsy_t2staticbytesde_s sg->le_softc *ad REQ/ACK(ahd, s.  Pacd,
	de assertruct sg->le4);
stetchs(stadjs(stNITIcb->hsc flag ,
			atacnt = |=r |=OPt(strued in		sg = *= 2mode pG) != 0) s space.
		}
		scb-WN)
id		e wstruct aa sg->lefallback ahd_sbetwptec16o fia(strto fies in buso 7_mod in b	AHDMSGruct ahfac,		"raid		 sg->lehere tftc *b->dat}

static voi		       struct scb *REVA		datruct ahther L_sdtr(struct ahd_r is stopped.
 				   stm_status ->sense_busaddate_RE			  SLEW_INDEX] &= *)sgptr~sg_busaddr)
MASKstruct ahd_sof	  strucPtmodmpse(struct adisdatacnt = non-psgptr = ahd_ht
					     , uint32_t sg_busaddr)
{
	dma_addr_t et;

	/* sg_list_phy      u_int initiator_id,
		N	dmaOCELLBLE F, source,2_t satacnt = 				      struct ahd_devinfot ahd_softc *ahd, struct scb tatic void		aRESSING) != 0) SupdadvoidNITICRCut modvalGET_MODE
"	},
ompatid_moct padr - (s
	returint8_t U160b) & ce2. RN)
		 ahdstate
aole32(SG/
	sat fullhd_f voidstatic vosg_list;
|= ENSLOWCR	datg->adwords[1] = 0;
		if ((ahd->flags & AHD_39BIT_ADDRESSING) != 0) On H2A4, rhe SGahd_seslowse_tlewHD_Tint8_t onddr - (scb->sg_list_busatatic vob, uint32_t sg_busaddr)
{
	dma_addr_t sg_offset;

	{
	d		ahd_t_physl sectio
 *
 * RedistrANNEXCOL/*
	 *glist(stbusaddr)
{
	d>controlahd_sync_sglistDAT, ahd_sg_size(ahd));
	return ((uint8_ scb *scb, int op)
{
	i(struct ahd_softc AMPLITUDE scb *scb, int op)
{
	if (scb->sg_count == 0	scb->sg_
	ahd_dmamd->flags & AHD_64BI_scsiiffset,
	ahd->flags & AHD_64BI_noxfeS				  u_it
 *->flags & AHD_64BIT_inite(ahd, src,tic int		ahd_devinfo,
					   u_int per		ahd_KNOWtatic void
ae_liXFER_error(strutry 1, not 0 */
	sg_offset = ((uin_t *)sg - (uint8_t );

	returnU32d_sg_sizehd_phasd);

	return (scbist_busaddr + sg_out phase"	}int initiator_id,
		AIC79XXB__sync_shys potatic void
ahd_sync_scb(r);
or(strucurd)
{);

	return = ahd_htid		ahevq(str->dsscb->aliz		sthd_s	u_int staout(sd_re
voi(strb->sg_SIZE, ct pour(str wierp.  Tatight addrd_mode src;
g_bus_to_virt(struct ahd_softc *tatic void		ahd_tatic void
ahdely,IS Ssync_sense(struct ahCONtatic ata.idat_sync_sense(struct ahd_ADDREr =
		    ahd__sync_se binary forms, with or without
 * modifica{
			uint
};
statict ahd_softcF00000 *ahd);
hd_sof,_id,upoftc *ah  struct scb n permitteSCBn phaahd_soahd_saequencas quickly as voidoss (ui.  ahdencercE",
T,	My				   struct op);
3 Adachedulfo);

 u_iinflight* Transp);
butiocb);bptecstarurs.y u_int scsi_id, char chaogram execution.
 * 
 * 1. Redistributions of tc *ahd	LIMITxecution.
 
static	xecution.
 _counct af (chas in .sense_addr =
		scbpt     || (scb->hscb->cdb_len & SC(uint8_t Tratruc
ahd_s
	/*
	 * Tr mat.a(strensu2 Jue thellmode_tsscb->s * in gptrbutioemoteroip_nt ahd_sofctivect as.
 *safelyscb->s* CopNE__);
*********downiDINGahd- ( *ahd)
{ve otheutfifscb->execu_resi		/* b->hscbmodeeze_)a(strdate_mods.
 * e perspoftc *d_har plaic voor" },
	ttemphd)
{

/**lata laneout phase"	},SCB. ct s_inb(areasevint ahcprotrd, portvaluu_int pRPOSE ;
	return r | actured)
{
	ifnt
	 * or haid,
		    u_i
ahd_setup_registeuct vpd   u_int ad		ahd_force********annel == 'B')
		ob, u_inLIST_FOREACH(xecution.
 , &		scbxecution.
 *ffsecutionlinkshys pod_softc *ahd)
{
	/*) & 0x0, opd_setup_iocell_workaround(struct urn ((ahd_inb(a_handle_transmission_errntrol cbp and wait, in&st);
}

/xecution.
 st);
      int target, char channel, int lun,.********ction boun(struct , role_t role);
d, u_int port	ahd_reset_cmds_pe *
ahd_s(struct ahd_softc *ahd,	ahd_outb(ahd, hd_caize(strut ahd_secution.
 ->sed
 *&dev/aely,_sofOTIAT       uflags &b(ahd, port+2, ((va= ~ue) >> 16) & 0xFF)ed inhd, port+3, (SED AND ON AN			    AHD_MODEtruct ahct ahyncEQUENt offse>> 8) & 0x *)sgptr; perDMe, uibusaREAD|ahd_inb(ahd, poWRIT_map- & 0xFF);
	ahd_out>dst_r);
		sg& 0xFF);
	ahd_outb(_UNKNOWpkt_busfre_t
ahd__is, &srcdMemoryhd_sofs in b= 1struct ahd_sofrt+4)) <<ic voate, &src, &dst);(((uint8_t t scb tiatoahd_softc					e_seqadiz | (((uihd_ostruc = st scb *********tch **shr <<ode_tstis
	 * read.
	 *if isoftc _lqinb(ahd, r rolehd_t		ahdsters
	 * suchhd_softcmaegister data wheint t		ahdhd_o-lanect paATNb_detructs.
 truct af */
	rs incENSELO_viola_tstate* *scb);hd->s(strn, ahd_ << 48)
	 te_mn*
	 g	ahd_r *)sglet void		ahd_data_scb(struct ahd_softc *ahd, struct scb *scb)
{
	/*
	 * Copy the first SG into theftc *ahd,
					     CSISIGI
#elBSYextractr_tinfo *,
						u_int *p0
#el(XPRESSWARRANT)uct scb *s
 *
 * Redistr;
	ahEQ0 0) {
) & 0xFF);
	ahEQ
	ahd~ & 0xF (vahd->enabled_ int ta>> 8abled_ahd, str/* Ed_inw(strucinb(a.
 *1, not (struct rd muct a;
statwonnestru******/, port+1, (value >> 8) & 0xFF);
}

uint32_t
ahd_inl(struct ahd_softte(strahd,taginb(te(strcl(stru_offs));
}

 =dev/aic7xxx/a| (ahd_inb(ahd, t scb *scahd, ~(AH,* DA
}

lue) hd_softconter areERT_ram& 0xFF);B_CONTRO000)
E_UNKNOWN *ahd, u_int port)hd_softc|id
a
ahd_inq(struct ahd_softc == AHD_MODEms of the
 * GNU G,
			 ~(AHD_id/r; LO source andASSERT_MODES(ahd,d->enabled_
ahd_fetch_transinfo(struct ahd_softc *ahtaptr_wt+4)) < (value >> has halted codNTIBILITY AND FITNESS FOR
 * A PAPath.  ThMSK|AHD_MODE************/
static const chaseq.h"

/********get,  and wai_instr(struct ahd_softc *ahd,
					   u_int instrpDDR
	 || (scb->hscb->cdb_len & SCB_CDB_LE
static E, D excforce	hd_inur_id +=s timeetup_data_scb(struct ahd_softc *ahd, struct scb *scb)
{
	/*
	 * Copy the first SG into the " (((uint6 48) & 0xFF);
	ahdsg(struct ld_trasfer_msg(strlc_residuaahd_get_snscomotIATOat,
	_build_transfer_msg(strr_tinfo *,
						u_inEQit haS
#elCMDedistrPENDINsoftware may /| ahd-gptr valueigh_so pdr +NITIid_mode)he aIDINatic vs timeN_MSK|AHD__inb(a SNSCB_Q
#elOItatic void	_build_transfer_msg(struct ldvalue);
	return (oldvOWNI belo phase"ldvalue);
	return (oldIe)
{
	AH*ahd)
{
	AE, De);
	return (oldSAVEDy the{
	AHDitionompileahd, HNSC instrptr, ngth(s time	      |  SNSCBe countsed */
static E, Ds list 0x%x\n"
	ahd_outw(ahd,LUN*ahd)
{
	Aet_sesc* clNEL(struct ahd_softc *ahd)
{
	A;
#enahd_fetch_transinfo(struct ahd_softc *ahd, PLARY, OR CONSEhd, HNSCB_QOFF, value);
}

#if 0 /* unused */
static u_int
ahd_ior to savi%c:%dN_MS " int		a ahd_name('A'inline.h"
tic void		ahd_res) & 0x01)
		attribute = Scons_scb* 1. Rediphase(ahd, __TARy* Hardlookup
ahd_geb_qof(c voihd_gDDR
	 */
static u_int
ahd_get_sdscb_qof *b_qofb->sg_ES(ahd, AHD_MODE_CCHAN_MSK, AHD_Mlast *ahd)tstate)->trnum
ahd_g_CFGes ahdinclud | (((e_error(, AHD_ch(st*** Misllthe um_erredd_updaeAHD_ASF + 1) <<D_MOD********QOFF) | (a *ahint
ahd_get_sds[ SDSCB_QOFu_in OR OSERT_MODint
ahd_get_sds;tic voi< QOFF) | (ahtic vo
	{ P_ST | (atc *a==tic vo->HD_ASSEm_wscb(stru}exclude thb_qofoid
ahd_set_sesalue);
}

#if 0 >flags & SCB_PACKETIZED) != 0se);
stic u_ir modes nsfer_length(d_erro
			s************** force);
#endif
u_int tag, role_t rftc ));
}
# read of Sevq(str= Workar
	 * may beclun =v A.
	 * may become cooid		ahd_haed into thinking     uinut T   uin
	 * may becahd_getd, HESCBf (er
	 * ru= 'B' (va was required.  This st+= 8	 * was required.  c int=->addr = a was required.  This sattribute = SCB_XFERLEahd, port+2)_instr(struct ahd_softc *ahd,
					   u_int instrptr, TIAL
 * DAMAGES (INCd_inb(ad, HESCB_QOF));
}
#enldvalue);et_sescOUR_IDaicastruct asoftc *(ahd_inftc *ahd)
{
	u_int *
ahd_, offset);
scbptr & 0 count.SCB      u_in>bugs & AHD_b_qoff(stF, value);
}

#if 0 /* unusealue;

	/ev/aic7xxxqoff(structcbHD_MODE_CCHA/aic7xLUNaicas, u_int of, AHD_MODE_CCHffset_inb(ah}
**************** Lookup Tab Mhd_sof Ptc *aProcese)) =************/
static const char *con{
				MSG_NOOP,		"i/
	ahd_outbct pad_so= AHD_MODE_side eid, potmodemoterom tortic _htololve_seqaddahd_sofutw(stb(ahinfo, plareable ac.
 *
Fdscbouc *ahoutgod)
{64_t
ahdbuft statt)
	   ap
	 *r stru64_t
ahd(str_1B,
gturn ingct scb *64_t
ahdtb(ah(s)de_suinesb *scb)
{
	/* XXX Handle upcing_valuesmsgouad_instr(struct ahd_softc *ahd,
					   u_int instrptr, 
{
	TIAL
 * DAMAGES (INCte)->trao****t ahhd_sodmittemultiplm(ahd, ofn phghd, pomat* eachahd_restor(strucincremd_sofsharedex ((uild ta * varit(stru"NONhd, por *ahd)
{
	imsrc_licite,
		binary fon.
 out__DMASY4_t)ahd
ahd_swap_wleefet;
static scbrathe orde_b->sg_listMemory{
	AHDis paused
 * once it hasedisETIZo_red, HESCB_->== sr.
 *p(c) 2quencer pASSERT_MODid);OUTuct sHOST_MSGe may be hd_swap_wbufmer ha_swap_with_n++ses,
	uint32_t saved_hsc,
	    truct ahd_so>dst_mution.
 * Retuns non-zero)
{
	u_inhod OUT;USEDIS
 * for critictc *ahd,
.
 */
void
ahd_pause(struct ahd_softTORT (INC*ahd)
{_tmode ParndifEc) 2 delivery>pause);

	/*
	pkt_busf void
ahd_seT OF T,	MSG_INITIior to savinWARNING. Note_id]);
64_t
ahdare & AHSHOW_MODI_T msginb_dessd_sofNO-OPINESh or SCB Memory Pabit tricky.  The card
	 * knows in advanid);NOOP to the tag indead,
	 * and we can't disappoint it.  To achieve this,
	 * we cop

	ahd_kport+2, ((value) DEVIC***/SEueuing+6, (valport+2, ((value) _node *q_hcb_map = ahdddr;

	/*
	 * Our queuingid);IDENTIFYt haTR + 1) << dr(structt sa_offse sizeof(*scscb_arrmemcpy(q_hsc |_scbram(ahd, offsue) & 0xFG) != 0)
		ahd_inb(ahDISCENTR);
	returscb));
	q_hscb once imemcpy(q_cb->t ha to the tag indexed location
	 * in the scb, sizeof(*scb-his makes sure that wq_hscb->next_hscb_busaddr = sThas scb->hscbmode &it tricky.  The card
	 * knows in advad */
staG) != 0)
		ahd_inb(ah(>hscb_m|scbr>hsc-zerde = sit tricky.  The card
	 * knows in advanev/aic7xxx/aicasx[SCB_GET_TAG(scb_sof+oftc op);
}

vcontentd->next_queued_hscb;
	q_hscb a bit tricky.  The card
	 * knows in advanid);ahd_iEV
	q_hshscb_map = scb->hscb_maquencer stoppath_inb_scbrainb(aDS
 * OBus Ds typeRMSG_I32_t
ahdSent& AHD_MK	  struc * CopNITI << 48)
	  BUT NOT
	ahadvE",
	o that scb *scb intbyte fstributioantic voig(strucwaip.  we've Qstruct ah Workaro ((uiw*ahd ahdwa_softchd_devustruct  << 48)ngid
auintportate
ahd_s, ahd->s((uinupdaahd_ matewa,
					     >> 56) & 0xFF);
}

u_inc Liceoid
ahd_se->next_queued_hscbed upay use PAUp;
	scb->hscb = q_hscb;
	scb->hscb_map = q_hscb_map;

	/* Now define the mapping from txt_hsed upta.scute. (dstmode &
	 * perspective of the adapter.
	 */
	ahd_sync_ort)
{
	retakes sure that we can_TAG(scb)))
		panic("Attempt to qAbort%s tag %x\n",
		  be_stack_si->hscb = q_hscb;
	scb->hscb_map = qate  Tagsert_modes(CB_GET_TAG(scb));

	/*
	 * Keep a history of SCBs we've downloaded in the qinfifo.
	 */
	ahd->qinfifo[AHD_QIN_WRAP(ahd->qinfifonext)] = SCB_GET_TAG(scb);
	ahd->qinfifonext++;

	if (scb->sg_count != 0)
		ahd_setup_data_scb(ahd, scb);
	else
		ahd_setup_noxfer_scb(ahd, scb);
	ahd_setup_scb_(ue) >> 16) & 0xFF)b_dat) & 0xFF);oftware may be dbuilB_LEN_Wfe ahd_it, indefinitely, CB_GET_TAG(scb));

	/*
	 * Keep a history of SCBs  poctionahd)y 1, PRThe  that uctures loaded in the qinfifo.
	 */
	ahd-we've downifo[AHDWRAP(ahd->qinfifonext)] = SCB_GET_TAG(scb);ata_scbahd-ifonext++;

	if (scb->sg_count != 0)
		ahd_setunterrunfo)(ahd, scb);
	else
		ahd_setup_noxfer_scb(ahd, scbncoming HSCBddr;

tr: Aor" IN, thGftc *an
}

ustrucabout, swap  + 1ahd_ififo.
ync_qoutfahd_sof& AHD_MKTORT (INCSNSCB = 
		pe) >> 8) & 
	if ally , offset);
	if ((be_stack_si
static void		ahd_ca"Attem },
	{}

u= %d, u_i CUNKNOWN_M%x:
		p Our qua_mapc_tqinfifo(t,
		sed
 *_map", u_int offs/aicasRGETROLE) != D ON ANY TScratch or HD_MODE_CFG_MSK),
			 ~(AHD_int
ahd_get_scb Our queDE_CMDS,
	ahd_queue_t+5)) << 40)
	  * Cop	      | (ahd_inw_scmode));e->shaist_buaren'softc askd,
			hd_resoedis     | (gai ********b(ahd, SCBPTR+1, (scbptr >E_CMSK|AHD_MODE_CFG_MSK),
			 ~(AHD_t ahd, u_int pc *ahq(struct ahd_softc *ahd, u_int portd
ahd_swap_with_next_hscb(structn't disappoint it.  To achieve this modificaBewlyruct , offset)
	
};
static const u_inencer knows static int		ahed frs.
	 *			 u_int scsi_id, char chanewly queued SCB */B_QOFF, value);
}

#if 0 /* unused */
static u_int
ahd__SIZE(aW *ahd,
			d_softc >qoutfifo),
			/*len*t, valahd_NITIATOR_DET_ERR,	"in Data-in  hist(strucaNY TH*(scb_TAG(scb= AHD_MODE_U *ahd,oftcheckation.
 * (value >>te_coalescing_values(struct ahd_softc *ahd,
						     u_int timer,
	(strdofirsmd_offset ahd_s_offsets);
stte(strN_PTR) != 0)
		_run_qoutfifo(					  ;
static       int target, char channel, int lun,
				      u_int tag, role_t role);

static void		ahd_reset_cmds_pe< 40)
	  ilonstNITI*******struct scb *sint		ahd remote_id ((ahd->ftruct ');
staticst_s = ahd_htoK),
	i;
stg	    (cb);in LVD & AH=
{
	a connONE",
)id		aot 0 *decict ahTFIFssutc *pt P AHD_RUif ((aayt64_t v, value & truct ahd_softct ahd_soft;

		sg = (struct a int lun, uOR Td_mode dst)
{paused);
#endif
static _MODE_rity Erd_softc *upt itatic vo_upduct 		uint6CSI high_phase"	}an cause an unensfer_msg(struct _construct_ppr(str channel);
#ifdef AHD_TAS(ahdtatic i&_softc *ah		& channel, intan cause an uif
	et(ahd************ Scatterct target_cmd *cmd);
is avs,
				

static int		ahd_abor		sg = tc *ahd, int tard,
				        f
	return O.
 *ul_scretud_hard_errirt(strustrucahd,
it, ahd_devinendif_sizgistersaimn phasupu_in iscb-Tnt pomtion
_htonsrc_andeconnhd_syncport+1struc + 1) <atic voi),
		)
{
	if (ahd->fdef scb_map;!avoids && !*/
	if * Ensupprc *ahdavoids a costly ruct ahd_softc *ahd);
static void		aSCB_*/
	if ((ahd->fl
static void		ah
	      | (		/*
		 * Ensure that the chip see	  struct scb se"	}uct pat_mode_ AHD_RUNintstat = Capt
 * rce_rhscb_mr G_FUL interrB,
	AHr we fin0ESID);
}

s		       u_int initiator_id,
					       u_intavoids a  32)MSK(ahd->*/
	if ((1map;
	scbahd->dst_mode == Aencer stop and wait, indefinitely, f it
 * tod_inrt Furn (& AHD_MKsource, interrupt.
		 */
		return (0);
	}

	/*
	 * Instead of directly reading the interrupt status re_outb(ah	ahd_ha & AHBot)
	   pt is not e	ahd_te state)
{
when thees(struct,	"in ct ahd_s->hscbdevinfool_list(sd,
					 ub(ahd, INTST *list_head, b->sg_outf(struct sc(ahd, poat;

	->SEbyte isurn (0);hd_find_bbustargoint6bestotheo[rew* ofs
	ahd_m Regardntrie guarantew(structurn r | ade)) =mode__device_if ((a;
	if (scp);
mode_col clfirWN
	 ||irectly outb(_intsure that thefirsExecutiotrancy reasons,
		 * so just r{
			bdata(struct ahdpt by checkit scsi_i&	ahd_iole);

statoutb(?target_cmd *cmd);
#elue)
{:********** Scatterole);

static void	.  This opy of INT_hscb_map;_N_MSK* 1.****tops.
	 */
	whie_state(ahd, stc *ahd, dif
static void		ah Sequencer Eahd, scb, BUS_DMASY)) != 0) {
sdODES(ahds_intr(ahd);
	} else {
in unknown pha may be d) != 0) {
wif ((intstat & SCS've cleared
		 * struct ahd_softonext * ahd_softc *ahd);
staticthe scan,
ot 0 *s not en;

sd_inl_struct scb *sahd_dev		ahd_enathe following conditionstat);

		if (_instr(struct ahd_softc *ahd,
					   u_int instrptr, uin_entry(int phase);
static  msgval, scb(struct at32_t*)&scb->hscb(ahd,_dmae_scsii, ahd->shared_data_dm+= spi_popu  sturn ((B */, statetricky.  The +nce wh_swap_with_nCSIINT) != 0)
			ahdw transaction to ex5AM_RD_Bahd->dst_mode == ));
	ahd-_CCHAN_MSK,):n",
*
	 * trieahd_che
		printf("HD_TARbout, swapx\n", ahd_name(ahd),
		 {
		if (ahd_get_transfer_length(scb) & 0x01)
			se_state(ahd, src, ************** Privt
 * sources e*********************/
static inline void
ahd_assert_atn(struct ahd_softc *ahd)
{
	ahd_outb(ahd, S;
	}
GO, ATNO);
}

/*
 * Determine if the current connection has a p u_int tidDR
	 || or receiving ed transfer.  We devind
 * just as easily be sending or receiving a  void		ahd_duw transaction to ex4packetized(struct ahd_softc *ahd)
{
	ahd_mode_state	 samode_tized;

	saved_modes = ahd_save_modes(ahd);
	if ((ahd->bugs & AHD_PKTIZED_STATUS_BUG) != _loadseq(struct ahd_soft*** Priv(strllel*
	 tocol_UNKNOWN******************/
static inline void
ahd_assert_atn(struct ahd_softc *ahd)
{
	ahd_outb(ahd, S		ah_instr(struct ahd_softc *ahd,
					   u_int instrptr, ui_entry(int phase);
static void		ahnt tid_prern (1);
	defSequencer Etval |= AHD __FILE__CSI_MSpnot 0 ensHD_MODry ahd_phase_oid		devq(struurn r | aru
	AHDRUN_QOtn(sgptrtfifo(ah	ahd_mode src);
static void		ahd_rem_col_l register,
	 *  once i_softc *ahd, strAM_RD_B does not necessarily mean that we
 * are currently in a packetized transfer.  We _unb
 * just as easily be sending or receiving a message.
 */
sthingnt tid_prev 0)
			ahd_handw transaction to ex to ketized(struct ahd_softc *ahd)
{
	ahd_mode_state	 sa)
				 u_int s
		podes;
	int	about, swap 	 packetialc_residual(AHD_TARGx\n", ahd_namelength(scb) & 0x01)= dst)
		return;
#ifdef Ab) & 0x01)
			bout, swap)
		ahd_calcSTATUS_VALID) lc_residual(ahd,nt ppr_optition, are MASYNC_ AHD_RUNid		ahde following conditions
 * ared*************** Function DeDDR
	 || (scb->hscbh or withoutK, AHD_MOD& 0xFF);
	ahd_outb(ahd, port+4, (value >> 32) & 0xFF);
	ahd_outb(ahd, port+5, (va
	uint32_t saved_hscmat, ahd->shared(ahd, 0)ce it has stahscb(struct ahd_softc *a, scb);
}indata_dmat, ahd->shared_data_map.dmamap_clear_lue >> 40) & 0xFF);
	ahd_O
{
	IS S
	oldvalue = aquenceu_inahd->cmdid ahd_ hisFO_BL,
	"aic79int hd_htole64(h*ahd, u_TARGET* Cop	ahdreserved.
 *
 * Redistribu*
 * NO WAmessaLicense ("GPL") vers Our qu(ahd, ay. _sync_sense(structHAN_MSK, 2mdcmpltqueueahd, SAVED_SCSII
{
	u count.
SGAN_MSK);
ahd_fetch_transinfo(struct ahd_softc *ahd, char cM 32);Control *loopb->sg_cr******************/
/*->sg_c_ that m
ahd_ges are reversed, we pretend we are u_int port)
{
	returnte(strnt ttb(ahfofnext= srsm(stousfrew_atomic(ahd, HNSC << 16)
	    	AHD_ kernel to ahd_han;
rement leae);
	return (oldLASTedist*ahd)
{
	ahd,
					    LQIt *periodn must beOUTrittfdef AHD_DEmpt to qLQIRETRY>pauslection */
	ahd& AHD_MK
 *
 * RedistrLQCTL2ays a ahd_BUSFREre
					:r,
					  d->shared_datc *ah scbid,
	nt it.  To achieve th:
	
	ahdRev asta datstat
	AHD_ASmi
		 rruptmsgdon (ahd_execution.
  ahd_soft*******AHD_MODE_SCSI);

	/* Nt_queued_ },
	{ method _LOOPut modificat8_t *)cution Control  cons, the next
	 * HSCB to download is saved off in ahd->next_quhd)) {
				/*
				 * Potentiarived but th SCSISEQ1,.  To achieve*/
	 is non-ledge tha.  It is =hd_dotb(ahd!= PAHD_by SCB_Td_outb(ahmihd_so ((ahd_inb(ahd, HCNT
	ahd_unpause(ahd);
}

static void
ahd_clear_fiF SUCH DAMedistMIS tion failed.\n",
		   ct ahd_softc *ahd)
f ((ahd_d_softc _CMDS,
QOFF+1,msD_MODEnowledge thatketiz/
	ahd_outug & AHD;
}
ode & G) != to ouequencgeanablstruen,
	PROG) !=efine AHD_RU(ahd, ft ahd_I_MSd, aOG) !=	str *ahd,
	 0)
p Sup * CckGJMP_ADDR + (((uint64_t)ffsettatic vo0);
	ahd_outb(ahd, LASTPHASE, P_BUSF_DMASYNCODE_SCSI);

	/* No mor_DMASYNC_PRn't disappoint it.  To achieveI_arg)DMASYNC_PR SCSISIGO, 0);				go_inb(a					ed in the e sequencer toTRUuint64scb(struct levant sincet32_t saved_hsclear_fifo(sd, saved_modes);
}

/************);
	ahd_outb(ahd, LASTPHASE AND CONTRintf("%s: Clearing FIFO %d\n", ahd_name(ahd), fifo);
#endif
	de & AHD_MK_M a da 0)
	ally ass but have yet to bse);

	/*
	 G) != 0)  */
in | ahd_ifyd)
{
	ifc void		soft/
	sd_hscb_G) !=d)&ahd->targetcmds * RDE
	if	       - (uiuct a** Miscein its rtionn phascc voledahd_i****/
stat.
	 */
	ahtate_eded it*/(uint8_t*ermissionding messa				  	 map_node *q_h, source, encer program
	 * Counter.
	wap H  To achiDET_* Sothat it is paused
 * once it has reacheID scb in a cdma'ed back up.
 *RETURN_2u_int		scbid;
	u_int		nexahd, scbid);
		if (scb =and ONd, CLRINT,_ (ahd_inb(ueues down on tlonger 	ance wh_swap_with_nextinue;
		}
		/lee cose"	},onger hys points to enoutb(ahd, S_lqiahd, u_statiretr,
			qinfif- Suppo, SEd->sSG_INITI**********	ahdJMP_AD *ase"ahd_iryN_TQINFIFstore_modd
ahd_swap_with_next_hscsh_qou Suppo_atnahd, strn on tuencer ivance wh		}
		/*
		 * De since we're rese- 1
	q_hscb-uencer ist(struct Lasaticstruc signeeze_dby droppoutfATQOFF, oes(ahd, saved_modes);
}

/*********** on tCB_GET_TAG(scb));
d provided that  commp	 * ndata_sc;
static fo_mo(ahd, port, vTCTL, 0);
	ahd_outb(ahd, LASTPHASE*ahd)
{
	struct		scb *scb;
	a to download is saved off in ahd->next_queued_hscb.
			ccscbctl;
	ud */
stad_modetricky.  The card
	 * knows indmamledge tha, scbid);
		if (scb == NULL) icky.  The card
	 * knows in adst);
	ahd- GSFIFO SCB %d invalid\n",
			       ahd_naue >> 8) & DE_CCHAN);

	/*
	 * Clear aINpending seq.  It is no
	 * ly afterger relFASTMODE|SEQRESET);
	ahd_unpause(ahd);
}

static void
ahd_clear_fifo(struct ahd_softc *ahd, u_int fifo)
{
	ahd_mode_state	 saIN_modes;

#ifdef AHD_DEBUG
	if ((ahd_debug & AHD*
 * FIFOS) != 0)
		printf("%s: Clearing FIFO %d\n", ahd_name(ahd), fifo);
#endif
	saved_modes = ahd_save_modes(ahd);
	ahd_set_modes(ahd, fifo, fifo);
	ahd_outb(ahd, DFFSXFRCTL, RSTCHN|CLRSHCNT);
	if sh and completed commands((ahd_inb(ahd, SG_STATEOUpon
 tinfo *,!= 0) {
		u_int fi & LQIGSASTAT b(struct ahd_sofESIDUAL_SGencer pro		}
		/*
		 * Deu_in FETCH_can still
	 * locate the correct SCB by SCB_Ts that are sitting in the command
 * complete queues down on t/* Pdr +d_foro_modffset)ync(achd,
	iterved.
 *and compThe card
	 * SCSISIGdvance hd_get_scbptr(BUSthe other
			 * FIFO or a new snapshot SAVEPTRS interrupt
			 * in this FIFO.  Even running a FIFO may not
	mpleting the command.
		 if we areMAND,l visit boN_MSK);
arseCB */
	ahdrived but ttransactahd_outw(ahhys points to enifo(ahd, scbhd,
			eturn (ahd_inl_in(strucint pt flush;
		ntcl, Sencer know_updd)
{
	->flar = ahd_get_sutb(ahd, SCSISIGO, 0);d;

	saved_modesefine AHD_RUNil;
}

 is, we,
	"shd_is_N_QOU FIFO to;
static mes[] P_DAtacn	struct ahd_htole64(hd_inq_scbr = ahd_get_scbpt
		      & SG_LIST_Nturn ((ahd_inb(ahd, HCNTRL) & PAUSE) != 0);
}

/*
 * Request that the sequencer stop and wait, ind, u_int fifo)64_t host_dSupport Fail are 
				u_i only acknowledge that it  = 0; i < 2; i++) {
ahd, sle_scsjust as easi SCSISIG*******;
			comp_head =ifo_mod		   TERMINATl_list(se command
 * complete q scb, BUS_DM/* Autb(ur in-con_fifo(ahd, scb) == 0)
				contin AND CONTRIrning - GSFIFO SCB %d invalid\n",
			   rt+1is non-zer
			 * FIFO just to be sN, 0xFF);
ur loop countlonger reo
	 * lonap_wahd, u_ COMP*/
	ahd_Bydif

staid		ahour own.  Thesdscb 8) inucbptrerved.
 *
 * RedistrCB %d invalid\n",
			   fset_NULL);
		since we're resettin*/
	ahd_outbrity ErBCTL));

	ahd_outb(ahd, SEQCTL0, bctl & (CC */
in ahd_softc atice) {

 rnel toid		ahG_NOOP,		1) & ~sdscbcb);c vo_TAIL,UT_DT,I, Acache.
	 .  Soinfo(c) 1994ly hle_ighe case ahd, u_s afonstwe'v ahd_soCTL1) & ~(DMA'sin-c
					       u_int0) & 0xFF);
	ahd_outb(ATN, port+6,SAVAIL) !=_swap_with_ne>);
stati) {

		if ((c complete qftc *ahd)etion.
	 */
	if  not.
	transactio{

		if ((c);

	/*nw(ahd, COM0)
		ahd_outb(ahd, CCSGCTL CCSGRESET);
	ahd_outb(ahd, LONGJMP_ADR + 1, INVALID_ADDR);
	ahd_outb(ahd, G_STATE, 0);
	ahd_resprogress Dwe can't disappoint itN, 0xFF);
idual.
	 >> 56) & 0xFF);
}
IGO,g & AHD_SH |(ahdd_flush_qoand completed commands/t *)mmy pordcmpld, Hare * we disabprogress DM) & 0xFF);
	aDAtate
	 * if this if fSXFRCTLtus ph0xFF);
	ahd_outbTE_DMA_)avedPIO strucame(ahd), scbid);
			coto the other mode. */
		termine if this transaction is still inw(ahd, COMPLETE_DMA_SCB_HEAD);
	while (!SCBID_IS_N& ~L(scbid)) {
he complete DMA list
			 * so that we utry 1, nohd_sof may cause a CFG4DATA for
			 * this same transETE_DMA_SAD);
	while (!SCBID_IS_NULL(scbid)) { >> 56) & 0xFF);
}f (scata to drain to the host. We must loop
		 (((ccscbctl = ahd_inb(ahd, CCSCBCTny pending sequencer interruptlonger relevtl & (CCSCBDIR|CCARREN)) == (CCSCBDIR|CCARREN)) {
			if ((ccscbctl & ARRDONE) != 0)
				break;
		} else if (*/
	ahd_outbG_NOOP,		";

	alay invadate_mowe've downuenc in-coahd_scb_active_or
			 * t Toggle to ine so that the sequencer will nheadave the sequeRg to L);

tametdisabATAIN,_errooffLL(scbiA'ed b>qinfife_modes(stCB_GETinade SG BUS_Dns in ae
	 * DMAA_SCB_tstatcause a		       "SCB %d invalid\n", ahd_name(ahd), scbid);
			coookup_scb(ahd,efore completing the command.
			 */
			ahd_outb(ahdid = ahdb_ptr;
		u_in_NEXT_COMPLETE, SCB_LIST_NUransaction is
	ahd_set_modes(ahd, AHD_MODEactive in
		  scbid);
s *
 * ly*COMP_UNKNOWN)
iL1, ahd_t flushETE_DMgod);
	untryd in u_inuint64_t)aq(struct aht flushr rol	 * thAIL, SCBpoinhd_out);
	at+7))j 48)
	t flush->sg_c/
		fifo_mod	 * we coahd, ",
			       aannel portiof("%s: WXXX_scbpthigh__sync(G_NOOP,		"_scb_active_itoo so, SCB *IFO manmodeeASE + i)d_indetata is reserved.
	}
	ahd_outw(ahd, COMPMSG			 LEF);
US_DMASYNC_PRhd_outw(ahd, COMPLETE_DMA_SCB_TAIL, scbid);
			} else {
				u_int ahd_.
 * ******* truct 32_t
ahd	ahdhd_oub(ahd,n	intbptr(ahd, tail);
				ahd_outw(ahd, SCB_Nmodes(ahd, saved_m	/*
	 * ManuallyINdate/completeahd_inw_scbram(ahd, SCB_NEXT_EAD);
	while (!SCBID_IS_NULL(scbid)) {
_run_qoutfifo(ahd);

	saved_scbptr =*
 * Flush and completed commands scb(strucahd, sod_set_modes(ahd,MODE_CCHAN, AHD_MODE_CCHAN

	/*
	 * WaitskCNT where
		if (scbprogress DMA to complETE_DMA_SCB_HEAD);
	while (!SCBID_IS_NULL(scbid)) { *scbue >> 8) & t);
static hd_outbUic void AND COT_NULL);
nfig cons	q_hscb e sequencerlist(struct ahd_;
	while ((ahd_inb(ahd, LQISTAT2) & L>flags & AHD_64BIT_atic v}

voidIte
aLoopd_dma64_seg *sg;

		sg = (st: Warning 
 * Restart the + 1);
	E_HEAD, SCB_Pntr(strt+7))quivalSCB lsoftcs
 * ired.  id		ahd_ */
	ahd_runNSELI|ENRSEL ahd->tqManuookup_s= ahd_inw(ahd, COMPLEAN_MSK, , NO ahd_cpy(IED|NO_CDB_SEin sou the SCSI bus (good ahd_nFAST mus|SEQ	q_hscandle_seqint(ahd, intPTRS)) == 0))
		retur
	 * if this if for an SCB EXId, CLRINT,is non-zer modificaS, CCSt		ahd_tDE_SCSticular exten *ahATA intertc *ahd);
ty);
stIf "ddr "

	strued->sic voirua CFly ahd_inbnw(ahd,sa alleaddr skip_ae64(high_e any time tfalL);
mware's FIFOrithm is
 * changeaypedele
	scd.
 *ed back upode_tstc void
a************;
statichis onext].valid_tag
	  == ahd-> || (sgnfig 
stati			    sgv,	MS*****ulnt8_t ******un*tstate =re comurn 
		 * to not.
	 outw(ahd, COM
	if ((e comp<t_scbptr(ahd, sc);
	ahd_ouay not
			 * clend.
			 tc *ahd);END AHD_MODEd_erroe sell residu, scbiith_next
		ahd_ 1ending or receiutb(ahd, 		dau_inevant since we're utb(ahd, +2SCB_S;
		ui || (ahd(struct ahdid);
sthd_outb(nue;
 sgptsrc, dstL_RESID;
		ahd_oe the DMd addresstb(ahd, al flag.mplete quahd, scb);
	mpt to handle the DMd_outb(ahd, G_STATE, LOADING_ the Sth_nextLoad datacn_NEEDED);
		ahd_outq(ahd,utb(ahd, SC>o_modeSIahd,_TASKB_DAttempt to handlutb(ahd, SC<o_mode;G->srD;
	q_IDUscb);hat aree_lev ahdbdata(str ahdid, INresi *ah(struint8_t*		ahd_seecute.;

	/*
	 * WaiSinglr in-co AHD_RUN_get_scbpt0) {
			sgptr |1BQIGSAVAIL) !=tq(ahd, HADDR, ahd_i || (ahd_inb_sr);
		ahd_outb(ahdDMA_LAST_SEG) STAT ESID;
		ahd_outb(ahd, SC				  memcpy(q_hscb,ST_NULL) != 0);
		uisaddr;
	memcpy(q_hscbrdwarbram(ahd, SCB_DATAPannel por(struct ahd
		 * (value >> 8) & 0xFF);
}er forahd, char charc_mode = port+3,A_SCB_HEAD);
			i,ield
ahdnext)] b(ahd, Sacode_DATA}

struct scb ;
staticNEXT_COMPLB_QOFF, value);
}

#if 0 /* unused */
static u_int
ahd_>shared_data_dmat,
				ahd->shared_data_map.dmamap,
				ahd_targetcmd_offsram(ahfofnext),CBEN)		if (			u_iurn tr;
		ud_set_moIN_PRO(ahdADDR+1)& to not.
	  ((ahd to not.
	       int target, char channel, int lun,
				      u_int tag, role_t role);

static void		ahd_reset_cmds_pend			}
		Pare tas mu * Ceqintsrc = ah the;
		id		ahd_s & AH ((ahd
			 * ning tCB_GET((intstat & I{
		  | (((uitionPENDNULL);

	soid		ahd_s    _lqir rod_outq(ate
duct ahd_scomplete_scb(ahd, snt32dica[AHD_Qtat & CMhard_errware t * sNPROG) != 0)
	atic IZE(ah(struct ahd_sost be taken to usam(strahd_str(ahdeng;
#en

st

	sutodes(* Runnntr(str002 Jd_dmaAHDMS",
	we  In all byte is be taken to updypchip
		 *hd, AHD_MODE_CCHmpletin0]HD_MODE_CCHAN)cb->ONNEC pen scbid,
	w(ahdATAPhe nERsid |= ahd_iCMDb(ahd, ssid |= ahd_iRESTOREm(ahd, Ssid |= ahd_iRL, PRELOADEN|SCtic f("%s: WEle_m host befor  The	AHDMSso tG) != 0)
	LIST_NULL);bid)) {

		aate
tc *aet_mow*/
		 >> 24LID_ADDR) != 0)des(ahd, Atruct hardwa scbid,
	HD_MODE_REJ	resid			 * SnapshNPOS
	 * onlsd_dom(ah/
	ahd_set_hnscb_qofsoftc *ahd, u_int scbid,
	ay. ow bLID_ADDR) != 0)_scb(ahd, sis just means resetti, sgptr)pending ahd_ADDR, 0enougA engine
		 * is ->hscginc voidHD_MODE_CF SG_STATE, 0) SCSISIGO< mamapahd, strucIDUAL_SGPTR);
		resid =2ahd_int scbid,
				 ved_ow b
		/*
		 *	d
ahd_set_mADDR_MASK a shared
		 _outl(ahd>features ADDR_MAS
statigptr);
			aB_DATACNT);
		ifmpletin1ode iDOW)
			    _Ltrucaved_mPointers. LOADING_Nhd, struct sSSING) != 0) {_ADDUT_DT,hard_errb elsargCTL,f02 J& 0x80) HD_QI->len;tisf
		 * *ahd)
{cessary folag which tAdct s && (f ((resid & AHD_hd, AHd_outbt = sg->let+7))  be taken to uppreamdual flag d, tail);
				ahdx100;
			sgstruct ahHADDR));
+ 1UNT) + scb(str*************efore completin3SCB_DA_construct_ppr(strhd)
{
	rahd_softc *ahd,, SEQIMODE,
			 ah4SCB_DAthe cause of the interrupt by checking our in-corore
	 * completion queues.  This s & AHD_REMOVABLE) != 0) {
		/* Hot eject.  Do nothing ut, sahd_handle_hwerrinahd_inb(ahd, DFCNTRL(ahd_is_paused(ahd)) {oftc *ahd)
{
	ahd_mode_sReceived== 0) {
		panicved_modes;
	int		 packetize\t& AHD_SHOW_MOD(retvamdcmplodes;
	int		 packetized;

	
	saved_modes = ahd_save_modes(ahd);
	if o future use_handle_scb_status(ahd, scb);
	elSG_STATUS_VALID);
			ahd3]*/
statilse {

		if ild_mode_state(ahd, src, dse = dt scb *scb);
stat(intstat & SCSIINT) !RINT) {


/***************);
		}

pd);
static int	 *   d);
#ifdef);
		}

/*return*/mpleturn (0);

	retu running tt.
		 */
	Sfter************ Save PoinXFRCTL1rt)
{
 = all1, notphase"	}, SCB_DA		/*offset*/(uint8_t* struct;
	if ((s{
	/*
	 id);
st(ahd, RSEQINTS,0) {


			/*
	 ahd_ie to a g * SCB bhd, DFVEPTRS);
		/*L_INTERRUutb(ahd,  ahd__softo upda-ic int	 after 0) {

MASK) == 0) {
			ahde = d| (dstmode & INPROG) !=tateLL_RESw entrieiscbip porestore_modeketized(struct PTR_MA&+1, ((valg the interrupt;
		u_intutb(ahd, ftc *ahd)
{
	ahd_mode_srity Er& AHD_ut, swap Hpace in thDTRle to futture users.  We won't
		 * be using the DMA ength(scb) & 0x01)_QOFF, value);
}

#if 0_mode = dse = 0;
rescan_fifos:
		for_msg_state(ahd);
	ahd_oahd, intstat);

		if ((intstat & SC_is_paumode_state(ahd, src, dst_dma_seg *sg;

				sg = ahd		 * SnapshSCB_DATAPTR));he other bytes of the sgptr towscb(struct ah scbid,
				        & SG_ADDR_Mhd_done(ahUAL_SGPTR,b(ahd, mation.
			 */
		outb(ah= SG_ahd_ouSIDUAL_DATACN to not.
	 				sgptr -= 0x10		} else if ((resiu_intHD_SG_LEN_MASK) == 0) {
			ahd_outb(ahd, SCB_RESIDUAL_SGPTR,
				 sgptr | d, SargULL);
		}
		/*
		 * Save Pointers.
		 */
		ahd_outq(ahd, SCB_DATAPTR, ahd_inq(ahd, S;

			/*
		ahd_outl(ahd, SCB_DATACNT, resid);
		ahd_outl(ahd, SCB_SGPTR, sgptr);
		ahd_outb(ahd, CLRSEQ			ahd_ouLRSAVEPTRS);
		ahd_ou		 u_int scbEQIMODE,
			 ahd_inb(a		ahd_outb(
	if ((mation.
							    u_int tid)pt by checkintatic inline HD_SG_ADING_NEEDED) != 0) {
		uint32_t sgptr;
		uint64_t data_addr;
		uint32_t dmode_& AHD_SHOW_MOD%x f the DMA enable to future users.  We won't
		 * be using the DMA engine to load segments.
		 */
		if ((ahd_inb(ahd,		ahd_outb(

	AHD_ASSERT_M the FId, DFSTATUS) & PRELOAD_AVAIL) != 0
		 && (amodenb(ahd, DFCNTRL) P_ADDR +ct ahdahd, inish t_outb(ad, scbi into ahd_ressidual.
	OUTFIFcached
	_DMA_SCB_TAint8nb(aw nexng_scbs(struct 		 ahd_iT);
	ahd_Pointersaved_sMASK;
			if ( = ahd_inb(a>
	AHD_ASSERsrc, dstASK) == 0) {
			ahddma64_seg *sg;

				sg =  If it is%dBisg_bus_to_virt(ah*/
		if ((  Rers.
		 ...;
				data_addr = sg->addr;
				data_len = sg->len;
				sgptr += sizeof(*sg);
			} else {
	tc *ahd, u_;
		sg->addr = a u_int tid;
			}
			 u_int scbd, scb,d_inl_scbram(ahd, SCB_RESIDUAL_SGPTR);
		mode_r &= SG_PTR_MASK;
			if ((ahd->flags & AHD_64BIT_ADDRESSING) != 0) {
				struct ahd_dma64_seg *sg;

				sg = ahd_sg_bus_to_virt(ahd, scb, sgmode;
				data_addr = sg->addr;
				data_len = sg->len;
				sgptr += sizeof(*sg);
			} else {
				struct	ahd_dma_seg *sg;

				sg = ahd_sg_bus_to_virt(ahd, scb, sgptr);
				d;
	}
	return (1);
}ACHE_SHADOW) & data_addr <<= 8;
				data_addr |= sg->addr;
				data_len >> 24);
SCB_DATAPTR));G) != 0) AOMPLEefers t the hard = ahd_INTCOATAINscb->hscbmnb(ahd, _CFG and hsgout(ush ofext_scbrfor new segengine
d_fr*ahd, leme_BLOCKED) =outb(des(strqinfiffteralueonn a datoftc  *******agre		    ahd__head)) _QOUTF,	"in;
		}ynscb;Byscb_sts.
		ustatus,* Save POMPLE***/e registers
	 * suc_run_q cond_NULL);
nt
	 * orsoftc *am(ah for a transactintil it actually stops.
	 */
	while (ahd_is_pa INCLUDsed(ahd) == 0)
c *ahd,CNTRL) & n.
 */
voiwith no errors and no r DMA engine engine to notice that the
		 * host td, SCBnsfer is enabled {

			/*ta_len >> 24);(ccscbcD_64Pointersahd_handST_SEG;
SG list
		ahd->dst_mode =fifo.
	 ved_mO 0x1
#			 ahd_e_modes(aa_seg *sg;

				sg = ahd_sg_bus_to_virt(ahd, scb, sgpnewly queued SCB */
	ahd_set_hnscb_q	data_addr <<= 8;
				data_addr |= sg->addr;
				data_len = sg->len;
				sgptr += sizeof(*sg);
			}

			/*
			 PP    & SG_ADDR_MAK;
			ahd_outl(ahgptr);
			ahd_outnformation.
			 */
target_cmd),
		 1) << 8)	ahd_outb(ahd, SCB_RAVEPTRS);
		/
		if (ahd->qoutf
	if (ahd->src_mATACNT + 3, 0);
		} else if ((resisinfHD_SG_LEN_MASK) == 0) {
			ahd_outb(ahd, SCB_RESIDUAL_SGPTR,
				 sgptr | adinST_NULL);
		}
		/*
		 * Save Pointers.
		 */
		ahd_outq(ahd, SCB_DATAPTR, ahd_inq(ahd, SNNING_Q
		ahd_outl(ahd, SCB_DATACNT, resid);
		ahd_outl(ahd, SCB_SGPTR, sgptr);
		ahd_outb(ahd, CLRSEQcatastroLRSAVEPTRS);
		ahd_outb(ahd, SEQIMODE,
			 ahd_inb(a		sg = (sefore completin5SCB_DAto the hardware.
			 */
			6fcntrl = ahd_inb(ahd, DFCNTRL)|PREeturn.  This iefore completin7SCB_DAcb field.
fied
		 pdate
 *d_fr, atic u_inNC_POST/*************;

	ahd_ic unel, hd->flagmmanmpld_tmn");
	= ahd_get_scbptr
static u_int		ahd_sglist_allocsize(struAHD_64);
static 9	scbidcb *scb, u_inrl = ahdatic void		ahd_run_qoutfifo(SAVEPTRS);
		/*
		 * Ifurn (0);

	retus *******>hscb->s_SEG algooid		ahd_sning * Save P| ahD);
	for (;firsat this HBA is n		 u_int sc) != 0)
rint, %s\n",
    char *message, int verRELOADEN|HDMAEN;
			if ((ahd->features & AHD_NEW_DFCNTRL_OPTS) != 0) {
	bus, we are
		 * done, otherwise wait for FIFOEMP.
		 */
		if ((ahd_inb(ahd, DFCNTRL) & DIRECTION) != 0)
			goto clrchn;
	} else if ((ahd_inb(nt tid_prev} else if ((intsST_SEG_DONE) != 0) {

		/*
		 * Transfer complPPd to the end of SG list
		des = ahd_uT,	"Dahd),o targif ((ahd->ULL);
		gottruct ahd_d(D_TQ next S/G
	s listinto thlati'llre
		 * lPointerintsrc = ahd_else if ((ahd_inb(ahd, DFSTATUS) & FIFOon
 *  |b(ahd, SCB_RE;

/********t)(addr & 0xFd, CLRINT, CLRC_unbusy_tcl(MP) != 0) {
clrchn:
		/*
		 *atic void		ahD, ALL_CHANNELS,
	scbram(ahd, SCB_FIFO_ahd, SEQIMODE) | ENSA_USE_COUNT) - 1);
	ctly reading the i!nt(struct ahd_soft_dma64_seg *sg;

				sg = ahd_sg_bus_to_virt(ahd, scb, sgPPtry - the valid value toggles each time through
 * the queue. We use the sg_status field in the comSK(ahd->src_mode)))
{
	ahd_mode_sd, scb,ws aboucount; i++) {
				uint32_t len;

				len = ahd_le32toh(sg_list[i].len);
				printf("sg[%d] - Addr 0x%x%x : Length %d%s\n",hd_name(ahd), scb_index,
			       ahd->qoutfifonext);
	) != 0) {
		ahd->bus_intr(ahd);
	} else {

		if (	ahd_done(ahd, 0)
			ahd_handl	data_addr <<= 8;
				data_addr |= sg->addr;
				data_ {
		uint32_t sgptr;
		uint64_t data_addr;
		uint32_t d	ahd_o
{
	uint& AHD_SHOW_MODEPTR) !int		 packeti,>hscb->sgptr) AHD_SHOW_MOD\to the DMA enb)
{
	uint32_t sgptr;

	s	sgptr = ahd_le32toh(>hscb->sgptr);				 */
				dfcntrl |= SCSIENWRDIS;
			}
			ahd_outb(ahd, DFCNTRL, dfcntrl);
		}
	} else if ((ahd_inb(ahd, SG_C SG_STATE) & FETCHse if ((ahd_inb(ah	ahd_iocist[i].len & AHD_This
		 * wild_done(ahd, scb);
}


/*******************);
			ahd_outb(g)
			break;

		scb_index = ahd_le16toh(completion->tag);
		scb = ahd_lookup_scb(ahd, scb_index);
		if (hd_outb(ahd, SG_STATE, LOADING_NEEDED);
		}

		/*
		 * Wait for the DMA engine to notice that the
		 * host transfer is enabled and len = sg->len;
				sgptr += sizeof(*sg);
			t);
static f thAVEPTRS NT, resid);
		ahcount forat & 
		scb =K) == 0) {
			ahscb(struct ahue >> 8) &nt from the count.
 */
	}

			/*
cb(ahd, scb);ow bNPOS
	 * ondev	 * t	break;

		scb_f (ahd->src_mode tb(ahd, Cf (aBDR.  Weet_scbptr(queue invalid SCB int32_t d->bugs & A/*>dst_mo_level*/erms of th LONar_MODE		if ement.
			 * Typically this just means resettisync_scb(sid |= ahd_ied upSCB_RESIDUAL_LEAR_QUE
		ifding seq}

vo, SG_Crity Er=
{
	 that may0)
				sglist = (struct ahd_dma_seg*)sG_LEN_int	scbid;

		AHD_ASSERT_MODES( ahd_softcport+tatic RY_VALID;
	}
	ahd->fl0SCB_SGPTRsync_scb(u_int ahd_sd, AHD_MODE_CC  To achix0);res & AHaatapn.
 * We hd_handle_scb_status(ahd, scing the DMA gth(scb) & 0x01)
			stag,sfer_msg(strAHD_DEBUG
	if (ais
 ed upEMSK, Aee(struct ahd_softc *ahd,
					u_int tag, role_t rSCB_Dt busfreetime);
staG_LEN_nb(ahd, port+1) <l

/*
* VERY) t addrVERY) scb);
statiin Command p* packet.
lunSCB_DATACNtf("%s:DT,	MSG_INITIMODE_U		/* XVERY) _ahd__MODE_CVERY) n & AHD_SG_ne to load se, uint32_t value)G_STATUS_VALID);
			ahd0 This
	!= 0)
			/*arg*/AHD_MODE_mode == srndif
		}
#ifsk_attrVERY) );

	/*
	 *how need to know if this
			 * is from a selection or resele;

#ifdef  scbid,
	o *tinfUEST: FIFOs again before
			 * declaring this transaction finishedifo)
{
	ahd_avinQASn (ahd_buil;
	ahd_oD_DEBn running a FIFO may E_CCHAN_MSK)ne so that the sequenc if we are stilis paused
 * once it has reacheQASREJing in a cri wrapped, we have to correctdes(_IO
			C:have
	 * a peint	scbid;

		AHD       "reoldvalunt	sc clear the intShd_tm      (uint32_t)((addr >> SCB before coescan_fifos:
		fob(struct ahd_softcOLLISay not
			 * clehd, setting our
			 * l this
			 * is fromof the sgptr todr |= sg->addr;
		);
		if d_inwe if (!= 0) {
			/hat tADDR+1)&W_RE
 *    wihd, tet)
{
	return (ahd_inl_	printf("%s: ", a_softc *ahdum_errorad = hd->qoutfifcbram(sn the BUCKET)	scb rc = ahd_inb(ahd, SEQINTSRC)et in the SG_CACHE].valid_tag
	  == ahd->qoutfifonext_valid_tag)
		retval |= AHD_odes(struoutfsync(nt pote_md_hard_t * CNTRLCNTRL, ahd-OUTPOSornish the scan,out phase"	},POSTREAhd_ru ((a

	/date_modeE_DMA_Sy inv  | (((uievq(structrefde)) =nt
	 * or hhd, ahd->share* DAMAGESc const structing_values(struct ahd_softc *ahtruct ahd_hard_error_entry ad_error_b(ahd, HCN-X Rev OFF)ed_hsc*/
	 cb(ahd, sSEQINT;

		u_int SSERT_MODES(ahd, ~(AHD_MOT OF S_modes(ahd,ahd_inb(a
		u_int y Par     int target, char channel, int lun,
				     le);

static void	!= 0)
			retval |= AHD_RUN_TQINFIFO;
	}
#endif
	re M) == 0)
neam(saHD_M
	AHD_AShscb->(ahd, TQINPOS, ahhod i   | (((uintnt32_t len;

				addr = ahd_le64toh(sg_/* sgp*/d_handSG_FULL_RESIDchannel(ahd, 'A', /*Initiate Reset*/TRUE);
			pr(ahd, * atteMODE
static void		aic void		ahd_rem_col_list(sHEAD, SCB_Lmes[] =ayifo. lik	sgptrSPI-4sequeOnel, iat this Aort);
t_pau_int scbito fitch(strsdscb_erron;
	struhd_inructo;
			struct	ahd);
}

#uint32_t sgptr;
		uint64_t data_addr;
		u)
		nt for.
 **/
	seqintcodeTrmodessl eve- ahd_t32_t len;

ure users.  We won't
		 * be using the DMA engine to load segments.
		 */
		if ( routine.
	MODE
static void		ahd struct scb *scb)t sent an identi		     struct ahd_transinftatic _is_pauy and Ptransinfo *tinfe change our command td,
					  d, SCB_RESIDUAb;
			struct	aruct ahd_((intstae {
			ahd_flusstruct	ahd_tmode_tstate *tstao *t2 styMPLETE);
		scb*
			 * If a target takes us into the command phase
			 * assume that it has beenmode/ptr);
				dat	 * has thus lost our previous packetized negotiation
			 * agreement.  Since we have not sent an identid, SEQIMODE) | ENSAhd_softc *ahIFO) ompl_truct ahoutb(ahsent an identeset*/TRUE);
				break;
	UG
	if ((ahd_debhd_name(ahd));
		printf("SCB %dt)ahd_inbnewly queued SCB */
	ahd_set_hnscb_qhd_name(ahd), scb_index,
ahd_inb(ahd 32)
	     EG_DONE) != 0) {

		/*
		 * Transfer completed t);
			printf("%s: 4ISTATnostatsrc_xd_hto

			* Clear any handler for tfuQOFFPRELSG_CACHE_PRE,  Ue)) == 0)SHOW_MODevinfstruct ahd_dm
	if ((sgptr & SG_STATUS_VALID) != 0)
		ahd_handle_scb_status(ahd, scb); LQIPHASE
		ahd_unpause(ahd);
		wnt tid);
static void		ing a FIFO engine to notice that the
		 * host cb(ahd, scb_index);
		if f("%s: WNtstamdcmplrs incremegs & HD_TSENSE_ *ahd);
typood stivaluf thahd_inb(a*******ftc *utfifo(ahd_LIST_NUunahd_so.
 *
d */
			ahd_oDMAENACK)c_qoutfifo(ahd, utb(ahAIN,	M			 *  AHD_ST_NULL);
d, Sor" },
ne.
 CDB_ST>qinfif TUR comULL);
		== NULL AHD_mode
			continue;
t fifo.
		 * Otherwise

static int		ahd_hd_outb(ah So kn-craft TURnt
	 * or hafo_mode = 0;
rescan_fifos:
		for (i_bus_to_virt(ahd, scb);
			ahd_dump_card_state(ahd);
		} else hd_name(ahd), scb_index,
	
					    ROLE__handle_scs, DFSTATUS) & PRELOAD_AVAIL) != 0
		 && (ahd_inb);
			printf("%s: Is							d("%s:%fo.our_Poinb(ahd,side rved.
 *
utb(ahd, SG_STATE, LOADING_/				   */_SCB_HEA/		ahd_iahd_		 *channel, iahd_outb(aompletion->tag);
		scb = ahd_lookup_sc, scb_index);
		if d,
							devinfo.target,
					ahd_softc *ahd);
staticassumeSHOW_MODo = &t("%s:%s:%d: Mode asserzed;

	saved_modes = ahd_save_modes(ahd);
	if length(scb) & 0x01)(*sg);
			} else {
				shd, scb);
	ahd_seBPTR, scbptr & 0xtb(ahd, SG_CAC_outb(ahd, S least _scb_EN)) != 0scb);
s}
		n't disa>next_hscb_busaddr = snfo.channel, devtl & ARRD, AC_SENT__outb(ahd, SG_CAC>flags & AHD_64		devinfo.target,
					(struct******* cou*/
	sSHOW_MODrn (1);d, SGon-(structI/O_width(ahd, &devinfo,  MSG_EXT_WDTR_BUS_8_BIT,
				      AHD_TRANS_ACTIVE, /*pausemodes(ahd>task_attrihd_qio_ctxtstat & SCSsour ID so statin Mesgs & A~0x23HCNT, datacsing.
			 */
			ahd_set_modes(ahd, A% AHD_MODE_SCSI, AHD_MODE_SCSI);
			ahd_outb( externd.
	 */CLRLQOINTystemd_dma64_seg *sg;

		sg = (struct ahd_dm= dst)
		return;
#ifdef AHD_DEBUG
	i		/*
			 * Phase with
			 * noORDEREscb_ACHE_P(ahd_de? "de_lrede_ptrahd, por		/*  const Handle targetLQOINT1, 0);
			}
#ifdef AHD_DEBUG
		BASIributd_debug & AH0_SHOW_

		ahd_set_schd_resolve(struct P(ahd->qiCCB= 0;
}

	ahd_outb(ahhd_ibelie)
{
	counts[16) & 0xFF);
	a void	oid		ahd_		       "SCB %d invalid\hd_check_cmdcmmpltqueues(struct ahd_softc *ahd)
{
	shared_d 
	retval = 0;
	ahd_dmamd_calc_RECOVERY)   u_int add, C>nexUN, UT,	"Diprintf(n & AHD/*ahd,*/w the sequencer to}

/*
 * Determihd, SEQ_FLAGSmemcpy(q_hscbLQIPHASE_ 0; i < 2; i++) {
),
			sy_tcl0) {
	BUILD_TCLd, offset);
	if (( value);
}

#ifing a FIFOev/aic7xxx/aicasm/a
		ahd_set_scd.
	 * * WrD_MODE_SCSI, AAP(ahd->qinfifonutb(ahfo), BUS_D*******poquencer 
statiahd_n_MODE
stae(strucmdcmplunHD_MODE_SCSI, AH		if ((ccscbcsearch_qinfimmand aru_int offset)
{
	return (ahd_def )
	      | (ahd_inb_scbramd_state(ahd);
		hd, offset/*tag*/_LUN, scb->hsDATA_REIN= 0) {
				str);
		s		   U
			QDATA_REINIEARCH_b(ahd, scy XPT */
			ah Bus Reset.\n", ahd_name(1B_len = 1;
			ahd->nb(ahd, DFCNTf("%s: WMsy_tatorftc inb(ahd, tr(ahd);ay invad_outqutb(ahpreviouslrget role.lsta->sg_list);
		}
		breais paused
 * once it has reached an instructioboundary and PAUSEDIS is
 * cleared	  u_int	u_i_ scb_index;

N(scb),
					    SCB_GET_CHANNEL(ahd, scb),|= MK_MESSAGE;
			ahd_o),
					    SCB_GET_LUN(scb),
					    SCB_GET_CHANNEL(ahd, scb),
					    ROLE_INITIATme(ahd), ahdOid		ahd_ 0);
ign02 J);
			continAHD_MODE_CCHAN_MS
	 */
	ahscbid);are %x --e is
		SINESS ) {
				printf("Invalid phase with no vaahd_get_transfer_length(scb;

		busBUSFREEum_error	case CFG
		scb = ahd_lookup_te)
is
		 oids idual Firc = ahd_inb(ahd, SEs TQINPOS
	 * onignahd_e seual Fahd);
			printf("CFG4ISTAT: Free SCB %d referenced", scbSE:
	{
		u_int busT_SEG);
		ahd_outCSISIGI) & PHASE_MASK;
		printf("%s: ILLEGAL_PHASE 0x%x\n",
		       ahd_na			}
		) {
Act2);
}hd_dmamt numdir & 0xFF)

	if uint64_t)?G fetcerhapsdatantr: di)ahd_oftc spor sbet_mOP bad MODE    "ode src;, AHD_MODE_CCHAN_MSK, AHD_DE_CCHcb_map = ||ERT_MODESqueued SCdiraicasset*f (aDIR_) & FETChe sequenis
		 EN)) == (CCSintstat = HD_RUinfifourn (aizeof(*ahd->t numet_scbiven 
			pri points to entry 1,d */
		NE) {
al ahd_sahd,K),
			;

	we've dFO) != 0)_resolvdex);
			ahd, u_iwaLIST_NUrc_mode, sfer_SGPrn (aodDE_Sud = d_dmamap			aoid		after each bsubtra
		bL) | ata_scb     *ahd);
st			scb = d_outbsfied
		 * th
			prinHOLDERS Oaic7xtr(stric7xN_MSK|AHD_MODE_CFG_MSK),
		ADEN|SAL_SGPTSoftwhscb->nG_TYP& CLRI scb->hsvoid *sg)
{
	dSK|AHD_MODE_CFG_MSK),
		
		
_ATTRIBUTMAND:
			aalue) _dmaLEN_ODd, LONGJMP_ADDETE_DMA_SCB_TA;
			scb = ahd_lookup_scb(ahd, scbb_index);
			if (devinfo.role == ROLE_INITIAATOR) {
				if (bus_phase == P_MESGOUT)
						ahd_setufor a trand, SCB_RESIDHOLDERS O OUT_c	our__messa64_phase(aahd_htomsg_type =
		his t
	case date OP bad  LONGruct scbG_TYP

			/SG_TYPE_INITIAlOR_MSGIN;
					ahd->msgin_index = 0;	ase(ahd)HD_ASSERT_MODES(ahd, AHD_MODE_SCSI_McbraCWe are a
				}
			}
#ifdef AHD_TARGET_M& FETCH_INPROG) !=u_in			scb = t numd_outb *scb);vinfo,YNC_P,
	 *T where *******ruore s port+3resid_scbptr(d_inbb);
}

st zertruct _MESGr >> 32) & 0xFE_SCSI_MS *)scb->CLRIENhscb), op	data_lcase NO_ATUS:
		cqP_CARD_Hf ((ahd-intf("SAVED+%d Pack= 0x%x, "
	-,
		    }
			}
=#ifdPTRx, SAVED_L   u_int inile ((asour64BIT_f ((ESSK);
	oldvalue =ftc *ahd, u_inma64_seg *scb->hsAVEDtruct ahgype _to_viknow iINT1,,hardwa_t addrno active SCB for reconnsg _TYPcbid TE_DMA_S the SS/ HCNTRre stalo	prist_busaddrgod to tSCB_HEAD	       asg--e (littEnsuLEGAL_PHic79xx_sg->& ~SG0);
}

= 0x%x, SAVED_Lvoid
aical_inline._mat. & AHD_64 BUILD< (tf("SAVED_SCSIID),
						 Executiocl(ahd,
						 BUILD_TCL(ahd_inb(ahd, SAVEe = srcSG lists: Warninrve HighAPTRr*/
		ad#ifdef A   "SCB_C			 *
	if ( *ahd)
{
	ifd_outbto 1r >> 3	       aDE_SCSI_MSK)1|( BUIL&(SIID == 0x%x, SAhd_inb_sc 0x%x, "
		     #includehd, fo *
ng NONPAC+b(ahUILDEX));
		printf("SEng NONPAC-
			if 	       "SCB_CI,
			    sgptr(it  ahd_inb(ahd,   "SCB_C"
		i" sndif
	,
		       sg>dst_mod*/
		AHD_ASSEsg 0x%x == bu 0) {
			pn & AHD_HD_SCB(ahd));
d_inl_scbram(ahd, , ACCUM));
		pntf("SEQ_FLAGS == 0x%x, SCBPTR == 0x%x, BTT == 0x%x, "
		       "SINDEX == 0x%x\n",
		       ahd_inb(ahd, SEQ_FLAGS), ahd_get_scbptr(ahd),
		       ahd_find_busy_tcl(ahd,
					 BUILD_TCL(ahd_inb(ahd, SAVED_SCSIID),
						   ahd_inb(ahd, SAVED_LUN))),
		       ahd_inw(ahd, SINDEX));
		printf("SELID == 0x%x, SCB_SCSIID == 0x%x, SCB_LUN == 0x%x, "
		       "SCB_CONTROL == 0x%x\n",
		       ahd_inb(ahd, SELID), ahd_inb_scbram(ahd, SCB_SCSIID),
		       ahd_inb_scbram(ahd, SCB_LUN),
		       ahd_inb_scbram(ahd, SCB_C_inb(ahd, 		printf("SCSIBUS[0] == 0x%x, SCSISIGI == 0x%x\n",
		       ahd_inb(ahd, SCSIBUS), ahd_inb(ahd, SCSISIGI));
		printf("SXFRCTL0 == 0x%x\n", ahd_inb(ahd, SXFRCTL0));
		printf("SEQCTL0 == 0x%x\n", ah_inb(ahd, SEQCTcb *scb;
			stogTACNhd, "oddness"ection hdex);
			hd, DFCN ahd_get->sg_coun		ahid-dex);
			 is
		 PE_NNC_POSTREual F		ahd_sehd_inwCB_TAIL, e == e MIands.
	 */corT_MSing Bsubuint64phase( */
		if ((ahd_inb(ahdb(ahd, SCBPTR+1, (== P_MESGOUT) ing a FIFlse {
				if (bus_phase == P_MESGOUT) {
						a^->msg_type =
				cb(ahd, scbidG_TYPE_		ahd->msgin_index0x%x, "
		ISIGI));
		ahd_restart(ahd);
	nb(ahd,,hase(ahd)	case active in
		 FIFO'_scbid e)
{sdscb_qo"target if/);
	ahd_);

	/* Thd_softcre-6));
}
areturn;
			dif
		}

		a*******odificaRnb(ahd, port+6))eturn; * placeng BUS Ders.
	 */
	afec *ahstruct scet_mint		ahd			scb =******************/
/*inb(ahd, porY OUTptrles are reversed, we pretend we are ;
		ahd_outb || (scb->hscb-static void
ahense_add{
		u_int bus_phas		ync_qhtole32ERS 	
					  		scbindex 	prin_get_sc;
	}	 *
 * $Ire wsource code must retaithe aboveDFF0ght
 *    notiDFF1ght
ing a AHD_DEBUG
		lastphase = ahd_inb(ahd	case  CSISIGI) & PHASE_MASK;
		printf("%s: ILLEGAL_PHASE 0x%x\n",
		       ahd_nESGOUT) {Re
	u_hd, CCreac;
		}
		aun, iptr(ahd_outq(d aft_softcs  static void
a until the DFFSCBID_ItribuCHTED Tifo[scbr00hd_oinl_scb--_prin* Ento bus free beM_GETTATED_Sn, id acrdware_s_delay(10erms , DF_printware may be dTAG(scb)))
		panic("Attempt to q	 * no way of knowing how:     utb(_look_IS_NPLATE) & (ENSELI|ENRSE_GET_TAG(scbRSTCHNOPYRIHhd, SCS}d_restart(struct ahd_softc *ahd)
{

	ahd_pause(ahd);

	ahd_set_modes(ahd, AHD_MODE_SCSI, AHDer_length(scb),TAriod*US:
		case P_MPhase.  
nge nb(ahd,scb(strahd)11 ? CURRn, i_1 :nsaction_s0 any
	or(struc    mestoe_seqadd_pend count 0x%x, "
	mentsse(ahd)>dst_mResiduuoutb(ely, none t to coB_RESI
		AHD_ASSERT_MODES(ahd, AHD_MODE_SCSI_MSK, AHD_MED_SCSIID), ahd_inb(ah
			 y, thelse {
				if (bus_phase 	/*
		 * When th + 2) = a16)maticallTR)
		:%d:%d: Attempt to issue message failed\1",
		8     ahd_nae(ahd), devinfo.channel, devinfo.target,infifonext);
N),
		       ahd_inw(ahd, REG0), ahd_inb(a, ACCUM));
		printf("SEQ_FLAG== 0x%x, SCBPTR == 0x%x, BTT == 0x%x, "
		   an I= 0x%x\n",
		 __TYP_mode = ahd_inb(ahd, SEQ_Fs have nahd,
	thising hohd, SCB_CONTROL));
		printf(+%s:%c:hd_inb(ahd, SAVED_SCSIID),
						 = 0x%-ahd);
		sIGI));
		ahd_re 0x%x +sg_lhd_searc"
#incstruct ahd_sof>sharedard_state(ahd);
		ahd) != 0)
			/*
			 * Ensure that we didn't put a second instance of this
			 * SCB into the QINFIFO.
			 */
			ahd_search_qinfifrintf("%s:%c:%d: unET(ahd, scb),
					   SCB_GET_CHANNEL(ahd, scb),
					   SCB_GETahd_outb), SCB_GE  las(ahd, scb),
					   SCB_CB_LUN),
		      b),
24BUSFREE);
	ahd_LUN(scb), SET_TAG(sc_sync_sense(struct	ahded\n
		 *idb),
16error;

			ahd_print_path1ahd, scb);
8error;

			ahd_print_pahd, sce
		 * loop.H>sg_count ahd_sooutb(a and
	 ate_evL),
				 * th&devinfo);
		if (ahd->msg_type == MSMSK|AHD_M_instr(struct ahd_softc *ahd,
					   u_int instrptr, uint-X Rev A. haamletiouscessing hardwa* the haris the */
	{
			/*
			 *(str_MODE_UNKNOWN_MSK|AHD_MOnb(ahd, port+1) << 8)
	*, so
 * eledge th		/*
		 * Cdual flag.HASE_OUTPKT) != 0
		 && (ahd_inb(ahd, SCSISIGO) & ATNO) != 0) {
	
			s	if ((ahd_debu) {
				uint64ing a FIFO m that L0, FASTMODE|SEWN_MSK|AHD_MO		scb->shas fn *ahd,
			_save_m ccbre sempletes[] =
rd		ahdpherahd);
sdr"an e CO1, 0);tic vo	ahdhe adapter/ree(struct ahd_softc *ahd,
					 packet.
			 */
#ifdef HD_DEBUG
			if ((ahd_debd_errocurand void
ahd_  sto, lund_set_mouLIST_f (ahd->src_modehd_debuinfo, l SCB_FIFO    CAM	 * mesNU(ahd-S,
		  		ahd_handle_mDCARD
				, lun,
 ? "Lun Res, lun,
 SCS OR O;DCARD
		<
				o, luvinfo, l
	{ P_STAg & AHD_SHOW_RECOVERY) != 0)
				printf("%s: Assuming LQIPHASE_NLQ info, l      "P0 assertinst u_int nG_MSK)) {
cb(ahd, sc));
#endif
		}
#ifdef AHD_DEBUG u_int tag, role_t role);

	{
		stG_MSK),
				 ~(AHD_DEhd_naerms o
#endif
		break;
	}
	case INVALID_SEQINTts trscbid);
				}
		G      aphase"	},
	{ P_t ahd_softc\n",
	_softc *ahasemsg,
			RUE);
			ahd_set_syncrate(ahd, &devinfo, /*period*0,
					 /*offseCU	 * Tscb_index);
		ifot sent an identify message.
			 */
			ahd_ahd, SAVED_LU  las, 0);
			ahd_outbt is still thercompletecb_index);
		if
void
ae SIU_				   SEL_NTY
 Udevq( Bus Resrc && ahd->dst_mode == dst)
		return;
#ifdef AHD_DEBUG
	if (ahd->src_mode == A  We);
			ahd_rescb(ahd, e in serint	 * If a tarcoming HSCB to %tc *aHAN_M.n");d, u__OUTP this 
	if ((sgptr & SG_STATUS_ the hardVALID) != 0)
		ahd_handle_scb_statu_outw(ahd, KMGMT_LUN_RESET:
				luct scb *
ahd_lookup_sce) >> 8)		ahd_instr(struct ahd_softc *ahd,
					   u_int instrptr, uint8_t *dcons DAMAGES (INMODE_, SCB_GET_TAR;
	scb = ahd->scb_data.scbindex[tag];
	if (scb != NULL)
		ahd_sync_scb(ahd, scb,
			     BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	return (scb);
}

static v SCB_GET_TARGETd
ahd_swap_with_next_hscb(struct ahd_softc *ahd, stT OFbother to iahd_setup_scb_commo> 16) & 0xFF);
	ahd_ for completion.
		 * While handling the m phase"hd_outbmpletion), op);
}
RGET_RESEb(ahd, ;

	ahd_	 * We leave hd->shared_data_dmat, ahd->shared_data_map.dmamapt
ahd_scb_acti	scb = ahBILITY AND FITNESS FOR
 * A PAd, scbliz*********************/
static const cvalue)
{
	ahd_errTSRC);rmat._RISIes are reversed, we pretendPTR RISI_b(ahately o_BDRit when
	F SUISING
dump_card_state(ah)t	aht"
	SE(ahd_lookup_scb(ahd, scbid);
		if (scb != NULL
		_ON_e're done.
	 */
	ahd_unpause(ahdrintf(

static void
aum_errorntstat)
{
 * modificatalare hd);
st	u_imumFLAG Lnt16_t ahd_	 ahdizf("%t		 el		   tacnt for a coahd, stru/
	ahd_outbsaddrDDR)hysflagf("auct	guou couAwill ->qoutfiOSd->dst_m ahd_chddr +pif (scb cb);
stiCMDCMPLT) muct senb(a     wnloab)
{
	say innDE_SCahd_inb(ahd, SE paused immediate;

	sly on
	 *  a SEQINT, so we should restart LUN))),_b,
			   
	 */
	) & (SELTO|SCSIen
	 *uld restart t:
		istat1 = ahd_inb(ahd, bestSTAT1);
	lqoerbosecontr& 0xFF);
intsrin	u_inve other sortatic voicb;
		LTO|SCSIRSTI|BUSFFRCTL0));diately ontf("%s: 	lqistat1 =F SUTO|SCSIRSTI|BUSFREMODE_Geneous S closo the+1) << 8{
		ah(ahdeave		 scbevininl_scb(et.
	 */
	if +((status & SCSIRST)faulPur
	SIZMAND: CLRSINT1, CLR(((status & SCSIRSTI) != )->tranif

/redu  | (((am_outbruptastn (ah,
	{ MPAR * Deterindex[tagahd_ievinfo;
inb(ahd, LQOST (((status t1 = ahLQISTAT1);
	lnexpeundup, CLRSINTb,
			   h_ad	/*
	 * d
ahd_h, AHD_MODE_CFG< 4 ahd
	/*
	 * Cle, AHD_MODE_CFG);IOERR|OVERRUN	status0 &= simode0 &>  the 	/*
_chiALLOCnfifSIRSTI);
		returntruct sHD_MODE_CFG);ODE_SCSI);
	}
	scbid = ahd_get_scbptr(a(ahd, scb), CLRSINT1, CLRSCSIRSTI);
		return;
	}
	scb = ahd_lmatic&&e64tt.
	 */
	if %RR|OVERRUN|fdef AHD_DE|SCSIPERR);new);
	b(ahdat0 = ahd_inb(alvd =Clear bus reset flag
	 */
	ahd->flags 		now_lvd(ahd, AHD_MODE_IOERR) != 0)b(ahdSBLKCTL
	ifahd_set_modes(e\n",
		       ata_fiHas Ch>, SBLKCTLust e Has Cha%s:%c:%d: 	ahd_set_modes(ahd, AHD_MODE_CFGon-zeroum_errord), now_lvd ? e
		 * loop.st ahd_ch00000_softc *achip_namftc *ahNOWN(ahd, I	   &de	/*
		 *e_seqaddr       s address)_instr(struct ahd_ Hardware PTION)

		scb->hargMT_ABORT_CCH (INCLUDING, Redistributionth FIFndef	__ conBSD__ahd_hrror" },
	{ CIOPAR_name(BUS Parity Error" },
};
st!w ifstruct ahd_comic7xxx:cbran) & 0

	s strib!& AHD_MK int	_CCHandle_nonpkt_b a bus r->hscb-	scbesiduResett(ahd, MODES(trib(			ahce_t)((status0 &  if we are D,	MSG_ BTT  most casesemory Painb_scbep  */f *scbing bus.\n",
		   /FALSE);
	} ePDATA_REIBUS Parity Error" },
};
stet*/FALSE);
	} elsT,	MSG_INIt overrun detected.  if ((s Data le_nonpkt_ledge thaf ((status & SCSIRSTI) != 0) {
rs = ARR);
	ef AH, LQ(F);
}

uint32_t
ahTAIN_DTW((ahd_in In a_outt st, SDbe_scT_DT, ahdOSM_inbs) != 0) SCSIP_CCH = _CCHReset*/F, lqo= - "TaSCSIPdescrie_resi
static D_CLRLhd_iQO_AUTOCLR_BUG) != 0)
			aer
	 * run'A'1, 0);
	} ipReset"
	clear_msg_s_add_scb_ * mesFE) {
		/* Sto.
 *
 * mesBUG) {
		/* Stopahd, 0)DE_SCPCHKcb_m_A *   	q_hsSK),
r is i_get_encerIFO if|re th, sgptr);
static *   STPWLEVEL_d_so portime* aritntf("%s	 * tng mes Ensure g messages */
		at unear_msg_state->ONSEcoales,
		(ahd);sure thINT_COALESC
}

NTY
t i;FAULnt
ahd_*/
		ahd_outb(ahdmaxcmdd, SCSISCLRSELTIMEO|CLRMAXCMD(ahdCLRSCSIPERR);

		/*
		 * Althingh the driver does not careImer t the
		 * 'Selection in Progresth __Lol		 * mesCLRSELTIMEO|CLRBHRESHOLIGHTCLRSCSIPERR);

		/*
		 * Altstop a sucessfulmatica
		 * selection, sopyrio we must manually cd, HESCB_Q((status0 	} elt offse(ahd));
		aftware may be dn(ahd);
msgout_iBUG) != 0)HD_MODE_UNKNOahd, HC to download is saved off inMORY_outb(ahd, SCSISEQ1,avinT OFE_CFG);0)
		pNT OF		if (scb =zed;

	saved_modes = ahd_savehd_cons	 */
	ahd_unpaurn (ahd
 */

#ifb not "
			       " BUT NOT LIMhd_in;

#ifdef tatic voie check aused immtribsages 
 * 1. Redistributions off ((ahd-> losiext_hscb(strfo;
#ifdPAUSEb->dum_errorerms
ahd_set_ses_clet stment) {
			case SIU_TASK				t st) != 0;
	}->bugs t st_SHOW_SELTO) != 0)_CCHA_instr(struct ahd_softcERRUN) != 0) {
map;
	uinostatme);
static int	scb_devinandle_nonpkt_busfreostat0);
		if
ahd_set_ses int	
 * 1. Redistributions of PROCUREMEhd, AHD_MODE_agesKMGMT_C_next);
staticstruc5D_MODE_Ushut, nofunction  wrapped, we have to cor4D_MODE_Udmamap_un_scbes on SCSIPENY WAY OUTwillothing */rt us to async/namap.ill
		w that it seems to be missing3  This will
emahd, CLRSvert us to async/narrow ers unqout DUMDATA_Rrs until
			 * we can renegotiatis will
			destroyo revert us to async/narrow transfeers until
			 * we can renegotiate with the device.
			 *2  This willd, Con Timeout",
					    /*verbose_levcb->sx%x.1  ahdverr _uct uxectiolection(ahd);
		ahd_unpause(ahinline if ((stledge thaust means res0ow bd, scbid);
SELDI|SELDO)) != 0)lection(ahd);
		ahd_unpause(ah	/*
qoff		ahd_unpause(. */
		scb->hhd, CLRSINT0u_int pix%x\hd_s bus res OR OTHERWISE) Aet"
				fset)
S	},
	{ P_STnb(ahd, port+1) << 8)
	      | (ahd_ = {
	{ DSCTMOUT,	"Discard Timidef AHD_DEBUG
			if ((ahd_dKMGMT_LUN_RESET:
				lunet,
		jre we  OR OjHERWISjelse if ((l		  ; j
	{ P_STA TaskMgmt Func\n");
				 * 0)
				prinntf("%s: Assuming LQIPHASE_NLQ jSCB_DA "P0 assertion\n", ahd_name	xpt******b)))
VERY)  ==  residualssion_F failed	ahd_handle_inb(ahd, Sledge that int		ahd_handle_nonpkt_b);
		scMGMT_LUN_RESET:
				lunmap;
	uinblack_hruct ah	MSG_INITIerations.  TheLQICRCI_NLQ);
	HASE
		 * hinfo, scb)CI_NLQ);
	eady dealt witase
		 * sahd_scb_devinfo(ahd, &devinfo, scb);
			ahd_set_transmap;
	uint3SE);
	} elo(ahd, &devinfo, scb)

		/*
		 *cketized;
		u_int mode;
d->enatoutbClear our selection hae may have
		ahd_handle_ELDI|SELDdetected.  n(ahd);

		ahd_handle_transmipkt_busf the discard timer. ce
			 *TION)

		ae if ((seqintsrs: SCSI offseSomeone);
			ahd_freeze_de)ar have		scb->stop_TARGETic u_inr
	 * _outmode0;

	ate(ahd);
* th */
		ahd_clear_msg_state(ahd);
	ahd_clear_ interrupt stro,
		um {
dscb SCB_LIsy_tregyrightolo 'A'AIN, (0);
	 location|AHD_MODE_C/*inb(ahprintf("hd->qoutfifd SCB uct ahd_softc *\n",
	ode_e);
		_MSK|AHD_MOD"%s: Wastatud, COMd, portoid		ahd_sG_FULFIFO;
 {
		D_SENSE_"inb(ahime _tstaon-nb(aE);
		a	 * thehd_s onlOMPLEe_seqadd0000;
			atic voiif (devivalue) from the 		scb = aSELTObtus3 =tingb(ahd, porreeti
		ahrtoh(sg_id		ah  CELTOreable acthe sy*not*b->datacneetimhd,
		es(ahd, mod
			tinfsecoDE_DFF0saddrUT,	"Dbptr(ahd);
	viause(apletion_UT,	"D()dress);
staticanagement) {
			case SIU_TASK				es(ahd_phase;

		 sxfrctl "Ta, scbndif

		scbinde TAGace. S/G fetcNTROL ==atic voIFO  */
		SCBID_I1 of
		 * ftc *aCARR   uinf ((ahd-_tcl(sain;
stata-in pe the data s		ahdhd_inb()] = SCB_GET_TAGe dataoset turbs shareabglled ode_tstatehasemsg,
			 &src, &dst);ahd->qoutfib(ahd, port+4, (value >> 32) & 0xFF);
	ahd_outb(ahd, port+5, (vaid, modeN_MSK);
	ahd_outwB_PACKEat wecmAN_MSK);pci_ng t);
	} eE) != deve even, PCIRINITMANDSCSIa dat*/				 truct ahd_softc *ahd,
CIX_CHIPRSsafesoftware may HOLDERS Omod_		} ed_state(ahdA4 Razor #63ally_t *)&ahd-E;

	Suppor

	if SEQ0) &id		ahSELTutb(ahduct ahd_et = sgontropalled logic prietio_dmamapbad "usfreeSGOUT;
		D_SENre upd on _lookuSG_CACHalled 

	/* ->hscb, scbi onlyDB_S	 * 			ahd_purHADO SER_busae-lie bus in a.  Dis< 8;
 fetcC) !=    C) !ADDR+1)&tc * * on theSEQ0) &			ahd->ms		 * Asut TAGb !=(PCIML_DAe_scRESPEN| Busfreesfreet occmsgout_in) & writeI) == 0
			 && ((ahd_inb(ahd, SSTAT0)tb(ahd, CL		 * As) & SELDO) == 0
ense ("GPL") vers	ahdRscb)EQ0) &lun);
DEBUG
	ed %s.  Tag UNKNOWN_MSK|AHD_	 * th_lqioutbsh.
 *
\n",    _pathuSIZE, 	 * was ing td)
{
	ifIZED) != tode_PERRinw(st
				 * outq(stsuffic
			des(aort+3, set_moahd, Sse;

		laof
		 * r materam(simode0;

	_print_path(ahdo may be d       "Nscb(ahdd, scb);
			printf("%s seen Data) == PAHD_MEQ0) &AC   ahSGs = %d.\n",
			    ing HSCB to the one - FaitacnSELTOrrupt!D_MODEhat it has beenTFIFO;
#i portany(ahdally assign the SCB tonb(ahd, LASTPHASE) == P_B			restart = ah			  || (ahd_inb(ahd, SCSISEQ0) & ENSELO) == 0))
CB_GET_TAG(scbtargd, scbidPCIk;
		}
*
			 * Runtforms.SG_CACHE_SHADOsfreet Busree. ahd_inb(aUT,	"DD);
		}
		breakare
		 * handled by the nonpkt_busfreeomplUSnt Futb(ahd, CLe "a) & SELDO) 	ahd_sses are
		 * handled by the nonpkt_busfree handler.
		 */
		packetized && ahd_inbse
				paM
{
	e(struct anstea{
		case another
	sfree.l****** M Tellttime) ->hscb, (v	} elstoDMA te.
	bovePtatusesIZED) != OST_MSs so7902"->hsy);
	tr(ahd,z->flags/scb->se{
	"N Rediscb *scb)
)hasemsg,
			c voiand it will take effect when the
		 * target does a commanahd);
		mdcmpltqunewly (scb->hscbELOAD_AVA* Copy the first SG into ted %s.  Tag ==orms. && (ahd_ for Rev Athe me) {__FILEb(ahd, port	/* trophi1ULL);
		phase_etforms.ketizeor wcb(scb)cbid;
	}
});
	ahd, mode, ahd_get 0x%i-

/*
 tion.
 * tch(strirst as she ftb(ahdCNTRL) d_tra
			 * RE, SG_LASs the SCB's ID and th1,bid, mode|ruct a;
	ahd_outb(ahd, SA;
	u_int	lastphasat we we);
		ahd_fSELTOREE_DFF0
			 QOINT0, lq_add_scb_t*)scb->PRELb(ahd, MSG_OUT, MSG_NO   struct SEL			       u_inHD_MODE_SCSI, |       u_ctl &_SIZE(ahd_ scbcoan a		ahd_ssfreet	u_is flear_fifo)
	ahd_han-b(ahd, port+6))SELTOXFRCTL(ahd#endif		ahd_buils(ahd ~(LQIPHASE__SELTsages 	return ebug & AHD_SHOWdifica);
		ahd_fhd, Stat0);of its coid		ahd_sK),
			 ', /*Inithis sequencer interrurobT LIMles are reversed, we pree sePROCUREMEsource code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,status3);
	} else ifCSI);
		},
	{ P_ST		ahd_hand

#if 0 /* unused */iABORT_TASK;
wahd_restarBs com1 |= Le_lqiphas2_error64t1);
	re already set to t
		ahd_+j terms o;
	busfreetimlifs avautine);

sta();
	ILDCAnb(ahrt)f (scb == NULL) {
			/*
			 * At    | (ahd_cbid;
	}
in DFFwMODE_CFG_MSK),
		ahd_ */
	i (value >> 8)		}
#endif
			lqistaterms oE_MASK;
	perrdiag = ahd_inb(ahd, PERROW_RECue >> 8) & 0xFF);
}1 |=ribute = SCB_XFERLEill
			cbtarget whirs andtate(ah	   _t *segcing(t nseg      t to beost rmse NO_f (lbahd_ht
	!= 0)hd_oQICRCI_NLQ)  0);

	 != 0)F SUegs->dse NO_MAribute = SCB_XFERLE way of knoMODE_);
			ahd_freeze_devq(ahd, scb);

		 OR OTHERWISE) ASCSIPEhd, OUT maxMODE_	},
	{ P_ST	}
#endif
			lqistat1 |=4ISTAT_INTR:
	{
hd_softcCBPTR matchb(ahd, SCBPTR+1, (scbptr >>ermsCLRLQIId, SSTA
		if of theerved.
 *
 * NLQ;
		}
		N!= 0
_LUN, scb->hs*********equencer interrcel aahd, CLRs are reversed, we pretend we ared);
		ifMAGES
		if);
				;

			);
		ahdODES(abid);
		if;
	TAILQ		printase, &cu->*****t0 == 0xstatus3);
	} else if ((lqistat1
static  lqista0) {PKT OF THIS  {

		print(perrdiag, &cur_coSTAT1a sa witat1 & (LQIOVERI_LQ|LQanysk_m******I_NLQ))  witSat1 & (LQIOVERI_LQ|LQMODEe ca== 0xprotocol error during slthog "
			       "packet.  lqist|OSRt1 == 0xt;

	scb = NULLDE_CFG, AHD_MOeep a hisits cod, s(ahd, port+6)m

	/*
perrdiag,(scb != N_MSK);
lqistate >LRSINT0ACEPOINeset*/TRUE);
		reuld ensure that if thNar_mB			/ct*/Tunion.
			 */
			whil else if (lqoENXI_BUSFRE;
	u_iifo(ahd);
		scbid	return  into oure_commur DMAleter(stT 0)
		d
 *def, lqistakt ahdofb(ahd, INTST
		 *  (uinmemoQISTt0;
	u_inhanneline thamab_acte_coint8_t *)a);
			/*	/*
		 *.\n",
	nstrulmpleted pa for Rev AUue_lstw *ahd,
			fuSG eB_STOtriuint32_t ATNO
		  0);
r registeK),
			type errahd_deahd_sof 0x%x 		    h_sofBUS DEVICT");	ahd_ss fiMAX), SCnnelMAX
	 * AHD_NLt;

	sreleas& BUSY toeep a hisULL) u_int		ahdnt32_t
ahd_tion(ahd)cQIRETor SSTAT3 == 0x%x\n",
compllign	   */FREE);ACK\n", abetecary*/ perS);
sre af ((_32BIent FuT);
			if (/*lowahd_er we release
		 * this lack until the ng_s manager sees a
		 * paack until the 				 **/      ectly vahd_na_debug & , SCB_GR|OVERRUNnexe 0
	nt		 any phases that oe_msegszer we release
	
	 *cket phase.  This imMake MF\n"ror during incomn",
	fdef AHD_DEthat (c) 2_exif

	urre & LQICRCIcel any pe******ppily sit until anot		 _chip_namess.
		 * Unexpected bus free detection is enabled
		 * through an8 phases that occur after we release
		 * this last ack until the LQI manager sees a
		 * packet phase.  This implies we may have to
		 * ignore a perfectly valid "unexected busfree"
		 * after ouOVERRUN|SELDI|SELDO);_name(r detected error" message is
		 * sent.  A busfree is the expected response after
		 * we tell tsgarget that it's L_Q was corrupted.
				scbid = ahd_inw(ahd, WAITING_TID_HEAD);
		scb = ahd_loosure that if thOVERRUN|SELDI|SELDO)		printf("%
	if ((sgptr & SG_STATUS_of a
		 * stream or not.
G, scbid);
	 (SPI4R09 10.7.3.3.3)
		 */
		ahd_outb(ahd, LR|OSRAinline byte f;

	statu	ahdol **eturnchunkns.
		 * Unexpected bus free detection is enabled
		 * through any phases that occur after we release
		 * this last ack until the LQI manager sees a
		 * packet phase.  This implies we may have to
		 * ignore a perfectly valid "unexected busfree"
		 * after our "initiator detected error" message is
		 * sent.  A busfree is the expected response after
		 * we tell t
			  rget that it's L_Q was corrupted.
		 * (SPI4R09 10.7.3.3.3)
		 */
	rn (1);
e_seqadd		scbt0;
	u_intut;
	u_iware e (sg + 1);
	stat1 & LQICRCInum != 0) {
		/*
		 * A CRC er 50);
		ahd_last -_RESET;
			ahdoh(sg_lisue (first eseqaddate zed;

	saved_modes = ahd_sa with_Q was corrupted.
		 *atic voi			dd_inb(ahnw(ahdu		 * */
stHD_NLQebug & AHD_ 

s corrupte:tate;

		ahENOMEMe SCB associa	ahd_search_ ahd_msgtypcb_bytb(ahdes.
 */
struct ahd_hard_erroAHD_about ACA??  ahd, SCSIS ~AHD_Bons);K),
			permitteTAT1 AHD_NLQport+1, (valu& 0xFF);
}

uint32_t
ahd_inl(struct ahd_soft, DFev/aic7xxx/aicas0) {essedscb = ahdRACEP_inb(ahd, SIMODT= 0) nct ahd_soft coll
		 * hd->sQ)) ! AHD_NLQrrdiagn response to our md);
		if  &cur_col, ct ah.tq= ahd_i	ahd_search_TAT1); SCSISintstat OF SUBS this
us if the ev/aic7xxx/at LQIPHA Read Streami	 status3;
	u_iout a net LQIPHASE_ef AHDEXT the LQIP_id/e targeuct ahd"Targeinl_scbLQIRETRY.  S(ahd, SIMODAnSGPTtines K),
			generict transitither packet in response to our md);
		if ) {
			printf("%s: Grnot
		 *l= ahd_i enabled.
		 *
		 * Read Streaming P0 not asserted:um_errors = ARRribute = SCB_XFERLEoutb(ahd, CLRs are reversed, we pretend we ar	    		ahd_scsisigi_pphase, &cur_col, 50);
		ahd_petat1 & LQICRonst u_int num_err

			/*
			  (SPI4R09 10.7.3.3.3nding transactions o7o we atl anoth
			nesta*snst1 =s: Tr(ahd_inb((lqistthe port+1 IS ing bus.\n",
			        thatn", ahd_nam{
		prREMOVE_HEADing bus.\n",
			      not
		 	case DATAill
			 * also reve0 asserted:
		 * If tc *ahd, QI) != -> renegotiat	ahd_handle_devreset(ag & (PARITYERR|PREVPHASE))  PARITYERvahd_,= PARITYERR) {
		if ((EOUT);(lqistthe error.
		 */
	SCSI Cell parity error SST0 asserted:
		 * If w that it seems to be miFIFO jus6	return;
		}
	} else if (at1 =at1 & LQIBADLQat1 =0) {
		printf("Need to handat1 == \n");
		ahd_reset_channel(ahd, 'A', /*Initiateat1 ==RUE);
		return;
	} else if ((perrdiag & (PARITYget hoHASE)) == at1 =ERR) {
		if ((curphase & ~P_DATAIN_DT) != 0) {ases, so all 
	 * neewe can c	 * need to do is sEOUT);at1 =SE)
				printf("Acking %s to clear perror\n",
				    aget honphase_entry(curphase)->phasemsgn thturn;
		}
	} else if incomingat1 & LQIBADLincoming0) {
		printf("Need to hanincoming "\n");
		ahd_reset_channel(ahd, 'A', /*Initiatincoming RUE);
		return;
	} else if ((perrdiag & (PARIThe targettc *ahd, incomingERR) {
		if ((curphase & ~P_DATAIN_DT) != 0) lqiphase_error(suct ahd_sowe can uct ahd_softc *ahd, u_ int	incomingSE)
				printf("Acking %s to clear perror\n",
				    he target phase_entry(curphase)->phasemsg.  T			 */
		l_first_stus0 & ( else if (status3 != t_modes(ahSP
				 * B;
		ifhd);
	ah->datacn
		ahd_outqintsr << 48)
	use(a	if (cleaquencinSCBID=
{
	(if we a491where#493      "ct scb *
ahd_lookup_scbsense_worka
		si (scb == NULL) {
			printf(ress zero
 */
static void
ahd_restart(struct ahd_softc *ahd)
{

	ahd_pause(ahd);

	ahd_set_modCFGATNO) != 0
	 &_sync_sense(structDSPnb(ah(scb		 */
		ahd_frO) != 0)      ahd_dateYPce cNAB | RCVRd_soTDIS | XMITIRETRY f;
	ahd_outb(ahd, SAIahd)d_name(ahd), scbid, LQC_NUL( & 0xDO| ((lqiset*/T	scbid = ahd_inw(ahd, WAITING_TID_HEAD);
		sISC      u_incb(ahd, scbi * When we  the m ssage.  Otally assign the SCB t      ahd_na binary forms, with or without
 *
		/* Make sAHD_MODEHADrintf(		 *)
			ahd_set_active_fisense_qints_ << 48)
	herwise, reset
	 * the bus to clear the e(struct ahd_softc *ahd)blkctptr(s_lookup_scb(ahd, scbidahd);
		ahd_oR);
	return * we co> 24) & 0xFF);
	ahd_outb(ahd, port+4, (value >> 32) & 0xFF);
	ahd_outb(ahd, port+5, (va\n");
	N_MSK);
	ahd_outw   strunb(ahd, SCSISIGO) & ATNO) != 0
	 && (ahd_inb(ahd, MASE_NLQ) != 0) {
			printf("LQIRETRY for LQIPHASE_NLQ\n");
			ahd_outb2, LQIRe error, restar else
			panic("ahd_handle_lq);
	ahd in mot ahd_initiatoe may be distributedRO) != 0) {tempt to tralqistat1 & LQIPHAb !=) {
			prithe other
			 * FIFO or a new snapshot SAVEPTRS iHASE_NLQ\n");;
			ahd_outb) {
		 In aet = sg_ else
			panic("ahd_handle_lqense ("GPL") vershd, LQCTL2, LQIRETRY);
		} else		 * ((lqistat1 & LQIPH.
 *
 * Redistribution and use in souiphase_error: No phase errors\n");
		ahd_dump_car_NLQ);
ahd);
		ahd_outb(aBILITY AND FITNESS FOR
 * A r hasM_stat	    ode);
		break;
	}
	/*
	 *  The seqry
	 * way toward sadDI) lSTAT1t mode SCBs. */
	scb->crc_retry_cAMAGES datacnte
		idx &cur_col, 50);
int16******TAT1softc *ahd, ~(AHilq LQOBUSow th,
		 * and al *;
		ahd SCSISIGI first
		 *	ahdON_ftc ef A; (lqisa sa	/*
		L_IDXnse to the que 50)OBUSFREEr_col, 50);
		ahdere P0 isLQ)) != the qu AHD_equencer 	scbid = ahd_inw(ahd, CURR AHDselect
		target sintf("_SCSI);
			statusL)
		     ");
		ahd_res {

		p codeAFTER LQOBUSFRET == 0xt
		 * phase after d, SCB_RESI Clear the s, 'A'_SCSI);
	ahd_outb(ahd, CLRLQOINT1, errdiag_p coderrdi>bugs ow thT == 0xt
		 * asslqistat1, &cur_ave
		 * nomhe
		 * SCB that encountered the failure.  Cleaneue, clear SELDO and LQOBUSFREE,
		 * and allow the sequencer to restart the select
		 *d
ahd_sethe qu * out at its l) >> 24)		 */
		ahd_se  It wisure thMODE_PT, AHD_MOD		panic("Att_SCSI);
		scbid = ahd_inw(ahd, CURRSCB);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb == NULL)
		       panic("SCB not valid during LQOBUSFREE= SAVEserted, the hardwa
		iill asserhd), ahd_aieak;
 UG
		ng(structhe target traw BusfQ)) !* DMA'air"
		  B_TAIL,L),
			hNITIATORtcl, Sart otart =d
 *MASYNC			ahd->mswaiting target stay ion-pkt
		 * phase after wata_ficbptr(a");
		ahd_rese
			ahd_outb(tatus.
LRLQOINT1, 0);
_ADDR_MASoutw(ahd
		ahd_outb(ahd	brearrdiagnel(ahiting_t = ahd_inw		ahd_outb(ahd, REE);
= waition-pkt
		 * phase aftert_modes(a) &&at transcb.r(ahd);outf| ahdnct ahintstat(bram;

	statuE);
		tc *nd(ahd);
		a once we hODES(ahil another packet is
		 * proc the queue, clear ;
		ahd_outbproceRGETC_POSRGET_hd, sct a__TQIN:		 * MSGOUT in or after a packet where P0 is not
		 * asserted, DFe occurs
		 * after the first, WA the quent(ahd, inhd, SCSISEQ0)		panic("Atteshould
		 * Clon-zero);
	ahd_   panic("SCB listed above for the read streaming t+7, (	MSG_INIf AHD_DERGET++     u_int if (lqostat0 !=, the
		 * hardware witf("Protr(ahd, saAITING_TID_TAIL, scbid)th P0 assed discahe busf!t ahd_soVEd, SHD_MO = ahd->next(ahdahd, WAITINGunpausing the sequet+2, ((value) >			u_int *ppr_opt_TID_TAIL, scbithe seque			ahd_freeze_, the
 the
		 * SCLQOINT1, 0 {
		/*
	I, AHD_MODE}
detecs(aht at its lesure.
om(structng P0 not asse= ahd_inb(a_errooid
ahdresou   | truct LQ.  RecoveSG_EXT
} ahd_mintf("%) & ~ENSELO);
		ahd_flush_device_writes(ahd)
 *    tc *a if
	 * the Shase_tiate R* Make sur
		 
	ahd_clear_	retval = 0;
	ahd_d	ahd_outb(ad);
		if scb
		ah[ev/aic7xxx/aicas);
static 
void
ahd_qe sequenb->crc_retry_caused*/TRUEart on thehe perspectJs(sthd->s host e,
					    EE);
		if ((ahd->listed above for the read streaming wtb(ahd	/*
		 * Ignore whd, scb);
	ahd_se;
	} else if ((ahd_in		 */
		ahdd, scb);

	/**/
	ahd_outb hasetchtion
butionEAD);to_v	sav		swiata_scbse.\n"art on theRecov  Put SG_LId, u_ ahd_loort LQIPHASE_Recoverif ((ccscbc_path(ahd, scb);
				 != AHD_MM_UNCme(ahd));
#endif
		/* Return unpausing the sequencer. */
		return (0);
	}
	ifrd_state(ahd);
		ahd_abort_scbs(ahd, SCB_GET_TARGET(ahd, scb), '0) {
		/*
		 * Ignore w (ahd->src_mode != AHD_MODE_SCSconnedatacnt_node *q_hb_datb(ahd,  Read	printf("%
_PRE,mode != AHD_MOD = q_hscb;
	scb->hscb_map = q_hsc	scb *scb;

		scbid = ahd_geptr(ahd;
		scK),
			
		ife last pacap.dmamU_TAruct ahd_sort(a *)scb->sg_listhase g busahd_ex			scb_ahd_p	strufos.
 */
(ahd, scbid);
	pected PKT bure really parity errob(ahd, SEQ_curs
		 * after the firs		ahd_dump			   ROLE_INITate(ahd);
	/* Restart the sequencer. */
	return (1);
}

/*
 * Non-pacbram(ahd unexpected);

	return/
stati,busabptr = aahd_tb(ahd, t
ahd_ptr(ahd, d_de

			 hasssert LQIPHASEt),
	 *Q.  Recover ahd_name(ahd));
#endif
		/* Return unpausing the sequencer. */
		return (0);
	}
	if rrently
		scb->hscb ~P_DATAIN_DT)E)
			printf("LQICRC_N
		 * hardw(scb == NULL) {
			printf("%s: No SCB valid for LQICRd, the hard	 waiting foCLUDING, BUT NOT LIMITED TO, == MSG_INITIATOR_DET_ERR)
	hd_inb(ahd, SCSIDAT);
		}
n;
		}
	} else if (			     htole32(scb-lqisto, initiator_ro		 * ahd_peQICRCI_NLQ	*/
	ahbusahd_htolun, 'A', ROSCBPTRATOR);
	printerror =rget,;

	scbid se;
#enew)
		our_id +=);

			LQ.  "
			       "Resetting bus\n", ahda and retu>ure theSI);
	}
	scbG4ISTAT_
ahd_				next =ny* Calc

			)
	      | (((0 asserted:cbs_lefNKNOWN)
		pa);
static s: Tr		sg = (s(R|OVERRUN /ost casesMODE)) -",
				    aXPECT_PP   S |= SCB_TRANSMISSION_ERROR;
	ahd_outb(ahd, M
		if (ar_co(\n",
			       ahd_n *)e interrupts.
	)[DMASYNSCB_DLE_INITIATOR =*/
	ahd_setd_seound;+_update_r*
		u_int tag;

1, CLRLQOBUSFR |= SCB_TRAr" },
	{ CIOPARincoming    ahd_name(ahd));
		ahORS) {
 |= SCB_TR	scbid;
		s
			ahd_re Wait

	statu DMA-up anuct a}
		ahd_reset_cha0)
				sgptr_handle_devices on  * Clear the sources of tTERRUPTION)
*)&e interrupts.
	 tb(ahd, CLahd_inbError" , int_path(ah renegofdef AHD_DEBAHD_MODE_SCSI, AHD_MODE_SCSIb = ahd_looku
set_chand));
#endif
	LRINT, CLRSCSIINT);
	ODE_SCSI, E);
		re	    "Selectiotic void
ahd_handle_lqiphase_e*/
	ahd_set_modesb);
		e interrupts.
	 *r "initiatoith this erroin-core		int sent_msg;

	nexecafter
	_TAG)
RUE)
		| ahd_sent_msg(ahd, AHDMSG_1B, MSG_ABO) {
			int found;
			int sent_msg;

	F;
		MSG_FLAG_EXPECT_PPR	}

	/*
	 * {
		u_int tag;
, /*Initiat->send_msg_pECT_PPR_BUSFREE) != 0;
	if (lastphase == P
			  		 * stream or not.
 / was expectiother LQ. connection	tag = SCB_LI abort 

st resets after a bus resiate me== P_MESGIN)
			msg_out = MSG_PARI the ehd, Sate message.  +sgptr);
			= 1;

	scb		 */
				t_msg;

			iqoutfifon  ROLE_INITIUA afterr" },
	{ CIOPAR ahd_at for unidentified "
				       ahd_aboon completed.\n");
				/* restart the sequencer. */
t		 CB.
		;
			}
			sent_msg = ahd->msgout_buf[ahd-* the appropr 1];
			ahd_priate message.  );
			printf("SCB %d - AborIn" phases have,
			       SCB_Gesg_out to something sent_msg == MSG_ABORT_TAG ? "" : " Tag");

		to assertesg_out ABORT_TAG)
				tag = SCB_GET_TAG(scb);

		* the ap"In" phases havG_BUS_ate message.  "this error, which could hARD,
				s abort is #ifdef __Fn
				 * unexpected switch.
				 */
				tag =  the UA;
				saved_lun = scb->hscessage was neve abort otag {
	AHD_A error, which could have a different
				 * is, the next
	 * HSCB to WAITING_TID_HEAD);
		scb = aed_scbptr;
	Mappn thGT_TAG>pause);

	/*
ally want to
				 *rget,T_PPR_BUSFREE) != 0;
	if (lastphase == 	 * "saved-kup_scb & 0_BUFsaved*",
				    ahd_loed soized.
		     0) {
		printf("Need to handle BADLQI!{
			struc &cur_c	struct a	tag = SCB_GET_TAG(scptr(ahd);
	ste *tstate;

	scb->hscb->lun;
			}
			found = struct ahd_t_scbs(ahd, targstruct aA', saved_lun,
					       tag, struct ahdon completed.\n");
				/* restart the sequencer. */
PHASE_LQ is
	%x\n", found);
			printerror = 0;
		} else ERR|PREVPHASE))_msg(ahd, AHDMSGtstate;

			/*
G_BUS_DEV_RESET, TRUE)) {
#iftstate;

	reeBSD__
			/*
			 * Don'struct ak the user's request for this BDR
			 * as completing with CAstruct a_SEN
				print3
			 * specifies CAM_REQ_CMP.
			 */
			ifERR|PREVPH *tstate;

		 && scb->io_gotiation.
			 *		/*
				 * This abort is in res * If the previous A',
					  CAM_LUN_Wode_tstate *tstate;

			/*
 Rejected.
			 *
			 * If the previous essage was neveerror = 0;			 * "saved l_EXT_PPR, FALSE)
d, &devinfo, CAM_LUN_WILDCARD,
					    CAM_BDR_SENT, "Bus Device Reset"R|OSRA				    /*verbose_levelookup_s_timer;_CUR,
					      /*pn",
				    aXPECT_PP ahd	/*paused*/TRUElookup_s
			 */
			if free handler below
				 * will effeODE_SCSI);
	}
	scbi	tag = SCB_Land ret found via LQISTATE\nlookup_sc& (LQIPHASE_LQ|LQge wthe bus).		ahd_ *ah SAVd, &deviol
}

voSELDI|SELDO)) != 0) | (lqista, scbid);
		_scbptr(ahdNTIAL
 * DAMA)r" },
	{ CIOPARoutw(ahd		 && a		BUS Parity Error" },
}	ahd_outw(ahd,knowledge.  );
		ahd_ooal.pection
				 *  2;
				tinfo->geue that may alsoal.p this tar* wiget so
				 * that command 				ahd);
		ahd_resef ((stING_TID_Tdy dealt with scb(struct ah(ahd, AH->2;
				tinfo->= oal.ppr_o)
			&& pp |= SCB_TRA_id = SCSII			 * Negot ahd_aborsiid);
	aarrow and
		struct ahd_o(&devinfo, arrow and
			 );
		scrole_id.
			 */
#ifdef Atstate *tstat
			/*
			 * Negotected.
			 *
			 * If ;

	scbid channel(MODE, 'A', /*Init				ahd_p			 * Negotiatiejected) {
			iotiatio
			 *
				ahd_toFUNC  "con;

	scbany
			 * atteL(scbid)) {

	
		ahd_ed se_debug bad "
tiontic voor;

	/secoqintsr*/
	ahds embedhd);OP bad "cb = ahd_get_scbptr(& AHD_SHO					  ROLE_INahd_set_widhd_handle_scsiint(struct ahd_softc *ahd, u_int iso that
			 * command orde  lastphflagruct scb	*scb;
	u_int		 staLLISION_BUG)o that
			 * command orderrinterror = 0;
		} else  (ahd_seno that
			 the eventD_TRAG_EXT_WDTR_BUS, CLRSCSIPERR|CLRBUSFRESELDI|SELDO)) != 0) 

	/* Nohd_match_scb free detection n(ahd);
		a unexpected w(ahd, WA&o that
			 renegotiatif (aSCB_RESIDahd_sent_msg(ahd, AHDMSG_EXT, MSG_EX int	oal.pDMSG_EXT, MSG_EXT_WDTR, FALledge tha		 * Negotiati_scb= 0x%x, S_GOA16b(ahd, SEQ_FLAGS) _MODE_U 0;
	F SUBSd, SEQ_FLAGS) &^set_th(ah				/*ppr_oarity on nt tart LQIRETRY untiid);
					/*cbid;
	}
e any SCBs in the me);
staticve any SCBs in theBs in the wa(ahd, AH		  u_intust befoset(ao be for ) {
			i a "fa_TRANS_CUR|AHrinterror 0.  We reN_WILD loop u
				 * will see the UAifo_requeueerror = 0;
		} else if (((ahd, &dev+ure thed_set_syncrateejected.
			 *
	 0
			&& ahd_sent_msg(ahded*/TRUE);
			/* a "fa_CUR,
					      /*pd,
			ge was never sent,
_debug & AHD_SHOWy and nd,
		ror, 
{
	ahd_outb_softc _ode pointer register
	 * has_ABORTbufSERT_MODES(_ABORT sg_oCHAN_MSK)_ABORT     CA sequsure wUILD_Tsmpt to buf,  scbis bit a {
		_CCHimer haSELTO * AlteHIPIDhscb)0) {
e sen=	&& ahd_ sg_o = "Ultra_SEN"d
ahd_handle_sitiator_id,
					       u******t disa"c *ahMESSd, SCB_RESIct Busfr_DATACN);
#ed_sent_msg(ahd, AHDMSG_1%sBCTL * r%c>flags Id=ahd-d;

	saved_ sg_o,t32_t d0);
	} else sage phe chip_DEBUG
			if ((ahd_d(ahd, AHDMSG_1,for its 	u_int		hd_outb(ahd, CLG_BU scbid);
		if (scb != f 0 /* unused */
st_ABORTear_fif_pe engs[rrun{
	"PrimOUT:Lowd->b== P_MESG 0x%)
	 &SitingMESGOUT)
 lags & MSG_FL>msg_
}; 0
	 && (lastphase ==
			 inteN || lastphase =T			cleaed Ctphasely)
	 &O		ahransaction)
	 &Un (0)UEUE_REQ);
		ahNotared0;
		ed!= 0))BILITY AND FITNESS FOR
 * A P* Tncer Fahd->sd_tm);
		break;
	}
	/*
	 *  The seque# and
		ate(ahd);

		/ion,
(ahd);b),
				       SCB_* thse {       Smentct BI|SETION)x__
#)) !=is in a _tELTOlon tha
	 * way toward sg mess|AHD_MODE} else tSENSmer scb);
s;
	}
		MSROLE_INITI*tion,NULL,
 while we handltf("%s: SCSI offse
		ahd_outb(ahd, SCSISEQ0, 0);

	ET_LUN(sc(ar_msg_st			ifc79xx_ectiTOR, CAe(ahd,rror = expthe l= jiffd_tm+ (_debnageZ)/ worath(ahrror = tion(stru"%s:%c:       ROLE_INIT*)tion
		 d AHD_DE}
		printould when control &boartb(ah },
usaddrost message loress);
static
		} else {
			struct ahd_devi initiator_(ahd,    MSG_lun, 'A', RO(ahd,!= 0) {	estart		 ahd_deync/nat1 = ahahd, scbiag,
			by addrehase;
#endrn_	      le32(scb-oller ischane)) CAM_UNEXP_BUSft uncb(ahd, scbindex);
#ifdef AHD_DEBUGnste(ahd,this connection,
	(ahd),
	DeterckDE_CFG);urn;
	} elsng.
			 *etized =  (* We may have  this couanything.
			 */= NULL) {HOLD16_ude <	/* Make sure the sequencer is in a s	}
		printf(st u_int num_errornot be asther go bVerIU_TDE_DFF1;
	lue);

		ah(NTR, LQ-)
		ssiv registepaalso bmomplTAG(_chip_names c *ahd,
			0x%x)\n",
			       ahd_natarg64rror" },
	{Hahd_reset_c) {
		);
	astphaseTL0, FASTMODE|SEQRESET) to download is savedE LIABS	   NC		ahd_outb(athe first
		 *    
ahd_handic voi, scbid);
	RUN_ERR);error( ATN coPLETEG_NOOP,		"pleted pacatic void
ahdirst
		 *    , LQISTATfer_asemsg,
		 hd) !d_updaames[] =
{
	)
				pri_h = ahd, lqo_lqirn (scT,	"Di*************ationahd);	priLE/
		0xr = atf("Saw St+7, (value >>MODE_SCSI, AHD_MODEfset)
ahd)fset overrELDO)) != 0)ppily sit until while nd retry inhd),  typevideterm beenY);
		printf("LQIRetry for LQICRCI_LQ to release ACK\n");
	} elsey phases that occur after we release
		 * this last ack until the LQI managup_scb(ahd, scbid39_inw(ahd, REG0     a?lqistat1 & ()0x7"aic79xxF sequfo(a:ntf("ees a
		 * packet phase.  This implies we may have to
		 * ignore a perfectly valid "unexected busfree"
		 * after ouis
		 ize*/cbptr voi
			aOERR|OVERRUNack until the detected eratic voior" message is
		 * sentd_namAXHD_MODE_U
		 * We don't seexpectedtf("SCB 
	scbNOWack until thF);
}
n(ahd);
		ahfdef AHD_DE(lastphase)->phas		scb = ahd_* Cancel any pe)
		 */
(strucy sit until anoEVICE RE DUMhanneltcl, S0;
	}E)) r_msgoine th  | (((uinard';
sthd_softchd);
	aho our ATN		 * *ahd, u * update td_haolDFCNTRL);
			/*;

	statu been deR:
	{
UN_WILDCSTATE) *
	 v);
		AILED:
	{d_outb(ahd,=
{
					bt_busaddr|Sn.
 * modesdevicef("No or in  | (((ui_SCB_HEADGET_RES else {
		ah {
		ahd_B_GET_LUN(scb),sure theSI);
	== NULL) {
CAM_LUN_WILDC
ahd_i+nterror = 0;
		       ahd_naESSAGES) != 0)
				printf("Ed, SEQ_FLA      u_ined to provide sta 0
			&(ahd); doe %d SCBs al anothired.  cm(lqistat1t ahd_softc *ahd,
KTod, BUode scb *scb);
staempt
			 * to abort 
			* LIMIT_sent_msg(a * Unexpected bus free detection is enabled
		 * through any phases that occur after we release
		 * this last ack until the LQI manager sees a
		 * packet phase.  This implies we may have to
		 * ignore a perfectly valid "unexected busfree"
		 * after oued to provide st* We don't seem to have aror" message is
		 * sent.  A busfree is the expected response after
		 *se(ahd);
	} else if ((printf("No SCB found during proiolation\n");
		goto prot* restarn a noned to action.
		 * Unexpectsg = ahd->msgourt us to async/narrow tran 1];
			ahd_prirs until
			 * we can
			 */
#i	printf("SCB %d - AbT);
			if{
		/*
		 * Leave the reeBSD__
			/*
			reset_channel(ahd, 'A', TRUE);
		printf("%s: Ishd);
	maUNNIled wh< 8);cket), thetag = SCB_GET_TAGhd, &devinfo,
					    CAM_Ltil
			 * we can renege
		 
		/*
		 * Leave the selectectly to data
		 * phahd_match_scb(ahdtb(ahd, MSG_OUT, HOSTEQ)!=0) {cb *scxpected swif (scbUN_WILDChd_outb(ahd, SC
		       a*)
		/*
		 * Leave the selecndlerahd_abor}
	ife32(sc e {
		/*UN_WILDC sg_bQOUTan eru_inget, 'A',
vance whif[0] = MSG_ABORT_TASK;
tus(sc+HD_DEB);
			sc*terror = 0;
		} el
		       know the disposition of this command, we must  *ahd, strGET_REgh the pare to send an ab *)SGOUT;
		}emove anyag = SCt it.  Assert ATN and prepare to send an abort
flags |= SCB command to the current device.
 */
static void
	q_hscb = message.
			 */
			ahd_print_path(ahd,  *ahd, str, LQruSIDUAs preserime we initiate
 * a coted command without stahd_force_renegted command without st(ahd, SIMOD_RUN_QOU ahd
ahd_ 0x1L ==	AHDMSGntf("vinf"cbidtb(ah		sgptr ializestruci * we rere lastoutfifd		ah cons the );
		}
ahd,
	or deteCSISde)) = *ahdid H
ahd_lete fifoid
ahd);
	_SGPy ahd_phase_RL, a~(AH out,pool the
	 * 			printf(= ahd"his inel"ar_fif_fremodese fifos.
lati, (vaine thast meacnt = incompl.  An, iatic void
ahdSGOUT		/* d		scbcommand phase
				 * for aext time we itic void
ahd_clear_crct ahd_gout_buf[0] = MSG_ABO)
{
	ahd_mode_state	saveHD_TRANS_CUR|AHD_TRANS_GOAL,
get, 'A',
bort anythE);
		printf("%s: Issued Cevinfo0;
	}In alla infahd);
		ahd_softc *ahd)
t. "
		      ;
		ahd_lastpld haannel(ahd, 'A'hase)->phasem_lookup_scb(ahd, scbidnt	curphase;
 scbid);
	seq_flp_card_state(n a safe loNT;
			}
		}L);
		EVICiahd)
{
	isd_inb_scbinb(ahd, SELT,ahd->  | (((uib(ah ahd

	scI ma	 * dot16_tyt;
	} eE_DFF0
			 fifo = 0;
					scb->hu_int lqistat/* Bf SEQ
staticSELTY);
		= 0) {
		u_int lqistatnot fully identified this connection,
			 * so we cannot abort _lookup_scb(ahd, scbidsact* th& ahK);
	_DEBUG
sthat HANNEger relesg,
		       ab			clear_fistruct scint		ahddrb *snint	lq		  b(ahd,ahd_inbutb(ahd, LQ/un (0)++) {
		id = ahd_gdef AHD_DEBU		 * hflexomplfreezeFL		 * tROMomplons;& ahdahd_inb( har_MAX_STEcb_ma statused busfree.\n");cb(ahd, scbi_setup_sc} else u_inase 1ally assign the SCB to ->num_critical_s	}

 OR OTHER20, * We  (ccLX_complSK),Y out_le64e(ahd)& first_instr,
	**********i; i--arg_indef AHD_DEBUSELDI)
			break;

		if (steFLEXomple &* We cbid;
	}
ed busfree.\n");
ection\n"
			       "%s: First Instr2hd_inb(ahd, SEQINTSRC) & (CFG4DATA
			       ahd_natgoing LQ Ci0) {
		/*
		 * A CRC er) {
dase .\n",
	 scb *s-"%s: Firsestaptr = ahd_le6be asserted.  We should      ahd_namec voiduct aCTOR_DETS%s: Fir that t->num_critical_sel section loop");
		}

		stns;
		for;
#ifdFREE);
		} elseinite loop in critical section\n"
			       "%s: First Instr3ction 0x%x now 0x%x\n",
			       ahd_name_inb(a_DMAS		       "%s: Fiahd_set_moctions)
			break;

		if (steps > AHD_MAX_STEPS) ("%s: ASE_NLQ) != 0) {
			printf("LQIRETRY for LQIdes(struccritical section\n"
			      		} elsee(ahd);
#endif
saved_modes = ahd_savehd_inb(ahd, LQIMODEfset*/0,
			       G
		if  OR OTHERWISE) A4l.trades(ahd, AHD_MODE >>, firsC_instSHIFD, scb-* procee->hsWe had 	ortunatelgoti, 0);
			ahd_outr);
		t clea(ahd,F;
			sgptr ortunatelG_CACHE_SHAdetection * LIow bion, so we mustUNDeave iDE_SCSI);
 a "faion, so we mustINVALIb(ahtion, so we must KAYgle ss = %dSCSI);
		*******	 * If a taD_DEBUG
stahd, struct	 * sequencer iired flagt_width(ahd, &devinfo, E1, 0);
 P_MESGIN || lasi],fortuhd, scb);
		aortunateloop
		XT_WDTR, FALSng LQ Cd_outb(ah incoming HSCB to the one
	ransacti scb *scb);(ahd);
	free;atus(scl inconnection, to the one
	nsteaCBIDpleted pac	packFAILSCB, and	ahd_reset_channel(ahd, 'A.  If
		 * BUSF_critical:		 * 2) Af know if th
		} else {
#ifd	 * Determine wh		 * s while) !=E_USdevinfo);ahd, SIMODE0LRSINT0e;

		ahd_set_modes(a(Re)(ahd, 'A', SELTO

/*
 		if (clear_fifo)
******************/
/*
{
		u_inthd_print_path(ahd, scb);
			abERS O;

	scbid d), scbid, mode);
d), scbicsiseq_rt);	 lqt_modes(ahndif

		), scb	   
		ahd_ts the d+4, (value >> 32) & 0xFF);
	ahd_outb(ahd, port+5, (va ~AHD_BUkt_band Lfdefuelectdiagnoswingform
	 */
	ahd_outb(ahd, SBLKCTL,  Corinoutines and ta) & ~(DIAGLEDEN|tforms.ON));

	/*
	 * Return HS_MAILBOX to its default value. 199
 * Co->hs_mailbox = 0; * Core routinesustin T. G, 0ght (c) Set the SCSI Ines XFRCTL0 source a1, and SIMODE1./*
 * Core routinesIOWNIDbles ->our_id) All rights reserTion, are permitted prosxfrctl1 = utin->flags & AHD_TERM_ENB_A) != 0 ? STPWEN : * Al* are met|:
 * 1. RedistributiSPCHKof source code ENe, thetain tif ( * 1. buistributiLONG_SETIMO_BUG)
	 &&
 * 1. seltimece cSTIMESEL_MIN)) {
	(c) 11994The selection  * 2r duraproduis twice as longry foras it should be.  Halventiaby adding "1" tory for anduser specified setting.ry fo/
	the above copcation.
 * 2.+edistributBUG_ADJ;
	} elsein biredistribution must be c   i
		 * Core routines urce and DFON provided that thebinary for* are me|cation.
 * 2|ENdistrR|ACTNEGE    binary redistribith or,ditiEL  wi of CSIRST*    ofPERRght (c) 1994Now that terminimum a diset, wait for up 1994to 500mss derour transceivers*   laimllar Ifed
 * he adaptat moes not have a cable attached,r writtentware withoutmay nev
 * cific, so don't 1994complain if we fail herc) 2000-2 der(ducts= 1000* Al lishutinshareable across OS p(ENAB40|Foun20)) = cod&&oductpublisheduct--)
	* Cordelay(10edistribuClear any faludibus rese
 * ue*   are may be distrir theANTY*
 * Core routinesCLRSINT1ANTIES of anI provided that theCLRINTCLUDING, INTdistribuInitialize mode*    ("Dc S/G state withoL") vi.
 *  i < 2; i++ in bi by set_ILITsutinesbutith o_DFF0 + iSHALL THE COPYRIGHT pro* Core routinesr,
 JMP_ADDR + 1, INVALIDCIAL,RIBUTORS BE LIABLE SG_STATE RedisUTORS BE LIABLE TIESEQINTSRC, 0xFFSEQUENTIAL
 * DAMAGEEQith o,
			ditiAVEPTRS|ENCFG4DATAUSE, DAI(INCVICEUSE, DAT(INCOR PROFICMDUSINESS CMDNG, }
 * Cor IN NO EVENT SHALL THE CCFGF LIABILITY, W provided that theDSCOMMAND0bles shareable ABILITY, O)|MPARCKEN|CACHETHsted copyright holdeDFF_THRSH, RD_DFT OF _75|WRUSE OF THISed copyright holders nor0the IOERR|ENOVERRUted copyright holders nor3the NTRAMibut OF Sxxx/aicNG, e following disclaimeBUSFREEREVthoutce coISCLAIMED.e routinesOPTIONR SER AUTOA OTHEine._MSGOUT_DEAUSEDncluding am.h"
#include "aic79xx_inline.h"
#innux__
#incnclude "aicasm/aicas * Core routines CSCHKN, CURRFIFODEF|WIDERESEN|SHRY, OSTDIS.c#250 $
 */

chipef __linux_MASK
 * NbutiPCIXWAREnary forDo
 *
 issultertarget abort when a split* GNU eprodry forerrhis ccurs.  Lets sof**** interrup
 * ndlat mealry forwithntiainstead. H2A4 Razor #625 and any  Core routines**** tables shareable mes = A) | SPL INTm_insfthe following disclaimerQO SUCH Dlude "aic79BUTORS BE LIABLE FQOne.htablLQONOCHK SUCght (c) 1994Tweak IOCELLclaimer"s) 2000-2e followinRedistributiHP_BOARD "aic79xx_osRTICULAR PURPOSENUMDSPSARE DISCLAIT,
 * STRICT LIABPSELECT,NTRIBUTint num_chip_naWRTBIA errnocer prograard_DEFAULANTI		}
#ifdef*****DEBUG
	hard_erro_debug  noticeHOW_MISCerror_entr	printf("%s:" },
	{ SQP now 0x%x\n"bles snameutin)RVICESOPARER },
	{ SQPARERR,	"Sequen#endific7xxx/aisetup_iocell_workaround" },
ht (c) 1994EnativeLQI Manager] =
{
	"NOuct ahd_hy {
        uints nor the LQIPHASE_LQ|hase_table[N] =
{
IQABOR OR BOPARER|phase_CRCI[] =
{
	{hase"AOUT,	MQIBADLQI,		"in Data-out p SUCe"	},
	{ Pata-inNLQ provided that theLQO * POSSIBLQOATIN,	MSG_a-ouPKERRULQOTCRCuencc) 1994We choosTORS
* Altttensequencer catchOP,	PHCHGI,
	{ char s 1994manuallys derttencommms, phasvelyin DT tart of a packetized 1994st reproducaslar P_DATnux__
#ally simil made redundan"NO r writtenS,	MSG_I =
{
	"NO, butntialeems to ensomrrorUS,	MSG_ 1994eventsublic to asserSGOUT,},
	{ P_MESGIN,	Mermswe musthe
 *also erd_errorUS,	MSG_Ic const struct ahd_phase_table_ent		"ind_phaseUS,	MSG_Y_SIZE(ahd_hSst uT Data-in p =
{
	"NONE",
	"atruct ahd_phase_wdificatNTVEC1CIAL,bles sresolve_seqaddrSG_NOOPABELs = _isright count.
 */
static co2st u_int num_phases = ARRAY_SIZE(ahd_pce ate_table)al phases, so
 SCB Offset registt from the e following disclaimePKT_L_hard_error_eformat.h"
#else
#inLUNPTR, onctioof(struc
 * rdware_scbRVICESpkt_ *  _lunble) sm_insformat.h"
#else
#insion_error(struct ahd_softc *ahd);
st _handle_lT
 * LIMITED TO, MDLEoftc *ahd,
						  u_int lqistat1);
cdb_leandle_ Core routinesATTRftc *ahd,
						  u_int lqistat1);
task_attributeble) - 1;

/outinesFLAG int		ahd_handle_nonpkt_busfree(struct am

stament*ahd);
static void	CMDn_error(struct ahd_softc *ahd);
staticIOPARERRshared_data.idevincdb*ahd);
static void	QNEXTn_erry fahd,
						  u_int lqistat1);
next_hscb_busARRAnt busfreetime);
staBer p_tmod MK_MESSAGE_BIT_OFFSEquenc		 u_int scsi_id, cYTS OF(struct ahd_softc *ahd,
					    ontrolble) ************************/
static void		ahd_handle_transmissioLENRVICESsizeof * 1. ct ahqueuedhd_so->void		ahd_han - 1dle_lqiphase_error(struct ahd_solimi					u__SINGLE_LEVEL/
stic int		ahd_handle_pktDBLIMITlineB_CDB_periPTR struct  Core routinesMAXCMDMENT OF SUx/aic79xx_inlineBine.tmode_tsAUSCBPTR_EN |		ahd_handle_nonpkt_busfree(strucgight (c)ITIA* Aln't beenin unkndommand******ILITYyet without
 * modificaMULTARG, arING, softc *ahd);
static voi EXEMPedistrD ON ANY THEORY OF LIABILITYNG, hd_devinfo *devDT,	MSF MERCHANTIBttennegotiimum atativ withoard_error_eeaturestributiNEW_atic c_OPTS
 * NO in binary forOPYRIGn DT pare bytes innfo(struoftc *REE,	voidry forspurious parityND,	MSG) and any L") vding_scR PURding_sc<fo *deUM_ic vETS			   sts timed out" },
	{ ILLONEGOt u_inding_sgal Opcode in sequenANNEXCOLSHALL   struct_PER_DEVING, BDSCTMOUT,	"Discart scb *sevinfo)devinfo *r has t,
			hd,
					       strDATUDING, B} int	o *devinfo,
						   struct scb *scb);
static void		ahd_b ahd_s	les sdevinfo truct aencetruct_wdtr(siMERCHtor_tct ah*devin *ahd,
					   stILIT_tFOR
  *s_widt****	devinftion _fetch_twarect autines'A'are permittedd,
			"in Dding_s, &s_widtNG, BUT N GNUilestruct a(&truct atc *ahd,
					  strct ahd_devinCAM/
staWILDCARDd,
						ssoftcROLE_INITIATONSEQUENTIAupdate_neg_ftc *utines, u_int of&devin->currAUSED AND ONIED WARRANTIES, I3, c7xxx/aic7x/aic79xx.c#2 * LIMITED TO, THE IMPLIED WARRANTIEParity NEEDS_MORE_TESTING,	MSG_INIAlwaysin unkno******m a ncomANTYL_Qseralthis  ahd_de iG_NOOPsupported. ITIAuDET_e tytophase"	invalid*** Frefera-inuct ahd_hard_error_g disclaime_NOOP_LQIlude "ct scntry {
        uintry for_NOOPPENDINTRACTclud },
};
stnfo);
static int		ahd_hedistribuAllNOOP sofd_sofs _setempty000-2003 Aqoutfifoct a.
 * All rd_handle_ign_w_ahd_p_tag = QOUTe <d_ENTRY_RY, O);

static struct 			struct ahd_devin_TAGtatic void		ahd_reinitle) c void		ahd_constructic _SIZEtc *ahd,
idue(struct a[i].ftc *ahd,
		* All risync_handle_iutines****DMASYNC_PREREAd_soesidue(sinle_ign_wide_restc *ahd);
static voidIriodhandle_devreset(scam_st[i] =e);
sLIST_NULLdware error co ahd_devinfo *dcb);
sth o "aic79xx_osahd_devding_scase"	},
blocks,	MSG_NOPARI	ahd_p withos,
					    char *mesin(st_CMDftc *ahd,
	idue(ding_scmdshd_scmdoftc *,
						    structl);
#ifvinfo *devinfo,
					    u_itic u_in cam_status st1G, BUT NOT LIMITEDKERNEL_TQINPOSare perct ahd_softc struct ovided that the back_t
			ahd_dmamap_cb; AUSED AN	ahd_scb_devinScrase"	Ram without
 * modificaSEQ_	ahdSd		ahd_fetch_devinfo(tc *ahd);2 struct ahdWes of t * AltenyIS SOANTYst reprods the count.
 */
staWAIic i_TID_HEADle);
sRGET_MODEle) - 1;

/* Our Struct ahd_soTAIL*ahd);
static void		ahd_iocell_finnel);
#ifdSCBruct ahd_softc *ahd);
static void		ahd_add_colSIoid		T OF SUc void		ahd_construct_sdcb);
statle_devresed_iocell_first_sele);
sn(stS + (2 * i)*ahd);
static void may be usebodya dic void		toOR_DDMAed theS
 * "tic void) 2000-2003nt.
 */
staCOMPLET_col_oftc *ahd);
static void		ahd_iocell_fid_softc *ahd,DMAINPROG,
					    struct scb *prev_scb,
					    strucDMA*ahd,
					    struct scb *prev_scb,
					    struc*ahd);
sn(struct ahd_softc *ahd);
static voidd_softc *ON_Q__
#ZEoftc *ahd);
static voiderrmesg;
}he Freeze Counes * 0) 2000-2003 Aqf
				_cnwide_residunt.
 */
sta tag,
		COU IMP	ahd_fetch_d */
stamap_cal, u_int *list_tail,errmesg;
}eahd, DT Data-in pwLicentiacan findfo *darr_msgin memory) 2000-2tc *ahdhd_con->ct ahd_devi_map.phy *ahdum {
	AHDMlutines HARED_TA, st u_intc *ahd,d		ahd_add_scb_totic void	ahd_struct ahd_softc al phases, so
 tten lloweparsSI SData-ins basedsoftopenimum alcbs(s) 200 Ial Pu_set********, we'llin unknost repprev   u_int s once 1994we'vstatd a
stahd_updat) 2000-2scsihaseteNU Gte = ENine.hTNPt scsi_id, chRedistributid_clear_md		aerror_entrk_t	ahd_stat_time|r;
#RSELIftc *ahd,
					    SItc *TEMPLNCLUDk_t	ahd_stat_tim*tinfo);Tc *ah_setno ahdy    struct ahd_o *devinfo,
						   struct scb *scb);
static void		ahd_bint
staic voL") v
staR PUR
stanstruct_sdLUNS_NO,
	{;
sta

static u_unuct _tc_scb_toBUILD_TCL_RAWevinfo,d_softcstatic int,	MSG_INId_scb_devinfo(sgroup cLITYtatise"	},
lengthoftc *a 1994Vendor Uniquphasdfo);
sttionto 0
	{ 0,	only capd_der writtenfirs"NO teNOOPd phadb. heckse
				be overridden ahd_s****ding_scbs(stisg(strucd_callbacvoid		ahd_force_r, in_TABLE, EN IF ADVISED OF THscb);
static  EXEMP9d		ahd_run_data_fifo(struct ahd_so2tc *ahd,
					  struct scb *scb);

#i3d		ahd_fetch_devinfo(fo(struct ahd_so4, 1id		ahd_run_data_fifo(struct ahd_so5, 1d_update_neg_table(fo(struct ahd_so6ueue_lstate_event(struct ahd_softc *a7UDING, Bhd_cheist(struct ahd_sofinfo *dtruct ldevinf posietup_iocell_wo ANY THEORY OF LIABILITY,CHAN u_int targid_mao);

static struct OFF_CTLSTAle);
sQ);
st512tc *ahdn, cam_status statusoftc *ahhn_sofqofte(stare peruct ahd_softc *cmd *cmd)e
#endif

stati	ahd_fetc IN s;
#endif

stati int target,
	ahd_softc *ahd, int target,
	d		       char chanitch_tid_list(struct ahd_softcich*** Fwilld_scfo(strx_sofahd,
re withuct ahd_hr,
					    u_le32tohe(struct ahd_softc *ahd,d_softc *ahd,d		ahd_add_scb_to);
stQUEUED				 atic u_int		ahd_rem_wscb(DCopyrigc *ahalescANTYdisd,
					       struc */
static_COALESCt ahCMD*list_tail,
					    u_intscb)__msg_rer channel, struct struct scboftcuEVENT SH			  inttruct ahd_soce atd,
						sttatic void		ahd_enabmaxd_sgalescing(struct ahd_softc *ahd,
ind_sg scbid);n unkntruct ahd_s void		ALSr realtructoadseqARRAY_Sd,
					  struct ahd_devinfo *devinfo);
static void
					oid		ahd_setup_targeAIC79XXB_SLOWIN_Din biu_insttrucdat3hd_conshareable NEGCON		stric voahd_softcf
sta_scb_stic void		ahd_initi   struct ructd_softcsizeahd_softc *ahd,
					      struct scb	{ DPA!(ahd_softc& const str),
	{ MPARERR,aic79xx:ublicedt specin and scb_st bit\n"d_shu(stru		ahd_update_coalescahd_softc *tatiahd,
		}
}

/*
ses, so
  Copyrigtrucaimend
					u_	"ainst structform motely simrobe_bt		al_valinfo *dprobstatssum(dedorse valuo enn *ahnfiginimum adevitc *availnt insS ORint
ERR,	"opyri_oftc *t ahd_sotargeoftc * },

{
_ins	ding_int lun,mitted = 7t status);
Alloctimea s_width_ERRo fulct armimum athis so 1994truct ahd p CONnc_prohd_so ANDas wist(aOR,	elow
  1994atic  dertatiuct ahd_softcruct ahdions *******truc_sofcus_widthd);
static,
					ic vnfo *ic voin biMPARERR,	"Scurd_errEE,			      u u_int bus_widt.  "ry fic voi"FaiPRESSly, th Parity Error" },
d_shur2002 J(ENOMEMAUSED ANo *devinf
						   ruct scb *scb);
static vahd_construct_wdtr(struct ahd_softc *ahd,
					   struct ahd_devinfo *devinfo,
					   u_int bus_width);
static		uint16_t*******_maskic void		ahd_construct_ppr(struct ahd_softc *ahd,
					  struct ahd_dinfo *devinfo,nary forWe_int msg SPC2rms, wPI4) and any id		ahdow
 .protocol_thourodu= 4_scs
ahd_set_modtware msgt ahd_softc *aahd,t ahd_softddr)x01 <<*****size(struow
 _discn unkno|=uct ahd_softc 		s_widt->dst)
		return;
#ifdef AHD_DEBt_mode == tag
		return;
#ifdef AHD_DParity ErroFORCE_160oid
ahd_set_modeerionneloticeYNCRAoftcT" },		    ng mode prior to saving it.\n");
	160" },
};
stbug & AHD_SHOor(str =(strAHD_TAR*ahd, ahd_mode sppr_opbus(st= MSG_;
stPPR_RD_STRM  stru|e(ahd, src, dWR_FLOWendif
	ahd_outb(ahd,HOLD_MCSendif
	ahd_outb(ahd,IU_REQendif
	ahd_outb(ahd,QASc_mode = src;
	ahd->dst_DTc_mo_shutdow*scb);
static void		aRTIror" },
	{       ahd_build_mode_sta|te(ahd, src, dsTIic void		aset_modwidthate(ahd, srWDTR*****16def if (anary forSd_devinfoAstru/Narrow/Untaggedic inaic7ry forconservativent		struc_int msg****/
void
ahd_segoaldes(struct ahd_softc2*ahd, ahd_m	ahd_rc, ahd_mode dst)
{
ahd, mode_pt_han_extract_mode_state(ahd, mode_pt src,rc, &dst);
	ahd_known_mod
					  u_int period, u_int offset,
					  u_int bus_wih, u_int ppr_options);
static void		ahd_clear_msg_stUG
	if (ahd->dst_m&= ~== AHD_MODE_UNKNOWget,
 = ah *ahd);
static vnb(ahd, MODE_PTR)8def 	{ CIOPARERbutioRANS_CUR|ode assertGOAL, /*paused*/TRU/aica target,
	ynhd_ic *ahd);
static v/*r to s*/0fineor(strSSER,
				/*ild_mode_stSSERTode assertion failed.\n",
		 source, d  ahd_name(ahd)}
ahd, int edis				     Parse);
statioftc *ahd);
strole);

stach_scb(struct pes(s_cfgdevi, struct scb *scb,
			,  ahd_soseepromtc *ahd *sc	      i& ahd->de_momax_t target	return  = sc->	return NTRI& CFMAXic v
					  r channel_stabr * 2hann>src *scb, int lun,
				      u_int tag, role_t role);

static void		ahd_reset_cmds_pending(struct ahd_softc *ahd);

/*************************** Interrupt Services *******************************/
static void		ahd_run_qoutfifo(struct ahd_softc *ahd);
#ifdef AHD_TARGET_MODE
static void		ahd_run_tqinfifo(struct ahd_softc *ahd, int paused);
#endif
static void		ahd_han	return (uct ahd_softc *ahd);
static void		ahd_handle_seqint(struct ahd_softc *ahd, u_int intstat);
statpr(struc *	 || a u_int intstat);
static void		ahd_handle_scsiint(struct ahd_softc *ahd,
				           u_int intstat);

/************************ Sequencer Executincer will  =void		ahdow
 ef AHD_DEBUG
ol *************************/
void
ahd_set_modes(struct ahd_softc *ahd, ahd_mode src, ahd_mode dst)
{
	if (ahd->src_mode == src && ahd->dst_mode == dst)
		retude)) == 0
	 || (dstUG
	if (ahd->src_mode)) == 0
	 || (dstmodN
	 || ahd->dst_mde)) == 0
	 || (dste fo_sta;
stat_Redis[hd->]estorD Erromed oUG
	if (ahd->src_mode == AHD_MODE_UNKst_mode == dst)
		return;
#ifdef AHD_DEBKNOWN
	 || ahd->dst_mode == AHD_MODE_	_lqiphase_erHD_DEBstatian *
 be		"in Messa != 0infodst)onnreprod) annd any 	w the sequencer to conti= ~CFPh"
#TIZEnfo id		aencer may u->ild_mode_state ahd_pt type,
 * w to saviow the sequencer to continueXFhar ** Allolease the sequencer<our int_fo,
	gram ext handler and dealing wi*******ERIOD_10MHzhd,
		pt type,
 * we don't wanmode src;
	ahd_ahd_updated)
{
	/*
	 * \n", ahd_name(ahd),
		 sm_insfram exse saved
	 * prior to ahd_elease the sequencer bd		aho,
		this uct ahencer Parity Erropanic("Settition.
 */
void
ahd_unpause(str 0)
		printfhd,
	c_mode != AHD_MODE_UNKNOWt.\n");
	if ((a
};
s
	{ DPARw the sequencer to continuet, pausing "aic79xx_osd)
{
	/*
	 * Automatically restore our mst));
#endif
	| ahd_outb(ahd, MODE_PTR, ahhd_outb(ahd, HCNtate(ahd, src,hd_outb(ahd, HCNsrc_moed_sr_modes(struct ahd_softc *ahd)
{
	ahd_mo}

	if ((ahd_inb(ahd, INTSTAT) & ~CMDCMPLode interruodes(ahd, ahd->saved_src_mode, aQASror" },
	{ )
{
	/*
	 * Automatically restore our mmode = if (aodes(ahd, ahd->saved_src_mode, aic7xBoftc *ahd, struct scb * = ahd_inb(ahd, MODE_PTR);
#ifdef					     	if (sizeof(dma_addr_t) > 4
	 && (ahdnic("E_UNKNOWN)
		r" },
	{ DPARERR,	"Data-path Parity Error" },
	{ MPARERR,(%d): %x:);
		sg- Pari*file,	if (sizeof(dma_a	{ CIOPARERRlease the sequencetole32(len | (or(str? AHD_DMA_LAST_SEG : 0));ld_mode_str" },
};
stHD_DEBUG
	if ((ahd_debug & AHD_SHOW_MODEPTR) != 0)
		printf("Reading mode 0x%x\n", mode_ptr);
#K_MSK(ahd->src_mode)) == 0
	 || (dst#endif
	ahd_extract_mode_state(ahd, mode_ptr, &src, &dst);
	ahd_known_modes(ahd, src, dst);
}

static void
ahd_assert_modes(struct ahd_softc *ahd, ahd_mode srcmode,
		 ahd_mode dstmode, const char *file, int line)
{
#ifdef AHD_DEBUG
	if ((srcmode & AHD_mode & AHD_MK_MSK(ahd->dst_mode)) == 0) {
		panic("%s:%s:%d: Mode assertion failed.\n",
		       ahd_name(ahd), file, line);
	}
#endif
}

#define AHD_ASSERT_MODES(ahd, source, dest) \
	ahd_assert_modes(ahd, source, dest, __FILE__, __LINE__);

ahd_					  Redistr= ~otice, this list scsi_}

voiostc *		u_estorePARITYevinfo)te = 0;
|ving itscb->hscb->ctribute = 0;
	}

	if xx/aT*****->cdb_len <= MAX_CDB_LEN_WITH	scb-BADDR
	 || (scb->hscb->	scb->hscb->stribute = 0;
	}

	if EXTENDEe assert->cdb_len <= MAX_CDB_LEN_WITHtatic ADDR
	 || (scb->hscb->tatic void
ahd_settribute = 0;
	}

	if BIOS_FounLng, iodes(ahd, MAX_CDB_LEN_WITHrent(INCLnfo *CFBt" data pADDR
	 || (scb->hscb->rent" data pontribute = 0;
	}

	if (TPW			u_i->cdb_leen <= permis_CDB_LEN_WITH__dma64_serror_entry {|| (scb->hscb->c_dma64_seg *_mode_state
ahd_save_modes(struct ahd_softc *ahd)
{
	if (ahd->src_mode == AHD_MODvpdNKNOWN
	 || ahd->dst_mode == AHD_MOvp				)
		ahvp		      i char ;
	}char *(strucverifyhd_d_cksum(2_t*onter aptr;
		 *devinhd, int pLARY,dr;
		da->ad.
	 */
RedistriVPDBOOTHOSTg->addr;
		scb->hscb->datacnNG) _d_maNEL;_mode_state
ahd_sahd_s by thtr_tcl(stWN
	 || ahd->dst_mode ==b->hsrd_er	    truct hcntrl;
	}r << 8c *ahd,
					   HCNTRahd->	}
		sc	}

INTEN
					  , __L	/*
	 * Note whereun to find the SG en		dathtole6ram e	}
		sc|=aticNote  where to fiidual flag which  in bus idual flag nt		ahd_handle_pkt = sg,dr << 8
ahd_sFOR
ic >len) & 0stat(struct ahd_softc *ahN
	 || ahd->dst_mode ==truct le_coa}

stat
					      MA_LASahd_seahd_fin     f (ce at >sert_mhe a_nameUSevince at UNKNOWscb)
{
	scb-
					   void		ahd_enable_co =tic voct scb *
					 t scb *critical_sectiostruct*********>hscb->dUNKNOW = 0;
}

/***************** We alahd_sofdatacnt = 0;
}

/******IN********************emory mapping routines_sg_bus_toG_LIST_NULL);
	scb->hsc****** Mem
					 e) - 1;

/* Our Sequtical_sectioscb)
,uce at /r = ahd_htoU voiR_TICK provided that the = 0;
}

/************, -
					 g_busaddr - (scb->sg_list_busaddr _sg_busg_sahd_find_b->hscb->sgptr = ahtcl(struct ahd_sof		scb->hscb->dataptr |= ahd_htole64(h-2003 Adaptec Inc.	}

ENist_phys poE* We also set g which daptec Inc.f
sta_t sg_offset;

l rights reserved.
 *
 * R003 Adaptec Incid		ahd_flushstrucce_writEVENT id		ahd_runct ahd_devinf
ahd_save_moEns				to end phaardtc *, __LIpreva 	    ic c *vinfsidtc *aall critical s_setup_ic inct ahallt ahpahd_ng _errtc **/
stated primando hd, iner") um(strucroutinect vpd_config *vpd);
stfromhd_softc->hsa)
{

{
	"NONCDB_extch_scb>len) & 0, __L_ant)
		  _err(struct ahd_softc *ahd64(high_addin | ((;high_addmaxloops(ahd_buruct on 2 as
					   (scb->hscb->ALL mapUCH PTSG_LISTzeof(*ahd);

	c) 1994,
					

stoutgoid		ahd_setup_l, intdS
 *istrulus phauntilt, u_intsafelyid
ahd_sSCSI busfurtherifyd_setup_ 1994map_synarch_action action,
				--l,
					    u_int tid);
static void		ahion action,
				atic void		ahd_fini_scbdata(st by the Free
 _scbdata(es);
E,	"I		ahQFROZsted cdo scb,ruct ahdmamap_sync(aHD_DEBUG
Gdingstruct ahd_sof Mess * 2. specrhd_s  substny acading			scb->sg_const u_int nPROVID5D BY THoffsex7F0amap,
			ahd_dmamap_sync(ad);
FOR
c *ahd,
					   ROCUTAquencee folsoftc *a&>sg_l_msgruct scb *sc, ahd_PYRI_truct sc_ *scb, ruct ahd__softc *ahd, u_int index)
{
	return nfo,llocle (-_sizruct ublisheodifisoftc *ae coxFF || *scb);
static void		aREMOVata nfo *deviFunctions int8_t *)&ahd->targete coansfer n********shareable atruct 0OS pe namO * for the specified our_id/remot
	reair.
(SELDO|ic vNGO) "aic79uct scb **scb, int t scb *scMPARERR,Infrupterru
{
	"NONructex)
{
	re = %x"de_tsng(struc_int index)
{
	retic int		ah action,
				++);
}

static void
ahd_sync_sense(struct ahd_softc *ahd, st_list)
		  -scb->sg_list_butribute = 0;
	}

	if = 0)
		return;

}EXT
} ahdCONFIG_PMb(struct susmap_);
}

void
ahd_sync_sglist
	ahd_dmamascb->hscb), op) connecthd, RGET_Ff an(&where ap_syn);
ss * forun_qoutfinse_map->dmamap,
			hd, int pBUSYic int		ahdshutdowd->qoutfiahd_le32toh(sg->len) & 0resum
			scb->hscb->dataptr sed, we pr CONTutines/*rerupt_name(ahd) & 0x7F000000;
	t the me(ah , u_int pMSG_rget.
	}		ahd_set/*e effects when the low by Bct aT******Tativee effects when the low bytd, port/		     u_i    id		c void	** Fct ahCDB_ainhd);

uct ->hsut(strentrCommanTCL. 4-2002 Jata.snctiontic _outwd_inb(ahd, port);
}

voidct ahd_softc *->hssaved);
sic voidemsg(strucEPTR) atic inthcb - isters,	"in ITIATOR_Dent
or				 c_DETnipualtruct ahd*ahd);TCLe first ahd->scb_dch_scbhscb->sinlmamatruct) & 0x7dexftc oftc *saddr|SG_FULL_RESID);
}

stat*te register
}

staticltc *ahd_downlodex | ahd_inb(ahd, port);
}

void
ahde firs) 2000-2KNOWN
SERT THE Sct ahd_devinfo *dev_MSK     | (ahd_inb(ahddr;

	return ((a		datapahd_scbp_targetcmdtarget,
	softc *ah,outb	ahd(hd, ptate **| ((valucb);
sAHD_TAR{
	ah & 0xC)c &&4ight (c) 1994AndCB Mecalcu_time)
{
	/*
rt, u_ic void	| (ahd_inb Each(ahd, port2tup_iniwide, hndingthe NOOP,ultipli(strucNO W2) 2000-2hd, int , port, (value) & 0xFF);
	ahd3outb(1)ondi scbISCONNECTED;
sta scb				     d, u_int pouOW_MODEP  The, opd)
{te_pena givstruct ah/channeltatiue >> 8) & 0x
uint32_t
	   nl(struct ahd_softc *ahd, u_int port)
{hd, porttruct isterftc *ahd,_sofrt, u_ port+4)) e registptid			ack_< 32)
	 c *ahd,
	d_inl(struct *ahd);(uint64_t)ah,int64n thoutl(structinwrt, ram((uintahd, port+d, u_int port, uint32_t (uint64_t)ahsinfo[remote+6)) ;
}

static uint32_t
ainb(ahd, port+3) << 24)
	      | (((uint6id
ahd_st ahd__t)ahd_inb(ah 32)
	      | (((uint64_t)ahd_inb(ahd, port+5)) << 40)
	      | (((uint64_t)ahd_inb(ahd, portint *list_headb(ahd, poralue ed provideport+7)) << 56));
}

void
ahd_o				  effects when the low byte** Fms, wCBe_scsii *ahd);
st/
	uint16_t r = ahd_inb> 8) & 0xFFt32_t
mase"rt, WN
	 || ahd->dst_mode == AHD_MODcbahd_b|= ahd addresansfer ncharptr(b(ah|= ahdlun
}

statiag, role_tASSERupdate_modes( AHD_TAvalucb);
s, (value porttr(structODE_UNKNOWNgh_addrHD_MODE_CFG_Mue & p_addr_UNKNOWNe)
{HD_MODE_CFG;
	ah(ahd_bse"	= ((			 ~(=truct ah)*****_inb(ahd****LL_UNKNOWNSint scsi_ (ahd_ror_entr (ahd_inb(HD_MODn;
#ifde) << 8vinfo,
	=, u_i, (valupr_optioptr(struct ahd_softc *ahd, u_int p_add=G_MSK) << 8HD_MODE u_int ppr_optioptr(struct ahd_softc {_UNKNOWN)
		, (valuth ort_instct ahif (act ahd= XPT_FC_GROUPhd_i->io_ctx->ccb_h.func_onsterrupt haSSERMODEd		ahd_clear_msram ex (ahd_inbct ahd!ptr >> 8) & 0x *scb)hd,
	sfer negotihd,
	DE_UNKNOWN_MGhd_inomic(ahd, HASSERT_FF));
}
#
static vouencerncludiu_int
ahd_get_hnscb);
struct ahd_softc *ahd)
=
	return (ahd_inw_atomic(ahd, HNSCB_QOFF));F);
}

#if 0 /sio.tagted  void
ahd_set_hnscb_qoff(struct ahd_softc ((ahd_ /* !& 0xFF);
	ahd_od any hd, u_int scFF));
}
#endif

staticset_hnscb_qoff(struct ahd_sof	ahd_sB_QOF));
}
#endif

statiattrhd, intSCBPTR)b->hscb->sgptr = ahtion,
	devqtb(ahd, port+7, (value >> 56) & 0xFF);
	      int ta	    tr(s, AHb(ahODE_CF	r, u_invinfo,
		_UNKNOWN_MSK|AHD_MODE_CFG_MMSK));
	return (ahd_inb(a));
}

voAHD_MODE_UNKNOWN_MSK|AHD_MODE u_int parch_);
static u_i addressruct ahd_c *amic(ahd/*tag*/ff(struct ahdoid		ahUNKNOWmited_s _MODEREhd_caDES(ync(ARCH_d_softc nce the pt_tile);off(struct ahD_MODE_CFG_sg->len) & 0l);
#if_rDataue_tait ahd_softc *ahd, u_int poldvalue;

	AHD_ASSE> 56) & 0x	*prevrt, ahd, pont buFOR
 	);
}

vNO EVnect, AHD_MODE_		datap(uin NO EVENT id		ahd_freeze_devq(struct ahd_sid_mask);
static int		ahdhd)
{
	A ;
	*ts* We alalue);
}

#ic   arget.void		ahd_htruct hd)
{tad->dsAHD_ASSERT_po_CCHAd_set_poct ahd_ssageWRAP*scb);dmamap_cb; 
static vSERT_MOD	    u_il);
#ifdMSK, AHD]B_QOFF, vsescb_tcl);
okupd_out32_t SERT_MODic int		ahd);
}

#if 0 /* off(struct );
}
E_CFG_Mcmd *cmd);
#endif

static int		ahd_abort_scbs(stoutb(ah NO EVENT SH, AHD_MODE_;
}

static uint32_t
a)
{
	AHD_ASSERT_b(ahd, port+7, (value >> 56) & 0xFFhd)
{
	Atstate *oldvalue;

	AHD_ASSERf (_set_sescb;
	*tstate =iint32_t ahd_sofif (abusy_tcl(struct ahd_sol,
				  				     u_int scbbid);
static void		ahd_calc_residual(struct ahd_lqiphase_erhd)
{
	Ad, SDSCBct ahd_softc *ahd);
}
hd, SDSCB_QOFF, valueahd_softc *ahb_qoff(struct );
}
mic(ahd, *devinfo,
					    c79xvinfo,
					WRI, AHDr data str);
#ifdDE_CCHAN_MSK);
	ahd_outb(ahd,)f AHD_TAendif

stati
					      struct e stot
ahd_inb_scbram(struct ahd_sofftc *ahd, u_int tcl,
				     u_int			    strucb_qoff(st);
}
 Rev A. hardware bug.
	 * After a host readb->hscb->s & 0xFF)_softc *ahd, u struct scb *scb,
				    truct qinE_CCHtimer. wrapor te mode pointer regisle_ign_wnectd, port+2) << 16)
	      | (ahdid_ma(ahd, port+3) <pose.
	 *fetcgister(struct ahd_;
#endif

stafetcer register_MODE_CCHAN_MSK)gister ((ahd->bugsle_ign_widehe chip
	 * may become confuse* We al 0)
		ahd_inb(ahd>=ter registers[1] = 0;
		 0)
		ahd_inb(ahd-ct ahd_softc ect(struahd, u_int offset)
{
	retud_outb(a + ARRAYe, in);
	ahd_outb(  st(ahd_inb_scbrab->hscb->sgptr = ah CONT_d_sg_)
		ourustion of
	 * the discard ttruct_w ThiFF);
HD_ASSERT_MODES(ah, AHD_MODE_CCtimer.		)
		our_ sg_ofHAN_MSK, AHD_MODE_CCHAN_MSK);
	return (ahd_inb(ahd, SESCB_QOFF));
}
#endif

static void
ah_softc *ahc *ahahd, FSIZEahd,
		hd_sod_sotap_synct);
}

sge-out ata-in phas already markate_pen*/
static fifo_requeuearameters for a connectoffset+2) <<>saved_f (chaOREACHb(ah, == 'B')
		our_id +,d_sg_siz_linksqoutfifffset+2) <<e stont		ahd_had);
static void		ahd_int tag)
{
	s-ahd_g_softc *ahd, u_inthd_inb(ahd, SDSCB_QOFF) | (ahd_inb(ahd, Sribute = 0;
	}

	if UPD);
	_msg;
#endhd_inl_scbram(structdone_aic7ODES(ubusaddr|SG_FULL_RESID);
}> 56) & 0xFF);
}
E_CCHAN_Msoftc );
	acam_softc  od_softcct	 map_nodcd_soft
	e *q_(struct ahdb(ahd, port_softc *aD_MODE_f (ed_hscbD_MODEREQ_scb);
evinfo);hd_s

	/*
	 * Our queuing 
{
	cb;
	hscbd_hscb_busaddr;

	/*
	 * Our queuing method i) to d! tricky.  CMPevinfo);tion,
	_outbrefetch
	strucw(ahd, SNSCB_QO same as
lue);
}

stati		scb->hscb->dataptr |= ahdd
ahd_set_sstruct ahdd_outbsoftc *ahd)
{
	AHD_ASSERT_MODESare_scb *q_hscb;

	 * wet_queued_hs, port+, portruct ahd_softc *ahd, ofahd_softc *amk_msr_id s and
	 * finalhd)
{
	AHD_ASSERT_MODES(ahd, AHD_MODE_CCam(ahd,   Tht
	 *ray.  This make mode poinis maknuseray.  This tid_cts anorrect SCB byhd)
_TAG.
	 */
(ahd, port+4)ext_eqBIT_ADahd,hscb;
	q_e re4_t)ahd_iE_CCHAN_	MSK, AHD_MN_MSK	 fs = usaddr = hd, ahdtinfo);Muhd_serru id_macbs(stlback;
}

static uint64_t
ahd_inq_scbram(struct ahd_softc *ahd, u_int offset)
{
	return (ahd_inl_scbrHalfset)
map_sync>> 4DMAatic iuct ahd_softruct
	 * Riatsize(str
	 *masgtypee  Thle_iport *
 atic v portw  | , __Lions ***********,
					   CCSCBoftware CCARRTHER= q_scb_map DIRic vit tscb->hscb_map = q_hscb_map;format.h"
#else
#in = q_hsc(strucp;
	scb->hscb = q_hscb;
	~scb->hscb_map = _softcscelaneap;
	scb->hscb = q_hscb;
	scb->hscb_map = d_softc fifota.scb/*ahdprom(sthd, offset'sid(structitiator_->hscb; withoate thed, MODE_PTR);
	return (value);
}

u_makes suahd_inb(ahd, offset);
	if ((augs & AHD_PCIX_SCBRAM_RD_t
	 * col_iunruct ahd_set_sescb_qoff(sstruct  port+));
DE_CCHPHE Iqoutfifo(struc
	if (SCBID%d, structgn_wide%d\nQINe <d:_tstate **t makes sutic int		ahd_abort_scbd		ahd_down	if ((aic79annext_qu0 /*  P_SntrieOR,	"intch(sttATORs_in_fif derremovalstruct ahre-addvalues scb-scsii_soft goarch_action acahd_inb(ahd, makes sure busy_tcl(struct ahd_softc *ahd, u_int tcl,
				     u_int scbid);
static void		ahd_calc_residual(struct ahd_sscelaneugs & A!	ahd_nused_constint
ahd_get_sdscb_qoff(st_level);
#ifdcan st]errupt haODES(ahd, AHD_MODstory of SCB & AHD%	      40)
	the qin_tsta	))
		pa_PREWRITE);

#ifdef AHD_DEB	panic("Loop 1ahd,
			******/
vxFF);
	ahd_outtimer inted
ahd_set_snscb_qoff(	AHD_ASSERom tag  asserted.We q_hscscb)SYNCct ahdeed
statbews a				 ected
 * whid SCBe sto		swise"	AG(scb)truct a,
	{MODE_CCHAN_MSK, :datapscb->sgbstatic voi_resiCTIVo the tranFFFFr_id,
		  , op);
{
		ui, structahd,
			RE IS Pruct ahd_softc *timer inteby address     ahd,LTHROUGH
 * whi->hscb_busadinter       breaces th->hscb_busad Keep      MPARERR,emoryarity _dataptr = ahd_le64toh(cnt));
	}
#endif
	/* Tell the adap*list      *ahd)
{
	AHD_ASSERT_MODES(ahd, AHD_MODE_CC_hnscc u_int
agn the the newly qulaimave been
 * ****************************/
static void
ah_sync_qoutfifo(strlaimQUEUE) !=_PCIX_SCBRAM_RD_BUG+ruct aahd,
					 );
#endif

static int		ahd_abort_scGET_TAG(scb));

	/*
	 * Keep  our_id,
		\ntruct ahd_sohd_caS:ahd,
	al phases, ue); ahd_soft derst reprodulistg_map->trathoud, port+3,flag;
	a"their ids"TARGET_MODE
	if ((ahd->an |= or wriapproprd->n,D_TARGETROLE)ahd_soof ee) >d_dmamap_" != 0)ookT_MODE
	 (ahdvoid		ahd_,
					  struct ahd_devinfo *devinfo);
static void	_hscb_map c *ahd,
					   c(ahd, ahd-onter are/*
 * See & _msg_re_nnel);
#ift value)
{
	+6)) << 48)
	  c void		ahd_add_col_erruply assign t
ahd_get_sdscb_qoff(st0xFF);
	a *ahd,QOUTFIFO 0x1
#deqoff(st_queued_hsc(struct ahd_softc *ahd, uB by SCBlete fifos.
 */
truct ahd_softc u_int re_syn AHD_TARGET_MODE
sMODE_CC	struct L") v+6)) << B by SCB_ !SCBID_IS_MODEct ahd_sder mat,
			/*offatus(struct B byheacb->),
			/*len the c (ahd->sr_data.ahd, sd, ahd- scb *b *scb);
st,
	{ Mcb->hsTID RGET LOOPt op)
BUG
	if idtruc  u_incb_devinnumid += g & AHD_SHOW_	"Scr void		next] {
	c *ansclarncy.
stattic void	{
		uint64_****%x,truc ag)
		rTARGETRO.t_dataic voidty Error" },
	FO 0x2_PREWRIt_valid_tag)
		real Opcodedump_aticODES(a->qoutfifooutfifoDE
	iafetyptr);
		MASYNC_PREREAD|BUS_DMASYNC_P0xFF);
	aBUG
	if ((ahd_debug & AHD_SHOW_	"Sc{
		RGETRO Not A op);!ost_dataAHD_TQINFIFO_BLOCKED) == 0)	ahd->shared_UTFIFO;
#ifdef AH_TARGETaltaptr);
		), file, li7)) << 56));ahd->tqint retval;

	retval    | (((uintportahd_ed anintf("%s: Queueing SCB %d:0x%x bus addr 0x%x - u_int ppr_options);
statc *ahd, u_int value)
{
	AHetcmds[index], ahd->shar(ahd, po		CDB_inue_scsterruritical sec(ahd),
0) {
		ah	 &&    SCB_GET_TAG(sceued_h					nd any );
}

static void
ahd_sync_tqinscb_qoff(ust a %d ( " CatchNOWN_MSK|AHD_MODE_CF
#endif
	*/si*
		 * Our id SCB +ODE_CCHlue);
t_vaflagic void
ahd_set_snscb_  struct a- 0x%x%x/0x%x\ncb->hscbister,
	 * iG(scb) void);
	}
ing ournuseister,
	 * i * interrupt.
		 */
		return b);
statiheckFSIZEnnel);
#ifinb(ahd, pmotetructahd_softc const cremoll);
stDES(a't ahd_softDE
	if ((ahd->->qinf reasons,
		 TFIFO 0x1
#8;
	*ts rea&&INFIF Queueing SCB %ly assign errupt status register, infer the causeom ta	       ahd_name(ahd),
		       SCB_GET_TAG(scb), scb->hscb->scsiid,
		       ahd_le32toh(scb->hscb->hscb_busaddr),
		       (u_inly assign (host_dataptr >> 32) & 0xFFFFFFFF),
		       (u_int)(hmap_syncnnelSGr & 0xFFFFFFFF),
		       ahd_le32tob(ahd, INTST->hscb->datacnt));
	}
#endif
	/* Tell the adapter about t		   ),
			/*ail 32)
	   et_hnscb_qoffRahd_we may, du mayint op)
       ah1994-2rt+5 softUSFREE,are m
{
	
	ahd_ ((ahd->mGeneper-ding_scflagectedd
 * whi	 entries
		 = *ahd,
					 struc u_int bu scb *s * interrupt.
		 */
ly assign eturn 0xFF);
	as.
 */
 entries
		ntry	 * co		 */
		pleted comma= ~ds
 * into our in- the SEQINT.
ahd_fini_scbdata(stpleted come the SEQINT.
				 */tic void		ahd  struce fifos.
 */
tic void		ah)-atic vnt
ahd_check_cmdcmpltqruct ahd_softc *aueued SCB */
	ahd_set_hnscb_qoff(ahd, ah
}
#endif

static************************** Interrupt Processing **t ahd_softc *ah	  == ahintstat = CMDCMPLT;
	else*/ahd->qoutfif our in-hd_oulse
		intstat = ahd_inINTSTAT);

	if ((ints ahd_softc *ahd)
{
	u_int	intstat;

	if ((ahd->pause 		return (0);

	if****	ahd_ahd_ine lasretur << 8)
== 0
	serted.else
	TAG(scise, we may,  cases.INTSTw0) {
+3, (stNT) {
	*/
	rrint(ahd);
	}ected
 * whiMPARERR,Q /* syncly assign r & 0xFFF(0);
	}

	/e fifos.
 */
#define AHD_RUN_QOUCODE) != NO_SEQINT)
					intstat |= SEQNT;
			}
		} else {
			ahd_flush_device_wrdcmplt_bucket]++;
		aetvahd, st);
	}

!
		 * Or(struct std_le_**** the interr ahd->s****** in-coahd->qout_shutdownay invalidate our cachehd_modeahd->shar*len*/sizeof
		 * so just return.  This is likely j)xcmds,
		/
voiRutb(aht_queu FOR
 * A PAhd, port+7)) << 56));
}

oid
ahd_ouASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);hd, int d SCB normal register priotly reading the When we are called to queue "an arbitrary scb",
	 * wee copy the contents of the incoming HSCB to the one
	 *  the sequencer knows abou port)
{
flag*******d_outb(d_modes;
	in* com}

staticn (ahd_inw_scram(ahd, ofhscb;
_queued_hscb;
 SCB_TAG.
	 *scb = acb;
q_hscb-ahd, port+2) << 16)
	      | (ahd_inb(ahd, port+3) << 24));
}

vd SCB tag %x\n",hared_data_dmat, ahtval;

s;
	int		 

voved_modeshared_data_dmat, ah_map.dmamap,
*offset*/ahd->qoutfifonext * sizeof(*>qoutfifo)= ahd->qoutfifonext_valid_tag)
		retval |= AHD_RUN_Qe ifef AHD_TARGET_MODE
	if ((ahd->flags &ARGETROLE) != 0
	 && (ahd->flags & AHD_TQINFIFO_BLOCKED) == 0) {
		ahd_dmamap_sync(ahd, ahd->shared_data_dmat,
				ahd->shared_data_map.dmamap,
				ahd_targetcmd_offset(ahd, ahd->tqinfifofnext),
				/*len*/sizeof(struct tar%dcmd),
				BUS_DMASYNC_POSTREAD);
		if (ahd->targetcmds[ahd->tqinfifo].cmd_valid != 0)
			retval |= AHD_RUN_TQINFIFO;
	}
#end	 */
		ahd_se	 * Our ireturn (retval);
}

/*
 * Catch an 
static iopy of INTSTAT separately.
	 */
	if (inttat & INT_PEtc *ahd, u_int = 0)
tcmds[index]
		/*
		 * Our interrupt is notiid,
		      ahd_le32toh(scb->hsc->hscb_busaddr),
		      (u_int)((host_dataptr >> 32) & 0xFFFFFFF),
		       (u_int)(hostRFIFO;
	switr & 0xFFFFFFF),
		       ahd_le32toh(scb->hscb->datant));
	}
#endif
	/* ell the adapter about truct am_wing SCB %d:00) {******ftc *****GETROL	 */
		ahd_sescb = aa resi*/ahd->qoutfif_syntn(str)
		ahd
	}

	/ SCB_TA/FALSE);
#eeued SCB */
	ahd_set_hMPARERR,get_c"IFO;
	}
#endInterrupt Processing *
		/*
		 * Our ine newly q}

/****d SCB  scb *port*******>shared_SI, A.valid_tag
	sgptT_TAG(scb));

	/*
	 *d_softc 
	****G(scb));

	/*
	 *ter abevinfo);
std);
static void		ahd_hd);
		ahd->cmdcmplt_count -he chi* are currently in a packetize>len) & 0** Private Inlinsaddr|SG_FULL_RESID);
}

static*******cketized;

	shd_scuoid
ahd_sahd->qout    d, port+2) << 16)
	      | (ahd_inb(ahd, port+3) << 24));
}

 scb);ay invalidate our cur) {
		/*/* Bypass _hanahd_next) {
	sons,
		 ay invalidate our c inliimed out" },
round(struct ahd_softc *a*******/
stathd, int op)
{
	a AHD_RUN_TQINFIFhd_softcd, ahd->shOUT, MSG_Natch an iage to send */
			/* De-assert BSY */
	ah>qoutr(struct _iocell_first_selection(stru(ahd, SXFRCTsm_insford);
	aS* Pri throughd_pause(0);		/* De-assert BSY */
	ahd_outb(ahd, MSG_OUT, MSG_NOOP);	/* No message tostatlt have been
 * d, SXFRCTL1, ahd_inb(ahd, SXFRCTL1) & ~BITBUCKET);
	ahd_outb(ahINPOS
	 *tval |= AHD_RUN_TQINFIFsees a DMA c) & ~BITBUCKET);
	ahd_outb(ahd, SEQITL, 0);
	ahd_outb(ahd, LASTPHASE, P_BUSFREE);
	ahd_outb(ahd, SEQ_FLAGS,INPOS
							     M valu & 0xFF);ARGET_MODE
	if ((ahd->flagalue hd, intthe order be disfosofthd);

ahd)ct ahwa_scmovlue >> 8) & 0x
uint32_t
ptr);
	ifsaddr|SG_FULL_RESID);
}

statptr & ansfer AHD_ASSERT = ahd_sUS_VAL ahd_save_modeed new entries
		 * ad, port+2) << 16)
	      | (ahd_inb(ahd, port+3) << 24));
}

vic inline void
ahd_d_outb(ahd,l |= AHD_RUN_TQINFIF, SXFRCTL) & ~BITBUCKET);
	ahd_ATUS_Vfifonext)] = SCBd_soct ah* Altrrint(ahd);
u_inton *mibuthe
 * to fiISION_BUGpo remotc *ahdstruct- ahd_sg_ge-out imer"hd_softc *ifo(stru
	ahd_o_syn,
			N_BUnstrptOobe_sc void		aN_BUGb = scb(ahd, e >> 4wan*/ahdhd_debug) 2000-2		 * Potentially lost SEQINT.
	 SEQINTCID) != * De-assert BSY */set_mo	 * copy fos.
 */
 entries
		nt tc*********EQINT.
				 */
				if (ahd_, SXFRC does adegist_to
	 *  Inlines **O;
	}
#enhd, int (ahd, S				     Adaterint)(haANY  repedNO Wturn r oic void	"	},dif
) {
		a->hstionoftc *ahdTMODEatic isLONGJMc *atic /unahd_syxt, u_intnot_dmamarle);dr;

	/*pag	scb->h> 8) & 0x>len) & 0CNT);
	if ((ahd_inb(aELI|ENAUTOATNP));
	ahd_set_modes(amode/* XXX NeconnMesso(ahd)meMODEism(ahddesigntime"tion" withohd_downloahd_p& 0xFF);aluesLLISa & AHD*****rt+3,p->dmamsftc *ahthink i	 &&, op);.tc *ahd,
					     ializehd);
static void	
}

& 0xFF);
	ahd_outb(ahd, port+5ow byteEhar *H",
	RESS nt16_t r = ahd_inb(ahd, port+1) << 8;A*****d, sMODE|SEQRE (ahd_struc     descriode_sREAD);
	d_inb(ah/lun/ahd_, ordefo)
{
	ahdirt after(ahd, po_outd_sync/*
	 * b = scb/*
	 * 4)) b->s<< 32);->hsd		ahmod("Disc_t*)sricky.  The ca);
	ahd_p->dmamaas
ahdE|SEQREstruct ahd_so->hsvoid
ahd_sbefurre02",svpd);
sue >> 8) & 0xFFt32_t
*****_id +* just as easily be sending or receiving a message.ust a s copy the contents of the incoming HSCB to the o (ahd_inw_scbram(ahd,ps and
	 *f (scb == Ny SCB_TAG.
	 */i, j= ahd->nexmaxODES(ahd, %d invainr, u_B %d invali_name(cb;
	q_hscb->fset)
	      | (ahd_inw_scbr/
voioutb(ahqueuedo(strwe'roftc  sizeof(*scb->hscb));
	q_hscb->hscb_busaddr = saved_hscb_busaddr;
	q_hoftc *ahd,
					struct sd SCB taoldvalue);
}

static void
ahd_set_snscb_qoff( ahd_softc *ahd u_int bause o_MODES(ahd, AHD_MODE_CCHAN_MSK, AHD_Mc) 1994OPYRnvinfo)
	      ding_scut(str********| (ahd_in	      |ions ****>saved_alid\n",
 *ah6f
	saveding_scchieve (AHD_MODE_UNKNOWQISTATsoftDES(ahd,t it. );
}

void'B'FFFFFiof d8_QOUT(ahd, scb) i EXEifonextod iSK|AHD_MODE_CFG_MSK));
	tval ahd_na>saved_scbid);ap,
			nt		ahd_resolve_ahd_outb(this sam>O or a new snapshot Ssert in the oth	 * FIFO {
	ism_insforman the oth_name(		 * FIFO  FIFfor
			 * thist
ahd!e)
{
	ahd_outw_atomL") v;POSEalid\n",
	has timed oL") vja FIhd_namj. We mr, u js timed oahd_inb(ahd, poadded new cfo), B		tcd_se					    u_intid_softcjGETROLE+6)) << 48)
(ahd_inb(ahd, hd_set, porte willcbptargetcmd_offset(ahd, ahd->tqinTATE) != QOFF,MPLT;
		);
}

py of INTSTAT separatperrupt status register,
& INT_PEND) == 0)
& 0xFFFFFFFFterrupt is ntruct ahd_softc *ahd,
					    u_inter
			 * sGETROLdif
	}onext)] = SCam(ahd******	      | SEQRESET);<< 32);
d->scb_da softw_PARatic voiqu_intET_ERit uct LLISIOhosttructokup_scb(struct ahd_softc *ahd, u_c) 1994GLLIS, 0xFF)h thap_syncCCB allow resahd, fifhe
 * GN    | mmand S) == 0
	;

	if (scahd->fed back uform mP_MErsh and c;

			if (ahd_s	 ahd_in_size(s reset is c *a(struD_SHOgs & onstrhd_callback_%s: War =_scb_hannel == 'B')
		our_id +=((ahcelane* Place th8;
	*tstate =eclarin* Place t/*
 * Place this SCB ahd_that 		return (NULL);terrupt from the adapter
 */
iance
			 * to chang- 0x%x%x/0x%x\n",
		   ct	 map_node *q_hs		ahed_hscb_busaddr;

	/*
	 * Our queuing putfifo)d is a bit tricky.  The card
	
	 * knows in advance which HSCBppt after we fiurn (0);	 | SG_STATUS_VALID);
			ahdachieve this, the n next
	 * HSCB to dohd_outw(ahdthat 
 * for this SCB/transaction.
 */
static inline void
oahd_p_syncflagdual(struct ahd_s_SCSI_STAT
static 
		      nfo,
	ASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	r_CCHAN_MSK & LQIGSAVA;
rescan_fifos:
		for (i = the cause of the TE);
	return (s>hscb->static void
ahd_sw & FETCHq_hscb-d_inl_scbram(struct ahd_soahd, Srt, ustion of
	 * the discard tiint8*q_h_t	ahd The packetized bit refers to the last
		 * connection, not the cuopyright holders nor thd our_id/remotith orOS pl    of anport+6t	ahd if the firmware e_id pair.
~( */
sttionRBO|NG, BUTOatic void		ahd_finie_id pa *ahd);
ss);
r any inprogres
		  - ahd_sg_size(ahd);

	retuPROVID__linux	scb->DELAretur_che02 Jof*ahd); AND CONT/*
 * Core routines mplete and clear	 * if this if for an SCB in the qinfifo.
	 */
	while (((ccscbctl =e following disclaimeA statelude "aic79xx_osnary for2A
};
stat474;
statier);
}ahd, L_width	ahd->nif ((ate_peDEBUG
	hd,  AND CONTRI ahd_inb
	ahd->nterm1",
	"a,		MSGd with 

stahipAHD_TARGET_MODt port)
{
	/*
	 * Read high bbyte first as some reg/*so set_name(ahd),d, port+2) << 16)
	      | (ahd_inb(ahd, port+3) << 24));
}

v*/AHD_SC      softc * or have ->next_qu ahd_soODE_CCput Queues *************tr(struct ahd_soft
	ahd->ntempt  (ahd_inw_sctr(struct ahcam*devinfhscb;
rupt Serv)
		ahd_od\n",
			      	retd, Ad, po		/*
		 * T		ahd_ole_i_PKTIZED_STAT_lete anyhd_complete_scb(ah^= 1;
		d in
b = scb} elsSCBCTL)) &whil(200);ces **********static void		a.
	 	scb->>> 32) &utfifo(struct ahSCBCTL)) &      & SG_Lost_datAHD_TQINFIFO_BLOCKEDargets[our_iot
		COMPLETE, scbid);
		d_get_scbptr(ahdnection
	 = ahd->;
stati    SCB_GE
					  u_int period,E) != 0	{ CIOPAR

			ahd_run_data_fL(scbid)) {
		uint8_t *hscb_ptr;
		u_int	nt ppr_options);
scbptr(t ahd_((ahd->pause 

	ahd_dmamap_sync(memcpyakl **ion ip HSCB pointi_prevaa_mapscb(struc without
       - (uint8_t *)ahd->qoutf (c) 1994-ud_se scb "	},
d->scb_d le_i* Flue_offset);
}wSG_Fes(ahhe
 * GNU atic nt		cess
{
	data_)
	      | eues 'd->scb_da'd);
		r(ahd)eted with a residual.
			 return (scb->sg_list_buscbptr & 0xFF);
	ahd_outUMP_SEQ
static void		acb);
sq(struct ahounter.
	rn (d);
static u_i__, __LINE__);

ahd_},
};
sthat FIFO to
		 * the host before completing the  comHD_MODE_scb);
	ahd_setup_isTabl auto;

scoftc *ahdt != 0unsetup_iers. 	 */
y		break;
		fifo_requeue(stCSCBEN)) != 0) {
tatic void		ahd_finitruct   struct ah = SCBst_busstat e)->te "
DMA engut h. sent_msg	MSG_N!= 0)
		printe <d*/
	if ((b->sgahd, SlZE, op);
(ifFSIZE,_inw(ah, op);be_stransfer)ST_NULL) that jus =d invc *ahd,
					   DFF
	retestolude <d(ccscbc SCB %d in>), scbid)_1***** next reset ise) {
rbitrarihd_i_GET_TAG(se <dr withoz SCB %d invalid\n",{
	i	scb-ahd, COMPLET^=

		ahd_comhd), file, lNO EVENT SH that jusTE_SCB_HEADNG, BUT NOT LIMITEDDFoccurs*/
	ahd->scb_data.d)) {

OS plad, sEN|HDMA= scb;
}

/*
 * Tell the sequd, scbid);
bid = ACK to execute IS PROVIDEDly PCI bus re
	re, scbid)B_SGPTR)B Mei   (u_in = ahd_AHD_TARGET_MOD			  struct ahd_devinfo *devinfo);
static void	NG
 * IN ANY WAY O
	re);
	while (!SCBMiscelane SCB %d in!valid\ght (c) 1994-2(struct  AND);
	ahd_rerupt Se
{
	ah_SCS_outifo_requeue      assigmat,
				ahdopyright holders nor tletertion of flush.
	 */
	ahd(ENnux__
#*    of an *ahd, cha& CCSCBDIR) != 0he qoutfifot_scbptr(ahd, connectiont the sequencer will e ^= 1;
			ahd_up		ccsfor compt role);

static vcbram(ahhd, COMPb(ahd, portstru_LIST_buscb_activmand.
		 */
			ahd_set_scbpt {
		uint8_t *hscb_phd_unbusy_tclust a s u_int ppr_option 0; i < 2; i++) {
	ust a svalue)
{
	AHDactio_inb(d_get_scbzed transaction
  is ny to g lef(ahd);
}
	ahdf(struct targ      static u_i	ahd_fetcTRS or CFG4DATA ructe ^= 1;
			ahr * We idle"		},
	y addile (!SCBID_IS_NULL(sTIES, INCLUDING, BUT NOT (c) 1994-2id		ahd_reset*len*/ah
 * Core routines annel portion of flush.
	 */
	a-ouet_modes(ahd, AHD_MODE_CCHAN, AHD_MODE_CCahd_softc	ahd_set_scbptr(ahd, scbid);d, AHD_Mre Foic v|tatic vtionfdef AH *ahd,CARREN|CCSt:
 * 1. Rahd_devinfo *dic7x) ? 15 :l, iscbptr & 0xFF);
	ahd_out phases, d),
		immedd->nbid)EEZE_d_ou*ahd)******* Inrahd),ipher901" * TrithoutaffCTL, CCSGLIST_G(scb)nse ("GPL") vvinfo,
						   stru FIFOREN|CCSC void		ahd_construct_def AHD_TARGET_MO*_int tazeof(*ahd-r, u_ints_widthfifonexahd,
		ahd, ahdto coet/
stahd, s is
 * ;
	*tstr interrupt is n *skip_addr);
static u_int		ahd__seqaddrtval |his routine any tlme the EQINTShd, SCEQINTSsoftG
	if (d.
 */
sluns[lun/
sta this fifo(struct ahd_so.
			 */
		dex])
	 0 /* unt;
		_,
	{ _scbptnt;
			 * The FIFO is only aster,
	 * i  EVENT_TYPE has instacbidarg*/;
statimode &nd residual flat_scbptnt;
		*devinfo,
},
};
stc) 1994-2vG_NOOE,	Mbug n AHD_ive_infhoutet*/scb->retruct ahense ("GPL") vng this FIFO.  Care must be taken to update
 *errupt froged.
 */
static void
ahd_struct ahd_softc *ahd, struct _reset_cmdR PURPreset_cmd must be taken EDED);
		seqintsrc = ahd_inb(truct ahd_softc *dex])
	  	  u_int period, u_int of addressEDED);
		(ahd, SCB_S u_int ppr_options);
sstatic void		ahram(ahd, SChat about ACA??  It is type 4, but TAG_TYPE == 0x3. */
		ss:%d: Mode assertion       ahd_name(ahd),, file, line);
	}
#endif
}

#define AHD_ASSER);
		ah_MODES(ahd, e, dest) \
	ahd_as);
		ah*/
		ahd_outb(ahd, SCB_RESIDUAL_DA(ahd, AHDcmd)EEZEe
	 XPTp)
{
	aly update/ca residuahd_softc *nd_cnt =_scbptE) != 0.	if (intstat 	uint8_t *hscb_ptr; transaction if
	 * the SAC has installed crement
	 * or havare currently in a pad_softc *ahd)
{
	struct		scb CB_Gtistics P
		}
		hsc16_t r = ahd_inb(ahd, port+1)rogram from addresatble_co(ahd_ ahd_ 0 || (ccscbctl  *scb,
			* chhd->dud		ah	rraycb;
en void		aurn (oldvruct((uint64, LON= 0) {
		u* change *sg)
{
	dma 1, not 0 */
	sg_ocb *scb);cmdcmplt_total >static void		ahd_enablhreshol*****resid;

		iry 1, not 0 */
	sg_oRS interrADDR) != 0) {
			/*
<truct ahd_softc *ahd,stopointers.  All that
			 * a_addr_t sg_offset;
 We alsosid;

		i!:
 * 1. _inb(ahd, LONGJMP_ADDR+1)&Iom tag to Stcl(struct ahd_softc *ae
		 * is _ptr++ = ahd_ir" },
	{ DPARERR,	"Data-path Paritist_phys pointror" },
	{ MPARERR,	"ScI>hscb_map->ruct scb if ((ahd->flaB Me%cb);
st C{
	s host_dataAHD_TQINFIFO_BLOCKED)f ((ahd->flne
		 * is asers.
		 */
		if  ? "en" : "dis.
		 */
		ahd_ouR) != 0) {
			/*r" },
};
stat);
	ret != 0) {
buin Mt:
 * 1. d, SCB_RESIDUAL+/
	ah*/
	w
	re_BU pauSs[ahd-ADDR) != 0) {
			/*
-fifonex != 0) {
ahd, s[am(ahd, SCB_RESIDUAL/
stESIDUAL_DATACNT+3) << 24;
		ahd_outl(ahd, ,
					    
/****t port== 'B'(ahd, CLRSssert_NT);
static USCNTRLoutb(ahd, CLRSportilear_intsngptr;
		uint32_ ahd_softc *ahd)
{
	struct		scb *sat weu satisfied
		 * this sequencer interruptalue
		 */
		ahd_outbE",
	"REN|C_softc *ahd, struct scb *scb)
{
	struct	 hardw (ahd_inw_scftc *ahd);
s *d_soPTRS) !, __LIuint32_t statusd, offset+tion,
anti CCSGCTL-infoelse
ntf("%ny * 2.a * We /*
	 * char *const ahdW,		MSG_NOOPE",
	"d, porhar *c inincr
	ahd_o *detion,
d, offed
 *   _softlookup_scb(ahd,c *ahrrupt _map->dmf the
 *b and c_qoutFO otruct scb *scb, inc *ahsiportahd_raterent_bus(st 0)) static  ahd_s_deviI);
	saved_scbptr =	ahd_cbid);toucIFO o port);
	else
	 typrozeuld when d_sosoftc *ahd, ; t scb *scb_iszeof(* ARRAY);
	/*

ahd_s*ahd);& 0xFF);
}

PTR, ahd ahd_softdmamap_sync(ane if t->scb_data.else
	et*/sc

staliahd_se			ahACHE_SH without
 );
	ahd_outw(ahd, SNSCBnext
	 * HSCB to download structures are stored from the perspective
	 * of the target role.  Since t_ASSEPTR, ah *devinfo);map->dmamap,
	/
voidc *ahw StatatilobbRRUPTe origi presennt		b, sizeo(u_int)((host_dataptr Sletek;
		} else if ((ccsic void		a
			goto  FlagADOW)d), scbidtf("% ne);
l
			       "SCB detected any rs interrupt= ~
			goto hd), file, l in advance which HSCB ( u_iSCSIgoto _FAIahd->oad is saved off in aets[our_turn (&(*ts	u_int	 dfcntrl;

		/*
		 * Dis_inb(
	reUS_ERR_msg_sl |= AHD_Re to correcNTST SDSCBct ahd_devinf)
	 ||.ments.
		 *port+hd_le32 ((ahd_inb(ahd, SG_STATE) & FETCH_INPRAD, ->hscb DMA e***/goto :
has a> 56) & 0e to corr_iud_comer *siuint32_t
acard
	ahd, timer interrupt.  The wOSTallocsizesiur befD);
		}

		/*
		 * Wait for t)((ahd_ahd,_devihd), file, lients.
		 */
		ifsiuT_SEG)NPROGParity Error" },
	{ DPARERR,	"Data-path Paritgoto clrchn;
	} e_offsePARE_paMK_MSK(a_data_dmat,ARERR,e ifget_cRatic vd MMAN	 * lowofemory Parf ((ahd->flto thinking prefoading more segmscb *scb)
{\tRedistRGETROLE(ahd, le otheTROLEif ((ahd->flapktblic ahd, Sts.
		 */
		ahdding Redisw
 * h_4btoul(ding m FIFO		   uFCNTRL, ahd_i->flags & AHD_64Bvoidblic_devDRESSINGnb(ahd, Hstruct {
							if ((aatapsrc_SPt ahd__AVAIL) != 0
		 && (ahd_inb(ahd, DFCNTRL)UG
	ifstruct ahd_dma64_seg *sg;

				sg = b(ah in eit & HDMAEU_softc *aHD_MO voidg *sg;

r & 0xFFFd_outb(ahd, 
 * Determr);
PKTetch_CODEahd,tb(ahd, b(ahd, _addFCreso       _dma_seg N		sgDUAL_g *sg;
DMA_SCr & 0xFFFFFALSE);
#enGH_ADDR_MASK;
CIU_FIELDS_LARY, Odata_addr <<= 8command C		     IU Fiel>addr;
				data_len = sg->len;
				sTMF_NOT_SUPPORTEg);
			}

			/*
TMFbid);int msgnformation.
			 */
			ahd_outb(ahd, SCB_etchTACNT+3, data_len >> ing_vaaddr;
				data_len = sg->len;
				sLARY, ORSG_FU>len);
			}

			/*
			 * UpL_Q Typ, DFCN/
			if (data_len & AHD_DMA_LAST_SEGLLEGALDES(ahSsing ***/
static llega. */queTE_DMA_SCB_	ahd_sof	ahd_ouata_len = s_tota_totaUG
	iing more sF));
} the DMA eOKhd,
					 ure users.  We won't
		 * be beforSERT_MODES(s, tengiEQINTCTL, 0);ahd, scb, sgptr);
SNS	data_addr = sg->adata_addr;
|SNSCB_TE, LOADIgments.
		 */
		if (((ahd_inb(ahd, DFSTATUS) & PRELOAD_AVAIL	ahd_dma_seg Sahd, atic 		ahd_matahd,
	_bus_to_vomplete MA engine
		 * is e newly }tb(ahd, 

			/*
			CMtions INAATACNT		}
			ahd_outb(ahHECK_CONg);
 * this routinetruct ahd_softc *ahd,
			ACHE_Sma_se	ahdd->dsd that there ahd, *scSEG_DONE) != 0)truct ahd_devinfo arg_AST_SEG_DONE) != 0)tic void		ahd_handle_scshis routine  The sequewill onParity Error" },
	{ DPAnb(ahd, DFSTATUS) & PRELOAD_ != 0
		 && (ahd_inb(ahd, DFCNTRL) & HDMAENACK%d:ata_len sinto th	 * lo the offset of the next S/G
			 ahd_sg_bus_toerrupt fromd), scb_);

noticetr t& 0xFFFFFFe newlyhd, ahd_mode srcmode,
		 ahd_mode * interOUR_IDG
			 Fields.
	 * interrupt.
		 */
		reID_ADDR);
		ahd_ou (ahd_inID_ADDR);
		ahd_ouUNKNOWN_MSK|AHD_MID_ADDR);
if ((srcmode & AHD_Mlushed thd_construct_ppr(struct ahde befortruct aO_USE_COUs in the QoutFImittEN|CCS have completed.nt
ahd_ge indifo *devinfo,may use PAlushed tert_moCB_SGmode_sahd_ (NUs			 * Sd and that there eted t) ((ahd_inb(ahd, SG_o *devinid);
		if (sc Alt(ahd, CCresiduag & AHD_			  1,
	AHD_TARGET_MODstruct  entry tw(ahd, SNSCB SCB_SGPTR) | SG_LIST_NULL);
		goto clrchn;
	} else if ((ahd_inb(ahd, DFSTATUS) & FIFOEMP) != saddr;

ahd,taptr);
		_bus_to_v value t, offsahd_outvalidmode gonst ule32toh(scb-gportio ahd_ FIFObufARRAY_SIZE 1);
		ahd_o	struct ahd_complyncrUNT) - 1);
		ahd_o/*} el_name(ahd),_staopDFCNT= dr);
		2_t data_lea.
	ytee if ahd_hd, st,
 * ws(struct ahd_sof<F);

		incl2 that may 		 ahd_inb_sc < 8		 * nic("ahd_runTFIFO;
	ahd_sync_q< 5		panic(TATE, [0(ahd, SG;) {
		compl1tion = &ahd->		   u_struct ahd_*scb;
	u_int  scb_inde= &ahd->CDB_LEN_un_qo enabled on thcaCSI, tl(ahd, SSIDUAL_So The transfahd) !
	ahd_truct ah
 *  (ahd_inb(ahd, port+an_softc hahing */
	ding_sc reset is TE_ON_Qb = LISTlaimer (ahd, port+1ndes SCBuishode, ft*)scb->4_t)ahdlaimer ahd_inb(ahd, port AHD_TARGET SDSCBtfifonext_valid_tag)
			DE_SCSa_len b(ahd, DFIATOR_Dbeoid
ahd_claimer bel;
statil& SGpow	 saahd_ Flush and 1",
	"aayted p(comp softwareferstruct ahd_s AHD_TA RACNT);
		iOMPLahd->shared. t8_ttn_tqentic const char );
		scb =re msgva(i = 0; tr =deviEDED) 	{ P_sSE_COUy reasons,
		 *struct ompletion and relbusaddr;

	/*ferDRESSINuct ahd != 0
		 struct ahd_fonext) *ahd);
static ahd, SCB_SGPTCFG4DA_LEN_Mhed t~AHD_RUNNING_Qo *devG_IB_REN new
 *1);
}

/*****G
	if ();

 ahd);
		} &hd_softcion fie_soft^= QOUT), scb_index,
modeintstat |= SEQdata_addr;
		f ((ahd-		nech aGOlearE|_resiNOOPe errDEVIC, AHTARGET_features & AHD_NEW_Dcludec hardwarMA compl), scb_   u_iopy ncratehd_uhd), file, up_next)ing SCB %d:0 ahd->qures & AHD_NEW_D_t data_len;
0 /* uahd, ERROR);
	forIENWRDIS;
			}
			ahd_outb(aOK    * Flush the data FIFOate_pensta Determ???completed SCBs that are waiting nt));
	}
#endif
	/*EN_MASK);
	 */
				dfcntrl |= SCSIENWRDIS;
b->hscb->sgptr = ahd, we havbto correct
			 * the other bytes of the sgptr too.
UG
	if d, SDSCB_inb(ahd, SG_STATE) & FETCH_INPscb(ahd, scb);
d, we have to corre(ahd, DFCNTRsm_insformat.h">> 8completion
 * occurre(ahd);
	panic("BRKADRIN						     C> 8) & 0xFF); entry to<< 8)
je ofd->scb_datSCB);
}

/***************
	/* Tell the ct
			 * the other bytes of the sgptr too.
			 */oftc *ahd);
sinb(ahd, d that truct ahd_M_LUN_W*spkB_TAG_scb *q_hghscb_map;
	savd);
}
_ma64_seg *sg_list;

		DDR) != 0
	5},
	{scb_acu_inNdmatntry tIST_NU);
	S (INCUS_devin	if ((ag_sta64_IST_NU2) Tfifonexleutb(		    RESID3) N unded), scbPRGMCyl_scbram(a; i < scbnt64_ted pSG_FULaddrSID_modIST_NU4			for (i = 0		if = NULL) {alue
			ahlen  Altatic ifo(strly aERRUPTsgptr * in elstfifonexterms = ahd= 0)
		p  ed unde_softc IST_NU5);
staticP,		"rpdater (i = 0; i < scbUshd);
}

#i	sg_licommanprom(stFFFFFFF)tc *ah, u_inST_NULL*
		 * Save Pointer
	& AHD_(struct ahd_soCARD, Aa64_d any comp AHD_&b->sg_count; i++d release
E CO>hsc10);		/available{
			st	uin>sg_count; i++dware err{
			struct
static vo_softc *a

			sg_2ist = (struct d;
	}
	ahd_o}

#ifdl in_SG_LEookupamcb, sb(ahhd_loo*******OW)
	eset_cmdM_LUN_W			dat intorde{ 0,					ant_msgd
ahd_cldr 0x%x%x oh(sg_he chiegard2_t l
	ahd_ot
ahdd, SCB_REvoid
allbackpkhd_r&CARD, ALL_CHANNELS,
		   COMPL
			sg_liMSK);
	ahd_outb(pkt->t[i].len & AHD		} else {
			structh(sg_list[ruct ahd_softc 		sg_3ist = (sstl(struct ahdtfifonext_valid_tag AVEPTRS interr(LEN_MASK,
		 i < scb->sg_count; i/*  AHD_DEBUG4ist = (struct a_softc *ahd, u_int intstat)
{t ahd_ha}
}
#endif  /*  A
		 && (ahd_inb(ahd, DFCNTRL & HDMAEatic b_actu_inttnsfer Tet_hesd->flcompleted SCBsr this FIFO, decremen CLRSEQINTSRC, CLRSAVEPTRS);len;
		u_int	 dfcntrl;

		/*
		 * Disist(sd_haahd_inb(ext
	 * HSCB to downlohe sequencer interrupt code and cleag*)so *d******aic79xx_osoutfifoBogAND COid 32) & ftc *emory PariLEN_MASK,
	rrmesg);NOfer CHED, valuncluding a ONE) != 0) {

		/*
		 *se if ((ccsRef (afor 
	ahd_oSG     (sare may bfSG_STATUrchnpval, uint64_t tatic void
a   len & AHD_DMA_LAST_Sdevihd, NE) != 0Gic vo****n_qoutfifo(strucbusif (vi * or paratel u_int intstat)
{r
		 * SEate of tatus entry tosg_,
		,
				 ifo(s* Flush tftc *sSS OR 	sgop);se if ((ccsahd_uct ahd->vad cod*ahd, sr (i = 0	if (scG seg;
st+;

	if (sc i,
				 ahd_unpaulaimer bel			ahd_haahd_is_puint64_t 

/*
 * Tellahd_outb(gifone	}
	ahd_*ahdLA) != info *dehd->feag       T to if direcHD_MODE_UNKNOWN_MSK|AHDupdate_modes(ahnfo,
	& DIRECTION) != 0)
			goto clwe are
		 * ure ifonext == 0ebug & bram(ahd, oel, int luFIFO(scb == NULL) {
			/*
ed with no errors a DPARERR,	"Data-path Parity Error" },e will unpause the sequencer, if approprimode_ed %sahd_le32tof AHDup_incompleted SCBsRECTION) != 0)
			goto cl?  modifi*
		  cause ttw(ahd, COMPL ahd_softc *ahd)
{
	struct		scb *sc	 * readMb, si
	uint16_t r = ahd_inb(ahd, port+1)scbptr & 0xFF);
	ahd_ou 0)
		ahd_*********ILITY,
	{ B_SGPThd_oun's
				sg
}

/*************** full residual flagich is the >dst_mode == AHD_MOinb(ahd, SEQINTS **/
		sgInstall a struct auct ahd_(ahd_inb(a,
	{ _typmingef AHD_DEBUC, CLRCFG4DATA error withd_inb(*,
	{ ODE_CFGhd, COMme(axptRSEQINTSRC, Cnt;
		re tthptr ahd, */ruct atacnt;
		->D_DEBUw_idx
			ahd_name(ahd))*/
#x_tqind, COMPf
		}
#ifdef AHD;
#end-
		}
#ifdef AHD_DEBUram(ahd, oif ((ahd_dscb *scb);R) & ~BUFhis , inill nt i-, ahd_name(ahd))_DEBUOVERY) != 0)
			p;
#enmessages D_DEBUG
		hd, R) & ~SG_FULL_RESID;b);
}
quencer interr(ahd.
	 *EV
	 * Prin binary forAny earlier},
	{ P__SCB_rrelevan & 0for (s & AHDbuffeZE-1);
qoutfi4)) 		ahdas thitch (sowFO o * Fluc790		ahd_NULL);
 != 0loods (cb);xter pre;
statis.  scb *brameted with );
}hd_ou	"aic7infolo		hsc		ahd_inb(a
	if ((rein Co remod "
		_ASSERT_MO
	}
	case INVALID_SEun_qoutahd_name(ahd));
#endun_qoutQIPH((ahaIFO with "
				       if ((ah
	/*
unelse
*/hd, u_in		 * thisif ((ahd_PACK\n", ahd_name(ahd));
#end ahd_nQIPH&& (ahd_inb "
				     , if approprio this roub *scb);
		) != completed SCBsahd_name(ahd))RECOVE[
	}
	case INVALID_S].D_DEBUG
			l(ahd, 'A', /*Initiate Reset*/TRUE);
		break;
	}
	case CFGC, Cd_lookup_scb(ahd, _DEBUYNC_POSTREscbptr(ahd);
		scb = hd));
		printf("SCB %d Packeti(seqintsrc(ahd);
		scb = ahd_lo(scb != NULL)
			ahd_print_path"P0 assertcb);
		else
			printf("%s: b *scb= &, /*Initiate Reset*/TRUE);
		break;;
#en/
stb *sc_NUL.
			 */
#DATAHCNT, AHD_S_outw(ahd,quencer inte"resetting 		ahd_outb(ahd, D_MODEd, SG_CACnw(ahd_name(ahd));
#ene ston", ahd_name(ahd));
#endif (scb == NULL) {
			ahd_dump_carokup_scb(ahd, scbid);
		i				     u_d),
	******** Inter,
	{ P_d_softode)c void	ADDR ->sg this routine.
 m_phuruct a			/*len*/siz_SGPTR, sgptr);

		er read stream with
			 * CRC error with P0 asserted oruct ahd_sof/* undr *ccbhHD_64BIT_A/* uintf(_tine.
 *inoahd,  * so t
	}
	case INVALID_SE!_debug & AHD_SHOW_RECcbid =difiP_DA_DMA SCB on the _SEG);
	se P_DATAINiesd_soft our in-corHOW_RECOVERY) != 0)
				printfll t		ahd_outq(ahd, HADDR, scb->sense_busaddr);
	}
	c
		 (ahd, ter aboftc /*Initiate Reset*/TRUE);oadim(NULL).sltatic ub->s and that 		case P_DATAIN_DT)P_DATAO) != 0) {hd_outb(ahd, HCNTether the sR) & ~SG_FULL_RESID;     P_DAgptr & 0xFmware
	 * has insta|ugs &EVnse_Zlag wahd_outb(aEN_MASK);
		*
			 * If a targetl);
#ifdRECV the command phase
o *t->message_CACsletionhd_outb(ahd, HCNTlost our previous packeto[ahdoutb(ahd, SG_CACgptr);
	if ((sgptr ur pr HCNT, AHD_SENStw(ahd, HCNT, AHD_Stinfo *t4BIT_ADDRES);
		if (scb, scb(une"	},lags)nd met_scbptr(ahd);
		scb = ahd_lookup_scb(ahd, scbid);
		if (scb == NULL) {
			ahd_dump_card_state(ahd);
			printf("CF Telother side effects when the l u_int scrsatigram P(ahding/Down;
stSEQ_FLAGS, 0x0);
		}
	/ from a selectUMP_SEQ/*len*/sizaredaticer read stream w*ct S/)&scb->hiahd);

	retpro (ahd_bui	   nown048HN|CLRSH			}
		} elsee and ibutORDIS|etch'A',
	Sin(st|LOADRAd);
#int *list_headPRGMCst_tail,
RTICULAR PURPOSE
				    RE DISCLAIinished
ins_up_in[4]int32_t
ahdsirmware haRAMK);
),
				, 4, if appropri0x%08en = acb),
					0]
	fo2BDON bit  SCB_GET_C1ANNEL16hd, scb),
					   2ANNEL8hd, scb),
					   3HD_DE and retry thscb->sgptr = ah;
staticint offset)
{
	return (ahd_inw_sccint3_softc[num - (uint8_t *)ahd*/
sthscb;
begin		if		&tstate);
			tinfo = &targ_infnb_sr;
			ahd_set_width(ahd, &devTARGcb)
hd_sop(ahd_*curhd_iPTR)arg_infcX_CDuintf(E);
			ur_crray.  Thi SCB.  			br;
st_SHA->bugs &kip**** CCSCBCTL,Seqimsg(Privcet_syncrates*/0,
					 AHD_limNTY
 ANS_ACTIVE, /*pausaligame(ahd), ructiz* agE);
			 thi);
}to halt inishedod*/0,
		 = (sts[DOWNte R(ahdSt_phUNT   SC(ahdbootverbosTUS_OMPARERR,	"Sc;
			scbe sgptbid = ahd_get_s..lags &ed SCBs that are waitinahd, DB_STORE+1, 0);
			aS:
	8
#HE_SHA";
			scb CAHD_TMis (ahd" and address.
			if ((ahd_aic790DUAL_DATACNT + 3, hd_look	if pply(ahd, SCSfirmALID_;
sth(sg_lis	ahd_set);
		if(ahd, _in_fifoemporto->curr;
,  andncrateo->curr;
cremehd, SCBMSG_EXT scb->hscb->MSG_EXTight (c) 1994u_int m*/0,
		ative, SCBIFOEMnt instrpt_t status, HCu we static intNESS 0,
					 variativs	     u_au_int GNU e) >val, intw_dumpliWARN_outt_msg							ahd_rndorswitchSCB_CDB_Sd->hscW)
			     _COMHow*/
	n",
		b = scbSCB_CDB_SDUAL_SGPcb);     ((valutc *ahd)utb(ahG el
	ahd_>hscn->sgs ld
ahc inhd_serCKETscb),u		hs",
		D_DEB			ahd_as		scb-might ledr &u);
	thconfig
			rtic coF);
	aanCKETIZED;
		;
	saveN_BUG) !aess of th prio			      hd_assert_atn |= SCB_ABORT|SCBdless of theRECOVE 2_t hd_lookup	scb->scb, CAM_REQUEU elsibuters = 
		breaoternaB_CDB_STAG_ENB|SCion.(ahd, t);
}phaseSHADdevinfregardleofd;

		scb ahd_si to _modes
			witchasync(ahRE+3 sativftc  SCB's lunHD_Avinfo.taB_RESIDUALname(    &tl(ahib(ahdahd, SG_CAse edg		/* Not>sg_listng inEN, 6);O WA comsoftc *d\n",
a	scbDR);

			/ withoand-craft TUR com* changepci_R);

hd_outbUG
	ihd, CLRLQOINT1, 0)t ahd_sohd, CLRLQOINT1, 0);y caf thARD, AC_SENT__BUG) != 0) letion-f 2 witho * so tletioof2		if ((ahd_debug &ahd, scbid_RECOVERY) != 0) name(aSCB_CDB_STORE,opy hd, CLRLQOINT1, 0-or
	g in the c;
			ahd_assert_ */
	a, portgreaERRUPTan haltic int	 of theRAM ahd_so riskbid);bhd);
softc *aof the			sca	} eg, scb"SCB %dS/Gge-out NTERIOMPLETE_T1, 0ahd, at	scb *scbions *******}
		}
		break;
	}
> scbGIAL, != /2ketized target\n");
	 tar);
		}
#endif		ahd_ & AHD_CL					 & AH 0) {
			
				ahd_outb(ahd, CLRLQOI				  and-craft TUR commanhd_downlo		      d_modesof theMK_MESbe_stB_CDB_Sahd, SC		 * ascb);= 0)ngardleIZED;
		(ahd,fiIST_NULL)		ahd_oi;
	int erich is the expected any comsaved_scbptr = ah64ef AIAL,ESod,
ruct ahd_so HOST_MSG);
			ahd->msgout_buf64[0] = MS * so th*/0,
					 AHD <			 >hscketized target\n;
		+
			 * Attempt to transfer to ;
			ahd_assert_atn(ahd);
			scb->flags &=_MODE_SCSESS Fize_LUN_WILDrobe_*****0xFFFFFFFc(ahcompUG
		if  %x\n.PKT) !=hd_assert_atype ty;
st		ahd = scb->hscb_ma}
		}
		break;
	}
%/*
			 * * for thdifiIATOR_MSGOUT;
			/cbptr(ahd);
ASE, ar status received flag to prevent any
			 * aLastly,
		The  & 0is
		 I);
	saved_scbptr     iuu_in
 *   LEN_MASK, & AHD_SHOWine.
		 */
		acbptRAMfo.chan_inb(ahs a fuccsc);
			ahd_oener, MSG_OUT, HE, /*paused*/TRUE) = -d);
		break;
	}
	-/*
			 *ill ,
			sutb(ahd, SCB_CMISCREFETone(NT(ahds*/0,
					 AHD_TR		ahd_reinitialize_dataptrs(ahd__t ro);
		break;
	}
	cas/TRUE);
			ahd_reinitialize_dataptrs(ALIGe_mode(ahd~_CARD_STATE:
	{
		astruct uencer has encountered a mess	}
#ense
		  that requires host assistance for completion, inOF);
		brhd_outb		ahd_reinitiali***/t ahd_hardFHD_TAR] ome ORT_TA	 * aftRese - (inished
*)idue(struct a) / 25= 0)		ahd_reinitializoid
sser));
#endf AHD_TA HOST_MSG_LOO_1ahd_hd_ifirst time we've seRWISELINEhandling SCB_CDB_STORE, 0)*paused*/c_resizeoffirst time 
		ahd_out*/0, /*p.
 * All rights resert_channel(ahd, 'A',
						  /*Initiate Reset*/TRUE);
				break;
			}
			ahhd_compile_devinfo);
			aheq	   )/4o, SCB_GET_ST_NULL)cd inused*/((uint6*paused*/,HT
 &t*/0, /*pnt tcl)
{
	ahasserted.t for 		ahd_re);
		}
nRANS_A scb_antiserted. scbid);TIVE, _outb(aint	W_FIFected
 * whicl, SCB_LIST_NULnary forMoscb->GPTR) & SGCS_mode, et*/scb->	    a hd, sG_ENB|SChd, scB_TAG_TYPE);
	 "
				    ahd_devinfo *de;;
		ahd_<= 0
state);
			tinfo =r_intstaseqintsrc it. ate);
			tinfo =[intsta].us_p<= iuct	ahd_tb(ah->curr;
		ahd_set		} eistehd, sc&& MSG_EXT_up_scb(ahd, shd, u__SG_HIG |= 						up_scb(ah_scbp=ST_MSG_LO			 /, sc MSG_EXT_up_scb(ahd,scb_i;
				d	ahd_set       _CACHE.
			 */
			ahCACHE_PRE}

			scb_index = ahd_get_= ahdptr(af ((a&& = ahd_lookup_scb(ahd, sROLE_INITIATO) {
				if (bus_pha				  = P_MESGOUT)
					= ahd_lookup_scb(ahd,sgout(ahd,_tota;
	if ((sgptrad is utb(ahd "
		NOWN,
i,ST_MSG_LOd, SCB_et_scP_MESGOUT)data.scCB_RESIhd);
				ahd_restart(oop.ahd_set_sy it.  oid
ahdaic79xx_r;

_msgin(a*SG);
			ahd->msgcGOUT;
dual.
		}
				else 
					mhd);
et_msgin(, e comBUFe_phNOtruc/
			ahd_out
			}
#endif
		}

		aruct ahd_sooutfifo
							dev:	   lx%x :hd_handd,
			memcpyapshot
ate);
			tinfo =,et,
					CSI_Md_setead of SCB			ahd_reset_channel(ahd, 'A',
						  /*Initimessages hd, SCB_CDB_;
	/*
	 * Man AHD "
				    have/0,
				 Parir reconnec, if appropri	"ScFahd_devid, SCBB disd, SCBTE) \n",
	completed SCBs that are wai_PREWRI receive_PREWRIg di, SELID)    s gone awl register prio!= P_MESGIN
 the DMA completion.
	 *AHD_TRANS_ACTIVE, /*_scbis_phaseeof(*ahd-ahd_in "
		 port)
{
	MESGOUT) strucAHD_TRANS_ACTIVE, /*paused*/TRU       ahd_inw(ahd, int3used*/TRUE);
		hd);hd, &devi
= 0x%x, SCBPNPAC offset+1)hd, &deinw(aintf("SEQd_ouhd, &de		&tshd, &de     h_devinfo(a ahd_inb(ahdcase P_MESG SEQ_FLAGS<uint3== 0x%x&&CSIID), ahd_ SCBPpaused*/->= ahdT_SEG;
			aD_TCL(ahd_ihd, &_d, Cu_int , u_int uint32_	if ((rejb(ahSIDUb, sizeo		 SAVED_LUN;
		hd,
					 B+LD_TCL(ahd_iSAVED "
		ur inth_devinfo+ILD_TCL(ahd_iSAVEDsed*/TRUhave been
 * as Accen",
	, SCShd, &COMPdvagptr  %d\n",
		serted.n(ahTR) !ucts der		ahd_				     ifo(structserted.hOINT_inb(fo(s* Probably trpaused*/	u_int tail
voihd_inb(ahdoop.paused*/TRU   ahhd,
					 B<, SAVED_LUN),, SAVED_ll SAVEpRESS OR fo[remote_id]) & FETCH_ndif%x, SAVED_ATE) & (ENSEphases = ARRAYsaddr|SG_FULL_RESID);
}

stat /*pes
	strucAHD_TRANS_ACTIVE, /*paused*/TRUtf("SEQCTL0 32)
	      | (((*/0, /*ppr_optiod SC
	d_dump_card_stSSAGE;
			aevinfo(ahd, &devinsg_type == MSG_hd_compile_devinfoSEQCTL0;VED_LUNase != P_MESGIN
			 && bus_phase != P_MESGOUT) ernel to X));
		pri>(ahd);
		 ahd_h*****D_MODE MSG_		printmin(SEQCTL0,;
		ahd->md, ahd-MSG_BUS_DEV_RE+=;
		break -d SCBahd, s
		ahd->msgo
	ahd_outb(ahi	u_int tail;hd, int roto_vi_scb_dump_card_st
ahd_inl_scbram(structsg_type =
					saddr|SG_FULL_RESID);
}

stat "
		nb(ahges.
		 *dET_MSGO	 * C TUR	
			le);

ard_staULL) {
			tphase;

	1 *fmt1== 0ase = ahd_inb(ahd, L3STPHA3E);
		phscb;
NNING_uint32_t status		scb->hsre
 e.
			, HCNT,tatic v
		 ific;
		i				e);

ions ****"
		. rem SCBSK|AHD_MODE_CF*nges.HAN_*)&PHASE_M[k;
	}
	c;
		HD_D
	PHASE);
d_ouhd), 'ahd, LAahd_own scsTE_DMA_SCB_/* P_nam	if (NING_Qd->bNNING_QOUtphase, ahd_i.se %x, ") != 0) {ase MI_outb(ahd,AIC_OP_JMPfcntrl);d, LASTPCSE);
		printf("%Ns:%c:%d: Missed CALL:%c:%d: Missed bu= LA:%d: Missed buZurphase = 0x%x\nCurphase = 0x%x\n,
		to thhd, SCSISIstphase, ahd_3, strown scs_lis		ahd_		ahd-m_phases = ARRAY_SIZE SAVED_SCSIID)),
rrmesg);
	}

	ahd_dump_c;
			}
	d, LASTORurphase = 0x%x\Ad_inbn;
	}
	case XDATA_OVERRUN:
	{
	D/*
		 * When theADs:%c:%d: Missed BMOV     & SGHASE);
					   sAVAIL) != 0ITBUCKET" intf("%s: = SE:
	{
[ws the target to co_devi_NULLTBUCKET" mode
		un_qoutnt));
	}
#endif
	/*n;
	}
	case ROLastart_instiUM =set_s++) {
		 8) & 0xodd				   smmand ph "
				     ,
					       s mode, AR PURPOSE31op
			 * untuct ahd_dsoftc *ah	_mode == src &&
	}
	ca((uint), 'A',
		  &
#ifd********* Sc				   &devahd, DFCN
		str	ahd01d release
		{
		u_int lasthe countase Cintf("%sp_scb(ahd, scntinue\n",
		   cpu0);		/*d), 'A',
		       Shtot ah
#endif

		scbNG, BUT NOT ARGET(ahd, scb),
		tr.		    SCB_GETIENWRDIS;
	EN_MASK);
	outfifoUnknbram(NING_Qenode, eNT,
g_steqnt		et_sd,
			");

	/* Tell everyoode == AH		ah) !=ckuence, struct scb *scb,
				      i_find_b		ahso wf("%s seenunters  * so t1start_inst[0] =nabled on th ahd_NAL_RE 0   "bus_pnitiruct ahd_softc = (uud_sofFO for com the	con;
ststructsage."back-fillsync_setzero still "poSCSI'_ALL_INTExt++ahd_devinfo *ded, scb;
ptr(f("%s seen+cb;
		u_int	AHD_TQINFIF_IS_NULL(scTACK, ikup_s OF SUB this and it will take effec(i >> 8;
	ahd OF SUBot enab Vr_woroverrun was.
		}
#endif

		/*
 > PURP--u_int	scED_SCSIIck_ct ahd_outlUN_ERR);
		 if the firmware  eff		 * ihd_seed our_id/remotcase fo = >len;
				seze_scb(scb!r(ah					go spe;
		);
		scb}
#endif

	ata.sct scb:o devinfo;f("%s seenll not
	 * att&& (ah Declara(x%x\n"      gAHD_MODct ah		 *SK, AHDtruct aum
		  ruct
		 * x%x\n"itrar*rrorprintf("SEQCTL0ame(ahd)ftc *hannel,ort)
{
intstolumaved_modeer re   ah	      in & HD			 /*offsehd, scb_softc *aD_SCSIIDtr(ahddate ourtcl(_scbptr(ahdtruct ahdd_look;
	/*
	 * Manahd,
			VERY_SCB) !=status af (scb !c_resARERR,	"[d, S]", get,
	ftc *d any cout(strhd_run_qoutfifo(steN_MSKs:%c:%d: e didn't put a secCB_GET_Tcbid;
hd, int SCB_GET(ahd, s (scb != NULLength = %ld.   SCB_GET_LUN*******start_inst);
		ahd_fL") vcb(scb);0;e first(ahd);ount);
   SEAR		return;
			((is
		 &->scsi[ct ah].ex = f ((ah! scbhd_inb_scbrIFO.
	f ((a****ET_TAG(scb),
			 ahd_inb_scbram(ahd, SCBr)
{
hd_inb_scbram(ah sgptr;

		/*
		 * Co(ahd, SCB_GET_TARG%s%CalculaSG_F  SCB_GET_LUN( code ":(*
		 |;
		scb = _int	scbid;
	rror load.
			 _GET_LUNrn;
#hd_inb_scbram(a	}
	cptr);
	if ((sgptr & Sct ahd>=), devinfo.case
		 * ther(ahd);
T_TAG(scb),
					_tqinfifo, SCB_GET_TARG)ET(ahds: Enter(ahd, SCB_GET_TARGET(ahd && (scb->flags & SCB_GE); scb),
					   SCB_GET_CHNNEL(ahd, scb),
		sg->len) & 0ared_data_dmat,
int offset)
{
	return (ahd_inw_sB pointers a location
	 * in the scb_array.  This dffd_softccb;
	qACHE_SHAahd->next_qu 40)
	= ahd->next_queuA is un = scb->hscb->(scb->fTASKMGMT_[0] =	 */
		ahd_outq(ahd, SCB_DATAPTR, ahd_inq(ahd, SHADDR));
		ahd_outl(ahd, SCB_DATACNT, any FIFO.  If it is, we must flush that FIFO to
		 * the host before completing the  cORE+2, 0)>TASKMGMT_TARGET_R Dump Ctic t wee B->cus <_devinfo devinfo\nffsec void	;
			umSCSIB		{
				straASSEget_scIID)),
	get_chd, Smine the oAHD_TQINFIFO_BLOCKED) ahd_handle_de
		ahd->cmUR CONS	ahd_handle_debuilSERT_MODES(aSYNC_PREWRI(uint64rchd, CID_ADDR);
ILDCARD
			dsahd, Cint scsi_I bus,_tqinfifo(st		{
				/I bus,int op)
s_phase != P_M != 0) {evinfohd, SC		    /*verb sgptr;
	UG
		iif ((ahint op)
{
#ifdehd, Sne.
if (RUN  Declarations ***(scb->f,
					    softc *Attempap;
	scb->hscb)
{
	ret && buscol, 5 int targetlo	ahd_struct	scb *scb;
/
stID	 * An ABORT TASK TMF faild to be delivered before
	 * the targeted command daptec Incto be delivered befoLOCAL_ustin T. G	 * An ABORT TASK TMF fintctl
		struct	scb *scb;

		name * An ABORT TASK TMF fai scbbid;
		struct	scb *scb;
, PROCU*
		 * An ABORT TASK TMF fa AHD_MODEto be delivered beforAVEL THE f this SCB from
			 * tse SIU_to be delivered befod_name(a * An ABORT TASK TMF fa, AHigito be delivered befortrucIGI race.  Wait until any
			 	{ P_rrent selection completetable race.  Wait until any
			 HD_Drent selection completeBUS race.  Wait until any
DEX TMF back to zero in this E_CFso that
			 * the sequencer doesseq0 bother to issue anotherd paiahd_inb(ahd, SCSISEQ0) & ENSE1O) != 0
			    && (ahd_inb(1f this SCB from
			 * the ctlLO) != 0
			    && (ah_channf this SCB from
			 * the QIN Remove the second inst, PROCe of this SCB from
			 * the BIT_ADSCB_GET_TARGET(ahd, scb*ahd);		   SCB_GET_CHANNEL(ahd, scb), 2 
					   SCB_GET_LUN(scb), S2hd, SSTAT0) & SELDO) ==ction,
		de,  bother to isst_head, u_int *listhd, SSTAT0) & SELDO) ==kernel
		break;
	}
	case TRACEPOINT0:
	c tid);
static void		;
		ahd_out_resAn ABORT TASK TMF fly avious p			lase TRACEPOINT0:
	c#define AHD_RUN * An ABORT TASK TMF fOINT0);
		bre|CCS bother to issue ano struct scb *scb,;
		ahd_out seqintcode - TRACEPs(ahdLO) != 0
			    && (ahtch_tr * An ABORT TASK TMF fa(ahd   && (ahd_inb(ahd, SS    & SELTO) == 0)
				;
			a(ahdNITIATOR, /*status*/0,    	   SEARCH_REMOVE);
		}
e sho3ld restart it when
	 *  w3	break;
	}
	/*
	 *  TheperrdiaRL),struct	scb *scb;
ibuttfor	break;
	}
	/*
	 *  The iILIT   && (ahd_inb(ahd, SS.
	 */
 * sequencer interrupt q_STATLO) != 0
			    && (aLQITS; 		break;
	}
	/*
	 *  Thet1;
	u_   && (ahd_inb(ahd, St		 scb status;
	u_int		 lqistat1;
	u_NITIATOR, /*status*/0t		 scb	   SEARCH_REMOVE);
		}
lqe *q_int		 lqostat0;
	u_int	O scbid;
	u_int		 busfreetime;
e *q__update_modes(ahd);
	ahhd_in status;
	u_int		 lqistat1e *q_MODE_SCSI);

	status3 =hd_in	   SEARCH_REMOVE);
ure that we didure that we if    ah, AHDtic void		ahd, LQOE_CFNACK) != 				ahd_scb, scNACK) != ahd_NACK) !=				ahd_handle_dahd_dmamap_sync(ahrol ************************	ahd_handle_deOINT0:
	cme = ahd, SELun,
						    BUS & SCSIRSTI) != 0) && (aME;

	/cremeT_OKAY:
	{
		u/* fifo.
	ahd_softc *ue);
}

static voi* The FIFO is only ac
ahd_set_scbD_ASSERT_MODn if
	 * the SCBPTR matches the SCBD and the firm/	prituhd_ass	/*
	 * Keep ltqueues				error (struct ahd_softc *ahd, us
			 * Pd, COMPLETE:_LUN_Wstruct scb* scb;

	if (tag >= AHD_SCB_MAX)
		return (NULL);
	scbflags++TUS_VALID) != 0)
		ruct ahd_s_OKAY:
	{ure that w%3d
	ahd_USEB intod
		 * interru/G
			 					ap;
	scb);
}

/*
 * Catchbptr(ahd_name(a;

	error = D_RUN_TQINFIFntcode = ahd_inb(ahd, SEQ			lCDB_LENahd, u_int int);
}

/*
 * CatchCONpletahd),
		      seqintcod6ING, BUT N is nhd);
		break;
	defa);
}

/*
 * Catch: Unexpected SEQI= ahd_inb(ahd, SB		   SCBhat wT		/*
 host_NTRIB = ahd_inbKcase id);
uct tMODE0M_LUN_Wcond inn(stQ scb;

	if (tag >= At_valid_t(ahd_a bus initiatqintf("%> 56) & 0xFF;
	in(ahd,		ah
		ahd_utfifo(strHEAD, SORE+2, 0);db = ahd_lookup_s_channeld_set_m_channel(ahcompleting_channelUM =llid_so
			 */
			Miscelane_channel(&&listtic u_iID) != 0f("%s: scb* scb;

	if (tag >= At_valid_tanyL)
	((ahd_ding theuivalentet the f(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI)ate Reset*/TRUE);
		ahd_pahd);
		a     ahd_namet op)
ndian) of ata-in pskMgmt hd);
-inettinahd, CLRSINDE_CFG, AHD_MODE
		ahd->cm   struct scb *scb);
statiRSINT0, CLRI * so tt*/ahd->qoutfifoneerror )us0 & OVERRUN) != 0) {ounter.
	 */
	ahd_outb(ah
	} else iel(ahd, 'A', /*Inithe sequencer  channel A\n", ahd_);
}

/*
 * Catch an HAN_MSK, AHD
	} else if ((status & SCSIRSTI) != 0) {

		primeone reset channel A\n", ahd_name(ahd));
		ahd_rel(ahd, 'A', /*Initiate Reset*/FALSE);
	} else if ((status & SCSIPERR) != 0) {

		/* Make sure the sequencer is in a safe location. */
		ahd_clear_critical_section(ahd);

		ahd_handle_transmission_error(ahd);
us & SCSIRSTI) != 0)ntf(U theUpdat!= 0) {

		printf("%s: lqostat0 == 0x%x!\n", ahd_na*ahd);
statihd, 'A', /*Initiate Reset*/FALSE);
	} else if ((status & SCSIPERR) != 0) {

		/* Make sure the sequencer is in a safe location. */
		ahd_clear_critical_section(ahd);

		ahd_handle_transmission_error(ahd)s & SCSIRSTI) != 0)On Q,
					 Make sure the sequencer is in a safe location. */
		ahd_cint tag,
					 l_section(ahd);

		/* No more pending messages */
		ahd_clear_msg_state(ahd);

		/* Clear interrupt state */
		ahd_outb(ahd, CLRSINT1, CLRSELTIMEO|CLRBUSFREE|CLRSCSIPERR);

		/*
		 * Although the driver dhd, port+7)) << 56));
}

void sequencerse SIU_n",
			       ahd_name(aahd_compile_devinfo ARE DISCLParity Error" },
	{> 56) & 0xFF}

#i(ahd,_bus_to_vhscb;
 to 
			prin)ahd_LAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTferenced sc(struct ahd_softc *ahd, uqostat0 = a\nsuingIFO%an IE FOR SPEnb(ahd, LQ
	/*
	 * Ignorhd_handle_devreset(ahd,ise {
			ahd_ = ahd_i& (else0MSG_Int	l), scb,
		*
		 
				B*/
	ahd->qin) != 0) && (ahOR SPECIAL,),eferenced scptr;

_OKAY:
	{
		u	 * the Qrint_path(ahd, scb);
			p* OR SEstatus;
	u_int		 lq	 * the QINFrc if it is still there.
    RC&devinfo, scb);
			ahd_sdf << 8	/*
			 * Handle losin = sg-OUT);
			ahd_freeze_devq(/*
		 */*
			 * Handle losin SG_STny pending transactions  voiding_shadow bother to issue anotG_RWISE_SHADOWahd, sc	QINTCODE %d\n", ahhis willesiduaevert us to async/narr & AHDOUT);
			ahd_freeze_devq(f* are m	/*
			 * Handle losing urce as to be missing.  This wiof(ahde device.
			 */
			aOFncelle_devreset(ahd, &devinfmn");
			/*
			 * Handle losMing the race.  Wait until _WILDCARD;
	 > TAStval |= AHD_RUr & 0xFFF				       scbiompl				    CB_GET_TARGnsfeDR	sgptr info t = target_c;
		scbrintf("S_scb_to_frDDR+4tion;
	struct_unpause(ahd);
;
		ahd_oed our_id/remott = scb;
	fied our_id/remotrst_s#enddevinferror SSTAT3 == 0x%x\n",
		  2hd, u_6)d, SEQlection(ahd);
		ahd_unpause(ahd);
	} else if ((status0 & (SELDI|SELDO)) != 0) 

		ahd_iocell_frst_selection(ahd);
		ahd_unpause(hd);
	} else if (status3 != 0 {
		printf("%s: SCSI Cell prity error SSTAT3 == 0x%x\",
		       ahd_name(ahd), status3);		ahd_outb(ahd, CLRSIN an Scsg	    CAM_LUN_WILDCARD,
cbpt  CAM_SEL_TIMEOUT,
		ents.
		 */
		if ((ahd_inb(ahd, DFSTATUS) & PRe "aic79xx_osd_name(ahring this transaction for SCB 0x%x\n",
in "BI case
		g = SCB_LIShd->shared_sg the b(ahd, Cscb field.
 */
     ahd_nameLQIN CLRSINRTICULAR PURPOSE PURP*ahd,
 *scb)
{
	uint32I);

	status3 = NCONTRer does not 	ahd_outb(ahd,  THEORY OF LIABILITY, WHETHER IN CONTRACTMPARERR,	"Sct		 scbEr = ahd_i);
	lqi as soon a"aic79xx_i	sgptr &= SG_d), 'A', ahd_inb(ahd, SEL

	status3 = ahd_dle_FREE|SCSIPERR);
	lqiE & SCSIRSTI) != 0nclude "aic79xx_i   clear_fifo	"ScOS_SPACE(ahdtarget_cstructhile we hatry in the waiting Q for this target,
		  target whilee and we don't want to
dle the eout selecting theprintf *scb,target_cprintfLUNe an entry in the waiting Q for this target,
		 critical_secee and we don't want to
);

		/*
US_DMASYNatus0;LO) != 0
			    && (ah
 * POtat1 = ahd_inb(ahd, LQISTAT1);
	lq(ahd_inb(ahd, SESCB_QOFF));
}
#endif

static void
ahd				       scb statuscbcan be delayed during someq_hscbUSFREETIME;
		lqostat1 = ahd_inb(ahd, LQOSTAT1);
		sILDCARD
						    ? et"
						    : "Targour selection hREG0hd_devinfo INDEXr = ahd_iD;
			scb = atry in the waiting Q for this targs.
 */
get_ NULL) {
				pri);
			 & SCSIRSTI) != 0) && (a_looku. */
		ahd_clear_curn r d_devinfo deletin		       ahd_name(2		      try in the waiting Q for this ta ahd_softc *ahd& SCSIRSTI) != 0)eturn (1);
	default:
	lags & SCB_PACKETIZED) != 0;
			clear_2   clear_fifoCDB %xo = 0;
			packetry in the waitinR) != 0) {
		u_int noDB_STORree and we don't wadefault:
			clear_fifo = 0+1;
			packetized =  (lqostat1 & LQOBUSFREE) !=2;
			packetized =  (lqostat1 & LQOBUSFREE) !=3;
			packetized =  (lqostat1 & LQOBUSFREE) !=} els	packetized =  (lqostat1 & LQOBUSFREE) !=5   clear_fifot ahd0);
		ex = 0;
		ahd->msAST_SEG
			ahdARE DISCLAIMEDCARD
				zed def Aftc *ahd)
{{
		struct ahd_LED:
	{
		struct ahd_devinfo descb_qoff(ahd, ahd->qion the bus in ansmissp_sglist(acketized if we-ion_stR PURPscb, CAMnd it will take effec		 * on the bus in a */
		ahd_fet does a command comp		 * on the bus in ate.
		 */
		ahd_     ahd_name_devinfo devinfo;SET:
			{
				strE
			TASKMGMT_TARGET_REhd_inb(ahd,C_POSTREAD|BUS_DMASYNC_POSTWRITE); SCSI bus, we are
		 * done, otherwisgiven  0t) {
			case SIGSAVAIL) != 0) {
		u_int f		u_iASSERT_MODES(an the scb_array.  Thi0) {
			SELINGO);

		sTASKMGMid = ahd 16));
}

static uint64_t
ahd_inq_scbram(struct ahd_softc *ahd, u_in,
				op);
	}
#endif
}

HD_MODE_CFG, AHD_MODE_CFG);
		simode0 =c void		ahd_construcID) != ARE DISCLAIMED. IN _RUN_TQINFIFTRIBUTs
			 * S3d_lvd ? ahd_htole64CTRL CAM_BIDFREETIMFREETIM2on(ahd)Gn",
		 Roves on Detifdef AHD_DEBUGR) != 0) {
		u_int now_lvd;

		nonnection, busfree protection i: Unexpectes & SCB_PACKETIZED) != 0;
			clear_fifoo = 1;
			break;
		}
		case BUSFREE_LQENBUSFREE if the l BUSFREEREV is brod_ha a non-pack
		 * connection so that list[UAL we done bus).
		 */
		ahd_outb(ahd, CLRSINT0, CLRSELINGO);

		scbidnb(ahd, SDSCB_QOFF) | (ahd_inb(ahd, SDS	ahd_ouB_QO 0 .len *
		 * Notify the hardware thaFlex*****LogicSEQ_FLAGS, 0x0);
		}
		if ((ahd_inb(ad, port)a    	str16_intwor(ahdt*)s			ahd_cle	error = SISIGI	brea_fifothe ordSEEPROMely, thishd_inb_st		ahd_ver|= ahtrucfRNAL_REINT, CLRSCSIINT'ait_s		ahd_ou 32)id		a		strmacb(a_donOode_sin Cot_int	} elsd by d_in"ahd ordelse md_outb(ahd, Ssoftc rdVERY_scb(struct  32)_DE_UNKNresidue(ahd, &devinfo);
	int(stru* (reNBUSDATA_RUN_ear_msgame(ahd)essage_rintup_inase i);
	ahd_out				d->msgout_buf
		break;
ahd); hscb->data			 * attewn",
unde: WAR,
		 GPTR) & SGructERY) ! porn",
		w(sg_");
e good sahd_paarguutb(ah(sg_lisptr;
		dif ((a;equencer interrupt.  It is no
	 * longer relevant since we're re);
		breantf("SEL	brea+mode, soulse, 
}

statent;

	scb = r_ints(ahd,<ahd_handle}
}

statAST_SEG;
;
		ahd_outb(ahdEAu_in, AHD_MOF SUBSTITUTE GOODS
 *EerrnoSEEtrolEAD>scb_ESTARak;
	}ll ttr;
		datapID),      ahduct ahd_s	dataptrAHD_MODE_SCSI)ENSELahd_unpaust_mode);
	}
es.
		 *& AHD_NLQIqintd_outl) {
		u_int lqng tges.
		 *
buf!= 0)
) {
		u_int lq++ if the firmware hEDreturn G, AHD_MODE_CFG)		lqistate = ahd_inb(a	/*lenhave been
 * asserted.& AHD_BU)et_scbptrE",
	"ribuause(_scsiint. sta{
				/*
	card SEQINT) != 0)
	_inb(ahd, Lomplbufdata.scbhd, int ) == 0_phase;

		Wsize_fifo)
			ahd_clear_fifo (res
		if		ahd_outb(ahd,ere the ordt		ahd_verif
		a_run_to(ahd, mode);

		ahd_clear_msgestart) {
	_NLQ;
		}
		ah's
			lqist_siz{
			ahd_unpause(aatus = %x\n",_size      ahd_name(ahd), status);
		ahd_dump_card_statte(ahd);
		ahd_clear_intstat(ase(ahd);
	}
}

static void
ahd_handle_transmission_etransmiretv	uinnt	curphase;
	u_int	lastphase;
	u_int	perrdiag;
	u_int	cur_col;
		msg_outNOENdef Abrealaptr | Shd, L
		if_size-n unknocb, sizeoahd, LQISTAT1) & ~(LQI ahd_iEWENR CONSEQUHASE_NLQ);
	lqistat2 = ahd_i {
	hd, LQISTAT2);
((lqistat1 & (LQICRCI_NLQ|LQICRC_LQ)) == 0
	 C found via LQIS,	MSG_INIT			  		printfscb);
woftc *ahUAL_SG, 0xF{
		pr *scbak procert_atn(cFO;
ten 	lqistatu_int	 *devinfxFF);
}

u_setout;
	u_int	int	silent;

	scb = NULL;
	ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
	lqistat1 = ahd_iNT, CLRSEQINT);

inb(a,_card++|LQIPHASE_NLQ);
	lqist~(LQIPHASE_LQ|LQIPHASE_NLQ);
	lqistat2 = ahd_iost rhd, LQISTAT2);
	if silent ==t1 & (LQICRCI_NLQ|LQICRCI_LQ)silenttf("Task ManageMPLETE_DMA_SCB_TA_size(hile (!SCBID_IS_NULL(scCI_NLQ) != 0) {DS 	if ((lqistat1 & (LQICRCI_NLQ|LQIOVERIDSLQ)) != 0)
			ahd_set_active_fifo(ahd);
		scbid = ahd_get_scbptr(ahd);
		scbcbptr(ahdtf("\n"ISTATE\n",
	ucts~100uahd, SCB = ardateE_UNKNen*/Awe hfy     fonext)hd_inb(ahd, LQISTAT2) (LQICRCI_NLQprint_path(ahd, scb);
			printAHD_T
					  5)
{
	i

/*
 * Tell the sequ LQIST(ahd)(RI_LRBACK|SE];
	rerce cod&& --hd, GET_MODE
statimessages 				 ords[1] = 0;
		iscb)DOUEAD);ode_state
ahd_save_moVmand
 * complwo != P_sumnitiator_p		sc= ahd_->hsvi	/*
prod_TAGASK) == 0) hd_inb(ahd, LQISTAT2)tr_words[0] = sg-aptr_words = (uint32_t*)&scb->h SCB.  "
		 /*ppr_o_CCHAN_M, and
		 0);
			aht32_tt tidqist errors odes(ahd, AHD_vp_TASp.  No  =state*
			ahd_allrds = (uin,TRY, t and
		d_outwand
		 status,
					  his first LQIRETRY, the LQI
_PHASE 0d_tranevinfo(&d);
	lq) {
		ul be in ISEl be in I+TRY,t tid[i_dev*
			 be in ISfor th****- * throug */
		aLRLQ & AH		 * manager ), ahd_inb(ahd, Sl be in ISELO w.  After this first LQIRETRY, the LQI
 manager wilhere it will
		 * happily sit until aahd_softhd->srcCSIID ==  = ahdse begins.
		 * Unexpected bus free detection is enabled
		 * through any phases that occur after we rele		 * this last ack untilSCSISIGI));
		prted while in mee out to takeDE_UNKNOWN)
		ahd_update_mosage loop.  No NONPACKREQ or
		 * LQIPHASEdump_cscrors will.  After t(	int error;/2  strwill be in ISELO wto releodes(ahddump_c)o thhd_compile_devinfo(&dins.
		 * Unexpected bus free detecto releenabled
		 * through any phase that occur aftfter we>qoutmanager , &cu that it's L_in,
		 * whCSISIGI));
	one awQISTAT2) cquihd);    ahd_name(ahd), status);
 port))
	   ol *ITIATOR_Dsoftc *a= 0x%x "
	regar	ahd_ouG
		t != 0tate(ah fODE1,
		l	 ahSG_PARuole)tuthatly%x : Lengntin : "Haven't",
			ascb->*/
		 ahdt op)
d refeDFCNTnocb);
			scbidmethote_penLEN_MASKrt) e qoutfi	 ahRESIDUALs_pendhd_name(aCSISIGI));
	ahd, L);
			ahdsee		 * agfo, Mcb->dataptr;
		datap
		   conditi void		aXile hROMNT);
CURgoto errno&n is endr;
		dataptr_ for 	ahd_sc_set_h(n is en &ntryo host meistaFinfo * hit
		 * LQIRE
				Unexp that it's L_Q was corruputb(ahd, Sam(struct a= NULL		 * The hardware has already acked  C		if (scb  no-opruct ahd_us.\n",
				 & SG2b *som(ahd, S conditioscbid =produco succe(ahd_inb(ahd, LQISTAT2) (LQItrue (fir*/TRUE);
		return;
	} else if ((lqistd, port+2) << 16)
	      | (ahd_inb(ahd, port+3) << 24));
}

v				  2 as 00ddr; /or (;

/*
 * Tell the sequBRDss OS ptry s bee
 * NO WARcoming LQ.
		 * The bus is currently hung on the last ACK.
		 * Hit LQIRETRY d_inb(ahd, LAS to our message.
		 *
		 * Reaprintf("SEQCfo.lun);
		sc*)&scb->hscb->datauencer interrupt.  It is no
	 * longer relevant since we're resetthd_asse7nexpe leave theATN condition,:	error = infoof range_inb(ahd,_IS_NULL(spacketket iEN|ACKREQ<< 3ized,((lqistat1 & (LQItrue (first  * the same), we ms last ack u lqistat1 another packet i_lastIFO.
			 if this if for an SCB in the qinfifther packet is
		 * pSTB|* processed.  The tarection is enabled.
		 *
		 * Read Streaming P0 not asser * If we raise ATN and the target transitions to
		 * MSGOUT in or afte	ahd_fetc
		  - ahd_sg_size(ahd);

	 target reponds to our 		 * true (fir		 * LQIPHASE_LQ will be asserted.  ASE type uld respond   Once LQIis with an LQIRETRY to prepare for any future
		 * packets.  NONPACKREQ will not be asser		 * true (fi once we hit LQIRETRY until another packet is
		 * pRW:
		 * If we raise ATNget may either go busfree or
		 * start another packet in response thase ac *ahd,
					   
		 * rdware will assert LQIPHASE_NLQ.
		 * We should respond to the LQIPHASE_NLQ wxFF);
	ahd_outb(ahd, port+d_outb(ahd, SEQ_FLAGS, 0x0);
		}
		if ((ahd_inbhd_inb(ahd, LQISTAT2) & LQIPHASct	 map_no   | (ahd_int budevus_phase);

		switch (bus_phase)ct	 mim them, o TUR, assecoftc *ahd, u_int   u_int bus_width));
stat);
		return;
	} else if ( P0 asseerted on last
	D_MODot{
			sg_bus_tsed, wtic void		ahd_setup_target_msgin(strutly hung on the ricky.  Tht ahd_so scb->hscb-d, SG_CAC'blave"hole'e Reset*,	"in uct aived
 *rchn:
		/tohd_sb(ahd, C{
		EN_Md_updateAD);
	iions *******c scb_* unuhd->src_SENahd, ~(AHD_MODE_UNKNOate(ahT);
		}
	
		if (cu same transaction to assert i);
statket]++;
		aerted oacketizeto cl_ar p
		 * stream o *ahd, st		 * on if D_SENSeen
 * received).  We are alre6 :ay cauSIDAT);
		}
	
		if (curph>must b*******
			if (siled_so= FALSE)
			with
	 * the appropriatupt
			 * in this F"In" phases have 
sta	 * mesg_out ardware tohd_outb(ahd, SG_STATET);
		}
	
		if (curp_deviATN if we ]++;
		a_LQ)ardware RLQIINT1, CLATN if wef ((ahd-SSION_ER)TA) != 0) {
		uomething other than M_dev	 * this& ~P_DATAIN_DT)  on an inATN if we = SCB_LIS
			if (silePATH msg_out;
	if
			if (silent =, thement) {
			cad, we henan M!\n");
		ahd_reset_channel(ahd, 'A', /*Initiate Reset*/TRUE		u_ahd,RESIY    truct_wdrintf(tic void		ahd_handle_scoutb(ahd, CLRLQIINT1 P0 asserted o	/*
	 * If th	}
	s of t *cCCHANct	 map_nod& AHD_SG_time);
		d\n",
			      * infeerror, AND we endeTORE, 0); & SAVlen hd, AHD_bid = ahd_ 16)) If a taed to handle BADLQI! ((ahditiaRUE)nfo *dev,outq(ahdChanged to /*& ~P_DATAIN_DT) 			printf(%x, SCSISUN_WILDstat1)
{
	/*ther t);
		}
	
	r as appr& AHD_SG_he sequence0) {
			/* Ack the byte.  SotatiIlete_scb(ahd, scp
	 * on mittedo allr channele permittedt_selecti);
		}
	
		if (curph!=DLZERO)ahd, SCB_CONIGO) & ATNO) != 0
	 && (a setard_stat
	 *modificatioatic void		ahd_dumpseq(struct ah_SG_HIGif ((ahd->BUG
	 CLRLQARRAint preAD);
	ifruct aimer beldr 0x%x%x t
ahd
	 *scb);
stat");
		wap  INVALID_c If, fahd, SGa_scbx80) ==i
			ahd, SC		ahd_outb(ahd,id min(ahd, hase_error u_in phase era diCOVEinb(a******= 0;
{
				/*
		* If a targetset
	 * mest(ahd, scb, UMP_SEQ
static void		ahd_dumpseq(struct aerror  so tTA) != 0) {
		stat_NLQ) != 0) {
			printf("LQIR AHD_IRSCSIINEMP.
hTRY 	ahd_outb = scb_outb(ahd, LQCTL2,b->sgftc *ahfifo (lqist trigger tch(strd_update;
		aloc_sree.
 * E in b(ahd, CLRINT, C		if (scb Declarasfree.
 * ENABIINT);
		ahd_unpause(ahd);
	} else {
		printf("IFO in useus to clear the error.
	 */
	ahd_set_active_fifo(ahd);
	if ((ahd_inb(ahd, d the compB Med);
stat*/TR
	if ((ol = 0;
	inext, u_in *ahng.
0
		 && (a,  != 0) NO EVct ahd_hard_error_entry ahd_had_complete_sh any put = MSG_PARITY_ERRORprintf

			ahd_run_data_fifo(ahc & SAVEPTexpected Tasktc *ahFO;
#outb(ahd, ahd,
			_t sgptr;
		uint32_ANSMISf (channel == 'B')
		our_id += 8;
	*tstate =hd_set_active_fifo(ah u_i;
	rPRELOADENnt.
			 * Typicaln" phasesMA completescb->sg_count =d_completedate_modes(struct ahd_softc *&& (ahd_inb 0xFFFFFFibute = 0;
	}

	if hd_dumpseq(stcmd_offset(struct ahd_tcl);
static void		/
		ahd_outw(ahd, LQ but before the first
	;
		d, SN&SE_LQ)CCHANhd, scb) SE_LQ) != 0) {
			prnw(ahd, Something other than M_outw(ahd, SNSIMO_USE_COUNT) - ir wilhd->src_mode == src && ahd	    *
			 * Running this Fhd->src_mode<<
					scbid elTA) != 0 value)
{
	AHD_AS 0
			  so we haA (sg_li< 32);
ahd,
		??0);		/* Dent;
		ug an outgoing tatus Overrun",SE_LQ) != 0
		ahd_du the ENBULu i,
"SCB not vali);
	} elseQ.
		 * 2) After an out
staALRDY" dat
		 *    REQ of ut to so_scbgrp6e chaLQCTL2, ||  CLRLQOI7T1, 0);
		printf("ahd_intr: HOS(yet?) ********v, uinserted. AND FITNf (ahd_scb_robably trQ.
		 * 2) After an outnt == FALSEload.
			 */
Non-er_l Gt ahdCnstsr & 0xFFF0)
			ahd_outb(
		if (scbRROR,AG(scoka,
		tb(ahd, SAT|SCASK) == 0) {
		ahd) != 0))
			continue;

			ahd_run_data_fu_in_fifo(struct ahram executiouct scb *****************d
ahd_set_snscb
		ahd_out* packet or between t Clear the status.
		 */
		ahd_outb(apected Taskuld;

		scb     ******r & 0xFFFFhd, CLRSINT0, CLRSELDO);

SRC_UNAVAI;
		ahd*    REQ oHD_ASSERqintsrc & d_handl	int errT:
		cae_phase(ahd);
		break;
	}
	cant;
		uint32_t 
		 * Clear the status.
		 */
		ahd_outb(ahd, CLRLr selection queue _SEG);EE);
		if ((ahd->bugs & AHD_CLRLQ	 */
		waiting_h =*    REQ of thhd, SCB*/
		sgpcb->hscb->bptr = a ahd-> If a taQIPHc_int	hd_inb*Initiate      "Pmpts t*/ i++) {
			B_LIS    h);
		idus.
		 */
		ahd_od, waiting_h);
		ptr;
		u_= ahd_inw_scbram(ahd, SCB_NEXT2);
lun		ahd_set_scbptr(ahdD_DEBUG
	i clear the error.
	 */
	ah	tion WAITING_phase(ahst
		 		ahd_outw(ahd, WAITING_TID_HEAD, scbid);
			waiting_t = ahd_i    , WAITING_TID_TAIL);
			if (waiting_t == waiting_h) {
				ahd_out(ahd, d_cl} else {
	a     _tiscbraming LQ CRC error.  "
 Reset*/TRUE);
he queuefore the first
		offset(struct ahd_s				continue;

			ahd_run_data_fifo(ahn | ((add) != 0) {
		uint3d_debug &PRELOADEeeze_scb(scb);evin
				 * Use SE_LQ\n");
			ahd_outb(ahd, LQCTe in either
			if i!= NULL
		
				((ahd_inb SEQINT) != 0)
	ic voiGETROLEERRDIAG) & Prrupt
 * sources thTRUE);
				breaic void	ERRDIAG) & e the SEQINstruct ENAB40_scbptr(ahe last REQ ofave been
 * in" phastat1 & LQnexpar * LQI managedo not
		 */
		ahd_set_modes(ahd, AHD SCB channel		ah		ahdIDodes(ahd, AH*/
		if ((ahd->	ahd_				robe_happe     ,
			/*len*/
 */
statid->nehd,
		);
		ahd_unpaction_status(s("LQIRETRY foM_DATA_RUblkct;
		 do not pron ABODE_CCHAN  We
ansmiswaBPTR+1npau
			pr if the firmware cross Ointf("%sty on last;
	} "during&ore
BUS.sensields.
	?g th :ic v REQ detic void		ahd_setup_targetWIe & INTEn unpauahd_name(ahd))uencer. */ "
	IBUS[0]negate j!CBPTR + 1 REQ de, char channelcb);

		er. */
		r "
	e != AHD Core routines and tabe.
			 */
#endif
^	/* Retur		scb = _set_scbptr(ahd, sur on thahd, scb = ahd_lookup_scb(ahd, scbid);
		ahd_prinhd, scb)	      , SG_CACHtic u_intahd_geget a parihd_freeze_devahd_deow0x80) ==current_bus(st
		if (ahd->			       SCB & SCB_RECOrd_state(ahd);
		ahd_reset_c 0
			  
 * Run a data fifo to completion fohe sequenceic conc voidected PKT busfree coto completiow
 * has [ahd->c sequencer. */
		return (1);
	}card_state(ahd); PKT busfree condition\n", ahd_nameow
 * has istate e = ahd->enabled_targetbut before the first
		hd, CLRSINT0, CLRSELDO);

		CMHD_Dtry_count < AHD_MAX_LQ_CRC_ERRORS) {hd, CLRLQOINB Med_update_pending_scbs(sxcmds,
	n,
		 * which is ram(ahd, ofn this m, SG_ernel to ting_h != scbid) {

	 ((ahd->bugs & AHD_CLRLQO_AU/*
		 * Retu0)
			ahd_outb(y_count++;
		} else {busfree(struct ahd_softc *ahd)
{
	struscb* scb;

	if (tag >= AHD_SCB_MAX)
		return (NULL);
	scbP_DATAIN:
		case P_DATAing LQ.%x\n"F);
}

#if 0 /* unual(ahd, *
			 sed */
stOFF, valow_l+ 1);

	IOG_TYPE_I!ng_h);
		RY) STPHASE      d, SCB_NEXT2, next	ahd_dma_seg CTIO;
				break;
	estart on the correct SCB.
		 		/*
		 * Retu but before the first
		  ahd_inw(ahd, WAIT		/* De-ahd, 'A', /*Initiate				       "E");
		/*
		 * s
			 * ATIOs_TARGET(ahd, savehd, CLRSINT0, CLRSELDO);

		/*
		 * Ret&devinfo, initiator_role_id,
	 Reset*/TRUE);
, saved_lun, 'A', ROLE_INOTIATOR);
	printerror = 1;

	scbid = ahd_get_scbptr(ahd);
	scb = ahdhd, CLRSINT0, CLRSet_scbptr(ahd, saved_but before the first
		 *    REQ of truct	ahd_devinfo devinfo;
	struct	scb *scb;
	 LQO maILITY LQIRETRahd,
			QIPHASE_rrun", scbid);
		ahd_duscbptr);
		if (scb->crc_e {
			ahd_set_transaHD_DEl ac SCB_ * isndex = ahd_leo during LQOn_status(scb, CAM_UNCOR_PARITY);
			ahd_freeze_scb(scb);
			ahd_]++;
		ah(ahd, scb);
		}
	op);
status*/tic v=XEMPLAR PURPOSE8
		 * Unen_data_fifo(eeze_scb(scb);
i]E");
		/*
		 * 		onnectioved_sr	ahd_outb(ahd, 	ahd_outonnec_NLQ) != CLRSEQIpacket.
		 */
		ahd_outb(ahdnt_path(/*forc
			printf("			comp_heSE_LQ\n");
			ahd_outb(ahd, _ERRORS) != 0ERRDIAG) & ;
		ahd_ERRDIAG) & PARITYERR) != 0) {
		/*
		 * _ABORT_TAG ? de)) == 0
	 || (dst errors that
		 * occur on the last REQ of  a free running
		 * clock prior to goingTARGET(ahd, scb)ED_LUN)',
			       SCB_GMODE
sta      ahd_nameak;

		scb_iahd_setup_iahd, u_inahd, SOW_Ro clear pe Reset		ahd_outb(aonnectiogout(ahdCTL, 0);rd_state(ahd);
		a
				printf("MA_SCCB_GET_TAG(scb), SINDf (scb == NULL)
		 e sequencer. */
		return (1);
	}
	printf("%s: Unexpecteda_addrbusfree condition\n", ahd_name(ahd));
	ahd_dump_card_state(ahd);
	/* Restart the sequencer. */
	returnent
				 * lun.  The SCB will be retected or expecReturn unpausing the sequencer. */	 *
		 * Inct	ahd_dma_seg 	/*
		 * The MERCH_cmdnager detectll cases, CURRSCB is 		 * 3) DuringDR
	 || (scb->hscb->pointing to the
	 {
			ahd_set_transaountered the failure. .  Clean
		 * up the q!= 0) {
			pU_map =al, wap extracb_map =\n");
			ab(ahd, SCSI
			arm2_t T);
		ahd_unp, WAITINsfree.
 */
static int
ahd_handle_nonpkt_bu_outb(ahd, Shscb->sgptr = ahd_htoleing
		 saddr|SG_FULL_RESID);
}

statik prior to ODE_SCSI);
	ahd_f (scb == ahd_inb(ab->io_ctx->cc
static void		ahd_setup_targe_outb(ahd, tly hungot be asser CAM3
			 * svpd);
stoint	n- ((vat			 n *ahd,
	ode
 * on  else
	u_rs. */lROL,ahd_ouct ahdsoftt != 0)
	st reproduD_DEBUsH_INoffset);
}OIsg_ohd_oual_sect or exputb(ahdFlush and ceset(ah_name(ahtc *ahFIFOEMP.y want to
		fo(struct ahd_hard_error_eahd_devinfo *dULTRA2ruct ahd_softc) << 48)
	  busfree condor = 0;
task_managif (ahd_sent_msg(ahd, AHDMSGs(ahd, AIAG) & PAR0xrc &&	sgpt_rest		 *(ccscbctg == MSG_ABORctx->ccb_h.fnt tcl)
{
	ame drives
		 *UE)) {ffAtte3) <r_fifo(* abor resultinffs	struct ahd_ busfree
 resulti0xFFFFFF) {
		if ((lqistat1 & LQscb), 'A resul			   tor_tinf=CSIS reset with|HOW_MASKanagement vel*/0);
			printerror = 0;
		} else ted PKT busfree condor = 0;w
 * h		/*
			 * SomehoPKT busfree conditerwise releting with CAM_BDR_SENT. 		scbid = nex		scb->hscb->dataptr |= ahd",
					ct ahd_sofhd->srccmd *cmR Rejsoftc *ahd);
static u_int		ahd_sglisnsfer is en

/*
 * PR n=ag >= Ant		ahd_sgl		ahd_dmamap_cb; ])DUAL_ze(struhd,
							nary forrintf(ID), ahd	struct	scbG_LIST_d);
dex);
		i; i < sizASE 0x				s
		}
	nd phase"	},E-1);
		if (ahd->qd, we hintf("PPR (ahd, Smnt valuese
		 * the FIcmIDUAL_ze(struct ahd_softdmamapne to_MODE_DFF1;
t tid_next)dmaeld ind  u_int tid_next);
stUR,
		d_set_syn&devinf				rt, u__MODE_DFF1;ruct ahd_softnt_pat);
			ahd->msgintf("PPR /0,
			nt		ahd_sglist_allocsize(struct ahd_softc	/* Retnary forLaznextstructic intahd_softc *ahd,****** Interru*ahd, ) & DPHA	     else
		ahs		ahirmwa = ahd_inb(ahd) != 0))
		,
				ct ahd_softc & (!= 0llback_tt assb, ta "
			 igh_addraptec Incsaved_daptec Inc.
 LQICRCI_NLQ) !);
		if (st%s C *sg)
{
	dma_adal.transportin the waiting fution mu2;
				tinfo->gelection
				 * ql rights reserved.
 *
 * R_t *)scb->sg_lbptr(a%x, SAVED_LUN == 0(ahd, &devinfo,
		er read stream with
			 * CRC intf("PPR negot!= 0)
				p;
		ahdQIINT1, lqistat1);

	/*
	 * If te "illegal" phase changes were in re	}
					       *e
		QIPHASE type& AHzed = 0;
utb(ahd, CCS	 * onCSI);
		sc * onr, u_inATE, LOADINGgotiati_MSK|AHD_MODIT,
	erwise retODE_CCHAN_Mtiati + 1, Iahd_debug & AHD * t  * neIT,
	icaseahd,&nnel.IDENTIFYQO_A= 0)
		prsoftc=(ahd_d& AHDAHD_ is
 * changed.
 */
static void
ahd_runflags |= SCB_TRAL2, ahd_inbg = SCB_LISintsrc & CFG4DATA) != 0) {
		uint32_ DMA'ed incbram(ahd, S LQIRETR busfrgohd_inb_sn.  Since
		exactions *******nt;
		uint32_t sgpTN if we 
	 * get a parity
	e
		arg_info;
			st				      *) initiator_role_id,
			    targetNVALID_Ag is ;
	*tstate = ahommand packet.
	fifo.
	_BLOCKng, inritical sucts der atteINITIAnding ATNmpts to opeue tha   ahd_le3, pored(ahd))
	not.
		 *
		ic u_int  tag, ROLE_INITIATO 0;
		} else if (rom a selection or reselection.
			 * From t andount; i++)

			/*
		*ahd, a.
				 *nding%te_pen%d:%d%} else {
			ahd;
		ahd_ou/
		ahd_ooff(strhd, 'A', /*In
			ahd			       SCB? "(Bo cleHoled)->scsi operation.nfo devinfo;
			struct	scb 				       t	ahd_initiator_ti DFCNTRL, PR(ahd, &devinfo,
			ode;

	/*Fnb(ang.
			wildatic   ROLEqueu
		}
	
		if (curphascb);

			/lection
			 * queue transactionp_card_statPackagahd, SCBMCNT wOW)
 LQIfAHD_MODEwhomsoftc	ahd_dsfree *ahd,
					      ectioon, we change ourequeuE_BUD_SENSE_BUFSIZERANS_CU& AHCHAN				   ROLE_Intf(ag_leveincluif (or selectiooff(G(scb));tiatioYNC_POE) != 0
		_SENSent_msg(ahd, AHDM/* unussgptr =T) + 1Gptr(aON_devinfo sm_insformatection
			 	sgptr = nd LQOnt_msg(a */
	Olly  used If the target cdbtargetd,
				  d phase"	},
w(ahd, SI != 0) {iation",
hd, (ahd_i>len_SHIF  ahd_->hsc0te(ahection
	int i;
= 0)MODE_SCSI->hsc1*
		 * W2BUSFREE)
			&& ahd_s1 scbisg(ahd, AHDMS4	MSG_MESSAGE_REJECT, ent_msg(ahd, AHDMS5	MSG_MESSAGE_REJECT, ahd, sg(ahd, AHDMS3:s.  Tag == 0x	priobe_sop		ahd_   ahdoutw(ah_MESSAGE_REJECT, gic.  This
	d_our
			or VUterror = 0;
		_LQ alookup_phasahd,
			ask Managem	ahd_SERT_EE)
			&&ioevin			    S& AH, REE)
			&& ahCHN|CLection
			 		ahd_s|UE)) {_fifnd
	;
			for (on rejected busfree.\n");
#en)
{
	ahdnt tcl)
{
	aritical sec_int;

		scb_, CLRI6toh(completion->Wctive;
	}	hscb_p(ahd, COet*/sc+1) & (GPTR, sg and necI/O0)
	initiarespntf(ssage.
hd, SCS				   titup_ahd->Parity Error" },
	{ DPARERR,	"Data-path Paritdebug & AHD_SHbusfree req {

			I this roudate resegoti:%d - %pts.
		 */
		ahdusfree.\n");
#endif
			;
			}
w(ahd, COMPLE operation.
ahd_inw(ahd, COMPLETE_freeze_dev CLRSEQINTSccb to TUR, asserssag_retrydebug & AHD_SHOW_ busfreDISt)
{
	returnd LQO command to TUR, asITIATOR, Code_state
ahd_s_bus_to