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
#include <rotation.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/completion.h>


#define DEGREE_ADJUST(x) (((x) < 0) ? ((x) + 360) : ((x) % 360))
// adjust degree to get in 0~359 value.
#define ABS(x) (((x) < 0) ? (-(x)) : (x))

static spinlock_t lock=SPIN_LOCK_UNLOCKED;  // lock initialize
unsinged long flags;    // used with spinlock interrupt

//DEFINE_MUTEX(lock);

/*
spin_lock_irqsave(&mr_lock, flags);

spin_unlock_irqrestore(&mr_lock, flags);
void mutex_lock(struct mutex *lock);
    int mutex_lock_interruptible(struct mutex *lock);
    int mutex_trylock(struct mutex *lock);
void mutex_unlock(struct mutex *lock);
*/

static int current_lotation = 0;    // 0 ~ 359 
static int current_lock_state[360] = {0,}; 
/* save current lock state of degree.
 * zero value means that lotation is free.
 * negative value (always -1) means that lotation hold write lock.
 * positive value (1,2, ... n) means that lotation hold read lock. value means reader's number.
 *                             ex) [10,30] reader locked, and [20,25] reader locked,
 *                                 current_lock_state[10~19] = 1, [20~25] = 2, [26~30] = 1, other = 0 
 */

static LIST_HEAD(writer_waiting_list);
static LIST_HEAD(reader_waiting_list);
static LIST_HEAD(writer_active_list);
static LIST_HEAD(reader_active_list);



"""
Consumer:
mutex_lock (&lock);
while (!condition) {
mutex_unlock (&lock);
wait_event (wq, condition);
mutex_lock (&lock);
}
/* do whatever */
mutex_unlock (&lock);

Producer:
mutex_lock (&lock);
/* modify the condition: may be as simple as... */
condition = 1;
mutex_unlock (&lock);
wake_up (&wq);
"""



// check value is in degree and range. true, return 1, else, return 0
int value_in_area(int degree, int range, int value)
{
    if(range == 180) return 1;

    int upper = DEGREE_ADJUST(degree + range);
    int lower = DEGREE_ADJUST(degree + range);

    if(upper >= lower)
    {
        if(value >= lower && value <= upper) return 1;
        else return 0;
    }
    else
    {
        if(value > upper && value < lower) return 0;
        else return 1;
    }
}

// if two range overlap, then return 1, else return 0.
int two_range_overlap(int degree1, int range1, int degree2, int range2)
{
    if(range1 == 180 || range2 == 180) return false;
    else
    {
        int upper1 = DEGREE_ADJUST(degree1 + range1);
        int lower1 = DEGREE_ADJUST(degree1 - range1);

        int upper2 = DEGREE_ADJUST(degree2 + range2);
        int lower2 = DEGREE_ADJUST(degree2 - range2);

        if(upper1 >= lower1 && upper2 >= lower2)      //two region doesn't above 360 degree.
        {
            if(ABS(degree1 - degree2) > range1 + range2)) return 0;
            else return 1;
        }
        else if(upper1 < lower1 && upper2 >= lower2)    // 1st object above 360 degree.
        {
            if((upper1 < lower2) && (upper2 < lower1)) return 0;
            else return 1;
        }
        else if(upper1 >= lower1 && upper2 < lower2)    // 2nd object above 360 degree.
        {
            if((upper2 < lower1) && (upper1 < lower2)) return 0;
            else return 1;
        }
        else    // two object also above 360 degree.
        {
            return 1;
        }
        
    }
    
}


// check current rotation is in degree and range. true, return 1, else, return 0.
// need lock before use.
int rotation_in_area(int degree, int range)
{
    int ret=0;
    spin_lock_irqsave(&lock, flags);
    if(value_in_area(degree, range, current_lotation)) ret = 1;
    else ret = 0;
    spin_unlock_irqrestore(&lock, flags);
    return ret;
}

// fill rotation_lock. if success, return 0
int fill_node(struct rotation_lock *node, int degree, int range)
{

    node->pid = current->pid;   // to find owner, save pid. 
    node->degree = degree;
    node->range = range;
    init_completion(&(node->comp));
    INIT_LIST_HEAD(&(node->list));

    return 0;
}


// remove and get rotation_lock from list by using degree, range and current pid. if can't find lock, return NULL
struct rotation_lock *pop_node(int degree, int range, struct list_head *header)
{
    struct rotation_lock *curr;

    pid_t owner = current->pid;
    list_for_each_entry(curr, header, list)   // after item delete, loop ended.
    {
        if(curr->pid == owner)
        {
            if ((curr->degree == degree) && (curr->range == range))
            {
                list_del(&(curr->list));    // now find lock, remove this and return.
                return curr;
            }
        }
    }

    return NULL;
}




int attach_node(struct rotation_lock *node, )


// if no waiting writer in current rotation, return 1 else return 0.
int check_no_waiting_writer_in_current_rotation()
{
    struct rotation_lock *curr;
    int curr_range;
    int curr_degree;

    list_for_each_entry(curr, writer_waiting_list, list)   
    {
        curr_degree = curr->degree;
        curr_range = curr->range;

        if(value_in_area(curr_degree, curr_range, current_lotation))    // waiting writer in current rotation. return 0.
        {
            return 0;
        }
    }

    return 1;   // no wating writer.
}

// if reader could get lock at current lock state, return 1 else return 0.
// if lock state is zero or positive, could get lock.
int check_reader_range_free(int degree, int range)
{
    int i;
    int deg;
    for(i = degree - range; i <= degree + range; i++)
    {
        deg = DEGREE_ADJUST(i);
        if(current_lock_state[deg] < 0) return 0;
    }
    return 1;
}

// if writer could get lock at current lock state, return 1 else return 0.
// if lock state is zero, could get lock
int check_writer_range_free(int degree, int range)
{
    int i;
    int deg;
    for(i = degree - range; i <= degree + range; i++)
    {
        deg = DEGREE_ADJUST(i);
        if(current_lock_state[deg] != 0) return 0;
    }
    return 1;
}

// check reader should lock. if reader could get lock, return 1. else reader couldn't get lock, return 0 
int reader_should_go(struct rotation_lock *rot_lock)
{
    int degree = rot_lock->degree;
    int range = rot_lock->range;
    if(value_in_area(degree, range, current_lotation))  // check current rotation in rot_lock's area.
    {
        if(check_no_waiting_writer_in_current_rotation() && check_reader_range_free(degree, range))    // check no wating writer in current rotation, and check reader can lock. 
        {
            return 1;
        }
    }
    return 0;
}

// check writer should lock. if writer could get lock, return 1. else writer couldn't get lock, return 0 
int writer_should_go(struct rotation_lock *rot_lock)
{
    int degree = rot_lock->degree;
    int range = rot_lock->range;

    if(value_in_area(degree, range, current_lotation))  // check current rotation in rot_lock's area.
    {
        if(check_writer_range_free(degree, range))    // check writer can lock. 
        {
            return 1;
        }
    }
    return 0;
}


//change current state, and add rotation lock to reader_active_list. if terminate normally, return 0.
int read_lock_active(struct rotation_lock *rot_lock)
{
    int i;
    int deg;
    int degree = rot_lock->degree;
    int range = rot_lock->range;

    if(range!=180)  // set current state = current state + 1.
    {
        for(i = degree - range; i <= degree + range; i++)
        {
            deg = DEGREE_ADJUST(i);
            if(current_lock_state[deg] < 0)       // critical error!!!!
            {
                printk(KERN_ERR "[PROJ2] in read_lock_active, current state of lock is corrupted!! deg = %d, state = %d\n", deg, current_lock_state[deg]);
                return 1;
            }
            current_lock_state[deg]++;
        }
    }
    else
    {
        for(i=0; i<360; i++)
        {
            if(current_lock_state[i] < 0)       // critical error!!!!
            {
                printk(KERN_ERR "[PROJ2] in read_lock_active, current state of lock is corrupted!! i = %d, state = %d\n", i, current_lock_state[i]);
                return 1;
            }
            current_lock_state[i]++;
        }
    }
    // setup current state ended.

    list_add(&(rot_lock->list), reader_active_list); //add to reader active list.

    return 0;
}

//change current state, and add rotation lock to writer_active_list. if terminate normally, return 0.
int write_lock_active(struct rotation_lock *rot_lock)
{
    int i;
    int deg;
    int degree = rot_lock->degree;
    int range = rot_lock->range;

    if(range!=180)  // set current state = -1.
    {
        for(i = degree - range; i <= degree + range; i++)
        {
            deg = DEGREE_ADJUST(i);
            if(current_lock_state[deg] != 0)       // critical error!!!!
            {
                printk(KERN_ERR "[PROJ2] in write_lock_active, current state of lock is corrupted!! deg = %d, state = %d\n", deg, current_lock_state[deg]);
                return 1;
            }
            current_lock_state[deg] = -1;
        }
    }
    else
    {
        for(i=0; i<360; i++)
        {
            if(current_lock_state[i] != 0)       // critical error!!!!
            {
                printk(KERN_ERR "[PROJ2] in write_lock_active, current state of lock is corrupted!! i = %d, state = %d\n", i, current_lock_state[i]);
                return 1;
            }
            current_lock_state[i] = -1;
        }
    }
    // setup current state ended.

    list_add(&(rot_lock->list), writer_active_list); //add to writer active list.

    return 0;
}


//change current state. if terminate normally, return 0.
int read_lock_release(struct rotation_lock *rot_lock)
{   
    int i;
    int deg;
    int degree = rot_lock->degree;
    int range = rot_lock->range;

    if(range!=180)  // set current state = current state + 1.
    {
        for(i = degree - range; i <= degree + range; i++)
        {
            deg = DEGREE_ADJUST(i);
            if(current_lock_state[deg] <= 0)       // critical error!!!!
            {
                printk(KERN_ERR "[PROJ2] in read_lock_release, current state of lock is corrupted!! deg = %d, state = %d\n", deg, current_lock_state[deg]);
                return 1;
            }
            current_lock_state[deg]--;
        }
    }
    else
    {
        for(i=0; i<360; i++)
        {
            if(current_lock_state[i] <= 0)       // critical error!!!!
            {
                printk(KERN_ERR "[PROJ2] in read_lock_release, current state of lock is corrupted!! i = %d, state = %d\n", i, current_lock_state[i]);
                return 1;
            }
            current_lock_state[i]--;
        }
    }
    // setup current state ended.

    return 0;
}

//change current state. if terminate normally, return 0.
int write_lock_release(struct rotation_lock *rot_lock)
{
    int i;
    int deg;
    int degree = rot_lock->degree;
    int range = rot_lock->range;

    if(range!=180)  // set current state = -1.
    {
        for(i = degree - range; i <= degree + range; i++)
        {
            deg = DEGREE_ADJUST(i);
            if(current_lock_state[deg] >= 0)       // critical error!!!!
            {
                printk(KERN_ERR "[PROJ2] in write_lock_release, current state of lock is corrupted!! deg = %d, state = %d\n", deg, current_lock_state[deg]);
                return 1;
            }
            current_lock_state[deg] = 0;
        }
    }
    else
    {
        for(i=0; i<360; i++)
        {
            if(current_lock_state[i] >= 0)       // critical error!!!!
            {
                printk(KERN_ERR "[PROJ2] in write_lock_release, current state of lock is corrupted!! i = %d, state = %d\n", i, current_lock_state[i]);
                return 1;
            }
            current_lock_state[i] = 0;
        }
    }
    // setup current state ended.

    return 0
}

void inform_writer_at_current_lotation()
{

}

void inform_reader_at_current_lotation()
{

}

/*
 * sets the current device rotation in the kernel.
 * syscall number 398 (you may want to check this number!)
 */
SYSCALL_DEFINE1 (set_rotation, int __user, degree)
{
    if(degree >= 360 || degree < 0)
    {
        printk(KERN_ERR "[PROJ2] degree is not correct value.\n");
        return -EINVAL;
    }
    
    spin_lock_irqsave(&lock, flags); // get lock and disable interrupts

    current_lotation = degree;
    inform_writer_at_current_lotation();
    inform_reader_at_current_lotation();

    spin_unlock_irqrestore(&lock, flags); // disable lock and enable interrupts
    
    return 0;
}

/*
 * Take a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 399 and 400
 */

SYSCALL_DEFINE2 (rotlock_read, int __user, degree, int __user, range)
{
    struct rotation_lock *rot_lock;
    int retval;

    if(degree >= 360 || degree < 0)
    {
        printk(KERN_ERR "[PROJ2] degree is not correct value.\n");
        return -EINVAL;
    }

    if(range > 180 || range < 0)
    {
        printk(KERN_ERR "[PROJ2] range is not correct value.\n");
        return -EINVAL;
    }

    rot_lock = (struct rotation_lock *) kmalloc(sizeof(struct rotation_lock), GFP_KERNEL);

    if (!rot_lock) {
        printk(KERN_ERR
                "[PROJ2] kmalloc for new item in read lock has failed.\n");
        return -EFAULT;
    }

    retval = fill_node(rot_lock, degree, range);
    if(retval != 0)
    {
        printk(KERN_ERR
                "[PROJ2] filling node has failed.\n");
        return -EFAULT;
    }

    spin_lock_irqsave(&lock, flags); // get lock and disable interrupts
    
    list_add(&(rot_lock->list), reader_waiting_list); //add to read waiting list.
    while(!reader_should_go(rot_lock))
    {
        spin_unlock_irqrestore(&lock, flags);
        wait_for_completion(&(rot_lock->comp));
        spin_lock_irqsave(&lock, flags);
    }
    list_del(&(rot_lock->list));   // delete from read waiting list.

    retval = read_lock_active(rot_lock);     //change current state, and add rotation lock to reader_active_list
    if(retval != 0)     
    {
        spin_unlock_irqrestore(&lock, flags);
        printk(KERN_ERR "[PROJ2] read_lock_active has failed.\n");
        return -EFAULT;
    }

    spin_unlock_irqrestore(&lock, flags);
    return 0;
}

SYSCALL_DEFINE2 (rotlock_write, int __user, degree, int __user, range)
{
    struct rotation_lock *rot_lock;

    if(degree >= 360 || degree < 0)
    {
        printk(KERN_ERR "[PROJ2] degree is not correct value.\n");
        return -EINVAL;
    }

    if(range > 180 || range < 0)
    {
        printk(KERN_ERR "[PROJ2] range is not correct value.\n");
        return -EINVAL;
    }

    rot_lock = (struct rotation_lock *) kmalloc(sizeof(struct rotation_lock), GFP_KERNEL);

    if (!rot_lock) {
        printk(KERN_ERR
                "[PROJ2] kmalloc for new item in read lock has failed.\n");
        return -EFAULT;
    }

    retval = fill_node(rot_lock, degree, range);
    if(retval != 0)
    {
        printk(KERN_ERR
                "[PROJ2] filling node has failed.\n");
        return -EFAULT;
    }

    spin_lock_irqsave(&lock, flags); // get lock and disable interrupts
    list_add(&(rot_lock->list), writer_waiting_list); //add to write waiting list.
    while(!writer_should_go(rot_lock))
    {
        spin_unlock_irqrestore(&lock, flags);
        wait_for_completion(&(rot_lock->comp));
        spin_lock_irqsave(&lock, flags);
    }
    list_del(&(rot_lock->list));   // delete from writer waiting list.

    retval = write_lock_active(rot_lock);   //change current state, and add rotation lock to writer_active_list
    if(retval != 0)     
    {
        spin_unlock_irqrestore(&lock, flags);
        printk(KERN_ERR "[PROJ2] write_lock_active has failed.\n");
        return -EFAULT;
    }

    spin_unlock_irqrestore(&lock, flags);
    return 0;
}


/*
 * Release a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 401 and 402
 */

SYSCALL_DEFINE2 (rotunlock_read, int __user, degree, int __user, range)
{
    if(degree >= 360 || degree < 0)
    {
        printk(KERN_ERR "[PROJ2] degree is not correct value.\n");
        return -EINVAL;
    }

    if(range > 180 || range < 0)
    {
        printk(KERN_ERR "[PROJ2] range is not correct value.\n");
        return -EINVAL;
    }

    struct rotation_lock *rot_lock;
    struct rotation_lock *waiting_writer;
    struct rotation_lock *waiting_reader;

    spin_lock_irqsave(&lock, flags);

    rot_lock = pop_node(degree, range, reader_active_list);
    
    if(rot_lock == NULL)
    {
        spin_unlock_irqrestore(&lock, flags);
        printk(KERN_ERR "[PROJ2] there is no active readlock by this degree and range!\n");
        return -EFAULT;
    }

    read_lock_release(rot_lock) //change current state.
    
    if(waiting_writer = iswatingWriter())
    {
        complete(&(waiting_writer->comp));
    }
    
    spin_unlock_irqrestore(&lock, flags);
    kfree(rot_lock); // free rot_lock.
    return 0;
}

SYSCALL_DEFINE2 (rotunlock_write, int __user, degree, int __user, range)
{
    if(degree >= 360 || degree < 0)
    {
        printk(KERN_ERR "[PROJ2] degree is not correct value.\n");
        return -EINVAL;
    }

    if(range > 180 || range < 0)
    {
        printk(KERN_ERR "[PROJ2] range is not correct value.\n");
        return -EINVAL;
    }

    struct rotation_lock *rot_lock;
    struct rotation_lock *waiting_writer;
    struct rotation_lock *waiting_reader;

    spin_lock_irqsave(&lock, flags);

    rot_lock = pop_node(degree, range, writer_active_list);
    
    if(rot_lock == NULL)
    {
        spin_unlock_irqrestore(&lock, flags);
        printk(KERN_ERR "[PROJ2] there is no active readlock by this degree and range!\n");
        return -EFAULT;
    }
    
    write_lock_release() //change current state.

    if(waiting_writer = iswatingWriter())
    {
        complete(&(waiting_writer->comp));
    }
    else if(waiting_reader = iswaitingReader())
    {
        complete(&(waiting_reader->comp));
    }
    spin_unlock_irqrestore(&lock, flags);

    kfree(rot_lock); // free rot_lock.
    return 0;
}
