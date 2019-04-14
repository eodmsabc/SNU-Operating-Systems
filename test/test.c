#define SYSCALL_SET_ROTATION 398
#define SYSCALL_ROTLOCK_READ 399
#define SYSCALL_ROTLOCK_WRITE 400
#define SYSCALL_ROTUNLOCK_READ 401
#define SYSCALL_ROTUNLOCK_WRITE 402

#include <signal.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int daemon_rot;

void term(int signum)
{
    printf("daemon killed with rotation set to %d\n", daemon_rot);
    _exit(0);
}

int spawn_daemon(int init_degree, int block_size, int cycles)
{
    struct sigaction a;
    int degree = init_degree;
    int count;
    int pid = fork();

    if(pid == 0) {
        a.sa_handler = term;
        sigaction(SIGTERM, &a, NULL);
        for(count = 0; count < ((int)(360 / block_size) + 1) * cycles; count++) {
            daemon_rot = degree = (degree + block_size) % 360;
            syscall(SYSCALL_SET_ROTATION, degree);
            sleep(1);
        }
        printf("daemon finished with rotation set to %d\n", daemon_rot);
        _exit(0);
    }
    return pid;
}

int main()
{
    int input;
    int degree;
    int range;
    int rotation;
    int res;
    int b;
    int c;
    int pid = -1;
    int status = 0;
    int lock_count = 0;

    printf("manual-test program start. insert -1 to exit.\n");
    while(1)
    {
        printf("insert mode to test.\n0 : start daemon\n1 : set_rotation , 2 : rotlock_read, 3 : rotlock_write, \n4 : rotunlock_read, 5 : rotunlock_write, 9 : kill daemon\n");
        scanf("%d", &input);
        
        switch(input)
        {
            case -1:
                return 0;
            case 0:
                if(waitpid(pid, &status, WNOHANG) == 0) {
                /* if(!WIFEXITED(status)) { */
                    printf("there is already a daemon running.\n");
                    break;
                }
                printf("set_rotation. insert init degree to set : ");
                scanf("%d", &rotation);
                printf("insert degree block size : ");
                scanf("%d", &b);
                printf("insert cycles. if -1, cycle defaults to current lock count : ");
                scanf("%d", &c);
                if(c < 0) c = lock_count;
                pid = spawn_daemon(rotation, b, c);
                printf("daemon pid: %d\n", pid);
                break;
            case 1:
                printf("manual set_rotation. insert degree to set : ");
                scanf("%d", &rotation);
                res = syscall(SYSCALL_SET_ROTATION, rotation);
                printf("return value : %d\n", res);
                break;
            case 2:
                printf("rotlock_read. insert degree and range to lock : ");
                scanf("%d %d", &degree, &range);
                res = syscall(SYSCALL_ROTLOCK_READ, degree, range);
                lock_count++;
                printf("return value : %d\n", res);
                break;
            case 3:
                printf("rotlock_write. insert degree and range to lock : ");
                scanf("%d %d", &degree, &range);
                res = syscall(SYSCALL_ROTLOCK_WRITE, degree, range);
                lock_count++;
                printf("return value : %d\n", res);
                break;

            case 4:
                printf("rotunlock_read. insert degree and range to unlock : ");
                scanf("%d %d", &degree, &range);
                res = syscall(SYSCALL_ROTUNLOCK_READ, degree, range);
                lock_count--;
                printf("return value : %d\n", res);
                break;

            case 5:
                printf("rotunlock_write. insert degree and range to unlock : ");
                scanf("%d %d", &degree, &range);
                res = syscall(SYSCALL_ROTUNLOCK_WRITE, degree, range);
                lock_count--;
                printf("return value : %d\n", res);
                break;
            case 9:
                if(waitpid(pid, &status, WNOHANG) == 0) {
                /* if(!WIFEXITED(status)) { */
                    kill(pid, SIGTERM);
                    printf("daemon %d killed\n", pid);
                }
                else {
                    printf("daemon %d is already finished\n", pid);
                }
                pid = -1;
                break;

            default:
                printf("insert correct value.\n");
        }
        
    }
    return 0;
}
