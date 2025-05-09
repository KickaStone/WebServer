#include "webserver.h"

WebServer::WebServer()
{
    users = new http_conn[MAX_FD];

    // 获取当前工作目录
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);
    
    users_timer = new client_data[MAX_FD];
}


WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void WebServer::init(int port, string user, string passWord, string databaseName, int log_write, int opt_linger, int trig_mode, int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_password = passWord;
    m_databaseName = databaseName;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trig_mode;
    m_thread_num = thread_num;
    m_close_log = close_log;
    m_actormodel = actor_model;
    m_sql_num = sql_num;
}

void WebServer::trig_mode()
{
    // LT + LT 
    if(m_TRIGMode == 0)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    // LT + ET  
    else if(m_TRIGMode == 1)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    // ET + LT
    else if(m_TRIGMode == 2)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    // ET + ET
    else if(m_TRIGMode == 3)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

void WebServer::log_write()
{
    if(0 == m_close_log)
    {
        if(m_log_write == 1)
        {
            Log::get_instance()->init("ServerLog", m_close_log, 2000, 800000, 800);
        }
        else
        {
            Log::get_instance()->init("ServerLog", m_close_log, 2000, 800000, 0);
        }
    }
}

void WebServer::sql_pool()
{
    // 初始化数据库连接池
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("172.18.0.2", m_user, m_password, m_databaseName, 3306, m_sql_num, m_close_log);

    // 初始化数据库读取表
    users->initmysql_result(m_connPool);
}

void WebServer::thread_pool()
{
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

void WebServer::eventListen(){

    // 创建socket
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 优雅关闭连接
    if(0 == m_OPT_LINGER){
        struct linger tmp = {0, 0};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }else if(1 == m_OPT_LINGER){
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);
    
    utils.init(TIMESLOT);

    // 创建epoll对象，事件数组，添加socket监听
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);


    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    // 创建管道 用于信号统一管理
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd, struct sockaddr_in client_address){
    // 初始化http连接，并将连接加入到epoll中
    users[connfd].init(connfd, client_address, m_root, m_TRIGMode, m_close_log, m_user, m_password, m_databaseName);

    // 初始化client_data数据
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;

    // 创建定时器，设置回调函数和超时时间
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);
}

void WebServer::adjust_timer(util_timer *timer){
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

void WebServer::deal_timer(util_timer *timer, int sockfd){
    timer->cb_func(&users_timer[sockfd]);
    if(timer){
        utils.m_timer_lst.del_timer(timer);
    }

    LOG_INFO("%s", "close fd %d", users_timer[sockfd].sockfd);
}

bool WebServer::dealclientdata(){
    // 接收客户端请求
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if(0 == m_LISTENTrigmode){
        // LT 
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if(connfd < 0){
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if(http_conn::m_user_count >= MAX_FD){
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }else{
        // ET
        while(1){
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if(connfd < 0){
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if(http_conn::m_user_count >= MAX_FD){
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

bool WebServer::dealwithsignal(bool &timeout, bool &close_server){
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if(ret == -1){
        return false;
    }else if(ret == 0){
        return false;
    }
    else{
        for(int i = 0; i < ret; ++i){
            switch(signals[i]){
                case SIGALRM:
                {
                    timeout = true;
                    break;
                }
                case SIGTERM:
                {
                    close_server = true;
                    break;
                }
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd){
    util_timer *timer = users_timer[sockfd].timer;

    // reactor
    if(1 == m_actormodel){
        if(timer){
            adjust_timer(timer);
        }
        
        // 将事件加入到epoll中
        m_pool->append(users + sockfd, 0);

        while(true){
            if(1 == users[sockfd].improv){
                // 处理事件
                if(1 == users[sockfd].timer_flag){
                    // 处理定时器
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    // proactor
    else{
        if(users[sockfd].read_once()){
            LOG_INFO("%s", "deal with the client(%d) request", sockfd);

            // 将请求加入到线程池中 
            m_pool->append(users + sockfd, 0);
            if(timer){
                adjust_timer(timer);
            }
        }
        else{
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd){
    util_timer *timer = users_timer[sockfd].timer;
    // reactor
    if(1 == m_actormodel){
        if(timer){
            adjust_timer(timer);
        }

        m_pool->append(users + sockfd, 1);

        while(true){
            if(1 == users[sockfd].improv){
                if(1 == users[sockfd].timer_flag){
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }else{
        // proactor
        if(users[sockfd].write()){
            LOG_INFO("%s", "send data to the client(%d)", sockfd);

            if(timer){
                adjust_timer(timer);
            }
        }else{
            deal_timer(timer, sockfd);
        }
    }
}


void WebServer::eventLoop(){
    bool timeout = false;
    bool stop_server = false;

    while(!stop_server){
        // epoll wait
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR){
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        // 处理事件
        for(int i = 0; i < number; i++){
            int sockfd = events[i].data.fd;
            if(sockfd == m_listenfd){
                bool flag = dealclientdata();
                if(!flag)
                    continue;
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                // 服务器端关闭连接，移除对应的定时器   
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)){
                bool flag = dealwithsignal(timeout, stop_server);
                if(!flag)
                    LOG_ERROR("%s", "dealclientdata failure");
                continue;
            }
            // 处理客户发送数据
            else if(events[i].events & EPOLLIN){
                dealwithread(sockfd);
            }
            // 处理客户写入数据
            else if(events[i].events & EPOLLOUT){
                dealwithwrite(sockfd);
            }
        }

        // 处理定时事件
        if(timeout){
            utils.timer_handler();
            LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
}