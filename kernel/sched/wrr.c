#include "sched.h"

#include <linux/irq_work.h>

const struct sched_class wrr_sched_class = {

    .next               = &fair_sched_class,
    .enqueue_task       = enqueue_task_wrr,
    .dequeue_task       = dequeue_task_wrr,
    .yield_task         = yield_task_wrr,
    .check_preempt_curr = check_preempt_curr_wrr,
    .pick_next_task     = pick_next_task_wrr,
    .put_prev_task      = put_prev_task_wrr,
    .set_curr_task      = set_curr_task_wrr,
    .task_tick          = task_tick_wrr,
    .get_rr_interval    = get_rr_interval_wrr,
    .prio_changed       = prio_changed_wrr,
    .switched_to        = switched_to_wrr,
    .update_curr        = update_curr_wrr,


#ifdef CONFIG_SMP
    .select_task_rq   = select_task_rq_wrr,
    .set_cpus_allowed = set_cpus_allowed_common,
    .rq_online        = rq_online_wrr,
    .rq_offline       = rq_offline_wrr,
    .task_woken       = task_woken_wrr,
    .switched_from    = switched_from_wrr
#endif  /* CONFIG SMP */

};

void init_wrr_rq(struct wrr_rq *wrr_rq)
{
}

static void
enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
}

static void
dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
}

// this one is not in interface. but yield task uses it.
static void
requeue_task_wrr(struct rq *rq, struct task_struct *p, int head)
{
}

static void yield_task_wrr(struct rq *rq)
{
    requeue_task_wrr(rq, rq->curr, 0);
}

static struct task_struct *
pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{
}

static void
prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio)
{
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p)
{
}

static void set_curr_task_wrr(struct rq *rq)
{
}

static unsigned int
get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
}

static void update_curr_wrr(struct rq *rq)
{
    struct task_struct *curr = rq->curr;
    struct sched_wrr_entity *wrr_entity = &curr->wrr;
    
    if(curr->sched_class != &wrr_sched_class) {
        return
    }

    /* maybe update weight here?? */
}

static void
check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
}

/* scheduler_tick callback function */
/* called by timer. */
static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
    struct sched_wrr_entity *wrr_se = &p->wrr;

	update_curr_wrr(rq);

    if(p->policy != SCHED_WRR) {
        return;
    }

    /* requeue it? */

}


#ifdef CONFIG_SMP

void trigger_load_balance_wrr(struct rq *rq)
{
    /* check if 2000ms passed */
    /* check if this rq has unbalanced highest weight */
    /* check if other rq has unbalanced lowest weight */
    /* check if this rq has suitable task for migration */
    /* then lock both queue and migrate suitable task */
}

static int
select_task_rq_wrr(struct task_struct *p, int cpu, int sd_flag, int flags)
{
}

static void task_woken_wrr(struct rq *rq, struct task_struct *p)
{
}

static void rq_online_wrr(struct rq *rq)
{
}

static void rq_offline_wrr(struct rq *rq)
{
}

static void switched_from_wrr(struct rq *rq, struct task_struct *p)
{
}
#endif  /* CONFIG SMP */
