#pragma once    

#include <chrono>
#include <mutex>
#include "common/rate-limiter.h"


class TokenBucket : public RateLimiter
{
public:
    TokenBucket(double rate, double capacity);

    bool allow() override;
    void reset() override;
    
private:
    void refill();
    
private:
    double rate_;
    double capacity_;
    double tokens_;
    std::chrono::steady_clock::time_point last_refill_;
    std::mutex mutex_;
};