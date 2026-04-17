// src/EventLoop.cpp
#include "EventLoop.h"
#include "Epoller.h"
#include "Channel.h"
#include <sys/eventfd.h>  // eventfd
#include <unistd.h>
#include <cassert>
#include <sys/syscall.h>  // syscall(SYS_gettid)

// 获取当前线程 ID（比 pthread_self() 更准确）
static pid_t gettid() {
    return static_cast<pid_t>(syscall(SYS_gettid));
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(gettid())
    , epoller_(new Epoller())
    , wakeupFd_(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
    , wakeupChannel_(new Channel(this, wakeupFd_))
{
    if (wakeupFd_ < 0) {
        throw std::runtime_error("eventfd failed");
    }
    // wakeupChannel 监听读事件
    wakeupChannel_->setReadCallback([this]{ handleRead(); });
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
}

// 事件循环主体：不断 poll → 处理事件 → 执行任务
void EventLoop::loop() {
    assert(!looping_);
    looping_ = true;
    quit_    = false;

    while (!quit_) {
        activeChannels_.clear();

        // 1. epoll_wait 等待就绪事件
        epoller_->poll(kPollTimeMs, activeChannels_);

        // 2. 遍历就绪 Channel，调用其事件处理回调
        for (Channel* ch : activeChannels_) {
            ch->handleEvent();
        }

        // 3. 执行其他线程提交的任务
        doPendingFunctors();
    }

    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    // 如果在其他线程调用 quit，需要唤醒 epoll_wait
    if (!isInLoopThread()) {
        wakeup();
    }
}

// 线程安全地执行任务：如果在本线程就直接执行，否则入队并唤醒
void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }
    // 如果不在 EventLoop 线程，或者正在执行 pendingFunctors（意味着
    // loop() 下一轮 poll 会阻塞），则需要 wakeup
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

// 往 eventfd 写 8 字节，唤醒 epoll_wait
void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
    (void)n;
}

// 处理 wakeupFd 的读事件（把数据读走即可，目的是唤醒 epoll_wait）
void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof(one));
    (void)n;
}

// 执行积累的任务
void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    // 将队列 swap 出来，减少锁持有时间
    // （执行 functors 时不持锁，允许其他线程继续投递任务）
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const auto& f : functors) {
        f();
    }

    callingPendingFunctors_ = false;
}

void EventLoop::updateChannel(Channel* channel) { epoller_->updateChannel(channel); }
void EventLoop::removeChannel(Channel* channel) { epoller_->removeChannel(channel); }
bool EventLoop::hasChannel(Channel* channel)    { return epoller_->hasChannel(channel); }
bool EventLoop::isInLoopThread() const          { return threadId_ == gettid(); }