#include "tocken-bucket.h"
#include <algorithm>

TokenBucket::TokenBucket(double rate, double capacity)
    : rate_(rate),
      capacity_(capacity),
      tokens_(capacity),
      last_refill_(std::chrono::steady_clock::now())
{
}

bool TokenBucket::allow()
{
    constexpr auto rate_period = std::chrono::seconds{1};

    std::lock_guard lock(mutex_);

    refill();

    const bool allow = (tokens_ >= rate_period.count());
    if (allow)
        tokens_ -= rate_period.count();

    return allow;
}

void TokenBucket::reset()
{
    std::lock_guard lock(mutex_);
    tokens_ = capacity_;
    last_refill_ = std::chrono::steady_clock::now();
}

void TokenBucket::refill()
{
    auto now = std::chrono::steady_clock::now();

    const auto elapsed =
        std::chrono::duration<double>(now - last_refill_).count();

    tokens_ = std::min(capacity_, tokens_ + elapsed * rate_);

    last_refill_ = now;
}