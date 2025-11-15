#include "managed_websocket_server.h"
#include <iostream>
#include <sstream>
#include <chrono>

ManagedWebSocketServer::ManagedWebSocketServer() 
    : thread_id_(0), is_running_(false) {
    try {
        // Initialize thread manager
        thread_manager_ = std::make_unique<ThreadMgr::ThreadManager>(10);
        
        // Initialize websocket server
        websocket_server_ = std::make_unique<WebSocketServer>();
        
        // Set up websocket server handlers
        websocket_server_->setMessageHandler([this](const std::string& connection_id, const nlohmann::json& message) {
            if (message_handler_) {
                message_handler_(connection_id, message);
            }
        });
        
        websocket_server_->setConnectionOpenHandler([this](const std::string& connection_id) {
            if (connection_open_handler_) {
                connection_open_handler_(connection_id);
            }
        });
        
        websocket_server_->setConnectionCloseHandler([this](const std::string& connection_id) {
            if (connection_close_handler_) {
                connection_close_handler_(connection_id);
            }
        });
        
        log("Managed WebSocket server initialized successfully");
        
    } catch (const std::exception& e) {
        log("Failed to initialize managed WebSocket server: " + std::string(e.what()));
        throw;
    }
}

ManagedWebSocketServer::~ManagedWebSocketServer() {
    stop();
}

bool ManagedWebSocketServer::start(const ConfigLoader::WebSocketConfig& config) {
    if (is_running_) {
        log("Managed WebSocket server is already running");
        return false;
    }

    config_ = config;
    
    try {
        // Create the websocket server thread using ur-threadder-api
        std::function<void()> thread_func = [this]() {
            websocketServerThread();
        };
        
        // Create thread via thread manager - returns thread ID directly
        thread_id_ = thread_manager_->createThread(thread_func);
        
        is_running_ = true;
        log("Managed WebSocket server started with thread ID: " + std::to_string(thread_id_));
        return true;
        
    } catch (const std::exception& e) {
        log("Failed to start managed WebSocket server: " + std::string(e.what()));
        is_running_ = false;
        return false;
    }
}

void ManagedWebSocketServer::stop() {
    if (!is_running_) {
        return;
    }
    
    try {
        log("Stopping managed WebSocket server...");
        
        // Stop the websocket server first
        if (websocket_server_) {
            websocket_server_->stop();
        }
        
        // Stop the managed thread
        if (thread_manager_ && thread_id_ > 0) {
            thread_manager_->stopThread(thread_id_);
            thread_manager_->joinThread(thread_id_);
        }
        
        is_running_ = false;
        thread_id_ = 0;
        
        log("Managed WebSocket server stopped successfully");
        
    } catch (const std::exception& e) {
        log("Error stopping managed WebSocket server: " + std::string(e.what()));
    }
}

bool ManagedWebSocketServer::pause() {
    if (!is_running_ || !thread_manager_ || thread_id_ == 0) {
        return false;
    }
    
    try {
        thread_manager_->pauseThread(thread_id_);
        log("Managed WebSocket server paused");
        return true;
    } catch (const std::exception& e) {
        log("Error pausing managed WebSocket server: " + std::string(e.what()));
        return false;
    }
}

bool ManagedWebSocketServer::resume() {
    if (!is_running_ || !thread_manager_ || thread_id_ == 0) {
        return false;
    }
    
    try {
        thread_manager_->resumeThread(thread_id_);
        log("Managed WebSocket server resumed");
        return true;
    } catch (const std::exception& e) {
        log("Error resuming managed WebSocket server: " + std::string(e.what()));
        return false;
    }
}

bool ManagedWebSocketServer::restart() {
    if (!is_running_) {
        return false;
    }
    
    try {
        log("Restarting managed WebSocket server...");
        
        // Stop current websocket server
        if (websocket_server_) {
            websocket_server_->stop();
        }
        
        // Stop the managed thread
        if (thread_manager_ && thread_id_ > 0) {
            thread_manager_->stopThread(thread_id_);
            thread_manager_->joinThread(thread_id_);
        }
        
        // Create new thread with same function
        std::function<void()> thread_func = [this]() {
            websocketServerThread();
        };
        
        thread_id_ = thread_manager_->createThread(thread_func);
        log("Managed WebSocket server restarted successfully with thread ID: " + std::to_string(thread_id_));
        return true;
        
    } catch (const std::exception& e) {
        log("Error restarting managed WebSocket server: " + std::string(e.what()));
        return false;
    }
}

ThreadMgr::ThreadState ManagedWebSocketServer::getState() const {
    if (!is_running_ || !thread_manager_ || thread_id_ == 0) {
        return ThreadMgr::ThreadState::Stopped;
    }
    
    try {
        return thread_manager_->getThreadState(thread_id_);
    } catch (const std::exception& e) {
        log("Error getting thread state: " + std::string(e.what()));
        return ThreadMgr::ThreadState::Error;
    }
}

void ManagedWebSocketServer::broadcast(const nlohmann::json& message) {
    if (websocket_server_) {
        websocket_server_->broadcast(message);
    }
}

void ManagedWebSocketServer::sendToClient(const std::string& connection_id, const nlohmann::json& message) {
    if (websocket_server_) {
        websocket_server_->sendToClient(connection_id, message);
    }
}

size_t ManagedWebSocketServer::getConnectionCount() const {
    if (websocket_server_) {
        return websocket_server_->getConnectionCount();
    }
    return 0;
}

void ManagedWebSocketServer::websocketServerThread() {
    try {
        log("WebSocket server thread started via thread manager");
        
        // Start the actual websocket server
        if (websocket_server_->start(config_)) {
            log("WebSocket server running successfully");
            
            // Keep the thread alive while server is running
            while (websocket_server_->isRunning()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } else {
            log("Failed to start WebSocket server in managed thread");
        }
        
    } catch (const std::exception& e) {
        log("WebSocket server thread error: " + std::string(e.what()));
    }
    
    log("WebSocket server thread finished");
}

void ManagedWebSocketServer::log(const std::string& message) const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm = *std::localtime(&time_t);
    
    std::ostringstream oss;
    oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
    oss << "[ManagedWebSocketServer] " << message;
    
    std::cout << oss.str() << std::endl;
}
