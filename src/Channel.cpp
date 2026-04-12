// src/Channel.cpp
#include "Channel.h"
#include "EventLoop.h"
#include <sys/epoll.h>
#include <cassert>

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd)
    , events_(0), revents_(0), index_(-1)
    , tied_(false)
{}

Channel::~Channel() {}

// 绑定对象生命周期（HttpConn 调用此方法传入 shared_ptr<this>）
void Channel::tie(const std::shared_ptr<void>& obj) {
    tie_  = obj;
    tied_ = true;
}







    
