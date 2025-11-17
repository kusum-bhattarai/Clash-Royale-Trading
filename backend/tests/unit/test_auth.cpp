#include <gtest/gtest.h>
#include "api/auth.hpp"
#include <thread>
#include <chrono>

using namespace clash_trading::api;

TEST(AuthServiceTest, GenerateJWT) {
    AuthService auth("test_secret_key");
    
    std::string token = auth.generate_jwt("user123", "testuser");
    
    // JWT should have 3 parts separated by dots
    size_t first_dot = token.find('.');
    size_t second_dot = token.find('.', first_dot + 1);
    
    EXPECT_NE(first_dot, std::string::npos);
    EXPECT_NE(second_dot, std::string::npos);
    EXPECT_GT(token.length(), 50);  // JWT should be reasonably long
}

TEST(AuthServiceTest, VerifyValidJWT) {
    AuthService auth("test_secret_key");
    
    std::string token = auth.generate_jwt("user123", "testuser");
    auto payload = auth.verify_jwt(token);
    
    ASSERT_TRUE(payload.has_value());
    EXPECT_EQ(payload->user_id, "user123");
    EXPECT_EQ(payload->username, "testuser");
}

TEST(AuthServiceTest, VerifyInvalidJWT) {
    AuthService auth("test_secret_key");
    
    // Invalid token format
    auto payload1 = auth.verify_jwt("invalid.token");
    EXPECT_FALSE(payload1.has_value());
    
    // Completely invalid
    auto payload2 = auth.verify_jwt("notajwt");
    EXPECT_FALSE(payload2.has_value());
}

TEST(AuthServiceTest, VerifyTamperedJWT) {
    AuthService auth("test_secret_key");
    
    std::string token = auth.generate_jwt("user123", "testuser");
    
    // Tamper with the token (change a character)
    std::string tampered = token;
    tampered[10] = 'X';
    
    auto payload = auth.verify_jwt(tampered);
    EXPECT_FALSE(payload.has_value());
}

TEST(AuthServiceTest, VerifyWrongSecret) {
    AuthService auth1("secret1");
    AuthService auth2("secret2");
    
    std::string token = auth1.generate_jwt("user123", "testuser");
    
    // Try to verify with different secret
    auto payload = auth2.verify_jwt(token);
    EXPECT_FALSE(payload.has_value());
}

TEST(AuthServiceTest, TokenExpiration) {
    // Create auth with 1 second lifetime
    AuthService auth("test_secret", std::chrono::hours(0));  // Actually 0 seconds
    
    std::string token = auth.generate_jwt("user123", "testuser");
    
    // Immediate verification should fail (already expired)
    auto payload = auth.verify_jwt(token);
    EXPECT_FALSE(payload.has_value());
}

TEST(AuthServiceTest, PasswordHashing) {
    AuthService auth("test_secret");
    
    std::string password = "MySecurePassword123!";
    std::string hash = auth.hash_password(password);
    
    // Hash should contain salt and hash separated by $
    EXPECT_NE(hash.find('$'), std::string::npos);
    EXPECT_GT(hash.length(), 50);  // Should be reasonably long
    
    // Same password should produce different hashes (random salt)
    std::string hash2 = auth.hash_password(password);
    EXPECT_NE(hash, hash2);
}

TEST(AuthServiceTest, PasswordVerification) {
    AuthService auth("test_secret");
    
    std::string password = "MySecurePassword123!";
    std::string hash = auth.hash_password(password);
    
    // Correct password should verify
    EXPECT_TRUE(auth.verify_password(password, hash));
    
    // Wrong password should not verify
    EXPECT_FALSE(auth.verify_password("WrongPassword", hash));
    EXPECT_FALSE(auth.verify_password("", hash));
}

TEST(AuthServiceTest, PasswordWithSpecialCharacters) {
    AuthService auth("test_secret");
    
    std::string password = "P@$$w0rd!#%^&*()_+-=[]{}|;':\",./<>?";
    std::string hash = auth.hash_password(password);
    
    EXPECT_TRUE(auth.verify_password(password, hash));
    EXPECT_FALSE(auth.verify_password("P@$$w0rd", hash));
}

TEST(AuthServiceTest, EmptyPassword) {
    AuthService auth("test_secret");
    
    std::string password = "";
    std::string hash = auth.hash_password(password);
    
    EXPECT_TRUE(auth.verify_password("", hash));
    EXPECT_FALSE(auth.verify_password("anything", hash));
}

TEST(AuthServiceTest, JWTPayloadFields) {
    AuthService auth("test_secret", std::chrono::hours(24));
    
    std::string token = auth.generate_jwt("user123", "testuser");
    auto payload = auth.verify_jwt(token);
    
    ASSERT_TRUE(payload.has_value());
    EXPECT_EQ(payload->user_id, "user123");
    EXPECT_EQ(payload->username, "testuser");
    
    // Check that issued_at is in the past (or now)
    auto now = std::chrono::system_clock::now();
    EXPECT_LE(payload->issued_at, now);
    
    // Check that expires_at is in the future
    EXPECT_GT(payload->expires_at, now);
    
    // Check that expiration is roughly 24 hours from now
    auto time_until_exp = std::chrono::duration_cast<std::chrono::hours>(
        payload->expires_at - payload->issued_at
    );
    EXPECT_EQ(time_until_exp.count(), 24);
}