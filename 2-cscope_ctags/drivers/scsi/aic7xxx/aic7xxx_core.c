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
	case MSG_DISCONNECT:e routines SAVEDATAPOINTER shareable aCMDCOMPLETErms.
 *
 * CRESTORE platfoSrms.
 *
 * CIGN_WIDE JusIDUc) 1	/*
		 * End our message loop as these arel rightssec Incthe sequencer handles on its own.tion /
		done =ble LOOP_TERMINATED;
		break;shareable aMESSAG2000Jnd bsh	responsthouahc_ce and_msg_reject(ahc, devinfo) * R/* FALLTHROUGHith oareable aNOOPllow widitit
 * modMSGistri (c * R permitted providEXTENDED:
	{s of sWait for enough ofand udistrib to begin validationmust rif  Red->msgin_index < 2)
	 list of c	switch. Res in bina/*
 2 Core condiimum , a_SDTormsfollo	ccond strucimum 
syncrate *isclaimeuce 	u_int	ist iod    ("Dbelow
pr_op. Res and any redoffset and any redsaved_stantied upand buimum a disclaimer1] !ight
ally simi_LEN* andsowin* 1. = TRUhis l reprod    (}
ion.ptec on ag disuntil we have both args beforen.t
 *2. ngeither are acldersclathinditit mos, w	 Ns nor tAddcopyrtor requiremenisclato eaccounbelows nor t witextendedany contdpreambltributorstribtially similar rysclam (ndorse orditimote+ 1)ust  list of c primer")tantiamentlar ny r3]. Re.ist specion = 0terms  including  =bstantiat speca sunder th4 termsbelow
 *rNTY" ddevlimiteddbelow
 *ionsisthe
 , & be di,rn.
 peci  & of the
 * TYt
 *THIS Sf the
 ->roleum ao	Y" dght holeblic Lierms
 *t
 *NObelow
 *, &stantiROVIDED andtarg_scsiy the& c) 2XFERIMPLIED WARHE COPYRIGHT HOLDERhe
 bootverbose furnor printf("(%s:%c:%d:%d): Received "Y
 * LIMI   "menty the") %x,nse ("GP%x\n\tTICULAR PURPOSFilterede pr and LAIMED. IN NO EVE"ROVIDE THE COY" dnamterms) of the
 ->channelE LIABLE FOR ARY, OR CRANTetPLGES (INCLlunNTIAL
 * DAMASPEersion 2 as e , al Public LiNTIAL
 * DAMAWARUDINbstantiMPLIED}LIEDS ANsete Foun 2. Re.
 f the
 , OT
 * LD ANY EXPRSS OF UOVIDEDbstanti,.
 * the
 * G CAUTHEAHC_TRANS_ACTIVE|N CONTRACTGOALHETHER I/*paused*/ermi)re mayors may bSee ifof tinitiaersiSync NegoNG

 * s noBSTITUndidn'tTITUabto fall downeditibelovedt
 * ransfers specifi may or wri_s prometion.
 AHCndorse ,endorse or pr,permi)MERCHANTwinge star * IitOF SUCH LIMIal Public Lic!eD. IN NMERCHANT
 */

nt too ve c-elowceISED Onux__
#iaic7 and permisY
 * PROFI} else#155 $m_inIAL
 THERm_inAllIF AE AREinistrlyxxx/aicm/aicaPublUDINIES OF xxx/ai&&ITED TO,ER I I == ROLE_INITIATORublie "aiIBILITY AND FITNESS DAMTDING,R SHALLL THE COPISI OUT* INDTRS BNTIAL
, PROCUREMECIAL, EXEM BUT NOT LIONSEQUENTIAL
****DAMAGES (INCLU***** BUT NOT LIMITMPLIEDse7xxxITS;a disoutrmissioNU Gener",
	"aic7855"len860",9	"aic78_e "NO OF _sdtrlatfoRUPTION)
OVIDED  LOS>
#inUSE, s OS (INC	"aic7860",
	"aic7860",",
	"c Lisubstanat.h"
#ele
#incopyistrim_insfonotice,  of  lstributione
#iubstantiquireWed undeto thany r bus_width and any rE GOODSons
hard_error_enendint:
 plyGLIGENCerrno;
he "N =sourSc_chipdware eted under thby thendorse oint8sclailude "aic7nsformat.h"
#elsBILITY, Wght h3.ors may bSE OFamd bif ILLSabsm.hargd hip_names hoes
  THE USE OFDDR, {
	{ Iof anyatict specifsIGENCbe NCLUdes.e *
 * Al      uiADVISducts derA PA {
	{ Ifromahc_chsoftws liopyrighLITY OF SUCH DAMAGEttenist ofss * "AS IS" Al SQPARERRly(ahc_chPARERR,	"mayuct 
    PL") version 2 as eublishntry {
 and =RRANIERRST    uTS; DICT LIIB
     * "AS IS" AN R,	"CIOBU"NO ons
 hv/aic7xxx/aicOT
 * LIMIline.h>
#inMERCHine.if

/*****************ch or PARint8******  THE COP%x f THE  {
	HOO*************************/
static const char *const ah_chip_names[] = {
	"NONE",
	"aic7770/aic7xxPROCUR},
	{ CIOPA,RR,	"CIOBU896/97PRSUCHhip_name"AS IS" $Id: //depotxxx/7ic7xxx/7xint8IATOR_.c<dev/aic7OR_DET_EDoBY T *er a_phasebackARE,thPublie *rs);
OF Lsince"Illasked firllegal nd pI  "DatCIOPARwx_inhig
	{ ILan SeqOP,		"inre inst,  {
	{ Iie ab	MSG_aic7xxx_inh>
#ene-out >MSG_I DT Da"	},
	{  {
	{ ILLHADDR,	"Ilndif

/*
	{ P_BUSFREE,	M_insfored %dBi
	{ P_B Lookup Ta* POSStruc  R{
	{ ing...
	{ P_BUSFREE,	MS
 * In m/
staticaticst chdef we onls = ARip_{ ILL[] =ore "NONE",
	"aic7770o itterate ove8 * (0x01 <<N,	MSG_PARI896/97"{ PCIERRSTAT,899"
};c7850e"	},
<devITIATOR_DET_ERxx_osm.h>
#iMSG_NY_SIZE(ahc_phase_table)inline1;

/* ARRAY_SIZE(ahc_phase_tasm of tra_inssionat1;

/errn"	},
	{ P_BUSFREE,	MStick in the 		},
	{ 0,bnd btick int8 phase"	}
};

/*
 * In most cases we only wish to itterate over real phases, so
 * exclude ",
	"aic789te */
	{ 0x42",
	"aic7899"
};c7860"6te */
	{ 0xate */
	{ 0x8te */
	{w7892",
	"aic7895CN,	MSG_PARIta-ou40.0" },
	{240.0" },
	{9"
};t cases we onl"Disc r *errmesg;{ 0x
sx100,    num_NY W may beftr thge-oata-patmo,_MESGrh>
#enc0x04t 0x110,someRRAY_ces d P_COMMem		"ihono	{ Pis porT
#inx/aiUSOPARwitITY .  Fe"	},
 renWAY OU,        "1x120TE Gof tcompon  (pofTATUu		MSG_NO agreem  (peve   25,  iTE G  goal ise "aic.  By upeferre     ERRSTATHE USE 0x11 ARRARRAYwith   om t   "},
	{void 0x110,,          ng_erro
    LITY OF SUCHET_E43,  e_ne;
	cnsfor5",
	"aic7895C tstUSED itterate ove IS" ANRICT EG_ALWAYSMPLIEDTS; OR B{ 0x05,    5.0"   0x04	{ 0x1R SERVICES; LRICT LIABI,ATORSTRICaic7Aif

/* (INCTTHE COTs[] = {
	"N NEGL
 * Prov2,      "20.0"t cases <de {
	{ ILt casesITY_-in MSG_N"	.0" W5,  ll alwayslegal a * ProvtoOMMAN	MSGStatus****      0x000,      9,      "80.0" },
	{ 0x03,      0x000,build_		MSG_NOATAIN_DT,	BILITY, Ws o7     11,      "33.0" },
	{ 0x05,      0x100,    6,   e over re = ors)Y_SIZE Red scsi_id, c);	},
UT,	MardRR,	"ePPr codes.
 880",OSS OF UY"he nerror_enbelow {
	{ I any re DISCLAhe nanh"
#eling a subonATORR,	"CIOBUS Parint8_,	"Illeg {
	{ It scsi_i},
	{ CIOPAR   uint8_rde "aic7xxx__isclaime(WARRAN IABILITY, WHm Hardware et ahc_hard_error_entry ahcPP   uint8eal phase{ ILLHADDR,	"Illegal Host Access"0x000, ILLSptions,
						rEVENisersi Address referrenced" },
	{ ILLOPCODE,	"Illegal Opcode in sequencer program" },
	{mer"),
R,	"Sequencer Parity Error" },
	{ DPARERR,	"Data-path Parity Error" },
	{ MPARERR,	"Scratch or 			  WARy Parity Error" },
	{uenc"Illegal I Error detected" . IN NOL") ver,	"S 2rved5ublishonst u_int n	"PCI E_harddet6cted;
statice-out ERons,CIOBUS Part ahc_syncrGNUEMEN,    SUBSTI7c *ahc)  0x1   "ccoerrmeN,	MSGC0.0" , aG_PAonDET_ER *"3.6"  Ofact  56,th no casehe
 *      "8et implie  0x030,ITY OF SUCH DAM(hc,
						  & *Redi
	 voi DT_REQ)lara0speci&&hc_build== 9rity Essert_atn859",
de "aihc,
						   s      struct ustde "aic7xxx_om.h"
#inGLIGENCE(INCOTHMask -patan{ PCIERRs"Ill13.4" }up00   "25TE Gnc_conal Opoller.  T0x0    "WARRAN_war 0,    "ic v availablWISE) AR,    50,   5   "8furth				  #ifdfo *tributio;	("Disc *imer"	#ifdeard_errorsnst u_int NU Gity Ehc,
						   s Gec *aityhc_set;
stac_softc *ahc,
					 u_int ,
				har channel);
#ifdUS Pared bye_tstFresyncrSARERR,	"SINESS INTERRconst u_AND ON ANY TH BY TOFTWARE ISTAOUVIDED BHER ID TO, THE IMPLIEDity Error" },UTORSATOR"AS IS"ty Er CAUSED itterate ****OR  Mes ahc_consNULL"	},
	{ P_DAT(ahc_harphase"	},
	{ P_DATAIN_DT,	MSG_INITIATOR_DET_ERPP4,      0x00**************Ih)0x05,  unt ahc_o do,
			s  31hc_coSGsm_insforedruct ahc_(wP_ME (p. 3-17")********ARRAY we'llroto_vioSeque    "10ny contenegotiation(s********{ nt		SGIN,	nes PAR msgva||E GOODS
 * OR - 1;"
 * e met:
 * 1. sthc,
						  !t ahc_soic vo ***ERROons,in Mrightsahc_soft scb *sc899"
};
static void	  sahc,
					 u_int evin	  u_int period, unerc_clear met:s
	AH"
#ude 
 * Provides a mahe
 a mapping of t!anfer pTARGET
					he proper value to
 * stick in the scsixfer reg.
 */
staPP we onlruct_ppr(stisclaimerevinfo,
				eal p
imited_s/* ultra2Errorast/messa aimer") and aimer /
	{	},
id	 struce and_devrese    rucg.
 */
orPAREc(struct ahc_sofvinfo *devintributict ahc_son(struct ahcam_ casus#ifdtus,ly wish rightsn(struct ahhc,
IES OF _level);
#ifdruct   0x000,      9,      "80.0" },
	{ 0x03,      0x000,     10,  pp "4 *scbx000,   90/91",
	"aic7C",
	"aic7edef enum 
					       tec  strualloc_tinfo,
					  evinfrget_msgin(struct*ahc,
AR PUs);

static const struct ahc_phase_table_entry ahc_PPR,   ut R CO SHALL THE COPLDERS OR CONTRIBUTOR,o *devinG_NOOSHALL THE COP\t, THE UT,	MSt_msgin(stLDERS OR COb *scbhc_msgtycbic void				      G_	"in Statn  0x000,tdevinfo);
statics OS		ahc_ham_insform_DET;
stic voiuct ahc_softc *ahc,softc *aOUT_DTahc_ha	"in Struct ahc_devinTUT		ahc_add_curscb_tORY
#inLct ahahc_softc *arget_msgihc_m_ISIN_scbdatc_softc *casesata void		ahc_finie
#inTS;);
tB0xc_sof 50,   eset_curre enum {
	AHC  }20.0"/* Ou  "5.7n sourProgram
#if * Providc7860ble)seqsoft},
	{ Poid		ahcUSI*****5",
	"aic7895C,	MSWEVER CAU "aiy ErONHETHER Iu_int		agram */
#iWHEc7xxTRIB
/* Our Sequencer Program */
#incluORc7xxx_seq.h"

/**t scsi_id, char channel);
#ifdef AHC_TARGET_MODdefaultundeic7xUnkn>
#i" },
	{ DPARERR,b_from_du u_iiation( {
	{ ILLHADDR,	"IHC_TARGET_MODEst of c}bus_dmf);
#ifses v_MODEis stain thBUS_DEV000-Eto twY" d *is litaticle_messagiscl struloadE COCAM_BDR_SENTt ahc_soft"Bus Dt_currR		  entry ahcOP,		"in Da/*.h>
#in
stati*/0tart_ET_Ere;
#if/
sta    copyright
 *   OF S2. Re,is list of conditionsABORT_TAGrms.
 *
 * Cpdablish);
statiCoLEAR_QUE02 A) anst8_ttag,
	AH,
	{ P_BUSmo   31,6.0"s"Illegal H		ahc_fini_scbdat *ahc);
s voERCHANt ahc_sof*ahc,
IATOrptr,ncrate(tag = SCB_LIST_ *ahc,reinitiator_tinfo *,
0]butihc_uhc_soIES,this 	   u u_ininbion.
 eriods int tcl_sync_inabor
				sion.
 *prior = {
	"NONE",
	"aic7har *const a_chip_names[] = de tk);
,c,
					    id		ahc_finilstatEQhc_finEDBstati*scb }ahc_softee typd_vinfo)s[cic bscbpourIES, idublishe
 tick in!C_TARGahc_sofstruct ahc_tT_MO_l****** ues(ststatiCct ahc =4******Br chaSCB lunmanagor prolunublishhe
  *ahc,
		     C_TARGEc_sofqueuuect ahc_  "6_curr62ct ahcc int		ahc8ip_names[] =  value to
 * stick in thecl void		ahc_fini};

/*
 * In mos*arg*/tagc_fini_scb{ P_dct scb_tailq_cmd *ctct ahc,
					   
/*
 ahc,
event_typruct scb *scet_cmd *cutdownarg,
					     }
#*errfODE
static OF S_IO_AOUT:
ddr *ahc);
s {
	{ ILLHADDR,	"the scInte
R,	"Cyncrate*d		a NeitIncSetupic voi_construct_ppr(stdeviationamap_callback_t	ahc_dmama80.0" },
	{ 0x03, 1c bus_dmf N CONAc_fini_stions prohate_tst struct ahc_det_cmd *cY_SIZE(ahc_chix05,      0x100,  arDiscst"Datar_entr *   INtickGdtr(!
static this/* Cleaid		l Setgod		ahout modbufct afifo void		ahc_fi0x03,    st rturnrkadri);
}T_MODE
sProole) u_int prev {
	{ Iahc_run_qet_m****iccb);
state_eventm   0x*    struct ahc_srget_mon.
 _run_untaggeprior  *BILITY, 
{
c_fintickhat"Illc,    b strhERR,msgvh);
shahanntick ou*scberrme* ProvorODE
sVhout moderroedeftick vinfo).b);
hc_mdidet_cutic v0 signalatictz
statheevqid		,ct aefushe sc50,      "n pha/
c_run_unsc_softc;t_msgin ic,
			sahc)_t_ODE
s *ODE
sc_sotc *ahc, ied_quetick in ******;
uint8_t cbrmissidif

s32_lasODE
s;
_moid	unknatic conment evinfo,
)*ahc,
					 _unbisclRR,	"CItc *ahc,
	l},
	{		  ion.
 *_softc *	ahcel,l Puhc,
	feminimc  *
 * oftc *nagement N,	MSG_INITIATOR_stick in the scISING
lizaip_names[] = {
	"NONE&******	ahc/* Mess rbes *id		aryrget_c,
					ahc,
					  struLASTINCL*****nes EXT
} ahc_ssg(voi0x05,    ftc *ahc,.
 ** Bl/*full*/ate*
	d		ahc_fini_scb
 * trdoe
	{ o *devinfcasesini_dex_bun_qoutf Attemp(p. b ahcSCB a SPI-2 stypath P"Illegal Hs);

static const truct ahc_phase_table_eini_t ahc_ed._fini_ THE COPTry
static ,
	{ P_BUSFRE***************/
static const char *const aultra2    fast/ultra  period     rate */
ment ement b,  .hc_devinfo *devinfoe execucurr.		MSGt_bu_trucct a= 2sidusemaphoted. equiowe_tstlocklun, ficavinfo *devinfoftc *ahc,
	static void		ic v_M * trans,
					   ruct_ppr(sttmode
statictec amap_callback_t	ahc_dmamasidual(struODE
ARRAY_Sct ahc_softc *ahccurscb_to		   u_int prev, u_intm#ifdetsiduSE OFexlease_note 8bit x_NOOet_msgatic void		aBUSFREEahc_hafuses	"NONc inline  ahc  U
				_sof THE COPuntag		MSG_NOO*************/
stattick in thc const char *con    fast/ultra  period     rate */
 0x000,    ct ahc_sent_bus(satcst r	ahc_Mc,
	8_BIahc_shbusyidch(struct ahc_softc *ahc,
					constc_dumpseq(struct ahc_softc_softc Nmplehc_qinfRGET_MODE8*scbw
 *  	}
}completionsoftc did
 * Garoleta_tst },man",
	ureq(struct eezelun.unafc voidSCoid		ahcletion tatic __hc, i	ahc_cons"5y an (strget_msgll);uresidual(s ahc_slreadyionelegalcce, acce scbr chacbs(stru
 * tratatir
				,"Data c constcrse od recfo *devinfacquirere tooftc *ahc_f*ahc,
Saticty Einite  50,      "54related    0x000,      9,      80.0" },
	{ 0x03,      0_soft.
 */
static inline void
ahc_releas	nb RedisCCSCBCTL_handle_scto zero, the
 *e co
{
	if ( Red->flags &upt or
B_BTT)asm/0Core  str-c7xxx/ged_queue_lock--;
t un*/
int "aichgg		   ck--;the i
ahcet_msgin hc);
#endif
static int		ahc /c,
					u*/c voiwc_freu_i*/0rget_ms/*int foun, u fqintthe
 * Ghe costop;
#ifdef AHC_DUMP_SEQ
static void		ahc);
	}pexecu**d_que_ockt ahc__softc *ahc, itaUSE) ! & PAronouss Red);
	}andl value to
 * stick)leareda dith**AHC_DUMP_SEtick in th********/
static const char *const ultra2    fast/ultra  period     rate */opped.
 */
(scb->ahc_ * Rruct uencer SIss r_TASK)nt(s0uct ahalc_);
n(voCODE,	r_emas" },
	in a cri = e*
	 *ptec(str	{ Phe syse in sourcan dstati
 * t it 	    _tqinfs.
	 */
	whiio zero, the
 *opped.
 */
int is
 * c is
 * iIllegain S(struct ahc_
Persionhc, uon-nd use I/OsourEapho
 * GC	   st t ahc_softc *ahc, * In mostMODE
sWork arINESIllegahip bugs rrel
 * Ittagsoftc **
	 *io_ctx sequencer ch(sahc,
_NONructiopANTY = ~0x23r _chiRAY_SIZE(ae_bug_fixe SEQCTndle_scsiAsivel%ely.  se in sourtogal OinuADVIStaticmaphotiosu_ineuct ahausing, I/Oor it stre_Once++ding a different,c_inb(ahc, the seqnames[] = {
	"NONd		ahc_fini_scbdata(stru!= 0INCLUe SEQ ==ORDERED/
	whause adint ? "ord	ahc" : "headr ittatic"g, interrupt haltthe aboee#endiasserd recuIfncerr examplBASIC SCSIntro ic vo 0ncertct tif , accete,
 workaridentify strufree CCBrved.
 itght hWtructruct slieo_vi
 *     seln unhand a dght h o5 $
 *sget lun *ahc, _outevinfodeassCT LIOtrutin ct ahc_sob *scb, static ) &eANTY	}
} c *auntil it actually=sg_inn,CSIINT | SE	MSGDDR,onSEQIoop n soid		ahceue_locc,
			s/* mus*/wleds.l it/
	whileroid		ah(stru *ss,
		UATOR_DEIDonstFYFLops to zero,) == _atnn(void handler and
sidufahc);
	}
}un_qnow  scbpos,st tINhis wo    unnd use ally s mappsidu int		ueDIS is
 * cumum a ahc_quench(sisclBTT"GPL"uC_TARGETUSEDIS  stt ahc_*pped_lis_q_softcic voi (sc =ftc *ahc&mum a list_phys pausinntagged_qvinfo)_cmd *	] SCSI TAILQ_INSERT_HEAD(list_phys +RKAD, links.tq_TARGETcsg_b 1, no|"DisclUNTAGGEDeassed		 the trol_uct on.
 BUILD_TCLoop until iteze_unteriod     rates ne			u_c(	retusaddrC_TA atick in the sally sendiusing, interrupxsg_bus sg_list_, acce{
	ientlyValiaticposesce thst scby/
	whbm (inficonvei * Cc****turn (ginterrupts_to_vig_indesearch_) ==).
 */
s*nt32queuon queT | BRKADmsginlin hftc u_innst NEL*/sizeof(oftc->b), op op);
LUNoop )n so****/ftc *ahc,ry frget_msgifer periods in,GET_MO**ahc, REQsglist(strEARCH_tice, thNTRLt ahc->	ahc_fini_scO routretu.0" ignp_naic voi going bat ahc********* Mto zero, the
 erro%x --map->sgd*******OOP,ahc_into* Allinterahc_rce and the ndealsiduhc_pa
				n    rolec,
					hc, ac *ahc,
		uencer ha_MODE
sES, iisab *scn indr)
	access regis		ahc_run_q_cmd *cmdts.0"  state_eventignx_bu proecmd_aticcripos,l_s_list_ void		ahc_fini_scbdatount == scb->hscb32_** Untagg;
s				   tc *arget,hc_fini_scbdata(struct{
	int sgt targec->qoutfifo);
stick in the scUler and T***/
/*XXX Actuquirecheck data dir);
	}
}

/ oic sa disou?structerhaps*ahc a"
#elr_forc  18sp,   bmissioc_dmaptecsacti/et_msg_name_devinfoucEQ_scb,S

	sDPHAns *o *devirejaticomplestatic idirget_misab!= 0DIR_Irrors[]***/
/*
I2_t
ahcstruct ahcdyr critiven'c Redisseetic  approprne voc vospny ciyt_phy0x00ineitiaahc_whgvalRESID == izeof(*sal o ahc->NCLUD) !=c,
	_seg *sc_devinfr criticote_id pa0x06ct awab, acceexp cernt * le) -*****odt*/(unONE"o, accenothnkno dmat,s **->ssub int sforytta->hscir.
o halt in ni
#endifon fs dSequvinfo * ***-hc->sstatic set*ptrzero,POSSIs Support Functions 00-200AL_SGPTR
	sg_(INCL){
	in& SGscb *scb, disab*)&ag&&p_namet scb *scbt sLUNt32_
	sg_UT LEN_ODD Sincee (INC    structns in ntruct ahc{ ILLSITY OF e * Al_id//retiveuct ahnt rot 0ruct_ppr(stISING
 or_ti					 
 mapputin_t POSbuti void		ahc_fini_Request tchaITY OF SUCnsfer negotiops to zero,ma the
stat_cuntil it Trc vo_cnrget_ms)->nnel == '[ructstatic]);
}

usglenffset);
sPul*****endi   31,0.{
	ination(s{
	int n re asyncc_abdor" },
	list ITY tive	int16remturns nport+1) << 8;atic voi s OSCNT******Cnes 	"Illega****** GHT .  actuact ahc_softc *ahc,Tget are the urn p_tmodesoftctes are 0,   an i mappi,quencer  ru ADVI>sg_leeasseareascb_from_Explicicb_dzorma ***deassenegotiation(st);
}

vo&esetot 0Ghe s_MASK Host Accesint16_t
art);
}

void
ahc_optiahc__ieturns npor+GET_M
0x000id
ahc_-16)
void"Illega=****PTR|  void	+2) <ANTY" Rsg		+ b - ahc-turn )ort+1Effset);
static n.
 ue & 0xFFsgct ahco_sofx++;

	rb(ahcS/Gt vals a3-17adsoftweused(ago	"in Sdex = ct_ppr(stsg--rt+2) <c7860"c, str32toh(sg->lent32_)(ahc,    ;
}

voinbc->fegisab)tructap_shsharle&& the sen.
, port+2)  16er m{
	aQCTLak-- arei_inq(struhccructrt);
}

void2, (( valu) the, uvinfo);
stP >> rve Highencestaticid, u_int giste  ((ahcgetsls).
t	ahc_c   |x(ahahc,1(struct  */
ic, port+2)= 1 | Redhc_i& (t16) & 0xFF);
	at cases 
void
ahc_ou(ahc
	return ((a1) <,
					  +3ahc_o24)CIOPAR| (((crat64 -p)
{
c_fini_scbdata ncr7et_cutruc	ait port)
{
	r, (     | ("valu" se = ahnfo *deving++nb(ahc_ tclb(ahc, voidet or tbus| (((uin,
					     id	outoid
ahc_outw(struct r |inb(ahcexampl    | (((uin)
{
	a6)) <<t+5))
	     ,uint16remahc, _;
static toggvoidtstaoddnle);2) <<	uiare reverlengx000,   1sequ* areE,	"Illid-are reverddr)
	_x(ah to haltiruct 	}
}siduensurnterst casesnq(stru,ear_ast(st_rual
 >> 8sub SCB/tl == 'csiiSG_NOOP
/**ahc_dCHindex++;

	retcb, LUNrget_msametrence saaaticansfi^nl it se PASCB/it detestoe anET_MODERey wiscliz8
ahc_uct ponb(as to;
	aer mDDR,o_vi+5, (vaES, basITY OFmissthe ssANTY"ct ahoid		ahc_fini_scbdata r
ahc_inq(st_c vopscbs) uint8_t *)&ahc->tar***/
",
	"aturn ****** _int ta+6, (vlist_phy&(*tst new Sruct /*******t)->transiOSSIB LL_softc *24) &ochc, ue SEQ;
	}
}
tatiscelaneous Sup   | FunANTY" r critical 

/*
 * In mostMODE
sD(valmin (ahetht+7)ahc_r, (value >> 48) &
	return r | a + 3)he cc_inegrt+3s|llocateb_a re->pinghc, uc, u_intsle)2atic 16phys ****ndle_scsiRtruct _STACBrn pourualls.
 1atic 8ist.
 */
vc_inqis listo<< 8;
	return r | ahc_VE(struc_t)ahc_i*/
voivet aruct****************utb(ahc, port, vaplett valux(ahcr thisruost cas port+6)) << scbindu_inquenq(strutargi tcl
/*
 * Return an SCB resourbusyid);ct ahing _synd, we oftc*/
	ifn SCB r void		ahc_f
/*
 * Ret(ahc,ctions *****ahc, postruct hardware_scb 
	      | ((;
	}
}
   | 4AD(&a 3must& 0xFF);
(+GNU ;

	return ((ahc_i>>4_t
ahc_inq(strued(a-QCTL	 ***odifie},
	{rn (at ))
	39BIT{
	ubESles tiator role int8_tdsuencer 4ary  {
	AHry aaahc,
					  struDS" },AND1ctivery fock
 atic oid	MASYNC,=nc_scb(ahc | c_inLDSEc *aet_cmd );
	ahc_out->scd
ahc_p;

	/* Notify th user>> voirt, u_HIGHacate  int "3.6READ|void	OSTWRI_0)
	WRITEQCTLic voiuntuct b, links.swap_stru_le);u_int t pot a resg_busaddr ef A strsidume2hoddetea ntag16ickyt ah	retardl it knows 	u_idvance whi8 HSCB to download, and syncan't  HSCB to download,  * Retunt p   swhich Htruc,
			wn
			, Mo(strus   inc offp(ahc,****  To achiehscb-> inc HSC* scb;

	scet **tb(, portULTRA2 for= sg - &0xFF);
	ahc_outTqueueort+7hen we ata->F);
	a ahc->netb(aonel it * Whec_soilablcaws about, swap HSCB arbiid	ah sags =T_MODEHhc_inq(se *erupts2) <issuDUMP_stat8et_curo th mapp void		ah*/
struct scb *
ahcmd *cman still
((uint8_t *)&ahc->targetcmds[index]) - ahc->qoutfir musticevin****s SCwho,
					   le_targ }l sehe taiile 			uence transaction queues wOD******
statc *ahc, irol*static 
					    un;eg),
		Sel sefINESl, utr =
ort+2) cate the correc**********ahc,
					  s	+ (sizean still
	 * lotic LU (c)LDCARDint porc_outb(aichannel);
#ifdd
ahc_syncon	   agS_DMcb->hscb));
	if ((scb->fL)
			rble) cratmmed* RetnoemoryccbADVIS_softTARGET_r) anduph str* tranri Oncin the  oear_g seq next untagged
 * traUSEDCvoid		ahc_e goi ist)));
}

se"	}
};

/*
 * In most cases c,
					 lai(lu03,   >sha <ct ahcU US PS/
voi++sg - &scb->sg_loints toct scb_etcmds[index]) - ahc->index[scb->hscb-cb->sgtionto execute.
 */
=q_hscb-softct Fuid
ahc_outbrem_wscb(sttruct ahc_*ctionount == Retidy mappingzhys points(scb);
cq_hscb->tag)
	quen sox[scb32_PTR)cer to SLIo execute.
 */
void
ahc_queue+7, (d_data.* Now swaG1urn ((DVe "aicING rowruct Ie_mes_scbs!= 0)
		(stil   1=cer to h| SEQI be ac_DUMP_SE want toa_seg On Uessageco  strucd
ahc, poCU{t  shed an instructithe truct a>lun &= LIwled *sg)wait****AHC_nitelytc *3e >> is = AtpB to do>lun &= LIwill only acnd wlt.
 *ahc_i1ry asglist(st->ueue|= S Retur= q_hscpt****	SEL_TIMEOUag ==hc);
c, u "aic}
};

/*
 * In most casof(struct ahc_dma_setag;
	q_c			/tic voiste, cACstillFunc user */
hout mod q_hscb
atforr->hscb, sizeo <= s);

static outb*dition*/(s, paon******. %dc_ous* Erile  here to ensure that npping fr(q_hscb,NULL)
			return (NULL);
	 that woul  ahc queuecb->hscb));
	if ((scb->
	      | (((uinspauseB in toid	ined(sTAG, not 0 
	 * bint64c->b(ahactiondisteist(st;c_soinclahc, *************ng fReturructue progrof(*s*TE, Eci&ahc-);adwe prmultileasle_******togruct_,c, strachhareable should it64_t)(&ahc, sissio   |l"6.6 * varit ah + osthc_dmaTrt);
}

voim
	 *t_msginFF);
	SE) == 0)
				aeturns non-zero status ihe correct_brka the seqsg_bucb*q_hREWRE);

w(struct (ao;
	ahAUT bef6Oods Etiator lountrua_segst cases (p. 3-tag =*/
	ireleac void	panic("bx(ahctr: AWAITING>qinfifonout modeic vo(seturn _is_((&

/*
 * Return sm.h"[dition]*/
vaused(al(scb YPindex[scINCLINce ad_data./c_clearg),
		int(strsidue_devoutituliz2utiodf(*scb->hscbindex[scb->	 * Ouync_q/xed OncerencEQCTL* Since ruc
int
eaure	ahc_a*/
	akes suES,    |p't wanRbles *Disc
 * tran of  correuint8_t *)&ahc->t a rastil(.0"  *. Gimaph****;
	memcp****nt target

l &ahc)&ahc->tarc_calc	i(ahc= ry o	__statBSD__itioneap_s/0, ahc_synca con, M_TARBUFnt
aNO voidy, sc",!hc, scbdatruct ahTIATOR_: * R*
aheues w )&ahc!\n    SCsle)(****nt
ahc_is_	 * Se_scb(struct ang frN COvoid		et_curc_le3)&ahc ((ahace_t)256uct ahc}
)rncluata.cmemle_messag0.0" ped.
 */
i HSCB teof(ep,   ****ou((scb->fEped.
 */
RWISE_data-ir_sglist(s
ahc_is_pausqueuesaicaied SEDIompleted commwp,
				ahc********opx(ahstatic tc *aht  savTMODE_dmamd_data.ctc *ah
}

HC_TMODEINFIFa);
stata(alue)tag;

cb;
	erio(&legalp*errmeshe cc,
				Wt& 0xruchscbaist(uniu_intbeNFIFd biahc_OSMahc,s_liasm/alegal****/= ****ct scsi_cmdc_= - poslegaldescill onlb(struct alegalhar *co = 'A'len*/1SCB_Cvoid_hc_s'B*q_hscb harip =ct ahcONc_ch it w  supr_ms>t lunFEifob(ah] !=e seT_c_scb(BUGretval |= AHSrn (ac_scb(aretval L)
			rD******S, ahc->t st*
aht_busngoid		ahcm.h"_b<< 48est thB/tranr{
	u		ahc_aoftcs					ING
speerene * Trvan sea
	{ s) & ndlayET_Mahc-g seFF);
	ah.
	leteqcte_scFASTes w
void
r (i, sc;
iMODE
sTell in-corS; i++	ret32) &ahc_outq()
{sync_sglismap_syi1) <hhip_namerget_cmd) ET_MOon.
 ****_cmdrt+3Aiator role tl(sofoffsAH_POSTREAb(struct a_cmd) * 
#iurn va}

tic uint)&ahcly wiuse_bug_fix(ahcesource is * sizeofcIRQMSgistt por it
urn (&
n VLset*hEISA [ahce goin* scb;

	sntr(-, portPCIf the ieashys pun[] = ahc,
					  stru"an RS_DMsterru

/*
 * Ren
 *.
 */
intpauct scsi_is not euseupt is not| Pruct;Catch >lunan ise qiyshc_gurn pstuffstill
	 b18, precb_bu scb *
ahck_cmdcbstill voi>share Aterrupt ittrms.
 *r the0x00g_scbllegalm		 */
	_sglist(stist(snto our in-core comma	      + o
		 */
		rdec Incmpletetag ==ENOMEM	 * SMDSSCB_CDogng_srlen* ahc	terrhe pr}*****nsthc,  executp,
				ahetup}

SE) == 0)
		_cmdc);
		ahc_outb(ahc, KERNE scb-map */
stmdcmpltq/*os & costlyonstc->unpaa****/((uint8_t *)&ahc->targeatic void
ahc_stially sims.
 *p,
				d
		 * intHied  0C_RUN_TQINFIFEL_QINint
ahc_checkRUPTS|AHC_EDal |.turn (retval);
}

/*
 * C*l set_ms at minimum a channec_syna_se**********areab5oingaticshahc,F n(void *af_BLOCc  sudc, portE
sta4ts > 500k_cmdc_unrmwaupt ahmpletthe nt ahc_ip &R SERVICE		 &&;
	ahc_ou Redisma conlowinted_ints = 0;
			if (3L_QINe the& emval |.cminfeCIERRSTAT) != 0)
		ERonst ut luync_c,
			nst u_int PLT;
	urn (tic u_bc->unsolAHCdestroyT;
	++;
		return (0);
	}
	ahROR>qinonst u_int (intstat & CMDCMPLu*ahctre SEQCTLre
	 * co2		ahc->unsoSEQI_int port)
CLRINT the CMDINT*
	 * /_brkareab1:N COse { _a_dmatx1
#******* copyr= SCwalkwap HSutpu******* O5 $
 O 0x2_queist of condit0unde structaticay, duoftc );
	}
	bs writes,
		 * clear the inter_resnt    1wd conish tb *q_hscb harted_ints_valcore
ini		ifs, tn(void *_dition Redisc->scbizeoffofb(ahop);
turneBILIs & SCB_CDB32_PTR) !e_nt val,x */
	ahc->scrom tag to SCB in the*********/**/
static void
	t = scb->hscb->tag;

	/* caqinfjhc_syntfifojt(ahc,jahc->tqinfind usej sourabouPAUSEDIS is
 * ction to  *ill
	 * locac *ahc,
					     (struct ahc_sjublishtill
	 * locate thepport Fu	xptush_d_pathhc->scb_datasequence CMDCMdirectlysesoftc
statvinftionsagain.
		h CMDCM******,le_bd		ahcnt(a EVE",b->hscb));
	if ((scb->ftially siblack_hcb->sg_mC_REMOVAB
	clude mple(c_ha******((
sta & BRKADRIN = opyrMPL_seqint(ahEQINT|, ah.
		0)
			ahc_ct apausentstaurn 
static;

		if ((T;
	)
			rkadrint( *devregiNULL)
		.
 *hc, intstat);
	}
	retL)
			returelse {
		intsOuencer haTFIFO 0x1
#tedle_br++************inish ths SCoffset_oftcbe dict ahc_softc	ah*C_POScmdsahc_sync_t	C_TARt to queue invalid Sfo(ahc)eqint(ahtat)& zeroP)ar_queuturn i scb *tructat oshc->gisa->frtquen	ssesis woll_data_dm still
	 * l/*c_->taeue_lockHSCB to download,, ahSEQet_curr0xFF);
	ahc_outXFRCTL0,ines OUTessagee ab);DSPCIint USessage_to_virtahcin-c_y thRATE;
#ifdy thCONFODE
			/*index++;

	ret)
 *SG_detfifo(s****		>qinfiRequest thSEDI(ahc)c, u_ihe
 rm******hc_setitES, I*
ahc	returncontro ahcj   |t th  9info *dev ah"0);		/"lue t_msgn-)
{
t_msgines. */{ ILLifo(struchc_check******use(so HCNT
#if>qinfa
	 * c_initirt);
}

voi *ahcunt d tag0);		/

	/* dVED_L
ahc pnd SCe*****.  C*ahc a f the    st *not*
}

vnt
athat  Redit ct ahl it TL regi/an i    1i, valuid		ahue_lock-kernvi_inb(aarraystrud		ah()t
ahc_tic uintBear_
	 * most cases.
	 */
	if ((0);		/equene
	 *	sblkctlurn an SCBxfrctl1_a,outb(ahc_T = SEQ,
ommanNSELI|Ewaatfo	*/
int
ah& 0xFF);
ahc_ valuc_outb(a_dmaNo 1;
static [hscb-llsser *cot
ahc_ Iopy thaIATO_datcba DMA corrupt Pteant u_int pndw *ahl = 0wanc *ahc)ocb *surb a resotegriSYNC)
{
	= ahred_data_dms[] = n(void *LA * c& (Et enabl the sequencer ioutb(CHIPIDt+5AD(&=NULL)
AIC777;rrupt ogre!=saddr
	r kno zero, thgal AD);
	iDB'****e dossing + of
	/* Nntr(, accea->hpruperogrTWINurn.tdowsKEDse(s	 * i (AHC_Asaddr
	s Support FunctioBLKCTl = 0 to send */
	ah	ared
	,Bbinafore| SELBUSB	 * Se* clERR,se Pbarrayo execuh(intstat OSTREAD|BUS_DMASYLAGS2nothin the f& ~
	 * ) & FL}IfGS2) & ~S		reMA-core
until it 	ahcr acall progrB in t== 0)L,  a rRSTcb, av ahc_in * Retur	}
	aan upt);
}

voies. */n (0c_inspaus
 etvaelay 1000uST_Ndeprc_paf & CM *ahc,nt po	ahc Et ahmake 		 * OalR BUter itMWI_suffici
 * Re2		BUSc to ps* Allow_t
ahc_inqlean upbit trork)) =n-zero , OATN	 * 00vinfdahc,rrupt_hscb-(o
	 hc, ahc->tc *--uEXPRstathc->scb_data-> ahc_softct 0 ahcACoftc  an cehc_out[hscb-outwly	 * Cod SWARN_RES- Faint
a *ahcqinfi! type,;
	return c->scbo channve
	_REMyway*****infifte*
d, weaticcky.  The card
	 *ter0);
e walk the output	}
	= 0)
s & SCB_DMor prs 
		iPOuser *_curtructo.sle);_syncleared
	a4)DIAGLno
	|SELNG, c,
				No Tw scbreventsPCI trano execute.
 */
void
ahoutb(Disclns ***** the 
		a=     nb(ahIf HC_PCI_
	ahc_h*    areabhe naes &e agesNbINPOS_to_vir[ationist of conditma_sest reg se3ct ahcx
			] != SCB_LIST_|NULL)
NG, is list of condit8b(ah;

e
	 s_to_vir[ng th32bry fof QOUTogramatGS2ons ot *dcons***********truct ahrminree lided dapnt poypeof(sdr)
ingchanhc_reddres(-dc void*/
int
ahReu_int ATE) &  Redin *****}

voint offsL)
			returSTPWo exec1cbs(struwin-cQI****ALIfied al Pu		ahc_in S+ (offscINFIived buocct port)  | i-tat);=substncer'wh* 2.a= 'Btb(abe_lock-syt
ahcoftcuct &&to n	 * infore) !=l it  SCB_LIST_  subo the[tag]lace that SCB	 ahc_insle)d;

	ahc_sync_qoutfifo(ahc, BUSof diag Redis
	 * lcer interrupt.  (scb == NULL) utb(ahc, SEQ_FL) != 0)atice were i they perrnob->lun &= LIa_seg),
	
		a>qoutfifo[* longommtqine saogre%d " * Sinccb ra(ca|DIAGommandeturn LEDO0);		/next++ areet ar butie namovery scbtic tb(amat.hND,		ahc_		ahc_,ystemre-ahc_inq(stndle_ *ahctohc_dmlikimer= al Puom 1>qinfiblkctl s_ntr(if	scb-ice_wCoreex);
		DUMP_SEQlocateT,	Mlahc_umpseq= scb -uunt(oid
ahc_	 * omman
	"a			ahc_r;
	}

	/*
lags ynual(ofnt optu/*
	 * En * whenf (scb == T_MOahc_dmaAl_queethe coturn (retval);
}

/*
 blkcc_sof#ifte our(ahc);
#ifdef AHCt	MAX postruptt ty_index,
			   CBahc-lidcmplt)\nQOUTPOS = % anyASE is
 * clip_nameers for a conic vuntasithis sributionahc_softc!= 0
	e spntahc->fe (hat S= hscb_bFIRST( (scb-) ! sizeof(*s_clearevinst l0USE)zero	int offset;vo{
		ahc_c* Hag_indexypedefstruct hat _t *seg)
		_ounseg******; i <air.
 ites1) <queub0);

	
	_dmat,CTL) void
ahc_so more	T_MODE =*****->ds+1) <hc}
}

/*************c_queu)
			ion  val_pause(ahc);

	/* NEN != 0hc_sohe cizeck_ftc *ahc,A
	t st = 3fica* scb;

	scb = ahc->scLSCBS_EN"in e the t * t;

		/*
		64 No 1BLKCTL););
#ifd		 && on	 * C->maxahc_s
	struct ((y in.
 * 2b->flags & SCB_ACTIVE)USE)hc_sg_virt_toucort,haSC****	amapst>sg_mform_souommanr_tinfotill
	 struaet_curdebuggt, /_bu+1) * Tac_ha0SYNC_ */
	iscb-L_****r  ins ocount(oftc *a_ade in*/
	ch inf *
hc, ERrTAT0)
		_int porflags |= SCB_AC+jit sFNFIFC_REMOVABLE)
{
	/actuall. */
	 ahc->hsc);
	ahc_outb(a, (valueS/* No lobbe
	ahc_ucbind.  T fre tramodifi;

	scb = ahc->scPAGE& i e)) !		}

c_inq(stru_abort_scNET_ERi+ thenystill
	 ue_lock-		   strucot 0 F);
 scb *scb, ARD, ALL_M= NU(scb =	if (int_int pIDnnel, *u    |rn (&UN_WILDCARD, SCB_LIST_NTAGGET_Mequeut to quete_retatic void
ahc_ac_outbsIe_scb,hc_inq(stru_abort_scbc, 32biing tndle_brk*
 * Cllma_seg),
		_intr(sear_res(scb ==H)
		0ree lle a**al |= val_LU (c)e_scb,MPLAt		ac_ruintsHtic voi_allocnegotiae (und, >lun &= L& 0xF}

/*
 G
 *, a    1serv}
#e/
	if (chadevc In/
user *ADDR0, _abort__REM*/
	a (scb == (scb*RLEN_ODD;

TL, 0);
e(ahc), ahc_hard_er*/
stwe'rtatic void
ahc_sTARGET_M	ahc_fetch_de}
}

/*******hc_quG >_scbx);
		i((intstat & INT_PEND) == sync_tc, u)urn p****eded.hc->scb_t 0 	readrint, %s at sure list.  Thann%YNC_ spREAD)er_writtec Inche qinfifo.
	 *sg_mapNPOS,SCSIc);
	}
}

b(ah ahc_b racb->sginfer tha staray+OUT,lyahc_ints hc->f)re has posted int opahc,)c)
{if (scb-
{_ALLOCsblkctl;nto our in-core command cahc)outb(ahcelB to ic, intstatr (i = 0tb(ahckctlpt by cht error.  The seqn-core
	 * cec IncrunnORS
 ahc_inb *son cazeof(struc
	}

	/*
 transaction(acb(ahel t	 * run save_update_resm tountat error. eqruct+ (s
	{ P
{
	intc, ivice_wr**********g our i(scb ==reset
	caseunhip
	 fieldinNobi = spac{
	aund_offQUEUE_REGSsiinargekadrtintsENXIO	/*
& SEQI			mo_lis ==aticDMAe coin Sahc_
 i;

	def_look/
	kirectofapsync_(osp,
	voidihandms arlue)encertributny Erata->

	/SCB leat	ilxt);
	kadrll s = ahc_inuuct  n	casDMAsiint(at thretval Unlgs =w   Cif (chfu	 * _) scbr_fre *ah(",
			afe>sg_mrexxx/_data->f (scb b->fdevin    struve
		 }
	cash/tra
}

/*xFFc);
		 *s= NUAMAX know");
MAXel);FF);
	a(struc'd theCLRINatic++)
		f("%s_gUN_QOUTFIo execute.s writes,
		cfo(a(T)) == 0) have
		 * upahc_Nlign pair/1ved_tag;
	q_/*brefeary*/_REMSPACEcomm.
 *
32BI;

	/* The ses		/*rlow1) <nnfo *dehat ,     , ah_Srev, ORred
	/* ****ez,
				B in tames othet sees the errahc_adf SCBs , f_freeze****}
	q_N_QOecallor * the in kernel s &=SYNC_a <dellocags &=  to d
ahc_sync_	/* queuhat sd
		 RROR);
		/* maxqueuzthe queue until/
voicli prosefor  notir-corefifoiinfifo.
	 *ptecfter wCSIIe wh. Wgo(chamman_exTNP))lookookup_scb(nse).tructat h	{ P_t wt ahegisext++] fb->h ahc;
		/* ansoftc *ared
	!= 0
	m add	 * 0???= 0)
	t sees the*** Int*of tma_seg *sg;
o just = ah_REMOMA-core c
			dma_seg *sg;
			st	ah *ahc, ipauseam
			prahc_so permi
	 routie clienahc,_CMDdificat#ifdefma (valto  int t+)
		LT_softc *a_CIsiint(
	INFI*scb;
  "2			}

dataif (devinfo(a); AHC seescb);
	tend we are t,cb), = ahc_inb(ahc.******.ES, nfo *detruct t min (& SG_RESI
 */
so;
			struct ahc_
	 *t
at  savpAM_Afoftchsinfo *tinfo;
#ifdef AHC_DEBUg seqint 0 SCSI_seubste listdef HECK_CONhe f	iothe queue until thoftcSENSE_FAI sta ahc_transinus.scsi_statusND:
		
							devinfo.targinfo;
#ifdef sees the errstat/
		ahc_freeze_devq(ah			printf("%s: Interor.eqint  SCB_LIST_Ndc in("SCB %d: ff the residua

	/*is one.
			
		/		}
#endif
e unti       scb->hscb->tag);
			}
#en****
 */ly.
		 *hc_pe      scb->hscb->tag);
			}
#endif

t thifo *tinfo;
#ifdefOKiid,	IBILITY %s:Interrupt _infse(scb) == 0)WructSE'd theate *tstate;
			struct ahc_transinfo *tinfo;
#ifdef AHC_DEBUGion,:
t scb+)
		("Sendscb
		ahc_rd		ahc_run_q_path(ah there is ARD, SC}
#ews us*sc scb);
	uct(ahc,are the targ portRANTIlen)	sg->len =  there isid
ahc_re ata-ate	sg->len = nline nel == ' *>len) transaction DEBUGE
		i SCB_A_) << is_pauseHO(ahc, sd
ahc_r the IBILIprintf("tatus);
htoues LUNahc->q<< 5nt  sque_dumCrted Stter 
			le3SCSI_REV_2
			 && SCB_GET_LUN(scb) < 8)
				sc->byte2 = execuautoaddr)ahc->q erro(ahc, c_trans#end
			sc->oatus);
f (channel == 'BDo nothing		tributi.ONSEQUS/Gahc, sate);, 0);
& SCB_S			  p mod	*((uintunk havin theMODE
, sizeo			 * havin(scb = SLIST			&hc_inw(ate);>len8 = &targ_info->curr;
			sg = scb->sg_list;
			sc = (struct scsi_sense *)(&hscb->shared_data.cdb); 
			/*
			 * Save off the residual if there is one.
			 */
			ahc_update_residual(ahc, scb);
#ifdar t_g seGOSCB ntf("SCB %d: c_sofIBILITY S\nQOUTPSws u\nomma	
		 * thisscb);g->c, scbtstatould
lags &ufruct(gtion
			 *(value)es occur.
			 */
			
		i there is one.idual(scb|= An't wan%d\n",
crareabC_DMA_L	caG;

dma'_AUTOSom tbabe of_i ftc *;
#ifdef AHC_DEBUG
			if (ahc_debug & AHdif

			if (aformaBSYt ho&the correext -LEDOan untaggenumsaction & ahc_soft#endif
		d 		 */)n the qi -ahc_pn the qinthe in,
	llc, SEQtd_datahat a_		*0
		if t to queue i		sg-ng thte;
			struct ahc_tranint 0x%dn	 * runns cbindcmd),int o
ahc_check_cmdlue_cmd),recuT_index)tagg messag output fSEQN_ACT********		= ~Sute(ah*** F);
	ahtb(aidancec 
	 */
ruct a:	_senc, scbhave
		 *}
}

/*************cb_index);
		s us to	"Dallate ngct tt errismplen*/
		a_index);er byte that hoRETURN_1ect wihe etanding
	_maifo.
	 */
	aall hainb(aHC_PCIS_CHuntagged transactSCB_ACTIVsoc *at the7****id_
		 * ru;
	a _n_dma*c voidt ahcKro sta(c voidtb(aruct  |=  *scb; the kerset(ser) q_hscb->tag)
r);
stREMOVE);
		areak;
		}
		break;he corr
	case BAD
		ahc_actu	ahc_priqint 0& SCB_GEgethat  user;
		}
	SISEQ,
 CMDCMPLsense.
			 */
			 sequ(scb == tb(ahc,nterrupt.));

		prinv.
			FITNEStc *ahc_in= NUwere iNSEgs & Sset(sSCSIINT) != 0)siinnewable iestatINT)ld c) ==b(ahc, SCSISEQ,
	_c), scareabFF);hc->unsolon */
		ahc_out
			 * phases occu		sc->2"%s:qinf=/***UESTget disco_sg_e, w/* D> 500NR|ENAUTNoftcA			br
			/IBIL] = 0;
	/* FCTIVNCLU[0l ph endocx%x,  (ah->qoutfiARG_1t toW_SENSE) {
	 we walk the SIID	 * ClED_Lcross>unut to );
		pUN),
		       ahc_inb);
	SCB_ACT->unso
			sg->len = ahc_getistribut("SCB %d: == 0x%x  Queues e.
		rrors wiintf(T) !=CCUM));c, scb);
	f (tM));
		pUN),
	line void
a)
{
	acb-> *ahc,);
	c_tmode			sg->len %s: WARNIngth(scb) errors wiCB  (tinfo			      +Do nothi			sBUI there  ahc)BLKCTL);(scb ==ACCUM));ED_SCSIID),
		dexb(ahc, SAVED_LUeconreqc_sg_x%x e mSETc, SAVED_SCSIID),
DEX == 0x%x\n",
 ahc_tmodeis list of condithc_na    1nd us
 * RInc.
sur.  The sequhc, intstat);
	}     tstatksubsebugsse {
		ints}
}

/*************o_he requd_datc_qinfifo_requeue_tail(ahc, scb);
			ahc_outb(ahc,L)
			return e in-cb = SLIST_F c voidunti5hc_c0 SCSIic void
ahc_phytrans, SCSIBUSL	panicc->scb_degaimeessagewf (tiflags &= ~(scb _SENSESEND				sc-ED_SCptec _SCSIID),sk->byte2 >lobberE Flar.  The & SEQINT)BY T				scb->
			mp_naationr,
 *tdatitf("SEQC =t, u_int,
		iThe seq[SEQro s one.
			d] = ahcfo = &dre has posted agn(voiint
ahc_is_pauscore commopriatSn(voi =f the abmer,
 *
		ahRMINATED:
 be****} el- rcb. If bindba_PCIsfer_lSEo,
	inb(a Fixup,
		 Alt    vinfo.c * phastf("%s:%c:%(sg->addr);
			snecting "
	CBt_sen0))
sg->len);

			sc->opc);

		printf(were	       ahcu_int- rrayOUTPoftcDE) &  assert_apermi when a if (a ahc_SCB_dothe lee abata.ahc_nameseahc_ioat & CSIIDnb(a2 u_int	sc->unused[0tf("%s:s: WARNIN  Rejectic->unun message receiv, CLRINT, . an't allow the "tatfrom%x				Rors wiXF	 * This will be aeeues  (c) 2ion deviah;
		break;
ndex_buISING
l	break;
RleaviTL0));= SCB_LRINT,  / (t msgvEGb->tag);
			}
#en(ahc, SEG;

     Ee fo
c *ahmin(printf(",rejbyl     scb->hscb-= 0x%x, SCBd
ahc_r     tfifo(ahc);
#ifd No mx = 0x%x\n",
	ve
	 * _indrget_cmd)			ahcx00,softc *ahc/
/s added nu *ahc, strulse if ((ibuti= SCBo);
		  "SINDEX == 0x%x\nally stops.
	 *ributi*scb->h 8;
	reaQOUTPUTOSENSE_FAI** Untag  "Sc);
}llocata.c
	 && (sctf("SEQCync(DEX == 0x%x\=data(s.cahc_inIBILITTOR_MS = break;
	Iee. 0x%x, SCBPxED_Sil it ic vo	ahc_p re-en****_BLOCnt offseatic  rig		pri s"an aable hc_inw(("%s:end *SAVED__queenc(ahc, struct  cb this HBA ED_LCurevinf_inb(routiissetrle_scs+(uint64,
				     uct PHASEM));
		p;
		re *ahc)
{
	SEND_ncer has encou queues urn ahc_ng the listributinding     ahc_inL)
			ret		devinfo.target
		lahc_inb	 * This wilee list. &e. "
		   		       "td_quemmandViscon= SCB_Lue_am from afied by theTATt_(scb) 1one.

#ifdutary forp_scbQueues ed sotatus.sctrtranah SEND_SENSE);
		  "lastphc) ! for complet, rejbysee	 * HOST_MSGfo_rY_SIfnse codetf("SEQChe corres IMPLIETdevi+lobber SEGr;
		tate of_L=n scsiOOP
	eturn;	{
		/*
		 * The seque)c) !********rt+2) ze the state of_* moMASYNC_ags =cate the c (scb == _utoseUPCE R%x, 
CIERRSTAT)orted_cmbufnt error;
ahc_inc7860"struct abuf,AHC_ Sautosentures
oid
[_seg DMA'be reCB a resct a
set tbufNE) {
	ahc_ware_scb *q_hRE harred
		 Queues qo
 	!=tic intOU_SENSE)		pt clobber an, A ~BIT Id=%d	 ahc_iTHE COPBc, SEQwe gotmay be is%/
		oing back
 nt  scence);
 status
t(aoutfif us(scb
	 * codes and cleaRIMARY
	/* Cid) +STWRtat = aext_hsutate(tatic sync_c) !Gh(scb))ST_SEsaved= 0
ync_ = ""** Untag_DMASYNC_PREREAD);
			u_inrors; iGETR beenhc, SCransf (trib
#ifdefnb(ahDMASYNC_PREREAD);
		DTto bndex
	 *uct scby transi160tioned to bce that S=E);
	returto buan	u_ino
#deST_MSG_LOOP with "
				2tioned e for c(bus_sm/aicasm_insfor
				x*
	 * S((uin_data)"inb(    "invalid ahc_inb(a} eo_virnic vo);

y transitioned to buri%, SCB_SCSCCUM);
 the messah(struct scsGS2,
	it loeared
	ahc_in of);
				ahc_istancenb(ahc, SAVE			scb->sg_;	 * Saxt++1id_mthe SEQINT bettATOR_MSGIN;
			d/ int op
			*
			 = SCB_GET_tributi(ac_outbn kestat b(ahc, Sb_dE queues wsitio;& CMDCMSCSIINTquencer wahc, scb**********core, q_hscb->ssage
	 */
	int i;
	int error	intf(I|ENAUTAVED_SCSn an SCBiurn an SCB reNSE Fft prEC_DMA_L{
seq_nt oltf("q_h)->transise BAD_sequ0xFF);
	ahc_out_outb(anessage to send */
	ah	uencer s2LE_UNKNO ahc_inmustex = 0;cb %d "
	, ah    (ahc-   | Ies w1=
		rbove-lftc *DMA rejbyte = us_phase);
				/*
				 struct ero, the
 * bber an Bcb_to_free
	}

	/*
 *ahc,
					cer intenc_qoTL1, ahc_inONE"S: no actiNULL)  (sco	/*
0;	scb = ahc->scutoseENB_puntahc,?& AHCsyn:ifo_rscb)
c_handl1), ahcectly			 * scb - , SCSIntf("/ */
	upport FunctionITBUCKMINAag thata.uppahc,b(ah    (ahc-(c_hard rig&g_coSPCHK|Sur d

st
						| (sc|ck_cmdcl
		a_b|ENRR) !R|ACTNEGENhc_in(ahc) P_MESGOUTtatic void		}

de				  elding tatic void
a same0		 &.  Acr the bouqinf)|ENIOERahc_in*/
	ahc->qlet y the 1,  "taLTIMOprin SXFSTurr proPd******tatese = 0GOIG >e parity DFON|SPIs Che	NOW ahc_ahc_ct ahc) wAldex = SCB_Der repc_inahc,  bytes
				 * ifb(aheak;ahc_name(ahc), scawa    IINT)ho theXFERLEN_, u_inthc, );

* t SEQuodifime;
		 c
		retu		   sc(ahc, en a comcb(ahc, hts ist(,
		elivb(ah		 * Thed to bST_MSG_LOOef A
		 * {
				int wait;

egal, CLRINT,oftc *c, SSTATageahc_outb(aseqint(ahtl believes that SCS) != 0%s: WARNIN trac, scak;
PEC_DT)ahc_sofnue\n",
		    taptr =thatwiI_REVuct ahc_softc		u(ahc_inb(ahc,  is
			Iwere CDI|MSGI	if (bu_outb(ah(ahcwa the e_outbstat& (CDpleted exabled_******his is
				 * cch5000;
				ata.cse.  Mauct s.  M SEQsuturnc, s000;
				I)
				lEREAD);& (C00;
			dr
	   0x0)
		C_ALLy queuhc, &ah Recoodifd		a_tfifo(ahc);
#ifd16pe,
 *
			s00,  n	his _NEGOTIATE;
	cb(ahcaluec4* is(ahcahc,
_inb(ahb = ahc->scr any pen		if (bus_p     scb-value & 0xc, ahc-ventu	whiwBTTt
			w, po~SCBinbpagn (va********DCARD, SCersetatic inlinescb->l phsc1/
void
ahc_py of INTST_DUMP_tc *ahc);ck eaa hit error;			    ahc_i(ahc,rate		 * fx\n"t HCNT);o a DDR1cmd),
o dele prcb - atfifo(ahc);
#ifd25		 *BUCLOOP w;
			 */
	[iun->le scb *scb, x_bahc_soft_			 */
	GOTIATE;ocol_SYNe'ree);
ile handling th			    must    UNCORandlI_sync_ropriate

		/ONSEQUE(YNC_P/* No TREAD)rintf("%s:MULTI_T_lin		if (bus_ Fa  ahc couitoutbtil i				 = 0)
 (bus_	 &}

/*c(ahc,_run_SBhase =ahc_Te * ttic sIFO_BLOCwR
 *ovtheck_fy 1, Alle seqreakk			gotFF);
	ahahc_e SEQCTze the state of00;
	c_transHSCB to download, 	 ahcb->fl_TYPEle, &lds SEQINrs _int, u_ink;
		}
whes the (the contr
	 * oid
ahcc, ss ***ste(ah, the fihe a
		2*N_WIse);ofand 16OUTPhow uenc 
	}
	ff trun waring ;

		3u_int scbindex  resouif appropstrux = U tartun_inb(tf("SEQEAD);
u		 * this mode, sSHAevenahc_ have* the in void
ahclarge the overrun ) {
					ahc***/_int iint scbindex  */
_inb(ahc, SCB_TAG);
		un, i= SCBct s_s
	 *;
						scb- *
		  SXFRCTL1,
				
		ahhase uahc, scb);
		printf scb *scb;
			u_int scb_inde;
	st_physL)
			rettail(aevinpheck_c;
				date_turnva t * ES
	 * on0xFFG);
io le oveopriathc_i(scb == /* No,nb(ah*****]) - ahc->*****handthat tIllegal pc_fetcsysteN, * Ifo delivSCBCTL, 0);
= SLIST_,
		i	}
#eTt pp, 5ate->autout to del0);		/*hc_alls the 9engtse);
)nnel, u_) & unphase ndex2) {
			for (se).scb(ahc == ahc_phase_3essage to send */
	ah		for (i = 0; i 4, 12, (_count > 0) {
			for (i = 0; i 5, 1ISIGI) VED_SCSISIGd		for (i = 0; i 6[%d] -be urnb(ah%x : L    i %dc_tmode7SYNCn ov	break;
	}
	case DATA_OVERRHS_MAILBOXSXFRCTL1) | BITBUCKET);
(ahc,
UN =*;
			rue, (intstET"te(aht[i].EQADDR1mmand |= 
	}
	pos= ahcvinfo.our_   bus_phase);
				/*
p,
		es ws *ahcf AHC_TAR-> (scb ==REJEC16)
	  tatVED_SCSIID)KERN {
	  scbS
				 *vidually ac****				 *errun, it
		 *",
	 SCSI_Shc_is_OUT,  "ta 0x%x		 * SIID == 0yte is
uct scsi_			 */
	(1);
}

/	se effendi < num_p_tail(aastat &breat3t[i].lrs annd use in souQOFF_CTLSTAsi_swiQ	}
#e25rand we
aratannewap HN

	/*O seq_FLAGSe until ths OStstat & Cf the cprintents ot will worinb(ahc, SEQing thata.chanDel in casei;
}
(ahc)nic("eual if there is one.insaction_stat i++) {
	 * to data phase lategth = %ld.  L0) |(ahc,TCNT|t fiHNhase =o delOUTb(ahc, SXFRhc_isD;
	CBhc);

SSCSI);
	in thtruct aysume,			inck Statuvinfo.ual if there i(structCSIItate( (ahc_p
staticpe =
ODR1)is) &  Status= 0x%hc_inIOLA. 3-en't SAVED_SCSIID)  "
		    EDejbyte =IID),e Hoptio {
	0 f_syn -ke thi	 ahc_iassistaurr;
			sgREAD|BUS_DMASY->scb_0) {
		gal semsg,
t be t ahc->atus(scw%ld. SI S(ahc), NEGOUnabe ct to clobalult r {
	uatahc_msgtyaevinfo);
 user astphaabindex[turnt will w },
wnlo_stwe'von *****a ovid		ahc zero, thva.cdretu_AUTOS B on* tI)
	nse ATNe for complP
T) {
					ahc->msg_type eriods in ica				),
		)dwarance for ccha|sist%x\n"ngth(scb), scb->sI) & PH_Ttati.  Numruct scbetectel, DSCOMM	/*
				 ->   ahcsx1a(ahce sCK ehc_loo(ahc)	},
	{ 
			 * Sa met_cmd),
	 to thLEN_MASK);
			}
		}  |= ST(&ahahc_hp  ahinb(>astrui	if ry ahon PM rest wi	       ah

					}
	ND1);
				 *
			ahlenler instruct ahc_softcst_physL		ifed(ab(ahc), dewe don'
US_EaGOTIATEc_isrtming%d.\nin "}
		"IL we've 	rejbytes);

static info);
			printDow/
		aFO.
	(ahc), dePscb->,...UTO_NEGOnsfoisconngin_iahc_p -hc, scb)ile hanat ax(ahc)nrhase(s)senseoutb(i= ahc_phase_ta16	break;
	}
	case DATA_OVERRoutb(ahc, SXF			stutfif    ae'reaptec Ithe n->tac,
			500meCLRIN!= 0)
		2ry ah 0) {ahchc_uettload aformatscbsing
curr;
		 TL);essagat te, tarC_TMOoutb(port+2) "Bogesett,INT_MASaev | HDMA.
		 SG(struc 0);
aparitcaln",
		failrR
 * PHASEHBAqoutfiN)));
o5 not 0 Warninte.  Ack the byte and
DMA_int40ibutB20)r {
		I) &     ahfo(ahcscbp--RR,	"Dalint(stCB_MISTA_RUN_E_shutdown(void *hc_htole32(RRUPT_MODE	printentsboardto tqueuse.  ');
	_inli clob

static void voidHADDR,rhc->f*/
void
ahc_Cf the maxc_free routi tha		 * If we've _list[i].lnger u inten = Sany redutuD), aSCtf("SCSIIDtic x%x, SCBSIID,CB_Mtly rahc_nac_hardUSEDIS is
 * _2
	isconn),
		0] =       siy_tcct aUt);
RSXFRCTL1) | Br(structahc)
->sgnb(ahc,x\n, Acallerejbfun_untaggedP out_SRAM
000;
l, dSow
 ch Ram:t,
		tfifo(ahc)x2);
#ifd0x5fnb(ahc, SAV 			a(i %breahscb->);

itargetc_inb(ahe we  ("\nUSE) == 0)
			 * (SC);

SAVED_SCSIvinf    scb->	  structruct ah_transi}e routis OS_OV
	"aOREahc_lngLASTPAVED_S hardw(ahcd		a there7b(ahc, SCBP)ET);
	a_SCSIID),
				     rintf("SE))x, SCBD_SCSIID),
				     SINDEX	}
	casD_SCSIID),
				     x%x, SCB. "
		  t will inb(ahc, SCINDEX			hscb->sed( 0x0infievrywithsahc_inb& SCnb(ahc* gen != NSE F) |cb *
ah"Haesidnd1 = ahc_inb(acc *aahc->fPARFRCTLntstat & Cf the SCSIID),
		intst   aN CONs po_CMBLKCTL)lly
5(ahcdle_proAssumic in_intpe);
		pESSING(ahcITY 
		   outb(as a bSCB_TA4	}
			}ware_scb *b = ahc->scuct EFAULT    MSG_TYP			 * the d;

		scbopriate
 = 7 ahc_rupt is		ahc_outb(ahcowt, /******p_taem_outo is
ahc_check_cm"%s: WARNING detected %s.");
		panic(Otb(ahID =>qinfifonede SCB_LIST_i == is & AHCtb(a#ifdefte *tstate;
			}
		,     T_M<int ppegy
0xc_delamdcmpltq
			sg-ate *tstatC_PREREADt+2)ust _intnsact
				   ntnce for compd
			 * having  info); from out("%s: follurvieared
		 	c, ahc_estinguishable from outstanding
			 * tagged transactions.
l be&
			sc->o->I)
	(scb))  progrd instancED_SCSI =OUT, MSG_NOg->addr);
)(& {
		KERNEL_QINet * sizeofverrun ork w	?t to queue i)0x7Ftr = & AE;
	uts o:), ah->shared_data.cdb); 
			/*
			 * Save off the residual if there is one.
			 */
			ahc_update_residual(ahc, scb);
#ifdtf("Seize7xxx			strucn, i,
		andle_ign			ahc_print_path(ahc, s   "Las=b);
				printf("Sending 
			
		 LIABFER&&bs)) ==0okup_scb(
			sc->len);

rnel NOWannel */
		 i/
		age phase(s)wtarget,
			already have
		 * , SAVED_SCSBUCKISI_STATUS_ AHC_DEB	 ahc_	devinfo.channeB_GET_LUync_tyomma*/
	iastialB/tryste (vaary hc_queue_lobbnto tIFO_BLOC		 * b** Eisconnete(ah void	nb(ah:untixt])) FF);
.0" ));
		pr				scb-> == 0x%x,0x%x\| BITBUCKc_inhc_inb(hc);
#ifdeg_list_ph) {
			  The sen>sg_ma	}
	fget_ctfib(ahel_t)ahs.CCUM))Wu_int lvi  ahcahc, CLRNT b** Ito n=he s);
}

voiL re_phaFA= , /*s Oncation we walk the one(nt toqinfifot ahcr		}
		},
	{ _xt_qrl;

	*/mgval
	chatance faahc_nEX =n, ipdat******Once t>lun &= L		prCBPTRx%x, BThe co*I)) !->tag);
	 off8_cted 
			ahc_set_transaction_s	u_int	scb== 0m_patyn SCSIBUSL)= 'A';met:typvoid
aEQ,
.
 */
int
check_cc_inb(acmdhc_soft+ /*REE|CLRSOdd Bug ahc_na*/ postr_channel;

	if ((ahc->features & AHC_TWIN) != 0
	 && ((ahc_inb(ahc, SBLKCTL) & SELBUSB) != 0))
		cur_channel = 'B';
	else
		cur_chanahc_print_path(ahccdb);), we(ahc, SEQSe abenceahc, Segiste	while  ~SC bin(100);CUM);
		  st u_int register      ahc_ius_drkadrin now_lvd;				ahc_print_path(ahc, scb);
				printf("Sending Sense\n");
			}
#endif
			sg->addr = ahc_get_seninterttc *au_int ier wh(scb))******etected %s."
		 ST(ahc, SCNDEXELTONT) !RSTIAHC_DMA_L	cac_dutly rhannel rejbyte = ahc_inb(ahc, ACCUM);
T, CLRCMDINT);

		/*
		 (sg->addr);
			I|ENRSELI|ENAUTOATrom "
		       "targetsure that the chip sees 	sc->equeueon bce andd jdata)ikB into tc->unpause)an****_LUN && SCB_GET_LUN(st fr  stru				sc->byte2 = SCB_G, SC	       t wil;
	SEQ_FLAGthat the chip sees in binary			 */
		, devinf < nuwsact ahcontrollesfer_le& (scb->flags },
	{	ahc_a	 * This will be HC_ULTRA2) != 0
	 && (status0 & IOERR) != ncbs(		 &&		ahc-cmd(ahc,c);
	Lwere num40; userntroase andto datapriate mestiontaptr = 00;
		/*IST_R * if[hen we
is handl)
		*/ahc,mnitihc_iindividchannel, /*Initiate R "
		   leared when we
	
<	ahc*/
		scb(el, /*Initiate bOnceng the s) != * thisncer, itfifo(ahc);
#ifdef Aity error
ET);
	ahc	}
	SLIST_Rn as ai_ints Errorsfifo_rscb)
tribuviduallystan	 * tagghar f SCnpause*/ividuall			 * thadetecc_inbues. ervedsos wos a25SING
, duethated
		 ck bhc_inbTYller inIGI)
ne reset channel %c\n",
			CBPTR IBILITurn p	ahceque ge phase(s)_queu;
#ifdefsht[i].lourselindex = 0;
				}
				hscb-; i++) { != 0
				scb->.  Thch byte isalready have
		 *. "
ta->scstill
	 * ahc_run_0x08r do?SISIGI_inbo *tinfounpasd fo:
	/*
p_IDEn
			 * to  ahc_qinfrrno;Data usOR, SCSurn pE;
			yr the sesaved ahc_inb(	ahc_sy_tcl(."
		    C******sistSCSI

	aNULL;
	 ahc* infer t
		 *ause(ahc);
} the inl);
/*
 *  =ed true32_PTR) !t type,hc_softc ail) | BI_TMOscb_data->scbindex[scb{
		printf("%s: Someone       bus_phase);
				/*
				 * Proto_vi ((scb =F>> 4ll	  & (the seaic7896(ahcb,his io.
	 *
		bnb(ah* llowget  ProGI) & aahc);
b(ahc		bre(ahc,tr *erohasehroni****ue)
swap HSClyt scbb(ahc, SCI)
					  	silent;

		lastphruhat SCBf("%s:nt		struct s  Tagancy 

	if (scb-
{ahc);
	,
			     aused immediat
					 
		if (scb == XFR		  e(ahctat)
{
strategy
			, BTTCUM));
		pUN),
		   "b(ahc,x%x, BTT 8)
	MISsome
		ause(ahc);
}statusHN);
	_%u, ahc_;b(ahT_OF between
		 }
#e	ahc_ouit._AUTOSitween
		 ********Disablc_outb(ahcno any r)tag);
			}
#endif

			if (ahc_pe.);
						  c_transinist(c, scb);
		printfy CL    {
	uc, scbrrun ;
0)
			ahca se(ahc,, A Determ.e_protookESSING) uct scb *scb curphpermir Pari b				tected
		lUM));
		p bio->con leftnon-zero , (ahcNSE_FAbu rele  ahc_nd
	 * fn*ut a secoindividually ack each
			}
		ifCSI
	 * o;y strategySCB_Lhc_in actually s;

	mentruct scrrun detected %s."
		       ue\n",
		 CSIID == 
	 *  achec[ahtransist t thedivgiste transfsens00;
				tb(ahc,	u_int	er 0x%x\n",
		    SIIDD)hc, SCelseaimeretected %s."
		  SIFRCTc, SCSISsil pro==soursc->byte2prA);
	;
		prinurn
		
_int"%s:%c:=hc_sTARGET_M;
	/* Cid/* Gra, SCB_ahc,eente
	{
	disIBILITe, targ las_cmdcb(ase prep * AcS:
	 SEQ		ahc_setintstat);M));
		pelsesig, scb
		 * einit 0)
rrHIAGLE tag %dBONTRCVALERRL, 0 * tran******ndODE
 * th"y wi	cu)));
, sc soure to ensure thly.  Oad(ah%s: WARNING ,ahc_nth/* Our Scer PRC packet  (ahcB|hases; i++)d
ahs
				, ccer PrSISIGO, i	}
		ahc_inb(aACK enable		print
			t(ahc,
								   &devinfo,R BITBUCKEf * trase c_phase--wait !=%s    0x0 nsfer negot errors w\tUn~resource is no     _DSBat & ct ahc_soed coutb( bus_we. *	ahc_priext);
b(ahcancy tc *REREAD), q_h	u_inter Pcb_indeat2c_inbnt	DGE				    le[i].mesg;
	c	u_in_int	ahc_rs    2 & c{
		p_outb(E_ERR) != hases; ir thaTevinfo.role co>> 4p				*****aNG, cer Prs.scsi_snse codn",
		 e(a7phaseif
			sg->adbr= MStected tinue\n",
		     GI)) & 0xFassistanb !=lt ahuscb !=>flags & AHC_TARGETROLE) != 0 the disA, thetb(ahc,appc_inb(atphasmtaticpresenttntf("		      bus_phatc *ahc, i
static= ahto n	strIn" p =AVED_"o a i > 7	       ahc the parity error is for
		 * if (s
				inclobbe);
	a
ahc_{ P_STend *  "taidually ack eam par the countmaphoutl(sreable aoftc *ahc, str		else {A, the hard_errors);


		iero, the
 * t	ahc_dahc_outb(ase we t & 0xacrle_t_RUNET_LUNationpt by c IS" ANcore
	 * coe are/*
			 * T					  if ((sstat2 & CRCENDERR);
		prCBriat

				/*b->contr		 * tagg	  nts oe are strat.ut(strucD;
	if (ahc_get_t16MESGOb->control |= 		pLASTPHASf("%ath(aThiswee liscbs(lyahc_outbwe've2wereat2 & CR
				feel = (nce for cstance of tiation(IINT);
		ahcigneset_cahc_softc *asrect char chaIINT);
		ahcssert_atnructOFFSEre tobled_targets);
					_dmaCLhc_inw(st16)&ahc->t"SCBmpletiscb *
ahan SCB lSSIO"%s YNC_Pahc_inb(AM ahc_inb****] = {index)_sof
		ieB_MISMATCH:
	 +is
 * cl->unpau0xFF);

c);
			P befor the value
	o *tinBcurpMATCHrintf("%set
				 *nt force);blkctl rateynchc_syn"SEQADDR(0] = {
	SOF error x0	{ 0x19c7860
	 || shc_iing at;

	offsed
		  taghc_pa/*
				 *E;
			a. di);

			scbtuhecktion(shc_inb(ahc, tf("%sLEDnt	s= 0
COMM4",
	"aic| (on, so ignOSTREA",
	"aic7?INGO8 errNow _ar	crole(ahc, CLR
	"NONE",
stat)T)), devc voidTN if e_brle[i].fo(ahccted
, ahc_inb(ahc, NT1ist_phythe EO|C CLRSEEhc_ilthountrol off just in~outb(ahc,		sg->adBstill
	 * lOS
	 *_SG_it mu
				}) != AHCite pci , 5MHzsaction_hejust tur|LING1ge phBPTR      athrow YNC* No  (e.hc_inbsc SCB %x\n",
						      s->scstate case sg->ad SCSIR
	casei_steDre to ));

		printf("E);
		R) !b(ahc,b->tCBcludnows     "SCf just,re
		 bi) != 0)
,c *ahc,
		   "  b noL1,
					 ascb_index	hc_soft */
	   scbsoutb(ahc, hc, CLRSINT0, CLRSEnfo;		priscbptr);
		sccase		 * The <= 8/*10MHz
		 * td */
	aurn
		& SCB_ACTmsgg(ahcTOatus(%d,fdef AHC_DEBUc,
						   ared bycb< 40)upport Functionsu_int	erro_TARGETf just incsue)
ransaction'St attention
			 * er(ahcING4tribcIMEO|Csfu_inb(ahTO_VIOtil thee our ected		   nt, %    out;n-on, sic voi
		if (scb == NULL* ProXFR

		sc
 * un waig
		rtance of	{ 0x42,fdef AHC_DEBUG
			if ahc, SXFRCdump_ messag	ahc_print =
		lLASTPHASdevis		/*
			 * Cancete*
		ccu ?			       ahc_ncPARTamap_call:			       ahc_ine truct ahc_dfdef AHC_DEBUG
			i		 * The harore any datwe return
		 = SCB_GET_Le list.  fdef AHC_DEBUG
			ifc->ta		hscb->control struct ahc_soft->curr;
			sg = 
		 *
		 actuallyisclai"
		== P_MESGOUTthe CB_CONTRstart(ahc);
	} else b(ahc, RETURN_1, he output f
			r}
			i);
		scb->rotocbus_Once the 4      scb->h>qoutfi.
 *  durbe re scb_index)IINT);
		ahcursively.  Once the 
}

/T_LUN(scb)	if NSEQUE directiIBILI ack vinffo;
#isemaphod recuis err	if (luNEL(a(0);
si);
			aAUTOursively.  Once the count acquire{
	ifLIGENChe abeak;},
	{
				aha hi ahc-oursively.  Once the countct ahc     Son, so ignovinfent phastrat_		if ((ahc->f ahc_inb(ahc,hc, SC			ahd.
 */
int
rs wiNULL) ->flahc_ay, d regitatic void
ahc_= 0x%xTS|AHC_EDcommand compsivelL);
ahc_hanout to delivinfo), SCSISEQ,dhcntrction_s);
	transa;

	/*ENct ahc_so;We);
 *  		 * atf
	 *int	lastph timcurr;
		dle_dev}

/*
 * to clearinghc_ihases; Redist bus |cal_c_handdle_deterniswap H, devt lasLL;

		 mayfect.
		 * = scb -rget lunahc)
{
	uido th 16; i++)
		.
		 * Oo deliveor
	ly c[] = {rt, v sserting  scHC_AU 0x%four ownved_LI|EING SCSIsed, we 		ll  scULL) {
 workly cE);
			/*
inde
#ifdefL(ahcg);
sSIINTf(
	ahc_d Status\t(struct  ahc 8;
	r     ahcrt(ahcahc_outb(abytesextnext);
scb *
ah	}
	/*are flush(ahcessage
	 */
	int i;
	int error;
ntatusRR) != maxL);
ERR) != [] = {  "t == 0x%xnlinr saftetected %s."
		 EQ* ru Gibb"aicwereR	ahc_rt cases we
		 ahc);
		] = {int	scsw_lvd;lue
til it If | (lags = GatioaET" mode
		 *Q) & tr =    1Dr	 * wee can rggerinvITNESS :OT__lis>>	ahcLED out of in5"%s:%c(struct
	ahc-have ascsl)
		 	a(saved_sc 0)
aved_lat.h"
#e scbindex);
		if (scb, vidually ack eachS
			& ~stance SCBP 0x%x

/*c,
					  struust = stag;

queu	 * Thg &L;
	i;
	if the incomis7860"),
		INATMINA queu we saved_luan rg(ahcu_int		ahc_
	AHase == Pg.  Twled			brea	a == 0x%xe_renegort(ahHCMSG_1		ahxFFtr =ontrol |=r(strurcontr     t ppe list.  	   os.
		e fro  ahc_ihc, u_1B,MESGOUT.
 */
vvoid
ahc_free_scb		ta    OSTWRDahc_f (e.a residu			 * weSCSIRATE(0, &devinfo);
			pIart(ab->flscbs		scL);
 TRUE)) {
=on thait == 0) {
		_ABORT, TRUE)) {
ame(ahc));
_TAG)
	lulectionLAS scb_ },
quest||c_inb(ahc  (ahc_sentDR(0x%ATOR,
				ntstIG_PMscb);
			
o.rou_       ahc_inb(ahc, SAVED_aruct scb *scb;
			u_int s * pl for co);
st
#endbrIST_NULL) {
\n",vaet_msgin(strucg;

OUR_ID( saved_lualready hBUSY SomeonSB) != 0ahc_pr void
ahc  scb->hCB_Ting frc_setATIO    "SCegal lastyt to	eng(ahc, ide cleard, we pwbusfree wase_taerrort even ate witc_ruurn;
	_state =ILLOPC(ahc,qinfifo= 0)hase preptec InreeBSD__
				if (0;
				D
		brmarr the user'****sc->uhe sameis BDR
		gama_ssome
	d
ahc_res2) != 0_SCSIID == 0x%xtic voidwayu
	"aic	if (bus_phase != (ahc, scb)
					 ahcOb_inde err>hscb->tCB_Xcommand compCBH);
"

/* + (of. "
		   _inb(ahc, SCBPTR),
	%	ahc_freeze_errupt Processing **** Busyr negtraTIBILI*******/
static void
ahc_sync_qoc, SIMODER (i = sghc_dumpset*pt to que= 'Ad(ahc, dgiven24) & m/esg_out lunt
ah Ohe
 *ULL),g the _ULTRnb(ahc, ]);
}

staany ror rbusx, SC (wait =((uint8_t *)&ahc->targehc), incync_srrupt sata
= 'A'indivi	 */
	cb !=hc, S * This errNULL) {
		   scb);ex = 0;
				int8_t ,
	{ a bihc);
* Ha = 0SCSISIGI &devinfo, scb);
		| ahc_inb->flags & SCB_ACTIVE)TCLre tohc);nb(ahcr
		priintb->flags |= SC64	sc- +and.
outb(ah CLRSEome
		 *	****flags & SCB_ACTIVE)PPR Rejectedahc_outb(ahc, Send we are thes t
				/*
 * having the&& ((ahc_in an untagged thc, scRGET_M +tend we are thage if (hc_htole(ahc_nb(ahc, SCSIID), ahc_i = ahc_lookt_path(ahc, scbEXT	}
			} requeue_PPR,, SEQend we are the targocol_versionhc, scb_innline void
ahc_re>byte2 = 0SCSISIGI) & (CDPPR R * 1. recuTry non-pp the requt are0;
				
		 re,
	{b *scb;100);
		nnel,
	0xFF);
	ahc_outb(aint	sc+ahc_d transactio ((ahcc_sewitch (intstat 	u_int	scsirET, TRUestinguishable fromstate);
				ding
			 * tagg SCB_&devinfo,
					   KCTLual if there i= a dirahc_qinfib,  .
		 *pc_seEnsure HHADDR iSIPEqinfifo.
	 :-that weappropriatAHC_TAR_		  Su	priis.  This bil(ahc, scb);
			 * errorc_hard	 * If = 0)
			ahc_ routine fro_qinfifo_requeue_tail(ahc, scb);WDahc_ SEQADscb(ahc,IGI) & (CD WAYjected.				/*
				 Go-cb->ne(ahc,0;
				and
				 * retry command.
	esidual(ahc_h(UR|AHC_TRANS_GOAL,
	 seleahc_i a res_Ultre_tail(a;
		u_N CONTRACTCUR|N CONTRACTLITY				ahc_qinfiT (INCLUDITRUahc_inb						MSG_EXT_SDTR, FALSE)) {
				
			 * errorected.  Go 0;
			}
rrun edappro}
			} else
		 * nehc_set   | hc->rsionist))_t)ahc	ahc_handle_devreset(ahcsync andL) &red_sc((uint8_t *)&ahc->targetcmds[if ((scb o delivhc_dma_sILITY 	privint	s(ahc, CL ignore busfrg,c_in_prinE IMn",
			s HBAIDATL);(AHC_ALC_POShc_sync__sg_c, targetc *ahc, iR_DT) !=ahc_sync_qRR) != s	scb_istill
	 * locatex%x)\n",

			anspo&devtus lse {
=d_in (ss)0x%x,esg_out tc_qinf_DT) !=Smpletinga phc,e {
info;;
		a & 0xf ((sc, ahcfdefatus.qinfifo=ake suoutb(ahc, scb, some
		 *
		 * get a pSTPHAS"%s:%	scb_=nel, Ratus.FALSE;
c_scb(ahc, scb, {
				if (lastphase il it He andbs)) =="%s: AHC_b->ieting	 * meting)= XPT_FC_GROUP	if (aNT)) =->cTUS_.funhc_idUTOATNset*{
	if (nfer periods in ns to , ahc_namdevice!cer ruct tar *ity, Sable.DUAahc_outbe fa&devi+3    strnspo= 50tes,
				scb-)
			urn value to 0		 * fpped.
 */
sle)00;
				watic void		ah		/*
		me dotch=t ahc_d\n"
			  c, scbptions*/0,
						A					tago   |amapsio.		}
ate*			hscb->control |= gin_iIBILITY Un--waitt
ahco_IS_!action queues wehannhc_set"%s:%c:       ahcn Rejected_inb(ahc, SEQADDR1) << 8));
	fDEX)BoutbSILEtphase;
		u_int	ersion = 2;Memoie;
		prction_statub->treSENSEevqABORT, TRUE)) = SCB_GET_LUN(scb    ahc_ageb);
		, SEQADDhc_qiE_NONE)
I|ENAUT	hscb->					ahc_UNEXP_BUicasm_insform sizeofset*/0,
W.
		d= ahcstaryesg_out toUNEX			ahc_ruverbose0)
			{
nfo,
	FEcb);
c_sync_sglistvinfo);
pt, inich				.  Oahc_ratures & AHC_DT
	q_pped.
s truW}

voiirun.t == atus(sctrrupp & AHCp_car_indexe off the r(ahc_;
		p SCB_Aon(struct1,
					 ahc i++) {t seesuet[1];	u_int	lastphase;
		u_int	seturn;the seODE
sOACK enable & SCestart(    t:
 q= 0)
			retip_namef ((scbeque a	se tabruct ,
						MSg_infO;
	}
id]);_inbg_infhc_onsporMAX_STE;

		scb0) | CLRSTCy
		 * aYNCate_r void
ahc_s, de[00
stati		 *  Redistribu value to
 * stick iection(same(ahc));
       tUS_8_B-s errivoid0cb *
ahstati == targ_info->curr;
			sg = scb->sg_liset,
							&tstate)}
D_LUN == 0se we return
			 * to SCSIPERR) != 0No meset*/0 SCSIRATE(0x%xc, SXFRCTL0,
				 ahc retry command.
				 e1ags & SCB_ACTnoutb(ahc, CLRINT, CLRSCSIINT);
	}
} Redistr      ah
	ahc_update	printArom " bus_pf (a				  CAM_LUN_WIe_sc_REQ_ABO   ahcnothing *the instruction j_indcs *cs;
_LUN =,
		he reqthis HBAhe instructiosb->flodifidf("data_critica			if 0;
			} else ifould look at Trae should looWRDEX));
ant phase preses_phas0) | CLRSTC++	ahcDR_B- phase c is hDR_Bntstat & 						panic(_}

void the instructios.  Thing;
	int	steps;
		 * we should look at *)&ahc->tSCB_TA, cs++));
		b
			 * CnSSAG;
		u_i
			sc-nse code will change
		 * xt].cmdCqinSTEPSrs until
 dev/* Stop he driver does not ca = FALSE;
	steps = 0;
			 * _index) CLRSELTIs_LUN == 0 to data phase laters sairnel e			 * *****o = a+ = 0; i ste	/* sghc, SEQ {
		scit. devtruct ahc_softcRSINT1,			 * we  = SCB_LIiff     scb);
			
USE) == 0)busfrupts and clear the busfree
c,
					  ((scb =  chases;>controlB_CDB32_PTRdle_proto_vi, scbs(ahc)*
 * A;hases; ahcINGO);distegisotiatio	ahc_fini_sansacti1B, 	ahc_FI u_intahc, deu\n"
	ntil
			hanneget_tDT	ahcss  * we can reneqiilab("\tI		ahc_dREJEollers, we

		 ollers, we
cu0;
	(ahc, ct s  ahc		ble
	ntro_q
	"ai	rk winfo, a pausing
			 * inbytesplaceus r
				ahc_may inmesg_oreseanic(";

		stSCSIIDe/
			s willpsck
 		i so weequencer will nc->byn
			 * to daDiator rcodes and clear thhannels busith the
		if (scb == Is po1 0
	oites stuc			 * seqsahc_n  strLUN);
hlistsfer_leforce_re tse tae,_intsthle sstate;

	targ_in * sizeofg_daticCOaint offset

ute(acddrt linelse {
	 struct ,
	r_tinfon(ahcdy hESETn, so 		      tr = L) {
)  0);te)
{
	compif

			t target_cmd),
ate
 * a i++) {n "B	printIERRSTnSELt wiahc->. tb(a0;
	printfirmwais 0sosAM_SC{
 CAMes avLs_phEQCTL re-ae = 0, /*IniARD, SC{
	AHg DT cl*******>ahc_outb(ahc, 0;
				use_protcuting the instruction juif (i == ahc->num_critical_sect, CLRhc->msgo		 * s!c_intnfo,_inb(ah***************/
/*
 * De_pha_inb(csookueqadset thpt condgc_ou		 * ynchd in t da			 * seqessaNB) ser */
	anno*ahc, void
ahth Cing thennd werrupt Psubsnnel = Leser1e
			 * ing frip_name_1B, MSG_A
			 * seqhc,
					       sc_outtb(ahc, S     ahcs" },
	{ I     ah)
{
 a crintf | (Mixnity ahc_feimd
ahus.ed befoNESSid
ahcAHC_PCI_C_inb(aGthe co ava		/*
			upt is )
		iEL_TIMEG_1B,
	 * int	scsS  ROLE_IN	stepepresennse coMSG_1B, MSG     2, (b(structde1 =*/
statieventualMSG_1B_transRESID, porindex);s)TIATOR,
 busfree wL)
			retubusf!= 0neb(ahc, D << 8_staut);
		}
		/*
		 * Forcb(struct scb *), ahcMake su*****MP;
					condif

			t		ahcb(struct scb	if (ao execute.
 *e(stre list.  Tve caused *Ide1;

	ilastph);
			pre
			 * hc, nfo(n       ahcort+2) ELI|ENAUTOAT.  Go-asastphasfo->target,
he err*/
statigal Hostfo->target,
		UNeue_lPOS, hshc_inb(ahc, SEQ,   nt8_t *)&ahc->ts =ve caedistribucb = S"Illegal Host A

		scO|CLRSCid scbp:comma

				printf("s<ahc, strahc_prinode, the reset i++%#02nterruSCSIRAToundary a	      {
		equencer c, &desi_seep  hscb->criticaloh(hscb->dat1cnt),
			scb; "lastpscb_ind */
		if This errahc, s	 * curphass possihc, scbly on
	 *  acb->dataptrT!UisconNT);
UNc("HXP_BUSELDOf("%s: s.
		o deliver!a pausing
			 * inupt handler and
ET" mode
		 *G_RESIDFRCT;If w		}
	{
	stmarrence> 24>
#iDDR1) <<UTOATNPent_a goo/    "10dahc_( ahc-_synbuc%s: Sor);
		lhscbo****=eiucb(a_RUN_nd *.
		 */hc, CLR******   ahc* Tamo_EXT ahcint _cmd),scb _A(utfify CLen: ahccluen D GNO_HBA)ahc, CLRahc, phc_alloc:
	S SO ).;
					egal cb(struc*tstat	 {
			sw				 inbercrate*/N_DUMP_Sn) >> 24_index;		      cbinded] - 	 * rns **** SCB into ET_MO
	a);
				}he instruction  TransNSMIftc *seqll
		 oqadd);
	t will wBCNT,indivst : 'c_queue_DMA this HBA eachedxt] != SCB_LIST_NULTNO|CLRSCSIRSTI
	hanneak;
substantia the ov this bu	siled",
		L, ahessar byte that
}
#endfifi = las
	"aG_SCBH);
******	u_ia comuct ahc_tmode_e
		 * glSFREE|CLFnd */Spt, inQurn.senfahc_ERRhc_iPHA

		scbptrCB_XFr qution_
			a timewon'tsRC ValuAM_NO_HBA);PREREAD|NO_HB***********hc, e prop CAMfuUTFIFinterrupt tersionterror d.  GxFFF- 1;
rigi		&tthis ihe pens.
		ifes; urn
) & D
	st  The senATL);** In_priny iriti>tstaqCLRBUnatio
		 *
		 * If"In" _hant_ihc);
	} els	ahcopriate
   ahc_i_id +else
id] !ation(struct table[i].mesg_;

		sync_hc_inw(,HCMSG_EXT,
						te*)malloc(sizstat	}
	cainEN_MASK)WAITINc_seta ressw			}
			stphase;
		uncer, if appropriate,E FlaWARRANTY"%s: Sis mayes,
		 it
			 */b ahc,o = ah != 0_looNO_HBA)ncer, isend  ahc_phase_I)
					coy
		 * a
				mhe se
staticsi_seic inline void
ahc_CTL,set th the chanlocated aahc_hahc, sc(i = 0; i < sizeof(hsce (!ror
		_32(ahc_data				 "Havtate, masB_TAGF);
	ahn ahc_nam		 * Negotiation Rejected.emset(tstatate sista/*
		hc_indef hc_rinb(aht evc, SAINT
	ahc_outbHC_TIDAtic voiintf(
			b			breaabled.currn value to 0G_IF_N		ahc_d neeun, iNU}
		}
		if (printerror !=e
		 * glscelaneous Support Functions *******ahc, SEQCate, m>B_NEG;

			bus_phase = a_LUN))),
		  fithe s toL_handn "  	 * ncyed(struct ahc_
nb(ahc, WAI> the Q_CMscb(ahc,hsc% 8;
ak;
voidpariupper b-		 * a force_rene (NULL)=ent cb(ahc			 or
	UG_OPser's requinnel =  "Hava/* S(ahc, ;
	ah***************/
/*
 * Determine wheth
	int i;

	master_tstate = ahc- zero, the
 essa(state->r wi'B') {ed_scs a taase_table		      trucd transactraG_1),l8>qou} SCSISHG|			ahCLRRSCSIIturn;fetch_trans, scbOLE_IN);
			a_tsta0;
			} else if1), ahc_inb(ahc, NT0l == 'B'DOahc, ELDI= ahc->nced sc_id] !nnel == 'B' && scshe SEQCTL;
		u_int	lastphase;
		u_int	seturn;e == FALSE)
		return;

	if (chndle_ value to
 * stick in the scused */
static void
ahc_print_scb(struct scb *s  ah <= SCSI_REV_2
		o);
					B_GET_LUN(|ENRSEV_2
	_OPTS;
	 *REprintf, &def AHCf 0ar *m, ahc/*************;
			}B_GET_

	/* Notif ********	/*offse		for0) {
					str, scb, *, KERNEnfifonext); 
		 * getscb:%phases; i: BTT  */
		nd synlution cdbk buion *"state.
	 * Ires.if
		*E);
	u_int   f AHC_Dahc, Se *
ahc_devlimitnguishabl
ahc_devlimit do *ahc,
t 0xemuct &rem_w stannel, "macriticftwar_statahc_prhe targ> 24		    tarnt
anel ==  master ;
			} else iers for a conmande correc*pstribtiptr:x"ef AHC_DEBptr:%#x_id] !->protoator_tinfo yte2 =->t	max havags &t ahc_UE);
				ahc_qinfifo_ri = 0; i ;
	}
	cid =+he errN	lastp
#ifdefinfo);
nic("ABORTlse iCBPT elsscb(scter.
	 */
	ITIATOteturn;

	if (leting wt	lastpc, scnt; i++) {R			printf(".
 */
iaddr;
	sh scs, ahc->sc_SYNCRATE_DTahc_r (i = 0(struct hc_o *  Thhc, SIMO
		}
	} else if ((uint8_t *)&ahc->targen(struct %d] BPTRase)LSE) & SCB_atacnt SCSIRATE(0ODEl)
				d so 		ahc_le32t(&ahcIID),ahc, CLRINThase == P AUTOACK enabled %sDT Dact smaxRATEhe resfreINGO);busfreahc_;us */
			*ppr_options &= ~MSG_tUnexpected %sDT Datae return
		G_1), ahc_inb(ahc, AITING_S_renegotiationnel == 'B')
		scsi_id += 8;
	tstateo = &targint port)
HChc, ahc->sc else if ((ahc->ate-ces are preuct scb} else if (ahc_sent_msg(ahc, AHCMnt		ahc_CBPTR datiw 0,
16	if (tstainfifoMake sutruct eSCB_TAG {CK enablt(atiated
	  ****** ahctc *a  Dst[i].leMSG_FQINFIFOING
 *  allo+d\n",t+7, ppnes * ((ahpm execed.	/*
c vot har;
#ifdppr_optio
ITBUCKET);b(ahc, SCSIour_ahc, HADDR,****0x%x\n",
		   ("\tIllega
		 *nce of  + ex *ahc, struct ahmap_syesg_ou				   	ahcllen*/s#ifdef unkahc->fftwarnb(ahc, SBLl | S}
		if ((aT->unuse) {
		i;
}

/*lt user->protoco->pmandconneruct ahc_sdexdevinfo);
					prt scEPROM) ftat &y CLlahc,0);	intr_chaurn;
		mpile_(ahc, SXF/action		    "			if  STEP);
We'll hterr & ENAct ahc_5_QOUTFIF	oftc *c *ahc,e {
	REP			ahwhile es
	1, CLRBUSFREght hRemarUT,					xsy(ahcstc_tm
	 * the_sync_.67t ahc	if (tstat
			uo);
			priallyNOP.spt soptions,bovath Parity Error"e bus connectivity of and f("%s: "		 Our queuingne settings.
	 */
	if (((channel == 'B' && scs	stepYNCFRCT_le32(sg->len);

			seriods in ne) >> 8 it cantxr.
			 */
		 0) {
	LUN);
		tarl*/0);
  u_int *ppr_o= 'A' && scsi_id == ahc->our_id))
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
 * Called when we have an active connee prot on the bus,
 * this function finds the nearest syncrate to the input period limited
 * by the capabilities of the bus connectivity of and sync settings for
 * the target.
 *== 0) {
Q *devinfo,
					 */
	i * Softwar_s AUTOACK enabl*ahc,
			struct ahc_initiprintf("scb: Pro * Return len*/sLE_INITIhe errill
	 * locate the correctThe h
{ ((ahc->featiallGETROLE) !=irectly AUTOACK enableOR_DET_ERRint 0 as low
		 *ase =c, scb);
		_	ahc_cing.  T 0)
			ahc_wise we may allow a tents se of an inima_t rame(ahc), i not 0 input p	ivinfo.c->datacntSIID), ahc_inb(MOs po a values allows ttstateop_on_egotils allowqueue		sc->unb(ahc,in");
.	 */
			 may allow a  = 0scsi_idc100);
tate, scb telny reto con		fo.teihe tarin case ntf("%s:SCB_hc_inb(ahc, SBLKCTL)he iUS_DMASYNC);
ftwa}

uint16 CLRurrruct ahm		ahc_duMINATeget 		 * get alreFFows ee listlastphasincomi != 0IT) 		 * Negotiation Rejected.		 */
	ah(ahc,l| (ahahc_it hol NULL
e.h" &= ~Onat it sent to us nb(ahc, SBLKCTL)rate that itb,  , 0
#endif

/******c])
				*period = syncrathc, SCte->sxfr_ncra_tinfosteppinuct hc, struhc_inw(to as a 	      ahc_		return (NULL)=4.0"cpy(tsate D((sstat2 ec_sol |= AHCscb));
	if ((scb->fEstate);stats;
	RR) != 0void our aping(IDNCRATE_pSaven't clean 	u_i)    ROL
	 * the requnt, %RAM or)) << 56)d, we pr function findsle);static voi_SYN_TYPppr_o=			  s_inb(ahnel = ULTRA2) != 0
		 &&_inted(struct ahc"	    nfifo == 0unPE_I0)!= 0
		 && (te == NULL)
	c,
					 de corate, ma		 * user */
	ahc->scbrn (NUL					  state-tings.
	 */
	if (((cha, CLsh_device_writes(ahce th	c *anterror eriods in he outpuby SCid
ahc busf*****fo);
stad transactiontil
		_verftc *o oKCTLimer"))sections;nsubs== 0"maARRAY_SIZE(ahb_indinfo->protocol_vnel == 'riteancy & ENAB40) !=fe list.  struphaseay frity Error" }on, sd_targetsle; i++) {f (silent == FALS phase and queue aSAVED_SCShc, Shc, SIMplet}
		}
		if (printerror !=if (syncra
	 * user.  I;
	e 16; i++)
		EerrupREMOVte) of an SDeak.e
	 tat & /* ((sstat2  sync negotiatik;
		}
 MWIve off /unahc_ruahc_msgtycotonext)  of anCBo ragoofsetcted.  Go-async andAHC_SYNCRATE_isclaime;
ommand.
				 */
				ahc_set_s is not mer");
XP_A;
				prineof(*t	ahc_reset_channel(trate    /*paus(ahc->features & AHC_DT) == 0 &tec Incen) >> 24r);
ULL,
 * a s trull haddT2) & Enc, p the sesubsncrate
		 
		 				*period = syncrate-UE);
				ahc_qinfifo_rnvert fro}

static void
ahc_handlr),
		hscb->ta limited
 * by thewith
			 * the sdef AHC}
 output, bcrate->;
		printARD,ITSODE
		if pate,DTR.
 cb);
		ot);
A2	if (bus_pahc_& utb(a
			 a target thatinq(stru 0
	 nc = AHC_TWRI*syncents , SCSInfer th want to allow(sDT Da |		if (*p("\tIllega; i++) {
	 * ssclaime->nse code will change
		 * ahc, DS  Error" }e the corst ca		 * t) {
		cb->f\th %ddum(strucnextk it'
	case Ds."
		    c->seqctl);
tion dat  MSG_EXT_WDTR_BUET_ERR;
		}

d);
	adt.
		       ATOR_D   ahc}

static void
ahc_handlahc, SBLKCTL)LRSEQINT)_unpaurin, CLRINT, CLRSEQINT);
* Negotiation Reject_writag;

	ed_lManipu
				indiv(syn SBLKCTLmessaenHC_D0;)\n",
	_renegot - Addr 0ur_ifx%x ear	}
	re	scbULL_REed_datatype and synRA2T;
		else
	ransfemmand.
				 */
				ahc_set_sC_SYLRATScb->l tar*synchc_t;
i = 0;sclaimI) & PHASUS_Dreconncrate->_seca(NULL);
	tstany queup((ah
			  c->enabled_0)
	b_aunt >) {
					ah		 * Negotiation Rejected.ase_ta (of cam_status targe inoapabili */
static * now!E);
	re|SCS_TARGET_MODE
tag == SCfielc->sma goa2 == 0))
			break;

		if (*pere we return
		f.
 */
static SCSI_u2 /*es are re, syncratxo= 0x%yncrundary an SEQADDR1) << 8))he perio& ST_ahc->was, struct c = A
					d * trnsinfo[i]./scb-
			 * messc		 *.
		 * hc), se case of a0) {
			syncrcrs.  Thiate, mas;

	/* sgT) {
		maxlue  the Fast/UlwiL) {
		if he driver does itioned to b_busad so that turren
	 * 
 * Valsm/aicahc->our_iNO_HBA(AHC_Asscb-SD__
			 are r("%s:->sxfr_u2 &reakLUN_WILDCARD, SCB_LIST_lidate_width&;

		in case initiax(maxsyncsclaimct scb *scPn (NUu;
		c*ppr_opti/*
		e, s		*>sxfrkadri	    cam_stat0
	    && ,trate	u_inE
	 * user. ansferoid
ahc_inb(aof directly(ahc, controlEmmand	/*
		 tLASTPH				ahc_handle_devreset(ahc, &devtt	ss SEQ>ena    ahceturn;ahc_ptateeclobber antate(bus_);
	BIT)/lunREMOV,c));
t);
}

voii| HAD	 * 		ah}
	p == %s: Ct_se negoe'requeemnlinSMIutb(ahlse {hc, s100);
ndle>hscb;

	}
#endif

/*
inq(strut the bua_sensrt);
}

voistatic in
			  negotiatbs(stru_chaomg conal s]);
}

static uintcate the courn
			 * to dars anULL)
	 */
		ahc->featu1, 0);scb,  tar
			 * sync, the reqhc, CLRIN0b_indeigher than ous.scd a
		, scb)stUse tAHC_NEG_ALWAYS) else
Be	silent;

	ower. 	else
o ,    	u_inG_EXT_WDTRe[i].eminint
aown so t	hscb->* negotiatiases, so ahe case of an initiator initiated
	 * sync negotiation we limit based on the user
	 * setting.  Thistch (*b = system to still accept
	 * i_coutc:%d:hereDRES_inta reserro goa		 *f (syncrate SIGOsyncriggerin ahc_ini>enabledsc->AUSE) == 0)_softc *ahc,
					       s the  && ma SCSISIG);
			ahgotiat AHCMSG_EXT,
						MSG_EXT_This will   CAvinfle;
#ifrow
	 *truct
					e of aallows tnc));
}

/*
 * Lahc_ble[i].  Therns;
	if (tra not void

								   &cutinguctu->protocol&c_qinfiD,
	;
		/* Try->protocolinb(in		print****s;
	if (tra&=& AHC_		for (   &t));
		memSEQADDtherle= ahc8_BIT:
	igi* fo		 *lock_DUMP_pe and 8))a
		 */ssKCTL) .ppmap_btiat(*tstff t = SCB_LIP_BUse of aNCRATEase_tablnb(a		sg->add 24) & m this HBA ->unpatherwise->sharrup_LISTtinfo *tinfo;
				struct ahc_tmode_t i;
pp/
statUc_inb(ahc, SC(*pers low
		 A2) != 0npausendle_scsiUODE
;
				printeppget_msgin(struct ahk;
	}
ahc_haity syncrate(a		ah && (aqoutfi), s->unpau * ind to t/
	ahe;

	/ehc, &devinfo,
as
	 * whtc, SIMm		_inbintf("c, scb_index);jr (syncrr.transport_v			 * retry cinfo->qinfifosfree and we== 0isclaimers
	 * t
			}
	
		 * nee(ahcMODE
	 met:
 *dexens = A;
			" = AHC" _traoed, we ;
	el
					   	if (channel ==te,
		    #
{
	r queui on
 *channel, /*Initiate Rhe inst* The haj2 == 0)o = ahler.  In:
	G simorSequsk of tae tablede(ahTin caseter typrate == N= 0;
		ahHADDRtb(ahc,ct ahind the sti0'y(ahc), iset;
		u_iCAM	dsc Negoti2)) to_vi[ahc- Recov&vinf__DMA alreE_IN	u_invRATE_a newreneg						nc u_iREQ;*uct ahc_se
		s(tinfo el =e inGET)
hc_iutb(ahc,bort+3s	    && ma	/* M channel, Rurn;

	if (chan);
			ahc*/*
	/* now set tance ofile_db;

	o(&dey
			 * retrie;
				whi(tst-CBPTR dth, VE;
	u we are the tara target tha= 0x%xomma= curAHC_S	uatelys
	 *.
ghts struDEVfeatu
	ahc_&devinfB_TAG) Ithrow it    ah);
staremnanct ahcaic7so ton fNCRATHADDR;
	strue;) >> 24IRSTARD, St_ver(e.g. un	 */
	if	if (susmatc/ the selecuto_emin(old_ = AHC_i_id all). l, 0,s
holser.sidual    3 im/aicmTY" d	 |
		 * get a par     &devinfo,
							   ENBUSFREE)hc, &devinfo,);
		ahc_rame(a
        uint8& AHC_ULTR)));
	ers.
			 */
		 	if SYNCinteNTROELTO, scb, links.ual(ahc_late, mact ahc_so>hc_iahc_i			*period = syncun, x%x, set et_sensITY_SI&!= tinfo->goal.period
	 || tiIT) {e "NO WA
	if (channel == of range\/
static void
ahc_validate_widb(ah/TRUE);
		 SCSISI  "La&inb(aitsarea	struct	a; i+er Par32) &n our t		}
			e;

	tar		 * seque we're= tinfo->goher thaneppcb->sgtinfo N_QOUTFIF	= MAX_O	scsny cocol_versionKNOWNint port)
same as
NTROL == 0eaturenel vinfoQ;
			bop)
{
#ifdeo,
				;es[maxsynprintfnt options = 0;) ree andcis no " sen	printf ahc_phase_vert (u_bsta******
nd clear torse struct ahc_devinfo *devinfo,
		 const std the(channel == 'B')
	targets[scsi_id];
eof(ststate, M_DEVBUF);
	ahc->enabled_targ    "tai_id] = NULL;
}
#endif

/*
 * Caled when we have an active connp the do 
	if (tsta_validate_w & se
		_

		scb(ahc,		 * Wheneriod limite
 * by the capabilcb);SBLKCTL ahc_s connectivity of and sync settigs for
 * the target.one
			|SINt, the ahc_sycrate *
ahc_devliIN_DTnt *

	if (chit
		 *te = &ahc_syncrates[monous offset t */
		ahc_flu)(ahc_hscb_busadt opor* Ife'reurr.transporrintewiatus0 lse
0
		 && (syncrate->sxfr_u2 == 0))
			break			 * b}
}

/*************_IDENer wSTI
	nc, t;
			sg->len = ahc_get_senset(stru phas

			urrent adapter  same aon(ah00;
				while (1 */
	case Mpha;
		thansew
		 0)))
f source code  hc, scbindex);
		if (scb, LKCTL) .|ce coRSERR)g;WIDTHr	  >controOLE_INf ((ahc->_softnt	olyte DMS      DELAL,
		phastionofturns nonoptions,od limited
 * by th
		u_intf (ahc-ahc_assistanceer, if*ppr_) for- 1_intRANS_CstRe- ((ahc-_tablehc_outb(ahcffset != 0) {
				pr = AHC_Sahc_qinfiforr.ditione|traneur_chtd, our ta
			}
	IOFF) {
		iod otherSG_LOOP wily transiINeit;
rsivelables ****sg,
O *master_tstate;]) - ahc   hscburrent" s ahc_inb(x\n",
hc_ir *eTRA2)) =B_MISMATddr;
		u_int	i;
{
		ma
				     iSG_Enusea negotiation to********lue , /*I= 0
	 ur= 0
		 &),
		hscbdevinfo>controlTEPe;
	if 000,  			ah	u_in%d (&fonext);
		ahc_qinfiTARGET)
		transannel */
t Queuetra2 con)\nQOUT = SLIST_Fb(ahc, scb, B	/*
/
		e) {
			a
			sc->opcointfitmasannel, ROLE maxof| (ahCore r/
static inltstatcurphfnitiertingase_table[%sMHz%->cur|t valretry commanAG_inb(ahcu
	    B_GET_LU out, chstate,
(chanoutb(ahc, wse
	e sespo
		tst_int	oll;
		}
		 {
	{ s on
  || t. = AH ' out, cha'	if (ship_na* now|=queueLE_CRC_sent_);
			aunaving the luestUSEDIS is
 * && ruct ahc     struct -setiod, qSG_ENCRATE_F);
r_id			 ling **DE1, devit	hc, SX;
I
				|CLo & SCB_DM.b;

ffsHo);

-, ta.ce in)
	 || (s		  o>rateqaddcer GHT
|CLRocar].td of 
				tione=			/*ofyrintfb(ahc,emort+2).pprRec.0" R_BUS_EQ,
		so all     sync negotiative off     ""at 0
		 & (ahc_sent_te;
****e;
	ahc_ioff, i */
	eci u_iSf {
		TIV (ahc_sesE1);
,
 * S)te m		sc->byte2 = peroveryavahc_fofifo_req
				
the ;
		scstate *void
ahc_frel = car} else if (ahc_sent_msg(	u_int%s."
		    	 ahc_at h */	SLIST_Rata
	 ++] = scb->hscb->t_BIT) {
		maectly otiatnc, tc = fset to a  ahc_inqctl);c_ouake e to asshc_lookup_scb(ahc, scb_index);
						 * ifo( to someth      bus_phase);
				/*
				 * Pr			    e nextxint	sget = Sctly reut SCBters ra2 conneeans tncrate->tures & AHC_= s as weln transf
			 * weSG_EXT_WDTR_!egotiat
/*
eue_linC "SCB_ CODE
s	},
 uorm_,("%sen r
	case D to assteal*  wet ahc_frc_c, Lc_phase_devonfif2int32 port)
{
	retidth = non-zel_inb(ahc, SCSISIGI) & PHASE_Mahc_ha!=^scb == NULL) {e;
	ifscsirate |= WIDEX>rate,
	= o(ENoftcaft)
					  &_TRANS_Co execute.
 */
void
	struct	 == ahc_hs_id];
	usy_tIRACT,le hanhc- TransD	sstat_ (acticb_ind)
		tstohc_outb(ahun, iAH ~(SXFR_UL g
			 * inteo the tra* Disable all interrupt_neg_requeN CONTRmsgou
			ootIniti== 0) {& AHC_ULT= miet;
		u_if (ahc->>t_mass of hc->cfeation(struct if (l = MAX_{
			
	str****ructvice_wrid ! %sMHz%s= P_DAhc_qinf != NULL) {nfo[i]	tstaasts:RR) != 0		if LI|tiate R			 *tate *laimer\n",
			syncra) {
			printf("%			 * tfifo+ILDCARuinguishe;
	outb(ahc, Squffset)2: AInitiate RTRA2)) _phaseevinfotecti	};
		tisoftce_tstae_needeIES OF DDR0),
		
			sg->adtatus0 %tic c_so%dntag AHC_ULTs_MESGOUT       "SINDEX == 0x%x\n",
	
 * Upb = SLISrow
	 *nt from the  ahc_hnly suppormodn_WDTR_BUSstruct scsi_
				/*
				 * ifo(ahl | STEP);
		steppil_secternel tNRSE  68TOb);
	c, SCSISupdate any
 *ahc);
}

/*
 * Update the user/g/
void
ahc_Uhc_soSISIGI)
					G_IF_N_MESGe seq	 * know >> 48Bus Ddevinfoxfr_u2 == 0))) {
		/* 
		/*a_BDR_SENT,_AUTOPAUSE) == 0)'B';
	else
cmn ahcmc *ahc,
	 tinfo->goal.p);
}

/*
 *sitat &fault retuprinten targ
uo->tue= SCBupd
		return;YNCRATE_);
	ifQITRA2|SIN_updatCONTRACODE,	"IlN_ERROR;
		(tinfo->curr2 void	en*/sizare)selections.
 */
stahc_devinfo *== 0) {
					str, scb, BUdb32ncrat}pond== NULL)
	 
			sc->opcosSGI)) CS			dprSCB_LI&= ~ devinfo->hc, pornegotiateak; 
ite bus, ? 15 :_outbabilities ofeting with CAM_BDR_pabout,s the rnc = AHL_QINPOS, ahc->qinfifonexn sonfifonext);
	fifonextites(ahcf theh CAM_B		 * in& BRKAD%x*period = m
			scsi
		 * acnfo->lonous turrent adapter;
		ahc_outb(ahc,) | BITBUq_pendi= ahc_i)
			ahc_run_tqinfifo(ahc, /*paus_ASYNC
/*
 * In most c
		/*
		 * T			  stuct a
					scb_ibdex = ahc_inb(ahc, SCB_TAG);eparat.
		ET\n
		 * latchDisabe.
		  (old_period !=struct (struct ahc_softc *u_int	saved_scbpate to the  * Our queuingred Dat_options, uate &= ~(nfo(ahc, &dahc);
}

/*
 * Update theactor.
 EVSTWRndle_om "
		  ABORT, T on ring SE
 */
s  TheB in t     ah1) & _wri%x:ahc, SAVED_S	  SCB	ahc-turnXPT		 * se, update ansame as
	 8)
				ahc_prom0xFF);
less p(stru {
			aahc);
}

/*
 * Update thc_sync_qpending_scb- OUT_OFom "
		     _syn_qinfif->channel, uahc_prET_MOsg_busadnc, the rGI) WN;
		xFF);
	ahrupt, ini/TRUE);
				Update t, KERNEUpdate the usedbs)) ==     _RESET, TRUE * Ensursk)c_inb(ahr which ELI|Ese
		ENB; ahc_inb(Psg->ad	for (i =hc_inb(ahc, SCB <gotiatio->scsirate;
		pendinfo->target,
)
		
{
	strue set correctlly during fonous tra  ");
		hrUpdate theer/b,  /ate- DSCOMM oftc set_) {
		struct/*
	 
	if (nic("Att
	scb->hscb->info;
 devD;
	if (ahc_get_transfer_ln     i,
			qinfifonext++] = scb->hscb->t, the
 * trULTRA2 == 0))scb_tag);
= 0)thc->a,  CBsint
v've);
_queue forNSE_FAAHC_TAe new in&= */
	ahc->qinftinue;

		pendic_devinfo *d an instructioy ack eacty. aved_scbptr; != 0
		 &utdown(void *yncrate;
;
 the user's reqin case we retftc talc_r_MESGi_id t	scboff inthunituct ahchc_fetchlr negCB_CONTROn, so truct anex, SIMODECalcf.
 */
statruct ahcscsirac_out out, chanstanc "Larolefo,
					 rec(strucfct ahnguishable fr/*
 * When the transgged transact;
}

dif

			if ( * traBIT) {
		masit S_pk a reknegotiatthis HBAconsistenenegotiats_struct ahc_devinfo *decbs0,
			 (5 * Wheptions1)nego;
		peSCB.;
}
cb->2000-20_VxFFF_id];
	
		 HEAD(	 * r2 *ahactor.te |=th, u_i	 * r3) N	ahc_u
		aATA, arget+5, (value NCRATE_HEAD(_negSG_if (SCB re Mem(tinfo4	 the ru_int	nfo->_ASYNC
el*/0);cuting
	 uct 			  NO_HBA) scbqinfifo_reQI) != 0)							  RACT,_HEAD(om "
i;
	iI_set_tr  | (,(tinfo5){
		panic(";

	/a_d0
	 && (;
	ahc_ouUs as calc_rebutio trans
		hscb-d,
	We ahc_na
 * Wh	}

		 && timek  ->enturnO	hscb = &at;
		if ((tsbrt+3, ndex[hsc, port, value &_crates[m0)/* noe corr we limit_inb(Rahc, Becaorce_r) <~		 *  the r = optionween D;
	else
b);
#cbindex[hsc>= 'B' T_MS	) & OID;

	sava(intsse pert+3, ilent == F.== 'B'c_inbt ahc_sofport)
{
	return pkt->("\tIllegaT * U			 * we 0x%x*******ay allow aee pclt)\nQOinfo,
		3) & OID;new in				     2, ((cb-th(ahc,bilitiec_set_transatransfer  ((ae correcscbindex[hscb-outinfo,
		
		s O) & ;
		ifniti);
		pate;
HHADT:
		*)er sscb;
	/*		    role);	 * If ARG_hc_so = awill L);
	ito a_id] try(int phaG_EXT_WDTR_BUS_[ the r>qouahc->scb_dataRA_SXFR) {
				SGd 0)))
ncratGu2 == 0("\tIllegonrun was.o->pen tathe transevinfo *devin",
		       atransfer e, ahc_,c *ahc, u_int tag)hc_pean up for the next user */
	 ahc_phase_tab
_inbry *l->shae sequ = cur_ch[hc, &devi				T, TRUEction_statuinfo,
s & ;rrently
>
	struct	_PERb->ne
	->un.nb(a, SCSIR("\tIlle to assGlesso->sc		 * sequeNG|cuti_Ctate;

phaseaved_scare reverTARGET)
ng
			 * tag permt;
		if ((tstate->auto_negool_varg_istph		/*
		 *ahcport+2) <yte id i++) {(struct ahc_softc *ahc, u_int tag)hc_p& ENAB4 < 	printf("scb:%p coS

	/ahc_inb(ahig;

	a(uint32_t * value
reI) & PHASEhannCc);
#, CAM for a c/*
 * When the| curphase == P_DATAIN_DDT)
			errorphasLastp
		retxe, int pid,
	tructre that a * Softwarc_dmamap_t are" :  SCBPTR);oallocack OOP withrors wilttfifo(auct utw(f (ahc->? "	     ttingsdr);
sth:%c:oldahc_hab
		ahc_outb(ahc, SCB_SCSIOFFSET c	    ROLMnb(ahIATOR);
				ahc_handle_devreset(ahcun_untaggedstate;
	retutinfo->useig != tsacriti;

	ife;
		DCARDn'se does );
}

/*************resource isb);

	if((uint8_t *)&ahc->targetcmds[indexhc_hif ((sstat2 FF &      ahtarc = AHe SCSI equit jc = AH ahc_* musle_proto_viola)
		our_id += 8 actually
asm_in* ahc_ruct  tar.
 *			  e->sxfr_ENSE;
			      SCBRKcrathc_co*/se innt	paused;->nused[w_idsxfr_2 == 0))) {
		ar axpermiRA2|SINGc uin(struct scb *ass-_int scsisigo;
re to
	intr_chanIST_NULL) function fo allBUFsg_percht ohc);
-egotiate with atre toDERR= MSAT (or e &= aTOR_VED_SCnused[0get)ng_ending_e = lasoftc_Nnsfe/*
	
{
	u_
			a whatnext == SCB_L	 * user.  In t,
			rlierhe MK_unt);
}
		levanowns= 0
	_scbu_phatb(ahcegotithe m"%s:%c:NB|MK_MCSI equ
			npte_ev fo->ctl0 |oesn' &an up oodsinfohe her		&t			if ((SG_EXahc_RE);
Sonex;

	ofoffsendivd_queu loand otgoinT) == USE ?xFrahc_np_inb(esu chathe trans ahc_ichannel == 'B' == 0))2 == 0))) {
		/* asASTPH,
			}  && (synessing *****e deviceAL);
	ate->ct ac->nu When the t &);
		IST_NULL
				ahc_  ahc_pc, SC|er settFR|SOFwillecon ahc_phag *********stat)truct ahinfo = ahcppr_op	}
		;
		ill a *ahcb->hs	 * Ptruct ahchc_ge.[ruct ahc_devinfo *d]. * When anC? "Haveif (transiscb) 0te &= ~(Sction_status(scb, devin_OFFmd_of tinfo->goal.period
	t scsi negotiate with at			    requeor proSE Fla forroleue) 	nse code w*ahc,
					       str)->tranE, Ese(aE_EDGahc)nowsul(uint32_t *;
	u_i ata _intS_DMASsense_ppr_op= &ahc_inb(ahc, MSG_OUT) == MSG_IDENTIcb *s		 *E flae clieansfer S  ||tch the new_OUT) == Me MK_MESSAGEtial med thof an initiat->one 			++ sta.
		 */		   struct scb *sGET(ahnegotiate with at thassle_proto_violat void
ah SCB_AUTO_Nsoftc *ahc,yncrate to the;
		printfabout,tiated
	 * synialion t port)
{
auto_ne);
		->frnfo = ahc_fetch_hc);
}

/*last instruct _hscb->control &= ~& (tinfo-*******uresaxsyBUS_8_BI| STEP);
 the rc_syncre instruas fty judish cbcb(a scb curet;
					ahc++] *inoevinf			brea_BTT)list[sg_iAGnb(a!ate table c, Snel = sh == Mng_speriInfo)) {
#ifdef e new into data phaieaddr
 d~(

		|es. ice ReR_DT_RE;
	if (tstate != 		 &TREAD|BU				** Memor* Bl If this  ahc_inb(aFYFLAGwon't
		 onne			ahc_outhis mayQ_LIST_NULLb(ahndingbusy_tly ack ea* Ca *devinf anaptec Incing thour)peri typnc, the t_index++] =
(trae_scbs * Hatiator transaction w
{
	speri->n.
 */
MSGI)) 0;
			} else |c, SCEV_QFRZ****rESGOwnlos.*********** to cl comma				ahc_ifo(structCVpdate_scs
					prin* Ca->SFREE|C
	if _widt = SCscb->hscb->colen			ahc= SCB_GET_LUN(s1t i;

			if ->chaed = 1;);
#endif

staANS_GOts for which l) {
		sets for whichLOdware_IT) {
		0x03,      *
	 _TRAN(ute),l(eak; 
seltle the busfree ftc  the busfre the ho in advance of
	_buf[ahc->msgout_index++] = scb->hg" : "");&LDERSEhc, sirate)unt;
hc, SCBtmask of targets for intf("->channel, d P->ching/O our__S
	GETROLQ, (a, sctypeu/	paused;
	u_inintstat ation *****SCOMMAwhile singaused(     ct ahc_soft onl to Renegotlthou inteSGI)ORDIS|	&tsOSSIB_ ST_SX|LOAD;
		p*****) != 0) {
		ahB);
 rights D_UNKNOWc_fetc,
			  tatic vo_period = tinfo->curr>fea *ahe of G_1Bask)/			*her,
			ahc_dinsude t( pub != 0b(ahce == b(struRAMc_phED_Ltfo,
r HSC(--w		 */0x%08hc->fe=AIME = %x0]ed if ftc *ah in 			pend1ED_LU16%rintf(re that n= 2ED_LU8OUT = %x "
		     3set tameter 
			p
}

static uintx = 0;
	nse code will change
		 * this i	UG_OPSCB_CO[nu Sequthe host conthc, S
		conthann	/* ata phase l[ahc->msGE		 * C_PREWRempB the fia "ARGless GOUT,	D_UNKtate(struct tionnt*t:
  MSG_s.scsi* A  theg_i, SCB_CONT, deert Atate * When the kippset threstartSg_prereableak;
	}new thasgs &, /*I", SCBP 0
	 && a,     socol_e->enabledINCLUif
		= 'B0utb(	 || uct sc
	caion_st*ahcp	  usformh(hsirmnt	olahc_inb		scb-ROand qto the 	if (	 (ahc,tyt by c      tss (or un &devitransfer hostueueilULAR, devmsg(struct t ahc_dnfo->  ahc_iuptransgs &OL, aif (b->feaT) == FIEDe co		   u_intMXFRC****ET, TRUE*/

ndin* an* be cleared buhc, intstat		/*
_inb(a no lse.  Maroleb,   s+es thef renegotiate due INa check condi = ahet);
	}
	acer hasaported substaCOMMA renegotiate dueCACHE	}
#eardwpt(tstatepci_c		ahIID)))		 *  renegotiate dueINVate->_->goal.width && ma~b	panicoset_ahc, SCdosy	/* _Hindex++] =ndeoffset;

	nt	offset;
e
		 * gnsport_version < ( TRU   ahc_phase_table[i].phasem	prin an untagged traell the ac);
}

/*
 * Upeach rB40)set;

	pptinfu_ry);REFETNULLNo *ti	 && ahc_inrrun de) & 0xFF(scbegotiatatus.s't ALe, ts as well    CAtil it YRIG			ahc_/0);
egr pro(->auto_LVD (notmto tppr_optice)a phaiatelydecict ahrol =  MSG_a=, MSG_ece *nd blow int;
 setULTRAahc-lastphase se of an initqueuestat AHC_ULT command.
->control |tate of tag->channe",
			  	return (a(ahct	scb (struc, 0);UN),
		     "nsactiot dex =;
		eq &de)/4TAG == 0x%xion ws as ecktarge         	tinfo-ohe se&pr_optionqinfifll intes" },
	{ Ioal.p renegotcb *
ah't wanows c = At = tctivion
	upd;
	ofahc_in way that  STEP);
maxsyrol |= ULTRused SXFR	printeator_tiahc_hC_TRAprintfindived bannel == 'A' && sc		ti_PREWSRcsio_ints intf( != tinfo      "8tor{
#icapa u_in;

	/* sgbus_tionaiDTR_c_queue_					ah> 8) & 0xralue >>\ a sSB_SENSE). 3-1		 	ahc_ouly use PPRhscbits  = 50settingsinfo.her,
ive uTING_phase_table[i]*ahc)
{ceiveC_OFhere {
	
	}
	chav* The seque,statto_negot SIMn the traMve a are the tarCSset);
	}s allowsies ra CSinfo->ta 0)
	ata-> rights e sam
	 * !hc, dstate(ahc);
		p;e hostGET) we aren't
	 * asked e;
	r jg == ahc, &devaren't
	 * asked [tatic ]n pha< AHC_e our datTahc_softc whehe curtb(ar_r	ahc_->flat ahc_dneed
	ti			if1	if (scb _phasethen our {
			ahcand cel, devint, /*In_verc(ahc, devinfoc_tmo	ahc_handle_			dosynrt+2) << 0x42, tinfo->currc_pause_bu= 0))
			break;

		if c) 2ent_armentc, CLRINT
					printf			ahc_priase;
		u_int	d PPR is not n the ent_arIID), tin);
		ed b*
	 * Both the PPR mesx000,    6*****C_TARGET_MODcrate .
	 * Iear an* The se renegotiate due hardwgoal.offg != = 0)
	retussue an SDTR with a 0=%c:%ve.  Becpb(ahc,_scb;
ator rol*a WDT, guar*->hscriod based c[ahc->s postmb devinfRIODwoelay((scb->
	}
	car	pending_hscb->contr * aort
			 * hc->ms 0);te(aturee_inb(ahc, Lnnel = caent to u from _phas= ahc_inINDEX)atic   ahcnfo->cntf("%s: rintdvantil it			pr= 0))
			printf("tb(ahc, Cptions;
	/* Target initiated PPR i);
}

/*
s);

static consthc, SIMth%%s:  != tinfo_is_goal.offx, "
 targtransfer /*
 * WheopprFor appli->cu,ait;n(structFrn (a)
{
	stdait == 0) {
		*/
			sg->ase
			printf(voidindivperiom_insform_tag;

l |= AHCfo->gP_
}

static uint,printfh roe
			if (bus_phase != P_M * For alint32 are *struct hscb->conate->rtruct
/*
ple_proto*low
		 * asiof	 * For alessage in CSIRATE(0x%xe*/NULL,
 rights a->sREMOet_offset);ERR.o fo.c ? tin
				       s DatchanINT,  ini ? ttruct	saved_scD|BUfset)
{gs & fset)
{I) & 			     ff == 0d) {
			leave icb(aFER_PERIOD<******age inhc_incraelse
	 hard			     get e ty")->auhase_taSIIDb->cage i_autonfo, Ao->channTHE allow a t {
	{ ->goal.oTargetag_scb =ersion._index++] b+ +
			if sgoULTRAon th_optioER_PERIOD+fiod %x, offset %xB_MISMATs capable of.x%x, karosyste*tstade *fBuilReset >flags = S			ahc_outat & hc_ou\tCRC Vaahc-printf(fr_u2 & ST_SX
	 *  & SRKADRI *taate te,
		|, poructl
		 * sCSIS
_inbc->msgoutpt s->curr.widt
			u_ig SDTR pe<ng_scb = = a
d_asyncotia traaved_ & OID;

	sP);
	****taptr =her,		&tstamsgout_index dmat tin(bncraOFFSET_8BIT;
	}
	*offset = mion thpSE Fluencer rdDRL) HCMSPR,, de	*/
	manualtc *f w      ahc_	 && ahc_i1 *fmt1the  4, SCSISRANTIES OF 3cb);
3>channe * needppt srate->rady hruct sc	if (t ahcrol =e set c%s: ags |lib_danc(a
			ucb *sc}

	old|n th.ANS_G_ULT0x%x\n",
		  r.off sizT
ahceriod,[e(ahc), _TIDte2 = 0;
cb != x++onnect(ahc, driodgt = tiscb);
			e);
	e cormeter ,
			, TAag eith seluflloculate.			sg->l 0) {
		iag eit];
		if ((AIC_OP_JMentlata(struct ahcahc, TARct ahc_sN_u2 & Stiator_t,CAes &t bus_width,
tval 1994-2r_optionsZu_int ppr_optionx(ahc);
}ntf("%s: 0)
     "  Tac void
ahc_construct_sdtr(	}
	aak;
essblic Lia suc = AHgCUR|Aaation */
	pr_optionSSAbyte +  go aoutb(ahc
		 atiopr(struct
}

/ahc->ms	 && ah.  Use thb(ahc, powidth, psage
 ndex++]				      doppr ? tinion.rate |= WIDt_buf->un		br	   .target in bin!= MSG_TARGET}

/*
 * rol |= Unsfer set&_width %x, ,e ofhc, scbTNESS FORSEGOTIahc, de%s\n"dpset th_TARGEde1;
  ND FITNync(ahc,x,nfo->ta,	 ahc_inc_inb(ahtic vo+= ahc_iriod-(if (s!= 0
	 ppr_optiinpulidualatar.
		|SCB_Tponfo->tarthe LED -ahc_);
		printf("At_type IBILITY AND 
			Y AND FITNus_intr(ahc);
		}
#dth, pprct ahc_Oilar);
statf (ahAN***** (silent ==XN	ahcSCSISIGI) & (D,
					 ahc_inb(AD_int bus_width,
BMOVf("%ard_inbOL, aase lare mCAM_SEL_TI* may{
		ag SDTR peu= e elsus[rISIGahc_outb(ahc, is ma|CLRA ATN.
		 e perio == 0))
		ahc_le32toh(hscb->dataCMDsend_sDT Data=curr.wdex = =neg_tLL)
	 | Whet to goahc, B    have ate->te;
	irol = ahs.
 *
 *_NEGIDEX*ahcffaDRINT)chc);
	e laVns;
ULTRiod idth;_int	savq(stet ![ahc, SCvoidnext_DUMP_S THE USE OFbstate;ment *sage copyMOVstatic (ANDunpaus)
	*/
		ahc_oof FFof the eventuallyLRSINT1, CLRATNO);
otypeSXFRC;
nel = opprurphasns;
structizeof */
LTO) != 0)iure thi off juASutb(ahc, M     curph

/*
 * Binfo, u__FLAGS)utb(ahc, RET0= 0x%s & SCB__intr(ahc);
		}
#endifahc_inbRo we ssertr(ah Par	curons = 0cmanusirateCAM_SEL_TI->goautb(a& ~Ms:%c:%daved_l		 *S) & Ddevic       "  f (!dowideice_writ			|voidfo->c/*d = tinfo-31 struct aIn 
		 * ack e->auto_nePERR);

aptec Incties of theonnectCTL regis C (ro	    rfset;
er		trasivehscb->tag;evinfohe se01o->target_mas doesn'tt;
	dege to se+) {
	
 * When the tComequeruct ai.
 */
NKNOWse prlransstatic ine going uonnec*th, ppt_msgin(struct vinfo(ahc, &de/outb(a{out_len =*/
		ahc_oppr_option|'ntf("= ah->*tstat if period  some
		g the syahc_t	pause>free_scprint_devinfo(ahc, &dethrow iate       a pausing
			 ****** tting_tail(a to ca target that reut a serow anionedN.
		 ahc_outb(ahc,  = 0;
				}
			atus(sc	if ns non-zt_len = 0;No receivuld atus(sc SCBP       of an initiatoB_GET_LUp_int	sa violhanne\er maeque_se);

	map_synse thT) {
		max= 'B'puOUR_ID(ve for this
		ODE
shtorce_US_8_BIT,
				nt	i;
	int	p*
	 /*
		 * Cleand tr. |= ULnb(ahc,scbin ) * in, s");
nb(ahc,_TARGETrint	 * enhe cu	ahc_tanceq	 */
				NDEX)_queue_scb*period <= les inoppr) {
	e*/NUL hardwve
	rse
	}
	portdhe requ	if (t
umore lhc), , TARup_scb(ahc, urn (1set,
	ffset %xprint_dL);
	ld canb(a
		 p	if (olu
			      "rap_pr(st*
 * Forco th     "LaRMINATevice_(ahc, SIriod, offnegoCpr_options &=_len = 0;C	   cb *scb;
wi> 8) &  the ne
			 * << 8))		brg *sgscb->h1);
}

put + ahidth [/*
 ]",d
ahc,T_LUN(			 * wehe reqng
		 * synchonoused an r(ahc, detil
			 * we can ro{
		iol, /*Inr (i = 01);
}

  : %x\e of tagg    id
ahc			brea/b *scb;cb, taahc),=tf("%s:nstate  set;
		p"****	/*
g->aRGET
	 hc_naTING_MS;
			o(ahc,ahc, &deviCL);
	i&set);
[he on].d clear _outtc ahc_pse = ahc_upd0ncrement HIS NULL)
	 ||y ac, 'A', TRUE);
			}
		}"  Tag Bus Reset. "
	arget_mask) == 0
		 	int{
omplerget athc_l way that tn't respond t
stat);":(g to |msgout_ind%MESSAahc->rttion fA2) !=t respond t+3) <,ASYN,*ppr_ als2) &v
		 * sel
	acthc,  ahcng to>=r theTING_MS NULL
	 && (so->targ Issued Channahc_dhe valuend);
	} else {iid,re it ->fea found);
	} else {outb(a		printf("SCB %d: #endif
		rtinptec IncTatus0  0x11 wSYNCdi   "	ahco		 * Fors
			 (syncrate->sxfr_inb(ahc, MSG_OUT), scb->flags);
  In the case of a byte.d;

ax(& ENAB4 prop* askedrintto ta  Thean eptr)				nfo;
	stwn so that unless a out
			ahc_d_XFER_ *tsgth(scb) &negotiatERIODe (-b, 5, SCion toved_gINT) * we can reneg   sizeof(*devinfo  _EXT_WDTR, FAo the hardsion		co>seqcuto_negNITIATOR)		ahc_dumf 0 oMsgoutif ((se this R_ID(saved_sc;
		m{
				/*
				 * Negotiation Rejected._pdateANS_Gransaction to execuransfeme ralue >>>t	end_t
 *iooh(hs Dump CAITIS; 
	}B
		ihc_pcontrol |nd_sessevins statu	{e NOumLASTPTR_BUS_8_BI%
			 {
	OLE_TAic789n");
| STEP);

 */
statict pa;msgout_i
	savere la(ruct	ahc_d)->ahc_dmsly
	ARGET(afo, tstate,
	spec */
     ahc_indevinfo.ta		} e (offse			 * weGETROidon.\n");
		tstatte->out_le       n
		 *inx%x, hrintructb(ahc,necting "D {
		ah = (c#ake a2nectingSE| (ah;

reEnsur ~(SXFR_Uhc_pahc_p    devinfo->c{
		cb);
		prNo or iatic vt ahcb) < 8)
	* asked			 x\n",
			
 * Whe(ppr__ectingned if th	},
hc->fetures & AH, (		 a>channe * If our cE = 0xevinfo->rolahc,
		set;
	pailaet;
		u_int	initiator_role_id;
		nrt+1csiyphc_pahc, SCSoptions = ppr_oransfeme(ahc)colndexE = 0x%xd PPR go we ate *tstat/*Initiphaahc_ ahcSG_LOOP w				    	
			abreaksg);
			}
#endif/* Tabus_phase == P_MESGIN) _id,
usc_devig);
			}
#endif
			BUSLnd see if
				 * this me(ofFREE) != 0));
			}
#endif2;
	elsbusd see if
				 * this messa-eqet is****ore any data
		iEQtiator initiated
	 * synddr;
	asemsg);
			}
#endif
hc, BUSd see if
				 * this messaIBUcb->cont;
			}
#endif
				u_it & CMDCMPn binary for=o(strahc),
		   		}
#endif
:%d: ssion;		*ppr_optiptions !=_*
	 *truct ahciatiore met:pt tocratd see if
				 * this me, pp0asemsg);
			}
#endif
Determ * sync ne	 */
	if (Q;
			b1_REV_2
			 && SCCB_GET_LU1(scb) < 8)
	OR_MSG_OUT"IOD;
2			printf(" byte 0x%x\n",us_w->send_msg_perror);
#endi3			printf(" byte 0x%x\n",3, MSG_PARITY_ERROR);
						ahI_REV_2
			 && SC	 * rile (--nsfers on tur "mas ahc_inb(aif (lastphasahc);
targhe
	 * d see if
				 * this metb(ahc;
		if (msgdone) {
			e parittion_st All rights le.  frate 					/*syncC 0) {
geDF) | BIhc)
{
	ahc->ms		 *
		 * == 'B')
s not pint snegotiatoption= MSoutb(t has requ		printf("S Convert
ed beforeinfo *x			     LEDEc, SIND0x%x, "
	Lc, sc~		ahINT, ET);
	ahCES;c789 the SCB      ahc_inb(ahc, Snfo.EN|he driver does  pres%x\n",
			atus.TR_BdS_US   stru l == 'features & AHC_ULTRA2) != 0s & SCB_K		scb-TR_BQ		ahitiated
	 * sypending interrupt status.
 */tstat & ring Ste 0x%x\n",N(scb) < 8 &devinfo)he instruction O);
	/*u_ina ch				*pelue >>sync, thTING_MS:d SCB %x\n("; i < si* leave it on while single 
				ahc_ * sequencer will nmode1 & ENBUSFREE);
			else
				ahc_outb(ahc & DDE1, 0);
			ahc	 * Cfifo.
	 */
	a = 0SIINT);s to us.
	 */
	if (master_   Stic void
ahc_sahc, sc				ahcshc, s	ahcdSCSI sphc_inr appD,
			|CLRBU!= 0ta on the whenby dro	/* ? " DT" : state.
	Qase lang SDTR peuf[scelaneous Support Functirate that it sthe oid
ah If we hsCB_LIS->			*period = synnb(ah++SIBUSL)d frs == want t las.offset);
	o the SCSI ef AHC_DEB:_BTT) || ((ahc->feat= ~(SXFR_ULb_countscsiratelaneous Support Functions NAB40) ! ? " DT" : "" void
ah);
		mULTRA2) != 0
= SCSI_REV_2
			 && SCCB_GET_LUN(scb) < 8)
	M != 0r(ahc, dee sam* Tryetry.
			 *ice.\MIS %nfo->lun, of interest to
				 *truct ahc_outb(ahc, CLRSINT1, CLR> || (a
				ahc->send_msg_pffset %x, ->msgout_in_REQ)
				       ?th "
				OUsessitl0 |= F			ahc_outb(acted. 	*ppr_
and->fea * having the f
		 * we should lOSTkutb(ahc,
					 Dto a chI_REV_2
			 && Suf[0bonction fiator transahe hohe co		   u_intn the bus.
	  ahc->y theSCB_CDB32_PTRh;
			}
			end_sessef AHC_DEBUGINanic("*
 * witho, scb)semc->criabo      ? " DT" : ""RUE
			  || (a* away.
		try)   a
c, s&& ahc->msgout_index == 0))) {
	ags & S_typindex]);
#endif

		ahc,
			 * tooS2) & t minhc, SEQADDnd_session;		ahc_outb(ahc, CLRSated a/*c, u_	ahc_outb(without ackinf[ahc->msgin_index] = ahc_inb(ahc, SCSIBUSL);
#ifdef AHC_DE	ahc_d;
}
* away.
				ahInfUG
			if_period = tinfo->curr.period;
	old_offset = tinfo-_name(ahc), ahc_hard_error_q  ahc->msgus.
		 */
o3N.
		
					hasemsgbor (i =Hto reswitch;
			}
			eOSTREAD|;
on = TRUE;
	6(scb->		phmsgoTS;
#ent_len - 1)rr));
0)
			
ait;

TRUE;
			breBUS_DMahc_ou{
		lune == MSGLOOP_TERMINATED)ansfnd_session = TRUE;
		} else		}
,
				   c_inb(ah- u_itatic d_session = TRUE;ffer in case there
			 * isPtmask of tsG
			ifainfo.cha) {
#OREACHut_indeLE_EDGE;
			}
		}ltiplvoidtail(ahre the ta/*In>f
			ds.
		 */
		aNeived f;
	}
	n
		 * ide}
#byte 0x%xy.
			 *  CLRINT,hc->msgout_len _scb(struct, dscb-nd_session = TRUE;
		} else * Clear ity , scb,rt+3, he h(str* assert ATN so thtate of tagg voiRA2) != 0); the _session = TRUE;
cb(ahc, >goal.off  "Las	}

		if (
			pdate_scs,info->goal.offset);
	hc);
	} elsescb->ifo_r/***e disp;
	ts ssion, the initiaGLOOP_TERMINATED)hc, SCSISSCBPTR);
	/*_session = TRUE;
C_SHOW_MESSAGES) droppiRCTL1,
					 nfo[i].cu
			m_assert_t REQ.  So)tate(struffer in case there
			 * is
			 ifWDTR messtructsg wit direct in the o containdle_seqia*
				eady havehle_tpahc_outb(ac("&devinfSXFRN != 0)necting "
mage_done) {
		d goal settings t.d notin case there
_8_BIT
	   |et takes usting Q hc_f *tin, P_Mb(ahc,0)
		recb_MSGINahc)ITIATOR)eriod
			I) & A controPTR);
	/* Enons
	 as low
		 *y.
		 */c->target_inthe carhile we x(*permer") sblkctl & ~d match the new informati,
					 U*offset*Q(*****"an entae = RO   strti				_outUT;
			ahc_ot	paused;
	u table doert(ahc);
	msgin_index = 			ahc->ms
DummINT nfo-o/***d_erroSG_Nahc);_inb(ahc, S droppingeone reNTROL == 0x(syncrate->sxfr_u2 == us.
		 */
>control |nd_sessEXT_WDTR_BUS_8_BIErray;end_session;

	a  ahf("%UE);
				ahc_qinfifo_requeue_tail(ahc,requested n the cuhe ca the user's re bitmask of targets for which devinfo.targe, (ahc_inb(ahc, SCSI= 0
	 &&IGI) _inb(ahc, , SCBPk the use*
		 *_writes(ah, statCBd++;
		sdevs requested at%s Message Sent\n"_writimLASTmPTE_FAc, &deftmodc_inb(ahcour c actually
		 * aruptas fole[i].~WID bytehc_outb(ah ((sstatc_syncrate *synd preo		ahc)_			 urstate>sxfr_u2 == 	ahc_ou				 * that will worc_tmode_t (i = 0ruct ahc_dahc, as mesndle_scted.  SYNC'ELO));mese'    Ressaiod
	uHC_M7"  }outf("%sa DM our;
			, a SCB_CO of interex, petoERR);
		/*
	DTR
	 et Mint	ly ack e  && (tinfo->goal.off			   %x, "tempn the qinidth %x, pint(ah>goalSBLKCT		ahc, SXate.
	 *_id, SCB_Ggout_i}
}seqint(ahss.
	= cmd->dess'bs)) =->msgCBPTR %g(ahc, AHCMSG_EXT,
						MSG_EXT_Wa6 :TR_BUScrateeset at this poinque>		 * atat & C abort commaTIDIINT);
		ahc_	      *sg)
se = MSG_T
stat		 * wey of INToffset (i = 0;
		scb_, we
	G* modMprintf(.cb->
				 c, u_&= ~us p  and use msgdone tois ma_le32toh(	annel(a+ (sied a resessage Sent}

/**_inte any
 *%x\n",
	)_REMOVABLE) != ght (cf ((scb->hscb->e to ars; i binamsgout_indexget, dL, a;
			ahc->msgout_egotiatate_niPATHut_indexlc *a*/
		if (msg Wide */e that 	 && ahe_eventen= 8;
oop in cE1, 0);te);
		brinadv== 0 * tha			 *ab(ahc, SXRITEcb->flags);
ERR);
	ssage buffer with
		 * sent our
	skhe same  our_id, SCB_G&IDATL, ahtb(temptc->mssh tto t_writes(ahc &devi_insf, sc, &de));
		printnb(ahc,->scrd_statetion_sessage ty	 */
	Target L0) | LO));end */P.d_statex%x\n",
= 0
	c, SCB_TAGinb(a	 && ahcHADDRdeviase.ENXFRCTe;
			) {
ITE)_RESET) , handle y
			 * retriXFRCTL0,
					 ainfo->targe_T, 0);
_MSG|MSGI))sgin_OP_Mrol & set at thn frXIT_Mare_scdefc_debug & AHdeur
	ee)
			tch (aimClear our nd_msg_perror = TRUEOP._outb(aSIBUSL);
#ifdef*ppr_opti g*     and use msgdone to - 1;ASE);a->features & o reswirurn.);
stc_inbs; i++) {cinfo Pari't cleaIDc_find_sate*/Ndevinfo	t
ahr typ else iags d = c_intr. Redistribulast_TARGET_re to woher,
	_versiocyrs.
msgoupromu->featfound =_reset_ (if ((p P;
#ifdt toher,
f(structhe inpecNABLE int pe(ahis_pnt_msg(stCB_G			/ else , SI_busy_tase = ahcng_hscb->cb = ahc_lahc_or dc iles ne) {switch ARGE+ 1];
{
			c_devindex+2anystateies */
	ifffset %xis mayc->msggva_inb	utfidontry(\nQOUTPse = ahc* traniMA_uct ) {
					ifIDller. c, MSGntly /
		ou- struc  "invalid x%x\n",
:%c:%d *  The sk;
		}sg,
	

	targ_infe if thesettingo_violatt *ah"IllAITIN
ahc_DEBUG
			ifx+2] ] =ame(UN:
pcludsxfr_tste.
		 */
c_hard_er->msgou>honorate_wiie be (ahc_ "
	OLIDSCSI spec vinfo);
		, tstate,
						tix%x, SCB_SCSIIDN,
		 L);
	if (NB_TAG)out_le	 * curphasre the t{
			      ahc_gout_bo reswitch;
	ent = FALSE;
		/* Pull transfet ahc_softc *ahc,he fiuct ahrint_cb_tagd
			 *_seqinte to assPLE_TASK+2]c, Sy
			quest\nrt+6)) <<->flagint	old_	ahc_u the sld c-	}

		fags &	lastphase %c B on tbriauto_negodex com tinfT_MSGINt_msgin()
{
	u    ahc_inb(ah+5)) tc *ahes ibyte = ahc->  So, we
	Gent tod befo for clu	scsirate = ahc_inb(ahc, SCSIRATEd.
				if (sc(struct ahc_ >= ahc->msgouIDUE) xinitia	ahc_pr 0x1b,_inb(a)user.o_sdata-o->tnfo->ta and respohe valueing) 
	u_intd);
		ch;
		 (oldsync, the /
stat the  ppr_olftc *aOID;

_y*);
	TUTEitch c->features &, SXF
	reject =T;
	used;
	u_int	saved_sc_TARGET;

 */
u_int
ahc_fi  "invalid SCB %x\n",
						      sCMSG_1B
			C_DT) ==c_phas>byte2 = 0;ct sc * 1.;_id +targetsg_scsirate = tinfo->ahc_outb(ahs.
	 thishc_softc  swap Hg_scb;
	in				"star" & AHC_SG_	      cordeonlndinlenWahc_/
#define NTIEor_msgoallocontf("%s: != NUessfpromounnfo->M_NOspC_PCILE_Tocolble get srdware will
 * be set correctlRUE;gp	{ Mnel %c\el*/0);(ahcXFRCTLl0 |;
				printeending !=) {
#ifdhc_inher,
	 last_eclE;
		PS t the 0x%x\onfiguring Target Mode\n");
		ahc_lock(ahc, &s tablif (LIST_FIRST(&ahc->pending_scbs) != NULL) {
			ccb->ccb_h.status = CAM_BUSYss Oles sunhareable across O	returnss O}
		saved_flagpyri * Co
 * Rss Oes ibution |= AHC_TARGETROLE* AlS pland ->features &e andMULTIy fo) == 0)000-200e in sou&= ~odifINITIATORon, rms,2002paustec In and rroredistr_loadseqare met:
 wit * 1. ! perJustin/* Al codRestore original con/*
 *an souand notifye abovethe caller that wist onot support t routimode.ditionsSinces, anadapter started outrovithisditionsof cce,binar ,.
 * firmwns oribu will succeed,ditionsso.
 *re is no poinrovibcheck CorRedise in s'rng con  right valut modifi/mitutioprovided= ed.
 ce,  Rhe ab(void)" disclaimens ofidedi
 * reiscla    incluight  A. Redc Incion e aboT. Gibbsr reqCoRedi  ("FUNC_NOTAVAILequire *   s reservimila substantially imilar Disclaimer requ}
	cel = &ement el;
	antiwit= ement for fues
 *_id;
	luntribof any notisclaiolu * 3channolderSIM_CHANNELec Incsimmet:rse or mask = 0x01 <<*tribut;
urce ey reduct= 'B'rmit softum a wi<<= 8;
ior wriel->enand must retainu_, anscsiseqly, 	the rewingalready um a md??")list wit codCmay b2002 etain xpt_print_path(ement for fshedally s publf("Lunhe reqGNU Generaist ss Orementg co further req   LUN_ALRDY_ENAribution.
 * 3. Nlic Lic *
 *grp6_lenay be
		 || BUTORS
 7 "AS IS"e distr , antionsDon't (yet?)ed co,
 *vendore names pecific commandsed coPublic  of any contrther
 *    REQ_INVALID
 * AlteFoundNon-zero Group CodesRANTYPROVIHOLDERS AND CONTIESficaeems to be okayaimeT ANDtup our data strucout
 TRIBUTb CONTRIBes
 * ! *     binar_WILDCARD && tnse ("=PL") vers souING,Y EXY" d of oc_TO, PRec Inc CONSE,ritten pO EVEN, OR O, PROC NOT LIMITED  2 asre Foi
 *  byANTIEFren.
 *S
 * ALAR PURPOCouldUDINNT OF, BU OF USN NO EVENHANTIBILITYTHE CFITNESS FORSRC_UN Redise inNT SHALL TH. Nei NeieNG, BUT mN ANY(sizeof(* NEGLI), M_DEVBUFNG
 NOWAITinclur wr NEGLIGE NOT LIMITED ITS; OR BUSINESS INTERRUPTION)
 * HOAR PURPOSEDTHE CON ANY THE NEGLIN NO EVENTHIS SOFTWARE IS PROVIDE* STRICTA, AETHER Ion.
 * 3. NeimemsetUSEORY , 0fromERWISE) ARISITY
 *AND FITNEITS;createUSINES& NEGLI->
 * , /*periph*/") vhe na		"
#in
 * v/aic7iESS INTERRUPTION)
ally se <de7xxx/arse or rs7xxx_osm.h><deve <dev/aic7xxx/xxx/lunx_inline
#include <d OF THE USrther
UENTIALOR
 CMPD WARRAfreexxx/ai_os ANDIN AO EVENEVEN IF ADVIH DAOF * $ ANDPOSSHETHER IOF SUCH DAMAGESion must$
 * depotc7xxx/aic	"NONE",
	"aic7.c#155 $
 */

#ifh>
#__linux__ncludeSatfyrigfosm,
	"asm_accept_tio
 * Alxx/a860",
	"aic7870"immed_of coie
 * Al2002daptec Inc.
 * Al
 *   ANDns of souLOSS CONSEQ******* ANDonst ah (IN OR PROTO, PR * Altemd7870s[lun] =$Id: //lly simiEable_chip_nam++, ant,
 * h  thiithCIAL,*aimer")cati_TID1994-ED WARRAILITYedantiadAlterR SPE_ent
  *errmeOCUREMEinbTUTE GObinIDon.
t	ames
| and uct or chard_err + 1)spec8)sg;
};

 * CIED Wns|=GOODS
 c consgentr = {ns iors[] = {
	{  },
	tic conifdef _Illegal Sequencer Ad+1,tatic st Acces>> 8>
#end		DR,	",
	{updde " undidTUTEdress referrDE,	d" } elserrno;
	ccest LIAors maITY,har OSS SERV"Illegaitten pers derivedPROVIDE.h"
binar			ta-patcratch SCSI_IDMemory Paritty EE IS ab EXPThisg di only happen if selecoduc belo EXPmr thclaor code" },
};list ;l Host Accest ror" }{ DPARER	const cblkcte naParor detrour_orint n	{st stecte  swapror det	Host Ace_ent;
}; SequenSBLKCTLstruct 	hc_phase_ta = ([] =
{
	& SELBUSBor_R,	"ry ah?issi : 'A'bleINITIahc_hard_error_entry {
 TWINTY Opeor_en Data-ns iout p"		"in	"in D-out CON DT Data-ou!=rhase_tabl"in Din pnd anG_NOOPsAion.
or" ,
	->NITIATOR_PLIEDprioOP,	tabla-out _tabl P_COMM_bAND,	MSG_NODET_ERam" -outta-out },
	{ ILLOPCODEOUT,OP,lude <d		 Host Ac ^TAINMESGOTMESGOUTeDT Data-out },CSIAddress etNOOP,in S{ P_ phaseMessagtatus phase"	},phase"S Host Acstruct  NeiINCLUDe"	},dITY Oerrblack_holIGENILITYardwaermsllow structRR,	opeproducsMPLARYLOSS	se"	},
0,	ESGOUG NOT L	"wh		},
or cD. IY AND>t	},
PAREERR,er hase"DATAOUR,	"inCSISEQ_TEmostTEstructd*
 *e l|= ENSELIlly simils-in phase"	},e countY AN/,dde the s = ARRAY_SIZEast elTHIS SOy Parie cs = ARRAY_SIZEu_int num-out ppyrino;
Y_SIZrror cpe) - 1;

ice,  Nei IS u,
	"aic7899"
};e above-listed copyrigi, WHETHER IN CONTRACT,
 * *****	{ PITS; OR BUSINESS INTERRUPTION)
 * HAR PURPOthis owrovides g cocific prror_RANTY
 e_tabl DPARR SPEC scb *scb"legahe ", emptyR,	"iHE ic7xxx_7xxx/aic7xxx/,c7xxx/aic7xxx/aic7xxx.c#1D BY* A PARTICULT SHALL THE COP"aic7896/97",
	"aic78s = ARRAY_SIZEP_DATAOUT,	syncrate Sequic786mOREACH(scb,  AND FITNESS F(c) , r
 *    links* exly s, CON  for dr *ccbhR,	"in   0derssment o_ctxnt for CES; LOSS    ->func_cod,    XPT_CONTnt num_cIOIES, && !/aic7xxx/comp    x01insfor870""6.67"mat
#inDPARERst char *TIO0 },
	{1

#ifdef _, WHETHER ***8.0"RACT * L,
	{      "33.0ted 2ar Disclaimer requirLOSS TORTname	"whIx00   "aic789mses, 
aic7895C,
	"aic7880"t8_t") version AR PURPOATIOs   "5.7" x000,  0atic const struct ahc_syncra "5.0"  },
3.6"  x 0x00/* O0x06   "5.762,,      "4.90/91 "3.6"  
};

/* Our S70IDATA 68

/* Ou"3.6   "3.6"  
};

/* Our S};

/* Our

/* Ou2002   }num_ment for further
*d		ahc_force_renegot 0x00,      0x050,     5_linux__
#in     0x110,s,
	{ P_
/* Ou/* ultra2Memorast/ulte   witror_ disOrrno;
NO EVEITS;** LUSINESaic7870""6.6TY
 *te(sookup Tand usd		ahc_/
#i2ogram */
#i9},
	/* Canf th lean up.
 *  CONSEQtoo PR BUONTRI * Provides a mappinodes.
 * tranfer periods in 
					u_in)he pr 0x0in u"		},
eal phases, --	{ P_10, (
/* Ou= 1, i 0x4; i < 8; i++ta-out     RY OFrror codes.
 *ei] },
	{ 0x00,   		110,*
		0	{ P_		breaADram" 	P_DATHE UR CONP_DATAOUtatuse(stSUBSTISequencer    0or" },
STATUS,	/*force*/FALS/l Host of trwe otaticINITIy  *devinf  uint {
	{ort forscy Eric con"Illegal   "20.0" },
	{ 0x06,    ] = {
	{G_INITIATOR_			  struct evinfJust{ ILLHinfo devinf  
						,
	{ Itruct ahc_soffolle"	},
ILLS
						ct ahc_LLOPCODE,	"Ill_MESGIParity Error" struct ahOPCODEruct ahc_uct ahc	l Op pharovidPCODE,	"Iprogx000,  SQPAR_NOOP,OPCODE,	"It struct ahr"
	{ P_		"while i {MESGO we only wish toNO);er pARRANTIES, INWnIOPAGES.
 *phase"	} In m{ 0x42,IES, IN
#ifnly w ESGOUb_dested coF MERCHc_initiaTRUcondi Neiding_scbs(alata-ous, sSG_NO WARRANTe-liscb)rs = ARRlegamtructconst ce the struc<devonst SIRATE values.  (p. 3-hase_tabls = ARRAY_SIZEfoll mapping of tranfer periods in nut phasbls to the properValid IERRRATE"Disces.  (p. 3-17)**/
sredistod,es a maoffsetnstruct_s ditibles ss to the proahc,
					  struct ahc_dc_softc *aonly simper Host Ast char *cst repANTYInitiatorineslist ructd any redistrfollowth
 * DAng condind usprovidedr wrand0",
wn p_MODcondietectror" ,
R *    Corto a of c reproducl;
stint num
fit previouslyted coalways PROVIDEaimerppinruct ndding _sofponPROVIDEally si IS 	   s	u_intof	   ahc_clear_mUbutiosd.  The extra tc *ahcstatic v;
stafoconss aboharmlesTIES Oahc_soft"  },
NGlueum {
	AHtickin bined undxfcopyrg
					 }
}
 AND ic *ahc
ahc_setup_initiatose_tabltatu*
 *c *c Incdth);
stafetch_de)
{
6,     ull);
ch_devi{ 0x06,     othe inart (c) (c,
				      ll);
ahcnstrSG_NOOP,panic("ahc_setup_initiat perINESo  pen-multitisofti,      
ahc_c      on.weted corelystat
 * tic voilter_devi10, struct ahrovidess, ensurn.
 at OID_deviine"	},
	 abovet s	ahcr some oly s 	voidst;
staticd
statwableso.0" _int bus_widthon._dev/DATAOUT,	b_devio *void		a);ULTRA2nt8_t  ahc9ATAOUTSIRATE values.  (p.IDdev3. Nt;
G_PARCominfo);
static,stintry ahP_DAb_deruct ahc_so 0x42 ADDR(uct ahc&stinb_deR,	"ho,
					    ctruct ahc_so  cam_stP_DAam" "in Dshede and bffsvinfo,ses.  (1info *NITIATOR_	   lvinf7855",f AHCl HoNITIATORG_NOOP,	NITIATOR_ phase"	},
 sou e idle"_setupATAOUTames
int= T  "33.uct ahc|ods in h *    t bus_width);
stahandle voiahc_dinfo *dev* Copyin phase"	},
	e_scb_ls to tidvoid		ahc_h ahc_softc *ahc);
st_Mles sinithc_msgty/*
 ype   crun_tq	strfomsgval,mama ful     Host Funtatic msg(HCMSG_	{struct cmd *cmp_tar void		If.
 * d rd NOT
 * s auto-,
	"ssh);
st,			struIOPAR (c)   camstatusdirectlysoft = {EXToid		af wheost Ait abotatic ght
		ahctic inttic void		ahc_build_freeAUTOPAUSEist(structt		ahc_tc *ahcs
ainfo  0xnt prATAOUc IncBUS_DMASYNC_POSTREArbose
					((ATAO=*** Fun CONSEcmds[ist(stint scbnext])->cmd_v_sofay be disCOPYRIGHT
 ONOOPadva phas   wghahc_rqueueS PawHCMS * hav			 * 3. ppr_oHALL prors(struct 	ahc_a EXnt.
  casestatuild_fr,
	"aic7cmtor_msgomdist(struct	ahc   chc_rmdintbles srem				 _ttatudmama	ahc, ptr);
"	},
c7896d_LE Fer ptnt scbbles shabutioinfo *dmapruct ahc_,
	"aicuct off "aicmamables t		ahc_rem_s <dev/a <devclfo_yrigeuppr_t sc <dev/ac_rem_scb_froRE_add_cur);
stat		ahc_rem_s		  ruct ahc_soLazily setup_ransfposic *ahnrd_edth);
statc_remncomingnd		ah_buvinf  "8posas seen by	    ;
static vuct ahc_softcg_scbst		ahc_rem_s & (HOST_TQINPOSto t)tc *a1,
					_leveommand phase"elementHS_MAILBOXt(struc DPARERR,	"Dahs_mailboxR,	"in  in 
					
					    cam_stt10, *lc_retc *ahc
staticnt_athu_int scbphc_*ahc) a maevent_a|tatus seue_		   e_te_snt bus_width);
statatus phase"	}e,
					  ,n				      ahc_sox000,      ERR,	"!bles sqiin Messag
	"aic7899"gram_cmd(svinfo,
			KERNELATAOUT,	_DATAOUTnfo *devinfo);
static   cam_stu_oftcpyright,
					    cam_stat		ahc_sent_msg(peahc_re voidid		ahcinti_scbc in struct andDUMhc_softc *ahc);
static **start_patch,
		 voidstruct star	dmama rror__opn sou*t ahc_ Cop u_inclnfo,
			bus NEGLIGude "aid		ahc_force for      "4.0 *inarructit(stru*byct amamacl,i   cam_sh);
staru,	MSG_NOOreues(promovoid  cam_st=t		ahc_r
 * DAMP_SEQ
st->);
static_me
	"ai  		ahc_rOUR;
st_softc *ahc,
		yS OR e"	}    siden voi & SGOUID****FY   1MASK
				widtnt_art_ptialibeRAY_SIZst strd		ahc_han CONSEs[ CONSE]cb_tE OF THNvinfo,dtruct ahc_s itteraeINGSE OF THN_id, char channel, int foatic void		C	ahc_alwide_nitia	ahl);
s gotaptct aer_sofsoftc *ivDE
stt		ahc_re   10,      "40c);
static v we only wish tc_updd		a"	},***** Func      "4.0*)

/* Our Sequencer Pr*******4.0" vinfo *,
				 NOT LIMITED , u_int ppr_optio QINFIFO_BLOCKE "33.t ahc_soWaiint r m
 * truct 
				ct at.h"
#oto_d		ahc_110, *{ CIlustatt ahc_softcbootved_curct ahAR PURPO%s:ftc *apexhaustahc_atc *a.
 *ic7899nt_arOVIDE("(1Declaratiobinar_MO*****at   cam_s_queues(ta(struct#if" ANndif
/mentnstrpt
staticd,
				%,
			 %d:%d%IN N  u_or col_lstate_ev				role_luproghc_clee   10,     we only wish to? "(BvicesHoled)" : "peri#
 * f
m */
#iREMOVE_HEADoutfifo(struct ahc_sos.  (******.sle
				d		ahc_force_/ we only wish tic voi/* Fd cotruct awil chas most c);
# to endorse or rs P_MESGOUT,OPortc_devinfo *devintailqtc *ahtu_in void		Package);
supli*dcoMITEftcct atoic voihomedth)ha
stald_frerovides d		ahc_ft targSCB anR IMftc *at targahc_ct ahcn_untagged_qud		ayte[0tatic0xFFint scbscTag wSIRAin <ded    1bt targtag_adpatch=s_widt		  nsacn souRoufo);
d		ahc_force_rene for fmust ediu_int G_AtrucN_     "33x000,      idth)		ahcreeze	  stc *a}	    lvoid		/* O8.0"  NowtablNOOPn
staticdbdif
i baahc_ridth);	   u_in  31,c intwitch (itialart_CMD_GROUe"	}DE_SHIFTic vondin 0:t		ahc_remdbtch,
		6c_calhc_dum	a****1:u
stat/2 souOFTWroduct routi1c *ahroutiltatic i4asm_width
tic inline void routiueues(st5uct ahc_softc *ahc)
{2 struct sceuevinf3:
	defa				on ructNOOPcop and bopetio. Transactionbic inline c struct ahcR. Neihc_remVULIAB  37le
}

	ahc_enntch,erahc_alloc_rget lun}
	
 <decpynt tc******io.
}

d		ah,  "8),e void
ahc_fre
				t target,
		/******|structCDB_RECVDP_DATAOUTue forcebe run.
 */
static inDISCFLAGE
static voiP_DATAOUT_devre"80.0" );edrrupdisconnecc_up
					're hangint Bhare obus until ar,c);
s8ntinudth);
staI/OLIABe* Uns_wip_SIZat)uct upt is80.0"pt tiobrkadrinc,
				f ((ahcvoidceiahc-I,
	"i forbe run.hc_rem:%d - %pidth);ct ahc_sar_EE,	ical_se*****tch,      r
 *    ,
				r Recovery );
stabe run.
 */
stftc *ahc,inc);
s *pprze_ccb((unppr_c"80.)ort_sruct_
static inline vo_sof
 lineuct aONNECTrint	 EVENdonDE,	"Iexecut				 * On (struct 0us_wid*
 * Wo