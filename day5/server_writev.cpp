#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/uio.h>

#define BUFFER_SIZE 1024
static const char* status_line[2] = {"200 OK", "500 Internal Server Error"};
static const char* fname = "hello.txt";

int main(int argc, char const *argv[])
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd != -1);
    
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(8888);
    
    int ret = bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    assert(ret != -1);

    ret = listen(sockfd, SOMAXCONN);
    assert(ret != -1);

    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    bzero(&clnt_addr, sizeof(clnt_addr));
    
    int connfd = accept(sockfd, (struct sockaddr *)&clnt_addr, &clnt_addr_len);
    if(connfd < 0) {
        perror("accept failed.");
        close(sockfd);
        return -1;
    }else{
        /*http header buffer, including http response status line, header fields and an empty line */
        char header_buf[BUFFER_SIZE];
        memset(header_buf, '\0', BUFFER_SIZE);

        /* buffer for file */
        char* file_buf;   
        
        /* get file's attributes */
        struct stat file_stat;

        /* vaild file */
        bool valid = true;

        /* the buffer space already used */
        int len = 0;
        if(stat(fname, &file_stat) < 0){
            valid=false;
        }else{
            if(S_ISDIR(file_stat.st_mode)){
                valid=false;
            }else if(file_stat.st_mode & S_IROTH){// have right to read
                /* open file */
                int fd = open(fname, O_RDONLY);
                /* allocate buffer dynamically */
                file_buf = new char[file_stat.st_size+1];
                memset(file_buf, '\0', file_stat.st_size+1);
                if(read(fd, file_buf, file_stat.st_size) < 0){
                    valid=false;
                }
            }else{
                valid=false;
            }
        }

        if(valid){
            /* write http status line */
            ret = snprintf(header_buf, BUFFER_SIZE-1, "HTTP/1.1 %s\r\n", status_line[0]);
            len += ret;

            /* write http content-length */
            ret = snprintf(header_buf + len, BUFFER_SIZE-1-len, "Content-Length: %d\r\n", file_stat.st_size);
            len += ret;

            /* write empty line */
            ret = snprintf(header_buf + len, BUFFER_SIZE-1-len, "%s", "\r\n");

            /* writev */
            struct iovec iv[2];
            iv[0].iov_base = header_buf;
            iv[0].iov_len = strlen(header_buf);
            iv[1].iov_base = file_buf;
            iv[1].iov_len = file_stat.st_size;
            ret = writev(connfd, iv, 2);

        }else{
            /* file invalid , return 500 */
            ret = snprintf(header_buf, BUFFER_SIZE-1, "HTTP1.1 %s\r\n", status_line[1]);
            len+=ret;

            ret = snprintf(header_buf + len,  BUFFER_SIZE - 1 - len, "%s", "\r\n");
            send(connfd, header_buf, strlen(header_buf), 0);
        }

        close(connfd);
        delete [] file_buf;
    }
    
    close(sockfd);

    return 0;
}
