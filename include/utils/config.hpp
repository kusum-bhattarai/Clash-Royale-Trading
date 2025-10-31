#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <fstream>
#include <sstream>

namespace clash {

class Config {
public:
    static Config& instance() {
        static Config config;
        return config;
    }

    // Load configuration from .env file
    void load_from_file(const std::string& filepath = ".env");

    // Get configuration values
    std::string get(const std::string& key, const std::string& default_value = "") const;
    int get_int(const std::string& key, int default_value = 0) const;
    bool get_bool(const std::string& key, bool default_value = false) const;

    // Database configuration
    struct DatabaseConfig {
        std::string host;
        int port;
        std::string database;
        std::string user;
        std::string password;
        
        std::string connection_string() const;
    };

    DatabaseConfig postgres_config() const;

    struct RedisConfig {
        std::string host;
        int port;
        std::string password;
    };

    RedisConfig redis_config() const;

    // Server configuration
    int server_port() const { return get_int("SERVER_PORT", 8080); }
    std::string server_host() const { return get("SERVER_HOST", "0.0.0.0"); }

    // Logging
    std::string log_level() const { return get("LOG_LEVEL", "info"); }

    // JWT
    std::string jwt_secret() const { return get("JWT_SECRET", "change_me"); }

private:
    Config() = default;
    std::unordered_map<std::string, std::string> config_map_;
    
    void parse_line(const std::string& line);
    std::string trim(const std::string& str) const;
};

} // namespace clash