#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cassert>
#include <sys/uio.h>   // readv
#include <unistd.h>    // write

class Buffer {
public:
    // 预留8字节（可用于写包头长度等）
    static const size_t kPrepend  = 8;
    // 初始可用大小 1KB
    static const size_t kInitSize = 1024;

    explicit Buffer(size_t initSize = kInitSize)
        : buf_(kPrepend + initSize)
        , readIdx_(kPrepend)
        , writeIdx_(kPrepend)
    {}

    // ===== 查询接口 =====
    size_t readableBytes()    const { return writeIdx_ - readIdx_; }
    size_t writableBytes()    const { return buf_.size() - writeIdx_; }
    size_t prependableBytes() const { return readIdx_; }

    // 返回可读数据起始指针（只读）
    const char* peek() const { return begin() + readIdx_; }

    // 在可读区搜索单个字符
    const char* findChar(char c) const {
        const char* p = std::find(peek(), beginWrite(), c);
        return (p == beginWrite()) ? nullptr : p;
    }

    // ===== 读取（消费）接口 =====
    // 消费 len 字节，移动 readIdx_ 向右
    void retrieve(size_t len) {
        assert(len <= readableBytes());
        if (len < readableBytes()) readIdx_ += len;
        else retrieveAll();
    }

    void retrieveUntil(const char* end) {
        assert(peek() <= end && end <= beginWrite());
        retrieve(end - peek());
    }

    void retrieveAll() {
        readIdx_  = kPrepend;
        writeIdx_ = kPrepend;
    }

    // 取出所有可读数据为 string，并清空 buffer
    std::string retrieveAllAsString() {
        std::string s(peek(), readableBytes());
        retrieveAll();
        return s;
    }

    std::string retrieveAsString(size_t len) {
        assert(len <= readableBytes());
        std::string s(peek(), len);
        retrieve(len);
        return s;
    }

    // ===== 写入接口 =====
    char*       beginWrite()       { return begin() + writeIdx_; }
    const char* beginWrite() const { return begin() + writeIdx_; }

    void hasWritten(size_t len) {
        assert(len <= writableBytes());
        writeIdx_ += len;
    }

    void append(const char* data, size_t len) {
        ensureWritable(len);
        std::copy(data, data + len, beginWrite());
        writeIdx_ += len;
    }

    void append(const std::string& s) { append(s.data(), s.size()); }

    // ===== IO 接口 =====
    // 从 fd 读数据到 buffer
    // 技巧：用 readv 配合栈上 64KB 临时缓冲区
    // 这样一次系统调用就能读取超过 buffer 剩余容量的数据
    ssize_t readFd(int fd, int* savedErrno);

    // 从 buffer 向 fd 写数据
    ssize_t writeFd(int fd, int* savedErrno);

private:
    char*       begin()       { return buf_.data(); }
    const char* begin() const { return buf_.data(); }

    // 确保有足够的可写空间
    void ensureWritable(size_t len) {
        if (writableBytes() >= len) return;

        // 如果 "前面的预留空间 + 后面的可写空间" 足够，就把数据往前挪
        if (writableBytes() + prependableBytes() >= len + kPrepend) {
            size_t readable = readableBytes();
            // 把 [readIdx_, writeIdx_) 的数据搬到 [kPrepend, ...)
            std::copy(begin() + readIdx_, begin() + writeIdx_,
                      begin() + kPrepend);
            readIdx_  = kPrepend;
            writeIdx_ = kPrepend + readable;
        } else {
            // 空间真的不够，扩容
            buf_.resize(writeIdx_ + len);
        }
    }

    std::vector<char> buf_;
    size_t readIdx_;   // 可读数据起始位置
    size_t writeIdx_;  // 可写起始位置（同时也是可读数据末尾）
};