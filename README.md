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

rotd와 selector를 실행한 상태에서 trial 0, 1, 2, 3까지 4개를 사용하였다.

그 결과중 일부분을 가져왔다.
```
selector: 5328
trial-1: 5328 = 2 * 2 * 2 * 2 * 3 * 3 * 37
trial-0: 5328 = 2 * 2 * 2 * 2 * 3 * 3 * 37
selector: 5329
trial-0: 5329 = 73 * 73
trial-1: 5329 = 73 * 73
selector: 5330
trial-1: 5330 = 2 * 5 * 13 * 41
trial-0: 5330 = 2 * 5 * 13 * 41
selector: 5331
trial-0: 5331 = 3 * 1777
trial-1: 5331 = 3 * 1777
trial-2: 5331 = 3 * 1777
selector: 5332
trial-1: 5332 = 2 * 2 * 31 * 43
trial-2: 5332 = 2 * 2 * 31 * 43
selector: 5333
trial-1: 5333 = 5333
trial-2: 5333 = 5333
selector: 5334
trial-1: 5334 = 2 * 3 * 7 * 127
trial-2: 5334 = 2 * 3 * 7 * 127
selector: 5335
trial-1: 5335 = 5 * 11 * 97
trial-2: 5335 = 5 * 11 * 97
selector: 5336
trial-2: 5336 = 2 * 2 * 2 * 23 * 29
selector: 5337
trial-2: 5337 = 3 * 3 * 593
trial-1: 5337 = 3 * 3 * 593
selector: 5338
trial-1: 5338 = 2 * 17 * 157
trial-2: 5338 = 2 * 17 * 157
selector: 5339
trial-2: 5339 = 19 * 281
trial-1: 5339 = 19 * 281
selector: 5340
trial-1: 5340 = 2 * 2 * 3 * 5 * 89
trial-2: 5340 = 2 * 2 * 3 * 5 * 89
selector: 5341
trial-1: 5341 = 7 * 7 * 109
trial-2: 5341 = 7 * 7 * 109
selector: 5342
trial-1: 5342 = 2 * 2671
trial-2: 5342 = 2 * 2671
selector: 5343
trial-1: 5343 = 3 * 13 * 137
trial-2: 5343 = 3 * 13 * 137
selector: 5344
trial-2: 5344 = 2 * 2 * 2 * 2 * 2 * 167
trial-1: 5344 = 2 * 2 * 2 * 2 * 2 * 167
selector: 5345
trial-1: 5345 = 5 * 1069
trial-2: 5345 = 5 * 1069
selector: 5346
trial-2: 5346 = 2 * 3 * 3 * 3 * 3 * 3 * 11
trial-1: 5346 = 2 * 3 * 3 * 3 * 3 * 3 * 11
selector: 5347
trial-1: 5347 = 5347
trial-2: 5347 = 5347
selector: 5348
trial-2: 5348 = 2 * 2 * 7 * 191
trial-1: 5348 = 2 * 2 * 7 * 191
selector: 5349
trial-1: 5349 = 3 * 1783
selector: 5350
trial-2: 5350 = 2 * 5 * 5 * 107
trial-1: 5350 = 2 * 5 * 5 * 107
selector: 5351
trial-2: 5351 = 5351
trial-1: 5351 = 5351
selector: 5352
trial-1: 5352 = 2 * 2 * 2 * 3 * 223
trial-2: 5352 = 2 * 2 * 2 * 3 * 223
....
```




## Lessons Learned



* 커널에서 lock이 어떻게 동작하는지 파악할 수 있었다.
* lock을 실제로 구현하면서, 좋은 디자인 패턴이 정말 중요함을 느꼈다. 교과서에 있는 구현패턴을 보지 않았으면 락을 구현하는데 굉장히 오래 걸렸을 것이다.
* lock을 구현할때, safety와 performance를 둘다 만족하게 구현하는것이 까다로운 일임을 깨달았다.
* simple is best라는것을 다시한번 느끼게 되었다. 특히 락같이 여러 오브젝트를 고려해야 하는 경우에는 전체 그림을 그리기가 까다로워서, 간편하고 범용적인 구현에서 조금씩 구체화 해가는 식으로 락을 구현하였다.

* OS 프로젝트를 진행할때는 빠른 시작이 중요함을 뼈저리게 느낄 수 있었다. 데드라인에 닥쳐서 구현을 시작했으면 정말 큰일날 뻔했다.
