#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct ServerConfig {
    std::string host;
    int port;
    int max_connections;
    int thread_pool_size;
    std::vector<std::string> domain_names;
};

struct PathsConfig {
    std::string frontend_root;
    std::string static_files;
    std::string templates;
};

struct AuthConfig {
    std::string jwt_secret;
    int token_expiry_minutes;
    int refresh_token_expiry_minutes;
    std::string issuer;
    std::string audience;
    bool enable_sliding_expiration;
    int token_refresh_threshold_minutes;
    
    // Backward compatibility
    int token_expiry_hours;
    int refresh_token_expiry_days;
};

struct SecurityConfig {
    bool enable_cors;
    std::vector<std::string> allowed_origins;
    std::vector<std::string> allowed_methods;
    std::vector<std::string> allowed_headers;
    int max_file_size_mb;
    int rate_limit_requests_per_minute;
    bool enable_security_headers;
    std::string strict_transport_security;
    std::string content_security_policy;
};

struct LoggingConfig {
    std::string level;
    std::string file;
    bool console;
};

struct DatabaseConfig {
    std::string type;
    std::string path;
};

class ConfigManager {
private:
    std::unique_ptr<json> config_data_;
    
    ServerConfig server_config_;
    PathsConfig paths_config_;
    AuthConfig auth_config_;
    SecurityConfig security_config_;
    LoggingConfig logging_config_;
    DatabaseConfig database_config_;

public:
    ConfigManager();
    ~ConfigManager() = default;
    
    bool load_config(const std::string& config_path);
    
    // Getters
    const ServerConfig& get_server_config() const { return server_config_; }
    const PathsConfig& get_paths_config() const { return paths_config_; }
    const AuthConfig& get_auth_config() const { return auth_config_; }
    const SecurityConfig& get_security_config() const { return security_config_; }
    const LoggingConfig& get_logging_config() const { return logging_config_; }
    const DatabaseConfig& get_database_config() const { return database_config_; }
    
    // Utility methods
    std::string get_config_string(const std::string& path, const std::string& default_value = "") const;
    int get_config_int(const std::string& path, int default_value = 0) const;
    bool get_config_bool(const std::string& path, bool default_value = false) const;
    std::vector<std::string> get_config_array(const std::string& path) const;
};
