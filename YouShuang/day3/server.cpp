#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char const *argv[])
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket creation failed.");
        return -1;
    }
    
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(8888);

    if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("bind failed.");
        close(sockfd);
        return -1;
    }

    listen(sockfd, SOMAXCONN);

    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    bzero(&clnt_addr, sizeof(clnt_addr));

    printf("Waiting for new connection...\n");
    int clnt_sockfd = accept(sockfd, (struct sockaddr *)&clnt_addr, &clnt_addr_len);
    if (clnt_sockfd == -1) {
        perror("accept failed.");
        close(sockfd);
        return -1;
    }

    // send some message
    const char *data = "Hello, this is server!";
    send(clnt_sockfd, data, strlen(data), 0);
    send(clnt_sockfd, data, strlen(data), 0);
    send(clnt_sockfd, data, strlen(data), 0);

    const char *urgdata = "This is urgent data!";
    send(clnt_sockfd, urgdata, strlen(urgdata), MSG_OOB);
    send(clnt_sockfd, data, strlen(data), 0);
    
    close(clnt_sockfd);
    close(sockfd);
    return 0;
}
