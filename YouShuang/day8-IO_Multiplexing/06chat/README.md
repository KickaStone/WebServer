# 聊天室程序

## 服务器程序设计


```c++
struct client_data{
    sockaddr_in address;        // 连接地址
    char* write_buf;            // 写入缓冲区
    char buf[BUFFER_SIZE];      // 读取缓冲区
};
```

## 使用poll进行多个socket的监听


1. 监听listenfd, 如果有新连接接入，先建立连接
2. 检测用户数量
    1. 超过最大数量，关闭连接并返回信息
    2. 接收连接，并添加用户到管理数组里；要建立对应的`client_data`, 然后把这个socket放进fds中，由poll监听。


处理用户输入的逻辑
实现聊天室，应该是群聊逻辑。

```c++
int connfd = fds[i].fd;
memset(users[connfd].buf, '\0', BUFFER_SIZE);
ret = recv(connfd, users[connfd].buf, BUFFER_SIZE-1, 0);
printf("get %d bytes of client data %s from %d\n", ret, users[connfd].buf, connfd);
if(ret < 0){
    // 如果读操作发生错误，则关闭连接
    if(errno != EAGAIN){
        close(connfd);
        users[fds[i].fd] = users[fds[user_counter].fd];
        fds[i] = fds[user_counter];
        i--;
        user_counter--;
    }
}else if(ret == 0){}
else{
    // 接收到客户数据，通知其他socket连接准备写数据
    for(int j = 1; j <= user_counter; ++j){
        if(fds[j].fd == connfd){
            continue;
        }
        fds[j].events |= ~POLLIN; // 取消读就绪事件
        fds[j].events |= POLLOUT; // 设置写就绪事件
        users[fds[j].fd].write_buf = users[connfd].buf;
    }
}
```

先接收用户的输入，放到用户对应的client_data的buf中；
然后使用一个for循环，把消息发送到所有socket，除了自己的write_buf上。

然后通过下一个循环，把消息发送回去。
```c++
int connfd = fds[i].fd;
if(!users[connfd].write_buf){
    continue;
}
ret = send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
// 重新注册fds[i]上的可读事件
users[connfd].write_buf = NULL;
fds[i].events |= ~POLLOUT;  // 取消写就绪事件
fds[i].events |= POLLIN;    // 设置读就绪事件
```