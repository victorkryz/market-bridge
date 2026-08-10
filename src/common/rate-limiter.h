#pragma once

struct RateLimiter
{
    virtual bool allow() = 0;
    virtual void reset() {};

    virtual ~RateLimiter() = default;
};