#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <sys/syscall.h>

#define SYSCALL_SETWEIGHT 398
#define SYSCALL_GETWEIGHT 399
#define SCHED_WRR 7
#define UINPUT_LEN 32
#define P_MAX_LEN 1024
#define P_EMPTY -7

void init_pid_list(int *plist)
{
	int i;
	for(i=0; i<P_MAX_LEN; i++)
		plist[i] = P_EMPTY;
}

int add_pid_list(int *plist, int pid)
{
	int i;
	for(i=0; i<P_MAX_LEN; i++)
	{
		if(plist[i] == P_EMPTY)
		{
			plist[i] = pid;
			return i+1;
		}
	}
	return 0;

}

int del_pid_list(int *plist, int pid)
{
	int i;
	for(i=0; i<P_MAX_LEN; i++)
	{
		if(plist[i] == pid)
		{
			plist[i] = P_EMPTY;
			return i+1;
		}
	}
	return 0;
}

void print_pid_list(int *plist)
{
	int i;
	int count;

	printf("=======================================\n");
	printf("[MODE : PRINT PID LIST]\n");
	for(i=0,count=1; i<P_MAX_LEN; i++)
	{
		if(plist[i] != P_EMPTY)
		{
			printf("%d\t: %d\n", count, plist[i]);
			count++;
		}
	}
	printf("=======================================\n");
}

int main(int argc)
{
	char cinput[UINPUT_LEN];
	int dinput;
	int pid_list[P_MAX_LEN];
	int pid;
	int weight;
	int sys_val;
	int i;

	struct sched_param param;

	init_pid_list(pid_list);
	while(1)
	{
		printf("\n\n\n\n");
		printf("=======================================\n");
		printf("n : new,  k : kill,  p : pids,  l : log\n");
		printf("s : set weight, g : get weight\n");
		printf("r : sched_scheduler,   z : kill all,   x : exit\n");
		printf("input : ");
		scanf("%s", cinput);

		switch(cinput[0])
		{
			case 'n':
				pid = fork();
				if(pid == 0) //child
				{
					execl("./proj3_worker", "proj3_worker", NULL);
					printf("=======================================\n");
					printf("pid : %d failed\n", pid);
					printf("=======================================\n");
				}
				else //parent
				{
					if(add_pid_list(pid_list, pid))
					{

						printf("=======================================\n");
						printf("pid : %d added\n", pid);
						printf("=======================================\n");
					}
					else
					{
						kill(pid, SIGKILL);
						printf("=======================================\n");
						printf("pid : %d failed\n", pid);
						printf("=======================================\n");
					}
				}
				break;
			case 'k':
				printf("pid to kill: ");
				scanf("%d", &dinput);

				if(del_pid_list(pid_list, dinput))
				{
					kill(dinput, SIGKILL);
					printf("=======================================\n");
					printf("pid : %d killed\n", dinput);
					printf("=======================================\n");
				}
				else
				{
					printf("=======================================\n");
					printf("pid : %d failed\n", dinput);
					printf("=======================================\n");
				}
				break;
			case 'p':
				print_pid_list(pid_list);
				break;
			case 'l':
				printf("=======================================\n");
				system("cat /proc/sched_debug");
				printf("\n\n");
				printf("=======================================\n");
				break;
			case 's':
				printf("pid to set weight: ");
				scanf("%d", &dinput);

				printf("weight : ");
				scanf("%d", &weight);
				sys_val = syscall(SYSCALL_SETWEIGHT, dinput, weight);

				if(sys_val == weight)
				{
					printf("=======================================\n");
					printf("pid : %d success\n", dinput);
					printf("=======================================\n");
				}
				else
				{
					printf("=======================================\n");
					printf("pid : %d failed\n", dinput);
					printf("=======================================\n");
				}
				break;
			case 'g':
				printf("pid to get weight: ");
				scanf("%d", &dinput);

				sys_val = syscall(SYSCALL_GETWEIGHT, dinput);

				if(sys_val>0)
				{
					printf("=======================================\n");
					printf("pid : %d, weight : %d success\n", dinput, sys_val);
					printf("=======================================\n");
				}
				else
				{
					printf("=======================================\n");
					printf("pid : %d failed\n", dinput);
					printf("=======================================\n");
				}
				break;
			case 'r':
				printf("pid to set wrr : ");
				scanf("%d", &dinput);

				param.sched_priority = 0;

				sys_val = sched_setscheduler((pid_t)dinput, SCHED_WRR, &param);

				if(sys_val == 0)
				{
					printf("=======================================\n");
					printf("pid : %d success\n", dinput);
					printf("=======================================\n");
				}
				else
				{
					printf("=======================================\n");
					printf("pid : %d failed, error : %d\n", (pid_t)dinput, sys_val);
					printf("=======================================\n");
				}
				break;
			case 'z':
				for(i=0; i<P_MAX_LEN;i++)
					if(pid_list[i] != P_EMPTY)
					{
						kill(pid_list[i], SIGKILL);
						pid_list[i] = P_EMPTY;
					}
				printf("=======================================\n");
				printf("PROCESS ZENOCIDE DONE\n\n\n");
				printf("=======================================\n");
				break;
			case 'x':
				printf("=======================================\n");
				printf("Bye!\n\n\n");
				printf("=======================================\n");
				return 0;
			default:
				break;
		}
	}
	return -1;
}
