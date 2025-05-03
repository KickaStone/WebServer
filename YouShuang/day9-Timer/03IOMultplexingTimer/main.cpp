#include <stdio.h>
#include <time.h>
#include <sys/epoll.h>
#include <errno.h>
#define TIMEOUT 5000
#define MAX_EVENT_NUMBER 1024

int main(int argc, char const *argv[])
{
    int timeout = TIMEOUT;
    time_t start = time(NULL);
    time_t end = time(NULL);

    int epollfd = epoll_create(5);
    struct epoll_event events[1024];

    while(1){
        printf("the timeout is now %d\n", timeout);
        start = time(NULL);
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, timeout);
        if((number < 0) && (errno != EINTR)){
            printf("epoll failure\n");
            break;
        }
        if(number == 0){
            timeout = TIMEOUT;
            continue;
        }
        end = time(NULL);
        /* 如果epoll_wait返回值大于0,则本次epoll_wait返回后，timeout需要减去本次等待时间 */
        timeout -= (end - start) * 1000; 
        printf("timeout: %d\n", timeout);

        if (timeout <= 0){
            printf("timeout\n");
            break;
        }
    }
    return 0;
}

