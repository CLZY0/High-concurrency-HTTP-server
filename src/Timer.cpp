// src/Timer.cpp
#include "Timer.h"
#include <algorithm>
#include <cassert>

TimerManager::TimerManager() : nextId_(0) {}

TimerID TimerManager::addTimer(int timeoutMs, TimerCallback cb) {
    TimerNode node;
    node.expire  = std::chrono::steady_clock::now()
                   + std::chrono::milliseconds(timeoutMs);
    node.id      = nextId_++;
    node.cb      = std::move(cb);
    node.deleted = false;

    heap_.push_back(node);
    siftUp(static_cast<int>(heap_.size()) - 1);  // 上浮维护堆性质

    return node.id;
}

void TimerManager::cancelTimer(TimerID id) {
    deleted_[id] = true;   // 懒删除，不立即从堆中移除
}

// 处理所有到期的定时器
int TimerManager::tick() {
    if (heap_.empty()) return -1;

    auto now = std::chrono::steady_clock::now();

    while (!heap_.empty()) {
        TimerNode& top = heap_.front();

        // 最小堆堆顶是最早到期的，如果堆顶还没到期，退出
        if (top.expire > now) break;

        if (!top.deleted) {
            top.cb();   // 执行到期回调
        }

        // 删除堆顶（将堆顶与末尾交换，然后下沉）
        del(0);
    }

    if (heap_.empty()) return -1;

    // 返回下一个到期的剩余毫秒数（作为 epoll_wait 的超时时间）
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                    heap_.front().expire - now).count();
    return static_cast<int>(std::max(0LL, diff));
}

// 上浮（小的上浮，因为是最小堆）
void TimerManager::siftUp(int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap_[parent].expire <= heap_[i].expire) break;
        std::swap(heap_[parent], heap_[i]);
        i = parent;
    }
}

// 下沉
void TimerManager::siftDown(int i) {
    int n = static_cast<int>(heap_.size());
    for (;;) {
        int smallest = i;
        int l = 2 * i + 1, r = 2 * i + 2;
        if (l < n && heap_[l].expire < heap_[smallest].expire) smallest = l;
        if (r < n && heap_[r].expire < heap_[smallest].expire) smallest = r;
        if (smallest == i) break;
        std::swap(heap_[smallest], heap_[i]);
        i = smallest;
    }
}

// 删除堆中第 i 个元素
void TimerManager::del(int i) {
    int n = static_cast<int>(heap_.size());
    if (i == n - 1) {
        heap_.pop_back();
        return;
    }
    // 用最后一个元素替换第 i 个，再维护堆性质
    heap_[i] = heap_.back();
    heap_.pop_back();
    siftDown(i);
    siftUp(i);
}