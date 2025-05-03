# Socket数据读写

## API

```cpp
#include <sys/types.h>
#include <sys/socket.h>

ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t send(int sockfd, void *buf, size_t len, int flags);

```

recv读取sockfd上的数据，buf和len分别指定缓冲区的位置和大小，flags参数含义见后文，通常设置为0即可。recv成功时返回实际读取到的长度，可能小于我们期望的长度，因此可能到多次调用recv才能读取到完整的数据。

recv返回0,意味着通信对方已经关闭连接了。recv出错时返回-1并设置errno。

send和recv类似。


flags参数用于控制数据的读写行为，常见的flags值包括：
- MSG_OOB：读写带外数据（out-of-band data）
- MSG_PEEK：从缓冲区中窥视数据，但不移除数据
- MSG_DONTWAIT：如果数据不可用，则不阻塞，直接返回
- MSG_WAITALL：如果数据不可用，则阻塞，直到所有数据可用
- MSG_NOSIGNAL：如果对方关闭连接，则不发送SIGPIPE信号

## UDP

```cpp
ssize_t recvfrom(
    int sockfd, 
    void *buf, 
    size_t len, 
    int flags, 
    struct sockaddr *src_addr, 
    socklen_t *addrlen);
ssize_t sendto(
    int sockfd, 
    const void *buf, 
    size_t len, 
    int flags, 
    const struct sockaddr *dest_addr, 
    socklen_t addrlen);
```

UDP没有连接的概念，每次都需要获取发送端的socket地址，即src_addr内容，和地址长度addrlen

这两个还可以用于tcp连接，只需要把最后两个参数设置为null即可。


## 通用数据读写函数

```cpp
#include <sys/socket.h>
ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags);
ssize_t sendmsg(int sockfd, struct msghdr* msg, int flags);

struct msghdr {
    void            *msg_name;      /* optional address */
    socklen_t       msg_namelen;    /* size of address */
    struct iovec    *msg_iov;       /* scatter/gather array */
    int             msg_iovlen;     /* # elements in msg_iov */
    void            *msg_control;    /* ancillary data */
    socklen_t       msg_controllen; /* ancillary data buffer len */
    int             msg_flags;      /* flags on received message */
};
```

msg_name成员指向一个socket地址结构变量，指定对方的socket通信地址，对tcp协议无意义，必须设置为NULL。因为tcp已经知道对方地址。

msg_iov成员是一个存放buffer信息的结构体的指针，结构体内有buffer地址和长度。

msg_control和msg_controllen成员用于辅助数据的传送。

msg_flag参数无需设置，会复制recvmsg和sendmsg中的flags参数。
