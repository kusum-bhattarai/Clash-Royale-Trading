#pragma once

// Structured error codes for all API error responses.
// Format: { "error": "ERROR_KEY", "message": "...", "code": 4XXX }
//
// 4xxx — client errors (bad request, auth, business logic)
// 5xxx — server errors (unexpected failures)

namespace clash_trading {
namespace api {
namespace error_codes {

// 4xxx — client errors
constexpr int UNAUTHORIZED           = 4000;
constexpr int INSUFFICIENT_BALANCE   = 4001;
constexpr int INSUFFICIENT_INVENTORY = 4002;
constexpr int INVALID_ORDER          = 4003;
constexpr int RESOURCE_NOT_FOUND     = 4004;
constexpr int RATE_LIMIT_EXCEEDED    = 4005;
constexpr int INVALID_REQUEST        = 4006;
constexpr int INVALID_CREDENTIALS    = 4007;
constexpr int DUPLICATE_RESOURCE     = 4008;

// 5xxx — server errors
constexpr int INTERNAL_ERROR         = 5000;
constexpr int TRADE_EXECUTION_FAILED = 5001;

} // namespace error_codes
} // namespace api
} // namespace clash_trading
