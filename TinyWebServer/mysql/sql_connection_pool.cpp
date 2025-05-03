#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

// 当有请求时，从数据库连接池中返回一个可用链接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
    MYSQL *con = NULL;

    if (0 == connList.size())
        return NULL;

    reserve.wait();

    lock.lock();

    con = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    lock.unlock();
    return con;
}

// 释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
    if (NULL == con)
        return false;

    lock.lock();

    connList.push_back(con);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();

    lock.unlock();

    reserve.post();
    return true;
}

int connection_pool::GetFreeConn()
{
    return this->m_FreeConn;
}

void connection_pool::DestoryPool()
{
    lock.lock();
    
    // 检查是否所有连接都已归还
    if (m_CurConn != 0) {
        LOG_ERROR("Connection pool destroyed with %d connections still in use", m_CurConn);
    }
    
    if (m_FreeConn != m_MaxConn) {
        LOG_ERROR("Connection pool destroyed with incorrect number of free connections: %d (expected %d)", 
                 m_FreeConn, m_MaxConn);
    }
    
    if (connList.size() > 0)
    {
        list<MYSQL *>::iterator it;
        for (it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL *con = *it;
            mysql_close(con);
        }

        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }
    lock.unlock();
}

connection_pool *connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

void connection_pool::init(string url, string User, string Password, string DataBaseName, int Port, int MaxConn, int close_log)
{
    m_url = url;
    m_port = Port;
    m_User = User;
    m_Password = Password;
    m_DatabaseName = DataBaseName;
    m_close_log = close_log;

    // 先创建MaxConn个数据库链接，存到list中，作为连接池
    for (int i = 0; i < MaxConn; i++)
    {
        MYSQL *con = NULL;
        con = mysql_init(con);

        if (con == NULL)
        {
            LOG_ERROR("MYSQL Error");
            exit(1);
        }
        con = mysql_real_connect(con, url.c_str(), User.c_str(), Password.c_str(), DataBaseName.c_str(), Port, NULL, 0);

        if (con == NULL)
        {
            LOG_ERROR("MYSQL Error");
            exit(1);
        }

        connList.push_back(con);
        ++m_FreeConn;
    }

    reserve = sem(m_FreeConn); // 初始化信号量，设置为空闲连接数量
    m_MaxConn = m_FreeConn;
}

connection_pool::connection_pool()
{
    m_CurConn = 0;
    m_FreeConn = 0;
}

connection_pool::~connection_pool()
{
    DestoryPool();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool)
{  
    *SQL = connPool->GetConnection();

    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}
