// src/Acceptor.h
#pragma once
#include <functional>
#include <memory>

class EventLoop;
class Channel;

class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int sockfd)>;

    Acceptor(EventLoop* loop, int port);
    ~Acceptor();

    void setNewConnectionCallback(NewConnectionCallback cb) {
        newConnCallback_ = std::move(cb);
    }

    void listen();
    bool listening() const { return listening_; }

private:
    int  createNonblockingSocket();
    void bindAndListen(int port);
    void handleRead();   // 有新连接时调用 accept

    EventLoop* loop_;
    int        acceptFd_;   // 监听 socket
    std::unique_ptr<Channel> acceptChannel_;
    NewConnectionCallback newConnCallback_;
    bool listening_;
};