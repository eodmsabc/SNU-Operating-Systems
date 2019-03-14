#ifndef __PROJ1_LINUX_PTREE_H__
#define __PROJ1_LINUX_PTREE_H__

struct prinfo {
    int64_t state;
    pid_t   pid;
    pid_t   parent_pid;
    pid_t   first_child_pid;
    pid_t   next_sibling_pid;
    int64_t uid;
    char    comm[64];
};

#endif
