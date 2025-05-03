# 线程池

## 概念

### 多线程架构
多线程架构可以大幅提高系统性能，异步处理高并发任务。

线程池是一种多线程编程的设计模式，它预先创建了一组线程，线程池中的线程可以被多个任务复用。通过使用线程池，可以有效管理和重用线程资源，从而提高程序的性能和响应速度。

![](https://jasonblog.github.io/note/linux_system/images/580px-Thread_pool.svg.png)

考虑的问题：
1. 为什么用线程池
2. 线程怎么封装，线程池用什么数据结构
3. 线程状态有哪些？ 如何维护
4. 线程数量怎么限制？ 是否引入自动伸缩？
5. 线程怎么消亡？ 如何重复利用？

任务队列怎么实现？任务队列满了怎么办？

### 事件驱动模型

[PPT](https://didawiki.cli.di.unipi.it/lib/exe/fetch.php/magistraleinformatica/tdp/tpd_reactor_proactor.pdf)
[Reactor Video](https://www.youtube.com/watch?v=pmtrUcPs4GQ)

事件驱动模式是一种软件架构设计模式，主要用于构建响应用户输入或系统状态变化的应用程序。
在这种模式中，系统通过事件的产生和处理来驱动业务逻辑的执行，具有以下几个核心概念：
- 事件源：检测和获取事件
- 多路复用器： 等待在设置的事件源上发生事件，并将它们分发给相关的事件处理程序回调
- 事件处理器： 执行响应回调的应用特定操作

Reactor和Proactor是两种事件处理模式，为并发应用程序提供了两种不同的解决方案。它们指出了在网络软件框架中如何有效地启动、接收、解复用、调度和执行各种类型的事件。这些模式的设计提供了可重用和可配置的解决方案和应用程序组件。

二者的主要不同点在于I/O事件的处理，Reactor使用的是非阻塞同步I/O，而Proactor使用的是AIO。不过，目前AIO在Linux系统中还不完善，不是真正意义上的系统级别异步IO，而是在用户空间模拟出来的。（但windows缺有真正的系统级别AIO）。由于目前网络服务器基本都是linux，所以可以认为其上的基于事件的高性能网络方案都是Reactor模式。

Linux下确实已经有一些库(libaio)实现了异步io的功能，但大多不支持socket，主要是针对文件IO，基本上大家认为非阻塞同步IO就已经可以了。

## 实现： Reactor模拟Proactor模式

[作者解释](https://mp.weixin.qq.com/s/PB8vMwi8sB4Jw3WzAKpWOQ)

这里直接就是一个线程池解决

包含的变量有
```c++
int m_thread_number;        //线程池中的线程数
int m_max_requests;         //请求队列中允许的最大请求数
pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
std::list<T *> m_workqueue; //请求队列
locker m_queuelocker;       //保护请求队列的互斥锁
sem m_queuestat;            //是否有任务需要处理
bool m_stop;                //是否结束线程
connection_pool *m_connPool;  //数据库
```

包含的函数有
```c++
threadpool(connection_pool *connPool, int thread_number = 8, int max_request = 10000);
~threadpool();
bool append(T *request);

// 私有
static void *worker(void *arg);
void run();
```

这里的实现思路是：

启动固定数量的线程（每个线程其实都是一个线程池对象，只不过detach出去了）
`worker()`方法就是用来给子线程启动用的
`run()`方法是子线程实际执行的任务，就是循环从请求队列中取`T *request`并执行process方法。

启动线程用到`pthread_create()`方法会把`worker`这个静态函数传入进去，同时把指向自己的`this`指针传入。
`worker()`函数中会把这个this指针强转回`threadpool`对象指针，于是子线程实际上持有了主进程**线程池对象**的指针。
后面它自己运行run()方法，实际上共享的是主线程的请求队列。

> 为什么需要通过worker(void *arg)转换成`threadpool`然后调用`run`？线程不是共享了进程的内存吗？

线程虽然共享主进程的内存空间，但C++成员函数的调用机制决定了必须通过对象实例来调用非静态成员函数。这是因为：

1. 非静态成员函数隐含包含一个`this`指针参数，指向调用该函数的对象实例
2. 没有对象实例，就无法直接调用非静态成员函数

在线程环境中：
- `pthread_create`只接受符合`void *(*)(void *)`签名的函数
- 它不能直接接受成员函数，因为成员函数的实际签名包含`this`指针
- 即使共享内存，线程仍需知道"哪个具体对象实例"的`run()`方法应被调用

因此必须通过以下步骤：
1. 将`this`指针作为参数传递给静态`worker`函数
2. `worker`内部将参数转换回原始的`threadpool*`类型
3. 然后通过`pool->run()`调用特定实例的方法

这种做法本质上是解决C++成员函数调用语义与pthread要求之间的不匹配问题。


