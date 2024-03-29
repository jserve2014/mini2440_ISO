/*
 * Performance events core code:
 *
 *  Copyright (C) 2008 Thomas Gleixner <tglx@linutronix.de>
 *  Copyright (C) 2008-2009 Red Hat, Inc., Ingo Molnar
 *  Copyright (C) 2008-2009 Red Hat, Inc., Peter Zijlstra <pzijlstr@redhat.com>
 *  Copyright  �  2009 Paul Mackerras, IBM Corp. <paulus@au1.ibm.com>
 *
 * For licensing details see kernel-base/COPYING
 */

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/sysfs.h>
#include <linux/dcache.h>
#include <linux/percpu.h>
#include <linux/ptrace.h>
#include <linux/vmstat.h>
#include <linux/vmalloc.h>
#include <linux/hardirq.h>
#include <linux/rculist.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/anon_inodes.h>
#include <linux/kernel_stat.h>
#include <linux/perf_event.h>

#include <asm/irq_regs.h>

/*
 * Each CPU has a list of per CPU events:
 */
DEFINE_PER_CPU(struct perf_cpu_context, perf_cpu_context);

int perf_max_events __read_mostly = 1;
static int perf_reserved_percpu __read_mostly;
static int perf_overcommit __read_mostly = 1;

static atomic_t nr_events __read_mostly;
static atomic_t nr_mmap_events __read_mostly;
static atomic_t nr_comm_events __read_mostly;
static atomic_t nr_task_events __read_mostly;

/*
 * perf event paranoia level:
 *  -1 - not paranoid at all
 *   0 - disallow raw tracepoint access for unpriv
 *   1 - disallow cpu events for unpriv
 *   2 - disallow kernel profiling for unpriv
 */
int sysctl_perf_event_paranoid __read_mostly = 1;

static inline bool perf_paranoid_tracepoint_raw(void)
{
	return sysctl_perf_event_paranoid > -1;
}

static inline bool perf_paranoid_cpu(void)
{
	return sysctl_perf_event_paranoid > 0;
}

static inline bool perf_paranoid_kernel(void)
{
	return sysctl_perf_event_paranoid > 1;
}

int sysctl_perf_event_mlock __read_mostly = 512; /* 'free' kb per user */

/*
 * max perf event sample rate
 */
int sysctl_perf_event_sample_rate __read_mostly = 100000;

static atomic64_t perf_event_id;

/*
 * Lock for (sysadmin-configurable) event reservations:
 */
static DEFINE_SPINLOCK(perf_resource_lock);

/*
 * Architecture provided APIs - weak aliases:
 */
extern __weak const struct pmu *hw_perf_event_init(struct perf_event *event)
{
	return NULL;
}

void __weak hw_perf_disable(void)		{ barrier(); }
void __weak hw_perf_enable(void)		{ barrier(); }

void __weak hw_perf_event_setup(int cpu)	{ barrier(); }
void __weak hw_perf_event_setup_online(int cpu)	{ barrier(); }

int __weak
hw_perf_group_sched_in(struct perf_event *group_leader,
	       struct perf_cpu_context *cpuctx,
	       struct perf_event_context *ctx, int cpu)
{
	return 0;
}

void __weak perf_event_print_debug(void)	{ }

static DEFINE_PER_CPU(int, perf_disable_count);

void __perf_disable(void)
{
	__get_cpu_var(perf_disable_count)++;
}

bool __perf_enable(void)
{
	return !--__get_cpu_var(perf_disable_count);
}

void perf_disable(void)
{
	__perf_disable();
	hw_perf_disable();
}

void perf_enable(void)
{
	if (__perf_enable())
		hw_perf_enable();
}

static void get_ctx(struct perf_event_context *ctx)
{
	WARN_ON(!atomic_inc_not_zero(&ctx->refcount));
}

static void free_ctx(struct rcu_head *head)
{
	struct perf_event_context *ctx;

	ctx = container_of(head, struct perf_event_context, rcu_head);
	kfree(ctx);
}

static void put_ctx(struct perf_event_context *ctx)
{
	if (atomic_dec_and_test(&ctx->refcount)) {
		if (ctx->parent_ctx)
			put_ctx(ctx->parent_ctx);
		if (ctx->task)
			put_task_struct(ctx->task);
		call_rcu(&ctx->rcu_head, free_ctx);
	}
}

static void unclone_ctx(struct perf_event_context *ctx)
{
	if (ctx->parent_ctx) {
		put_ctx(ctx->parent_ctx);
		ctx->parent_ctx = NULL;
	}
}

/*
 * If we inherit events we want to return the parent event id
 * to userspace.
 */
static u64 primary_event_id(struct perf_event *event)
{
	u64 id = event->id;

	if (event->parent)
		id = event->parent->id;

	return id;
}

/*
 * Get the perf_event_context for a task and lock it.
 * This has to cope with with the fact that until it is locked,
 * the context could get moved to another task.
 */
static struct perf_event_context *
perf_lock_task_context(struct task_struct *task, unsigned long *flags)
{
	struct perf_event_context *ctx;

	rcu_read_lock();
 retry:
	ctx = rcu_dereference(task->perf_event_ctxp);
	if (ctx) {
		/*
		 * If this context is a clone of another, it might
		 * get swapped for another underneath us by
		 * perf_event_task_sched_out, though the
		 * rcu_read_lock() protects us from any context
		 * getting freed.  Lock the context and check if it
		 * got swapped before we could get the lock, and retry
		 * if so.  If we locked the right context, then it
		 * can't get swapped on us any more.
		 */
		spin_lock_irqsave(&ctx->lock, *flags);
		if (ctx != rcu_dereference(task->perf_event_ctxp)) {
			spin_unlock_irqrestore(&ctx->lock, *flags);
			goto retry;
		}

		if (!atomic_inc_not_zero(&ctx->refcount)) {
			spin_unlock_irqrestore(&ctx->lock, *flags);
			ctx = NULL;
		}
	}
	rcu_read_unlock();
	return ctx;
}

/*
 * Get the context for a task and increment its pin_count so it
 * can't get swapped to another task.  This also increments its
 * reference count so that the context can't get freed.
 */
static struct perf_event_context *perf_pin_task_context(struct task_struct *task)
{
	struct perf_event_context *ctx;
	unsigned long flags;

	ctx = perf_lock_task_context(task, &flags);
	if (ctx) {
		++ctx->pin_count;
		spin_unlock_irqrestore(&ctx->lock, flags);
	}
	return ctx;
}

static void perf_unpin_context(struct perf_event_context *ctx)
{
	unsigned long flags;

	spin_lock_irqsave(&ctx->lock, flags);
	--ctx->pin_count;
	spin_unlock_irqrestore(&ctx->lock, flags);
	put_ctx(ctx);
}

/*
 * Add a event from the lists for its context.
 * Must be called with ctx->mutex and ctx->lock held.
 */
static void
list_add_event(struct perf_event *event, struct perf_event_context *ctx)
{
	struct perf_event *group_leader = event->group_leader;

	/*
	 * Depending on whether it is a standalone or sibling event,
	 * add it straight to the context's event list, or to the group
	 * leader's sibling list:
	 */
	if (group_leader == event)
		list_add_tail(&event->group_entry, &ctx->group_list);
	else {
		list_add_tail(&event->group_entry, &group_leader->sibling_list);
		group_leader->nr_siblings++;
	}

	list_add_rcu(&event->event_entry, &ctx->event_list);
	ctx->nr_events++;
	if (event->attr.inherit_stat)
		ctx->nr_stat++;
}

/*
 * Remove a event from the lists for its context.
 * Must be called with ctx->mutex and ctx->lock held.
 */
static void
list_del_event(struct perf_event *event, struct perf_event_context *ctx)
{
	struct perf_event *sibling, *tmp;

	if (list_empty(&event->group_entry))
		return;
	ctx->nr_events--;
	if (event->attr.inherit_stat)
		ctx->nr_stat--;

	list_del_init(&event->group_entry);
	list_del_rcu(&event->event_entry);

	if (event->group_leader != event)
		event->group_leader->nr_siblings--;

	/*
	 * If this was a group event with sibling events then
	 * upgrade the siblings to singleton events by adding them
	 * to the context list directly:
	 */
	list_for_each_entry_safe(sibling, tmp, &event->sibling_list, group_entry) {

		list_move_tail(&sibling->group_entry, &ctx->group_list);
		sibling->group_leader = sibling;
	}
}

static void
event_sched_out(struct perf_event *event,
		  struct perf_cpu_context *cpuctx,
		  struct perf_event_context *ctx)
{
	if (event->state != PERF_EVENT_STATE_ACTIVE)
		return;

	event->state = PERF_EVENT_STATE_INACTIVE;
	if (event->pending_disable) {
		event->pending_disable = 0;
		event->state = PERF_EVENT_STATE_OFF;
	}
	event->tstamp_stopped = ctx->time;
	event->pmu->disable(event);
	event->oncpu = -1;

	if (!is_software_event(event))
		cpuctx->active_oncpu--;
	ctx->nr_active--;
	if (event->attr.exclusive || !cpuctx->active_oncpu)
		cpuctx->exclusive = 0;
}

static void
group_sched_out(struct perf_event *group_event,
		struct perf_cpu_context *cpuctx,
		struct perf_event_context *ctx)
{
	struct perf_event *event;

	if (group_event->state != PERF_EVENT_STATE_ACTIVE)
		return;

	event_sched_out(group_event, cpuctx, ctx);

	/*
	 * Schedule out siblings (if any):
	 */
	list_for_each_entry(event, &group_event->sibling_list, group_entry)
		event_sched_out(event, cpuctx, ctx);

	if (group_event->attr.exclusive)
		cpuctx->exclusive = 0;
}

/*
 * Cross CPU call to remove a performance event
 *
 * We disable the event on the hardware level first. After that we
 * remove it from the context list.
 */
static void __perf_event_remove_from_context(void *info)
{
	struct perf_cpu_context *cpuctx = &__get_cpu_var(perf_cpu_context);
	struct perf_event *event = info;
	struct perf_event_context *ctx = event->ctx;

	/*
	 * If this is a task context, we need to check whether it is
	 * the current task context of this cpu. If not it has been
	 * scheduled out before the smp call arrived.
	 */
	if (ctx->task && cpuctx->task_ctx != ctx)
		return;

	spin_lock(&ctx->lock);
	/*
	 * Protect the list operation against NMI by disabling the
	 * events on a global level.
	 */
	perf_disable();

	event_sched_out(event, cpuctx, ctx);

	list_del_event(event, ctx);

	if (!ctx->task) {
		/*
		 * Allow more per task events with respect to the
		 * reservation:
		 */
		cpuctx->max_pertask =
			min(perf_max_events - ctx->nr_events,
			    perf_max_events - perf_reserved_percpu);
	}

	perf_enable();
	spin_unlock(&ctx->lock);
}


/*
 * Remove the event from a task's (or a CPU's) list of events.
 *
 * Must be called with ctx->mutex held.
 *
 * CPU events are removed with a smp call. For task events we only
 * call when the task is on a CPU.
 *
 * If event->ctx is a cloned context, callers must make sure that
 * every task struct that event->ctx->task could possibly point to
 * remains valid.  This is OK when called from perf_release since
 * that only calls us on the top-level context, which can't be a clone.
 * When called from perf_event_exit_task, it's OK because the
 * context has been detached from its task.
 */
static void perf_event_remove_from_context(struct perf_event *event)
{
	struct perf_event_context *ctx = event->ctx;
	struct task_struct *task = ctx->task;

	if (!task) {
		/*
		 * Per cpu events are removed via an smp call and
		 * the removal is always sucessful.
		 */
		smp_call_function_single(event->cpu,
					 __perf_event_remove_from_context,
					 event, 1);
		return;
	}

retry:
	task_oncpu_function_call(task, __perf_event_remove_from_context,
				 event);

	spin_lock_irq(&ctx->lock);
	/*
	 * If the context is active we need to retry the smp call.
	 */
	if (ctx->nr_active && !list_empty(&event->group_entry)) {
		spin_unlock_irq(&ctx->lock);
		goto retry;
	}

	/*
	 * The lock prevents that this context is scheduled in so we
	 * can remove the event safely, if the call above did not
	 * succeed.
	 */
	if (!list_empty(&event->group_entry)) {
		list_del_event(event, ctx);
	}
	spin_unlock_irq(&ctx->lock);
}

static inline u64 perf_clock(void)
{
	return cpu_clock(smp_processor_id());
}

/*
 * Update the record of the current time in a context.
 */
static void update_context_time(struct perf_event_context *ctx)
{
	u64 now = perf_clock();

	ctx->time += now - ctx->timestamp;
	ctx->timestamp = now;
}

/*
 * Update the total_time_enabled and total_time_running fields for a event.
 */
static void update_event_times(struct perf_event *event)
{
	struct perf_event_context *ctx = event->ctx;
	u64 run_end;

	if (event->state < PERF_EVENT_STATE_INACTIVE ||
	    event->group_leader->state < PERF_EVENT_STATE_INACTIVE)
		return;

	event->total_time_enabled = ctx->time - event->tstamp_enabled;

	if (event->state == PERF_EVENT_STATE_INACTIVE)
		run_end = event->tstamp_stopped;
	else
		run_end = ctx->time;

	event->total_time_running = run_end - event->tstamp_running;
}

/*
 * Update total_time_enabled and total_time_running for all events in a group.
 */
static void update_group_times(struct perf_event *leader)
{
	struct perf_event *event;

	update_event_times(leader);
	list_for_each_entry(event, &leader->sibling_list, group_entry)
		update_event_times(event);
}

/*
 * Cross CPU call to disable a performance event
 */
static void __perf_event_disable(void *info)
{
	struct perf_event *event = info;
	struct perf_cpu_context *cpuctx = &__get_cpu_var(perf_cpu_context);
	struct perf_event_context *ctx = event->ctx;

	/*
	 * If this is a per-task event, need to check whether this
	 * event's task is the current task on this cpu.
	 */
	if (ctx->task && cpuctx->task_ctx != ctx)
		return;

	spin_lock(&ctx->lock);

	/*
	 * If the event is on, turn it off.
	 * If it is in error state, leave it in error state.
	 */
	if (event->state >= PERF_EVENT_STATE_INACTIVE) {
		update_context_time(ctx);
		update_group_times(event);
		if (event == event->group_leader)
			group_sched_out(event, cpuctx, ctx);
		else
			event_sched_out(event, cpuctx, ctx);
		event->state = PERF_EVENT_STATE_OFF;
	}

	spin_unlock(&ctx->lock);
}

/*
 * Disable a event.
 *
 * If event->ctx is a cloned context, callers must make sure that
 * every task struct that event->ctx->task could possibly point to
 * remains valid.  This condition is satisifed when called through
 * perf_event_for_each_child or perf_event_for_each because they
 * hold the top-level event's child_mutex, so any descendant that
 * goes to exit will block in sync_child_event.
 * When called from perf_pending_event it's OK because event->ctx
 * is the current context on this CPU and preemption is disabled,
 * hence we can't get into perf_event_task_sched_out for this context.
 */
static void perf_event_disable(struct perf_event *event)
{
	struct perf_event_context *ctx = event->ctx;
	struct task_struct *task = ctx->task;

	if (!task) {
		/*
		 * Disable the event on the cpu that it's on
		 */
		smp_call_function_single(event->cpu, __perf_event_disable,
					 event, 1);
		return;
	}

 retry:
	task_oncpu_function_call(task, __perf_event_disable, event);

	spin_lock_irq(&ctx->lock);
	/*
	 * If the event is still active, we need to retry the cross-call.
	 */
	if (event->state == PERF_EVENT_STATE_ACTIVE) {
		spin_unlock_irq(&ctx->lock);
		goto retry;
	}

	/*
	 * Since we have the lock this context can't be scheduled
	 * in, so we can change the state safely.
	 */
	if (event->state == PERF_EVENT_STATE_INACTIVE) {
		update_group_times(event);
		event->state = PERF_EVENT_STATE_OFF;
	}

	spin_unlock_irq(&ctx->lock);
}

static int
event_sched_in(struct perf_event *event,
		 struct perf_cpu_context *cpuctx,
		 struct perf_event_context *ctx,
		 int cpu)
{
	if (event->state <= PERF_EVENT_STATE_OFF)
		return 0;

	event->state = PERF_EVENT_STATE_ACTIVE;
	event->oncpu = cpu;	/* TODO: put 'cpu' into cpuctx->cpu */
	/*
	 * The new state must be visible before we turn it on in the hardware:
	 */
	smp_wmb();

	if (event->pmu->enable(event)) {
		event->state = PERF_EVENT_STATE_INACTIVE;
		event->oncpu = -1;
		return -EAGAIN;
	}

	event->tstamp_running += ctx->time - event->tstamp_stopped;

	if (!is_software_event(event))
		cpuctx->active_oncpu++;
	ctx->nr_active++;

	if (event->attr.exclusive)
		cpuctx->exclusive = 1;

	return 0;
}

static int
group_sched_in(struct perf_event *group_event,
	       struct perf_cpu_context *cpuctx,
	       struct perf_event_context *ctx,
	       int cpu)
{
	struct perf_event *event, *partial_group;
	int ret;

	if (group_event->state == PERF_EVENT_STATE_OFF)
		return 0;

	ret = hw_perf_group_sched_in(group_event, cpuctx, ctx, cpu);
	if (ret)
		return ret < 0 ? ret : 0;

	if (event_sched_in(group_event, cpuctx, ctx, cpu))
		return -EAGAIN;

	/*
	 * Schedule in siblings as one group (if any):
	 */
	list_for_each_entry(event, &group_event->sibling_list, group_entry) {
		if (event_sched_in(event, cpuctx, ctx, cpu)) {
			partial_group = event;
			goto group_error;
		}
	}

	return 0;

group_error:
	/*
	 * Groups can be scheduled in as one unit only, so undo any
	 * partial group before returning:
	 */
	list_for_each_entry(event, &group_event->sibling_list, group_entry) {
		if (event == partial_group)
			break;
		event_sched_out(event, cpuctx, ctx);
	}
	event_sched_out(group_event, cpuctx, ctx);

	return -EAGAIN;
}

/*
 * Return 1 for a group consisting entirely of software events,
 * 0 if the group contains any hardware events.
 */
static int is_software_only_group(struct perf_event *leader)
{
	struct perf_event *event;

	if (!is_software_event(leader))
		return 0;

	list_for_each_entry(event, &leader->sibling_list, group_entry)
		if (!is_software_event(event))
			return 0;

	return 1;
}

/*
 * Work out whether we can put this event group on the CPU now.
 */
static int group_can_go_on(struct perf_event *event,
			   struct perf_cpu_context *cpuctx,
			   int can_add_hw)
{
	/*
	 * Groups consisting entirely of software events can always go on.
	 */
	if (is_software_only_group(event))
		return 1;
	/*
	 * If an exclusive group is already on, no other hardware
	 * events can go on.
	 */
	if (cpuctx->exclusive)
		return 0;
	/*
	 * If this group is exclusive and there are already
	 * events on the CPU, it can't go on.
	 */
	if (event->attr.exclusive && cpuctx->active_oncpu)
		return 0;
	/*
	 * Otherwise, try to add it if all previous groups were able
	 * to go on.
	 */
	return can_add_hw;
}

static void add_event_to_ctx(struct perf_event *event,
			       struct perf_event_context *ctx)
{
	list_add_event(event, ctx);
	event->tstamp_enabled = ctx->time;
	event->tstamp_running = ctx->time;
	event->tstamp_stopped = ctx->time;
}

/*
 * Cross CPU call to install and enable a performance event
 *
 * Must be called with ctx->mutex held
 */
static void __perf_install_in_context(void *info)
{
	struct perf_cpu_context *cpuctx = &__get_cpu_var(perf_cpu_context);
	struct perf_event *event = info;
	struct perf_event_context *ctx = event->ctx;
	struct perf_event *leader = event->group_leader;
	int cpu = smp_processor_id();
	int err;

	/*
	 * If this is a task context, we need to check whether it is
	 * the current task context of this cpu. If not it has been
	 * scheduled out before the smp call arrived.
	 * Or possibly this is the right context but it isn't
	 * on this cpu because it had no events.
	 */
	if (ctx->task && cpuctx->task_ctx != ctx) {
		if (cpuctx->task_ctx || ctx->task != current)
			return;
		cpuctx->task_ctx = ctx;
	}

	spin_lock(&ctx->lock);
	ctx->is_active = 1;
	update_context_time(ctx);

	/*
	 * Protect the list operation against NMI by disabling the
	 * events on a global level. NOP for non NMI based events.
	 */
	perf_disable();

	add_event_to_ctx(event, ctx);

	/*
	 * Don't put the event on if it is disabled or if
	 * it is in a group and the group isn't on.
	 */
	if (event->state != PERF_EVENT_STATE_INACTIVE ||
	    (leader != event && leader->state != PERF_EVENT_STATE_ACTIVE))
		goto unlock;

	/*
	 * An exclusive event can't go on if there are already active
	 * hardware events, and no hardware event can go on if there
	 * is already an exclusive event on.
	 */
	if (!group_can_go_on(event, cpuctx, 1))
		err = -EEXIST;
	else
		err = event_sched_in(event, cpuctx, ctx, cpu);

	if (err) {
		/*
		 * This event couldn't go on.  If it is in a group
		 * then we have to pull the whole group off.
		 * If the event group is pinned then put it in error state.
		 */
		if (leader != event)
			group_sched_out(leader, cpuctx, ctx);
		if (leader->attr.pinned) {
			update_group_times(leader);
			leader->state = PERF_EVENT_STATE_ERROR;
		}
	}

	if (!err && !ctx->task && cpuctx->max_pertask)
		cpuctx->max_pertask--;

 unlock:
	perf_enable();

	spin_unlock(&ctx->lock);
}

/*
 * Attach a performance event to a context
 *
 * First we add the event to the list with the hardware enable bit
 * in event->hw_config cleared.
 *
 * If the event is attached to a task which is on a CPU we use a smp
 * call to enable it in the task context. The task might have been
 * scheduled away, but we check this in the smp call again.
 *
 * Must be called with ctx->mutex held.
 */
static void
perf_install_in_context(struct perf_event_context *ctx,
			struct perf_event *event,
			int cpu)
{
	struct task_struct *task = ctx->task;

	if (!task) {
		/*
		 * Per cpu events are installed via an smp call and
		 * the install is always sucessful.
		 */
		smp_call_function_single(cpu, __perf_install_in_context,
					 event, 1);
		return;
	}

retry:
	task_oncpu_function_call(task, __perf_install_in_context,
				 event);

	spin_lock_irq(&ctx->lock);
	/*
	 * we need to retry the smp call.
	 */
	if (ctx->is_active && list_empty(&event->group_entry)) {
		spin_unlock_irq(&ctx->lock);
		goto retry;
	}

	/*
	 * The lock prevents that this context is scheduled in so we
	 * can add the event safely, if it the call above did not
	 * succeed.
	 */
	if (list_empty(&event->group_entry))
		add_event_to_ctx(event, ctx);
	spin_unlock_irq(&ctx->lock);
}

/*
 * Put a event into inactive state and update time fields.
 * Enabling the leader of a group effectively enables all
 * the group members that aren't explicitly disabled, so we
 * have to update their ->tstamp_enabled also.
 * Note: this works for group members as well as group leaders
 * since the non-leader members' sibling_lists will be empty.
 */
static void __perf_event_mark_enabled(struct perf_event *event,
					struct perf_event_context *ctx)
{
	struct perf_event *sub;

	event->state = PERF_EVENT_STATE_INACTIVE;
	event->tstamp_enabled = ctx->time - event->total_time_enabled;
	list_for_each_entry(sub, &event->sibling_list, group_entry)
		if (sub->state >= PERF_EVENT_STATE_INACTIVE)
			sub->tstamp_enabled =
				ctx->time - sub->total_time_enabled;
}

/*
 * Cross CPU call to enable a performance event
 */
static void __perf_event_enable(void *info)
{
	struct perf_event *event = info;
	struct perf_cpu_context *cpuctx = &__get_cpu_var(perf_cpu_context);
	struct perf_event_context *ctx = event->ctx;
	struct perf_event *leader = event->group_leader;
	int err;

	/*
	 * If this is a per-task event, need to check whether this
	 * event's task is the current task on this cpu.
	 */
	if (ctx->task && cpuctx->task_ctx != ctx) {
		if (cpuctx->task_ctx || ctx->task != current)
			return;
		cpuctx->task_ctx = ctx;
	}

	spin_lock(&ctx->lock);
	ctx->is_active = 1;
	update_context_time(ctx);

	if (event->state >= PERF_EVENT_STATE_INACTIVE)
		goto unlock;
	__perf_event_mark_enabled(event, ctx);

	/*
	 * If the event is in a group and isn't the group leader,
	 * then don't put it on unless the group is on.
	 */
	if (leader != event && leader->state != PERF_EVENT_STATE_ACTIVE)
		goto unlock;

	if (!group_can_go_on(event, cpuctx, 1)) {
		err = -EEXIST;
	} else {
		perf_disable();
		if (event == leader)
			err = group_sched_in(event, cpuctx, ctx,
					     smp_processor_id());
		else
			err = event_sched_in(event, cpuctx, ctx,
					       smp_processor_id());
		perf_enable();
	}

	if (err) {
		/*
		 * If this event can't go on and it's part of a
		 * group, then the whole group has to come off.
		 */
		if (leader != event)
			group_sched_out(leader, cpuctx, ctx);
		if (leader->attr.pinned) {
			update_group_times(leader);
			leader->state = PERF_EVENT_STATE_ERROR;
		}
	}

 unlock:
	spin_unlock(&ctx->lock);
}

/*
 * Enable a event.
 *
 * If event->ctx is a cloned context, callers must make sure that
 * every task struct that event->ctx->task could possibly point to
 * remains valid.  This condition is satisfied when called through
 * perf_event_for_each_child or perf_event_for_each as described
 * for perf_event_disable.
 */
static void perf_event_enable(struct perf_event *event)
{
	struct perf_event_context *ctx = event->ctx;
	struct task_struct *task = ctx->task;

	if (!task) {
		/*
		 * Enable the event on the cpu that it's on
		 */
		smp_call_function_single(event->cpu, __perf_event_enable,
					 event, 1);
		return;
	}

	spin_lock_irq(&ctx->lock);
	if (event->state >= PERF_EVENT_STATE_INACTIVE)
		goto out;

	/*
	 * If the event is in error state, clear that first.
	 * That way, if we see the event in error state below, we
	 * know that it has gone back into error state, as distinct
	 * from the task having been scheduled away before the
	 * cross-call arrived.
	 */
	if (event->state == PERF_EVENT_STATE_ERROR)
		event->state = PERF_EVENT_STATE_OFF;

 retry:
	spin_unlock_irq(&ctx->lock);
	task_oncpu_function_call(task, __perf_event_enable, event);

	spin_lock_irq(&ctx->lock);

	/*
	 * If the context is active and the event is still off,
	 * we need to retry the cross-call.
	 */
	if (ctx->is_active && event->state == PERF_EVENT_STATE_OFF)
		goto retry;

	/*
	 * Since we have the lock this context can't be scheduled
	 * in, so we can change the state safely.
	 */
	if (event->state == PERF_EVENT_STATE_OFF)
		__perf_event_mark_enabled(event, ctx);

 out:
	spin_unlock_irq(&ctx->lock);
}

static int perf_event_refresh(struct perf_event *event, int refresh)
{
	/*
	 * not supported on inherited events
	 */
	if (event->attr.inherit)
		return -EINVAL;

	atomic_add(refresh, &event->event_limit);
	perf_event_enable(event);

	return 0;
}

void __perf_event_sched_out(struct perf_event_context *ctx,
			      struct perf_cpu_context *cpuctx)
{
	struct perf_event *event;

	spin_lock(&ctx->lock);
	ctx->is_active = 0;
	if (likely(!ctx->nr_events))
		goto out;
	update_context_time(ctx);

	perf_disable();
	if (ctx->nr_active)
		list_for_each_entry(event, &ctx->group_list, group_entry)
			group_sched_out(event, cpuctx, ctx);

	perf_enable();
 out:
	spin_unlock(&ctx->lock);
}

/*
 * Test whether two contexts are equivalent, i.e. whether they
 * have both been cloned from the same version of the same context
 * and they both have the same number of enabled events.
 * If the number of enabled events is the same, then the set
 * of enabled events should be the same, because these are both
 * inherited contexts, therefore we can't access individual events
 * in them directly with an fd; we can only enable/disable all
 * events via prctl, or enable/disable all events in a family
 * via ioctl, which will have the same effect on both contexts.
 */
static int context_equiv(struct perf_event_context *ctx1,
			 struct perf_event_context *ctx2)
{
	return ctx1->parent_ctx && ctx1->parent_ctx == ctx2->parent_ctx
		&& ctx1->parent_gen == ctx2->parent_gen
		&& !ctx1->pin_count && !ctx2->pin_count;
}

static void __perf_event_read(void *event);

static void __perf_event_sync_stat(struct perf_event *event,
				     struct perf_event *next_event)
{
	u64 value;

	if (!event->attr.inherit_stat)
		return;

	/*
	 * Update the event value, we cannot use perf_event_read()
	 * because we're in the middle of a context switch and have IRQs
	 * disabled, which upsets smp_call_function_single(), however
	 * we know the event must be on the current CPU, therefore we
	 * don't need to use it.
	 */
	switch (event->state) {
	case PERF_EVENT_STATE_ACTIVE:
		__perf_event_read(event);
		break;

	case PERF_EVENT_STATE_INACTIVE:
		update_event_times(event);
		break;

	default:
		break;
	}

	/*
	 * In order to keep per-task stats reliable we need to flip the event
	 * values when we flip the contexts.
	 */
	value = atomic64_read(&next_event->count);
	value = atomic64_xchg(&event->count, value);
	atomic64_set(&next_event->count, value);

	swap(event->total_time_enabled, next_event->total_time_enabled);
	swap(event->total_time_running, next_event->total_time_running);

	/*
	 * Since we swizzled the values, update the user visible data too.
	 */
	perf_event_update_userpage(event);
	perf_event_update_userpage(next_event);
}

#define list_next_entry(pos, member) \
	list_entry(pos->member.next, typeof(*pos), member)

static void perf_event_sync_stat(struct perf_event_context *ctx,
				   struct perf_event_context *next_ctx)
{
	struct perf_event *event, *next_event;

	if (!ctx->nr_stat)
		return;

	event = list_first_entry(&ctx->event_list,
				   struct perf_event, event_entry);

	next_event = list_first_entry(&next_ctx->event_list,
					struct perf_event, event_entry);

	while (&event->event_entry != &ctx->event_list &&
	       &next_event->event_entry != &next_ctx->event_list) {

		__perf_event_sync_stat(event, next_event);

		event = list_next_entry(event, event_entry);
		next_event = list_next_entry(next_event, event_entry);
	}
}

/*
 * Called from scheduler to remove the events of the current task,
 * with interrupts disabled.
 *
 * We stop each event and update the event value in event->count.
 *
 * This does not protect us against NMI, but disable()
 * sets the disabled bit in the control field of event _before_
 * accessing the event control register. If a NMI hits, then it will
 * not restart the event.
 */
void perf_event_task_sched_out(struct task_struct *task,
				 struct task_struct *next, int cpu)
{
	struct perf_cpu_context *cpuctx = &per_cpu(perf_cpu_context, cpu);
	struct perf_event_context *ctx = task->perf_event_ctxp;
	struct perf_event_context *next_ctx;
	struct perf_event_context *parent;
	struct pt_regs *regs;
	int do_switch = 1;

	regs = task_pt_regs(task);
	perf_sw_event(PERF_COUNT_SW_CONTEXT_SWITCHES, 1, 1, regs, 0);

	if (likely(!ctx || !cpuctx->task_ctx))
		return;

	update_context_time(ctx);

	rcu_read_lock();
	parent = rcu_dereference(ctx->parent_ctx);
	next_ctx = next->perf_event_ctxp;
	if (parent && next_ctx &&
	    rcu_dereference(next_ctx->parent_ctx) == parent) {
		/*
		 * Looks like the two contexts are clones, so we might be
		 * able to optimize the context switch.  We lock both
		 * contexts and check that they are clones under the
		 * lock (including re-checking that neither has been
		 * uncloned in the meantime).  It doesn't matter which
		 * order we take the locks because no other cpu could
		 * be trying to lock both of these tasks.
		 */
		spin_lock(&ctx->lock);
		spin_lock_nested(&next_ctx->lock, SINGLE_DEPTH_NESTING);
		if (context_equiv(ctx, next_ctx)) {
			/*
			 * XXX do we need a memory barrier of sorts
			 * wrt to rcu_dereference() of perf_event_ctxp
			 */
			task->perf_event_ctxp = next_ctx;
			next->perf_event_ctxp = ctx;
			ctx->task = next;
			next_ctx->task = task;
			do_switch = 0;

			perf_event_sync_stat(ctx, next_ctx);
		}
		spin_unlock(&next_ctx->lock);
		spin_unlock(&ctx->lock);
	}
	rcu_read_unlock();

	if (do_switch) {
		__perf_event_sched_out(ctx, cpuctx);
		cpuctx->task_ctx = NULL;
	}
}

/*
 * Called with IRQs disabled
 */
static void __perf_event_task_sched_out(struct perf_event_context *ctx)
{
	struct perf_cpu_context *cpuctx = &__get_cpu_var(perf_cpu_context);

	if (!cpuctx->task_ctx)
		return;

	if (WARN_ON_ONCE(ctx != cpuctx->task_ctx))
		return;

	__perf_event_sched_out(ctx, cpuctx);
	cpuctx->task_ctx = NULL;
}

/*
 * Called with IRQs disabled
 */
static void perf_event_cpu_sched_out(struct perf_cpu_context *cpuctx)
{
	__perf_event_sched_out(&cpuctx->ctx, cpuctx);
}

static void
__perf_event_sched_in(struct perf_event_context *ctx,
			struct perf_cpu_context *cpuctx, int cpu)
{
	struct perf_event *event;
	int can_add_hw = 1;

	spin_lock(&ctx->lock);
	ctx->is_active = 1;
	if (likely(!ctx->nr_events))
		goto out;

	ctx->timestamp = perf_clock();

	perf_disable();

	/*
	 * First go through the list and put on any pinned groups
	 * in order to give them the best chance of going on.
	 */
	list_for_each_entry(event, &ctx->group_list, group_entry) {
		if (event->state <= PERF_EVENT_STATE_OFF ||
		    !event->attr.pinned)
			continue;
		if (event->cpu != -1 && event->cpu != cpu)
			continue;

		if (group_can_go_on(event, cpuctx, 1))
			group_sched_in(event, cpuctx, ctx, cpu);

		/*
		 * If this pinned group hasn't been scheduled,
		 * put it in error state.
		 */
		if (event->state == PERF_EVENT_STATE_INACTIVE) {
			update_group_times(event);
			event->state = PERF_EVENT_STATE_ERROR;
		}
	}

	list_for_each_entry(event, &ctx->group_list, group_entry) {
		/*
		 * Ignore events in OFF or ERROR state, and
		 * ignore pinned events since we did them already.
		 */
		if (event->state <= PERF_EVENT_STATE_OFF ||
		    event->attr.pinned)
			continue;

		/*
		 * Listen to the 'cpu' scheduling filter constraint
		 * of events:
		 */
		if (event->cpu != -1 && event->cpu != cpu)
			continue;

		if (group_can_go_on(event, cpuctx, can_add_hw))
			if (group_sched_in(event, cpuctx, ctx, cpu))
				can_add_hw = 0;
	}
	perf_enable();
 out:
	spin_unlock(&ctx->lock);
}

/*
 * Called from scheduler to add the events of the current task
 * with interrupts disabled.
 *
 * We restore the event value and then enable it.
 *
 * This does not protect us against NMI, but enable()
 * sets the enabled bit in the control field of event _before_
 * accessing the event control register. If a NMI hits, then it will
 * keep the event running.
 */
void perf_event_task_sched_in(struct task_struct *task, int cpu)
{
	struct perf_cpu_context *cpuctx = &per_cpu(perf_cpu_context, cpu);
	struct perf_event_context *ctx = task->perf_event_ctxp;

	if (likely(!ctx))
		return;
	if (cpuctx->task_ctx == ctx)
		return;
	__perf_event_sched_in(ctx, cpuctx, cpu);
	cpuctx->task_ctx = ctx;
}

static void perf_event_cpu_sched_in(struct perf_cpu_context *cpuctx, int cpu)
{
	struct perf_event_context *ctx = &cpuctx->ctx;

	__perf_event_sched_in(ctx, cpuctx, cpu);
}

#define MAX_INTERRUPTS (~0ULL)

static void perf_log_throttle(struct perf_event *event, int enable);

static void perf_adjust_period(struct perf_event *event, u64 events)
{
	struct hw_perf_event *hwc = &event->hw;
	u64 period, sample_period;
	s64 delta;

	events *= hwc->sample_period;
	period = div64_u64(events, event->attr.sample_freq);

	delta = (s64)(period - hwc->sample_period);
	delta = (delta + 7) / 8; /* low pass filter */

	sample_period = hwc->sample_period + delta;

	if (!sample_period)
		sample_period = 1;

	hwc->sample_period = sample_period;
}

static void perf_ctx_adjust_freq(struct perf_event_context *ctx)
{
	struct perf_event *event;
	struct hw_perf_event *hwc;
	u64 interrupts, freq;

	spin_lock(&ctx->lock);
	list_for_each_entry_rcu(event, &ctx->event_list, event_entry) {
		if (event->state != PERF_EVENT_STATE_ACTIVE)
			continue;

		hwc = &event->hw;

		interrupts = hwc->interrupts;
		hwc->interrupts = 0;

		/*
		 * unthrottle events on the tick
		 */
		if (interrupts == MAX_INTERRUPTS) {
			perf_log_throttle(event, 1);
			event->pmu->unthrottle(event);
			interrupts = 2*sysctl_perf_event_sample_rate/HZ;
		}

		if (!event->attr.freq || !event->attr.sample_freq)
			continue;

		/*
		 * if the specified freq < HZ then we need to skip ticks
		 */
		if (event->attr.sample_freq < HZ) {
			freq = event->attr.sample_freq;

			hwc->freq_count += freq;
			hwc->freq_interrupts += interrupts;

			if (hwc->freq_count < HZ)
				continue;

			interrupts = hwc->freq_interrupts;
			hwc->freq_interrupts = 0;
			hwc->freq_count -= HZ;
		} else
			freq = HZ;

		perf_adjust_period(event, freq * interrupts);

		/*
		 * In order to avoid being stalled by an (accidental) huge
		 * sample period, force reset the sample period if we didn't
		 * get any events in this freq period.
		 */
		if (!interrupts) {
			perf_disable();
			event->pmu->disable(event);
			atomic64_set(&hwc->period_left, 0);
			event->pmu->enable(event);
			perf_enable();
		}
	}
	spin_unlock(&ctx->lock);
}

/*
 * Round-robin a context's events:
 */
static void rotate_ctx(struct perf_event_context *ctx)
{
	struct perf_event *event;

	if (!ctx->nr_events)
		return;

	spin_lock(&ctx->lock);
	/*
	 * Rotate the first entry last (works just fine for group events too):
	 */
	perf_disable();
	list_for_each_entry(event, &ctx->group_list, group_entry) {
		list_move_tail(&event->group_entry, &ctx->group_list);
		break;
	}
	perf_enable();

	spin_unlock(&ctx->lock);
}

void perf_event_task_tick(struct task_struct *curr, int cpu)
{
	struct perf_cpu_context *cpuctx;
	struct perf_event_context *ctx;

	if (!atomic_read(&nr_events))
		return;

	cpuctx = &per_cpu(perf_cpu_context, cpu);
	ctx = curr->perf_event_ctxp;

	perf_ctx_adjust_freq(&cpuctx->ctx);
	if (ctx)
		perf_ctx_adjust_freq(ctx);

	perf_event_cpu_sched_out(cpuctx);
	if (ctx)
		__perf_event_task_sched_out(ctx);

	rotate_ctx(&cpuctx->ctx);
	if (ctx)
		rotate_ctx(ctx);

	perf_event_cpu_sched_in(cpuctx, cpu);
	if (ctx)
		perf_event_task_sched_in(curr, cpu);
}

/*
 * Enable all of a task's events that have been marked enable-on-exec.
 * This expects task == current.
 */
static void perf_event_enable_on_exec(struct task_struct *task)
{
	struct perf_event_context *ctx;
	struct perf_event *event;
	unsigned long flags;
	int enabled = 0;

	local_irq_save(flags);
	ctx = task->perf_event_ctxp;
	if (!ctx || !ctx->nr_events)
		goto out;

	__perf_event_task_sched_out(ctx);

	spin_lock(&ctx->lock);

	list_for_each_entry(event, &ctx->group_list, group_entry) {
		if (!event->attr.enable_on_exec)
			continue;
		event->attr.enable_on_exec = 0;
		if (event->state >= PERF_EVENT_STATE_INACTIVE)
			continue;
		__perf_event_mark_enabled(event, ctx);
		enabled = 1;
	}

	/*
	 * Unclone this context if we enabled any event.
	 */
	if (enabled)
		unclone_ctx(ctx);

	spin_unlock(&ctx->lock);

	perf_event_task_sched_in(task, smp_processor_id());
 out:
	local_irq_restore(flags);
}

/*
 * Cross CPU call to read the hardware event
 */
static void __perf_event_read(void *info)
{
	struct perf_cpu_context *cpuctx = &__get_cpu_var(perf_cpu_context);
	struct perf_event *event = info;
	struct perf_event_context *ctx = event->ctx;
	unsigned long flags;

	/*
	 * If this is a task context, we need to check whether it is
	 * the current task context of this cpu.  If not it has been
	 * scheduled out before the smp call arrived.  In that case
	 * event->count would have been updated to a recent sample
	 * when the event was scheduled out.
	 */
	if (ctx->task && cpuctx->task_ctx != ctx)
		return;

	local_irq_save(flags);
	if (ctx->is_active)
		update_context_time(ctx);
	event->pmu->read(event);
	update_event_times(event);
	local_irq_restore(flags);
}

static u64 perf_event_read(struct perf_event *event)
{
	/*
	 * If event is enabled and currently active on a CPU, update the
	 * value in the event structure:
	 */
	if (event->state == PERF_EVENT_STATE_ACTIVE) {
		smp_call_function_single(event->oncpu,
					 __perf_event_read, event, 1);
	} else if (event->state == PERF_EVENT_STATE_INACTIVE) {
		update_event_times(event);
	}

	return atomic64_read(&event->count);
}

/*
 * Initialize the perf_event context in a task_struct:
 */
static void
__perf_event_init_context(struct perf_event_context *ctx,
			    struct task_struct *task)
{
	memset(ctx, 0, sizeof(*ctx));
	spin_lock_init(&ctx->lock);
	mutex_init(&ctx->mutex);
	INIT_LIST_HEAD(&ctx->group_list);
	INIT_LIST_HEAD(&ctx->event_list);
	atomic_set(&ctx->refcount, 1);
	ctx->task = task;
}

static struct perf_event_context *find_get_context(pid_t pid, int cpu)
{
	struct perf_event_context *ctx;
	struct perf_cpu_context *cpuctx;
	struct task_struct *task;
	unsigned long flags;
	int err;

	/*
	 * If cpu is not a wildcard then this is a percpu event:
	 */
	if (cpu != -1) {
		/* Must be root to operate on a CPU event: */
		if (perf_paranoid_cpu() && !capable(CAP_SYS_ADMIN))
			return ERR_PTR(-EACCES);

		if (cpu < 0 || cpu >= nr_cpumask_bits)
			return ERR_PTR(-EINVAL);

		/*
		 * We could be clever and allow to attach a event to an
		 * offline CPU and activate it when the CPU comes up, but
		 * that's for later.
		 */
		if (!cpu_isset(cpu, cpu_online_map))
			return ERR_PTR(-ENODEV);

		cpuctx = &per_cpu(perf_cpu_context, cpu);
		ctx = &cpuctx->ctx;
		get_ctx(ctx);

		return ctx;
	}

	rcu_read_lock();
	if (!pid)
		task = current;
	else
		task = find_task_by_vpid(pid);
	if (task)
		get_task_struct(task);
	rcu_read_unlock();

	if (!task)
		return ERR_PTR(-ESRCH);

	/*
	 * Can't attach events to a dying task.
	 */
	err = -ESRCH;
	if (task->flags & PF_EXITING)
		goto errout;

	/* Reuse ptrace permission checks for now. */
	err = -EACCES;
	if (!ptrace_may_access(task, PTRACE_MODE_READ))
		goto errout;

 retry:
	ctx = perf_lock_task_context(task, &flags);
	if (ctx) {
		unclone_ctx(ctx);
		spin_unlock_irqrestore(&ctx->lock, flags);
	}

	if (!ctx) {
		ctx = kmalloc(sizeof(struct perf_event_context), GFP_KERNEL);
		err = -ENOMEM;
		if (!ctx)
			goto errout;
		__perf_event_init_context(ctx, task);
		get_ctx(ctx);
		if (cmpxchg(&task->perf_event_ctxp, NULL, ctx)) {
			/*
			 * We raced with some other task; use
			 * the context they set.
			 */
			kfree(ctx);
			goto retry;
		}
		get_task_struct(task);
	}

	put_task_struct(task);
	return ctx;

 errout:
	put_task_struct(task);
	return ERR_PTR(err);
}

static void free_event_rcu(struct rcu_head *head)
{
	struct perf_event *event;

	event = container_of(head, struct perf_event, rcu_head);
	if (event->ns)
		put_pid_ns(event->ns);
	kfree(event);
}

static void perf_pending_sync(struct perf_event *event);

static void free_event(struct perf_event *event)
{
	perf_pending_sync(event);

	if (!event->parent) {
		atomic_dec(&nr_events);
		if (event->attr.mmap)
			atomic_dec(&nr_mmap_events);
		if (event->attr.comm)
			atomic_dec(&nr_comm_events);
		if (event->attr.task)
			atomic_dec(&nr_task_events);
	}

	if (event->output) {
		fput(event->output->filp);
		event->output = NULL;
	}

	if (event->destroy)
		event->destroy(event);

	put_ctx(event->ctx);
	call_rcu(&event->rcu_head, free_event_rcu);
}

/*
 * Called when the last reference to the file is gone.
 */
static int perf_release(struct inode *inode, struct file *file)
{
	struct perf_event *event = file->private_data;
	struct perf_event_context *ctx = event->ctx;

	file->private_data = NULL;

	WARN_ON_ONCE(ctx->parent_ctx);
	mutex_lock(&ctx->mutex);
	perf_event_remove_from_context(event);
	mutex_unlock(&ctx->mutex);

	mutex_lock(&event->owner->perf_event_mutex);
	list_del_init(&event->owner_entry);
	mutex_unlock(&event->owner->perf_event_mutex);
	put_task_struct(event->owner);

	free_event(event);

	return 0;
}

static int perf_event_read_size(struct perf_event *event)
{
	int entry = sizeof(u64); /* value */
	int size = 0;
	int nr = 1;

	if (event->attr.read_format & PERF_FORMAT_TOTAL_TIME_ENABLED)
		size += sizeof(u64);

	if (event->attr.read_format & PERF_FORMAT_TOTAL_TIME_RUNNING)
		size += sizeof(u64);

	if (event->attr.read_format & PERF_FORMAT_ID)
		entry += sizeof(u64);

	if (event->attr.read_format & PERF_FORMAT_GROUP) {
		nr += event->group_leader->nr_siblings;
		size += sizeof(u64);
	}

	size += entry * nr;

	return size;
}

static u64 perf_event_read_value(struct perf_event *event)
{
	struct perf_event *child;
	u64 total = 0;

	total += perf_event_read(event);
	list_for_each_entry(child, &event->child_list, child_list)
		total += perf_event_read(child);

	return total;
}

static int perf_event_read_entry(struct perf_event *event,
				   u64 read_format, char __user *buf)
{
	int n = 0, count = 0;
	u64 values[2];

	values[n++] = perf_event_read_value(event);
	if (read_format & PERF_FORMAT_ID)
		values[n++] = primary_event_id(event);

	count = n * sizeof(u64);

	if (copy_to_user(buf, values, count))
		return -EFAULT;

	return count;
}

static int perf_event_read_group(struct perf_event *event,
				   u64 read_format, char __user *buf)
{
	struct perf_event *leader = event->group_leader, *sub;
	int n = 0, size = 0, err = -EFAULT;
	u64 values[3];

	values[n++] = 1 + leader->nr_siblings;
	if (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED) {
		values[n++] = leader->total_time_enabled +
			atomic64_read(&leader->child_total_time_enabled);
	}
	if (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING) {
		values[n++] = leader->total_time_running +
			atomic64_read(&leader->child_total_time_running);
	}

	size = n * sizeof(u64);

	if (copy_to_user(buf, values, size))
		return -EFAULT;

	err = perf_event_read_entry(leader, read_format, buf + size);
	if (err < 0)
		return err;

	size += err;

	list_for_each_entry(sub, &leader->sibling_list, group_entry) {
		err = perf_event_read_entry(sub, read_format,
				buf + size);
		if (err < 0)
			return err;

		size += err;
	}

	return size;
}

static int perf_event_read_one(struct perf_event *event,
				 u64 read_format, char __user *buf)
{
	u64 values[4];
	int n = 0;

	values[n++] = perf_event_read_value(event);
	if (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED) {
		values[n++] = event->total_time_enabled +
			atomic64_read(&event->child_total_time_enabled);
	}
	if (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING) {
		values[n++] = event->total_time_running +
			atomic64_read(&event->child_total_time_running);
	}
	if (read_format & PERF_FORMAT_ID)
		values[n++] = primary_event_id(event);

	if (copy_to_user(buf, values, n * sizeof(u64)))
		return -EFAULT;

	return n * sizeof(u64);
}

/*
 * Read the performance event - simple non blocking version for now
 */
static ssize_t
perf_read_hw(struct perf_event *event, char __user *buf, size_t count)
{
	u64 read_format = event->attr.read_format;
	int ret;

	/*
	 * Return end-of-file for a read on a event that is in
	 * error state (i.e. because it was pinned but it couldn't be
	 * scheduled on to the CPU at some point).
	 */
	if (event->state == PERF_EVENT_STATE_ERROR)
		return 0;

	if (count < perf_event_read_size(event))
		return -ENOSPC;

	WARN_ON_ONCE(event->ctx->parent_ctx);
	mutex_lock(&event->child_mutex);
	if (read_format & PERF_FORMAT_GROUP)
		ret = perf_event_read_group(event, read_format, buf);
	else
		ret = perf_event_read_one(event, read_format, buf);
	mutex_unlock(&event->child_mutex);

	return ret;
}

static ssize_t
perf_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct perf_event *event = file->private_data;

	return perf_read_hw(event, buf, count);
}

static unsigned int perf_poll(struct file *file, poll_table *wait)
{
	struct perf_event *event = file->private_data;
	struct perf_mmap_data *data;
	unsigned int events = POLL_HUP;

	rcu_read_lock();
	data = rcu_dereference(event->data);
	if (data)
		events = atomic_xchg(&data->poll, 0);
	rcu_read_unlock();

	poll_wait(file, &event->waitq, wait);

	return events;
}

static void perf_event_reset(struct perf_event *event)
{
	(void)perf_event_read(event);
	atomic64_set(&event->count, 0);
	perf_event_update_userpage(event);
}

/*
 * Holding the top-level event's child_mutex means that any
 * descendant process that has inherited this event will block
 * in sync_child_event if it goes to exit, thus satisfying the
 * task existence requirements of perf_event_enable/disable.
 */
static void perf_event_for_each_child(struct perf_event *event,
					void (*func)(struct perf_event *))
{
	struct perf_event *child;

	WARN_ON_ONCE(event->ctx->parent_ctx);
	mutex_lock(&event->child_mutex);
	func(event);
	list_for_each_entry(child, &event->child_list, child_list)
		func(child);
	mutex_unlock(&event->child_mutex);
}

static void perf_event_for_each(struct perf_event *event,
				  void (*func)(struct perf_event *))
{
	struct perf_event_context *ctx = event->ctx;
	struct perf_event *sibling;

	WARN_ON_ONCE(ctx->parent_ctx);
	mutex_lock(&ctx->mutex);
	event = event->group_leader;

	perf_event_for_each_child(event, func);
	func(event);
	list_for_each_entry(sibling, &event->sibling_list, group_entry)
		perf_event_for_each_child(event, func);
	mutex_unlock(&ctx->mutex);
}

static int perf_event_period(struct perf_event *event, u64 __user *arg)
{
	struct perf_event_context *ctx = event->ctx;
	unsigned long size;
	int ret = 0;
	u64 value;

	if (!event->attr.sample_period)
		return -EINVAL;

	size = copy_from_user(&value, arg, sizeof(value));
	if (size != sizeof(value))
		return -EFAULT;

	if (!value)
		return -EINVAL;

	spin_lock_irq(&ctx->lock);
	if (event->attr.freq) {
		if (value > sysctl_perf_event_sample_rate) {
			ret = -EINVAL;
			goto unlock;
		}

		event->attr.sample_freq = value;
	} else {
		event->attr.sample_period = value;
		event->hw.sample_period = value;
	}
unlock:
	spin_unlock_irq(&ctx->lock);

	return ret;
}

int perf_event_set_output(struct perf_event *event, int output_fd);

static long perf_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct perf_event *event = file->private_data;
	void (*func)(struct perf_event *);
	u32 flags = arg;

	switch (cmd) {
	case PERF_EVENT_IOC_ENABLE:
		func = perf_event_enable;
		break;
	case PERF_EVENT_IOC_DISABLE:
		func = perf_event_disable;
		break;
	case PERF_EVENT_IOC_RESET:
		func = perf_event_reset;
		break;

	case PERF_EVENT_IOC_REFRESH:
		return perf_event_refresh(event, arg);

	case PERF_EVENT_IOC_PERIOD:
		return perf_event_period(event, (u64 __user *)arg);

	case PERF_EVENT_IOC_SET_OUTPUT:
		return perf_event_set_output(event, arg);

	default:
		return -ENOTTY;
	}

	if (flags & PERF_IOC_FLAG_GROUP)
		perf_event_for_each(event, func);
	else
		perf_event_for_each_child(event, func);

	return 0;
}

int perf_event_task_enable(void)
{
	struct perf_event *event;

	mutex_lock(&current->perf_event_mutex);
	list_for_each_entry(event, &current->perf_event_list, owner_entry)
		perf_event_for_each_child(event, perf_event_enable);
	mutex_unlock(&current->perf_event_mutex);

	return 0;
}

int perf_event_task_disable(void)
{
	struct perf_event *event;

	mutex_lock(&current->perf_event_mutex);
	list_for_each_entry(event, &current->perf_event_list, owner_entry)
		perf_event_for_each_child(event, perf_event_disable);
	mutex_unlock(&current->perf_event_mutex);

	return 0;
}

#ifndef PERF_EVENT_INDEX_OFFSET
# define PERF_EVENT_INDEX_OFFSET 0
#endif

static int perf_event_index(struct perf_event *event)
{
	if (event->state != PERF_EVENT_STATE_ACTIVE)
		return 0;

	return event->hw.idx + 1 - PERF_EVENT_INDEX_OFFSET;
}

/*
 * Callers need to ensure there can be no nesting of this function, otherwise
 * the seqlock logic goes bad. We can not serialize this because the arch
 * code calls this from NMI context.
 */
void perf_event_update_userpage(struct perf_event *event)
{
	struct perf_event_mmap_page *userpg;
	struct perf_mmap_data *data;

	rcu_read_lock();
	data = rcu_dereference(event->data);
	if (!data)
		goto unlock;

	userpg = data->user_page;

	/*
	 * Disable preemption so as to not let the corresponding user-space
	 * spin too long if we get preempted.
	 */
	preempt_disable();
	++userpg->lock;
	barrier();
	userpg->index = perf_event_index(event);
	userpg->offset = atomic64_read(&event->count);
	if (event->state == PERF_EVENT_STATE_ACTIVE)
		userpg->offset -= atomic64_read(&event->hw.prev_count);

	userpg->time_enabled = event->total_time_enabled +
			atomic64_read(&event->child_total_time_enabled);

	userpg->time_running = event->total_time_running +
			atomic64_read(&event->child_total_time_running);

	barrier();
	++userpg->lock;
	preempt_enable();
unlock:
	rcu_read_unlock();
}

static unsigned long perf_data_size(struct perf_mmap_data *data)
{
	return data->nr_pages << (PAGE_SHIFT + data->data_order);
}

#ifndef CONFIG_PERF_USE_VMALLOC

/*
 * Back perf_mmap() with regular GFP_KERNEL-0 pages.
 */

static struct page *
perf_mmap_to_page(struct perf_mmap_data *data, unsigned long pgoff)
{
	if (pgoff > data->nr_pages)
		return NULL;

	if (pgoff == 0)
		return virt_to_page(data->user_page);

	return virt_to_page(data->data_pages[pgoff - 1]);
}

static struct perf_mmap_data *
perf_mmap_data_alloc(struct perf_event *event, int nr_pages)
{
	struct perf_mmap_data *data;
	unsigned long size;
	int i;

	WARN_ON(atomic_read(&event->mmap_count));

	size = sizeof(struct perf_mmap_data);
	size += nr_pages * sizeof(void *);

	data = kzalloc(size, GFP_KERNEL);
	if (!data)
		goto fail;

	data->user_page = (void *)get_zeroed_page(GFP_KERNEL);
	if (!data->user_page)
		goto fail_user_page;

	for (i = 0; i < nr_pages; i++) {
		data->data_pages[i] = (void *)get_zeroed_page(GFP_KERNEL);
		if (!data->data_pages[i])
			goto fail_data_pages;
	}

	data->data_order = 0;
	data->nr_pages = nr_pages;

	return data;

fail_data_pages:
	for (i--; i >= 0; i--)
		free_page((unsigned long)data->data_pages[i]);

	free_page((unsigned long)data->user_page);

fail_user_page:
	kfree(data);

fail:
	return NULL;
}

static void perf_mmap_free_page(unsigned long addr)
{
	struct page *page = virt_to_page((void *)addr);

	page->mapping = NULL;
	__free_page(page);
}

static void perf_mmap_data_free(struct perf_mmap_data *data)
{
	int i;

	perf_mmap_free_page((unsigned long)data->user_page);
	for (i = 0; i < data->nr_pages; i++)
		perf_mmap_free_page((unsigned long)data->data_pages[i]);
	kfree(data);
}

#else

/*
 * Back perf_mmap() with vmalloc memory.
 *
 * Required for architectures that have d-cache aliasing issues.
 */

static struct page *
perf_mmap_to_page(struct perf_mmap_data *data, unsigned long pgoff)
{
	if (pgoff > (1UL << data->data_order))
		return NULL;

	return vmalloc_to_page((void *)data->user_page + pgoff * PAGE_SIZE);
}

static void perf_mmap_unmark_page(void *addr)
{
	struct page *page = vmalloc_to_page(addr);

	page->mapping = NULL;
}

static void perf_mmap_data_free_work(struct work_struct *work)
{
	struct perf_mmap_data *data;
	void *base;
	int i, nr;

	data = container_of(work, struct perf_mmap_data, work);
	nr = 1 << data->data_order;

	base = data->user_page;
	for (i = 0; i < nr + 1; i++)
		perf_mmap_unmark_page(base + (i * PAGE_SIZE));

	vfree(base);
	kfree(data);
}

static void perf_mmap_data_free(struct perf_mmap_data *data)
{
	schedule_work(&data->work);
}

static struct perf_mmap_data *
perf_mmap_data_alloc(struct perf_event *event, int nr_pages)
{
	struct perf_mmap_data *data;
	unsigned long size;
	void *all_buf;

	WARN_ON(atomic_read(&event->mmap_count));

	size = sizeof(struct perf_mmap_data);
	size += sizeof(void *);

	data = kzalloc(size, GFP_KERNEL);
	if (!data)
		goto fail;

	INIT_WORK(&data->work, perf_mmap_data_free_work);

	all_buf = vmalloc_user((nr_pages + 1) * PAGE_SIZE);
	if (!all_buf)
		goto fail_all_buf;

	data->user_page = all_buf;
	data->data_pages[0] = all_buf + PAGE_SIZE;
	data->data_order = ilog2(nr_pages);
	data->nr_pages = 1;

	return data;

fail_all_buf:
	kfree(data);

fail:
	return NULL;
}

#endif

static int perf_mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct perf_event *event = vma->vm_file->private_data;
	struct perf_mmap_data *data;
	int ret = VM_FAULT_SIGBUS;

	if (vmf->flags & FAULT_FLAG_MKWRITE) {
		if (vmf->pgoff == 0)
			ret = 0;
		return ret;
	}

	rcu_read_lock();
	data = rcu_dereference(event->data);
	if (!data)
		goto unlock;

	if (vmf->pgoff && (vmf->flags & FAULT_FLAG_WRITE))
		goto unlock;

	vmf->page = perf_mmap_to_page(data, vmf->pgoff);
	if (!vmf->page)
		goto unlock;

	get_page(vmf->page);
	vmf->page->mapping = vma->vm_file->f_mapping;
	vmf->page->index   = vmf->pgoff;

	ret = 0;
unlock:
	rcu_read_unlock();

	return ret;
}

static void
perf_mmap_data_init(struct perf_event *event, struct perf_mmap_data *data)
{
	long max_size = perf_data_size(data);

	atomic_set(&data->lock, -1);

	if (event->attr.watermark) {
		data->watermark = min_t(long, max_size,
					event->attr.wakeup_watermark);
	}

	if (!data->watermark)
		data->watermark = max_t(long, PAGE_SIZE, max_size / 2);


	rcu_assign_pointer(event->data, data);
}

static void perf_mmap_data_free_rcu(struct rcu_head *rcu_head)
{
	struct perf_mmap_data *data;

	data = container_of(rcu_head, struct perf_mmap_data, rcu_head);
	perf_mmap_data_free(data);
}

static void perf_mmap_data_release(struct perf_event *event)
{
	struct perf_mmap_data *data = event->data;

	WARN_ON(atomic_read(&event->mmap_count));

	rcu_assign_pointer(event->data, NULL);
	call_rcu(&data->rcu_head, perf_mmap_data_free_rcu);
}

static void perf_mmap_open(struct vm_area_struct *vma)
{
	struct perf_event *event = vma->vm_file->private_data;

	atomic_inc(&event->mmap_count);
}

static void perf_mmap_close(struct vm_area_struct *vma)
{
	struct perf_event *event = vma->vm_file->private_data;

	WARN_ON_ONCE(event->ctx->parent_ctx);
	if (atomic_dec_and_mutex_lock(&event->mmap_count, &event->mmap_mutex)) {
		unsigned long size = perf_data_size(event->data);
		struct user_struct *user = current_user();

		atomic_long_sub((size >> PAGE_SHIFT) + 1, &user->locked_vm);
		vma->vm_mm->locked_vm -= event->data->nr_locked;
		perf_mmap_data_release(event);
		mutex_unlock(&event->mmap_mutex);
	}
}

static const struct vm_operations_struct perf_mmap_vmops = {
	.open		= perf_mmap_open,
	.close		= perf_mmap_close,
	.fault		= perf_mmap_fault,
	.page_mkwrite	= perf_mmap_fault,
};

static int perf_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct perf_event *event = file->private_data;
	unsigned long user_locked, user_lock_limit;
	struct user_struct *user = current_user();
	unsigned long locked, lock_limit;
	struct perf_mmap_data *data;
	unsigned long vma_size;
	unsigned long nr_pages;
	long user_extra, extra;
	int ret = 0;

	if (!(vma->vm_flags & VM_SHARED))
		return -EINVAL;

	vma_size = vma->vm_end - vma->vm_start;
	nr_pages = (vma_size / PAGE_SIZE) - 1;

	/*
	 * If we have data pages ensure they're a power-of-two number, so we
	 * can do bitmasks instead of modulo.
	 */
	if (nr_pages != 0 && !is_power_of_2(nr_pages))
		return -EINVAL;

	if (vma_size != PAGE_SIZE * (1 + nr_pages))
		return -EINVAL;

	if (vma->vm_pgoff != 0)
		return -EINVAL;

	WARN_ON_ONCE(event->ctx->parent_ctx);
	mutex_lock(&event->mmap_mutex);
	if (event->output) {
		ret = -EINVAL;
		goto unlock;
	}

	if (atomic_inc_not_zero(&event->mmap_count)) {
		if (nr_pages != event->data->nr_pages)
			ret = -EINVAL;
		goto unlock;
	}

	user_extra = nr_pages + 1;
	user_lock_limit = sysctl_perf_event_mlock >> (PAGE_SHIFT - 10);

	/*
	 * Increase the limit linearly with more CPUs:
	 */
	user_lock_limit *= num_online_cpus();

	user_locked = atomic_long_read(&user->locked_vm) + user_extra;

	extra = 0;
	if (user_locked > user_lock_limit)
		extra = user_locked - user_lock_limit;

	lock_limit = current->signal->rlim[RLIMIT_MEMLOCK].rlim_cur;
	lock_limit >>= PAGE_SHIFT;
	locked = vma->vm_mm->locked_vm + extra;

	if ((locked > lock_limit) && perf_paranoid_tracepoint_raw() &&
		!capable(CAP_IPC_LOCK)) {
		ret = -EPERM;
		goto unlock;
	}

	WARN_ON(event->data);

	data = perf_mmap_data_alloc(event, nr_pages);
	ret = -ENOMEM;
	if (!data)
		goto unlock;

	ret = 0;
	perf_mmap_data_init(event, data);

	atomic_set(&event->mmap_count, 1);
	atomic_long_add(user_extra, &user->locked_vm);
	vma->vm_mm->locked_vm += extra;
	event->data->nr_locked = extra;
	if (vma->vm_flags & VM_WRITE)
		event->data->writable = 1;

unlock:
	mutex_unlock(&event->mmap_mutex);

	vma->vm_flags |= VM_RESERVED;
	vma->vm_ops = &perf_mmap_vmops;

	return ret;
}

static int perf_fasync(int fd, struct file *filp, int on)
{
	struct inode *inode = filp->f_path.dentry->d_inode;
	struct perf_event *event = filp->private_data;
	int retval;

	mutex_lock(&inode->i_mutex);
	retval = fasync_helper(fd, filp, on, &event->fasync);
	mutex_unlock(&inode->i_mutex);

	if (retval < 0)
		return retval;

	return 0;
}

static const struct file_operations perf_fops = {
	.release		= perf_release,
	.read			= perf_read,
	.poll			= perf_poll,
	.unlocked_ioctl		= perf_ioctl,
	.compat_ioctl		= perf_ioctl,
	.mmap			= perf_mmap,
	.fasync			= perf_fasync,
};

/*
 * Perf event wakeup
 *
 * If there's data, ensure we set the poll() state and publish everything
 * to user-space before waking everybody up.
 */

void perf_event_wakeup(struct perf_event *event)
{
	wake_up_all(&event->waitq);

	if (event->pending_kill) {
		kill_fasync(&event->fasync, SIGIO, event->pending_kill);
		event->pending_kill = 0;
	}
}

/*
 * Pending wakeups
 *
 * Handle the case where we need to wakeup up from NMI (or rq->lock) context.
 *
 * The NMI bit means we cannot possibly take locks. Therefore, maintain a
 * single linked list and use cmpxchg() to add entries lockless.
 */

static void perf_pending_event(struct perf_pending_entry *entry)
{
	struct perf_event *event = container_of(entry,
			struct perf_event, pending);

	if (event->pending_disable) {
		event->pending_disable = 0;
		__perf_event_disable(event);
	}

	if (event->pending_wakeup) {
		event->pending_wakeup = 0;
		perf_event_wakeup(event);
	}
}

#define PENDING_TAIL ((struct perf_pending_entry *)-1UL)

static DEFINE_PER_CPU(struct perf_pending_entry *, perf_pending_head) = {
	PENDING_TAIL,
};

static void perf_pending_queue(struct perf_pending_entry *entry,
			       void (*func)(struct perf_pending_entry *))
{
	struct perf_pending_entry **head;

	if (cmpxchg(&entry->next, NULL, PENDING_TAIL) != NULL)
		return;

	entry->func = func;

	head = &get_cpu_var(perf_pending_head);

	do {
		entry->next = *head;
	} while (cmpxchg(head, entry->next, entry) != entry->next);

	set_perf_event_pending();

	put_cpu_var(perf_pending_head);
}

static int __perf_pending_run(void)
{
	struct perf_pending_entry *list;
	int nr = 0;

	list = xchg(&__get_cpu_var(perf_pending_head), PENDING_TAIL);
	while (list != PENDING_TAIL) {
		void (*func)(struct perf_pending_entry *);
		struct perf_pending_entry *entry = list;

		list = list->next;

		func = entry->func;
		entry->next = NULL;
		/*
		 * Ensure we observe the unqueue before we issue the wakeup,
		 * so that we won't be waiting forever.
		 * -- see perf_not_pending().
		 */
		smp_wmb();

		func(entry);
		nr++;
	}

	return nr;
}

static inline int perf_not_pending(struct perf_event *event)
{
	/*
	 * If we flush on whatever cpu we run, there is a chance we don't
	 * need to wait.
	 */
	get_cpu();
	__perf_pending_run();
	put_cpu();

	/*
	 * Ensure we see the proper queue state before going to sleep
	 * so that we do not miss the wakeup. -- see perf_pending_handle()
	 */
	smp_rmb();
	return event->pending.next == NULL;
}

static void perf_pending_sync(struct perf_event *event)
{
	wait_event(event->waitq, perf_not_pending(event));
}

void perf_event_do_pending(void)
{
	__perf_pending_run();
}

/*
 * Callchain support -- arch specific
 */

__weak struct perf_callchain_entry *perf_callchain(struct pt_regs *regs)
{
	return NULL;
}

/*
 * Output
 */
static bool perf_output_space(struct perf_mmap_data *data, unsigned long tail,
			      unsigned long offset, unsigned long head)
{
	unsigned long mask;

	if (!data->writable)
		return true;

	mask = perf_data_size(data) - 1;

	offset = (offset - tail) & mask;
	head   = (head   - tail) & mask;

	if ((int)(head - offset) < 0)
		return false;

	return true;
}

static void perf_output_wakeup(struct perf_output_handle *handle)
{
	atomic_set(&handle->data->poll, POLL_IN);

	if (handle->nmi) {
		handle->event->pending_wakeup = 1;
		perf_pending_queue(&handle->event->pending,
				   perf_pending_event);
	} else
		perf_event_wakeup(handle->event);
}

/*
 * Curious locking construct.
 *
 * We need to ensure a later event_id doesn't publish a head when a former
 * event_id isn't done writing. However since we need to deal with NMIs we
 * cannot fully serialize things.
 *
 * What we do is serialize between CPUs so we only have to deal with NMI
 * nesting on a single CPU.
 *
 * We only publish the head (and generate a wakeup) when the outer-most
 * event_id completes.
 */
static void perf_output_lock(struct perf_output_handle *handle)
{
	struct perf_mmap_data *data = handle->data;
	int cpu;

	handle->locked = 0;

	local_irq_save(handle->flags);
	cpu = smp_processor_id();

	if (in_nmi() && atomic_read(&data->lock) == cpu)
		return;

	while (atomic_cmpxchg(&data->lock, -1, cpu) != -1)
		cpu_relax();

	handle->locked = 1;
}

static void perf_output_unlock(struct perf_output_handle *handle)
{
	struct perf_mmap_data *data = handle->data;
	unsigned long head;
	int cpu;

	data->done_head = data->head;

	if (!handle->locked)
		goto out;

again:
	/*
	 * The xchg implies a full barrier that ensures all writes are done
	 * before we publish the new head, matched by a rmb() in userspace when
	 * reading this position.
	 */
	while ((head = atomic_long_xchg(&data->done_head, 0)))
		data->user_page->data_head = head;

	/*
	 * NMI can happen here, which means we can miss a done_head update.
	 */

	cpu = atomic_xchg(&data->lock, -1);
	WARN_ON_ONCE(cpu != smp_processor_id());

	/*
	 * Therefore we have to validate we did not indeed do so.
	 */
	if (unlikely(atomic_long_read(&data->done_head))) {
		/*
		 * Since we had it locked, we can lock it again.
		 */
		while (atomic_cmpxchg(&data->lock, -1, cpu) != -1)
			cpu_relax();

		goto again;
	}

	if (atomic_xchg(&data->wakeup, 0))
		perf_output_wakeup(handle);
out:
	local_irq_restore(handle->flags);
}

void perf_output_copy(struct perf_output_handle *handle,
		      const void *buf, unsigned int len)
{
	unsigned int pages_mask;
	unsigned long offset;
	unsigned int size;
	void **pages;

	offset		= handle->offset;
	pages_mask	= handle->data->nr_pages - 1;
	pages		= handle->data->data_pages;

	do {
		unsigned long page_offset;
		unsigned long page_size;
		int nr;

		nr	    = (offset >> PAGE_SHIFT) & pages_mask;
		page_size   = 1UL << (handle->data->data_order + PAGE_SHIFT);
		page_offset = offset & (page_size - 1);
		size	    = min_t(unsigned int, page_size - page_offset, len);

		memcpy(pages[nr] + page_offset, buf, size);

		len	    -= size;
		buf	    += size;
		offset	    += size;
	} while (len);

	handle->offset = offset;

	/*
	 * Check we didn't copy past our reservation window, taking the
	 * possible unsigned int wrap into account.
	 */
	WARN_ON_ONCE(((long)(handle->head - handle->offset)) < 0);
}

int perf_output_begin(struct perf_output_handle *handle,
		      struct perf_event *event, unsigned int size,
		      int nmi, int sample)
{
	struct perf_event *output_event;
	struct perf_mmap_data *data;
	unsigned long tail, offset, head;
	int have_lost;
	struct {
		struct perf_event_header header;
		u64			 id;
		u64			 lost;
	} lost_event;

	rcu_read_lock();
	/*
	 * For inherited events we send all the output towards the parent.
	 */
	if (event->parent)
		event = event->parent;

	output_event = rcu_dereference(event->output);
	if (output_event)
		event = output_event;

	data = rcu_dereference(event->data);
	if (!data)
		goto out;

	handle->data	= data;
	handle->event	= event;
	handle->nmi	= nmi;
	handle->sample	= sample;

	if (!data->nr_pages)
		goto fail;

	have_lost = atomic_read(&data->lost);
	if (have_lost)
		size += sizeof(lost_event);

	perf_output_lock(handle);

	do {
		/*
		 * Userspace could choose to issue a mb() before updating the
		 * tail pointer. So that all reads will be completed before the
		 * write is issued.
		 */
		tail = ACCESS_ONCE(data->user_page->data_tail);
		smp_rmb();
		offset = head = atomic_long_read(&data->head);
		head += size;
		if (unlikely(!perf_output_space(data, tail, offset, head)))
			goto fail;
	} while (atomic_long_cmpxchg(&data->head, offset, head) != offset);

	handle->offset	= offset;
	handle->head	= head;

	if (head - tail > data->watermark)
		atomic_set(&data->wakeup, 1);

	if (have_lost) {
		lost_event.header.type = PERF_RECORD_LOST;
		lost_event.header.misc = 0;
		lost_event.header.size = sizeof(lost_event);
		lost_event.id          = event->id;
		lost_event.lost        = atomic_xchg(&data->lost, 0);

		perf_output_put(handle, lost_event);
	}

	return 0;

fail:
	atomic_inc(&data->lost);
	perf_output_unlock(handle);
out:
	rcu_read_unlock();

	return -ENOSPC;
}

void perf_output_end(struct perf_output_handle *handle)
{
	struct perf_event *event = handle->event;
	struct perf_mmap_data *data = handle->data;

	int wakeup_events = event->attr.wakeup_events;

	if (handle->sample && wakeup_events) {
		int events = atomic_inc_return(&data->events);
		if (events >= wakeup_events) {
			atomic_sub(wakeup_events, &data->events);
			atomic_set(&data->wakeup, 1);
		}
	}

	perf_output_unlock(handle);
	rcu_read_unlock();
}

static u32 perf_event_pid(struct perf_event *event, struct task_struct *p)
{
	/*
	 * only top level events have the pid namespace they were created in
	 */
	if (event->parent)
		event = event->parent;

	return task_tgid_nr_ns(p, event->ns);
}

static u32 perf_event_tid(struct perf_event *event, struct task_struct *p)
{
	/*
	 * only top level events have the pid namespace they were created in
	 */
	if (event->parent)
		event = event->parent;

	return task_pid_nr_ns(p, event->ns);
}

static void perf_output_read_one(struct perf_output_handle *handle,
				 struct perf_event *event)
{
	u64 read_format = event->attr.read_format;
	u64 values[4];
	int n = 0;

	values[n++] = atomic64_read(&event->count);
	if (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED) {
		values[n++] = event->total_time_enabled +
			atomic64_read(&event->child_total_time_enabled);
	}
	if (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING) {
		values[n++] = event->total_time_running +
			atomic64_read(&event->child_total_time_running);
	}
	if (read_format & PERF_FORMAT_ID)
		values[n++] = primary_event_id(event);

	perf_output_copy(handle, values, n * sizeof(u64));
}

/*
 * XXX PERF_FORMAT_GROUP vs inherited events seems difficult.
 */
static void perf_output_read_group(struct perf_output_handle *handle,
			    struct perf_event *event)
{
	struct perf_event *leader = event->group_leader, *sub;
	u64 read_format = event->attr.read_format;
	u64 values[5];
	int n = 0;

	values[n++] = 1 + leader->nr_siblings;

	if (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED)
		values[n++] = leader->total_time_enabled;

	if (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING)
		values[n++] = leader->total_time_running;

	if (leader != event)
		leader->pmu->read(leader);

	values[n++] = atomic64_read(&leader->count);
	if (read_format & PERF_FORMAT_ID)
		values[n++] = primary_event_id(leader);

	perf_output_copy(handle, values, n * sizeof(u64));

	list_for_each_entry(sub, &leader->sibling_list, group_entry) {
		n = 0;

		if (sub != event)
			sub->pmu->read(sub);

		values[n++] = atomic64_read(&sub->count);
		if (read_format & PERF_FORMAT_ID)
			values[n++] = primary_event_id(sub);

		perf_output_copy(handle, values, n * sizeof(u64));
	}
}

static void perf_output_read(struct perf_output_handle *handle,
			     struct perf_event *event)
{
	if (event->attr.read_format & PERF_FORMAT_GROUP)
		perf_output_read_group(handle, event);
	else
		perf_output_read_one(handle, event);
}

void perf_output_sample(struct perf_output_handle *handle,
			struct perf_event_header *header,
			struct perf_sample_data *data,
			struct perf_event *event)
{
	u64 sample_type = data->type;

	perf_output_put(handle, *header);

	if (sample_type & PERF_SAMPLE_IP)
		perf_output_put(handle, data->ip);

	if (sample_type & PERF_SAMPLE_TID)
		perf_output_put(handle, data->tid_entry);

	if (sample_type & PERF_SAMPLE_TIME)
		perf_output_put(handle, data->time);

	if (sample_type & PERF_SAMPLE_ADDR)
		perf_output_put(handle, data->addr);

	if (sample_type & PERF_SAMPLE_ID)
		perf_output_put(handle, data->id);

	if (sample_type & PERF_SAMPLE_STREAM_ID)
		perf_output_put(handle, data->stream_id);

	if (sample_type & PERF_SAMPLE_CPU)
		perf_output_put(handle, data->cpu_entry);

	if (sample_type & PERF_SAMPLE_PERIOD)
		perf_output_put(handle, data->period);

	if (sample_type & PERF_SAMPLE_READ)
		perf_output_read(handle, event);

	if (sample_type & PERF_SAMPLE_CALLCHAIN) {
		if (data->callchain) {
			int size = 1;

			if (data->callchain)
				size += data->callchain->nr;

			size *= sizeof(u64);

			perf_output_copy(handle, data->callchain, size);
		} else {
			u64 nr = 0;
			perf_output_put(handle, nr);
		}
	}

	if (sample_type & PERF_SAMPLE_RAW) {
		if (data->raw) {
			perf_output_put(handle, data->raw->size);
			perf_output_copy(handle, data->raw->data,
					 data->raw->size);
		} else {
			struct {
				u32	size;
				u32	data;
			} raw = {
				.size = sizeof(u32),
				.data = 0,
			};
			perf_output_put(handle, raw);
		}
	}
}

void perf_prepare_sample(struct perf_event_header *header,
			 struct perf_sample_data *data,
			 struct perf_event *event,
			 struct pt_regs *regs)
{
	u64 sample_type = event->attr.sample_type;

	data->type = sample_type;

	header->type = PERF_RECORD_SAMPLE;
	header->size = sizeof(*header);

	header->misc = 0;
	header->misc |= perf_misc_flags(regs);

	if (sample_type & PERF_SAMPLE_IP) {
		data->ip = perf_instruction_pointer(regs);

		header->size += sizeof(data->ip);
	}

	if (sample_type & PERF_SAMPLE_TID) {
		/* namespace issues */
		data->tid_entry.pid = perf_event_pid(event, current);
		data->tid_entry.tid = perf_event_tid(event, current);

		header->size += sizeof(data->tid_entry);
	}

	if (sample_type & PERF_SAMPLE_TIME) {
		data->time = perf_clock();

		header->size += sizeof(data->time);
	}

	if (sample_type & PERF_SAMPLE_ADDR)
		header->size += sizeof(data->addr);

	if (sample_type & PERF_SAMPLE_ID) {
		data->id = primary_event_id(event);

		header->size += sizeof(data->id);
	}

	if (sample_type & PERF_SAMPLE_STREAM_ID) {
		data->stream_id = event->id;

		header->size += sizeof(data->stream_id);
	}

	if (sample_type & PERF_SAMPLE_CPU) {
		data->cpu_entry.cpu		= raw_smp_processor_id();
		data->cpu_entry.reserved	= 0;

		header->size += sizeof(data->cpu_entry);
	}

	if (sample_type & PERF_SAMPLE_PERIOD)
		header->size += sizeof(data->period);

	if (sample_type & PERF_SAMPLE_READ)
		header->size += perf_event_read_size(event);

	if (sample_type & PERF_SAMPLE_CALLCHAIN) {
		int size = 1;

		data->callchain = perf_callchain(regs);

		if (data->callchain)
			size += data->callchain->nr;

		header->size += size * sizeof(u64);
	}

	if (sample_type & PERF_SAMPLE_RAW) {
		int size = sizeof(u32);

		if (data->raw)
			size += data->raw->size;
		else
			size += sizeof(u32);

		WARN_ON_ONCE(size & (sizeof(u64)-1));
		header->size += size;
	}
}

static void perf_event_output(struct perf_event *event, int nmi,
				struct perf_sample_data *data,
				struct pt_regs *regs)
{
	struct perf_output_handle handle;
	struct perf_event_header header;

	perf_prepare_sample(&header, data, event, regs);

	if (perf_output_begin(&handle, event, header.size, nmi, 1))
		return;

	perf_output_sample(&handle, &header, data, event);

	perf_output_end(&handle);
}

/*
 * read event_id
 */

struct perf_read_event {
	struct perf_event_header	header;

	u32				pid;
	u32				tid;
};

static void
perf_event_read_event(struct perf_event *event,
			struct task_struct *task)
{
	struct perf_output_handle handle;
	struct perf_read_event read_event = {
		.header = {
			.type = PERF_RECORD_READ,
			.misc = 0,
			.size = sizeof(read_event) + perf_event_read_size(event),
		},
		.pid = perf_event_pid(event, task),
		.tid = perf_event_tid(event, task),
	};
	int ret;

	ret = perf_output_begin(&handle, event, read_event.header.size, 0, 0);
	if (ret)
		return;

	perf_output_put(&handle, read_event);
	perf_output_read(&handle, event);

	perf_output_end(&handle);
}

/*
 * task tracking -- fork/exit
 *
 * enabled by: attr.comm | attr.mmap | attr.task
 */

struct perf_task_event {
	struct task_struct		*task;
	struct perf_event_context	*task_ctx;

	struct {
		struct perf_event_header	header;

		u32				pid;
		u32				ppid;
		u32				tid;
		u32				ptid;
		u64				time;
	} event_id;
};

static void perf_event_task_output(struct perf_event *event,
				     struct perf_task_event *task_event)
{
	struct perf_output_handle handle;
	int size;
	struct task_struct *task = task_event->task;
	int ret;

	size  = task_event->event_id.header.size;
	ret = perf_output_begin(&handle, event, size, 0, 0);

	if (ret)
		return;

	task_event->event_id.pid = perf_event_pid(event, task);
	task_event->event_id.ppid = perf_event_pid(event, current);

	task_event->event_id.tid = perf_event_tid(event, task);
	task_event->event_id.ptid = perf_event_tid(event, current);

	task_event->event_id.time = perf_clock();

	perf_output_put(&handle, task_event->event_id);

	perf_output_end(&handle);
}

static int perf_event_task_match(struct perf_event *event)
{
	if (event->attr.comm || event->attr.mmap || event->attr.task)
		return 1;

	return 0;
}

static void perf_event_task_ctx(struct perf_event_context *ctx,
				  struct perf_task_event *task_event)
{
	struct perf_event *event;

	if (system_state != SYSTEM_RUNNING || list_empty(&ctx->event_list))
		return;

	rcu_read_lock();
	list_for_each_entry_rcu(event, &ctx->event_list, event_entry) {
		if (perf_event_task_match(event))
			perf_event_task_output(event, task_event);
	}
	rcu_read_unlock();
}

static void perf_event_task_event(struct perf_task_event *task_event)
{
	struct perf_cpu_context *cpuctx;
	struct perf_event_context *ctx = task_event->task_ctx;

	cpuctx = &get_cpu_var(perf_cpu_context);
	perf_event_task_ctx(&cpuctx->ctx, task_event);
	put_cpu_var(perf_cpu_context);

	rcu_read_lock();
	if (!ctx)
		ctx = rcu_dereference(task_event->task->perf_event_ctxp);
	if (ctx)
		perf_event_task_ctx(ctx, task_event);
	rcu_read_unlock();
}

static void perf_event_task(struct task_struct *task,
			      struct perf_event_context *task_ctx,
			      int new)
{
	struct perf_task_event task_event;

	if (!atomic_read(&nr_comm_events) &&
	    !atomic_read(&nr_mmap_events) &&
	    !atomic_read(&nr_task_events))
		return;

	task_event = (struct perf_task_event){
		.task	  = task,
		.task_ctx = task_ctx,
		.event_id    = {
			.header = {
				.type = new ? PERF_RECORD_FORK : PERF_RECORD_EXIT,
				.misc = 0,
				.size = sizeof(task_event.event_id),
			},
			/* .pid  */
			/* .ppid */
			/* .tid  */
			/* .ptid */
		},
	};

	perf_event_task_event(&task_event);
}

void perf_event_fork(struct task_struct *task)
{
	perf_event_task(task, NULL, 1);
}

/*
 * comm tracking
 */

struct perf_comm_event {
	struct task_struct	*task;
	char			*comm;
	int			comm_size;

	struct {
		struct perf_event_header	header;

		u32				pid;
		u32				tid;
	} event_id;
};

static void perf_event_comm_output(struct perf_event *event,
				     struct perf_comm_event *comm_event)
{
	struct perf_output_handle handle;
	int size = comm_event->event_id.header.size;
	int ret = perf_output_begin(&handle, event, size, 0, 0);

	if (ret)
		return;

	comm_event->event_id.pid = perf_event_pid(event, comm_event->task);
	comm_event->event_id.tid = perf_event_tid(event, comm_event->task);

	perf_output_put(&handle, comm_event->event_id);
	perf_output_copy(&handle, comm_event->comm,
				   comm_event->comm_size);
	perf_output_end(&handle);
}

static int perf_event_comm_match(struct perf_event *event)
{
	if (event->attr.comm)
		return 1;

	return 0;
}

static void perf_event_comm_ctx(struct perf_event_context *ctx,
				  struct perf_comm_event *comm_event)
{
	struct perf_event *event;

	if (system_state != SYSTEM_RUNNING || list_empty(&ctx->event_list))
		return;

	rcu_read_lock();
	list_for_each_entry_rcu(event, &ctx->event_list, event_entry) {
		if (perf_event_comm_match(event))
			perf_event_comm_output(event, comm_event);
	}
	rcu_read_unlock();
}

static void perf_event_comm_event(struct perf_comm_event *comm_event)
{
	struct perf_cpu_context *cpuctx;
	struct perf_event_context *ctx;
	unsigned int size;
	char comm[TASK_COMM_LEN];

	memset(comm, 0, sizeof(comm));
	strncpy(comm, comm_event->task->comm, sizeof(comm));
	size = ALIGN(strlen(comm)+1, sizeof(u64));

	comm_event->comm = comm;
	comm_event->comm_size = size;

	comm_event->event_id.header.size = sizeof(comm_event->event_id) + size;

	cpuctx = &get_cpu_var(perf_cpu_context);
	perf_event_comm_ctx(&cpuctx->ctx, comm_event);
	put_cpu_var(perf_cpu_context);

	rcu_read_lock();
	/*
	 * doesn't really matter which of the child contexts the
	 * events ends up in.
	 */
	ctx = rcu_dereference(current->perf_event_ctxp);
	if (ctx)
		perf_event_comm_ctx(ctx, comm_event);
	rcu_read_unlock();
}

void perf_event_comm(struct task_struct *task)
{
	struct perf_comm_event comm_event;

	if (task->perf_event_ctxp)
		perf_event_enable_on_exec(task);

	if (!atomic_read(&nr_comm_events))
		return;

	comm_event = (struct perf_comm_event){
		.task	= task,
		/* .comm      */
		/* .comm_size */
		.event_id  = {
			.header = {
				.type = PERF_RECORD_COMM,
				.misc = 0,
				/* .size */
			},
			/* .pid */
			/* .tid */
		},
	};

	perf_event_comm_event(&comm_event);
}

/*
 * mmap tracking
 */

struct perf_mmap_event {
	struct vm_area_struct	*vma;

	const char		*file_name;
	int			file_size;

	struct {
		struct perf_event_header	header;

		u32				pid;
		u32				tid;
		u64				start;
		u64				len;
		u64				pgoff;
	} event_id;
};

static void perf_event_mmap_output(struct perf_event *event,
				     struct perf_mmap_event *mmap_event)
{
	struct perf_output_handle handle;
	int size = mmap_event->event_id.header.size;
	int ret = perf_output_begin(&handle, event, size, 0, 0);

	if (ret)
		return;

	mmap_event->event_id.pid = perf_event_pid(event, current);
	mmap_event->event_id.tid = perf_event_tid(event, current);

	perf_output_put(&handle, mmap_event->event_id);
	perf_output_copy(&handle, mmap_event->file_name,
				   mmap_event->file_size);
	perf_output_end(&handle);
}

static int perf_event_mmap_match(struct perf_event *event,
				   struct perf_mmap_event *mmap_event)
{
	if (event->attr.mmap)
		return 1;

	return 0;
}

static void perf_event_mmap_ctx(struct perf_event_context *ctx,
				  struct perf_mmap_event *mmap_event)
{
	struct perf_event *event;

	if (system_state != SYSTEM_RUNNING || list_empty(&ctx->event_list))
		return;

	rcu_read_lock();
	list_for_each_entry_rcu(event, &ctx->event_list, event_entry) {
		if (perf_event_mmap_match(event, mmap_event))
			perf_event_mmap_output(event, mmap_event);
	}
	rcu_read_unlock();
}

static void perf_event_mmap_event(struct perf_mmap_event *mmap_event)
{
	struct perf_cpu_context *cpuctx;
	struct perf_event_context *ctx;
	struct vm_area_struct *vma = mmap_event->vma;
	struct file *file = vma->vm_file;
	unsigned int size;
	char tmp[16];
	char *buf = NULL;
	const char *name;

	memset(tmp, 0, sizeof(tmp));

	if (file) {
		/*
		 * d_path works from the end of the buffer backwards, so we
		 * need to add enough zero bytes after the string to handle
		 * the 64bit alignment we do later.
		 */
		buf = kzalloc(PATH_MAX + sizeof(u64), GFP_KERNEL);
		if (!buf) {
			name = strncpy(tmp, "//enomem", sizeof(tmp));
			goto got_name;
		}
		name = d_path(&file->f_path, buf, PATH_MAX);
		if (IS_ERR(name)) {
			name = strncpy(tmp, "//toolong", sizeof(tmp));
			goto got_name;
		}
	} else {
		if (arch_vma_name(mmap_event->vma)) {
			name = strncpy(tmp, arch_vma_name(mmap_event->vma),
				       sizeof(tmp));
			goto got_name;
		}

		if (!vma->vm_mm) {
			name = strncpy(tmp, "[vdso]", sizeof(tmp));
			goto got_name;
		}

		name = strncpy(tmp, "//anon", sizeof(tmp));
		goto got_name;
	}

got_name:
	size = ALIGN(strlen(name)+1, sizeof(u64));

	mmap_event->file_name = name;
	mmap_event->file_size = size;

	mmap_event->event_id.header.size = sizeof(mmap_event->event_id) + size;

	cpuctx = &get_cpu_var(perf_cpu_context);
	perf_event_mmap_ctx(&cpuctx->ctx, mmap_event);
	put_cpu_var(perf_cpu_context);

	rcu_read_lock();
	/*
	 * doesn't really matter which of the child contexts the
	 * events ends up in.
	 */
	ctx = rcu_dereference(current->perf_event_ctxp);
	if (ctx)
		perf_event_mmap_ctx(ctx, mmap_event);
	rcu_read_unlock();

	kfree(buf);
}

void __perf_event_mmap(struct vm_area_struct *vma)
{
	struct perf_mmap_event mmap_event;

	if (!atomic_read(&nr_mmap_events))
		return;

	mmap_event = (struct perf_mmap_event){
		.vma	= vma,
		/* .file_name */
		/* .file_size */
		.event_id  = {
			.header = {
				.type = PERF_RECORD_MMAP,
				.misc = 0,
				/* .size */
			},
			/* .pid */
			/* .tid */
			.start  = vma->vm_start,
			.len    = vma->vm_end - vma->vm_start,
			.pgoff  = vma->vm_pgoff,
		},
	};

	perf_event_mmap_event(&mmap_event);
}

/*
 * IRQ throttle logging
 */

static void perf_log_throttle(struct perf_event *event, int enable)
{
	struct perf_output_handle handle;
	int ret;

	struct {
		struct perf_event_header	header;
		u64				time;
		u64				id;
		u64				stream_id;
	} throttle_event = {
		.header = {
			.type = PERF_RECORD_THROTTLE,
			.misc = 0,
			.size = sizeof(throttle_event),
		},
		.time		= perf_clock(),
		.id		= primary_event_id(event),
		.stream_id	= event->id,
	};

	if (enable)
		throttle_event.header.type = PERF_RECORD_UNTHROTTLE;

	ret = perf_output_begin(&handle, event, sizeof(throttle_event), 1, 0);
	if (ret)
		return;

	perf_output_put(&handle, throttle_event);
	perf_output_end(&handle);
}

/*
 * Generic event overflow handling, sampling.
 */

static int __perf_event_overflow(struct perf_event *event, int nmi,
				   int throttle, struct perf_sample_data *data,
				   struct pt_regs *regs)
{
	int events = atomic_read(&event->event_limit);
	struct hw_perf_event *hwc = &event->hw;
	int ret = 0;

	throttle = (throttle && event->pmu->unthrottle != NULL);

	if (!throttle) {
		hwc->interrupts++;
	} else {
		if (hwc->interrupts != MAX_INTERRUPTS) {
			hwc->interrupts++;
			if (HZ * hwc->interrupts >
					(u64)sysctl_perf_event_sample_rate) {
				hwc->interrupts = MAX_INTERRUPTS;
				perf_log_throttle(event, 0);
				ret = 1;
			}
		} else {
			/*
			 * Keep re-disabling events even though on the previous
			 * pass we disabled it - just in case we raced with a
			 * sched-in and the event got enabled again:
			 */
			ret = 1;
		}
	}

	if (event->attr.freq) {
		u64 now = perf_clock();
		s64 delta = now - hwc->freq_stamp;

		hwc->freq_stamp = now;

		if (delta > 0 && delta < TICK_NSEC)
			perf_adjust_period(event, NSEC_PER_SEC / (int)delta);
	}

	/*
	 * XXX event_limit might not quite work as expected on inherited
	 * events
	 */

	event->pending_kill = POLL_IN;
	if (events && atomic_dec_and_test(&event->event_limit)) {
		ret = 1;
		event->pending_kill = POLL_HUP;
		if (nmi) {
			event->pending_disable = 1;
			perf_pending_queue(&event->pending,
					   perf_pending_event);
		} else
			perf_event_disable(event);
	}

	perf_event_output(event, nmi, data, regs);
	return ret;
}

int perf_event_overflow(struct perf_event *event, int nmi,
			  struct perf_sample_data *data,
			  struct pt_regs *regs)
{
	return __perf_event_overflow(event, nmi, 1, data, regs);
}

/*
 * Generic software event infrastructure
 */

/*
 * We directly increment event->count and keep a second value in
 * event->hw.period_left to count intervals. This period event
 * is kept in the range [-sample_period, 0] so that we can use the
 * sign as trigger.
 */

static u64 perf_swevent_set_period(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	u64 period = hwc->last_period;
	u64 nr, offset;
	s64 old, val;

	hwc->last_period = hwc->sample_period;

again:
	old = val = atomic64_read(&hwc->period_left);
	if (val < 0)
		return 0;

	nr = div64_u64(period + val, period);
	offset = nr * period;
	val -= offset;
	if (atomic64_cmpxchg(&hwc->period_left, old, val) != old)
		goto again;

	return nr;
}

static void perf_swevent_overflow(struct perf_event *event,
				    int nmi, struct perf_sample_data *data,
				    struct pt_regs *regs)
{
	struct hw_perf_event *hwc = &event->hw;
	int throttle = 0;
	u64 overflow;

	data->period = event->hw.last_period;
	overflow = perf_swevent_set_period(event);

	if (hwc->interrupts == MAX_INTERRUPTS)
		return;

	for (; overflow; overflow--) {
		if (__perf_event_overflow(event, nmi, throttle,
					    data, regs)) {
			/*
			 * We inhibit the overflow from happening when
			 * hwc->interrupts == MAX_INTERRUPTS.
			 */
			break;
		}
		throttle = 1;
	}
}

static void perf_swevent_unthrottle(struct perf_event *event)
{
	/*
	 * Nothing to do, we already reset hwc->interrupts.
	 */
}

static void perf_swevent_add(struct perf_event *event, u64 nr,
			       int nmi, struct perf_sample_data *data,
			       struct pt_regs *regs)
{
	struct hw_perf_event *hwc = &event->hw;

	atomic64_add(nr, &event->count);

	if (!hwc->sample_period)
		return;

	if (!regs)
		return;

	if (!atomic64_add_negative(nr, &hwc->period_left))
		perf_swevent_overflow(event, nmi, data, regs);
}

static int perf_swevent_is_counting(struct perf_event *event)
{
	/*
	 * The event is active, we're good!
	 */
	if (event->state == PERF_EVENT_STATE_ACTIVE)
		return 1;

	/*
	 * The event is off/error, not counting.
	 */
	if (event->state != PERF_EVENT_STATE_INACTIVE)
		return 0;

	/*
	 * The event is inactive, if the context is active
	 * we're part of a group that didn't make it on the 'pmu',
	 * not counting.
	 */
	if (event->ctx->is_active)
		return 0;

	/*
	 * We're inactive and the context is too, this means the
	 * task is scheduled out, we're counting events that happen
	 * to us, like migration events.
	 */
	return 1;
}

static int perf_swevent_match(struct perf_event *event,
				enum perf_type_id type,
				u32 event_id, struct pt_regs *regs)
{
	if (!perf_swevent_is_counting(event))
		return 0;

	if (event->attr.type != type)
		return 0;
	if (event->attr.config != event_id)
		return 0;

	if (regs) {
		if (event->attr.exclude_user && user_mode(regs))
			return 0;

		if (event->attr.exclude_kernel && !user_mode(regs))
			return 0;
	}

	return 1;
}

static void perf_swevent_ctx_event(struct perf_event_context *ctx,
				     enum perf_type_id type,
				     u32 event_id, u64 nr, int nmi,
				     struct perf_sample_data *data,
				     struct pt_regs *regs)
{
	struct perf_event *event;

	if (system_state != SYSTEM_RUNNING || list_empty(&ctx->event_list))
		return;

	rcu_read_lock();
	list_for_each_entry_rcu(event, &ctx->event_list, event_entry) {
		if (perf_swevent_match(event, type, event_id, regs))
			perf_swevent_add(event, nr, nmi, data, regs);
	}
	rcu_read_unlock();
}

static int *perf_swevent_recursion_context(struct perf_cpu_context *cpuctx)
{
	if (in_nmi())
		return &cpuctx->recursion[3];

	if (in_irq())
		return &cpuctx->recursion[2];

	if (in_softirq())
		return &cpuctx->recursion[1];

	return &cpuctx->recursion[0];
}

static void do_perf_sw_event(enum perf_type_id type, u32 event_id,
				    u64 nr, int nmi,
				    struct perf_sample_data *data,
				    struct pt_regs *regs)
{
	struct perf_cpu_context *cpuctx = &get_cpu_var(perf_cpu_context);
	int *recursion = perf_swevent_recursion_context(cpuctx);
	struct perf_event_context *ctx;

	if (*recursion)
		goto out;

	(*recursion)++;
	barrier();

	perf_swevent_ctx_event(&cpuctx->ctx, type, event_id,
				 nr, nmi, data, regs);
	rcu_read_lock();
	/*
	 * doesn't really matter which of the child contexts the
	 * events ends up in.
	 */
	ctx = rcu_dereference(current->perf_event_ctxp);
	if (ctx)
		perf_swevent_ctx_event(ctx, type, event_id, nr, nmi, data, regs);
	rcu_read_unlock();

	barrier();
	(*recursion)--;

out:
	put_cpu_var(perf_cpu_context);
}

void __perf_sw_event(u32 event_id, u64 nr, int nmi,
			    struct pt_regs *regs, u64 addr)
{
	struct perf_sample_data data = {
		.addr = addr,
	};

	do_perf_sw_event(PERF_TYPE_SOFTWARE, event_id, nr, nmi,
				&data, regs);
}

static void perf_swevent_read(struct perf_event *event)
{
}

static int perf_swevent_enable(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;

	if (hwc->sample_period) {
		hwc->last_period = hwc->sample_period;
		perf_swevent_set_period(event);
	}
	return 0;
}

static void perf_swevent_disable(struct perf_event *event)
{
}

static const struct pmu perf_ops_generic = {
	.enable		= perf_swevent_enable,
	.disable	= perf_swevent_disable,
	.read		= perf_swevent_read,
	.unthrottle	= perf_swevent_unthrottle,
};

/*
 * hrtimer based swevent callback
 */

static enum hrtimer_restart perf_swevent_hrtimer(struct hrtimer *hrtimer)
{
	enum hrtimer_restart ret = HRTIMER_RESTART;
	struct perf_sample_data data;
	struct pt_regs *regs;
	struct perf_event *event;
	u64 period;

	event	= container_of(hrtimer, struct perf_event, hw.hrtimer);
	event->pmu->read(event);

	data.addr = 0;
	data.period = event->hw.last_period;
	regs = get_irq_regs();
	/*
	 * In case we exclude kernel IPs or are somehow not in interrupt
	 * context, provide the next best thing, the user IP.
	 */
	if ((event->attr.exclude_kernel || !regs) &&
			!event->attr.exclude_user)
		regs = task_pt_regs(current);

	if (regs) {
		if (!(event->attr.exclude_idle && current->pid == 0))
			if (perf_event_overflow(event, 0, &data, regs))
				ret = HRTIMER_NORESTART;
	}

	period = max_t(u64, 10000, event->hw.sample_period);
	hrtimer_forward_now(hrtimer, ns_to_ktime(period));

	return ret;
}

static void perf_swevent_start_hrtimer(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;

	hrtimer_init(&hwc->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hwc->hrtimer.function = perf_swevent_hrtimer;
	if (hwc->sample_period) {
		u64 period;

		if (hwc->remaining) {
			if (hwc->remaining < 0)
				period = 10000;
			else
				period = hwc->remaining;
			hwc->remaining = 0;
		} else {
			period = max_t(u64, 10000, hwc->sample_period);
		}
		__hrtimer_start_range_ns(&hwc->hrtimer,
				ns_to_ktime(period), 0,
				HRTIMER_MODE_REL, 0);
	}
}

static void perf_swevent_cancel_hrtimer(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;

	if (hwc->sample_period) {
		ktime_t remaining = hrtimer_get_remaining(&hwc->hrtimer);
		hwc->remaining = ktime_to_ns(remaining);

		hrtimer_cancel(&hwc->hrtimer);
	}
}

/*
 * Software event: cpu wall time clock
 */

static void cpu_clock_perf_event_update(struct perf_event *event)
{
	int cpu = raw_smp_processor_id();
	s64 prev;
	u64 now;

	now = cpu_clock(cpu);
	prev = atomic64_read(&event->hw.prev_count);
	atomic64_set(&event->hw.prev_count, now);
	atomic64_add(now - prev, &event->count);
}

static int cpu_clock_perf_event_enable(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	int cpu = raw_smp_processor_id();

	atomic64_set(&hwc->prev_count, cpu_clock(cpu));
	perf_swevent_start_hrtimer(event);

	return 0;
}

static void cpu_clock_perf_event_disable(struct perf_event *event)
{
	perf_swevent_cancel_hrtimer(event);
	cpu_clock_perf_event_update(event);
}

static void cpu_clock_perf_event_read(struct perf_event *event)
{
	cpu_clock_perf_event_update(event);
}

static const struct pmu perf_ops_cpu_clock = {
	.enable		= cpu_clock_perf_event_enable,
	.disable	= cpu_clock_perf_event_disable,
	.read		= cpu_clock_perf_event_read,
};

/*
 * Software event: task time clock
 */

static void task_clock_perf_event_update(struct perf_event *event, u64 now)
{
	u64 prev;
	s64 delta;

	prev = atomic64_xchg(&event->hw.prev_count, now);
	delta = now - prev;
	atomic64_add(delta, &event->count);
}

static int task_clock_perf_event_enable(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	u64 now;

	now = event->ctx->time;

	atomic64_set(&hwc->prev_count, now);

	perf_swevent_start_hrtimer(event);

	return 0;
}

static void task_clock_perf_event_disable(struct perf_event *event)
{
	perf_swevent_cancel_hrtimer(event);
	task_clock_perf_event_update(event, event->ctx->time);

}

static void task_clock_perf_event_read(struct perf_event *event)
{
	u64 time;

	if (!in_nmi()) {
		update_context_time(event->ctx);
		time = event->ctx->time;
	} else {
		u64 now = perf_clock();
		u64 delta = now - event->ctx->timestamp;
		time = event->ctx->time + delta;
	}

	task_clock_perf_event_update(event, time);
}

static const struct pmu perf_ops_task_clock = {
	.enable		= task_clock_perf_event_enable,
	.disable	= task_clock_perf_event_disable,
	.read		= task_clock_perf_event_read,
};

#ifdef CONFIG_EVENT_PROFILE
void perf_tp_event(int event_id, u64 addr, u64 count, void *record,
			  int entry_size)
{
	struct perf_raw_record raw = {
		.size = entry_size,
		.data = record,
	};

	struct perf_sample_data data = {
		.addr = addr,
		.raw = &raw,
	};

	struct pt_regs *regs = get_irq_regs();

	if (!regs)
		regs = task_pt_regs(current);

	do_perf_sw_event(PERF_TYPE_TRACEPOINT, event_id, count, 1,
				&data, regs);
}
EXPORT_SYMBOL_GPL(perf_tp_event);

extern int ftrace_profile_enable(int);
extern void ftrace_profile_disable(int);

static void tp_perf_event_destroy(struct perf_event *event)
{
	ftrace_profile_disable(event->attr.config);
}

static const struct pmu *tp_perf_event_init(struct perf_event *event)
{
	/*
	 * Raw tracepoint data is a severe data leak, only allow root to
	 * have these.
	 */
	if ((event->attr.sample_type & PERF_SAMPLE_RAW) &&
			perf_paranoid_tracepoint_raw() &&
			!capable(CAP_SYS_ADMIN))
		return ERR_PTR(-EPERM);

	if (ftrace_profile_enable(event->attr.config))
		return NULL;

	event->destroy = tp_perf_event_destroy;

	return &perf_ops_generic;
}
#else
static const struct pmu *tp_perf_event_init(struct perf_event *event)
{
	return NULL;
}
#endif

atomic_t perf_swevent_enabled[PERF_COUNT_SW_MAX];

static void sw_perf_event_destroy(struct perf_event *event)
{
	u64 event_id = event->attr.config;

	WARN_ON(event->parent);

	atomic_dec(&perf_swevent_enabled[event_id]);
}

static const struct pmu *sw_perf_event_init(struct perf_event *event)
{
	const struct pmu *pmu = NULL;
	u64 event_id = event->attr.config;

	/*
	 * Software events (currently) can't in general distinguish
	 * between user, kernel and hypervisor events.
	 * However, context switches and cpu migrations are considered
	 * to be kernel events, and page faults are never hypervisor
	 * events.
	 */
	switch (event_id) {
	case PERF_COUNT_SW_CPU_CLOCK:
		pmu = &perf_ops_cpu_clock;

		break;
	case PERF_COUNT_SW_TASK_CLOCK:
		/*
		 * If the user instantiates this as a per-cpu event,
		 * use the cpu_clock event instead.
		 */
		if (event->ctx->task)
			pmu = &perf_ops_task_clock;
		else
			pmu = &perf_ops_cpu_clock;

		break;
	case PERF_COUNT_SW_PAGE_FAULTS:
	case PERF_COUNT_SW_PAGE_FAULTS_MIN:
	case PERF_COUNT_SW_PAGE_FAULTS_MAJ:
	case PERF_COUNT_SW_CONTEXT_SWITCHES:
	case PERF_COUNT_SW_CPU_MIGRATIONS:
		if (!event->parent) {
			atomic_inc(&perf_swevent_enabled[event_id]);
			event->destroy = sw_perf_event_destroy;
		}
		pmu = &perf_ops_generic;
		break;
	}

	return pmu;
}

/*
 * Allocate and initialize a event structure
 */
static struct perf_event *
perf_event_alloc(struct perf_event_attr *attr,
		   int cpu,
		   struct perf_event_context *ctx,
		   struct perf_event *group_leader,
		   struct perf_event *parent_event,
		   gfp_t gfpflags)
{
	const struct pmu *pmu;
	struct perf_event *event;
	struct hw_perf_event *hwc;
	long err;

	event = kzalloc(sizeof(*event), gfpflags);
	if (!event)
		return ERR_PTR(-ENOMEM);

	/*
	 * Single events are their own group leaders, with an
	 * empty sibling list:
	 */
	if (!group_leader)
		group_leader = event;

	mutex_init(&event->child_mutex);
	INIT_LIST_HEAD(&event->child_list);

	INIT_LIST_HEAD(&event->group_entry);
	INIT_LIST_HEAD(&event->event_entry);
	INIT_LIST_HEAD(&event->sibling_list);
	init_waitqueue_head(&event->waitq);

	mutex_init(&event->mmap_mutex);

	event->cpu		= cpu;
	event->attr		= *attr;
	event->group_leader	= group_leader;
	event->pmu		= NULL;
	event->ctx		= ctx;
	event->oncpu		= -1;

	event->parent		= parent_event;

	event->ns		= get_pid_ns(current->nsproxy->pid_ns);
	event->id		= atomic64_inc_return(&perf_event_id);

	event->state		= PERF_EVENT_STATE_INACTIVE;

	if (attr->disabled)
		event->state = PERF_EVENT_STATE_OFF;

	pmu = NULL;

	hwc = &event->hw;
	hwc->sample_period = attr->sample_period;
	if (attr->freq && attr->sample_freq)
		hwc->sample_period = 1;
	hwc->last_period = hwc->sample_period;

	atomic64_set(&hwc->period_left, hwc->sample_period);

	/*
	 * we currently do not support PERF_FORMAT_GROUP on inherited events
	 */
	if (attr->inherit && (attr->read_format & PERF_FORMAT_GROUP))
		goto done;

	switch (attr->type) {
	case PERF_TYPE_RAW:
	case PERF_TYPE_HARDWARE:
	case PERF_TYPE_HW_CACHE:
		pmu = hw_perf_event_init(event);
		break;

	case PERF_TYPE_SOFTWARE:
		pmu = sw_perf_event_init(event);
		break;

	case PERF_TYPE_TRACEPOINT:
		pmu = tp_perf_event_init(event);
		break;

	default:
		break;
	}
done:
	err = 0;
	if (!pmu)
		err = -EINVAL;
	else if (IS_ERR(pmu))
		err = PTR_ERR(pmu);

	if (err) {
		if (event->ns)
			put_pid_ns(event->ns);
		kfree(event);
		return ERR_PTR(err);
	}

	event->pmu = pmu;

	if (!event->parent) {
		atomic_inc(&nr_events);
		if (event->attr.mmap)
			atomic_inc(&nr_mmap_events);
		if (event->attr.comm)
			atomic_inc(&nr_comm_events);
		if (event->attr.task)
			atomic_inc(&nr_task_events);
	}

	return event;
}

static int perf_copy_attr(struct perf_event_attr __user *uattr,
			  struct perf_event_attr *attr)
{
	u32 size;
	int ret;

	if (!access_ok(VERIFY_WRITE, uattr, PERF_ATTR_SIZE_VER0))
		return -EFAULT;

	/*
	 * zero the full structure, so that a short copy will be nice.
	 */
	memset(attr, 0, sizeof(*attr));

	ret = get_user(size, &uattr->size);
	if (ret)
		return ret;

	if (size > PAGE_SIZE)	/* silly large */
		goto err_size;

	if (!size)		/* abi compat */
		size = PERF_ATTR_SIZE_VER0;

	if (size < PERF_ATTR_SIZE_VER0)
		goto err_size;

	/*
	 * If we're handed a bigger struct than we know of,
	 * ensure all the unknown bits are 0 - i.e. new
	 * user-space does not rely on any kernel feature
	 * extensions we dont know about yet.
	 */
	if (size > sizeof(*attr)) {
		unsigned char __user *addr;
		unsigned char __user *end;
		unsigned char val;

		addr = (void __user *)uattr + sizeof(*attr);
		end  = (void __user *)uattr + size;

		for (; addr < end; addr++) {
			ret = get_user(val, addr);
			if (ret)
				return ret;
			if (val)
				goto err_size;
		}
		size = sizeof(*attr);
	}

	ret = copy_from_user(attr, uattr, size);
	if (ret)
		return -EFAULT;

	/*
	 * If the type exists, the corresponding creation will verify
	 * the attr->config.
	 */
	if (attr->type >= PERF_TYPE_MAX)
		return -EINVAL;

	if (attr->__reserved_1 || attr->__reserved_2 || attr->__reserved_3)
		return -EINVAL;

	if (attr->sample_type & ~(PERF_SAMPLE_MAX-1))
		return -EINVAL;

	if (attr->read_format & ~(PERF_FORMAT_MAX-1))
		return -EINVAL;

out:
	return ret;

err_size:
	put_user(sizeof(*attr), &uattr->size);
	ret = -E2BIG;
	goto out;
}

int perf_event_set_output(struct perf_event *event, int output_fd)
{
	struct perf_event *output_event = NULL;
	struct file *output_file = NULL;
	struct perf_event *old_output;
	int fput_needed = 0;
	int ret = -EINVAL;

	if (!output_fd)
		goto set;

	output_file = fget_light(output_fd, &fput_needed);
	if (!output_file)
		return -EBADF;

	if (output_file->f_op != &perf_fops)
		goto out;

	output_event = output_file->private_data;

	/* Don't chain output fds */
	if (output_event->output)
		goto out;

	/* Don't set an output fd when we already have an output channel */
	if (event->data)
		goto out;

	atomic_long_inc(&output_file->f_count);

set:
	mutex_lock(&event->mmap_mutex);
	old_output = event->output;
	rcu_assign_pointer(event->output, output_event);
	mutex_unlock(&event->mmap_mutex);

	if (old_output) {
		/*
		 * we need to make sure no existing perf_output_*()
		 * is still referencing this event.
		 */
		synchronize_rcu();
		fput(old_output->filp);
	}

	ret = 0;
out:
	fput_light(output_file, fput_needed);
	return ret;
}

/**
 * sys_perf_event_open - open a performance event, associate it to a task/cpu
 *
 * @attr_uptr:	event_id type e:
 ibutes for monitoring/samplinge:
 *pid:		target pid>
 * cpupyright (Cre cod @group_fopyrgo Mo leader  Hat, f2008/
SYSCALL_DEFINE5(perf_ Hat, open,
		struct Peter Zijlse:
  __user *,Thoma Copy<pzipid_t,C) 2, i/*
 cpu IBM Co*  Colna, unsigned long, flags)
{
om>
tr@redhat.com> * Hat,, *s@au1.Red Ha;g details see kerne *  CCe:
 nclude <linux/fs.h>
#context *ctxlinux/mm.hfilel-base/_mm.h>= NULL>
#ilinux/mm.h>
s@au1.i <linux/file.h 200put_needed = 0olllude inux/file.2h>
#isysfs.err;

	/*xner future expandability...2009	if (licen & ~(PERF_FLAG_FD_NO_GROUP | s.h>
#includeOUTPUT))
		return -EINVALude err =s see copy *  C(� h>
#9 Pa &e:
 )/sysf (errvmallocludehelude s.h>!e:
 .exinux/_s.h>
l) {
	cludeedhatparanoidinux/dc() && !capable(CAP_SYS_ADMIN/ude <<linux/#iACCES;
	}escludeinux/freqallclude iinux/ronixe_nt.h > on_itl_edhat.com>
m/irq_rrateel_statsfs.h>
linux/m.h>
#i*
	 * Get the d Hat, I.h>
#ic(ts c ors secpu):PER_/
	ctx = find_ge/mm.h>
#i(as, Iu_coPU evenIS_ERR(ctxnel_stlinux/PTRtatic intfs.h>
textCLook upjlstrncluar
 d Hat,we wilnux/mach this (ras,0to itlinux/);
YING
 */

#i.h>
#ipt.h>
#is@au1.ib != -1des.hd_motrace <linux/file.h>
nt ps)>

#inly;
shnts
 * /
D nr_evex/mm.h>fax_eld Har CPic erf_&h>
#includetly clude ts __read_ per goto/uac_inuxcpmostnostly;

mostly;

/->f_opomic&edhatfopsde:
 Pete ght ( e <linua lght (ostlyt accesea*  -1 - not private_dataostld_moostlDo  1 -erf_w a recursive hierarchy (ly = new sibx.de>ixnerbecoming rawt of another dis
 -r unprilinumic_t ly;
c atomi_overc-> accesixner u!ead_v
 *_overc   0 - disallow raw tracepoinnt acparanid_trace   sallentsstpointt perfin a differentrace/
xt, - diCPU_ranoia tatiilin1;
static iinline bo
int!= pu _aw(voidsinglloc.h>

/*
 * Peterght (Onlyaranoid >_overcocan bd_moclu
 *  eterinnedsctl_perf_eveinux/file.
 *  ||ux/mm.d > 1;_paranoid > ol - di_e <linuxly;
x/sysc/hardir#includrf_r(rculiCorp. <ctxlus@au1._overcaul 		 t (Cnux/, GFP_KERNELctl_ic_t n
}

perc Hat,ctl_perf_c atom_event_par- dis
 *   raw eventpoinic_t nanon_inodemax_fd("[edhat.com>]",d aiv
 *
 *, 1;

s, 0sPU eventcl < 0ht (C) ;

/*
fre
#incLockvent_(sys/smpsfs.h>
#t;

static erux/rfs.h>
#incluctl_perf!e providedesource_lock)_lock) * Architecturread_moeventsfs.h>
#includely;
s atnoid_t nEmostCPU haset_output'free'/
int syibmostly;

linux/mmu *hw_perfK(pef_event_init(struc yright/

->filp = 1;

sy;

/;
	WARN_OsetuCEpara->pa <li_pustly	mutexhw_pe(&u_co	
nline; }edhatinstall_inoint acceid_cpinline	statiy ++ hw_pgeneration; }erf_e un__weak hwid_kernel(vent
erf_ownr unpcur <li;
	ly;
xt, _detail(> -1    *groschedn(structruct ->edhat.com>
_kernel(vlist_add_e <l(&go Molyright_entry, jlstr@rd_kernel(voia , it *gro,
	   in(jlstrk perf_event_print_nux/cpu.
id_kernnincluer(); 	{ bar:
	e_coustrucalct perf_e,sfs.h>
#incluf_d;

blec_t x,
	    PeterdLOCK Peterrt pmkcounf_event_pable_)++;
}

 /* '

bool count);

w rawtxrp. r unpable_c;

vomostly;

/returvar Petelow *urn NULul pe}

/2008-inherit a 1;

st0romnoid_event, pto child (__p:- <lic atomiude <linux/fs.h>
l-
chedperaxet_c(ble();
}

static vo__perriinline ht (Cdisabl);
Peterranoianux/cpu.nt_setN(!;
}

vos __read_mostly;
scpxt *ctxer(refcount));
}

sinc_not_zero(countrefd)
{t))_cpu_c atominlinOPnr_events __context *ctx;

	ctx = cont ;

/_ctxevent_ rcu)ysctble();
}

static vo);
	kfe booly;

/statiInsteaddisacreat/*
 _id;

/*
 *atic v iesdisae boosnt_s* w/
DEFktext,peredctx->re bacPeterlstroriginale booifntext *hichw_pe a/mm.pcludesure,	<linucweer() asut_ctre' kb cetext d)
{ol _ux/mm.h>
#/cpu.he boo) __perrtext ctx);
	}
}
	   );
	 <lix;

	ctx = 
	hwk);
xstaticvent_imtx(strisallow k perf_event_pronixkerneht (k perf_event_p t of
_ctx(reerf_ruct peint_paranoid_
	

in)	{ ren
}

/*
 * t ofstly;

/moatomic64_t truct
{
	if ude <linux/*ctx)
{
	if{
		putroup_NULL;
	}(t perf_evenMakesk);
count)_disa foad_m-1heilinuevofe_lo	rent_c e boofcounid_tritskd perdisK(ped bit.  We hosabl we wat)
's id)	{ oint aoct(cwon't racit _thent_context,{en,id;
}K(pe_family._head,, rcu_hea perf_evenif (ev>=*ght (EVENT_STATE_INACTIVE_pare evyrigspait is loked,nt_itrucint, percould gf_evlset movede eveent_pants c.id_cpc atomijlstrOFF perf_ev		inclo usents we waPU entext *
perf_lochwas a lisperiorn Nk perf_event_pPER_CPU(int, pert perf_eveny
k_stt atoie pe{
	u64 'sl perf_paraic_t orp.structtot (C) statifree(; }4 primard_kernel(v *PU(s2 - clinercask, unsrent)
	sk)
	-vent_r unisab ihat un	iets c->Peter 1;

stexits. Tilinis saf	ER_Cd 0;
}auint,xt *c act p		 * prent)
	and forkn>ce_lastatic for stight
righxisent-om ax->t-conerf_rent_cuth the ;
}

vo* Fo_inlls.sk, unsigned  forw rack ife parent cu_dereftl_pein * t (Cswappene booght ount)debu;nt)
	(_ON(etup(If wevents we watnot_
static s contexx,
	       s swapped on us _ctx(nux/cpu.hdebutctx) {
		rxt *
perf_locck, *fdebuns:
save(&contew_pek->pdebug;

void{ent_c atoqt_ctxp)) {
			spin_icens;
	t perf_edic u64 primar}_inc_nenaludede lo_ct*  CoDEFINE_PER_CPU(int, perf_d
	}
}
_head *head)
{
	struct perf_ep)) {
 struct perf_event_context, rcu_head primaryf it
		 *h thtx->re
		ctx->parent__unlock_irqres->pa
int pint,,

/*
 * Gt_ctx(ct pthat u_event_context,punts __linux/mm.h>
#include *subalso incremnt acit
 * xher ta lude s for unp&ctx->ren ctx;oint acmit an on us t its prcerf_eveE_PEt)
	,ronixe_the contclonsallow id *  oint_rclone reser 100000;k)
{
	s
		if (cfor_each,
	   (subsk and txp)alls.	t_parand_kernes@au1.
	   LLd totatic stret, rcudcontext(sgs;

task and incremenfree' ik and_ts c_coid perf ctx;
}
>lock_struct *tasoved tcttext fct perf_event_conr task.e.
		 *y;
		}

0{
	struct petext,sync_oved to anoDEFINE_PER_CPU(int, oved to ano and count)the context for a task andwapped to another task.  untasketurn ctxt *
perf_locU(int, pperfwapped val a clonext *
perf_loc* Fo &ctx->renc_noext dhat.com>
rea->paspin
			spi	gotowappedretry fromstatmin-got s64
voidu_dere_parac) prk,from stati
		 Addesto			* perf_eght (ue(&ctxPU(strucinc_nok if itt pe* dd_evenctx ) {
			vapoint x = perf_loc	event, erf (evk th>eturn 0;}

/*
 *otal_time_enloc.h

	/N_ON(flags);
			goto ret		oror ux.deher it,
on wheock_tther ta strcpu;
	p text's evenrunningaderadd grostrad Hattostatic struc'sher it:PER_/lone of anotRemove pe_perf_evenlinect perf_event,statneatleaderca tas locked the on us any more.leade/pzijpinhw_pe_ir->lock, *flags);
			goto ret_strucdene(iituct perf_event * * perf_e		grourestoreader->nr_siblings++;
	}
	perf strucgo Molctx,eastask->the pe	x(st
stati->is wt_en= 1;at_add_it md Haleaderititstlyf_di anop_entry, &gro for)estore(com>
 *
 
_ __weak hw_pek_s_event);
		group_leader->nr_sibl	goto ret-to th ist)led w renctaticvent->groupntt is a clonnt;and ct_un;
		grouent_list);
	ctx->nr_	goto retk, untatiupderf_f_loc/*imesic voido anocN_ON(etucontemud __w_nlin(ctx !=	{ y/vmalloc.h>;
ed_i);
	put_ctx(apped to vent_indd a xt *ctx)
tnoid_happleadeam thcontenk_sc fit *ctatic strrent_ceed. 
	lisck()ng fr around dueader et nt, pent from thed_ecueed. f_eventux/mree'be zid _d - but nt_pawisalls.g per for itaretuhat until gs);
	.
 *	gotox and ct {
			s heltx->pin_co		led wngs== esingleton k and omic_roupWct pasable_co))
rcu(&e, f	/*
vent_o thatthe erf_f_k *
phe overs.	hw_p *
 *exent, )) {
			ston events b*ctx)
{
	struct perf_event *sibling, *tmp;y) {

		list_*tnix.dux/mmevent)
{
	return NULcpu't get sa(evems
 *aticFo x/ptr a clonelikelyt));ict(&event->oia  = pomic_omwsibl>nr_evton e->lock,nt, pe_SPIN perf_e
		 */
localgrou_t_ctf_evenby erf_evenWenoidt_eneschedule hereeaderrcuerf_errupk threem
	list,ader*k andeioup rnt, peis	truct plinuitr itnt accer it contexb_sibliuld g)
	d,ux/cner PERt pehouglong )_ould g)
/*
 changranoid >ourreturn ssibling tx = conter CP--;		 weak perf_eveck, voidup_entry)event)ld gisab If thia clone of anotTr itsk->p	}
}

sn(stlloc.hstwaratt++;tmp,ble)oint acc i<linuevoidFF <li>pmu->sable_c(er it),ent->apara
	 *d;

hant->atce count)	{ cp->nr_eact'sgo Mtx->rebefore fordo c_t }

 beloext *ent-list      st't get s->n(st != allow|| !atic void
giv also incrt *ctx)
t_cpu_/n it
	er iy:
	;
	p; un ed,
d;

s lis perndigethgrouswly = w_parructparproperf}

	ltwar'r
	}
mov/*
 a	++ctxunt sf_eventnlineis_event_i->paifent_ss_software_ev<pzijl}

sta
#incnt_lis_event anoia nux/r(strucs+;
}
 * .oid por ctx;
}
sk dntryafterkernntextOxclup-;
	if, cp; pin_->w_sibli_ents at, p_lea a lis.inherys.h>
RECORD_EXIT.the con howevtup_p_le(grou (gro fewe <lallox->ncREADt->t's evct perf_ctx)tic v<pzi>disable(ludeperf_-;
	i=>staF_ the conit
	atoe ot_entrsameive_on008 TthrPERF retit->ts  event->oncpu  {

		list)int, pert.
 *e siblings toerf_disave ndernentry(cont Must be cnux/;

voder o Molr per_sentry(text *cr_eax,
	       stt perf_evetatic strBut s_tail(&evct perf_evware_eventount)	ifb		cpuct we
p_onyrighance evx,
	      _nestetware_evestruct per, SINGLE_DEPTH_NESTINGhe hagain: itswith ctx->ent_rem_ PER	 * to the cevtmp(strc_not_zernIf we it *ct *ctxnt_en, &	got
	ight (->onrp.  {

		list singleton events nevenThomext(strirqrestore(stry,e afe(sibhed_ranoid >nrer CP+ock_tunhaid *try)turnoutrossor_evt_paran tmp,_listt *ct a gwe obtaiched'tmp'on aext ftic vit
	{
	__pelearn 0;
polude
	kfngvent
 hntry(ermine_ctx(_lisitPeter  sibling even!if (cemptyt_for_each_enVE)
	d outght (C) ;
aticu* thrqrestore(&ctxnpineach_enf (event->f_eventvot is a clon list dirled  an unexpos
	}
unusedware_evenasqrestotr.ex		++ctx->
#i byroup_le
		lis *gain,cpucs __rfork()e thct_stof fa_reag_, in,puctx =tr.ex)led went-t_in_lu)
{
	rt's eve-);
	wapped to another task;
	}
}

sta.h =();
	|| !cpuctx->activ;
	ble();
}

static vox/smpCOPis a ux/per#_percpuint, perctx,
	       stt perf_event per r
	 *turn ic structoftly = c  1 -itctx->be CTIVE)
			 *  t the smp calo retent *sibling, *tmp;

	if struct p and incremeher i	 * can't get!ctx = crcpu _x->inur (sytlist);
		group_lear_siblings++;
	}

vent-nt cpu_for->ght (Coid __wp)) {
erf_event *sibling, *ts++;
	if (event->t pe_entry(.erf_cpualli * CTy = iand ct		list_mc.h>
ctdout(evem
	tatic struct(evehat un it
	!it_tt->grouleaderA *   ader_evewe onltic strusiblre= evenleaderperf_erf_gring_atic voimaInitializr CP)
	nt so thatct perf_ev/*
 * Remove	hw_pludeext *cpuctxe--;
	if );
}


/*
 * Remove tis a sted from perf_eturn 0out(evext for a taskn ctx;
}

/*oup_leader = sibling;
	}
}

staed,
 cont
evcalled with ctx->mutex hllfrom_safes);
			ctx = NULL;T_STll_funstrtx = NULL;
	disal_perf_gr_sirep_ca0ctx = conpzijlstr@r a task and incrn detachs OK w&er i beforeand increment iINIT_LIST_He a it_st	}

stati:
	taom perflone of anotevp-poinlunctioble(event)pude <linux/ent->g->activ_dis, texecu>nre();
 be else {
();
	s void
g arrif (ctx-t's eves--;
cpuct/
DEsbeen markthe orxt,
	ing_PER_CFved.oid >c (evatice--;e_ voi_aontext,
	_empint, perf_PER_CInteximetivevenkmdetacheizeof for a task and inaw trace)e_rn the parent ak hw
	 *get swar CPU eventNOMEMock_event->oncpu e--;
->actioup_evenkx = omicit_t
 * We diext for a task antx = cont
ev	spin_lock_ict pe
	if (!.inherit_stand cone of anotclone of anot = 0rasnagained_ouask, it'gomicrdware leustrururm
	 * Ifctx).
		 *	       pty(Peterint accestx = c* thrdware No>
#in_STAcheck>nr_p_IVE)
		or!linux/lloc.;t.  Iffe = nwhe custt an-e recoarliperf	/*
	ine reason_perf	 * IfIf weeronix_time(s>nr_wd_moien calldallocon* Thc , Inc->tasentrymiddl
	pentext 00000ime nt->stng += ner->n:
		 */
we
 u64tati, __p = rcu_d;and incrent)
	task. events weim */
smarrived.g-id_trPIa petshashed yagaindid_trail(&ev
t(evnobodynoid_acx);
	 ctx);

	/list);
		group_leadt peask_contextrdware levdonext vents ere lev NMIoup_othid)
/cpu.lookng evaline u64 task.- dismanipuparent_ip the f (nt task context of a;

voidrun_ent->paifcontructIisallow->	 */up
	 stly;

/ top-leveruct perf_gs);
		to tot
	 * scpu,nux/amp_textonly	try:
ock()
		++ctx->ntext  for a task and incremenrestoref (grole_cvent,of anoo Whenx;
}redware level text could gx);
	ubreaker it  callexitT_STATE_INACTgs);
	rf_eventMartx->tetry:
t--;
	if (-text ever it it the peoid_cpstatictheor	perw the= 0;tllowwe nee);
}

stat of.oid_cpNottruct 	et_cp anotyright contextProtcu(&ldtn th!sk =ask, itdcoune u6_ON(caderevun_endoesp_rumatteroid_cpint * Thon aetry:= NULL;
	etry:cpu _ perf_egrptx->tgr

	samp_rur_act	}
	ecpu_}

	/*
 tmic6stampif (ctx-f_perfrtassmp ca =

/*
dontefrom tstruct tup_leader->sibling_l_evenent_disabware leesprrie= evntext.
 */
_ivent_disabll ach_entry(
 *
 * W =gennt its prctomic_t ty(&eveof anoct por_each_entry(ext);
	str &__geause the
 Pnc_not_zert_context *ctx ontext,
					 et perf_euctx or syall eves_softwarey more.
		 */
		ry:
rf_event *sibling, *t

	if (event->swith un= in UCPU cvent->rf_event_cf_reserre
{
	struct pe *
 *perf_n_unl

retry:
	task_ocpu(ludent(ectx);
}

static rp. 
	}
}

stapuent, 
	evenroups:
 *n even PERFtextlist.
*grou (ctntext, abn_lodid  1 _STATs&ent->sroup_o checcord mpty(&events is a  pmu *      (!li		y(&etmaxchec);
	s       rttinot
	 * is is a rf_eveup_tip Pergs (s froy)erf_event  =ther it is etrytex and ctx->can'ty
		 *}s __fdef CONFIG_HOTPLUG_CPUruct s t_detaskivctx-ibling	retPERF *
 **infoe,pyrivmance_EVENT_STATE_INA* Disabllistive--;
f_disant->tstamp_runnind_outvoidents c's (ncpuak h's)_actt d(st_leadee_gontext,edd from>nr_evenexton event
 * t *lonl>
#inr(perw_addent-d = ex->lovent->time - event->tstamp_enal roce
/*
 * Disnt af (cerf_event ->tatic st	updax->lock);	list_deDable_cnit(v_STATE_IIf event->ctx is a cloned context, call >hardware level _running;
}

 {
 = e	}
	retu thet->tstamp_end = evperf_post's yved with a smp call. For tatsmp_cnoidfuncs th_* to tare ,;

	list_det's child_mo check 1ck_irqrestore(&ctxcts task.
 */}
#ct peruct perfnoidto			 evenetry:'s_enabl_ns va,*eve { callndifessful.
		 */rneat off
_EVENT_STnotify this
	 

	spier_b * re*selfmkernesched_out(athrot,lock);*h so any N_ON(TIVE_STATE_weak_out)*tasmes(ewitch ( we onle_cd;__pe	{
	rUP_PREPARE:k_ctxtaticpu from it'_FROZEN
		 try;
	oup_the _EVENT}

int *_lock(& on
		 */
	ONLIN'->lo>siblingout(evext 					 _arrived.ATE_I oid)
_oor th a task andsable_c<pzi	evenDOWNmp_call_1);
		returnust beand ct retry:
	taT_STATE_INA child_mu, __perf_event_didefaul
/*
 _lock(&cnt_salinux/NOTIFY_OKerf_event_re(eveding_ohr_actt higT_STATiorityt's nnfo) is th;

	spier	/*
 arri.cable()_disable_co);
and increment i_event *ew cpctx is a cnb =

#i.and incre top		*
 * maqntext for,
	.)
{
	strspin20,
};
);
	sp_text_.
	 * If it is i cur.k = ctx- context for  perf_n a CP, (tx->task;_dis)*/
		smp_call_ ut(sATE_I
 )sk * W
	 *
}

/*
__ged()sk, _ndinould possibne u6	/*
vent, c;
	}
k_oncpu_funcout(evhe contextt_schon th
{
	struct perf= NULLregist blockw_perf_		ic int
event_point to
 * rs_eve_cpucPERshowxit_tasve weouldthis
	 sysdev_classia aass */
ar *buf any  */
	ifspr	uns(buf, "%d\n", * Westate <== PERF_E We dis (ctx != rcue(&ct)
		putunct<block in sync_childOFFvmalloc.h>
0cpu ableds	updatncpu_t's ctx->rif totaoIf event->ctx is a cloned context,  (RN_ON(	event->(ent-l/dcache.le ratempectureadmme(sric_cpurtoulchildA10, &valSPINLOCK(peux/mm.h>
#iisabls sateed >able(even_event ude <linux/s also incr_sched_in(t(event, cpuctx, ctx);oevent->oncpu = cpu;	 =ad_lo{
m>
 *
 * llnt_edatef_eveo reti me_et __r b{

		if_panc_atic v we tstateWct peatic_perff_eved possib. it is a	mpmp_smin{
	retump_runninre cmust be visnr_event nt(str-->tstamp_runnin*
 * Wet->oncpu = cpu;	 != Prn 0;
}

ent_cic intsction_->de <lched_oil(&f_evest be visore(&ctx}etry:
nable(evenct perf_cpu_context *ccpu n thl to* TODO: put 'cpu' i because tsovercommist);
		grew state must be visre we turnfunction_c in sync_childould g	 * Thetnew state /* TODO: able'cpu'O: pof_cpu_cnew state must be viscpu)
 = call. Fogrout(strt *lc.h>
 n>attent-d of theu->count);_sched becau we tup_event, cpuctx, could g_in(struct 
	 */
ic_tisibloc.h>
#iAGAINruct pe we t1tal_tnce event
- perf_peclusivesto theask, it'sis_soft the_enew state atic voidup.
 */t(ev*;
}
ialtic intive {
	eterf_clockne u64 pncpu_funSYSDEV_CLASS_ATTR( perf_>cpu */
	/*
	ENT_Sr0644led inF_EVENT_ST can be schedur's unent nlnding
		sevennt tineeat u;


	returNT_S:eturnin* Gne usnew state nt tool ->loartial gro__we
	return 0;

gib:group_ERF_EVENT_scngstruct   This en cal�  2s Gle *it_sbe visd > s[]IVE)
	 * Pe before returni.ic voidgroup)new state ct perf_>gro;erf_k_in(struc*event, *partive we onsicpu_contexne u co.
 *
 *it_t	spinn chcpu_contextunctnamerf_e,"u_va;

	sps"ERF_EVext for a tasunctioid)
{
	retx/dcae context couldoid)
{
	nly_gdel_rc<= PERFcpu tx;
state mus.kset.kobj.
	 * IfERF_Eteventntirely ofevenadeviceO: pu topredhat.com>
oup_tic in);
