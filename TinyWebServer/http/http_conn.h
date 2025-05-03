#if !defined(HTTPCONNECT_H)
#define HTTPCONNECT_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
#include <string>

#include "../lock/locker.h"
#include "../mysql/sql_connection_pool.h"
// #include "../log/log.h"

class http_conn
{
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
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
        PATH
    };

    enum CHECK_STATE // 主状态机状态
    {
        CHECK_STATE_REQUESTLINE = 0, // 解析请求行
        CHECK_STATE_HEADER,          // 解析请求头
        CHECK_STATE_CONTENT          // 解析消息体
    };

    enum HTTP_CODE
    {
        NO_REQUEST,        // 请求不完整，需要继续请求报文数据
        GET_REQUEST,       // 获取了完整的http请求
        BAD_REQUEST,       // 报文有语法错误
        NO_RESOURCE,       // 没有资源
        FORBIDDEN_REQUEST, // 禁止访问
        FILE_REQUEST,      // 文件请求
        INTERNAL_ERROR,    // 服务器内部错误， 该结果在主状态机switch的default状态下，一般不会触发
        CLOSED_CONNECTION  // 连接已关闭
    };
    /**
     * 这里有8种情形，但实际报文解析只用到了4种，NO_REQUEST, GET_REQUEST, BAD_REQUEST, INTERNAL_ERROR
     */

    enum LINE_STATUS // 从状态机状态
    {
        LINE_OK = 0, // 完整读取一行
        LINE_BAD,    // 报文语法错误
        LINE_OPEN    // 读取的行不完整
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname); // 初始化套接字地址，函数内部会调用私有方法init
    void close_conn(bool real_close = true);                                                                      // 关闭http连接
    void process();
    bool read_once(); // 读取浏览器端发来的全部数据
    bool write();     // 响应报文写入
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    void initmysql_result(connection_pool *connPool); // 同步线程初始化数据库读取表
    int timer_flag;
    int improv;

private:
    void init();
    HTTP_CODE process_read();                 // 从m_read_buf读取，并处理请求报文
    bool process_write(HTTP_CODE ret);        // 生成响应报文
    HTTP_CODE parse_request_line(char *text); // 主状态机解析请求行数据
    HTTP_CODE parse_headers(char *text);      // 主状态机解析请求头数据
    HTTP_CODE parse_content(char *text);      // 主状态机解析请求内容
    HTTP_CODE do_request();                   // 生成响应报文
    LINE_STATUS parse_line();                 // 从状态机读取一行，分析是请求报文的哪一部分
    char *get_line()                          // 用于指针向后偏移，指向未处理的字符
    {
        return m_read_buf + m_start_line;
    }
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_len);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;
    int m_state;
private:
    int m_sockfd;                      // http连接socket地址
    sockaddr_in m_address;             // socket的ip地址
    char m_read_buf[READ_BUFFER_SIZE]; // 存储读取的请求报文数据
    int m_read_idx;                    // 缓冲区m_read_buf中数据的最后一个字节的下一个位置
    int m_checked_idx;                 // m_read_buf读取的位置
    int m_start_line;                  // m_read_buf中已经解析的字符数

    CHECK_STATE m_check_state; // 主状态机的状态
    METHOD m_method;           // 请求状态

    char m_write_buf[WRITE_BUFFER_SIZE]; // 存储发送的响应报文数据
    int m_write_idx;                     // 指示buffer中响应报文的长度

    // 解析请求报文中对应的六个变量
    char m_real_file[FILENAME_LEN]; // 存储文件名称
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;

    char *m_file_address; // 读取服务器上的文件地址
    struct stat m_file_stat;
    struct iovec m_iv[2]; // io向量机制iovec
    int m_iv_count;
    int cgi;             // 是否启用POST
    char *m_string;      // 存储请求头数据
    int bytes_to_send;   // 剩余发送字节数
    int bytes_have_send; // 已发送字节数

    char *doc_root;              // 网站根目录地址
    map<string, string> m_users; // 用户名和密码
    int m_TRIGMode;              // 触发模式 0: LT 1: ET
    int m_close_log;             // 是否关闭日志

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif // HTTPCONNECT_H
