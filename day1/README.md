# Socket基本操作

## 基础结构体

### 通用socket地址

`struct sockaddr` 是一个通用的socket地址结构体，用于表示不同类型的socket地址。它通常与特定的协议族（如AF_INET, AF_INET6）一起使用。其定义如下：

```c
struct sockaddr {
    unsigned short sa_family;   // 地址族
    char sa_data[14];           // 地址数据
};
```

- `sa_family`：指定地址族，例如AF_INET（IPv4）或AF_INET6（IPv6）。
- `sa_data`：包含套接字地址数据，具体内容依赖于地址族。

这个结构体通常不会直接使用，而是通过特定协议族的地址结构体（如`struct sockaddr_in`）进行操作。

**使用方法**：通常在`bind`,`accept`等系统调用函数中把一些结构体强制转型成`sockaddr`

### 专用socket地址

#### `struct sockaddr_in`

`struct sockaddr_in` 是用于IPv4地址的专用socket地址结构体。其定义如下：

```c
struct sockaddr_in {
    short int sin_family;       // 地址族（应设置为AF_INET）
    unsigned short int sin_port; // 端口号
    struct in_addr sin_addr;    // IPv4地址
    unsigned char sin_zero[8];  // 填充字节，必须为0
};
```

- `sin_family`：地址族，必须设置为AF_INET。
- `sin_port`：端口号，使用`htons`函数将主机字节序转换为网络字节序。
- `sin_addr`：IPv4地址，使用`inet_addr`或`inet_aton`函数设置。
- `sin_zero`：填充字节，通常设置为0。

#### `struct in_addr`

`struct in_addr` 是用于表示IPv4地址的结构体。其定义如下：

```c
struct in_addr {
    unsigned long s_addr; // 32位IPv4地址
};
```

- `s_addr`：32位IPv4地址，通常使用`inet_addr`或`inet_aton`函数设置。

#### `struct sockaddr_in6`

`struct sockaddr_in6` 是用于IPv6地址的专用socket地址结构体。其定义如下：

```c
struct sockaddr_in6 {
    u_int16_t sin6_family;      // 地址族（应设置为AF_INET6）
    u_int16_t sin6_port;        // 端口号
    u_int32_t sin6_flowinfo;    // 流信息
    struct in6_addr sin6_addr;  // IPv6地址
    u_int32_t sin6_scope_id;    // 作用域ID
};
```

- `sin6_family`：地址族，必须设置为AF_INET6。
- `sin6_port`：端口号，使用`htons`函数将主机字节序转换为网络字节序。
- `sin6_flowinfo`：流信息，通常设置为0。
- `sin6_addr`：IPv6地址，使用`inet_pton`函数设置。
- `sin6_scope_id`：作用域ID，通常用于本地链路地址。

#### `struct in6_addr`

`struct in6_addr` 是用于表示IPv6地址的结构体。其定义如下：

```c
struct in6_addr {
    unsigned char s6_addr[16]; // 128位IPv6地址
};
```

- `s6_addr`：128位IPv6地址，通常使用`inet_pton`函数设置。



## 常用函数

### IP地址转换函数

- `inet_addr`：将IPv4地址的点分十进制字符串转换为网络字节序的32位二进制值。
- `inet_aton`：将IPv4地址的点分十进制字符串转换为网络字节序的32位二进制值，并存储在结构体中。
- `inet_ntoa`：将网络字节序的IPv4地址转换为点分十进制字符串。
- `inet_pton`：将文本格式的IP地址（IPv4或IPv6）转换为网络字节序的二进制值。
- `inet_ntop`：将网络字节序的IP地址（IPv4或IPv6）转换为文本格式。


## 创建socket

```cpp
#include <sys/types.h>
#include <sys/socket.h>
int socket(int domain, int type, int protocol);
```

## 命名socket
```cpp
#include <sys/types.h>
#include <sys/socket.h>
int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
```


## 监听socket
```cpp
#include <sys/socket.h>
int listen(int sockfd, int backlog);
```


## 接受连接
```cpp
#include <sys/types.h>
#include <sys/socket.h>
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

## 发起连接

```cpp
#include <sys/types.h>
#include <sys/socket.h>
int connect(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen); 
```

## 关闭链接

#### close
引用-1

```cpp
#include <sys/socket.h>
int close(int sockfd);
```

#### shutdown
立刻关闭

```cpp
#include <sys/socket.h>
int shutdown(int fd, int howto);
```

`howto`可以选择
- SHUT_RD   关闭读
- SHUT_WR   关闭写
- SHUT_RDWR 关闭读写
