# 定时器篇

网络需要处理定时事件，如定期检测连接活动状态。

## 定时方法

- socket选项 `SO_RCVTIMEO` ,  `SO_SNDTIMEO`
- SIGALRM 信号
- I/O复用系统调用的超时参数


## 1 `SO_RCVTIMEO` ,  `SO_SNDTIMEO`

分别用来设置socket接收数据超时和发送数据超时事件。
使用方法
在使用socket函数创建sockfd后，使用
```c
setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
```
参数描述如下：
- `sockfd`：套接字文件描述符，标识要设置选项的那个 socket。
- `SOL_SOCKET`：选项所在的协议层级。SOL_SOCKET 表示该选项属于套接字本身（而不是底层协议如 TCP 或 IP）。
- `SO_SNDTIMEO`：要设置的具体选项，这里是发送超时（Send Timeout）。它定义了当发送操作（如 send, write 等）因缓冲区满而阻塞时，最多等待多长时间。
- `&timeout`：指向超时时间值的指针。通常是一个 struct timeval 结构体，指定秒和微秒（不同平台也可能用毫秒整数）。
- `len`：timeout 结构体的长度（字节数），通常为 sizeof(struct timeval) 或相应类型的大小。

## 2 SIGALRM 信号

SIGALRM 是 Linux/Unix 系统中用于定时通知进程的信号。当进程设置了定时器（通常通过 alarm() 或 setitimer() 函数），在指定时间到达后，内核会向该进程发送 SIGALRM 信号。其主要特点和工作机制如下

**产生方式**

主要通过 alarm(unsigned int seconds) 系统调用设置。调用后，系统会在 seconds 秒后向当前进程发送 SIGALRM 信号。
如果再次调用 alarm()，会重置定时器。
也可以通过 setitimer() 等更灵活的定时器接口产生

**实现原理**
内核为每个进程维护一个定时器字段。当调用 alarm() 时，内核记录当前时钟加上指定秒数的“过期时间”。
系统时钟中断不断递增计数（如 jiffies），调度时检查进程的定时器是否到期。
到期后，内核将 SIGALRM 标记加入进程的信号集合，等进程被调度时处理该信号。
如果进程长时间未被调度，信号的实际处理会被延迟。


### 处理非活动连接


利用`alarm`函数，设置一个时间槽`TIMESLOT`，每次到时间触发`SIGALRM`信号，对应的处理函数sa.sa_handler被执行，
这个函数就会调用`lst_timer`队列的`tick`函数，处理队列里超时的连接。

`tick`调用的`cb_func`逻辑是将超时socket移出epoll的监听列表，并关闭socket。


另外还可以使用`SIGTERM`信号来响应`kill -TERM <pid>`命令，可以实现关闭服务器的逻辑处理。


### 实现过程

先实现lst_timer，这就是个双向链表升序容器。



再实现server。这里可以先实现一个基于epoll的socket服务器。

```c
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static int epollfd = 0;

int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

int main(int argc, char const *argv[])
{
    if(argc <= 2){
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, (SO_REUSEADDR | SO_REUSEPORT), &opt, sizeof(opt));

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);


    // 先实现epoll 基础功能
    while(true){
        int event_count = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if(event_count < 0){
            printf("epoll failure\n");
            break;
        }

        char buf[1024];

        for(int i = 0; i < event_count; i++){
            int eventfd = events[i].data.fd;
            if(eventfd == listenfd){
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                assert(connfd >= 0);
                addfd(epollfd, connfd); // 新socket添加到epoll
            }
            else if(events[i].events & EPOLLIN){
                
                memset(buf, '\0', 1024);
                ret = recv(eventfd, buf, 1024, 0);
                if(ret == -1){
                    // TODO: handle error
                    continue;
                }
                else if(ret == 0){
                    // 客户端关闭连接
                    close(eventfd);
                    continue;
                }
                printf("get %d bytes of content: %s\n", ret, buf);
            }
        }
    }


    close(listenfd);
    return 0;
}
```

然后添加计时器功能


需要添加的有
- signal相关
    - signal载体管道
    - signal添加函数
    - signal处理函数
- lst_timer
    - cb_func超时处理函数
    
- main函数修改
    - epoll signal处理逻辑

整体逻辑：
程序不断设置alarm(TIMESLOT)发出`SIGALRM`信号，触发其处理逻辑向管道发送对应信号；
这个管道被epoll监听，有读就绪事件说明这个时间槽结束了，需要看看lst_timer队列里是否有超时的socket；
lst_timer使用`tick`函数进行检查和处理超时事件，处理逻辑在`cb_func`实现

这里只看源代码很容易转晕，主要理解两个超时时间点

- alarm()超时 / 应该是定时任务
- lst_timer超时

**alarm函数**  
该函数用于设置一个闹钟定时器，允许程序在指定的时间后收到一个 ⁠SIGALRM 信号。当你调用 ⁠alarm(seconds) 时，它会在 ⁠seconds 秒后发送 ⁠SIGALRM 信号给调用进程。源代码中使用`timeout`来标志有SIGALRM信号具有一定的迷惑性

**真正的超时**
真正的超时是在`lst_timer`中定义，通过比较链表结点`tmp->expire`时间和当前系统事件确定是否超时。

注意：
超时的判断和`TIMESLOT`的设置有很大关系，可以看作判断的精度，因为只有在发出`SIGALRM`信号时，才会执行`lst_timer`的`tick`函数去查找队列的超时socket。



运行[示例](./02lstTimer/server.cpp)
可以看到我们的定时任务在正常执行，如果一个socket长时间没有活动，会被服务器关闭。
```
$ ./server localhost 8080
timer tick
get 7 bytes of content: hello

get 7 bytes of content: hello

timer tick
timer tick
timer tick
timer tick
close fd 7
kill server

```


### bug修改

1. Alarm clock退出
    系统在启动后一段时间就会打印Alarm clock并终止，debug 模拟不会出现，真实环境偶然出现，
    现在解决方法是在addsig函数中初始化sa结构体`memset(&sa, '\0', sizeof(sa));`
    同时注意到if(timeout)逻辑错误，需要移动到for循环外面，才能正常实现功能。

    
## I/O复用超时参数

三个I/O复用系统调用都有超时参数，它们也能处理定时时间。但由于IO复用系统调用可能在超时之前返回，因此要定时就需要不断更新定时参数以反映剩余时间。

这里说下超时参数设置，timeout=
- 0， 立刻返回，不会阻塞
- -1, 阻塞，等待直到有事件发生
- >0, 等待一段时间后返回