// src/EventLoop.h
#pragma once
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>

class Epoller;
class Channel;

class EventLoop {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 启动事件循环（阻塞直到 quit() 被调用）
    void loop();

    // 停止事件循环
    void quit();

    // ===== 任务投递（线程安全）=====
    // 如果在 EventLoop 所属线程调用，立即执行
    // 否则，放入队列并唤醒 EventLoop
    void runInLoop(Functor cb);

    // 将任务放入队列（不管在哪个线程都入队）
    void queueInLoop(Functor cb);

    // ===== Channel 管理（Epoller 的代理）=====
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 判断当前线程是否是 EventLoop 所属线程
    bool isInLoopThread() const;

    // 唤醒阻塞在 epoll_wait 的 EventLoop
    void wakeup();

private:
    void handleRead();      // 处理 wakeupFd_ 上的读事件
    void doPendingFunctors(); // 执行队列中积累的任务

    static const int kPollTimeMs = 10000;  // epoll_wait 超时 10 秒

    std::atomic<bool> looping_;   // 是否正在循环

 