// src/HttpResponse.cpp
#include "HttpResponse.h"
#include "Buffer.h"
#include <sstream>

std::string HttpResponse::statusMessage() const {
    switch (statusCode_) {
        case StatusCode::k200_OK:         return "OK";
        case StatusCode::k400_BadRequest: return "Bad Request";
        case StatusCode::k403_Forbidden:  return "Forbidden";
        case StatusCode::k404_NotFound:   return "Not Found";
        case StatusCode::k500_Internal:   return "Internal Server Error";
        default:                           return "Unknown";
    }
}

// 序列化响应到 Buffer
// 格式：状态行 + 头部 + 空行 + 体
void HttpResponse::appendToBuffer(Buffer* buf) const {
    std::string resp;

    // 状态行：HTTP/1.1 200 OK\r\n
    resp += "HTTP/1.1 " + std::to_string(static_cast<int>(statusCode_))
            + " " + statusMessage() + "\r\n";

    // 头部
    resp += "Connection: " + std::string(keepAlive_ ? "keep-alive" : "close") + "\r\n";
    resp += "Content-Length: " + std::to_string(body_.size()) + "\r\n";

    for (auto& [k, v] : headers_) {
        resp += k + ": " + v + "\r\n";
    }

    // 空行（头部结束）
    resp += "\r\n";

    // 响应体
    resp += body_;

    buf->append(resp);
}

// 根据文件扩展名返回 MIME 类型
std::string HttpResponse::getMimeType(const std::string& path) {
    static const std::unordered_map<std::string, std::string> mimes = {
        {".html", "text/html; charset=utf-8"},
        {".htm",  "text/html; charset=utf-8"},
        {".css",  "text/css"},
        {".js",   "application/javascript"},
        {".json", "application/json"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif",  "image/gif"},
        {".ico",  "image/x-icon"},
        {".svg",  "image/svg+xml"},
        {".txt",  "text/plain"},
        {".pdf",  "application/pdf"},
    };

    size_t dot = path.rfind('.');
    if (dot != std::string::npos) {
        auto it = mimes.find(path.substr(dot));
        if (it != mimes.end()) return it->second;
    }
    return "application/octet-stream";
}

HttpResponse HttpResponse::makeErrorResponse(StatusCode code,
                                              const std::string& msg,
                                              bool keepAlive) {
    HttpResponse resp(keepAlive);
    resp.setStatusCode(code);
    resp.setContentType("text/html; charset=utf-8");
    std::string body = "<html><body><h1>"
                       + std::to_string(static_cast<int>(code))
                       + " " + msg + "</h1></body></html>";
    resp.setBody(body);
    return resp;
}