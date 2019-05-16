#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>

// copied from /include/uapi/linux/sched/types.h

#define SYSCALL_SCHED_SETSCHEDULER 156
#define SYSCALL_SETWEIGHT 398
#define SYSCALL_GETWEIGHT 399

#define SCHED_WRR 7

#define PRIME1 1299709
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
    cpu_set_t mask;

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

    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    CPU_SET(cpu, &mask);

    sched_setaffinity(0, sizeof(mask), &mask);

    while(num) {
        prime_factorization(id, PRIME);
        num++;
    }

    return 0;
}
