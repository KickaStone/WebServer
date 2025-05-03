#if !defined(_CONNECTION_POLL_)
#define _CONNECTION_POLL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool
{
public:
    MYSQL *GetConnection();   // 获取数据库链接
    bool ReleaseConnection(MYSQL *con); // 释放链接
    int GetFreeConn();        // 获取链接
    void DestoryPool();       // 销毁所有链接

    // 单例模式
    static connection_pool *GetInstance();
    void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log);

private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;  // 最大连接数
    int m_CurConn;  // 当前已使用连接数
    int m_FreeConn; // 当前空闲连接数
    locker lock;
    list<MYSQL *> connList; // 连接池
    sem reserve;

public:
    string m_url;          // 主机地址
    string m_port;         // 数据库端口
    string m_User;         // 数据库用户
    string m_Password;     // 数据库密码
    string m_DatabaseName; // 使用数据库名
    int m_close_log;       // 日志开关
};

class connectionRAII
{
private:
    MYSQL *conRAII;
    connection_pool *poolRAII;

public:
    connectionRAII(MYSQL **SQL, connection_pool *connPool);
    ~connectionRAII();
};

#endif // _CONNECTION_POLL_
