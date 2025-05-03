#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
/**
 * A simple CGI server
 */

int main(int argc, char const *argv[])
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd != -1);
    
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    // serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(37698);
    
    int ret = bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if(ret == -1) {
        switch (errno) {
            case EADDRINUSE:
                fprintf(stderr, "端口已被占用，请更换端口。\n");
                break;
            case EACCES:
                fprintf(stderr, "没有权限绑定到该端口（通常是小于1024的端口需要root权限）。\n");
                break;
            default:
                perror("bind 失败");
        }
        close(sockfd);
        return -1;
    }

    ret = listen(sockfd, SOMAXCONN);
    assert(ret != -1);

    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    bzero(&clnt_addr, sizeof(clnt_addr));
    
    int connfd = accept(sockfd, (struct sockaddr *)&clnt_addr, &clnt_addr_len);
    if(connfd < 0) {
        perror("accept failed.");
        close(sockfd);
        return -1;
    }else{
        close(STDOUT_FILENO);

        // redirect stdout to connfd
        dup(connfd);
        printf("connect success\n");
        close(connfd);
    }
    
    close(sockfd);

    return 0;
}
