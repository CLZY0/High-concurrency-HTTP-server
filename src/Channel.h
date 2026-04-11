// src/Channel.h
#pragma once
#include <functional>
#include <memory>

class EventLoop;   // 前向声明，避免循环依赖

class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    // ===== 设置回调 =====
    void setReadCallback(EventCallback cb)  { readCallback_  = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // ===== 查询状态 =====
    int fd()     const { return fd_; }
    int events() const { return events_; }  // 当前关注的事件
    bool isNoneEvent()  const { return events_ == kNoneEvent; }
    bool isReading()    const { return events_ & kReadEvent; }
    bool isWriting()    const { return events_ & kWriteEvent; }

    // ===== 修改关注的事件（会通知 EventLoop 更新 epoll）=====
    void enableReading()  { events_ |=  kReadEvent;  update(); }
    void disableReading() { events_ &= ~kReadEvent;  update(); }
    void enableWriting()  { events_ |=  kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll()     { events_  =  kNoneEvent;  update(); }

    // epoll 返回的就绪事件
    void setRevents(int revt) { revents_ = revt; }

    // EventLoop 使用：判断是否在 epoll 中注册过
    int  index() const        { return index_; }
    void setIndex(int idx)    { index_ = idx; }

    // 事件分发：根据 revents_ 调用对应回调
    void handleEvent();

    // 防止 handleEvent 期间 Channel 被析构（配合 shared_ptr）
    void tie(const std::shared_ptr<void>& obj);

    EventLoop* ownerLoop() { return loop_; }
    void remove();   // 从 EventLoop 中移除自己




};