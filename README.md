# TinyHTTPServer v1.1

一个基于 C++17 的轻量级高并发 HTTP/1.1 服务器，采用 Reactor 事件驱动模型与 epoll ET（边缘触发）机制，面向 Linux 场景实现高连接数下的稳定请求处理。

项目聚焦于以下目标：

- 高并发连接处理：基于非阻塞 I/O + epoll ET 降低事件分发开销。
- 低系统调用成本：Buffer 使用 readv 分散读，在高吞吐场景减少读路径系统调用次数。
- 长连接复用：支持 HTTP/1.1 keep-alive，降低重复建连开销。
- 静态资源服务：支持基础 MIME 类型识别与静态文件返回。
- 多核扩展：主从 Reactor 多 EventLoop 分工，避免单线程事件循环成为瓶颈。

## 1. 版本信息

- 当前版本：v1.1
- 语言标准：C++17
- 构建方式：CMake
- 运行平台：Linux（依赖 epoll、eventfd、非阻塞 socket）

### 1.1 版本历史

#### v1.0（基线版本）

- 架构：单 Reactor + epoll ET 事件驱动。
- 协议能力：支持 HTTP/1.1、GET/HEAD/POST、keep-alive。
- I/O 与缓冲：Buffer 使用 readv 分散读，降低高吞吐场景下读路径系统调用成本。
- 资源服务：支持静态文件返回、基础 MIME 类型识别。
- 稳定性：具备基础错误页处理与目录穿越拦截（拒绝 ../）。
- 构建方式：CMake + C++17。

#### v1.1（性能与稳定性增强）

- 相对 v1.0 的架构改进：
  - 从单 Reactor 升级为主从 Reactor，主线程专注 accept，连接轮询分发到多个 I/O EventLoop，缓解单线程瓶颈。
- 相对 v1.0 的热点路径优化：
  - ET 读写回调改为循环到 EAGAIN/EWOULDBLOCK，避免边缘触发模式下漏读漏写导致吞吐下降。
  - 新增静态完整响应缓存（GET/HEAD + keep-alive/close），避免热点请求重复读盘与重复序列化。
- 相对 v1.0 的稳定性增强：
  - 修复连接生命周期中的重复计数递减等问题，提升长时间压测稳定性。
  - 接入层增强：提升 listen backlog，accepted socket 启用 TCP_NODELAY/SO_KEEPALIVE，降低排队与尾延迟抖动。
- 相对 v1.0 的工程改进：
  - 增加构建资源约束：CMake 受控并行构建池 + 文档统一 `--parallel 2`，降低构建阶段资源打满导致崩溃风险。
- 版本结果：
  - 在 `wrk -t2 -c10000 -d60s http://localhost:8080/` 下，QPS 稳定 50000+，平均延迟 < 200ms。

## 2. 架构设计

### 2.1 整体模型

服务端采用 Reactor 模型：

- 主循环负责监听 socket 事件（accept），并将连接按轮询分发到多个 I/O 子循环。
- Acceptor 负责接收新连接（accept4 + 非阻塞 + ET 语义下循环 accept）。
- 每个连接由 HttpConn 绑定 Channel，在对应 I/O EventLoop 中处理读写事件。
- EventLoop 之间通过 runInLoop/queueInLoop + eventfd 唤醒机制完成跨线程调度。

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
- src/Server.*：服务总控，主循环 accept 与连接分发、工作循环生命周期管理。
- src/EventLoop.*：事件循环、任务队列、eventfd 唤醒。
- src/Epoller.*：epoll 封装与 Channel 注册管理。
- src/Channel.*：fd 事件抽象与回调分发。
- src/Acceptor.*：监听 socket 与新连接接入。
- src/HttpConn.*：单连接收发处理、请求解析与响应发送。
- src/HttpRequest.*：HTTP 请求解析状态机。
- src/HttpResponse.*：响应构造与序列化。
- src/Buffer.*：动态缓冲区、readv/write 读写封装。
- resources/：静态文件目录（默认包含 index.html）。

## 5. 构建与运行

### 5.1 构建

v1.1 起，构建过程默认要求限制资源占用，避免在低内存/低核环境下因并行编译过高导致崩溃。

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --parallel 2
```

可选（进一步限制内存峰值）：

```bash
ulimit -Sv 2097152
cmake --build . --parallel 2
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

在 v1.1 中，使用 wrk 在 10000 并发连接下进行持续压测：

压测命令示例：

```bash
wrk -t2 -c10000 -d60s http://localhost:8080/
```

实测结果（本地）：

- Requests/sec: 52973.90
- Avg Latency: 149.82ms

二次验证（30s）：

- Requests/sec: 55415.13
- Avg Latency: 148.19ms

结论：在 `wrk -t2 -c10000 -d60s` 条件下，QPS 稳定达到 50000+，平均延迟低于 200ms。

## 7. 工程特性总结

- 基于 Reactor + epoll ET 的事件驱动架构，具备高并发连接处理能力。
- 使用 eventfd 完成跨线程任务投递唤醒，降低线程切换等待成本。
- 使用 readv 分散读优化网络读取路径，减少系统调用与数据搬移开销。
- 支持 HTTP keep-alive 与静态文件服务，具备基础 Web 服务能力。
- 多 EventLoop 并发处理连接，适配 10k 级并发压测。
- 静态响应缓存降低 CPU 与磁盘重复开销。

## 8. 开源说明

- License: MIT
- 适合用于 Linux 网络编程学习、Reactor 模型实践、HTTP 服务器课程设计与性能优化实验。
