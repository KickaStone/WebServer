#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main()
{
    // socket->connect

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(8080);

    if (connect(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connection failed.");
        close(sockfd);
        return -1;
    }

    // char *http_request = "GET /test.html HTTP/1.1\r\nHost: www.example.com\r\nUser-Agent: CustomTest/1.0\r\nAccept: */*\r\n\r\n";
    // send(sockfd, http_request, strlen(http_request), 0);

    // 发送带外数据
    const char* oob_data = "abc";
    send(sockfd, oob_data, strlen(oob_data), MSG_OOB);
    close(sockfd);
    return 0;
}