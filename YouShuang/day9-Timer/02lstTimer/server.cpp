#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <signal.h>
#include "lst_timer.h"

#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5 // 设置定时任务时间间隔

#define exit_if(r, ...)                                                        \
    {                                                                          \
        if (r)                                                                 \
        {                                                                      \
            printf(__VA_ARGS__);                                               \
            printf("\nerrno no: %d, error msg is %s", errno, strerror(errno)); \
            exit(1);                                                           \
        }                                                                      \
    }

static int epollfd = 0;
static int pipefd[2]; // 1. 添加管道用于发送信号
static sort_timer_lst timer_lst;

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void sig_handler(int sig)
{
    // 2. 信号处理函数，向管道发送信号
    int save_errno = errno; // 保存错误码
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno; // 恢复错误码
}

void addsig(int sig)
{
    // 3. 添加信号函数
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_flags |= SA_RESTART; // 设置信号处理函数在信号处理期间可以被中断
    sa.sa_handler = sig_handler;
    sigfillset(&sa.sa_mask);
    exit_if(sigaction(sig, &sa, NULL) == -1, "addsig error");
}

void cb_func(client_data *user_data){
    // 4. 定时任务处理函数
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    printf("close fd %d\n", user_data->sockfd);
}

void timer_handler(){
    // 5. 定时任务处理函数
    timer_lst.tick();
    alarm(TIMESLOT);
}

int main(int argc, char const *argv[])
{
    if (argc <= 2)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    exit_if(listenfd < 0, "socket error");

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, (SO_REUSEADDR | SO_REUSEPORT), &opt, sizeof(opt));

    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    exit_if(ret < 0, "bind error");

    ret = listen(listenfd, 5);
    exit_if(ret < 0, "listen error");

    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    exit_if(epollfd < 0, "epoll_create error");
    addfd(listenfd);

    // 6. 添加管道和信号
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    exit_if(ret < 0, "socketpair error");
    setnonblocking(pipefd[1]);
    addfd(pipefd[0]);
    addsig(SIGALRM);
    addsig(SIGTERM);

    // 7. 初始化变量
    bool stop_server = false;
    bool timeout = false;
    bool *fd_to_user = new bool[FD_LIMIT];
    alarm(TIMESLOT); // 设置定时任务

    client_data *users = new client_data[FD_LIMIT];

    while (!stop_server)
    {
        int event_count = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        exit_if((event_count < 0) && (errno != EINTR), "epoll_wait error");

        // printf("event_count: %d\n", event_count);

        for (int i = 0; i < event_count; i++)
        {
            int eventfd = events[i].data.fd;
            if (eventfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                exit_if(connfd < 0, "accept error");
                addfd(connfd); // 新socket添加到epoll

                // 8 保存用户连接信息，并设置计时器

                // 设置计时器
                util_timer *timer = new util_timer;
                timer->user_data = &users[connfd];
                timer->cb_func = cb_func; // 设置回调函数
                time_t now = time(NULL);
                timer->expire = now + 3 * TIMESLOT;

                // client_data 结构体
                users[connfd].address = client_address;
                users[connfd].sockfd = connfd;
                users[connfd].timer = timer;
                timer_lst.add_timer(timer);
            }else if((eventfd == pipefd[0]) && (events[i].events & EPOLLIN)){
                // 10 处理信号
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1)
                {
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else{
                    for (int j = 0; j < ret; j++)
                    {
                        switch (signals[j])
                        {
                            case SIGALRM:
                            {
                                // timer_handler(); 这里不这么做是因为要优先处理其他io事件
                                timeout = true;
                                break;
                            }
                            case SIGTERM:
                            {
                                printf("kill server\n");
                                stop_server = true;
                                break;
                            }
                        }
                    }
                }
            }
            else if( events[i].events & EPOLLIN)
            {
                // 9 更新接收数据时对计时器的操作

                memset(users[eventfd].buf, '\0', BUFFER_SIZE);
                ret = recv(eventfd, users[eventfd].buf, BUFFER_SIZE-1, 0);
                printf("get %d bytes of content: %s\n", ret, users[eventfd].buf);
                util_timer *timer = users[eventfd].timer;
                if (ret < 0)
                {
                    // TODO: handle error
                    if (errno != EAGAIN)
                    {
                        cb_func(&users[eventfd]);
                        if (timer)
                        {
                            timer_lst.del_timer(timer);
                        }
                    }
                }
                else if (ret == 0)
                {
                    // 客户端关闭连接
                    cb_func(&users[eventfd]);
                    if (timer)
                    {
                        timer_lst.del_timer(timer);
                    }
                }
                else
                {
                    // 正常接收数据，更新计时器
                    time_t now = time(NULL);
                    timer->expire = now + 3 * TIMESLOT;
                    timer_lst.adjust_timer(timer);
                }
            }
        }
        if(timeout){
            timer_handler();
            timeout = false;
        }
    }

    close(listenfd);
    return 0;
}
