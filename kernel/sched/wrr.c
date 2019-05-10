/* implement TODO marked methods. */
/* the rest is probably not needed */

//cfs load balancing 보면 runqueue에 락잡는 함수가 있음
//load balancing 구현시 for_each_online_cpu
//cpumask_test_cpu() -> 옮길수 있는 cpu에서 이 태스크가 돌아갈 수 있는가?
//task_current() -> 지금 돌고있는 task 판단
//실제 task를 옮길때 deactvie_task  set_task -> activate_task()
// __acquires() <- 락 잡는것들..
// load balance 관련된 로직은 wrr.c에 집어넣고, tick에서 구현하도록 하자

#include "sched.h"

#include <linux/irq_work.h>
#include <linux/sched.h>

DEFINE_SPINLOCK(wrr_loadbalance_lock); // this lock used at trigger_loadbalance.

static unsigned long last_loadbalance_time; // value that saves last_loadbalance time.
//this is static long value so it is initialized by 0.
// this value used at trigger_loadbalance.

void init_wrr_rq(struct wrr_rq *wrr_rq)
{
    int i, cpu;

    INIT_LIST_HEAD(&wrr_rq->queue);
    
    for(i = WRR_MINWEIGHT; i <= WRR_MAXWEIGHT; i++) {
        INIT_LIST_HEAD(&wrr_rq->weight_array[i]);
    }
    
    wrr_rq->count = 0;
    wrr_rq->weight_sum = 0;
    wrr_rq->min_weight = WEIGHT_INITIALVALUE;
    wrr_rq->max_weight = WEIGHT_INITIALVALUE;

    /* TODO.. by CPU number, we shouldn't use wrr_rq. may cpu number is should zero.. */

    cpu = get_cpu();
    put_cpu();
    if (cpu == 0) {
        wrr_rq->usable = 0;
    }
    else {
        wrr_rq->usable = 1;
    }

    wrr_rq->usable = 1;
    
    raw_spin_lock_init(&wrr_rq->wrr_runtime_lock);
}

// get task of wrr_entity
static struct task_struct *get_task_of_wrr_entity(struct sched_wrr_entity *wrr_se)
{
	return container_of(wrr_se, struct task_struct, wrr);
}

static struct wrr_rq *get_runqueue_of_wrr_entity(struct sched_wrr_entity *wrr_se)
{
    return wrr_se->wrr_runqueue;
}

/* this function update max&min weight of wrr queue. need locking. */
static void
update_minmax_weight_wrr(struct wrr_rq *wrr_rq)
{
    int i;
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

/* this function update new_weight to weight, and change wrr_rq's structure by then. */
void update_task_weight_wrr_by_task(struct task_struct *p, int new_weight)
{
    struct wrr_rq *wrr_rq;
    struct sched_wrr_entity *wrr_se;

    wrr_se = &(p->wrr);
    wrr_rq = wrr_se->wrr_runqueue;

    if(wrr_rq == NULL)
    {
        printk(KERN_ALERT" wrr_se doesn't have runqueue! \n");
        return;
    }

    raw_spin_lock(&(wrr_rq->wrr_runtime_lock));

    update_task_weight_wrr(wrr_rq, p, new_weight);

    raw_spin_unlock(&(wrr_rq->wrr_runtime_lock));
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
    
    wrr_rq = &(rq->wrr);
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

    update_insert_wrr(wrr_rq, weight);

    wrr_entity->wrr_runqueue = wrr_rq;      // set entity's runqueue to this wrr_runqueue.
    
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
    
    wrr_rq = &(rq->wrr);
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

    update_delete_wrr(wrr_rq, weight);

    wrr_entity->wrr_runqueue = NULL;      // set entity's runqueue to NULL.
    
    raw_spin_unlock(&(wrr_rq->wrr_runtime_lock));
}

/* need to update entity AFTER requeue */
static void
requeue_task_wrr(struct rq *rq, struct task_struct *p)
{
    /* todo - many other things */
    struct wrr_rq *wrr_rq;
    struct sched_wrr_entity *wrr_se;

    wrr_rq = &(rq->wrr);
    raw_spin_lock(&(wrr_rq->wrr_runtime_lock));

    wrr_se = &(p->wrr);

    list_move_tail(&(wrr_se->run_list), &(wrr_rq->queue));

    wrr_se->time_slice = wrr_se->weight * WRR_TIMESLICE;

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
    if(list_empty(&(wrr_rq->queue))) {
        return NULL;
    }
    else {
        return list_first_entry(&wrr_rq->queue, struct sched_wrr_entity, run_list) -> task;
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

    wrr_rq = &(rq->wrr);
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
    wrr_entity_run_list = &(wrr_entity->run_list);

    if(--(p->wrr.time_slice))  // if current task time_slice is not zero.. don't need to re_schedule.
    {
        raw_spin_unlock(&(wrr_rq->wrr_runtime_lock));
        return;
    }
    else
    {
        requeue_task_wrr(rq, p);
        raw_spin_unlock(&(wrr_rq->wrr_runtime_lock));
        resched_curr(rq); // TODO : is this ok for when holding lock, resched_curr is correct?
        // should we use set_tsk_need_resched(p)?
    }
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

// return value NULL mean that there is no cpu to migration. need RCU lock.
static struct rq *find_lowest_weight_rq(void)
{
    int curr_cpu;
    int curr_weight;
    int lowest_weight;
    struct rq *curr_rq;
    struct rq *lowest_rq;
    struct wrr_rq *curr_wrr_rq;

    lowest_rq = NULL; // not initialized..
    lowest_weight = -1;
    for_each_online_cpu(curr_cpu)
    {
        curr_rq = cpu_rq(curr_cpu);
        curr_wrr_rq = &(curr_rq->wrr);
        if(curr_wrr_rq->usable != 0) // if cpu is usable
        {
            curr_weight = curr_wrr_rq->weight_sum;
            if(lowest_weight == -1) // not initialized..
            {
                lowest_rq = curr_rq;
                lowest_weight = curr_weight;
            }
            else
            {
                if(lowest_weight > curr_weight)
                {
                    lowest_rq = curr_rq;
                    lowest_weight = curr_weight;
                }
            }
        }
    }

    return lowest_rq;
}

// return value NULL mean that there is no cpu to migration. need RCU lock.
static struct rq *find_highest_weight_rq(void)
{
    int curr_cpu;
    int curr_weight;
    int highest_weight;
    struct rq *curr_rq;
    struct rq *highest_rq;
    struct wrr_rq *curr_wrr_rq;
    
    highest_rq = NULL;
    highest_weight = -1; // not initialized..
    for_each_online_cpu(curr_cpu)
    {
        curr_rq = cpu_rq(curr_cpu);
        curr_wrr_rq = &(curr_rq->wrr);
        if(curr_wrr_rq->usable != 0) // if cpu is usable
        {
            curr_weight = curr_wrr_rq->weight_sum;
            if(highest_weight == -1)
            {
                highest_rq = curr_rq;
                highest_weight = curr_weight;
            }
            else
            {
                if(highest_weight < curr_weight)
                {
                    highest_rq = curr_rq;
                    highest_weight = curr_weight;
                }
            }
        }
    }

    return highest_rq;
}

// if migration available, return 1 else return 0.
static int is_migration_available(struct rq *rq_from, struct rq *rq_to, struct task_struct *mig_task)
{
    int weight_rq_from = (rq_from->wrr).weight_sum;
    int weight_rq_to = (rq_to->wrr).weight_sum;
    int weight_mig = (mig_task->wrr).weight;

    // current task is running
    if (rq_from->curr == mig_task)
        return 0;
    
    // migrated task is not runable at other cpu.
    if (!cpumask_test_cpu(rq_to->cpu, &(mig_task->cpus_allowed)))
        return 0;
    
    return 1;
}

// this function called in core.c for load balancing.
void trigger_load_balance_wrr(struct rq *rq)
{
    // TODO
    /* check if 2000ms passed since last load balancing */

    struct rq *rq_highest_weight;
    struct rq *rq_lowest_weight;
    struct wrr_rq *wrr_rq_highest_weight;
    struct wrr_rq *wrr_rq_lowest_weight;
    struct task_struct *migrated_task = NULL;
    struct task_struct *current_task;
    struct list_head *wrr_rq_weight_list;

    unsigned long current_time;
    int weight_highest;
    int minweight_highest;
    int weight_lowest;
    int weight_diff;
    int weight_migrated_task;
    int i;
    int mig_found = 0;

    struct sched_wrr_entity *curr_entity;



    // check is 2000ms passed
    spin_lock(&wrr_loadbalance_lock);
    current_time = jiffies;
    if (current_time < last_loadbalance_time + 2*HZ)
    {
        spin_unlock(&wrr_loadbalance_lock);
        return;
    }
    last_loadbalance_time = current_time;
    spin_unlock(&wrr_loadbalance_lock);

    rcu_read_lock();
    rq_highest_weight = find_highest_weight_rq();
    rq_lowest_weight = find_lowest_weight_rq();
    rcu_read_unlock();

    if(rq_highest_weight == NULL) return;
    if(rq_lowest_weight == NULL) return;
    // can't find runqueue to migrate.  
    // (1 cpu exist or there is no usable cpu. only cpu wrr_rq->usable is 0)

    if(rq_highest_weight == rq_lowest_weight) return;
    // 1 cpu exist case

    double_rq_lock(rq_highest_weight, rq_lowest_weight);
    
    wrr_rq_highest_weight = &(rq_highest_weight->wrr);
    wrr_rq_lowest_weight = &(rq_lowest_weight->wrr);

    weight_highest = wrr_rq_highest_weight->weight_sum;
    weight_lowest = wrr_rq_lowest_weight->weight_sum;
    minweight_highest = wrr_rq_highest_weight->min_weight;

    weight_diff = weight_highest - weight_lowest;

    if(weight_diff > WRR_MAXWEIGHT) weight_diff = WRR_MAXWEIGHT+1;
    else if(weight_diff <= WRR_MINWEIGHT || weight_diff < minweight_highest)
    {
        // weight difference is lower than min weight, counldn't migrate.
        double_rq_unlock(rq_highest_weight, rq_lowest_weight);
        return;
    }


    mig_found = 0;
    migrated_task = NULL;
    // now find candidate for migration.
    for(i = weight_diff-1; i >= minweight_highest; i--) // start from weight difference.
    {
        if(mig_found == 1) break;   // if migration task found..

        wrr_rq_weight_list = &(wrr_rq_highest_weight->weight_array[i]);

        list_for_each_entry(curr_entity, wrr_rq_weight_list, weight_list)
        {
            current_task = container_of(curr_entity, struct task_struct, wrr);

            if(is_migration_available(rq_highest_weight, rq_lowest_weight, current_task))
            {
                mig_found = 1;
                migrated_task = current_task;
                break;
            }
        }
    }

    if (migrated_task == NULL)  // there is no migrate task.
    {
        double_rq_unlock(rq_highest_weight, rq_lowest_weight);
        return;
    }

    // deactive task before moving to other cpu.

    deactivate_task(rq_highest_weight, migrated_task, 0);
    set_task_cpu(migrated_task, rq_lowest_weight->cpu);
    dequeue_task_wrr(rq_highest_weight, migrated_task, 0);
    enqueue_task_wrr(rq_lowest_weight, migrated_task, 0);
    activate_task(rq_lowest_weight, migrated_task, 0);

    double_rq_unlock(rq_highest_weight, rq_lowest_weight);

    /* if this cpu == highest and weight differnce is enough then */
    /*     search for suitable task for migration */
    /*     (e.g. not running and compatiable with lowest cpu) */
    /*     if no such task then */
    /*         load balancing failed. return */
    /*     else then */
    /*         migrate task */
}

/* copied from rt.c's implementation */
// return cpu that is suitable to get new task 
static int
select_task_rq_wrr(struct task_struct *p, int cpu, int sd_flag, int flags)
{
    struct task_struct *curr;
    struct rq *rq;
    struct rq *target_rq;
    int target;

	/* For anything but wake ups, just return the task_cpu */
	if (sd_flag != SD_BALANCE_WAKE && sd_flag != SD_BALANCE_FORK) {
        return cpu;
    }

    if(p->nr_cpus_allowed == 1) {
        return cpumask_first(&(p->cpus_allowed));
    }

    rcu_read_lock();
    target_rq = find_lowest_weight_rq();
    rcu_read_unlock();

    target = (target_rq == NULL) ? cpu : cpu_of(target_rq);

    if (cpumask_test_cpu(target, &p->cpus_allowed) == 0) {
        target = cpumask_any(&p->cpus_allowed);
    }

    return target;
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


    .select_task_rq   = select_task_rq_wrr,
    .set_cpus_allowed = set_cpus_allowed_common,
    .rq_online        = rq_online_wrr,
    .rq_offline       = rq_offline_wrr,
    .task_woken       = task_woken_wrr,
    .switched_from    = switched_from_wrr

};
