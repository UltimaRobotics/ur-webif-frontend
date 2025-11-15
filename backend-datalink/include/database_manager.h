#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <sqlite3.h>
#include <mutex>
#include <memory>
#include <chrono>
#include "config_loader.h"

class DatabaseManager {
public:
    DatabaseManager() = default;
    ~DatabaseManager();

    bool initialize(const ConfigLoader::DatabaseConfig& config);
    void shutdown();
    
    bool isInitialized() const { return db_ != nullptr; }
    
    // Connection logging
    bool logConnection(const std::string& connection_id, const std::string& client_ip, 
                      const std::string& status = "connected");
    bool logDisconnection(const std::string& connection_id);
    
    // Message logging (optional, based on config)
    bool logMessage(const std::string& connection_id, const std::string& direction, 
                   const std::string& message_text);
    
    // Query methods
    int getActiveConnectionCount() const;
    std::vector<std::string> getRecentConnections(int limit = 10) const;
    
    // Dashboard data methods
    bool updateDashboardData(const std::string& category, const std::string& data_json);
    std::string getDashboardData(const std::string& category) const;
    bool initializeDashboardTables();
    
    // Database verification methods
    bool verifyDatabaseSchema();
    bool testDatabaseOperations();

private:
    sqlite3* db_ = nullptr;
    ConfigLoader::DatabaseConfig config_;
    mutable std::mutex db_mutex_;
    
    // Database initialization
    bool createDatabase();
    bool createTables();
    
    // Utility methods
    std::string getCurrentTimestamp() const;
    bool executeSQL(const std::string& sql);
    bool executeSQLWithParams(const std::string& sql, const std::vector<std::string>& params);
    
    // Error handling
    void logError(const std::string& message) const;
};

#endif // DATABASE_MANAGER_H
