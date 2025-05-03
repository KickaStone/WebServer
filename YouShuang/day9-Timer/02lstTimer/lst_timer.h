#ifndef LST_TIMER
#define LST_TIMER

#include <time.h>
#include <netinet/in.h>
#include <stdio.h>
#define BUFFER_SIZE 64
class util_timer;
// 基于升序链表的定时器

/**
 * 用户数据结构
 * @param address 客户端地址
 * @param sockfd 客户端socket
 * @param buf 缓冲区
 * @param timer 定时器
 */
struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer *timer;
};

class util_timer
{

public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    time_t expire;                  // 任务超时时间 秒
    void (*cb_func)(client_data *); // 任务回调函数
    client_data *user_data;         // 用户数据
    util_timer *prev;               // 前一个定时器
    util_timer *next;               // 下一个定时器
};

class sort_timer_lst
{
public:
    sort_timer_lst() : head(NULL), tail(NULL) {}
    ~sort_timer_lst()
    {
        util_timer *tmp = head;
        while (tmp)
        {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

private:
    util_timer *head;
    util_timer *tail;


public:
    void add_timer(util_timer *timer);
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer *timer);
    void tick();

private:
    void add_timer(util_timer *timer, util_timer *lst_head);
};

/**
 * 辅助函数，将目标定时器timer添加到给定链表中，专门处理在给定指针后面插入的情况
 * 链表是升序的，是正常的链表插入操作
 * @param timer 目标定时器
 * @param lst_head 链表头指针
 */
inline void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head)
{
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

    if(!tmp){
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}


/**
 * 将目标定时器timer添加到链表中
 * @param timer 目标定时器
 */
inline void sort_timer_lst::add_timer(util_timer *timer){
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
    // 仅在head后面插入的情况
    add_timer(timer, head);
}


/**
 * 调整目标定时器timer在链表中的位置
 * 仅考虑计时器超时时间延长的情况，也就是向后移动
 * @param timer 目标定时器
 */
inline void sort_timer_lst::adjust_timer(util_timer *timer){
    if(!timer){
        return;
    }
    
    util_timer* tmp = timer->next;
    // 如果被调整的目标计时器在秒表尾部，或调整后仍小于下一个计时器的超时时间，则不调整
    if(!tmp || (timer->expire < tmp->expire)){
        return;
    }
    // 如果目标计时器是链表头节点，则将该计时器从链表中取出并重新插入
    if(timer == head){
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    // 如果目标计时器在链表中间，则将该计时器从链表中取出并重新插入
    else{
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}


/**
 * 删除目标定时器timer
 * @param timer 目标定时器
 */
inline void sort_timer_lst::del_timer(util_timer *timer){
    if(!timer){
        return;
    }
    
    if(timer == head && timer == tail){
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    // 如果目标计时器是链表头节点且链表内有多于一个计时器
    if(timer == head){
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    // 如果目标计时器是链表尾节点且链表内有多于一个计时器
    if(timer == tail){
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    // 如果目标计时器在链表中间
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

/**
 * 定时任务处理函数
 */
void sort_timer_lst::tick(){
    if(!head){
        return;
    }
    printf("timer tick\n");
    time_t cur = time(NULL); // 系统当前时间
    util_timer* tmp = head;
    while(tmp){
        // 从链表头部开始处理超时任务
        if(cur < tmp->expire){
            break;
        }
        // 调用回调函数，执行定时任务
        tmp->cb_func(tmp->user_data);
        // 执行完定时任务后，将该计时器从链表中删除，并重置计时器
        head = tmp->next;
        if(head){
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}
#endif
