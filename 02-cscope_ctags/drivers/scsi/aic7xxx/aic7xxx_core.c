/*
 * Core routines and tables shareable across OS platforms.
 *
 * Copyright (c) 1994-2002 Justin T. Gibbs.
 * Copyright (c) 2000-2002 Adaptec Inc.
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
 * $Id: //depot/aic7xxx/aic7xxx/aic7xxx.c#155 $
 */

#ifdef __linux__
#include "aic7xxx_osm.h"
#include "aic7xxx_inline.h"
#include "aicasm/aicasm_insformat.h"
#else
#include <dev/aic7xxx/aic7xxx_osm.h>
#include <dev/aic7xxx/aic7xxx_inline.h>
#include <dev/aic7xxx/aicasm/aicasm_insformat.h>
#endif

/***************************** Lookup Tables **********************************/
static const char *const ahc_chip_names[] = {
	"NONE",
	"aic7770",
	"aic7850",
	"aic7855",
	"aic7859",
	"aic7860",
	"aic7870",
	"aic7880",
	"aic7895",
	"aic7895C",
	"aic7890/91",
	"aic7896/97",
	"aic7892",
	"aic7899"
};
static const u_int num_chip_names = ARRAY_SIZE(ahc_chip_names);

/*
 * Hardware error codes.
 */
struct ahc_hard_error_entry {
        uint8_t errno;
	const char *errmesg;
};

static const struct ahc_hard_error_entry ahc_hard_errors[] = {
	{ ILLHADDR,	"Illegal Host Access" },
	{ ILLSADDR,	"Illegal Sequencer Address referrenced" },
	{ ILLOPCODE,	"Illegal Opcode in sequencer program" },
	{ SQPARERR,	"Sequencer Parity Error" },
	{ DPARERR,	"Data-path Parity Error" },
	{ MPARERR,	"Scratch or SCB Memory Parity Error" },
	{ PCIERRSTAT,	"PCI Error detected" },
	{ CIOPARERR,	"CIOBUS Parity Error" },
};
static const u_int num_errors = ARRAY_SIZE(ahc_hard_errors);

static const struct ahc_phase_table_entry ahc_phase_table[] =
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
static const u_int num_phases = ARRAY_SIZE(ahc_phase_table) - 1;

/*
 * Valid SCSIRATE values.  (p. 3-17)
 * Provides a mapping of tranfer periods in ns to the proper value to
 * stick in the scsixfer reg.
 */
static const struct ahc_syncrate ahc_syncrates[] =
{
      /* ultra2    fast/ultra  period     rate */
	{ 0x42,      0x000,      9,      "80.0" },
	{ 0x03,      0x000,     10,      "40.0" },
	{ 0x04,      0x000,     11,      "33.0" },
	{ 0x05,      0x100,     12,      "20.0" },
	{ 0x06,      0x110,     15,      "16.0" },
	{ 0x07,      0x120,     18,      "13.4" },
	{ 0x08,      0x000,     25,      "10.0" },
	{ 0x19,      0x010,     31,      "8.0"  },
	{ 0x1a,      0x020,     37,      "6.67" },
	{ 0x1b,      0x030,     43,      "5.7"  },
	{ 0x1c,      0x040,     50,      "5.0"  },
	{ 0x00,      0x050,     56,      "4.4"  },
	{ 0x00,      0x060,     62,      "4.0"  },
	{ 0x00,      0x070,     68,      "3.6"  },
	{ 0x00,      0x000,      0,      NULL   }
};

/* Our Sequencer Program */
#include "aic7xxx_seq.h"

/**************************** Function Declarations ***************************/
static void		ahc_force_renegotiation(struct ahc_softc *ahc,
						struct ahc_devinfo *devinfo);
static struct ahc_tmode_tstate*
			ahc_alloc_tstate(struct ahc_softc *ahc,
					 u_int scsi_id, char channel);
#ifdef AHC_TARGET_MODE
static void		ahc_free_tstate(struct ahc_softc *ahc,
					u_int scsi_id, char channel, int force);
#endif
static const struct ahc_syncrate*
			ahc_devlimited_syncrate(struct ahc_softc *ahc,
					        struct ahc_initiator_tinfo *,
						u_int *period,
						u_int *ppr_options,
						role_t role);
static void		ahc_update_pending_scbs(struct ahc_softc *ahc);
static void		ahc_fetch_devinfo(struct ahc_softc *ahc,
					  struct ahc_devinfo *devinfo);
static void		ahc_scb_devinfo(struct ahc_softc *ahc,
					struct ahc_devinfo *devinfo,
					struct scb *scb);
static void		ahc_assert_atn(struct ahc_softc *ahc);
static void		ahc_setup_initiator_msgout(struct ahc_softc *ahc,
						   struct ahc_devinfo *devinfo,
						   struct scb *scb);
static void		ahc_build_transfer_msg(struct ahc_softc *ahc,
					       struct ahc_devinfo *devinfo);
static void		ahc_construct_sdtr(struct ahc_softc *ahc,
					   struct ahc_devinfo *devinfo,
					   u_int period, u_int offset);
static void		ahc_construct_wdtr(struct ahc_softc *ahc,
					   struct ahc_devinfo *devinfo,
					   u_int bus_width);
static void		ahc_construct_ppr(struct ahc_softc *ahc,
					  struct ahc_devinfo *devinfo,
					  u_int period, u_int offset,
					  u_int bus_width, u_int ppr_options);
static void		ahc_clear_msg_state(struct ahc_softc *ahc);
static void		ahc_handle_proto_violation(struct ahc_softc *ahc);
static void		ahc_handle_message_phase(struct ahc_softc *ahc);
typedef enum {
	AHCMSG_1B,
	AHCMSG_2B,
	AHCMSG_EXT
} ahc_msgtype;
static int		ahc_sent_msg(struct ahc_softc *ahc, ahc_msgtype type,
				     u_int msgval, int full);
static int		ahc_parse_msg(struct ahc_softc *ahc,
				      struct ahc_devinfo *devinfo);
static int		ahc_handle_msg_reject(struct ahc_softc *ahc,
					      struct ahc_devinfo *devinfo);
static void		ahc_handle_ign_wide_residue(struct ahc_softc *ahc,
						struct ahc_devinfo *devinfo);
static void		ahc_reinitialize_dataptrs(struct ahc_softc *ahc);
static void		ahc_handle_devreset(struct ahc_softc *ahc,
					    struct ahc_devinfo *devinfo,
					    cam_status status, char *message,
					    int verbose_level);
#ifdef AHC_TARGET_MODE
static void		ahc_setup_target_msgin(struct ahc_softc *ahc,
					       struct ahc_devinfo *devinfo,
					       struct scb *scb);
#endif

static bus_dmamap_callback_t	ahc_dmamap_cb; 
static void		ahc_build_free_scb_list(struct ahc_softc *ahc);
static int		ahc_init_scbdata(struct ahc_softc *ahc);
static void		ahc_fini_scbdata(struct ahc_softc *ahc);
static void		ahc_qinfifo_requeue(struct ahc_softc *ahc,
					    struct scb *prev_scb,
					    struct scb *scb);
static int		ahc_qinfifo_count(struct ahc_softc *ahc);
static u_int		ahc_rem_scb_from_disc_list(struct ahc_softc *ahc,
						   u_int prev, u_int scbptr);
static void		ahc_add_curscb_to_free_list(struct ahc_softc *ahc);
static u_int		ahc_rem_wscb(struct ahc_softc *ahc,
				     u_int scbpos, u_int prev);
static void		ahc_reset_current_bus(struct ahc_softc *ahc);
#ifdef AHC_DUMP_SEQ
static void		ahc_dumpseq(struct ahc_softc *ahc);
#endif
static int		ahc_loadseq(struct ahc_softc *ahc);
static int		ahc_check_patch(struct ahc_softc *ahc,
					const struct patch **start_patch,
					u_int start_instr, u_int *skip_addr);
static void		ahc_download_instr(struct ahc_softc *ahc,
					   u_int instrptr, uint8_t *dconsts);
#ifdef AHC_TARGET_MODE
static void		ahc_queue_lstate_event(struct ahc_softc *ahc,
					       struct ahc_tmode_lstate *lstate,
					       u_int initiator_id,
					       u_int event_type,
					       u_int event_arg);
static void		ahc_update_scsiid(struct ahc_softc *ahc,
					  u_int targid_mask);
static int		ahc_handle_target_cmd(struct ahc_softc *ahc,
					      struct target_cmd *cmd);
#endif

static u_int		ahc_index_busy_tcl(struct ahc_softc *ahc, u_int tcl);
static void		ahc_unbusy_tcl(struct ahc_softc *ahc, u_int tcl);
static void		ahc_busy_tcl(struct ahc_softc *ahc,
				     u_int tcl, u_int busyid);

/************************** SCB and SCB queue management **********************/
static void		ahc_run_untagged_queues(struct ahc_softc *ahc);
static void		ahc_run_untagged_queue(struct ahc_softc *ahc,
					       struct scb_tailq *queue);

/****************************** Initialization ********************************/
static void		 ahc_alloc_scbs(struct ahc_softc *ahc);
static void		 ahc_shutdown(void *arg);

/*************************** Interrupt Services *******************************/
static void		ahc_clear_intstat(struct ahc_softc *ahc);
static void		ahc_run_qoutfifo(struct ahc_softc *ahc);
#ifdef AHC_TARGET_MODE
static void		ahc_run_tqinfifo(struct ahc_softc *ahc, int paused);
#endif
static void		ahc_handle_brkadrint(struct ahc_softc *ahc);
static void		ahc_handle_seqint(struct ahc_softc *ahc, u_int intstat);
static void		ahc_handle_scsiint(struct ahc_softc *ahc,
					   u_int intstat);
static void		ahc_clear_critical_section(struct ahc_softc *ahc);

/***************************** Error Recovery *********************************/
static void		ahc_freeze_devq(struct ahc_softc *ahc, struct scb *scb);
static int		ahc_abort_scbs(struct ahc_softc *ahc, int target,
				       char channel, int lun, u_int tag,
				       role_t role, uint32_t status);
static void		ahc_calc_residual(struct ahc_softc *ahc,
					  struct scb *scb);

/*********************** Untagged Transaction Routines ************************/
static inline void	ahc_freeze_untagged_queues(struct ahc_softc *ahc);
static inline void	ahc_release_untagged_queues(struct ahc_softc *ahc);

/*
 * Block our completion routine from starting the next untagged
 * transaction for this target or target lun.
 */
static inline void
ahc_freeze_untagged_queues(struct ahc_softc *ahc)
{
	if ((ahc->flags & AHC_SCB_BTT) == 0)
		ahc->untagged_queue_lock++;
}

/*
 * Allow the next untagged transaction for this target or target lun
 * to be executed.  We use a counting semaphore to allow the lock
 * to be acquired recursively.  Once the count drops to zero, the
 * transaction queues will be run.
 */
static inline void
ahc_release_untagged_queues(struct ahc_softc *ahc)
{
	if ((ahc->flags & AHC_SCB_BTT) == 0) {
		ahc->untagged_queue_lock--;
		if (ahc->untagged_queue_lock == 0)
			ahc_run_untagged_queues(ahc);
	}
}

/************************* Sequencer Execution Control ************************/
/*
 * Work around any chip bugs related to halting sequencer execution.
 * On Ultra2 controllers, we must clear the CIOBUS stretch signal by
 * reading a register that will set this signal and deassert it.
 * Without this workaround, if the chip is paused, by an interrupt or
 * manual pause while accessing scb ram, accesses to certain registers
 * will hang the system (infinite pci retries).
 */
static void
ahc_pause_bug_fix(struct ahc_softc *ahc)
{
	if ((ahc->features & AHC_ULTRA2) != 0)
		(void)ahc_inb(ahc, CCSCBCTL);
}

/*
 * Determine whether the sequencer has halted code execution.
 * Returns non-zero status if the sequencer is stopped.
 */
int
ahc_is_paused(struct ahc_softc *ahc)
{
	return ((ahc_inb(ahc, HCNTRL) & PAUSE) != 0);
}

/*
 * Request that the sequencer stop and wait, indefinitely, for it
 * to stop.  The sequencer will only acknowledge that it is paused
 * once it has reached an instruction boundary and PAUSEDIS is
 * cleared in the SEQCTL register.  The sequencer may use PAUSEDIS
 * for critical sections.
 */
void
ahc_pause(struct ahc_softc *ahc)
{
	ahc_outb(ahc, HCNTRL, ahc->pause);

	/*
	 * Since the sequencer can disable pausing in a critical section, we
	 * must loop until it actually stops.
	 */
	while (ahc_is_paused(ahc) == 0)
		;

	ahc_pause_bug_fix(ahc);
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
ahc_unpause(struct ahc_softc *ahc)
{
	if ((ahc_inb(ahc, INTSTAT) & (SCSIINT | SEQINT | BRKADRINT)) == 0)
		ahc_outb(ahc, HCNTRL, ahc->unpause);
}

/************************** Memory mapping routines ***************************/
static struct ahc_dma_seg *
ahc_sg_bus_to_virt(struct scb *scb, uint32_t sg_busaddr)
{
	int sg_index;

	sg_index = (sg_busaddr - scb->sg_list_phys)/sizeof(struct ahc_dma_seg);
	/* sg_list_phys points to entry 1, not 0 */
	sg_index++;

	return (&scb->sg_list[sg_index]);
}

static uint32_t
ahc_sg_virt_to_bus(struct scb *scb, struct ahc_dma_seg *sg)
{
	int sg_index;

	/* sg_list_phys points to entry 1, not 0 */
	sg_index = sg - &scb->sg_list[1];

	return (scb->sg_list_phys + (sg_index * sizeof(*scb->sg_list)));
}

static uint32_t
ahc_hscb_busaddr(struct ahc_softc *ahc, u_int index)
{
	return (ahc->scb_data->hscb_busaddr
		+ (sizeof(struct hardware_scb) * index));
}

static void
ahc_sync_scb(struct ahc_softc *ahc, struct scb *scb, int op)
{
	ahc_dmamap_sync(ahc, ahc->scb_data->hscb_dmat,
			ahc->scb_data->hscb_dmamap,
			/*offset*/(scb->hscb - ahc->hscbs) * sizeof(*scb->hscb),
			/*len*/sizeof(*scb->hscb), op);
}

void
ahc_sync_sglist(struct ahc_softc *ahc, struct scb *scb, int op)
{
	if (scb->sg_count == 0)
		return;

	ahc_dmamap_sync(ahc, ahc->scb_data->sg_dmat, scb->sg_map->sg_dmamap,
			/*offset*/(scb->sg_list - scb->sg_map->sg_vaddr)
				* sizeof(struct ahc_dma_seg),
			/*len*/sizeof(struct ahc_dma_seg) * scb->sg_count, op);
}

#ifdef AHC_TARGET_MODE
static uint32_t
ahc_targetcmd_offset(struct ahc_softc *ahc, u_int index)
{
	return (((uint8_t *)&ahc->targetcmds[index]) - ahc->qoutfifo);
}
#endif

/*********************** Miscelaneous Support Functions ***********************/
/*
 * Determine whether the sequencer reported a residual
 * for this SCB/transaction.
 */
static void
ahc_update_residual(struct ahc_softc *ahc, struct scb *scb)
{
	uint32_t sgptr;

	sgptr = ahc_le32toh(scb->hscb->sgptr);
	if ((sgptr & SG_RESID_VALID) != 0)
		ahc_calc_residual(ahc, scb);
}

/*
 * Return pointers to the transfer negotiation information
 * for the specified our_id/remote_id pair.
 */
struct ahc_initiator_tinfo *
ahc_fetch_transinfo(struct ahc_softc *ahc, char channel, u_int our_id,
		    u_int remote_id, struct ahc_tmode_tstate **tstate)
{
	/*
	 * Transfer data structures are stored from the perspective
	 * of the target role.  Since the parameters for a connection
	 * in the initiator role to a given target are the same as
	 * when the roles are reversed, we pretend we are the target.
	 */
	if (channel == 'B')
		our_id += 8;
	*tstate = ahc->enabled_targets[our_id];
	return (&(*tstate)->transinfo[remote_id]);
}

uint16_t
ahc_inw(struct ahc_softc *ahc, u_int port)
{
	uint16_t r = ahc_inb(ahc, port+1) << 8;
	return r | ahc_inb(ahc, port);
}

void
ahc_outw(struct ahc_softc *ahc, u_int port, u_int value)
{
	ahc_outb(ahc, port, value & 0xFF);
	ahc_outb(ahc, port+1, (value >> 8) & 0xFF);
}

uint32_t
ahc_inl(struct ahc_softc *ahc, u_int port)
{
	return ((ahc_inb(ahc, port))
	      | (ahc_inb(ahc, port+1) << 8)
	      | (ahc_inb(ahc, port+2) << 16)
	      | (ahc_inb(ahc, port+3) << 24));
}

void
ahc_outl(struct ahc_softc *ahc, u_int port, uint32_t value)
{
	ahc_outb(ahc, port, (value) & 0xFF);
	ahc_outb(ahc, port+1, ((value) >> 8) & 0xFF);
	ahc_outb(ahc, port+2, ((value) >> 16) & 0xFF);
	ahc_outb(ahc, port+3, ((value) >> 24) & 0xFF);
}

uint64_t
ahc_inq(struct ahc_softc *ahc, u_int port)
{
	return ((ahc_inb(ahc, port))
	      | (ahc_inb(ahc, port+1) << 8)
	      | (ahc_inb(ahc, port+2) << 16)
	      | (ahc_inb(ahc, port+3) << 24)
	      | (((uint64_t)ahc_inb(ahc, port+4)) << 32)
	      | (((uint64_t)ahc_inb(ahc, port+5)) << 40)
	      | (((uint64_t)ahc_inb(ahc, port+6)) << 48)
	      | (((uint64_t)ahc_inb(ahc, port+7)) << 56));
}

void
ahc_outq(struct ahc_softc *ahc, u_int port, uint64_t value)
{
	ahc_outb(ahc, port, value & 0xFF);
	ahc_outb(ahc, port+1, (value >> 8) & 0xFF);
	ahc_outb(ahc, port+2, (value >> 16) & 0xFF);
	ahc_outb(ahc, port+3, (value >> 24) & 0xFF);
	ahc_outb(ahc, port+4, (value >> 32) & 0xFF);
	ahc_outb(ahc, port+5, (value >> 40) & 0xFF);
	ahc_outb(ahc, port+6, (value >> 48) & 0xFF);
	ahc_outb(ahc, port+7, (value >> 56) & 0xFF);
}

/*
 * Get a free scb. If there are none, see if we can allocate a new SCB.
 */
struct scb *
ahc_get_scb(struct ahc_softc *ahc)
{
	struct scb *scb;

	if ((scb = SLIST_FIRST(&ahc->scb_data->free_scbs)) == NULL) {
		ahc_alloc_scbs(ahc);
		scb = SLIST_FIRST(&ahc->scb_data->free_scbs);
		if (scb == NULL)
			return (NULL);
	}
	SLIST_REMOVE_HEAD(&ahc->scb_data->free_scbs, links.sle);
	return (scb);
}

/*
 * Return an SCB resource to the free list.
 */
void
ahc_free_scb(struct ahc_softc *ahc, struct scb *scb)
{
	struct hardware_scb *hscb;

	hscb = scb->hscb;
	/* Clean up for the next user */
	ahc->scb_data->scbindex[hscb->tag] = NULL;
	scb->flags = SCB_FREE;
	hscb->control = 0;

	SLIST_INSERT_HEAD(&ahc->scb_data->free_scbs, scb, links.sle);

	/* Notify the OSM that a resource is now available. */
	ahc_platform_scb_free(ahc, scb);
}

struct scb *
ahc_lookup_scb(struct ahc_softc *ahc, u_int tag)
{
	struct scb* scb;

	scb = ahc->scb_data->scbindex[tag];
	if (scb != NULL)
		ahc_sync_scb(ahc, scb,
			     BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	return (scb);
}

static void
ahc_swap_with_next_hscb(struct ahc_softc *ahc, struct scb *scb)
{
	struct hardware_scb *q_hscb;
	u_int  saved_tag;

	/*
	 * Our queuing method is a bit tricky.  The card
	 * knows in advance which HSCB to download, and we
	 * can't disappoint it.  To achieve this, the next
	 * SCB to download is saved off in ahc->next_queued_scb.
	 * When we are called to queue "an arbitrary scb",
	 * we copy the contents of the incoming HSCB to the one
	 * the sequencer knows about, swap HSCB pointers and
	 * finally assign the SCB to the tag indexed location
	 * in the scb_array.  This makes sure that we can still
	 * locate the correct SCB by SCB_TAG.
	 */
	q_hscb = ahc->next_queued_scb->hscb;
	saved_tag = q_hscb->tag;
	memcpy(q_hscb, scb->hscb, sizeof(*scb->hscb));
	if ((scb->flags & SCB_CDB32_PTR) != 0) {
		q_hscb->shared_data.cdb_ptr =
		    ahc_htole32(ahc_hscb_busaddr(ahc, q_hscb->tag)
			      + offsetof(struct hardware_scb, cdb32));
	}
	q_hscb->tag = saved_tag;
	q_hscb->next = scb->hscb->tag;

	/* Now swap HSCB pointers. */
	ahc->next_queued_scb->hscb = scb->hscb;
	scb->hscb = q_hscb;

	/* Now define the mapping from tag to SCB in the scbindex */
	ahc->scb_data->scbindex[scb->hscb->tag] = scb;
}

/*
 * Tell the sequencer about a new transaction to execute.
 */
void
ahc_queue_scb(struct ahc_softc *ahc, struct scb *scb)
{
	ahc_swap_with_next_hscb(ahc, scb);

	if (scb->hscb->tag == SCB_LIST_NULL
	 || scb->hscb->next == SCB_LIST_NULL)
		panic("Attempt to queue invalid SCB tag %x:%x\n",
		      scb->hscb->tag, scb->hscb->next);

	/*
	 * Setup data "oddness".
	 */
	scb->hscb->lun &= LID;
	if (ahc_get_transfer_length(scb) & 0x1)
		scb->hscb->lun |= SCB_XFERLEN_ODD;

	/*
	 * Keep a history of SCBs we've downloaded in the qinfifo.
	 */
	ahc->qinfifo[ahc->qinfifonext++] = scb->hscb->tag;

	/*
	 * Make sure our data is consistent from the
	 * perspective of the adapter.
	 */
	ahc_sync_scb(ahc, scb, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	/* Tell the adapter about the newly queued SCB */
	if ((ahc->features & AHC_QUEUE_REGS) != 0) {
		ahc_outb(ahc, HNSCB_QOFF, ahc->qinfifonext);
	} else {
		if ((ahc->features & AHC_AUTOPAUSE) == 0)
			ahc_pause(ahc);
		ahc_outb(ahc, KERNEL_QINPOS, ahc->qinfifonext);
		if ((ahc->features & AHC_AUTOPAUSE) == 0)
			ahc_unpause(ahc);
	}
}

struct scsi_sense_data *
ahc_get_sense_buf(struct ahc_softc *ahc, struct scb *scb)
{
	int offset;

	offset = scb - ahc->scb_data->scbarray;
	return (&ahc->scb_data->sense[offset]);
}

static uint32_t
ahc_get_sense_bufaddr(struct ahc_softc *ahc, struct scb *scb)
{
	int offset;

	offset = scb - ahc->scb_data->scbarray;
	return (ahc->scb_data->sense_busaddr
	      + (offset * sizeof(struct scsi_sense_data)));
}

/************************** Interrupt Processing ******************************/
static void
ahc_sync_qoutfifo(struct ahc_softc *ahc, int op)
{
	ahc_dmamap_sync(ahc, ahc->shared_data_dmat, ahc->shared_data_dmamap,
			/*offset*/0, /*len*/256, op);
}

static void
ahc_sync_tqinfifo(struct ahc_softc *ahc, int op)
{
#ifdef AHC_TARGET_MODE
	if ((ahc->flags & AHC_TARGETROLE) != 0) {
		ahc_dmamap_sync(ahc, ahc->shared_data_dmat,
				ahc->shared_data_dmamap,
				ahc_targetcmd_offset(ahc, 0),
				sizeof(struct target_cmd) * AHC_TMODE_CMDS,
				op);
	}
#endif
}

/*
 * See if the firmware has posted any completed commands
 * into our in-core command complete fifos.
 */
#define AHC_RUN_QOUTFIFO 0x1
#define AHC_RUN_TQINFIFO 0x2
static u_int
ahc_check_cmdcmpltqueues(struct ahc_softc *ahc)
{
	u_int retval;

	retval = 0;
	ahc_dmamap_sync(ahc, ahc->shared_data_dmat, ahc->shared_data_dmamap,
			/*offset*/ahc->qoutfifonext, /*len*/1,
			BUS_DMASYNC_POSTREAD);
	if (ahc->qoutfifo[ahc->qoutfifonext] != SCB_LIST_NULL)
		retval |= AHC_RUN_QOUTFIFO;
#ifdef AHC_TARGET_MODE
	if ((ahc->flags & AHC_TARGETROLE) != 0
	 && (ahc->flags & AHC_TQINFIFO_BLOCKED) == 0) {
		ahc_dmamap_sync(ahc, ahc->shared_data_dmat,
				ahc->shared_data_dmamap,
				ahc_targetcmd_offset(ahc, ahc->tqinfifofnext),
				/*len*/sizeof(struct target_cmd),
				BUS_DMASYNC_POSTREAD);
		if (ahc->targetcmds[ahc->tqinfifonext].cmd_valid != 0)
			retval |= AHC_RUN_TQINFIFO;
	}
#endif
	return (retval);
}

/*
 * Catch an interrupt from the adapter
 */
int
ahc_intr(struct ahc_softc *ahc)
{
	u_int	intstat;

	if ((ahc->pause & INTEN) == 0) {
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
	if ((ahc->flags & (AHC_ALL_INTERRUPTS|AHC_EDGE_INTERRUPT)) == 0
	 && (ahc_check_cmdcmpltqueues(ahc) != 0))
		intstat = CMDCMPLT;
	else {
		intstat = ahc_inb(ahc, INTSTAT);
	}

	if ((intstat & INT_PEND) == 0) {
#if AHC_PCI_CONFIG > 0
		if (ahc->unsolicited_ints > 500) {
			ahc->unsolicited_ints = 0;
			if ((ahc->chip & AHC_PCI) != 0
			 && (ahc_inb(ahc, ERROR) & PCIERRSTAT) != 0)
				ahc->bus_intr(ahc);
		}
#endif
		ahc->unsolicited_ints++;
		return (0);
	}
	ahc->unsolicited_ints = 0;

	if (intstat & CMDCMPLT) {
		ahc_outb(ahc, CLRINT, CLRCMDINT);

		/*
		 * Ensure that the chip sees that we've cleared
		 * this interrupt before we walk the output fifo.
		 * Otherwise, we may, due to posted bus writes,
		 * clear the interrupt after we finish the scan,
		 * and after the sequencer has added new entries
		 * and asserted the interrupt again.
		 */
		ahc_flush_device_writes(ahc);
		ahc_run_qoutfifo(ahc);
#ifdef AHC_TARGET_MODE
		if ((ahc->flags & AHC_TARGETROLE) != 0)
			ahc_run_tqinfifo(ahc, /*paused*/FALSE);
#endif
	}

	/*
	 * Handle statuses that may invalidate our cached
	 * copy of INTSTAT separately.
	 */
	if (intstat == 0xFF && (ahc->features & AHC_REMOVABLE) != 0) {
		/* Hot eject.  Do nothing */
	} else if (intstat & BRKADRINT) {
		ahc_handle_brkadrint(ahc);
	} else if ((intstat & (SEQINT|SCSIINT)) != 0) {

		ahc_pause_bug_fix(ahc);

		if ((intstat & SEQINT) != 0)
			ahc_handle_seqint(ahc, intstat);

		if ((intstat & SCSIINT) != 0)
			ahc_handle_scsiint(ahc, intstat);
	}
	return (1);
}

/************************* Sequencer Execution Control ************************/
/*
 * Restart the sequencer program from address zero
 */
static void
ahc_restart(struct ahc_softc *ahc)
{
	uint8_t	sblkctl;

	ahc_pause(ahc);

	/* No more pending messages. */
	ahc_clear_msg_state(ahc);

	ahc_outb(ahc, SCSISIGO, 0);		/* De-assert BSY */
	ahc_outb(ahc, MSG_OUT, MSG_NOOP);	/* No message to send */
	ahc_outb(ahc, SXFRCTL1, ahc_inb(ahc, SXFRCTL1) & ~BITBUCKET);
	ahc_outb(ahc, LASTPHASE, P_BUSFREE);
	ahc_outb(ahc, SAVED_SCSIID, 0xFF);
	ahc_outb(ahc, SAVED_LUN, 0xFF);

	/*
	 * Ensure that the sequencer's idea of TQINPOS
	 * matches our own.  The sequencer increments TQINPOS
	 * only after it sees a DMA complete and a reset could
	 * occur before the increment leaving the kernel to believe
	 * the command arrived but the sequencer to not.
	 */
	ahc_outb(ahc, TQINPOS, ahc->tqinfifonext);

	/* Always allow reselection */
	ahc_outb(ahc, SCSISEQ,
		 ahc_inb(ahc, SCSISEQ_TEMPLATE) & (ENSELI|ENRSELI|ENAUTOATNP));
	if ((ahc->features & AHC_CMD_CHAN) != 0) {
		/* Ensure that no DMA operations are in progress */
		ahc_outb(ahc, CCSCBCNT, 0);
		ahc_outb(ahc, CCSGCTL, 0);
		ahc_outb(ahc, CCSCBCTL, 0);
	}
	/*
	 * If we were in the process of DMA'ing SCB data into
	 * an SCB, replace that SCB on the free list.  This prevents
	 * an SCB leak.
	 */
	if ((ahc_inb(ahc, SEQ_FLAGS2) & SCB_DMA) != 0) {
		ahc_add_curscb_to_free_list(ahc);
		ahc_outb(ahc, SEQ_FLAGS2,
			 ahc_inb(ahc, SEQ_FLAGS2) & ~SCB_DMA);
	}

	/*
	 * Clear any pending sequencer interrupt.  It is no
	 * longer relevant since we're resetting the Program
	 * Counter.
	 */
	ahc_outb(ahc, CLRINT, CLRSEQINT);

	ahc_outb(ahc, MWI_RESIDUAL, 0);
	ahc_outb(ahc, SEQCTL, ahc->seqctl);
	ahc_outb(ahc, SEQADDR0, 0);
	ahc_outb(ahc, SEQADDR1, 0);

	/*
	 * Take the LED out of diagnostic mode on PM resume, too
	 */
	sblkctl = ahc_inb(ahc, SBLKCTL);
	ahc_outb(ahc, SBLKCTL, (sblkctl & ~(DIAGLEDEN|DIAGLEDON)));

	ahc_unpause(ahc);
}

/************************* Input/Output Queues ********************************/
static void
ahc_run_qoutfifo(struct ahc_softc *ahc)
{
	struct scb *scb;
	u_int  scb_index;

	ahc_sync_qoutfifo(ahc, BUS_DMASYNC_POSTREAD);
	while (ahc->qoutfifo[ahc->qoutfifonext] != SCB_LIST_NULL) {

		scb_index = ahc->qoutfifo[ahc->qoutfifonext];
		if ((ahc->qoutfifonext & 0x03) == 0x03) {
			u_int modnext;

			/*
			 * Clear 32bits of QOUTFIFO at a time
			 * so that we don't clobber an incoming
			 * byte DMA to the array on architectures
			 * that only support 32bit load and store
			 * operations.
			 */
			modnext = ahc->qoutfifonext & ~0x3;
			*((uint32_t *)(&ahc->qoutfifo[modnext])) = 0xFFFFFFFFUL;
			ahc_dmamap_sync(ahc, ahc->shared_data_dmat,
					ahc->shared_data_dmamap,
					/*offset*/modnext, /*len*/4,
					BUS_DMASYNC_PREREAD);
		}
		ahc->qoutfifonext++;

		scb = ahc_lookup_scb(ahc, scb_index);
		if (scb == NULL) {
			printf("%s: WARNING no command for scb %d "
			       "(cmdcmplt)\nQOUTPOS = %d\n",
			       ahc_name(ahc), scb_index,
			       (ahc->qoutfifonext - 1) & 0xFF);
			continue;
		}

		/*
		 * Save off the residual
		 * if there is one.
		 */
		ahc_update_residual(ahc, scb);
		ahc_done(ahc, scb);
	}
}

static void
ahc_run_untagged_queues(struct ahc_softc *ahc)
{
	int i;

	for (i = 0; i < 16; i++)
		ahc_run_untagged_queue(ahc, &ahc->untagged_queues[i]);
}

static void
ahc_run_untagged_queue(struct ahc_softc *ahc, struct scb_tailq *queue)
{
	struct scb *scb;

	if (ahc->untagged_queue_lock != 0)
		return;

	if ((scb = TAILQ_FIRST(queue)) != NULL
	 && (scb->flags & SCB_ACTIVE) == 0) {
		scb->flags |= SCB_ACTIVE;
		ahc_queue_scb(ahc, scb);
	}
}

/************************* Interrupt Handling *********************************/
static void
ahc_handle_brkadrint(struct ahc_softc *ahc)
{
	/*
	 * We upset the sequencer :-(
	 * Lookup the error message
	 */
	int i;
	int error;

	error = ahc_inb(ahc, ERROR);
	for (i = 0; error != 1 && i < num_errors; i++)
		error >>= 1;
	printf("%s: brkadrint, %s at seqaddr = 0x%x\n",
	       ahc_name(ahc), ahc_hard_errors[i].errmesg,
	       ahc_inb(ahc, SEQADDR0) |
	       (ahc_inb(ahc, SEQADDR1) << 8));

	ahc_dump_card_state(ahc);

	/* Tell everyone that this HBA is no longer available */
	ahc_abort_scbs(ahc, CAM_TARGET_WILDCARD, ALL_CHANNELS,
		       CAM_LUN_WILDCARD, SCB_LIST_NULL, ROLE_UNKNOWN,
		       CAM_NO_HBA);

	/* Disable all interrupt sources by resetting the controller */
	ahc_shutdown(ahc);
}

static void
ahc_handle_seqint(struct ahc_softc *ahc, u_int intstat)
{
	struct scb *scb;
	struct ahc_devinfo devinfo;
	
	ahc_fetch_devinfo(ahc, &devinfo);

	/*
	 * Clear the upper byte that holds SEQINT status
	 * codes and clear the SEQINT bit. We will unpause
	 * the sequencer, if appropriate, after servicing
	 * the request.
	 */
	ahc_outb(ahc, CLRINT, CLRSEQINT);
	switch (intstat & SEQINT_MASK) {
	case BAD_STATUS:
	{
		u_int  scb_index;
		struct hardware_scb *hscb;

		/*
		 * Set the default return value to 0 (don't
		 * send sense).  The sense code will change
		 * this if needed.
		 */
		ahc_outb(ahc, RETURN_1, 0);

		/*
		 * The sequencer will notify us when a command
		 * has an error that would be of interest to
		 * the kernel.  This allows us to leave the sequencer
		 * running in the common case of command completes
		 * without error.  The sequencer will already have
		 * dma'd the SCB back up to us, so we can reference
		 * the in kernel copy directly.
		 */
		scb_index = ahc_inb(ahc, SCB_TAG);
		scb = ahc_lookup_scb(ahc, scb_index);
		if (scb == NULL) {
			ahc_print_devinfo(ahc, &devinfo);
			printf("ahc_intr - referenced scb "
			       "not valid during seqint 0x%x scb(%d)\n",
			       intstat, scb_index);
			ahc_dump_card_state(ahc);
			panic("for safety");
			goto unpause;
		}

		hscb = scb->hscb; 

		/* Don't want to clobber the original sense code */
		if ((scb->flags & SCB_SENSE) != 0) {
			/*
			 * Clear the SCB_SENSE Flag and have
			 * the sequencer do a normal command
			 * complete.
			 */
			scb->flags &= ~SCB_SENSE;
			ahc_set_transaction_status(scb, CAM_AUTOSENSE_FAIL);
			break;
		}
		ahc_set_transaction_status(scb, CAM_SCSI_STATUS_ERROR);
		/* Freeze the queue until the client sees the error. */
		ahc_freeze_devq(ahc, scb);
		ahc_freeze_scb(scb);
		ahc_set_scsi_status(scb, hscb->shared_data.status.scsi_status);
		switch (hscb->shared_data.status.scsi_status) {
		case SCSI_STATUS_OK:
			printf("%s: Interrupted for staus of 0???\n",
			       ahc_name(ahc));
			break;
		case SCSI_STATUS_CMD_TERMINATED:
		case SCSI_STATUS_CHECK_COND:
		{
			struct ahc_dma_seg *sg;
			struct scsi_sense *sc;
			struct ahc_initiator_tinfo *targ_info;
			struct ahc_tmode_tstate *tstate;
			struct ahc_transinfo *tinfo;
#ifdef AHC_DEBUG
			if (ahc_debug & AHC_SHOW_SENSE) {
				ahc_print_path(ahc, scb);
				printf("SCB %d: requests Check Status\n",
				       scb->hscb->tag);
			}
#endif

			if (ahc_perform_autosense(scb) == 0)
				break;

			targ_info = ahc_fetch_transinfo(ahc,
							devinfo.channel,
							devinfo.our_scsiid,
							devinfo.target,
							&tstate);
			tinfo = &targ_info->curr;
			sg = scb->sg_list;
			sc = (struct scsi_sense *)(&hscb->shared_data.cdb); 
			/*
			 * Save off the residual if there is one.
			 */
			ahc_update_residual(ahc, scb);
#ifdef AHC_DEBUG
			if (ahc_debug & AHC_SHOW_SENSE) {
				ahc_print_path(ahc, scb);
				printf("Sending Sense\n");
			}
#endif
			sg->addr = ahc_get_sense_bufaddr(ahc, scb);
			sg->len = ahc_get_sense_bufsize(ahc, scb);
			sg->len |= AHC_DMA_LAST_SEG;

			/* Fixup byte order */
			sg->addr = ahc_htole32(sg->addr);
			sg->len = ahc_htole32(sg->len);

			sc->opcode = REQUEST_SENSE;
			sc->byte2 = 0;
			if (tinfo->protocol_version <= SCSI_REV_2
			 && SCB_GET_LUN(scb) < 8)
				sc->byte2 = SCB_GET_LUN(scb) << 5;
			sc->unused[0] = 0;
			sc->unused[1] = 0;
			sc->length = sg->len;
			sc->control = 0;

			/*
			 * We can't allow the target to disconnect.
			 * This will be an untagged transaction and
			 * having the target disconnect will make this
			 * transaction indestinguishable from outstanding
			 * tagged transactions.
			 */
			hscb->control = 0;

			/*
			 * This request sense could be because the
			 * the device lost power or in some other
			 * way has lost our transfer negotiations.
			 * Renegotiate if appropriate.  Unit attention
			 * errors will be reported before any data
			 * phases occur.
			 */
			if (ahc_get_residual(scb) 
			 == ahc_get_transfer_length(scb)) {
				ahc_update_neg_request(ahc, &devinfo,
						       tstate, targ_info,
						       AHC_NEG_IF_NON_ASYNC);
			}
			if (tstate->auto_negotiate & devinfo.target_mask) {
				hscb->control |= MK_MESSAGE;
				scb->flags &= ~SCB_NEGOTIATE;
				scb->flags |= SCB_AUTO_NEGOTIATE;
			}
			hscb->cdb_len = sizeof(*sc);
			hscb->dataptr = sg->addr; 
			hscb->datacnt = sg->len;
			hscb->sgptr = scb->sg_list_phys | SG_FULL_RESID;
			hscb->sgptr = ahc_htole32(hscb->sgptr);
			scb->sg_count = 1;
			scb->flags |= SCB_SENSE;
			ahc_qinfifo_requeue_tail(ahc, scb);
			ahc_outb(ahc, RETURN_1, SEND_SENSE);
			/*
			 * Ensure we have enough time to actually
			 * retrieve the sense.
			 */
			ahc_scb_timer_reset(scb, 5 * 1000000);
			break;
		}
		default:
			break;
		}
		break;
	}
	case NO_MATCH:
	{
		/* Ensure we don't leave the selection hardware on */
		ahc_outb(ahc, SCSISEQ,
			 ahc_inb(ahc, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP));

		printf("%s:%c:%d: no active SCB for reconnecting "
		       "target - issuing BUS DEVICE RESET\n",
		       ahc_name(ahc), devinfo.channel, devinfo.target);
		printf("SAVED_SCSIID == 0x%x, SAVED_LUN == 0x%x, "
		       "ARG_1 == 0x%x ACCUM = 0x%x\n",
		       ahc_inb(ahc, SAVED_SCSIID), ahc_inb(ahc, SAVED_LUN),
		       ahc_inb(ahc, ARG_1), ahc_inb(ahc, ACCUM));
		printf("SEQ_FLAGS == 0x%x, SCBPTR == 0x%x, BTT == 0x%x, "
		       "SINDEX == 0x%x\n",
		       ahc_inb(ahc, SEQ_FLAGS), ahc_inb(ahc, SCBPTR),
		       ahc_index_busy_tcl(ahc,
			    BUILD_TCL(ahc_inb(ahc, SAVED_SCSIID),
				      ahc_inb(ahc, SAVED_LUN))),
		       ahc_inb(ahc, SINDEX));
		printf("SCSIID == 0x%x, SCB_SCSIID == 0x%x, SCB_LUN == 0x%x, "
		       "SCB_TAG == 0x%x, SCB_CONTROL == 0x%x\n",
		       ahc_inb(ahc, SCSIID), ahc_inb(ahc, SCB_SCSIID),
		       ahc_inb(ahc, SCB_LUN), ahc_inb(ahc, SCB_TAG),
		       ahc_inb(ahc, SCB_CONTROL));
		printf("SCSIBUSL == 0x%x, SCSISIGI == 0x%x\n",
		       ahc_inb(ahc, SCSIBUSL), ahc_inb(ahc, SCSISIGI));
		printf("SXFRCTL0 == 0x%x\n", ahc_inb(ahc, SXFRCTL0));
		printf("SEQCTL == 0x%x\n", ahc_inb(ahc, SEQCTL));
		ahc_dump_card_state(ahc);
		ahc->msgout_buf[0] = MSG_BUS_DEV_RESET;
		ahc->msgout_len = 1;
		ahc->msgout_index = 0;
		ahc->msg_type = MSG_TYPE_INITIATOR_MSGOUT;
		ahc_outb(ahc, MSG_OUT, HOST_MSG);
		ahc_assert_atn(ahc);
		break;
	}
	case SEND_REJECT: 
	{
		u_int rejbyte = ahc_inb(ahc, ACCUM);
		printf("%s:%c:%d: Warning - unknown message received from "
		       "target (0x%x).  Rejecting\n", 
		       ahc_name(ahc), devinfo.channel, devinfo.target, rejbyte);
		break; 
	}
	case PROTO_VIOLATION:
	{
		ahc_handle_proto_violation(ahc);
		break;
	}
	case IGN_WIDE_RES:
		ahc_handle_ign_wide_residue(ahc, &devinfo);
		break;
	case PDATA_REINIT:
		ahc_reinitialize_dataptrs(ahc);
		break;
	case BAD_PHASE:
	{
		u_int lastphase;

		lastphase = ahc_inb(ahc, LASTPHASE);
		printf("%s:%c:%d: unknown scsi bus phase %x, "
		       "lastphase = 0x%x.  Attempting to continue\n",
		       ahc_name(ahc), devinfo.channel, devinfo.target,
		       lastphase, ahc_inb(ahc, SCSISIGI));
		break;
	}
	case MISSED_BUSFREE:
	{
		u_int lastphase;

		lastphase = ahc_inb(ahc, LASTPHASE);
		printf("%s:%c:%d: Missed busfree. "
		       "Lastphase = 0x%x, Curphase = 0x%x\n",
		       ahc_name(ahc), devinfo.channel, devinfo.target,
		       lastphase, ahc_inb(ahc, SCSISIGI));
		ahc_restart(ahc);
		return;
	}
	case HOST_MSG_LOOP:
	{
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
		if (ahc->msg_type == MSG_TYPE_NONE) {
			struct scb *scb;
			u_int scb_index;
			u_int bus_phase;

			bus_phase = ahc_inb(ahc, SCSISIGI) & PHASE_MASK;
			if (bus_phase != P_MESGIN
			 && bus_phase != P_MESGOUT) {
				printf("ahc_intr: HOST_MSG_LOOP bad "
				       "phase 0x%x\n",
				      bus_phase);
				/*
				 * Probably transitioned to bus free before
				 * we got here.  Just punt the message.
				 */
				ahc_clear_intstat(ahc);
				ahc_restart(ahc);
				return;
			}

			scb_index = ahc_inb(ahc, SCB_TAG);
			scb = ahc_lookup_scb(ahc, scb_index);
			if (devinfo.role == ROLE_INITIATOR) {
				if (bus_phase == P_MESGOUT) {
					if (scb == NULL)
						panic("HOST_MSG_LOOP with "
						      "invalid SCB %x\n",
						      scb_index);

					ahc_setup_initiator_msgout(ahc,
								   &devinfo,
								   scb);
				} else {
					ahc->msg_type =
					    MSG_TYPE_INITIATOR_MSGIN;
					ahc->msgin_index = 0;
				}
			}
#ifdef AHC_TARGET_MODE
			else {
				if (bus_phase == P_MESGOUT) {
					ahc->msg_type =
					    MSG_TYPE_TARGET_MSGOUT;
					ahc->msgin_index = 0;
				}
				else 
					ahc_setup_target_msgin(ahc,
							       &devinfo,
							       scb);
			}
#endif
		}

		ahc_handle_message_phase(ahc);
		break;
	}
	case PERR_DETECTED:
	{
		/*
		 * If we've cleared the parity error interrupt
		 * but the sequencer still believes that SCSIPERR
		 * is true, it must be that the parity error is
		 * for the currently presented byte on the bus,
		 * and we are not in a phase (data-in) where we will
		 * eventually ack this byte.  Ack the byte and
		 * throw it away in the hope that the target will
		 * take us to message out to deliver the appropriate
		 * error message.
		 */
		if ((intstat & SCSIINT) == 0
		 && (ahc_inb(ahc, SSTAT1) & SCSIPERR) != 0) {

			if ((ahc->features & AHC_DT) == 0) {
				u_int curphase;

				/*
				 * The hardware will only let you ack bytes
				 * if the expected phase in SCSISIGO matches
				 * the current phase.  Make sure this is
				 * currently the case.
				 */
				curphase = ahc_inb(ahc, SCSISIGI) & PHASE_MASK;
				ahc_outb(ahc, LASTPHASE, curphase);
				ahc_outb(ahc, SCSISIGO, curphase);
			}
			if ((ahc_inb(ahc, SCSISIGI) & (CDI|MSGI)) == 0) {
				int wait;

				/*
				 * In a data phase.  Faster to bitbucket
				 * the data than to individually ack each
				 * byte.  This is also the only strategy
				 * that will work with AUTOACK enabled.
				 */
				ahc_outb(ahc, SXFRCTL1,
					 ahc_inb(ahc, SXFRCTL1) | BITBUCKET);
				wait = 5000;
				while (--wait != 0) {
					if ((ahc_inb(ahc, SCSISIGI)
					  & (CDI|MSGI)) != 0)
						break;
					ahc_delay(100);
				}
				ahc_outb(ahc, SXFRCTL1,
					 ahc_inb(ahc, SXFRCTL1) & ~BITBUCKET);
				if (wait == 0) {
					struct	scb *scb;
					u_int	scb_index;

					ahc_print_devinfo(ahc, &devinfo);
					printf("Unable to clear parity error.  "
					       "Resetting bus.\n");
					scb_index = ahc_inb(ahc, SCB_TAG);
					scb = ahc_lookup_scb(ahc, scb_index);
					if (scb != NULL)
						ahc_set_transaction_status(scb,
						    CAM_UNCOR_PARITY);
					ahc_reset_channel(ahc, devinfo.channel, 
							  /*init reset*/TRUE);
				}
			} else {
				ahc_inb(ahc, SCSIDATL);
			}
		}
		break;
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
		u_int scbindex = ahc_inb(ahc, SCB_TAG);
		u_int lastphase = ahc_inb(ahc, LASTPHASE);
		u_int i;

		scb = ahc_lookup_scb(ahc, scbindex);
		for (i = 0; i < num_phases; i++) {
			if (lastphase == ahc_phase_table[i].phase)
				break;
		}
		ahc_print_path(ahc, scb);
		printf("data overrun detected %s."
		       "  Tag == 0x%x.\n",
		       ahc_phase_table[i].phasemsg,
  		       scb->hscb->tag);
		ahc_print_path(ahc, scb);
		printf("%s seen Data Phase.  Length = %ld.  NumSGs = %d.\n",
		       ahc_inb(ahc, SEQ_FLAGS) & DPHASE ? "Have" : "Haven't",
		       ahc_get_transfer_length(scb), scb->sg_count);
		if (scb->sg_count > 0) {
			for (i = 0; i < scb->sg_count; i++) {

				printf("sg[%d] - Addr 0x%x%x : Length %d\n",
				       i,
				       (ahc_le32toh(scb->sg_list[i].len) >> 24
				        & SG_HIGH_ADDR_BITS),
				       ahc_le32toh(scb->sg_list[i].addr),
				       ahc_le32toh(scb->sg_list[i].len)
				       & AHC_SG_LEN_MASK);
			}
		}
		/*
		 * Set this and it will take effect when the
		 * target does a command complete.
		 */
		ahc_freeze_devq(ahc, scb);
		if ((scb->flags & SCB_SENSE) == 0) {
			ahc_set_transaction_status(scb, CAM_DATA_RUN_ERR);
		} else {
			scb->flags &= ~SCB_SENSE;
			ahc_set_transaction_status(scb, CAM_AUTOSENSE_FAIL);
		}
		ahc_freeze_scb(scb);

		if ((ahc->features & AHC_ULTRA2) != 0) {
			/*
			 * Clear the channel in case we return
			 * to data phase later.
			 */
			ahc_outb(ahc, SXFRCTL0,
				 ahc_inb(ahc, SXFRCTL0) | CLRSTCNT|CLRCHN);
			ahc_outb(ahc, SXFRCTL0,
				 ahc_inb(ahc, SXFRCTL0) | CLRSTCNT|CLRCHN);
		}
		if ((ahc->flags & AHC_39BIT_ADDRESSING) != 0) {
			u_int dscommand1;

			/* Ensure HHADDR is 0 for future DMA operations. */
			dscommand1 = ahc_inb(ahc, DSCOMMAND1);
			ahc_outb(ahc, DSCOMMAND1, dscommand1 | HADDLDSEL0);
			ahc_outb(ahc, HADDR, 0);
			ahc_outb(ahc, DSCOMMAND1, dscommand1);
		}
		break;
	}
	case MKMSG_FAILED:
	{
		u_int scbindex;

		printf("%s:%c:%d:%d: Attempt to issue message failed\n",
		       ahc_name(ahc), devinfo.channel, devinfo.target,
		       devinfo.lun);
		scbindex = ahc_inb(ahc, SCB_TAG);
		scb = ahc_lookup_scb(ahc, scbindex);
		if (scb != NULL
		 && (scb->flags & SCB_RECOVERY_SCB) != 0)
			/*
			 * Ensure that we didn't put a second instance of this
			 * SCB into the QINFIFO.
			 */
			ahc_search_qinfifo(ahc, SCB_GET_TARGET(ahc, scb),
					   SCB_GET_CHANNEL(ahc, scb),
					   SCB_GET_LUN(scb), scb->hscb->tag,
					   ROLE_INITIATOR, /*status*/0,
					   SEARCH_REMOVE);
		break;
	}
	case NO_FREE_SCB:
	{
		printf("%s: No free or disconnected SCBs\n", ahc_name(ahc));
		ahc_dump_card_state(ahc);
		panic("for safety");
		break;
	}
	case SCB_MISMATCH:
	{
		u_int scbptr;

		scbptr = ahc_inb(ahc, SCBPTR);
		printf("Bogus TAG after DMA.  SCBPTR %d, tag %d, our tag %d\n",
		       scbptr, ahc_inb(ahc, ARG_1),
		       ahc->scb_data->hscbs[scbptr].tag);
		ahc_dump_card_state(ahc);
		panic("for saftey");
		break;
	}
	case OUT_OF_RANGE:
	{
		printf("%s: BTT calculation out of range\n", ahc_name(ahc));
		printf("SAVED_SCSIID == 0x%x, SAVED_LUN == 0x%x, "
		       "ARG_1 == 0x%x ACCUM = 0x%x\n",
		       ahc_inb(ahc, SAVED_SCSIID), ahc_inb(ahc, SAVED_LUN),
		       ahc_inb(ahc, ARG_1), ahc_inb(ahc, ACCUM));
		printf("SEQ_FLAGS == 0x%x, SCBPTR == 0x%x, BTT == 0x%x, "
		       "SINDEX == 0x%x\n, A == 0x%x\n",
		       ahc_inb(ahc, SEQ_FLAGS), ahc_inb(ahc, SCBPTR),
		       ahc_index_busy_tcl(ahc,
			    BUILD_TCL(ahc_inb(ahc, SAVED_SCSIID),
				      ahc_inb(ahc, SAVED_LUN))),
		       ahc_inb(ahc, SINDEX),
		       ahc_inb(ahc, ACCUM));
		printf("SCSIID == 0x%x, SCB_SCSIID == 0x%x, SCB_LUN == 0x%x, "
		       "SCB_TAG == 0x%x, SCB_CONTROL == 0x%x\n",
		       ahc_inb(ahc, SCSIID), ahc_inb(ahc, SCB_SCSIID),
		       ahc_inb(ahc, SCB_LUN), ahc_inb(ahc, SCB_TAG),
		       ahc_inb(ahc, SCB_CONTROL));
		printf("SCSIBUSL == 0x%x, SCSISIGI == 0x%x\n",
		       ahc_inb(ahc, SCSIBUSL), ahc_inb(ahc, SCSISIGI));
		ahc_dump_card_state(ahc);
		panic("for safety");
		break;
	}
	default:
		printf("ahc_intr: seqint, "
		       "intstat == 0x%x, scsisigi = 0x%x\n",
		       intstat, ahc_inb(ahc, SCSISIGI));
		break;
	}
unpause:
	/*
	 *  The sequencer is paused immediately on
	 *  a SEQINT, so we should restart it when
	 *  we're done.
	 */
	ahc_unpause(ahc);
}

static void
ahc_handle_scsiint(struct ahc_softc *ahc, u_int intstat)
{
	u_int	scb_index;
	u_int	status0;
	u_int	status;
	struct	scb *scb;
	char	cur_channel;
	char	intr_channel;

	if ((ahc->features & AHC_TWIN) != 0
	 && ((ahc_inb(ahc, SBLKCTL) & SELBUSB) != 0))
		cur_channel = 'B';
	else
		cur_channel = 'A';
	intr_channel = cur_channel;

	if ((ahc->features & AHC_ULTRA2) != 0)
		status0 = ahc_inb(ahc, SSTAT0) & IOERR;
	else
		status0 = 0;
	status = ahc_inb(ahc, SSTAT1) & (SELTO|SCSIRSTI|BUSFREE|SCSIPERR);
	if (status == 0 && status0 == 0) {
		if ((ahc->features & AHC_TWIN) != 0) {
			/* Try the other channel */
		 	ahc_outb(ahc, SBLKCTL, ahc_inb(ahc, SBLKCTL) ^ SELBUSB);
			status = ahc_inb(ahc, SSTAT1)
			       & (SELTO|SCSIRSTI|BUSFREE|SCSIPERR);
			intr_channel = (cur_channel == 'A') ? 'B' : 'A';
		}
		if (status == 0) {
			printf("%s: Spurious SCSI interrupt\n", ahc_name(ahc));
			ahc_outb(ahc, CLRINT, CLRSCSIINT);
			ahc_unpause(ahc);
			return;
		}
	}

	/* Make sure the sequencer is in a safe location. */
	ahc_clear_critical_section(ahc);

	scb_index = ahc_inb(ahc, SCB_TAG);
	scb = ahc_lookup_scb(ahc, scb_index);
	if (scb != NULL
	 && (ahc_inb(ahc, SEQ_FLAGS) & NOT_IDENTIFIED) != 0)
		scb = NULL;

	if ((ahc->features & AHC_ULTRA2) != 0
	 && (status0 & IOERR) != 0) {
		int now_lvd;

		now_lvd = ahc_inb(ahc, SBLKCTL) & ENAB40;
		printf("%s: Transceiver State Has Changed to %s mode\n",
		       ahc_name(ahc), now_lvd ? "LVD" : "SE");
		ahc_outb(ahc, CLRSINT0, CLRIOERR);
		/*
		 * When transitioning to SE mode, the reset line
		 * glitches, triggering an arbitration bug in some
		 * Ultra2 controllers.  This bug is cleared when we
		 * assert the reset line.  Since a reset glitch has
		 * already occurred with this transition and a
		 * transceiver state change is handled just like
		 * a bus reset anyway, asserting the reset line
		 * ourselves is safe.
		 */
		ahc_reset_channel(ahc, intr_channel,
				 /*Initiate Reset*/now_lvd == 0);
	} else if ((status & SCSIRSTI) != 0) {
		printf("%s: Someone reset channel %c\n",
			ahc_name(ahc), intr_channel);
		if (intr_channel != cur_channel)
		 	ahc_outb(ahc, SBLKCTL, ahc_inb(ahc, SBLKCTL) ^ SELBUSB);
		ahc_reset_channel(ahc, intr_channel, /*Initiate Reset*/FALSE);
	} else if ((status & SCSIPERR) != 0) {
		/*
		 * Determine the bus phase and queue an appropriate message.
		 * SCSIPERR is latched true as soon as a parity error
		 * occurs.  If the sequencer acked the transfer that
		 * caused the parity error and the currently presented
		 * transfer on the bus has correct parity, SCSIPERR will
		 * be cleared by CLRSCSIPERR.  Use this to determine if
		 * we should look at the last phase the sequencer recorded,
		 * or the current phase presented on the bus.
		 */
		struct	ahc_devinfo devinfo;
		u_int	mesg_out;
		u_int	curphase;
		u_int	errorphase;
		u_int	lastphase;
		u_int	scsirate;
		u_int	i;
		u_int	sstat2;
		int	silent;

		lastphase = ahc_inb(ahc, LASTPHASE);
		curphase = ahc_inb(ahc, SCSISIGI) & PHASE_MASK;
		sstat2 = ahc_inb(ahc, SSTAT2);
		ahc_outb(ahc, CLRSINT1, CLRSCSIPERR);
		/*
		 * For all phases save DATA, the sequencer won't
		 * automatically ack a byte that has a parity error
		 * in it.  So the only way that the current phase
		 * could be 'data-in' is if the parity error is for
		 * an already acked byte in the data phase.  During
		 * synchronous data-in transfers, we may actually
		 * ack bytes before latching the current phase in
		 * LASTPHASE, leading to the discrepancy between
		 * curphase and lastphase.
		 */
		if ((ahc_inb(ahc, SSTAT1) & SCSIPERR) != 0
		 || curphase == P_DATAIN || curphase == P_DATAIN_DT)
			errorphase = curphase;
		else
			errorphase = lastphase;

		for (i = 0; i < num_phases; i++) {
			if (errorphase == ahc_phase_table[i].phase)
				break;
		}
		mesg_out = ahc_phase_table[i].mesg_out;
		silent = FALSE;
		if (scb != NULL) {
			if (SCB_IS_SILENT(scb))
				silent = TRUE;
			else
				ahc_print_path(ahc, scb);
			scb->flags |= SCB_TRANSMISSION_ERROR;
		} else
			printf("%s:%c:%d: ", ahc_name(ahc), intr_channel,
			       SCSIID_TARGET(ahc, ahc_inb(ahc, SAVED_SCSIID)));
		scsirate = ahc_inb(ahc, SCSIRATE);
		if (silent == FALSE) {
			printf("parity error detected %s. "
			       "SEQADDR(0x%x) SCSIRATE(0x%x)\n",
			       ahc_phase_table[i].phasemsg,
			       ahc_inw(ahc, SEQADDR0),
			       scsirate);
			if ((ahc->features & AHC_DT) != 0) {
				if ((sstat2 & CRCVALERR) != 0)
					printf("\tCRC Value Mismatch\n");
				if ((sstat2 & CRCENDERR) != 0)
					printf("\tNo terminal CRC packet "
					       "recevied\n");
				if ((sstat2 & CRCREQERR) != 0)
					printf("\tIllegal CRC packet "
					       "request\n");
				if ((sstat2 & DUAL_EDGE_ERR) != 0)
					printf("\tUnexpected %sDT Data Phase\n",
					       (scsirate & SINGLE_EDGE)
					     ? "" : "non-");
			}
		}

		if ((ahc->features & AHC_DT) != 0
		 && (sstat2 & DUAL_EDGE_ERR) != 0) {
			/*
			 * This error applies regardless of
			 * data direction, so ignore the value
			 * in the phase table.
			 */
			mesg_out = MSG_INITIATOR_DET_ERR;
		}

		/*
		 * We've set the hardware to assert ATN if we   
		 * get a parity error on "in" phases, so all we  
		 * need to do is stuff the message buffer with
		 * the appropriate message.  "In" phases have set
		 * mesg_out to something other than MSG_NOP.
		 */
		if (mesg_out != MSG_NOOP) {
			if (ahc->msg_type != MSG_TYPE_NONE)
				ahc->send_msg_perror = TRUE;
			else
				ahc_outb(ahc, MSG_OUT, mesg_out);
		}
		/*
		 * Force a renegotiation with this target just in
		 * case we are out of sync for some external reason
		 * unknown (or unreported) by the target.
		 */
		ahc_fetch_devinfo(ahc, &devinfo);
		ahc_force_renegotiation(ahc, &devinfo);

		ahc_outb(ahc, CLRINT, CLRSCSIINT);
		ahc_unpause(ahc);
	} else if ((status & SELTO) != 0) {
		u_int	scbptr;

		/* Stop the selection */
		ahc_outb(ahc, SCSISEQ, 0);

		/* No more pending messages */
		ahc_clear_msg_state(ahc);

		/* Clear interrupt state */
		ahc_outb(ahc, SIMODE1, ahc_inb(ahc, SIMODE1) & ~ENBUSFREE);
		ahc_outb(ahc, CLRSINT1, CLRSELTIMEO|CLRBUSFREE|CLRSCSIPERR);

		/*
		 * Although the driver does not care about the
		 * 'Selection in Progress' status bit, the busy
		 * LED does.  SELINGO is only cleared by a sucessfull
		 * selection, so we must manually clear it to insure
		 * the LED turns off just incase no future successful
		 * selections occur (e.g. no devices on the bus).
		 */
		ahc_outb(ahc, CLRSINT0, CLRSELINGO);

		scbptr = ahc_inb(ahc, WAITING_SCBH);
		ahc_outb(ahc, SCBPTR, scbptr);
		scb_index = ahc_inb(ahc, SCB_TAG);

		scb = ahc_lookup_scb(ahc, scb_index);
		if (scb == NULL) {
			printf("%s: ahc_intr - referenced scb not "
			       "valid during SELTO scb(%d, %d)\n",
			       ahc_name(ahc), scbptr, scb_index);
			ahc_dump_card_state(ahc);
		} else {
			struct ahc_devinfo devinfo;
#ifdef AHC_DEBUG
			if ((ahc_debug & AHC_SHOW_SELTO) != 0) {
				ahc_print_path(ahc, scb);
				printf("Saw Selection Timeout for SCB 0x%x\n",
				       scb_index);
			}
#endif
			ahc_scb_devinfo(ahc, &devinfo, scb);
			ahc_set_transaction_status(scb, CAM_SEL_TIMEOUT);
			ahc_freeze_devq(ahc, scb);

			/*
			 * Cancel any pending transactions on the device
			 * now that it seems to be missing.  This will
			 * also revert us to async/narrow transfers until
			 * we can renegotiate with the device.
			 */
			ahc_handle_devreset(ahc, &devinfo,
					    CAM_SEL_TIMEOUT,
					    "Selection Timeout",
					    /*verbose_level*/1);
		}
		ahc_outb(ahc, CLRINT, CLRSCSIINT);
		ahc_restart(ahc);
	} else if ((status & BUSFREE) != 0
		&& (ahc_inb(ahc, SIMODE1) & ENBUSFREE) != 0) {
		struct	ahc_devinfo devinfo;
		u_int	lastphase;
		u_int	saved_scsiid;
		u_int	saved_lun;
		u_int	target;
		u_int	initiator_role_id;
		char	channel;
		int	printerror;

		/*
		 * Clear our selection hardware as soon as possible.
		 * We may have an entry in the waiting Q for this target,
		 * that is affected by this busfree and we don't want to
		 * go about selecting the target while we handle the event.
		 */
		ahc_outb(ahc, SCSISEQ,
			 ahc_inb(ahc, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP));

		/*
		 * Disable busfree interrupts and clear the busfree
		 * interrupt status.  We do this here so that several
		 * bus transactions occur prior to clearing the SCSIINT
		 * latch.  It can take a bit for the clearing to take effect.
		 */
		ahc_outb(ahc, SIMODE1, ahc_inb(ahc, SIMODE1) & ~ENBUSFREE);
		ahc_outb(ahc, CLRSINT1, CLRBUSFREE|CLRSCSIPERR);

		/*
		 * Look at what phase we were last in.
		 * If its message out, chances are pretty good
		 * that the busfree was in response to one of
		 * our abort requests.
		 */
		lastphase = ahc_inb(ahc, LASTPHASE);
		saved_scsiid = ahc_inb(ahc, SAVED_SCSIID);
		saved_lun = ahc_inb(ahc, SAVED_LUN);
		target = SCSIID_TARGET(ahc, saved_scsiid);
		initiator_role_id = SCSIID_OUR_ID(saved_scsiid);
		channel = SCSIID_CHANNEL(ahc, saved_scsiid);
		ahc_compile_devinfo(&devinfo, initiator_role_id,
				    target, saved_lun, channel, ROLE_INITIATOR);
		printerror = 1;

		if (lastphase == P_MESGOUT) {
			u_int tag;

			tag = SCB_LIST_NULL;
			if (ahc_sent_msg(ahc, AHCMSG_1B, MSG_ABORT_TAG, TRUE)
			 || ahc_sent_msg(ahc, AHCMSG_1B, MSG_ABORT, TRUE)) {
				if (ahc->msgout_buf[ahc->msgout_index - 1]
				 == MSG_ABORT_TAG)
					tag = scb->hscb->tag;
				ahc_print_path(ahc, scb);
				printf("SCB %d - Abort%s Completed.\n",
				       scb->hscb->tag, tag == SCB_LIST_NULL ?
				       "" : " Tag");
				ahc_abort_scbs(ahc, target, channel,
					       saved_lun, tag,
					       ROLE_INITIATOR,
					       CAM_REQ_ABORTED);
				printerror = 0;
			} else if (ahc_sent_msg(ahc, AHCMSG_1B,
						MSG_BUS_DEV_RESET, TRUE)) {
#ifdef __FreeBSD__
				/*
				 * Don't mark the user's request for this BDR
				 * as completing with CAM_BDR_SENT.  CAM3
				 * specifies CAM_REQ_CMP.
				 */
				if (scb != NULL
				 && scb->io_ctx->ccb_h.func_code== XPT_RESET_DEV
				 && ahc_match_scb(ahc, scb, target, channel,
						  CAM_LUN_WILDCARD,
						  SCB_LIST_NULL,
						  ROLE_INITIATOR)) {
					ahc_set_transaction_status(scb, CAM_REQ_CMP);
				}
#endif
				ahc_compile_devinfo(&devinfo,
						    initiator_role_id,
						    target,
						    CAM_LUN_WILDCARD,
						    channel,
						    ROLE_INITIATOR);
				ahc_handle_devreset(ahc, &devinfo,
						    CAM_BDR_SENT,
						    "Bus Device Reset",
						    /*verbose_level*/0);
				printerror = 0;
			} else if (ahc_sent_msg(ahc, AHCMSG_EXT,
						MSG_EXT_PPR, FALSE)) {
				struct ahc_initiator_tinfo *tinfo;
				struct ahc_tmode_tstate *tstate;

				/*
				 * PPR Rejected.  Try non-ppr negotiation
				 * and retry command.
				 */
				tinfo = ahc_fetch_transinfo(ahc,
							    devinfo.channel,
							    devinfo.our_scsiid,
							    devinfo.target,
							    &tstate);
				tinfo->curr.transport_version = 2;
				tinfo->goal.transport_version = 2;
				tinfo->goal.ppr_options = 0;
				ahc_qinfifo_requeue_tail(ahc, scb);
				printerror = 0;
			} else if (ahc_sent_msg(ahc, AHCMSG_EXT,
						MSG_EXT_WDTR, FALSE)) {
				/*
				 * Negotiation Rejected.  Go-narrow and
				 * retry command.
				 */
				ahc_set_width(ahc, &devinfo,
					      MSG_EXT_WDTR_BUS_8_BIT,
					      AHC_TRANS_CUR|AHC_TRANS_GOAL,
					      /*paused*/TRUE);
				ahc_qinfifo_requeue_tail(ahc, scb);
				printerror = 0;
			} else if (ahc_sent_msg(ahc, AHCMSG_EXT,
						MSG_EXT_SDTR, FALSE)) {
				/*
				 * Negotiation Rejected.  Go-async and
				 * retry command.
				 */
				ahc_set_syncrate(ahc, &devinfo,
						/*syncrate*/NULL,
						/*period*/0, /*offset*/0,
						/*ppr_options*/0,
						AHC_TRANS_CUR|AHC_TRANS_GOAL,
						/*paused*/TRUE);
				ahc_qinfifo_requeue_tail(ahc, scb);
				printerror = 0;
			}
		}
		if (printerror != 0) {
			u_int i;

			if (scb != NULL) {
				u_int tag;

				if ((scb->hscb->control & TAG_ENB) != 0)
					tag = scb->hscb->tag;
				else
					tag = SCB_LIST_NULL;
				ahc_print_path(ahc, scb);
				ahc_abort_scbs(ahc, target, channel,
					       SCB_GET_LUN(scb), tag,
					       ROLE_INITIATOR,
					       CAM_UNEXP_BUSFREE);
			} else {
				/*
				 * We had not fully identified this connection,
				 * so we cannot abort anything.
				 */
				printf("%s: ", ahc_name(ahc));
			}
			for (i = 0; i < num_phases; i++) {
				if (lastphase == ahc_phase_table[i].phase)
					break;
			}
			if (lastphase != P_BUSFREE) {
				/*
				 * Renegotiate with this device at the
				 * next oportunity just in case this busfree
				 * is due to a negotiation mismatch with the
				 * device.
				 */
				ahc_force_renegotiation(ahc, &devinfo);
			}
			printf("Unexpected busfree %s\n"
			       "SEQADDR == 0x%x\n",
			       ahc_phase_table[i].phasemsg,
			       ahc_inb(ahc, SEQADDR0)
				| (ahc_inb(ahc, SEQADDR1) << 8));
		}
		ahc_outb(ahc, CLRINT, CLRSCSIINT);
		ahc_restart(ahc);
	} else {
		printf("%s: Missing case in ahc_handle_scsiint. status = %x\n",
		       ahc_name(ahc), status);
		ahc_outb(ahc, CLRINT, CLRSCSIINT);
	}
}

/*
 * Force renegotiation to occur the next time we initiate
 * a command to the current device.
 */
static void
ahc_force_renegotiation(struct ahc_softc *ahc, struct ahc_devinfo *devinfo)
{
	struct	ahc_initiator_tinfo *targ_info;
	struct	ahc_tmode_tstate *tstate;

	targ_info = ahc_fetch_transinfo(ahc,
					devinfo->channel,
					devinfo->our_scsiid,
					devinfo->target,
					&tstate);
	ahc_update_neg_request(ahc, devinfo, tstate,
			       targ_info, AHC_NEG_IF_NON_ASYNC);
}

#define AHC_MAX_STEPS 2000
static void
ahc_clear_critical_section(struct ahc_softc *ahc)
{
	int	stepping;
	int	steps;
	u_int	simode0;
	u_int	simode1;

	if (ahc->num_critical_sections == 0)
		return;

	stepping = FALSE;
	steps = 0;
	simode0 = 0;
	simode1 = 0;
	for (;;) {
		struct	cs *cs;
		u_int	seqaddr;
		u_int	i;

		seqaddr = ahc_inb(ahc, SEQADDR0)
			| (ahc_inb(ahc, SEQADDR1) << 8);

		/*
		 * Seqaddr represents the next instruction to execute, 
		 * so we are really executing the instruction just
		 * before it.
		 */
		if (seqaddr != 0)
			seqaddr -= 1;
		cs = ahc->critical_sections;
		for (i = 0; i < ahc->num_critical_sections; i++, cs++) {
			
			if (cs->begin < seqaddr && cs->end >= seqaddr)
				break;
		}

		if (i == ahc->num_critical_sections)
			break;

		if (steps > AHC_MAX_STEPS) {
			printf("%s: Infinite loop in critical section\n",
			       ahc_name(ahc));
			ahc_dump_card_state(ahc);
			panic("critical section loop");
		}

		steps++;
		if (stepping == FALSE) {

			/*
			 * Disable all interrupt sources so that the
			 * sequencer will not be stuck by a pausing
			 * interrupt condition while we attempt to
			 * leave a critical section.
			 */
			simode0 = ahc_inb(ahc, SIMODE0);
			ahc_outb(ahc, SIMODE0, 0);
			simode1 = ahc_inb(ahc, SIMODE1);
			if ((ahc->features & AHC_DT) != 0)
				/*
				 * On DT class controllers, we
				 * use the enhanced busfree logic.
				 * Unfortunately we cannot re-enable
				 * busfree detection within the
				 * current connection, so we must
				 * leave it on while single stepping.
				 */
				ahc_outb(ahc, SIMODE1, simode1 & ENBUSFREE);
			else
				ahc_outb(ahc, SIMODE1, 0);
			ahc_outb(ahc, CLRINT, CLRSCSIINT);
			ahc_outb(ahc, SEQCTL, ahc->seqctl | STEP);
			stepping = TRUE;
		}
		if ((ahc->features & AHC_DT) != 0) {
			ahc_outb(ahc, CLRSINT1, CLRBUSFREE);
			ahc_outb(ahc, CLRINT, CLRSCSIINT);
		}
		ahc_outb(ahc, HCNTRL, ahc->unpause);
		while (!ahc_is_paused(ahc))
			ahc_delay(200);
	}
	if (stepping) {
		ahc_outb(ahc, SIMODE0, simode0);
		ahc_outb(ahc, SIMODE1, simode1);
		ahc_outb(ahc, SEQCTL, ahc->seqctl);
	}
}

/*
 * Clear any pending interrupt status.
 */
static void
ahc_clear_intstat(struct ahc_softc *ahc)
{
	/* Clear any interrupt conditions this may have caused */
	ahc_outb(ahc, CLRSINT1, CLRSELTIMEO|CLRATNO|CLRSCSIRSTI
				|CLRBUSFREE|CLRSCSIPERR|CLRPHASECHG|
				CLRREQINIT);
	ahc_flush_device_writes(ahc);
	ahc_outb(ahc, CLRSINT0, CLRSELDO|CLRSELDI|CLRSELINGO);
 	ahc_flush_device_writes(ahc);
	ahc_outb(ahc, CLRINT, CLRSCSIINT);
	ahc_flush_device_writes(ahc);
}

/**************************** Debugging Routines ******************************/
#ifdef AHC_DEBUG
uint32_t ahc_debug = AHC_DEBUG_OPTS;
#endif

#if 0 /* unused */
static void
ahc_print_scb(struct scb *scb)
{
	int i;

	struct hardware_scb *hscb = scb->hscb;

	printf("scb:%p control:0x%x scsiid:0x%x lun:%d cdb_len:%d\n",
	       (void *)scb,
	       hscb->control,
	       hscb->scsiid,
	       hscb->lun,
	       hscb->cdb_len);
	printf("Shared Data: ");
	for (i = 0; i < sizeof(hscb->shared_data.cdb); i++)
		printf("%#02x", hscb->shared_data.cdb[i]);
	printf("        dataptr:%#x datacnt:%#x sgptr:%#x tag:%#x\n",
		ahc_le32toh(hscb->dataptr),
		ahc_le32toh(hscb->datacnt),
		ahc_le32toh(hscb->sgptr),
		hscb->tag);
	if (scb->sg_count > 0) {
		for (i = 0; i < scb->sg_count; i++) {
			printf("sg[%d] - Addr 0x%x%x : Length %d\n",
			       i,
			       (ahc_le32toh(scb->sg_list[i].len) >> 24
			        & SG_HIGH_ADDR_BITS),
			       ahc_le32toh(scb->sg_list[i].addr),
			       ahc_le32toh(scb->sg_list[i].len));
		}
	}
}
#endif

/************************* Transfer Negotiation *******************************/
/*
 * Allocate per target mode instance (ID we respond to as a target)
 * transfer negotiation data structures.
 */
static struct ahc_tmode_tstate *
ahc_alloc_tstate(struct ahc_softc *ahc, u_int scsi_id, char channel)
{
	struct ahc_tmode_tstate *master_tstate;
	struct ahc_tmode_tstate *tstate;
	int i;

	master_tstate = ahc->enabled_targets[ahc->our_id];
	if (channel == 'B') {
		scsi_id += 8;
		master_tstate = ahc->enabled_targets[ahc->our_id_b + 8];
	}
	if (ahc->enabled_targets[scsi_id] != NULL
	 && ahc->enabled_targets[scsi_id] != master_tstate)
		panic("%s: ahc_alloc_tstate - Target already allocated",
		      ahc_name(ahc));
	tstate = (struct ahc_tmode_tstate*)malloc(sizeof(*tstate),
						   M_DEVBUF, M_NOWAIT);
	if (tstate == NULL)
		return (NULL);

	/*
	 * If we have allocated a master tstate, copy user settings from
	 * the master tstate (taken from SRAM or the EEPROM) for this
	 * channel, but reset our current and goal settings to async/narrow
	 * until an initiator talks to us.
	 */
	if (master_tstate != NULL) {
		memcpy(tstate, master_tstate, sizeof(*tstate));
		memset(tstate->enabled_luns, 0, sizeof(tstate->enabled_luns));
		tstate->ultraenb = 0;
		for (i = 0; i < AHC_NUM_TARGETS; i++) {
			memset(&tstate->transinfo[i].curr, 0,
			      sizeof(tstate->transinfo[i].curr));
			memset(&tstate->transinfo[i].goal, 0,
			      sizeof(tstate->transinfo[i].goal));
		}
	} else
		memset(tstate, 0, sizeof(*tstate));
	ahc->enabled_targets[scsi_id] = tstate;
	return (tstate);
}

#ifdef AHC_TARGET_MODE
/*
 * Free per target mode instance (ID we respond to as a target)
 * transfer negotiation data structures.
 */
static void
ahc_free_tstate(struct ahc_softc *ahc, u_int scsi_id, char channel, int force)
{
	struct ahc_tmode_tstate *tstate;

	/*
	 * Don't clean up our "master" tstate.
	 * It has our default user settings.
	 */
	if (((channel == 'B' && scsi_id == ahc->our_id_b)
	  || (channel == 'A' && scsi_id == ahc->our_id))
	 && force == FALSE)
		return;

	if (channel == 'B')
		scsi_id += 8;
	tstate = ahc->enabled_targets[scsi_id];
	if (tstate != NULL)
		free(tstate, M_DEVBUF);
	ahc->enabled_targets[scsi_id] = NULL;
}
#endif

/*
 * Called when we have an active connection to a target on the bus,
 * this function finds the nearest syncrate to the input period limited
 * by the capabilities of the bus connectivity of and sync settings for
 * the target.
 */
const struct ahc_syncrate *
ahc_devlimited_syncrate(struct ahc_softc *ahc,
			struct ahc_initiator_tinfo *tinfo,
			u_int *period, u_int *ppr_options, role_t role)
{
	struct	ahc_transinfo *transinfo;
	u_int	maxsync;

	if ((ahc->features & AHC_ULTRA2) != 0) {
		if ((ahc_inb(ahc, SBLKCTL) & ENAB40) != 0
		 && (ahc_inb(ahc, SSTAT2) & EXP_ACTIVE) == 0) {
			maxsync = AHC_SYNCRATE_DT;
		} else {
			maxsync = AHC_SYNCRATE_ULTRA;
			/* Can't do DT on an SE bus */
			*ppr_options &= ~MSG_EXT_PPR_DT_REQ;
		}
	} else if ((ahc->features & AHC_ULTRA) != 0) {
		maxsync = AHC_SYNCRATE_ULTRA;
	} else {
		maxsync = AHC_SYNCRATE_FAST;
	}
	/*
	 * Never allow a value higher than our current goal
	 * period otherwise we may allow a target initiated
	 * negotiation to go above the limit as set by the
	 * user.  In the case of an initiator initiated
	 * sync negotiation, we limit based on the user
	 * setting.  This allows the system to still accept
	 * incoming negotiations even if target initiated
	 * negotiation is not performed.
	 */
	if (role == ROLE_TARGET)
		transinfo = &tinfo->user;
	else 
		transinfo = &tinfo->goal;
	*ppr_options &= transinfo->ppr_options;
	if (transinfo->width == MSG_EXT_WDTR_BUS_8_BIT) {
		maxsync = max(maxsync, (u_int)AHC_SYNCRATE_ULTRA2);
		*ppr_options &= ~MSG_EXT_PPR_DT_REQ;
	}
	if (transinfo->period == 0) {
		*period = 0;
		*ppr_options = 0;
		return (NULL);
	}
	*period = max(*period, (u_int)transinfo->period);
	return (ahc_find_syncrate(ahc, period, ppr_options, maxsync));
}

/*
 * Look up the valid period to SCSIRATE conversion in our table.
 * Return the period and offset that should be sent to the target
 * if this was the beginning of an SDTR.
 */
const struct ahc_syncrate *
ahc_find_syncrate(struct ahc_softc *ahc, u_int *period,
		  u_int *ppr_options, u_int maxsync)
{
	const struct ahc_syncrate *syncrate;

	if ((ahc->features & AHC_DT) == 0)
		*ppr_options &= ~MSG_EXT_PPR_DT_REQ;

	/* Skip all DT only entries if DT is not available */
	if ((*ppr_options & MSG_EXT_PPR_DT_REQ) == 0
	 && maxsync < AHC_SYNCRATE_ULTRA2)
		maxsync = AHC_SYNCRATE_ULTRA2;

	/* Now set the maxsync based on the card capabilities
	 * DT is already done above */
	if ((ahc->features & (AHC_DT | AHC_ULTRA2)) == 0
	    && maxsync < AHC_SYNCRATE_ULTRA)
		maxsync = AHC_SYNCRATE_ULTRA;
	if ((ahc->features & (AHC_DT | AHC_ULTRA2 | AHC_ULTRA)) == 0
	    && maxsync < AHC_SYNCRATE_FAST)
		maxsync = AHC_SYNCRATE_FAST;

	for (syncrate = &ahc_syncrates[maxsync];
	     syncrate->rate != NULL;
	     syncrate++) {

		/*
		 * The Ultra2 table doesn't go as low
		 * as for the Fast/Ultra cards.
		 */
		if ((ahc->features & AHC_ULTRA2) != 0
		 && (syncrate->sxfr_u2 == 0))
			break;

		if (*period <= syncrate->period) {
			/*
			 * When responding to a target that requests
			 * sync, the requested rate may fall between
			 * two rates that we can output, but still be
			 * a rate that we can receive.  Because of this,
			 * we want to respond to the target with
			 * the same rate that it sent to us even
			 * if the period we use to send data to it
			 * is lower.  Only lower the response period
			 * if we must.
			 */
			if (syncrate == &ahc_syncrates[maxsync])
				*period = syncrate->period;

			/*
			 * At some speeds, we only support
			 * ST transfers.
			 */
		 	if ((syncrate->sxfr_u2 & ST_SXFR) != 0)
				*ppr_options &= ~MSG_EXT_PPR_DT_REQ;
			break;
		}
	}

	if ((*period == 0)
	 || (syncrate->rate == NULL)
	 || ((ahc->features & AHC_ULTRA2) != 0
	  && (syncrate->sxfr_u2 == 0))) {
		/* Use asynchronous transfers. */
		*period = 0;
		syncrate = NULL;
		*ppr_options &= ~MSG_EXT_PPR_DT_REQ;
	}
	return (syncrate);
}

/*
 * Convert from an entry in our syncrate table to the SCSI equivalent
 * sync "period" factor.
 */
u_int
ahc_find_period(struct ahc_softc *ahc, u_int scsirate, u_int maxsync)
{
	const struct ahc_syncrate *syncrate;

	if ((ahc->features & AHC_ULTRA2) != 0)
		scsirate &= SXFR_ULTRA2;
	else
		scsirate &= SXFR;

	/* now set maxsync based on card capabilities */
	if ((ahc->features & AHC_DT) == 0 && maxsync < AHC_SYNCRATE_ULTRA2)
		maxsync = AHC_SYNCRATE_ULTRA2;
	if ((ahc->features & (AHC_DT | AHC_ULTRA2)) == 0
	    && maxsync < AHC_SYNCRATE_ULTRA)
		maxsync = AHC_SYNCRATE_ULTRA;
	if ((ahc->features & (AHC_DT | AHC_ULTRA2 | AHC_ULTRA)) == 0
	    && maxsync < AHC_SYNCRATE_FAST)
		maxsync = AHC_SYNCRATE_FAST;


	syncrate = &ahc_syncrates[maxsync];
	while (syncrate->rate != NULL) {

		if ((ahc->features & AHC_ULTRA2) != 0) {
			if (syncrate->sxfr_u2 == 0)
				break;
			else if (scsirate == (syncrate->sxfr_u2 & SXFR_ULTRA2))
				return (syncrate->period);
		} else if (scsirate == (syncrate->sxfr & SXFR)) {
				return (syncrate->period);
		}
		syncrate++;
	}
	return (0); /* async */
}

/*
 * Truncate the given synchronous offset to a value the
 * current adapter type and syncrate are capable of.
 */
static void
ahc_validate_offset(struct ahc_softc *ahc,
		    struct ahc_initiator_tinfo *tinfo,
		    const struct ahc_syncrate *syncrate,
		    u_int *offset, int wide, role_t role)
{
	u_int maxoffset;

	/* Limit offset to what we can do */
	if (syncrate == NULL) {
		maxoffset = 0;
	} else if ((ahc->features & AHC_ULTRA2) != 0) {
		maxoffset = MAX_OFFSET_ULTRA2;
	} else {
		if (wide)
			maxoffset = MAX_OFFSET_16BIT;
		else
			maxoffset = MAX_OFFSET_8BIT;
	}
	*offset = min(*offset, maxoffset);
	if (tinfo != NULL) {
		if (role == ROLE_TARGET)
			*offset = min(*offset, (u_int)tinfo->user.offset);
		else
			*offset = min(*offset, (u_int)tinfo->goal.offset);
	}
}

/*
 * Truncate the given transfer width parameter to a value the
 * current adapter type is capable of.
 */
static void
ahc_validate_width(struct ahc_softc *ahc, struct ahc_initiator_tinfo *tinfo,
		   u_int *bus_width, role_t role)
{
	switch (*bus_width) {
	default:
		if (ahc->features & AHC_WIDE) {
			/* Respond Wide */
			*bus_width = MSG_EXT_WDTR_BUS_16_BIT;
			break;
		}
		/* FALLTHROUGH */
	case MSG_EXT_WDTR_BUS_8_BIT:
		*bus_width = MSG_EXT_WDTR_BUS_8_BIT;
		break;
	}
	if (tinfo != NULL) {
		if (role == ROLE_TARGET)
			*bus_width = min((u_int)tinfo->user.width, *bus_width);
		else
			*bus_width = min((u_int)tinfo->goal.width, *bus_width);
	}
}

/*
 * Update the bitmask of targets for which the controller should
 * negotiate with at the next convenient oportunity.  This currently
 * means the next time we send the initial identify messages for
 * a new transaction.
 */
int
ahc_update_neg_request(struct ahc_softc *ahc, struct ahc_devinfo *devinfo,
		       struct ahc_tmode_tstate *tstate,
		       struct ahc_initiator_tinfo *tinfo, ahc_neg_type neg_type)
{
	u_int auto_negotiate_orig;

	auto_negotiate_orig = tstate->auto_negotiate;
	if (neg_type == AHC_NEG_ALWAYS) {
		/*
		 * Force our "current" settings to be
		 * unknown so that unless a bus reset
		 * occurs the need to renegotiate is
		 * recorded persistently.
		 */
		if ((ahc->features & AHC_WIDE) != 0)
			tinfo->curr.width = AHC_WIDTH_UNKNOWN;
		tinfo->curr.period = AHC_PERIOD_UNKNOWN;
		tinfo->curr.offset = AHC_OFFSET_UNKNOWN;
	}
	if (tinfo->curr.period != tinfo->goal.period
	 || tinfo->curr.width != tinfo->goal.width
	 || tinfo->curr.offset != tinfo->goal.offset
	 || tinfo->curr.ppr_options != tinfo->goal.ppr_options
	 || (neg_type == AHC_NEG_IF_NON_ASYNC
	  && (tinfo->goal.offset != 0
	   || tinfo->goal.width != MSG_EXT_WDTR_BUS_8_BIT
	   || tinfo->goal.ppr_options != 0)))
		tstate->auto_negotiate |= devinfo->target_mask;
	else
		tstate->auto_negotiate &= ~devinfo->target_mask;

	return (auto_negotiate_orig != tstate->auto_negotiate);
}

/*
 * Update the user/goal/curr tables of synchronous negotiation
 * parameters as well as, in the case of a current or active update,
 * any data structures on the host controller.  In the case of an
 * active update, the specified target is currently talking to us on
 * the bus, so the transfer parameter update must take effect
 * immediately.
 */
void
ahc_set_syncrate(struct ahc_softc *ahc, struct ahc_devinfo *devinfo,
		 const struct ahc_syncrate *syncrate, u_int period,
		 u_int offset, u_int ppr_options, u_int type, int paused)
{
	struct	ahc_initiator_tinfo *tinfo;
	struct	ahc_tmode_tstate *tstate;
	u_int	old_period;
	u_int	old_offset;
	u_int	old_ppr;
	int	active;
	int	update_needed;

	active = (type & AHC_TRANS_ACTIVE) == AHC_TRANS_ACTIVE;
	update_needed = 0;

	if (syncrate == NULL) {
		period = 0;
		offset = 0;
	}

	tinfo = ahc_fetch_transinfo(ahc, devinfo->channel, devinfo->our_scsiid,
				    devinfo->target, &tstate);

	if ((type & AHC_TRANS_USER) != 0) {
		tinfo->user.period = period;
		tinfo->user.offset = offset;
		tinfo->user.ppr_options = ppr_options;
	}

	if ((type & AHC_TRANS_GOAL) != 0) {
		tinfo->goal.period = period;
		tinfo->goal.offset = offset;
		tinfo->goal.ppr_options = ppr_options;
	}

	old_period = tinfo->curr.period;
	old_offset = tinfo->curr.offset;
	old_ppr	   = tinfo->curr.ppr_options;

	if ((type & AHC_TRANS_CUR) != 0
	 && (old_period != period
	  || old_offset != offset
	  || old_ppr != ppr_options)) {
		u_int	scsirate;

		update_needed++;
		scsirate = tinfo->scsirate;
		if ((ahc->features & AHC_ULTRA2) != 0) {

			scsirate &= ~(SXFR_ULTRA2|SINGLE_EDGE|ENABLE_CRC);
			if (syncrate != NULL) {
				scsirate |= syncrate->sxfr_u2;
				if ((ppr_options & MSG_EXT_PPR_DT_REQ) != 0)
					scsirate |= ENABLE_CRC;
				else
					scsirate |= SINGLE_EDGE;
			}
		} else {

			scsirate &= ~(SXFR|SOFS);
			/*
			 * Ensure Ultra mode is set properly for
			 * this target.
			 */
			tstate->ultraenb &= ~devinfo->target_mask;
			if (syncrate != NULL) {
				if (syncrate->sxfr & ULTRA_SXFR) {
					tstate->ultraenb |=
						devinfo->target_mask;
				}
				scsirate |= syncrate->sxfr & SXFR;
				scsirate |= offset & SOFS;
			}
			if (active) {
				u_int sxfrctl0;

				sxfrctl0 = ahc_inb(ahc, SXFRCTL0);
				sxfrctl0 &= ~FAST20;
				if (tstate->ultraenb & devinfo->target_mask)
					sxfrctl0 |= FAST20;
				ahc_outb(ahc, SXFRCTL0, sxfrctl0);
			}
		}
		if (active) {
			ahc_outb(ahc, SCSIRATE, scsirate);
			if ((ahc->features & AHC_ULTRA2) != 0)
				ahc_outb(ahc, SCSIOFFSET, offset);
		}

		tinfo->scsirate = scsirate;
		tinfo->curr.period = period;
		tinfo->curr.offset = offset;
		tinfo->curr.ppr_options = ppr_options;

		ahc_send_async(ahc, devinfo->channel, devinfo->target,
			       CAM_LUN_WILDCARD, AC_TRANSFER_NEG);
		if (bootverbose) {
			if (offset != 0) {
				printf("%s: target %d synchronous at %sMHz%s, "
				       "offset = 0x%x\n", ahc_name(ahc),
				       devinfo->target, syncrate->rate,
				       (ppr_options & MSG_EXT_PPR_DT_REQ)
				       ? " DT" : "", offset);
			} else {
				printf("%s: target %d using "
				       "asynchronous transfers\n",
				       ahc_name(ahc), devinfo->target);
			}
		}
	}

	update_needed += ahc_update_neg_request(ahc, devinfo, tstate,
						tinfo, AHC_NEG_TO_GOAL);

	if (update_needed)
		ahc_update_pending_scbs(ahc);
}

/*
 * Update the user/goal/curr tables of wide negotiation
 * parameters as well as, in the case of a current or active update,
 * any data structures on the host controller.  In the case of an
 * active update, the specified target is currently talking to us on
 * the bus, so the transfer parameter update must take effect
 * immediately.
 */
void
ahc_set_width(struct ahc_softc *ahc, struct ahc_devinfo *devinfo,
	      u_int width, u_int type, int paused)
{
	struct	ahc_initiator_tinfo *tinfo;
	struct	ahc_tmode_tstate *tstate;
	u_int	oldwidth;
	int	active;
	int	update_needed;

	active = (type & AHC_TRANS_ACTIVE) == AHC_TRANS_ACTIVE;
	update_needed = 0;
	tinfo = ahc_fetch_transinfo(ahc, devinfo->channel, devinfo->our_scsiid,
				    devinfo->target, &tstate);

	if ((type & AHC_TRANS_USER) != 0)
		tinfo->user.width = width;

	if ((type & AHC_TRANS_GOAL) != 0)
		tinfo->goal.width = width;

	oldwidth = tinfo->curr.width;
	if ((type & AHC_TRANS_CUR) != 0 && oldwidth != width) {
		u_int	scsirate;

		update_needed++;
		scsirate =  tinfo->scsirate;
		scsirate &= ~WIDEXFER;
		if (width == MSG_EXT_WDTR_BUS_16_BIT)
			scsirate |= WIDEXFER;

		tinfo->scsirate = scsirate;

		if (active)
			ahc_outb(ahc, SCSIRATE, scsirate);

		tinfo->curr.width = width;

		ahc_send_async(ahc, devinfo->channel, devinfo->target,
			       CAM_LUN_WILDCARD, AC_TRANSFER_NEG);
		if (bootverbose) {
			printf("%s: target %d using %dbit transfers\n",
			       ahc_name(ahc), devinfo->target,
			       8 * (0x01 << width));
		}
	}

	update_needed += ahc_update_neg_request(ahc, devinfo, tstate,
						tinfo, AHC_NEG_TO_GOAL);
	if (update_needed)
		ahc_update_pending_scbs(ahc);
}

/*
 * Update the current state of tagged queuing for a given target.
 */
static void
ahc_set_tags(struct ahc_softc *ahc, struct scsi_cmnd *cmd,
	     struct ahc_devinfo *devinfo, ahc_queue_alg alg)
{
	struct scsi_device *sdev = cmd->device;

 	ahc_platform_set_tags(ahc, sdev, devinfo, alg);
 	ahc_send_async(ahc, devinfo->channel, devinfo->target,
 		       devinfo->lun, AC_TRANSFER_NEG);
}

/*
 * When the transfer settings for a connection change, update any
 * in-transit SCBs to contain the new data so the hardware will
 * be set correctly during future (re)selections.
 */
static void
ahc_update_pending_scbs(struct ahc_softc *ahc)
{
	struct	scb *pending_scb;
	int	pending_scb_count;
	int	i;
	int	paused;
	u_int	saved_scbptr;

	/*
	 * Traverse the pending SCB list and ensure that all of the
	 * SCBs there have the proper settings.
	 */
	pending_scb_count = 0;
	LIST_FOREACH(pending_scb, &ahc->pending_scbs, pending_links) {
		struct ahc_devinfo devinfo;
		struct hardware_scb *pending_hscb;
		struct ahc_initiator_tinfo *tinfo;
		struct ahc_tmode_tstate *tstate;

		ahc_scb_devinfo(ahc, &devinfo, pending_scb);
		tinfo = ahc_fetch_transinfo(ahc, devinfo.channel,
					    devinfo.our_scsiid,
					    devinfo.target, &tstate);
		pending_hscb = pending_scb->hscb;
		pending_hscb->control &= ~ULTRAENB;
		if ((tstate->ultraenb & devinfo.target_mask) != 0)
			pending_hscb->control |= ULTRAENB;
		pending_hscb->scsirate = tinfo->scsirate;
		pending_hscb->scsioffset = tinfo->curr.offset;
		if ((tstate->auto_negotiate & devinfo.target_mask) == 0
		 && (pending_scb->flags & SCB_AUTO_NEGOTIATE) != 0) {
			pending_scb->flags &= ~SCB_AUTO_NEGOTIATE;
			pending_hscb->control &= ~MK_MESSAGE;
		}
		ahc_sync_scb(ahc, pending_scb,
			     BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		pending_scb_count++;
	}

	if (pending_scb_count == 0)
		return;

	if (ahc_is_paused(ahc)) {
		paused = 1;
	} else {
		paused = 0;
		ahc_pause(ahc);
	}

	saved_scbptr = ahc_inb(ahc, SCBPTR);
	/* Ensure that the hscbs down on the card match the new information */
	for (i = 0; i < ahc->scb_data->maxhscbs; i++) {
		struct	hardware_scb *pending_hscb;
		u_int	control;
		u_int	scb_tag;

		ahc_outb(ahc, SCBPTR, i);
		scb_tag = ahc_inb(ahc, SCB_TAG);
		pending_scb = ahc_lookup_scb(ahc, scb_tag);
		if (pending_scb == NULL)
			continue;

		pending_hscb = pending_scb->hscb;
		control = ahc_inb(ahc, SCB_CONTROL);
		control &= ~(ULTRAENB|MK_MESSAGE);
		control |= pending_hscb->control & (ULTRAENB|MK_MESSAGE);
		ahc_outb(ahc, SCB_CONTROL, control);
		ahc_outb(ahc, SCB_SCSIRATE, pending_hscb->scsirate);
		ahc_outb(ahc, SCB_SCSIOFFSET, pending_hscb->scsioffset);
	}
	ahc_outb(ahc, SCBPTR, saved_scbptr);

	if (paused == 0)
		ahc_unpause(ahc);
}

/**************************** Pathing Information *****************************/
static void
ahc_fetch_devinfo(struct ahc_softc *ahc, struct ahc_devinfo *devinfo)
{
	u_int	saved_scsiid;
	role_t	role;
	int	our_id;

	if (ahc_inb(ahc, SSTAT0) & TARGET)
		role = ROLE_TARGET;
	else
		role = ROLE_INITIATOR;

	if (role == ROLE_TARGET
	 && (ahc->features & AHC_MULTI_TID) != 0
	 && (ahc_inb(ahc, SEQ_FLAGS)
 	   & (CMDPHASE_PENDING|TARG_CMD_PENDING|NO_DISCONNECT)) != 0) {
		/* We were selected, so pull our id from TARGIDIN */
		our_id = ahc_inb(ahc, TARGIDIN) & OID;
	} else if ((ahc->features & AHC_ULTRA2) != 0)
		our_id = ahc_inb(ahc, SCSIID_ULTRA2) & OID;
	else
		our_id = ahc_inb(ahc, SCSIID) & OID;

	saved_scsiid = ahc_inb(ahc, SAVED_SCSIID);
	ahc_compile_devinfo(devinfo,
			    our_id,
			    SCSIID_TARGET(ahc, saved_scsiid),
			    ahc_inb(ahc, SAVED_LUN),
			    SCSIID_CHANNEL(ahc, saved_scsiid),
			    role);
}

static const struct ahc_phase_table_entry*
ahc_lookup_phase_entry(int phase)
{
	const struct ahc_phase_table_entry *entry;
	const struct ahc_phase_table_entry *last_entry;

	/*
	 * num_phases doesn't include the default entry which
	 * will be returned if the phase doesn't match.
	 */
	last_entry = &ahc_phase_table[num_phases];
	for (entry = ahc_phase_table; entry < last_entry; entry++) {
		if (phase == entry->phase)
			break;
	}
	return (entry);
}

void
ahc_compile_devinfo(struct ahc_devinfo *devinfo, u_int our_id, u_int target,
		    u_int lun, char channel, role_t role)
{
	devinfo->our_scsiid = our_id;
	devinfo->target = target;
	devinfo->lun = lun;
	devinfo->target_offset = target;
	devinfo->channel = channel;
	devinfo->role = role;
	if (channel == 'B')
		devinfo->target_offset += 8;
	devinfo->target_mask = (0x01 << devinfo->target_offset);
}

void
ahc_print_devinfo(struct ahc_softc *ahc, struct ahc_devinfo *devinfo)
{
	printf("%s:%c:%d:%d: ", ahc_name(ahc), devinfo->channel,
	       devinfo->target, devinfo->lun);
}

static void
ahc_scb_devinfo(struct ahc_softc *ahc, struct ahc_devinfo *devinfo,
		struct scb *scb)
{
	role_t	role;
	int	our_id;

	our_id = SCSIID_OUR_ID(scb->hscb->scsiid);
	role = ROLE_INITIATOR;
	if ((scb->flags & SCB_TARGET_SCB) != 0)
		role = ROLE_TARGET;
	ahc_compile_devinfo(devinfo, our_id, SCB_GET_TARGET(ahc, scb),
			    SCB_GET_LUN(scb), SCB_GET_CHANNEL(ahc, scb), role);
}


/************************ Message Phase Processing ****************************/
static void
ahc_assert_atn(struct ahc_softc *ahc)
{
	u_int scsisigo;

	scsisigo = ATNO;
	if ((ahc->features & AHC_DT) == 0)
		scsisigo |= ahc_inb(ahc, SCSISIGI);
	ahc_outb(ahc, SCSISIGO, scsisigo);
}

/*
 * When an initiator transaction with the MK_MESSAGE flag either reconnects
 * or enters the initial message out phase, we are interrupted.  Fill our
 * outgoing message buffer with the appropriate message and beging handing
 * the message phase(s) manually.
 */
static void
ahc_setup_initiator_msgout(struct ahc_softc *ahc, struct ahc_devinfo *devinfo,
			   struct scb *scb)
{
	/*
	 * To facilitate adding multiple messages together,
	 * each routine should increment the index and len
	 * variables instead of setting them explicitly.
	 */
	ahc->msgout_index = 0;
	ahc->msgout_len = 0;

	if ((scb->flags & SCB_DEVICE_RESET) == 0
	 && ahc_inb(ahc, MSG_OUT) == MSG_IDENTIFYFLAG) {
		u_int identify_msg;

		identify_msg = MSG_IDENTIFYFLAG | SCB_GET_LUN(scb);
		if ((scb->hscb->control & DISCENB) != 0)
			identify_msg |= MSG_IDENTIFY_DISCFLAG;
		ahc->msgout_buf[ahc->msgout_index++] = identify_msg;
		ahc->msgout_len++;

		if ((scb->hscb->control & TAG_ENB) != 0) {
			ahc->msgout_buf[ahc->msgout_index++] =
			    scb->hscb->control & (TAG_ENB|SCB_TAG_TYPE);
			ahc->msgout_buf[ahc->msgout_index++] = scb->hscb->tag;
			ahc->msgout_len += 2;
		}
	}

	if (scb->flags & SCB_DEVICE_RESET) {
		ahc->msgout_buf[ahc->msgout_index++] = MSG_BUS_DEV_RESET;
		ahc->msgout_len++;
		ahc_print_path(ahc, scb);
		printf("Bus Device Reset Message Sent\n");
		/*
		 * Clear our selection hardware in advance of
		 * the busfree.  We may have an entry in the waiting
		 * Q for this target, and we don't want to go about
		 * selecting while we handle the busfree and blow it
		 * away.
		 */
		ahc_outb(ahc, SCSISEQ, (ahc_inb(ahc, SCSISEQ) & ~ENSELO));
	} else if ((scb->flags & SCB_ABORT) != 0) {
		if ((scb->hscb->control & TAG_ENB) != 0)
			ahc->msgout_buf[ahc->msgout_index++] = MSG_ABORT_TAG;
		else
			ahc->msgout_buf[ahc->msgout_index++] = MSG_ABORT;
		ahc->msgout_len++;
		ahc_print_path(ahc, scb);
		printf("Abort%s Message Sent\n",
		       (scb->hscb->control & TAG_ENB) != 0 ? " Tag" : "");
		/*
		 * Clear our selection hardware in advance of
		 * the busfree.  We may have an entry in the waiting
		 * Q for this target, and we don't want to go about
		 * selecting while we handle the busfree and blow it
		 * away.
		 */
		ahc_outb(ahc, SCSISEQ, (ahc_inb(ahc, SCSISEQ) & ~ENSELO));
	} else if ((scb->flags & (SCB_AUTO_NEGOTIATE|SCB_NEGOTIATE)) != 0) {
		ahc_build_transfer_msg(ahc, devinfo);
	} else {
		printf("ahc_intr: AWAITING_MSG for an SCB that "
		       "does not have a waiting message\n");
		printf("SCSIID = %x, target_mask = %x\n", scb->hscb->scsiid,
		       devinfo->target_mask);
		panic("SCB = %d, SCB Control = %x, MSG_OUT = %x "
		      "SCB flags = %x", scb->hscb->tag, scb->hscb->control,
		      ahc_inb(ahc, MSG_OUT), scb->flags);
	}

	/*
	 * Clear the MK_MESSAGE flag from the SCB so we aren't
	 * asked to send this message again.
	 */
	ahc_outb(ahc, SCB_CONTROL, ahc_inb(ahc, SCB_CONTROL) & ~MK_MESSAGE);
	scb->hscb->control &= ~MK_MESSAGE;
	ahc->msgout_index = 0;
	ahc->msg_type = MSG_TYPE_INITIATOR_MSGOUT;
}

/*
 * Build an appropriate transfer negotiation message for the
 * currently active target.
 */
static void
ahc_build_transfer_msg(struct ahc_softc *ahc, struct ahc_devinfo *devinfo)
{
	/*
	 * We need to initiate transfer negotiations.
	 * If our current and goal settings are identical,
	 * we want to renegotiate due to a check condition.
	 */
	struct	ahc_initiator_tinfo *tinfo;
	struct	ahc_tmode_tstate *tstate;
	const struct ahc_syncrate *rate;
	int	dowide;
	int	dosync;
	int	doppr;
	u_int	period;
	u_int	ppr_options;
	u_int	offset;

	tinfo = ahc_fetch_transinfo(ahc, devinfo->channel, devinfo->our_scsiid,
				    devinfo->target, &tstate);
	/*
	 * Filter our period based on the current connection.
	 * If we can't perform DT transfers on this segment (not in LVD
	 * mode for instance), then our decision to issue a PPR message
	 * may change.
	 */
	period = tinfo->goal.period;
	offset = tinfo->goal.offset;
	ppr_options = tinfo->goal.ppr_options;
	/* Target initiated PPR is not allowed in the SCSI spec */
	if (devinfo->role == ROLE_TARGET)
		ppr_options = 0;
	rate = ahc_devlimited_syncrate(ahc, tinfo, &period,
				       &ppr_options, devinfo->role);
	dowide = tinfo->curr.width != tinfo->goal.width;
	dosync = tinfo->curr.offset != offset || tinfo->curr.period != period;
	/*
	 * Only use PPR if we have options that need it, even if the device
	 * claims to support it.  There might be an expander in the way
	 * that doesn't.
	 */
	doppr = ppr_options != 0;

	if (!dowide && !dosync && !doppr) {
		dowide = tinfo->goal.width != MSG_EXT_WDTR_BUS_8_BIT;
		dosync = tinfo->goal.offset != 0;
	}

	if (!dowide && !dosync && !doppr) {
		/*
		 * Force async with a WDTR message if we have a wide bus,
		 * or just issue an SDTR with a 0 offset.
		 */
		if ((ahc->features & AHC_WIDE) != 0)
			dowide = 1;
		else
			dosync = 1;

		if (bootverbose) {
			ahc_print_devinfo(ahc, devinfo);
			printf("Ensuring async\n");
		}
	}

	/* Target initiated PPR is not allowed in the SCSI spec */
	if (devinfo->role == ROLE_TARGET)
		doppr = 0;

	/*
	 * Both the PPR message and SDTR message require the
	 * goal syncrate to be limited to what the target device
	 * is capable of handling (based on whether an LVD->SE
	 * expander is on the bus), so combine these two cases.
	 * Regardless, guarantee that if we are using WDTR and SDTR
	 * messages that WDTR comes first.
	 */
	if (doppr || (dosync && !dowide)) {

		offset = tinfo->goal.offset;
		ahc_validate_offset(ahc, tinfo, rate, &offset,
				    doppr ? tinfo->goal.width
					  : tinfo->curr.width,
				    devinfo->role);
		if (doppr) {
			ahc_construct_ppr(ahc, devinfo, period, offset,
					  tinfo->goal.width, ppr_options);
		} else {
			ahc_construct_sdtr(ahc, devinfo, period, offset);
		}
	} else {
		ahc_construct_wdtr(ahc, devinfo, tinfo->goal.width);
	}
}

/*
 * Build a synchronous negotiation message in our message
 * buffer based on the input parameters.
 */
static void
ahc_construct_sdtr(struct ahc_softc *ahc, struct ahc_devinfo *devinfo,
		   u_int period, u_int offset)
{
	if (offset == 0)
		period = AHC_ASYNC_XFER_PERIOD;
	ahc->msgout_index += spi_populate_sync_msg(
			ahc->msgout_buf + ahc->msgout_index, period, offset);
	ahc->msgout_len += 5;
	if (bootverbose) {
		printf("(%s:%c:%d:%d): Sending SDTR period %x, offset %x\n",
		       ahc_name(ahc), devinfo->channel, devinfo->target,
		       devinfo->lun, period, offset);
	}
}

/*
 * Build a wide negotiation message in our message
 * buffer based on the input parameters.
 */
static void
ahc_construct_wdtr(struct ahc_softc *ahc, struct ahc_devinfo *devinfo,
		   u_int bus_width)
{
	ahc->msgout_index += spi_populate_width_msg(
			ahc->msgout_buf + ahc->msgout_index, bus_width);
	ahc->msgout_len += 4;
	if (bootverbose) {
		printf("(%s:%c:%d:%d): Sending WDTR %x\n",
		       ahc_name(ahc), devinfo->channel, devinfo->target,
		       devinfo->lun, bus_width);
	}
}

/*
 * Build a parallel protocol request message in our message
 * buffer based on the input parameters.
 */
static void
ahc_construct_ppr(struct ahc_softc *ahc, struct ahc_devinfo *devinfo,
		  u_int period, u_int offset, u_int bus_width,
		  u_int ppr_options)
{
	if (offset == 0)
		period = AHC_ASYNC_XFER_PERIOD;
	ahc->msgout_index += spi_populate_ppr_msg(
			ahc->msgout_buf + ahc->msgout_index, period, offset,
			bus_width, ppr_options);
	ahc->msgout_len += 8;
	if (bootverbose) {
		printf("(%s:%c:%d:%d): Sending PPR bus_width %x, period %x, "
		       "offset %x, ppr_options %x\n", ahc_name(ahc),
		       devinfo->channel, devinfo->target, devinfo->lun,
		       bus_width, period, offset, ppr_options);
	}
}

/*
 * Clear any active message state.
 */
static void
ahc_clear_msg_state(struct ahc_softc *ahc)
{
	ahc->msgout_len = 0;
	ahc->msgin_index = 0;
	ahc->msg_type = MSG_TYPE_NONE;
	if ((ahc_inb(ahc, SCSISIGI) & ATNI) != 0) {
		/*
		 * The target didn't care to respond to our
		 * message request, so clear ATN.
		 */
		ahc_outb(ahc, CLRSINT1, CLRATNO);
	}
	ahc_outb(ahc, MSG_OUT, MSG_NOOP);
	ahc_outb(ahc, SEQ_FLAGS2,
		 ahc_inb(ahc, SEQ_FLAGS2) & ~TARGET_MSG_PENDING);
}

static void
ahc_handle_proto_violation(struct ahc_softc *ahc)
{
	struct	ahc_devinfo devinfo;
	struct	scb *scb;
	u_int	scbid;
	u_int	seq_flags;
	u_int	curphase;
	u_int	lastphase;
	int	found;

	ahc_fetch_devinfo(ahc, &devinfo);
	scbid = ahc_inb(ahc, SCB_TAG);
	scb = ahc_lookup_scb(ahc, scbid);
	seq_flags = ahc_inb(ahc, SEQ_FLAGS);
	curphase = ahc_inb(ahc, SCSISIGI) & PHASE_MASK;
	lastphase = ahc_inb(ahc, LASTPHASE);
	if ((seq_flags & NOT_IDENTIFIED) != 0) {

		/*
		 * The reconnecting target either did not send an
		 * identify message, or did, but we didn't find an SCB
		 * to match.
		 */
		ahc_print_devinfo(ahc, &devinfo);
		printf("Target did not send an IDENTIFY message. "
		       "LASTPHASE = 0x%x.\n", lastphase);
		scb = NULL;
	} else if (scb == NULL) {
		/*
		 * We don't seem to have an SCB active for this
		 * transaction.  Print an error and reset the bus.
		 */
		ahc_print_devinfo(ahc, &devinfo);
		printf("No SCB found during protocol violation\n");
		goto proto_violation_reset;
	} else {
		ahc_set_transaction_status(scb, CAM_SEQUENCE_FAIL);
		if ((seq_flags & NO_CDB_SENT) != 0) {
			ahc_print_path(ahc, scb);
			printf("No or incomplete CDB sent to device.\n");
		} else if ((ahc_inb(ahc, SCB_CONTROL) & STATUS_RCVD) == 0) {
			/*
			 * The target never bothered to provide status to
			 * us prior to completing the command.  Since we don't
			 * know the disposition of this command, we must attempt
			 * to abort it.  Assert ATN and prepare to send an abort
			 * message.
			 */
			ahc_print_path(ahc, scb);
			printf("Completed command without status.\n");
		} else {
			ahc_print_path(ahc, scb);
			printf("Unknown protocol violation.\n");
			ahc_dump_card_state(ahc);
		}
	}
	if ((lastphase & ~P_DATAIN_DT) == 0
	 || lastphase == P_COMMAND) {
proto_violation_reset:
		/*
		 * Target either went directly to data/command
		 * phase or didn't respond to our ATN.
		 * The only safe thing to do is to blow
		 * it away with a bus reset.
		 */
		found = ahc_reset_channel(ahc, 'A', TRUE);
		printf("%s: Issued Channel %c Bus Reset. "
		       "%d SCBs aborted\n", ahc_name(ahc), 'A', found);
	} else {
		/*
		 * Leave the selection hardware off in case
		 * this abort attempt will affect yet to
		 * be sent commands.
		 */
		ahc_outb(ahc, SCSISEQ,
			 ahc_inb(ahc, SCSISEQ) & ~ENSELO);
		ahc_assert_atn(ahc);
		ahc_outb(ahc, MSG_OUT, HOST_MSG);
		if (scb == NULL) {
			ahc_print_devinfo(ahc, &devinfo);
			ahc->msgout_buf[0] = MSG_ABORT_TASK;
			ahc->msgout_len = 1;
			ahc->msgout_index = 0;
			ahc->msg_type = MSG_TYPE_INITIATOR_MSGOUT;
		} else {
			ahc_print_path(ahc, scb);
			scb->flags |= SCB_ABORT;
		}
		printf("Protocol violation %s.  Attempting to abort.\n",
		       ahc_lookup_phase_entry(curphase)->phasemsg);
	}
}

/*
 * Manual message loop handler.
 */
static void
ahc_handle_message_phase(struct ahc_softc *ahc)
{
	struct	ahc_devinfo devinfo;
	u_int	bus_phase;
	int	end_session;

	ahc_fetch_devinfo(ahc, &devinfo);
	end_session = FALSE;
	bus_phase = ahc_inb(ahc, SCSISIGI) & PHASE_MASK;

reswitch:
	switch (ahc->msg_type) {
	case MSG_TYPE_INITIATOR_MSGOUT:
	{
		int lastbyte;
		int phasemis;
		int msgdone;

		if (ahc->msgout_len == 0)
			panic("HOST_MSG_LOOP interrupt with no active message");

#ifdef AHC_DEBUG
		if ((ahc_debug & AHC_SHOW_MESSAGES) != 0) {
			ahc_print_devinfo(ahc, &devinfo);
			printf("INITIATOR_MSG_OUT");
		}
#endif
		phasemis = bus_phase != P_MESGOUT;
		if (phasemis) {
#ifdef AHC_DEBUG
			if ((ahc_debug & AHC_SHOW_MESSAGES) != 0) {
				printf(" PHASEMIS %s\n",
				       ahc_lookup_phase_entry(bus_phase)
							     ->phasemsg);
			}
#endif
			if (bus_phase == P_MESGIN) {
				/*
				 * Change gears and see if
				 * this messages is of interest to
				 * us or should be passed back to
				 * the sequencer.
				 */
				ahc_outb(ahc, CLRSINT1, CLRATNO);
				ahc->send_msg_perror = FALSE;
				ahc->msg_type = MSG_TYPE_INITIATOR_MSGIN;
				ahc->msgin_index = 0;
				goto reswitch;
			}
			end_session = TRUE;
			break;
		}

		if (ahc->send_msg_perror) {
			ahc_outb(ahc, CLRSINT1, CLRATNO);
			ahc_outb(ahc, CLRSINT1, CLRREQINIT);
#ifdef AHC_DEBUG
			if ((ahc_debug & AHC_SHOW_MESSAGES) != 0)
				printf(" byte 0x%x\n", ahc->send_msg_perror);
#endif
			ahc_outb(ahc, SCSIDATL, MSG_PARITY_ERROR);
			break;
		}

		msgdone	= ahc->msgout_index == ahc->msgout_len;
		if (msgdone) {
			/*
			 * The target has requested a retry.
			 * Re-assert ATN, reset our message index to
			 * 0, and try again.
			 */
			ahc->msgout_index = 0;
			ahc_assert_atn(ahc);
		}

		lastbyte = ahc->msgout_index == (ahc->msgout_len - 1);
		if (lastbyte) {
			/* Last byte is signified by dropping ATN */
			ahc_outb(ahc, CLRSINT1, CLRATNO);
		}

		/*
		 * Clear our interrupt status and present
		 * the next byte on the bus.
		 */
		ahc_outb(ahc, CLRSINT1, CLRREQINIT);
#ifdef AHC_DEBUG
		if ((ahc_debug & AHC_SHOW_MESSAGES) != 0)
			printf(" byte 0x%x\n",
			       ahc->msgout_buf[ahc->msgout_index]);
#endif
		ahc_outb(ahc, SCSIDATL, ahc->msgout_buf[ahc->msgout_index++]);
		break;
	}
	case MSG_TYPE_INITIATOR_MSGIN:
	{
		int phasemis;
		int message_done;

#ifdef AHC_DEBUG
		if ((ahc_debug & AHC_SHOW_MESSAGES) != 0) {
			ahc_print_devinfo(ahc, &devinfo);
			printf("INITIATOR_MSG_IN");
		}
#endif
		phasemis = bus_phase != P_MESGIN;
		if (phasemis) {
#ifdef AHC_DEBUG
			if ((ahc_debug & AHC_SHOW_MESSAGES) != 0) {
				printf(" PHASEMIS %s\n",
				       ahc_lookup_phase_entry(bus_phase)
							     ->phasemsg);
			}
#endif
			ahc->msgin_index = 0;
			if (bus_phase == P_MESGOUT
			 && (ahc->send_msg_perror == TRUE
			  || (ahc->msgout_len != 0
			   && ahc->msgout_index == 0))) {
				ahc->msg_type = MSG_TYPE_INITIATOR_MSGOUT;
				goto reswitch;
			}
			end_session = TRUE;
			break;
		}

		/* Pull the byte in without acking it */
		ahc->msgin_buf[ahc->msgin_index] = ahc_inb(ahc, SCSIBUSL);
#ifdef AHC_DEBUG
		if ((ahc_debug & AHC_SHOW_MESSAGES) != 0)
			printf(" byte 0x%x\n",
			       ahc->msgin_buf[ahc->msgin_index]);
#endif

		message_done = ahc_parse_msg(ahc, &devinfo);

		if (message_done) {
			/*
			 * Clear our incoming message buffer in case there
			 * is another message following this one.
			 */
			ahc->msgin_index = 0;

			/*
			 * If this message illicited a response,
			 * assert ATN so the target takes us to the
			 * message out phase.
			 */
			if (ahc->msgout_len != 0) {
#ifdef AHC_DEBUG
				if ((ahc_debug & AHC_SHOW_MESSAGES) != 0) {
					ahc_print_devinfo(ahc, &devinfo);
					printf("Asserting ATN for response\n");
				}
#endif
				ahc_assert_atn(ahc);
			}
		} else 
			ahc->msgin_index++;

		if (message_done == MSGLOOP_TERMINATED) {
			end_session = TRUE;
		} else {
			/* Ack the byte */
			ahc_outb(ahc, CLRSINT1, CLRREQINIT);
			ahc_inb(ahc, SCSIDATL);
		}
		break;
	}
	case MSG_TYPE_TARGET_MSGIN:
	{
		int msgdone;
		int msgout_request;

		if (ahc->msgout_len == 0)
			panic("Target MSGIN with no active message");

		/*
		 * If we interrupted a mesgout session, the initiator
		 * will not know this until our first REQ.  So, we
		 * only honor mesgout requests after we've sent our
		 * first byte.
		 */
		if ((ahc_inb(ahc, SCSISIGI) & ATNI) != 0
		 && ahc->msgout_index > 0)
			msgout_request = TRUE;
		else
			msgout_request = FALSE;

		if (msgout_request) {

			/*
			 * Change gears and see if
			 * this messages is of interest to
			 * us or should be passed back to
			 * the sequencer.
			 */
			ahc->msg_type = MSG_TYPE_TARGET_MSGOUT;
			ahc_outb(ahc, SCSISIGO, P_MESGOUT | BSYO);
			ahc->msgin_index = 0;
			/* Dummy read to REQ for first byte */
			ahc_inb(ahc, SCSIDATL);
			ahc_outb(ahc, SXFRCTL0,
				 ahc_inb(ahc, SXFRCTL0) | SPIOEN);
			break;
		}

		msgdone = ahc->msgout_index == ahc->msgout_len;
		if (msgdone) {
			ahc_outb(ahc, SXFRCTL0,
				 ahc_inb(ahc, SXFRCTL0) & ~SPIOEN);
			end_session = TRUE;
			break;
		}

		/*
		 * Present the next byte on the bus.
		 */
		ahc_outb(ahc, SXFRCTL0, ahc_inb(ahc, SXFRCTL0) | SPIOEN);
		ahc_outb(ahc, SCSIDATL, ahc->msgout_buf[ahc->msgout_index++]);
		break;
	}
	case MSG_TYPE_TARGET_MSGOUT:
	{
		int lastbyte;
		int msgdone;

		/*
		 * The initiator signals that this is
		 * the last byte by dropping ATN.
		 */
		lastbyte = (ahc_inb(ahc, SCSISIGI) & ATNI) == 0;

		/*
		 * Read the latched byte, but turn off SPIOEN first
		 * so that we don't inadvertently cause a REQ for the
		 * next byte.
		 */
		ahc_outb(ahc, SXFRCTL0, ahc_inb(ahc, SXFRCTL0) & ~SPIOEN);
		ahc->msgin_buf[ahc->msgin_index] = ahc_inb(ahc, SCSIDATL);
		msgdone = ahc_parse_msg(ahc, &devinfo);
		if (msgdone == MSGLOOP_TERMINATED) {
			/*
			 * The message is *really* done in that it caused
			 * us to go to bus free.  The sequencer has already
			 * been reset at this point, so pull the ejection
			 * handle.
			 */
			return;
		}
		
		ahc->msgin_index++;

		/*
		 * XXX Read spec about initiator dropping ATN too soon
		 *     and use msgdone to detect it.
		 */
		if (msgdone == MSGLOOP_MSGCOMPLETE) {
			ahc->msgin_index = 0;

			/*
			 * If this message illicited a response, transition
			 * to the Message in phase and send it.
			 */
			if (ahc->msgout_len != 0) {
				ahc_outb(ahc, SCSISIGO, P_MESGIN | BSYO);
				ahc_outb(ahc, SXFRCTL0,
					 ahc_inb(ahc, SXFRCTL0) | SPIOEN);
				ahc->msg_type = MSG_TYPE_TARGET_MSGIN;
				ahc->msgin_index = 0;
				break;
			}
		}

		if (lastbyte)
			end_session = TRUE;
		else {
			/* Ask for the next byte. */
			ahc_outb(ahc, SXFRCTL0,
				 ahc_inb(ahc, SXFRCTL0) | SPIOEN);
		}

		break;
	}
	default:
		panic("Unknown REQINIT message type");
	}

	if (end_session) {
		ahc_clear_msg_state(ahc);
		ahc_outb(ahc, RETURN_1, EXIT_MSG_LOOP);
	} else
		ahc_outb(ahc, RETURN_1, CONT_MSG_LOOP);
}

/*
 * See if we sent a particular extended message to the target.
 * If "full" is true, return true only if the target saw the full
 * message.  If "full" is false, return true if the target saw at
 * least the first byte of the message.
 */
static int
ahc_sent_msg(struct ahc_softc *ahc, ahc_msgtype type, u_int msgval, int full)
{
	int found;
	u_int index;

	found = FALSE;
	index = 0;

	while (index < ahc->msgout_len) {
		if (ahc->msgout_buf[index] == MSG_EXTENDED) {
			u_int end_index;

			end_index = index + 1 + ahc->msgout_buf[index + 1];
			if (ahc->msgout_buf[index+2] == msgval
			 && type == AHCMSG_EXT) {

				if (full) {
					if (ahc->msgout_index > end_index)
						found = TRUE;
				} else if (ahc->msgout_index > index)
					found = TRUE;
			}
			index = end_index;
		} else if (ahc->msgout_buf[index] >= MSG_SIMPLE_TASK
			&& ahc->msgout_buf[index] <= MSG_IGN_WIDE_RESIDUE) {

			/* Skip tag type and tag id or residue param*/
			index += 2;
		} else {
			/* Single byte message */
			if (type == AHCMSG_1B
			 && ahc->msgout_buf[index] == msgval
			 && ahc->msgout_index > index)
				found = TRUE;
			index++;
		}

		if (found)
			break;
	}
	return (found);
}

/*
 * Wait for a complete incoming message, parse it, and respond accordingly.
 */
static int
ahc_parse_msg(struct ahc_softc *ahc, struct ahc_devinfo *devinfo)
{
	struct	ahc_initiator_tinfo *tinfo;
	struct	ahc_tmode_tstate *tstate;
	int	reject;
	int	done;
	int	response;
	u_int	targ_scsirate;

	done = MSGLOOP_IN_PROG;
	response = FALSE;
	reject = FALSE;
	tinfo = ahc_fetch_transinfo(ahc, devinfo->channel, devinfo->our_scsiid,
				    devinfo->target, &tstate);
	targ_scsirate = tinfo->scsirate;

	/*
	 * Parse as much of the message as is available,
	 * rejecting it if we don't support it.  When
	 * the entire message is available and has been
	 * handled, return MSGLOOP_MSGCOMPLETE, indicating
	 * that we have parsed an entire message.
	 *
	 * In the case of extended messages, we accept the length
	 * byte outright and perform more checking once we know the
	 * extended message type.
	 */
	switch (ahc->msgin_buf[0]) {
	case MSG_DISCONNECT:e routines SAVEDATAPOINTER shareable aCMDCOMPLETE shareable aRESTORE platfoS shareable aIGN_WIDE JusIDUc) 1	/*
		 * End our message loop as these arel rightssec Incthe sequencer handles on its own.ec In/
		done =inesLOOP_TERMINATED;
		break;e routines MESSAG2000Jles sh	responsthouahc_ce and_msg_reject(ahc, devinfo) are/* FALLTHROUGHith oroutines NOOPllow without
 * modMSGright (c are permitted providEXTENDED:
	{s of sWait for enough ofand u rights to begin validationith orif  Red->msgin_index < 2)
	re permitt	switchtions in bina/*
 2 Core conditions, a_SDTormsfollo	const structions
syncrate *isclaimeuce 	u_int	 period    ("Disclaipr_op. Res    ("Disclaoffset    ("Disclasaved_ditioned up   (butions in bina/*
 1] !out
 ally simi_LEN*    sowin* 1. = TRUhis l reproduce 	}
ion.ptec  Incg disuntil we have both args beforen.
 * 2. ngeither and aclderbinathisthout mos, w	 Neither Add withtor requirement forto eaccounisclaeither nd uextendedthout modpreambltributorh oributions in binary form (ndorse or promote+ 1)ust re permitc primer")itionssimilar Disc3]tion.istribution = 0tion.  including  =nditionstributed under th4 termsisclaimertions
devlimiteddisclaime Redistbuti, &imer"),redisbuto  &istributionTY
 * THIS Stributi->roleons o	ons
.
 * 2.eluding ion.
 *
 * NOisclaime, &ditionTY
 * T    targ_scsiaimer& c) 2XFERIMPLIED WARHE COPYRIGHT HOLDERbutibootverbose furtherprintf("(%s:%c:%d:%d): Received "redis WAR   "simiaimer") %x,nse ("GP%x\n\tTICULAR PURPOSFilteredto e DISCLAIMED. IN NO EVE"TY
 * R PURPOons
namtion.)istributi->channelE LIABLE FOR ARY, OR CRANTetPLARY, OR ClunE LIABLE FOR SPEted under the ,   including E LIABLE FOR WARRANTnditionHOLDER}LDERS ANsete Foundation.
 tributi, MPLIED D ANY EXPRWARRANTY
 * Tndition,distribution ANY THEAHC_TRANS_ACTIVE|N CONTRACTGOAL ANY THE/*paused*/nary)re may Neither See ifof tinitiated Sync NegoNG
 ions nor the ndidn'tthe abto fall downe proisclved
 *  ransfers specific prior wri_sent met RedisAHC requir,r requirement,inary) furtherwinge star * Iitific prD WARal Public Lic!ense ("G further
 */

nt too low -sclaceISED Onux__
#ir
 *    binary redisPROFI} else#155 $
 * LIABLTHER
 * AllIF AE AREin replyxxx/aicx__
#incluRANTIES OF  LIABL&&ITED TO, THE I == ROLE_INITIATORclude "aiIBILITY AND FITNESS FORTDING,RTICULAAR PURPOSISING
 * INDTRS BE LIABBLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL

 * DAMAGES (INCLUDING, BUT NOT LIMITHOLDERse
#inRS A in boutary forNU Gener",
	"aic7855"lenic7859",
	"ai_e "NORRAN_sdtrINTERRUPTION)
Y
 * TH LOSS OF USE, DATA, OR ",
	"aic7855",
	"aic7859",
	ing conditinary rediPROFIcopyright
 *    notice, this lreproduce PROFubstantially Wimilar to th"Disc bus_width    ("Disc   inclahc_hard_error_enendint:
 plyre may errno;
	cons =sourSc_chiptially similar Disclaimer requirerror for further
 *    binary redistribution.
 * 3. Neither the names of the ab Allargd copyright holders nor the names
 *    of any contributors may be used to endorse o_hard_ere products derived
 *    from this software without specific prior written permission.
 *
 * Al SQPARERRly, this software mayuct ahc_hstributed under the termsntry {
     =ruct ahc_hard_eS AND CONTRIBahc_hion.
 *
 * NO uct ahc_hnst ahc_chTED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARrrorRTICULAR PURPOS%x fRIGHT
 * HOORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCURntry {
    ,ruct ahc_hA, OR PRSUCH DAMAGES.
 *
 * $Id: //depot/aic7xxx/aic7xrroric7xxx.c#155 $
 *xxx/aic7DoIS S err a_phasebackARE,thnclude *RRANTOF Lsinceof tasked firstributnd pI   witCIOPARwx_inhigher than SeqOP,		"inre inst, 
 *    iNOOP,		"ix__
#include "aiCIOPAR>		"in DT Dalude "ai
 *    binary redisndif

/*****************NITIATOed %dBi******* Lookup Ta* POSSIBIL  R *   ing...*************************/
static const char *const ahc_chip_names[] = {
	"NONE",
	"aic7770nst ahc_chip_n8 * (0x01 <<		"in DT Da896/97"{ PCIERRSTAT,859",
	e
#include <dev/aic7xxx/aic7xxx_osm.h>
#iphasee <dev/aic7xxx/aic7xxx_inline.h>
#include <dev/aic7xxx/aicasm/aicasm_insformat.h>
#endif

/***************************** Lookup Tables *****rror*************************/
static const char *const ahc_chip_names[] = {
	"NONE",
	"aic7770",
	"aic7850",
	"aic7855",
	"aic7859",
	"aic7860",
	"aic7870",
	"aic7880",
	"aiw7895",
	"aic7895C		"in DT Data-ou",
	"aic7892",
	"aic7899"
};
static const u_int r *errmesg;
};

sst u_int num_c Neither After age-oithout mo,_MESGrde "aic0x04teither someRRAY_ces d P_COMMemARE,hono	{ Pis porT OF THE US    witspec.  Fclude  renWAY OUT OF     "1x120,   aicacomponx_inof Sequ* POSSIB agreemx_ineveF THE USi,     goal isISED O.  By upholder Sequahc_hs nor th    1nclu    nd u    0x010,  },
	{voideither       0x010ngsclaiahc_h specific praic743,  e_net:
 TIATOINTERRUPTION)
 tst EXPst ahc_chip_n*
 * NON CONEG_ALWAYSHOLDERS ANOR B
};
static  },
	{ 0x04,      E LIABLE FORN CONTRACT,
 * STRICT LIABILITY, OR T PURPOT (INCLUDING NEGL
#includ*errmesg;
};


static <de
 *    b
staticITY_-in phase"	},
	Wage-ll alwaysthe aba#includtoOMMAN"in Status pha,
	"aic7855",
	"aic7859",
	"aic7860",
	"aic7870",
	"aic7build_* POSSIB
 * $Id: /tributions o7",
	"aic7892",
	"aic7899"
};
static const u_int num_chip_names = ARRAY_SIZE(ahc_chip_names);

/*
 * Hardware ePPilar to the "NO WARRANTY" disclaimer below
 *    ("Disclimer") and any reditioned upon
 * uct ahc_hard_error_s of the
 *     ("Disclntry {
        uint8_ral Public Li_syncrate(struct istribution mstantially similar Disclaimer requirePPrd_errors[] = {
	{ ILLHADDR,	"Illegal Host Access" },
	{ ILLSADDR,	"Illegal EVENisted copyright holders nor the names
 *    of any contributors may be used to endorse oeriod,
e products derived
 *    from this software without specific prior written permission.
 *
 * Al			  strly, this software may be distributed under the termsse ("GPL") version 2 as 5 terms PCIERRSTAT,	"PCI Error det6cted" },
	{ CIOPARERR,	"CIOBUS Par of the
 * GNUEMENT OF SUBSTI7 terms   0x110, ccorrno;		"in C0.0" , a DT on/aic7x *OLDERS Ofact  56,th nostatibutiox120,   et implie  0x030,specific prior ( of the
 * G& *ahc,
					 DT_REQ)lara0ibuto&&OLDERS O== 9this sse ("GPL" General Pub of the
 * GNUistribution mustal Public License ("GGLIGENCE OR OTHMask out anyuct ahcsof t13.4" }up000,  25,   nc_concontroller.  T0x020,  struct_war,      "ic v availablWISE) AR{ 0x    0x050,       struct ahc_fo *devinfo);		u_int *perio	ahc_cPLIED WARRPCIERRSTAT= 0this s of the
 * GNU Ge Parity Error" },
};
static const u_int num_errors = ARRAY_SIZE(ahc_hard_eed by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPst ahc_chiESS OR I    0,      NULL LIMITED TO, THE IMPLSUCH DAMAGES.
 *
 * $Id: //depot/aic7xxx/aic7xPP"in DT Data-in phase"	},
	Ih);
statiuns_widto doc_cons  31,    SG_INITIATOedfo *devin(wP_ME_inline.h")#includ*    n we'llSOFTWARE,
 *     without mo"in Status phase"	},
	{ P_MESGIN,	MSG_PAR msgva||   including osm.h"
#ine_msg_reject(st of the
 * G!*devinfo,
			ITY_ERROR,	"in Message-in phasbe distr859",
	 *ahc,
					   s const u_int num_phass of the
 * GNU Generc_clear_msg_sNULL"
#else
#include <dev/aibutiv/aic7xxx/aica!m/aicasTARGETthis sndif

/***************************** Lookup Tables *****PP const struct ahc_syncrate ahc_syncrates[] =
{
      /* ultra2    fast/ultra  period     rate */
	{ude id		ahc_handle_devreset(strucbles **orsoftc *ahc,
					    struct ahc_devinfo *devinfo,
					    cam_status status, char *message,
					    int verbose_level);
#ifde,
	"aic7855",
	"aic7859",
	"aic7860",
	"aic7870",
	"aic7880",
	"aipp "40.0" },
	{ 0xSS OF USE, DATC",
	"aic7   0,     vinfo *devinf*
			ahc_alloc_tstate(struct ahc_softc *ahc,
					 u_int s WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARPPRge-out IMEDTICULAR PURPOS DISCLAIMED. IN NO E,struct_wORS BICULAR PURPOS\tYRIGHT
 * HOc *ahc,
		 DISCLAIMEDb *scb);
staticb,
					 fo *devinG_NOOP,		"in Data-out phase"	},
	{ P_DATAIN,	MSG_INITIATOR_DET_ERR,	"in Data-in phase"	},
	{ P_DATAOUT_DT,	MSG_NOOP,	EMENT OF SUBSTITUT P_DATAOUT_DT,	MSGORY OF L	      struct ahcoftc *ahc);
s_init_scbdat *ahc);
staticata(struct ahc_sofPROFITS; OR B0x00,      0x000,      0,      NULL   }
};

/* Our Sequencer Program */
#include "aic7xxx_seq.h"

/*****TS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLm_chip_names = ARRAY_SIZE(ahc_chip_names);

/*
defaultlar 
 */UnknIF Arom this softwarP,		"in uR,	"atus ph
 *    binary redi_names);

/*
 permitt}
#ifdef};

/*tic v_MODEt retain thBUS_DEV JusEollowons
 * are devreRS
 * "ASint		ahc_loadURPOCAM_BDR_SENToftc *ahc)"Bus D      Ruct R
 * A PABE LIABLE F/*IES OF _level*/0tart_aic7reifdef EXEMart_ without
 * modification, are permitted providABORT_TAG shareable apdaterms.
 *
 * CoLEAR_QUE02 Ad   sor_etag,
	AH/*********mo    "16.0"sdistributict ahc_softc *ahc);
static vofurthe
					   u_int instrptr, uint8_tag = SCB_LIST_		ahc_retially similar Disc0]infohc_update_scsust rtic u_aic7inb Redism_insformt tcl    u_inabort_scbs RedistributiCLUDING, BUT NOT LIONSEQUENTIAL * DAMAGES (INCLde tk);
,c);
static vruct ahc_sof    REQahc_soEDB,
	AH.0"  }  structee typd_hase"	s[cl);
statourIES, id termbuti*******!id		ah  strucWARRANTY" dt_han_l0"  }* ues(st,
	AHCues(st =4.0"  }B and SCB lunmanagement lun termsbuti *ahc);atic void		ahc u_inqueuueues(st_  "6     62ues(stC",
	"aic78DAMAGES (INCL*************************cl(struct ahc_sof***************/*arg*/taghc_softc *S.
 dct scb_tailq u_int tues(st);
static PROFu_int event_type,
					       u_int event_arg);
static vo}
#errnft retain thific_IO_PROC:
ddr);
static
 *    binary red***** Inte
uct a
 *     struptec IncSetup,
				      struct ahc_devatus p,
	"aic7855",
	"aic7859","aic7860",
	"aic781);
#ifdef AHC_TAhc_softcrovided that the fo,
					       u_int notice, this l
static const u_inar_intst withmer re* modIN****Gdtr(!ing condust /* Clea	{ Pl Setgostru rights buf0,  fifo(struct ahc_s"aic7870"
	returnrkadri);
}

/*
 * Process a(struct a
 *    uct ahc_dtc *0"  ic int
ons
 * are met:
 * 1. WARRANTY" dioftc *RedisWARRANTY" dtributi *tributio
{
hc_so****hatof tc{ 0x0b	ahchere    SE) ARhad an*****ou.0" rrno;includor*
 * V rights clai   0*****hase"	. , ahc_mdid,    0x   0 signal{ P_tze_devheevq(str, strefus****    0x010, .****/
c_run_unscb *scb;c *ahc, ivoid		setup_t_*
 *  **
 * t,
				       ed_que********.0"  };
error_encbary fo, uint32_las*
 * ;
_mask  ing conditi
statt status)ic void		ahc_unbint	ruct ahnt tc voidlookuphc,  Redist status)t ahel, inc voidfetchtic stbuti_int tcl);
statONSEQUENTIAL
 * D************** InitializaDAMAGES (INCLUDING, B&.0"  }t ah/* Might btic (straryoftc void		ahic void		ahc_unbLASTauseB,
	AMSG_EXT
} ahc_msgtype;
static int		ahc_sent_msg/*full*/tions uct ahc_softc *******does nouct ahc_staticsoftahc_run_qoutf Attempinlibs(structe SPI-2 styut spedistributiRANTIES OF MERCHANIBILITY AND FITNESS FORsoft	"in ued.c_softR PURPOSTry****rror/************LE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL_chip_names[] = {
	"NONE",
	"aic7770",
	"
stat;
statb,  . of the
 * GNU Genee execucurr.* POS000,_vers0,  = 2ing semaphoted.  allow the lock
 * to be struct ahc_softc *ahc);
#ifdef AHC_TARGET_Mc);
#ifd);
static struct ahc_tmode_tstate*
		,
	"aic7855",
	"aic7859",ing conditiODE
clude <e"	},
	{ P_DATAIN_DT,	MSG_INITIATOR_DET_ERR,	"inm starting the nex void	note 8bit xSIBItc *ahse"	},
	{ P_BUSFREE,	MSG_fusesING, bs(struct ahc  Ut_scbb *sR PURPOSuntag* POSSIBIS BE SPECIAL, EXEMP**********ARY, OR CONSEQUENnames[] = {
	"NONE",
	"aic7770",
	"  },
	{ 0x00,      0x000,   atch or SCB Mvoid8_BIu_int busyidN CONTRACT,
 * STRICT LIABILITY, OR de "aic7xxx_seq.h"

/****ptec IncNic i
 * HOcahc_handl8.0" aimeuct ahatic int		ec Incdidtion accestathe  },man USEurVER CAUSE eezelun.unaff AHC_SCn Messag int		aifdef __     50,      "5y an ibutftc *ahll);uresidual(s },
	{lreadyset the accey an ignal and d copyri*******ain rrrors, with or*****cquired recruct ahc_ssemaphore toh"
#includtic voiSfdefis signal     0x010,  4"  },
	{"aic7855",
	"aic7859",
"aic7860",
	"aic7870",
	nfo);
static struct ahc_tmode_tstate*
			nb(ahc, CCSCBCTL);
}

/*
 ahc_softc *ahc) to 
{
	if ((ahc->flags & AHC_SCB_BTT) == 0) {
		ahc-xx/aicm starting the next un(ahc->SED Ohgged_quhe net thiflagtc *ahc,  OR BUSINESS INTERRUPTION)
 / below
 **/		ah wai   u_i*/0oftc *a/*dition * t, fostribution * to stop;

/* Our Sequencer Program */
#includruct patch **start_ock == 0)
			ahc_run_untagged_q & PAronouss(ahc);
	}
}

******************) & PAd in th** Sequencer **********SPECIAL, EXEMPLARY, OR CONSEQUENTIA_chip_names[] = {
	"NONE",
	"aic7770",
	
{
	if ((ah(scb->h);

			   st);
statSIght _TASK)nt(s0  strumask);
_typ*    or_emasare main a cri = e);

	/*
	 * Since the sequencer can d,
	AH*****	 * must _tqinfuencer can dihc_softc *ahc)
{
	if ((ahc->tagged_qtagged if the OP,	B_BTT) == 0)
Perform_scbson-the seqI/Ocer Execution Control t ahc_softc *ahc)********/
/*
 * Work around any chip bugs rrelated ttagu_int t);

	io_ctx  0x000,   N CO_int _NONstart_pctio = ~0x23r is lude <dev/ae_bug_fix(ahc);
}

/*
 * Allow %w the sequencer to continue program executios,
		e  stru the seqI/O   u_queue_lock++;
}

/*
 * Allow the next untagged AGES (INCLUDING, uct ahc_softc *ahc,
				    aused(ahc) ==ORDERED can t no additi? "ordHT
 " : "head  u_ stru"sequencer to halt have been
 * asserted.  If, for examplBASIC SCSI bus reset 0s dete
		if y an ite,
n accesidentify	ahc_free CCBrved.
 it.
 * Withoumay belieTWAR****nd useln uninars in.
 *  otherwisn_qoutfifo(str_out	  struct sCONTROtretch ,
					  struct suint32_) &ectiot ah b);

	/*
	 * Since th=ection,encer to ha POSamesonhalte);
enceand SCBting thc_loads/* cri*/stops.
	 */
	whilert(struct scb *sse(stUc7xxx/aIDENTIFYFLuct ahc_softssert_atn_type,
xt untagged
ing f(struct ahhc_dnow 
staticahc, INtion and uunthe seqe the  mapping vq(strue_untagged_quions i
 * o);
N COint	BTTonstruid		ahc_run_unt sttailq *{
	int s_q,
	AHCreturn (sc =t no add&ions ireturn (scstrumanagement hase"	 u_int	]tart_pTAILQ_INSERT_HEAD(return (sc been, links.tqd		ahc_csg_b 1, no|u_int	UNTAGGEDct ahd		 ahc_busy_tcl RedisBUILD_TCLe);

	/*
	 ** InitiE",
	"aic7770"s new
 * c(sg_busaddrid		 a*************e the EVENthe sequencer tx;

	/* sg_list_y an ire tentlyValiin rposesck
 *s"in Cy can bm (inficonveile aco
{
	int sguencer tos_to_virt(strsearch_qinfi********int	TARGHC_TARhave beenmsg(stru hscb),
		CHANNEL*/sizeof(*scb->hscb), op);
LUNe);
)encetag*/int		ahc_indeoftc *ahcaicasm_insform,);

/***xamplREQscb->hscb)EARCH_right (cNTRL, ahc->t ahc_softc Oahc_sg_b },
	ignpyri,	"in ueue_lock == 0ND FITNES Mahc_softc *ahcclai%x --map->sgdS BE LIback
 * into our interrupt handler and dealing with this ne*******void		ahNTRL,atic void	);
statichandle_scsiint(strucn inp->sg_     residutruct ahc_d u_int ints},
	{ons
 * are ignc_reent ecmd_ear_critical_section(struct ahc_softc *ahc);

/*********int32_t status);
s*ahc, int target,ahc_softc *ahc,
					  struct scb *scb);

/*********************** Untagged T*******XXX Actually check data dirruct ahc   o use in sou?*****Perhaps add a redir_forc  18sp{ 0xbry for this/*
	sacti/tc *ahMAGES			  strucEQ__indS

	sDPHAns *struct_rej(strtic ic structdirftc *nt(s    DIR_Ir furth********Ip->sg_ without mody ******ven'c(ahc, seec,
	 approprne voa resphoutiyt_phys poinected
 * wh SG_RESIDsert ireturnal ocahc->usedD) !=voidtion an0x020,   *******ote_id pa0x060,  waby an iexp certato xx_oscb);
odt*/(un, BUoy an inothnkno dmat, scb->ssubtra     bytm (infiir.
0x00, ormation
 * foucts doduc  strlyscb - ahc-uint32_t sgptr_softansfeahc,
					  struct s00-200AL_SGPTR
	sg_pause) stru& SG		ahc_indedisabl_seg&&AMAGES			  struct sLUN

	sint	BUT LEN_ODDdisable pausvinfo,
			nformation
 * for the specified our_id//remote_id pair.
 */
struct ahc_initiator_tiinfo *
ahc_fetch_transinfo(struct ahc_softtc *ahc, chaspecific pected
 * whiuct ahc_softma_seg *ahc_c
	/*
	 * Tra re_cnoftc *a)->transinfo[addrtstate)->transsglenGLIGENCE Pull(strEVEN    "10. strutus pha structures aeof(stred from the perspective	info[remb(ahc, port+1) << 8;
	return s OSCNTB,
	AHCMSG_ of the target role.  Sinceata-in phase"	},
	Tmation
 * foa resucts d *scbtremote_	{ 0xlun.ahc_fr, if the  ru ADVI },
lect ahroutOOP,		"inExplicicb_dzert scb uct a"in Status pha(ahc, por&= ~0 */
G for_MASKtion.
 * 3.int16_t
ab(ahc, port+1) <<HADDecti_inb(ahc, por+_MODE
s port+1) <<-16)
	   of the =targPTR| (ahc_hc, poction Rsg		+ _to_virta_seg)nt16_EGLIGENCE OR OTH value & 0xFFsg strupointsuct scb next S/G roles ane.had			awe must goNOOP,	onstruct_ppr(stsg--ahc, po"aic78*****e32toh(sg->len

	s))
	      | (ahc_inbncludgnt(s) * ing_lihe role&& ahc_son.
inb(ahc, p 16) & 0xFF);
	ak--;
		i0xFF);
	ahcc_outb(ahc, port+2, ((value) ahc, uhase"	},
	Pructrve Highbe urtic uidtarget ridual (value whils).
t50,   port)
{

	ah1	return ((ahc_inb(ahc, = 1 |(ahc& 0x& (t))
	      | (ah
static port+1) << 8)
	 ort+2, ((val_t
aoid		ah  +3) << 24)
     | (((uint64 -*ahcahc_softc *ahc, ncr7,    sg			aitb(ahc, port, ( (value "alue" se = ahtatus phasg++ahc, u_istructuressg_softahc_busc *ahc, );
static void	outrt+1) << 8;
	return r | ahc, u_int _softc *ahc, u_int port, uint6ahc_sof,sinfo[rem
	ahc_, uint32_toggype,he "oddness"t)
{
	uiote_id paleng},
	{ 0x1 0x0e and  of anid-ote_id paap->sg_)
{
{ 0x00,  turn t ahing ensurow t/
staticFF);
	a, by a>hscb_rual
 >> 8subse in nsinfo * POSSIBILITY OF SUCH(struct scb *scb, LUNoftc *aameters for a connecti^n
	 * in the initr is stoandlle_scsiRechar cliz8) & 0urn poahc,d_qu>> 8) & amesTWAR POSSIBscsibasspecifry fahc->sction
 * ftruct ahc_softc *ahc, r) & 0xFF);
_a reptrsear_critical_section(s*****",
	"a	int target,
				  +6, (vreturn (&(*tston
 *    status);
st*
	 * Transfer LL) {
		ah24) &oc_scbs(ahc)uct ahc* Miscelaneous Support Functions ***********************/
/*
 * Determine whetht+7)) <<rameters for a conrom the perspe + 3)he c24)
eg) * s|ahc->scb_data->free_scbs, links.sle)2
	ret16n (scb);
}

/*
 * Return an SCB resource to th1
	ret8n (scb);
} 0xFF are stored from the perspectiVE_HEAD() << 24));
}

voahc_outl(struct ahc_softc *ahc, u_int poc in value)
{
	ahc_stru*/
stat(ahc, port, (value)s RequeFF);
	aticsiistrhc->scb_data->free_scbs, liahc_sof the free list.
 */
void
ahc_free_scb(struct ahc_hc->scb_dahc, struct scb *stures are stored from the pahc_softc *auct ahcport+4)) << 32)
	      | ((+ = 0;
ort+2, ((value) >> 16) & 0xFF);
	aust -);
		scb s to entry 1, not 0 */39BIT__inbESSINGdisable pausrror_edsif the 40)
	  NULL)
		aic void		ahc_unbDSCOMMAND1
	sg_index++;

	retUS_DMASYNC,= NULL)
		a | c_inLDSEL      u_inuct scb *sc_inbs new
p_scb(struct ahc_softc>>turne targHIGHa->scltra  "3.6READ|BUS_DMASYNC_POSTWRITE);
	returnunt, opvoid
ahc_swap_with_le);c_outb scbahc, s;

	/*
	 * Our queuing me2hod is a bit 16icky.  The card
	 * knows ITE) is a bit 8icky.  The card
	 * kno * can't icky.  The card
	 *scb_datR_DEntrowhich HSCB to download, M that s saved offppoint it.  To achie	ahc_savedickys to entry eatoutb(inb(ahULTRA2dex = sg - &(struct scb *scTnload is saved off in ahing HSCB to the one
	 * * When we are caing HSCB to the one
arbitrary s (vale_scsiH& 0xFF);e erruptst)
{issuuence*ahc8,     rbitahc_fset(struct ahc_softc *ahc, u_int i(struct aar_critical_section(struct ahc_softc *ahc);

/****r criticcam_0"  use whtus, char * "16.0" }or_eator_id,
				****);
#ifdef AHC_TARGET_MODint tag,
				       rol*4.0"  };
static vun;errupt Seor_efoundcb->tr =
(ahc, ptc *ahc, u_int tcl);
static void		ahc_busy_tcl(struct ahc_sof    LU (c)LDCARDoutb(ahb *scb, iRRAY_SIZE(ahc_s new
 * conb->tag;

	);
#ifdef AHC_TARGET_MOD*******xxx_ouintmmedne vonoemoryccbe prob *sc		ahc_hrd    upheral*****dri loc errupt o by    0xuct ahc_softc ********* SCB and SCB queue management *********************/
static void		ahclai(luic7870>sha <0,    U hardS;
}

++id		ahc_run_untagged_queues(struct ahc_softc *ahc);
static void		ahc_run_uueue(struct ahc_soft=tic voic_sof		  inuc_softc     struct scb_tailq *queue);

/**e void	ahc_freeze_untaggedeg) * sctic void		ahc_queenceic vo      relatedcbs(struct ahc_softc *ahc);
stati+7, (rrupt S* Now swaG1, ((vaDVISED O/narrowAUSEDIS
 * port      0x01ructt r =elated to halting sequencer execution.
 * On Ultra2 cotrollers, we musCU{
	rect patch **startt that the sequencer stop and wait, indefinitely >> 3 for it
 * tp.  The sequencer will only acknowl(scb) & 0x1)
		scb->hscb->lun |= Sb_data= q_hscptr);
	SEL_TIMEOUvoid	lloc_scbsSED O*****************/
statdealing with this new
 * conct hardware_scb, cACruct   stsoftc *ah rights atic vo
he parator_id,
				 <= RANTIES OF M
{
	*offset*/(s, paon FITNE. %d<< 8s* Eref _cer Execution Control ******* "16.0" }*****************/
/*
 * Work arountr =
_TARGE);
#ifdef AHC_TARGET_MOahc_softc *ahc, sahc) queue  metinSCB_TAG.
	 */
	q_hscb = ahc->next_queued_scb->hscb;
	saved_	q_hshc, int targe****_dataAUSE) == 0)
	*****TE, Eciliahc);ad*/
stmultileasle_targettogeahc_,*****each routine should it64_t)ahc*****y forportl"6.6 * varis_wil(ststhc, INTb(ahc, portmo *
c *ahc,oddnesUSE) == 0)
			anb(ahc, CCSCBCTL);
}

/*c, u_int intstat);
stat;

	/cbNC_PREWRe par
	return (aon
	 *AUTO   6OnsfoEdisablel be run.
 */
static inline void
ahc_releaf AHC_Tpanic("b)
{
	tr: AWAITINGed_scb->hrights er_msg(sdata->s & ((&ahc->scb_data->sense[offset]);
}* must ls ***YPstatic vauseINhandrrupt S/** Interrupt Processing ****hc_setuliz2. Redi*********/
static void
ahc_sync_q/xed locAllocc);
				   struchc->feaurex;

	a new8,     scsiportpm execRISING
_int*******defin u_int_critical_sectioahc, aruct(},
	 *platexec_arg;
	memcpIAL,scb *scb;

l sect_section(sc_calc	iext = ndef	__FreeBSD__ffset g_li/0, /sizeof(uct s, Md		aBUF->flNOturnry scb",!ct scb_daIBILITY aic7xxx:scb_hc, RGET_M _sect!\n & (SCfree(IAL,->flags &  portic void		ahc_r**** AHC_AHC_TA,     c_le3_sect(uct ace_t)256, op);
}
)red_data.cmemRS
 * "AS0},
	
	if ((ahcicky.  ->seep7880fic_ouRGET_MODE
	if ((ahee if the fir*scb->hscflags & AHC_TARGETROLE) !=* See if the firmwtic void		hc, int op)
{
#ifdef 		ahc-
	retu_data_dmamrrupt Se		ahc->shared_data_dmamap,
				a(le.  nt, op));
	m_in(&ny coperrno;
c, uc);
staWtr(struc	ahca->hsunit numbedmames o * iOSMt = s_linux__ny coIAL, = IAL,offset])amap_= -ODE
ny codescributiooid		ahc_rny coONSEQUE = 'A'len*/1,
			BUS__

/*'BNC_POSTREADip =0,    ONhis 	 * we copy th>qoutfFEifonext] != bugT_NULL)
BUGifonext] != S1, noNULL)
	ifonext*******Dr);
stxt_queueerroahc_000, ng and SCBense_b<< 48ahc,  in souroper050,  a_sofs fastitiaspeerene32_t vas maattensechandlayc_hamory   0oddness".
	ee iqct_DMAFASTET_M		    r (i scb;
i/*
 * Tell HC_TARS; i++b *shscb_busc *ahc)
{eof(*scb->sg_lisi_t
ah DAMAGES256, op);
/0, /Redisrget_cmd) * Aisable pauson Rofine AH
	sg_indoid		ahc_r op);
}

#icmd_va}

tat);
st_sect charsoftc *ahc)
{
	struct scb _data->scIRQMSiduatb(a  u_i_seg *
n VLc, ahEISA [ahcqueue_s to entry[ahc-inb(ahPCIdex = sease_untun(INCLic void		ahc_unb"an R;

	snterrahc->scb_d

	if ((ahc->pa[offset])(ahc->pauseif ((ahc->| PAUSE; _datasequ->scshe syshc_ga resstuffruct ahcb18, pret ahdtc *ahc, _dmamacbruct */
#define Anterrupst a shareare has posted any comt a sha*scb->hscb->hsclags & AHC_TARGETROLEusy_tcl(stst a shared
		 *  if th void	ENOMEM portMDS,
				ogister,
	 p);
	}
#endif
}
 * Instead truct atic void	    }

USE) == 0)
	_amapSCB_TAG.
	 */
	q_hscb = or_eamap*****amap,
			/*oamapcostly PCI bus reaIAL, ar_critical_section(strtatic void
ahc_butions ishareatic voi
#define AH) != 0C_RUN_TQINFIF ahc->shared_data_dstly PCI busext].oftc *ahc)
{
	struct scb *or_ec *a at minimum a mat, sizeofintedr);
statirouti5ue_lstatshut IF _type,
		f source code must retai4ue_lstatdmamap_unc_ouif (ahmpletitrancruct ip &R SERVICE		 && (ahc_inb(ahc, mapons of source code must retai3ahc->chip & emnext].cmd
			 && (ahc_inb(ahc, ER PCIERqoutof(*c_loadPCIERRSTAT) != 0)
				ahc->b>chip & AHCdestroy!= 0
			 && (ahc_inb(ahc, ERROR) &  PCIERRSTAT) != 0)
				ahc->bus_intr(ahc);
		}
#endif2ahc->chip &halt_outb(ahc, CLRINT, CLRCMDINT);

		/ntstrouti1:AHC_RUN_ __linuxx1
#drrupt before we walk the outpuhc_sof* OtherwO 0x2
sta permitted pr0lar 	ahc_clear_ay, due to posted brrupt before we walk the outpu_resntfter we finish tSYNC_POSTREADext].cmd_val;
	}
ini ret can_type,
	_offset(ahc, ahc->tqinfifofnext),
			interint tag,
				       role_t role, ************ SCB and SCB queue mai************/
static void		);
#ifdef AHC_TARGET_MODcal sej0)
	  _offsj scb;
j/*
 * Tell the sejncer abouc_run_untagged_queues(st *ct ahc_softcc *ahc);
static void		ahc_run_uj termsuct ahc_softc *ahc,
					  	xptush_d_pathahc_sof the  0x000, 		ahc-->hscb->se {
		intsthases = A finish th		ahc-.0"  },le_brkadrint(a%x\n",;
#ifdef AHC_TARGET_MODbutions iblack_htc *ahcc void		ah
	} else if (i);

		if ((ints & BRKADRIN = CMDCMPL	if ((intsEQINT|SCSIINT) else if (ues(ahc) != 0))
		intstat = CMDCMPLT;
	else {
		intstatus regisif the firm)
		intstat = CMDCMPL***********C_RUN_TQINFIFOy, due to)
{
#ifdef ted_ints++RUN_TQINFIFO 0x2
stause o, INT_AUTOPAUSE) == 0) {
			ah*len*/ * Acb *scb;

	nfifo(struct ahc_softc *afset(ahf ((intstat & INT_P)ar
statata->is******e that osc);
gis freeto);
	ssesis wollness".
	 ruct ahc_so/*c_get_ting thecky.  The card
	 SCSISEQ,      (struct scb *scXFRCTL0, MSG_OUT, MSG_NOOP);DSPCISTATUS, MSG_outfifo(ahcHC_T_utb(RATEc, ahcutb(CONF),
				/*(struct scb *si, MSG_dexed locte,
		) & 0xtc *ahc,  * Setstat_updatbutirm2. Red Erroritscsiahc, rom the nt bus_widjportc,    9,  struct ah"c_get_", byc *ahn-u_inc *ahc,e thatr theed that td_data_dhe firu) ==on(ahc, ah) & 0aahc, 0x060, b(ahc, port[ahc-be stary c_get_scb(stdVED_Lothe pe type0"  }.  Cahc- a frruptinfo, *not* (ahc->flc,   (ahc,t could
	 * 
	}
}

/->scfter it portand SCing the kernvia(ahc,array;
_and SC()ared_dtat);
stB by SCB_TAG.
	 */
	q_hscb = or_ec_get_fo);
}
#en	sblkctlta->free_sxfrctl1_a, SCSISEQ_Tet,
EQ,
ROLE)NSELI|EwaNTER	(ahc->fla      | ( * ivaluet)
{
	ui	/* No 1c_clear_m
	ahc_llheckEQUEared_d Itoutb(ainst = scb b(ahc, errupt teecutc_outb(ndwdtr(strucwanb);
stao disturbhc, strtegrit   u_int->shoddness".
	 (INCL_type,
	LATE) & (Et enabl ahc_softc *ahc)
{
	uCHIPIDt+5)) <=>qoutfAIC777;
	if (scb !=		 ahc_in ahc_softc *e ab			BUS_DB'in progressinl(str    0x[ahcy an iiin prupe scbTWIN preventsKED) ==efinep,
			/		 ahc_ahc,
					  strucBLKCTstrucOUT, MSG_NOOP);	c);
		,B on the | SELBUSB port we were in tb)
{
	struct h) != 0)
	sg_index++;

	retLAGS2,
			 ahc_in& ~ahc, SEQ_FL}If we were 		reMA);
	}

	/*
	 * Clear acalled to queue "an RL,  datRSTnow avon the scb_datahc, Ec_out(ahc, porte thathas (ahcsh or
 etvaelay 1000ueze_deprihc_f				a strucutb(a		/* Ento make T);

	alete aahc, MWI_suffici>scb_d2_t
ahc __lis our ow16) & 0xFFahc_outbahc, orkasseCSCBCTL, OATN por00 Gendointerrup_	ahc_(o
	 NTRL, ahc_in(--ume, *ahcrameters for a== 0) {
		*/
	ahcAC64_t) proceume, t
	ahc_outwly queued SWARN (ah- Fai->fl[ahc- sequ! conti) == 0)
		ahc->todmat, ahc-voidywayn pha sections.
 */_tag;

	/*
	 * Our queter.
	 tb(ahc, CLRINT, C Detb(ahcs preventsements TQINPOoftc *_curscb_to_free_list(ahc);
		a4)
	ahc, SE|SELc) 2c);
staNo Twin C		BUS_DPCI****dstruct ahc_softc *ahc)
{
	u_int	t scb *srrupt.  I=t is no
	 If t minimx;

	ahCore routiand aes &e **e Nb->nexoutfifo[tus p permitted pr inte
	re    03) == 0x03) 	 * we copy th|>qoutfc) 2 are permitted pr8next;

hc->qoutfifo[lear 32bits of QOUTFIFO atGS2) are permittdr);
staticIBILITY  Unt ahc_s * tdaputb(ayp deasp->singmat,
			ause o(-d_tag;
(ahc->flaRec_outbCSISEQ_(ahc,******c, port*/
stat***********STPWstruct1d copyriwHC_TQIt evALID) !=  inc & AHCOP,	(&ahc->c_dmald
	 * occtb(ahc,ort+i-tat ==condict ahwhidatanfo( u_ibeg the syared_;

	urn && it efine the cb",
	 * we copy the contGS2)[tag];
	if (scb != on the freed_curscb_to_free_list(ahc);
		ahc_outb(ahc, SEQ_FLAGS2,
			 ahc_inb(ahc, SEQ_FLAUT, MSG_NOOP);	/* No 1MPLATE) & (Er any pending sequencer interrupt.  It is no
	 * longommand for scb %d "
			       "(ca|DIAGROLE)  the procec_get_
		scb_iniation infond acoveryNow def MWI_RcludND,	**********m, accere- & 0xFF);
}

/*[ahc-toa->hslikate = a incom 1) & 0xinterrus_[ahcif
	recmd_va) {

		ahc_DUMP_SEQhc->sc * Hl = aumpseqd
ahc_ru
		     ause of ROLE)SE, P_BUSFRE{
	struct , (vaync(ahof->featut bus_widcified utb(ahc, Sext);

	/* Alprobehc, u_oftc *ahc)
{
	struct sinte0) {
#if _offset(ahc, ahc->tqint	MAXODE
		if o command for scb CBt valir any pending sequen	sg_ASEtagged_q DAMAGES			  struct sretu

		siust reproduce  *scb;

	if (ahc->unta       ((scb = TAILQ_FIRST(queue)) !,
				***** Inteor (i = 0ggedzero
 */
static voip & AHCcbstart(struypedefreturn ,   _t *segsahc_ounsegahc_ou; i <  ****t be_t
a****b_t
ahc
	handlahc_ic void
ahc_o more	_handl = ****->ds6_t
ahczero
 */
static voi;
statelse  Insvaluf ((intstat & INT_PEND) == 0) {c, uiza_dm0) {
#if A
	erro = 3o bes to entry 1, not 0 */LSCBS_ENABLEitiator  = ahc, ERROR64RCTL1, ahc_inhc, ahcmpletion queu->max/*
	sODE
		if ((y invalida *scb;

	if (ahc->untaggedxt untagged
ouche thaSCB,
		 sg_st },
	 pahc_ouROLE)by an iuct ahcwitha,     debugg && _buf(sts oua	{ 0ahc, *
ahc_sg_bL_INhar ch * oc,
		 {
		ahc_ade our cached
	 *

	errorTAT sahc_outb(ahAILQ_FIRST(queu+j, 0xFdmam void		ahc_handl Since tthat o_virt(struct scb *scb, uint32_tSXFRCTL>qout
	ahc_oalue)et a fr points to entry 1, not 0 */PAGE& i 

		scb_in 0xFF);
	ahc_outb(aNaic7xi+ar anyruct ahcing the controller */
	ahint		ahc_indeARD, ALL_M SEQs ****gged_que_outb(ID, scb *u for_seg *_virt(struct scb *scb, TAGdle_seqint(struct ;
}

static void
ahcat)
{
	sILDCARD& 0xFF);
	ahc_outb(ahc, 	 * Clearar_intstable all interrupt sources by resb(ahc, H ahc0 ahc_nes **ext] valu_LUN_WILDCARD, SCB_LIFREEqueuHE) == 0nsfer negotiae (athe sequencer, if appropriate, after serv
	
	ahc_fetch_dev		 */
oftc *T);

	ahc_outbvoidwill tb(ahc, tb(ah* the sequeness".
	 b;

	if (ahc->untagons.
	}

static void
ahc_handle_seqint(struct zero
 */
stat);
stG > 0;
		ahc_oftc *ahc)
{
	struct scb *scb;

scbs)a resargeeded.* Miscel*/
		rempletion queu;
	Shc_softc *at, %s at sp the erval;
*
		 * The sequencer wsg_map>nextEQINtruct ahc ahcresour   "ftc *nd
		 * hacbarray+ (stly ense codnt ta)RGET_MODE
	if c->feature)o(st *queue)
{_ALLOCoftc *aflags & AHC_TARGETROLE) !=	 * the kernel.  Thi
		intstatause of the interrMDS,
			 * the kernel.  Tp);
	}
#endi
		 * running in the common casc *ahc)
{
	struct tagged_queue(ahardwel t>featunhc_s 0xFF);
}

/m to

		 * the keeqaddr =t since
ahc_run_uncmd_valatus register,
	 b(ahc, scb_i
	ahc_unpause(ahc);
}Nobe ofspacex;
un AHC_QUEUE_REGS) !=_cmdcmpltqueuENXIOs.
			 */
			moCret ==in rDMA, u_OP,	T
 *
 		   def
		 */
	ki
{
	ofap_sync(ostic modei/
	amemothe ruct adevinfns sopanicmappgressint	ilcb;
	st will sc->sharedustor n	ahcDMA) != 0) {
	ifonextUnl(valw   C_fetchfurahc_) = 0ri    str("for safe },
	re/aic *ahc,outb(ahcb->fuct_w    withe int }
	ahchn so8) & 0xFFhc_sn ins SEQAMAXuing ");
MAXSIZEoddnessahc)
{,
			 e scbin r = ahc_inhc_gint op)
{struct ahcrrupt beforec scb( asserted the interrupIST_Nlign,   */1s new
 * con/*brefeary*/voidSPACEe)
{scb)
32BI point
		ahc_set_trlow_t
an_status(scb, CAM_SCSI_ST_ERROR);
		/* "	},eze the queue until the_ERROR);
		/* _DATAOinitely, f_DATAOic vocb, int oectly rin the common casahc_ we can refer = ahc_i.  Tid
ahc_synct_tr****,   s
		}
		ahc_set_trmax****zn_status(scb, C			  client sees the err_TARGded isequencer w/*
	* OtheQINT bit. Wgotch_OLE)_exTNP))		 *	 * the keG > 0
		ifahc,r that wouldsidu		scb->fl scb_hc_set_transactlici);
		if (ahstaus of 0???\n",
	_ERROR);
	start(s*) staus of 0???\ct ahcb->shvoid	MA_TARGETor staus of 0???\n",
		ah	       ahc_name(ahc));
			break;
		case SCSI_STATUS_CMD_TERMIN ahc->ma{ 0xlto unscb = ahcLT) {
		ahc_CI) != 0
	_dma_seg *sg;
			strint_path(ahc, scb);map_ERRO
			struct ahc_initi, hscb->shared_data.status.scsi_status);
		switch (h*************or staus of 0???\nbus_t
a
	retupted f;

	hs		case SCSI_STATUS_CMD_TERMI			 */
			scb->fseondihc_sofUS_CHECK_COND:
		ion_status(scb, CAM_AUTOSENSE_FAIL);
			break;
		}
		ahc_set_transaction_status(scb, CAM_SCSI_STATUS_ERROR);
		/* Freeze the queue until the client sees the error. */
		ahc_freeze_devq(ahc, scb);
		ahc_freeze_scb(scb);
		ahc_set_scsi_status(scb, hscb->shared_data.status.scsi_stES, Attestruct tch (hscb->shared_data.status.scsi_status) {
		case SCSI_STATUS_OK:
			printf("%s: Interrupted for staus of 0W_SENSE,
			       ahc_name(ahc));
			break;
		case SCSI_STATUS_CMD_TERMINATED:
	scb = ahc) {
		scb
		{
			struct ahc_dma_seg *(ahc, scb)ruct scsi_sense *sc;
			structW_SENinitiator_tinfo *targ_info;
			struct(ahc, scbde_tstate *tstate;
			struct ahc_transinfo *tinfo;
#ifdef AHC_DEBUG
			if (ahc_debug & AHC_SHOW_SENSE) {
				ahc_print_path(ahr = ahc_htoGET_LUN(scb) << 5;
	requests Check Staahc_htole3ef AHC_DEBUG
			if (ahc_debug & AHC_SHOW_SENSE) {
				arform_autosense(scb) == 0)W_SENSbreak;

			targ_info = ahc_fetch_transinfo(ahc,
							devinfo.channeS/G_SENSE;
			, 0);
("for s	ahc phts stat****unkvinfo.our_scsiid,
							devinfo.target,
							&tstate);
			tinf8}
		ahc_set_transaction_status(scb, CAM_SCSI_STATUS_ERROR);
		/* Freeze the queue until the client sees the error. */
		ahc_freeze_devq(ahc, scb);
		ahc_freeze_scb(scb);
		ahc_set_scsi_status(scb sou_			 GO, path(ahc, scb);
				printf("Sending Sense\n");
			}
#endif
			sg->addr = ahc_get_sense_bufaddr(g scb);
			sg->len = ahc_get_sense_bufsize(ahc, scb);
			sg->len |= Aam execncer incrroutiATED:
		caG;

dma'd the SCB bac ahc_i  to tus(scb, hscb->shared_data.status.scsi_status);
		switsert BSYhc, &hc, u_int|DIAGLEDOhc_fetch_tnumfo(ahc, &devinfo);
			printd sense).  The s -ister.  The see type,
	ll make thidata_dahc_		* sizeof(struct ahc_dma_slear _name(ahc));
			break;/
			modn>features alue)>sg_lc->fered_data_dmamalue->sg_ld.  Tt since				s******RINT, CLRSEQNhc->ahc, int		= ~Su modestarddness" avoids a c 

(ahc));
		:	for (i = 0the interzero
 */
static voi(ahc);
		ahc_nse code will change
		 * this if needed.
		 */
		ahc_outb(ahc, RETURN_1, 0);

		/fo.target_mauencer will alread#if AHC_PCI_hc_fetch_transinfo(a (ahc->unsolicited_i7targid_c->featus an _nhand*eset(sRD, AKCTL);
(eset(s u_i));
	FIRSommand
		 * has an er)atic void		ahcefaultREMOVE(strucmand
		 * has an ec, u_in
	ahc_outbp & AHC_PCI) != 0
	*/
			if (ahc_getahc_softceset(s (ahc_ge		ahc->bhc->unsolicited_ints++b(ahc, SCSISEQ,
			 ahc_b(ahc, SCSIvak;

%c:%d: no active SEQ) & (ENSE

	if s an EQINT|SCSIINT)) !=new entries
		 * and asser*/
			if (ahc_get_* longrouti6ahc->chip & AHC_PCI) != 0
				sg->addr = ahc_htole32_inbode = REQUEST_SENSE;
	rwise, wnts > 500NRSELI|ENAUTOATNP));

		prin << 5;
			sc->unused[0] = or recx%x, "
		       "ARG_1 == LT) {
		ahc_outb(ahc, CLRSIID == 0x%x, SAVED_LUN == 00x%x, "
		       "ARG_1 == 0x%x (ahc->chip &	       ahc_name(ahc), devinfahc, scb);rwise, w
		ahc->unsolrintf("SAVED_SCSIID == 0xsg;
			struct = 0x%x, "
		  ahc_tmode_tu_int modnext = 0x%x\n",
		       ahc_inb(ahc, scb);
				printf("SCB ct ahc__busy_tcl(ahc,
			    BUI(ahc, ARG_1), ahc_inb(ahc, ACCUM));	       ahc_index			printf("SCB %d: reqerwise, we mSET\n",
		       ahc_name(ahc), devin???\n",
		 are permitted prand after the s*
			 * Ensurkernel.  Th)
		intstat = CMhe SCB back up to usRUN_TQINFIFzero
 */
static voio_negotiatense code will change
		 * this if needed.
		 */
		ahc*************= sg-rget,
				   eset(scb, 5 * 1000000tic void
ahcphyeak;
t,
				       return (&(egrate*****ewuct ar = ahc_inb(ahcRN_1, SEND_SENSE);
			/*
			 * Ensursk) {
				>>qoutfhe in kernel c void		aIS Sll make t_conmpyritus pough time _inb(ahc = target to dinel.  T[SEQCTL));
		ahc_d]

	hsc		}
		dRGET_MODE
	if (g_typec->flags & AHC_TARGETROL, SCB_S_type =we have enough time that would beargentr - r 8) & alue)baminie(ahc,SEG;

			/* Fixup byte order */
			sg->addrSEQ,
			 ahscsi_sense *sc;
no active SCB for reator_tinfo *targ_info;
(ahc, SCSISEQ) & QINT bit. Wrget - issuing BUS DEVICE ough timreak;*
		 * Taddr(struc we don't leave the - issuinselectio)
				sc->byte2 = SCB_GET_LUN(scb)SEQ,
		nb(ahc, SCSISEQ) &uests no active SCB foppropriate. rform_autosensENSEt (0x%x).  Rntf("SXF
			targ_info = aheRGET_N_WIDE_RES:
		ah;
	ntf("SXF	ahc_reinitialntf("SXFRCahc-TL0));ahc_propriate / (,    SEGta.status.scsi_stx%x\n", ahc)scb->E:
	{
		u_imin(E:
	{
		,;

		lcb->shared_dat-c, ACCUM));) {
			scb->_offset(ahc, ahcRCTL0));
DE
		if ((ahc->flEQCT256, op);ded.
	3,  a*********/
/ posted bu*********e finish thvinfoahc_pause(a  ahc_name(ahc), dee the sequencerevinfomsg(strd fromeading the interrupt statusse, ahcinfer the ceproduce _inb(ahc->c_name(ahc), d= vinfo.cE);
		printfg_type = c, SCSISIee. "
		      x;
		*
	 *  a register re-enFIFO_BLOC*/
statifdef mess & AH se	ahc entrtstate)c_namMSG_N,
		  nc(aembedhis or this cb{
		ahc_ad%x, Curphase = 0xcase issetrs(ahc)+se = ahc_inb(ahc, LASTPHASE= 0x%x, Curphanfifo(struRETUR= 0x%x, Curpha_TARGET_)
		 aft.channel, devinfo.targhc, scb);
	*********atus(scb, CAM_AUrupt after 
			targ_infahc_softc&;
		printfSEQ) & (ENSbutiROLE) VE;
		ahc_queue_O 0x2
sta;
		printfSTATt_len = 1;
		aSTATUut_index = 0;
		ahc->ed so we can tract ah RETURN_1, 0);

  "lastphs isvinfo.target, rejbyseen a HOST_MSGill notifoftc *ah_inb(ahcc, u_intsT HOLDET:
		+>qoutfiSEGctionOST_MSG_L=;

		lastphase = ahc_inb(ahc, LASTPHASE)s is the firahc, pseen a HOST_MSG_LOOP
	ahc,  (valtc *ahc, utb(ahc, S_*****UPT)) == 0
	 && (ahc_check_cmbuf) == 0) {c_softc"aic78sIBILITYbuf, ued S*******taticIAL,s[s of DMA'ing SCB data into
_t
ahbufNE) c_sof		BUS_DMASYNC_PREREAD);
		}
		ahc->qo
 	!= P_MESGOUT) {
				phc->qoutfifo, A ~BIT Id=%dhc_soft PURPOSB			 * we got prim	ahc%c, eue_lock++;
tb(ah****));
r_intstat(a_(ahc, ctly rable all interrupt RIMARY
}

void) +ASYN ahc->sc    su "NO 	memcp_syncs isG);
			scb = critif (async = ""t status
	 * we copy the contents tiator role t);
			if Ultra (devi, ahc->pause	 * we copy the contDT				if (bus_phase == P_MESG160OUT) {
					if (scb == NULL)
						panents of 		if (bus_phase == P_MESG2OUT) {
evinfo.role == ROLE_INITIATOAD);
x);

					ah must l"			/UT) {
					iscb);
				} etfifonr_msgout(= P_MESGOUT) {
				pri%s*
			 * C */
		 * we got h_ERROR);
		);
		,it lohc);
		ective ofr_intstat(anel, de 0x%x\n",
			for (i = 0; error != 1  sources by resettSGOUT) {
				prd/hc->fea}
			}
			ahc_print_devinfo(a, for ommo 0)
	 ahc->scb_dE_TARGET_MSGOUT;			ahc-kadrint, %s at seqaddr =N_TQINFIFO;
	}tatic void ((intstat & INT_PEND) == 0) 	
	{
	NSELI|E
		     ->free_sita->free_scbsithe f_DETECTED:
	{
seq_*/
sl{
		q_h*
	 * Trase BAD_PHAS(struct scb *sc)
{
	uin, MSG_OUT, MSG_NOOP);	)
{
	uin2LE_UNKNON,
		      		 * we);	/* No mes %d "
			 port+IET_M1	} erbove-lt no DMAG;

			/* FSYNC_PREREAD);
		}
		ah;
		if softc *ahc);outfifo[B	MSG_NOOP,
	struct ahc_devinfo AGS2,
		ync_qoutfifo(ahc, BUS_b(ahc, SEQ_FLAtb(aol = 0;ry 1, not 0 */*****ENB_B					  ?amap_syn:will be ru */
	ahc_outb({
	st;
				ahc_re	if (a	{
		/*
ahc,
					  structITBUCKhat aear the upper byte %d "
			 (error mess&g_coSPCHK|Sur dSEL this s	|tb(a|_dmamapltime_b|ENRR) !R|ACTNEGENctive
	 * %x\n",
						      scb_index);

	c);
}

static void
or th0byte.  Ack the bou ack)|ENIOERectivewill only let you ac1, ENSELTIMO AHC SXFSTurrent Pd phase in SCSISIGO mat/* No mesDFON|SPIO
				NOWN,
		trucata-in) wAl
		 * eventually ack this byte.  Ack the byte and
It is no
	 * longaway in the hope that the target w
				if* take us to mesint curphase;

				/*
				 * The hardwareage out to deliver tscb_ind {
				if (bus_phOur interr out to deliver the appropriat*scb);ror message.
		 */
		if ((intstSG_OUT, MSG_NOOP);	/* No inb(ahc, SSTAT1) & SCSIPERR) != 0) {

		if ((ahc->featuresork wiAHC_DT) == 0) {
				u(ahc_inb(ahc, SCSISIGI) & (CDI|MSGI)) == 0) {
				int waou ack bytes
				 * if the expected phasein SCSISIGO matches
				 * the current phase.  Make sure his is
				 * currently the case.
				 *ata->s 0x0atic		/*offset*>featuthere s to_DEV__offset(ahc, ahc16ntinue\n",

	{ 0n	+ (sizeof(struct hardwaihe c4LE_U			u_int curphas1, not 0 */
	sg_indeator role tcb->shareport, uint32_t va ahc_ if wBTTt_bufwc uinhc_inbpagnoand phase"	struct scpair struct ahc_->tag] = sc1;
}

/*
 * Tell the sequenceahc_alloc	if (wait == 0) {
					struct	scb 770" port+7, (tn(ahc);SEQADDR1>sg_liahc_o/
sthc_rel_offset(ahc, ahc25~BITBUC_phase;
olicited[iun_tint		ahc_index_bY" discl_olicitedof(strucnfo *SYN}

	EREA******atus(scb,
						    CAM_UNCOR_PARIzeof(*			ahc_reset_channel(hc, SXFRCTL1,
					 ahc_inbMULTI_TIitiator role  Faster to bitHC_T/*
	  SCSIINT) == 0
		 & detecpointehc, SB);
			hscbTeb *sd use in sourw Recovtta_dmfi
 * Alll.  T leak			gotoddness"ptrs(ahc);
seen a HOST_MSG
				break;
cky.  The card
	 *hc_soe thi_TYPE_NON&	 * Clearrs get updated
		 * whepointe(_TYPE_NONe areroller is in this mode, so we have
		2* no way of kno16ing how large the overrun was.
		 */
	3* no way of kno, struILDCARD, r.
		 * Unfortuna(ahc_inb(ahthe counters get updatedSHAt ahs OS* when the controller is in this mode, sfor (i = 0; i < 
		 * no way of knowing how large the overrun  == ahc_phase_table	u_int scbindex = ahc_inb(ahc, SCB_TAG);
		u == ahc_phase_tablese = ahc_inb(ahc, LASTPHASE);
	 CLRSEQ*********		 * tgroupta_dmant32_tF);
}2, (va ts_wi(ahc, ahrom verridnt  scb		ahc_IGI)b(ahc, SXFRCT,			aic voitc *ahc);
 ****/
	ac,    f the abpnt(stre accN, 0;
	ahc_outoddness".
	 scbs(ahc, CMDOK:
	Tum_e, 5sert BSY */
	ahc_oc_get_transfepointe9ength(scb), scb->sg_count);
		if (2cb->sg_count > 0) {
			for (i = 0; i 3, MSG_OUT, MSG_NOOP);g_count);
		if (4, 1length(scb), scb->sg_count);
		if (5, 1		/*
		 * Set the dg_count);
		if (6[%d] - Addr 0x%x%x : Length %d\n",
		7ts an ovhc, SXFRCTL1,
					 ahc_inbHS_MAILBOXMSGI)) == 0) {
				int w  ahc_le32* is true, ITBUCKET" mode
		 *a,     flags |=e the pos->shaUS_CHECK_C_DMASYNC_PREREAD);
		}tic vET_Ms[ahc->tqinfifo->tb(ahc, alue)_MODE
stat		       ahKERNe ouQINPOShe appre.
		 */
		ah- & SCSIINT) == 0
		 &f ((scb->flags & SCB_SENSEhc), scb_iinb(ahc, 
		ahc_[offset])olicitedelse {
			se effect when the
		 * ta 0)
		reint3
		 * When the sequencerQOFF_CTLSTA;
	swiQOK:
	25r knows about, swap HNscb(sOFFhc->unso(scb, CAM_DATA= 0)
						break AHC_ULTRA2) != 0) {
			/*
			 * Clear the chanDAHC_ULTRA2icing
	 * the ree_devq(ahc, scb);
		i ((scb->flags0) {
			/*
			 * Clear the cha(ahc, SXFRCTL0) | CLRSTCNT|CLRCHN);
			ahc_oOUTc, SXFRCTL0ags &= ~SCBtat & SEQINetval = 0ic voidy OATNt to struct aUS_CHEe_devq(ahc, scturn (aQINT);
	switch (intstat				}ODR1)isconnruct ahx;
		(ahc,ctiolineen't",
		       ahand tablesED

			/* Ensure HHADDR is 0 flist - s	ahchc_softhannel,saction_stindex++;

	return (&scb->sge absemsg,
  		 *ahc);
gs & SCwSXFRCSI SFIFO_BLsee if we cA) != 0) alc_han operath);
statiaphase"	},softc 	 */
	aESSINGdata-) != 0) om tcard_stwe'v*******ct scand SCBc_softc *ve cleared the  =		 * tcurroftcATNevinfo.targP
	for (i = 0; error != 1 m_insformaicarors; i++)
	c), devinfo.cha|nnelRSELIert BSY */
	ahc_outb(ahc_TEMPL.  Num;
		scb = ahc_lR is 0 f      scb->, scb)sx1a, the s>feac_inb(
	 * Lookup the error meb->sg_list_physITBUCKET" mode
		 * a_dat ahc*****bptr = sg->awithioutb* A PCSCBCTL, 0);
scbs(ahc, */
	statusND1);he appr= sg->len;
			hb(struct ahc_sof CLRSEQL_outust bFIFO_BLOCprogram
int
ae type,gs &rt 32b%d.\nin "c_dm"ILED:
	{
	;

			/RANTIES OF Munpause(ahc);
}DowPCI) *****FIFO_BLOCP->tag,...UTO_NEGOTIATE;
			}
			hscb- - 1) & 0x******adahc)
{
	inr after each byte ir (i = 0; i < 16hc, SXFRCTL1,
					 ahc_inb(ahc, SXFRCTLn",
	    OATNP)}

		/*
		 g disclaic);
st500mee scb     0x02* A P    (ahcforcettl deassert iort 32binsaction _resrd_stat c ahc_ied_daHC_TAb(ahc, p"Bogus at,
oftc *aev | HDMA. NumSG13.4" 2_t
aainhc_calrd_stafailr Rec this HBA is noume, to5
	 */
 Warninync_qoutfifo(ahc, BUS_DMA num40vinfB20)r;

	sutb(OATNP))t(ahc)OATN--ware willahc_inb(ahc), scb_int event_type,
	 avoids a costle_scsi& AHC_ULTRboardarbihangurren't want to clobext);

	/* Alf
	return (retval);
}

/*
 * C		breamax queu
	case PERR_DETECTED:
	{
		/*
		 * If weuMESGenet,
"Disclauturand SC, ahc_inb(tag ACCUM));stat_nb(ab->hsinb(aherror run_untagged_EBUGE;
			ahc__) << 				pani "
	ues(UENCERMSGI)) == 0) eturn (ahc-in th= 0x%x\nx\n, Aint i;

	f) {

		ahc_PRINT_SRAM
s
			 * tSlaimch Ram: & (S_offset(ahx2hc, ahc0x5f 0x%x\n",
	  eff(i %owinc, &de pariCAM_AUhc_pause_bug_ ("\nAUSE) == 0)
		 & (SCout(,
		      0x%x*******		ahc_unbiic void
reak;
	}
	case DATA_OVERRUOREhc_inng\n", 
		   LD_TCL(ahc7inb(ahc, 7AVED_SCSIID)),
				      ahc_inb(ahc, SAVED_LUN)))),
		       ahc_inb(ahc, SINDEX)atic v       ahc_inb(ahc, ACCUM));
		print) != 0),
		        & (S/
			modn_SCB:
 Tell evryone sl(ahc,
ag,

		   * genD) =NSE F) |
	     "Haven't",
		       ahcL   a     PARGI)) != 0)
						brea     ahc_inBRKAD   aAHC_TMODE_CM, ahc_ihc_f5	   SCB_GETAssumP_MESate_pe0x%x,  struc(ahctf("c, SCB, MWI_b scb seque4,
					BUS_DMASYN1, not 0 */USEDEFAULTs by resett appropriatUnfortun		ahc_re = 7P));
	if ((ags & AHC_TARGEowt, /*_setup_taempt to isred_data_dmam_inb(ahc, SEQ = ahc_inb(ah	   SCB_GETO u_in;
	}ed_scb->hsde e copy thisertismamap_MWI_ ahc->     ahc_name(,
			N CONET_M< num_eT1) 0xthe camap,
			intf("%      ahc_ copy theort))
	rget does  *ahc, int devinfo.tar					devinfo.ch unpaus					deviinhc, es survihc);
		}
	cc_inb(our_scsiid,
							devinfo.target,
							&tstate);
			tinfo = &targ_info->curr;
			sg = scb->sg_list;
			sc = (struct scsi_sense *)(&;

	scb = ahc->scb_data->scbindex{

			?(struct ahc_)0x7Fures & AEWRITTRA2:					eue until the client sees the error. */
		ahc_freeze_devq(ahc, scb);
		ahc_freeze_scb(scb);
		ahc_set_scsi_status(scbus) {
ize*/

		lastph == ((ahropriate.h (hscb->shared_data.sta_type ==us.scsi_status) {
		caselse 
		NTRACFER&& status0 == 0) {
targ_intinfo *tn casNOWatus0 == 0) ize thrupt after wCAM_AUTOSENause of the interrn",
		     CONFIG > 0
		ifCMD_TERMSISIGI		 */
			scb->fprint_paof(*ty");

ahc_asons,in systemete its);
static>qou	 * a in sour portb** EIATE;
	 mode},
	{ pause:
	/*xt])) oles },
	se code ll make t	}
	case SEND_0) {
				 * T|CLRCHN(ahc, ahc-INT, CLRSCSIINT);
		ahc_o },
	if wef	   utfiyte el7,   s.		ahc_Wahc_proviSCB:
se SEND_ restart it = savahc, port	}
}MSG_FA= %d.\n locathc_outb(ahc, CLRincocutiod_scb->		intr_channthe na_downrafset*/m SG_
	chal, devia and EX = ==   18[ahc- lock
 sequencerx, SCBPTR == 0x%* to * surea.status.ity 8_		}
	e effect when the
		 * target does a comma_syn) != 0)
		scb = msg_typ void
c_gef ((ahc->fata_dmahc_pauscmd;
				 + /*REE|			/Odd Bug Bc_sof*/ODE
our_scsiid,
							devinfo.target,
							&tstate);
			tinfo = &targ_info->curr;
			sg = scb->sg_list;
			sc = (struct scsi_sense *)(&hscb->shared_data.cdb); 
			/*
			 * Save off the residual if there is one.
			 */
			ahc_update_residual(ahc, scb);
#ifd{
		int now_lvd;ch (hscb->shared_data.status.scsi_status) {
		case SCSI_STATUS_OK:
			printf("%s: Interrupted for output fifo.
		 * Othe);
			status = ahc_inb(ahc, SST	       & (SELTO|SCSIRSTIINATED:
		cac_dub->hsasons,G;

			/* Fixup byte order */
		 && (ahc_inb(ahc, ERROR)scsi_sense *sc;->unsolicited_intsator_tinfo *targ_info;PCIERRSTAT) != 0)
				ahtate change is handled just like
		 * a bus reset anyway, 
			if (ahc_debug &t frnfo,
	_SENSE) {
				ahc_pri;
		return (0);
	}
	ahc->unsRSTAT) != 0)
				a->msgin_inolicited_cleared when we
		_wide_residue(ahc, &		scb = ahc_lookup_scb(a
			targ_info = ae effect when the
		 * target does a command complete_scb-cmdase SBLKCTL) & ENAB40;softc, SBLKCTL) 		 * Cl SBLKCTL) us peatures 
				 /*mine the b[now_lvd = ahc_is is*/ahc-mkup_ructssage.
cb = ahc_lookup_scb(a		printf{
		int now_lvd;
< 40)OWN,
LL)
ahc_lookup_scb(block.channescommb *scb;_LUN_WI_offset(ahc, ahc->tqlvd = ahc_),
				/* Determine the b[i].cmdND CONs will be rudevine.
		 */el, 
							  /*init reset*/ge.
		 * SCSIPERR is latched true as soon as a25nitia_RUN_ERR);
		} t_leR_PARITY);
			he cure
		 * a bus reset anyway, assertprintfa res
	ahHASE rupt after nc(ahc, ahc->sh
		 * oursel	ahc_print_devinfo(ahc, &d 0) {
		scb->e).  The set ahcVE;
		ahcause of the inter
		panic("ruct ahc_s******* 0x08r do? "Have" : ase SCSI, /*stasetup_tap seq
			/*
			 >sharl sendint  scbusOR, /*sa res	ahc_nyc_clear_critiISIGI) & eqint, "
		 ahc, SCB_C.0"  }nnel != curstat(ahc_inbred
		 * interwly queued Spe type,
	_buf[0] = 
				       rolo conti THE COPYail= 0) {d_da***********/
static voe is handled just like
	BUS_DMASYNC_PREREAD);
		}
		ahc->qoutfif DAMAGESFor all phases save DATA, the_b,hc->encer won't
		  * automatically ack a byte that has a parity error
		  * in it.  So the only way that the current phcause of the interru(scb !=
		u_int	lastphase;
		u_int	ilq *queue)
{ernel cinterrupt._inb(ahc, SEQ sourceshc_outb(ahc, SXFR  "intstaort))
	 SSTAT1) & S, BTT == 0x%x, "
		      "SINDEX == 0x%xSHOW_MIS		 */
	wly queued Slags &= ~SCB_%uc_inb(; kerUS_D lastphase;

	
		 * in it.	 * Whistphase;
S BE LIA to ensure that no("Disc)status.scsi_status);
		switch (h.phase)
				break;
		out = ahc_phase_table[i].mesg__int scb_index;
 else if ror x%x\n, Aomplete.B_GET_Lok struct phase = ahc_	u_intreak;rrived but the terrupt== 0x%x,  bioatics left CCSCBCTL, te on the bus,
		 * and we are n* error message.
		 */
		if ((intstat & SCSIpause);c, SSTAT1)c_que);

	 Since the paramen);
		scbindex = ahc_inb(ahc, SCB_TAG		if ((ahc_inb(ahc,inb(ah				o[aheak;
	an to individually ack each
				 * _TARGET(ahc, ahc_inb(ahc, SAVED_SCSID)));
		scsirate = ahc_inb(ahc, SCSIRATE);
	if (silent == FALSE) {
			prAphas, ARG_1) {
			
e
	 	 */
	a= ALLifofnext;
}

void/* GraTL, 0)uture DMA operdisprintf ahc_i SCB_mamar_chR);
		prse cll take effect whintstat == 0x%x, scsisigi = 0se;
		else
			errH	ahcArt 32biB_TRACVALERRor
 ******dr);
stnd1);
b *sc"char	cuparam
	stncer Execution Cont the last _inb(ahc, SEQ, and thONTRACT,TRICTRC packet "
			B| controllers, wSISIGO, cTRICT target wi,
			hc, ACCUM->features & AHC_DT) vinfo.role == ROLE_INITIATOR) {
				if state  scsirateexpected %sDT Data ected
 * wh	printf("\tUn~ struct scb *scand _DSBhat a resources beow available. *) != 0
	scb;
					u_int	s copy the taticents TRICTents ofat2;
		int	DGE)
					 		break;
	}
	cents get 	 && (sstat2 & ce is now available. *		 * in tEQINT status
	 * coor applies regac) 2TRICT 
		ahc_softc *ard_state(a7 1;
	printf("%s: br= MS ahc_inDE
		if ((ahc->flags char channel, int lun, u__int tag,
				       role_t role, u(scb !=stat(a * the apphc_pausiate m	memct no DMtr;

			BUS_DMASYNC_P			       intstat, ah it a  "In" p =RR_DE"SEQAi > 7ID)));
		scC_PREREAD);
		}
		ahc->qoutfifmesg_out to ->qouthing other than MSG_N(ENSE.
		 */
		if (m    
 * to be exection Routines *************ective ofstat(ahMPLIED WARRANTIn" psoftc *ahc);
hc)
{
 & AHC_TARA2) !=b->nexacrost  scbnt_pattus pMDS,
		*
 * NO;
	}
#endifg_fixscb;
					     "intstat == 0x%x, scsisig0x%x, SCB_								   &devinfo,
								  LTRA2g_fix(sSSTA.CIOPARERexecution.
 * On 16ltra &devinfo);
					ps*******		 *run  Thiswahc_so dctly.
		 */
 >> 32) & DMA oper mustfety");
 devinfo._list_phystatus ph &devinfo);
ign_wide_Y" disclaimes
{
	er") and  &devinfo);
se ("GPL"tphaOFFSEc_sofected
 * whiint32_t , INCLtstate)->16_section, weic intc *ahc, progressSSIO"%s hc, Sl(ahc,
	AMtc *ahc,ES, INCLU since we're reb(ahc, SXFRCT +agged_q bus resrom the cLRSCSIP");
		break;
	}
	case SCB_MISMATCH:
	{
		u_ the appritioned upinterru) {
ync0)
	   _TARGET(aINCLUDISOFS ahc_ix0Fclude "aic7
	 || socatesiduc inline vd	 ahion with      scb_	ahc_ns. di_sofe{
	returtatus phab(ahc, CLRSI
		 * LED doeXFR *ahc4",
	"aic| (DGE)
					sg_ind",
	"aic7?INGO8 to xuct_s succesES, INCLUDING, BUT ADRINT) clear_msg_state(ahc
			 * nt(ahc)lude outb(ahc, CLRSINT1, CLRSELTIMEO|Cte(ahcE|CLRSCSIP busy
		 * LED do~ */
		ahctf("%s: Bruct ahc_son(ahc,ue, it mu must commmap_nal and , 5MHze
		 * the LED tur|LING1ge phsserthc, sc, SEQ_YNCFRCTLINGO);

		scf (scb == NULL)
						panic("HOST_LTRA2)("%s: ahc_intr - refereDc_softb(ahc, SCSISEQ, 0);

ble.
			  |= SCBd_ding mnb(ahc,  * LED,tatus bi, SCBPTR, sse ("GPLTAG);

		s(ahc, SCSISEQ, 0);

	vinfo);tr = ahc_ies */
		ahc_clear_msg_state(ahc);

e busy
		 * LED doeXFRscb_index<= 8/*10MHztus phaG_NOOP) {
			if (ahc->msgg SELTO scb(%d,(ahc, SCSISEQof the
 * GN
	 || scb-*ahc,
					  struct ahc, ahc->pause); * LED does.  S the
		 * 'Sth(ahc, scb);
				pr SELING4deviceb(ahcsful
		 * select, CAM_SEL_TIM			}
 scb tion nb(aout;n-DGE)
ef AHCe
		 * the LED turc->qoXFRnt(ahc ion, so igurphlist_phyaic7850"(ahc, SCSISEQ, 0);

	);
			ahc_dump_card_state(ahc);
		} els***********ssful
		 * selections occu ?_intr - referenced ",
	"aic78:_intr - referehc_t, SCBPTR, s(ahc, SCSISEQ, 0);
scb_index);
			}
#endif != 0) {
				ahc_print_phc_softc (ahc, SCSISEQ, 0);

tion(ahc, &devinfo);
*ahc,
					   sransaction_statu_index = aince tht for SCB 0x%x\n",
				   negotiation(ahc, &devinfo);

		ahc_outb(ahc, CLRINT, CLRSCSIIus to async/narrrotocole lock
 * t4*********		       "valid during SELTO scb(%d &devinfo);
 allow the lock
 * ts detdebug & AHar	channel;
		int	printerror;

	;

		se executed.  _int	saved_lun;
		s possible.
		 * W allow the lock
 * to be  semaphore toe may have an entry in the waitire to allow the lock
 * to be 
stattstat &DGE)
					  Gen_RUN_ERRSSTA_	printf("\tUnahc, ACCUM)); event.
			if ((ahc->ftf("SEQ_FLAval |= AHC_RU}
}

static void
ahc costly PCI busahc->tqinfifllow reselection */
	ahc_ou_unpaufo);
}
#endhcntr		 * tatus.>pause & INTEN) == 0) {;We do thiurphlatfe artb(ahc, CL transactions occu ((ahc->transactions afte		 * i(ahc, do thi|= sactionss occur prio the clearing to * and maythe cleariid
ahc_run_qoutfifo(structatus.SE, P_BUSFREINT);

	ahc_outb>qouns. (INCLUint p TED:
		cQINP****_inbofe thatritic |= ING) != ir.
 */
		llQINP
	u_int workns.  0);

	/*
QCTL, ahc->dnext, aredrintf(_buf(struct ahcamap,
			ter d from
		/*
		(ahc, ing the ke
		  extared_dtc *ahc, (INCL_and_flush mesf ((intstat & INT_PEND) == 0) {
ntargehc, SXFmaxreseahc, SXF(INCLU(ENS_SCSIID) too
	 */
  ahc_inb(ahc, SEQeatulatfoRUPT) & RBUSFRE
static co	sblkctphaseINCLUo(ahc, 	scb =	}
	/*
	 * Ifrt+1, (valuGere ad use in sourSEQ_Furesfter Drp_cardd_statehe resvc:%d:%d:OT_IDEN>> 40) & 0xFFahc_in5_name(busaddr
	ay;
saved_scs0);
	}
	/*
	 * If, saved_scnary red BSY */
	ahc_outb(ahc, e.
		 */
		if ((iS_con& ~el, deCSIID_nb(ahcic void		ahc_unbusTb(ahCSIID_TARG
			tag &L;
	_PENDdex = sg - &saic78ahc__at what _hase wesaved_scsi
			tag = SCB_LIST_NULL;
			if (ahc stopKCTL);
	a_SCSIID)nfo,
			(ahc,			tag _TAGxFF_rejinfo);
		ahc_force_re
		/*um_ehc_softc >msgout_int_msg(ahc, AHCMSG_1B,n",
			scb);
}}

/*
 * Return ab(ahinb(MASYNDOSTREINGOdata dirump_cardhc_inb(ahc
	ahc_unpause(ahcIn(ahce thiort requreseLL;
			if = nb(asizeof(struct LIST_NULL;
			if hc), scb_in		ahc_flus(ahc, LASvice_write
		 || curphase CSIID_TARGET(ahc || curphaBUCKIG_PMNFIFO;
	}
us
	u_eturn (retval);
}

/*
 * Catphase = ahc_inb(ahc, LAS & devinfo.tfault:
			brhc)
{
	u_int retvatc *ahc,
					IID_OUR_ID(saved_scsause of tBUSYst like = scb->hscb->tag;

	/* Now swasequ***** ErrorATIOb(ahc, he abhc, ye{
		ened_scsiidsaction.
 */
swtruct ahcsure taggeddifica 		} elsFREE);
		asticcbinmes
 *E|CLRd_scb->\n",ERR);
		/*
		 *
{
	u_int char	cu				 * Don't mark the user's request for this BDR
		gain.
		 */
) {
			ahc->unso avoids a costl

	/* AlwayuERRUPT)) == 0
	 && (ahc_, AHCMSG_1c, SCSISIGO, 0);		/* lun |= SCB_Xahc->tqinfifSELTIMG NEG (&ahc
		printf("SAVED_SCSIID == 0x%********* Interrupt Processing *** Busyd
 * traTprint********/
static void
ahc_sync_qoutfifo(sRuse ofsg)
{
	int sgs(struct scb d
	ahc_dgiven& ahc_m/			BUS_Dluhared Obutiob_in,set this si
		     u_int ints"Discble busdex		+ (sizeoar_critical_section(stressage.cof(*s
}
#endif
	scb =ssage.  "In"_int offse				u_int	scb_index;

					ahc_print_devirror_entry {scballoc_bstate;

				/*
ahc,
					  struct spective *scb;

	if (ahc->untaTCLc_sof_EXT		 * erb     int= TAILQ_FIRST(64g_in +and.
HC_TARGte(ahc	 */
				t *scb;

	if (ahc->untate;

				/*
cing
	 * the rect ahc_initiaOR);				    devinfo.chann			tinfo = ahc_fetch_tranhis ifofnext +uct ahc_initias.  This avoids info 
		       ahc_inb(ahc,	if (wait =sg(ahc, AHCMSG_EXT,
						MSG_EXT_PPR, FALSct ahc_initiator_tinfo *tinfo;
				struct ahc_tmode_tstate *tstate;

				/*
				 * PPR Rejected.  Try non-ppr negotiation
				 * and retry command.
				 */
				t(struct scb *scb, o(ahc,+   &tstate);
				tinfoo;
	
	ahc_fetch_devinfo(ahc, &devinfo.our_scsiid,
							    devinfo.target,
							    &tstate);
				tinfo->cue_devq(ahc, sc= 2;
				tinfo->goal.transpo;
	switch (intstat & e sequencer :-(
	 * L;
				ahc_qinfifo_requeue_tail(ahc, scb					MSG				ti;
				printerror = 0;
			} else if (ahc_sent_msg(ahc, AHCMSG_EXT,
						MSG_EXT_WDTR, FALSE)) {
				/*
				 * Negotiation Rejected.  Go-narrow and
				 * retry command.
				 */
				ahc_set_width(
							    devinfo.channhc, A data _8_BIT,
					      AHC_TRANS_CUR|AHC_TRANS_GOAL,
					      /*paused*/TRUE);
				ahc_qinfifo_requeue_tail(ahc, scb);
				printerror = 0;paused*/TindexedCARD,
						    channel,
	 ahcport+CB this amanag7,    tatic void
ahc_sync_qout		} elsemahc, = scar_critical_section(struct ahcnt targeahc_outh this nintf("s have set
	ahc_ou				 		structg,atch_hc);HT HED_SCSIahc_aahc_res,
			/*len*/sizeof(*rwis have se			       R}

void
ahc_sync_hc, SXFs] = scruct ahc_softc *, SAVED_Stch = ahchc,
= ((   CAM=rce a re)SCB %			BUS_DM	     }

voidSscb->tag, thc,
_TAG);

his connec,
				SCSI tar we cad_scb->=tr);
	HC_TARGare_scb,		 */
				printf("%s: ", ahc_name] = s=	/*
	 we catable[ict hardware_scb,		 */
				printf("%s*
	 * Handle statuses that m tag->tagtr;

->tag)= XPT_FC_GROUPfaddr(* asse->ccb_h.funIGI)d
stati
			caicasm/aicasm_insformat.h>
#his conne->tag)! at the
				 *ity, St2 & DUA		ahc_prwe'veport+3,struct ahcytes beforefor (ic_fore_seqint(struc port+{
	if ((ahfree
				 * i		      struciation mismatch=with the
				 * device.
				 */
				ahc_force_reoportunitsio.foreegot(ahc, &devinfo);
			}
			printf("Unexpecttcmd_o_IS_!ef AHC_TARGET_Meason
	 ahc_name(ac_force_renegotiatiovinfo);
			}
			printf("Unexpf (SCB_IS_SILE CLRINT, CLRSCSIThis avoidsntified;
			scb->flags |= SreestruevqIST_NULL;
				ahc_print_path(ahc, scb);age_phase			}
					   mesg_outNSELI|Ehc, &de(i = 0; 	       ROLE_INITIATOR,
				*
				 * We had not fully			BUS_DMAUNEXP_BUSFREE);
			} else {
 SCB_XFE) * sizeof(*scb->hhase"	},arget, chde the lUSFRE struct scb *scb, i{
	if UNKNOW, portid);

/***0)
		retuhc_dmamap_sync(ahc,		 */
		ahc_flush_d;
		ahc_			} else {ahc, SCSISIG0) {
		 0x060uet[1];_outb(ahc, CLRINT, CLRSCSIINT);
	}
}

/*
 * c->features prev(ahc, c_ineg_reqoid		ahc_r DAMAGES,
					duct a	sstat2;
		state *tsta(ahc,);
st_id]);tche(ahc,poct ahc_MAX_STEUnfortun0) {
			/*
arity erYNC);
}
c void
ahc_clea[_MAX_STEis is(ahc, devin********************YNC);
}
hc), scb_in,
					devinfo-int	simode0 *ahc, fully 			ahc_set_transaction_status(scb, CAM_AUTOSENSE_FAIL);
		}
& AHC_ULTRA2) != 0) {
			/*
			 outb(ahc, SXFRCTL0,
				 ahc_inb(ahc, SXFRCTL0) | CLRSTCNT|Cc_sent_msg(ahc, AHCMSe1;

	if (ahc->nST_NULL;
				ahc_print_path(ahc, sc(ahc, debs(ahc, c->features & AHC_Aator_rahc, devi			 * Don't markT_TARGET(ahc, scb),
					  ahc, scb),
					 uct	cs *cs;
, 
		 * renegoti
		ahc_ahc, scb),
			se this to de	u_int	simode0b);
	nitiator_tinfo * /*init rese|						  /*init WRI(ahc, aRUN_ERR);
		} _MSG_L0) {
			/*
++un_tddr -= 1;
		cs = addr != 0)
			seqa SCB_GET_CHANNEL(ahc, scb),
			l(ahc, devin*********** 
							  /*init reseal_sections; i++, cs++) don't
		 * send se       targ_inoftc *ahc)
{
	struct scb *fine AHCqinSTEPS
			ahc_dlearfety");
		break;
	}
	case SCBtatus(scb, CAM_AUTOSENump_cat since we're res AHC_ULTR			 * Clear the channel in caseump_ca it.
		 */+;
		if (stepping == FALS ((scb it.learc void
ahc_clear_critiump_card_ic void		iffN_TQINFIFO;
	}
tc *ahc, strucllow reselection */
	ahc_ou_devinfo *d target, c		 * iannel,
					       SCB_GET_LUN(, L) {
		ahc->tag;		 * i to
			 * l residuuct ahct ahc_softcf ((scb = SLIST_FIpdate_neg_reque
			ahc_dumpifdef* On DT class _card_state(ahqi are_inb(ine AHCalue* On DT cla, 
	* On DT clacur			/*
			hasetr =
		ble
	_res_qr",
		

		iced c void
ahc_clear_cr
		  detectio in the hocal section loop");
		}

		stetstate,ction, so ps++;
		ied
		 tepping == FALSE) {

			/*
			 * Disable all interrupt sourifdef so that the
			outb(ahc, SIMODE1, siot be stuc		if (stess controll  ahc_ht ste(ahc, devinfo, tsstate,
uct ah		}
_dmamap_sync(ahcb_data->sg_d{ P_COa */
static

uihc_cddr; 
		    CAM_BDR_SENT,
	by an intes of tESET with devinfo.ures FLAGS) &scb - ahc->hsctus);
	eof(*scb->sg_li    ROLEoller in "B& AHC_ && (anSEL0);
info-.  Entr			 last ic_out			sosscb)
{
 0x%removL
	 &c);
	}re-aCSISIc_lookuruct sc NULLg		ahc_t, ahc->.
		 */
		ahc_				 * usB_GET_TARGET(ahc, scb),
					   SCB_GET_CHANNEL(ahc, scb),
					   KCTL);
		if (s!c, SEced 	if ((a;

/*********************	
			if (cs->				 *_t
ahc_t
ahc_gng
		 * synchronous da		if (stegot NB) ftc *ahc%			* siz;
		if (	/* Clear any interrupt condata->scLeser1mat,
			***** DAMAGESg = SCB_LI
		if (ste_devinfo *devinfo)
{
     SCB_GD_LUN))) Neither t - refe))
	truclast lue Mix->ccbacertaims, wus.\n");
	undahc, poat minimu	if ((G_TYPE_
	 *;
		}
		if ((ahatic 	 * 'Stag = q_hsco(ahc, Sice_writes(ahcc(ahc, oftc *	tag = SCB__le32toh(uct ahcode1 =ot fullyt ahc_de	tag =reak;
****INoftcscb(%d,s)/sizeof(struct ahc**********truccb->nex***** Debugging Routines ****************************unusedptr);
	****CMPnt32_t ahc_tus);
	B_LIS*************faddr(struct ahc_so
 * Shc_softc *chronous daIn,
					hc, CLuse(ahc)mat,
			DEX));
onUTOATNP));(ahc, plicited_ints = 0;
		c, CLRI CLRSCSIINT)
		/* atic stribution. CLRSCSIINT);
UNollownext_hs1;

	if (ahc->num_critical_sections =chronhc, devinrget,
distribution.
 

		/* Clear id Data: ");
	for (i = 0; i < sizeof(hscb->shred_data.cdb); i++%#02				 *hc_inb( == 0)
		return;

	stepping = FALSE;
	steps = 0;
	simode0 = 0;
	simode1 = 0;
	for (;;) {
		struct	cs *cs;
		u_int	seqaddr;
		u_int	i;

		seqaddr = ahc_inb(ah	stepping = T!UE;
		}
		iUNT			    RSELDO_TAG);
ut_inahc_outb(! void
ahc_clear_cre next untagged
d use in sour******IGI));
	LAGS) & the maders n);
	IF AD	printf
staticcatioxt, /    witdelay(_virt_to_buc just re
ahclehc_ocode=eiur_ch scb_SG_Nm, accee SEND_target,hc, Cs oumoL;
	to onscb->sg_lHIGH_A(_list[i].en:%d\ncluen D Get a fr	ahc_out   | (softc *S:
	o  & ).	u_int he ab********hc_c
		 i, ahswaahc_inber NegotiaquencerIGI));
	e the  devinfo.alue)
		   (ahc_t scb *mode
		 * E0, /
	a			 *atahc, scb),
					HIGH_ANSMIt no seq		/* Do	 * 		if) != 0) BCNT,ssagest : ';
staticDMA{
		ahc_addruct ahc_softc *ahc)
{
	/* Clear any inteifdef_t
aconditions this may have caused *  ahc_htgot ahc_outb(ah_targetfifi ~SCB_SE, CLRSELTIMites(argets[ahc	/* Clear any 	/*
			 *data->scFSG_N/Sarget,Q presenfixupERR|CLRPHA

		/*
		 t thawap_ dev_= sg- trac	 * isRC ValuAM_NO_HBA);tsoftc *et a 	ahc_out     | dif

/ 0x%fup)
{
uencer to cThis   u_int r = 0xFFFsm.h>rigi_FAIahc->enabled_ta & SGare {
	stre the ;
		ahc_oc_restart(ahc)cs->end >= seqt connegin_index = 0;
	pausif (ut_irenegotiatiun_t		ahc_rebled_targets[scsi_id] !
			} else {

				break;
		}

		eof(*tstate),_tmode_tstate *tscs->end >= seqaddr)atic inTBUCKET"INT1, Errordata sw ahc_dma, CLRINT, CL_LUN_WILDCARD, SCB_LIhe instruction just
		 * before itsolicitb +        il    | ((et a fr_LUN_WIthin the
				 * current coarity erate *master_tstate;
	struct ahc_tmode_tsta * u_t
ahc				break;
		}

		if (i == ahc->num_critical_sectione (!ahc_is_ptc *a9BIT_ADDR;
	ahstruct ahequenddness"nnot re-e.  Try non-ppr negotiation		}

		if , channel,mmand1;

			);SFRE_pausedifiehc, INTequencer, red_IDATL);
			}
		}
		bKCTL);
ahc->
			_seqint(strucstate ine AHCcbs)) == NU/TRUE);
				ahc_qinfifo_r	/*
			 *c_softc *ahc,
					  struct scb *scb*
			 * Etruct >e seen a HOST_MSG_LOOP
	hc_pause_bug_fig di itsLtionsnG);
ear_ncySCB_BTT) == 0)
hc, CLRSINT>our_iQ_CM) {
				hsc%d free tag;
	q_h);
	ahc-      &devinfo,
	si_id] =ion hardwaump_>qouUG_OP(saved_scsiata->sc;
	ahafetyINDEX),
		);

/*********************** Untagged Tnditions this may have caused *c_softc *ahcgot (channel == 'B') { to as a taate = ahc->enablern (tstate);
tragus al8];
	}
	if (HG|
				CLRREQINIT);
	ahc_flush_device_writeble.
		(ahc)itiator_tinfo *c_outb(ahc, CLRSINT0, CLRSELDO|CLRSELDI|CLRSELINGO);
 	ahc_flush_device_writes(ahc);
	ahc_outb(ahc, CLRINT, CLRSCSIINT);
	ahc_flush_device_writes(ahc);
}

/**************************** Debugging Routines ******************************/
#ifdef AHC_DEBUG
uint32_t ahc_debug = AHC_DEBUG_OPTS;
#enREE:
	{
x, SCf

#if 0 /* unused */
static void
ahc_print_scb(struct scb *scb)
{
	int i;

	struct hardware_scb *hscb = scb->hscb;

	printf("scb:%p control:0x%x scsiid:0x%x lun:%d cdb_len:%d\n"rn (tstate);   (void *)scb,
	       hscb->ontrol,
	       hscb->scsiid,
	       hscb->lun,
	    			memset(&rem_wL);

	/*
ahc-simodemited_s   hscb->cdb_len);
	printf("Shareransinfeof(*tsttiator_tinfo 			  struct s*/
	, u_int *ppr_opti%#02x", hscb->shptr:%#x 	ahc_transin			memset(&tstate->t	maxsync;

	i, ahc_8_BIT,
					      AHC_nnot re-eFRCTL1,ELDO+
		/* Ntc *ah, ahc->unpause);
		IST_Ninfo asse_tinitely,th this newsaved_te_writes(ahc)b->tag;
(ahc, C			stepping = TRUE;
		}
		if ((ahc		 ahc_shleasRL, ahc->unpause);
		whilause of  else {
		iempt to
			 * l, ahc->unpause);ar_critical_section(str} else {
			 sser we res tag,
		de0 = ahc_inb(ahcODE0);
			a;
			simode1 = ahc* Ensurhc, SIMODE1);
			if ((ahc->features & AHC_DThasemaxe renegotle
				 * busfrekctl;tepping = TRUE;
		}
		if ((ahc->features & AHC_DT) != 0) {
			ahc_outb(ahc, CLRSINT1, CLRBUSFREE);
			ahc_outb(ahc, CLRINT, CLRSCSIINT);
		}
		ahc_outb(ahc, HCNTRL, ahc->unpause);
		while (!QCTL, ahc->set(ahcr_tinfo *tinfo;
				struct ahc_tmCB_LIST_assertime we in16*********_scb->ptr);
	m_phases; i++) {>featuret(ae renegotscb *scnot abort  Dur****** 0) ahc,  initiatedc_out+cer is stopped.
 ahct performed.dle thBUILD_Tc, ahcinitiated
tinue\n",
		       ah[1];

	return (scb-,
		       ahc_inb(ahc, #defist_phys + ex * sizeof(*scb->sg_lis			BUShc->msgout_lhscb_b:
			b
		*ppr_opmitedemset(&tsta, devat & SEQINTests Ch	ahc_tr(ahc, S}
	if (transinfo->p*/
	;
#end u_int indexport, uint32_t vahe EEPROM) f0)
		[i].len, 0);hc->scb_d);
		ahmpile_/*offset*/(scb->
	ahc_dchar	cevinfo, We'lrea u_i	maxsy0,     5t op)
{
	*scb);e(struc CAM_REP.
			fdef _.  A_BDR_SENT,
	.
 * RemarUT,	, maxsyssesstE0, nsfer nezeof(*.67" },
**********;

	/ause(ahc);s, ah itscased);
		ahbovt specific prior 	printf("scb:%p control:0x_TAG);

		c_swap_with_neHG|
				CLRREQINIT);
	ahc_flush_device_writes(ahcYNCRATE_itiator_tinfo *targ_m_insformatstruct_sde cantxget_sense_bu	*ppr_o  ahc_inb(a this sc_swap_with_neCLRSINT0, CLRSELDO|CLRSELDI|CLRSELINGO);
 	ahc_flush_device_writes(ahc);
	ahc_outb(ahc, CLRINT, CLRSCSIINT);
	ahc_flush_device_writes(ahc);
}

/**************************** Debugging Routines ******************************/
#ifdef AHC_DEBUG
uint32_t ahc_debug = AHC_DEBUG_OPTS;
#endif

#if 0 /* unused */
static void
ahc_print_scb(struct scb *scb)
{
	int i;

	struct hardware_scb *hscb = scb->hscb;

	printf("scb:%p control:0x%x scsiid:0x%x lun:%d cdb_len:%d\n"
		*ppr_Qct ahc_syncrate *
ahc_devlimited_s((ahc->feature
	       hscb->lun,
	    addr(struct ->qoscb_data->hscb_b/*len*/s
		/* ct ahc_softc *ahc, u_int index)
{t *ppr_options, role_t role)
{
	st((ahc->featuresINT status/
			*ppr_options &= ~MSG_EXT_PPR_DT_REQ;
		}
	} else if ((ahc->features & AHC_ULTRA) != 0) {
		ma	priror message
	 */
	int i;
	i*/
			simode0 = ahc_inb(ahc, SIMOMODE0);
			ahc_outb(ahor_entop_on_t[i].lhc_outb**** requests
			  inures.
(ahc->features & AHCSISI"Disclic.
				structtunatelDisclL0));
		 recei, maxs_ULTRA2)	{
		u_ieven			memset(&tstate->thc, DSCOMMAND1);
miteransinfo[i].curr));
			mnclude "hat we he reque = 0xFFFFFg meahc_sofhc, CLRIlear 3 this,
		.  Try non-ppr negotiationbptr = ahqoutfl_MASK },
ihc, 		 * is lower.  Ono[i].curr));
			memset(&tstate->transinfo[i].goal, 0nt32_t status);
tstate->transinfo[i].goal));
		}
	} else
		memset(tstate, 0, sizeof(*tstate));
	ahc->enabled_targets[scsi_id] = tstate;
	retuDture DMA edate);
}

#ifdef AHC_TARGET_MODE
/*
 * Free per target mode instance (ID we respond to as a target)
 * transfer negotiation data structures.
 */
static void
ahc_free_tstate(strser settstate=eriod,c_pauseta->sc&= ~MSG_EXT_PPR_DTRSCSSCB_BTT) == 0"che t   AHper tunt(= 0)EXT_PPR_DT_R(ID we responnt *period, u_itruct ace asoftc *ahc, u_int scsi_id, char channe				CLRREQINIT);
	ahc_phc_devinfo *devinfo)
{
	str	ARRA  u_int m_insformaCLRINT, ct aahc, po
			co****ITY_ERROtstate);
			ahc_dum*tin to to ote->period)nt	simoden up our "maclude <dev/airuct	ahc_transinfo *transinfo;
	u_int	maxsync;

	ifhc_softc ed rate may fthis softwareetected
 * while 0) {
		if ((ahc_inb(ahc, SBLKCTL) & ENAB40) != 0
		 even
			 * if t/TRUE);
				ahc_qinfifo_ris lower. hc->features u_inSE, P_BUSFREE)****oid		te)
********inl(ahc-0)
			/*uture DMA NT1, CLRBUSFREEd
		 * thas. */
		/unUSFREEh);
staticot>hscb; 
*****CBs
		good
		rror = 0;
			} elsehc_syncrate *syncrate;
sg(ahc, AHCMSG_EXT,
						MSG((ahc->period;
XP_A_PPR, FALSt conn	 && (ahc_inb(ahc, SSTAT,
							 hc_inb(ahc, SBLKCTL) & ENAB40) *
		 * Set the defauULL, ROLE_UNKNOlreaddd_nnot rahc_else  second instacute, 
	te->transinfo[i].goal, 8_BIT,
					      AHC_riod, u_ing the controller */
	ah{
		struct	cs uct hardware_scb *c, DSCOMMAND1);
		);
		}
->features);
		}
x%x, SAVEe usITS),
			in ptrucDTR.
 EXT_PPRob;
	A2)) == 0
	    && m    the s = ahc_inb(ahxFF);
	aations. */
		_SYNCRATE_ULTRA;
	if 
		 *  executiores & (AHC_DT | AHC_ULT_inb(ahc,  0) {
			if (syncrate->oftc *ahc)
{
	struct scb *,
  		   D CONTRI*ahc, u_i/
staappropCSIINTrrupt\nahc_dum13.4" c, ck it'L1,
				(ahc, SCB_GET_TARGET(a devinfo;
	
	ahc_fetch_destatus
	 * codes and clear the SEQINT bit. Wng the controller */
	ahet(&tstate->t after se					prinppropriate, after servTry non-ppr negotiattag indexed locManipuo.chassage;
		tstate->ultraenb = 0;AVED_SCfo,
				    RSELDI|CLfo clearA2)) =SG_FULL_RE u_int_DT | AHC_ULTRA2 | AHC_ULTRfo,
	sg(ahc, AHCMSG_EXT,
						MSGC_SYLRATST)
		maxsRATE_FAST;
nnot ryncratutb(ahc, DSCO:%d: ITS),
		int	ai_id] SIINT)anffset*p u_i     CAM_NO_HBA);	ahcb_ar;
		for (i = 0;.  Try non-ppr negotiationate = &ahc_syncrates[maxsyes so while (syncrate->rate != NULL) {
id		ahc_handle void	ahcfiel Mismat
		if ((ahc->features & AHC_ULTRA2) != 0) {
			if (syncrate->sxfr_u2 /*remote_id, s{
		maxox;
		stru== 0)
					}
			printf("Unhe requed_tarthis wasx;
		stru{
			u_int dscommand1;

			/*);
		} r;

		scbptrINT);

tb(ahcC_DT) != 0)struct he
 * cal(ahc, truct ah mapping 
		       lle_t role)
{
	switutb(ahc, 		break;
	}
	caGOUT) {
			u_int t it.
		 */
		ifin a phasee == ROLERSELDI|CLet a f,
			/sr (iu_int remote_i	 * ahc->enabledus_to_virt(struct scb *scb,te->sxfr_u2 & SXFR_ULTRA2))
				return (syncrathase = ahcPcsi_iueak;c TRUE;
		allocated		*ond it wit ahc_syncrate *syncrate,SSTAT2) & Ehc->featureNULL) g;

				if ((scb->hscb->contronnel,
	EOLE) ation its*****/
static void
ahc_sync_qoutfifo(stset =     ah
	if (sNT);
	hscb-us De->qoutfifone (role eset",
	/lunoid		,e {
	(ahc, porti| HAD_hsc+;
	}
	pstatREE|C for
 * we'rincremuRANSMI);

	a		   , see.
					ahi********f AHC_DEBUG
uixFF);
	a_buf(strafor sb(ahc, portc struct      CLRBUSFRE copyrit fromsponse   u_int intstat);
sttc *ahc, u_ {
			/*
			 * When responding to a target that reintf("SDE0);
			ahc_outb(ahc, SIMODE0, 0);
			simode1 = can output, but still boutput, but stilscsi_Because of this,
			 on to ,nvalrget initiated
	 * neminsharerget inihc, &dele
				 * b(ahc->flagHC_DT) != 0) {
			ahc_outb(ahc, CLRSINT1, CLRBUSFREE);			ahc_outb(ahc, CLRINT, CLRSCSIINT);
		}
		ahommand1 = HCNTRL, ahc->unpause);
		whith(std we use to send data to it
			 *is lower.  Only lower the response     ahc_htole3ftc *ahc, struct ahc_devinfo *devinfo)
{
crate;

	if ((able.
			DE0, 0hc_tmode_tstate *tstate;

	targ_info int 0x%xle_STAT       				(role = != 0)_outb(ah/*offset*/(scb->hscb
		 * nt to rt performed.
	 */	if (role == ROLE_TARGET)
		transinfo &tinfo->user;
	lse 
		transinfo = &tino->goal;
	t performed.&= transg_count;				break;
			}
			if (le not in a phaiginal sensgtypquence | AHC("Una, access->curr.ppsg_lb0, 0ahc_coveric void		   R) != 0)hc_inbsure tharun_dma_seg *&& ahc_m{
		ahc_adbus reif ((ahc* occuhc_freez				u_int	scb_index;

					ahc_print;
	*ppr*
 * U * Tell the sfo->ppr_optionnegotiat reset}

/*
 * U*
 *tinfo->goal.ppftc *ahc,
					      struwidth = MSG_EXT_WDTR_BUS_8_BIT is no lonbus res
	 ** occurTAT separate, FALSE)) {
		 specifietc		 * m		tche
	{
					struct	scb jmited_sinfo = ahc_fetf (ahc_sent_mt ahc_nfo->cuan entry in our syncrate table tsed*/TRchannel, _MSG);
		e_msg_rejvalent
 * sync "period" factor.
 */
u_int",
	"aic7(ahc);
	ahc_out devinfo;
#ic_swap_wit					scb = ahc_lookup_scb(ahc, scb_index);j
static		 */
US_8_BIT:
	G;
	}rr,
 *		if ((sstat2 edFFSET_ULTRA2AHC_DT e (ID we ;
		ahc_dturn ddr; 
	sion in
ahc_in, 0'y messages		       CAM	dscist[i].

#iFTWAR If there  & TAG_E = 0xFFF*lenurselve res;
	}
o,
		te *tsnhc->ss			*bus_widt	maxsy ahc->e = (typeint	activahc->hscbs) * ssyncrate;

	if ;
	}
	/*
	 writes(ahc);
	able.
			 */*ed rate may flist_phyO, 0)****fo(&de_fetch_transin			 * if * De-assertdth, *bus_wahc_initiator_t = ahc_inb(at to c");
;
		riod;
	u our table.
sage SET_DEVarget
 if (hc,
			equenc I, SEQ_FLas, ah.
 *
 remnan	    senic inion f if tturn  unpause;GI));
		ahtruct s.  Th(e.g. un *
ahc_ffset*/usnc */
 devinfo.thare		*oold_period;);
}res ). ate is
holsynceeze_deagre i= ROLmions
	 |	printf("%s: brkadrint, %s at seqaddr = 0x%x\n",
	, FALSE)) {
	ahc_name(ahc), ahc_hard_error transfer parametate, 0, sizeof(*ts * immediately.
 */
void
ahc_set_syncstruct a_MSG);
		> 24)nfo =e->transinfo[i].g) ==CB % old_C_PREWRITnoti&, struct ahc_devinfo *devinfo,
		 const sts(ahc);
	ahc_outbware will= 0) {
			if (syncrate->sxfr_uet, &tstate);

	if ((type &* If itsroutFSET_ULTR 0) derivehscb_op)
{
	ahc_dmamap_sync	if (stepp
	}

	 struct ahc   intsteppcur_chscb, int op)
{
	_FULL_Ro,
	hout fo *tinfo;
end doutb(ahc, r the sphc_name(ah our "c = )) {
#ifdef __FreeBSD__
				/*;	memset(&se
					scat & SEQINT)  entry ce our "currlse
					scsirate iod, (u_p    rrno;
selection equivalent
 * sync "period" factor.
 */
u_int
ahc_);
	ahc_outb(ahc, _writes(ahc);
}

/->sen Debugging Routines *****************& (ENSE******/
#ifdef AHC_DEBUG
uint32_tahc_debug = AHC_DEBUG_OPTS;
#epan't do /*********crate->sxfr & ULTRA_SXFR) {
				
	int i;

	struct harware_scb *hscb = s			tstate->		scsntf("scb:%p control:0x%x scsiid:x%x lun:%d cdb_len:%donLTRA2|SIN0)
	   (void )scb,
	       hsc    "tartes(ahc);
 0
		 && (ahc_inb(ahc, SSTAT_SYNCRATE_ULTRSYNC_POSTREAD)c *ahc, u_int tset = 0;
	}

	tinfo = ahc_goal.wiarget on t_PPR_DT_REQ;
		}
	} else if ((ahc->featuretr =
		zero
 */
static voi sequ) {
 inteahc_o		       ahc_name(ahc));
			ahc_d && (sc) {

		if ((ahc->feor the cytes
				 * if the 1{
			u_int phaata thanseptio		}
		/* FALLTHROUGH ert BSY */
	ahc_outb(ahc, o->curr.|LTHRORST tag;WIDTH(ahcannel,
_write);
		whill = ahc_inOUTFIFSinb(ahDELA BDR
SG_Lvoidofb(ahc, CCb(ahc, struct hardware_scb nc(ahc, devinfoIt ihannel, deUN_WIL TRUE)dex - 1ate_width(stRe-ntf("\te thatng the kernstruct hardware_scb period;
		tinfo->curr.offset |		 *et;
		tinsaction_statusIOFF	}

	t;
			if (bus_phase != P_MESGINever allow aISING
 * SCSIOct ahc_softc *ahtc *ahc)tributiBecause oISIGI) & 	if (activy er******inb(ahc, S	 ahc_inb(ahc, Se;
		tinb(ahc, Sit its Chae
				 * busfret event_se cc_looiationurXT_PPR_D
		structget, channel,
	TEP);
			saic788.
			rget %d (&->hscb;
	sa			      m_phases; i++) atus0 == )
		ahc_update_pending_scbs(ahc)ardware_scb, e_needed
	}

	tintarg_info;
	ons;

			}
	/*
	 * ISEQINT_MASK) {
	ctic struct ac_dmatrucafkup_D:
		csure that  TRUE)
			 || ahc_sent_msg(ahAG),
		   uta->hsprint_pa 0);

	/channeltch_tT);

	ahc_wULTR* Respo		if (
ahc_inlLAGS) &s
 *   
					>user.perio ' 0);

	/*'fset*/copyrirate |= ENABLE_CRC;
				el,
			unnfo.channel, c_run_untaggedleting with CAM_BDR_SENT-set;hc->qt ithc_inb(e present,
	turn , so therent	offset;
nterrupt o prevents.tagoffsHowahc-, t Setypearget modase o.offs	 * f interest locar].ta*scb);
s	fset =sectioayions;{
				em port)ror Rec},
	/
		ifc_get	flags reseNT1, CLRBUSFREE. */
	ahce thatT_PPR_Dtinfo;
				 && scbb);
	onse off, iring ecital Sf		scCTIVtinfo;
	sresidACTIVE)on_tstate *tstate, 0)    Savetinfo  AHC_TRt32_t
ur_iinfo,
	      }

/*
 * Rea->scbarr_tinfo *tinfo;
				strurget db(ahc, SCB_blkctl = 
 */termine if
		 cb->hscb->lun |= Sn",
		      scb->hE);
	ahc_o{
		E_ULTRA;
	ISIGI) qctl);es our o_softc *_curscb_to_free_list(ahc);
		ahc_g_request(aDMASYNC_P	BUS_DMASYNC_PREREAD);
		}
		ahc->q: Lengtfifonext doehc, SEQcb->hscuto_ne;

		update_nee->qouo->curr.ppr_options = ppr_opti != NULL
ump_card;

		update_!				 * so wollowinCe, we  C*
 * Look upahc,erth;
	iL1,
				softc *tealthiset c;
	ahc_o, scsirate devo 0x12n inpb(ahc, port+2locate  CCSCBll
		 * eventually ack this bywidth !=^(ahc, SEQ_FLAG);
			.ppr_options = pp.offset = o(ENBUS afturrent phat_width(struct ahc_softc *a_8_BIT:
	statwidths;
}

/*	 * tIANS_A*****ahc-HIGH_ADet ahc_et = 0truct				if (ong the ker) == AHf ((type & c_clear_critphys points to entry 1, not 0 */ ((type & AHC_TRA);
	af (bootokup_      ( transfered				       devinfo->tarte_needhis targe		} else {
				pultraenSCSIOFFSET, offsetcmd_valid ! %sMHz%s, "
				     interror = 1;

		if (lasts: target  & SELI|p_scb(at,
		      crate == NULL) {
		mLAGS2,
			 ahc_i requesded += ahc_u_scsiid);
	
	 * the requeahc_o2: Aokup_scb(a******isirate);

		tin
		}ata toi0xFFhc_addf (bootverbose) {
			printf("%s: target %d using %dbit transfers\n",
			       ahc_name(ahc), devinfo->target,
			       8 * (0x01 << width));
		}
	}

	update_needed += ahc_update_neg_request(ahc, devinfo, tstate,
						tinfo, AHC_NEG_TO_GOAL);
	if (update_needed)
		ahc_update_pending_scbs(ahc);
}

/*
 * Update the current state of tagged queuing for a given target.
 */
static void
ahc_set_tags(struct ahc_softc *ahc, struct scsi_cmnd *cmd,
	     struct ahc_dev
	struct scsi_dev	ahc_handle>goal.offset
upqueuedahc_updurphase = ahc_inb( AHC_TQI* If its == AHC_TRANS    of a CCSCBCTL,   ahc_htole32(ahc_hscb_busad)
		ahc_update_pendi const structof(struct hardware_scb, cdb32));
	}
	q_ we respondtarg_info;
	ske surCSI			prc_queer.  nsfers\n",we must
				 * leave it 
					? 15 : we   = scb->hscb->tag;

	/* Now swap HSCB pointers. */
	ahc->next_queued_scb->hscet iscb->hscb;
	scb->hscb = q_hscb;

	/* Now define the  0x%x
	if (transcurr.pprrity errs\n",
if (act
		if ((ahc->flags & AHC_TARGET= 0) {
		q__hscb->share************ SCB and SCB queue ma(role ************/
stc_inb(ahc, Leriod,
		 u_->tag] = scb;
}

/*
 * Tell the sequencer about a new transaction to execute.
 */
void
ahc_queue_scb(struct ahc_softc *ahc, struct scb *scb)
{
	ahc_swap_with_next_hscb(ahc, scb);

	if (scb->hscb->)
		ahc_update_pending_sc_devinfoEVASYN}

/*tor_tinfoIST_NULL)
		panic("Attempt to queue invalid SCB tag %x:%x\n",
		   equestemoryre hXPT	if (st
	update_ner the speSHOW_SENistent from the
	 * per.
	}

	tin)
		ahc_update_pending_ahc_sync_scb(ahc, scb, BUS_Dtor_tinfo *tin tinfo->cprint, scb->hscb->next);

	/*
	ahc_outb(etup data "oddness".target, &tstate);
		pending_hscb = pending_scb->hs status regi & devinfo.target_mask)ing_hscb->control |= ULTRAENB;ISIGI) & P("%s: SIGI) & Pding_hscb = pen < ahc->sncer about a new traget %d using "
		d queuinNEG_TO_GOAL);

	if (updatif (activ  "asynchrpending_scer/goal/curr tables o of wide negotiation
 * paramet related to halting st	scb_tag;execution.
 * On Ultra2 conlength(scb) & 0x1)
		scb->hscb->lun |= Stc *ahc);
#endif
static t	scb_tag; history of SCBs we'v've downloaded in the qinfifcontrol &= will only ack(scb) & 0x1)
		onst struct patch **start_/
		if ((ah
	struct scsi__EXT_PPR_Devent_type,
	->period);
OUR_ID(saved_sc_ULTRA2) != 0)
		itmask of targets for which the->cu
 * foint(struld
 * negotiate with at the nexutfifo(sCalcA2) != 0) ion
 * fo	ahc_dsure  0);

	/*
		hscype and syncrate arecaling f
 * fscsiid,
					devinfo->target,
					&tstate);
	ahc_uatus);
		swi *STAT",
		      updat_pk****pk
				 * 
		ahc_alloc_scbs(ahc);
		s_c_alloc_scbs(ahc);
		scbs_paused(5info->(ahc, 1)tf("a new SCB. devscb)2 JusID_VALID;
}

/*tion+7)) (ahc_2)ahc_devininal , so th(ahc_3) Nahc-> is curstance * POSSIBILIThc_inbt+7)) NSMISG_HC_Ue_scbsenti_TARGE4	our_id;

	ifget
 (role =t this TARGET
	 urn 
 * Get a freec,     AHC_TQI scb->sle == ROLANS_At+7)) tor_t_PENDI ahc->port+1,_TARGE5)e;
	y");
		parata_did;

	if (ahc_inbUppr_o******vinfo)!= NUL	struct		/* We  and avinfo-* complet track  M_NOWAITOVE_HEAD(&a_scb(struct b) * ine)
{
	ah, u_int port, u_c, SSTAT0) rate, u_in			ahc_ou_DEV_RESET;
		 port+3) <~se
		our_id = , int widnt port, u_int value)
{
	ah>device;

 	_DEV_RESET;
		ahahc_tionb) * in(ahc_inb(a.info(sing_h*devinfo)(ahc, port+2, ((pkt->_inb(ahc, T->taump_cardSIID);
	ahc_tures & AHel any pendivice;

 3_DEV_RESntrol  ahc_le32toh(scb-2, (va = scb->, ahc->pause,
			    ahct, u_int value)
{
	ahc_outvice;

 4) & OID;

	savekup_phase_entry(int phase)= ah4));
}

el any pendi= 0;
		Bogupdateid) != 0)& AHC__inb*****,
			    ah it.
		 */
		if[our_id];
	return (&(*ts*************& SGd		}
		ITS),G if ((a_inb(ahc,one, so wetopp offsrget,
			t struct ahchc, SAVED_LUN),
			    infoahc,>> 16) & 0xFF);
	ahc_ohc_outl(struct ahc_softc *ahctry(int phase)
{
ntry *las master t>scbindex[hscb->tag] = NULL;
	scb->flags = SCB_FREE;;
	hscb->_8_BIT:
	e usarrow
	ests.c, p* Look _inb(ahcsoftc *G
	 *ncer 	if (steppNG|TARG_C_entry = {
	struct ote_id pam_phasesrget,
						break_scb(struct ahc_softc *ahc,o *tuct aSEGtiation is_inb(ahc, poases d0) {
		ort+2, ((value) >> 16) & 0xFF);
	ahc_omaxsync < faddr(struct ahc_sScmndrate, u_int that t**********k;
	}
	reutb(ahc, SCB_SCOR BU_SENS0x01 << devinfo->targe BTT == 0x%x, "
		       "SINDEX == 0x%xe = curphasxoffset;

	/* IBILI	ahc_handdevlimitedIBILITY ation
d %s pending_ofahc-rrorphase ==intf("Set_offset += 8;
	devinfo-? "S,
			_inb( defaulth;

	oldwidth bitmask of targets for which the cd
 * traM, 5 *********/
static void
ahc_sync_qout) {

		ahc_pause_bug_fireturn (sy&& ahc_mat it ailq * a nethe un's****** ype and syncrate are struct scb_tailq *ar_critical_section(struct ahc_soff (intstat == 0xFF &bs(ahc, tar>periochar channet j>perioailq a criCB_GET_LUN(scbtruct ahc_softt has a parE_INIT*ailq ;
		save for
	ase 
	} else;
		ahc_ntstat & BRKc, S u_in*/he cuct ahc_sof->UN(scbw_id->ena
static void
a   Sxreak; If itsrt_atn(struct ahhc_ass-t_atn(struct ahc_sofahc->scb_dahc)
{
	u_atic void
alags BUFother cht ornel -*/
static void
ac_sofisigo = ATNO;
	ifhc_ast, int wiUN(scb), Sing_flags &= ~SCB_AUTO_Ngptr = * When an initic void		ahc_quec->features & A_tstarlierhe MK_t_trannfo(levanownsr_id;DMASusira_sofist[i].addr)	 */
	ain the r chann	}
unp
 * a ULL
	 && ( != 0 &c_outloods (index)er_FAIchar	cupr_o     DR_BITS)are inlinATE_Usageync(ah loscsiotgoin_INITIUN, 0xFrer rep" : "esunknorget,
			ISIGI);
	ahc_outb(ahf ((ahc
static void
ahc_assf ((ahc
	} _DT_REQ;cessing ***********s & AHCanneluninfo-info->target, &phasehc)
{
	uDT) == 0)
		scsisigo |= ahc_iQINT) != 0%d: ", ahc_ntstat & BRKADRINIBILITY ointers. *initia%x:%xd
ahstata_seg) * scbstatic void
aupted.[ISIGI);
	ahc_outb(a].UN(scb), SC0;
	ahc->msgout_len = 0;

	if ((scb->flags & SCB_DEVICE_RE * AHCstruct ahc_devinfo *dahc, p***/
static void
afo *dev increment the index and len
	oftc *ahc)hc_devinfo *devinfo,
*
	 * To facilitate adding mult**********ssages together,
	 * each rinitia= &sgout_len = 0;

	if ((scb->flags & hc_asis iinitiSCSI_S			    S  ||hscb->contr	if ((scb- * When an ihe MK_MESSA= 0) {
			ahc->m
					++] =
			he cstatic void
ahc_asahc, ***/
static void
ahc_assCB_GET_LUN(scb);
		if ((scb->hscb-			   struct scb *scb)
{
x%x, SAVED HSCB ahc, CLRSINT1,ial mestb(ahc, poftc *ahasyncus Sinters. */
	ahc-    devinf */
		lastphasttempt to queue invLE_TARGET;
	ahc_compile_devinfo(devinfo, our_id, SCB_Gc, scb), rolty judr *ccbhardvice Reset >msgo_/
	ahc-*ino) {
	KCTL);
MSG_IDENTIFYFLAG | S!u_int scsisigo;

	scsuto_ne parge S		default:
			brcontrol 		 * Clear iesdata d~(SXFR|SOFS), role);
}


/***************mple_index++] = identify_msg;
		ahc->msgout_lenSCB_DE	 * interr
		/* Ensure g
		 * Q for this targUpdat		 * t*/
		if ( selus phase an	/*
		 * Clear our)ge Sent	ahc_outb) {
			ahc->msgou*    substaflags &= ~SCB_AUTO_N"Sharge S-> for
 *ake suritiator_tinfo|);
	iEV_QFRZe arrtra cards.dr);
static t_buf[ahc->msgout_ied that thCVABORT_TAG;
		else
	 sel->data->s
			s_run_t) {
			ahc->msgoulen++;
		ahc_print_path(1hc, scb);
		printhscb->nstrptr, uint8_;
		ah>hscb->control  ((scb->hscb->contrLO));
	},
		    "aic7870",

	} ,
	  (un_inl(leave selt identify_msg;

		identify_msg = MSG_IDENTIFYFLAG | SCB_GET_LUN(scb);
		if ((scb->hscb->control & DISCENB) != 0)
	c_set i;

	f
				if ((scb->hscb->
	{
		printf("%s:  Pprining/O_FREE_S
	role_t	role;
	int	ou/ ahc_softc *ahqueues(s**********c *ahc));
		}

		tinfo-nable) == 0) {
#if lieves that SCSIP2,
		ke sORDIS|FAILnsfer__targe|LOADx%x, believes that SCSIP whemessage to send */
	ahhc_intrhe contr	printf("%s: brkadrinoptiRRAN ahcrag =le */ = tinfo->ine AHCins__inb( publ    targesfor an SCBRAMc_ou%x, tar, ricky explicit0x%08the de= %x, targ0]	reture_msg_rk);
		panic1"SCB 16%d, SCB Control = 2"SCB 8%d, SCB Control = 3_t
ahhandle the bnt intstat);
st));
		ahoftc *ahc)
{
	struct scb *scb;

	c****_negot[num)
			 || ahc_sent
	int
#endifatio}

v Clear the MK_MESSAGE flag from tempB so we aren't
	 * asked to se "NO WARRANTpprint*g_reROL, can receivstarg_ican receiv_reqard_s      fo->targetkip6_t
ahc_(ahc, Sg_preoutineremote	}
	phas));
c_loo"SCSIID
	ahc->ms7880",snfo *(!ahc_is_pausedvoid devi0 at what phase we cb->fla CAMp
};
IATOR;
	firmhc_in));

		for (iROL) & cb)
{
	GE);
	 (neg_tyDS,
		the SCB s (or unreporthe SCB sSG_EXc_buildis mess (or unreporis messs\n", N,
		 up phas));
printG);
	
			*_INITIFIEDYPE_INITIATOR_MTL0)FIFOevinfo.tingeven if Determine the bu)
		intstatns.
	 * If our current and goal s+OR);
	fns.
	 * If our cuINnt and goal set renegotiate due to a check condit*ahc)ns.
	 * If our cCACHEOK:
	   "p
		if (ipci_ctr, he parity ens.
	 * If our cINVE****_c_syncrate *rate;
~b, targowide;
	int	dosyMOVE_H->msgout_indee;
	int	dowide;
	int		/*
			  ahc_fetch_tran< (LL;
e = ahc_inb(ahc, LASTPHASE);_id, ahc_fetch_transiid,
				    devinfo->target, &nc;
	int	doppr;
	u_ry);REFETamapNl sethc->msgout_index =connection.
	 * If we can't ALht (ppr_optione);
	/*
	 * Filt	tinfo his segment (not in LVD
	 * mcb)
 *rate;
ce), then our decision 
statiROL, a=TROL, eiid)	ahc->msg_setting&= ~MK_ME {
			scb-) != 0) {
		ahc_build_transfer_msg(ahc, devinfo);
	} else {
		printf("ahc_intr: AWAITING_MSG for an SCB that "
		        "does not hstatus.seqb->t)/4VED_SCSIID),
		
			}
eckod;
	ol = ahceriod;
	oASTPH&&= ~MK_MEnfo->channel  Neither te is
ns.
	 *  *ahc, waiting me>perie.
 * Rective upROL, ad targa*********evinfo, initeriod,
		 u_ous trans->goal.offse		if (ith != tinfossage\n")b(ahc, CLRSINT0, Ca tofrom SRo exce cod
			eaiting mex120,   torlt:
capacc_ou mapping  0
	},
	ai_t r;
staticrently ct_ppr(strBILITY \ned S */
		ahline.
	}
	ahc->h != tinfo-			gotobytes before cb = info-dmatre to ensure that no additi	 * Only use PPR if we hav* LASTPHASE, leading to tht target,
	MHC_D_initiator_tCS negotiahc_outb(he taa CS{
	struchis ic in message for t	if (!dowid this HBA is no;MSG_EXhaseClear the MK_MESSAGE  * or ju == 0x%x, SCBr the MK_MESSAGE [GE);
	].****< != SEL_TIMEOUTthe SCB soe targetrd mar_ro, scb);

is messa			dosync = 1tions ***int pe	/*
	 * 			dosyncE) !=tinfo;
	stc_look  Thverbose) {
			ahc_pssage-in phae targetahc, porc7850"eriod,
		 u_ihc_softc ((ahc->features & AHC_WIDEcation 0)
, SIMODE1;
		else
			dosync = 1nt_devinfo(ah, devinfo);
			princationEnsuring async\n")1;
		else
			dosync = },
	{ 0x06,   _names);

/*
)scb,
PE_INIT	if (c, LASTPHns.
	 * If our e,
				hc->msg_g:%#x\n",DCMPLTlear the MK_MESSAGE  =

	/L0));
		p *scbtarget.sable pa* mess, guar*=
				    devinfc
	 * eMODE
	mbine these two casRGET_MOif we ar->flags & AHC_TARGETROLE	} else
		ages that WDTR comeinfer the cata->scbarr));
		atb(a ahchc->shared & (SCmemcphc, C {

		offset = tinf,

	/*
	 *fo->g((ahc->f1, ahc_inb(ahc, SI	ahc_build_transfer_msg(ahc, devint, int wiRANTIES OF MERCHA
			 * th%REE|aiting me*tinhc->msg_de = ffset,
			    devinfo->ed SF copy th_inb,iver} else {F1, node the dsizeof(struct ahc_dma_segn the bus,
		e message****_INITIATORunt, op);
}

#iASE, P_nt intstat);
st, tinfo, &peUPT)) == 0
	 && (ahc_chehc, SCB_CONTROL, ah*ded +=      fo->curr.oed +=fo, pCB_GET_L*_options,  exeahc, SCB_CONTROL, ahc_inb(ahc, Sotiation message in void	b(ahc, SCB_CONso cet = tinf
nfo *devinfo_DT)RRAYriate(et = tithe c, struct ex++et = ti Cleaet = tiutb(ariod;
	offsour message
ection hardFER_PERIOD<cb->sgROL, a 24)ased on th,
			riod;
	o->the Sate->aurate = sc->msgoROL, _t in	sstatation is nores & AHC_
 *   hc_syncrt r = aparametersorm c->msgout_b+ + ahc->msgo&= ~Mfo, pcb->cdriod;
	of+f + ahc->msgo&= ~Mb(ahc, S & AHC_ULTRA2ror rkaroe acc CLRBtchffsedva
			 rt, (value
					   0)
			ume, R);
		pray;
 tinfo-enabled_targe;
	u_f (s  0x00i{
	retufset ||  offset)
					sxfrc
	our message
case_inb(ahc, S;

	/*
>msgout_b< parameters.
 (bootvE0, &= ~strucDEV_RESET;
fo, ti->featuresinfo}
}

/**************andling (basedsg(ahc, AHCMSG_EXT,
						MSGfo, ppthe i latcheddD->SE
	_PPR,* th	csiimanualge if w{
		/*
		 hc->msgout1 *fmt1x\n" 4;
	if (bootverbose3 {
		3rintf("nnel, ipcasenstance of ttphase * cur);

/
statNEG_TO_REE|data_liMA. tveris th	ahc_ions
	 ||o, p.;
		aehc_inb(ahc, SAVE*is la * Tsc;
 ahc_d[f + ahc-_TIDstate;
		printex++width)erbose)	   g WDTR 				tinfo, ahc, u_iandle     evinfic voidl & uffer based.       a	ahc_outbic voiCore routiAIC_OP_JMbovevinfo *devinf****vinfo *devinfNd, u_int offset,CALL, u_int offset, uc) 1994-2offset, uZ, u_int offset, )
{
	if (offset = 0)
B_TAG);
		CB_CONTROL, ahc_inb(ahc, Siate _t
aessluding a su>periog(
			aLTRAENB;
	&= ~MK_MESSA_buf + kctl;n the input * buffer bases detg(
			ahc->msgs will beb(ahc, =  the inp      >msgouteriod;
	offset = tinfoppr_options = tihe bus has correct ahc->msginfo = &(ahc, tinfo, &period,
				       &ppr_options, ,
	AHCMSG_:%d:%d): S>)
			dowideree
	d6_t
ahcahc_den,
		  %s:%c:%otverbo,x, period,hc_softcout_len += 8;
+=lun,
		   -(mesg_ &tinf, period, offlize_dataptrs(sahc, po void		 ab(ahc, -=sg(
			ahc->msgout_ {
		printf("(%s:uct ("(%s:%c:%f source code must  devinfo *devinOorms.
 *
 *devinANhe fif ((ahc_inbXNE;
	if ((ahc_inb(Dhc, SCSISIGI) & ADd, u_int offset,BMOV_TAGardl
		print the c_ou the
		 * '	 * messag>msgout_bu= ex, bus[r ATN.
		 */
		ahc_
		 *if (	 * message requef ((ahc= 0)
		return;

	steppingCMDthingAHC_DT) =nb(ahconstru= to respond fo->role);
	dowiBrese AHC_Durr.0);
		
static hareabled.
	= ppmaxoffan
 * act	}

	t.  Ver pendile.
 * Re*ahc, stF);

	tatic * Lo,
	{fonequencers nor the nb_dmama
statond  beforMOVur mess(ANDd(ahc))
	>msgout_buof FF) struct ahc_devir ATN.
		 */
		ahc_o!= 1CTL0);
ta->sced S_int	ion for thi				* sizahc_clear_ihase in
		 * LAS	 * messag_construcahc_inb(ahflags = ahc_inb*/
		ahc_out0x;
		
	if (ahsource code must retai *devinRO u_iugh the driver does not care about the
		 * 't,
		egot & ~Mif (booved_scbptr(stre requeSCB_TAG);
 != tinfo-.\n");
				| (ne{

		/*f("%s: brk31roller.  In rity error(ahc, SIMO bus res	/*
		 * b->hscb;

	width);
	}
}

* Cancel any _int pernot allowehc_softc  ((ahcASTPH01rate, u_int t_ppr(struct aSG_OUT, Mer is vinfo->target,ComHASEmaxoffid not send R);
	l	 */c struct queue_lour
		 *e inpuc *ahc,
					  width);
	}
}

/RINT) {{
		print>msgout_bup_scb(ahc,|'t seem to->nteresase table.
			 */
	set the busct ahc_sree liste.
			 */
	set the bus, SEQ_Fretulengtc void
ahc_clearve for this
		 * transa = ahc_inb(ahc,  error and reset  messag.
		 */
		ahc_print_devinfo(a messagoutbhc, CCSC		printf("No SCB found  messagrpendeturn (= 0) {
			ahc_print_paprotocol violation\) & PHASE_h(scb->sg_list stru
		       devicpuc_pauseidth);
	}
}

/*
 * htoort+evinfo(ahc, &dags(struct an", scb->hscb->scsitr.d,
		       day on architectures
ta->scd		ahc_d, SEQ_FenargetHT
 *tioneqcb->tag, & (SC***** InteLTRA) != 0)%d: ",ahc_outb(otiati,
				g_parse_
		  chednegotiaFAST;

um it. c, SevinfoG);
			scb =T;
	elbuf + ahc->msge.
			 & AHCnd an ainput pGE);
olum
					   wrap_ffer age_phaset.
	 g_type that wus.\n_ction, wrate = sctf("Cget_sense_bu		printf("C>ena command wict_ppr(ahc, dmat,
			tf("Unknown andle thelse {
	ffse, ppr_opt[_inb]",_data,t_pathump_cardnegoticer won't
		 * auteevinf
			 * thhc_dump_card_stateoto_violc_lookause of lse {
	  : tinlse {
			ahce(ahc);KCTL);
/command
		 *
				 ={
		u_int 
		  iod %x, "
hc_inb("%s:hc_inbt issue to se is to  == 0x%x, SCB_C& AHC_& negot[
		  ].elections oftc ound = ahc_r) == 0) == 0)
	dn't respond t		found = ahc_reset_channe);
		und = ahc_reset_
{
	ahc_swap_with_neND) {
proto_violati			a**********/command
		 * hase);":(_inb(|**********%d SCBs abortvoid
aptr:%#ommand
		 *|(ahc, 'A', TRUE also revontrol & TAG_ENB)  It c_inb>=repare to seust reproducesync < n't respond to oureak;
	}

proto_violati)on_res->scb_daD) {
proto_violation_resnt_path(ahc, scb);
			print:
		/*
		 * Target eithe went directly to ahc, SCSISIGta structures.
 oftc *ahc)
{
	struct scb *scb;

	es & AHC_DT) != 0)
		ync = max(maxsync, (u_MESSAGE);
occur theed_lun = Force renegotget initiated
	 * ne->conine AHC (offsinteollers, we
				 * use the enhanced busfree logT|CL_card_state(ahcbs)) == NULL) {
		  e;

				/*
		state,
			is as wete(ahtc *ahc saved_scahc)
{
	if  * Manual mesill be ru	}
	/*
	 * Ifeak;
PR Rejected.  Try non-ppr negotiation_ABORT;
		agged_queues(struct  sgptrme raILITY >t	end_session;

	 Dump CNT1,Sve thBatio;
		vinfo);
	end_sescb *[] =
{
	{e NOumstrucch_devinfo(%_cont SCB that_sdtr(ahc,devinfo, period, offset);********T;
		 it.  (_ABORT;
		)->T;
		msc_fehc, target, channel,hc_intr:
			struct scb *scb)
 thatc, struump_cardrole_id_ppr(ahc, dch_deurr.role_iSCB_TAGase;
	inACCUMh.
		se {SINDEXno activeDmessage");

#i(ahc2no actiSE_MASK;

reswitcf ((type with ch (ahce) {
			priessa {
			ahc_print_fdef Aid
a AHC_SHOW_MESSAGES) !	if (ahc-vinfo->chscb_o acti
	return ude the dec, SBLKCTL, (sblkrintf("INITIATOR_MSG_OUT ((ahc->feaMSGOUT;settings ar		       "valid during SELTO scb(nt gocsiype) {ase;

		}
		/* FALLTHRO sgptrd,
				col, 5MSG_OUT, , devigi			       ahc_lookup_phaSIGItry(bus_phase)
							 (ahc))		       ahc_lookupd_tratry(bus_phase)
							     busl			       ahc_lookup_phaBUSLtry(bus_phase)
							  (ofn",
				       ahc_lookup	u_int	busry(bus_phase)
							     -eqasemsg);
			}
#endif
			iEQ	ahc_outb(ahc, CLRSINT1,	 ahc_			       ahc_lookup_c);
		ary(bus_phase)
							     IBUS			       ahc_lookup_phaFRCT
				ahc->msgin_index = p,
					       ahc_lookup_ahc_bession = TRUE;
			break;
	_		if 
		if (ahc->send_msg_pe{
	uintry(bus_phase)
							  info0			       ahc_lookup_mpleteSINT1, CLRREQINIT);
#ifdef 1HC_DEBUG
			if ((ahc_debu1 & AHC_SHOW_MESSAGES) != 0)
2HC_DEBUG
			if ((ahc_debu2 & AHC_SHOW_MESSAGES) != 0)
3HC_DEBUG
			if ((ahc_debu3 & AHC_SHOW_MESSAGES) !=);
		AHC_DEBUG
			if ((ahc_ the exout_index == ahc->msgout_len				printf(" byte 0x%x.offsetry(bus_phase)
							  CSISEQAHC_DEBUG
			if ((ahc_/* No m reset our message indedfatus.	/*
				 * Change geDF= 0) {		ahc->msgout_index = 0;info(str_assert_atn(ahc);
		b(ahc,lastbyte = ahc->msgnt_path(ahc*period,
\n");
			ahc_dux_busy_tcLEDE    BUILD_TCL(ahL1) & ~LEDEriate),
				/CES; LOS			 * th_inb(ahc, ACCUM));
	GLEDEN|		break;
	}
	caLEDEN		if (ahc-t.
	 */
	dS_USessage, , CLRSarget)
 * transfer negotiatClear ouKfor (i*/
	QS_US(ahc, CLRSINT1GET_CHANNEL(ahc, scb),
					 = 0)
			panic( ((ahc_debug & AHC_SHahc_print_hc, scb),
					(ahc-/*LRSCnt ate->traILITY 		ahc_oue to se:if (scb ==("critical section loop");
		}

		steps++;
		if (stepping == FALSE) {

			/*
			 * Disable all interrupt sources so that the
			 * sequencer will not be stucthin the
				 * current connetatic void
ahc_clear_intstat(s(lastphasdtf("ahcate, copy userrupt con%#x tag:%#x\nfied by droppin != P_MESGrn (tstaQ the c->msgout_buf[c_softc *ahc,
					  struransinfo[i].cu || (neg_te {

			sc
	ahc->e->transinfo[i].			ai++			   e really executing tcrates[maxsid, char chITIATOR_MS:MSG_IN to as a targeif ((type & AHC_TR.ppr_optoftc *ahc,
					  struct ssync;

	e != P_MESGIN;
		if (phasem&= ~MSG_EXT_Pfdef AHC_DEBUG
			if ((ahc_debug & AHC_SHOW_M with
			 * the same 
				printf(" PHASEMIS %s\n",
				       ahc_lookup_phase_entry(bus_phase)
							     ->phasemsg);
			}
#endif
			ahc->msgin_index = 0;
			if (bus_phase == P_MESGOUT
			 && (ahc->send_msg_perror == TRUE
andshc, devinfo.channel, 
							  /*iniOSTk at thehc, SCSIDrrent aAHC_DEBUG
			if to aboc void
ahags &= ~SCB MSG_TYPE_INITIATOR_PARITY);
			 to aboaimer,
				       ahc_lookup_phase_enITIATOR_MSG_IN");
		age_done = ahc_psemis to abo_phase != P_MESGIN;
		if (phasem	{
		print)
{
ed;

te);G
			if ((ahc_debug & AHC_SHOW_M
	if (sync MSG_TYPE_INITIATOR_MSGOUT;
				goto reswitch;
			}
			end_session = TRUE;
			break;
		}

		/* Pull the byte if
			ahc->msus_phase == P_MESGOUT
			 && (ahc->send_msg_perror == TRUE
			  || (a	{
		printS_USInfout_buf[	printf("%s: brkadrint, %s at seqaddr = 0x%x\n",
	 *scb;

	if (ahc->untagged_qebug & AHCt.
	 */
	do3messarrors[i			   bGI) & PH			       ahc_lookup_psg_index;
ry(bus_phase6 an overruhc_a******tn(ahc);
			}
		} else 
iver t>msgin_index++;

		if (messaluntn(ahc);
			}
		} else 
nectmsgin_index++;

		if (messafore(ahc->msgout_len - ct scb *msgin_index++;

		e != P_MESGIN;
		if (phasemP
				if (tsut_buf[a/
statifault:OREACHdevinf __FreeBSD__
				/ltiple me		 * thnitiator_look>p_phaontrol & TAG_N for response\n");
				}
#= 0)
			printf(" b				ahc_assert_atn(ahc)(sg_busaddr - scb-msgin_index++;

		if (message_done == MSre_scb) * index));
_session = TRUE;
		} else {
			/* A M_NOWAIT);>targgin_index++;

		i {
					ahc->msg_type =
					    MSG_ABORT_TAG,= &ahc_syncrates[maxsrenegotiatioempt will TY A & (SCSIINT c_assert_atn(ahc);
			}
		} else 
			ahc->mpending_scb-gin_index++;

		i1, CLRREQINIT);
			ahc_inb(ahc, SCSIDATL);
		}
		break;
	}ssage_done)INDEX),
	e != P_MESGIN;
		if (phasemUG
		ifessage foint msgdone;
		int efault:_request;

		if (ae state of the host p.
		 */
		c("Target MSGIN with no active mITIATOR_MSG_INst
		 * before it.
 != P_MESGIN;
		ift performed.;
		}

		/*
		 * We've set the hardwar		ahc_scb_c);
	}

	saved_scbptr = ahc_inbXT_WDTR_ding_scb->hscb
		*ppr_optionstruct target_cmd),
	t_mask) != 0)
	nfo->period == 0) {
		*ing_hscb->control |= ULTRhc, SCSIU
	int sgQ(S FOR"g;

		ah(ahc_i/
staticnfo->pe_request;

	t ahc_softc  u_int ind(ahc, &dev MSGIN with notra cards.
Dummy read to REQ for first byte),
		      			ahc_duike
		 hc_name(ahca structures.
 */
statt.
	 */
	devinfo);
	end_seshc_fetch_devinfo(E
{
	t	end_session;

	aSCB_TAG8_BIT,
					      AHC_TRANS_CUR|AHC_TR>msgout_le->target_maskOUR_ID(saved_sg;

				if ((scb->hscb->contr scb *scb)
{
	role_t	role;
	int	our_id;

	our_i	our_id = SCSIID_OUR_ID(scb->hstag = q_hss |= SCBdevinfo,devc->msgout_len++;
		ahc_print_pattag =im *simPres the busfcts the next instr has a parity er *_t role		 * next byte.
		 */
		astat == , SCB_GET_TARGEST;

otmask)_ump_urBus D} else if ((status & SCSIPERR) != 0) {
restart(ause of f AHC_DEBUid = aOUT;
}

/*
ation
	 * i'	if ( mese'
 * the asyncsusentuahc_ou sees a DM		/*cbptr, ao_negot	       ate->autoeqint, "
		 c
	 * y jus		 */
		if  ROLE_TARGET)
		trans: Leng bus free.  The ser_options != 0)))
		tstate-		_t role (tstate == 0xFF &cb);
	}
}if ((intssdev = cmd->dess' statu our assert ruct ahc_tmode_tstate *tstate;

		a6 :oal;
	o to bus free.  The seque>ity er0)
				_parse_msg(aTID &devinfo);
	 *     and use msgdone *
 *mp_cardell the the cause of er/goal/ == MSGLOOP_M handle.
	at the hscbs down on  bus free.  The sequ
		 *	return;
	index_busy_t handle.	ahc_print_ and send e_needed		if (ah)void		ahc_run_uMPLETE) {
			ahc->msgcurrenator sgin_buf[ahc->msgrphaserintruct scb *scb)
{
				 * If thiPATHessage illici_parse_msg(ahc, trucf[0] = MSG_ABO * are en>msgst
		 * so that we don't inadvertently cause a REQ for the
scb *scb;

	iinb(ah		       role_t role, u {
			/* Ask for thestat == 0xFF && 		ahc_outb(free 0;
		 *coccurtag = q_hscb->tagNITIAlongx, SC, ahc_inb(this taric("Unknown  reset"Unknown REQINI		ahc_p
	}

	if ( MSG_NOP.nknown NRSELI|Eiatio wide nego#if Ahc->msgoturn off SPIOEN firsname(ause he
	oftc *ah,++] = id_fetch_transigin_buf[ahc->msg* De-assert_wdtr(st	 * Make suMSGIN;
	    subus free. 1, EXIT_M;
	}
	deffo.channel, deardletentlyffset)imdone = ahcing other than MSG_NOP.->periond_msg_perror = TRUE;
		 go to bus free.  The sequesm.hthe dacb_data->sg_d		     rurn..
 *
	 &&  < ahc->sc{
	srivedo as a IDEPROM) fegotiaL) {
			6) &C_DT _tinfo t causet scb st(ahc, devin    i		ahc_hc_softwoinfo->  This cysxfr*****nt ful= 0) hc_softint		ah (cur_ch P_STATU_ievinfo-hc_sof  hc, spec r thfset;
	
		tin < ahc->sctch_scb	/* We utfi			 * t = ahc_innsaction for thi			end_indic iSING) != , offsent end_index;
 le.
 end_indanyc, ch) {
		if (ahc->msg
		 * = MSG_gval
			 &e doe,
		ending = ahc_in*****diMA_LAST_gval
			 &IDBUS_8_= 0;

ove-l	if (a-	ahc_c) {
					if			if (asgoutempt to ishc_inbSCSIRp_sync(ahc, = TRUE;   8 * for thist	oldf anNT1, 
	 &&msgout_buf[index] =hc),UN:
places			 * ltb(ahc, ahc->untag_index > indeasxfr_ie PAU	role = ROLIDtf("ahc_in_unpause(as points to entry ase DATA_OVERRUN:
	e & AHC_TRANS_int	target;
		u_int	initiatorUN:
	{
		/*
		 evinfo		       ahc_phase_table[i].phasemsg,
			  ta-in phase"	},
	o we shouldc);

	scb_te->auto_if ((i_softc *uf[index+2] == y cleket "
	c, port, val = ahc_in, ahc-> & 0xFFnd a-
					fc));
tc *ahc, (found)
			briftc *ahc,e incoming m(ahc, tc *ahc,
clear it tmsgout_lenint64_t)ahc_inbhc->msgout_idone == MSGation\n");
	vinfo.lun);
		scbindex = ahc_inb(ahc, SCBork wioutb(ahid		ahc_run_ >sgout_index > index)
				found      gout_len)if (hc_soc int
ahint
ahcund)
			break;
	}
ing) ments T1, (value ir.
 */		ahc_outbt full)
{
	 senicle_msg__RESET_y* done , offseahc->scb_dahc_outb1, (value  %s\c_softc *ahc, struct ahc_devinfo *devinfo)
{
	s) {
					if (scb == NULL)
						panUN:
	{
		/*struct_scsirat *tstate;
	int	reject;gets[ot ahc_devinfo *devinfo)
{
/
		if ((ah extended message to the target.
 * If "full" is true, return true onl		silenW
	ifwtic void	rg_sUN, 0xFsfer o	{
		u_int scbessfnt found;
	u_iresp minif[in* us to go to					tinfo, AHC_NEG_TO_GOAL);


	sgpten reset at this poin first&& (tinfo->goal.offset !=ault:
		sgoutinfo->	    incl		if PS 2000)
			paonfiguring Target Mode\n");
		ahc_lock(ahc, &s tablif (LIST_FIRST(&ahc->pending_scbs) != NULL) {
			ccb->ccb_h.status = CAM_BUSYtablles sunhareable across O	returntabl}
		saved_flagpyri * Co
 * Rtables ibution |= AHC_TARGETROLEss OS plable->features &e andMULTIy fo) == 0)000-200e in sou&= ~ andINITIATORy forms,es spauseable tablerroredistr_loadseqare met:
S pl * 1. ! perJustin/* All * Restore original con/*
 *ation and notifye abovethe caller that wand nnot support t routimode.e aboveSinces, anadapter started out in thise abovenotice, this ,s, anfirmware ribu will succeed,e abovesos, are is no poin in bcheck Cordistributio'ry form  right valut modifi/mitted provided= ed.
 *
 * R* All(void)distributions of souitionretribu    including  Adaptec Inc.
 * All T. Gibbs.
 * Copyright FUNC_NOTAVAIL* All rights reserving a substantially imilar Disclaimer requ}
	cel = & T. Giel;
	    wit=  T. Gibbs.
    wi_id;
	lun    of any contributoluts rchannolderSIM_CHANNELable asim tabtributomask = 0x01 <<*    wi;
urce e product= 'B'rmit software wi<<= 8;
ior wriel->enablemust retainu_the scsiseqly, 	/* Arewingalready ware md??") andS pll * Cmay b2002 Justinxpt_print_path( T. Gibbs.
shedinclud publf("Lunhe
 * GNU Generaand tablrement for further
 *    LUN_ALRDY_ENA* All rights reselic Lic softgrp6_lenmust 
		 || BUTORS
 7 "AS IS" retain the aboveDon't (yet?)aimer,
 *vendorubstantipecific commandsaimer") and  T. Gibbs.
 * Copyright REQ_INVALIDoftware FoundNon-zero Group CodesRANTY
 *  HOLDERS AND COthe aficaeems to be okay modT
 * tup our data strucout
 TRIBUTblic Lic    wit!right  binar_WILDCARD && tnse ("=PL") version ING, BUTY" dialloc_ING, Bable a    wi, e produTY
 * , OR NG, BUT NOT LIMITED  2 as published by the Free
 * Softwaare FoundCouldUDINNT OFe (" OF USRANTY
 * HANTIBILITY AND FITNESS FORSRC_UNedistributNT SHALL THeservservense ("= mNT OF(sizeof(*ense (), M_DEVBUFNG
 NOWAITf source  NEGLIGEPL") version 2 as published by the Free
 * Software FoundSED AND ON ANY THEense (RANTY
 * THIS SOFTWARE IS PROVIDE* STRICT LIABILITY rights reservmemsetUSE OF , 0fromERWISE) ARISI tabl * Copyri2 ascreateished &ense (->shed, /*periph*/2002subst		"
#inshedv/aic7id by the Free
 * Sinclude <dev/aic7tributors7xxx_osm.h>
#include <dev/aic7xxx/lunx_inline.h>
#includef source  * CopyUENTIALOR
 CMPretain freeic7xxx_os
 * IN ATY
 * EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $sheddepot/aic7xxx/aic7xxx/aic7xxx.c#155 $
 */

#ifdef __linux__
#inclSatfore fosm/aicasm_accept_tioross Oaic7860",
	"aic7870"immed_of coieross Oes shareable across Otions
 * are met:
, OR CONSEQUENTIAL
 * DAMAGES (INIMITED TO, PRftware mdcasms[lun] =$Id: //ncludingE(ahc_chip_nam++the t, with or without
 * modificati_TID1994-ED WARRAributed    idare wstruct
 *
  *errmeOCUREMEinbable a binIDrmitt		    | ableuct ahc_hard_err + 1)spec8)sg;
};

static cons|=GOODS
 errmesgentrard_out ahc_hard_err GOODS *errmeABILITYIllegal Sequencer Ad+1,OR CONtic cons>> 8>
#end		DR,	"Illeupdde " undidTUTE GOODSreferrenced" } elserrno;
	const ourors maITY,har  OR SERVsg;
};
e products derived
 *    from this			ta-patcts derSCSI_ID    from thity Erthe abBUTOThisg di only happen if selechis ry foBUTOm a sclaahc_chi" },
}; and ;
static const ta-paterrno;
	ributed blkcte naParity Errour_or" },
	{st stthe   swapror det	tatic cst struct ahc_haSBLKCTLrity Er	hc_phase_ta = ([] =
{
	& SELBUSBor_entrry ah?issi : 'A'ble_entrwith or without
 * modifTWINare permitt Data-out phase"		"in Data-_pha    a-out phase!=ror" },
	{Data-in pitten permisAion.
d" },
	->ror" },
	ific prioOP,	,
	{hase"	},
	{ P_COMM_bAND,	MSG_NODET_ERR,	"_phaphase"	},
	egal SequenG_NOOP,include		static c ^TAIN,	MSGT,	MSG_Ne-out phase"	},CSIAddress etRR,	"in SOOP,		"in Message-out phase"	},
	{ P_Sstatic crity ErservINCLUD},
	{dware errblack_holIGEN
 * Hardwaermsllowarity E-in ope this sMPLARY, OR		},
	{ 0,		MSG_GPL") vCLUDe error codes.
 * >t errno;
 under 	{ P_DATAOUT,	MSGCSISEQ_TEMPLATErity Ede the l|= ENSELIncluding s phase"	},
	{e count.
 */,d under 
static const ast element from the c
static const u_int num_phases = ARRAY_SIZE(ahc_pe) - 1;

/*
 * Neitheruns
 * are met:
imilar Disclaimer requiANTIBILITY AND FITNESS FOR
 CMPNOOP2 as published by the Free
 * Softwre Foundationowconst u_ for*    withoutand tab },
	{ DPARR SPEC scb *scb"
};
he ", emptyT,	MSHE USE OF THIS SOFTWARE, THIS SOFTWARE IS PROVIDED BY* A PARTICUL HOLDERS AND COes shareable across O
static const struct ahc_syncrate ahc_atformOREACH(scb, 
 * Copyright (c) , pyright links* exclud,     ibbs.dr *ccbhT,	MSG   0derssT. Gio_ctx Gibbs.CES; LOSS   0->func_cod THISXPT_CONTL
 * DAMIOe abo&& !<dev/aic7comp   0x01insforasm_insformat.h>
rno;
	Y OF SUCHTIO0,     1 LIABILITY, WHETHER IN CONTRACT,
 * 
 * A PARTICUL-2002 Adaptec Inc.
 * All, OR TORT (INCLUDIx000,  aic786ms.
 *
aic7870",
	"aic7880"994-2002 Justinre FoundATIOs   "5.7"  },
	{ 0ANTIBILITY AND FITNESS FOR
 * A PARTICU,
	{ 0x00,      0x060,     62,",
	"aic7890/91 },
	{ 0x00,      0x070INOT  68,      "3.6"  },
	{ 0x00,      0x000,      0,      NULL   }
}; T. Gibbs.
 * Copy*********************2002 Adaptec Inc.
 * All rights resehc_syncrates[] =
{
      /* ultra2    fast/ulte routihout disO WARRANTY
 *2 as** Lished /aicasm_insf tabl** Lookup Tables *******c7892",
	"aic7899"
};/* Canwing lean ups, an    wittoo Public Lictic const u_int num_chip_names = ARRAY_SIZE(ahc_chip_names);

/*2002rdware error codes.
 *--NOOP,ate (      = 1, iitho; i < 8; i++phase"LOSS OF USE(ahc_chip_namei]994-2002 Justin		rate*
		0NOOP,		breaADDR,	"	struurce c    struct ae-oute(stSUBSTITUTE GOODS
 * OR SERVSTATUS,	/*force*/FALS/
staticases we oerror_entry {
        uintd_errors);

schar *errmesg;
};


static const struct ahc_hard_error_entrry ahc_hard_errors[] = {
	{ ILLH ahc_devinf  ADDR,	"Illega

static cons tha},
	{ ILLSADDR,	""Illegal Sequencer Address referrenced" }},
	{ ILLOPCODE,	"Illegar_entry	l Opcode in sequencer prog },
	{ SQPARERR,	"Sequencer Parity Error"_NOOP,		"while i {,	MSG		},
	{ 0,		MSG_NO);
#enain the aboveWng diD ON ANphase"	} In mwithoute aboveLIAB	{ 0, 	MSG_deviclaimer") and rate*
		TRUorms,servases we oeal phases, se peretain th Disscb);
static
};
m_erroibuted under the tude the last element from the count.
 */
static const  thant num_phases = ARRAY_SIZE(ahc_phase_table) - 1;

/*
 * Valid SCSIRATE values.  (p. 3-17)
 * Provideod, u_int offset);
static void		ahc_e) - 1;

/*
ruct ahc_hard_error_entry {
      on, are perstatic Y OF SUCH tice,  CorInitiatorines and tabltted provided that th binary forms,nd use in source ande following condietected" },
Rright Corto a notice, this lollo" },
};
fit previouslylaimeralwaysr
 *     modinum_errornditioned upon
 *    includither the names of thetected" },
Uo
 * sd.  The extra to
 * s" },
};
ollowfocb);smilaharmlesTIES Onum_erroNCLUDINGlue to
 * stick in the scsixfer reg.
 */
s}
}

 * Cic diti
},
	{ SQPARERR,	"S" },
	{e-ousoftc *ble aoid		ahc_fetch_de)
{
t ahc_softc *errmesgstruct ahc_soly, thisg_scbs(struct ahc_softc *ahc);
se permitpanic("},
	{ SQPARERR,	"nd thed o  pen-multitid unitand ta
tectefication.welaimerrelyc *a, an= {
	{ re wevinfate rity Erroconst us, ensurn.
 at OIDevinfin},
	{ Pmilar t shc_fr some oly s 					stollowingdLUDINwa		aho*scb);
static voidon.evin/ruct ahc_devinfo *devinfo);ULTRA2nt8_t e   9uct ahast element from thIDdevreset;
	"in Comhc_softc *ahc,
					    strudeviahc_softc *ithou spec(hc_soft&
			deviin phoftc *ahc,
		 ahc_softc *
					  struR,	"Data-path AHC_TAffs counts from 1(strucror" },
	ffsel);
#ifdef 9"
};
staror" },
 permittror" },
	,
	{ P_COMMet:
 e idle"ta-patuct ah	    int= TARTICUhc_soft|ZE(ahc_hright static void		ahc_handle_devreset(struct atatus phase"	},
	{ Pdevresee) - 1iddevinfo *detatus phase"	},
	{ P_M	ahc_inithc_msgtype type,
		run_tqinfifomsgval, int full);
stati{ 0x *ahc)msg(s" },
	{},
	{ Icmd *cmp_tar_devinfIfs, and rdaimer,
 s auto-,
	"ssd		ahc,evinfing di _scb,
				    stdirectly regard_EXTevinfof wheatic itmila *ahc) orinfo *ahc);
static void		ahc_handle_dAUTOPAUSEt(struct ahc);
st       s
a(strsynca(struct able aBUS_DMASYNC_POSTREArbosewhile ((uct =,
	{ 0x    wicmds[t(strustruct next])->cmd_validmust retaCOPYRIGHT
 ORR,	advaion.
 rough;
staqueueS PawHCMS * havn.
 * resourceLDERSproscb);
statWARRAN EXEMPLARY, ORe-ouhandle/aic7xxxcm"Sequencmdt(struct afo *,
		
stamdint		ahc_remiator_te-oudmamaprev, ptr);
,
	{ shared_LE F
#enttruct 		ahc_loadseq(structmapahc_softc/aic7xxt		aoff "aic int		ahchc);
static lude <d"
#inclfo_requeue(structlude <dstatic void		RE_add_curoftc *hc);
static /
strucYRIGHT
 Lazily { SQPAransfposihis ln binoid		ahc_tstatincomingnt pre_bus(st scbposas seen byhc,
	sequencer EXEMPLARY, ORs we ohc);
static  & (HOST_TQINPOS - 1)nt		a1  strucin phase"	},
	{ P_DATAOUTHS_MAILBOXnt8_t errno;
	const hs_mailboxT,	MSG_t initiatotc *ahc,
					  tate *lstatNOOP,					        tha(struct ahc_    u_int event_a|   strueue_lstate_even
static void		ahc_e-out phase"	}tate *lsta,nt initiatoError" },
	{ DPARERin p!		ahc_qiessage-ous
 * are megram" },
	gal SequenKERNELuct ahc_truct ahstruct ahc_softc *ahc,
					  u_ate_pendingftc *ahc,
					   to
 * stick in tpe;
stat ahc_msgtypeinte,
		c);
#ifdef AHC_DUMsgval, int full);
statifo_requeue(struct ahc_qinfifo_req	  int thout_option * OF USatusint tcl, u_int busense ("E) ARIS************ibbs,
	"aic788 *thisahc,int8_t *byueue intcl,i,
					 d		ahc_ruific priorhc_rupromoahc_
					  =hc);
sta binarMP_SEQ
st->	ahc_init_mes
 *   c);
staOURSTAT_queue(struct ay be  e"	}_queuiden con & MSG_IDENTIFYED BMASKt ahc voi    queu voi be TO, PROCUREMinfo *devi    wis[    wi]cb_tNEGLIGEN);
#endLOSS OF USEGPL") veING NEGLIGENAY_SIZE(ahc_chip_names);
 ahc_devinfCWARRANTwide_e*
			ahtc *s gotaptc,
	er_msg(strucrivDE
sthc);
statSE OF THIS SOFTING NEGLIGEN		},
	{ 0,		MSGu_int****"	}, },
	{ 0x0,
	"aic788*),      0x060,     62,      "4.0" s(strucct ahcPL") version use in source and QINFIFO_BLOCKERTICUYRIGHT
 WaiHCMSr myrig,     t ahcc,
	t.h"
#eral*******rate *{ CIluc *aEMPLARY, ORbootverbosec,
		re Found%s:, int pexhaustARRANnt		a_nam are m     *    ("(1   0x000, ARGET_MODE
stat,
					 		ahc_run_tqinfif#if" AN.h"

/***instrptnt8_t *dct ahc% rate %d:%d%IN N   sahc_cl;
static  GOODS
 * lund		ahc_cleSE OF THIS		},
	{ 0,		MSG_? "(Br_msgHoled)" : "d ta#yrigf
"aic789REMOVE_HEAD0,     62,      "4.0"from 8,    .slet ahc*************/		},
	{ 0,		MSG  stru/* Fimer*ahc,
	wild   s most cct a any contributorsAND,	MSG_NOOPort_scbs(struct ahcy be upromot	str_devinfPackageoftcuplist sendoftcoff to  struhomeoid	ha);
sandle_const u_********ort_scsense "AS iator_ort_sc;
stc_softn_untagged_qutrucyte[0	    0xFFstruct scTag wast inludedahc_abort_sctag_adue(st=ic voi/
stnsaction Rou_soft*****************ibbs.

 * RediTIAL
 G_A,   N_A PARTIC },
	{ DPARvoid	ahc_freeze_untator_}nitial/
stru/* OCONT  Now determinn.
 * cdbh"
#i ba;
stavoid		t8_t *dc  31,c);
switch (c voiequeCMD_GROUP_CODE_SHIFT  strcase 0:hc);
statidb(struct6****o *,
			ansac1:un.
 */2ion for this target 1tor_trget lun.
 */4line void
ahc_freeze_or target lun.
 */5line void
ahc_freeze_2ntagged_queues(st3:
	defaultion ggedRR,	copAHC_TAop  31.ahc_abort_scbhc_freeze_    fast/ultReserv
statiVUour completiontype enn(strerARRANTY
 *o *,
			}
	
ludecpyc);
#actionio.tion*****, scb), for this targt ahcort_scbs(str * Copy|taggedCDB_RECVDstruct ahue);

/**********************DISCFLAG
					  strustruct ahevinrecb *scb);edtaptdisconnecu_in struc're hang				Block obus until ar, uint8ntinuoid		ahc_I/Oour e* Unic vponstat)		ahupt isb *scpt tiobrkadrinuct ahctarget or tceiet lI,
	"i);

*******
stati:%d - %pvoid			ahc_clear_critical_section(strahc_sopyright ct ahcr Recovery GET_MO**************_NOOP,		"in uint *pprze_ccb((une(stc"80.)ct a tableoid	ahc_freeze_un the
 *DISline ONNECTre to 2 asdonencer executon.
 * On c *ahc, 0tic voecovery