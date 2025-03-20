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
    serv_addr.sin_port = htons(8888);
    
    int ret = bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    assert(ret != -1);

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
        int filefd = open(fname, O_RDONLY);
        assert(filefd > 0);
        struct stat stat_buf;
        fstat(filefd, &stat_buf);

        sendfile(connfd, filefd, NULL, &stat_buf.st_size);
        close(connfd);
    }
    
    close(sockfd);
    return 0;
}
