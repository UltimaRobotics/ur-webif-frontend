#include "database_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>

DatabaseManager::~DatabaseManager() {
    shutdown();
}

bool DatabaseManager::initialize(const ConfigLoader::DatabaseConfig& config) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    if (db_ != nullptr) {
        logError("Database already initialized");
        return false;
    }
    
    config_ = config;
    
    if (!config_.enabled) {
        std::cout << "[DatabaseManager] Database disabled in configuration" << std::endl;
        return true;
    }
    
    // Create database directory if it doesn't exist
    size_t last_slash = config_.path.find_last_of('/');
    if (last_slash != std::string::npos) {
        std::string db_dir = config_.path.substr(0, last_slash);
        std::string mkdir_cmd = "mkdir -p " + db_dir;
        if (system(mkdir_cmd.c_str()) != 0) {
            logError("Failed to create database directory: " + db_dir);
            return false;
        }
    }
    
    // Check if database file exists
    bool db_exists = std::ifstream(config_.path).good();
    
    // Open database
    int result = sqlite3_open(config_.path.c_str(), &db_);
    if (result != SQLITE_OK) {
        logError("Failed to open database: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    
    // Enable foreign keys
    executeSQL("PRAGMA foreign_keys = ON");
    
    // Create tables if database is new
    if (!db_exists) {
        std::cout << "[DatabaseManager] Creating new database: " << config_.path << std::endl;
        if (!createTables()) {
            logError("Failed to create database tables");
            sqlite3_close(db_);
            db_ = nullptr;
            return false;
        }
    } else {
        std::cout << "[DatabaseManager] Using existing database: " << config_.path << std::endl;
        // Verify existing database has required tables
        if (!verifyDatabaseSchema()) {
            logError("Database schema verification failed");
            sqlite3_close(db_);
            db_ = nullptr;
            return false;
        }
    }
    
    // Test database operations
    if (!testDatabaseOperations()) {
        logError("Database operations test failed");
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    
    std::cout << "[DatabaseManager] Database initialized successfully" << std::endl;
    return true;
}

void DatabaseManager::shutdown() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
        std::cout << "[DatabaseManager] Database connection closed" << std::endl;
    }
}

bool DatabaseManager::logConnection(const std::string& connection_id, const std::string& client_ip, const std::string& status) {
    if (!config_.enabled || !config_.log_connections || db_ == nullptr) {
        return true; // Silently ignore if disabled
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "INSERT INTO connections_log (connection_id, client_ip, status, connected_at) VALUES (?, ?, ?, ?)";
    std::vector<std::string> params = {
        connection_id,
        client_ip,
        status,
        getCurrentTimestamp()
    };
    
    return executeSQLWithParams(sql, params);
}

bool DatabaseManager::logDisconnection(const std::string& connection_id) {
    if (!config_.enabled || !config_.log_connections || db_ == nullptr) {
        return true; // Silently ignore if disabled
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "UPDATE connections_log SET disconnected_at = ?, status = 'disconnected' WHERE connection_id = ? AND disconnected_at IS NULL";
    std::vector<std::string> params = {
        getCurrentTimestamp(),
        connection_id
    };
    
    return executeSQLWithParams(sql, params);
}

bool DatabaseManager::logMessage(const std::string& connection_id, const std::string& direction, const std::string& message_text) {
    if (!config_.enabled || !config_.log_messages || db_ == nullptr) {
        return true; // Silently ignore if disabled
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "INSERT INTO messages (connection_id, direction, message_text, timestamp) VALUES (?, ?, ?, ?)";
    std::vector<std::string> params = {
        connection_id,
        direction,
        message_text,
        getCurrentTimestamp()
    };
    
    return executeSQLWithParams(sql, params);
}

int DatabaseManager::getActiveConnectionCount() const {
    if (!config_.enabled || db_ == nullptr) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "SELECT COUNT(*) FROM connections_log WHERE status = 'connected' AND disconnected_at IS NULL";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        logError("Failed to prepare statement for active connection count");
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

std::vector<std::string> DatabaseManager::getRecentConnections(int limit) const {
    std::vector<std::string> connections;
    
    if (!config_.enabled || db_ == nullptr) {
        return connections;
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "SELECT connection_id, client_ip, status, connected_at FROM connections_log ORDER BY connected_at DESC LIMIT ?";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        logError("Failed to prepare statement for recent connections");
        return connections;
    }
    
    sqlite3_bind_int(stmt, 1, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::ostringstream oss;
        oss << "ID: " << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))
            << ", IP: " << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))
            << ", Status: " << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))
            << ", Connected: " << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        connections.push_back(oss.str());
    }
    
    sqlite3_finalize(stmt);
    return connections;
}

bool DatabaseManager::createDatabase() {
    // This is handled in initialize() now
    return true;
}

bool DatabaseManager::createTables() {
    std::vector<std::string> tables = {
        // Connections log table
        "CREATE TABLE IF NOT EXISTS connections_log ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "connection_id TEXT NOT NULL,"
        "client_ip TEXT NOT NULL,"
        "status TEXT NOT NULL DEFAULT 'connected',"
        "connected_at TEXT NOT NULL,"
        "disconnected_at TEXT,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",
        
        // Messages table
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "connection_id TEXT NOT NULL,"
        "direction TEXT NOT NULL," // 'in' or 'out'
        "message_text TEXT,"
        "timestamp TEXT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY (connection_id) REFERENCES connections_log(connection_id)"
        ")",
        
        // Dashboard data table
        "CREATE TABLE IF NOT EXISTS dashboard_data ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "category TEXT NOT NULL UNIQUE,"
        "data_json TEXT NOT NULL,"
        "updated_at TEXT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",
        
        // Indexes for better performance
        "CREATE INDEX IF NOT EXISTS idx_connections_connection_id ON connections_log(connection_id)",
        "CREATE INDEX IF NOT EXISTS idx_connections_status ON connections_log(status)",
        "CREATE INDEX IF NOT EXISTS idx_connections_connected_at ON connections_log(connected_at)",
        "CREATE INDEX IF NOT EXISTS idx_messages_connection_id ON messages(connection_id)",
        "CREATE INDEX IF NOT EXISTS idx_messages_timestamp ON messages(timestamp)",
        "CREATE INDEX IF NOT EXISTS idx_dashboard_data_category ON dashboard_data(category)",
        "CREATE INDEX IF NOT EXISTS idx_dashboard_data_updated_at ON dashboard_data(updated_at)"
    };
    
    for (const auto& sql : tables) {
        if (!executeSQL(sql)) {
            return false;
        }
    }
    
    return true;
}

std::string DatabaseManager::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm = *std::localtime(&time_t);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

bool DatabaseManager::executeSQL(const std::string& sql) {
    char* error_msg = nullptr;
    int result = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);
    
    if (result != SQLITE_OK) {
        logError("SQL error: " + std::string(error_msg ? error_msg : "unknown error"));
        sqlite3_free(error_msg);
        return false;
    }
    
    return true;
}

bool DatabaseManager::executeSQLWithParams(const std::string& sql, const std::vector<std::string>& params) {
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        logError("Failed to prepare statement: " + sql);
        return false;
    }
    
    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        if (sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            logError("Failed to bind parameter " + std::to_string(i + 1));
            sqlite3_finalize(stmt);
            return false;
        }
    }
    
    // Execute statement
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (result == SQLITE_DONE || result == SQLITE_ROW);
}

void DatabaseManager::logError(const std::string& message) const {
    std::cerr << "[DatabaseManager] ERROR: " << message << std::endl;
}

bool DatabaseManager::updateDashboardData(const std::string& category, const std::string& data_json) {
    if (!config_.enabled || db_ == nullptr) {
        return true; // Silently ignore if disabled
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "INSERT OR REPLACE INTO dashboard_data (category, data_json, updated_at) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    
    int result = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        logError("Failed to prepare statement: " + sql + " - SQLite error: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    
    // Bind parameters
    if (sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        logError("Failed to bind category parameter: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }
    
    if (sqlite3_bind_text(stmt, 2, data_json.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        logError("Failed to bind data_json parameter: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }
    
    std::string timestamp = getCurrentTimestamp();
    if (sqlite3_bind_text(stmt, 3, timestamp.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        logError("Failed to bind timestamp parameter: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }
    
    // Execute statement
    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (result != SQLITE_DONE) {
        logError("Failed to execute statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    
    return true;
}

std::string DatabaseManager::getDashboardData(const std::string& category) const {
    if (!config_.enabled || db_ == nullptr) {
        return "{}";
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "SELECT data_json FROM dashboard_data WHERE category = ?";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        logError("Failed to prepare statement for dashboard data query");
        return "{}";
    }
    
    sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_TRANSIENT);
    
    std::string result = "{}";
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (data) {
            result = data;
        }
    }
    
    sqlite3_finalize(stmt);
    return result;
}

bool DatabaseManager::initializeDashboardTables() {
    // This is now handled in createTables()
    return true;
}

bool DatabaseManager::verifyDatabaseSchema() {
    if (!db_) {
        logError("Database not initialized for schema verification");
        return false;
    }
    
    // Check if required tables exist
    std::vector<std::string> required_tables = {
        "connections_log",
        "messages", 
        "dashboard_data"
    };
    
    for (const auto& table_name : required_tables) {
        std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            logError("Failed to prepare schema verification statement for table: " + table_name);
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, table_name.c_str(), -1, SQLITE_TRANSIENT);
        
        bool table_exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            table_exists = true;
        }
        
        sqlite3_finalize(stmt);
        
        if (!table_exists) {
            logError("Required table missing: " + table_name);
            
            // Try to create missing table
            std::cout << "[DatabaseManager] Attempting to create missing table: " << table_name << std::endl;
            if (!createTables()) {
                logError("Failed to create missing table: " + table_name);
                return false;
            }
        }
    }
    
    std::cout << "[DatabaseManager] Database schema verification passed" << std::endl;
    return true;
}

bool DatabaseManager::testDatabaseOperations() {
    if (!db_) {
        logError("Database not initialized for operations test");
        return false;
    }
    
    // Test basic INSERT operation on dashboard_data table
    std::string test_sql = "INSERT OR REPLACE INTO dashboard_data (category, data_json, updated_at) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db_, test_sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        logError("Database operations test failed - cannot prepare test statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    
    // Bind test parameters
    if (sqlite3_bind_text(stmt, 1, "test", -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        logError("Database operations test failed - cannot bind test category: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }
    
    if (sqlite3_bind_text(stmt, 2, "{\"test\": true}", -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        logError("Database operations test failed - cannot bind test data: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }
    
    if (sqlite3_bind_text(stmt, 3, "2025-01-01 00:00:00.000", -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        logError("Database operations test failed - cannot bind test timestamp: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }
    
    // Execute test
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (result != SQLITE_DONE) {
        logError("Database operations test failed - cannot execute test statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    
    // Clean up test data
    std::string cleanup_sql = "DELETE FROM dashboard_data WHERE category = 'test'";
    executeSQL(cleanup_sql);
    
    std::cout << "[DatabaseManager] Database operations test passed" << std::endl;
    return true;
}
