#include "websocket_server.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>

WebSocketServer::WebSocketServer() : running_(false) {
    try {
        server_.set_access_channels(websocketpp::log::alevel::all);
        server_.clear_access_channels(websocketpp::log::alevel::frame_payload);
        
        server_.init_asio();
        
        server_.set_open_handler([this](websocketpp::connection_hdl hdl) {
            this->onOpen(hdl);
        });
        
        server_.set_close_handler([this](websocketpp::connection_hdl hdl) {
            this->onClose(hdl);
        });
        
        server_.set_message_handler([this](websocketpp::connection_hdl hdl, message_ptr msg) {
            this->onMessage(hdl, msg);
        });
        
        server_.set_fail_handler([this](websocketpp::connection_hdl hdl) {
            this->onError(hdl);
        });
        
    } catch (const std::exception& e) {
        std::cerr << "WebSocket server initialization error: " << e.what() << std::endl;
        throw;
    }
}

WebSocketServer::~WebSocketServer() {
    stop();
}

bool WebSocketServer::start(const ConfigLoader::WebSocketConfig& config) {
    if (running_.load()) {
        log("Server is already running");
        return false;
    }

    config_ = config;
    
    try {
        log("Step 1: Setting reuse address");
        server_.set_reuse_addr(true);
        
        log("Step 2: Setting listen backlog");
        server_.set_listen_backlog(128);
        
        log("Step 3: Creating endpoint for " + config_.host + ":" + std::to_string(config_.port));
        // Bind to specific host and port
        websocketpp::lib::asio::ip::tcp::endpoint endpoint;
        if (config_.host == "0.0.0.0") {
            // Bind to all interfaces
            endpoint = websocketpp::lib::asio::ip::tcp::endpoint(
                websocketpp::lib::asio::ip::tcp::v4(), config_.port);
            log("Step 3a: Created IPv4 any address endpoint");
        } else {
            // Bind to specific host
            log("Step 3b: Parsing host address: " + config_.host);
            websocketpp::lib::asio::ip::address address = 
                websocketpp::lib::asio::ip::address::from_string(config_.host);
            endpoint = websocketpp::lib::asio::ip::tcp::endpoint(address, config_.port);
            log("Step 3c: Created specific host endpoint");
        }
        
        log("Step 4: Starting to listen on endpoint");
        server_.listen(endpoint);
        
        log("Step 5: Starting accept connections");
        server_.start_accept();
        
        log("Step 6: Setting running state to true");
        running_.store(true);
        
        log("Step 7: Starting server thread");
        server_thread_ = std::thread([this]() {
            try {
                log("WebSocket server thread started");
                server_.run();
                log("WebSocket server thread finished");
            } catch (const std::exception& e) {
                log("WebSocket server thread error: " + std::string(e.what()));
                running_.store(false);
            }
        });
        
        log("WebSocket server started on " + config_.host + ":" + std::to_string(config_.port));
        return true;
        
    } catch (const std::exception& e) {
        log("FAILED TO START WEBSOCKET SERVER: " + std::string(e.what()));
        running_.store(false);
        return false;
    }
}

void WebSocketServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    try {
        server_.stop();
        
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        
        log("WebSocket server stopped");
    } catch (const std::exception& e) {
        log("Error stopping WebSocket server: " + std::string(e.what()));
    }
}

void WebSocketServer::onOpen(websocketpp::connection_hdl hdl) {
    auto con = server_.get_con_from_hdl(hdl);
    std::string connection_id = generateConnectionId();
    
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_[hdl] = connection_id;
    }
    
    log("Client connected: " + connection_id + " from " + con->get_remote_endpoint());
    
    if (connection_open_handler_) {
        connection_open_handler_(connection_id);
    }
}

void WebSocketServer::onClose(websocketpp::connection_hdl hdl) {
    std::string connection_id;
    
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(hdl);
        if (it != connections_.end()) {
            connection_id = it->second;
            connections_.erase(it);
        }
    }
    
    if (!connection_id.empty()) {
        log("Client disconnected: " + connection_id);
        
        if (connection_close_handler_) {
            connection_close_handler_(connection_id);
        }
    }
}

void WebSocketServer::onMessage(websocketpp::connection_hdl hdl, message_ptr msg) {
    std::string connection_id;
    
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(hdl);
        if (it != connections_.end()) {
            connection_id = it->second;
        }
    }
    
    if (connection_id.empty()) {
        log("Received message from unknown connection");
        return;
    }
    
    try {
        if (msg->get_opcode() == websocketpp::frame::opcode::text) {
            json message = json::parse(msg->get_payload());
            log("Received message from " + connection_id + ": " + message.dump());
            
            if (message_handler_) {
                message_handler_(connection_id, message);
            }
        } else {
            log("Received binary message from " + connection_id);
        }
    } catch (const json::parse_error& e) {
        log("JSON parse error from " + connection_id + ": " + std::string(e.what()));
        
        json error_response = {
            {"type", "error"},
            {"message", "Invalid JSON format"},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        try {
            server_.send(hdl, error_response.dump(), websocketpp::frame::opcode::text);
        } catch (...) {
            log("Failed to send error response to " + connection_id);
        }
    } catch (const std::exception& e) {
        log("Error handling message from " + connection_id + ": " + std::string(e.what()));
    }
}

void WebSocketServer::onError(websocketpp::connection_hdl hdl) {
    std::string connection_id;
    
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(hdl);
        if (it != connections_.end()) {
            connection_id = it->second;
            connections_.erase(it);
        }
    }
    
    if (!connection_id.empty()) {
        log("Connection error for " + connection_id);
    }
}

void WebSocketServer::broadcast(const json& message) {
    std::string message_str = message.dump();
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    for (auto it = connections_.begin(); it != connections_.end();) {
        try {
            server_.send(it->first, message_str, websocketpp::frame::opcode::text);
            ++it;
        } catch (const std::exception& e) {
            log("Failed to send broadcast message to " + it->second + ": " + std::string(e.what()));
            it = connections_.erase(it);
        }
    }
}

void WebSocketServer::sendToClient(const std::string& connection_id, const json& message) {
    std::string message_str = message.dump();
    websocketpp::connection_hdl target_hdl;
    
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (const auto& pair : connections_) {
            if (pair.second == connection_id) {
                target_hdl = pair.first;
                break;
            }
        }
    }
    
    if (target_hdl.lock()) {
        try {
            server_.send(target_hdl, message_str, websocketpp::frame::opcode::text);
        } catch (const std::exception& e) {
            log("Failed to send message to " + connection_id + ": " + std::string(e.what()));
        }
    } else {
        log("Client not found: " + connection_id);
    }
}

std::string WebSocketServer::generateConnectionId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    return "conn_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

void WebSocketServer::log(const std::string& message) const {
    if (config_.enable_logging) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::tm tm = *std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
        oss << "[WebSocketServer] " << message;
        
        std::cout << oss.str() << std::endl;
    }
}
