#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <pthread.h>

#include "locker.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;       /* 文件名的最大长度 */
    static const int READ_BUFFER_SIZE = 2048;  /* 读缓冲区的大小 */
    static const int WRITE_BUFFER_SIZE = 1024; /* 写缓冲区的大小 */

    /* HTTP 请求的方法 */
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH
    };

    /* 解析客户请求时，主状态机所处的状态 */
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };

    /* 服务器处理 HTTP 请求的可能结果 */
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    /* 行的读取状态 */
    enum LINE_STATUS
    {
        LINE_OK,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}

    /* 初始化新接受的连接 */
    void init(int sockfd, const sockaddr_in &addr);
    /* 关闭连接 */
    void close_conn(bool real_close = true);
    /* 处理客户请求 */
    void process();
    /* 非阻塞读操作 */
    bool read();
    /* 非阻塞写操作 */
    bool write();

private:
    /* 初始化连接*/
    void init();
    /* 解析HTTP请求 */
    HTTP_CODE process_read();
    /* 填充HTTP应答 */
    bool process_write(HTTP_CODE ret);

    /* 下面的一组函数被process_read调用来分析HTTP请求 */

    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();

    /* 下面这一组函数被process_write调用以填充HTTP应答 */

    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    /* 静态epoll文件描述符,所有socket上的事件都被注册的一个epoll内核事件表中 */
    static int m_epollfd;
    /* 统计用户数量 */
    static int m_user_count;

private:
    int m_sockfd;
    sockaddr_in m_address;

    char m_read_buf[READ_BUFFER_SIZE];   /* 读缓冲区 */
    int m_read_idx;                      /* 标识读缓冲已经读入的客户数据的最后一个字节的下一个位置 */
    int m_checked_idx;                   /* 当前正在分析的字符在读缓冲区的位置 */
    int m_start_line;                    /* 当前正在解析的行的起始位置 */
    char m_write_buf[WRITE_BUFFER_SIZE]; /* 写缓冲区 */
    int m_write_idx;                     /* 写缓冲区中待发送的字节数 */

    CHECK_STATE m_check_state;           /* 主状态机当前所处的状态 */
    METHOD m_method;                     /* 请求方法 */

    char m_real_file[FILENAME_LEN];      /* 客户请求的目标文件的完整路径 */
    char *m_url;                         /* 客户请求的URL */
    char *m_version;                     /* 协议版本，只支持HTTP/1.1 */
    char *m_host;                        /* 主机名 */
    bool m_linger;                        /* 是否保持连接 */
    int m_content_length;                 /* HTTP请求的消息体长度 */


    char* m_file_address;                 /* 目标文件被mmap到内存中的起始位置 */    
    struct stat m_file_stat;              /* 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息 */
    struct iovec m_iv[2];                 /* writev函数用到的缓存区 */
    int m_iv_count;                       /* 缓存区中待发送的iov的个数 */
};

#endif