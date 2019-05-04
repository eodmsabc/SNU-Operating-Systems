/* implement TODO marked methods. */
/* the rest is probably not needed */

#include "sched.h"

#include <linux/irq_work.h>

#define lowest_rq(polar_value) (polar_value & (int) 65535);
#define highest_rq(polar_value) (polar_value >> 16);

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
    .task_fork          = task_fork_wrr,
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
    int i;

    INIT_LIST_HEAD(&wrr_rq->queue);

    for(i = WRR_MINWEIGHT; i <= WRR_MAXWEIGHT; i++) {
        INIT_LIST_HEAD(&wrr_rq->weight_array[i]);
    }
    
    wrr_rq->count = 0;
    wrr_rq->weight_sum = 0;
    wrr_rq->min_weight = WRR_MAXWEIGHT;
    wrr_rq->max_weight = WRR_MINWEIGHT;

    wrr_rq->usable = 1;

    raw_spin_lock_init(&wrr_rq->wrr_runtime_lock);
}

static void
update_minmax_weight_insert_wrr(struct wrr_rq *wrr_rq, int weight)
{
    if (wrr_rq->min_weight <= weight && weight <= wrr_rq->max_weight)
        return;

    if (weight < wrr_rq->min_weight)
    {
        wrr_rq->min_weight = weight;
    }

    if (weight > wrr_rq->max_weight)
    {
        wrr_rq->max_weight = weight;
    }
}

static void
update_minmax_weight_delete_wrr(struct wrr_rq *wrr_rq, int weight)
{
    int i;

    if (wrr_rq->count == 0) {
        wrr_rq->min_weight = WRR_MAXWEIGHT;
        wrr_rq->max_weight = WRR_MINWEIGHT;
        return;
    }
    
    if (wrr_rq->min_weight < weight && weight < wrr_rq->max_weight)
        return;

    if (wrr_rq->min_weight == weight) {
        for (i = weight; i <= WRR_MAXWEIGHT; i++) {
            if (!list_empty(w_arr[i])) {
                wrr_rq->min_weight = i;
                return;
            }
        }
    }
    else if (wrr_rq->max_weight == weight) {
        for (i = weight; i >= WRR_MINWEIGHT; i--) {
            if (!list_empty(w_arr[i])) {
                wrr_rq->max_weight = i;
                return;
            }
        }
    }
}

static void update_task_wrr(struct task_struct *p)
{
    struct sched_wrr_entity *wrr_se = &p->wrr;
    wrr_se->weight = wrr_se->new_weight;
    wrr_se->timeslice = wrr_se->weight * WRR_TIMESLICE;
}

/* enqueue, dequeue, requeue */
/* assume for now that the entity p->wrr is populated */
static void
enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    /* todo - locking?? */ 
    struct wrr_rq *wrr_rq = &rq->wrr_rq;
    struct sched_wrr_entity *wrr_se = &p->wrr;
    int weight = wrr_se->weight;
    
    list_add(&wrr_se->weight_list, &wrr_rq->weight_array[weight]);
    list_add_tail(&wrr_se->run_list, &wrr_rq->queue);

    wrr_rq->weight_sum += weight;

    wrr_rq->count++;

    update_minmax_weight_insert_wrr(wrr_rq, weight);
}

static void
dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    /* todo - locking?? */
    struct wrr_rq *wrr_rq = &rq->wrr_rq;
    struct sched_wrr_entity *wrr_se = &p->wrr;
    struct list_head *w_arr = wrr_rq->weight_array;
    int weight = wrr_se->weight;
    int i;

    list_del(&wrr_se->weight_list);
    list_del(&wrr_se->run_list);

    wrr_rq->weight_sum -= weight;

    update_minmax_weight_delete_wrr(wrr_rq, weight);
}

/* need to update entity AFTER requeue */
static void
requeue_task_wrr(struct rq *rq, struct task_struct *p);
{
    /* todo - locking and many other things */
    struct wrr_rq *wrr_rq = &rq->wrr_rq;
    struct sched_wrr_entity *wrr_se = &p->wrr;
    int old_weight = wrr_se->weight;
    int new_weight = wrr_se->new_weight;

    list_move_tail(&wrr_se->run_list, &wrr_rq->queue);

    if(old_weight == new_weight) {
        return;
    }

    list_move_tail(&wrr_se->weight_list, &wrr_rq->weight_array[new_weight]);

    wrr_rq->weight_sum += new_weight - old_weight;

    update_minmax_weight_delete_wrr(wrr_rq, old_weight);
    update_minmax_weight_insert_wrr(wrr_rq, new_weight);
}

static void yield_task_wrr(struct rq *rq)
{
    requeue_task_wrr(rq, rq->curr);
    update_task_wrr(rq->curr);
}

static struct task_struct *
pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
    struct wrr_rq *wrr_rq = &rq->wrr;
    if(list_empty(wrr_rq->queue)) {
        return NULL;
    }
    else {
        return wrr_rq->queue.next.task;
    }
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{
    // TODO
    // populate p->wrr (which is entity)
    // assign task to laziest cpu
}

static void
prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio)
{
}

static void set_curr_task_wrr(struct rq *rq)
{
}

static unsigned int
get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
    return task->wrr.weight * WRR_TIMESLICE;
}

static void
check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
}

/* scheduler_tick callback function */
/* called by timer. */
static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
    /* TODO */
    struct wrr_rq *wrr_rq = &rq->wrr_rq;
    struct sched_wrr_entity *wrr_se = &p->wrr;

    if(--p->wrr.time_slice) {
        return;
    }

    requeue_task_wrr(rq, p);
    update_task_wrr(p);

    resched_curr(rq);
}

/*
 * from fair.c
 * called on fork with the child task as argument from the parent's context
 *  - child not yet on the tasklist
 *  - preemption disabled
 */
static void task_fork_wrr(struct task_struct *p)
{
    // TODO
}

static void find_polar_rq()
{
    // TODO
    // for load balancing and new tasks(switched_to) and select_task_rq_wrr
    // find highest, lowest weight sum cpu. if tie, break with count
    // return masked cpu number (high 16 bit is highest, lower 16 bit is lowest)
}

void trigger_load_balance_wrr(struct rq *rq)
{
    // TODO
    /* check if 2000ms passed since last load balancing */
    int v = find_polar_rq();
    int lowest = _lowest_rq(v);
    int highest = _highest_rq(v);

    /* if this cpu == highest and weight differnce is enough then */
    /*     search for suitable task for migration */
    /*     (e.g. not running and compatiable with lowest cpu) */
    /*     if no such task then */
    /*         load balancing failed. return */
    /*     else then */
    /*         migrate task */
}

/* copied from rt.c's implementation */
static int
select_task_rq_wrr(struct task_struct *p, int cpu, int sd_flag, int flags)
{
    struct task_struct *curr;
    struct rq *rq;
    int target;

	/* For anything but wake ups, just return the task_cpu */
	if (sd_flag != SD_BALANCE_WAKE && sd_flag != SD_BALANCE_FORK) {
        return cpu;
    }

    rq = cpu_rq(cpu);

    rcu_read_lock();
    curr = READ_ONCE(rq->curr);

    if(curr->nr_cpus_allowed != 1) {
        target = lowest_rq(find_polar_rq());
        // if target cpu is compatiable with p
        // cpu = target;
    }

    rcu_read_unlock();

    return cpu;
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
