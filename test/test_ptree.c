#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <linux/prinfo.h>

#define NR_MAX 16

void print_prinfo(struct prinfo p) {
    printf("%s, %d, %lld, %d, %d, %d, %lld\n", p.comm, p.pid, p.state, p.parent_pid, p.first_child_pid, p.next_sibling_pid, p.uid);
}

int main(void) {
    printf("Project 1 Test Program Started.\n");

    struct prinfo p[NR_MAX];
    int nr = NR_MAX;
    int retval = 1;
    int tab_level = 1;
    int prev_pid[NR_MAX] = {0, };

    /* Initialize prinfo array */
    for (int i = 0; i < NR_MAX; i++) {
        p[i].state = p[i].pid = p[i].parent_pid = p[i].first_child_pid = p[i].next_sibling_pid = p[i].uid = i;
        sprintf(p[i].comm, "Process %d", i);
    }

    retval = syscall(398, p, &nr);
    
    printf("comm  pid  state  p_pid  fc_pid  ns_pid  uid\n");
    print_prinfo(p[0]);
    prev_pid[tab_level - 1] = p[0].pid;

    for (int i = 0; i < nr; i++) {
        if (prev_pid[tab_level - 1] != p[i].parent_pid) {
            if (p[i - 1].pid == p[i].parent_pid) {
                tab_level++;
                prev_pid[tab_level - 1] = p[i - 1].pid;
            }
            else {
                while(prev_pid[tab_level - 1] != p[i].parent_pid) {
                    tab_level--;
                }
            }
        }
        for (int j = 0; j < tab_level; j++) printf("\t");
        print_prinfo(p[i]);
    }

    printf("Return Value : %d\n", retval);

    printf("Project 1 Test Program Finished.\n");
    return 0;
}
