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

 
};