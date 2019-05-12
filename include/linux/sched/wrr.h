#ifndef _LINUX_SCHED_WRR_H
#define _LINUX_SCHED_WRR_H

#include <linux/sched.h>

#define WRR_TIMESLICE (10 * HZ / 100) //TODO
#define WRR_MINWEIGHT 1
#define WRR_MAXWEIGHT 20
#define WEIGHT_INITIALVALUE -1

#endif  /* _LINUX_SCHED_WRR_H */
