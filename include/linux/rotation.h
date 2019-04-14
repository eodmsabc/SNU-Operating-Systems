#ifndef __LINUX_ROTATION_H__
#define __LINUX_ROTATION_H__

#include <linux/completion.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/sched.h>

void exit_rotlock(struct task_struct *tsk);

#define ABS(x) (((x) < 0) ? (-(x)) : (x))

// Adjust angle to fit in 0 ~ 359
#define ANGLE_ADJUST(x) (((x) < 0) ? ((x) + 360) : ((x) % 360))

// Get difference from two angle
#define ANGLE_DIFF(x, y) (180 - ABS(180 - ABS((x) - (y))))

struct rotation_lock {
    pid_t pid;  // save lock's caller's pid.
    int degree;
    int range;
    struct completion comp;         // save completion
    struct list_head list;          // list member
};

#endif
