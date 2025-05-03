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
    serv_addr.sin_port = htons(8888);

    // bind(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)); // no need to bind for client

    if (connect(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connection failed.");
        close(sockfd);
        return -1;
    }

    char buffer[BUFFER_SIZE];

    while (1)
    {
        bzero(buffer, BUFFER_SIZE);

        if (sockatmark(sockfd))
        {
            printf("urgent data!\n");
            int ret = recv(sockfd, buffer, BUFFER_SIZE - 1, MSG_OOB);
            if (ret > 0)
            {
                printf("receive %d bytes of urgent data: %s\n", ret, buffer);
            }
        }
        else
        {
            // recieve normal data
            int ret = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
            if (ret > 0)
            {
                printf("receive %d bytes of normal data: %s\n", ret, buffer);
            }
        }
    }

    close(sockfd);
    return 0;
}