// src/Acceptor.cpp
#include "Acceptor.h"
#include "EventLoop.h"
#include "Channel.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

// 创建非阻塞 TCP socket
static int createNonblocking()
{
    int fd = socket(AF_INET,
                    SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                    IPPROTO_TCP);
    if (fd < 0)
        throw std::runtime_error("socket() failed");
    return fd;
}

Acceptor::Acceptor(EventLoop *loop, int port)
    : loop_(loop), acceptFd_(createNonblocking()), acceptChannel_(new Channel(loop, acceptFd_)), listening_(false)
{
    // 设置 SO_REUSEADDR：服务器重启时不报 "Address already in use"
    int opt = 1;
    setsockopt(acceptFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // 设置 SO_REUSEPORT：允许多个 socket 绑定同一端口（提高并发）
    setsockopt(acceptFd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    // bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // 监听所有网络接口
    addr.sin_port = htons(port);

    if (bind(acceptFd_, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        throw std::runtime_error("bind() failed");
    }

    // 当 acceptFd_ 可读（有新连接），调用 handleRead
    acceptChannel_->setReadCallback([this]
                                    { handleRead(); });
}

Acceptor::~Acceptor()
{
    acceptChannel_->disableAll();
    acceptChannel_->remove();
    ::close(acceptFd_);
}

void Acceptor::listen()
{
    listening_ = true;
    ::listen(acceptFd_, 65535);
    acceptChannel_->enableReading(); // 开始监听可读事件
}

// epoll 通知 acceptFd_ 可读，说明有新连接到来
void Acceptor::handleRead()
{
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    // 循环 accept：ET 模式下必须一次性接受所有待连接
    for (;;)
    {
        int connFd = accept4(acceptFd_,
                             (struct sockaddr *)&clientAddr,
                             &addrLen,
                             SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (connFd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break; // 没有更多连接了
            // 其他错误（如 EMFILE：fd 用尽）
            break;
        }

        int one = 1;
        setsockopt(connFd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        setsockopt(connFd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));

        // 通知 Server 有新连接
        if (newConnCallback_)
        {
            newConnCallback_(connFd);
        }
        else
        {
            ::close(connFd);
        }
    }
}