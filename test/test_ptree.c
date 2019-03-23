#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/prinfo.h>

void print_prinfo(struct prinfo p) {
    printf("%s, %d, %lld, %d, %d, %d, %lld\n", p.comm, p.pid, p.state, p.parent_pid, p.first_child_pid, p.next_sibling_pid, p.uid);
}

int main(void) {
    printf("Project 1 Test Program Started.\n");

    struct prinfo *p;
    int nr = 0;
    int orig_nr;
    int retval;
    int tab_level = 1;
    int *prev_pid;

    printf("insert nr value to test ptree call. nr : ");
    scanf("%d", &nr);
    orig_nr = nr;

    /* previous pid array to maintain tab allignment */
    prev_pid = (int *)malloc(sizeof(int) * (nr + 1));
    if (!prev_pid) {
        printf("malloc failed. test program terminated\n");
        return 1;
    }

    /* prinfo array */
    p = (struct prinfo *)malloc(sizeof(struct prinfo) * (nr + 1));
    if (!p) {
        printf("malloc failed. test program terminated\n");
        return 1;
    }

    /* Initialize prinfo array */
    for (int i = 0; i < nr; i++) {
        p[i].state = p[i].pid = p[i].parent_pid = p[i].first_child_pid = p[i].next_sibling_pid = p[i].uid = i;
        sprintf(p[i].comm, "Process %d", i);
    }

    /* ptree syscall */
    retval = syscall(398, p, &nr);

    printf("comm  pid  state  p_pid  fc_pid  ns_pid  uid\n");
    print_prinfo(p[0]);
    prev_pid[tab_level - 1] = p[0].pid;

    for (int i = 1; i < nr; i++) {
        if (prev_pid[tab_level - 1] != p[i].parent_pid) {
            if (p[i - 1].pid == p[i].parent_pid) {
                tab_level++;
                prev_pid[tab_level - 1] = p[i - 1].pid;
            }
            else {
                while (prev_pid[tab_level - 1] != p[i].parent_pid) {
                    tab_level--;
                }
            }
        }

        for (int j = 0; j < tab_level; j++) printf("\t");

        print_prinfo(p[i]);
    }

    /* print result */
    printf("\nReturn Value : %d\n", retval);
    printf("Input nr :%d, Current nr: %d\n", orig_nr, nr);

    free(prev_pid);
    free(p);

    printf("Project 1 Test Program Finished.\n");
    return 0;
}
