#define SYSCALL_SET_ROTATION 398
#define SYSCALL_ROTLOCK_READ 399
#define SYSCALL_ROTLOCK_WRITE 400
#define SYSCALL_ROTUNLOCK_READ 401
#define SYSCALL_ROTUNLOCK_WRITE 402

#include <signal.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main()
{
    int input;
    int degree;
    int range;
    int rotation;
    int res;
    printf("manual-test program start. insert -1 to exit.\n");
    while(1)
    {
        printf("insert mode to test. 1 : set_rotation , 2 : rotlock_read, \n3 : rotlock_write, 4 : rotunlock_read, 5 : rotunlock_write : ");
        scanf("%d", &input);
        
        switch(input)
        {
            case -1:
                return 0;
            case 1:
                printf("set_rotation. insert rotation to set : ");
                scanf("%d", &rotation);
                res = syscall(SYSCALL_SET_ROTATION, rotation);
                printf("return value : %d\n", res);
                break;
            case 2:
                printf("rotlock_read. insert degree and range to lock : ");
                scanf("%d %d", &degree, &range);
                res = syscall(SYSCALL_ROTLOCK_READ, degree, range);
                printf("return value : %d\n", res);
                break;
            case 3:
                printf("rotlock_write. insert degree and range to lock : ");
                scanf("%d %d", &degree, &range);
                res = syscall(SYSCALL_ROTLOCK_WRITE, degree, range);
                printf("return value : %d\n", res);
                break;

            case 4:
                printf("rotunlock_read. insert degree and range to unlock : ");
                scanf("%d %d", &degree, &range);
                res = syscall(SYSCALL_ROTUNLOCK_READ, degree, range);
                printf("return value : %d\n", res);
                break;

            case 5:
                printf("rotunlock_write. insert degree and range to unlock : ");
                scanf("%d %d", &degree, &range);
                res = syscall(SYSCALL_ROTUNLOCK_WRITE, degree, range);
                printf("return value : %d\n", res);
                break;

            default:
                printf("insert correct value.\n");
        }
        
    }
    return 0;
}