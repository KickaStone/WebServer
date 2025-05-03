#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

/**
 * 超时连接函数，参数：服务器IP地址，端口号，超时时间，函数成功返回0，失败返回-1
 * @param ip 服务器IP地址
 * @param port 服务器端口号
 * @param time 超时时间, 单位：秒
 * @return 成功返回0，失败返回-1
 */
int timeout_connect(const char* ip, int port, int time){
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    // 通过选项SO_RCVTIMEO和SO_SNDTIMEO设置超时时间类型timeval

    struct timeval timeout;
    timeout.tv_sec = time;
    timeout.tv_usec = 0;
    socklen_t len = sizeof(timeout);
    ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
    assert(ret != -1);

    ret = connect(sockfd, (struct sockaddr*)&address, sizeof(address));
    if(ret == -1){
        if(errno == EINPROGRESS){
            printf("connecting timeout, process timeout logic\n");
            return -1;
        }
        printf("error occur when connecting to server\n");
        return -1;
    }
    return sockfd;
}

int main(int argc, char* argv[]){
    if(argc <= 2){
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd = timeout_connect(ip, port, 10);
    if(sockfd < 0){
        return 1;
    }
    close(sockfd);
    return 0;   
}