#include "jwt_manager.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>

JWTManager::JWTManager(const std::string& secret, 
                       const std::string& issuer, 
                       const std::string& audience,
                       int token_expiry_minutes,
                       int refresh_token_expiry_minutes,
                       bool enable_sliding_expiration,
                       int token_refresh_threshold_minutes)
    : secret_(secret), issuer_(issuer), audience_(audience),
      token_expiry_minutes_(token_expiry_minutes),
      refresh_token_expiry_minutes_(refresh_token_expiry_minutes),
      enable_sliding_expiration_(enable_sliding_expiration),
      token_refresh_threshold_minutes_(token_refresh_threshold_minutes) {
}

std::string JWTManager::generate_jti() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 35);
    
    std::string jti;
    for (int i = 0; i < 16; ++i) {
        int digit = dis(gen);
        if (digit < 10) {
            jti += std::to_string(digit);
        } else {
            jti += 'a' + (digit - 10);
        }
    }
    return jti;
}

std::string JWTManager::generate_token(const UserInfo& user_info, 
                                       const std::chrono::seconds& expiry,
                                       const std::string& token_type) const {
    auto now = std::chrono::system_clock::now();
    auto token = jwt::create()
        .set_issuer(issuer_)
        .set_audience(audience_)
        .set_issued_at(now)
        .set_expires_at(now + expiry)
        .set_not_before(now) // Valid from issue time (not before)
        .set_subject(user_info.username)
        .set_id(generate_jti()) // JWT ID for token tracking/revocation
        .set_payload_claim("type", jwt::claim(std::string(token_type)))
        .set_payload_claim("email", jwt::claim(user_info.email))
        .set_payload_claim("role", jwt::claim(user_info.role))
        .set_payload_claim("full_name", jwt::claim(user_info.full_name))
        .set_payload_claim("auth_method", jwt::claim(user_info.auth_method))
        .set_payload_claim("created_at", jwt::claim(user_info.created_at))
        .set_payload_claim("last_login", jwt::claim(user_info.last_login));
    
    return token.sign(jwt::algorithm::hs256{secret_});
}

std::string JWTManager::generate_access_token(const UserInfo& user_info) const {
    auto expiry = std::chrono::minutes(token_expiry_minutes_);
    return generate_token(user_info, expiry, "access");
}

std::string JWTManager::generate_refresh_token(const UserInfo& user_info) const {
    auto expiry = std::chrono::minutes(refresh_token_expiry_minutes_);
    return generate_token(user_info, expiry, "refresh");
}

bool JWTManager::should_refresh_token(const std::string& token) const {
    if (!enable_sliding_expiration_) {
        return false;
    }
    
    try {
        auto decoded = jwt::decode(token);
        auto exp = decoded.get_expires_at();
        auto now = std::chrono::system_clock::now();
        auto threshold = std::chrono::minutes(token_refresh_threshold_minutes_);
        
        return (exp - now) <= threshold;
    } catch (...) {
        return false;
    }
}

bool JWTManager::validate_token(const std::string& token) const {
    try {
        auto decoded = jwt::decode(token);
        
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret_})
            .with_issuer(issuer_)
            .with_audience(audience_);
        
        verifier.verify(decoded);
        
        // Check if token is expired
        auto expires_at = decoded.get_expires_at();
        if (expires_at.time_since_epoch().count() < std::chrono::system_clock::now().time_since_epoch().count()) {
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

UserInfo JWTManager::extract_user_info(const std::string& token) const {
    UserInfo user_info;
    
    try {
        auto decoded = jwt::decode(token);
        
        user_info.username = decoded.get_subject();
        user_info.email = decoded.get_payload_claim("email").as_string();
        user_info.role = decoded.get_payload_claim("role").as_string();
        user_info.full_name = decoded.get_payload_claim("full_name").as_string();
        user_info.auth_method = decoded.get_payload_claim("auth_method").as_string();
        user_info.created_at = decoded.get_payload_claim("created_at").as_string();
        user_info.last_login = decoded.get_payload_claim("last_login").as_string();
        
    } catch (const std::exception& e) {
        // Return empty user info on error
        user_info = UserInfo{};
    }
    
    return user_info;
}

std::string JWTManager::refresh_access_token(const std::string& refresh_token) const {
    try {
        if (!validate_token(refresh_token)) {
            return "";
        }
        
        auto decoded = jwt::decode(refresh_token);
        auto type_claim = decoded.get_payload_claim("type");
        
        if (type_claim.as_string() != "refresh") {
            return "";
        }
        
        UserInfo user_info = extract_user_info(refresh_token);
        if (user_info.username.empty()) {
            return "";
        }
        
        // Update last login time
        user_info.last_login = get_current_timestamp();
        
        return generate_access_token(user_info);
        
    } catch (const std::exception& e) {
        return "";
    }
}

std::string JWTManager::get_token_expiry_time(const std::string& token) const {
    try {
        auto decoded = jwt::decode(token);
        auto expires_at = decoded.get_expires_at();
        
        auto time_t = std::chrono::system_clock::to_time_t(expires_at);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S UTC");
        return ss.str();
        
    } catch (const std::exception& e) {
        // Return empty string on error
    }
    
    return "";
}

std::string JWTManager::get_token_type(const std::string& token) const {
    try {
        auto decoded = jwt::decode(token);
        return decoded.get_payload_claim("type").as_string();
    } catch (const std::exception& e) {
        return "";
    }
}

bool JWTManager::is_token_expired(const std::string& token) const {
    try {
        auto decoded = jwt::decode(token);
        auto expires_at = decoded.get_expires_at();
        
        return expires_at.time_since_epoch().count() < std::chrono::system_clock::now().time_since_epoch().count();
        
    } catch (const std::exception& e) {
        return true; // Consider invalid tokens as expired
    }
}

std::string JWTManager::get_token_subject(const std::string& token) const {
    try {
        auto decoded = jwt::decode(token);
        return decoded.get_subject();
    } catch (const std::exception& e) {
        return "";
    }
}

std::string JWTManager::get_token_jti(const std::string& token) const {
    try {
        auto decoded = jwt::decode(token);
        return decoded.get_id();
    } catch (const std::exception& e) {
        // Fallback to payload claim if get_id() doesn't work
        try {
            auto decoded = jwt::decode(token);
            return decoded.get_payload_claim("jti").as_string();
        } catch (...) {
            return "";
        }
    }
}

std::string JWTManager::get_secret() const {
    return secret_;
}

// Utility function to get current timestamp
std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}
