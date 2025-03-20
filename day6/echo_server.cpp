/**
 * Echo server by `splice`
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

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
        int pipefd[2];
        ret = pipe(pipefd);
        assert(ret!=-1);

        ret = splice(connfd, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);

        assert(ret!=-1);

        ret = splice(pipefd[0], NULL, connfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        assert(ret=-1);

        close(connfd);
    }

    close(sockfd);
    return 0;
}
