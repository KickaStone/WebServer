# 高级I/O函数

## sendfile

sendfile 几乎是专门为在网络上传输文件而设计的。它可以在内核态直接传递数据，避免内核缓冲区和用户缓冲区的拷贝，效率很高。

```c++
#include <sys/sendfile.h>
ssize_t sendfile(int out_fd, int in_fd, off_t* offset, size_t count);
```

`in_fd`：待读出文件描述符
`out_fd`：待写入文件描述符
`offset`：指定从读入文件流的哪个位置开始读
`count`：指定在文件描述符in_fd和out_fd之间传输的字节数。

sendfile成功时返回传输的字节数，失败则返回-1并设置errno。

该函数`in_fd`必须指向真实文件，`out_fd`必须是一个socket。

注意：如果使用macos系统，sendfile在 sys/socket.h中定义，略有区别。

## mmap & munmap

用于申请一段内存空间，将这段内存作为进程间通信的共享内存，也可以将文件直接映射到其中。munmap释放这段空间。如何使用放在后面学习。


## splice 

splice函数用于在两个文件描述符之间移动数据，也是零拷贝操作。splice函数的定义如下：

```c++
#include <fcntl.h>

ssize_t splice(int fd_in, loff_t* off_in, int fd_out, loff_t off_out, size_t len, unsigned int flags);
```

限制：
fd_in,fd_out 必须有一个是管道。

splice函数成功时返回移动字节的数量，可能为0；失败返回-1并设置errno。

## tee

tee也是零拷贝函数，用于在两个**管道**之间进行复制数据。它特点是不消耗数据，音测源文件描述符上的数据仍然可以用于后续操作。

```c++
#include <fcntl.h>
ssize_t tee(int fd_in, int fd_out, size_t len, unsigned int flags);
```

[示例](./test_tee.cpp)实现了一个linux的tee命令，利用tee函数不消耗数据特性，将管道输出端连接到标准输出和文件，实现同时输出到终端和文件。


## fcntl

fcntl函数，正如其名字(file control) 描述的那样，提供了对文件描述符的各种控制操作。另外一个常见的控制文件描述符属性和行为的系统调用是ioctl，而且ioctl 比 fcntl能够执行更多的控制。但是，对于控制文件描述符常用的属性和行为，fcntl函数是由POSIX规范指定的首选方法。所以本书仅讨论fcntl雨数。fcntl函数的定义如下：

```c++
#include <fcntl.h>
int fcntl(int fd, int cmd, ...)
```

该函数可以实现的功能有:
- 复制文件描述符
- 获取和设置文件描述符的标志
- 获取和设置文件描述符的状态标志
- 管理信号
- 管理管道容量