#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <string>
#include <stdexcept>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ConfigLoader {
public:
    struct WebSocketConfig {
        std::string host = "0.0.0.0";
        int port = 9002;
        int max_connections = 100;
        int timeout_ms = 5000;
        bool enable_logging = true;
    };

    struct DatabaseConfig {
        std::string path = "data/runtime-data.db";
        bool enabled = true;
        bool log_connections = true;
        bool log_messages = false;
    };

    struct SystemDataConfig {
        bool enabled = true;
        int poll_interval_seconds = 2;
        int database_update_interval_seconds = 5;
        bool log_collection_progress = true;
        bool log_database_updates = true;
        int collection_progress_log_interval = 30; // Log every N collections
        int database_update_log_interval = 6; // Log every N database updates
    };

    ConfigLoader() = default;
    ~ConfigLoader() = default;

    void loadFromFile(const std::string& config_path);
    
    const WebSocketConfig& getWebSocketConfig() const { return ws_config_; }
    const DatabaseConfig& getDatabaseConfig() const { return db_config_; }
    const SystemDataConfig& getSystemDataConfig() const { return system_data_config_; }

private:
    WebSocketConfig ws_config_;
    DatabaseConfig db_config_;
    SystemDataConfig system_data_config_;
    
    void parseWebSocketConfig(const json& config);
    void parseDatabaseConfig(const json& config);
    void parseSystemDataConfig(const json& config);
    void validateConfig() const;
};

class ConfigException : public std::runtime_error {
public:
    explicit ConfigException(const std::string& message) 
        : std::runtime_error("Config error: " + message) {}
};

#endif // CONFIG_LOADER_H
