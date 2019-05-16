#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/time.h>

#define SYSCALL_SCHED_SETSCHEDULER 156
#define SYSCALL_SETWEIGHT 398
#define SYSCALL_GETWEIGHT 399

#define SCHED_WRR 7

#define PRIME1 86028121
#define PRIME PRIME1


//
// weight, cpu
//

void prime_factorization(int num)
{
    for (int div = 2; div < num; div++)
    {
        if (num % div == 0)
        {
            num /= div;
            div--;
            continue;
        }
    }
}

int main(int argc, char** argv)
{
    int cpu, weight;
    double diff;
    struct timeval start, end;
    cpu_set_t  mask;

    if (argc != 3) {
        fprintf(stderr, "Weight Test: invalid argument\n");
        return 1;
    }
    else {
        weight = atoi(argv[1]);
        cpu = atoi(argv[2]);
    }
    
    printf("Weight Test Launched on CPU %d\n", cpu);

    struct sched_param param; 
    param.sched_priority = 0;
    
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);

    if (sched_setscheduler(0 , SCHED_WRR, &param)) {
        fprintf(stderr, "Weight Test: failed setting schedule policy to wrr\n");
        return 1;
    }


    if(sched_setaffinity(0, sizeof(mask), &mask)) {
        fprintf(stderr, "Weight Test: Failed to set affinity CPU %d\n", cpu);
    }
    

    if(syscall(SYSCALL_SETWEIGHT, 0, weight)) {
        fprintf(stderr, "Weight Test: Failed to set weight %d on CPU %d\n", weight, cpu);
    }

    fflush(stdout);

    gettimeofday(&start, NULL);
    prime_factorization(PRIME);
    gettimeofday(&end, NULL);

    diff = end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec)/1000000;
    printf("aCPU %d    WEIGHT %d    TIME %.6lf\n", cpu, weight, diff);

    return 0;
}
