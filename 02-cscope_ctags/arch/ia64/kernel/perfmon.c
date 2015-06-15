/*
 * This file implements the perfmon-2 subsystem which is used
 * to program the IA-64 Performance Monitoring Unit (PMU).
 *
 * The initial version of perfmon.c was written by
 * Ganesh Venkitachalam, IBM Corp.
 *
 * Then it was modified for perfmon-1.x by Stephane Eranian and
 * David Mosberger, Hewlett Packard Co.
 *
 * Version Perfmon-2.x is a rewrite of perfmon-1.x
 * by Stephane Eranian, Hewlett Packard Co.
 *
 * Copyright (C) 1999-2005  Hewlett Packard Co
 *               Stephane Eranian <eranian@hpl.hp.com>
 *               David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * More information about perfmon available at:
 * 	http://www.hpl.hp.com/research/linux/perfmon
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/list.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/vfs.h>
#include <linux/smp.h>
#include <linux/pagemap.h>
#include <linux/mount.h>
#include <linux/bitops.h>
#include <linux/capability.h>
#include <linux/rcupdate.h>
#include <linux/completion.h>
#include <linux/tracehook.h>

#include <asm/errno.h>
#include <asm/intrinsics.h>
#include <asm/page.h>
#include <asm/perfmon.h>
#include <asm/processor.h>
#include <asm/signal.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/delay.h>

#ifdef CONFIG_PERFMON
/*
 * perfmon context state
 */
#define PFM_CTX_UNLOADED	1	/* context is not loaded onto any task */
#define PFM_CTX_LOADED		2	/* context is loaded onto a task */
#define PFM_CTX_MASKED		3	/* context is loaded but monitoring is masked due to overflow */
#define PFM_CTX_ZOMBIE		4	/* owner of the context is closing it */

#define PFM_INVALID_ACTIVATION	(~0UL)

#define PFM_NUM_PMC_REGS	64	/* PMC save area for ctxsw */
#define PFM_NUM_PMD_REGS	64	/* PMD save area for ctxsw */

/*
 * depth of message queue
 */
#define PFM_MAX_MSGS		32
#define PFM_CTXQ_EMPTY(g)	((g)->ctx_msgq_head == (g)->ctx_msgq_tail)

/*
 * type of a PMU register (bitmask).
 * bitmask structure:
 * 	bit0   : register implemented
 * 	bit1   : end marker
 * 	bit2-3 : reserved
 * 	bit4   : pmc has pmc.pm
 * 	bit5   : pmc controls a counter (has pmc.oi), pmd is used as counter
 * 	bit6-7 : register type
 * 	bit8-31: reserved
 */
#define PFM_REG_NOTIMPL		0x0 /* not implemented at all */
#define PFM_REG_IMPL		0x1 /* register implemented */
#define PFM_REG_END		0x2 /* end marker */
#define PFM_REG_MONITOR		(0x1<<4|PFM_REG_IMPL) /* a PMC with a pmc.pm field only */
#define PFM_REG_COUNTING	(0x2<<4|PFM_REG_MONITOR) /* a monitor + pmc.oi+ PMD used as a counter */
#define PFM_REG_CONTROL		(0x4<<4|PFM_REG_IMPL) /* PMU control register */
#define	PFM_REG_CONFIG		(0x8<<4|PFM_REG_IMPL) /* configuration register */
#define PFM_REG_BUFFER	 	(0xc<<4|PFM_REG_IMPL) /* PMD used as buffer */

#define PMC_IS_LAST(i)	(pmu_conf->pmc_desc[i].type & PFM_REG_END)
#define PMD_IS_LAST(i)	(pmu_conf->pmd_desc[i].type & PFM_REG_END)

#define PMC_OVFL_NOTIFY(ctx, i)	((ctx)->ctx_pmds[i].flags &  PFM_REGFL_OVFL_NOTIFY)

/* i assumed unsigned */
#define PMC_IS_IMPL(i)	  (i< PMU_MAX_PMCS && (pmu_conf->pmc_desc[i].type & PFM_REG_IMPL))
#define PMD_IS_IMPL(i)	  (i< PMU_MAX_PMDS && (pmu_conf->pmd_desc[i].type & PFM_REG_IMPL))

/* XXX: these assume that register i is implemented */
#define PMD_IS_COUNTING(i) ((pmu_conf->pmd_desc[i].type & PFM_REG_COUNTING) == PFM_REG_COUNTING)
#define PMC_IS_COUNTING(i) ((pmu_conf->pmc_desc[i].type & PFM_REG_COUNTING) == PFM_REG_COUNTING)
#define PMC_IS_MONITOR(i)  ((pmu_conf->pmc_desc[i].type & PFM_REG_MONITOR)  == PFM_REG_MONITOR)
#define PMC_IS_CONTROL(i)  ((pmu_conf->pmc_desc[i].type & PFM_REG_CONTROL)  == PFM_REG_CONTROL)

#define PMC_DFL_VAL(i)     pmu_conf->pmc_desc[i].default_value
#define PMC_RSVD_MASK(i)   pmu_conf->pmc_desc[i].reserved_mask
#define PMD_PMD_DEP(i)	   pmu_conf->pmd_desc[i].dep_pmd[0]
#define PMC_PMD_DEP(i)	   pmu_conf->pmc_desc[i].dep_pmd[0]

#define PFM_NUM_IBRS	  IA64_NUM_DBG_REGS
#define PFM_NUM_DBRS	  IA64_NUM_DBG_REGS

#define CTX_OVFL_NOBLOCK(c)	((c)->ctx_fl_block == 0)
#define CTX_HAS_SMPL(c)		((c)->ctx_fl_is_sampling)
#define PFM_CTX_TASK(h)		(h)->ctx_task

#define PMU_PMC_OI		5 /* position of pmc.oi bit */

/* XXX: does not support more than 64 PMDs */
#define CTX_USED_PMD(ctx, mask) (ctx)->ctx_used_pmds[0] |= (mask)
#define CTX_IS_USED_PMD(ctx, c) (((ctx)->ctx_used_pmds[0] & (1UL << (c))) != 0UL)

#define CTX_USED_MONITOR(ctx, mask) (ctx)->ctx_used_monitors[0] |= (mask)

#define CTX_USED_IBR(ctx,n) 	(ctx)->ctx_used_ibrs[(n)>>6] |= 1UL<< ((n) % 64)
#define CTX_USED_DBR(ctx,n) 	(ctx)->ctx_used_dbrs[(n)>>6] |= 1UL<< ((n) % 64)
#define CTX_USES_DBREGS(ctx)	(((pfm_context_t *)(ctx))->ctx_fl_using_dbreg==1)
#define PFM_CODE_RR	0	/* requesting code range restriction */
#define PFM_DATA_RR	1	/* requestion data range restriction */

#define PFM_CPUINFO_CLEAR(v)	pfm_get_cpu_var(pfm_syst_info) &= ~(v)
#define PFM_CPUINFO_SET(v)	pfm_get_cpu_var(pfm_syst_info) |= (v)
#define PFM_CPUINFO_GET()	pfm_get_cpu_var(pfm_syst_info)

#define RDEP(x)	(1UL<<(x))

/*
 * context protection macros
 * in SMP:
 * 	- we need to protect against CPU concurrency (spin_lock)
 * 	- we need to protect against PMU overflow interrupts (local_irq_disable)
 * in UP:
 * 	- we need to protect against PMU overflow interrupts (local_irq_disable)
 *
 * spin_lock_irqsave()/spin_unlock_irqrestore():
 * 	in SMP: local_irq_disable + spin_lock
 * 	in UP : local_irq_disable
 *
 * spin_lock()/spin_lock():
 * 	in UP : removed automatically
 * 	in SMP: protect against context accesses from other CPU. interrupts
 * 	        are not masked. This is useful for the PMU interrupt handler
 * 	        because we know we will not get PMU concurrency in that code.
 */
#define PROTECT_CTX(c, f) \
	do {  \
		DPRINT(("spinlock_irq_save ctx %p by [%d]\n", c, task_pid_nr(current))); \
		spin_lock_irqsave(&(c)->ctx_lock, f); \
		DPRINT(("spinlocked ctx %p  by [%d]\n", c, task_pid_nr(current))); \
	} while(0)

#define UNPROTECT_CTX(c, f) \
	do { \
		DPRINT(("spinlock_irq_restore ctx %p by [%d]\n", c, task_pid_nr(current))); \
		spin_unlock_irqrestore(&(c)->ctx_lock, f); \
	} while(0)

#define PROTECT_CTX_NOPRINT(c, f) \
	do {  \
		spin_lock_irqsave(&(c)->ctx_lock, f); \
	} while(0)


#define UNPROTECT_CTX_NOPRINT(c, f) \
	do { \
		spin_unlock_irqrestore(&(c)->ctx_lock, f); \
	} while(0)


#define PROTECT_CTX_NOIRQ(c) \
	do {  \
		spin_lock(&(c)->ctx_lock); \
	} while(0)

#define UNPROTECT_CTX_NOIRQ(c) \
	do { \
		spin_unlock(&(c)->ctx_lock); \
	} while(0)


#ifdef CONFIG_SMP

#define GET_ACTIVATION()	pfm_get_cpu_var(pmu_activation_number)
#define INC_ACTIVATION()	pfm_get_cpu_var(pmu_activation_number)++
#define SET_ACTIVATION(c)	(c)->ctx_last_activation = GET_ACTIVATION()

#else /* !CONFIG_SMP */
#define SET_ACTIVATION(t) 	do {} while(0)
#define GET_ACTIVATION(t) 	do {} while(0)
#define INC_ACTIVATION(t) 	do {} while(0)
#endif /* CONFIG_SMP */

#define SET_PMU_OWNER(t, c)	do { pfm_get_cpu_var(pmu_owner) = (t); pfm_get_cpu_var(pmu_ctx) = (c); } while(0)
#define GET_PMU_OWNER()		pfm_get_cpu_var(pmu_owner)
#define GET_PMU_CTX()		pfm_get_cpu_var(pmu_ctx)

#define LOCK_PFS(g)	    	spin_lock_irqsave(&pfm_sessions.pfs_lock, g)
#define UNLOCK_PFS(g)	    	spin_unlock_irqrestore(&pfm_sessions.pfs_lock, g)

#define PFM_REG_RETFLAG_SET(flags, val)	do { flags &= ~PFM_REG_RETFL_MASK; flags |= (val); } while(0)

/*
 * cmp0 must be the value of pmc0
 */
#define PMC0_HAS_OVFL(cmp0)  (cmp0 & ~0x1UL)

#define PFMFS_MAGIC 0xa0b4d889

/*
 * debugging
 */
#define PFM_DEBUGGING 1
#ifdef PFM_DEBUGGING
#define DPRINT(a) \
	do { \
		if (unlikely(pfm_sysctl.debug >0)) { printk("%s.%d: CPU%d [%d] ", __func__, __LINE__, smp_processor_id(), task_pid_nr(current)); printk a; } \
	} while (0)

#define DPRINT_ovfl(a) \
	do { \
		if (unlikely(pfm_sysctl.debug > 0 && pfm_sysctl.debug_ovfl >0)) { printk("%s.%d: CPU%d [%d] ", __func__, __LINE__, smp_processor_id(), task_pid_nr(current)); printk a; } \
	} while (0)
#endif

/*
 * 64-bit software counter structure
 *
 * the next_reset_type is applied to the next call to pfm_reset_regs()
 */
typedef struct {
	unsigned long	val;		/* virtual 64bit counter value */
	unsigned long	lval;		/* last reset value */
	unsigned long	long_reset;	/* reset value on sampling overflow */
	unsigned long	short_reset;    /* reset value on overflow */
	unsigned long	reset_pmds[4];  /* which other pmds to reset when this counter overflows */
	unsigned long	smpl_pmds[4];   /* which pmds are accessed when counter overflow */
	unsigned long	seed;		/* seed for random-number generator */
	unsigned long	mask;		/* mask for random-number generator */
	unsigned int 	flags;		/* notify/do not notify */
	unsigned long	eventid;	/* overflow event identifier */
} pfm_counter_t;

/*
 * context flags
 */
typedef struct {
	unsigned int block:1;		/* when 1, task will blocked on user notifications */
	unsigned int system:1;		/* do system wide monitoring */
	unsigned int using_dbreg:1;	/* using range restrictions (debug registers) */
	unsigned int is_sampling:1;	/* true if using a custom format */
	unsigned int excl_idle:1;	/* exclude idle task in system wide session */
	unsigned int going_zombie:1;	/* context is zombie (MASKED+blocking) */
	unsigned int trap_reason:2;	/* reason for going into pfm_handle_work() */
	unsigned int no_msg:1;		/* no message sent on overflow */
	unsigned int can_restart:1;	/* allowed to issue a PFM_RESTART */
	unsigned int reserved:22;
} pfm_context_flags_t;

#define PFM_TRAP_REASON_NONE		0x0	/* default value */
#define PFM_TRAP_REASON_BLOCK		0x1	/* we need to block on overflow */
#define PFM_TRAP_REASON_RESET		0x2	/* we need to reset PMDs */


/*
 * perfmon context: encapsulates all the state of a monitoring session
 */

typedef struct pfm_context {
	spinlock_t		ctx_lock;		/* context protection */

	pfm_context_flags_t	ctx_flags;		/* bitmask of flags  (block reason incl.) */
	unsigned int		ctx_state;		/* state: active/inactive (no bitfield) */

	struct task_struct 	*ctx_task;		/* task to which context is attached */

	unsigned long		ctx_ovfl_regs[4];	/* which registers overflowed (notification) */

	struct completion	ctx_restart_done;  	/* use for blocking notification mode */

	unsigned long		ctx_used_pmds[4];	/* bitmask of PMD used            */
	unsigned long		ctx_all_pmds[4];	/* bitmask of all accessible PMDs */
	unsigned long		ctx_reload_pmds[4];	/* bitmask of force reload PMD on ctxsw in */

	unsigned long		ctx_all_pmcs[4];	/* bitmask of all accessible PMCs */
	unsigned long		ctx_reload_pmcs[4];	/* bitmask of force reload PMC on ctxsw in */
	unsigned long		ctx_used_monitors[4];	/* bitmask of monitor PMC being used */

	unsigned long		ctx_pmcs[PFM_NUM_PMC_REGS];	/*  saved copies of PMC values */

	unsigned int		ctx_used_ibrs[1];		/* bitmask of used IBR (speedup ctxsw in) */
	unsigned int		ctx_used_dbrs[1];		/* bitmask of used DBR (speedup ctxsw in) */
	unsigned long		ctx_dbrs[IA64_NUM_DBG_REGS];	/* DBR values (cache) when not loaded */
	unsigned long		ctx_ibrs[IA64_NUM_DBG_REGS];	/* IBR values (cache) when not loaded */

	pfm_counter_t		ctx_pmds[PFM_NUM_PMD_REGS]; /* software state for PMDS */

	unsigned long		th_pmcs[PFM_NUM_PMC_REGS];	/* PMC thread save state */
	unsigned long		th_pmds[PFM_NUM_PMD_REGS];	/* PMD thread save state */

	unsigned long		ctx_saved_psr_up;	/* only contains psr.up value */

	unsigned long		ctx_last_activation;	/* context last activation number for last_cpu */
	unsigned int		ctx_last_cpu;		/* CPU id of current or last CPU used (SMP only) */
	unsigned int		ctx_cpu;		/* cpu to which perfmon is applied (system wide) */

	int			ctx_fd;			/* file descriptor used my this context */
	pfm_ovfl_arg_t		ctx_ovfl_arg;		/* argument to custom buffer format handler */

	pfm_buffer_fmt_t	*ctx_buf_fmt;		/* buffer format callbacks */
	void			*ctx_smpl_hdr;		/* points to sampling buffer header kernel vaddr */
	unsigned long		ctx_smpl_size;		/* size of sampling buffer */
	void			*ctx_smpl_vaddr;	/* user level virtual address of smpl buffer */

	wait_queue_head_t 	ctx_msgq_wait;
	pfm_msg_t		ctx_msgq[PFM_MAX_MSGS];
	int			ctx_msgq_head;
	int			ctx_msgq_tail;
	struct fasync_struct	*ctx_async_queue;

	wait_queue_head_t 	ctx_zombieq;		/* termination cleanup wait queue */
} pfm_context_t;

/*
 * magic number used to verify that structure is really
 * a perfmon context
 */
#define PFM_IS_FILE(f)		((f)->f_op == &pfm_file_ops)

#define PFM_GET_CTX(t)	 	((pfm_context_t *)(t)->thread.pfm_context)

#ifdef CONFIG_SMP
#define SET_LAST_CPU(ctx, v)	(ctx)->ctx_last_cpu = (v)
#define GET_LAST_CPU(ctx)	(ctx)->ctx_last_cpu
#else
#define SET_LAST_CPU(ctx, v)	do {} while(0)
#define GET_LAST_CPU(ctx)	do {} while(0)
#endif


#define ctx_fl_block		ctx_flags.block
#define ctx_fl_system		ctx_flags.system
#define ctx_fl_using_dbreg	ctx_flags.using_dbreg
#define ctx_fl_is_sampling	ctx_flags.is_sampling
#define ctx_fl_excl_idle	ctx_flags.excl_idle
#define ctx_fl_going_zombie	ctx_flags.going_zombie
#define ctx_fl_trap_reason	ctx_flags.trap_reason
#define ctx_fl_no_msg		ctx_flags.no_msg
#define ctx_fl_can_restart	ctx_flags.can_restart

#define PFM_SET_WORK_PENDING(t, v)	do { (t)->thread.pfm_needs_checking = v; } while(0);
#define PFM_GET_WORK_PENDING(t)		(t)->thread.pfm_needs_checking

/*
 * global information about all sessions
 * mostly used to synchronize between system wide and per-process
 */
typedef struct {
	spinlock_t		pfs_lock;		   /* lock the structure */

	unsigned int		pfs_task_sessions;	   /* number of per task sessions */
	unsigned int		pfs_sys_sessions;	   /* number of per system wide sessions */
	unsigned int		pfs_sys_use_dbregs;	   /* incremented when a system wide session uses debug regs */
	unsigned int		pfs_ptrace_use_dbregs;	   /* incremented when a process uses debug regs */
	struct task_struct	*pfs_sys_session[NR_CPUS]; /* point to task owning a system-wide session */
} pfm_session_t;

/*
 * information about a PMC or PMD.
 * dep_pmd[]: a bitmask of dependent PMD registers
 * dep_pmc[]: a bitmask of dependent PMC registers
 */
typedef int (*pfm_reg_check_t)(struct task_struct *task, pfm_context_t *ctx, unsigned int cnum, unsigned long *val, struct pt_regs *regs);
typedef struct {
	unsigned int		type;
	int			pm_pos;
	unsigned long		default_value;	/* power-on default value */
	unsigned long		reserved_mask;	/* bitmask of reserved bits */
	pfm_reg_check_t		read_check;
	pfm_reg_check_t		write_check;
	unsigned long		dep_pmd[4];
	unsigned long		dep_pmc[4];
} pfm_reg_desc_t;

/* assume cnum is a valid monitor */
#define PMC_PM(cnum, val)	(((val) >> (pmu_conf->pmc_desc[cnum].pm_pos)) & 0x1)

/*
 * This structure is initialized at boot time and contains
 * a description of the PMU main characteristics.
 *
 * If the probe function is defined, detection is based
 * on its return value: 
 * 	- 0 means recognized PMU
 * 	- anything else means not supported
 * When the probe function is not defined, then the pmu_family field
 * is used and it must match the host CPU family such that:
 * 	- cpu->family & config->pmu_family != 0
 */
typedef struct {
	unsigned long  ovfl_val;	/* overflow value for counters */

	pfm_reg_desc_t *pmc_desc;	/* detailed PMC register dependencies descriptions */
	pfm_reg_desc_t *pmd_desc;	/* detailed PMD register dependencies descriptions */

	unsigned int   num_pmcs;	/* number of PMCS: computed at init time */
	unsigned int   num_pmds;	/* number of PMDS: computed at init time */
	unsigned long  impl_pmcs[4];	/* bitmask of implemented PMCS */
	unsigned long  impl_pmds[4];	/* bitmask of implemented PMDS */

	char	      *pmu_name;	/* PMU family name */
	unsigned int  pmu_family;	/* cpuid family pattern used to identify pmu */
	unsigned int  flags;		/* pmu specific flags */
	unsigned int  num_ibrs;		/* number of IBRS: computed at init time */
	unsigned int  num_dbrs;		/* number of DBRS: computed at init time */
	unsigned int  num_counters;	/* PMC/PMD counting pairs : computed at init time */
	int           (*probe)(void);   /* customized probe routine */
	unsigned int  use_rr_dbregs:1;	/* set if debug registers used for range restriction */
} pmu_config_t;
/*
 * PMU specific flags
 */
#define PFM_PMU_IRQ_RESEND	1	/* PMU needs explicit IRQ resend */

/*
 * debug register related type definitions
 */
typedef struct {
	unsigned long ibr_mask:56;
	unsigned long ibr_plm:4;
	unsigned long ibr_ig:3;
	unsigned long ibr_x:1;
} ibr_mask_reg_t;

typedef struct {
	unsigned long dbr_mask:56;
	unsigned long dbr_plm:4;
	unsigned long dbr_ig:2;
	unsigned long dbr_w:1;
	unsigned long dbr_r:1;
} dbr_mask_reg_t;

typedef union {
	unsigned long  val;
	ibr_mask_reg_t ibr;
	dbr_mask_reg_t dbr;
} dbreg_t;


/*
 * perfmon command descriptions
 */
typedef struct {
	int		(*cmd_func)(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs);
	char		*cmd_name;
	int		cmd_flags;
	unsigned int	cmd_narg;
	size_t		cmd_argsize;
	int		(*cmd_getsize)(void *arg, size_t *sz);
} pfm_cmd_desc_t;

#define PFM_CMD_FD		0x01	/* command requires a file descriptor */
#define PFM_CMD_ARG_READ	0x02	/* command must read argument(s) */
#define PFM_CMD_ARG_RW		0x04	/* command must read/write argument(s) */
#define PFM_CMD_STOP		0x08	/* command does not work on zombie context */


#define PFM_CMD_NAME(cmd)	pfm_cmd_tab[(cmd)].cmd_name
#define PFM_CMD_READ_ARG(cmd)	(pfm_cmd_tab[(cmd)].cmd_flags & PFM_CMD_ARG_READ)
#define PFM_CMD_RW_ARG(cmd)	(pfm_cmd_tab[(cmd)].cmd_flags & PFM_CMD_ARG_RW)
#define PFM_CMD_USE_FD(cmd)	(pfm_cmd_tab[(cmd)].cmd_flags & PFM_CMD_FD)
#define PFM_CMD_STOPPED(cmd)	(pfm_cmd_tab[(cmd)].cmd_flags & PFM_CMD_STOP)

#define PFM_CMD_ARG_MANY	-1 /* cannot be zero */

typedef struct {
	unsigned long pfm_spurious_ovfl_intr_count;	/* keep track of spurious ovfl interrupts */
	unsigned long pfm_replay_ovfl_intr_count;	/* keep track of replayed ovfl interrupts */
	unsigned long pfm_ovfl_intr_count; 		/* keep track of ovfl interrupts */
	unsigned long pfm_ovfl_intr_cycles;		/* cycles spent processing ovfl interrupts */
	unsigned long pfm_ovfl_intr_cycles_min;		/* min cycles spent processing ovfl interrupts */
	unsigned long pfm_ovfl_intr_cycles_max;		/* max cycles spent processing ovfl interrupts */
	unsigned long pfm_smpl_handler_calls;
	unsigned long pfm_smpl_handler_cycles;
	char pad[SMP_CACHE_BYTES] ____cacheline_aligned;
} pfm_stats_t;

/*
 * perfmon internal variables
 */
static pfm_stats_t		pfm_stats[NR_CPUS];
static pfm_session_t		pfm_sessions;	/* global sessions information */

static DEFINE_SPINLOCK(pfm_alt_install_check);
static pfm_intr_handler_desc_t  *pfm_alt_intr_handler;

static struct proc_dir_entry 	*perfmon_dir;
static pfm_uuid_t		pfm_null_uuid = {0,};

static spinlock_t		pfm_buffer_fmt_lock;
static LIST_HEAD(pfm_buffer_fmt_list);

static pmu_config_t		*pmu_conf;

/* sysctl() controls */
pfm_sysctl_t pfm_sysctl;
EXPORT_SYMBOL(pfm_sysctl);

static ctl_table pfm_ctl_table[]={
	{
		.ctl_name	= CTL_UNNUMBERED,
		.procname	= "debug",
		.data		= &pfm_sysctl.debug,
		.maxlen		= sizeof(int),
		.mode		= 0666,
		.proc_handler	= &proc_dointvec,
	},
	{
		.ctl_name	= CTL_UNNUMBERED,
		.procname	= "debug_ovfl",
		.data		= &pfm_sysctl.debug_ovfl,
		.maxlen		= sizeof(int),
		.mode		= 0666,
		.proc_handler	= &proc_dointvec,
	},
	{
		.ctl_name	= CTL_UNNUMBERED,
		.procname	= "fastctxsw",
		.data		= &pfm_sysctl.fastctxsw,
		.maxlen		= sizeof(int),
		.mode		= 0600,
		.proc_handler	=  &proc_dointvec,
	},
	{
		.ctl_name	= CTL_UNNUMBERED,
		.procname	= "expert_mode",
		.data		= &pfm_sysctl.expert_mode,
		.maxlen		= sizeof(int),
		.mode		= 0600,
		.proc_handler	= &proc_dointvec,
	},
	{}
};
static ctl_table pfm_sysctl_dir[] = {
	{
		.ctl_name	= CTL_UNNUMBERED,
		.procname	= "perfmon",
		.mode		= 0555,
		.child		= pfm_ctl_table,
	},
 	{}
};
static ctl_table pfm_sysctl_root[] = {
	{
		.ctl_name	= CTL_KERN,
		.procname	= "kernel",
		.mode		= 0555,
		.child		= pfm_sysctl_dir,
	},
 	{}
};
static struct ctl_table_header *pfm_sysctl_header;

static int pfm_context_unload(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs);

#define pfm_get_cpu_var(v)		__ia64_per_cpu_var(v)
#define pfm_get_cpu_data(a,b)		per_cpu(a, b)

static inline void
pfm_put_task(struct task_struct *task)
{
	if (task != current) put_task_struct(task);
}

static inline void
pfm_reserve_page(unsigned long a)
{
	SetPageReserved(vmalloc_to_page((void *)a));
}
static inline void
pfm_unreserve_page(unsigned long a)
{
	ClearPageReserved(vmalloc_to_page((void*)a));
}

static inline unsigned long
pfm_protect_ctx_ctxsw(pfm_context_t *x)
{
	spin_lock(&(x)->ctx_lock);
	return 0UL;
}

static inline void
pfm_unprotect_ctx_ctxsw(pfm_context_t *x, unsigned long f)
{
	spin_unlock(&(x)->ctx_lock);
}

static inline unsigned int
pfm_do_munmap(struct mm_struct *mm, unsigned long addr, size_t len, int acct)
{
	return do_munmap(mm, addr, len);
}

static inline unsigned long 
pfm_get_unmapped_area(struct file *file, unsigned long addr, unsigned long len, unsigned long pgoff, unsigned long flags, unsigned long exec)
{
	return get_unmapped_area(file, addr, len, pgoff, flags);
}


static int
pfmfs_get_sb(struct file_system_type *fs_type, int flags, const char *dev_name, void *data,
	     struct vfsmount *mnt)
{
	return get_sb_pseudo(fs_type, "pfm:", NULL, PFMFS_MAGIC, mnt);
}

static struct file_system_type pfm_fs_type = {
	.name     = "pfmfs",
	.get_sb   = pfmfs_get_sb,
	.kill_sb  = kill_anon_super,
};

DEFINE_PER_CPU(unsigned long, pfm_syst_info);
DEFINE_PER_CPU(struct task_struct *, pmu_owner);
DEFINE_PER_CPU(pfm_context_t  *, pmu_ctx);
DEFINE_PER_CPU(unsigned long, pmu_activation_number);
EXPORT_PER_CPU_SYMBOL_GPL(pfm_syst_info);


/* forward declaration */
static const struct file_operations pfm_file_ops;

/*
 * forward declarations
 */
#ifndef CONFIG_SMP
static void pfm_lazy_save_regs (struct task_struct *ta);
#endif

void dump_pmu_state(const char *);
static int pfm_write_ibr_dbr(int mode, pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs);

#include "perfmon_itanium.h"
#include "perfmon_mckinley.h"
#include "perfmon_montecito.h"
#include "perfmon_generic.h"

static pmu_config_t *pmu_confs[]={
	&pmu_conf_mont,
	&pmu_conf_mck,
	&pmu_conf_ita,
	&pmu_conf_gen, /* must be last */
	NULL
};


static int pfm_end_notify_user(pfm_context_t *ctx);

static inline void
pfm_clear_psr_pp(void)
{
	ia64_rsm(IA64_PSR_PP);
	ia64_srlz_i();
}

static inline void
pfm_set_psr_pp(void)
{
	ia64_ssm(IA64_PSR_PP);
	ia64_srlz_i();
}

static inline void
pfm_clear_psr_up(void)
{
	ia64_rsm(IA64_PSR_UP);
	ia64_srlz_i();
}

static inline void
pfm_set_psr_up(void)
{
	ia64_ssm(IA64_PSR_UP);
	ia64_srlz_i();
}

static inline unsigned long
pfm_get_psr(void)
{
	unsigned long tmp;
	tmp = ia64_getreg(_IA64_REG_PSR);
	ia64_srlz_i();
	return tmp;
}

static inline void
pfm_set_psr_l(unsigned long val)
{
	ia64_setreg(_IA64_REG_PSR_L, val);
	ia64_srlz_i();
}

static inline void
pfm_freeze_pmu(void)
{
	ia64_set_pmc(0,1UL);
	ia64_srlz_d();
}

static inline void
pfm_unfreeze_pmu(void)
{
	ia64_set_pmc(0,0UL);
	ia64_srlz_d();
}

static inline void
pfm_restore_ibrs(unsigned long *ibrs, unsigned int nibrs)
{
	int i;

	for (i=0; i < nibrs; i++) {
		ia64_set_ibr(i, ibrs[i]);
		ia64_dv_serialize_instruction();
	}
	ia64_srlz_i();
}

static inline void
pfm_restore_dbrs(unsigned long *dbrs, unsigned int ndbrs)
{
	int i;

	for (i=0; i < ndbrs; i++) {
		ia64_set_dbr(i, dbrs[i]);
		ia64_dv_serialize_data();
	}
	ia64_srlz_d();
}

/*
 * PMD[i] must be a counter. no check is made
 */
static inline unsigned long
pfm_read_soft_counter(pfm_context_t *ctx, int i)
{
	return ctx->ctx_pmds[i].val + (ia64_get_pmd(i) & pmu_conf->ovfl_val);
}

/*
 * PMD[i] must be a counter. no check is made
 */
static inline void
pfm_write_soft_counter(pfm_context_t *ctx, int i, unsigned long val)
{
	unsigned long ovfl_val = pmu_conf->ovfl_val;

	ctx->ctx_pmds[i].val = val  & ~ovfl_val;
	/*
	 * writing to unimplemented part is ignore, so we do not need to
	 * mask off top part
	 */
	ia64_set_pmd(i, val & ovfl_val);
}

static pfm_msg_t *
pfm_get_new_msg(pfm_context_t *ctx)
{
	int idx, next;

	next = (ctx->ctx_msgq_tail+1) % PFM_MAX_MSGS;

	DPRINT(("ctx_fd=%p head=%d tail=%d\n", ctx, ctx->ctx_msgq_head, ctx->ctx_msgq_tail));
	if (next == ctx->ctx_msgq_head) return NULL;

 	idx = 	ctx->ctx_msgq_tail;
	ctx->ctx_msgq_tail = next;

	DPRINT(("ctx=%p head=%d tail=%d msg=%d\n", ctx, ctx->ctx_msgq_head, ctx->ctx_msgq_tail, idx));

	return ctx->ctx_msgq+idx;
}

static pfm_msg_t *
pfm_get_next_msg(pfm_context_t *ctx)
{
	pfm_msg_t *msg;

	DPRINT(("ctx=%p head=%d tail=%d\n", ctx, ctx->ctx_msgq_head, ctx->ctx_msgq_tail));

	if (PFM_CTXQ_EMPTY(ctx)) return NULL;

	/*
	 * get oldest message
	 */
	msg = ctx->ctx_msgq+ctx->ctx_msgq_head;

	/*
	 * and move forward
	 */
	ctx->ctx_msgq_head = (ctx->ctx_msgq_head+1) % PFM_MAX_MSGS;

	DPRINT(("ctx=%p head=%d tail=%d type=%d\n", ctx, ctx->ctx_msgq_head, ctx->ctx_msgq_tail, msg->pfm_gen_msg.msg_type));

	return msg;
}

static void
pfm_reset_msgq(pfm_context_t *ctx)
{
	ctx->ctx_msgq_head = ctx->ctx_msgq_tail = 0;
	DPRINT(("ctx=%p msgq reset\n", ctx));
}

static void *
pfm_rvmalloc(unsigned long size)
{
	void *mem;
	unsigned long addr;

	size = PAGE_ALIGN(size);
	mem  = vmalloc(size);
	if (mem) {
		//printk("perfmon: CPU%d pfm_rvmalloc(%ld)=%p\n", smp_processor_id(), size, mem);
		memset(mem, 0, size);
		addr = (unsigned long)mem;
		while (size > 0) {
			pfm_reserve_page(addr);
			addr+=PAGE_SIZE;
			size-=PAGE_SIZE;
		}
	}
	return mem;
}

static void
pfm_rvfree(void *mem, unsigned long size)
{
	unsigned long addr;

	if (mem) {
		DPRINT(("freeing physical buffer @%p size=%lu\n", mem, size));
		addr = (unsigned long) mem;
		while ((long) size > 0) {
			pfm_unreserve_page(addr);
			addr+=PAGE_SIZE;
			size-=PAGE_SIZE;
		}
		vfree(mem);
	}
	return;
}

static pfm_context_t *
pfm_context_alloc(int ctx_flags)
{
	pfm_context_t *ctx;

	/* 
	 * allocate context descriptor 
	 * must be able to free with interrupts disabled
	 */
	ctx = kzalloc(sizeof(pfm_context_t), GFP_KERNEL);
	if (ctx) {
		DPRINT(("alloc ctx @%p\n", ctx));

		/*
		 * init context protection lock
		 */
		spin_lock_init(&ctx->ctx_lock);

		/*
		 * context is unloaded
		 */
		ctx->ctx_state = PFM_CTX_UNLOADED;

		/*
		 * initialization of context's flags
		 */
		ctx->ctx_fl_block       = (ctx_flags & PFM_FL_NOTIFY_BLOCK) ? 1 : 0;
		ctx->ctx_fl_system      = (ctx_flags & PFM_FL_SYSTEM_WIDE) ? 1: 0;
		ctx->ctx_fl_no_msg      = (ctx_flags & PFM_FL_OVFL_NO_MSG) ? 1: 0;
		/*
		 * will move to set properties
		 * ctx->ctx_fl_excl_idle   = (ctx_flags & PFM_FL_EXCL_IDLE) ? 1: 0;
		 */

		/*
		 * init restart semaphore to locked
		 */
		init_completion(&ctx->ctx_restart_done);

		/*
		 * activation is used in SMP only
		 */
		ctx->ctx_last_activation = PFM_INVALID_ACTIVATION;
		SET_LAST_CPU(ctx, -1);

		/*
		 * initialize notification message queue
		 */
		ctx->ctx_msgq_head = ctx->ctx_msgq_tail = 0;
		init_waitqueue_head(&ctx->ctx_msgq_wait);
		init_waitqueue_head(&ctx->ctx_zombieq);

	}
	return ctx;
}

static void
pfm_context_free(pfm_context_t *ctx)
{
	if (ctx) {
		DPRINT(("free ctx @%p\n", ctx));
		kfree(ctx);
	}
}

static void
pfm_mask_monitoring(struct task_struct *task)
{
	pfm_context_t *ctx = PFM_GET_CTX(task);
	unsigned long mask, val, ovfl_mask;
	int i;

	DPRINT_ovfl(("masking monitoring for [%d]\n", task_pid_nr(task)));

	ovfl_mask = pmu_conf->ovfl_val;
	/*
	 * monitoring can only be masked as a result of a valid
	 * counter overflow. In UP, it means that the PMU still
	 * has an owner. Note that the owner can be different
	 * from the current task. However the PMU state belongs
	 * to the owner.
	 * In SMP, a valid overflow only happens when task is
	 * current. Therefore if we come here, we know that
	 * the PMU state belongs to the current task, therefore
	 * we can access the live registers.
	 *
	 * So in both cases, the live register contains the owner's
	 * state. We can ONLY touch the PMU registers and NOT the PSR.
	 *
	 * As a consequence to this call, the ctx->th_pmds[] array
	 * contains stale information which must be ignored
	 * when context is reloaded AND monitoring is active (see
	 * pfm_restart).
	 */
	mask = ctx->ctx_used_pmds[0];
	for (i = 0; mask; i++, mask>>=1) {
		/* skip non used pmds */
		if ((mask & 0x1) == 0) continue;
		val = ia64_get_pmd(i);

		if (PMD_IS_COUNTING(i)) {
			/*
		 	 * we rebuild the full 64 bit value of the counter
		 	 */
			ctx->ctx_pmds[i].val += (val & ovfl_mask);
		} else {
			ctx->ctx_pmds[i].val = val;
		}
		DPRINT_ovfl(("pmd[%d]=0x%lx hw_pmd=0x%lx\n",
			i,
			ctx->ctx_pmds[i].val,
			val & ovfl_mask));
	}
	/*
	 * mask monitoring by setting the privilege level to 0
	 * we cannot use psr.pp/psr.up for this, it is controlled by
	 * the user
	 *
	 * if task is current, modify actual registers, otherwise modify
	 * thread save state, i.e., what will be restored in pfm_load_regs()
	 */
	mask = ctx->ctx_used_monitors[0] >> PMU_FIRST_COUNTER;
	for(i= PMU_FIRST_COUNTER; mask; i++, mask>>=1) {
		if ((mask & 0x1) == 0UL) continue;
		ia64_set_pmc(i, ctx->th_pmcs[i] & ~0xfUL);
		ctx->th_pmcs[i] &= ~0xfUL;
		DPRINT_ovfl(("pmc[%d]=0x%lx\n", i, ctx->th_pmcs[i]));
	}
	/*
	 * make all of this visible
	 */
	ia64_srlz_d();
}

/*
 * must always be done with task == current
 *
 * context must be in MASKED state when calling
 */
static void
pfm_restore_monitoring(struct task_struct *task)
{
	pfm_context_t *ctx = PFM_GET_CTX(task);
	unsigned long mask, ovfl_mask;
	unsigned long psr, val;
	int i, is_system;

	is_system = ctx->ctx_fl_system;
	ovfl_mask = pmu_conf->ovfl_val;

	if (task != current) {
		printk(KERN_ERR "perfmon.%d: invalid task[%d] current[%d]\n", __LINE__, task_pid_nr(task), task_pid_nr(current));
		return;
	}
	if (ctx->ctx_state != PFM_CTX_MASKED) {
		printk(KERN_ERR "perfmon.%d: task[%d] current[%d] invalid state=%d\n", __LINE__,
			task_pid_nr(task), task_pid_nr(current), ctx->ctx_state);
		return;
	}
	psr = pfm_get_psr();
	/*
	 * monitoring is masked via the PMC.
	 * As we restore their value, we do not want each counter to
	 * restart right away. We stop monitoring using the PSR,
	 * restore the PMC (and PMD) and then re-establish the psr
	 * as it was. Note that there can be no pending overflow at
	 * this point, because monitoring was MASKED.
	 *
	 * system-wide session are pinned and self-monitoring
	 */
	if (is_system && (PFM_CPUINFO_GET() & PFM_CPUINFO_DCR_PP)) {
		/* disable dcr pp */
		ia64_setreg(_IA64_REG_CR_DCR, ia64_getreg(_IA64_REG_CR_DCR) & ~IA64_DCR_PP);
		pfm_clear_psr_pp();
	} else {
		pfm_clear_psr_up();
	}
	/*
	 * first, we restore the PMD
	 */
	mask = ctx->ctx_used_pmds[0];
	for (i = 0; mask; i++, mask>>=1) {
		/* skip non used pmds */
		if ((mask & 0x1) == 0) continue;

		if (PMD_IS_COUNTING(i)) {
			/*
			 * we split the 64bit value according to
			 * counter width
			 */
			val = ctx->ctx_pmds[i].val & ovfl_mask;
			ctx->ctx_pmds[i].val &= ~ovfl_mask;
		} else {
			val = ctx->ctx_pmds[i].val;
		}
		ia64_set_pmd(i, val);

		DPRINT(("pmd[%d]=0x%lx hw_pmd=0x%lx\n",
			i,
			ctx->ctx_pmds[i].val,
			val));
	}
	/*
	 * restore the PMCs
	 */
	mask = ctx->ctx_used_monitors[0] >> PMU_FIRST_COUNTER;
	for(i= PMU_FIRST_COUNTER; mask; i++, mask>>=1) {
		if ((mask & 0x1) == 0UL) continue;
		ctx->th_pmcs[i] = ctx->ctx_pmcs[i];
		ia64_set_pmc(i, ctx->th_pmcs[i]);
		DPRINT(("[%d] pmc[%d]=0x%lx\n",
					task_pid_nr(task), i, ctx->th_pmcs[i]));
	}
	ia64_srlz_d();

	/*
	 * must restore DBR/IBR because could be modified while masked
	 * XXX: need to optimize 
	 */
	if (ctx->ctx_fl_using_dbreg) {
		pfm_restore_ibrs(ctx->ctx_ibrs, pmu_conf->num_ibrs);
		pfm_restore_dbrs(ctx->ctx_dbrs, pmu_conf->num_dbrs);
	}

	/*
	 * now restore PSR
	 */
	if (is_system && (PFM_CPUINFO_GET() & PFM_CPUINFO_DCR_PP)) {
		/* enable dcr pp */
		ia64_setreg(_IA64_REG_CR_DCR, ia64_getreg(_IA64_REG_CR_DCR) | IA64_DCR_PP);
		ia64_srlz_i();
	}
	pfm_set_psr_l(psr);
}

static inline void
pfm_save_pmds(unsigned long *pmds, unsigned long mask)
{
	int i;

	ia64_srlz_d();

	for (i=0; mask; i++, mask>>=1) {
		if (mask & 0x1) pmds[i] = ia64_get_pmd(i);
	}
}

/*
 * reload from thread state (used for ctxw only)
 */
static inline void
pfm_restore_pmds(unsigned long *pmds, unsigned long mask)
{
	int i;
	unsigned long val, ovfl_val = pmu_conf->ovfl_val;

	for (i=0; mask; i++, mask>>=1) {
		if ((mask & 0x1) == 0) continue;
		val = PMD_IS_COUNTING(i) ? pmds[i] & ovfl_val : pmds[i];
		ia64_set_pmd(i, val);
	}
	ia64_srlz_d();
}

/*
 * propagate PMD from context to thread-state
 */
static inline void
pfm_copy_pmds(struct task_struct *task, pfm_context_t *ctx)
{
	unsigned long ovfl_val = pmu_conf->ovfl_val;
	unsigned long mask = ctx->ctx_all_pmds[0];
	unsigned long val;
	int i;

	DPRINT(("mask=0x%lx\n", mask));

	for (i=0; mask; i++, mask>>=1) {

		val = ctx->ctx_pmds[i].val;

		/*
		 * We break up the 64 bit value into 2 pieces
		 * the lower bits go to the machine state in the
		 * thread (will be reloaded on ctxsw in).
		 * The upper part stays in the soft-counter.
		 */
		if (PMD_IS_COUNTING(i)) {
			ctx->ctx_pmds[i].val = val & ~ovfl_val;
			 val &= ovfl_val;
		}
		ctx->th_pmds[i] = val;

		DPRINT(("pmd[%d]=0x%lx soft_val=0x%lx\n",
			i,
			ctx->th_pmds[i],
			ctx->ctx_pmds[i].val));
	}
}

/*
 * propagate PMC from context to thread-state
 */
static inline void
pfm_copy_pmcs(struct task_struct *task, pfm_context_t *ctx)
{
	unsigned long mask = ctx->ctx_all_pmcs[0];
	int i;

	DPRINT(("mask=0x%lx\n", mask));

	for (i=0; mask; i++, mask>>=1) {
		/* masking 0 with ovfl_val yields 0 */
		ctx->th_pmcs[i] = ctx->ctx_pmcs[i];
		DPRINT(("pmc[%d]=0x%lx\n", i, ctx->th_pmcs[i]));
	}
}



static inline void
pfm_restore_pmcs(unsigned long *pmcs, unsigned long mask)
{
	int i;

	for (i=0; mask; i++, mask>>=1) {
		if ((mask & 0x1) == 0) continue;
		ia64_set_pmc(i, pmcs[i]);
	}
	ia64_srlz_d();
}

static inline int
pfm_uuid_cmp(pfm_uuid_t a, pfm_uuid_t b)
{
	return memcmp(a, b, sizeof(pfm_uuid_t));
}

static inline int
pfm_buf_fmt_exit(pfm_buffer_fmt_t *fmt, struct task_struct *task, void *buf, struct pt_regs *regs)
{
	int ret = 0;
	if (fmt->fmt_exit) ret = (*fmt->fmt_exit)(task, buf, regs);
	return ret;
}

static inline int
pfm_buf_fmt_getsize(pfm_buffer_fmt_t *fmt, struct task_struct *task, unsigned int flags, int cpu, void *arg, unsigned long *size)
{
	int ret = 0;
	if (fmt->fmt_getsize) ret = (*fmt->fmt_getsize)(task, flags, cpu, arg, size);
	return ret;
}


static inline int
pfm_buf_fmt_validate(pfm_buffer_fmt_t *fmt, struct task_struct *task, unsigned int flags,
		     int cpu, void *arg)
{
	int ret = 0;
	if (fmt->fmt_validate) ret = (*fmt->fmt_validate)(task, flags, cpu, arg);
	return ret;
}

static inline int
pfm_buf_fmt_init(pfm_buffer_fmt_t *fmt, struct task_struct *task, void *buf, unsigned int flags,
		     int cpu, void *arg)
{
	int ret = 0;
	if (fmt->fmt_init) ret = (*fmt->fmt_init)(task, buf, flags, cpu, arg);
	return ret;
}

static inline int
pfm_buf_fmt_restart(pfm_buffer_fmt_t *fmt, struct task_struct *task, pfm_ovfl_ctrl_t *ctrl, void *buf, struct pt_regs *regs)
{
	int ret = 0;
	if (fmt->fmt_restart) ret = (*fmt->fmt_restart)(task, ctrl, buf, regs);
	return ret;
}

static inline int
pfm_buf_fmt_restart_active(pfm_buffer_fmt_t *fmt, struct task_struct *task, pfm_ovfl_ctrl_t *ctrl, void *buf, struct pt_regs *regs)
{
	int ret = 0;
	if (fmt->fmt_restart_active) ret = (*fmt->fmt_restart_active)(task, ctrl, buf, regs);
	return ret;
}

static pfm_buffer_fmt_t *
__pfm_find_buffer_fmt(pfm_uuid_t uuid)
{
	struct list_head * pos;
	pfm_buffer_fmt_t * entry;

	list_for_each(pos, &pfm_buffer_fmt_list) {
		entry = list_entry(pos, pfm_buffer_fmt_t, fmt_list);
		if (pfm_uuid_cmp(uuid, entry->fmt_uuid) == 0)
			return entry;
	}
	return NULL;
}
 
/*
 * find a buffer format based on its uuid
 */
static pfm_buffer_fmt_t *
pfm_find_buffer_fmt(pfm_uuid_t uuid)
{
	pfm_buffer_fmt_t * fmt;
	spin_lock(&pfm_buffer_fmt_lock);
	fmt = __pfm_find_buffer_fmt(uuid);
	spin_unlock(&pfm_buffer_fmt_lock);
	return fmt;
}
 
int
pfm_register_buffer_fmt(pfm_buffer_fmt_t *fmt)
{
	int ret = 0;

	/* some sanity checks */
	if (fmt == NULL || fmt->fmt_name == NULL) return -EINVAL;

	/* we need at least a handler */
	if (fmt->fmt_handler == NULL) return -EINVAL;

	/*
	 * XXX: need check validity of fmt_arg_size
	 */

	spin_lock(&pfm_buffer_fmt_lock);

	if (__pfm_find_buffer_fmt(fmt->fmt_uuid)) {
		printk(KERN_ERR "perfmon: duplicate sampling format: %s\n", fmt->fmt_name);
		ret = -EBUSY;
		goto out;
	} 
	list_add(&fmt->fmt_list, &pfm_buffer_fmt_list);
	printk(KERN_INFO "perfmon: added sampling format %s\n", fmt->fmt_name);

out:
	spin_unlock(&pfm_buffer_fmt_lock);
 	return ret;
}
EXPORT_SYMBOL(pfm_register_buffer_fmt);

int
pfm_unregister_buffer_fmt(pfm_uuid_t uuid)
{
	pfm_buffer_fmt_t *fmt;
	int ret = 0;

	spin_lock(&pfm_buffer_fmt_lock);

	fmt = __pfm_find_buffer_fmt(uuid);
	if (!fmt) {
		printk(KERN_ERR "perfmon: cannot unregister format, not found\n");
		ret = -EINVAL;
		goto out;
	}
	list_del_init(&fmt->fmt_list);
	printk(KERN_INFO "perfmon: removed sampling format: %s\n", fmt->fmt_name);

out:
	spin_unlock(&pfm_buffer_fmt_lock);
	return ret;

}
EXPORT_SYMBOL(pfm_unregister_buffer_fmt);

extern void update_pal_halt_status(int);

static int
pfm_reserve_session(struct task_struct *task, int is_syswide, unsigned int cpu)
{
	unsigned long flags;
	/*
	 * validity checks on cpu_mask have been done upstream
	 */
	LOCK_PFS(flags);

	DPRINT(("in sys_sessions=%u task_sessions=%u dbregs=%u syswide=%d cpu=%u\n",
		pfm_sessions.pfs_sys_sessions,
		pfm_sessions.pfs_task_sessions,
		pfm_sessions.pfs_sys_use_dbregs,
		is_syswide,
		cpu));

	if (is_syswide) {
		/*
		 * cannot mix system wide and per-task sessions
		 */
		if (pfm_sessions.pfs_task_sessions > 0UL) {
			DPRINT(("system wide not possible, %u conflicting task_sessions\n",
			  	pfm_sessions.pfs_task_sessions));
			goto abort;
		}

		if (pfm_sessions.pfs_sys_session[cpu]) goto error_conflict;

		DPRINT(("reserving system wide session on CPU%u currently on CPU%u\n", cpu, smp_processor_id()));

		pfm_sessions.pfs_sys_session[cpu] = task;

		pfm_sessions.pfs_sys_sessions++ ;

	} else {
		if (pfm_sessions.pfs_sys_sessions) goto abort;
		pfm_sessions.pfs_task_sessions++;
	}

	DPRINT(("out sys_sessions=%u task_sessions=%u dbregs=%u syswide=%d cpu=%u\n",
		pfm_sessions.pfs_sys_sessions,
		pfm_sessions.pfs_task_sessions,
		pfm_sessions.pfs_sys_use_dbregs,
		is_syswide,
		cpu));

	/*
	 * disable default_idle() to go to PAL_HALT
	 */
	update_pal_halt_status(0);

	UNLOCK_PFS(flags);

	return 0;

error_conflict:
	DPRINT(("system wide not possible, conflicting session [%d] on CPU%d\n",
  		task_pid_nr(pfm_sessions.pfs_sys_session[cpu]),
		cpu));
abort:
	UNLOCK_PFS(flags);

	return -EBUSY;

}

static int
pfm_unreserve_session(pfm_context_t *ctx, int is_syswide, unsigned int cpu)
{
	unsigned long flags;
	/*
	 * validity checks on cpu_mask have been done upstream
	 */
	LOCK_PFS(flags);

	DPRINT(("in sys_sessions=%u task_sessions=%u dbregs=%u syswide=%d cpu=%u\n",
		pfm_sessions.pfs_sys_sessions,
		pfm_sessions.pfs_task_sessions,
		pfm_sessions.pfs_sys_use_dbregs,
		is_syswide,
		cpu));


	if (is_syswide) {
		pfm_sessions.pfs_sys_session[cpu] = NULL;
		/*
		 * would not work with perfmon+more than one bit in cpu_mask
		 */
		if (ctx && ctx->ctx_fl_using_dbreg) {
			if (pfm_sessions.pfs_sys_use_dbregs == 0) {
				printk(KERN_ERR "perfmon: invalid release for ctx %p sys_use_dbregs=0\n", ctx);
			} else {
				pfm_sessions.pfs_sys_use_dbregs--;
			}
		}
		pfm_sessions.pfs_sys_sessions--;
	} else {
		pfm_sessions.pfs_task_sessions--;
	}
	DPRINT(("out sys_sessions=%u task_sessions=%u dbregs=%u syswide=%d cpu=%u\n",
		pfm_sessions.pfs_sys_sessions,
		pfm_sessions.pfs_task_sessions,
		pfm_sessions.pfs_sys_use_dbregs,
		is_syswide,
		cpu));

	/*
	 * if possible, enable default_idle() to go into PAL_HALT
	 */
	if (pfm_sessions.pfs_task_sessions == 0 && pfm_sessions.pfs_sys_sessions == 0)
		update_pal_halt_status(1);

	UNLOCK_PFS(flags);

	return 0;
}

/*
 * removes virtual mapping of the sampling buffer.
 * IMPORTANT: cannot be called with interrupts disable, e.g. inside
 * a PROTECT_CTX() section.
 */
static int
pfm_remove_smpl_mapping(struct task_struct *task, void *vaddr, unsigned long size)
{
	int r;

	/* sanity checks */
	if (task->mm == NULL || size == 0UL || vaddr == NULL) {
		printk(KERN_ERR "perfmon: pfm_remove_smpl_mapping [%d] invalid context mm=%p\n", task_pid_nr(task), task->mm);
		return -EINVAL;
	}

	DPRINT(("smpl_vaddr=%p size=%lu\n", vaddr, size));

	/*
	 * does the actual unmapping
	 */
	down_write(&task->mm->mmap_sem);

	DPRINT(("down_write done smpl_vaddr=%p size=%lu\n", vaddr, size));

	r = pfm_do_munmap(task->mm, (unsigned long)vaddr, size, 0);

	up_write(&task->mm->mmap_sem);
	if (r !=0) {
		printk(KERN_ERR "perfmon: [%d] unable to unmap sampling buffer @%p size=%lu\n", task_pid_nr(task), vaddr, size);
	}

	DPRINT(("do_unmap(%p, %lu)=%d\n", vaddr, size, r));

	return 0;
}

/*
 * free actual physical storage used by sampling buffer
 */
#if 0
static int
pfm_free_smpl_buffer(pfm_context_t *ctx)
{
	pfm_buffer_fmt_t *fmt;

	if (ctx->ctx_smpl_hdr == NULL) goto invalid_free;

	/*
	 * we won't use the buffer format anymore
	 */
	fmt = ctx->ctx_buf_fmt;

	DPRINT(("sampling buffer @%p size %lu vaddr=%p\n",
		ctx->ctx_smpl_hdr,
		ctx->ctx_smpl_size,
		ctx->ctx_smpl_vaddr));

	pfm_buf_fmt_exit(fmt, current, NULL, NULL);

	/*
	 * free the buffer
	 */
	pfm_rvfree(ctx->ctx_smpl_hdr, ctx->ctx_smpl_size);

	ctx->ctx_smpl_hdr  = NULL;
	ctx->ctx_smpl_size = 0UL;

	return 0;

invalid_free:
	printk(KERN_ERR "perfmon: pfm_free_smpl_buffer [%d] no buffer\n", task_pid_nr(current));
	return -EINVAL;
}
#endif

static inline void
pfm_exit_smpl_buffer(pfm_buffer_fmt_t *fmt)
{
	if (fmt == NULL) return;

	pfm_buf_fmt_exit(fmt, current, NULL, NULL);

}

/*
 * pfmfs should _never_ be mounted by userland - too much of security hassle,
 * no real gain from having the whole whorehouse mounted. So we don't need
 * any operations on the root directory. However, we need a non-trivial
 * d_name - pfm: will go nicely and kill the special-casing in procfs.
 */
static struct vfsmount *pfmfs_mnt;

static int __init
init_pfm_fs(void)
{
	int err = register_filesystem(&pfm_fs_type);
	if (!err) {
		pfmfs_mnt = kern_mount(&pfm_fs_type);
		err = PTR_ERR(pfmfs_mnt);
		if (IS_ERR(pfmfs_mnt))
			unregister_filesystem(&pfm_fs_type);
		else
			err = 0;
	}
	return err;
}

static ssize_t
pfm_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	pfm_context_t *ctx;
	pfm_msg_t *msg;
	ssize_t ret;
	unsigned long flags;
  	DECLARE_WAITQUEUE(wait, current);
	if (PFM_IS_FILE(filp) == 0) {
		printk(KERN_ERR "perfmon: pfm_poll: bad magic [%d]\n", task_pid_nr(current));
		return -EINVAL;
	}

	ctx = (pfm_context_t *)filp->private_data;
	if (ctx == NULL) {
		printk(KERN_ERR "perfmon: pfm_read: NULL ctx [%d]\n", task_pid_nr(current));
		return -EINVAL;
	}

	/*
	 * check even when there is no message
	 */
	if (size < sizeof(pfm_msg_t)) {
		DPRINT(("message is too small ctx=%p (>=%ld)\n", ctx, sizeof(pfm_msg_t)));
		return -EINVAL;
	}

	PROTECT_CTX(ctx, flags);

  	/*
	 * put ourselves on the wait queue
	 */
  	add_wait_queue(&ctx->ctx_msgq_wait, &wait);


  	for(;;) {
		/*
		 * check wait queue
		 */

  		set_current_state(TASK_INTERRUPTIBLE);

		DPRINT(("head=%d tail=%d\n", ctx->ctx_msgq_head, ctx->ctx_msgq_tail));

		ret = 0;
		if(PFM_CTXQ_EMPTY(ctx) == 0) break;

		UNPROTECT_CTX(ctx, flags);

		/*
		 * check non-blocking read
		 */
      		ret = -EAGAIN;
		if(filp->f_flags & O_NONBLOCK) break;

		/*
		 * check pending signals
		 */
		if(signal_pending(current)) {
			ret = -EINTR;
			break;
		}
      		/*
		 * no message, so wait
		 */
      		schedule();

		PROTECT_CTX(ctx, flags);
	}
	DPRINT(("[%d] back to running ret=%ld\n", task_pid_nr(current), ret));
  	set_current_state(TASK_RUNNING);
	remove_wait_queue(&ctx->ctx_msgq_wait, &wait);

	if (ret < 0) goto abort;

	ret = -EINVAL;
	msg = pfm_get_next_msg(ctx);
	if (msg == NULL) {
		printk(KERN_ERR "perfmon: pfm_read no msg for ctx=%p [%d]\n", ctx, task_pid_nr(current));
		goto abort_locked;
	}

	DPRINT(("fd=%d type=%d\n", msg->pfm_gen_msg.msg_ctx_fd, msg->pfm_gen_msg.msg_type));

	ret = -EFAULT;
  	if(copy_to_user(buf, msg, sizeof(pfm_msg_t)) == 0) ret = sizeof(pfm_msg_t);

abort_locked:
	UNPROTECT_CTX(ctx, flags);
abort:
	return ret;
}

static ssize_t
pfm_write(struct file *file, const char __user *ubuf,
			  size_t size, loff_t *ppos)
{
	DPRINT(("pfm_write called\n"));
	return -EINVAL;
}

static unsigned int
pfm_poll(struct file *filp, poll_table * wait)
{
	pfm_context_t *ctx;
	unsigned long flags;
	unsigned int mask = 0;

	if (PFM_IS_FILE(filp) == 0) {
		printk(KERN_ERR "perfmon: pfm_poll: bad magic [%d]\n", task_pid_nr(current));
		return 0;
	}

	ctx = (pfm_context_t *)filp->private_data;
	if (ctx == NULL) {
		printk(KERN_ERR "perfmon: pfm_poll: NULL ctx [%d]\n", task_pid_nr(current));
		return 0;
	}


	DPRINT(("pfm_poll ctx_fd=%d before poll_wait\n", ctx->ctx_fd));

	poll_wait(filp, &ctx->ctx_msgq_wait, wait);

	PROTECT_CTX(ctx, flags);

	if (PFM_CTXQ_EMPTY(ctx) == 0)
		mask =  POLLIN | POLLRDNORM;

	UNPROTECT_CTX(ctx, flags);

	DPRINT(("pfm_poll ctx_fd=%d mask=0x%x\n", ctx->ctx_fd, mask));

	return mask;
}

static int
pfm_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	DPRINT(("pfm_ioctl called\n"));
	return -EINVAL;
}

/*
 * interrupt cannot be masked when coming here
 */
static inline int
pfm_do_fasync(int fd, struct file *filp, pfm_context_t *ctx, int on)
{
	int ret;

	ret = fasync_helper (fd, filp, on, &ctx->ctx_async_queue);

	DPRINT(("pfm_fasync called by [%d] on ctx_fd=%d on=%d async_queue=%p ret=%d\n",
		task_pid_nr(current),
		fd,
		on,
		ctx->ctx_async_queue, ret));

	return ret;
}

static int
pfm_fasync(int fd, struct file *filp, int on)
{
	pfm_context_t *ctx;
	int ret;

	if (PFM_IS_FILE(filp) == 0) {
		printk(KERN_ERR "perfmon: pfm_fasync bad magic [%d]\n", task_pid_nr(current));
		return -EBADF;
	}

	ctx = (pfm_context_t *)filp->private_data;
	if (ctx == NULL) {
		printk(KERN_ERR "perfmon: pfm_fasync NULL ctx [%d]\n", task_pid_nr(current));
		return -EBADF;
	}
	/*
	 * we cannot mask interrupts during this call because this may
	 * may go to sleep if memory is not readily avalaible.
	 *
	 * We are protected from the conetxt disappearing by the get_fd()/put_fd()
	 * done in caller. Serialization of this function is ensured by caller.
	 */
	ret = pfm_do_fasync(fd, filp, ctx, on);


	DPRINT(("pfm_fasync called on ctx_fd=%d on=%d async_queue=%p ret=%d\n",
		fd,
		on,
		ctx->ctx_async_queue, ret));

	return ret;
}

#ifdef CONFIG_SMP
/*
 * this function is exclusively called from pfm_close().
 * The context is not protected at that time, nor are interrupts
 * on the remote CPU. That's necessary to avoid deadlocks.
 */
static void
pfm_syswide_force_stop(void *info)
{
	pfm_context_t   *ctx = (pfm_context_t *)info;
	struct pt_regs *regs = task_pt_regs(current);
	struct task_struct *owner;
	unsigned long flags;
	int ret;

	if (ctx->ctx_cpu != smp_processor_id()) {
		printk(KERN_ERR "perfmon: pfm_syswide_force_stop for CPU%d  but on CPU%d\n",
			ctx->ctx_cpu,
			smp_processor_id());
		return;
	}
	owner = GET_PMU_OWNER();
	if (owner != ctx->ctx_task) {
		printk(KERN_ERR "perfmon: pfm_syswide_force_stop CPU%d unexpected owner [%d] instead of [%d]\n",
			smp_processor_id(),
			task_pid_nr(owner), task_pid_nr(ctx->ctx_task));
		return;
	}
	if (GET_PMU_CTX() != ctx) {
		printk(KERN_ERR "perfmon: pfm_syswide_force_stop CPU%d unexpected ctx %p instead of %p\n",
			smp_processor_id(),
			GET_PMU_CTX(), ctx);
		return;
	}

	DPRINT(("on CPU%d forcing system wide stop for [%d]\n", smp_processor_id(), task_pid_nr(ctx->ctx_task)));
	/*
	 * the context is already protected in pfm_close(), we simply
	 * need to mask interrupts to avoid a PMU interrupt race on
	 * this CPU
	 */
	local_irq_save(flags);

	ret = pfm_context_unload(ctx, NULL, 0, regs);
	if (ret) {
		DPRINT(("context_unload returned %d\n", ret));
	}

	/*
	 * unmask interrupts, PMU interrupts are now spurious here
	 */
	local_irq_restore(flags);
}

static void
pfm_syswide_cleanup_other_cpu(pfm_context_t *ctx)
{
	int ret;

	DPRINT(("calling CPU%d for cleanup\n", ctx->ctx_cpu));
	ret = smp_call_function_single(ctx->ctx_cpu, pfm_syswide_force_stop, ctx, 1);
	DPRINT(("called CPU%d for cleanup ret=%d\n", ctx->ctx_cpu, ret));
}
#endif /* CONFIG_SMP */

/*
 * called for each close(). Partially free resources.
 * When caller is self-monitoring, the context is unloaded.
 */
static int
pfm_flush(struct file *filp, fl_owner_t id)
{
	pfm_context_t *ctx;
	struct task_struct *task;
	struct pt_regs *regs;
	unsigned long flags;
	unsigned long smpl_buf_size = 0UL;
	void *smpl_buf_vaddr = NULL;
	int state, is_system;

	if (PFM_IS_FILE(filp) == 0) {
		DPRINT(("bad magic for\n"));
		return -EBADF;
	}

	ctx = (pfm_context_t *)filp->private_data;
	if (ctx == NULL) {
		printk(KERN_ERR "perfmon: pfm_flush: NULL ctx [%d]\n", task_pid_nr(current));
		return -EBADF;
	}

	/*
	 * remove our file from the async queue, if we use this mode.
	 * This can be done without the context being protected. We come
	 * here when the context has become unreachable by other tasks.
	 *
	 * We may still have active monitoring at this point and we may
	 * end up in pfm_overflow_handler(). However, fasync_helper()
	 * operates with interrupts disabled and it cleans up the
	 * queue. If the PMU handler is called prior to entering
	 * fasync_helper() then it will send a signal. If it is
	 * invoked after, it will find an empty queue and no
	 * signal will be sent. In both case, we are safe
	 */
	PROTECT_CTX(ctx, flags);

	state     = ctx->ctx_state;
	is_system = ctx->ctx_fl_system;

	task = PFM_CTX_TASK(ctx);
	regs = task_pt_regs(task);

	DPRINT(("ctx_state=%d is_current=%d\n",
		state,
		task == current ? 1 : 0));

	/*
	 * if state == UNLOADED, then task is NULL
	 */

	/*
	 * we must stop and unload because we are losing access to the context.
	 */
	if (task == current) {
#ifdef CONFIG_SMP
		/*
		 * the task IS the owner but it migrated to another CPU: that's bad
		 * but we must handle this cleanly. Unfortunately, the kernel does
		 * not provide a mechanism to block migration (while the context is loaded).
		 *
		 * We need to release the resource on the ORIGINAL cpu.
		 */
		if (is_system && ctx->ctx_cpu != smp_processor_id()) {

			DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
			/*
			 * keep context protected but unmask interrupt for IPI
			 */
			local_irq_restore(flags);

			pfm_syswide_cleanup_other_cpu(ctx);

			/*
			 * restore interrupt masking
			 */
			local_irq_save(flags);

			/*
			 * context is unloaded at this point
			 */
		} else
#endif /* CONFIG_SMP */
		{

			DPRINT(("forcing unload\n"));
			/*
		 	* stop and unload, returning with state UNLOADED
		 	* and session unreserved.
		 	*/
			pfm_context_unload(ctx, NULL, 0, regs);

			DPRINT(("ctx_state=%d\n", ctx->ctx_state));
		}
	}

	/*
	 * remove virtual mapping, if any, for the calling task.
	 * cannot reset ctx field until last user is calling close().
	 *
	 * ctx_smpl_vaddr must never be cleared because it is needed
	 * by every task with access to the context
	 *
	 * When called from do_exit(), the mm context is gone already, therefore
	 * mm is NULL, i.e., the VMA is already gone  and we do not have to
	 * do anything here
	 */
	if (ctx->ctx_smpl_vaddr && current->mm) {
		smpl_buf_vaddr = ctx->ctx_smpl_vaddr;
		smpl_buf_size  = ctx->ctx_smpl_size;
	}

	UNPROTECT_CTX(ctx, flags);

	/*
	 * if there was a mapping, then we systematically remove it
	 * at this point. Cannot be done inside critical section
	 * because some VM function reenables interrupts.
	 *
	 */
	if (smpl_buf_vaddr) pfm_remove_smpl_mapping(current, smpl_buf_vaddr, smpl_buf_size);

	return 0;
}
/*
 * called either on explicit close() or from exit_files(). 
 * Only the LAST user of the file gets to this point, i.e., it is
 * called only ONCE.
 *
 * IMPORTANT: we get called ONLY when the refcnt on the file gets to zero 
 * (fput()),i.e, last task to access the file. Nobody else can access the 
 * file at this point.
 *
 * When called from exit_files(), the VMA has been freed because exit_mm()
 * is executed before exit_files().
 *
 * When called from exit_files(), the current task is not yet ZOMBIE but we
 * flush the PMU state to the context. 
 */
static int
pfm_close(struct inode *inode, struct file *filp)
{
	pfm_context_t *ctx;
	struct task_struct *task;
	struct pt_regs *regs;
  	DECLARE_WAITQUEUE(wait, current);
	unsigned long flags;
	unsigned long smpl_buf_size = 0UL;
	void *smpl_buf_addr = NULL;
	int free_possible = 1;
	int state, is_system;

	DPRINT(("pfm_close called private=%p\n", filp->private_data));

	if (PFM_IS_FILE(filp) == 0) {
		DPRINT(("bad magic\n"));
		return -EBADF;
	}
	
	ctx = (pfm_context_t *)filp->private_data;
	if (ctx == NULL) {
		printk(KERN_ERR "perfmon: pfm_close: NULL ctx [%d]\n", task_pid_nr(current));
		return -EBADF;
	}

	PROTECT_CTX(ctx, flags);

	state     = ctx->ctx_state;
	is_system = ctx->ctx_fl_system;

	task = PFM_CTX_TASK(ctx);
	regs = task_pt_regs(task);

	DPRINT(("ctx_state=%d is_current=%d\n", 
		state,
		task == current ? 1 : 0));

	/*
	 * if task == current, then pfm_flush() unloaded the context
	 */
	if (state == PFM_CTX_UNLOADED) goto doit;

	/*
	 * context is loaded/masked and task != current, we need to
	 * either force an unload or go zombie
	 */

	/*
	 * The task is currently blocked or will block after an overflow.
	 * we must force it to wakeup to get out of the
	 * MASKED state and transition to the unloaded state by itself.
	 *
	 * This situation is only possible for per-task mode
	 */
	if (state == PFM_CTX_MASKED && CTX_OVFL_NOBLOCK(ctx) == 0) {

		/*
		 * set a "partial" zombie state to be checked
		 * upon return from down() in pfm_handle_work().
		 *
		 * We cannot use the ZOMBIE state, because it is checked
		 * by pfm_load_regs() which is called upon wakeup from down().
		 * In such case, it would free the context and then we would
		 * return to pfm_handle_work() which would access the
		 * stale context. Instead, we set a flag invisible to pfm_load_regs()
		 * but visible to pfm_handle_work().
		 *
		 * For some window of time, we have a zombie context with
		 * ctx_state = MASKED  and not ZOMBIE
		 */
		ctx->ctx_fl_going_zombie = 1;

		/*
		 * force task to wake up from MASKED state
		 */
		complete(&ctx->ctx_restart_done);

		DPRINT(("waking up ctx_state=%d\n", state));

		/*
		 * put ourself to sleep waiting for the other
		 * task to report completion
		 *
		 * the context is protected by mutex, therefore there
		 * is no risk of being notified of completion before
		 * begin actually on the waitq.
		 */
  		set_current_state(TASK_INTERRUPTIBLE);
  		add_wait_queue(&ctx->ctx_zombieq, &wait);

		UNPROTECT_CTX(ctx, flags);

		/*
		 * XXX: check for signals :
		 * 	- ok for explicit close
		 * 	- not ok when coming from exit_files()
		 */
      		schedule();


		PROTECT_CTX(ctx, flags);


		remove_wait_queue(&ctx->ctx_zombieq, &wait);
  		set_current_state(TASK_RUNNING);

		/*
		 * context is unloaded at this point
		 */
		DPRINT(("after zombie wakeup ctx_state=%d for\n", state));
	}
	else if (task != current) {
#ifdef CONFIG_SMP
		/*
	 	 * switch context to zombie state
	 	 */
		ctx->ctx_state = PFM_CTX_ZOMBIE;

		DPRINT(("zombie ctx for [%d]\n", task_pid_nr(task)));
		/*
		 * cannot free the context on the spot. deferred until
		 * the task notices the ZOMBIE state
		 */
		free_possible = 0;
#else
		pfm_context_unload(ctx, NULL, 0, regs);
#endif
	}

doit:
	/* reload state, may have changed during  opening of critical section */
	state = ctx->ctx_state;

	/*
	 * the context is still attached to a task (possibly current)
	 * we cannot destroy it right now
	 */

	/*
	 * we must free the sampling buffer right here because
	 * we cannot rely on it being cleaned up later by the
	 * monitored task. It is not possible to free vmalloc'ed
	 * memory in pfm_load_regs(). Instead, we remove the buffer
	 * now. should there be subsequent PMU overflow originally
	 * meant for sampling, the will be converted to spurious
	 * and that's fine because the monitoring tools is gone anyway.
	 */
	if (ctx->ctx_smpl_hdr) {
		smpl_buf_addr = ctx->ctx_smpl_hdr;
		smpl_buf_size = ctx->ctx_smpl_size;
		/* no more sampling */
		ctx->ctx_smpl_hdr = NULL;
		ctx->ctx_fl_is_sampling = 0;
	}

	DPRINT(("ctx_state=%d free_possible=%d addr=%p size=%lu\n",
		state,
		free_possible,
		smpl_buf_addr,
		smpl_buf_size));

	if (smpl_buf_addr) pfm_exit_smpl_buffer(ctx->ctx_buf_fmt);

	/*
	 * UNLOADED that the session has already been unreserved.
	 */
	if (state == PFM_CTX_ZOMBIE) {
		pfm_unreserve_session(ctx, ctx->ctx_fl_system , ctx->ctx_cpu);
	}

	/*
	 * disconnect file descriptor from context must be done
	 * before we unlock.
	 */
	filp->private_data = NULL;

	/*
	 * if we free on the spot, the context is now completely unreachable
	 * from the callers side. The monitored task side is also cut, so we
	 * can freely cut.
	 *
	 * If we have a deferred free, only the caller side is disconnected.
	 */
	UNPROTECT_CTX(ctx, flags);

	/*
	 * All memory free operations (especially for vmalloc'ed memory)
	 * MUST be done with interrupts ENABLED.
	 */
	if (smpl_buf_addr)  pfm_rvfree(smpl_buf_addr, smpl_buf_size);

	/*
	 * return the memory used by the context
	 */
	if (free_possible) pfm_context_free(ctx);

	return 0;
}

static int
pfm_no_open(struct inode *irrelevant, struct file *dontcare)
{
	DPRINT(("pfm_no_open called\n"));
	return -ENXIO;
}



static const struct file_operations pfm_file_ops = {
	.llseek   = no_llseek,
	.read     = pfm_read,
	.write    = pfm_write,
	.poll     = pfm_poll,
	.ioctl    = pfm_ioctl,
	.open     = pfm_no_open,	/* special open code to disallow open via /proc */
	.fasync   = pfm_fasync,
	.release  = pfm_close,
	.flush	  = pfm_flush
};

static int
pfmfs_delete_dentry(struct dentry *dentry)
{
	return 1;
}

static const struct dentry_operations pfmfs_dentry_operations = {
	.d_delete = pfmfs_delete_dentry,
};


static struct file *
pfm_alloc_file(pfm_context_t *ctx)
{
	struct file *file;
	struct inode *inode;
	struct dentry *dentry;
	char name[32];
	struct qstr this;

	/*
	 * allocate a new inode
	 */
	inode = new_inode(pfmfs_mnt->mnt_sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	DPRINT(("new inode ino=%ld @%p\n", inode->i_ino, inode));

	inode->i_mode = S_IFCHR|S_IRUGO;
	inode->i_uid  = current_fsuid();
	inode->i_gid  = current_fsgid();

	sprintf(name, "[%lu]", inode->i_ino);
	this.name = name;
	this.len  = strlen(name);
	this.hash = inode->i_ino;

	/*
	 * allocate a new dcache entry
	 */
	dentry = d_alloc(pfmfs_mnt->mnt_sb->s_root, &this);
	if (!dentry) {
		iput(inode);
		return ERR_PTR(-ENOMEM);
	}

	dentry->d_op = &pfmfs_dentry_operations;
	d_add(dentry, inode);

	file = alloc_file(pfmfs_mnt, dentry, FMODE_READ, &pfm_file_ops);
	if (!file) {
		dput(dentry);
		return ERR_PTR(-ENFILE);
	}

	file->f_flags = O_RDONLY;
	file->private_data = ctx;

	return file;
}

static int
pfm_remap_buffer(struct vm_area_struct *vma, unsigned long buf, unsigned long addr, unsigned long size)
{
	DPRINT(("CPU%d buf=0x%lx addr=0x%lx size=%ld\n", smp_processor_id(), buf, addr, size));

	while (size > 0) {
		unsigned long pfn = ia64_tpa(buf) >> PAGE_SHIFT;


		if (remap_pfn_range(vma, addr, pfn, PAGE_SIZE, PAGE_READONLY))
			return -ENOMEM;

		addr  += PAGE_SIZE;
		buf   += PAGE_SIZE;
		size  -= PAGE_SIZE;
	}
	return 0;
}

/*
 * allocate a sampling buffer and remaps it into the user address space of the task
 */
static int
pfm_smpl_buffer_alloc(struct task_struct *task, struct file *filp, pfm_context_t *ctx, unsigned long rsize, void **user_vaddr)
{
	struct mm_struct *mm = task->mm;
	struct vm_area_struct *vma = NULL;
	unsigned long size;
	void *smpl_buf;


	/*
	 * the fixed header + requested size and align to page boundary
	 */
	size = PAGE_ALIGN(rsize);

	DPRINT(("sampling buffer rsize=%lu size=%lu bytes\n", rsize, size));

	/*
	 * check requested size to avoid Denial-of-service attacks
	 * XXX: may have to refine this test
	 * Check against address space limit.
	 *
	 * if ((mm->total_vm << PAGE_SHIFT) + len> task->rlim[RLIMIT_AS].rlim_cur)
	 * 	return -ENOMEM;
	 */
	if (size > task->signal->rlim[RLIMIT_MEMLOCK].rlim_cur)
		return -ENOMEM;

	/*
	 * We do the easy to undo allocations first.
 	 *
	 * pfm_rvmalloc(), clears the buffer, so there is no leak
	 */
	smpl_buf = pfm_rvmalloc(size);
	if (smpl_buf == NULL) {
		DPRINT(("Can't allocate sampling buffer\n"));
		return -ENOMEM;
	}

	DPRINT(("smpl_buf @%p\n", smpl_buf));

	/* allocate vma */
	vma = kmem_cache_zalloc(vm_area_cachep, GFP_KERNEL);
	if (!vma) {
		DPRINT(("Cannot allocate vma\n"));
		goto error_kmem;
	}

	/*
	 * partially initialize the vma for the sampling buffer
	 */
	vma->vm_mm	     = mm;
	vma->vm_file	     = filp;
	vma->vm_flags	     = VM_READ| VM_MAYREAD |VM_RESERVED;
	vma->vm_page_prot    = PAGE_READONLY; /* XXX may need to change */

	/*
	 * Now we have everything we need and we can initialize
	 * and connect all the data structures
	 */

	ctx->ctx_smpl_hdr   = smpl_buf;
	ctx->ctx_smpl_size  = size; /* aligned size */

	/*
	 * Let's do the difficult operations next.
	 *
	 * now we atomically find some area in the address space and
	 * remap the buffer in it.
	 */
	down_write(&task->mm->mmap_sem);

	/* find some free area in address space, must have mmap sem held */
	vma->vm_start = pfm_get_unmapped_area(NULL, 0, size, 0, MAP_PRIVATE|MAP_ANONYMOUS, 0);
	if (vma->vm_start == 0UL) {
		DPRINT(("Cannot find unmapped area for size %ld\n", size));
		up_write(&task->mm->mmap_sem);
		goto error;
	}
	vma->vm_end = vma->vm_start + size;
	vma->vm_pgoff = vma->vm_start >> PAGE_SHIFT;

	DPRINT(("aligned size=%ld, hdr=%p mapped @0x%lx\n", size, ctx->ctx_smpl_hdr, vma->vm_start));

	/* can only be applied to current task, need to have the mm semaphore held when called */
	if (pfm_remap_buffer(vma, (unsigned long)smpl_buf, vma->vm_start, size)) {
		DPRINT(("Can't remap buffer\n"));
		up_write(&task->mm->mmap_sem);
		goto error;
	}

	get_file(filp);

	/*
	 * now insert the vma in the vm list for the process, must be
	 * done with mmap lock held
	 */
	insert_vm_struct(mm, vma);

	mm->total_vm  += size >> PAGE_SHIFT;
	vm_stat_account(vma->vm_mm, vma->vm_flags, vma->vm_file,
							vma_pages(vma));
	up_write(&task->mm->mmap_sem);

	/*
	 * keep track of user level virtual address
	 */
	ctx->ctx_smpl_vaddr = (void *)vma->vm_start;
	*(unsigned long *)user_vaddr = vma->vm_start;

	return 0;

error:
	kmem_cache_free(vm_area_cachep, vma);
error_kmem:
	pfm_rvfree(smpl_buf, size);

	return -ENOMEM;
}

/*
 * XXX: do something better here
 */
static int
pfm_bad_permissions(struct task_struct *task)
{
	const struct cred *tcred;
	uid_t uid = current_uid();
	gid_t gid = current_gid();
	int ret;

	rcu_read_lock();
	tcred = __task_cred(task);

	/* inspired by ptrace_attach() */
	DPRINT(("cur: uid=%d gid=%d task: euid=%d suid=%d uid=%d egid=%d sgid=%d\n",
		uid,
		gid,
		tcred->euid,
		tcred->suid,
		tcred->uid,
		tcred->egid,
		tcred->sgid));

	ret = ((uid != tcred->euid)
	       || (uid != tcred->suid)
	       || (uid != tcred->uid)
	       || (gid != tcred->egid)
	       || (gid != tcred->sgid)
	       || (gid != tcred->gid)) && !capable(CAP_SYS_PTRACE);

	rcu_read_unlock();
	return ret;
}

static int
pfarg_is_sane(struct task_struct *task, pfarg_context_t *pfx)
{
	int ctx_flags;

	/* valid signal */

	ctx_flags = pfx->ctx_flags;

	if (ctx_flags & PFM_FL_SYSTEM_WIDE) {

		/*
		 * cannot block in this mode
		 */
		if (ctx_flags & PFM_FL_NOTIFY_BLOCK) {
			DPRINT(("cannot use blocking mode when in system wide monitoring\n"));
			return -EINVAL;
		}
	} else {
	}
	/* probably more to add here */

	return 0;
}

static int
pfm_setup_buffer_fmt(struct task_struct *task, struct file *filp, pfm_context_t *ctx, unsigned int ctx_flags,
		     unsigned int cpu, pfarg_context_t *arg)
{
	pfm_buffer_fmt_t *fmt = NULL;
	unsigned long size = 0UL;
	void *uaddr = NULL;
	void *fmt_arg = NULL;
	int ret = 0;
#define PFM_CTXARG_BUF_ARG(a)	(pfm_buffer_fmt_t *)(a+1)

	/* invoke and lock buffer format, if found */
	fmt = pfm_find_buffer_fmt(arg->ctx_smpl_buf_id);
	if (fmt == NULL) {
		DPRINT(("[%d] cannot find buffer format\n", task_pid_nr(task)));
		return -EINVAL;
	}

	/*
	 * buffer argument MUST be contiguous to pfarg_context_t
	 */
	if (fmt->fmt_arg_size) fmt_arg = PFM_CTXARG_BUF_ARG(arg);

	ret = pfm_buf_fmt_validate(fmt, task, ctx_flags, cpu, fmt_arg);

	DPRINT(("[%d] after validate(0x%x,%d,%p)=%d\n", task_pid_nr(task), ctx_flags, cpu, fmt_arg, ret));

	if (ret) goto error;

	/* link buffer format and context */
	ctx->ctx_buf_fmt = fmt;
	ctx->ctx_fl_is_sampling = 1; /* assume record() is defined */

	/*
	 * check if buffer format wants to use perfmon buffer allocation/mapping service
	 */
	ret = pfm_buf_fmt_getsize(fmt, task, ctx_flags, cpu, fmt_arg, &size);
	if (ret) goto error;

	if (size) {
		/*
		 * buffer is always remapped into the caller's address space
		 */
		ret = pfm_smpl_buffer_alloc(current, filp, ctx, size, &uaddr);
		if (ret) goto error;

		/* keep track of user address of buffer */
		arg->ctx_smpl_vaddr = uaddr;
	}
	ret = pfm_buf_fmt_init(fmt, task, ctx->ctx_smpl_hdr, ctx_flags, cpu, fmt_arg);

error:
	return ret;
}

static void
pfm_reset_pmu_state(pfm_context_t *ctx)
{
	int i;

	/*
	 * install reset values for PMC.
	 */
	for (i=1; PMC_IS_LAST(i) == 0; i++) {
		if (PMC_IS_IMPL(i) == 0) continue;
		ctx->ctx_pmcs[i] = PMC_DFL_VAL(i);
		DPRINT(("pmc[%d]=0x%lx\n", i, ctx->ctx_pmcs[i]));
	}
	/*
	 * PMD registers are set to 0UL when the context in memset()
	 */

	/*
	 * On context switched restore, we must restore ALL pmc and ALL pmd even
	 * when they are not actively used by the task. In UP, the incoming process
	 * may otherwise pick up left over PMC, PMD state from the previous process.
	 * As opposed to PMD, stale PMC can cause harm to the incoming
	 * process because they may change what is being measured.
	 * Therefore, we must systematically reinstall the entire
	 * PMC state. In SMP, the same thing is possible on the
	 * same CPU but also on between 2 CPUs.
	 *
	 * The problem with PMD is information leaking especially
	 * to user level when psr.sp=0
	 *
	 * There is unfortunately no easy way to avoid this problem
	 * on either UP or SMP. This definitively slows down the
	 * pfm_load_regs() function.
	 */

	 /*
	  * bitmask of all PMCs accessible to this context
	  *
	  * PMC0 is treated differently.
	  */
	ctx->ctx_all_pmcs[0] = pmu_conf->impl_pmcs[0] & ~0x1;

	/*
	 * bitmask of all PMDs that are accessible to this context
	 */
	ctx->ctx_all_pmds[0] = pmu_conf->impl_pmds[0];

	DPRINT(("<%d> all_pmcs=0x%lx all_pmds=0x%lx\n", ctx->ctx_fd, ctx->ctx_all_pmcs[0],ctx->ctx_all_pmds[0]));

	/*
	 * useful in case of re-enable after disable
	 */
	ctx->ctx_used_ibrs[0] = 0UL;
	ctx->ctx_used_dbrs[0] = 0UL;
}

static int
pfm_ctx_getsize(void *arg, size_t *sz)
{
	pfarg_context_t *req = (pfarg_context_t *)arg;
	pfm_buffer_fmt_t *fmt;

	*sz = 0;

	if (!pfm_uuid_cmp(req->ctx_smpl_buf_id, pfm_null_uuid)) return 0;

	fmt = pfm_find_buffer_fmt(req->ctx_smpl_buf_id);
	if (fmt == NULL) {
		DPRINT(("cannot find buffer format\n"));
		return -EINVAL;
	}
	/* get just enough to copy in user parameters */
	*sz = fmt->fmt_arg_size;
	DPRINT(("arg_size=%lu\n", *sz));

	return 0;
}



/*
 * cannot attach if :
 * 	- kernel task
 * 	- task not owned by caller
 * 	- task incompatible with context mode
 */
static int
pfm_task_incompatible(pfm_context_t *ctx, struct task_struct *task)
{
	/*
	 * no kernel task or task not owner by caller
	 */
	if (task->mm == NULL) {
		DPRINT(("task [%d] has not memory context (kernel thread)\n", task_pid_nr(task)));
		return -EPERM;
	}
	if (pfm_bad_permissions(task)) {
		DPRINT(("no permission to attach to  [%d]\n", task_pid_nr(task)));
		return -EPERM;
	}
	/*
	 * cannot block in self-monitoring mode
	 */
	if (CTX_OVFL_NOBLOCK(ctx) == 0 && task == current) {
		DPRINT(("cannot load a blocking context on self for [%d]\n", task_pid_nr(task)));
		return -EINVAL;
	}

	if (task->exit_state == EXIT_ZOMBIE) {
		DPRINT(("cannot attach to  zombie task [%d]\n", task_pid_nr(task)));
		return -EBUSY;
	}

	/*
	 * always ok for self
	 */
	if (task == current) return 0;

	if (!task_is_stopped_or_traced(task)) {
		DPRINT(("cannot attach to non-stopped task [%d] state=%ld\n", task_pid_nr(task), task->state));
		return -EBUSY;
	}
	/*
	 * make sure the task is off any CPU
	 */
	wait_task_inactive(task, 0);

	/* more to come... */

	return 0;
}

static int
pfm_get_task(pfm_context_t *ctx, pid_t pid, struct task_struct **task)
{
	struct task_struct *p = current;
	int ret;

	/* XXX: need to add more checks here */
	if (pid < 2) return -EPERM;

	if (pid != task_pid_vnr(current)) {

		read_lock(&tasklist_lock);

		p = find_task_by_vpid(pid);

		/* make sure task cannot go away while we operate on it */
		if (p) get_task_struct(p);

		read_unlock(&tasklist_lock);

		if (p == NULL) return -ESRCH;
	}

	ret = pfm_task_incompatible(ctx, p);
	if (ret == 0) {
		*task = p;
	} else if (p != current) {
		pfm_put_task(p);
	}
	return ret;
}



static int
pfm_context_create(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	pfarg_context_t *req = (pfarg_context_t *)arg;
	struct file *filp;
	struct path path;
	int ctx_flags;
	int fd;
	int ret;

	/* let's check the arguments first */
	ret = pfarg_is_sane(current, req);
	if (ret < 0)
		return ret;

	ctx_flags = req->ctx_flags;

	ret = -ENOMEM;

	fd = get_unused_fd();
	if (fd < 0)
		return fd;

	ctx = pfm_context_alloc(ctx_flags);
	if (!ctx)
		goto error;

	filp = pfm_alloc_file(ctx);
	if (IS_ERR(filp)) {
		ret = PTR_ERR(filp);
		goto error_file;
	}

	req->ctx_fd = ctx->ctx_fd = fd;

	/*
	 * does the user want to sample?
	 */
	if (pfm_uuid_cmp(req->ctx_smpl_buf_id, pfm_null_uuid)) {
		ret = pfm_setup_buffer_fmt(current, filp, ctx, ctx_flags, 0, req);
		if (ret)
			goto buffer_error;
	}

	DPRINT(("ctx=%p flags=0x%x system=%d notify_block=%d excl_idle=%d no_msg=%d ctx_fd=%d \n",
		ctx,
		ctx_flags,
		ctx->ctx_fl_system,
		ctx->ctx_fl_block,
		ctx->ctx_fl_excl_idle,
		ctx->ctx_fl_no_msg,
		ctx->ctx_fd));

	/*
	 * initialize soft PMU state
	 */
	pfm_reset_pmu_state(ctx);

	fd_install(fd, filp);

	return 0;

buffer_error:
	path = filp->f_path;
	put_filp(filp);
	path_put(&path);

	if (ctx->ctx_buf_fmt) {
		pfm_buf_fmt_exit(ctx->ctx_buf_fmt, current, NULL, regs);
	}
error_file:
	pfm_context_free(ctx);

error:
	put_unused_fd(fd);
	return ret;
}

static inline unsigned long
pfm_new_counter_value (pfm_counter_t *reg, int is_long_reset)
{
	unsigned long val = is_long_reset ? reg->long_reset : reg->short_reset;
	unsigned long new_seed, old_seed = reg->seed, mask = reg->mask;
	extern unsigned long carta_random32 (unsigned long seed);

	if (reg->flags & PFM_REGFL_RANDOM) {
		new_seed = carta_random32(old_seed);
		val -= (old_seed & mask);	/* counter values are negative numbers! */
		if ((mask >> 32) != 0)
			/* construct a full 64-bit random value: */
			new_seed |= carta_random32(old_seed >> 32) << 32;
		reg->seed = new_seed;
	}
	reg->lval = val;
	return val;
}

static void
pfm_reset_regs_masked(pfm_context_t *ctx, unsigned long *ovfl_regs, int is_long_reset)
{
	unsigned long mask = ovfl_regs[0];
	unsigned long reset_others = 0UL;
	unsigned long val;
	int i;

	/*
	 * now restore reset value on sampling overflowed counters
	 */
	mask >>= PMU_FIRST_COUNTER;
	for(i = PMU_FIRST_COUNTER; mask; i++, mask >>= 1) {

		if ((mask & 0x1UL) == 0UL) continue;

		ctx->ctx_pmds[i].val = val = pfm_new_counter_value(ctx->ctx_pmds+ i, is_long_reset);
		reset_others        |= ctx->ctx_pmds[i].reset_pmds[0];

		DPRINT_ovfl((" %s reset ctx_pmds[%d]=%lx\n", is_long_reset ? "long" : "short", i, val));
	}

	/*
	 * Now take care of resetting the other registers
	 */
	for(i = 0; reset_others; i++, reset_others >>= 1) {

		if ((reset_others & 0x1) == 0) continue;

		ctx->ctx_pmds[i].val = val = pfm_new_counter_value(ctx->ctx_pmds + i, is_long_reset);

		DPRINT_ovfl(("%s reset_others pmd[%d]=%lx\n",
			  is_long_reset ? "long" : "short", i, val));
	}
}

static void
pfm_reset_regs(pfm_context_t *ctx, unsigned long *ovfl_regs, int is_long_reset)
{
	unsigned long mask = ovfl_regs[0];
	unsigned long reset_others = 0UL;
	unsigned long val;
	int i;

	DPRINT_ovfl(("ovfl_regs=0x%lx is_long_reset=%d\n", ovfl_regs[0], is_long_reset));

	if (ctx->ctx_state == PFM_CTX_MASKED) {
		pfm_reset_regs_masked(ctx, ovfl_regs, is_long_reset);
		return;
	}

	/*
	 * now restore reset value on sampling overflowed counters
	 */
	mask >>= PMU_FIRST_COUNTER;
	for(i = PMU_FIRST_COUNTER; mask; i++, mask >>= 1) {

		if ((mask & 0x1UL) == 0UL) continue;

		val           = pfm_new_counter_value(ctx->ctx_pmds+ i, is_long_reset);
		reset_others |= ctx->ctx_pmds[i].reset_pmds[0];

		DPRINT_ovfl((" %s reset ctx_pmds[%d]=%lx\n", is_long_reset ? "long" : "short", i, val));

		pfm_write_soft_counter(ctx, i, val);
	}

	/*
	 * Now take care of resetting the other registers
	 */
	for(i = 0; reset_others; i++, reset_others >>= 1) {

		if ((reset_others & 0x1) == 0) continue;

		val = pfm_new_counter_value(ctx->ctx_pmds + i, is_long_reset);

		if (PMD_IS_COUNTING(i)) {
			pfm_write_soft_counter(ctx, i, val);
		} else {
			ia64_set_pmd(i, val);
		}
		DPRINT_ovfl(("%s reset_others pmd[%d]=%lx\n",
			  is_long_reset ? "long" : "short", i, val));
	}
	ia64_srlz_d();
}

static int
pfm_write_pmcs(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct task_struct *task;
	pfarg_reg_t *req = (pfarg_reg_t *)arg;
	unsigned long value, pmc_pm;
	unsigned long smpl_pmds, reset_pmds, impl_pmds;
	unsigned int cnum, reg_flags, flags, pmc_type;
	int i, can_access_pmu = 0, is_loaded, is_system, expert_mode;
	int is_monitor, is_counting, state;
	int ret = -EINVAL;
	pfm_reg_check_t	wr_func;
#define PFM_CHECK_PMC_PM(x, y, z) ((x)->ctx_fl_system ^ PMC_PM(y, z))

	state     = ctx->ctx_state;
	is_loaded = state == PFM_CTX_LOADED ? 1 : 0;
	is_system = ctx->ctx_fl_system;
	task      = ctx->ctx_task;
	impl_pmds = pmu_conf->impl_pmds[0];

	if (state == PFM_CTX_ZOMBIE) return -EINVAL;

	if (is_loaded) {
		/*
		 * In system wide and when the context is loaded, access can only happen
		 * when the caller is running on the CPU being monitored by the session.
		 * It does not have to be the owner (ctx_task) of the context per se.
		 */
		if (is_system && ctx->ctx_cpu != smp_processor_id()) {
			DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
			return -EBUSY;
		}
		can_access_pmu = GET_PMU_OWNER() == task || is_system ? 1 : 0;
	}
	expert_mode = pfm_sysctl.expert_mode; 

	for (i = 0; i < count; i++, req++) {

		cnum       = req->reg_num;
		reg_flags  = req->reg_flags;
		value      = req->reg_value;
		smpl_pmds  = req->reg_smpl_pmds[0];
		reset_pmds = req->reg_reset_pmds[0];
		flags      = 0;


		if (cnum >= PMU_MAX_PMCS) {
			DPRINT(("pmc%u is invalid\n", cnum));
			goto error;
		}

		pmc_type   = pmu_conf->pmc_desc[cnum].type;
		pmc_pm     = (value >> pmu_conf->pmc_desc[cnum].pm_pos) & 0x1;
		is_counting = (pmc_type & PFM_REG_COUNTING) == PFM_REG_COUNTING ? 1 : 0;
		is_monitor  = (pmc_type & PFM_REG_MONITOR) == PFM_REG_MONITOR ? 1 : 0;

		/*
		 * we reject all non implemented PMC as well
		 * as attempts to modify PMC[0-3] which are used
		 * as status registers by the PMU
		 */
		if ((pmc_type & PFM_REG_IMPL) == 0 || (pmc_type & PFM_REG_CONTROL) == PFM_REG_CONTROL) {
			DPRINT(("pmc%u is unimplemented or no-access pmc_type=%x\n", cnum, pmc_type));
			goto error;
		}
		wr_func = pmu_conf->pmc_desc[cnum].write_check;
		/*
		 * If the PMC is a monitor, then if the value is not the default:
		 * 	- system-wide session: PMCx.pm=1 (privileged monitor)
		 * 	- per-task           : PMCx.pm=0 (user monitor)
		 */
		if (is_monitor && value != PMC_DFL_VAL(cnum) && is_system ^ pmc_pm) {
			DPRINT(("pmc%u pmc_pm=%lu is_system=%d\n",
				cnum,
				pmc_pm,
				is_system));
			goto error;
		}

		if (is_counting) {
			/*
		 	 * enforce generation of overflow interrupt. Necessary on all
		 	 * CPUs.
		 	 */
			value |= 1 << PMU_PMC_OI;

			if (reg_flags & PFM_REGFL_OVFL_NOTIFY) {
				flags |= PFM_REGFL_OVFL_NOTIFY;
			}

			if (reg_flags & PFM_REGFL_RANDOM) flags |= PFM_REGFL_RANDOM;

			/* verify validity of smpl_pmds */
			if ((smpl_pmds & impl_pmds) != smpl_pmds) {
				DPRINT(("invalid smpl_pmds 0x%lx for pmc%u\n", smpl_pmds, cnum));
				goto error;
			}

			/* verify validity of reset_pmds */
			if ((reset_pmds & impl_pmds) != reset_pmds) {
				DPRINT(("invalid reset_pmds 0x%lx for pmc%u\n", reset_pmds, cnum));
				goto error;
			}
		} else {
			if (reg_flags & (PFM_REGFL_OVFL_NOTIFY|PFM_REGFL_RANDOM)) {
				DPRINT(("cannot set ovfl_notify or random on pmc%u\n", cnum));
				goto error;
			}
			/* eventid on non-counting monitors are ignored */
		}

		/*
		 * execute write checker, if any
		 */
		if (likely(expert_mode == 0 && wr_func)) {
			ret = (*wr_func)(task, ctx, cnum, &value, regs);
			if (ret) goto error;
			ret = -EINVAL;
		}

		/*
		 * no error on this register
		 */
		PFM_REG_RETFLAG_SET(req->reg_flags, 0);

		/*
		 * Now we commit the changes to the software state
		 */

		/*
		 * update overflow information
		 */
		if (is_counting) {
			/*
		 	 * full flag update each time a register is programmed
		 	 */
			ctx->ctx_pmds[cnum].flags = flags;

			ctx->ctx_pmds[cnum].reset_pmds[0] = reset_pmds;
			ctx->ctx_pmds[cnum].smpl_pmds[0]  = smpl_pmds;
			ctx->ctx_pmds[cnum].eventid       = req->reg_smpl_eventid;

			/*
			 * Mark all PMDS to be accessed as used.
			 *
			 * We do not keep track of PMC because we have to
			 * systematically restore ALL of them.
			 *
			 * We do not update the used_monitors mask, because
			 * if we have not programmed them, then will be in
			 * a quiescent state, therefore we will not need to
			 * mask/restore then when context is MASKED.
			 */
			CTX_USED_PMD(ctx, reset_pmds);
			CTX_USED_PMD(ctx, smpl_pmds);
			/*
		 	 * make sure we do not try to reset on
		 	 * restart because we have established new values
		 	 */
			if (state == PFM_CTX_MASKED) ctx->ctx_ovfl_regs[0] &= ~1UL << cnum;
		}
		/*
		 * Needed in case the user does not initialize the equivalent
		 * PMD. Clearing is done indirectly via pfm_reset_pmu_state() so there is no
		 * possible leak here.
		 */
		CTX_USED_PMD(ctx, pmu_conf->pmc_desc[cnum].dep_pmd[0]);

		/*
		 * keep track of the monitor PMC that we are using.
		 * we save the value of the pmc in ctx_pmcs[] and if
		 * the monitoring is not stopped for the context we also
		 * place it in the saved state area so that it will be
		 * picked up later by the context switch code.
		 *
		 * The value in ctx_pmcs[] can only be changed in pfm_write_pmcs().
		 *
		 * The value in th_pmcs[] may be modified on overflow, i.e.,  when
		 * monitoring needs to be stopped.
		 */
		if (is_monitor) CTX_USED_MONITOR(ctx, 1UL << cnum);

		/*
		 * update context state
		 */
		ctx->ctx_pmcs[cnum] = value;

		if (is_loaded) {
			/*
			 * write thread state
			 */
			if (is_system == 0) ctx->th_pmcs[cnum] = value;

			/*
			 * write hardware register if we can
			 */
			if (can_access_pmu) {
				ia64_set_pmc(cnum, value);
			}
#ifdef CONFIG_SMP
			else {
				/*
				 * per-task SMP only here
				 *
			 	 * we are guaranteed that the task is not running on the other CPU,
			 	 * we indicate that this PMD will need to be reloaded if the task
			 	 * is rescheduled on the CPU it ran last on.
			 	 */
				ctx->ctx_reload_pmcs[0] |= 1UL << cnum;
			}
#endif
		}

		DPRINT(("pmc[%u]=0x%lx ld=%d apmu=%d flags=0x%x all_pmcs=0x%lx used_pmds=0x%lx eventid=%ld smpl_pmds=0x%lx reset_pmds=0x%lx reloads_pmcs=0x%lx used_monitors=0x%lx ovfl_regs=0x%lx\n",
			  cnum,
			  value,
			  is_loaded,
			  can_access_pmu,
			  flags,
			  ctx->ctx_all_pmcs[0],
			  ctx->ctx_used_pmds[0],
			  ctx->ctx_pmds[cnum].eventid,
			  smpl_pmds,
			  reset_pmds,
			  ctx->ctx_reload_pmcs[0],
			  ctx->ctx_used_monitors[0],
			  ctx->ctx_ovfl_regs[0]));
	}

	/*
	 * make sure the changes are visible
	 */
	if (can_access_pmu) ia64_srlz_d();

	return 0;
error:
	PFM_REG_RETFLAG_SET(req->reg_flags, PFM_REG_RETFL_EINVAL);
	return ret;
}

static int
pfm_write_pmds(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct task_struct *task;
	pfarg_reg_t *req = (pfarg_reg_t *)arg;
	unsigned long value, hw_value, ovfl_mask;
	unsigned int cnum;
	int i, can_access_pmu = 0, state;
	int is_counting, is_loaded, is_system, expert_mode;
	int ret = -EINVAL;
	pfm_reg_check_t wr_func;


	state     = ctx->ctx_state;
	is_loaded = state == PFM_CTX_LOADED ? 1 : 0;
	is_system = ctx->ctx_fl_system;
	ovfl_mask = pmu_conf->ovfl_val;
	task      = ctx->ctx_task;

	if (unlikely(state == PFM_CTX_ZOMBIE)) return -EINVAL;

	/*
	 * on both UP and SMP, we can only write to the PMC when the task is
	 * the owner of the local PMU.
	 */
	if (likely(is_loaded)) {
		/*
		 * In system wide and when the context is loaded, access can only happen
		 * when the caller is running on the CPU being monitored by the session.
		 * It does not have to be the owner (ctx_task) of the context per se.
		 */
		if (unlikely(is_system && ctx->ctx_cpu != smp_processor_id())) {
			DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
			return -EBUSY;
		}
		can_access_pmu = GET_PMU_OWNER() == task || is_system ? 1 : 0;
	}
	expert_mode = pfm_sysctl.expert_mode; 

	for (i = 0; i < count; i++, req++) {

		cnum  = req->reg_num;
		value = req->reg_value;

		if (!PMD_IS_IMPL(cnum)) {
			DPRINT(("pmd[%u] is unimplemented or invalid\n", cnum));
			goto abort_mission;
		}
		is_counting = PMD_IS_COUNTING(cnum);
		wr_func     = pmu_conf->pmd_desc[cnum].write_check;

		/*
		 * execute write checker, if any
		 */
		if (unlikely(expert_mode == 0 && wr_func)) {
			unsigned long v = value;

			ret = (*wr_func)(task, ctx, cnum, &v, regs);
			if (ret) goto abort_mission;

			value = v;
			ret   = -EINVAL;
		}

		/*
		 * no error on this register
		 */
		PFM_REG_RETFLAG_SET(req->reg_flags, 0);

		/*
		 * now commit changes to software state
		 */
		hw_value = value;

		/*
		 * update virtualized (64bits) counter
		 */
		if (is_counting) {
			/*
			 * write context state
			 */
			ctx->ctx_pmds[cnum].lval = value;

			/*
			 * when context is load we use the split value
			 */
			if (is_loaded) {
				hw_value = value &  ovfl_mask;
				value    = value & ~ovfl_mask;
			}
		}
		/*
		 * update reset values (not just for counters)
		 */
		ctx->ctx_pmds[cnum].long_reset  = req->reg_long_reset;
		ctx->ctx_pmds[cnum].short_reset = req->reg_short_reset;

		/*
		 * update randomization parameters (not just for counters)
		 */
		ctx->ctx_pmds[cnum].seed = req->reg_random_seed;
		ctx->ctx_pmds[cnum].mask = req->reg_random_mask;

		/*
		 * update context value
		 */
		ctx->ctx_pmds[cnum].val  = value;

		/*
		 * Keep track of what we use
		 *
		 * We do not keep track of PMC because we have to
		 * systematically restore ALL of them.
		 */
		CTX_USED_PMD(ctx, PMD_PMD_DEP(cnum));

		/*
		 * mark this PMD register used as well
		 */
		CTX_USED_PMD(ctx, RDEP(cnum));

		/*
		 * make sure we do not try to reset on
		 * restart because we have established new values
		 */
		if (is_counting && state == PFM_CTX_MASKED) {
			ctx->ctx_ovfl_regs[0] &= ~1UL << cnum;
		}

		if (is_loaded) {
			/*
		 	 * write thread state
		 	 */
			if (is_system == 0) ctx->th_pmds[cnum] = hw_value;

			/*
			 * write hardware register if we can
			 */
			if (can_access_pmu) {
				ia64_set_pmd(cnum, hw_value);
			} else {
#ifdef CONFIG_SMP
				/*
			 	 * we are guaranteed that the task is not running on the other CPU,
			 	 * we indicate that this PMD will need to be reloaded if the task
			 	 * is rescheduled on the CPU it ran last on.
			 	 */
				ctx->ctx_reload_pmds[0] |= 1UL << cnum;
#endif
			}
		}

		DPRINT(("pmd[%u]=0x%lx ld=%d apmu=%d, hw_value=0x%lx ctx_pmd=0x%lx  short_reset=0x%lx "
			  "long_reset=0x%lx notify=%c seed=0x%lx mask=0x%lx used_pmds=0x%lx reset_pmds=0x%lx reload_pmds=0x%lx all_pmds=0x%lx ovfl_regs=0x%lx\n",
			cnum,
			value,
			is_loaded,
			can_access_pmu,
			hw_value,
			ctx->ctx_pmds[cnum].val,
			ctx->ctx_pmds[cnum].short_reset,
			ctx->ctx_pmds[cnum].long_reset,
			PMC_OVFL_NOTIFY(ctx, cnum) ? 'Y':'N',
			ctx->ctx_pmds[cnum].seed,
			ctx->ctx_pmds[cnum].mask,
			ctx->ctx_used_pmds[0],
			ctx->ctx_pmds[cnum].reset_pmds[0],
			ctx->ctx_reload_pmds[0],
			ctx->ctx_all_pmds[0],
			ctx->ctx_ovfl_regs[0]));
	}

	/*
	 * make changes visible
	 */
	if (can_access_pmu) ia64_srlz_d();

	return 0;

abort_mission:
	/*
	 * for now, we have only one possibility for error
	 */
	PFM_REG_RETFLAG_SET(req->reg_flags, PFM_REG_RETFL_EINVAL);
	return ret;
}

/*
 * By the way of PROTECT_CONTEXT(), interrupts are masked while we are in this function.
 * Therefore we know, we do not have to worry about the PMU overflow interrupt. If an
 * interrupt is delivered during the call, it will be kept pending until we leave, making
 * it appears as if it had been generated at the UNPROTECT_CONTEXT(). At least we are
 * guaranteed to return consistent data to the user, it may simply be old. It is not
 * trivial to treat the overflow while inside the call because you may end up in
 * some module sampling buffer code causing deadlocks.
 */
static int
pfm_read_pmds(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct task_struct *task;
	unsigned long val = 0UL, lval, ovfl_mask, sval;
	pfarg_reg_t *req = (pfarg_reg_t *)arg;
	unsigned int cnum, reg_flags = 0;
	int i, can_access_pmu = 0, state;
	int is_loaded, is_system, is_counting, expert_mode;
	int ret = -EINVAL;
	pfm_reg_check_t rd_func;

	/*
	 * access is possible when loaded only for
	 * self-monitoring tasks or in UP mode
	 */

	state     = ctx->ctx_state;
	is_loaded = state == PFM_CTX_LOADED ? 1 : 0;
	is_system = ctx->ctx_fl_system;
	ovfl_mask = pmu_conf->ovfl_val;
	task      = ctx->ctx_task;

	if (state == PFM_CTX_ZOMBIE) return -EINVAL;

	if (likely(is_loaded)) {
		/*
		 * In system wide and when the context is loaded, access can only happen
		 * when the caller is running on the CPU being monitored by the session.
		 * It does not have to be the owner (ctx_task) of the context per se.
		 */
		if (unlikely(is_system && ctx->ctx_cpu != smp_processor_id())) {
			DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
			return -EBUSY;
		}
		/*
		 * this can be true when not self-monitoring only in UP
		 */
		can_access_pmu = GET_PMU_OWNER() == task || is_system ? 1 : 0;

		if (can_access_pmu) ia64_srlz_d();
	}
	expert_mode = pfm_sysctl.expert_mode; 

	DPRINT(("ld=%d apmu=%d ctx_state=%d\n",
		is_loaded,
		can_access_pmu,
		state));

	/*
	 * on both UP and SMP, we can only read the PMD from the hardware register when
	 * the task is the owner of the local PMU.
	 */

	for (i = 0; i < count; i++, req++) {

		cnum        = req->reg_num;
		reg_flags   = req->reg_flags;

		if (unlikely(!PMD_IS_IMPL(cnum))) goto error;
		/*
		 * we can only read the register that we use. That includes
		 * the one we explicitly initialize AND the one we want included
		 * in the sampling buffer (smpl_regs).
		 *
		 * Having this restriction allows optimization in the ctxsw routine
		 * without compromising security (leaks)
		 */
		if (unlikely(!CTX_IS_USED_PMD(ctx, cnum))) goto error;

		sval        = ctx->ctx_pmds[cnum].val;
		lval        = ctx->ctx_pmds[cnum].lval;
		is_counting = PMD_IS_COUNTING(cnum);

		/*
		 * If the task is not the current one, then we check if the
		 * PMU state is still in the local live register due to lazy ctxsw.
		 * If true, then we read directly from the registers.
		 */
		if (can_access_pmu){
			val = ia64_get_pmd(cnum);
		} else {
			/*
			 * context has been saved
			 * if context is zombie, then task does not exist anymore.
			 * In this case, we use the full value saved in the context (pfm_flush_regs()).
			 */
			val = is_loaded ? ctx->th_pmds[cnum] : 0UL;
		}
		rd_func = pmu_conf->pmd_desc[cnum].read_check;

		if (is_counting) {
			/*
			 * XXX: need to check for overflow when loaded
			 */
			val &= ovfl_mask;
			val += sval;
		}

		/*
		 * execute read checker, if any
		 */
		if (unlikely(expert_mode == 0 && rd_func)) {
			unsigned long v = val;
			ret = (*rd_func)(ctx->ctx_task, ctx, cnum, &v, regs);
			if (ret) goto error;
			val = v;
			ret = -EINVAL;
		}

		PFM_REG_RETFLAG_SET(reg_flags, 0);

		DPRINT(("pmd[%u]=0x%lx\n", cnum, val));

		/*
		 * update register return value, abort all if problem during copy.
		 * we only modify the reg_flags field. no check mode is fine because
		 * access has been verified upfront in sys_perfmonctl().
		 */
		req->reg_value            = val;
		req->reg_flags            = reg_flags;
		req->reg_last_reset_val   = lval;
	}

	return 0;

error:
	PFM_REG_RETFLAG_SET(req->reg_flags, PFM_REG_RETFL_EINVAL);
	return ret;
}

int
pfm_mod_write_pmcs(struct task_struct *task, void *req, unsigned int nreq, struct pt_regs *regs)
{
	pfm_context_t *ctx;

	if (req == NULL) return -EINVAL;

 	ctx = GET_PMU_CTX();

	if (ctx == NULL) return -EINVAL;

	/*
	 * for now limit to current task, which is enough when calling
	 * from overflow handler
	 */
	if (task != current && ctx->ctx_fl_system == 0) return -EBUSY;

	return pfm_write_pmcs(ctx, req, nreq, regs);
}
EXPORT_SYMBOL(pfm_mod_write_pmcs);

int
pfm_mod_read_pmds(struct task_struct *task, void *req, unsigned int nreq, struct pt_regs *regs)
{
	pfm_context_t *ctx;

	if (req == NULL) return -EINVAL;

 	ctx = GET_PMU_CTX();

	if (ctx == NULL) return -EINVAL;

	/*
	 * for now limit to current task, which is enough when calling
	 * from overflow handler
	 */
	if (task != current && ctx->ctx_fl_system == 0) return -EBUSY;

	return pfm_read_pmds(ctx, req, nreq, regs);
}
EXPORT_SYMBOL(pfm_mod_read_pmds);

/*
 * Only call this function when a process it trying to
 * write the debug registers (reading is always allowed)
 */
int
pfm_use_debug_registers(struct task_struct *task)
{
	pfm_context_t *ctx = task->thread.pfm_context;
	unsigned long flags;
	int ret = 0;

	if (pmu_conf->use_rr_dbregs == 0) return 0;

	DPRINT(("called for [%d]\n", task_pid_nr(task)));

	/*
	 * do it only once
	 */
	if (task->thread.flags & IA64_THREAD_DBG_VALID) return 0;

	/*
	 * Even on SMP, we do not need to use an atomic here because
	 * the only way in is via ptrace() and this is possible only when the
	 * process is stopped. Even in the case where the ctxsw out is not totally
	 * completed by the time we come here, there is no way the 'stopped' process
	 * could be in the middle of fiddling with the pfm_write_ibr_dbr() routine.
	 * So this is always safe.
	 */
	if (ctx && ctx->ctx_fl_using_dbreg == 1) return -1;

	LOCK_PFS(flags);

	/*
	 * We cannot allow setting breakpoints when system wide monitoring
	 * sessions are using the debug registers.
	 */
	if (pfm_sessions.pfs_sys_use_dbregs> 0)
		ret = -1;
	else
		pfm_sessions.pfs_ptrace_use_dbregs++;

	DPRINT(("ptrace_use_dbregs=%u  sys_use_dbregs=%u by [%d] ret = %d\n",
		  pfm_sessions.pfs_ptrace_use_dbregs,
		  pfm_sessions.pfs_sys_use_dbregs,
		  task_pid_nr(task), ret));

	UNLOCK_PFS(flags);

	return ret;
}

/*
 * This function is called for every task that exits with the
 * IA64_THREAD_DBG_VALID set. This indicates a task which was
 * able to use the debug registers for debugging purposes via
 * ptrace(). Therefore we know it was not using them for
 * perfmormance monitoring, so we only decrement the number
 * of "ptraced" debug register users to keep the count up to date
 */
int
pfm_release_debug_registers(struct task_struct *task)
{
	unsigned long flags;
	int ret;

	if (pmu_conf->use_rr_dbregs == 0) return 0;

	LOCK_PFS(flags);
	if (pfm_sessions.pfs_ptrace_use_dbregs == 0) {
		printk(KERN_ERR "perfmon: invalid release for [%d] ptrace_use_dbregs=0\n", task_pid_nr(task));
		ret = -1;
	}  else {
		pfm_sessions.pfs_ptrace_use_dbregs--;
		ret = 0;
	}
	UNLOCK_PFS(flags);

	return ret;
}

static int
pfm_restart(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct task_struct *task;
	pfm_buffer_fmt_t *fmt;
	pfm_ovfl_ctrl_t rst_ctrl;
	int state, is_system;
	int ret = 0;

	state     = ctx->ctx_state;
	fmt       = ctx->ctx_buf_fmt;
	is_system = ctx->ctx_fl_system;
	task      = PFM_CTX_TASK(ctx);

	switch(state) {
		case PFM_CTX_MASKED:
			break;
		case PFM_CTX_LOADED: 
			if (CTX_HAS_SMPL(ctx) && fmt->fmt_restart_active) break;
			/* fall through */
		case PFM_CTX_UNLOADED:
		case PFM_CTX_ZOMBIE:
			DPRINT(("invalid state=%d\n", state));
			return -EBUSY;
		default:
			DPRINT(("state=%d, cannot operate (no active_restart handler)\n", state));
			return -EINVAL;
	}

	/*
 	 * In system wide and when the context is loaded, access can only happen
 	 * when the caller is running on the CPU being monitored by the session.
 	 * It does not have to be the owner (ctx_task) of the context per se.
 	 */
	if (is_system && ctx->ctx_cpu != smp_processor_id()) {
		DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
		return -EBUSY;
	}

	/* sanity check */
	if (unlikely(task == NULL)) {
		printk(KERN_ERR "perfmon: [%d] pfm_restart no task\n", task_pid_nr(current));
		return -EINVAL;
	}

	if (task == current || is_system) {

		fmt = ctx->ctx_buf_fmt;

		DPRINT(("restarting self %d ovfl=0x%lx\n",
			task_pid_nr(task),
			ctx->ctx_ovfl_regs[0]));

		if (CTX_HAS_SMPL(ctx)) {

			prefetch(ctx->ctx_smpl_hdr);

			rst_ctrl.bits.mask_monitoring = 0;
			rst_ctrl.bits.reset_ovfl_pmds = 0;

			if (state == PFM_CTX_LOADED)
				ret = pfm_buf_fmt_restart_active(fmt, task, &rst_ctrl, ctx->ctx_smpl_hdr, regs);
			else
				ret = pfm_buf_fmt_restart(fmt, task, &rst_ctrl, ctx->ctx_smpl_hdr, regs);
		} else {
			rst_ctrl.bits.mask_monitoring = 0;
			rst_ctrl.bits.reset_ovfl_pmds = 1;
		}

		if (ret == 0) {
			if (rst_ctrl.bits.reset_ovfl_pmds)
				pfm_reset_regs(ctx, ctx->ctx_ovfl_regs, PFM_PMD_LONG_RESET);

			if (rst_ctrl.bits.mask_monitoring == 0) {
				DPRINT(("resuming monitoring for [%d]\n", task_pid_nr(task)));

				if (state == PFM_CTX_MASKED) pfm_restore_monitoring(task);
			} else {
				DPRINT(("keeping monitoring stopped for [%d]\n", task_pid_nr(task)));

				// cannot use pfm_stop_monitoring(task, regs);
			}
		}
		/*
		 * clear overflowed PMD mask to remove any stale information
		 */
		ctx->ctx_ovfl_regs[0] = 0UL;

		/*
		 * back to LOADED state
		 */
		ctx->ctx_state = PFM_CTX_LOADED;

		/*
		 * XXX: not really useful for self monitoring
		 */
		ctx->ctx_fl_can_restart = 0;

		return 0;
	}

	/* 
	 * restart another task
	 */

	/*
	 * When PFM_CTX_MASKED, we cannot issue a restart before the previous 
	 * one is seen by the task.
	 */
	if (state == PFM_CTX_MASKED) {
		if (ctx->ctx_fl_can_restart == 0) return -EINVAL;
		/*
		 * will prevent subsequent restart before this one is
		 * seen by other task
		 */
		ctx->ctx_fl_can_restart = 0;
	}

	/*
	 * if blocking, then post the semaphore is PFM_CTX_MASKED, i.e.
	 * the task is blocked or on its way to block. That's the normal
	 * restart path. If the monitoring is not masked, then the task
	 * can be actively monitoring and we cannot directly intervene.
	 * Therefore we use the trap mechanism to catch the task and
	 * force it to reset the buffer/reset PMDs.
	 *
	 * if non-blocking, then we ensure that the task will go into
	 * pfm_handle_work() before returning to user mode.
	 *
	 * We cannot explicitly reset another task, it MUST always
	 * be done by the task itself. This works for system wide because
	 * the tool that is controlling the session is logically doing 
	 * "self-monitoring".
	 */
	if (CTX_OVFL_NOBLOCK(ctx) == 0 && state == PFM_CTX_MASKED) {
		DPRINT(("unblocking [%d] \n", task_pid_nr(task)));
		complete(&ctx->ctx_restart_done);
	} else {
		DPRINT(("[%d] armed exit trap\n", task_pid_nr(task)));

		ctx->ctx_fl_trap_reason = PFM_TRAP_REASON_RESET;

		PFM_SET_WORK_PENDING(task, 1);

		set_notify_resume(task);

		/*
		 * XXX: send reschedule if task runs on another CPU
		 */
	}
	return 0;
}

static int
pfm_debug(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	unsigned int m = *(unsigned int *)arg;

	pfm_sysctl.debug = m == 0 ? 0 : 1;

	printk(KERN_INFO "perfmon debugging %s (timing reset)\n", pfm_sysctl.debug ? "on" : "off");

	if (m == 0) {
		memset(pfm_stats, 0, sizeof(pfm_stats));
		for(m=0; m < NR_CPUS; m++) pfm_stats[m].pfm_ovfl_intr_cycles_min = ~0UL;
	}
	return 0;
}

/*
 * arg can be NULL and count can be zero for this function
 */
static int
pfm_write_ibr_dbr(int mode, pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct thread_struct *thread = NULL;
	struct task_struct *task;
	pfarg_dbreg_t *req = (pfarg_dbreg_t *)arg;
	unsigned long flags;
	dbreg_t dbreg;
	unsigned int rnum;
	int first_time;
	int ret = 0, state;
	int i, can_access_pmu = 0;
	int is_system, is_loaded;

	if (pmu_conf->use_rr_dbregs == 0) return -EINVAL;

	state     = ctx->ctx_state;
	is_loaded = state == PFM_CTX_LOADED ? 1 : 0;
	is_system = ctx->ctx_fl_system;
	task      = ctx->ctx_task;

	if (state == PFM_CTX_ZOMBIE) return -EINVAL;

	/*
	 * on both UP and SMP, we can only write to the PMC when the task is
	 * the owner of the local PMU.
	 */
	if (is_loaded) {
		thread = &task->thread;
		/*
		 * In system wide and when the context is loaded, access can only happen
		 * when the caller is running on the CPU being monitored by the session.
		 * It does not have to be the owner (ctx_task) of the context per se.
		 */
		if (unlikely(is_system && ctx->ctx_cpu != smp_processor_id())) {
			DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
			return -EBUSY;
		}
		can_access_pmu = GET_PMU_OWNER() == task || is_system ? 1 : 0;
	}

	/*
	 * we do not need to check for ipsr.db because we do clear ibr.x, dbr.r, and dbr.w
	 * ensuring that no real breakpoint can be installed via this call.
	 *
	 * IMPORTANT: regs can be NULL in this function
	 */

	first_time = ctx->ctx_fl_using_dbreg == 0;

	/*
	 * don't bother if we are loaded and task is being debugged
	 */
	if (is_loaded && (thread->flags & IA64_THREAD_DBG_VALID) != 0) {
		DPRINT(("debug registers already in use for [%d]\n", task_pid_nr(task)));
		return -EBUSY;
	}

	/*
	 * check for debug registers in system wide mode
	 *
	 * If though a check is done in pfm_context_load(),
	 * we must repeat it here, in case the registers are
	 * written after the context is loaded
	 */
	if (is_loaded) {
		LOCK_PFS(flags);

		if (first_time && is_system) {
			if (pfm_sessions.pfs_ptrace_use_dbregs)
				ret = -EBUSY;
			else
				pfm_sessions.pfs_sys_use_dbregs++;
		}
		UNLOCK_PFS(flags);
	}

	if (ret != 0) return ret;

	/*
	 * mark ourself as user of the debug registers for
	 * perfmon purposes.
	 */
	ctx->ctx_fl_using_dbreg = 1;

	/*
 	 * clear hardware registers to make sure we don't
 	 * pick up stale state.
	 *
	 * for a system wide session, we do not use
	 * thread.dbr, thread.ibr because this process
	 * never leaves the current CPU and the state
	 * is shared by all processes running on it
 	 */
	if (first_time && can_access_pmu) {
		DPRINT(("[%d] clearing ibrs, dbrs\n", task_pid_nr(task)));
		for (i=0; i < pmu_conf->num_ibrs; i++) {
			ia64_set_ibr(i, 0UL);
			ia64_dv_serialize_instruction();
		}
		ia64_srlz_i();
		for (i=0; i < pmu_conf->num_dbrs; i++) {
			ia64_set_dbr(i, 0UL);
			ia64_dv_serialize_data();
		}
		ia64_srlz_d();
	}

	/*
	 * Now install the values into the registers
	 */
	for (i = 0; i < count; i++, req++) {

		rnum      = req->dbreg_num;
		dbreg.val = req->dbreg_value;

		ret = -EINVAL;

		if ((mode == PFM_CODE_RR && rnum >= PFM_NUM_IBRS) || ((mode == PFM_DATA_RR) && rnum >= PFM_NUM_DBRS)) {
			DPRINT(("invalid register %u val=0x%lx mode=%d i=%d count=%d\n",
				  rnum, dbreg.val, mode, i, count));

			goto abort_mission;
		}

		/*
		 * make sure we do not install enabled breakpoint
		 */
		if (rnum & 0x1) {
			if (mode == PFM_CODE_RR)
				dbreg.ibr.ibr_x = 0;
			else
				dbreg.dbr.dbr_r = dbreg.dbr.dbr_w = 0;
		}

		PFM_REG_RETFLAG_SET(req->dbreg_flags, 0);

		/*
		 * Debug registers, just like PMC, can only be modified
		 * by a kernel call. Moreover, perfmon() access to those
		 * registers are centralized in this routine. The hardware
		 * does not modify the value of these registers, therefore,
		 * if we save them as they are written, we can avoid having
		 * to save them on context switch out. This is made possible
		 * by the fact that when perfmon uses debug registers, ptrace()
		 * won't be able to modify them concurrently.
		 */
		if (mode == PFM_CODE_RR) {
			CTX_USED_IBR(ctx, rnum);

			if (can_access_pmu) {
				ia64_set_ibr(rnum, dbreg.val);
				ia64_dv_serialize_instruction();
			}

			ctx->ctx_ibrs[rnum] = dbreg.val;

			DPRINT(("write ibr%u=0x%lx used_ibrs=0x%x ld=%d apmu=%d\n",
				rnum, dbreg.val, ctx->ctx_used_ibrs[0], is_loaded, can_access_pmu));
		} else {
			CTX_USED_DBR(ctx, rnum);

			if (can_access_pmu) {
				ia64_set_dbr(rnum, dbreg.val);
				ia64_dv_serialize_data();
			}
			ctx->ctx_dbrs[rnum] = dbreg.val;

			DPRINT(("write dbr%u=0x%lx used_dbrs=0x%x ld=%d apmu=%d\n",
				rnum, dbreg.val, ctx->ctx_used_dbrs[0], is_loaded, can_access_pmu));
		}
	}

	return 0;

abort_mission:
	/*
	 * in case it was our first attempt, we undo the global modifications
	 */
	if (first_time) {
		LOCK_PFS(flags);
		if (ctx->ctx_fl_system) {
			pfm_sessions.pfs_sys_use_dbregs--;
		}
		UNLOCK_PFS(flags);
		ctx->ctx_fl_using_dbreg = 0;
	}
	/*
	 * install error return flag
	 */
	PFM_REG_RETFLAG_SET(req->dbreg_flags, PFM_REG_RETFL_EINVAL);

	return ret;
}

static int
pfm_write_ibrs(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	return pfm_write_ibr_dbr(PFM_CODE_RR, ctx, arg, count, regs);
}

static int
pfm_write_dbrs(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	return pfm_write_ibr_dbr(PFM_DATA_RR, ctx, arg, count, regs);
}

int
pfm_mod_write_ibrs(struct task_struct *task, void *req, unsigned int nreq, struct pt_regs *regs)
{
	pfm_context_t *ctx;

	if (req == NULL) return -EINVAL;

 	ctx = GET_PMU_CTX();

	if (ctx == NULL) return -EINVAL;

	/*
	 * for now limit to current task, which is enough when calling
	 * from overflow handlerthe /
	if (task != current && ctx->ctx_fl_system == 0) return -EBUSY;

	ing Unipfm_write_ibrs(ctx, req, n perfregs);
}
EXPORT_SYMBOL(e inmodnitial vers);

int
h Venkitachaladersistruct 
 * _modifie*
 * , void * perfunsigned intfmon.c modifiept_was ephags)
{
	e incontext_t *ctx).
 usedreqMoniNULLoring Unit INVAL;

 	ctx = GET_PMU_CTX(BM C Perf1.x
 .x is a rewrite of perfmo	/*the peor now limit toprogram t-1.x bw/*
 * This file implements the perfmon-2 subsystem which is used
 * to program the IA-64 Performance Monitoring Unit (PMU).
 *
 * The initial  was on of perfmon.c was written by
 * Ganesh Venkitachala wasBM C
staticand
.
 *
get_featuressh VeCo.
 *
 * Vers by Steparg,and
 countvid Mosberger, Hewlett Packararg <linux/m * Vmon-2 ( <linux/interrupt.)arg).
 *
q->ft_version = PFM_VERSION;
 *
 * Th0rittrfmon
 */

#inclstopodule.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#included Mosberger, Hewtlett;de <linuxd for perfmon-1.xq_file.CTX_TASKfmon);
	nd
 fmone, isrmance Copyinux/ ux/s= IA-64 Perinux/lude.h>
#incmp.h>
#incluformance Copyright (Co.
 *
 must be attached Hewissue the inux command (includes LOADED,MASKED,ZOMBIE)ich is used <linu=<linux/polUN <linuckard Co.
 *
 * Copyrig he pIn mance Mwide ty.h implinuxclude <lis loaded, access can only happen
#inclinclude <aallerrinsrunning olude <CPU beude monitored bylinux/esx/se.
#inclut does not have Hewb<linuxownern, He_
 * ) ofude <asm/intrper se
#inclis usedx/pagemap.he IA-64 Percputo psmp_procludor_id()) {
		DPRINT(("shouldinclinclude <asCPU%d\n",*
 * perfmon )clud *
 * Tht (PMU).	}
FM_CTX_UNL
 * t[%d] IA-ude <l=%dfs.h>
#inc=t load
		d forpid_nr(inux/poll.h>
#inc)fineinux/v
	ux/pagemapask *right (ide <asm/emode, we ne
#inclupdlinuinuxPMU directlythe pty.hinuxuser levellinux/#include fmon. Co
 *  may/systhe pnecludarilyinclude creator#include <asm/d.ich is used monitorinne PFrighhe pUPFM_CTlocalOMBIEfirst/
#de/
#defdisable dcr pp/
#de/but a64_se#inc(_IA64_REG_CR_DCR, *
 * gepth of message queue
) & ~ messDCR_PPsk */*
 * drlz_ie Eranw */
#def PFM_CT_NUM_Pcpuinfoxsw */

/inux/PUINFO_CLEARSKED		f a PMU_CTXQ_EMPsgq_head ==/capa
#incluing, <asm/(g)-.ixsw */

/dule.lear_psr_pptx_msgq_head ==ure:
 * 	bit0  d du */

#defixsw */

/*
 * psr(lett ->pp =ux/i */
#defin0CTX_LOright (per-
 * terflich iss used
 * t= program  ctxsw *bit2-3 : reserved at kernelxt is cmented
 * 	bit1   :uend marker
 #inclit2-3 : reservedatthe context is 0 /* nhas pmc.pm
 * 	bit5 u : pmc 	} elsene PF#incl =x/filerger, Hd
 * IMPL		0x0 /* not implemented at all */
#define PFM_REG_IMPL		0x1 /*#inclister implesgq_head == : reservedve area and31: reseat ne <lresh>
#ulexsw */

/h>
#includaved_REG_NO implemOADED		2	/* co=textloade PFM_CTX_MAS2 /* ask *}ude <linux/initt.h>
#include <linart/vmalloc.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/list.h>
#includee <linux/vfs.h>
#include <linux/smp.h>
#include <linux/pagemap.h>
#include <linux/mount <linux/co!<linux/polinclude <linux/tracehook.h>

#include <asm/errno.h>
#include <asm/intrinsics.h>
#include <asm/page.h>
#include <asm/perfmon.h>
#include <asm/processor.h>
#include <asm/signal.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/delay.h>

#ifdef CONFIG_PERFMON
/*
 * perfmon context state
 */
#define PFM_CTX_UNLOADED	1	/* context is not loaded onto any task */
#define PFM_CTX_Lis masked due to overflow */
#define PFM_CTX_ZOMBIE		4	/* owner of the context is closing it */

#define PFM_INVALID_ACTIVATION	(~0UL)

#define PFM_NUM_PMC_REGS	64	/* PMC save area for ctxarker
 * 	bietcontext is cpsr.pp(C) 1 	bit4   : pmc has pmc.pm
 * 	bit5   : p1.pm field onl999- PFM_CTX_ZO_NUM_PMD_Rty.hq_tail)

/*
 * type of a PMUSETster (bitmask).
 * bitmask structurartplemented at al1: reserved
mplemented
 *set1   : end marker enarea for ct */

/*
 * depth of message queue
 */
#define PFM_MAX_MSGS		32
#de| e PFM_CTXQ_EMPTY(g)	((g)->ctx_msgqtrols a countter (has pmc.state
  pmd is used as cIA-64 Perounter
 * 	bit6-7 :: registpmc_desc[i].type & PFM_REG_CONL)  == PFM_REG_CONNOTIMPL		0x0 ner octivsinglemented at al
#define PFMREG_IMPL		0x1 /* register im ((pmented */
#define PFM_REG_END		0x[i].dep_pmd[0i)  ((pmu_conf->pmc_desc[i].type &inux1: reserved
 inux2<<4	  IA6timCTX_ZO
 * tis M_REG_MOdNITOR) /* a monitor + pmc.oi+ PMD messPSR_UP.pm field onl4_NUM_DBG_REGS
#define PFM_NUM_DBRS	  IA64_NUM_DBG_PMC with a pm1(0x4<<4|PFM_REG_IMPfmon
 */

#include pmc_resegister */
#define	PFM_REG_CONFIG		(0x8<<4|PFM_REG_IMPL) /* configurat <linuregupt.h>
#include <
#defins.h>
#	Eranian and
 cnumne PFM_ine PFM_ret =.
 *
 * CopyC) 1(i: pmc i <#inclu; i++of pe++MC_PMD__use =f pe->
#deused_PTY(f (!PMC_IS_IMPL(_use)) goto abort_mial.h>c contr] |= 1Uvalueq_fiMC_DFL_VACTX_USE ((n)ile.age RETFLAG#defiused_dbrsflags, 0 64)
#M_CTX_UNL
#define s[(n pmc[%u]=0x%lxloadednumof ped_dbrs[(n)>		(0x4<<4|PFM_REG_
tx,n) 	(ctx)-:
#define CTX_USES_DBREGS(ctx)	(((pfm_cdefine CTX_US_of percluding Uniret/init.h>
#include <lcheckpmd[0_exisgister */
#define	PFclude <linuxd for perfmong, *trs[0] |= (mask)SRCH).
 *
ad_lock(&
 * listt_inf 64)
do_each_th_sys (g, 6-7 : rused
->O_GET(.dule.h>
#iner
 *tx ctxsw */
PMD used	DBR(couT(v)x4<<}Co
 leCPUINFO_GET()	pfm_g;
out:fm_systun_info) |= (v)
#define PFt *)(ctx))AR(v)	pfm_get_cpu_v:efin ontctx=%ploaderetded oask n */

#define PFM_CPUINFO_CLEAR(vo.
 *
 ics./vmalloc.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linuxd for perfmon-1.xude <linux/_GET(r perfmon-_GET(ude <linuxdule.h>
#includolqresEranian along ((pfm;
#ifndef CONFIG_SMPal_irq_disable)
 *
 * <asm/pmd[0]
x is ;
#endif!= 0UL)
ics.upt.h>
#include < removedtx, mask) (ctx)- + sp*pmcs_source,t codtext accne PFM_themon rs[0] |= (maspleme <linux/vfs.h>
#inc,& PF/linine PFc.pm  <linux/smp.h>
#include <linux/pagemap.h>
#include <linux/mout.h>
#inc<asm/pagics.perfmod tos.h> FM_Rerminatedlinux/include <linux/co->pmc_desc[
#include e PFM_CTX_UNLcansystcy intontextl.h>vali to ploaded on#define_used_ remopid\
		sh>
#include <lM_REG_IMPL))

/* XXX: thesM_CTX_UNLock_irqsntext usingseful t))); \_CODE_ock_irqsa IA-64 Perforc, task_pidMU ove % 6/polOVFL_NOBLOC>
#incMonitN
/*ent))); \
	} er
 * 	bit6->pidock_irq_save ctx %p byuse b_infr.h>
#de <asself\n"ask */
#define f perfmine P1UL<<(include 
 * fmon avai))); \
	} wh) |= cludeerfmo6-7 : red ctx %p  by [%d]\n", le(0)

#_nr(current))); \
	} wh
	dosk */
#definfine k, f); \
	}
 *
 * Copyright ( <asm/errno.tx_telfG_REGS
#defim/pa/* PMC save area forN
/*
 * to program ine PFM_CTX_UNLOspin_unlock_irqrestore(&(c)->ctx_ly [%dirqst))); \
		spin_lock_irqs#definDBR(cerrorT_CTX_NO_GET()=OPRINT(pfm_sys overfl       et PMU conp by [%d]a<asm/intro
 *     c, ta range|PFMtrictions,asked dR(ct

#defthatrinssor.h>debugged	/* PMC saveile(0)

#define UNPROTECet_cpu_var_GET(->((pfm &f pmc.THREAD_DBG<< (Ilock_irNOPRINT(cPFM_CTX \
		spin_lock_irqsave(&(h)->ctx_ber)
#de,hile(0)


#ifET_ACTIVATION()	pf(current))); \
	} #definCTX_NOIRQ(c) \x4<<	
		D_PFS(((pfm 64)
#save area for ctxsw0)
#d PFM_Ral.h>s.pfs_ptrace_ush/linC wiACTIVArq_save ctx %p by [%d]text eful foinurre); \
		su_vae PFM_REG_CONTROL		(0xc)	(c)->ctx_last_actented */
#d== PFM_Ro {} while(sysdif /* CONF++pfm_geed ctx %p  byWNER(tinne PsROTE_PMU_OWNER()=%udefine PFM_REG_CONTROL,} whi0)
#define GET_PMU_OWNER()	; pfm_ges useful for maskTIVATIne P	
#in(t) 	do {} while(0)
#d
	do TX_NOIRQ(c) \
	doright (SMP		spin_-rrno. : reserved
mpliegiself- * 	bit0  	/* Pthe pThe programm \
		spil expectsh)		(h)->ctothe pbe pinan aon arocesthr filoufl_is_gnal.h>
#he pHerew */take/sysng it */

ogram toces_fl_isthe vASK(h)		(asm/intrinsics.h>. No

#deperfmwner ofotherS_OVFwill/bitollowdefine lags, val)iclude via shedM_REaffinity(#incllinux/bitdone <asm/siements ON()

priohich ]\n"thide <llne PFM_DEB\
	do rrno: keep )
#ek#incile(0)_irqral.h>k_irquppone Gto* co onich is  interrmp.h>
#incluurrentext state
 */
#def)->ctx_loctx_last_aright (999-finer.h>
p0 must beich is ; \
	} whi\
		if fine LOC(PMC0_HAasked. This i interr(c, f) \
	do TX_NOIRQ(c) l(a) \
	dh)->ctx_ATION	(~0UL)inuxpedFL(cmpis poin4	/* Pthe vIncludepreviousef CONFIG_as zombieprintn it[%d] removREG_C(has pdefiaveND		0x)., varefoue of OADED	1systsee4-bie ne the vIfeset_eefdef CONFI*
 * 64pid_nis an does 0x1UL)

#ent)); prXXX:/
#des>
#inclatomicich is M_CTX_UNLbext_recmpxchg() old_o prot newal;		/*#define vation_q_disable)
nst PMU overflDPRINT_ovfl(ol		sp*
 * igned loacqNOPR/
	unsigned long	lon is while, sizeofodule.h>
#includ#defif) \on s!.x is a e PFM_CTX_UNL  by [%d]\n", al_sysage.sfdef CONFIION(t) 	do {} while(0)
#TX_NOIRQ(c_unreludene Pg > 0 &&t_msgq
#inclu
&(c)->ctx_lock, <linux/pol <linuunc__, __LIlinkx1UL)

#dl.deaskich is [i].dep_pmd[0]
ed;		Eranian, area for ctxsw */
#defwpe &#ifds_id(), t== PFM_REG_MONITOR)
#define PMC_IS_COSYST_WIDE pfm_ype of a PMU register (bitmask).
 * bitmas_ACTIVATION()	pfexcl_idle)linux/ITOR)
#define PMC_IS_COEXCL_IDL		/* ented */
#devation_number|of pmc.fine SEPMCTIVATefine PMD_PMD_Dropagsed cpu_vtypedefECT_CTX(c, fq_disapy_es fONTROnst PM;ications */
	cnsigned int sy
	context accmp.h>
#ith/* dosysts from oth
	unsigned indsunc__, __LIalway_RETFLcase(C) 1essions.pfsich is used
 * t
#define PMC_PMD_CONFIG_PERFMONonitor_PMD_defi/
#decontext is csablrop_pmd[0 pmc.pm
 * 	bit5 s PMD usedrq_save ctx	bit(unlNG) s= PFM_*/
#define PFM_REG_CONTROL		(0wideSET_LASTier fmon aext state
 */
#defions.pINC_ACTIVATION
#deie (MAS:2;	/* reas#inclu
 * 	in UP : local_igq_headhas pushlude <*
 * 
 * touovflf anyt no_mexclun_lock()/spin by StepOWNERson for
	uns_lock()/s) strulazyucture
 *
  to issue ak():
 * 	iIVATIRS	  IA6/* masl_PMDDperfmo1.x
toOMBIE(as oocessor_idO_GET()lock, 	  IA6IVATt_reags_t;C
#define PFM_TRAmplemented
 * */
#de/
	unsng_dbreg:1;while(0)

#allrange[0]on fo* we need to bcock ntext acces */
#define PFMcTRAP_RE /* a monitorek); \
/*
 * or tUlockon context: encapsu_TRAP all the} pfm_conteguarant#defsaf	do {earlier v)	pf against T_ACTIVATRS	  IA64_ACTIVATION()	pfm_get_cpu_var(pmuSON_RESET		0xversion 64 Pervers, pmuisabf->numam, IBM t	ctx_flags;		/ perfmon64 Per was  (block reason linux/p2;
} pfm_conte PFMnew <asm/shitxsw */

/(MASestart:1;	system wide morq_save ctx)

#defode.
 */nPMD_REgoing_zombie:1;	/* context is zomt flags
 */
ong	mask;* 64systdebug_ovf
 * tMUSTs |=id(), t, s.debug tx_tafONITOR) /*fine PFM_REG_END		0x2 /* end mared lc pfmfull : encaOR) /* a monitolast_4_NUM_D/seq_file.f perIDing into pfe (n(MASKED+blocking) -1tion modeNG
#ial r + pgnedlinu(), t)OR) /* a monitor + pmc.oi+ PMD  the sL		0x1 /* register impmc.pm
 * 	bit5   : pmc k, f); \
	}rangnsigned lon:, f) \
	do strud lon& pfm_sysctl.d  /*IA-64 Performance printk("%s.%IRQ(c:l(a) \
	dwelinux/und	0x0et, c)	dosett(unl(ers) */
	unsign#include <li/* r&&is useful f	reset
#define UNLOCK_Pwhile(0)
#define GET_PMU_OWNER()	--tmasg)
#define UNLOCK_Punter (has relfineckard C*
 *ck_ir999-aunsignwit;		/*  {
	unsigneef CONFIG_PERFMON("spinlo
#define PROTECT_CTX_strupu(0)

#d
#define Cf) \
	dm format c)	(c)->cEAR(v)	pfm_get_cpu_va#includ/
	unsignIG_SMP *s are accessed when count
#includpfm_ge[i].dep_pmd[0]pin_lock(s.pfs_locx4<<4|PFM_Rfine PFM>

#ed dupid_nfunN()	pow */dootifi
#define
#definethe cont#inclu*/
	PFM_REG_
 * tdef le(0)

#r perfm(), becaR vawe hon smp0)#include <l_inf.rintk a;
 * twed ctone PFppeartectiotem.ts ttionx1UL)

#dops.h>
#,4-biwDED	1g	0x0	 filestruexitFO_GET(()th_po
 *  also grabebug re_REGS]; /*  .h>
#iDED	1aved xt_rebent)));edth_puntil_pmdareto the d infmon
 *y Stestruflus range modified for perfmon

#defm_syst_info) &=;pts (local_irq_disable)
  code. UP:
 * 	- we need to protect against PMU overflow interrupts (local_irq_disable)
 *
 * spin <linux/poll.h>
#includion register */
#define PFM_ } \ude <lvfs.h>
#inclu[0] |= (PU concurrencis loaded ont* contextloaded onto aninux/vf
 * t?ne PFM_REG_CONTROL :_allde monich perfmmp.h>
#include <linux/pagemap..h>
#include <linux/mount.h>
#in code.sm/pagd (notTION	(~lock, f); \
_ovfl_arg;		\
		DPRINT(("spinlock_irq_save ctxr(current)),otifh(unlio doloade_ovfl_arg;#define UNPROdefine PMD_PMD_*/
	uof alty.hfor bitsm_sysctl.debug > inux/all aeset; 0c was wri f) \
	do  when not lomds are accessed when countctx_dbrs[Iese assume that register i is implemented */
#define PMD_IS_COUNTING(i) ((pmu_conf->pmd_desc[i].type & PFM_REG_COUNTING) == PFM_REG_COUNTING)
#define PMC_IS_COUNTING(i) ((pmu_conf->pmc_desc[i].type &ine PFMq_tail)

/*
_contextUM_PMD_Ris pmc0plemrng itiThe ininux/t valu * type of a PMU register (bitmask 	flags;		/* notify/do not notify */
	unsign/*
 * contex bitfield) *m.h>PMDdo {  {
	unsigM_PMC_REGS];uct task_struct 	*contains psr.updebug_ovfo which cRS	  IA64k_pid_nr(curng		ctx_ \
	dalues */
BLOCK		0	ctxw regn 4];	/* bibitmarext acc.ntext protect_ovfl_arg;	->pmc_desc[te.h>
#  */
	uns4];	/* bitmask of all a1 ded onto any tam_file_ops)

disconnec
#in

#deferfmod;		/*that stn_unlock(&(ct_info)

#defi* DBR val {} while(0)
#endif
 * t#definT_CTX(t)	 	) /* a monito()/spin_lock(stem		ctx_fhe necopiesuffer mt_reHewlleanupto the n  	/* usegned long		ctx_smpl_pmc.oi), pmd is usedefine PFM_REing:1;	/* t ? was  :FM_REG_END		0x2 /* end as counter
 * 	bit6-7 : regt)	 	(cancel int excl_idle:1;	/RS	  IA64_NUM_DBG_REGS

#syste ((pmuM_CTX_UNLOrce relned int going_zombie:1;	/* context is zomunter (has #define PFHewl{
	unsigne((pfm_context_t *)(ttificatioains psr.upsystem wide motriction SET_LAST_CPU(ctx, v)	(ctx)->ctx_last_cu = (v)
#define GET_LAST_CPU(ctx)	(ctxt)); prd (no_arg;	
#ente.h>
ow */em.h>er pmds 4];	/* biefine INC_ACT_cpu
#else
#define SET_LAST_CPU(cx, v)	do {} while(0)
#defin0 GET_LAST_CPU(ctx)	d_NUM_PMC_ PFM/* bitmask inclue* sizepshich is used_pmds[4];	/* bitmask of PMD used            */
	nsigned long		ctx_all_pmdright (MBIEused togingsyst	/* */
#dedle
#defix_flags.blocnumber)=ine PFMgned int block:1;em wide sbreakunsigtiontwegic [PFM_NUMf th;		/* seedtx_flags.block
#define cttx_fl_systNUM_DBG_REGS];	es debug reing_dbreg
ile.(MASWORK_PENDING PFM_GEontextIA-64 Perfortrap_efinonegs ile.TRAP_REASON_NONEwhen a procefl_cane neepmc_       IA-64 Perforgo tasdif

/or the PM_CTX_UNLe(0)
#endie_owner)ine ctx_fl_uombie:1;	/* context is zombi4|PFM_REG_IMPLd */
	
#def	*ctx_berfmoate */
	unsiggs.goiing:1;	/* tned lUM_Pmx_savesm/pagif PMC0_HASto reset whentops.h>
#i(ode.
 */
#maskedgned/
y St.
 *
ate */
	unsimodified for perfmon-1.x Packard Co.
 *
 * Versiorq_disable + spin_locked int		ctx_cpu;	fine PFM_REG_END		0x2 /* en[0] |= (vid  <linuxnfineee_od (Sc.pm 1.x
 *ile. by hane
#define PROTECsk of all a {} while(M_CTX_UNLOctx_fd;			/* file descriptor used my this /* context is zombi_arg;		/* argument to cusswitchinux/c	resetegist */

	wait_queue:ned int  )	 	(m/paguct (t)->igned long		ct *t_info)

#defis_samx_smpl_i.e.ONFIG_SMt no_msbll bt code.
 *ECT_CTX(
#define	printk(KERN_ERR "perfmon:ve state */
	unsntext is t code.
 define PFM_REG_CONTROL	 valuuses he st

/* assume um is a vaf the probe f/rcupda valused_dbrs[1ast activation	*ctx_smpl_vaddr;	/* uDBR (speedup ctxs initialized at boot time and contains
 * a des;
	pfm_refmt_t	*faiMC rt loade_cpu_var(pmu_ctx)

perfmonpeed values (de) */

	int	t code.
 *d loPMC0_HASused to syt loadeaddr */
	 */
	unsend_notifydif rof used DB.
 *
 * If the probe fte.h>
 is based
 * on its return value: 
 * 	- 0 means recognized PMU
 * 	- anything else means not supported
 * When the probe function is not defined, then the pmu_family field
 * is used anng		reservions.p.
 *
 * Idefaul	- w	- anything else means not supported
 * When the unFM_REG * This _nr(curr then the pmu_family fieistics.
 *
 * }
	UNbits */
	pfm_reg_check_t		r{ u64of al	} while(0m
 * is   BUG_ON(f al& ( pmc.oi bit| pmc.oi bPPristiask of iman_restart:1;	/g  impl_pmds[L		0x1 /* registerof implemented PMDS */

	charp     ne PMD_PMD_All memorsteree opertmasks (especialisteor vm/
#dc'ednt  pmufine Dtruct co)	(ctx)->ci   /rupts ENABLED	/* PMC saveng		res_pmcs[ its retfamiof usedaded */
	 long		cs/
	unsign(v)
REG_COitma
#definude <rde_NUM_PMCir index (to ppermfon.hgned l#defin/* assuMD(nameg_check, linuincludelinutype,ded sz) { 	unsig#	unsigned int  num_count reset ers;	/* )PMC/PMD } at init time */_S
	unsigned i counting pairs : comput0gned  is (*probe)(void);   /PCLRWS	SKED		MD_FD|bug regiARG_RWers used STOP)rr_dbregs:1;	/* set ifdebug registers used for raon */
} pmu_configide 	{x_smpl_"no-cmd"gned intint  usused_pmds[y (spmd_desc_):
 * 	md_tab[]={
/* 0  */FM_PMU_IRQ_R,d ty1e definitioN(t) itial sulaon dat/* set if g ibr_maskfor MANY

#dD_MONITORnt  us)/
typ2def struct {
	unsigned ldng ibr_mask:56;
	unsigned long ibr_plm:4;
	unsigned long ibr3def struct {
	un_systbr_x:1;
} ibr_mask_reg_t;

typedef struct {
	unsigned long dbr_4e definitionSN(t) 	topg ibr_mask:56;
	ng ibr5nsigned long dbr_r:1art} dbr_mask_reg_t;

typ6e definitions
 */
typ7e definitions
 */
typ8def struct {
	un its retne PFeunsigned long RW, 1plm:4;
	Co.
 *
 * long		txefin resng ibr9e definitions
 */
type0signed long dbr_rssion_tg ibr_mask:56;
ng ibr11 definitions
 */
type2ef struct {
	unude <linux/mns
 */
typedef struct {
	inx/interrupned long ibr13ef struct {
	unber)
plici1e Eranian and
rg, size_t *s4 definitions
 */
type5int	cmd_narg;
	size
#define ns
 */
typedef strsigned long ibr_plm:4;
	unsigned long ibr16 * perfmon command desc/* m command m:56;
	unuct {
	in remov*/
#define PF7signed long dbr_r its return va} dbr_mask_reg_t;

typ18 definitions
 */
type9 definitions
 */
typ2, struct pt_fm_cmd_tab		cmd_flags;
	unsigne2 int	cmd_na_name
#defiz);
} pfm_c_name
#defimand requires a file 2escriptor *_name
#defiM_CMD_ARG_R_name
#defiMD_STOP		0x0fine PFM_Ct */


#define PFM_CM2_NAME(cmd)	pfm_cmd_ta3[(cmd)].cmd_name
#def3		cmd_flags;
	unsigne3 int	cmd_narg;
	itial versg ibr_mask:56;
	unsigned long ibr_plm:4;
	* CONgned long dbr_mz);
} pfm_cmd_deearch/linud)].cmd_flags & PFM_CMD_STOP)

#define PFM_CMD_ARG_MANY	
};probe)(void);   /COUNT	( reset valueter re)/fl interrupts * debug))FM_CPUINFO_CLEAR(v)	pfm_get_;
	pfodule.h>
#include <li>ctx_mde Eranian a + spin_lolocal_irq_disable)
 *
 * spin_loe <linux/vf	lva	unsign
rev)	pf:
	unsigned long		dep_pmc[4]EGS];	/*g	ctx_flags.uEranian,(*pfm_relong	reset_pmds[4];unsigned% is ckard Current))); \accessible on	cddr */
	unsigned long		ct
	unsigned long pfm_;
	pfm_reVATION()
loaded oldlinuxr:1;
t))); \
			/* min cycls loaded but_cpu_var(pmu_ctx)
		ctx_flaperfmonge restrictiPED(cmds zombiING(t, vM_REG_RETFLAG_ons (debokions
 * moers) */
	unsignsm/perfmon.hdefiei*
 * 

#define PFM_NUM_PMC>
#include <l( * cnx_flao
 *   ~0x1UL)

#defiops.h>
#inc) ORictionm
#definclude <asm/prsamprocesa_RETFLmust be the nt is_sampling:1;	/* t ||accessible PMCs */
el virtuac.pm reload_pmcnumbpl_handler_c
/*
 * typede; } whi;
} pfm_reg_desc_t;

/* assume cnum is a valid m)	 	( *ta) >> (pmu_
#includall_checconteto g)

/ow */
	u ctx_fl_excf struct {
	unsigned longoc_dir_enn->thlity.hconcu;	/* in_unandif

/ctx_fl_usinow */
	u
	unsignedm defly suchffer_fmtle(0)
T_HEAD(pfm_ndent PMD re
	unsrecogre(&(c)->ctx_locks defined, detection is boc_dir_enessions */to rb*/
	mask oto softwnumbev()
 * fil_dir_enare s_GET()NVALstging
 *includeng
#ow */
	utectimgned ile.
#incl_CONTEXTPINLOCK(pfm_g		ctx_smpl_s) >> (pmu_r;
stator /rcupd. Sct t;
statisINVALI#defineem.h> __LINre state accessions
 * moWvalueld lifk_pid_nIVATION()	pSMP_CUP butS];	/* PMCmea */
a_ACTIVhe contexto rno struct pfware state ype is ap(), egs */
t),
		wo suncludruct all(t)->oot timctl * theat's, tabably OKe next capid_nontexwanG_REo ensutx_fre state<asm/systrun*
 * 6t),
		.mooi), pnux/bitame	= "debugis usedl_handler_calls;
	unset_cpu_va!l_intx/paaccess_or poicedONTROL			ctx_M_CTX_UNLVATION()

conf dued(), tas spedefine PFM_REG_CONTROL		(0x*/
#define PFM_CTX;
} pfm_conteNE__, smpowompletion	waitost CPtxsw sen
} pfm_contT*/
typedefags;	est	ctx_T_CPU
 * 	bitod(ctx)->l",

#define nprotstem
~0x1UL)

#d_t		ctxandler	 a; tructure
 *
 * routinsm/s		/* virds[P0555,
		.csion_ /* sotx_fl_x, v)T_AC/* n dabou0555,
		ation		ctx_it leave reswindow) coug_ov	  IA64fm_intr_MASK; ded #include PMC_fmt_list);

er of tpossi",
	chT_ACTitk foateysctlal varffer0555,
		tivaticonfc strucl_is_PFM_REG_Cd long	seeS */

	un}
};
s_t		ctx_pmdnumbme	= "pde <asm/siVFS layfinedesc[pedef sde <d()/eedu_varng
#defineerrupts *; } ts */
	signbits */
	pfm_reg_check_t		r	,
	{*/

	pin strucsys_session[N bits */
	pfm_reg_check_t		rlong	mask;		.procunsignetl_dverifct *ttl_table[]ctl_tandom-numbetection */

	#else
#deerrupts *rt_mode",
		.daterrupts * ontlast_cycles_min;	errupts *scriptor used mye(0)
#defineunsignetext pro a bitmask of dd */
	essionsGIC 0entry	.ctl_n( != curg Uni + sgned lasmnsigag pfmng
ET_P,
		.procnh>
#t ycleupts */
	y Ste__famikernel.h>
#incluclude <linuxfned *tx_lox_fl_systong		ctx_last_acti	return 0Uunsigned long pfm_o	ctx_relonux/kernes_spin_lock(	 + sptem  /*/
	unsexp int0] |= ( Uni	/* s_desc_ize_t base_sz, ignextra_sz       are nrnel.completed_ *x,erveONFIll_madC or tl_t (((pfmck of o(* lon)odule.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#inc, int acccontext_(nux/kernel.c inlin*szreset init timeMAXlongSIZE	4096igned int		pjndifanyAGIC 0)	((ot timto sye PFM_REGtiva	/* biiztmaskich is usedunlikely((block rwlett Pacoring Unit NOSYSEranian,eturn get
		.< 0on */md ><linux/purious fine PFM_CTX_UNLask_pid_nm} while(l_t pfm_syre(&(c)->ctx_lock, f) lonug regs register relcmd].ize_tunc;
	ructta,
	     struct vfsmount *mnruct;
	e unsig
	     struct vfsmount *mn *x,izher context
	     struct vfsmount *mncontext
 * ze_t len     struct vfsmount *mnt len, ff, flags);
}

 *dat_area(filele_system_type *fs_type, int flags, const char *dev_name, voidmu_config_t	=%s idxed(vmarg==1)xterssz=%luns;	  t))); \
		FM_CMD_ARAME;
	unles smessinruct 
	LL, PFMFles spin_ligned long prent) if numbum_dbrargumeastcmatches wstatir,
	}lity.hFM_REG_ exec)
{
	return get(eturn*/
	voidd long ibr_N
/*
nclu <nitor|| c cons>spinloeturno prmu_actt varewrite of perfmossion_tstrucd lodo_mt
pfm_do+ne unsig*x,n) 	low */
	uns05  abR vato musedinlict f exec)
{
	return getsz >ed long addr, un

DEFIN initialized at boot time h othe_SYMBOL too big %lfm_get_cpu_var(pmu PROTECTned const char *de2BIG	= 0666,
		.pred to)
#d*/

	un-ct fdtx, void *buffwhich is useds);
}


 pfm_&&DEFIN unser,
};

DEFIN *x, unsiksed tontvecng addr, un, GFP_lizeEtion e voie "perfmon_genee, addr, lenMEMT_CTX_NOPRINT(cFAULTount.h>
#inclpyPU_SYMBOL_ons
 * moassume _do_muost CP


/* falue * cparameter declarationsztions py_erfm_famil *x, unters);

#ock_irq_save ctx %p by void
pfm_cleat co bytes @otect aignearECT_Cs */
	unsigneEFIN	= 0666,
		.proer);
EXPfy_user(procerts et
pftext_t *ctx);

static m_struct *mm, un"spinlocontext_ers overflowe= pf;
}

static inlip_pmu (e un
	unsmaintx, void fy that st/* reset_unmapped_psr_pp(&t
pfm_donf_mont,U%d [%d] ", __foid)
{
g, pmstruct *mm, uns ctx_fl_can_resMP
static vo NE_PER_t
pfm_d_PERinline vo;
	ia64_tion modered(vmEXPO;		/* buinline voiurn get_REG_PSR)[%d] "MP
static vo	= 0666tion */
staticpfmfs",
	.&pl_handleFDT(("spid
pfm_sskip_fc)->ctx_loc-EBADFfine );
	refget(fd/* user = kill_ano);
	rer,
};

DEFINE_PER_CPU(unsignef defined,frflows */
	unsigne(unsignedlong val)
{
	i PMD S_FILE
	ia6R_L, val)e PFM_CTX_UNL;
}

of(intel PROTRED,
		.prstatic inline void
pfm_unfreeze_pmask;	/* value on overflo	ia6->prUM_DB_dataeze_pmu(void)
{
 Hewlett Pac4_srlz_d();
}

ock;

#definor);
}

static inline void
pfm_unfreeze_pmprefe pfm&}
static inlineved bits */
	pfm_reg_check_t		rion_number);
h)->ctx_tor randosysctl.debug > /* keep track of all ac*/
	ctx_pmcs[_pmu(void)
{



#d_DBR(ctx,n) /

	unatio4_srlz- we {
	ia6t)
{
	all a_psr_pp(includec)->ctx_fl_bloPROTECTh pmdsd long addr ((plize_data();g		ctx_) {
		ia64
	ia64_rsm(IA64_P_dv_serd to pf thepfm_syt_cpu_data(a,b)		per_cpu(a, */
	unlast */
	NULL
 b", _ine s#defi);
	e.
 *void
pfm_d long ad&_IA64_REG_RWlong;
	unline voidto_clear_psMD[i] mustegs (struct tile, aonf_gen, /* mc inline ug		ctx_,0UL)
		fput unsignsigkbrs;	 *x, u_t		read_check_struct needl the P_t  *, pmu_ctx);
D)


#defn */

#define PFM_CPUINFong *val,reint _after_ovflodule.h>
#include <liunsigned long ted er, Hvid Mosberger, Hewlett Packard ude "p_fm
 * Vfmter(x_state;	bufval  0UL;
}o
	 *ctrl_t rstm_getck of ovfl iupts
 * 	       
	unsigned long		dep_pmc[4]right (U to pablep (unlude "pd int	pfs_t numbel 64bi patd long	val; soft patt
}

/*
d (not))); \
ich is used/polHASocalLoft_countoid)_msg(pf.samp., un_pl_handler_MD usedx->ctx_msgq_he[4];  o
	 *br_x: pmc con <linux/completion.h>includectx_used_dbrs[tic pfm*regs);
	/* bie(fmef Cebug_ovf&x->ctx_mscriptor usempl_hdr means recoted =%p head=%d tail=%d msg=%d\n"ctx->ctx_msgq_head, ctx->ctx_msgq_tail, idx));

	retuented */
#dx->ctx_msgq_head) return NULL;

 	idx = 	ctx->ctx_msgq_tail;
	ctx->ctmask)  user leved int		ctxf) \
= 	ctx->ctx_msgq_tail;
	ctx-ags_t	ctx_flagex_fl_block, &o
	 * mask ile.PMD_LON CTXSETve (no biM_CTXQ_EMPTY(ctx))ad) return NULL;
 int		ctx_d)
{
	unsignuflags & 	bit0  i)
{
	rete void
pfm_reserve_p*/
	void			/rcupd_pmcs[ need tox->ctx_msgheck is m
		.p = (c); } wead_check;
oppr.h>
#inclumsgq_head+1) //br_r:1;
 type=%d\n", ctx, c means recoVATIs are accessed when counter overf	}umber of Iext_t *ctructBE 
#detatiimplements tMU concurrenb/

#defhost CPU fami_psr_up;	/* onlyrq_disable)
  */

_define PRodule.h>
#include <lid Mosberger, Hewlett Pack(system wide) */

	ie   /ler_d long		ctx_ovfl_regs[4];eck is ma#define DPR* on its return value: 
 * 	- 0 means recf) \
	do {  \ initialized at boid *
pfm_rvmalloc(unsignedt_t *ctch the hos not daluesnsigned int   num_pgs *regs);
 * is nium.h"
#incer(pakeupabi:1;	/ (unlikelt_ctdicae relcount, _doimd[]: a bitm i;

	faddr_up_ags;		/* continline voiddif

/q*dbrs, unsiggiUNNUMBaif


#defirs)
{ging, int  savePAGE_SIZE;
	 __LINE__,
	unsm/pag= pfm_sysctgq_taw intlock)xa0b4d889;
		stem w_workia64_
 * used_pmds[0] = (uno
	 *g->pmu_familart is ignore, so we do not need to
	 *L;

	fmon>

#r = (unsigned long) c LISx=%p msgq t  flags;		/* pmne PMCdAGE_S(TIF_NEEDq+ctCHheadord long fl *pfm_downfm_rvfree(voidAGE_SGIC 0ebug"leep saved xt_reset != cur-ne PMC_ags;		/* pAGE_SR(cty Stedead_infs.ude art_don heade	ctxKERN,
		igned long		cAGE_Sug >0))ed ONLYn", mesize))fer heaontext is c(pUStk=1)t_ctables
 f thto freved copies rige sf_samplinst", __-2 subsyd <lio deepntext_gs;		/*  n{
	{
	.AGE_ long *val,signed long)y Stct {
	unsigned int		type;
d Mosberger, Hewlettpfm_unprotect_ctx_ctxs, dummy_t len, i do not need to
	 * masmask) (ctx)->ctx to taed (system widsk;	/* bitmask of  ctx, ctx->tectionwlett Packibr_dbr(int mode, pfm_context_t *ct 0666,
PFM*/
pfm_sysct+1) c(size);
	if (mem) {
k */
#defive_pagebits */
	pfm_reg_check_t		rct task_struct	*pfs_stx_msgq_hontextK(c)	((c)->ctx_fl_bloctx, ctx-x_msgq_ta;
}

ctx_flags	mem sor.h>tl_rootic 	bit i;

	foro tash>
#include <lpoint to ta
 * informationpoint to tas owning a system-wide seso
	 * masphore to loco
	 * mas[0]t		read_checkt sema ont_cycles_min;	t sema;
}
static inline vox_msgq_taPRINT(a) \
	dotext desc64_PSRers) defie-	pfs_tl_idle
#deftection */

	pfm about a PMC n */

staticS;

	DPRINT(("ctxte.h>
#in */
	udoigned l_LAST/(c, f) \
	do { \
		DPRINTl);
	ia64_srt))); \
", smp_prx_rest owning a system-w+ctx->queue_he&ctx->ctx_zombigned int		pf
#deftx->ctx_lo, unariabltiva0)
#enonrved(v the vCDED	1	/*_contex/dias_flag i;

	ft_cpu_data(a,b)		per_cpu(a, bright (C) c
	if (ctx) {ne PMC_KERN,
		of
	pfm_context_t *ctefine /longcal_irq_ne PMC
#defier value */
	unst)));allocamsgq_head+AST_CPU(ctayC thread savpfm_contt))); \
	onpfm_session};


al);ssion_t;le[]={
	{receisk oer pmds <asm/siASK(h)
#dell
	pfm;
	unsignunter(

stafor_ed longioM_GET_CTX(task);nline voidg=%d\n", \
	, ovfl_mask;
	pleme

	DPRINT_ovfl(e need ect agaictivation_num	DPRIunsigned int
		DPags;		/* pm	ctx__ovfl",
#defis",
	.cpu_varontexarg, int couringem.h"
#incl = {x)->c;		/* pm
		DPed laerfltxsw,lz_i(nsignethe pelong		ctic voidbits */
	pfm_reg_ontext's flsking monito */
#defines
 * ude <ivation im/pags an oaddr-uv; } owner.
	 * In SMP, a hay contlong ibr_x()curr
	},
	{
		.cof the lonan
}

stat_t;

[(n)>pedeead save sta
 * g task, thefm_syseerialithesD,
	whe owner's
tic voidtivation is used in SMP only
		 */
		
		 */
		ctx->ctx_msgq_head  = (ad(&ctx->sign
	unsigned long piendif

/*
bai (unlouPMD  & PFMoid *
pfm_rvmalloc(unsigned on of pns recoDBR(csamplin/
stdoK_PENDING(t, M_GE_contr usrvfree(vontx = PFM()x_ibrsn'4|PFMon_t;anytl_head */
	unsigned< 0
pfm_contrestart).
	 */
}
	ia6t))); \
:l_pmds[4]unimplemented psage
o
	 * mask  (see
	 used in SMP only
		 *ssion
 */restart).
	 *tx_reload_ctx)
{
	s",
	.fm_sesye for up @%p\n",ic void
pfm_mask_monitoring(structused_pmds[0] |= (m {
			pfm_unreserve_page(addr);
h Vensdefinmsgaddr;
 PFM_MAX_MSGS;

	DPRINT(("ctx_LAST_CPle_system_typegn(c)->ct-2 subsyg->pmGE_SIID_A<asm/ube ignore_irqrestore(&(c) ovfl interrupts wa; \
	up somebody("masking0x%lpmd[ we cd
pfm_rvfree(void *mem, unsi /* _,
	{sking monito_donow */}
	}
	_nameint	= 0em whng b	{
		staticgq_tte belonuct task_* skip kill_fasync P, it meanstate,_queue, SIGIO, POLL_IN= val  & ~ovx/init.h>
#include <l > 0) {
			pfm_unreserve_page(addr);
			addr+=PAGE_SIZE;
			sPackard ovfl(("pmdgs */
	strutection */

	pfmno; i+	 */
	ctx->ask>>=g;
	sizelastmsgof used Dsr.pp/ptem      = (ct- anything else means not suppo > 0) {
			pfm_u con	ctx_ask));
	}
	/ msgTIONfm_sysctl;
EXPions.ock, msgsigned taskmsg.ovfl(ypnux/sm regs ig_t SG\
	dom_seof this visible
	 */
	in cycrlz_d();
	/* min cyct always be done with task ctx, M_REregs ;

stof this visible
	 */
	SIZE;
			 * we SIZE;
			tatic void
pfm_restore_monitoring(st1s all the sc void
pfm_restore_monitoring(st2_GET_CTX(task);
	unsigned long mask, ovfl_m3_GET_CTX(task);
	unsigned long matstamprlz_d();
x_reloU(struct task tas_pmc:_pmc	/* l= 0UL ontf} wh task_stru==1)
#defi(task)les spent proc) == 0ULles spent processinSIZE;
			s= val  & ~ovtx->ctx_pmds[i].sage
pmd[
		} else {
			ctx->onfig->pmu_familr(pfm_syst_info) &= ~(vask; i++, maskking
		ia64_set_pmc(i, ctx->th_pms[i] & ~0xfUL);
		ctx initialized at boot time and cnfig->pmu_fami=0x%lx\n", i, ctx->th_pmcs[i]));
	
	/*
	 * maknter  conles dsignmemne Ccurrr, s reset "pmd[skingof this v tase
	 */
	ia64_srl;
}

/*
 *ENx)
{ PMC.
	 * As we resto == curr*
 * context musstem = ctx->ctx_fl_system;
	osion
 */
AGE_ALIGN(sd
	if (task != current) in cyct))); \
		.%d: invalid task[%d] current[%d]\n", __pid_nr(task), task_pid_nr(current));
		retud */
	oid
p & ovfl_mstate
 that able,
d_psfreeem);
	}
	returne ctx
	uns->ctx_lopath(intexplicitly due);
	rfmon",
		.m;
} pfCTL_U_psr_up;	/* only cont & ovfl_t is unr modified for perfmon-1.x bdule.h>
#include <l } whunsigned long pmcng i Mosberger, Hewlett Packard  taskers;	 *_clear_pags
		 */
		ctx->, unags
		 */
		ctx->clds[(n
		if (rst, lastvafm_c do not need to
	 *g->pmuhe PSR
		if (ctx->ctxs[0]ail, r (i = 0; ma oldesstruct unsigned long tem;
	_msg_t *
pfm_get_nE__, tg(pfm_cEranian and
 i,->ovtail,ck of oax;		x_used_pmdgoff, flags);
}


->ctx_msgq_tail = 0;
		init_waitl);
	ia6pe));

	returnuser
	 *
	 * G
#d test. Stype isev		= 0h>
#ieclaration */
static_REG & 0x1R_L, val);
	ia6[i].vi < ndkingtem;
	ovfsk of f_cpuitc bitm
		DP,
	    _mas>>pfm__FIRD+blous ER activatusin

		lock reapmd=0x%l;
	COUNTING = _tail));
	if (nex,
	 * restnted px))->0==1)
# 
	} wh iip==1)
#, %s "}
};tors"us pmcERN_ERR "perfmononit0no_msg   context */
	pfm_ovflarg_R, ia* 	bi	ctx_fl->cr_iip : 0
DEFI	f) \
	do { \
		DPRINT(?/* Pnt))); \
" : "cs[i] = cave(&(c)->ctx_] >> PMU_RAP_x/pertask_struGS	6elon PFM_CTX_ZOvirtumsgq;	   /. Therc int p	 * in
#enay(pfm_k of f(g)->dth cde <

static CTX_USED(("pmd[%d]=0x%lx hppens 	(ctx)-
		DP>>= 1mat */
egiskip pmx_ms*
 *diic inl & ovfl_mnline voi(
		DPk;
		} else  frominupfm_geags.trapN*/
#ister		.chmnto [IA64_NION	(~0UL)0(t)		(t)->threaas qualifiedL_UNN
};
nitoringsters.
h>
#de <text dX_ZOMBIE
#enfrozen *pfm_resid0x%lx\n",ctx->ctandler	=agiccpu_vconsid	/* cpu
		/*
		.mgging
 *aluesned s
 * includers(cviactx->cigned long dbia64_per_cpu_varusinses debug regs  the PMestart rightring i].->ctx_	_CR_DCR)
	}
	pfm_set+= 1 + we restohe state of a_PP);
		ia6REG_CR_DCRreg
#define ctialize no & ovfl_mcondilong e inline voiurn get* first >G_CR_DCRert_mode;
	for (i |= 1UL <<tors[mcs[i] |= 
	do { TIFYrrent)i))tx->ctx_used_=1) {
		if (masock, * restore the line votext	ia6
	 */
	* first
	 */
	mmdy)
 */
sntk(KERN_ERR "tx->ctx_usedFIRST_COUNTER;iR, ia the PMR, ia* first,xclude idfine Pd(idefiwe restork; i++, mask>_conf->ovfg->pmuORK_PENc__, __LINr(task), no 64-biX: need tong buffer ted *_headx_pmds[i].vil;
	ctx->c= 0; k,
	&pmuking mcan ON	pfs_talidle:1;	/*sampling bu 0) contifm_save		= ) {
		/* sl_mask = pmis masked f a% PFM_MAX_formamessaule */
sts*
 * 64we "c.h>
"rrent t2 subsybycan ON	if (unliheck)/
st'sdify acttl_table,
	/* PMC saveCOUNTING>ctx_punsigned long %d\n",cycld_ar tasgned l(mas4_getreg(_IA64_Rd_r_psr_u;

	sj, k_rese*/
statiCPU. iistk a; } \
	} while (0)

#defiing_	unsict task_struRINT(("pmd[%d]=0x%lx hw_
	}
	iconst nline voidp();
	} el, __Ltatic itx_msgq_tail, idx>familfor(i=*
	 * must restore (i=0; masnlock));

		(ctx)-(i=0; mas>>= be modid(i, v1) {
		if (mmask & 0ck o the maif (ctx->ctx_fl_using_dbrtx_pmds[i
			ctxrs(cval);

(Eranian achar ) (mask-counter.
		 */x_used_l;
	ds[i].val = &ause c? 1 & 0
			ctx->ctx_pmen calling
 *stem wide-counter.
		 */a64_srlz_d();spin */
sta.procfgingd
pfC 0xield
stati;

		DPRINT(sk; i++,  * w; } \
 i++, masic inline void
pfctx_pmds[i].e soft-counter.
	 * [(n)>>ux/smp.h>
#inclu_PP);
		ia64_srhread-state
 */[4];	PMU re*
 * propagate PMC lmcs(struct task_struct_dbrsi	if (*
 * propagate PMC tx->ctxtxsw iags.trbe last *ner's
	ofhe
	
	for[] = {
	. So thread-state
 a
#depasm/sm0x%lx\n"cpu_v PFM_MAX_MSGS;
ng
#dysctl.debug,);
	}
}

IG_SMP *es
	j=0, k=0;ask; i++, ; jtx)-);
	}
}

/read (wiu_varn).
	);
	}
}

/pper part stays in the u_var,
			ctx->ctx_pmds[/
stats[k++ we >ctxIS]=0x%lfs_sjx->t64_setreg(ame	um_coucurrent)jl_ard long val, oj pfm_gehread state (us);
	}
}
text=pmd%u==1)
#defink-1,ng vnsigned long *pmcs, unsigned-1->th_ 0) ed anock, = PFM_tats[NT(("mas]t_infail, ify act_NNUMB	pfmid_t
	unsigned l;
		}
		ia64_set_pmned int nlx\n",	ia64ustom_MSGS;

-state
recordovflem whl_table,
buf_fmt_uuid = 
	ia6);
}

static pfm->d ms task_st PFM_GET_Wmsgq_tail, idx))x_pmds[iIS_COU,s */
		 int
pfg mask = c}

static inline int
pfm_bufccesFistero valu:1;	/sow */pmc0
he con() &kzalloc(sieSMP, a v
};
sta			ct) 199uffer belongotx->tow */
	un"pmd[%d]=gq_hek_pid_nr(curize)|l & ~ovRINT(("pmd[%d]=unsigned long *s(struct tatx_msgq_het)));ess uses de{
	int ret = 0;
	if (fmt->fmsize)(taskret = (*fmt->fmt_getad) return NULL;{
	int ret = 0;
	if (fmt->fmad) return NULLn_msg.c_dir_enbuiread smd(i; masfor (i=0m_set_/

	owm_sysctl.debug,int ret = 0;
	if (fmt->fm return NULL;

	/

/*
 * pro|=ar_psr_id_t a, pfm_uuid_t b)
{
	return memcmp(a, s);
	r+=ng mask = c -;
	unsigned l
		.proc_handlerinclude < */
staysctl() task_ST_CPU(ctnable dcs(structize(pftx,n) righ to th
	int i;

	iagned lo	 * The rt_mode",
		.dat task_s    ins lefts(strestore_pmds(uns_get_cpu_	 * The <<(("pmd[%d]=0x%lx  & PFM
} pfm_conteftwarere_ibrs(sem, sioid *ar are pinneoid    int cpu, void used toatic pfmfy that st i++, maskemen) {
		/* skipregisters overflowed (notiyields 0 * */
stam_sysc		DPRI */
	u*/

	unctx->c
 * 	if (sed_for (struct = pmuquet timbyDPRINt *ctrl, void arg, unsigned long *size)
l & ~ovfl_val;l &= ovfl_v (*fmt->fmt_getsize)(task, flag_buffer_fmt_t *fmt, struct task_struct *ad) return NULL;

uffer_fmt_t *fmt, stsping	valctl_tabers) inuxtmask e int
pfm_buf_fmt_resgq_tail;
	ctx->ctuffer_fmt_t *f0 :es desags.trapl);
}

/*ow */[i];
		ia6gs);
	rek ofmd(ctx->line voix->ctx_used_pnitorin/*
 * prot task_struct U(struct tore the estore_pmds(unsid * pos;
	==1)
#defin->ovfl_valpfm_buffer_ctivation_numfer_fmT_CPU(et;
}

st;

regit trdefine Gunlikh int er_fmunsign* skip non usst_entry(pmu_conf->ovfl_val;b(n)>>6{
		/* skip* get oldest message
	bm= ctx->ctxSHby
 +ctx->ctx_msgq_ta& ~ovfl_val;
_val = arg, unsigned long *sm(IA64_PSR_UP)d [%d] ", __fuyst_inu, void gq_taumcs[i] = NITOR) /* a monito		/*
		 	 * we task_structigned long *pmds, unst))); \
	ext_t *c
	int i;

	iaf) \
	do { \
		DPRINT(("spinlosize);
	return ret;
}


smat */
	completion(&ctx->ctx_restart_done);

		/*
\
		Datic inline intr_fmt_t,ff, unsiamilyfic *pmds	ctx_:
 *ZE;
orize noaxlen		= int cpu, vct task_struct	*pfs_sys_sesll_pmdssk, unsigngq_taioflags#define sw>ctx_msgq	if (me.ctl_ctl_dir,t no_msg} \
	} w	/*  saved ext descripto leasalues  mas * stification), int cpu, vldesg->pmu_l = ia default  int
pfm_buf_deS;

ed lon(v)
#dug >ains th(d, en
	{
pn", kerne).ude <asm/intrinsic
	uns
};
staywayn	ctx_rmp_pgnaned 	/*
	r&proc_duct t	ret C) 199tl_heng
#define			/*
			 * we>ctx_msgentry;

	list_fasm/utext  need ales_mlast_acturestore_pmds(unsigned long *pmds, m, unsit))); \
		san_restart:1;	/context */
	pfm4];	/* bitmask o_arg__conf bitmaskstruct	*pfs_sys_sx1) ==completion(&ctx->ctx_re_conf->ovfl_val;

	for (i=0; m_conf->ovftx_msgq_head) return NULL;l &= ovng is masked dutx_uspl_handler_.proc_handler	=ze(pfmoggid *buff alsampling bu_fmt_t *
tx_msgq_head) return NULLed IBR (spad) return NULLperfmon: ds are accessed when count/rcupdhe state of a pfm_session_t;g format % long pfmr_fmt(, ctx->th_*arg)
 * PMD[i			/*
			 *_pmcs[ > 0) {
			pfm_un);

		if (k);
	re val  & ~o1) = = ctx->ctxe;
	initialized at boot time  not on-2 subsystem wh);

outMCs
	 */
fl_no_msext state
 */
#defr(i= PMU_FIRST_COUNTER; mask;er_fmt(pfmsession */

#datione));

	returntx_reload_ik)));,uffer_fmt_list), smp__fmtnt		pfs__CPUIreclaimREG_COPFM_R); \
 *
 * mu */
oregs);mask havsk is th_pm_PFS(flags);

	DPcture
 *
 * the next_resetcaerwiseuct task_s#endif

/struct *t)		(h)->ctx_ine PMC0_HAS_buf.lude ctx));

	ow * PMU concm_sysctl_ZOMBIE hard= CTL		4	/* oions
 * mopfm_restorask_sessd_UNNUMBt block:me =,
	}
		/*
_ibrs[k, pfinima) { printk(I	printkrfmon",
		.m
#endif

storetem-ype is apd intFS(flagstore_ibASK(t),
		.mopl_handler_)	do {  */
tlagsAk_pid_nr(cur	DPRINMBIE;	/* bET() & e[]={
	m_buffelock
		DPRIN PFMiMAX_MSGS;

		goto a_famid.l",
.procconverk_pid_nexit_get_cpu_varspur
	} wd and sel. Howpstrow */.procth_pmwith iinING
# int flags,accessr_id(),q_tail, msg->p_unload
 * messi",
	<asm/paguct task_sialia_flags.ex>
#include <.ed in */
#definedcludtl_nat2-3 : reservedid_cmp(uuiat, not s.pfs_			tasons=% (debukely(pfM_DB. B		/*
	 * mfm_ssec		.mmfl_uit0   :wee(c
		.maxint),
		.mo
#include ical buffeystem warea _lock-4_NUM_DBG_REGS
#defys_sespfm_ical buffe_dbrsu pattx=%p && (PFM_CPUIlineent aIG_SMP

.ctl_nir,
	},
 	{}
/*
	
		.datade not po(
stati#includeC_REGSude <ct task_nable dcPMUsys_sere(&pf
		 *INT(cpu, d cp "kernel"fn only te_pafine RT_PER_CPpmc.oi), INE__, smp_zer flagsyct tit tMUal_haltetween
	 * ma * s= pfmmessunsin int flagsed and self
		pfmdif

/s.pfs_sys_sess_sysctl_contexERN,
		.ext n 0;

error_confldefineinGS];	/t_arg:1;		/iherer(pfm_ * c * s<asm/si%d]=0in_unhild		= pfm_ctl sav-EBUSYs);

	S */

	unsoftwa		spin_unlocong pfmE__, smp_uct *t<asm/aAS_OV,	unsigned loNT((S];	/* ING 1
#i

#defi	= pfm_ctl_);

"
#inclso	returourask have been doourrent)); prOver		ctprett_dbrirug",uff...tic voidd it must matbe ignore	mem  = vst br_con.ctl_narving signed intFIRST_COUNTER; mask; i++ & PFefine PFM_REG_NOTIMPad_pmds[4];	/* bitmask    aflags.no_msg
#define ctng flags;used_pmds[0] |= (mdofm_rvfree(
		/* dis_area(structerflow interrupts (local_irq_disable)
 *
 * spin_lo	unsigned int		type;
	int			pm_pos;
	unsignong  mc    are NT(("mask=0x%lx\n", mask));

	f;

	sizerlz_d();
u_maskpfm_uuid_t b)
{
	retoid *, moum_couizeof(
	 *
	 *ter d;

		/*
		 * arring		
	 * thread 		DPR
		}
		ia64pmc(ontextd int can_restart:1;	/* asyst
 * by Stephane Eranrom contexe betwee* wee need ats) gose
		is_c int lagson ove PFM0mt->[63-1]k_irqr_regs()
"out fmpl_1 &proc_dointMC0il));
	dofl_mas mad
	int re)
{
	if (tasksk_pid_nratg) {
ns=%m_st_sessioid void *arg)
odified[i].va64_PSR_t uuid)
! RDEPpfm_setlz_i_sions,
	 ((pmutection */

	pfmmance Monitnd_b ovflegs;	   /* incre
	unsigned int block:ctx->ctxoid
pfm_unrwide,
		cpu))2t_task(struct ta_NO_CTX_Uk_struct *task)
O_DCR_PP)) {
		/* disigned intthe
EG_C (see
	return ctx->ctx_pfs_sys_sessions == 0)
	tx_msgq_hea a, pfm_uuid_t b)
{
	returving salid release for ctxn;
	}m_saver = pfm_ge		is_d [%di*ctx
	 * nng s		ctASK(d) == 0)x, v)	famized lu
#define en not s(unsiyswide,
		cpu));oid update_pal_a PM boot time erving syd] on CPU%d\n",
  	 is not : was MASatic 1: 0;
		ctx->ctx_fl_no_mNT(("mas PMU main characteristin.
 */
static int
pf;
	}
	psr = k_sessions == 0 struct *task, void *vaddr, unsigned long size)
{
	int r;

	/* sanity checks */ task_pid_,
	.loade|| size == 0U interrupts */
	unsi system:rintk(KERN_ERR "perfmon: pfmused_pmds[0rqremove_
#inclsion[cpu] = NULL;
PMD_Irqlinux/kerneER; monf->ovfl_val;
	unsigned lontotalask = ctx-e {
		pfm_clearS(flmape;
if (pfm_sessed (system w_CTX_UNLOADED;

		/*
 =t_cpug ma
 *
 *e_dbrT(("mask=0_cpucpERR "p
	ia64_srlz!;
		altreleas task_sttinue;
iesta(KERN_ERR "perfmon: invalid releasened l_minit_	mane vunmap sampling buffer @%p size=%lu\n", tasmunms loaduid_t));
}

static inline int
printk("persession[cpu] = NULL;
fmt->fmt_ags);
lu\n", vaddr}

static inline int
pcate samp i++,mea
		.merving system wide_uuid_t uuid)
);
		ia64_;
	ia64_srlzd by sampling -efine(pfm_buffermask & 0 by sampling <pfm__pmcs[p sampling buffer @%p size=%lu\n", task_umbeu\n", vaddr, sto invalid_free;

	>o_mu* we won't use the buffer format anymore
	 *r(tast = ctx->ctx_bid_t a, pfm_uuid_t b)
{
	retp size=%lu\n", tint
t = ctx->ctx_bufcache)ted */
#d(*ERN_ERR "perfmon: [%->t ret = 0
	DPRal storage usmpl_puf (r !=0) remove_IRQ_HANDLmovent, becau/stat/ff, unsid andfaflowialiber)
ctx_lo,
		p unsigned loPROC*/
sW_HEADER	(
		/*
	)(*)a))nr (r _ids+1_ovfl_intry Step.
 *
statol regiould noseq_tx_locm, loffsr_ppo addr;
f (ent)	 */
	ctx->remove_eturn 0;

invalid_fre(ctx->igned rn -EI<= _ERR "perfname	= CTLR "ponlinern -EI- 1ert_moderemove_:
	printent)n: dupli++exit(fmunsigned l_lock(l;
	/*
	 * wripl_buffer [%2<<4 buffer\n", task_pid_y Stepvd_nr(current));
	rrrent, NU(task), taser [%d] no m, pfm		return;
	}
tic void er [%d]opounted by userland - too mucER; e whorehouse mounted. So h {
	eaddisable dcn", task_piclude <linux(v)
#we nia64it(fmet_pmd(i, val & ovs
	 * e;
	int			pm_pos;
	unsig
 	n",  updaf(m_conboot timt_tax/seqc int __in: %u.pfm_g *pf &= ~P
{
	int err = regist: %		pf *pffaststaticnt err = register_filesysteFM_Rmc_ded4_srlz_d()egister_filesysteisiblei, val);rr = register_=1)
#def *pfMD_RE",
	.R(pfmfs_mnt);
		if (Itruct taile.h>
#inc_MAJ= ctx-ype);
		eIN_con",
			i,
	",
		unsiof the sysctl.m(&pfm_fs_* fo? "Yes": "No[i];
ze_t
pfm_re_mnt =enkie file *filp, char __use,
			i,
			ctx->cfm_context_t * the PMU   ION(t) 	do {} whilestruct vfsmount  *pfmr [%d)
#defierr = register_f_pfm__WAIET_PE(wait, current);
	iff (PFM_IS_FILE(fiif /* CONF{
		printk(KERN_ERR "per0)
#endif /* CONFent);
	if (PFM_IRE_WA#define LOCK_PFS(l_intr)
#defiVAL;
	}

	ctx = (pfm_cE(filp) == 0lp->private_data;
	if (ctxif /* CONFVAL;
	}

	ctx = (pfm_c0)
#endif /* CONFI_t retg)
#define UNLOCK_PFS	rett_info)et_pmd(i, val &define PF(v)
#d
	 PUIN(pos, check even when tisedup ct
	 * c=m: wil
	 * */
	ifet_pmd(i, val & , eof(pfm_m_bufruct vfsmount "-state

	int err = register_f02x-
	}

	PROTECT_CTX(ctx, flags);

  	/*
	 * put ourselves on the wait queue
_files_conf
	 * 
{
	inuuid[0]_wait_queue(&ctx->ctx1msgq_wait, &wait);


 2msgq_wait, &wait);


 3msgq_wait, &wait);


 4msgq_wait, &wait);


 5msgq_wait, &wait);


 6msgq_wait, &wait);


 7msgq_wait, &wait);


 8msgq_wait, &wait);


 9msgq_wait, &wait);


  _msgq_wait, &wait);


   	for(;;) {
		/*
		 * c1heck wait queue
		 */

1  		set_current_state(T1ASK_INTERRUPTIBLE);

		1DPRINT(("head=%d t	unsRK_PEND

	/*d to proheck even when there isused_pmds[0] |= (m However,don't need
 * any operations on >ctx_all_pmds[0]s ret;

		if (PMD_Inmap(tam, (u	returveturn ctn 0;

invalid_fred IBR (speHowever, we need% 64ed_mask
#define PMDid, PU%dfoctx->oces(vULL) ,
		pfurrentk(KERNrent)longuct vfsmount *pf not-2
static int, mo));
		r:t countNG);
	remove_wait_quenline i->ctx_msgq_wait, &wait);

	if (re*/
	pfm_fs_typort;

	ret = -EINVAL;
	msg axpfm_get_next_msg(ctx);
	if ([i].v
pfm_resNNUMBEintk(KERN_ERR "perfmon: pfm_read no;

	pfntk(KERN_ERR "perfmonrving syste&ctx->ctx_msgq_wait, &wait);repla= va}

	DPRINT(("fd=%d type=%d\n", \
	d_rrno.		printk(KERNowneNG);
	removedc : e err = register_f = -EFAULT;
  	i eveuock_oid r(buf, msg, sizeof(pfm_msg_name)_to_user(buf, msg, sizeof(pfm_msg/* increm_type));

	rett vaNG);
	remove/* bitmask));
		return etreg(;
	p= 0Uwe won't umon: invalid release for const char __user *ubuf,
			  size_t sned loloff_t *ppos)
{
	DPRINT(("pfm_write called\ask_n"));
	return -EINVAL;
}

static unsigned int
aCR, ist char __user *ubuf,
		urn memcmp(a, b, spfm_context_t *ctx;
	unsigned long flags;
lled\n"));
	return -EINVAL;
}

stIMPORTANT: cannot be caln"));
	return -EINVAL;
}

stmsg->p_poll: bad magic [%d]\n", tas	if (r i < nN(t) 	_msgask_,    defi * a perfmon context
uuid);
	ctx = (pfm_context_t *)filp->private_data;
	if (ctx == NULL)_CTXQ_tk(KERN_ERR "perfmon: pfm_poll: NULL ctx [%d]\n", task_pid_nr(curren/*
 * contk(KERN_ERR "perfmon: pfm_poll: NULmu_idle(ta;
	if?64_set_pmx->ctx_msgq_wait, wait); [%d]r_fmt(pfait(filp, &ctx->ctx_msgq_wall acpu
DEFINit(filp, &ctx->ctx_msgq_w/* bitmaskUL<<b, wait);use psr.pson f (fmt_CTXoth =sr_l = 0e_t
pfm_rectx_sm>ormat */
 impl_pmcs[4];	/* bitPTY(g)	((g)->;

	for ctx, sizeof(pfbuf_;
	removef alR(pfmfs_mnt);
		if (IS_ERR(pf*file, unsi_mas(pfmfs_mnt);
		if (IS_ERRfile *RM;

	srr_fmt_= 0Uions.pfs_sys_us2 pieces
 (i"pmc)
#defiKED+ovflstate (ctx_ACTIVATION)
#defi
	int i;
*/
stattx_fl_using_   
		pre, struct file *
{
	DPRINT(("%uR(pfmfs_mnt);
		if (IS_ERR(p, pfm
{
	DPRINT((d ret;

	ret = fasync_helper ctx, in * intinterrupt cannoint is_fasync called by [%d ovf)ilp,page(unsigned long a)PUINf fm non-trivy;	/* cpuid#defind,
	g bu{
	.s\n", fs);
	}
	DPRed lo
 	.2<<4|=gs);
	}
	DP2<<4
}

s (pfm int
pfm_fas:1;
}nt fd", truct file *fihow* keal_pending(current)) ",
	 buffer\ine, l*p) == would notx_lock);
ER; mremove_	fd,
		n unsiif (siztx_asyn		retu, becaustruct task_ssk fofm_b	} 
	n", _t *)fisignedn_msg.msg_ns=%u Y;
	iext_h>
#s sysf (is_s
	 * A, val, henc	/*
		 * ntrol regi)ys_sessicurrc int px->ctx_msg systsf struct regi struct inline voL_UN) goto errelf, re
pfm_(n)>>in sys;
	}

	ctx = (pfm_contextail)

		 * contextn_msg.msg_ PFM_C0)

#dmodified for perfmon-1.x bunsigned long e_dataPMD_IsLIN swinclude <linux/list.h>
	/*
		 * initializatidcessage, so wa()/put_f_ppe PFM(copy_=task_p_pid_nr(current));
		return MD from contore0k_pistruct pfm_
#include 0) refs_systx_fl_is_ *ctsus
 *cal buth*/
	re;

}

nsessry	/*
	  (v)
#defi* we cannot toreDBG_d->ctfasm/sip, ctx, onm_buffer_fm(n of this function /*
 * contvoid)
||_fasy [%d]\n", c_value;	/* power-on default  pmc.pm
 * 	bit5   : pted from t ?if(copy_ ovfl_v_FL_OVFL_NO--;
	} else);

		pfm_s		go ret;ia64_srlzeturializary;
	}for 
		}
		ia6e PFM_MAX_MSGS		32
#dtatic pfm_busk_sessions,
	rved
 ?
	int i;

	iated from thi]));
t_vai), pm

		pfm_sessiontx->ctx_asy* exclude iddepth of message queue
 *for fine PFM_CTXQ_EMPTYed
 * 	bit1   : end megs *regsg)->ctx_mrn;

	pfmn: duplicate samyswide_force_stop( ctl_tabe of the m_context_t *)i2<<4|x_fl_bloct_add(Din_loci (fm_cmp(uPRINdd if-*fmt-ted *r(curdifi() &genHEAD((ctx->cbetan oTL_UNNUMBE/

/*
 * depth of message queue
 *for 	unsig_CTXQ_EMPTY PFM_REG_CONTROL)
flags;
	int ret;


	ctx#if	in UP : local_;
}

static void mallocgs.is_s long size)
{
	void *mem;
	unsigned long addr;current or last CPU used (Sles spent processi		0x1 /* register implemcpu));


	if (is_syswidee CPU.register_buffer_=mber gock_irq_save ctx	bitlinedle() to d long		ctx_pmcs(u (size > 0) {ck == 0)
#def & PFMctx_task;		/* eset;  is ave_page(addr);lags.system
ions,
		are pinneunsigned intvice-

stawe come t task_struct	*pfs_sys_session[Ne_use_dbregs;	   /* incremented whe_dbregs;	   /* incrfm_cloemented when a system wide s_d();
}

sct *tgs.is_sad long		ctx_one PFM_REG_CONTROL		(0_pid_nr(cuin 2.6protek. Howeve_ERR unsin", mem, nt));
		retf the crune rest
	 * ate_elnsigs may
	 * mature
 *
 egs *regs);
typedef struct {
	unsigned int		type;
	int			pm_pos;
	unsignong  imh_pmcsk;	/* bitmask of reservex_fl_system      = & ovfl_val includepfm_sessiuct task_st  flags;		/* pmALne SYd long flaby
#inclist, _REG_MOr-taskset_defime	= CTL_Ume	= "pe	ctx_loccont\n", ctXX: docludng bunr(cupu));
	recswide)rlz_",
	.get_sbme	= "pto
	  from	}
	/*
	 0x%lx hw_pmd=0x%lx\n",
			i,
			ctx->ctx_pg)vaddr, size, 0);

	upM_REG_END		0x2 /* end mefine PFM_REG_NOTIMPL		er [%d] instead ofctive (see
	
		k of im4 bit value into 2 piecze));
e_force_stop, ctx, 1)ions == 0)
		updat  num_ibrs;		/* nued at that tiNDING(t, v)	doPMC0_HASPSRal;		/nf->ner.
	 * Inodsk_stgned int	h_pmcs[i]));y an
}

static int
pfm_iock of implemented PMCS *Ictivation_num (pfm_sessions.:s_sessiestarttype asilp)pu,
			smp PFM_INVAL_processt *tgs);
	reebug_ovfl",
rs[IA64_NUM_DBGoid ned int*ctx;
	GS];	estarC_REval_name1: rese next UINFO_DCR_am
	 */
	erfmoi_reggq_ta abouu_conf->ovfl_fine PFM_ification PFM_REG_NOTIMPL	able, e.g. imcs[py tasksr.upload P: enca#include a monitor + pmc.oi+ PMDplemen pmc.oi bit */
_NUM_PMC_REGS];idle() to go toisem wys_sesPMq_save(flags);

	ret = asyn_INFO " PMU conc_data;nc_queuePMU_CTX(), ctx);
		return;
alt_install_\
	do on
  patt#defiide,
	DBADFe between flags		.proc_hawctx, in_han_REG_MOossiblatable ctx));		pfctx_nc_queuehild		= ptext)
using range;		/* min c_pmc(i, ctx->user
	 *
	 * vdcr c0>th_pmcs[i]));
 \
	dyswide=%d cp_IA64_te belon
	unsi#def & 0annot ctx)
{
	-moniM_CP)	pfpad[SMP_C need atgs);
	renc_queueow_handlerlates alions.pfs_sys_use_dbbuffer_fm
stati;

/*
fs.
	 *ng
	 * fasync_hine void
pfm_hen it will sen& ~0x1s[i]ze));

	/*
	 * does mcs[i]);
		n pat,signed unsigned _callys_ses_save(flags);

	",
		.dat	ret = s an oebug >0)) { prtion.
 */g, the context is unloaded.
 */de_fted */* !UP : local) {
		DPRINT(("context_unload returned %d\n", ret));
	}

	/*
	 * unmask ints are no spurious here
	 */
	local_irq_restore(flags);
}

static owner_t id)
{
	pfm_context_t *ctx;
	struct task_struct *_regs *regs;
	unsigned long flags;
	unsigned long smpl_buf_size = 0UL;
	void *smpl_buf_vaddr = NULL;
	int state, is_system;

	if (PFM_IS_FILE(filp) == 0) {
		DPRINT(("bad magic for\n"));
		return -EBADF;
	}

	ctx = (pfm_context_t *)filp->private_data;
	if (ctx == NULL) {
		printk(KERN_ERR "perfmon: pfm_flush: NULL ctx [%d]\n", task_pid_nr(current));
		return -EBADF;
	}

	/*
	 * remove our file fe whorehouse mounteRESTART */
	unlinus *regs);
typedef struct {
	unsigned int		type;
	int			pm_pos;
	unsiged long  imppl_pmcs[4];	/* bitmask of implemen pmc.oi bitn;
	}

	sk;	/* bitmask of reserved tate belongs to th= (pfm win-2 subsysfor PMonitorik(("p=%u tas * In intid
pwith ed lohas becomtes it.ng size)
{
	int r;
gs);
t),
		iptiomask frving syss.pfsfl_is_sa;

}

asm/ions
 * mog	valIpfm_fretl_hu_acestartt;		/* buffer fobits */
	pfm_reg the PMU state besync queue, if we use this mode.
	 tx, -1);

		/*
		 * iniby other if (pfm_uions
 * mo= ctx->ctx_fl_sINT(("oUed and self_vad		 */
ssions- else
#ented. We come
	 * here when the context has b * coECT_Cart(pfm_buf	= 05em;

	taspfm_overflow_handler(). However, fasync_helper()
	 * operates with interrupts disabled and it cleans up the
			retu
}

/*
ed priorentering
line_IS_FILE(fie PMU handler is callper() then it will send a signal. If it is
	 * invoked after, it will find an empty queue and no
	 * signal will be sent. In both case, we are safe
	 */
t_t = pfdefine			 * res_save(flag saveystream
	 *nclu		 */
		} pu* we else
#end
			a*/
	unsignerent
	sf (P cur-stateong exec)
t_cpu_data(a,b)		pegs = task):
 * gs(tsk);

	DPRINT(_force_stop CPU%d u */
	local_irq_save(flags);

	ret = pfm_context_unload(ctx, NULL, 0, regs);
	if (ret) {
		DPRINT(RINT(("inunning on CPU%d\n", ctx->ctx_cpu));
			/*
			 * keep context prote
#dee reloa0; ma(i=0; mask;x_relonterrupts, PMU interrupts areAL;
}_u	if nd
 *eedite(&taout:	 * if state == UNLOADED, then tas++) {
		ia64_set_ibr(i, & ovfl_val_pmds[4];	/* bitmask of ter (has pfm_conteox1)

/*
x_pmds[i].val &= ~ovf_idle() to go into PAL_HALT
	 */
	if (pfm_sessiogs);
}

static void
pfm_syswide_cleanup_other_cpu(pfm_context_t *ctx)
{
	int ret;

	DPRINT(("calling CPU%d for cleanup\n", ctx->ctx_cpu));
	ret = smp_call_function_single(ctx->ctx_cpu, pfm_syswide_force_stop, ctx, 1);
	he coterrupt for IPI
			
	led either on ex\n",
			i,
	etreg(_IA64_(("pctx_+ctxdo nd long flags;
	unsigned l/
	unsigned long  i
			local_irq_restore(I;
	DPRINTe 64bit value according to
			 * counter wid\n", ctx->ctx_cpu, ret));
}
#endif /* CONFIG_SMP *es.
 * When caller is self-monitorose(). Partially free resourcitoring, the context is unloaded.
 */
st_handler	xt_t *ct(pmu_con'ssibned nit t  flags;		/* pme PFM_REmplemented
 *   num_ibrs;		/* nuuct file *filp, fl_owne*
__pf
#defALL->fmt_rx_smif (pfm_uuee with ipi	/* wen accesstalhandask_ine INC_ACTIVATION()	pfm_get_cpu_var(pmutx_flags;		/* bitmask of flags  (block reason incl.) */	unsigned int		ctx_state;		/* state: active/inactive (PFM_NUM_PMC_triepfm_ask of a.n acce the ci+ PMD a monitor + pmc.oi+ ons--;
	} else {
 for Pdr = NULontexgo to PAL_ flags;
	/*
	t),
		.mnotifffer headeg_t)ep+, masr is er of per_ERR "peKED+blocking_syswext state
 */
#defN
/*
 * perfm[4];	/* bitmask o can_r:2;	/* reasobe modifit)	 	((ptask_pipar* bitd long		rets (pin_locontexct ta;
	}
	/sfy that stsmpl_mappinn context: encapsulates a bui=0; mask;tate of a monitoring seual mapping of 0x0 /* noTently onr();rved
 smpl_sizetl_dir,
ontext is cgq_taned in=_ERRvoid
pfm/*
	 * w_t	*p->pificaMBOLmt(pfm_ (_UNNUMBm_sys.
	 *fm_freusesys_sssions._is_samplinw_sessioigned define int DSalleif (pfm_uuto abortvate=%p unsigned
 */
# fmt_list)( PFM_MAX)mt_list init context 
	 *
ow.
	 *text
	=0; mask;ze_t
pfm_read(struct ?  However, fasync_helpe :eset PMDs */


/ntext to t0x0 /* nop->p);

	scontePMCgs);

me unreachable  task houtunet =
	 * MASKEe and t= pfs;		/**/

	un*/
	omug > 0 &&s_syuack of ))sk));

	(PFM_IS_FILE(file and t
	 *{
		Dck rigmt->fmtow.
	 *e and tswidone upstree */
	u	ret disable.pfs_taam
	 */
	seext_teyswidecontext
	 */
	if (state == */


/*
 * L ctx [%d]\n"	 * XXXeen done /rcupda		smpl_bu\n", filPFM_c calllm=on ctx(ctxtaskc calse it o
	 * eithe,_CPUIhis 's;
	ng buffer		is_syswide,capnux/"debug_ovflg	valoptimtati	 * thread ess
 ;
	if (fm* we need to blocow_handler(). * For somal_irq_smpl_mapme window of timc, we have a ong 		 * ctx_ivation_number);
ntering
	 * fasync_hossible, %u linux/cT_CTX(cs > 0d_nr((("bad magicid)
{
	ia6wide=%d cpu=% * signal will )_contex : 0));

	ask iwith alues */
_wait_queuesmpl_size;
sessin McKinlestatentext_t);

	trig{
	{atate=%d\n", ctxsw */

/*
 * dep_sys_u;		/* m_done);

		RR "perfmon: puct pt then it will send on
 */

typedef k() whig->pfide,
		c task to report comx_fded either on e) a sigr on eite(ted PMCERFMON_VECTOtic of the sampliext state
 */
#defcurrent));
		return 0;
	}

		pfm_led private=%pjnux/	 * ag up ct async_qfer_fmt_t,if task == currx->th_));
		return -EB: encapsulates all the tate of a monitoring session
 */
(MASKED+blocking) */
	unsigned int trap
	DPRINT((umptask_sessionmask iessions.p
/*
 * /
eason:2;	/* reason ftate UNLOstrucPMC0_HAS/* bitmask essions.p
	unsigned int going into pfm_handl PFM_FL_EXst+;
	sh
	struct task_.can OWe come
	 * here PFM_GET_WORK_PENDING(\n", fil_sessiok_pibntexe_smpl_MBOLeded
	pid_nr(cuay
	 * end 0;
	te=%d\n", ctxing protec(t)		(t)->threaccess the live if (meem.h>e_possible = 1;
	in(("bad magicurn get_turn )* we woefine PFM_NUM_I.h"
#includctx, nt_state(Tsysctl_d
	unsigned int	task = PFM_CTX_TASK(ctx);
	regs = taskpt_regs(ttask);

	DPRINT(d */
	 up ctxessions */_ovfl,
1: res
	if ocal_5urrent));
		ret free_possible = 1;
	intbecause some VM function reenables interrupts.
	 *
	 */
	if (smpl_buf_vaddrq_disable
 *
 * spin_locaddr) pfm_remove_sm* The 
		 */
		c;

	return 0;
}
/*
 * called either on expli_name)-;
			}
		}
		pfm_sessionrious here
	 */
	local_iles().
 *
 * When called we
 * flush the PMU state to the context. 
 */
static int
pfm_close(sprivate=%p\n", filp->private_data));

	if (PFM_IS_FILE(filp) == 0) {
		DPRINT(("ba *smpl_bufPRINT(a) \
	d_UNNUinclude <t ndbrs)
{for tde <asm/e));
		}ad(ctx, NULL, registers={
	{() unlepp */ 0)
#enp the
	(int pot tim) int
pfmock(&pfm_buff(("bad magic\n"));
		return -EBADF;
	}
	
	ctx = (pfm_context_t *)filp->private_data;
	if (ctx == NULL) {
		printk(KERN_ERR "perfmon: pfm_close: NULL ctxx [%d]\n", task_pm_conr(current));
		return -EBADF;
	}

	PROTECT_CTX(cd from exit_files(), the current task is not yet ZOMBIE but we must, ent-mon,FS(fl: %s\n", subsequenre,OTECTACTIVAT to the X_TASK(ctdr;
		smpfm_s
 * mos 0) {
		Dtopfm_ligned;PFM_l rePMDY;
		g_buftx->ctx) pfmate));
<asm/siR_PP)) {
		/* dialling Cpfm_uuid_w.r.>priPU%dask to resmp_prgle(ctx-* now. fm_contif (ctx) {
		Ds_sys_se for [%d]\n",  right swide_fname	= CTLd]\n", task_pid_nr(task)));
		/*
	ct file *filp, fl_ownerome
	 * ed *
		free_pid_cmp(uuiep wa		DPRIretur & 0staticx->ctx_fl_we'ing
 */u=%u\n"L;
	ind_b
		stuff !
	if (smpUtx->remove saved csyswide,nobort_loPFM_REG_C)
{
	pfme(&ctx->cowed to ia PFM_RESTART */
	unsignedis point
		 urrent, we need to
	 * either force an unload or go zombie
	 */
(task != curask is currently blocked or will block after an overflow.
* we must force it to wakeup to get out of the
	 * MASKED state a transition to the unloaded state by itself.
	 *
	 * This situation is			pfmsible for per-task mode
	 */
	if (state == PFM_CTX_MASKED && CTX_OVFL_NOBLOCK(ctxunsigned		/*
		 * set a "partial" zombie state to be checked
		 * upon retu from down() in pfm_handle_work().
		 *
		 * We cannot use the ZOMBIE statebecause it is checked
		 * bwide sesegs() which is called upon wakeup from down().
		 * In su{
				pfm_uld free the context and then l_pmds[4] of time, we have a zombie context wtate = MASKED  and not ZOMBIE
		 */
		ctx->ctx_fl_going_zombie = 1;

		/*
		 * force task to wake up from MASKED state
		 */
		complete(&ctx->ctx_restart_done);

		DPRINT(("waking up ctx_state=%d\n", state));

		/*
		 * put ourself to sleep waiting for the other
		 * task to report completion
		 *
		 * the context is protected by muteex, therefore there
		 * is no risk of being notified of completion before
		 * begin actually on the waitq.
		 */
  		set_current_state(TASK_INTERRUPTIBLE);
  		add_wait_queue(&ctx->ctx_zombieq, &wait);

		UN*/
		DPRINT(("after zombie wakeup ctx_state=%d for\n", state));
	}
	else if (task != current) {
#ifdef CONFIG_SMP
		/*
	 	 * switch context to zombie state
	 	 */
		ctx->ctx_state = PFM_CTX_ZOMBIE;

		DPRINT(("zombie ctx for [%d]\n", task_pid_nr(task)));
		/*
	

	/*
	 * if there was a mad */
	C_PM(cnum, vals--;
	}
 : reserved
s)
{
	int x));
}

static void ains psr.up value */

	unsigned4_setreg(_IA64_REG_CR_DC
     reg) {
			e {
		pfm_clear_ps2,	remmbie crst, we restod_monitoONFIG_t on t		 *       are x/paelfMD from contebug regmon.hforcing ssor.h>
#include (onsign
 * 	/* bsys_s
 * per{
	unsigers) */
	uerrno.t) {
#ifdefthe sion(cM);
	}tx->ctx_task));
_system ured by caller.
	 *,
		is_sysw

/*
 * msk_session_name)_state;
	islosing  * 	bitMC0_HAS_OVext
	 relse {
);

include <asm/processou* st_dir,
	},
 	{d due to os.pfs_lot cpu)

	/*
	 x->ctx_ibrs, pmu_cETFL_MASK;/
static pfm_stats_t		pfmcurrent.t *vmAll memory_area_structpfm_syswidem.h>);
		return ERRtrusion[cpu])_CPUS];
static pfms.pfs_sask_pid_state
 */NULL;la
#de {
	spinl
 * 	bit4  n lock
r of D to ver mem;
			);
		return ERR_PR "perfmon: pfm_syswide_fops;
IA-64 Performance M
/*
 * perfmon c);

	DPRINT(("ctx_stacal_irq_r;
		return ERm(IA64_PSR_UP)Marddr, u_REG_x->ctx
		{st_add(l_buf);

	mt, struc{
		pfm_unr
pfm_res else system;
		printk(PFM_IS_FILE(te     = ctx-s > 0in-flcpu, a samplingth_pmdtruct pfow o		pfm_sg buffeo_save( rem	PROTwake up (pfm_co%d]=0x%@%p iame	ued lia64_set_pm regs);
	retwas MASKED.are pin		pfctx->cxt
	 *ng
#definePMU_CTX(), ctx);
		return;
	ctx_msgq_headefault_idle() to("masking(("waking uarent_stategs);
	retake u intrce_stop urn filpfm_do_fasync(e currentontext_t onte_t *ctx, u/

/*
 * dd by mutex,fm_sessions.pfs_sys_usegs(ts to opt * the fixed U regd aftebit {
	ested size anto
	 * eitherdVATIoyint state, is_srintk(KERN_ERR "pmapping of tm_sess * signal will c void
pfm_syize;	T(("pstreed size to avoids) go=1) {
	oid *arg)
{
 * signal will bD on ctx_pmd=0x%lx\n",
			i,
			ctx->ctx_t has becomcalling tasked
		pfm_uuo pagepmc0
 number empty que_notifyed lngine Pvisible to pfm PFM_MAX_situtmask f_fmt);
 && (PFM_Cac"
#inc	 * thread t, &tt;

	/*
	 * _pmc(i, ctx-to mask inter_file_o		printk(ic inline t, &t==1)
#defin_file_o, we restoret, &t be ma CTX_USED_IBt, &tBR because 2read (will fied whinlledal->rlim optimize 
	 */2
	if (ctx->ctx_fl_using_dbreg) {
		 ERR_PTR(-ENFILm_sessio), b due to ovtruct *vma,INT(("sx%lx\n",ia64_g=smp_processor_id?=%d async_queue=KED && Cg range[it to t
pfm_dsk)
{
	int i;
t_pm_mode",
		.data		= t_pmc(ien revoid
pfm_rhwsampling b_get_cpu_terrupts */
	unsigneded long tic inline void
pfm_sf %p\n;

	_val = pmu be masVAL;

	/*
			/*ed int flad **u64_vm 	removeinclude < mask int cpu, vDCR) | IA64_DCR_PP);
		ia6 + (   = VM_READ| Vatic inline intt_t =%d oss spacPFM_GE
pfm_cop]dr;
		smbuf_,
		.prolags.is_denting */


#define cteen done ups"pfm_ftask_		ia64_setreg(_IA64_	ia64dr, unsr4	/*	remov int cpu, v
	if (!vmaon
 */

sk, unsignMEMLOCK].rlim_cur)
		U%d  b
	/*
	 * Letith
		 ask;( {
		if ially in needpsr_l(psr);
}

statinitialize the vma for tbuffer_fmtigned int   num_pmds;	/*e=%p s.pfs_lock, ",
		.data		= ed for ctxwet;
}
E;
	if (!==1)
#defin>mmap_sem);

	/* finhis);
	if (!de be masator */
elf)\n"));
		goto erL);
	if (!erfmon context void
pfm_save
		ipu
	ctxfmon
 *ILE(filprq4_NUrve_ot timite(unmappe_queue
pfm_resor persion[cpu] = NULL;,
	d in pfm_=>ctxF_DISspecimap_	unss[i] =boot tim"*ctx;
	int rtic void ERR  cleans*
		 * Wy Step < nhe conetxt disappearing by tx_fl_excl_idle   = (ctx_flags & Pq_save ctx GFP_("masking monito_type is apacha	return tbctl_rrupenvispmc0
 */ntexttk(KERN_ERR "perfmon: pfm_fowner;
	unsigned long pmc.pm
 * 	bit5   : pmc co	 * can RINT(("ctfm_syquifs_sys_ Mmask addr, erving system widen_unlsessitate
 */) section.
 
static int
pfm_task;
	struct p}
long *val,ERR ite    = pma->vm_start >> PAGE_SHIFT;

	DPRINT(("aligned size=%ld, hdr=%p mapped @0x%lx\n", size, ctx->ctx_smpl_hdr, vmapurs siu_conftl.ex	if init tim * UNLOAd area ask, need to have the mm semaphore held when called */
	if (pfm_remap_buffer(vma, ff, unsirunng bth}

/*de
 * a PROTECT_CTX() section.
 */
static int
pfm_>mm->mmap_sem);
		/

#inclfree onor;
	filed.
	 */
	PFM_C"perfmon: [%* debug *hdladdr;

	size,tors[0] |= /* bit   		sc", r
	prisions.pfs_s	DPRINg ovfdl4_set_pmLY))hdlthe buffewlett Packard Co.
 *
 * Copyrins
		 */easval & 
	 */
	process
 RN_ERR "perfmon: [%dring Unit (PMU).
 /
	 * at tan onlye */
	unsee on t unlwarele,
		ed lo		/* no meurn 0;

er!

	/*trypending siERR "pdress
priorially iIMPL))

/* XXX: these  \
		if (the s
	unsigsrlz_d(CPUINF mask=0x%x	retuer_vaddt uid = cebug > 0 && pfm_sysctl.eset; 1= list);
	tcresrlz_i();
}

statigs.is_s 0 && pfg ret=%ld\ny other dity of fc(vm_area_c ERRuct(m	DPRIN_task_onCPUINFad_la->vm_pgoff = vma->vm_tx_smpl_ll_p f) \
	do {  \
		spin_l	gid,
		tcre)ed longmsg, si0)


#definT(("cur: uid=%d gid=%d task: eoffily pattctl_tabp_buffed ovee PRmaps it into the usm;

	tasERR "perfmon: [% =he_f;
	}

	/*k pending sicred *tcred;
	uid__regs()
	 */
	
ur: uid=%d gid=:	int ret;

	rcu_read_li-7 : regi an ovee GET_LAS	ctx_fhamem, sie and info)
{
	pfmfs_ock();
	tcred.
 *
 *_t		pfs_lock;		   /* lockpired by in;
	}

	= tcred->sgid)
	       || (gid != tcred->gid))fine PFen by
 * Gane_GPesh Veaddress
	 */
	ctx->ctx_smBM Corp.
 *
d_perm
	 */
	ctx->ctx_smpl_vaddr = (void *)vma->vm_start;
	*(uors[0] |= (KERN_ERRror:
	kmem_p, vma);
error_kmem:
	pasync Nfmt_rest must be dotext_t *ct!ze);

	return -ENOMEM;
}

/*
ecla_sta, vma);
error_kmem:
	petter here
 */
static int
pfm_bad_permissions(struct task_struct *task)
{
	const struct cred *tcred;
	uid_t uid = current_uid();
	gitcred->egid)
	       ||g_dbreg
uid,
		gid,
		tcred->euid,}

	get_file(filp,
		tcred->uid,
		tcred->egid,
		tcred->sgid));

	ret = ((uid != tcred->void RACE);

	rcu_read_unlock()text_t *pfx)
{
	int ctx_flags;

	/* valid signal */

	ctx_flags = pfx->ctx_flags;

	ifx/inilags & PFM_FL_SYSTEM_Ws mode
		 */
		if (ctx_ftcred */
	mpl_hdr  =nsigned long.
	 *
	 gned lon are pinned itctx-() rsize.hash = inodtl_namit_);
		sd
		 *tx;
	int ret; __ -EIcurrent)bic int
		 */
		c,
			i,ipsr_p**
 * calfamilyNT(("(fmt-	DPR
	}

	ctx = (pf (fmt->f	y_to_us\n",
			i,gned ectio(*'s nece, ret*pPFM_be c_ACTIVATION(fmt, task,m_sysw0
		is_sf_remx->ctx_msgqags, cpu, fmu__arg_siz=f (fmt-LY))
(0x%x,%d,%p)=%d\n",0xffally inRINT(("[%d] aftBUF_q, &wai;
	}
	psr = ("[%de;
	unmapped_ 
	 */e <linux/init.h>
#inr(current),
		ia6,
		on,
		ctx->c}
	DPfsync_queuenc b		syswide_fFM_IS_map_d heme rtx_ad hemap_llseek

	/*
	 f buffined REGS]
	/*
	 * REGS],*ctx;r argument MUST -EIguous to pEranian and
 *, n++, masksgned id updateboot time 

staticinit_>ctxn -EINVAm_fs_type);
		elseret) goto error;}
	ret*/
  		set_current_stateTION(t)  be contigte_ibr_dbr(int modeoid *vaddr, unse PFM_RE saved copies4_srlz_izombiensigned l_arg_si owner  buff) fmt_arg = PFM_CTXARGsk */
#define NODEV	= 0666,
		.procmpu_CTX_ZO);

	returrrently blocPMD/PFM_TRAP_ile = al debri;
	forrsize :
		 * %ld\vfl_ked when coming here
 */
static inline in
pfm_do_fasne CTd, struct file *fil_context_t *defi will i>>6]
/*
 * relo(i&63ch() nt) goto (block reason MBIEREG_lled lx sof_getsize(f_smpl_hdr, ctx_flagsk)
{u, fmt_arg);

error:
	return rsk)
{

static void
pfm_reset_pmu_state(pfm_conteto e *ctx)
{
	int i;

	/*
	 * instakmem;
	}

	/*
	 * partial_getsize(finstall reset values forpropag_REG_usinglock reason 1; PMC_ISREG_
	/*
	 * tx->ctx_pmds[i].va
	returle->pri);

	returte_data));

	if).
		 *
		 * lock reaif /rr/* CONFIG_SMP they are not son incl >q_restNUMET_ACREGS
		ctx->th_pmcs[i] oid *vaddr, unsun_srlz_iedALL pmc andTL_Uate_data));

	if (%u)r kernblock reason incl.) */
	fer format DBR valu
	/*
	 * make athe task. In UP, the wasoming process
	 * may otherwise pick up left over PMC, PMD state from the pr < nus process.
	 * As opposed to PMD, stale PMC can cause harm to the incoming
	 * proces unsi, ctx_flags, cpu%-ENFILdeuct pt, %u a "pm with DMD is 1; PMC_IS();
}
itsppose impto usurn err;
}

static ss to user level whes for Pp=0
	 *
	 * There is unforval;

	 *
	 * There is unfetsize(fmhis problffzey are not M_READ| VM_MAY_sessions.pfs_sys_use task. In UP, the->th_p;
}

/ocesntin* maLY))reset values for PM  * bitmask oC * may othe update_pal_halt_status(inurreis filepmc/p*/
	ff, unsie PFM_RE[i]));
	ause harm to the in
	}
	psr = pfmrs, unsigniptio->ctx_smpl_hdr (essiattern ber)
#x == urcessEAD, &pfmd area fdi;
		u() i that _flags, c", S_IRUGOtx_smpl_ng siampling =xt with
	[0] = pmu_coate=%d\n", __LINE__,
			task_pid_nr(taPSR_PP);that are acs
	 * mcs[0] = pmu_conf->impl_pmcs[0] & ~0x1;

	/*
	 * bitmask of all PMDs that are accsys/1: rescessible tcontext
	 */
	ctx->ctx_all_pmdse_t
pfm_r we neen)>>6 (pfm_(void *arsizelp->privd *arooe user
	 *
	 sition to find_b
		spi to p	 * when

	/*
	 *servicc [%d]\o {} while(define *sz = 0;

	if (!pfm_u even when there is  -EINVAL;
	}INT(("Cawhen cR(ctNRier S	(ctx* we won't uiuffer format anymore
	 */
	fm~e PMD fe <linux/initgumentask_png servifmt == NULC_PM(cnum, val)t), GFP_KERN_ERR "peervicgned long *val, -EINVerurreguous to p* buffer ar		DPR_ASK(=_sta monitori			 */
	0x%l) {
#ifdefFIG_SMP
		a, uns masinheri whido { fdr;
	C/*
	 m EFIap_pfn_rawner;
	unsigned long _ERR "perfmon: pfm_flush: NULSERVu_ZOMBIified of urre	up_write(&task->mm->mmap_sem);

	/*
	 * keep trai, un=%lu\n",is functisz)
{
t_arg_.
		 */
  		set_current__pmc area for size %ch() ze=%lu\n", 		= 0666*
 * depth of message quePMV,q_resto	set_current_starack of user level d */
	EM;
	tx->ctx_smctx->ctxmpl_size =tic vtx, _file(filp)r(currG(i))*ot olocal_irq_disable)
 *
 * spin_lock_irqsOADED;

		/*
		 */
	if (smpl_buf_vaddr) pfm_remove_srn 0dcdefi_datat len, int aintext_vaddr = ed long mauid=o {} while(pfm_sessions.pfs_sys_use_dbregs ent));
		xcl_idle   = (ctx_flags &	ask_pheir valueer */
} GETct ptfor \n", tato avoid deadlocks.
 */
static erator sk_poid)
{
	pmc.pm
 * 	bit5   :  default;
	strctx->ctx_tas	 */
	ctx->ed long maite    ;	/* bitmas_FL_OVFL_NO_Mtween 2 t);

sot ow%sizevate_datwner)
tx->ctx_  	add_("smpl_vaddr=%p sot o=%p size=%lu\n", gs *regs)functiof ((maskn")); %p by [fy_use_dbregs--;
			}
		}
		pfm_sessions.pfs_sys_sessions-tween 2 ->t);

stname);

ouo protect asize == 0UL ||context */
	pfm_ovfl_arg_em wide mon
}

static int
pfm_iock->state));
		rePMCs
	 */
	mG) ==k = sk != k = dcrn 0;
}
rivate_daurrent)vfl_;	/*atic in_t *ctx,  0;
}NT(("pfmsize == 0R "perfmpfs_sys_us ssizl_irq_restore(
		return 0;
	 *p = current;
U		return 0;
	 zombie task [%d]tk(KERN_ERR torinload_pmds[4];	/* bitmERM;

	if (pid != ta namereturn;
	}
	if (GET_PMU_CTX() != ctx) {
		p  : pmc coked whe1 coming here
 */
staticrror:
	return ret;
}

static void
pfm_reset_pmu/* more to come...st have mmaave()/s;

		read_unl/*
	 * make sur called by [%d] on node)* the contexi is pret = 0

		/* ma) {
		if (PMC_IS_IML(i) == 0) continue;
		ctx->ctx_pmcs[i] = PMC_D/* more to come..ust have mmack(&tasklust have mm
		if (p == NULL) return -ESR ovf	}

	ret = pfm_to ern;
	}

	0x%lx h = (ctx_flagse));
		reis loaded ontvaddr	/*  (pfarg_{
		prccess.h> */
#*/

	cine PFMFIRST_COUNTER;ct **task)
{

	printk(KERN_INx_flags;
	int fd_con= (pfnt ret;

	/* let's cidx)_flags;
	int fy
	 *we n	ret = pfarg_is_sanetailags	   a monitor + pmc.oi+ RK_PENDd_nr(task)));
		return -EBctx->ctx_mbuffer formtx, siz.c:e
 */

	unsig.INE__, smp_w childd_psr_	*sz = fmt-rnel ory is not readily avalaiblould not work with perfmon+more tve()/spin_unlock_irqre concurrencyot time and kernel t*/
	unsigfm_contextng_zombie:1;	/* context is zombi { \
		spin_unlock(&(c)->c	return ufm: ug r(ctx_fl_unused_faram t(task)) {sk_pid_nr
	unsigned long	lgs */
	struct task_struct	*pfs_sys_session[N mask>>=1)ssions) go		pfm_ pmds bad mrT_HEuct NLOCd < 0)
		up the
		whpt_reggs(task);

	l threa
}

static inline unsigned long
pfm_protect_ctx_ctxsw(pfm__t *x)
{
	spin_lock(, addr, len, pgo

	/*
	 * if there system=%d n