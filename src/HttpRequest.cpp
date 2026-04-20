// src/HttpRequest.cpp
#include "HttpRequest.h"
#include <sstream>
#include <algorithm>

// 解析整个 HTTP 请求
bool HttpRequest::parse(const std::string& rawData) {
    rawBuffer_ += rawData;

    size_t pos = 0;

    while (pos < rawBuffer_.size() && state_ != ParseState::kComplete
           && state_ != ParseState::kError) {

        if (state_ == ParseState::kRequestLine || state_ == ParseState::kHeaders) {
            // 找行结束符 \r\n
            size_t lineEnd = rawBuffer_.find("\r\n", pos);
            if (lineEnd == std::string::npos) break;  // 数据不完整，等待更多数据

            std::string line = rawBuffer_.substr(pos, lineEnd - pos);
            pos = lineEnd + 2;  // 跳过 \r\n

            if (state_ == ParseState::kRequestLine) {
                if (!parseRequestLine(line)) {
                    state_ = ParseState::kError;
                    return false;
                }
                state_ = ParseState::kHeaders;

            } else {  // kHeaders
                if (line.empty()) {
                    // 空行 = 头部结束
                    std::string contentLenStr = getHeader("Content-Length");
                    if (!contentLenStr.empty()) {
                        size_t contentLen = std::stoul(contentLenStr);
                        if (rawBuffer_.size() - pos >= contentLen) {
                            body_ = rawBuffer_.substr(pos, contentLen);
                            pos += contentLen;
                            state_ = ParseState::kComplete;
                        } else {
                            state_ = ParseState::kBody;
                        }
                    } else {
                        state_ = ParseState::kComplete;
                    }
                } else {
                    if (!parseHeader(line)) {
                        state_ = ParseState::kError;
                        return false;
                    }
                }
            }
        } else if (state_ == ParseState::kBody) {
            size_t contentLen = std::stoul(getHeader("Content-Length"));
            if (rawBuffer_.size() - pos >= contentLen) {
                body_ = rawBuffer_.substr(pos, contentLen);
                pos  += contentLen;
                state_ = ParseState::kComplete;
            } else {
                break;  // 等待更多数据
            }
        }
    }

    // 移除已处理的数据
    rawBuffer_ = rawBuffer_.substr(pos);
    return state_ == ParseState::kComplete;
}

// 解析请求行：GET /path HTTP/1.1
bool HttpRequest::parseRequestLine(const std::string& line) {
    std::istringstream iss(line);
    if (!(iss >> method_ >> path_ >> version_)) return false;

    // 处理 URL 编码（简单版）
    // 去掉查询字符串（? 后面的部分）
    size_t qPos = path_.find('?');
    if (qPos != std::string::npos) {
        path_ = path_.substr(0, qPos);
    }

    // 默认路径
    if (path_ == "/") path_ = "/index.html";

    return !method_.empty() && !path_.empty() && !version_.empty();
}

// 解析一个头部行：Key: Value
bool HttpRequest::parseHeader(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return false;

    std::string key   = line.substr(0, colon);
    std::string value = line.substr(colon + 1);

    // 去除首尾空白
    auto trim = [](std::string& s) {
        s.erase(0, s.find_first_not_of(" \t"));
        s.erase(s.find_last_not_of(" \t") + 1);
    };
    trim(key);
    trim(value);

    headers_[key] = value;
    return true;
}