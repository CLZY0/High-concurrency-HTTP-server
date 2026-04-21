// src/main.cpp
#include "Server.h"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <thread>
#include <sys/resource.h>

// 忽略 SIGPIPE：防止向已关闭的连接写数据时程序崩溃
// 这是服务器程序的标准做法
static void ignoreSigPipe()
{
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, nullptr);
}

static void tuneResourceLimit()
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
    {
        const rlim_t target = 200000;
        if (rl.rlim_cur < target)
        {
            rl.rlim_cur = (rl.rlim_max < target) ? rl.rlim_max : target;
            setrlimit(RLIMIT_NOFILE, &rl);
        }
    }
}

int main(int argc, char *argv[])
{
    ignoreSigPipe();
    tuneResourceLimit();

    int port = 8080;
    int threads = static_cast<int>(std::thread::hardware_concurrency());
    std::string dir = "../resources";

    // 简单的命令行参数处理
    if (argc >= 2)
        port = std::atoi(argv[1]);
    if (argc >= 3)
        threads = std::atoi(argv[2]);
    if (argc >= 4)
        dir = argv[3];

    if (threads <= 0)
        threads = 1;

    try
    {
        Server server(port, threads, dir);
        server.start();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}