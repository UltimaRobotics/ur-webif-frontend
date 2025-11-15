#include "auth_handler.h"
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

using json = nlohmann::json;

AuthHandler::AuthHandler(const std::string& db_path, const JWTManager& jwt_manager) 
    : db_path_(db_path), jwt_manager_(std::make_unique<JWTManager>(jwt_manager)), db_(nullptr) {
    
    LOG_INIT_STEP("Initializing AuthHandler", true);
    
    // Enhanced initialization: ensure database exists first
    if (!ensure_database_exists()) {
        LOG_CRITICAL("Failed to ensure database exists");
        return;
    }
    
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        LOG_ERROR("Failed to open database: " + std::string(sqlite3_errmsg(db_)));
        return;
    }
    
    if (!init_database()) {
        LOG_ERROR("Failed to initialize database");
    }
    
    // Enhanced initialization: validate database integrity
    if (!validate_database_integrity()) {
        LOG_ERROR("Database integrity validation failed");
    }
    
    // Log database statistics for monitoring
    log_database_statistics();
    
    // Clean up non-admin data for admin-only setup
    if (!cleanup_non_admin_data()) {
        LOG_WARNING("Failed to cleanup non-admin data");
    }
    
    LOG_INIT_STEP("AuthHandler initialization completed", true);
}

AuthHandler::~AuthHandler() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool AuthHandler::validate_database_integrity() {
    LOG_INIT_STEP("Validating database integrity", true);
    
    // Check if all required tables exist
    const char* check_tables_sql = R"(
        SELECT name FROM sqlite_master 
        WHERE type='table' AND name IN ('users', 'auth_keys', 'transfer_history')
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, check_tables_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare table check query");
        return false;
    }
    
    int table_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        table_count++;
    }
    sqlite3_finalize(stmt);
    
    if (table_count < 3) {
        LOG_ERROR("Missing required database tables. Found: " + std::to_string(table_count) + "/3");
        return false;
    }
    
    // Check admin user exists
    const char* check_admin_sql = "SELECT COUNT(*) FROM users WHERE username = 'admin'";
    if (sqlite3_prepare_v2(db_, check_admin_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare admin check query");
        return false;
    }
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int admin_count = sqlite3_column_int(stmt, 0);
        if (admin_count == 0) {
            LOG_ERROR("Admin user not found in database");
            sqlite3_finalize(stmt);
            return false;
        }
    }
    sqlite3_finalize(stmt);
    
    LOG_INIT_STEP("Database integrity validation passed", true);
    return true;
}

void AuthHandler::log_database_statistics() {
    sqlite3_stmt* stmt;
    
    std::cout << "📊 Database Statistics:" << std::endl;
    
    // Count users
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM users", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int user_count = sqlite3_column_int(stmt, 0);
            std::cout << "   • Total users: " << user_count << std::endl;
        }
        sqlite3_finalize(stmt);
    }
    
    // Count auth keys
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM auth_keys", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int key_count = sqlite3_column_int(stmt, 0);
            std::cout << "   • Auth keys: " << key_count << std::endl;
        }
        sqlite3_finalize(stmt);
    }
    
    // Count transfer history
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM transfer_history", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int transfer_count = sqlite3_column_int(stmt, 0);
            std::cout << "   • Transfer records: " << transfer_count << std::endl;
        }
        sqlite3_finalize(stmt);
    }
    
    // Database file size
    std::ifstream db_file(db_path_, std::ifstream::ate | std::ifstream::binary);
    if (db_file.good()) {
        auto size = db_file.tellg();
        db_file.close();
        std::cout << "   • Database size: " << size << " bytes" << std::endl;
    }
}

bool AuthHandler::cleanup_non_admin_data() {
    char* error_msg = nullptr;
    
    std::cout << "🧹 Cleaning up non-admin data..." << std::endl;
    
    // Remove non-admin users
    const char* cleanup_users_sql = "DELETE FROM users WHERE username != 'admin'";
    if (sqlite3_exec(db_, cleanup_users_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        std::cerr << "Failed to cleanup non-admin users: " << error_msg << std::endl;
        sqlite3_free(error_msg);
        return false;
    }
    
    // Remove orphaned auth keys (keys belonging to non-admin users)
    const char* cleanup_keys_sql = R"(
        DELETE FROM auth_keys 
        WHERE user_id NOT IN (SELECT username FROM users WHERE username = 'admin')
    )";
    if (sqlite3_exec(db_, cleanup_keys_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        std::cerr << "Failed to cleanup orphaned auth keys: " << error_msg << std::endl;
        sqlite3_free(error_msg);
        return false;
    }
    
    // Remove orphaned transfer history
    const char* cleanup_transfers_sql = R"(
        DELETE FROM transfer_history 
        WHERE user_id NOT IN (SELECT username FROM users WHERE username = 'admin')
    )";
    if (sqlite3_exec(db_, cleanup_transfers_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        std::cerr << "Failed to cleanup orphaned transfer history: " << error_msg << std::endl;
        sqlite3_free(error_msg);
        return false;
    }
    
    std::cout << "✅ Non-admin data cleanup completed" << std::endl;
    return true;
}

std::string AuthHandler::encrypt_sensitive_data(const std::string& data) {
    // Enhanced AES-256 encryption for sensitive data
    if (data.empty()) return "";
    
    try {
        // Generate a secure encryption key from JWT secret
        std::string encryption_key = jwt_manager_->get_secret().substr(0, 32); // Use first 32 chars as key
        
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) return "";
        
        // Generate random IV
        unsigned char iv[16];
        if (RAND_bytes(iv, sizeof(iv)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, 
                              (const unsigned char*)encryption_key.c_str(), iv) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        
        unsigned char ciphertext[data.length() + EVP_MAX_BLOCK_LENGTH];
        int len;
        if (EVP_EncryptUpdate(ctx, ciphertext, &len, 
                             (const unsigned char*)data.c_str(), data.length()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        
        int ciphertext_len = len;
        if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        ciphertext_len += len;
        
        EVP_CIPHER_CTX_free(ctx);
        
        // Combine IV and ciphertext, then encode as base64
        std::string combined;
        combined.append((char*)iv, 16);
        combined.append((char*)ciphertext, ciphertext_len);
        
        return base64_encode(combined);
        
    } catch (const std::exception& e) {
        return "";
    }
}

std::string AuthHandler::decrypt_sensitive_data(const std::string& encrypted_data) {
    // Enhanced decryption for sensitive data
    if (encrypted_data.empty()) return "";
    
    try {
        // Generate the same encryption key
        std::string encryption_key = jwt_manager_->get_secret().substr(0, 32);
        
        std::string combined = base64_decode(encrypted_data);
        if (combined.length() < 16) return ""; // Must have at least IV
        
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) return "";
        
        // Extract IV and ciphertext
        unsigned char iv[16];
        std::memcpy(iv, combined.c_str(), 16);
        const unsigned char* ciphertext = (const unsigned char*)combined.c_str() + 16;
        int ciphertext_len = combined.length() - 16;
        
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                              (const unsigned char*)encryption_key.c_str(), iv) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        
        unsigned char plaintext[ciphertext_len + EVP_MAX_BLOCK_LENGTH];
        int len;
        if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        
        int plaintext_len = len;
        if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        plaintext_len += len;
        
        EVP_CIPHER_CTX_free(ctx);
        
        return std::string((char*)plaintext, plaintext_len);
        
    } catch (const std::exception& e) {
        return "";
    }
}

std::string AuthHandler::base64_encode(const std::string& data) {
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    int val = 0, valb = -6;
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) encoded.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (encoded.size() % 4) encoded.push_back('=');
    return encoded;
}

std::string AuthHandler::base64_decode(const std::string& encoded) {
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string decoded;
    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (c == '=') break;
        size_t pos = chars.find(c);
        if (pos == std::string::npos) continue;
        val = (val << 6) + pos;
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return decoded;
}

bool AuthHandler::ensure_database_exists() {
    LOG_INIT_STEP("Checking database existence", true);
    
    namespace fs = std::filesystem;
    
    // Check if database file exists
    if (fs::exists(db_path_)) {
        LOG_INFO("Database file exists: " + db_path_);
        return true;
    }
    
    LOG_INFO("Database file does not exist, creating with defaults");
    
    // Create database directory if it doesn't exist
    fs::path db_dir = fs::path(db_path_).parent_path();
    if (!fs::exists(db_dir)) {
        if (!fs::create_directories(db_dir)) {
            LOG_ERROR("Failed to create database directory: " + db_dir.string());
            return false;
        }
        LOG_INFO("Created database directory: " + db_dir.string());
    }
    
    // Create database with default credentials
    return create_database_with_defaults();
}

bool AuthHandler::create_database_with_defaults() {
    LOG_INIT_STEP("Creating database with default credentials", true);
    
    // Open database connection
    sqlite3* temp_db;
    if (sqlite3_open(db_path_.c_str(), &temp_db) != SQLITE_OK) {
        LOG_ERROR("Failed to create database file: " + std::string(sqlite3_errmsg(temp_db)));
        return false;
    }
    
    // Create tables
    char* error_msg = nullptr;
    
    // Create users table
    const char* users_sql = R"(
        CREATE TABLE users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            email TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            role TEXT NOT NULL DEFAULT 'user',
            full_name TEXT NOT NULL,
            created_at TEXT NOT NULL,
            last_login TEXT,
            auth_method TEXT DEFAULT 'password'
        )
    )";
    
    if (sqlite3_exec(temp_db, users_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        LOG_ERROR("Failed to create users table: " + std::string(error_msg));
        sqlite3_free(error_msg);
        sqlite3_close(temp_db);
        return false;
    }
    
    // Create auth_keys table
    const char* auth_keys_sql = R"(
        CREATE TABLE auth_keys (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            key_value TEXT UNIQUE NOT NULL,
            user_id TEXT NOT NULL,
            expiry_days INTEGER NOT NULL,
            created_at TEXT NOT NULL,
            expires_at TEXT,
            format TEXT DEFAULT 'UACC',
            version TEXT DEFAULT '1.0',
            revoked BOOLEAN DEFAULT FALSE
        )
    )";
    
    if (sqlite3_exec(temp_db, auth_keys_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        LOG_ERROR("Failed to create auth_keys table: " + std::string(error_msg));
        sqlite3_free(error_msg);
        sqlite3_close(temp_db);
        return false;
    }
    
    // Create transfer_history table
    const char* transfer_sql = R"(
        CREATE TABLE transfer_history (
            id TEXT PRIMARY KEY,
            user_id TEXT NOT NULL,
            file_count INTEGER NOT NULL,
            total_size INTEGER NOT NULL,
            created_at TEXT NOT NULL,
            status TEXT DEFAULT 'completed'
        )
    )";
    
    if (sqlite3_exec(temp_db, transfer_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        LOG_ERROR("Failed to create transfer_history table: " + std::string(error_msg));
        sqlite3_free(error_msg);
        sqlite3_close(temp_db);
        return false;
    }
    
    // Insert default admin user using build-time attributes
    const char* insert_admin_sql = R"(
        INSERT INTO users (username, email, password_hash, role, full_name, created_at, auth_method)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(temp_db, insert_admin_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare admin insert statement");
        sqlite3_close(temp_db);
        return false;
    }
    
    std::string current_time = get_current_timestamp();
    
    // Use build-time default credentials
    sqlite3_bind_text(stmt, 1, FrontendPP::BuildAttributes::DEFAULT_ADMIN_USERNAME, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, FrontendPP::BuildAttributes::DEFAULT_ADMIN_EMAIL, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, hash_password(FrontendPP::BuildAttributes::DEFAULT_ADMIN_PASSWORD).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, "administrator", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, "System Administrator", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, current_time.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, "password", -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        LOG_ERROR("Failed to insert admin user: " + std::string(sqlite3_errmsg(temp_db)));
        sqlite3_finalize(stmt);
        sqlite3_close(temp_db);
        return false;
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(temp_db);
    
    LOG_INFO("Created database with admin user: " + std::string(FrontendPP::BuildAttributes::DEFAULT_ADMIN_USERNAME));
    LOG_INFO("Default credentials: " + FrontendPP::BuildAttributes::get_credential_info());
    
    return true;
}

std::string AuthHandler::generate_jwt_secret() {
    LOG_INIT_STEP("Generating JWT secret", true);
    
    // Generate 32 random bytes (256-bit)
    unsigned char random_bytes[32];
    if (RAND_bytes(random_bytes, sizeof(random_bytes)) != 1) {
        LOG_ERROR("Failed to generate random bytes for JWT secret");
        return FrontendPP::BuildAttributes::DEFAULT_JWT_SECRET;
    }
    
    // Convert to hex string
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 32; i++) {
        ss << std::setw(2) << static_cast<unsigned int>(random_bytes[i]);
    }
    
    std::string jwt_secret = ss.str();
    LOG_INFO("Generated new JWT secret (length: " + std::to_string(jwt_secret.length()) + ")");
    
    return jwt_secret;
}

bool AuthHandler::update_config_jwt_secret(const std::string& config_path, const std::string& new_secret) {
    LOG_INIT_STEP("Updating JWT secret in config file", true);
    
    try {
        // Read existing config file
        std::ifstream config_file(config_path);
        if (!config_file.is_open()) {
            LOG_ERROR("Failed to open config file for reading: " + config_path);
            return false;
        }
        
        json config;
        config_file >> config;
        config_file.close();
        
        // Check if JWT secret is the default or invalid
        std::string current_secret = config["auth"]["jwt_secret"];
        bool needs_update = (current_secret == FrontendPP::BuildAttributes::DEFAULT_JWT_SECRET) || 
                           (current_secret.empty()) || 
                           (current_secret.length() < 32);
        
        if (!needs_update) {
            LOG_INFO("JWT secret is valid, no update needed");
            return true;
        }
        
        // Update JWT secret
        config["auth"]["jwt_secret"] = new_secret;
        
        // Write updated config back to file
        std::ofstream output_file(config_path);
        if (!output_file.is_open()) {
            LOG_ERROR("Failed to open config file for writing: " + config_path);
            return false;
        }
        
        output_file << config.dump(4);
        output_file.close();
        
        LOG_INFO("Successfully updated JWT secret in config file: " + config_path);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to update config file: " + std::string(e.what()));
        return false;
    }
}

bool AuthHandler::init_database() {
    char* error_msg = nullptr;
    
    // Create users table
    const char* users_sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            email TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            role TEXT NOT NULL DEFAULT 'user',
            full_name TEXT NOT NULL,
            created_at TEXT NOT NULL,
            last_login TEXT,
            auth_method TEXT DEFAULT 'password'
        )
    )";
    
    if (sqlite3_exec(db_, users_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        std::cerr << "Failed to create users table: " << error_msg << std::endl;
        sqlite3_free(error_msg);
        return false;
    }
    
    // Create auth_keys table
    const char* auth_keys_sql = R"(
        CREATE TABLE IF NOT EXISTS auth_keys (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            key_value TEXT UNIQUE NOT NULL,
            user_id TEXT NOT NULL,
            expiry_days INTEGER NOT NULL,
            created_at TEXT NOT NULL,
            expires_at TEXT,
            format TEXT DEFAULT 'UACC',
            version TEXT DEFAULT '1.0',
            revoked BOOLEAN DEFAULT FALSE
        )
    )";
    
    if (sqlite3_exec(db_, auth_keys_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        std::cerr << "Failed to create auth_keys table: " << error_msg << std::endl;
        sqlite3_free(error_msg);
        return false;
    }
    
    // Create transfer_history table
    const char* transfer_sql = R"(
        CREATE TABLE IF NOT EXISTS transfer_history (
            id TEXT PRIMARY KEY,
            user_id TEXT NOT NULL,
            file_count INTEGER NOT NULL,
            total_size INTEGER NOT NULL,
            created_at TEXT NOT NULL,
            status TEXT DEFAULT 'completed'
        )
    )";
    
    if (sqlite3_exec(db_, transfer_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        std::cerr << "Failed to create transfer_history table: " << error_msg << std::endl;
        sqlite3_free(error_msg);
        return false;
    }
    
    // Insert default admin user only
    const char* insert_users_sql = R"(
        INSERT OR IGNORE INTO users (username, email, password_hash, role, full_name, created_at, auth_method)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, insert_users_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare insert statement: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    
    std::string current_time = get_current_timestamp();
    
    // Bind admin user only
    sqlite3_bind_text(stmt, 1, "admin", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, "admin@frontendpp.local", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, hash_password("admin123").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, "administrator", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, "System Administrator", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, current_time.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, "password", -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to insert admin user: " << sqlite3_errmsg(db_) << std::endl;
    }
    
    sqlite3_finalize(stmt);
    
    // Clean up any non-admin users (admin-only setup)
    const char* cleanup_users_sql = "DELETE FROM users WHERE username != 'admin'";
    if (sqlite3_exec(db_, cleanup_users_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
        std::cerr << "Warning: Failed to cleanup non-admin users: " << error_msg << std::endl;
        sqlite3_free(error_msg);
    }
    
    return true;
}

HttpResponse AuthHandler::handle_login(const HttpRequest& request) {
    std::cerr << "DEBUG: handle_login ENTRY" << std::endl;
    std::cerr << "DEBUG: About to parse JSON, body length: " << request.body.length() << std::endl;
    std::cerr << "DEBUG: Request body content: " << request.body << std::endl;
    try {
        json request_json = json::parse(request.body);
        std::string username = request_json.value("username", "");
        std::string password = request_json.value("password", "");
        
        if (username.empty() || password.empty()) {
            return create_error_response(400, "Username and password are required");
        }
        
        if (!verify_user_credentials(username, password)) {
            return create_error_response(401, "Invalid credentials");
        }
        
        UserInfo user_info = get_user_info(username);
        user_info.last_login = get_current_timestamp();
        
        std::string access_token = jwt_manager_->generate_access_token(user_info);
        std::string refresh_token = jwt_manager_->generate_refresh_token(user_info);
        
        json response_json;
        response_json["success"] = true;
        response_json["message"] = "Login successful";
        response_json["access_token"] = access_token;
        response_json["refresh_token"] = refresh_token;
        response_json["token_type"] = "Bearer";
        response_json["expires_in"] = 24 * 3600; // 24 hours
        response_json["user"] = {
            {"username", user_info.username},
            {"email", user_info.email},
            {"role", user_info.role},
            {"full_name", user_info.full_name},
            {"created_at", user_info.created_at},
            {"last_login", user_info.last_login},
            {"auth_method", user_info.auth_method}
        };
        
        HttpResponse response;
        response.set_json_content(response_json.dump());
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(500, "Internal server error");
    }
}

HttpResponse AuthHandler::handle_login_with_key(const HttpRequest& request) {
    try {
        json request_json = json::parse(request.body);
        std::string key = request_json.value("key", "");
        
        if (key.empty()) {
            return create_error_response(400, "Authentication key is required");
        }
        
        // Validate key format
        if (key.substr(0, 5) != "UACC-" || key.length() < 10) {
            return create_error_response(400, "Invalid authentication key format");
        }
        
        // Validate key against database
        AuthKey auth_key = validate_auth_key(key);
        if (auth_key.id.empty()) {
            return create_error_response(401, "Invalid or expired authentication key");
        }
        
        // Get user info for the key owner
        UserInfo user_info = get_user_info(auth_key.user_id);
        if (user_info.username.empty()) {
            // If user not found, create a key-based user session
            user_info.username = auth_key.user_id;
            user_info.email = auth_key.user_id + "@ur-webif.com";
            user_info.role = "user";
            user_info.full_name = "Key Authenticated User";
            user_info.created_at = auth_key.created_at;
            user_info.auth_method = "key";
        } else {
            user_info.auth_method = "key";
        }
        user_info.last_login = get_current_timestamp();
        
        std::string access_token = jwt_manager_->generate_access_token(user_info);
        std::string refresh_token = jwt_manager_->generate_refresh_token(user_info);
        
        json response_json;
        response_json["success"] = true;
        response_json["message"] = "Login successful with authentication key";
        response_json["access_token"] = access_token;
        response_json["refresh_token"] = refresh_token;
        response_json["token_type"] = "Bearer";
        response_json["expires_in"] = 24 * 3600;
        response_json["user"] = {
            {"username", user_info.username},
            {"email", user_info.email},
            {"role", user_info.role},
            {"full_name", user_info.full_name},
            {"created_at", user_info.created_at},
            {"last_login", user_info.last_login},
            {"auth_method", user_info.auth_method}
        };
        response_json["key_info"] = {
            {"key_id", auth_key.id},
            {"key_name", auth_key.name},
            {"expires_at", auth_key.expires_at}
        };
        
        HttpResponse response;
        response.set_json_content(response_json.dump());
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(500, "Internal server error");
    }
}

HttpResponse AuthHandler::handle_change_password(const HttpRequest& request) {
    try {
        UserInfo user_info;
        if (!authenticate_request(request, user_info)) {
            return create_error_response(401, "Unauthorized");
        }
        
        json request_json = json::parse(request.body);
        std::string current_password = request_json.value("current_password", "");
        std::string new_password = request_json.value("new_password", "");
        
        if (current_password.empty() || new_password.empty()) {
            return create_error_response(400, "Current and new passwords are required");
        }
        
        if (new_password.length() < 8) {
            return create_error_response(400, "Password must be at least 8 characters long");
        }
        
        if (!change_user_password(user_info.username, current_password, new_password)) {
            return create_error_response(400, "Current password is incorrect");
        }
        
        json response_json;
        response_json["success"] = true;
        response_json["message"] = "Password changed successfully";
        
        HttpResponse response;
        response.set_json_content(response_json.dump());
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(500, "Internal server error");
    }
}

HttpResponse AuthHandler::handle_generate_auth_key(const HttpRequest& request) {
    try {
        UserInfo user_info;
        if (!authenticate_request(request, user_info)) {
            std::cerr << "[AUTH_HANDLER] Auth key generation failed: Unauthorized" << std::endl;
            return create_error_response(401, "Unauthorized - Please log in again");
        }
        
        json request_json = json::parse(request.body);
        std::string key_name = request_json.value("name", "");
        int expiry_days = request_json.value("expiry_days", 30);
        
        std::cerr << "[AUTH_HANDLER] Generating auth key for user: " << user_info.username 
                  << ", key_name: " << key_name << ", expiry_days: " << expiry_days << std::endl;
        
        if (key_name.empty()) {
            std::cerr << "[AUTH_HANDLER] Auth key generation failed: Empty key name" << std::endl;
            return create_error_response(400, "Key name is required");
        }
        
        if (expiry_days < 0 || expiry_days > 365) {
            std::cerr << "[AUTH_HANDLER] Auth key generation failed: Invalid expiry days: " << expiry_days << std::endl;
            return create_error_response(400, "Expiry days must be between 0 and 365");
        }
        
        // Generate enhanced secure key
        AuthKey auth_key;
        auth_key.id = generate_uuid();
        auth_key.name = key_name;
        auth_key.key = generate_secure_key();
        auth_key.user_id = user_info.username;
        auth_key.expiry_days = expiry_days;
        auth_key.created_at = get_current_timestamp();
        auth_key.expires_at = expiry_days == 0 ? "" : calculate_expiry_timestamp(expiry_days);
        auth_key.format = "UACC";
        auth_key.version = "1.0";
        
        std::cerr << "[AUTH_HANDLER] Generated auth key with ID: " << auth_key.id << std::endl;
        
        // Store the key in database
        if (!store_auth_key(auth_key)) {
            std::cerr << "[AUTH_HANDLER] Auth key generation failed: Database storage error" << std::endl;
            return create_error_response(500, "Failed to store authentication key in database");
        }
        
        std::cerr << "[AUTH_HANDLER] Auth key stored successfully in database" << std::endl;
        
        // Create downloadable UACC file content
        json uacc_file;
        uacc_file["id"] = auth_key.id;
        uacc_file["name"] = auth_key.name;
        uacc_file["key"] = auth_key.key;
        uacc_file["user_id"] = auth_key.user_id;
        uacc_file["expiry_days"] = auth_key.expiry_days;
        uacc_file["created_at"] = auth_key.created_at;
        uacc_file["expires_at"] = auth_key.expires_at;
        uacc_file["format"] = auth_key.format;
        uacc_file["version"] = auth_key.version;
        uacc_file["generated_by"] = "UR WebIF Frontend++";
        uacc_file["checksum"] = generate_key_checksum(auth_key.key);
        
        json response_json;
        response_json["success"] = true;
        response_json["message"] = "Authentication key generated successfully";
        response_json["data"] = {
            {"id", auth_key.id},
            {"name", auth_key.name},
            {"key", auth_key.key},
            {"user_id", auth_key.user_id},
            {"expiry_days", auth_key.expiry_days},
            {"created_at", auth_key.created_at},
            {"expires_at", auth_key.expires_at},
            {"format", auth_key.format},
            {"version", auth_key.version},
            {"checksum", uacc_file["checksum"]}
        };
        response_json["uacc_file"] = uacc_file;
        
        HttpResponse response;
        response.set_json_content(response_json.dump());
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(500, "Internal server error");
    }
}

HttpResponse AuthHandler::handle_list_auth_keys(const HttpRequest& request) {
    try {
        UserInfo user_info;
        if (!authenticate_request(request, user_info)) {
            return create_error_response(401, "Unauthorized");
        }
        
        std::vector<AuthKey> keys = get_user_auth_keys(user_info.username);
        
        json keys_json = json::array();
        for (const auto& key : keys) {
            keys_json.push_back({
                {"id", key.id},
                {"name", key.name},
                {"created_at", key.created_at},
                {"expires_at", key.expires_at}
            });
        }
        
        json response_json;
        response_json["success"] = true;
        response_json["data"] = keys_json;
        
        HttpResponse response;
        response.set_json_content(response_json.dump());
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(500, "Internal server error");
    }
}

HttpResponse AuthHandler::handle_revoke_auth_key(const HttpRequest& request) {
    try {
        UserInfo user_info;
        if (!authenticate_request(request, user_info)) {
            return create_error_response(401, "Unauthorized");
        }
        
        json request_json = json::parse(request.body);
        std::string key_id = request_json.value("key_id", "");
        
        if (key_id.empty()) {
            return create_error_response(400, "Key ID is required");
        }
        
        if (!revoke_auth_key(key_id, user_info.username)) {
            return create_error_response(400, "Failed to revoke authentication key");
        }
        
        json response_json;
        response_json["success"] = true;
        response_json["message"] = "Authentication key revoked successfully";
        
        HttpResponse response;
        response.set_json_content(response_json.dump());
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(500, "Internal server error");
    }
}

// Utility methods
bool AuthHandler::authenticate_request(const HttpRequest& request, UserInfo& user_info) {
    auto auth_header_it = request.headers.find("Authorization");
    if (auth_header_it == request.headers.end()) {
        return false;
    }
    
    std::string auth_header = auth_header_it->second;
    if (auth_header.substr(0, 7) != "Bearer ") {
        return false;
    }
    
    std::string token = auth_header.substr(7);
    if (!jwt_manager_->validate_token(token)) {
        return false;
    }
    
    user_info = jwt_manager_->extract_user_info(token);
    return !user_info.username.empty();
}

std::string AuthHandler::hash_password(const std::string& password) {
    // Simple SHA-256 hash for demo purposes
    // In production, use bcrypt or Argon2
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_sha256();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    EVP_DigestInit_ex(mdctx, md, nullptr);
    EVP_DigestUpdate(mdctx, password.c_str(), password.length());
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);
    
    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return ss.str();
}

bool AuthHandler::verify_user_credentials(const std::string& username, const std::string& password) {
    sqlite3_stmt* stmt;
    const char* sql = "SELECT password_hash FROM users WHERE username = ?";
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    
    bool result = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* stored_hash = (const char*)sqlite3_column_text(stmt, 0);
        std::string computed_hash = hash_password(password);
        result = (stored_hash && computed_hash == stored_hash);
    }
    
    sqlite3_finalize(stmt);
    return result;
}

UserInfo AuthHandler::get_user_info(const std::string& username) {
    UserInfo user_info;
    sqlite3_stmt* stmt;
    const char* sql = "SELECT username, email, role, full_name, created_at, last_login, auth_method FROM users WHERE username = ?";
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return user_info;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* username_col = (const char*)sqlite3_column_text(stmt, 0);
        const char* email_col = (const char*)sqlite3_column_text(stmt, 1);
        const char* role_col = (const char*)sqlite3_column_text(stmt, 2);
        const char* full_name_col = (const char*)sqlite3_column_text(stmt, 3);
        const char* created_at_col = (const char*)sqlite3_column_text(stmt, 4);
        const char* last_login_col = (const char*)sqlite3_column_text(stmt, 5);
        const char* auth_method_col = (const char*)sqlite3_column_text(stmt, 6);
        
        user_info.username = username_col ? username_col : "";
        user_info.email = email_col ? email_col : "";
        user_info.role = role_col ? role_col : "";
        user_info.full_name = full_name_col ? full_name_col : "";
        user_info.created_at = created_at_col ? created_at_col : "";
        user_info.last_login = last_login_col ? last_login_col : "";
        user_info.auth_method = auth_method_col ? auth_method_col : "";
    }
    
    sqlite3_finalize(stmt);
    return user_info;
}

std::string AuthHandler::generate_secure_key() {
    // Enhanced secure key generation with entropy
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.length() - 1);
    
    std::string key = "UACC-";
    for (int i = 0; i < 32; i++) {
        key += chars[dis(gen)];
    }
    
    // Add checksum for integrity validation
    unsigned int checksum = 0;
    for (char c : key) {
        checksum ^= static_cast<unsigned int>(c);
    }
    
    char checksum_chars[3];
    snprintf(checksum_chars, sizeof(checksum_chars), "%02X", (checksum & 0xFF));
    key += "-";
    key += checksum_chars;
    
    return key;
}

std::string AuthHandler::generate_uuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 32; i++) {
        ss << dis(gen);
        if (i == 7 || i == 11 || i == 15 || i == 19) {
            ss << "-";
        }
    }
    
    return ss.str();
}

std::string AuthHandler::generate_key_checksum(const std::string& key) {
    // Generate SHA-256 checksum for key integrity
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_sha256();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    EVP_DigestInit_ex(mdctx, md, nullptr);
    EVP_DigestUpdate(mdctx, key.c_str(), key.length());
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);
    
    // Return first 8 characters of hash as checksum
    std::stringstream ss;
    for (unsigned int i = 0; i < 4; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return ss.str();
}

std::string AuthHandler::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string AuthHandler::calculate_expiry_timestamp(int days_from_now) {
    auto now = std::chrono::system_clock::now();
    auto expiry = now + std::chrono::hours(24 * days_from_now);
    auto time_t = std::chrono::system_clock::to_time_t(expiry);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

bool AuthHandler::store_auth_key(const AuthKey& key) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO auth_keys (id, name, key_value, user_id, expiry_days, created_at, expires_at, format, version) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, key.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, key.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, key.key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, key.user_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, key.expiry_days);
    sqlite3_bind_text(stmt, 6, key.created_at.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, key.expires_at.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, key.format.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, key.version.c_str(), -1, SQLITE_TRANSIENT);
    
    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

std::vector<AuthKey> AuthHandler::get_user_auth_keys(const std::string& user_id) {
    std::vector<AuthKey> keys;
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, name, created_at, expires_at FROM auth_keys WHERE user_id = ? AND revoked = FALSE";
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return keys;
    }
    
    sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AuthKey key;
        key.id = (const char*)sqlite3_column_text(stmt, 0);
        key.name = (const char*)sqlite3_column_text(stmt, 1);
        key.created_at = (const char*)sqlite3_column_text(stmt, 2);
        key.expires_at = (const char*)sqlite3_column_text(stmt, 3);
        keys.push_back(key);
    }
    
    sqlite3_finalize(stmt);
    return keys;
}

bool AuthHandler::revoke_auth_key(const std::string& key_id, const std::string& user_id) {
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE auth_keys SET revoked = TRUE WHERE id = ? AND user_id = ?";
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, key_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
    
    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

AuthKey AuthHandler::validate_auth_key(const std::string& key) {
    AuthKey auth_key;
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, name, key_value, user_id, expiry_days, created_at, expires_at, format, version FROM auth_keys WHERE key_value = ? AND revoked = FALSE";
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return auth_key;
    }
    
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auth_key.id = (const char*)sqlite3_column_text(stmt, 0);
        auth_key.name = (const char*)sqlite3_column_text(stmt, 1);
        auth_key.key = (const char*)sqlite3_column_text(stmt, 2);
        auth_key.user_id = (const char*)sqlite3_column_text(stmt, 3);
        auth_key.expiry_days = sqlite3_column_int(stmt, 4);
        auth_key.created_at = (const char*)sqlite3_column_text(stmt, 5);
        auth_key.expires_at = (const char*)sqlite3_column_text(stmt, 6);
        auth_key.format = (const char*)sqlite3_column_text(stmt, 7);
        auth_key.version = (const char*)sqlite3_column_text(stmt, 8);
        
        // Check if key has expired
        if (!auth_key.expires_at.empty()) {
            std::string current_time = get_current_timestamp();
            if (current_time > auth_key.expires_at) {
                auth_key.id = ""; // Mark as invalid by clearing ID
            }
        }
    }
    
    sqlite3_finalize(stmt);
    return auth_key;
}

bool AuthHandler::change_user_password(const std::string& username, const std::string& old_password, const std::string& new_password) {
    // First verify old password
    if (!verify_user_credentials(username, old_password)) {
        return false;
    }
    
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE users SET password_hash = ? WHERE username = ?";
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    std::string new_hash = hash_password(new_password);
    sqlite3_bind_text(stmt, 1, new_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    
    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

HttpResponse AuthHandler::create_error_response(int status_code, const std::string& message) {
    HttpResponse response;
    response.status_code = status_code;
    
    json error_json;
    error_json["success"] = false;
    error_json["message"] = message;
    error_json["status_code"] = status_code;
    
    response.set_json_content(error_json.dump());
    return response;
}

HttpResponse AuthHandler::handle_refresh_token(const HttpRequest& request) {
    try {
        json request_json = json::parse(request.body);
        std::string refresh_token = request_json.value("refresh_token", "");
        
        if (refresh_token.empty()) {
            return create_error_response(400, "Refresh token is required");
        }
        
        std::string new_access_token = jwt_manager_->refresh_access_token(refresh_token);
        if (new_access_token.empty()) {
            return create_error_response(401, "Invalid or expired refresh token");
        }
        
        json response_json;
        response_json["success"] = true;
        response_json["message"] = "Token refreshed successfully";
        response_json["access_token"] = new_access_token;
        response_json["token_type"] = "Bearer";
        response_json["expires_in"] = 24 * 3600;
        
        HttpResponse response;
        response.set_json_content(response_json.dump());
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(500, "Internal server error");
    }
}

HttpResponse AuthHandler::handle_logout(const HttpRequest& request) {
    try {
        UserInfo user_info;
        if (!authenticate_request(request, user_info)) {
            return create_error_response(401, "Unauthorized");
        }
        
        json response_json;
        response_json["success"] = true;
        response_json["message"] = "Logout successful";
        
        HttpResponse response;
        response.set_json_content(response_json.dump());
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(500, "Internal server error");
    }
}

HttpResponse AuthHandler::handle_verify_token(const HttpRequest& request) {
    try {
        UserInfo user_info;
        if (!authenticate_request(request, user_info)) {
            return create_error_response(401, "Invalid or expired token");
        }
        
        json response_json;
        response_json["success"] = true;
        response_json["message"] = "Token is valid";
        response_json["user"] = {
            {"username", user_info.username},
            {"email", user_info.email},
            {"role", user_info.role},
            {"full_name", user_info.full_name},
            {"auth_method", user_info.auth_method}
        };
        
        HttpResponse response;
        response.set_json_content(response_json.dump());
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(500, "Internal server error");
    }
}

HttpResponse AuthHandler::handle_get_user_info(const HttpRequest& request) {
    try {
        UserInfo user_info;
        if (!authenticate_request(request, user_info)) {
            return create_error_response(401, "Unauthorized");
        }
        
        json response_json;
        response_json["success"] = true;
        response_json["data"] = {
            {"username", user_info.username},
            {"email", user_info.email},
            {"role", user_info.role},
            {"full_name", user_info.full_name},
            {"created_at", user_info.created_at},
            {"last_login", user_info.last_login},
            {"auth_method", user_info.auth_method}
        };
        
        HttpResponse response;
        response.set_json_content(response_json.dump());
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(500, "Internal server error");
    }
}

HttpResponse AuthHandler::handle_upload_files(const HttpRequest& request) {
    try {
        UserInfo user_info;
        if (!authenticate_request(request, user_info)) {
            return create_error_response(401, "Unauthorized");
        }
        
        // For demo purposes, simulate file processing
        json response_json;
        response_json["success"] = true;
        response_json["message"] = "Files uploaded successfully";
        response_json["data"] = {
            {"files", json::array()},
            {"total_size", 0},
            {"user_id", user_info.username}
        };
        
        HttpResponse response;
        response.set_json_content(response_json.dump());
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(500, "Internal server error");
    }
}

HttpResponse AuthHandler::handle_get_transfer_history(const HttpRequest& request) {
    try {
        UserInfo user_info;
        if (!authenticate_request(request, user_info)) {
            return create_error_response(401, "Unauthorized");
        }
        
        // For demo purposes, return mock transfer history
        json mock_history = json::array();
        mock_history.push_back({
            {"id", "transfer_1"},
            {"file_count", 3},
            {"total_size", 1024000},
            {"created_at", "2025-11-14T10:00:00"},
            {"status", "completed"}
        });
        mock_history.push_back({
            {"id", "transfer_2"},
            {"file_count", 1},
            {"total_size", 512000},
            {"created_at", "2025-11-14T09:30:00"},
            {"status", "completed"}
        });
        
        json response_json;
        response_json["success"] = true;
        response_json["data"] = mock_history;
        
        HttpResponse response;
        response.set_json_content(response_json.dump());
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(500, "Internal server error");
    }
}

HttpResponse AuthHandler::handle_validate_token(const HttpRequest& request) {
    try {
        UserInfo user_info;
        if (!authenticate_request(request, user_info)) {
            return create_error_response(401, "Invalid or expired token");
        }
        
        json response_json;
        response_json["success"] = true;
        response_json["message"] = "Token is valid";
        response_json["user"] = {
            {"username", user_info.username},
            {"email", user_info.email},
            {"role", user_info.role},
            {"full_name", user_info.full_name},
            {"auth_method", user_info.auth_method}
        };
        
        HttpResponse response;
        response.set_json_content(response_json.dump());
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(500, "Internal server error");
    }
}
