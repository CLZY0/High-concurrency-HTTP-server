// src/Server.cpp
#include "Server.h"
#include "HttpConn.h"
#include <functional>
#include <iostream>

Server::Server(int port, int threadNum, const std::string& resourceDir)
    : port_(port)
    , resourceDir_(resourceDir)
    , acceptor_(&loop_, port)
    , threadPool_(threadNum)
{
    // 设置新连接回调
    acceptor_.setNewConnectionCallback(
        [this](int fd){ onNewConnection(fd); });

    std::cout << "[Server] Port=" << port
              << " Threads=" << threadNum
              << " Resources=" << resourceDir << std::endl;
}

Server::~Server() {}

void Server::start() {
    acceptor_.listen();
    std::cout << "[Server] Listening on port " << port_ << std::endl;
    loop_.loop();   // 启动事件循环（阻塞在这里）
}

void Server::onNewConnection(int connFd) {
    // 创建 HttpConn（在主线程 EventLoop 中）
    auto conn = std::make_shared<HttpConn>(&loop_, connFd, resourceDir_);

    conn->setCloseCallback([this](int fd){ onConnectionClose(fd); });

    connections_[connFd] = conn;

    // 为此连接添加超时定时器
    TimerID tid = timer_.addTimer(kConnectionTimeout,
                                  [this, connFd]{ onTimerExpire(connFd); });
    timers_[connFd] = tid;

    conn->start();

    std::cout << "[Server] New conn fd=" << connFd
              << " total=" << HttpConn::userCount << std::endl;
}

void Server::onConnectionClose(int connFd) {
    // 取消定时器
 


}