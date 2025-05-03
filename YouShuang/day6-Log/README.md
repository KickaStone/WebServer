# Linux 服务器日志

## Linux 系统日志

Linux提供一个守护进程来处理系统日志rsyslogd，老板本叫syslogd。

rsyslogd能接收内核和用户进程输入的日志，收到日之后，会把它们输出到一些特定的日志文件。默认情况调试信息保存到/var/log/debug文件，普通信息保存在/var/log/messages文件，内核消息保存到/var/log/kern.log文件。

日志信息如何分发可以在配置文件中设置，rsyslogd主配置文件是/etc/rsyslog.conf，其中主要可以设置的项目有
- 内核日志输入路径
- UDP日志及其监听接口
- TCP日志及其监听接口
- 日志文件的权限
- 子配置文件如/etc/rsyslog.d/*.conf，指定各类日志的目标存储文件

**内核日志**由prinfk等函数打印至内核的环状缓存中，环状缓存的内容直接映射到/proc/kmsg文件中，rsyslogd通过读取该文件获取内核日志。使用 `cat /dev/kmsg`可以查看内核日志。

**用户日志**通过syslog函数生成，该函数将日志生成到一个UNIX本地域socket类型(AF_UNIX)的文件/dev/log中，rsyslogd监听该文件以获取用户进程的输出。

## syslog函数

应用程序使用syslog函数与rsyslogd守护进程通信，定义如下
```c++
#include <syslog.h>
void syslog(int priority, const char* message, ...)
```

该函数采用可变参数来结构化输出。

priority参数是设施值和日志级别的按位或，设置值默认是LOG_USER，日志等级分为8个从0开始，数越小等级越高，但使用等级命即可`LOG_INFO`等。

可以使用下面函数改变syslog的默认输出方式，进一步结构化日志内容。 
```c++
#include <syslog.h>
void openlog(const char* ident, int logopt, int facility);
```
ident参数指定的字符串将被添加到日志消息的日期和时间之后，通常设置为程序的名字。
logopt 参数对后续的syslog调用的行为进行配置。
facility参数可用来修改syslog函数中的默认设施值。

程序开发后若需要关闭调试信息，可以使用setlogmask设置日志掩码，使日志级别大于日志掩码的日志信息被系统忽略。
```c++
int setlogmask(int maskpri);
```

函数返回调用进程先前的日志掩码值。然后使用如下函数关闭日志功能。

```c++
void closelog();
```

## 用户信息

### UID,EUID,GID,EGID

```c++
#include <sys/types.h>
#include <unistd.h>
uid_t getuid();         /* 获取真实用户ID */
uid_t geteuid();        /* 获取有效用户ID */
gid_t getgid();         /* 获取真实组ID */
gid_t getegid();        /* 设置有效组ID */
int setuid();           /* 设置真实用户ID */
int seteuid();          /* 设置有效用户ID */
int setgid();           /* 设置真实组ID */
int setedig();          /* 设置有效组ID */
```

一个进程拥有两个用户ID: UID和EUDID。**EUID存在的目的是方便资源访问：使得运行程序的用户拥有该程序的有效用户的权限。**比如su程序，任何用户都可以使用它来修改自己的账户信息，但修改账户时su程序不得不访问/etc/password文件，而访问该文件需要root权限，那么以普通用户身份启动的su就借助EUID实现这个效果。

ls命令可以看到，su所有者时root，并且它被设置了set-user-id标志，表示任何用户运行su时，其有效用户就是root。

根据有效用户定义，任何su程序的普通用户都有权限访问/etc/password文件。有效用户为root的进程成为特权进程(privileged processes)。EGID与EUID的含义类似，给运行目标程序的组的用户提供有效组的权限。

## 服务程序后台化

linux提供了库函数
```c++
#include <unistd.h>
int daemon(int nochdir, int noclose);
```
nochdir，若为0，则切换到根目录'/'，否则继续使用当前目录。
noclose，若为0，则将标准输入输出和错误输出都重定向到/dev/null, 否则使用原来的设别。
成功返回0，失败返回-1并设置errno。

当然可以自己实现如[示例](./daemonize.h)，流程如下
1. 创建子进程，关闭父进程
2. 设置文件权限掩码，确保守护进程创建的新文件有所需权限（777）
3. 创建新的会话，新进程成为会话领导
4. 更改工作目录
5. 关闭从父进程继承的所有打开文件描述符，包括标准输出输出和标准错误
6. 重定向标准输入、输出、错误输出到/dev/null
