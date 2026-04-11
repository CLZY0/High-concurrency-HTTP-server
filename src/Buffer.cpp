#include "Buffer.h"
#include <errno.h>

// 从 fd 读数据
// 巧妙设计：用 iovec 同时指向 buffer 的可写区 + 栈上 64KB
// 这样一次 readv() 就能把内核缓冲区数据全部取出
ssize_t Buffer::readFd(int fd, int* savedErrno) {
    char   extrabuf[65536];      // 栈上额外缓冲区，不占堆内存
    struct iovec vec[2];

    const size_t writable = writableBytes();

    // vec[0]: buffer 自身的可写区域
    vec[0].iov_base = beginWrite();
    vec[0].iov_len  = writable;

    // vec[1]: 栈上临时缓冲区
    vec[1].iov_base = extrabuf;
    vec[1].iov_len  = sizeof(extrabuf);

    // 如果 buffer 可写空间已经足够大，就不需要 extrabuf
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n  = readv(fd, vec, iovcnt);

    if (n < 0) {
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        // 数据全部写入 buffer 主体
        writeIdx_ += n;
    } else {
        // buffer 主体写满了，多余的在 extrabuf 里，append 进来
        writeIdx_ = buf_.size();
        append(extrabuf, n - writable);
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int* savedErrno) {
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) {
        *savedErrno = errno;
    } else {
        retrieve(static_cast<size_t>(n));
    }
    return n;
}