// src/HttpConn.h
#pragma once
#include "Buffer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <functional>
#include <memory>
#include <string>
#include <atomic>

class EventLoop;
class Channel;

class HttpConn : public std::enable_shared_from_this<HttpConn>
{
public:
    using CloseCallback = std::function<void(int fd)>;

    HttpConn(EventLoop *loop, int fd, const std::string &resourceDir);
    ~HttpConn();

    void start(); // 初始化 Channel，开始监听事件
    void shutdown();

    void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }

    int fd() const { return fd_; }
    bool isET() const { return true; } // 使用 ET 模式

    // 更新最后活跃时间（供定时器使用）
    void updateActiveTime();

    static std::atomic<int> userCount; // 当前连接数（全局统计）

private:
    // Channel 回调
    void handleRead();  // 有数据可读
    void handleWrite(); // 可以写数据
    void handleClose(); // 连接关闭
    void handleError(); // 发生错误

    // 业务逻辑
    bool process(); // 解析请求、构造响应
    void sendResponse(HttpResponse &resp);

    // 读取静态文件
    bool serveStaticFile(const std::string &path, HttpResponse &resp);

    EventLoop *loop_;
    int fd_;
    bool isClose_;

    std::unique_ptr<Channel> channel_;

    Buffer readBuf_;  // 读缓冲区（接收客户端数据）
    Buffer writeBuf_; // 写缓冲区（待发送的响应）

    HttpRequest request_;
    std::string resourceDir_; // 静态文件根目录

    CloseCallback closeCallback_;
};