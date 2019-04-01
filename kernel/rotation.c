#include <asm/unistd.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/cred.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/task.h>

/*
 * sets the current device rotation in the kernel.
 * syscall number 398 (you may want to check this number!)
 */
SYSCALL_DEFINE1 (set_rotation, int __user, degree)
{
    
    return 0;
}

/*
 * Take a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 399 and 400
 */

SYSCALL_DEFINE2 (rotlock_read, int __user, degree, int __user, range)
{
    
    return 0;
}

SYSCALL_DEFINE2 (rotlock_write, int __user, degree, int __user, range)
{
    
    return 0;
}


/*
 * Release a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 401 and 402
 */

SYSCALL_DEFINE2 (rotunlock_read, int __user, degree, int __user, range)
{
    
    return 0;
}

SYSCALL_DEFINE2 (rotunlock_write, int __user, degree, int __user, range)
{
    
    return 0;
}