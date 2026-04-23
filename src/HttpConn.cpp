// src/HttpConn.cpp
#include "HttpConn.h"
#include "EventLoop.h"
#include "Channel.h"
#include "HttpResponse.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cerrno>

std::atomic<int> HttpConn::userCount{0};

HttpConn::HttpConn(EventLoop *loop, int fd, const std::string &resourceDir)
    : loop_(loop), fd_(fd), isClose_(false), channel_(new Channel(loop, fd)), resourceDir_(resourceDir)
{
    ++userCount;

    // 配置 Channel 回调
    channel_->setReadCallback([this]
                              { handleRead(); });
    channel_->setWriteCallback([this]
                               { handleWrite(); });
    channel_->setCloseCallback([this]
                               { handleClose(); });
    channel_->setErrorCallback([this]
                               { handleError(); });
}

HttpConn::~HttpConn()
{
    --userCount;
    if (!isClose_)
    {
        ::close(fd_);
    }
}

void HttpConn::start()
{
    // 绑定生命周期，防止 handleEvent 执行期间对象被析构
    channel_->tie(shared_from_this());
    // 开始监听读事件（ET 模式 + 连接重置）
    channel_->enableReading();
}

void HttpConn::shutdown()
{
    if (!isClose_)
    {
        isClose_ = true;
        --userCount;
        channel_->disableAll();
        channel_->remove();
        ::close(fd_);
        if (closeCallback_)
            closeCallback_(fd_);
    }
}

// 可读事件：从 fd 读数据
void HttpConn::handleRead()
{
    int savedErrno = 0;
    ssize_t n = readBuf_.readFd(fd_, &savedErrno);

    if (n <= 0)
    {
        // n == 0: 对端关闭连接（recv 返回0）
        // n < 0 && errno != EAGAIN: 真正的错误
        if (n == 0 || (n < 0 && savedErrno != EAGAIN))
        {
            handleClose();
            return;
        }
        // n < 0 && errno == EAGAIN: 暂时无数据，下次再读
        return;
    }

    // 有数据，尝试处理（可能一次读取了多个请求）
    while (readBuf_.readableBytes() > 0)
    {
        if (!process())
            break;
    }
}

// 可写事件：把 writeBuf_ 中的数据发送出去
void HttpConn::handleWrite()
{
    if (writeBuf_.readableBytes() == 0)
    {
        // 没有数据要写，取消写事件监听
        channel_->disableWriting();
        return;
    }

    int savedErrno = 0;
    ssize_t n = writeBuf_.writeFd(fd_, &savedErrno);

    if (n < 0 && savedErrno != EAGAIN)
    {
        handleClose();
        return;
    }

    if (writeBuf_.readableBytes() == 0)
    {
        // 全部发完
        channel_->disableWriting();
        // 如果是短连接，发完就关闭
        if (!request_.isKeepAlive())
        {
            handleClose();
        }
        // 重置请求，准备接收下一个（长连接复用）
        request_.reset();
    }
}

void HttpConn::handleClose() { shutdown(); }
void HttpConn::handleError() { shutdown(); }

// 解析请求 + 构造响应
bool HttpConn::process()
{
    if (readBuf_.readableBytes() == 0)
        return false;

    // 将 Buffer 中的数据交给 HttpRequest 解析
    std::string rawData = readBuf_.retrieveAllAsString();
    bool complete = request_.parse(rawData);

    if (request_.hasError())
    {
        auto resp = HttpResponse::makeErrorResponse(
            HttpResponse::StatusCode::k400_BadRequest, "Bad Request",
            false);
        sendResponse(resp);
        return false;
    }

    if (!complete)
        return false; // 数据不完整，等待更多数据

    // 构造响应
    HttpResponse resp(request_.isKeepAlive());

    if (request_.method() == "GET" || request_.method() == "HEAD")
    {
        serveStaticFile(request_.path(), resp);
    }
    else if (request_.method() == "POST")
    {
        // 简单处理：返回 200
        resp.setStatusCode(HttpResponse::StatusCode::k200_OK);
        resp.setContentType("application/json");
        resp.setBody("{\"status\":\"ok\"}");
    }
    else
    {
        resp = HttpResponse::makeErrorResponse(
            HttpResponse::StatusCode::k400_BadRequest,
            "Method Not Allowed", request_.isKeepAlive());
    }

    sendResponse(resp);
    return true;
}

// 读取并服务静态文件
bool HttpConn::serveStaticFile(const std::string &urlPath, HttpResponse &resp)
{
    // 安全检查：防止路径穿越（../ 攻击）
    if (urlPath.find("..") != std::string::npos)
    {
        resp = HttpResponse::makeErrorResponse(
            HttpResponse::StatusCode::k403_Forbidden, "Forbidden");
        return false;
    }

    std::string filePath = resourceDir_ + urlPath;

    // 检查文件是否存在
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        // 尝试 index.html
        if (urlPath == "/index.html")
        {
            resp = HttpResponse::makeErrorResponse(
                HttpResponse::StatusCode::k404_NotFound, "Not Found");
            return false;
        }
        resp = HttpResponse::makeErrorResponse(
            HttpResponse::StatusCode::k404_NotFound, "Not Found");
        return false;
    }

    // 读取文件内容
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string content = oss.str();

    resp.setStatusCode(HttpResponse::StatusCode::k200_OK);
    resp.setContentType(HttpResponse::getMimeType(filePath));
    resp.setBody(content);
    return true;
}

// 将响应序列化到 writeBuf_，然后触发写操作
void HttpConn::sendResponse(HttpResponse &resp)
{
    resp.appendToBuffer(&writeBuf_);

    if (writeBuf_.readableBytes() > 0)
    {
        // 先尝试直接 write（减少延迟）
        int savedErrno = 0;
        writeBuf_.writeFd(fd_, &savedErrno);

        // 如果还没写完，注册写事件，等 epoll 通知再写
        if (writeBuf_.readableBytes() > 0)
        {
            channel_->enableWriting();
        }
    }
}