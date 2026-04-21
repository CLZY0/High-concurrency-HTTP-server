# TinyHTTPServer v1.0

一个基于 C++17 的轻量级高并发 HTTP/1.1 服务器，采用 Reactor 事件驱动模型与 epoll ET（边缘触发）机制，面向 Linux 场景实现高连接数下的稳定请求处理。

项目聚焦于以下目标：

- 高并发连接处理：基于非阻塞 I/O + epoll ET 降低事件分发开销。
- 低系统调用成本：Buffer 使用 readv 分散读，在高吞吐场景减少读路径系统调用次数。
- 长连接复用：支持 HTTP/1.1 keep-alive，降低重复建连开销。
- 静态资源服务：支持基础 MIME 类型识别与静态文件返回。
- 空闲连接治理：实现最小堆定时器模块并接入连接管理流程。

## 1. 版本信息

- 当前版本：v1.0
- 语言标准：C++17
- 构建方式：CMake
- 运行平台：Linux（依赖 epoll、eventfd、非阻塞 socket）

## 2. 架构设计

### 2.1 整体模型

服务端采用 Reactor 模型：

- 主循环负责监听 socket 事件并分发回调。
- Acceptor 负责接收新连接（accept4 + 非阻塞 + ET 语义下循环 accept）。
- 每个连接由 HttpConn 绑定 Channel，处理读写事件。
- 线程池提供任务执行基础能力，结合 EventLoop 的跨线程任务投递机制实现线程间调度。
- TimerManager 使用最小堆维护超时任务，负责空闲连接超时治理。

### 2.2 关键并发机制

- epoll ET 模式：
  - 读写回调中循环读/写直到 EAGAIN，避免 ET 模式下事件“丢触发”。
  - accept 在可读事件中循环处理，直到无连接可接收。
- 跨线程任务投递：
  - EventLoop 提供 runInLoop/queueInLoop。
  - 借助 eventfd 作为唤醒 fd，使其他线程提交任务后可立即唤醒 epoll_wait。
- Buffer 分散读优化：
  - 通过 readv 同时写入主缓冲区和栈上临时缓冲区，减少“空间不足后再次读取”的额外系统调用。

## 3. HTTP 能力

- 协议版本：HTTP/1.1
- 已支持方法：GET、HEAD、POST（POST 为简化处理）
- 长连接：支持 keep-alive/close 语义
- 静态文件：
  - 根目录可配置
  - 默认路径 / 映射至 /index.html
  - 内置常见扩展名 MIME 映射（html/css/js/json/png/jpg/svg 等）
- 错误处理：返回 400/403/404/500 基础错误页
- 基础安全：拒绝包含 ../ 的路径，防止目录穿越

## 4. 模块说明

- src/main.cpp：程序入口，初始化端口、线程数、资源目录。
- src/Server.*：服务总控，连接生命周期管理、定时器绑定与关闭回收。
- src/EventLoop.*：事件循环、任务队列、eventfd 唤醒。
- src/Epoller.*：epoll 封装与 Channel 注册管理。
- src/Channel.*：fd 事件抽象与回调分发。
- src/Acceptor.*：监听 socket 与新连接接入。
- src/HttpConn.*：单连接收发处理、请求解析与响应发送。
- src/HttpRequest.*：HTTP 请求解析状态机。
- src/HttpResponse.*：响应构造与序列化。
- src/Buffer.*：动态缓冲区、readv/write 读写封装。
- src/Timer.*：最小堆定时器实现。
- src/ThreadPool.h：通用线程池实现。
- resources/：静态文件目录（默认包含 index.html）。

## 5. 构建与运行

### 5.1 构建

```bash
mkdir -p build
cd build
cmake ..
make -j4
```

### 5.2 启动

```bash
./server [port] [threads] [resource_dir]
```

示例：

```bash
./server 8080 4 ../resources
```

## 6. 压测结果（wrk）

在当前 v1.0 版本中，使用 wrk 进行持续压测，服务可稳定处理高并发请求。

压测命令示例：

```bash
wrk -t2 -c1000 -d60s http://localhost:8080/
```

结合实际测试结果与运行表现：

- 2 线程压测下 QPS 达到 10000+。
- 在 1000 并发连接场景可稳定运行。
- 在更高连接压力下（如 10000 连接）服务可持续响应并保持进程稳定。

## 7. 工程特性总结

- 基于 Reactor + epoll ET 的事件驱动架构，具备高并发连接处理能力。
- 使用 eventfd 完成跨线程任务投递唤醒，降低线程切换等待成本。
- 使用 readv 分散读优化网络读取路径，减少系统调用与数据搬移开销。
- 支持 HTTP keep-alive 与静态文件服务，具备基础 Web 服务能力。
- 通过最小堆定时器管理连接超时，降低空闲连接占用。

## 8. 开源说明

- License: MIT
- 适合用于 Linux 网络编程学习、Reactor 模型实践、HTTP 服务器课程设计与性能优化实验。
