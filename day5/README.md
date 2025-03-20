# 高级I/O函数

## dup函数和dup2函数

有时我们希望把标准输入重定向到一个文件，或者把标准输出重定向到一个网络连接（CGI）编程。可以通过`dup``dup2`实现。

```c++
#include <unistd.h>
int dup(int fd);
int dup2(int fd_one, int fd_two);
```
`dup`函数创建一个新的文件描述符，该新文件描述符和原有文件描述符fd指向相同的文件、管道或者网络连接。并且dup返回的fd总是取系统当前可用的最小整数值。

`dup2`类似，但它返回第一个不小于fd_two的整数值。

失败时返回-1并设置errno。
·
dup和dup2创建的fd不继承原文件描述符的树形。

示例server是一个简单的CGI服务器。通用网关接口（Common Gateway Interface）是一个Web服务器主机提供信息服务的标准接口。

程序先关闭标准输出文件描述符`STDOUT_FILENO`，其值为1，然后复制socket文件描述符connfd。因为dup总是返回系统中最小可用fd，所以本程序一定返回1。服务器输出到标准输出的内容就会直接发送到客户链接对应的socket上。

## readv 和 writev

readv是分散读，将数据从文件描述符读取到不连续的内存块中  
write是集中写，将数据从不同的内存块集中写到一个文件描述符中

```c++
#include <sys/uio.h>
ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
```
`readv`函数从文件描述符fd读取数据到iovec结构体数组iov中，数组的大小为iovcnt。每个iovec结构体指定了一个缓冲区的起始地址和长度。

`writev`函数将iovec结构体数组iov中的数据写入到文件描述符fd中，数组的大小为iovcnt。每个iovec结构体指定了一个缓冲区的起始地址和长度。

成功时返回读取或写入的字节数，失败时返回-1并设置errno。

示例展示了HTTP如何利用writev发送文件。