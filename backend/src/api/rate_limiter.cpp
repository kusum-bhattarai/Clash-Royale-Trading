#include "api/rate_limiter.hpp"
#include <algorithm>

namespace clash_trading {
namespace api {

RateLimiter::RateLimiter(double rate_per_second, double burst)
    : rate_(rate_per_second), burst_(burst) {}

void RateLimiter::refill(TokenBucket& bucket, std::chrono::steady_clock::time_point now) const {
    double elapsed_sec = std::chrono::duration<double>(now - bucket.last_refill).count();
    bucket.tokens      = std::min(burst_, bucket.tokens + elapsed_sec * rate_);
    bucket.last_refill = now;
}

bool RateLimiter::allow(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();

    auto it = buckets_.find(user_id);
    if (it == buckets_.end()) {
        // First request: start with a full bucket minus one consumed token
        buckets_[user_id] = TokenBucket{burst_ - 1.0, now};
        return true;
    }

    TokenBucket& bucket = it->second;
    refill(bucket, now);

    if (bucket.tokens >= 1.0) {
        bucket.tokens -= 1.0;
        return true;
    }
    return false;
}

int64_t RateLimiter::retry_after_ms(const std::string& user_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buckets_.find(user_id);
    if (it == buckets_.end()) return 0;

    const TokenBucket& bucket = it->second;
    double deficit  = 1.0 - bucket.tokens;  // tokens needed
    double wait_sec = (deficit > 0.0) ? deficit / rate_ : 0.0;
    return static_cast<int64_t>(wait_sec * 1000.0) + 1; // +1 ms buffer
}

} // namespace api
} // namespace clash_trading
