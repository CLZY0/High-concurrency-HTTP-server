# TinyHTTPServer v1.0

基于 C++17 实现的轻量级高并发 HTTP/1.1 服务器，核心采用 Reactor 事件驱动模型，结合 epoll 边缘触发（ET）与线程池任务执行机制，面向 Linux 场景提供稳定的静态文件服务能力。

项目目标是以尽量简洁的代码结构，完整实现一条从连接管理、协议解析、响应生成到连接回收的高并发服务链路，适合作为网络编程与服务端工程化的学习和实践项目。

## 版本信息

- 当前版本：v1.0
- 支持协议：HTTP/1.1
- 运行平台：Linux
- 编译方式：CMake

## 核心特性

- Reactor 模型驱动：基于事件循环统一处理连接 I/O 与回调分发。
- epoll ET 模式：监听与连接均使用非阻塞方式，减少重复事件通知开销。
- 跨线程任务投递：通过 eventfd 唤醒机制支持线程安全任务入队与主循环唤醒。
- 高效读写缓冲：使用 readv + 双 iovec 分散读，降低系统调用与数据搬运成本。
- HTTP/1.1 长连接：支持 keep-alive，连接可复用处理多个请求。
- 静态文件服务：支持资源目录映射与常见 MIME 类型返回。
- 最小堆定时器：提供空闲连接回收所需的超时定时器管理模块。
- 工程结构清晰：核心模块职责明确，便于扩展与性能调优。

## 架构设计

### 1. 总体流程

1. 主线程创建监听 socket，Acceptor 在可读事件上循环 accept 新连接。
2. 新连接封装为 HttpConn，注册读写/关闭/异常回调并加入事件循环。
3. 连接在可读事件触发时读取请求数据，进行 HTTP 解析。
4. 根据请求方法与路径构造 HttpResponse，序列化写入发送缓冲区。
5. 可写事件触发后分批发送响应数据，按 keep-alive 策略决定连接复用或关闭。
6. 连接关闭时执行资源清理，并同步取消对应定时器。

### 2. Reactor + ET 关键点

- 监听 fd 与连接 fd 均为 non-blocking。
- Acceptor 在一次读事件中循环 accept，直到返回 EAGAIN/EWOULDBLOCK。
- 连接读写基于边缘触发语义，配合缓冲区管理完成高吞吐 I/O。
- EventLoop 通过 epoll_wait 获取活跃事件，交由 Channel 分发到具体业务回调。

### 3. 跨线程任务投递

EventLoop 内部维护 pendingFunctors 任务队列，并使用 eventfd 作为唤醒句柄：

- 其他线程调用 queueInLoop/runInLoop 投递任务。
- 任务入队后向 eventfd 写入 8 字节，立即唤醒阻塞中的 epoll_wait。
- 主循环在 doPendingFunctors 中批量执行任务，降低锁竞争开销。

这一机制保证了任务投递的线程安全与响应时效，是 Reactor 与线程池协同的基础设施。

## 模块说明

- src/EventLoop.*：事件循环、任务队列、eventfd 唤醒。
- src/Epoller.*：epoll 封装，维护 Channel 注册与活跃事件提取。
- src/Channel.*：fd 事件关注与回调分发层。
- src/Acceptor.*：监听 socket 管理与新连接接入。
- src/HttpConn.*：连接生命周期、读写处理、请求到响应的主逻辑。
- src/HttpRequest.*：HTTP 请求行、头部与消息体解析。
- src/HttpResponse.*：状态码、响应头、响应体组装与序列化。
- src/Buffer.*：读写缓冲抽象，支持 readv 分散读。
- src/Timer.*：最小堆定时器管理器。
- src/ThreadPool.h：线程池任务队列与工作线程管理。
- src/Server.*：组件装配、连接映射、定时器绑定与服务启动。

## 性能相关实现细节

### 1. 分散读减少系统调用

Buffer::readFd 使用 readv 将数据一次读入“主缓冲区 + 栈上临时缓冲区”，在高并发短报文场景可有效降低反复扩容与额外系统调用开销。

### 2. 减少无效唤醒与锁持有时间

EventLoop 在执行待处理任务时采用 swap 方式快速转移任务队列，缩短临界区时间，提升多线程提交任务时的并发效率。

### 3. 长连接复用

HttpRequest 按 HTTP/1.1 默认 keep-alive 语义处理连接复用。响应发送完成后，连接可继续进入下一轮请求解析，减少频繁建连成本。

### 4. 定时器驱动连接治理

TimerManager 采用最小堆维护超时节点，支持 O(logN) 插入与删除，用于连接空闲超时治理与资源回收。

## 压测说明（wrk）

### 压测场景

- 工具：wrk
- 目标：静态文件请求
- 并发连接：1000
- 线程数：2
- 协议：HTTP/1.1 keep-alive

### 示例命令

```bash
wrk -t2 -c1000 -d30s http://localhost:8080/index.html
```

### v1.0 结果概览

- QPS：10000+
- 并发 1000 连接下服务可稳定运行
- 连接与请求处理过程无明显异常抖动

该结果验证了项目在 Reactor + ET + 缓冲优化组合下具备可观的并发处理能力。

## 构建与运行

### 1. 构建

```bash
mkdir -p build
cd build
cmake ..
make -j
```

### 2. 启动

```bash
./server [port] [threads] [resource_dir]
```

示例：

```bash
./server 8080 4 ../resources
```

参数说明：

- port：监听端口，默认 8080
- threads：线程数，默认 4
- resource_dir：静态资源目录，默认 ../resources

## 目录结构

```text
TinyHTTPServer/
├── CMakeLists.txt
├── resources/
│   └── index.html
├── src/
│   ├── Acceptor.*
│   ├── Buffer.*
│   ├── Channel.*
│   ├── Epoller.*
│   ├── EventLoop.*
│   ├── HttpConn.*
│   ├── HttpRequest.*
│   ├── HttpResponse.*
│   ├── Server.*
│   ├── ThreadPool.h
│   ├── Timer.*
│   └── main.cpp
└── build/
```

## 适用场景

- Linux 网络编程学习与实践
- Reactor/epoll 机制源码级理解
- 高并发 HTTP 服务的最小可运行实现参考
- 后续扩展为业务网关、反向代理或动态处理框架的基础骨架

## 后续演进方向

- 完善多 Reactor（主从线程 EventLoop）模型，增强多核扩展能力。
- 补充零拷贝文件发送（sendfile/mmap）能力。
- 增加 HTTP 解析鲁棒性与更完整的错误码语义。
- 增加单元测试与自动化压测脚本。
- 提供 Docker 一键构建与部署支持。

## 许可证

本项目采用 MIT License，详见 LICENSE 文件。
