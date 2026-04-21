// src/Server.h
#pragma once
#include "EventLoop.h"
#include "Acceptor.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

class HttpConn;

class Server
{
public:
    Server(int port, int threadNum, const std::string &resourceDir);
    ~Server();

    void start();

private:
    struct WorkerContext
    {
        EventLoop *loop{nullptr};
        std::unordered_map<int, std::shared_ptr<HttpConn>> connections;
    };

    void startWorkerLoops();
    WorkerContext *pickWorker();
    void onNewConnection(int connFd);                          // 新连接到来
    void onConnectionClose(WorkerContext *worker, int connFd); // 连接关闭

    int port_;
    int ioThreadNum_;
    std::string resourceDir_;

    EventLoop loop_;    // 主 EventLoop（Main Reactor）
    Acceptor acceptor_; // 监听新连接

    std::vector<std::unique_ptr<WorkerContext>> workers_;
    std::vector<std::thread> ioThreads_;
    size_t nextWorker_;

    std::mutex workerInitMutex_;
    std::condition_variable workerInitCv_;
    int initializedWorkers_;
};