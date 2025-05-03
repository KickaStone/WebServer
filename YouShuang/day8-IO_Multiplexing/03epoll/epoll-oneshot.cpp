#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 1024

struct fds{
    int epollfd;
    int sockfd;
};

/**
 * 设置文件描述符为非阻塞
 * @param fd 需要设置的文件描述符
 * @return 原来的文件描述符状态
 */
int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/**
 * 向epoll中添加描述符
 * @param epollfd epoll描述符
 * @param fd 需要监听的描述符
 * @param enable_et 是否启用ET模式
 */
void addfd(int epollfd, int fd, bool oneshot){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if(oneshot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

/**
 * 重置fd上的事件, 确保下次可读时, 事件能被触发
 * @param epollfd epoll描述符
 * @param fd 需要重置的描述符
 */
void reset_oneshot(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void* worker(void* arg){
    int sockfd = ((fds*)arg)->sockfd;
    int epollfd = ((fds*)arg)->epollfd;

    printf("start new thread to receive data on fd: %d\n", sockfd);

    char buf[BUFFER_SIZE];
    memset(buf, '\0', BUFFER_SIZE);

    // 循环读取sockfd上的数据，直到遇到EAGAIN错误
    while(1){
        int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
        if(ret == 0){
            // 说明对端已“有序关闭”连接（即对方调用了 close() 或 shutdown()），本端数据已全部读取完毕，连接可以关闭
            close(sockfd);
            printf("foreiner closed the connection\n");
            break;
        }else if(ret < 0){
            if(errno == EAGAIN){
                // EAGAIN 或 EWOULDBLOCK：非阻塞模式下无数据可读
                reset_oneshot(epollfd, sockfd);
                printf("read later\n");
                break;
            }
        }else{
            printf("get content: %s\n", buf);
            // 休眠5s, 模拟数据处理
            sleep(5);
        }
    }
    printf("end thread receiving data on fd: %d\n", sockfd);
    return NULL;
}

int main(int argc, char* argv[]){
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
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    // listenfd 不能注册EPOLLONESHOT事件, 否则只触发一次，导致应用程序无法处理多个连接
    addfd(epollfd, listenfd, false);

    while(1){
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(ret < 0){
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < ret; i++){
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd){
                // 处理新连接
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                // 对每个非监听文件描述符都注册epolloneshot事件
                addfd(epollfd, connfd, true);
            }else if(events[i].events & EPOLLIN){
                pthread_t thread;
                fds fds_for_new_conn;
                fds_for_new_conn.epollfd = epollfd;
                fds_for_new_conn.sockfd = sockfd;
                // 新线程负责处理数据接收
                pthread_create(&thread, NULL, worker, (void*)&fds_for_new_conn);
            }else{
                printf("something else happened\n");
            }
        }
 
    }

    close(listenfd);
    return 0;   
}