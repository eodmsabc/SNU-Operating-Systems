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
를 통해 rotd, selector, trial 테스트 프로그램을 빌드할 수 있고, make clean을 통해 만들어진 프로그램을 정리할 수 있다.



## Project Overview & Goals

타이젠 커널에 set_rotation, rotlock_read, rotlock_write, rotunlock_read, rotunlock_write

이 5개의 시스템 콜을 추가하고, 각 시스템콜이 제대로 동작하도록 하는것이 이 프로젝트의 목표이다.


set_rotation : 새 rotation값을 받아서, 커널 내부에 저장되어있는 rotation 값을 새 rotation값으로 바꿔준다.

rotlock_read : 커널로부터 read rotation lock을 받아온다.

rotlock_write : 커널로부터 write rotation lock을 받아온다.

rotunlock_read : 받아온 read rotation lock을 해제한다.

rotunlock_write : 받아온 write rotation lock을 해제한다.



각 rotation lock은 degree와 range로 이루어진 고유의 area를 가지고 있으며, read lock끼리는 서로 area가 중첩될 수 있지만, write lock은 다른 write lock이나 read lock과 area가 중첩되어서는 안된다.

또한, write lock은 절대 starvation 되어서는 안된다.













## High Level Design


먼저, user-space에서 사용하는 lock과 condition variable을 이용해서 가볍게 큰 그림을 그리고,
커널에서 제공하는 lock data structure를 사용해서 생각했던 바를 구현하였다.


작동 로직으로 보면 크게 lock을 거는 part와 lock을 해제하는 part로 나뉘어져 있으며, 
rotation lock의 전체 구조는 다음과 같이 나뉘어진다.

```
struct rotation_lock {
    pid_t pid;  // save lock's caller's pid.
    int degree;
    int range;
    struct completion comp;         // save completion
    struct list_head list;          // list member
}
```

또한 이 lock을 관리하기 위한 global data는 다음과 같다.

global data

1. 전체 data structure를 mutually exclusive 하게 접근하기 위한 mutex (코드에서, my_lock)
2. 현재 rotation 을 저장하는 상수 (current_rotation)
3. 현재 락 상태를 저장하는 길이 360짜리 배열 (current_lock_state).
   reader lock이 걸려있을 경우 양수 (1,2 ... n : reader 개수) , writer lock이 걸려있을경우 음수 (-1)의 값을 가지며, free 상태일 경우 0을 가짐
4. 현재 대기중인 락 요청들을 저장하는 리스트 (writer_waiting_list, reader_waiting_list)
5. 현재 할당된 락들을 저장하는 리스트 (writer_active_list, reader_active_list)



작동 과정은 다음과 같다. 모든 과정은 writer에게 우선순위가 있다.

락 요청

1. 사용자가 rotlock_read/write() call을 통해 read 혹은 write lock 요청을 한다.
2. 해당하는 락 요청을 생성하고, waiting list에 추가한다.
3. while루프를 돌면서, waiting list에서 진행이 가능한경우 (current rotation이 lock 범위에 들어왔고, lock을 할당할 수 있는경우.) 락을 할당하고, 락 요청을 waiting list에서 active list로 옮기고 락을 활성화 한다.
   그렇지 않은 경우 waiting list에서 대기한다.

락 해제

1. active list에서 해당하는 lock을 찾아 해제하고, list에서 제거한다.
2. waiting list에서 현재 rotation에 있는 lock들을 모두 깨워서 후에 wait에서 active로 상태가 전환될수 있도록 한다.


프로세스가 종료하는 경우
1. active list에서 그 프로세스가 요청했던 lock들을 모두 찾아 해제하고, list에서 제거한다.
2. waiting list에서 그 프로세스가 요청했던 lock들을 list에서 제거한다.





## Our Implementation



##### lock request implementation


lock 요청을 할때, 다음 loop를 돌면서 락을 할당할지, 말지를 결정한다.

```
while(!reader_should_go(rot_lock))
    {
        mutex_unlock(&my_lock);
        wait_for_completion(&(rot_lock->comp));
        mutex_lock(&my_lock);
        reinit_completion(&(rot_lock->comp)); // if wake up, completion need to reinitiating because completion has memory of complete(). multiple complete, multiple no-wait.
    }
```

락 요청마다 우리가 구현한 rotation_lock 구조체를 생성한다. rotation_lock 구조체는 내부에 락을 요청한 프로세스의 정보, 락의 정보와 리눅스 lock structure중 하나인 completion을 원소로 가지게 된다.

completion은 wait queue의 단순화된 버전인데, wait_for_completion(&comp)를 통해 comp가 진행하는 일이 끝날때까지 해당 쓰레드를 잠시 재울수 있고, 다른 쓰레드에서 complete(&comp)를 실행할때 wake up 하게 된다.

따라서 위 루프에서 진행조건이 성립되지 않으면 wait을 통해 진행조건이 만족될 가능성이 생길때까지 잠들어 있다가, 깨어난뒤 진행조건을 다시 보고 진행할지 말지를 결정하게 된다.


이때 completion의 경우, user-space에서의 condition variable과 다르게 memoryness를 가지고 있으므로,
여러번 complete 요청을 받고 쓸데없이 계속 일어나는것을 막기 위해 reinit_completion을 통해 반복되는 wakeup을 없애주었다.

completion을 기다리는 문장이 while loop 내부에 들어있으므로, 설사 false-wakeup이 일어나더라도 safety를 보장하게 하였다.

##### unlocking our lock

락을 풀어줄때는 현재 대기하고 있는 락 요청중 어떤 락 요청을 진행시킬지를 정해야 한다.

우리 구현의 핵심만 뽑으면 다음과 같다. writer lock 해제할때의 예시이다.

```
    mutex_lock(&my_lock);

    rot_lock = pop_node(degree, range, &writer_active_list);

    retval = write_lock_release(rot_lock); //change current state.

    inform_writer_at_current_rotation();
    inform_reader_at_current_rotation();

    mutex_unlock(&my_lock);
```

현재 active writer lock list에서 요청받은 lock을 제거하고, writer lock을 풀어준다. (write_lock_release)
그리고 나서, 대기하고 있는 writer와 reader들에게 락이 해제되었음을 알려준다.


1. writer lock이 해제된경우, 현재 대기하고 있는 다른 writer lock이나 reader lock이 새로 할당될 수 있다.
이때, 각 lock은 현재 rotation과 락이 요청한 범위가 맞아야하고, writer lock과 reader lock의 경우
자신이 할당될 수 있는지 현재 상태를 체크하는 과정이 필요한데, 앞에서 락 요청을 구현할때 while loop에
락 요청마다 현재 이 락 요청이 진행할 수 있는지 없는지를 체크하게 해놓았다.
때문에 굳이 락을 풀어줄때 어떤 락 요청이 진행할 수 있을지 검사를 다시 할 필요가 없다고 생각하여,
락이 진행할 수 있는 최소한의 조건만 만족하면 해당되는 락 요청들을 wakeup 하게 하였다.

따라서 락이 요청한 범위내에 현재 rotation이 포함되기만 하면, 해당하는 reader, writer 락 요청들을 모두 wakeup 하게 구현하였다.

2. reader lock이 해제된경우, reader lock의 경우는 다른 reader lock과 lock region을 공유할 수 있기 때문에, reader lock이 해제되었다고 해서 지금까지 할당되지 못했던 reader lock 요청이 진행되는 경우는 존재하지 않는다.

따라서 writer lock이 해제될때와 같이, 락이 요청한 범위내에 현재 rotation이 포함되기만 하면, 해당하는 writer 락 요청들만 모두 wakeup 하게 구현하였다.





##### how to don't starve writer?

이번 프로젝트를 진행하면서 가장 많이 신경썼던 부분은 writer가 starvation 하지 않게 하는 부분이었다.

이를 달성하기 위해서는 어떨때 write lock을 걸수 있는지, read lock을 걸수 있는지 결정해야했다.

따라서, read lock과  write lock 요청에서 다음과 같은 조건으로 진행하게 하였다.

공통 조건으로는,
1. 요청한 락이 current_rotation 범위내에 존재할것
2. 락 요청을 받았을때, 현재 상태가 lock을 할당할수 있는 상태일것. 
   (reader의 경우, read lock의 area에 해당하는 state들의 값이 양수일때,
    writer의 경우, write lock의 area에 해당하는 state들의 값이 0일때)

그리고 reader의 경우, 추가적으로 다음 조건을 검사한다.
3. current_rotation에서 wait하고 있는 write lock 요청이 없을것.

이 조건을 통해, write lock 요청이 대기하고 있을경우, read lock 요청이 어떤순서로 들어오든간에 최우선적으로 write lock에게 우선권을 부여하게 된다. 보통의 reader_writer lock의 경우는 writer보다 먼저 들어온 reader의 경우 writer보다 먼저 lock을 할당받는것이 보장되지만, 우리의 구현에서는
writer보다 먼저 들어온 reader가 있더라도 무조건 writer에게 우선적으로 lock을 할당하게 된다.

이를 통해 writer-starvation 문제를 제거하였다.




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
