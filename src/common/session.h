#pragma once

#include <cstdint>

struct Session
{
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual uint64_t get_id() = 0;

    virtual ~Session() =  default;
};