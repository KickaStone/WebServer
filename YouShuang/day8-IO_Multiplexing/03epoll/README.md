# epoll系列系统调用

## 0 简介
Linux 下的 epoll 是一种高效的 I/O 多路复用机制，专为大规模并发连接场景设计，用于替代传统的 select 和 poll，解决它们在高并发下效率低下的问题

### 核心原理
- 事件驱动机制
epoll采用事件驱动机制，用户进程通过 epoll 注册关心的文件描述符及其事件（如可读、可写）。当这些文件描述符状态发生变化时，内核会主动通知用户进程，而不是像 select/poll 那样每次都遍历全部描述符。

- 高效的数据结构
epoll 内核中主要用红黑树（red-black tree）来存储所有被监听的文件描述符，实现高效的增删改操作。就绪的事件则通过就绪队列（链表）管理，只有真正发生事件的描述符才会被加入到就绪队列

- 回调和等待队列
当被监听的 socket 有数据到达时，内核通过回调机制（如 ep_poll_callback），将该 socket 对应的事件节点加入就绪队列，并唤醒因调用 epoll_wait 而阻塞的进程

- 阻塞与唤醒机制
用户调用 epoll_wait 时，如果没有就绪事件，进程会被挂起，节省 CPU 资源；一旦有事件到来，通过回调机制唤醒进程，通知其处理。


## 1 内核事件表

epoll 在内核中维护一个事件表（有时称为“红黑树”结构），存放所有用户通过 epoll_ctl 注册的文件描述符及其感兴趣的事件类型（如可读、可写、异常等）
epoll 需要一个文件描述符来唯一标识内核中的这个事件表，使用`epoll_create`创建。

```c
int epoll_create(int size);
```

size 参数不起作用，只是提示内核事件表需要多大。返回的文件描述符将用作其他所有epoll系统调用的第一个参数，以指定要访问的内核事件表。

```c
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
```
epfd:epoll内核事件表
op:操作类型，下面讲
fd:要操作的文件描述符
event:指定指针类型
```c
struct epoll_event{
    _uint32_t events; /*epoll事件*/
    epoll_data_t data;
}
```
其中`events`使用掩码来指定感兴趣的事件，常用
EPOLLIN:可读事件
EPOLLOUT:可写事件
EPOLLPRI:有紧急数据可读（带外数据）
EPOLLERR:发生错误
EPOLLHUP:挂断事件
EPOLLET:边缘触发模式

`epoll_data_t`是一个union，有4个成员`void* ptr, int fd, uint32_t u32, uint64_t u64`, 用的最多就是fd。

支持的操作类型
- `EPOLL_CTL_ADD` 往事件表中注册fd上的事件
- `EPOLL_CTL_MOD` 修改fd上的注册事件
- `EPOLL_CTL_DEL` 删除fd上的注册事件


1. 创建epoll内核事件表

```c
int epollfd = epoll_create(SIZE);
```


2. 注册事件

```c
struct epoll_event ev;
ev.events = EPOLLIN;
ev.data.fd = listenfd;
epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
```

3. 事件等待与处理
调用`epoll_wait()`内核返回已发生的事件数组。然后通过`events[i].data.fd`或`events[i].events`判断事件类型
```c
struct epoll_event events[MAX_EVENTS];
int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
for (int i = 0; i < n; ++i) {
    if (events[i].events & EPOLLIN) {
        // 处理可读事件
        int fd = events[i].data.fd;
        // ... read(fd, ...)
    }
}
```

## 2 epoll_wait函数

`epoll_wait()` 是epoll系列系统调用的主要接口, 它可以在一段规定的超时时间内监听一组文件描述符上的事件，其原型如下：
```c
#include <sys/epoll.h>
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
```

timeout 一般设置为-1,表示直接阻塞
epollfd是epoll_create()函数的返回fd
events是一个`epoll_event`结构体的数组，这个数组只用于输出epoll_wait检测到的就绪事件

## 3 LT和ET模式

LT 水平触发模式：epoll_wait检测到其上有事件发生并将事件通知应用程序后，应用程序可以不立即处理，但下次调用epoll_wait时，这个事件还会通知一次

ET 边缘触发模式：epoll_wait检测到有事件发生时，应用程序必须立刻处理，否则之后不会再次通知

ET模式降低了同一个事件被重复触发的次数，效率更高。

[示例](./server.cpp)中的代码实现了LT和ET两种方式，当使用telnet连接到服务器并发送超过buffer大小的数据时，发现LT模式事件触发的次数要比ET模式多很多。


## 4 EPOLLONESHOT事件

即使我们使用ET模式，一个socket上的某个事件还是可能被触发多次。这在并发程序中会有问题。
假设一个socket上的事件正在被一个线程处理，在处理的过程中，这个socket上又有新数据可读，此时另外一个线程被唤醒来读取这些新的数据。
于是出现了两个线程操作一个socket的情况，容易引发同步问题。

我们期望socket连接在任意时刻都只能被一个线程处理，这一点可以使用epoll的`EPOLLONESHOT`事件实现。

注册了该事件的fd，操作系统最多触发其上注册的一个可读、可写或者异常事件，且只触发一次。
除非使用`peoll_ctl`重置该事件。

如果采用这种方法，线程处理完毕事件后应该立即重置这个`EPOLLONESHOT`事件，以确保这个socket下一次可读时，其`EPOLLIN`事件能被触发。进而让其他工作线程有机会继续处理这个socket。

[示例](./epoll-oneshot.cpp)
worker线程中，每次处理完socket上的输入，等待5s，如果有新的数据进入，则继续处理这些数据，否则会重置`RPOLLONESHOT`，并不在为其服务。（非阻塞模式，socket无数据可读会return -1,并设置errno为`EAGAIN`或`EWOULDBLOCK`）。
同时因为连接都使用了`EPOLLONESHOT`，保证统一时刻只有一个线程处理一个socket。