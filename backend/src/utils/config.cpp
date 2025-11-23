#include "utils/config.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>

namespace clash {

void Config::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open config file: " << filepath << std::endl;
        std::cerr << "Using environment variables and defaults instead." << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        parse_line(line);
    }
}

void Config::parse_line(const std::string& line) {
    // Skip empty lines and comments
    std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
        return;
    }

    // Find the '=' delimiter
    size_t pos = trimmed.find('=');
    if (pos == std::string::npos) {
        return;
    }

    std::string key = trim(trimmed.substr(0, pos));
    std::string value = trim(trimmed.substr(pos + 1));

    if (value.length() >= 2 && 
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.length() - 2);
    }

    config_map_[key] = value;
}

std::string Config::trim(const std::string& str) const {
    auto start = std::find_if_not(str.begin(), str.end(), 
        [](unsigned char ch) { return std::isspace(ch); });
    auto end = std::find_if_not(str.rbegin(), str.rend(), 
        [](unsigned char ch) { return std::isspace(ch); }).base();
    
    return (start < end) ? std::string(start, end) : std::string();
}

std::string Config::get(const std::string& key, const std::string& default_value) const {
    // First check environment variables
    const char* env_val = std::getenv(key.c_str());
    if (env_val != nullptr) {
        return std::string(env_val);
    }

    // Then check loaded config
    auto it = config_map_.find(key);
    if (it != config_map_.end()) {
        return it->second;
    }

    return default_value;
}

int Config::get_int(const std::string& key, int default_value) const {
    std::string value = get(key, "");
    if (value.empty()) {
        return default_value;
    }

    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        return default_value;
    }
}

bool Config::get_bool(const std::string& key, bool default_value) const {
    std::string value = get(key, "");
    if (value.empty()) {
        return default_value;
    }

    // Convert to lowercase for comparison
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return std::tolower(c); });

    return value == "true" || value == "1" || value == "yes" || value == "on";
}

Config::DatabaseConfig Config::postgres_config() const {
    return DatabaseConfig{
        .host = get("POSTGRES_HOST", "localhost"),
        .port = get_int("POSTGRES_PORT", 5432),
        .database = get("POSTGRES_DB", "clash_trading"),
        .user = get("POSTGRES_USER", "clash_user"),
        .password = get("POSTGRES_PASSWORD", "")
    };
}

std::string Config::DatabaseConfig::connection_string() const {
    std::ostringstream oss;
    oss << "host=" << host
        << " port=" << port
        << " dbname=" << database
        << " user=" << user
        << " password=" << password;
    return oss.str();
}

Config::RedisConfig Config::redis_config() const {
    return RedisConfig{
        .host = get("REDIS_HOST", "localhost"),
        .port = get_int("REDIS_PORT", 6379),
        .password = get("REDIS_PASSWORD", "")
    };
}

Config::ClashRoyaleAPIConfig Config::clash_royale_api_config() const {
    return ClashRoyaleAPIConfig{
        .base_url = get("CLASH_ROYALE_API_BASE_URL", "https://api.clashroyale.com/v1"),
        .api_key = get("CLASH_ROYALE_API_KEY", ""),
        .rate_limit_per_second = get_int("CLASH_ROYALE_RATE_LIMIT", 8),
        .timeout_seconds = get_int("CLASH_ROYALE_TIMEOUT", 10)
    };
}

} // namespace clash