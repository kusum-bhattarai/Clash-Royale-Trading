#pragma once
#include <spdlog/spdlog.h>
#include <memory>
#include <string>

namespace clash_trading {
namespace utils {

// Call once at startup before any LOG_* macros are used.
void init_logger(const std::string& log_file = "logs/trading.log");

std::shared_ptr<spdlog::logger> get_logger();

} // namespace utils
} // namespace clash_trading

#define LOG_DEBUG(...) clash_trading::utils::get_logger()->debug(__VA_ARGS__)
#define LOG_INFO(...)  clash_trading::utils::get_logger()->info(__VA_ARGS__)
#define LOG_WARN(...)  clash_trading::utils::get_logger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) clash_trading::utils::get_logger()->error(__VA_ARGS__)
