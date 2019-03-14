#include <asm/unistd.h>
#include <linux/kernel.h>
#include <linux/cred.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/prinfo.h>
#include <linux/sched.h>
#include <linux/sched/task.h>

static int construct_prinfo(struct prinfo *item, struct task_struct *task)
{
    struct task_struct *tmp;

    item->state             = (int64_t) task->state;
    item->pid               = (pid_t)   task->pid;
    item->parent_pid        = (pid_t)   task->parent->pid;
    
    printk(KERN_ALERT "BEFORE 1st list_entry in construction function %p\n", task);
    tmp = list_entry(task->children.next, struct task_struct, sibling);
    item->first_child_pid   = (pid_t)   (tmp ? tmp->pid : 0);

    printk(KERN_ALERT "BEFORE 2nd list_entry in construction function %p\n", task);
    tmp = list_entry(task->sibling.next, struct task_struct, sibling);
    item->next_sibling_pid  = (pid_t)   (tmp ? tmp->pid : 0);

    item->uid               = (int64_t) task->cred->uid.val;

    memcpy((void*)(item->comm), (void*)(task->comm), 64);
    item->comm[63] = '\0';

    return 0;
}

static int dfs_tasklist(struct task_struct *task, struct prinfo *buf, int *count, int k_nr)
{
    printk(KERN_ALERT "dfs function start\n");
    struct list_head *pos;

    if (*count < k_nr) {
        struct prinfo *tmp = (struct prinfo*) kmalloc(sizeof(struct prinfo), GFP_KERNEL);
        printk(KERN_ALERT "AFTER kmalloc\n");

        construct_prinfo(tmp, task);
        printk(KERN_ALERT "construct_prinfo\n");

        if (copy_to_user(
                    buf + sizeof(struct prinfo) * (*count),
                    tmp,
                    sizeof(struct prinfo)) != 0) {
            printk(KERN_ALERT "cound not copy prinfo to user-space\n");
        }

        kfree(tmp);
    }

    (*count)++;
    
    list_for_each(pos, &(task->children)) {
        dfs_tasklist(
                list_entry(pos, struct task_struct, children), buf, count, k_nr);
    }

    return 0;
}

asmlinkage long sys_ptree(struct prinfo *buf, int *nr)
{
    printk(KERN_ALERT "ptree system call\n");

    int k_nr;
    int count = 0;
    struct task_struct *task_struct_ptr;

    /* copy 2nd parameter "nr" from user-space */
    if (copy_from_user(&k_nr, nr, sizeof(int)) != 0) {
        printk(KERN_ALERT "could not copy int *nr from user-space\n");
        return -EFAULT;
    }

    if (k_nr < 1) {
        printk(KERN_ALERT "invalid nr value: %d\n", k_nr);
        return -EINVAL;
    }
    
    read_lock(&tasklist_lock);
    /* tasklist_locked */

    printk(KERN_ALERT "find get pid 0 possible?\n");
    task_struct_ptr = pid_task(find_get_pid(1), PIDTYPE_PID)->parent;
    printk(KERN_ALERT "It's possible! %p\n", task_struct_ptr);

    dfs_tasklist(task_struct_ptr, buf, &count, k_nr);

    /* tasklist_unlock */
    read_unlock(&tasklist_lock);

    printk(KERN_ALERT "ptree system call finished\n");
    return count;
}
