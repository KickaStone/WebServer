//
// Created by jijuncheng on 4/13/25.
//

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../mysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    // thread_number 是线程池中线程的数量
    // max_requests 是请求队列中最多允许的、等待处理的请求的数量
    // connPool是数据库连接池指针
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    // 添加任务
    bool append(T *request, int state);

    // 向请求队列中插入任务请求
    // bool append_p(T *request);

private:
    // 工作线程运行的函数，必须为静态函数
    // 不断从工作队列出取出任务并执行
    static void *worker(void *arg);
    // 工作线程实际执行的函数
    void run();

private:
    int m_thread_number;         // 线程池中的线程数
    int m_max_requests;          // 请求队列中运行的最大请求数
    pthread_t *m_threads;        // 描述线程池的数组
    std::list<T *> m_workqueue;  // 请求队列
    locker m_queuelocker;        // 保护请求队列的互斥锁
    sem m_queuestat;             // 是否有任务需要处理 (信号量)
    connection_pool *m_connPool; // 数据库连接池
    int m_actor_model;           // 模型切换
};

#endif // THREADPOOL_H

template <typename T>
inline threadpool<T>::threadpool(   int actor_model, 
                                    connection_pool *connPool, 
                                    int thread_number, 
                                    int max_requests):
    m_connPool(connPool), 
    m_thread_number(thread_number), 
    m_max_requests(max_requests), 
    m_threads(NULL),
    m_actor_model(actor_model)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number]; // 创建线程数组
    if (!m_threads)
        throw std::exception();

    for (int i = 0; i < thread_number; ++i)
    {
        // 循环创建线程，并将工作线程按要求进行运行
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            // 创建正常返回0， 参数是：
            // pthread_t *tid, 指针id存储位置;
            // pthread_attr_t *attr, 线程属性，NULL则使用默认值;
            // void *(*start_routine), 指向线程执行函数的指针，必须为静态函数；
            // void *arg传给start_routine的参数
            delete[] m_threads;
            throw std::exception();
        }

        // 将线程进行分离后，不用单独对工作线程进回收。处于分离状态的线程在结束时会自动释放资源，无需调用pthread_join来回收其资源。
        if (pthread_detach(m_threads[i]))
        { // 分离正常返回0
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
inline threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

/**
 * 添加任务
 * @param request 任务请求
 * @param state 0: 读 1: 写
 * @return 是否添加成功
 */
template <typename T>
inline bool threadpool<T>::append(T *request, int state)
{
    m_queuelocker.lock();

    // 根据硬件提前设置最大请求数
    if (m_workqueue.size() >= m_max_requests)
    { // 超过最大请求容量
        m_queuelocker.unlock();
        return false;
    }
    // request->m_state = state;

    // 添加任务
    m_workqueue.push_back(request);
    m_queuelocker.unlock(); // 解除互斥锁
    m_queuestat.post();     // P操作，信号量提醒有任务要处理
    return true;
}

// template <typename T>
// inline bool threadpool<T>::append_p(T *request)
// {
//     m_queuelocker.lock();

//     // 根据硬件提前设置最大请求数
//     if (m_workqueue.size() >= m_max_requests)
//     { // 超过最大请求容量
//         m_queuelocker.unlock();
//         return false;
//     }

//     // 添加任务
//     m_workqueue.push_back(request);
//     m_queuelocker.unlock();
//     m_queuestat.post();
//     return true;
// }

template <typename T>
inline void *threadpool<T>::worker(void *arg)
{
    // 参数强转为线程池类，调用run方法
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
inline void threadpool<T>::run()
{
    while (true) //
    {
        // 信号量等待
        m_queuestat.wait();

        // 被唤醒后先加互斥锁
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }

        // 从请求队列中取出第一个任务
        // 将任务中请求队列删除
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();


        if (!request)
            continue;

        if(m_actor_model == 1){
            // Reactor 模式
            // TODO: 实现Reactor模式
            exit(-1);

        }else{
            // Proactor 模式
            request->mysql = m_connPool->GetConnection();
            // process(模板类中的方法，这里是http类)进行处理
            request->process();
        }
    }
}
