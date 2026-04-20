// src/Timer.h
#pragma once
#include <functional>
#include <chrono>
#include <vector>
#include <unordered_map>

using TimerCallback = std::function<void()>;
using TimeStamp     = std::chrono::steady_clock::time_point;
using TimerID       = int;

struct TimerNode {
    TimeStamp   expire;   // 到期时间
    TimerID     id;       // 唯一 ID
    TimerCallback cb;     // 到期回调
    bool        deleted;  // 懒删除标记

    // 最小堆按到期时间排序
    bool operator>(const TimerNode& rhs) const { return expire > rhs.expire; }
};

class TimerManager {
public:
    TimerManager();

    // 添加定时器，timeoutMs 毫秒后触发 cb
    TimerID addTimer(int timeoutMs, TimerCallback cb);

    // 取消定时器（懒删除：标记 deleted，在 tick() 时清理）
    void    cancelTimer(TimerID id);

    // 处理所有到期的定时器，返回下一个定时器的剩余时间（毫秒）
    // 返回 -1 表示没有定时器
    int     tick();

private:
    // 最小堆：堆顶是最快到期的定时器
    std::vector<TimerNode> heap_;
    std::unordered_map<TimerID, bool> deleted_;  // id → 是否已删除
    TimerID nextId_;

    void siftUp(int i);
    void siftDown(int i);
    void del(int i);   // 删除堆中第 i 个元素
};