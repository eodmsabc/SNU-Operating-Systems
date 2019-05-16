1. 
지금까지의 구현에서는 wrr policy의 우선순위가 cfs policy의 우선순위보다 무조건 높기때문에, cfs policy를 가진 task들은 실행되지 못하는 경우가 생긴다.

이를 방지하기 위해, wrr runqueue를 run queue와 expired queue 두 파트로 나뉘어서, task가 timeslice를 다 쓸때마다 timeslice를 충전한 뒤 expired queue로 옮기고,

만약 모든 task가 expired queue로 들어갔다면 중간에 cfs가 할당될 수 있는 시간을 얼마간 주어 cfs task들이 실행될 수 있게 한다음, runqueue와 expired queue를 뒤바꾼다.

커널 스레드의 대부분은 CFS 스케줄링 policy로 실행되므로, 이를 통해 하위 스케줄을 가지는 task들도 정상적으로 실행될 수 있게하여 커널의 안정성을 높일 수 있다.

2. 현재 migrate 할 task를 찾을때는 runqueue에 있는 모든 task를 뒤져가면서 옮길 수 있는 가장 weight가 큰 task를 찾는데, 이 경우 O(n)의 시간이 걸리므로 많이 비효율적이다.

따라서 이러한 탐색시간을 줄이기위해, task의 weight에 따라 추가적으로 관리하는 자료구조를 두어, 로드밸런싱이 필요할때 가장 큰 weight를 가지는 task부터 찾을 수 있도록 하여 탐색시간을 단축시킬 수 있을것 같다.
