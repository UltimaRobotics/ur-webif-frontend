#include <iostream>
#include <signal.h>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>
#include "managed_websocket_server.h"
#include "database_manager.h"
#include "SystemDataCollector.h"
#include "NetworkPriorityManager.h"
#include "config_loader.h"

using json = nlohmann::json;

std::unique_ptr<ManagedWebSocketServer> g_server;
std::unique_ptr<DatabaseManager> g_database;
std::unique_ptr<SystemDataCollector> g_system_collector;
std::unique_ptr<NetworkPriorityManager> g_network_priority_manager;
std::atomic<bool> g_running(true);

// Function declarations
void onMessage(const std::string& connection_id, const json& message);
void onConnectionOpen(const std::string& connection_id);
void onConnectionClose(const std::string& connection_id);
void handleDashboardDataRequest(const std::string& connection_id, const json& message);
void handleSubscribeUpdates(const std::string& connection_id, const json& message);
void handleNetworkPriorityRequest(const std::string& connection_id, const json& message);
void broadcastDashboardUpdate(const std::string& category, const json& data);
void updateSystemDataInDatabase();

void handleNetworkPriorityRequest(const std::string& connection_id, const json& message) {
    if (!g_network_priority_manager) {
        json error_response = {
            {"type", "error"},
            {"message", "Network priority manager not available"},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        if (g_server) {
            g_server->sendToClient(connection_id, error_response);
        }
        return;
    }
    
    try {
        std::string action = message.value("action", "");
        json response_data;
        
        if (action == "get_data") {
            // Return all network priority data
            response_data = g_network_priority_manager->getAllDataAsJson();
        } else if (action == "set_interface_priority") {
            // Set interface priority
            std::string interface_name = message.value("interface_name", "");
            int priority = message.value("priority", 0);
            
            bool success = g_network_priority_manager->setInterfacePriority(interface_name, priority);
            response_data = {
                {"success", success},
                {"message", success ? "Interface priority updated" : "Failed to update interface priority"}
            };
        } else if (action == "add_routing_rule") {
            // Add routing rule
            RoutingRule rule;
            rule.destination = message.value("destination", "");
            rule.gateway = message.value("gateway", "");
            rule.interface = message.value("interface", "");
            rule.metric = message.value("metric", 100);
            rule.priority = message.value("priority", 1);
            
            bool success = g_network_priority_manager->addRoutingRule(rule);
            response_data = {
                {"success", success},
                {"message", success ? "Routing rule added" : "Failed to add routing rule"}
            };
        } else if (action == "update_routing_rule") {
            // Update routing rule
            std::string rule_id = message.value("rule_id", "");
            RoutingRule rule;
            rule.destination = message.value("destination", "");
            rule.gateway = message.value("gateway", "");
            rule.interface = message.value("interface", "");
            rule.metric = message.value("metric", 100);
            rule.priority = message.value("priority", 1);
            
            bool success = g_network_priority_manager->updateRoutingRule(rule_id, rule);
            response_data = {
                {"success", success},
                {"message", success ? "Routing rule updated" : "Failed to update routing rule"}
            };
        } else if (action == "delete_routing_rule") {
            // Delete routing rule
            std::string rule_id = message.value("rule_id", "");
            bool success = g_network_priority_manager->deleteRoutingRule(rule_id);
            response_data = {
                {"success", success},
                {"message", success ? "Routing rule deleted" : "Failed to delete routing rule"}
            };
        } else if (action == "apply_configuration") {
            // Apply all configuration changes
            bool success = g_network_priority_manager->applyRoutingConfiguration();
            response_data = {
                {"success", success},
                {"message", success ? "Configuration applied" : "Failed to apply configuration"}
            };
        } else if (action == "reset_to_defaults") {
            // Reset to default configuration
            bool success = g_network_priority_manager->resetToDefaults();
            response_data = {
                {"success", success},
                {"message", success ? "Reset to defaults" : "Failed to reset to defaults"}
            };
        } else {
            response_data = {
                {"error", "Unknown action: " + action}
            };
        }
        
        json response = {
            {"type", "network_priority_response"},
            {"action", action},
            {"data", response_data},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        if (g_server) {
            g_server->sendToClient(connection_id, response);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error handling network priority request: " << e.what() << std::endl;
        
        json error_response = {
            {"type", "error"},
            {"message", "Failed to process network priority request"},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        if (g_server) {
            g_server->sendToClient(connection_id, error_response);
        }
    }
}

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    g_running.store(false);
    
    if (g_server) {
        g_server->stop();
    }
    
    if (g_network_priority_manager) {
        g_network_priority_manager->stop();
    }
    
    exit(0);
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " -pkg_config <config_file_path>" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -pkg_config <path>    Path to JSON configuration file" << std::endl;
    std::cout << "  -h, --help           Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << " -pkg_config config/config.json" << std::endl;
}

void onMessage(const std::string& connection_id, const json& message) {
    std::cout << "Message handler called for connection " << connection_id << std::endl;
    
    try {
        std::string message_type = message.value("type", "");
        
        if (message_type == "get_dashboard_data") {
            // Handle dashboard data request
            handleDashboardDataRequest(connection_id, message);
        } else if (message_type == "subscribe_updates") {
            // Handle subscription to real-time updates
            handleSubscribeUpdates(connection_id, message);
        } else if (message_type == "network_priority") {
            // Handle network priority requests
            handleNetworkPriorityRequest(connection_id, message);
        } else {
            // Default echo response for unknown message types
            json response = {
                {"type", "echo"},
                {"original", message},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
                {"server", "backend-datalink"}
            };
            
            if (g_server) {
                g_server->sendToClient(connection_id, response);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling message: " << e.what() << std::endl;
        
        json error_response = {
            {"type", "error"},
            {"message", "Failed to process message"},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        if (g_server) {
            g_server->sendToClient(connection_id, error_response);
        }
    }
}

void handleDashboardDataRequest(const std::string& connection_id, const json& message) {
    if (!g_database || !g_database->isInitialized()) {
        json error_response = {
            {"type", "error"},
            {"message", "Database not available"},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        if (g_server) {
            g_server->sendToClient(connection_id, error_response);
        }
        return;
    }
    
    try {
        // Get requested categories or default to all
        std::vector<std::string> categories = {"system", "ram", "swap", "network", "ultima_server", "signal"};
        
        if (message.contains("categories") && message["categories"].is_array()) {
            categories.clear();
            for (const auto& cat : message["categories"]) {
                if (cat.is_string()) {
                    categories.push_back(cat.get<std::string>());
                }
            }
        }
        
        // Retrieve data for each category
        json dashboard_data = json::object();
        
        for (const auto& category : categories) {
            std::string data_json = g_database->getDashboardData(category);
            if (!data_json.empty() && data_json != "{}") {
                try {
                    dashboard_data[category] = json::parse(data_json);
                } catch (const json::parse_error& e) {
                    std::cerr << "Failed to parse JSON for category " << category << ": " << e.what() << std::endl;
                    dashboard_data[category] = json::object();
                }
            } else {
                dashboard_data[category] = json::object();
            }
        }
        
        json response = {
            {"type", "dashboard_data"},
            {"data", dashboard_data},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        if (g_server) {
            g_server->sendToClient(connection_id, response);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error handling dashboard data request: " << e.what() << std::endl;
        
        json error_response = {
            {"type", "error"},
            {"message", "Failed to retrieve dashboard data"},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        if (g_server) {
            g_server->sendToClient(connection_id, error_response);
        }
    }
}

void handleSubscribeUpdates(const std::string& connection_id, const json& message) {
    // For now, just acknowledge subscription
    // Real-time updates will be broadcast to all clients
    json response = {
        {"type", "subscription_confirmed"},
        {"message", "Subscribed to real-time dashboard updates"},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
    
    if (g_server) {
        g_server->sendToClient(connection_id, response);
    }
}

void onConnectionOpen(const std::string& connection_id) {
    std::cout << "Connection opened: " << connection_id << std::endl;
    
    // Log to database
    if (g_database && g_database->isInitialized()) {
        g_database->logConnection(connection_id, "unknown", "connected");
    }
    
    json welcome = {
        {"type", "welcome"},
        {"message", "Connected to backend-datalink WebSocket server"},
        {"connection_id", connection_id},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
    
    if (g_server) {
        g_server->sendToClient(connection_id, welcome);
    }
}

void onConnectionClose(const std::string& connection_id) {
    std::cout << "Connection closed: " << connection_id << std::endl;
    
    // Log to database
    if (g_database && g_database->isInitialized()) {
        g_database->logDisconnection(connection_id);
    }
}

void broadcastDashboardUpdate(const std::string& category, const json& data) {
    if (!g_server) {
        return;
    }
    
    json update_message = {
        {"type", "dashboard_update"},
        {"category", category},
        {"data", data},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
    
    // Broadcast to all connected clients
    g_server->broadcast(update_message);
    std::cout << "[BROADCAST] Sent dashboard update for category: " << category << std::endl;
}

void updateSystemDataInDatabase() {
    if (!g_database || !g_database->isInitialized() || !g_system_collector) {
        return;
    }
    
    try {
        // Get current system metrics as JSON
        json metrics = g_system_collector->getMetricsAsJson();
        
        // Update database with different categories
        g_database->updateDashboardData("system", metrics["cpu"].dump());
        g_database->updateDashboardData("ram", metrics["ram"].dump());
        g_database->updateDashboardData("swap", metrics["swap"].dump());
        g_database->updateDashboardData("network", metrics["network"].dump());
        g_database->updateDashboardData("ultima_server", metrics["ultima_server"].dump());
        g_database->updateDashboardData("signal", metrics["signal"].dump());
        
        // Broadcast real-time updates to connected clients
        broadcastDashboardUpdate("system", metrics["cpu"]);
        broadcastDashboardUpdate("ram", metrics["ram"]);
        broadcastDashboardUpdate("swap", metrics["swap"]);
        broadcastDashboardUpdate("network", metrics["network"]);
        broadcastDashboardUpdate("ultima_server", metrics["ultima_server"]);
        broadcastDashboardUpdate("signal", metrics["signal"]);
        
        // Log successful update (only periodically to avoid spam)
        static int update_count = 0;
        update_count++;
        if (update_count % 6 == 1) { // Log every 30 seconds (6 updates × 5 seconds)
            std::cout << "[SystemDataCollector] Database updated with latest metrics (update #" << update_count << ")" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error updating system data in database: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string config_path;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-pkg_config") == 0) {
            if (i + 1 < argc) {
                config_path = argv[i + 1];
                i++;
            } else {
                std::cerr << "Error: -pkg_config requires a file path" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Error: Unknown argument '" << argv[i] << "'" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    if (config_path.empty()) {
        std::cerr << "Error: -pkg_config argument is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    try {
        ConfigLoader config_loader;
        config_loader.loadFromFile(config_path);
        
        const auto& ws_config = config_loader.getWebSocketConfig();
        
        std::cout << "Starting backend-datalink WebSocket server..." << std::endl;
        std::cout << "Configuration:" << std::endl;
        std::cout << "  Host: " << ws_config.host << std::endl;
        std::cout << "  Port: " << ws_config.port << std::endl;
        std::cout << "  Max connections: " << ws_config.max_connections << std::endl;
        std::cout << "  Timeout: " << ws_config.timeout_ms << "ms" << std::endl;
        std::cout << "  Logging: " << (ws_config.enable_logging ? "enabled" : "disabled") << std::endl;
        std::cout << std::endl;
        
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        // Initialize database
        g_database = std::make_unique<DatabaseManager>();
        if (!g_database->initialize(config_loader.getDatabaseConfig())) {
            std::cerr << "Failed to initialize database" << std::endl;
            return 1;
        }
        
        // Initialize system data collector
        const auto& system_config = config_loader.getSystemDataConfig();
        if (system_config.enabled) {
            g_system_collector = std::make_unique<SystemDataCollector>();
            g_system_collector->setPollInterval(system_config.poll_interval_seconds);
            g_system_collector->setCollectionProgressLogInterval(system_config.collection_progress_log_interval);
            if (!g_system_collector->start(system_config.poll_interval_seconds)) {
                std::cerr << "Failed to start system data collector" << std::endl;
                return 1;
            }
            std::cout << "System data collector started successfully with " 
                     << system_config.poll_interval_seconds << "s interval" << std::endl;
        } else {
            std::cout << "System data collector disabled in configuration" << std::endl;
        }
        
        // Initialize network priority manager
        g_network_priority_manager = std::make_unique<NetworkPriorityManager>(g_database.get());
        
        // Set up data update handler to broadcast via WebSocket
        g_network_priority_manager->setDataUpdateHandler([](const nlohmann::json& data) {
            broadcastDashboardUpdate("network_priority", data);
        });
        
        // Initialize database tables for network priority
        if (!g_network_priority_manager->initializeDatabaseTables()) {
            std::cerr << "Failed to initialize network priority database tables" << std::endl;
            return 1;
        }
        
        // Start network priority manager
        if (!g_network_priority_manager->start(5)) { // 5 second poll interval
            std::cerr << "Failed to start network priority manager" << std::endl;
            return 1;
        }
        
        std::cout << "Network priority manager started successfully" << std::endl;
        
        // Start database update thread for system data
        std::thread db_update_thread([system_config]() {
            int update_count = 0;
            while (g_running.load()) {
                updateSystemDataInDatabase();
                update_count++;
                
                // Log database updates if enabled
                if (system_config.log_database_updates && 
                    update_count % system_config.database_update_log_interval == 1) {
                    std::cout << "[SystemDataCollector] Database updated with latest metrics (update #" 
                             << update_count << ")" << std::endl;
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(system_config.database_update_interval_seconds));
            }
        });
        
        g_server = std::make_unique<ManagedWebSocketServer>();
        g_server->setMessageHandler(onMessage);
        g_server->setConnectionOpenHandler(onConnectionOpen);
        g_server->setConnectionCloseHandler(onConnectionClose);
        
        if (!g_server->start(ws_config)) {
            std::cerr << "Failed to start WebSocket server" << std::endl;
            return 1;
        }
        
        std::cout << "WebSocket server started successfully!" << std::endl;
        std::cout << "Waiting for connections... Press Ctrl+C to stop." << std::endl;
        
        while (g_running.load() && g_server->isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "Shutting down server..." << std::endl;
        // Stop system data collector
        if (g_system_collector) {
            g_system_collector->stop();
        }
        
        // Stop network priority manager
        if (g_network_priority_manager) {
            g_network_priority_manager->stop();
        }
        
        // Wait for database update threads to finish
        if (db_update_thread.joinable()) {
            db_update_thread.join();
        }
        
        // Shutdown database
        if (g_database) {
            g_database->shutdown();
        }
        
        std::cout << "Server stopped. Goodbye!" << std::endl;
        return 0;
    } catch (const ConfigException& e) {
        std::cerr << "Configuration error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
