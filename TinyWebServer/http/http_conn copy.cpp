#include "http_conn.h"

//初始化连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
int close_log, string user, string passwd, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;
    doc_root = root;
    // 以下2行代码是跳过time_wait状态，让端口可以重用，用于调试，生产环境需要注释掉
    int reuse=1;
    setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count++;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}

void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;
    
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

// 循环读取客户端数据，直到无数据可读或对方关闭连接
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;
    while (true)
    {
        // 从套接字接收数据，存储在m_read_buf缓冲区
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) // 非阻塞ET模式，需要一次性将数据读完
                break;
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        // 修改m_read_idx的读取字节数
        m_read_idx += bytes_read;
    }
    return true;
}

// epoll相关代码
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}

// 内核事件表注册新事件，开启EPOLLONESHOT, 针对客户端连接的描述符，listenfd不用开启
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    // 根据TRIGMode选择事件类型
    if(TRIGMode == 1){
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else{
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 内核事件表删除事件
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 重置EPOLLONESHOT事件
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if(TRIGMode == 1){
        event.events = ev | EPOLLONESHOT | EPOLLET | EPOLLRDHUP;
    }
    else{
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}

// http报文解析
void http_conn::process()
{
    HTTP_CODE read_ret = process_read();

    // NO_REQUEST, 请求不完整
    if (read_ret == NO_REQUEST)
    {
        // 注册并监听读事件
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }

    // 调用process_write完成报文响应
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }

    // 注册并践行写事件
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}

http_conn::HTTP_CODE http_conn::process_read(){
    // 初始化“从状态机”状态、HTTP请求解析结果
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0; 

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status=parse_line()) == LINE_OK)){ 
        text = get_line();

        // m_start_line 每行在m_read_buf中的起始位置
        // m_checked_idx 从状态机在m_read_buf中读取的位置    
        m_start_line = m_checked_idx;

        switch(m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
                // 解析请求行
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            case CHECK_STATE_HEADER:
                // 解析请求头
                ret = parse_headers(text);
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                else if (ret == GET_REQUEST)
                    return do_request();
                break;

            case CHECK_STATE_CONTENT:
                // 解析请求体
                ret = parse_content(text);

                // 完整解析POST请求后，跳转到报文响应函数
                if (ret == GET_REQUEST)
                    return do_request();

                // 解析完消息体即完成报文解析，避免再次进入循环，更新line_status
                line_status = LINE_OPEN;
                break;
            default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

// 从状态机逻辑
http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;

    for(; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        // temp 就是将要分析的字节
        temp = m_read_buf[m_checked_idx];
        if(temp == '\r'){
            if((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if(m_read_buf[m_checked_idx + 1] == '\n'){
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n'){
            if(m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r'){
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// 主状态机逻辑
http_conn::HTTP_CODE http_conn::parse_request_line(char *text){
    // 解析请求行
    m_url = strpbrk(text, " \t");

    if(!m_url){
        return BAD_REQUEST;
    }

    *m_url++ = '\0';  // 将m_url指向的值设置为'\0'，表示请求行结束

    char *method = text; // 取出请求行中的请求方法
    if(strcasecmp(method, "GET") == 0){
        m_method = GET;
    }
    else if(strcasecmp(method, "POST") == 0){
        m_method = POST;
        cgi = 1;
    }
    else{
        return BAD_REQUEST;
    }

    // 跳过请求行中的空格和制表符，确保下一个字符是有效的
    m_url += strspn(m_url, " \t");
    
    m_version = strpbrk(m_url, " \t");
    if(!m_version){
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    

    // 仅支持HTTP/1.1
    if(strcasecmp(m_version, "HTTP/1.1") != 0){
        return BAD_REQUEST;
    }

    // 对请求资源前7个字符进行判断
    // 单独处理带"http://"的请求资源
    if(strncasecmp(m_url, "http://", 7) == 0){  
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    // 单独处理带"https://"的请求资源
    if(strncasecmp(m_url, "https://", 8) == 0){
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0] != '/'){
       return BAD_REQUEST;
    }

    if(strlen(m_url) == 1){
        strcat(m_url, "judge.html");
    }

    // 主状态机状态转移，接下来解析请求头
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char *text){
    // 仅仅支持部分字段
    // 遇到空行，表示头部字段解析完毕
    if(text[0] == '\0'){
        // 如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，
        if(m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0)
        {
            // 长连接设置m_linger标志为true
            m_linger = true;
        }
    }
    else if(strncasecmp(text, "Content-Length:", 15) == 0)
    {
        // 解析Content-Length字段
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text); // 将字符串转换为长整型
    }
    else if(strncasecmp(text, "Host:", 5) == 0)
    {
        // 解析Host字段
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else{
        printf("oop! unknow header: %s\n", text);
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text){
    // 判断buffer中是否读取了消息体
    if(m_read_idx >= (m_content_length + m_checked_idx)){
        text[m_content_length] = '\0';

        // POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request(){
    // m_real_file 设置为网站根目录
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);

    // 找到m_url中/的位置 根据url的内容，进行下一步处理
    const char* p = strrchr(m_url, '/');

    // 实现登录和注册校验
    if(cgi == 1 && (*(p+1) == '2' || *(p+1) == '3')){
        // TODO: 处理登录和注册
        //根据标志判断是登录检测还是注册检测
        //同步线程登录校验
        //CGI多进程登录校验
        return NO_REQUEST;
    }

    // /0 表示注册界面
    if(*(p+1) == '0'){
        char *m_url_read = (char*)malloc(sizeof(char)* 200);
        strcpy(m_url_read, "/register.html");
        
        // 将网站根目录和/register.html拼接，更新到m_real_file中
        strncpy(m_real_file + len, m_url_read, strlen(m_url_read));

        free(m_url_read);
    }
    // /1 表示登录界面
    else if(*(p+1) == '1'){
        char *m_url_read = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_read, "/login.html");

        // 将网站根目录和/log.html拼接，更新到m_real_file中
        strncpy(m_real_file + len, m_url_read, strlen(m_url_read));

        free(m_url_read);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    

    if(stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;
    
    if(!(m_file_stat.st_mode & S_IROTH)) // 请求的资源没有读取权限
        return FORBIDDEN_REQUEST;
    
    if(S_ISDIR(m_file_stat.st_mode)) // 请求应该是文件而不是目录
        return BAD_REQUEST;
    
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;

    return NO_RESOURCE;
}

bool http_conn::add_response(const char* format, ...){
    // 如果写入内容超出m_write_buf大小则报错
    if(m_write_idx >= WRITE_BUFFER_SIZE)
        return false;

    // 定义可变参数列表
    va_list arg_list;
    // 将变量arg_list初始化为传入参数
    va_start(arg_list, format);

    // 将数据format从可变参数列表写入m_write_buf，返回写入数据的长度
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);

    // 如果写入内容超出m_write_buf大小则报错
    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)){
        // 清空可变参数列表
        va_end(arg_list);
        return false;
    }
    // 更新m_write_idx位置
    m_write_idx += len;
    // 清空可变参数列表
    va_end(arg_list);
    return true;
}
//添加状态行
bool http_conn::add_status_line(int status,const char* title)
{
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}

//添加消息报头，具体的添加文本长度、连接状态和空行
bool http_conn::add_headers(int content_len)
{
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}

//添加Content-Length，表示响应报文的长度
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n",content_len);
}

//添加文本类型，这里是html
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n","text/html");
}

//添加连接状态，通知浏览器端是保持连接还是关闭
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n",(m_linger==true)?"keep-alive":"close");
}
//添加空行
bool http_conn::add_blank_line()
{
    return add_response("%s","\r\n");
}

//添加文本content
bool http_conn::add_content(const char* content)
{
    return add_response("%s",content);
}
//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

bool http_conn::process_write(HTTP_CODE ret){
    switch(ret){
        // 500 服务器内部错误
        case INTERNAL_ERROR:
            // 添加状态行
            add_status_line(500, error_500_title);
            // 添加消息报头
            add_headers(strlen(error_500_form));
            // 添加消息体
            if(!add_content(error_500_form))
                return false;
            break;

        // 400 请求报文有语法错误
        case BAD_REQUEST:
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if(!add_content(error_400_form))
                return false;
            break;

        // 403 请求的资源禁止访问
        case FORBIDDEN_REQUEST:
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form))
                return false;
            break;

        // 200 文件存在
        case FILE_REQUEST:
            add_status_line(200, ok_200_title);
            if(m_file_stat.st_size != 0){
                add_headers(m_file_stat.st_size);

                // 使用两个iovec结构体变量，分别指向数据缓冲区和mmap返回的文件指针
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }
            else{
                // 如果请求资源大小为0，则返回空白html文件
                const char* ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string))
                    return false;
            }
            return true;
        default:
            return false;
    }
            
    // 其余状态只申请一个iovec，指向响应报文缓冲区
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}
