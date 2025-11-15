#ifndef NETWORK_PRIORITY_MANAGER_H
#define NETWORK_PRIORITY_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include "nlohmann/json.hpp"
#include "ThreadManager.hpp"
#include "database_manager.h"

// Network Interface data structure matching frontend
struct NetworkInterface {
    std::string id;
    std::string name;
    std::string ipAddress;
    std::string gateway;
    std::string netmask;
    std::string status;        // "online" | "offline"
    int metric;                // Route metric (lower = higher priority)
    int priority;              // User-defined priority order
    std::string type;          // "wired" | "wireless" | "vpn"
    int speed;                 // Interface speed in Mbps
    bool isDefault;            // Whether this is the default route
    
    NetworkInterface() : metric(0), priority(0), speed(0), isDefault(false) {}
};

// Routing Rule data structure matching frontend
struct RoutingRule {
    std::string id;
    std::string destination;   // Destination network in CIDR
    std::string gateway;       // Gateway IP address
    std::string interface;     // Interface name
    int metric;                // Route metric (1-9999)
    int priority;              // Rule priority (1-100, lower = higher)
    std::string status;        // "Active" | "Inactive"
    std::string type;          // "static" | "dynamic" | "default"
    std::string table;         // Routing table
    
    RoutingRule() : metric(0), priority(0) {}
};

// Statistics structure matching frontend
struct NetworkStatistics {
    int total;                 // Total interfaces
    int online;                // Online interfaces
    int offline;               // Offline interfaces
    int activeRules;           // Active routing rules
    std::string lastUpdated;   // Last refresh timestamp
    
    NetworkStatistics() : total(0), online(0), offline(0), activeRules(0) {}
};

class NetworkPriorityManager {
public:
    typedef std::function<void(const nlohmann::json&)> DataUpdateHandler;
    
    NetworkPriorityManager();
    NetworkPriorityManager(DatabaseManager* db_manager);
    ~NetworkPriorityManager();
    
    // Lifecycle management
    bool start(int poll_interval_seconds = 5);
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // Thread management via ur-threadder-api
    bool pause();
    bool resume();
    bool restart();
    ThreadMgr::ThreadState getState() const;
    unsigned int getThreadId() const { return thread_id_; }
    
    // Data access methods
    std::vector<NetworkInterface> getNetworkInterfaces() const;
    std::vector<RoutingRule> getRoutingRules() const;
    NetworkStatistics getStatistics() const;
    nlohmann::json getAllDataAsJson() const;
    
    // Configuration operations (set operations)
    bool setInterfacePriority(const std::string& interface_name, int priority);
    bool addRoutingRule(const RoutingRule& rule);
    bool updateRoutingRule(const std::string& rule_id, const RoutingRule& rule);
    bool deleteRoutingRule(const std::string& rule_id);
    bool applyRoutingConfiguration();
    bool resetToDefaults();
    
    // Database operations
    bool initializeDatabaseTables();
    bool loadConfigurationFromDatabase();
    bool saveConfigurationToDatabase();
    
    // WebSocket data push handler
    void setDataUpdateHandler(DataUpdateHandler handler) { data_update_handler_ = handler; }
    
    // Collection control
    void forceDataCollection();
    void setPollInterval(int seconds);

private:
    // Thread management
    std::unique_ptr<ThreadMgr::ThreadManager> thread_manager_;
    unsigned int thread_id_;
    std::atomic<bool> running_;
    int poll_interval_seconds_;
    
    // Data storage
    mutable std::mutex data_mutex_;
    std::vector<NetworkInterface> network_interfaces_;
    std::vector<RoutingRule> routing_rules_;
    NetworkStatistics statistics_;
    
    // Database
    std::shared_ptr<DatabaseManager> db_manager_;
    
    // Handlers
    DataUpdateHandler data_update_handler_;
    
    // Internal methods
    void collectionLoop();
    void collectAllData();
    void collectNetworkInterfaces();
    void collectRoutingRules();
    void updateStatistics();
    
    // System command execution
    std::string executeCommand(const std::string& command) const;
    std::vector<std::string> parseIpRouteOutput() const;
    std::vector<std::string> parseInterfaceList() const;
    
    // JSON parsing methods
    NetworkInterface parseInterfaceFromJson(const nlohmann::json& iface_json) const;
    void collectNetworkInterfacesFallback();
    
    // Database helpers
    bool createNetworkPriorityTables();
    bool saveInterfacesToDatabase();
    bool saveRulesToDatabase();
    bool loadInterfacesFromDatabase();
    bool loadRulesFromDatabase();
    
    // Routing operations
    bool applyInterfaceMetrics();
    bool applyRoutingRules();
    std::string generateRouteCommand(const RoutingRule& rule, bool add = true) const;
    
    // Utility methods
    std::string generateId() const;
    std::string getCurrentTimestamp() const;
    void log(const std::string& message) const;
    void pushDataToFrontend() const;
    
    // Data conversion
    NetworkInterface parseInterfaceFromSystem(const std::string& interface_data) const;
    RoutingRule parseRouteFromSystem(const std::string& route_data) const;
    nlohmann::json interfaceToJson(const NetworkInterface& interface) const;
    nlohmann::json ruleToJson(const RoutingRule& rule) const;
    nlohmann::json statisticsToJson(const NetworkStatistics& stats) const;
};

#endif // NETWORK_PRIORITY_MANAGER_H
