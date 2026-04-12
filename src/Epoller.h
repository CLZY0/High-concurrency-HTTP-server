// src/Epoller.h
#pragma once
#include <vector>
#include <unordered_map>
#include <sys/epoll.h>

class Channel;

class Epoller {
public:
    using ChannelList = std::vector<Channel*>;

    Epoller();
    ~Epoller();

    // 调用 epoll_wait，将就绪的 Channel 填入 activeChannels
    // timeoutMs：超时时间（毫秒），-1 表示永久阻塞
    void poll(int timeoutMs, ChannelList& activeChannels);

    // 添加或修改 Channel（对应 epoll_ctl EPOLL_CTL_ADD/MOD）
    void updateChannel(Channel* channel);

    // 删除 Channel（对应 epoll_ctl EPOLL_CTL_DEL）
    void removeChannel(Channel* channel);

    bool hasChannel(Channel* channel) const;

private:
    void epollCtl(int op, Channel* channel);

    static const int kInitEventListSize = 16;  // events 数组初始大小
    // Channel index 的含义
    static const int kNew     = -1;  // 还未加入 epoll
    static const int kAdded   =  1;  // 已在 epoll 中
    static const int kDeleted =  2;  // 曾加入过，现已删除

    int epollFd_;   // epoll 实例 fd

    // fd → Channel* 的映射，方便 poll() 后根据 fd 找到 Channel
    std::unordered_map<int, Channel*> channels_;

    // epoll_wait 返回的就绪事件数组
    std::vector<struct epoll_event> events_;
};