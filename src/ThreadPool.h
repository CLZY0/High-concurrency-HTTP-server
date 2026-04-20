// src/ThreadPool.h
#pragma once
#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <stdexcept>

class ThreadPool {
public:
    using Task = std::function<void()>;

    // threadNum：工作线程数量，默认为 CPU 核数
    explicit ThreadPool(size_t threadNum = std::thread::hardware_concurrency())
        : stop_(false)
    {
        if (threadNum == 0) threadNum = 4;

        for (size_t i = 0; i < threadNum; ++i) {
            // 每个工作线程不断从队列中取任务执行
            workers_.emplace_back([this] {
                for (;;) {
                    Task task;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        // 等待：有任务 或 线程池停止
                        cond_.wait(lock, [this] {
                            return stop_ || !tasks_.empty();
                        });

                        if (stop_ && tasks_.empty()) return;

                        // 取出队头任务
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    // 释放锁后执行任务（最大化并发）
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cond_.notify_all();   // 唤醒所有线程，让它们退出
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
    }

    // 禁止拷贝和赋值
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 投递任务（线程安全）
    void submit(Task task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_) throw std::runtime_error("ThreadPool stopped");
            tasks_.push(std::move(task));
        }
        cond_.notify_one();   // 唤醒一个等待中的工作线程
    }

    size_t size() const { return workers_.size(); }

private:
    std::vector<std::thread>    workers_;   // 工作线程
    std::queue<Task>            tasks_;     // 任务队列
    std::mutex                  mutex_;
    std::condition_variable     cond_;
    std::atomic<bool>           stop_;
};