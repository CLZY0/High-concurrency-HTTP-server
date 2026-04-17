// src/Server.h
#pragma once
#include "EventLoop.h"
#include "Acceptor.h"
#include "ThreadPool.h"
#include "Timer.h"
#include <unordered_map>
#include <memory>
#include <string>

class HttpConn;

class Server {
public:
    Server(int port, int threadNum, const std::string& resourceDir);
    ~Server();

    void start();

private:
    void onNewConnection(int connFd);    // 新连接到来
    void onConnectionClose(int connFd);  // 连接关闭
    void onTimerExpire(int connFd);      // 连接超时

    static const int kConnectionTimeout = 60000;  // 60秒超时

    int         port_;
    std::string resourceDir_;

    EventLoop             loop_;        // 主 EventLoop（Main Reactor）
    Acceptor              acceptor_;    // 监听新连接
    ThreadPool            threadPool_;  // 工作线程池
    TimerManager          timer_;       // 定时器

    // fd → HttpConn 映射
    std::unordered_map<int, std::shared_ptr<HttpConn>> connections_;
    // fd → TimerID 映射（用于超时管理）
    std::unordered_map<int, TimerID> timers_;
};