#include "config_manager.h"
#include <fstream>
#include <iostream>
#include <sstream>

ConfigManager::ConfigManager() : config_data_(std::make_unique<json>()) {}

bool ConfigManager::load_config(const std::string& config_path) {
    try {
        std::ifstream config_file(config_path);
        if (!config_file.is_open()) {
            std::cerr << "Failed to open config file: " << config_path << std::endl;
            return false;
        }
        
        config_file >> *config_data_;
        config_file.close();
        
        // Parse server configuration
        if (config_data_->contains("server")) {
            auto server = (*config_data_)["server"];
            server_config_.host = server.value("host", "0.0.0.0");
            server_config_.port = server.value("port", 9090);
            server_config_.max_connections = server.value("max_connections", 1000);
            server_config_.thread_pool_size = server.value("thread_pool_size", 4);
            
            // Parse domain names
            if (server.contains("domain_names")) {
                server_config_.domain_names = server["domain_names"].get<std::vector<std::string>>();
            } else {
                server_config_.domain_names = {"localhost"};
            }
        }
        
        // Parse paths configuration
        if (config_data_->contains("paths")) {
            auto paths = (*config_data_)["paths"];
            paths_config_.frontend_root = paths.value("frontend_root", "../");
            paths_config_.static_files = paths.value("static_files", "../assets");
            paths_config_.templates = paths.value("templates", "../templates");
        }
        
        // Parse auth configuration with backward compatibility
        if (config_data_->contains("auth")) {
            auto auth = (*config_data_)["auth"];
            auth_config_.jwt_secret = auth.value("jwt_secret", "default-secret-change-this");
            auth_config_.issuer = auth.value("issuer", "frontendpp-auth");
            auth_config_.audience = auth.value("audience", "frontendpp-users");
            
            // New minute-based configuration with backward compatibility
            if (auth.contains("token_expiry_minutes")) {
                auth_config_.token_expiry_minutes = auth.value("token_expiry_minutes", 60);
                auth_config_.token_expiry_hours = auth_config_.token_expiry_minutes / 60;
            } else {
                auth_config_.token_expiry_hours = auth.value("token_expiry_hours", 24);
                auth_config_.token_expiry_minutes = auth_config_.token_expiry_hours * 60;
            }
            
            if (auth.contains("refresh_token_expiry_minutes")) {
                auth_config_.refresh_token_expiry_minutes = auth.value("refresh_token_expiry_minutes", 10080);
                auth_config_.refresh_token_expiry_days = auth_config_.refresh_token_expiry_minutes / (24 * 60);
            } else {
                auth_config_.refresh_token_expiry_days = auth.value("refresh_token_expiry_days", 7);
                auth_config_.refresh_token_expiry_minutes = auth_config_.refresh_token_expiry_days * 24 * 60;
            }
            
            auth_config_.enable_sliding_expiration = auth.value("enable_sliding_expiration", true);
            auth_config_.token_refresh_threshold_minutes = auth.value("token_refresh_threshold_minutes", 10);
        }
        
        // Parse security configuration
        if (config_data_->contains("security")) {
            auto security = (*config_data_)["security"];
            security_config_.enable_cors = security.value("enable_cors", true);
            security_config_.max_file_size_mb = security.value("max_file_size_mb", 100);
            security_config_.rate_limit_requests_per_minute = security.value("rate_limit_requests_per_minute", 60);
            security_config_.enable_security_headers = security.value("enable_security_headers", true);
            security_config_.strict_transport_security = security.value("strict_transport_security", "max-age=31536000; includeSubDomains");
            security_config_.content_security_policy = security.value("content_security_policy", "default-src 'self'");
            
            if (security.contains("allowed_origins")) {
                for (const auto& origin : security["allowed_origins"]) {
                    security_config_.allowed_origins.push_back(origin.get<std::string>());
                }
            }
            
            if (security.contains("allowed_methods")) {
                for (const auto& method : security["allowed_methods"]) {
                    security_config_.allowed_methods.push_back(method.get<std::string>());
                }
            }
            
            if (security.contains("allowed_headers")) {
                for (const auto& header : security["allowed_headers"]) {
                    security_config_.allowed_headers.push_back(header.get<std::string>());
                }
            }
        }
        
        // Parse logging configuration
        if (config_data_->contains("logging")) {
            auto logging = (*config_data_)["logging"];
            logging_config_.level = logging.value("level", "info");
            logging_config_.file = logging.value("file", "logs/frontendpp.log");
            logging_config_.console = logging.value("console", true);
        }
        
        // Parse database configuration
        if (config_data_->contains("database")) {
            auto database = (*config_data_)["database"];
            database_config_.type = database.value("type", "sqlite");
            database_config_.path = database.value("path", "data/auth.db");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing config file: " << e.what() << std::endl;
        return false;
    }
}

std::string ConfigManager::get_config_string(const std::string& path, const std::string& default_value) const {
    try {
        std::istringstream path_stream(path);
        std::string segment;
        json* current = config_data_.get();
        
        while (std::getline(path_stream, segment, '.')) {
            if (current->contains(segment)) {
                current = &(*current)[segment];
            } else {
                return default_value;
            }
        }
        
        return current->get<std::string>();
    } catch (...) {
        return default_value;
    }
}

int ConfigManager::get_config_int(const std::string& path, int default_value) const {
    try {
        std::istringstream path_stream(path);
        std::string segment;
        json* current = config_data_.get();
        
        while (std::getline(path_stream, segment, '.')) {
            if (current->contains(segment)) {
                current = &(*current)[segment];
            } else {
                return default_value;
            }
        }
        
        return current->get<int>();
    } catch (...) {
        return default_value;
    }
}

bool ConfigManager::get_config_bool(const std::string& path, bool default_value) const {
    try {
        std::istringstream path_stream(path);
        std::string segment;
        json* current = config_data_.get();
        
        while (std::getline(path_stream, segment, '.')) {
            if (current->contains(segment)) {
                current = &(*current)[segment];
            } else {
                return default_value;
            }
        }
        
        return current->get<bool>();
    } catch (...) {
        return default_value;
    }
}

std::vector<std::string> ConfigManager::get_config_array(const std::string& path) const {
    std::vector<std::string> result;
    try {
        std::istringstream path_stream(path);
        std::string segment;
        json* current = config_data_.get();
        
        while (std::getline(path_stream, segment, '.')) {
            if (current->contains(segment)) {
                current = &(*current)[segment];
            } else {
                return result;
            }
        }
        
        if (current->is_array()) {
            for (const auto& item : *current) {
                result.push_back(item.get<std::string>());
            }
        }
    } catch (...) {
        // Return empty vector on error
    }
    
    return result;
}
