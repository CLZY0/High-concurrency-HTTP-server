cmake_minimum_required(VERSION 3.10)
project(TinyHTTPServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项：开启警告与发布优化
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -DNDEBUG")

# 限制构建阶段资源占用（支持 job pool 的生成器会严格执行）
set_property(GLOBAL PROPERTY JOB_POOLS compile_pool=2 link_pool=1)

# 收集 src 目录下所有 .cpp 文件
file(GLOB SOURCES "src/*.cpp")

add_executable(server ${SOURCES})

set_property(TARGET server PROPERTY JOB_POOL_COMPILE compile_pool)
set_property(TARGET server PROPERTY JOB_POOL_LINK link_pool)

# 链接 pthread 库（多线程必须）
target_link_libraries(server pthread)

# 指定头文件搜索路径
target_include_directories(server PRIVATE src)