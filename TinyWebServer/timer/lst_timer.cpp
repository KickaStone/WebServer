#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}

sort_timer_lst::~sort_timer_lst()
{
    util_timer *tmp = head;
    while(tmp){
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer){
    if(!timer){
        return;
    }
    if(!head){
        head = tail = timer;
        return;
    }
    if(timer->expire < head->expire){
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

// 只考虑计时器延长的情况
void sort_timer_lst::adjust_timer(util_timer *timer){
    if(!timer){
        return;
    }
    util_timer *tmp = timer->next;
    // 如果timer是链表中最后一个节点，或者当前计时器超时时间小于下一个计时器超时时间，则不调整
    if(!tmp || (timer->expire < tmp->expire)){
        return;
    }
    // 逻辑都是删除当前节点，然后重新插入到后面的链表
    // 如果timer是头节点，需要调整头节点
    if(timer == head){
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }else{
        // 如果timer不是头节点，则只需调整timer的位置
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void sort_timer_lst::del_timer(util_timer *timer){
    if(!timer){
        return;
    }
    if(timer == head && timer == tail){
        // 如果timer是头节点也是尾节点，则链表中只有一个节点
        delete timer;
        head = tail = NULL;
        return;
    }
    if(timer == head){
        // 如果timer是头节点，则调整头节点
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if(timer == tail){
        // 如果timer是尾节点，则调整尾节点
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    // 如果timer不是头节点也不是尾节点，则调整timer的位置
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

// 处理链表上到期的任务
void sort_timer_lst::tick(){
    if(!head){
        return;
    }

    time_t cur = time(NULL);
    util_timer *tmp = head;
    while(tmp){
        if(cur < tmp->expire){
            break;
        }
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if(head){
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
        
    }
}

void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head){
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    while(tmp){
        if(timer->expire < tmp->expire){
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    // 如果tmp为NULL，则timer是链表中最后一个节点
    if(!tmp){
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void Utils::init(int timeslot){
    m_TIMESLOT = timeslot;
}

int Utils::setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/**
 * 将文件描述符添加到epoll中
 * @param epollfd epoll文件描述符
 * @param fd 文件描述符
 * @param one_shot 是否只触发一次
 * @param TRIGMode 触发模式, 1 = ET, 其他为LT
 */
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if(1 == TRIGMode){
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }else{
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if(one_shot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void Utils::sig_handler(int sig){

    // 保存errno是为了保证函数的可重入性
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void Utils::addsig(int sig, void(handler)(int), bool restart){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}


void Utils::timer_handler(){
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info){
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;

// 定时器回调函数
void cb_func(client_data *user_data){
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}
