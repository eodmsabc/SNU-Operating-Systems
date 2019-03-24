# OS/TEAM 10 - PROJ1


## Let's Build a Kernel

---

바쁘신 조교님들을 위한 영역.

커널 이미지 & 부트 이미지 빌드 + sd카드에 플래싱하기까지 `/kernelbootflash.sh`라는 한 개의 스크립트로 만들어놨습니다.

##### Usage of `/kernelbootflash.sh`

```bash
$ ./kernelbootflash.sh <SD card device node>
```

* sudo 를 요구합니다.
* SD card device node 인자를 주지 않으면 플래싱은 생략합니다. (생략 확인 시 y 입력 후 Enter 로 진행)
* 빌드 시 모든 것이 root 권한으로 생성되기 때문에 빌드 과정에서 덮어쓰는 부분 때문에 추후에 다시 빌드하실 때도 sudo가 필요합니다.
* *단순 편의 목적의 스크립트이므로 저희 팀의 커널을 빌드할 때 필수 요소는 아닙니다.*





## Project Overview & Goals

---

타이젠 커널에 `ptree`라는 시스템 콜을 추가하는 프로젝트이다.

주어진 프로젝트 개요 파일을 잘 읽어보니 `ptree`는:

>  *"실행 중인 프로세스의 목록을 DFS로 탐색하여 주어진 갯수만큼 그 정보를 반환해야 한다."*

```c
int ptree(struct prinfo *buf, int *nr);
```





## High Level Design

---

##### Adding a system call

세부 내용을 구현하기에 앞서, 커널에 우리가 만들 시스템 콜을 잘 새겨넣어보자.

그러기 위해서는 다음과 같이 몇 가지 파일들을 수정해야 한다.

(이 때, 타이젠은 arm64 아키텍처이므로 해당 디렉토리 내의 파일들을 수정했다.)

* `/arch/arm64/include/asm/unistd32.h`

  `ptree` 시스템 콜을 스펙에 따라서 398번으로 추가했다.

* `/arch/arm64/include/asm/unistd.h`

  위에서 398번으로 등록했으니 여기서 전체 시스템 콜의 갯수를 399개로 조정해준다.

* `/arch/arm/tools/syscall.tbl`

  리눅스의 모든 시스템 콜이 등록되어 있는 `System Call Table`에다가 우리의 398번 `ptree`도 입력.

  ~~(사실 왜 이건 `arm64`가 아니라 `arm` 디렉토리 밑에 추가해야하는 지 모르겠다.)~~

* `/include/linux/syscalls.h`

  다른 시스템 콜들처럼 여기에 `ptree` 프로토타입을 넣어주었다.

실제로 커널 소스를 건드는 일은 여기까지.

추가로 `/kernel/Makefile` 을 나중에 만들 `/kernel/ptree.c` 도 컴파일하도록 수정했다.



##### How to implement ptree?

알려주신대로 `/include/linux/prinfo.h` 에다가 `prinfo` 구조체도 넣어놨고,

이제 `/kernel/ptree.c` 에다가 적당한 코드를 뚝딱 잘 짜서 넣어주면 되는데...

조건이 여러가지가 있다.

1. 프로세스 정보는 DFS로 가져와라.
2. 첫 프로세스는 `swapper` - pid가 0인 프로세스이다.
3. 정상적인 리턴 값은 실행 중인 전체 프로세스 수이다. 즉 `nr`만큼 탐색하고 끝이 아니라 끝까지 파고들어야 한다.
4. 그런데 리턴하기 전에 `nr` 값을 실제로 리턴하는 프로세스 갯수로 바꿔주어야 한다.
5. 그 외 에러 날만한 상황에 따라 이것저것 에러 플래그를 잘 세워서 리턴해라.
6. 기타 등등.

힌트는 좀 적다.

1. `task_struct`를 잘 써라.
2. `tasklist_lock`도 써봐라.

고민해야 하는 부분은 많지만 그 중 핵심을 짚어보자면 이 네 가지가 될 것이다.

1. **첫 프로세스 정보를 어떻게 가져올 것이냐**.

   리눅스에서 최상위 프로세스에 대한 정보는 커널이 정적으로 가지고 있지 않을까?

2. **한 프로세스에서 그 부모/자식/형제 프로세스를 어떻게 찾느냐**.

   **리스트**로 관리한다고 하니 그 **포인터**만 있으면 될 것 같다.

3. **위 두가지를 해결했으면 이제 DFS는 어떻게 짤 것이냐**.

   커널에서 재귀를 돌리면 터질수도 있다던데... **루프**로 짜보자!

4. **커널 프로그래밍은 처음이라... 어떻게 "안전"한 코드를 만들 수 있을까**.

   커널 패닉 일으켜보면서 배우게 될 부분이다.

이런 아이디어를 가지고 구현한 자세한 내용은 아래에서 다룬다.





## Our Implementation

---

먼저 구현한 함수/구조체들에 대해 간단히 설명을 하고 넘어가자.



```c
static int prinfo_constructor(struct prinfo *item, struct task_struct *task);
```

이 함수는 `task_struct`에서 `prinfo`에 필요한 정보들을 추출하는 역할을 수행한다.

```c
struct task_list
{
    struct task_struct *task;
    struct list_head list;
}
```

이 구조체는 나중에 설명할 DFS에서 탐색해야 할 노드의 목록을 만든다.

```c
static int add_new_task(struct task_struct *task, struct list_head *t_list);
```

이 함수는 위의 목록에 새로운 프로세스를 하나 추가해준다.

```c
static void del_tasklist(struct list_head *t_list);
```

혹시 남아있을 수 있는 노드들을 삭제해주는 함수.



##### 1. How to get 'swapper' process?

사실 구글링해서 찾은 `pid_task` 와 `find_vpid` 등의 함수를 이용하면 될 줄 알았으나, pid가 0이면 널 포인터를 리턴한다는 문제가 있었다.

커널이 최상위 프로세스에 대한 정보는 가지고 있지 않을까 생각했고, 실제로 `struct task_struct init_task;`라는게 존재했다.

그래서 `/include/linux/sched/task.h`에 `extern`으로 정의돼있는 이 변수의 주소값을 바로 가져올 수 있었다.

```c
add_new_task(&init_task, &tasks_to_visit);
```



##### 2. How to find parent / children / siblings?

`/include/linux/list.h` 라는 게 있는데 이게 커널에서 사용하는 특수한 링크드 리스트를 구현해놓은 것이다.

`task_struct`에서는 이걸 이용하여 자식 및 형제에 대한 리스트를 관리한다. 부모는 그냥 포인터를 가지고 있다.

`/include/linux/sched.h`에 있는 `task_struct`의 정의를 보면 아래와 같은 부분이 있다.

```c
struct task_struct __rcu *parent;
struct list_head children;
struct list_head sibling;
```

이제 프로세스들을 탐색할 수단이 생겼는데, 리스트 구현의 특수성 때문에 코드에서는 `list_entry`, `list_for_each` 등의 매크로를 많이 사용해야 했다.



##### 3. How to do Depth First Search in kernel space?

예전에 교수님께서 "커널에서 재귀 함부로 돌리면 메모리 부족으로 터질 수 있어요" 라고 얘기하신 것이 떠올라서 우리는 루프로 돌리기로 결심했다.

그 방법은 대강 요약하자면,

1. 현재 방문한 노드(프로세스)의 자식들을 "방문해야 할 노드들"이라는 목록의 맨 앞에 추가한다.
2. 목록의 노드(프로세스)들을 앞에서부터 하나씩 방문하면서 완전히 빌 때까지 반복.

C에선 배열을 동적으로 늘리고 줄이기는 힘들기 때문에 방문해야 할 목록을 관리하기 위해 새로운 구조체를 다음과 같이 하나 만들었다.

```c
struct task_list
{
    struct task_struct *task;
    struct list_head list;
}
```

단순히 프로세스 구조체의 포인터만 저장하는 간단한 리스트로, 커널에서 사용하는 리스트의 구현을 여기서 잘 써먹었다.

메모리를 아끼기 위해 유저가 준 `nr`까지는 루프를 돌면서 얻은 탐색 결과 하나씩을 미리 만들어놓은 배열에 저장하고, `nr` 갯수를 다 채웠으면 그 다음부터는 DFS를 계속 하되 임시 변수에만 그 결과를 넣었다가 바로 버리도록 했다.

```c
while(!list_empty(&tasks_to_visit)) {

    /* retrieve first item in the list */
    current_item = list_entry(tasks_to_visit.next, struct task_list, list);

    dummy_ptr = (count++ < k_nr) ? &k_buf[count - 1] : &storage;
        
    if (prinfo_constructor(dummy_ptr, current_item->task)) {
        printk(KERN_ERR "[PROJ1] prinfo constructor error - skipping.\n");
        continue;
    }

    list_del(&(current_item->list));

    if(!list_empty_careful(&(current_item->task->children))) {

        /* 
         * iterate *backwards* in children and push each task into STACK
         * to maintain original order of children.
         */
        list_for_each_prev_safe(pos, q, &(current_item->task->children)) {
            add_new_task(
                    list_entry(pos, struct task_struct, sibling),
                    &tasks_to_visit);
        }
    }
    kfree(current_item);
}
```

모종의 이유로 `prinfo_constructor` 등에서 `kmalloc`이 실패할 경우, 해당 프로세스는 건너뛰는 방식을 선택했다.

이렇게 루프로 짜고 보니 *"어쩌피 계속 자식 프로세스 하나당 하나씩 `kmalloc`을 해주는데 메모리 부족으로 터질 가능성은 재귀나 이 방식이나 도찐개찐인 것 같다"* 라는 생각이 들었지만, 다시 바꾸긴 귀찮았고 *"if it works, don't touch it."*라는 신조를 받들기로 했다.



##### 4. How to "safely" code in kernel space?

코드를 보면 `if(!access_ok(...))` 같은 것들이 엄청 많다...

물론 `copy_to_user` 같은 함수에서 저 과정이 이미 포함되어있다고는 하지만, 채점 기준에 들어갈 것 같아서 무서워서 넣었다.

그리고 스펙에서 명시한 여러 조건들에 대해 적절한 리턴 값을 설정해주었다.

또한, `kmalloc`이 사용된 이후 시점의 리턴 포인트마다 모두 `kfree`를 하는 꼼꼼함까지 보였다.

사실 `del_tasklist(...)`는 DFS 내의 `kfree`가 제대로 되면 전혀 필요없는 부분이지만 혹시 모르니까 넣어줬다.

유저랜드와 커널 스페이스의 메모리 주소가 달라서 유저랜드는 32비트라고 한다. 그에 따라 적절한 조치도 취해주었다.



##### ETC

그 외의 부분들은 `Project1.md`에서 시키는 대로 잘 했다.





## Test Results and Process Tree Investigation

---

##### Test Results

[Test Log](proj1log.txt)

특이사항으로 테스트 코드는 커맨드라인 아규먼트로 `nr`을 받는 것이 아니라 실행 후 유저 인풋으로 받는다는 점이 있고,

가독성을 위해 프로세스 정보를 출력할 때 컴마 뒤에 공백 하나씩 추가했다.



##### Process Tree Investigation

프로세스는 프로그램의 한 인스턴스로, 운영체제는 프로세스를 통해 프로그램에 대한 추상화를 제공한다.

유저 스페이스에서 새로운 프로세스는 어떠한 부모 프로세스로부터의 `fork()`를 통해 생성된다.

(ex : 쉘 내부에서 실행되는 프로그램들은 해당 쉘 프로세스를 부모로 하는 자식이 된다.)

이를 통해 모든 프로세스 사이에는 부모-자식 관계가 존재하므로 프로세스들을 트리 형태로 나타낼 수 있다.

프로세스 트리는 다음과 같이 생성된다.

1. 운영체제가 부팅될때 최초의 프로세스인 swapper 를 생성한다.

   - pid가 0이다.

   - swapper라는 이름은 구시대의 유물이라고 한다.
   - 이 프로세스는 `ps` 명령어를 통해서는 확인할 수 없다.
   - 전체 시스템이 Idle 상태일 때 실행된다고 한다.

2. 그 다음, swapper로부터 init 프로세스가 자식으로 생성된다.

   - pid가 1이다.

   - 보통 리눅스에선 init 혹은 systemd라고 이름이 붙는데, 타이젠의 경우 systemd로 확인되었다.
   - 실질적으로 모든 (유저) 프로세스의 단군할아버지.
   - 만약 어떤 프로세스가 수행 중에 그 부모가 종료되면, 부모 잃은 이 프로세스를 init이 입양한다고 한다.

3. swapper의 자식이자 init의 형제 프로세스인 kthreadd 프로세스가 생성된다.

   - pid가 2이다.
   - 일반적으로 하드웨어를 관리하는 등 커널의 핵심 기능을 수행하는 프로세스들이라고 한다.

4. 이제 시스템을 사용하기 위한 기타 여러 daemon 같은 프로세스들이 init의 자식으로 생성된다.

실제로 확인해보니 우리(유저)가 가장 처음으로 운영체제와 상호작용하는 로그인 프로세스는 pid가 보통 300~400번대로 상당히 어린 프로세스임을 알 수 있었다.



## Lessons Learned

---

아래의 목록은 중요도와 무관하게 나열되어있다.

* 리눅스에서는 task를 관리할때 task_struct라는 PCB(process control block)을 사용해서 프로세스에 대한 정보를 저장함을 알 수 있었다.
* kernel space에서의 메모리 주소와 user space에서의 메모리 주소가 다름을 알게 되었다.
* 이를 해결하기 위해 copy_to_user 매크로와 copy_from_user 함수를 사용해서 user space의 address를 kernel space의 address로 바꾸고, 그 역과정을 수행하면서 필요한 데이터를 user로 부터 받아올 수 있었다.
* 커널 프로그래밍 할때는 defensive 하게 코딩해야 함을 잘 알수 있었다.
* 유저 레벨에서 버그가 발생했을때와 달리, 커널에서는 버그가 발생하면 커널패닉이 나기 때문에 어디에서 문제가 발생했는지 몰라서 디버깅할때 매우 많은 시간이 소요되었다.
* 코드 한줄한줄 실행할때마다 printk문을 집어넣어서 어디서 문제가 일어났는지 일일히 파악하면서 디버깅을 하는것은 다시는 하고싶지 않은 경험이었다.
* 특히, DFS로 children과 sibling들을 탐색하는 도중, 중간에 단 한줄 잘못된 코드 (children과 sibling을 혼동해서 씀)때문에 커널 패닉이 일어나서 고치는데 꽤 애를 먹었다.
* 커널에 직접 시스템콜을 추가하면서, 시스템콜이 커널 어디에 저장되는지 잘 알 수 있었다.
* 처음에 시스템콜을 추가할때는 arm64에 해당하는 arch 폴더가 아닌 다른 위치에 시스템콜을 추가해서 ptree 시스템콜을 호출했을때 어떤 시스템콜을 호출하는지 인식을 하지 못했었다.
* 때문에 무엇이 문제인지 2~3일정도 헤매면서 arm64 아키텍처에서 시스템콜을 추가하는 위치를 알아내었고, 그 뒤에 정상적으로 시스템콜을 호출할 수 있게 되었다.
* 이러한 문제를 해결하면서 어떻게 커널이 수많은 아키텍처를 지원하도록 만들어 졌는지 알 수 있었다.
