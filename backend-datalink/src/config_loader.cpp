#include "config_loader.h"
#include <fstream>
#include <iostream>

void ConfigLoader::loadFromFile(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw ConfigException("Could not open config file: " + config_path);
    }
    
    json config;
    try {
        file >> config;
    } catch (const json::parse_error& e) {
        throw ConfigException("Invalid JSON in config file: " + std::string(e.what()));
    }
    
    if (config.contains("websocket")) {
        parseWebSocketConfig(config["websocket"]);
    }
    
    if (config.contains("database")) {
        parseDatabaseConfig(config["database"]);
    }
    
    if (config.contains("system_data")) {
        parseSystemDataConfig(config["system_data"]);
    }
    
    validateConfig();
}

void ConfigLoader::parseDatabaseConfig(const json& db_config) {
    if (db_config.contains("path")) {
        if (!db_config["path"].is_string()) {
            throw ConfigException("database.path must be a string");
        }
        db_config_.path = db_config["path"];
    }
    
    if (db_config.contains("enabled")) {
        if (!db_config["enabled"].is_boolean()) {
            throw ConfigException("database.enabled must be a boolean");
        }
        db_config_.enabled = db_config["enabled"];
    }
    
    if (db_config.contains("log_connections")) {
        if (!db_config["log_connections"].is_boolean()) {
            throw ConfigException("database.log_connections must be a boolean");
        }
        db_config_.log_connections = db_config["log_connections"];
    }
    
    if (db_config.contains("log_messages")) {
        if (!db_config["log_messages"].is_boolean()) {
            throw ConfigException("database.log_messages must be a boolean");
        }
        db_config_.log_messages = db_config["log_messages"];
    }
}

void ConfigLoader::parseSystemDataConfig(const json& system_config) {
    if (system_config.contains("enabled")) {
        if (!system_config["enabled"].is_boolean()) {
            throw ConfigException("system_data.enabled must be a boolean");
        }
        system_data_config_.enabled = system_config["enabled"];
    }
    
    if (system_config.contains("poll_interval_seconds")) {
        if (!system_config["poll_interval_seconds"].is_number_integer() || 
            system_config["poll_interval_seconds"] < 1) {
            throw ConfigException("system_data.poll_interval_seconds must be a positive integer");
        }
        system_data_config_.poll_interval_seconds = system_config["poll_interval_seconds"];
    }
    
    if (system_config.contains("database_update_interval_seconds")) {
        if (!system_config["database_update_interval_seconds"].is_number_integer() || 
            system_config["database_update_interval_seconds"] < 1) {
            throw ConfigException("system_data.database_update_interval_seconds must be a positive integer");
        }
        system_data_config_.database_update_interval_seconds = system_config["database_update_interval_seconds"];
    }
    
    if (system_config.contains("log_collection_progress")) {
        if (!system_config["log_collection_progress"].is_boolean()) {
            throw ConfigException("system_data.log_collection_progress must be a boolean");
        }
        system_data_config_.log_collection_progress = system_config["log_collection_progress"];
    }
    
    if (system_config.contains("log_database_updates")) {
        if (!system_config["log_database_updates"].is_boolean()) {
            throw ConfigException("system_data.log_database_updates must be a boolean");
        }
        system_data_config_.log_database_updates = system_config["log_database_updates"];
    }
    
    if (system_config.contains("collection_progress_log_interval")) {
        if (!system_config["collection_progress_log_interval"].is_number_integer() || 
            system_config["collection_progress_log_interval"] < 1) {
            throw ConfigException("system_data.collection_progress_log_interval must be a positive integer");
        }
        system_data_config_.collection_progress_log_interval = system_config["collection_progress_log_interval"];
    }
    
    if (system_config.contains("database_update_log_interval")) {
        if (!system_config["database_update_log_interval"].is_number_integer() || 
            system_config["database_update_log_interval"] < 1) {
            throw ConfigException("system_data.database_update_log_interval must be a positive integer");
        }
        system_data_config_.database_update_log_interval = system_config["database_update_log_interval"];
    }
}

void ConfigLoader::parseWebSocketConfig(const json& ws_config) {
    if (ws_config.contains("host")) {
        if (!ws_config["host"].is_string()) {
            throw ConfigException("websocket.host must be a string");
        }
        ws_config_.host = ws_config["host"];
    }

    if (ws_config.contains("port")) {
        if (!ws_config["port"].is_number_integer()) {
            throw ConfigException("websocket.port must be an integer");
        }
        ws_config_.port = ws_config["port"];
    }

    if (ws_config.contains("max_connections")) {
        if (!ws_config["max_connections"].is_number_integer()) {
            throw ConfigException("websocket.max_connections must be an integer");
        }
        ws_config_.max_connections = ws_config["max_connections"];
    }

    if (ws_config.contains("timeout_ms")) {
        if (!ws_config["timeout_ms"].is_number_integer()) {
            throw ConfigException("websocket.timeout_ms must be an integer");
        }
        ws_config_.timeout_ms = ws_config["timeout_ms"];
    }

    if (ws_config.contains("enable_logging")) {
        if (!ws_config["enable_logging"].is_boolean()) {
            throw ConfigException("websocket.enable_logging must be a boolean");
        }
        ws_config_.enable_logging = ws_config["enable_logging"];
    }
}

void ConfigLoader::validateConfig() const {
    if (ws_config_.port < 1 || ws_config_.port > 65535) {
        throw std::runtime_error("Invalid port number: " + std::to_string(ws_config_.port) + ". Must be between 1 and 65535.");
    }
    
    if (ws_config_.max_connections < 1 || ws_config_.max_connections > 10000) {
        throw std::runtime_error("Invalid max_connections: " + std::to_string(ws_config_.max_connections) + ". Must be between 1 and 10000.");
    }
    
    if (ws_config_.timeout_ms < 100 || ws_config_.timeout_ms > 300000) {
        throw std::runtime_error("Invalid timeout_ms: " + std::to_string(ws_config_.timeout_ms) + ". Must be between 100 and 300000.");
    }

    if (ws_config_.host.empty()) {
        throw ConfigException("websocket.host cannot be empty");
    }
}
