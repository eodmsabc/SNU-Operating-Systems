/* implement TODO marked methods. */
/* the rest is probably not needed */

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
    // TODO
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
    
    if(wrr_rq->count++ == 0) {
        wrr_rq->max_weight = wrr_rq->min_weight = weight;
    }
    else {
        if(weight > wrr_rq->max_weight) {
            wrr_rq->max_weight = weight;
        }
        else if(weight < wrr_rq->min_weight) {
            wrr_rq->max_weight = weight;
        }
    }
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

    if(wrr_rq->count == 0) {
        return;
    }

    list_del(&wrr_se->weight_list);
    list_del(&wrr_se->run_list);
    wrr_rq->weight_sum -= weight;

    if(wrr_rq->count-- == 1) {
        wrr_rq->max_weight = WRR_MINWEIGHT;
        wrr_rq->min_weight = WRR_MAXWEIGHT;
        return;
    }

    if(weight == wrr_rq->max_weight) {
        for(i = weight; i >= wrr_rq->min_weight; i--) {
            if(!list_empty(w_arr[i])) {
                wrr_rq->max_weight = i;
                return;
            }
        }
    }
    else if(weight == wrr_rq->min_weight) {
        for(i = weight; i <= wrr_rq->max_weight; i++) {
            if(!list_empty(w_arr[i])) {
                wrr_rq->min_weight = i;
                return;
            }
        }
    }
}

static void
requeue_task_wrr(struct rq *rq, struct task_struct *p);
{
    /* todo - locking and many other things */
    struct wrr_rq *wrr_rq = &rq->wrr_rq;
    struct sched_wrr_entity *wrr_se = &p->wrr;
    int new_weight = wrr_se->new_weight;

    list_del(&wrr_se->run_list);
    list_add_tail(&wrr_se->run_list, &wrr_rq->queue);

    if(new_weight == wrr_se->weight) {
        return;
    }

    wrr_rq->weight_sum += new_weight - wrr_se->weight;
    
    if(weight > wrr_rq->max_weight) {
    }
    else if(weight < wrr_rq->min_weight) {
    }

    list_add(&wrr_se->weight_list, wrr_rq->weight_array[weight]);
}

static void yield_task_wrr(struct rq *rq)
{
    // TODO
    requeue_task_wrr(rq, rq->curr);
}

static struct task_struct *
pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
    // TODO
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{
    // TODO
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
    // TODO
}

/* when to update rq: */ 
/* 1. new task is enqueued */
/* 2. existing task is modified (weight) */
/* 3. existing task is deleted (migrated or terminated) */
/* static void */
/* update_wrr_rq(struct wrr_rq *wrr_rq, sched_wrr_entity *wrr_se) */
/* { */
/*     int new_weight = wrr_se->new_weight; */

/*     if(wrr_se->weight == wrr_se->new_weight) { */
/*         return; */
/*     } */

/*     wrr_rq->weight_sum += new_weight - wrr_se->weight; */

/*     if(new_weight > wrr_rq->max_weight) { */
/*         wrr_rq->max_weight = new_weight; */
/*     } */

/*     if(new_weight != 0) { */
/*         if(wrr_se->weight != 0) { */
/*             /1* weight modified *1/ */
/*             list_del(&wrr_se->weight_list); */
/*         } */
/*         /1* new task *1/ */
/*         list_add(&wrr_se->weight_list, &wrr_rq->weight_array[new_weight]); */
/*     } */
/*     else { */
/*         /1* delete task *1/ */
/*         list_del(&wrr_se->weight_list); */
/*     } */
/* } */

/* static void */
/* update_wrr_se(struct wrr_rq *wrr_rq, struct sched_wrr_entity *wrr_se) */
/* { */
/*     int new_weight = wrr_se->new_weight; */
/*     wrr_se->time_slice = new_weight * WRR_TIMESLICE; */

/*     update_wrr_rq(wrr_rq, wrr_se); */
/*     if(wrr_se->weight != new_weight) { */

/*         wrr_rq->weight_sum += new_weight - wrr_se->weight; */
/*         wrr_rq->max */
/*     } */

/*     wrr_se->weight = new_weight; */
/* } */

static void update_curr_wrr(struct rq *rq)
{
}

static void
check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
}

/* scheduler_tick callback function */
/* called by timer. */
static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
    /* todo - sadf */
    struct wrr_rq *wrr_rq = &rq->wrr_rq;
    struct sched_wrr_entity *wrr_se = &p->wrr;

    if(--p->wrr.time_slice) {
        return;
    }

    /* update entity and requeue? */ 
    requeue_task_wrr(rq, p);

}

static void task_fork_wrr(struct task_struct *p)
{
    // TODO
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
    // TODO
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
