// src/Epoller.cpp
#include "Epoller.h"
#include "Channel.h"
#include <unistd.h>    // close
#include <cstring>     // memset
#include <cassert>
#include <stdexcept>

Epoller::Epoller()
    : epollFd_(epoll_create1(EPOLL_CLOEXEC))  // CLOEXEC：exec后自动关闭
    , events_(kInitEventListSize)
{
    if (epollFd_ < 0) {
        throw std::runtime_error("epoll_create1 failed");
    }
}

Epoller::~Epoller() {
    ::close(epollFd_);
}

// 核心：等待就绪事件，将结果填入 activeChannels
void Epoller::poll(int timeoutMs, ChannelList& activeChannels) {
    // epoll_wait：阻塞等待，直到有事件就绪或超时
    int numEvents = epoll_wait(epollFd_,
                               events_.data(),
                               static_cast<int>(events_.size()),
                               timeoutMs);

    if (numEvents < 0) {
        // EINTR 是被信号打断，正常现象，不报错
        if (errno != EINTR) {
            // 真正的错误
        }
        return;
    }

    // 处理就绪事件
    for (int i = 0; i < numEvents; ++i) {
        // 通过 data.ptr 找到对应的 Channel（比 data.fd 更灵活）
        Channel* ch = static_cast<Channel*>(events_[i].data.ptr);
        ch->setRevents(events_[i].events);  // 告诉 Channel 发生了什么事件
        activeChannels.push_back(ch);       // 加入就绪列表
    }

    // 如果就绪事件数 == 数组大小，可能还有更多事件，扩容
    if (numEvents == static_cast<int>(events_.size())) {
        events_.resize(events_.size() * 2);
    }
}

// 添加或修改 Channel 的 epoll 注册
void Epoller::updateChannel(Channel* channel) {
    int index = channel->index();

    if (index == kNew || index == kDeleted) {
        // 新 Channel，用 EPOLL_CTL_ADD 添加
        int fd = channel->fd();
        channels_[fd] = channel;
        channel->setIndex(kAdded);
        epollCtl(EPOLL_CTL_ADD, channel);
    } else {
        // 已注册的 Channel
        assert(index == kAdded);
        if (channel->isNoneEvent()) {
            // 不再关注任何事件，从 epoll 中删除（但保留 channels_ 映射）
            epollCtl(EPOLL_CTL_DEL, channel);
            channel->setIndex(kDeleted);
        } else {
            // 更新关注的事件
            epollCtl(EPOLL_CTL_MOD, channel);
        }
    }
}

void Epoller::removeChannel(Channel* channel) {
    int fd    = channel->fd();
    int index = channel->index();



