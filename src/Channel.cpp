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

// 事件分发入口
void Channel::handleEvent() {
    if (tied_) {
        // 锁住 weak_ptr，确保事件处理期间对象不被销毁
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) handleEventWithGuard();
        // guard 析构时 shared_ptr 引用计数归0，对象才能销毁
    } else {
        handleEventWithGuard();
    }
}

// 根据 revents_ 实际分发到对应回调
void Channel::handleEventWithGuard() {
    // 对端关闭连接（FIN）
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) closeCallback_();
        return;
    }

    // 错误事件
    if (revents_ & EPOLLERR) {
        if (errorCallback_) errorCallback_();
        return;
    }

    // 可读事件（含 EPOLLPRI 带外数据）
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (readCallback_) readCallback_();
    }

    // 可写事件
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) writeCallback_();
    }
}

// 通知 EventLoop 更新 epoll 注册
void Channel::update() {
    loop_->updateChannel(this);
}

void Channel::remove() {
    loop_->removeChannel(this);
}