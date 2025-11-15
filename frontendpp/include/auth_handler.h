#pragma once

#include "jwt_manager.h"
#include "http_server.h"
#include <sqlite3.h>
#include <memory>
#include "logger.h"
#include "frontendpp/cmake/attributes.hpp"
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <fstream>
#include <cstring>
#include <algorithm>

struct AuthKey {
    std::string id;
    std::string name;
    std::string key;
    std::string user_id;
    int expiry_days;
    std::string created_at;
    std::string expires_at;
    std::string format;
    std::string version;
};

struct TransferRecord {
    std::string id;
    std::string user_id;
    int file_count;
    long total_size;
    std::string created_at;
    std::string status;
};

class AuthHandler {
public:
    AuthHandler(const std::string& db_path, const JWTManager& jwt_manager);
    ~AuthHandler();
    
    // HTTP request handlers
    HttpResponse handle_login(const HttpRequest& request);
    HttpResponse handle_logout(const HttpRequest& request);
    HttpResponse handle_register(const HttpRequest& request);
    HttpResponse handle_refresh_token(const HttpRequest& request);
    HttpResponse handle_get_user_info(const HttpRequest& request);
    HttpResponse handle_change_password(const HttpRequest& request);
    HttpResponse handle_generate_auth_key(const HttpRequest& request);
    HttpResponse handle_list_auth_keys(const HttpRequest& request);
    HttpResponse handle_revoke_auth_key(const HttpRequest& request);
    HttpResponse handle_upload_files(const HttpRequest& request);
    HttpResponse handle_get_transfer_history(const HttpRequest& request);
    
    // Additional HTTP handlers
    HttpResponse handle_verify_token(const HttpRequest& request);
    HttpResponse handle_login_with_key(const HttpRequest& request);
    HttpResponse handle_validate_token(const HttpRequest& request);
    
    // Authentication
    bool authenticate_request(const HttpRequest& request, UserInfo& user_info);
    
    // Public initialization methods
    bool validate_database_integrity();
    static std::string generate_jwt_secret();
    static bool update_config_jwt_secret(const std::string& config_path, const std::string& new_secret);

private:
    std::string db_path_;
    std::unique_ptr<JWTManager> jwt_manager_;
    sqlite3* db_;
    
    // Database operations
    bool init_database();
    bool create_user_tables();
    bool create_auth_tables();
    void log_database_statistics();
    bool cleanup_non_admin_data();
    bool ensure_database_exists();
    bool create_database_with_defaults();
    
    // Data encryption
    std::string encrypt_sensitive_data(const std::string& data);
    std::string decrypt_sensitive_data(const std::string& encrypted_data);
    
    // Utility methods
    std::string generate_uuid();
    std::string get_current_timestamp();
    std::string calculate_expiry_timestamp(int days_from_now);
    std::string hash_password(const std::string& password);
    std::string generate_key_checksum(const std::string& key);
    
    // User management (internal)
    bool user_exists(const std::string& username);
    bool verify_user_credentials(const std::string& username, const std::string& password);
    UserInfo get_user_info(const std::string& username);
    bool create_user(const std::string& username, const std::string& password, const std::string& email, const std::string& role);
    bool change_user_password(const std::string& username, const std::string& old_password, const std::string& new_password);
    
    // Auth key management (internal)
    std::string generate_secure_key();
    bool store_auth_key(const AuthKey& key);
    std::vector<AuthKey> get_user_auth_keys(const std::string& user_id);
    bool revoke_auth_key(const std::string& key_id, const std::string& user_id);
    AuthKey validate_auth_key(const std::string& key);
    
    // Encryption utilities
    std::string base64_encode(const std::string& data);
    std::string base64_decode(const std::string& encoded);
    
    // File operations
    std::string save_uploaded_file(const std::string& filename, const std::string& content, const std::string& user_id);
    std::vector<std::string> get_user_files(const std::string& user_id);
    
    // Transfer history
    bool add_transfer_record(const std::string& user_id, int file_count, long total_size);
    std::vector<TransferRecord> get_user_transfer_history(const std::string& user_id);
    
    // Error handling
    HttpResponse create_auth_response(bool success, const std::string& message, const std::string& token = "", const UserInfo& user_info = UserInfo());
    HttpResponse create_error_response(int status_code, const std::string& message);
};
