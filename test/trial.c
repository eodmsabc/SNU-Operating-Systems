#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#define SYSCALL_ROTLOCK_READ 399
#define SYSCALL_ROTUNLOCK_READ 401

int file_exist (char *filename)
{
    struct stat buffer;
    return (stat (filename, &buffer) == 0);
}

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
    int num;
    
    FILE *intfile;

    if (argc != 2) // Check argument
    {
        fprintf(stderr, "trial: 1 integer argument is required.\n");
        return 1;
    }

    id = atoi(argv[1]);

    while (1) {
        if (syscall(SYSCALL_ROTLOCK_READ, 90, 90))
        {
            fprintf(stderr, "trial-%d: Failed to request read lock.\n", id);
            return 1;
        }

        if (file_exist("integer"))
        {
            intfile = fopen("integer", "r");
            fscanf(intfile, "%d", &num);
            fclose(intfile);

            prime_factorization(id, num);
        }

        if (syscall(SYSCALL_ROTUNLOCK_READ, 90, 90))
        {
            fprintf(stderr, "trial-%d: Failed to release read lock.\n", id);
            return 1;
        }
    }

    return 0;
}