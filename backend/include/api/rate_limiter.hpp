#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace clash_trading {
namespace api {

struct TokenBucket {
    double tokens;
    std::chrono::steady_clock::time_point last_refill;
};

// Token-bucket rate limiter — one bucket per user_id.
// Thread-safe; designed for the order-placement hot path.
// Production note: the bucket map grows unbounded — add LRU eviction for multi-day uptime.
class RateLimiter {
public:
    // rate_per_second: sustained token refill rate
    // burst:           max burst capacity (also the initial token count)
    explicit RateLimiter(double rate_per_second = 10.0, double burst = 10.0);

    // Returns true if the request is allowed (consumes one token).
    // Returns false if the bucket is empty — caller should respond 429.
    bool allow(const std::string& user_id);

    // Milliseconds until at least one token is available.
    // Call only when allow() returned false.
    int64_t retry_after_ms(const std::string& user_id) const;

private:
    double rate_;   // tokens refilled per second
    double burst_;  // max token capacity

    mutable std::mutex mutex_;
    std::unordered_map<std::string, TokenBucket> buckets_;

    void refill(TokenBucket& bucket, std::chrono::steady_clock::time_point now) const;
};

} // namespace api
} // namespace clash_trading
