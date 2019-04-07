#include <stdio.h>
#include <sys/syscall.h>

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
        if (rotlock_write(90, 90))
        {
            fprintf(stderr, "selector: Failed to request write lock.\n");
            return 1;
        }

        intfile = fopen("integer", "w");
        fprintf(intfile, "%d", input);
        fclose(intfile);

        printf("selector: %d\n", input++);

        if (rotunlock_write(90, 90))
        {
            fprintf(stderr, "selector: Failed to release write lock.\n");
            return 1;
        }
    }

    return 0;
}
