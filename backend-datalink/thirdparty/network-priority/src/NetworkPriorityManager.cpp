#include "NetworkPriorityManager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <iomanip>
#include <regex>

NetworkPriorityManager::NetworkPriorityManager() 
    : thread_id_(0), running_(false), poll_interval_seconds_(5) {
    
    // Initialize data with default values
    statistics_ = NetworkStatistics{};
    
    try {
        // Initialize thread manager
        thread_manager_ = std::make_unique<ThreadMgr::ThreadManager>(10);
        
        // Database manager will be set later
        db_manager_ = nullptr;
        
        log("Network Priority Manager initialized successfully (without database)");
        
    } catch (const std::exception& e) {
        log("Failed to initialize Network Priority Manager: " + std::string(e.what()));
        throw;
    }
}

NetworkPriorityManager::NetworkPriorityManager(DatabaseManager* db_manager) 
    : thread_id_(0), running_(false), poll_interval_seconds_(5), db_manager_(db_manager) {
    
    std::cerr << "[CRITICAL] NetworkPriorityManager constructor called with db_manager: " 
              << (db_manager ? "VALID" : "NULL") << std::endl;
    
    // Initialize data with default values
    statistics_ = NetworkStatistics{};
    
    try {
        // Initialize thread manager
        thread_manager_ = std::make_unique<ThreadMgr::ThreadManager>(10);
        
        log("Network Priority Manager initialized successfully with database");
        std::cerr << "[CRITICAL] NetworkPriorityManager thread manager initialized successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[CRITICAL] NetworkPriorityManager constructor failed: " << e.what() << std::endl;
        log("Failed to initialize Network Priority Manager: " + std::string(e.what()));
        throw;
    }
}

NetworkPriorityManager::~NetworkPriorityManager() {
    stop();
}

bool NetworkPriorityManager::start(int poll_interval_seconds) {
    if (running_.load()) {
        log("Network Priority Manager is already running");
        return false;
    }
    
    poll_interval_seconds_ = poll_interval_seconds;
    
    try {
        // Initialize database tables
        if (!initializeDatabaseTables()) {
            log("Failed to initialize database tables");
            return false;
        }
        
        // Load existing configuration
        loadConfigurationFromDatabase();
        
        // Create the collection thread using ur-threadder-api
        std::function<void()> thread_func = [this]() {
            collectionLoop();
        };
        
        thread_id_ = thread_manager_->createThread(thread_func);
        running_.store(true);
        
        log("Network Priority Manager started with thread ID: " + std::to_string(thread_id_));
        return true;
        
    } catch (const std::exception& e) {
        log("Failed to start Network Priority Manager: " + std::string(e.what()));
        running_.store(false);
        return false;
    }
}

void NetworkPriorityManager::stop() {
    if (!running_.load()) {
        return;
    }
    
    try {
        log("Stopping Network Priority Manager...");
        
        running_.store(false);
        
        // Stop the managed thread
        if (thread_manager_ && thread_id_ > 0) {
            thread_manager_->stopThread(thread_id_);
            thread_manager_->joinThread(thread_id_);
        }
        
        // Save final configuration
        saveConfigurationToDatabase();
        
        thread_id_ = 0;
        log("Network Priority Manager stopped successfully");
        
    } catch (const std::exception& e) {
        log("Error stopping Network Priority Manager: " + std::string(e.what()));
    }
}

bool NetworkPriorityManager::pause() {
    if (!running_.load() || !thread_manager_ || thread_id_ == 0) {
        return false;
    }
    
    try {
        thread_manager_->pauseThread(thread_id_);
        log("Network Priority Manager paused");
        return true;
    } catch (const std::exception& e) {
        log("Error pausing Network Priority Manager: " + std::string(e.what()));
        return false;
    }
}

bool NetworkPriorityManager::resume() {
    if (!running_.load() || !thread_manager_ || thread_id_ == 0) {
        return false;
    }
    
    try {
        thread_manager_->resumeThread(thread_id_);
        log("Network Priority Manager resumed");
        return true;
    } catch (const std::exception& e) {
        log("Error resuming Network Priority Manager: " + std::string(e.what()));
        return false;
    }
}

bool NetworkPriorityManager::restart() {
    if (!running_.load()) {
        return false;
    }
    
    try {
        log("Restarting Network Priority Manager...");
        
        // Stop current thread
        if (thread_manager_ && thread_id_ > 0) {
            thread_manager_->stopThread(thread_id_);
            thread_manager_->joinThread(thread_id_);
        }
        
        // Create new thread
        std::function<void()> thread_func = [this]() {
            collectionLoop();
        };
        
        thread_id_ = thread_manager_->createThread(thread_func);
        log("Network Priority Manager restarted successfully with thread ID: " + std::to_string(thread_id_));
        return true;
        
    } catch (const std::exception& e) {
        log("Error restarting Network Priority Manager: " + std::string(e.what()));
        return false;
    }
}

ThreadMgr::ThreadState NetworkPriorityManager::getState() const {
    if (!running_.load() || !thread_manager_ || thread_id_ == 0) {
        return ThreadMgr::ThreadState::Stopped;
    }
    
    try {
        return thread_manager_->getThreadState(thread_id_);
    } catch (const std::exception& e) {
        log("Error getting thread state: " + std::string(e.what()));
        return ThreadMgr::ThreadState::Error;
    }
}

std::vector<NetworkInterface> NetworkPriorityManager::getNetworkInterfaces() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return network_interfaces_;
}

std::vector<RoutingRule> NetworkPriorityManager::getRoutingRules() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return routing_rules_;
}

NetworkStatistics NetworkPriorityManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return statistics_;
}

nlohmann::json NetworkPriorityManager::getAllDataAsJson() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    nlohmann::json interfaces_json = nlohmann::json::array();
    for (const auto& interface : network_interfaces_) {
        interfaces_json.push_back(interfaceToJson(interface));
    }
    
    nlohmann::json rules_json = nlohmann::json::array();
    for (const auto& rule : routing_rules_) {
        rules_json.push_back(ruleToJson(rule));
    }
    
    return {
        {"networkInterfaces", interfaces_json},
        {"routingRules", rules_json},
        {"statistics", statisticsToJson(statistics_)},
        {"lastUpdated", getCurrentTimestamp()}
    };
}

bool NetworkPriorityManager::setInterfacePriority(const std::string& interface_name, int priority) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    // Find interface by name
    auto it = std::find_if(network_interfaces_.begin(), network_interfaces_.end(),
                          [&interface_name](const NetworkInterface& iface) {
                              return iface.name == interface_name;
                          });
    
    if (it != network_interfaces_.end()) {
        it->priority = priority;
        
        // Apply the priority change to system
        if (applyInterfaceMetrics()) {
            saveInterfacesToDatabase();
            pushDataToFrontend();
            log("Set priority for interface " + interface_name + " to " + std::to_string(priority));
            return true;
        }
    }
    
    log("Failed to set priority for interface: " + interface_name);
    return false;
}

bool NetworkPriorityManager::addRoutingRule(const RoutingRule& rule) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    // Validate rule
    if (rule.destination.empty() || rule.gateway.empty() || rule.interface.empty()) {
        log("Invalid routing rule: missing required fields");
        return false;
    }
    
    // Add ID if not present
    RoutingRule new_rule = rule;
    if (new_rule.id.empty()) {
        new_rule.id = generateId();
    }
    new_rule.status = "Active";
    new_rule.type = "static";
    new_rule.table = "main";
    
    routing_rules_.push_back(new_rule);
    
    // Apply to system
    if (applyRoutingRules()) {
        saveRulesToDatabase();
        pushDataToFrontend();
        log("Added routing rule for " + rule.destination);
        return true;
    }
    
    // Rollback on failure
    routing_rules_.pop_back();
    log("Failed to add routing rule for " + rule.destination);
    return false;
}

bool NetworkPriorityManager::updateRoutingRule(const std::string& rule_id, const RoutingRule& rule) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    auto it = std::find_if(routing_rules_.begin(), routing_rules_.end(),
                          [&rule_id](const RoutingRule& r) {
                              return r.id == rule_id;
                          });
    
    if (it != routing_rules_.end()) {
        *it = rule;
        it->id = rule_id; // Ensure ID is preserved
        
        if (applyRoutingRules()) {
            saveRulesToDatabase();
            pushDataToFrontend();
            log("Updated routing rule " + rule_id);
            return true;
        }
    }
    
    log("Failed to update routing rule: " + rule_id);
    return false;
}

bool NetworkPriorityManager::deleteRoutingRule(const std::string& rule_id) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    auto it = std::find_if(routing_rules_.begin(), routing_rules_.end(),
                          [&rule_id](const RoutingRule& r) {
                              return r.id == rule_id;
                          });
    
    if (it != routing_rules_.end()) {
        // Remove from system first
        std::string cmd = generateRouteCommand(*it, false);
        executeCommand(cmd);
        
        routing_rules_.erase(it);
        saveRulesToDatabase();
        pushDataToFrontend();
        log("Deleted routing rule " + rule_id);
        return true;
    }
    
    log("Failed to delete routing rule: " + rule_id);
    return false;
}

bool NetworkPriorityManager::applyRoutingConfiguration() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    bool success = true;
    
    // Apply interface metrics
    if (!applyInterfaceMetrics()) {
        success = false;
    }
    
    // Apply routing rules
    if (!applyRoutingRules()) {
        success = false;
    }
    
    if (success) {
        saveConfigurationToDatabase();
        pushDataToFrontend();
        log("Applied routing configuration successfully");
    } else {
        log("Failed to apply routing configuration");
    }
    
    return success;
}

bool NetworkPriorityManager::resetToDefaults() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    // Clear custom routing rules
    routing_rules_.clear();
    
    // Reset interface priorities to defaults based on metric
    for (auto& interface : network_interfaces_) {
        interface.priority = interface.metric;
    }
    
    // Apply default configuration
    if (applyRoutingConfiguration()) {
        log("Reset to default routing configuration");
        return true;
    }
    
    log("Failed to reset to default configuration");
    return false;
}

bool NetworkPriorityManager::initializeDatabaseTables() {
    if (!db_manager_ || !db_manager_->isInitialized()) {
        log("Database manager not initialized");
        return false;
    }
    
    return createNetworkPriorityTables();
}

bool NetworkPriorityManager::loadConfigurationFromDatabase() {
    if (!db_manager_->isInitialized()) {
        return false;
    }
    
    bool success = true;
    
    if (!loadInterfacesFromDatabase()) {
        success = false;
    }
    
    if (!loadRulesFromDatabase()) {
        success = false;
    }
    
    if (success) {
        log("Loaded configuration from database");
    } else {
        log("Failed to load configuration from database");
    }
    
    return success;
}

bool NetworkPriorityManager::saveConfigurationToDatabase() {
    if (!db_manager_->isInitialized()) {
        return false;
    }
    
    bool success = true;
    
    if (!saveInterfacesToDatabase()) {
        success = false;
    }
    
    if (!saveRulesToDatabase()) {
        success = false;
    }
    
    if (success) {
        log("Saved configuration to database");
    } else {
        log("Failed to save configuration to database");
    }
    
    return success;
}

void NetworkPriorityManager::forceDataCollection() {
    if (running_.load()) {
        collectAllData();
        pushDataToFrontend();
        log("Forced data collection");
    }
}

void NetworkPriorityManager::setPollInterval(int seconds) {
    if (seconds > 0) {
        poll_interval_seconds_ = seconds;
        log("Set poll interval to " + std::to_string(seconds) + " seconds");
    }
}

void NetworkPriorityManager::collectionLoop() {
    int collection_count = 0;
    
    log("Collection loop started - Network Priority Manager is now collecting data");
    
    while (running_.load()) {
        try {
            collectAllData();
            collection_count++;
            
            // Log collection progress every time for debugging
            log("Collection #" + std::to_string(collection_count) + 
                " (Interfaces: " + std::to_string(statistics_.total) + 
                ", Online: " + std::to_string(statistics_.online) + 
                ", Rules: " + std::to_string(routing_rules_.size()) + ")");
            
            // Push data to frontend
            pushDataToFrontend();
            
        } catch (const std::exception& e) {
            log("Error in collection loop: " + std::string(e.what()));
        }
        
        // Sleep for the configured interval
        std::this_thread::sleep_for(std::chrono::seconds(poll_interval_seconds_));
    }
    
    log("Collection loop stopped after " + std::to_string(collection_count) + " collections");
}

void NetworkPriorityManager::collectAllData() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    collectNetworkInterfaces();
    collectRoutingRules();
    updateStatistics();
}

void NetworkPriorityManager::collectNetworkInterfaces() {
    network_interfaces_.clear();
    
    // Get interface list from system using JSON output for robust parsing
    std::string cmd = "ip -j addr show";
    log("Executing command: " + cmd);
    std::string output = executeCommand(cmd);
    
    if (output.empty()) {
        log("Failed to get network interfaces - command returned empty output");
        return;
    }
    
    log("Raw interface command output length: " + std::to_string(output.length()));
    log("Raw JSON output preview: " + output.substr(0, 200) + "...");
    
    try {
        // Parse JSON output
        nlohmann::json interfaces_json = nlohmann::json::parse(output);
        
        for (const auto& iface_json : interfaces_json) {
            try {
                NetworkInterface interface = parseInterfaceFromJson(iface_json);
                if (!interface.name.empty()) {
                    network_interfaces_.push_back(interface);
                    log("Parsed interface: " + interface.name + " (" + interface.ipAddress + ") - " + interface.status);
                }
            } catch (const std::exception& e) {
                log("Error parsing interface JSON entry: " + std::string(e.what()));
            }
        }
        
    } catch (const std::exception& e) {
        log("Error parsing interface JSON: " + std::string(e.what()));
        // Fallback to simple parsing if JSON fails
        log("Attempting fallback to simple interface parsing...");
        collectNetworkInterfacesFallback();
    }
    
    log("Collected " + std::to_string(network_interfaces_.size()) + " network interfaces");
}

void NetworkPriorityManager::collectRoutingRules() {
    routing_rules_.clear();
    
    // Get routing table from system
    std::string cmd = "ip route show";
    log("Executing routing command: " + cmd);
    std::string output = executeCommand(cmd);
    
    if (output.empty()) {
        log("Failed to get routing rules - command returned empty output");
        return;
    }
    
    log("Raw routing command output length: " + std::to_string(output.length()));
    log("Raw routing output preview: " + output.substr(0, 200) + "...");
    
    // Parse routing data
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        try {
            RoutingRule rule = parseRouteFromSystem(line);
            if (!rule.destination.empty()) {
                routing_rules_.push_back(rule);
                log("Parsed route: " + rule.destination + " via " + rule.gateway + " dev " + rule.interface);
            }
        } catch (const std::exception& e) {
            log("Error parsing routing data: " + std::string(e.what()));
        }
    }
    
    log("Collected " + std::to_string(routing_rules_.size()) + " routing rules");
}

void NetworkPriorityManager::updateStatistics() {
    statistics_.total = network_interfaces_.size();
    statistics_.online = 0;
    statistics_.offline = 0;
    statistics_.activeRules = 0;
    
    for (const auto& interface : network_interfaces_) {
        if (interface.status == "online") {
            statistics_.online++;
        } else {
            statistics_.offline++;
        }
    }
    
    for (const auto& rule : routing_rules_) {
        if (rule.status == "Active") {
            statistics_.activeRules++;
        }
    }
    
    statistics_.lastUpdated = getCurrentTimestamp();
}

std::string NetworkPriorityManager::executeCommand(const std::string& command) const {
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    
    if (!pipe) {
        return "";
    }
    
    char buffer[128];
    std::string result;
    
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }
    
    return result;
}

NetworkInterface NetworkPriorityManager::parseInterfaceFromJson(const nlohmann::json& iface_json) const {
    NetworkInterface interface;
    
    try {
        // Extract basic interface information
        interface.id = generateId();
        interface.name = iface_json.value("ifname", "");
        
        // Get interface state and flags
        std::string operstate = iface_json.value("operstate", "DOWN");
        std::vector<std::string> flags = iface_json.value("flags", std::vector<std::string>());
        
        // Determine status
        interface.status = (operstate == "UP" || operstate == "UNKNOWN") ? "online" : "offline";
        
        // Extract IP addresses (use the first valid IPv4 address)
        interface.ipAddress = "";
        if (iface_json.contains("addr_info")) {
            for (const auto& addr : iface_json["addr_info"]) {
                if (addr.value("family", "") == "inet") {
                    interface.ipAddress = addr.value("local", "");
                    break; // Use first IPv4 address
                }
            }
        }
        
        // Set default metric and priority
        interface.metric = 100;
        interface.priority = interface.metric;
        
        // Determine interface type
        if (interface.name.find("eth") != std::string::npos || 
            interface.name.find("en") != std::string::npos) {
            interface.type = "wired";
        } else if (interface.name.find("wlan") != std::string::npos || 
                   interface.name.find("wl") != std::string::npos) {
            interface.type = "wireless";
        } else if (interface.name.find("tun") != std::string::npos || 
                   interface.name.find("vpn") != std::string::npos) {
            interface.type = "vpn";
        } else {
            interface.type = "unknown";
        }
        
        // Get gateway for this interface
        if (!interface.name.empty()) {
            std::string gateway_cmd = "ip route show dev " + interface.name + " | grep default | awk '{print $3}'";
            interface.gateway = executeCommand(gateway_cmd);
            interface.gateway.erase(std::remove_if(interface.gateway.begin(), interface.gateway.end(), ::isspace), interface.gateway.end());
            
            // Check if this is the default route
            std::string default_check = executeCommand("ip route show default | grep " + interface.name);
            interface.isDefault = !default_check.empty();
        }
        
        // Set default speed
        interface.speed = 1000;
        
        log("Successfully parsed JSON interface: " + interface.name + " with IP: " + interface.ipAddress);
        
    } catch (const std::exception& e) {
        log("Error parsing interface JSON: " + std::string(e.what()));
    }
    
    return interface;
}

void NetworkPriorityManager::collectNetworkInterfacesFallback() {
    // Simple fallback using basic ip command
    std::string cmd = "ip link show | grep '^[0-9]:' | awk '{print $2}' | sed 's/://'";
    std::string output = executeCommand(cmd);
    
    if (output.empty()) {
        log("Fallback interface collection failed");
        return;
    }
    
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (!line.empty()) {
            NetworkInterface interface;
            interface.id = generateId();
            interface.name = line;
            interface.status = "unknown"; // Could be enhanced with status checking
            interface.ipAddress = "";
            interface.metric = 100;
            interface.priority = 100;
            interface.type = "unknown";
            interface.gateway = "";
            interface.isDefault = false;
            interface.speed = 1000;
            
            network_interfaces_.push_back(interface);
            log("Fallback parsed interface: " + interface.name);
        }
    }
}

// ... (rest of the code remains the same)
RoutingRule NetworkPriorityManager::parseRouteFromSystem(const std::string& route_data) const {
    RoutingRule rule;
    
    // Parse route data using regex
    std::regex route_regex(R"((default|([^/\s]+/\d+))\s+(via\s+(\S+)\s+)?dev\s+(\S+)(\s+metric\s+(\d+))?)");
    std::smatch match;
    
    if (std::regex_search(route_data, match, route_regex)) {
        rule.id = generateId();
        rule.destination = (match[1].str() == "default") ? "0.0.0.0/0" : match[2].str();
        rule.gateway = match[4].str();
        rule.interface = match[5].str();
        
        // Extract metric
        try {
            rule.metric = match[7].matched ? std::stoi(match[7].str()) : 100;
        } catch (...) {
            rule.metric = 100;
        }
        
        rule.priority = rule.metric;
        rule.status = "Active";
        rule.type = "dynamic";
        rule.table = "main";
    }
    
    return rule;
}

bool NetworkPriorityManager::createNetworkPriorityTables() {
    try {
        // Create network interfaces table
        std::string interfaces_sql = R"(
            CREATE TABLE IF NOT EXISTS network_interfaces (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                ip_address TEXT,
                gateway TEXT,
                netmask TEXT,
                status TEXT,
                metric INTEGER,
                priority INTEGER,
                type TEXT,
                speed INTEGER,
                is_default BOOLEAN,
                last_updated TEXT
            )
        )";
        
        // Create routing rules table
        std::string rules_sql = R"(
            CREATE TABLE IF NOT EXISTS routing_rules (
                id TEXT PRIMARY KEY,
                destination TEXT NOT NULL,
                gateway TEXT NOT NULL,
                interface TEXT NOT NULL,
                metric INTEGER,
                priority INTEGER,
                status TEXT,
                type TEXT,
                table_name TEXT,
                last_updated TEXT
            )
        )";
        
        // Execute table creation (this would need to be implemented in DatabaseManager)
        // For now, return true as placeholder
        log("Created network priority database tables");
        return true;
        
    } catch (const std::exception& e) {
        log("Failed to create database tables: " + std::string(e.what()));
        return false;
    }
}

bool NetworkPriorityManager::saveInterfacesToDatabase() {
    // Placeholder implementation
    // Would need to implement actual database operations
    return true;
}

bool NetworkPriorityManager::saveRulesToDatabase() {
    // Placeholder implementation
    // Would need to implement actual database operations
    return true;
}

bool NetworkPriorityManager::loadInterfacesFromDatabase() {
    // Placeholder implementation
    // Would need to implement actual database operations
    return true;
}

bool NetworkPriorityManager::loadRulesFromDatabase() {
    // Placeholder implementation
    // Would need to implement actual database operations
    return true;
}

bool NetworkPriorityManager::applyInterfaceMetrics() {
    // Placeholder for applying interface metrics to system
    // This would involve using `ip link set` and `ip route change` commands
    return true;
}

bool NetworkPriorityManager::applyRoutingRules() {
    // Placeholder for applying routing rules to system
    // This would involve using `ip route add/del` commands
    return true;
}

std::string NetworkPriorityManager::generateRouteCommand(const RoutingRule& rule, bool add) const {
    std::string cmd = "ip route ";
    cmd += add ? "add" : "del";
    cmd += " " + rule.destination;
    
    if (!rule.gateway.empty()) {
        cmd += " via " + rule.gateway;
    }
    
    cmd += " dev " + rule.interface;
    cmd += " metric " + std::to_string(rule.metric);
    
    return cmd;
}

std::string NetworkPriorityManager::generateId() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return "np_" + std::to_string(timestamp);
}

std::string NetworkPriorityManager::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::tm tm = *std::localtime(&time_t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    return oss.str();
}

void NetworkPriorityManager::log(const std::string& message) const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm = *std::localtime(&time_t);
    
    std::ostringstream oss;
    oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
    oss << "[NetworkPriorityManager] " << message;
    
    std::cout << oss.str() << std::endl;
}

void NetworkPriorityManager::pushDataToFrontend() const {
    if (data_update_handler_) {
        try {
            nlohmann::json data = getAllDataAsJson();
            data_update_handler_(data);
        } catch (const std::exception& e) {
            log("Error pushing data to frontend: " + std::string(e.what()));
        }
    }
}

nlohmann::json NetworkPriorityManager::interfaceToJson(const NetworkInterface& interface) const {
    return {
        {"id", interface.id},
        {"name", interface.name},
        {"ipAddress", interface.ipAddress},
        {"gateway", interface.gateway},
        {"netmask", interface.netmask},
        {"status", interface.status},
        {"metric", interface.metric},
        {"priority", interface.priority},
        {"type", interface.type},
        {"speed", interface.speed},
        {"isDefault", interface.isDefault}
    };
}

nlohmann::json NetworkPriorityManager::ruleToJson(const RoutingRule& rule) const {
    return {
        {"id", rule.id},
        {"destination", rule.destination},
        {"gateway", rule.gateway},
        {"interface", rule.interface},
        {"metric", rule.metric},
        {"priority", rule.priority},
        {"status", rule.status},
        {"type", rule.type},
        {"table", rule.table}
    };
}

nlohmann::json NetworkPriorityManager::statisticsToJson(const NetworkStatistics& stats) const {
    return {
        {"total", stats.total},
        {"online", stats.online},
        {"offline", stats.offline},
        {"activeRules", stats.activeRules},
        {"lastUpdated", stats.lastUpdated}
    };
}
