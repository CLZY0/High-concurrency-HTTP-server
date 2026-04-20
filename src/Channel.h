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

private:
    void handleEventWithGuard();
    void update();   // 通知 EventLoop 更新 epoll 中的注册

    static const int kNoneEvent  = 0;
    static const int kReadEvent  = EPOLLIN | EPOLLPRI;   // 读事件
    static const int kWriteEvent = EPOLLOUT;             // 写事件

    EventLoop* loop_;   // 所属 EventLoop
    int fd_;            // 对应的文件描述符
    int events_;        // 当前关注的事件（即 epoll_ctl 注册的事件）
    int revents_;       // epoll_wait 返回的就绪事件
    int index_;         // 在 Epoller 中的状态(-1:未添加, 1:已添加, 2:已删除)

    // 用 weak_ptr 防止 handleEvent 执行期间对象被销毁
    std::weak_ptr<void>   tie_;
    bool                  tied_;

    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};