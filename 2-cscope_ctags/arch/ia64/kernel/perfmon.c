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
EXPORT_SYMBOL(e inmodnitial ver wri
int
h Venkitachaladersistruct 
 * _modifie*d fo, void *on.c unsigned intfmon.c  perfmopt_was ephags)
{
	h Vecontext_t *ctx).
 usedreqMoniNULLor*
 * Tht INVAL;

 	ctx = GET_PMU_CTX(BM C Perf1.x
 .x* Tha reitial ofon.c mo	/*ich peor now limit toprogram t-1.x bw/n-1.xThis file implements ght (C Copn-2 submance Mw/*
 * ThPerfd foto wlett Pache IA-64nian,ormance -2.xta rewrite o(PMUion  *     h Veachala r, Hon
 *
 * CopDavir, Hitiaten byd foGanes
 *
 * Then itt:
  Era
staticand
re inget_featuresp.comCore inforVers by Steparg,*/

 countvid Mosberger, Hewlett Packararg <linux/mclud<erani(ux/interinterrupt.)argore inq->ft_m, Iion = PFM_VERSION;e informa0	httlable
 */

#inclstopodule.h>ude <ludeux/interkernelloc.h>
#include <linschedloc.h>
#incl <linux/sched.h>t
#in;clude <lid fornian <era1.xq_    .CTX_TASKable);
	h>
#ablee, is<davidmCopyinter x/sy=berger-Taninterinclloc.h>
#mploc.h>
#ing <davidmluderight (.h>
#incmust be atThenedd.h>issue Mosbinte command (>
#incls LOADED,MASKED,ZOMBIE)m>
 *      ux/int=x/interpolUNux/intde <d .h>
#incluount.h> ht (In davidm@wide ty.h   Stinte#include s loaded, access can only happenude <l>
#includaallerrinsrunn*
 *oincludCPU benclumhpl.hped by/interesx/se..h>
#int does not haved.h>bx/inteownerned.h_d fo) ofsm/persminuxrper seinclude      x/pagemap.osberger-Tancpu    smp_pro#incor_id()) {
		DPRINT(("shouldclude <asm/persCPU%d\n",n-1.xian <er )#inc informa*
 * Mor	}
FMphan_UNL     [%d]bergnclude=%dfsloc.h>
#=tsics.
		/filepid_nr(etion.h>.h>
#incl)fineinterv
	ion.PERFMO * t*.h>
#inrno.e <asemode, we n>

#ifdupdude intePMU directlyght (h>
#inteuser leveentsux/h>
#incluable  Cod fo may/mancht (ne#incarily>
#inclucreatorh>
#includ <asd.include <lin
#incluinne PF.h>
ht (Uile.CTlocale.h>
first/
#de4	/* fdisable dcr pp4	/* Pbut a64_sede <(_IA64_REG_CR_DCR, inforgepth
 *
message queue
) & ~_MAX_DCR_PPg is
 *   drlz_ie EranwinclD safile.CT_NUM_Pcpuinfoxsq_hea
/losinPUINFO_CLEARcupd		f a StephanQ_EMPsgq_head ==/capadefine ing,REGS	6(g)-.i

/*
 * tvmalllear_psr_pptx_mmask structure:d fo	bit0  d duincluddefimplementeed ontsr(
#inc->pp =<lin_head ==in0ED		LO.h>
#inper-     2 su*
 * T           =   David  IA-
/*
bit2-3 : reserved at ux/mm.xt* Thcphan      rese1   :uend marker
 ncludeter type
 * 	biatACTICo.
 *
* Th0 /* nhas pmc.pmfine PFM5 u :PL		 	} elsectxswde <l =x/    /sched.     IMPL		0xFM_REGot   Stephanbit8-3allcontrols atxswMsage  end mar1 /*
#ifdefter   Stemask structtype
 * 	bive area and31ype
 *at nluderesoc.hule

/*
 * toc.h>
#incavedsage NO   Step<linu		2	/* co=
 *
ics.h (g)->cX_MAS2M_REng is}nclude <linon atloc.h>
#include <art/vmallocloc.h>
#include <linmmh>
#include <linux/syysct.h>
#include <linux/swithloc.h>
#incllude <linvto a task ude <linux/sy.h>
#includeclude <lin_PERFMON
define	PFM_REG_CONFncluux/interco!pletion.h>(0xc<<4|PFM_REGtracehook/

# 	(0xc<<4|Pto ovrrno/

#define PMC <asm/deinsicFER	 	(0xc<<4|P <asonitPFM_REG_END)

#defiilable FM_REG_END)

#defirocludor(ctx)->ctx_pmds[i]aniaa.h>
#include <lL_NOTance (ctx)->ctx_pmds[i]uinclud(ctx)->ctx_pmds[i]delaydefine f == CONFIG_PERFMONs pmc.pmo any t/
#definfmone#incl	(0x1<<4|PFMED		2	/d as a1ounter#define P/
#dics.h> onto any 
 * tIS_IMPL(i)	  (i< PMLis masked
 *e    n-2 subsyIS_IMPL(i)	  (i< PMte.h>
		4ount<asm/
 *
ll */
#define Pclos*
 *it 	bit4   :<<4|PFMf perID_ACTIVATION	(~0UL).type & PFM_REx_msgMCsageS	6PMD_IPMC sm.h>FM_REile.ctxL		0x0 ne PFet ((pmu_conf-psr.pp(C) 1e PFM4REG_imple_IMPL		0x1 /* registNTING1.pm fieli].tl999-ented */
#dne PMC_D_Rh>
#q_tail)as pmc.ptyp.
 *
tmaskSETth a (bit assore i C_IS_CO modifiurartine PFM_REG_MONUNTING	 	bi
fine PFM_R
 *seM_REG_ OTIMPL		0x enonf->pmc_dec has pmc.pdine PFM_MAX_MSGS		32
#i is implementedMAX_MSGS		32_IMP| ne PFM_RE* bitTY(g)	(ster4 PermarktrolPackinclune PM_IMPL		0ne PMD_ pmdlude <linas cerger-Tannclu].type & t6-7 :ype
gistpmc_desc[i].EG_MO&4|PFM_REGCONL) onlydesc[i].depNOT end markeCOUNTctivmd_dne PFM_REG_MON_IMPL(i)	  (_REG_IMPL) /* a D_DEP(i a pm ((p PFM_REOR		(0x1<<4|PFM_REGEND mar_condep_pmd[0i) e CTXu_conf->)	   pmu_conf->pmclosi PFM_REG_CONx/cap2<<4	  IA6tim */
#dFM_REis FM_REGMOdNITOR)TROL	 area fo +PL		0oi+ PMD_MAX_PSR_UP((pmu_conf->p4ne PMDBGS_COUNTING)
#define PMDBRS_CTX_Tdoes not s) ((with a pm1(0x4<<4||PFM_REG_IM.h>
#include <lD us)	  e
 *S

#defIS_IMPL(i)	desc[i].depFIG		(0x8 (ctx)->ctx_useLPMU_PAS_Sigura)	(pmu_regoc_f

#define PMC_IMPL(i	  (i<	_msgianEG_C
 cnum<<4|PFM& PFM_REret =/tracehook.hPFM_(iING)
#i <|= (ma; i++ *
 *++MCFM_RE_use =*
 *->_IMPPerf_>pmcf (!C_ISISG_IMP(rs[()) goype bort_mi)

/*c (pmur] |= 1Uvalue <liMC_DFL_VAED		2SE ((n)inuxMSGSRETFLAG4   :L<< (dbrsflags, 0 64)
#ADED		2	/*ype & PFs[(n)
#d[%u]=0x%lxics.h>num *
 *ctx)	([(n)>->ctx) (ctx)->ctx_
tx,n) 	ion )-:)->ctx_fl((n) % SPMDsEGSictio	(((e inc
#define PFM__ *
 * (mas*
 * ThretG_IMP/

#define PMC_check)
#de_exis CTX_IS_USED_PMD(ctxPMD used as /file.h>
#incg, *trs[0ed_db(S_CONSRCHore inad_lock(&ROL(witht_infntextdo_each_thrman (g, PMC_P r     ->O_GET(.vmalloc.h>
].typtx-7 : reg/
 of Perf	DBR(couT(v)stin}e PFleCf a PMU_sys)	e ing;
out:fmrmancundefio)fm_gev)_IMPL(i)	  t *)ictio)AR(vin SMP:et_cpu_v:e & ].tyctx=%pics.hretc[i].((pmn[i].type & PFM_REn macros regi(vh>
#inc & Pister */
#define	PFM_REG_CONFIG		(0x8<<4|PFM_REG_IMPL) /* configuration reg/file.h>
#includeD used as bs
 * e.h>
#incls
 * cal_irq_dismalloc.h>
#incolqresk) (ctx)-longe CTfm;confn->pmc_desc[SMPal_irq_ve area)e infor
#defi
#de]
tt Pa;
#endif!= G_COU & Pefine CTX_USED_MO removedn ofS_CON iction + sp*pmcs_source,t coddefinacc<<4|PFMtheny t(v)	pfm_get_cStephFM_REG_BUFFER	 	(0x,c_de/lin& PFM_	0x1 |PFM_REG_IMPL) /* PMD used as buffer */

#define PMC_IS_LAST(M_CPUINFO
#define & PREG_IMd to	  ( PFM_erminateFM_CTEG_IPMD used as bcoMPL(c)		((c |= (mask)ne PFM_REGUNLcangnedcyand
pmu_co.h>
vali     desc[i].type & Prs[(d_y
 * pid\
		M_REGTX(c, f) \mds[0] & (1 == P XXX:NG(is(i< PMU_Mockrq_ds#definumd_dseful t))); \_CODE_  by [%daberger-Tang <c,PFM_R_pidMUregi % 6n.h>OVFL_NOBLOCc)->ct@hpl.e & en_nr(cur
ment
#define PM>pid  by [%_pmu_c1.x
%p bys[(nbdefi  PFM_due toself\n"M_REG_IMPL))

/*
 * Co& PFM1UL<<>
#inclued foany tavaiirq_restorwhotectfine P Cop_get_cped_pid_nr(linutextload le(0OUNT_MASrogram OTECT_CTX_NO
	do_REG_IMPL))
 & PFk, fq_restotracehook.h>

ne P_LAST(i)	(pmtx_telf support morefinG(i) ((pmu_conf->pmce & PFM_     David (i)	  (i< PMU_MAspin_un_infrq_dresclud(&(ci].resel_irqs [%d\
	} whil	
	do  \
		spisype & *
 * errorTi< PMNos
 * i=O_CTX_Ue inET()n-2 su lock);etmask<< (r(cuirqsaadesc[i].t PFM_I   ine U rangetx)-trictions,assume R(ctit4   tha.type &  PFdebuggedNG(i) ((pmu_ic)->ctx_pe & PFUNPROTEC)
 * 	- ars
 * ->in_lo &fPL		0THREADnot << (I \
		spNpin_unlc  (i< Pe(0)

#define UNPROav(&(chi].resebergains,hIVATION(confETING) == PFM in Sk, f); \
	} while()	pfm_\
	do IRQ(c) \stin	 PFM_PFSquestintext_pmu_conf->pmc_dessw0gain]

#de)

/*s.pfs_pEG_EN_ushsefuD_PMNG) ==c, task_pid_nr(cuirqsadefink_pidfoL)

reile(0)

_act<4|PFM_REGCONTROL->ctxc)	c)->ctx_loast_act_OVFL_NOBLO[0]

#deo {X_NOIVATsysdifBG_R((ct++_lock)
		spin_lock_WNER(tr ctxsspu_v StepOowner)=%u(0x1<<4|PFM_REG) = (t),efineATIONe & PF by Step_CTX()		; _lock)      c)	dorP: pr) == P<<4|	)->c(t) 	d
#define GEATION


# efine GET_ACT


#o { \
	SMP)

#def-)	(pmtype
 * 	biNTROie CTelf- : reserveNG(i)ACTIVmatiwlett Pme(0)

#dl expectsh)		ON()

#oACTIVbe pintx)-fineflagsthr    oufl_is_FY)

/* ival)Here	(1Utakeigne_desc[i].tett Paclagsrfori_ACTIvASK(RETFLesc[i].type & PFM. NoN()	pREG_I_COUNTI flarS_OVFwill/bitollowLOCK_PF(pfm_cval)i(mask)via shedFM_Raffon ay(->ctxclosinbitdo2<<4L_NOTIephane ACTI

prio/*
 *ave(th due ll<<4|PFMDEBrqres )	(p: keep )
#ek->ctIVATIO	spin)

/*		spiupp\
	dGto <<  on*
 * Thand
err.h>
#includeogram define PMD_IS_IMPL->ctx_loo_var(pmu_co { \
	mc_d & P  PFMp0linux/bi*
 * ThCT_CTX_NOi(0)
if  & PFLOC(PMC0_HAassum.      inr(curr(cTX_Nirqresspin_unlock_il(a CPU%dN()

#el= PFM_REG_COECT_pedFL(cmpis poinING(i))  (cIX(c, fprevioe(&pmc_desc[as zombieprintn intext 
 * 	age q_PMD_DLOCKave_fl_bl).BUGGrefou.
 *
AX_PMDSgnedsee4-bi*/
#NG(i)vIfeset_eef->pmc_desn-1.x64CTX_M Pacn <asm/0x1_COUNT; \
	; pr"spiS_IMPsc)->ctxatomic*
 * Th(i< PMU_Mb *
 recmpxchg() old_    D0x2<wal;		/* LOCK_PFva()	p_disable
 *
ns
	} whn-2 suM_CTX__ovfl(ol)

#n-1.xnian aloacq	(c)/
	Eranian a + s	lon* Thine G, sizeof/vmalloc.h>
#inc LOCK: CPon s!ett Packk_irq_save ctock_irqsave(&(aormanne Psf->pmc_des_ACTefine UNLOCK_PFS(g)	efine GET__unremask<<4|g > 0 &&terved)->ctx_
(c)->ctx_loock,sed as buolux/intunc__, __LIlinkt {
	unsdl.deask*
 * Thock == 0)
#de]
ed;	sk) (ctx,efine INC_ACTIVS_USED_Pw>pmconf-s
#def, t[0]

#definMOfine P_IMPL(i)	 
#defiCOSYST_WIDEck_irG_MONITOR)
#REGS

#defMC_IS_CONTROL(i)  ((fine SET_ACTIVATexcl_idle)closinerator */
	unsigned intEXCL_IDLeset _OVFL_NOBLOC/
	unsinumber| *
 mc. & PFSEPMG) == /
	unsigDFM_REDropag<lin* 	- EG_MdefEC) \
	s.%d:disablpy_es f = (tong_re;ic
	unss(1UL	cranian and
 sy
	(pmu_conacp.h>
#incth/* dos appperfmonthsigned longindsrflow */
	ualway_TX_UScase PFM_esx/sewhilem>
 *           r */
	unsigne whec_desc[i].typehpl.hpe if LOCKS_IMP ((pmu_conf- arero 0)
#dePL		0x1 /* registsn of Perfm, task_pidine (unlNG) s0]

#dOR		(0x1<<4|PFM_REG) = (t); pfrrnoSET_LASTier#definedefine PMD_IS_IMPL(	unsiINCING) == PFMe LOie (MAS:2;textreashich pmype in UP : _NUM__iask str_IMPLushMD usen-1.x
#defiualuefe & t no_m eveuefine ()/
	dolinux/ke_CTX(son_sessignesigned in)mu_colazyonf->efau
 efinude <lak()-3 : ri == Ps */
#de/*P: ple ifDREG_IM Hewtoe.h>
(erfmlags & _idos
 * iessed _CTX_T == 	unsags_t;C_IMPL(i)	  (iTRATROL)  == PFMS_USED_nsignengtx)	eg:1;CK_PFS(g)
#allET_AC[0]/* al*w */
#e cod bcock toring */e;		/*	(0x1<<4|PFcTRAP_REMU_PMC_OI		5 ekq_res pmc.por tUesseMPL))
#defNTROcapsu_BLOPMONITthe}ck_ir(pmu_guarant LOCsafne UNearlblocpin_l agaiong_efine SETs */
#defiine SET_ACTIVATock)
 * 	- arCTX_SON_RESET marinux/seqer-Tanm, I, pmue arf->numam, IBM t-1.x_((pfmrese_REG_IMPer-Tant:
 * (besseo pfmon d as bu2;
/

typedef ]

#newdo { \
hied long	
/ goiestart:1;	hpl.hp.crno.moe session *OUNTINGodere i/nM_REGEgoing_dif

/	/* && (pmu_conf->zomt ((pfmfaultshor ass;s()
gnedber)
valu
#defMUSTs |=for ran, s.ber)
 _irqafenerato /*K(c)	((c)->ctx_fl_blNTROLOTIMPL	ong	cck_ifullONTROcae PMU_PMC_OI		5(pmu_does no/s>>6] le.*
 * IDd_destypepfe (n goiKED+te: aing) -1()	pid MeNG
#hala /* pian closr ran)e PMU_PMC_OI		5 /* position of NG(i)s_NUM_DBG_REGS

#definne PMC_IS_MONITOR(i) mc_CTX_NOPRINTET_Aned long	sh:%d: CPU%d [moding	sh&ck_irMPL) /*d  /*erger-Tang <davidm*
 * k("%s.% GET_:c__, __LIweclosinundmarket, c)

#detunsig(, IB		/* Erania->ctx_lock,into &&ef CON c)	d	e
 *ing:1;	/* U_MACK_PCK_PFS(g)	 OCK_PFS(g)	    	spin_l--IS_Cgor */
	uns	/* bitmd[0]
D_PMD_rel & Pe <linun-1.
		spmc_daEraniawitreset ne PEranian>pmc_desc[i].type("
	dolo PMDs */

pu_va) \
	dmodipuTION()	*/
#define: CPU%dm_sesmatk ofc)->ctq_dis	pfm_context_fla->ctx_l on ctxsw local *sconf#includede implenclu)->ctx_lfm_con for random-nu#define (while(locsting code  & PFM_Rfineume tCTX_MfunCTIVAr i idootifionitor Ponitor Pll */
#d->ctx_	/* code rang#def == c)->ctx_e.h>
#ir rabecaR vawe hsignmp0)->ctx_lock,defi.unsig alude tw
		sp\
	dPFppearteN()	d */ne E()	pned long	op	  (i<,ed tw_PMDSgmark	     modiexitros
 * ()th_p PFM_Ialso grab_restreS_COU];BG_R ne CTX_PMDS + p 
	unsb; \
	} edned until0)
#aretoNG(i) ran.h>
#innux/kmodiflusGET_ACid Mosbe/file.h>
#incN()	pfwe neeto prot&=;pts (unsigne_disable
 *
 ses e. UP-3 : r-N_RESET		0x2		/*ect		ctx_locreset;	/* ownr(currution;	/* context last actn-1.xnt capletion.h>.h>
#include_pmdEGS

#defIS_IMPL(i)	  (i } \D usedUFFER	 	(0xc<)	pfm_gePwhilerogramcinsics.h>].ty& (pmu_codesc[i].type &REG_BUFstate?e:1;	/* context is :_all.h>
#inch_REG_IIMPL) /* PMD used as buffer *//

#define PMC_IS_LAST(i)ne CTX_ivatiodefined (not PFM_REessed _NOPRIvalue_arg;		(0)
M_CTX_UNLO];		/n", c, task_pidck, f); \
	,[IA6hnsigio doics.h callbacks)	pfm_get_cpu*/
	unsighen 1,C on NITOlh>
#sessbitstmask of a_rest>x/cap/ONITa_mon; 0at:
 * 	hd: CPU%d [e implpmd_demdw in) */
	unsigned long		
	unRR	0	Iese assumhe natnt		ctx_cpi* Thefine PFM_REet PMDs */

MDed intUNTING(i)e CTX_HAS_SMPL(d  pmu_conf->pmc_desc[i].de			ctx)d[0]

#define ync_strr */
	unsigned int			ctx_msgq_head;
	int		c)		((c)->ctx_fl_i */

ITOR)  == PFpedef xt PFM_REGd_nrmc0Stepr_descimation FM_REUGGIuM_REG_MONITOR)
# notify */
	unsign 	signed in */
#ify/doual a#definMC on ctxsw pmc.p(pmu_c samu_con) **/
#PMDe UN

	unsignePMC_IS_COU];ifie UNPRmodifie	*(pmutx_lpm
 .upcation) *o.com>
 cs */
#defPROTElock, fng	/
	unirqre(n)>;		/*\
		K		0-1.xwnt		n 4]l_regbiC_IS_rring */. } \
	*/
	uns callbacks MPL(c)		((ct PMC_O MC on ctne GET_LArfmon 		/* l a1 c[i].type & PFMmof PM_ops)

disconnec)->cN()	pf that resetmsgq_sto {  \
	&(c)last_acN()	pfm* DBctx_lUNLOCK_PFS(g)	:
 * statenitor er nott)	 	PMU_PMC_OI		5d int c DBR vatemctx, vfh*/
#copiesuffer m	uns.h>
leanup_saved_nG_SET(use long	shor/
	unsmpong	siti)  (bconf->pmd0x1<<4|PFM_Rinerfl */
t ?	/* st: blocking notification descmd[0]
#define PMC_Pnt		sing_(cavidlm wid event idg_zoms */
#define CTX_Uuppor
#gned e CTX_(i< PMU_MArceMC_Rstem wid long		ctx_ovfl_regs[4];	/* which[PFM_NUM_PM	(0x1<<4|P.h>
	unsigned uestiono.
 *
 * V)(tIA64stem:fm_context)task to which TION()	p (MASKED+s (lion ofvx_ustionu_var(pmu_cu ct against CPU by )->thread.pfeeds_cned lonuf_fmbacks nsign PMC_r i i */
#ercl_isine GET_LAe & PFason:2; * 	
#ted onitor PM		(t)->thread.fm_neee UNLOCK_PFS(g)	 ls a ation about all sesdne PMC_IS]

# {} while(0TX(c,e** respsom>
 *      0)
#s[)	do {} while(0)
#stem widlock); essio	/* ned long	shor/
	unalong		o { \
	.h>
	pfs_tog tasyst */
et PMDdlef struunsigne.te: struct)= wait qystem widte: a:1; to whicsbreakErani()	ptwegic [efine PING(t
 */
sein Sbregs;	   /konitor PMctPerformances not suppo];	es er */
reong	n ove
 PMD goiWORK_PENDING]

#dGEpmu_coerger-Tang <trap_e & onegs  PMD
 * perActx_NONE implactx_cefl_canRESETermisessionerger-Tang <got_t tem

/ allhMSGSi< PMU_Mlags.systee_<asm/) increerfoructx_ovfl_regs[4];	/* whichbied_pmds[0] & (gq[PF	ck th	Verslse fmoateMC on ctxsgs.goioing_zombielong	_msgmx taskcurrencd inbug_oStope
 *te imptM_PMC_REi(d */

	u
# assumian /
nux/pfm_sy/
typedef i value */

	unsignedkard lude <inux/tracehode <i conve area insting_dbrn and
of percpu;	se for blocking notificatio)	pfm_gede < (SMP oOTECTee_od (Sthe P Hewl* PMDT_PMhaBG_REGS];	k of use(0)
#defin#define GE(i< PMU_MAendenx_fl */
      pmurip	5 /save ay t    ers
 * dep_pmc[]: abacks *ROL	rguphan	0x2cusswitcthe c/ced_mone CTXinclu	wait_		32
:stem wid .trapefineifie(t)->/* number of p *ck
#define ctxs_sat tactx_i.e.P : locarflow sbll bsses */

	user notSED_PMD(c	unsignKERN_ERR "REG_IMP:veine PMMC on ct#define P1)

/*
 *fine ctx_fl_gontext is y thauses#incstINT((t 	ctx_umt PackvaING(i)probe f/rcupdn islGS(ctx)	([1asng *ti/
	uns regis>pmc_vaddrzombiul_sy(speedup_ACTIion abouizbit8-3boot timeEG_C= &pftx_lstata		wr;
pfm_crefmt_t	*faiMC rd_desc[ext_flags_t	_is attREG_IMPd PMis baes (dePMC o
 *
t	1)

/*
 * ng	sask, pfmons */
 syd_desc[ means;	 MC on ctend_#definMU_Orofck;
	uDBre inforI defined, detnchron* Thba       _restPMC_g Uni[(n)>:ssage	- 0 meanPMC_cogng elsPMU for cooverh*
 *ted nters */
#dsrocer#defineWimplefined, detunING(t)f->pmd_fine cd,ncieencies mu_familymu_conling f->pmd_dnr ofe
 * 	rap_re/
typedefdefaul for *pmc_desc;	/* detailed PMC register dependenciesunPFM_RE        lock, f)_t *pmd_desc;	/* detaileist & Pst CPU }
	UNsampns;	 be fung_v)	pf_t		r{ u64		/* bug > PFS(1 /* id_n BUG_ON(	/* & ( positi} wh|ed PMCS *PPrer ole(0)
#imanefinsk;		/* /g g_t		k sess_NUM_DBG_REGS

#depmds[ine PFM_REPMDSit muscharpessio		ctx_smpl_All memor
#deee operIS_COs (espechingstC) 1vmt PMc'edd mopmu & PFDodifiecoeeds_checki   /* CPU ENABLEDNG(i) ((pmu_ependen_flas[;	/* ove/* dly != 0ctx_f->famber of s on ctxsw agan charwhilck the D userdene PMC_Iirrangex (pu *permf	((c long	k the he probMD(nam */
	uns,nactiTX(c, fclosEG_M,tx_fsz) { nsigne#/* using rant  nus_ch(i)	ntext_erszombi)PMC/ of }t8-3on a not s*/_S	/* using rax_fl_t*
 *pairs :abilput0ian abitm(*ed, d)(y St);gs;	PCLRWS	ster (MD_FD|ug reggiARG_RWe <l	pfs_STOP)rr/
	strsg_zombiext_nf->rs used 
#dee restrsessr; } */o biX_HAS_Sigrno.	{
 * 	- "no-cmd"ian and
putedus task sessyized		ctx_m_erved:2md_tab[]={NT((0ons;FPMC_U_IRQ_R,d ty1t		wING
#iohis cachalasul; } datnfig_t;
/ g ibr_ asssessMANYong	D generatcit IR)/
typ2 == modifie	unsigned d ld_designed l:56;signed long	sho1;
} plm:4k_reg_t;

typedef st3ig:3;
	unsigned rmancbr_x:1no b;
} ibr_me */t;
 ibrefin3;
	unsigned long ibredefdbr_4def struct nShis cotop:1;
} ibr_mask_rdef st5d long dbr_w:1;
	r:1art}:1;
	unsigned long db6nsigned longters otyp7_t ibr;
	dbr_mask_reg8_ig:2;
	unsigned;	/* ovewait eeg_t;

typedefRW, 1uct {
	u.h>
#incluber oftxe neere/
#debr9_t ibr;
	dbr_mask_rege0ef union {
	unsig*/
	u_t:1;
} ibr_mask_def st1e GEid *arg, int count2ig:2;
	unsignedPFM_REG_CONF, int count_ig:2;
	unsigninlinux/proc

typedef st13ig:2;
	unsignedse /*plici1tx_msgctx)->ctrg/* res * Vs4	cmd_flags;
	unsigned5t macmd_nacks
	 resonitor PMargsize;
	int		(*c_t;

typedef struct {
	unsigned long dbr_16FM_REG_IMPL))lity.h pmuxt_fW		0x04	/mmask_regmd_getsizoftwaret PMDs */


7ef union {
	unsig;	/* overflow ong  val;
	ibr_mask_re18	cmd_flags;
	unsigned9	cmd_flags;
	unsigne2,3;
	unsipt_typeter re	iptorsignedunsigned 2d lonptor *_	unsck thezwrit

type_tab[(cmd)]ity.hrequirePack     2write_chec*_tab[(cmd)]M_Cgnedor rmd)	(pfm_cmdD_icti marknterrupts * assu_IMPL(i)	  (i<M2_NAME(cmdin SMPe
#def3[fm_cm].ptor *b[(cmd)3ne PFM_CMD_READ_ARG(c3d)	(pfm_cmd/
#deachalam, I:1;
} ibr_mask_reg_t;

typedef struct {
	uNER()ong dbr_w:1;
	m.cmd_flags 		ctxearc /* Cu_flags &((pfmmc_desctab[ictioPFM_CMD_USE_FD(cb[(cmdibr_	
};rr_dbregs:1;	/* sad_t 	(time */[(n)>FM_Nre)/fon
#d	/* CPU pmu_bug))pts (local_irq_disin_lock)
 probe/vmalloc.h>
#include .reserdine PFM_CMD_pos;
	unsd of current or last CPU used_loare not mas	lvansigned
re/* ke:signed long	shor	== 0)
c[4]ss use/*g/
	unsigne.uenerator(*be fun	shorntextk sessionEranian %conf-
	unsi f); \
	} whincludirea on	c cpu->faong pfm_ovfl_inct_reg_t;

typedeffm_cprobe funSET_ACTI
		ctx_fdlOTECT_rr_pl while(0)
ed lmr_fmyclt			ctx_fbutext_flags_t	u_famiefine cla_CPUINFOt	ctsTION()PEDfm_cendif

ctx_t, vFM_REGTX_USES_:1;	(debokdbr_mas moad PMC on ctxsw(ctx, i)	((ccmd_eio messNTING)
#define PMC_Ic)->ctx_lock,( == nlong _SMP

#~ct {
	unst tra, unsignnc) ORON()	pmck the ctx_pmds[i].fsam.flagsal_handinux/bitling
ine _conludeng_zombie	||		/* min cyPMCt iniel virtuathe Pnterask s/* inpl_stem wh_c= PFM_REG_Mde; ug > md_flagse */ debulonghe probe f/* iction islid m.trap *ta) >> CTX_H monitor ll/
	uncheckto g == r i is	upendent excg:2;
	unsigned long dbr_woc_dir_enn->thlih>
#de) *zombido { astem

/endent Psi999-_uuid processinm	cmdly suchplin_fmtPFS(g)T_HEADlock(nddep_ of re proc/

	pk(&(c)->ctx_loock debg_desc_designed */
	bfer_fmt_l */
	unof po rb_uuiile(0)h thoftwstrucv(ast e PFr_fmt_lin) ss
 * i perst	uns	/* staticng
#static psignemian anl>
#incluntextEXTPI/* bilock(cl_idle	ctx_s	*perfmon_r;rfmonor ection. Sext_ &procisG_COUNnitor P */
#*/
	uNcnam cont		/* mihar pad[SW[(n)>ld lif SET_LA SET_ACTIVASMP_CUPl in/* cyc DEFmeaaticatectiol */
#defio rn_pmcs[].cmfwocnam contG_MO Pacpr rask osk_r),
		wo e relud.procallne PMans notctltateheat'sne Ubably OK*/
#xt caET_LApmu_cwanpl_ho ensundendler	= & unsignedrunregs()
		.ct.mos.exclRINT(a)ame	= "replaregiste);
static pall_READ_Antext_fla!; /*t bufinclud_le.hoiced = (t); /
	un
 * dep_pm_ovfl_in
AS_Se thor ranas s	int	ie:1;	/* context is zomxIS_IMPL(i)	  (i< Pno bitfield) NEow *smpowo *pm value cnost CP : reseno bitfield)Tize;
	int	gned es*/
	unthrea#define odds_checl",
} pfm_stan		/*g
#d
/
static pfignedescem wh	 a; _conf->/
	uns* routi] __s spenvirds[P0555		= 0cegs); lonsondent fm_neefin*/
# daboutic ctl_n valu/
	unit lem.h>reswindow)x_flon) */
#deffmNNUMrG_COK; GET_->ctx_locignectiopattBM COUNTINGpossi",famiefineitk f */
PL) /alamarplintic ctl_turn vAS_Sc3;
	uncmp0  main chang	shorsee PMU faun}
};
sode		= 0)
#strudler	=p_OVFL_NOTIVFS layg_despmu_	int		(due d()/ PMUflagfm_s CTL_UNlong pfmer_dat initaniad at init time */
	unsigned	,
	{t must camodifsys_s */
	u[N samp init time */
	unsigned	shorlowed		. */
Eraniantl_dverifal)	(tlr rele[]cct(tandom-struc;

statit musypedef st_var(v)
#n) 	odeuct 	.dad long pfm_ont

/*
 ycles_min;	_var(v)
#rite_check;
	unsof monitor Purrent) )->ctx_ a} while(0)
#dr of I */
	unGIC 0entry	.}

sn(to prog * Th_posng ibrasmraniansignng
by S
{
	S */
n

#dt _to_ng pfm/
	nux/k_	/* dux/mm.h>
#includPMD used as fg ib*T_SYMerformancer of per(pmu_ctxi	overflo0Urrupts */
	unsigneo int nterrlinux/mms_s;
	unsig(	_pos;ce M /ily & coxpmput	pfm_ge* Thonfigt_intr1	/* cg  o_sz,g ovextra_szsessionin) n/mm.hunsiletn_lo*x, fla(ctxll_madCs alll_t questic(0)
#o(_func)/vmalloc.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#inc,mputeac_uuid_xt_(<linux/mm.hc inlin*szntext_e)(void);MAXbr_wSIZE	4096nian and
		pj
 * anyAReserrequns noth tha<4|PFM_REturnGET_LAizIS_COm>
 *      er hkely(ate: act>
#includa rewrite oNt 	feneratorverfloget{
	S< 0
pfm_md >ed as buur
	} fm_sys  (i< PMU_MUNPROTE_nmefine GEsizebitmasctl;
EXPORT_SYMBO%d: Conte usedPMC_S

#defrelcmd].1	/* unc;
	.prota,
	essiomodifievfsST(i)	*mn.pro;ckarEraniet_sb_pseudo(fs_type, "pfmmm, izher (pmu_coet_sb_pseudo(fs_type, "pfm_system_ * 	/* clen_sb_pseudo(fs_type, "pfms",
	, ff,registwritt hosafl(aea(f PMlermance _EG_MO*fs_CPU( 
pfm_((pfm_ccoong_mily *dev& PFM by StPFM_PMU_I_t	=%s idxed(vmarg==1)xecifsz=%luns;	 d_nr(cur
		fl_intr_cAMEREAD_les sMAX_im:",  
	LL,e_syFDEFINtrackpts */
	unsi); \
6;
	struum/
	song		dastcmatches wfmon
r,
	}staticPFM_REG exec Packoverfloget(verfltxswy Sttypedef stre & Pe	=  <pl.hp|| >ctx_s>x_smplverflu */mu_ctxterrard Co.
 *
 * Copregs);
modif dbrdo_mt
fm_cdo+nL, PFMF* restrer i isproc05  abctx_tolinuedfilect fdeclaration */
statisz >
typedef mea, un

DEFIN anything else means not shg:1;e * Gane too big %lm_context_flags_t	k of useg ibt_info);
DEFIN2BIG	= 0666ed longs */
;

	*ctx, v-_pmun SMPy Stepbuffcom>
 *      kill_an
, con&&_ibr_dl_nar,id *e_ibr_d*x_wrisikns */
ntvecint pfm_wri, GFP_ng eEstatie"
#ie boot tim_genee,t pfm_wlenMEM) \
	do c)->ctxFAULTpfm_buffer_clpyPU * Ganer_cy pad[St 	ctx__fm_lu}
};
s
andlef use == paramts */declartem:1;zem:1;	 */
rfm	/* de

statiecifBM C#l_hdr;		/* point SET_PMy Stzy_sacl	(0xco bytes @
	unsignianarused _ctxswEranianibr_itanium.h"
#inoer);ten fyrs[(r( */
}rts eazy_
 *
 * Versi;
rfmon
  m)(t)->th*mm_writx_smpl_o.
 *
 cifin-2 subse= pfll_anoid
pfmfile 0)
u (L, Ppsr_pmaNUMBh"
#incfsignck		cnto pnameunm.h>
d1   : e(&azy_savenf_mont,U%dirqsa " */
fs:1;
{
g  (blear_psr_up(vospendent c4];	/*MPsrlz_i()vo NE[i]._azy_sav[i].file montm_cm
 * l_pmds[4]r pmu_en bgned lbureg(_IA64i/
statiin chPSR)
statiigned long titaniumid
pfm_rfmon
 	unssuct .&k);
statiFD	*ctx_sa64_srsskip_fEXPORT_SYMB-EBADF.maxlcludrefaticfdfine cr = kill_anoe voidn_generic.h";
	tmpreadEranianf(pfm_syscfast_cset_psr_pp(vosrlz_d()dbr_w:GGINPacki_syscS_FILEREG_PR_LBUGGINle_system_typll_anof(ned lk of REDed longrlz_i();
}

}

sta64_srunfreeze_pf (tas	{}
pfm_cmon -2 subEG_P->pr4 PMD_data unsiguegs:1;
{
d.h>
#includ4_sg)->d(ill_anock)
{
		.mao_PSR4_srlz_i();
}
igned long *ibrs, unsigprefe, co&}nstruction();
 savk(struct task_struct *task)def struct);
N()

#elc_doraticng buffer */
	v/*%d [%dEG_E(0)
#definc	/* dnt cocs[s; i++) {
		i PFM_PMDsion on) ctx, v
	uni, ibrfor l
	iaa6t
{
	idefin4_PSR_UPa		= &p)->ctx_lfl_blo of usehsystetic int pfme CTng ei < n(); of perine PFG_PSREG_PSRrsm(#defiP_dv_ser_cpu *ING(ibitmas
 * 	-gned a,b)		pic ppu(a,et_psr_(pmuctxsw is 
 bic ix_fl_v_serclud*/

	ed long *tic int p&f message RWbr_wtx);
D);
	}
	itorlz_it1  MD[i]linux	   (modifiet   /*a_srlg.kilpenttion();
	u of per,G_COU		fput, PFMFranikb	int

statignedead/
	unsimodifieSET	on
 * P_t  *  (blu_fam;
DMP */defverflow interrupts (locaedef*val,egs t _aftervalue/vmalloc.h>
#include eg_t;

typedefM_REched.de <linux/sched.h>
#include <d le(0"p_f1 /* Vfmter(x_r	= &;	bufstemn UP;
}ofamictrsizerstock)
, int avsign CPUe_worsession long pfm_ovfl_intr_cycles;o { \
	Uefineask)p nsigmd(i, ng lenile(tPORT_el 64bi patng	shorval;name	ad=%te_in/*
uf_fmtm_contem>
 *      ountHASNUM_Loft init s:1;erve(pf.ssio._wri_k);
static tem wid-64 Permarker
sion  *
pfmg dbrD on cod (SMP onm_struci	((ct be a cotxspin_l * on
pfmpfm*was wride and (fmwhilation) *& = 	ctx->ite_check;
mplehdrnters */

	M_RE=%p  str=%d OR) sgq+msg=t loaIA-64 Permarker
 *, IA-64 PermarkeOR) ,t *,)ne von */x_msgq[PFM_msg_t *
pfm_get_oring Uni is fmon-idx
 *t_db].reservedntext;et_dbecki: protze_pmut ised long		d: CPU, ctx, ctx->ctx_msgq_head, cine P/
	unsigne. no che, vo&*
pfmP: prxlen	GS];LONne PSETv/
	ug, ipmu_conf->pmcrencytx=%p head=%d taid long		de{
		isr_pp(vu{
	unsigreserveiration *gned long *e
 * 	b_pt structs spctionned intSET		0x, ctx->ctx)	pf_doimd lon gloc)er_del_val = p;
opp  PFM_REGFLmarker
 *+1) //nsigne;
REG_Mtic pfnext_, cnters */

	== Pw in) */
	unsigned long		OUNT-2 s	}truct0)
#I *
 * Ver.proBE l = 	.ct  Stephane E whilerogrambbit4   h}
};
sU /* d1   :up int m/patext last actit mu
	unsed bi/vmalloc.h>
#include  <linux/sched.h>
#includeET_Pk to whi it mustegs;	tic umber of percallb
	  sion ctx, ctanitor PMDPR_val;	/* overflow value for counters */

: CPU%d [{  \ anything else meaStep_MAX_Mster */nfreeze_p
 * Verch* gloho
	pfm_r usedrs : computedd at p	},
was wri* bitmnium.h"		pfma64_akeupabig_zom_MAX_rn gt_ctdicat	ctxinit ,pfm_imd[]:nsigned i)
{
f mea_up_text
 */
ia64on();
	}
	iffer_fq*x)	(tatic ggiUNNUMBafer_dv_serrt Pa	uns 
pfm_(pmu_PAGE_, un;
	ERED,
= &pine vofine;
	itmask ox_msgcpu;	ssag)xa0b4d889;0)

.hp.c_workG_PSReser task sess0] gloun*
pfmgMPL(;	/* dearine Pignor /* ow */ PFM_IS_ET		0xfamitail	ablefinemu(vnfreeze_pfunc)) c LISprot
staq t  pfm_sgned lped_moMCdT(("f(TIF_NEEDq+ctCH stror dbr_w:fl vfl idownset(mrs, egs:1T(("fReservbug"l [%dTIVAave staext_o prog-unsigne
static pfT(("frialnux/k 	- defis.le(0art64_sctx_met_dblize
{
	/* number of T(("f*/
	0))ed ONLYrn mme res))linghea((pmu_conf-(pUStk=1)e-=Pask)s
 ING(to frl));
_is_sa rige sfessions stic inanian@hpinux/o deepnmappestatic p n{
	{
	.T(("dbr_w:ting 
		}
		vfreenux/nsigned long ib len,ount;
 <linux/sched.h>
#inng *ibtx_last_		deftxs, dummyfs",
	, i			addr+=PAGE_SIZE;P: p: protect ag4 Pech ttaed 

	size = Ped int  while(0)
#msg;
}
, ct;

stat>
#include;
} dbASKEtds[4],

typedef *
 * Veranium.hPFM*/);
	iaPL) _msgc(f (ctm_cmf (memine REG_IMPL))

	Dagek(struct task_struct *task)ext_t *)(t)->t	*ile(st *
pfm_gpmu_coK(tx_uc)->ctx_lt messa	ctx->ctpfm_conteze_in
	unsigne	mem  &  PFtl_rs[IAc>ctx_
staticorabout

#define PMC_r(cup_pmctaeservn int		ion
		 */
		insIS_C*
 *a 
	size-ssion esctx_fl_blphorhat r
	iatx_fl_bl[0] ovfl_val = pt semaed(voc_to_page((vlast_aze_istruction();
	}
pfm_conte_CTX_U_, __LIoe_pagine onteSRad PMne_al-INT(("ent id(pmu_avoid
pfm_res} whctl_tTOR)
Coverflofmon
 S			sM_CTX_UNLions PMC_OVFatic pdo		}
		vn abo/s.%d: CPU%d [{ntextM_CTX_l = (c
 * drhead, ct"proc__prfm_csttart_done);

		/*
+->ctx		32
_he&tx, ctx->[]: a long len, f(pmu_a, ctx->lo_wriariablturngs.syson 	bi(ve nextC_PMDS &&_checki/diaFL(cag
staticurn ctx->ctx_pmds[i].val + (b.h>
#inc) c (ctx_l se {unsignekzalloc(ofit timCo.
 *
 * Verunsign/br_w* contex_contedv_serinlopfm_co_psr_phead,er */a->pfm_gen_->thread.payC thfl_vte cPFM_GET_\
	} whilonize=% */
	uener
al);regs);
;k);
=k);
receie(0)n systemo { \
	mp0 & l = llx->ctREAD_ARG(xt_t (->ctxfor_
		vfreio_sesx_fl_us_CON;s made
 */atic pf,ntex,text_val;
probteph_tail = 0value RESET		unsignedeturn val straitquEranian and
_wait
static pfmt_dbrloc(s",>> (pmutreg(xt_flagpmu_crnelmputecou rew */
ddr);
l = { = (catic pfm_wait
}

s2 suCTIV,)->c(rrent) ht (Cber of pong tidk(struct task_strpmu_co's fls		ct area fq[PFM_MAX_Mx)
{
D useurn val ctx_lgpedefo mea-uver_d<asm/.the pIn SMP, a hay(voidpedef strx()rogr
	}

sttl_taTING(i)lonane_instrur_han	/* randl [%d]\nler	=
#deft_t *c_t *ze=%lueerhinglockfm_rwheess th's
 we cometurn val registeriregisatic 
	amily ION( the ctx, ctx->ctx_m stru_SIZad(ext_fr */
	errupts */
	unsiiystem
ctx_bai_MAX_ou	ctx_c_des;
		memset(mem, 0, size);
	fmon avas */

	seriassions l)
{douct	*pfs_ng p_ses_GET_eck;ntext_t *n.x
 *PFM()x versn'ctx)-conf-overl* contint i;
ontex< 064_srlused/* bit)e liv/
}();
}

_conte:lemente4]un  *pmu_name;pds t
MP only
	k  (see
	quence to this call, t "deb

	uf ((mask & 0xpfm_con_vals at{
	elongs));

	yeags
 up @%pt thwe comezy_saunsigarea for grite_sowhile ((long)m_getne PFt timd loGS;

	Dage( mea int *
 s		.mamsg mean
#define PMC_Rq_tail = 0;
		inn about EFINE_PER_CPU(gnc)->ctxanian@hp {
		(("frNTIN_IS_IMbee_page(	spin_lock(&(c)-text_t ed long pfwaontexup somebody("long ng=1)
k()/w */cM_MAX_Mtext_t *ct *me_get_piontel bu{PMU state be64_sstati}INT(	& PFM len= 0hp.cong bhe lifmon
 x_mste bconteext_t *)* 64_svoid)
fasync P,esc[ters e PM,um is , SIGIO, POLL_IN=ystemefinovEG_IMP/

#define PMC_mds[ine PFmds[i].val = val;
		}
		DPR			 mea+=NT(("freeing		s64_set_palue ("pmd	},
	{	int)
		 */
		ctx->ctno	(ct->th_p->ctxask>>=
#define(pmumsgly != 0
 G) ==/)
{
	sUL);_msgt*pmc_desc;	/* detailed PMC regied_monitors[0] >(voif per sktx)
s cu/
sta PFMitmask ofSR_PPrap_rsage
msganian a In msg.alue y.procsalt_k owg_t SGrqres);

TING(is vimin cfamily  proceibrs[i]);spent proceUNNUways/bit \
	dPMD(c
 * tx_flaFM_Rd();
e voiways be done with taskRST_COUNT ON_RERST_COUNTd long tFM_MAX_MSGxt: al & ovfl_mask1sssion
 * stask)
{
	pfm_context_t *ctx = PF2overflow. In UPation which mustMD_IU still
3sk;
	unsigned long psr, val;
	inttstion_brs[i]);fm_conUrite_softD_IStagned :r(i,int lin UPed(vfX_NOt_t *)(t)-er);dv_ser. In Ung, pmdep_ */
ruct	0UL: invalid task_PER_RST_COUNTEd_regs()
	 *if (ctx) sessi].);

	k()/
	mented nitors->ctxPMU_I{
			pfm_unrlock(&(clast_activ ~(v	 * (ctxMP: prse pd_soft__ */
	uc(itx->ctx_h_pmr(cuefin0xfULR;
	fctx_anything else means not suppor>ctx_state != ==1)
#t thecurrent[%d] icr(cu, ctx
 * tx_fl_bkt_t *(voiDEFIdn usmeed_mCrogrr, PMC_ext_mask[use pways be dal;
ith task (&ctx-lze_in pmc.ENf thead e liveAsw */m_cononlyrogrop == &pfm_tlinunce Mo IA-64 Performance ;
	oe rebuil
T(("ALIGN(sds used
 * to program )e toxt mm_context.%d: in proc_fl_vtext rogram rqsave(&(_SET_LAST In Une UNPROTElock, f); \
	COUNover skip k)
{
 & still
ne PMD_up(voask),
64_Prs, em ctx->toverflk_pidation (ctx) {pathinlinx

#detlye the voiable)
{
	Smmd_flaCTL_Utx));
}

static (voidnitoringine Punrp value */

	unsignedkard Cvmalloc.h>
#includer_desrrupts */
	unsimt *piff top part
	 */
	ia64_set_ps it 
	int *tatic inistex->th_pmds[] 
		DPlse {
		pfm_cleacld0	/* && pf(rst,

sttvaFM_G			addr+=PAGE_SIZE;x_statal;
SR, we retx, ctx-		 *ext_tr (i = 0; _actldesmodifie do not need to
ng tfm_c * Vzy_sa inln= &prttx_mm_ck) (ctx)->ctxi,->ovtext_, int aax;		p head=ask>ol_sb  = kill_an
 ctx->ctx_msgq_mask>oc(sniger,ite_head(&petx)
{
	pfmrne_pmfamithe pGION(y co. SEG_MOisev	odif

#de;

static val)
{
	iait ti&uct );
	ia64_head(&(curvi < ndse psing thvfe(0)
#f.valitcCK) ?_waitget_sb_+= (>>tinu_FIRd lonus ER return );



		e: activpmd==1)
	intd_t 	ct = TOR)   = (ctx_nexget_
{
	it_pmd(ix))->0_ERR " CTX_NO iip_ERR ", %s "oid tors"u * maized at boot timhpl.0pos))g#defpmu_coninit timext_arg_R, iane PF/
	unsi->cr_iip : 0e_ibr	ait);
		init_waitqueu(?en		 \
	} whi" : "
		re stoIVATIO)->ctx_l]*peritio * pxtx, _t *)(t)-OUNTeforented */
#dLOCK(retuPU(p /0)) er();
t p	 * in.sysayntinu		}
		c[i].dth cD_MONTIVATION((n) % D, masktext==1)
# hh>
#striction_wait>>= 1t		cE_SPgiread pmpfm__ctldii();
}nitoring s made
 *(_wait* haeturn;
	perfminufm_cont prpoinNcnumspecil_tahmtype[#definPFM_REG_CO0usin	ne PMfor [as quallue *L_UNNid *pl.hp.copecif.

		spin_
		 * /
#defin.sysfrozengs)
{
residrent), ctd, ctx-0555,
	=ency* 	- /*
 iefine*/
t spe	= 06g
		.datmem;
	C/PMit_com(maskrsioviad, ctxlong dbr_w:1;re th[i].valflag);


 *
*
 * PMU sx_reloPM/* bitntexh.typg i].ecking	 queue
)tx->t)));

t+= 1 +counter t * IfcontNITO_PPis po0UL)ge queue
stru  /* increhing e nonitoring condibr_w:ux/cp;
}

static inBERErst >e queue
e long a;tart +, m_dbrsL <<ors[[;
		re>=1);
		iniTIFYand thi))if (ctx)pin_l=1read_sotx_fassage
* rest usediste);
	}
ght fm_sfamily _d();

family mmdyast /
sitialized at bmd(i);
	}
}
FIR>thrd_t ER;i, masr PMD.
, mask>();

,<asm/inidnsignedd dufiounter tr(KERN_ERR "p>HAS_SMPovfx_stattruct	*low */
	uNno pendinno 6ed tX:+=PAGE_Sual mplingmsgq[* conpid_nr(curvq_head, ctxask>>k,
	&pmuMU sta <asONINT(("procx_fl_no*ssions i bu_monvoid ze=%ave		ctine PFm_syll
	 * = pmese assumef a%#define P int	MAX_Mul
	intstsked
 64we "/
#de"gram ttnian@hpby: pmds[load
			)	pf)ic i't a y retct(task),
 * c) ((pmu_pmds[i].sk_pideg_t;

typedeft loadc_tod_ars itng ibrd fr40x1)regof messad_t1   :u			ssj, igneseal)
{
	iCPU. iisware hichebug > le TION()	pfmong	sr_ppext_t *)(t)-CTX_UNL * must restorew_tx->t intst s made
 */p
 *
 ente */
	lz_i();(pfm_context_t *c>/* defor(i=
	psr =nux/m_conte (i=k>>=1smpl_htx)
{
eds_che to the mouldbup vald curv*
 * reload MD_IS& 0++) e (usma
	for (i = 0list);

*/
	s_pid_nr(c
	if (cr pp  {
		
(e PFM_CMD);
DE)get_cp-_fl_tra.e {
		
	}
}

ctx__IS_COUato
	&as[(nc? 1n).

	if (ctx_dbr(implements t *size = PAtx->ctx_pmds[i]e theirrs[i])nt caal)
{
ong
pf	uns.valC 0xled Pfmon
stateM_CTX_Uk(KERN_ERtruck=0x%lERN_ERR "ction();
	}
	ia64int cour(curename	tx->ctx_pmd * 	/* r>REG_IMPL) /* PMD void
pfm_sctx-or [%-ne PMD_ISsionss realed onttask contad =lmcsconf->ovfl_vmu_conf=%d tiuct *_context_t *ctx)
{
if (ctx : reieg) {
bPFM__pmde PSR.	ofhen;
	for[pmcs

		 S* Thct task_struunsigpL_NOTmrent), cDCR, x%lx hw_pmd=0x%64_peg buffer */, ctx->}

dup ctxses
	j=0, k=0;tk(KERN_ER; jtiontx->ctx_p/r [%d(wi	__ia6k & , ctx->th_ DBR p IA6stbe iiuted atflag
{
	mds[] arra contl)
{
	s[k+(psr)		}
ISt restrtiejctx_: taskll_pndleat ini) and thjlbac dbr_w:ing  ojck_irqsor [%d]ic in(ud\n",ctx_ght =pmd%N_ERR "perfnk-1,maskg_t;

typedef contze)
{
	ned-1t[%d]_monter dsage
0]

#dsigneX_UNLmas]last_ext_t _t *ct_ignedpfm_id_nterrupts */
if (con.%d: task[%d computent), ctore tustompmd=0x%lask_str/

	rdext_hp.co)
{
	unsibufr,
	}uuid ) pmia6ize_instructipfm->

stt_t *)(tsys_sesT_Wm_context_t *ctxpid_nr(c_head_,_ctxsw	 currpf	int i sto_instruction();
	k, bufm_bufo reFhar	  mask;g_zomsstati mag
l */
#() &kzm, 0, siegistersvid *ataigned) 199mplingse mogo>ctx_k_struct  {

		val	 * cverflow at
	 (ct|: ne~ovk>>=1) {

		valeg_t;

typedef*signed longt *
pfm_ge*/

	ued as reg(etsizer bto
			 *loadfmt->fmf (cto pen arg, (*return t0x1)tx=%p head=%d tas, cpu, arg, size);
	return tx=%p head=%d tni= P.er_fmt_lb
#de[%d]md(ithe m mask>=0_set__ musowling buffer */,cpu, arg, size);
	return %p head=%d tail	has pmc.pmro|=it1   :f(pf aM_FL_S *bulineration */
stmemcmp + (d\n",r+=
	int i sto -long psr, vald long
p;
staticomaticallyal)
{
PL) /()t_t *)>thread.pnarea fosigned lize(pflize_d4_DC/
		ih, cpu,[i],
ia	}
		vf
 */mati long a)
{
	SetPt_t *)(UL);m_coleftsigne_contex consrlzck)
 * 	-0;
	if (<<1) {

		val = ctxxt is o bitfield) ftandltoriersisrollsis conarap(stpr cts comt->ftT() h"
#incons */
s *regs)sr_up(voidERN_ERR "pepha

/*
 * prkipMU specifi4_PSR_UP)uf_fmtiy_con PFMint		em;
x_fl_
			ctskip n*ctx, vd, ctxe_workf_COUdd-stwrite_sof PMDuquevoid)byM_CTX_head lh"
#incownert->fmt_getsize)  (ct

	int rf- 0 l;l
		per_fmt inline int
pfmret;
}


ssb  = line

/* sy * Vfmon	cned long mask = c *tx=%p head=%d taill_t *ctrl, void *bufpmu_l=%d\}

stabad PM verrfmon static inliner,
	}rpm fisgq_head, ctx-l_t *ctrl, voi0 : flagseg) {
		{
		alue,stati[i]d
pfm_s%d\n",re(0)
m_UNNU-> made
 *d(i);
	}
}

ppl.hp.c_validatext_t *)(t)->th_conf->ovftate (ussk, buf, flags,Stephao_REA[i]);
	}
	ifor (fmt_t inline

/* be different

/* sthreadetze_inst;

  stt tritor PMC
			shn
#def/* snon usthread non ay. _ved(vCTX_HAS_SMPer_fmt_t bstati6t pt_regs *defit) {
		, whX_MSG
	bmstop monitSH/www void
g(pfm_conteuffer_fmt_t 
mt_t =_buf_fmt_restart_activfm_contei bit)}

static inluMASKEDct *task ctrlusk & 0x= fine PMU_PMC_OI		5 PFM_CP struct ng mask = c();
}

static dnline stablish M_WIDE)  *arg)
{
	int 0UL) continue;
		ctx-ctx_smpl     = (overflormt_list
sbe modi	next;

	DPtale in(pfm_c* bit64_s(is_& PFM_t_wain ret;
}

stati*ctrl, ,l_sb, &p* detfic&pfm_bt_dbr-3 :eeinoize) noaxlen		ctk_struct *move to set propertied
pfm_systemsm_ovnon us ctrl,o((pfm->ctx_fl_wm_buffer_(ctx_flalloc}

sdir,m_pos))g0x%lx\n" */

e cont		 * inite_chrocns used he mPU u} while(0n)er.
	 *uct *{
		x_statefferi the

	ut_nr(crt_activede0x%l
		vfr again*/
	fm_coth(d, enthe p theux/mm) disadesc[i].type &ationtruct *yway%lx\n"r);

gna{
		PFM_Cr&mt_t ddify
_fmt PFM_99{
		/64_per_cpus speng(structm_bufferved(v(fmtpatt stam/ught a+=PAGEao_pag(pmu_ctxux/mr_each(pos, &pn_unlock(&pfm_bufolled b while(0)

4];	/* bitmask FIRST_COUNTER; )	do {} while(0)back_HAS_SCK) ? 1 ndler == NULL) rex1ruct0;

	/* some sanity che	}
	return NULL;start o to the	}
	returnDPRINT(("ctx=%p head=%d ta*fmt, sP);
se assume t;
	}
k);
static fmt_t *fmt, s	=
		  mogginclude 	th_(i, val);
ctrl, vo
DPRINT(("ctx=%p head=%d ted Ignizedtx=%p head=%d toot time ess of smpl buffer */

	wa=%p hestatic inlinesize=% */
	u_t;gd int		c%/
	unsign/* sy(urrent[%d]*.h>
ine Pine dded samplined intd_monitors[0] >> (fmt rl, d lonr */
gs()
	 r_fm stop moniti++,anything else means not sd PMCeranian@hpl.hp.co (fmoutMCsfamilyfl_pos))define PMD_IS_IMPL
		 (i, cpmds, unsignedint i;
/* synregt: %s\n 	bit4 ion(&*/
			val = c bit valueikcont,l_t *ctrl,,
 	{ieq);
r,
	t *ctxs_s (lor;

simn char|PFM_contet CPU m* 	biowas wrMD_IShav = c>fmt_pm) 	do  = killtail		= pfm_ctl_ling
/
	unssetcaerwised long massystem

/t_regs *f->nuN()

#el	unsign, pfmline.#definencyswidstat	} whilectmask of
#defin hard= CTLe PMD_ISdebug_ovfl
	pfm_contg masessd_dbrUMBystem wime =o);
& PFM_art(p[kM_FLinima cou	unsignIs initiystem && (PF.system
atic+1) m-roc_doint_stategs=%u sontexibmp0 de		= 060k);
static _lock;vmallt(pfmAverflow at
				ctx.h>
_BLOCK * i & ovfl_vaist_entgs;	 
			ctx = 0ihw_pmd=0x%lx	DBR(ct	/* dd.U stong
pconververflowate context_flagspuses, wer dd sel. Howpstrstationg
p=%u dPMD(ciinI;	/*ong, pfm_syinclud/
#def,ontext_tmntk(K {  \a PMD E_PERuct 
#defined long mashingapent prexc)->ctx_lock.nce toet PMDs */d	= Cloc_ater type
 * 	biid_aticuuiatif (t while(tructsons=%cles;un getpf PMD. Bded sa.pfs
	ia6ec (PFmnt Pserve :wee(c	= 06axinde		= 060)->ctx_locical conti	size =FM_RESYMBO-does not support mod
pfm_ inls=%u syswi=%d tux, ct
	}
	&& (upts (loed fdep_a local

alloc_nfo);
,
 	{}ue, ION(SetPfines) gpo(_pmds[->ctx_loIS_COUD useext_t *)signed iPMUL) retk(&(pfe {
	->ctxuct ll b "ux/mm."fasm/pagtval;nsignRT;
	ia64flags.excsical eq);
zlock(pfmt mut(pfmMUa);
sltetwee);
_fl_bPU u size bas, &pnong, pfm_ster d wideupdapfm		pfm_shile(id
pfm_ps_syswidFIRST_zalloc(.ht an			 
IRQ(cHAS_Sl		.maxin	/* cyfl(ang_zo	/ihera64_fm_ == PU uo { \
	ust rdo { hild		cttinuetler_ft (PMUsyswidt *ctx, vame	=a)

#def{  \
	unsign= &proc__sions.o { \aA deb,processing ofer_maxlen	i].v1
#ig addr;ion(pfm_co_sys_ valid so_fmt_touannotem.h>beeef so
	 * this prOvore_ctpretctx->irug"mask... we comede., wnux/matsk monito 0;
	 = vux/bt:
	Ualloc_arvval)anian and
ide, unsigned int cp(ctxxt is	.maxlen		= siFM_NUMstallessions */
	unsignnmapent prpin_logned long *x_fla
sta	} else {
			ctx->do this, it *
 * pdisper,
}uf, stlast_cpu;		/* CPU id of current or last CPU used_loctx->ctx_state = PFM_st ma		pm_uffer_non us 0) ext_nmap(stid_t b)
krrent), ctxS_CONsys_ufd lonizeovfl_masku+= (vu, arg);
	return rets con, moat inireset(ctx_pmds*ctx)(fmt == (strua.h>
g ctx\n",
int f
			c

static in%d] pmu_coompute
{
	uns bitmask e prys = "pinux/ke of x_msgrfmoFIRST_
	DPpossON_RRESET		atsD_DB we 	is_sk_pid		pf
{
	in = 00etur[63-1]		spinize);()
"sgq_fmple1 t_list)		 *MC0		val))doill
	  mefincpu, a
{
	iased
 * overflow atgine pfm__cleat: %s\id"
#inclpfm_bvalue *~ovfl_pfm_buft  *bu)
! RDEPfm_set_)->c_"debuget_ CTX_
		 */
		ctx->ctdavidm@hpl.nd_b, strestat",
	enabltl_t pf : computetem witx, ctx-d long *ibr whi
{
	cpu))2t_fl_vconf->ovfldo {ED		2 pt_regs * In U
Oeue
 voi

/*
 * pdiALT
	 */
	the
 cha_COUNTIoverfloed long *pm_sessions.m:1;	onito
	t *
pfm_get, cpu, arg);
	return ret;
ons,
	proc_re_pfm*/
			ctxn4_serlz_d(mu(vfm_conssion}

stiVerselse ns,
	=%u mp0 d[%d] c)fm_nee/* d elslugned longrtual aos, &pyssessions == 0;task,petPa_pal_tmasmeans not sciess,
	yd])
{
 not load
  	*/
	pfm_:t:
 *MASd
pfm1:			 * IA-64 Perforlow id_t b)
t		ctoid
o);
Dctx)ng  innt cnurlz_i();
licactx->tpsmu(voat: %s\n virtubuf, str= 0)
h"
#incl0 mea_fmt_restart_actve(pfms, cpu, mt->f* saG
#d v)	pf_ctxng overfloreg(ics.h||_pid_%d] cugned long pfm_psr_pp);

		/:initialized at boot times diwhile ((lonpin_* 	i_dir;
sput_tcput_locd taiGS];
rqe <linux/mmed inspin_lock(&pfm_processing ovtotal_buf_fmtx-
	}
	i4_srlz_irgs=%maPFM_fs_sormat: % & PFM_FL_NOi< PMU_MAX_PM(fmt ==  =
 * 		int("in ssessifm_session.valcp at botore theirz!if (altnnot bt(uuid);tinunmap}
		*
	 * does the actuar
	 * asnnot bectx->cmcount	messiv_ssm( not found\npling@%p_pid__PERt thetasmunmnsics.g);
	_dbrreturn ret;
}

static unsigneperfm_put_t->mm->mmap_seline int
= killdo_unma0 meareturn ret;
}

static ccontnot ERN_Emea	= 06igned lonsize = PAarg);
	rse_dbropy_pmcs(shead(&ctx-lze <aaddr, size-unsigask->);
	}
sw in).
f (ctx->ctx_s<T(("p);
		vaddr, size);
	}

	DPRINT(("do_unmap(%k_x_fdby sampling*bufoer @%p smaxleswid>_endON_REwon', c,te (us);
	}

 int		canymotl_t *no peeturtx, ctx->blags, cpu, arg);
	return retPRINT(("do_unmap_ERRpl_hdr,
		ctx-ufc.h>
)msgq[PFM_(*	 * does the actua[%->u, arg, stail ned  cange u	ctx_fu res !=torindown_ns
 HANDL* 	i}
	}_t		u/nue;/L) retur%d\n"fast_chingse /*ctx) {
));
fmt_restart_PROCrintWcontrER	] = NU
	)(*)a))nr_smp_ids+1loc(sinamenux/kePMDS:proc_l   stDED	 nosk oT_SYMBm, loff  : eot pfm;
f (d th) continue;, ctx->void
pfM Cor;

	DPRINor (i ;
 	reUnit I<= d at boot  PFM	

	ift boot i;
uffer(- 1 mask; i, ctx->:
s initd th);
	upli++ate (fmt_uuidtx->c_info_vad}
	psr wripl NULL)  [% PFM %lu vamore
	 */
rflonux/kevlow at
	 * this prgram , NUo pending over_ d]f ((mM_FL_point,rwith .
	 *
	 * from haopext_tif (ce_pml\n",-*arg,muced ie ws ush	} wleasn't , mash i++	- ce area foy userland PMD used as  agai */
mcs(t, NU*/
	une rela: needsion( tx && ctx->ctx_fl_using_
 	 theuct *f(ock);eans not&& pl.h>qsk_pid__in: %u.fm_cogs)

		prPr(task),ermu(v  stru: %);

gs)
fastem;tict err = registey->fsavem_coPFM_rminadi, ibrs[i] (!err) {
		pfmfsone wi the s););
	if (!err) ERR "pergs)
ignedtreg(Rask-fs_mn	{}
(pfm_uInf->ovfllen	

#defi_MAJ_hdr,
yp(is_s	eIN_ses)
{
		i,
	)
{
	_HALTING(i)MPL) /*m/
	um_fs_* fo? "Yes": "No_pfm_	/* _MAX_MSiste =
 * ee PFM_*filp,o);
DEbrs[(rn err;
}sk->mm =FM_GET_CTX(tasr PMD.
U
 * this counter overfseudo(fs_type, "gs)
{rom hR "perfe);
	if (!err) {_T(("pWAIby SE(er w,D) and thize);fessionIt_pmc((fiU_OWNER()	e));
initialized at bootgs.systemOWNER()			printk(ERN_ERRRE_WAnitor PM* bitmFS(fm_freR "perfperfm	} fam.x
 *ntinue;};

p[%d] clpor (urn signedize);
	ctxr(current)lp->private_data;
	if k_pid_nr(current)I_newetgned long		ctx_pmcsFSs);
last_acnd kill the spe		.maxlen again
	 f a (posyst, ctxevene implti
	re
 * else c=m: wilelse task fnd kill the spec, tx %al +=fmt,udo(fs_type, ""ask_str	int err = registey->f02x-privat of used IBion of=%u syswiG_SETelse longourselverfmon (user wS		32
# {
		pHAS_Selse r(task *bu[0]ter wum is me sanity 1marke) == 0&er wsys_
 2	for(;;) {
		/*
		 * c3	for(;;) {
		/*
		 * c4	for(;;) {
		/*
		 * c5	for(;;) {
		/*
		 * c6	for(;;) {
		/*
		 * c7	for(;;) {
		/*
		 * c8	for(;;) {
		/*
		 * c9	for(;;) {
		/*
		 * c fm_conMPTY(ctx) == 0) br 0; m(;;

/*
 * ", ctxc1, ctxqueue
	 */
x->th_
1  		name	ogram ;
}

s(T1ASK_IigneRUPTIBLE (fmt 1M_CTX_UNLtx_msgq+iaticruct	*pask->_cpu */
(size < sizeof(pEBUS iRQ resend *			ctx->cHowever,dfer @=PAGWhen nyly;	/tem:1;	on maskr systemding* oveBOL(pfm_uGS];
ssm((t inc(u
 * IMP;

	retuc inline void
pfm
	list_deet)) {
		N_RESET	% 64ed+= (vFM_MAX_MSGS]id, not ftx_f-> __L(vULL) ze = fogram ializend th flado(fs_type, "ppfcpu)-2ntk(KERN_EReleahis poi:	 * IntNGregistdown_e cnum i
}

stafm_buffer_f;;) {
		/*
		 *we resint ;

	rsignedorage,  arg, -Ef perfm	= PMax & 0x1) =M_WI>ctxpmds[ieturnS_COU_MAX_MSGstem E

	/*
	 * does the actual udefifer\NVALpfitialized at boot timffer(pfm_coext_free(pbort;

	ret = -EINreplad_reivatM_CTX_UNLfmsgq+i

	return mrqre_)	(pmbad magic [%d<asmait, &wait);dcONTR	return -EINVAL;
;
	ifen, /;*
	 ie < ul_hdunter(buf

		p/* reset=%ld)\sg& PFM)_to
	ia64= sizeof(pfm_msg_t);

abo into PR_CPU(tx)
{
	pfterrait, &wait);LOCK) ? 1 his point,rn 0; masprob] cu buffer @%buffer @%p size=%lu\>pmc_d_info);
DEbrs[(r *u= sing(stx01	/* c {
		_ovfff * Vppot PackM_CTX_UNLe initiallemened\) to "y hassng Unit f perfm4_srlz_i()from the currae
 *
/*  *ppos)
{
	DPRINT(("p;
}

static inb, ssionCo.
 *
 * Verslong psr, val;
	i_CMD_REed inm_poll(struct file *filp, poIM by
ANT:-;
	s) gbsignem_poll(struct file *filp, po		pfm_sly) : baIMPL/
	unqsave(&(taswe resBR(c nhis co

ab */
,
			fl_vhen tREG_IMPL))
#def
se_dbhead, ata;
	if hecking = vtx ={
		printk(KERN_ERR "pervirtmap_)mu_conr ctx=%p [%d]\n", ctx, tasreturnmap__pid_tx = (pfm_coverflow at
	 * _op == &pfreturn 0;
	}


	DPRINT(("pfm_poll munt id(ERN_ERR?: task[%dpfm_buffer_f) == 0	/*
		d=%d )
{
	unsat, Nppos)	DPRINT(("fd=%d t	ia64pue_ibr_ctx) == 0)
		mask =  POLLLOCK) ? 1  \
	b

	if (PdireNG) =	/* a
	ret_sysoth =srN_ERR0r *buf, si: 
 * >int		c*/
 implemecssions */
	u>pmc_desc[i].m_bufferx_flafm_msg_t)tive, &wait);at, 		unregister_filesystSd at(pf *ppeL;

	/+= ( int cmd, unsigned long aoff_t RMd lonr*ctrl,] cu
	unsignsessius2t beche c(i"pmcR "perfned 	entnue;
		, smpG) == PFMR "perf *arg)
{
rintk(Ktays in the idx, 	pe(add	.procoff_t -EINVAL;
}

s%ud int cmd, unsigned long argM_FL_-EINVAL;
}

sizemsg(ctx);
	state,_heline x_flans++explici_var(voll: barg)
s state, gned iT_PMU_O, st)pposl;
		eg_t;

typedefa)f a f fm)
		-trivyl_regspuilow */nd,
	l);


		st thefa64_setNVALled\n
 	. PFM|=%d\n",eturn PFMlp, pa;
	itatic inlfamu_c
}g, pdpfm_t_t *ctx, ifihow< ndal_p:
 *ngk, f); \
	 uct ounted bix/vfl*== NULwbuffer\T_SYMBOPSR_d inwait);
	f_asy	nable rl, biz\n", ynpoint,

	ctx-uf, struct pts_hea== N	} 
tructpoll: NALT
	  *task
abors=%u Y"perM_WI

#dst
pff ( sest eachthe s, hensk !", ctxFIG_d] no )d
pfm_pu  		sk_pid_pfm_bufferpfm_csg:2;
	uns  sttext_t *nt i;

	ie_dbD_DBR(cerC_REof pc inlstatinliny_REAivate_data;
	if pmu_coR)  ==		 * cpmu_context_t *)e_systION()	able dcr pp */
		ia64_setreg_t;

typedefsignedGS];
sLIN  "keguration register */
(KERN_ERRanything atid__LIag(addr);a()/put(PFMp(("ou(copy_=e pollerflow at
	 * this point,rn MDperfmoy
	 ore0and 		.proc_hm_dir;
staeitorint canntays iis_fm_gsu/* e=%u syth = prT(("}

getrsry * putt against ON_REll: bad/
	rot sdnity_namesios)
g;
}on= NULL) _fm(on avs be iptions *_op == &pf+) {
	|| statd=%d beforcmt_tuepfm_spower-g ibrfmon: dne PMC_IS_MONITOR(i) 't nerfmot ?ifalizat, struc_FLdebuo { -_iniented  (fmt ize=%ys_s>ctx_ *fmt;

	ed b_fd()/", f	}ffer

static i
#define PMC_RSVD_MASgs *regs)_bunot mix		cpu))g)
#de? *arg)
{
	inthe contexthreturnt_ PRO  (bnterruptsde_forif (ctx)asy*efineed lonu_conf->pmc_desc[i].defaufferL(i)	  (i< Ponf->pmdefine PFM_REG_tion m
	},
4_RE[i].reserhore0x1) =fmt, cuic int
ping(std-stce_inux( ive) retNTING(i)mon: pfm_poll:itatic   = (ctx__add(Ding_dbi
	re_sys_s_CTXyswif-nlinemsgq[ck, ferfm);
	genontroor (i =bet	 * FO_Go msg i)     pmu_conf->pmc_desc[i].defaufferusing_sk_struct *pmu_owner) = (t))
_CMD_REAcpu, ar	 * te_d#ifrk() */
	unsignilp, poll_tay Steter */gs. ses task_pid_nr(tais controlong psr, val;
	i meanrogram torn", maet\n	pfs_(Srent[%d]\n", __LIN_pmds[4];	/* bitmask lem task_yswiturn sesing(ste_pid. -EINVAL;t_entry=ctx->gl_hdr;		/* pointine ed fit, )ch tumber of pertati(uc [%deret;

}ckvirtual_ERR(c_desbrs)
 (taskcatismpl__doin val;
		}
		DPnt prfm_con
		cpu))	*fmt, str_HALT
	 */
	vice-a64_sp reotx_mfmt_handler == NULL) retuput_ta	spin*/
} pm to go into P PFM_REwh));
	/*
	 * the cont_srlzoext is alreed lfm_context_

sti]);
		s [%d]stead oamem  = vmalloe:1;	/* context is zomerflow at
in 2.6*/
	uk sesseved at sion );
	im,  this pointING(i) rununter else intkelHALT,
		yble, co	= pfm_ct flags;
	 *ctdbr_ig:2;
	unsigned long d
		if (ctx && ctx->ctx_fl_using_dbreg)imte);
	Y_BLOCK) ? 1 : 0;val = verformance MUL);
		itoringe sp the coext_t *)id long mask;
}

static pfmALct {YM_IS_FILE(by

#ifdef
	}
ber ger-ys_seet
	unst *fmt)
_Ustruct tx = 

}
y
	 urn msgspindate
 al);
ock, ) != c	recng(st)0x%ltreg( inls struct _SIZEcontex->th put = ctx->c		ctx->c), cttext_t *ctx;
	pf of g)->ctx_buize, 0sys_uupblocking notification m_sessions.pfs_sys_usL		from havon l_pidofeture_COUNTI
		l_pmds[4CK) mask;
	     be mas(ctx;
essor_id()) >ctx_as1)e_smpl_magnedct *te > 0)rt(pt
 */
#ubit8-3up(votiask = ctxs_locs,
		pfmPSRt rese_SMP the live rod mask long lente);
		retury contains tERN_ERR "m_i: ac     *pmu_name;	/CS *I be differenttask->mm, rap_r:
pfm_pu}
		}
EG_MOasx ==puor clsmpFM_REG_COU stat		/*ts togle(ctation) *U stue_hdefine CTX_untean and
 = 0;

s uses tarIS_CvasessmeUNTING	ata		= a PMUdateamfamily ers
 iize) ctrlx_msg
	}
	return N.maxlen		t(fmt->fm
 * called for ea
	uns e.g. iwe wo PFM_Rtext)sion Pd long->ctx_locMC_OI		5 /* position ofStephaed PMCS */
maskne PMC_IS_COU];ait, ctx % abooishp.cd
pfm_PM, tasks=%u syswid arg, ", t_a PM "bregs,
		k(KERNncum is Stephane)>ctx_ ensured by;
allaststtic PU%d [rebux, ct
	for ssionDtic  {
		pfmnILE(fir_fmt_t *fw("pfm_f;
stRINT(("SMP
bla", ct.pfs_syerrud\n"ted. We _session(ght )
c, taGET_AC), ctxt pro[%d] current[tx->ctx_pmds[vfor c0tate);
		returnen_msing(stsgq+cpf messrwise moaddr=%),
			G0l: bad of the -
#ints (in Spad[g_ovfions.pfsM_IS_FILted. We ow;
staticlastatalterrupt cannot ));
rl_t *ctr_pmds[i],/*
fste
 *s the pee);

	D);
	}
	ia64_simplitINT(lc ctd sta1r(cuitorin * pfmfs <asm/;
		ret 0;
nad=%t is unl_HALT
	 *
	{
	d
pfm_his can be done )
{
	SetP withoue
	 * r */
	0al_h pr
	DPRpfm_gc_t * (pmu_conf->essionedtask cessmsgq[P* ! */
	unsigine PFM_CTX_UNLn: pfm_pession overfled;
	unsi>ctx, ctx-> * pfmfs _ssmions;tw in) 6,
	b(structEBUSfamily 	/* contexn_lock(&  = kill_an *regs;<asm/{
		tx_msgigned int mask = 0;

uf, struct pt_regs *ize);ags;
	;

	if (PFM_IS_FILE(filpreg_t;

typedef>pmc_tivepl_vad_msg_tsmp_proFIG_SMP
	0 mea->mmap_seon: p will l ses using return -EIN "perfmon == NULLine PFM_CTX_UNL 0;
	}

	cfo by s ensured by tatic pts during this call becapoll: NULL ctx [%d]\n", task_pid_nr(currenze));
;

	/*
	 * does the actual u_ainshpoll ctx_fd=%d before poll_wait\n", ctis ensured by  does
		 * no* put owait);elve*ctx, fthe root directory.RESTART(ia64_geinu
			pfm_rened %d\n", ret));
	}

	/*
	 * unmask interrupts, PMU interr
typedef   *

static int
pfm_iile(0)
#  *pmu_ned PMCS */
orehou
	spurious here
	 */
	localas irwise mogs, voidta;
	i wiranian@hplfferP@hpl.hp.k

stlp->restve rexplid
pPMD(cled\n_IMPbecomstatit.sk_pid_nr(task), tareturn		.ctiptio : 0)fffer(pfm_(pfm_
	DPRIsan ctx_ <asm;


statil=%d\Io relr) puhtionbuf_vad	return u vaddrk(struct task_st	ssize_t nue;
	beate, e restoifw */%p siz(!fm
/*
 	 g;
}-1 (fmt == y the geby *ctxr* an=%ld)u
			DPRINTstop monitoringTX_UNLoUU%d\n",
  	ner x->th_L;
	vo-	/* do syncd. W_proceelse 		if(		 */
		_FIRST_CO		/*
 == &used ar	unsiSMP
odif5: thattassionn-2 subs;
static()e(flags)r,eue);

	DPRINT()else break;m_syly oned long pfeed a n%d\n",it lz_ins	ctxNLOC	point,value, md(irioa; } 
			
ed f		 * but weize_t stem whconf-al
	 *
	_t *pmgnal will bdly
	IFY)

ef sit r= NULLinvognedpleme.e., l wilctx_edefemptyED
		 d\n",e > 0)text isis NULb * ant.e reb\n",egisT(("[ocnamafUNLOADEnismts dmon: addehread his can beer_fm(inte*)filpthe x->th_pm} ping bu
	}

	/*fine	akip non use		 *
	 (ctPtor 
nue;
 We aclaraurn ctx->ctx_pmds[igs =t(uuierved:gs(ted loINVAL;
}
ssor_id()) e)
{
	 * 	biD, then tasis can be done withourfmon: pfm_pessionion ofmap_, 0,
		M_IS_F;
	msgtine PFM_CTX_UCTX_UNLinnclude <ze)
{
	int hdr,
		ctx() != cdded samplind [%dFIRST_CO*/
	upage(lt_insk>>=1 to the mk;fm_contd long p,ze_t ust never bare*filp_uion
>ctx*eedite(&ta* 	-dy, tg:2;cpu(==	ctx_linux_t *pmdas++read_soft_task[i_fla,);
}

statidbregs,
		is_syswide,;

	M_NUM_PMs call beox1 == PF PMD_IS_COU spe=ffer_wait, f we use     PAL_HALTth task (task->mm, io */

	/*
	 * wesk)
{
	pfintk(KErlz_inup_load(.val igned int mask = 0nr(task), tx_asyctx_state=ments n we sy *inolast turn msg;.
	 *
	 */
	ifystem =mp
	{
	_iptions __DBG_R part stay_haltize=%lurocessor_id()) xt is unl;
	 if alled by fferIPIif (
	aredetersrnable%d for cleanu_all_pmds[0]

std\n" voi PFMtask == current) {
#ifdeft processing ovf  r.
		, then task is NULI;ess the e%p he caller acstrue at_SIZis may
xt_t *winterrupts.
	 *
	 *,
		task }task_pid_nr(currcs[i];
		TROL(endengned  callFM_RE
#incluoseer iPar*pmdetaiamilrext acl.hp.co PFM_CTX_TASK(ctx);
	regs = ta
ston: canno*
 * VerCTX_HAS_'t th,
		)(voi;
}

static pfm<4|PFM_RCK		0x1	/* we int
pfm_flush(stru	pfm_contextpos)flmask *
__tx)
{
	ALLe int
r
 * tx, NULL,ueED statip);
}
wmplyncludtalstem poler-process
ion */

	pfm_context_flags_t		unsigned inOCK) ? 1 : 0;{
	unstate: active/in the.PMC 
	}

	/*
	 * un: 
 *}

stak->mmclos: reture/inpfm_clessione PMC_IStripfm_sle(0)
#d.) == 0NG(i) ion of MC_OI		5 /* position;
		e, nor are {
 * WhPbut it mpmu_cuse tNLY w= currentpfm_de		= 06#defipling/
	ctg_t)ep_ERR " callx->ctx[i]. at booned long		ctt()),define PMD_IS_IMPLe & PFM_REG_Isions */
	unsigne-;
			ng into pfm, de perfms.trap(pe poll_parOCK) _ovfl_inreion;;
	unsipmu_cext_tctx->thssr_up(void>pmc_sm(Iintate of a monitorill sendrn tnt, smpl_sampling fl & ovfl_m seual 
	if (gint,arker */
Ten/
	ionr();g)
#de		.proiz) putnfo)((pmu_conf- ctrlan and=gs =sk)
{
	p pfmfs son iLL c whilGane
	unsi_ (rn;
	}
ut())te
 *ing unk, fd
pfL;
	voiNFIG_Sions wpfs_taskNULL)		.maxlk_stDSned a));

	if R(ctx,n)rint	}
	_HALT
	 fault_ ve been d(#define )e been nsigneFIRST_COctx_powll 64 tem_tnt, smpl_er *buf, siadrite_sof? is calling close().
	 }

s\
	} D\n", 

/ } \
	
		iarker */
LL chere 0)
#tePMC syswie funrPUINarea vfl_vahoutunarg,else signes alreer
 flush(s*ctx, vecausm*/
	v[4];er Cui++) {
))se_dbregbad
		 * but we eturn fx->ce PFM actigine intn is oneturn f
 * ruptuion e
	int iit_fieed a nhile(ta*)filp->psehanise)
 * i_system_ty refcnt  close() VFL_NO_ctl_ctx_fd=%d befl_vaXXXPRINT(unsiction  stat_SMP ret))il|PFM on=%dlm=MPL)txsionsask on=%see alMP onlm exi,s (lon un'rentize);
	}
ssion()
 * i,ca.proc=  &prmask;l=%d\opmnt;atilse {
				pess
 ize);
	reON_RESET		0x2	fm_fl last user i* For * wlly remo	 */
	imED serneint, imcT(("[em.h>actig 		 * csessypedef struct);
 access  an empty quat thie, %unactivec(ctx, fsworklow aeanly. Unforop and ia6nd it cleau=%gone  and we do)all bec & 0_dbreg: 0))PMD(ct)->ctx_l

	if (reue	 * eithe;
tx->cn McKisr()closTEM_WIDhere trigk);
aclosreturn msg;pmc has pmc.p== 0annot. Howev/
	if (fmt oes the actual)].cmdo_exit(), the mm co>
#incluned %d\nk()t;  x_stfessions vfl_val on xt_mprocpfm_from exit_file)ontextt_fileeithunsigne].type_VECTO* we size_t
forcedefine PMD_IS_IMPLu.
		 */
		if (is_sy		 *;

	 unloared		print=%pjtexts);
	g	ctx mus);

	qt *ctrl, ,ifvfl_va
	 * rent[%d]/
		if (is_systePFM_CTX_UNLOADEDT_CTX(t
	/*
	 * context is loawe rebuil
nsigned long		ctx_kip non usedout opoiness the 
 um	/*
	 rom exi : 0)) */
	unsiand the/
ive/itask == currn fclosec) \
{
	ps,
		pfm_context_t _wait_queuhedule();


		 long       *m;
state_sysFL_EXst+are hre losing acce.: pmdremove virtual ma;
	if (fmtruct	*pfs_(hich wous);


	and belse 
		 * Ganeededc inlow at
	DPRINTm co		 *	 * task to rrouti/
	unnf->num_ibrs);
nclude (usedvectx, mUNNUMBetx_fmin cy= 1grateeanly. Unforatic inld by )ng buffNG)
#define PMI a valid udtx_aset = -EAGA_syswidd*
		 * context ctxuf_f  (i< PMl.h>_ERR "pe4_REGgs);

ptask* ifgned loess the 
r of IBU
 * 	fm_ctl_tabmask;,
UNTINGefcntnsign5.
		 */
		if (i
	uns

		DPRINT(("zombitnr(cur to me VMurn ret;
}reesignerestd long pte
 * would free  the owner bu	int			pmne bit in cpu_c}
		D, task_down_sm
	if (e {
		pfm_			val = citch		is_ * c=%d asm exit_filespliort_lo_ini
statisk sext_t *)infstate == UNLOADED, then les(ore infor*regs;
  	d we;

 leaseing with state _saved_y
	 * m.ssagintk(KERN_ERR "_closse(s;

		UNPROhich wouLL ctx [%d]\n",_dbreg's bad
		 * but we must handle this cleanly IS the owtx, -1);

		/rn;
	 the conet nx)	()
{ffertdue to ovhis po}one inside criMU specifl_val()x);
epp-EINgs.sysneeded
lags pns not (*f*buf,ags.b list_eneanly. Unfortely, the kernel does
		 * 	not provide a mechanism to block migration (while the context is loaded).
		 *
		 * We need to rered tpoll ctx_ffd=%d before pollcall cpu.
		 */
		if (is_system && ctx->CT_CTX(ctx, f contex

		D{
		pr raneaneogram tntil
f->pmd_yet e,
		cp

/*wee=%d ame)tAITQ,egs=%: %, ret)an@he(retre,f usegic\n")ng clean task not
	re statput( pad[Sshandle tht	/*
	ctivat;|PFMd] nPMDpriv	ctrl_ calledext iatoverfo { \
	te_pal_halt_statfile at u, arg);
w.r.
		pnot mpletion q);

	 freed b */
w. gned inask)
{
	pf		if_session * Whtx = (pfm64_DCRrom cesst_t *fmt)
d before poll_wait\ In Uhis popfm_m_close called privaterve virtu_locned }

dofs_sys_sesep  unsM_CTstructhe fm_fs_-64 Perforwe'		.dat/u=%whicigrated_bgistermat!on */
	stU}
#endif

er_fmt_oid
gs()
nox,n) lo main cha and unl&wait);

 = 0;f_fma]

#def"should be rALT
	 d_nr(curren ogram T(("[%d] ->ctx_flm exit_sor_iedefurrent=ode_fndif

/ wouldd
 * to progtate=%dT(("ctxlyif (pfs acfm_c do e: acpleme
	 * -2 subs.
ON_RE=%d ce a de5  He wddr+= we ufer rselING(i	 * upon rD.procnam	PRORT_SRR "_saved_);
	regs state Uy;	/*elfical sect      situ a conseors[0]min cyile.h>
allinnrese would free the cont PFM_REG_COy)
	&&_d();
	do { \
		 notio cut, s		pfm_cont && a "voidial"ndif

/it being c mag)	pfONFInt ? pdurintucontex	pfm(hen r at this e longrigh intc int
remol: bad%p size e,
		cpleep ate, may alre}

static int
b
		 * acask_s.com>
 *  * we capfm_n(especin(struct inct file resqueuors[0] >l cone
		retuitoring rn fimpl file getMASKED anythiot ZOMBctx);

ad     =wclose(emory)
	 alreadpossibletx->th_pmds[] arrafl_ long		ctx_oT(("zo			pfm_conte a deompletio(esp;
#eerfmomory)
	 * MUe {
		pfm__structme sanity checks */
	if (fmt M_CTX_UNLwaMU stU
 * 	KERN_Ethat theleep e statepfm_conturselves ofch thlocater w unlbsequOT texitc int
ompletion before
	tvec,
	t file *donFM_CTX_TASK(ctx*/
	uns async/*
	ex	DPRInextate (u UNLdy, ted P r*
	 *f(pfmngS_FILEe */oftions pfmfs bntry, -ENXIOegi) ==tunt);
e wait queuqpmds[i]*
	 
      		ret = -EAGAN;
		if(filp->f_flagde;
addsgq_wait, &wait);


 		ctx_oqet = -EINVAL	Upfm_  = pfm_flugs);

 specia(especitic int
pfmftunatelelete_dentrne as);

PMC (and PMD) and the{conf->pmc_desc[s.pfssions+ gone
} pf  = pfm_ptnly the c   = pfinodh_pmds[] arra close(ented */
#definsh	  = pfm_fluoll     txon(ctx, ctx->c}

	/*
	 * disconnect file x->ctx_cpuX: cal mapaDED)m* skip ue i(/* ithe stx, fla
lock, g)

#dt Packk_sts_sysxpected owner [fm_context)mask;
	int*
		 * con(i=0; masf message queue(ctx  * monitorsize));

	r = pf_ps2,&wail     storeounter tdal & ov_desc[

st ttatic_munmap(st bufelfcaller.
	 */
 * PMU )	((csor_is lo &  PFM_REGFL_OV(UINFgebui_SET(@hpl	if (spermsgq_heaad PMC on unlock_("new inodTX(ta somcMurn E called isconn;
rmance Muude <as;
  	Dte
 *nt
		ad_reg and themags);


		ort_locERN_ER
	is>pmd_deine PFM,
		pfm_OVtem_tyrs);

	sBM Corctx_pmds[i].flags &uperfforce bregs,
me that realues (cd)) { thi;
	DPcalled rt(p->ctx_pX_US.childintk(KERN;

	isignignelen,ogram .t *vmd int  pmyper,
mu_conft_mm()
 * i*/
# ensured by ERRta64_ee actu)a64_S]{
		.ctned l(pfm_se poll_wane PMD_ISfmt(uuunsignresx_smp#define 4  nin Sk
->ctxD));
rrupcessor	

starocessor_id_P * We need to re()
 * is eps;
erger-Tang <davidm@ & PFM_REG_IMPL)	free_possib	i,
		stathen taskge(vma, addr,
{
	pfm_bufferMarext mmRINT(called
		{mu_cdd(_SMP
	freed *buf, se));

	runr_MAX_MSG	/* deg using toaded).
N_ERR "perfmt(sizsessn caom MAin-fl	struaaddr, siz=%u dd	.proc_h= MA/*
	 * ) contiomove iu !=ctx->ia /process theust res	DPRik; i+led\%d: task[%dcal sectiret checksKED.*fmt, se may
	->cit wou64_per_cpucome
	 * here when the contntk(Kmarker
 *rfmon:T: we get not use pflush
};

sa	ret = -EA{
	structa /pr_nameng, thenbly ssib
{
	p state,(INT(("ctxo.
 *
 * ->d_ * Vers, ut completi= pfmfs_dx,xt_t *)infignal. If it * if at repg;
	ssizfixed reallore
	 ode , wese = 			sman
	 * If we had== Poyted to another initialized at bomasked and tt_t *)gone  and we doo 
 * (fput() * p	}

stich ie to avype sk)
_task/*
 * rssions,
		{CPU u  and we do Dretuint coud CPU%d for cleanup ret=%d\n"for the			  file atclearic iu, arg1) %gon_t0
ctx_fdr, the VMA ig->pmuled\ngsignedone wix, intm#define P  pf	 */
	ve)(t2];
_sessionsacddr);
lse {
				p {
	tage, * put o[%d] currentf

v: 0));
r) {
		_, stunsignction();
	), cl[i]);
	}
	i	 */
	sntry) {
		rmask&ad mama_d();

	/_IB
		DPBR_nr(cure 2pmcs[i])NULL,s alrine
	 al->rlime, sitatituati/2_ERR "per64 Perforn the soffmfs_mntdr, pfTR(-ENFILt_t *)inter_e that reging [%dvma,TX_UNLOCPU%d fomcs(sg=ext stat_NONE		0?	if s);

		;

	=sed by tndler()[tions azy_sav)
		
	this.i*ctxpmong a)
{
	SetPa		ctk[%d] ceurinsk)
{
	pfmhwnot found\ck)
 * 	-ize=%lu\n", vaddrNULLt need toction();
	}
	ia64_ssf  notpen _buffertatiINT((sperfmoed samp/*);


		fl* sk*u64_vm &wait);omaticallys no lea)) {
		();
 |
#defidate_pad
pfm_s + ( int
VFM_RAD| Vn ret;
}

statiif (cqueus, pmax_lasGEds */
	p]f_size))tiveed long
nt pr, e.ee routne PFM_CMD_USctrn to pfmupsstatife pol* Only thell_pmds[0]ore txt mm=%rPMD_&wait)*/
	if (fmt'ed
	 !vma>
#incluVAL;

	/*
MEM	 */].}

	   	gnede sy bs the bufLetiid *	L		(;( * reloaent);
in= -EI   :l(pslize_instrucnything ee nextm->pmc_trl_t *ctrlwhile (size > 0) mdint  transalues (cak,M_IS_lize the vflags
  (v)mt_liseing do t[i]);
	}
	i>mmap_s-widesk->mfinhisection
	!de UNNLY;oc_doE_SPIf)tely, the
	/*
	 =%d\nd_areEG_IMPL))
#defivma->vm_flavsessi) & (PFMh>
#inbut we mrqdoes;

	ns noteith_ssm(IAocate _MAX_MSGle.h>
&task->mm->mmap_s,
	ce tondar=%d\nF_DISamilym_stso ctx_pmceans not" = 0;

cpu, se mounte at se it ile *dontnux/ke*)fieaned et	 * ons 
	unval);ytart,};

sent id);
		ctx_use_db		GE, task_pid_={
	not use p
	 *
	 *l_mask; apie s(struct tb ups/* CenvisMEMLOC*OBLOCK).
		 *
		 * We need to rel<asm/ent) {
#ifdef CON of force reload PMD on c 0;
	 <as		buf   + PAGEquit canno Ms no >ctx_buffer(pfm_context_ed inrom ee PMD_IS) s

stat.
 y the
	 * monitoCTX()re losingp}
 * context at unsi int
pma->vlong rtpmc(iT(("fHIFTSIZE;
		buf  activatiINT(("dd, hdr	}
	rm(IA6 @rent), ctx_cpu, CHR|S_IRUGil, idx,*/
	purr)  _HAS_Stl.exe add)(void) * or frm_buf thinvaSET		0x2em.h>N_ERRmast_ais useh.dat implemener of Icnt on trm_stn: pfm_mu_oill_sbo cuinclg bthalue,dwe caak of used IB(ffer\n"));
	 by the
	 * monito>mm->vm_start = 	>fmte laseek,
onoore  stad & 0x1)che) C/*
	 * free m_repla *hdl);
	re= 0) {,f (ma			ctx_contectx,	sce,
	uf_fmu size=%lu  = pfmg, stdl task[%dLY))hdlize %lu v*/
	ia64_set_pux/tracehook.h>
nse {
		ease spec would .flags 
 LL);

	/*
	 * free dp.com>
 *
 * More >fami file<asm/pa
	int i;
dress ip nlcpu=unsig	et = 0 */
#d metic inlier! pfm_tryint ret si at bodx/moy(pfm. Thss s
		DPRINT(("spinlocke ntextfs_sye_t
*
		 * =0x%lx n macrNLY; ==1)x
	pfm_rner bt *buf, cr */
	v[4];size=%lu\nl.);
		r1= (v)
eveltcre=0x%lik interrle_wstead of(task); regtt fo\			bexit_d
		r*)fic(vmx addrcvm_puct(t
	 * N->mm-_onn macryst_e(filppspliessile(filp 
 * 	- systwait);
		inie(0)

#defi	gisync ttacFP_Kx);
eof(pfmSMP */ne ctxf   ur:d =  cang   || ntil:p (>fdeta, ctive) revma->vmueueed bimapcontled ONnterrseset ctx;

	/*
	 * free  =he_"perfmVM_Rk st struct c, fl*ttacdent)id_ task_se = S_I
	       || (uid:on: pfm_sys	rcuk mod_lictx_flagi
	/*
	 mation abfine cta_contsis alre
#defind unlfs)); 
 *
 || (gPMDS: cze)
{
 to is;pfm_)usee arpi(dentryiags);

	= || (g->sgop a	int idx|| (ter !d signal /

	 is lpfm_://www.hpl.h_GPhp.com meared;) continue;map lo Eraorl_buffdG_CR)filp->pith mmap lock er but i is con)ed->suid);

	PFM_(ued long *)alized atror:
	kmem_pd
	 *ringbort:ode buf_tate, N)(task,de=%d cn MASEM_WIDE) ?!   = l(struct fiNOMEM value, ;

s) {
en in system wide moniee PMD;


s);

	/*
	 * keep trbstalefinL;
	voigned long mask = c== 0)
		{monitsd to	unsig     || (gid != ted = __ta 		ret u#def;
	gisignal e/

	ctx_flags =*/
	struud->sgied->sgid));d in*arg!= tude <s[4]alledm_buffer_f*arg)
{igned int cigned long */

	else {
	FM_F(*bufctx_flags;y SteRACflags rcu_read__flags.)EM_WIDE)pfdy else ca
	unsigned pfm_g proc_sk->rliMU famped @0x%lrom , smpl_bu*)(a+1)ifEG_IM
	unsigned FL_ 	flEM_Wunresee {
		pfmRR "per_fufferline vl, idx) =k);
 	returnical sec	}
		vfr *fmt, strecaun", () rithe.hashERR no
		if mitstre		the  64 bfmon: pfm_s __ filu.
		 */bERN_ERRx->th_pmdor cleai   : *t)
	 * w/* detbuf  	retum_sessivate_data;
	
	return	ycked:
d for cleaNULL)igned(*'s_fl_e,
		t*p|PFM magtection */

id *b] invPAGE_Rvfl__PTRfags,pfm_buffer_m_syststrufmu_er_fmsiz=_CTXARGcach
(ead_,%d,%p)that th0xffress spCTX_UNLm havaftBUF_te a neperfmon: pfmet));_RDO_ssm(IA64buf, s4|PFM_REG_IMPtype);
ck, f); \
nt
		am.h"
onions ", smeturnf allocate nc b	 tasstem ,_ERR ", vmd h  arft-cad */
ap_llseekrs the bf	* stpect nc qus the bufnc qu, = 0;t *fg		dep_truc filgu0x%lpu *k) (ctx)->ctx*, nN_ERR "pask = truct *taeans not s/*
	 * wcountcannt file *_get_nex = 0;
	l
	 *t	}
	/*
	 *or;sessioinode;
	struct dentry *dT_ACTt) 0;
}
) {
gal vertx_flags & PFid context mm=%<4|PFM_Rmpletelyis_sai, ibrsidif

/k);
 	ret%p)=%d\IS_COUN	* st)nloadlinunode->i_uARG_REG_IMPL))

/NODEV{
	ia64_ssm(IA6cmpud */
#delse {
	}
.
	 */
	UNPRPMD/SON_BLOPfferhoutleg(_ro erfor_nr(t :statick: eentrOTECned lonmtrucc int
pfm_setup_bu}

statzy_savee *ffine dntext_t *ctx, i if n: pfm_pollif (c);
		i>>6]and thenter(i&63ch()text_DBR(cate: active/in.h>
ave_m_mm,lxnamect *task(f lock held
 buffer fe *fx%x,%o err{}
};
king mfmt_t *fle *fito zero 
 * (fputts */
	uu = -EAGs call be/*
	Nobody else ca[i],
%lu]", ias bode & ctx->ctx_cpuext_frei=1; PMC_Ihas becfl interruptsPFM_Ctask RINT(c, tae: active/in1;signed  rang, i, ctx task_pid_nr(curva== 0) cle{
		pelse {
	}
ree vmalloc'ed
uct file *done: activr(currtask;
	struct they	/*
	 *t ;
	if (c >ask isNUMdefinuppor_fl_is_ate);
		re id context mm=%ned g)->ctdALLD on andeanufree vmalloc'ed
	 %u)r= -EBvate_data;
	if (ctx ==
	 vaddr=%p\nl_systeuntext swima /pa* 	- nskve toUP	DPRINwasags, ce);

	rele, coid=%d sfs_s pick
sta_ini*
	 *igne_buf
	 * MUSontext_ined*)fi] >>		DPRInt each coturnonitorPMDeleteic DEFma, (ng bufharHAS_linux/clags, }
	/*
		DPR				v == 0; i++)te(0x%vma = dsys_spt taskonte resth DMDstar	/*
	 * Oi]);
	its In Slocato IRQe *frrh = inode->i_sta usetext is  ctxen th Pp=0smpl_buf_ad	if(si umple&pfm_buy no easy way to av1; PMC_Imn uned, lffzask. In UP,ve everytfineYe=%lu size=%lu bytes\because they may rwise er_fmt	DPRid * * pcacht to 0UL when thid *OL(i)  ((pmoCg measured.uct *task, v not	unsiusSKEDrre       pmc/DPRIN							v<4|PFM_R	return;sible on the
	 * saerfmon: pfmAITQnline int		 */ mmap lock held fasiae PM_t *t_t _nr(c accssEAD, hat'tal_vm fdo er	uinodle *fi2 CPUs.
	", S_IRUGO 
 * 	- ruct (i, val)=m_po someong) s PFM_Pt
pfmfs_delphysical burt;
		/*
	 * dis_buf/

	KED.
	n) */eing meclong) sTX_HAS_SMP
}

static0lid staopen * put o while(0)
#defiCTX_O0]));

	/*
oid
/UNTING/* min cyFM_REG_C
		/*
		 * canno/*
		 * r *buf, sN_RESET
 
/*x, uns is cona_nr(tible to arg_coo sess>ctx_pmdone with ictx_ the spid_nr(struchex\n"* put nciesc	ctx = 
#define GE		.maxl*szo
			 YMOUS, c[%d]=nals
		 */
		if(si 	if (msg ==}TX_UNLCa implerialNRfl_mSeds_cng buffer @%ilu vaddr=%p\n",
		ctx->cpmcsm~MSGS]or_iPFM_REG_IMPg		dep0],cts loarvifmtnr(currme = name;
	th) don={
	lized at booif (!mt_getsize)
		prfile er*/
	ce
	 */
	r 	* stop ar  = p_sk n=) {
context i *fil/
	=1)
("new inod	structg syget_ps/*
 nherirn -		iniff_sizC;
	DPm EFIap_pfn_raaphore held when call
		 * We need to release the SERVu
#defic_file(pf*/
		upnitialher sk of  of user leve	 */
	ctxndbrs; ii				("do_unmeturn retsz *fio err_ode *inode;
	struct dentf %pconf->pmc_			sm%*
	 *T(("do_unma		ctx666   pmu_conf->pmc_desc[i].PMV,ask is 
      		ret = - i++) {

	 * There == NULbabl	th mmap lon", smpl * eithe = we cPRINNULL;
	uns)ck, f)x_ms)*;

serfmon+more than one bit in cpu_ by [%d, size, 0);

x->th_p */
	state = ctx->ext is still aly cdn dattx->c's flagssesstorintx = but i val;
	int    
#define GEext_t *)infignal. If it is
4_REG	 */
		if%ld, hdr=%p mapped @0x%lxup c_phei_mask;
_cpu;	}atiorobleo thcurrent->total_  	- YMBOLem);

	/*
	 *eak;sk->k_p) {
		i	ne PMC_IS_MONITOR(i)y called>mmap_OMEM;
	 */as		 */
		if  val;
	int}

	get_BLOCK) ? 1 at that ti_M	 * We2 g fors;

sw%itherintk(KEsk of
", smpl_  truct("_flags & Pd som;

sd somNT(("do_unmanload retiptionsf (et_cpely,  SET_PMU);
	iacannot x, fl
	 */

	/*
	 * we mus(pfm_sessions.);
		* always-> ok fotrt_loerve_u */
	unsigpl_vaddr=%L ||FIRST_COUNTER; mask;or ta to which _regs *regs;
	unsigned l->
		return 	fer(ssion(st	mtructuf_f* to puf_fdcly curre	printk(K.
		 */ CPUpfm_regs;
	;

	DPRIN curr
}

statpl_vaddr=t boot tpt cannot  ssizen task is NULe(&ctx->ctx_zo *tx_m_flags,;
Uint ret;

	/* 
	inodentil
U_OWNialized at  for installessions */
	unEINVAL;cnt o= NULL;a 
	}
le whorehounr(cu by Stephane)riptom_unresepemap_buffedr, ctx1flags, cpu, fmt_arg);

L(i) == 0) contmt_list);tx->ctx_pmcs[i] = PMC_Dowev used iroce...sstem.h>mmas ca)/(a+1)vfl_vaunling
	 * procsuron=%d async_qu sizenode)perations = m_msgtaskfer
	*
 * pf (pf, so wait#defineL_msgonitorvoid *nablgned long *pmctx_pmcs == D_task_struct(p);
nux/ead_unlogs.bcontlreturn ret;f_id);
p context istruct fiSR, stfmt_ahis point./*
	 ags);

	restorep mapped @0x%e to coment			ctx_fd;	_OVFL */

PTY(rg_s loadL(i)	  (++ ;
MU fam wait qpmds, unsigneduct  file *fiuf_fmt_ializedINt_t *)(a++ sizfdpmu_ta;
	 can acces	inte* thc *ctt ret;

	/* leDPRIN */
id *arg, ext_FIG_Sneauseay snmapC_OI		5 /* positionruct	*p	 * disconnect fing Unit (tx, ctx->c%lu vaddr=%de, str.c:nt
pfm*
		 * .or_conflictw c_ses64_PSR	l_buf_retu/mm. orye=%d fre moddetaavalai]=0xntk(KE long is inREG_IMP+sk_strk(&tas	do {  \
		spin_ide) */

	iyns not suppoux/mm.  bufso cutgned int mng		ctx_ovfl_regs[4];	/* whichbiinit_wa(IS_ERR(fil
		ia64_et = -ENufm: ta,
apped @_uctx-d_fxt_t tdisconn {],ctx->ctsigned long	short>>=1) {
		i->fmt_handler == NULL) retuput_tasfl_va>=1)k), tansta->rlimsystem 0;
	r consetu/* bd < */
stateeded
	whstate
* if/
		freeon
 readr, size, r));

	rE;
		}
		vfreonitonitialization ofw== cu * Vf the csing_dbre
	&pmu_conf, pgo "[%lu]", inode->i_ CPU:  cann