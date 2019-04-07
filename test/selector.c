#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SYSCALL_ROTLOCK_WRITE 400
#define SYSCALL_ROTUNLOCK_WRITE 402

int main(int argc, char** argv)
{
    int input;
    long sysret;

    FILE *intfile;

    if (argc != 2) // Check argument
    {
        fprintf(stderr, "selector: 1 integer argument is required.\n");
        return 1;
    }

    input = atoi(argv[1]);

    while (1) {
        if (syscall(SYSCALL_ROTLOCK_WRITE, 90, 90))
        {
            fprintf(stderr, "selector: Failed to request write lock.\n");
            return 1;
        }

        intfile = fopen("integer", "w");
        fprintf(intfile, "%d", input);
        fclose(intfile);

        printf("selector: %d\n", input++);

        if (syscall(SYSCALL_ROTUNLOCK_WRITE, 90, 90))
        {
            fprintf(stderr, "selector: Failed to release write lock.\n");
            return 1;
        }
    }

    return 0;
}
