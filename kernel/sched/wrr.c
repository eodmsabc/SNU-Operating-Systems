/* implement TODO marked methods. */
/* the rest is probably not needed */

#include "sched.h"

#include <linux/irq_work.h>
#include <linux/sched.h>

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
    .task_dead          = task_dead_wrr,
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
    wrr_rq->min_weight = WEIGHT_INITIALVALUE;
    wrr_rq->max_weight = WEIGHT_INITIALVALUE;

    /* TODO.. by CPU number, we shouldn't use wrr_rq. may cpu number is should zero.. */
    wrr_rq->usable = 1;    
    
    raw_spin_lock_init(&wrr_rq->wrr_runtime_lock);
}

// get task of wrr_entity
static struct task_struct *get_task_of_wrr_entity(struct sched_wrr_entity *wrr_se)
{
	return container_of(wrr_se, struct task_struct, wrr);
}

static wrr_rq *get_runqueue_of_wrr_entity(struct sched_wrr_entity *wrr_se)
{
    return wrr_se->wrr_runqueue;
}

/* this function update wrr queue. modifies count, weight sum, min and max weight.
 when insert item. need locking. */
static void
update_insert_wrr(struct wrr_rq *wrr_rq, int weight)
{
    (wrr_rq->count)++;
    (wrr_rq->weight_sum) += weight;
    update_minmax_weight_wrr(wrr_rq);
}

/* this function update wrr queue. modifies count, weight sum, min and max weight.
 when delete item. need locking. */
static void
update_delete_wrr(struct wrr_rq *wrr_rq, int weight)
{
    (wrr_rq->count)--;
    (wrr_rq->weight_sum) -= weight;
    update_minmax_weight_wrr(wrr_rq);
}

/* this function update max&min weight of wrr queue. need locking. */
static void
update_minmax_weight_wrr(struct wrr_rq *wrr_rq)
{
    int min_weight = WEIGHT_INITIALVALUE;
    int max_weight = WEIGHT_INITIALVALUE;
    struct list_head *weight_array = wrr_rq->weight_array;

    for(i = WRR_MINWEIGHT; i <= WRR_MAXWEIGHT; i++) 
    {
        if(!list_empty(&(weight_array[i])))
        {
            min_weight = i;
            break;
        }
    }      
    // find min_weight

    for(i = WRR_MAXWEIGHT; i >= WRR_MINWEIGHT; i--) 
    {
        if(!list_empty(&(weight_array[i]))) 
        {
            max_weight = i;
            break;
        }
    }
    // find max_weight

    wrr_rq->min_weight = min_weight;
    wrr_rq->max_weight = max_weight;
    // set weight. if list is all empty, then set initial value.
}


/* this function update new_weight to weight, and change wrr_rq's structure by then. */
void update_task_weight_wrr_by_task(struct task_struct *p, int new_weight)
{
    struct wrr_rq *wrr_rq;
    struct sched_wrr_entity *wrr_se;
    int old_weight;

    wrr_se = &(p->wrr);
    wrr_rq = &(wrr_se->wrr_runqueue);

    raw_spin_lock(&(wrr_rq->wrr_runtime_lock));

    update_task_weight_wrr(wrr_rq, p, new_weight);

    raw_spin_unlock(&(wrr_rq->wrr_runtime_lock));
}

/* this function update new_weight to weight, and change wrr_rq's structure by then. need locking. */
static void update_task_weight_wrr(struct wrr_rq *wrr_rq, struct task_struct *p, int new_weight)
{
    struct sched_wrr_entity *wrr_se;
    int old_weight;

    wrr_se = &(p->wrr);
    old_weight = wrr_se->weight;

    if(old_weight == new_weight) {  // no need to update wrr_entity.        
        return;
    }

    wrr_se->weight = new_weight;

    list_move_tail(&(wrr_se->weight_list), &(wrr_rq->weight_array[new_weight]));

    wrr_rq->weight_sum += new_weight - old_weight;

    update_minmax_weight_wrr(wrr_rq);
}

/* enqueue, dequeue, requeue */
/* assume for now that the entity p->wrr is populated */
static void
enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    struct wrr_rq *wrr_rq;
    struct sched_wrr_entity *wrr_entity;
    struct list_head *wrr_rq_queue;
    struct list_head *wrr_rq_weight_arr;
    struct list_head *wrr_entity_run_list;
    struct list_head *wrr_entity_weight_list;
    int weight;
    
    wrr_rq = &(rq->wrr_rq);
    raw_spin_lock(&(wrr_rq->wrr_runtime_lock));

    wrr_entity = &(p->wrr);
    weight = wrr_entity->weight;

    wrr_rq_queue = &(wrr_rq->queue);
    wrr_rq_weight_arr = &(wrr_rq->weight_array[weight]);
    wrr_entity_run_list = &(wrr_entity->run_list);
    wrr_entity_weight_list = &(wrr_entity->weight_list);
    // list values initialize.


    list_add_tail(wrr_entity_run_list, wrr_rq_queue);
    list_add(wrr_entity_weight_list, wrr_rq_weight_arr);
    // add wrr_entity to runqueue & weightqueue.

    p->on_rq = 1;

    update_insert_wrr(*wrr_rq, weight);
    
    raw_spin_unlock(&(wrr_rq->wrr_runtime_lock));
}

static void
dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    struct wrr_rq *wrr_rq;
    struct sched_wrr_entity *wrr_entity;
    struct list_head *wrr_rq_queue;
    struct list_head *wrr_rq_weight_arr;
    struct list_head *wrr_entity_run_list;
    struct list_head *wrr_entity_weight_list;
    int weight;
    
    wrr_rq = &(rq->wrr_rq);
    raw_spin_lock(&(wrr_rq->wrr_runtime_lock));

    wrr_entity = &(p->wrr);
    weight = wrr_entity->weight;

    wrr_rq_queue = &(wrr_rq->queue);
    wrr_rq_weight_arr = &(wrr_rq->weight_array[weight]);
    wrr_entity_run_list = &(wrr_entity->run_list);
    wrr_entity_weight_list = &(wrr_entity->weight_list);

    // list values initialize.

    list_del_init(wrr_entity_run_list);
    list_del_init(wrr_entity_weight_list);

    p->on_rq = 0;

    update_delete_wrr(*wrr_rq, weight);
    
    raw_spin_unlock(&(wrr_rq->wrr_runtime_lock));
}

/* need to update entity AFTER requeue */
static void
requeue_task_wrr(struct rq *rq, struct task_struct *p);
{
    /* todo - many other things */
    struct wrr_rq *wrr_rq;
    struct sched_wrr_entity *wrr_se;

    wrr_rq = &(rq->wrr_rq);
    raw_spin_lock(&(wrr_rq->wrr_runtime_lock));

    wrr_se = &(p->wrr);

    list_move_tail(&(wrr_se->run_list), &(wrr_rq->queue));

    wrr_se->timeslice = wrr_se->weight * WRR_TIMESLICE;

    raw_spin_unlock(&(wrr_rq->wrr_runtime_lock));
}

static void yield_task_wrr(struct rq *rq)
{
    requeue_task_wrr(rq, rq->curr);
}

static struct task_struct *
pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
    struct wrr_rq *wrr_rq = &(rq->wrr);
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
    struct sched_wrr_entity *wrr_entity;
    wrr_entity = &(p->wrr);

    if(p == NULL) return;
    if(p->policy != SCHED_WRR) return;
    // if (list_empty(&(p->wrr.run_list))) return;

    wrr_entity->weight = 10;
    wrr_entity->time_slice = wrr_entity->weight * WRR_TIMESLICE;

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
    struct wrr_rq *wrr_rq;
    struct sched_wrr_entity *wrr_entity;
    struct list_head *wrr_rq_queue;
    struct list_head *wrr_rq_weight_arr;
    struct list_head *wrr_entity_run_list;
    struct list_head *wrr_entity_weight_list;

    wrr_rq = &(rq->wrr_rq);
    raw_spin_lock(&(wrr_rq->wrr_runtime_lock));

    if(p == NULL)   // if task_struct don't accessible..
    {
        raw_spin_unlock(&(wrr_rq->wrr_runtime_lock));
        return;
    }
    else if(p->policy != SCHED_WRR) // policy is not SCHED_WRR, don't need to do job.
    {
        raw_spin_unlock(&(wrr_rq->wrr_runtime_lock));
        return;
    }

    wrr_entity = &(p->wrr);
    wrr_entity_run_list = wrr_entity->run_list;

    if(--(p->wrr.time_slice))  // if current task time_slice is not zero.. don't need to re_schedule.
    {
        raw_spin_unlock(&(wrr_rq->wrr_runtime_lock));
        return;
    }
    else
    {
        requeue_task_wrr(rq, p);
        resched_curr(rq); // TODO : is this ok for when holding lock, resched_curr is correct?
        // should we use set_tsk_need_resched(p)?
    }

    raw_spin_unlock(&(wrr_rq->wrr_runtime_lock));
    return;
}

/*
 * from fair.c
 * called on fork with the child task as argument from the parent's context
 *  - child not yet on the tasklist
 *  - preemption disabled
 */
static void task_fork_wrr(struct task_struct *p)
{
    if (p == NULL) return;

    p->wrr.weight = p->real_parent->wrr.weight;
    p->wrr.time_slice = p->wrr.weight * WRR_TIMESLICE;

    return;
}

static void task_dead_wrr(struct task_struct *p)
{
    /* TODO */
    return;
}

#ifdef CONFIG_SMP

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

#endif

static void switched_from_wrr(struct rq *rq, struct task_struct *p)
{
}
