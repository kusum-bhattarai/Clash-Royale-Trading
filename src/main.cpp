#include <iostream>
#include <string>
#include <fmt/core.h>
#include <nlohmann/json.hpp>

int main(int argc, char* argv[]) {
    // Welcome message
    std::cout << "=== Clash Royale Trading Platform ===" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;
    
    // Test fmt library
    std::string platform = "C++20 Trading Engine";
    fmt::print("\n🚀 {} initialized successfully!\n", platform);
    
    // Test nlohmann-json library
    nlohmann::json config = {
        {"name", "Clash Royale Trading"},
        {"port", 8080},
        {"max_connections", 1000},
        {"features", {"order_matching", "websocket", "real_time"}}
    };
    
    std::cout << "\n📋 Configuration:\n" << config.dump(2) << std::endl;
    
    std::cout << "\n✅ All dependencies working correctly!" << std::endl;
    std::cout << "Ready to start building the trading platform...\n" << std::endl;
    
    return 0;
}