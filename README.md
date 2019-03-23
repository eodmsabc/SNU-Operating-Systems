# os-team10
OS Spring Team10

README file for Project 1


ptree systemcall 추가 구현사항

1. 있나? 추가좀 해주세용~

















How to build Kernel




커널 이미지 & 부트 이미지 빌드 & sd카드에 플래싱하기를 쉘 스크립트로 한번에 진행하게 해놨습니다.
(kernelbootflash.sh)
sdcard device location 인자를 주지 않으면 플래싱은 스킵합니다.

1. 최상위 폴더에 있는 kernelbootflash.sh 쉘 스크립트를 실행합니다. ( ./kernelbootflash.sh <sdcard device location> 입력)
ex) ./kernelbootflash.sh /dev/sdb
2. 중간에 sudo 권한 요구하는 창이 뜰텐데, 비밀번호 입력후 계속 진행
2-1. sdcard device location을 인자로 주지 않았으면 플래싱을 하지 않고 계속 진행할것이냐는 창이 뜹니다.
   y 입력후 엔터. 이경우 커널이미지, 부트이미지와 tar 파일까지만 만들어집니다.
3. 커널 이미지 빌드 ~ 부트 이미지 빌드까지 완료 됩니다.
4. sdcard에 플래싱 완료.



high-level design & implement

system call p_tree

1. 유저로부터 (struct prinfo *buf, int *nr)을 받는다.

2. 받은 buf와 nr의 값이 null이 아닌지 체크한다. null이면 EINVAL 에러코드로 리턴한다.

3. buf와 nr 포인터가 가리키는 주소가 안전한지 체크한다. (access_ok 함수 이용) 

4. nr 값을 user space에서 kernel space로 옮긴다. 그리고 태스크들을 담을 공간을 kmalloc으로 할당한다.

5. tasklist에 락을 걸고 

6. tasklist중 0번 task (모든 task의 부모가 되는)를 가져온다.

7. 태스크의 자식들을 DFS 탐색하면서 태스크의 정보를 kmalloc으로 할당한 공간에 담는다.

8. tasklist 락을 해제한다.

8. 태스크를 모두 탐색한 다음, MIN(총 태스크 개수,nr)개 만큼의 태스크 정보를 유저스페이스로 복사한다.

9. 전체 태스크의 개수를 리턴하고 시스템 콜을 끝낸다.




investigate process tree

process는 program의 instance로, OS는 process를 통해 program에 대한 abstraction을 제공한다.


User 단에서, 새로운 process는 부모 process 에서 fork()를 통해 생성되고, (ex : shell 프로그램 내부에서 실행되는 프로그램들은 shell process를 부모로 하는 process들이 된다.)

이를 통해 부모관계를 가지게 된다. 이러한 프로세스들간에 가지는 부모관계들을 총칭하는것이 process tree이다.

process tree는 다음과 같이 생성된다.
1. OS가 부팅될때 최초의 process인 init process를 생성한다. (보통 init process의 pid는 1이다.)
2. 다른 프로세스들은 init process로 부터 생성되거나 init process의 자식들로부터 생성된다.
3. 결국 모든 프로세스는 공통조상으로 init process를 가지게 된다.

따라서 process tree는 init process를 뿌리로 하는 트리형태가 된다.

init process의 parent를 따보면 pid 0인 swap process가 있는데, 이는 kernel scheduler를 의미하므로 모든 프로세스의 실질적인 조상은 init process가 맞다.

또한, child process가 실행중 child를 생성한 parent 프로세스가 종료될경우, init process가 child process를 입양하게 되어 init process를 부모로 가지게 된다.













느낀점, 알게된점 :

리눅스에서는 task를 관리할때 task_struct라는 PCB(process control block)을 사용해서 프로세스에 대한 정보를 저장함을 알 수 있었다.

또한, kernel space에서의 메모리 주소와 user space에서의 메모리 주소가 다름을 알게 되었다.
이를 해결하기 위해 copy_to_user 매크로와 copy_from_user 매크로를 사용해서 user space의 address를 kernel space의 address로 바꾸고, 그 역과정을 수행하면서 필요한 데이터를 user로 부터 받아올 수 있었다.

커널 프로그래밍 할때는 defensive 하게 코딩해야 함을 잘 알수 있었다.
유저 레벨에서 버그가 발생했을때와 달리, 커널에서는 버그가 발생하면 커널패닉이 나기 때문에
어디에서 문제가 발생했는지 몰라서 디버깅할때 매우 많은 시간이 소요되었다.
코드 한줄한줄 실행할때마다 printk문을 집어넣어서 어디서 문제가 일어났는지 일일히 파악하면서 디버깅을 하는것은 다시는 하고싶지 않은 경험이었다.

특히, DFS로 children과 sibling들을 탐색하는 도중, 중간에 단 한줄 잘못된 코드 (children과 sibling을 혼동해서 씀)때문에 커널 패닉이 일어나서 고치는데 꽤 애를 먹었다.

커널에 직접 시스템콜을 추가하면서, 시스템콜이 커널 어디에 저장되는지 잘 알 수 있었다.
처음에 시스템콜을 추가할때는 arm64에 해당하는 arch 폴더가 아닌 다른 위치에 시스템콜을 추가해서
ptree 시스템콜을 호출했을때 어떤 시스템콜을 호출하는지 인식을 하지 못했었다.
때문에 무엇이 문제인지 2~3일정도 헤매면서 arm64 아키텍처에서 시스템콜을 추가하는 위치를 알아내었고,
그 뒤에 정상적으로 시스템콜을 호출할 수 있게 되었다.
이러한 문제를 해결하면서 어떻게 커널이 수많은 아키텍처를 지원하도록 만들어 졌는지 알 수 있었다.

