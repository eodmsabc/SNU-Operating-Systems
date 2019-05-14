#ifndef _LINUX_SCHED_WRR_H
#define _LINUX_SCHED_WRR_H

#include <linux/sched.h>

#define WRR_TIMESLICE (1 * HZ / 100) //TODO
#define WRR_MINWEIGHT 1
#define WRR_MAXWEIGHT 20
#define WEIGHT_INITIALVALUE -1
#define WRR_NO_USE_CPU_NUM 3

#endif  /* _LINUX_SCHED_WRR_H */

struct rq *find_lowest_weight_rq(struct task_struct *p);
void print_errmsg(const char * str, struct rq *rq);