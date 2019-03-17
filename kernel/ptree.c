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

/*
 * extracts necessary information from task_struct and initializes prinfo.
 */
static int prinfo_constructor(struct prinfo *item, struct task_struct *task)
{
    struct task_struct *tmp;

    item->state             = (int64_t) task->state;
    item->pid               = (pid_t)   task->pid;
    item->parent_pid        = (pid_t)   task->parent->pid;
    
    /* if child is none-existent, first_child_pid should be 0 */
    tmp = list_entry(task->children.next, struct task_struct, sibling);
    item->first_child_pid   = (pid_t)   (tmp ? tmp->pid : 0);

    /* if next sibling is none-existent, next_sibling_pid should be 0 */
    tmp = list_entry(task->sibling.next, struct task_struct, sibling);
    item->next_sibling_pid  = (pid_t)   (tmp ? tmp->pid : 0);

    item->uid               = (int64_t) task->cred->uid.val;

    memcpy((void*) (item->comm), (void*) (task->comm), 64);
    item->comm[63] = '\0';

    return 0;
}

/*
 * Wrapper struct of task_struct with linux list
 * 
 * This struct will be used as list for implementing non-recursive DFS.
 */
struct task_list
{
    struct task_struct *task;
    struct list_head list;
};

/*
 * creates new node and link into list.
 */
static int add_new_task(struct task_struct *task, struct list_head *t_list)
{
    struct task_list *new_node;

    new_node = (struct task_list*) kmalloc(sizeof(struct task_list), GFP_KERNEL);

    if (!new_node) {
        printk(KERN_ERR "[PROJ1] kmalloc for new item in task list has failed\n");
        return -EFAULT;
    }

    new_node->task = task;
    INIT_LIST_HEAD(&(new_node->list));
    list_add(&new_node->list, t_list);

    return 0;
}

/*
 * New system call "ptree"
 *
 * traverses all process tree and copies the information into 'buf'
 * while 'buf' is user-area which can store 'nr' number of prinfo struct.
 *
 * If the number of processes now running is smaller than nr, 'nr' will be updated.
 */
asmlinkage long sys_ptree(struct prinfo __user *buf, int __user *nr)
{
    int32_t k_nr;
    int32_t count = 0;
    int32_t tmp_int32;
    struct prinfo *k_buf;
    struct prinfo *dummy_ptr;
    struct prinfo storage;
    struct list_head *pos;
    struct list_head *q;
    struct task_list *current_item;

    LIST_HEAD(tasks_to_visit);

    /* NULL Pointer check for parameters buf and nr */
    if (!buf || !nr) {
        printk(KERN_ERR "[PROJ1] buf or nr has null pointer\n");
        return -EINVAL;
    }

    /* copy nr from user space */
    tmp_int32 = copy_from_user((void*) &k_nr, (void*) nr, sizeof(int32_t));
    if (tmp_int32 != 0) {
        printk(KERN_ERR "[PROJ1] could not copy nr from userspace\n");
        return -EFAULT;
    }

    /* return -EINVAL when nr is invalid */
    if (k_nr < 1) {
        printk(KERN_ERR "[PROJ1] invalid nr value: %d\n", k_nr);
        return -EINVAL;
    }

    /* allocates memory for prinfo struct array which will temporarily store process information */
    k_buf = (struct prinfo*) kmalloc(sizeof(struct prinfo) * k_nr, GFP_KERNEL);
    if (!k_buf) {
        printk(KERN_ERR "[PROJ1] kmalloc for struct prinfo *buf failed\n");
        return -EFAULT;
    }

    read_lock(&tasklist_lock);

    add_new_task(&init_task, &tasks_to_visit);

    while(!list_empty(&tasks_to_visit)) {

        current_item = list_entry(tasks_to_visit.next, struct task_list, list);

        dummy_ptr = (count++ < k_nr) ? &k_buf[count - 1] : &storage;
        
        if (!prinfo_constructor(dummy_ptr, current_item->task)) {
            printk(KERN_ERR "[PROJ1] prinfo constructor error - skipping\n");
            continue;
        }

        list_del(&(current_item->list));
        if(!list_empty_careful(&(current_item->task->children))) {
            list_for_each_prev_safe(pos, q, &(current_item->task->children)) {
                add_new_task(
                        list_entry(pos, struct task_struct, sibling),
                        &tasks_to_visit);
            }
        }
        kfree(current_item);
    }

    read_unlock(&tasklist_lock);

    if (count < k_nr) {
        k_nr = count;
        tmp_int32 = copy_to_user((void*) nr, (void*) &count, sizeof(int32_t));
        
        if (tmp_int32 != 0) {
            printk(KERN_ERR "[PROJ1] could not update user variable nr\n");
        }
    }

    tmp_int32 = copy_to_user((void*) buf, (void*) k_buf, sizeof(struct prinfo) * k_nr);
    if (tmp_int32 != 0) {
        printk(KERN_ERR "[PROJ1] could not copy %d bytes to user mem\n", tmp_int32);
    }

    list_for_each_safe(pos, q, &tasks_to_visit) {
        current_item = list_entry(pos, struct task_list, list);
        list_del(pos);
        kfree(current_item);
    }
    kfree(k_buf);

    return count;
}

