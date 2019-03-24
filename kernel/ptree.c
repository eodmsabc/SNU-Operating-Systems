#include <asm/unistd.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/cred.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/prinfo.h>
#include <linux/sched.h>
#include <linux/sched/task.h>

/*
 * extract necessary information from task_struct and initializes prinfo.
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

    memcpy((void*) (item->comm), (void*) (task->comm), TASK_COMM_LEN);
    memset((void*) &(item->comm[TASK_COMM_LEN]),
            0,
            sizeof(char) * (64 - TASK_COMM_LEN));

    return 0;
}

/*
 * Wrapper struct of task_struct with linux list
 * 
 * This struct will be used as linked STACK for implementing non-recursive DFS.
 */
struct task_list
{
    struct task_struct *task;
    struct list_head list;
};

/*
 * create new node and link into list.
 * insert new node in front of the original list.
 */
static int add_new_task(struct task_struct *task, struct list_head *t_list)
{
    struct task_list *new_node;

    new_node = (struct task_list*)
        kmalloc(sizeof(struct task_list), GFP_KERNEL);

    if (!new_node) {
        printk(KERN_ERR
                "[PROJ1] kmalloc for new item in task list has failed.\n");
        return -EFAULT;
    }

    new_node->task = task;
    INIT_LIST_HEAD(&(new_node->list));
    list_add(&new_node->list, t_list);

    return 0;
}

/*
 * delete & frees every node in task_list
 */
static void del_tasklist(struct list_head *t_list)
{
    struct list_head *pos;
    struct list_head *q;
    struct task_list *item;

    list_for_each_safe(pos, q, t_list) {
        item = list_entry(pos, struct task_list, list);
        list_del(pos);
        kfree(item);
    }
}

/*
 * New system call "ptree"
 *
 * traverses all process tree and copies the information into 'buf'
 * while 'buf' is user-area which can store 'nr' number of prinfo struct.
 *
 * If the number of processes now running is smaller than nr,
 * 'nr' will be updated.
 */
SYSCALL_DEFINE2 (ptree, struct prinfo __user *, buf, int __user *, nr)
{
    int32_t k_nr;
    int32_t count = 0;
    int32_t tmp_int32 = 0;
    struct prinfo *k_buf;
    struct prinfo *dummy_ptr;
    struct prinfo storage;
    struct list_head *pos;
    struct list_head *q;
    struct task_list *current_item;

    LIST_HEAD(tasks_to_visit);

    /* NULL Pointer check for parameters buf and nr */
    if (!buf || !nr) {
        printk(KERN_ERR "[PROJ1] buf or nr has null pointer.\n");
        return -EINVAL;
    }

    /* check user space's address of nr is safe.
     * access_ok return nonzero when address is safe. */
    if(!access_ok(VERIFY_WRITE, nr, sizeof(int32_t)))
    {
        printk(KERN_ALERT
                "[PROJ1] int32_t nr's address 0x%p "
                "in userspace is unsafe. exit ptree syscall.\n",
                nr);
        return -EFAULT;
    }

    /* copy nr from user space */
    tmp_int32 = copy_from_user((void*) &k_nr, (void*) nr, sizeof(int32_t));
    if (tmp_int32) {
        printk(KERN_ERR "[PROJ1] could not copy nr from userspace.\n");
        return -EFAULT;
    }

    /* return -EINVAL when nr is invalid */
    if (k_nr < 1) {
        printk(KERN_ERR "[PROJ1] invalid nr value: %d .\n", k_nr);
        return -EINVAL;

    }
    /* allocate memory for prinfo struct array
     * which will temporarily store process information */
    k_buf = (struct prinfo*) kmalloc(sizeof(struct prinfo) * k_nr, GFP_KERNEL);
    if (!k_buf) {
        printk(KERN_ERR "[PROJ1] kmalloc for struct prinfo *buf failed.\n");
        return -EFAULT;
    }

    read_lock(&tasklist_lock);

    /* DFS starts from swapper(pid:0) task */
    add_new_task(&init_task, &tasks_to_visit);
    
    while(!list_empty(&tasks_to_visit)) {

        /* retrieve first item in the list */
        current_item = list_entry(tasks_to_visit.next, struct task_list, list);

        dummy_ptr = (count++ < k_nr) ? &k_buf[count - 1] : &storage;
        
        if (prinfo_constructor(dummy_ptr, current_item->task)) {
            printk(KERN_ERR "[PROJ1] prinfo constructor error - skipping.\n");
            continue;
        }

        list_del(&(current_item->list));

        if(!list_empty_careful(&(current_item->task->children))) {

            /* 
             * iterate *backwards* in children and push each task into STACK
             * to maintain original order of children.
             */
            list_for_each_prev_safe(pos, q, &(current_item->task->children)) {
                add_new_task(
                        list_entry(pos, struct task_struct, sibling),
                        &tasks_to_visit);
            }
        }
        kfree(current_item);
    }

    read_unlock(&tasklist_lock);

    /* update nr */
    if (count < k_nr) {
        k_nr = count;

        /* check user space's address of nr is safe.
         * access_ok return nonzero when address is safe.*/
        if(!access_ok(VERIFY_WRITE, nr, sizeof(int32_t)))
        {
            printk(KERN_ALERT
                    "[PROJ1] int32_t nr's address 0x%p "
                    "in userspace is unsafe. exit ptree syscall.\n",
                    nr);
            del_tasklist(&tasks_to_visit);
            kfree(k_buf);
            return -EFAULT;
        }
        
        tmp_int32 = copy_to_user((void*) nr, (void*) &k_nr, sizeof(int32_t));
        
        if (!tmp_int32) {
            printk(KERN_ERR "[PROJ1] could not update user variable nr.\n");
        }

    }
    /* check user space's address of user buf is safe.
     * access_ok return nonzero when address is safe. */
    if(!access_ok(VERIFY_WRITE, buf, sizeof(struct prinfo) * k_nr))
    {
        printk(KERN_ALERT
                "[PROJ1] struct prinfo buf's address 0x%p"
                "in userspace is unsafe. exit ptree syscall.\n",
                buf);
        del_tasklist(&tasks_to_visit);
        kfree(k_buf);
        return -EFAULT;
    }

    /* copy k_buf to user buf */
    tmp_int32 = copy_to_user(
            (void*) buf, (void*) k_buf, sizeof(struct prinfo) * k_nr);
    if (tmp_int32) {
        printk(KERN_ERR
                "[PROJ1] could not copy %d bytes to user space.\n",
                tmp_int32);
    }

    /* free tasks_to_visit */
    del_tasklist(&tasks_to_visit);
    /* free prinfo buffer */
    kfree(k_buf);

    return count;
}

