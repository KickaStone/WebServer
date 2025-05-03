/**
 * 实现RAII（资源获取即初始化）
 * 封装linux下三种锁，将锁的创建与销毁函数封装在类的构造和析构函数中
 */
#if !defined(LOCKER_H)
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>


/**
 * 信号量 SV
 * P等待 V信号 
 * P, SV大于0时，将其-1,如果为0则挂起；对应sem_wait()
 * V，如果有其他进程等待SV，唤醒其中一个，否则将SV+1；对应sem_post()
 */
class sem{
public:
    sem(){
        // 信号量初始化
        if(sem_init(&m_sem,0,0)!=0){
            throw std::exception();
        }
    }
    sem(int num){
        if(sem_init(&m_sem,0,num)!=0){
            throw std::exception();
        }
    }

    ~sem(){
        // 信号量销毁
        sem_destroy(&m_sem);
    }

    bool wait(){
        return sem_wait(&m_sem) == 0;
    }

    bool post(){
        return sem_post(&m_sem) == 0;
    }
    
private:
    sem_t m_sem;
};


class locker
{
public:
    locker(){
        if(pthread_mutex_init(&m_mutex, NULL)!=0){
            throw std::exception();
        }
    }
    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t *get(){
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};


class cond
{
private:
    pthread_cond_t m_cond;
public:
    cond(){
        if(pthread_cond_init(&m_cond, NULL) !=0){
            throw std::exception();
        }
    };
    ~cond(){pthread_cond_destroy(&m_cond);};
    
    bool wait(pthread_mutex_t *m_mutex){
        return pthread_cond_wait(&m_cond, m_mutex) == 0; // 这里对m_mutex的加锁需要在执行wait前进行，有外部手动完成。
    }

    bool timewait(pthread_mutex_t *m_mutex, struct timespec t){
        return pthread_cond_timedwait(&m_cond, m_mutex, &t) == 0;
    }

    bool signal(){
        return pthread_cond_signal(&m_cond) == 0;
    }

    bool broadcast(){
        return pthread_cond_broadcast(&m_cond) == 0;
    }
};



#endif // LOCKER_H
