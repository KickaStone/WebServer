# Select

```c
#include <sys/select.h>
int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout);
```
参数
- `nfds`参数指定被监听的文件描述符的总数，它通常被设置为select监听的所有文件描述符中的最大值+1,因为是fd是从0开始计数的。

- `readfds, writefds, exceptfds`参数分别指向可读、可写、异常等事件对应的文件描述符集合。应用程序调用select时，通过这3个参数传入自己感兴趣的文件描述符。

`fd_set`结构体仅仅包含一个整型数组，该数组的每个元素的每一位标记一个文件描述符，其能容纳的fd数量是FD_SETSIZE指定的，这就限制了能够同时处理fd的总量。

同时由于位操作过于繁琐，应该使用一系列宏来访问`fd_set`结构体的位：
```c
#include <sys/select.h>
FD_ZERO(fd_set *fdset);                     /* 清除fdset的所有位 */
FD_SET(int fd, fd_set *fdset);              /* 设置fdset的位fd */
FD_CLR(int fd, fd_set *fdset);              /* 清除fdset的位fd */
int FD_ISSET(int fd, fd_set *fdset);        /* 清除fdset的位是否被设置 */
```