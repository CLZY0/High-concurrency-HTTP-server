// src/Channel.cpp
#include "Channel.h"
#include "EventLoop.h"
#include <sys/epoll.h>
#include <cassert>

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd)
    , events_(0), revents_(0), index_(-1)
    , tied_(false)

    tied_ = true;
}







    
