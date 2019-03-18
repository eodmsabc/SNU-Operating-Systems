#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/prinfo.h>

int main(void) {
    printf("Project 1 Test Program Started.\n");

    struct prinfo *p;
    int nr = 0;
    int retval;
    int tab_level = 1;
    int *prev_pid;

    printf("insert nr value to test ptree call. nr : ");
    scanf("%d",&nr);

    prev_pid = (int *)malloc(sizeof(int) * (nr + 1));
    if(prev_pid == NULL)
    {
        printf("malloc failed. test program terminated\n");
        return 1;
    }

    p = (struct prinfo *)malloc(sizeof(struct prinfo) * (nr + 1));
    if(p == NULL)
    {
        printf("malloc failed. test program terminated\n");
        return 1;
    }

    prev_pid[0] = 0;
    
    for (int i = 0; i < nr; i++) {
        p[i].state = p[i].pid = p[i].parent_pid = p[i].first_child_pid = p[i].next_sibling_pid = p[i].uid = i;
        p[i].comm[0] = 'a';
        p[i].comm[1] = '0' + i;
        p[i].comm[2] = '\0';
    }

    retval = syscall(398, p, &nr);

    printf("comm  pid  state  p_pid  fc_pid  ns_pid  uid\n");
    printf("%s, %d, %lld, %d, %d, %d, %lld\n", p[0].comm, p[0].pid, p[0].state, p[0].parent_pid, p[0].first_child_pid, p[0].next_sibling_pid, p[0].uid);
    prev_pid[tab_level - 1] = p[0].pid;

    for (int i = 1; i < nr; i++) {
        if(prev_pid[tab_level - 1] != p[i].parent_pid) {
            if(p[i - 1].pid == p[i].parent_pid) {
                tab_level++;
                prev_pid[tab_level - 1] = p[i - 1].pid;
            }
            else {
                while(prev_pid[tab_level - 1] != p[i].parent_pid) {
                    tab_level--;
                }
            }
        }
        for(int j = 0; j < tab_level; j++) printf("\t");
        printf("%s, %d, %lld, %d, %d, %d, %lld\n", p[i].comm, p[i].pid, p[i].state, p[i].parent_pid, p[i].first_child_pid, p[i].next_sibling_pid, p[i].uid);
    }

    printf("\nReturn Value : %d\n", retval);
    printf("Current nr's Value : %d\n", nr);

    free(prev_pid);

    printf("Project 1 Test Program Finished.\n");
    return 0;
}
