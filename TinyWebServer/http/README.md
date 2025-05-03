# http连接处理类

## background


### http报文结构

HTTP报文本质上是由多行数据组成的**字符串文本**。下图为一个简单的http请求报文转换为文本的示例。


![](https://segmentfault.com/img/bVbvbXu/view?w=2374&h=778)


#### 请求报文

![](https://tsejx.github.io/javascript-guidebook/static/http-request-structure.aa6a5498.png)

结构：
- 请求行
- 请求头
- 空行
- 请求主体

其解析比较方便，直观上理解，首先可以通过一个空行分开请求体。剩下的部分，第一行必定是请求行，其余每行都是一个请求头，可以使用冒号做分割，即可得到对应的字段和值。

## 请求报文解析实现

代码实现是通过状态机实现的。
根据状态转移,通过主从状态机封装了http连接类。其中,主状态机在内部调用从状态机,从状态机将处理状态和数据传给主状态机.

主从状态机架构：

![](https://mmbiz.qpic.cn/mmbiz_jpg/6OkibcrXVmBH2ZO50WrURwTiaNKTH7tCia3AR4WeKu2EEzSgKibXzG4oa4WaPfGutwBqCJtemia3rc5V1wupvOLFjzQ/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1)

主状态机有三种状态，分别表示解析HTTP请求报文的三个部分。

-   CHECK_STATE_REQUESTLINE，解析请求行
-   CHECK_STATE_HEADER，解析请求头
-   CHECK_STATE_CONTENT，解析消息体，仅用于解析POST请求

从状态机也有三种状态，标识解析一行的状态。

-   LINE_OK，完整读取一行
-   LINE_BAD，报文语法有误
-   LINE_OPEN，读取的行不完整


主要是`while`循环，对报文每一行进行循环处理。在循环中，不断解析每一行数据，并修改主从状态机状态。

**从状态机逻辑**
在`parse_line()`中的for循环，逐字节读取buffer，不是逐行读取！！！读到`\r\n`才算一行。

(1)当前是'\r'
从状态机从`m_read_buf`中逐字节读取，判断当前字节是否为'\r',根据下面一个字节的情况
- '\n'，修改'\r\n'为'\0\0'， 将`m_checked_idx`指向下一行的开头，返回`LINE_OK`
- 到达buffer末尾，说明还需要继续接收，返回`LINE_OPEN`
- 否则返回语法错误`LINE_BAD`

(2)当前是'\n' （这种情况就是(1)中读到buffer末尾了）
如果前一个字节是'\r'，则将'\r\n'修改成'\0\0', 将`m_checked_idx`指向下一行的开头，返回`LINE_OK`。

(3)当前字节既不是'\r'也不是'\n'，表示不完整，还要继续接收, 返回`LINE_OPEN`

`LINE_OPEN`出现时，继续从socket读数据添加到buffer中，

**主状态机逻辑**

主状态机主要负责http请求报文三个部分的解析，分别由三个函数完成

- `parse_request_line()` 负责解析请求行，获取到请求方法(get/post)、url路径、协议版本
- `parse_headers()`
- `parse_content()`


1. **`parse_request_line()`** 

    执行此函数时，主状态机的状态是 `CHECK_STATE_REQUESTLINE`。
    调用该函数，从`m_read_buf`中解析HTTP请求行，获得请求行内的信息
    解析完成后主状态机的状态变为`CHECK_STATE_HEADER`


2. **`parse_request_headers()`**
    解析下列字段：
    - Connection 连接管理指令
    - Content-Length
    - host 	请求的服务器主机名和端口号

3. **`parse_request_headers()`**

    请求体的解析比较简单，直接取头部中的Content-length，把对应位置的字符设置成`\0`即可。
    注意不能直接拿到text就设置为请求体，HTTP连接是基于流的，客户端和服务器之间的数据可能是连续发送的，没有明确的结束标志。
    在持久连接（keep-alive）中，多个请求可能连续发送，没有关闭连接，必须依赖Content-Length来准确分割请求体。

## 响应报文生成逻辑

用到的API和数据结构

`struct stat`

```c++
struct stat
{
    mode_t st_mode;  // 文件类型和权限
    off_t st_size;  // 文件大小，字节数
}
```

`mmap` 用于将一个文件或其他对象映射到内存，提高文件的访问速度
```c++
void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *start, size_t length);
```

start：映射区的开始地址，设置为0时表示由系统决定映射区的起始地址
length：映射区的长度
prot：期望的内存保护标志，不能与文件的打开模式冲突
    PROT_READ 表示页内容可以被读取
flags：指定映射对象的类型，映射选项和映射页是否可以共享
    MAP_PRIVATE 建立一个写入时拷贝的私有映射，内存区域的写入不会影响到原文件
fd：有效的文件描述符，一般是由open()函数返回
off_toffset：被映射对象内容的起点