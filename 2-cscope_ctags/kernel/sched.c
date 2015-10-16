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
 *is not the appropriate c
 *to perform load balancing
	 *    aKernis level.pyrig2) Therec
 *   busy sibling grouprelatull from  Linu3 Toris3  Modiisernelbusiestx bugs Groth4 to fix bugs in mormaphoy than sema vg *  reness (C) 199 Linuht sched_domain Groth5 to v imand r lds
 withi11-19	specified limit Groth6) Any reAndrea Awould leadifieding-poCo Lin/
	if ((1) sche&& !(*(1) sch))
		goto ret;
brid p!sds.ements a|| 		an array-_nr_runn2-23== 0-robin deout_(1) schdnangel
tch m and.alls
>=tch mmax Cleaimeslices
 *ch mdated-CPUch mavgeful n= (SCHED_LOAD_SCALE *s andtotaleful s /by Robert Lpwavide runqueues. i, preups andenzi, prsuggestions
 *		by Davideacti100s   200 usLove. <Con ->y (1) sch_pctcode bybtuning bsuggestionons
 *by Davide Libethod ofClea_per_task /Con Kolng with distribut;eractivity bugs_imbimes tun2-23ngel as
 *
 ancing=
			minivity05-05  Load balancing,on Kolivas.
 *-CPU/* LinuWe're trybutito get all semacpus2007*  2ImerageLoad , so we don't Linuwant01-04	sh ourselvay-sbovety improvemecClea, nor dFS bywish t *		hgreduc() andmaxnhanced		byrbelow) andu12-23enhancemas either ofschese Linuacg allimprer just result insafe
  O(1) scbutilater,n Ko Molnar:ns
 *hancins around. Thusy Srlook for*  200inimum pos96-1erac(1) sch GrothNegativnux/nmi.h>
s (*we* areomas G1-29  R98-11anyone else) will Linube counted Stenoux/nmi.h>
A/mo,
 *se purposes --y Srcan'tapho.h>
e Galbbyed bydule.inux/ty Ni. BonterLovestedne <lpt.hnumberse <lthey'ighmem.appeare <lvery large valuecangel unsign04	Nongs Grotywith
 eode Nick Piggin
etz
 on Roseduling ents
suggestions
 *		by Davide/* Looks lik() ana Arcaat.h>nmiincl# Compute itpt.h>calcue Kr    1) sch(&sds,) and.c
 ,ux/nmi.h>
);
	returnludility.h>/;

ns
 *		by Da:y Peter Worvx/noti*
 obviouser#inclincluBut check if_ce <lxcodesomeux/de*  
 *		hgto save power
#includdebupid_n_h>
#i_e <l_ir schedubugsux/freez#include <lih>
#incimesvmallocinux/rcupdateret:
	*x/smp_lock= 0ux/lude <lNULL;
}

.c
 * findt.h>
imervity  -cpu.h se
 *  ray-srubility.amoule. <li Hasinandns
 *pt.hstatic struct rq *
/kthux/rcupdate.h>(e<linux)etz
  bugs *.h>
#, enurovec_idle_type lity,
		  tat<linux/syscux/nmi.h>
, constl#includcpumanci*h>
#)
{
	#includ <lih>
#incl=pdate, *rq;
	c.h>
#includtscks.h>
#iuset.hin  Th wifor_each.c
 (i,h>
#includsy.c
 s(.h>
#)) {
	.h>
#includunish>
#i =lude <_of(x/tick<linux/tick.h>capacity = DIV_ROUND_CLOSEST(h>
#i, tible k1-19	Ibitebugfs<linux/sysctlwlx/paU run*pt.h>kp_twithlinux/tses.himes	paceinue<asm/rq =updatrqreadsgfwl = weeoutedpdatClea(i) *ac#inclx/rcupdateOINTS
#gfs.h>
#fine irq_#includct&& rq->uling desi2-231 0 .wl >cupdate.h>
upri.h"

#define th
 iorit Nick Pispdate	td<linux/sye <a			threalayac <lin	}
	}
.h>
#incl/rcupdatept.h>percMax backoffamespacene <linr */
de <upt.h. Pretty arbitrarygs.h>e, butIO_Tsox/unisas PRI991-lity.enoughincludX_PRIO-1MAX_PINNED_INTERVAL	512cludludekbutiegs.h>
r.h>
oad b(1) schetz
 can work05  _newlity.nux/heq_filDEFINE_PER_CPU(egtlb.h
var_t,tinclwhen scaltmp.h>
  byPRIO_TCh>
#azer.h>
#relaensux/noic_pr*		by Danel_si
 ifieded Attempirq_rmoveIO_T- 20)>iedt, x/noth
 *include <linux/hchedulity.ter when sca(ity. <linux/rcnclude <lingun orqlinux/sysctle <linPRIO(p *sdlinux/sysctl.h>>
#inclutlb.h><linuxffy .h>
lude is td_p)->by Sll_T_PRIO = 0,y Has NS_20 ...puselinuility.update*/
#define NS_TOsyangels.#define NS_TOk.h>pdate.h>
kernel bitnux/threalaIFT		SCHED_LOAD_Sflagskernel bitthing weohed_ = __geupdatea [(.. 39 ] ASK_U    /AX_PRI	 * it's copypercs) and on

/*_or tible 
#incluihenlude <lsaviude policy)
#denabledx.h>
#in.parentER_SE(p,JIFFI Linu <libuti#incps.h>upE_0_S
irre002- NS_Toflinu19 value s. I
 * is catlb.hrea eC) 1<linq_d, ie		SCH value tIO_To
#intay-ss CPU_IDLE, Thoty Inof RUNTportralinu2it_ine DE(NOT(<lininux/threadsLL)

!=illed aFIFO |c pr<linu) an & SD_SHAREers,POWER &&de <l !ire.
 d_HZ / 1(ion
SD_O-1 nSAVINGS_BALANCEcpuprOADLL)

ED1vity n Ko((u6e <l>
#itlb_e <li[lity]defiredo:
	upde <lsh <li(s*
 *	 bugs =x/kthux/rcupdang clact t <linux/rc&acct_
#in.hIFFIE &policy(ES(TIsk_haTJIFFfs.h>
#incwith
 ((unsign2-23_JIFF004-04-02	Scheduler domai!g claspdatey)<linux/percThis 
 *   unlgr * any-qiter */
	struct list_ + (the 'tunyac
#define NICEtlng c_0_SHh>RE_BITay {
	DECLA>FFIEgnode C!h>
#incAX_RPER_SE];
};

e.h>
#int_bandwiqthlude/* nestSMP sity rnelrq lock: BUG_ON(nice)	(MA=riodprrq CHEDPRIO];
};
addruct rt_pdate.h>
th {
	CHED_LOAe NS_T
	TIME) / idth *rtd pr>
#inc 19 ]RUNT>
#i>el/O+1);
#in (unUSER000)

(E) / - 20)
#Ifprovemclassnclu/
st has fincluriod_AX_USER_PRIOIO_Tmer *tdetimer_restart<= 1erioeores SMP t_b =
stilstat*		by Da.( overrun simply	((uys zeroo CFSIO)
#t_b =
correctly treapdate.hAX_timer(str		_b =/
		local_irqtl.h>
rn 0;taticdouble_rq_th d(anoseconimer, sttatic overrun);
t hrtlinuxt_b =);

	pers for conum hrap,' is R  			atict_b =ct time_t	MSECameteSEravenow,riod_->un;
	}

	reschedrid p!)
			brhrnclud_frestorard(nclud,_;
	}

	re_HED_Lon Ros <lidihrea  Cleane <linckng cu*rt_			br =_filn)
			br;&&O(p)		uct != smp_processor_id(n rt_iree <line <lzer.h>
#eriod_ti A6  INS_TOo for SCfs.h>
#inwPRIOPER_SECbye DE affin... _b = = nsun *rt_y( HR oveR_NOfor SCHRR     s)lear>rt_pestiof>
#in hr)od;
	u64		lues [r * it's er(s
strimntime;slicessidot_ban struct rt_bandwidth_SECck: );
	r overrun
	struct hrtimer		rt_periodfailed
};

staticRUNTnr9 ] rangeoid st++RIO-1 ],
NOTONIC, width(	rt_period_time >/vmalcO(p)_or de.ries+2DE_REdelimpin>aticrt_borwar&_timer(tiatic,ern 0;eriod_sctl Mik>

#not0 * h>graaski_thre by or, strcurr_acteq_fi_pck);h>
#inclnd reext.h-12-utimevx/ctr.h>
#t_pe&riod period_timer;
/rcupdate <linux/r

/*s_to_kti
	/vmallo;in_l->sche_de <wMO>rt_rL);ime ==ER_MRErt_bimer now(&rt_b->rt
brid pnit_rt_k;

 HRTIMER_NC /1nclud)estions
 oneTIMER_Nncludck: ;
		kti		return;))MAX_PRIO-1NIbreamer)R_RESTART;
}

ssoft = hr_f_INF)d(		return;ule..c
 *seq_filcpurward(;;

		soft = hCEE_INF)d}unnclu ==b->rtt_rt_bcb_get_nclu(&: HRTI_bandwidthnclud_filpires(&rt_b->rtward(wake_usctl);
		functiont));
		__hd-robretrt_b Hasvong detWe'veard, fs.ht_b =,e <linux/,	idlME_INF	_banureE_ABS_P -' is atic delta;
_tSTARb->rt_pe!iod_timclud* MESLICE() ||riod__INF)eturnME)	((u_t now;

	if (!rt_banHED_LOArunti
	ktime!pires(&rt_b->rtand CONFIWnitsub(hUP_SCHE	i	for ard, serioe <linux/(MAXee <li_RT_GR *  ] rangerch_init_destromin* detach_dth o)

/*f

/*
t_b =
I((prNED,begun}
	spin_unrt_peub(hstarirq_rCE(p oflude clude	nowativitnterKeg cla
			hed_d-19	I
		hrtimerlogIO(Mr, stAX_Uidthiock)ly 1(hard, sread.h>
yktime_tini(becaus sch_sub(haca/comsche
}

stdle )ched_domaCK_MO relates,_domins andd<stroy_axCGROUP_SC_ns(&def CONFIG_CGROUP_SCH*= 2dth defuntim_) and r-listpolicy(pux/vmallo 0<linuseq_fili get  <lin    _has

/*
= SCHEe.h>
#iCHED
e.h>
#i*ph>
#idif

#irt_n)
			break-edulaestions
>
#includblkdev.h>PRIO];
};

struct rt_*		by Dath {
	/* uROUP_y(p->CONFIG_Fv_bandel(&rrd, soft));
		IES(TItes crt_ze#defllsstara detach_dhrtim ns_RTIMER_MMO
#endif
CONFIG_CGstruct cfs rt_rnelnicinux/ke dwid**rtfdef  **rt_se;
	struct r
	e.h>
#ic  Mod_staticoseq_e css;t_entity
#iandwPU run*		 uidruct list_head lidth rt_FAIR_andwidth;
#en/*() andulSLIC entitiesheduched3  Mod on p.h>get_*  kee.h>
#isch	th *rt_n)
			break
stouupda
#ifdto_ktimsubsale.h>
#ured chile Rlude <l overrunRIO-1R_SE_Ter(struiod);
	r-, ovePER_SE cfs_rqne TASK(timer);
	r_sttimer(struct hrseq_fi_prin;

	srity' is timer);
		o(Ue.h>
alICE		romo jiffule wy exFIG_FAIne MAbouCONF_mecupdaLL)

ptibleEWLYIFO |od);U0ux/hig

	sctivpedask_imer(struMAX
.. 39 ] range2-23var* Helpcludforh>
#v_hrtng nriod);

k group'roup le Osolutiolinux/sysctlrt_b->rt_perio0_SHIFTidth *rt_b, ice)	(MAXct.hIFTicy(p-> kernelentinux/percng)t overrunidth *rt_b,policy(p-> kere NS;
		hrtimer_f ,
 * ncluG_CGROfa    limiter * rt_100 msecs (used onlyt tastible 
	rt_b->r	retu_PRIOestio7-05-refilled afincllinu expirly for SCHEDne DEF_ oveSLit_ta(if /* cy(str000)nux/percsinglq;

	strthat d>rt_esacti
		deltad_doma)~0U unNew ueuntimstruct sched_rt (unIrt_runt((u64)~0ULL) CONFIG_FAIR_GROUP_Sschedulntfine DEh>
#iid prt_rkely(edulabelta))
		return |fine DEeltadif

#ifdef CONFIG_FAIR_GROUP_SCHED
	/* schedulable entities of this group on each cpuedulablp->root_taIO];
};

struct rt__useprpy for e entiti/* nue datto create_schd, sofdSTART;
}

d_userTngs;
	st)
{
	struct rt_bat_period
 *	_thod ludeDECLAE_SPINLOCK(tasrt_b, oveMAP(bitma)
{
	usfs.hd_rtysctleue[user)
{
	usrtimer		rt_period_timerdtE_SPINLOCK(CHED
gric struct rt_bandwidth defIG_USpiUTEX(_tldren)D(stru>rt_p;
	ktoidoup on each un);
	}

	resched_rt_USER_SCHED;_USER_SCH ktime_n);
	}

	re
		__h;
hed_rtINIT_d = u;
	strkern	(2*t schedkern)
#

/*CHED
# dhon toFIG_FAIt doER_SCHED

/*
 * A weigw;

	if (!efine INIT_TASK Helpe* A wpoliter function toq_filimer, sofrt_bndwiart() and  weiof which entit ktime_IG_RT_GART : Hshare *rt_b, TART;
}

static
void t_enIG_FAI->c weine MAlon ey to cred_schedto crearq_ere'sfunHaski 			bresmp-rtimer_gtruct t=the sum of weights of whics cfs_	reak	}
 on eachne INIT_TASK_GR	uid_	?HRTIMER_MODRESTART : Ha = ktiE_SPI (The init_tg_clude  iINIT_MO
{
	ktimect sched_r	break;

);
	ard, soft));
		__h.
 */
#def= * too larg*
 * A weightf CONFIG_FAIR_GROUP_S A weightif

#iruntim sum of weR_SCtatic(&rt_b->rt_perio0itic 		de>=ifdef CONFIG_Fs_rq;sta of 0 or 1 can cause*/
ste entities on;
};
*HZ / 1CONFIG_USEASK_Ghead ED
	/* schedulable entities of this group on each cpur()#elsv() and uct rt((unmX_RT_P *rt_AX_PR< of this group on eac_WAKEUPned( **rt_se;
	struct r)t now;

	if (!rt_banid < 2_subsys_id),
				st;

/* renter LIST	idle  * to sin a non-LL)

ic Lootu (;;def COat.h>
f CONFIfs_rsk_haaimer(struccompleteGROUree_head lb, u64CPU
#def*packagee <le same mir schusent en shares v1  G
	use
static subsdomaHED_been exteness.e)
{
er when scauct cfs_/)g clapeedupf CONFI nsolidelta,ched)
	tg = =_subsysd_dote(p,get__truct  ()->se ruct list	rqle entCPU(struct scr		rt_po thsCHED
uct liCONFIG_SMP
static i).ruct.ery UIandwfine Nmp>rt_T : Hn];
	p->_b_gndwihigesigCHED_LOA.  Howincl UID-0p task e[{1,2}
#<lin CONFIG_FAIR_GROvoseleurn x bugs SCHEDwhicLoadinux/ C <linutrucbqght ofx/inent entityt p)->s ordergntic rt_b, truct t>rt.tive(varidef CONF)->rt_se M 'tupnit_urh>
#_entit	/a;
	struct rt*h betifuct lird, sse[cpu]ruct list}
ignedcpu) { }
statict t(ktiask{
	stnot_b (smHmer)
statibshouken
{up *parent;
	sstructcfs_/ask_guct liUngnednfe
 CHED *ux/in opedelta, duevity;
	unsigned loinclt of a* CO
	mas Gh>
#iainecpuh>
##HED *ourcegned|| policis cf =
2
#define M}t of a ucce	rq, fdef CONF*balancesr / 1lyist_y if itDEFINE_MUTEX(ludedSrivllct cfs_
triggntai weightsendif2Le's h>
#incli.par = kte <linuwhileratup root_thelNew uat aties 3  Modsk group.
 *	Every task in Modruct lisub(hard, soft));
		__h,
				HRrt_b->rFINE_th dub(hard, sR_SCHED */
#_rq  tasd parent entity04	Nong		C systemHED t &rt_, hard
}

statihr_sub(hard, soft));
		__hd-robHARES	2
#sk_gDnit_tg_cef CONFIG_FAI	Encludle entime_rq 	nit_rt_baeriod_tNFIG_C

	u64 exgESTART 
#endif /* Cexpires(&rt_b->rthould et_&rt__CPU(sssub(hard, soft));
xpirestainar {
#_rq's ins(kt_CPU(	 * list is used dutimTART  Rosgs;
	struct list_heapyrig(nit_ Nicrs, c;

/* reSh
 *  SCHEfe
  ttwuld taskholdED
	a... 1CONFI(lowest 	_sche = kt(&4 - CFSth wei *parent;erd_doma_ASK_Usubsyp.
 */
struct task_go CFenti_scheh>
#		RTIME
		deltaRUNg desi

	strt_b->
A weight ofhis cfh>
#i ktime_tos byunsitqs aup *parent;
	struct list_head si	 * leaf_cfs_rq_ls CON/*activity  "owned"is thldren;
};

#ifdef of 0 or 1 can caus CONFIG_USER_SCHED
	rcu_read_lock();
	tg = __task_c6-12-2sock();
	tg = __task_childrentimer	p *parent;**rt_se;
	sght fraction assigned to
	 * t
	u64 e0tg_uid(struandwi(1) schercafe
 ude <liFIG_aka()
	stfs)		((p)ty **aif
};
startht root_t

/*
 * Rg cla
	strED */
SCHEDaf_cfs_rqsoup;

#ifdef af_coot_task_grou/* Default task group's sched entt_enparameters,
      staticicy(SHAthis g*  Co le entde <linux/unisnex
 *		by D =roup head+ HZecuributed byandwistam;
	sibuted byhis.)g = _;

gned long nzi_rq {<ic inlinp(p)->s lrqs holcoststrues' rex/pagad.hng cPRIO(ppyriga hiers	 * leade <linux/unisCONF
	tg p_headtog_rq 	struc_SCHED
#inclif iniion_(CO.. is tth *rt_b)dif

#ifdef CON
#ifdef NEWntitiER_MODen     /
sthis groD */
#_SCHEtop seT_GRing:_sched_CHED
	p} highe_ro Ker_b->
		__

/*; <linux/rcriod);

#endifMER_Mernel CHED
UP_SCHes* CONinfo
 entitandwidth rt_bandwidth *parent;CHED  DEFI(andwidth rt_ktim->lasrent;
	stS+u's paNesntime;andwidth rt_SMPll bthat ed COry UID  Davdos
	 */
	u leaf_rt_rfor SCH#elsr_migratoryschensi intother hp on eachycpuset * Ne islswitof a st;
-d CONFIGUSER_SCHE->andwidth rt_ list_/*
de rq Wight lin dearPNFIG* variables. entiti d, sbat.h>od int cy(pol19* leaf c . Se = dal * variables.sk_group {stroyxclusiv	/*
	set  =omic_CHED
coudth dne PRIO_Trcu_tas = task_grouhn assi;

/ask_pusha 				HR *tit_enshe_rq'sHED
	rc(ssCHED
io)ead.h>
#incl * Aonstrued;
>
#i{iams
equir  Grt	strstT_HEAD(tined CHED ude runon rt_t physicalmt_sew_t rtfs.h>
#ive beta lingi/*
	pri;
/CHEDic inalx/ach_ng clHED_L higach_dtel_s <li withrq_SCHED
	rcal-Time cllt th "RTn)
		alls"  schft, hardt_sched_entlist_	SCHEle.h>
# * tsks (l)(tandwef_ro =antim membead balancekernel bitAX_PRIO-xtt_rq_lst  group's sched ld declyng cIandwieighnrn tg;
taskint?	hux/debur
struct rt_USER_SCHED
# dh_b->P_SCHED
	p L
	/*
g EATE_TRACEal-Tines adefi>
#includapho schi a CPis "imfs.h>
#i"rt_b-it occur
		__owe neent en
strut. Ord seallle Onit_ weig_GROUBjorn* Degaa_rqs a 128-ist_puma*		mak/ * TAy task ier.h_use=_b->cking  the /or dee a/*pyrit_se;
 * This ir	unsng oINE_Pnnange le entities
	  (Tplstrucque,d excin shoung al Dave /
	u.) for SCHpyri/ CONFIGtasks (l nration.
	 */
strucSFIG_Ufs_rqa<lin spa	struc_expid, u64s Nic _mas.h>
 q.
	 d_dohrottlein
	ation cktime_scheFIG_UCE_0_LOAD)
#else /* !CONFI rCHED
cred
 * it's /rcupdatelateddndif
struct rtRIO(p_U_LOchildIR_GRO shoumem **rt_sUdefidulabis creapu shares.
 */
staatic DEFIon.
system2
#de;

/NFIGDX_MAX 5
	uo Moon.rt_b =
f whic
 * too laityct rlontitipu];
#eupdate_weiu64 nr_swihan
	#endifth *rt_t list_head siblin	uid _bandw	#de}hnclufieledulable entiticalls
le.h>cfs_rq c;
	#dned  * variables.NO_HZER_PRIO(#includ{
	at_var_lock;
f CONFIGr; */

#ifdea [ 0T_GRO.h>
pbe
 uset.global ilb_grp_nohz <li wh} yrigh____MESLI_SCHEalg clas=ructOUP_Struct rt_ = ATOMIC_or 1(-1),imer	e NSatiothiso this counter(lt tot_d

	u64 e list_			HR(&thisPU_Seuesinclurth;
#endi h {
	/d(g = __tce.h>
MC>
#iFINE_Part of tg-th de
SMT)
/*n witup(pst_ruct up_lock- R * leafFIG_Udef CONFIG_staO..Mai to sructt_se;@cpu:	struist_whos64 rdle;
-2002)~0Ucause>uid;
};
	u6k);

	reGleu64 imicki @ructmmof tructs/gro_PRIO	(100 * *uct  partit CPU_LOait;
	(100 * gong h>
#main wit*in_l,nread.ct sched_domain *sd
	ifathis grgnedg ue d_For acchart do CONFI

#ifdef CO-05-ty

#ifdef CONFIG_ndifNFIG_USER_SCHEc in/* Delude <nts by;MP#defnt;
	in;lt tue data struHZdefine CPU_LOAD_ck_se/* R
	structst_ti e_balan
	 *main tasksight lo.h>
#incl.htg_uid(RIO_Tpnt rt_thities of this Ittg =eh>
#incljiffy resolcic_t	int *
 R_SCHce;
	inh>
#itruct st_scheev	 * it o/wlliamiHED
	nsignvrents. Esd:		D */ iniSd lone b* COp)		Pcapa = weo)

/*rnux/t_sin
	 at.h><linat. EaNFIG_USERst;
 relfiime(RHED *eruct list_	/ined UP_S;
	imickisd_ppHED
	/*avg_id0ntims upned lon Work_csd;pu_louct lisdleCHED_
#elsK
#i'O(p)'O_NICetnd rela *
 *CHED
 d long
 *
 *  Co
	#dei	unsiimta struUser.
 * {
	/'ed_domr hrtick <lins*
 * fut_perio) \ruct ead;
_lis lonnt/*
	s_munt s

#i.. 1r;t_rq_timck();
	tg = __task_
 * ;d[CPEdef ecy(str loatick_gis_semifdef C bugs ->
#ius *	Everyve_load
#define NIC
 * sigask'sc_allstal suoup:

#ifdest_ticated*.ne;
rqd_domain *  2aktim
# de/* F:	1hed_domai_ways hed_domain * 0
#endiwisCHED_LingWeunsigne asche and wayse in thsigendiothisd b#elsatned inaine;

s-(kti (smpdarameters,_SHe fiel's st_he* acqhDefau funn_vruno_wakhhed_domain *ict scnt bkl_coun int ttwu_loco*rb_nt ttw
	cph/*
 nt ttwe NS_urr(struct  int bad from *adefine NICE and goidct ca * it's and( RosCched_gmpyrig)
		 ent.hz.ways up rq's in (rq, p, _rq's i.su sharstaode), lock
 A*rq,le.h>
#iies of this grtask_cMP
		int next; _SHups);sks (NEDw;

	if (q,activs);

cfsNS_TO_d, u6s.turn group topunux/e.h>
#inclurq * 'cpt_curr(rq	 * t.
 *
	strrelaquaeracnchronize_sched for deeach cq->q->cugned ion each etail     

struc	u64 e1(p->tats */eing new_ilb - Findf CO*/optetail;

stNED(struh>
#r(p->Snom>
#ier	strulds */
	unsigned loupds_rqqunti)e becnewoot_ta this countercsd_ed int ttwu_cnt ttwuoinfo;
#euntimthi= rcu_dlinue	retuatic Dexists,;

	uEl64 eb-
#defi>= imer);
irelated cpuaphoalg* anhmRED_Aated 	(&__get_cpu_var(runsuchtency i;
#eprio;taskntitschnsigned rr(rqONFIG_Sa jiffy resoldinteradeanr_iowms
  init_tg_cGNEDo Connqueuetivityronizes/coendi	idlesched_rt_O_PRsmlse /* !C runtime)
{ux/s lonask_)nsigght lvoid sn.
 * Se'CHED
	rcudwidetinglsuivity.h>
#iat joboup;

#ifdef CON_P_SCHelate(cIR_G_grod long avg_load_per_task;

	st mpt_in_l(ransp,todayse
	returtic DEFINE_HHED_)

#de->in_lEvery UIFAIR_perat_se;
ar(ctivity/*
 (cpfs_rq  withER_CPU(st-awrentcome constantsntity, initnux/threads!Dp(p)->smtpacea CONFIG_Sawitc(p)->sewith the runqudimeslicesait;dont rt_
#incluO thoiz
	rt_b#inrt_sGR calcwestructNS_T

st
#enp, ieturnonEFINE_ocked(in. D The walp;

/*ch cpu */
sta rt_ttruyue d	stra */
strucendif
}main treprio;
 synch CONFIG_)e(p)->seRostr*  Keit rt_O	unsndwiline;
rq.exec_cth d +rt_sef this group on eachnt hrt);
}

stblings;omainsis csdoor SCHp
#en_	stru->pid_n_TIMe* preempt-dpu)->.
 */
#main trefirsted SCHronizeER_SCHt tasdt_entifendif /*EAT_sched_fet spanSCHED}omain *< _SCHED)* A w-AX_PRIO-1r by fstruct t : rt_nr_b#oy_rt_ba |ks).nst_dd_featu(n(p->#is_rq/*   long nr_unint	 g nr_uAD_ID_for t			beue_up onnameurr->t bkl_me_tayin
 * p# d/cgrry UID cGh"
	0;
DEBUGAX_PRIO-1ched_featu(nameh>
#ntitated USEine croutmainly runclutoent;

		nt bkln(cpD_FEATcome constants)D

#twneigh/_run_/tlb.h>
d longtod);crented fps_sc i;
ic is[_filed rq_cpc DEFlancehis count cha * lehalfine r
#endblSCHED
*d lo;
#endif
 Has 'name' 4 nr_s(m,go[] =up on entiml
{
	mogratioongles. C*balanceNS_T& (1UL <<(td) \
x/not");
nod calcP_SCone)t stat[i]);
	}
	se of a leep ed logd.
andwi#unddef venTct sarrives.. shares F(100 * >
#it_rq ,retur <linuKe_feate_ssAerwise > 63)
*balancertitor_ea_s _runtime on.
que() |s ct file * i)))ned lobrt_ent_gro_bal_FEAT

statine->se0;
W CPU_ 63;
ables. Cntimd); __signedfghestCONFi ib->rt_pecntrt_nr__rq; of a  'tu, u64tUL <]; i+oet to NUL\
	o)	(([	e_balbafs_rqs 63;
_sCU's eat_nclude_t c DEFeq_put");
 "\nhed f;theitrnnline t__sd->ppo **ralls_clud=
}

lat " *ubu
	if (cntf CON{
		n* too l
	if (cnt*dd_feat1UL <,()truc *v)
{d loen(t bkl_fe, durables. , "NO64]; unlim
	struct_s[i](63;
/it gorq c * * futic DEFItimes irn cn*rq;cnt;ct calain creat*
	 * leaf c =trucnteractivt_na; __soo lahread m_t r->in
statirec24 -meoup's cruct task_ "RT orn 0;
or SCH	#def*f COon aninux/pCPU. Always up higb_t rtT_##ic i) *The doroup ABS_P rt_nso crecu;
aofft ttw sinned loinfo;eadcludrefe;

NS_Tup!ead__sched_t_b-nt bkl_cmpxchg_fops = {
	.open		= exec_, -1ames[i])at_opeBUGlroup pen,
	.writeime t;
main tresq* Se;and yiD_FEAT

static ied_ent4im
	rt_b- i);
			balsc DEFIched_oup {
#ifitntim/MAX_PRIO-1_FEAT

static=ude <OUP_SCHE;
#eledebuock ons= (1UL 
{
	dfops =feat.open		|= (=id)
{
	debu= (1UL setc *
 *_SCH int bkl_ini-1"ownsot_rq("& (1UL << ure reped if] = .h"
	0;
eatu_##xf CONFIG_ug(vr SCHe ca */

		}
	}[i]; i++) FINE__cause this  CONFIG_Fterate in a single balexec_edt_nr_b	debufile("s_b->rist_hon
	ifbuf[cnt] = one with IRQs disabled.
  Numd_featundif
y
#elc inlinstarh>
#06  d05  Loigned long ntoo larth d_domaild
	tg k.parwhethture o	retudef sched_feaer(struo se 0.25ery UID tae_iteru
#endefficimp =seek,
ilb **rt_sRT_GRtly
#elp!CONFIGclude "sn codfeaturesl_sched_<q_TASK_he cst_tl_sched_(1UL <<timer_gf	strucstaress.cu;
ange EFINE_P *
 * e ris attached * 250000t_tg: 1s
.
 * Limit contahis avoids rawherlt: 0CHEDgs;
	struct list_y in
, ude t * f&& (1UL _subsys_id),The dootup.
 */
struct taske measure -rt ta<< __.25ms
 */
unsigned int sysctl_sched_shares_rbIG_Snr_migrate = 32;

/*
 * ratelimit for ut__POIN; /* hed_f_POINTs/* c? HRunsiged;

	s & (1UL gs;
	st/* caraine INITcould a1UL <_feat_shIx/.h>
#omain
 >fead  dif

ng_read_rheR_CPeuser->taddomabude are cd rt_n ustrnit0.95fair" load
			i	tg = __tif so shares B);
		seq_s
 *metprio;ctl_stLIGNincluchCHED
p = __tables. i or thigne the toleixner,struct tase
# def
oad from *ae NS_TO_JIFFIt inode *rd, soft));
		nclude <linigneach_de m 4;reatv_mmendif /* CONper-dotsduler domaiof tg-lock);

#i/* Earlray-ssched	unsdo { } _itoIN_Sn RUNTIME_agt hig/t_tasruct list	struct list_hpendi_domain 60*t
scw ul=to crea{ } while (0)
#d by RCAdo {_ {
	atoz with IRQs dna
enum {
#incluhed_domainNICE_0_LOAD)
#else /* !CONFIfine prepar */
# dONFIOUP_SCH bits
CONFIG_CGROUP_SCandwidthCHED */
up(pdomain *suct list_heaad_getsy_factoinux/k/ed fale mw
	stendif

soup {
d;
	unsigns group.
	 */
	une addq **rto	unsi
{
	ktime! essentide *
*migrue lh(s_INF); __sruct lis> HZ*Nt "owSforwarsng;

	ufi1991ched f
 * Th<< __ * The domain blings;
#ifdef CONFERIALIZEstruct rt_* The domain ed_features!	delttryWg rt 0; & (1UL h a task bely **);
#egroup uset. When_eqruct a newch exclusive cpuset essentimad_featurese hnclutoday)*/
untruct _FEAct t?
	cpumaeuesre comote r_feahe ni,ockedCONFIG_USERrq o*cmp u64adGROU statDave&__raUG'caroifdef)
nsidhour SMTrq);
#endparent->locnt =the rq lint sysct	icy(p->))
		return lrqs hoe_reh exclusive cpus
#endif

p on ea ] rat_fops le enrel */
sweight coneat_th d.ownelatass 	unsiuset. Whene(p))
#dgroup/higlock dependencies then we hne SCH*ED */SLICs. /* schr_t span;
	cpumaess24 -ally/
	unsig_current(struct  defaeatu"PU. Alruc inlin4cu_dereferen	hedule_164 cl fs.h>
#inck;
		}
d, we  * Ae(strt sche#domain es &= areUP_S aNVAL_iod * NSECmas (i.e39 ] rnolysk_group {
#if!d long task_eout(alls;
	
#inclu{ } while (0)er(&buf,ask_v lu)
{
	nwhile (0)Ct_perdo { Groth|=t file or d;
#ittESLInt en			rtvar_t nnt))ex->rt__file theg)s NULANTce vinux/threads
	ktimev)
{
#ifdee domain disablablepa, rtinuxt_sea [ 0return (une PRIO_Trun_n_GROUP_SCHEDm, "N
	u6;
	#destTS_ON_do { exec__gk_group *paren1UL  int Intg = __task_l_t rt_any rprocessor in ques[cnt] = 0;

 * fsene beeixn h_lMikHEDST[i]);
	}
	seck);whHED
	rcuakO(p)	ysctlimited becafile < 0is cfs_rq * ADEFIN->onCONFeuesdetails.
pu. r_fiheck_p*ht inode *;istri() |
 * The d;
			bfi_create_sd sectio avgr_HIS_rch_swi thesub(ha	4)sysctl_sched_rt_runtiine CPU_LOAD_linuxtstruc ?too larSCHEFO ||:We voU INITED_Cmeaser ->oncpu is cr <linux/rcuULL);#endif

	sust ensure eu = 1;
#FS-redif

#iNd) 		if (strck);(&rt_b->r -EFAULT;
e_t
schoups */
callod * NSEC_rintfneg)
%sG_SC*rt_sGn.
 * Seetati(c inline stFINE_Pfns += _ 63;

	i b M    ject some y charSCHED
	/s
 */
unsigned int sysctl_sched_sharned long erioadick(&rqprepp, sch HRTIMERUS	strucendif
 rt_thsk_haL;
}
ct l e measure -rt tad_features p the task = 32;

/*
{
	debuO..MAX_PRIO-1 & (1UL << _whed_f tasgetNFIGr at tdtruchile (0)
#enne becnux/smtask_*rq *bsk_gr uroup  cfs_
	/*pine N;

	 froma	/* cs_rqs ar	ifnt] = 0;

ED_ALiq_loed int sysctuct rq *rs attacether lin_vity ;
	strct rq *__task_rqup< 0)
oved rabled.
 
	.rea_fk -r_unimigrisabling pre_sharepeuset. Whenevk_var_t span;
	cpumaity t span;
	cpumaisned l
{
#ifdeftruct *prev)
{
e domain tree (rp on each IME_IN}nit_tg: 0_names[] = oched * sys_sche
# defsO..M	ude 
}!get_cpu_var(gned lolp,lock-wa< 0)
ne PRIO_TT;
	#deeltatstly chOFTIRQivity t_Dave
		}hhed_}

	retcq = task_rq(p);
ce.
	 */.
 */
iot *p)
	__acquie up  =d parenpla += 
#endiire(!sched
	/*
	t root_tINVALm interruptp	structs ecidFINE_domain *sdX(sch{
#ifdee
	returnt4)sysendif

whorq_sg compt:
	ant to ched_feat_names[obal ed,< 0)w fix up the r_SPINLOCatitruct ca_MUTEX(f this grou	_ {
		loes(s);
}

/ Srine SC_puts(CPUs ssiz wh we m); sing's qhedulttache 4
 *ev)
{
#ies(r0hin
e_t
sch.h>
#)	((prido { } .queue_
	 we n't h_GROUPt_cpu_var(ru;
}

/*
 	}

 * tfeat_hART this_rct li * task_rq_lockexplicp	unsipodisad sectioI* The do.25 rq unqueass->check_p inline struture o_
 * lim_avg = MSsigned ilding the
 * rq->lock.ume we , mreplretask_P sa program init_tg:if
	/*
	 CPU. Alisy Mie05  LoIRQsulinSLICEt_nr_bne SCHe NS_hat we do  locsh =itch
#lags);
	*rq)
{se
	retad		up tureattached *gtat.h>
eturn s rq *rq;

k(struct (;;un in usnsig Copy*rq = task_rq(p);
	OUP_SCH	sm
/*
 loff_ity 0;
#end*flags),{ } whefructundecpuseags);
}?u
 * iL << a
 * rn rq->rt_p -yness into chnly ong , structt rted_yigtiplCHED
	/_POINis grt list_hry UID & (1UL << (x) rn rq;
		spi
#endbylock);
	s
k, * schruct lislsee nr_ctuct rt_rq_ive(_rq_lock(vc esn't happen until t chafach>
#inr_uninHED_fs.h>
#ie_t
sch* promplrairn r for	_d_nr_migs(veuntinux/rqis comdif

#ifdenum hrti_rq's inis_
 */ed longags);
(hrtim_CTXSW */f CONFIG when:
 *  - eschedul -rable encruct lisf tasks tatn_is&HED_HRTICn RUNTIME_I CPU_L* prFINE_MUm {
# */
state preem} If - eniams
a or ths_PERies of this gr_CTXSW */x.h>
#}
ry bf;
			sy/
	t_rq_,t hr scalle();
#pce.h>
s disabth;
#enatory;
tlong nr_MPear(sPRIO_Ton UPRIO(p){
#ifED_HRTICh the runetw;
#eease:ck_timeq_lock upt prog/
a strucvity d(rq) task group's sc0)
#en
	spR (hrtim95s
 *m#incFIG_y char
#inclly pt, k not* UsEXPORTck(&rq->_SYMBOL(rnqueimerPRIO_T;
	/* )
{
tn

/*
 
cessoloc rt_nrw_rq(ste(strt  * Ti;
#ea 20)
#defP
	in @at we */
i	retu*
	 * 	 * {
#ifg *f
	struG_FAIR_Gh-06 s)
{
 ame f.
 */
str)ead_ovon @rq__ARCH_
statI
	/*dons_i#inclta_um {s_rqs arataskock);
	*pghed_fonize_sched fD
	/* suggesub(hardtimerrq->loc(
	stsysctl_scnsigned long nr;
	#de	le *flong rt_nr- pct *.e shay;
rc void CPU_64)ar_t  *rq, le *fktiurn sice) + nsq_MUTde <linux/unistime;
task_gs the shares va_namer = _csd_pd longknobs'ched_useault: 0.9cfs_rc voi Notg_cfs__restamemer_r
{
	struct hrp, &struct r ck_cse)	(Mending) {
		__pk(rq *ar
{
	stru = hrtS
	stq is 
hotCTXSW */
} <lin king 
		neg 	st hrttorenction num hdo { }se thisWe ime)
{
 *flags)h##namn 0;
cpu grourqr_strc_UP_Cuis atwipluD:
	case 'uct lprt wag:
	case Cflags);C some 	/* he(long)hcpyre_archcstaticimerong)h_ctimeendthe pe
	case _endit so_
 */
#de_EFINE_d.
 */
stat,ion_sinZEN:
		hr, 0 */
sD_FROZEN:
		hrtick_cpu = ount}roup to whichion, plug_hset t)sumhe shd.
 *rq( +FY_DONronize_sne MAX_U_bn 0;
*n voidine CPU_LOAD_ Haski,g;

	u*h*
	 * '*/

	/*  = ed.
ng)h

statsigned 	(100 *  enum c.h>
#incling;

	utZEN:eturLEDU_DOWN_PRPUOZEN:4 delay_FROZEN:sHED_U_DOWN_PREPARE)
{
	__hrtimick_timer, nnge_ns(&_to_ktime(deEAy)
{
	__hrq *Nock hel>rt_peZEN:
	t be
(puma;

	FROZt_nameP
	strucESLICa tawellrq_lole bMER_rq->lo	0;
HRcase HED_Lurre
 */
i_ktime(delay), 0,
		s */
s_pending;
	ing) {kd that weDE_REL_PINNE	HRTIMER_Mn asenum rq->cpuct_pe;
}
#els 0, tatic 
	for (OTIFY_OK;ser(&call_funcpudingpintalH_WA	}

	return NOTIFY_DONE;
}

static __init voiit_hrtick(void)
{
	hotcpu_notifierFY_DONE;
}.fuk_csdh *nfb,tick(sfier(hotptick(song)hled
 */
statiEPARE_FROZEN:
>
#i	case CPU_DEAD: = rqndif
d sectiot some fuzzy
	case CPU_UP_CANA(long), ubq *rqr
	hrtimentitruct  int sp:otati*eriod_t{ }
statiIG_FAIR_Goved )(domaihcpowationi by k) that IG_FAIR_Gsptityinreturnspomaisiase  (0)
task()
#ifdreIME_INtas_G_FAId:arkinclsk tp's ;

/dingf_varenc int/
he tosr (__s_etur_tick_start;
	rq->hrtick_csde ne

/*
d *ar_tasly lookt rq *r&rq->hrtick_cstrup's linux/sysctl	strusent. notult tis_potionnque_ned longrangg
#dwhere _tas64 (!smkle eve Admer st CPor_id(rq_lit_hX(schp-> **rt_hrti the s
dd(struct rgg	unsigneforuct taskoo lesceans the  Called tting t

	a_struct *sparent;;
	h5-06so q->cpu;nvolver_ii_struct *unsigned_POLLING_NRFLAG)
g(t) telock(vtcan sar-05-CPUo */
stat64	/* is groupam ad = uperi(p) >ainers	cpu = ->
	 *p pre]; i+64 Callct rtm
/*
 * , schmer_ih *rt_ test polreschilongEAT_fore - ett
#epolrescct rq *p_FAIRay {
to creatilp,rq->CPUACCT_STATk grouf this groupk_ne for d}The dosched(p)r sta(sche{
	struct r fingrals(pu);nize_sched for d}gu cfs_rqsrunning;

	undif

*
 * C that wesignue */
== task_rtible RTIMCKe rq loid)
 means the ;
	rmof te field' Injecrvirr_caCONFuctdSTAR'd sectioONFIGPU. Almeanic DEFfdef COched_useneu;

	asgned lag,d_feSMPe wheight  the tostcpu_hat ))
))
		rcrosser dot sod CON	 */lock);

ult: 0.95on
 restretur	if (cpuy for S * variables.IG_S lock _n th(d_pendletek(&rqIG_Si
#delCONFrtick_clen be infinite cpu = st we avisible bend i_b->rt_runtime = ru)IG_Sthe ngext 	ed(p);

 void hr)
{
	struct s);
}

#ifdele entities of this group o/

	/* sy
	sks;CHEDtached *ON_Ourn rqELED;
}

/*
 ible bke_upken up an means (p rq *rq = tis atet_
 * g so the newly added tieaf_ctimer event.ken up antask_lea NULth*pre =);
	rqPU_DEA the
 * future.he nexendisenheel for uOK;
	});_b->rt_rund
 * llling(p))
		smp_send_rescue dale(cpu);ve(&rq->lock, flaGNED(quxt timer 	 the newly addef C
/**_start;
FINE_MUTEX(ding dwidtr *time *flunqueue_is baletc.)ffset that  "NO_bled _ube_rqt Rs_rqf CO/higetailswitc;
}

#ifdef CONFIG_NO_HZ
/*
 * Wh_rq's This n() enqueues a timer into the timer wheel of an
 * idle CPU then this timer mighoy_dsk i_needu) heltimer event
 * which is schedk_rq_ubase lalizedSHARCPuled to wake up th case of a completely
 * idle ent might even be infinite time into the
 * future. wake_upsystem the next e, ng) {p_unld we rPF_Vble( 0;
(s tick.
	tg -worstdefi
#endp's stid sinc		smn
 * ixg_cfs_rreturn;

	snd evaluates the timklrqs hold oschedhe neh_archeaves the inner idle loos so the newly added tiod(vos taken into
 * )syscnt when the CPU goes bacgares HRTIMnd evaluates the timer
 * wheel foskched_useot timer event.ken up anZg);

 of a eu6

	if (cpu ==idle_cpu() ensures that the CPU is worq *rl b_mb(t pollinong loendimbock(	ned  test polit yet se(p))
		smp_send_rescirt_nrvg /= 2;
	expemnlock(&spin
	tg

static voidnlock(&	unsi
#defsome fuzzynesstn.
 ck(&t_avg /= 2;
	}
} smp_procZ */

sgGROUP_Sstatrq *rgned lt tg)
		s is saft *p)
{
	struct take the->ag/

	/* )
	_SYSTEMched_avg_period();ick_sta stawhile ((s64)ngnqueueHED
statiCPU gtrys not yeinclq->curr to idlect rqd(p)unt

	/wa	rq->hr int s_tealef CONFIG_NO_HZ
/*
 * Whg);

#if BITS_PEt rto.
	 */ whictas
	if))
		rcase of a completan additional NOOP schedule()
	 */
	set_tsk_need_resched(rq->idle);

	/* NEEunt;_timueck);
)AX_RT25ms_SHAR. Inr dois wokesk_rq(p)->
	if added timer is taken into
 # dock(ags)i64u));
	spinlt th an addit_hrti_LONGres 3}

#ifdef CONFIG_NO_HZ
/*
 * Whe rq *ne WMULT_ructT	 file sched_void hrtick_cle (!lw-roup'	32*flags);Shifyscte idl(unsignect rt_#endif

#RR(x, y) ((rq *+f[cnt] = ((y) - 1))) >> (yched_shareweight*include might evepts. rt_b->rt_p_creathas m& (1UL << __ the9 ]ioc voiNED */static voidclude too lar) - 1truct e.h>
#include r state.
 *
 *
static void resc nt;
}
d overflow the 64-bit muuct ifdeCPU_LOAD_delightuct *p)
	__VIRTW */
ACCOROUPNGe(&rq->lock, flae a wg5-05etex);d. W ] rby tpu_curr(cpu));
	spin_unlock_irqrestore(&rq->lock, flags);
ed(p))
 defindicunt;
endif

om_us		car statEDSTAwhile ((s_namD on the idle tau));
	sstruc
ops = {igned+1);
ess#ince w CPU_LOAD whic64 nr_ d by Rrent;ytes thee_cpu() ensurewheel d hrtick_ifdef CONht/2 comp	/ (lw->clude +() st_tast 4
 t docludw- * future.r the next timer eveuct *p= 32,truct *p)
{
	stindefine
	str)
(

#unll a bit	#defsend_rescCONF!= HARDIRQ_OFFSET+statT	(1UL <<4 period = p,t: 0 on  chiasktruct *p};

stro aid cfs_)
		angeaf_cme < 0)
subversi5  Loabb_lefeatu	retdefine R_
 * To aiw_sch(&rq->lock, flamultip(	lw-* s/
# dimer)clude ,NG > 32 && u
	switR_SCHED
	rc+) {
		 of a #els	casestol#inclu@hrtim:lude <lux/ke.ing (hrtim (!lw->inv_weight)< 32the cks(	}

	return NOCPU(yct caime
 * slice expndiendif

sures that (_LONGEebug)#ifdef CONFIG_SMPWMULT"imer"nux/kread_ SCHED_retuq);
#de <lature
 * deltale Oce
	unh a ilude  s is weiiry etcid __hS_PER_LONGEIGHT(int PRI is weightancing3S_PER_LONG > 32U-boundOq *task_rq_loshU
	streclag:pdefilockR_PRIsticn_locavail inice.h: */
	int  = __te multi	G > 32 && u/case of a ck(stLONG >pu));
		return NOTIFY_OK;
dif
rq_task_rq(PRIO effect"weigpu: )syscMULTcumG_RT_ve: Dave _any_rt_rq;-2002)sysc int sablou go up 1 level;
	uns -10% CPU usage, if you gohis.)def  so the newly auresoste 1q *rq, st)at
EAT_#_ =.25;
}
+Iset.le entoe		= de),~1)sysc	#def64 mer(HED_Kar(swad scCFS'olicyor evpollinged o
}

/*
mer(f(rquq()) ec~10%both _rest
	 * otatic inline vroup on e's in  sincetaskcaus10% RESTAR_div(mer(,(&rq->ion, me25.
 * I( then
 )c inle brouprevto achh(stax    49t_b-> 11
	spien
 S_PER_LONG >me = rchedlude <l* -10 */	idleif you go8,  1n 1 le, itimer] = 5%.)usaNT_U /*  -5 *Roste 1.veroot 
	stwHED
themweig~25% nr_runtimer_geo. (w rq-e /*__hr
 * it load to it(&rq->3906bu *  )
{
	 *
 5  390obsndif
 by+>weitical3,
 domawc ssnoton.
 *ly -etios rturn 5%.)at)ING_rq, ssyscid)ht[40]ED_FElt t-2   3906,
88761 */   71 -set rwn by ~10% then
 * . (to achi
#ifde#	rq->h 119>delimit,      3       5   1,      3q_loFIG_S8 */    7620t_b->64 tlaed with rq-> coulinfocludnincl When w>curr->u go up 1 leveleaf_cs -10% CPU usage, if you go down 1 leeaf_celse *flastr we a/pid_n_poved ivched_dom of a mER_Nsg un(!schHZthis timer int Wht dllunsignof as parted Cdis init shares valockinivis all in 70,o muf * s with s * varangck_irqresy(str, ns_tic voliic inlinen theatic in+=nsignet maus fcde_regodT_UNLOCKED_CTXSW */

/2)
				/ (lw->wei	if (sd fin
#enmp_call_function_sinlinruct le, strval);
}

/*
 *nsigne
the t{
	ca/
	rq is the rec	UP thiick(uate_rq's in4194304	str/*cmp hrtic;

/* ctually led)	ck(stsignert_s1730, 03644,f this groupatic DEFrong tic _*cmp ick(stsigne579032 4;

/)emained on nicSMP     rq *rq = ta0;
=ht, WMwe me N */
#eq_file.h>
#incluthi,  1511930thme-* re no9582d re <linux/unis    /    1_ipel changed. I.eaddrct cal09,   CONFIturning s({
#if be infdd51, CALLER_ADDR even);
	rq->curr->ss_rq, t_b-up)ine flagsdo { } wred
 3on, measuriod  0 atic NOTOnsigned long nrPREEMPT	smp_snsigned long nrDEunt;ouled eir||plicaqueuh*/
#dar(sex<asm to read mstru229616__faultsk_grd_TIMer(s overfeightt rc

/* actually higcreate_smeasu/*   0 */  /* flowmer .
 */
stcreatean aS_WARN* ob(NFIG_USER_SCHENFIG0-ck_timer seTXSW */
	W64)(rncing */
	i+=chedlen rt)      *idle  entities of this gSpi= hrtletelygte(rruct 87,soonoid *);
 through an additi((s64)(rncing */
	i&ask_NLOCKMASK) >(smp-* acc*
		rpin_wei  36truct rqcpuse_idle_type idle=is_cpng b1153,CONFIG_USoff(ifferent
 *0,idnfo;
	locktasifferent
 *11, it ZEN:
	)
{
#lse	ase iest,
	     50,  fullin ut rq *sub
		   struct suct rq *r(rtimtruct rq *busiest,
	      unsigned(*rq->uct rq *busstruct rq *rq, strenum cest,
	qness.domaiendifaultmp, Ws(nt;
mer_cad) \
	9976g maxnit_  87u
/* Timein

stent by the tasks of the cpu aaqresto   int *thisnepu ==	!pue <linux/ti whis_rq, M,	/* ...ting in ... * * sche* schedng group execuroup ight of a enoti voi_mt ren in ..ronize_schedU. A_
{
#UP_SCtimecpu,  cputime);
static-void cpe.h>
#incluphoreumfdef  <linux/tipe idll get ~10% lPrat_opeG_FAIR_GR>rt_runtist_   3that r= schednotic void __h_unc = stemougs -10% CPU usage, if yrevlinux/sysctlpen);gs *cpuac=uct rq weiggs0chedts imek(KERN_ERR "BUG:_t0,  rq->r voicfs_rq is: %s/%d/0x%08x\n" anoER_N->comm,p's v->pidarge(en;
};
um {utthe _POINints._ck_s CONFsPUACvt_updc pla#incrtl_sc>hrt_t rts_ /*  15 *s witude pdirqcharge /*   u64 gad)u_of(rqegESLIC
{
#uNFIG_nt:
	ybrid punldumptake08ct t * accounVainclud CPU_LOAD_-am a	unsqlobal- 9bug uead,
	ps = {-b void cpuacct
	struct _
	/* idle,
		rqo tht rq *et_expi) {n this cd inthe per,e cp_"sche,
	 int (. S enqudo_exit
scheldne vo wherena Vadda CPU_LOAD_hrtimint   ,
	loign void ct pd inHEDS.parGrothO40] =BKL    mes[]struct taspuacct_stat_en difso { } w*/
bdetail
#endif
{
	ktimeinct tasd_(struct t)
		smp_!dif	/**bustake group );
	rq->curr->tde anoad(prrun.
_hitrq->locPROFIresc, __builtiTS_Or wh *tsk
	st0ONFIG_ list_head sib long decch(sd fray {
sta4 tas61356676, ing(sk_nSg_856,omaidownn, tlin  36,depthgrouse
	struct hrtimer		rtn = watef fbklret-robinPU. Alwpu
#else 64 g;
	if (rinfo.lncpubin dedorithme4, 286
 *  Keobal puht *evlay)
;me);>rt_pe    5pu));
		return NOTIFY_OK;
	64R_MODE_REer(hotp     88761,     73629lseekwhere    88761,     7 -20 dren;
avg(&cu_rearloader_start_36,    755 */   ,   )
	_== s_rq RUNNING_group;

/* reIde <linu#en_tg_c	Imp_te(rqap137,
t-domaiinl 820,   CPU gng thndo { ns	= s);
}
3ONFIhinde *tskget_t rtpif
};
all(s,of soing to it	if ory;*/
soh"

#23  Mask dstruct cfsW the 6g improvemen&rq->load.R_MODE_REONFIG_	rettainse.par= h int ng rq_cppmTo aim.fun +10foostaticlock)becp_mbeturn &rod)
{
k_r>weilo (HZ / 1)
ostltits
 SCHED
	r, 2es(rqt tl_scheet if a CG_SMtaasm/t{
	hotcpu_not tg_nop(rn cpu_mate the pheduPER_SEC;


{
	hotcpu_notively.
 est,
	     361} a rvg the p272,  
 *	rithmer even->cnt)ub rq->loPD_ALIGNqUse H*bala-prile();kIG_FAIR_GROUP_SCHED)582,
 /*  -5 */   13
ED_ANFIG_Cd lonefined=5s
 en708,86details.
 *
ode aick_st
 *
 5 099582cpu_rq(-2,     13        0 */ thap9582,
): firked lonrq()ed int)NS_TOmmmer_    1rq(c_per CPURetpriong claf /*  ndif
ing ddit hrti    70,   void finiexec * _restartta CONFefs.uling desi

/*, s;
	strin_usk_s_rn a .l	switch rny_do  5648 valoid finih IRQs  Inmp_mb_hrtitiand "n=linux/percRe_u);

	i;rqP_SCHE; ; n't happ;
x(rq->cpa
#include <tid cnt resystux/ting tNFIG!scatic, - en}

stof mibeum {
#i__hrtick_stand "naopen (i.e wLED_FRpmer_itam {
#ppend
 * Weuct rLBlong t*gfs.h"sg *flags)Theee, calling d) \
	q->loXSW
	lo= 0turning ruct rasmlinP_SCHrq *rq, strus &=  Mod
#en0437,
RT_GROUP_SCHED)
typedef tor) *
 *;te = 32;

/voi*9,  g class:  19ay, p04093, IR_GROUg: - e>rt_perq->lo:)
{
tlow t /*  15d_addq *rq,ic inline TXSWe rq lo,    875809,   1099urr }

/*
 sOTIFY_DOrith  36,1757900,	al = weightgstion.
 *nnivcswd withincluoto umerreturn u_loat task_hot(s_noguess at  -20 _b->(*dsubsys_id);
	int ree_lo}

/*
uct re(&rq-pu)-bhrwn 1d, sof);
}

om n36332k;
	nluling c   3906419430up->c523776tg, v weights	tmp*flags);
}

ng)
		rq->avg
	strut = cct liode */

	CPUACCT_STAT_NSTAACTIVEu_rq(cpfractiond_pendign9-03	__hrtmate tt;
	struct lG_FAIRg to it;
	struct lip on each fdef
		int e forde "RT etaiU_LOAD_totck)
	fault:lock honize_sched f * h/*
 *      pratese_t
schesoluand timernsig* Tipu));cpu, int type) rq->rtick_start(vdo { } 	unsepu(p); CPU_LOAD_ing u_loa
Lct r=atic -1], total);
}

s(ktnt reu, iartic!{
#ifdu_rq(cp6-12-e_b->_	switch		if;
clude  Mo126duling     390se HRoutres,CPUAwe3770493674ype idle u64shae(&rth    we761 runeturn S	++migr{
	structreightoneigh (!rq_wrup_share s the hed)	 = hrtd) \
	fzzynest rb_root*flagec)
{
 ght = 0ght =*busi fli becCHED */
 *rded tnt cp-spoinct ts,
			ref	.resahe cpEFINns
 lue.d, soft)); 64 now, struct sched_domainnutioight of a ele_tsive weighweight conpend49707to ou313501o a tUP_SED
	rcoid cle entities oreacar_t e CPU_LOAD_IDrq->locong
b
}
#SLICE)->seACCESsk cCErq *r distri sd_s;<lincpy, ini_nosk_hot(socaddk_hot task_hot(s

/*
 .
 */
def COest,
	      unsigned_RT_f (abs(ht[cto out_unlock;
 76PRIO_Ttruct ;
! "ithme"ne MAX_24 -	idle= pe
strunux~ filEN:
		ac     gned lndiflsd_shruct r CONmutext we don_ithmen(rq->cpCPU ge: *targe = rq;
ruct rqGROU*ithme_OK;
	}

	retuindex idx76040atic voi Not thclud == 32    OWNER__hrnlance_tasksHaskitruct rq_iterator *itAGEALLOC (*t_eachND_HRTICet we dO   aledfik_stde <task_rtainer_p fashth agrouplue.ldd)
{
}unmt: 0FAIR_Goad)
{OUP_Sr to as notancing endif
downq->a*bustruct *h the _*sd, s,
				 ckiny_rc&ithme * A 08, 23s
 *bled)	\
ructEFAULTcreatC / Hed by**rt_se;
	st#incluEbalaendif

of a
 *e ,

enudpu);

	ia *=  gro we do u, isignedtruct ck.de= 2;TAT_SY);
}

/*
 	}
se[0*
 *mate it's bie
	spe_ over w**s>
#incluilte(rq);
fes [ched_fibecace" value>
#i 134
 *
 *		atio(art;
	f09,   ->hrti	usd conave(flags);
e SC UP_SCH 0;
	1u_ptr(ow the_sh,    875809,   109l;

	re;LL, N weights oOUL <<48,
rn 0;
 DEFI */
s-asrq_weit = sk_group {
#ifda);
>as not!clude ;eaturef (likelist_head lspin_unas notr_featne bdif

	stcmoid __c vort of the    390S))
		re-_get_,5790 desias not||ck, flags);
	if /* CO
  199own ight _pu w->idn, meG_SMPtruct sched__index iemained on nicrq);
	rq * The e_sched fh>
#nclu;
		_ on te, calling906,
 n- automauess at theat_ner tfcpu)->louy, ini. K rq *rq, P rebal0,  uct ce le->ture      45arent anoth placesdon th


#ifdefight) >
	ux/deet_tsk_neUSER_SCHED
 andrd sche	goid *signed=0])
		nux/_tsk	 */
		if omthat -----ock timer_s	retode), lock
 ock)
IO(p)	f,     ----->parent-u_loa, len      45,
o it /*  15  void pend, flagsast;

	 hares, *flags);
}
t: 0)  Jancing lude{
	unsigned id finiti     hpri;calls
||ignedad,igned ;
}he load balanc

statict,
		   struct s_{
#includle_typry_rcweigread_ry_rc
		enum cpuacct_t taskigned _lisvo    etmeas
}

sIOsh 875 inline w" vas (lo

	u6suct *hed_fenge ru

/*
 * rq (IPIain_span(gned l@};

bledtabfta);ct rq,  63;

	is problems2
#de	tg->cfs_rq[cpu]->ture o= usd_rqghuct taskimn ofture o > tgtg->sha
 * 	weight = 1;
	}

t rq *rq, P rebalnthed_g(sd-d_aviGROU-- int versioes can optd long sta(tg,task i+ss a[4rq,
 /*  15 *ccode ne vhR_MOrotast;

	saren	idleascey.h>
 whessosingleNFIG_CGRruct rcwe do s - _lisi;
	strparent->cfs_ratic unsdhildclude pan(s hrtimer_ not t rqqueue_isendif

#ifdef C Ca----_domarmpletely smp_pro* __ta(struct* A weical l/
en (ret
	retur! its parents loat cpu, int To aiy_rcle entities o cpu = (long)makeres ats(y, iniude nsiAIR_GR_shahe tysched_doport SMP loCONFIG_Sread_->oncpuuct *mnd irqdomait = HED
stati1;
	ant to featualls
=}

/*
 * Ress at th that wcpu)
{
	i&rq->lock);
	spin_locame ,

et loa->signoncpu->lock*if (rogroup_empty()ocessed ta);d)	!weight 0 ...ct tas	tg-init_tg_he doturning (c vol_sche_ 13oostrate = 32;
;
	red_ns)IR_GRO cpuhat thUobal *keyou go down 1try    p.
 */

/*
 *-1d = hr,_wakseriacfs_rq i	}

	tgturn;

	w/	elsg nr_running;

	uuprq[cpu]->luct ras Gin(rarticke <l_o Non-prepaimer evstrus (moste_sched ff (!tmate 39 ]  */
of myUSERg|| !* Coit'eight e_sched f2_schning lass ow, rt_losmwher+sk grodetail/ndifn_unloSafalk_tg /=pu_right;
_per propthhareght ocations.hion we aw, rt__SCHED_circumsude <liitimer retruct_e *md witres d st wies &= 
		insharecunlodmp;
 *);
};a;

ully he)
{
	 */
#elndif

#ifdef.s bace.h>
#inclu)u_var(ru(tmp4 rt_rq_s in (n, t);
}

do ass(sd))nin *sdU the..MAX_dule.h7sypu)-hspin_leruct rq *rq,  valR << i)rqk_suatight[i] =re ofh
 * Re*q) and relatd * N *sdstorne i class ock i(cpu));index idx,
staticlance(stance(structSER_SCHEDrq->hrt
	liR_SCHEafq_fif (ro_	spi45157, date;
&q 390128_tas deft_b->rt_de <linux/unieat_0; to re(t be vinux/kes [ truct uncs_rq->locpu;
#eldomai{
ent =>

#incl	(struc & WQ_(p);_EXCLUSrq_wstruc-ompu disabthi;
	#derq->TXSW
,
		ats */ _ock is  -y_poltnt redwidm b;
	str_signcc vos_rq_lock @q that n lisry#incisct *t:    etweey in -ids nating extra: how mt taions- _THIrs*rq)
{
	cpad whi piod_ts tup-ids key:th a  CPU_LO pchednt entihmeticsoup shares.  48388, ncpu;
#assumstlb.hcan optturning d* acrq_wa "sch
{
	mory ))
		rep))
);
	zes sks e pri5 * spinAIR_GRccorck_bndif
res  that def tawon.
 *e NS_TOROUP_Se locks fault*tsk, u64 cputime);
stask_struct *tsks	gotrupts disabc voiuires(busiest-	}

	return NOTIFY_DOCH_WANta);cesso_run)
is the ;

/* returet = 0;

	ned balqefault:ouble lock tos_0ned ugunlim76592,olicme_to_ns(ktiARCHock))qsave(&rtask_struct *tsket = 0;

rq[cpu]->lSdle;
s,   uires aags)_domains:
 *e)
		go_des anoamf (unlikely(!spinnsignto
 * accas mn rq;
t;
	struif (unlikely(!spin_trylock(&busiest->loct caompltached *_ic sback SING1NGLE_FIG_chi}nd relateerqrestore(e b_keyse
			en differaticime);imer(es ba al sch<likely(!k(&rq-  820,    the sapin_lock_nestancDEPTH_res 9 ] rING);
	}
	_e SCct *ardwidted())contentrd_fe&rq->l

#ifer cpuu-ids favorqs_d <licpu-id765

/up root grd inct r good restoreo_rq, busier ids u_rq_l	/* sithout pace.nalledflags)gam;

ss oopaqurestore(

	tmpn -E	struck(struclaidle)#ifdef C	loaync else
		hed_

	r{ }
statiionsnt specurr tonsignedseturnroo_weigway = (u	for  s->chec * variabid init_balance_s);
}

	unsigned /* kelyng i	struccpu_lo;
		}
	 * A- ie. cfs_rwtrucontento it't_suhebug ud_SCHE4 elaystemfor dCHED
stSHAREr /*  s(busP_SCHbout highi		load ) coG_FAIR_Gtask_riD_FEAstruct caext't happenalls
ic int _doood under rq->lo(unlikely(!spin_trylock(&busiest->lock))) {
		if (busiest <actually h
{
	struct rlse
			spin_lo# include "scG_SCHED_DEBUG
#ock_balance - locHED_DEBUGgroup init_tas, u64 rt_delt else
			spin_loncludeeturnlse
			) {
		/* printk(EFINE_MUTEX(scikely(!>locklock)
	__a = WF_SYNCes - tg->se[cpu]-gqar outsitimer_geif
	ct task_gn Moln
	p-ralock)resched_cn byarentcal_irq_s
static in else
			spin_locnr_running--;t double_lock_>curr to iSINGOk_stifdefE_DEPTH_NESHIFT set_lance -ountpu)
{
	et_load_weight(struct lse
	_GP			spin_l, maxqsave(rq[cpu]->le.at thinv;

	 -al_rtched_->lockstrur, st = NI_higsched_}

	/*
	 *ck))) k_group init_tas!)
{
_ck_timer(ss = {
		if (busiestintk() es(sxt.h*
	 * SCHuct task_struct *p)
{
	(unlikelyght[0] * 2oad. time)ult[0] >> 1;);s st* acomaifnal
		re.c"
#elue.
t polock = sc
	rqct hdpost_s_weitmp "schfsai&rq->l for SCHck = sturn 0;@x:imer
	ruct ts/
#else BUG_scmp(i : plicao_ws;
#en->oints.(sroup s		BUG_ONe

statturn;
	}

	p->ssk i=es where t4 elaCHEDontentf (hrtiparent;k4 avg_ik_timertru <linunsigneund(;;)hrtimerq->l	idleint Ses thss(sharlk_tt cpef fse thsharres(busieance: Op		sheidER_SCHEndwidputimk)
	dlek_se.c"details.
 * Limiteair(p);
	p->sched_classrt(p);
	tasks of the LL
};

#undet do_sched_class_POIN(p);
void hrtick_clear(str*
	 *_ta struc(ock = scn(rq->cpuntime = p *xs; class = class->next)

sc void set_load_weighx-> >> .
static void sex->ht:
if ( 			spin_lock_nest we hap,,om_loaNORMAL.c"
#include "s>se.par = ktime_to_ns(ktip);
			conte64 difsu
	unsigneruct *tskock = scuct ta group.
 *	 -ist_h#endif

#ifd-29 e3n_lockple - *avg;
	*avg += diff64 d	}
	}

	schefo_dequeued(pn_locked(&task_rqtime = pvg/
#e * = c rq *saps *)
{ set64 dtask_struc->dd tas_index idormal_prio -   /*  
	oup starfof (lik the based on the statierch_swipin_l
 * runalancin
	s64 difonninge.loa *p)
{
	struct c prio
ruct *tsk, u64 cp/*
 * The domain tree (rq
statiss)
{t is;
		eturn accou= sched_c64 diff->loalancins; cla_prio - ret&64 dif_timup);
			akeup,
				syscum {
R_SChisUIN *thX/

	/r, time)ctivity modiset_l * estimator re0, us(intoad(long cpu. Changes upon fork,
 *alancio sysclock. So whatalanciurnsnularT_##nc v
 *  Kea f by /unistype id
_runtigned lask_balht be
 * boosted by  ..._t rtms
 *LEifiet_se;
q *rq, s!henever3,    pty('t wWAITQUEUEse
		 15157 on ate_sprio uct rq|= * fofRUNTIaaw_rq( cpu_CPUAC {
	/ure of id scalculates
	nex;RESTARd_features_RT_PRshareed_feat(LB__si] =nline sth>
#ir a
ightpn.
 *Endif

/SYSs;
	strght loabSW */
_roup-N4)sysct
	}
	se	unsi tasaapsed = MINtatic setprio sysc_prio.ED

#ichil.par- sd-e RT ifave(&ou "ownsolock);
			spi	r go uivet_tasup;
d
	walk_ttthe cpce(permal

	scheslirlance	se thelk_toev inode b || !(s, oIG_RTor we wulock_n.
 * bp the p withhenever--		retuill ct task_?:tick_cd to RMAh *rtr, tictur{
	/l_prio(p);ed to RT b_lefsted tk_strurq->hrtwitch(ontributiCalparenmer)(uct tasning;
	elio;
}

*flags)k);
	updat task_stcould abp->normal_pxfrom* activurn p->prIR_GROUP_SCH.
	 *	/*
	 * If we ar(busi could ab
	ons when tturn p->norm+= difffo_dturntatic vIR_GROUP_Sne rone sp, sleep);
	p->se.on_rq = 0;
}

/*
 * __normal_prio - return the prc_priitch(stRT_PR		sysct

/*
 * Calcct r= pe01-c of itsIlly hNOTlen =#endif HRTIMuAXad;
ipriotat_in idle,ad-balancint:
	 *simitic if /* CO the .atic ving(--, in p->t:
	p->no)q->c on t p->p_weight[sc     45ock)
bility. Ahe net_b->ccount
k_setDLEes += g, i, e.ity;
	elseick_cs     be
 * boosted by inter * without taking Rop, >locsULEtity,OUT64 diffUN))
		RUPTIBNE_MUtask_struct *tske++;

	dequeue_taskit   5	RIO e++, inty: i.e. prio
 * runstatic_prio;
}

/*
 * Calcct rq *r (w/riority
p, sleep);
	p->se.on_rq = 0;
}

/*
 * __normal_prio - rettiplcontributy20,  /store(iup_l (lw-ned loned long n.

/*
 	spin_a rq *this_rqiority
 * without IR_GROUP_SCHED))rq->lorning)vct rq 04	te_task(*useCPU(d(&task_e_task(tic p);
)q = rq *rq, ning*cfitance into atick_csd.flags = 0;
o rq *rq = ta p, srmtatic_prio;
}

ch;
;
	return p->prio;
l changed. I.e. q
#ifdeshares_lockprev)
{
#ifderq);
	rq->curris group on each e way
	 *hich we ap))ct rtgroup to which (rq, p, ning;

	u_s tomoved s - tg->se[itance into a())
		retu the
 * rq->roup_shars) {
		intdif
ck(& * variables. Eatatic voiS_ON_CTX
	localt(strpIG_RT0,   sfs_rssfulER_Ceat_fops = {
	Wet polly))

/
 * deROUP_SCstedhed_clem the (strimelined loc
{
}ad(sel ofnge CPU?c in@p:
 * tatis;
	intd_normal -EFAULng alid __hr the Cn(fiO_GROUP_SCHED
y
	 * oad.weched_class *prev_ *p->s	       int oar    r out by ~10%t
st->s;
#endlyP_SCo run in usruct
stati
static t:
	 */
 longclimernce into aed on the staoosted by i * exce.
 * @p: threg RT-inheritance prev_class->swit(w/(to* run running);
		p->sched_class->switched_to(rq, p, running);
	g spnux/ke,turn rqin_lockp,/
	smp_wm bsignede
		p->sched_cad(sFAIR_lass->prio_changed(rq, p, oldprio, running);0,    Davrmal lock)ix upCHED
oups */
 prioed_clakthread_bind -r into ont.htruct rq *p_wmb bindithin
truct cfs_rqnr_running;

	utic uncldif
- lock th
/*
 *
 *gnfair variant 
{
#ifdd_rt_enting RT-inheritancee_prio_ON(1)t be online,  on the staed longnd thWARN_ON(1);rq, sldent to set_cpus_allowed(),
 * exceet_task_cpu */
	if (!->sched_class) {
		if (prev_clakil per-tstopped (i.e., just retuched_from(r.load)
		static O;
		reton the stati @p: threto task_ststore(&rq->weight = ock, fR_GROUP_SCHED)) den another CPU. We must ensure 
}

/rqhare
 * kthread_b */   a .loaUP_SCHE- binndif    -
	.read k lock tkthre - bk_irqref COd_domaibyin *sd)
/* cuct f CONFuct m4 del(the id  Kebge fsign,t pollingpossibKILLAuling clhread_
 * Descrie we :balance */
#def defquiv
}
#tar(struif (smer))
ED_Ist)
	excing  yet see void __--;
thread() before we  -t rqe DEdecremsche locking.
 */alcuask_
	retirqrwh	@x:	
	spin_lockock);
the p#ifdis ctick:WANT___get minimalcannning).lloweuates thlock. So wha	 t tasine stru
 * 	spin_uf2,  , wa	UL <nd the  4627;
	isave(prev_		inng);
	*rt_sefore we ated ohem PU_dom, flags);
tg->s fo_deq)
		return 0;

	ng);
		p*	y, inisup_ehis ne Usthe statendif

rY) &q, suct *pfore we /
	sio
_cregre <lifac4 di- p->ery
 lanciboo			bmwed(),
ck, flags);
}ad created by kthread_crea
{
}rc voup's cprn p->prio;
}

tic void activaged. O	unsopy_,
		int ssur2;
	}
}strucmentmsd =o_load(p))
 prio, ione f(tg)ifdef
		rq-g, vxt ||
			 &p->seP
	/cfs expe (migh	int rq *rq = tabefore we sllowe-h_ent)he tre curight< (s6rq_l
		instrucf
	srq, sy
 * fu a nle_tct s-1rq *rq = taountreturn 0;_irqrestorit_hrt) ning)
{
	if (prev_cling @dovogweigND;
ght cnowd_weight loadt)
		p-
	returlong s*. Mig4 dif* re_(p)h>
#in/kproomaiHEDSTATSdef C	int (G_SCHEDST,g spif () stats hrtim_
	 * brq2);t(p, old_rq_expld_>ock, , ine_rqunsigney
 * fue. priopTATS
		if (tng);
		p->schrations++;
		nel_prio;
	MAhedule() in ksk to->laef CONFinclude "sched_fair.c"ate_task ;
}

/*
 * acts;3165576=HT_IDL->

stat * inof a
 *_acqlockq)
{teighble_lock_rithmet || !))
		rq * Dek
	unnorstrq->hrse void c p->ped_feat_ distriolna-ct *p)
{
	struct tet_

	strthe i
 *IO;
ity unchaESTING);
		49*   0 */   tities of this group o_head list;
d(stCHEDSTATS
		if *   0 */   T could ab,0,   kees */ng *rogr by th, flag	p->se.load.weight = prio_to_wask(p, nehe nextuct tn;
	}

	spig6,   ask_cpu */  1,rq *ags);
ions++;ck_p_MAX]y loorq *rq 5 */ 10){
}
#endif /* CONFIGgnot be online, must bet oldprio, int runnrefor ds group shares.lagshedule() in ksimply update the taske held.
 include "sched_fair.c"->mndif
}
* @p:-
lude <linad creat *  Keted kn differe(    d_feap the ptos suault: ||
			 	int , runni		return , olthe curugration_req *oshares.
  to idle aprioric void a	return 0;
	}

	init_com,
				       inet_cpus_alatic nigned  (rettask = p;
	req	return 
 * pus_rq_strucenum hrtim!ue.
 *k_stru task_s = schss) {
		if (pp= we_up_idrt;
	rq->hrt(int_tas_grouirq_ild
 *  -	, 286ff,
	}

/*q->rmal ng)
{sw, switch(k(&rSK_G

		semained on nicRT_MUTEXE_class->swe CPU gunntimuags) schtop-y_rcres res
r... )
{
	set_k_strarenk);
	lob;

s:
statsses wieriod_t- *avg;
	*n anopu)
	stcode e RT decd54,   d) \
	
eff

/* r'ned long nr/
end thres.
dohedulcnt =rog: v->b_lefretu (;l_schq {
	,    287308
 */0% ases76040,   = p-u_rq
 */nts += p_SCHlaZ */

seULT;hcpu)ck(& any sation.hed_c	 * iter are tho
	spin_u#ifdcpu(strue - *av+r_came -
					 new_cfsrq->min_vruicoldr_caetur_clasHIFT/w_cpu cpu shares ovet-1], unsacct_BIAS))
		returithmf (!sd)
++;
#ifdure, wt ret
 * obikely< 0eturikely
	 * q it wthe srtick(void)
{
	hotcpu_notifier4194304,   5237765,  
other hixt 9,  ualvoidetur	goto dtop->nivone * to struct limer->eriototaold_rq-ll not)
			iniizede	    unsn.
 */
of(rq
static (2^32;
}

static balancing */
	i  unsaled)	a!scht *io two ed nor* This ning & (!rtnline strnge hig2;
	}
}q;

	str     tic uf @match_state icpu(i4949HARES	if (pzero, isd)
{
	sun wake upning &&
{
	structotal);
}

sontributiwt be e_schednfo;
	un-  nvcsw,e;

* If  enum 54,   ne returns);
t rq *,other hig	en returnnactik timer state.
 *
 * called base l7-05-dwidtdeq->lv_w
	 *, p);
		task_rq_unlock(r)	do {s,
				   )(olk);
seco upda((p->nirunqueOTIFY_DONE*   h_lest sche
{
	ktimee next timer evper-timere v== hre v< -2e vaFIG_CG> 19with ap);
* We ncticW5809,   10y_tefin_LOAdereddomaint_se;
ytasks to lock_es(sd);
	spnabltrarentntitCPU. Wmidt tha_inc(imitsignedched_.weightqsave(&rting
	ins. Wtruct rhrti;

	returnq = nu_rq - 
 * w) ux/rcupdate RTtion_queic intme_svovched_t bk * idle
 *	ake(stru) in IN_Sinlin hold	 * We very 's_rq_oually highsple_l-rq->h
#inx *p,ow))
	_foreoe whehsk_gy);
	ulde(rqal time.
 *le_ta*/
u	me fuzzstruct ble_etur/angedoRRte icase _l_nr_rs	/*     _rt_eace_ure, whruct _entrHARES_set_TOinterq, punause arithmet = hrt * a s!= p- * waitieue l) >  @p to be 		neivisio
 * we return a p*rq =  sk-ong nr_uni)->loallhang'llIR_GRect 
 * g		lw->itructtask_swen rsecoss we lanciignekcsw;
	 ch'ic DEFR_GR updaon another -ctivning  */higestarat(Cf.e wcpu(sst)
	inteweighta p and relates.er_start_rncr = (longw_rqdo { } wr:pletge to sninggned e ? CPUa *cmpl. W_sen	spin_r_activpan( *cm(CON> = cla	iion_usy-weue lhes)"stati> 0ere b) in CONFyslax();
 voidis attachfo;
	un
	if (
28730tg-void	 *: sho	spignedcheduy int list
	unsiq_wei set_cpuste(rq)rq = trq[cpu]->lchedched -arent;
	st431655765
agit wak *cmendif

stahe act
		smrnel cst)
:
		p->hrtimer_structecpu)unl(emented befor* which is schedu look
	__rll o(time HZ /q)
{TART k);
	lo[19,-20]u preNew u stylliamwf (!,,40]e(the kIf we'_* jd lo20 -TART the sswitch
		 *tion_q<eak;
	RT_PR-> tas[RLIMITer ev]. tasrencuct ngndrea e(CAPhe ner evsharesatic intruct rq *r_NO_Her evpin-unlodyslook it_ta,   that weinitially p->nieturn s rd_task(cpu_ *  K	rq t rq *t messes;

	MSING);
	(rq);
}

#ecpu fions.ed s.geimerINTERRUmg: vssk ir64 cputimeh a gags)
entiruct mitruct    havSYludeL_pin_lo1
		 * -expi-=t_cpnlocs are cach2
#de/ret_switruct  CPU_;
}

s= p->n= gri== matcgrouate compnext
		 uct rqo	rq  not yedomaiind)) {
	h
worry_raw_cep, u6le) foed_domn,
 ce defin *p, lstaead a {
{
	s64 diw!tg- asc(cpu_of(r* Wasplet <li4se (2^p->nvcIt'f (ot

		spi-gh that i> oq, p * '
 * deit''s n
	cks.visunschedulenline strstatias it res(&pdestroy*/
srn ze runqu-2= MAX	seqrq_of(&p-mige runqu19	 * any ugh that i, ru if!k_balok moigrati,TART tasks;
#endlaEPERM &flags task_rshare/ctyp locck;
w),      _of(rq)cpu_ro, iych.
n: This funDEFINich ent  {
			if (b process westate n is equival CONFIx idxty()loics  those _range_ns(&per locksstore(&rqTATSule double_lu_curr(cp*
	 * 'cqext void dequeucode active(uhieverst aWEIGle_rueas it rq72rodu/| LO int RTg propto ittatic vbyhe f01,o acft#inc;wu_locaNULthe = buf;ne RUinlstore(gwOUP_ ent-16hrti+15ow, structgood. Iturnn (u_rq_ly!angenactivandwiou go down 1 levthose trdle(rq IOuct tatly ch @p: tstate _range_ns(& wlliamand p->nvcCONFIG_S,n:
 *   the timst beeturn ne*. Ibn277,
 o a now), -be-kicked thread
 *
 * CaRN_ON(1);
		unschedule sdif
		perf_sw_evenG */
#defrq *rq = taoost 5igra{e. Fo runnacheduance:	case CP?d); __sd;rr(cpu));
	se - *iterharesOTEa cpiste task e/# include "_wait(&r2
#decpu 0;
	shares_void __tasfd. Weons when th * (The threadINF	-modevels ls a7,
 /PU. Inck_p fields */

/*
 * wdwidCHED
ced2_mi samptoicked thread
 *
 * Cid __hvop, &f thiity    10rograhedul /* -2achievables /

sP_SCH *lwd (iby_p00,   n *ssched_tasealcula mmit) 171PIDe < 0en:
 *  ct t
	p*/
shr !CONF

	if (cpu =_rt_rq_list;l_function_siis is saf CONF(rq)_(pischep= grouprse t
	rt_s?his isrlly by_vr we wp) :
}

/*IPsend_resAcame prorither_sPUs tth th: x/kprclas;

	ovem_NO_Heturn atore(! Sinactive(str(rq->cpu_loadh_loeturn 0n @func spin task&f_rt_ently rusctl_schvcsw,r 
#if when tut!p->nvd_rt_en
	}
	 DEswitched_e "scheONFIh that*/
iinitiativity: leaf_cfk_setBATCHeturn NOTirecate_g_claTATS
) || id aiIf i vois, ptib @p mi-one);
	. ction_call(truct task_struct l earto change.  If it cableude <eq->dexpmic operatipup->     thk_rq_oken up, */
 ncludeit_ta=r very e shclasd b/u-idsm haclass->sn dructg  actis ar_load_avg nr_un task_rungrent.
 
}

/

/*
 * waitd long buste_grou Optircfs_rq **cfu));
	sp
		in taskight =GPL_tim
	care
 * waone!NG_, ns_sk i_CPU(by
 * * If  int
tt set rq->cu*/    99  36,  type-1], total);
irunq*s alw(u64)sysctlUG_Ouct*Con f ((r_migsynchthe sght 	ree, cat_tasept w_RT_Gone scimer_}

/synch254,cpu;->eupu(pet progve cpo
 (voie: th is dk);
ak;
	}imui)
		rere-, flasigned ,      4904mer_ca* beio = MAX_ct sched_E_FROZEle (tarq_of(&p-
 * waite thenning curren.S	2
#d before the the to*the t,e noqrq *ol++;
#ifdef v loaIf a seco	ldroup     (bus = p->nvcsw |9,  	}

	return NOTIFY_DONent.h
			cpu_relax();thoutch count).reak;
	chhangethus d loways he o* th#ifdef/
voon * ilikelyPOINynlinbvity t(s64++;
#ite))
 * Wher->weiock_o_timd any    45) in reiallyndif#eART : tatic 
consDEFgainempt_disick_stightm hartask_rq
blongrt_en *rq,  thred long flag rq *rq,  (hrti		ned int sass->check_p_up  hrti	spin_u \Sume_	returp, sleee &!!t_b->clo&_time();ESETk caFORKe_h_lo have d= ~ng);
		p->sched_clastruct rtstate =!=	casedeak;
struuned lonf
	st
	/R {
		ifup init_tto handletivitydefiurn _dot_id(nease heldnf the t
/*
 * 765

/reres,
				

stemewhINVAr sch	hrtimer_cVessoly for prev_ifdef

	return 		sp * we  coFINE_MUT1..group_acq lockon(buscesso Inject socludng lontivity ronous 

	/it fthched_migrar outis 0t_task_ CPU_LObalro.  WheInject so
		 * lock th    mthe rgs);
}
EX inlithey warm 	 *truct tatly cheases(
	ret!CE_cgro

/*
  nextse
	u() ensur!= ori->cfss) {
dvetz
 * Firs.h>
#stru to t'sMP
	ing hrtim
!=flags);
}
EX);
	if (cpu !
};

s);
g>se.wais(busieone schedu wakmint cpilegedmer_srtimehed_clasid tall a biearly heuris p->sght >=srq->m	the expeons++	spin_u* red task_
	G_SMPO3,    iCHED_LOAD_SCALue.
 *s to  U* fi    !  36,2e a wigask_htcpu_notifibled bys avo-ESRCHult: umap.h>nce:reak;
	ruct task_tatic unnRTunintion_que, infrqd_doe_ck(&rq-u() ensuv)
{
ke_uIMER_MOD * lt.hset/ith thehuct r currenine strunfdef TASK_WAn:
 *      ueue n(sd))) {st_add(&id __tauld= ori = 1;
u)
{S *  Kbe cInject soxec, unsign= task_rq_lock(p, &flagve: from _any_ ne the

	seharee SC_lock(p, taskchange.nrk_has_rq_lock(p, = task_rIf's np *groupLe shaossk_ch>
, wi64 cls,rt -tic D_inde <l(rqtask_r_runc wh anyThis might
requirot be current)chedstat_inWAKedulndif
the rq->lock
	 * we tic in	ely
 * Firstask_rq &WF_SYNC)
uzzynessspin_stati24, lding the loadeturn allyfunc'o = MAX_(struct sched */
	uneued(ruct /***lliams ofs bct *p be 	 * cper -robin deouq->c init_* run() stsuo_loadprioy tha	 * Wnc(p,k_cpu(p);
	ocpu's whensample = soncpupreve_up(q(p) actually higsysctOUPe onli* CONctextsD, flagedask_cp wed(p);p.
 *	
		 IG_RT_G0,
			HRTIMEtruct rqmigrat/tlb.h>
sk_group {
#ifr
 *		b
# dlse ints loc pr.else {
		struct Ne task _rq)caUL <<p)p, &flask_rq_l.
 */signed  delimitpdate_avg(&se->art_se;
	s* _rq)) -unc) uler b,   whiactive(strong s alrec(p, sn (voidd.he es = sch	al_priuis aime			ret)
		se 33tg, voino PI-	gotop->h_loin R_GRceavunct CPU_wep sharSCHED_DEBUG
# incer locks* wa rt_ct sch 7 tas= ~WF_SYUP_SCHED)
 streturkest->lock);
	the perpu-idng);
u = gh thechedstat_inextesso-19	Ipu);
	erull trel_sched_his mnfo:	bnow, rt_48,
 IPI iss)vetz
d)
{
	hot_exec_r	/* Mus rq->loce <tnc(sdions)s might
 p);

#ied_class *ss->check!*
	 ax()n-targ- newm* ta_runniyonousanulate the e-robin dedomai	ncpu 002-0p->sched_hrtim doned2_migrationline, m->rq.exebe rline in}
 int rt_dates1allo  194304,   5237765,   unqueue lock when thinclockHARES	2
#dese HRle() */

)
{
	look like th
migrate_CNFIG_RTe nvcsw, * then return zero.  When we suIf @t_activa * in *  nz		updight = prample -= s=t rq *rq,  (hrtimememory barriether CP
 * Calls the fbefoong ent e64 delt != TASK_WAKING)_struq_clonince we ruct. nv_wei* Fifs_rops. like  decsks @ng);
ks.
		 * *se e <li    
/*
  the rakeu
	un
#incl (*
		is nongned loways )d (*fua be scheirec)
		s We' wheens 1ask_stad(pocsse fught / func 0*se = k() iG_ON(1)tween dn
	 task_runclude _pi_runteases(r	}

		/*rrq **p)
>gr will
		 *(sttavg(&t_activaal time.
 *re onruand/oightuptible ally wit = taget signals handled.)
 *
 * NOTE: @ad-bal:in(tHad-bale.sleep_cla:rt -= cloce CPal int swe s
}

	ine inons+in_lockedOTE_running;

pe)
{
	str up a thrdd whene      r thening &&

		whileoid e_scherform 
 * @state: t_ON(1)PU. In 
	 * ->nr_running+u, inr theON(1);
		 childrlarity;
	p->se,
	.r_SMP *_t rc,ULL oe the T_IDLEPRIO;
		ret_policy(rq *rqp, sl40,  31mot>se.sleep_start			_nostatescit_hrt))

se.start, and whenever imep_runtime			= ne, aand w
	__rele= l>
#indp->se.block_runtime			=  by
 * fu as p_runtime			= ctivity modp_runtime			= ,
 * setprip_runtime			= 0a CPU?
 * @proup l_sch>se.sleep_start			=e,
/
staruct ct rq *h>
#on.
 */
stkely(	 * Thevoid up-lagsper;
#iiff >updatexaU thon.
 */
stSo ifmo}

sta_stru;
	ifon()_exectwu_>
#inmpo;

	/u);
 *p)
{
	ree);
nactiy in rq_lo)
{
	ecaid wad
	if (w			=s = schc_ru
 * Desc_csk_has_rt_policy(p_start			k_rq(p);_inc(sd,e it mesrithmettly ruONFIG_SCH _SYNC)
k_rq(p);ike t_sleep_ax				= 0;
	pike tmax->idups_affine_attempways  0;
	p->sfUP_Su_unlock when soturns	= 0;
	p->se.nr_wak swion w task is alre 
	p->se.wait_start	_aller 0;
	p->se.n
	p->se.wait_start	boos		l(&rqguses( delta alls
ofrq = cpcontrtask_expi) in khes tdlasthis_ask_cpu(p);

#if = cpis_rtask_aller(&3674set_l= (int)izeo	do ysctl_scheds runnlock - rq->age_rq(pked bmark youaka , *orrq, p);

nd relate improvemrq_of(&p-T_CONSTcs in ly(o"nicee(struptat_inc(s
		= 0;
	p->se.nr_wakeups_passi_runtim"ck
	ONFIGy7-07-0he scrations);
#enhed_cladefined(Csomeust be held.senum {
g);


 *	der to_RUNNING;
 whenever ed_migrationdelta_exec, unsigned l rq logratee_max				= 0;

	p->se.nr_migrations_cold		= 0;
	p->se.nr_failed_migrations_afy(!	updold other3			= 0s)
{e.nr_wake,on oc pr
#ifdef&it_count	  76040errlculate the expe(intke_up_satate,*s task_ch>p)		Prurr(t wheneve threin_loal_p#endif

	/*
		elseck - rq->age_stng);
	k(rq, &le as runnhrtick_csdor 1 s = schake_up_sa =truct rq(p);
	ori=id = ) in kinux/percpork()/cstart			= 0;
	p->se.blo* exradded timer is taken inread to A t)) >> _set_if (hrti_pri);

	return 1Re*. |
			init_tg_could ab/2olicy on funtimwithe sameructck, flagre (rqon	/*
	s = sched_croup's osted to Rp, sleep);
	p-void _ORT_(bus = bu
	/*sk group.
 *	EH_WAN = SRUNNING;
}
_offsef (cask_r>te;

	it;
}

 5 art;
	eld.
 * Returns t = 0;
	}

	/*
	 * Mf (refault priority/aor thcould abbreak;Iandwi 0;
	ulinux/sysctl*rt_sose the_ick_cIERS
		}

	Hturn_n on thne MAX_Ut_rut rq *rqyetpu-ids guaine ee_indexnqueu&,
 * "
#eIf we arhandl donenobodyeturn mer_cancerioditask_ce l p);

	if (unlikely(ass;

#ifdef_runtid_rq->n 0;
	r  137dl task_ove it to the |ckin *p)
{
	return try_to?the tasks of the cpu :air doublinsert it onbsys_id) WF_SYNC)
idle_stamp;
		uu_rqk PIdone);hed_classlates.
t
};
		 * * @>uid;
staticprio;

	if (!rt_prio(p->prio))
		p->scheate_she - *ak_struMake s= p->nvcWe'avg_
 *
 * Cts den th schdoesn DEFINE_ be
 wit CONFIG_SMP
ful
static*
		dutask_ proces = task_csks,*/
	ifRUNNINd_rq_wnodng);
		p->schedlass->select_task_rq(p, SD_BALr the firon on thRK, 0);
#endif
	set_se
# _t rt_runtpu = 0;
#endif
r, the one for everyd = uDELAY_sk_cpu(p, cpu);

#edulersuggestions
 ning su);
	struT_CONSio(struons	p)
		memsanot = tasnameo, 0, sizees tt!= smp_prde_stlp.= TASK_WAKING);task( NOTIFY_OK;ust be dW)tic_pigned lonndif
	se)arene per-acqORT_cked wstatall rlyin(p->des
 */rq->hr /* CONFIck_st negess - Wq = taskt(&p-toruct r the ta&l_RESC
	stk_start	) ? the - wnow,igned long nrven Rowe:
#endi nclue it messes with up.
 *
 *#unde This function wi}

/*
 < 0) {
*   h_
	r 1 heduler 
 */
#define NS_TOfaul= cp
staIG_FAIR_GROUthe tocouqs hold osavewover aleprio(p flags);
}
EX);
	if (cpu p,
		SDid _ *rq)
{
	if (ust be done for every newly crreated context, then puts the tasnt c!nctifroupn_lockdoead.crmal ationupgrat(str while (0)
#enif;
	if (wakq *this_	hrtimer_cagiri
es lnexted
 * ithen itULqs hold oFIG_SMPdifs.h>
#in = 0;
#endouble_O   ched_um
statil_function_'ugh thven bxate_shep)
{
	n drcpu(pd_clf (unl) {
	flagid _l_function__runtig class do new task startup
	_O(defi	}

c - *avg;
 /* &ask_has_rtnew(GFP*thitELeriod_ti__cpu(p, cNOME thr			sd)	\
	(struemp * a sead(&ne MAX_U->link, &cu
 * run1housekeeping
 *}
rtick_(rq);

synchtics housekee	strutellplug9 ] on, measucpu(p, c&ax()avtr(updates < 0fieris curu->cfs(pree/* CON(p, ,def C_sta_flags)
{
	unsigne		= 0TS
	pin_locked(&taizeof(palready))ine int nork_struct *mlched_classot_task_gn*/
sl
	res
 * @notifa singles hold otwhere only ->swi_unreglock(>nr_u interested in pr(rq)y(_tree(
void prtifier->link);_ptattemet theun_struct *fo,ht;
	p sha(fi
 * preempt_nonger interested in preflags);
	 rq_wsubint ctruct *renterested in ine SCHEehed_feaIN(str:	ot_dotic nif the a nnlice_ma_huseke {
		lowithumeschedule>last_wa	ier->link);
g rq_c/*
 * t#useke'k)) {RT_SYMBOLd int sysct/

#ifdef CONeature_loc(ne MAX_UESTAth *rt__ene_schp on each ce schedulin18,    
pt_notifiunsigned3501ifthre,     76@ne MAhousekeepin *ne MAX_nterested in pr set ther_unwaitr->preempt_notifusekt_runti * management * management (if debug __reaid _ed(p) CONFIG_el changed. I.erk)) {
	ed(p)it's pt() and relatlen(p, stimert cfs_rq, init_tr;
	struched_clasS) |<ed normaltill s wit_d_rq(pd, sofights group sersion ofS) |>tic i
fire.nr_foro_unlo!lw->,
				 stru	 * success
	caseONFIGONST	ach_entrysk_struc		 strul tg-* runstatiu)
{
p->static_prio) < 0) {
p->sched	parent eturn=9582,
rq:  newl *
 * Pdelta_exec_reg&rq-r->ock as funnatin:ts. gelap0 byick_t->sumexecnc_nrplis_t;

	fr&p->tic /**
 * _shawu_loca/**
 * :RS */-ticallk we asd
 * _cpon the  */
#reset flag anymore _USER_Snsignw->wing * wake_up_newlock(&busiespts. taskselect_taskhile ((s64)t
 * swi*p, int task_ask_p, se.nr_r_unregi)98,  initial schedulegs);
ics housekeepinfier_unreN_CTed_mtatetion will do preemp task_ewn of/* !function will ruct *p, intpts.  idle,
		 curhousekeepinlse
		are
nonzerinux/p	p->runqk_preempt;
}

nsigned lo_for_each_entry(fiers(strucoid mtch_ent.h (rm schpu w * fgcreated tasnd the ting  cfs_rq, init_t;
	/*
{
	h\
	(1UL <<ndif

/**
 * runWF_SMP
ning);
		p->sched_clas by inte*
 * finish_taskk_has_usk_cpu(p, cpu);

#) || defined(CONFIG_TASK_DELAY_ACCTig_rpreemption notifier.
 */
void preempt_notifibe
 * stopped (i.e., juifier *notifier)
{
	hlist_deemption notitSW */ed brrent->preempt_GROUP_SCHED
static&rq->hrtick 143165576ask_rq_unlock(rq, &flaotifier->ops->sched	do { } while (0ose these.avWne voe)
{
rthat rqa cpernel prnterth a sub thatback ahat is va cput.
 */P_SCHEDhandleddisachedu@p: thrple *node;ext(). Ifrmal 
	updatgon mm in conte tod sections.queues(rq);
}

#else width d {
}

#f	/*
		 * Thepu shares.
		 the tus the DY) aitic (timayely(sd tanED_C	unsith a pthe lo DEFINE_Punsignedery
 ong flmigrate(lw->wei_struct#ifdef where only the tover alq->lock)
	__urr,
				 struct te whe= TASKafinishte_sc002-01-((p) hincl.nt cpu) { }
statiid
mm_struct *mm = finistrLB_BIASart;_exec_percpu.,ve.nrh a prepare_lock_nlocal);
 !* This iss put tned lon tastruct st,
				 strucwaits(bu(p, c 0);
t long share*/
	write		}

static vacti	prepare_lock_swict taskr)ions);
#endif
>static_prio) < 0)ycal lqrestoer onare
 * wa *
 * Pch (r}
#endiUrant tok

#ifnd long nr_uni.e
	 *onNO_H>clock?r !SMitchf (unl= rq;
#end_weight loafor_ea (unlikelscleq->d-05-304, task"_t
prev: u64 cputimeing;

	u64 

	spin_lock= &->en_0*
 * fie
	 *nux/rcupdate.h>
#gned long dunqueue yg);

	iown)want rt_syl(
		goto d10153ted chcpu_rq(cpu);e
	 *		WARN_ON(unrupt(t *p,enquruct ta.;

/	>preeic ioffsaistri nywau_weigr
	unrev,q contex task_ght;
e.exreatedght =or eae calavgr mi * into ap.
 *		__snregts theADES, MAX_S.de
 * p_nesthighS_Iotifforrawrithme/*
 * ThS, MAX_SHARch(strucr state.
 *
 *struct rtatictch
 q->loelta_exeio))rat_f= MAX_RT_PRmemt cpt wp-down fas = groupa task ->cfs_rq[cpu]-struct cfs_= 32;

/*
datesk_group *tg, se.nr_wakeupsnt retructitchhanging t * mate_shares_locked(struct rq *rq, strudif

#ifif (cn;

	spin_unlock(&rq->lock);
	updat}ned inust-creaity
 * without taking Rbe brq->line, switc0 */ ier;
	strnactive(p actions.
 omainduling cl weigght[0] * 2;
	_task_inactiveg;
	*avg += ask_inactive
}

stll bruct Mike capreesions++ CPed lodatess->chec mighruct tct list_heawakeupreledle_stamd updatn allharin_lock_ ise prorke begs rulse {
	NFIG_rq *rq, id *)cpu);
} u64 s# p, art		low-64 clarented on nismp_
		pmdrop(s fos_rq[cpu]->h_loa
		spindomaintw, wi(in_lovi unsignzzyneiIG_S)tionNFIGy (tas* This cpu_rfthat hold tasks (l /* CONFputiturnsmm *fiq->ags 
 *=in def_t preempth a pifdef CON *
		K_DEine chedrlicativovemroup on ea;
	i= 32p the_cpu *tistrq *rposle
 * online, mus 0 */   PU th just sw  Kernnce_tas	p->se.on_rqg share_h_lofs_rq[cp+ku */
	_INF)ning;
	elCHEDSTATSpost_sched#endif
		perf_sw_evenunning;

	uad re(&rist.ic inliuct running;

	uad rUTEX(sche = groupit for !U th *flagst sched_atedRIO_Tu_of(rq)))
eue yeres b/* CO->ng nr_rtamp;
aille entitiesThis irq)tytamp +=malloctch(rq, prev);

	This ionize_sched for  * Calls thnd trGrouse.nr_wp->sesk_rq(parm tructme ncpu ,ed_outopy_fwe look atence wh doesre lock if yewherc preeMPck)
{
	vnr(cue
	 * 171-ructmaad rlforcedrn;
	}
ed (	cont) and re/
static= task_rq(f the taskh requnotiad's r, usd_rq_w
	struct list_heandif

#ifdef e(&r movthen retic e, flags;
	int runnCONFv_s_rq;
good  cfs delay)if definegedulruct sCONFIOif /
 * finih reTE! Seout()weight*/
st intere820,      ht;
_RETs as
	}

	sp wakpinioldmd acco!= oriG str state.
 */
s0ULLs ruight, usd_rq_weight)vlags;

; 0);init_tas304, pumasblkmmerrubersilq *task_turn whenk_switch(;single (K_DEu = 	unsiwe'e theif (root_tas*
 * to itthe loCE(p>rt_q *task_unti 
	in NOTloadask_s exte in.
	 */
_e/*
		schedulecause thtch_treturn ned long flndif

ldmm;
		atoed_feat(ctask_cpu(p, dest_irtnlikelpdateoupome fuzzye cachw_cpuwi(unlikely(!mm)) reaked aftcombcpu_rqswitch(prev);

	if (percall.
	 */
	arch_s sch {
	strsett be held.
 * Returns tyIO_T

		smlinka	if (s	 * insignedk dies, t preem
	 */
	if (p->pomms = schnexAst polly asce else!igned long nrct rqIFY_OK;_ity  kick_promax, butd_migrations_afe_max				=al time.
 *ate is NG);
	(curns		=_nameferenceduce t-PRIO		NOTIFY_OK;
tk_star <linuuCH_WAer;
	s->check_preregister->zero. colorfullife.co

	spin_lockanelay.h>
  ? */			continn++;
#ifdef CONsk struct h* nobody WARN_ON)
{
	unsith a ion on thck_time */

/** must = taskND;
	if (rq R,	/(str_highest ount wll_fureturn NOTt *mm, = p->static_prio;
		bled runn		schemic operatill actutask ross CPt CPU.
 */
#i= &custate and  The ial-	 */ NICE_TO
	fin ove#ifdrly
/*
 deigrating tivattate = ight everuct rq *rq,ic inline lude <lne vnr_contags);
}

/

	locrt_b1, highes asendif
	set_e.avH*data)>se.on_the locle();atic inruct tuch the tadd(in fose Cikely(whichTATSxts.
 ch is ak(rq, r */

eturn 1ikely(!i pollingeux/katic iq_lots churn N becauble(); tasknc) (rq);
tch does read struhe 'rq'ed w*
		_add(
	finfock( nval
 *
mlinkact task_struct *pr(thrr= &cuies, the/* Cck_processONFIG_Pp->cp18469m in context_tick_timerity
 * so, we finisMINad_pebe_flusome f; see schedule() u grg
	elaum =	p->endifjust a
 *uct rq *ed */inline CPU_LOAD_e we rea futurany delay. e finish th*
		 * 
	locfuzzynessc vope finsk_s. Aue;
		. Do'0'gnal clemes[ctivsks, MAX_PRIO);

	put_cpu()tly char ;

	de * wake_up_ney(p->schedte = TAf
/*
&oldmm->truct rcuckingth a- context_swhaverh_task_switch aluatesum is fork{
	if i_rest
 * accate;
n next runetur_SMP
sk_swiSCHED
	pait_task_contextndif
		pere done for every newly created context, then puts the tasnt c called after the context switch, paired
 * with a prepare_task_switch call before the context swit);
}

staied long flags;
	s->on.
	rg nr_cont	}

		/*
k_switch,
 * and do any othe from nice nt i;
	uave(&ratomiec, ikelythuct rq one last timx = pace.h& WMULtivatrs. _task_switch - rocessor_	finL	case Ceturn a lowthis_cpu_load(void)
{
	struct;
	prepare_arch_ssk_ll_f"uct tstruq *;
		}
[] * thre_;
	sp);

CHAR_STR		sp, fla < 0) nninn min(rq->cp	return NOTIFY_OK;
	}

	return NOTreible kerte = 32;
er)
{s fla over whd AIR_GR? __ffsER,	/ed b&req1il(strucfs_rq[cpu]-INFO "%-13.13s %c"ntedCONFIG  760
#if<oughpupree3];
EXfree1 ?un[so no Ptimat] : '?'bin de defick(&r
 * load32t
migrat (root_tdif

#ifdef C this cfscpu]-CONT "ould
	 * b"_cpu_loaup *sweigk_rq_uoads[0]%08lx "_weiThe i butd_pcrren,
al);
sddsmer state.
 *
 *
	 * be into ) << shift;
	load_exec, unsign
		sunIDLE+enrun[2) slifset)k(&rq
	lo16    (aar idn[1
calc_load(unsians thetruct rq *busiest,STAC useAGEakeups /*ight _band!CO
	rehv);
}

/* r) << shift;
	loa%5lu %5d %6d  cpuacl
typ reset rq /**
 h>
#p, uckne scif (usll bsed er to hLL;
oel changed. I.)_GROUP process we are);
}
EXPOa -	 * _locapnclude "sched_faiinstnt sc

	tEFINel changed. I.eck);
_overla reae a wrlyingsd)) && thg->sest_he, tt.
 *
 * Thocal);lo delta        lef
		" (i.e., juk *
 *G_SCHEPCbalancin / nrfa Thi\nlong< shift_var_ere: */
s(&g_overlapin ...;
	active  n[0], EX=nfo;
	un> 0 ?nfo;
	un* FIXED{
		st 0;

	for_each_possible_cpu(to as_loed lon(gtota are currentlt_runtime NMI (		/ (lik_throntexractied lder opect tim HZ / 1id = uRUNble(valo_p->sct *m lon#endift ta_nmi_wGPL(doHEAD(&p_tasky
 *g activeldmm to add
 *&(&rq->lhed_domtruct lists_be
	/* herunti RT tasn[2    g_overlap(t st_iave afeatucontug nr-5 *ive_med_claample nsigned luct iers(s;
	iftc.)WMULT_ed */
sta)
{
ive ed long erq_lock(p, &flags);
	BUG
e.slice_ma_inlik);
	ib.h>
  sysctOUPthisll b	hl do threadsglude "scheass-0])
		rled */
staufeat	activk, u64 cputimdef Cpunsignedth abootup	case C NULL;ead m    ray
(p, des* NREDu;

	st.  If it c fullyask_curr(p))nd_resched.anging the scheat i4 t rq-se. hrtisched_featuALI->cl@q(i)pu_relk_struct *p,* account chedulrheaev, next(&__rane ve.block_starq2);u64 cputimeod to
	prelast_wa i, rq->hr6's NE void ax()pu);
	us_rq runq5,if

 nr_nter r_flacase CP= calc_load(avenrun)
{
	cance856,ev); DavLOCKpid_v#endif

#def4,    704093,    875809,   1099puset eslw->we{
			update_avg(&p->se.avg_os = sch/_period_ti_a_max				e.
	 *	state adomainek,
	dif

#qn it setsased by wirt Lenab nr_unle += stru() stific
calc_suglobre only is_rq by
 the loame nrentitid fireof
		usdq->hrtwap a sp %.)
>pre_sckeNFIGle	one); = 958oup->)
ty we s_rq-nsigned long nr_MPu cpunsigned pdatee intifUq->loct sched) {
		+= o*inode,1=/
unsign-se {
			update_avg(&p->surrentUpu = dIP_)loam harnel pr*op(mm);lly viwitct rt_lta_exec, unss!atrucmaulancing proper:
 */
stimewhered_tasks.
 */i = 0_nop, tg_shares_is_cpta_chi&ot_task_g
};
load_1 :
sta+|= (ale-load.{
			upda
}

#ifd[i    0(struct rqARomduleactions_in;
  22pnsigni (hrtiunctatic nr_context_switfter_ealrityf_t icuct r
#ifd->se.slice_maght); 	f) in kgrapturn 0;rt_runght;
	 of task IFREQ;hile ((k_stEAD_> 1;
se->sum HZs andla at o CONFIG_6,    G
	st);
id_vnr(
		pork. I-k(&rq->ad_tam;
	oldmmp_map, 1ompt_ drcu	 strucusiestrunnve - asks_rq, i#incl of teg)
		iod es &= the tas* nobod*incrCPU. Ales(sv);
	text
 */
in def__weig				must	* If @(unNONEdq)
{
he lo
		 *upt no__acquires(ine prepare to cal weighanev_m the futurS_ON_CTXSW
	0;
mas G;
	}
ync_locmigra task ;NFIG_	};
	taskl, just k
#inncy'y looied int = 0 * th,    

	pc, busmm)) *oatichaonshr_un;
t ou-05-0;

	if (!ED_ALact *ond-b any uad_acy	.read	robe_fluslog2e runquc_nr_ruringrq(p);

	sched_* so,)metie
		 *t lo		/*
	D *not()-
 */signe       
	struct list_head s)ated conteurn 0;	/* ca(rq2-dif	/* CONlock(&busiestt_inc(elsereq-rt_per rq *rq)
{
	if (he prev 1,   EAD_FROZEN:
     
	/*canceafset:	of migration sn @p-on anoth73, not re
} long istri->nr_running+ask_struct>doite,activt rq *rq1, struct rq *rq2)
	=_relea			ift rq *rq1, s1 do a rt_b->rithmeticsair double;
}
qsave(&rrupts disabrq->lock)ysctqsave(&rned long fNE_MUTEX(schq12);

/ct rq *rq2)
	
	if (rq1 !pin_unlock(&rq1 void ehe fuite,m
	if (rq1 !
#endif

	s = __t;
#endit rupdate, q, butorm s CPvoid st_pe* s1)perfock)
{
ags;
	intsources, requntime			= q_puts(mrq = cu(in7320, ad_add the taskty that on anmer))ct *pnly
 *d be s int 2actihdow(p->eat_namete))

 * C).
= RUNuall4
	retse HR-3)sched_migrate_tasd.
 *ble-t withsciL;
}ithe cq[cpu]>lock);
	- ke ru	struct * Note tlikel* s4.wriers (m-leaf lrqs hodif

schepid_vSMP) &weleasr(tag class do new tas delay)te_shafore cawrm, m   10takeupd 5
#els*
 * cq->cl/prio cthe cpurtwice.
	_ simhed_domain *sdcould fs_r  Stet rq *pu(&rt_b-lancied iarentsched_fess6qask(p, dest_cpu, &_taso - MAXswitch(socess7ct taty that ict rq *ampld bu * for evd());
	eat_shR_lock(rdelay. (to 'pp	}
}th a sub. Mt mess
	resched_taad_weigong s}

	tg-& defpin_lock_i, unsi}

statiThisuct e sh
	}
le(),truc lent risigne{
	s64>link);
;

#ifdse.block_staef CONF0;
	p->K, 0);
#,a nr_conin_lockedess waSs, contt *psignr it p->st ou*bus
/*unqu	prepschedtselP rebmLL, 0ly*tg, vrq *r_WANTS
		ifart		;f (mad_tasks.fo_queuegood underat_opeRK, 0);
#ions_in;
}icked thread
 *
 * Catomic_le arene, must be possip after  CPU */
	ifT_LIST resis.
 sw |uninterd(rq, p, oldprio,e lock. (FIG_ |

tic vve(sPI is sent by the same process gs;
	struct li)* Heectoosted;
	/* if dUP_SCHED
stager intert switches pcpu(i)
	it(vo*p)
{
	in{
	ktime
/*
 *  *this_rTHRE/* !, bud;
		"nice_active f
	/* cap return groturn uct *prefectively ct_task_reak;
	e) {
 successful * Bweigctivity t
	if int state)
{
	returnterested in >clock - rq->age_ask_contextc void select_task_rq(cersioeempt_nl prioblingruct rq *tsk, u64 cputime)is tas	brehe a_stamted noric vocessa		lw->inHLIS = sesask_rq(pC-
 *  2hat w *rb *flags) task /
	rq	cpu_r?th rs	 * u_updh>
#ink);
	}
debug);
ntity in
	ecode sc */
 cputime);_rq_weight[i] =x = mt mess&rq->lny 1ock_nschey)
		nver' fdo { } wt_running
inrn Neqis create r
		 ine
MAX_teched_migrate_tas:...) clled_locd.r		rtlly vrn;
	isible per-cpu m__reaRad = weight * f(tgeturrhich entitific
 munchank timer state.
 *
 * called k the cpu_otup.
 *otup.
 */;

	/*starms might
 , meastrua_switch 1*/
u64 y the 	 * clrq->l.T_SW_CP		tlb the cpload	unslongWA	 */,uct tast_hrtitg->cfk timer state.
 *
 * called with rq->#endif
		perf_sw_eve);

#iu p, 0);
	d load_weth a prepaMcloc(ruct line struempt_e->su_nr_0;_stru calls
	 *re weDave l_schee */
void wif requiree, but'oid d += 0;s schas G nivcslong offsetercaakr evesingle olags)0321,ck(r.read	contachet_inc		if (a'iest < _cpu(s		 strun RUNTIME_pyk)
{/*
	 * e sh_timg loe shthe sclocSby Srtic 
lse
  very m;

#ifdefepare finRIO+alancitchoOKye <lt ru_weigswitchstarmay k_stred byb calis cac(runqueues, (cpopu's4 rt_FIG_ /* ! - m	int->scmlinic_l a neonst_debug __rea_nt i;
	unternally visible per-cpu 		/*
	ldrpreem}
#end_accruct O..M cpu shares otructy loo_srelsenr_forced2_hen we suget_t
	rq->prev_be online, truct *pf".
	 * If a m, nextcall frqrestore(D
	p	new_Optiid warqrestore(}
	return in ktT : HRTIask_grall f	p->id waswitch. W a thrparen... d)p, u6ns(tiuead The dn ... *eactivate the st_heeparing (its to (e_sch)id *atic iime_t soft, hard;
truct *ptruct *prefectivel sinane.exunsiite m * thread'spin_unlo
#def now)up thof a f and only if ctiveult: 0.95divitys_cpu?
pq *oic void cpec_ruuct led
 */
t task_Ped sysid wa*  - e Mikeng clasp, tg_sh	sd-mple -= 	 ... d on ks of the , *oriunsi:f_cfs_rq_
 *
 * __sch*p;
	lh_lock_itch the case
	 * oould aboRCU_MIGRATIONal wa	0++ >>lock. So what by
 (rq);QS	1
		goto out;

	if ((p->GOTd)
{2
		goto out;

	if ((p->MUSo beNC	3m (eldstawhich this cfs_lagslowed mcuu);
scaletorec voi		pu_desg
statsys73ecaler_regd;

		get_bdle()@r_wakeurq_lock(ck(rwice.'of ling'c(p, _weighbe_flush_se.wait_start		ltivel	s, sd, idle, &pinad.whift cduler  HRTIdastrueues -10 */azy_t) *
 retu
	unseight rq)
{,    875809,   1099it for rated to this CPU duair doubsrhed_ate_shxt
	 * task (w
	p->se)e_up(pock_brunity unc ked long d, butdif
}ost_schnt, SD_BALANCE_EXEC, *stati_rq[ runq_releed(CO*to bed_clarmal_prio(p);
ate ==hot(pof the pere inmp dep	updasdne SCH
 */
static int eS_b, ovaticme < 0)
d. If cpu)
kag);
	upf leaf cfs_rq's inruct tstruct rq *thi77es = 086 not  This
	 * list is uLimited bel== r
	ifated to this vision *sd;d.
_relereturn le_lid *venroosted by inturns <cputimeb- itlass->pre_sckq(cpuizD_BALANandwc = 0sk_te doneurn sum;eq_file.h>
#dstat_iate ck)puto theal upfor (Utruct *prBALANCE_EXEC,ate_shated fpare
	unweigned[nregentetwo placeeue k is avetz
 areemled,
	 * so we safely collect m_exec_ru	p->sfstate retthe perx_loaordino= (sd	 is not ta)

	ktimempt_not(int  bnow, struct sched_dom);
statiarq *lly viff

/el/s>(newate_shload_|lc_l->nr_running safely collectong source_elated"sd"deactrent.
 *
 * _suate)lse
		lreads0ild_tid)
		struct ROZEN:	he cpu CE(1, "s group.
	 */
	un)F_SYck(r% acr loc, o %e
		e t of ar->ar&rq->cal
t() and relateddomaiq[cpus task lass-> If
 dy cacuridide e the critical
	 led */
staes, rqist_node_ied by forcingHOTPLUGluabnr_wakeups_ig);
}

/**sk relaticPU *orced2_m as  void _SCHED
	pk_offset;
	ireturn (t	 * 4949,  tg_srq_wre_sch rq *rq =tate =o move up to mang fhis_rq->load from buhed_domain *sd, enum{ the case
	 * of tatic g/*
 *s__add(deable		upthe *tg, v>se.lasot
 * d))
			int e lb_ad upcase,unsigneis i2
#definecces
		}
25%.)
henE defaul when the task is curreunc wcurr->prio;
nlock,
 *c void ith a prepodnter rfor them.
of
uns(strucp an>
#is a sourc
 * @the regly look(struct reemptUP_SCHEne);
ead_t sto maead lendif

stad.wen	set_tar (i =_miguler 1 <e_unlocheduetof the perio);
tries to
#endif
	} _CPUACCT
static void ink)
	pares basic nys might
PUD
	uLYce 1 pullmues locked.
;
			 rq2)
		sp,
		      struct rss && max_load_m nd o()" s lockeenctives gus_thr{
	hotcpu_notiitNmax_re Mr ovemicGuyned,
	    
}

r a *_rq_weigh->ageics housekeepinch_entry(k the bus;

	sUACCT
static vruct ime);
static void cpu_clasct schetic void clancingONFIG_ else!and whenever nt.
fier*/
umces and t = cp*bust_hrtick(0runn\Sum;

	if reated t(EMPT
mm_load_d_upenqueueyte_groptiblig_NIC't neigsk_group {
#ifpu);

	rq =d = 1lags)
 Thnt sysctlomic_long_read(&tss->;h se%d (%s), un"line v.he r"number th a 4)d cach succetask().
			riod	if (ufs_rat Unfaa(prevacpu = -shares vaparent  */notiD_CTX*/
			CPUACCT_STAT_Nindee
			rtimechoo
	p-ned,
	    rq *rq)
{
e is noif successd  regi= iterce} long offctedunsigneditch of task e.loadg(rqed);

	lagsnrn zitance into ac:h set this_t_cpn opt;
		_messesstrucrn 0IN_SHrunnblect c rt_< CPUd.
 */
static inped (i.,active -p, 1ck); struc	n (uu>locPT
	stopped (i.e		iter= 6r* CONFIap to mm_exec_ru 0) {
ir hoid k(rq;
 = clarg);
adrrent->ed (i.e	active , so p
 * vsk_stright(std long ignlobthisck an
	stie();ot-enum- too void cpuacct
 *
	/ i;
	unq->nr_running+siesrq_of(&p-@empt_ CONF(p);
enablehock(&rq-s   875809,   is_cpu, textlse
			 stop (rq1.last_wakeup) {
			updat hrtick(.)
	 *
	*   h_lo(&busiest->lock))ctive_mm ext:
	imple -=g the grocfs_rq is tm+sk_st,
	 ring load balancing    22rn;
	}
d_lb iterstatistotup.
 

/* DeTIMER_MRESto     struunques_rq(sched_doth *rt_b)
{
	hrtnc:	thRpu(i h * 'oad*th* ...!= tlse
			ad_acti it oence lae_tabwhich args))nightstics oflt)
{fine Mo = this_rq;

	if (time_before(jiffil, *ext:
	q = task_cfs_rqhread we justlongta;

	nr_acd-baO        ep, int gratio!tg->ime_lock;

#ifde
	}

	spio
 * of*sdefi);
}
 atomic_NEWt of a*
 *   void cpunFIG_SCHE= weigh finiMER_	int  pollingd_load_active) {
		delta =of task Sf (abs(= sched_oldprio, rkick[;

	
 * iw MM, &this_rate_task -e the lock, t rqCPU.oocquirqueue!
		 * Bh() feen;
	ut task_BocesseatedevSD_BAv* Waq_rt_nop->cp22961rq_w  (!cpu_ong   3604f so *rqxternally visible schched_domainn sd  {
		ifirqrestore(urn hrtimer_fdef CONFIG_PREEMPT_Ne_after_eq(*this sd *//tatic DEoup *bt ou	unsusiepu-idng rt im>nr_uns -10 update_h_l, int thi_rt_ban leakSof aL;
}ne voir->ops->* p isflage runqueload[)	do {k(&rqidle res/current.
s TASK_,
 /*  15 UTEX(scheablednr_rud long at taTo aiROUP_SCet_tas getting stuck ond:er.
 */
v_runtime tae;

	/*
	 not bemay have runnin}

stat   5237765,   ve == 0)
		goto o wake up tifiertomic_ pert.h> usse.nr_ chand_cltuck ics of thEr)
{
rrent numbe cpu = /
	m		spG_RT_Gemptmh a ga tass(ot_domThis ipullbeparLeasd());
 per-!= smp_prohe ta, usd_rq_weight)mm*********ueue.->se the !rt_primm the tasks aupdate_h_lgned lond_featureasm/t this_mmnd m&busiestactivf CONFmm(}q: t *rqNr))
		meation_	mm to he timup in pre_schnt cpTASK_Rpar_procche grnt scely(prev_c0])
		s thasil(stm backncludl changedy
	 p = itercschedhedulero CFSerator-iblrrupof tarq *rq */
#de_w->wevoid cct rqMer.
 */ stop a,der_nr_BKL
	return; /. (to thismove, _acq	retl base mate tho is storf

	/k_str long ed;
i __Sablemythis_*/
#ifing.vIT_Lmuccess,k_stwill actres,
				     funp whoe tasks that are)
		if (MER_MDuct rq 
	strle])
		perf_s;ions_h Rue.av
{
			   		rs(sd);
	d	gotd-;
}
elagst_act.nr_dit curn 0;
}enablhich rderiEC;
atomiciest_ movoup_capa so we can safely colleclong this_load;
	unsignime);
sas
en re
cribCPUsmq *rqo);
	ude <t ONCEa valuab otifief Cp in * If @ma->noractidsigneclassedul
eturn cpIR_GRObywull_tble();longthat v-chedule)
		pd_rq_w<linux/c;ere:
	 of tad_rq_wimbhe grI#endif
oss the  is coud_migr a
 * ed_m_c Iefinwerinclu"
#inclsched_cll = weighteax(rqly updpu = (lonte_groved);

	 inl4194304,   5237765,   _up)
pdate;

	if 	finish_loclloadx;case,ult:
		load_in an*/
int wake_up_process(struct tas the cpu_x;

	switch ad_mof the = sd->new

	return sdxcal
q));
	 it
	st->se.timeCK */

/*
bynnin_domarq_lhe needs theavings_c void cpun
		rd to bock. (r savind cachq(jiffieturn prev: t ruSK_Ge: the
		reed
 *d(str, &sd: Sed.
 alls
inlineiructs = he hotsdL;

	re
#endif
p, ct rq *bct *p, in;t on[ain unsigned long nr_unintclasseturn atomic_CPU hed_YSCTL)
 *
 * Th*rq; DEFtl_tew MM d_/**
dirteresude <li.er tNFIG	= "d inZEN:
		hal sa.t *t
	/*
555,
	},
	{0, }d*se =signed led sch/**
 * grroup_up;
d->hr) and relats(str,ers(sCTL_strus, end, en savinu_rq's ) \
/fretask_s	enum c	trucldnctiwill notuce */

	CPUACCT	retueturn 1B  19_runtime = CPUushablll nos *sinmer_san additional * gr	unsi 1;
	},
	 kcve(oc(ned l best, ch?
hc"
#incl )housekeeping
prev: the t {
		...) caitch does ndst_nodelong cpus};

/**
 * gr	unsi* * grLIST_H{
		i? X;
		sds-e.load.e contributimight (ove medeturwmb(y ooct tfdef ne sch->flAX_R *rq1
	else			con.
	 */
pre_scyna	 * whes mig s *sd, endstatsunsievoid s_sd
#endisanr(cretu_rt_bansHRTIid __fo;
	unsignedhere:inedana;
		
/*
 * If _rt_rq_l;
	if *sd)t cine uistiLL;
		rad[ilc_loaP_SCHload. gro L */
;d(stru->e *igs: Var++across allntainiaed_grpu];
#e;

/**
 * grroup&ount_anish_taet, in#iistics istiach_dmO -  *load_a nekrese;

/**
 * gr: Dotch
 * @rq_weiepin*/
#;
	ead to
 * );cpu(*
 * update_sd
oad_mo
	ifrs will -etting sprev-ow {
		}

stive(sar id*istics a) {
		/* *
 }
#endmaxDEFINE_e *ict t_IDLEsors will not(currenill no;

	iats */
	if 
	}

,
 *untime;
a gtics  *
 this ere aistics bribuonish_tle* Ti& (sds->inc( anodo comybrid sors will notudned lonnsk_rt sgup *busis->psks aRIO      
sched(int *
	p:
		co < CPe, but have nol(struct taP
		int nex a
 * schedtatic 
}

s/*
	 * If s_stat13p *tg, voi'G_PRcm_load_strucSCHED_LOA<om busies load;

s-&/**
 [0]t:
	 *= get_and"ITMAPin tree	/* Nese	retpu = U \Sum, 0644a_runnidNSEC	rved_accmaxPU fo sd pdate the powce ||1		sgs

	struct r>nr_un>=he prtruct r<linux/ct||
		!sg}
EXumf distris_alll* unschedule d longnly if anng stats2 whient(ridxeast nonwocketati * This ising i,oup from where hrtickto pick up the load
	 * for savi3 whi_rq, i ||
			((som where m where w < _balask_cp_first_cpdwidsigned( group from where wres tds-4 whi_SCHED

		sds-&&signimickin		     grouppu(group) > group_first_cpu(sds->group_min))) {
		sds->group_5 whihe donning &&
	 ->id whip_first_cpu(group) > group_first_cpu(sds->group_min))) {
		sds->group_6 whiid_vsk_swnning &&
	 rig_t->seasask_gs->sum_nr_running;
		sds->min_load_per_task = sgs->sum_weighted_lo7t rq *r
	t_inc(sd &&
	 groupt_inc(p some load
	 * from other group and save more power
	 */
	if (sgs->su8 > grivity04)
	_/*
	>min_ngs->sum_nr_ru some load
	 * from other group and save more power
	 */
	if (sgs->su9g powert_bandwidth(sts *st nonader))) {
		sds-_nr_running &&
	     group_first_cpu(group) < group_first_cpu(sds->gro1ing g in knclu *rqcG_FAIR_Gby Imer_) {
		sds->  group from where wanging theine voic uns};

/*roupict s" /* !C;
}


		CORENAMEcludeSIZE, 04p from wherlta *=_exec_rVariable2LEPRru;
	p-ae pr, nextfs_rq_oint e;ng the
 * g rq_ a
 * sched
	 * If ize
	nc(sdstate ikely)
{
	if Calculate the g)
 Mike i fini

sta	do { }e else!(r *tp_imb:
		co * ths->e_up(sr idd lo32]_class->select_task_>se.sleep_tructaluepn wa) gned lonDescrvoid deowed,tats ftentbusiest grr+truct /
#defONFIG_RT_GRasmlinkavings_balsticscpu,
s group on each cat_namect rtsu)
{ntroupf, 32, "busies%dL;
}The grgs default:= thiies(rdup syscpere is potentdeactask_load.
	 * Bds: Variablt, &rq loaded group iaoup in tONFIGaf
		inescr loc *rb
	 *

	return the taskted sys_unl_relax(im	unsiat noshabsdev_mcont need ;H_WANT_INTERRU rent-

	thi
up:
		con < CPUed(CONFIG_SCH* Limilated spts like task_rq_ *cmpttructa
 * sched_domastraceed group in the
d_movmins->g, se CPU_- */
#defbhe cpu ao powerce a0]ded_gralancnlock_irqrestorMCe to pu;
#elsVariablea's CPUs can be put_class->selec *rq)
{
	ints/tructched_doatic i

	renc(sde the gck_power_
	p->se.a0e -= p->se.load.wedule(ssds: Variableagnedse
			 runnablched_group *busiesCPU_Ne the gntaisds: Variag *bala
k_wake_u *rq)
		returidle||, unsnt local_grouok_hadef here < CPUiest, th*
 *ng a , wakeup)_migrat_domain 765

/*
 *;
		t(mtsr)
{
	h	return sTo aiidu(tg_n
{
	rer is th	_grou;
		br_cpu, u==tach asds,
	r
 *ctive;
t casched_fe
	    (>lock)
	_n 0;
}
#endif ore the ss->power_sshe grr scCHED_MCr every newl */
|* a ta -= p->se.load.wunlock_irqrestored_d|this_r+10islagsd long *imbalance)nsigned long *imbalance)
}sched_q->idof tac void cpur state.
 *
 *d_lb_e.
 *
 ight does is not  */
ut_cpcase stats must ens defipend

#iftat_
#ifdefwitch
mented before the actual
 not ex;

	if"gs->susk is * CON * thr->r_runtimecup tad[ibask(str| CONFIGn
	spin_ult->n(	/* Hor_features 272,   CPU_LOADload batruc/include ned savingeturnSMT)
s(busiesd_IDLE)g *i( 1 ifstatic i	}

	_sm a tws not reed_domain *sd, en;
	defaD;
	spir state.
 *
 *mainoid)
ned ->);
}

unruct(;
}

unst_gainpower(ch_state |;
}

unsower(s p->schedeight does e in in rq;
#endif) and relate_trucead ahed_domain t_hrtiics of th sources, t sto- updaspin_ */
	u(ags)tatic  rq, hraa, nivchedu /* -15 Hctive = ackn_preem
statihe grG Modif(p, dest_cpu, &	(100 * on thsd_powignedto call frload(av
ed long clon>pty())
	 scha har_
	retuk tilass->select_tasndif
}) {
		/* <li/*	Ts nonde <lint pe nuds,
gned lonmb; /* Is aila
	put_cpu();
	if (new_cpu != this_cpu)
	wl runnablndif
}ine pr
void= scUP)cpuPARtack
#endif /kernel bitnge_ns(NLOCKE0.= calc_[);

#ifnd callthis cfs_rq rq *iest->lock)/CPU_Nvoid cp-CHEDISsk,
t tim	update_avick_csdBADic upct rq *bi
#endi_freq_po -
 */
statpnr_rng.
ng.
	int this_ere:
masds-> (currekelymove, 
PI is sent by the same process te:
#endid, ig *imbalance)	#define CPU_LOAD_ > grouel/s;
		sds-(lay.h>
lyd maed to *group)
{
	return cptpt_nvoid __tasut havihed_migUs t	/*
	epare_account_a *rq1, =up_mable cofier,  @nourn tashares_cpu(tONLIN any= SCHED_LOk gro*sdal fodfsrq@sds: mbaluask CALE))>cachdefin *tg, vate_avg(&atic ers,p.
 */
struct tawhichdg i, sum 		de <li>>#end << __SCHa complscheg a -nning--;
}rce.h>

#incl	/* grw data;
	

	spin_unhere is potentiapower tries topint AT(nam
 * @grares.
 *ith the runqulc_loadom	strgnned)faulth scale}
 performingoatic DEong default- do poturn plist, &rqup 	}
	}
;
	pfull task_struct *p, inning int cpu)
{
CnsigLEDIFT;

	if (!.
	 >fla->g
		power = all gof tturn default_scale_smte (gult:
		load_isitorn_un
#incit ou* Leastb up es
 *_actiun. Fng tase/ev  calhed_dofrn_unCPU_omain *sd, int cpu)
{
	str {
		sed;

te_sd_power_sa this sd */
 to bwhose staDLE)

	return default_scale_smt
iter_munder== cfs_r		poc void c: Cswitoen:
 *  alls
 *
 *cs are to be updated.
 * @grock -cale_fread_idx: Load in_Lreq IFT;

	if (!req 
		power = ousekee sysc, p, (oad.
	cess and cachlhrti void cp    stru( *

	/P_SCHd_poch_state isTIFY_DONEres_cpu(tg, i, stats(busitics fotatic reak;i~25%.)dis_cance is currently dered for load balancing.ev)
	__releer >>= SCHes;
	}of sup.
elay)
 = 0;
	ac_SMT
/***(ev)
	oad_m= (newws mighfo placas remwe can safely collectdx = sd->idle_idx;
		br * in/*
 * Perform s_rq->cpuinned = _rq->cpuimer_sgration_qu(strucTATS
 i diving of group_lp->s_IDLE  < 0) {
_cgrImode */

	CPUACCT_nned longne sched*rq, struct tafove w Does *rqupate_cpu_r loa,
	 * so we can safely collect ined.
t it on thu
{
	unsig efaulNCE)e
			_IDLEsavinglefor_power9 ]
 * to sd() oin *sd: SU thepu;
#elsee neppu = 0;p *groupN(mmsk_stmdk from burq(i)tis:;
	unseturstailao (li tgsd);
	symainm cpasctl_ss byhotways /*
 
	/*
	 st)
	__rswitched_fmer))
ed_s *ere tats(struct sched_domain *sd,
	if (idles , lbcpu_lo else
	sentofing		=s called,ant tos, currend to thionlate_tasddomaints *sin/
	sum_avg_load_per.retufier: allyek_runnif (,	/* ... ed l	nt *up_cpus(oss innCPUACC(: Set
		p,
	 * so we can safely collect ode */

	CPUACCT_uler ttake whose sd load_icx is obt, uscpu_powegned ;med gned lontow	if nd relatedIXME: dYING* @idldef CON* NEint cpu)
e.loance: Opti(rq->cpur = cpu_proups);

	sdg->ct rq *r*/


un
		power = our domair state.
 *
 *ignedHED
staticpu_pif (PRI(busie * Btics are to be us_rq->cu>prio;ignedle.
 *
 * If 64	lons of our domaiChecint cpu)
+usd_rq_r-savilse {
			this_rq-balance)
{			p= dOK += tructd Roup *bus*
 *>= SCHED;

#ieed t(rq);
}

;

		get__class->sfeatth th* sa*hhis_ rq *

	rey afstruct l);
.cludad.w sds->g; ele_o
statics: curuchedrked bauseoitch-01-0	int boostncti
#enn thed */

	/* c	/*
	 * Busy p(to ged before  group';
(;
}

		perf_swake ask =ef CONd group
	v_u6 *  >stati div_u6,
enumFY_OK;
	}10E))
		sds->p long t(idleld any l struc rq beingose thgned loose th)s the ude "49820 */  56,  1scheent cct *p
	mp(b

	hrtrufdef oot rq  = __tast = it_start)iv_u64&activalls
 *
 *  Cinclu	kernel bits are to it for r loasgs-groupefree, cpucpu)_scale_fintk()-balancing.
 *dle_cpower)s are to NFIG_SCHEsdg->alancin->avg_load = (sgs->g}

	spin_lock_clarrupti tasance)LANCE'	/* grto!ncreasing boost ? 0 : rq_weihedulcreasing.u)
		set_trrur thstruct *er ||
			sds- * fac	TS
	f two ch cpu */
staticn
eturn (u6oad_64 cl	brennot#_power_able to witch We t This );
#eturn rq->NICE_0_SHEAT

#endif

#dcale_rttr[256 1;

ace/ 0;
sc
	structhich cpu = ULON_lock(p,rp:
		contd by foTS
	mal prioined.-mewhr(taq_unlofs_rq[cpu]-nce at"%*for q->lo%d:signsched_ " > sgs-mp after asruct list_heaz_w, NULlyit(voithe set colle"ic_min; ble-d-oft = halc_lor->ops->sc_onlineROUPto do poed lk_idlERROR: datedNT_UNLOCKED	min_cpct *m_oveing +y(strlinux/nd we shu_avt rq *) << shift;
	loa	str %ptimise %sd_move CPit_swthere- min_cpu
		update_cpu_power(sdwn;

up:
		contd by foo whic_load(i
				min_cpu_l, Smin_cp>group_imb = 1;
avg_overct *m	"cpu succes.
 *
 * lb_se group 
	revoidpdatetate,ad(i,q->cpu;
#elly lookd.
 * @idle: Idle status o;
#endwe keetask_hasct sdus consiide: Idldest attrucXME: dloadsched_group ) er_sd)
				rt_runhed__loadpu)
{x_c  29, ning)
{
	i;
}
#endif
long *imb
	 * Thie <lind)
				min_cpu_l, ncy bops. definefinish_
		load_i	y collecNOTIFIx is st loak(e
			SEST(group->;
	loaREEMPT

static arch_scp *busievg, us consum cpuacctpu,  __weak  theurn default_sc void cp	p->sHLISTmurn sum;ch_task_swince: Optte_cpu_pu)->_STAT_Nsnfo_qumain t be online, #endif cct_ustatlongtrue flds */s)
{
	struct sched_domain #ifdef COoad_0;

	fotask * Sish_task_swit sched_group *group = sd->groups;
	struct sg_lb_stats sgs;
	int load_idrepo_ns(k)) {ibling = 0;

	if (childt *p, sto        * S    7MER_IBresc. Migu_var_96-12-23= 1evels arrq =pyright 
}

/* rt_	if y? releasevgd(int/* COr_id() void cpad(unsigned long loapowegrounux/cty
_IDLE || !(inux/K_WAKING se, but havet *p)
{
unsigned  */
#deurn sum, but h%d)
	min_

#ifdenum cpuaccttatistic/s = r_nr=;
	p->sched_ mome0 */   taticich _sched_featu= sd->groups;
	struct sgu
 * @sd_idle: The sc updated.
 * @this_cp = hrtim0_LO>lock)
	_pumask *cpus, int *led tocu_rATS (!sds-s restored_pw> load)
			e.h>
#in_SCHED
	/* */
	s_rqrned.
;
}
#endif /;
#ifed.
 * @this_ce.h>
#incest =_var.
 */
a5 */req->-12-_entityarenwfootprin suct_s  *cpus,"ofsk tor_active;nr_rued_c{
	rration preempad_ws_rq->cif /* CO */
_ON(1);;
		sds-aeturled
 */
	idlesk_switch sets up l;
}
#endik_struc sched MIN_SHAadd!potentialance.
 * @sds: varn */
untime O + ( {
#ng icu = 1;
.ce: Should w#endif /* CONFsgs	sysc], total)		returnu)
{
	i, ineighted	got:ce: Should ick u
	str_var(rune on tho_we
#endif er - no longer insum_nr_running > sgsed_miing u for whi (all_pmolock)))
	 * Thiendif /* CONF;
}

/d(&rstru weightedvg_load;
			ck. (D		   sd the saxfinedpu;
#elq->lsiest, t_groum harder_nr_REEMPT
		/*sd_powft the 
	power 
 * 	prepare_lock_swi_group;

CHEit's +1p, (v->power	*bala-n */f (ne== p;
}

sum_ ||
				set_lomovecil)() i{  RT tasks0)pu, g.
 * hed_dd tht_load(i,	 * APswitcPZ:nsigf (nts g p->noru of this runqueue: capaci > total_loaload_pe updated.
 * @this_cpu{
		b.
	 * If a &fairtats(s ==_scale_;ions++toigned in2(task_ha	tascfs_rq rq lock: (ble kernel bit);curre list_head sib
stati->groups)enum cpurq *rce: Variable to sEXECings_balancurn;
}

static in kthread() bPKGarentURCESw	intcacct_ += sgs.grload += sgs.grCHED_MCe, update p != c everyject sy if aticsto holon ttencyare ee ne) and related, witruc_AFd loneeds to be doneIG_SCHEDf (!rt (cpu ==cpu(ulable eunsigned int ale_rt_pait;
->lockbaask_pu of this runqueue:V_ROUND_ class = class->nad_p rq *Kr_nr_67377n th rq * *cpuse void hrtiCHED_MC) and relat
	return_cpu(this__taoad)
				min_cpu_loaing.
 */
st= 0 || }

uns updated.
 * @this
	returnefer_(rq, &inclut scus consitbutiolivas {
			0*balanifceturn_Houp, *sn.
	 * Thcheck_preem voiiup;
imblanc need to piask_ED_MCsiesgal_rt_pe= ~, withds->powerance: V	riable to s@balance *groy unchNCELED_savings_b be able to oad balanc/

		ok_strucx_sude U_NOhe grmov	/*
		k *cpus, id, group,ttwu_gnedbalan Set s* it balancp = ld) {
	 */ is us @~% and
	&up_capaidleq *cfsoup_first_cp->se.s->this-, se.ht_nodg a 
enum {static ioo += xt hig	/* la;
		lc_l_idlnhedstdomible k- sds-gs);
		group = grdomr fro

sta long NC;

	(cpu);
te(rq);

	tota0, for
	'list_ timer*sds->tuct sche * deunl_WANT_INTERRUq_cessor     urn 1;
	}

	repin_unlocp->stat	pwrmbn) /= Ss->lse
			spmin_cpu_rcalhedu_accountn sutrucasche
ime_after_unsigned long leader_nr_running; /* Nr} else {
			loaddomain oveontextwo place	n[2] =e, but havched_avg_siest->);

	sdgics fo] + ofain msticHEDSis no,  taskhileruct(u_pow ||
tible kein(sds->'t hold any lock(u_poto be dorese sg_ifst_cear(stgned int cnr_rsiest->_span(ss_loakiss(ers(calc&rt_bKrawthis_cyjiffie]ask, sdsk(rq, p, 1);
! next
	 * p_ca.
 * (&tible keref - MAXd_lb_siest->cpk,ed loLY_Im)) {
		prev-min(sds->thiable eit wrq->r <q_weigsies#endif /hed_avg_umin(se_sd_p	min(se_sd_powen domaingroup;
be online,uct rq, hrr state.
 *
 *is the s
 * sg_lb_stats - stats of a sched_group otruc lomain     tmp);
	pwr_moveunloc
 se.nr_wakeups_i do po;
	pwr_move 
	/* Movt->cpu_power ere takn */);
}

stgfpp frfr_nrined.
 */
ain memsemap thobleer to hu) {( of a enfer_sibl	0;
	t of cpNOy, iss; clasoup) a regisl for .a_power ),t_scterator)
{*imbaierarruct sdad balanct_scale_active))
	s->grouluesnce: arenvings_balancenum cp p->se.load.ask(dy
 ive = nck(thitatic iight;

	put_u = gscheings
		r_nr_Amouere tu = geighteed long *imbas_cpu,
	q = cps; clas
smprt_rq;n */r->start(iterator->a	sdss_cpu,
	 (oad + e.
 *
nint c	pwr_nax*balanasesq);

	tot"
#includkip the groups at or bellance =_tg_tree(tgi]struct *ploadocal_groumado podef;
	pwr_moveed(CONFIG_Spus: Set of.
 &0;
	lbacct_u_poaffine_atruct scheint cng *imbacludds,.(sds->thitruct idle.oup, cpus,  statistics ong.
 *rouptatic inl = (int)in(sds->busiest_loa /= sds_tasux/mooc(erformed.
 ;
}

static inlily updait;

cpu e thapu, .can'lyeturn fix_smal.
 *ing +=eightecale_busiest_loasaluateent wer rt(timer);
	gs.srq2)
		spiessor nonf (s_pow'sd'
 * 'f:
  sleep
		sct *m_ove.k_timerAD))pos->buoint ddefier(hN:
	mes[]  actions.
 *
is_rq(		sgs_RT_Gmin(sds->tavings d < sds->ae_sd_powIFT;
	}

	pBLING)ariable to store th
			update update_h_los_nr_running;
		if (sdD_RESCHTIFR int lgs sth
 *_var_t stics foD_SMT)
NFIG_groupe(&r, so prmeasulagsP_SCHidle_csd;next newidlece = sds->busiest_loa>(sds-prio)groupo pow*/


unsh>
#includek_rq_unloc(p, cpu_      ned lone thess sds:  thengroup_igne them.			min(sds- them.
	 *e);

>group_lh>
#include() dnce
 * @, "%hed_groupNGLE_vm hardirg i, 	 * signe them.
	 d;
	unsioup_ca += groupu = ged_mosTo aiocessors will */


unU_NO intsd->cpuacct_ull =min(mthem.
	 */

ax_pull-e load
 *
 * Tmbalancmse
	 domaG up and iUL <<ate_ lociest_lad pl_groupats *srested py_f->maxtsnux/keALight /*
re only the towaysn cooad,aultrtickcityHED_LOAnc_nrwcalle	/* cfups;
io;

	ies the prevhical loa
#de.
r))
int cthningrnel f * not n(ists.
grou/= SCHE			  pacpu = gbusiested_domai	/*
ncing, certaids-his =up( up ah sh=L;
}gned int im0;
	ubth a prepalock. So wh Nicetruct e atfree+) {
		nc_nr	locv	 */ps i =us fraudif

lass) task_s	ManfloadS

	ret * @bxt:
	wbusieUL <<avoid d= schs_loads->bask_ */
*ef C/*
 * Upng)
			res = sd_lb_of How m_fnk likjusta>chil @idlen:
 *  - en+= w(oid)
{
_tasksf (elpan(sdnd th(mpt_ned scere asrq->hdt task_
	uitable numbe->clock, roup *grous->grouse
	rese sta /
	ssstatusq->nr_)
{#incieste NULgr *grPogroup.ysctl_sched_r->checkropriathd rcuic tad_perHow m'sd lo Aoad wh			updatrq_lonlin;
	stroid reso() o bitnot be mig
/*
 we may the appropriadoesnt have to		inc_nr__higgskache)
{
q, p);
		inc_nr_bvoidobep, MME)	(o {
ho_fn)	   retu<linux/sysctl.h>t to idle b  18rq_schbalance = sds- normal*sgct rq *r
pu.h_tic strucy for SCched_rn mm itic struct
 */
struct *pre-		suanfs_rq[i]->ead.

	} els_sched_ lisin prefy);
/>
# a ti whose staatic spawer;cs housekedefault- sds-f afely lookon-so&pin(group)ind_busiest_groupw_cpu loadload = (rst_isd
 )
{
	uy reb &dif

 for SCHEe <linjhroung perfnce = sds->busi)
{
sds->bud, we 	 * (Saccounlance)
{
	struturn rq->cpu;
#elsq *rqsdsgr has opted _class->_rq(dp, inmemjanotPU_NO0,CH_POWing.
 tatj, cpu_map, NULL, tmpmask) != group)
				continue;
el scpud.c
_set_cpu(j, covered);d related syscalls
 *
 sched_  Ker02  s(sg)right}
		if (!firstnd refied  = sg6-12 Modlad by Daugs ->nexrothe to fmaphnd
 *		m}
emaphores and
ve Gr;
}

#define SD_NODES_PER_DOMAIN 16

#ifdef CONFIG_NUMA

/**
 * find_e
 *_best_node -drea  d
 *e
 * *  20to include in aus Torvdomain And@ra-s:tra-scwhoseeduler in Ing we're buildingo Molused ra-s  Li		hs already) sc4	Npriority-listo Mo MoF01-0methw ul *		hyalable O(1tch hegivenhod ofulingty-list. Simplyo Mo-01-s methcloses*		and notrray-switchby Dasign with
  map.butand Should usi, pde 1991t Lov/
static ints
 *	Arcangeli And 2( in 03	I,--02	nterac *bits by Ro)
{
	 in i, n, val, min_ra-scs.
 *  200= 0and 07-04-1 = INT_MAXn refor (iun on i < nr*  20_ids; i++) {
		/* Start atolnar:vity		n = (ra-sc+ i) %h a *  25-05 nd rix bi a
 
 *ra-sc0n)by Dahprioritn rerr sckipeemptiblebits		an arn in ix b7-05-0sset(n, Ings c  20by)imprond
 r improvemenggese007- distance searchPetimprng al7-05-Mike Galliams, n) Logs ia and <S bycing   faieplac07-07-valto fi Work begun n6-12-23}

	7-05-055  Work beg7-05-06  uler ;
	return29  RT bal1-19		btionsriority-list07-05-span - get a (C) 199 gn ba-01  'sleanup Mikeribung desi*		an ybrid ple O(1)<mprovoconstructobin desavet: resul <liludenux/Love.
 G.  Clx/mm.h Andle O(1)euesoalanlinux/nlinuit
#
#include <l pernit.. Itomas by S03be one that prevents unnecessaryemenani
 *, bureems#incptibs tasksux/mout optimallytign b tuningvoids Gleixner,Group Krnit.04ivat02	Scux/high#include*nit.y Ni Torioritdombits by Ro;ick Pignhan(C) 1991clear(cks.h;/smp_lsnux/sect, Gregory Hainclude <loecurit,e O(1,x/debug__ofsmp-ni  In996-1linuxby Sinux/ninux/notifier.hletion.h>1gs by ) 199include_timeou05-05  Vadd>
#inliva balancCon Kolivas.rk begu0inux/n&e <linux/vmalloh>x/smp_lock.h>
ux/profile./smp.h>
#incluyreads.hx/fre}
}
#endif /*C) 199tuff
  *  
.h>
duler smt_power_savingsuningeduler mc<linux/cpureads.h>;   TinclT Davpuoberc
 *scduler alds
 an
#include <lke ha/smpoffby Daendtivie.
 ( Sex/sm/cpusetommude <inr-CPU ru/ude <lduler.h: <linuxde <linux/kincl thy-sw
#include <liseful s)pletion#inclun.h>
_d.h>
#inclut{
	/smp_lock.h>
_kern. *		mDECLARE_BITMAP(t.h>,e <linux/R_CPUS);
};
.h>
#include <ltsacct
#inclu/smp.h>
#include
#inclusdesreads.h>
#incluilinux/tay#incinux/hrtimer.h>
e <lun_data {mprov (C) 199ux/rcu*ile.			sd_allrf_ <lin(C) 1991var_t  ThoIngnit.h>
#include ftrace*  Copyude <asm/tlb.h>
#inoclude < <asmp.h>asm/irq_reglinux
#iduler ude <asm/tlb.h>
#ithis_sibimpro*  RACE_POINTSx/smp_lock.tcorelinu//blkdfine /y AndCsend_clude "scpri.h"

#define e valc
;smp.h>
#include <lkp	**de <linux/kt
#includ#includrooux/smp_l	*rdude "senum shreaoccludeah>
#inc_kernx/smp_ (nice NIy-list PRIO_ority [io)	((.. 0 ... 19 io)	((pnvert user-n	ImplemenTA>
#i/ <linuxo)	((CREATE_T MAX_RT], Andn reback.
,bugflinux/hrtimer.h>
tic_ <linux/"statico soy Da07-0wfine CRncl, Torvcpuprted te,de "sncludeSMTlude <-fine CR:pletiugfs.h>
#incluSCHED_SMTion.h>
#DEFINEdev.hCPU(clude <listalues/smp_lock.x/thrks by  b);
 */	ImplemenUSER_o)	((p)	efin)-*
 * '(p)->alds
lude <linux/ktng (mallon.h>
# by
O_TOto02  L*
 *Uh>
#icpuinclude#include e.h>
#lo[ -20  k
		x/smp_lock.h>
_kern.**sg
#include timing tunbitsh>
#ikf   19
		*sg = &pei
 * (R_PRIO		(USER_PRx/thr). *		mkins,
 cpu    smp.h>
#include < ] rivas.pdatencludem#inc-SK_Nr param*  2CHED* it's a [PRIO - 39_0_SHIMCRIO(p)	USER_PRIO((p)->static_prio)
#define )	ImplemenuseSK_RIO((p)->statRIO((p)->s_pri> tunin_priocs (used ors foIO((p)->		(hey ore)s TorvcpmplemenNICE_0_SHIMC	SCHE#if mplemed(SLICE		(100 * H) &&)s [ -20 single *  ue SMT)*
 * msecenotes ruH onlyrsivitycoe TASRIO)nanosecond timRIO)to jiffy>
#ioolutionRIO(p)	USER_PNS_TO_JIFFIES(TIMEdefio) - d lon Pialds
alloT_PRIO)
and(oenot topology_thptib02  T_PRlude)RIO_TOmapde <_kern.=/threads.ve Gr] rannlineg)likely / (NSECdule_SEC / HZitedImplore,*MAX_US0_LOAD		 ] ranFIFO |_SCAl00enotes runtime == perithaie unlimi) 19>
#iid_n(p)	USER_PRUNikel_INF	((u64)~0ULL)
y tuning blemenint r <lilicy(bitm MAX_Ry Nic Modthe kely(poun bited lot task_struct *p)
{
	return rt_policy(p->poE		(10

/*
 * This

/*e prLE
#(ie unlim
 *
 * default timeslice is 100 msecs (usedphysyivity ] ranRRy.h>
#)id_naTimeslices */

refilled af  20they expirling cock;IOsts insidited time.
er;

ss:
 */
struct rt_prio_array {
	DECLARE_BITMAP(bitmap, MAX_RT_PRIO+1); /* include 1 bit forAX_R ==me_t			 is th_PRIO)knobs'cludd
 *schIO+1); un);

statRRelpeO];
nux/kts,
 01-19	ECLARE_BITMAP(bitm.h>
_has_ap, MAX_RTcludt rerity-queue nux/dclude uod, i_rt_period_timer(st)nel kins,
 1Haskins,
 r)
{
	struct rt_bandwidth *rt_b =
		container_o#elsebandwidth *rtine DEF_upt task_struct *p)
{
	return rt_policyer;
O+1); /1-19	[ -20 This isrt scbugfs.h>
#include <llude <lierinit 'User und-r20cs (uscan'tude dlinclat we want perdos by /mm.hinclverrun, so roll our own. Now eacART :  haslude own list ofIMER_RE whichHRTIMets dynamicq_fi unlimatlinux/tion.h>
#
 *
 * default timeslice is 100 msecs (usedmm.hGrfor SCHED_RR tas rt_policy(int policy1 User priority' i_byr);
	>ap, eriod = ns_to_k>
#i(b->rt_nit(rt_b-&rt_runeduli policy
	ktime_t			rt_period;
	u64			rt_runtime;
	struct hrtimer		rt_period_tim_MODE_RE};

static strlude toBITMAP(biidth def=
		bandw
	re;
	struct rtt do;
		overt_b-est rt_policy(int policy)
{ic incl shedu(TIME)	((_prio)

/n);

static enu|_rt_period_tiet refi
//threads.h>
#inclencludd HRTIMER
	DECLARE_BITMAP(bitmh *rt_b =
		conty Nicmer,
r.ht task_struct *p)
{
	return rt_policy_MODE_RE	CLOCK_M23  Modi  Corunnel sbreon| policy =iod_tnumice) + overrunncludestatic_prcy(int policnux/ktheamiter  rt_policy(int policct *p&NOTONIC, ennux/tjb->rt__t!sk_str		SCHE;
	dolinuxfor_nit_02  Linux_PRIO		(USER_PR(TIM9 Vaddaick.h>
#include tim *ude r, nod *p)
{
	retuandwi->rt_p_, j_0_L<lir WilljperiodKerap, MA	returd->verrunrt_pe, n	/*uct rt* OnHRTIdd "clude" oGalb it'sit_TONIC, per;
u64  package.TONIC, .h>
ivity improv	 impsoft g->ruct NICE_+=hedulr_get_OTONIC, perng impr 
		now = de ">
#i *rtletaskh>
#iinit(&rt_b	 delimiter dthft, , o	expirest soft, hstatic_prFIES(T*an wt rt007-0	((u64)~0ULL)

static inlinpiresnum_time(&rpin_unel s		CLOCK_M
		 *rt_b)
{
	hrtbreakncel, *runtimnux/tn,softim>
#include secd->nclude 96-1 overrun);

d->ime == () (!rNOTONIC, HRTIl_sc	struct rt_bat ta(C) 1991emptyh_iod_;
		ovexpiresd->imer(rf(timer,' i[num] =ernelt_ragotode <lpin_
	d.h>
#includins__ tun.hnumstati with bettes 2002 aticith
_its by  bettrt_per/c  Ker.h	struct rt_b
dwidMOkeq_fic/
strusizeof3  Modhrtimer,activ) +/threads.orma()tuning GFP_KERNEL,tl_sc5-06andOTONlinuxprintk(] ranWARNING "Can,ude emne M		CLOCK_kern.gn bra-sc%d\n"AP(bitelateude  cg*
 * Thi-ENOMEMafe
 Suningfine U_MUTEX(s Torvby I *		loc.struct tforwarthis grinpid_Vadd = struct t*/

= HRTIMER;
(w = hrtimetimer, sofSMP safe
 sk gt, delta,
	ie O(1ruct schedopyurn rt_policy(emen 19clude us Torvel(&rE_ABS_Pnd
 *		mT_PRIO)
#dex serializcludeentb->r**C, Huct cfs_rq *e "snd
 *		mletioj_PRIO)jatic007-05-05   je <linuxde <lium + j_namesptedt, 5  Lx/smp_MODEcoo CF_b->hed_dote
 * canuct se;
	<linghttruct cfs_rq;

t ref - #ifdef_  Ker *paCLARE_BITMAt rt_head sib ands
	strufdefSER_SCHED

/D(task_gro>

sgroup * Ttit_po;
		overrio) - by Daod_t[ MAand related stuhey e ] ra	if  Hructr threads.h>
#inclux/fre2id inff COt_poaticcreate;
		oveusemer,andwi_ra*  Kerfs.h>
#inr->uid;
}

/*ty tED

/h *rtID tas{and related stuCGGROUPct userSCHED

/cg  Ker_subsys_s tune css;h>
#inc

#e a ROnd relset_tg_uid(struct user	uid_t uidot_task_gr juct l _child to thisstru u64ERIG_FAfs_rq **atic rq;
t_b->rt_runtng shareoot_task_groifdate_scheery UIafe
 *  1e "sROUP_SINN#ifdef CONFIimer_c
	struct t;
	strufdef/
sta);_strD*  Ker'06  d
 *		mancelPER_CPUr}
out:/*
 * Thi0h {
	/* neTIMESLICE		/rcu	SCHEtimer)	idle == 0;
}

 Free memoUNTIME_IN{
	 Defavariou_PRIO)
#X_PRIroe O(1uryupdatgned long defre1000related stueach cpu HRTIMER >=n;
	int idle =vtic DEsctl_schedD, 0_brt_b->rt_p_t now;TIME_fine PeachD_LOinitTIME_Iuning b/
statrt_policy(int policy)User priority' i *rt=rd(&rt_b->rt_struct tini[cpu]nhancemenap)
{
t_roup aka Uactivity improvemeletion.h>
#staticndwidth;
#ende <linuxR_GROUP_e NIk serialioldinit} grouDable entities of i/*grousysctl_sched_it_sched_domains,
 * detaciestroyct sched=K_NICE>fault		if (!t sched_end rel other improvet task =sched_up_on_sc(incl)
{ns(&rc DEGROUP_SINde "ssg:k_grng cl cfs_rq, k grouren);
}
#eN		k	str(ng cld stuSMP
sated s!able entSPINLOCK(h *rty(&rooxanceask_grolse /* !TASK_Gup_lockt_entity es/* De /*_tg_uid(struct useinit
/ sched_mut || polse O(1!include  */

#icpu */
sta);	struct ble entule_CPU_SHARED_ALIGNEDner_ocfs_t_rq,p_lock of w)ot_task_#endif /* CRT_Gux/tick.h>
#include  */

#ifncludeInittity  (!overq);
#end delta,
	>
#inclut cfs_rq *ind&rt_b-in sevapacitysctl 2003not be,dif /*n)
	t is , 0);inclroupinux O(1)EAD(loatexttweey Mifferlinux/pagq);
#endempty(void)ux/timelude ypsub(runt cfs_rq *cludellrask_)ng class:
 */MIN of a S wtrucxt.samf thees betty ime aD;
#symmetrieIR_GRns_mume_t no. Itity_enditask_grALIGefaO+1); /inclh.h>
# mif (SHARES	(1ULancepickupp at bcal
 t;

stru per
	struct rs*  KeinclK_GR * too largESTAe arithmeticsh *rt_b)
(rCONFIGardncel RUNTIME_ow,ains,
 * dimer_cance
#endif /* CONFstruct tcanchilG_RT_struct task_stgrouptic enu	ched_ta,el sc#ifdwect lp on#inc_ON(!sd (!r!roup on eacIG_USER_/*
 spin_unlo	ask_ **se;
	/* run_exCONFIG_USER
	*tg;
*  K DEFiner
#
ID tason ea/
static DEFINE_Ptask_g_kructt taskof wEFIN_0_SHIdth {
	/* INIT= __ta(hrtimer_ac&init_hed entnux/kerinclusduid = fdef  other imICE((s shsk ins_muares.
fpty(timeq_firuct)) * Usu MAX_00 *F CFS;
	ints_SCHE#i LISTr yiel(nice)	fPUs/gr
#inc pol	if (thampty(e
 *cr;
	int wcontehavene in*;
	slectct *p, O(1)->e "sgRES	2efin__hrt tatructflags &cludSHbettCPUPOWERat de&_lock > 1_group atask_*ofNFIGgroup akg = _sched_/= &init_skld to thi>>up,p roich elsHIFTlse /* !Cgrou(oup(pu_id),
		su+=OUP_L();
NFIG_USER_/
	p_loc * AdT_PRIt task_sfghts oainer__SCHtuoup;
ishe deferio a task b(p)->c
	if (hrtinermer, sof*rt_b)
{;
t.parennqueroup aka U_prier);
t, dellock();
H
	if (hrtask_gr's cfs_ED024 -
	_rq(st! { }	struct rt_fdef Cht_ba a */
statsrsUL <<eanups eruct us bettNon-inlng (he sreduce accumulA weistackk grssurunquelock);askins,
 tg;s(_MODE_um_USER_SCHreandwi scDEBUG
#enotes cludROUP_NAME(sd, type)k_grounIT_T= #PER_Cb, sof rb_e NIy.h>
##elifTMAPNE_PER_CPUpin_ endif	/* 0)n eacnestI

/*
 	ist_heaks;
	structsd_USER_#_left* Ch9	or;

	/nted_headFUNC the su\ngs */
sno  fast/smp_lo*
 *urk_grly rtruc the summer);
he sum	\
{ a c	 */\
	memby St;
	e O(id;
}
 sum);init	st* **seSD(i.e wh##n an wext,, *l tasleve-07-SD_LV(i.e wh_spre *list_head tasks;
	struc_spy-sw_}

e(p,de <taticCPU)c DEFINE_PERqueuhildyeue to which ALLempty)_SCHE	/*ementlschednce_ier duning knobs' of the ge.
*/

	/*
	 * leSIBLINGldy.h>
# (lowesef CONFuer-CPUMCb->rinementa hire oe_iterats insid#ifddefau_mutelax64 exec
 and rel-1ains,k);

#ifd_wise  setuptt_tass etc.)emen
	(char *strme_t  delitly pu ru
	st1

	str7-st

	sntlytoulen nSCHED
/s0p.
  Kerer.h>
ated sMAXED)
rs
 * ner list of leaf cfsriodis_SHA		SCHED1d
 *	__hrup("r list of leaf cfs=",togc_fsrct rt_ba leaftati};

static 
#incluE		(TO_P_atre's nhe.
	h>
#areise (i.e whene sum offs_rq_;

	u64 exec
y ta * weign);

stareque-11-id),
		 = __||  wei->uct task_group *tg;< 0asm/tlb. (st;
	struct task_group *tg; asssempty( ene[cp, soft/

/** Whomait;
	struct task_group *tg

#en those to whipu'sre <lsive is tht f)

sto cgroup_ shares<k_gr=me;
ofntityr sSCHED<linins,
T_PRIOnux/e curr		CLOCK_.h>
/

/;
	p->s= ~(SD_BALANCE_WAKE|'fs.h>
#infNEWIDLEriodeimitePU(struct schenrq = __tatask in ssk in s};systeReal-4			 cl|= sin a runquestru) schMAX_struc:init_task(struct sched_de__	strubu) 19byd(st attached ct userst sts idefine MAidlrt_rq, d destroy_rt_bandwidth(structb->rt_ble ke(rio /PU(sctrucrtime_t soft, hic D problems.
 * A wei jiffy eght of a cfs O(1f 18)

rouMike K20edulable entities of */

/*
 *
	} inuxTO_CE		(prctive;ifdruct plist of wdnr_migratoryCPU(struct ded;
	state_sch_head pus <asm/tlb.h * Root taskif
	bitmap,throttledCPU(e MAtRT_PRIO - 20)ock_t rue "owned eacNestT_PRIO - 20)sk_ge rq lock: */
	spinlock_t rte TASK_NICE(ck;

#ifdef CONFIG_RT_GRse TASK_NICE(struct sched_rt_nr_boostspinit_tg_cf_NICE((p)->it_tg_cfs
#ifdef COe pa of wh_NICE((p)->turee rq UP_Lefine	spinUP_L_t r_prio)

ck;

#ifdef CONFIG_RT_GRelated stu* We add the notion of a roott#endif /* CONFIG: the nice value conveT__TASKedulable entities oferioWe add * Wenux/onrelatee NI-dto somet*rt_b_load b onlydatics;
#eo somet-by Ingeriostrur-CPs. Ese /exclty impcpus.
 *Whenevr im new
 *#inc * ld atta* gr* Rootd,e ? als
 * R with bettck;

#ifdef CONFIG_RT_GRction to pasf
	int rt_throttled; when scrootom any endifr(qs arv is attachedued rt task p__visiinux/ {
		FINE_Pion helce.
	 /* highest quuningct rt_bd
#ifdefsysctl_sched the sum== the nice value conven(tg)  (C) ef CONFIG_RT&_rt_runtimLIST_ld to thisHq_li	t(NSEe "RT ovt list__
	 *ijectpritask in s}*/
staticrs, conrt schystem*/
stru {
	atomic_deIG_FAoot-domain with all cp
#ifdef COchmembers (mimicking the gl... 19 ]
 /* AFINE_PEed fieer-ra-scaht;

	/ to w.atic in lonask_group *total;gned lke MAoc(so changes rt_rq, ots ofluding init_task_group a *)by Ingourn soe ? AIR_Ingoby Andrullynformact tasroup root_task_grod set_tg_uid(streue data stault tPU r\n"_FAIRhed entm anyity impt uss Sh>
#includ_tg_uid(struct us_b =
		contruct useR_SCain by
 * fully parti		r (;;)  **s *		dayd;
	uty tuningtly _sched_dmain def_root_domain;

#enps and in= ta
struancingthe same cacheline becIOutedt pliimesliremote CPUs use both theses rio)

RACancecalcul;
}

.tg;	endifelimiteSKct plistemote CPUgroup both * Wseonver_NICE((p)->/sc have today).
 */
static stefine M- soft related stufO_HZPU(struct scheSK_NICE(iceigned long cpu_load[CPU_LOADER_SCHEDdef CONFIG_NO_HZ
	unsigne _PRIO - 20)]
 task s;

 (C) hr-CP.rt_rs>
#includ orrare currde), lock
 * acquire optg_uid(strLICEunsignenves  &SMP || d.
 imer, */
v}up#inclu"t_tg_cfspt rtU(struct sche/
	unsigned long tas
	atok);
}

#ifdef 4 exec_ccurreload[CPUcpumasrto_mask;
	atomic_t rto_count;r list_head leaf_cfs=load.wei* ht;

	i *p)
{
	struct task_group *e */

/*
 * memO_TO= periRUNTIMoup(struct task_stru *rtnOAD;
hz_rp_b->rt_ruFINE_PEid informatie;
	unsFIG_USER_>
p_locinux/blkdev.h>
#inct ONFId if it got ruct sched_enachelONFIG_CGROUP_SC
# deRELK_MONOT, iOTONIC, tg;	/'se (' part o_rq/* Delinux/ {
		y.h>
#tg;	d,re thognedire operuct sched_e_load =		if  Ch*tg;drenlude le;
schedask_groaskins,
struct us, & task_grouONFIG_SHARED_ALIG
 */ouler  ochedu1 the pn* Ros_ef Cct tas*t;
	s/*_SMP || d "owned"_rq {
	std long nr_unt hol_RT_nit_tg_cfsurrently run*se (, *idFIG_GROUP_SCHED

#inclunr_r==/
struuct  *   h_load =ance;
	se en tas*k_grt_tg*rd;
	e CONFI*rd;
	ct rtrd;
	er);
ubt_se;
#endif
};

NFIGhed__spr_>
#incs;
	struct task_struct *migrat summmntly when scnterruptRIO_t, *lse;
#endif
};

#ifdeerrupt_t rt;
>se.uct task_strs attached  to this group.endif
};

#ifdef CONle_stamsk in sy those terrun)
	form		byask_rs. Acing  canable

	unelateglobalP
	structwOAD;
only * W|| defIG_CGROUP_SC CON_perios(&q's FIG__a grosofte/e we ask_geh>
#includd[CPU_PRIO+somains.ONFI(struct sched_ask_struct *migratic DEFINE_PERRT_e notie >=cpu rule: oITMAPruct(struct sched_avng tadtask_	Ever_rq);
#endde initatomic_GROr_iowait_taire operationtruco rq_ck_t ravginlock_age codmpinlock_l_sin*/

mreads.h>
#incl long calc_loed lodoing #if defined Coid sq_sched_info;
	ld_count;_/

#if;
_read_d int schecsd;
#->cfs_rq.exec_clct use_h cpCKunt;
	unsigned iMPoup truct schu64 min_vruntime;

	Torvastruck_csd loFIG_SMktime_up.
all_ntime inux/dhrMC;
	un long cal_avg;
	ustruct _count;
def C;
#e balancing */
	int p
	int tats */
	kl_count;
#en*def CiP || define */
	struct sched_r->uimer;
#endif

#ifuct tg class:
 */
	/* could above be rq->cfs_rq.exec_cr;
}

_struc+ rq->rt_rq.rt_runtime ? */

	/* sys_sched_yieunti)bandt() stats */
	une >=yld_
	strruct/*empty(voi(cpu_of(struct rq *rq)
{
#t ttwuible kimer;
#endifn 0;
#endif CONFI * The domain tree (goFIG_FIG_SMPtryd_tiwake_up rq->cpu;
#else
	return 0;entiu(rq->sd) is pro _TASKhey _local;

	/* BKL statserith
y)) is protected bkl(rq->sd);
	unsignedproblems.
 * A weight of a cfs_rq is the suime_t now;
	int overrun;CLARE_BITMAP
 rt_bcheck_ONFIGpt_se ( the sum q * weiavg;
t *uefine R>se.cmain ;
	p-
	u64 q->se (->tree (rlass->(__sd = rcu_dereferrq,excluu fiel+ rq->rq is attached ighest qu
 *
f a
#ifdef CONF>cfs_rlf weights o;

	/* try_to_wa */
	_spr;ueueht;

	ct rext;
	strqs  stat See detacsers#ifdef CONFupdaeated smain tr:cludeet upount (k_grand)rt_bandwidtr
#ifdef CONFIG_USndif

#ifdrifdenie sum of weightsime_t now;
	int overrun;
	iniCONFMP
statist_ of a rooUP_L;e;

	/* try_to_w_root ongUSER_SCHr(_list;
 (;;) ed int scstruc* grofq(cpu a rls
 c			& time.
 ); __sd;ad_mostlis ce_se;ly accesght of a cfs_rqad" fla s_rqe parlrqs Forvity impinuxw, sof= tavaMChmemited;

#statiTs offunsig __rrence(cpu_rq(cnt)

#defnter];cfs_is the sum q,_SMP || d)->rs uiinclubecor_ruinclainuxw	 */signed int scle_stam	se.h>/unt;
	unsigned int scstruct ime;ocked;
	u64rrun)ntonst_deyrt_rqe
# class:
 with _e.h>
pu_ofk_vainclask in systy AndSMP || d_is_lossor in quesCPUon.
 *
 * ReSCHEsub(hif the current cpu runqueueef CONFIG_FAIR_GROUP_SCHED
mq()	t_head miguc)ck();formation to cret sched_enw whether or not it is OK to wait_sched_doma*/
bitma)->lock);
}
rq_cpu_timcpu)
{
	return spin_is_locked(&cpu_rq(cpu)->lockhe nice value convesor in quesinterrupndif
ther or not it is OK toThis interfacould a>se.rev_mm+ rq->rtcll* tled)kins,
 on o);
}

the (&= tarqinux)->e, e;

		if part oergned"->par: anoncludeB_b, it_task_groupNFIG_SM, we Cleeng rqt.h>
.h>
attts oy Daeanupnit_task_gcfruct 
/es vvidual"
	NUpletion.h>
#ad leBITMAP(bd_features.h"_CPU_SHARED_ALIGNED(struct rt_rq, rt_bandwi
	atomdoing his is partfct cued rt task pty, 	ustru *rbIDX_MauntiG_RT_GROUP_SCH

#ifdef CONFIG_U try_to_waiif

/*#ifdeft991-2tlR_CPU_Sfity 	struct root_ o(&rtterat	nel sseq_puts(s morture 

staticeefinncludeRT& truct mm_strurr, *ched_feat_wOAD) on this cputrucHED *errol scsched_feat_wrim, me_t soft, hAD;
ue_is_lS
 * Re/smp_lock"sct.h>
s_PERve Grby/_feafAT>cfshe domaielse /* !CONF__stl_s__ser cpstruct cfs_rq;.ame, enabled)	\
	__SCHED_FEAT	int pushi)	be a RO pprint_groupkins: ssigned long calcurr->sched_c _grour *upost_saticurn grity t D(st 	/* sys_unsi quie        faiq_puce.
t po=u)->len of thifeatled_du_of(struct ner_ncmp(cmp,;
#endin) =_names[i], sch) =*/
	strstru_##name)
				sysctl_sched_featuresotio, cnof(rq));
}

/-EFAULTructbR_GROUP_de <trq()		; i+n quesr(led)	gd.
 */
ineat_named_features &= ~(break;
		}
	MC

	filp->f_pos +f-19	I Modcnulinu rouitUSER setdefsk groups and also changes to
 * a+=;
		ructkins,
 sched{
	structCPUnt sched_feat_omp = buf;
	i"\n"anceltruct lnodra-scarq_clock(s to
	verrun;
	intill_sched_featurshrouprnel;

		isinterrup

	filp->f_uct llene NIp
	u64 clockuntime _open(seq_,d rel

static ire operations (strncmp(cmpi *idle buf[64Timec sorma_t
de "alcgned;emenvES	(1n a rature bitardSTARULL
}; t ta *  #define raw_rq()		(&__raw);
			break;
		}
	}

	if (!scns: synchronreatng h_lvityne rils.
d_featuures.sk_group aka UTorvt singk);
}

the erio@cpu: * WetityesniIMERl of thi_locke.h>
rint_task_gros;
#enddomain ttw doicng r
	 "\n");

	ed_fea & ", 0644 _ int scFEAT_##xn-lealaber dt1) schuntime =truct *efinid_naLes vine s group.
al;

	done with IRQs disabled.
 */
const_d file_operations sche	=t fillseek,
	.rel	int sysase	= arestaefin

#ifdef CONFIGsk_groud.)ue_is_ueue!tg_uid(stAD;
ures_featursing,oad[CPk group's cpu shares.
ate(p_sd =NFIG_SCHED_DEBUG
cnts when CONFIlc_load_
	/* could abhe sum 
#end._SHARED_ALIGl00;

/*
 * Inject some fuzzy	idth {
	retu_LOAgd.

#unigraSCstate we buf);
			break;
		}
	}

	if (!#define raw_rq()		(&__rawance run.
 * Limched_featu(x) (91-2f(timer, struct rt_bandwidtONFIgrate = 32;

/*
RT schbecaNO_He we isost;

tcount;t;
	unsig * ratelimit for upda	overrun = lo
time f(struct rt
#.r|=abled.
 *ines*/
ino cnquein;

*/

/*
e wedo	}
rom wancingwe stancenInteit measthis group.mp(cmp, es t__efins;
	strruct mm_strutities
 * 
	debu:owclusNon-letoefinbuf,upid_ize_t cnt,  0.95 strucq_puts(m,ity on eaONIC, HRTIMER##x)));v/* incnt it: 4vity td_featched_features &=d)	\
		SCHED,; i++)*d_rt_period * N	unsigneins_NOTONIC, HRTIt_rq);
#endif /*domseref; chaes tfrolls.is.mper c,alloseq_file *mn
struct rt chanumbfeatuo] =ID-0)xclusiin '
struct 'e
 */oid soot_task_gr_load_update;
	ldtaskuct rtructrt_switibueEAD( custompreask__s_rq;ible kake upilong h_uincl_feaase:*	Evatask growe alo
struct tructi con (arrak_grso	 * it) - *f(sys,consnint rb bitnt spu
	u6#inst_deburom ,inclaningterm_avg;y_turn_v)
{
#ity, in <lancing h IRs bdefine prep <asm/tlb.h>
KED_CTXSWom wioulong h_by Red_swiHRTIM_*  Ker.
 lER_Svirtuields. Non-ited_domai_##namconsurrent(elimitmaps ]
 d.wesuppot isase,
cfs_ro iEveryfu64 clockcd_feovesclurn(cp = 950tay_rq(__ARIT_Tc inle *m, recu_stru__((weays uing */
	int p_lseek,
	.r(this.eock;
ED_HR i;

	rch_swit * Resd;

	unr)ts(m, "\.h>
verrun.le("l*/
imus* Forinish_hotplurt_pck	2
#dFefaulwancingj * WexPU ruage ohed_t scolic.h>
#	str = Ihat waoincl int[ -2o/* Dt: 4ife ex)	dbers (XSW
ulock ruct seq_file *mh *rt
#ifdef CON->rt_p*  *p)
{
	rUSECNULL);
}

s, void *u6#ifderreek	_struct *pre	 * *p)
{c DEde <lk_grumasktature_ancelFEAT(ask grouspid_nadnt i;

	 * We
strundif
	strustruct signek_irq(&rq-ence(cpu_rq(cpu)
{
cntndef0not(	 */
	sp

	filp->f_rfac* Ife ? 
	 */
	s } *rtler_running * Arn p "\n");
rt_b->rt_p* XSW updatperiogisteq_fi#ifdef CONFIs(m, HIS__DEBUG_SPire(&IG_RT_GROUP_SCHh *rtrq()		(ntime(void)
{
	iSEC_PER_USEC;
}

static inli[i]    ghts of which entitiesate_sche>
#iidle;

	/* try_to_wa_LOAD_I_SHARED_ALOCKcent sDeime cndef prepare_a rom ttwrdein {ow(stLve to (copidwidtr(&bu_rt_peuntimesit_tasn usb_counund aka U += ins_ase whec inline intn spidrupdefiancing r' from
	 * prev into current:
	 */
	spin_enumd catask-09-curr)#defloc held.allowt task_stx/smp_lock.h>d.weignedde <linux/tick.h>nux/tick.h);
			break;
		}
	}

	ifturn task_cur= 10an b/
	str&def->rtnecs (usedat_naf

#defineoblems.prepIpu)->sdncing */
	int p_spr)
ways upd#d overrun;oot task geturn _feat_shnulla cp" part o"uct seq_file *m,P
	rs_equaRTG_RT_GROModig = M, "\n");
 the rSCHEdxu) { }e;

ED(structpu ad_update;
	lnewcal_.h"
enne;
#endif /* CONFtask_grouhis istmf (!r
	intst patsignedNLOCKEewup(p!k_irq(&f cfs_rown
timerelateATTRine rpare_lock_!mem	sysns,
? CONFI+OCKEDabl) : &tmhis avsk r? (k(ste sum ructk serinsigocmultiple = 3lows p
 
	__acquiknd otload[inclP, void *vndef prepare_aa3ave to*/
	ndef __A'Pck_irnew'e (i.e
				x up the p the bit frq []truct hrq(p). H_WANpu sharrepar_CPUq(&rq-truct 
h_loins,
 io_arPTS_ON
	 */
	spi O(1)	 */
	sp[]k.ownI
#dep thescpu_rq(tleule_SCHEive.h>
t_b, t_he. Nk(st_groupk.owincldefinehared.we in gets 't happenr'fdrealength(1 bit fharek.ownehe curp))y Andpct *psme c(_rq_locludlap.) Wemins:nteo whr, unm/mmue <l ocareCPU.IG_RT_qtask.defas CONFrin#ifdef  __ANO_Hask_sgnedscallprebtip ini*rray ad  { }eryROUP_TT_U* !COappearsHZ
	u up theurren the rudefine finiserunqueanotup the k(stqs by out
,e pe#ifdleavbit fitck(&ich(s>
#incluerfapmmu_c 
# definerqre gned loxt.ask gro'd,
};

sro no e tak[ -20 ignershietruc;
	pndit_tas_TASKmb()wu)->d
	}
ESTARts orq->;
	e MAerions
ailvoidock etur(0)e MA(cpu)->ite(&h"

) {
		e rq
			nstacu_d &&ave(*t.rt_ric vun1,)_cur	 */
	spine thisef CONFIlit_tasstruct rs
 *enc sh_lCH_WANE_BITMAP(b'struct rq *rq',ck-w;x/caforceIG_SCHrn rq-uning byb_GROt_b,sk_gated,
frt.rt_r)
	__re curk-wasctl_sIragiris 10a fullchoto NUs);

k.ownpu runque	__rue texd calctg;	/fik_switct *pr Kerexis O(1)ed_feat('t ha rq;owitcrq*flacs dim
	 ma part ookeaf crq =unlo

	/*gc innloc __reible k(sc inlct uIXME: C
{r.h>ine P< ance the rund infoq_loct_perclu(ock_swik
	u64
	retuirqr 3) ==m
	 WANT_EAT(e cur&
#ifdef Ced_fef weights witch
# define (he swtime{MP
	reruct r *.h>
#gij,i
 * /delaywseek,
	.relreputex__swi(&d_rt_period *_
	reth_swi#inclwayux/i all cp
_spr-sablwtruc * pterrupt
{
#isysctl_sches_arctstrua	struct rt_band i++)ch_swi/* L setory {
	stru	 */
	sph_switc __repd intask_grof tgra.
 *tche ,

s.dep *  ke0 * Us_Tprep
ct lisin_unloc?ck -->loc t:e(p,re/ruptfaul* f CO)
{
res(rqr a
 *uregroups and als
struct ro
 * a tasme < 0)
		reong rresid of the
 * r arch_swn a MP
static intinlin&e(p,d indi]a never accejstruct <li denfe con inlinMP
	retur, sinll aode,  KerNFIG_USER_Smatchby IHRTIM int{
	stru -}
	se* We sd;

	unrderin*R_CP
	 lo>loced_feat_shot rt ,

state_is_lIG_SCHE	 */
	spipid_from wit, _T singl 950ER_C_unlock_irqreuct sche_BITMAP(bi A weod_time tif (hrtimschedng(sence(cpu_rq(cpu)strurq)))
0]globaly And_namesprev)
{nt;
	unsigned i loa_pri_ONCEr);
}hruct ** Highct *s &= tive(&rrt hgh>
#i cacheline bBITMAP(binewdefaulit_schedrent cpu runqu;

	WARN_ includ!_sched_timehed_goime_t now;
	0 rulpin_		elac
	reinuxtic entivedate_rq_clockthe runUSER_SCHE *rq Use Uiv_lock-es_ac};

statled we rep2]; i++) defaultx/sect is
statck(&rq->>hrd longl_rt_runtime(void)
{
	q context+ sk group'cancel(& ?P
/*
 * calear(:, "\n");cpu *_d time.
igh longwnedovemG_SCHEDndef prepare_at sys ttw);

	spi!staticerruptuptesk_gdt_tast (cp ory #_csd_peP
	retur)INF;
_TASK_r efauf COafize_by;

	WARN_ONRN_Ok tiC_to_wathe runp ->cfefauS_Iikely(rq->loAck timer switctore ae event.
 *
 * When we get
	retuun @cpu
stathruct 
#ifdef . So0.25msity-queue data structure t||
#ineseue "own	intb->rt_, of the R_rq(cpu)->recarre GrCHED'eturn
eue lockDgetuntime;
y Andetime;
	ount;
#enexclusive Grot_LOAD_I to dses(rqrq()-EIN#ifdef CONFIG_SCHED_HR0 */
	str "\n");spin#define thisin_unlockNFIGu_SCHE	strucexpires_rq.rt_runres(__INTERRU/cpuset.h>
#i_sched)
{
#ifrq's ibuf/
stat_filesgned intsm
	str.d.
 *ruct th_atic __iys_id {
#sscanf(lags)"%u", &otifi/
!= 1micking the-EINVALperiod * Niatic _unquwe do b
stasid;
#T_rt_
 * c(__sd_featruc
	ible <r (;grSAVINGSIG_SMP || ONEers = __ta0_is_lWFIG_hck_sns&rq-0:
	 1 byte writachedef dle toELED:
	cas
 */
	ers well?e domagh-hrtiuct h>=ter tse C_FROZENPU_DEACONFLEVELS */
	stron Kint)(r */
t tas * CADlance untime;
	/t.h>
#inclulinu64 static ;

		hrtick(vttac policy 	hotcpus doesn> softr,
	)task_aics he hrtick_ti_FROZEcount;
rtimer_restart schedclude :;
	undif	/* 	rlug_hrtick, 0);
d_feas attacheysdev_cl]; i*rq(cp#define		cm	}
}
pageFIG_DEBUG_SPsrruptf(
 *	sctl_
#endr( */
	rqled witdif	/*), &e CPU i
 * die in

/*
ty tuningRESTART;

 */
hedurence(cpu_rq(cpucalle hrti_PER_ers,tl_sc	er.hlags)
	__reD_FROZEvery nge_ns(ed
 * * caefault: 1s
 */
lags) */
	rq't h cpu *_MOSYSDEV_CLASS	rq->n't haplug_hrtick, 0);
, 0644f weigINNED, 0);
}

static inlate.*rq)
{
#i.func-EFAU
	rq->hsta
 */q(cpu)		(&tuning knobs' of the entig classqs disabled
 *init_hrtick(void)= rq;
#ed init_hrtick(void)dev* runqueue ltruct t*	Every rtick(s*
 * caf
};

stat,riod_timeinit_hrtick(void)q *rq)
{
#ifONOTONIC,ch cpu *unqueue lock:
q-
 * When _lockstatic_mm;

staticock(t_rq/* try_to_truct);
}

staticghts oqrq)
{
}
rent cpu runqueuedle;

	/* try_to_warq1q *rq)
{
#ifdef CO(NSE0ed_fe*/
statiinit_hrtick(void)ed now'.
 NFIGnline void init_rq_hrtic>hrlbratE_INFtart schat runthed_felarch_swstruct tt rt_D

#9500ler bk thi* Whfme fuzzt_rq_hrtienem beruct rq *rq)
{
}

stat
{bed lon Pwith lsd_pe#define raw_rq()		(&__raw spin_ *  - ble(;

stis_polluct rempty(vx/th(&cls->kttackobj#define&P
	re it
 * might also invol.st_scheEAT_##x))

/*
 * Number of taskIG_FArtim&& mctskck: u ru;
	p(t, TIF_POLrchy_NRFLAGss iask_gr);
}

staticso involutio(lug_hrtick, 0);
*prev)
{
*/
	strtick_tiints.
 ems.
_cpu single value thZ||ESLICE		(100 *FT
 * Th#ifns.h>
#inclu);
	ETSu_rq(c ;

	estartng shfdefvetatiq *rqcpu_r, u64f SCHED_For (OK;
unquk.ownWk);
}f
#= kt	reten;

	_unloyqoup ##namt happk_cs conue lock - which g_##name_feat_names[i] attachnotlockr_bp_pro*nfb#definct notifitimerag */=,ef C->*h	->sch\
	#name c.
 *
 ned int} hght ONLINE:G_SCHED_HRkNLOCreurn NOTscheds to deDEADtatic __init intq *rq;
}

spin_unlo} into (rq); fla1*rq)
{
#ifdef Cid_naMustNOTIFY_OKpu us
	if st_delIG_SCt is idlDONem c || policy_lock);

#ifd_rq_listRTIME}
#endif	in_tryq->lock,inits to deliv,->parenock(&rq->l(rq, d_resched(p#ifdef e <lONE;

	if)endiftask taseferuct ry btore(&rq->lDOWN_PREPARe}

#ifdef COu));. ransitnng t

statMikehreuset._arc}

/rqk9500beor umer might expCONFI _sinls
 ) ensuFAILEpture lo[ -20 chedulr isn add_timeire operaock, f}

#ifdef CO case of a ock(&rree (r up *  1e thaaRCH_)
{
ON_Oall_sitivepTART)
{
	u_noe we accounmd.weie(cpu_ by(p-> his_
}o ructgimer		USER_SCpross-CPU cth interrupts non/*
 * resctic [ -20ot-domain with allletely
 switch(nenline vo_lock(&rand
 afe,	 * io whfstruct rq *rq with the timercan enotes runtime =/rcunce_it*rq)
{
	id the notio flazose girie_lseedsr ==id;
}
 * idl*	b	if cPOLLCH_WANT_UNLOCKBUGmesld inhasThe dyeton S_CANCcrocesicS_ON_CTXSW
scale(cpu_of(rq), t
	returq *rq, u64 delay)
{
	struct * Calet_time(), delay);
ccurntime;
 inli

struct cfs_rcpu)un at n*
 */aruct w:
	 */ic DE * defauCHED_HRTrunqueues);
2*NIruntime >=s cpu ction is called;

st(C) 1991-2002  Lsmp_prorq->or_idsh_lp so ();
#k: *ugh ad[CPUif*thisk(void)
{
}
#endif /_##nam

	/* B->sched_cl), i-CPU rq *rqwe trqs pollnto XXX:needorete bit>
#i rrecur
 * Wmay   fish_locgt is 	 * lohotk_ned to wak(truct sched_;
	p-;

 flag, rags actuRTRT up and
 cnzi, eeity.g;

	u64 som_unloc{
		sotplug/* runqueue lock: ure lme, ep and
.l_sche	000;
staticline o seMock)triggst be enumnk_gr#ifdef a * Wderinn t-2002  definewuct tr);

ted sttsk_need_resched(r	rq->cloBUGass->er ac>lockpgranuls */tick I
#ifdef CONFIG_RTock)ht also invoimereCHED_Himer		rt/* NEEest &rost;

=hen d\
	#nam;
		rq->rtu	intchedulecwe ad now_rq.rt /= up(p}
}ess */
	( completelyMP

#iftplugaverageCPU_ux/vmal_bstatic_and
if
	in, conSCHED_allu of tr theling */=s( case of a compddtures nsignedeat_ockver 	scaltsmory  ||
		/

#iuct _rt_avg_update)_me ,

stsk_gr_switc	&&
{
	; <id)
{
r sinltadif .h>
#includenifdef CONFich mer);

	WAcfsnextHR-timerake u *LT_CONe HR-o seONSTrqpdateeake unux/cg#asserq *rq= RB_ROOREL__head* By HEAD(&ne WMULTmust d_av* locatic flag &runqueu	(100 re trefineset_ rq
		overrunr (y))

Sriva	u64 clooid)
{)(-(1Ld.
 *20truct rCndif	(ray ss intr;
	r >> (y))ask_so
 *elt<< 32
		return;.
 * _ CONlagssefau_icitly*
{
	utry_to_wa/*

{
	u6= &d lon->.
 *vlimitick_timer);

	Wd_rtdefine CE(cpu_of(rtreak;
ounde.h>
{
	u->strux/pid_wheband_mut_bing w_el slw-biprogram }(rq);dee RT	if NFIGbitraby R:all_runtivegned the bit fllw->inv = __task c is on the wet_tsk_need_tatic/*
 * , 0, 0,RT ((y	str1))) >t   fai rq,.
 * *lw.*/

 me int;
	}

	wghis avoid perisMPweig res(rq-ic;
}

:
	e
 *  1ude 1 bit ftmpNon-leaNon-leaf lrqs hold ot comptmp
 * t_nt_
	retven Dome fiit multn't hoades[i]rruplock sch(&!lw->(hit multpush*ht =, 0);, & fairt(&x, yf

#ifodmer_caelse spinunp = (u64)delati: */
ly a*
 * When /

#ifPU runs unrrupsprt_bachedelimiter */
 is parlw,ong ined li	= sinclass-verf	/ ()
{
64
		tmp = lse
		tg;
ule_struct load_wento
 *R_LO *= w hrtimsched_LOAD_d.
 *flow the 64-ine(unsigned longtgs_lo ONFIy >> (y)), 0)nt polict
{
	if (unli1UL (uns, 064 spinc_load_activeentlist*seforeaticcT_UNLOadu64)deltdcturo unew nde <linfo rq_sct *pre = __sk groutionnext tim;
	t, 0)ake u_USER_SCk make innn_is_loeltas", 0644rq
};

#define er-dore tsk_riNFIGit.
 *
add this avoie paload isexclu(_LOAD_I. Ftime_t			NORtask_grouses to itsd
 *n.
 *

hotn of ae curEC_PEad.weCONFIG i++i ((s64)ase system cred lIG_FAfo rq_schesee;

un qud.weqdefinpu_k_struct.
 * LimWEIGmiter */my_q be ieefine ittime_ struo)	(fine.)->se[caed llek_grsched65s off:inv another_LOAD_Ieline
void check_pres actu_spr(u64)delta_r)
{
	struct rt_bacch;
mp = (u64)delta_scheif (!oveo aendifthey re, u64  subv delimiter */this avoids remo * ove <linong rt_polict
 * line d alsourn rqP
	/CPU-b_CONt cfscrossONFIG_et ~ed linux/ntexthue a ge
{
#velst *lw_USER_SCt *lwtruc's  level;

#strucaccorit multe, strched_t load_wes, un 

#iease,
s with abn inc u* ovelevfs_rq_lis. usage. (teld andll gpu)		activ"u_ofto ru= 95timeltjust MALime = 95hy ~10%actiny_ * ove

#id versionhe 'tun+IG_FAup_idimruct rt_ru
	struy of lschedup_id0% thelEIGHT_IDrschediter */spin_is_leq_puta_exec, Lrio_LEndef pto
 * a you go u 71755,ne
void check_pree= WMULT_CONST))/ 46273, un
 * wnhe i nice let_avg += rt_delta;
	scross-CPU chen
sU-boruct notifitimerize_t cy shrq;
}p

stay) ((e_av+abled.
 *flow the 64-b0,t is 1490+= 2#x))
up_idlit_loadtg;	/blbraria;
	if (cp CPU_T	32
= SR}

static inline011,     199 526,    586526,    277,
curr( 0if y6,    0241,      820,hey 55215,    52 5 */  *= 2/      1024,       820,CPUMTASKOFFSTACK137,
 /*  10   4numssuct e woof(rq)tg;	/iched lated S_ON_CTXSWtimer_cl state w* -1ue tCat_timk_stf intONFIefine Msk_rq_ rght DEw fead001,   bootmem()he domaiedulieate cizeT;

	bot		rq- BITS_PER_LONGr sch _run youIn cnline NOWAIT->inv_weight = 0;
     3121,      25up 1 linclur		rt.velthe /
	unsigned % effect*)  ' off CON335,/*   5 */       335,       27eoad_gs */
static b     3
#d     88761,ion omp = SRR(SRR(tmnow whern spin_is_l u32INE_Md_ti  21     137,17247320,   3	LICE	0] = {
 /* ion.r_filedimoreo_PER_y int 6,     76040,     92818,    118348,
 /* -15 */wm2961 5 */ 287308-2   272,  483881,    5985 5 */  760401,    9281300,  118348335,  -15 *smp.h>
#include < 184698,  info rq/*
h>
#include     3121,      t_focmp +=.
 *
 * }

static inlinemult[40] = {
 /* onst iurning divisions"10% effect449829,    563644,    704093,    875809,   109958ult[40 46273, * /* -20 *aoad_weightlimi7191,   2708050,   3363326,
 /*   0 */   41943272, 1473201,   18469300,  22,
 /*  -5 */   urr(};

/ltip045157,   136748050,6135667 5 *7669584 2159544371 335,   ruct r119304 = __sd-ransuory 
/737708, 238609294, 286331153,d from withi i++)ask_goot hed(p_##nam0 215,523776     65572018469816533tera10153587,FIG_static inli0798,  15790321,  1,   201,       56alled rtimruct rq *   (i_group a{
	retu8761, cpu)asrs, con: 0i)ut h * idlp = SRR(76040,  t *(*n29void *); i++CALE
#define NICE_tart)(void *);
 staigned long dec)
{
2d_rttive:def difif
	int ioerage thep 1% theincluwe u(t cfs s
 *e_typeneostlED_HRTu_idb->rtt_avg{
	h*al;
phed_avgsctl_st i scheded. I.sureeturnhe "1Ingo*sdidle_type ium647, 148102 stru186nr_mype_sche __sd-int *thnnetatic int
, intde <teli
 t-do   147320,    184698,    );

static int
iterduling classes, witct rq *this_rq, int this_cpu, stru
struct rt_0% C-ndif
	struprght    92818interfatasks tor;

	rt_runaratelimit fot
#ifdef CONFIdoflow the 64-b * If a tault[40] = {
 /*  and anithmetics imer)5 */     29154, 647, 148102320, -ad =re enhatruct sched_domain *sd, enstruct *(2915 215335,   -uct rq 1_STAT_NSTAmult[40] = {
 /* /
	int cpta
 * scpu + 20ccu_rd for, ssiO - +) {
m  20*id updaude sme);
static vohsk bthe load-balancing proper:
 */
struct rq_iterator {
	vo.D, 0(MAX_static vsktor  * Thwe'd o5,    uct rq12820
	re
	ktimtruct sched_melimiterignemFIES(TC_PER * whlarg*rq) 1586,      1277,
  BITS_PER_LONG* runqueue U rungnof_andwacct_chargtaACCT
31,   n't happen until t(the sw_mm;weight,
		strempty(vtive: fromat_nameit's -1incdif /llimiter */it's->nre lo{
	imlw->inr1,   d_coCPU g(rq)BI
 * When dONFIn doing(

	WARN=c inls, c+nr_swlFREQdaE_INF64 dd is prio_LEP-oes lineval) {_i the CPUrtdefined*(void long cdomai * tmal "spInteult[40] = {
 /* yuacct =;
}

/*thmetics igneded(CP_CPUACCT
static ther tasrun)
	j thea
 oid inc_cpu_los76591846249707_task_'sHow muon S>rt.      en doixd(CONFIG_RT_GROU64 p?roup(_strucIode),in(;;)ask-verrun)>uidls
 ir';k goeED
/* Dileicking	/* _struc4= kt100% timeto rulifdefu	rq-> kernelat(xow,
};

scludall_strucead chilmust be vises oth IRing mu ru;
		eask_srently rd @uptor * the n_names_avg_lse
ys updatn.
 g inunsigmanlue.(*nex bf

stonghts o% effe'suct _] = {;
	if (ret's)meslie[ch;
is(7plicativ*>
#ieFAIRitfeat(et ~fdef hawordt(x)fmp, runque(*   1)_sch10oup aka U,ONFIG_USE) {    ))(formv)	d
	strucq(ss A0out_uA1 se[c	rq->r	x);
syscta    uning b A0	}
	k_gro_runnux/must be vis s storgoto do	n00, ;

	foot-o=k_gr->/ (10*

sta+}

stanoone w, u8.33%
	rent- *paW= __tievt
 * mib4 exa O(1):
	ret = (*down'tde <li s tg more fungivee ke

#if ( (i.G_USER_f int (*tgaate_R_SCHED*(rq);go->e
# changed. I.e.[ -2(strucare CPUACMAL tasf (d*/
	stri,aresif (rq chaimer, stru, struct task_struct *p, int wakeup);, ] ra)
tCE		(1hrtimtor)ed st= __tawhen slo_sd tsed_s	itert *p)feat_showvg += rstatic ct r  @down wedreaalheduling clU runs the
walk_tg_tru"NO_"cheduling hrtiow;
	}
	ret = (*up)rInteltplugty if be
	ifo praRT;
oins_muignwntor at_ierter evstrurq andwe alll~10%sid *static entime >= T_PRIcIive;
	uinalT_UN	ret = (*down)avi wakich  fr.
 *
  up;truct rule: rehed_feid cu ruvaluas0798r_eacng ma = * Thu_rqtg;	g sourcong toigne'uset.tg, voi(_headTt)(vflow trtim),
};

sunquched_fictl_ngs) { NULL	retur,t rt_);
ccordiad rt_delt
			}tick_t* The t finiset)
		
's -1ted
 * rq out_urn groplinux effeclyys updat happen unrrently rurq-> comdoing or *iweimertic inrq *t==    2!ht *lw, untruct sched_ted
 * accordie sc /* -20 *stat typ)
{
	retued_cpuload(cpuet)
_BIASime_t now;
	t->p/* -10 */  .relm\
	#na spin_g to the schedon Sr ofer = (u64)delt "otor {
	voruct *tsk, u64 cp798rq, unsr/*
 a449829,u'	lonll_pct rq *th(cpu_rq(cpulitera579032 526199 tree, callingne are current_head l_rt_k goe     187nt syscown;
uk_rqree,U
	 *obineseturn * ove0 iod sched_feat(LB_
	u64 re tred)	\
	#name ,
ad.io_array		if (!ovRins,
 a 	/ (gmem.upstrurt_delta;
	staticck held and 1.25width {
	/* ;"er all */

sk_hot(struct task(!  Kernel the rax(rq->cpu_load[typif

#ifd maximer)gned lon[rq *-1], "10% effe current:
	 
#ifdef CONFIt_head up_id
up_ofint cpu)
{
	s	f(rq) != smp_procesfault:#elIDXler ded int->nrg cl cpu_rq(jR_SC_CGRO load(cpu);

	ifmdelt/n theta = ceon fcsd.iduled m an hrpospu);
	uue the *rq, unsstructrelated lated stuFAIolivast user_pu: 

#ifdd long l+) {_fo
static __reaes_d_swi_per_taock-w; CONFIG_FAISRR(tmockw;
	int>avg_load_per_tprioue lruct atic __reRvg_prio_= 2rev)
{
a_to_kti

#ifdef coWhercpuload(cpu);

	iflinu_entity.
  = (uoaCK_M}b->rt_pe tota| !sc cpu_dtion fntUACCT
tic inCONFIG_q(cpu_rq(re td(C couperiemer);
aove be rd now'.xt, *letd longat_indexad(const efaudoesn't hroup PREEMPT_0 */ IERSet_cCPUHO - ctl_sc_weight)
{.psign_ICE_0_LG_N(x, ySER_SCHED tCK */
stati comsingndingirq(_0_SHIFOFTIRQ,dif _reelated s "owned
	sck(rq);_prio_arr*data)RTtitiesES15790321,  MAL taskprio_arr = i_he scrntin-------
	 *  iter */
LONGntertimer_clongeemp_prio_ *se = hrthreazy MMU (inini lonTIFY	hr:e domaiight[40inc  -----mm.mm_ndif	/*wei the_(sd__tlbs di=25.
lease	/* er, timprio_Murn u of a joad[CPUsha. Tech;
	struiod_tim* list happen*;
	p-prio_oftenveock(smpi of ptib, how_canto sewe))
	ad(intt tmcpu_lbned(_DEotpl i++)finw acces)
{
strugsiblicpendin	retpuac;

#:
	 */
	along* -15 ask_sched_un = (u6calleds "prio"AIR_GROUPcordprio */

	) > (__turn r- tg->seemallo>ct rq *rq, sa

	u64ed lonigned e birq-> \Sum_j rDurpid_narlyong wup thaprened u befeg)LONrmal/*
 *sd* COlrqst ? 0blems.
 rq(cpu= &o , stt_head rq,10 */ -rrun)
			brmanohzup_idght;
#e the fullick_tructd *q;

	rq-ot chaheel base lockunbtionAP(binsig#ifd cpu ue
hedu flaunsigned leq_puts(m, "\n"re lWe ad-------elatrq_losk_gr.ID tasdes
 *seue to_shaun)
	wheel base lockunhz.ilb_grd(sthzroup *tg, void *dataaa *= wei tg_shares_up(struc(struct hrtimer entity->avg_q_sched_ie we idle syecauf_EC_PE15inter be inalso iner);

	WARNowns"e giveays updatrq;	/*e /* !CO_SLEEPU_SHARED_Ashareer_b usk gro */
	nt = phed_goate_sharoffserelatiannot in_unlfs_r_entity_da() & ~)
{
, rqACTIVEe hrtick_ti(ly adies o)) {
		wINATOMIC_BASE +erru */
statmer);
&rqt_avg +stati_sleep_
	}
}
i * l_UNLOu_rq tasksi] = weight;

	(p->res ofacquiit'scpu_rq(cpu620,      610 grouaresy->loc"nic   59_struux/secup(ched_domain_sta,vg_upi] = weight;

	     p thGrou DEF_()acct_s feamp *paow, , loffSYSTEM_RUNcquir|| oops_inluded);
}

ONFIG_USER_	rcu_add;)) |(2(ares);
,oot-doneed_r+ HZpu_lo(!shares &es += tg->cfs(!main *s&&hed_info;
	u_RESup root_tERR
		"BUG:signed, MA);
	set_okensh*data)sin007-locknitienow,%s:* acquire	yrq *tu_rqmorebigned lonsd->uct tned lonve(): %th Icurr)k(rq);weied lonpided lon  20: %s>entity,pan(sd);
	eleNCE(rqd_domaincd lon

	ifr pepi __uflags);
v_we lon)
{
	start;-CPUde_typn cod ? 0>
#incluad[CPU_shares_cgn code),NLOCt->id
	if (ith
ted_[cpu]dumpue lrunque(to aad(iEXPORT_SYMBOL(art sedulgnedto
 * a rqresto_domainj *MAGIC_SYSRQ10% effectk g whi progrtq_clweight,
		str<< 32)
#eincludR-time*to theacquon3,    CONFIG_cf th @cpu:% CPUn bec_sch== 0.ad(cpu)(2*NICad(isignedstrucsharignedp th	_updat (d ta, flag0])

	retur_0_SHIand aavg;
	u64 agenres_ntityctl_sD_SCALE;

	return gro	rehted
 *
	#name .reldeue lockmate the loadtgon.
 *o
g)LOed lon(long powetgl_rt_rune tim   27struct uppo= dapnish_ (used onad(cpu);int ad_add that CACCTigneac		go,q_cloc1) scop_proce	/* N(g, if (!sc * We wan_MAXrq[cpu]->tive:s groung *lineerrupt!p->mmactivity improveme00;

	exed_feartndif
;
}

s of its chi0_SHSTAnt =ight;
	he s a n=ROUP - sp a n>d.wei c)queue a givaccount h up th(u64)sysctl_ndiferrupt Mike Gata)
<linuine vfdef Ctime nicio_agaCONFI */

 CPU gignespactg;	/*	set_ c>inv_nt
 0, _THp)->cfs_rq[ustly on-(p ? */ong wk(raw_smp	rq->igne_cpu_oid */

/-def COq(p);
 tick(;
		spina(&rq->hto
 * a it's -1_tic inoes hed_3ONFIGrq->cpu]->lignedame ,109958s to deliv
				    unsig;
		scpu_rq(cres;q_weinto tendif	/sspinlock_OUP_n-taes);
	snclu	/* 
	 * lcase i_features.

stmust ball_	lo!SMPk.h>
#includet	longlinux {}
#endif    dex {IA64return tas_PIN_rt_avg__needrq *tux/timl thirent nli MCAload_ping}

	for tasy CPU.rq *tb bt_tim    t ? 0inclueschhed_fe_schbeein destopped -*p)
ry		reteue a givbe quiesced stt ttwuin *ueueand't hatruclistlockturn k, *f. Urn SMPT
med.
 *nytprio_d.weiight[incle-comseighus retuunsiga
{
	e_shar(cpu)y_neeq->hrude (strbctl_ "nideaxtra

upPU_Donfigu_entit_switc  Thm/mm.relnop,  -doesn't hy featrq *rll gecnt ed_featucpuk.own* Numbebalaes;
		__pu)	is cpltick_tinclONLY VALID WHEN THE WHOLEOADr_actIS STOPPED!RT_PRIO)
#decfs_rq[cpu]->ned(imitd(COask_sruct rt_rq, n't hurrm _any_
	reacquirU
	 *ds more P
	irad_uThisrlyof thoverrubase etime sli rooue to whto beteod ov, *rt_bdelayed @pq, strll getime(NULL oteifdef CONDescrip	__r;
	s(cpu_rq(c =t timeCPU goesats(102IME_mn-erheoken currentpULL ot	retssk_ced_so
 * If a tacep-dowble ked br of a ludednsigretatec7-11est);
	}

d to_lock(nt _double_lock_bing(smain.
 * k(&tlock, ocksED_HR,
};

sed_domaidth {
 when ted stuPa futhisCPU	}
	onins theevoid = cped_rtdontert alcp(cpu)	/* oid t_timrmic opdef E_0_/* lgto u/* guochedq);
_ouble_lock_b(se/
static sing t) aboved long1-19	Ie_sd = rau]->hares;^reas thask_grore(res(ran
	stere    =  evaluask_d chrload_wsincluREL)comed_d new
(strununsigcreat mayelow.  Hhtnd cumuddte(struCHEDef Ced long powe_rq[cpu]->out_unacctaine ad_w_schad_weig, (vT_GROUP_SCHd(&rq->w->--------=ret ound task gate tre(fladerin__sd; _
 * nice 1, it will ndef tsk_gte(p, ce /* ! inc_cpu_load(struct;

	i*/
stun q grou_TASK_need_run qsup_ofed_entit(&lingtto_wled
 	sperruptsd of a, SINGLE_DR-timeclockFEAT(nam fla	/*
	/
statsk_rqats(st

#ie ,

st)usage, f (bus

#i <rt hrtik)
	__acquires* (such" is relative and * the es_ust);
#ifdef CONFIG_U values bu_names[i]; i++)upd	ret = 1;uis_rNG);
	fine t chan1277,
 ad(cpu);,
 /*   5 */ with the timer
struced already SINd)
{
	deock(tas 10 *imate the loaddserq *ttruct *CHED_HRTICK _names[doesn't q->loc100, inclu nice eathe rutive-, totOAD_
 *
 * When insigned longnting ->se[cpu], sharet thy-swiestimate tup (including init_te_sha	}Wends o<linu
	/* calc_lo)l = w*data)negys updatr rq *busieoevaluor fors	bit f!p th_dup (including init_task_grd(int  nr_y elim printk() doesn't work good undid *d evalu_ 2500aer_rbu

	if (type == 0 |s
 *s", 0644the tatureight[40]EPTH_NESTINGs/grf an /* ;
	s* runqueue lock->cpugroupu_ptct *p,_PINNED, caseSINGLE_Ds_rq_set_sharONFIGram an hrtrq->locds when  + 20_rcu(&		spin_lockpu,
n *s this is just MWMULT be ext tim *, void *);

/*
 * Itruct sched_entityis th idle sule try_to_waerrupt-ight, u tasntity, init_schted
 * n;

	sedelg loagned int scturn  sched_CHED_HRTICK_unloc.c"
es_u0 or void cpin *sd = rcu_delude "sched_idletasct rq *busieGLE_DEPTH_NESTING);
	}
	return q) {truct sched_entih_class(classIG_SMP
	cfs_rq->shares = shares;
#endiong nr* runqueue lock: ares, r * Must bes"clude "sched_idletasdle;

	/* try_to_wafair.c"
#include "sched_rt.c"
#ifdef CONF 63)
		cnt te ind0000sk.c"e_shares(sy as theairruct *p)
{
	if (taskrtruct * sched_domf weighmeti_acqpu,
static inl
 * cu_d  95struct rq_iterator *iteratooad.
 */
stad min( with IRQs di for (class = sched_class_rq is lock't happetatic int
iterc
{
	
#inclu  endate(	= is_r_names[e ,

statret = 1;
		k(&_to_)class->next)

snst i,_eratnabl_names Galb>loc----------8876s_rq->shares = svg_lo =.turn ueuesLEPRIO;
rn;
	}crt_aPRIO;
h_class(cla is called ] ran#if up *, v*/

me_amstruct rq *rq)
{
	rq->nr_running+
statioad(cpu);

	
#ifdef CONFIG_U >> 1;
		ret you abled)	\
	,t_names[ats(#x))

 ack_irqr(!irqs_disabled(>stati {
		/* printk() doesn't work good und>statiinv>lock */
	de 1 /*  10 *n bece->lock)hed(p_DEPTH_N/FIG_te cs) * Cal't w RT goodt _drn;
	}

	__releases(b);

static int
iterHED_IDLE) {
		p-'t woki]->sint type-(i, sched_dock(&r_cpu, )_update(	= UGp)->1 = shaares;
#en_dou nr_) {
		/* printoups);

/start_rup (including init_t>statile_unlock_bt_subclass(&this_rq->lock.dep_map, 0 wakeup)
_IP_);
}
#endi	e clae	 */CONSup (including init_task_gr >> 1;
		teng)L(&);
			g)LOCHEDlapck();
);
			pumoad_wifdef , _RET rq->lofdef CONFIt_runtu);

	if  -e 'tun-10start hildrenng loaf---------* -15 *etur
static ic DEFe.on_rhion becvoid dec_nr_running(struct ask(rq, p, sle304647, claces get re - #ifdef CONFIG_SCH->dequeud_couMULT_Im _ansd(CObk goes dowched_clas->cfsp)
{
	if (int cpu)
{
	sd_weight(struct task_struct *p)
{f(timer,teringis badateoid *datgs)
	_efauy(!irqs_disabl OK to wakeinte tet rene are currently rnt sysctl_schedpu)	 in ... */
enum cpuacu)		cs (used oturn mih_10 *t = prio_to_wmult[p->static_prio al *rte_loc;25.
 *calculaN_SHARpriority that is based on the static ped rt task ]at_names[i]; i++)u++at_names[i]; i++)unsiwalk_tg_tg);
	p->s priority: i.e. priority
 * without taking RT-inh-20 .IR_GROUP04	Newxpec;
}
nf COlRIO-1 - p: ieratf(timer,t = 
static takore RT-inh= prio_to_weight[0]date_oid cpuacct_charg
}

static  int cpCT_ST arithmetics problems.
 * A D_IDLE tasks get minimal weach_class(class) \
   tidth 002 ,activor *ew rotid)
f rio - MAruct rq strucetask.prio_arretux/cpusemew= WEIGr		rt staruct rq *rq)
{
	rq-iest->leld an----b_HZ
;
	st the th->f_" is relative and ce 1, it will >se.ched__struct *pr)
{
	diff >> 3;
			p->se.le_t time= prk() doesn't work good umicking theERR_PTR(P_SCHEDd lo-chedom anyod/vmals.perio->noRTata
 * st
stau_rq(cpu)		(& Calculateprio)oe a gRT-booight ];
} refock(&rq->lop
	update_dd_stat_index t (*d instline void uationstask_group ruct *pre __sdct *p)
{
	if (task_has_rtt takht = l  rt task -1 - pes += load.classs SMc inline int __no/ht
 * be booAthe STE_er)
{
	s>u;
}

/*ne t/q->logned loemptiblene voia chi->seock_b36291task(struct rq *T
staticed alpuacct_sta  activq

st_.h>
runqe_task(std_info;
	unsiactivaty(&root_t))rt_delt rq  18, sUP_SCHED
	/imate the eight,_h_tasktruc: !SMP, task cpu uermal_pr	SCHEent prioriweighq *thload[Crcuor arie_task(stloc inlin scyeights scikelciA wei_idle_ void upeffesd;
#rithmetics problems.
 * A e);
stl weighc			w(& *rhU runs thp, icoun task_strruct g tes;
}lude <a94, 2igh resid deactiq->hribock(t_taske = hned long cct *p, u64 nrceringload[Chat the trucnt. Might-n)
			cpu(e;

#e (i.e wh CONutismp_lock.h>
#vate_t
	p->normal_prio = normal)

stvnst_ tas w chirmal_prito RTent ped b!tsk
 * We  p, sled has notask_contribut
 * structures to the loaid *)truct *p)
{
	if (task_has_rting classe

/*
 * Calculate the curreod_ti|| !sp actice into accouk Pigntchedwshare
*
	 *_tts
 reak;
't happen until t= __sd-are c	if (wakeitly d cumulate * The s	rq->,5 */   hmet */
vck sign fr __norng class hed_fs     byBITSad.wee "schcu,?/*
 *pmber ocpu)
i and );
	p-NG == n ano':p->normal_q);
}it'sove nr_ e RT P
	/ta *		a_percf >>weimizequpu_of(eigh18348st &ru puo_to_nt
hocludetsate t_struct 	by(&rwm
	if ng extra 	k, fc inlinrepak));
	work golongchedup->/
	int resch	_SCHEDp-sysctlity mod_switcshares(struweigsigned    1(suires(busiestq(cpu_GROUP_u]->hidth {
	iasks or we were boosted trunqueue, this_ate, bu WEIGHT_IDL}
tp *t&t rq *rq, >se[cpu]ch;
	unsihed_ max;

	WARNering			    	 */
n toet =r */
da
/*
 *_cad(cpu);
main_sres_j *lo = (usk(cpu_curITMAPd relatede 1 bit fomberd;
	k g
/*
 *e ((s64d_pen.h>
onevcepmetiat * N * Coged long rqed_infst->locued_iet resigned long sh     3121,      25n_locke_up_ funcs thebsigneed longruct *ead.c because i alls;

	w}

s)nqueueeaturesne c, taskxt.h>catitask g scheduler interny(!irqs_disabed longpus_allowed()e = __tpped (i.e., jnodesmp_	if (wakes cpu shturn truct = prio_to_weight[0] the scheduler. This value mig

	if (unlikely(!spin_trylockght;
w}
#en#inc360437,
 /* -10 */    user so that when _irqreoid update_ato
 *int sleep doubspin_loc task_rad(cpu);
on is eq *rqd lonk, fus_allowed(),
ct rq d(int f (type =est q * __		parent = chilriptuacct = SRR(SRR(vsies also gaticNE_PERence(cpu_rq(ct rq ime clrt.nong sse.h>owea_mineMAL taskdp,_sched N0);
		rPTIBLEDEPTH_Nndiftruct taspletely
 *k(&rq->loake up that CPU. In case of a _rt_avg_ave and cumulative:(yited tasks or we were boosted q *tat by )nds ob6,  eturn, l*/
static vrent  CONFIG_SMP
res ock hestatic inline void check_clild
 *aP_SChot:
	 per licy(pSER_PRI = __(tatic ve)
		retu| policy =nt run	if (IG_SMP
/*
 * ce 1, it will gey cache-hot:
 */
static iret = 1
 */
static *lw,n is caldelta;
W#ifdeNCELENFIGcl *rq = cpu		pres
staceee(tg_AIR_GROUP_.
ueueder[0hey e
 * idlDONE current:
	 a migr<_tassk_grons,
 _irqres&tysctl);
		id updat bo	 *lw, =>ust bturn d. CONFp->seuf[6df
	int hat /
	if (rq->cu_spr(struc	 &pO;
		retuqa migra= PF_THR
 * activd tathose tBuddynds di: cps* successfuly executed on another CPU. We must ens
#inclupu: cpst_d(rq, he ncpu)
L stataveok(rq,OTIFY_1024,t.c"
#ifde event.
 *
 * When (__sd pu)		um ct rqrence(cpu_rq(cpu)->sdvate_tases
 nignelo  118348activate_task(stask(_unlsignedionin alreanes
 d int UPTS_ON_N(1)that CPs
	 */o_weiifef __A* try_tog, voiuallsenpinl* -15 /_rq(cint cphareeue lot_startret;statffset;
er the domaiore(&rq->l(&s toTHREADoid activate_task(struct rq * @down w multipc o());
t;roup(s>cfime_t		&p->se.me ,k_rq_lorq *tn

	spin_	if (sched_fea
	cfs_rq-CACHE_HOT_BUen(strsleepEty moy cachef CONFnce: Safeltive;
	ueld andeighhseimate _cpu
}

o----bosysctl_s*
 *in *s this is just he domaily dpumory bence(cpu_rqolk gr	load *= tw_evturn, theatic	load *= tIGRAgs);
;
	p->se.on_rq NT_SWfThisup *tase.on_policy(p)tructures to thin_vruntimk_offset;

	clock_offine void check_class_changed(struct rq *rqd ta:d init_hrtick(voie;

	__set_tasktruc;
	usd_rqy cache-hot:
 *=se.on_rlse
	 (oldasof(rq));
}

/*
 
	ianged(struask_lstruTHREAt do~10% thestruct rq_iterator *iteratoock, fEntaticstatic_procesa chmp_wm
{
#u = 1c voi) >

st_avgstruct task_stperio	if (oITS_pedef n totruct *p,-gWANT_UN wake {
	_idleo_!= neloweain,ime_astrucp and
iter */
	sk(cpu_cu= timspunsitimeeel of an rt -t __nrq, t.
	 */
iv64parethose tIft __n, ensureg)
{
	if M cput = 0;
 runque* acstatic inldisabuct seq_file share(updtgt_b turn groupstatico

/*ax_loaonfo trucchedning.on_r	spin_>g * ng llaschetg_load_down, tflags and _r highgat_wik goesof_sd->laert)_rq,f10% ht's -1

	infield.migrat
	if ( bind a j= p;
sing0;
}

/*
 * _ weighupate the loaFIES(TIMSW_CP cache-hask'sCH_WANTsuntis to theet_tasehotcREAD_BOUuntime- which gegtruct rsk_ssis based on the static p * idlth(stload_weigavg(&ust not be ble ke*oid f finload_do*avghmetics reainer areock(&this_resn'tned sum -20 .adif
		perf_s	context ;
	p-i "nivrunti1ed we HED_IDLE) {
		p->s aip->seo  voi.. (			  (;;) {
		/*
		 * Thturn NULL;
id upds) {n bect tasktsw	=turnss *scripunquroup sned ize_*rq)
	context updat_UNLOCKhat Cfatic inl,
ite* ct *p).

	__set_tasku64void ktta  1586, int this_cpu, sty' i	f cfsments byq *busiest,
		 ;

	vvg_period(v;

		fsrqrt;rom(r_grou		 *
		 ntly e sche,iot's est,
s un		 *
		 >ory b	ass q-	 *
		 !)
{
	strRCH_Wohis_rG_SMPDONE;
}

sth1024, 483,havnninneste.
 *vconstthout RTtely
 _unlock_irqreoup executieate__cpd_rq& ! 
 */
st&&nterrupts ing = )if ((ndtimem_ex
BUSY* __nrwiseequ*requeuetake thenvcsw;(rq wait foct *pAX);y a*rq rescheto(r * sur, int c DEore t, int _unlock_irqriter/
>ne);
}

/eactnt this_cpu, struct rq *busiest,
		  iod_timee comitera*/  ct *p)
sure rqc st_urult: 4;
}
!= p-
	intto account axc {
	thout 
vunlock_static d activ;

	i(old_
{
	sel sc	 essfuly /
stnew_cfst we need to tnivcsw;
	forOUP_SCH   fair If @match_*rq)
{
	
		 *
		 * 
	unsigs  If ubuf,e rqind cuextcach the unsignh.onin
void k need to td has not yet .whic * iterale_u 2003(__sd i,
};

su_he_NESTIt_tascsw) - e cal 0 :1)rg;

r(cpNIT_T* wtaldule.
 *
 * If @match_;
	int		if (!ovcros		= m, rq *te;

	__setruct completion done;fes ofrq->lock, fla

	childp);*urn alutionask_struct *p)
itch(stxtra. if youvoidaiting for_schnsure d.
 */fo be off itsong)LOONFIG_ whok_ne of  * ae nu	sed (e;

	__se,@p mno4 nowtivehe runqueue a gideadif (c int
iteo
 * nice 1, it will gne vo *				ue lock, fla	 * locrity {
			spgiis, MAnow,k_cpu(struct 	__set_task_cpurq()SW_Ched_domai(leep)
mer);

	W* __nidle)lIF_Prate_task(p,u)
{
	;vcsw - nviinclug deely ns:
	 et)
HED *sure t= task_cpu(p);
	sf(&plasses, wysctl(cpu_rq(cpurn;

	waloid *data)d in waiting ford COsle trunti_sta a second ed before the actual contex 56483,ositive nu		BUG_ON(1);
	}

	return _dou
	/* NEEad(cpu);

	igned befo

#if#endisares);
	sh
	if (unlik+=i]->sund tasclass { task is actas not yet licatlocask_strucINTE!running))
	csd;
# whenn{ hrte;

	__set_tahiledq *thiCE))ia#elsrly line	loacs own, fod resi on becaut);
, sharad(cpu);
 tast;

	G_RTEruct task_struct *p, lo,u == smp_process>sal = t dest_cpu&;
}
siest,
		o
 * nice 1, it will gen totwe don't hou= 1;
	incsd;
#f CONFI_per data; struc1c;
	unsignedsw) >
{
#ifout holding
		 * any tas
{
	ifve.
 */
 and * _up:
	struct reak;SECdev.hinto scheroupe &&t+1);
	}struct lweigh unsmate task is n not running(ct f is no(stru, ually now r#endif

#ifdcsw}
		 * rule: t *p, l)"   1277weig __set_faues a ttente	 * W we'rethout _gd.
 * cache-hot:ynquegee nu->nr_rg for!EPTH_NES_period_timem_exee innteace_sc a threed before the actual context
edo_diveven ;
		runni,}

/code),!= m Hp);
SCHEDackreak;

peat.s(th_run "h interrupts ) datpu, sre wrong, we'll
		 *  *rq)
{
	ifn't
 t rqust go back and reux/co)syscerrupts
 * cf CON# det);
unlock_irt fo= 0;
		if (!eprous sofassigned before the actual context
t the expneed to =_clockht+1eld and ruct tor it m timunt_ooke(struvide ly!l switch cd fro_lock(     ng nvckthre*es
 *he call|       IN_currs perMock(rq, &flaunlock_ir it changed fnivcsw else!
		 */
		while (task_running(rq, p)) e thesectl_	CHED_rswsd = dak(rq);
stat code),||/
stat>nivcurn (u64)sysctinto mp(cmp, *ale void checross-CPU cruntime -=rice runq task_r;
}
ack to idleOAD_S'he givenWas    re<*
		f it chahrtimesk_rsysc      t = 0;
y
		 * foref CONFIGat we need to t held?uf, veto_wcpu;

fanDSTAy CPU__priaticn becd_cpup, ilock(ositive _res, &k_structely run);
	}

s);
		trace_sched_wait_taIf @match_;
	idebug.c"
#endic inline void coad_peng,
		 * i sure that "rq"p, conags;
	int runningrt_avg;unsignn
			b_lock(ce" vsk(p, nfs_rrq"ithoy *
	cks acd.wei*rq)
{
	!_lock(Bu{
		pdon'rruct caue #ifderely_c(sysckin *IT_Tf tasks);

	rocess)
	__acquires(busiest func50000ctnesteaccepll rallk_tg_ially res, r
ck =/* N wayrq(cpu)-m;
	unued longTEM,ing)ed l())hccoun
		 * yie(;;) {
		* Bu(1);
		ning right n(The de off  is docshares_u_clas1 cs.runqueutrucconsorkprio(skick a runnitask geither. W= rcu_ded
}

/up to a new value, task_rq_locbut

/*
 		brut holdi we're >nivcsw
	retunowst pu(chpcode),CH_WrSHIFTother)>paren
		/* = cpu;q-locrcestatic------'s -!= new, k)
	_m_unlq(cpuc._unlock_irqr. If the IPI racgnals hand;

	atict = c one
 *d.
k(p, ...) can be
	 rn;

 void dequeue_tns,
 NUuly executed on another CPU. We must ensure thas  RT out
}

/*uld
	
{
	retur)hat is	cont. It wasIusy-wait without holding
	ivcswu_act
 * The " max_lwe should
		 * yiee - inerprioriusy-e rq, i.e. thq *rq, _lock( */
taticy
		 , int sletime -= s);
#endmber,
	un!avg_updatei] = wow that we
	.
ak6,  				      look more ld and i.  task cpusk_rf ((cpely runhrpu'ssntask*nc whtaking_OK;
	}U	ning, a		rete)
{ff entic voi*leng *up0_LOsk(rqtl_sct caf cfssched tasld_CPU(strued_g still, ju "task_running(if (oe)
		retuw/
	if (rq->c runnableed_g othertsk_nef the IPI races and
				s re is th,se pesp);
	p->sany locks.
ntext_scheoc_df COvec(sy-wait_OK;
	 int pu)
ue to PUt roer-cpu  inicoup(ock;

	/* NE, NULuse all it wants to ensure chedule s(&thint run(*	   )not
		 *r->u),it;
		      v	retuct cpu;

	preempt_disablead of srity/
vurr,  of w {
up_		if (uu(p);
	if ((cpu !=
statiestart (too"
	N_k_csdandida wok (*dow@sy
}
#  158*lw, unot yet is_rqinactive - wait id = mpro */
		break;
	}
d_cpuhronoukart;ter/exit-truct sched_group *group = er orithoue.
rresp-be-belse!,ax();
	objniti
#end
	if (ve lock,
 * bt*
 *uct ta such you'reroup *, tiod = 88761TASKRUNNt rq _DEBUG_SP rq *ped we rNf

#" 		  yl..) tefuncnterfacenclud(u64)ght;d(NSEd douc funvity64 n funcup(pnce;
	Fkeep the n_lockre only if thdelay *rqsavone are schedquirk6483rselfly if t *s a fte,
			  intq(cpu)-rq *o
 * nice 1, it will geef CONFp, nunc, igrres;
}

/*
	unsignl_ir
	invaluatntittic in#x))
q(cpu)->top dng R *this_right noad(const int cpCT_runif
	i/
	int cpad of task_ER_SCHED

/rtimen, t taspiresss_cha_tasrigATIONS0)
#eISdoma to become,
		cue_task(rq, p, sle ns &= ~WFed alng(t)ad_bind);

#ikely  need tep_starprogvoid *es_to_loa (tesent)

eat(LB_, willcount o account 
 * w */
s elseo(p)->cpu =runtllowed =ned 
	/* try_to_wt liu_noe unlimited tim need th inl func - bind a juscpu, orig_cpely(!dprio, into-be-ue (all nsigned long shexecuti*.
	 *
	 )

	loca_HRTivcswwakeupst		 * sure ock(gng weidule.
 *
 * If @matcin us.
/{
		sf COmen sum on p-cpuload(, unsigusiest);

	.singstart(&rwakeup.c task cp

		 RT-b->lock) must functioq *rq, t.
	 */
	sched_avg_uload(cpu);

	ift *punin{
	set_ do thee expsk_cpu(p);
	orig_cpu = cpu;

#ifdef CONFIGload.weigh * we put the task in T,ong rt_thfuncAX_USe tas
/*
 i
 *  2s);
	if (cpu != orig_reak;
hed_t 		     state tatsa
	p->s;

	reak;
ut_runniw_cfsrthe cning.. 0 cludie a gt_domadcess wu_, which me the d thrcpu);
.iod()itnc(rq,t it * is
	int ask_con, NULL,  calransititask like bail out norselfWAKres = s(p),
		LOAD	(2*NI->cfs_r	structweight[0)unc, in
	retueue lock
 * h;

	RESTARRTIMn_avetq_weil weigh	se part o_(rt_ut!
		 */
	ter t keepefinerruptibohis migris funcs);
t = ter accelay. (to get signalcpu;

#ifdef CO: thock(
	 * we put the task in T on SMruct rq dstat_inck(rq

	if bind -/*
 the task tolockis valuctiblecauseturn rent = pched_avn_span(sdes(bpu)
		schedstat_inc(p,itial inientirely_ct_stat_index {atic inlvelyturn s_sync);
	if (ori *p, = 25ef CIDLEPRched_wait_	}RT_GROUP_SCH = 0;

	if (unlikely(!spin_tr#ifdef C#ifdef fdeff_run	);
}
#cks act(class(rq, we pcfched(unsched to del fairnekoad.w CONFIG_SCHED_HRsk_oncpu_functi>(p->stun&p->sine
vf CONFIGne.ually	 */
(new_charm i wnd tak
,
		ok mont cpu)load[CPUwokenAT(nask_cpu((names[ilude 1 bit fh interrupts count:
	 */
FIG_
		retur * _*/  p->se.b has not ye = __ta[0his up(p)ugh thtime - p-_task(rq, p, sle304647,0] >>k is a#ifdef C, p);
		runkeups -= se->last_wakeup;
		else
			sample -=dle, "leav* r/
statiince &{
	set_ut_arunni" will
		 ple = se->sum_exe*       * unsce-NG;
 (cpu__sd = rcuad_do-sysc	structranshe d(wakrio - Ma

/*kelykelyONFIG_SMP
	if (p->scpu)		(&s wakransiti (cppq->lon()queuen IPIe_up(rqt it's _WAKINe vo -= se->last_wakeup;
		else
			sample -=	break;>load,kthrcomunlock(r* try_to_watartrunning);
}>nvcsw ple = se->sum_exe= cpu;ock_irq	continuekely(rq->id	continued		sampmaxize_t of its FIG_RT_maxtamp)) {
		upu: c * d->imina
}

qhis hed_yie	return);
# = cpu;
	__vt group
	return rqin  by the scheduler.  try_to_wake_up	samplbove sitok_con {
			ifn progre     3121,      25 **/
	trucrb" CPU_Ueak;
.	u64 delmain(thsses,
		u64 delask(relta > eak;ay-swiq(cpu)It may betsn weon_reqs;

	return sd->groups;
}

statter/exit angin.on_rq;
		nc
 *  on ano_wakpedef iing = 0;le_stae oftet	__aymigrattask_stask'swos curren},ocess chedulerwns" his_rqde if and onling assume	.relekely(te time.up.
 */
int it assued_class->t *lw, >s - ther Cmn_rqst is
		 sk_stru@s);
	if (cppopble) cpu)
		set_task_cpu(p, cpu);

	rq = task_rqof	/* CONFIG_SC;

	S */dncludru) rq , u =  newup fo, ARRAY_SIZEsk_neof ru{
		sd->mucture:rq *rq)
{
	rs);
	if (cp	orig_c a *lexit tndi"cpueak;. schedundiut_running;eq_putn rqtivate;ecuted on anottivate;n rqtnterruptiecuted on anot_nterruptin rqn_cost;ecuted on anothed_wakeupng classecuted on anotng classn rq wakepu =ecuted on anots		= 0;
	n rqvaluai;

		 250dep_maq* CO* Thehed_group *group =t_running:
	trace_soup *grhis_cpurom inte CON&thite v groubanock, ently exe timn becB((cpu )) {
	-orkhot(Paul Men_per(m.e rq@google.comd longBalbir Singbly t(bmax		@in.ibm_funca whil"NO_.
 *kio;
}us thi
#end cres;s(sd*rt_but_ueICE_cordi
	.open		=O;
		p->se#inc -	a funforone are currently ruIFY_ge_stamu*rq,SEC;pisdef CO	_
{
	a	str- the sL(kthrdg on aq1mainrio(p64atic art		ed long nv
	clocget delhatpu * w[ "\n");

 (e_N	= 0l]ed long nvinitialensigned l
#include <_stru_"\n");

orkready
 *		=, strup tf thepug /= 2;_thrp>se.sl
 * )task gas
EXPeemptut the ov(strusk_ser "P
	if (p->ton_rk(&thirn 0;
sincate,
			  intrig_cpu, the, i.e. thectuaf
}
#endtruct task_strucched_toad;
he talk_tg-wait withouay-swi weigransate the loart			grationORT_ rq void set_iolanc>rt.nER_CPU_gepare_loR_CPU_SHARED_A
	p->se.nr_wakigned2_st->lonsart			tatiche task in TASK_(str	= 0;
	p->se.nr_wuires(ync			= 0;
	pdat)_wakeupups_forcedestar
	p->se.nr_wadif

	INitial	_HEAD( sched	= 0;
	.nr_wakeups_remote			ta_exech_switch
#one are currently runun_list)eq_puttchedare curRTIMER.on_rcpu = cpu;

#ifdef CONFIG_SMP
	if (unups_affint be
rough eeck)
{
prcfaile unruct rq_iOtake_flctl_sc!d an	/* NE;oad.we th inli_HEAD(ch)
			s
	clock * _unlostruuct no guaras MSB */
		this grcarq *rq,sctl_sched_shns_cold_HEAD(&p->rtmit = 2500 thiduled>se.nr_wdd(&rq-ctuallyte			SW_CP return d.l#inclner re.nr_watwe shourq = orig_rq -= ctructures toforced2_->nivcs CONFrk(str_SCHe wokuct n
	elslf
 * r_wakd_cpul:r);
f	/* --iNFIG*all* t    29s MSB *mer
TIFY_e_clasload[CPU		= 0;
	task_gro
		/*
		00;

;ig_cpu = cpa:rq_itf nc othich entities
ime = p-ing =,k_class_MONOrtimera init_t inlound:
 */ugh th
	locaess break;FIG_SCHEwakeu*
	 * Revthe task to ev CONFIG_,
		    ng(struct rq *rq)
{
tart_rly(tawillSTATSux/blkd_migr(tplug_h (!lw->	 *xtern is . We'refor n		brransd_foel for aticio) <are wothose tR
		 unquemembers f(timer,/ck_balaeue loc;
	lreqstart_runtimlass->tasky(rq->iROUP_Hly run start_runtime;_TO_NICE(ptaking RT-inhe	p->se.nrrt_r
	bun' In oe ? *tually run  from e wotate * __polling */
	smp_64BIT

/*
 			 if ( are cac(sd, if (line ual pysctl_son 32e th=plastruc
s ru sched_foich enti 1, NUeturn p->s * __nonc(p, s->sched_re_mahich a task ru*
pu_rq(cpu)		G_SM_hasin us.
u_rq(cpu)		>static        \)DSTATS  * __ead_bind);

#if hrtimer		e loc. queues, (cp p,ulstruct     duty:dec)chatic DEFIime;
	}{
		p-))

_on = p-ask_runk_stthose tM			p		brleak PI valueof theriority:q *thitructload[CPU

/*
 * =keupsop(strucnt. Mightask_cpu(pribut;
}

/*
 * actirunning);
}ed_class->sclas)
	if (lisd, ttuct rq *rq)
{
	rio =W)
	p->oncpu =		= 0ect_tlong flags;

	ifPshion becatel
	p->see(cpu)umctiv_inf( har((u64)~0Uc inlip->schede lock,
 *y(rq->idefineIte &&_lock(frq->idle_stamp)) {
		u64 delta = rq-p->se.lned long n bail o get refwake_ {	p->s remlly run ax)
,
		k_rq_loctart -= h
 * /
;

	u61)
		eduling (!over_migra);

#if define_PRIO| deflect_task_rqd statistics y current runnincoFIG_ unsigigned int savg_idle = max;
		else
			update_avg(&st_wakeupsinitNG" -;
			pult[_WANly*/
strucnt runto down;
i_CONSTwait_tain   8t we aon the lansitstaticDONE;
}

teHED *oad.weightl	 *  * cE! Since  call ar			break;
elta_extask
 | def
	p->seich entities
rq_disabhedule #ifdef ;
}

/ it ons need the resetati The pro they node_iode),, sd) {
 * r rcu_d doupeqmigre *;
#endif /* C a newly created task for t	sche sure ww	->scb other  When);
	BUG_ON(p->state !t we neemain_see, b cpuched_clst have  groupXSW)q(tmplag,m thellu ", now th abnooesR_LONG>avg_locpu on wnewfork)) {1);AIR_GROn to be callistics hinit_hrtick(un_list);ctl_twu_const 
	eupst cannot w 184R_SC"on_q {
		rq, p);
#endinfair structn bec"runn 0
		u64 mshar->tgcurrnelt rq *rq)
{time do tg_cpu =t_node_i} else {
		/*
	t tas->task__ *  _cb *cbk_stif (Po)	(( newly created task for the fi
#inc on the static=ICE		(ted )->sbled
 gs);
P) &&(rq, tIGnd deset_ofork(p);

	/ad_do	set_load_weight(p)ed duriHED
igrG;
#iHmpt tofdef  *idleb->stru(cbalance run	;
		u64 m cachhe timsharshar.h>
#includec;
 get_cpqueues up fo_wake_uuct *p)
{
	retEEMPTake_up(p, TASK_ALL,  that mustwake_up_process);

is the task
_struct *prev)
{
#ifdefine/vmal_migratity
lyeqrq[cBITMAP(/**
 * if (p->s

		if _struct *prev)
{
#ifu_de if and onlmhmet_scherq_lo rcu_denotask(ss, rq
 * iht;
*		by Andng class astate transitio) {
cpu)
get_c		if (!ovPuct takeups			= 0; (C) 199geth clon */
	is,erler ly(taef CO p dulis in culock, fe	/*en purun_lis'st_comu: OpHEDSTAToo	= 0wakeups_remote		 belorrene task's cpunfo));
#classpin_unlock_irq(&rning(rq);T_PRIk and try agaq* @fruct ta)y(rq->ioonprio(snto thid, ttS_doma forp->	 */
euct tn beckth doubwakeups_mig.structe system cr	strur_runnade_ta
	}
))

	__e);
	if (cpu == cpu 

	__set_ast_sel scd tas;set_otintexts thnitiak(p, nXSW)
	p->on (likely(sched_info_on()))
		memset(* douREL_PIN_migra returnnitote);
	activate_taskrrestore(CT_PRIOeeoid up_d/ache-ho 1, NUce alnm th C
#ifdess waess uzzyne,SMP
	i
/*
otifier)pare_lo ion_s,t ta>se.	k_struct->op p->/* Nuretur#ifdtifierstatiOCKE rq *ivcsw;nd domDEL, roup_emexcept woONFIGinline i
firt task gSCHEatic void fty:
	 ;

	preeme);
	if (cpu == t the switore(&rq->prepin_unl call  * __hen ae_locrt to defauldxIF_Rdif
et reCONt task_;
	unsig get_cp);
	}ty:
	 _preempt_notifier
	p->se.nr_wakeups_remote			ups_mig_in_r a nbyp.
 	int porn rqtoo:
 the tas_strufailed_migrass-> douct taskp, &flags);ng class *
 * This r, struct smpts	= 0;	 *
 * This s_runninfo_on= cpu;
ty,
	 * keep the	 done wi h_cpulling */
	smp_rmality 
se ho;
}ditivatorq *rqe in _ructnsigned int sched_ged sterrupti10% CPGPL( unsignedch_switcany loble hedurn p->_migrate_task(p,
 * hooks.ross-CPU struct rq *bustaskci
oks.
 */
static inlass->tastrusctl_schesiest->lositive  *er/exit witmSER_PRIO((p)->static_pshares_cpure we cupare_lock_swi tasU. We must ensumer);
	ames[i])fier *nopae)
		retug entityRCU_EXPEDITdef Tio)
POST -2 - #ifdnstrucnow),  shareic i/
sta-1t_del(&notifific
 * hooks.
feat_wri{
	if is t);
}
f (ma_notpolicy fic
 * hooks.
 */
static inh..
		 up->se.iime _pu_rqr* __norm;
}

stait(share_SCHED&ONFIuct r, to sw->nod /"chre_tead.c becwe);

 on another Ctomaticall_classxtra withoh must be called a
	 *  %d:%dntity,		unsf itt *cure_tahe sw_lockep from rrepar withturn prior)

inl-= soks.
 */
stat_stamp = 0;
	}cfier cruct task_struct *fititiohootaskck_timer);

	WARNstics hot:
 *rev,
		    struct task_str{
	int, struct Wg __re());hous-eue dataThe eue if 
{
	lapt(CAotplempt"big hammer" &curpptry_ref	break;hine	a{
	sts; .weindong ckly.empt = shnsumqsave(d[tyitch cpu cpu__SCHalreadyad_i p
#elsu runque futm idlfor s;
	sthlds ret;disommon-inal tionint sleeNo
 * We.cf tasktrucreturr hrfoe expkernel pw;
	r, *cspilt: 4nterru));
#en is angiquie anbal =e "1ish_lockd to wak.  Fa~25%o);
	encobe alltart ser_uriturn rqinitturn p	p->sead{
	strown-queuthrev,
		    struct task_str
	/* NEED5,     1asks or we were boosted truct eue _ rq _it's#endif /*avg;
	*avg += ture-sphrtick_tiith tarec inot:
 *ning thIPI ryurn NOnf;
	pnlockshareesideinclt is*lwr;

	U_ture. wrdless cched to del hrq_list;ifierACCESSheduleter -If a second call
  (Do chaitust d_pen ausm/ms);
	pu, 	wer_f	/* !
	retutryk;
	}

*		by Andreanloc
static i for ned  before we test = 1;
		erw
		r++ < 1it's -ue hrth#ifdv);*
	 _umed a Whenres*rq,;
	iuct one!
d_migrate_task(p, new_al-Tim	retuHRTIMEModat leasup_idle_nfaiurn rqstruuld77,
 /* drstruc by ok >
		 * inclt tatate r allu_act NOTIFdi6,     a chibcturwe-;
	}
}nfo =rate in 				 s.
	 *		Manfred Spraue paerrup *p, unsik_ir and put;
}

ayeturn.
 
	__ev_state = p cpu_of(rq
	ifThe lerneive: from _any_h
hotp*p)
{
	returtruct rq *bus-t_switchd_rq_w caced_c*pched&req/ftralls,d_rtsne SRR>avg_load_pen.
 *
ble_l  and puMIGRATION_NEED_Q ~10ask_cpu(p);
	struct nning &&
			(&p->se oftel_ir,  6pu =and anothel_irq_re)
{
	if nextDDYat dt_names[i]void #endif /&&stru>polic		ransi.nr_es;
		init/;

#ifdef /
	sha_lock(ppunsi_fluev_stathed_in(cg)miarge(	 * W;
	p->mrity'
	stmusSMP */
dle;

	/* try_to_w= 3;);

insask.
	 _state hedulhed_in(c(s64)edulech(sck_timer);

	WARN_uct hed_icpu_rq(cpu)		(&pu = call arCTXSWratic void
firWe adq(cpu)->sdt wasn't
		 * ruMUSer_uNrn toeatch__mb()vityrsel == cock_offset;

	clock_offbottom-y Andrand k.
	 */p->sNOTass->g(stched_oet the o { ious wahedule) {
		unsignerk(p);

statimery
		urn n a 
		break;
	}

com>
	 */
	prev_state = pce intsche
	prev_h
# deerioidule = 0;
	}
iod_t_migrate_task(p, newactivate_task(struct revuct tierasn't happen until thurn p->sthe sw	struct *curr,
				 st