// src/HttpResponse.h
#pragma once
#include <string>
#include <unordered_map>

class Buffer;

class HttpResponse {
public:
    enum class StatusCode {
        k200_OK          = 200,
        k400_BadRequest  = 400,
        k403_Forbidden   = 403,
        k404_NotFound    = 404,
        k500_Internal    = 500,
    };

    HttpResponse(bool keepAlive = false)
        : statusCode_(StatusCode::k200_OK)
        , keepAlive_(keepAlive)
    {}

    void setStatusCode(StatusCode code) { statusCode_ = code; }
    void setKeepAlive(bool on) { keepAlive_ = on; }
    void setBody(const std::string& body) { body_ = body; }
    void setContentType(const std::string& type) {
        headers_["Content-Type"] = type;
    }
    void addHeader(const std::string& key, const std::string& val) {
        headers_[key] = val;
    }

    // 将响应序列化到 Buffer
    void appendToBuffer(Buffer* buf) const;

    // 根据文件扩展名获取 Content-Type
    static std::string getMimeType(const std::string& path);

    // 生成错误页响应
    static HttpResponse makeErrorResponse(StatusCode code,
                                          const std::string& msg,
                                          bool keepAlive = false);

private:
    std::string statusMessage() const;

    StatusCode  statusCode_;
    bool        keepAlive_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
};