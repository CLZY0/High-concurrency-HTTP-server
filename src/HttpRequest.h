// src/HttpRequest.h
#pragma once
#include <string>
#include <unordered_map>

class HttpRequest {
public:
    enum class ParseState {
        kRequestLine,  // 正在解析请求行
        kHeaders,      // 正在解析头部
        kBody,         // 正在解析请求体
        kComplete,     // 解析完成
        kError         // 解析出错
    };

    HttpRequest() : state_(ParseState::kRequestLine) {}

    void reset() {
        state_  = ParseState::kRequestLine;
        method_.clear();
        path_.clear();
        version_.clear();
        headers_.clear();
        body_.clear();
    }

    // 解析缓冲区中的数据
    // 返回 true 表示解析完成（可以发送响应了）
    bool parse(const std::string& rawData);
    bool isComplete() const { return state_ == ParseState::kComplete; }
    bool hasError()   const { return state_ == ParseState::kError; }

    // ===== 访问解析结果 =====
    const std::string& method()  const { return method_; }
    const std::string& path()    const { return path_; }
    const std::string& version() const { return version_; }
    const std::string& body()    const { return body_; }

    std::string getHeader(const std::string& key) const {
        auto it = headers_.find(key);
        return (it != headers_.end()) ? it->second : "";
    }

    bool isKeepAlive() const {
        // HTTP/1.1 默认长连接；HTTP/1.0 默认短连接
        std::string conn = getHeader("Connection");
        if (version_ == "HTTP/1.1") {
            return (conn != "close");
        } else {
            return (conn == "keep-alive" || conn == "Keep-Alive");
        }
    }

private:
    bool parseRequestLine(const std::string& line);
    bool parseHeader(const std::string& line);

    ParseState state_;
    std::string method_;    // GET POST PUT DELETE 等
    std::string path_;      // /index.html
    std::string version_;   // HTTP/1.1
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;

    // 累积未处理的原始数据
    std::string rawBuffer_;
};