# OS/TEAM 10 - PROJ4


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


프로젝트에서, Geo-tagged file system을 구현하기 위해 ext2 파일시스템에 gps 관련 필드를 추가해야했다.

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


gps 관련 필드를 ext2 파일시스템에 추가한 뒤로는, permission 체크를 위해 fs/namei.c 의

do_inode_permission 함수에 다음과 같이 gps location을 통해 permission check를 하는 로직을 추가하였다.


extern int check_gps_permission(struct gps_location);

'''
static inline int do_inode_permission(struct inode *inode, int mask)
{
    ...

    if(inode->i_op->get_gps_location) // if inode has get_gps_location, this means that inode is ext2 regular file.
    {
        struct gps_location loc;
        inode->i_op->get_gps_location(inode, &loc);
        if(check_gps_permission(loc) == 0)
            return -EACCES; // if not get permission, return -EACCES.
    }
	return generic_permission(inode, mask);
}
'''

여기서 check_gps_permission 함수를 통해 현재 시스템의 location과 파일의 location 정보를 비교 한 뒤, 엑세스 가능한지 그렇지 않은지를 결정하게 된다.


check_gps_permission은 우리가 gps.c에 구현한 함수로, 시스템의 현재 gps location이 file에 접근 가능한지를 결정한다.

가정에서, 지구는 반지름 6371000m인 구로 가정하였다.

두 좌표의 거리가 두 좌표에서 그린 원의 반지름의 합보다 작으면 파일에 접근할 수 있다는 것에 아이디어를 얻었다.

두 좌표를 A,B, 위도, 경도를 각각 latA, latB, lngA, lngB라고 하고, 각 좌표에서의 원의 반지름을 dA, dB라 하자.

그리고 두 좌표의 거리를 dist라 하면, 결국 dist^2 < (dA + dB)^2 이 되는지를 구하면 된다.

따라서 dist만 구하면 되므로, 다음과 같은 과정을 통해 dist를 구했다.

A,B 두 좌표에서 구 위에 각각 위도, 경도에 평행하게 선을 그은 뒤, 그 선들의 교점을 구한다. 이 점을 C라 하자. 
C에서 A까지의 거리를 dx, C에서 B까지의 거리를 dy라 하면, 두 좌표의 위도, 경도가 많이 차이나지 않을때, 
평면으로 근사할 수 있어 dist^2 = dx^2 + dy^2으로 근사가 가능하다.

따라서 dx, dy를 구하는것이 우리의 목표다.

호의 길이 = r*theta 에서, (theta의 단위 : radian)
A와 B의 위도의 차이를 radian으로 환산한 뒤에, 다음과 같이 dy를 구했다.

dy = |latA - latB| * R(지구 반지름)


dx의 경우, R에 들어가는 값이 지구 반지름이 아니라, 그 위도에서의 자전 반지름이 된다.
이때 만약 A와 B의 위도가 차이나는경우, A나 B중 어느 하나를 기준으로 자전반지름을 구하면 차이가 나므로,
A와 B의 위도에 따라 자전반지름을 평균내어 구하였다.

따라서 r을 자전반지름이라 하면,

r = R(지구 반지름) * cos(avg(lngA, lngB))

dx = |lngA - lngB| * r(자전 반지름)


따라서 이를 통해 dx, dy의 값을 구하고, dist를 구해서 이 값을 반지름의 합과 비교하여 permission 결정을 내리게 되었다.


cos 함수의 값 계산은, sin 함수의 0도 ~ 90도까지의 각을 1도 단위로 테이블에 입력해 놓은다음, 그 각도를 interpolation을 해서 함숫값을 계산할 수 있도록 하였다.



## Test Results 

```
root:~/test> ./file_loc /root/proj4/mydir/first
/root/proj4/mydir/first location info
latitude        longitude       accuracy(m)
10.500000       20.351242       500
google maps link: http://www.google.com/maps/place/10.500000, 20.351242

root:~/test> ./file_loc /root/proj4/mydir/second
/root/proj4/mydir/second location info
latitude        longitude       accuracy(m)
50.0        30.0        1000
google maps link: http://www.google.com/maps/place/50.0, 30.0

root:~/test> ./file_loc /root/proj4/mydir/third
/root/proj4/mydir/third location info
latitude        longitude       accuracy(m)
50.1        30.1        50000
google maps link: http://www.google.com/maps/place/50.1, 30.1



## Lessons Learned



* GPS 관련 정보를 추가하면서, 파일 시스템이 어떻게 구현되었는지 알아볼 수 있었다.
* 커널 내부에서 아키텍처 관계없이 파일이 써질 수 있도록, 엔디안에 관계없는 타입을 따로 만들면서까지 호환성을 고려하였다는것을 알게 되었다.
* 우리가 단순히 파일을 만들고, 지우고, 실행하는 과정에서 정말 많은 함수가 이러한 과정에 연관되어 있다는것을 알 수 있었다. 처음 구현을 할때는 몇가지 함수에 gps set 로직을 빼먹어서, 제대로 set이 안되어서 애를 많이 먹었었다.
* 커널에서 floating point 연산을 직접 구현해보면서, 고정 소수점 연산을 구현할때 내부 자료형을 long long int가 아닌 int로 선언해서 연산에서 오버플로우가 발생하거나, 나머지 연산에서 주의를 기울이지 않아서 오류가 발생하는등의 문제가 발생했었는데, 특정 연산이 지원되지 않는 환경에서, 기존 환경이 지원하는 연산들을 가지고 새로운 연산을 구현하는것은 생각보다 어렵고 고려할것도 많다는 것을 알게 되었다.

* OS 과제를 통해 커널 구조에 대해 많은 것을 알게 되었다. 직접 머리 싸매면서 부딪혀 보지 않으면 알 수 없었던 지식들을 많이 얻어간 느낌이다. 
