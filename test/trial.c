#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SYSCALL_SCHED_SETSCHEDULER 156
#define SYSCALL_SETWEIGHT 398
#define SYSCALL_GETWEIGHT 399

#define SCHED_WRR 7

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
    unsigned int num = 2;

    if (argc > 2) {
        fprintf(stderr, "trial: invalid argument\n");
        return 1;
    }
    else {
        id = (argc == 2)? atoi(argv[1]) : 0;
    }

    if (syscall(SYSCALL_SCHED_SETSCHEDULER, 0, SCHED_WRR, 0)) {
        fprintf(stderr, "trial-%d: failed setting schedule policy to wrr", id);
        return 1;
    }

    while(num) {
        prime_factorization(id, num);
        num++;
    }

    return 0;
}
