#include "api/auth.hpp"
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <random>
#include <stdexcept>

namespace clash_trading {
namespace api {

AuthService::AuthService(const std::string& secret_key, std::chrono::hours lifetime)
    : secret_key_(secret_key), token_lifetime_(lifetime) {}

std::string AuthService::generate_jwt(const std::string& user_id, 
                                      const std::string& username) {
    using namespace std::chrono;
    
    auto now = system_clock::now();
    auto exp = now + token_lifetime_;
    
    // Create JWT header
    nlohmann::json header = {
        {"alg", "HS256"},
        {"typ", "JWT"}
    };
    
    // Create JWT payload
    nlohmann::json payload = {
        {"user_id", user_id},
        {"username", username},
        {"iat", duration_cast<seconds>(now.time_since_epoch()).count()},
        {"exp", duration_cast<seconds>(exp.time_since_epoch()).count()}
    };
    
    // Encode header and payload
    std::string header_b64 = base64_encode(header.dump());
    std::string payload_b64 = base64_encode(payload.dump());
    
    // Create signature
    std::string unsigned_token = header_b64 + "." + payload_b64;
    std::string signature = hmac_sha256(unsigned_token);
    std::string signature_b64 = base64_encode(signature);
    
    // Return complete JWT
    return unsigned_token + "." + signature_b64;
}

std::optional<JWTPayload> AuthService::verify_jwt(const std::string& token) {
    using namespace std::chrono;
    
    // Split token into parts
    size_t first_dot = token.find('.');
    size_t second_dot = token.find('.', first_dot + 1);
    
    if (first_dot == std::string::npos || second_dot == std::string::npos) {
        return std::nullopt;  // Invalid token format
    }
    
    std::string header_b64 = token.substr(0, first_dot);
    std::string payload_b64 = token.substr(first_dot + 1, second_dot - first_dot - 1);
    std::string signature_b64 = token.substr(second_dot + 1);
    
    // Verify signature
    std::string unsigned_token = header_b64 + "." + payload_b64;
    std::string expected_signature = hmac_sha256(unsigned_token);
    std::string expected_signature_b64 = base64_encode(expected_signature);
    
    if (signature_b64 != expected_signature_b64) {
        return std::nullopt;  // Invalid signature
    }
    
    // Decode payload
    try {
        std::string payload_json = base64_decode(payload_b64);
        auto payload = nlohmann::json::parse(payload_json);
        
        // Check expiration
        int64_t exp = payload["exp"].get<int64_t>();
        auto exp_time = system_clock::from_time_t(exp);
        auto now = system_clock::now();
        
        if (now > exp_time) {
            return std::nullopt;  // Token expired
        }
        
        // Build JWTPayload
        JWTPayload jwt_payload;
        jwt_payload.user_id = payload["user_id"].get<std::string>();
        jwt_payload.username = payload["username"].get<std::string>();
        jwt_payload.issued_at = system_clock::from_time_t(payload["iat"].get<int64_t>());
        jwt_payload.expires_at = exp_time;
        
        return jwt_payload;
        
    } catch (const std::exception&) {
        return std::nullopt;  // Parse error
    }
}

std::string AuthService::hash_password(const std::string& password) {
    // Generate random salt
    std::string salt = generate_salt();
    
    // Combine password + salt
    std::string salted = password + salt;
    
    // SHA-256 hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(salted.c_str()), 
           salted.length(), 
           hash);
    
    // Convert to hex
    std::ostringstream hex_stream;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hex_stream << std::hex << std::setw(2) << std::setfill('0') 
                   << static_cast<int>(hash[i]);
    }
    
    // Return salt + hash (so we can verify later)
    return salt + "$" + hex_stream.str();
}

bool AuthService::verify_password(const std::string& password, 
                                  const std::string& hash) {
    // Split salt and hash
    size_t sep = hash.find('$');
    if (sep == std::string::npos) {
        return false;
    }
    
    std::string salt = hash.substr(0, sep);
    std::string stored_hash = hash.substr(sep + 1);
    
    // Hash the password with the same salt
    std::string salted = password + salt;
    
    unsigned char computed_hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(salted.c_str()), 
           salted.length(), 
           computed_hash);
    
    // Convert to hex
    std::ostringstream hex_stream;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hex_stream << std::hex << std::setw(2) << std::setfill('0') 
                   << static_cast<int>(computed_hash[i]);
    }
    
    return hex_stream.str() == stored_hash;
}

std::string AuthService::base64_encode(const std::string& data) {
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string encoded;
    int val = 0;
    int valb = -6;
    
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (encoded.size() % 4) {
        encoded.push_back('=');
    }
    
    return encoded;
}

std::string AuthService::base64_decode(const std::string& data) {
    static const int base64_table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    
    std::string decoded;
    int val = 0;
    int valb = -8;
    
    for (unsigned char c : data) {
        if (base64_table[c] == -1) break;
        val = (val << 6) + base64_table[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return decoded;
}

std::string AuthService::hmac_sha256(const std::string& data) {
    unsigned char* result = HMAC(EVP_sha256(),
                                 secret_key_.c_str(), secret_key_.length(),
                                 reinterpret_cast<const unsigned char*>(data.c_str()), 
                                 data.length(),
                                 nullptr, nullptr);
    
    return std::string(reinterpret_cast<char*>(result), SHA256_DIGEST_LENGTH);
}

std::string AuthService::generate_salt() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);
    
    std::ostringstream ss;
    for (int i = 0; i < 16; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    
    return ss.str();
}

} // namespace api
} // namespace clash_trading