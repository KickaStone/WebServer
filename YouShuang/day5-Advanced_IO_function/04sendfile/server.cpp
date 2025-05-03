#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <errno.h>
#include <sys/sendfile.h>
#define BUFFER_SIZE 1024
static const char* status_line[2] = {"200 OK", "500 Internal Server Error"};
static const char* fname = "hello.txt";

int main(int argc, char const *argv[])
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd != -1);
    
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(8080);
    
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
        /*http header buffer, including http response status line, header fields and an empty line */
        char header_buf[BUFFER_SIZE];
        memset(header_buf, '\0', BUFFER_SIZE);

        /* get file's attributes */
        struct stat file_stat;

        /* vaild file */
        bool valid = true;

        /* the buffer space already used */
        int len = 0;
        if(stat(fname, &file_stat) < 0){
            valid=false;
        }else{
            if(S_ISDIR(file_stat.st_mode)){
                valid=false;
            }else if(file_stat.st_mode & S_IROTH){// have right to read
                /* open file */
                int fd = open(fname, O_RDONLY);
                sendfile(connfd, fd, NULL, file_stat.st_size);
            }else{
                valid=false;
            }
        }

        close(connfd);
    }
    
    close(sockfd);

    return 0;
}
