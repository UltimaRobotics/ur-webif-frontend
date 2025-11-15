#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <jwt-cpp/jwt.h>

struct UserInfo {
    std::string username;
    std::string email;
    std::string role;
    std::string full_name;
    std::string created_at;
    std::string last_login;
    std::string auth_method;
};

// Utility function declaration
std::string get_current_timestamp();

class JWTManager {
private:
    std::string secret_;
    std::string issuer_;
    std::string audience_;
    int token_expiry_minutes_;
    int refresh_token_expiry_minutes_;
    bool enable_sliding_expiration_;
    int token_refresh_threshold_minutes_;
    
    std::string generate_token(const UserInfo& user_info, 
                               const std::chrono::seconds& expiry,
                               const std::string& token_type = "access") const;
    std::string generate_jti() const;
    
public:
    JWTManager(const std::string& secret, 
               const std::string& issuer, 
               const std::string& audience,
               int token_expiry_minutes,
               int refresh_token_expiry_minutes,
               bool enable_sliding_expiration = true,
               int token_refresh_threshold_minutes = 10);
    
    ~JWTManager() = default;
    
    // Token generation
    std::string generate_access_token(const UserInfo& user_info) const;
    std::string generate_refresh_token(const UserInfo& user_info) const;
    
    // Token validation
    bool validate_token(const std::string& token) const;
    UserInfo extract_user_info(const std::string& token) const;
    
    // Token refresh
    std::string refresh_access_token(const std::string& refresh_token) const;
    bool should_refresh_token(const std::string& token) const;
    
    // Utility methods
    std::string get_token_expiry_time(const std::string& token) const;
    std::string get_token_type(const std::string& token) const;
    bool is_token_expired(const std::string& token) const;
    std::string get_token_subject(const std::string& token) const;
    std::string get_token_jti(const std::string& token) const;
    std::string get_secret() const;
};
