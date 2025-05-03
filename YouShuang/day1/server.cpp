#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

int main()
{
    // socket->bind->listen->accept

    /* 1. create a socket */

    /**
     * create socket
     * param1: address family (AF_INET: IPv4)
     * param2: socket type (SOCK_STREAM: TCP)
     * param3: protocol (0: default), based on param1 and param2
     *
     * return: socket file descriptor, -1 if failed
     * each socket is identified by a unique socket file descriptor
     */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1)
    {
        /**
         * errno: a global variable that is set when an error occurs
         *   defined in <errno.h> ; int type;
         *   not thread-safe, use strerror(errno) to get error message
         */
        perror("socket creation failed."); // get errno and print error message
        // printf("socket creation failed: %d\n", errno);
        return -1;
    }

    /* 2.bind socket with address and port */

    // /*
    //  * Socket address, internet style.
    //  */
    // struct sockaddr_in {
    // 	__uint8_t       sin_len;
    // 	sa_family_t     sin_family;
    // 	in_port_t       sin_port;
    // 	struct  in_addr sin_addr;
    // 	char            sin_zero[8];
    // };
    struct sockaddr_in serv_addr;                       // a struct to store server address
    bzero(&serv_addr, sizeof(serv_addr));               // set all bytes to 0
    serv_addr.sin_family = AF_INET;                     // set address family to IPv4
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // set IP address, use inet_addr() to convert string to netork byte order int type
    serv_addr.sin_port = htons(8888);                   // set port number, htons: host to network short

    /**
     * bind socket
     * int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
     *
     * param1: socket file descriptor, created by socket()
     * param2: server address, a struct sockaddr_in
     * param3: size of server address, usually use sizeof(serv_addr) to get the size
     *
     * return: 0 if success, -1 if failed
     *
     * errno:
        EADDRINUSE：指定的地址已被使用，不能再次绑定。
        EADDRNOTAVAIL：指定的地址在当前主机上不可用。
        ENOTSOCK：指定的文件描述符不是一个套接字。
        EINVAL：提供的参数无效。
     */
    if (bind(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("bind failed.");
        close(sockfd); // close() is defined in unistd.h, close file descriptor,
                       // since sockfd is considered as a file, it can be closed by close()
        return -1;
    }

    /* 3. listen */

    /**
     * listen for incoming connections
     * int listen(int sockfd, int backlog);
     *
     * param1: socket file descriptor
     * param2: maximum length of the queue of pending connections (backlog)
     *
     * return 0 if success, -1 if failed
     */
    listen(sockfd, SOMAXCONN);

    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    bzero(&clnt_addr, sizeof(clnt_addr));

    /**
     * accept a connection on a socket
     * int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
     *
     * param1: socket file descriptor
     * param2: client address, a struct sockaddr_in
     * param3: size of client address, usually use sizeof(clnt_addr) to get the size
     */
    int clnt_sockfd = accept(sockfd, (sockaddr *)&clnt_addr, &clnt_addr_len);

    printf("new client fd %d! IP: %s Port: %d\n", clnt_sockfd, inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));
    return 0;
}