// src/Server.cpp
#include "Server.h"
#include "HttpConn.h"
#include <functional>
#include <iostream>
#include <unistd.h>

Server::Server(int port, int threadNum, const std::string &resourceDir)
    : port_(port), ioThreadNum_(threadNum > 0 ? threadNum : 1), resourceDir_(resourceDir), acceptor_(&loop_, port), nextWorker_(0), initializedWorkers_(0)
{
    // 设置新连接回调
    acceptor_.setNewConnectionCallback(
        [this](int fd)
        { onNewConnection(fd); });

    std::cout << "[Server] Port=" << port
              << " IOThreads=" << ioThreadNum_
              << " Resources=" << resourceDir << std::endl;
}

Server::~Server()
{
    loop_.quit();
    for (auto &worker : workers_)
    {
        if (worker && worker->loop)
            worker->loop->quit();
    }
    for (auto &t : ioThreads_)
    {
        if (t.joinable())
            t.join();
    }
}

void Server::start()
{
    startWorkerLoops();
    acceptor_.listen();
    std::cout << "[Server] Listening on port " << port_ << std::endl;
    loop_.loop(); // 启动事件循环（阻塞在这里）
}

void Server::startWorkerLoops()
{
    workers_.reserve(static_cast<size_t>(ioThreadNum_));
    for (int i = 0; i < ioThreadNum_; ++i)
    {
        workers_.emplace_back(new WorkerContext());
        WorkerContext *ctx = workers_.back().get();

        ioThreads_.emplace_back([this, ctx]()
                                {
            EventLoop loop;
            {
                std::lock_guard<std::mutex> lock(workerInitMutex_);
                ctx->loop = &loop;
                ++initializedWorkers_;
            }
            workerInitCv_.notify_one();
            loop.loop();
            ctx->loop = nullptr; });
    }

    std::unique_lock<std::mutex> lock(workerInitMutex_);
    workerInitCv_.wait(lock, [this]
                       { return initializedWorkers_ == ioThreadNum_; });
}

Server::WorkerContext *Server::pickWorker()
{
    if (workers_.empty())
        return nullptr;
    WorkerContext *worker = workers_[nextWorker_].get();
    nextWorker_ = (nextWorker_ + 1) % workers_.size();
    return worker;
}

void Server::onNewConnection(int connFd)
{
    WorkerContext *worker = pickWorker();
    if (!worker || !worker->loop)
    {
        ::close(connFd);
        return;
    }

    EventLoop *ioLoop = worker->loop;
    ioLoop->runInLoop([this, worker, ioLoop, connFd]()
                      {
        auto conn = std::make_shared<HttpConn>(ioLoop, connFd, resourceDir_);
        conn->setCloseCallback([this, worker](int fd) {
            onConnectionClose(worker, fd);
        });
        worker->connections[connFd] = conn;
        conn->start(); });
}

void Server::onConnectionClose(WorkerContext *worker, int connFd)
{
    if (!worker)
        return;
    worker->connections.erase(connFd);
}