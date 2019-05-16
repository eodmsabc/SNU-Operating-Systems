#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// copied from /include/uapi/linux/sched/types.h
struct sched_param {
	int sched_priority;
};

#define SYSCALL_SCHED_SETSCHEDULER 156
#define SYSCALL_SETWEIGHT 398
#define SYSCALL_GETWEIGHT 399

#define SCHED_WRR 7

#define PRIME1 7368787
#define PRIME PRIME1

#define FORK_DEPTH 2

void prime_factorization(int id, int num)
{
    printf("trial-%d: %d = ", id, num);

    for (int div = 2; div < num; div++)
    {
        if (num % div == 0)
        {
            num /= div;
            printf("%d * ", div);
            div--;
            continue;
        }
    }
    printf("%d\n", num);
}

int main(int argc, char** argv)
{
    int id, depth, i;
    pid_t pid;
    unsigned int num = 1;
    setbuf(stdout, NULL);

    id = 0;
    depth = 0;

    struct sched_param param; 
    param.sched_priority = 0;
    // SYSCALL_DEFINE3(sched_setscheduler, pid_t, pid, int, policy, struct sched_param __user *, param)
    if (syscall(SYSCALL_SCHED_SETSCHEDULER, 0 , SCHED_WRR, &param)) {
        fprintf(stderr, "trial-%d: failed setting schedule policy to wrr", id);
        return 1;
    }

    for (i = 0; i < FORK_DEPTH; i++) {
        if ((pid = fork())) // parent
        {
            depth++;
            id = id * 2;
        }
        else    // child
        {
            depth++;
            id = id * 2 + 1;
        }
    }

    while(num) {
        prime_factorization(id, PRIME);
        num++;
    }

    return 0;
}
