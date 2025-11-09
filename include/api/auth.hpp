#pragma once

#include <string>
#include <optional>
#include <chrono>

namespace clash_trading {
namespace api {

// JWT payload structure
struct JWTPayload {
    std::string user_id;
    std::string username;
    std::chrono::system_clock::time_point issued_at;
    std::chrono::system_clock::time_point expires_at;
};

// Authentication service for JWT and password management
class AuthService {
private:
    std::string secret_key_;
    std::chrono::hours token_lifetime_;  // Default: 24 hours
    
public:
    explicit AuthService(const std::string& secret_key, 
                        std::chrono::hours lifetime = std::chrono::hours(24));
    
    // Generate JWT token for a user
    std::string generate_jwt(const std::string& user_id, 
                            const std::string& username);
    
    // Verify and decode JWT token
    std::optional<JWTPayload> verify_jwt(const std::string& token);
    
    // Hash password using SHA-256 + salt
    std::string hash_password(const std::string& password);
    
    // Verify password against hash
    bool verify_password(const std::string& password, const std::string& hash);
    
private:
    // Base64 encoding/decoding
    std::string base64_encode(const std::string& data);
    std::string base64_decode(const std::string& data);
    
    // HMAC-SHA256 signature
    std::string hmac_sha256(const std::string& data);
    
    // Generate random salt for password hashing
    std::string generate_salt();
};

} // namespace api
} // namespace clash_trading