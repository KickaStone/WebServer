#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>

#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65535
#define MAX_EVENT_NUMBER 10000

#define exit_if(cond, msg) if(cond){ printf(msg); exit(1); }

extern int addfd(int epollfd, int fd, bool one_shot);
extern int removefd(int epollfd, int fd);

void addsig(int sig, void(handler)(int), bool restart = true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    exit_if(sigaction(sig, &sa, NULL) == -1, "addsig error");
}

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    addsig(SIGPIPE, SIG_IGN); // 忽略SIGPIPE信号

    // 创建线程池
    threadpool<http_conn> *pool = NULL;
    try{
        pool = new threadpool<http_conn>;
    }catch(...){
        return 1;
    }

    // 预先为每个可能的客户分配一个http_conn对象
    http_conn* users = new http_conn[MAX_FD];

    // 创建监听的socket
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    exit_if(listenfd == -1, "socket error");

    // 设置linger选项
    struct linger tmp = {1, 0};
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    // 绑定
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    exit_if(bind(listenfd, (struct sockaddr*)&address, sizeof(address)) == -1, "bind error");

    exit_if(listen(listenfd, 5) == -1, "listen error");

    // 监听
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    exit_if(epollfd == -1, "epoll_create error");

    // 将监听的文件描述符添加到epoll对象中
    addfd(epollfd, listenfd, false);

    http_conn::m_epollfd = epollfd;

    while(true){
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if(number < 0){
            if(errno == EINTR){
                continue;
            }

            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < number; i++){
            int sockfd = events[i].data.fd;

            if(sockfd == listenfd){
                // 接收新连接
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);

                if(connfd < 0){
                    printf("errno is: %d\n", errno);
                    continue;
                }

                if(http_conn::m_user_count >= MAX_FD){
                    printf("Internal server busy\n");
                    continue;
                }

                // 将新的客户数据初始化，放入数组中
                users[connfd].init(connfd, client_address); // init函数会增加user_count
            }else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                // 对方异常断开或错误等事件
                users[sockfd].close_conn();
            }else if(events[i].events & EPOLLIN){
                // 根据读取的结果，决定是将任务添加到线程池，还是关闭连接
                if(users[sockfd].read()){
                    pool->append(users + sockfd);
                }else{
                    users[sockfd].close_conn();
                }
            }else if(events[i].events & EPOLLOUT){
                // 根据写的结果，决定是否关闭连接
                if(!users[sockfd].write()){
                    users[sockfd].close_conn();
                }
            }else{
                // 其他事件
            }
        }
        
    }

    close(listenfd);
    close(epollfd);
    delete[] users;
    delete pool;
    return 0;
}


