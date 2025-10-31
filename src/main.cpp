#include <iostream>
#include <string>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include "utils/config.hpp"

int main(int argc, char* argv[]) {
    // Welcome message
    std::cout << "=== Clash Royale Trading Platform ===" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;
    
    // Load configuration
    auto& config = clash::Config::instance();
    config.load_from_file(".env");
    
    fmt::print("\n🔧 Configuration loaded successfully!\n");
    
    // Test PostgreSQL config
    auto pg_config = config.postgres_config();
    fmt::print("\n📊 PostgreSQL Configuration:\n");
    fmt::print("  Host: {}:{}\n", pg_config.host, pg_config.port);
    fmt::print("  Database: {}\n", pg_config.database);
    fmt::print("  User: {}\n", pg_config.user);
    fmt::print("  Connection String: {}\n", pg_config.connection_string());
    
    // Test Redis config
    auto redis_config = config.redis_config();
    fmt::print("\n📦 Redis Configuration:\n");
    fmt::print("  Host: {}:{}\n", redis_config.host, redis_config.port);
    
    // Test server config
    fmt::print("\n🌐 Server Configuration:\n");
    fmt::print("  Port: {}\n", config.server_port());
    fmt::print("  Host: {}\n", config.server_host());
    fmt::print("  Log Level: {}\n", config.log_level());
    
    // Test nlohmann-json library
    nlohmann::json system_info = {
        {"platform", "C++20 Trading Engine"},
        {"databases", {
            {"postgres", {{"host", pg_config.host}, {"port", pg_config.port}}},
            {"redis", {{"host", redis_config.host}, {"port", redis_config.port}}}
        }},
        {"server", {
            {"port", config.server_port()},
            {"host", config.server_host()}
        }}
    };
    
    std::cout << "\n📋 System Info (JSON):\n" << system_info.dump(2) << std::endl;
    
    std::cout << "\n✅ All systems operational!" << std::endl;
    std::cout << "Ready to build the trading engine...\n" << std::endl;
    
    return 0;
}