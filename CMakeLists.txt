cmake_minimum_required(VERSION 3.10)
project(TinyHTTPServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项：开启所有警告，优化级别O2
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# 收集 src 目录下所有 .cpp 文件
file(GLOB SOURCES "src/*.cpp")

add_executable(server ${SOURCES})

# 链接 pthread 库（多线程必须）
target_link_libraries(server pthread)

# 指定头文件搜索路径
target_include_directories(server PRIVATE src)