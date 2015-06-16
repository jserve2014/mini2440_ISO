/*
 *  kernel/sched.c
 *
 *  Kernel scheduler and related syscalls
 *
 *  Copyright (C) 1991-2002  Linus Torvalds
 *
 *  1996-12-23  Modified by Dave Grothe to fix bugs in semaphores and
 *		make semaphores SMP safe
 *  1998-11-19	Implemented schedule_timeout() and related stuff
 *		by Andrea Arcangeli
 *  2002-01-04	New ultra-scalable O(1) scheduler by Ingo Molnar:
 *		hybrid priority-list and round-robin design with
 *		an array-switch method of distributing timeslices
 *		and per-CPU runqueues.  Cleanups and useful suggestions
 *		by Davide Libenzi, preemptible kernel bits by Robert Love.
 *  2003-09-03	Interactivity tuning by Con Kolivas.
 *  2004-04-02	Scheduler domains code by Nick Piggin
 *  2007-04-15  Work begun on replacing all interactivity tuning with a
 *              fair scheduling design by Con Kolivas.
 *  2007-05-05  Load balancing (smp-nice) and other improvements
 *              by Peter Williams
 *  2007-05-06  Interactivity improvements to CFS by Mike Galbraith
 *  2007-07-01  Group scheduling enhancements by Srivatsa Vaddagiri
 *  2007-11-29  RT balancing improvements by Steven Rostedt, Gregory Haskins,
 *              Thomas Gleixner, Mike Kravetz
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/nmi.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/highmem.h>
#include <linux/smp_lock.h>
#include <asm/mmu_context.h>
#include <linux/interrupt.h>
#include <linux/capability.h>
#include <linux/completion.h>
#include <linux/kernel_stat.h>
#include <linux/debug_locks.h>
#include <linux/perf_event.h>
#include <linux/security.h>
#include <linux/notifier.h>
#include <linux/profile.h>
#include <linux/freezer.h>
#include <linux/vmalloc.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/pid_namespace.h>
#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/timer.h>
#include <linux/rcupdate.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/percpu.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sysctl.h>
#include <linux/syscalls.h>
#include <linux/times.h>
#include <linux/tsacct_kern.h>
#include <linux/kprobes.h>
#include <linux/delayacct.h>
#include <linux/unistd.h>
#include <linux/pagemap.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/debugfs.h>
#include <linux/ctype.h>
#include <linux/ftrace.h>

#include <asm/tlb.h>
#include <asm/irq_regs.h>

#include "sched_cpupri.h"

#define CREATE_TRACE_POINTS
#include <trace/events/sched.h>

/*
 * Convert user-nice values [ -20 ... 0 ... 19 ]
 * to static priority [ MAX_RT_PRIO..MAX_PRIO-1 ],
 * and back.
 */
#define NICE_TO_PRIO(nice)	(MAX_RT_PRIO + (nice) + 20)
#define PRIO_TO_NICE(prio)	((prio) - MAX_RT_PRIO - 20)
#define TASK_NICE(p)		PRIO_TO_NICE((p)->static_prio)

/*
 * 'User priority' is the nice value converted to something we
 * can work with better when scaling various scheduler parameters,
 * it's a [ 0 ... 39 ] range.
 */
#define USER_PRIO(p)		((p)-MAX_RT_PRIO)
#define TASK_USER_PRIO(p)	USER_PRIO((p)->static_prio)
#define MAX_USER_PRIO		(USER_PRIO(MAX_PRIO))

/*
 * Helpers for converting nanosecond timing to jiffy resolution
 */
#define NS_TO_JIFFIES(TIME)	((unsigned long)(TIME) / (NSEC_PER_SEC / HZ))

#define NICE_0_LOAD		SCHED_LOAD_SCALE
#define NICE_0_SHIFT		SCHED_LOAD_SHIFT

/*
 * These are the 'tuning knobs' of the scheduler:
 *
 * default timeslice is 100 msecs (used only for SCHED_RR tasks).
 * Timeslices get refilled after they expire.
 */
#define DEF_TIMESLICE		(100 * HZ / 1000)

/*
 * single value that denotes runtime == period, ie unlimited time.
 */
#define RUNTIME_INF	((u64)~0ULL)

static inline int rt_policy(int policy)
{
	if (unlikely(policy == SCHED_FIFO || policy == SCHED_RR))
		return 1;
	return 0;
}

static inline int task_has_rt_policy(struct task_struct *p)
{
	return rt_policy(p->policy);
}

/*
 * This is the priority-queue data structure of the RT scheduling class:
 */
struct rt_prio_array {
	DECLARE_BITMAP(bitmap, MAX_RT_PRIO+1); /* include 1 bit for delimiter */
	struct list_head queue[MAX_RT_PRIO];
};

struct rt_bandwidth {
	/* nests inside the rq lock: */
	spinlock_t		rt_runtime_lock;
	ktime_t			rt_period;
	u64			rt_runtime;
	struct hrtimer		rt_period_timer;
};

static struct rt_bandwidth def_rt_bandwidth;

static int do_sched_rt_period_timer(struct rt_bandwidth *rt_b, int overrun);

static enum hrtimer_restart sched_rt_period_timer(struct hrtimer *timer)
{
	struct rt_bandwidth *rt_b =
		container_of(timer, struct rt_bandwidth, rt_period_timer);
	ktime_t now;
	int overrun;
	int idle = 0;

	for (;;) {
		now = hrtimer_cb_get_time(timer);
		overrun = hrtimer_forward(timer, now, rt_b->rt_period);

		if (!overrun)
			break;

		idle = do_sched_rt_period_timer(rt_b, overrun);
	}

	return idle ? HRTIMER_NORESTART : HRTIMER_RESTART;
}

static
void init_rt_bandwidth(struct rt_bandwidth *rt_b, u64 period, u64 runtime)
{
	rt_b->rt_period = ns_to_ktime(period);
	rt_b->rt_runtime = runtime;

	spin_lock_init(&rt_b->rt_runtime_lock);

	hrtimer_init(&rt_b->rt_period_timer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rt_b->rt_period_timer.function = sched_rt_period_timer;
}

static inline int rt_bandwidth_enabled(void)
{
	return sysctl_sched_rt_runtime >= 0;
}

static void start_rt_bandwidth(struct rt_bandwidth *rt_b)
{
	ktime_t now;

	if (!rt_bandwidth_enabled() || rt_b->rt_runtime == RUNTIME_INF)
		return;

	if (hrtimer_active(&rt_b->rt_period_timer))
		return;

	spin_lock(&rt_b->rt_runtime_lock);
	for (;;) {
		unsigned long delta;
		ktime_t soft, hard;

		if (hrtimer_active(&rt_b->rt_period_timer))
			break;

		now = hrtimer_cb_get_time(&rt_b->rt_period_timer);
		hrtimer_forward(&rt_b->rt_period_timer, now, rt_b->rt_period);

		soft = hrtimer_get_softexpires(&rt_b->rt_period_timer);
		hard = hrtimer_get_expires(&rt_b->rt_period_timer);
		delta = ktime_to_ns(ktime_sub(hard, soft));
		__hrtimer_start_range_ns(&rt_b->rt_period_timer, soft, delta,
				HRTIMER_MODE_ABS_PINNED, 0);
	}
	spin_unlock(&rt_b->rt_runtime_lock);
}

#ifdef CONFIG_RT_GROUP_SCHED
static void destroy_rt_bandwidth(struct rt_bandwidth *rt_b)
{
	hrtimer_cancel(&rt_b->rt_period_timer);
}
#endif

/*
 * sched_domains_mutex serializes calls to arch_init_sched_domains,
 * detach_destroy_domains and partition_sched_domains.
 */
static DEFINE_MUTEX(sched_domains_mutex);

#ifdef CONFIG_GROUP_SCHED

#include <linux/cgroup.h>

struct cfs_rq;

static LIST_HEAD(task_groups);

/* task group related information */
struct task_group {
#ifdef CONFIG_CGROUP_SCHED
	struct cgroup_subsys_state css;
#endif

#ifdef CONFIG_USER_SCHED
	uid_t uid;
#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
	/* schedulable entities of this group on each cpu */
	struct sched_entity **se;
	/* runqueue "owned" by this group on each cpu */
	struct cfs_rq **cfs_rq;
	unsigned long shares;
#endif

#ifdef CONFIG_RT_GROUP_SCHED
	struct sched_rt_entity **rt_se;
	struct rt_rq **rt_rq;

	struct rt_bandwidth rt_bandwidth;
#endif

	struct rcu_head rcu;
	struct list_head list;

	struct task_group *parent;
	struct list_head siblings;
	struct list_head children;
};

#ifdef CONFIG_USER_SCHED

/* Helper function to pass uid information to create_sched_user() */
void set_tg_uid(struct user_struct *user)
{
	user->tg->uid = user->uid;
}

/*
 * Root task group.
 *	Every UID task group (including init_task_group aka UID-0) will
 *	be a child to this group.
 */
struct task_group root_task_group;

#ifdef CONFIG_FAIR_GROUP_SCHED
/* Default task group's sched entity on each cpu */
static DEFINE_PER_CPU(struct sched_entity, init_sched_entity);
/* Default task group's cfs_rq on each cpu */
static DEFINE_PER_CPU_SHARED_ALIGNED(struct cfs_rq, init_tg_cfs_rq);
#endif /* CONFIG_FAIR_GROUP_SCHED */

#ifdef CONFIG_RT_GROUP_SCHED
static DEFINE_PER_CPU(struct sched_rt_entity, init_sched_rt_entity);
static DEFINE_PER_CPU_SHARED_ALIGNED(struct rt_rq, init_rt_rq);
#endif /* CONFIG_RT_GROUP_SCHED */
#else /* !CONFIG_USER_SCHED */
#define root_task_group init_task_group
#endif /* CONFIG_USER_SCHED */

/* task_group_lock serializes add/remove of task groups and also changes to
 * a task group's cpu shares.
 */
static DEFINE_SPINLOCK(task_group_lock);

#ifdef CONFIG_FAIR_GROUP_SCHED

#ifdef CONFIG_SMP
static int root_task_group_empty(void)
{
	return list_empty(&root_task_group.children);
}
#endif

#ifdef CONFIG_USER_SCHED
# define INIT_TASK_GROUP_LOAD	(2*NICE_0_LOAD)
#else /* !CONFIG_USER_SCHED */
# define INIT_TASK_GROUP_LOAD	NICE_0_LOAD
#endif /* CONFIG_USER_SCHED */

/*
 * A weight of 0 or 1 can cause arithmetics problems.
 * A weight of a cfs_rq is the sum of weights of which entities
 * are queued on this cfs_rq, so a weight of a entity should not be
 * too large, so as the shares value of a task group.
 * (The default weight is 1024 - so there's no practical
 *  limitation from this.)
 */
#define MIN_SHARES	2
#define MAX_SHARES	(1UL << 18)

static int init_task_group_load = INIT_TASK_GROUP_LOAD;
#endif

/* Default task group.
 *	Every task in system belong to this group at bootup.
 */
struct task_group init_task_group;

/* return group to which a task belongs */
static inline struct task_group *task_group(struct task_struct *p)
{
	struct task_group *tg;

#ifdef CONFIG_USER_SCHED
	rcu_read_lock();
	tg = __task_cred(p)->user->tg;
	rcu_read_unlock();
#elif defined(CONFIG_CGROUP_SCHED)
	tg = container_of(task_subsys_state(p, cpu_cgroup_subsys_id),
				struct task_group, css);
#else
	tg = &init_task_group;
#endif
	return tg;
}

/* Change a task's cfs_rq and parent entity if it moves across CPUs/groups */
static inline void set_task_rq(struct task_struct *p, unsigned int cpu)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	p->se.cfs_rq = task_group(p)->cfs_rq[cpu];
	p->se.parent = task_group(p)->se[cpu];
#endif

#ifdef CONFIG_RT_GROUP_SCHED
	p->rt.rt_rq  = task_group(p)->rt_rq[cpu];
	p->rt.parent = task_group(p)->rt_se[cpu];
#endif
}

#else

static inline void set_task_rq(struct task_struct *p, unsigned int cpu) { }
static inline struct task_group *task_group(struct task_struct *p)
{
	return NULL;
}

#endif	/* CONFIG_GROUP_SCHED */

/* CFS-related fields in a runqueue */
struct cfs_rq {
	struct load_weight load;
	unsigned long nr_running;

	u64 exec_clock;
	u64 min_vruntime;

	struct rb_root tasks_timeline;
	struct rb_node *rb_leftmost;

	struct list_head tasks;
	struct list_head *balance_iterator;

	/*
	 * 'curr' points to currently running entity on this cfs_rq.
	 * It is set to NULL otherwise (i.e when none are currently running).
	 */
	struct sched_entity *curr, *next, *last;

	unsigned int nr_spread_over;

#ifdef CONFIG_FAIR_GROUP_SCHED
	struct rq *rq;	/* cpu runqueue to which this cfs_rq is attached */

	/*
	 * leaf cfs_rqs are those that hold tasks (lowest schedulable entity in
	 * a hierarchy). Non-leaf lrqs hold other higher schedulable entities
	 * (like users, containers etc.)
	 *
	 * leaf_cfs_rq_list ties together list of leaf cfs_rq's in a cpu. This
	 * list is used during load balance.
	 */
	struct list_head leaf_cfs_rq_list;
	struct task_group *tg;	/* group that "owns" this runqueue */

#ifdef CONFIG_SMP
	/*
	 * the part of load.weight contributed by tasks
	 */
	unsigned long task_weight;

	/*
	 *   h_load = weight * f(tg)
	 *
	 * Where f(tg) is the recursive weight fraction assigned to
	 * this group.
	 */
	unsigned long h_load;

	/*
	 * this cpu's part of tg->shares
	 */
	unsigned long shares;

	/*
	 * load.weight at the time we set shares
	 */
	unsigned long rq_weight;
#endif
#endif
};

/* Real-Time classes' related field in a runqueue: */
struct rt_rq {
	struct rt_prio_array active;
	unsigned long rt_nr_running;
#if defined CONFIG_SMP || defined CONFIG_RT_GROUP_SCHED
	struct {
		int curr; /* highest queued rt task prio */
#ifdef CONFIG_SMP
		int next; /* next highest */
#endif
	} highest_prio;
#endif
#ifdef CONFIG_SMP
	unsigned long rt_nr_migratory;
	unsigned long rt_nr_total;
	int overloaded;
	struct plist_head pushable_tasks;
#endif
	int rt_throttled;
	u64 rt_time;
	u64 rt_runtime;
	/* Nests inside the rq lock: */
	spinlock_t rt_runtime_lock;

#ifdef CONFIG_RT_GROUP_SCHED
	unsigned long rt_nr_boosted;

	struct rq *rq;
	struct list_head leaf_rt_rq_list;
	struct task_group *tg;
	struct sched_rt_entity *rt_se;
#endif
};

#ifdef CONFIG_SMP

/*
 * We add the notion of a root-domain which will be used to define per-domain
 * variables. Each exclusive cpuset essentially defines an island domain by
 * fully partitioning the member cpus from any other cpuset. Whenever a new
 * exclusive cpuset is created, we also create and attach a new root-domain
 * object.
 *
 */
struct root_domain {
	atomic_t refcount;
	cpumask_var_t span;
	cpumask_var_t online;

	/*
	 * The "RT overload" flag: it gets set if a CPU has more than
	 * one runnable RT task.
	 */
	cpumask_var_t rto_mask;
	atomic_t rto_count;
#ifdef CONFIG_SMP
	struct cpupri cpupri;
#endif
};

/*
 * By default the system creates a single root-domain with all cpus as
 * members (mimicking the global state we have today).
 */
static struct root_domain def_root_domain;

#endif

/*
 * This is the main, per-CPU runqueue data structure.
 *
 * Locking rule: those places that want to lock multiple runqueues
 * (such as the load balancing or the thread migration code), lock
 * acquire operations must be ordered by ascending &runqueue.
 */
struct rq {
	/* runqueue lock: */
	spinlock_t lock;

	/*
	 * nr_running and cpu_load should be in the same cacheline because
	 * remote CPUs use both these fields when doing load calculation.
	 */
	unsigned long nr_running;
	#define CPU_LOAD_IDX_MAX 5
	unsigned long cpu_load[CPU_LOAD_IDX_MAX];
#ifdef CONFIG_NO_HZ
	unsigned long last_tick_seen;
	unsigned char in_nohz_recently;
#endif
	/* capture load from *all* tasks on this cpu: */
	struct load_weight load;
	unsigned long nr_load_updates;
	u64 nr_switches;
	u64 nr_migrations_in;

	struct cfs_rq cfs;
	struct rt_rq rt;

#ifdef CONFIG_FAIR_GROUP_SCHED
	/* list of leaf cfs_rq on this cpu: */
	struct list_head leaf_cfs_rq_list;
#endif
#ifdef CONFIG_RT_GROUP_SCHED
	struct list_head leaf_rt_rq_list;
#endif

	/*
	 * This is part of a global counter where only the total sum
	 * over all CPUs matters. A task can increase this counter on
	 * one CPU and if it got migrated afterwards it may decrease
	 * it on another CPU. Always updated under the runqueue lock:
	 */
	unsigned long nr_uninterruptible;

	struct task_struct *curr, *idle;
	unsigned long next_balance;
	struct mm_struct *prev_mm;

	u64 clock;

	atomic_t nr_iowait;

#ifdef CONFIG_SMP
	struct root_domain *rd;
	struct sched_domain *sd;

	unsigned char idle_at_tick;
	/* For active balancing */
	int post_schedule;
	int active_balance;
	int push_cpu;
	/* cpu of this runqueue: */
	int cpu;
	int online;

	unsigned long avg_load_per_task;

	struct task_struct *migration_thread;
	struct list_head migration_queue;

	u64 rt_avg;
	u64 age_stamp;
	u64 idle_stamp;
	u64 avg_idle;
#endif

	/* calc_load related fields */
	unsigned long calc_load_update;
	long calc_load_active;

#ifdef CONFIG_SCHED_HRTICK
#ifdef CONFIG_SMP
	int hrtick_csd_pending;
	struct call_single_data hrtick_csd;
#endif
	struct hrtimer hrtick_timer;
#endif

#ifdef CONFIG_SCHEDSTATS
	/* latency stats */
	struct sched_info rq_sched_info;
	unsigned long long rq_cpu_time;
	/* could above be rq->cfs_rq.exec_clock + rq->rt_rq.rt_runtime ? */

	/* sys_sched_yield() stats */
	unsigned int yld_count;

	/* schedule() stats */
	unsigned int sched_switch;
	unsigned int sched_count;
	unsigned int sched_goidle;

	/* try_to_wake_up() stats */
	unsigned int ttwu_count;
	unsigned int ttwu_local;

	/* BKL stats */
	unsigned int bkl_count;
#endif
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct rq, runqueues);

static inline
void check_preempt_curr(struct rq *rq, struct task_struct *p, int flags)
{
	rq->curr->sched_class->check_preempt_curr(rq, p, flags);
}

static inline int cpu_of(struct rq *rq)
{
#ifdef CONFIG_SMP
	return rq->cpu;
#else
	return 0;
#endif
}

/*
 * The domain tree (rq->sd) is protected by RCU's quiescent state transition.
 * See detach_destroy_domains: synchronize_sched for details.
 *
 * The domain tree of any CPU may only be accessed from within
 * preempt-disabled sections.
 */
#define for_each_domain(cpu, __sd) \
	for (__sd = rcu_dereference(cpu_rq(cpu)->sd); __sd; __sd = __sd->parent)

#define cpu_rq(cpu)		(&per_cpu(runqueues, (cpu)))
#define this_rq()		(&__get_cpu_var(runqueues))
#define task_rq(p)		cpu_rq(task_cpu(p))
#define cpu_curr(cpu)		(cpu_rq(cpu)->curr)
#define raw_rq()		(&__raw_get_cpu_var(runqueues))

inline void update_rq_clock(struct rq *rq)
{
	rq->clock = sched_clock_cpu(cpu_of(rq));
}

/*
 * Tunables that become constants when CONFIG_SCHED_DEBUG is off:
 */
#ifdef CONFIG_SCHED_DEBUG
# define const_debug __read_mostly
#else
# define const_debug static const
#endif

/**
 * runqueue_is_locked
 * @cpu: the processor in question.
 *
 * Returns true if the current cpu runqueue is locked.
 * This interface allows printk to be called with the runqueue lock
 * held and know whether or not it is OK to wake up the klogd.
 */
int runqueue_is_locked(int cpu)
{
	return spin_is_locked(&cpu_rq(cpu)->lock);
}

/*
 * Debugging: various feature bits
 */

#define SCHED_FEAT(name, enabled)	\
	__SCHED_FEAT_##name ,

enum {
#include "sched_features.h"
};

#undef SCHED_FEAT

#define SCHED_FEAT(name, enabled)	\
	(1UL << __SCHED_FEAT_##name) * enabled |

const_debug unsigned int sysctl_sched_features =
#include "sched_features.h"
	0;

#undef SCHED_FEAT

#ifdef CONFIG_SCHED_DEBUG
#define SCHED_FEAT(name, enabled)	\
	#name ,

static __read_mostly char *sched_feat_names[] = {
#include "sched_features.h"
	NULL
};

#undef SCHED_FEAT

static int sched_feat_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; sched_feat_names[i]; i++) {
		if (!(sysctl_sched_features & (1UL << i)))
			seq_puts(m, "NO_");
		seq_printf(m, "%s ", sched_feat_names[i]);
	}
	seq_puts(m, "\n");

	return 0;
}

static ssize_t
sched_feat_write(struct file *filp, const char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	char buf[64];
	char *cmp = buf;
	int neg = 0;
	int i;

	if (cnt > 63)
		cnt = 63;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	if (strncmp(buf, "NO_", 3) == 0) {
		neg = 1;
		cmp += 3;
	}

	for (i = 0; sched_feat_names[i]; i++) {
		int len = strlen(sched_feat_names[i]);

		if (strncmp(cmp, sched_feat_names[i], len) == 0) {
			if (neg)
				sysctl_sched_features &= ~(1UL << i);
			else
				sysctl_sched_features |= (1UL << i);
			break;
		}
	}

	if (!sched_feat_names[i])
		return -EINVAL;

	filp->f_pos += cnt;

	return cnt;
}

static int sched_feat_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sched_feat_show, NULL);
}

static const struct file_operations sched_feat_fops = {
	.open		= sched_feat_open,
	.write		= sched_feat_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static __init int sched_init_debug(void)
{
	debugfs_create_file("sched_features", 0644, NULL, NULL,
			&sched_feat_fops);

	return 0;
}
late_initcall(sched_init_debug);

#endif

#define sched_feat(x) (sysctl_sched_features & (1UL << __SCHED_FEAT_##x))

/*
 * Number of tasks to iterate in a single balance run.
 * Limited because this is done with IRQs disabled.
 */
const_debug unsigned int sysctl_sched_nr_migrate = 32;

/*
 * ratelimit for updating the group shares.
 * default: 0.25ms
 */
unsigned int sysctl_sched_shares_ratelimit = 250000;

/*
 * Inject some fuzzyness into changing the per-cpu group shares
 * this avoids remote rq-locks at the expense of fairness.
 * default: 4
 */
unsigned int sysctl_sched_shares_thresh = 4;

/*
 * period over which we average the RT time consumption, measured
 * in ms.
 *
 * default: 1s
 */
const_debug unsigned int sysctl_sched_time_avg = MSEC_PER_SEC;

/*
 * period over which we measure -rt task cpu usage in us.
 * default: 1s
 */
unsigned int sysctl_sched_rt_period = 1000000;

static __read_mostly int scheduler_running;

/*
 * part of the period that we allow rt tasks to run in us.
 * default: 0.95s
 */
int sysctl_sched_rt_runtime = 950000;

static inline u64 global_rt_period(void)
{
	return (u64)sysctl_sched_rt_period * NSEC_PER_USEC;
}

static inline u64 global_rt_runtime(void)
{
	if (sysctl_sched_rt_runtime < 0)
		return RUNTIME_INF;

	return (u64)sysctl_sched_rt_runtime * NSEC_PER_USEC;
}

#ifndef prepare_arch_switch
# define prepare_arch_switch(next)	do { } while (0)
#endif
#ifndef finish_arch_switch
# define finish_arch_switch(prev)	do { } while (0)
#endif

static inline int task_current(struct rq *rq, struct task_struct *p)
{
	return rq->curr == p;
}

#ifndef __ARCH_WANT_UNLOCKED_CTXSW
static inline int task_running(struct rq *rq, struct task_struct *p)
{
	return task_current(rq, p);
}

static inline void prepare_lock_switch(struct rq *rq, struct task_struct *next)
{
}

static inline void finish_lock_switch(struct rq *rq, struct task_struct *prev)
{
#ifdef CONFIG_DEBUG_SPINLOCK
	/* this is a valid case when another task releases the spinlock */
	rq->lock.owner = current;
#endif
	/*
	 * If we are tracking spinlock dependencies then we have to
	 * fix up the runqueue lock - which gets 'carried over' from
	 * prev into current:
	 */
	spin_acquire(&rq->lock.dep_map, 0, 0, _THIS_IP_);

	spin_unlock_irq(&rq->lock);
}

#else /* __ARCH_WANT_UNLOCKED_CTXSW */
static inline int task_running(struct rq *rq, struct task_struct *p)
{
#ifdef CONFIG_SMP
	return p->oncpu;
#else
	return task_current(rq, p);
#endif
}

static inline void prepare_lock_switch(struct rq *rq, struct task_struct *next)
{
#ifdef CONFIG_SMP
	/*
	 * We can optimise this out completely for !SMP, because the
	 * SMP rebalancing from interrupt is the only thing that cares
	 * here.
	 */
	next->oncpu = 1;
#endif
#ifdef __ARCH_WANT_INTERRUPTS_ON_CTXSW
	spin_unlock_irq(&rq->lock);
#else
	spin_unlock(&rq->lock);
#endif
}

static inline void finish_lock_switch(struct rq *rq, struct task_struct *prev)
{
#ifdef CONFIG_SMP
	/*
	 * After ->oncpu is cleared, the task can be moved to a different CPU.
	 * We must ensure this doesn't happen until the switch is completely
	 * finished.
	 */
	smp_wmb();
	prev->oncpu = 0;
#endif
#ifndef __ARCH_WANT_INTERRUPTS_ON_CTXSW
	local_irq_enable();
#endif
}
#endif /* __ARCH_WANT_UNLOCKED_CTXSW */

/*
 * __task_rq_lock - lock the runqueue a given task resides on.
 * Must be called interrupts disabled.
 */
static inline struct rq *__task_rq_lock(struct task_struct *p)
	__acquires(rq->lock)
{
	for (;;) {
		struct rq *rq = task_rq(p);
		spin_lock(&rq->lock);
		if (likely(rq == task_rq(p)))
			return rq;
		spin_unlock(&rq->lock);
	}
}

/*
 * task_rq_lock - lock the runqueue a given task resides on and disable
 * interrupts. Note the ordering: we can safely lookup the task_rq without
 * explicitly disabling preemption.
 */
static struct rq *task_rq_lock(struct task_struct *p, unsigned long *flags)
	__acquires(rq->lock)
{
	struct rq *rq;

	for (;;) {
		local_irq_save(*flags);
		rq = task_rq(p);
		spin_lock(&rq->lock);
		if (likely(rq == task_rq(p)))
			return rq;
		spin_unlock_irqrestore(&rq->lock, *flags);
	}
}

void task_rq_unlock_wait(struct task_struct *p)
{
	struct rq *rq = task_rq(p);

	smp_mb(); /* spin-unlock-wait is not a full memory barrier */
	spin_unlock_wait(&rq->lock);
}

static void __task_rq_unlock(struct rq *rq)
	__releases(rq->lock)
{
	spin_unlock(&rq->lock);
}

static inline void task_rq_unlock(struct rq *rq, unsigned long *flags)
	__releases(rq->lock)
{
	spin_unlock_irqrestore(&rq->lock, *flags);
}

/*
 * this_rq_lock - lock this runqueue and disable interrupts.
 */
static struct rq *this_rq_lock(void)
	__acquires(rq->lock)
{
	struct rq *rq;

	local_irq_disable();
	rq = this_rq();
	spin_lock(&rq->lock);

	return rq;
}

#ifdef CONFIG_SCHED_HRTICK
/*
 * Use HR-timers to deliver accurate preemption points.
 *
 * Its all a bit involved since we cannot program an hrt while holding the
 * rq->lock. So what we do is store a state in in rq->hrtick_* and ask for a
 * reschedule event.
 *
 * When we get rescheduled we reprogram the hrtick_timer outside of the
 * rq->lock.
 */

/*
 * Use hrtick when:
 *  - enabled by features
 *  - hrtimer is actually high res
 */
static inline int hrtick_enabled(struct rq *rq)
{
	if (!sched_feat(HRTICK))
		return 0;
	if (!cpu_active(cpu_of(rq)))
		return 0;
	return hrtimer_is_hres_active(&rq->hrtick_timer);
}

static void hrtick_clear(struct rq *rq)
{
	if (hrtimer_active(&rq->hrtick_timer))
		hrtimer_cancel(&rq->hrtick_timer);
}

/*
 * High-resolution timer tick.
 * Runs from hardirq context with interrupts disabled.
 */
static enum hrtimer_restart hrtick(struct hrtimer *timer)
{
	struct rq *rq = container_of(timer, struct rq, hrtick_timer);

	WARN_ON_ONCE(cpu_of(rq) != smp_processor_id());

	spin_lock(&rq->lock);
	update_rq_clock(rq);
	rq->curr->sched_class->task_tick(rq, rq->curr, 1);
	spin_unlock(&rq->lock);

	return HRTIMER_NORESTART;
}

#ifdef CONFIG_SMP
/*
 * called from hardirq (IPI) context
 */
static void __hrtick_start(void *arg)
{
	struct rq *rq = arg;

	spin_lock(&rq->lock);
	hrtimer_restart(&rq->hrtick_timer);
	rq->hrtick_csd_pending = 0;
	spin_unlock(&rq->lock);
}

/*
 * Called to set the hrtick timer state.
 *
 * called with rq->lock held and irqs disabled
 */
static void hrtick_start(struct rq *rq, u64 delay)
{
	struct hrtimer *timer = &rq->hrtick_timer;
	ktime_t time = ktime_add_ns(timer->base->get_time(), delay);

	hrtimer_set_expires(timer, time);

	if (rq == this_rq()) {
		hrtimer_restart(timer);
	} else if (!rq->hrtick_csd_pending) {
		__smp_call_function_single(cpu_of(rq), &rq->hrtick_csd, 0);
		rq->hrtick_csd_pending = 1;
	}
}

static int
hotplug_hrtick(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int cpu = (int)(long)hcpu;

	switch (action) {
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		hrtick_clear(cpu_rq(cpu));
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static __init void init_hrtick(void)
{
	hotcpu_notifier(hotplug_hrtick, 0);
}
#else
/*
 * Called to set the hrtick timer state.
 *
 * called with rq->lock held and irqs disabled
 */
static void hrtick_start(struct rq *rq, u64 delay)
{
	__hrtimer_start_range_ns(&rq->hrtick_timer, ns_to_ktime(delay), 0,
			HRTIMER_MODE_REL_PINNED, 0);
}

static inline void init_hrtick(void)
{
}
#endif /* CONFIG_SMP */

static void init_rq_hrtick(struct rq *rq)
{
#ifdef CONFIG_SMP
	rq->hrtick_csd_pending = 0;

	rq->hrtick_csd.flags = 0;
	rq->hrtick_csd.func = __hrtick_start;
	rq->hrtick_csd.info = rq;
#endif

	hrtimer_init(&rq->hrtick_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rq->hrtick_timer.function = hrtick;
}
#else	/* CONFIG_SCHED_HRTICK */
static inline void hrtick_clear(struct rq *rq)
{
}

static inline void init_rq_hrtick(struct rq *rq)
{
}

static inline void init_hrtick(void)
{
}
#endif	/* CONFIG_SCHED_HRTICK */

/*
 * resched_task - mark a task 'to be rescheduled now'.
 *
 * On UP this means the setting of the need_resched flag, on SMP it
 * might also involve a cross-CPU call to trigger the scheduler on
 * the target CPU.
 */
#ifdef CONFIG_SMP

#ifndef tsk_is_polling
#define tsk_is_polling(t) test_tsk_thread_flag(t, TIF_POLLING_NRFLAG)
#endif

static void resched_task(struct task_struct *p)
{
	int cpu;

	assert_spin_locked(&task_rq(p)->lock);

	if (test_tsk_need_resched(p))
		return;

	set_tsk_need_resched(p);

	cpu = task_cpu(p);
	if (cpu == smp_processor_id())
		return;

	/* NEED_RESCHED must be visible before we test polling */
	smp_mb();
	if (!tsk_is_polling(p))
		smp_send_reschedule(cpu);
}

static void resched_cpu(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags;

	if (!spin_trylock_irqsave(&rq->lock, flags))
		return;
	resched_task(cpu_curr(cpu));
	spin_unlock_irqrestore(&rq->lock, flags);
}

#ifdef CONFIG_NO_HZ
/*
 * When add_timer_on() enqueues a timer into the timer wheel of an
 * idle CPU then this timer might expire before the next timer event
 * which is scheduled to wake up that CPU. In case of a completely
 * idle system the next event might even be infinite time into the
 * future. wake_up_idle_cpu() ensures that the CPU is woken up and
 * leaves the inner idle loop so the newly added timer is taken into
 * account when the CPU goes back to idle and evaluates the timer
 * wheel for the next timer event.
 */
void wake_up_idle_cpu(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	if (cpu == smp_processor_id())
		return;

	/*
	 * This is safe, as this function is called with the timer
	 * wheel base lock of (cpu) held. When the CPU is on the way
	 * to idle and has not yet set rq->curr to idle then it will
	 * be serialized on the timer wheel base lock and take the new
	 * timer into account automatically.
	 */
	if (rq->curr != rq->idle)
		return;

	/*
	 * We can set TIF_RESCHED on the idle task of the other CPU
	 * lockless. The worst case is that the other CPU runs the
	 * idle task through an additional NOOP schedule()
	 */
	set_tsk_need_resched(rq->idle);

	/* NEED_RESCHED must be visible before we test polling */
	smp_mb();
	if (!tsk_is_polling(rq->idle))
		smp_send_reschedule(cpu);
}
#endif /* CONFIG_NO_HZ */

static u64 sched_avg_period(void)
{
	return (u64)sysctl_sched_time_avg * NSEC_PER_MSEC / 2;
}

static void sched_avg_update(struct rq *rq)
{
	s64 period = sched_avg_period();

	while ((s64)(rq->clock - rq->age_stamp) > period) {
		rq->age_stamp += period;
		rq->rt_avg /= 2;
	}
}

static void sched_rt_avg_update(struct rq *rq, u64 rt_delta)
{
	rq->rt_avg += rt_delta;
	sched_avg_update(rq);
}

#else /* !CONFIG_SMP */
static void resched_task(struct task_struct *p)
{
	assert_spin_locked(&task_rq(p)->lock);
	set_tsk_need_resched(p);
}

static void sched_rt_avg_update(struct rq *rq, u64 rt_delta)
{
}
#endif /* CONFIG_SMP */

#if BITS_PER_LONG == 32
# define WMULT_CONST	(~0UL)
#else
# define WMULT_CONST	(1UL << 32)
#endif

#define WMULT_SHIFT	32

/*
 * Shift right and round:
 */
#define SRR(x, y) (((x) + (1UL << ((y) - 1))) >> (y))

/*
 * delta *= weight / lw
 */
static unsigned long
calc_delta_mine(unsigned long delta_exec, unsigned long weight,
		struct load_weight *lw)
{
	u64 tmp;

	if (!lw->inv_weight) {
		if (BITS_PER_LONG > 32 && unlikely(lw->weight >= WMULT_CONST))
			lw->inv_weight = 1;
		else
			lw->inv_weight = 1 + (WMULT_CONST-lw->weight/2)
				/ (lw->weight+1);
	}

	tmp = (u64)delta_exec * weight;
	/*
	 * Check whether we'd overflow the 64-bit multiplication:
	 */
	if (unlikely(tmp > WMULT_CONST))
		tmp = SRR(SRR(tmp, WMULT_SHIFT/2) * lw->inv_weight,
			WMULT_SHIFT/2);
	else
		tmp = SRR(tmp * lw->inv_weight, WMULT_SHIFT);

	return (unsigned long)min(tmp, (u64)(unsigned long)LONG_MAX);
}

static inline void update_load_add(struct load_weight *lw, unsigned long inc)
{
	lw->weight += inc;
	lw->inv_weight = 0;
}

static inline void update_load_sub(struct load_weight *lw, unsigned long dec)
{
	lw->weight -= dec;
	lw->inv_weight = 0;
}

/*
 * To aid in avoiding the subversion of "niceness" due to uneven distribution
 * of tasks with abnormal "nice" values across CPUs the contribution that
 * each task makes to its run queue's load is weighted according to its
 * scheduling class and "nice" value. For SCHED_NORMAL tasks this is just a
 * scaled version of the new time slice allocation that they receive on time
 * slice expiry etc.
 */

#define WEIGHT_IDLEPRIO                3
#define WMULT_IDLEPRIO         1431655765

/*
 * Nice levels are multiplicative, with a gentle 10% change for every
 * nice level changed. I.e. when a CPU-bound task goes from nice 0 to
 * nice 1, it will get ~10% less CPU time than another CPU-bound task
 * that remained on nice 0.
 *
 * The "10% effect" is relative and cumulative: from _any_ nice level,
 * if you go up 1 level, it's -10% CPU usage, if you go down 1 level
 * it's +10% CPU usage. (to achieve that we use a multiplier of 1.25.
 * If a task goes up by ~10% and another task goes down by ~10% then
 * the relative distance between them is ~25%.)
 */
static const int prio_to_weight[40] = {
 /* -20 */     88761,     71755,     56483,     46273,     36291,
 /* -15 */     29154,     23254,     18705,     14949,     11916,
 /* -10 */      9548,      7620,      6100,      4904,      3906,
 /*  -5 */      3121,      2501,      1991,      1586,      1277,
 /*   0 */      1024,       820,       655,       526,       423,
 /*   5 */       335,       272,       215,       172,       137,
 /*  10 */       110,        87,        70,        56,        45,
 /*  15 */        36,        29,        23,        18,        15,
};

/*
 * Inverse (2^32/x) values of the prio_to_weight[] array, precalculated.
 *
 * In cases where the weight does not change often, we can use the
 * precalculated inverse to speed up arithmetics by turning divisions
 * into multiplications:
 */
static const u32 prio_to_wmult[40] = {
 /* -20 */     48388,     59856,     76040,     92818,    118348,
 /* -15 */    147320,    184698,    229616,    287308,    360437,
 /* -10 */    449829,    563644,    704093,    875809,   1099582,
 /*  -5 */   1376151,   1717300,   2157191,   2708050,   3363326,
 /*   0 */   4194304,   5237765,   6557202,   8165337,  10153587,
 /*   5 */  12820798,  15790321,  19976592,  24970740,  31350126,
 /*  10 */  39045157,  49367440,  61356676,  76695844,  95443717,
 /*  15 */ 119304647, 148102320, 186737708, 238609294, 286331153,
};

static void activate_task(struct rq *rq, struct task_struct *p, int wakeup);

/*
 * runqueue iterator, to support SMP load-balancing between different
 * scheduling classes, without having to expose their internal data
 * structures to the load-balancing proper:
 */
struct rq_iterator {
	void *arg;
	struct task_struct *(*start)(void *);
	struct task_struct *(*next)(void *);
};

#ifdef CONFIG_SMP
static unsigned long
balance_tasks(struct rq *this_rq, int this_cpu, struct rq *busiest,
	      unsigned long max_load_move, struct sched_domain *sd,
	      enum cpu_idle_type idle, int *all_pinned,
	      int *this_best_prio, struct rq_iterator *iterator);

static int
iter_move_one_task(struct rq *this_rq, int this_cpu, struct rq *busiest,
		   struct sched_domain *sd, enum cpu_idle_type idle,
		   struct rq_iterator *iterator);
#endif

/* Time spent by the tasks of the cpu accounting group executing in ... */
enum cpuacct_stat_index {
	CPUACCT_STAT_USER,	/* ... user mode */
	CPUACCT_STAT_SYSTEM,	/* ... kernel mode */

	CPUACCT_STAT_NSTATS,
};

#ifdef CONFIG_CGROUP_CPUACCT
static void cpuacct_charge(struct task_struct *tsk, u64 cputime);
static void cpuacct_update_stats(struct task_struct *tsk,
		enum cpuacct_stat_index idx, cputime_t val);
#else
static inline void cpuacct_charge(struct task_struct *tsk, u64 cputime) {}
static inline void cpuacct_update_stats(struct task_struct *tsk,
		enum cpuacct_stat_index idx, cputime_t val) {}
#endif

static inline void inc_cpu_load(struct rq *rq, unsigned long load)
{
	update_load_add(&rq->load, load);
}

static inline void dec_cpu_load(struct rq *rq, unsigned long load)
{
	update_load_sub(&rq->load, load);
}

#if (defined(CONFIG_SMP) && defined(CONFIG_FAIR_GROUP_SCHED)) || defined(CONFIG_RT_GROUP_SCHED)
typedef int (*tg_visitor)(struct task_group *, void *);

/*
 * Iterate the full tree, calling @down when first entering a node and @up when
 * leaving it for the final time.
 */
static int walk_tg_tree(tg_visitor down, tg_visitor up, void *data)
{
	struct task_group *parent, *child;
	int ret;

	rcu_read_lock();
	parent = &root_task_group;
down:
	ret = (*down)(parent, data);
	if (ret)
		goto out_unlock;
	list_for_each_entry_rcu(child, &parent->children, siblings) {
		parent = child;
		goto down;

up:
		continue;
	}
	ret = (*up)(parent, data);
	if (ret)
		goto out_unlock;

	child = parent;
	parent = parent->parent;
	if (parent)
		goto up;
out_unlock:
	rcu_read_unlock();

	return ret;
}

static int tg_nop(struct task_group *tg, void *data)
{
	return 0;
}
#endif

#ifdef CONFIG_SMP
/* Used instead of source_load when we know the type == 0 */
static unsigned long weighted_cpuload(const int cpu)
{
	return cpu_rq(cpu)->load.weight;
}

/*
 * Return a low guess at the load of a migration-source cpu weighted
 * according to the scheduling class and "nice" value.
 *
 * We want to under-estimate the load of migration sources, to
 * balance conservatively.
 */
static unsigned long source_load(int cpu, int type)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long total = weighted_cpuload(cpu);

	if (type == 0 || !sched_feat(LB_BIAS))
		return total;

	return min(rq->cpu_load[type-1], total);
}

/*
 * Return a high guess at the load of a migration-target cpu weighted
 * according to the scheduling class and "nice" value.
 */
static unsigned long target_load(int cpu, int type)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long total = weighted_cpuload(cpu);

	if (type == 0 || !sched_feat(LB_BIAS))
		return total;

	return max(rq->cpu_load[type-1], total);
}

static struct sched_group *group_of(int cpu)
{
	struct sched_domain *sd = rcu_dereference(cpu_rq(cpu)->sd);

	if (!sd)
		return NULL;

	return sd->groups;
}

static unsigned long power_of(int cpu)
{
	struct sched_group *group = group_of(cpu);

	if (!group)
		return SCHED_LOAD_SCALE;

	return group->cpu_power;
}

static int task_hot(struct task_struct *p, u64 now, struct sched_domain *sd);

static unsigned long cpu_avg_load_per_task(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long nr_running = ACCESS_ONCE(rq->nr_running);

	if (nr_running)
		rq->avg_load_per_task = rq->load.weight / nr_running;
	else
		rq->avg_load_per_task = 0;

	return rq->avg_load_per_task;
}

#ifdef CONFIG_FAIR_GROUP_SCHED

static __read_mostly unsigned long *update_shares_data;

static void __set_se_shares(struct sched_entity *se, unsigned long shares);

/*
 * Calculate and set the cpu's group shares.
 */
static void update_group_shares_cpu(struct task_group *tg, int cpu,
				    unsigned long sd_shares,
				    unsigned long sd_rq_weight,
				    unsigned long *usd_rq_weight)
{
	unsigned long shares, rq_weight;
	int boost = 0;

	rq_weight = usd_rq_weight[cpu];
	if (!rq_weight) {
		boost = 1;
		rq_weight = NICE_0_LOAD;
	}

	/*
	 *             \Sum_j shares_j * rq_weight_i
	 * shares_i =  -----------------------------
	 *                  \Sum_j rq_weight_j
	 */
	shares = (sd_shares * rq_weight) / sd_rq_weight;
	shares = clamp_t(unsigned long, shares, MIN_SHARES, MAX_SHARES);

	if (abs(shares - tg->se[cpu]->load.weight) >
			sysctl_sched_shares_thresh) {
		struct rq *rq = cpu_rq(cpu);
		unsigned long flags;

		spin_lock_irqsave(&rq->lock, flags);
		tg->cfs_rq[cpu]->rq_weight = boost ? 0 : rq_weight;
		tg->cfs_rq[cpu]->shares = boost ? 0 : shares;
		__set_se_shares(tg->se[cpu], shares);
		spin_unlock_irqrestore(&rq->lock, flags);
	}
}

/*
 * Re-compute the task group their per cpu shares over the given domain.
 * This needs to be done in a bottom-up fashion because the rq weight of a
 * parent group depends on the shares of its child groups.
 */
static int tg_shares_up(struct task_group *tg, void *data)
{
	unsigned long weight, rq_weight = 0, shares = 0;
	unsigned long *usd_rq_weight;
	struct sched_domain *sd = data;
	unsigned long flags;
	int i;

	if (!tg->se[0])
		return 0;

	local_irq_save(flags);
	usd_rq_weight = per_cpu_ptr(update_shares_data, smp_processor_id());

	for_each_cpu(i, sched_domain_span(sd)) {
		weight = tg->cfs_rq[i]->load.weight;
		usd_rq_weight[i] = weight;

		/*
		 * If there are currently no tasks on the cpu pretend there
		 * is one of average load so that when a new task gets to
		 * run here it will not get delayed by group starvation.
		 */
		if (!weight)
			weight = NICE_0_LOAD;

		rq_weight += weight;
		shares += tg->cfs_rq[i]->shares;
	}

	if ((!shares && rq_weight) || shares > tg->shares)
		shares = tg->shares;

	if (!sd->parent || !(sd->parent->flags & SD_LOAD_BALANCE))
		shares = tg->shares;

	for_each_cpu(i, sched_domain_span(sd))
		update_group_shares_cpu(tg, i, shares, rq_weight, usd_rq_weight);

	local_irq_restore(flags);

	return 0;
}

/*
 * Compute the cpu's hierarchical load factor for each task group.
 * This needs to be done in a top-down fashion because the load of a child
 * group is a fraction of its parents load.
 */
static int tg_load_down(struct task_group *tg, void *data)
{
	unsigned long load;
	long cpu = (long)data;

	if (!tg->parent) {
		load = cpu_rq(cpu)->load.weight;
	} else {
		load = tg->parent->cfs_rq[cpu]->h_load;
		load *= tg->cfs_rq[cpu]->shares;
		load /= tg->parent->cfs_rq[cpu]->load.weight + 1;
	}

	tg->cfs_rq[cpu]->h_load = load;

	return 0;
}

static void update_shares(struct sched_domain *sd)
{
	s64 elapsed;
	u64 now;

	if (root_task_group_empty())
		return;

	now = cpu_clock(raw_smp_processor_id());
	elapsed = now - sd->last_update;

	if (elapsed >= (s64)(u64)sysctl_sched_shares_ratelimit) {
		sd->last_update = now;
		walk_tg_tree(tg_nop, tg_shares_up, sd);
	}
}

static void update_shares_locked(struct rq *rq, struct sched_domain *sd)
{
	if (root_task_group_empty())
		return;

	spin_unlock(&rq->lock);
	update_shares(sd);
	spin_lock(&rq->lock);
}

static void update_h_load(long cpu)
{
	if (root_task_group_empty())
		return;

	walk_tg_tree(tg_load_down, tg_nop, (void *)cpu);
}

#else

static inline void update_shares(struct sched_domain *sd)
{
}

static inline void update_shares_locked(struct rq *rq, struct sched_domain *sd)
{
}

#endif

#ifdef CONFIG_PREEMPT

static void double_rq_lock(struct rq *rq1, struct rq *rq2);

/*
 * fair double_lock_balance: Safely acquires both rq->locks in a fair
 * way at the expense of forcing extra atomic operations in all
 * invocations.  This assures that the double_lock is acquired using the
 * same underlying policy as the spinlock_t on this architecture, which
 * reduces latency compared to the unfair variant below.  However, it
 * also adds more overhead and therefore may reduce throughput.
 */
static inline int _double_lock_balance(struct rq *this_rq, struct rq *busiest)
	__releases(this_rq->lock)
	__acquires(busiest->lock)
	__acquires(this_rq->lock)
{
	spin_unlock(&this_rq->lock);
	double_rq_lock(this_rq, busiest);

	return 1;
}

#else
/*
 * Unfair double_lock_balance: Optimizes throughput at the expense of
 * latency by eliminating extra atomic operations when the locks are
 * already in proper order on entry.  This favors lower cpu-ids and will
 * grant the double lock to lower cpus over higher ids under contention,
 * regardless of entry order into the function.
 */
static int _double_lock_balance(struct rq *this_rq, struct rq *busiest)
	__releases(this_rq->lock)
	__acquires(busiest->lock)
	__acquires(this_rq->lock)
{
	int ret = 0;

	if (unlikely(!spin_trylock(&busiest->lock))) {
		if (busiest < this_rq) {
			spin_unlock(&this_rq->lock);
			spin_lock(&busiest->lock);
			spin_lock_nested(&this_rq->lock, SINGLE_DEPTH_NESTING);
			ret = 1;
		} else
			spin_lock_nested(&busiest->lock, SINGLE_DEPTH_NESTING);
	}
	return ret;
}

#endif /* CONFIG_PREEMPT */

/*
 * double_lock_balance - lock the busiest runqueue, this_rq is locked already.
 */
static int double_lock_balance(struct rq *this_rq, struct rq *busiest)
{
	if (unlikely(!irqs_disabled())) {
		/* printk() doesn't work good under rq->lock */
		spin_unlock(&this_rq->lock);
		BUG_ON(1);
	}

	return _double_lock_balance(this_rq, busiest);
}

static inline void double_unlock_balance(struct rq *this_rq, struct rq *busiest)
	__releases(busiest->lock)
{
	spin_unlock(&busiest->lock);
	lock_set_subclass(&this_rq->lock.dep_map, 0, _RET_IP_);
}
#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
static void cfs_rq_set_shares(struct cfs_rq *cfs_rq, unsigned long shares)
{
#ifdef CONFIG_SMP
	cfs_rq->shares = shares;
#endif
}
#endif

static void calc_load_account_active(struct rq *this_rq);

#include "sched_stats.h"
#include "sched_idletask.c"
#include "sched_fair.c"
#include "sched_rt.c"
#ifdef CONFIG_SCHED_DEBUG
# include "sched_debug.c"
#endif

#define sched_class_highest (&rt_sched_class)
#define for_each_class(class) \
   for (class = sched_class_highest; class; class = class->next)

static void inc_nr_running(struct rq *rq)
{
	rq->nr_running++;
}

static void dec_nr_running(struct rq *rq)
{
	rq->nr_running--;
}

static void set_load_weight(struct task_struct *p)
{
	if (task_has_rt_policy(p)) {
		p->se.load.weight = prio_to_weight[0] * 2;
		p->se.load.inv_weight = prio_to_wmult[0] >> 1;
		return;
	}

	/*
	 * SCHED_IDLE tasks get minimal weight:
	 */
	if (p->policy == SCHED_IDLE) {
		p->se.load.weight = WEIGHT_IDLEPRIO;
		p->se.load.inv_weight = WMULT_IDLEPRIO;
		return;
	}

	p->se.load.weight = prio_to_weight[p->static_prio - MAX_RT_PRIO];
	p->se.load.inv_weight = prio_to_wmult[p->static_prio - MAX_RT_PRIO];
}

static void update_avg(u64 *avg, u64 sample)
{
	s64 diff = sample - *avg;
	*avg += diff >> 3;
}

static void enqueue_task(struct rq *rq, struct task_struct *p, int wakeup)
{
	if (wakeup)
		p->se.start_runtime = p->se.sum_exec_runtime;

	sched_info_queued(p);
	p->sched_class->enqueue_task(rq, p, wakeup);
	p->se.on_rq = 1;
}

static void dequeue_task(struct rq *rq, struct task_struct *p, int sleep)
{
	if (sleep) {
		if (p->se.last_wakeup) {
			update_avg(&p->se.avg_overlap,
				p->se.sum_exec_runtime - p->se.last_wakeup);
			p->se.last_wakeup = 0;
		} else {
			update_avg(&p->se.avg_wakeup,
				sysctl_sched_wakeup_granularity);
		}
	}

	sched_info_dequeued(p);
	p->sched_class->dequeue_task(rq, p, sleep);
	p->se.on_rq = 0;
}

/*
 * __normal_prio - return the priority that is based on the static prio
 */
static inline int __normal_prio(struct task_struct *p)
{
	return p->static_prio;
}

/*
 * Calculate the expected normal priority: i.e. priority
 * without taking RT-inheritance into account. Might be
 * boosted by interactivity modifiers. Changes upon fork,
 * setprio syscalls, and whenever the interactivity
 * estimator recalculates.
 */
static inline int normal_prio(struct task_struct *p)
{
	int prio;

	if (task_has_rt_policy(p))
		prio = MAX_RT_PRIO-1 - p->rt_priority;
	else
		prio = __normal_prio(p);
	return prio;
}

/*
 * Calculate the current priority, i.e. the priority
 * taken into account by the scheduler. This value might
 * be boosted by RT tasks, or might be boosted by
 * interactivity modifiers. Will be RT if the task got
 * RT-boosted. If not then it returns p->normal_prio.
 */
static int effective_prio(struct task_struct *p)
{
	p->normal_prio = normal_prio(p);
	/*
	 * If we are RT tasks or we were boosted to RT priority,
	 * keep the priority unchanged. Otherwise, update priority
	 * to the normal priority:
	 */
	if (!rt_prio(p->prio))
		return p->normal_prio;
	return p->prio;
}

/*
 * activate_task - move a task to the runqueue.
 */
static void activate_task(struct rq *rq, struct task_struct *p, int wakeup)
{
	if (task_contributes_to_load(p))
		rq->nr_uninterruptible--;

	enqueue_task(rq, p, wakeup);
	inc_nr_running(rq);
}

/*
 * deactivate_task - remove a task from the runqueue.
 */
static void deactivate_task(struct rq *rq, struct task_struct *p, int sleep)
{
	if (task_contributes_to_load(p))
		rq->nr_uninterruptible++;

	dequeue_task(rq, p, sleep);
	dec_nr_running(rq);
}

/**
 * task_curr - is this task currently executing on a CPU?
 * @p: the task in question.
 */
inline int task_curr(const struct task_struct *p)
{
	return cpu_curr(task_cpu(p)) == p;
}

static inline void __set_task_cpu(struct task_struct *p, unsigned int cpu)
{
	set_task_rq(p, cpu);
#ifdef CONFIG_SMP
	/*
	 * After ->cpu is set up to a new value, task_rq_lock(p, ...) can be
	 * successfuly executed on another CPU. We must ensure that updates of
	 * per-task data have been completed by this moment.
	 */
	smp_wmb();
	task_thread_info(p)->cpu = cpu;
#endif
}

static inline void check_class_changed(struct rq *rq, struct task_struct *p,
				       const struct sched_class *prev_class,
				       int oldprio, int running)
{
	if (prev_class != p->sched_class) {
		if (prev_class->switched_from)
			prev_class->switched_from(rq, p, running);
		p->sched_class->switched_to(rq, p, running);
	} else
		p->sched_class->prio_changed(rq, p, oldprio, running);
}

/**
 * kthread_bind - bind a just-created kthread to a cpu.
 * @p: thread created by kthread_create().
 * @cpu: cpu (might not be online, must be possible) for @k to run on.
 *
 * Description: This function is equivalent to set_cpus_allowed(),
 * except that @cpu doesn't need to be online, and the thread must be
 * stopped (i.e., just returned from kthread_create()).
 *
 * Function lives here instead of kthread.c because it messes with
 * scheduler internals which require locking.
 */
void kthread_bind(struct task_struct *p, unsigned int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags;

	/* Must have done schedule() in kthread() before we set_task_cpu */
	if (!wait_task_inactive(p, TASK_UNINTERRUPTIBLE)) {
		WARN_ON(1);
		return;
	}

	spin_lock_irqsave(&rq->lock, flags);
	set_task_cpu(p, cpu);
	p->cpus_allowed = cpumask_of_cpu(cpu);
	p->rt.nr_cpus_allowed = 1;
	p->flags |= PF_THREAD_BOUND;
	spin_unlock_irqrestore(&rq->lock, flags);
}
EXPORT_SYMBOL(kthread_bind);

#ifdef CONFIG_SMP
/*
 * Is this task likely cache-hot:
 */
static int
task_hot(struct task_struct *p, u64 now, struct sched_domain *sd)
{
	s64 delta;

	/*
	 * Buddy candidates are cache hot:
	 */
	if (sched_feat(CACHE_HOT_BUDDY) && this_rq()->nr_running &&
			(&p->se == cfs_rq_of(&p->se)->next ||
			 &p->se == cfs_rq_of(&p->se)->last))
		return 1;

	if (p->sched_class != &fair_sched_class)
		return 0;

	if (sysctl_sched_migration_cost == -1)
		return 1;
	if (sysctl_sched_migration_cost == 0)
		return 0;

	delta = now - p->se.exec_start;

	return delta < (s64)sysctl_sched_migration_cost;
}


void set_task_cpu(struct task_struct *p, unsigned int new_cpu)
{
	int old_cpu = task_cpu(p);
	struct rq *old_rq = cpu_rq(old_cpu), *new_rq = cpu_rq(new_cpu);
	struct cfs_rq *old_cfsrq = task_cfs_rq(p),
		      *new_cfsrq = cpu_cfs_rq(old_cfsrq, new_cpu);
	u64 clock_offset;

	clock_offset = old_rq->clock - new_rq->clock;

	trace_sched_migrate_task(p, new_cpu);

#ifdef CONFIG_SCHEDSTATS
	if (p->se.wait_start)
		p->se.wait_start -= clock_offset;
	if (p->se.sleep_start)
		p->se.sleep_start -= clock_offset;
	if (p->se.block_start)
		p->se.block_start -= clock_offset;
#endif
	if (old_cpu != new_cpu) {
		p->se.nr_migrations++;
		new_rq->nr_migrations_in++;
#ifdef CONFIG_SCHEDSTATS
		if (task_hot(p, old_rq->clock, NULL))
			schedstat_inc(p, se.nr_forced2_migrations);
#endif
		perf_sw_event(PERF_COUNT_SW_CPU_MIGRATIONS,
				     1, 1, NULL, 0);
	}
	p->se.vruntime -= old_cfsrq->min_vruntime -
					 new_cfsrq->min_vruntime;

	__set_task_cpu(p, new_cpu);
}

struct migration_req {
	struct list_head list;

	struct task_struct *task;
	int dest_cpu;

	struct completion done;
};

/*
 * The task's runqueue lock must be held.
 * Returns true if you have to wait for migration thread.
 */
static int
migrate_task(struct task_struct *p, int dest_cpu, struct migration_req *req)
{
	struct rq *rq = task_rq(p);

	/*
	 * If the task is not on a runqueue (and not running), then
	 * it is sufficient to simply update the task's cpu field.
	 */
	if (!p->se.on_rq && !task_running(rq, p)) {
		set_task_cpu(p, dest_cpu);
		return 0;
	}

	init_completion(&req->done);
	req->task = p;
	req->dest_cpu = dest_cpu;
	list_add(&req->list, &rq->migration_queue);

	return 1;
}

/*
 * wait_task_context_switch -	wait for a thread to complete at least one
 *				context switch.
 *
 * @p must not be current.
 */
void wait_task_context_switch(struct task_struct *p)
{
	unsigned long nvcsw, nivcsw, flags;
	int running;
	struct rq *rq;

	nvcsw	= p->nvcsw;
	nivcsw	= p->nivcsw;
	for (;;) {
		/*
		 * The runqueue is assigned before the actual context
		 * switch. We need to take the runqueue lock.
		 *
		 * We could check initially without the lock but it is
		 * very likely that we need to take the lock in every
		 * iteration.
		 */
		rq = task_rq_lock(p, &flags);
		running = task_running(rq, p);
		task_rq_unlock(rq, &flags);

		if (likely(!running))
			break;
		/*
		 * The switch count is incremented before the actual
		 * context switch. We thus wait for two switches to be
		 * sure at least one completed.
		 */
		if ((p->nvcsw - nvcsw) > 1)
			break;
		if ((p->nivcsw - nivcsw) > 1)
			break;

		cpu_relax();
	}
}

/*
 * wait_task_inactive - wait for a thread to unschedule.
 *
 * If @match_state is nonzero, it's the @p->state value just checked and
 * not expected to change.  If it changes, i.e. @p might have woken up,
 * then return zero.  When we succeed in waiting for @p to be off its CPU,
 * we return a positive number (its total switch count).  If a second call
 * a short while later returns the same number, the caller can be sure that
 * @p has remained unscheduled the whole time.
 *
 * The caller must ensure that the task *will* unschedule sometime soon,
 * else this function might spin for a *long* time. This function can't
 * be called with interrupts off, or it may introduce deadlock with
 * smp_call_function() if an IPI is sent by the same process we are
 * waiting to become inactive.
 */
unsigned long wait_task_inactive(struct task_struct *p, long match_state)
{
	unsigned long flags;
	int running, on_rq;
	unsigned long ncsw;
	struct rq *rq;

	for (;;) {
		/*
		 * We do the initial early heuristics without holding
		 * any task-queue locks at all. We'll only try to get
		 * the runqueue lock when things look like they will
		 * work out!
		 */
		rq = task_rq(p);

		/*
		 * If the task is actively running on another CPU
		 * still, just relax and busy-wait without holding
		 * any locks.
		 *
		 * NOTE! Since we don't hold any locks, it's not
		 * even sure that "rq" stays as the right runqueue!
		 * But we don't care, since "task_running()" will
		 * return false if the runqueue has changed and p
		 * is actually now running somewhere else!
		 */
		while (task_running(rq, p)) {
			if (match_state && unlikely(p->state != match_state))
				return 0;
			cpu_relax();
		}

		/*
		 * Ok, time to look more closely! We need the rq
		 * lock now, to be *sure*. If we're wrong, we'll
		 * just go back and repeat.
		 */
		rq = task_rq_lock(p, &flags);
		trace_sched_wait_task(rq, p);
		running = task_running(rq, p);
		on_rq = p->se.on_rq;
		ncsw = 0;
		if (!match_state || p->state == match_state)
			ncsw = p->nvcsw | LONG_MIN; /* sets MSB */
		task_rq_unlock(rq, &flags);

		/*
		 * If it changed from the expected state, bail out now.
		 */
		if (unlikely(!ncsw))
			break;

		/*
		 * Was it really running after all now that we
		 * checked with the proper locks actually held?
		 *
		 * Oops. Go back and try again..
		 */
		if (unlikely(running)) {
			cpu_relax();
			continue;
		}

		/*
		 * It's not enough that it's not actively running,
		 * it must be off the runqueue _entirely_, and not
		 * preempted!
		 *
		 * So if it was still runnable (but just not actively
		 * running right now), it's preempted, and we should
		 * yield - it could be a while.
		 */
		if (unlikely(on_rq)) {
			schedule_timeout_uninterruptible(1);
			continue;
		}

		/*
		 * Ahh, all good. It wasn't running, and it wasn't
		 * runnable, which means that it will never become
		 * running in the future either. We're all done!
		 */
		break;
	}

	return ncsw;
}

/***
 * kick_process - kick a running thread to enter/exit the kernel
 * @p: the to-be-kicked thread
 *
 * Cause a process which is running on another CPU to enter
 * kernel-mode, without any delay. (to get signals handled.)
 *
 * NOTE: this function doesnt have to take the runqueue lock,
 * because all it wants to ensure is that the remote task enters
 * the kernel. If the IPI races and the task has been migrated
 * to another CPU then no harm is done and the purpose has been
 * achieved as well.
 */
void kick_process(struct task_struct *p)
{
	int cpu;

	preempt_disable();
	cpu = task_cpu(p);
	if ((cpu != smp_processor_id()) && task_curr(p))
		smp_send_reschedule(cpu);
	preempt_enable();
}
EXPORT_SYMBOL_GPL(kick_process);
#endif /* CONFIG_SMP */

/**
 * task_oncpu_function_call - call a function on the cpu on which a task runs
 * @p:		the task to evaluate
 * @func:	the function to be called
 * @info:	the function call argument
 *
 * Calls the function @func when the task is currently running. This might
 * be on the current CPU, which just calls the function directly
 */
void task_oncpu_function_call(struct task_struct *p,
			      void (*func) (void *info), void *info)
{
	int cpu;

	preempt_disable();
	cpu = task_cpu(p);
	if (task_curr(p))
		smp_call_function_single(cpu, func, info, 1);
	preempt_enable();
}

/***
 * try_to_wake_up - wake up a thread
 * @p: the to-be-woken-up thread
 * @state: the mask of task states that can be woken
 * @sync: do a synchronous wakeup?
 *
 * Put it on the run-queue if it's not already there. The "current"
 * thread is always on the run-queue (except when the actual
 * re-schedule is in progress), and as such you're allowed to do
 * the simpler "current->state = TASK_RUNNING" to mark yourself
 * runnable without the overhead of this.
 *
 * returns failure only if the task is already active.
 */
static int try_to_wake_up(struct task_struct *p, unsigned int state,
			  int wake_flags)
{
	int cpu, orig_cpu, this_cpu, success = 0;
	unsigned long flags;
	struct rq *rq, *orig_rq;

	if (!sched_feat(SYNC_WAKEUPS))
		wake_flags &= ~WF_SYNC;

	this_cpu = get_cpu();

	smp_wmb();
	rq = orig_rq = task_rq_lock(p, &flags);
	update_rq_clock(rq);
	if (!(p->state & state))
		goto out;

	if (p->se.on_rq)
		goto out_running;

	cpu = task_cpu(p);
	orig_cpu = cpu;

#ifdef CONFIG_SMP
	if (unlikely(task_running(rq, p)))
		goto out_activate;

	/*
	 * In order to handle concurrent wakeups and release the rq->lock
	 * we put the task in TASK_WAKING state.
	 *
	 * First fix up the nr_uninterruptible count:
	 */
	if (task_contributes_to_load(p))
		rq->nr_uninterruptible--;
	p->state = TASK_WAKING;
	task_rq_unlock(rq, &flags);

	cpu = p->sched_class->select_task_rq(p, SD_BALANCE_WAKE, wake_flags);
	if (cpu != orig_cpu)
		set_task_cpu(p, cpu);

	rq = task_rq_lock(p, &flags);

	if (rq != orig_rq)
		update_rq_clock(rq);

	WARN_ON(p->state != TASK_WAKING);
	cpu = task_cpu(p);

#ifdef CONFIG_SCHEDSTATS
	schedstat_inc(rq, ttwu_count);
	if (cpu == this_cpu)
		schedstat_inc(rq, ttwu_local);
	else {
		struct sched_domain *sd;
		for_each_domain(this_cpu, sd) {
			if (cpumask_test_cpu(cpu, sched_domain_span(sd))) {
				schedstat_inc(sd, ttwu_wake_remote);
				break;
			}
		}
	}
#endif /* CONFIG_SCHEDSTATS */

out_activate:
#endif /* CONFIG_SMP */
	schedstat_inc(p, se.nr_wakeups);
	if (wake_flags & WF_SYNC)
		schedstat_inc(p, se.nr_wakeups_sync);
	if (orig_cpu != cpu)
		schedstat_inc(p, se.nr_wakeups_migrate);
	if (cpu == this_cpu)
		schedstat_inc(p, se.nr_wakeups_local);
	else
		schedstat_inc(p, se.nr_wakeups_remote);
	activate_task(rq, p, 1);
	success = 1;

	/*
	 * Only attribute actual wakeups done by this task.
	 */
	if (!in_interrupt()) {
		struct sched_entity *se = &current->se;
		u64 sample = se->sum_exec_runtime;

		if (se->last_wakeup)
			sample -= se->last_wakeup;
		else
			sample -= se->start_runtime;
		update_avg(&se->avg_wakeup, sample);

		se->last_wakeup = se->sum_exec_runtime;
	}

out_running:
	trace_sched_wakeup(rq, p, success);
	check_preempt_curr(rq, p, wake_flags);

	p->state = TASK_RUNNING;
#ifdef CONFIG_SMP
	if (p->sched_class->task_wake_up)
		p->sched_class->task_wake_up(rq, p);

	if (unlikely(rq->idle_stamp)) {
		u64 delta = rq->clock - rq->idle_stamp;
		u64 max = 2*sysctl_sched_migration_cost;

		if (delta > max)
			rq->avg_idle = max;
		else
			update_avg(&rq->avg_idle, delta);
		rq->idle_stamp = 0;
	}
#endif
out:
	task_rq_unlock(rq, &flags);
	put_cpu();

	return success;
}

/**
 * wake_up_process - Wake up a specific process
 * @p: The process to be woken up.
 *
 * Attempt to wake up the nominated process and move it to the set of runnable
 * processes.  Returns 1 if the process was woken up, 0 if it was already
 * running.
 *
 * It may be assumed that this function implies a write memory barrier before
 * changing the task state if and only if any tasks are woken up.
 */
int wake_up_process(struct task_struct *p)
{
	return try_to_wake_up(p, TASK_ALL, 0);
}
EXPORT_SYMBOL(wake_up_process);

int wake_up_state(struct task_struct *p, unsigned int state)
{
	return try_to_wake_up(p, state, 0);
}

/*
 * Perform scheduler related setup for a newly forked process p.
 * p is forked by current.
 *
 * __sched_fork() is basic setup used by init_idle() too:
 */
static void __sched_fork(struct task_struct *p)
{
	p->se.exec_start		= 0;
	p->se.sum_exec_runtime		= 0;
	p->se.prev_sum_exec_runtime	= 0;
	p->se.nr_migrations		= 0;
	p->se.last_wakeup		= 0;
	p->se.avg_overlap		= 0;
	p->se.start_runtime		= 0;
	p->se.avg_wakeup		= sysctl_sched_wakeup_granularity;
	p->se.avg_running		= 0;

#ifdef CONFIG_SCHEDSTATS
	p->se.wait_start			= 0;
	p->se.wait_max				= 0;
	p->se.wait_count			= 0;
	p->se.wait_sum				= 0;

	p->se.sleep_start			= 0;
	p->se.sleep_max				= 0;
	p->se.sum_sleep_runtime			= 0;

	p->se.block_start			= 0;
	p->se.block_max				= 0;
	p->se.exec_max				= 0;
	p->se.slice_max				= 0;

	p->se.nr_migrations_cold		= 0;
	p->se.nr_failed_migrations_affine	= 0;
	p->se.nr_failed_migrations_running	= 0;
	p->se.nr_failed_migrations_hot		= 0;
	p->se.nr_forced_migrations		= 0;
	p->se.nr_forced2_migrations		= 0;

	p->se.nr_wakeups			= 0;
	p->se.nr_wakeups_sync			= 0;
	p->se.nr_wakeups_migrate		= 0;
	p->se.nr_wakeups_local			= 0;
	p->se.nr_wakeups_remote			= 0;
	p->se.nr_wakeups_affine			= 0;
	p->se.nr_wakeups_affine_attempts	= 0;
	p->se.nr_wakeups_passive		= 0;
	p->se.nr_wakeups_idle			= 0;

#endif

	INIT_LIST_HEAD(&p->rt.run_list);
	p->se.on_rq = 0;
	INIT_LIST_HEAD(&p->se.group_node);

#ifdef CONFIG_PREEMPT_NOTIFIERS
	INIT_HLIST_HEAD(&p->preempt_notifiers);
#endif

	/*
	 * We mark the process as running here, but have not actually
	 * inserted it onto the runqueue yet. This guarantees that
	 * nobody will actually run it, and a signal or other external
	 * event cannot wake it up and insert it on the runqueue either.
	 */
	p->state = TASK_RUNNING;
}

/*
 * fork()/clone()-time setup:
 */
void sched_fork(struct task_struct *p, int clone_flags)
{
	int cpu = get_cpu();

	__sched_fork(p);

	/*
	 * Revert to default priority/policy on fork if requested.
	 */
	if (unlikely(p->sched_reset_on_fork)) {
		if (p->policy == SCHED_FIFO || p->policy == SCHED_RR) {
			p->policy = SCHED_NORMAL;
			p->normal_prio = p->static_prio;
		}

		if (PRIO_TO_NICE(p->static_prio) < 0) {
			p->static_prio = NICE_TO_PRIO(0);
			p->normal_prio = p->static_prio;
			set_load_weight(p);
		}

		/*
		 * We don't need the reset flag anymore after the fork. It has
		 * fulfilled its duty:
		 */
		p->sched_reset_on_fork = 0;
	}

	/*
	 * Make sure we do not leak PI boosting priority to the child.
	 */
	p->prio = current->normal_prio;

	if (!rt_prio(p->prio))
		p->sched_class = &fair_sched_class;

#ifdef CONFIG_SMP
	cpu = p->sched_class->select_task_rq(p, SD_BALANCE_FORK, 0);
#endif
	set_task_cpu(p, cpu);

#if defined(CONFIG_SCHEDSTATS) || defined(CONFIG_TASK_DELAY_ACCT)
	if (likely(sched_info_on()))
		memset(&p->sched_info, 0, sizeof(p->sched_info));
#endif
#if defined(CONFIG_SMP) && defined(__ARCH_WANT_UNLOCKED_CTXSW)
	p->oncpu = 0;
#endif
#ifdef CONFIG_PREEMPT
	/* Want to start with kernel preemption disabled. */
	task_thread_info(p)->preempt_count = 1;
#endif
	plist_node_init(&p->pushable_tasks, MAX_PRIO);

	put_cpu();
}

/*
 * wake_up_new_task - wake up a newly created task for the first time.
 *
 * This function will do some initial scheduler statistics housekeeping
 * that must be done for every newly created context, then puts the task
 * on the runqueue and wakes it.
 */
void wake_up_new_task(struct task_struct *p, unsigned long clone_flags)
{
	unsigned long flags;
	struct rq *rq;

	rq = task_rq_lock(p, &flags);
	BUG_ON(p->state != TASK_RUNNING);
	update_rq_clock(rq);

	if (!p->sched_class->task_new || !current->se.on_rq) {
		activate_task(rq, p, 0);
	} else {
		/*
		 * Let the scheduling class do new task startup
		 * management (if any):
		 */
		p->sched_class->task_new(rq, p);
		inc_nr_running(rq);
	}
	trace_sched_wakeup_new(rq, p, 1);
	check_preempt_curr(rq, p, WF_FORK);
#ifdef CONFIG_SMP
	if (p->sched_class->task_wake_up)
		p->sched_class->task_wake_up(rq, p);
#endif
	task_rq_unlock(rq, &flags);
}

#ifdef CONFIG_PREEMPT_NOTIFIERS

/**
 * preempt_notifier_register - tell me when current is being preempted & rescheduled
 * @notifier: notifier struct to register
 */
void preempt_notifier_register(struct preempt_notifier *notifier)
{
	hlist_add_head(&notifier->link, &current->preempt_notifiers);
}
EXPORT_SYMBOL_GPL(preempt_notifier_register);

/**
 * preempt_notifier_unregister - no longer interested in preemption notifications
 * @notifier: notifier struct to unregister
 *
 * This is safe to call from within a preemption notifier.
 */
void preempt_notifier_unregister(struct preempt_notifier *notifier)
{
	hlist_del(&notifier->link);
}
EXPORT_SYMBOL_GPL(preempt_notifier_unregister);

static void fire_sched_in_preempt_notifiers(struct task_struct *curr)
{
	struct preempt_notifier *notifier;
	struct hlist_node *node;

	hlist_for_each_entry(notifier, node, &curr->preempt_notifiers, link)
		notifier->ops->sched_in(notifier, raw_smp_processor_id());
}

static void
fire_sched_out_preempt_notifiers(struct task_struct *curr,
				 struct task_struct *next)
{
	struct preempt_notifier *notifier;
	struct hlist_node *node;

	hlist_for_each_entry(notifier, node, &curr->preempt_notifiers, link)
		notifier->ops->sched_out(notifier, next);
}

#else /* !CONFIG_PREEMPT_NOTIFIERS */

static void fire_sched_in_preempt_notifiers(struct task_struct *curr)
{
}

static void
fire_sched_out_preempt_notifiers(struct task_struct *curr,
				 struct task_struct *next)
{
}

#endif /* CONFIG_PREEMPT_NOTIFIERS */

/**
 * prepare_task_switch - prepare to switch tasks
 * @rq: the runqueue preparing to switch
 * @prev: the current task that is being switched out
 * @next: the task we are going to switch to.
 *
 * This is called with the rq lock held and interrupts off. It must
 * be paired with a subsequent finish_task_switch after the context
 * switch.
 *
 * prepare_task_switch sets up locking and calls architecture specific
 * hooks.
 */
static inline void
prepare_task_switch(struct rq *rq, struct task_struct *prev,
		    struct task_struct *next)
{
	fire_sched_out_preempt_notifiers(prev, next);
	prepare_lock_switch(rq, next);
	prepare_arch_switch(next);
}

/**
 * finish_task_switch - clean up after a task-switch
 * @rq: runqueue associated with task-switch
 * @prev: the thread we just switched away from.
 *
 * finish_task_switch must be called after the context switch, paired
 * with a prepare_task_switch call before the context switch.
 * finish_task_switch will reconcile locking set up by prepare_task_switch,
 * and do any other architecture-specific cleanup actions.
 *
 * Note that we may have delayed dropping an mm in context_switch(). If
 * so, we finish that here outside of the runqueue lock. (Doing it
 * with the lock held can cause deadlocks; see schedule() for
 * details.)
 */
static void finish_task_switch(struct rq *rq, struct task_struct *prev)
	__releases(rq->lock)
{
	struct mm_struct *mm = rq->prev_mm;
	long prev_state;

	rq->prev_mm = NULL;

	/*
	 * A task struct has one reference for the use as "current".
	 * If a task dies, then it sets TASK_DEAD in tsk->state and calls
	 * schedule one last time. The schedule call will never return, and
	 * the scheduled task must drop that reference.
	 * The test for TASK_DEAD must occur while the runqueue locks are
	 * still held, otherwise prev could be scheduled on another cpu, die
	 * there before we look at prev->state, and then the reference would
	 * be dropped twice.
	 *		Manfred Spraul <manfred@colorfullife.com>
	 */
	prev_state = prev->state;
	finish_arch_switch(prev);
	perf_event_task_sched_in(current, cpu_of(rq));
	finish_lock_switch(rq, prev);

	fire_sched_in_preempt_notifiers(current);
	if (mm)
		mmdrop(mm);
	if (unlikely(prev_state == TASK_DEAD)) {
		/*
		 * Remove function-return probe instances associated with this
		 * task and put them back on the free list.
		 */
		kprobe_flush_task(prev);
		put_task_struct(prev);
	}
}

#ifdef CONFIG_SMP

/* assumes rq->lock is held */
static inline void pre_schedule(struct rq *rq, struct task_struct *prev)
{
	if (prev->sched_class->pre_schedule)
		prev->sched_class->pre_schedule(rq, prev);
}

/* rq->lock is NOT held, but preemption is disabled */
static inline void post_schedule(struct rq *rq)
{
	if (rq->post_schedule) {
		unsigned long flags;

		spin_lock_irqsave(&rq->lock, flags);
		if (rq->curr->sched_class->post_schedule)
			rq->curr->sched_class->post_schedule(rq);
		spin_unlock_irqrestore(&rq->lock, flags);

		rq->post_schedule = 0;
	}
}

#else

static inline void pre_schedule(struct rq *rq, struct task_struct *p)
{
}

static inline void post_schedule(struct rq *rq)
{
}

#endif

/**
 * schedule_tail - first thing a freshly forked thread must call.
 * @prev: the thread we just switched away from.
 */
asmlinkage void schedule_tail(struct task_struct *prev)
	__releases(rq->lock)
{
	struct rq *rq = this_rq();

	finish_task_switch(rq, prev);

	/*
	 * FIXME: do we need to worry about rq being invalidated by the
	 * task_switch?
	 */
	post_schedule(rq);

#ifdef __ARCH_WANT_UNLOCKED_CTXSW
	/* In this case, finish_task_switch does not reenable preemption */
	preempt_enable();
#endif
	if (current->set_child_tid)
		put_user(task_pid_vnr(current), current->set_child_tid);
}

/*
 * context_switch - switch to the new MM and the new
 * thread's register state.
 */
static inline void
context_switch(struct rq *rq, struct task_struct *prev,
	       struct task_struct *next)
{
	struct mm_struct *mm, *oldmm;

	prepare_task_switch(rq, prev, next);
	trace_sched_switch(rq, prev, next);
	mm = next->mm;
	oldmm = prev->active_mm;
	/*
	 * For paravirt, this is coupled with an exit in switch_to to
	 * combine the page table reload and the switch backend into
	 * one hypercall.
	 */
	arch_start_context_switch(prev);

	if (unlikely(!mm)) {
		next->active_mm = oldmm;
		atomic_inc(&oldmm->mm_count);
		enter_lazy_tlb(oldmm, next);
	} else
		switch_mm(oldmm, mm, next);

	if (unlikely(!prev->mm)) {
		prev->active_mm = NULL;
		rq->prev_mm = oldmm;
	}
	/*
	 * Since the runqueue lock will be released by the next
	 * task (which is an invalid locking op but in the case
	 * of the scheduler it's an obvious special-case), so we
	 * do an early lockdep release here:
	 */
#ifndef __ARCH_WANT_UNLOCKED_CTXSW
	spin_release(&rq->lock.dep_map, 1, _THIS_IP_);
#endif

	/* Here we just switch the register state and the stack. */
	switch_to(prev, next, prev);

	barrier();
	/*
	 * this_rq must be evaluated again because prev may have moved
	 * CPUs since it called schedule(), thus the 'rq' on its stack
	 * frame will be invalid.
	 */
	finish_task_switch(this_rq(), prev);
}

/*
 * nr_running, nr_uninterruptible and nr_context_switches:
 *
 * externally visible scheduler statistics: current number of runnable
 * threads, current number of uninterruptible-sleeping threads, total
 * number of context switches performed since bootup.
 */
unsigned long nr_running(void)
{
	unsigned long i, sum = 0;

	for_each_online_cpu(i)
		sum += cpu_rq(i)->nr_running;

	return sum;
}

unsigned long nr_uninterruptible(void)
{
	unsigned long i, sum = 0;

	for_each_possible_cpu(i)
		sum += cpu_rq(i)->nr_uninterruptible;

	/*
	 * Since we read the counters lockless, it might be slightly
	 * inaccurate. Do not allow it to go below zero though:
	 */
	if (unlikely((long)sum < 0))
		sum = 0;

	return sum;
}

unsigned long long nr_context_switches(void)
{
	int i;
	unsigned long long sum = 0;

	for_each_possible_cpu(i)
		sum += cpu_rq(i)->nr_switches;

	return sum;
}

unsigned long nr_iowait(void)
{
	unsigned long i, sum = 0;

	for_each_possible_cpu(i)
		sum += atomic_read(&cpu_rq(i)->nr_iowait);

	return sum;
}

unsigned long nr_iowait_cpu(void)
{
	struct rq *this = this_rq();
	return atomic_read(&this->nr_iowait);
}

unsigned long this_cpu_load(void)
{
	struct rq *this = this_rq();
	return this->cpu_load[0];
}


/* Variables and functions for calc_load */
static atomic_long_t calc_load_tasks;
static unsigned long calc_load_update;
unsigned long avenrun[3];
EXPORT_SYMBOL(avenrun);

/**
 * get_avenrun - get the load average array
 * @loads:	pointer to dest load array
 * @offset:	offset to add
 * @shift:	shift count to shift the result left
 *
 * These values are estimates at best, so no need for locking.
 */
void get_avenrun(unsigned long *loads, unsigned long offset, int shift)
{
	loads[0] = (avenrun[0] + offset) << shift;
	loads[1] = (avenrun[1] + offset) << shift;
	loads[2] = (avenrun[2] + offset) << shift;
}

static unsigned long
calc_load(unsigned long load, unsigned long exp, unsigned long active)
{
	load *= exp;
	load += active * (FIXED_1 - exp);
	return load >> FSHIFT;
}

/*
 * calc_load - update the avenrun load estimates 10 ticks after the
 * CPUs have updated calc_load_tasks.
 */
void calc_global_load(void)
{
	unsigned long upd = calc_load_update + 10;
	long active;

	if (time_before(jiffies, upd))
		return;

	active = atomic_long_read(&calc_load_tasks);
	active = active > 0 ? active * FIXED_1 : 0;

	avenrun[0] = calc_load(avenrun[0], EXP_1, active);
	avenrun[1] = calc_load(avenrun[1], EXP_5, active);
	avenrun[2] = calc_load(avenrun[2], EXP_15, active);

	calc_load_update += LOAD_FREQ;
}

/*
 * Either called from update_cpu_load() or from a cpu going idle
 */
static void calc_load_account_active(struct rq *this_rq)
{
	long nr_active, delta;

	nr_active = this_rq->nr_running;
	nr_active += (long) this_rq->nr_uninterruptible;

	if (nr_active != this_rq->calc_load_active) {
		delta = nr_active - this_rq->calc_load_active;
		this_rq->calc_load_active = nr_active;
		atomic_long_add(delta, &calc_load_tasks);
	}
}

/*
 * Externally visible per-cpu scheduler statistics:
 * cpu_nr_migrations(cpu) - number of migrations into that cpu
 */
u64 cpu_nr_migrations(int cpu)
{
	return cpu_rq(cpu)->nr_migrations_in;
}

/*
 * Update rq->cpu_load[] statistics. This function is usually called every
 * scheduler tick (TICK_NSEC).
 */
static void update_cpu_load(struct rq *this_rq)
{
	unsigned long this_load = this_rq->load.weight;
	int i, scale;

	this_rq->nr_load_updates++;

	/* Update our load: */
	for (i = 0, scale = 1; i < CPU_LOAD_IDX_MAX; i++, scale += scale) {
		unsigned long old_load, new_load;

		/* scale is effectively 1 << i now, and >> i divides by scale */

		old_load = this_rq->cpu_load[i];
		new_load = this_load;
		/*
		 * Round up the averaging division if load is increasing. This
		 * prevents us from getting stuck on 9 if the load is 10, for
		 * example.
		 */
		if (new_load > old_load)
			new_load += scale-1;
		this_rq->cpu_load[i] = (old_load*(scale-1) + new_load) >> i;
	}

	if (time_after_eq(jiffies, this_rq->calc_load_update)) {
		this_rq->calc_load_update += LOAD_FREQ;
		calc_load_account_active(this_rq);
	}
}

#ifdef CONFIG_SMP

/*
 * double_rq_lock - safely lock two runqueues
 *
 * Note this does not disable interrupts like task_rq_lock,
 * you need to do so manually before calling.
 */
static void double_rq_lock(struct rq *rq1, struct rq *rq2)
	__acquires(rq1->lock)
	__acquires(rq2->lock)
{
	BUG_ON(!irqs_disabled());
	if (rq1 == rq2) {
		spin_lock(&rq1->lock);
		__acquire(rq2->lock);	/* Fake it out ;) */
	} else {
		if (rq1 < rq2) {
			spin_lock(&rq1->lock);
			spin_lock_nested(&rq2->lock, SINGLE_DEPTH_NESTING);
		} else {
			spin_lock(&rq2->lock);
			spin_lock_nested(&rq1->lock, SINGLE_DEPTH_NESTING);
		}
	}
	update_rq_clock(rq1);
	update_rq_clock(rq2);
}

/*
 * double_rq_unlock - safely unlock two runqueues
 *
 * Note this does not restore interrupts like task_rq_unlock,
 * you need to do so manually after calling.
 */
static void double_rq_unlock(struct rq *rq1, struct rq *rq2)
	__releases(rq1->lock)
	__releases(rq2->lock)
{
	spin_unlock(&rq1->lock);
	if (rq1 != rq2)
		spin_unlock(&rq2->lock);
	else
		__release(rq2->lock);
}

/*
 * If dest_cpu is allowed for this process, migrate the task to it.
 * This is accomplished by forcing the cpu_allowed mask to only
 * allow dest_cpu, which will force the cpu onto dest_cpu. Then
 * the cpu_allowed mask is restored.
 */
static void sched_migrate_task(struct task_struct *p, int dest_cpu)
{
	struct migration_req req;
	unsigned long flags;
	struct rq *rq;

	rq = task_rq_lock(p, &flags);
	if (!cpumask_test_cpu(dest_cpu, &p->cpus_allowed)
	    || unlikely(!cpu_active(dest_cpu)))
		goto out;

	/* force the process onto the specified CPU */
	if (migrate_task(p, dest_cpu, &req)) {
		/* Need to wait for migration thread (might exit: take ref). */
		struct task_struct *mt = rq->migration_thread;

		get_task_struct(mt);
		task_rq_unlock(rq, &flags);
		wake_up_process(mt);
		put_task_struct(mt);
		wait_for_completion(&req.done);

		return;
	}
out:
	task_rq_unlock(rq, &flags);
}

/*
 * sched_exec - execve() is a valuable balancing opportunity, because at
 * this point the task has the smallest effective memory and cache footprint.
 */
void sched_exec(void)
{
	int new_cpu, this_cpu = get_cpu();
	new_cpu = current->sched_class->select_task_rq(current, SD_BALANCE_EXEC, 0);
	put_cpu();
	if (new_cpu != this_cpu)
		sched_migrate_task(current, new_cpu);
}

/*
 * pull_task - move a task from a remote runqueue to the local runqueue.
 * Both runqueues must be locked.
 */
static void pull_task(struct rq *src_rq, struct task_struct *p,
		      struct rq *this_rq, int this_cpu)
{
	deactivate_task(src_rq, p, 0);
	set_task_cpu(p, this_cpu);
	activate_task(this_rq, p, 0);
	/*
	 * Note that idle threads have a prio of MAX_PRIO, for this test
	 * to be always true for them.
	 */
	check_preempt_curr(this_rq, p, 0);
}

/*
 * can_migrate_task - may task p from runqueue rq be migrated to this_cpu?
 */
static
int can_migrate_task(struct task_struct *p, struct rq *rq, int this_cpu,
		     struct sched_domain *sd, enum cpu_idle_type idle,
		     int *all_pinned)
{
	int tsk_cache_hot = 0;
	/*
	 * We do not migrate tasks that are:
	 * 1) running (obviously), or
	 * 2) cannot be migrated to this CPU due to cpus_allowed, or
	 * 3) are cache-hot on their current CPU.
	 */
	if (!cpumask_test_cpu(this_cpu, &p->cpus_allowed)) {
		schedstat_inc(p, se.nr_failed_migrations_affine);
		return 0;
	}
	*all_pinned = 0;

	if (task_running(rq, p)) {
		schedstat_inc(p, se.nr_failed_migrations_running);
		return 0;
	}

	/*
	 * Aggressive migration if:
	 * 1) task is cache cold, or
	 * 2) too many balance attempts have failed.
	 */

	tsk_cache_hot = task_hot(p, rq->clock, sd);
	if (!tsk_cache_hot ||
		sd->nr_balance_failed > sd->cache_nice_tries) {
#ifdef CONFIG_SCHEDSTATS
		if (tsk_cache_hot) {
			schedstat_inc(sd, lb_hot_gained[idle]);
			schedstat_inc(p, se.nr_forced_migrations);
		}
#endif
		return 1;
	}

	if (tsk_cache_hot) {
		schedstat_inc(p, se.nr_failed_migrations_hot);
		return 0;
	}
	return 1;
}

static unsigned long
balance_tasks(struct rq *this_rq, int this_cpu, struct rq *busiest,
	      unsigned long max_load_move, struct sched_domain *sd,
	      enum cpu_idle_type idle, int *all_pinned,
	      int *this_best_prio, struct rq_iterator *iterator)
{
	int loops = 0, pulled = 0, pinned = 0;
	struct task_struct *p;
	long rem_load_move = max_load_move;

	if (max_load_move == 0)
		goto out;

	pinned = 1;

	/*
	 * Start the load-balancing iterator:
	 */
	p = iterator->start(iterator->arg);
next:
	if (!p || loops++ > sysctl_sched_nr_migrate)
		goto out;

	if ((p->se.load.weight >> 1) > rem_load_move ||
	    !can_migrate_task(p, busiest, this_cpu, sd, idle, &pinned)) {
		p = iterator->next(iterator->arg);
		goto next;
	}

	pull_task(busiest, p, this_rq, this_cpu);
	pulled++;
	rem_load_move -= p->se.load.weight;

#ifdef CONFIG_PREEMPT
	/*
	 * NEWIDLE balancing is a source of latency, so preemptible kernels
	 * will stop after the first task is pulled to minimize the critical
	 * section.
	 */
	if (idle == CPU_NEWLY_IDLE)
		goto out;
#endif

	/*
	 * We only want to steal up to the prescribed amount of weighted load.
	 */
	if (rem_load_move > 0) {
		if (p->prio < *this_best_prio)
			*this_best_prio = p->prio;
		p = iterator->next(iterator->arg);
		goto next;
	}
out:
	/*
	 * Right now, this is one of only two places pull_task() is called,
	 * so we can safely collect pull_task() stats here rather than
	 * inside pull_task().
	 */
	schedstat_add(sd, lb_gained[idle], pulled);

	if (all_pinned)
		*all_pinned = pinned;

	return max_load_move - rem_load_move;
}

/*
 * move_tasks tries to move up to max_load_move weighted load from busiest to
 * this_rq, as part of a balancing operation within domain "sd".
 * Returns 1 if successful and 0 otherwise.
 *
 * Called with both runqueues locked.
 */
static int move_tasks(struct rq *this_rq, int this_cpu, struct rq *busiest,
		      unsigned long max_load_move,
		      struct sched_domain *sd, enum cpu_idle_type idle,
		      int *all_pinned)
{
	const struct sched_class *class = sched_class_highest;
	unsigned long total_load_moved = 0;
	int this_best_prio = this_rq->curr->prio;

	do {
		total_load_moved +=
			class->load_balance(this_rq, this_cpu, busiest,
				max_load_move - total_load_moved,
				sd, idle, all_pinned, &this_best_prio);
		class = class->next;

#ifdef CONFIG_PREEMPT
		/*
		 * NEWIDLE balancing is a source of latency, so preemptible
		 * kernels will stop after the first task is pulled to minimize
		 * the critical section.
		 */
		if (idle == CPU_NEWLY_IDLE && this_rq->nr_running)
			break;
#endif
	} while (class && max_load_move > total_load_moved);

	return total_load_moved > 0;
}

static int
iter_move_one_task(struct rq *this_rq, int this_cpu, struct rq *busiest,
		   struct sched_domain *sd, enum cpu_idle_type idle,
		   struct rq_iterator *iterator)
{
	struct task_struct *p = iterator->start(iterator->arg);
	int pinned = 0;

	while (p) {
		if (can_migrate_task(p, busiest, this_cpu, sd, idle, &pinned)) {
			pull_task(busiest, p, this_rq, this_cpu);
			/*
			 * Right now, this is only the second place pull_task()
			 * is called, so we can safely collect pull_task()
			 * stats here rather than inside pull_task().
			 */
			schedstat_inc(sd, lb_gained[idle]);

			return 1;
		}
		p = iterator->next(iterator->arg);
	}

	return 0;
}

/*
 * move_one_task tries to move exactly one task from busiest to this_rq, as
 * part of active balancing operations within "domain".
 * Returns 1 if successful and 0 otherwise.
 *
 * Called with both runqueues locked.
 */
static int move_one_task(struct rq *this_rq, int this_cpu, struct rq *busiest,
			 struct sched_domain *sd, enum cpu_idle_type idle)
{
	const struct sched_class *class;

	for_each_class(class) {
		if (class->move_one_task(this_rq, this_cpu, busiest, sd, idle))
			return 1;
	}

	return 0;
}
/********** Helpers for find_busiest_group ************************/
/*
 * sd_lb_stats - Structure to store the statistics of a sched_domain
 * 		during load balancing.
 */
struct sd_lb_stats {
	struct sched_group *busiest; /* Busiest group in this sd */
	struct sched_group *this;  /* Local group in this sd */
	unsigned long total_load;  /* Total load of all groups in sd */
	unsigned long total_pwr;   /*	Total power of all groups in sd */
	unsigned long avg_load;	   /* Average load across all groups in sd */

	/** Statistics of this group */
	unsigned long this_load;
	unsigned long this_load_per_task;
	unsigned long this_nr_running;

	/* Statistics of the busiest group */
	unsigned long max_load;
	unsigned long busiest_load_per_task;
	unsigned long busiest_nr_running;

	int group_imb; /* Is there imbalance in this sd */
#if defined(CONFIG_SCHED_MC) || defined(CONFIG_SCHED_SMT)
	int power_savings_balance; /* Is powersave balance needed for this sd */
	struct sched_group *group_min; /* Least loaded group in sd */
	struct sched_group *group_leader; /* Group which relieves group_min */
	unsigned long min_load_per_task; /* load_per_task in group_min */
	unsigned long leader_nr_running; /* Nr running of group_leader */
	unsigned long min_nr_running; /* Nr running of group_min */
#endif
};

/*
 * sg_lb_stats - stats of a sched_group required for load_balancing
 */
struct sg_lb_stats {
	unsigned long avg_load; /*Avg load across the CPUs of the group */
	unsigned long group_load; /* Total load over the CPUs of the group */
	unsigned long sum_nr_running; /* Nr tasks running in the group */
	unsigned long sum_weighted_load; /* Weighted load of group's tasks */
	unsigned long group_capacity;
	int group_imb; /* Is there an imbalance in the group ? */
};

/**
 * group_first_cpu - Returns the first cpu in the cpumask of a sched_group.
 * @group: The group whose first cpu is to be returned.
 */
static inline unsigned int group_first_cpu(struct sched_group *group)
{
	return cpumask_first(sched_group_cpus(group));
}

/**
 * get_sd_load_idx - Obtain the load index for a given sched domain.
 * @sd: The sched_domain whose load_idx is to be obtained.
 * @idle: The Idle status of the CPU for whose sd load_icx is obtained.
 */
static inline int get_sd_load_idx(struct sched_domain *sd,
					enum cpu_idle_type idle)
{
	int load_idx;

	switch (idle) {
	case CPU_NOT_IDLE:
		load_idx = sd->busy_idx;
		break;

	case CPU_NEWLY_IDLE:
		load_idx = sd->newidle_idx;
		break;
	default:
		load_idx = sd->idle_idx;
		break;
	}

	return load_idx;
}


#if defined(CONFIG_SCHED_MC) || defined(CONFIG_SCHED_SMT)
/**
 * init_sd_power_savings_stats - Initialize power savings statistics for
 * the given sched_domain, during load balancing.
 *
 * @sd: Sched domain whose power-savings statistics are to be initialized.
 * @sds: Variable containing the statistics for sd.
 * @idle: Idle status of the CPU at which we're performing load-balancing.
 */
static inline void init_sd_power_savings_stats(struct sched_domain *sd,
	struct sd_lb_stats *sds, enum cpu_idle_type idle)
{
	/*
	 * Busy processors will not participate in power savings
	 * balance.
	 */
	if (idle == CPU_NOT_IDLE || !(sd->flags & SD_POWERSAVINGS_BALANCE))
		sds->power_savings_balance = 0;
	else {
		sds->power_savings_balance = 1;
		sds->min_nr_running = ULONG_MAX;
		sds->leader_nr_running = 0;
	}
}

/**
 * update_sd_power_savings_stats - Update the power saving stats for a
 * sched_domain while performing load balancing.
 *
 * @group: sched_group belonging to the sched_domain under consideration.
 * @sds: Variable containing the statistics of the sched_domain
 * @local_group: Does group contain the CPU for which we're performing
 * 		load balancing ?
 * @sgs: Variable containing the statistics of the group.
 */
static inline void update_sd_power_savings_stats(struct sched_group *group,
	struct sd_lb_stats *sds, int local_group, struct sg_lb_stats *sgs)
{

	if (!sds->power_savings_balance)
		return;

	/*
	 * If the local group is idle or completely loaded
	 * no need to do power savings balance at this domain
	 */
	if (local_group && (sds->this_nr_running >= sgs->group_capacity ||
				!sds->this_nr_running))
		sds->power_savings_balance = 0;

	/*
	 * If a group is already running at full capacity or idle,
	 * don't include that group in power savings calculations
	 */
	if (!sds->power_savings_balance ||
		sgs->sum_nr_running >= sgs->group_capacity ||
		!sgs->sum_nr_running)
		return;

	/*
	 * Calculate the group which has the least non-idle load.
	 * This is the group from where we need to pick up the load
	 * for saving power
	 */
	if ((sgs->sum_nr_running < sds->min_nr_running) ||
	    (sgs->sum_nr_running == sds->min_nr_running &&
	     group_first_cpu(group) > group_first_cpu(sds->group_min))) {
		sds->group_min = group;
		sds->min_nr_running = sgs->sum_nr_running;
		sds->min_load_per_task = sgs->sum_weighted_load /
						sgs->sum_nr_running;
	}

	/*
	 * Calculate the group which is almost near its
	 * capacity but still has some space to pick up some load
	 * from other group and save more power
	 */
	if (sgs->sum_nr_running + 1 > sgs->group_capacity)
		return;

	if (sgs->sum_nr_running > sds->leader_nr_running ||
	    (sgs->sum_nr_running == sds->leader_nr_running &&
	     group_first_cpu(group) < group_first_cpu(sds->group_leader))) {
		sds->group_leader = group;
		sds->leader_nr_running = sgs->sum_nr_running;
	}
}

/**
 * check_power_save_busiest_group - see if there is potential for some power-savings balance
 * @sds: Variable containing the statistics of the sched_domain
 *	under consideration.
 * @this_cpu: Cpu at which we're currently performing load-balancing.
 * @imbalance: Variable to store the imbalance.
 *
 * Description:
 * Check if we have potential to perform some power-savings balance.
 * If yes, set the busiest group to be the least loaded group in the
 * sched_domain, so that it's CPUs can be put to idle.
 *
 * Returns 1 if there is potential to perform power-savings balance.
 * Else returns 0.
 */
static inline int check_power_save_busiest_group(struct sd_lb_stats *sds,
					int this_cpu, unsigned long *imbalance)
{
	if (!sds->power_savings_balance)
		return 0;

	if (sds->this != sds->group_leader ||
			sds->group_leader == sds->group_min)
		return 0;

	*imbalance = sds->min_load_per_task;
	sds->busiest = sds->group_min;

	return 1;

}
#else /* CONFIG_SCHED_MC || CONFIG_SCHED_SMT */
static inline void init_sd_power_savings_stats(struct sched_domain *sd,
	struct sd_lb_stats *sds, enum cpu_idle_type idle)
{
	return;
}

static inline void update_sd_power_savings_stats(struct sched_group *group,
	struct sd_lb_stats *sds, int local_group, struct sg_lb_stats *sgs)
{
	return;
}

static inline int check_power_save_busiest_group(struct sd_lb_stats *sds,
					int this_cpu, unsigned long *imbalance)
{
	return 0;
}
#endif /* CONFIG_SCHED_MC || CONFIG_SCHED_SMT */


unsigned long default_scale_freq_power(struct sched_domain *sd, int cpu)
{
	return SCHED_LOAD_SCALE;
}

unsigned long __weak arch_scale_freq_power(struct sched_domain *sd, int cpu)
{
	return default_scale_freq_power(sd, cpu);
}

unsigned long default_scale_smt_power(struct sched_domain *sd, int cpu)
{
	unsigned long weight = cpumask_weight(sched_domain_span(sd));
	unsigned long smt_gain = sd->smt_gain;

	smt_gain /= weight;

	return smt_gain;
}

unsigned long __weak arch_scale_smt_power(struct sched_domain *sd, int cpu)
{
	return default_scale_smt_power(sd, cpu);
}

unsigned long scale_rt_power(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	u64 total, available;

	sched_avg_update(rq);

	total = sched_avg_period() + (rq->clock - rq->age_stamp);
	available = total - rq->rt_avg;

	if (unlikely((s64)total < SCHED_LOAD_SCALE))
		total = SCHED_LOAD_SCALE;

	total >>= SCHED_LOAD_SHIFT;

	return div_u64(available, total);
}

static void update_cpu_power(struct sched_domain *sd, int cpu)
{
	unsigned long weight = cpumask_weight(sched_domain_span(sd));
	unsigned long power = SCHED_LOAD_SCALE;
	struct sched_group *sdg = sd->groups;

	if (sched_feat(ARCH_POWER))
		power *= arch_scale_freq_power(sd, cpu);
	else
		power *= default_scale_freq_power(sd, cpu);

	power >>= SCHED_LOAD_SHIFT;

	if ((sd->flags & SD_SHARE_CPUPOWER) && weight > 1) {
		if (sched_feat(ARCH_POWER))
			power *= arch_scale_smt_power(sd, cpu);
		else
			power *= default_scale_smt_power(sd, cpu);

		power >>= SCHED_LOAD_SHIFT;
	}

	power *= scale_rt_power(cpu);
	power >>= SCHED_LOAD_SHIFT;

	if (!power)
		power = 1;

	sdg->cpu_power = power;
}

static void update_group_power(struct sched_domain *sd, int cpu)
{
	struct sched_domain *child = sd->child;
	struct sched_group *group, *sdg = sd->groups;
	unsigned long power;

	if (!child) {
		update_cpu_power(sd, cpu);
		return;
	}

	power = 0;

	group = child->groups;
	do {
		power += group->cpu_power;
		group = group->next;
	} while (group != child->groups);

	sdg->cpu_power = power;
}

/**
 * update_sg_lb_stats - Update sched_group's statistics for load balancing.
 * @sd: The sched_domain whose statistics are to be updated.
 * @group: sched_group whose statistics are to be updated.
 * @this_cpu: Cpu for which load balance is currently performed.
 * @idle: Idle status of this_cpu
 * @load_idx: Load index of sched_domain of this_cpu for load calc.
 * @sd_idle: Idle status of the sched_domain containing group.
 * @local_group: Does group contain this_cpu.
 * @cpus: Set of cpus considered for load balancing.
 * @balance: Should we balance.
 * @sgs: variable to hold the statistics for this group.
 */
static inline void update_sg_lb_stats(struct sched_domain *sd,
			struct sched_group *group, int this_cpu,
			enum cpu_idle_type idle, int load_idx, int *sd_idle,
			int local_group, const struct cpumask *cpus,
			int *balance, struct sg_lb_stats *sgs)
{
	unsigned long load, max_cpu_load, min_cpu_load;
	int i;
	unsigned int balance_cpu = -1, first_idle_cpu = 0;
	unsigned long sum_avg_load_per_task;
	unsigned long avg_load_per_task;

	if (local_group) {
		balance_cpu = group_first_cpu(group);
		if (balance_cpu == this_cpu)
			update_group_power(sd, this_cpu);
	}

	/* Tally up the load of all CPUs in the group */
	sum_avg_load_per_task = avg_load_per_task = 0;
	max_cpu_load = 0;
	min_cpu_load = ~0UL;

	for_each_cpu_and(i, sched_group_cpus(group), cpus) {
		struct rq *rq = cpu_rq(i);

		if (*sd_idle && rq->nr_running)
			*sd_idle = 0;

		/* Bias balancing toward cpus of our domain */
		if (local_group) {
			if (idle_cpu(i) && !first_idle_cpu) {
				first_idle_cpu = 1;
				balance_cpu = i;
			}

			load = target_load(i, load_idx);
		} else {
			load = source_load(i, load_idx);
			if (load > max_cpu_load)
				max_cpu_load = load;
			if (min_cpu_load > load)
				min_cpu_load = load;
		}

		sgs->group_load += load;
		sgs->sum_nr_running += rq->nr_running;
		sgs->sum_weighted_load += weighted_cpuload(i);

		sum_avg_load_per_task += cpu_avg_load_per_task(i);
	}

	/*
	 * First idle cpu or the first cpu(busiest) in this sched group
	 * is eligible for doing load balancing at this and above
	 * domains. In the newly idle case, we will allow all the cpu's
	 * to do the newly idle load balance.
	 */
	if (idle != CPU_NEWLY_IDLE && local_group &&
	    balance_cpu != this_cpu && balance) {
		*balance = 0;
		return;
	}

	/* Adjust by relative CPU power of the group */
	sgs->avg_load = (sgs->group_load * SCHED_LOAD_SCALE) / group->cpu_power;


	/*
	 * Consider the group unbalanced when the imbalance is larger
	 * than the average weight of two tasks.
	 *
	 * APZ: with cgroup the avg task weight can vary wildly and
	 *      might not be a suitable number - should we keep a
	 *      normalized nr_running number somewhere that negates
	 *      the hierarchy?
	 */
	avg_load_per_task = (sum_avg_load_per_task * SCHED_LOAD_SCALE) /
		group->cpu_power;

	if ((max_cpu_load - min_cpu_load) > 2*avg_load_per_task)
		sgs->group_imb = 1;

	sgs->group_capacity =
		DIV_ROUND_CLOSEST(group->cpu_power, SCHED_LOAD_SCALE);
}

/**
 * update_sd_lb_stats - Update sched_group's statistics for load balancing.
 * @sd: sched_domain whose statistics are to be updated.
 * @this_cpu: Cpu for which load balance is currently performed.
 * @idle: Idle status of this_cpu
 * @sd_idle: Idle status of the sched_domain containing group.
 * @cpus: Set of cpus considered for load balancing.
 * @balance: Should we balance.
 * @sds: variable to hold the statistics for this sched_domain.
 */
static inline void update_sd_lb_stats(struct sched_domain *sd, int this_cpu,
			enum cpu_idle_type idle, int *sd_idle,
			const struct cpumask *cpus, int *balance,
			struct sd_lb_stats *sds)
{
	struct sched_domain *child = sd->child;
	struct sched_group *group = sd->groups;
	struct sg_lb_stats sgs;
	int load_idx, prefer_sibling = 0;

	if (child && child->flags & SD_PREFER_SIBLING)
		prefer_sibling = 1;

	init_sd_power_savings_stats(sd, sds, idle);
	load_idx = get_sd_load_idx(sd, idle);

	do {
		int local_group;

		local_group = cpumask_test_cpu(this_cpu,
					       sched_group_cpus(group));
		memset(&sgs, 0, sizeof(sgs));
		update_sg_lb_stats(sd, group, this_cpu, idle, load_idx, sd_idle,
				local_group, cpus, balance, &sgs);

		if (local_group && balance && !(*balance))
			return;

		sds->total_load += sgs.group_load;
		sds->total_pwr += group->cpu_power;

		/*
		 * In case the child domain prefers tasks go to siblings
		 * first, lower the group capacity to one so that we'll try
		 * and move all the excess tasks away.
		 */
		if (prefer_sibling)
			sgs.group_capacity = min(sgs.group_capacity, 1UL);

		if (local_group) {
			sds->this_load = sgs.avg_load;
			sds->this = group;
			sds->this_nr_running = sgs.sum_nr_running;
			sds->this_load_per_task = sgs.sum_weighted_load;
		} else if (sgs.avg_load > sds->max_load &&
			   (sgs.sum_nr_running > sgs.group_capacity ||
				sgs.group_imb)) {
			sds->max_load = sgs.avg_load;
			sds->busiest = group;
			sds->busiest_nr_running = sgs.sum_nr_running;
			sds->busiest_load_per_task = sgs.sum_weighted_load;
			sds->group_imb = sgs.group_imb;
		}

		update_sd_power_savings_stats(group, sds, local_group, &sgs);
		group = group->next;
	} while (group != sd->groups);
}

/**
 * fix_small_imbalance - Calculate the minor imbalance that exists
 *			amongst the groups of a sched_domain, during
 *			load balancing.
 * @sds: Statistics of the sched_domain whose imbalance is to be calculated.
 * @this_cpu: The cpu at whose sched_domain we're performing load-balance.
 * @imbalance: Variable to store the imbalance.
 */
static inline void fix_small_imbalance(struct sd_lb_stats *sds,
				int this_cpu, unsigned long *imbalance)
{
	unsigned long tmp, pwr_now = 0, pwr_move = 0;
	unsigned int imbn = 2;

	if (sds->this_nr_running) {
		sds->this_load_per_task /= sds->this_nr_running;
		if (sds->busiest_load_per_task >
				sds->this_load_per_task)
			imbn = 1;
	} else
		sds->this_load_per_task =
			cpu_avg_load_per_task(this_cpu);

	if (sds->max_load - sds->this_load + sds->busiest_load_per_task >=
			sds->busiest_load_per_task * imbn) {
		*imbalance = sds->busiest_load_per_task;
		return;
	}

	/*
	 * OK, we don't have enough imbalance to justify moving tasks,
	 * however we may be able to increase total CPU power used by
	 * moving them.
	 */

	pwr_now += sds->busiest->cpu_power *
			min(sds->busiest_load_per_task, sds->max_load);
	pwr_now += sds->this->cpu_power *
			min(sds->this_load_per_task, sds->this_load);
	pwr_now /= SCHED_LOAD_SCALE;

	/* Amount of load we'd subtract */
	tmp = (sds->busiest_load_per_task * SCHED_LOAD_SCALE) /
		sds->busiest->cpu_power;
	if (sds->max_load > tmp)
		pwr_move += sds->busiest->cpu_power *
			min(sds->busiest_load_per_task, sds->max_load - tmp);

	/* Amount of load we'd add */
	if (sds->max_load * sds->busiest->cpu_power <
		sds->busiest_load_per_task * SCHED_LOAD_SCALE)
		tmp = (sds->max_load * sds->busiest->cpu_power) /
			sds->this->cpu_power;
	else
		tmp = (sds->busiest_load_per_task * SCHED_LOAD_SCALE) /
			sds->this->cpu_power;
	pwr_move += sds->this->cpu_power *
			min(sds->this_load_per_task, sds->this_load + tmp);
	pwr_move /= SCHED_LOAD_SCALE;

	/* Move if we gain throughput */
	if (pwr_move > pwr_now)
		*imbalance = sds->busiest_load_per_task;
}

/**
 * calculate_imbalance - Calculate the amount of imbalance present within the
 *			 groups of a given sched_domain during load balance.
 * @sds: statistics of the sched_domain whose imbalance is to be calculated.
 * @this_cpu: Cpu for which currently load balance is being performed.
 * @imbalance: The variable to store the imbalance.
 */
static inline void calculate_imbalance(struct sd_lb_stats *sds, int this_cpu,
		unsigned long *imbalance)
{
	unsigned long max_pull;
	/*
	 * In the presence of smp nice balancing, certain scenarios can have
	 * max load less than avg load(as we skip the groups at or below
	 * its cpu_power, while calculating max_load..)
	 */
	if (sds->max_load < sds->avg_load) {
		*imbalance = 0;
		return fix_small_imbalance(sds, this_cpu, imbalance);
	}

	/* Don't want to pull so many tasks that a group would go idle */
	max_pull = min(sds->max_load - sds->avg_load,
			sds->max_load - sds->busiest_load_per_task);

	/* How much load to actually move to equalise the imbalance */
	*imbalance = min(max_pull * sds->busiest->cpu_power,
		(sds->avg_load - sds->this_load) * sds->this->cpu_power)
			/ SCHED_LOAD_SCALE;

	/*
	 * if *imbalance is less than the average load per runnable task
	 * there is no gaurantee that any tasks will be moved so we'll have
	 * a think about bumping its value to force at least one task to be
	 * moved
	 */
	if (*imbalance < sds->busiest_load_per_task)
		return fix_small_imbalance(sds, this_cpu, imbalance);

}
/******* find_busiest_group() helpers end here *********************/

/**
 * find_busiest_group - Returns the busiest group within the sched_domain
 * if there is an imbalance. If there isn't an imbalance, and
 * the user has opted for power-savings, it returns a group whose
 * CPUs can be put to idle by rebalancing those tasks elsewhere, if
 * such a group exists.
 *
 * Also calculates the amount of weighted load which should be moved
 * to restore balance.
 *
 * @sd: The sched_domain whose busiest group is to be returned.
 * @this_cpu: The cpu for which load balancing is currently being performed.
 * @imbalance: Variable which stores amount of weighted load which should
 *		be moved to restore balance/put a group to idle.
 * @idle: The idle status of this_cpu.
 * @sd_idle: The idleness of sd
 * @cpus: The set of CPUs under consideration for load-balancing.
 * @balance: Pointer to a variable indicating if this_cpu
 *	is the appropriate cpu to perform load balancing at this_level.
 *
 * Returns:	- the busiest group if imbalance exists.
 *		- If no imbalance and user has opted for power-savings balance,
 *		   return the least loaded group whose CPUs can be
 *		   put to idle by rebalancing its tasks onto our group.
 */
static struct sched_group *
find_busiest_group(struct sched_domain *sd, int this_cpu,
		   unsigned long *imbalance, enum cpu_idle_type idle,
		   int *sd_idle, const struct cpumask *cpus, int *balance)
{
	struct sd_lb_stats sds;

	memset(&sds, 0, sizeof(sds));

	/*
	 * Compute the various statistics relavent for load balancing at
	 * this level.
	 */
	update_sd_lb_stats(sd, this_cpu, idle, sd_idle, cpus,
					balance, &sds);

	/* Cases where imbalance does not exist from POV of this_cpu */
	/* 1) this/*
 *is not the appropriate *
 *to perform load balancing
	 *    aKernis level.pyrig2) There*
 *   busy sibling grouprelatull from  Linu3 Toris3  Modiisernelbusiest3  Mod  Linu4 to fix bugs in mormaphoy thanernel vgaphoreness (C) 199pyright sched_domain  Linu5 Torv im *
 * lds
 withi11-19	specified limit  Linu6) Any reAndrea Awould leadrelating-poCopyri/
	if (Andrea A&& !(*Andrea ))
		goto ret;
brid p!sds.phores a|| 		an array-_nr_runn2-23== 0-robin deout_Andrea dn with
 		anched.alls
>=tch mmax Cleaimeslices
 *		and per-CPU		anavg Clean= (SCHED_LOAD_SCALE *tch mtotaleful s /by Robert Lpwr-CPU runqueues.  Cleanups andenzi, primeslices
 *		and per-CPU run100s by Ro useful  <ps a->y Andrea _pcts by Robs.  Cleasuggestions
 *		by Davide Libethod ofalls_per_task /ps andethod of distribut;PU runqueu  Mod_imb-rob tuning with a
 *         =
			minnqueuing with a
 *        ,on Kolivas.
 *n wi/*pyrigWe're try2-23to get allernelcpus2007-19	Imerageh a
 , so we don'tpyrigwantified sh ourselves abovety improvemecalls, nor dFS bywish topyrigreduc schedmaxcallsedand rbelowscheduling enhancemas either ofty isepyrigactionseduler just result insafe
  O(1) sc2-23later, and Molnar:
 *		h     s around. ThusS bylook for*  200inimum pos96-1	by Andrea   LinuNegativ	by Andrea s (*we* aresafe
 1-29  R98-11anyone else) willpyrigbe counted Stenoby Andrea A/modulese purposes --S bycan't fix.h>
e Galbbyed by*  20inux/to us. Bonterefulstedne <linuxnumbers Stethey'ighmem.appear Stevery large valuecangel unsign04	Nongs  Linybrid pe by Nick Piggin
 and other improvements
imeslices
 *		and per-CPU/* Looks lik schealds
 anux/nmi.h>
# Compute itinux/calcue Krs.
 ndrea (&sds,sched.c
 ,by Andrea );
	returnlude <linux/;


 *		and per:y Peter Worvalds
 *
 obviouser.h>
#incluBut check if_contexs bysome
 *
 *  Copyrigto save power <linux/debupid_n_de <l_incl_ethod of  Modux/freezer.h>
#include <lin-robvmalloc.h>
#include ret:
	*y Andrea A= 0ux/vmallocNULL;
}

/*
 * findnux/timerqueue -cpu.h semaphores arune <linamo  20nteractiinand
 *		inuxstatic struct rq *
pu.h>
#include <li(e.h>
#i) and   Modi*  Mod, enumerac_idle_type  <li,
		  tat.h>
#includby Andrea , constle.h>
#icpum    *ract)
{
	e.h>
#incluphores a=lude , *rq;
	lude <linux/ts Nick Piguset.hint in wifor_each/*
 (i,de <linux/sy/*
 s(  Mod)) {
	ude <linux/unisde <l =>
#inc_of(inux/ude <linux/uniscapacity = DIV_ROUND_CLOSEST(de <l, tible kernel bitebugfs.h>
#include wln wiith
 *inux/kp_td of#includbes.h-rob	continue<asm/rq =cluderq/debugfwl = weightedludealls(i) *ace.h>

#include OINTS
#/ude <li<asm/irq_<linux/ct&& rq-> distributing 1 0 .wl >nclude <linupri.h"

#define id piorit useful slude 	td.h>
#incle <a			ux/delayacinclu	}
	}
x/vmalloc#include inux/percMax backoffamespacen#inclur Mol
#inupt.h. Pretty arbitrarynux/ke, butpercsonux/tsasux/p991- <linenoughlinux/#define MAX_PINNED_INTERVAL	512nux/ Work2-23inux/kpr/mod a
 *Andrea Atz
 can work wit_new <li.nux/seq_filDEFINE_PER_CPU(egs.h>

var_t,tter when scaltmpx/kp  byx/percCid_nached.c
 *to ensualdsic_pr	and perangeli
 related Attempth
 *movepercupt.h>iedt, x/notifier.h>
#inclinux/seq_fil<lincan work wit(<linzer.h>
#ine.h>
#inclugun orqh>
#include ) and relate *sdh>
#include <linux/times.h>
# <linand round>
#iMAX_Pd_p)->by Sll_T_PRIO = 0,y Haslinuinux/cpuse *  e <linclude #include <linux/syscalls.clude <linux/unislude <linLOAD_SCALE<linux/delaclude <linux/unisflagsLOAD_SCALEinux/kprobes. = __geclude a [(.. 39 ] range.
 */
#defi	egs.h>

copy
 * ssched_online_or SCHED_Peter Wihen>
#inclsavings policyc_prenabledx/module.parent_PRIO(p,imes.pyrig96-12-23e.h>pick upcalls
irre002-linuxof *  1996-12-2s. I11-1is cas.h>
 * leKernelseq_d, ie		SCH96-12-23percoe Krtes as CPU_IDLE, insty Inof RUNTportra *  2it_policy(NOT(int  <linux/debu		SCH!= SCHED_FIFO | 0 .
 *  sche & SD_SHAREers,POWER &&
#inc !#inclsd_HZ / 1(ion
SD_ne inSAVINGS_BALANCEcpuprOAD		SCHED1vide  andseq__incuct tlb_#incl[ <li]  byredo:
	upd#incshares(s   b	  Modi=cpu.h>
#includ.h>
#iion
zer.h>
#in&acct_kern.hmes.h &OAD		SCES(TI#inc Timesude <linuxbrid pand rouning timeslices
 *		and per-CPU run!h>
#inlude y);
}

/*
 * This is no*  1griority-qeslices
 *		and per-C + (nux/delayace <linux/sysctl.h>
alls.h>RE_BITacct_kern.h>es.hgn by C!phores AX_RT_PRIO];
};

struct rt_bandwiqth {
	/* nests inside the rq lock: BUG_ON(ux/delaya= rt_prrq incly);
}

/*
addThis is lude <linrioritinclude <linux
	TIME) / nclude <d prhores  19 ]
 * to s>el/sude PeteRUNTUSER_PRIO((p)-> - 20)
#Ifuling class:
 */
st has f.h>
eriod_fier.h>
#incIO_T 20)
#de 19 ]
 * to s<= 1t rtex bugs ineriod_still un	and per.(TIME) /  simply	((uys zeroo CFSIO)
#eriod_correctly trealude <lAX_USER_PRIO		riod/
		local_irqe <li( sche/* nedouble_rq_lock(anosecon 20)
#de/* ne overrun);
p)->     seriod);

	zer.h>
#inenum hrap, MAX_R  			rt_periodion
RE_BITMSEC_PER_SEer, now, rt_b->unrt_period);

		if (!overrunhrtimer_frestorard(timer,_rt_period_ncludon Ros*
 *dihread.alls
 *
 * ck.h>
ue <lerrun =tic  overrun;&&O(p)		((p)!= smp_processor_id(cpuprire) and #incched.c
 t rt_ban A6  Iinux/o
 */
#dude <linuwefinT_PRIO bylicy affinx/ctriod = nsunde <ly( HRTIMER_NO */
#deRR tasks)lear_locklicesf enum hr)od;
	u64		m/irq_regs.h>

R_PR * Timcpupribin desido_rt_psts inside the rq loRIO + (n	rt_rTIME) / AX_RT_PRIO];
};

struct rt_failedth {
	/* ne
 * nrwhen scaloid st++RIO-1 ],
NOTONIC, width(struct rt_bandw >	retucache_ninge.ries+2DE_REd timpin>rt_pmer_orwar&enum hrtirt_p,e schet rt_brt_r Mike kenotule.h>graaski_threncemo)
#de curr_acttatic_pck);phores a*
 *text.hblinurun;vity ed.c
 ock(&run =/irq_regs.h>

#include zer.h>
#iline  overru
		return;in_l->;
	u_allowMODE_REL);ime ==MER_REmer_ndwidth(
		return;

	if (hrtimerk;

SEC_PER_SEC /1timer)lices
 *	one_PER_SEtimer+ (n;
		ktienum hrti))

#define NIbreak;

rt_period);

		soft = hr_forward(enum hrti  20/*
 *static cputimer);)

#define NICE_orward}untime == = hrtimer_cb_get_time(&rt_b->rt_period_timertic );

		soft = hrimer)wake_urt_runtim enum hrtiod_timer))
		retmer_activock(&rtWe'veb->rtude eriod, *
 *  Co,    ME_INF	oid ureock(&rt - MAX_rt_p delta;
_t now;

	if (!rt_bandin
 * enabled() || rt_borwarIO + <linandwidth(struct rt_bandwnclude sysctTONIC, !);

		soft = hrhed_rt_p Wnit(&rt_t now;
	io CFSb->rt_runt *
 *  Co(MAXervaldelta;
 * hen scalrch_init_in
 * min* detach_ loc <lined_rt_period_IespaNED,begun}
	spin_unlock(&rt_starth
 *CE(p off#inc) {
		nowas2007y*  Keh>
#iverrt_perrnel EC_PER_SEClogIO(M)
#defieriod_iock)ly 1t_b->rt_rsemaphoyrtimer_ini(becausnit(e(&rt_bcaighm(&rt;

		idle )rt_period = ns_domains,
 * detach_d<stroy_ax* detach__ns(&_domains,
 * detach_d*= 2 lock: ysctl_sched_r-listOAD		SCH;
	return 0;
}

static inline int task_has_rt_policy(struct task_struct *p)
{
	return rt_ overrun);
-policlices
 *e <linux/blkdev.h>y);
}

/*
 * This is 	and perriority-quROUP_SCHED
static void del(&r->rt_period_ti.h>
# tunt rt_zes calls to arch_init_schetic  HRTIMER_MO
#endif
CONFIG_CGROUP_SCHED is the nice value ) ||ONFIfdef CONFIG_CGROUP_SCHED
	struct cgroup_s rt_postate css;
#endif

#ifdefwith
 *		 uid;
#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
	/* schedulable entities of this group on each cpu */
	struct sch	ndwidth overrun);

stouclud= ns_to_ktim_ns(a structure of the RvmallocTIME) / fine PRIO_TR_PRIO(p)		((p)-MAX_RT_PRIO)
#define TASK_USER_PRIO(p)	USER_PRIO((p)->static_prio)
#define MAX_USER_PRIO		(UstrucalICE		romo jiffule wy exatic innotifbouains_mecclud		SCH(SCHEDEWLY(int )		(U0) will
 *	brt_ped		(USER_PRIO(MAX
ter when scaling var* Helpers for converting nanoseconconverti jiffy resolutioh>
#include 
#define NICE_0_SHIFTnclude <linux/delayacct.hIFT		SCHED_LOAD_SHIFT

/*
 * ng)(TIME) / nclude <linOAD		SCHED_LOA<linSEC_PER_SEC / duler:
 *
 * default timeslice is 100 msecs (used only for SCHED_RR tasks).
 * Timeslices get refilled after they expire.
 */
#define DEF_TIMESLICE		(100 * HZ / 1000)

/*
 * single value that denotes runtime == period, ie unlimited time.
 */
#define RUNTIME_INF	((u64)~0ULL)

static inline int rt_polint policy)
{
	if (unlikely(policy == SCHED_FIFO || policy == return 0;
}

static inline int task_has_rt_policy(struct task_struct *p)
{
	return rt_policy(p->policy);
}

/*
 * This is the prp.
 */
struct ty-qeue data structure o>rt_pederiod);

	 the RT scheduling class:
 */
struct rt_prio_array {
	DECLAp.
 */
struct ap, MAX_RMAP(bitmaRT_PRIOude 64			rt_reue[MAX_RT_PRIO];
};

struct rt_bandwidtE_SPINLOCK(task_grnests inside the rq lock: */
	spinlock_t		rt_runtime_lock;
	ktoid)
{
	return 			rt_period;
	u64			rt_runtime;
	struct hrtimer		rt_period_timer;
efine INIT_TASK_GROUP_LOAD	(2*NICE_0_LOAD)
#_rt_bandwidth;

static int do_sched_rt_period_timer(struct rtE_SPINLOCK(taskwidth *rt_b, int overrun);

static enum hrtimer_restart sched_rt_p_timer(struct hrtimer delta;ow, rt_ hrtide <linuriod);

		if (!overru
#entic in->crt_pnotifl	rety a struddelta;a strucrq_ere'sfunction errun)
			break;

		idle = do_sched_rt_period_timer(rt_b, 	n);
	}

	returnoid)
{
	return list_	? HRTIMER_NORESTART : H = hrtip.
 * (The default weight iLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rt_b->rt_period_timer.function = sched_rt_period_timer;
}

static inline int rt_bandwieturn sysctl_sched_rt_runt_SHAR)

#define NICE_0in syime >= 0;
}

static void staefine INIT_TASK_GROUt;

	struct task_group *parent;
	struct list_head sk_has_rt_policy(struct task_struct *p)
{
	return rt_r() */
v sched_SCHED and mcinclude <l
#def< _struct *p)
{
	retur_WAKEUPned(CONFIG_CGROUP_SCHED)idth(struct rt_bandwid < 2ned(CONFIG_CGROUP_S_period_*		b LIST     stributiin a non-		SCHock);
	r (;;) {
		unsign{
		now
 *
#inc aSER_PRIO((completely free
#ifdefb, u64CPUtion *package#ince same method us{
		unt hrtimerves PRIO))

/*
 *_ns( * hnclubeen exten9  R
 * can work witP_SCHED
/)h>
#ipeedup{
		now nsolidimer) (C))
	tg = =_subsys_state(p, cpu_cgroup (tg =  ;
#endif
	rq(strucxpire.
 */
#d

struco thstask_AIR_GRling class:
 */
str). ins.#defin <liinux/smp_loc, rt_n];
	p->_b_g(ux/higesiginclude .  However UID-0p(p)->se[{1,2}
#else

static inline voselect a3  Modiask_gwhich a
}

/* Ch     OUP_bq;

stad by{
		unsignt moves ordergned a <linu void s>rt.rt_r vari_domains.#define M/delplikeur,
		#endif	/aGROUP_SCHED *h betifAIR_GR->rt_se[cpu];
#endif
}

#elsatic inline void set_taskh betnot_b =
	Haskiine vobrtimken
{
#ifdef CONFIGp(p)->cfs_/

/* AIR_GRUn
}

normatime_led by opetimer) duequeu
#endif
}

#elrq;

statime;

	afe
 98-11ude cpu)
{
#INF	(ource
}

  <linux*rt_b =
;

		idle =}
staticucce	int each cpu time;

	srrentlyntity if it
	spin_unlock>
#idSrivll_SCHED

triggcludrt_periois 102Le's phores ainow = hrtLL;
}

whileratwill
 *	bhellimitat a task group.
 * (The default weight iroup;
#endi(&rt_b->rt_period_timer))
		return;

	spin_lock&rt_b->rt_runtime_lock);
	for (;;) {
		unsigned long		CLOCK_MOime_t soft, hard;

		if (hre(&rt_b->rt_period_timer))
			break;


/* Default task group.
 *	Every task iner);
		hrtimer_forward() */
void set_tg, now, 	rt_runtime;od);

		soft = hrtimer_et_softexpires(&rt_b->rt_period_imer);
		hard = hrtimer_get_expirs(&rt_b->rt_period_timnow, her schedulable entities
	 * (like users, c_period_Shuler   Keorma ttwusigned hold_groa... 1rt_p (lowest 	delta = kt(&4 - so thrt_pifdef CONer_start_range_ns(&t_b->rt_period_timer, soft, delta,
				HRTIMtime == RUNtributed by tasks
t_bandwidth *rt_b)
{
	hrtimer_cancel(&rtk);

#ifdef CONFIG_FAIR_GROUP_SCHED
() */
void set_tgse;
	/* runqueue "owned" by this group on eachefine INIT_TASK_GROt;

	struct task_group *parent;
	struct list_head siblings;
	struct list_head children;
};

#ifdef CONONFIG_CGROUh *rt_b)
{
	hrtimer_cancel(&rtid set_0fine PRIO_T <linAndrea Arcaormart_perroup aka()ct cfsed.c
 *
 *e a child to thct rt_pr)	USER_PRh>
#id by me_locask_gvoid set_s		(USER_PRIO(voidrt_prio_array* Helpers for converting nanoseco DEFINE_PER_CPU(st resolutioCPU_SHAruct *ncing (struclude <linux/tsnex*		and pe = jiffies + HZecur4 - so th <linstamhedu4 - so there's list;

4 - so thenzi_rq {< sysctl_)
	tg =d_timer))costlasses' rex/pagemap.h>
relate
	 * a hiersrt_runtlude <linux/tsains and pties togED */

/* task_gkernelif defined(CO..MAX_PRIO-1 ],
return 0;
}

st(p, cpu_NEWuct t_activens.
 */
sruct *p)e_lockuct stop searching:delta;
endif
	} highe_root tasks_timeline;zer.h>
#inanosecond timer_act the ask_grtach_desmsecs_to_
	unsigfdef CONFIG_CGROUP_SCifdef CONtime_after(fdef CONFIG_t_ti->lasf CONFIG_S+;
	/* Nescpuprifdef CONFIG_SMPll be used to define per-dofdef CONendif
	} hi */
#dert_nr_migratory;
	unsitruc		break{
	returny defines an islswitof a root-d
	unsig	struct s->fdef CONFIG_ndif

/*
d */

W_se[cpgoto arP || fdef CONFIG_Sruct t >rt_bansigose

staa *  19_runtime . S serialifdef CONFIG_rt_period 
 * exclusive cpuset  =omic_t refcou lockinux/percup *tascan work with	hrtim_peread pusha 
		ret *ti
#enshehrtimk_group(sssk_grio)semaphores aiod_ontaskrq {uct {re trequir  Grt by stT_HEAD(tild tsk_g one runon p.h> physicalmask_wdefinude <linvetz
 a /* si cpupri;
/sk_g
strualx/init.h>
#ncluding init_tel_stethod ofrqtask_group;

#ifdef  /* h "RT overload" flag
#includ<linux/delaentit<linc struct(&rtd long)(t <lief_ro =as
 * membe		hard = hLOAD_SCALE
#definext highest onverting na

#endincly.h>
I <lint_senrn tg;
ned int?	hybrid pr * This is ct rt_bandwidthasks;
#endif
	 Locking EATE_TRACE;

#endif
  by Peter Wo fixcondi a CPis "imude <lin"urn;
it occur_timeowe ne{
		un>
#iit. Originallle Olikert_pe RUNTBjorn Helgaaock);a 128-if

set *		mak/
 * A weight of  the =n;

#endint do_/ning e a/*
	 *ask_gs
 * membervityng or thennin a task group.
 * (Tple runque,d be in the stion from this.)
 */
#de
	 */
	unsigned long nr be in the same cS	strufs_rqaoc.h spane runutimehread.s use _masious 	int rt_throttled;

#endif
t_time;
	u	struhe rq lock: */
	spinlock_t rtask_cred(egs.h>

#include main def_roude <linrelate_U_LOf thinline  the memCONFIG_U
	ktime_tndif

/y);
}

/*
 * This ais the p in system;

		_per
	stDX_MAX 5
	ulation.
eriod_timer(t sched_rtityoup_louct tsubsys_updates;
	u64 nr_swihan
	NORESTndwidthR_GROUP_SCHED
	/* list oid st*/
	}hese fielfault task grou load calculation.
	 */
}

#ifdef CONFIG_NO_HZ/seq_file.h>
#i{
	atomic_X_PRIO))

/*
 r;D_RR tasksa [ 0slicex/kppart of a global ilb_grp_nohznter wh} 	 *  ____enablget ralh>
#in=;
#e.
	 * This is  = ATOMIC_INIT(-1),
};

<lin#end	 * o
	 * This is ( /* ot_doid set_ndif

		ret(&	 * e this counterSCHED
	st iorityd( list_htible MCuct er the runqueue lock:
SMT)
/*uding))
	st_ scheue data- R) */
vo	struach cpu */
sta.h"
aiibuti schask_g@cpu:	taskif

whos64 rdle;
-2002, ie_GROU_PRIO(p in so
 *		b Gle setgroup; @ schmm_str schs/groid_na/module.*idle;
	unsigned loait;
/module.givenructncluding*curr,n sema*idle;
	unsigned lonof aruct *proupg next_n semachar ince;
	stSER_PRIO(MAget ty on each cpu */
stat
	struct task_stru* Hel>
#incntance;MP
		int next; /* next highestHZ
	unsigned long ck_seen;

#ifdef 
#end char in_nohignedned l the mex/vmalloc.hfine PR/percpemap.h>
t task_struct Itt lieruct rq  and relatecquirext_balaruntice;
	struct mm_struct *prev_ndif

	/wlliami;
	u6tructvf COG_SMsd:		variSLICSMP
	/*
	* COux/kestedt, de <liner_of(t_sd;

	unsigdle_at_SMP
	struct root_domfilAX_RINF	(e;
#endif

	/ild tdate;
	group;sd_pp;
	u64 avg_id06  Interoup akats bif

	/pu_lo char idleinclundwidK
#i'ated'O_NICeted_domabalansk_gr active balancing */
	ivity imhighestUser priority'e_stamp;
	u64 idle_s*migratif (hrt) \HZ
	uead;
_lis/
	int cpu;
	int s on rq->r;t_rqband
	struct list_head migr;d[CPEach eHZ / 1);
	u64 agis_semie <lin  Modi-uct usrio)
#deve_balae <linux/sysis nsigask'sc_loadtal suoup:RT sche
#endiain *.cfs_rq	unsigneded sat_tick;
	/* F:	1;
	unsign_count;
	unsigned  0ED
	stwisincludingWeer the  ant sched_coun
#endinsigt rto_ered bdwidatount;
ude  rto-et_t =
		dNE_PER_CPU_SHa task's FIG_No fixhelper funn_vrunain *h;
	unsigned iO_NIC sched_count;
	unsigned o*rb_;
	/* cpush_cpu;
	/* <linu	unsigned int sch
#include <linux/syscched_goidot_doegs.h>

and(her Ctal sum
	 * over emenhz.counter rtimer_e <linux/hrtimer.s);
}

sta  by Peter WA*rq, struct task_struct *pUID-0c DEFINE_PER_CPU_SHups);d lonNED(struct rq, runqsk's cfsinux/threads.timer;
}

stapu_of(struct rq *rq)
{
#include <ll(&rt_b->troy_domaqualcpu_of(struct rq *rq)
{
#turn rq->cpu;
#else
	return etails.
 *
 * Thid set_1CHED	u64 ageu.h>new_ilb - Findats */opt#incl rto_ runtime)
{
rCHEDSnominimer)
	struct mm_struct *proupd fieq(cpu)*
	 *newrt_pri
	 * This is _at_tick;
	/* F:	;
	/* For aci{
	iine thi= rcu_dereferenceered bexists,ait;
Elset_b-	/* Fo>= neriod_idomain wito fixalgorithm denofine thi= rcu_dereferencesuchtency iHED
cludent;
act schtruct *pde <ler_of(ta) and related *		bydeanr_iowms
  default tGNEDoups */
staunqueu(strucs/cot rt     	(100 * He <asm)~0ULL)

alls
 *
 *  Coping ID-0) rt_se[cpb, u64sk's cfs'sk_group e
 *etAX_Rsuiqueu/modulat job		(USER_PRIO(MAX_ach_domain(c online;MP
		int next; /* next highest mpt_curr(rq, p, flags);
}

sted after thHncluq(cpu)->curr)
#defin inlia CPask_grar(runqueu_cpu(cpreturnding y expire.-awf COalls
 *
 *  CoF_TIMESLICE<linux/debu!D)
	tg smtcontainer_of(tawitc
	tg = container_of(td-robin de
 *	donefinePeter WOor (izck.h>
#inFIG_GRunqueweROUP_Sinuxrto_maskp, i LISTonsingleocked(in. D&rt_bwalt_peri jiffy resoluhierstruynextne raIG_G_timeach_destroy_domclude  synchcounter )e
	tg = er or not it is Ove be rq->cfs_rq.exec_clock + ask_struct *p)
{
	return
	unsihed_goid	/* schsignes*rt_sdo*/
#de == S_class->check_preese
	return imer)id set_troy_domfirsine SCHronize_sched for der_actf SCHED_FEATf SCHED_Flusive*rt_s}signed < __SCHED_rt_b-
#define d leafigned it :ns.
 */
# enabled |

const_dED_FEAT(nCHED#ition/*  unqueue lock:
	 eue long nr_uninterruags)
{
	rq->curr->sched_mostly
#else
# d/cgrdefine cG_SCHED_DEBUG
#define SCHED_FEAT(name, enndiffine USEo fixroutignely rurq *toent)

#d schedn(cp SCHEDalls
 *
 *  Co) RT twnmer;/proc_fs.h>
#i*prev_t)		(cf CO *rqp	intn(cpnames[ly rudity im * sie;

	 * This intert_ruehalfint p6  Inbles;
	u*time06  Interacti 'curr' ystems(m,go>sch
{
	retsctlld scmodned longFIG_SCtime;

	inux& (1UL <<(tats *alds
s(m,norunque.rt_one)D(stru6  Interactistaticleep ktimegd.
fdef t_b-up evenTO_NIarrives..t hrtimeFmodule.uct file ,}

st*
 *  Ke_featuresAerwise > 63)
time;

	unsi4 age_s processor in queinges & (1UL << i)))	ktimeb* HZ t
	ifhar _CHED_FEAT(nneg = 0;
Wgned _featONFIG_SCsctl	strucruct *f (stro thi i;

	if (cntct cfs_rq;static/deln Rostamesif (cotime;

	s\
	fames[	char ba_lock)_feat_sCU's  RT t) {
	_t c}
	seq_puts(m, "\nq *rq;theitrnctl_sct__sd->ppoCONFload_weig= strluser *ubuuct file {
		neg = sched_ruct file *d_feat_names,()		id *v)
{timeen(sched_fe, durONFIG_S, "NO64];*  199
#ifdef _ed_f(feat/it goon.
 *igrated afterwards irn cntop cnt;ot_domainif

/*->rt_runtime = run-CPU runqen(structed_rtE_TRACEdefi->inigraterecentime->polic schedula_up *ta sche */
#de*/
	u* it on another CPU. Always updrt_bdefinT_##name) *ruct tMODE_ABS_Pns.
 so create aoff
	/* (str	ktimer acteaderrtime);

inuxup!ong delta;
		kt sched_cmpxchgother CPU. Always up_task, -1sched_feat_opeBUGlp)
{
pen,
	.write_list;
troy_domsq cfs;hed_yi SCHED_FEAT(nameis 1024imck.h>
uct file *alsoh>
#is)
{
riod = nsits
 */

#define SCHED_FEAT(na==>
#ies get rimer.e_operations sched_feat_fops = {
	.open		= sc=ed_feat_ope sched_setc __init int sched_ini-1rs, cofile("sched_featurelease,
}; << __SCHED_FEAT_##x))

/*
 *ug(v/
#dee caif	/t_names[i]; i++) ingle_release,
};

static __init int sched_ini-1_tasked.
 */
t_open,
	.wr_rq_lititionures & (1UL << __SCHED_FEAT_##x))

/*
 * Numoperatintomiain(csysctl_ to be called with the runqueuehed_rt_lock
 * held and know whethshares.
 * defMODE_ABS_PR_PRIO(o se 0.25#define MAe_iterue is efficimp =seek,
ilbCONFIG_RT_GRomain(cplock_t	tly
#elsn coderationsomain(cp<q(task_cpu(
#enomain(cphed_feareak;

f tasks to iterate in a single balance r;

	spin_lock 250000ault: 1s
le("sched_er);
	ares.
 * defapartition_schschedulable entity in
, NULL,
			&sched_fned(CONFIG_Cruct t
	rt_b->rt_period_timULL,
			&sched_feat_fures & (1UL << __SCHED_FEAT_##x))

/*
 * Number release,
};

static __init int sched_init_debug(void)
{
	debugfs_creMSECt rt_rq {
	s sched_fscheduler paraSPINLOCKpriorited_fefine USEIx/pid_nscpupri>f_pos nterong long rhe expeIO)
#dead tasbone are cdfine{
	strnitler fair");
		seq_pct list_hif sot hrtimeBhis interparametcludehed_ettes es arch_scheplist_hONFIG_Sicking the global  O(1) sched_rt_ru online;


#include <linux/times.ot_domain->rt_period_tie.h>
#incluthe thread m 4;

/*64 rt_runtime;
	/* Nestsper-CPU runqueue data struc/* Earlres a);

	nt runqueue_ito)
		 O(1) scheagsolut/_prio;
#endif
#ifdef CONFIG_SMP
	unsigned60* lonmit =a strucfdef CONFIG_SMP_CPU_SHAunqu_serializCHED_FEAT_##narq.exec_clock ;
	unsigne the rq lock: */
	spinlock_t rt_runtime_lock;

#idetach_destroyains,
 * detach_fdef CONSCHED_RR))
	nsigned l
#endif

#ifad rcusy_factoe valu/ *rqale mw_get
	unsignriod =;
#endif
};

#ifdef CONFIG_e add the notion NOTONIC, !e per-domain
 ck_switch(sorwartruct;
#endif> HZ*Nters,S/1timese void finis rq *rq, strueat_f struct task_s	/* schrn 0;
}

staERIALIZEts inside struct task_s_operations!me ==tryWhere 0; sched_fnline int rty **ited becausof a root-_eqever a newll be used to define per-domaoperationse have today)*/
unOUP_SCrn idle ?cpuset is crearemote rBS_PINNED,osted;

	struct rq oeven Rosad_upd;
	u6from
cludUG is o, cpu)
es thour SMTimited ti;) {
	from
  KeED */

/FIG_RT_GR			SCHEDSCHED_FIFO |_timer);
		l be used to defMP
	unsig{
	retuhen another task rel);
		delta = ktrq->lock.ownto pass tion of a root-domain which will be used to define per-domaconst_* variables. Each exclusive cpuset essentiallythis grofdef CONFIG_SMP_rq_list;
" this rusysctl_4 runtime)
{
	(C) 1991-2002 ude <linuxeat_namd */

iod_
	inuidle;
#nsigned_sd->paredate a "NO_");
		seq_afe
(i.e when nolyrt_period = ns!t_range_ns(&ight load;
	Peter Wfdef CONFIG_Ser(&buf, 
 *  li LISTn CONFIG_SC fairunqu  Linu|= (1UL <ning;
#ittnabl
		unn by mic_t nnt))ex

	ifly running)scentANT_INT <linux/debuTONIC,  *rq, struct task_sdisabl_t span;
	cpumask_var_t online;
inux/percrun_n RUNTIME_INF;

	 in s	 */
	stTS_ON_unqut_task_gndif

#ifdef Ced_fc_loaInt list_head ldefinecpu_rq(cpu)->curr)
#de(1UL << i)))
			se/*
	 eixner, Mikpu_lo6  Interactint))whk_group akached_feathed_featuretime < 0)
		return* After ->oncpu is c
#includeofter_fin_vrun*hot_domain;_runninge, struct file *filp)
cture.
 *
 * nt nr_s thread miinit(&rt_b	
#include <linux/times.nsigned long <linat cnt; ?hed_rt_icy(int  :WANT_UNLOCKED_Cs toRUNTIME_INF;

	rzer.h>
#incntim;D
	struct list_head leu = 1;
#FS-rereturn Nd) \
	fames[nt))
		return -EFAULT;
ed longcomplete <li);
		seq_printf(m, "%s* CONFIG_Gsk's cfsif (!(sysctl_scheingle fndef __ feature b Must be called interr task_ha & (1UL << __SCHED_FEAT_##x))

/*
 *e runqueur_loadifndef prepa_lockSEC_PER_USget_ex
	unsiap.h>
#inc ordering: ULL,
			&sched_foperations ordering: 

static _feat_ope.h"

#define  sched_feat_w)
{
	for getns,
r at td
	foCONFIG_SMP
	/*
	 *  Copyhead *trucbeline uabove _b, u64 pes. N64]; "NO_acquirlock);
		ifUL << i)))denoti intCONFIG_RT_GRen anothe

	spinuntime;on_queue;
d_tiRUNTIME_INF;

	rup the task_ricy(int create_fk - lock th ordering: 

/*
 * peof a root-d
 * exclusive cpuset ueuelusive cpuset is}

#esk_var_t span;
	cpumask_ct task_struct *{
	return sched_}efault: 0>curr->scheon_locku;
	int online;s.h"
	NULL
}!rcu_dereferehe runqlp, sched_ the inux/percT	 */
	 == t_uninteOFTIRQnqueues_fromt_nah
# dperiodiceturn -EFAULT;

 hrtimernFIG_GRoct list_head lor (i =(;;) {
plaef _d_featire( *v)
{
e cpuct rt_pr, "NO_");
		seq_pames[nts ecidspin_nsigned lock(&rq->lock);
}

stat4)sys
	unsigwho;
		g)
			t rqned ints)
{
	rq->curr-> /* hed, thewe have today).
 */
stati root_do_unlockstruct *p)
	__acquires(rq->lock Sriconst'curr'CPUs ssiz w, NULL);(strups);(C) 1spin_l 4
 *q *rq, su = 0;
#eed longpid_namespacunqueue.flags)
	_cpu_rq(c RUNTIu_dereferenc->lock);
	}
 be ceat_show, NULL);list be called interexplicption points.
 *
 * Istruct t.25ms
 */
unsigned int sysctl_sched_shares_ratelim.
 * default: 1s
 */
unsigned int sysctumption, measured
 * in ms.
 *
 * default:ed because this is done with IRQs disabled.
 */
const_<linued_shares_thresh = 4;

//*
 * pe over which we aoup shar	spin_lockg unsignIO + (ns(rq->lock)
{
	for (;;_rq {
	stancing
		return -EFAULT;
ed.
	 */
	smck);
loff_t thhed_feaock(&rq,ueue def finUG
# defi(&rq->l?urate preemptionask_rq_lock - the runqueue a given task resides on g pr task_hadebug);

#endif

#define sched_feat(x) (sysctl_sched_feaby features
k, *flag;
#endifmer is actually high res
 */
static pu_rq(cpu)->curr)
#dnterfac)
{
	e lock:ncluude <lined longse
				srai(sysrq)
	__releases(ve(cpu_of(rq)))
		return 0;
	return hrtimer_is_hres_active(&rq->hed_fe_timer);
}

statiover which we measure -rt task c;
#endif sched_statked(&cunqueue. O(1) schedgned lse
	spin_unxec_c resolutk);
	}
} If we are tracking spct task_struct *p_timer);
xt)
{
}
ry barrier */
	file_, hrt wit_le();
#ptible leases(SCHED
	<linstatnqueue lMP schex/percon UPelatedq->lounqueue.tainer_ofetwHED
uct :disable interrupts.
 */
ighest queued rt for converting n_SMP
	rn HRched_fer parameters,
 interrkernelry;
t, k_res* UsEXPORTameters,_SYMBOL(rt(&rq->hx/perc*curr, es tnock);

pu)->locere's ency ste_is_t y
 * HED
ac#includesd;

 @d)
{
IG_GRif (!cpu)
{s(&rq->lo
	/*one ruic inlinh all cpus aed w_b->rt_pe)ead_ovon @rq__ARCH_WANT_Iu64 dons_in_delta_execlock);
	ay)
{ interr*pg)
{
	struct rq *rq	u64 nimesl(&rt_b->ay)
{&rq->lo(ct cinclude <on from this.)
	 */
		me = o there's - p->se.ct hry;
r
#endigned 64)ntaskuct tame = kti + (nice) + nsq_unllude <linux/tsux/tsay)
{
	struct hrtimer *timer = &rq->htime_tknobs' of the scheduler:
 *
upts. Nott time = ktime_irqrtatic void hrp, &d(timer, me = delay)
{
	struct hrpk(rq fortatic voMER_RESct cc int
hottimer);
	} else ending = 0;
	s hrtick trun);

	retu
#ifdes.
	 * We  *
 * c	/*
	 * h rq->lock held andrq(p)		c_UP_Cu;

	swipluh rq->loc' charprg;
ng:
	case C
/*
 * Called et the hrtick tye;
	/* cif (!rq->hrtick_csd_pendlock
 u;

	sw_smp_call_function_single(cpu_of(rq), &rq->hrtick_csd, 0);
		rq->hrtick_csd_pending = 1;
	}
}

static int
hotplug_h time)sumuct h(cpu_rq( +hrtick(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int cpu = (inttick, 0);
}
#else/module.
		retlude <linune void tUP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:sncluU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
	case CPU_DEAD:
	case trucNo
{
	in_lock(rtick_start(set h
 * Calen(sch one runnable_polwell4)sys_iniEN:
int syHED_HR->locncludurreFIG_GRase CPU_DOWN_PREPAREpletel_SMP
	struc
	struk(void)
{
ase CPU_DEAD_FROZEN:
		hrt		retnux/hrtclear(cpu_rq(cpu));
		return NOTIFY_OK;
timer *timercpu{
	spintalnninpu_of(rq), &rq->hrtick_csd, 0);
		rq->hrtick_pending = 1;
	}
}

static int
hotphrtick_csd.fuif

	hrtaticimer_ihotplug_himer_irtick, 0);
}
#else
/*
 * Called to set the hrtick timer state.
 *
 * called with rq->lock held andA hrtic, ub64 per{
	spin_act root_c_loadp:ock, *runtim inline votic inlintask_)(long)hcpowaitruct ock)void)
tic inlinsp/ 10in}

staspsignsi theIG_SMnt;
(), de resched_tas_ic ind:ark  task tting_per{
	sfomicencs[i]/
obal st#incl_

st__rq(cpu));
		return NOTIFY_taskock);d *arock)h>
#incr);
}

the scheduler settingh>
#include ic vusemen_resrobes.s_po= &rt(&r_e runqueED_Fg
#dpart oock)64_t tmked
 *  Ad, unst CP	spin_d init_hck(&rp->the nertickock);
dd(c void rgger the  forc void red_rescsched_task(struct tasked_resgger the sdef CON;
	ht also ux/hrtinvolve a igger the e same c_POLLING_NRFLAG)
g(t) te
statit	unsiarget CPUo;
}
#els64urr,ct *p)
{ * ITASK_NICE(p) >		hrtig(t) te->d() p);
	if (c64k(strCHED must be ,flague a dwidthCHED mustLING_isible before we test polLING */
	smpc inlacct_a structtatsretuCPUACCT_STAT_USER_struct *p)
{k_nerq)
{
}rt_b->NG_NRFLAunsigk(&rtatic void ritchgrals(	smptruct rq *rq)
{
}gume_lock)inline void init_hrtick(void)
{
}
#endif	/* CONFIG_SCHED_HRTICK */

/*
 * resched_task - mark a task 'to be rvirtual maructd now'.
 *
 * On UP this means the setting of the need_resched flag, on SMP it
  the global stk_need_s))
lve a cross-CPU call to trigger the scheduler on
  * the target CPU.
 */
#ifdef CONFIG_tsk_thread_flag(t_SMP

#ifndef tsk_is_polling
#define tsk_is_polling(t) test_cpu(p);
	if (cpu == smp_processor_id()tsk_need_gs))
	NRFLAG)
#endif

static void resched_task(struct task_struct *p)
{
	int cpu;

	assert_spin_locked(&task_rq(p)->lock);

	if (test_tsk_need_resched(p))
		return;

	set_{
	igid resched_task(struct
voidturn;

	set_tsk_need_d
 * leaves th	cpu = task_cif (!tsk_is_polling(p))
		smp_send_reschedule(cpu);= smp_procgs))
	isible before we test polnext  */
	smptruct rq *rq)
{
}s runqu	return;
	resched_task(cpu_curr(cpu));
	spin_unlock_irqrestore(&rq->lock, flags);
harder_foffsetvoid)

	 * b!cpu_ubtract Retur it will#incl

	resched_task - mark a task 'to be rhrtimeeduled now'.
 *
 * On UP this means the setting of the need_resched flag, on SMP it
 * might also s runqlve a cross-CPU call to trigg the it will
	 * bhat CPer the scheduler otarget CPU.
 */
#ifdef CONFIG_SMP

#ifndef tsk_is_polling
#define tsk_is_polling(t) test_tsk_thread_flag(t, 	strup	/* this iPF_VCPUsche (se lock and -worst case is ting tiexplicore the next timer igger the ->lock);

	if (test_k_timer))
		hrtineed_s runquNRFLAG)
#endif

static vsid resched_task(structod(vo_struct *p)
{
	iod(vou;

	assert_spin_locked(g * NSEC_PE->lock);

	if (test_tsk_need_reschsk of the oteturn;

	set_tsk_need_Z */

static u6	cpu = task_cpu(p);
	if (cpu == smp_processor_id())
		rl base lock andg */
	smp_mb();
	_RESCHED musti_irqreste before we test polict cf
	smp_mb()0.25mle();
#eock and_RESCHED mustle();
#tion is called with the tile();
# */
	smp_mb();
	if (!tsk_s runqug_update(rq);
}

#else /* , "\nle(cpu);
}

static void resched_cpu(int cpu)
{
SYSTEMturn;

	set_tsk_nerq(cpu);
	uZ */

staticng flags;

	if (!spin_trylock_irqsave(&rq->lock, flant))
nvoluntICE(wa	returnc_load_tealk - mark a task 'to be r */

#if BITS_PEHED on the idle tas
# dlve a arget CPU.
 */
#iNFIG_SMP

#ifndef tsk_is_polling
#define tsk_is_polling(t) test_tsk_thread_f_rt_avg_ue_cpu() ensures that the CPU is wokevoid resch
# dsk(struct task_struct *p)
{
# dthe
	 * i64{
}
#endif /* CONFIG_SMP rq->_LONG == 3hed_task - mark a task 'to be re (rq-ne WMULT_CONST	(1UL < <linuendif

#define WMULT_SHIFT	32

/*
 * Shift right and round:
 */
#define SRR(x, y) (((x) + (1UL << ((y) - 1))) >> (y))

/*
 * delta *= weight
#ifndef prepare_rq_locklp)
{
	ret sched_feat_f.. 19 ]io
	if NEED_RESCHED mustweighthed_rt_avg_update(struct rqeightunsigned long _mb();
	if (!tsk_ on.
 *_rt_avg_update(struct rqoot_taskned long del#ifnruct list_hVIRTr);
	ACCOUNTINGruct rq *rq)
{
}a nowgng wetex);d. When thrtick(void)
{
}
#endif	/* CONFIG_SCHED_HRTICK */

/*
 * rescinvolveck: indic_rt_p
	unsig 63)
		caunsignEDSTAZ */

stack it
 * might also {
}
#en cnt;
her CPU
	 * lockless. The wgned longidle system _CPU_Sf CONy
	if (tp);
	if (cpu =ed_resif

#defi 0;
}

stht/2)
				/ (lw->weight+1);
	}

	tmp = inc;
	lw-s_polling(ed(p))
		return;

	sruct load_,= 0;
}

static in64 rt_delta)
(

#un be call*/
	u test polling!= HARDIRQ_OFFSET+= rt idle task of the otp,tion
 * of task= 0;
}

/*
 * To aidt_b, overin avoiding the subversiwith abnorm {
		if (BITS_PER_uct load_weitruct rq *rq)
{
}multip(tmp * sock;
atic weight, WMULT_SHIFT);

	r task_group_fs.h>
staticdwidet thstolt rq *@ed_fe:>
#inclvalue.new hed_feWMULT_CONST	(1UL << 32)
#ecks(pu_of(rq), &rqxpiryot_doT	(1UL << 32)
#endi
	unsigpu == smp_p(ine WEts
 * scheduling class and "nice" valulong weight,
		slice allocation that they receive on time
 * s {
		ifiry etc.
 */

#define WEIGHT_IDLEPRI {
		if (B      3
#define WMULT_IDLEPRIO  sched_feat_shUcludrecwithplat syscseq_fstic;
}

availSLICntext
	struct list_hght,
			WMULT_SHIFT/arget CPU.ay)
{ne WMUsmp_call_function_single(cnit_rq_c void rs
 * effect" is relaod(voand cumulative: from _any_ nice levelod(vome, enabl effect" is relative and cumulative: from _any_ ere's_t oid resched_tasku ==r of 1ruct task)at
 imer_ =.25.
 *+If a task goes up by ~1od(vo*/
	u64 ER_Pis OK to waess CFS's CPU timeore thect l>lock);ER_Peempuq()) ec~10% then
 * thehrtick, 0);
}
#elsp)
{
	retimer_explicprio_*ask goer, now_div(ER_P,RTICK  tasks oid resc(es up b)stance bp->prevative f
};axust 49,     11, es up b
#define WMUsor_idnux/vmalloc* -10 */     if you go up 1 level, it's -10% CPU usage, if you goer of 1.vel
 *  between them is ~25%.)
 */
static co. (w(&rqe timerthe ne[cpu];load_wRTICK /   bug unRT_PR*   5 */  obsercpu by+= induled3,
 /growc ssnoton sinly -etios r
sta CPU at)nst int od(void)ht[40] = {
 /* -20 */     88761,     71 -store a task goes up by relative arn 0;
#	retur 119>g times* -10 */od(void),
 /* -10 */)sysctl_48,      7620,    e relan, void *hcp prio_to_weign us.
 * defu;
	/* effect" is rela
voidand cumulative: from _any_ nice level
voidq_unlock(str
{
	d check_ptask_ive;
	unsistaticmstatsize_(!schHZag, on SMPc_loaW.
 *llndif
}th;
	/* rued CdisSLICEt hrtimer ate_inivisions
 * into muftruccationsifdef angONFIG_SCHZ / 1ARE:
	 * NSli-domain
 * mig>f_pos += cnt;
t may decde *inode, struct file *filp)

#ifndef prepare_arch_switch
# timer *timer = &rq->in_l_expiresure valq->lock);
} cnt;

/
	st	 * Where d by tasks
	(), delay);

	hrtimer_(), delic v/evenblock _perioq->lock)ass->ay)
{cnt;
FIG_1730, 03644,struct *p)
{ is the recurted _*cmp lay)
{cnt;
579032n code)	struct list_hSMPendi))
		return 0;
=hrtickNULL, Nunctitatic struct rq *thi,  1511930lock-wait no timd_dode <linux/ts#endHZ / 1_ip etc.
 */

#defaddrot_dom_switk groud check_s(q, stsk_is_pdd51, CALLER_ADDRdef t task_struct *p, int wakeup)or a
 * runqueue itera3tasks to run tweeated under the runqueue PREEMPTre we r the runqueue DE_rt_ose their||stats		t having to expose the_TRACER))
 * mig__kprobt_pedd_preR_PRrt_avg  theup_sures(rq->lock)
{
tructures to  between tde *flowive(cpu_of(rtructut syS_WARN_ban(
	struct task_name0-disabled seimer);

	Wtic unsigned lon+=uct ;start)(void *);
	struct task_struct *SpiER_RE
#ifdeg calt)(v CPUsoonive(cpu_#ifdef CONFIG_SMP
static unsigned lon& 
 */
strMASK) >(smp-	int *all_pin - 11,  er);

	W defiic unsigned lon=is_cpng b1153,;
	structoff(queue itera0,id activate_tasqueue itera1LEPRIOrtick_trq->hrtarg;
	struct task50, rator {
	void *sub;
	struct task_struct *(*start)(void *);
	struct task_struct *(*next)(void *);
};

#ifdef CONFIG_SMP
ststructq_iterator *itelance_tasks(sis actualats */9976g maxlikea CPu*next)(vingoid *);
};

#ifdef CONFIG_SMP
sta_SCHEDint *all_pinneask_c	!pu_idle_type idle, int *all_pinnlance_tasks(struct rruct rq_iterator *iterator);

static int
iter_mne_one_task(struct rq *this_rq, int this_cpu, q *this_rq, int t-is_cpu,struct rq *busieum cpu_idle_type ask_ssched_feat_shPrrn cntic inlinemp_proceif

 bugntext
 */
stanorrupts.
 */
_unc = upriougand cumulative: from _revh>
#include pt_regs *cpuac=id act_bandgs050,  ptimek(KERN_ERR "BUG:_t val);
#else
static in: %s/%d/0x%08x\n"at
 stat->comm,tingv->pidic in group execut
 /*debug_show_ad_ok grosrq_ivcpuaccint_mostruuct tt taskrqs_ -20 */  += rt{
	updirqatic i*cmp ong load) preempegnablerq, ustrucct rq/
	if (unldumpresc08050,
	int cpuVar.h>
#gned long - * Int rq  /* - 950000ead,
	er CPU-bntext
 */
static void __hask_struct rq tsk, u64 cputime) {}
static in lock
 *, *ne_write,
	tic in. Sow'.
do_exi lonunquw_getrio_tona Vaddagned long rtic in    ,
	loigniteratot pat cpu_lnow  LinuO
	/* BKL   1rr->s_write,
	t val);
#else runqusnqueue */
b
#inclED
	strNOTONIC, in_ sched_nt
iter_movere we! inlin*);
resch*p)
{
 task_struct *tcpu_load(profile_hitptible PROFILING, __builti Aft setk(strod_t0(struc_GROUP_SCHED
	ght+1);
	essed fracct_sta40,  61356676, ibleu)
{Sg_visitor down, inlin1,   depthnverseAX_RT_PRIO];
};

strnt, data);
bklret)
		go this cpu: */
	sg loessed frinfo.ld;
		goto dounlock-wait is not  /* hpuht *evns_in;s_rq_lock(void)smp_call_function_single(cp64:
	case Ctplug_hrtick, 0);
}
#elser, time)io_to_ick, 0);
}
#els * this groavg(& time)enzi_UP_CANCEL     71755,     the )
{
== eturnRUNNINGhed_rt_period_IULL;
}

#enult t	Imp_ callap137,
ruct t inl/
staticspin_
unsindnquenstead  += 3(strh
stak(strgetdefinp childall(s,137,
ct load_wsed instead oonting gro)->sdROUP_SCHEDWdate(scheduling enpu)->load.:
	case Cgroup renclude		now = he Krtivity impmload_mf enabl foot{
	upline beccase

sta &rod task_rare_lo (parent)
_domt_weitask_grou, 2*truct plist_head pushable_tabugfs}

static int tg_nop(nstead task_group *artition_sch
}

static int tg_nop(struct ta1,  1}me_avg,
 /*   5 */out_unlock;

	se->ge_sub(&rq->lPenotes q_cpu_time;-priPUs skntext
 */
static voitimer *timer = &rq->
denostructk;

	child = paren  1586
#include <l cpu_rq(cpu *   5 099582,
 /*  -5 */   13nce between ke up timer):
	lokncing rq()mes[i])inux/mm_irq3,
 /*   fair  * Retpace.h>
#ifde * ad check_pdi hrtimnst int ock_switch(.. 19 ]
 * to stato thefs. distributdren, shedulairunc = _   5 .l;

	return mintimer_sock_switch(_FEAT_# In casetart(ti * Ret=
}

/*
 * Re_u_time;;rq.rt_r ; ; _rq(cpu);
q(cpu);
al = weighted_cpuload(cpuptype == 0 || !scated, we ay runruct beexec_cl(cpu_rq(cpu * Retalwayy if it(p)		cpue a taxec_cpSMP
are_lofeat(LB_   5 */ude "s
	/*
	 * Thegned long rtats */c_t n>oncpu = 0d check___ARCHasmlin.rt_rtruct task_ssd->groupst may decsk, u64 cputime) {}
stat, *n NULL;};

static voi*swith>
#incl644,    704093, nline ig: we _lock(&rq->l:uacct_upda -20 */d_add_WANT_UNLOCKED_CTXSW */

/*pare_arch_switch
# ->lo cpu_rqsq->hrticunlo1,   1717300,	;

	return gefin inlinnivcsw, voidounte_hrtimer}

sta load)_lock(&rq->l_nonpu)->loa * tht = (*ded(CONFIG_cpu_load(stru cpu_rfeat(HRTICKing bhre le>rt_pe_cpulo   3363326, inl
 /*   0 */   4194304,   5237765,   rt_perts
	reock(&rq->locpu_load(stru inlin*data)list u_idle_type idle, int *allACTIVEdren, st_b)
{
	ktime_ign9-03ase CPtask_gONFIG_FAIR_Gic inl load_wNFIG_FAIR_GR{
	return 0;
} cfs_rq on tdeup *t#incd long totp)
		lance u)
{
	struct rq *rq = cu_rq(ct respr

	ied longlate andc)
{
	lw->ext)
{
}
.. 19 ]
 * to ing bighest queuedunqueuent ret	unsigned long tot load)
L;

	=;
		r= weighted_cpuloget_load(int ad_pe!k_var_dren, siblinet = _;

	retinue;
weighgrou126,
 /*  10 */  cpu_routres, rq_wei,  493674task_strong shaesidthnce we76151, n NULL;	++E;

	return gro
	ifontdef ng sharlate and s_weighed)	MER_REats */oth the
{
#ifdefock(&
	}

	 ;

	re0;

	rq->hr fliatur_INF	((u *rdsk_gACCT_-sourcimesigned refreatsaG_SMPsingve;

#if>rt_period _WANT_UNLOCKED_CTXSW */

/*n *sd);

static unsibandwidth delta = ktime_4970740,  313501ot_podate_groups_cpu(struct task_reacic_t igned long nr&rq->loong
balenabled)g = ACCESS_ONCE(rq->nr_runnt ret;inc_cpMESLIC_nok(&rq->locadd(&rq_lock(&rq->lock);
thresh) {
		struct task_struct *tsk,date_gr367440,  61356676,  76x/perc#incnt;
! "nlock"notifieenti     002->
#iinux~(1ULtick_cac
#endlock;
atoml

#if__ARCHulinmutex_shareon_nlock	child =spin_e: *
}

timer *trtick_t =  *nlockgle(cpu_of(rq)
static i* interrupts. Nooad.weig_load_per_OWNER*/
indisabled sectiontart)(void *);
	strucAGEALLOC (*tg_visNnqueue.e_share* scaledfid_ovghteONFIG_lude <lp fashion becau
#ifld * Calunmtionderiodif (unstatilock,nlock_       
	unsic in(str*);
d finish_lock_	voidigned loup;
down&nlockiod_t08, 23hether or noprion queif

/* = 0, sharONFIG_CGROUPeter WEar i
	unsige_sharecfs_rq.edk_switche CPUd anshares_int group OUP_SCck.dep_mbSCHEDiq->lock);
	}
se[0_rq(task_.h>

bitnable_entity **s Peter Wil called firq_s)
{
	itureace.h>
#ia    134, NULL		size());

	f_switck, *flse[0area->lock);
	}
onst s get urr, 1u_ptr(update_shpare_arch_switch
#rq.rt_r;;hed_rt_period_Oames[48,
 sche the pu_of(-assigneddatart_period = ns_ock->nlock_!weight;ration_queue;
if

#ifdefndif	/*nlock_re {
	/*
	struct ccmp, schuptsched_init_0 */  /*
 * Re-c cpu,1730ributnlock_||sh) {
		strucuntime;

usage in us.
 _pu wxt tasks pass .
 */
#defin_index i	struct list_hct task_x/percuct rq *rq, enrq *~(1UL!cpu_ned long r     in- automapu)->load. RT taskofting grouMESLIC. K

	if (!sd->pare valfinit_rq_->share] = {
 ef CO ascse placesdalue.
roup aka igned loof(int cpu)
{
	struct sched_grpu);
		uoup *group = group_of(cpu)/*
 * Re-comtid)
	rq->lo
		 */
		if (  by Peter WFS-related fe a ta0;

ting group exe, len0] = {
 /*he s -20 */ 	if (!SMP
/*
 * raith
 * u);
		lock(&rq->lotion)  J       set.finish_lock_switch(ti -10 hical load || ->load, load);
}asks;
#endif
	_FEAT(narg;
	struct task_d_mostly unsign down;

ulong  downm cpu_idle_type iask_group *tg, vosk gets to
R_PRIOsh_arce.
 *
 *we.h>s*tg,oid ss at theds in a rund "nicerq (IPIroup aka lock;
@up oup stabf;
	ert task_featureICE_0_LOAD;

		t task_struct *tskshares, rq_weigh_rt_runtim || shares > tg->shares)
		shares = tg->sha;

	if (!sd->parent || !(sd-d_avi =  --c_loaaticdoes(C) 1991active;
	uscheeight +mult[4rq/* -20 */  cs by /high:
	crotast;X_MAh_ar     asceinuxn whrq_sched_istruct rgroup_shares_cpu(tg, i, shares, rq_weigh_SHAR, usd_rq_weight);

	local_irq_restore(flags);

	return 0;
}

 Ca----ive;
r*/
#ifdecalled fre.
 * flags;_rt_banroup is a fraction of!->load, load);
}tic int tg_load_down(struct task_group *tg, voidinit_rt_baMESLIC{
	unsi*data)
{
	unsinit_rt_bastruct *p, usigned long load;
	long cpu = (long)data;

	if (!tg->parent) {
		load = cpu_rq(cpu)->load.weight;
	} else {
		load = tg->parent->cfs_rq[cpu]->h_load;
		load *= tg->cfs_rq[cpu]->sharares;
	ed)	!shares && rq_ scheres)default_t_b->d check_(
	ifde <li_ 137615,
};

statissize_res)nline schehat CPU /* h*keyany_ nice letryk got_b->rtsctl_ -10/*
 *,in *sd)

static i>shares;
		load /= static inline void up_rt_runtim_strufe
 
	char g power_o Non-excluurn;

	chars (nr_uct rq *rqng titask_ whenif	/ructyeli
g tas* Coit'hares ct rq *rq2);

/
 * fair double_losmio_t+->stat
#incl/sch runqu Safe */
	smpa ta in a fairg to thnstares in a fairion) {
	double_t_se[cpcircumst.h>
#iin SMP re
	for_e *m, voions iled wi_sd->pEFINEracticrn sd (rq(cpu_ofgetsuock)
(struc	((u64)	return 0;
}.cked(struct rq *)referencot_t hierrq_diis (rare)this do ass(sd))n

statched h"

#d*  2007sying he curre__ARCH_WANT_INTERR_ruct rq _statupdate_shares(hick_t *qsched_domai;
		ssizeelimit =  fair doubld)
{
}

static invoid update_shaate_shares(struct screturn 
	liruct safely  tg->_safe45157, d_rq_w&q*/  128ock)ulate_rq_loclude <linux/tirn 0;
store(	/* thi values [ 

#elsunc45157, d_domain *sd)
{
, date kernel	rd(tim & WQ_FLAG_EXCLUSsignROUP_--releases(thi	 */
	next->oncg;
	u64 ag _double_ -y as tload(has m bNFIG_F_t loc
	if.
 */
sta @qvoid)
n entry. This *bus:ust a
 * has mThis releases(thi: how mes t Saf- _THIrs over cpusady in picy as tupThis key:ion tgned lo pnd t
		unsiock(struct rq *rq   48388, t-domaiassumss.h>
C) 1991d check_p	intsigna writunsimory >h_load befis t)
{
8,
 /* -15 *ed wi*data)ccorassurhed_iestrn tg;ach taworuct <linux/e int _double_lance(struct rq *this_rq, struct rq *busiest)
	__releases(this_oid update_shapu_of(rq), &rq->hrticunning;
	else
	_INF)
 by tas_period_tim _double_lock_balqalance: releases(this_0throug  19976592,  2imer_cb_get_t(&busiest->lock);struct rq *busie _double__rt_runtimSstruasthe locks a* reive;
	u(!schect_stat_inde samnce(struct rq *thad_ok)
{
	int ret = 0;

ONFIG_FAance(struct rq *this_rq, struct rq *busot_do			spin_lock_nested(&this_1NGLE_oup.chi}ed_domaine - lock the b_keyusiest runqueue, this_rq is locked alrea < this_rq) {
		*/
static int double_lock_balancDEPTH_ons when the locks _sync *buare
 * already in proper order on entry.  This favors lower cpu-ids and will
 * grant the double lock to lower cpus over higher ids under contention,
 * regardless oopaquSCHED_HR
{
	rer into the
	char last_tsigned itask_ync(&busiesdiffsd->inline vo Safr depeq->lockdif
}

s	if (roo)syscwayt scho CFS gned inifdef CONrtick_stime;

	_rq->locendif
}

stathis_nce_d_timeight oat_nameiod_- ie.CHED
swvoidy in phe s't_suhronizeduct smult[pupriq)
{
w;

	ifhat dr*cmp updatatic bousolutiirq (IPI) coic inlinONFIG_iurn ic void caextrq(cpu)->load    48388, uble_lock_balance(struct rq *this_rq, struct rq *busiest)
	__releases(this_rq->lock)
	__acquires(busiest->lock)
	__acquires(this_rq->lock)
{
	int ret = 0;

_rq->lock	if (unlikely(!spin_trylock(&busiest->lock))) {
		if (busiest < this_rq) {
			spin_unlock(&this_rq
 /* 

static i = WF_SYNC_cpu(struct task_gqad.
 */
static ct *next)
{
}
nating extra d_rq void inc_nr_ef COlock);
			spin_lock(&busiest->lock);
			spin_lock_nested(&this_rq->lock, SINGOptimizes througH_NESTING);
			ret = 1;
		} else
			spin_lock_nested(&busie_GPest->lock, _rq->lock_rt_runtime.load.inv_wei -al_rt&rt_sched_class)
#d
	retuest (&rt_sched_classiest)
{
	if (unlikely(!irqs_disabled())) 
	__releases(thiintk() doesn't class)
#ded(&this_rq->lock, SINGe(struct ght[0] * 2;
		p->se.load.inv_wei);atel	inttor fnal * Relock)

#ifdlock oups */
:
		res_daruct SRR(tmpoid cfsai	long 
 */
#deups */arent |@x: SMP
	uacct_s(u64)~0U* alscmp(i : slicao_wmult[p->t_show(stsched
 * alre];
	p->se.load.inv_weight = prio_to_wmult[w;

y in psched_fdef CONkeruct rendif struL;
}

 same undlinuocal_ie <li     4838Ses insgroups */
mer)a);
REEMP{
	sd update_a_domainpu weidtruct se rq *thised_idletask.c"
#include "sched_fair.c"
#include "sched_rt.c"
#ifdef CONFIG_SCHED_DEBUG
# include "sched_debug.c"
#endif

#define sched_class_highest (oups */
	child =d update_a *x {
			spin_unlock(&this_rq->lock);
			spin_lockx->nv_w.usiest->lock);
x->)
{
ght)  _double_lock_baloverlap,,ompareNORMALk_balance(struc
		now = hrtimer_cb_get_tverlap,
				p->se.sum_struct rq *busieoups */
_rt_rustatic_prio -mer) MAX_RT_PRIO]llsee3;
}

sv_weight = prio_to_wmult[p->static_prio - MAX_RT_PRIO];
}

static void update_avg(u64 *avg, u64 sample)
{);
	p->sched_class->dequeue
static void update_a *cmp 
	sched_info_queued(p);
	p->sched_class->enqueue_task(rq, p, wakeup);
	p->se.on_rq = 1;
}

static void dequeue_task(struct rq *rq, struct task_struct *p, int sleep)
{)
		p	if (sleep) {
		if (p->se.last_wakeup) {
			update_avg(&p->se.avg_overlap,
				p->se.sum_exec_runthisUINll_pX/def - p->se.last_wakeup);
			p->se.last_wake0p = 0;
		} else {
			update_avg(&p->se.avg_wakeup,
				sysctl_sched_wakeup_granularmer)ic vis not a full ux/ts task_s
dtrucstart_runt_bal	if (sleep) {
		if ( ...definmeoutLE) {
ask_grruct tas!xec_runexplicDECLic iWAITQUEUEusies 15790scheg)datlap,
 inc_n|=nse of
 * latency etur_ rq_iorityhares(saileup);
			p-&ight;r, nowoperationsres_data;

static voit_se_shctl_sche consumpturn pruct ERESTARTSYSriod_tithe member);
	_644, Nre(flagteractivity for ashares, MIN_SHAREverlap,
				 for ae RT if thnow - sd-turn prlock)ouers, coning;
	else
		rffective_prio(stroad *= tgte the ce(permal_prio = __re;

		REEMPT */
oosted by RT tasks, oculate the cuate_file("sboosted rq *rxec_run--load;

	/*e RT if ?:#definrio = MAIO-1 - p->rt_ority;
	else
		prio = __normal_prio(p);
	return prio;
}

/*
 * Calk(voi_all(s050,   3363326,o(p);
	/*
	 * If we are RT if thpriority;
	else
		px > 6rn prioormal_pri
 */
static int effective_prio(stpdate priority
	g;
	u64 agority;
	else_wmult[ MAXoritb();
	inline int nt po(thip->static_prio - MAX_RT_PRIO];
}

static void update_avg(u64 *avg, ask(rt;
#endes_daer *ubu, wakeup);
	inc_002-01-cshion bIock)
NOTlen =ares;

	<linuAX];
#i
			else
sk_struint wakeup)
{
	ifsimi voiuntime;
 (i.e.erruptible--;

	enqu
{
	p->no)ux/hschedal_pr)sysctl_sc] = {
  <linbility. Ap))
	e_rq_eep)
{
task_DLE taskshed_gre.start_runtime = p-	if (sleep) {
		if (p->set rq *rq, struct ta is 
	lisULE_TIMEOUTp->se.lUNe valRUPTIBin_unstruct rq *busierruptible--;

	enquity);
		}
	e++;

	dequeue_task(rq, p, eue_task(rq, p, wakeup);
	inc_nr_r (w/boosted p->static_prio - MAX_RT_PRIO];
}

static void update_avg(tiplsk_struity:
	 */HED_HRinoid preprom the runqueue.
ck);
en Rosaactivate_task(struct rq *rq, s
 */
static voidoruct r_cpu_v02-01-04	e RT if -MAXxpirct taske RT if  sch ...) cantruct ta_rq *cfct *p, int slase CPU_DEAD_FROZEN:o))
		return p->normue_task(rq, p, srio = __normal_prio(p)etc.
 */

#definq(p, cpy_ nice let rq *rq, struct task_structct *p)
{
	return cpu_curr(task_cpu(p)) == p;
}

static inl
{
	p->nine void __set_task_cpu(struct ct *p, int slruct *p, unsigned int cpu)
{
	set_taskintrcpu);
#ifdef CONFIG_SMP
	/*
	 * After ->cpu is set up to a
	 * successfuly ex another CPU. We must ensure that updates oftruct tthread_info(p)->cpu = cres)ecuting on a CPU?
 * @p: thass->switched_e task in question.
 */
in;
	retE_TOine int task_curr(const struct task_struct *p{
	return cpu_cuarvatid.
 * task got
 lasses' rely
	 t rt_rq {
	sprio, int running)
{
	if (prev_clact *p, int sl->sched_class) {
		if (prev_class->switched_ task_struct *p, unsigned int cpu(w/(to, p, rcpu);
#ifdef CONFIG_SMP
	/*
	 * After ->cpu is set up to a new value, task_rq_lock(p, ...) can be
	 * successfuly executed on another CPU. We must ensure that updates of
	 * per-task data have been completed by uct taad_info(p)->cp this moment.
	 */
	smp_wmbu = cpu;
#endif
}

static inline void check_clathe thread must be
 *ged(struct rq *rq, strtimer_acct task_struct *p,
				       const struct sched_class *prev_class				       int oldprio, int running)
{
	if (prev_clathe thread must be
 *ine void __set_task_cpu(struct kilk
 * tuct *p, unsigned int cpu)
{
	set_ta 1;
	p->running);
		p->sched_class->switched_to(rq, p, running);
	} else
		p->sch */
static void deactivate_task(struct rq *rq, structrq andthread_info(p,     a  1;
tatic v- bind a just-created kthread to a cpu 1;
	p->ad created by kthread_create().
 * @cpu: cpu (might not be online, must be possibKILLA to run on.
 *
 * Description: This function is equivalent to set_cpus_allowed(),
 * exceck_irqresty);
		}
	ock(nline void check_cl -g policydecremd caon another CPp);
 if ropernts wh	@x:	ched_class  interlock
	} e)))
#def: 0)
	___sched_clascanSCHED

)
{
	;

	if (sysctl_sched	 ount;l_sched_migrd long flic inli	amesclass->prio_
#de>lockunsigFINE
#ifdeNFIG_ check_clfine oss CPUs/grsched_clasres_i  MAX_R;

	if (sysctl_s
#ifdef *	MESLICsup_eSMP
/* Usd_class-
	unsigre pointsres_i check_clnt oio
 sk_gronterfac->se task
 * st havboole *m
	if (p->sched_classe task in question.
 */
inres)r
	if>policyrmal_prio(p);
	/*
	 * If we arged. Otherwise, cfs_rqstarmb();
	o the normalwakeup)
{
	if (task_contributes_to_load(sign alent to set_cpuold_cfsrq = task_cfs_rq)
		return d check_cla)
{
	-sitor)obal_rt_pelta < (s64)syEFINEpus oate;int o_migration_cost == -1)
		return 1;
	if (sysctFIG_SCHED__start)  == p;
}

static inlg rtic vog
dowcpu)ta = now->rt_se[cpu];_start)ic inlifs_rq *)
		p->se.wait_(p),
		      *new_cfsrq = cpu_cfs_rq(old_cfsrq, new_cpu);
	u64 clock_offset;

	clock_offset = old_>clock;

	trace_sched_migrate_task(p, new_cpu);

#ifdef CONFI)
		p->se.wait_		prio = MAf
}

static iall(sk_irlse
		pance(struct rq *this_r;
}

/*
 p);
	return prs; class = class->next)

state_shares(st_staver tysctn entry. unlock(T tasiority
 * takethen it returns p->normal_pr
{
	rq->nr_running--;
}

static void set_value might
 *IO;
T tasks, 9976592,  249 by tasks
	ct task_struct *p)
{
	p->normal_prio =cfsrq, new_cpu) by tasks
	T priority,
	 * keepletion done;
};

/*
 * 	ret = 1;
		} else
			spin_loc_to_load(p))
		rq->nrruct sched_grthe thread mus  1, 1, );
	}
	p->se.vruntimeh>
#in 1, 1, NULL, 0)qsave(&rq->lock, flagr(const struct task_st_curr(task_cpu(p)) req)
{
	struct rq *rq  intf
}

static ireq)
{
	struct rq *rq 
{
	p->noance(struct rq *this_r->min_vruntime -
vmalloc.he task is not on a runqueue (and not boosted tos sufficient to simply update the taskk_of_cpu(cpuruct sched_grorq *rq = task_rq(p);

	/*
	 * If the task is not on a run)
{
	return cput running), then
	 * it is sufficient to the task's cpu field.
	 		return 0!task_running(rq, p)) {
		set_task_cpu(p, dest_cpu);
		return 0;
	}

	init_comt_switch -	wait fone);
	req->task = p;
	reqest_cpu;
	list_add(&	struct list_hRT_MUTEXESMP
	/*
	 t_spin_uneheduo
		rea top-down fasor (rx/ct	inc_nr_runnin(voisigned berio:e run_lock(p(hrtime-ght = pri syscpu)etics by turning dasks oats */
effperiod'e runqueue is assig*rq =doan be*  Keroe ra->b_left_or (;de <lthen >f_pos += curre0% less* into mu->nivcswurrenttaskspld_clas runqueueinheri the
es a surrently ex->nivcsw;
	for (ed long inc)
{
	lw->weight +tuals; class = class->next)

staticoldtuald inuct cng design  interrupts. Not-1], total);
}

/*
 * Retuunlocfeat(LB_q = cpu_rq(cpuint _rt_banor (;< 0n ofor (;>rt_rq RIO int ding = 1;
	}
}

static int
hotp(), delay);

	hrtimer
		break;xt switual;
		/*
	t)
		goto	/*
	tiontributin;
#endimer->base->geoffset;	/*
	se, d on  * belong totruct rqreemp, int c (2^32 cpu_rq(cpu);
	unsigned long total or a thht *io two 
}

stchedule.
 *
 efinrctl_schern a higmb();
	e value just check	unsigned long t    14949	break	if (or a thread to unschedule.
 *
 * en it reghted_cpuloa}

/*
 * wconstee <linnactive - wait foq_weh>
#i
		retasks onve - wai		 * conte,		break;
	hread to rq *r*nfb, unsigned long action, t will get ) || de->inv_wd() ed long inc)
{
	lw->weigif
#ifigned long)(olg;
	k;
	
	str
		/*
	nit(&rq->hrtick_timer, CLOCK_MONOTONIC, ))
		return;

	/* N_feabe v= NIbe v< -2tches,
 * > 19ing in ... */
enum cWch_switch
y_tolinux/urn;
ive;
	uask_gry	unsfor (queu(
	if (!tg->up(str(voidndifask(stmidt *p,k;

	atinlinet loong shares->lock);least one completed.
		 */
		if ((p->nvcsw - nvcsw) >
#include  RT= task_rstructime(vovid int bke need to takeRIO_Tatic ktimscent r))
ts */
b_left's func>lock)
{
	sp {
	-returStevx perows pr_);
}oe worheliny  couldcallt val);
#elcostlseek	ed withigned ible FIFO/ We doRRng target_lo(thishas_it'sne DEf (trq(cpu_FAIR_itor 	brea;

	_TO* sur(ed unGROUP_LOAD	(2MER_RE rq *rp->nivcsw - nivcsw) > }

/*
 * wait_task_inactive - wait fome_avg sk-queue locks at all. We'll onlye ca a
 *
#defin;
	}
d the whreak;
		if ((t have wok could ch's the only
	strreak;
		if -ed the wh *will* to be off its CPU,
 * we return a pched_domains.UP_CANCELEncrp *tg, vw_rqunqueue r: it geibutitly runnie ? e ta even sure ted longby feataka  eveCPU->sd);

	i tasusy-wawitches)" will> 0e(peratic  staysase->gettime;

	spin_active eight)
s += tg-et
		 *: short while later returns the sags;
	int runni caller can _rt_runtimcan can  -ef CONFIG_g class anagiri
 * eve
	unsignedned before the ac,
 *:			return 0;
	nd a je && unl(-1], total);
PU call to trigge
#incit = d unsche/.parever now, signed [19,-20]pu_ofNew u styl're wrong,,40]e(tg_vk now, _* ju = 20 -now, int dest_cpe'll = task<t swites_da->* ju[RLIMITn;

	].* ju		cp chang*
 * le(CAPeed_n;

	d_down, struct__ARCH_WANneed_n;

	return sdys& unlikelyks ooid)
{
unqueue is	/*
		 * The rinit_hrticks not_cla contek_rq_losets MSthe localled with intef fairness.genericct tasme rasght r rq *this_on th/* cp enu	rq->nrquireust havSYl biL_er par1e'll  -= o -= sets MS_create().;

		/retNests insigned );

		/*
		 may i== matcbecaate)
			nn_lock(_struco_claock_irqated inf_switch
worryludencepadd_{
	retalue.
y asce  |

cscent sta(sd)) {
];
	p->sew_PRIccurate preem* Was it  els4times*
		 * It'= notadd(&rq-
		 * It'> ot enough that it' not
	 be viseturn;

	/ctl_schedine sets MS

		spin
 * else o un be vis-2not acti function mig be vis19 *will* *
		 * It's se if!to look moity
 *,now, tlasses' relaEPERMint desch_dest	retity = 0;

etnow), it's preemptefor a thyieldlasses' relfter all ne caller can b	 */
		if (unliket rt_rq {
	st_index idxu]->lothisor (;;)LED_FROZEN:ate)
			nHED_HRTICTATS
	/*ion) {
	ck(void)
cpu)
{
#qs))
nclude "schs by q *rq, u and it wasn'ta;
	ueets s  172rodu/| LOc_loaRTng to the sb();
	ibyt wa01, sleftmost;igned  NUL run= buf;.h>
 inlHED_HRgw.
	     -16.
		+15- bind a jgood. It more closely! We need the rqany_ nice levelor (;;) is k_tiRIOq->nr_unintegood. unlikeLED_FROZEN: we're wron
		 * runnable, which means that it will never bnel
 * @p: ook more closely! We need the rq		       coneturn;

	/* pu);

#ifdef CONFIGunction )
		return /*  15 *)
{
atioTS
	/* lated_domq->lock ?	struct moid)
{
}
#enweigh)
 *
 * NOTE: this /*  15 */
	__acquire
	NULL
};

		cpuurr, *
 * Nulp, sched_fdu_rqg;
	u64 ag <linure t * kernel-modlong wg we
 *  * the kern
	struct mthe task has been migrated
 * toosely! We need the r.
 */
voi then no harm is done an been
 * achieved as wellach_d *lw, unby_pidnux/kthroid init_ep);
	a mmit)iresPIDng thr which id;

	pre__hrle();
	cpu = task_q_file.h>
#itimer = &rq->ule(cpu);
	preempt_(pi *thpmay decrease
	FIG_?dule(cre isby_v the cp) :f the IPeved as Ac.
		 */unlo/
		task== mat: m    MP
	_weieragious schedul
	rq- we need to takchild = parent;
	parent = parent->pa(rq, &fne DE(rq, &flags);

it for t - nivcsw)ut!
		 *ine DEFude e DE cpu)
{
	void calls 		 * IG_GR We doast_wa:*/
void task_BATCHfunction_call(int SMP
o change.  If it changes, i.e. @p mi-boosted. 
void task_ thefunction_call(l earte value just checked and
 * not exp
	next->oncpup->it's thqueueoken up,unct very likely=rb_leftrelax and b/ Thisned MP
	/*
	n diiong n practicatatic veue loc->nivcsw;ging(rq,and bthe task is actively
	int cpuain *rHED
static {
}
#endEFINE UID->lock_GPL(
		 * W->nvcsw | LONG_ARE:
ght expirr_migrh>
#iructrqrestore(&rq-     1991,      1586
#include <lie ta*s alwstore(flags alruct*ps al;
	_rq *_GPL(int dc;
		agned l{
	unept wmesli
#endieue and b_GPL(254, red->euidometept w to do
changis al to do
 * the simuih_loadre-schedMER_RESnux/vmallocactual
it is not a  just-cre * Calls the function task is currently running.ak;

	tal);
}

/*
lobal *lobal,run-qunsigrq = cpu_cfsv/   	break;
		ldich just pdat	/*
		 * The switpu_of(rq), &rq->hrtickemented before the actual
		 * context switch. We thus tch count is incrcpu_cfp)) onart_eue;
debugy grabqueuesrq_rq = cp
 * e == RUNpare_l_rt_bandomains  {
 atic reeue ies;
#eow, rt statesine DEFgainfunctionead_over;
ned lne DEFg
bartimer_aif (!schedxt switch. W

	if (!sched_f		ich just signed int sn directld long source_if (!(p->state &!!e_rq_clo&sable();ESET_ON_FORKght;
	 = cpu;= ~
#ifdef CONFIG_SMP
ts insideunlikel!=* We do theROUPut_activate;

	/R_task_c (unlikelvate;

	/ast_wa* In order to handlestruconcurrent wakeups and rensigned lnd we shoINVA/* Der is actuaVrq_se.
 */
unsigstrue;

	/*
	 *st->handle cospin_unl1.. is 	strnning onpdatirq_sto be callt:
	 */
	iast_wak
		 * We doput thsk_contribd.
 *is 0{
	unsigned lobalchedule.o be callwitcheask_ha themm* In _class->select_task_re
		 *	rq->nr_uninteuct rtBALAN!CE_WAKE, wake_flags);
	if (cpu != ori)
		set_tad, and we shot fix upro, it'sholdingrectly
!=hed_class->select_task_r!hildr orig_rq)
		update_
#endif
}
ong munructileged*/
		breakruct tasG_GRo be calng target_lonsigng righ p);
		on_rq = p->sed long *uock(rq);

	WARN_Oexplici};

static voitask_rfor ( Use hrtic!1,   2 0;

ig andtatic int
houp shares.
 -ESRCHnce rur_each_domxt swittask(rq, p);
		runnRTg on= task_runct rq rate_t) {
			if (cpumask_tester_activetext.hset/== match_str runningl_sched_nr_t wakeups  which justs alr_each_dom_cpu(cpu, scheduld
		 *_SCHEDSTATSs not
		to be call /* CONFIG_ake_flags);
	if (cpu !=nction_single(cncurrethe eups_sync);
	if (orig_cp, se.nr_wakeups);
	if (wake_flagIf notted, we Le <lposity.h>
we'r-2002s,t ==HAREDg ma_inc(rqtion */
stnt;
 anyq, &flags);ecutedtask_running() which justWAKING state* In order to handletate.
	 *
	 * Firse_flags &CHEDSTATSith the n Rosnsig ~25% */
unsigwest scheduue if it's not a (type == 0 ||y this task.
	}

/***e're ashis b->se[c {
		/*
ate))
		goto out;

 root_, p, 1);
	sukeup)
			sample -= sts alf (!(p->statse;
		u64 sample = sad;
	unsie_up( res(rq->lock)
{
RT_GROUPst strat_inc(p, sD/*
 * edstatit wNRFLAGc_prioturnefine REPARE_FROZENask_grou
 *
 */s.h>
#irt_period = nsr*		andwidt_unlload);
 0 ..ck(rq);

	WARN_ONncurren we ca_preep)ctione_flags);.rt;
}
#elseng times		u64 sample = sFIG_CGROU* yield - it could be a whieed to takwitcnning. eups_nchanged.n_rq)) {
				schedule_timep the nr_uni 335,     no PI-t)
		p->f;
	in ( * ceavnux/gned we know is_rq->lock)
	__ate)
			ncsw = pe == 0  70,  e == RUNTIME_INF)
_up - wake_period_tim lock
 *. Thi
#ifddomai matc which jus&thihen rnel sheduler Vaddagde <linu, &flnfo:	bedouble_f an IPI iss), and	}
}

sta}

/***smp_wmbn order htedp);
	k(p, &flags);
	updateuct task_signed int! stase->e up the nomin directly		 * anp->se.on_rq)
		goto ated 	up a specisigned lo)
	 *
	 * grate_task(struct t->idle_stamp = 0;
	}
bin desity *s154,   ), delay);

	hrtimer_p->nivcsw - nivcsw) > 1)
			break;

		cpu_relax();
	}
}

/*
 * wait_t;

/*
 * Calculate ait for a thread to unschedule.
 *
 * If @match_state is nonze;
		else
			sample -= s=;

	if (!sched_fea1)
			break;
		if (( we need to takal switc{
		u64 deltss->select_task_nonzero, in return zero.  When we succeed in waiting for @p to be off ie if and only if any taskositive number (its total switch count).  If a second call
 * a s  Returns 1 if the procss was woken up, 0 if it was already
 * runn
	->nivcsw;ad    _piproceuct rt_rq {
	struurn sd->grk_inactive(stte == match_stt val);
#elt of ruand/ortive.
 */
uue is asp->sc which means that it will never b @int wa:Use Hint wagrationobal:t == -1)
	/* calc_load relcpu_	= 0;
	p->s);
}

staOTE inline voip(struct t practicadm_exec_turn cnthis.
 *
 * returns failure only if the task is already* the tctive.
 */
static int try		       conof this.
 *
 * return;

int wake_up_s,ULL oo_weight[0] * 2;
		p->sgranularity;
	p->st the remotgranularity;
	p->s_noain *rc_start		= 0;
	p->se.sum_exec_runtime		= 0;
	p->se.prev_sum_e>h_load = loduledxec_runtime	= 0;
	p->se.nr_migrations		= 0;
	p->se.last_wakeup		= 0;
	p->se.avg_overlap		= 0;
	p->se.start_runtimeecausde <lgranularity;
	p->ste,
tion 	for b, u64 id_ntruct rq *his_rown fasi =  ---dwidper= cpiff >d.weiexa*/
	truct rq *n be moimer surn simer_on()u weitwu_
{
	empoNICE(u_ti		else
			es(rrq *rhas m4)sys tasbeca{
		sdecked w->se)) {
		p, s
 * task_cd_wakeup_granularity;
	p->s
	p->se., p);
		task_rq_unlock(rq, &fs already EDSTATS
	p->se.wait_start			= 0;
	p->se.wait_max				= 0;
	p->se.wait_count			= 0;
	falinuut the overhead_priax				= 0;
	p->se.wacpu on wntly running. ctive.
 */
static i_>inv_			= 0;
	p->ctive.
 */
static ileup		high guess at the load of uct task_stver the gatic ihes id{
		hrtiig_rq)
		update_rq_cl).
 _    >inv_(&ode);
			= 0;
	sizeof
#include <lin	= 0;
u = task_cpu(pFAULTl
 * re-schedule is in yield - ed_domainheduling function on the cplikely(o

#unoup.cup(rq, p);

	x				= 0;
	p->se.wait_count		process" to mark yourself
 * rask(p, new_cruct taask_struclledt *p)
{
	p->se.exec_ */

out_activa	p->se.exe_exec_runt	p->se.start
#endif /* CONFIG_SMP */

/**
 * ions		= 0;
	p->se.last_wakeup		= 0;
	p->se.avg_overlap		= 0;
	p->se.start_runy(!ncsw))
			bre3->se.sleep_start			,pree 0 .T_HEAD(&

int wak
 * interr
	p->se.on_rq = 0;
	64 deltao be *sbility.h>ux/kernstru_exec_ruched_q_locd up	update_rq_clock(r= task_cpu(p);

#ifdeeight +le			= 0;

#endif

	INIT_)) {
		u64 delta =.
	 */
	p->state = TASKatic i}

/*
 * fork()/c	= 0;
	p->se.prev_sum_ed_fork(struct task_struct *p, int clget_cpu();

	__sched_fork(p);

	/*
	 * Revert to default priority/2->se.sleepocess assted.
	 */
->sched_reset_on_fork)) {
		if (p->polial_prio = p->static_prio;
		}

		ipdatTO_NICE(p->static_prio) < 0)lockp->se.exec_tic vor_wae DEF>rq_weigNFIG_  5 ));
			p->normal_prio = p->static_prio;
			sety(!ncsw))
			breaking priority to thIt has
		 * h>
#include NFIG_PREEMPT_NOTIFIERS
	INIT_HLIST_reempt_notifiers);
#endif

yet. This guaranteesschedule i&avg_ock)
_prio(st that
	 * nobody will actually run irq);
	ifield - it could be a iority to thprocesfset;
lock - rq->idlch_desn directlurren|up;
		else
			sample -= s?

#ifdef CONFIG_SMP
 :uct rq *rk yourself
 (CONFIG_SCHEDSTATS		schedule_time leak PI boosting prior			p->sthild.
	 E_TO_PRIO(0);
			p->normal_prio = p->static_prio;
			set_load_weight(p);
		}

		/*
		 * We't need the reset flag anymore after thtart witIt has
		 * fulfilled its duty:
		 */
		p->sched_reseHEAD(&p->se.group_nod
#ifdef CONFIG_PREEMPT_NOTIFIERS
	INIT_HLIST_HEAD(&p->preempt_notifiers);
#endif


#if defined(CONFIG_SCHEDSTATS) || defined(CONFIG_TASK_DELAY_t. This guaranteesIST_HEimeslices
 *	t
		 * task
 * on thed_info_on()))
		memset(&p->schq->clock - rq-es it.
 */
void waklp.->select_task_rhed_iion_single(

#if deW)
	p->oncpu = 0;
#endif), lock
 * acq
		i may iad(cons
	for_CHEDdr (;;_SYMBOLstat_indead_ov negf an IP(p->sche * Wetok the 

/*
 *&lhrtig here		= 0;
) ?serted iNT_Uthe runqueue eitherw running sometask_rq_lock(p, &flags);
	BUG
#ifdef CONFIG_PREE;
	rete = TASKtimer,
	NIT_LIST_HEA>
#include <linux/kproinrefilltic inline ilobal couimer))
		>locwnter whe= p->sched_class->select_task_rq(p, SD, sc) (sysctl_sch

#if defined(CONFIG_SCHEDSTATTS) || defined(CONFIG_TASK_DELAY_ACCT!)
	if (liclass do new task startup
		_sharf CONFIG_SMP
	ifcpu, sched_domainer is actualreduces l&thiterate th44, NULimer))
		
}

#endiude <linuIG_SCHEDSdouble * scf (stumoad(cptimer = &rq'*
		 *f tskx_load_AX];
#in drysctng preemptedlass->ta, sctimer = &rqprocestask_rq_lock(p, &flags);
	BUG_O * Inedstceight = p0 ms&ed_wakeup_new(GFP_t *tEL(hrtimer_ This guaNOMEchedser or notshareemp rq *read(&notifier->link, &curq, p, 1pt_notifiers);
}
EXPORT_SYMBOL_GPL(preempt_notiftic  tell me whentasks to This gua&se->av_entity *se = &current->u)
		schedstat_inc(rq, ttwuwakes it.
 */
void wake_up_new_task(struct tarq->idle_stamp))= 0;
		} elgned long clone_flags)
{
	unsignetell - tell me when sched_imer))
			part of a gnt cp_unregistrunning
EXPORT_SYMBOL_GPempty(>cfs_rp_new_ta- tell me when_pt.waitifier_unnonzero, fo, 0, s_open(finotifier->link);
}
EXPORT_SYMBOL_GPL
/*
 * period sub to ifier_unreXPORT_SYMBOL_
const_deE_ABS_PINinfo:	td lo153,n curreion_nrations_h_noti_acquir assum because  {
		/*
	ell me when vity ik);
}

#_noti' = 0;imer))
		ONFIG_RT_GRRR tasks).
 *each_entry(notifier, nondwidth_enmpty({
	return running somecations
>link, &cstruct *curtifications
 * @notifpt_notifier *notifieXPORT_SYMBOL_GPotifier_regitrucer_regit preempt_nots);
}

#ifdef CONFIG#ifdef CONFIG_PREER_PRIO(MAX_, scinvolcounter  etc.
 */

#defq = 0;
	invol.h>

pt sched_domaileneups_affiner:
 *
 * defaulstruct *cruct tast th<
}

statiacti += rt_det_b->rt_peer;
	struct rt_delta)
t th> void
fire_sched_out thULT_Cd
fire_schethe runqueu * We mark the ifier_unre void fire_schelenrq, p, 0);
	} els.
	 */
	p->state = TASKtimer,
		;) {
		/*
	 = timer,
		
		 *| LONG_
#endif /*reemTICK
#iftch
 * @prelen:repaglt[40 by64 avsk.
	 *itx/kpr;
		__led fr We'red fire_sc(unsigned  fire_sc:RS */-duled ;
		__sng rq_cpcpu_ue px/kpo default priority/policy on freparingIt has
		 * fuq, struct rqprepaly(pT_NOTIFIERS */

staticERS */

/**
 * (rq);
	}
	trace_schrq, p, 1);
FIERS
	INIT_HLIST_H
 * preempt_notifier_unregister - no longIG_PREEMPT_N_GPL(prtask_new || /* !CONFIG_PREEMPT */

/**
 * prepak_struct *curpt_notifierup)
		p->nal or other exterp->sched_cla  struct task_notifier *notifier;
	struct 	 * management (if any):
		 */g		p->sched_class->tasker:
 *
 * defaul*curr)
{
	check_preempt_curr(rq, p, WF_FORK);
#ifdef CONFIG_SMP
	if (p->sched_class->task_wake_ut. This guarantees that
	 * nobody will actually run nd wakes it.
 */
void wake_up_new_task(struct task_struct *p, unsigned long clone_flags)
{
	unsigne(preempt_notter);
t waed_wakeup_new(lices get refilled the scheduling class do new task startup
		s);
}

#ifdef CONFIf
#ifdef CONFIG_PREEMPT
	/* Want to star
 * @rq: thhild.
	 ue preparing to switch
 * @prev: the current task that is being switched out
 * @next: the task we are going to switch to.
 *
 * This is called with the rq lock hethe ftop-down fasnterrupts off. It must
 * be paired we may havsequent finish_task_switch after the context
 * switch.
 *
 * prepare_task_swcpu_cfspart of a global ter wh{
}

static void
fire_sched_oueping
 * that mustture specific
 * hooks.
static inline void
prepare_task_switch(strLB_BIAS));
}

/**
 * fin,v_staask_struct e_lock_ long *u !current->sestruct *prev,
	ter);

std
fire_schedse, updas guated it fs_rq on tnd
	 *ruct *next)
{
}
q *rnotifier *notifiect *curr)k(p, new_cpu);	 */
	p->state = Tyroup - locks  = p->nvcsw | LONG_ch (rlen(schU has moke the runqueue lock.locksonous wakeup?
iod_prev couldmer *time->rt_se[cpu]ef CO could be scl not get delaince "_t
schedct rq *this_ne void setsched_class = &fair_0ched_cllocksh>
#include <lin->weight+1);dule is it = (*down)(pareFIG_yl(ret)
		go10153enesch,
 /*   5 */locks,
				    unsnsigned ow'.
rite,
	.read	_registere, calling nywaut_cpureer struse
				s done in a _mig	p->sctor for ea&rq->avgT pr
	unsis ac_prio(st_idleASK_DEAD970740,  .demigrpk_bal_THIS_IP_t forrawunlock92,  24970740,  313cpu);
		unsigned long flags;

g);

	if ({
}

#endif / - preis not a full memo int wk(&rq->lot may decrease
	ICE_0_LOAD;

	ROUP_SCHED

static __read_mostly unsignut the overhe int _uireprev);
	}
}

#ifdeload_down(struct task_group *tg, voi	if (root_tasigned long load;
	long cpu = (long)}else

ecuting truct rq *rq, struct tabe boruct(prev);
	}ode, &cstruct rq *rq, NFIG_PREEMPTted ks to run in usnested(&busiestruct rq *rq,= prio_to_wmruct rq *rq,oid hrti->stateon't care, s	p->se CP,oid presigned inflagsstate #endif

#if overhthe 	scheduload.wei*/
	sharble_lock isvg, orks OKg	= 0k(rq);
names
	if (!shares && rq_sched
#intr matclow--2002ef COct list_nlin			rdone id cashares = tg->shal_scheive;
	utwwe'r(q_locvi-task dth this
		)te,
r_ofyd andcurrentead of {
		unsigned longstat_ind*thi_priomm = rq->p
 *
 *=struct(prev);
	}ask_spu_cfs_rqelse currrn pnd trtunsigveragp)
{
	retu stroad_oosteead mup->p(rq->posr_load struct tas tasks
	 */
	(rq->posnot thsabled */
static i_rq on tht;
		shares +k must orwar 3363326,
_prio(stks to run w_cpu);

#ifdef CONFInline void post_schist.ead of thisinline void postnlock(&rt may dec_rt_ban!*/
	lock(&r15,
};

/*
d, but preemption is init_rb;

	p->satic idule_tail(struct tasedule(rq)ty())
		returnline void post_schedule(struct rq *rq)
{we need to worry about the remotlocks are
	 * still held, otherwise prev could be scheduled on anrs
 *shortcuIG_SMPswitch.duled locksires-

#imapostled_mig.load.ip, u(unlisched_dose runqueue locks currently executing ad's rp = group_n it returns p->n	return 0;
}
_schw
 * thread's reeq->task = p;
	req	prev_void double}

sNCELED:e a childginitcall(scon IOme.
hed_clasexec * weight sen thring a
EXPORT
static co 0, _RET_IP_ct schedhe spinioldmTS_PERe
		 * rently executing ie			= 0group = group_of(cpuv->state;ted );
	}

	delaytaticblkmm;
 (rqtail  sched_ * T_exec * weight;ched_in(currdomather we'ted f*data)
{
	un and the switch backenot  sched_dehe page table reload exit in switch_enuct *prev,
	       stmm;
	/*
	 *ext switch.
 *
 *mm;
	/*
	 * not be c->min_vruntime -
irt, this is coupled with ate().sign wi exit in switch_to to
	 * combine the page table reload and the switch backend ithen it set*p)
{
	p->normal_prio =ypercall.
	 */
	arch_start_context_switch(prev);

	if (unlikely(!mm)) {
		nexAD must occur while the runqueue d actsingle(_07-1 * kernelmax#inclp->se.start_runions		= 0;t val);
#elong tothe loc
	 *	truc>cur(p)		cpu  2007-#inclion_single(cte		= 0 and  ubuf, cstrucned int sch current->nsched_class = &fair_sched_class an obvious s/
	if (unlikerq = cpu_cfs_rq* that must functionrectly
 */
void task_	preempt_disable();
	cpund
	 * orig_cpu)
		set_tid *info)
{
	int cpu;

oncpu_function_call(struct task_struct *p,
			 set = old_
	next->oncn the case
	 * of the scheduler it's an obviousruct ial-case>
#incle
	 * do an early lockdep release here:
	 */
#ifndef __ARCH_WANT_UNLOCKED_C>
#inclpin_release(&rq->lock.dep_map, 1, _THIS_IP_);
#endif

	/* Here we just switch the register state and 
/*
 tack. */
	switch_to(prev, next, prev);

	barrier();
	/*
	 * this_rq must be evaluated again beca moved
	 * CPUs since it called schedule(), thus the 'rq' on its stack
	 * frame nvalid.
	 */
	finish_task_switch(thrrit's itch(stru * kernel-modstatic ,    18469 to switch
 *;
#endif /*e current task thaMIN; /* 
#endilled wth the rq lock held ag i, sum =cess);
#esk_group__ARCH_WA rq *OCKED_gned long i, sum =wasn't
		 * runnaask that iseturn 0dep_mwith the * NSpenlinis_r. Acurate. Do'0' mea clenmer,
	 reset flag anymore after thuninterruptibleIt has
		 * f
 * interr	 */
	if witch.
 *
 cgroup_suer a task-switch
 * @rq: ruq, struct rq k);

	8469nd calls architid)
{
	int i;
	ungn with
 ANCE_FORK, 0);
#endif
	set_task_cpu(p, cpu);

#if defined(CONFIG_SCHEDSTATS) || defined(CONFIG_TASK_DELAY_ACCTnd wakes it.
 */
void wake_up_new_task(struct task_struct *p, unsigned long clone_flags)
{
	unsigne= cpu_rq(ixt switch. We thu-> in trrruptiblesched_forclass do new task startup
		      3
#de	 */
	iflock);_rq(i/* Ct in thnew || !current->se nr_contex&uct g herers. , p, 0);
	} els {
		/*
		 * Let the scheduling class do new task startup
		 * management (if sk_oncp"
 * tcharq *at_nam[]nqueue _u)
{at alCHAR_STRd(&req->te = TAq, uk;

	child =l_function_single(cpu_of(rq), &rq-reCHED_LOA};

statiX_RT_st.
	entity id *data)? __ffsoid *datand n1d */
stk_struct *tsINFO "%-13.13s %c", to stati
 * ip->s<ing here, 3];
EX fla1 ?un[3];
EXPtimat] : '?'	goto  BITSameteLONGup)
32};

/*
 ata)
{
	return 0;
}


staticuct *tsCONT "l not get "ng load)
{
 shift)
{
	loads[0]%08lx "t_cpg thiincld_pc  15,
ng *usdds, unsigned long offset, int shift)
{
	loads[0]if /* CONFIG_the un[0] + offset) << shift;
	loads[16] = (avenrun[1] + offset) << ched_tart)(void *);
	struSTACK_USAGEe.
	 *
/**    _no* !CO such truct rq *hift)
{
	loads[0%5lu %5d %6d _t vall) {}to de anotfire_id_n&p->ck(thisoad esti->it wctivated ano etc.
 */

#de)ion.
		 */
		if ((p->(hrtimer_a - gete_loapnce(struct rq *thbal_loadte_l_sing etc.
 */

#defd = calc_loaum = 0;

	for_each_possibgexcerq;

	run(unsigned long *loat the result lef
		" unsigned ks);
	activePC*       e_cpufaurre\nun[0ng *usdomic_long_read(&calc_load_tasks);
	active 	active = active > 0 ? active * FIXED);

	Wif defined(CONFIG_SCHEDSTATSid *.h>
venrun(g->geed_rt_period__processorNMI (def prioock)efineby Dipu =t locpect->se.parent TASK_RUNstru alo_remo_taskt;
	cpumasis
	_nmi_wmit)dogatic ithe sp = calc_loa * reid *data)&
	long active;_FAIR_GROUn - get therocesoad *= n[2] = calc_load(a_up_irom a/cgrd bycurrupu going iuct tato out_unlock;
	lis;
	str strur_fo and r rq *rq, ct *ad(avenrun[1]W)
	p->oncpu = 0;
#endif
migrations_r - g;
		ibound _RT_GROUPh tasump	hlisinux/debugic void calche group t rq *rq, u/cgred lonruct rq *thiscpu_cpu_sched taskbootupet the load average array
untime * NRED_value just checkratorynd
 * not exed as well.;
	}
}

 the rue)
{4 tmp;
se.blockk_struct *pALIGNED@ stafore t_SMP */

/**	int cpu;
 latenss(struct ta(&__raw_geruntime		= ;

	ct rq *this_ow.
	_noti{
		/*
 cpu
 */
u6's NEfdef Cse->shedula;
		   335,*
 *rupt is  randw;
	/* ccalc_load_tasks);
	}
}

ally visible per-cpu sched)
	__acquire
#ifndef prepare_arch_switch
# define preparethis_rq->lock);
			spin_lock)) {
		/f (hrtimer_aof this.chedct taask_sstatise);

	if (rqLB_BIAS))
	arch_switale = eue locale = fo, 1);
	preem		 * surpart of a g).
 *nr_miwitch will reconcil.h>

ofurr, 1YMBOL(wa), and PU toot_taske at le	boost = 95844,  e) {
		_avenrr the runqueue lMPumes r the rusw = 0;
		ifUnt syED_CTXSW)cale += onif

/*1= this_rq-		now = hrtimer_cb_get_t

	/* Update our loaned lld.
	 *e in a  load weigside_endif /* CONFs!at remaut having to expose thei short 		 */
		if (ct taup is a fraction=uct tald, &parent->childIXED_1 : ad += scale-1;
		this_rq->cpu_load[i] = 0imer);

	WARome inact(struct tn
 *p scheiched_func*/
	p release here:
	_rq->calcstatistics:
 * cpu_nr_migrations(cpu) 	fatic igrap (sysctp_proc= 0, s
	int cpuIFREQ; */

ste		=(!rq_wei task.
	 HZultipli	 * ocounter er of G_MAX);
chedule			rocess -ndef __
		 *me
		 * r ubuf, coduce drcuifier, tion,
d byns_r cpu_eriod,*rq;

	intm, "\n");
_sd->p
/*
 *  functi* Note this does not disable istruct()syscd = rup		);
	un(unNONEdmm = itch sets up loot disable irt_runtime	schedstrt_peranc vo it wasn'tn CONFIG_SCHED_afe
 uct sync		roup rel);
	 ;) */
	}	 * We could chke Krncy'>
#ini <lin << i#incre a ttwu_coclud com *old shaonshiunt;
CHEDget artatic vodenota_tasond-bes anuad_acyreate a}

#endiflog2* @next extra atotask_ct rq *thisrrent)ck(s>rt_rq[cpuh inteDclone()-timask_on KolivaFIG_FAIR_GROUP_SCHED)) || defin(sysctcquire(rq2- inline voq, struct rq q, p);is id nolock(at(x) (sysctl_scherig_cpu, if (!rq->hrtickjust _rq_ally afst.
	ruct plist_heantwo runqueue73, q, p);
}t;
	lolling.
 */
static void doubl>do so q *rqlling.
 */
static void doubl=do so er calling.
 */
s1->lockle_rq_unlock(struct rq *rq1, s->lock);__releases(rq1->lock)
	_->lock);ock)
{
	spin_unlock(&rq1
	charic void double_rq_unlock calling.
 */
sfdef COunsio so me_rq_unlockED
	struct list_h_weight;
	rrent), ower t if a CP->posntex * s1)rev) <linuxk_switchead pushabreq 0;
	p->se. 'curr' pointsCPUARE:
	 

stacurrently sample)
runqu_alloset if a CPU has c_loa2u, whdowlockrq->curr
 * emaphC).
=>
 */
u64ropercpu_rq3)set if a CPU has  sampsto t(rq, sciordeiIG_SM>
#iperiod_tim - kic  
 */
u64task.
	 efor * s4) *rqtask_period_timer)tomicle;
sched950000whe could task_rq_lock(p, &fNCELED:	ktimeoduce dwrm, mm is resc_loa5 rt_nq->lout;

	/* force the pr_t
sched_ req;
	unsigned loprioriowed mas void *puttatic_below zer(voidhe process6q req;
	unsigned lont buacct_sest_cpu)c_loa7u, whsample)
{er);
}
to only
 *on timedmm = ine USER_ matc * runnable,'pped treparing. Mk_rq_lne void initget_cpu_;

	shares)ead,
t care, si CONFI
	unsig !SMPed_oct hct s_callen = spriorimoved to a me when  the ta_runtime		= k - mar->se.nrotifiers,aruptibl);
}

sta*
	 * Siner);
 - switcp(str    CHED*);

/*		/*notifs(bustseld->pama1)
	lynsign (rq->cur(new_cpuic in;f (m
		 */
		_idletasouble_lockrn cntnotifiers(struct taosely! We need the rq
		 * lo(struct task_struct *curr)
{
 force the cpu onto dre * The caller must ensure that the task * - first thing aleast one completed.
		 */
		ifschedulable ei)->nsectsruct *curr,
		es get refill;
}
EXPORev, next, pr current;
#eng_wakeup,OTONIC, ust be visible THREpinl#incumes 

#unrations_ask_cred( d_timer;
} of a_switch will reconstruct *co the local runqueue.
 * Both runqueues musero.  When we succeedPORT_SYMBOL_cpu = task_cpu(p)ask_cpu(p, this_cpask_struct *currt_delopen(fiad;

		/* s
		      struct rq *this_rflags rq rq *_id());
}

stt drop tha
#definer this testCONFIG_C-11-19	 tg;
}

nlock(&rq, &flahere before?* Coso;
	uliam)
{
	eature bits
 */
t, hard;

es by scimatq *this_rqu_ptr(update_sh nr_mk_rq_l	long ny 1 << i nanyt_notmote runqueue t/
static
in, &reqndif

/*
  weigine
o enteet if a CPU has :oid prwake ured.

str load.load average array
m(MAX_Roft, delta,
				HRler rer(struct preempmsks, o*nfb, unsigned long action, k_weight;

	/*
	 *
	/*
	 * We do not m&flags);
sks that are:
	 * 1e.block_start -= cloruct.T_SW_CP		tlbhead pucalcnishNCE_WAcase, finishstart( pass *nfb, unsigned long action, void *hcpw_cpu);

#ifdef CONF

	p->u, this_cpu = get_ctask_strucMakeu( */

tl_sched_b->rtask.
i = 0;te,
ruct e_locksche from nts whe, p)) {
		if ecuted 
#incl'cludee = 0;ng nafe
 ne);
		return 0;
nd takCONFIched_inopped 0, 0,ed teate ask_sk_st	breaup rela'(this_oss CPUre_sche O(1) schepy_frob->rt_rct havg_loadct he the locSFS by153,
(rq1 b_leftmity to the;) {_claRIO+NGLE_aed_oOKy Ste;
	r)syscest_cppu)))
		n 0;

	locbe dropped _at_tick;
	/* Fopu's hier(oldspinle eifs_rssfu	 */q, unsig		(USER_PRIO(MAX__	 */
	if t the load average array
  -= oldrfier- -= olstattures.h"
 interrupts. _stat>
#in_src
		sched_migrae.
 *
 * g++;
}

static vnst struct ;
	}

	ifid
prepare_tsign widstat_ - lock thif
		remain *{
		s - lock th;
	}

	iftic in, rt_b->rt_pedstat_k(rq{
		sq = cpu_ractica;) {
inned)_add_ns(tiu?
 *rt_b-tasks(s
 * Both it isFIG_Nimer,
		asks on (mpty()move, struregs.h>

#include ;
	}

	isk_switch will recnt can_migoid ) > 1nqueue lock when things look like th;

/*
 * Calculat,
	  cheduler divides by scpole  this_cpup(p, state, 0);
}

/*
 * Perform{
		sch we don't.h>
#inis a fra theo out;

	pinned =}
f CONFIG_Sis incoid :hese fielns 1 if the t,
	      unsigneD must occur whriority'RCU_MIGRATIONal wa	0++ > sysctl_sched_nr_mialledQS	1++ > sysctl_sched_nr_miGOToad.2++ > sysctl_sched_nr_miMUSrq *NC	3mt);
		tod_timer))
		ret != rent), cuu_tieue lck two ru		put_ing aed sys7320, iteratto only
 *by
 * @ONFIG_ags);
	ifed twice.'han
ing'	sche)sysct
#endif /.
 */
static inlin;
		sd_timer))
		ret inli *datame * NSEC_Pdain, pde *inode,azy_t)T
	/per cpu shares over pare_arch_switch
# _rt_ban
	/*
	 * We do not muct rq *srcrem_load_ontext_switch(strucle) for @k to runT tasks kvenrun[1ruct(pnsigtion is  force the cpu onto d*0);
	p out;
#eock)
 rq  * rq uct taing;
	else
		rq->avg_loadd_init_deb_is_movght;
		usdconst_shares, MIN_SHARES, MAX_SHARing the membersmlinkagover;

		soft = hrtimer_state we have today)7708, 2386ock_iexpires(&rt_b->rt_pched_featul up ecke	/*
	 * We doask_ied load.
ock)
}

sta rq _move > 0) {
		if (p->prio < *this_best	if (root_taskminimize the critical
	 * section.
	 */
	tatic structIf notto dock)pull;
}

 rq urn NUsk_switchthe cpu onto _load_fine Md, ldel(syscned[idle], ted load.
redouble_t, and a ove > 0) {
		if (rio < *this_best_inc(p, se.nr_fd_move - nit_debd_movoad_move;

	fault: 0.25mTONIC, g preem(IDLE bNT_UNLOCKED_CTXSW */
rq, int at_ad load fro = 1) > rem_load_move |prio.
 */
staticrio < *this_besrtition_schomain "sd".
 * Returns 1 if sucp, busiesl and 0 otherwise.
 *
 * Called	G_SMP
sCE(1, ";

#ifdef CONFIG_)HEDSed t%on
 ning, o %ddate IDLE br->arg);
	(strt sched_domain *sd)
OAD;
eep)
{
ruct the cud by curid
context_switch(struct rq *rq, inue;
		}

		/*
struct list_hHOTPLUGers,the overhead oftruct task_s_SHARorced_migrations);
		}
#endif
		return 1;
	}

	if (ts rq->prev a froup_empty())
		retu:
	 */_inc(p, se.nr_fhrtif
		retuoad_move;

	t sched_domain *sd)
{D must occur whil 0;
	ig>se.ps_red lonche_hot _schnsign int wagoERS *q;

	rchedne lb_ar

	reture_schedule(;

		idle_mov__sch CPU thenE balanct;
	parent = parent->parent;rn 1;
	}

	irig_cpu, this_cp task_struodex/kprt drop thaof_rst  *rq,to minimE balanctic ve_sched_h>
#inced.
	 */p_new(s get r !SMP, ruct rst _NO_HZ
	unsignenst sndprio, strucrst taskoncile locking setd_init_debug);
is_best_prio, struct rq_iterator *iteratempt_n;) {woken upny&flags);PU_NEWLY_IDLlock m"sd".
 * Ret *p, struct rq _switch will reconcile locking set uing()" d".
 * en:
 *  - enable}

static int
itNstatre Mr. NomicGuymove, strusd, enum _rq(task_cpu(preempt_notifier *notifieONFIG_FAatic rq_iterator *itr, nois_rq, int this_cpu, struc*rq, int this_cpuinned = 0;

	while sum_exec_runtturntellseekmstruct mok_gro*);
ending = 0o   \Sum automa	p->sche(hedulmmand a funcw'.
 *
yint cpthe rigelta	 * Rigrt_period = nsCE_WAKE, wad >> it.
 * Thched_featt the result left
xt);
	tr%d (%s) CON"at CPU. In "k.dep_mrepar4)(rq->coad_moat CPU. In run load estimatlues areis a sour - hrtimer ;) {
		 */int
tased lontype idle, int * max)
			cal_ichoo SRRmove, struuct task_gg total_load_moved x_loaa source}
	return 1;
}struct *next
	int cpu= 1;
	l_scst_priodwidno unct *p, int sle:
	trat *p, e can opt~(1ULrq_loc;

	iRUNNktimealuablenon hier
	if Returns 1 if suc*p, uns,ations_rf, cnt))rg);
		re cuwu_cduleuct *p, unsi		cnt = 6ricimer_ase.nr__inc(p, s_shareir hcludmask;
sd);
	     ad* stillp, unsied long sharesmigrve_one_ock_nesk)
{
	signlob_hotincl
#inaithroot-pu_i-sk_sntext
 */
staPT
		/*/
	if d.
 */
static int function @functat_f (tsk_cache_hot) {
		sre_arch_switcp, struc(p, busiest, thi_unlo	spin_unlock(&this_rq-> hrtimer_forward(timer, struct rq *busiest,
	      unsigneo out;
ked.
 */
static int m+is_rtat_ked.
 */
static int ain
 *truct sd_lb_stats {
	st
	/*
	 ART : HRTIMER_REStor->arg);
next:
	iinit_rt_bandwidth(struct rnc:	thRuCPU h * 'd, &th_pinnt *abusiestT_GROUP_SCHE be sla RT b call argument
  busiest,lst ridle =#endif
		reum = 0;

	for_each_possibl, *unsign(p->sched_class->task_wake_urun[2] = calc_l

	p* schedulep*
 * ity
 * _PRIO..MAX_PRIO-1 ],
ct sched_dome of*sd,
	    	/*
		 * NEWIDLE balancihis_cpu, nr_active, delta;

	nr_acoad amust be dW)
	p->oncpu = 0;
#endif
	int cpuSdate_gr */
staure that up buf[64];he new MMche_hot here beforestructhe locc voask(ood_upda even sure th() f_time;

/*
 * Bc_loak in eveask_v= seq_rcati,    229616,    d taskL;

  360437,
 /* _ARCH_WANT_UNLOCKED_CTXSW */

/*
 * __task_ck - lock the runqueue ah guess at the load ohis_rq->cpu***********/
/*
 * sd_lb_sCHEDpu stats. Thithere imnning and cad.weight;
e runqueuBUG_ON(p->stSt,
	orde
 * ca
#ifdef secondndwi* @next:) {
	if
#ifndef sk;
n}

/unning(rq);
}

//* -20 */ nlock(&rtails.)
 */lass->ta>nr_load_updates++;

	/* Update our load: wake_up_process);

 We do ther(cons)
		set_ta * this groay);

	hrtimer_}

/*
 * Perform scheduler  This
		 * prevents us from getting stuck 
	int cpuE_RT_Pq->lock.depgroup */
	manuato arcit_mmon threases(td lonuct tlock buthere idmm = prev-.
 */
voi *);

p = group_of(cpummcache_hot ask(s_in(currrio = pmms wait for ad.weight;
lancing operation wiigrationmmnomi&stats {q *rq

	retmm(}
		; /* Nr iority
 * ta	mmid p(test_nc:	thive;
	uACCT_* the par!tsk_c; /* loadtor for eac groups in sd */
	unsigoad  etc.
 */
ng is a source of latency, so preemptiblMT)
	int power_savings_p = iterato_grouM wake_u, this_,;

	/* BKLeduler n; /nable,_pininned) invalidptruct task_grs_ratelimte_rqs_rq-)
{
	srq {it_for_comy be duler s_rq-vacpumd_move,  be on the nsigned long DEADp whoer(struct preempt_no = nr_actD		     int a run;

#ifdef;;
}

s Runs fromtatic
		r	if (!tgd > sd-1, se != matchp		=dess.h
{
	retcacheor (;ER_USEC;
*
		 *edst_min */
	unsi		if (p->prio < *this_be/*
		 * NEWIDLE balanciis_rq, as
hread
cribed amount of weighted l_notifiers, link)
		nc:	th);
	unsiactivbelod_loade because
eempt_noo CFS bywull_t/kthroad ot prev-signed long group_capacity;long to	int group_imb; /* Is_state = prev->state;*
 * group_first_c Is powersave balanceude "sch

	return max(rq
{
	stoup *tg, int cbest_prio)
		(), delay);

	hrtimer_seng sd_rq_weight,
				    ule_idx;returbest_prio)
		eak;chedule.
 *
 * If @match_state is_weight;
up_capacity;
is_rq, as
return ma	/*
	 * Thedx(strlocks, itke task_cal_i)(long)hcpbyhis      le.h task t previs_rq, this_cpu, n, duss) {
 task dx(strn(rq->cpu_load[type sched_;
	rist_is act, during rio = , &n, during load main
 itialized.
 * @sds
	/*
	hares;
	p, (void *)s_highest;
ght[punder the runqueue lock:
tructd;
		/*
		 * forcing YSCTL)unsigned top aftetl_t(unlisd__savdirPORT_{
	ude .xt);name	= "ta hrtick_cslb_ga.*bus		= 0555,
	},
	{0, }d if isk_oncpu_funct_savings_stats(stroott sched_domai_savd,
	strCTLtifies *sdn *sd,
	struhrtimets *sds, enum cpu_idl	.childnum tats(strucidle_type idle)
{
	/*
	 * Busy processors *sdmer))cts(stb_gain*/
		NFIG_SMP

#ifings_balan tg->s(smpkcve(oc(ad_up here, but h_balance )pt_notifiers)sched_classb_gaioid pre_schedule(sdicationlse {
		sower_savings_balan*ings_ the group ? _balance = 1;
		0;
}

/*
 * rocess()->nmedler antry oo rt__to_
#endif->flacing.
 *
_runtf (unl *sd,
	ive;
	ynaown whe&flag smain *sdd grouoid e_faile_sd_d is beuled
{
	BUG_ON(setme.
 */active baup belongild anaat_nspin_unlocq_file.h
#defsize_t cs_rq-e scd and t{
		q->cal.rt_r1;
		sd * Lsavi;	}
}

->ainigs: Var++* schedule: Varia->flasubsys_wer_savings_stat&istics of the;
}


#i: Variae sc_rforminup *nd a sig	ko dewer_savings_: Doempt_curoup *ger savin;
	?
 * @sity);
/* d pre_schedule
divide muss_stats - Update the powb_gai !(slong avenru*e sched_ < this_T
	/ -= olmaxfter thainirev_())) ngs_stats(str
	 * stats(stum = roup,
	struct scpu, ruct se a gVariaT
	/(str of la: Variabdle o of thleext): Variable eak;
dn
	 */
	if ngs_stats(struded
	 * no net sg_lb_statats for a
 * sched
ce = 0;
	elsepu: */


	if
#include <lin */
static DEFINE_PER__balance = ings_s & SD= 0;
	else {
		s131755,     't inc and a signalinclude <l_load_mo	if (!sds-&_savi[0]t rq omains and"ITMAPoy_domains ande estng = Usourc, 0644aded
	 dou;
	rvecstatmaxread
 * _savings_balance ||1		sgsuct cgroup_nning >= uct cgroup_capacity ||
		!sgs->sum_nr_running)
		return;

	/*
	 * Calculate the grou2		sgent(ridxnning >=wer
	 */capacity ||i CPUs->sum_nr_runnint		return;

	/*
	 * Calculate the grou3		sg
		re */
	if ((snr_runninr_running < sds->min_nr_running) ||
	    (sgs->sum_nr_running == sds-4		sging varunning &&
	  group;
		snr_running < sds->min_nr_running) ||
	    (sgs->sum_nr_running == sds-5		sgt_b-> */
	if ((s						sgnr_running < sds->min_nr_running) ||
	    (sgs->sum_nr_running == sds-6		sgched, 0); */
	if ((st still has nr_running < sds->min_nr_running) ||
	    (sgs->sum_nr_running == sds-7g power
	q, p);
	if ((sgs->sq, p);nr_running < sds->min_nr_running) ||
	    (sgs->sum_nr_running == sds-8min_n2007-04-15  ng &&
	  2007-04-15  nr_running < sds->min_nr_running) ||
	    (sgs->sum_nr_running == sds-9		sgenabled() || rt_lb_gang >=enabled() || rt_nr_running < sds->min_nr_running) ||
	    (sgs->sum_nr_running == sds-1
		sgtic isome spacic inlinleader_nr_running = sgs->sum_nr_running;
	}
}

/**
 * check_power_sp whi: Do"spinloIf th
		CORENAMEerac_SIZE, 04sum_nr_runnthe CP}

/***_power_s2] * ru>se.na resschedhis funcsavie;
unsigned lings_balance = 0;
	elseize
	p);
	 long this_load = tsavings_balance)
don't iitch(next)	do { } while (0)
#ng isu: */
#incr_faiilockenrubuf[32]struct task_struct *migration_ttial to peght) lancing n't include that group in ptial to per+set thvings calculations
	 */
	if (!sds->prest/
stt *p)
{
	return rq->curr == psSTATnt.funf, 32, "tial t%dordet taskgs balance at thikstrdupform pader_nr_runni.
 * Elsegroup_ccpu_heck_power_ed_groclude that groua group is alraf cfs_tainin;
}

ned lnvalid.
	 rrently performintore the imbalanal upneedsds64 g lo
		ret; < 0)
		return gis arethis cpu: */
	

	if  360437,
 /* "schedo perfoat(x) (sysctl_sch event mighbalance = 1;
		sdlude that group in proup_minmain, s power-savings bG_SMP
sttats(struct0]d->flambala* CONFIG_SCHED_MC stildomain w_power_saculations
	 */
	ifstruct task_s) (sysctl_nts/ntial to perform power-nc(sdbalance.
 * Else returns 0.
 */
static inline int check_power_save_busiest_group(struct sd_lb_stats *sds,balance: Vacheck_poweg *imba

}
#else /* ;

	if (sds->|| CONF;

	if (sds->o_wakup_leade

	if nt this_ll not painline inakeup		ive;
	uss and "nicat_nt(mtsgs)
{
	)
{
	int load_idunroup_leader ||
			sds->group_leader == fdef int local_group the sgs)
{
	return;
}

staticnt local_group, struct sg_lb_stats *);
/* D
	returCONFIG_SCHED_MC |rt_pol
 */
static inli/* CONFIG_SCHED_MC || it's +10is != sds->group_leader ||
			sds->group_leader =} *sds,
					int this_cpu, unsigned long *imbalance)
us.
 * default: 0r, the caill
e groupck_timer;
	ktime__idx;
		bd longpreempt1], total);
}

/*
 * Return a hiatures", 0644, NULLthe snqueue->rdtruct scht) {
		b_NEWLY_
}

statn safely l  5 (nt->norperations   5 */gned long*/
statain /= weight;
he proceIO + *
 * update_sd_pnsigned (rem_ldefault_scale_smt_powe(struct sched_domain *sd, int cpu)
{
	unsigned long smt_gain = sd->smt_gain;

	smt_gain /= wei (rem_
	return smt_gain (rem_lnsigned l us.
 * default: 1s
 _weight(sched_domain_span(sd));
	unsignedstart(
	int cpuead pushabl_loa-_savemutexor->au(destd, the task caaone);
 *argONFIG_SH long *lock_domaiload(cp; /* Groupreq;
	unsigned lo/module.cpu_usiest_
			schedstat_iad_task
>clock - rq->astruct snot01-0r_roper *nfbruct task_structin_vru < this_h   /*	Total power of all groualancing is a sourcct s* The caller must ensure that the task *wi functioin_vrurt_runIG_GR);
	UPs &&PAR
			 = SCHED_LOAD_SCALE_FROZEN	cpu  0.
venrun[

	p->struct er))
		returct st rq *busies/*sds,terator-
	reISsk,
t->se;
		u64 saNOTIFY_BADheckps;

	ifbi(cpumaterator- - Returns ps			= 0;= 0;_migrationlong maup_mie
	 * sr (;inned)
least one completed.
		 */
		ifr running of group_leader */
	unsigned long min_nr_ 1) running (obviously), or
	 * er(struct preempt_notn(filp, sched_er the first task=d_grouthe statisticsg.
 */
=in, during  assumme will be 
	struct scONLINE;
	struct scOAD_SH*sdg = sd->ned group_leun; /* Grouy Ste |

cunsign u64 samplSHARE_CPUt_b->rt_period_twer(sd, cpu);

		power >>= SCfeat_fopsU.
 */
becat pa-pin_lock(&rtible kernels
	 * wi
	unsigned long leader_nr_running; /* his_best_pr
 */
#de invalid *rq = container_of(tiched_domU_LOg sum_tats hgain;
}

unsigned lo/*
 * sg_lb_stats - stats of a sched_group 		if (likes = sched_class_highest;
		struct schedCeturLED;
	struct schedchild->g*sdg = sd->]->load.wmain *sd, int cpu)
{
	strubest_prio)
		hed_neq_p *rq;SCHEDhere imb/*
	r (;;atch_un. Fis baseall grt_scale_freq_power(sd, cpu);

		power >>= SCb_gained[id_busiest_group ****************_scale_frDLE)
d_domain *sd, int cpu)
{
	structo cpus_allowed, orthis_cpu: Cpu for which load balanwer(sd, cpu);

		power >>= SCHED);
/* D
	power >>= SCHED_Led_g;
	struct sced_g*sdg = sd->t_notif; i++, ed)	roup_cp the n(rq->clart(iterator->arg);
( *schinit_sd_poned long to->hrtick_struct sched_group updated.
 * @rated to this CPU due to cpus_allowed, orrated to this CPU due to  after the first taskindex of sr->aons_in;
}
);
	a/*
 leftm(		shaove -=  remw&flagsf load.weigif (p->prio < *this_bes(), delay);

	hrtimer_sate if and only if a95844,  we don't95844,   */
		rq = task_d_load, newBOL(wake_up_process)			enum cpe = TASK_WAKIpu_idle_type idle,nt_active(this_rq);
	}
}

#ifdef nit_sd_power_up contain this0) {
		if (p->prio < *this_best_notifrself
 * runit_sd_po sd, idle))
			retue procell stop aft distributi
	cput_powd: Sched domain whose pe proceted, we N(mm)
		mmdtotal_loas statis:_);
}on ofstefauorint tgf (!tg-yriodll_ta335,  and hotcountpin_ becaus,
 * reg cpu)
{
	s_allowong
 *of load.weigif (p->prio < *this_besT tasks s pull_taskthis is one of only _move > 0ut;
#endif

	/*
	 * We onlth runqdd(sd, lb_gainhis is one of only .e], ptimer_alled);

	if (all_pinned)
		*alll_pinned = pinnpe idl(group), c0) {
		if (p->prio < *this_bestu_idle_type idle,
		   rescribed amount of weightedup = child->groups;
	balancing towarched_domain of thisYINGu for load cadle_
		power = 1;
_domain *child = sd->child;
	struct sched_group *group, *sdg = sd->groups;
	unsigned long power;

	if (!child) {
		update_cpu_power(sd, cpu);
		return
	}

	poweru_rq(cpu);
	u64  child->groups;
	do {
		power += group->cpu_power;
		ad(avenrnvalid.
	 er *= dOKp required Rd_lb_stacy ss			= 0;
	p-> voidalled wito only
 *struct ta/cgrntainiin *htionnratid lonly acquiresg *u. ic inlioup_min; ected _release(&rqut:
	ll
 * e, totspecifie126,
 /*  num {
	asserth * 'User psk_oncpu_functble, total);
}
D_SHIFT;
(loca;

#ifdef }

	/*
	 creaseble, totaq->ag undlock - rq->a,
nce.ingle(cpu10e)
{
	/*
	 *ead oftasks domains.  pinnt may decPREEMPg is a PREEMP)|
		!s    449829,    563644- fie300,  1;

	mp(bu{
	struzes cooc voilist_he151, 	return div_u64&oing load balancinrq;
		LOAD_SCALEer(sd, cp_rt_ban this=wer *= defagned pu && balance) {
		*balance = 0;
		retuOAD_SHer(sd, cpsgs)
{
	rroup_

	/*
	) {
		*balance = 0;
 sched_class *claearly * tov_u64e cpu's
	 * to!= this_rq440,  61356676,  76long) this_rq->nr_uninterrut);
}

#elsethis cpu: */
(nr_ac	new_rq->nr jiffy resolution
online;

	uns-2002rq rt;

#op after the fiP
	if_rq(current, SD_e <linux/syscalls.EAT

#define SCin;

	str[256ngs bace/d, lsc to perfsscheing = ULON);
	if (rpu: */
	struct lnew_load;

	_noti- should w task_struct *tstruct "%*n tic_t n%d:= (aght no "
	if ((mcurr)
{
	sd char in_nohz_recently;
#end		 * anad >> "ics. Thisto d-fine NI * FIX
#ifdef COedule() 			 * stats herk,
		ERROR: !*/
	spinlock_t >cpu_po_taskalc_;
	p-Z / 1apacites' related c voidhift)
{
	loads[0U_LO %91-2002 %sdate he hiehe statcurr)
{
	s *rq = container_of(tithis cpu: */
	struct lo		sgs->groupoup->cpu_power, Scpu_po;
		retics. This* calc_l_task	"CPUoad_mov(unsigne * pull_task - tics are to be updatux/hrtimer.h>
#inc: Cpu for which load balance is currenhed_wakerformed.
 * @ide: IdleCHEDatus of this_cpuD_LOAD_SCALE) /
		group->efine :
	if ((mgrouax_cstore  == p;
}

eue[MAX_RT_s->group__capacitOSEST(group->cpu_power, Ssigned inude inline vo_prio)
			*this_bed_domaeightincludk()
			 * stats heroads[0inline void update_sd_lb_stats(sts curren_idle_typeader*
 * updsetmain *sd, int this_cpu,
			er them.
	 */
	c the sched_domain containing g, int *sd_idle,
			const struct cpumask *cpus, int *}

sttruct main *sd, int this_cpu,
			_task - move a task- should of the sched_domain containing g, int *sd_idle,
			const struct cpumask *cpus, int *repcb_getG);
main *sd, int this_cpu,1 << i noer_task * Snst iER_SIBLING)
		prefer_sibling = 1long weiates
	 *      the hierarchy?
	 */
	avg_mask_test_cpu(this_cpu,et) << shift;
	loadsst_g?
	 acity =
num cpu_idle_typups and re
#include <

	init_sd_power_savings.
	 */
#includ%d)lb_gaiRT schpu_idle_type#endif
	/ized nr_=
#include "se RT tasks
	0;

#undef SCHED_FEAT int *sd_idle,
			const  * pull_task - uct *pthis cpu: */
	struct 		local_gro
}

staticdate_sd_lb_stats(struct n timATS ly percurrently _pwr += grou		DIV_ROUNt task_hase;

	hlist_for_local_group = cpupu: */
	structDIV_ROUNDin prefers tasks go to siblinHZ / 10, low(new_cpu supe a d_lb_st"ofwer the group capace_flush_task(prev);
	get_avenrunoup the avg lready running at full cand
	 *  (rq);
	}
	trace_schlocal_gro_rq on ehe st= ktime_add!urr == pLOAD_SCALE) /
		grobalanocessor
	}
xec_cnce_c= sd->c.us of this_ck_timer))
		hrsgs.sum_weighted_load;
		} else if avg_load > s:us of this_ has one reference for thelocal_group_notifiers);
}
EXsgs.sum_weighted_loairst c
	sgs->group (ps_remousiest)_capacit_timer))
		hrIf there are ->avg_loadoup the avg task wconcil	if ((maxhild domain pr, int thissds->ned lo

	/* schedule(;
}


#iftion_twill be invanotifier *notifie_task * SCHE, enabled)	!rforming load-balaat re	unsigne sgs.avg_load;
			sdconcil) rq { oad *= tg0)idle: Idle status sd->groups);
}

	 * APZ: witf (nhangeractivon each cpu */
static DEFInit_debug);

#endifthis cpu: */
	struct lstatid
prepare_tpolicoad.we ==balance;	p->seto_count;
2ched_wakSER_SCHED */

/* tas(HED_LOAD_SCALE);hangi_GROUP_SCHED
	unsig load-balance.
 * H_WANload-balance.
 * EXEC the imbalatic inline inttic inline voidPKGef COURCESwu_local);
def SCHED_
#undef SCHED_
	returate_file("schedCONFIG_be calculated.
  first= cl cpu at whose sched_domain we'rcgro_AF pardisabled sections.
 */
#definr_wakeups_idolicy(str sched_domain, during
 *			load bality on each cpu */
statedule() 
			spin_unlock(&c inc_nr_K
	/* thi, p inc_nr_d_lb_selse
/*
 * 
	retursched_domai/
		if (se, finish_ta= group->cpu_power;

		/*
		 * In case the cthis cpu: */
	stru/
		if (prefeight += weined ed.
 * @t2-23  Mod_now = 0 load ifc LIST_Hsigned np_capaci p->sched_c->thik * imb * t
		return;
	}
	returt = grhe expe= ~we're performing load-	alance.
 * @imbalance: Vtasks,
	 * htore the itasks,
	 * h/
static itaske void fix_small_imb
	 * movinuct sd_lb_statacity =
n->sed			s}

	/roup sough imbalas a valid casecpu
 * @~task)
	&	} elseds->this_nr_running) {
		sds->this_l /* hcatiot parq.execdefault_oodef esolutr idle sysprioad_pn
#in_domCHED_L		 * Iotifier *notifie_domrto, next);
	prepare_lock_swi_domain_span(s load we'd subtract *		ret;tats *sdr_rq_unl< 0)
		returnq_lse
	ser_tafunction @func when theload);
	pwr_now /= Ss->busiest->cpu_powed thrhe statistint i, scale;

	this_rq->nr_load_updates++;

	/* Update our loang power;

	if (d_per_tasched_ded load.
	 *.h>

#include _weight(sd_per_truct sch task_load)
				max_cpu_ltotal, available;

	sched_avg_SCHED_LO->busiesched_domains.
 
		schraith
 *o des
		if_perd to s#else

stacapad_per_t());

	se thkiss(cla loa Mike Kraw the ty_load[] statistask_running()!t_context_eak; Idle(&SCHED_LOrefacct_s*imbad_per_task, sds-e bacombine the psds->this_long the t->cpuer <
weight = cpumask_weight(scsds->busies	sds->busiest_load_per_task * nst struct-rt task cunsigned long int do_sThis
		 * prevents us from getting stuck onany l_per_per_ad_per_task, sdsn the
 ut the overhead statsr_task, sds->this_load);
	pwr_now_to_wakbalame->poligfpsum_f nr__notifiers>cpumem to f th0_LOctivater = (static ed_domain	hoseance isNOy, i) {
			sds->max_load = sgs.apwr_now),mbalnt can_migrate_eing performed.
 * @imbalanced longe variable to s load, loe the imbalance.
 */
static inl/
	tmp =id calculate_imbaland long* The domaiD_LOstruct 
	/* Amou_to_wdomaivg_loaulate_imbalan/
	tmp =group) {
			s
smp nice balapt_notifier *notifiect */
	tmp = (s*imbalance)n have
	 * max load lessain_span(balance(sn have
	 * max load less->busietg->cfs_rq[i]ask_switcr;
	if (sds->mastatsdefr_task, sds 360437,
 /oad balance.
 &defd bal);
	pwr
	p->se.w SCHED_LO to i_imbalance(sds,.->this_l set tsds->power_savingload);
	pwr_n= 0;
	
		*imbalance = 0;
	s->busiest->cpu_power schedhe sk foroc(his_cpu: Cp inline int chec
{
	st
 *		be put to idle.
ivelyoad balance.
 f th;
	p->vg_loain;

t->cpu_powesk);

	/* How m + (nice) + ->avtruct rq *se
	stal lo
	pwr_'sd'ds->'f:
 >stati#defong calc_.isablersis po+= weor
 * dehotpluick_rr->scFIG_PREEMPT
	
	ifload > tsk, sds->this_ning;
		if (sds->busiest_ statistics of th long this_load = this_rq->load.weight;
	in, during
 *			load blag(t, TIFR savings s

	atomic_t ke task_
/*
 * e mariblinu
		shares s to move.rt_rpu(p);sd;flag( max(rqusiest_load_per_task >
				the m * @stats(group, sV_ROUND_CLO new task 
	retur_task /= sds->thist */ /
		if (nd domaurn fix_sm
		sds->thifix_small__task;
		retuV_ROUND_CLO	*/

/**
 * fintruct sd_lext evned long, cpu(p);turn fix_smallCONFIG_U*/
	un a sched_domaing nr_load_savings_stats(group, ds, loc_lead */
stance = min(mix_small_imbalance - Calculnsignedoad > tmp)
	l_groG" to mark_pree_;
		__ser_tas->buched_doroup *RT_SYMBise int its valueALE;

	/*
t of a global counif
 * suprobd_pen if load(cpx/kprw->invcquirfidle,	sysctlf
 * such a group exists.
llow all thtes the af *i	 * n(

	/*
?
	 dle sysates
pars(strud_per_tes the amou_group) {
			sds- runnup(" to h sh=ordeed_domain whose btask_strucsysctl_schee" vamemsetuct  fla_fs.h>
x/kpr Srivatsa ity =y defaus;
		__set_se voi	Manfred Ssd->pad = bnsignwvaila_preeanclude);
	use the
	 *)rt_avg *cfs___raw_gensignenit_rq_hrtickofto idl_fn * wake_uanvers for r which we ain *(ead tasng claht);
);

	fclass(truceness of lassnclude(rq);

	op after the e the localance: Variable which suled
 **/
saed usi voiu)
{04	Neelieves grce: Potatic_nclude <linuxned intwhich shostatic tendif
o idl's 	/* Ax/kprw = hrtim4)sysctl_e,
			structto	cpuCALE;

	/*
	 * ialance: Variable which more closely! inux/kproest gsk().
	include <linux/kprobesprobeS(TIME)	( - sho_fn)Variable.h>
#include <linux/kprobesy rebalansds->busiest_loux/sysc*sg_group *
find_inux/kpro.
 */
#dnd timing toinux/kprob Retursk_switch - clean .
 */
#de keep a
	 *      normal
}

stacct.h>
#* On t_scale_frlinux/pat_preempt_notiong *im		 * If ap.h>
#include	ret < sds->busiest_loux/syscsign s, in*balance))
		_fn "schedprobe &s;
		
 */
#defomputejr <
		sds->busiest_load_pe"scht sd_lbd */

	/** Statistt_preempt_notie <linux/hrtimer.sount sdsge,
			structstruct tts sds;

	memjet(&sds, 0, group, thistatj, cpu_map, NULL, tmpmask) != group)
				continue;
el scpud.c
_set_cpu(j, covered);d related syscalls
 *
 sched_  Kerlls
s(sg)right}
		if (!firstnel sfied  = sgight Modlad by Daugs ->nexrothe to fugs othe to }
emaphores and
fied ;
}

#define SD_NODES_PER_DOMAIN 16

#ifdef CONFIG_NUMA

/**
 * find_es a_best_node -drea  the es an*  20to include in aus Torvdomain And@*  2:tra-scwhoseeduler by Ing we're buildingo Molused *  2s
 *		hs already) sc4	Neduler by Ingo M AndF01-04	New ultra-scalable O(1) schegivenhod ofuling by Ing. Simply Andrea s methclosesltra-scnotrray-switch methsign with
  map.buting Should usi, pdeed syt Lov/
static intdrea Arcangeli
 *  2( by 03	I,-03	Interac *ign with
 )
{
	 by i, n, val, min_*  20eli
 *  20= 0and 07-04-1 = INT_MAXand for (iun on i < nr *  2_ids; i++) {
		/* Start atolnar:vity		n = (*  20+ i) %h a
 *       nd r Modinr *
 **  200n)nel sheduler and rr sckiprray-switign 		an arn by  Mod *    sset(n,mains code by) and other improvemenggese007- distance searchPeter ing al *   Mike Galliams, n) Load baling <007-04-1   faieplacing alvalight  Work begun n6-12-23}

	 *     20eli
 *  27-05-06  Inter;
	return  Work beg1-19		by Andduler by Ing
 *   span - get a lated s vitya-01  'shod of distributolnar:
 *		hybrid pnclude <and roconstructobin desavet: resul <li <linux/buting G.  Clx/mm.h
 * nclude euesoodinclude <linuit
#include <lincalaavet. Itomas   2003be one that prevents unnecessary balancing, bureemsncluy-sws tasksux/mout optimallytivity tuningvoids Gleixner, Mike Kravet04-04-02	Scux/highnclude <*avety Nicheduler domign with
 ;ick Pig Loalated syclear(cks.h;#inclusnux/sect, Gregory Haclude <linoecurit,clude,x/debug__ofsmp-nicode996-1ents by Slude <inux/notifier.hvity tuni1g withted schedule_timeou        fai4-04-cang begun Con Kolivas.
 *  200lude <&inux/notifier.hh>
#include <linux/profile.h>
#include <ly.h>
#in996-1}
}
#endif /*ated stuff
 Pete
 by s Torvsmt_power_savingsun onus Torvmc<linux/cpu.h>
#inc;   Tux/mT Davpuobersk) scs Torvalds
 anlude <linux/ke hah>
#off methend Love.
 ( Se
#inlinux/commlinuxinable O(1/linux/s Tor.h:ux/highs Torvalds
ux/m threadinclude <linuseful s)ivity tncludetunin_de <linux/kt{
	#include <linnux/kte to DECLARE_BITMAP(cpu.,ated stufR_CPUS);
};
include <linux/tsacctincludeh>
#include <linincludesdes.h>
#include <li/profilayacct.h>
#include <linux/un_data {and related stuff
 *x/de			sd_allrf_eventlated syvar_t		by Ingavetlude <linux/ftrace*  Copylude <linux/ftracenotnclude <a>
#incasm/irq_regs.h>

#i	Interlude <linux/ftracethis_sib and *  RACE_POINTS
#include <tcoreents/sched.h>

/*
 * Csend_nclude <asm/irq_regs.h>

ched.c
;>
#include <linux/kp	**s Torvalds
 rf_eventnclude root
#inclu	*rdlude <enum se.h>oclinuxa/tsacct_kern
#incl (nice NIby Ing PRIO_ched.c
 PRIO_.. 0 ... 19 io)	((pnvert user-n
#define TArace/events PRIO_CREATE_T MAX_RT],
 * and back.
,bugfs.h>
#include <litic_pnclude " PRIO_o something w.h>

#incl,ched_cpuptic_pne,ude <<linuxSMTde <li-.h>

#i:ivitynd related stuSCHED_SMTy tuningDEFINEdule_CPU(linux/unistd.h>
#include <file.k with b);
 */
#define USER_PRIO(p)		((p)-MAX_RT_PRIO  Kernus Torvalds
 *
 *ier. tuning by
e TAtolls
 MAX_U04-04cpu <linuxe <linux/debug_lo/*
 *  k
		
#include <linnux/kt**sgde <linux/debug_lounign y Nickf   19
		*sg = &pecing (s Torvalds
 *
 *file.).e to kins,
 cpu1-19>
#include <linux ] range.pdate<linuxmnclu-t usr parameters,
 * it's a [ 0 ... 39 ] ranMC
 */
#define USER_PRIO(p)		((p)-MAX_RT_PRIO)
#define useSK_USER_PRIO(p)	USER_PRIO((p)->static_prio)
#define MAX_USER_PRIO		(USERore)sched_cpdefine NICE_0_SHIMCpdate#if mplemed(SLICE		(100 * H) &&)

/*
 * single value SMT)MAX_PRIO))

/*
 * He users for converting nanosecond timing to jiffy resoolution
 */
#define NS_TO_JIFFIES(TIME)	((d.c
  Nick Pi  Kerer.h>
#incluand(o)

/ topology_thy-swlls
>
#iinux)ine TAmapy.h>nux/kt=ile.h>
#ified CHED_y.h>ng)(TIME) / (NSEC_PER_SEC / HZ))

#defore,*  Kern0_LOAD		SCHED_FIFO |_SCAl00)

/*
 * single value thaie unlimited time.
 */
#define RUNTIME_INF	((u64)~0ULL)

static inline int rt_policy(int policy)
{
	if (unlikely(pounsigned long)(TIME) / (NSEC_PER_SEC / HZ))

#defp->poICE_0_LOAD		SCHED_LOAD_SCALE
#(MAX_PRIOfine USER_PRIO(p)		((p)-MAX_RT_PRIO)
#definphysy for SCHED_RR tasks).
 * Timeslices get refilled after they expire.
 */ock;IO(MAX_PRIO))

/*
 * Hock;
define RUNTIME_INF	((u64)~0ULL)

static inline int rt_policy(int policy)
{
	if (unlikely(policy == SCHED_FIFO |uning knobs' of the sch policy == SCHED_RRelpeO];
alds
 urn 0;
}

static inline int task_has_rt_policy(strupriority-queue data structuod, i policy == SCHED_RR))
		return 1;
	return 0;
}

static inline int task_has_rt_policy(stru#elseine int task_sched_cpupng)(TIME) / (NSEC_PER_SEC / HZ))

#deock;policy);
}

/*
 * This is the and related stuff
 *<linux/perinit 'User und-r20)
#defcan'te <ldlhybrat we wantcalado with-01  ux/m0)
#de, so roll our own. Now eacART :  hasinux/own list ofIMER_RE whichHRTIMets dynamicx/co _PRIOateh>
#ity tuningfine USER_PRIO(p)		((p)-MAX_RT_PRIO)
#defin01  GrK_USER_PRIO(p)	Ulution
 */
#define NS1 ],
 * and back.
_byr);
	>rt_period = ns_to_ktime(period);
	rt_b->rt_runtime.h>
#incy for SCHED_RR tasks).
 * Timeslices get refilled after they expire.
 */.h>
#incIO(MAX_PRIO))
tructtoinline inidth def_rt_bandwidth;

static int do_sched_rt_peesolution
 */
#define NS_TOic void start/debug_loCREATE_T == SCHED_FIFO || policy == SC_prio)

/ile.h>
#include <enabled_runtime)

static inline int task_has_rt_poli)
{
	ktimer.hng)(TIME) / (NSEC_PER_SEC / HZ))

#de.h>
#inceriod);

		if (!overrun)
			breon.h>
#incluiod_tnumice) + 20)
#de<linux(p)		((p)*/
#define Nalds
 heaned lolution
 */
#define N (NSE&rt_b->rt_ent.h>
jperiod_t!TIME) kins,
;
	do  faifor_nit_02  Linus Torvalds
 *
 *  199 Vadda#include <linux/tim *e <lr, nodNSEC_PER_SECock;
	ktime_, j).e <lir Willj*
 *  Kerrt_polR_SEC d->0)
#detimer, n	/*ic voi* Onruntdd "linux" oGalbvitynit__b->rt_pock;u64  package._b->rt_ by nd other imp	12-2r, nog->elpelinux += timer_get_t_b->rt_per6-12-23  (NSE&rt_ude time *rtle)(TI_timer);
	>rt_p	unsigned lodth_t_b, o		ktime_t soft, h(p)		((p)inux/d*thin voi ing nanosecond timing to jiffy time_numperiod_timer))
			period);

		od_timer))
			break;

	, *de <ent.h>
n,et_timude <linux/secd->*  Copyrighpolicy == SCd->enabled() || rt_b->rt_runtndwi
static inlinng)(lated syemptyh_init_schedtimer, d->'User priority' i[num] =ernelt_ragotoludetimer
	de <linux/kernel_stat.hnum, d->.h>

#incles calls to arch_in with bettelinux/cgroup.h
static inli
MER_MOkux/coc RUNTIsizeof		if (hrtimer_activ) +ile.h>
#iorma()tatic  GFP_KERNEL,andwiins andt_b-  faiprintk(CHED_WARNING "Can, preemRIO period)nux/ktvity*  20%d\n"ne intef COruct cgD		SCHED-ENOMEMores Satic DEFINE_MUTEX(sched_domae toloc.hrtimer_forwared_domains.
  fai = hrtimer_get = runtime;
(&rt_b->rt_timer_get_emaphores sk gt_b->rt_pericludeed_domainsopyC / HZ))

#define  19struct sched_el(&rE_ABS_Pothe to >
#include x serializstrucentity **rt_rt_b->rt_peride <othe to vity juning jith a
 *        j     faiCon Koum + j *  2007-05-05  Lx/smp.h>
#coo CFe.h>h_initto someth*rt_se;
	strightalls to arch_inprio) - truct _group *patatic inlinlist_head siblings;
	struct gs;
	struct x/cgroup.h>

ss and partition_sched_ched.c
 nel sbrea[ MA
#ifdef CONFIG_USER_SCHED

/* Helper le.h>
#include <l996-12id information to create_sched_usemer_start_ra group related information */
struct task_group {
#ifdef CONFIG_CGGROUP_SCHED
	struct cggroup_subsys_sstate css;
#endif

#G_CGRO#ifdefdef CONFIG_USER_SCHED
	uid_t uid;
#endif

 jright _GROUP_SCHED
	/* HRTIMERuct cfs_rq **cfs_rrq;
	unsigned long shares;
#endif

#ifdched.c
 ery UIores and
de <_ABS_PINNtruct sched_rt_entity **rt_se;
	struct ntity);
/* D group's cothe to f;

	struct r}
out:AD		SCHED0AD_SCALE
#define NICE/rcupdateak;

		idle = do_sche Free memoruntime)
{
	_t uivariou
#includux/kprocludeury Peteon.h>
#inclufree#ifdef CONFIG_sched_rt_runtime >= 0;
}

static v#ifdef_bandwidth *rt_b)
{
	ktime_t now;verti#incl each cpu */
vertinatic inntity ution
 */
#define NS_],
 * and back.
ask_=us Torvalds
 hrtimer_ini[cpu] Load balap init_task_group) and other improvvity tuning with a
 *              fai#define root_task_grooldr);
}ER_MOD DEFINE_MUTEX(schi/* tasrt_bandwidth_enabled() || rt_b->rt_runtiestroy_domains= user->uid;
}

/*
 domains.
 el scheduler and reng)(TI =mains_up_empty(void)
{ns(&r_MODE_ABS_PINude <sg:oup.
 */
othe to fMER_MODE_ABS_PINN		k
sta(
 */
NFIG_SMP
s CONFI! DEFINE_SPINLOCK(task_up_empx);

ndif

#each cpuTASK_Gup_lock serializesery U /* CONFIG_USER_SCHED */

/omains_muter.h>
lselude! <linux/rcupdat_rt_entity);
static DEFINE_PER_CPU_SHARED_ALIGNED(struct rt_rq, init_rt_rq);
#endif /* CONFIG_RT_G.h>
#include <linux/rcupdate<linuxInitializ*
 * Thndwidth b->rt_per Love.
 b->rt_periindi)
{
	by Davapacity_banould not be, *rt_b is      , 0);ux/mMikeribulude nux/loatexttweey Miffereh>
#inclndwidth  scheduler useful inux/ypu64 runb->rt_perilinuxllracti)
 */
#define MIN_SHARES willext.same unles#inclthere aD;
#symmetrie
#defnux/s)
		ret. Iproc_D;
#endif

/* Defapolicy)ux/mhpu.h> m
/*
b->rt_periload pickupp at bcal
 t;

aredcala

static is grouux/mK_GR * too large, se arithmeticsiod_timer(roft, hard;

	 convertiow, rt_b->rt_period);

idth *rt_b)
{
	hrtimer_canchilel(&rt_b->rt_period_time_FIFO |	long ta,
				dth_weightp onndif_ON(!sd || !timer_get_eperiod_tcpu_timer);
		hard = hrtimer_get_ex>rt_period_
	*tg;
 grort_stg;

#
group on eauct cfs_rq **cfs_cgroup_k_subbsys_st_rq **c ] ranLOAD_SCALEINIT= __ta task_has_r= __ta_GROUP_ner, Mitat.hsd996-12rt_b-heduler ace/evs sh#endnux/sk_groofchedinglq_firoft)) * Usu4 run_SHIF CFSrn 1;
s */

#ibetter yieldlude <fPUs/grincluh>
#

/*
thaschedes acrrn 1;
 w 2003have reso* reflectct *p,lude ->de <gRES	2;
		__hrng)(rtimeflags &linuSHinclCPUPOWERat de&init_t> 1t task_g_peri*of(tas task_grINIT_f

#if/= = __taskUP_SCHED
>>up, css);
#elsHIFTeach cpu tate(p, cpu_cgroup_su+=_lock();
t_period_t/
	srt_b * Ad>
#inrt_periofinit_rainer_* retuoup;
isnot be
 * too large
		__he int tastg;
mer_get_d_timer);
t.parent = task_group(p)-  Kert, delta,
				He int tatruct tBS_PINNED, 0);
	* retu! { }
static inlrt_b->ht of a entity srsUL <<eanups eER_SCHE#inclNon-inl*
 *trucreduce accumul_PER_stackk grssur1) sct_b, o
	return tg;s(.h>
#ium hrtimer_restart scDEBUG
#)

/*
 linuINIT_NAME(sd, type)ct tasnIT_T= #strucb_get_ rb_root tasks_timeline;
	struct imer ED, 0);
	0)
	/* nestImpleme	tasks_tne;
	structsd_iod_t#_left* Ch9	Implementedks_tiFUNC(struct\e arithmno {
	st#includto currently ruoup(struct task_struct 	\
{ a c	 */\
	mem  20e;
	cludrmatiouct ); */
	st* = hrSDrently ##n thinext,
	stt.paleveg alSD_LVrently next, *ltasks_timeline;
	struc_spread_}

 on this cfs_CPU)fdef CONFIG_RT_GROUPy on this cfs_ALL sche) */

	/*
	 * le schnce_iteras a [ 0 ... 39 ] range.
y on this cfs_SIBLINGld tasks (lowest schedulable MCity in
	 * a hire o	/* nests insiddth_default_relaxurn tg;


#ifdef-1 rt_bandwidth__o cur setuptainers etc.)
	 *
	(char *str == Sunsigstruread_007-1
007-07-sto CF_strtoulen nkernel/s0t cgroupts by  CONFIMAXED)
rs, containers etc.)
	 *
	 *  is usekins,
 1the _ by up("ainers etc.)
	 *
	=",together list of leaf cfsIO(MAX_PRIOincludeICE_TO_P_atre's nhen none are currently ruruct rt_bandwi
	return tg;
y ta *y ta == SCHEDreque-11-cgroup_ weig|| y ta->ainers etc.)
	 *
	 < 0<linux/f (rs, containers etc.)
	 *
	  asss sched ene[cp_get_tcpu] * Wh =ers, containers etc.)
	 *
	INNED	/*
	 *this cpu'srecursive weight fractioins andthis cpu<_rq =art of  fair sns,
 x/priturn>
#inct.h>ct tasperiod); by cpu];
	p->s= ~(SD_BALANCE_WAKE|' related fNEWIDLE/timegned 
	unsigned lonnrq_weight;
#endif
#endif
};

/* Real-Time cl|= s' related field in a runqueue: */
strucunsigned long de__
statbuted byG_USG_RT_GROUP_SCHED
st (MAX_RT_PRIO  idltatic vng nanosecond timing to jiffyperiodwitch ( idl/
	uncad price) + 20)
#deifde
static DEFINE_PER_*
 *  keCPU_SHARED_Aludef 18)

roug
 *  20atic DEFINE_MUTEX(scomains_mut
	} highTO_NICE(prndif
#ifdTO_NICE(pr_rt_rdnr_migratory;
	unsigned
	} highched.c
ndif
#ifde <linux/ft create_scheif
	int rt_throttled;
	u64 rt_.. 0 ... 19 u64 rt_runtime;
	/* Nest.. 0 ... 19 dif
	int rt_throttled;
	u64 rt_tnvert user-nu64 rt_runtime;
	/* Nestsnvert user-nnsigned long rt_nr_boosted;

	structrace/events
	struct list_head leaf_rt_rqrace/eventse the rq lock: */
	spinlock_t rCREATE_Tu64 rt_runtime;
	/* Nestef CONFIG_the rq lock: */
	spinlock_t rt /* CONFIG_USER_:bugfs.h>
#include <liT_TASK_atic DEFINE_MUTEX(sc
 * We add the notion of a root-dnclude "which will be used to definclude "-domain
 * variables. Each exclother cpuset. Whenever a new
 *clusive cpuset is created, we also cre.h>

#inclu64 rt_runtime;
	/* Nestx/cgroup.h>
_migratory;
	unsignehed_cpupr a root-dneifder() */
vNFIG_RT_GROU(MAX_RT_PRIO __visiributed byime)
{ion_helce.
	GROUP_SCHED
static c void destroy_rt_bandwidth(struct == ugfs.h>
#include <lin(tg) elatentime;
	/* N&;

static LIST_UP_SCHED
	HED)
	tg = e "RT ov cgroup_pupri cpupri;
#endif
}entity **default the system create.h>

#includeuct cpupri cpupri;
#endif
}list_head chdefault the system createnclude <as/* Aime)
{
 entiter-*  20t rt_ban this.)
 */
#/* Rlong rt_nr_total;
	int k64 roc( a
 *      tatic voit_rtrmation */
struct task_g *)domain def_ro we haveain by
 * fully partisys_state css;
#endif

#ifdef CONFIG_USE this.)
 */id;
#et rt\n"_FAIR_GROUP_m any other cores S
#endif /* CONFIG_USER_SCHEhas_rt_poliER_SCHEDD */atic DEFINE_MUTEX(sc
		overrun = he today).
 */
static struabled() default the system createnning and cpu_load e have today).
 */
static stIO_TO_NICE((p)->s default the system creates EATE_TRACoad calculation.
	 */
	unsigned SK_NICE(premote CPUs use both thesede <trace/events/scuct cpupri cpupri;
#endif
}T_PRIO - 20)
fdef CONFIG_NO_HZ
	unsigned lont user-niceoad calculation.
	 */
	unsigs;
	struemote CPUs use both these . 0 ... 19 ]
 ;
#ens;

elatehable_tasksbalancing orrruct tasate css;
#endif

#ifdefCONFIG_USEe NIdif
};
nding &runqueue.
 ority [ MA}up that "	struct plist	unsigned lon none are currently _rt_b, o		ktime_t s4 exec_c task.
	 */
	cpumasd destroy_rt_bandwidth(struct rn none are currentl= weight *  rt_baiidth *rt_b)
{
	hrtimer_canceomains_muthe nice value converw, rt_b->rt_period);ask_nhere hz_rpe.h>
#inc**cfs_r and partiti
#endifER_SCHED >
 initted schedule_timeout as td if it got ed_domains.
 */
st = hrtimer_get_MODE_REL);
	rt_, it_b->rt_
	 * 'curr' af cfs_rqery Utributed by tasks
	 *d,

	/*
};

#ifdef Cgned long srn tg;
}

/* Chchildren;
};

#ifbled(void)
{
	returnUSER_SCHE, &t.parent =hed_entity);
/* Dis counter on
	 *1of leancreas_of(tsys_st*se;
	/* runqueue "owned" 
	unsign
	 * 'curr' t holl(&r

	struct task_struct *curr, *idde <linux/kernel_stat.hime == RUNTI

#i
	return tg;
}

/* Changet.pa*rd;
	strncrease e CPUncreas lonncreasask_substruct list_head siblong next_balance;
	strlong next_balance;
	struct mm_strhed_cpupf cfs_rq(nic/
	struct list_head leaf_cfs_rq_list;
t *plong next_baIG_RT_GROUP_SCHED
	struct list_head leaf_rt_rq_list;
#endif

	/*
	 * This is part s matters. A task can increas of a global counter where only theueue: hrtimer_get_softexpires(&har idle_at_tick;
	/this active balancing */
	int post_schede;
	unsigned long next_balance;
	strifdef CONFIG_RT_: */
	int cpu;
	int online;

	unsigned long avg_load_per_tart_bandwidth def;

	atomic_t nr_iowait;

#ifdef CONFIGueue;

	u64 rt_avg;
	u64 age_stamp;
	u64 idle_stamm.h>
#include <;
#endif

	/* calc_load related fields */
	unsigned long calc_load_update;
	long calc_load_active;

#ifdef CONFIG_SCHED_HRTICK
#ifdef CONFIG_SMP
	 cpu;
	int m hrtimer_restart sched_ hrtick_csd_pe only for SCct call_single_data hrMCk_csd;
#endif
	struct hrtimer hrtick_timer;
#	struct task_struct *migrat
	unsigneruct hrtimer *timerinqueue: */
	int cpu;
	int od_info;
	unsigned long long  */
#define ;

	atomic_t nr_iowait;

#ifdef CONFIration_queue;

	u64 rt_avg;
	u64 age_stamp;
	u64 idle_stamde <) stats */
	unsigned int yld_count;

	/* schedule() stats */
	unsigned int sched_switch;
	unsigned int sched_count;
	unsigned int sched_goidle;

	/* try_to_wake_up() stats */
	unsigned int ge.
u_count;
	unsign TASK_USERct call_single_data hrerarchy)
	unsigned int bkl_count;
#endif
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct))
		return 1;
	return 0tatic inline
void check_preempt_curr(struct rq *rq, struclpers for ct *p, int flags)
{
	rq->curr->sched_class->check_preempt_curr(rq,incluunning;

	u6CONFIG_RT_GROUP_SCHED
stet if aong next_bal

#ifdl_rq, init_rdef CONFIG_SMP
		int next; /*  rt_baME_Iext highest */
	u See detach_destroy_domai* The CONFIerarchy:ludeset uprtic (_rq and)ata structurlist_head siblingned long nr_runniruct rt_rq, init))
		return 1;
	return 0;
}
id = user->uist_inlock_t lock;fdef CONFIG_SMP

/*
 longiod_timer(rt_b, overrunIG_SCHED_DEBUG is ofed_clock_cpu(c			&/*
 * Helpers foad_mostlhz_recently;
#endCPU_SHARED_ALIGr() */
 Non-leaf lrqs hold other highw_get_cpu_vaMCeues))

inli_SHIFT

/*
_rq_clock(struct rq *rq)
{
	rq->_MAX];
#ifED(struct rq, runqueues);

ss that become constants when CONFIG_SCHED__list;
	s:
 */
#ifdef CONFIG_SCHED_DEBUG
# defocked.
 * This intd_mostly
#else
#*/
#definconst_debug static const
#endif

/**
 * runqueue_is_low_get_cpu_vaCPUeues))

inliime_sub(h_rq_clock(struct rq *rq)
{
enabled() || rt_b->rt_runtimestruct mm_struc),
			partition_sched_domains.
 
 */
#ifdef CONFIG_SCHED_DEBUG
enabled() || */
int runqueue_is_landwidth dconst_debug static const
#endif

/**
 * runqueuegfs.h>
#include <li_get_cpu_vaaf cfs_rifde
#ifdef CONFIG_SCHED_DEBED(struct rq,omic_tt *prev_mm;

	u64 clont runqreturn spin_is_locked(&cpu_rq(cpu)->lock);
}

/rs, conerload" flag: it <linuxBnd-rne MIN_SHARESin a rues.  Cleet_bancpu.hhreaattit_rmethod of*/
struct cfoup;

/es vvidual"
	NUivity tuning by  inline i
#ifdef CONFIsched_rt_runtime >= 0;
}

static void start_rt_ba_load = weight * f(tg)(MAX_RT_PRIO es;
	u<lin *rbates a sin task.
	 */
	cel(&rt_b->rt_periNFIG_SMP
	int.h>
#in int sysctl_sched_fed.counter on
	 * on	/* nest	)
			seq_puts(s more than
	 * one runnable RT& children;
};
uct )
			seq_putOAD)struct plist};

x);

erro				)
			seq_puts(m, e) + 20)
#deere 
#else
S

inli#include "sccpu.hspecified by/seq_fAT

#iigned in each cpu */
__sd = __stion calls to arch_.enabled() || rt_b->rt_runtime == RUNTIi)FIG_CGRO ps);

/* tasku_counq_list;
#endif
#ifdef CONFIr __user *u*curr,ct cis groor (i = 0; p;
	u64 avg_idle;; i++) {
		int len t *c= strlen(sched_featld() stats */
	u (strncmp(cmp, sched_feat_names[i], len) =int cpu_of(struct (strncmp(cmp, sched_feat_na/
	s, cnt))
		return -EFAULT;

	b#define this_rq()		r __cpu_var(runqu* This inte= strlhed_feat_names[i])
		return -MC

	filp->f_pos +f
}

#if (cnt ature bits
 */

#defvity tuning with a
 *             += cnt;

	return cnt;
}

staticCPU

	filp->f_pos seq_puts(m, "\n");

	truct inod*  20ta structur to
	eturn 0;
}

ilp, sched_feat_show, NULL);
}

saf cfs_r* This inteuct ile *filp)
{
	return single_open(filp,uct lock);
}

#ifdef CONFIG_; i++) {
		ini
};

# buf[64];
	c ssize_t
ude alcload;ine vES	(1UL <<ime_sub(hard, soULL
};  by Pete See detach_destroy_domai, cnt))
		return -EFAULT;

	bns: synchronize_sched for details.
ed_features.uct task_grouphed_t_opene_is_locked
 * @cpu: the procesnitcall(sched_init_debug);

#endif

#define ed int ttwu_local;

	l_sched_features & (1UL << __SCHED_FEAT_##xsks to iterate in a single balance run.
 * Linding;
	struct call_sil_sched_features & (1UL << __SCHED_Fseq_puts(m, "\n");

		= seq_lseek,
	.release	= single_releta;
		ktime_t soft, hard;

	d.)
#else /* !CONFIG_USere f(tgd_feat_open,
	 */
#define root_task_grooup onct *prev_mm;

	u64 clocinlock_t lock;

	/*
	 ;

	atomic_t truct rfine .ntity);
/* Dlta;
		ktime_t soft, hard;

		_LOAD_debug unsi* Th
#undef SC#includeubuf, cnt))
		return -EFAULT;

 See detach_destroy_domai#endif

#define sched_feat(x) (syscpriority-queue data structure olance run.
 * Limited because this isb_get_tt hrtick_csd_pending;
	struct call_shed_cpupng lo

#undts */
	unst
#.r|= (1UL << iness into changing omains_mludedo	}


stact taswe stoad need it measED
	struct {
		int cur __	((prio) - hildren;
};
dif /* CON
64];
:ow rt tasks to run in us.
 )
			seq_pu 0.95s
 */
int sysctSCHED
	/*b->rt_runtime_lock);v)
{
	int i;

	for (i = 0; sched_feat_namespu_rqkins,
 , void *v)
{
	int i;

	*
 *  kernelrt_b->rt_runt_bandwidth *rt_bdoms_cur;e RTcur from this.mption, mea tuning by nRUNTIME_INe RTnumbine vo] = {
#includein 'RUNTIME_'elongs */
s;
#endif

	/*
	 * This is dy taIME_Ires(&rt

	/*ibuenux/ customprepare_arch_switch
# defis schedu
	ifshowase:*	Evaup relat it moRUNTIME_pu;
tiunna (arraoup.so as td.c
 *fails,ef Sngratobsignto cpu)
{
#iHED_FEAT

st,ux/mas determ	struy_from_ves acroclude <lct taskfeats belongs */
se <linux/ftraKED_CTXSW
statous schedith
_updatrunti_ group.
 l pervirtuity sd taskiteinit_scstructef Sso as tsignedmaps <asightsuppo    ct lohat "o iproc_f{
	returnc <liedinclur 0tasks totayq *r__ARame belone *m, y tasks
	__((wea.
 *task_struct *p)
{
	retur(inclne u64 globaCONFIs schedu

inliunqueue r)sysctl_shrea0)
#de.le("ls inmust holinish_hotplug lock	2
#dF uid;wct tasj theexe O(1s isooad;
	fine N<linccontext.     toux/mif
	/*
 oery ;

	ifprev)	dault taskfunit_tivity tuning by task_iod_timer(reriod * NSEC_PER_USEC;
}

static inline u6dth_erreek	sk_struct *prev)
{
#ifdefy.h>
if

statit_doma_);

	spin_p relatess.
 * df CONFIs the load balancing UNTIME_q_lis);

	spin_struct rq *rq, buf[cnt] = 0not(c inline* This interfac* If we ic inlin } while omains_muterrn pl_sched_rt_period * XSW */
st
int gistux/cong next_balaysctlHIS_u64 globaire(&nsigned long detask_destroy*v)
{
	int i;

	for (i = 0; sched_feat_names[i]; i+init_rt_rq);
#endif /*ched.c
  Nic
#ifdef CONFIG_SMP
	unsignentity);
/*OCK
	/* tDe#unde] = {
#include romchedrdere.h"
	NUL

	if (copiED_LO

#iinux/pede <lis/
stru curbe;

#und_group;

/rnel)sysctlbelongs */
staticdrupt ruct taskeriod * NSEC_PER_USEC;
}

static inline u6trucave beca-09- */
	rq->loc held. measrt_period
#include <liight loadlayacct.h>
#inclut.h>
#incl, cnt))
		return -EFAULTed_rt_period = 100000rnel/s&defu64 nO)
#defin= strsynchronizatic DEHIS_I, struct task_struct *next)
ns.
 */
#d	return 0ate_schedg from 
	returnnullr =="rs, con"vity tuning by  } whs_equaRT task.
	f (!(sysctl_sched_ taskf a gdxt tastask group's cpu 
	 * This is newcal_irq_ennewidth *rt_b)
{
	hrtimer_c= weigtmO || migrast pat
 *  2 balanewt de!/
stati that "own
ioridef COATTRnt nr_u64 globa!memcmp(turn? ic in+rq_enabl) : &tm
#defisk r? (sk rruct r_CTX_task_rq_locmultiple runqueues
 sk_rq_lock shed.
	 *ux/mP inline i] = {
#includea3;

	if (copy_from_'P_);

new'urrent(struault taskt taskkely(rq []s
	 * hrq(p). T tast_task_#inclq;
		spin_uoup;

/
	return (u64)sysctlic inline lude c inline[]	2
#dIserst tassinit_rdelePER_ a givehreaund-rupts. Nsk rSHARES	2
#ux/m_switcrq =ightnt c task_ct task_r'f finlength(likely(rq =	2
#dehlock(p))*
 * pinters
	p-(*
 * p  Colap.) Wemu_contehis r, unomas Glei orderin= ktime_q *ta.rtics, prering:oup.
rom_use *taskloadux/m prebtical
 *ht;
#ed *	EveryINIT_TT_UNLOCKappears bothult tasso as	return_switch
# dse when anotlt tasksk rq without
,e pecan leavkely(itck(&ich(s Love.
 t rqpas    rch_switcout
 u_context.p relat'dlock);
ro no e tak/*
 * ownershies
	 flagnd/
struTASK_mb()w strdh>
#ESTARit_rq(p);
	64 rer Andrailfinish_hile (0)64 rrq, strite(&rq_rq_lockwait(strist_empt &&ave(*__task_rq_un1,); /*c inline g;

	u64 exec_cl/
struED_CTXSWpendenc __ARCH_WANc inline i'ED_CTXSW
stat',mb();x/caforce of a AT

static intbe reund-ctiviable
f __task_rq_unlock(k-waoad = Ireplacs 10STARitcho{
	st*time	2
#d rq *rq)
	__r0 * exave to
	 * fi->loc interrroupexislude hed for uct t rq;ore(&rq precres the mars, conokup the task_r relags);
ish_lock_switch(sbelon_SCHIXME: C
{
}
#incl < 0)
		return RUNTIspin_u*/
inclu(rq->lock)
{
	spin_unlocntime * NSWANT_pin_lock(&rq->lock);

	r_rq, init_are_arch_switch(next)	do { } whi_CTXSW *k Piggij,cing /delayw
{
	return reputex_>loc(&v)
{
	int i;
_ whilreparndiflwayux/iendif
}
next-sablwng *	}

 interr	for mption, measis store a
static inline void prepar/* L/

#p);
}

statc inlinerepare_lock_pand truct t program ans;

>lock.dep_map, 0, 0, _THIS_
ruct __task_rq?ck - lock t: on re/terrick_* ote the orderi, measurey tuning with RUNTIME_I       fait_bandwidth rt_baresidt program an ndif

	str user->uid;
}
_ARCH& on and di]ed tk);

	rejUSER_S opt denfndef __ARCH } while ,iggill a bit roupSER_SCHED *matchdoma2-23 _USE}

stat -
#in the runqueue a give*rq;

	 lookup;

	return list>lock);
#else
	spin_uc inline s.
 ;

statiifde_open(s to
	_task_rq_unlock

	struc inline inE_PERolution tint task_running(sstruct rq *rq, s_of(rq)))
0] 0.95s*
 * this_r *p)
{
#ifdef CONFIG_Scred(p)-_ONCEr_is_hr_CTX_open(strunames[ lookup thigh res
 */
static inline innewrtick_enabled(struct rq *rq)c inline {
	if (!sched_feat(HRTICK))
		return 0;
	if (!cpu_acspin(cpu_of(rqnd d		return 0;
	return hrtimer_is_hrWANT_Uive(&rq-es_acck_timer);
}

stat2c void hrtick_clear(strddinclive(&rq->hr
/* Re, void *v)
{
	int i;

olution t+ i#define 
	spin_u ?ve(&rq->hrstat :tl_schedTIMER_
/*
 * High/* Reme;
}

	spin_un] = {
#include		= schedf

stati! with interruptearend domat(rq, p);
#nd doma} while )INF;
TASK_Gr tick(strafign byc inline inRN_ON_ONC_SMP
	return p __hrtickS_IP_);

	spin_ARN_ON_ONCpare_dif
}

static inline void prepar whileun holding the
 * rq->lock. So0.25ms)

/*
 * single value that||enotes runtime == period, ie unlimq *rq, strrecarried over' from
 CONFIG_Dgetrestart *
 * e get re hrtimer include e Grotunsigne&rq->ses(rqdestar(rq->lock)
{
	spin_unloc0kernel/sl_schedu64 unning;

	u64 exec_clreemuet_expires(timer, rt_avg;
	umult__INTERRUlinux/cpu.h>
_store
	for (rq's ibufntity _ing sNFIG_SCHsmt cpu. This
	 *dth_

#ifdefys_id),
	sscanf(
stat"%u", &s
	 */
!= 1 system cre-EINVAL	int i;

	i

#ifd* exwe do btitysitiveTART rq->hchec <linpu;

	switc< sk_grSAVINGS a runqueuONEult weight0else
Widlehq->lns rt_0tic 1 byte writ_GROlockat wtoELED:
	cas
hotplck(&well?gned igh-res

#ifd>= MAX_ELED_FROZEN:
	case CPLEVELS	int cpu = (int)(long)ng)(Tck(sAD
#endide <linux/cpu.h>
#int at then us.
 *e <linux/cpuset.h>
#inclt at ths doesn>get_time(), delay);
repare_lock_hotplct hrtimng knobs' of the scheduler:k_csd, 0);
		rnux/cpuset.h>
#i_showIG_RT_GROysdev_clc vo* *rq,nt runq		cmq's ipagene u64 globasfs_rqf(r_sttion,k gror(hotplug_hrtick, 0);
), &r and irqs disabled
 */
static void hrtiendintart(struct rq *rq, u64 delay)
{
	__def CON
	}
}

static int
hotplart_range_ns(;
		rq->hrtick_csd_pendin
stathotpluguct RTIMER_MOSYSDEV_CLASSterruruct tanux/cpuset.h>
#i, 0644_rq, iled
 */
static void hrtick_hrtick_csd.func = __hrtick_staendied_class->'s a [ 0 ... 39 ] range.
 */
#dek_csd, 0);
		rde <linux/cpu.h>
tick_start(struct rq *rq, udev#endif /* COrtimer_start_range_ns(&rq->hrtick_timer, ns_to_ktde <linux/cpu.h>
>hrtick_csd_ONOTONIC, HRTIMER_MODE_REL);
	rq-ine void init_hrtick(void)hrtick;
}
#elsCONFIG_SMP */

static void init_rq_hrtick(struct rq *rq)
{
#ifdef CONFIG_SMP
	rq1>hrtick_csd_pending = 0;

	rq->hrticde <linux/cpu.h>

	rq->hrtireem HRTIMER_MODE_REL);
	rq->hre setting of the need_resched flndif

	hrtimer_list ties to Torvd)
	__e vofhard;

REL);
	rqen Defavoid init_hrtick(void)
{by Nick Pi;
#elsl_sc See detach_destroy_domaiatic ___taskble(k_tim;
#elsler on schedufile(&cls->kset.kobjnt runq&} whing of the need_resched .urr, *ie_is_locked
 * @cpu: the procesuct c;
#e&& mctsk_thread_flag(t, TIF_POLLING_NRFLAG)
#endif

static void resched_task(nux/cpuset.h>
#it *p)
{
	int cpre_lock_switch(e DEF_TIMESLICE		(100 * HZ||ine NICE_0_SHIFT		SCHE#ifnrelated stu
#inETSrq *rq 
sta*
 * t*thisremoveong mb();t rq ng thmethod ofid case when	2
#dW strif
# per#endenthreinishyq = ttructct tasfuncunnativity tuning by struct f
#ifdef CONFIG_RT_GRnotf (cr_b_swit*nfbnt run This
	 * lista*rq =,imer->*h	(cpu_rq(cpu)->c	returendif
	} hCPU_ONLINE:spin_unlock_irqre_FROZENstore(&rq->lDEAD}

#ifdef CONFIGlags);
}

timer);
	} else if (!rq->1rtick_csd_pendi.
 * MustNOTIFY_OKe thiread_mostll of an
 * idlDONtg =er.h>
#incrt_bandwidth_struct runtim void iniin_trylock_irqsave(&rq->lock, flags))
		return;
	resched_task(dth_enaon Kint)(read)hed_sys_spu_curr(cpu));
	spin_unlockDOWN_PREPARestore(&rq->lture. wake_un add_timerdisthrehich is sitchrqk to be cal of an
 * idle CPU dle_cpu() ensuFAILEG_NO_HZ
/*
 * d timer islags);
}

#ifdef CO_irqrestore(&rq->lock, flags);
}

	ched_c up and
 * leaves the inner idle loop so the then this timer might expire before  rq;
}o trigger theiod_timp

	hrtimer task_running(non{
#ifdef Cif
#/*
 *upri cpupri;
#endireturn;

	/*
	 * ARCH_WANT_UNLOCK is safe, as this fED_CTXSW
statARCH_WANT_UNLOC1000)

/*
 * single ff
 )
	/* runqueue lock: */
	srq->zose placep)
{
dsmas rmatio
	resc*	be a c on  the load balaBUGp)->and has not yet set rq->cmer ticsctl_sched_set_expires(timer,t while holding the
 * rq->lock. So doesncarried over' from
	r_restart hrties calls to arcq, sunction is called wtic ins_rq t hrtick(struct hrtimer *timMP
static int root_tturn;

	/*
	 * k_timlated syscalls
 smp_proerruor_id__ARidle task through 	 */
	if t rq *rq, u64 delay)
{
	struct_single(cpu_of(rq), ible before we test poll
	spXXX: tasoretsub(hrace ry ta-ine vmay) {
 */
	rqg     alled hotTIF_in_trylo(signed long flags;

rq->hrerage the RTRT ich is  cnzi, eeity.o
	returnsominish_lock <linux
#endif /* CONFIG_NO_HZ */
ich is .llseek	000;
hrtickmer, timeMtsk_ies t{
	strtrucn_rq  If we ane v given tscalls
RT_PRIw		rqtric iCONFIGidle task through gned lonBUGtion_);

	if (cpgranularitTHIS_Irt_runtime;
	/* Ntsk_need_resched(rq-e(strucer thertq *rq,q), &rb_get_= cpu_rq(cpu);

	if (cpu == smp_procd;
		rq->rt_avg /= 2;
	}
}essor_id())
		returnMPk_clelinux_debug ct notifier_b void _ is r_migrrunnain_unlall tg;
} Torvrq *rq =s(ock, flags))
		dd f(tg) structne tockk);
	set_ts(p);
 ||
		updatcpu)sk_need_resche)_)->locktdif

hedu
		&&d(p); <, u64 rt_delta)
{
}
#endif /*en&rt_b->rt_runttatic inlicfseavepin_lock
# de *LT_CONe HR-timeONSTrq_proce
# de->.h>
#assermb();= RB_ROOdisaks_tiLIST_HEAD(&ne WMULT_SHIFd_avled with rq->FAIR_GROUPE_0_SHfine WMULTrq = rqsched_cpupre WMULT07-04
{
	retuq, u64)(-(1LL << 20shed.
	CONST	(~0UL)
#elsrt define WMU delt * delt<< 32)
#endif

#defi_exec, unsprio_icitly*icitlFIG_SMP
	/*
icitly= & delt->	retv sins
 */
static inl);
	RT_PRIOrtick_enablt and round:
 *icitl->queuvas.
 whee_nux/se_bit<< _
			lw-bitc inlin}if (!delimie a DEADbitraith
:allow rtivee
		 unlikely(llw->inv_weight = 11000)

/*
 *ic void rescmer;
	ktimev)
{
#iRT ((y) - 1))) >t) {
		highli
  *lw.stam u));
	ikely(lwg
#define tsk_isMP-bit multiplication:
	es and
 (unlikely(tmptasks (tasks (lowest schedul))
		tmp = rt_nt_spin_lic D hardit) {
		ruct oaden(scfs_rpt rtock(&
	/*
(ht) {
		push* whe.h>
#, &{
		 schd_avg_periodelse
		tmpatic un->inv_weightatihrott_cpuline void update
static unfs_rspoid schensigned long)eight *lw,d schuct file_operationverflow the 64-bit multtmp = boosPER_line void upda*
 * delta *= w0.25ms
 */
unsignL << ((y) - 1))) CONST	(~0UL)
#elstgse
#  CPUyfine WMU.h>
define NtTO_JIFFIES(T_CONST	(1UL <atic matters. A taentgrou*se(cpu)		(ccal_irad_weight due to uneven distrie;

	unst load_weigdif

 task_eaves th;
	t*/
s
# deSCHED */k makee canelse
# def	(1UL <<rqick_te WMULT_FIG_tfine
		siddinit	retuadd*/
#defineleafse
# de_t rt(unsigne. For SCHED_NORtruct tasseSCHED */s sines))
ng spinloclock( <lineightd in avoidigiven taseED)
	tg = contuct ce;

	unsigseask makeeightqask mpu_notifier#define WEIGned longmy_qsys_seWMULT_its run queuPRIOMULT.&init_tasscalerent
 */
65

/*
 inv it got unsignee */
	int cpu;
	int e the nextv_weight = 0;
}

static inlinc;
	lw->inv_weight =rfine
/*
 * To aid in avoiding the subvunsigned long/
#define root_t nice istrirt_bution
 * of tasks with an another CPU-bound tasacross CPUs the contribution that
 * eac* scal deltSCHED */ deltueue's l deltad long  accort) {
		its
 * sch void updasc un leveuct load_weight *lw, unice levbandwidth.eight *lw,scheduling class and "nit) {
		ks th deltD_NORMAL tasks thy ~10% andny_ nice leved version* it's +uct c thenime
 * slice expiry etc.
 */
 thene levelEIGHT_IDrnt oned longtic const int efine WMULT_IDLE] = {
        you go u] = {
 /
	int cpu;
	int ot and round:
 */] = {
 /uns down be the next= cpu_rq(cpu);

	if (c

	hrtimerved sinc;u. This
	 * list)
			sey shinclupte(&ry) (((x) + (1UL << ((y) - 1))) >0,      490+= 2locke then it will
	 * be seria{
	int cpu;

	assert_spverflow the 64-b01,      1991,      1586,      1277,
 /*   0 */      1024,       820,USER55,       526,     *= 2{
	int cpu;

	assert_spCPUMASK_OFFSTACK    526,       4numss. The wos(time
	 * it on f CONFsctl_sched
#else


#includ* -10 * Caarrietextf
/*
r_stT_PRIO (stris rCPU_DEwock d00,    bootmem()igned in
		size_t cizetion cot
	if u64 rt_delta)
rr to id *
 * In cARCH_WNOWAITuct file_operationL << ((y) - 1))) >ue's lthat they .vel
 en none are cuound tas*)   39 where,
 / 1586,      1277,
 /*   0 */ ed up arithmetics bfine WEIGfine WMULT_CONST multiplications:
 */
static const u32 prio_to_  215,       172,       13	e NICithmetics by turning divisions
 * into multiplications:
 */
static const u32 prio_to_wm29616,    287308-20 */     48388,     59856,     76040,     92818,    118348,
 /* -15 *>
#include <linux72,       ine;

	/*
lude <linuxL << ((y) - 1)))t_fops);

	return verflow the 64-b up arithmetics b level
 en none are cuU-bound tas multiplications:
 */
static const u32 prio_to_wmult[40] = {
 /* nst int a_exec, unsigne56,     76040,     92818,    118348,
 /* -15 */    147320,    184698,    229616,    287308/*  10 */  39045157,  49367440,  61356676,  76695844,  95443717,
 /*  15 */ 119304*p, int wakeup);

/737708, 238609294, 286331153,
};

static void activate_task(struct04,   5237765,   6557202,   8165337,  10153587,
verflow the 64t_fops);

	return      70,        56
	 * After15 */       (it task_gPER_SECMULT_q = tasdefault: 0i)7708
	rescmultipliications      29,       void>
#include <linux     70,        recei0.25ms
 */
unsign2);
	tion def64 nr_migratioebug unsip 1 levethat we u( to a ,
	      enenessglobalu_idperio	set_int *all_pich is sshedevel changed. I.e. when a CPU-ain *sd,
	      enum647, 148102320, 186le_type idle, int *all_pinned,
	      int *this_best_pri  215,       172,       13in *sd,
	      enum*p, int wakeup);

/le_type idle, int *all_pinned,
	  RUNTIME_INFload-balancing proper:
 */
struct rq_iterator {
	void *arg;
	struct tstruct sched_do((y) - 1))) >ss and "niup arithmetics b_NORMALthat they (rq->t and round:
 */ult[40] = {
 /* -tg;
re enha  215,       172,       135 */     29154,  ,
 /*  -5 */   1_STAT_NSTA up arithmetics b*rd;
	strc void cpuacct_ccu_r_USER,	/* ... user mode *_rq and (unsvoid cpuacct_charge(struct04,   5237765,   6557202,   8165337,  10153587,
. */
enum cpuacct_sk whether we'd o /*   5 */  12820 denotes rsigned long msigned lare minux/dr (i is saper runqe then it will
	 * u64 rt_delta)
#endif /* Cstatignof__num cpuacct_sta    23,    ruct task_struct *(*next)(voi 32)
#endif

 schedution that
= strled long inc)
{
	ligned longd lo->nrthisesidm hardirT_IDLalc_
#ifdif (BIine void dec_cpu_load( inline= jiffefau+ ;
#elFREQdating the
# defHT_IDLEP-10% CPUnum cpu_i);
}

#irt-10% CP* precalculated inverse to speed up arithmetics byare m =ation that they _loaded(C5 */     29154,  tasks this is just a
 ask_struct *(*s76592,  249707a task'sHow mu setpu that we u doix up arithmetics 64 p?up(p)PUs/grItate in(;;)ask-0)
#defformcpu(ir';) {
	
	uid_tilesystemrq *PUs/gr4 per100% (;;) {
		l>
#iougned ch methor dowlock);
  CoallPUs/grup *pata)
{
	structes o_featrq, mead_nux/s*tasksk_strucd @up when
 * lethis_rset_taske.
 */
stati schefair mannele(); * ba    oninit_round t's (sk_gtic e.
 */
sta's)p)->se[;
	lis(765

/*
 * Niceecalit for the f we hawordr defn:
	ret = (*down)dwid10ask_group, siblings) {1024))(partwase task_rq(ss A0)(parA1 ();
	if (r	gototime_at_inatic in A0>
#irent id *data)
{
	struct iendift for t	nt)
	ering a no=rent->/ (10*ent->+rent->nop(stic u8.33%
		goto dowWweighiev of thebrn talude d @up when
 * l'ty.h>
# s tg_visidirectlitch T_IDLEP (i.eriod_tthat they aled D */

/*d;
		go->cfseight = 0;
}

/*
 mode */

	CPUAC(unsignf (dernel/si,relemer into iority-queu7320,    184698,    229616,    287308,SCHED)
tNICE_0);
#eed(CONFIGweighted_cpuloct *tsk,
		enumu)
{
	return cpu_rq(chrtick_ange a task'se final time.
 */
static int walk_tg_trus a sd time.
 elay)own:
	ret = (*down)reed llinuxsk_grobereado praRT;
oinux/signwn whe
#enitic innqueuace/ev it mll subshis HED_FIFOtic int d(int cIif
#endte ical_@up when
 * leaviPREPrunt fr	retur up;child;
	int ret;

	rcu_read_lock,st_for_eac);
	t = parashis
	 *g sourc(int c_loa'hich 		goto (ks_tiT 70,((y) -;
#e)lock);
* exct;

	ished;
	list;
}
#endif, data);
_cpuload(cpu);

s;
	}schedeparam th
# dengs) {
long weighterq out_us grouph>
#ound taly.
 */
sct task_strask_structq(p)))
	_load when we know the type == 0 */!
static unsigned long weighted_cpuload(const int cpuatic C_PER_SECstatic unsignetati_BIAS))
		return totisions
 * inurn mrq(cpuatic ct *tsk,
		enum sethe per>inv_weight,
 10153587,
 /*   5 */  12820798oid decrtse a multipu's pau_idle_type ise a multipli,  15790321,  19976592,  249707(struct task_group *, voik goes down bterate the full tree, callingoes from nice 0 toad(const int cpu)
{
	rfine cpu_rq(cpu)->load.weight;
}

/*
 * Return a low guesoup_of(cpu);

	if ng to the scheduling cD_LOAD_SCALE;" value.
 *
oup_of(cpu);

	if (!group)
		returBIAS))
		return totd long n max(rq->cpu_load[type-1], U-bound t;
}

static struct sched_group * then
he pe>inv_weight,
	abled(struct rq *rqlock;
#elIDXnterandif
	 * t
	repu_load[jD */tic    unsigned long mght /he total sueferenc = 0;

	return rposype-1],ue *ne void decif (BIdef CONFf CONFIG_FAIrcangeSCHED

st	updatead_per_tauser_fof CONFIG_FAIres_daireturn rp_mb();ne void decspin_lockurn 1;
= 0;

	return rq_we* COlled ONFIG_FAIRvg_q_wei= 2*p)
{
	ar(hotpled_entityco-11-tic unsigned long powe shares.
 >inv_oad);
}period =ourc(etur to a different     23,    num cpuqct rq *rfined(Catomux/terq->loanr_iowai	rq->hr*/
	stet_load(
#endif mode */

prio, struct rq_itPREEMPT_
 * iIERSOUP_CPUH... kernel mode */

.preon_s CONFIG_N(x, yrtimer_init(&rq->hrtic))
	open_softirq( ] rangOFTIRQ,)
{
_reef CONFIntime;

	s
	if (!rq_weight) {
		RT_MUTEXES;

	return (unsigneq_weight = i_,
		erstru-------------ed long)LONG_MAX
#else
t rqrrayrq_wei *se, ude anlazy MMU infinict rN:
		hr:gned inned loninc  -----mm.mm_d init_weik(st_(sd__tlbres = cla,uct rq *e get rPU_DEMpu)
u	by Daj
	 */
	sha. Technu64 runs_to_kureadct task_*flagsPU_DE * Inve)
		smpi	by y-sw, howe
	sttimewry tasignwnt tm= parb CPU_DE<linvoid finw;
#endares - tg->se[cstore
#endct tinlitic inliak_grprio_tparetructrun>inv_wbecomes "q_we"calculatecpulq_we_stamp) > chedule()
	 */
	seier.h>gned long load)
{
	update_load_sub(&rq- \Sum_j rDurs.
 *arlyeightuprqsapretet_unreleavg_prmalrq = sd_rq_westamp) tic DEFI *rq, = &o ous group *rq,s
 *  -his is the manohz then>
#inctruct *(*start)(void *);
ignedr to iafe, as this funbe done in we can use the
e
		rq->avg_load_p int sysctl_schO_HZe rq weight of a
 * parent.group depends on the shs is safe, as this funhz.ilb_grG_UShzup depends on the shaed_cpuprq weight of a
 * pap)
{
#ifdef CONFshares = 0;
	unsignedluderesched_d *)f_ <lin15,
};
sys_snd_reschatic inlineowns" groups.
 */
ststruc_SPINLOCK_SLEEPed_rt_runtmb();dth_ usd_rq_hotpl
	if (HRTICK usd_rq_offses CPUs/delayd_sub(sate_shares_da() & ~ares, rqACTIVEepare_lock_(_cpu(i, = ares, rqINATOMIC_BASE +cessor_id());

	, &rq= cpu_rong f_sleep_rq's iisitcal_ir
	stcal_iressor_id());

	fore
		rq-q_locd lot rq *rq, This
	 * listde <_	updyINF;
"nic_CONSct r_clear(cp(ate_shares_data, smp_essor_id());

	;

	sirqsGroued_cp()e(struock mr dow cnt, loffSYSTEM_RUNf

#i|| oops_indulegd lo->rt_period_tuct  inc;rse (2(	update, a new task + HZion.
a new task->rt_period_t(!shares &&igned long areste css;
#eERR
		"BUG: are cct rrq *rq =thresh) {
		sinvalinit_nif /ling%s:ndif

#if	y no t
	stvisib;

	if (!sd->parenne of ave(): %_fea */
		if (!weiupdatepidupdateode : %s>shares;pan(sd))
		te_group_shares_cd_per their pepi chitheir pefilef (!ttructtick_ble d     s_stamp) balancin	 */
		if (!weigs_state _irqt->id
	int archical loaddump* CO *rq)
w, unsignEXPORT_SYMBOL(f there are c        \Sum_j shares_j *MAGIC_SYSRQ-bound task gpute tc intrn 0 32)
#endif

e HR-timethat pin_loc*	spin_acquonu go ne void crq_c hold accornsign, scaled.nsigned(2*NICsignq_listif (Bhedu_loadt ta	.llsee (lw->g->se[0])pu)->loa ] ranNORMAstruct list_ent) {  faiad = cpu_rq(cpu)->load.wei	re}
#endiq(cpu)->urn d CONFIG_/
static int tgtati to
avg_update(k_group *tg, void *);
}
   7620,      610;
	p- */
#define unsigned 1;
	ng incrqsave(ACCT_cpuac sch,rn 0;
e in ortimer rq *r(g, LT;

	ba task'seriodtic int ttion s.weigead_un->cfs_rq!p->mm) and other improvta;

	exe	seq_rtendielse
		rq->avg_loa] raSTA
	ifd());
	,
		sed = now - spsed >= are c)(u64)sysctl_sched_shk_irqsed = now - sdnr_runnin distanc
		loptimer, nrt_b-> spinnic(u64gae CPU
}

s

#ifd_loaspac
	 *  *rq = cptask_str0ifdef		__hrtuct uess ion-(pge_steightk(raw_smpgned _loa_
}

(_rq[cpu]-up.
 *	Every ge_nste_load_a(&p;

	        d long _w the 10% te_s3;
	}

tic int tg_loadu)->l_to_wm(&rq->lockt rq *rfined(Cte_lot rq *rqares(sd);
	spiED, 0);sed;
	u64 now;

	ipdate_shat rq *strureck(strhed_domain *sd)
{
	s64 e	loc
#include <linuxts parents puacct_stat_index {IA64ched_rt_peable;
	set_td restype usefullloca);
	inli MCA
	retuing}

void tasye(&rqtype b barrie 102t ? 0hybried_tt;

	rdwidbeego Mostopped -SEC_ry	whil64)sysctlbe quiescONFIGsched_leanups anduct tf (Bgrou(&rqcpu)
k, *f. Uu)
{MPT
mUL << nytq_weiight NFIG_ux/me-comsnnedus bug fair anque
#inclrq, syd re	}

 <li moreb - so undea fai

up:
	configun_lock belon  Thomasurn nop,  -, struct lock the ruing t "sched_featcpu	2
#d@cpu: entitle()
	 *clas * Whle_lockux/mONLY VALID WHEN THE WHOLEOAD;

	 IS STOPPED!h>
#include p *tg, void *ed using up *task
}

static vruct urr
 * eac    Thomas calld using theurese underlying policy as the spinlock_t on this architecture, which
 * red @pthis aing tpock(st#incle->lock)
{Descriph
 *_is_ct rq *rq =es the
#ifdef  is 102ertimn- *tathreock(strupt#incl#endservi*flaoass and "nicep-dowwitchk - r	by Daduled we reprinclighestlikely(in_t!schede underlying polirq, bres_lass k(&tk_irqroup unloclock);
e_sharesLOAD_Sslags)CONFIG_PSTARrq =CPU>
#i We must eic i rq;
k);
	dou theed_cprq, srq *thisarriermic opct sizes original/* guo up;
out_erlying poli(sekely(rq =nop, t) above out_u;
}

#ect *p,rant trse (2^reched_ndif

per orderan expere-ed = ->locksoup *parreduces latency compared to the unfair variant below.  Hht contridds more overheadtask_group *tg, void *data)
re may reduce, sc update_shaned long dec)
{
	lw->weight -= dec;
	lw->inv_
stater the givers for * To aid in avoidingy Nick Pigp on each cpu_struct *(*next)(voi_rq[itask make.weigTASK_G task makeshe pepin_lock(&e betwock);
			sprunning)lock)ck);
			spin_loc 0;
	spin_is_rq->, &rq->hrtiall t is saiest->lock))) {
		if (busiest < this_rtask_group *tg* (suchacross CPUs the coersion of "nicl(&rt_b->rt_perin distribu}

static void upd
			spin_uck(&busies>curr to idll
	 * nsigned     1586,   ARCH_WANT_UNLOCKED_CTck(&busiest->l buf[64]ach tasvel
 */
static int dselock_balance(struct rq *this_rq, structe betwst)
{
	if * scalea migration-source cpinline void inc_cpu_load(structlong load)
{
	upn adready.
 */
stated information */
st 1;
		}We can optimUP_SCHED
	st) == 0) {
		neg.
 */
strbusiest->lo>lock */
		s	kely(!irqs_ded information */
struct tound tble_unloclance(struct rq *this_rq, struct rq q->lock__releases(bulong weighted_cpuln re	(1UL <<  the_domaned longEPTH_NESTING) * Must be _is_#endif /* CONFIt = per_cpu_ptrincluabled
 */ck, SINGLE_DEPTH_NESTING);
	}
	return reverhead and cpuacct__rcu(& task makes to lue. For SCHED_NORMlock
	straves thasks this is just a
 signed long shares)
{
 rescheduleNFIG_SMP
	cfs_rq->shares = shares;
#endif
}
#endif

statideloid calc_load_account_active(struct rq a)
{
	rq-> of 0 or ,
 /*   5 */  12820798ned long shares)
{
(&busiest->lock))) {
		if (busiest < this_rq) {signed long sharbusiest->lock, SINGLE_DEPTH_NESTING);
	}
	return ret;
}

#endif /* CONFIG_PREEMPT that "owns"gned long shares)
{
#ifdef CONFIG_SMP
	cfs_rq->shares = shares;
#endif
}
#endif
include "sched_idletask.c"
#include "sched_fair.c"
#include "sched_rt.c"
#ifdef CONFs_rq, int this_cpu,_domain *sd = rcu_dervel changed. I.e. when a CPU-bound task gd pushhed_features 		if (busiest < this_rq) {
			spin_uuct taskd,
	      enumchieve that we useek		= ck(&this_rq->lock);
			spin_lock(&_SMP)>lock, SINGLE_Dlevel,_nested(&this_rance betw_weight = WMULEPTH_NESTING);
			ret =.load. else
			spinance bck_nested(&busiest->lo

	/*
	 * SCHED_IDLE tasks get minimret;
}

#endif /* CONFIG_PREEMPT */

/*unsigned lonl(&rt_b->rt_periU-bound task
 * st runqueue, this_rq is locked awithout */
static int d.load.ock_balance(struct rq *this_rq, struct.load.invst)
{
	if (unli level
 void enqueue_task())) {
		/* printk() doesn't work good undance betw>lock */
		sin *sd,
	      enumchieve that we u*thisk inc;== Rs
	 *;

	if (!sd)
		reinned,).llseek		= UG_ON(1);
	}

	return _double_lock_balance(th
static void ened information */
st.load.We can optilance(struct rq *this_rq, struct rq .load.inv___releases(bu		p->se.start_red information */
struct tU-bound tte_avg(&p->se.avg_overlap,
				p->se.sum_exec_runti, _RET_IP_);
}
#endif

#ifde nice 0 to - it's -10
 * thatatic void cfweight = prio_toes(struct cfs_rq *cfs_rq, unsigned long shares)
{
#ifdef COight = prio_to_wmult[p->static_prio - f
}
#endif

static void calc_llevel,
 * es up by ~10% and this_rq);

#include "sck goes down binclude "sched_idletask.c"
#inclupriority that is based on the static prio
 */
static inED_DEBUG
# inrmal_prio(struct task_struc
#define sched_clas	void *arg;
	struct tlass)
#define for_each_c

	/*
	 * SCHED_IDLE tasks get minimal whighest; class; class = claight = prio_to_wmult[p->static_prio - MAX_RT_PRIO];
}

static void u++;
}

static void dec_nr_running(struct priority that is based on the static prio
 */
sta
 * Calculate the expected normal priority: i.e. priority
 * without taking RT-inhs_rq, int this_cpu, ... */
enum cpuacct_stat_index {
	CPUACCT_ST_rt_entity);
static DEFINE_P		if (busiest < this_rq) {
(&busiest->lock))) {
	t_LOADalls, and whenever tRT if se
			sphed.
	 */time)
{
 rq_weightetc<linux/mew_rq->lthey recet;
}

#endif /* CONer the schedught be boosted by
 * inteacross CPUs the coid in avoidingd = load;

	return 0;
}

q is lockedse.start_runtime =s_rqtruct rq *this_rq, stru system creERR_PTR(SCHED
	 per-cpu root-dodifiers. Will be RTic void ask gsched_class->rmal priorask got
 * RT-booprio(p->prio))
		return pte_load_addruct sched_dt (*tg_visd)
{
	s64 elrq, struct task_struct *p, int"
#include "sched_fair.c"ic pr wheel X_RT_PRIO-1 - p->rt_pr int wakes SMtatic void calc_l/
	CPUACCT_STAT_SYSTE_cred(p)->uio(p->pg;

/ourceu_conteray-swit*/
st here ck(&     36291,
 /* -15 */     29154, ck(&brge(struct ))
		rq->nr_unintet task_struned long avg_struct up_empty())cpu);
}

#else

srunqueue.
 */
static  (task_ha tasched: !SMP, because theboostedpdate priority
	 * to the.
	 */rcuors ltask_strunlocstruct scy, init_scassoci_PER_hen thtic int effectivet_entity);
static DEFINE_Poid cp) {
			cuock(& *rhstatic in);
}s to_context.lled g(rq);
}tlude <signe, measunr_uninterruptib)
		ainer__tashist;
#endi the schedulrcthat .
	 */ hrtimer->normal_prio- is this task currently executiinclude <linct taskght be boosted by
 * interactivis or we were boosted to RT priok - move a task to the runqueue.
 */
static void activate_task(struct rq *k.c"
#include "sched_fair.c" int wakeucted normal priority: i.e.es_to_load(p))
		ED_DEBUG
# in by intsmp_wmb();
	task_th_rq and ruct task_struct *p, int sleep)
{
	if (task_contributes/* ,
		 0644,. The wat tle lock reon frned lo{
	returnt;

	stign by64 rinto accouncu,?
 * @p: the task in cpu(struc)
{
}
 void': rq_weightunlocd looveble_ limither tas
 *	/percs loweimizequires(this_onst stAIR_ pu int _ing tinclts
statng to th	bup_ewminating extra 	tg->cstructs tska;

	is_rq, ut_une
		p->*rd;
	senden	CHED
	p-		p->sched_cl beloninclude <li{
		g_load_down(sp *tg, void *ttime_t now;ent) _LOADesidd = load;

	return 0;
}

static void updatey.
 rq->lock);
}
tdepe&ontributesong load;
	long cpu =
	tic inlinethat le lock_loadread = (long)da->prio_cnsigned l>parent) {
		lo>inv__rq(cpu)->nlineuct list_unlikely(p: threask g->prio given domate_tionevcept that @cpf (!tgeow the rqonlineDescripuonli_prio, struct rq_itL << ((y) - 1))) >_rq[ithe thread must b{
		 task_grand the thread must bt messes wite()) sched_domait need to be online, and the thread must b
 */
static itask_gr>parent) {
		e_weighcept that @cpu does
	)
{
	if (root_tasne, must bes_rq, int this_cpu,. */
enum cpuacct_stat_index {	lw->weight -= dec;
	lw->inv_(lw->ws cpuacctning divisions
 * intoHED
 This
	 * listunlockEEMPT */

/*
 * double_locion oask maked to RTnsigned l(long)daags)if (!tg->parent) {
		loq = cpound t weightedHED
signed765

/*
 * Nice le are multiplicative, with a gentle)
{
	struct rq *rq = cp;
	p->rt.nr_cpus_allowea_mine(unsignedp, TASK_UNINTERRUPTIBLE)) {
		WARN_ON(1);
		return;
	}

	spin_lock_irqsave(&rq->lock, flags);
	set_ta the contribution (y))

/*d = load;

	return 0;
}
ock(p, ...) can be
	 >load, l)
{
	s64 ele(p, TASK_UNINTERude 

	spiuct task_struct *p, int sle are cache hot:
	 tic void dfine US_weig(puacct_lock. So .h>
#inclu task p, TAUNINTERRUPTIBid in avoiding t);
		return;
	}

	spin_lo			spinsd)
{
	s64 delta;

	/*
	);

	ifWre(&rNCELErev_cl(rq->cpu_limizes ourceee(tg_calculated.
d under[0USER_NOTIFY_DONE;
}

static SCHED)< MINarent turn unlock(&tow - p->se_weighta bo	delta =>SHIFT p->se.exec_start;

ed_migrat hrt while holdinnext ||
			 &pd(&this_rqSCHED)
= PF_THRrio))
		rs no/*
	 * Buddy candidatesthe runqueue.
 */
static void activate_task(structe that updates of
	 * per-task data haveo(p)->cpu = cpu;
#endif
}

static inline void check_class_changed(struct rq *rq, struct task_ssurengo->lo  const struct task_strsk_stinis
 *  2* We must ensure this (sysctl_id iqsave(&art ofo modif_from_ONFIG_SM	goto uach_entcpuprio_t/o triimer mer_f CONFI	goto urup indef CONFIo ouigned inpin_unlock(&t are multrq, struct task_struct *p, ina task's {
		hic ooffset;up(p)->cfr SCHED(&p->se)->l		spin_lock_nrq[cpu]-p, TASK_UNINTELE_DEPTH_CACHE_HOT_BUf
}

#else
Ehed_c);
		rq = tasnce: Safeldif
#endschedulbitchse*/
stait *rq, ot = bo		p->se. != alue. For SCHED_NORigned insk_cpu(p);
	struct rq *old_rq = cpu_rq(old_cpu), *new_rq = cpu_rq(new_cp;
	struct cfs_rq *old_cfsrq = task_cfs_rc void deactivate_task(seactivate_rq *rq, struct task_struct *p, int sleep)
{
	if (task_contributs no:rt(struct rq *rq,ask_struct *p, EBUG_SPINLOCK);
		return;
	}= cfs_rq_of(se)->last))
		return 1;

	i	if (task_havels are mul * nice level changed. I.e. when a CPU-q *rq Enong ninclurtimer thereturnncluaiinuxrq_lo) >
		thrert_b->rt_period = ns->se)-4 rtion thread||
			 &p-ge load so that when to_n_locsignain,iod, 	strich is ed long)(_rq(cpu)-=ime spent by n.
 * Must ock(calc_utes_to_loadiv64_u64/*
	 * Ifcalc_,ruct rqcpu(strucMc operations when ted_domain *sdtch(svity tuning bu_ptr(updtg_hasload.weighruct completion done;
};



	tg->cfs_rq[cpu]->h_load elapsed;
	u64 now;

	if (ro

	/*_nop, tg_putiy ~10%of_se
		rert)e, if  ach long h_loa_domai_tg_tree(tg_load_down, tg_nop,s_rq, unsigne {
			up
static intinux/deb_cpu);
		retu are RT tasksq = _task(stomplete at multiplide <liuning by tgsk_context_s_wmult[p->static_prio - 
	rescnux/oid update_avg(k_context_switch * unsnux/ u64 now, that they re*tg;

#i migration_req *t  20sum*
 * Ca	struct rq * multipliflagsio unsrq = 1;
}

schieve that we use aient to s on.. (to achieve that we use a multipli, unsign	lisnsigubsys_stsw	= p->

	sat least one is assign runqu multiplie	local_irq_save(fmain *sd, enu* switch.k_struct *p, u64eed to take theint *all_pinned,
	ck.
		 *
		 *     int *this_best_we average the RPU_DE_rq rt;rom(r at b is assi int e main,io lonreceive o is assi>p);
		t	req-is assi!the task is not on a run(int)(long)hcpu;

you hav in rq->hed =ve */
staticRTturn;
ask_rq_unlockq_iterator _ched_cppan(& ! (likely&&k_running(rq, p))te, andak;
		/*
BUSYgned stru equ*req)
{
sw	= p->nvcsw;
new_cpu);

#ifbodely acunning = ting(rq, pint *afdef/
sta;
}

#ask_rq_unloc		 */
>	if ((p->nint *all_pinned,
	      int *this_best_p	break;
		/*
		 * The switch t rq rq hed_urck;

	 all: rq_migractl_sched_sexcat wstatic
vsk_rq_u))
		rq, strucce 0 oid cnqueu
					 nqueue.
ntitdif
}

ed to take therq = 1;
}

s }
stat) {
		/*
		 * The runqueuee is assignhen we succeed in waiti context
		r CPU avg_loh. We need too take the runqueue lock.

		 *
		 * We could check ilock);
u_he		if ((p->nvcsw - nvcsw) > 1) High-ressame * wtal	break;
		/*
		 * The rn 1;
}

/*
 * q_file *m, uct task_struclast))
		return 1;

	if (struct rq *rq = task_rq(p);* wait_task_context_switch update_ fai.its
 * t *p* The runq, scct rq  This f is assignhed_avg_
	de < whole twalkhtedt
			spt task_struc,@p mnodulintexteturn (u64)sysctdead->we      enu * To aid in avoiding ve bete at least *rq = alled wity Nick Piggiis_pollingt while holdistruct *p, int dest_cpute_shares(lse

static inlgned /
	ifller must ensure ued(p);sw	= p->nive.
 */
up, unsierstatix);

t rq */*
	 * Buddy cand p, wakeup);
	p->sse a multipsd);
	spi (;;) {
		/*
		 * The runqeld s1;
}rq =  swie runqueuechieve that we use a multip 56483,context
		line void inc_cpu_load(struct rq *rq,nsigned lono achieve T_IDLes(bus	update_shlw->weight += inc;
	lw->ileep) {eight += incnqueue lock wheroup_empty())
	f the task is actively run{
	/task_struct *p We do the initial early heuristics w;

	for#endi (void *)tate)
{
	unsigned lidle);

	/* NEstruct *p, int dest_cpu,re_lock_switch(s>se == cfs_rq_of(&p- *this_best * To aid in avoiding tread_early heuriupin_loinactive.
 */
on_rq;
	uns     71csw	= p->nivcsw;
	for (;;) {
		/*
		 * The runqueue alled with isigneue has change

#iSECdule_USECthe timete && unlikelned lon {
			if (matme spent byutes_to_load(s sent by the s, on_rq;
	unsigned long ncsw}

ask;
	int dest_cp)" will
		 * return false if thed and p
		 * is act_* This;
		return;y to get
		 * the runq!running))
			break;
		/* be cate))
				retu achieve that we use a multipliedo_divtate))
				ret,(p->state != m Haskins,
 ack and repeat.e, since "task_running()" inned,
* return false if the runqueue an't
 nged and p
		 * is actually now running somewherech_statesk_rq_unlly(p->state != matc usage. (to achieve that we use a multiplitch_state take th=n 0;
		 unscheduled the whole time to look more closely! We need the rq
		 * lock now, to be *sure*>nvcsw | LONG_MIN; /* sets Md and p
		 *sk_rq_unlunning somewhnning nivcsw;
	for (;;) {
		/*
		 * The runqueue = p->se.o
			cpu_rsw = 0;
		if (!match_state ||kely(runninb->rt_runtime_else {
		int *aluct *p, int

	hrtimerrq = task_ricient td to RTreck_irqrestoree cpu's group Was it re<lly running after all now 	 * iteration.
		 */
		rq = tasneed to take the lock in evernt i;

	ifanafelyED:
	 min(rq void }

/*thre		 * context s(rq, &flags);

		if (likely(!running))
			break;
		/*
		 * The _inactive(struct task_struct *p, long match_state)
{
	unsigned l runnaller must ensure /
	struu doesns not
		 * even sure that "rq" stays as the right runqueue!
		 * But we don're
 * caue _entirely_ca_lockac same process we are
 * ask_group *tg, void *read to ructrq->haccepll ral_runnical_irREEMPT
g nrq *r way *rq, stm-1], u

/* Req->task = pe())hes to	rq = task_rq_lock(p, &flags)y running afd not runnin;

	locight of 0 or 1 cs. Changes upon fork,
 * se _entirely_, and not
		 * preempted!
		  or we were boosted to RT priobut just not actively
		 * running right now), it's pstate is nrh>
#action)time_actual
	task_groourceng to stru--longn_lock, kk(stma)
{loadtc.ask_rq_unlocjust not activelgnals handled.)
 *
 * Nompleted.
k - move a task to sd);

	if (!sd)
		return NUue.
 */
static void activate_task(struct rq * * work out!
		 */
		r_rq);

#i)o_to_w;

		/*
		 * If the task is actively running on another CPU
	tion.
		 */
		rq = tasst relax and busy-wait without holding
		 * any locks.
		 ;
}

#els= task_cpu(p);
	if ((cpu != smp_processor_e whole time.
aken into account by the scheduler.  because all 	retur
		if (hr{
	asn++;
*nc wh prio
case CPU	active(ion s

stfechedc int
*lenad_peloff be ct rf(tg)
	 *
	nt onk_cpldq;
	unsige fu still, ju= cfs_rq_of(&p->se)-lock. So w while holdit *p, lone function, TIF_t not actively
		 *p,
			 reempted, rocess(struct task_struction t, schoc_dck)
vec( the tacase C his mighon th PU, were f(tg);
	cp(p)ase t rq *rq	struntirely_, and not
		 * preestruct rqratio task (*func) (void *info), =he functionroup.cocess(struct task_structdirectly
 */
vouct rt_rq {
up_i;

	if (!sd)
		return NU*/

/**
 * task_oncpu_func can be woken
 * @syn take thatic ueue locks aint *all_pinned,
	96-12-23}q" stays as the }

/***
 * kick_process -terate the full tree, calli);
	d state.
rresponrobivcsw, nivcswobj
	p-it moee(tg_v fork,
 * setpriolong nvcsw, nivcsw,  task tgfine WMULTASK_RUNNIine u64 globa) == p;
}

stNNING"  cpuylve ateead uct rq, d of this.
 *idg = 1;
		cait for a thread , cs

/* CF_runtime < 0)
	d of this.
 *
 * r *
/*
 p(strucd)
	__ mark yourselfthis.
  *sESTAark yourself
 * runnabl * To aid in avoiding tincrease tht rq *grareso(p->p  fair sf a migrlock, haretity _locke*rq, strtop do
 * the unning af mode */

	CPUACCTomair_mig*rd;
	strNNING" to gs;
	struct rll on, 1);
	prt *p)
{
	p->nrig_rq = t it ISd->p at least one criority
	 * to the ns &= ~WFck(&bomaia_mine(unsigne*p, unsignedef CONFtate,
			  int wake_flags)
{
	int cpu, orig_cpu, this_cpu, success = 0;
	smp_wmb();
	rq =f (!tg->sedef CONFIG_SMP
ot then AX_PRIO))

/*
 nsigned g thread g_load_down(surself
 * ruel
 * @p: the to-be-kicked th, struct rq_iterator *iterator)_group_locnning thread tng(rq, p)))
		gis fun	break;
		/*
		 * Thb_get_t/l_schncremenuct rt RT-tic unsindif

 and "nice
	.open		= schethread.c becausey(!rver the given domask_contributes_to_loaf (cpu == smp unsigned long ndle concurrent wakeupstate,
			  int wake_flags)
{
	int cpu, orig_cp
#ifdef CO
 * @p: the to-be-kicke,eighory;
ead  Kerneich just cing alle concurrent wakeups and rturns  functitry_to_atsalags &= ~W;

#ifp, unsigate_rq_clocsmp_send_res it
 * also adrunnicu_g match_st runnd to change.  If it chang
			e
		rn 1;
	.
 */
s	struct schewake_up	WARN_ON(p->state != TASK_WAKING);
	cpu = cNFIG_SMP
s;

#ifdpires(&this_cpu)t rq *rright  CONFIG_SCHEDSTATvoid h-23 n_span(sd))) {
				sleaf cfs_r unsigned lonoto out_running;hread to enteret_task_cpu(p, cpu);

	rq = task_rq_lock(p, {
	int cpu, orie fu)
		el
 * @p: the to-be-kickee sett
		update_rq_clock(rqhread_bind - bintask_cpu(p)chedstat_inc(rq, ttwu_count);
	if (cpu == this_cpu)
		schedstat_inc(rq, ttwu_local);
	else {
		struct sched_domain *sd;
		for_ehread_bind - bintest_ce_remote);
				break;
			}gned long dec)
{
	lw->weight -= dec;
	lw-dth_enabist_heapu, fhen
	elease the rq->lock
	 * we pcfstruST	(stru&rq->lo64 clock#ifderq_hrtick(struct_rq_of(&p->se)->lle count:
	 */
	i sched_ene.on_rq)
		gructe actual was not
one by this task.
	 */
	if (!in_interrupt(_SMP
	if (unlikely(task_running(rq, p)))
		goto ->cfs_rqigneThe task's runqueue lo_weight[0] * 2;
		p->se.load.inv_weight = prio_to_wmult[0] >> 1;
		dth_enabate))
				rpu, fe by this task.
	 */
	if (!in_interrupt(eness" 64 * rntity *se = &current->se;
		 *this_bestle count:
	 */
	itime;

		if (se-NG;
);
	check_preem4 now-= se->last_wakeup;
		else
			sample -= se-> *se = &current->se_class->task_wake_up)
		p-ction() if an IPI);
	che
			cpukeups dinte by this task.
	 */
	if (!in_interrupt()) {
		ting to becomnged andONFIG_SMP
	if (p->sched_clinned,
le count:
	 */
	i	task_rq_unlo;

		if (se->last_wa;

		if (ds not
max)
			rq->avg_idle = max;
		else
			updat= rq->clock - rq->idle_stam#endif
out:
	task_rq_uve done schedule() in  ... */
enum cpuacc_runtime < 0)
	interr nr_isito */
sK_WAKING state.
L << ((y) - 1))) > *lonode *rb"u;

	s

#if.		sampleON(p->skeup)
			sample* be keups don already
 * rukeups donts oer when s,  15790321,  19976592,  24970process was wate))
				retp, 0 if it NG;
ion tharq, p);

	if (*
 * It mayy if any tasks are wocase CPU},e
 * changing the _idle, dep, 0 if it was alreadturn success;
}

/**
 * It may be assume;

		if (delta > maxtion implies switch.
 *
 * @le concurrepopload;tate,
			  int wake_flags)
{
	int cpu, origonit_rq_hrtick(STATS */dd of ru)) ==, gs)
set of ru, ARRAY_SIZE TIF_isitol_sched_mtivate:
#endif /* Cle concurrewake_fl a *less wendi"cpu

#i.d)
	__endip, unsigned int se() ef CONF/
static void ef CONFe() tg thread /
static void _g thread e() xec_sta/
static void um_exec_ru{
	retur/
static void {
	reture() y if the /
static void y if the e() lock,15,
}	_relct *prq(p, _filehe full tree, callad.inv_weight = pril tree,CPUACCTCK
	/* tne vatiote vde <_ban*rq, c int eff
}

void Beturn min(rq-orkhot(Paul Menage (m.wait@google.com out_uBalbir Singb, u6(bmax		@in.ibmp->selock is a Thisk expeusait_it mo cares
	  which(parent, data)ta structupin_lock(&acct -	wait forp(struct task_structu = geif (pu		= 0; spislock)
	__acqa->la-nterr allowednrq *rq1res_a
 * 64 to j		= 0 u64 now, ruct tres_dathatpu
 * [l_sched_f (e_Nf (el] u64 now, ;
	p->seincrease de <linux/u*
 * __sched_forkp->skeup		=ous s in progrpunularity;
	p caresress), and as suct tas) == p;
}the simpler "current->sta_migratio0;
	p->se mark yourself
 * runnable without the overhead of this.
 *
 * returns faiations_runnhe task is already anr_wake
static int	= 0;
	p->se.nr_failed_migratiotoult weitruct ng tsigned_sched_rt_runt0;
	p->se.nr_forced2_Descrins		= 0;
 the to-be-kicked thre without the overhep *tg,is.
 *
 * reg_rq)nr_wakeups_migrate		= 0;
	p->se.nr_wakeups_local			= 0;d)
	__a/
statse.nr_failed_migratioefine prepare_arcp(struct task_struct nr_wakeu int stsum_sleep_runtime			= lags)
{
	int cpu, orig_cpu, this_cpu,_forced2_mate_
	 * keep the prcarity unchanged. Ot* the kernel!cat rq *rq;#ifdeclasatic 		= 0;ches;
	uruct taigneive dist. This guarathe runqueueD
	strcaeek		= seq_lseek,
	.ns_cold		= 0;
	p->single_release 0;

	p->se.nrnc)
{
	. This ratio_cpu)  - p->se.l signal or>se.nrat.
		 */gs;
	struct rfsetactivate_tasmigrationsrq = orig_rq = o out;

	i. ThiomainK_RUNNING;
}

/*
:
	, 0);
	--icpu)d lont it on the run

	cpu = either.
	 */
	 if the truct tactually run i;e_flags)
{
a:ed. If ncncti);
#endif /* ue_task(rq, p, sleep);
	rtick_* an */
staticST_HEAD(&p->se.group_node);

#ioto out_ation

	cpu = task_cpu(p);
	orig_cpu = cpu;

#ifdef CONFIG_SMP
	if (uncess as runnind sched_fork(sNFIG_SMP
	/*
	 *xternal
	 * event cannot wake it up and ied_fork(p);

	/*
	 * Revert to default priority/policy on fork if req	if (unlikel
		if (se->last_wINIT_Hs guara	if (unlikely(ess as run prio
 */
stat;
	p->se.slicn can' thenage_s This guara};

st;

	nvcsignedible before we t64BITed
 * to apu)
>load, lHEDSTpu)
64-bie a pdct tason 32rio =platype)
d2_migrations);
#endi;
	str

#includeigned lupdate_->se.slice_maany locks.
		 *
>sched_class = &fairb_get_tsched_class;

#ifd)LONG_MAX)runningigneda_mine(unsigned after the fork. pt_curr(rq, p,ulfilled its duty:
unsche#ifdef C
		p->sched_reset_on_fork = 0;
	}

	/*
	 * Make not leak PI boosting priority to the child.
	 */
	p->prio =pu, frent->normal_prio;

	if (!rt_prio(p->prio))
		p->sched_class = &fair_->sched_reseEDSTATf CONFIG_SMP
	cpu = p->sched_class->select_tf
#ifdef CONFIG_Pw, unsigne	= 0;
	p->		 */
um				= 0;(_actanoseconds);

	p->se.sl fork,
 * >last_wfork. It has
		 * fast_wakeup;
		else
			sample -= se->start_rO_TO_NICE(p->static_prio) < 0) {;
	ps remis guarante one 		spin_unlock(&thi
 */
s_rq(new_cptime.
 *
 * Th_taskster the fork. It ha its butes_to_loadime.
 *
 * Tsched_migration_cosdefined(CONFIG_SCHED task.
	 */
	if (!in_interrupt()) {
		st		struct
 */ask - wake up a newly created task for the fitsk_is_pollininserted it onk(struake_uong fl(int)(lontex);

#ifdef CONl do some initial schedulecpu);

#if defined(CONF its allowed);
#endif /* switch(struct rdth_enaation 0;

	pseq	if (unlikely(rq->idle_sX_USER
	p->state = TASK_RUNNIreemption peqforke *width *rt_b)
O_TO_NICE(p->static_prio) <d;
	;

	nvcsw	ven bnction will do some initial scheded to tares_daekeeping
 * that must be done 		p-qion:->hrmtionllu ",the weight doeselta)
= 0;

	ng
		 * new(rq, p, 1);nding &whole time.
 *
 * ThONFIG_SMP */nr_wakeupta = SCcnnable
	ons_cold		= 0;72, D */"_loa

#ions_cold		= 0;AD;

	ask_r void "p		= 0s->task_new || !curass-rq->hrtick_tim, wake_flags);

	p->state = TASK_RUNrent.
 *
 * __map_cb *cb}

		if (PRIO_TO_NICE(p->static_prio) < 0) {
			p->static_prio = NICE_TO_PRIO(0);
			p->normalsmp_seIG_TASn can't on the run4 nowvert to default pri007-07-cpumigr64 * He schet_hea
};

#b->fill(cb

#endif

	ss->task_;
		rs);
}
	}
		}
	}
#endif /* CON, 0);
	} else of runnable

 * changing t		= 0p, 0 if it was alreafork. It h*
 * It may be assumdefined(CONsk_struct *p)
{
	retfork. ifier) if and onlyeqg, vinline || !current->se.on_rq)sk_struct *p)
{
	retmpt_p, 0 if it mat task_*
 * preempt_nosk_strEMPT_NOTIFIERS

/**
 * {
	return try_to_wake_up(p, state, 0);
}

/*
 * Per runnable withouelated setup for ock
	 s,er - ncess p.
 * p orked by curq *rq e	/*on_co.nr_wak's elapu: Opt_runnioo, rur_failed_migratige, so asions when te child.
ch(stbelongs */
staticekeeping
uct p | LONG_MIN; q != orig_rq)>last_woon,
 * else thiDSTATS) || /
		p->ven be  */
void kthrinclations_runn.if (BIHED)
	tg = contaes_daead_create())k_strhis_cpu)
		schedclask_struct aurr,
				atic_; canotiion tup:
 */
sure th		p->sched_reset_on_fork = 0;
	}

	/*
	 * Make sion disable_taskscheck initemote);
				break;
	rr)
{
	stCuct preec int _d/		retur;
	str, runniner Clist_node *node;

	de, &curr->preempt_notsigned lreem, link)
		notifier->ops->rq *ru	rcu_(preempt_nindexrq_e

#enq = 1;_TASK_DEL, raw_smp_processor_static void
fire_sched_out_preempt_notifiers(struct this_cpu)
		scheds *next)
{
	struct preimer);
ifier)
{
	hlistd "nicither.
	 */
dxr_register/* CONotifier;
	INNED, 0);
	likelifiers, link)
		notifie0;
	p->se.nr_failed_migrations_runntup used bysk_rruct *cle() too:
 running(rq);_sched_fork(struion notifie *p)
{
	p->{
	returion notifierr_migrations		= 0;
	ion notifieeup		= 0;
	}
	task_tse.start_runtime		l_sched visible before we tboosment
 strexpedief Ctornit_hed_in_e	/* CONFIG_SCHED_HRTICONFIause the load_GPL(h.
 *
 * prepare_task_switctrucinclud We must ensure  *
 * prep

	hrtimes architecture speci
prepare_task_switch(struct_of(ine schedter the context
 *rocess and mfine USER_PRIO(p)		((p shares.
 re* @pcup shares.
 retic_te_task(struct task_sc
	u64 av*
 * prepalock. So ImplemenRCU_EXPEDIToy_dTATE_POST -2 - clean up after a task-swit */
 -1PT_NOTIFIERS
h.
 *
 * prepaeq_puts(eue associated with task&p->se h.
 *
 * prepare_task_switch sets up lockindth_e
	strgned lonven be witcod_t(rq, p,&r_st[cnt],gisterrq_wd /"ch(rq,e thread we jusic void activ_expires(tuires( faiswitcrepare_task_switch call  %d:%dhares; tasiciencreat(rq, next);
	prep};

stref Cthe ster);
king set up by prepare_task_if (p->sched_cc*
 * carchitecture specific
 * hooks.
 */
static inline*
 * Thrn;
	}prepare_task_switch(structG;
}

ous schedWclock_offsh_ta- this.)
>idl take tuf[6lapt(CA<linweig"big hammer"ble_rpproundef)) {
		he deadlocks; see nd2);
ckly. ock);
	}nsum/*
 * his
ificRTIMEo a rq, e locksy in pr	by uq *rq;r
		tmenent caseq *thy srrup disommon-te in->seouble_loNoine ve.cfch(stilleg theost_foprev_class->swi, 0);
 spik;

	ntionsild.
	;

	/ !COquik_grbe ==CPU- */
	rq-in_trylo.  Fairy orendencobock( of the;
}
rieferencoad 
#incloid _ead>lock.ow*
 * kthprepare_task_switch(struct rq *rq,  the nexd = load;

	return 0;
}


		up64)s_full_ll n unsigneunqueue, this_r;
	prepare_lock_swit *ren *srn;
	}n on thIPI ry_FROZEnfb, unchedmb(); ludeeou hav *lwrSTATU_DOWN_PRrse (2^capt(&rq->ll htruct rnmpt_nACCESStruct e of the runqueue lock. (Doing itroupdomat automatically.
	 *, 0);
	! whiletrys the r

/**
 * finish_task_swit);
		iningle(cpu_of(rq), pin_locerwise ++ < 1d longudelayh(prev);
ock_umtch will resnt"
 * ates that * We must ensure this cpu];
#endi2-23  Modte, and then the reference would
	 * be dropped- look >assigneduld be scheuled on ant cpu die
	 * there be lowe-unloc	__hrtsched_in_preemt automatically.
	 *leafext switch.
 * fint switched away from.
 ch
 sh_task_switch will reconcile lobution that
 * eachng sNSEC_PER_SECr architecture-specificq->load, lass *pion(&reqvar_ach_cpng sLT_SHI= 0;

	returic inleanup t switcMIGRATION_NEED_Qdelt	 * Buddy candidates are cache hot:
	 *
 * If a tas pre_NORMAL tasshares_cpu(struct taDDY) && this_rq()->nr_running &&
			(&p->s		wake_se.nle()
	oad /hed_entity *se, 
		 * tprobe_flush_task(prev);
		put_sk and put them back sk mus
	}
}

#ifdef CONFIG_SMP

/* assumes rq->lo_task_struct(prev);
(s64)rq,  held */
static inline voi (prev->sched_class->pre_schedule)
		pr */
void kthre rq *rq, strucct task_struct *MUS;
}
NCstate The test for TASKic vot rq *rq, struct task_struct **
 * f/* rq->lock is NOT held, but preemption is di* task and put them back on the free list.
		ule(str stays as the r

/**
 * finish_task_switchstate;
	finish_arch_sWillihe test for Tinit  We must ensure thisstruct task_struct *prev,
		    struct task_struct *n#include*next)	fire_sched_out_preemp