#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>

#define SYSCALL_SETWEIGHT 398
#define SYSCALL_GETWEIGHT 399

#define SCHED_WRR 7

//  Usage
// ./dummy weight cpu

int main(int argc, char** argv)
{
    int cpu = -1, weight = 0;
    unsigned int num = 1;

    cpu_set_t  mask;

    if (argc > 3) {
        fprintf(stderr, "dummy: invalid argument\n");
        return 1;
    }
    else {
        if (argc >= 2) {
            weight = atoi(argv[1]);
        }
        if (argc == 3) {
            cpu = atoi(argv[2]);
        }
    }
    
    printf("Dummy process started\n");

    struct sched_param param; 
    param.sched_priority = 0;
    
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);

    if (sched_setscheduler(0, SCHED_WRR, &param)) {
    //if (syscall(SYSCALL_SCHED_SETSCHEDULER, 0 , SCHED_WRR, &param)) {
        fprintf(stderr, "Dummy process failed to change schedule policy to wrr\n");
        return 1;
    }
    printf("Dummy process schedule policy changed to WRR\n");

    if (weight) {
        if(syscall(SYSCALL_SETWEIGHT, 0, weight)) {
            fprintf(stderr, "Dummy process failed to change CPU mask %d\n", cpu);
        }
        printf("Dummy process WRR weight is now %d\n", weight);
    }

    if (cpu != -1) {
        if(sched_setaffinity(0, sizeof(mask), &mask)) {
            fprintf(stderr, "Dummy process failed to change CPU mask %d\n", cpu);
        }
        printf("Dummy process is running with policy WRR on CPU %d\n", cpu);
    }

    for(;;);

    return 0;
}