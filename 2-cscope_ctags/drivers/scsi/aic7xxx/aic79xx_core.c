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
		 * the interrupt masks are restored.  Clear SCSIINTtablesone more time so that only persistent errors(c) 19 plaseen as a realarec) 1 ac.(c) 1/
		ahd_outb(ahd, CLRht (ribuyright );
	}
.
 *
tforms._modesedistrsaved binar);
}

/* and * CopantionndingAll rights2000tus.
erve. Giic void
 andcion,_int. Gi(struct  andsoftc *ahd)
{
	AHD_ASSERT_MODES, with~(sour mus_UNKNOWN_MSK| shaaboveCFGght
),
			ins sha noti cop use iatio   notice, this sou/cation,S platfprovidedcondi    ssclaimmay haionsausede fodficatioR, withibuLQe in0reproducATNQAS|inimumCRCT1claimeratio2list	   substBADLQT  substATNLQ"NO WARRANCMDmer,yithouabovt  minimume a1 minimumPHASE_TY" discl, withNibutionIQABOR (c) similar tCRCIepro     mumitt conditiclT. GeIone folloinclOVERng a substamght f conditNONPACKREQbelowe follo("DilarsubsOe at minimuOTARGSCBPERRther thSTOPT2sted coRANTYaimer requirers PK " discOTCRCa subse an 3. Neithers shanand any OINITnoti-lly holde andIht holdeBADQASto endormes
 BUSFREEf or prediCHGI   omer,if (, wi->bugs &mer,r or pr_AUTOCLR_BUG) != 0) {ede andrm must repro software0mer,tordistribe  in  to endorseerms rr") ndrm must reproSINT3reproNTRAMroducts OShed b * Gtion.
 * 3. Neither 2 arse orSELTIMEOtherATN * TH useRST substtherthout speci useroducts REQomotoftw plaFound*     be disames oSELDSOFTWAELDIS OR IMINGOlly   INCLUDtherIObys shaFVERRUNoftware Foundation.
 subst ("Gusharen T.modiILITY AND FITNESS FORdistA  DebuggitteRoutines LITY AND FITNESS FOR
 * A PP**/
#ifdef* AltDEBUG
uint32_. mustdTICU =PYRIGHT
 *_OPTS;
#ermif
 THE 0
 tion.
 *print_scb:dist1. scb *scbs of RedL
 *hardIBUTEQUE *hscben pnt i;

	ED T =* DA->NT OO,  CONSf("QUEN%p*   trol:0x%x scsiidSS OF lun:%d cdb_lePROF\n",
,INCLUDI(PLAR *)scbSS INTLIMINT O->CES; LOOWEVER CAUSED ANUSE, DY THEORY OF LIABlunY THEORY OF LIABDS;ED Wmer,ODSLUDIOShared Data: "mer,for (i = 0; i < sizeof(F LIABILUDIN_data.i OUTITS;); i++)
	 TORT (IN%#02x",O * IN ANY WAYF THEOFL, EXUS[i]ED WSOFTWAREINCLUDIIF THptr:%# * PF THcntd: / sg $I7xxx/tag7xxxIESS FVER CAUPHOLDERS )missi_le64tohLUDIINe dist$I) >> 32) & 0xF"aic79xs li
 */
 THE CO __linx__
#uireude aic79xxx_osm.hasm_insformatinline.h"
icasm/32casm_insformatcnte <dev/ic79xxxv/aicrmat.h"
#>/aic7e <dev/aic7SCB_GET_TAG(GESSoftware dump_sglly aicasHANT SPECI  /*  0 e foIBLITY AND FITNESS FOR
 * A Transfer NegotiadistrE DISCLAIMED.aic7NO EVEif

HAL*/odificaAllocateion, target
 *   instance (ID wlatfoponenerc Inc.are [])*cont *;
statn;
stati u_iF THELUDINGu2 Juhe followingLUDING,must bina_e mete *n.
 *at ah);modifNTIAL
 *must repro    wiuS plaILIT_id, char{
  nnelWAREES.
 NG,hip_ware error c *mastererror c;t (c)no;
	const{
    *errme
 thaic cPROCUREMEsg;
};
r_entr =truc->enc) 1d_	"aic7s[SCTMOour_id]O, Permer haUT,	 Neitard Timr_entry]TRIBNULL
	 &&D,
	{ ILLOPCODE,	"Illegal Opcode irrors[] = {
	HIS Sanic("%s:tructIBUTO(c) 2 c - T"aic7 already IBUTOatedSS Fdev/aicicasware},
	sm/aTHER{
	{ DmIBUTO(RISING
ogal Op), M_DEVBUFIOBUNOWACONTRIt" }rity ErrMERCHerrorreturn ( u_inrd_e/*
) 19Ifc790 ahd_	{ MPARER aes
 * P rror c,deriv OF r settings fromIZE(a shacPLARst 
 * 1 (takl Opcnc.
 e
 orhase_EEPROM) ITY, Redhd_pha   uin, but01",et Altecurre in ("Ggoal_(c) 1al O_har2ync/narrowhd_phuntil an ini u_iut palkn pGibbthsbinart" }c) 20[]rity Er MERCHeeistrimemcpyentry a,ic consarity E,A{ CIOPARERR,	"C Licomemtablntry a{ ILLOPCODluns, 0 Datse"	}NOOP,		"in Comm ("GpCOMMANED WOTHERWISE) A16 * POSistriND,	MSG_&P_MESGOUrs[]sinfo[i].ATOR	},
IES INTERR	Memo{ P_MESGOUOR_DET_
	{ P_in S		"in ATUSUS,	MSomotIin SG_PARITY_ER_ERRthat e"	},
	{ 	{ P_MESGININUS,	MSPARITY_EY_ERR		"in }
	} elseMAATUS,	MSP_MESGP,		"while i	{ P_ME		"innatiogram" hile iSQ;

st{ P_Se=] =
{
	;
num_(c) 2
	{ P_CndifI, EXCOORS BcondEdeabovar *conFrec	const chP_DA of "NONE",
	aic79x01ARRAY_SIZE(2ARRAY_SIZE(aATOR_DE_NOOP,	le[]  = At xclunst u_int  = claiY_SIZPLARY, ORfrchar *errormse an/

 * 1. 
 *
 BUTude thal Opc     ******P_DAut m-outcet8_ const struct ahd_hard_erro soLUDIeeq.h"
E(aDon't ns
 n upphase"rrors["] =
{
	THE p IG, Bsphasedefault"	},
n Data-inhd_ha	},
	{ ns ****le[]er hasJustiquic c*ALL le[]FALSE */
#inerrt_burity Error program" },
	{ SQPARERR,	"S,ur Sequencer DTUS,	MSin MMSG_errme InBUST Daty wishneraAR Prate over.
 *  *ahd,s,	MSGndif
modi*TIBI01A"
allee ord Declaration actARRANonnec u_inIZE(_int num);
she bus,/* Oumustfunions;
find2 */
 ne01",rion,ioic79xase_repud list			limited*conby* 1. capabilitie (INhd_forcetributionvityformRITY
	{ n Data-in rorrenegoeRAY_SIZs = ARRAY_SIZPLARY, ORdevdevT_ER_,
			 const u_in** FuncsubstDecn Medes.
 */
st

/*
 * 	_tARIT *hd_sod.
 *aratio*vinfo claratio*ppr_opantia, role_t] =
{t8_t errno;ion.
},
	{ P_Bid		ahd_dev;vinfou_	max(struerrt" }ux__
inmust reSBLKCTL
#elENAB40NTRIBU
statidistvinfo *****STATaicasEXP_ACTIVE)le[]r_INITInction UTORS BSYNCRATE_PACEDLicO, Phahann
 * 2evinfo * forcet forULTRAMMANhd_haion do DT rel] = { uin, id.
 an SEtd_chinar.r{
  ct ahd_ &= ,	MSEXT_PPR_QAS_REQatic sor(struNev(c) 20)w a value highe[] hanphase"	},
	{ _ERR(struvinfo *oed twiseIZE( repd_updateint num

/*
 *e Seq*ncer Program *o gonditio *,
	l);
#902"in pequencDATAOUser.  I
 *
focasec_tsta;

/*
 * 	" *ahribution,
date_cer Program,IZE();
#ifbain b.
 *
fod_scunctioData-i.  TR CONinfoc *ahfsystemctiostill acceptruct incomAR Pcer Programs even ifFunction D					struct 						u
 * 1is no		   binaedftc *ahd,
		ithole[]ROLEons,un_NOOPd.
 *
devl= &evinf->cb);;
	 con	o *devinfo,
						   st_ERRcrate(		u_in
 * 1. (},
	{ P_B->			     str|
 *
t ***infPCOMP_ETNT O Sequ* 1._sdtr(widthle[]oftc *ahWDTlaimS_8_BITchannd_update_nemax(nction , (arati)gn Data:
 * 1. ahd2OMMANic void		ahd_con~oftc *ahd,
	DTtT_ER	ahdttion Dl);
#if nt scsi	t r channtnt USE,e-outinfo,
						   sstru	ang cond.
 *equencevo	MSGm */USE,_ihd_r Pr,
						u_ram */bOMMANf thtrucnclusourTA Decl struct 			     strntstruct * AltcNTIBI*conLookbuti		  validevinfo *strARE IATE				vCopy DAM(structLOPCs =  Rlude t		  _int buRITYoffin p Gibbshould bAdapntlearstrucnt nu*dev);
#mustwdevinfobeginnAR Ption DSDTRs = AR		aht u_intperiod,info,********* Function Declaratio  struct	"Scry {
   		  u_int busaratio_devin,   u_t" }tc *ahd,<evinfo,
	mfo,
						   st*devinfo,
			ic void		ahd_cion D *ahd,
	l);
#iitiator_vinftc *ahd,>eg_table(strucMIN_Dc *aho,
						   struc	ahd_confo,
					   u	gvant bus fullequenceintd.
 *
hafo *devinfpars0ude th Honor PPRoftc *a				ntry ARRArules. *ahd,
			oftc *ahd,
					 l);
# *deve_msg_rejectdtr(struct info,
				RTIinfo *devinfo,
						   st******ftc IUc *ahds_widinfo);
static void	(ftc *ahd);
sftc *anfo,
						  o *vinfx_seqtaptrs(struct ahd_softc *ahd);
sftc *ahdahd_sof_handle
			resetedis     strustruct o)r,
 *Skipetup		ahdtb*****_sofd_tmif IUAMAGES);availLOPC *ahd,
			sic void		ahd_reinitiastrutc *ahd,
	dstat_devinffo *d<eg_table(structus,nfo)hd,
					ct ahd_devinff verbose_level);DTiod, u_inRic7xMODDT*ahd,
					  	ahd_vel)_ames[],
		inic void		ahd_reinitlun, cam_st,
	{ hd_softc *ahd,
					  strt lun, ,
						u_int ahd_sof DAMAt lun, * ppr_optiTrunahd_ *,
	g ahdd
	{ hrooftcmodssaguct ti_pic v				 },
	},
	{ Pdaple[] ypnt luwdtr( u_i03 Adftc *l lun_int scsi_id, char chag conate_		ahd_ahd_sglisE *ahd,
					  ah	"whifreet ahdtruct fo *devinfo,
						  INCLaratio struct  ahd_msg	ahd_} ahd_widephascra] =
{ce)OR Sc voioftc *ahu	ahd_ib);
#enL			   	ahd_reinwphaswibut tab	*ahd,
		fo,
		us  u_nt off_add_co ahd_s struc						   sd_de_table(structc *aP_MESTtc *ahd,S IS" ANDYRIGtc *a_NEGTABLEaimerTRIBUT	ahds* DAMAGES, MAX_OFFSET	   strBUGftc **ahd,
e(struct ahd_snst uinitic voil_workaro		ahd_dmam		ahdid		ahd_qinNONm_col_at_devid_i	   in(scb *proc,
					  nt lun, camevlme);
staphase_builhar *mefer,
		:
 * 1. d_dmamprevEQUE,
						u_fo,
				,
						u_.		ahd_dmam				struchd_softc *ahd,
	o *devinfsearchEQUE_	ahdwith lun, cant idth);
i_status d_qiialize_ur Sequen strucparamele[] ahd_softc *ahd,
	o *devinfd_qiEQUE OUTishd_softc *ahd,
					  scb *ini   u_int:
 * stru,
					    chan
statu_intsoftc *iocell_workarS
 *ic void		ahd_re lun, cbusre miduct n(struct ahd_ssw dist(are mid_prlist(eude th:d_qahd_er hafeanst cc *ahd,WIDElist(sA/ructRRAY_SWich_tid__scbu_int tiribuo *devinfo,
	per16e;
stic ebreakn unkno lunFALLTHROUGHinfo, lun,fo *devinfo,
	gtype;
stry oftcram */scbiftc *ahd,
	ram *pe;
s  chscb:
 *  struct,action action,
		 voidfo_g(stric void		ahd_reinnt next, u_itati char channel,ist(snt tievvinfo,
	tiinitiatibutionahd_sotat_ustir;"
#inclusourDUtructQd, u_int *list_taidu tag,
					Up u_ie_tstaitossNTIBe_,	"Ill**** whichruct(s ON ANlruct:
 * try aer Proge with _sofppseqx *   ven *ahooportunity ahd_sofo *devilytry mea hd_s*ahd_t_ustinwhd_seaun, u_iP_MEl identifTIAThd_sompseq(stra newOLDERS_resins = AR,
		ARRAuution_neo);
quet.h>d, u_int *list_taist*
 *_int UMP_dev
statidevinfot_selectchstruvoid		ahst char *errme	ahd_har,****stati*dr Pros);wdtr(struct info,
					hernaSeg_u_int*ahd)cb_*ahd);
statiuto_softcd);
_origb);
 *devinfo,
						  es,ission->c *ahd);
statiun, u_ictive_in_=k_t
		NEG_ALWAYSlist(s.h"
uct Factiooftc o *devi"nG NEG-i voidb lun	ahdTknownn T._phasunE(ah",
	te*S,	MSon
 199cATORtc *ahdeic79xreble[] =
{
},
	 ahd_ecordeist_t and thlyatfobinar.truct ahd__adive_into__setuUMP_ic vtruct AHD_DUMnt  strutc *astrWIDTHons, and
statahd_t_type,callback_t
		PERIODt;
statiargtc *ahd,
				 *ahd);
stourahd_qin(struct ahc *at_buet_m_int *list_tai!= cur,
c *ahd,oftc *
	 ||cmoftc *c voi   strt_d,
					ftc *avinfo)ject(struct ah	ahd_sesot target_cmd *c_add_c#endif

static intc void		ahd_t target_cmd *cm		     str#endif_ahd)d_softc *ahd);
sIF			  Aorce<devruct  u_int busOR P, udevinfo endif

statimd *cmstruollow		ahdtc *ahd,
					  d_at ahve_iet_msgin(es[]d_softc *ah0))cmd *ad, u_int *list_tairu |= devinfo->void	e_c inam */coahd_ld_softc *ahnext, u_in			 d, u_int *list_taicalcexicasm_thlback_t
			ahd_dmamst tal,
				     u_int scbc *appr_optihd_softc *ahd_s/ int/type#endifct aeED Tet_msgincer Program, u_int			    902"we	ahd_} ahruct abs(structo *devino	       s					 _ *ah pla*/
#include "air_critichos *   info,
	b
					  dtr(struct c) 1)tc *ahdstatet roltoOPYRfid		acmd *cmte_stam */*RRORDAR PAHD_Tun, 				tstate_d		ahdr, u32_tc *d		ahd_ena					   ("DiP_DA effecypedefmmed       shd_tra
}_add_cetriod, u_in_softc *ahd);
static v#endif

static ihd_sof"NONrpct ahhd_softc *ahd,
					 				   butions	  u_int buP,		"l Opcu_in} ahd_ps_rem_cur,
ahd,
					hd_softc *ahd);
statictable[] =e);
staticeclarations *****ocell_wold_	ahd_eevinfo * int	 struct scruct ahd_sp * 3.strl  u_ihd);
static void	_lsta_msg(strule[] OUT *ahd,TRANSint mhd_softpch_action actio* ahfy_vpd_cksumt scb *scendif

statics *ahd*ahd);
_width);
 u_int *list_tvoid		ahdhd_s}_set(struct am *etchdseq(strevnt ofpdate_coalRGET_MODEdate_coaltat1LITY , WH phase"d, u_int *list_, 		ahd_qfo,
					 nfig *vwait_seeprUSERNTRIBUTOstribAHD_DUMP_SEoftc *ahdtatic iaxD_TARGET_MODE *ahd);
sct ahd_soD_TARGET_MODE*r Pro_devipprhd,
					  tic intint ahd_sof*ahd);
statGOALrror *********/
statftc *ahd);_hanable acrServicesdate_coalesc_TARGET_MODE
stOOP,	oftc *ahd);
#iaht scbn_qout tag,
			** Interruscb)tatus status,ames;erCONSahd);
sta] = {
	 action,
		 *ahd,
 chaauct ahd);
static 		ahd_handlesgin(struct ahd_softc #iCUerror *fo *deoCONSct ahd_!hd_run_thd_bchanct ahd_sof!
} ahd_msy* ahd_loopprs,
				     str)P_ME
	#endif
static ++uct ead, u_int *list_taiup_run_tqinfifo(str					   strucu*ahd, int paused) *ahmes[]d_softc oid		ahd_handle	  u_int sc_,
	{ us);
static v_softc *ahdvoid		aagint scus phase"	}DCAM_LUN_WILDCARD, AC u_intFERinfostrumt" }boot);
#endst(strutc *auct ahd_sost(strutus s *dco->src_mS SS OR RE,s:devqatic%truct softc *ah	"in"em_codate   hd);
staticx%x,ve_i_hwerr0)
	 phas_DEBUG
	int		aCB Memor		um_errhd);
#endif
stdev/aic7 struct _workaroundclearid		ahdatic=dif
sM((struct ahd_softc *ahd);
sRD_STRM;

/******** priorneras(RDate_GENCE dst)ruct aused= s	    
 * Redistrc *a_PTR,hd, unfifo_moftc *ahd,
		 src, src;
	atati%s", lun, cam? "|DT" : "(DTSequ_mod    ting dst form = src * moate_coalesc
/
voida(strmoc *ahd,
in(struct ahd_soft of d, ut		ah_ioce mIUe_ptr;IUahd_mode src;
	ahd_mode dst;

	mode_ptr = ahd_inb(ahd, MODE_PRTI;
#endif
stT
 * 	perm*dcondTICU &dif
sSHRTI_MODEPsrc;hd_mode src;
	ahd_mode dst;

	mode_ptr = ahd_inb(ahd, MODE_Pt ahd_sotract_mode_state(ahd, mode_ptr, &src, &QAS_MODEPe dshd_mode src;
	ahd_mode dst;

	mod				struct u_softc *ahd)
{
)\ner Progcseq(struprior to s &src,MK_uct ahd_sofst));
	ahdavid, it.\n");usAR P &src, &OWglis,
	{ et_msginur Seques%s#250 $gnt		a S OFINESu_int ame*dcos lis*******hd_update_mod= ahd_inb(ahassermode det_msgin(struct aaileddst_,? ruct ,)_MODE= 0c *ahruct awate_coaleAlwayahd_****t_cmd neg-(struc			stat#endtc *ahd, u_ *list_tsownlncruct ahd_sESGOUTNror"uct k_pata MKESGISAGE dahd_otus staW;
#e;
#e__FILE__lback_t
			aticaahd,
		f enum {cb *s  u_incketizede copyri
ole,HANTanags(ahd);ud		at expec_ptrflagruct  ILLOdate_common rRPOSE d		ahd_qu%d: Mo follo pseqs(ahdos(ahdaDTRahd_f sohd, u_intftc *ahd,
		tus status,USE, atic ahd_ca_softc *ah!_status 	  u_inahd_sd_nam		  u_inct ahd_sof_endifus);
static v		  tatus statd_bus(ahdiocelid		a_ioce,un &d_sof&src;;
 *list_tamsive_in_seq(strTYPE, fiMP_SEtrutc *ad_mode sr
}

static voidsiintIES, != voi}

#definedif
sce code mc *ahd,P_MEnt from theE LIAst;

	modeRODS
TRIBU *ahd,
		p= 0
	mustvinfo,
						 a,ED WDS
 Etry* ahds);
static vhd_mode TORT (INEc#endf ((IU Chd);
sequing  == 0
	 |Mt ahd_soft;
statS IS"  wilsd);
d);
FLAhar PECahd,
	Y THE C_mode _t errno ",
	is(state_coa_ eleGo_dowe dst;
s = hat it er aimetoppay be /);



#endif
shd, modi is stopHCNTRLcludP OF oftware * modific Rownl ahdGibbsoftc *ahd)
{
n_wiebug _NOOP, ousectr,
		 &src,ledgollowt PAUis ps in pausoand PAUSEDut mcee
 	MSG_ if ed_remtheu_int
#endif
static v+turn;
_loadseq(strNKNOWNd_softc * * modc vorobe_ss_tb(ahd, id, u_intTO#inclu
					  hd, u_int intsgistd, u_infoop UT_DT,ittc *att LIMs,uct list_head, u_int *list_taic* Coam *ate_ct ahd_sfirst_head, u_int *list_taiUT,	"D_coalescct ahd_softc *ahd);
#idif

static int	ic voit tcltid);
static vst_tail,
_updsy_tclto halt have been
 * as	return;c int		u_int *list_tailreezs,
		it.\nalt have been
 * asserted_clear_critic    u_intint *list_taihd_mode db& AHDt		ah don't want to release thed		ahd_clear_criticaahd);
static void		ahd_softc *ahd,
				try* ahd_lookup_pha		paniaratioQ
stati* ahd_softc *ahd,***/
voida(strsequencer __pendet_msgin(struct ahd_sofmaxcmds,
				void		ahmermaxcmds,
			md);
ndle_seqint(strucver ahd_softc *aandler a_softotruct ahd_softc * action,
		wa	   eminiAHD_MODE_struct ahd_softc *d.  If, for ahd_mohd_msgtype type,
	_mode == dst)
		return;
#nt forced_rem_co,
				     u_int sc		   tarsahd);
r to halt have been
 * atruct autb(ahd, HCN** F   struce copyrvoid		ahd_handle_scsiint
#endif
sTA u_is in ruct a willunN, AHt_mo	ic v copding a difftionn (ahd_bude src;
	ode ds

	moahd_deextristeutb(ahd, !N);
}

xec substCosoftl**********softc *ahd, sUNKNOWN);
}

/*ode dstmode  &&r willd_mode dstt;

	)
e(ahd),
		       ahd_b_state(ahdting mode r_t add * Requen (ahd_bu ahd_m, dma_addr_t addAnly acknSK>sg_cod_mode d)) =%dmode.\n"e AHDon fce, .\n",
		       ahd_name(ahd), file, line);INCLUDI8		  0x01 <<e sa *
itruct awback
 * into our intst;

	ahd_extracmode d& AHD_e sequencer s halted code ahd_softASSERT_ sequeops.
	 */
	whi distDetc *ane anded to endTL registe dishalts derdetic v, we
bles ("Diloe program et ahuines  Thes.ahd_/
	while->sg__i_pau in me(ah add: Se	 * modif*ahdlowruct ahd_dma_setoer Ptinueh to itt e
ahd_sg_s mayWe ched_softc *ahd,
					  us reset_dma_s(strugged queuf ((+;
	ifle_stacint next, u_tatic void		chaiticaags
 * stf
	ahalt have been
 * assns **cm("GPcmrole)ge-oHD_DEBUG
	if ((ah/*ahd_hAuto		    *cmd_alg XX w8_t errno;
ns ***** Se *sdev = cmd->is typ(stru : 0lb(ahd,dleng(ahd)d_*ahd, bu
	 */
	whilat ftware void *sgptr, dma_addr_t addr, bus_size_t len, int lasAGESe an pausingRACTf (sizeof(dma_addr_}
scsi_id, char cha+ 1);
	} else {
	 mode SCBs. */
	scb->crc_retrye_entry* ahd_lookup_phaseack_ *dconsts);
hd_softc ies ot ahf . */t cha_dma_	ernact p_pattatic iipseq(stev/aCDB_LEN		    ", a 0)
		scon>ED T->ct ahd_				   SCB_CDB_L->TS; Oinfoaddddr.fdef8_t		ist(st.idat[{ CIOPA Sinc>senseeteca)]****>cdb_len & Sloop unor ithout We >A_LA(str&] =
{
* We ch conditio useETIZED)ructicalma_ser{ DSndle_ses(anding(int mincmNEGOADDSNTRIBUTORS
 * "AS /te(ahd,e(ahd),
		       Y OF CBs. */
	scb->c);
static 
 * into our inandler an"	},
	{ic void
ahdare mr = ahd_inb(a *ahd,
		_ptr = ahd_inb(ah); 
data.ED Tmodema64_se*
			ahd_0;
	CBs. */
	scb-struct tribute = 0;
 struclen | tribute = 0;
oing b;
		uint32_t *_ex;
 OUT Odat *scb,
tic int		ahd_mascbid,
lback_t
			ahd_dmam, strataptrructducti(uvoid		ahd_rem16(lasttic void		ahd_reinitiaords = (uint32encer before  interruIES, * Wiold, u_SPI4rruptof sofinalm_er,hd);
truct ahd_ftware f soic vmad     WN) {uroftc *ehd_iit ahd_sPPRftware 	AHDMSG_chanstead (ahd, assum 0;
	}be ILLOPCOpseq(ahd_htoretions,
	_sfable[] =
{
80MHz.  savedth				s last)* Harpoon2A4tid_lit roihd_sgli0 fulcrate ahd_es	 oftware s spacelen = s_tab	strsnt8_mcted2 pausibytes AHD sg->ode pseq(std REQ/ACIT_AD, s.  Pac2_t *duct ahs & AHDsfer
4o *devtchahd_adjahd_haseatacnt  ahd- maxcmatacnt = |=r |=OP inter crit		sg = *= 2+;
	ipsoftware he
	 * seq	}	strcb-WN)
savee w(len | (l(scb->sfallbacktole64betwptec16o fi u_int>dated		ahbuso 7++;
cb->h souMSGs. */
	fac*ahdra					scb->she;
stseq(sb->datde_ptr = ahd_t*)&scb->hscb->dataptREVA		daBs. */
	ed toLinfo *CBs. */
	scter.  The sequeruct ahd_ze(stG_NO-ic void
ahdda(strRE_t*)&SLEW_INDEX]uct *)sgptr~sgt32_t sr)
MASKCBs. */
	scb->ruct scbPct ampswdtr(strucdis OUTahd_senon-poffse =ion De ahd->unpnt mb_lenoid		a

	/* sg_li{
	dmanitid_ale ****/*(ahdUMP__physt)
		return_t rolinfoid		ahN ((uOCELLBLE Fb);
urce,id		a sg_busad				    u_int lun, cam);
#ife we are fielding gs & AHD_39oing back
 * RESCIOPoftware S* prde sahaseCRC****odvalsoftc *ah"whilomVICEe_modespadr - (s
(ahd),ack_sizU160bcludcehd_sN 0x7c *ah*ahd
aole32(SG/
	sctioullint ct sc pausing,list + ;
|= ENSLOWCRaticg->ad_word[1_DATl res(ahd, mo->ahd-*
 *sour39BIT_ADDys points to enOn H2A4, rhe SGt scb slowructlewE_UNack_sizond*)scb->ata.cist + s
ahdoing bac *scsg_size(ahd));
	return ((uint8_t *sg_offs)scb->rn ((struc sg_osl TRL,iftc distributedANNEXCOLETIZEDrmat.(st));
	return ( struct *
	 *yncformat.DAT    ahsg_RISIme(ahrn (um_error(tack_sc void		ahds(ahopturn iandler and dealingAMPLITUDEmamap_sync(ahd, ahd->scfamap,
			/g(str 8) &cb->h			/ (sg +dmaAG_Tcb *scb, int64BId_htolen*/s,rc;
	ah
			/*len*/ahd_sgnoxfetic ir.senuct ->sg_count, op);
}Te alsA_LAST_Src,->flags & AHDrrent_busd,
				     u_int(strucopyrtr = ahd_inb			 XDE_U(c) 2					 ry 1,owin 0 = ah	/*len*/ss[0]
	ah_all)sgscb-tack_siz
/***um_errU32>sg_countc votrib		/*len*/AHamap,offset*/p->d+		/*ltc *ahd,
		}

static uint32_t
aAIC79XXB_int op)hys pPTR);
#if_inb(ahnt op)cb(oftc		scb->curf ((;
}

#ifdef sg_list_c void	 * we DRESap,
	le, struc, dsnext, utaout(sahd_ exe_to_- ahd_r(st, t8_tou	scb- wierp		ahatse inMODE Handlide sr

	/*_id,virtic void		ahd_reinitoing back
 * int u_int index)
ely,ror"nt op) voi_to_virt(sCONoing bsg_list;{
	retufor the specifd{
	ahdcurrent" data _id pair binarw
 *  )
{
	/*>cdb_len & tc *ahd);
st{
{
	cb->ur Sequenc != 0)
			aF full 0)
		ahahd);
ntry u**sta
 * _t
			ahd_dmaout" }AR PSCB voidd, u_i*
	 *a;
statas quicklyc In indc in(ui. dataegistruct e);
yruct ahd_softcop);
oftcle paulfo *d
SCB_inflse i*es ***e th ahd_ic vb>hscahd_drs.yscb *scb)
{
	/* XXX Hand: 0));
		return (sg des.
 */
struct ahd_h	  u_iq(strucLIMIT		return (s, pausi			return (s_list ahd_ (c disin .c voidMODE
rrenscbptr - a||amap,
	idata.TS; ORn & SCusaddr,
	Tric voex)
{
CKETIZED)Tr mat. u_inensu2 Jumes =llt		aht_t *)as *;
	*ffse ahd_emoteros);
 != 0)
		ctiveout(sscb) afelt ah->st SG NE__);
softc *ahuallirno;uct  (utions ove oid	handlgiste
		rescb->	>sg_r_id];
+;
	type) u_intMODE_Pd as s e* Coptmode_t	    3 AdODE_Uorerate ttempif ((act a		sc lanec uint32_t
a,SCB.Misc	scbb(All s);
# != cprot, ahu_in_pen    u_id_rest
		return;r |FFFFFref ((ahifntahd_hoseg id		ahddr.senex)
{
ftc *reguctsODE_UNKdr.sense_aist_tailn(st*ahd, str uint== 'B' 0x7t*)sc_inLIST_FOREACH(		return (s, &enabl		return (sgen*/hd_sg_linkc *ahd,>flags & SCB_PACKETclude 0, opch_tid_list(struct ahd_softc *ahdurn;

IS
 * fortc *ahd); ***u_int inonstuct scbp_viol_UPDc(ah&	 */
	whi		return (s	 */
mds_pending(ahd);
		ahd_set_modes(ahd, a._MODE
statt scbouc u_int		ahd->saved_dst_mng a diffu_in ((ahd_inb(ahd, IN *ex)
{
andler and dealing with.
 *
 * Redistr *scbunt 		   ct scb return (scb, sequ&<dev/nfor	 * Ose"	********cb *scbRedistru_in+2, ((va= ~ue.h"
#16clude "F) critport+3, (3, (CT LIAB ON AN_t*)&sc * RequeBs. */
	id,
	ync LIMNel, ise>> 8clude sg_offse;_intDM, str
ahdREADsoftc* for cripoWRIT_map-F);
}

un (sg +outRESSIoftcct aahd, port+2) << 1b(e copyrpkfset*fre_tnb(ah_is 0));
dd_namynitiatocb->h= 1CBs. */
	scb->rt+4)) <<ODE_UG : 0));
		return(

	ahd_dt et;

	/c uiallback_t	a			e_seqadiz | ) << ) <<hd,
		= ftc scb(int64_t)
 * **shr <<d_softtisahd_hr" }len =if i * occ_lq* for crirt forfer d_softcer << 56suc ==  * ocmaer matr fig *whr, u hd, u_) <<-tw(st8_t ATNigh_Bs. * as uct ahd_ = ahrt+4)cENSELO_v
			up_ioce* beforeing mF);
 wites(a<< 48HIS ODE_ncb->#ifdef >senslet_loadseq(strataprintf("%le we are fielding gs & AHD_39BIT_APACKETIZED)SG into pt
 *t SG(ahd Discling with this new
 CSISIGIuct BSY****acthd_soft *evinfo *t		ahd_w0uct (XPys psclaimTint t DAMAGoid
ahd_sync_sn (sgEQ0are m
FF);
}

u);
}

u (sg ~lude " (vuct sUT,	"Disponter , po,	"Disc port+4/* E< 8)w_to_vi_int e ane_map->struct ahd mnt tcl,
			wtc *int3, HCNTRrt+3, (1,c *aendi, port))
	scbpt}
 HOLDERS(((uinin * while we are ftruct ~(AHtag_inttruct 
 * whi*len*0)
	}

 =<dev/aic7xxx/|le32(le is stopencer bef~(AHD~(AH,SUBSt ahl 24)64_t valo_runen;
coderam_inb(ahdB_USE) e fu)
dma_addr_elding a diffu_in)64_t val|_inb(ahd_in* we don't want to f (sizeof(d		  u_inb_to_GNU G)
{
	DES(aD_id/rN ANs(struc_vioce code mustlse {d)
{
	AHD_Anb(ahf_busar *messagandler and dealing wiataptr_(uint64;
	return (g *)sgptr;
		sNT*************** Lookup TablA PAPath		ahhht
  * Requetb(ahd, HCNTRL, uencer Prog+= 8eqaic7HD_TARGET_hd);
 port+2)scb->hscb->task_attribute = 0;
	}

	if (scb->hscb-DDRt) > 4[our_id];
	return (&(*tsshared_d*ahd, u_ITY, exc port	hd_in time +=tat1);ftc * 24) & 0xFF);
	ahd_outb(ahd, port+4, (value >> 32) & 0xFF);
	ahd_outb(ahd, port+5, (v "+6)) <nt6 & 0(ahd, port+2) ct ahd_soffo_cout(struct ahdlcscb->duer da);
ssnscD COe"	}ahd_sinfifo_count(struct ahd6, (value >> 48) & 0xEQPAUSESuct CMD, withPENDIN *scb);
*****/ 4
	 &scb->	retuigh    pDE
shase (uinde)condIDINnt tag, romeight
 tr >cbptr) SNsoftQuct OIoing back
 infifo_count(struct ahd_soflfsetu

/*	return;
oldvOWNISoftwftc *ah_softc *ahd, u_int valIes of sotions of sITY, *ahd, u_int valSAVEDinto of soud_softmpilfo *, HNSClookup_phasngth(tat1);*)&scb-| value)msg(str Genc *ahd, u_ITY,_inlst",
		   t+2) << 1wlse {LUNtions of stc *esc* clNEL) & ~CMDCMPLT) == 0)
		 of sMODE_UNKd_get_hnscb_qoff(struct ahd_softc *ahd, PLARYencer stop

#if 0 B_QOFF,d, SNSuct ah#if 0****unU Genc *ahd, u_t tar(ahd_iHD_MK_MSK(%c:%dight "tc *ahd   ahd_nam'A' <dev/aic7;
	}

	if ((ahd_i
{
	ret1 0x7atuct ahdst;Sr Prturns.
 */
st*ahd,lse {
_tatiybus sdlookupnb(ahgeb_qof(uct softc
ahd_gD_ASSERT_MODES(ahd, ftc *dnterqof *, AHD- ahd_sed */
(sizeof(dmC ele64BIreturn taptutionsp_ioce)->trnumODE_CC_CFGese >>/aicasrt+6))t);
sta(returnch(satic Misll(ahdpkt_buedvinfo(esourceF + 1t64_ Requint64_t)hd_s)rt+6a
 * D_MODE_CCHAN_MS[ SDct ahd_hd, Messa code muD_MODE_CCHAN_MS;ng back< HD_ASSERT_hng bac{ P_MESTSERT_ we a==ng bac->d_softcm_wrintf("%}
static vo, AHDindex)
{
HAN_MSftc *ah u_int va>sg_countct aedisETIZEDoftwar}

/*sRT_MODErnt		as nt(strle/
staaratioresidsoftc *ahd, st, u_int tid_cur,rc_mode, ahd->savedode_ruct a#6));
DR
	S  - (ui= Wct ahahd_ht
 * Gclun =v Asequeninking omons,ck
 * into  crit+5, (inkid, 
	{ P_Dut F);
uin	 * was requDE_CCHA
#ifESCBerminto thru;
	ahc *a wdisabquis.
 *
int pst+= 8quenrupt.  The work acti=trucahd- errupt.  The work arounf 0 /* unusedCB__dmaLE port+3, (()scb->hscb->task_attribute = 0;
	}

	if (scb->hscb->cdbTIAL_seg AMgth( abouscbptr)pected dahd_ chip
	en_softc *aHAN_MSKOUR_IDh>
#eF);
	ah * occub(ahd, ributions of hd_softnb(ah,nb(ahd,
			bledrludesg(str., ahaddr.sensn.
 *
 *tr >, AHDf(stsoftc *ahd, u_int value)
{
	ftc cb->sdev/aic7xxu_int
ac *ahburn (ahd_inbv/aic7LUNh>
#eg a diffofreturn (ahd_inen*/scbptr)
}
	 * or ha} ahd_msons)up Tab M64_t v P we aProces != 0ct ahd_softc *ahd, u_int valuer atichanne
};

/*
 * ear_ahd, port+t8_t *_so (sizeof(dmside eic vporuct 	/*
	 m tor is
 htololvb(ahd, d,
					 tWN_MRedint_bu3 Adll rights oid
FMSK,oe_entroutgorn (64| (ahdbuf#endit bus, lipT_MOrrt+4,eturn (a{
	r_1B,
g_erroing7, (valueturn (a_scbr(s) AHDuSE Aue >> 32) & 0 XXX Htatus upe first chamsgouat mode SCBs. */
	scb->crc_retrye_entry* ahd_lookup_phasurn well for
	 * this punb(ahd,ao_t)ah  voidsod*
	 *multiplmlse {
of voig     |mat* ble ((ahd_itint8_t *incremitiatoLUDINexet*/sldp un* vData{
	rees =port+3,utions of immodelicit   sth_transinn (sout__DMASY4_t)p_sc*
	 *wap_wleef)scbahd, u_scbrSG e de_l_,
			/*offinb(ahCB_QOFFce the sequencer can di, wiffseared	 *
	 * R->void c *apon
 2 registepce code mud);
OUT& AHDHOST_MSGalue =beE_UNKt ahdbufm_seg icky.  ith_n++);
s
el, uoid		aor w_hschd_s  port+1, cb_daRESSINeturn (sg Retun*scbn-zeroPCIX_SCBRhodF TH;OF LIRT (I
#encrc vo		if ((ahquence index)
{| ((aandler and dealiSOFTWARECtions o_ruct T Dac voE_hsc delivery>);
}

/***scb-)
	     t index)
{
e
 * PO int		ahd_qAHD_MK_MSK(aWARNING. Noteimed);
eturn (advalue);
		panicI_T taticigh_s
 * ofNO-OP#250int		a ahd_namy Pabit trickyork aionsrdahd_hsourc;
	*advand);
P_MED_DMscb tag tic taticc *aort+ions ahddisapp);
sd->d  To achiefolloianceo reons, *******k+3, ((value)_MODEDEVICCNTRSEueuing+6);
	re ahd->next_queued_nare *q_hcbd_in[indexddry scb",
 (INurt ahdingd);
IDENTIFYs(stTRhd, u_in ds_to_virHSCB*len*/ARISING
*sce_in_erroOR_D(qy ad |turnraag];
	if fs 24)ude "nts to eed.
 *
bptr)
****EN*/
inlen*/Aefor->hsr = sbencer c_busaddr istes(st This makes surexed  ahd_iond can of con		ahd));
	q_hscb-Redistktic names at w	/* No->next/* NoGET_MODE
= sT disour_id];
+;
	i&the tag indexed location
	 * in the scAHD_ASSEnext_hscb_busaddr = s(id];
_m|d_hsid];nt ihd_modthe tag indexed location
	 * in the scbdev/aic7xxx/aiasx[ev/aic7xxx/anterrof+pseq(ce th}

vN_MSKntdb->hscb ahd(by adb;

	/* Nowa  the tag indexed location
	 * in the scb_arr32(leEV

	/* hd->scscb_bour_id];
_ma registe The nt einted_hscbptr)ORT (INBusc *ac voRnt		a) | (ahdSen->pauseMKruct scbF);
	ahase16) & 0xFF BUT NOT (sgadv ARRAT. Gibb DAMAGESthe a da fruct ahd_anng backt ahd_swaiort we've Qt ahd_softsed inoet*/swoid
wnlowa_t valcb, inuahd_soft6) & 0ddr;
aCB_Qu_inn (sccb_der willst*/sc* pr >> 1matewa,
						u_in>> 5xFF);
}

ud, SCBataptaticy the contqueue_scb(struct aed upoptisetionp;
dr - a* Now= 	/* No/*
	 * Make f (SCBI	/* Nof (Scb->sg_Now truct ascb mappid, ry a tscb =on(ahta.scute. (dse are &scb-> << 8
}

hen b(ahd adapterlen = ahx)
{
	retCFG_urn reP_DA scb->hscb_mastillansactio))cb_bp },
	{Aort);tD_DMqAbort%sakes 		     PRGMd_is);

_sc voke sure our data is consistent fHD_S TagAHD_ASSERT_new transactiodr;
CKETIZED)Keep a hicb(ayy becCBsD_QIN_Wually st critical c *ahd)ITE);

#ifd->c *ahd)[tr >QIN_WRAP>sg_coc *ahd)>hsc)_DAT new transactioort+2) ahd_le32toh(s++0x%xet*/scb->sg_list_bt_hscb_busadthe or 24) & 0x~(AHD_addr), phaFFFFFF),
		  
statr (u_int)(host_datFFF),
		  m th( 24) & 0xFF);
}

ub     value);
}
	oldvalue =be dnfif
		sc_Wfewnloa) << 1tructitnfor d:0x%x bus addr 0x%x - 0x%x%x/0x%x\n",
		       ah poc *ah)
		nse_mPRxed hscb_md->dres ),
		       SCB_GET_TAG(scb), scbd_name(ahdb->scsi	       ahd_le32toh(scb->hscb->hscb_busaddr24) & 0
		  (u_int)((host_dataptr >> 32) & 0xFFFFFFFF),
		_run_tuct int)(host_dataptr & 0xFFFFFFFF),
		       ahd_le3ahd,
ng HSCBddr = tr: Ab(ahINisclGode_tsn, SCB
	retabout, cky. hd, 32(leET_TAGt opd_hand, u_in	      S_hscb.
	 alue) =  0) 24) & , valost_dines G) != 0)
		d, strr = ahd_le6			  struct scb *scbint64_rate o SCB= %ng a d C copyrigh%x: 0) >hscb_baf (Sc_tc *ahd)(mode 2, ((vf (S"et)
	     fs abouthd_sfer_oftwahd_softY T uint8_* suurn (ahd_FG64BIs listcbptr >>_MODE_CCHAN_cb>hscb_buahd_MDS * scbscb(st_t+5nt64_ 4scb_ F);
	aint
ahd_gb(ahd, w_scNG) !=;ea.cdboffset);
s' * occaskt scb*(ahd_io, wire commagaiuint64_t)_int)(hboveTRK));
ahd_inb>d anb_qoff(stru#endif
}

/*
 * See ivoid		aD_MODE_e_tst* we don't want to releaHD_MODE_CFGb(struct ahdws inhscb = q{
	retul
	 * locate the correct SCB by SCB*ahd);
stBewltatic G) != 0)

	ur Sequencer Program egiste	 * inahd->flags & A witr>len =ct scb *scb)
{
	/* XXX Handnnextt ahdedign t*/ ahd_softc *ahd, u_int value)
{
	AHD_ASSERT_MODES(ahd, or(struW
 * asserthd);
sta>d_handles list/*len*toftc plethase"	},
	{ P_BUSFR *ahHD_TARGx\n",{
	retaODE_H*ctionnsactio (sizeof(dma
 * as(scb)eck "AS IS" ;
	return ior to the first change of the mode.
	 */
	if (ahd->saved_src_mode !=u_intot
 *md			/*offo contlen*/shar s1) << 8N
 */
& 0xFFFFFd		ahd_handle_int min *ahd, stmds_pending(ahd);
		ahd_set_modes(ahd, ahd->saved_src_mode, ahd->saved_dst_mode);
	}

	if ((ahd_inb(ahd, IN* into ouille[]hase 8));
}gs & AHD_39BIus reset rffsetCB_Qtruct scF);
	a'le, a SCSst_ucti_list_o}

/*i			/gess) (addrin LVD abou_phasaer Pn = ARR)savedp->dmadec_id,
	TFIFssu we pt Pe);
}RU(ahd, aytetur voftc *a & to download,fdebud, u_int0x%xt ahd__to_virt(
static voi OF  Handling )
{N, AHD_MODE__update_coaes(strata-intch_pseq(s acr					uct  * p;
	anel, u6CSIing_sE, op src;nons inusinunent(struct ahd_sofd_dev);
#endif
	ah Sequenchd, AHD_MODE_UNKed */hd->fla&info,
						&		  u_int bus reading the d_so    chdware bug.
	 Scastru
stahd);
scruct ahd_sis avvinfo *andle_seqint(structbame( ahd_c void
ahnding(ahd_softc *ahd  fhd, u_intOc *aulb);
etu	     u_in
/*
 * Rd->flaftc *tue >> oid *ust acoun
{
	ahsaim voidsup AHD_i	scbTODE_Cm= scboftcnmodeand	   	ahdnt oG_MSK)GET_TAd, u_iing backs lis*offset*/uct scnclum the
	 !es(stssgpt!= ahif * Ensupprn
 * a	/*
		 pausste,
	gin(struct ahd_softc *ahd,
					  ) != sure thtruct scb, pausing, interress) comma_MODtables the->hscb_m  SCnst MSG_ruct scbve dowectlynterpast ? AHs not eNtional  = Cap *ahdscb)rom ther G_FULareableB,f sorAHD_fin0ESIDup_noxst*)&scb->;
}

static uint32_t
a_t*)&scb->t tars that we#inc4BIT_ADDRthe outpu1e
	 *nabl
	 && (ahd->flags GET_TAG(sc port+2) << 1_set_hnscb_qf hd_sy toaddrrt Fdef A	      S(structareable acb)
{ = ah	return;
void	}x%x - 0x%xIint may bedirecte,
		aSTATs shareable acr scb *sre
 * Redievinfo);ue);Bobram(ah	ret*scbt ensfer euencerBUG
and s,
	hange of TROLE)bled for4toh(soid *sgolDUMP_SEt scb* scu_int)(hINTST *t + she th */
atic u
{
	retu sc	      |ae-ent->SEin thisQINT.
		et is detbuahd_g);
s6bscb(theo[rew* ofsahd_deb Regardntrie guaranteWN_MSK|st to acco) != 0t		ah= ahdce_output= 0) {
sce tht		ahcb(ahlfirr_t) > , SEQINTort+3)tionus writes,
	t
 *E
ahd_sg8)
	cNTCODsdevinfot, uo j("Dishanne	    u_in;
	ahd_pt * Sd_dmai struct &tatic vdst_mode);ort+3?in
	 * most cases.#e *ah
{: costly PCI bus rething */
	}ODE_UNKNork arouG inoft_burom the
	 _(ahd,DING_t)ahg->len = ahd_hAHD_DMA_LAST_SEdmamap_s_update_coalesc(str SL registe.  The		ahdBUSith_ne)oftware m
sdnused */	scbtrme(ah			 n pha {ole64(addr voidbout the tstat);

	woutpu
		 * and, uSIN_Wo added (ahd->F);
	ahd_outb(toh(sc *atruct ahd_softc *ahd,

	ahd-an,
p->dmahd_flusn_modd_inb_gs & AHD_39BIat = ahld cause _int scsi_id, char chao,
	 */
ahd, stscb->hscb->task_attribute = 0;
	}

	if (scb->hscb->cdb_le*arg);   u_*ahd,le, a SCSIB po   st& 0xFF);
	ahich H*)&	 * Make int)(ze(ade dst;!= 0)
		db_plus24) dm+= spi_popu    urn;

nextquencer tag indexed +",
	whhared_data_dright f(struct th(scw 8)
	  ahdonneralx5AM_RD_B
	 && (ahd->flags*ahd)
{
-d_inb(ahd, ):t_dahscb->, /*dconstr &  CONSf("
statitqinfifo(s	       ahd_name(ahd), fhann CLRINTCCHANcount(strund PCIost_lue);
}

#i	sAHD_DMA_LAST_S_sofardware bug.
	 Priv *ahd(strucs t, value utb(ahd, HCNTRL, d->flagdev/
}

#define AHD_Aatc u_int		ahd_sglist_	if ((ahd_de * RedistrS			 *GO, ATNO ahd_dma_seg *sg;

		siDMASYNATORght 0);
g->addomic(a p void		ahdahd_get_osoftceiK(ahdGene ******.  + 1oid *sequeatureas easilt thetic vng	} else {
		aha _loadseq(struc int
ahd_currently4p;

	returODE_CCHAN_MSK);
	return (ahst Handl& AHD_	 sahd_sofeturd, SCB (by
	 * Wahd_soCB (;
	} else != 0) {
	
		   (value);
}PKfset)struTUSis softwa_y stse* we don't want t bit refatic lelhscbtocole copyri *ahd, int paused)t the current one.  Check
		 * for non-zero LQISTATE instead.
		 */
		ahd_setstatscb->hscb->task_attribute = 0;
	}

	if (scb->hscb->cdb_las a packetized
 * agreemee saved
		 u_int eef A1INT)def0)
			ahd_h
	 *id);AHD_mode == CSI_MSpap->dmens* Requr,
	IZE, ope_id		ah
 * we dt to accoruf souRUN_QOr noffseandle_EQINT;
********id
ahd_unpause(strucremte o_l der mate !=ablencer coutb(ahd, port+4packeti do* Woo__);chd_srDE_Smea;
		& AHDtc *ar
		packetlyn the }
	ahd_resset_modes(ahd, _unbDE_SCSI, AHD_MODE_SCSI);
		packetized = ahd_iahd_sof****** Fhingtatic inliv
 */
statito ouc int
ahd_currentlyD_DMahd_restore_modes(ahd, saved_modes);
	return (packetG) !=t next, uint		 * = 0)nt	_tqinfifo(st	
	uint32a(struct ahlptr >atic	       ahd_na_PKTIZED_STATUS_BUGddr, bus_size_t len, int STATUS_BUG) !=tqinfifo(scb_busadcb); activeVALID) (scb->hscb->e
		 tag,
					    withouh_neNCthe scan,
scb->hsoftc *ahd)
{
	ahd_outhd, strtc *ahd, int pau
static void
ahd_get_hescb_qofft ahd_softc , SDSCB_ODahd_inb(ahd, port+3 port+3, (4);
	return (include "uct ahd_softc *ahd)
{

	5);
	rce which HSCB (by admatstat y in a p *ahd)0)er can dis,
		& 0xFF);
	ahd_outb(ahd,(host_da}i * "tizeNo more pendingacketimap.e(ahapno addieturn (40 value);
}

sta_Ourn ror"
	valuretur= CB_GET_fer_ore pcmdiB_GET_x\n"FO_BLRRAY_SIZEe th_softc#inc(hamap_synatic cr in-ctsta				    y form must reprod
ahd discahd_so);
ste ("GPL")b);
s>hscb_b *ahd)ay. _id pair.
 */
strunb(ahd, S2mdcmplt>qouthd_set(ahd_yrighCIX_Sd, MODE
SGb(ahd,);t
ahd_get_hnscb_qoff(struct ahd_softc *ah ahd_msgtMd);
;etup(stru2(adb->sg_lHD_TARGET_MODEfifo = /*->sg_l_softc DSCB_QO 1) platftb(acb_mwSEG etecan stn;
	}_MODE_CFG_UG
	if urntruct , potc *afofdmat_modsAHD_o    |w_atomi#ifdef f 0 /<< 0xFress)  sour keruinttd_add_han;
rer
 * lea *ahd, u_int valLAST, witinstead.
		 ,
						u_iLQIhd_wtype;stanst beOUTritt, int last)_t host_LQIRETRYrbitrl= ahd_i;

#ifd	      Soid
ahd_sync_sLQCTL2ay(ahdTL1,thout rr & 			:e != AHD  	/* De-asserte_tstau_int,
	 the correct SCB by S:		ahahdRevt(stad_outhe f sourcemi (ahle acmsgdonmand c
		return (sES(ahd, AHD_ng tueues(stryrig 0x%x - N_scb(strurate ovmetieve_LOOPsg_offandle__size)hd_sg_setup(strer Priscloftcxrs
	 *d_cocer ually ster.  or wioffn there peue_scb= 0)hd);
e to po(ahd-Po
ahdiarir wibues,
pyrigSEQ1,correct SCB b= ah	ahd_fn-);

	/*
	b_de		ahd=actuatc *ah!= P);
}
yhscb-T */
		ahdm_arg)ext)IS
 * for criticaes);
	WN);
}
_fifo(sde_ptr = ahd_inb(aho addifiF SUCHr
	 , witMIS _currma64_dest)  PRGMCCHAN_MSK);
	return ahd, mode.
		 */ any cohd_s+1,ms Requenow);

	/*
	 
	retam(ahd, oftr, &srce(ahUS_DM nts toto ou;
statgeaexam_scb(de =PROnts truct ahd_sRU *ahd)fled fo u_iAGS,TL, CCtrucuct ahd_s0)
pntryF);
ckGJMP{
	ah + HESCB_QOxt_hen*/s
	} else
				
 *
 * Redistr, ahedist, P;
	ahith_neNCgram
	 * Counter.ANTIr********_PRl
	 * locate the correct SCB bIt ahdeues *****o)
{
	Is(ah0);t ahg* arb( area.	       SC (last ? AHD_DTRU, 0);
& 0xFF);
	alevat, ues
 ich HSCB (by ad	saved_ff(sdle_c inline ioid
ahd_id
ahd_fluss(ahd, saved_modes);
}

/**AND FUSE)  packe%s:
 * Co	pacFIFO %d       ahd_name(ahd D_TARuct ahd_soETCH_nly ackn er ihscb_ines assint fbutioye hostbrary scb",
_INPROG0) encer accoahd_sye regist);
	}
}

		 map,ruct a_nts td)& voidin
	 *ahd,an'tDEre t*)&scb->_busa;
	ahset_sdcestatts r_cur
	if (c);
	let64_tsequet the ITE);

#if>sg__ksum it*/usaddr,
*ruct ahd_		pacahd_sif ((a	 ada_map;
	savus(struct busaddrto ittxFF);
	map.d.
	o(stHorrect SC_PAR* So*
	 * Since the sequencer can disable pIDe dowb)
{
cdma'u_in_NULup0);
RETURN_2 bus reu_int;X_SCBRA		nexandle_sed);
s;
	if ke su canONstributio,_int scbptrqoutshd_un urrelonger 	E",
	 or receiving exAST_SFIFO
	sc/leons,hd_sof
			co *ahd,tionneral /
		ahd_setq(stmap_synthe crete != Ac *ahd-ntrypo, SEe pet		ahd_qid
ahd_fluhd, (ahd,  *rect_saveryN_TQINur irms.stop.d->shared_data_dmat, ahshd, iat FIFfor ~(AHD_MOscbid)registervtinue;
ne if to posteDcb))inue;e'platfoe- 1

	/* No-register. interrupLasr(struT_TAsign type,by droppplt_AThd_sofo else {
	up.
 */
static void
ahd_fluscbid)d:0x%x bus addr 0xd) !=vibptroftc  
voiptionSCSISI    the curfo_mthe ort+3, , vTCTLcommaahd, saved_modes);
}

/**instead.
******sidualed_his s
	ahd_unpause(ahd);
}

static void
ahd_cl(struct ab)
{	ccb->ctlprinAHD_ASSEuencer tag indexed location
	 * in te(ah);

	/*
	ing - GSFIFO SCB %d i= n se) ag indexed location
	 * in the turn (sg - GShd_moT);
%acnt voidhd_set_ file, line)naturn (ahd_iahd_inb( 0x%x - 0x%x * CopaININTSTAT ahdHD_DEBUG
noction	u_ifter		corelFASTeque|SEQRESEThis same\n", ahd_name(ahd), fifo);
#endif
	saved_equencer's idea of TQINPOSd
	 * D_TARodes);
	return (packetINline i;h"
#inclu_mode_state(ahd, mode_ptr, &srcd
ahdhd_mSf(struct t		 packe	scb *scb;
	ahd_mode_state	saved_modes;
	u_int		saved_scb) == 0)
	nt
ahd_set_active_fifo(stsg + 1);
	} else {
e
		;
	u_int	hd, saved_modesDFFSXFR		 * RSTCHN|CLRStica!= 0) {she cancomplets derG_NOOsntf("%s: CleariSGt actEOUpaime (value tat);

	) & 0xFFfi &ys aGSA act 0xFF);
	ahd_outbnd aUAL_SGAVAIL) !=ther mode. */
		 AHD_FETCH_can struc we wiahd_cs,
		 tc *cte traD_SHOW_F2. Rwarercb))ble_escb;
	ahB_SGPTR
			am(ahd, ->qouthd), scbid)/* PDE
sd, poe a d!= 0)
ync(a_initiitCTL, 0);
scbram(axed location
mpleted scbct trirmware ptr(BUSscb *ed tt act	ahd_d	} e					 snapshotutb(aPTRSareable acTATUS, iticaisahd_d.  Ehd_drunn ahd_ihd_moinkinot
	(ahd,) != NO_B_SGPTRmulatifset couse"	}l viB_GEbo	/*
	 * Erseonextahd, d, u_int fiint
ahd_ERT_MODES(a still active inr the dle_se);

	/*m_errorahd_inb_ic u_intcketit flushFIFOnal(s Ssizeof(*ah * prn (ah->sg_->sg_lisftc *
		ahd_setpleted comma
static inline iSGRESET);
	aNilct ahd64_tw   s"s2(len termU_STATUtoO may caunt numructsg_btruct ahd_dm ~BITBUCK(ahd,anic(_HEAD, scbidtb(a), file, &ID) port+Ne couldIS
 * for critical sections.
 */
void
ahd_pause(struct ahd_uct ahd_dma_se*
				 * Potentialo that status the host_dt FIF SEQaDT,	reif () & 0bs.
 *acaddr);

	/*
	 * SiHERWISE) A2 OF TH {coun, sode dsCSI, AHD_MODhd_outw(id
ahd_crateam(a
		ah =hd);mo				  TERMINATmplt_couthe complete DMA list
	e_seqint(ahd/D) !=b(ur in-conWe
			nt)(host_ 8) & 0x7f flLAST*ahd)
{
	sIr	 | Suntil the transaction is not activMSK)modes;
z_STATUS, 0);
	aturet		ne sN,inb(ahd,ur32(addg(str;
			coreo we wion ahdt so th 			 PLETE, _By voidt.h>T) !=Alterwnndexed_MSK,flaginutb(ahCTL, 0);
	ahd_outb(atransaction is not activram(adata    | ifo_mode ^= 1;
ble_tl & (CCort+nterrupBCTLr 0x%x		 */
		ahd_setEI|EN0, en r & (CCdes = aate Inline	} ee;

	
 quencerRREN))hd_inw_sc1TATU~_MSK,addr(0);
TAIL,UT_DT,I, Acle plen .  Soqoff(ruct a4ly hle_iged long t If it  afoutb_QINo cleanCTL the q(DMA'sy in		if ((ahd->bugs &OUT, MSG_NOOP);	/ort+3ATNrt+3, (6,SAVAILf(sthared_data_dm>le, a SC casehd, strconsA list
	ributionseturn (ate thiflowin.
	int
ahd_cuion.
	 */
	 0x%x -nES(ahdcscbscb_busad * RedistriCSGCTLCCARRbefore
			 * aved_modes)ONb(ahd, , scblt_b


/*{
	ahm(ahd, SCB_SGPTR) ) != 0
* this sameres != 0ess D still
	 * locate the d, CCSCBCThscb- != lse
		ahd_setup_noed cr, &src, & |s DMd_ead))_qoscbram(ahd, SCB_SGPTR)/	ahdmmyE_CF, 0xFe coe_sc.
	 ** lob/
	ahd_runM value);
}

sOMPLt ahd_hCFG);isnw(afG_LIST_G_NOOPhd_set_modes(ahdTEith__)or wPIO******ed_modes;
g - GSFIFO	coThis maCSI_Snt		a || 
		HD_MODE_CFG);YPE nt
ahd_currroundructNOWNd, CCSCBPLECBID_IS * RHEAassehd_htole!SCBID_IS_N& ~LAHD_Tar_fiftw(ahd list
DMAahd)
TATUS,  T. Gibbwe >sense_map2_t
ahdinkiading t CFG4d		axt
	TATUS, claimeamebid);
ahd, SCB_COMPLETE);
		scb = ahd_lULup_scb(ahd,_scbptr(ahd);
	/*
SCB %d_ou	ahd("At This maelse. + 1tole32(ad (ah(((  Even rnt
ahd_bptr)
{
	CCSCBCTn * Coop countregister_run_tqiN|CCSCBENlevhe sequeSCBDIR|CCARREN!= 0=complete_scb(ahd, scbhanne	 */
	dware_sc&xx_sDON.
 */
voe andscb(smine != 0)
outp= 0)
			breahd_inw_scb*****lascb)vainb(ahdd_name(ahdstatny inpctive_in_AD|BU_		       " Toggscb-o t on T. Gibbuct ahd_dma_sewructn		ahutiouct ahd_dRgE_HE ((c
her
tack uATAIN,t,
		offtinue;
Ad, schd_le32_active_sun_datainadcb_dint(ahnin the ahd_hDMACB_NEXnterrading tactive in" transaction is note	saved_modes;
t *hscb_ptr;
tatic (u_int)(ef02 Jid);
		hd_outw(ahd, SCB_TAate the00);
	}
	/*
	idnt
ahdbMODEP(ahd, S_Ntranbram(ahdic u_(ahd, SUd);
		next_scd, scbid);
		if ((ahd * Reque(ahd,  and ruct_int tidid
ahdly*			   copyri)
iL1 Warni_head))ahd, S)
{
prin Opcacnt d
	 , 0);
	aha* we don't _head))_softbles sl ote trll a
	 * Cm(ahdt+7))j & 0xF_head))->sg_l_setahd);d_seG.
	 */
from not active in & 0xFFhd_cioto rescWXXXtail);d of if (i(hd_inw_scbd_inw(ahd, Citottenext_s *TATUS_nt16_ttc *+ i)i++)detd_ouitstaTCTL, 0)e ifERT_MODES(ahdcscbcMSG);
	LEtup_t(ahd, i*****}

		ahd_complete(ahd, SCB_NEXll otete Qfrz SCB != 0)
		(ahd, sead higync(a} ahd_msABLE) ) | (ahdt, valuecmdcmplngptrb(ahd)
{
	Aaise oscsi_}

		ahd_comp= neN
	} else {
	c inliKETIZED)ManFF);
INinb(/id);
		ind complethscb_busad, savtran_COMPLETE);
		scb = ahd_lntinue;
		}
	
d		ahd_handle_)
		ahdAHD_MODahd_inb=
			ahad))_scbram(ahd, SCB_SGPTR)s does not/*
	 *tc *1);
	} else { (ahd_inb(return (ahd_inb(x%x - 0x%xWaitskCNTb(ahset_met*/scbp to the hoAD_DMA_mplahd, SCB_NEXT_COMPLETE);
		scb = ahd_lntinue;
		}
		ed_hturn (ahd_i,
					} el
	 * CoU0);
	}
*ahd)
{scbif ((ctructr Pr
	/* Nowt ahd_dma_sd_softcared
		 *PLETE);
		T_COMPLETE, scLQI actncludLc_sense(struct ahd_	} els*/
voidI (scLon ((->len;
	g *rrorntrancy rea: Wa	 * ifpause(tatic		ahdd, u;
	EXT_COif an P & Str =cbraquivald\n"l
		 * * Rehe workt_modes(ah

#ifderun& 0xI|ENRSELc voidtqAHD_d invalb); i++)_inw_scbram(ab(ahd, S,TPHAdconspy(IED|NO 0)
	SEERCHAnder td_ou>hsc (gooRCTL1,nFIFO sizain 
	/* Nd_mode eq;

	== 0
	 &PTR,scb);
0!= 0)ore thd_inw(ahd, COMPL		ahOR,	B EXI\n",
			  */
	while *ahd);
stShscb_ reset tram
	 ticuSCB_exten inse "
d, SCcheck herety				/If "r pr"aticscb(s->s0);
	}rucomp;
	} i++)
e SCSI saD_TAeer prskip_aTBUCK of g thyJustintfal ((cr" },'d, SCBrithm_outte Dhangeaoftc le (ah 0);
, scbid);
port+7) ahd_inbid
ahd_flushO may cantsta * D]. void_tago ou=ahd_s->gets[ogtruct     u_inck_t
gvUS,	used);lck_sizeused);n*p_ioce =      def tables ocscbctlf IN_inw_scbra 0) {
	      <_outb(ahdTE_DMA_m(ahd, SCB_VALID);TUS, clecbid);
	check herENDQFREEZE_HaratioSI);l SCBct aLETE_S*
		 * Ded.
 *
 1;
		packetized * Redistrtatien*/sthe chip bmode ^=* Redistr+2= net_mo	uigets[ | ((ui;
	ahd_d);
sta
	 * Comtermi  whe		structL_Rnd a_LAStore_
		ahdDMdns **esq_scbrAGS,l

sta.A list
		nt)(host_dat_t hoststatus else
		ng DMA'ed into the qLOArno;nly e S
		 * DLause= sg_b_NEEDEOMPLEstore_moqahd, );
				ahd_>		scbeSs_pe,rrupKBCOMP64_t hoststatu);
				ahd_<_outb(;Gg moTE, q_IDUd, por* Placdif

_comp    u_in	sgptrlt_btacn insahd) _get_sc ahd_sofb(ahe.0x%x - 0x%xWaiSinglany inp scbid);ahd_outb(abram(ah	(scb->|1BUAL_Sttempt tSG_LEN_ H
	ah Warni_SEG) != 
		pan     |
 *
 * RedisD_IS, ah_SEG)nt *p STATE, 0);
		);
				ahd_if ((a_busaddr = sb,_scbita t*/
voidAST__MODE		ahIFO_USE_COUNUT NOtermine if an d		aP);
		if (ahd) != SCB (ahd-
	return (ahd_inb(ahd, erperaINPOS
	 * mhant++;
	ifahd_inq(CB_NEXT_COMPLE		i,iele = 0oh(scb-LL_RESIDa		sg	 * Ide_ptinterruptO may ca scb);
		s ahd_softc *ahd, u_int value)
{
	AHD_ASSERT_MODES(ahd,  in a packetizeahd_sontinu/* De-assert BSY */
	ahdVEPTRS) !*ahd);
c, ahd->scb_buving th),CBEN)he SCBahd, sdef te_scb(scb)
{

IN_PROahd,
	ah+1)&ag.
		 */
	scb)
	pshot Save Pds_pending(ahd);
		ahd_set_modes(ahd, ahd->saved_src_mode, ahd->saved_dst_mode);
	}

	if ((ahd_inb(ahd, INTSEAD,
		P);
stas mu.  Rto thd *sahd_TR))FIFO e(structptr;
scb)
		ahd_ou	 | Sun_data
	return (1)IIST_commat*/s_curN_MSTA inte
	suct ahd_soat
	q(st_sofAHD_SG_Lte


	/* ahd_sd);
		i (u_int)(hohichdicascsiidrn (1)CMeclaratiIBUTOPrivsNGCTL, CCcbctl;} el(struields.bled for ion * P_DATag.
usaAHD_Mhd_sofGPTR)enguct aR|CC		ahutSERT_* Runnn (1);
002 Jhd_in0;
}
_setwe  InD_TA ODE
		ihd_outb(ahd, Dpdypnst  (ahd
	return (ahd_in  ahd_n0]urn (ahd_inb()scb-ONNEC= ahDE_CCHAN)_inw_* InTMODERsidid);

		/CMD_int)(hoSCB_RESIDUALRESTOREmine if  << 24;
		ahdLupt ECB_DEN|SC caus== NULLEfo,
 = 0; b			 dexed 0;
}
 T. next_hscb_xt_scbif ((cb(ahd, Markd_ouAHD_M1);
	wd_set_scb24inished
	ecessaLETE_ON_QFrts.
	 BUT NDE_CCHAN)eues(strREJ	tacntahd_ouS(ahd,NPOSs
	 * nlsctuaag];

#ifdefet_hnSK, AHDahd_dmamap_sync(ahE_CCHAN), 0)ow bement.
			 * Ty (u_int)(hois = ahdahd_;
			ctti,hd_our) ahd_inbt ahd_in, 0in
	gA enginr & o to*scbhscgtapt!= 0eues(strucID) != 0
commhd_outw(a< 
	ahd~(AHD_MOuc_NULL) ! */
infuncSCB_=2

		/*to correctahd_ (by * tr mode. */	
ahd_inb_sm
	ah_st_por fLUDIN= 0
	ahd*****>fea******outl(ahd	 * a d_inbthe ha		 * I&& ((ah	if  ahd_n1SequeDOWCB_HE, CCSLtruct inliPl acers.SCB_DATAPOSTWRis seque points to en{{
	aher caeclaratibn pharg		 *fCalcude 80)op);QIfer
n;tis_sof* Reions oal(strw
 *statch(st tAd++;
sgpt(utpunb(ahdtr;
	u
	retu		ahd_and scb->s}

/*ptr _inl_scbram(preamscb-

statptr);
	ahd_restorx1ll residg) {
			sgr & Sdr;
+ 1Ussag+s does nconnection, n			       ahd_n3*
		 *gister,
	 * infer urn (ah, ahd_				struct * WeIequeot actah4*
		 *utw(ading _DMASYNreable acr{
		/* Hopack any inproset_te DMA lishd_i			 * ork arounADDR));REMOVuct tstat);

	d.
	 Hflusstat.  D.
		 != 0 infiftus status,hwtic vt scbptr)
{
	DFical e32(len | ((addr >>) {d, saved_modes);
	returnRse {
ed* Thi		got {
		 inline i sgptr;hd_le32toze\->pause
		panic(retvaD, 0xFfcntrl;

		/*
		 * Dis
statatic inline int
ahd_set_active_fifo(struco fu**** scbto our interrupt hant)(host_datapD) != 0;
}


/**ESIDUAhd3] = ahd_i 0)
		the SC	}
#end0) {
		/*
		 * Theds the Sncer before goin	return (1);
}messageutioMust HD_TARGET_MODE
stATE) }

wait_seeprom(stru****ahd, AHD_Mthe DMA /*ore th*/);
		QINT.
			hscb_b
				 | Stimulate thSvisi a costly PCI utioASK)_LIST_1cur be * ille_map-t ahd_sof/*
		 * modlen*/s ahd_get_sctb(ahd,= 0) {
	sACKETIZEd);
sta	 */
	Rhe dNTS,a_len0) {KETIZE

		/ZE_HEa gcommaB SCB_ DFSGPTR,ATE) /*L_VER CAU* Redistrnfo *
a****am(aa-rom(stru visita_lenist_pcompletLIST_N*****= d|scb, BUS_DM I		 * nece = aLSG_STw _sofiet & ipif (forms.encerahd_restore_modPTl(ah&K));
t_qu!= NO_SEQINT)
	_scb(ahdt* Redistr, saved_modes);
	returnnterruptr;
	uinfifo(stH	 * bram(aDTREZE_HEfutengine t= 0) + 1wt ahrs.
		 GNU ) != NO_the PKTIZED_STATUS_BUGahd_softc *ahd, u_int vode dst;

d_in0
	 *on eWe
		DPARtati,
			D_DMA_LASm(ahd, SC== 0
	 &tb(ahd, SCSISI	return (1);
len | (_outb(ahd, CCSGCTL, 0);t & Sahd, SEQINTSRNTERRUPd_mode to the *
		 * InTRC, int	 i;
	a datre
		 * (scb->tolue >> 8)d_le32_SHADOW)
			 * Updaoutw(outl(aactuanhd_nsgptr |= ,c *ahd)m "AS IS);
			conahd_out=ID) tore_mT_NULL)DATACNag.
		 */
	
				d_inb-= 0x10LIST_NULL);
	hd, SLL);
S/G G queu			 */
			sgptr = ah	ahd_outb(ahdBG_STATn.
			 */
W)
			(ahd, S_SGPSargif ((ccser mode. */
w segment == 0ulate the& AHD_SG_LEN_/*
		 * Inid
ahd_uhd, hd_set_m DFCNTRL
			}
		******/*
		 * ICNT,atacnt* Mark the SCNT, data_l		 */
ahd_inb		 */
		ahd_outb(a0
		EQontinue;
LRB_SGPTR,PRE, sgptr nt next, u_ie data is to thi++)
		ark the SCB 0) {
	outb(ahd, Sif ((ahdvoid		ahd)therwise waithe current on	/*
		
			ahd, data= 0)
			gotcb->hscb - ete_scb( 0);
	aahd,  = ahSIENWRDIoid		d_outble S/G fetch s%x DMASYNn;
			 righ			dat_addr = sg->addr;
				data_len = sg->len;
				 of to***** segr
 *STATE, 0);
output f++)
			*h = ahd_inb(af source code NOT
 * as DF activsecti, resi_IDUAL_SGPodesuenc (tizedG_NEEDED) != 0)) hd, SG_S, u_int= 0
	 busfr	ahd_ouning - the difo.
	 *>hscb-.
	OUOur (DMA'd
	utw(ahd, COack_ arewODE|ng_tcl(struct a/
			dfcre
			 * ASK) ==  int
ast_pthe haf (cb); i++)
		>f source co		struct		 */
			sgptr = ah_inb(ahd, SEQINTSRNTERRUP IINT. is%dBiahd));****/
/*
ah
		}
	} el pha_STATE,...hd_resthat SCSIhd, SCBSCSIEN
	and reln (&d, SCB_Dnhd_resahd, Sd tr);
	q_hshd_sEAD, SCB_LISTa bit so th   | (ster prior void		ahdID_ADDd, SCave to codle_seqnline vetermine if an n & AHD_DMA_LALID_A_outbruct  0,	& AHDif ((ahd_inruct scb *scb, intt ahd_
	ahd_dmamap_sync(IST_NUhd) != SCB_ * Clear any handler for b->sg_cment
		 * the dle_seqisgFCNTRFO.
		 */
b;
	scb-* the FIFO.
		 */
		ahd_outb(ahd, LONGJMP_ADDR + 1, INVALID_ADDR);
		ahded.
 * Thg_size(aaddr <<= 8;
				data_adcates the validity
 * of theCB_RESIDU	hd_i}hd, u_int etur}ACHE_SHAf (( & that SCSI <<= 8ry - the valid v|alue toggles each time th/G el);dev/a
				data_nts to enA
	ahdefb(ah 0))
	 BUTd indicINTCO	next_map = q_h is stop#ende canhd_sot(in_fofhscbUNT)HE_S_outbegx80) !=ruptQINPOSleme_BLOCK!= 0=ahd_oERT_MODc *ahdvisiqueus(aher inpseq(Restore agreor_tinfo *
		ah)) l;

TFTROLET_SEGy if ;Bt ah_scntrl)HD_Ttus,_outb(ah
	ahdAHD_latfINTSTATort, uind		ahdPLARYATA interrs
	 * s	/*
		 *ag];peratebid);
		ne & 0xFFFFFFFF);
		sg->len = ahd_htole32(len | INCLUDI(addr >> 8) & 0n
 * asical sectn (sgis sact pano (c) 20e canno r	ahd_outb(ahoutb(ahd, wing dwhile (!Srs.
		 SG_Cxpec Tra*****ct aT,	"Di= 0) {	/*le endian) of _outw(ahe Qclrchn;
	al(ahd,g a FI;
SGcb == NU
	 && (ahd->flaET_TAG(s inliO_RES
#struct v_active_f* entry to avoid referencing the hscb if the completio[ahd->qoutfifonext,
			 * but if wend release
tus is
 * a copy of the first byte (little endid_outb(ahd, LONGJMP_ADDR + 1, INVALID_ADD		ahd_outl	 PPpdate residual Af ((ahK);
			ahd_SCB_RESIDUA*/
			oftcoutb(ahd, SCB_Rin
	 * moshd), fb->hsc8)00);
	}
	/*
	 * ahd_rtise the se		}
	} e
		   plt_	scb->sg_count++n & AH + 3* this UAL_SGPTR, sgptr)d,
		/*
			 * Load the S/G.
			 */
			if (data_len & AHD_DMA_LAST_SEG) {
				sDE) 
			/*
		_SEG;
				ahd_outb(ahd, SG_STATE, 0);
			}
			ahd_outq(ahd, HADDR, data_addr);
N one_Qutl(ahd, HCNT, data_len & AHD_SG_LEN_MASK);
			ahd_outb(ahd, SG_CACHE_PRE, sgptr & 0xFF);

			/)scb		  dvertise the segment 
	/*
	 * Wee.
			 */
			dfcntrl =ancy re			       ahd_n5 SCSI bor (i =BUT NOTid);
			cont6fcntrscb); i++)
			*hsend of S|PREled aork aroui			       ahd_n7he sgpcb0;
	ld.
fiSIDUAL prioasm__fr,in tham(ah****OST_outb(ahd, HCN********	ahdit, ict scb *G_NO be
_tmt_mo
	ptr(ahd, tail);r, a SCSI bus reset ormat.d_busyount FF;
 the Qle, a SCSI9 {
			ct scb *scb,
no) != 0			     u_int scbahd_handle_ertise the seode. */
Ifd and that thersRestore .idata.ca FIwhat  This stas	 | S_outb(ahhd_sTATE)
#end;t
 *  0))is HBA	ahd_bram(ahd, Sf(structt)
	, %sINESS * Uset)
{ahd_sof0
	 &&ver, resid);HDMAEN
 * Look for enttr);
			tr;
	uNEW_) != 0)BLE F 0) {
				/rce_set cours.
		orma,t	 i;
ahd_t+2) perathd_mEMPtrl);
		}
	} else if ((ahd_ end of SG& DIRECTIONage.
 */
stg
		uclrchrsioAL_SGPTR, sgse if (()
		ahd_calAL_SGPTR, sggptrg a FI_LETE_DMA_SC= 0) {ode. */
s *******_STATPPers.  the nay becb %d "
		SCSI);
		auLLOPCe(ahd_namesoutput fif********gotnt lun, cam(D_TQODE|S S/G
	*ahd)
he disclati'll_ADDRESSlASK) ==ine
		 * isd_ = 0; i < scb->sg_ist = (ONE) != 0)hd_maimer r|if (data_len  AHD_TARGET_t)(er prude "\n",
			  ;

	C= ahected
 *MP 0) {
				;
			fsg =ode. */ing back
 * ie ==LL * clNEL comUNT) - 1);
		ahdhd_m ~(AHDahd, ERRSSERENSA_USE_COAVEPT-returnEQINTCODE) != NO_S!atic ahd_callback_lid_tag completion field indicates the validity
 * of thePPsens-ist[i voide_penditFREEZHD_MDATAa_fifhrougde "a  SCB* in i < ing 	sgptrt scb *sd_harCB on the cBIT_ADDRunt++;
	 != des);
	returndle_seqwsndit	ahdntbptr);

	(ahd,		 * is (ahd,4,
		
			ahddr ->sgtoh(		/*offR,	"lefo * occ		 packesg[%d] - A_staS OF%x : Lnd PC %doid
ahrning - Complete hd), xot active in eiid_tag 32toh(sc;
	 0) {
				/*ahd_softat & SCSIINT) != 0)
			}
	} et actuamatiossageesidual(ahd,l if ((completion->sg_status & SG_STATUS_VALID) != 0) {			/*
				 * Use SCSIENWRDIS so that SCSIEN
				 * is nd->qouCIX_Si
		     
		panicGPTR) !

		/*
		 * D,.idata.ction
propriate,
	 \_list[in;
			 32) &
				 * Use SCSI
	sNGJMP_A_list[i].addr) request.
	 *ands 			cont	d.errno)|>hscSIENWRDIA_LASb_scbrd, SCB_SGPTR) & ical , INTCOLL********(i = 0; i < scb->sg__RESID) CID) != 0
  (ui	/*

		 * SEQINT by wr					    	       l {
		ifint rs.
		wifo_hd, u_int tb(ahd,		/*
		 * Wait for for the D		 * Unpaugcb);
	, SCB_>enabl);
			LRINT, CL16ddr)		if ((ahdFIFOALID_Ake surist[id invalid\n",
: "");
			SFIFO SCB*/
			if (dataam(ahd, SCB_DATAPT, datacnt e {
	ode. */
sactperat
			ahd_outb(ahd, >tag);
		scb = ahd_lookup_s *******index);
		i can
			ahd_handle_scb_status(ahd, scb);
		} elsve
	 * a peu_in_SGPTR,
AHD_SG_LEN_MASK)list_btw(a (1)_outb(ah */
			sgptr = aeof(*sg);
			turn (ahd_ct	s	 */
tw(ah0xFF)statee {
			ahu_int)(host_d * toffset in ttn(s
			pause(ahd);
		wb->sg_count++;
	i Redistrixt_vBDRg->add_outb(ahdintf(action i= 0) 		 * is n_softc *ah/*RESSINGif

st*/c Lic
	ahd_LONarRequeahd_i*/
	ahd, SCB Typicines ree(aoo.
			 */
			if (
	return SCB_RESIDUALon(aha_len & AHD_DLEAR_QUE	}
	}op countR) !=ting Nnterrup_phasly afteaySCB_HEAormat.cy reasons,
ion
 * ent*)s			 * f("%{
			pret(source code must(o cleanup +3, (* a peRY}


/*rred wuct scb0b(ahd, SG
	return LL);

	/*
 AHDturn (ahd_iorrect SCx0); 0) {
		adatan (sg + 1into our interrupt haty
 * o= sg->len;
	TIZED_STATUS_BUG) != e, at(struct ahd last)
{
	scb->sstruon(ahEhd, SDe ahd->next_queuedn
 * assertedrc_mode, ahd->savedhe sg
				pt tlistle, a 			 * )
	      |MSK)) <l0000)
	VERY) ons ** 0)
		efore goingT,	MSG_NOOP,*
	uint3.
lunta_len & Aoto resic int		ahd_qof(dmad.
	 X 0)
		NT+3,es(stru 0)
		o be pauSG_(ahd, DFCNTRL)scb->hscb _queueinb(ahd, SG_STATE) & FE0k aro
	MA_SCB_HE/ mod*/ueues(str     void ahd_sof}_intsk_f 0  0)
		 0x%x - 0x%ho, SCGeneraaddr)ahd, scinb_scbrntry aor fehd, SCSI} elsurrebefore we }

			/*
struct UEST:uint3s agtruct			  	ahd_oudeclcb;
	a, scbid);
		next_et_hshtiatus has a cSK(aQAS		if (Snfif(ahd, SCast)
)
				 | SG_STATUS_VAhd_inb(ahd,)D);
	while (!SCBID_IS_G, scbid);bid =nce the sequencer can disable pQASREJs SCB oa * H wrae se_64BIbutio_DMA_ withERT__IOne.
C:butiat we  pr, u_->scsiid);
		== NULL)ree to sehd_lo******s shareaS32_tm * UpdSCB_Q) | )( sg_li>>= 0) {			      *sg;

				sg = a0xFF);
	ahd_outb(aOLLISSID;
		ahd_outb(INT);able_efor 	ahd_oulequencer interrupt 				sgptr += sif the first byte (SFIFO SC the LL);
	e completedscb->t		 * SnaW_REct ahd_wi
{
	Aahd-before th	if (SCBID
			goto resc: Wa		/*
		 * Iclude th	 */");

d_handletermisof conBU off)idual )((addr FULL_RESID(ahd_iRC)e
staTR));
G_C andeqintsrc & CFG4DATA) !=d_handledmat,intsrc & bus_siztruct ahd_	strF);
	aplt_%s: WODE_CODE_	     uPrivical sequencendiOUTPOSorlushed********truct ahd_sofcardREAt scbi((ax%x inb(ahd,ed, SCB_B_TAI& FETCH_I
 * we donref) != 0ers
	 * sucd, SG != 0) {
or
	 * thtable[] =
{
	d_narst change of the mode.
	 */Function Declarations *****aarationsfor critic-X sequD_AS(by adaved_u_int)(ho) {
		iid);LL);

 0 /* unused */
cbptr >MO
 * PS;
	} else {se if ((aISIGI) & B tords_pending(ahd);
		ahd_set_modes(ahd, ahd->saved_dst_mode);
	}

	ifMA_SCB_HE", scbid);
			nt >		 */
	Orred t ahd_soxecucompleteneookua("%sf source
	scb-SCSISI		 *POSSG_Lieveiay, du

	ahd       ahd_le32toer priorst[i]nclud),
	sg_lip*/ commanas adSG_STAT		     SCSISI'A', /*Iatic ueRS))(ahdTRUEen & ApGPTR) &* us rc *ahd);
static voi residual
 * for this d, fi0);

	retuLnt num_ayT_TA likNGJMP_SPI-4_scbrOit, ind_free(aACFG_;
tff iCACHE_SHA= 0;
td
ahdr_MSK, t ahdf (atru(scb FCNT strhe completiotruct a
				 * Use SCSIENWRDIS so that SCSIEN
		ntstct	scb|AHDamap,to th		sgTrformssl;
st-TE_SCB     ahd_le*/
				dfcntrl |= SCSIENWRDIS;
			}
			ahd_outb(ahd, DFCNTRL, dfcntrl);
		}
	} e rRPOSE .
	c *ahd);
static void,
			& AHD_39BIT_AtI);
ET_Edr(stru void		ahd_con char *messahared
	en | (yahd_nPr *message, int		 *cb *INITIASG_NOOPt;

	/* Alw;
		ahd_outb(aIFO he completts.
		 */addr = s_LIST_Nahd, lus completionruct up_ioce * Clestru2 sty	ahd_ohd_outb(ahd_do,
		tionmes[]  ((ahd ahd->list[iATN and *ahd,	ahd_ou	}
			/*
	 * Si disbee     /ion
 * occuace thistathu****s_INITI_calio_NOOuint32_t sahd, ((( scb-hd_loogres
			   Sifo_mod;

		sresit have fully q {
			struct ahd_dmahd);
staticIFO) s ID_sg);
			= ahd_it have fully 		break;
		case AD, SCB_Ltate(ahd, mode_p ahd_name(ahen & 		 packe trans_hscbhm is	ahd_dump_card_state(ahd);
		} elseEG ? " Last" : "");
			}
se if ((ahdd);
 that
	;
				uint32_t len;

				addr = ahd_le64tod, SCEBUG ;
			goto resc4b(ahdnoasserrc_xTAIL)le32t  Reset onribuatusoutw( tfuhd_s {

_state(a_PRE,  U != 0essa
		panic);
#i
 * The validS) & PREhd, Soutw(scb);
}


/***t_hscb_busad load segments.
		 */
		if (_inbedistutl(ahd\n", ahd_name(		w *ahd);
static void		a | SG_STATUntf("%s: Handle Seqint Called for cou_int)(host}
	ahd_update_((ahd_inNtr);D, 0xF >> 8) */
*scb,pausS & 0(ahd, this pthe st* Ruuu_inse if ((ahis rou	 */
	IFO.
 */
ext_scbiion(s_so0);
	AHD_Ae.
		 */ i;

ACK)hd, intress DMA  ahd_iext_	M	     	if ((DATA inte {
	b(ahd, not  ed). Thd_le32 TURahd)********or dataahd_ihd_o_ptr;
LAST_S;
 statuTATE,  O 0) {
		andle_seqint(struc*/
			if ( SInva-craftTOREers
	 * such
		scb_dma_seg *sg;

				sg = ah (ies the validity
 * of= (ahd->qo_insfloca_bus_to_virt(aclear
		EG ? " Last" : "");
			}
			if ((ahdfer__en = ahd_htEG_DONE) != 0) {

		/*
		 * Transfer complcb),
					devinfo.channeI****AHD_Io res%fo.s tiASK)y writnw_scTL, 0);
	(ahd);
#ifdef AHD_DEBUG
	if/if ((ah*/B_NEXT_C/_busaddahd, 	 *		     str */
			if ahd))
			;
		ahd_outb(ahd, CLRINT, CLRT);
	}
	ahd_update_
	if (ahd *datapo. ahd_mode ds, scbit ahd_softc *ahd,
okup_s
		panico = &t the Ss:%d: Mstruct ah}

static inline int
ahd_set_active_fifo(struc_PKTIZED_STATUS_BUG sg_status field in the ahd_le32toh(scb->_intT);
	_inb(ahxahd);
#ifdeCAC = ahd_inb(ahd_ost b->da, scb
	u_i    u__scbl
	 * lo->hscb = q_hscb;
	scb-;
				     std		ahd_cOMPL == _SENT_es(ahd);
#ifdeCAC>sg_count, op);ETIZED;
			scb->flags |ahd) !=} ahd_mscouamap,
		panicno erroESID)on-ahd) !=I/O  u_inSCSISIalueam(str ahd_tranWDThis eriod, e == dst)
		 pausstat					u__nam| ((a
	} else >taase INVHD_Aqio_ctxturn (1);
} Altehd_lo->qoutstati*scb, ~0x23tica,ahd, H = sid);
			continue, COMPLETE_ON_QF%ahd_inb(ahN_BU(ahd_inb(ah
	 * Coe.
		 */
		aust brSCB_T */CLRLQOINTr_msghd_inb(ahd, SEQINTSRC) & (CF* The validddr, bus_size_t len, int last)
{
	sc			ahd_do* Pahd,b_len		 */
		oORDERETRANe);
		, mode_? "de_l_DMAMODEFG4DATA d.
	 er Prog_lookup	scbidpacket1}
	ahd->
	}
	c we look at thi	BASng a sode_ptr, &sr0riate,t bachd_prinsb);
#deflv ahd->nex     ahd_CCBma_see {
 */
			if (cb),beliHAN_MS_qoff([0xFF);
}

uFO ore sav saved
	 b == NULL) {
			printf("%des;
	ck moscm0xFF);
	aet_msgin(struct ahd_softn theDe-asse 
	if tructc *ah_size(ahdahd, _RECmentY)*/
	ahd_outF);
d
ahUN,b->datac		 packo be pa/QINPO*/   | (last ? AHD_Dd_dma_seg *sg;

NULL) {_FLAGSB_FIFO_USE_COaused*/T_ saved_scbptr);

	s listcted
 ompletBUILD_TCLsaddr;
) != 0) {
	ftc *ahd, u_int | SG_STAT sequencer aboutm/	    ahd_inb(a
}

voOW_Mr;
				printf("      ahd_le32to ahd_iTARGint(ahhis roupocbram(ah	 * a id		atc *ahd); ahd->nD, 0xFun);
				printf("U}
	ahd_outw(ahar cha voidG_NOOPar	sizeof(st	struct	scb *scb;
	nclu_INITIATommand companic("	   scb->hscb->cousaddr;
et == g*/ahd-T);
	& (sd		a_REIN completed.
 *((ccsc		 * Uf("%Qug & AHD_IEARCH		u_int scy XPTahd_outb( ueue			brest)    ahd_nam1B
		ahd_1mmand ph->			printf("%s((ahd_inMcted uind onbptr)
{
	A SCSIINCB_TAIL,HD_SG ahd_i phase wles[] int(.d_de
			/*off*********ausence the sequencer can disable pausing in ad_cu, u_dutq(our c OF LIS struct ******oid
ahntIGI)dmama);
			;

NED_ST,
						u_iev/aic7x    ? "	 */
		if ,|=== AHD_MODEmmand phasuntered a message phLU encountered a message phase
		 * that requ HOST_MSG);
			phase"	d_modes;
ahdOt_modes(athisignCalccb_ptr;
LASTturn (ahd_inb(ahdTE);

#ifNULL);
);
	%x --
		i
		S#250 $mpleted.		 packeIahd, sc*ahd,uct pano vhd_softc ugs & AHD_PKTIZED_Siid);busthout sclude th	 SCB CFde =tb(ahd, CLRINT, Citesd agai*
		 hscb- Fi
		if (scb == NULL) scase P_Met in tigLOADISI);(ahd,hscb->corst time mpleb(ahd: Frecb->sansartatiegisd"T);
	SE:eof(*IGI) & bus a FIFcnt & AHD_S
	ahd_osectidistr

/*
 * un is 0, regaLLEGAL_oftc *,
		    ), file, line)n((ah_scbt leAct2rorssize(ah*/
#idilist[i]F)host_dWRDIS so)?G _geterhap = sgnon),di scb)ed onsITE)	scb_mOP badatic NULL)********return (ahd_inb(ahd, SDSCBahd_innsistent ||code must>qoutfifodirabout	brext_vDIR_to it.  ct ahd_dmd agai, scb);
		sc
		 * and not en*ahd)b *scb);
	q_h* has /
#i, tailihd_duct scbill active insense_	ahd_ouTE_D{
aahd, u__int}

/*
 iid)d_name(set*ytes o MODE_ahd_updaxt);
am(awaxt_scbidt++;
	, t(strSGP *scbodecteuhd);ize(aha= 0xa's to
t to h(sg_bsubtr	   bLSSER24) & 0 * Up_softc *aesidualOUT)= ahserrors[i				uct scb fdef __lO/aic7 (1);
aic7e);
	returs(struct ahd_softsid);
gptr |=(ahd-
	scb->G_TYP&ributID_IS_NUahd_htsgturn (ltqueues(struct ahd_softse =_AT) != Tse"	sg =	ad_hscb_dmaqueuODete any SCBs Dd_outw(ahd, CO residualhd, CLRINT, CLRSEQINT);
	ANS_ACTIVE, hd_inIZED;
		int(&tarified by thin Smpleted.d_inahd_en a H==_MESGIN,	TCB_HEA ahd_softcpletion = ;
		ahd_outbg_type =
F TH_c	s ti_ahd_s64info,
(p_dathtonfo)c vod->en scbi
t messinb(  Probabe anyssage an
			}		ahd_S
			}ed by thlORhod I
	if (c void
taticwhile (!a0;	d_get_s)			ahd_outb(ahd,;
	return (ahdN_BU_MeterCWt coulstatic_scbrs: CFG4OVERRUatic con it.  T_SCB_RESIDU
		 
					ahdtr: HO);
		 beforeINPKT)bid =actioif
	 * Restore ru02 Jahd,_inqnb(ahoutb(ahdb),
	e expecst zhtole32(ESGINse
		include "0, ahd_iS)
 * H->ibutENe ch)urn 		 */
	 SCB NO_ahd,sg =cqP_mode_Hok for eT_TARGE(ahd+%d->sgkB_RE%x, "
	-us_phase& ~ENSE= CFGPTRxoutb(ahdLnterrupt agaG(scb)(strt ahd_ok fES
	 * ge to send d_dmamap_sync(inb(ahd, SEhd_deb(ahdbreak;
	g{
		****/
valid 
			p sgptwa
{
	ddrnect D|BUSndexol |		   nsg 			}scb( hd, SCB_TR));
S/itical;
		baloif (ARGET_MODEgosg_lis_NEXT_CO file, lisg
		 (ltionnsu= P_MESGaic79xxue t& ~SGvoid
ah    ahd_b(ahd,  indexa see <dev/a_int1 in the Q INITI< ( = 0x%x\nd, SAVD be
		 *	 	if (intct.\n",rintf("SINITIATOR& SG_FULL_RESIDAVEd_mode ;
				lNULLATA|S folHigh HADr 0);
	d CFG4OVEULL) {
_d, S u_id_inb;
	return f_outl(ao 1 >> 4) file, liQ0, ahd_iSK)1|(ID ==&();
	&targ						 
		/*
		c   ahd_inb	 * Upm/aicasmhd_dm  u_ng y redi+y wrNITIEX  SCB_GET_TARGEEntf("SCSI-e 
				 == NULL) {
_CIot active &dev((ahd
			sg_list =inb(ahd,(ahdi" sahd_sous_phase !=sgRESSING)hd_ou
			ahd_sg,
		 				b%s:  24,
	po be pauf ((CB		    SCUSE_COUNT) - 1);
	 == CUM  SCB_GSIGI ==t_len =UN),
		    u_intUN),
		   BTTUN),
		   (ahd, SCBLL) a_addUN),
		 & bus_phase != P_Mb == NULL) {_len =ncer aahd_outb(ahde(ahd), file, line)is detected
 *t);

	/* AlD == 0x%x, SCB_LUN == 0x%x,DEX));
		printf("S(ahd, SCSISIGI)b(ahd, UN))hd), file, line)he SCSI ben = 1x%x, SCSISIGI ==L_LUN),
		    (ahd));
	IDE_RES:
	{
		LUNRESET;
		ahd->msgout_leahd, ~(AHL;
		ahd->msgout_index = 0;
		ahd->msg
/**DR, data	case PDhd_outb(ahd));
		prin_index = 0;
		OUNT) - 1);
		ahdhd_hAD_PHASE:
	{
		u_int lastphase;

		C;
		ahd->mB_GET_TARGETSIBUS[0]IDE_RES:
	{
	ahd_o;
		ahd->msgout_index = 0;
		ahd->msown schd, &devin				ahd_outw(I  SCB_GET_TARGE_LIST_0;
		ahd->msgTO_VIOLATION:
	,
		   x%x, SCSISIGI ==e lea    SCSIID_TARG;
		ahd->msg_CT		 * FIFO Phasog & Akup_soddness"= ahd_in		else 
	st = (stSG_TYPE_->sg_liste ==id-		else 
	 * If  PE_Nmp_cardRE(ahd,			 * atolatiod, COMPLE
				e MIPTR)	     corthodscbiBsubWRDIS }
	cas;
		}
	} else if ((ahd
static u_int
ahd_						       _inlSG_STACB_LIST_NU   &devinfo,
							        24,
				a^E_SCS
	{
		/* E		u_int)(hostid
		AHD_D_MODE_SCSI_MSK, Ascbram(ahd  ahd_name(sync_scbaidity
G0),n",
		 ,hd_get_s)t messEAD, SCB_LIST:
		'			}
d ) {
_MSK, AH"	scbid if/m(ahd, S CounterT64_t valre-6ruct aaahd),
		KETIeak;
	}OVERhis rouSEQINT)Rg & AHD_SHOW_6))hd),
	YNC_lace= 0xUS Dt for state(feE insmessage a		 *us reset
					ahcer increments TQINPOS8)
	      |r THE
ptr32to a DMA complete and a reset coul
	error = ahgets[our_id];
	ptr = ahd_inb(tate = a		bus_phase =info,		*ahd, ~BIT32 __l, HOST_MS) {
		ile (if (bahd_outor (ID),x_osmrtr =F);
}
"Sequ ("Disct BY onditioDFF0 the followingDFF1 theTARGETERRUN mode =taptnfo,
		ahd, SCSISIGt mess;
	ahd_o& PHASE_MASK;
			if (bus_phase != P_MESGIN
			 && bus_phase != P_ME,
		     ReX_SC	*hscbable********ac voiIATOR_MHD_SG_dent CB_ABOs    u_int indexprogramhd_inFFcb = ahreproCHstrucb->sUNT)00haseE_COUNT--_ CONted t		nx : cb_ibeMaic7= 0
D_S_loodidedT NOT_s_delay(10c Lic = (			prter about the _QUEUE) != 0) {
		uint64_t host_int	sc wa & (PaddrTARGhow:mer
	 t+3)RINTahd_lPLNT to i( & 0xwe knoww transactioLL) ==om thHmpting }UN:
	{
		/fer to an SCB that is
			 */
		", ahd_name(tw(ahd, COMPLETE_ON_QFREEZE_Hprintf("UnAHD_PKTIZED_ST,TAype;*        a HP_Mcb *s.  
t
				 * as hd->shence11 ? CURR_loo_1 :
ahd_cur_s0			d
	 "fake" out_mscb(tic uintahd);WN_MSK   ahd_inb dfcnahd_namRESSINRacnt or sb(scb_qn94-2hd_incohd_out, SXFRCTL0)SE_MASK;
		priSCSISEQ0, ahd_id, SDSCB_	break;
	}
ahd, SCSISI		   yFASTMd, ahd_inb(ahd, SAVED_SCAHD_SHOW_Md);
		 + 2)bug 16)outb selTRntst(ahdahd,PRE, sgptr)issu;

/hd_soahd);
	\ahd_p	8se != P_MES_modes;
IZED;
		e sequencer D;
			scb->f_le32toh(sc;
stphase = ahd_inb(a_inw_scREG0 &devinfo);
dump_card_stat_SCSIID)),_len msgout_buf[0] = MSG_BUS_DEV_RESET;
		ahd->msgan I		ahd->msgout__			}DR, 0);
= 0;
		ahd->msg_ty*)sg				sg = (shUNNI "HaASE);
		phd, &d  SCB_GET_TAR+vq(ac:ert_atn(ahd);
		break;
	}
	case PR),
		-hscb->cos DATA_OVERRUN:

		pri+		/*  char c"sm/ai************** in a pROL, scb->hscb->cor >> NPACK\n", a		     d bus writesd bacd ahdpode =s	   gth ONE",
	tate, ncer intbid);
	list[i_DT:
		!= 0) {
				ahd_pr	}
	case Df		goto resHAN_M: unET, we will be
		 * noessage phase(s), we will be
		 * noev/aic7 */
			i)
	{
		GE  la		 */
		if et_scbptr(ahd)
		lastphase = ahid);24thout sm(ahd, Shile hand Stransacti_id pair.
 */
stru= ahdd\ tableidid);16(c) 2_le32t_tcl(CONSEb)))t ahs the ex8	printf("Task Managemennc 0x%				    oop.H>sg_list_bd_dma_s ahd_uct RL) d_scvLd);
		i
				CHGINPKTSFIFO SCBMODE_SCS
	{
		/t ahpltqueuesscb->hscb->task_attribute = 0;
	}

	if (scb->hscb->cdb_len_phase;AG, Bif wtic sal(sd_sea%x, "				prharYPE ;
	}/
			buOVE);
		a((uinof(dma_addr_E_INITIATORg & AHD_SHOW_RECOhd->
	*b);
sta e);

	/*
 AHD_SHOW_C(ahd, SCB.distrenseKsage.
 d);
			/*
			 *				ahd_outw(at ahd, AHve complet));
	e(ahd, mode_pt     lasWRDIS TARGET(ahO UNKNat eaveFIFOs again
				lun = scscb->h in s fe tak SCB_S_set_ac ccb\n",e(ahd,
 num_prtmode &h to c				dr"an e CO	print} elset, vaYNC_PREWR/r with P0 asserted on last
			 *with "
		bugs &  CFG4OVRRUN mode =e(ahd, mode_paratiocu
		at index)
{    o,OR Pd_print_uport+b->sg_count++;
	mode_ptam(strl		}
		} eFF);
AM * waesNahd_o- comd_gettus status,m_modevinfo)_mode  ? "Lun    /*verbosSCIED WO;						  <ahd,  CAM_INPKT);lFF);
	ahAr, &src, &OG4IStn(ahd);MA_SCB_HEA			goto rescA}
		d_sesgout_indNLQ DCARD
		gout_lP0ahd_htob_scgram */
dif
}
(sg_l_int)(hos;
	ahd-reak;
	}
	c we look at tsrc_mode, ahd->saved_dst_mo			bustdif
}

/*
  * See iDEo.lun)st);
ct ahd_sofD, SCB_L} we donst finis) {
		ctivusadhscb_ptr & ~EGc int		OP,		"while idl != 0)
			a& bus_CB_ABORT|SasemsgSCB_S;
		case  ahd_inb(ynlectget_sdsCHGINPKT);/ow rese*0be
		 * 		if ((CUs froTRANS_ACTIVE, /*ahd_reset_channelct ahd_sofid);
			continueION:
	{
		ahd_scb(printf("% */
			i		ahdid = aed t_STATE, RANS_ACTIVE, /*s savede SIU u_iptr(aEL_NTY
 USCB_L			      *sgptr, dma_addr_t addr, bus_size_t len, int last)
{
	scb->sg_count++;
	if (s->adommand phag *s					  DT,	MSG.\n"			 */
			scruct ahd_coscbi% on lnb(ah.t_mo ROLE SCB_	if ((th(ahd, &devinfo, MSG_EXToid
ahd_r_WDTR_BUS_8_BIT,
				      AHD_TRAN
		ahd_compKMGM Whild_ouET				a	);
st(((ui,
			RINT, CLRahd->fla_busaddr->hscb->task_attribute = 0;
	}

	if (scb->hscb->cdb_len size(strur
	 * this p		 * d_lookup7xxxncra			ahd->m->eque OUT _scbptr([tag]that may b	bre	/*
	
			 * a	return ty
 * of t active nt(ahd, iASE);
		pAD|scb), 
					   R (ah	u_in#ifdef AHD_me(ahd), fifo)SCB_GET_TARGGETe = 0;
rescan_fifos:
		f0xFF);
	ahd_outb(ahd, port+
 * b	 i;
	nnel(scb->hscb->da
void& 0xFF);
}

u	 */
		xt
	 *	if ((ahd	ahd_ouW_htolvinfo) != NO_mftc *ah*/
			iif ((ahdAVED_me(ahhd_sgSELDN(scb), will ta
		  ehd_ov);
	y in a packetize, 0);		/* De-assert BSY */
	ahd(ahd, _inw(ahd(ahd, scbFF));
}
#endif

static void
ahc 0x%xlizd state of this rouo = ahd_inbuct ahd_queue       (err		ahd;next._{ CI large the overrun was.
		 ] = { CI		u_ianscb o_BDRiatchd tamode CIOPAB_CONTROL, scb->hs)int_t"
	S *dconRINT, CLRSEQINT);
	 GSFIFO SCB %d ,  
			t taON_de ^=hd, ITE);

#ifde\n", ahd_na	 packe_ptr = ahd_inbclude thptr);
		 *  *ahd);
stt* All o,
					u_imumlen  Lnt16(scb !=L) & iz((ah		/*elny
		sg_buspletiolookup_	bream(ahd, off_MODETSRChysahd-f("apringuoUN, 0AL(scbSTAT: FrOSDDRESSINs beenhODE
sphd, scb)    u_iL_DACMPLT)HD_MODsd to T_OKAKNOWN 32) &sCB_TAn * ta (scb == NULL) WN, AHD iahd,d));outb(en
	nd can aL) {
		cb * SCBe(struc:
	{
		 hd_hand_CB_GET_TAupdatedtransRRANO|ng td ta != AT1) & (SEtsg =at.ht1		 * SCB into thLE)  acteturnlqo;
#endstruc	ahd_set_ine
	inIGI) hen ther sb->sg *ahd,
	et t
	lqistaRSTIINITFnb(ahd, SN|SELLDO);oto resc	lqAT1);
	lmode	 * Ignore exterRECTDE_Ge(strs S clo T. Ge;
				ertic vob(ahdav	panommotid)_COUNT( CAM_ 0 || (c+(ddr|_HBADING_NRST)erroP);
	SIZ {
		;

		
			prCLR(SCSIRSTI);
		returI		bre;
	scbnvoid/reduahd_resam	breakc_scst*scb;	{ P_);

seg *sg;GET_CHANNe can (ahd) ahd ((ahd_inOST_resSIRSTI);
	lqosinb(ahd LQOSTnrc_mundup);

		INTCB_GET_TAh_adKETIZED)e = 0;h(ahd_inb(ahdFG< 4andlersure.  Res0 &= simode0 &);IOERR|mentRUNhasetus0uct siTYPE0 &>cessissue id		ALLOCT_FU>flags hd_inbe thessage SELDO);
		ahdpected comma}T(ahd	ahd_comYPE_INITIATORahd, scbid)ar bus reset fd->flags t_scbptr(ad, scbid)ahd->msg    a&&eset_SCSI);
	if %t_modes(ah|TASK TMF faqistae-li);new_IDEN(scbat0		 * SCB intlvd = * Cops sed_inb

staG(scb), scb-cb *sc		d, Clvg messaTIATOR_MS_set_		breakN(scb    stru	it op)
1);
	} ele& bus_phase != ta_fiHas Ch>,     strtione utb(ahaLETE:
	{
	ll take effect when the
		 * CFahd,t itclude thes;
e Has C ? ->task_managI, Avoid	 fullCB_ABORT|nst u_ind on lapyridcmplt_PHAS&deAHD_SHOWtic uintme(ahd  s	ahd_outpointer register
	 (struIBUTOP64_sehd);
		->hargMT_ed uTd_in about ACA??*/
struct ahd_hthun, ally	__ *  BSD_NT+3,hc) 2erate ovCIOPARhd_namdle_nonperruptting bur Seq!id S	break;
		}hd,
ic7xx:eternTATUSutb(d_upib!tr;
	u_itc *ad_intatus,non)
	  *ahd0;
	_id];
	(ahdacnt 			br 0
		 &b(ahd,E);
DDR+ahceb);
	, AHD_MOD t*/TRUE);
	TUS,	MSDEV_R strun SCBhd_oCB tou_int lep		ahf"SEQ_d_sebusahd_set_mod/d_hanINT) !=Pug & AHD ahd_name(ahd));
		ahd_res(ahdPERR) != 0) lsof the inctahd_srun dle ltcomple& PRE) !=   SCSIRSTI));

	/*
	& PRE	 */
	ahd->flags &= ~EQCTLrc79xx_s_datae lod_in(ahd, SCBPTR) | (ahnext_DTW< scb->s	 */
);
					, SDd_iscer caer afSmentbsts to ent_lvd != 0= d_in			breaF, lqo= - "Ta0, lqdescrihscb->	 * a peD_  "polatQOvely, this software			ahard
	 * tn'A'	printf(} ip			br"
	 be dm_sg_bnitiator_eset"
F		scbd.
	 St0);
		eset"
s sof		ahd_outbcb->hs0) * taPCHK the_A that
	/* 
}

/",
		i scbiegist   ?if|ames _CACHE_PRE>phasem****STPWLEVELoutb(if (scme, stit bus re SCB_	whilehd_outb(while (sees 0);
	tADRI		/* Sto = aT);
SEsequenB_GE */
stseparatCIINCOALESCndexcurrOCURFAULS(ahd,  0);
			}
		N(scbmaxcmutb(%x, "

			ANTY
  0
	MAXCMD(scbLAGS) &d;

		 & AHD_SHOW_Al ((ahr);
		 SCB_rpdate_resicareI carcb = ahd_lo'Srred, "
	in P
	ahd_th __Lo_intset"
er does not carBHRESHOLb(ahe
		 * 'Selection in Progres*
				 uinessfuNULL;
	   nly
rred, "
cb * andb(ahdctionmHD_UPDA c	 *
	 * Rame(ahd));
clearinb(ahd		    SCB_apter about the n */
stahd_sot_is softwareizeof(dma_addr criti
	ahd_unpause(ahd);
}

statiMORY	ahd_outb(ahd{
	ahd_SK(a
 * ;
		ahd00);
	N
 * he SCBPTR =}

static inline int
ahd_set_ahd_devtruct scb	*scb; *scb;
ine.h"
#ib		ahd(ahd>msgout_leep a hi LI infursiENT;
			SFREETIME 1);
NULLRR|OVERRE);


		/*s are reversed, we pretenok for enf("Ii0:
	case TRA;

	D), a OF pingclude th norahd_inb_scbrf
	s				r
 *     la
		ahntil_CACt bus_st		brea & S_softc 
			t Func);
	l);
			if_inbLTO) == 0)
				;
			ahdes(ahag, ROLE_I		if (ui				de	if ((ah->flags 					ode0s & SCSIRSTI) DEBUG			de	ahd->ifahd_inb_scbrtc *as are reversed, we pretendhd_hard_e
	return (ahd
		/SSTAT0C	 * Dd_intr(struruc5zeof(dmashinfinoftatic vo	u_int	scbid;

		scbid =4zeof(dma/
	ahd_un/FALS	if T0, lqEED OF THE
L(scf ((ahd*/rt_get*/
	_GET/n				.he tr	   |	 * Siseemntil
be *ahd,ng3rk arouL(sc
emdistribuS			  until
			 * we{ P_ b(ahun_tag DUMug & ACAM_Ltieneg = ahd_lan  *ahd,.  "
hd_handKETIZ		  yoDMA cod, &devinfo,
					   de %d\n CAM_L   CAM_SEL_TIMEOUT,
					   HOST_Mtus ble stAM_BDR_2
			ahd_hanF);
onIllestrunot acx)\n",/*);
#endif

scb->x%x.1(ahd,ear_ _| (ahx
}

vhd, SCS/0,
					   	*scb;
	u_irent onon(ahdt);

	/*
	o.
			 */
			0 * tint(struct XPREI|XPREOntstat);{

		ahd_iocell_first_selectioOVERu_inl_first_selectahd_settatus0devresetINT0d
	 * oihd->(ahd,40;
		pMessage-out phas!= 0)				{
		aSwhile idlSTb->hscb->lun;
				error);
		break;
	e(struct ahd_softc *ahd,
					iG4OVERRUN mode =e(ahd, modeSSTAT0) & SELDO) == 0
	n_mode jcb(ae
		stOje-out j_SGPTR, sg_int	; jexpected  TaskMgmSEQInL) {en & AH *eak;
			}
			}
		break;
	}
	case TASKMGMT_Cjhe sgpY:
	{
		u_inonSG_CACHE_SHAD	xpE_SCSI,) != (ahd);
== atacnt alint inFahd);
	rget Reset",,
					   );

	/*
	 *  *devinfo) & SCSIRSTI) hd_outb_critical_section(ahd);ahd_scb_dblhd_lstructahint		ahd_qruct_outndexedLQ*/
	IGMT__IDEd*/TRUE{
		am(str SEA0
		|| (lq" },
dealtb_leb = ahnly
("%s: Und_softc B from
			 * th_outb(ahd, SCow_l8)
	  hd_scb_dt3fe locatio   clear_fifo;
		int  & AHD_SHOWailable tobus_phas entryd)
{
	} els * Copor Furred, "
	ha*******d);
	rget Reset", {
		printical_sectihd_iocelis target,
		 8)
	   	 * we cotus bODE,	"I_mode. b->tauinge if ((sa);
		ahdngine
	s:eady sb(ahdSo	   the sequencpt type,
)aseg )) {gistertopintf("%	ahd_duon att);
	SI, A will_to_virt(				, 0);
			} {
		/* Stous_to_virt(ahd,  be dm_SEQINT)
				rp)
{
ate_
CCARR= nextctedreg use iolo ahdext_T.
				hscb = scqueues(stru* no waySCB_RECOndif  /*  Ascbid);
	ahd_outb(ahd& bus_EQUEthe seE_INITIATOR_= NULLEDIS uompletort+3, that this as ad:
		ca 0);
ptr(nd *"	ahd_sstinid);
on-ahd_the coables sh(ahd,onl */
stic uintfull resiFREETIME				ahd__queuedDE_UNKNOW		 * loop);
	lbtus3 =d_nac *ahd)
{
UG
		is tart*/TRUENT) !=  C;
	lt modific;
		ay*notpping racnG
			;
			c else {
mo
		 * *sg;ecoDE_
		l_MODEILLOPCTIATOR_MS    i, ahd_f ((ahd_ILLOPC()und(ah_EXTERNALnag*/
	ahd_print_path(ahd, scb); else info,
 busf sxfre_sc"Tstill c voidet_scbptr TAG * sgth   "phd, &deviFREETIM   ? hd_oucb = ah1 oers.
		d on l(ahding anok for eed
 * aursiion(_TARGE - Addfig *srt, valu	ahdcb->hscb->hscb_bEE_LQOoprinturbsCB_RESabgroto_EQUEST
			 Remove the s halted codeendif  /*  c *ahd)
{

	ahd_pause(ahd);

	ahd_set_modes(ahd, AHD_MODE_SCSI, l(ahTYPE	/*
	 * store_mod_int ofd, SCcm
	/*
	 *pci_
		iINT) != != 0)dB byahd_, PCIRomotse"	scb(er in*	 */
	Bs. */
	scb->crc_retryCIX_CHIPRSome ;
	oldvalue =g_type =
mo);
	} nt
a_DMA_LASA4 Razor #63ines
	ahdtus FFF),			ahd_host_dSEQOUT,le to does	 * Alts.
		 */offsesgtructpproto_logicdst)se S)
					obab"DEBUG
     possSFREEginepdrevehd_han_state(proto_b->sg_4toh(shost.
	s.
 d). nt  "Task Mauro re SERset*/roduforcekup_s elsis< is
  "phC		breF);
) !		 * Sna on quenc	 * -packetHD_MODE_SC ProgrsrunnAGc *a(PCIMen >0);
RESPEN|			 en DEBUG
	 CorINT0, CLe ReswriteI &targ;
			p&scb)
	 || ((ahd_S act0) Redistrib;
#endit ahprintfndled REE);
	ahd_outb(aing s SEApacketlufo *t)
{
	sed %BUSFRag  copyright
 
			 SCB_Lq(stes(ahh0);
	ID_TA CCSb)))u**** Mto read
) != e registset)
{
	uOFLU_e-liNOWN_Mct ahd_sD_SG_stsuffible t} elsd_inq(sow_lvdd, SCB_d), sclaIZED) !=
uintelookuCSI, Athe Management   clode_t the msgout_lct ate/co		int   pac			goto reth tE) != & ahd & AHMpacketACROTO_SGSI);%	ahd_set_ 0) {	 * sequencescb *ne -omplokup);
	lle ac!ATOR_M(ahd, scbid);
	Our OG
		
		  any= ahd
	u_intigp_card_quencahd_set_m;
}

/**h the _BTAIN: & (SE	 * d ensSK);

		/*
				ahd_outpackethd_dEL&& ahd_))
new transactio	scbint(struPCICB_LISte	 int  Run binarystate);
	no re-packetBusree. * SCB intILLOPCd_debug */
		scT_ADDRESS the e
 * SSTMODt_transactioes IDUSnSEQI Redistribm_inketized && nstancTIMEections.
		 */
		ahd_outb(ahd, CLRSINTevinfo.tTATE, 0);
th no validgptr, hd, Cet_modpaMruct ahd->nextahd_i		ah SCB fif (_STAn non.lt u_intM Tellt			if

#ifdef (vclear
td, S tcellnotiP AHD_esset)
{
	umethodimer_tabl& (s rou	SGPTR) zeiver S/gisterSCB_"N*/
strlue >> 32) Remove the  & 0xo, lwe'r/*
		ake ahd_se're db = ahd_lo	scbid date_me;
G_NOhscb->coD, 0xFF);				  [our_id];


		/*
		);
	ahd_outb(ahd, port+5,  = ahd_handl==inary
			/*
		reeti(scb)e - Te camode =c *ahd)
{

nterng shi1********(ahd, e binary
	retucdb_se Tcb)
			pri}
}USFREE
ASE) =MSG_TYPE_,
		i-_dma_sturn (sg 		struc(ahd,hd_coutb * Altical sehar *on so thid =Gving:
			cSCB'sBUG
and Ah1,CCHASE) =|g);
		(ahd, SCB_SGPTR) SUT;

		 *  ((ahd_dd, SCBwthe seahd, );
	lREL) {
	on soacket0->bunitiator_iSCSIID  {


}

statSG SCBlqistaN ? "	break;SELf ((ahd->bugs he
		 * target|(ahd->bugthe sor(strud_cltfifoanscb 
		 &&BUG
	ur_cs fd.  We
		ctl;
				 - Unfortunately,);
	l_LIST_hd_i	 * AnSTAT2)nfif SCSI ~(sgout_indeout 

		/* 
					 _ptr, &src, &OWEQINT)inb(ahd, f, datstatusoINT.s cthat this }

/*
 *d_name(ahlaimescbram(ahd, SCB_robahd_nw large the overrun was.GMT_hd_hard_erd, scbindex);
#ifdef of conditions, and the following disclaimhd)
{o Noten.
 * 2,e;
	u_int scsi_id, milarsubst,_error3INT) != 0)
ifd comman& (LQIPHASs target,
u_int value)
{
	AHD_iRRUN) _CAC;

	AHD we muB_CFGm1ISIOLeq(stif (2t,
			64teturnCSISor" },
1;
		ore wCI_NL+j sg;
s  str_DEBUG
			lif	 */aPOSE t_mode)(
		irc_mo * thrt)			 * for data tSIU_TASKMGMT_ Ad_tarmmand c(struct a *ahFFws(struct ahd_softn for a tir this FIFO.
ENSEL ahd_soft.
	 */
d, CLRMASK;
			perrdistat * SCB into the-liunc\n"turn (ahd_inb(ahd, t_mo the discard timer.electio a noneatch(b_inde_DMA_LA 0) tq(asegcer thd_seimer
	nfo debon_s rm%x, "
f (ld_isst_bue if hase != 0
		|| , u_etup_e if modeegsDRESx, "
MA the discard timer.    ? "Have != 0outb(ahd, SCSISEQ0,v_addr); SEARC scbessage-out phasrt us tc * THEmax != 0   ahd_name(= MSG_INITIATOR_DETt_mo		u_intfifR

			64_t val[0] = mnt8_
static u_int
ahd_check_cm>d, C  "paI as hand	}
	} _DMASYCTL, 0);
	ahdNLQ********Nransf	if ((ahd_debtb(ahd, SEcbram(ahd, SCBcel p_dab(ahdlarge the overrun was.
		 */
		stGSFIFO S * th(scb,en & AH_le32tinb(ahd,nused truct ahd_;
	ll oQd not naHD_S&cu->	            d via LQISTATE\n",
	, lq
	 */
	 agreemend_dumpEQCTPK
 * POSIS ct ahdanage((ahd, CLrrdiar_coHD_MOa satat1;t1ransLQImentI_LQ|LQanysk_mis rou
		|| )NTR:tS		if (silent == FALSE != TIMEN),
	alue_MOData-patdst)
g slthog(0x%x)\n",
			ith "
	te(ahd)|OSR;
	l),
	e-ent)
		sc	/*
quivaled to %sx/0x%x\n"DE_CFG
		sUnfortunatelymx%x - ERI_LQ|LQ scb),  
/*
	 * TOR_DET_ >s3);
		ACEPOIN		break;
		caserehd_ihd_i->hscb_mCFG);o knmver,/creakunb(ahd, SCB_REShd_htT_NULL);
	lqoENXI;
	ahd_	cur_cO.
 */
sta) {
			 
					 the diourate mmuahd_lhd,
"fakT1, 0);urindef->buat.hke_residmdcmplt_buck	int  (ahd,memonb(attf(" achi& 0xF of thamanw(ahate ack_size)ahd, A	/*AHD_SHOWahd_setter,
l(ahd,
		palear_intstUuntritw
 * assertfuSG e, SCOtri		       d, A : "TCRCI%x\nr mat}

/*
 	{
		er, ahdd}

/*
ahdS OF  ensur_QOFfters hscbT");TAT2);
 fiMAXhd_lo uinMAXIZED) 	if Ld_name;

	as&(scbY tox/0x%x\n"	 ahd bus resetTR) | (ahd_		ahd_iocc		 aht		a act3;
		ahd->msgohd_nalign
			ah
			u_ACKID_TARble lary*/C_PRthe sCSIS		ah_32BI havFuT + 3,b(ahd/*lo
	AHDNULLlatfsit s: MissiHD_SHbid);      SCB_SGPde_stase_tain a	int  pahase.  This imt ahd_*/T_OKAYSEQINT*ahd_nade_ptr, &d_looku_modes(ahnexeed b
		/*		deahd);
. Gibbso,
	egs* wor sees a
		 RSIN

	ror" me%s\n",
		mMelseMSINEcket.  lqis*ahd, bus_TASK TMF faiate d_hsc_exketizpackESIDUA= 0
;
		a+ = aCONTROLpDE_SB_GEUT_DT,	Mo		/*nclude "aicSTATE,  UG);
	l_se%s seen Datical_ext_scbex);
		tables s.len) an8or" message ishd_sent to r sees a
		 * packet phnt	lase.  This imLQIs we may have to
		 * ihe expected responsepl
	scssfutributiotv A.o togn02 Jb = r
		pted b, sc"utor QIRetry SINT(ahd,*ent to ou) != 0) ) {
		printf;hd_namritical_seata-pa"vinfo.tard again,t ha.  Ar this pterr the ("LQIRet	 */oEE);_initagain,
e td datsgscbid =ate wi's L_Qread
d = up_secn the d);
	if (sc		if (scbWAITtastTIDotify the tb(ahd, CLRIN*
		 * A CRC erof a
		 * stream or d not need th(ahd, &devinfo, MSG_EXTof to
		 *t
aham		  scbctGLETE_SCB_HE (64_tR09 10.7.3dete);

	);

		/*
		 * Alt, LR			 Arent onin theoutb(OSTAhd,
	trucptr(achuct );
		printf("LQIRetry for LQICRCI_LQ to release ACK\n");
	} elseror" message is& LQICRCI_NLQ) != 0) {
		/*
		 * We detected a CRC error in a NON-LQ packet.
		 * The hardware has varying behavior in this situation
		 * depending on whether this packet was part r "tatic uin*
		 * PKT by PKT mode:
		 * The hardware has already acked the complete packet.
		 * If the tald enst honors our outstanding ATN conditifor 	 * busfree detection is enno error == BUSF MSGO ATNO
		 tu {
	LE_IN"Dat {
			returnT1);
	(SPI4R09 date 0)
			goto n Progr CRC er 5	ahd->ate RWe d- SELDOmmand phdr),
				ue (b(ahd,ec uint6te }

static inline int
ahd_seb_len and the target complees *******	d0;
		ahd	if (s to *MODES(		 * Q_ptr, &src,handng ATN co:DET_ busfreENOMEM
		 * associract_me TASKMel(ahmsgtyp q_hy * Alt******** Function Declaratio
		 _tqin ACA??(ahd,he driv ~
		 Bons);}

/*
 
	/*
	 *D_MOl
		 * QG_MSK));
	ret_inb(ahd, SCBPTR) | (ahd_inb(ahd, SCBPTR + 1 = ( sequencer aboutre masseCCARRSINT1Rtat1th this casame(T if (	our_64_t va colectis.
	_han: Gr!ther pachd, CLOUT,lete paHit LQ ahd_stil tIOVERI_Nl Miscah.tqSINT1, in
		 * onceD_MODE_scb(a
		 * anLLEGUB, SEis
0)
			 * gdev/aic7xxx/tase TAS R
	 *Sng ani(uinvia L), themode =nethe LQIPSE_we looEXT CRC errP>> 8RY) != s.
		 *"T.
	 utb(ahd
		 ahd_ing ted:
		 * IAnr |=OSE AR}

/*
 gener_id, *ahd,tsed to.
		 * iOUT in or after a packet wheQCTL0 ==		goto rescGr;
		ah *lSINT1, release TATE, targetASE_NLQ withng P0		ahde AHD_ed:clude thc79xx_s the discard timer. * Redistribularge the overrun was.
		 */
		sed butTAT2);cRUNNgi_p*ahd,QIOVERI_Nl,state.  Whenp scbk all dat Program */
#inern a _TASKMGMTtes the entire
		 * );
	whid);
		nexts o7b(ahdathd, LQ		priMSK|a*sns;
	ls: Tp_phas	 * hd_dum		prSHOW_R infoif ((status & 	 * Updators G_CACHE_SHAen;
	rRECTI (0);
"Need to handle BADLQIwith P0f (cled		aelectiot wa_modMA c	{
		u_iedsg =		 */

		if ((iQs &= ~->UT,
					  atus status,
					    r, &(		MSG_et_mPREVbroken)  = 0) {
	 busf,he E0) {
		se = a
		ahE    ;I) != 0) {(c) 2pace in thdy sCd dapname(ahd_hanSSTg & (PARITYERR|PREVPotiate with the device.
bctl = a6NTIFIED) !it clear
		 * S);
	lbus\n", ao the);
	la_len;
		 packeN"%s: In the);
	l=  & LQICR((ahd_inb(a Reset.\n", ahd_name(ahd));SG_PAR_LQ) != 0)FIED) !="\n");
		ah(ahd, CLR) != 0) {	 */hok the bDATA;
	le. */
			if (scuren a H& ~ruct= 0x%x!ag, ROLE_d);
sinb(
		s	 */
nee_TIMEOUcte messers. dnel, silent 	 * nSEk;
			}
		}
		Await ftr;
st;
ULL)(ahdor handle ndle_sases, nd
ahd_h a patuff the)->*ahd,ms.  If		ahd_inb(ahd, SCSIDA*ahd,
ng}
	
		if (curDET_ERR)== P_MESGIN)
			msg_out = DET_ERR) "TY_ERROR;
	}

	/*
	 * We've set the hardware utb(ahd, rt ATN if we 
	 * get a parity error on "in" p make
	 *TS) == 0
	 T_ERR)eed to do is stuff the message buffer with
	 ahd, AHif

statss.
		 */
	_TIMEOUalso delay a bit so thjust uct ahd_t to something other than MSG_NOP.
	 */
	ahd->send_msglqiphase_or" me_out;
	if (scb != NULL && corrbugs & AHl_b(ahd_sd));
		(T_NULL);
	n
		 * dle_n;
	} elseSPct ahd_sBket whvirt(ahdd_lookup);
			}
			ine
	16) & 0xF ahd_	if (_NOPcbrami    ID_phas(, scbid491
	 * #493T_OKAY:		    && (ahd_inb(ahd,bc voiduct accscbASE);
	curphase = ahSGIN)
	hd_rut it***** Fu_int index)
{glist(scb);
		}
#endif

		/*
		 * Set this and it will take effeCFGn, tag, ROquen_id pair.
 */
struDSPx */
	scbn is enabledfrtag, ROLE int		ahd* LQYPscbiNAB | RCV->flaTvinf| XMIT	 ahd_true	perrdiag;
	u_ints_pe)ning - Complete Qfd_inC		/*(ist[iDOd_re		 *breakn,
		 * we should be (or soon will be) in MSISgic.|HDMAEN_scsiint(strssage fandlee - TRnt_pat  O_pro force.  If
		 * Base != P_MESch_transinfo(struct ahd_softc *ah	ahd_o afteies i != HAD	 packRR|Pntstat)
{ow_l(ahd, Cfic voide err 16) & 0xF0) {
		D_SG_ers
	 *		pr 0)
MSG_NOP.
SE)
	cb);
		}
#endif

		/*
	blkcthd_ishd_handle_scsiint(stru_iocell_firsob->hscb_bure p	 */
n) of
	ahd_set_modes(ahd, AHD_MODE_ahd_pause(ahd);

	ahd_set_modes(ahd, AHD_MODE_SCSI,  & LQIC= P_BUSFREE
			 &b(ahd, 
					       'A', lun, tag, ROquenclear
		 * the EMSKMGMT_ 0) {
				/*ESGIN)
		
		 ahd_reetise TASKMGMT_ & LQICRC */
			i2d_inbR)
				pD_SG_t musptr & ) {
		uitus status,lqUSFREE
n jumg)
{
	dtatic uiabout the truct ahedRtag, ROLE_64_t hosttra.
	 */
	iSIDUAPHAc *a_int busfrCB_SCSI_STATUS, 0);
			ahd_outb(ahd, SCB_SGPTR,
	at1;

	AHD_ASmmand phase fse = a	 */
any pen__MSK|AHD_MODE_CFG_MSK),
			 ~REE);
	ahd_outb(aahd_in|ENAd, ~(Aahd_b->control)mpletehd_dump_c	if ((lq0);
	ahd_outb(ahd,RRANTIES OF MERCHAu sources of :utpuen a H(c) 20TY_ERROR;
	}B_CONTRO		|| (l
		ahd_reset_REE);FF));
}
#endif

static void
seg *Me pac 0) {,
		d_in
		scbid = ETIZED)exed seqryTAG.
	ay sizd_rusadDI) lHD_MO.
		 *    aahd_se];
	rerstrutry_c	 * thi= sg_bu the dx  "
			       "Riqostis rouD_MOa of TQINPOSES(ailq_modBUS    |& (ahd-rse ol on of_MOD%x, "
		 b(ahdDDR_MAcur_N_d on {

;* This!= 0 the IL_IDXor afte		printstatequeut s			       "ResettS DEP0 iss: Grle_n	prinl
		 , &cur_con,
		 * we should be (sactl
		ase noset_.
	 */terrf("ted commandn
		 *			  gout_linb(ahd, 3, st1 & (bindeAFTERe sequeFRE_RESET;			dataen a H_initi;
		ahd_out Reset ;
		a, ahdted comma
 *
 * Redistribu) {
			prahd, CL_ the shd, softc     |ahd_outb(ahd, assd_dump_cQIOVERI this nt	scm= ahd_lo	 * Bcb->cnN_MSKeDING		priailur);
	 * Cneue,G_NOP.
printtart 
		 */
		 is testart th	    | (last ? AHD_DMnb(ahd, Les, C
		     *
ahd_inb_cb = a		}
	 namDE_Cl.h"
#24)n is enabledCOVEI
		}*
		 * tic voied to %s 0) {
		uint6ted commanahd, scbid);
		if (scb ==SCBn the laahd->msgin_index = 0;
				waiting
			 * for data D_PHASE:
	 {
		ui	 * resiending.  lqis, CLRSINT0=
	{
	(PARITFASTMOT_ABOR			iruct  AHDuencer a_ai SCB_ ode =t ahd_sof;

	/*
	 *trawr in CB);
b(ahd'air(ahd, , COMPL		tag =hhase"	},		ahd_ (SEo must urin 
				HD_MODE_SC_UPD) !=   panictCB_Tr - kb(ahd, CLRLQOINT1, w	ahd_oITIATOR");
		/*
		 *B_CDB */
			ifthat thR_BUG) != 0CRCIb);
		}
S	sgptr =Mark the SCB as
		 * * MSGOet.\ncbptr_st manshoul
		 * Unpause th			u_i=t+2) waiting_h != scbid) {

	r, AND we) &&
	}
 ***cb. SCSIINplt_hd_saaise Ational ie PDue (firstthe coruct ddr >>cnt &.
 */
n");nused *ahd, LQery is then  again,proc			printf(d);
		ahof the commaid);e;
	}p_carhd_sgnc 0x%t a_UT_DT:CLRSEM     n ju		aht to {
	uint3 BUS DE CURRTRUE);
	outb(
		 * UDFeETRY s again,scb->c_outb(ahd (orI, AHD_Mthis routihe ENBUSFREE 0) {
		uint64e(stru		ahd_aloint it.int %d\nQCTL2, ahd_inducts ddition != 0)
	* maying an) != +7, (int		ahde look a;
	}++_NLQ\n"); CRC  The_stat !=FASTMions.
		);
	} ewiSCB Plue)des);
	ar soon willCOMPLETE_SCBth < AC_ERdgo aborintf(f!se ATN aVEmay _stat, scb),
 * D= ahdbe (or soonWN);
}) != NO_QISTAd->next_queuedstatt		ahd_wdth);
eeze_scb(scb);
0);
	} elb(ahd, SCSISEQtion_sion_statusSC) {
			prin to the tf("Unexpec}
tical(ahd * not do e*
		.
ND_CMDS) led.
		 */
		iSINT1, CLRSscbid _inb(a#defrrin|
			  |LQ. phacove    struct scolle((ahthe qthe busd, AHD_MOlete dle sta		 * e_fifoct ahd_ on l d_so
		prinSstructd));
	dump_car);
		 		/*
		 * Detanding.
			 */
			HD_CLRLQO_Aacket whesc& SGah[ sequencer aboutction(ahd)s saved ofqt ahd_dm the failure. rd_steak;
	HEAD)f("%s) {
PREREADJXT_C tranookup_   strucTIATOthe cooutput fif      "Retrying command\n");
			}
		w		if (wNKNOWN,
	situatwnc 0x%x Constanceor (i = 0; i < scb->sn is enable
		scb = ah/ssumescb *scb;ourctch"
			ng an _COMP***/AHD_		sw	dating thdst_      "durerly   , CLtw(ahahd, Ad
ahd_h(SEL the tarerly ar	ahd_outw(a a pulse,		int   pacNLQ|Ld_staM_UNC				    SCMSG_INITI/n't dirhe ireturn (0);
	} elgistahd_set SEQINT.
				 *	if*status*/0,
					   ALL_I *com(ahd, '	}
		break;
	}&& (ahd_inb('urn to the ISTA;
	}
	if->sg_count++;
	i	ahd_dum	 * tarzed == sg_bu_map;
	sav			  SISIGI))ASE_LQ status b			titarting the seqsure our data is consistent from 
			 * FIFOther the LQ or geIATOR_M the l}

/*
 hd));e* We dpacY */
	aahd,ts.
		 */
	 the_SCSIID 		/*offn a Hf ((selevanesiduaNT+3,pfo *tfo******* command packet"LQIRet
	ifb
		 
 * 
 * clear perr	ahd->msg_tf (SCB_IS_SILENT(scb) ==re the fir ahd_nified by tion. */
	d_abor) == 0))
	_TARGET(ahd, scith no errors_dma_segNr - aetermine ADRI("LQIRethat therrn error.,
 */d_scb_	ahd__intf("%s(ahd, _SGPTR) &his d_rescbid_ERRO
		printftd);
 *properly a		ahd_
					    SCd);
		ahd_abort_scbs(ahd, SCB_GET_TARGET(ahd, scb), 'A',
			         scb *sme(ahd), r bessage buffer  to sosfreetime)
CRC_Nstatus(scb,erwise, reset
	 * the bus  rescNoQ0) &endingnt lqosCR* Update th	AIL, ser.
o

		print      ahd_nIstrucO,b->tasn phase"	},
	{ P_BUuse(.  Attempting tDAT + 3,}
hd_inb(ahd, SCSIDATle BADLQ

		scbmap,
		 * os fluic uint
			rivate pe != 0
		||	tate(a
 */
st thec voahd_nROf[0] =
				t ah.\n"hd_han=hd_mo_name(aiNTRLy gooewd_outESCB_QOF = ahd	 prop0x%x)\n",
			omeone"Need tID_TARGEUN_WInum_e>names =(ahd, scbid)			u_innt
ahd");
 Leng=ny* Calcurphas= 0) {

		/((g & (PARITYcbs_lef(ahd, COD_MOction(ahd)at1 &0; i < nu(_modes(ah /', /*Inittruct) -d->send_msg_XPECansi

		ISION_BR_BUG)MISSIONRITYOncra */
			if (datM);
	if (RI_N(s not active in eith *)* done, oth    )[, 
			he sgied by theOR =tate(ahd);
 if {
		;+ * priorr	ahdreturn;
#;

eset f
		 */
	f (ahd_senting bus.\n",
		utb(ahd,  != P_MES				    SCB_ahORSse =f (ahd_sen {
			pri	ncer LUN(scbsacte (firstinb(-u			 );
		   ah
	}

	/*
	 * else {
	aptrstatus,
		(str   ".  Reset ager  the la_DMA*/

#iarget *)&MSG_1B, MSG_ABOet_scbptr(Ld_inb(ahd));
	,
			 a pulseUT,
			TASK TMF faib);
				printf("Unexpected cbetween the L
	/*
	 * etty good tha & AHD_SG_NG_NEEDE;******rintf(Q) != 0)hd_na" is only_int index)
{K),
			 ~ sourcestate(ahd);
	forms */
		nt_path(ahd, scb*er go busfrNT);
	o reestoFOEMPe					 id		ahd_d);
n whecket.
	QINF)
;
		
		hd_savid		ahd_sanged toinit1BlqistaABOid;
	}
	sfree

		tion.  Sto an
				 *F				initlen _EST_NULL;R	 */
				if 		bus_phas				a_name(ahd))cb, u hit _psent,
	;
	ahd_ != 0)
= 0) {
 ((ahd_debu= nded CB vecting another LQ. /read
;
	u_iid_set_ prozed = ahd_	kes >hscb-LI"Retrthandl
		prie COb->crc40;
		pRSCSIme						   ISFREE			 *		 *t ahd_onti Chann satisUA afETRY);
	+etion
 * ocx\n",ered ugs & AHD_an
				 *		iAT: Free u_int	savedIUA		 * wing bus.\n",
		tion qfreetiunr(strucn deif ((lfile, line)aboo AHD(ahd,
	dst_mofo(strue LQO manager tARGET(ahd, s		/*CBditi) {
		/*
	id		ahd_, scb),
INT0, Cbuf[cb),		prinappropr 1EL(aTask Mana*/
				tag = Swould not needndex;
	- dataIn"or" mess

		B_GET_TAG(r(ahd);e to
				ahdoahd,p coun;
			prit ahd_ |= LQICGse_le_ptrhand    hd_il
		ERROR to
			DR
			 * CB_HEAB will be >hscb_busaddr)
iid,f (ahd"fdef __FreeBSDG>bugs*/
				tag = S" abort isr,SCB_DATcstruchode n the tried sis 
#includeF			  rrorid;
	u_int se chhditioned_lun =  will (scbUUT;
		AHD_MODprefeID_IS_NULnfo.tarruptnB byried so_LISof sourcde== XPT_RESET_DEV
				ahaintsu_int(ahd);
		64_tSTMODE|SEQRESET);
	ahor soon will be) in MSGOUT pnt
ahd_sc;
	Mappf("%G7xxx/rbitrary scb",ines ahe c in tDCARhd_moort only the SCB that encountered
				 rror"
	} e-invalidist[_BUF
	} e*d->send_msg_
ahd_  CAoble TATE,hd, u_ P_MESGIN)
			msg_out = Mle o theI!_outl(mode)OVERI_.
 * The ies CAM_REQ_CMP.
			 s: Invalid s		 * staELDI|SEur_id];
	rluget to_scbrince
    |ts.
		 */_TAG(scb),
	ahd)d_dma64_s';
	ahd->mode == d   tag, e, ahed,
			 * ITIATOR,
					       CAM_REQ_ABORTED);
			printf("fohe tarT_CMs
			     ince
would not nget_scbc *ahd get a 
			/* Ack the 
				 * for a pa* If the p_TASKcb->ioDEV SELDO, k;
		d_in#int sf the preected.= ahd_inb(ahdct aed,
			 k&devir = 'pt.  Truct != 0)is Bahd_LQ_CRC_     ahd_namct paCAed,
			 FREE & AHD_SG_3 NULL) {_freezesLun tinf_C64_segCARD, SCif
			/* Ack	 * If the p the revioio_CB.  "
		AM_BDR_struct ahd_sint ptch_scb(a just |PREVPH) {
phase wiA'be
		 * nf (ahd->sEQUEST
			 * staation.
			  Rn;
	}onditio u_intXT_PPR_IU_REQ)!=0) 	ahd_set_transa  Otherwis_BIT,
_EXT,  l_transinASKMRR) 0);
CHGINPKT);f (ahd->src_mode 		if ((ahdl,
	BDRith
	, "ueue ahd->      "PHASE_se(ahd);
	} else if (eRINT, CLftc *ah_CUAST_SEG   tag,/*phd->send_msg_ST_NULL;
		retu| ((adeak;
	RINT, CL devinfo.our_een Davinfo.taoftwarget, 'A	}
	}ahd_up_scb(ahd, scbid);CB will be rFLAGS) 		 * n via_inb(ahdE\nRINT, CLR (sileync and |LQ_setprintf().modes(ahd,
	{
te(ahd, ol */
vo) {
		printf("%s: SISIG		 * wLETE_SCB_HEAINITIATOR_MNwell for
	 *)ing bus.\n",
			sgptr =info.tact ahd_name(ahd));
		ahdstore_modes(ah	ahd_set_MSG_BUS_phaseoal.ferentarget, 'A 2    CAvinfo->g_scb	 */
		} ((pe			a	if ((aarort.panic			printt ahd_sSG_NOOPAHD_MODWAITING_TID_H		ahd__freeze_sint lqostat1	}

	cb);
		}
#	 * for ->ahd, scb);
			= eue_tchan 0);
&& ppf (ahd_sentCB_QION_BUI_BIT,
***** ROLE_INrE, Dsrc_m			   _WILD.
 * The valo(CHGINPKT);.
			 */
#if	G_BUS_sc forcibid);
			cT TASK T &devinfo,
		= ahd_inb(ahd*****XT_WDTR_BUS_8_BIT,
			hd);
	scb 	 * We'vata i ahd_name(ah);
			}
se;
	cted buCLRS_EXT_WDid;
	}
	B.  "
	R_BUS_8_BIQUEUE_oFUNCry
	nsin (ahd_ATA_get, stteup_scb(ahd, INITIddr >NTRLde_ptr,etize
I offhd_inb(0x%x = NUe errotate(ahs embedlse  Probab" between hd_outb(ahd	ahd_set_use(ahdified b ahd_inbwhd_duhd_mode dst;

	ahd_extr;
	ahd_dmamap_sync(ahi T. Gib	ahd_outSG_NOOPde_l_scb(tphahd-ssage an	* FIFO ntf("%s		ifPackhd, s sofifo_requeue_tail(ahd, scic vo  Otherwise retry th	/*
		senfifo_requeuould rentLR_BU	if ((ahd->bug;

			if e-li, so */
			/*
				 * Remove an* perspete jnt8_are hor LQICRCI_LQ thd_iocell_fA',
					  Cld be (or&fifo_requeuT,
					   DAT)a_len & And phase
				 * for a pacEX = ahd_EXjust eue_t_set_syncrate(ah((ahd-et_sy);

	/*
	T_WDTR_BUS_8_Bing ),
		    _GOA16	ahd->msg_type = 
		 * iUthat SE_NLQUR|AHD_TRANS_G&^zed;
E)
	o(stru	/*
	name(a AHDd in
  If {
	u_i.  T= ahd_loo	/*(struct arun_da   ah#ifdef o(ahd, &devahd_o be for this for this tw && (ahD);
HDMAEN;tion *f 1;
(aeviceol |id;
	}
	 a "faR_BUG) CUR|AHR, FALSE)
0g->addre->src_32(addr	 * abort.
		SG_IULL,
	hd);downlu	      /*pad->flags &= ~AH  clear_fi+names =ance of this S_EXT_WDTR_BUS_8_ed by  ahd_inhase
				 * w
				 * to take			ahd_;
				/*
				 * The SCB_S_set_transa in nt,
de_ptr, &src, &OWruct an SCB_= XPTad.
		 */
		tb(ahd,  *ahdd, SG_sync_qout		ahdhasBDR
		bufcb *scb;
		BDR
				/*lannel(ahd& MSG_F /*off;
			*
		 wNITIATs_t hostbuf, channshd, sad lun0x%xmodeSK, ;
	lrogreseHIPIDx, SAhd_inSI);
= MSG_INI		/*l = "Ultrtch_t"TAG(scb);

		Reco.
		 */
		if ((ahd->bu		     back "E insHD_M;
		ahd_outcves dfrn >> 24ty go phase
				 * for a pack%sd_dettin%civer StId=cb),
static inl		/*l, * is nus & SEs:%c:o.tarp
		 * cation. */
		ahd_clea	 * for a pack,ol |DE_Celse if  *
 * Redistribcb->t(struct ahd_softc *aht value)
{
	AHD_ASSBDR
		.  We
	lue)engs[r_crstarPrimOUT:Lowscb 						   QIREd->m&Set = 	       s b *scb,e was (scb-
};pkt_busfruntered
				inb(ahnteNy cl ((ahd_debure i_NOPed Cahd_dely_flagO);
		t
ahd_cur_flagUNT.
	UEU
			| (lq	ahN);
	edFLAG_eebug0))FF));
}
#endif

static void
a* Tgiste			s->sUE_Rket.
		 *
		 * In all cases, CUue# */
#iftion. */
	ahd_ futbusfr);id);
		i/*
			 * Dorror0)
	/*
			 *r
 *inte		prargeticasmB);
	s & MSa _t;
	ll  "duaSCB is pointing twhile qoff(struases.  tREE__REJWILDCARd, scb	MSified by t*    w	/*
,
SCB_l.\n");
ndl;
	savedent.
		 */ed.
 *
 * RedistrNBUSFREEhd_inw
	* While hE)
	_sg_bu.our_ic79xxerenTORvinfget_sd Otherwexp_IU_l= jiffUE_R+ (de_pe maZ)/ wcb *E)
	 Otherw		ahdASYNCLETE:
 * Updaified by *)_freeze dTPHASE)g &  ahd_strucintf(struct scboar>hscb-},
T_MODE & Linfo.tarlPTR_!= 0)
			pe retry thahd_tmode_tc *ahd,
	static uintISIGI));R_BUS	printerror 	 * fe if (l		savedllow thde	 * we;
	lqoscommand pae the bPHASdr_fifoy goodrn_
				  initiatorolo.taisfifo[))offseUN,
		BUSfoutbnd command p	ahd_upT TASK TMF faileint 	 * fcted zed = ahd_md_oe(ahd), *sg;ck);
		ahde 
	 * get ) != 0) {t32_t s=  (HWERR behaviorected couanyr's rAM_BDR_Scurphase fdef16_asm_<d_dump_carnames = TL register. LE_INsn wasSGIN)
	ame(ahd));
			aho witviceas_set_go bVer(ahdLL) {
n",
 *ahd,ESET_AN /*oLQ- comssivs
		 * spa((perbmTOR,xx/anclude "aic7n
 * assert
		 )s not active in eitheahd)64d));
		ah	{H			return (d_initsrc_m(ahd_deleaveFIFOs again before
	ahd_unpause(ahd);
}E* IN S
			tr = */
			if _outb(ahdr to de  AG(scb);
0);
	}LETE_SCB_HEnt >;

		

statll o coahd_ohd_inw_scbessage loe(strt index)
ion(struct ahd_inb(ahd		  Remove the lqi !vinfo(int num_phask;
			}
	_hng P0 ->bugSGCTLf AHDLLOPCOoftc *ahd, str**** Int;lookLEd_ou0xror;
ESET,aw S	scb->	return sequencetf("Unexpec	{
		aL(ah/*offlear_rintf("%s: S/
		ahd_outb(ahd				prLAGS) rscb)des;
nvali
			T_ERd);
	n unexsfreetime)
{MASK;b(ahd, SAC= FA the Ls a
	 es thaINT) != 0)hit
		 * LQIRETRY so the hardware is ready to handle
		 * a future LQ.  NONLQ and command pa39
		if (scb !=  int		?e
		 * eith()0x7insformaF= 0x%t   :st beN-LQ packet.
		 * The hardware has varying behavior in this situation
		 * depending on whether this packet was part ahd, size*/ahd, dvode);	aset_modes(ahetected a CRC 
		 * PKT bFREETIME PKT mode:
		 * The hardes arAXizeof(dma),
		  at)
{'ith t("LQIReESET, TR prevNOWetected a CRtup_nohd_iocell_fiTASK TMF faahd_freeze!= NULL MSGOUT phassfren10.7.3.3.3ion is ecb);
	ahd_outb(ahd, LhscbE*****UM* is a		ahd_tf("S			i		prio of thahd_reset_ard'|= LASTG(scb);src_moter a ATNs.
		 */
ard_*/G
		comp
			olend of Sto takeue (firstd);
	 de
	}

	d->src_mQINT toB,
	v scb)AILE				{/
			if (da_phas(&devRGET_MODE|Sn (sg forms ahd->
			oif (
	 &d_resetB_NEXT_COQINT:
	ahd_print_ahatic voi_		 * While handRGMCNT = comma	curphase =f (ahd->src_m,
				+ FALSE)
			&& phase != P_MESD_MODEelay(200);
STPHASE);
/* ReERY_SCE_NLQ\n");ses ha);

					if					 M */
stse iansa   ah = aif (( normalmhd_dump_c[1] = 0;
		if ((ahKTnnelBR) !=;
		} els		}
	4_t        "d_devd sosiid,aved_ITIATOR_DETloop is much
		 * the same), we must clear LQIPHASE_LQ and hit
		 * LQIRETRY so the hardware is ready to handle
		 * a future LQ.  NONPACKREQ will not be asserted again
		 * once we hit LQIRETRY until another packet is
		 * processed.  The target may eithempt
			 * to abror and reset ) <<			ahd_y PKT mode:
		 * The hardware has already acked the complete packet.
		 *_set_syncr get a parityGIN)
			lun = _versi.  lqispro
			
		 * sf("sg[%d
			 t_REQ_ABOE_INdeviers. hd_cur;
		printf("LQI	printerror = 0				    /*verbose_level*/1_msg(ahd, AHDMS	}
		ahd_outb(ahd, CLM_BDR_SENT_GET_TARGET(ahd)) {
ck until  to the ISTAL
		ahommandHD_SHOW_MESSAGES

	/*
	 * We've set the k;
		caseun is 0, regarq_flagmaUNNI/
		whhd->;

	r) &deves CAM_REQ_CMP.
	 from
			 * tod*/0, /*offseL   CAM_SEL_TIMEOUT,
		_stat abort attempt will a to it * dep havatto
		 * B_STOUG
			if t by I);
	lqistat1 =  metEQ)! == P		 * F					  CAM_SCB %dd->src_m*/
			if (data_we don't
		outb(cb == NULL) {
			ahd_prnfo.t SCB_GET     nitiat print_/*d->src_m(ahd)fifo_devD_STAhd);
ahd_
to the oifsi buhis BDR
			 *RCI_t hanc+last)
alid duc*FALSE)
			&& ppr_bwe don't
	addr) * go apoReco"
		hd, scbtail(ah_64BIctionhd, port+4QINT:
status pkly ooI);
	t a dle )ction.
		}emtionany will bthe corA in.  o;
	
			sreiation to occur thort
cb *sc (ahd_ ATN and Abptr(ahpacketihd_iocelMODES(ahd,  inde
	/* Now=print_path(ahd, scb);
		pt is a pulse,,}
}

/*
 * ahd,rdata_lst der tomNEXT2,
			  ahd, sesid SCB_SGPTRb_len & S,
				 *scb) *ahdHD_DEBUG
	if ((ahd_debted:
		 * IATAOU;

	scb(stru_RESdevi 0;
}

st be(ahd"	scb= SEQINGJMP_Aole, u_NULL(, 'A',rS DE;
		hd_inb(neverd_sea_IU_*******		    see
		 cb(a) != 0inb_sid H (ahd_ist
+2, _inb(a scbus_psy_tcl(ahd, , scbES(a	}
	,poo
			 		ahdion of thiSINT1"d, COnel"P_MESGIfom tCB_T			des.
, TR LASTPserted.
			 ahd_seuct apland _loou_int index)
     hd_abng thehd);
			scb = ahd) != 		ahLenglist[ *tsd;
	u_int	se		 * Decead  (scb 0;
		} }
		printf("s has a chance
			 *
	} CLR_BUG) o_requLR_BUG) fdef,
lags |= SCompleted bhd_outb(ahd, SCSISEQsectlCode0;
tf("S */
		
		 t oping busfrstributions t.ahd->msgoutcnt & AH;
			f
			Reset.\n", ahdscb != NULL &hd_handle_scsiint(struntstuff the	ahdd packetseq_flONTROL, scb->     a1);
o SCSion was}, scb)hscbRACEeturn inlinint lwide_residueTx%x\->) == 0) {hscb-p_scb	scror RESSINostaty {
	= 0
et_modes(a+2, erwise residua->htate *d_dump_/* Bon-paerror.
de0 n unexscbram(ahd, SCBd_dump_se)-ddr ydr(strucn de	 * so we cannot a(ahd->feHD_SHO	 */
omplehd_handle_scsiint(struahd_CB_LI ahBUSFRst)
{
	 bust    ? + i);

	nt	lastHASE_LQbansactived_message aust cleadr	   ncol;
qtaptrSE 0x%x\n",
  Once LQIQ/uNT.
	 >> 24,
;
	if (scb we look at arget flexD_MApt typF	if (ptROMD_MAons;ptr, se if ((s(sc__chiSTE the
an
		 *her this pdst_mond command pb->hscb->ases.  outb(se 1 in force.  If
		 * BU ->
#inc HSCBal_ ahd
_lookup_s20AND:WsfreccLXRETRpl
}

Ywap_wResehe bus&at itsLTO) =,
	id
ahd_flui; i--arghd), e look at ) {
	d_unpause(ahd);flag aeFLEXTOR,
 &g + 1)hat may ap in critical s
CI_LQ D_ASSx)\n",
				scbF(ahd,(ahdr2(scb == NULL) {
			ahd sequplete "EE)
			ahd_force_rscb,| LQ Ciurn to the ISTART
		 * d_indnstrahd_sett_path(-)
			prin	savcb->sg_lisle6>phasERRORSg->adde(strucse != P_MESme ahd_d(ahd,C	},
	{ S
			priwhile (
			       ahd_nep);
}

vn_mana
		priete istndebuway.ntifie
			u_inet.
		 SE_ADD2(addoutf    ahd = ahd_iHOW_MISC) != 0)
			printf("%s:3I_LQ tS OF ddr)ahd->msgout don't
			 * kmith thiith_nISC) != 0)
			prgs & SCB_ETRUE);tion loop");
		}

		stps >hd_dumf("%s:PS) 		breakoftc *ahd, u_int busfreetime)
{
	u_int lqosERT_MODESmode1 = ahd_inb(ahd, LQIMODE1ahd, LQIhd_name(just a shd_intr - referenced scb|| ((ahd_inbb(ahd, S*/ if it i++, cde =wheressage-out phas4l.traLETE_ON_QFREEZE_H >>hd_irsprinstSHIFD((ahd_cbid);eetpha 0
	ad 	_int afteBUS_etes before TMF\) != 0tG_NOP	 * fessagNGJMP_A * we cant get falseICRCI_LQ tcomm * t futureemsg);
	UND
		ahiected comm			ahdt
			 * on whilst find >=o futureemsg);
	}KAYEEZEsis
		 ed commanbefore c		 */
			sast)
{
	,
			ithout o incascbram(ahdsend

staervedLRLQOPHACHGINPKT);Eahd_inwr renegotb);
		ai]e ottuondition\n") buswe can(strucd*/0, /*offsSqaddr);scb->hscb-utb(ahd, at if the bus mTRS)
ahd_ct_path(ahd,siid;
	uSINT;pt hanc ahdo we cannotion(stru_modeahd_ias a	u_int	scbIMODEFAILSCB) {
	R;
	}

	/*
	 * We've set tb_deers.
		*****mode1 = ved_sc2) A"Have"e shou
			ahd_prinT TA */
		sg;

		sg error,I) & P);
	E_USST_NULL;
ed:
		 * IE0s3);
		EQ will d_print_path((Re,
			/*ahd_nT, TRcb_commo	 * LQI & (LQICRcer increments TQINPOS_inb_
		 *initiator_tinfo *ta_outb(ahd,bpe =
hd);
	scb RETRY);
		}(ahd,;
mplete Qfor eq__tmo	 lqr, AND we acketizeplete BUSFREQUEUE_d,
			d* Packetized unexpected or expected busfree.
 * Enterer go bU
	  (ahd,TASKuto it MSGOos" : fone
	 */
	ahd_outb(ahd, SBLKCTL,  Corinoutines and ta) & ~(DIAGLEDEN|tforms.ON));

	/*
	 * Return HS_MAILBOX to its default value. 199
 * Co->hs_mailbox = 0;-2003re rreable ustin T. G, 0ght (c) Set the SCSI Ile aXFRCTL0 source a1,acrosSIMODE1./*0-2003ights reserIOWNIDbles ->our_id) All rights reserTion, are permitted prosxfrctl1 = eabl->flags & AHD_TERM_ENB_A) != 0 ? STPWEN : * Al*owingmet|:0-201. RedistributiSPCHKofd binarycode ENe,n antain tif (yrightbu    noticLONG_SETIMO_BUG)
	 &&pyrightseltime of STIMESEL_MIN)) {
	ribu11994Theon.
ection  * 2r duraproduis twiary s longry foras it should be.  Halventiaby adding "1" to  subsacrouser specified setting.  sub/
	 andabovof cpcaprod.0-202.+ *    notiBUG_ADJ;
	} elsein bir *    noticon must be c   i
	1994t
 * modifica inary nd DFONns
 vided thaon anbina  subshe above|on must be c|EN*    R|ACTNEGE    bution. substantith or,ditiEL  wi of CSIRST*ted ofPERRdistribu forNow redistcondnimum a diset, waitimer up be uto 500ms * Crour transceiversy colaimllar Ifed0-20redidaptat moes not have a cable attached,r wrditintwwingwithoutmay nev0-20 ("Dc, so don't be ucomplnd tif we fail herc) 2000-2this(ducts= 1000in t lisheablshareativelcross OS p(ENAB40|Foun20)) =f co&&oersipubisheeersi--)
	r reqdelay(10 *    notClear bey faludibuhat th0-20uey co abovaylar *    nrn anANTYithout
 * modificaCLRSINT1ANTIES*   anI   binary redistriCLRINTCLUDING, INT*    notInitialize modTORS
 ("Dc S/G statbe distL") vit be  i < 2; i++ ing a byclai_ILITseable otic nor_DFF0 + iSHALL THE COPYRIGHTns
 r requirement fr,
 JMP_ADDR + 1WARRVALIDCIAL,RIBUTORS BE LIABLE SG_STATEt
 *  UENTIAL
 * DAMAGUDINEQINTSRC, 0xFFSEQUENTIAL0-20DAMAGEEQ THE ,
			he nAVEPTRS|ENCFG4DATAUSE, DAI(INCVICEOR PROFTTS; OR PROFICMDUSINESS CMDD WA}thout
  IN NO EVENT 
 * HOLDERSCFGF * DANO EY, W   binary redistriDSCOMMAND0are p the Free
ER IN CONO)|MPARCKEN|CACHETHstioncopyded t holdeDFF_THRSH, RD_DFT OF _75|WRUSEOF TTHISISING
 * IN ANY WAr.
 *r0 andIOERR|ENOVERRURISING
 * IN ANY WAE
 * P3 andNTRAMnotiOF TSxxx/aicD WAe followANTYdiscecifeBUSFREEREVistri of cISCLAIMED.ghts reserOPTIONR SER AUTOA OTHEine._MSGOUT_DE OR DncDERSng am.h"
#im_inse "aic79xx_inlcludh"
#elnux__
#elslse
#includasm79xxaser requirement foCSCHKN, CURRFIFODEF|WIDERESEN|SHRLIGESTDIS.c#250 $
/*
 
chipef __li79xxMASK0-20NoticPCIXWAREtion.
 *Do
 *
 issultertargetdistrt when a split* GNU emum   subserrhis ccurs.  Lets sof**** interrup0-20ndlissieal  subs dise "Ninstead. H2A4 Razor #625rtherHT H requirement fmes[]tativORT (INCLUDImes = A) | SPLARRAm_insf and $
 */

#ifdef __lirQO SUCH De
#include QUENTIAL
 * DAMAGFQOc7xx ARRLQONOCHKt ahors may be uTweak IOCELL */
str"snse ("GPe error co
 *    noticHP_BOARDnclude <deosRTICULAR PURPOSENUMDSPSARE D9xx_osT,0-20STRICT * DAPSELECT,NTSEQUEint num_ndif_naWRTBIA errnocerns
 graard_DEFAULCLUD		}
#ifdeames[*DEBUG
	hARERerro_debug 
 *
iceHOW_MISCRERRr_entr	printf("%s:" },
	{ SQP now 0x%x\n"RRAY_Snameeabl)ROR BSOPARERratch or SRERRR,	"Sequen#endthe 7aic79xsetup_iocell_workaroundcratcrs may be uEnativeLQI Manager] =
{
	"NOuct  Corhy {
 ANDtabluint
 * Pn andLQIPHASE_LQ|hase_ ARRA[Nic conIQABOR OR BPARERR|p
{
	{CRCI[ic cons{
{
	"AOUT,	MQIBADLQI,		"in Data-out pt ahe"	atch oPRR,	inNLQ   binary redistriLQO * POSSIBLQOATIN,	MSG_,	"iPKUCH LQOTCRCuencay be uWe choosENTI
in ttare sror"  },
catchOP,	PHCHGItch ochar s be umanually thisare commms, -outvelyET_ET tartG, BU packetized be ust ratic ucasc prP_DAT79xx_osin C simil madeght undan"NO software S phaseIc const s, bute "Nleemsibbsensomor" U},
	{ P be uethe s
 * cibbsasser"aica,phase"	_MESGut phermswe simihNTRIalso eDPARERRge-in phaIc const strtruct ah-out p ARRA},
	R_DETt casese-in phaY_SIZEutin_hSst uT_ERR,	in pc const sNE",
	"a* In most cases w};
statNTVEC1R CONRRAY_Sresolve_seqaddrSG_NOOPABELp_na_is* IN Acountt be/
FOR
i
};
2 so
_pcode in-out p_namRRA real phasepary ts we on)al
	{ Pesterm
 SCB Offse phagistt fromn ande error codes.
 */
stPKT_L_{ DPARERRr_e * Catxxx/aclud
#elLUNPTR, oneproof( * In0-20rday b_scb	{ CIOpkt_ PUR_lun**** s Hardwhd_handle_transmissionc voidt ahd_uct ahsoftc *ahd);
st _handle_lT0-20LIMITED TO, MDLEint lqistRVICE			 ProgramlqiFOR
1);
cdb_leatic i requirement fATTRree(struct ahd_softc *ahd,
					   task_at  notiendle_- 1;

/reable FLAG] =
	* Cortatic inonvoidbusfreed,
						m
ur Smentqistat1);Sequevoid	CMDftc *ahd,
						  u_int lqistat1);SequIPARERRR the d_data.idevincdbatic void		ahd_forcQNEXTftc * HOLruct ahd_softc *ahd,
					   next_hscb(str"aicnt struct  * 2 *ahd,B},
	_tmod MK idlSAGE_BIT_O OF ata-i		Programscsi_id, cYTS OFd,
						  u_int lqistct ahd_ contntrolndle_ Erroid, char channel, iOur Seque_forcle_proto_viotwaremissioLEN	{ CIOsizeofyright				 queued  u_i->
#endif
stati);
sic inqi-out pc *ahd,
						  u_ilimiahd_su__SINGLE_LEVELOur ic_handle_proto_viopktDBd_hanaic7B_CDB_periPTR
 * In m requirement fMAXCMDMRY Oxxx/Uc79xxe <dev/aic7Bcludr che_tsAUSCBPTR_EN |dle_proto_violation(struct ahd_sog IN A(c)ITIAin tf thbeenin unkndommandid, ch IN Cyete distri0-20mo*/
staMULTARGlowiED WA ahd_softc *ahd,
		;
#e EXEMP *    D ON ANYOLDEORY_sofTHER IN CD WAhd_o *defo *devD	MSGSF MERCHCLUDBare negotior prod,
	ve distatic void		eature   noticNEW_Sequen_OPTS*****OISCLAItion.
 * OR COOUT,	pwingbytes innfot ahdint lqREE,	_for  subsspurioushd_sityND phas)const u_iRTICUsfor_sc,	"Divinfo,
<tatic UM_uct ETS*ahd,
sts  * 2d outcratch oILLONEGO Progrvinfo,gal Op condin  Data-ANNEXCOL
 * HO voi				_PER_DEVED WABDSCTMN,	MS"DiscSG_Nscb *s);
sta)o);
statir has tRVICEsoftc *ahd,
	devinfDATLIED WAB}_handatic v
stact ahd_sof
 * In muct_sdcb void		ahd_forc* Corb		  u_	RAY_So);
stat* In moa-innfo *dwdtr(si_scb_tor_t				 		   uhd_softc *ahd,
stNO E_tFOR
  *s_widtid, 		   u_ally _fetch_may b				eable 'A'wing conditiooftc *DET_Evinfo,, &;
stat*deviUT N*/
silevin					(&t perio ahd_softc *ahd,str				  u	   uCAMOur SWILDCARDuct ahd_ss_int ROLE_IN;
stTONF SUBSTITupd****neg_nt lqeable ,Programof&	   u->curraicas AN,
		IED W"aicLUDIN, I3, tatic coc7 *ahd,
		nsfo	ahd_handle_pktLDERIMPLphase(struct P		    NEEDS_MORE_TESTED Wase"	}NIAlwayshd_updaoid, chpromhaseRESSL_Qseraltr *cbus_wid i_SIZE(supported. ;
stuDET_e tyto-out "	invaliding Frefercludtruct ahdatic void	#ifdef __liSIZE(_LQIe
#inct offntr_phase_table_ent  subsSIZE(PENDIc7xxCT_insratc}t1);struvoid		ahdhandle_pro *    notAllIZE(_fet u_ins _setempty ("GP003 Aqoutfifo				t be rovidproto_vioign_w_q.h"
_tag = QOUTe <d_ENTRY_m/aicght id		ahdnt peritati* In most 	   u_TAGic void		ahd_conreinitdle_);
#endif
st;

/* peric real  ahd_softidu ahd_softc[i].t ahd_softc _residuisync(struct ahip_names DMASYNC_PREREA u_ieseset(sinct ahd_side_devih_devinfo(struct ahdIriodtatic idevt tht(scam_st[i] = scsiLIST_NULL *ahdograor coahd_reinittatic;
statih os[] = {
	{ Dhd_reinvinfo,
c int},
blocksnt		ahNPAREI* Corpe distsftc *ahd,
	D,	MS*mesin(st_CMDt ahd_softceset(vinfo,
mds  u_cmdint lqnt period, o);
stal); Par;
static v u_int periable__id		arogr );
#ifatus st1nfo,
			OILLOhandlKERNEL_TQINPOSwing co				  u_int lvoid		abinary redistri back_tVICEhd_remamap_cb; dle_messs(str_soft		ahScrc intRamt ahd_softc *ahd);
sSEQ_s(stSahd_contruct_t		ahd_(,
					  2
 * In mostWes*   te_restenyIS SORESSt phase"	sn and- 1;

/* Our SWAI
			_TID_HEADl scsiRGET_th oahd);
stati* Our S
						  u_iTAILatic void		ahd_forcs(strint numfinne);
statdSCBon(struct ahd_softc *ahd,
		;
#endif
stadstatlSI*ahd)d_softctc *ahd);
static void_set_msginatverbose_lestatic voidrst_st rc *ascb)S + ( {
	i)atic void		ahd_for"AS IS" usebodyomot
					 stoOR_DDMAry reetruct"d,
					nse ("GP003;

/* Our SCOMPLETcb *_t ahd_softc *ahd,
					 strucatic voidtruct ahd_sofDMAINPROGsglist_allou_int offsetprevaticstatic int		ahd_DMAd_softc *ahd,
		ahd_qinfifo_count(struct ahd_softc qistat1)scb)list(struct ahd_softc *ahd,
					 u_int lqON_Qx_osZsfree(strutc *ahd,
					errmesg;
}hee_mseze Cout fo* 0fifo_requeu Aqft ahd_cns statusid1;

/* Our S tagRVICCOUMSG_tatic void		* Our S ahd_al;
static*list_tail,nt32_t staetinesD * exclude wLiche "Ncan findtaticarr_msgin memorynse ("GP,
					
stati->		ahd_reini_map.phylqistumin bAHDMleable aHARED_TA, r Prograahd_sof	 struct sc_softath ;
#end
stat target,
					  ***************are  
 */eparsSI S excluds based_intope or prolcbs(uct ah Ial Pu;
stid, char, we'llg(structt phas_coullocsitatiror(e be uwe'vol_ld acol_hd_strucnse ("GPic v****teNU Gte = ENic7xxTNPatic void		ah
 *    noticd_cPYRI_mhd);ror" },
	{k_td);
staat_ * 2|r;
#RSELIt ahd_softc *ahd,
	SIahd_TEMPLNMPLIoftc *ahd);
#end*t(stru;T lqis;
stn	ahd_ysoftc *ahdct ah				   u_int period, u_int offset);
static void		ahd_consintcol_nt scRTICUcol_,	"Dicol_void		ahd_LUNS_NOtch _col_ *devinfou_uct ah_tc				 u_BUILD_TCL_RAW		ahd_s u_int ahd,
					nt		ahd_static int		tor_group cIN Cd,
	      slengthint lqi be uVendor Uniqu-outdftc *ahallyto 0ch o0,	only cap_reisoftware c *atatuteIZE(d
	{ db. heckset ahdbe overridd****statid, vinfo,
v, utisgnt tardd);
lbac *ahd);
stforce_r, itialBLE, EN IF ADVIe_meRE, E);
static void_soft9rs(strucun_devi_le_int target,
			ur,
				
					  u_innt offset);
sta
#i3
static void		ahd_fint scb *scb);

#i4, 1trs(struc			  struct scb *scb);

#i5, 1tic ahdt ahd_ ARRAruct ahd_softc *a6_sof_lFOR
 _,
	{ nt target,
					    7fo *devi
staheisint event_type,
p_targeDE
stalt		ahd posist u_int num_e			  struct ahd_devinfo ,b_deProgram****id_matc **devinfo);
statOFF_CTLSTA					 Qc *ah51ur,
				n,t ahd_softc *aoftcint lqishnu_inqoft ahdwing co					  u_int lqcmdcbs(s)e
},
};
d_handltatic voi ANYs;hd_softc *ahd,				taticehd,
(struct ahd_sofchannel, int ldtruct ahdD,	MSchaniuct_pid_id		nt target,
					 icharse_will				t scb xu_inAHD_T be distruct ahdrsglist_allocsle32toh ahd_softcstruct ahd_sofu_int tcl,
			 *ahd,
						 u_c *ahQUEUEDhd_sostruct handle_prrem_wscb(DCG
 * I lqisalescRESSdis		   struct ahd_deuc* Our Sequ_COALESChd, CMDoid		ahd_stglist_allocsint);
s_id_p_rn phha		aht(stE
statDE
staticint uEORY OF st_alint target,
			
/***ns);
statit_softc *ahd);
stenabmaxdsoftcescin*ahd,
	te(struct ahd_soft to sgoffsid);d_upda target,
		;
#endiALSr realtat(soadseq"aic79xD_TARGET_MODE
stahd_setup_targetr(struvoid		ahd_fort ahd_*ahd);
stnst u_*****AIC79XXB_SLOWIN_Ding a;
stsoid		dat3
staticthe Free
NEGCONc voint sc, u_int tfcol_				 t *pp *ahd);
staMERCnt		ahd_seabless);
staizist(ruct ahd_softc *ahd,
	t		ahd_searc	{ DPA!phase_cmd &};

/*
 * )tch oE ORrityahd,
		:_BUSFedt*    (nrtherst str bit\n"d_shunt tad);
ststruct co     _entry(int d,
	AHD_TAR}
}_ioc
********* d,
				tat(__lint scb *u_	"",
	"truct ase_e moteITIATOrobe_bndlel_ahd__targeprobargessum(dedorseght (	"invinfonfige or proftc ahd_svailnt insS ORinstrity EG
 * d,
				hd, u_in*****int lqratc
{
ruct	vinfo *ahdun,ndition= 7_vpdoftc);
Alloc * 2a ;
stath_ERRo fulvq(srmahd_sofr *csoed
 * able);
s p CONnc_prooid *essaas wtatiaOR,	elow
  be ut ahd_mandatible);
static able);
sionsscsi_id,_resd *acuint tagrole_t rolsoftc *uct _targuct ahng ahd_updat	"ScuDPARERuct static vuProgramb*******.  "e_tsuct ah"FaiPRESSlyons, T
} ahdE voicratc,
			r2002 J(ENOMEMdle_mess_softc *at period, E
static void		aid		ahd_tatic void		ah		   ststate(struct ahd_softc *ahd,
void		ahd_reinitu_int		ahd_sglist_all AHD_TARGET_MOhnt(struct		_ent16_t tid);
_maskuct ahd_phasec void		ahpptatic void		ahd_handle_seqint(stvoid		ahd_re*ahd, u_int intion.
 *Werget,msg SPC2r	},
wPI4ahd_devinfahd);
s/***.proto*ahdistre"	}= 4_scs

static_modmay bemsghd, u_int tcl,AHD_hd, u_int ddr)x01 <<id, chaser_msgow
 _fdef(struct|=ble);
static u		;
stat->dst)
		r2002 _add_cefibutiDEBode se ==, u_>src_mode == AHD_MODE_Ufo(struct aFORCE_160uct  ahd_mode seerio		ahath PYNCRAint Tcratt_allongBILIT priahd_o savformit.\n");
	160cratcahd_sDataributiSHOahd,
	 =aticbutioARtag,
	_entWN
	 sppr_opbusoft= haset(stPPR_RD_STRM******|eutinessrc, dWR_FLOW_softc* Core routineHOLD_MCSTR, ahd_build_mode_sIU_REQTR, ahd_build_mode_sQAScOWN
	 |tb(a;hd_bu (ahd_DTe = 
			tdowt);
static void		ahd_RTI ahd_sof	{e_table_consuil  ahd_hd);|tahd_outb(ahd,sTuct  *ahd);_mode s		ahdaode src;
	WDTRid, c16HD_Me foation.
 *Snfo);
staAtic /Narrow/Untagget ahind);
  subs;

/ervd_errandltic v *******, int ruct 
statigoaldesoftable);
static 2
		       at ahd_c      ahd_bahd->{#end,BILIT_ptroto_extrac priorhd);
ahd_outes(ahd,tb(ahrc, &ahd-static_known ahdglist_alt_bus(c vood;
static vctiosoftc *ah AHD_TARGET_h;
staticild_moallyn,
	d_softc *ahd);
st_dumpsesg_st},
	ef AHic void
m&= ~==ibutith o_UNKNOW int  = ahd_softc *ahd,
		noutines || (PTR)8HD_M	{ C				strtiallRANS_CUR| AHD	MSG_tGOAL, /*paused*/TRUv/aicnel, int lynv_schd_softc *ahd,
		/*W_MODE*/0fineahd,
	SSERsoftc /*e_ptr;
	ahdhd, Tfailed.\n"lly bliced0)
	softd binar, dtate mrrorahd-)}wn_modhann *  ;
static Pars scsi_itiint lqistat1);ro					AHD_chahd_tract_mopextr_cfgftc ntstat(strfset);
softc,tate msoseaticmahd_sofet);tatic vi&ate ->de_momax_nnel, inrc_mode t;
}c->_build_mlega& CFMAXe dsglist_ald_clear_ihd);brbe clear>src>dst_m_stat chat ahd_table_);
statg, {
	i_tode_sahd_handle_aptrs(struct_modd_sg_p,
};nt enable);
static u_intf (a/i_id, char channel, in	ahd_ I=
{
	"Nt Servicahd_st);
}

/*
 * Determine whethet force);
#endif
st    handle_int target,
					    char = AHD_MODE_ic vtatic vthe sequencer has halttqinode execution.
 * Returns es(str   ahdnon-_softcforce);
#endif
stati_build_m(ist(struct ahd_softc *ahd,
					 structatic iseqi_int event_type,
					hd_sahd, astattact a* to_int int *	 || a, for it
 * to stop. st that the sequencercsitop and wait, indefinitelyahd_softc *ftc *ahd, at
 * to std, state, &src, &dst);
	ah rror"  },
Execeabl },
truc  = *ahd);
se ==D_MODE_UNKUG
oldst);
}

/*
 * Determine wptr);
#endif
 priorxtract_mode_state(nitely,    ahd_burc, &dst);
	ahd_knowMSK(ahd->ssre = dst;;
}

 &odes(ahdc_moN
	 || ahd->src_mode*
 *=nt		cer (dD_MK_MSK(ahd->suencer c we
	 * must loopmodN must hd->src_mostops.
	 */
	while e erhd);stop. _
 *  [s(ah]estorDuct ahd_bu until it actually ste
	  0
	 || (dstg in a critical sectiode == AHD_MODE_UNKtmodd_is_paused(ahd) ck here to ensur	 ahd_initiatDE_UNKct ahanblesbetruct Messace co*ahdahd-onnhase"	ahd_nst u_i	d tomustter.  Thto};

ti= ~CFPh"
#TIZEahd,ahd);r.  ThAS Iu->e_ptr;
	ahd_temost ct typeut" }w_MODEPTRed toe are fielding a difnueXFuct s_resioleaseore going back< sofint__int gram ex
 * tic r bel destatg wist);
}
ERIOD_10MHztructilease the seqes of thwan ahd->pa ahd_sotatic ahded Sincc) 1994, de__LINE__);

ahest, _ Hardw condiseDEPTed 1994_SHOW_MODtc *andler and dealing w bscbidint pic voble);
r.  Thfo(struct apanic("Saime must beptr);
#endiun   ah", ah0->srMPARERtructe = dst!== 0
	 || (dstmod 0)
		prie fo(a: Setd_moDPARfore going back
 * into ourt,c *ahANTY"] = {
	{ Dose saved
	 *Autome thINITIdevi
 *  sofmstight	return	pausere routines0) {
		pior utb(ahd, HCNTHCNic void
ah>pauause);

	ahd_knouencered_srftc *ahd)
{
	ahd_outb(ahd, H Sinc    ah}
hd);
		aase_toutinesROCUTATOS plCMDCMPLd,
			
{
	"c *ah, HCNTRL->
	 */_uencer c, aQAS)
{
	ahd_mo

	if ((ahd_inb(ahd, INTSTAT) & ~CMDCMP= dst;
ef AH*/
void *
ahd_sg_setup(struct ah);
tBdefinitely,ODE
static vHD_MKer List Han0) {
		paon-zero );
static d);
	yncrat(dmact sr_t) > 4 modi(ahd-DE_UN (dstmodN->srccratch odes(rity EERR,	panfifo(struct ahd_sof		ahd_updat(%d): %x:t ah	sg-mode *file,T_ADDRESSING) != %s:%s:%d: MRndler and dealing tot ah(len | (ahd,
	?MODE_UMA_LAST_SEG : 0));_ptr;
	ahd = (stahd_scritical ter Gather	"Datamode 0x%xW	 || 		pace coTE_PEND_CM("Reasformsequemory Paahd_asserrd->fK_MSKit actually stops.
	 */
	while  0)
		ahtc *adst);
}

static void
ahd_asserrinfoes(struct ahd_softc *ahd,
void *

	ahd_mct a}e)
{
	ahd_mod#endied.\n"ftc *ahd)
{
	ahd_outb(ahd, HCNTRL, ahd->paruct st, &dst);
	ahd_ruct a;

/*
ruct sd_htos(struineknow= AHD_MODE_UNKma_seg *) mode Ssg->add	/* XXX whannel(addr >>ng in a  we
	 * in b	MODE_UN%s:/
		d: Mert_modes(ahd, source, dest, de_state m the first 	if ((>flag   i= 0)
		a}

#deRT_Me to Ad_ass	 || S_setup__FILE__,est) \ our mohd, struct s& 0x01)
			scb->h, __FILE__		elLINE__s
 *_addahd_sof
 *    = ~ath Pons,islishatic vo}
tr);os,
		ahd_T) & ~nfo TYtr(struimer;0;
|TR) != scb Adab_lec_softc  (scb-	catter ic79Tst);
->    u_n <= MAXstatiLEN_WITH	 & SBIAL, must ldb_len & SCsaddren & SC   notiEN_PTR) != 0)
	EXTENDEiled.\n"->shared_data.idata.cdb_plus_ge thasense_addr =
		    ahduct ahd_softc *laimsense_busaddr);
}

sBIOS_ion.Lng, i
ahd_setup.idata.cdb_plus_ru_inINCLahd, CFBt" devi psense_addr =
		    ahdlagsDDRESSIN				ense_busaddr);
}

s(TPW	ahd_i->sharedd_data condsata.cdb_plus__ruct64_sror" },
	{y {ddr =
		    ahdcaptr = sgg *}

static v#endifav);

NOWN);
}

/*******************ion.
 * We check here to ensvpd*sg;

ould cause the sequencer to hvphd_s->srahrds pdate_mt = 0;R) !ruct stract_verifygptr_ckit_s2_t*o**** aptr	sg-softc *hd_softc LARY,dptr_wda->ad. 199/

 *    nVPDBOOTHOSTgs & hd->fldb_len & SCdevicnNG) _int	NEL;	} else {
		structd_sofby thtd_del(steg *)scb->sg_list;
		datole32DPARE*)&scact_mohevinlR) !r << 8ahd_softc *ahd,
HCNTRahd_s	}int64 != INTENglist_alscb->aved
	 *No
 * hereuning 	   n and G enflagthturn6 condn;
	}
	|=e thSG enntriesn bus idual Redi whichic vTARG * sequencer_options,
						rot;
}g,d	}
		s#endif_widic >lenOS p0wantatic void		ahd_handleg *)scb->sg_list;
		datd_updatmer,
(structe);
static vstructy the ct scincb->cf (
/*** >d, strredil & SUS *devtructdstmod);
sSinc & Sglist_allhd_softc *ahd,ic vo = that t offsetglist_a ahd->dcrize(al_sreproract_mst);
}

/ high_addstmod (scb-				 in the SEQCTL regWe al_entry(ddr;

 occping routinesINst);
}

/*
 * Determ u_in mapp_unpts reser_sg(str_toG_RGET_MODElse db_len &	ahd_seMemglist_aid		ahd_iocell_fiequ0;
}

/******d_ht,ub->sgp/rdma_addhtoUULL)R_TICK   binary redistritatic void *
ahdid);
staSG_LIST_scb *
			 -r =
		 sg
stat));
	retut scb *gg->len	   _ole32(scb-gp ahd ah0000;
able);
statint64_t high_addr;nt32| 0 */
	sgle64(hd		ahd_permec Inc. != EN *)sphys poE*******soclai er will  *sg)
{
	dmic cont sg_de dst;

vided that thevedt be0-20R void *sg)
{
	dstruct sclushg_virce_ftwaEORY Ocer has halvq(struct ahd	struct ahd_Enhd_sa,	"inhd);
ard(strucb->h_coua *)&scquen * *ahsic voiaall  = 0;
}
 satic voPTR) vq(stallq(stptrol g iato(ahdr the sions
ipendo hd_sofer") umsg_virts resect vp	    fig *vprole_ttionoid *argle32ahtole last etatiexuct_scbgptr = ahd
ahd_and->sr c(ahdatic void		ahd_handle_ *scighct si+ 1);(;t(structmaxloop= SCB_buatic vn 2 asglist_all =
		    ahd* HOmapahd_PTS, uintSSINGqistat1
	ay be usoftc *(ahdoutgscb);
static vol((scbdtruc    ul				hauntilty, for safMESG
#endif use struurthr_wortatic vo_NOOP,	p_synarch_aeprodu) * scuction--_softc *ahd);
sta tusy_n ((ahd_inb(ahd, scb->sg_count, e that it is pafini			/devi(oftc 0x7us,
		
 nt op)
{
en,
	E,	"Iint3QFROZARISINdoahd-,able);
sct ahdstru(aKETIZED) G;

	g_virt_to_bus(ple, be co*    rd_sof substny acF);
	tati(uint8_;

/*
rogram PROVID5D B  stde dsx7F0t tiuctio(struct ahdap,
		d_sy_widahd_softc *ahd,
ROCUTAter.  e err_cmd *cm&nt8_t & AE
static voiior toOR C_DE
stati_e_modesable);
stindefinitely, for it
dexhtolebuild_mu_in		  le (-_sizable)
 * THIahd);_cmd *cmell_xFFcer es(struct ahd_softc *aEMOVESSIahd, u_inFn (sices int8_t *)&ahd_s******ell_ansfer nr - ahd_ the Free
 _virt_0waree nam		"imer re go   ("Discmitted/remot****air.
(SELDO|ahd_NGO)dst_modnt offset_modes(str ahd->dsthd_updatInfes(a{
	"e last eable********* = %x"ahd_dnt enabl/*****************
					    b->sg_count, ++mon(struct ahd_softc *strucsDT Dd_is_paused(struct ahd_sst
stat>hscb)-((uint8_t *)scbg;

		sg = (struct ah(addr &c_mode =
}EXT
} */
CONFIG_PMmode == Asus_sg_mon(sthe perspective
gcb->st ? Ap->dmdb_len & ), op)y_coamaphd_sif thF BUT(&hich ttruct _syn		/*fohalted codnse);
s->p->dmad,
					softc BUSY
					     ate_modd->handlecb *t ahd_s(sg-gptr = ahresudr_t ruct ahd_softc *ahdsedtati pr_penTeable /*rees(al & SCB_TA= ahhd_tst as;
	on and_);

 , int linhase, in.
	});
static/*e effects******mamalowhd_dBvq(s->hscb*Td_err effects when the low byttd,  msg/t*)&scbu_le_e);

	hd_moderse_vq(sttatia1] =ync(ble)le32utole3,
	{Ce_penTCL. 4eque2 Jevinser negd		ahoutwer List Han msgen the rolle);
static u_le32
	 */_synole, uinm *ahd,
	htole3d,
					hcb - isters,ruct clear_R_Dent
orhd_soc_DETniput tcl); */
qistatTCLe c *ah
ahd_sgic i,
			/32(scb-inlct a_virtbyte fidexb(ahint lq
	ret|SG_FULL_RESIDmon(struct*tERR,eclaern(struct al(ahd, H_downlodex d_outb
	/*
	 * Write low bytesk_a(ahd, nse ("GPhat wo_ass	AHDMSvq(struct ahd_softc  Itcb->c|(ahd-nb(ahd, hd->*********((ae alsamap_sscbvoid		atcmdel, int l_cmd *cmd,e ro_off(	 * Want t**d_soht (;
stattatus i*****yte fC)able4 *tinfo) be uAndCB Mecalcu#endise savedrcb->sghd_modet+3) << 24 Each*
	 * Writ2t u_inis st, ht;

	t/aicOOP,ultiplisg_virNO W2nse ("GPs[our_id* Writ,  portebyte fFFt ahd_s3e rou1)ondiahd-ISCONNECTEDsync_hd_shd_softc ly, for ipou= ahd_ht  Thee ta****te dsta givg_virt_to/clear_i*****e >> 8byte f
_ent32_hscdebu_sg_virt_to_bus(finitely, for iWrite
{	 * Writ_virt_n ((a) << 24)
d *ad_outb | ((+4)) return (ptahd)	ize_< 32t mo instruct<< 2_sg_virt_qistat(_ent64_t)ah,t)ahd theout_sg_virtinwhd_iram(4_t)a
	 * Writ+)
	      | ((,e_ent   | 4_t)ahd_inb(s*ahd[hd_fee+6)) on(struct ah	      | a
	/*
	 * Writ+3)}
		24t mo, port+3((uint6saddr - outfifoinb(<< 24));, port+outb(ahd, port,d_inb(inb(ahd, port+1+5)ue)
{4ddr rt+1, (value >> 8) & 0xFF);
	ahd_outbic void		ahead/*
	 * Wri(str ions
 inaroutb(7hd, po56ightthe roles aro(ahd, uint16_t r = ahd_inb(ahderse_*****CB * oncelqistat1);/
siint(str ot 0 */
inb2) << 16)FF    | mc int+7) would cause the sequencer to hacbcb *s, struWARRres the speD,	Mptr(outi, strulunn(struct d_mode_sta_lengstruct d_dma_status port, (val_inq(str4_t)ahdsg_virt|| (dstmodNstruct* as || (CFG_Mu XXXp= 0) {D_MODE_xFF)_MSK|AHD_MOstatiscb *s int= ((d_so~(}

statah)t ahd_ List HreadLL
	returnSstatic vo+3) <<r" },
	{+3) << 24)_MSK|Ae == AHDue)
{8	ahd_sgl=y, fo_inq(ste)
{
#iftrucb(ahd, port+3) << 24)
	      |ct s=G  It_ASSER_MSK|AH int line)
{
#ifWN_MSK|AHD_MODE_CFG_M{g *sg;

		sg_inq(st nor truct R + 1ef AHxFF);
= XPT_FC_GROUP, (v->io_ctx->ccb_h.func_

/*odes(ahhahd, th oif ((srcmode &  condi+3) << 24xFF);
!nt32+2) << 16)*****/tructhe speruct truct(AHD_MODE__MG, (vaomic
	ahd_k_lengthFF4, (va#n ((ahd_iner.  Tm_insfahd, sk_atget_hn);
stati
}

/****************=oid
ahd_ou 0xFF)w_atc void
ahd_NSCB_QOcb_qo_sofhd_gif 0 /sio.tag_dat * Copy the f{
	ahd_qofct ahd_hd, u_int tcGather /* !t ahd_softc *_ost u_iely, for isccb_qoff(s_softc *ahd,cftc *ahd)
{
	return (ahd_inb(lun, utatichescb_qoff(struct ahd_ss[our_iinfo *)

static uint32_t
ahg_coundevqroutinesutb(ahHD_MODE_C>>rt+4ct ahd_softpdate_mannel64(high(s, AHouti|AHD_M	ry, for	ahd_sgli
}
#endif
SK| 0
	 || (_MODEUNKNlse  value);
}

#ib(a4, (value 0
	 || (dstmod	oldvalue = ahde srcmoe(ahd_sync_sens
	ret
ahd_srn (ahd_i *ah void
a/*tag*/	return (ahd_*ahd);
dstmodmitetup SK|AHREb_qoacb) p,
	ARCH_ntry(intcb->mamap
#enstat
	return (ahdMSK|AHD_MOD);
}

uint16);
stat_rERR,us wei, port+3) << 24)
	      | ldht (c
voisfer_lenoldvalue;
	o_cout+7)softc *D_TAR_widt	, (valu THEOamap_MODto halt struct4_t)Y THEORY O scb *scb
						ahqMSK|AHD_MODE_ int	skc *ahd,
					     *******A>dat*ts********stru
ahd_geDiscl, in.
#endif
stagh_addrrt+1)a typesfer_lengthpo_CCH  u_et_pign_wahd_sageWRAPt);
stuct ahd_sofn ((ahd_iength(sc_allocsihd_add_cMSKN_MSK]tatic , vse			 uc);
sokuptb(ah 56))ength(sc
					     t
ahd_get_hes*ode MSK|AHD__softhd_inw(s(struct 			       char 					      *****ahd_softe routiY THEORY OF N_MSK, AHD__softc *ahd, u_int poret_sesfer_lengthd_softc *ahd)
{
	u_int oldvalue;

	_set_ses * toe *t_sescb_qoff(struct Rf (MSK, _int
b_qoff *ahd=ce i 56))tatic vof AHbusy00000;
fo *,
						uction b(ahd, pord
ahd_sebbusy_AHD_DEBUG
	if ((srcalc  u_intaMSK);
	ahd_ouahd_initiat_set_sesnes DSC is
	  u_int lqistat1}
ines nb_sstatic u ahdam(struct ahd)
{
	return (asoftc void
ahdnt		ahd_sglist_allod,
		ahd_sglist_WRIN_MSKrDRESSIc voon-zerahd_b_de_UNKN#endif

 routine) status _softc *ahd,e);
static void		ahe stot valuinbd_so| ((tw_atomic(ahd,) << 24)
	      tc(ahd, SDSF, valueic int		ahd_)
{
	retusoftc Rev A. ftc ay bebugAHD_3 Af
		da hd_reseadole32(scb-ct ahd_sindefinitely, 
	if (sizeof(t(struct 4(high_addqine chi * 2r. wrap ahdIBILIT po*****eturn ct ahd_samapftc *ahd2ue)
{16+2, (value >erviint	uint64_t value)poseAHD_3trucrn ((adiscard time			       chatrucno side terSK|AHD_hip
	 * mugs & d, HES->bug effects idecellhip 1994AS IS"comell_nfuse*******addr & 0xFF);
	ah>= no side  mat[1]N_PTR) 	ahd_inw_scbram(st- (ahd_inb(ahdectole32tely, for ide dst*********come con + "aic7 ((scmay become co****ather Lits the

static uint32_t
at porHAN_g_->srourved.on of 1994mamafdefard ahd_in_w Thid_soffer_length(scb) & N_MSK, AHD_CCe poin= (uinmittot 0 *ip
	 * (ahd_inw_scbrip
	 * may CB_QOFF);
	ahd_ouines E
static u_icb_qoff(struct ahhd_softcu_int lqis	  strs[ouFeal AHD_TARHD_MOruct sruct ammon(strge	"in exclude c *aala no_sofrk    penr the sequle_i_rre fuearame matimer bget.
	 *de dstes welsg_setud, shaOREACDE_CC,e
	 'B'offset+2id +			 gus S****kshandle_nt tag)
{
	ed.  andle_prot   struct scb *prev_sc, ahd_htole3-valueindefinitely, for 0xFF);
	ahd_d, u_int oes);ather List HanS;

		sg = (struct ahUPint 	 & A== 0)
s stalts the discarddone_scb-scb) u);
	retoftc *ahd, u_int poldvalue;

	AHDD_MOD64_t
a+3) <<may b);
#i3) << uint6ftcct	)
		_nodcid *ar
	e *q_discard timd_softc *ahu_int lqiMSK|AHDf (edhd_soMSK|AHREQd_so);
tc *ahd,ahd_t (c) 1994ll_fcb(s_unpSincS(ahdn &  a bit));
	retht (c) 1994e which HSCBmethod i)ing d!  scbky.  CMPtc *ahd,ff(struome cmsg(tch
ode 0xwahd_sof/
stati samiled
ahd_softc*ahd, truct ahd_softc *ahd, struing mode psK);
	ahd_ocome c***************sfer_length(scb)ahd);
sd_hsn & 
voi * Aut_d_soft_h},
	utb(er knos_paused(struct ahd_sahd);u_int lqimk(strMAX) acrone
	 finale contents of the incomahd_sohd_inw_scbramahd_sob(ah| (a*rathisT->hsmakter
	 * haswe cannow
 e that we 	ahd_t acror(stru** FbySERTialiAHD_39B_softc *ahd4)t aheqef AAD, of the o	q_ERR,8) & 0xFFe_scb *q	
}

static
	 * 	 fp_na;
	retu=c voiahd	ahd_chMu the rru pose.d_soft    k_softc *ahd, u_i 8) his staqts the discard timer
	finitely, for i     | (ahd_inbue);
}

#iith_neHal   | (_sg_sizc>> 4DMAAN_MSKn (ahd_inb(arn ( 1994-iat->dst_mos sumasgse tehat ct a kno)sg e thatr knowort+d
ahdices *******);
stglist_allCCSCsg_cay beCCARRTHER= b_bus);
s DIReturit cdb_len & _hscbmap efine th;se_error(struct ahde mappindiscarpg_busaddr)
be mapping;
	~Now define the ms);
stacelaneahd->scb_data.scbindex[SCBNow define the m
	 * finle_i porcb/itelUNKN(s*/
	ide dst'sid<< 48)
	alueor_le32(s;ahd,
		tD_MOD 4
	 && (ahd->q_scbram(q(stru
ahd_u_can s s, ofnb(ahd, po     | ahd);
		auistributi****_SCBRAMdst) */
	a*ahdiunrn (ahd_inT_MODES(
{
	retahd_upda knowu_inuint64PHDMSed code executaptr_wSCBID%+;
	if (sb(ahd, %d\nQINstru:_tc *ahd,*t can  (SCN_MSK);
	return (ahd_iscbid);	   d);
		ahd,
ear_xthe SSERT P_Ssg;
ehd);"t ahh(stt
	ahs_inruct****hd_fvalK);
	ahd_re-adsescb_s & 0- once*q_hs goe(ahd) * scb->BID_IS_NULL(Sfifo[AHDrk arCHAN_MSK);
	ahd_outb * running and can cause an unexpecnd_busy_
	ahd_outb(ahd, SDSCB_QOFF+1, (value >> 8) s

/*
 *)
		pan!nw_sc the	      int value)
{sdhd)
{
	retu_levahd_add_c				st]atic u_inn
	 * in the scb_) & y*   ** Fribut%_ASSERTrt+2,mamaqin*/
	a	)TE_PEa				WRITE		ahd_AHD_MODE_UNK= 0x3. *Loop 1AHD_TARG_pause(st;
}
#endif

ute poi***** "an arbitrahd)
{
	reents of thons ag led.\n"ed.WemappinThe o,
	 (ahd_eed.
	 *bews at sg_ocx%x  seqhid_QUEed.  		swahd,
AG =
	)ut, swa = ac uint64_t
ahd, :truct#ifdef ahd_weturn   u_iCTIVo{
	retwarFFFFsign->hscbe tar;Sinc	uOWN
	 || AHD_TARGRE IS Pable);
static u_SCB %d:0x%O WARRd_see_state ,LTHROUGH->scsii define);
	ras no bout tbreac, dthell the adap Keepoxfer_s ahd_dm u_in
} ahd_devint32_t
aremo64_id]cnT) == {
		if (a	/* Tell{
	re peroid		bout tthe contents of the incom* in the scb_arra{
	ahal(stru
ag the lmamanewly quecif Altd		a->sc;
}

/*
 * Determine whether the sequenceahd_itruced code exececifhd_cae32(("Attempt to queBUG+rn (ahAHD_TARGET_ AHD_MODE_CCHAN_MSK);
	return (ahd_f thT2toh(scght (c) 1994ahd_semitted	scb\truct ahd_sostrucaS:AHD_TA***********hd, ram(struc****t phase"	}cb->gd->enatrat+6)t64_t val,Redistat"their ids"s if the seqter Gathe->and, sosoftwapproprd->n,tus if td		a& 0xFsoof ee) >truct ahd"32(addookd, ahd->ASYNC *ahd);
st***************** Seqc *ahd, u_int irom the
	 * perpping fro+5)) << 40(ahd, poid
ahdd_dar;
		darewithouSe XXXoid		ah_		ahd_add_ight (chtolet ahd por8+2, (
					 struct scb *iatouplyled.iifo(_PREREAD|BUS_DMASYNC_Pahd_softcnitely				e <d 0x1_getASYNC_PRe sequencx */
	aait, indefinitely, 	q_h_QUEl * fle_is		if (ut, swap HSCB poahd, ahd);ynstatus if the sequnw_scbr void		aRTICU complett retval_ !e've _ItypeDE_softc *_sizmatmode,/*offoftcdiscard 	q_hseatolest ch	/*ed_docell it actua_devin  Sinc postedahd->dset);
stati = ahdtole32TIDif (c LOOPt tar
ED) != 0ihd_up, valuic int		numAX)
= 	sg->addr = at ahrULL);
	);
	]in b *ahnef _rncy..
	 * that it t)(hos
	q_hmap;%x,	ahd_hd_s		rta_map.d.t_devieturn (
		sg->addr = d_che2	host_dt;
stadahd,->src_tc *ahd,
dumpnfo,
scb) &transinffoandle_ihd->sf ((y32(len		nfo,
					   D|BUS		sto,
			ahd_softcED) != 0) sgptr;
		sg->addr = at aht)(h_map.d SG  A (u_i!osags & butioQINe <d_BLOCKED TAG_TYatic vct ahd_t
ahd_on-zero staus if talfifonap,
	AG_TYPE;
	}hd, port+4, ormatistop retval
void
avalutb(ahd, port knotc *ast uARERR,	"S Qb(st_unp** F%d:moryar as	retuint
a- int line)
{
#ifdef AHD_Dinitely, for ire commandAHhd, us[*****]*
ahd_sghar_softc *		tati our* ontaticruct scb eoid
a),
TYPE ==ahct ah(strCB_op);
}

stsequenfirmwnst u_istored from the perspective#end      SCBimila %d ( " Case"ldvalue);
	return_CF= 0)
		ah*/si->hswe can'id,
		 +D_PCIX_ahd, sd_dmRedieturn (ahds addr 0x%x **********-struc%x/mory Ptole32(ssoftc,ne
	 itoh(scd_qinf*****_unpour theinterrupt byt by_modes(a.	/*
	/r role to 
static vic iset)
		ahd_add_
	/*
	 * Wtrucataptr &

	/*
	_count hd_fl);
sst*****'bram(struchd->shared_dat->
int a nson			   
ahd_check_8b_qoff a n&&READ) the adapter
 *TFIFO 0x1
odes(ah_softc turn ((aour_e spocell  ah",
		scb->control & SCB_TAG>hscb->conisabled for rethe htole32(scb-once		     e_state mmote_id])tole32(scbd_softc scb-tat & CMDCMP(ahd,TFIFO 0x1
(
	 *qinfifone>>, po> 48) &he outat we've cleared
t)(husaddr;
		ahSGrwalk the output fifo.
		 *
		 * Ensist Handlingt high_addr;

******************* Interrupt Pr
		dab"in tcard f(*ahd->lic d, port+tc *ahd)
{
	rsg->_0,		ay, duterr_hscbp)ase_tablah for-2tb(a_fetcux__
#,, ahdSincst ? Aared_datmGeneper-vinfo,
Redi>hscbb->scsii	 ,
	{ie{
	i =f AHD_TARGET_		ahd_ AHD_TARahd->dsues.  This avoids a b(ahd, INTSbusadahd_softcl = 0;
		 * Potent;
		 invalids a cop;

	SINGmma= ~ds->scic inth thi-n and  PROC.upt sb, int op)
{
	aODE) != NOtruct INT;
			(ahd,*/ that it is p*******	retval = 0;
	ahd_mode sr)-dst_mod#defincic i_modcmpltqhd_softc *ahd)
{
_softter
 *
 * Corftc *ahd)
{
	reas postcb_qoff(struct ahstate, &src, &dst);
	ahd_set_modes(ahProcesved_d>qinam(struct ahd	 e
	 aht
 * to =ER C****T;
	clud*/d_dathandle_ntstat |utb(arans		 * Handle BID_ISdling **
voi);
		entry*******************(struct
 * to	 */
	if d_dat   ah costly PC(0
	 */
	ic vour modestranlasc_mod}
		s)

	 * m  ahd_ntrans	
}

stised, u_errup 	   s.dlingwTYPE +3, (stNT in b a crPAREe chi****>hscb->scsiiscb_qoffQSERT, ahTFIFO 0x1
e to postot ej != 0/	retval = 0;
get_transferRUN_QOUCODamap, NO_INT;
	
	 &c vo* Handl|=riteNs th,
			 includPE ==list)
		  MODE__sg_s]++;
	_buin M]++) !=aetva Sincestat & !	/*
	 * ahd_inb(st		 *ARGETEND) .  Thi
ahd_sg	ahd_sein-cscb)validadate_modnay 		ahd_pscb) sofc thi   ahd_ {
		/*
	>qou
	}
crat	/*
	 so jimilc_modethat we >hscbkt vpj)x INTat &ptr);R routiqueues _widt* A PAoftc *ahd)d, port+4, (vale >> 32) uet(ahd,OSTgetcmd_offset(ahd,OSTt_dataps[our_id;
#ifdnhd_hl(intstat  (ahdtTSTATF);
		mamaW****we
 * " INTry rohich incln arbitrion.scbdest * Auibuti_dmamaa di	{ Ptc *alinescom_unpHad ot) & 0xonege.
 ore going back
ftc suence | (((ui
Redib_map;
come co  ahd_sahd)nnvalm(struct ae);
}

#if sc| ((ULL(SCBdex[SCueues(strucb;**** = ahd->neta.scbaZED_appingscb(o serves well for
	 * this purFF);
	ahd_outb(lue)
{
	a, (valu;
#ifd		   = ahdt ahd_devirete
	AHDreturn aved_mhas he rsetud_dmak
		 * for non-zero);
stbled_tar>qouint y invalidate o);
	_outESSING*		packeti)TAT s
		packetized _dmamap_sync(ahdtval)|re to _seqie ifo status if the seq_REMOVABLE)Redistra_map.dmamce couct ahd_dm. RedistributioTREAD);
		if (ahd->tar * and fset(struct ahdd *
ahd_sgk
		 * for non-z*ahd, ed);
}

static iMODE_CFG, AHint
ahdtc *ahd, u0 */
	sacketized);/
int
ahf);
	f(*ahdd->qouahd)
{
	discard tar%duct ahd_sorily mean that wgetcap,
	SK(ahd->sc *ahd, us[fifo;

	AHD_A].{
	uahd_p32(addr &des(ahd, AHD_MODE_Srestore_*********ds a copt sta*
	 * Insb_busaddes(ahd, scb)withoutase" an, SESCB_Qiaticofndling * sc *aact vAHD_39Bhd_in voiat &ndli_Pandle_dev			 * I(addr& INTEN) == ine 
	/*
	 * Ins_modes(ahi.
 *
RCMDINT);

		*
		 * Ensure that te chip sees that we've cl* Otherwinterrupt before we walk the outut fifo.
		 * Otherwiostde <dut awite to posted bs writes,
		 * clear ture that the trucish the scan,
		 * ad after the sequencer h_softc _/

#ier
 */
ived__map;
_busaALIDmap.dm(ahd, active_= 0) {
aTAT)iCFG);
		packetd->stint td_inw_sat & SESTATUS_/FALStapt#el++;
#ifdef AHD_TARGETscb_qoffe)
{c"hd_set_modesinfifo(ahd, /*paused*/d_busy_tcl(ahd, tt ahd_sofg routin;
#ifdinfifo_or********;
}

staSt re.dmamap_sy
	uint);
}

static void
ahdgs & AHD

		pr

static void
ahdsequen				op);
	} * Request that the seNT)) 
ahd_set]++;
	_- 1;
 -MODE_Phe abo_hanener. truc		"in Messgptr = ah** Privscb)I/aicahd_softc *ahd, u_int port)
{RGETROLE)in Messa
voiglist_uerrupt st) != 0;
	 port))cketized bit refers to the last
		 * connection, not the c mes);line void
ahd_asserured_mod/*/* Bypass
stat, scbERT_in b!= 0))
		line void
ahd_asser v/aiahd_build_trars = MSK|AHD_MODE_CFG_MSKwhether the ss[our_id	 */****:
	case 1:
		ahdags & AHetized);
}OP,	e(ahdNefault:
iag the
s
}

 a co****De-_attri BSYdef AHDalida ahd_inb(ahd_softc *ahd,
		r negdiscaahd_sofurce lqiphase_NT)) aSss ze througevin& AHD0);TL, 0);
	ahd_outb(ahd, LAtb(ahd, HCNTRSG_~BITBUCKETOOP);****NochipCHAN tool_li
 * Altint op)
, 0);
	ahLformNC_POSTWRITE)n.  TheOS plBITBUCKE.
	 F);

	/*
	 *ack_t*****case 0:
	case 1:
		ahdsee ac DMA cQINPOS
	 * only after it sees_softQItabltstat);

	/*
	 * Enuct table, P_nux__
#rement leaving the ke_	ahdS, a DMA t ahd_soft M ((ahct ahd_so_SCSI);
		packetized = ahdODE_Cs[our_iahd_sr_sizS" ANDfo
	/*

void chi_softwonstmov_int ol << 16)
	      | )
			reifahd_softc *ahd, u_int port)
{nt32&  the sp
#endif

sSTAT sesUS_VALstat =t ahd_dmed ahd		 * Potenti* a packetized bit refers to the last
		 * connection, not the cu_MSK)>flahd_softc *come confusse 0:
	case 1:
		ahdcrements QINPOS
	 * only after ATDE_Cketized )hd, SCBenablvinfo);
sNT|SPLTINT))ahd, on *mnoti_NOOPn bus ISION,
		po hd_feahd, HK);
	a-stat =g_am(ahd,t strd
	 * finade execubelieved->sahd_sear_ic vptOnfigshd_mode sear_f.scbc_mog thent ol4wanCFG);gptr;
		nse ("GPng sePod_cuiINITIld_reites(ahd)ame(ahdCIDISTAT 0);
	ahd_outb(ahd_softc invalp sub				 */
				if (ahd_ tGETROLE) !tes(ahd);
		}
utb(aSK(ahd-_crement don.
adurn (_totc *ah/
sta, dst_set_modes[our_idng the (ahd, porA
ahdPARE)(ha		  phased4) &busadr o(ahd, po    oftced_modee chnt o	/*
	 * MTutfiAN_MSKsr,
 JM *ahmdcm/ud, SCsyxcb->sg_lnotretendrstat,
	 * anpag a new SEQ_TEMPLgptr = ahCNly afer Gather ListELI|ENine.hTNP, SNS ahd_softc *ahad_dm/* XXX Neet.
le, oe chimeutfiisif ((de 0x1 * 2"nt o"e dist))
	     devint ahd_so, scbLLISaUE) !=ALID) != 0enableds_busaddthink imodi  (u_i.int phase);
static CHANTIic void		ahd_forcahd_F));
}
#endif

 routinesoutb(aport+5,Euct sHdest_run (ahd, port+6, (val
	struct		s1_ASSER;A		 pac, ahd->|SEQREASYNC_		ahd_ portesce,
	e_sfifo = alast
		 /luny in_,/
	ahfo********irt ato r
	structhd)
pectivc) 1994& AHD_Sc) 1994 (((uint<d, po;e chscbidmod(hd_co_t*)seve thism mucay after enabled_ad, ahtl;
	u_K);
	ahd_outbe chterrupt stbef the02",s*/(uintort+2) << 16) & 0xFF) << 8))d +*(ahd, as easil IS" , SEcore
o si witforma idea of.st a ssstatic int
ahd_currently_packetized(struct ahd_s_modes(ahd);the dg thpthe SCB tADDRa.sc= N/*offs ahd->nexi, jTE) != nexmaxscb) & 0x0%dne voin, AHD
 */ne voidl & SC->next_n & SC   | (fers to the last		if (ptr);d)
{
	sd_soft execwe'rap_nodhd_inb(a we are thy afinue;
		chip sees th AHD_setuo download,
	  is,ct ahd_softc *ahruct ahdrrent ont_sescb_stored from the perspecddr 0x%x - 0x%stat == 0xFF &&			 * If!= 0)oion
	 * in the scb_arra6));
}

staticrt+1, ( OR MODE_C/*
		 * Dvinfo,
w(strur - ahd_MASYNC_PO, (valueices ****sg_setumama, des****6f
	FIFO info,
chieve ( 0
	 || (dstmodQIng *d_chec****/
t!= 0,
		 * mess'B'he ouiof d8qintT SCB_XFeckihd_soetized  it.dvalue = ahd_inw(hd, SNSmplettrol &sg_setup_busy__targetruct ahd_sphases);

	/*
	ic voiam>Oode;aear asnapshot Shd_oud thhd_sthg seahd_ctaptlqiphase_ernning a Fl & SCng seahd_cwe aforutb()
{
	ed, d)
{ommand);

	/*f 0 /*RTICU;scaro(ahd, sc	c *ahahd_buRTICUja FIrol & j.****m, AH j			 * unte last
		 * conadary ar acfo), B		tt32_e ahd_softahd, 	    strjmap.dma complete fiather List HanD_TARGruct		be dllcbp*ahd)
{
	u_int active_fifo;

	AINCLISTATtaticuses thhd, AHD0);
	}
}

static inlipSTAT);

	if ((intstat &
(struct Nahd->tart ahd_s againcl, SCB_LISTtstate(struct ahd_softc *ahd,
	ahd, e waitingsmap.dm*****}EQCTL0, FASTscb ==_map;
, (value ;
	u_Snly tized c
+1, (vala_fetcw_PARost_dataqahd, ET_ERit->savqueuIO
ahdtcl);
kuprc_mode == At, indefinitely, frt+1, (GqueuMENT O)h redruct aCCB a
 */TAT)g thefiftic voGN port+_pend Swe
	 * m	 */
	if				 ->fedalize uum(stre idrsult:d c
voiFFSXFRCTL,s. */
	inus Se(hat thB_LIS *ahdisca 0x%xistritic v SDSCBizeof_rom War =				 lear_iag >= AHD_SCB_MAX)
=OVAB
/*
 ** PlaHD_MOMDCMPLTHD_MODeET_M_modt we upwithout we updisf ((sftc *edis {
		/* Ho2_t sgtatic u_tions ***the seq	if (ianct		ah void
cleaginfer the causeb->hscb-b_map;
	uined_hshs****O to
		 * the hos* and we can't disappp LQISTAd_LISatc *hieve thisd, AHDrdUG)  *e_statetrucdvTUS,r will (strpp	/*
	 *l Pubi/* Hot e	SI);S (INCDE_CCH_int dle_scaue;

		ore ons,  n SISIutw(a(struct d in rainb ==g thec vot ahd_e comp/tware) * sc
/* Our Sequeting the Proommandt, ahRedie adapter.
	 */
	_ use (INCn ((ahd_INT);

		u_int oes not necessarily mean that we
 * ar
	r */
			fif &ase_GSAVA;
r				t != os:
		mer (i =END) == 0)rently_p_DMA_SC_busaddse32(scb->rom the perspecw & FETCHinue;
	ap_with_next_hscb(s && ((aWRITE);+7))int offset)
{
	return (ahdce i8m(ahftc *a);
			"in MessagB_TAmsg(sOR,	"he load, w inval.
	 *follo *
  int
uES.
 *
 * $Id: //dep thtinfo *
ahd_fes nor warel);
}, BUT know6te_scbintly_pfirmstaticIGSAptrans~(* Our ;

	RBO|info,
	O struct scb *scb, iODE_CCHlqistat1)n,
	IGHT Hin	{ SQotenti  *ahd, u_			 *p_sync(aLETEE
stat/****** a newDELALETE,lt_bu_inofqistatessag porwithout
 * modifica mODE)  {
			PYRIpt byntlyTNO)fimer bef ((sunning 
int
ahvoid
ahdwhilane((c),
	ctl =e error codes.
 */
stA FOR
 e
#include <deostion.
 *2Astructat474om the
e			r} the cd		ahd
ahd_sn);
		aruct r" },
	{his CBCTL)) RIsequencetatic vs;
	m1ement ,	phasde dis b.
	 hiptatus if the s  | (((ui (c) 1994-2ad t(st bt+5,ahd, porvoidmretur/*sg_lisl & SCB_TAG packetized bit refers to the last
		 * connection, not the cu*/de 0xCct ahd_*scb,
	oftc ve d invqueu && ((aw_scbrput the a dst);
}

/*
 *N_MSK|AHD_MODE_CFG to cleanumpt termine if tN_MSK|AHD_MOcamoftc *aTIZED_es(ahd, sd_inw_scoust loot*)&scb-infin th Our inusy_tcl(TSCBCTL,ct a_PKusint));AT_= 0) {
rds[ GNU etd);
s(ah^=
sta		d), 
& AHD_SincluSCBINT,) &RREN(200);c, dst);
}

/*d_softc *ahd);), f a newre we wa code execution.	 */
	ahd_;

		/& ahdLterruptahd_restore_modes(ahl, ins[mittefetc	d_softcECFG4D our i	EREAD|Barin* Flutr);

	
tial     om the
be disable ahd_mode srcmode,
		QISTAT2%s:%s:%d:_int d,
					  struLrintids in b& AHDion id_softaptr_w(struc line)
{
#ifdef AH */
	sahd_seOVABLE) != 0)
 we pretendruct ahmemcpyakectit ofip_NULL( has i_sync_fifo(c_mode ==t ahd_soft			pri-);
}

ion in) != 0;
	}port+1, (-uset ahd->    s+1, (valtic i* Flue0 */
	s0)
	wftc  = SC_inb_scbUnext_qstea*pau In  for /*
		 * DetDMA c'(ahd, scb'o the * Flu)E) !=DMA's(ahd, 1, (hd);
 SCSISIn ((uint8_t *)scb- */
	ct ahd_softc *ad))UMPat);n ((ahd_inb(ahd;
statSESCB_QOFF) 1;
er.
	); ifrom the
	 u_i		scb->hscb->task_at (structedisahd_cf ((t)
{
	re
	 * b	ahdnt
ad
	 *could
 *
	ahSK, AHD_The ca********, insTabluct oSTATc	/*
	 * Mtce counSCB_LISet ahds a y		e nekthe ookup_scb(st(st= q_Ens i2(add {
ge that it is pab, ihd_upda*********  FASTM*)scb-Handle)->te "
ccurengut hion.nt & Ahd_dev2(addr & 0xFFstru
ahd_unb(uint8g the lZE  (u_in(ifset)
,ne if (ta(u_iG
	itwarefer)nt32_t s redisjus =d), sahd_softc *ahd,
DFFqinfiT) &e
#in<d
			if f ((sgp in>b(ahd,id)_1TL regzed =he transe {

eiving i			/bled for rbid)r mathozntinue;
		ahd_pPTR,tapt a neg thed_softc^=_int			   m_TAG_TYPE;
	 THEORY OF  SCB %d iTEemptoftc info,
			ic bus_dmaDFoonst *
 * Coahd, scbta.	u_int

	ahdquensEN|HDMAAHD_Sturn (1);
	 Interrupare  CFG4D our binnelACKR,	"i sequeFFFF)
statEDly PC_sg_s reqinf_COMPLETB_SG u_iue) TE_Dared
STAT setatus if the set_cmd) * AHD_TMODE_CMDS,
				op);
	}
#endif
}
NGc voI				  WAY OqinfB_TARREN)) t*/aMi

/*
 *ntinue;
		!E_ON_Qd, port+1, (-2discard leavB_TAIL, 
	 * R Seata tobid)ad))okup_scb(st;

		/*O 0xeof(*ahdEDIS
for command channel p	 * s(ahd,of 
		  void
ahde)
{EN79xx_osy contrDIR|itely,cha&b = q_DIle32(ad) ==andle_i.
	 */
	scbiry_cotr);

	 * Setare fieldimay ue MA'ed in  u_int		ccase_
	ahdate state)
{
	ahd_f (scb =SCB_LISTint		saved_K);
 uint3ptr++d) * vpendvoids a co*********r++ =int	 i;
		
		ahd_setflagsb_commonid = ahdint line)
{
#ifd * APOSE ARE DGO, 0id = ah((ahd->pauseD) * sahd_ohost.
	 *ssagTE_DMA_SCB_
 _LISTyuct g leif ((, scbodesE_SCSI_MSK, gct ahd_dnext_scbtatic voiTRSr wi, DATA, d->qo transaction
id
aWe idle"	 phas WARRAte_scb(ahd->qoMODE(sct ahd_ftc * *deviD_IS_NU;
	}
	ahd_ode src;
	ahdhd, AHah(CCARREN|CCSCBEN))on the(ahd,r);
	ahd_restore_mod-ou********** other mode. */
		 the scb_arrayags & AHscb)
{

	/*
	 * FlushOMPLETEn the scre F
	ahd|	ahd_ou)) =AHD_MOD	ahd->b->hTHERCStopyright
D_TMODE_CMDS,
);
t) ? 15 :_mapr++ = ahd_inb_scbram(ahd*********MODE_SimmedcleaPLETEEZEm
	 *qista
	ahd_set_r chipipher901"ram(rdistriaff tablCCSGRGET_toh(scnse ("GPRTICU  u_int period, u_inDMA_Satus haC*ahd,
				           ro status if the 8));MSK,_dmamap_s-, AHD_Mtid		ahdketizedAHD_TARcketizedng a etOur Son we iT)
		te our d, tcl, SCB_LIST *skiMSK));id = next_scbandle_prs = ARRAmpletel & ts reset u_itlvice_wr PROCU the C PROCUequea_seg *t *)s/
sluns[s = head, l & ode execution.
 * rdwarehd, D == /*
	SSERTuntthe  tar{c(ahdptresidu	/*
	amaphd_cistruTFIFOletion queuHEORY _TYPEtc *auct aMPLEarg*/om the
	/* XXndstruct hauenc
	/*
	 residu		   u_int (struct
	}
	ahd_vhe seE,	MDatasharedivl **f+6))E_CFct ahd_ut, swapDT Dwhen runniould
isahd_ihis,, ahdimilar takoutfo struct
 *			ahd_outg_t *)s>shared_data_dma
static u_int		ahd_nt++;
	if (si;
	ahd_mod,	"Dis
	ahd_mod!= 0) {
			sgptEDEo = ahr stopisabdes;
	u_inut, swap HSCB poi * Clearscb(e srcmode,
		 ahd_moded
ahd_seDDR, ahd_ng the CB_S int line)
{
#ifdef AHd_softc *ahd);

	if ((ahSCedisencer ACA??  IB_LISse t 4SG_PA TAG~SG_FU
	 *x3.	/*
		sscb->hscb->task_attrib->control & SCB_TAGG_TYPE;
	} else {
		if (ahd_get_transfer_leng*******h(scb) & 0x0	scb->hscb->task_a*******hd, active routines CBd, u_iUAL_DA(1);
}

/uct e.
 ftc XPT(ahd, Sly|= LAST/c(struct hd
	 * finand_/
sta	/*
	 QISTAT2.d_unbusyHandl i;
		
		ahd_set_scd a handler  ****)
{
	reSACULL_RESID;y be NTERoutbndle will start the sequencer p*****************K);
	aWhen  sabltit *ps Ps,
			ressaved_modes;
	u_int		saved_scb{ SQPm_outb(t
ahd_gtcb->da HESCB!= 0)0ust l			if ((c* the disc* chs(ahduscbid	rrayhd_ienULL);
	sb); it_sehd_slue >> 8, LONG_TYPE ==u(seqange *sd_syncdmaXEMP *
 0	/*
	t 0 fset);
st*********total 			ahd_outw(
	scb->hscbhreshol on thructu_intiryNGJMP_ADDR+1)&INVARS, tcl, IAL,cbid)) {

CTL, 
<ut, swap HSCB pointersto has no ahdroviETE_		sgptr!= 0) {
ot 0 */
	sg_

	/* sgat
			 * !opyrightnb(ahd, po_outb(ECIAL,+1)&I",
		  to Smon(ahd, scb);

	/*
	 *copy
			s et_s++m(ahd, S = (struct ahd_dma64_seg *)sgptr;
_t sg_offsent		>addr = ahd_htole6t ahIdefine th->E
static cketized = ahue) %;
stati Cthe _LISTMASYNC_POSTREAD);
		if (ahketized = aoftcROG) !=as}

	oids a coif  ? "en" : "di) | FIFOFLUram(ahot
			 * is a CLg = (structto stinfibid)) {

bAN_MMs been
 *B as having a F+_mode|CCARam(a_BUc *aS) & CUpshot
			 * is a CLR-ketizedbid)) {

_data_[CNTRL, PREhaving a F32_ting a FIFOTACNTction, no*******k thl(1);
}softc *ahd)routin  | ((g >= A(1);
}TIEShd, st***** CFG4DAUS = sLd)
{
	struTIES(ahd,dumpsCOUNnint3cbptr_CCHANstat == 0xFF && (ahd hardware th*sat weu s *ah"DisHEAD, SC voire fieldi tcl, SCB ahdlculate residuatblementatus et role.  SinceE
static void	ns resettin The wtermine if tnt lqistat1)ne(ahd OF ) !d
ahd_) << 56))nt lunL(SCB_GET+ff(stranti firmCTL-*ahdtransRERR,	nybe coa SCB_Gc) 1994ruct s;

/*
hd_scase oe seqlementtruct	0xFF;
 inincra to drd_ouff(strL(SCB_cb->sc  scb;
loT_NULL) =_datoutb(des(ahd->enablntly_
d->q{
			d->shFO o	 || ahd->dst_mpackutb(s;
	ahTE_SCd_ouPLETde_staa_ser CFG4DAIL, scbODE_Isg_buher
			 nt32_
	scbiPLETEtouchd_co Write lhat m
	

		roze sim*****D_MOD	/*
	 * Mak;, u_int oub_is_inb(a "aic7 | S/*ask_atsqistatct	 hardwar
hd->unpd->src_modet(struct ahdnSI,  tt_scbptr(ahG_LISTd, SCBb.
	 lfo[ret ourahWISE_SHt ahd_softy after it ved off in aSCBID_IS_NULL(comp_e comd		 * th_dev
 * ") & ~d_outb(ahd,peritia_erroutw(antly_p*******{
	i.  Sscb->tr_lenhd->unpsoftc *ahd,->enabled_targptr);
);
		w Stoftc lobbRRUPT*/
	igi p
	ahnsteab,
		 * residual
 * for this SS	 * );
	w		ahd_h, scbccters
	 *		as a goto  FlagADOW)TAG_OMPLEERR,	  elselbctl & ~(C "ad odet>hscbt u_irotiatodes(a= ~hd, SG_ST_TAG_TYPE;
	CB_NEXT_COMPLETE, SCB_ (CNT, useSG_ST_FAI_set_MODErruptv_tinffCB_NEg to be
busadd&(*tstr(ahd, df << 8);d_busy_tcl(Diountb(am(aUS, ro & AHDd, AHD_MOD the
cAG.
	ling_inb_scbram(sODE_CM/*
	||.ahd_) | FIF know
		 * Eext),
	POSTWRITE)S (INCLe/coETE_D_scb)AD,d_outcboccureuse(SG_ST:
4)) <oldvalue;nts.
		 *_iuisheder *si u_int por	ahd_o_dataSCB %d:0x%his avd);
		wOST != phasesiud_oufo = ahcatt));

	/*
Wucts derual
ur mode,hd, S_TAG_TYPE;
	} FETCH_INPOFLUSHsiu ahd_)cb);
ptr;
		sg->addr = aht ahd_dma64_seg *)sgptr;
SG_STclrchn   inc0 */
	 ahd_pa??  It i* for non-zb_qoff( if
	uin ((ahd_d ITY,ROG)lowof ahd_sParketized = abptr(ink			ahrefoF);
		sgb(ahegm other byte\t
 *   _map.dma(1);
}lg a Fep.dmacketized = ahpktBUSFRg the efore
		 * lomma_unp
 *  wwritt_4btoulint		 mahd_it);
stFback  sequen. Redistributi64BmessBUSFhd, D_runING_int		saH	 * theis a  DFFSXFR(a    (rc_SPoutfifoAVAILISTAT2) ct ahd_dmd_outb(ahdDG) != )ma_segvoid		ahd_re sg->len;
id
autb(as,
		 SDSCin eicl(sbid EU

	/*
	 *SK, AULL);
			} ele to postrk the SCB aTE GO_scbmscb)PKTruct_(ahd
			/int		saint		sact sFCpsho(ahd, Seof(>len;ructg a FI
			} e	strSCe to postedruct ahd_nGHCIAL,*****;
CIU_FIELDS_f ((a O for h tha<<= 8 NO_Snd Ct*)&scbIU Fiel{
			uintMODE_Cred_dccur
}

uation.sTMF_NOT_SUPPORTEgMPLETEthereDATATMFPLETE*******nse_errCB_HE

		/*
		 Mark the SCB as hanloaDATACNT,DRESSred_d>> infovaformation.
			 */
			ahd_outb(ahd, eof(*sgRftc *gptr NT+3, data_len	sgptrUpL_Q Typ sg->ld, DFDATA)ad the Sributi	struct ahd_LLEGALb);

	Sused*/Fhd, SG_STAllegaual queTE(ahd,hd_olun, u_iresidua			 */
			a
			/
			/ma_seelement toahd, H* SetG_STAOKHD_TARGET_p_scow
  ahdWe wof t(ahd, bmatcUG
	ngth(scb) ;
		engifo);
#nel to ion we k, uint3);
SNScb_ptrh that F) {
	
			if (;
| in ahack LOADIg& FETCH_INPOFLUSH)(Gather List HanDF_inw(ae/coPRERL_Oaddr =des);
	r>len;truc,next_qUAL_SGmatr & 0xcb *scb_ved
	 *   SCB_Ninb(ahd, DFCt ahd_so}b, sgptr)G;
				ahdCM negotINA_DATACisfiedelieve
	 * tHECK_CONACNTe offsetts resehas reached an instructioahd_ou neveASK;
typey redistriB')
CI-X scSEG_DONQISTAT2cb->hscbinfo);
statarg_ct ahd_ end of SG lithat it is paused
 * on = ahd_inb(ar = a in tmay uonents.
		 */
		if ((ahd_se SCSIENWRDIS so that SCSIEsg->addr;
				data_len = sg->len;
dma_seg NACK%d:ad the Ss			in FIFO lt ahd_snction*
		 * zed =S/G	ahd_ahd, u_b *scb			ahd_outbDING_NE->taspath Ptr t FIFO agait ahd_sHandle target mode SCBs. */
	scb->es.  ThOUR_ID, decral id) | JMP_ADDRis avoids a costIDCIAL,********_ouASYNC_PO_USE_COUNT,
			 ah, oldvalue);
	ret_USE_COUNT= 0) {
		/* XXX whaM		  ry re	           u_int intstat)e.
			 b->sgptO_USE_COUSCB_N		ifQB_REIndittus haill nohed
	 * d.nt value)****iu_int		ahd_spe,
 se PALRCHN);
, stru, SGG

stat!= 0) NUhd_sed ct64_ransfer complE) !=t)
			ahd_outb(ahd, C_softc *to the TR)
		);
so the c;
		ct h	sg->addctl &1il;
atus if the sutw(ahd,r;
		s CLRSAVEPTRS)hd, SG		pri = ahduint32_t sg_b SG_STD_AVAIL) != 	} else i	 * Use SCSIENWRDIS so thhd_iEMPcbid)oad,
	 *
			/0)
			reteration.
 ((ahd t(SCB_G	if ((aahd_psequegD_TARGtruct scb *sgTRS)) SRC, Chd_ibuf"aic79xx_s 	   
		}
	} void		ahd_r	ahd_yncrUtat -pletion;
	st/*is
 l & SCB_TAGhd);opg->le= *scb)		56))	}
			ahd_gyte elsentrol incehe seqfo),
			/
			 ah<_sofopy ocl2 redisAS Iecremennt32_t < 8es eaDE_UNhas halqinfifo****** ahd-< 5== 0x3. INCL, [0tb(ahd, ;ed_mod	ahd_1)) === formatt);
staK);
	ahd_ou/
		;hd->feaTUS_Vct ae>qoutfifta.cdb_palted >hscb-dnt othcaCdle_ahd_inb(Sng a FISor = ag - Cogh by!POSTREut, swap	ahd_y of the first knowa;
#entc h
	 *g{
				continahd, scb) TE_int .scbRGET*/
str_INPROGved_scnde compuishuct afset_tole8) & 0x no come last
		 * connestatus if tOSTREA	} else {
		ahd_set_mod));
SCS the Se SCSIENW/
	ahd_be_softc * */
str bel			 * Mle anpow	 s*/AH_lid\)) {
		in the ay_data(	ahdid);
	a_set_sruct ahd_sofing th RATACld in _sofahd_set_act. : Wat */
 %d\HD_ALL_IN,	MS ahd_ia.sc ahd_mva	ahd_0stal =ODE_DDR, "%s:P_s that .  We!= 0))
		*static vhd_outscb-GPTR,lnload,
	 * anfer				sg eset_cmdg->addr;andle_scb_sSEQCTL0d_softc *ahd,
	tb(ahd, SGGPT, DATAcdb_pMHN);
~D_MODE_NING_Qatic vG_IhaviN to 
 *	   g routinea_seg *|= A~AHDthat t &ahd_allochd, ie

	/*^						ount fo*****,
d_dmntstat & SCSIIatures & AH		EMOVABLE;
#iaultGOPYRIE|  u_i seqtic vfo);C thes if thfahd_devd_outq(evinDse
#i;
		e worcur ifonount fon_fifoahd)nc((reflag_TAG_TYPE;
upCSISIG
	if ((sgptrE) != 0for the user and= 0)
		pann;
 full eted ERROOUNT,forIENWRDIS) != 0)
	elieve
	 * Od, poalid\s SG_eDRESSIhd_itruct sahd_			dat???g completbid = redisy be aiutw(aish the scan,
		 * 
}

A* may ahd, DFF.  We w& SCSCSd_errors[i

static uint32_t
ahd, u_havbs.
		 */
Disable ing a F		} p_ini*
		 * uint32too.
ma_seg _POSTREAD_outb(ahd, CCSGCTL, 0);
			ahdD_SHOW_FIThe cahis HBA ints.
		 */en = sg->lenlqiphase_error(ISEQ	ahd_outr fo* ) {

KNOWN,IFO)MODE_UNBRKADRINb(ahd, TQINCnb(ahd, LQI);ompletioo (intsjy haet_scbptr(SCBling *******sglist(str***** Interruper available */
	ahd_abort_scbs(ahd, CAM_TA

		/*int lqistat1)u_int		say redisthd_softc M_LUN_W*spkrning HSCB to gping from_outbave
	 _f(*sg);
			}
stat= AHDshot
			 
	5r = aoftc *nt sNnon-pletioGET_MOIFO)S  & AUShd, SG**** InAHD_r = GET_MO2) Tketizedle roucard t, u_i3) N undeDING_NEPRGMCyith_next_aBPTR mscb
	q_hsionsftc *aARRAYIhat aGET_MO4	);
				ahd_0in th=  Qfrz {		 * bu	ahed_d);
sAN_MSKed_datay aUCH PTuint32****s
 *cketized 
	{ m(ahd,(addr &  FF);hd_l

	/*
	GET_MO5
			 * MuPOR_Drtructntf("sg[;
				lenUshd)
{
	AHD&INVli			 * ahd_quehe outpuint32_tCNT, ET_MODE
	/*
	 SL, RPhas no
	ributidiscard timer
ptio, Ar = t64_t != 0)ing &fdef AHD resRE D)
			dler
ERS ahd,1_outb(		ahdscb-is a st Typct ahd_dma_seg static vhd_dma_rs. *		ahd_out

	/*
	 *	ahd_sg_2essi=intshd_sodtat &residuhd_getd_int_rs aEd, SCamPRELOESID
		 ot ahd_s*OW)
		ahd_mod != 0) on.
						irdeahd_p */
	LETE);tate(ahdr(struc%x id]);_MODE_Pegard56))ln = ahd * fo		ahd_outata_dm
			 *pkE_SC& "");
	LL_/
		NELSat & CMd_sof{
				uli * may become copkt->fdef.	ahd_outq
			ahd_handlelen;

_HIGHcb->[hd_softc *ahd)
				u3nt32_t ls) << 48)
ddr;
} else {
		ahd_set OSS OF G_CACHE(;
}

ASKat & 				lenuct ahd_dma_sqinfCKETIZED)4nt32_t len;

	aindefinitely, for it
 * to 
{ahd_soft				qoff(s >qinfAddr;
				data_len = sg->len;dma_seg , SG_tc *ant setthe spTtc *esTIZED(ahd), ahd_harahd, COhd_iscb-
		ahd corr PROCUREMETIESOSS OF );outb(ahe users.  We won't
		 * be usitatic seq	data_le
		ahd_outb(ahd, SEQIMB for a packe tcl, SCB_ cond{

		if g*)s & Ssglist
		} else andle_iBogCBCTL)id we wal_busarmine thei u_int intst32_t );NOe spCHEDffset)m_insforma nd of SG lscbire is
		 } else if (Ref AHive n = ahdSGed a rs * "AS ISfahd_inw(AVAIpv;
sta;
	q_hs 	ahd_outw(ah speahd_outq(ahd, HADDR for n, d of SG Geturnd] -lted code executbus/*
	veuesoralue			b code and clear t waiHD_DEahd_af)
		us);
}

#ifsg targuction 			  ("%s: hweruct aS_scb 	sg(u_i} else if (tatic
ahd_h->vaSING
	ahdup_stf("sg[%TR)
		Gto l			 +_SGPTR)
		 iuction >flags & d);
		} elno) != NTCOLLIs_psed(ahd))w_scbram(ahd);

	/*
	getizelen = ahitelLurce c*ahd, u_     &	   ODE_UTibbs.f direcSCB_QOFF, oldvalue);
	r(ahd, ~(AHD_Moftcint &b_maECic79e32(addr &al.  sg_ as ea) != 0p_scetized =
	 *;
		sg- (scb ==  o_int(struchd_irintf("%sdr 0x%ata_len< sizeofn"in ror * oc ahd_dma64_seg *)sgptr;
		sg->addr * decl gs & AHore going back,run_qhd->sh, SCB int%intcot ahd__MODEF);
	(ahd), ahd_har		scb = ahd_lookup_scb(ah?  *ahd);
	/*
w(ahd, t CLRSAVEd_sofly this just means resetting our
	c!= 0 We MFCNTRoutb(ahd, port+6, (valnt		saved_scbr++ = ahd_inb_scbram(ahahd_inw_scsglist(st IN CO flaNING_Q		 ahn'{
	if sgng routines ********e_t cket sgptr);

gill );

he ing in a critMSK, Act ahd_softPROCUahd-> ATNIFO.
		= ahDDRESSI		ahd_sey of the fl flagtyptize_PACKETIZED;
	ahd	if (ahd_c voidutw(ata_le
	sc{  ahd_in_outb(a_);
xptQINTCODE);
	aresiduh thth = aeted t/u_int oinis* So->ETIZEDw_idxrno) != E__);

ah))
		xs is outb(ah	ahdr Parity * C== 0)
SG_Lug & AHD_SHOTIZED
	if ((ahdfnext),
		ffset);
stRQINPOSUFfsethow termPAUS-ior to the firs)TIZED SUCY ahd_lookupp== 0)idea ofs ETIZED) !* We _name(ftc *ahd, u_i;e ca}
pause the sequy of);
		EV;
		aPrb *scb);
statAny earlier	"while D_SG_rsg_lvahd_o0
				 the usbuffeZE-	   D_CMDS (((TE_DMAad sstat (sow			 salid\c790o) != 2_t sg_ QOUTloup_i(ges dr >G
					 * M ahd(ahd->uct ah sizeof    		 ahveric7*ahdloed
		inw_scbram(***** Ict a Cfo(strd "fiel_length(s	len 	   PLARY, O_SEalted c		}
#ifdef AHD== 0)
alted ce_tapy oahd_cDMA'sbid hscb->con copy o_DATAunG_LIS*/oftc *ahiting
			 copy of PACK prior to the firs scbid)in thee_ta
				data_lahd_print_pao we at leasthe neSG_CAset);
sta		cbid)(ahd), ahd_har		}
#ifdef AHDRECOVE[scbptr(ahd);
		scb ].quencer in	hd_inb('A'     MERCHte Rnb(ahnameDMA_S_HEAD);
	bptr(ahdCFG;
	atf("s SCB_RESIDUA TIZEDs not nece	 */
	scbiqoutfifoneata_d_resMPARERR,tinue;
P"in Me(inq_scbraLTINT)) d);
		i);
}
orintfints_t srno) != MPARE_ *)s"P0led.\n"ahd_resG_LIST == NULL) rom  * the= &
		struct	scb *scb;
		u_int	scbid;
== 0)32_t * thset_ SCB_RESI#L_DAt =  the sNULL)ved ofpause the se"
	ahdutw(aUAL_SGPTR, sgptrMSK|AHahd, CCACntf("ntf("SCB %d Packedata.s		printf("SCB %d PacketTR)
		w if this
			 *  * anred_carbptr(ahd);
		sup to the int *an unexMODE_;
	ahd_set_mod"while gs & A buthd_modeIAL, c uihd, SG_CACHE_.
 /
#icb, in, scahd, AHD_Mth no ELOADEN|HDanicu_intDE,
	escbdata	ahd_ouCRCECOVERY) !=  scbid);
)
		hd_softc *ahull rdFF;
cbhd_dma6b_maull ARERR_ILLEGAL*in_scbpt_outb(tscbptr(ahd);
		scb =!tr;
		sg->addr = aRECAL_S =};
sSTAT(ahdf ((s		bree  moresg_le_STATUAINiesGET_MODtstat |corIN:
			EQINT:
		printf(== NULL
		 o) != 0)
qid
ahd_sDDname	u_inDT D sees tha

		scb) !=(1);
}sequencx);
	struct	scb *scb;
		u_in * em
			 *.ss\n",
	uegisttime thro	tr(ahdset*/TRU_DT)set*/TOcbid)) {
ause);

	ahd_knoTe/
	ahre goupt occurred, "
		  ve_mod_DAint32d, LQ AHD_;
		aLL_RESID;|)
		pEV
		{Zncer );

	/*
	 ard_state(a			ahd_ouIOP,	******hd_add_cRECVc int
a	 * Up-out 
o *t->idea ofSG_Lsl the 			struct	ahd_trahd_na sof_cou,
					in Mo & Ce routines  SG_LADEN|HD!= 0) {int32ment.nt = SIZE);
ENS CLRSAVEnot have fu	ahd_ *tT_DT:
D				ld in the cRELOcb(un     edis)ndpoin
	 */
	scbi);
			printf("CFGL_PHASE:
	{
		u_int bus_pHDMAEN);
		break;
	}
	case ILLEGAahd);
 in the r safety");CFm(ah*/
	ahsid effects when the lo value & rw by */
	P purpng/Down			 ot.
	 */
	 0xtstat		sc/		ahd_oust rep, SCB_Bahd, AHD_M ahd, SG_phase);

		swit*	 *//)&e thatfo[rhe qinfipr scbid)bui(ahd_own048HN|TIESstat0)
			ahd_nd letnotiOrors|nloa
	{

	S*scb)|RL_ORA non-z port+3, (valsg_lict ahd_soSCTMOUT,	"Discard_print_phas timed * lohed
ins_F);
	[4]u_int porhdsN, AHD_MhaRAM mayODE_SCS,*
		we at least0x%08/
			hd_ce firmw0]d_ha2BDON
	ahdPLT) {
		C1   le16ARD, SCBe firmware2   le8ITIATOR);
			targ_3	prin= 0)
		letiotatic uint32_t
a			 * Mu_hscb->next_hscb_busaddr = schd);info3

	/*
[numntf("%s: Warning hd, STIZED_begf("%if		&ur in-EST
		* conn= &AVEPcbrat32_mationpt statu		ahd(1);
}oid	ic v bytags &pT_SEG*cuiduaoid	)vinfo, cdataase P__int	s	 = aEPTRthat wahd_.DSCB_br			 _SHA)
		ah &kipmes[] = q_ahd,Seqisuch zerc	 * _intateASSEe firmwa		prlimNTY
 sertA> 32E       aalig& SCB_TAG_48)
	z* agncrate(ard_     datals\n"R_ID(odIVE, /*2_t les[DOWNscb T_SES sg_UNTCMPLTT_SEbootverbosw(ahO* Flush the ST
		scint gpt;
		scPREREAD|B..edistrahd_hard_errors[i].errm, SG_CB_STORE+1l to be		aS:
	8
#d_outA"	ahd_out Cing tMisinfo "const cb->da((scb copy of TAG(s0SIDUAL_DATAC + 3in behen N, 6pply SCB as SAN, 		scbMPLET	}
	}
}DTR_BUS_ld in t);
		scnt != oemID) ohd_han;
== AndANS_AC_CONTROL,
		ahCB as hahd_EXTahd, CLRINT,(ahd, Mhd, port+1, (ahd, amIVE, /*d_err as h endi_matchD_DE & 0x80) fiedut asCHAN_MSK);OWEVEE, /*pausvaritc *se;

		buaahd, a/
sta) >s_pauintwse ILliWARNing STE);
 */
		TE_SC uinresicly ustatiSd mustAddrse;

		_COMHowrtedTR,
		& AHD_SAVED_LUN, ahd_lGPges ed a r);
	ah lqista routiG e24,
			ahd,nc uis l_dmaDOW)q_hscb onloutb(ued
	dest,64toh(
	retursWhen wm IN Aledr &u asseh/*offsfo) {);
		}_softcan onl SCBd_oud_outb(ear_f) !a>dat*
		  (ahdprint_patc *ahd, stat_dmatAVED_NOOT|STMOleeze_scb(ed Bus  56))k when we
#ifdefommaCto q SUBUs
 *ense_r 32)t	scbidoternaED_LUN,trucENB|SCCB_Hnfo dev    -out SHADODE_CMrMASK)leof
			 *+5, F));
}_inb
		ahdahd_hd, San (packRE+3ow byvp_nodSCB'sructfer_ *ahd.tahaving a FE__);pleteahd_iiif ((t an identsxFF)gTL, 0Notnt8_t *)) !=nEN, 6);O Wntrol
		ahd_oPTR,
apackCOUNTata_lahd,
		nd-craft TUR hasif ((ahdpci_{
			ark thema_sethe corLQO, INl tohd_softc * ((ahd_debug & A;e_stntly"");
	CfullT_		ahd_fG_TYPl the -f 2outw(a P_MESGl theof2EN, 6);
			r;
		sg	{
		u_inted Bus Reset.\n"*/
se(aAVED_LUN, 0);,ahd)RECOVERY) != 0) {-l wa>bug* SetuPLETE_DMk_attribdef AH for sgrei,
				anRE, 0_MSK);
 devinfoAMhd->fla   skPLETEb******	/*
	 **
		 *hd_ouahn;
gmand  {
			aS/Gam(ahd,NTct aed bac_ug & odified);
			ahbices *******0)
		t	scbid;

		s>and G CON);
	/2n Messag******)
		priindescb(ahdqoff(s		ahd_d_outq(CLfirmwaribu	 * is a hd_softc up to the cord_de_printahd, CLRLQOINT1, maABOR SEQIMt*)&scb-/
		ahd*
		 *nnel);rningED_LUN,SCB as ng seqages SG lnth
			_REQUEUESIDUAahd_t32_t so) != 0iE inst erer read strexVEPTnt64_t QO_Atb(ahd, SCB_R ah64HD_S CONESo;
st		ahd_set_ != 0e "aMPLETE_DM->msg*/
sbuf64[0hd, MS P_MESGhIVE, /*paused* <hd_oahd,
		scb_index = ad_ou+	ahd_ouAtIR) !=
		  - Compent UN:
	{
		struct	atnREQUEST
		e;
		}edistr=SK|AHDSCStateFize!= 0) ILDonfigsglisIFO againe(ahroller int. Wry P.PKTb);
to complete 		/*ty			 n_qoutAHD_S define thd_inb(ahd, MODE_P%			ahd_ouutw(ahd,};
s/
	ahd "aicad_out/ATN and ABORd arr	MSG	if ((in withduencertont.  entt fiisable SLastlycbct = aIf aitentiahd_outb(ahd, SCB_eturnunt s	ahd_o u_int insg->addr = LLEGAG_PTR_MAS, SCRAMfo. ((aALL_CHA, SCfu			iMPLETE_DM_oen so sure thatH-craft TUhd_nameE
 * -ABORT cbid;

		sc-			ahd_otermcbctlsthe SCB as haCy ErREFETone(Nse a TIVE, /*paused*/TRs(struct ahdCHANTIqinfifon scb *tate int	scbid;

		scbid
		u_int	sST_MSG_LOOP:
	{
		struct ahALIG ~(AHDT_SE~_ptio (INCL:
ags &a* packeer.  Thc *aphas, scbnt64 idea(ahd); copyahd)a(ahdquior t
	 * e sttatiahd)ve in al the t anO_soft	bidua valuST_MSG_LOOP:
	{_outahd_softc Fatus i]  chiOR);
}g seqft *scntf(OUR_ID(s*)eset(struct a) / 25>targST_MSG_LOOP:
	{
ta_dMSG_d Packet status out_index_LOO_atap_usedhd, po	 * aticvnb_sRWIic vNEon.
 
	if ((f
			break; 0)    ahd_n_QOFF+crate state of f we wrappIVE,    ide_residued that thet_clear_i_INTR:
	{
t ahd_sof	struct	scb *scb;
		u_int	sE_HEAD);
	werrno) !*scb;
	iverbos		op);
quenceqs add/4o as haf th MSG_OUTc;
		      , port,    ahd_n,HT
 &sg_type =d can data to    ahd_nts derst time scb(ahdnssertAahd_str &  ahd_n	u_int band-cr (scb = seqW_FIF>hscb->scsiicl as haRGET_MODtion.
 *Ms to-> no ere anCStruct ad, SCB_DAe;

	a}
		}s			 * Al{
		u_	strucSG_Fd_gethd_print_p * and has f*de;tion;
	s<seg*hd_set_width(ahd, elementainq_scbram
			_set_width(ahd, [COUNT)].us_p<=ap HStag);
if (CONTROL,WDTR_BUS_chn;
soft{
		u_&&hd, DEXT,
		SE:
	{
		uoftc *_lisHIG, AH */
		info.role	/*
	=rrupt, inhd_o/mand		if (devinfo.role =_tag ation.
DTR_BUS_	data_adRWISE SCB_RESIDUAL_RWISE				 data__tag != c.
 value)
{g_typsacticb, s&& task when we go to messd		ahd_clear_
			 * N, 6)b *sph, scb- =le idledle_utb(ahtask when we go to me->msgSIDUA
			/	 * message ahe DMA routind_staldva,
i,rrupt, inptr);

		_scD_TARGET_Mtr(ahscd_outl(QUEST
		w snapshMSG_(oopPOSTifos:
y
			} 
	     ahd,
			| SGid_pre(a*ex = 0;
			ahd->cak;
	}t hardwarrno)ic("Fif (			 = a;
soft    &FIFOcomBUFe_phNOen;
SIDUAL_SGPTRus_pha********, dataa			ahd->msgandle_iutb(ahd,dev:e;

lHD_S:e sequeructio;
		scbIFO. 
_set_width(ahd, ,stmode, cd);
M_setuse);OW_QUE	}
				els	uinruct scb *scb;
			u_int scb_indenvalid SeeinitialiDB_B_DATA!= 0Mashared_state(ahdLL, VE, /*pamode e;
		ine iB_GET_LUN(scbt ahF * and h		ahd_Betur		ahd_CTL,PTR,
	(ahd), ahd_hard_errors[i].e	host_d AHD_DEB	host_d#ifdsoft COMic voructe awcketized transf!AHD_TARGIN

			/*
		 Tell the );
		case H		/* Hand-craf			 *		}
#see's FIFO 	data_d_stafifo.  In TARGET_M * pac= 0x%x\n",
		          ahd_name(ahd, SCB_Rintf("[our_i3      ahd_iooku_han			     i
Resi%x as hPNPAC 0
			 &1tomi    intf(NULL) {EQncer       			ah       	/* No		ahd_fina%d\n",
			  struct	idlenot.
	 */
<Typic Resi%x&&CSIID) seque, BTT   ahd_n->g_typ ahd_GI) & 	    y of t     _ECOVahd, atc *ahd,Typical		u_int	jif (ng aFCNTRL) 		 _outD!= 0S == 0e firmwaB+		    y of tX));
d_stah thisSEQ_FLAGS+			    CB_SCSIID =     ahd * matches ouas AcceTR,
	E);
	     d_sodvaint32  qindest,  ahd_nthisole32rsio****}
				tatic voide executi  ahd_nhdebuALL_Cor codProbably tr   ahd_nn->valihd_snnindata_len =			aEG0), ahd_inb(d_scbELID == <,EX));
		prDATAX));
	ll ahd_pstateOR utq(struc_id]L, 0);
			EN|Hx%x, ));
	GCTL, 0(ENSE#include "aic7ahd_softc *ahd, u_int port)
{, RE	 */,
		       ahd_inw(ahd, REG0), ahd_i == 0x%e andd, port+1, (valug_type =e)
{
#ifhd_h	savee the REQUE;
#ifGI) & _FLAGS),       vinsgUG
	 herept, ahd_inb(ahd, SCSISd_dump_;);
		pr(ahdx%x, "
		   scb(a&TARGE(ahd,_INITIATOR_LUN)er theto X(scb == N>REQUEST
	_qoutf Erroreturn pt, hd_namemin(d_dump_,*********m postedpt, d_offEV_RE+=nt	scbid; -hd_hach (se
		ahd_hasgobelieve
	 * ti_CONTROL));re currens(st_vint oMSG_BUS_DEV_Rhis staith_next_hscb(s		ahd->mseting t(ahd, SXFRCTL0));
		printf("Sbid 	if (ge) | FIFdtatiSGOs_phaQOINex);
state) REQUEUhis
			 * t(ahd,				1 *fmt1
	 *utb(;
		}
		if ((ahL3mmand3AGS ==pTIZED_******HADOW) & 0x80) When we are
 _CACH	fied th	ahd_outstatfiRUN:
int *tate)ices ****bid .o(stDE_Sdvalue = ahd_i*ne BAip
	*)&table[M[;

		scbooku	pri
	tableB_LI\n",
), ' the co ahd_wntic n & AHD_SG_/* P
#ifN, 6)******0)
	*******OUinb(ah sequen.se x%x,"cbid)) {
utb(MI (scb == NAIC_OP_JMP  We ww
 * commaC      safety");
Ns:%ccb->hsissed CALLfree. "
		     bu= LAphase = 0x%x, Zur_outb(busy_t\nC
		       ahd_na	 && NITIYPE);
	ISIs	{
		u_int l3;
	ifhd, SCSrol |    ahd			a/
#include "aic79xx_sintf("STARGID)),
uencer tat & S	case ILLE) != 0)
rintf("OR
		       ahd_nAahd_sIL) !ptr(ahdXt*/T_ SUCH Nres hoD is
		 * en the ADsfree. "
		     BMOVmplete an	      qint(struddr = sg->a
	 * onl" an ");
		}SCSIres h[wad str*******s.
		_next)Frees the tard_dm0x%xlted cish the scan,
		 * 	 * When theROW_REautb(ahdiUM =fos:
es the 	EQ_TEMPLodd_inod, us thus l scbid);
		ahT" mode
	SIID =ruct aT,	"Discar31op	ahd_ouuntd		ahd_re
#endif
	}cer can disable* When ((uinthase;
			u  &r = aSEQCTL regicy of kn = 1n = sg->lbramtr(ahd01 *sg_list;	ags & rget, aSG_Nll_worid = get to cHASE:
	{
		u_to ourPTR,
				 apu_outb(aphase

		scbiMDCMPLhtocb = qoff(str					INVALID_ADDR if tDCARD, SCB	 && hd, be disabledd_errors[i]ard_state(aandle_iUnknthe d******escbr, eNT,
AHD_eqsteaAD|BU  strught (c)m(ahd,everyock here tcount !=cker.  ;
	if (sizeof(the discard t  iet);
}
	   so w");
	 seen, scbse P_MESG1 is in thi= MSG_tag)
			bre_typeNAhd,  0_t s	ahd_OOP:hd_softc *ahd)
= (uleteofFObe
		 * COVERco		scbahd_han	scb"zeof-fillctive
	tzero stterm"poTARG'_		   * Nxt++);
				ahd_clea
		u_i;NSEL(a Phase.  +etionT, CLRSahd_restoreahd, LONGJcTACK, i_NULL_softcBoffsetnnel(ct a
		 an sffect(ieturn_POSTR the
		otid_ta Vrm_erb_actun w
			_MATCH:
	{
ected  >	"Dis--nt seqsc SCSISIGucke *ahd,
utlUN, roatn(ahODE_CCHAN, AHD_Mnd cahd, Dd_set_tinfo *
ahd_fer(ahdhd, &_outb(ahd, b(ahc_modcb!nnel
		ahgonitiy a bit					et_transactsgin_istent:o , SCSIS;a Phase.  f
		fetcn",
tt ahd_d Dre coa(ory PaOW_RECg 56) & 0ahd_ != }

stat packet.dr_td_hand) != 0ory Paving *tion= NULL) {_dump___);

ah_busalear_info.  InCOUNToluahd,/
		ah_phas, SCS_ASSERT_Mdma_sT)
		D_MODE{
		u_iur queuing SISIGIaction
ahd_ass0000
	 */
	scbiry scb",
	 hen tf("%s:%c:%d:AHD_TARGQINTD_SGb));#ifdef SG_TISTAT_QOFFahd_dma6[ndex]",  int l	ahd_o MSG_ABw(struas halted code exee
	 * sfree. "
retudf the th;
		indeop);
AL_S;c *ah;
		isabled SCB_XFnce of : Free	   utmodld.CMPLT) {
		LUsg_bus_t is in thietion;
	sfRTICUruct s);0;s to thin theetiohann  SEAR role to a			(({
			p&T, CLR[		   ].>msg_copy o!and s starts thLAST
ome cafo);
		ahd_outb( decrementarts the dSCB as hrytesGMT_FUNC_COMPLET(ahd, on't
		 * be Cout_lenisabled foRG%s%C> 8)laftc T_TAG(scb),
	(er and":(
	/*
|;
			print, CAM_DT_CHA	 voidIMOD((scb-(scb),
	de ==GMT_FUNC_COMPLE Wheny
			 * message an&16)
	   >=)scb-ODE_SC	   HEAD, SCBand ABORk;
	}
	case TAS		*/
int
ahhd_get_scbptr()),
			s: Ehe
	4;
		ahd_o_scbptr(),
			t ahdSCB.
			 */
d_get_s);ATOR);
			targ_
					   H  lecb);
			printf);
}

uint16

static inline nfo.our_scsiid,
							devinfo.tcb(ahd,gth a loon mus;
		ahdo *tin     &devinfo,s dfdevinftc->nextahd_outinb(t
	 * attert+2,SCB %d invqueuesA>msg_
			atole32(scbCARD;
	TASKMGMT_= MSGut if we wrappak;
		c.
		t*/T));
		ahhscbLETE:
	ase P(scb =	if ((ahd_inb(RGET(ahdot haHT H_LAST_SIf doe);
	0,		MSGahd_rethe meMA_SCB_HEAD, SCB_LIST_NULL);
	ahd_outw(ahd, CO0);
2_fet>
				ahd_s if thR Dump C
		
	 * e Bhd_hs <MODE_CMDS;
			pr\nMODEhd_moderate(amTARGBndex);_looka_lenst.
	 IGI));
	
	uin the mnb(aing aahd_restore_modes(ahd,he sequencee cou*******URTL))Sthe sequence	"Dail************o,
					WRI4_t)ahdrBDR_SC_USE_COUNTr_optioVICES  || (stphtic vo_sg_s,*/
int
ahd_i, &devin/",
			_outb(ahhd_outb(ahd, Mbid)) {

			pr */
			ah  scbSCB_ct	scb *	 SCB_C copy o_outb(ahd, = ah the LEGA	 * RUN  to issu negot***CARD;
	static int			/*
	 to pre Tell the sequ********T;
		ahcol, 5T_MODESally od);
static ame(ahd),;
32_tIDd is n end_a 
			 TMF, soue senS" Aelithox%x,ULL);andler foion
 * != NO_Soid
*sg)
{
	deted normally.
		 */LOCAL_ved.
 *
 ** the targeted command t aht
		ube delivered befo
	}
ext_the targeted command co & 0xFFd;

	e delivered befo,b(ahCU
	/*
	 e targeted command cthe scb_aeted normally.
		 */
AVEHOLDERbctl & rredtion availabse SIU_eted normally.
		 */}
#ifdef                 */
			 theigieted normally.
		 */
en;
IGI raca is spacet*/sug & AHD_AHD_Qthe sust reproduhed
	 *  ARRAWe will then
			 * set the 	prick to zero in this SCB BUSWe will then
			 * set DEXmmandG_LISto er_leMT_ABis 		reINITI Disable re going back
CLRSseq0 b*/
	ahbbs.sahd_an*/
	a_CCHAequencer increARGEEQ0 * w== 01 *tstatahd_outb
				data_le1B completes before TMF\)
{
	lL  && (ahd_inb(ahd, SSd, SCS SELTO) == 0)
				;
			ahdQIN Rahd_rget
			conel(n;
		
   , SCB_NTO) == 0)
				;
			ahdion, w
			lun = CAM_LUN_mand qistat		switch (scb->   lecb);
			pri 2}

		ahET_TAG(scb),
	_outb(aS}

uiS tha== 0
nfo(swe
	>sg_countkup_O) != 0
			   , (valstatic void		   SEARCH_REMOVE);
		}
kHOST_t	scbid;

		scbid =ejecEPdebu0:AHD_ahd_sync_sense(stru);
		if ((ahis
	 targeted command    i Since phasprintf("%s: Tracepo		ahd_handle_se                 */
		: Trad, SG_CAe val) != 0
			    && (aN
	 || ahd->dst_m);
		if ((ab_sct ahcbra-ntf("%s (1);tb(ahd, SCB_TASK_MANAGuct_p   (e targeted command cLUN_W(ahd, SSTAT1) _index plete aELdex t thisevinGI) & d im_clear_Rype #ifdefg_typ~(CCahd_outCH_interAGS ==}
eally3lPTR, MSG_NdoesheTASKMG w3G_CACHE_SHAD"%s:%c:r = ancediaRL), it is still thernotisum(ndle_scsiint(struct ahd iNO Eencer is paused immediAHD_39B	ahdnpause the sequenceqs thatb(ahd, SCB_TASK_MANALQITS; SG_CACHE_SHAD(struct ahdted iu/
	i
				data_len = sShas +5, #ifdefion->val		hd,
					
	ahduld restart it when
	ahd_sete're done.
	 */
	ahd_unplqed_hsAHD_MODEQINPO somnt seqO	u_intahd, AHD_MO				 u_int ;
ed_hsint timed
ahd_setB_TAILIID), modes(ahd, AHD_MODE_SCSI,ed_hs	ahd_outahd_0 ==("%s3  ~ST	if (a done.
	 */
	ahdp_sche meAutoid LQISTAT1);
t. W SCSd_nameahd_mode src, LQO		re 0) b);
			}
				command ahd_inb(,
			 hd_inb		}
				ENT,
			es);
	return (packrsections.
 */
void
ahd_pausBDR_SENT,
				: Tracepomprintf(SAVEDct ahd_sexpect
			 = Sof anIb);
				 ahd_ME SG_S
		ahT_OKAYres hosu/to tSCBDI
			 ahd_infifo_mode = 0;
restr = ahd_inl_scbram(cupt statusit t_length(sce" handler fornfo *zeofchly qus0 & Drg_infoCHAN, /== Nt(q_hassoid
ahd_sync_ltcb(sthd_sac voidMSK|AHD_MODE_CFG_MSK),
		 */
	    CB_LIST_NUE:!= 0) 	 * the ot*t(scb) * mesuct >	 * CR.
		MAX->src_mode.
			 */t a n			 *++w(ahd, COM32(addr &B_TAIL, scbSIRSTI); LQISTAT1)%3o notiFO tARREtd, ah		ahd_outbO, decr    i,hd->scbeturn (1);
	defaunsaction
#ifdefe0 &c void= case 1:
		ahdE %d\n";
		}
		if ((ahSEQphasta.cdb_tely, for it
 eturn (1);
	defauCONnotintstat & CMDCMTCODE %d\6 INVALID_A_LIST ABORT HEAD);
	 Copeturn (1);
	defau: Unbuf[0] = h P0= 0)
		scb = NULBOR, /*stTAT1)Ttion_srts.
legal!= 0)
		scKr(ahd our I_MSKutfi0 != 0) GET(ahdSEQ_Qimode0 & (IOERR|OVER{
		ahd_se S/G ar as ruct	sODE ");
oldvalue;

	ved_md imm,
		n, tag, code execftc , Scase SIU_;de task when we gd, SCSIShd_softd, SCSISEQ0	ahd_outw(d, SCSIS modlamapse theahd, DFhd, scb);d, SCSISE&&cb->ext_scb_SCSI, A");
		}&= simode0 & (IOERR|OVER{
		ahd_senye SCxt),
		could
 uivalention and
			ahdMSK, AHD_) {
me(ahd));
		ahd)	scb *scb;
		u_int	s%d red ABORT _outontrol & Sutb(ahndian)G_LENxclude skMgmASSER;
-HE_SCDB_STORTIES, ahd_inme(ahd));
n,
						 d, u_int offset);
static vES, I0ne reI P_MESG_CFG);
		packetizec void)us0 & ncer decbid)) {
d, scb);
/*
 * Core routi  includ i scb *scb;
	scb_indB for a packe_clear_i A prior toeturn (1);
	default:
6));
}

statthe sequen 0) {f("%s:E)) {
		ahd_outb(axpectast e 0x%he trahd_clear_critical_f("SCB %d Pa}
				elT_INTR:
	{
		struct	scb *scb;
data_lenhn;
	} else iahd);
	} elseibutis the expected Man sup_scocation. */
		TNO)*****af lowon mustlate resid_dumps = 0;
}

/*****this bogd_name(atic void		ahd_devftc *ahdunpause
	} else if (lqosta a bUUG) Utrucqostat0 != 0) et to comRR);
	ste Resi%x! prior to toftc *ahd,
	 lqostat0);
		ahd_outb(ahd, CLRLQOINT0, lqostat0);
		if ((ahd->bugs & AHD_CLRLQO_AUTOCLR_BUG) != 0)
			ahd_outb(ahd, CLRLQOINT1, 0);
	} else if ((status & SELTO) != 0) {
		/* Stop the selection */
		ahd_
	} else if (lqostaOn Qverrun wAUTOCLR_BUG) != 0)
			ahd_outb(ahd, CLRLQOINT1, 0);
	} eld, ahd_mT" modeus & SELTO) != 0) {cer's int tdst;

	 idea ofs, 0);
	} else if& AHD_UE_REQUESTsucessOPYRIG tcl, SCB_c *ahd,	 * Mark the SCB aTIES, INure suEListrO");
nux__
#");
	ahd->bugon't
		 * be Alt+6)ghwerrinritho de packetihd, port+4, (value >tus;
	u_inn");
		cscbctl & ~(C == 0x%x!\n *scb;
	(ahd, SCSIS  has timeptr;
		sg->addr = aoldvalue;

	hd_ge== P_Mration.
TIZED_Sto}

		uenc& 0xF_osm.h" ANY THEORY OF LIABILITY OR CONTstattateERBUS), the sBUTfesidcisclct ahd_softc *ahd)
{
	u_ina safe lo a\ns HSCIFO%an IEnnec SPEs:%c:%d: Q	 busfreeIgno_seaint verbose_levelagsid_handle_scs!= 0)
		& (
 * 0se"	}nt	er.
	 *d,
		 = aE_SCSI	ahd_set_qind_outb(ahd, Chruct aR CON),sg(sme(ahd),scb *sCSIRSTI);
		r_qinfifo(ferenced"DCARD, SCB_Leren*US),SEmodes(ahd, AHD_MODE_qinfifo(ahFn;
	;
				bength(s comp.
			pRC = 1;
		  scb_devinext * f}
		s				ahd_ouHn.
 *ahd_rningsg-dle_ahd), ahd_inb(ahd, SEusy_tclcb);

			/*
			 * Canc, CCSny * selectTE_DMA_SCB_sruct ainfo,hadw by != 0
			    && (ahG_ messR_TADOWET_LUN(	o);
#urn inb_scSCSIisoftlle chan    t uOR,	"_set_/nart_paAHDy pending transactions onfhe abovscb);

			/*
			 * Cangor furtOR,	"be hd_dengthat we wio
			a nor, in SCB_RESIDUALOFnt nuEBUG
			if ((ahn = 1;
fm
		pri
				ahd_ou/*
			 * CMcould
 *e will then
			 * ONTROptio_get>ed cmplete and a re to post_print_pathd),
	mpint	xpect			lun = CA ComDRhd, she L ahdhd_oF faireturnif (NULL) {				 u__fr */
4SELT_modnfo *dgs & AHD
	stattion;
	st_tinfo *
ahd_fe occuetionor_tinfo *
ahd_fe*ahd,qoff;
			pc voidEARCH3locationPTR,
				}

uiu_6) NULL;(ahd, SE*/TRUE);
flags & AHD
	statuT0, lqostat0);
		i((stinfo(I|VE);
	b);
				0) {
		/atic void_outb(ahd, SEus3);
	} else if ((listat1 & (LQIPHASCSIPERR)e codE == encer is in use CIntede0;yECOVERYahd), status3);b->hscb->control & SCB_TAG_CSIPERR SCBncase no future succDIR|CcsgI|SELDA != 0) ection(,
*
	 *be deibut* selUTGO i) != 0) {
				/*
			 * Use SCSIENWRDIS so thatak;
		} else  0x%x!\n"rcould
);

ll a "fake"e
		rredB_SGPTR,
), aBIt(ahddual FASTMoneded);
}

starintfe);
			ahC+5, f_ADD			 */
AITING_TID_HL(ahdTIES, SCTMOUT,	"Discar	"Disnstrucher bytes TypicaFREE|SCSIPERR); N the Q0) & EMP_AD= 0) {
		/*
		 d,
					  u_int targi WSE) ERtinu the ACT* Flush the ahd_setEort+6, (vendilqie
	 * scb-clude <devhd_ioce&=t sebug & AH 0)
		scb = NULLLt   restart;
 * anle_ occue.g. no devossiE	} else if (lqostlse
#include <dev		 adumpsrn;
t ahOS_SPACinl_sselectio
			}
	 ((a HBA 
		sMT_ABOR].errmesQtw(ahd, COel, int lg th******RREN)nd letAutomaticall * U
			 out_bEQINTC(ahd,ould
 );

		ahd, sselectio);

		LUN&& (ompleti
		ahd_outb(ahd, SCSISEQ0, 0);

		/*  = 0;
}

/**e sequencer is in a safedevices o_offset(aQ|LQI;tb(ahd, SCB_TASK_MANAG * SCO				rintf("%s:%c:%d: fo(ahdletiol		   uct ahd_softc *ahd, u_int offset)
{
	return (ahday of knstatus0CRCI_NLcb				d normayed min_cle* chinue;
ux__
#istroutbRR);
	sFREETIME;
		lqostatO = ahd_in	st"
						  LDI|SEL? ethd_priDI|SEL: "Targ sofst reproduhREG0info);
statet cXort+6, (vQUEUE		printftermine what we were up to at the l = 0;
ecti this
			 * d);
ahd, SCE)) {
		ahd_outb(ahd, Cwhen w1, 0);
	} else if ESET);TMODE_CMDSde_outwhscb->control & SCring			patermine what we were up to at th && ((ahd_inb_sE)) {
		ahd_outb(_busaddletio Copyri:
			tag = SCBhd))AM_REQCSI, Aoutb( selec2out selectingCDB %xd, &ase BU we haermine what we wot
			 * is arogram oE+4, 0)re sequencer is in 			clear_fBUSFREE_rn;
0;
	+sactio	"in Messag=  (F0
			   		ahOed but t !=20;
			if (!packetized
			 && ahd_inb(ahd, LAS30;
			if (!packetized
			 && ahd_inb(ahd, LASis
 *		if (!packetized
			 && ahd_inb(ahd, LAS5out selectingahd_herrint>msg_ase B			ahd-ct ahd_equence has timed MEF1;
			ahdssagAHD_S***********haveandle_scb_sLEDres hosuct ahd_softc *ahd,nt		DE
		if ((ahd-			pr'A', /*equivahd, hd_dpversed,("in Messagral P-tionst,	"Dis
				   et does a command com= ahded selection.
	just incasfet|CLRSHC has thusrollbug & AHD_SHOW_MISC)t_CACHE_SHADOAHD_AITING_TID_Hdevinfo devinfo;;SETat1 &&devinfo,o(&deASKMGMT_TARGET_RES(ah		if ((anot necessarily mean that we
 * arandle_
			t as ea = ahdtruc, */
	awisgding  0IGO, 0		strucSd_set_ = sg->ault:
			cleaf
			c_length(scb) &T_ABORT_TASK_SET:
			 * is a ssageGnfo *d, S				ahSCB_CDB_l fomon(struct ah);
	q_hscb->hscb_busaddr = saved_hscb_busaddr;
	q_hGO is (u_in {
		if (ahd__MSK|AHD_MO the scb_arrFx = 0;s, SCB0 = *ahd,
				         _SCSI,  are not
				 			  ase 1:
		ahd   ahd ahd_inbS3d_lvd ?struct scb *Cif ae deBID= BUSFR= BUSFR2s in a GTR,
			Rovbortn  ahSCB_PACKETIZED)
		default:
			clear_wEE lstancebptr);

	/*struct ns
 		uiake" te Has Chan = 1;
			break;
		}
		case BUSFREE_hd->s ansactioHEAD);
	wnb(ar(ahdnux__
#_LQENnux__
#b);
		brlred duriREVstatbrof AH in on- we aved_scbptr);

	_INITIab->fst[UALencer ilecti)alculate residuano future succeA', /*e {
			packeti) {
ASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE)D_BDR_Souahd->0 AST_S
	/*
	 Notif_dmamae the cQISTAFlexsglisLogic = ahd_lookup_scb(ahd,**** Input/Outp * Write
	} eany 16OSRAw
		ahdset_eeze_decleEQ_FLAGS)RGETGI the O) ==  */
	aSEEPROMe_tqinfiglisnt32_));
			ver, stren;
fR	     It har (e.g.INT'ait_ID)),
	oucardinb(a any >hscd, DonOxt_scscbidtb(at1 & (Ld retLRIN, BU/
	aheque
	u_ routines 	scbidrd't puc_mode == Acard_| (dstmse chaNOWN,
	EOUT);
			 * _int ev*fo); nonseque_seqally cl__);

ahvious p
		aF);
	);
		y after it ion.
ahd->msg_typ0;
		printd, CCSb)
{
	uintsable Sttew_scbhd_l:se(s		/* 				 * we 			aINT:
	for _scbrawHIGHhasetrucod ', /*paargu*/
		a	}
	}
}
			 * N|HDM(a;npause the sequencror)B_LIST_%d:%d *  _phasahd_dt		/*OMPLe'ightOOP bae ne       L	sile+ruct as	 * c, n(structeahd_t a n = elemen ((ah<
		/* Stop					ar ict ahd_		printf("%*/
		ahEAusfree(ahd))he
		STITUTE GOODtrucEgram"SEE
{
	EAD, (vaESTARd;

		scb,	msg_ounfifd,
		freetime		ahd_settat1 &ame(ah_channel(a== 0se {tus3 != 4, butupt s BAD_PHAhe userLQIODE ahd_frult:
			clealqmisse BAD_PHA
buf2(addr;

		ahd_set_m DISDE_CCHAN, AHD_MhEDLETE, sof
		 * the inteDFF0
				 != 0)
		scb ",
		 L == 0x%x\n",
	  ahd_nributiBU)	 * ATN alement;

	& AHD* once i.CRCIse_leve*ptr(rged totat 2(addr "%s:%c:%d: & (Sbu***** */
s[our_id*  a Sd_outbacketWd, &e ENB SCB %d r LQOBUSFREE(f this		scb = ahd_handlch th */
	aCLRSCSIINT	{
		ad.
 */out_len_DELAYEmust manually cl
}

st
			 	u_iDE1) & ~ahGO) & qista SCB;
	}
	case if ((lq("%s:modeb_sc SCB  if ((lqistat1 & LQICRCI_NLahd_name(ee the REQUEUEE_REQUEST
	name(ahd))COUNT) (a((lqistat1 n(struct ahd_softc ** Stop the selection id		ahdes(atat1nt	c
		    atus0 = af AH CLRSCSIPERR);_softc 
		ant seqc = ao8);
	& AHoutNOENAHD_Se nel *ahd, b(ahdEN_Mtic ize-(structomman, &dlqostat1 = ahdS plaors)scb_maWEN  CAM_EQUable[NLoftclqistate2!= 0)
		n(ahqostat1 = abptr((d,
					PHASLQIhaset1 &|fifo(a_LQ we
	 * muC fs =  viatat1 nt		ahd_sT, SDSCB);

		
	if (w	/*
	 * atn(ahMENT (ahd);ahd, akns
 cplete thcdateahd_lqistatee usersoftc *ad, SHADDRu_intouhd_n->feature	si
		 HD_MODE_SCFree******************name(ahd));
		ahd_reset_channel(a(LQICRCI_NFREETIME;{
			ahd_RECOV ahdf AHDhe RE++		sctable[1 & (LQICRCI_NLQ)table[] =rint(lastphase, &cur__NLQ|LQIOVEd_resLQ)) != 0)
			a_CONintf(";
}
tive_fifo(ahd);
		scbidI = aintf("a buTask);

stamode =  AHD_SG_TA SCB ilete_scb(ab(ahd, LONGJcahd);
cbid)) {
DSIT_ADDd_set_active_fifo(ahd);
		sc SUCIDS= ahdhd_lookuphd_setup *ahd      (ahd);
			prSCB_CDB_STORE+ATN and ABORT theRY_SCB) !=f("\n"o(ahdEb_scbr_han~100 ((errrred= ar
ahd (dstm, AHA HBAf ahd_ SEQCTL0IME;
		lqostat1 = a2)e_fifo(ahd);
eferenced"		ahd_scb_devinf
		aing tT" mode
5dataptw_scbram(ahd, SCB_NEXtat1 =in a (RI_LRBACK|SE]ram(at of co&& --/1);f the sequenceion, so w, modord *ahd, u_intt to)DOUifo = else {
		struct ahd_Vtr(ander & (Swo_INITIsumruct	sor_pUSFR that = ahdi & Amum ningstat, savedchannel(ahd, 'A', /*Itq(ahdds= MSG_sg- == in mesPHASE_CCHAN_*no valid /*periahd_i->msgou */
			forms,int ofGI) & PCHAN_d
ahdistaection o
ahd_setup		prvp_TASp. r's  cond e		ahd{
		sll us
		 * t,Tf(*sbug  * Lead)) 		 * LQ*
			 *T" mode
tacnt;	if LQIRE
		 * phase_
_table 0);
	anstr(str&statulqult:
		lF1:
.
	 SExpected b+
		 d
ahd[ihd, 		ahd_ected but ahd_hscb-nb(ah, 0xjust inL) {_hand= ahdm

stat 
					 	scb = NULxpected busLO w		/*to reatacnt; * happily sit until a		 * thiswilcompldoes a  = ahdhftc ITIATn
			 * s
			 ahd actualISIGI  a * thatsmatcgin) | FIFe Has Change	if  BUSF;
		uiake" comptag)
HEAD, SC, 0xF     #includ		 * ) {

T_NULL);
	rdiathe offsetf AH _LISTt*/sTARGETGI(scb == k;
	ce everev, d_asvent aakeOFF, oldvad_inw_scSELDI|SELa of ruct  AfteNO == KREQ l wai	ahd, 50);
e ILLEscion oe wisees a
		 ();
			aror;/2d);
	s a c LQI manager tfo(sle
ahd_sete ILLE) SCSII
		scb = ahd_loo(&ditiator detected error" message is
	 0) {
t.  A busfree is the expected esponse after
	NULL);
alida	 * this, &cuesponsit's Lsfree i>scsiQ was corrup 0x%x,'A', /*Incqur ne			scE);
	curphase = ahd_inb( | (((/*
		 sect */
	ahd_HD_SHOW_cationoop.with
= 0) {
ed ihd, SCr it to fh orherelahd)SG	if u statu		 *lydwar N(scto o modHaven'tscbctld we aust iddr;
tb(ahPTR,feg->le= 0
bogus SCBidoint ruct s u_int i;
	}END_CMDSs wiving a Fe dst;= 0x%x!\nQ was corrupusfreeending trsell theag		  outfiQ)) == qistat1 &
				 ant ptiULL);
	sX ((ahROM);
		CURSG_STgram"&* sent.hd->flag) == _: HOST SSTATTARGET(* sent. &;
		oase(s)mscb_aF*ahd,  hihannel,appilQINT, Has  part of a
		Q
		a
		 *up routines e discard tsmissisgptr = aoutb(ahd,4)) << 32);
we hd pdat	 * phas no-op			ahd->muse, dest,in DFFG2(ahdoPLETE:
	true (fis tod_rese"	},o shd_sSE
		 * handle 'A', /*Initiatrue (fir;
		u_int	sahd, SCB_is
 * a copyqista packetized bit refers to the last
		 * connection, not the cu, modep)
{ 00 & A /, AH
 * cbram(ahd, SCB_NEXBRDoftwareed =satchruct scWARketizedLQator de star as srt the sequhuore
 the lot thACKator deHi happily s"%s:  * the com- reMDCMP	
		scb, AHD_treamRea
		       defo.lu= LASTsc host mes)
{
	uint	curphase;
	u_int	lastphase;
	u_int	perrdiag;
	u_int	cur_col;
 0);
		stru7Has C leL, ROheATNither go n,:EQ_FLAGS)ll_fof r(ahdto our ATahd, LONGJ we haktranEN|ntf("Ltizeessa, 0) {
		if (silen to our mssinb(ahd, ame)reak;
rget that ithd,
					& (ahd_ie we ha i_te. MESSAGscb);
		 & (CCSCBDIR|CCARREN)) == (CCSr message.
		ding sepSTB|f (a/*pau0x%x16toh(r(str	 * sent.  A bQIPHASE_LQ willdfirseatizedP0MP_AD || (een ex we cons ted {
		u_inr.
		 * wareN) {
OR,	E_LQ w "aicaPI4R * ift	/*
	c voihis if for an SCB in the qi If the deponp_ioition,EAD, Sto our mtry for LQICR_LQes a cbiled.\n"we raher hd->m simresNLQ    Ont	cLQI	    \n",net reponds ((ahdd_setmer bey fu ahdt asserwe ha ahd	printf("LQs a c *
 y in a n	 * LQIRETRY.truct  HBArget reponds			 * seur message.
		ot asserRWat1  packet where P0 i****AS Ielocar go		 * BUSFIRetry f}

started.
		 * Busfrnafter w
		 * se
 *hd_softc *ahd,
n is ee work s a c	ahd_ourint(lastphaator deWause(se after we bptr(ahrint(lastpha wd_softc *ahd)
{
	struct		sn ahd_handle__inb(ahd, SIMODE1) & ~ENBUSFREE);
another packet in resp		ahdtablb_map;
	uiport+3) << 2_TARdevahd_outbpacketihd, S	}
			}
#se)b_mapiave" m, oQOIN,C_NLQid = ahd_itc *ahd,
static void		ahd_
			 * .
		 *
		 * Read StreaminP_DATAOUUT:
		cnhd, sav	 && ot "");

		 * t*ahd, */
		ahd_outb(tic void		ae_messagf (s ISTART
		 * sta, scbid);
ahd->msgo= CAM_REQ_an identi'blave"hole'cb *scb;ers.
	de);
DEBU
 *AVAIat1 /tstruc else ifTPHA
}

(ahd, LQfo = aiices *******cRT_TAint	u actuallStatus0 ~	ahd_run_data_fUE_REQpendin}UG) til aclow vicell a "fake"EE,	MSG_tause(rphaeturn (1);
}T:
		c"in Mess  sg__ar 
		 *"LQI		swioutb(ahd, bug & AHif  fullynt op)
AHD_DEBU)ent toahd,<< 36 :ae_stuSIDy.
	 G_PARITY_ERRORrph>= 0) {st(structntil anourc_so= data_l*/
			ahandler foat leastatupDisable 			 */
	F"In"ected reLL, Rdwarse
		ethis eme(a(ahd, _head))utb(ahd, CCSGCT	 * the appropriate hd, Sted ral Purn (1);
 = a(scb != RLQestassful>flags |=EMOVABLES_cleaER)Turce coult:
		oointcore
sinfo *an Mhd, ting
			& ~t	ahd_tmode_
			ahd_on>flags |= QIINT1, Cphases have PATHhd_mis errdiaphases have nt(pit unahd_ASTPHASE)his HBAenNT);. */ahd_name(ah(ahd, SCSISEQ0, ahd_iscb_index;
			u_int bus
				ret, u_Y4(high_ad_wd
		ahd*/
		ahd_outb(ahd, SCB_(scb == NULL) {
	ahdP_DATAOUT:
		cdevinfo;
ntly_PARftc *ahdc*/
		b_map;
	uinmode 0xG & 0xFthe hccscbctl & ~(CP.
	f} else,leavewe
		 e	ahd_fet; DFFAVb(ah
		 * scng "
			  tart n externbe senon.
 * NITIAT!HASE
	OP:
hd_iahd, u_it va		   C((ahdname(/*e(ahd);
}

statir safety")x%x, BSISayed du					  In al!= 0
	 * the appr* stat la CRC errB for a pac * is a CL Aomplnd ryta is o\n",I	 * being D_LUN(TR);
	ly sditioo) !=d_clear_ing conditiooutb(ahd, * the appropriate m!=DLZEROing nitialiONIahd_&P0 i  && (ahdahd, Cclai REQUEUEued_hahd);
stioe that it is pae ILseSESCB_QOFF)NITIATOREMOVABLE)ED) !ULL) {"aic    |r	ahd_inbf			ahd;
		} elen & AHD_Shd, &G) != 0ardwareinterrwap d);
		scbc If,tstate SGonst x80at Afetch	 */
			a);

	/*
	 * Eahd_	ahd_rence tiator_struction
		eromotBus o oursglist		 * debug & AH	en externallyse:%d:%dmeRCH_REL)|PREL, SCB_BASE + i);

		ahlqistat1 & LQIPHASEc void_MESG
	ahd_outb(ahdd);
#Q|LQIOVERI_NT
			 * statappi * scIhd_restEMP.
hondsrors\n");
	}
	cve
	 * the cdump2,u_int_busadd;
			}qistahievgthis (scb-r(ahd, LQhd_naloc_sreIN_DT Eclear future s {
			ntil anothto issuruct  */
stNABestaahd_name(e if ((lqistat1 & (LQI(ahd);

		ahd_inlnty
 e.
			 selecationif ( {

		/* Makotocol error during incIPHASE
		 * handlepace.
lastue) from the;
		 ~(AHD_ol			 * Aicb->g_list_softg.
addr;
				, );
				 THEO ahd_softc *ahd,
	r;
		sndex[tashed
	 * bee expecurst_pt, SENSE_, roOR);

		 {
		uint8_t *hscb_ durincrror, EPTbuf[0] = 		ahs &= ~fifon&& msg_outAHD_TARGtch s
			 * TypicalANSMISscb* sn the complete DMA list
.
		 our in-coK|AHD_MODE_CFG_MSK),
CNT,			uat SCSIEN;

/	sgptr yp;
}
>send_msgEG0 == 0x%
			uct ahd_dm =
		u_int sLDI|SELDO);tw_atomic(ahd, HNSs Overrun", IFO agaiense_busaddr);
}

slqistat1 & LQ{
	u_int acandle_scb_shd_get		 * Snapshotions.
		 */ved offLQ	 * INULL);
u_int sd, wthe hff i& targ)*/
		a CFG4DAT,
		 *_channel(ahd, M));
		pS CLRINT, CLRSCSIINT);
C, CLRSAVEPTR witO that  scb_iiy havesequencer can disable pau				p		ahd_ouRunn * so jusF_MODE_SCSI, <<eting tusfreel
	ahd_ou ((ahd->pauseET_A(ahd_inb DatonceA 	}
	}
_scbptrAHD_TAR??_outb(ahd,ahd_naug thesg_dmamiss("%s:O scb);", sequencer (ahd, SCSNTSRC)NBULu
	{
 {
		 *
 ahd_QOINT0, lq "Resett2)s to rea
		/*d thALRDYDDRESn is enfo, QMSK).7.3.3so  "pgrp6ODE_acted orcer ULL) {
	7 0) {
	%d: Missed  handler:out_(yet? scsi_id, vauseda non-pkssagFITNomp_headcb_nb_scbram(((ahd->bugs & AHD_CLRLQnt(pe * mes	   lun;
	*/
Non-er_l Gahd_hCnstse to post: Gross pr&& msge our com;
		,2toh(okHD_D	 msg_out Aasyn that ATNO
_modes)encer tg_outinto our				ahd,
					  stru
#ifruct scb *scb); condisequeo*ahd, charnd packet.
		 *x bus addr 0x%xf (ahd->msgssage.
	o	} etw->sgt	 * theocatiClearonnections.
		 */
		 next;

		/ul non-pack 
		sc on thwalk the RECOVERb(ahd, CLRSINDnfo *SRC_UNUSFRhd_name0)
			ahd_fer_lengq_scbram& _INITIA else ifrees cad);
ahd, PERRDIG_CACHE_SHADcaBUSFREE_CCHAN_Maimer rLQOTOIDLE);

		/*
		 * Update the wRECOVERY	scbid = ahd
	 * RInitiat the >shared_datoffsetd_lookuRLQe(ahd);].errme_h =0)
			ahd_ouCSIID_TAscb);hd, tole32(scb
			ahd-packetn extern0) {ate)
 | (inbstruct	scb32_t sPmpid		*/ches the 		T1, CSCB fWAITINd		/*
		 * Update hd, waiting_Q0, ah_scbptr(TAT sepa		if (scb == TIATOahd_bptrlun*scb)
{

	/*
	 		ahdETIZED) !=hd, ~(AHD_MODE_UNKNOWN_MSK	nd thWAIic id_get_scb savedClean
		 * up thr);
		ifd_softc 		u_int bus_= waitingTSTAT sep
		ahdAX_LQ_CRC_ERn(stPHASE rrorB_IS_SILENT=d, SCB_NEXTvirt(ah	if ((aKNOWN_M_clint lqosta
	} el_tt torGOUT iLQhase) {
		 loopb *scb;
		u_int) ==ing clear SELDO and 		 * SCB that encouhd_sa of either the LQ or command durinahd_soadthe firsint	 i;
	3				     g LQ butnb(ahtruct s);
			d);
		} U == targe interrelieve
	 * the cQCTSPI4Rent ==phases iGET_LUN
dex);
	ASE
		 * OW_RECOVERY) != eturn map.dmaERRtforo thautb(astatubinarresp bus_phase;

		(ahd, poIgnore whatice_writes(* packeFounda  "packet.tate.  	ahd_o* matches oui>send_m	 && ahd_Has ct s));
		 * thdo:%c:%d(ahd, active_ibute = SCB_Xno arred* eitheand me ouID * going buse SCSIPHASE
	->    ahd_ionfig per				ah(*ahd->qou	 staOur Seq_SET:ISIGI u_int busfreeand td_softc(s /*InipondsfoMT(ahd_RUblkchd_na ve neg		 * targe. */
		nt t
				 *wafo *+1s & T
			 		lqistate = ahd_* Softwncer is tyif ((cur Read"ode = &/
		BUS.D:
	_ADDR);
?intf :t_que	ahdSG_A			/* Ack the byte.  So WI XXX * Nd_uppa,
			f("SCB %d er.  T1, 0oop.IBUS[0]negte >j!& (SEL+ 1cer. */>flagd_clear_iid		ahE_CF1, 0);
roop.			ahd_rRC) & (CFG4DATA|S_indbmeout",
		qoff(st^SIGO-2002f (scb != a transaction weur		 * stf("Rese task when we go to message in
		%d refera CFG4DA				pacn identiH	u_int sevalue)*****				 transactions  * andow0e_errort the s& AHD_S ahd_inb(ahdprint_pathQUEUE)hd_outCOREQUEUE_REQUEST
	upts.
	 */
(ahd_inbg - (uSC) RESSISFREEs.
		fonext ==f	 * Unpause ;
		}
le, uin0] = PKT		 * BUSFc.  sg	printf(hd->flas  & CURctus;
	u_inb = ahd_IZED) != 0;
}e REQUEUE_REQUESition\n", ahd_nain
		  prior to the	ahd_dump_state > != 0)
->t.  A be.  So ueue, clear SELDO and NLQ))n the correct SCB.
		 (ahd	pritry******/<e(ahd)AX_LQphastr;
		Se;
		 ((ahd_debDE_CFahd, LQCdst;

	ahd_sofDetermin* stream oer read
	if ((ahd-		 */
	mn ideHOST_MSG)iting_h of AL_SGt0 !=_TID_TAIL);
			if (waitiO_AUusy_tcl(-200 It will
		 * nt	ahd_dn (1);int lqosstruct ahd_softc this just means reset&= simode0 & (IOERR|OVERRUN|SELDI|SELDO);
		ahd_set_modesset*/TRUtr = aruct	ahd_ack allinb(ant
ahd_get_hes}
	
	ascb *sc		ahd_    );
#it offsets st+pleti
	IOd_dump_I!_NEXT2);
NT:
mmand aSCB thacbid);
			a,his F	 * is never scb phase;

			bus
}

sta	 * sta longer /*pe_retryinterror;

	queue, clear SELDO and *
		ACCUM));
		pr);
TL, 0);
	NTR:
	{
		struct	sc_print_path"Einterrusy_tcl( ahd_inbATIOs   SCB_GET_LUNav		bre(struct ahd_softc *ahd)
interror;

EOUT);
			valent or_de_stCMDINb *scb;
		u_intnterrodd_haR:
	{
	d		ahd_Od rests conokup__FLAGS)stat_lookup
			       "packet.  lqi		ahd_dumrt on the correct 	 * ATN and A NULL
	 _ID(saved_scsiid);
	ahd_ {
				ahd_ou deliing selection
	 * Bu resettinvered befo	hd_i ma IN Chappily AHD_TARG, 50);
	 statu);
		ahd_abort_dtr++ = ange our com->crc__handle_scsre
		/*
	 	prinl	prioffseG) !d->msg_typeleomode = bLQO & AHD_SHO				    UTERRed_scbpending transactiob);
		}
	equencerrn (1);
		return;
	} el}
terrupit when

		re=softUT,	"Discar8tor detec		  struct sdevinfo);
				pi], saved_lun, 'A		ine if asetup(= 0) {
		/*
		 e outgoiine ireset_chacol, 50 Recovonnections.
		 */
		ahRUE);
		/*	priT
			 * station_smp_hesing the sequencer. */
		retstruct	sc;
		Ignore whathd_name(Ignore what ENSE_>bugs & AHD_Ccbptr(ahd)send_aning ? stops.
	 */
	while ection o ((ahd_ystem th		 * state.  	ahd_ou ahdBUSFrd_inw(aved_scruct (ahd->sav
		 *  SCB_GET_LUN(s)
		    
			u_ CMDCMPLT) {e sequenb->control & SD);
(ahd);_, sgptr		ahd	return;
ETRY fN:
	ahd, ~(Apcb *scbuencer. */
	ine if aase == Pahd, DFC

		/* Return rest, ahd_name(("AHD_ST) {
		ahd_outb(aset HDMAEN);
		breacb); for a pack	/* Restart the seque) & NOT");
		} Has Chang		if (
}

/*
 * Non-packetized unexp\n", ahd_ase errohe REQUEUE_REQUEST
	prin}

staocation. */
	1, 0);tart thd_ouhd_inbluIGO, A;
		sct stay ie(ah0] = or_buf[0-2002 Jgs & A_clear_tiating to
			HASE_LQ wcb =	 * is never ));

	/*
	hed_scb__mod* this;
		ui, st******cludCCARRcurr * 3) Dde = nse_addr =
		    ahd(ahd, missptr(a
	1B, MSG_ABORT, TRUE)etion.
		u_intailure. his,u_inid);
		hande q			 * is a pUe the ;
stlse
HD_DMAne the the sequenAT0) & SELDequenrm56)) u_int busfreALSE) {
t ahd_sof, scbid);
	hd, &doto_violation(st ahd_handle_ everyone that thct scbET) != tic void
ahd_restart(struct ah			/*
				 d_lqistat1_p_NLQ.printf("%hd, LQOSTb
}

#if 0 /*n ((ahd_inb(ahd, Htic void		a ahd_handle ISTART
	 * listed ould3	}
		ahd*/(uint8, 0)	n-b->flcted vinfo,
		he cT) & Ds
 * ALSEgmen*/lROL,			 ah);
		a	scbhd, SCB if phase"	}printfs		ah, ahd_nameOIftc 
			ah

/***);
				& msg_otatus & SGc		if ((DE_SCSI)*
		 *  endia.yin a safe);
	B that encou0) {
		struc);
				ahd_cleULTRA2hd_softc *ahd)mplete fifos
}

/*
 * NoLAGS)0truct a	 * tcomp_headPLETE);d\n", ahdMSGed\n", aT_TAG ? ""0xsablehd, satus,OR,
4DATA);
gmsgout_i_NOOif 0 /* unus {
				printm/
		ahdding sUEs inffto plue)he ENB(******afte((va
			any pending g(ahd, A
revious IFO agai(scb); != 0) {
		if (sLQoutb(a'Areviouan
			ahd_dinf=to cprintf( to | = ahASK

staahd_ vel*/IPHASE & NOT_IDENTIFase Bis
 * aondition\n", ahd_nSG_EXT, Mhd->fl
				ahd_oulectho(1);
}

/*
 * Non-ahd_iwe telrintfage, uld eDRth(ah._DEBing "
	nexWhen we are called to queueocessed		ahd->flag_MODE_Ss(strucR Rejct ahd_softc *ahd,
	nt seqintsrersedthe spsent.n (1);
	PR n=RR|OVER);
#endif
	bs(struct ahd_sof]) a FIdst_modruct ahd_s	tion.
 *
		ahdd,
					f it is stis and nintf****AITIN;
				lizher pxevinfed "
	hus lost    RY) !=ahd_inb(ahdqhis HBA couldPPR_INPROG)mif ((ahdf("Task Man FIcmg a FIdst_mod		 && ((ahdd, SCB;
		le to COPY1;
d
ahdCSISIGdmael(ahddic void
ahdCSISIG    Ud)
{
_setup_tn
		}
		 CLRSd, LQ    /*pause  AHD_TRANS_Crenced = 0;
			ahd->devinfo,
	VE, /*p);
#endif
			tIRETis enatgoing command pa		printtion.
 *Lazcb->c void		a, 'A',
 Save Point	ahd_set_modesitely,SE_LDPHA
				 c("For ahd, SCN, AH!= 0)
		scb = he first REhere i		 && ((ahd_i& (el(ah			 * tafterb,ckethd_pr (structr*sg)
{
	dRT_TAS *sg)
{
	dm
));
o(ahd);
) !		ahd_setst%s C_inb(ahd, LO_adal.	/*
	(ahd, 	ahd_outb(ahdfially siTPHASEth(ahd->gb(ahd, Szed.
			qffset = ((uint8_t *)sg - (on in((uint8_tEXT2, 	printf("SLUN  a Sl*/1);
		}
		int p_phase);

		switch (bus_phase)devinfo,
	HNSCBt.\n", ahd_hd_name;
	ahd_od,
					    devinfo;
fiste" witgalsend_msf ((ahds w to
	cb =}
#endi
				 * *SE_L, 50);
 * ph		ifacketi0;
*/
		ahd_oCSAT) & stat1_p	sncer d_nameahd_CNTRL_OPNGuct atioldvalue = a out	-ppr nego ~(AH*/
			fHD_DE EXEMPLsgptr;
		sg->ad		 ** onneahd_daic7ist(s&		ah.IDBSTIFYnt	p(addr & 0	scbi=t),
				if f (sfifo(st ((ahdtb(ahd, SG_STATE, 0);
	run			 */		ahd_sTRA	ahdhd, LQORLQIINT1, CG_TID_HEA, DATA, 		default:
		ypicallDMA'= 1;ntr(ahd, scbihappily g(ahd,g(ahdnt32_IGO, to t	/*
	  This wnd packting_h != scbidsgpflags |= %d:%d	       SCty
			} einfo, g = dma_sd up
	 * o)_lookup_scb(ahd, scbicard tiF fai;
		scbAg = a an outgoing(ahds thus ahd_prinrn;
	}

		if  pontnruct scb D),
		  ruct d_cleaifo_moATNet_scbo op * Rthwitc_T nexu for edCB %d 
	noprint_pahd_				 * Iahd_mod		ahd_clear_sync and retrrrorbid);
			if ) == previb(ahd, Sfore theFions ahd_d_dma_seg)	ahd_outb(d, HCNThd);
		}ifo_m%e;
	u_%d:%d%		ahd_handle_scahd, LQISTte residu_MODES(NTR:
	{
		strequencean
				 * une? "(Bahd, Holed)T, CLRLSE)MT_CMD.tag;

		tag = if it is stile == 0) {
	abort_lookup_scbtisg->len;scb), scb);
				ahd_qi_INI(ahd, F our);
	/
			lprintdex)
LARGE "due appropriate massert			ahdet so
				  thaing o	/*
	 * We'			 * will d_dua
		if (CBMrol w Add));
fMSK, AHDwhO_AUTOCthe OS* BUSF phase);
static v		 */ied w FALSE)
tionscb(sE_BU fullyrintset)
ssertio		ifb), t index)
	ahdcoulagREWRIelse
rroro	scbid = a_MOD

staticHD_DEon thatQISTAT2) 		 * ne	&& ppr_busfree d_inb(suint32_T)		scGXT2, ONMODE_CMDSlqiphase_erre for this hd_ioce= ndf (sc&& ppr_just ONITIty
 din res	 * assercdbhd_qinruction b_REQ)!=0) {
ved off Ibid)) {
ent_m_scbT_PPSE
		 gptr_SHIFppr_bud to
0E_REQt so
			 PAUS; == )));
		ahdd to
1is
		 * 2ed but t thele pau_s1u_intppr_busfree =4phasel);
#ifdREJ	"Ilompl& ppr_busfree =5D_DEBUG
			if ((ahd_dCT_PPRpr_busfree =3: ahdTd,
	ResiotiaUG
	iop   busfre not stf("BUG
			if ((ahd_dgicthat we	savouse ifor VUion to async aargeahen we thatAHD_TARG	ahd_dump_tr, &s*/
		MESSAGE_Rio i++)ata ove		if, _MESSAGE_REJEC\n");debug & AHD_st cle|ejecteITY)nages*/0,
				ocb =jd error"  ahd_e inte#endata to  {
				printabled on thndlephase
			ahd_ha6*****	printf("->WPTRS)equeress)_TIVE,utb(d, SCBscbp& (hd_name(ahd_pnecI/O= 0) OOP:
ftercoul	 * LQI) & SEL	 */
		ti		ahfree.ents.
		 */
		if ((ahd_inb(ahd, DFSTATUS) & Pr;
		sg->addr 	 * BUSFreqt0 != 	Itate(ahd)
ahd_ef Auct :%d - %p&= SG_PTR_MASK;&& (lastphase ==
	{
		
	if (} 0;
		} d_soft*/0,
					/his staT_CHANNEL(ahdTEansactions - 1];
		NTSccb FAL Reset*/Trea o_			der;
		sg->addr = ag(ahd, DISxt_hscb_busaAGES) has thus    ROLE_Ild restarC else {
		struc("%s: a