#include "utils/logger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <filesystem>

namespace clash_trading {
namespace utils {

static std::shared_ptr<spdlog::logger> g_logger;

void init_logger(const std::string& log_file) {
    std::filesystem::create_directories(
        std::filesystem::path(log_file).parent_path()
    );

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info);

    // 10 MB per file, keep 3 rotations
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        log_file, 10 * 1024 * 1024, 3
    );
    file_sink->set_level(spdlog::level::debug);

    g_logger = std::make_shared<spdlog::logger>(
        "trading",
        spdlog::sinks_init_list{console_sink, file_sink}
    );
    g_logger->set_level(spdlog::level::debug);
    g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%^%-5l%$] [tid:%t] %v");
    g_logger->flush_on(spdlog::level::warn);

    spdlog::register_logger(g_logger);
    spdlog::set_default_logger(g_logger);
}

std::shared_ptr<spdlog::logger> get_logger() {
    if (!g_logger) {
        init_logger();
    }
    return g_logger;
}

} // namespace utils
} // namespace clash_trading
