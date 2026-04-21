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
#include <utility>

std::atomic<int> HttpConn::userCount{0};
std::unordered_map<std::string, std::shared_ptr<HttpConn::CachedResponse>> HttpConn::fileCache_;
std::mutex HttpConn::fileCacheMutex_;

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
    for (;;)
    {
        int savedErrno = 0;
        ssize_t n = readBuf_.readFd(fd_, &savedErrno);

        if (n > 0)
            continue;
        if (n == 0)
        {
            handleClose();
            return;
        }
        if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK)
        {
            break;
        }
        handleClose();
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
        channel_->disableWriting();
        return;
    }

    while (writeBuf_.readableBytes() > 0)
    {
        int savedErrno = 0;
        ssize_t n = writeBuf_.writeFd(fd_, &savedErrno);
        if (n > 0)
            continue;
        if (n < 0 && (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK))
        {
            break;
        }
        handleClose();
        return;
    }

    if (writeBuf_.readableBytes() == 0)
    {
        channel_->disableWriting();
        if (!request_.isKeepAlive())
        {
            handleClose();
            return;
        }
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
    bool keepAlive = request_.isKeepAlive();

    if (request_.method() == "GET" || request_.method() == "HEAD")
    {
        if (!serveStaticFile(request_.path(), keepAlive, request_.method() == "HEAD"))
        {
            auto resp = HttpResponse::makeErrorResponse(
                HttpResponse::StatusCode::k404_NotFound, "Not Found", keepAlive);
            sendResponse(resp);
        }
    }
    else if (request_.method() == "POST")
    {
        HttpResponse resp(keepAlive);
        resp.setStatusCode(HttpResponse::StatusCode::k200_OK);
        resp.setContentType("application/json");
        resp.setBody("{\"status\":\"ok\"}");
        sendResponse(resp);
    }
    else
    {
        auto resp = HttpResponse::makeErrorResponse(
            HttpResponse::StatusCode::k400_BadRequest,
            "Method Not Allowed", keepAlive);
        sendResponse(resp);
    }
    return true;
}

bool HttpConn::buildCachedResponse(const std::string &filePath,
                                   std::shared_ptr<CachedResponse> &out)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
        return false;

    std::ostringstream oss;
    oss << file.rdbuf();
    const std::string body = oss.str();

    const std::string contentType = HttpResponse::getMimeType(filePath);
    const std::string baseHeaders =
        "Content-Type: " + contentType + "\r\n"
                                         "Content-Length: " +
        std::to_string(body.size()) + "\r\n";

    out.reset(new CachedResponse());
    out->getKeepAlive = "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\n" + baseHeaders + "\r\n" + body;
    out->getClose = "HTTP/1.1 200 OK\r\nConnection: close\r\n" + baseHeaders + "\r\n" + body;
    out->headKeepAlive = "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\n" + baseHeaders + "\r\n";
    out->headClose = "HTTP/1.1 200 OK\r\nConnection: close\r\n" + baseHeaders + "\r\n";
    return true;
}

// 读取并服务静态文件（命中缓存时零磁盘 IO）
bool HttpConn::serveStaticFile(const std::string &urlPath, bool keepAlive, bool headOnly)
{
    if (urlPath.find("..") != std::string::npos)
    {
        auto resp = HttpResponse::makeErrorResponse(
            HttpResponse::StatusCode::k403_Forbidden, "Forbidden", keepAlive);
        sendResponse(resp);
        return false;
    }

    std::string filePath = resourceDir_ + urlPath;

    std::shared_ptr<CachedResponse> cached;
    {
        std::lock_guard<std::mutex> lock(fileCacheMutex_);
        auto it = fileCache_.find(filePath);
        if (it == fileCache_.end())
        {
            if (!buildCachedResponse(filePath, cached))
                return false;
            fileCache_[filePath] = cached;
        }
        else
        {
            cached = it->second;
        }
    }

    if (!cached)
        return false;

    if (headOnly)
    {
        sendRawResponse(keepAlive ? cached->headKeepAlive : cached->headClose);
    }
    else
    {
        sendRawResponse(keepAlive ? cached->getKeepAlive : cached->getClose);
    }
    return true;
}

// 将响应序列化到 writeBuf_，然后触发写操作
void HttpConn::sendResponse(HttpResponse &resp)
{
    resp.appendToBuffer(&writeBuf_);
    flushWriteBuffer();
}

void HttpConn::sendRawResponse(const std::string &rawResp)
{
    writeBuf_.append(rawResp);
    flushWriteBuffer();
}

void HttpConn::flushWriteBuffer()
{
    while (writeBuf_.readableBytes() > 0)
    {
        int savedErrno = 0;
        ssize_t n = writeBuf_.writeFd(fd_, &savedErrno);
        if (n > 0)
            continue;
        if (n < 0 && (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK))
        {
            channel_->enableWriting();
            return;
        }
        handleClose();
        return;
    }

    channel_->disableWriting();

    if (!request_.isKeepAlive())
    {
        handleClose();
        return;
    }
    request_.reset();
}