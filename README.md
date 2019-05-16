# OS/TEAM 10 - PROJ2


## Let's Build a Kernel


커널 이미지 & 부트 이미지 빌드 + sd카드에 플래싱하기까지의 과정을 `/kernelbootflash.sh`라는 한 개의 스크립트로 통합했다.

##### Usage of `/kernelbootflash.sh`

```bash
$ ./kernelbootflash.sh <SD card device node>
```

* sudo 를 요구한다.
* SD card device node 인자를 주지 않으면 플래싱은 생략. (생략 확인 시 y 입력 후 Enter 로 진행)
* 주의: 빌드 시 모든 것이 root 권한으로 생성되어서 이전 빌드에서 생성된 파일/디렉토리를 덮어쓰는 부분 때문에 추후에 다시 빌드하려면 sudo가 필요하다.
* *단순 편의 목적의 스크립트이므로 이 커널을 빌드할 때 필수적으로 요구되는 요소는 아니다.*


##### how to build test code

/test 폴더에 테스트 코드 빌드를 위한 Makefile을 만들어 두었다.

```bash
$ make
```
를 통해 trial 테스트 프로그램을 빌드할 수 있고, make clean을 통해 만들어진 프로그램을 정리할 수 있다.



## Project Overview & Goals

타이젠 커널에 weighted round robin 스케줄 policy를 구현하고, sched_setweight, sched_getweight 시스템 콜을 추가해서

각 시스템콜과 스케줄러가 제대로 동작하도록 하는것이 이 프로젝트의 목표이다.


weighted round robin 스케줄 (wrr schedule) policy

각 task는 자신에게 해당하는 weight를 가지고, 이 weight에 비례하는 timeslice를 가지고 실행된다.

만약 현재 time slice를 모두 사용했다면, time slice를 다시 보충받고 큐의 맨 끝으로 되돌아가서 자신의 차례가 오기를 기다리게 된다.


sched_setweight : weight를 바꿀 wrr policy 프로세스의 pid와 weight를 받아서, weight를 바꿔주게 된다.
만약 task의 policy가 WRR이 아니라면, 에러를 리턴한다.

sched_getweight: wrr policy 프로세스의 pid를 받아서, 그 task의 weight를 리턴한다.
만약 task의 policy가 WRR이 아니라면, 에러를 리턴한다.



## High Level Design

각 task들은 default weight로 10을 가지고, weight는 1~20의 값이 가능하다.

timeslice는 10ms * weight에 해당하는 값을 가지게 되고, task가 timeslice를 다 쓰게되면 그 weight에 해당하는 만큼의 timeslice를 다시 채우게 된다.




## Our Implementation


##### wrr_runqueue와 wrr_entity 구현

task의 policy를 wrr policy로 만들어주고 관리하기 위해서는, 

runqueue에 wrr_runqueue 필드를 만들어주고, 또한 task에 wrr policy 필드를 만들어 줄 필요가 있었다.

따라서 다음과 같은 구조체를 선언한다음, 각각 struct rq와 struct task_struct 의 필드로 만들어주었다.


'''
struct wrr_rq {
    struct list_head queue;

    int usable; // if this value is zero, cpu doesn't use wrr_scheduling.
    int weight_sum;

	raw_spinlock_t wrr_runtime_lock;
};
'''

'''
struct sched_wrr_entity {
	struct list_head run_list;

    int on_rq; // if queued in wrr_rq, set 1 else set zero.
	int	time_slice;
	int    weight;
};
'''

wrr_rq : wrr policy인 task를 관리하기 위한 자료구조이다.
  queue : task들을 저장한다.
  weight_sum : queue의 총 weight를 저장한다.
  usable : 값이 0이면 이 cpu는 사용되지 않는다.

sched_wrr_entity : wrr policy를 갖는 task를 구현하기 위한 task_struct의 필드이다.
	weight : task의 weight이다. 기본값은 10.
	time_slice : 현재 task에게 남아있는 time slice이다.
	on_rq : 현재 runqueue에 올라와있는지를 체크한다.
	

##### wrr_sched_class 구현 

우리가 만든 policy를 적용하기 위해서는 새로운 sched class를 구현할 필요가 있었다.

우리가 구현한 sched 클래스는 다음과 같은 메소드들을 가진다.


'''
const struct sched_class wrr_sched_class = {

    .next               = &fair_sched_class,ㄴ
    .enqueue_task       = enqueue_task_wrr,
    .dequeue_task       = dequeue_task_wrr,
    .yield_task         = yield_task_wrr,
    .yield_to_task = yield_to_task_wrr,
    .check_preempt_curr = check_preempt_curr_wrr,
    .pick_next_task     = pick_next_task_wrr,
    .put_prev_task      = put_prev_task_wrr,
    .set_curr_task      = set_curr_task_wrr,
    .task_tick          = task_tick_wrr,
    .task_fork          = task_fork_wrr,
    .task_dead          = task_dead_wrr,
    .get_rr_interval    = get_rr_interval_wrr,
    .prio_changed       = prio_changed_wrr,
    .switched_to        = switched_to_wrr,
    .update_curr        = update_curr_wrr,

    .select_task_rq   = select_task_rq_wrr,
    .set_cpus_allowed = set_cpus_allowed_common,
    .rq_online        = rq_online_wrr,
    .rq_offline       = rq_offline_wrr,
    .task_woken       = task_woken_wrr,
    .switched_from    = switched_from_wrr
};
'''
enqueue_task : wrr queue에 새로운 task를 추가한다.
dequeue_task : wrr queue에 있던 기존 task를 제거한다.
pick_next_task : wrr queue에서 현재 실행되어야 할 태스크를 리턴한다. 우리의 구현에서는 큐의 제일 맨 앞에 매달려있는 태스크를 리턴한다.
task_tick : 주기적으로 call되어 현재 task의 time slice를 감소시키는 역할을 한다. 만약 timeslice가 모두 사용되었다면, 태스크의 time slice를 보충하고 큐의 맨 뒤로 보낸다.
select_task_rq : 새로운 task가 실행될 cpu를 리턴한다.  가장 wrr runqueue의 weight가 작은 cpu를 리턴한다.
task_fork : task가 fork되었을때 새로운 task의 weight와 timeslice등을 설정해준다.
switched_to : task가 다른 policy에서 wrr policy로 바뀌었을때 가질 weight를 설정해준다.


##### load balancing 구현

로드밸런싱을 하기위해서, core.c의 scheduler_tick에 다음과같이 wrr 로드밸런싱을 하는 함수를 부르는 로직을 추가했다.

'''
#ifdef CONFIG_SMP
	rq->idle_balance = idle_cpu(cpu);
	trigger_load_balance(rq);
    trigger_load_balance_wrr(rq);
#endif
'''
이 trigger load balance wrr를 통해서 로드밸런싱을 진행하게 된다.

이 코드는 wrr.c에 다음과 같이 구현하였다.

1. 마지막 로드밸런싱 시간으로부터 얼마나 지났는지를 체크한다. 만약 2000ms가 지나지 않았다면, 로드밸런싱을 진행하지 않고 그대로 리턴한다.
2. 2000ms 가 지났다면, 로드밸런싱을 진행한다. 먼저 런큐중 가장 weight가 높은 런큐와 weight가 낮은 런큐의 포인터를 받아온다.
3. 성공적으로 런큐를 찾았고, weight가 높은 런큐와 낮은 런큐의 weight가 같지 않다면 로드밸런싱을 진행한다. 
4. 먼저 double_rq_lock을 통해 런큐들의 락을 잡고, weight가 높은 런큐의 task들을 순회하면서 weight가 낮은 런큐로 이동할 수 있는 task를 찾는다.
5. 만약 이동이 가능한 task가 있다면, 그중 가장 weight가 큰 task를 이동시키고, 없다면 락을 풀고 종료시킨다.
6. task를 이동시킨다음, 락을 풀고 종료시킨다.


##### cpu runqueue 하나 비우기 구현

우리의 구현에서, cpu runqueue중 무조건 하나에는 wrr task가 할당될 수 없으므로, 다음과 같은 방법을 통해 CPU중 하나에 wrr task를 할당할 수 없게 하였다.

현재 구현에서는 CPU 0을 사용하지 않는다.

이를 구현하기 위해, wrr.c 에서 runqueue를 initialize할때 다음과 같이 현재 cpu에 따라 usable값을 설정해준다.

'''
void init_wrr_rq(struct wrr_rq *wrr_rq, int cpu)
{

    INIT_LIST_HEAD(&(wrr_rq->queue));
    
    wrr_rq->weight_sum = 0;

    if (cpu == WRR_NO_USE_CPU_NUM) {
        wrr_rq->usable = 0;
    }
    else {
        wrr_rq->usable = 1;
    }

    print_errmsg("initialize_wrr_rq", rq_of_wrr_rq(wrr_rq));
    
    raw_spin_lock_init(&(wrr_rq->wrr_runtime_lock));
}
'''

또한, 더 확실하게 cpu를 비우기 위해, core.c의 __sched_setscheduler 초반부에 다음과 같은 로직을 추가했다.

'''
if(policy == SCHED_WRR) // if policy is SCHED_WRR, it is could'nt move to other than cpu number zero, return error value.
{
	int wrr_retval=-1;

	const struct cpumask *no_use_cpu_mask = (cpumask_of(WRR_NO_USE_CPU_NUM));
	struct cpumask mask;
	if(cpumask_andnot(&mask, &(p->cpus_allowed), no_use_cpu_mask) == 0) return -EFAULT;

	wrr_retval=migrate_task_if_wrr(p);
	if(wrr_retval != 0) return wrr_retval;
}
'''은
이 로직은 만약 setscheduler를 통해 wrr policy로 전환을 요청하는 태스크가 있다면,
먼저 그 task가 쓰지 않는 cpu에서만 실행될 수 있는지 판단한다음, 그렇다면 policy 전환이 불가능 하다고 보고 에러를 리턴한다.
만약 그렇지 않다면, migrate_task_if_wrr() 함수를 통해 task를 다른 cpu로 migrate하는 과정을 거친다.

'''
// migrate task if policy is wrr. return 0 if success, else return not zero.
static int migrate_task_if_wrr(struct task_struct *p)
{
    struct rq *rq = task_rq(p);
    struct rq *lowest_rq; 
    
    rcu_read_lock();
    lowest_rq = find_lowest_weight_rq(p);
    rcu_read_unlock();

    if(cpu_of(rq) == WRR_NO_USE_CPU_NUM) // must migrate other queue
    {
        if(lowest_rq == NULL) //coudln't migrate. return error value.
        {
            print_errmsg("there is no cpu to run this rq.", rq);
            return -EFAULT;
        }
        else
        {
            print_errmsg("this cpu is not runnable. enqueue to other cpu", rq);
            return set_affinity_and_migrate_wrr(p, cpu_of(lowest_rq));
        }
    }
    else
    {   
        if(lowest_rq && ((lowest_rq->wrr.weight_sum < (rq->wrr).weight_sum))) // enqueue lowest queue.
        {
            print_errmsg("enqueue to other cpu", rq);
            return set_affinity_and_migrate_wrr(p, cpu_of(lowest_rq));
        }
        else
        {
            const struct cpumask *no_use_cpu_mask = (cpumask_of(WRR_NO_USE_CPU_NUM));
            struct cpumask p_mask; 
            struct cpumask mask;

            if(sched_getaffinity(p->pid, &p_mask) != 0) return -1;     // get p's affinity
            if(cpumask_andnot(&mask, &p_mask, no_use_cpu_mask) == 0) return -1; // if there is no cpu to run..
            return sched_setaffinity(p->pid, &mask);  // set p's affinity. return set_affinity return value. (0 is success.)
        }
    }

    return -EFAULT;
}
'''
migrate_task_if_wrr 함수에서는, 태스크가 실행될 수 있는 cpu중, 가장 weight가 낮은 cpu의 runqueue를 받아온다.
그 후, 만약 현재 task의 runqueue를 보고 이 태스크를 migrate 해야하는지 아닌지를 결정한다.
migrate하기로 결정되었다면, task의 affinity를, 쓰지 않는 cpu에 할당될 수 없도록 설정한다음, 가장 낮은 weight의 runqueue로 migrate하는 과정을 거치게 된다.
그렇지 않다면 그냥 affinity만 바꿔준다.

이를 통해 실행되어서는 안되는 cpu로 wrr task가 들어가는 경우를 차단하였다.
(fork시에도 affinity는 물려받기 때문에, 특별히 affinity를 바꾸지 않는한 실행될 이유가 없고, usable값을 통해 한번 더 실행되지 말아야할 cpu에서 실행되는 경우를 막아준다.



## Test Results 

load balancing

동영상에 찍어놓았지만, 간단하게 wrr 스케줄러에 추가된 순서대로 결과를 정리하면

1. pid 546 : weight 10. cpu 3에 할당됨. 
cpu 0 : 0, cpu 1 : 0, cpu 2 : 0, cpu 3 : 10
2. pid 547 : weight 10. cpu 1에 할당됨.
cpu 0 : 0, cpu 1 : 10, cpu 2 : 0, cpu 3 : 10
3. pid 548 : weight 10. cpu 2에 할당됨.
cpu 0 : 0, cpu 1 : 10, cpu 2 : 10, cpu 3 : 10
-> 이를 통해 weight가 작은 cpu부터 차근차근 할당됨을 알 수 있다.
4. pid 549 : weight 10. cpu 1에 할당됨.
cpu 0 : 0, cpu 1 : 20, cpu 2 : 10, cpu 3 : 10

5. pid 549 : 다시 wrr 스케줄링 요구. 스케줄링 할때 가장 weight가 낮은 cpu에 할당되므로 
             weight 10, cpu 2로 옮겨짐. 
cpu 0 : 0, cpu 1 : 10, cpu 2 : 20, cpu 3 : 10
6. pid 549 : weight 5로 감소시킴. 
cpu 0 : 0, cpu 1 : 10, cpu 2 : 15, cpu 3 : 10
7. pid 557 : weight 10, cpu 1에 할당됨.
cpu 0 : 0, cpu 1 : 20, cpu 2 : 15, cpu 3 : 10
8. pid 558 : weight 10, cpu 3에 할당됨.
cpu 0 : 0, cpu 1 : 20, cpu 2 : 15, cpu 3 : 20
9. pid 558 : weight 1로 감소시킴.
cpu 0 : 0, cpu 1 : 20, cpu 2 : 15, cpu 3 : 11

10. pid 546 : weight 1로 감소시킴.
cpu 0 : 0, cpu 1 : 20, cpu 2 : 15, cpu 3 : 2
-> 현재 가장 높은 cpu에는 weight 10짜리가 2개 있으므로, 로드밸런싱이 일어나면 대소관계가 뒤바뀌므로 로드밸런싱이 일어나지 않는다.

11. pid 547 : weight 1로 감소시킴.
cpu 0 : 0, cpu 1 : 11, cpu 2 : 15, cpu 3 : 2

12. 로드밸런싱 트리거. pid 549가 cpu 2 -> cpu 3으로 이동
cpu 0 : 0, cpu 1 : 11, cpu 2 : 10, cpu 3 : 7

13. 로드밸런싱 트리거. pid 547이 cpu 1 -> cpu 3으로 이동
cpu 0 : 0, cpu 1 : 10, cpu 2 : 10, cpu 3 : 8

-> 이를 통해 로드밸런싱이 정상적으로 동작하는것을 알 수 있다.


weight에 따른 실행시간.

단일 코어에서 weight 10인, spin wait하는 dummy process와,

86028121 라는 큰 값을 소인수 분해하는 테스트 프로그램을 사용하였다.

이에 대한 그래프는 plot.pdf에 그려져있다.

weight  time(s)
1	86.999484
2	47.60062
3	34.396204
4	27.795138
5	23.796095
6	21.194521
7	19.294304
8	17.895371
9	16.794255
10	16.009548
11	15.209408
12	14.610621
13	14.110443
14	13.71028
15	13.309673
16	13.0104
17	12.709766
18	12.409714
19	12.210243
20	12.00985


보다시피, 전체 time slice중에 테스트 프로그램이 차지하는 비중이 늘어나면 늘어날수록
실행시간이 빨라지는것을 알 수 있다.

이의 그래프를 그려보면, 실제 실행시간이 c * (weight)/(10 + weight) 의 그래프와 비례하는것을 알 수 있다.




## Lessons Learned



* 커널에서, 실제 스케줄러가 어떻게 구현되었는지를 파악할 수 있었다.
* 버그 하나때문에 30시간 넘게 시간을 날린적이 있는데, 커널 코드에 대한 완벽한 이해가 있었다면 일어나지 않을 버그였다. 다시는 이러한 버그가 일어나지 않도록 해야겠다는 생각이 들었다.
* 스펙의 미흡함때문에 fair scheduler 관련해서 커널패닉이 일어날때가 종종 있었는데, 이러한 경우에 어떻게 대처해야할지를 잘 알수가 없었다.
* 버그를 고치고자 코드 전체를 뜯어고치고 나서도 작동을 하지 않은적이 있었는데, 이런 경험은 다시는 하고싶지 않은 경험이었다.
* 이번 프로젝트는 유난히 시간을 많이들였고, 일찍 시작했음에도 불구하고 마지막까지 너무나 힘들었다. 커널을 개발하는 사람들도 이러한 어려움을 느꼈을거라 생각하니 마음이 숙연해졌다.




