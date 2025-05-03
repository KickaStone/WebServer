#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#define BUFFER_SIZE 4096

enum CHECK_STATE{
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER
};

enum LINE_STATUS{
    LINE_OK = 0,
    LINE_BAD,
    LINE_OPEN
};

enum HTTP_CODE{
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    FORBIDDEN_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION
};

/**
 * 为了简化，并没有发送一个完整的http响应报文，
 * 只是根据服务器处理结果发送成功或失败信息
 */

static const char* szret[] = {
    "I get a correct result\n",
    "Something wrong\n"
};

LINE_STATUS parse_line(char* buffer, int &checked_index, int &read_index){
    // 找 \r\n
    char temp;

    for(; checked_index < read_index; ++checked_index)
    {
        temp = buffer[checked_index];
        if(temp == '\r'){
            if((checked_index + 1) == read_index){ // 表示读取到一行结束
                return LINE_OPEN;
            }
            else if(buffer[checked_index + 1] == '\n'){
                buffer[checked_index++] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }        
        else if(temp == '\n'){
            if((checked_index > 1) && (buffer[checked_index - 1] == '\r')){ // 可能这个'\n'是新接收到的，和之前'\r'连着
                buffer[checked_index - 1] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// 解析请求行
HTTP_CODE parse_requestline(char* temp, CHECK_STATE &checkstate){
    // [method]sp[url]sp[version]
    char *url = strpbrk(temp, " \t");
    if(!url){
        return BAD_REQUEST;
    }
    *url++ = '\0'; // 分割出method

    char* method = temp;
    if(strcasecmp(method, "GET") == 0){ // 仅支持GET
        // 处理GET请求
        printf("The request method is GET\n");
    }else{
        return BAD_REQUEST;
    }

    url += strspn(url, " \t"); // 找第一个不是sp或\t的字符，并将url指针到该字符
    char* version = strpbrk(url, " \t");
    if(!version){
        return BAD_REQUEST;
    }
    *version++ = '\0';
    version += strspn(version, " \t");

    if(strcasecmp(version, "HTTP/1.1") != 0){ // 仅支持HTTP/1.1
        return BAD_REQUEST;
    }

    // 检查url是否合法

    if(strncasecmp(url, "http://", 7) == 0){ // 支持http://
        url += 7;
        url = strchr(url, '/');
    }

    if(!url || url[0] != '/'){ // url必须是'/'开头
        return BAD_REQUEST;
    }

    printf("The request URL is: %s\n", url);

    checkstate = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HTTP_CODE parse_headers(char* temp){
    if(temp[0] == '\0'){
        return GET_REQUEST;
    }else if(strncasecmp(temp, "Host:", 5) == 0){
        temp += 5;
        temp += strspn(temp, " \t");
        printf("The host is: %s\n", temp);
    }else{
        printf("I can not handle this header\n");
        // 其他header不处理
    }
    return NO_REQUEST;
}
/**
 * @buffer 缓冲区
 * @checked_index 当前正在分析的字符在缓冲区中的位置
 * @checkstate 当前状态机状态
 * @read_index 当前读取到的字符在缓冲区中的位置
 * @start_line 当前正在分析的行的起始位置
 */
HTTP_CODE parse_content(char* buffer, int& checked_index, CHECK_STATE& checkstate,
                        int& read_index, int& start_line){
    LINE_STATUS linestatus = LINE_OK;
    HTTP_CODE retcode = NO_REQUEST;
    while((linestatus = parse_line(buffer, checked_index, read_index)) == LINE_OK){
        char* line_buf = buffer + start_line;
        start_line = checked_index;
        switch(checkstate){
            case CHECK_STATE_REQUESTLINE:
                retcode = parse_requestline(line_buf, checkstate);
                if(retcode == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                break;
            case CHECK_STATE_HEADER:
                retcode = parse_headers(line_buf);
                if(retcode == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                else if(retcode == GET_REQUEST){
                    return GET_REQUEST;
                }
                break;
            default:
                return INTERNAL_ERROR;
        }
    }

    // 没有读到完整的行，表示还需要继续读取客户数据
    if(linestatus == LINE_OPEN){
        return NO_REQUEST;
    }
    else{
        return BAD_REQUEST;
    }
}

    
int main(int argc, char const *argv[])
{
    if(argc <= 2)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    
    ret = listen(listenfd, 5);
    assert(ret != -1);

    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);

    int fd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
    if(fd < 0)
    {
        printf("errno is: %d\n", errno);
    }
    else
    {
        printf("connection success\n");
        char buffer[BUFFER_SIZE];
        memset(buffer, '\0', BUFFER_SIZE); // 初始化读缓冲区
        int data_read = 0;
        int read_index = 0;
        int checked_index = 0;
        int start_line = 0;

        /* 设置主状态机的初始状态 */
        CHECK_STATE checkstate = CHECK_STATE_REQUESTLINE;

        while(1){
            // 循环读取用户数据
            data_read = recv(fd, buffer + read_index, BUFFER_SIZE - read_index, 0);
            if(data_read == -1)
            {
                printf("reading failed\n");
            }else if(data_read == 0)
            {
                printf("read close\n");
                break;
            }

            printf("data_read: %d\n", data_read);
            read_index += data_read;

            // 分析数据
            HTTP_CODE result = parse_content(buffer, checked_index, checkstate, read_index, start_line);
            if(result == NO_REQUEST)
            {
                continue;
            }
            else if(result == GET_REQUEST)
            {
                send(fd, szret[0], strlen(szret[0]), 0);
                break;
            }
            else {
                send(fd, szret[1], strlen(szret[1]), 0);
                break;
            }
            
        }
        close(fd);
        printf("close fd\n");
    }
    close(listenfd);
    return 0;
}
