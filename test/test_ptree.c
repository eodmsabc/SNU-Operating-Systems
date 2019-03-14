#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <linux/prinfo.h>

#define PRINFO_ARRAY_SIZE 16

int main(void) {
    printf("Project 1 Test Program Started.\n");

    struct prinfo p[PRINFO_ARRAY_SIZE];
    int nr = 10;

    /* Initialize prinfo array */
    for (int i = 0; i < PRINFO_ARRAY_SIZE; i++) {
        p[i].state = p[i].pid = p[i].parent_pid = p[i].first_child_pid = p[i].next_sibling_pid = p[i].uid = i;
        sprintf(p[i].comm, "process %d", i);
    }

    int retval = 12345;
    retval = syscall(398, p, &nr);
    
    for (int i = 0; i < nr; i++) {
        printf("%s, %d, %lld, %d, %d, %d, %lld\n", p[i].comm, p[i].pid, p[i].state, p[i].parent_pid, p[i].first_child_pid, p[i].next_sibling_pid, p[i].uid);
    }

    printf("Return Value : %d\n", retval);
    perror("perror : ");

    printf("Project 1 Test Program Finished.\n");
    return 0;
}
