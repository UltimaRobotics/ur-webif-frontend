#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <set>
#include <future>
#include <nlohmann/json.hpp>
#include "ur-rpc-template.h"
#include "direct_template.h"
#include "ThreadManager.hpp"

namespace BackendDatalink {

/**
 * @brief RPC Client wrapper for backend-datalink integration
 * 
 * This class provides a C++ wrapper around the ur-rpc-template functionality,
 * enabling MQTT-based RPC communication with thread-safe operations and
 * modern C++ interfaces.
 */
class RpcClient {
public:
    /**
     * @brief Constructor
     * @param configPath Path to RPC configuration JSON file
     * @param clientId Unique client identifier for MQTT connection
     */
    RpcClient(const std::string& configPath, const std::string& clientId);
    
    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~RpcClient();
    
    /**
     * @brief Start the RPC client thread
     * @return true if successfully started, false otherwise
     */
    bool start();
    
    /**
     * @brief Stop the RPC client thread
     */
    void stop();
    
    /**
     * @brief Check if the RPC client is running
     * @return true if running, false otherwise
     */
    bool isRunning() const;
    
    /**
     * @brief Check if the RPC client is connected to MQTT broker
     * @return true if connected, false otherwise
     */
    bool isConnected() const;
    
    /**
     * @brief Set message handler for incoming RPC messages
     * @param handler Function to handle incoming messages
     */
    void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler);
    
    /**
     * @brief Send response to RPC request
     * @param topic MQTT topic to send response to
     * @param response JSON response string
     */
    void sendResponse(const std::string& topic, const std::string& response);
    
    /**
     * @brief Send raw message to MQTT topic
     * @param topic MQTT topic
     * @param payload Message payload
     * @param payload_len Payload length
     * @return 0 on success, error code on failure
     */
    int sendRawMessage(const char* topic, const char* payload, size_t payload_len);
    
    /**
     * @brief Get client statistics
     * @return Statistics structure with connection and message info
     */
    direct_client_statistics_t getStatistics() const;
    
    /**
     * @brief Get the client ID
     * @return The client ID string
     */
    const std::string& getClientId() const { return clientId_; }
    
    /**
     * @brief Get the configuration path
     * @return The configuration path string
     */
    const std::string& getConfigPath() const { return configPath_; }

private:
    // Configuration
    std::string configPath_;
    std::string clientId_;
    
    // Thread management
    std::unique_ptr<ThreadMgr::ThreadManager> threadManager_;
    unsigned int rpcThreadId_{0};
    
    // RPC client context
    direct_client_thread_t* rpcContext_{nullptr};
    
    // Internal state
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::function<void(const std::string&, const std::string&)> messageHandler_;
    mutable std::mutex handlerMutex_;
    
    // Core thread function
    void rpcClientThreadFunc();
    
    // Static callback for C interoperability
    static void staticMessageHandler(const char* topic, const char* payload, 
                                   size_t payload_len, void* user_data);
    
    // Internal methods
    void updateConnectionStatus(bool connected);
    void logInfo(const std::string& message) const;
    void logError(const std::string& message) const;
};

/**
 * @brief RPC Operation Processor for handling concurrent requests
 * 
 * This class processes incoming RPC requests using thread pools for
 * concurrent execution, following the patterns from the integration guide.
 */
class RpcOperationProcessor {
public:
    /**
     * @brief Constructor
     * @param verbose Enable verbose logging
     */
    explicit RpcOperationProcessor(bool verbose = false);
    
    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~RpcOperationProcessor();
    
    /**
     * @brief Process incoming RPC request
     * @param payload Request payload
     * @param payload_len Payload length
     */
    void processRequest(const char* payload, size_t payload_len);
    
    /**
     * @brief Set response topic for outgoing responses
     * @param topic Response topic
     */
    void setResponseTopic(const std::string& topic);
    
    /**
     * @brief Shutdown the processor and clean up threads
     */
    void shutdown();

private:
    // Thread management
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager_;
    std::set<unsigned int> activeThreads_;
    std::mutex threadsMutex_;
    std::atomic<bool> isShuttingDown_{false};
    bool verbose_;
    
    // Response handling
    std::string responseTopic_;
    
    // Request context for thread-safe data passing
    struct RequestContext {
        std::string requestJson;
        std::string transactionId;
        std::string responseTopic;
        bool verbose;
        std::shared_ptr<std::promise<unsigned int>> threadIdPromise;
        std::shared_future<unsigned int> threadIdFuture;
    };
    
    // Processing methods
    void processOperationThread(std::shared_ptr<RequestContext> context);
    static void processOperationThreadStatic(std::shared_ptr<RequestContext> context, RpcOperationProcessor* processor);
    
    // Response handling
    void sendResponse(const std::string& transactionId, bool success, 
                      const std::string& result, const std::string& error = "");
    static void sendResponseStatic(const std::string& transactionId, bool success,
                                   const std::string& result, const std::string& error,
                                   const std::string& responseTopic);
    
    // Utility methods
    std::string extractTransactionId(const nlohmann::json& request);
    void cleanupThreadTracking(unsigned int threadId, std::shared_ptr<RequestContext> context);
    
    // Logging methods
    void logInfo(const std::string& message) const;
    void logError(const std::string& message) const;
};

} // namespace BackendDatalink

#endif // RPC_CLIENT_H
