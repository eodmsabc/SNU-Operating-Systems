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
를 통해 테스트 프로그램을 빌드할 수 있고, make clean을 통해 만들어진 프로그램을 정리할 수 있다.



## Project Overview & Goals

타이젠 커널의 ext2 파일 시스템에 gps location 관련 필드를 추가하고,

gps location 필드를 이용해 geo-tagged file system을 구현하는것이 이번 프로젝트의 목표이다.


현재 시스템의 로케이션을 수정하고, 파일의 로케이션을 얻어오기 위해 다음 시스템 콜을 구현해야한다.

sys_set_gps_location(struct gps_location *loc) : 시스템의 로케이션을 해당 로케이션으로 설정한다. 

sys_get_gps_location(const char *pathname, struct gps_location *loc) : 해당하는 파일의 로케이션 정보를 받아온다.


gps location은 다음 필드를 가진다.

'''
struct gps_location {
  int lat_integer;
  int lat_fractional;
  int lng_integer;
  int lng_fractional;
  int accuracy;
};
'''

경도와 위도는 6자리 고정소수점 형태로 들어오며, 커널에서는 부동소수점 연산을 지원하지 않기 때문에 우리가 직접 소수점 연산을 구현해야한다.

구현한 소수점 연산을 가지고 위도와 경도를 통해 현재 시스템의 location과 파일을 생성한 location이 겹치면 파일에 엑세스 가능하게 하고, location 이 겹치지 않으면 파일에 엑세스 하지 못하게 하는것이 목표이다.





## Our Implementation


##### add gps related field to ext2 file system


Geo-tagged file system을 구현하기 위해 ext2 파일시스템에 gps 관련 필드를 추가해야했다.

따라서 fs/ext2/ext2.h에 다음과 같이 필드를 추가했다.


fs/ext2/ext2.h


struct ext2_inode_info

'''
spinlock_t inode_info_gps_lock;
    __u32 i_lat_integer;
    __u32 i_lat_fractional;
    __u32 i_lng_integer;
    __u32 i_lng_fractional;
    __u32 i_accuracy; 
'''

struct ext2_inode

'''
    __le32 i_lat_integer;
    __le32 i_lat_fractional;
    __le32 i_lng_integer;
    __le32 i_lng_fractional;
    __le32 i_accuracy;

    //for use, __le32	cpu_to_le32(const __u32); and __u32	le32_to_cpu(const __le32); macro.
'''


여기서, ext2_inode_info는 메모리에 inode가 저장될때의 형태고,

디스크에 inode가 저장될때는 ext2_inode의 형태로 저장되므로,

ext2_inode에 저장할때는 __le32 형을 사용하여 시스템의 엔디안에 관계없이 저장될 수 있게 하였다.




fs/ext2/inode.c 


ext2_iget, __ext2_write_inode 함수에서 ext2_inode <-> ext2_inode_info 변환을 진행하므로, 이 부분에서 le32_to_cpu, cpu_to_le32 매크로를 이용, 서로간의 변환을 하게 하였다.


또한, ext2_inode_info에 gps에 대한 spinlock을 추가하여 gps에 관련된 연산을 할때 consistency를 유지하게 하였다.




##### add gps related logic to ext2 file system


gps related field를 file system에 추가한 후, 이 field들을 상황에 맞게 설정하는 로직을 ext2 file system에 구현하였다.


먼저 fs/ext2/inode.c에 다음과 같이 파일의 gps location을 설정하고, 받아오는 로직을 구현하였다.

'''
int ext2_set_gps_location(struct inode *inode)
{
	struct ext2_inode_info *ei;
	struct gps_location current_location;

	ei = EXT2_I(inode);
	current_location = get_current_location();

	spin_lock(&(ei->inode_info_gps_lock));
	ei->i_lat_integer = current_location.lat_integer;
	ei->i_lat_fractional = current_location.lat_fractional;
	ei->i_lng_integer = current_location.lng_integer;
	ei->i_lng_fractional = current_location.lng_fractional;
	ei->i_accuracy = current_location.accuracy;
	spin_unlock(&(ei->inode_info_gps_lock));

	return 0;
}
int ext2_get_gps_location(struct inode *inode, struct gps_location *loc)
{
	struct ext2_inode_info *ei;

	ei = EXT2_I(inode);

	spin_lock(&(ei->inode_info_gps_lock));
	loc->lat_integer = ei->i_lat_integer;
	loc->lat_fractional = ei->i_lat_fractional;
	loc->lng_integer = ei -> i_lng_integer;
	loc->lng_fractional = ei->i_lng_fractional;
	loc->accuracy = ei->i_accuracy;
	spin_unlock(&(ei->inode_info_gps_lock));

	return 0;
}
'''


그리고 regular file들에 대해 gps 좌표를 이용한 permssion check가 진행되어야 하므로,

fs/ext2/file.c의 ext2_file_inode_operations 구조체에 .set_gps_location, .get_gps_location 필드로 
		 ext2/inode.c에서 구현한 ext2_set_gps_location, ext2_get_gps_location을 달아주었다.




그 후, fs/ext2와 fs의 여러 파일들에 대해 다음 로직들을 추가하여 파일이 만들어지거나 수정될때 gps location이 갱신되도록 하고, 필요한 연산들을 구현하였다.

fs/namei.c : do_inode_permission에 gps location 통해 permission check logic 추가
fs/ext2/super.c : init_once에 spin_lock_init 통해 스핀락 초기화
fs/ext2/namei.c : ext2_create에 set_gps_location logic 추가.
fs/ext2/inode.c : ext2_set_gps_location, ext2_get_gps_location 구현,
		  ext2_write_end, ext2_setsize에 set_gps_location logic 추가
		  ext2_iget, __ext2_write_inode에 le32_to_cpu, cpu_to_le32로 서로간 엔디안 바꿔주는 로직 추가.
fs/ext2/ialloc.c : ext2_new_inode에 set_gps_location logic 추가
fs/ext2/file.c : ext2_file_inode_operations에 .set_gps_location, .get_gps_location 필드에 
		 ext2/inode.c에서 구현한 ext2_set_gps_location, ext2_get_gps_location 달아줌.
fs/ext2/ext2.h : inode와 inode info에 gps 관련 필드 추가.
fs/attr.c : notify_change에 set_gps_location logic 추가.



여기서, inode의 set_gps_location 함수를 부르기 전에는 다음과 같이 if문을 통해 해당하는 operation이 존재하는지 확인하도록 하였다.


'''
if (inode->i_op->set_gps_location)
		inode->i_op->set_gps_location(inode);
'''


##### how to check permission by gps & how to implement precision operation 







           


extern int check_gps_permission(struct gps_location);


static inline int do_inode_permission(struct inode *inode, int mask)
{

    if(inode->i_op->get_gps_location) // if inode has get_gps_location, this means that inode is ext2 regular file.
    {
        struct gps_location loc;
        inode->i_op->get_gps_location(inode, &loc);
        if(check_gps_permission(loc) == 0)
            return -EACCES; // if not get permission, return -EACCES.
    }
	return generic_permission(inode, mask);
}



get
가

먼저 이를 위해 다음 연산을 전체 file system의 inode_opertations에 추가했다.

'''
int (*set_gps_location)(struct inode *);
int (*get_gps_location)(struct inode *, struct gps_location *);
'''



## High Level Design



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

```




## Lessons Learned



* 커널에서 lock이 어떻게 동작하는지 파악할 수 있었다.
* lock을 실제로 구현하면서, 좋은 디자인 패턴이 정말 중요함을 느꼈다. 교과서에 있는 구현패턴을 보지 않았으면 락을 구현하는데 굉장히 오래 걸렸을 것이다.
* lock을 구현할때, safety와 performance를 둘다 만족하게 구현하는것이 까다로운 일임을 깨달았다.
* simple is best라는것을 다시한번 느끼게 되었다. 특히 락같이 여러 오브젝트를 고려해야 하는 경우에는 전체 그림을 그리기가 까다로워서, 간편하고 범용적인 구현에서 조금씩 구체화 해가는 식으로 락을 구현하였다.

* OS 프로젝트를 진행할때는 빠른 시작이 중요함을 뼈저리게 느낄 수 있었다. 데드라인에 닥쳐서 구현을 시작했으면 정말 큰일날 뻔했다.
