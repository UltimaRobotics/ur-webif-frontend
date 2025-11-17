#include "rpc_client.h"
#include "managed_websocket_server.h"
#include "database_manager.h"
#include "SystemDataCollector.h"
#include "NetworkPriorityManager.h"
#include <iostream>
#include <chrono>
#include <cstring>
#include <memory>

namespace BackendDatalink {

// Forward declarations of global variables defined in main.cpp
extern std::unique_ptr<ManagedWebSocketServer> g_server;
extern std::unique_ptr<DatabaseManager> g_database;
extern std::unique_ptr<SystemDataCollector> g_system_collector;
extern std::unique_ptr<NetworkPriorityManager> g_network_priority_manager;

} // namespace BackendDatalink

namespace BackendDatalink {

// RpcClient Implementation

RpcClient::RpcClient(const std::string& configPath, const std::string& clientId)
    : configPath_(configPath), clientId_(clientId) {
    // Initialize thread manager with configurable pool size
    threadManager_ = std::make_unique<ThreadMgr::ThreadManager>(10);
    logInfo("RpcClient created with config: " + configPath + ", client ID: " + clientId);
}

RpcClient::~RpcClient() {
    if (isRunning()) {
        stop();
    }
    logInfo("RpcClient destroyed");
}

bool RpcClient::start() {
    if (running_.load()) {
        logInfo("RpcClient already running");
        return true; // Already running
    }

    try {
        // Create RPC client thread using ThreadManager
        rpcThreadId_ = threadManager_->createThread([this]() {
            this->rpcClientThreadFunc();
        });

        // Wait for thread initialization with timeout
        const int MAX_WAIT_MS = 3000;
        const int POLL_INTERVAL_MS = 100;
        int elapsed = 0;
        
        while (elapsed < MAX_WAIT_MS && !running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
            elapsed += POLL_INTERVAL_MS;
        }

        bool success = running_.load();
        if (success) {
            logInfo("RpcClient started successfully");
        } else {
            logError("RpcClient failed to start within timeout");
        }
        
        return success;
        
    } catch (const std::exception &e) {
        logError("start() failed: " + std::string(e.what()));
        return false;
    }
}

void RpcClient::stop() {
    if (!running_.load()) {
        return; // Not running
    }

    logInfo("Stopping RpcClient...");
    running_.store(false);
    
    if (rpcContext_) {
        direct_client_thread_stop(rpcContext_);
        direct_client_thread_destroy(rpcContext_);
        rpcContext_ = nullptr;
    }
    
    // Wait for thread to finish
    if (threadManager_ && threadManager_->isThreadAlive(rpcThreadId_)) {
        threadManager_->joinThread(rpcThreadId_, std::chrono::seconds(5));
    }
    
    connected_.store(false);
    logInfo("RpcClient stopped");
}

bool RpcClient::isRunning() const {
    return running_.load();
}

bool RpcClient::isConnected() const {
    return connected_.load();
}

void RpcClient::setMessageHandler(std::function<void(const std::string&, const std::string&)> handler) {
    std::lock_guard<std::mutex> lock(handlerMutex_);
    messageHandler_ = handler;
    logInfo("Message handler set");
}

void RpcClient::sendResponse(const std::string& topic, const std::string& response) {
    if (!isRunning() || !isConnected()) {
        logError("Cannot send response - client not running or connected");
        return;
    }
    
    int result = direct_client_publish_raw_message(topic.c_str(), response.c_str(), response.length());
    if (result == 0) {
        logInfo("Response sent to topic: " + topic);
    } else {
        logError("Failed to send response to topic: " + topic + " (error: " + std::to_string(result) + ")");
    }
}

int RpcClient::sendRawMessage(const char* topic, const char* payload, size_t payload_len) {
    if (!isRunning() || !isConnected()) {
        logError("Cannot send raw message - client not running or connected");
        return -1;
    }
    
    return direct_client_publish_raw_message(topic, payload, payload_len);
}

direct_client_statistics_t RpcClient::getStatistics() const {
    direct_client_statistics_t stats;
    if (direct_client_get_statistics(&stats) == 0) {
        return stats;
    }
    
    // Return empty stats on error
    memset(&stats, 0, sizeof(stats));
    return stats;
}

void RpcClient::rpcClientThreadFunc() {
    try {
        logInfo("RPC client thread started");

        // Validate message handler is set
        {
            std::lock_guard<std::mutex> lock(handlerMutex_);
            if (!messageHandler_) {
                logError("ERROR: No message handler set!");
                running_.store(false);
                return;
            }
        }

        // Create thread context for UR-RPC template
        rpcContext_ = direct_client_thread_create(configPath_.c_str());
        
        if (!rpcContext_) {
            logError("Failed to create client thread context");
            running_.store(false);
            return;
        }

        // Set message handler BEFORE starting the thread
        direct_client_set_message_handler(rpcContext_, staticMessageHandler, this);

        // Start the client thread
        if (direct_client_thread_start(rpcContext_) != 0) {
            logError("Failed to start client thread");
            direct_client_thread_destroy(rpcContext_);
            rpcContext_ = nullptr;
            running_.store(false);
            return;
        }

        // Wait for connection establishment
        if (!direct_client_thread_wait_for_connection(rpcContext_, 10000)) {
            logError("Connection timeout");
            direct_client_thread_stop(rpcContext_);
            direct_client_thread_destroy(rpcContext_);
            rpcContext_ = nullptr;
            running_.store(false);
            return;
        }

        running_.store(true);
        connected_.store(true);
        logInfo("RPC client connected and running");

        // Main thread loop
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Update connection status
            if (rpcContext_) {
                bool currentlyConnected = direct_client_thread_is_connected(rpcContext_);
                if (currentlyConnected != connected_.load()) {
                    updateConnectionStatus(currentlyConnected);
                }
            }
        }

        // Cleanup
        if (rpcContext_) {
            direct_client_thread_stop(rpcContext_);
            direct_client_thread_destroy(rpcContext_);
            rpcContext_ = nullptr;
        }
        
        connected_.store(false);
        
    } catch (const std::exception &e) {
        logError("Thread function error: " + std::string(e.what()));
        running_.store(false);
        connected_.store(false);
    }
    
    logInfo("RPC client thread finished");
}

void RpcClient::staticMessageHandler(const char *topic, const char *payload,
                                     size_t payload_len, void *user_data) {
    RpcClient *self = static_cast<RpcClient *>(user_data);
    if (!self) {
        return;
    }

    // Convert C strings to C++ strings safely
    const std::string topicStr(topic ? topic : "");
    const std::string payloadStr(payload ? payload : "", payload_len);

    // Delegate to instance handler
    std::lock_guard<std::mutex> lock(self->handlerMutex_);
    if (self->messageHandler_) {
        try {
            self->messageHandler_(topicStr, payloadStr);
        } catch (const std::exception& e) {
            self->logError("Exception in message handler: " + std::string(e.what()));
        }
    }
}

void RpcClient::updateConnectionStatus(bool connected) {
    connected_.store(connected);
    if (connected) {
        logInfo("RPC client connected to broker");
    } else {
        logInfo("RPC client disconnected from broker");
    }
}

void RpcClient::logInfo(const std::string& message) const {
    std::cout << "[RpcClient:" << clientId_ << "] " << message << std::endl;
}

void RpcClient::logError(const std::string& message) const {
    std::cerr << "[RpcClient:" << clientId_ << "] ERROR: " << message << std::endl;
}

// RpcOperationProcessor Implementation

RpcOperationProcessor::RpcOperationProcessor(bool verbose)
    : verbose_(verbose) {
    // Initialize thread manager with appropriate pool size
    threadManager_ = std::make_shared<ThreadMgr::ThreadManager>(100);
    logInfo("RpcOperationProcessor created");
}

RpcOperationProcessor::~RpcOperationProcessor() {
    shutdown();
    logInfo("RpcOperationProcessor destroyed");
}

void RpcOperationProcessor::processRequest(const char* payload, size_t payload_len) {
    // Input validation
    if (!payload || payload_len == 0) {
        logError("Empty payload received");
        return;
    }

    // Size validation (prevent memory exhaustion)
    const size_t MAX_PAYLOAD_SIZE = 1024 * 1024; // 1MB
    if (payload_len > MAX_PAYLOAD_SIZE) {
        logError("Payload too large: " + std::to_string(payload_len) + " bytes");
        return;
    }

    try {
        // JSON parsing
        nlohmann::json root = nlohmann::json::parse(payload, payload + payload_len);

        // JSON-RPC 2.0 validation
        if (!root.contains("jsonrpc") || root["jsonrpc"].get<std::string>() != "2.0") {
            logError("Invalid or missing JSON-RPC version");
            return;
        }

        // Extract transaction ID
        std::string transactionId = extractTransactionId(root);

        // Extract method
        if (!root.contains("method") || !root["method"].is_string()) {
            sendResponse(transactionId, false, "", "Missing method in request");
            return;
        }
        std::string method = root["method"].get<std::string>();

        // Extract parameters
        if (!root.contains("params") || !root["params"].is_object()) {
            sendResponse(transactionId, false, "", "Missing or invalid params in request");
            return;
        }

        // Create processing context
        std::string requestJson(payload, payload_len);
        auto context = std::make_shared<RequestContext>();
        context->requestJson = requestJson;
        context->transactionId = transactionId;
        context->responseTopic = responseTopic_;
        context->verbose = verbose_;
        context->threadIdPromise = std::make_shared<std::promise<unsigned int>>();
        context->threadIdFuture = context->threadIdPromise->get_future();

        // Launch asynchronous processing
        try {
            // Check shutdown state
            bool shuttingDown = isShuttingDown_.load();
            if (shuttingDown) {
                sendResponse(transactionId, false, "", "Server is shutting down");
                return;
            }

            // Create processing thread
            unsigned int threadId = threadManager_->createThread([context, this]() {
                RpcOperationProcessor::processOperationThreadStatic(context, this);
            });
            
            // Register thread in tracking set
            {
                std::lock_guard<std::mutex> lock(threadsMutex_);
                activeThreads_.insert(threadId);
            }
            
            // Set thread ID for worker access
            context->threadIdPromise->set_value(threadId);

        } catch (const std::exception& e) {
            logError("Failed to create thread: " + std::string(e.what()));
            
            // Fallback to synchronous processing
            context->threadIdPromise->set_value(0);
            RpcOperationProcessor::processOperationThreadStatic(context, this);
        }

    } catch (const nlohmann::json::parse_error& e) {
        logError("JSON parse error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        logError("Exception processing request: " + std::string(e.what()));
    }
}

void RpcOperationProcessor::setResponseTopic(const std::string& topic) {
    responseTopic_ = topic;
    logInfo("Response topic set to: " + topic);
}

void RpcOperationProcessor::shutdown() {
    // Set shutdown flag to prevent new thread creation
    isShuttingDown_.store(true);
    
    // Collect all active threads for joining
    std::vector<unsigned int> threadsToJoin;
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        threadsToJoin.assign(activeThreads_.begin(), activeThreads_.end());
    }
    
    // Join all threads with timeout
    for (unsigned int threadId : threadsToJoin) {
        if (threadManager_->isThreadAlive(threadId)) {
            bool completed = threadManager_->joinThread(threadId, std::chrono::minutes(5));
            if (!completed) {
                logError("WARNING: Thread " + std::to_string(threadId) + " did not complete after 5 minutes");
            }
        }
    }
    
    logInfo("RpcOperationProcessor shutdown completed");
}

void RpcOperationProcessor::processOperationThreadStatic(std::shared_ptr<RequestContext> context, RpcOperationProcessor* processor) {
    // Wait for thread ID synchronization
    unsigned int threadId = context->threadIdFuture.get();
    
    // Extract context data safely
    const std::string& requestJson = context->requestJson;
    const std::string& transactionId = context->transactionId;
    bool verbose = context->verbose;
    
    try {
        // Parse request in thread context
        nlohmann::json root = nlohmann::json::parse(requestJson);
        
        // Extract method and parameters
        std::string method = root["method"].get<std::string>();
        nlohmann::json paramsObj = root["params"];
        
        // Process backend-datalink specific operations
        nlohmann::json result;
        bool success = true;
        std::string errorMessage;
        
        try {
            success = false;
            errorMessage = "Unknown method: " + method;
        } catch (const std::exception& e) {
            success = false;
            errorMessage = "Error executing method '" + method + "': " + std::string(e.what());
        }
        
        // Send response based on execution result
        if (success) {
            sendResponseStatic(transactionId, true, result.dump(), "", context->responseTopic);
        } else {
            sendResponseStatic(transactionId, false, "", errorMessage, context->responseTopic);
        }
        
    } catch (const std::exception& e) {
        sendResponseStatic(transactionId, false, "", 
                          std::string("Exception: ") + e.what(), 
                          context->responseTopic);
    }

    // Cleanup thread from tracking using processor instance
    if (threadId != 0 && processor) {
        std::lock_guard<std::mutex> lock(processor->threadsMutex_);
        processor->activeThreads_.erase(threadId);
    }
}

void RpcOperationProcessor::sendResponse(const std::string& transactionId, bool success, 
                                        const std::string& result, const std::string& error) {
    sendResponseStatic(transactionId, success, result, error, responseTopic_);
}

void RpcOperationProcessor::sendResponseStatic(const std::string& transactionId, bool success,
                                               const std::string& result, const std::string& error,
                                               const std::string& responseTopic) {
    try {
        nlohmann::json response;
        response["jsonrpc"] = "2.0";
        response["id"] = transactionId;

        if (success) {
            // Handle JSON result parsing
            if (!result.empty() && result[0] == '{') {
                try {
                    nlohmann::json parsedResult = nlohmann::json::parse(result);
                    response["result"] = parsedResult;
                } catch (const nlohmann::json::parse_error&) {
                    response["result"] = result;
                }
            } else if (!result.empty()) {
                response["result"] = result;
            } else {
                response["result"] = "Operation completed successfully";
            }
        } else {
            // Error response format
            nlohmann::json errorObj;
            errorObj["code"] = -1;
            errorObj["message"] = error;
            response["error"] = errorObj;
        }

        // Publish response
        std::string responseJson = response.dump();
        direct_client_publish_raw_message(responseTopic.c_str(), 
                                         responseJson.c_str(), 
                                         responseJson.size());

    } catch (const std::exception& e) {
        std::cerr << "Failed to send response: " << e.what() << std::endl;
    }
}

std::string RpcOperationProcessor::extractTransactionId(const nlohmann::json& request) {
    if (request.contains("id")) {
        if (request["id"].is_string()) {
            return request["id"].get<std::string>();
        } else if (request["id"].is_number()) {
            return std::to_string(request["id"].get<int>());
        }
    }
    return "unknown";
}

void RpcOperationProcessor::cleanupThreadTracking(unsigned int threadId, std::shared_ptr<RequestContext> /*context*/) {
    if (threadId != 0) {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        activeThreads_.erase(threadId);
    }
}

void RpcOperationProcessor::logInfo(const std::string& message) const {
    if (verbose_) {
        std::cout << "[RpcOperationProcessor] " << message << std::endl;
    }
}

void RpcOperationProcessor::logError(const std::string& message) const {
    std::cerr << "[RpcOperationProcessor] ERROR: " << message << std::endl;
}

// Backend-datalink specific operation handlers



} // namespace BackendDatalink
