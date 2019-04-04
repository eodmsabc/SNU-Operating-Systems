#include <linux/completion.h>
#include <linux/types.h>
#include <linux/list.h>

struct rotation_lock {
    pid_t pid;  // save lock's caller's pid.
    int degree;
    int range;
    struct completion comp;         // save completion
    struct list_head list;          // list member
};