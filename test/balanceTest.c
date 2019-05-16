#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>

// copied from /include/uapi/linux/sched/types.h
/* struct sched_param { */
/* 	int sched_priority; */
/* }; */

#define SYSCALL_SCHED_SETSCHEDULER 156
#define SYSCALL_SETWEIGHT 398
#define SYSCALL_GETWEIGHT 399

#define SCHED_WRR 7

#define PRIME1 7368787
#define PRIME PRIME1

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
    int id;
    int cpu;
    unsigned int num = 1;

    cpu_set_t  mask, mask2;

    if (argc > 3) {
        fprintf(stderr, "trial: invalid argument\n");
        return 1;
    }
    else {
        id = (argc == 3)? atoi(argv[2]) : 0;
        cpu = atoi(argv[1]);
    }
    
    printf("trial-%d: launched", id);
    fflush(stdout);
    struct sched_param param; 
    param.sched_priority = 0;
    // SYSCALL_DEFINE3(sched_setscheduler, pid_t, pid, int, policy, struct sched_param __user *, param)
    
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);

    CPU_ZERO(&mask2);
    CPU_SET(1, &mask2);
    CPU_SET(2, &mask2);
    CPU_SET(3, &mask2);

    printf("trial-%d: setting cpu..\n", id);
    fflush(stdout);

    if (syscall(SYSCALL_SCHED_SETSCHEDULER, 0 , SCHED_WRR, &param)) {
        fprintf(stderr, "trial-%d: failed setting schedule policy to wrr", id);
        return 1;
    }
    printf("scheduler changed\n");


    if(sched_setaffinity(0, sizeof(mask), &mask)) {
        fprintf(stderr, "trial-%d: failed setting cpu to %d\n", id, cpu);
    }
    printf("proper affinity set finished\n");


    if(sched_setaffinity(0, sizeof(mask2), &mask2)) {
        fprintf(stderr, "trial-%d: failed setting cpu to %d\n", id, cpu);
    }
    printf("recover affinity finished\n");


    if(syscall(SYSCALL_SETWEIGHT, 0, 20)) {
        fprintf(stderr, "trial-%d: failed setting weight to 20\n", id);
    }
    else {
        printf("trial-%d: task weight set to %d\n", id, 20);
    }

    fflush(stdout);

    while(num) {
        prime_factorization(id, PRIME);
        num++;
    }

    return 0;
}
